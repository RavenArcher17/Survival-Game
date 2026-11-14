// Biome armour sets, rings, salvage, removing extras and blueprints (cooks/build-crafting.md 2.5-2.9; numbers in
// Data/gear.json). The slot words and labels for the ten worn places live here too.
#include "WRGear.h"
#include "WRCrafting.h"
#include "WRInventory.h"
#include "WRLifeSkills.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRSound.h"
#include "WRTuning.h"
#include "Wildreach.h"
#include "Dom/JsonObject.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

#define LOCTEXT_NAMESPACE "WRGear"

// ---------------------------------------------------------------- slot words

namespace WRGearSlots
{
	const TCHAR* Word(EWREquipSlot S)
	{
		switch (S)
		{
		case EWREquipSlot::Weapon: return TEXT("weapon");
		case EWREquipSlot::Armor: return TEXT("armor");
		case EWREquipSlot::Ranged: return TEXT("ranged");
		case EWREquipSlot::Light: return TEXT("light");
		case EWREquipSlot::Head: return TEXT("head");
		case EWREquipSlot::Hands: return TEXT("hands");
		case EWREquipSlot::Legs: return TEXT("legs");
		case EWREquipSlot::Talisman: return TEXT("talisman");
		case EWREquipSlot::Ring: return TEXT("ring");
		case EWREquipSlot::Ring2: return TEXT("ring2");
		default: return TEXT("none");
		}
	}

	EWREquipSlot FromWord(const FString& W)
	{
		if (W == TEXT("armor") || W == TEXT("chest")) return EWREquipSlot::Armor;
		if (W == TEXT("ranged")) return EWREquipSlot::Ranged;
		if (W == TEXT("light")) return EWREquipSlot::Light;
		if (W == TEXT("head")) return EWREquipSlot::Head;
		if (W == TEXT("hands")) return EWREquipSlot::Hands;
		if (W == TEXT("legs")) return EWREquipSlot::Legs;
		if (W == TEXT("talisman")) return EWREquipSlot::Talisman;
		if (W == TEXT("ring")) return EWREquipSlot::Ring;
		if (W == TEXT("ring2")) return EWREquipSlot::Ring2;
		return EWREquipSlot::Weapon;
	}

	FText Label(EWREquipSlot S)
	{
		switch (S)
		{
		case EWREquipSlot::Weapon: return LOCTEXT("SWeapon", "Weapon");
		case EWREquipSlot::Armor: return LOCTEXT("SChest", "Chest");
		case EWREquipSlot::Ranged: return LOCTEXT("SRanged", "Ranged");
		case EWREquipSlot::Light: return LOCTEXT("SLight", "Light");
		case EWREquipSlot::Head: return LOCTEXT("SHead", "Head");
		case EWREquipSlot::Hands: return LOCTEXT("SHands", "Hands");
		case EWREquipSlot::Legs: return LOCTEXT("SLegs", "Legs");
		case EWREquipSlot::Talisman: return LOCTEXT("STalisman", "Talisman");
		case EWREquipSlot::Ring: return LOCTEXT("SRing", "Ring");
		case EWREquipSlot::Ring2: return LOCTEXT("SRing2", "Ring");
		default: return FText::GetEmpty();
		}
	}
}

// ---------------------------------------------------------------- the data

const FWRGear& FWRGear::Get()
{
	static FWRGear Instance;
	return Instance;
}

FWRGear::FWRGear()
{
	PortalOdds = { 0.1f, 0.2f, 0.3f, 0.45f, 0.6f };
	const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("gear.json"));
	if (!Root.IsValid())
	{
		UE_LOG(LogWildreach, Warning, TEXT("gear.json missing: no armour sets, rings or blueprints"));
		return;
	}
	const auto Num = [](const TSharedPtr<FJsonObject>& O, const TCHAR* F, float Fallback)
	{
		double V = 0.0;
		return O.IsValid() && O->TryGetNumberField(F, V) ? float(V) : Fallback;
	};
	const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
	if (Root->TryGetArrayField(TEXT("sets"), List))
	{
		for (const TSharedPtr<FJsonValue>& V : *List)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			if (!O.IsValid()) continue;
			FWRSetDef S;
			S.Id = FName(*O->GetStringField(TEXT("id")));
			S.Name = O->GetStringField(TEXT("name"));
			FString Str;
			if (O->TryGetStringField(TEXT("biome"), Str)) S.Biome = FName(*Str);
			if (O->TryGetStringField(TEXT("load"), Str)) S.Load = FName(*Str);
			const TSharedPtr<FJsonObject>* Bonus = nullptr;
			if (O->TryGetObjectField(TEXT("bonus"), Bonus))
			{
				for (const auto& B : (*Bonus)->Values)
				{
					const TSharedPtr<FJsonObject> BO = B.Value->AsObject();
					if (!BO.IsValid()) continue;
					FWRSetBonus SB;
					SB.Pieces = FMath::Max(1, FCString::Atoi(*B.Key));
					BO->TryGetStringField(TEXT("text"), SB.Text);
					const TSharedPtr<FJsonObject>* Stats = nullptr;
					if (BO->TryGetObjectField(TEXT("stats"), Stats))
					{
						for (const auto& St : (*Stats)->Values) SB.Stats.Add(FName(*St.Key), float(St.Value->AsNumber()));
					}
					S.Bonuses.Add(SB);
				}
				S.Bonuses.Sort([](const FWRSetBonus& A, const FWRSetBonus& B) { return A.Pieces < B.Pieces; });
			}
			Sets.Add(S);
		}
	}
	const TSharedPtr<FJsonObject>* Rings = nullptr;
	if (Root->TryGetObjectField(TEXT("rings"), Rings))
	{
		RingShare = Num(*Rings, TEXT("share"), RingShare);
		RingPerTier = Num(*Rings, TEXT("perTier"), RingPerTier);
		const TSharedPtr<FJsonObject>* Tiers = nullptr;
		if ((*Rings)->TryGetObjectField(TEXT("metalTier"), Tiers))
		{
			for (const auto& T : (*Tiers)->Values) MetalTier.Add(FName(*T.Key), int32(T.Value->AsNumber()));
		}
	}
	const TSharedPtr<FJsonObject>* Salvage = nullptr;
	if (Root->TryGetObjectField(TEXT("salvage"), Salvage))
	{
		SalvageBase = Num(*Salvage, TEXT("base"), SalvageBase);
		SalvagePerQuality = Num(*Salvage, TEXT("perQuality"), SalvagePerQuality);
		SalvageShards = Num(*Salvage, TEXT("shards"), SalvageShards);
		CoreShards = FMath::Max(1, int32(Num(*Salvage, TEXT("coreShards"), float(CoreShards))));
		CoreLevel = FMath::Max(1, int32(Num(*Salvage, TEXT("coreLevel"), float(CoreLevel))));
		SalvageXP = Num(*Salvage, TEXT("xp"), SalvageXP);
	}
	const TSharedPtr<FJsonObject>* Bps = nullptr;
	if (Root->TryGetObjectField(TEXT("blueprints"), Bps))
	{
		RepeatChance = Num(*Bps, TEXT("repeatChance"), RepeatChance);
		LegendaryFromRank = int32(Num(*Bps, TEXT("legendaryFromRank"), float(LegendaryFromRank)));
		const TArray<TSharedPtr<FJsonValue>>* Odds = nullptr;
		if ((*Bps)->TryGetArrayField(TEXT("portalOdds"), Odds))
		{
			PortalOdds.Reset();
			for (const TSharedPtr<FJsonValue>& O : *Odds) PortalOdds.Add(float(O->AsNumber()));
		}
		if ((*Bps)->TryGetArrayField(TEXT("list"), List))
		{
			for (const TSharedPtr<FJsonValue>& V : *List)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				if (!O.IsValid()) continue;
				FWRBlueprintDef B;
				B.Id = FName(*O->GetStringField(TEXT("id")));
				FString Str;
				if (O->TryGetStringField(TEXT("boss"), Str)) B.Boss = FName(*Str);
				O->TryGetStringField(TEXT("kind"), B.Kind);
				const TArray<TSharedPtr<FJsonValue>>* Recipes = nullptr;
				if (O->TryGetArrayField(TEXT("recipes"), Recipes))
				{
					for (const TSharedPtr<FJsonValue>& R : *Recipes) B.Recipes.Add(FName(*R->AsString()));
				}
				Blueprints.Add(B);
			}
		}
	}
	UE_LOG(LogWildreach, Log, TEXT("Gear: %d armour sets, %d blueprints"), Sets.Num(), Blueprints.Num());
}

// ---------------------------------------------------------------- salvage

TArray<TPair<FName, int32>> FWRGear::SalvageReturns(const FWRItemStack& Item, TArray<FString>* OutLost)
{
	TArray<TPair<FName, int32>> Out;
	const FWRGear& G = Get();
	const FWRItemDef* Def = Item.IsEmpty() ? nullptr : FWRItemRegistry::Get().Find(Item.ItemId);
	if (!Def)
	{
		return Out;
	}
	if (const FWRRecipe* R = FWRRecipeRegistry::Get().ForOutput(Def->Id))
	{
		const float Share = G.SalvageBase + G.SalvagePerQuality * float(static_cast<int32>(Item.Quality));
		// The main input: the most of anything that isn't a path essence (a hide vest's hide, a ring's band).
		FName Main;
		int32 MainCount = 0;
		for (const TPair<FName, int32>& In : R->Inputs)
		{
			if (In.Key.ToString().EndsWith(TEXT("_essence"))) continue;
			if (In.Value > MainCount) { Main = In.Key; MainCount = In.Value; }
		}
		for (const TPair<FName, int32>& In : R->Inputs)
		{
			const FString Id = In.Key.ToString();
			const bool bMetal = Id.EndsWith(TEXT("_ingot")) || Id.EndsWith(TEXT("_plate")) || Id == TEXT("star_iron");
			if (!bMetal && In.Key != Main)
			{
				continue;
			}
			Out.Add({ In.Key, FMath::Max(1, FMath::FloorToInt(In.Value * Share)) });
		}
	}
	// A quarter of the upgrade shards that went into it (+1 cost 1, +2 cost 2...).
	const int32 Spent = Item.Upgrade * (Item.Upgrade + 1) / 2;
	const int32 Shards = FMath::FloorToInt(Spent * G.SalvageShards);
	if (Shards > 0)
	{
		Out.Add({ TEXT("upgrade_shard"), Shards });
	}
	if (OutLost)
	{
		if (!Item.Element.IsNone() && Def->InnateElement.IsNone())
		{
			OutLost->Add(FString::Printf(TEXT("its %s infusion"), *Item.Element.ToString()));
		}
		for (const FName& Gem : Item.Sockets)
		{
			const FWRItemDef* GD = FWRItemRegistry::Get().Find(Gem);
			OutLost->Add(GD ? GD->Name : Gem.ToString());
		}
	}
	return Out;
}

FString AWRStation::Salvage(AWRPlayerController* Player, int32 BagSlot)
{
	FString Why;
	if (!HasAuthority() || !CanUse(Player, Why))
	{
		return Why;
	}
	AWRPlayerState* PS = Player->GetPlayerState<AWRPlayerState>();
	UWRInventoryComponent* Inv = PS ? PS->GetInventory() : nullptr;
	if (!Inv || !Inv->GetSlots().IsValidIndex(BagSlot) || Inv->GetSlots()[BagSlot].IsEmpty())
	{
		return TEXT("nothing there to salvage");
	}
	FWRItemStack& Stack = Inv->GetSlotsMutable()[BagSlot];
	const FWRItemDef* Def = FWRItemRegistry::Get().Find(Stack.ItemId);
	if (!Def)
	{
		return TEXT("unknown item");
	}
	const FWRGear& G = FWRGear::Get();
	// A spare major boss core: ten core shards for starsteel (one in a Hearth is on the Hearth, never in a bag).
	if (Def->Category == TEXT("core"))
	{
		if (StationType != TEXT("forge"))
		{
			return TEXT("a core is broken at a forge");
		}
		if (GetTier() < G.CoreLevel)
		{
			return FString::Printf(TEXT("breaking a core needs a tier %d forge"), G.CoreLevel);
		}
		const FString Name = Def->Name;
		if (--Stack.Count <= 0) Stack = FWRItemStack();
		const int32 Left = Inv->AddItem(TEXT("core_shard"), G.CoreShards);
		if (Left > 0) FWRInventoryRules::Add(Output, TEXT("core_shard"), Left);
		WRSound::Broadcast(Player->GetPawn(), TEXT("salvage"));
		UWRLifeSkillsComponent::Award(Player, TEXT("smithing"), TEXT("salvage"), G.SalvageXP);
		UE_LOG(LogWildreach, Log, TEXT("SALVAGE %s broke %s into %d core shards"), *GetNameSafe(Player), *Name, G.CoreShards);
		return FString::Printf(TEXT("Salvaged %s: %d core shards%s"), *Name, G.CoreShards, Left > 0 ? TEXT(" (some in the tray)") : TEXT(""));
	}
	if (!Def->IsEquipment() || Def->StackSize > 1 || Def->Slot == EWREquipSlot::Light)
	{
		return FString::Printf(TEXT("the %s can't be broken down"), *Def->Name);
	}
	const FName At = FWRCrafting::GearStation(*Def);
	if (At != StationType)
	{
		return FString::Printf(TEXT("the %s is salvaged at a %s"), *Def->Name, *At.ToString().Replace(TEXT("_"), TEXT(" ")));
	}
	TArray<FString> Lost;
	const TArray<TPair<FName, int32>> Back = FWRGear::SalvageReturns(Stack, &Lost);
	const FString Name = Def->Name;
	const bool bArmour = FWRCrafting::IsArmourMade(*Def);
	Stack = FWRItemStack();
	TArray<FString> Got;
	bool bTray = false;
	for (const TPair<FName, int32>& B : Back)
	{
		const int32 Left = Inv->AddItem(B.Key, B.Value);
		if (Left > 0)
		{
			FWRInventoryRules::Add(Output, B.Key, Left);   // what the bag can't hold waits in the station's tray
			bTray = true;
		}
		const FWRItemDef* BD = FWRItemRegistry::Get().Find(B.Key);
		Got.Add(FString::Printf(TEXT("%d %s"), B.Value, BD ? *BD->Name : *B.Key.ToString()));
	}
	WRSound::Broadcast(Player->GetPawn(), TEXT("salvage"));
	UWRLifeSkillsComponent::Award(Player, bArmour ? FName(TEXT("armorcraft")) : FName(TEXT("smithing")), TEXT("salvage"), G.SalvageXP);
	UE_LOG(LogWildreach, Log, TEXT("SALVAGE %s: %s -> %s (lost: %s)"), *GetNameSafe(Player), *Name, *FString::Join(Got, TEXT(", ")), *FString::Join(Lost, TEXT(", ")));
	return FString::Printf(TEXT("Salvaged %s: %s%s"), *Name, Got.Num() > 0 ? *FString::Join(Got, TEXT(", ")) : TEXT("nothing came back"), bTray ? TEXT(" (some in the tray)") : TEXT(""));
}

FString AWRStation::RemoveExtra(AWRPlayerController* Player, int32 Index)
{
	FString Why;
	if (!HasAuthority() || !CanUse(Player, Why))
	{
		return Why;
	}
	if (StationType != TEXT("forge"))
	{
		return TEXT("extras are taken out at a forge");
	}
	UWRInventoryComponent* Inv = Player->GetPlayerState<AWRPlayerState>()->GetInventory();
	FWRItemStack* Weapon = Inv->GetEquippedMutable(EWREquipSlot::Weapon);
	if (!Weapon || Weapon->IsEmpty() || !Weapon->Sockets.IsValidIndex(Index))
	{
		return TEXT("nothing in that socket");
	}
	const FName Gem = Weapon->Sockets[Index];
	Weapon->Sockets.RemoveAt(Index);
	const FWRItemDef* GD = FWRItemRegistry::Get().Find(Gem);
	const FString Name = GD ? GD->Name : Gem.ToString();
	// Careful hands (smithing perk): half the time it comes out whole.
	const bool bSaved = FMath::FRand() < UWRLifeSkillsComponent::StatFor(Player, TEXT("extraSave"));
	if (bSaved && Inv->AddItem(Gem, 1) > 0)
	{
		FWRInventoryRules::Add(Output, Gem, 1);
	}
	Inv->ApplyEquipment();
	WRSound::Broadcast(Player->GetPawn(), TEXT("socket.remove"));
	UE_LOG(LogWildreach, Log, TEXT("EXTRA %s took %s out (%s)"), *GetNameSafe(Player), *Gem.ToString(), bSaved ? TEXT("saved") : TEXT("destroyed"));
	return bSaved ? FString::Printf(TEXT("Your careful hands saved the %s"), *Name) : FString::Printf(TEXT("Removed the %s: it shattered"), *Name);
}

// ---------------------------------------------------------------- blueprints

namespace WRGearImpl
{
	const AWRPlayerState* PlayerStateOf(const AActor* Who)
	{
		if (const AWRPlayerState* PS = Cast<AWRPlayerState>(Who)) return PS;
		if (const APawn* Pawn = Cast<APawn>(Who)) return Pawn->GetPlayerState<AWRPlayerState>();
		if (const AController* C = Cast<AController>(Who)) return C->GetPlayerState<AWRPlayerState>();
		return nullptr;
	}
}

bool FWRBlueprints::Knows(const AActor* Who, FName Blueprint)
{
	const AWRPlayerState* PS = WRGearImpl::PlayerStateOf(Who);
	return PS && PS->KnownBlueprints.Contains(Blueprint);
}

FString FWRBlueprints::Learn(UWRInventoryComponent* Inv, int32 SlotIndex)
{
	AWRPlayerState* PS = Inv ? Cast<AWRPlayerState>(Inv->GetOwner()) : nullptr;
	if (!PS || !PS->HasAuthority() || !Inv->GetSlots().IsValidIndex(SlotIndex))
	{
		return FString();
	}
	FWRItemStack& Stack = Inv->GetSlotsMutable()[SlotIndex];
	const FWRItemDef* Def = Stack.IsEmpty() ? nullptr : FWRItemRegistry::Get().Find(Stack.ItemId);
	const FWRBlueprintDef* Bp = Def ? FWRGear::Get().FindBlueprint(Def->Id) : nullptr;
	if (!Bp)
	{
		return TEXT("nothing to learn from it");
	}
	// Its recipe's name: what the survivor learns to make.
	FString What = Def->Name;
	if (Bp->Recipes.Num() > 0)
	{
		const FWRRecipe* R = FWRRecipeRegistry::Get().Find(Bp->Recipes[0]);
		const FWRItemDef* Out = R ? FWRItemRegistry::Get().Find(R->Output) : nullptr;
		if (Out) What = Out->Name;
	}
	if (PS->KnownBlueprints.Contains(Bp->Id))
	{
		return FString::Printf(TEXT("You already know %s: hand the blueprint to a clan mate"), *What);
	}
	PS->KnownBlueprints.Add(Bp->Id);
	if (--Stack.Count <= 0) Stack = FWRItemStack();
	WRSound::Broadcast(PS->GetPawn(), TEXT("blueprint.learn"));
	UE_LOG(LogWildreach, Log, TEXT("BLUEPRINT %s learned %s"), *PS->GetPlayerName(), *Bp->Id.ToString());
	return FString::Printf(TEXT("You learned %s: it's in your Crafting tab under Blueprints"), *What);
}

FName FWRBlueprints::BossDrop(const AWRPlayerState* PS, FName BossId, bool bFirstKill)
{
	const FWRGear& G = FWRGear::Get();
	TArray<FName> Mine, Fresh;
	for (const FWRBlueprintDef& B : G.AllBlueprints())
	{
		if (B.Boss != BossId) continue;
		Mine.Add(B.Id);
		const UWRInventoryComponent* Inv = PS ? PS->GetInventory() : nullptr;
		if (PS && !PS->KnownBlueprints.Contains(B.Id) && !(Inv && Inv->CountOf(B.Id) > 0)) Fresh.Add(B.Id);
	}
	if (Mine.Num() == 0 || (!bFirstKill && FMath::FRand() >= G.RepeatChance))
	{
		return NAME_None;
	}
	const TArray<FName>& Pool = Fresh.Num() > 0 ? Fresh : Mine;
	return Pool[FMath::RandRange(0, Pool.Num() - 1)];
}

FName FWRBlueprints::PortalDrop(const AWRPlayerState* PS, int32 Rank)
{
	const FWRGear& G = FWRGear::Get();
	if (G.PortalOdds.Num() == 0 || FMath::FRand() >= G.PortalOdds[FMath::Clamp(Rank, 1, G.PortalOdds.Num()) - 1])
	{
		return NAME_None;
	}
	TArray<FName> Pool;
	for (const FWRBlueprintDef& B : G.AllBlueprints())
	{
		const bool bLegendary = B.Kind == TEXT("legendary");
		if (bLegendary && Rank < G.LegendaryFromRank) continue;
		if (PS && PS->KnownBlueprints.Contains(B.Id)) continue;
		Pool.Add(B.Id);
	}
	return Pool.Num() > 0 ? Pool[FMath::RandRange(0, Pool.Num() - 1)] : NAME_None;
}

#undef LOCTEXT_NAMESPACE
