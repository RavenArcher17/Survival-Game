// Metal, the 12 biome building sets, freeform beams and posts and the sets' decor (cooks/build-building.md; FULL-GAME
// 6.4; survival-crafting 2.17). Data/building_sets.json is expanded into ordinary pieces here.

#include "WRBuildSets.h"
#include "WRBuilding.h"
#include "WRCrafting.h"
#include "WRCreature.h"
#include "WRGameState.h"
#include "WRGear.h"
#include "WRInventory.h"
#include "WRMachine.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRSurvival.h"
#include "WRTuning.h"
#include "Wildreach.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"

namespace WRBuildSetsDetail
{
	FLinearColor Hex(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, const FLinearColor& Fallback)
	{
		FString S;
		if (!O.IsValid() || !O->TryGetStringField(Field, S) || S.IsEmpty()) return Fallback;
		return FLinearColor(FColor::FromHex(S));   // sRGB hex -> linear
	}

	TArray<TPair<FName, int32>> Costs(const TSharedPtr<FJsonObject>& O, const TCHAR* Field)
	{
		TArray<TPair<FName, int32>> Out;
		const TSharedPtr<FJsonObject>* C = nullptr;
		if (O.IsValid() && O->TryGetObjectField(Field, C))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& V : (*C)->Values) Out.Add({ FName(*V.Key), FMath::RoundToInt(V.Value->AsNumber()) });
		}
		return Out;
	}

	TArray<TPair<FName, int32>> Scale(const TArray<TPair<FName, int32>>& Wall, float Share)
	{
		TArray<TPair<FName, int32>> Out;
		for (const TPair<FName, int32>& C : Wall) Out.Add({ C.Key, FMath::Max(1, FMath::CeilToInt(C.Value * Share - 0.001f)) });
		return Out;
	}

	void AddCost(TArray<TPair<FName, int32>>& Cost, FName Item, int32 N)
	{
		if (Item.IsNone() || N <= 0) return;
		for (TPair<FName, int32>& C : Cost) if (C.Key == Item) { C.Value += N; return; }
		Cost.Add({ Item, N });
	}

	/** Pieces that make a base (no underground bases, DD): walls, floors, beds, chests, stations, turrets, the claim
	 *  and the Hearth. What caves need stays allowed: lights (and coil lamps), the drill on an ore vein, the dark crops
	 *  and greenhouse lights (cooks/build-farming.md, build-machinery.md). */
	bool IsBasePiece(const FWRPieceDef& Def)
	{
		return Def.Kind != TEXT("light") && Def.Kind != TEXT("crop") && Def.Id != TEXT("drill") && Def.Id != TEXT("torch")
			&& Def.Id != TEXT("coil_lamp") && Def.Id != TEXT("greenhouse_lights");
	}
}

const FWRBuildSets& FWRBuildSets::Get()
{
	static FWRBuildSets Instance;
	return Instance;
}

FWRBuildSets::FWRBuildSets()
{
	using namespace WRBuildSetsDetail;
	const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("building_sets.json"));
	if (!Root.IsValid()) return;
	const TSharedPtr<FJsonObject>* Dmg = nullptr;
	if (Root->TryGetObjectField(TEXT("materialDamage"), Dmg))
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& V : (*Dmg)->Values) Damage.Add(FName(*V.Key), V.Value->AsNumber());
	}
	const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
	if (!Root->TryGetArrayField(TEXT("tiers"), List)) return;
	for (const TSharedPtr<FJsonValue>& V : *List)
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FWRTierDef T;
		T.Id = FName(*O->GetStringField(TEXT("id")));
		T.Name = O->GetStringField(TEXT("name"));
		FString S;
		if (O->TryGetStringField(TEXT("biome"), S)) T.Biome = FName(*S);
		if (O->TryGetStringField(TEXT("block"), S)) T.Block = FName(*S);
		if (O->TryGetStringField(TEXT("boss"), S)) T.Boss = FName(*S);
		if (O->TryGetStringField(TEXT("signature"), S)) T.Signature = FName(*S);
		if (O->TryGetStringField(TEXT("upkeepItem"), S)) T.UpkeepItem = FName(*S);
		if (T.UpkeepItem.IsNone()) T.UpkeepItem = T.Block;
		double N = 0.0;
		if (O->TryGetNumberField(TEXT("hearth"), N)) T.Hearth = static_cast<int32>(N);
		if (O->TryGetNumberField(TEXT("span"), N)) T.Span = static_cast<int32>(N);
		O->TryGetNumberField(TEXT("wallHp"), T.WallHp);
		O->TryGetNumberField(TEXT("foundationHp"), T.FoundationHp);
		O->TryGetNumberField(TEXT("doorHp"), T.DoorHp);
		O->TryGetNumberField(TEXT("upkeep"), T.Upkeep);
		O->TryGetBoolField(TEXT("freeformOnly"), T.bFreeformOnly);
		T.Wall = Costs(O, TEXT("wall"));
		const TSharedPtr<FJsonObject>* L = nullptr;
		if (O->TryGetObjectField(TEXT("look"), L))
		{
			const TSharedPtr<FJsonObject> Look = *L;
			if (Look->TryGetStringField(TEXT("style"), S)) T.Look.Style = FName(*S);
			T.Look.A = Hex(Look, TEXT("a"), T.Look.A);
			T.Look.B = Hex(Look, TEXT("b"), T.Look.B);
			T.Look.C = Hex(Look, TEXT("c"), T.Look.C);
			T.Look.Glow = Hex(Look, TEXT("glow"), T.Look.Glow);
			T.Look.Glass = Hex(Look, TEXT("glass"), T.Look.Glass);
			T.Look.Trim = Hex(Look, TEXT("trim"), T.Look.Trim);
			Look->TryGetNumberField(TEXT("metal"), T.Look.Metal);
			Look->TryGetNumberField(TEXT("rough"), T.Look.Rough);
			Look->TryGetNumberField(TEXT("glassGlow"), T.Look.GlassGlow);
		}
		Tiers.Add(T);
	}
	UE_LOG(LogWildreach, Log, TEXT("Building sets: %d tiers"), Tiers.Num());
}

float FWRBuildSets::WeaponShare(FName Material) const
{
	if (const float* D = Damage.Find(Material)) return *D;
	const FWRTierDef* T = Tier(Material);
	if (T && T->IsBiome()) return Damage.FindRef(TEXT("biome"));
	return 1.f;   // not a building material (furniture, machines): weapons hit it fully
}

void FWRBuildSets::Expand(TArray<FWRPieceDef>& Pieces, TMap<FName, int32>& Span, TMap<FName, float>& Upkeep, TMap<FName, FName>& UpkeepItem)
{
	using namespace WRBuildSetsDetail;
	const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("building_sets.json"));
	if (!Root.IsValid()) return;
	const TArray<TSharedPtr<FJsonValue>>* Shapes = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Free = nullptr;
	Root->TryGetArrayField(TEXT("shapes"), Shapes);
	Root->TryGetArrayField(TEXT("freeform"), Free);
	const FWRBuildSets& Sets = Get();
	int32 Made = 0;

	for (const FWRTierDef& T : Sets.Tiers)
	{
		if (!T.bFreeformOnly)
		{
			Span.Add(T.Id, T.Span > 0 ? T.Span : 8);
			if (T.Upkeep > 0.f && !T.UpkeepItem.IsNone())
			{
				Upkeep.Add(T.Id, T.Upkeep);
				UpkeepItem.Add(T.Id, T.UpkeepItem);
			}
		}
		const auto Base = [&T](FWRPieceDef& D)
		{
			D.Category = TEXT("structure");
			D.Material = T.Id;
			D.Tier = T.Id;
			D.HearthLevel = T.Hearth;
			D.LiveWith = T.Biome;
		};
		// The 12 shapes.
		for (int32 i = 0; Shapes && !T.bFreeformOnly && i < Shapes->Num(); ++i)
		{
			const TSharedPtr<FJsonObject> S = (*Shapes)[i]->AsObject();
			FWRPieceDef D;
			Base(D);
			D.Shape = FName(*S->GetStringField(TEXT("shape")));
			D.Id = FName(*FString::Printf(TEXT("%s_%s"), *T.Id.ToString(), *D.Shape.ToString()));
			D.Name = FString::Printf(TEXT("%s %s"), *T.Name, *S->GetStringField(TEXT("name")));
			D.Kind = FName(*S->GetStringField(TEXT("kind")));
			S->TryGetBoolField(TEXT("lockable"), D.bLockable);
			double Cost = 1.0, Hp = 1.0;
			S->TryGetNumberField(TEXT("cost"), Cost);
			S->TryGetNumberField(TEXT("hp"), Hp);
			D.Cost = Scale(T.Wall, Cost);
			D.HP = D.Shape == TEXT("foundation") && T.FoundationHp > 0.f ? T.FoundationHp : D.Shape == TEXT("door") && T.DoorHp > 0.f ? T.DoorHp : T.WallHp * Hp;
			Pieces.Add(D);
			++Made;
		}
		// Freeform beams, posts and braces (every tier, wood and stone too).
		for (int32 i = 0; Free && i < Free->Num(); ++i)
		{
			const TSharedPtr<FJsonObject> S = (*Free)[i]->AsObject();
			FWRPieceDef D;
			Base(D);
			D.Kind = TEXT("beam");
			D.Shape = FName(*S->GetStringField(TEXT("shape")));
			D.Id = FName(*FString::Printf(TEXT("%s_%s"), *T.Id.ToString(), *D.Shape.ToString()));
			D.Name = FString::Printf(TEXT("%s %s"), *T.Name, *S->GetStringField(TEXT("name")));
			double Len = 200.0, Cost = 0.3, Hp = 0.6;
			S->TryGetNumberField(TEXT("length"), Len);
			S->TryGetNumberField(TEXT("cost"), Cost);
			S->TryGetNumberField(TEXT("hp"), Hp);
			D.FreeLength = Len;
			D.Cost = Scale(T.Wall, Cost);
			D.HP = T.WallHp * Hp;
			Pieces.Add(D);
			++Made;
		}
	}

	// The sets' decor: 6 pieces and a rug each, taught by the land's boss blueprint.
	const TSharedPtr<FJsonObject>* Decor = nullptr;
	if (Root->TryGetObjectField(TEXT("decor"), Decor))
	{
		double BlockN = 6.0, SigN = 2.0, Comfort = 2.0, RugComfort = 2.0;
		(*Decor)->TryGetNumberField(TEXT("block"), BlockN);
		(*Decor)->TryGetNumberField(TEXT("sig"), SigN);
		(*Decor)->TryGetNumberField(TEXT("comfort"), Comfort);
		(*Decor)->TryGetNumberField(TEXT("rugComfort"), RugComfort);
		FString Group = TEXT("biomeset");
		(*Decor)->TryGetStringField(TEXT("comfortGroup"), Group);
		const TSharedPtr<FJsonObject>* SetList = nullptr;
		if ((*Decor)->TryGetObjectField(TEXT("sets"), SetList))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Set : (*SetList)->Values)
			{
				const FWRTierDef* T = Sets.Tier(FName(*Set.Key));
				if (!T) continue;
				for (const TSharedPtr<FJsonValue>& RowV : Set.Value->AsArray())
				{
					const TSharedPtr<FJsonObject> R = RowV->AsObject();
					FWRPieceDef D;
					D.Category = TEXT("decor");
					D.Tier = T->Id;
					D.LiveWith = T->Biome;
					D.Id = FName(*FString::Printf(TEXT("%s_%s"), *T->Id.ToString(), *R->GetStringField(TEXT("key"))));
					D.Name = R->GetStringField(TEXT("name"));
					FString S;
					D.Kind = R->TryGetStringField(TEXT("kind"), S) ? FName(*S) : FName(TEXT("decor"));
					D.Arch = FName(*R->GetStringField(TEXT("arch")));
					if (R->TryGetStringField(TEXT("variant"), S)) D.Variant = FName(*S);
					if (R->TryGetStringField(TEXT("needs"), S)) D.Needs = FName(*S);
					D.Blueprint = FName(*FString::Printf(TEXT("bp_%s_decor"), *T->Id.ToString()));
					D.HP = 400.f;
					AddCost(D.Cost, T->Block, FMath::RoundToInt(BlockN));
					AddCost(D.Cost, T->Signature, FMath::RoundToInt(SigN));
					for (const TPair<FName, int32>& X : Costs(R, TEXT("extra"))) AddCost(D.Cost, X.Key, X.Value);
					const bool bRug = D.Arch == TEXT("rug") || D.Arch == TEXT("pelt_rug");
					D.ComfortGroup = bRug ? FName(TEXT("floor")) : FName(*Group);
					D.Comfort = FMath::RoundToInt(bRug ? RugComfort : Comfort);
					D.Note = FString::Printf(TEXT("Part of the %s set: counts once in the set's comfort group (+%d), whichever of its pieces you place."), *T->Name, D.Comfort);
					if (bRug) D.Note = FString::Printf(TEXT("The %s set's rug: the best floor comfort (+%d)."), *T->Name, D.Comfort);
					Pieces.Add(D);
					++Made;
				}
			}
		}
	}
	UE_LOG(LogWildreach, Log, TEXT("Building sets: %d pieces made from %d tiers"), Made, Sets.Tiers.Num());
}

FString FWRBuildSets::BlueprintSource(FName Blueprint)
{
	const FWRBlueprintDef* Bp = FWRGear::Get().FindBlueprint(Blueprint);
	if (!Bp) return TEXT("a major boss");
	const FWRCreatureDef* Boss = FWRCreatureRegistry::Get().Find(Bp->Boss);
	if (Boss) return Boss->Name;
	FString S = Bp->Boss.ToString();
	return S.Left(1).ToUpper() + S.Mid(1);
}

FString FWRBuildSets::PlaceCheck(const UObject* WorldContext, const FWRPieceDef& Def, const FVector& Location, const AActor* Builder)
{
	using namespace WRBuildSetsDetail;
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World) return FString();
	if (!Def.Blueprint.IsNone() && !FWRBlueprints::Knows(Builder, Def.Blueprint))
	{
		return FString::Printf(TEXT("learn its blueprint first (%s drops it)"), *BlueprintSource(Def.Blueprint));
	}
	// No underground bases (Raven, DD "Ruled out"): build on the surface; a torch or a drill still goes in a cave.
	if (IsBasePiece(Def) && WRBiome::IsUnderground(WorldContext, Location))
	{
		return TEXT("no building underground: bases stand on the surface");
	}
	if (Def.Needs == TEXT("vent") && !FWRMachines::LavaNear(Location, WRTune(TEXT("building.ventReach"), 800.f)))
	{
		return TEXT("a hot spring bath sits on a volcanic vent (lava within 8 m) or a hot spring");
	}
	return FString();
}

bool FWRBuildSets::IsHighTier(const FWRPieceDef& Def)
{
	const FWRTierDef* T = Get().Tier(Def.Tier);
	return T && (T->IsBiome() || T->Id == TEXT("metal"));
}

float FWRBuildSets::PlaceXP(const FWRPieceDef& Def)
{
	const FWRTierDef* T = Get().Tier(Def.Tier);
	const bool bHigh = T && (T->IsBiome() || T->Id == TEXT("metal")) && (Def.IsStructural());
	return bHigh ? WRTune(TEXT("lifeSkills.xp.placeHighTier"), 12.f) : WRTune(TEXT("lifeSkills.xp.place"), 8.f);
}

// ---------------------------------------------------------------- the Hearth offering

namespace WRBuildSetsDetail
{
	/** A dish cooked at a campfire, cooking pot or oven (what a household leaves on its new hearth). */
	bool IsCookedMeal(FName Item)
	{
		const FWRItemDef* D = FWRItemRegistry::Get().Find(Item);
		if (!D || D->Category != TEXT("food")) return false;
		for (const TCHAR* Station : { TEXT("campfire"), TEXT("cooking_pot"), TEXT("oven") })
		{
			for (const FWRRecipe* R : FWRRecipeRegistry::Get().ForStation(Station))
			{
				if (R && R->Output == Item) return true;
			}
		}
		return false;
	}
}

FString FWRBuildSets::TryOffer(AWRBuildPiece* HearthPiece, AWRPlayerController* Player)
{
	using namespace WRBuildSetsDetail;
	AWRPlayerState* PS = Player ? Player->GetPlayerState<AWRPlayerState>() : nullptr;
	const AWRGameState* GS = HearthPiece ? AWRGameState::Get(HearthPiece) : nullptr;
	if (!PS || !GS || !HearthPiece->Offering.IsNone() || HearthPiece->PlacedHours < 0.0 || !HearthPiece->CanAccess(PS->GetPlayerName()))
	{
		return FString();
	}
	if (GS->GetTotalHours() - HearthPiece->PlacedHours > WRTune(TEXT("comfort.offeringHours"), 24.f))
	{
		return FString();   // its first day is over: E goes on to the Hearth
	}
	UWRInventoryComponent* Inv = PS->GetInventory();
	for (const FWRItemStack& S : Inv ? Inv->GetSlots() : TArray<FWRItemStack>())
	{
		if (S.IsEmpty() || !IsCookedMeal(S.ItemId)) continue;
		const FName Meal = S.ItemId;
		Inv->RemoveItem(Meal, 1);
		HearthPiece->Offering = Meal;
		const FWRItemDef* D = FWRItemRegistry::Get().Find(Meal);
		UE_LOG(LogWildreach, Log, TEXT("HEARTH %s offered %s on a new Hearth"), *PS->GetPlayerName(), *Meal.ToString());
		return FString::Printf(TEXT("You leave %s on the new Hearth: every bed in this claim is a little more like home (comfort +%d)"),
			D ? *D->Name.ToLower() : *Meal.ToString(), FMath::RoundToInt(WRTune(TEXT("comfort.offering"), 2.f)));
	}
	return FString();
}

int32 FWRBuildSets::OfferingComfort(const UObject* WorldContext, const FVector& BedAt)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const AWRHearth* Hearth = World ? AWRHearth::FindCovering(World, BedAt) : nullptr;
	const AWRBuildPiece* Piece = Hearth ? Cast<AWRBuildPiece>(Hearth->GetOwner()) : nullptr;
	return Piece && !Piece->Offering.IsNone() ? FMath::RoundToInt(WRTune(TEXT("comfort.offering"), 2.f)) : 0;
}
