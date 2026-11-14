// Farming and food (FULL-GAME 6.2; showcase-notes cooks/build-farming.md). See WRFarm.h for the shape of it.

#include "WRFarm.h"
#include "WRBuilding.h"
#include "WRCharacter.h"
#include "WRCharacterBase.h"
#include "WRCrafting.h"
#include "WRCreature.h"
#include "WRFishing.h"
#include "WRFx.h"
#include "WRGameState.h"
#include "WRLifeSkills.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRSocial.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRSurvival.h"
#include "WRTuning.h"
#include "WRWater.h"
#include "Wildreach.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Misc/DateTime.h"
#include "Net/UnrealNetwork.h"

namespace WRFarmDetail
{
	FLinearColor ReadColor(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, const FLinearColor& Fallback)
	{
		// sRGB 0..255 in the data (like the mockups), linear here.
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (!O->TryGetArrayField(Field, A) || A->Num() < 3) return Fallback;
		return FLinearColor(FColor(uint8((*A)[0]->AsNumber()), uint8((*A)[1]->AsNumber()), uint8((*A)[2]->AsNumber())));
	}

	FIntPoint ReadRange(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, const FIntPoint& Fallback)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (!O->TryGetArrayField(Field, A) || A->Num() < 2) return Fallback;
		return FIntPoint(int32((*A)[0]->AsNumber()), int32((*A)[1]->AsNumber()));
	}

	float Tune(const FString& Path, float Fallback) { return FWRTuning::Get().Num(Path, Fallback); }

	AWRPlayerState* PSOf(const AActor* Actor)
	{
		if (const AWRPlayerState* PS = Cast<AWRPlayerState>(Actor)) return const_cast<AWRPlayerState*>(PS);
		if (const AController* C = Cast<AController>(Actor)) return C->GetPlayerState<AWRPlayerState>();
		if (const APawn* P = Cast<APawn>(Actor)) return P->GetPlayerState<AWRPlayerState>();
		return nullptr;
	}

	FString ItemName(FName Id)
	{
		const FWRItemDef* D = FWRItemRegistry::Get().Find(Id);
		return D ? D->Name : Id.ToString();
	}

	/** A built piece straight above, within Height: null if open sky (or something that isn't built: a cave's rock). */
	const AWRBuildPiece* RoofAbove(const UWorld* World, const FVector& At, float Height, const AActor* Ignore, bool& bOtherCover)
	{
		bOtherCover = false;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRFarmRoof), false, Ignore);
		FHitResult H;
		if (!World->LineTraceSingleByChannel(H, At, At + FVector(0.f, 0.f, Height), ECC_Visibility, Q)) return nullptr;
		const AWRBuildPiece* P = Cast<AWRBuildPiece>(H.GetActor());
		if (!P) bOtherCover = true;
		return P;
	}

	/** The share of 16 level rays at 1.2 m that meet a built piece within Reach (walls around a plot). */
	float Walled(const UWorld* World, const FVector& At, float Reach, const AActor* Ignore)
	{
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRFarmWalls), false, Ignore);
		int32 Hits = 0;
		for (int32 i = 0; i < 16; ++i)
		{
			const float A = i * (2.f * PI / 16.f);
			FHitResult H;
			if (World->LineTraceSingleByChannel(H, At, At + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * Reach, ECC_Visibility, Q) && Cast<AWRBuildPiece>(H.GetActor()))
			{
				++Hits;
			}
		}
		return Hits / 16.f;
	}

	/** A window or glazing within Radius (a dark shed has none). */
	bool WindowNear(const UWorld* World, const FVector& At, float Radius)
	{
		for (TActorIterator<AWRBuildPiece> It(World); It; ++It)
		{
			const FWRPieceDef* D = It->bGhost ? nullptr : It->GetDef();
			if (D && (D->bGlass || D->Shape == TEXT("window")) && FVector::DistSquared(It->GetActorLocation(), At) < Radius * Radius) return true;
		}
		return false;
	}

	/** The pen's animals: tamed by its owner (or their clan), standing inside its fence. */
	TArray<AWRCreature*> PenAnimals(const AWRBuildPiece* Pen)
	{
		TArray<AWRCreature*> Out;
		const FTransform T = Pen->GetActorTransform();
		const float Half = Tune(TEXT("farm.pen.half"), 390.f);
		for (TActorIterator<AWRCreature> It(Pen->GetWorld()); It; ++It)
		{
			if (!It->IsAlive() || !It->IsTamed() || !Pen->CanAccess(It->TamedBy)) continue;
			const FVector L = T.InverseTransformPosition(It->GetActorLocation());
			if (FMath::Abs(L.X) < Half && FMath::Abs(L.Y) < Half && FMath::Abs(L.Z) < 300.f) Out.Add(*It);
		}
		return Out;
	}

	/** A tamed Hiveback of the hive's owner at the hive (it's moved in). */
	AWRCreature* HiveDweller(const AWRBuildPiece* Hive)
	{
		const FWRPieceDef* D = Hive->GetDef();
		if (!D) return nullptr;
		for (TActorIterator<AWRCreature> It(Hive->GetWorld()); It; ++It)
		{
			if (It->IsAlive() && It->IsTamed() && It->CreatureId == D->NeedsCreature && Hive->CanAccess(It->TamedBy)
				&& FVector::Dist(It->GetActorLocation(), Hive->GetActorLocation()) < Tune(TEXT("farm.hive.homeRange"), 400.f))
			{
				return *It;
			}
		}
		return nullptr;
	}

	/** A crop plot (or a wild flower: any planted plot) within Radius: the hive's forage. */
	bool FlowersNear(const UWorld* World, const FVector& At, float Radius)
	{
		for (TActorIterator<AWRBuildPiece> It(World); It; ++It)
		{
			const FWRPieceDef* D = It->bGhost ? nullptr : It->GetDef();
			if (D && D->Kind == TEXT("crop") && !It->CropId.IsNone() && FVector::DistSquared(It->GetActorLocation(), At) < Radius * Radius) return true;
		}
		return false;
	}

	int32 Roll(const FIntPoint& Range) { return FMath::RandRange(FMath::Min(Range.X, Range.Y), FMath::Max(Range.X, Range.Y)); }

	/** Whole items from a fractional amount (the fraction is a chance of one more). */
	int32 Whole(float Amount) { return FMath::FloorToInt(Amount) + (FMath::FRand() < FMath::Frac(Amount) ? 1 : 0); }
}
using namespace WRFarmDetail;

// ---------------------------------------------------------------- biomes

bool WRBiome::IsLive(FName Biome)
{
	if (Biome.IsNone() || Biome == TEXT("any")) return true;
	static TArray<FString> Live;
	static bool bRead = false;
	if (!bRead)
	{
		bRead = true;
		FWRTuning::Get().Str(TEXT("world.liveBiomes"), TEXT("volcanic,tundra")).ParseIntoArray(Live, TEXT(","), true);
		for (FString& B : Live) B.TrimStartAndEndInline();
	}
	return Live.Contains(Biome.ToString());
}

FString WRBiome::Name(FName Biome)
{
	const FString N = FWRTuning::Get().Str(FString::Printf(TEXT("farm.biomeNames.%s"), *Biome.ToString()), FString());
	return N.IsEmpty() ? Biome.ToString().Replace(TEXT("_"), TEXT(" ")) : N;
}

bool FWRCropDef::IsLive() const { return WRBiome::IsLive(Biome); }

bool FWRAnimalDef::Eats(FName Item) const
{
	if (Feed.Contains(Item)) return true;
	if (Feed.Contains(TEXT("crop")) && FWRFarmDefs::Get().CropForProduce(Item)) return true;
	if (Feed.Contains(TEXT("spoiled")) && Item == TEXT("spoiled_food")) return true;
	return false;
}

int32 FWRDishDef::Specific() const
{
	int32 N = 0;
	for (const TArray<FName>& E : Pattern)
	{
		for (const FName& F : E)
		{
			if (FWRItemRegistry::Get().Find(F)) { ++N; break; }
		}
	}
	return N;
}

// ---------------------------------------------------------------- defs

FWRFarmDefs& FWRFarmDefs::Get()
{
	static FWRFarmDefs Instance;
	return Instance;
}

FWRFarmDefs::FWRFarmDefs()
{
	const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("farming.json"));
	const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
	if (Root.IsValid() && Root->TryGetArrayField(TEXT("crops"), List))
	{
		for (const TSharedPtr<FJsonValue>& V : *List)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			if (!O.IsValid() || !O->HasField(TEXT("id"))) continue;
			FWRCropDef C;
			C.Id = FName(*O->GetStringField(TEXT("id")));
			C.Name = O->GetStringField(TEXT("name"));
			FString S;
			C.Biome = O->TryGetStringField(TEXT("biome"), S) ? FName(*S) : FName(TEXT("any"));
			C.bRare = O->TryGetStringField(TEXT("rarity"), S) && S == TEXT("rare");
			const TArray<TSharedPtr<FJsonValue>>* Plots = nullptr;
			if (O->TryGetArrayField(TEXT("plots"), Plots)) for (const TSharedPtr<FJsonValue>& P : *Plots) C.Plots.Add(FName(*P->AsString()));
			if (C.Plots.Num() == 0) C.Plots.Add(TEXT("soil"));
			C.Needs = O->TryGetStringField(TEXT("needs"), S) ? FName(*S) : FName(TEXT("light"));
			C.Seed = FName(*O->GetStringField(TEXT("seed")));
			C.Produce = FName(*O->GetStringField(TEXT("produce")));
			C.Yield = ReadRange(O, TEXT("yield"), C.bRare ? FIntPoint(3, 5) : FIntPoint(9, 12));
			C.SeedsBack = ReadRange(O, TEXT("seedsBack"), C.bRare ? FIntPoint(1, 3) : FIntPoint(9, 11));
			C.Form = O->TryGetStringField(TEXT("form"), S) ? FName(*S) : FName(TEXT("bush"));
			C.Leaf = ReadColor(O, TEXT("leaf"), C.Leaf);
			C.Stem = ReadColor(O, TEXT("stem"), C.Stem);
			C.Fruit = ReadColor(O, TEXT("fruit"), C.Fruit);
			C.Flower = ReadColor(O, TEXT("flower"), C.Flower);
			double N = 0.0;
			if (O->TryGetNumberField(TEXT("height"), N)) C.Height = N;
			if (O->TryGetNumberField(TEXT("scream"), N)) C.Scream = N;
			CropList.Add(C);
		}
	}
	if (Root.IsValid() && Root->TryGetArrayField(TEXT("animals"), List))
	{
		for (const TSharedPtr<FJsonValue>& V : *List)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			if (!O.IsValid() || !O->HasField(TEXT("species"))) continue;
			FWRAnimalDef A;
			A.Species = FName(*O->GetStringField(TEXT("species")));
			const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
			if (O->TryGetArrayField(TEXT("products"), Arr))
			{
				for (const TSharedPtr<FJsonValue>& P : *Arr)
				{
					const TSharedPtr<FJsonObject> PO = P->AsObject();
					FWRAnimalDef::FProduct Prod;
					Prod.Item = FName(*PO->GetStringField(TEXT("item")));
					double Every = 600.0;
					PO->TryGetNumberField(TEXT("every"), Every);
					Prod.Every = Every;
					PO->TryGetBoolField(TEXT("shear"), Prod.bShear);
					PO->TryGetBoolField(TEXT("gentle"), Prod.bGentle);
					A.Products.Add(Prod);
				}
			}
			if (O->TryGetArrayField(TEXT("feed"), Arr)) for (const TSharedPtr<FJsonValue>& F : *Arr) A.Feed.Add(FName(*F->AsString()));
			FString S;
			if (O->TryGetStringField(TEXT("prize"), S)) A.Prize = FName(*S);
			AnimalList.Add(A);
		}
	}
	const TSharedPtr<FJsonObject>* WildObj = nullptr;
	if (Root.IsValid() && Root->TryGetObjectField(TEXT("wildSeeds"), WildObj))
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& B : (*WildObj)->Values)
		{
			const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
			if (!B.Value->TryGetArray(Arr)) continue;
			TArray<TPair<FName, float>>& Out = Wild.FindOrAdd(FName(*B.Key));
			for (const TSharedPtr<FJsonValue>& E : *Arr)
			{
				const TSharedPtr<FJsonObject> EO = E->AsObject();
				if (EO.IsValid()) Out.Add({ FName(*EO->GetStringField(TEXT("seed"))), float(EO->GetNumberField(TEXT("chance"))) });
			}
		}
	}

	const TSharedPtr<FJsonObject> Cook = FWRTuning::LoadDataFile(TEXT("cooking.json"));
	const TSharedPtr<FJsonObject>* GroupObj = nullptr;
	if (Cook.IsValid() && Cook->TryGetObjectField(TEXT("groups"), GroupObj))
	{
		// group -> [items]
		for (const TPair<FString, TSharedPtr<FJsonValue>>& G : (*GroupObj)->Values)
		{
			const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
			if (!G.Value->TryGetArray(Arr)) continue;
			for (const TSharedPtr<FJsonValue>& I : *Arr) Groups.Add(FName(*I->AsString()), FName(*G.Key));
		}
	}
	// Every fish item is Fish, every crop's produce Veg unless the groups say otherwise.
	for (const FWRFishDef& F : FWRFishRegistry::Get().All())
	{
		if (!Groups.Contains(F.Item)) Groups.Add(F.Item, TEXT("Fish"));
	}
	for (const FWRCropDef& C : CropList)
	{
		if (!Groups.Contains(C.Produce)) Groups.Add(C.Produce, TEXT("Veg"));
	}
	if (Cook.IsValid() && Cook->TryGetArrayField(TEXT("dishes"), List))
	{
		for (const TSharedPtr<FJsonValue>& V : *List)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			if (!O.IsValid() || !O->HasField(TEXT("id"))) continue;
			FWRDishDef D;
			D.Id = FName(*O->GetStringField(TEXT("id")));
			FString S;
			D.Station = O->TryGetStringField(TEXT("station"), S) ? FName(*S) : FName(TEXT("cooking_pot"));
			if (O->TryGetStringField(TEXT("biome"), S)) D.Biome = FName(*S);
			double N = 0.0;
			if (O->TryGetNumberField(TEXT("count"), N)) D.Count = FMath::Max(1, int32(N));
			if (O->TryGetNumberField(TEXT("seconds"), N)) D.Seconds = N;
			const TArray<TSharedPtr<FJsonValue>>* Pat = nullptr;
			if (O->TryGetArrayField(TEXT("pattern"), Pat))
			{
				for (const TSharedPtr<FJsonValue>& E : *Pat)
				{
					TArray<FString> Parts;
					E->AsString().ParseIntoArray(Parts, TEXT("|"), true);
					TArray<FName>& Entry = D.Pattern.AddDefaulted_GetRef();
					for (const FString& P : Parts) Entry.Add(FName(*P.TrimStartAndEnd()));
				}
			}
			if (D.Pattern.Num() > 0 && D.Pattern.Num() <= 4 && FWRItemRegistry::Get().Find(D.Id)) DishList.Add(D);
		}
	}
	UE_LOG(LogWildreach, Log, TEXT("Loaded %d crops, %d farm animals, %d dishes, %d food items"), CropList.Num(), AnimalList.Num(), DishList.Num(), Groups.Num());
}

const FWRCropDef* FWRFarmDefs::Crop(FName Id) const { return CropList.FindByPredicate([Id](const FWRCropDef& C) { return C.Id == Id; }); }
const FWRCropDef* FWRFarmDefs::CropForSeed(FName Seed) const { return CropList.FindByPredicate([Seed](const FWRCropDef& C) { return C.Seed == Seed; }); }
const FWRCropDef* FWRFarmDefs::CropForProduce(FName Produce) const { return CropList.FindByPredicate([Produce](const FWRCropDef& C) { return C.Produce == Produce; }); }
const FWRAnimalDef* FWRFarmDefs::Animal(FName Species) const { return AnimalList.FindByPredicate([Species](const FWRAnimalDef& A) { return A.Species == Species; }); }
const FWRDishDef* FWRFarmDefs::Dish(FName Id) const { return DishList.FindByPredicate([Id](const FWRDishDef& D) { return D.Id == Id; }); }

FName FWRFarmDefs::GroupOf(FName Item) const
{
	const FName* G = Groups.Find(Item);
	return G ? *G : NAME_None;
}

bool FWRFarmDefs::Fills(const TArray<FName>& Entry, FName Item) const
{
	const FName Group = GroupOf(Item);
	for (const FName& E : Entry)
	{
		if (E == Item) return true;
		if (!Group.IsNone() && E == Group) return true;
		// A golden crop fills any Veg or Fruit.
		if (Item == TEXT("golden_crop") && (E == TEXT("Veg") || E == TEXT("Fruit"))) return true;
	}
	return false;
}

const FWRDishDef* FWRFarmDefs::Match(FName Station, const TArray<FName>& Ingredients) const
{
	const FWRDishDef* Best = nullptr;
	for (const FWRDishDef& D : DishList)
	{
		if (D.Station != Station || D.Pattern.Num() != Ingredients.Num() || !WRBiome::IsLive(D.Biome)) continue;
		// One to one: some ordering of the ingredients fills every entry (4! = 24 at most).
		TArray<int32> Order;
		for (int32 i = 0; i < Ingredients.Num(); ++i) Order.Add(i);
		bool bFits = false;
		do
		{
			bool bAll = true;
			for (int32 i = 0; i < Order.Num() && bAll; ++i) bAll = Fills(D.Pattern[i], Ingredients[Order[i]]);
			bFits = bAll;
		}
		while (!bFits && [&Order]()
		{
			// next_permutation
			int32 i = Order.Num() - 2;
			while (i >= 0 && Order[i] >= Order[i + 1]) --i;
			if (i < 0) return false;
			int32 j = Order.Num() - 1;
			while (Order[j] <= Order[i]) --j;
			Swap(Order[i], Order[j]);
			for (int32 a = i + 1, b = Order.Num() - 1; a < b; ++a, --b) Swap(Order[a], Order[b]);
			return true;
		}());
		if (bFits && (!Best || D.Specific() > Best->Specific())) Best = &D;
	}
	return Best;
}

FString FWRFarmDefs::PatternText(const FWRDishDef& Dish, bool bHideItems) const
{
	TArray<FString> Parts;
	for (const TArray<FName>& E : Dish.Pattern)
	{
		TArray<FString> Alts;
		for (const FName& F : E)
		{
			const FWRItemDef* Item = FWRItemRegistry::Get().Find(F);
			Alts.Add(Item ? (bHideItems ? FString(TEXT("?")) : Item->Name) : F.ToString());
		}
		Parts.Add(FString::Join(Alts, TEXT(" or ")));
	}
	return FString::Join(Parts, TEXT(" + "));
}

// ---------------------------------------------------------------- plots

namespace WRFarmDetail
{
	FName PlotType(const FWRPieceDef& Def) { return Def.Plot.IsNone() ? FName(TEXT("soil")) : Def.Plot; }

	bool CanGrowAt(const FWRCropDef& Crop, const AWRBuildPiece* Plot, const AActor* Planter, FString& Why)
	{
		const FWRPieceDef* Def = Plot->GetDef();
		if (!Def) return false;
		if (!Crop.IsLive())
		{
			Why = TEXT("not in this world");
			return false;
		}
		if (!Crop.Plots.Contains(PlotType(*Def)))
		{
			const FName Want = Crop.Plots[0];
			Why = FString::Printf(TEXT("%s grows only in %s"), *Crop.Name, Want == TEXT("ashpit") ? TEXT("an ash pit") : Want == TEXT("paddy") ? TEXT("a paddy") : Want == TEXT("kelp") ? TEXT("a kelp line")
				: Want == TEXT("log") ? TEXT("a log bed") : TEXT("a crop plot"));
			return false;
		}
		return true;
	}
}

bool FWRFarm::IsPieceLive(const FWRPieceDef& Def)
{
	if (!Def.Biome.IsNone() && !WRBiome::IsLive(Def.Biome)) return false;
	if (!Def.LiveWith.IsNone() && !WRBiome::IsLive(Def.LiveWith)) return false;
	if (!Def.NeedsCreature.IsNone() && !FWRCreatureRegistry::Get().Find(Def.NeedsCreature)) return false;
	if (!Def.Plot.IsNone())
	{
		// A plot type no live crop grows in waits for its biome.
		bool bAny = false;
		for (const FWRCropDef& C : FWRFarmDefs::Get().Crops()) bAny |= C.IsLive() && C.Plots.Contains(Def.Plot);
		if (!bAny) return false;
	}
	if (!Def.Trophy.IsNone())
	{
		const FWRFishDef* F = FWRFishRegistry::Get().Find(Def.Trophy);
		if (!F || !WRBiome::IsLive(F->Biome)) return false;
	}
	return true;
}

FString FWRFarm::PlaceCheck(const UObject* WorldContext, const FWRPieceDef& Def, const FVector& Location)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World) return FString();
	if (!IsPieceLive(Def)) return TEXT("not in this world yet");
	if (Def.HearthLevel > 1 && Def.Kind != TEXT("machine"))   // machines: a Hearth of yours anywhere (FWRMachines::PlaceCheck)
	{
		const AWRHearth* Hearth = AWRHearth::FindCovering(World, Location);
		const int32 Tier = Hearth && !Hearth->IsBroken() ? Hearth->GetTechLevel() : 1;
		if (Tier < Def.HearthLevel) return FString::Printf(TEXT("needs a Hearth of level %d here"), Def.HearthLevel);
	}
	if (!Def.Biome.IsNone() && WRBiome::At(World, Location) != Def.Biome)
	{
		return FString::Printf(TEXT("only in %s"), *WRBiome::Name(Def.Biome));
	}
	const UWRWaterSubsystem* Water = UWRWaterSubsystem::Get(World);
	const float Depth = Water ? Water->DepthAt(Location) : 0.f;
	const FName P = Def.Plot;
	if (P == TEXT("paddy") && (Depth < Tune(TEXT("farm.paddy.minDepth"), 10.f) || Depth > Tune(TEXT("farm.paddy.maxDepth"), 50.f)))
	{
		return TEXT("a paddy stands in 10 to 50 cm of water");
	}
	if (P == TEXT("kelp") && (Depth < Tune(TEXT("farm.kelp.minDepth"), 200.f) || Depth > Tune(TEXT("farm.kelp.maxDepth"), 600.f)))
	{
		return TEXT("a kelp line goes in sea water 2 to 6 m deep");
	}
	if (Def.Kind == TEXT("trap") && Depth < Tune(TEXT("farm.trap.minDepth"), 50.f))
	{
		return TEXT("a fish trap needs water at least 50 cm deep");
	}
	if (Def.Kind == TEXT("saltpan"))
	{
		bool bShore = false;
		const float R = Tune(TEXT("farm.saltpan.shoreRange"), 600.f);
		for (int32 i = 0; i < 8 && Water; ++i)
		{
			const float A = i * PI / 4.f;
			bShore |= Water->DepthAt(Location + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * R) > 0.f;
		}
		if (!bShore || !Water || Location.Z > Water->GetSurfaceZ() + Tune(TEXT("farm.saltpan.maxAboveSea"), 200.f) || Depth > 0.f)
		{
			return TEXT("a salt pan goes on the upper beach, by the sea");
		}
	}
	return FString();
}

float FWRFarm::GrowthRate(const AWRBuildPiece* Plot, EWRFarmStop& Why, uint8& Flags)
{
	Why = EWRFarmStop::None;
	Flags &= WRFarmFlag::Fertilised;
	const FWRCropDef* Crop = FWRFarmDefs::Get().Crop(Plot->CropId);
	const FWRPieceDef* Def = Plot->GetDef();
	const UWorld* World = Plot->GetWorld();
	if (!Crop || !Def || !World) return 0.f;
	const FVector At = Plot->GetActorLocation() + FVector(0.f, 0.f, 60.f);
	const FName Biome = WRBiome::At(World, At);
	float Rate = 1.f / (60.f * (Crop->bRare ? WRTune(TEXT("crops.rareMinutes"), 120.f) : WRTune(TEXT("crops.commonMinutes"), 20.f)))
		* FMath::Max(0.1f, WRTune(TEXT("server.cropMultiplier"), 1.f)) * FMath::Max(0.1f, Plot->PlanterSpeed);

	bool bCovered = false;
	const AWRBuildPiece* Roof = RoofAbove(World, At, Tune(TEXT("farm.roofHeight"), 1500.f), Plot, bCovered);
	const bool bUnderground = WRBiome::IsUnderground(World, At);
	const bool bGlassRoof = Roof && Roof->GetDef() && Roof->GetDef()->bGlass;
	const bool bWalled = (Roof || bCovered) && Walled(World, At + FVector(0.f, 0.f, 60.f), Tune(TEXT("farm.wallReach"), 800.f), Plot) >= Tune(TEXT("farm.greenhouseWalls"), 0.8f);
	// Greenhouse lights (cooks/build-machinery.md 2.3): a walled room lit by running grow lights is a greenhouse even
	// with no glass and no sun (indoors, at night, underground), and it isn't dark.
	const UWRMachineSubsystem* Machines = UWRMachineSubsystem::Get(World);
	const bool bGrowLit = Machines && Machines->GrowLightNear(At, FWRMachineDefs::Get().Rule(TEXT("growLightReach"), 800.f));
	const bool bGreenhouse = (bGlassRoof || ((Roof || bCovered) && bGrowLit)) && bWalled;
	const bool bDaylight = !bUnderground && !bCovered && (!Roof || bGlassRoof);
	const bool bDarkShed = !bGrowLit && (bUnderground || ((Roof || bCovered) && !bGlassRoof && bWalled && !WindowNear(World, At, Tune(TEXT("farm.wallReach"), 800.f))));
	if (bGreenhouse) Flags |= WRFarmFlag::Greenhouse;

	if (Crop->Biome != TEXT("any") && Crop->Biome != Biome && !bGreenhouse)
	{
		if (!Plot->bPlanterGraft)
		{
			Why = EWRFarmStop::WrongBiome;
			return 0.f;
		}
		Flags |= WRFarmFlag::Grafted;
	}
	if (!Crop->Plots.Contains(PlotType(*Def)))
	{
		Why = EWRFarmStop::WrongPlot;
		return 0.f;
	}
	if (Crop->Needs == TEXT("dark"))
	{
		if (!bDarkShed) { Why = EWRFarmStop::NeedsDark; return 0.f; }
	}
	else if (!bDaylight && !bGreenhouse)
	{
		Why = EWRFarmStop::NoDaylight;
		return 0.f;
	}
	if (Crop->Needs == TEXT("windy") && !bGreenhouse && Biome != TEXT("storm_peaks") && At.Z < Tune(TEXT("farm.windyAbove"), 30000.f))
	{
		Why = EWRFarmStop::NeedsWind;
		return 0.f;
	}
	if (Crop->Needs == TEXT("withered") && !bGreenhouse && Biome != TEXT("wasteland"))
	{
		Why = EWRFarmStop::NeedsWithered;
		return 0.f;
	}
	// The survival lane's Ashfall: outdoor plots stop while ash falls (a roof keeps them going).
	const FWRWeatherKind* Weather = UWRWeatherSubsystem::ActiveKind(World, Biome);
	if (Weather && Weather->bCropsPause && !Roof && !bCovered)
	{
		Why = EWRFarmStop::Ashfall;
		return 0.f;
	}
	const UWRFarmSubsystem* Farm = UWRFarmSubsystem::Get(World);
	const bool bRain = Weather && Weather->bWatersCrops && !Roof && !bCovered;
	if (bRain || (Farm && Farm->IsWateredAt(Plot->GetActorLocation(), Tune(TEXT("farm.waterRange"), 800.f))) || PlotType(*Def) == TEXT("paddy") || PlotType(*Def) == TEXT("kelp"))
	{
		Flags |= WRFarmFlag::Watered;
		Rate /= Plot->bPlanterWaterWise ? Tune(TEXT("farm.waterWiseTime"), 0.5f) : Tune(TEXT("farm.wateredTime"), 0.67f);
	}
	if (Farm && Farm->HasHiveNear(Plot->GetActorLocation(), Tune(TEXT("farm.hive.pollinateRange"), 2000.f)))
	{
		Flags |= WRFarmFlag::Pollinated;
		Rate /= Tune(TEXT("farm.hive.pollinateTime"), 0.85f);
	}
	if (Plot->bPlanterCompanion && Farm && Farm->HasOtherCropNear(Plot, Tune(TEXT("farm.companionRange"), 800.f)))
	{
		Flags |= WRFarmFlag::Companion;
		Rate /= Tune(TEXT("farm.companionTime"), 0.9f);
	}
	return Rate;
}

FString FWRFarm::Plant(AWRBuildPiece* Plot, AWRPlayerController* Player, FName Seed)
{
	AWRPlayerState* PS = Player ? Player->GetPlayerState<AWRPlayerState>() : nullptr;
	const FWRCropDef* Crop = FWRFarmDefs::Get().CropForSeed(Seed);
	if (!PS || !Plot || !Crop) return TEXT("that won't grow here");
	if (!Plot->CropId.IsNone()) return TEXT("something is already growing here");
	FString Why;
	if (!CanGrowAt(*Crop, Plot, Player, Why)) return Why;
	const int32 Need = FMath::RoundToInt(Tune(TEXT("farm.seedsPerPlot"), 9.f));
	const int32 Have = PS->GetInventory()->CountOf(Seed);
	if (Have < Need) return FString::Printf(TEXT("Need %d %s to plant a plot (you have %d)"), Need, *ItemName(Seed), Have);
	PS->GetInventory()->RemoveItem(Seed, Need);
	Plot->CropId = Crop->Id;
	Plot->LastCrop = Crop->Id;
	Plot->bPlanted = true;
	Plot->Growth = 0.f;
	Plot->Growth8 = 0;
	Plot->FarmFlags &= ~WRFarmFlag::Fertilised;
	Plot->Planter = PS->GetPlayerName();
	// The planter's hand stays with the plot: their growing speed, and the perks that work on their own plots.
	Plot->PlanterSpeed = 1.f + UWRLifeSkillsComponent::StatFor(PS, TEXT("cropSpeed"));
	Plot->bPlanterWaterWise = UWRLifeSkillsComponent::StatFor(PS, TEXT("waterWise")) > 0.f;
	Plot->bPlanterCompanion = UWRLifeSkillsComponent::StatFor(PS, TEXT("companion")) > 0.f;
	Plot->bPlanterGraft = UWRLifeSkillsComponent::StatFor(PS, TEXT("grafting")) > 0.f;
	UWRLifeSkillsComponent::Award(PS, TEXT("farming"), TEXT("plant"), 10.f);
	WRFx::Broadcast(Plot, TEXT("gather.fiber"), Plot->GetActorLocation() + FVector(0.f, 0.f, 30.f), FRotator::ZeroRotator, 1.4f, TEXT("farm.plant"));
	EWRFarmStop Stop = EWRFarmStop::None;
	uint8 Flags = Plot->FarmFlags;
	const float Rate = GrowthRate(Plot, Stop, Flags);
	Plot->FarmFlags = Flags | (Rate <= 0.f ? WRFarmFlag::Stopped : 0);
	Plot->FarmStop = uint8(Stop);
	Plot->OnRep_Farm();
	if (Rate <= 0.f) return FString::Printf(TEXT("Planted. %s"), *Describe(Plot, Plot));
	return FString::Printf(TEXT("Planted %s: ripe in about %d min%s"), *Crop->Name, FMath::Max(1, FMath::RoundToInt(1.f / Rate / 60.f)),
		(Flags & WRFarmFlag::Watered) ? TEXT(" (watered)") : TEXT(" (water it: a well, a channel or the shore within 8 m grows it faster)"));
}

namespace WRFarmDetail
{
	FString Harvest(AWRBuildPiece* Plot, AWRPlayerController* Player)
	{
		AWRPlayerState* PS = Player->GetPlayerState<AWRPlayerState>();
		const FWRCropDef* Crop = FWRFarmDefs::Get().Crop(Plot->CropId);
		if (!PS || !Crop) return FString();
		UWRInventoryComponent* Inv = PS->GetInventory();
		const bool bFert = (Plot->FarmFlags & WRFarmFlag::Fertilised) != 0;
		const bool bGraft = (Plot->FarmFlags & WRFarmFlag::Grafted) != 0;
		float Yield = Roll(Crop->Yield) * (1.f + UWRLifeSkillsComponent::StatFor(PS, TEXT("harvestExtra"))) * WRTune(TEXT("server.gatherMultiplier"), 1.f);
		if (bFert) Yield *= Tune(TEXT("farm.fertiliserYield"), 1.25f);
		if (bGraft) Yield *= Tune(TEXT("farm.graftYield"), 0.5f);
		const int32 Got = FMath::Max(1, Whole(Yield));
		const int32 Seeds = Whole(Roll(Crop->SeedsBack) * (1.f + UWRLifeSkillsComponent::StatFor(PS, TEXT("seedExtra"))));
		TArray<FWRItemStack> Over;
		const auto Give = [Inv, &Over](FName Item, int32 N)
		{
			const int32 Left = N > 0 ? Inv->AddItem(Item, N) : 0;
			if (Left > 0) { FWRItemStack& S = Over.AddDefaulted_GetRef(); S.ItemId = Item; S.Count = Left; if (FWRSpoil::Perishes(Item)) FWRSpoil::StampNew(S, 1.f, FWRSpoil::ServerNow); }
		};
		Give(Crop->Produce, Got);
		FString Extra;
		// Endless field: the plot replants itself from the harvest's own seeds.
		const int32 Need = FMath::RoundToInt(Tune(TEXT("farm.seedsPerPlot"), 9.f));
		const bool bReplant = UWRLifeSkillsComponent::StatFor(PS, TEXT("autoReplant")) > 0.f && Seeds >= Need;
		Give(Crop->Seed, bReplant ? Seeds - Need : Seeds);
		// Rare seeds: now and then a seed of this biome's rare crop. Golden crop: a golden one.
		if (FMath::FRand() < UWRLifeSkillsComponent::StatFor(PS, TEXT("rareSeeds")))
		{
			for (const FWRCropDef& C : FWRFarmDefs::Get().Crops())
			{
				if (C.bRare && C.Biome == Crop->Biome && C.Id != Crop->Id) { Give(C.Seed, 1); Extra += FString::Printf(TEXT(", a %s"), *ItemName(C.Seed).ToLower()); break; }
			}
		}
		if (FMath::FRand() < UWRLifeSkillsComponent::StatFor(PS, TEXT("goldenCrop")))
		{
			Give(TEXT("golden_crop"), 1);
			Extra += TEXT(", a golden crop!");
		}
		if (Over.Num() > 0)
		{
			AWRLootBag::Spill(Plot->GetWorld(), Plot->GetActorLocation() + FVector(0.f, 0.f, 40.f), PS->GetPlayerName(), Over);
			Extra += TEXT(" (bag full: the rest is on the ground)");
		}
		UWRLifeSkillsComponent::Award(PS, TEXT("farming"), TEXT("harvest"), 20.f);
		if (UWRFoodComponent* Food = PS->FindComponentByClass<UWRFoodComponent>())
		{
			if (!Food->CropsHarvested.Contains(Crop->Id))
			{
				Food->CropsHarvested.Add(Crop->Id);
				UWRLifeSkillsComponent::Award(PS, TEXT("farming"), TEXT("firstHarvest"), 50.f);
				Extra += FString::Printf(TEXT(". First %s harvest!"), *Crop->Name.ToLower());
			}
		}
		// The mandrake's scream: everything near comes for you, and it knocks you off your feet, without ear plugs.
		if (Crop->Scream > 0.f && Inv->CountOf(TEXT("wax_earplugs")) <= 0)
		{
			AWRCharacterBase* Body = Cast<AWRCharacterBase>(Player->GetPawn());
			if (Body && Body->Status) Body->Status->Stagger(Tune(TEXT("farm.screamStagger"), 2.f));
			for (TActorIterator<AWRCreature> It(Plot->GetWorld()); It; ++It)
			{
				if (It->IsAlive() && !It->IsTamed() && Body && FVector::Dist(It->GetActorLocation(), Plot->GetActorLocation()) < Crop->Scream) It->SetAggroTarget(Body);
			}
			WRFx::Broadcast(Plot, TEXT("discover"), Plot->GetActorLocation() + FVector(0.f, 0.f, 50.f), FRotator::ZeroRotator, 1.f, TEXT("farm.mandrake"));
			Extra += TEXT(". The mandrake SCREAMS (wax ear plugs stop it)");
		}
		WRFx::Broadcast(Plot, TEXT("gather.fiber"), Plot->GetActorLocation() + FVector(0.f, 0.f, 50.f), FRotator::ZeroRotator, 2.f, TEXT("farm.harvest"));
		if (ACharacter* Body = Player->GetCharacter())
		{
			if (AWRCharacter* Hero = Cast<AWRCharacter>(Body)) Hero->MulticastBodyOneShot(TEXT("pickup"));
		}
		const FString Said = FString::Printf(TEXT("Harvested %d %s, %d seeds%s%s"), Got, *ItemName(Crop->Produce).ToLower(), bReplant ? Seeds - Need : Seeds, *Extra, bReplant ? TEXT(" (replanted)") : TEXT(""));
		UE_LOG(LogWildreach, Log, TEXT("FARM %s harvested %s: %s"), *PS->GetPlayerName(), *Crop->Id.ToString(), *Said);
		Plot->CropId = bReplant ? Crop->Id : NAME_None;
		Plot->bPlanted = bReplant;
		Plot->Growth = 0.f;
		Plot->Growth8 = 0;
		Plot->FarmFlags = 0;
		Plot->FarmStop = 0;
		Plot->OnRep_Farm();
		return Said;
	}

	FString CollectStorage(AWRBuildPiece* Piece, AWRPlayerController* Player, const TCHAR* Skill, const TCHAR* XPKey, float XP, const TCHAR* Empty)
	{
		AWRPlayerState* PS = Player->GetPlayerState<AWRPlayerState>();
		if (!PS) return FString();
		int32 Collected = 0;
		TArray<FString> What;
		for (FWRItemStack& S : Piece->Storage)
		{
			if (S.IsEmpty()) continue;
			const FName Id = S.ItemId;
			const int32 Moved = FWRInventoryRules::MoveFrom(S, PS->GetInventory()->GetSlotsMutable(), S.Count);
			if (Moved > 0) { Collected += Moved; What.AddUnique(ItemName(Id).ToLower()); }
		}
		if (Collected == 0) return Empty;
		UWRLifeSkillsComponent::Award(PS, Skill, XPKey, XP, Collected);
		WRFx::Broadcast(Piece, TEXT("gather.fiber"), Piece->GetActorLocation() + FVector(0.f, 0.f, 60.f), FRotator::ZeroRotator, 0.8f, TEXT("animal.collect"));
		Piece->OnRep_Farm();
		return FString::Printf(TEXT("Collected %d: %s"), Collected, *FString::Join(What, TEXT(", ")));
	}

	/** Fills a pen's trough from the bag with what its animals eat (up to farm.pen.trough). */
	int32 FillTrough(AWRBuildPiece* Pen, UWRInventoryComponent* Inv, FName Only = NAME_None)
	{
		TSet<FName> Kinds;
		for (const AWRCreature* A : PenAnimals(Pen)) Kinds.Add(A->CreatureId);
		const int32 Cap = FMath::RoundToInt(Tune(TEXT("farm.pen.trough"), 50.f));
		int32 Put = 0;
		for (FWRItemStack& S : Inv->GetSlotsMutable())
		{
			if (S.IsEmpty() || Pen->Feed >= Cap || (!Only.IsNone() && S.ItemId != Only)) continue;
			bool bEaten = false;
			for (const FName K : Kinds) if (const FWRAnimalDef* A = FWRFarmDefs::Get().Animal(K)) bEaten |= A->Eats(S.ItemId);
			if (!bEaten) continue;
			const int32 N = FMath::Min(S.Count, Cap - Pen->Feed);
			S.Count -= N;
			if (S.Count <= 0) S = FWRItemStack();
			Pen->Feed += N;
			Put += N;
		}
		if (Put > 0) Pen->OnRep_Farm();
		return Put;
	}
}

FString FWRFarm::Interact(AWRBuildPiece* Piece, AWRPlayerController* Player)
{
	AWRPlayerState* PS = Player ? Player->GetPlayerState<AWRPlayerState>() : nullptr;
	const FWRPieceDef* Def = Piece ? Piece->GetDef() : nullptr;
	if (!PS || !Def) return FString();
	const FName K = Def->Kind;
	if (!Piece->CanAccess(PS->GetPlayerName()) && K != TEXT("well") && K != TEXT("channel"))
	{
		return FString::Printf(TEXT("%s's %s"), *Piece->OwnerName, *Def->Name.ToLower());
	}
	UWRInventoryComponent* Inv = PS->GetInventory();
	if (K == TEXT("crop"))
	{
		if (Piece->CropId.IsNone())
		{
			// The seed that grows here you hold most of (the plot's last crop first).
			FName Best;
			int32 BestCount = -1;
			const int32 Need = FMath::RoundToInt(Tune(TEXT("farm.seedsPerPlot"), 9.f));
			for (const FWRCropDef& C : FWRFarmDefs::Get().Crops())
			{
				FString Why;
				const int32 Have = Inv->CountOf(C.Seed);
				if (Have <= 0 || !CanGrowAt(C, Piece, Player, Why)) continue;
				const int32 Score = Have + (C.Id == Piece->LastCrop && Have >= Need ? 100000 : 0);
				if (Score > BestCount) { BestCount = Score; Best = C.Seed; }
			}
			if (Best.IsNone()) return FString::Printf(TEXT("Plant %d seeds of a crop that grows in %s (shrubs and wild plants drop them)"), Need, PlotType(*Def) == TEXT("ashpit") ? TEXT("an ash pit") : TEXT("a crop plot"));
			return Plant(Piece, Player, Best);
		}
		if (Piece->Growth >= 1.f)
		{
			return Harvest(Piece, Player);
		}
		// Growing: fertilise it if you carry compost or bone meal and it hasn't had any.
		if (!(Piece->FarmFlags & WRFarmFlag::Fertilised))
		{
			for (const FName F : { FName(TEXT("compost")), FName(TEXT("bone_meal")), FName(TEXT("stone_dust")) })
			{
				if (Inv->CountOf(F) > 0)
				{
					Inv->RemoveItem(F, 1);
					Piece->FarmFlags |= WRFarmFlag::Fertilised;
					Piece->OnRep_Farm();
					UWRLifeSkillsComponent::Award(PS, TEXT("farming"), TEXT("fertilise"), 3.f);
					WRFx::Broadcast(Piece, TEXT("gather.fiber"), Piece->GetActorLocation() + FVector(0.f, 0.f, 25.f), FRotator::ZeroRotator, 1.f, TEXT("farm.plant"));
					return FString::Printf(TEXT("Fertilised with %s: +25%% at harvest"), *ItemName(F).ToLower());
				}
			}
		}
		return Describe(Piece, Piece);
	}
	if (K == TEXT("pen"))
	{
		// Feed first (what the animals eat, from the bag), then collect what's on the shelf.
		const int32 Fed = Piece->Feed < FMath::RoundToInt(Tune(TEXT("farm.pen.trough"), 50.f)) ? FillTrough(Piece, Inv) : 0;
		const FString Got = CollectStorage(Piece, Player, TEXT("husbandry"), TEXT("collect"), 5.f, TEXT(""));
		if (Fed > 0 || !Got.IsEmpty())
		{
			UWRLifeSkillsComponent::Award(PS, TEXT("husbandry"), TEXT("feed"), 5.f, Fed > 0 ? 1.f : 0.f);
			return FString::Printf(TEXT("%s%s%s"), Fed > 0 ? *FString::Printf(TEXT("Filled the trough (+%d feed, %d)"), Fed, Piece->Feed) : TEXT(""), Fed > 0 && !Got.IsEmpty() ? TEXT(". ") : TEXT(""), *Got);
		}
		return Describe(Piece, Piece);
	}
	if (K == TEXT("hive"))
	{
		const FString Got = CollectStorage(Piece, Player, TEXT("husbandry"), TEXT("hive"), 5.f, TEXT(""));
		if (!Got.IsEmpty() && FMath::FRand() < UWRLifeSkillsComponent::StatFor(PS, TEXT("royalJelly")))
		{
			Inv->AddItem(TEXT("royal_jelly"), 1);
			return Got + TEXT(", and royal jelly!");
		}
		return Got.IsEmpty() ? Describe(Piece, Piece) : Got;
	}
	if (K == TEXT("trap"))
	{
		const FString Got = CollectStorage(Piece, Player, TEXT("fishing"), TEXT("trapFish"), 2.f, TEXT(""));
		return Got.IsEmpty() ? Describe(Piece, Piece) : Got;
	}
	if (K == TEXT("saltpan"))
	{
		const FString Got = CollectStorage(Piece, Player, TEXT("cooking"), TEXT("salt"), 1.f, TEXT(""));
		return Got.IsEmpty() ? Describe(Piece, Piece) : Got;
	}
	return Describe(Piece, Piece);
}

FString FWRFarm::Describe(const AWRBuildPiece* Piece, const UObject* WorldContext)
{
	const FWRPieceDef* Def = Piece ? Piece->GetDef() : nullptr;
	if (!Def) return FString();
	const FName K = Def->Kind;
	const EWRFarmStop Stop = EWRFarmStop(Piece->FarmStop);
	const bool bStopped = (Piece->FarmFlags & WRFarmFlag::Stopped) != 0;
	if (K == TEXT("crop"))
	{
		const FWRCropDef* Crop = FWRFarmDefs::Get().Crop(Piece->CropId);
		if (!Crop) return FString::Printf(TEXT("%s: empty. Plant 9 seeds (E)"), *Def->Name);
		if (Piece->Growth8 >= 255) return FString::Printf(TEXT("%s: ripe. Harvest (E)"), *Crop->Name);
		if (bStopped)
		{
			FString Why;
			switch (Stop)
			{
			case EWRFarmStop::WrongBiome: Why = FString::Printf(TEXT("wrong ground: grows in %s (or a greenhouse)"), *WRBiome::Name(Crop->Biome)); break;
			case EWRFarmStop::WrongPlot: Why = TEXT("wrong plot for it"); break;
			case EWRFarmStop::NoDaylight: Why = TEXT("no daylight: it needs open sky or a glass roof"); break;
			case EWRFarmStop::NeedsDark: Why = TEXT("too much light: it grows only in the dark (a cave, or a roofed shed with no windows)"); break;
			case EWRFarmStop::NeedsWind: Why = TEXT("no wind: it grows high up (300 m) or in the storm peaks"); break;
			case EWRFarmStop::NeedsWithered: Why = TEXT("it takes only withered ground"); break;
			case EWRFarmStop::Ashfall: Why = TEXT("ash is falling on it (a roof keeps it growing)"); break;
			default: Why = TEXT("not growing"); break;
			}
			return FString::Printf(TEXT("%s: %s"), *Crop->Name, *Why);
		}
		const int32 Pct = FMath::RoundToInt(Piece->Growth8 / 2.55f);
		const TCHAR* Stage = Pct < 25 ? TEXT("sprouting") : Pct < 60 ? TEXT("in leaf") : Pct < 90 ? TEXT("flowering") : TEXT("ripening");
		TArray<FString> Help;
		if (Piece->FarmFlags & WRFarmFlag::Watered) Help.Add(TEXT("watered"));
		if (Piece->FarmFlags & WRFarmFlag::Fertilised) Help.Add(TEXT("fertilised"));
		if (Piece->FarmFlags & WRFarmFlag::Pollinated) Help.Add(TEXT("bees"));
		if (Piece->FarmFlags & WRFarmFlag::Greenhouse) Help.Add(TEXT("greenhouse"));
		if (Piece->FarmFlags & WRFarmFlag::Companion) Help.Add(TEXT("companions"));
		if (Piece->FarmFlags & WRFarmFlag::Grafted) Help.Add(TEXT("grafted: half yield"));
		return FString::Printf(TEXT("%s: %s, %d%%%s%s"), *Crop->Name, Stage, Pct, Help.Num() > 0 ? TEXT(" · ") : TEXT(""), *FString::Join(Help, TEXT(", ")));
	}
	int32 Waiting = 0;
	for (const FWRItemStack& S : Piece->Storage) Waiting += S.IsEmpty() ? 0 : S.Count;
	const FString Ready = Waiting > 0 ? FString::Printf(TEXT(" · %d to collect (E)"), Waiting) : FString();
	if (K == TEXT("pen"))
	{
		const FString Trough = FString::Printf(TEXT("trough %d/%d"), Piece->Feed, FMath::RoundToInt(Tune(TEXT("farm.pen.trough"), 50.f)));
		if (bStopped && Stop == EWRFarmStop::Hungry) return FString::Printf(TEXT("Animal pen: hungry, fill the trough (E with their feed) · %s%s"), *Trough, *Ready);
		if (bStopped && Stop == EWRFarmStop::Full) return FString::Printf(TEXT("Animal pen: the shelf is full · %s%s"), *Trough, *Ready);
		return FString::Printf(TEXT("Animal pen · %s%s"), *Trough, *Ready);
	}
	if (K == TEXT("hive"))
	{
		if (!(Piece->FarmFlags & WRFarmFlag::Working) && Waiting == 0) return bStopped && Stop == EWRFarmStop::NoFlowers ? TEXT("Beehive: no flowers or crops within 40 m") : TEXT("Beehive: tell a tamed Hiveback to stay at it");
		return FString::Printf(TEXT("Beehive%s%s"), (Piece->FarmFlags & WRFarmFlag::Working) ? TEXT(": humming") : TEXT(""), *Ready);
	}
	if (K == TEXT("trap")) return bStopped ? FString::Printf(TEXT("Fish trap: %s%s"), Stop == EWRFarmStop::Full ? TEXT("full") : TEXT("out of the water"), *Ready) : FString::Printf(TEXT("Fish trap: set%s"), *Ready);
	if (K == TEXT("saltpan")) return bStopped ? FString::Printf(TEXT("Salt pan: %s%s"), Stop == EWRFarmStop::NoSun ? TEXT("drying stops at night") : Stop == EWRFarmStop::Full ? TEXT("full") : TEXT("not on the beach"), *Ready) : FString::Printf(TEXT("Salt pan: drying%s"), *Ready);
	if (K == TEXT("well")) return TEXT("Well: waters crop plots within 8 m, and feeds irrigation channels laid from it");
	if (K == TEXT("channel")) return (Piece->FarmFlags & WRFarmFlag::Watered) ? TEXT("Irrigation channel: running, waters plots within 8 m") : TEXT("Irrigation channel: dry. Lay it end to end from water or a well");
	return Def->Name;
}

FString FWRFarm::UseItem(UWRInventoryComponent* Inventory, int32 SlotIndex)
{
	const TArray<FWRItemStack>& Slots = Inventory ? Inventory->GetSlots() : TArray<FWRItemStack>();
	if (!Slots.IsValidIndex(SlotIndex) || Slots[SlotIndex].IsEmpty()) return FString();
	const FName Item = Slots[SlotIndex].ItemId;
	AWRPlayerState* PS = Cast<AWRPlayerState>(Inventory->GetOwner());
	AWRPlayerController* PC = PS ? Cast<AWRPlayerController>(PS->GetOwner()) : nullptr;
	const APawn* Pawn = PS ? PS->GetPawn() : nullptr;
	if (!PC || !Pawn) return FString();
	const auto Nearest = [Pawn, PS](FName Kind) -> AWRBuildPiece*
	{
		AWRBuildPiece* Best = nullptr;
		float BestD = 450.f;
		for (TActorIterator<AWRBuildPiece> It(Pawn->GetWorld()); It; ++It)
		{
			const FWRPieceDef* D = It->bGhost ? nullptr : It->GetDef();
			const float Dist = FVector::Dist2D(It->GetActorLocation(), Pawn->GetActorLocation());
			if (D && D->Kind == Kind && Dist < BestD && It->CanAccess(PS->GetPlayerName())) { Best = *It; BestD = Dist; }
		}
		return Best;
	};
	if (FWRFarmDefs::Get().CropForSeed(Item))
	{
		AWRBuildPiece* Plot = Nearest(TEXT("crop"));
		if (!Plot) return FString::Printf(TEXT("Stand by an empty crop plot to plant %s"), *ItemName(Item).ToLower());
		return Plant(Plot, PC, Item);
	}
	if (Item == TEXT("recipe_scrap"))
	{
		UWRFoodComponent* Food = UWRFoodComponent::For(PS);
		TArray<const FWRDishDef*> Unknown;
		for (const FWRDishDef& D : FWRFarmDefs::Get().Dishes())
		{
			if (WRBiome::IsLive(D.Biome) && Food && !Food->Known.Contains(D.Id) && !Food->Hinted.Contains(D.Id)) Unknown.Add(&D);
		}
		if (!Food || Unknown.Num() == 0) return TEXT("The scrap tells you nothing new");
		const FWRDishDef* D = Unknown[FMath::RandRange(0, Unknown.Num() - 1)];
		Food->Hinted.Add(D->Id);
		Inventory->RemoveItem(Item, 1);
		return FString::Printf(TEXT("A recipe scrap: %s, from %s at the %s"), *ItemName(D->Id), *FWRFarmDefs::Get().PatternText(*D, false), D->Station == TEXT("oven") ? TEXT("oven") : TEXT("cooking pot"));
	}
	return FString();
}

void FWRFarm::WildSeeds(UWRInventoryComponent* Inventory, FName Biome)
{
	const TArray<TPair<FName, float>>* List = FWRFarmDefs::Get().WildSeeds(Biome);
	if (!Inventory || !List) return;
	for (const TPair<FName, float>& S : *List)
	{
		const FWRCropDef* Crop = FWRFarmDefs::Get().CropForSeed(S.Key);
		if (Crop && Crop->IsLive() && FMath::FRand() < S.Value) Inventory->AddItem(S.Key, 1);
	}
}

bool FWRFarm::IsCookStation(FName StationType, const AActor* Player)
{
	if (StationType == TEXT("cooking_pot") || StationType == TEXT("oven")) return true;
	return StationType == TEXT("campfire") && UWRLifeSkillsComponent::StatFor(Player, TEXT("campChef")) > 0.f;
}

TArray<int32> FWRFarm::PickIngredients(const TArray<FWRItemStack>& Bag, const FWRDishDef& Dish)
{
	// Greedy per entry, most specific entries first, each slot used once (a slot with 2 can fill two entries).
	TArray<int32> Order;
	for (int32 i = 0; i < Dish.Pattern.Num(); ++i) Order.Add(i);
	Order.Sort([&Dish](int32 A, int32 B)
	{
		const auto Spec = [](const TArray<FName>& E) { for (const FName& F : E) if (FWRItemRegistry::Get().Find(F)) return 1; return 0; };
		return Spec(Dish.Pattern[A]) > Spec(Dish.Pattern[B]);
	});
	TMap<int32, int32> Used;
	TArray<int32> Out;
	Out.Init(INDEX_NONE, Dish.Pattern.Num());
	const double Now = FWRSpoil::ServerNow;
	for (const int32 E : Order)
	{
		int32 Best = INDEX_NONE;
		double BestLeft = TNumericLimits<double>::Max();
		for (int32 s = 0; s < Bag.Num(); ++s)
		{
			if (Bag[s].IsEmpty() || Used.FindRef(s) >= Bag[s].Count || !FWRFarmDefs::Get().Fills(Dish.Pattern[E], Bag[s].ItemId)) continue;
			// Golden crops only when asked for by hand (they're worth keeping).
			if (Bag[s].ItemId == TEXT("golden_crop")) continue;
			const double Left = FWRSpoil::SecondsLeft(Bag[s], Now);
			const double Key = Left < 0.0 ? 1.0e12 : Left;
			if (Key < BestLeft) { BestLeft = Key; Best = s; }
		}
		if (Best == INDEX_NONE) return TArray<int32>();
		Used.FindOrAdd(Best)++;
		Out[E] = Best;
	}
	return Out;
}

FString FWRFarm::Cook(AWRPlayerController* Player, AWRStation* Station, const TArray<int32>& BagSlots)
{
	AWRPlayerState* PS = Player ? Player->GetPlayerState<AWRPlayerState>() : nullptr;
	if (!PS || !Station) return TEXT("no cooking pot nearby");
	if (!IsCookStation(Station->StationType, Player)) return TEXT("cook at a cooking pot or an oven");
	if (BagSlots.Num() < 1 || BagSlots.Num() > 4) return TEXT("put 1 to 4 ingredients in");
	TArray<FWRItemStack>& Bag = PS->GetInventory()->GetSlotsMutable();
	// Each named slot is one ingredient (the same slot twice = two from it).
	TMap<int32, int32> Take;
	TArray<FName> Items;
	float Fresh = 0.f;
	float Worth = 1.f;
	for (const int32 S : BagSlots)
	{
		if (!Bag.IsValidIndex(S) || Bag[S].IsEmpty() || Take.FindRef(S) >= Bag[S].Count) return TEXT("that ingredient isn't in your bag any more");
		const FName Group = FWRFarmDefs::Get().GroupOf(Bag[S].ItemId);
		if (Group.IsNone() && Bag[S].ItemId != TEXT("golden_crop")) return FString::Printf(TEXT("%s isn't food"), *ItemName(Bag[S].ItemId));
		Take.FindOrAdd(S)++;
		Items.Add(Bag[S].ItemId);
		Fresh += FWRSpoil::FreshOf(Bag[S], FWRSpoil::ServerNow);
		if (Bag[S].ItemId == TEXT("golden_crop")) Worth = Tune(TEXT("food.goldenWorth"), 1.5f);
	}
	// The pot is the station's queue: the dish (or Grey mush) is cooked like an order, and lands in the bag or tray.
	const FWRDishDef* Dish = FWRFarmDefs::Get().Match(Station->StationType == TEXT("campfire") ? FName(TEXT("cooking_pot")) : Station->StationType, Items);
	if (!Dish && Station->StationType == TEXT("campfire")) Dish = FWRFarmDefs::Get().Match(TEXT("oven"), Items);   // Camp chef: anything at a fire
	FName Out = Dish ? Dish->Id : FName(TEXT("grey_mush"));
	// Banquet (cooking perk): a hunter's feast from this cook is a banquet of twelve portions.
	if (Out == TEXT("hunters_feast") && UWRLifeSkillsComponent::StatFor(PS, TEXT("banquet")) > 0.f && FWRFarmDefs::Get().Dish(TEXT("banquet_portion")))
	{
		Out = TEXT("banquet_portion");
	}
	// (Bulk cook doubles it as it's made: FWRCrafting::MakeOne.)
	const FString Queued = Station->CookOrder(Player, Out, Dish ? Dish->Count : 1, Dish ? Dish->Seconds : Tune(TEXT("food.mushSeconds"), 10.f), Worth);
	if (Queued.StartsWith(TEXT("!")))
	{
		return Queued.RightChop(1);   // the pot's queue is full: nothing taken
	}
	for (const TPair<int32, int32>& T : Take)
	{
		Bag[T.Key].Count -= T.Value;
		if (Bag[T.Key].Count <= 0) Bag[T.Key] = FWRItemStack();
	}
	WRSound::Broadcast(Player->GetPawn(), TEXT("cook.sizzle"));
	FString Said = Queued;
	if (Dish)
	{
		UWRFoodComponent* Food = UWRFoodComponent::For(PS);
		if (Food && Food->Discover(Dish->Id))
		{
			UWRLifeSkillsComponent::Award(PS, TEXT("cooking"), TEXT("discover"), 100.f);
			Said = FString::Printf(TEXT("New recipe: %s! %s"), *ItemName(Dish->Id), *Queued);
			UE_LOG(LogWildreach, Log, TEXT("COOK %s discovered %s"), *PS->GetPlayerName(), *Dish->Id.ToString());
		}
	}
	else
	{
		Said = FString::Printf(TEXT("That makes no dish: Grey mush. %s"), *Queued);
	}
	return Said;
}

FString FWRFarm::CookKnown(AWRPlayerController* Player, AWRStation* Station, FName DishId)
{
	AWRPlayerState* PS = Player ? Player->GetPlayerState<AWRPlayerState>() : nullptr;
	const FWRDishDef* Dish = FWRFarmDefs::Get().Dish(DishId);
	UWRFoodComponent* Food = UWRFoodComponent::For(PS);
	if (!PS || !Dish || !Food || !Food->Known.Contains(DishId)) return TEXT("you don't know that dish yet");
	const TArray<int32> Slots = PickIngredients(PS->GetInventory()->GetSlots(), *Dish);
	if (Slots.Num() == 0) return FString::Printf(TEXT("Not enough for %s: %s"), *ItemName(DishId), *FWRFarmDefs::Get().PatternText(*Dish, false));
	return Cook(Player, Station, Slots);
}

bool FWRFarm::LegendAllowed(const APlayerState* Angler, FName Fish)
{
	const UWRFoodComponent* Food = UWRFoodComponent::For(Angler);
	const int32 Today = FDateTime::UtcNow().GetDate().GetTicks() / ETimespan::TicksPerDay;
	return !Food || Food->LegendDay.FindRef(Fish) != Today;
}

void FWRFarm::LegendCaught(APlayerState* Angler, FName Fish, bool& bFirst)
{
	bFirst = false;
	UWRFoodComponent* Food = UWRFoodComponent::For(Angler);
	if (!Food) return;
	Food->LegendDay.Add(Fish, int32(FDateTime::UtcNow().GetDate().GetTicks() / ETimespan::TicksPerDay));
	bFirst = !Food->LegendsEver.Contains(Fish);
	Food->LegendsEver.Add(Fish);
}

// ---------------------------------------------------------------- the player's food

UWRFoodComponent::UWRFoodComponent()
{
	SetIsReplicatedByDefault(true);
}

void UWRFoodComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UWRFoodComponent, Known, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRFoodComponent, Hinted, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRFoodComponent, SatedUntil, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRFoodComponent, LastDiscovery, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRFoodComponent, DiscoveredAt, COND_OwnerOnly);
}

UWRFoodComponent* UWRFoodComponent::For(const AActor* PawnOrState)
{
	const AWRPlayerState* PS = PSOf(PawnOrState);
	return PS ? PS->FindComponentByClass<UWRFoodComponent>() : nullptr;
}

void UWRFoodComponent::Sate(const AActor* PawnOrState, float Minutes)
{
	UWRFoodComponent* Food = For(PawnOrState);
	if (!Food || Minutes <= 0.f || !Food->GetOwner()->HasAuthority()) return;
	const float Level = UWRLifeSkillsComponent::StatFor(PawnOrState, TEXT("satedTime"));
	const float Until = float(FWRSpoil::ServerNow) + Minutes * 60.f * (1.f + Level);
	Food->SatedUntil = FMath::Max(Food->SatedUntil, Until);
}

float UWRFoodComponent::SatedSecondsLeft() const
{
	return FMath::Max(0.f, SatedUntil - float(FWRSpoil::NowFor(this)));
}

float UWRFoodComponent::HungerMult() const
{
	return SatedUntil > float(FWRSpoil::ServerNow) ? 0.f : 1.f;
}

bool UWRFoodComponent::Discover(FName Dish)
{
	if (Known.Contains(Dish)) return false;
	Known.Add(Dish);
	Hinted.Remove(Dish);
	LastDiscovery = Dish;
	DiscoveredAt = float(FWRSpoil::ServerNow);
	return true;
}

TSharedRef<FJsonObject> UWRFoodComponent::ToJson() const
{
	TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
	const auto Names = [](const auto& List)
	{
		TArray<TSharedPtr<FJsonValue>> A;
		for (const FName& N : List) A.Add(MakeShared<FJsonValueString>(N.ToString()));
		return A;
	};
	O->SetArrayField(TEXT("known"), Names(Known));
	O->SetArrayField(TEXT("hinted"), Names(Hinted));
	O->SetArrayField(TEXT("cropsHarvested"), Names(CropsHarvested));
	O->SetArrayField(TEXT("legends"), Names(LegendsEver));
	TSharedRef<FJsonObject> Days = MakeShared<FJsonObject>();
	for (const TPair<FName, int32>& D : LegendDay) Days->SetNumberField(D.Key.ToString(), D.Value);
	O->SetObjectField(TEXT("legendDay"), Days);
	O->SetNumberField(TEXT("satedSeconds"), FMath::Max(0.f, SatedUntil - float(FWRSpoil::ServerNow)));
	return O;
}

void UWRFoodComponent::FromJson(const TSharedPtr<FJsonObject>& Json)
{
	if (!Json.IsValid()) return;
	const auto Read = [&Json](const TCHAR* Field, auto&& Add)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (Json->TryGetArrayField(Field, A)) for (const TSharedPtr<FJsonValue>& V : *A) Add(FName(*V->AsString()));
	};
	Read(TEXT("known"), [this](FName N) { if (FWRFarmDefs::Get().Dish(N)) Known.AddUnique(N); });
	Read(TEXT("hinted"), [this](FName N) { if (FWRFarmDefs::Get().Dish(N)) Hinted.AddUnique(N); });
	Read(TEXT("cropsHarvested"), [this](FName N) { CropsHarvested.Add(N); });
	Read(TEXT("legends"), [this](FName N) { LegendsEver.Add(N); });
	const TSharedPtr<FJsonObject>* Days = nullptr;
	if (Json->TryGetObjectField(TEXT("legendDay"), Days))
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& D : (*Days)->Values) LegendDay.Add(FName(*D.Key), int32(D.Value->AsNumber()));
	}
	double Sated = 0.0;
	if (Json->TryGetNumberField(TEXT("satedSeconds"), Sated)) SatedUntil = float(FWRSpoil::ServerNow + Sated);
}

// ---------------------------------------------------------------- the farm's clock

UWRFarmSubsystem* UWRFarmSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UWRFarmSubsystem>() : nullptr;
}

void UWRFarmSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (const UWorld* World = GetWorld(); World && World->IsGameWorld() && World->GetNetMode() != NM_Client)
	{
		FWRSpoil::ServerNow = World->GetTimeSeconds();
	}
}

TStatId UWRFarmSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWRFarmSubsystem, STATGROUP_Tickables);
}

void UWRFarmSubsystem::Register(AWRBuildPiece* Piece)
{
	if (Piece) Pieces.AddUnique(Piece);
}

void UWRFarmSubsystem::Unregister(AWRBuildPiece* Piece)
{
	Pieces.Remove(Piece);
	BreedClock.Remove(Piece);
}

void UWRFarmSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}
	FWRSpoil::DisplayNow = FWRSpoil::NowFor(World);
	if (World->GetNetMode() == NM_Client)
	{
		return;
	}
	FWRSpoil::ServerNow = World->GetTimeSeconds();
	Pieces.RemoveAll([](const TWeakObjectPtr<AWRBuildPiece>& P) { return !P.IsValid(); });
	if ((WaterClock -= DeltaTime) <= 0.f) { WaterClock = Tune(TEXT("farm.waterSeconds"), 5.f); TickWater(); }
	GrowClock += DeltaTime;
	if (GrowClock >= Tune(TEXT("farm.growSeconds"), 2.f)) { TickPlots(GrowClock); GrowClock = 0.f; }
	ProduceClock += DeltaTime;
	if (ProduceClock >= 1.f) { TickProducers(ProduceClock); ProduceClock = 0.f; }
	if ((SpoilClock -= DeltaTime) <= 0.f) { SpoilClock = Tune(TEXT("food.sweepSeconds"), 10.f); TickSpoil(); }
}

void UWRFarmSubsystem::TickPlots(float Dt)
{
	for (const TWeakObjectPtr<AWRBuildPiece>& W : Pieces)
	{
		AWRBuildPiece* Plot = W.Get();
		const FWRPieceDef* Def = Plot ? Plot->GetDef() : nullptr;
		if (!Def || Def->Kind != TEXT("crop") || Plot->CropId.IsNone()) continue;
		EWRFarmStop Stop = EWRFarmStop::None;
		uint8 Flags = Plot->FarmFlags;
		const float Rate = Plot->Growth >= 1.f ? 0.f : FWRFarm::GrowthRate(Plot, Stop, Flags);
		if (Plot->Growth >= 1.f) { Stop = EWRFarmStop::None; Flags = Plot->FarmFlags & ~WRFarmFlag::Stopped; }
		Plot->Growth = FMath::Min(1.f, Plot->Growth + Rate * Dt);
		if (Rate <= 0.f && Plot->Growth < 1.f) Flags |= WRFarmFlag::Stopped;
		const uint8 G8 = uint8(FMath::Clamp(FMath::FloorToInt(Plot->Growth * 255.f), 0, 255));
		if (G8 != Plot->Growth8 || Flags != Plot->FarmFlags || uint8(Stop) != Plot->FarmStop)
		{
			Plot->Growth8 = G8;
			Plot->FarmFlags = Flags;
			Plot->FarmStop = uint8(Stop);
			Plot->OnRep_Farm();   // the listen server's own view
		}
	}
}

void UWRFarmSubsystem::TickWater()
{
	// Wells are sources; channels carry water end to end from a source (the sea's edge, a well, a fed channel).
	const UWRWaterSubsystem* Water = UWRWaterSubsystem::Get(this);
	TArray<AWRBuildPiece*> Channels;
	TArray<FVector> Sources;
	for (const TWeakObjectPtr<AWRBuildPiece>& W : Pieces)
	{
		AWRBuildPiece* P = W.Get();
		const FWRPieceDef* D = P ? P->GetDef() : nullptr;
		if (!D) continue;
		if (D->Kind == TEXT("well")) { Sources.Add(P->GetActorLocation()); if (!(P->FarmFlags & WRFarmFlag::Watered)) { P->FarmFlags |= WRFarmFlag::Watered; P->OnRep_Farm(); } }
		else if (D->Kind == TEXT("channel")) Channels.Add(P);
	}
	// A running irrigation pump fills its trough: a channel joined at the trough's lip runs on from it.
	if (const UWRMachineSubsystem* Machines = UWRMachineSubsystem::Get(this)) Sources.Append(Machines->PumpOutlets());
	const float Half = Tune(TEXT("farm.channel.half"), 200.f);
	const float Join = Tune(TEXT("farm.channel.join"), 250.f);
	const auto Ends = [Half](const AWRBuildPiece* C) { const FVector F = C->GetActorForwardVector() * Half; return TPair<FVector, FVector>(C->GetActorLocation() + F, C->GetActorLocation() - F); };
	TSet<AWRBuildPiece*> Fed;
	TArray<AWRBuildPiece*> Frontier;
	for (AWRBuildPiece* C : Channels)
	{
		const TPair<FVector, FVector> E = Ends(C);
		bool bSource = false;
		for (const FVector& S : Sources) bSource |= FVector::Dist2D(S, E.Key) < Join || FVector::Dist2D(S, E.Value) < Join;
		if (Water) bSource |= Water->DepthAt(E.Key + C->GetActorForwardVector() * Join * 0.5f) > 0.f || Water->DepthAt(E.Value - C->GetActorForwardVector() * Join * 0.5f) > 0.f;
		if (bSource) { Fed.Add(C); Frontier.Add(C); }
	}
	while (Frontier.Num() > 0)
	{
		AWRBuildPiece* From = Frontier.Pop();
		const TPair<FVector, FVector> A = Ends(From);
		for (AWRBuildPiece* C : Channels)
		{
			if (Fed.Contains(C)) continue;
			const TPair<FVector, FVector> B = Ends(C);
			const float Gap = FMath::Min(FMath::Min(FVector::Dist(A.Key, B.Key), FVector::Dist(A.Key, B.Value)), FMath::Min(FVector::Dist(A.Value, B.Key), FVector::Dist(A.Value, B.Value)));
			// Water runs along and down a run, not up: the next one's floor no more than a step above.
			if (Gap < Join && C->GetActorLocation().Z <= From->GetActorLocation().Z + Tune(TEXT("farm.channel.maxRise"), 40.f)) { Fed.Add(C); Frontier.Add(C); }
		}
	}
	for (AWRBuildPiece* C : Channels)
	{
		const bool bFed = Fed.Contains(C);
		if (bFed != ((C->FarmFlags & WRFarmFlag::Watered) != 0))
		{
			C->FarmFlags = bFed ? (C->FarmFlags | WRFarmFlag::Watered) : (C->FarmFlags & ~WRFarmFlag::Watered);
			C->OnRep_Farm();
		}
	}
}

bool UWRFarmSubsystem::IsWateredAt(const FVector& Point, float Radius) const
{
	if (const UWRWaterSubsystem* Water = UWRWaterSubsystem::Get(this))
	{
		if (Water->DepthAt(Point) > 0.f) return true;
		for (int32 i = 0; i < 8; ++i)
		{
			const float A = i * PI / 4.f;
			if (Water->DepthAt(Point + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * Radius) > 0.f) return true;
		}
	}
	for (const TWeakObjectPtr<AWRBuildPiece>& W : Pieces)
	{
		const AWRBuildPiece* P = W.Get();
		const FWRPieceDef* D = P ? P->GetDef() : nullptr;
		if (D && (D->Kind == TEXT("well") || D->Kind == TEXT("channel")) && (P->FarmFlags & WRFarmFlag::Watered)
			&& FVector::DistSquared(P->GetActorLocation(), Point) < Radius * Radius)
		{
			return true;
		}
	}
	return false;
}

bool UWRFarmSubsystem::HasHiveNear(const FVector& Point, float Radius) const
{
	for (const TWeakObjectPtr<AWRBuildPiece>& W : Pieces)
	{
		const AWRBuildPiece* P = W.Get();
		const FWRPieceDef* D = P ? P->GetDef() : nullptr;
		if (D && D->Kind == TEXT("hive") && (P->FarmFlags & WRFarmFlag::Working) && FVector::DistSquared(P->GetActorLocation(), Point) < Radius * Radius) return true;
	}
	return false;
}

bool UWRFarmSubsystem::HasOtherCropNear(const AWRBuildPiece* Plot, float Radius) const
{
	for (const TWeakObjectPtr<AWRBuildPiece>& W : Pieces)
	{
		const AWRBuildPiece* P = W.Get();
		if (P && P != Plot && !P->CropId.IsNone() && P->CropId != Plot->CropId && FVector::DistSquared(P->GetActorLocation(), Plot->GetActorLocation()) < Radius * Radius) return true;
	}
	return false;
}

float UWRFarmSubsystem::ContainerMult(const AWRBuildPiece* Piece) const
{
	const FWRPieceDef* D = Piece ? Piece->GetDef() : nullptr;
	if (!D) return 1.f;
	const bool bTundra = WRBiome::At(this, Piece->GetActorLocation()) == TEXT("tundra");
	// A running cold store in the same room keeps everything in it near frozen (cooks/build-machinery.md 2.5).
	if (const UWRMachineSubsystem* Machines = UWRMachineSubsystem::Get(this); Machines && Machines->ColdStoreFor(Piece))
	{
		return FMath::Min(FWRMachineDefs::Get().Rule(TEXT("coldStoreMult"), 0.05f), D->Cold > 0.f ? D->Cold : 1.f);
	}
	if (D->Cold > 0.f)
	{
		// An ice box keeps cold while it has ice (Feed = ice burning now); in the tundra the ground does it.
		return (bTundra || Piece->Feed > 0) ? D->Cold : (bTundra ? Tune(TEXT("food.tundraChest"), 0.5f) : 1.f);
	}
	return bTundra ? Tune(TEXT("food.tundraChest"), 0.5f) : 1.f;
}

void UWRFarmSubsystem::TickProducers(float Dt)
{
	const UWorld* World = GetWorld();
	const bool bNight = AWRGameState::IsNightIn(World);
	for (const TWeakObjectPtr<AWRBuildPiece>& W : Pieces)
	{
		AWRBuildPiece* P = W.Get();
		const FWRPieceDef* D = P ? P->GetDef() : nullptr;
		if (!D) continue;
		const FName K = D->Kind;
		uint8 Flags = P->FarmFlags;
		EWRFarmStop Stop = EWRFarmStop::None;
		const auto Free = [P]() { for (const FWRItemStack& S : P->Storage) if (S.IsEmpty()) return true; return false; };
		const auto Put = [P](FName Item, int32 N) { return N - FWRInventoryRules::Add(P->Storage, Item, N); };
		if (K == TEXT("pen"))
		{
			TickPen(P, Dt);
			continue;
		}
		if (K == TEXT("hive"))
		{
			AWRCreature* Bee = HiveDweller(P);
			const bool bFlowers = Bee && FlowersNear(World, P->GetActorLocation(), Tune(TEXT("farm.hive.forageRange"), 4000.f));
			Flags = bFlowers ? (Flags | WRFarmFlag::Working) : (Flags & ~WRFarmFlag::Working);
			if (Bee && !bFlowers) Stop = EWRFarmStop::NoFlowers;
			if (bFlowers && Free())
			{
				const APlayerState* Keeper = nullptr;
				for (APlayerState* PS : World->GetGameState() ? World->GetGameState()->PlayerArray : TArray<TObjectPtr<APlayerState>>()) if (PS && PS->GetPlayerName() == P->OwnerName) Keeper = PS;
				const float More = 1.f + UWRLifeSkillsComponent::StatFor(Keeper, TEXT("hiveYield"));
				P->ProduceClockA += Dt;
				P->ProduceClockB += Dt;
				if (P->ProduceClockA >= Tune(TEXT("farm.hive.honeySeconds"), 900.f)) { P->ProduceClockA = 0.f; Put(TEXT("honey"), Whole(More)); }
				if (P->ProduceClockB >= Tune(TEXT("farm.hive.waxSeconds"), 1800.f)) { P->ProduceClockB = 0.f; Put(TEXT("hiveback_wax"), Whole(More)); }
			}
		}
		else if (K == TEXT("trap"))
		{
			const UWRWaterSubsystem* Water = UWRWaterSubsystem::Get(World);
			const float Depth = Water ? Water->DepthAt(P->GetActorLocation()) : 0.f;
			if (Depth < Tune(TEXT("farm.trap.minDepth"), 50.f)) Stop = EWRFarmStop::Dry;
			else if (!Free()) Stop = EWRFarmStop::Full;
			else if ((P->ProduceClockA += Dt) >= Tune(TEXT("farm.trap.seconds"), 1200.f))
			{
				P->ProduceClockA = 0.f;
				const FName WaterKind = Depth >= WRTune(TEXT("fishing.openDepth"), 250.f) ? FName(TEXT("open")) : FName(TEXT("shallow"));
				// Commons only: a trap never takes a rare or a legend.
				const FWRFishDef* Fish = nullptr;
				for (int32 Try = 0; Try < 8 && (!Fish || Fish->Rarity != TEXT("common")); ++Try)
				{
					Fish = FWRFishRegistry::Get().Pick(WRBiome::At(World, P->GetActorLocation()), WaterKind, bNight, 0.f, false, FMath::FRand());
				}
				if (Fish && Fish->Rarity == TEXT("common")) Put(Fish->Item, 1);
			}
		}
		else if (K == TEXT("saltpan"))
		{
			if (bNight) Stop = EWRFarmStop::NoSun;
			else if (!Free()) Stop = EWRFarmStop::Full;
			else if ((P->ProduceClockA += Dt) >= Tune(TEXT("farm.saltpan.seconds"), 600.f)) { P->ProduceClockA = 0.f; Put(TEXT("salt"), 1); }
			Flags = Stop == EWRFarmStop::None ? (Flags | WRFarmFlag::Working) : (Flags & ~WRFarmFlag::Working);
		}
		else if (K == TEXT("chest") && D->Cold > 0.f && !D->Fuel.IsNone())
		{
			// An ice box burns an ice block every fuelMinutes from its own slots (none needed in the tundra).
			if (WRBiome::At(World, P->GetActorLocation()) == TEXT("tundra")) { if (P->Feed != 1) { P->Feed = 1; } continue; }
			P->FuelClock -= Dt;
			if (P->FuelClock <= 0.f)
			{
				const bool bBurnt = FWRInventoryRules::Remove(P->Storage, D->Fuel, 1) > 0;
				P->FuelClock = bBurnt ? D->FuelMinutes * 60.f : 30.f;
				if (P->Feed != (bBurnt ? 1 : 0)) P->Feed = bBurnt ? 1 : 0;
			}
			continue;
		}
		else
		{
			continue;
		}
		Flags = Stop != EWRFarmStop::None ? (Flags | WRFarmFlag::Stopped) : (Flags & ~WRFarmFlag::Stopped);
		if (Flags != P->FarmFlags || uint8(Stop) != P->FarmStop)
		{
			P->FarmFlags = Flags;
			P->FarmStop = uint8(Stop);
			P->OnRep_Farm();
		}
	}
	// Young farm animals grow up (a pen birth starts at half size).
	for (auto It = AnimalClocks.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid()) It.RemoveCurrent();
	}
}

void UWRFarmSubsystem::TickPen(AWRBuildPiece* Pen, float Dt)
{
	UWorld* World = GetWorld();
	const TArray<AWRCreature*> Animals = PenAnimals(Pen);
	const APlayerState* Keeper = nullptr;
	for (APlayerState* PS : World->GetGameState() ? World->GetGameState()->PlayerArray : TArray<TObjectPtr<APlayerState>>()) if (PS && PS->GetPlayerName() == Pen->OwnerName) Keeper = PS;
	const int32 Capacity = FMath::RoundToInt(Tune(TEXT("farm.pen.capacity"), 4.f) + UWRLifeSkillsComponent::StatFor(Keeper, TEXT("penCapacity")));
	const float Pace = (1.f + UWRLifeSkillsComponent::StatFor(Keeper, TEXT("produceRate"))) * FMath::Max(0.1f, WRTune(TEXT("server.gatherMultiplier"), 1.f));
	EWRFarmStop Stop = EWRFarmStop::None;
	bool bShelfFull = true;
	for (const FWRItemStack& S : Pen->Storage) bShelfFull &= !S.IsEmpty();
	TMap<FName, int32> Herd;
	int32 Counted = 0;
	for (AWRCreature* A : Animals)
	{
		const FWRAnimalDef* Def = FWRFarmDefs::Get().Animal(A->CreatureId);
		if (!Def || ++Counted > Capacity) continue;   // a crowded pen: the extra ones don't produce
		Herd.FindOrAdd(A->CreatureId)++;
		TArray<float>& Clocks = AnimalClocks.FindOrAdd(A);
		Clocks.SetNumZeroed(Def->Products.Num() + 1);
		// Growing young: half size at birth, full in farm.pen.growMinutes.
		if (Clocks.Last() > 0.f)
		{
			Clocks.Last() += Dt;
			const float T = FMath::Clamp(Clocks.Last() / (Tune(TEXT("farm.pen.growMinutes"), 30.f) * 60.f), 0.f, 1.f);
			A->SetActorScale3D(FVector(FMath::Lerp(0.5f, 1.f, T)));
			if (T >= 1.f) Clocks.Last() = -1.f;   // grown (still a pen-born one: Selective breeding's bonus)
		}
		for (int32 i = 0; i < Def->Products.Num(); ++i)
		{
			const FWRAnimalDef::FProduct& Prod = Def->Products[i];
			Clocks[i] += Dt * Pace;
			if (Clocks[i] < Prod.Every) continue;
			if (bShelfFull) { Stop = EWRFarmStop::Full; Clocks[i] = Prod.Every; continue; }
			if (Pen->Feed <= 0) { Stop = EWRFarmStop::Hungry; Clocks[i] = Prod.Every; continue; }
			Clocks[i] = 0.f;
			// Content herd: half the time a cycle eats nothing.
			if (FMath::FRand() >= UWRLifeSkillsComponent::StatFor(Keeper, TEXT("contentHerd"))) Pen->Feed = FMath::Max(0, Pen->Feed - 1);
			float N = 1.f;
			if (Prod.bGentle) N += UWRLifeSkillsComponent::StatFor(Keeper, TEXT("produceExtra"));
			if (Prod.bShear) N += UWRLifeSkillsComponent::StatFor(Keeper, TEXT("shearer"));
			if (Clocks.Last() != 0.f) N += UWRLifeSkillsComponent::StatFor(Keeper, TEXT("selectiveBreeding"));
			FWRInventoryRules::Add(Pen->Storage, Prod.Item, Whole(N));
			if (!Def->Prize.IsNone() && FMath::FRand() < UWRLifeSkillsComponent::StatFor(Keeper, TEXT("prizeStock"))) FWRInventoryRules::Add(Pen->Storage, Def->Prize, 1);
			Pen->OnRep_Farm();
		}
	}
	// Breeding: two of a kind with feed to spare and room in the pen make a young every farm.pen.breedMinutes.
	float& Breed = BreedClock.FindOrAdd(Pen);
	FName Pair;
	for (const TPair<FName, int32>& H : Herd) if (H.Value >= 2) Pair = H.Key;
	if (!Pair.IsNone() && Counted < Capacity && Pen->Feed >= FMath::RoundToInt(Tune(TEXT("farm.pen.breedFeed"), 5.f)) && Keeper
		&& AWRCreature::CountTames(World, Keeper->GetPlayerName()) < AWRCreature::TameLimitFor(Keeper))
	{
		Breed += Dt * Pace;
		if (Breed >= Tune(TEXT("farm.pen.breedMinutes"), 60.f) * 60.f)
		{
			Breed = 0.f;
			const FVector At = Pen->GetActorTransform().TransformPosition(FVector(FMath::FRandRange(-200.f, 200.f), FMath::FRandRange(-200.f, 200.f), 60.f));
			if (AWRCreature* Young = AWRCreature::SpawnCreature(World, Pair, At, FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f)))
			{
				Young->CompleteTame(Keeper->GetPlayerName());
				Young->TameOrder = EWRTameOrder::Stay;
				Young->SetActorScale3D(FVector(0.5f));
				TArray<float>& C = AnimalClocks.FindOrAdd(Young);
				const FWRAnimalDef* Def = FWRFarmDefs::Get().Animal(Pair);
				C.SetNumZeroed((Def ? Def->Products.Num() : 0) + 1);
				C.Last() = 0.01f;
				Pen->Feed = FMath::Max(0, Pen->Feed - FMath::RoundToInt(Tune(TEXT("farm.pen.breedFeed"), 5.f)));
				UWRLifeSkillsComponent::Award(Keeper, TEXT("husbandry"), TEXT("birth"), 10.f);
				WRFx::Broadcast(Pen, TEXT("discover"), At, FRotator::ZeroRotator, 0.6f, TEXT("animal.collect"));
				if (AWRPlayerController* PC = Cast<AWRPlayerController>(Keeper->GetOwner())) PC->ClientNotify(FString::Printf(TEXT("A young %s was born in your pen"), *Pair.ToString()));
				Pen->OnRep_Farm();
			}
		}
	}
	else
	{
		Breed = 0.f;
	}
	uint8 Flags = Pen->FarmFlags;
	Flags = Stop != EWRFarmStop::None ? (Flags | WRFarmFlag::Stopped) : (Flags & ~WRFarmFlag::Stopped);
	Flags = Counted > 0 ? (Flags | WRFarmFlag::Working) : (Flags & ~WRFarmFlag::Working);
	if (Flags != Pen->FarmFlags || uint8(Stop) != Pen->FarmStop)
	{
		Pen->FarmFlags = Flags;
		Pen->FarmStop = uint8(Stop);
		Pen->OnRep_Farm();
	}
}

void UWRFarmSubsystem::TickSpoil()
{
	// Every bag (x1; Preserver x0.7), chest (x1; x0.5 in the tundra; an ice box with ice x0.2), station tray and
	// dropped bag. Only stacks whose pace changed are re-stamped; spoiled ones turn into Spoiled food.
	UWorld* World = GetWorld();
	const double Now = FWRSpoil::ServerNow;
	for (APlayerState* PS : World->GetGameState() ? World->GetGameState()->PlayerArray : TArray<TObjectPtr<APlayerState>>())
	{
		const AWRPlayerState* WPS = Cast<AWRPlayerState>(PS);
		if (!WPS || !WPS->GetInventory()) continue;
		const float Mult = 1.f + UWRLifeSkillsComponent::StatFor(WPS, TEXT("spoilRate"));
		FWRSpoil::Sweep(WPS->GetInventory()->GetSlotsMutable(), Now, FMath::Max(0.05f, Mult));
	}
	for (const TWeakObjectPtr<AWRBuildPiece>& W : Pieces)
	{
		AWRBuildPiece* P = W.Get();
		if (P && P->Storage.Num() > 0) FWRSpoil::Sweep(P->Storage, Now, ContainerMult(P));
	}
	for (TActorIterator<AWRStation> It(World); It; ++It)
	{
		FWRSpoil::Sweep(It->Output, Now, 1.f);
	}
	for (TActorIterator<AWRLootBag> It(World); It; ++It)
	{
		FWRSpoil::Sweep(It->Items, Now, 1.f);
	}
}
