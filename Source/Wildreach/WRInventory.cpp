#include "WRInventory.h"
#include "WRSurvival.h"
#include "WRGear.h"
#include "WRFarm.h"
#include "WRFx.h"
#include "WRSound.h"
#include "WRCreature.h"
#include "WRLifeSkills.h"
#include "WRCombatLibrary.h"
#include "WRWeapons.h"
#include "WRPathComponent.h"
#include "WRResourceNode.h"
#include "Wildreach.h"
#include "WRAttributeSet.h"
#include "WRCharacterBase.h"
#include "WRCharacter.h"
#include "WRGameplayTags.h"
#include "WRStatusComponent.h"
#include "WRTuning.h"
#include "WRSocial.h"
#include "WRPlayerController.h"
#include "AbilitySystemComponent.h"
#include "Dom/JsonObject.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

// ---------------------------------------------------------------- registry

FWRItemRegistry& FWRItemRegistry::Get()
{
	static FWRItemRegistry Instance;
	return Instance;
}

FWRItemRegistry::FWRItemRegistry()
{
	const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("items.json"));
	const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
	if (Root.IsValid() && Root->TryGetArrayField(TEXT("items"), Items))
	{
		for (const TSharedPtr<FJsonValue>& V : *Items)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			if (!O.IsValid() || !O->HasField(TEXT("id")))
			{
				continue;  // comment entries ("_ranged": ...)
			}
			FWRItemDef D;
			D.Id = FName(*O->GetStringField(TEXT("id")));
			D.Name = O->GetStringField(TEXT("name"));
			D.Category = FName(*O->GetStringField(TEXT("category")));
			D.StackSize = FMath::Max(1, static_cast<int32>(O->GetNumberField(TEXT("stack"))));
			O->TryGetNumberField(TEXT("weight"), D.Weight);
			FString ClassId;
			if (O->TryGetStringField(TEXT("class"), ClassId)) D.WeaponClass = FName(*ClassId);
			O->TryGetNumberField(TEXT("keeps"), D.KeepsMinutes);
			O->TryGetStringField(TEXT("note"), D.Note);
			const TSharedPtr<FJsonObject>* Use = nullptr;
			if (O->TryGetObjectField(TEXT("use"), Use))
			{
				(*Use)->TryGetNumberField(TEXT("heal"), D.Heal);
				(*Use)->TryGetNumberField(TEXT("hunger"), D.Hunger);
				(*Use)->TryGetNumberField(TEXT("thirst"), D.Thirst);
				(*Use)->TryGetNumberField(TEXT("cooldown"), D.UseCooldown);
				(*Use)->TryGetNumberField(TEXT("sated"), D.SatedMinutes);
				double Cleanse = 0.0;
				D.bCleanse = (*Use)->TryGetNumberField(TEXT("cleanse"), Cleanse) && Cleanse > 0.0;
				// Status cures (cooks/build-combat-full.md 2.12): the statuses this drink ends.
				const TArray<TSharedPtr<FJsonValue>>* Cures = nullptr;
				if ((*Use)->TryGetArrayField(TEXT("cures"), Cures))
				{
					for (const TSharedPtr<FJsonValue>& C : *Cures) D.Cures.Add(FName(*C->AsString()));
				}
			}
			const TSharedPtr<FJsonObject>* Equip = nullptr;
			if (O->TryGetObjectField(TEXT("equip"), Equip))
			{
				const FString Slot = (*Equip)->GetStringField(TEXT("slot"));
				D.Slot = Slot == TEXT("weapon") ? EWREquipSlot::Weapon : Slot == TEXT("armor") || Slot == TEXT("chest") ? EWREquipSlot::Armor
					: Slot == TEXT("ranged") ? EWREquipSlot::Ranged : Slot == TEXT("light") ? EWREquipSlot::Light
					: Slot == TEXT("head") ? EWREquipSlot::Head : Slot == TEXT("hands") ? EWREquipSlot::Hands : Slot == TEXT("legs") ? EWREquipSlot::Legs
					: Slot == TEXT("talisman") ? EWREquipSlot::Talisman : Slot == TEXT("ring") ? EWREquipSlot::Ring : EWREquipSlot::None;
				// Gear (cooks/build-crafting.md): the outfit it shows, its biome set, a boss weapon's own element and effects.
				FString GearStr;
				if ((*Equip)->TryGetStringField(TEXT("look"), GearStr)) D.LookAs = FName(*GearStr);
				else if (WRGearSlots::IsArmour(D.Slot)) D.LookAs = D.Id;
				if ((*Equip)->TryGetStringField(TEXT("set"), GearStr)) D.Set = FName(*GearStr);
				if ((*Equip)->TryGetStringField(TEXT("element"), GearStr)) D.InnateElement = FName(*GearStr);
				const TSharedPtr<FJsonObject>* Innate = nullptr;
				if ((*Equip)->TryGetObjectField(TEXT("innate"), Innate))
				{
					for (const auto& In : (*Innate)->Values) D.Innate.Add(FName(*In.Key), float(In.Value->AsNumber()));
				}
				FString Metal;
				if ((*Equip)->TryGetStringField(TEXT("metal"), Metal)) D.Metal = FName(*Metal);
				(*Equip)->TryGetNumberField(TEXT("armor"), D.Armor);
				(*Equip)->TryGetNumberField(TEXT("poise"), D.Poise);
				// Status resistance worn (raises the wearer's buildup threshold per status).
				const TSharedPtr<FJsonObject>* Resist = nullptr;
				if ((*Equip)->TryGetObjectField(TEXT("resist"), Resist))
				{
					for (const TPair<FString, TSharedPtr<FJsonValue>>& R : (*Resist)->Values) D.Resists.Add(FName(*R.Key), float(R.Value->AsNumber()));
				}
				(*Equip)->TryGetNumberField(TEXT("durability"), D.MaxDurability);
			}
			const TSharedPtr<FJsonObject>* Ranged = nullptr;
			if (O->TryGetObjectField(TEXT("ranged"), Ranged))
			{
				const TSharedPtr<FJsonObject>& R = *Ranged;
				FString Str;
				if (R->TryGetStringField(TEXT("kind"), Str)) D.RangedKind = FName(*Str);
				if (R->TryGetStringField(TEXT("ammo"), Str)) D.AmmoKind = FName(*Str);
				R->TryGetNumberField(TEXT("damage"), D.RangedDamage);
				R->TryGetNumberField(TEXT("speed"), D.ProjectileSpeed);
				R->TryGetNumberField(TEXT("gravity"), D.ProjectileGravity);
				R->TryGetNumberField(TEXT("reload"), D.Reload);
				R->TryGetNumberField(TEXT("torpor"), D.TorporMult);
				R->TryGetStringField(TEXT("mesh"), D.HeldMesh);
				if (R->TryGetStringField(TEXT("socket"), Str)) D.HeldSocket = FName(*Str);
				if (R->TryGetStringField(TEXT("along"), Str)) D.HeldAlong = FName(*Str);
				if (R->TryGetStringField(TEXT("face"), Str)) D.HeldFace = FName(*Str);
				if (R->TryGetStringField(TEXT("pose"), Str)) D.HeldPose = FName(*Str);
				R->TryGetStringField(TEXT("meshAlong"), D.MeshAlong);
				R->TryGetStringField(TEXT("meshFace"), D.MeshFace);
				const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
				if (R->TryGetArrayField(TEXT("rot"), Arr) && Arr->Num() == 3) D.HeldRot = FRotator((*Arr)[0]->AsNumber(), (*Arr)[1]->AsNumber(), (*Arr)[2]->AsNumber());
				if (R->TryGetArrayField(TEXT("offset"), Arr) && Arr->Num() == 3) D.HeldOffset = FVector((*Arr)[0]->AsNumber(), (*Arr)[1]->AsNumber(), (*Arr)[2]->AsNumber());
			}
			const TSharedPtr<FJsonObject>* AmmoObj = nullptr;
			if (O->TryGetObjectField(TEXT("ammo"), AmmoObj))
			{
				FString Str;
				if ((*AmmoObj)->TryGetStringField(TEXT("kind"), Str)) D.AmmoOf = FName(*Str);
				(*AmmoObj)->TryGetNumberField(TEXT("damage"), D.AmmoDamage);
				(*AmmoObj)->TryGetNumberField(TEXT("torpor"), D.AmmoTorpor);
				(*AmmoObj)->TryGetStringField(TEXT("mesh"), D.ProjectileMesh);
				(*AmmoObj)->TryGetStringField(TEXT("meshAlong"), D.MeshAlong);
			}
			// Talismans: what wearing it gives. Rings: the gem set in the band (its stats come from the gem's, WRGear).
			const TSharedPtr<FJsonObject>* WornObj = nullptr;
			if (O->TryGetObjectField(TEXT("worn"), WornObj))
			{
				for (const auto& W : (*WornObj)->Values) D.Worn.Add(FName(*W.Key), float(W.Value->AsNumber()));
			}
			const TSharedPtr<FJsonObject>* RingObj = nullptr;
			FString RingGem;
			if (O->TryGetObjectField(TEXT("ring"), RingObj) && (*RingObj)->TryGetStringField(TEXT("gem"), RingGem)) D.RingGem = FName(*RingGem);
			const TSharedPtr<FJsonObject>* Gem = nullptr;
			if (O->TryGetObjectField(TEXT("gem"), Gem))
			{
				for (const TPair<FString, TSharedPtr<FJsonValue>>& S : (*Gem)->Values)
				{
					D.GemStats.Add(FName(*S.Key), static_cast<float>(S.Value->AsNumber()));
				}
			}
			Defs.Add(D);
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* KitArray = nullptr;
	if (Root.IsValid() && Root->TryGetArrayField(TEXT("startingKit"), KitArray))
	{
		for (const TSharedPtr<FJsonValue>& V : *KitArray)
		{
			Kit.Add({ FName(*V->AsObject()->GetStringField(TEXT("id"))), static_cast<int32>(V->AsObject()->GetNumberField(TEXT("count"))) });
		}
	}
	UE_LOG(LogWildreach, Log, TEXT("Loaded %d item definitions"), Defs.Num());
}

const FWRItemDef* FWRItemRegistry::Find(FName Id) const
{
	return Defs.FindByPredicate([Id](const FWRItemDef& D) { return D.Id == Id; });
}

const TCHAR* FWRItemStack::QualityName(EWRQuality Q)
{
	switch (Q)
	{
	case EWRQuality::Crude: return TEXT("Crude");
	case EWRQuality::Fine: return TEXT("Fine");
	case EWRQuality::Masterwork: return TEXT("Masterwork");
	default: return TEXT("Common");
	}
}

// ---------------------------------------------------------------- rules

FWRItemStack FWRInventoryRules::MakeInstance(FName ItemId, EWRQuality Quality, float Roll)
{
	FWRItemStack S;
	S.ItemId = ItemId;
	S.Count = 1;
	S.Quality = Quality;
	S.Roll = Roll;
	if (const FWRItemDef* Def = FWRItemRegistry::Get().Find(ItemId))
	{
		S.Durability = Def->MaxDurability;
	}
	return S;
}

int32 FWRInventoryRules::Add(TArray<FWRItemStack>& Slots, FName ItemId, int32 Count)
{
	const FWRItemDef* Def = FWRItemRegistry::Get().Find(ItemId);
	if (!Def || Count <= 0)
	{
		return Count;
	}
	if (Def->IsEquipment() && Def->StackSize <= 1)
	{
		// Equipment never stacks (thrown spears and bolas do); each one is its own instance.
		while (Count > 0 && AddInstance(Slots, MakeInstance(ItemId)))
		{
			--Count;
		}
		return Count;
	}
	return AddFood(Slots, ItemId, Count, 1.f, 1.f);
}

int32 FWRInventoryRules::AddFood(TArray<FWRItemStack>& Slots, FName ItemId, int32 Count, float Fresh, float Worth)
{
	const FWRItemDef* Def = FWRItemRegistry::Get().Find(ItemId);
	if (!Def || Count <= 0)
	{
		return Count;
	}
	if (Def->IsEquipment() && Def->StackSize <= 1)
	{
		return Add(Slots, ItemId, Count);
	}
	const bool bFood = Def->Perishes();
	const double Now = FWRSpoil::ServerNow;
	Fresh = FMath::Clamp(Fresh, 0.f, 1.f);
	for (FWRItemStack& S : Slots)
	{
		if (Count > 0 && S.ItemId == ItemId && S.Count < Def->StackSize && FMath::IsNearlyEqual(S.Roll, Worth, 0.01f))
		{
			const int32 Moved = FMath::Min(Count, Def->StackSize - S.Count);
			if (bFood)
			{
				// Joining stacks average their freshness by count (DST), then go on at the stack's own pace.
				const float Mine = FWRSpoil::FreshOf(S, Now);
				const float Rate = S.FreshRate;
				S.Fresh = (Mine * S.Count + Fresh * Moved) / float(S.Count + Moved);
				S.FreshAt = float(Now);
				S.FreshRate = Rate > 0.f ? Rate : FWRSpoil::RateFor(ItemId, 1.f);
			}
			S.Count += Moved;
			Count -= Moved;
		}
	}
	for (FWRItemStack& S : Slots)
	{
		if (Count > 0 && S.IsEmpty())
		{
			S = FWRItemStack();
			S.ItemId = ItemId;
			S.Count = FMath::Min(Count, Def->StackSize);
			S.Roll = Worth;
			if (bFood) FWRSpoil::StampNew(S, Fresh, Now);
			Count -= S.Count;
		}
	}
	return Count;
}

int32 FWRInventoryRules::MoveFrom(FWRItemStack& From, TArray<FWRItemStack>& To, int32 N)
{
	const FWRItemDef* ItemDef = From.IsEmpty() ? nullptr : FWRItemRegistry::Get().Find(From.ItemId);
	if (!ItemDef)
	{
		return 0;
	}
	if (ItemDef->IsEquipment() && ItemDef->StackSize <= 1)
	{
		if (!AddInstance(To, From)) return 0;
		From = FWRItemStack();
		return 1;
	}
	N = FMath::Clamp(N, 1, From.Count);
	const int32 Moved = N - AddFood(To, From.ItemId, N, FWRSpoil::FreshOf(From, FWRSpoil::ServerNow), From.Roll);
	From.Count -= Moved;
	if (From.Count <= 0) From = FWRItemStack();
	return Moved;
}

// ---------------------------------------------------------------- spoilage

double FWRSpoil::ServerNow = 0.0;
double FWRSpoil::DisplayNow = 0.0;

double FWRSpoil::NowFor(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World) return ServerNow;
	if (const AGameStateBase* GS = World->GetGameState()) return GS->GetServerWorldTimeSeconds();
	return World->GetTimeSeconds();
}

bool FWRSpoil::Perishes(FName ItemId)
{
	const FWRItemDef* Def = FWRItemRegistry::Get().Find(ItemId);
	return Def && Def->Perishes();
}

float FWRSpoil::RateFor(FName ItemId, float Mult)
{
	const FWRItemDef* Def = FWRItemRegistry::Get().Find(ItemId);
	if (!Def || !Def->Perishes()) return 0.f;
	return FMath::Max(0.f, Mult) / (Def->KeepsMinutes * 60.f);
}

float FWRSpoil::FreshOf(const FWRItemStack& S, double Now)
{
	if (S.IsEmpty()) return 1.f;
	if (S.FreshRate <= 0.f) return FMath::Clamp(S.Fresh, 0.f, 1.f);
	return FMath::Clamp(float(S.Fresh - (Now - S.FreshAt) * S.FreshRate), 0.f, 1.f);
}

double FWRSpoil::SecondsLeft(const FWRItemStack& S, double Now)
{
	if (S.IsEmpty() || S.FreshRate <= 0.f) return -1.0;
	return FreshOf(S, Now) / S.FreshRate;
}

void FWRSpoil::Restamp(FWRItemStack& S, double Now, float Mult)
{
	S.Fresh = FreshOf(S, Now);
	S.FreshAt = float(Now);
	S.FreshRate = RateFor(S.ItemId, Mult);
}

void FWRSpoil::StampNew(FWRItemStack& S, float Fresh, double Now)
{
	S.Fresh = FMath::Clamp(Fresh, 0.f, 1.f);
	S.FreshAt = float(Now);
	S.FreshRate = RateFor(S.ItemId, 1.f);
}

int32 FWRSpoil::Sweep(TArray<FWRItemStack>& Slots, double Now, float Mult)
{
	int32 Changed = 0;
	int32 Spoiled = 0;
	for (FWRItemStack& S : Slots)
	{
		if (S.IsEmpty() || !Perishes(S.ItemId)) continue;
		if (FreshOf(S, Now) <= 0.f)
		{
			Spoiled += S.Count;
			S = FWRItemStack();
			++Changed;
			continue;
		}
		const float Want = RateFor(S.ItemId, Mult);
		if (!FMath::IsNearlyEqual(S.FreshRate, Want, Want * 0.001f + 1e-9f))
		{
			Restamp(S, Now, Mult);
			++Changed;
		}
	}
	if (Spoiled > 0)
	{
		// What's left of it (compost, chum, pig feed); what doesn't fit is lost.
		FWRInventoryRules::Add(Slots, TEXT("spoiled_food"), Spoiled);
	}
	return Changed;
}

float FWRSpoil::ValueMult(const FWRItemStack& S, double Now)
{
	if (!Perishes(S.ItemId)) return 1.f;
	return FreshOf(S, Now) <= WRTune(TEXT("food.staleBelow"), 0.25f) ? WRTune(TEXT("food.staleValue"), 0.5f) : 1.f;
}

bool FWRInventoryRules::AddInstance(TArray<FWRItemStack>& Slots, const FWRItemStack& Instance)
{
	for (FWRItemStack& S : Slots)
	{
		if (S.IsEmpty())
		{
			S = Instance;
			S.Count = 1;
			return true;
		}
	}
	return false;
}

int32 FWRInventoryRules::Remove(TArray<FWRItemStack>& Slots, FName ItemId, int32 Count)
{
	int32 Removed = 0;
	for (int32 i = Slots.Num() - 1; i >= 0 && Removed < Count; --i)
	{
		FWRItemStack& S = Slots[i];
		if (S.ItemId == ItemId)
		{
			const int32 Take = FMath::Min(Count - Removed, S.Count);
			S.Count -= Take;
			Removed += Take;
			if (S.Count <= 0)
			{
				S = FWRItemStack();
			}
		}
	}
	return Removed;
}

int32 FWRInventoryRules::CountOf(const TArray<FWRItemStack>& Slots, FName ItemId)
{
	int32 Total = 0;
	for (const FWRItemStack& S : Slots)
	{
		Total += S.ItemId == ItemId ? S.Count : 0;
	}
	return Total;
}

float FWRInventoryRules::TotalWeight(const TArray<FWRItemStack>& Slots)
{
	float Total = 0.f;
	for (const FWRItemStack& S : Slots)
	{
		if (const FWRItemDef* Def = S.IsEmpty() ? nullptr : FWRItemRegistry::Get().Find(S.ItemId))
		{
			Total += Def->Weight * S.Count;
		}
	}
	return Total;
}

// ---------------------------------------------------------------- component

UWRInventoryComponent::UWRInventoryComponent()
{
	SetIsReplicatedByDefault(true);
	Slots.SetNum(NumSlots);
}

void UWRInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UWRInventoryComponent, Slots, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRInventoryComponent, EquippedWeapon, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRInventoryComponent, EquippedArmor, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRInventoryComponent, EquippedRanged, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRInventoryComponent, EquippedLight, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRInventoryComponent, EquippedHead, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRInventoryComponent, EquippedHands, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRInventoryComponent, EquippedLegs, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRInventoryComponent, EquippedTalisman, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRInventoryComponent, EquippedRing, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRInventoryComponent, EquippedRing2, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRInventoryComponent, PotionReadyAt, COND_OwnerOnly);
}

UWRInventoryComponent* UWRInventoryComponent::FindFor(const AActor* Character)
{
	const APawn* Pawn = Cast<APawn>(Character);
	const APlayerState* PS = Pawn ? Pawn->GetPlayerState() : nullptr;
	return PS ? PS->FindComponentByClass<UWRInventoryComponent>() : nullptr;
}

AWRCharacterBase* UWRInventoryComponent::GetPawnCharacter() const
{
	const APlayerState* PS = Cast<APlayerState>(GetOwner());
	return PS ? Cast<AWRCharacterBase>(PS->GetPawn()) : nullptr;
}

int32 UWRInventoryComponent::AddItem(FName ItemId, int32 Count)
{
	return GetOwner()->HasAuthority() ? FWRInventoryRules::Add(Slots, ItemId, Count) : Count;
}

int32 UWRInventoryComponent::AddFood(FName ItemId, int32 Count, float Fresh, float Worth)
{
	return GetOwner()->HasAuthority() ? FWRInventoryRules::AddFood(Slots, ItemId, Count, Fresh, Worth) : Count;
}

bool UWRInventoryComponent::AddInstance(const FWRItemStack& Instance)
{
	return GetOwner()->HasAuthority() && FWRInventoryRules::AddInstance(Slots, Instance);
}

int32 UWRInventoryComponent::RemoveItem(FName ItemId, int32 Count)
{
	return GetOwner()->HasAuthority() ? FWRInventoryRules::Remove(Slots, ItemId, Count) : 0;
}

void UWRInventoryComponent::GiveStartingKit()
{
	if (bKitGiven || !GetOwner()->HasAuthority())
	{
		return;
	}
	bKitGiven = true;
	for (const TPair<FName, int32>& Entry : FWRItemRegistry::Get().StartingKit())
	{
		AddItem(Entry.Key, Entry.Value);
	}
	// Start holding the starter weapon.
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		const FWRItemDef* Def = Slots[i].IsEmpty() ? nullptr : FWRItemRegistry::Get().Find(Slots[i].ItemId);
		if (Def && Def->Slot == EWREquipSlot::Weapon && EquippedWeapon.IsEmpty())
		{
			UseSlot(i);
		}
	}
}

float UWRInventoryComponent::GetPotionCooldownRemaining() const
{
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	return GS ? FMath::Max(0.f, PotionReadyAt - static_cast<float>(GS->GetServerWorldTimeSeconds())) : 0.f;
}

FWRItemStack* UWRInventoryComponent::GetEquippedMutable(EWREquipSlot Slot)
{
	switch (Slot)
	{
	case EWREquipSlot::Weapon: return &EquippedWeapon;
	case EWREquipSlot::Armor: return &EquippedArmor;
	case EWREquipSlot::Ranged: return &EquippedRanged;
	case EWREquipSlot::Light: return &EquippedLight;
	case EWREquipSlot::Head: return &EquippedHead;
	case EWREquipSlot::Hands: return &EquippedHands;
	case EWREquipSlot::Legs: return &EquippedLegs;
	case EWREquipSlot::Talisman: return &EquippedTalisman;
	case EWREquipSlot::Ring: return &EquippedRing;
	case EWREquipSlot::Ring2: return &EquippedRing2;
	default: return nullptr;
	}
}

const FWRItemStack& UWRInventoryComponent::GetEquipped(EWREquipSlot Slot) const
{
	static const FWRItemStack Empty;
	const FWRItemStack* S = const_cast<UWRInventoryComponent*>(this)->GetEquippedMutable(Slot);
	return S ? *S : Empty;
}

namespace WRInvGear
{
	/** Worn and whole: a piece at 0 durability gives nothing (talismans and rings never wear). */
	bool Intact(const FWRItemStack& S, const FWRItemDef& D) { return !S.IsEmpty() && (D.MaxDurability <= 0.f || S.Durability > 0.f); }
}

float UWRInventoryComponent::RingStat(const FWRItemDef& Ring, FName Stat)
{
	const FWRItemDef* Gem = Ring.RingGem.IsNone() ? nullptr : FWRItemRegistry::Get().Find(Ring.RingGem);
	const float Base = Gem ? Gem->GemStats.FindRef(Stat) : 0.f;
	if (Base == 0.f)
	{
		return 0.f;
	}
	const FWRGear& G = FWRGear::Get();
	return Base * G.RingShare * (1.f + G.RingPerTier * float(G.MetalTier.FindRef(Ring.Metal)));
}

int32 UWRInventoryComponent::SetPieces(FName Set) const
{
	if (Set.IsNone())
	{
		return 0;
	}
	int32 N = 0;
	for (const EWREquipSlot Slot : WRGearSlots::Armour)
	{
		const FWRItemStack& S = GetEquipped(Slot);
		const FWRItemDef* D = S.IsEmpty() ? nullptr : FWRItemRegistry::Get().Find(S.ItemId);
		if (D && D->Set == Set && WRInvGear::Intact(S, *D)) ++N;
	}
	// Set crafter (armorcraft perk): set bonuses need one fewer piece. The perk alone, not gear (no loop through StatFor).
	const UWRLifeSkillsComponent* Skills = UWRLifeSkillsComponent::FindFor(GetOwner());
	if (N > 0 && Skills && Skills->Stat(TEXT("setCrafter")) > 0.f) ++N;
	return N;
}

float UWRInventoryComponent::GearStat(FName Stat) const
{
	float Total = GemStat(Stat);
	// The biome sets worn: each set once, its bonuses for the pieces worn.
	TArray<FName, TInlineAllocator<4>> Seen;
	for (const EWREquipSlot Slot : WRGearSlots::Armour)
	{
		const FWRItemStack& S = GetEquipped(Slot);
		const FWRItemDef* D = S.IsEmpty() ? nullptr : FWRItemRegistry::Get().Find(S.ItemId);
		if (!D || D->Set.IsNone() || Seen.Contains(D->Set)) continue;
		Seen.Add(D->Set);
		const FWRSetDef* SetDef = FWRGear::Get().FindSet(D->Set);
		if (!SetDef) continue;
		const int32 N = SetPieces(D->Set);
		for (const FWRSetBonus& B : SetDef->Bonuses)
		{
			if (N >= B.Pieces) Total += B.Stats.FindRef(Stat);
		}
	}
	if (const FWRItemDef* T = EquippedTalisman.IsEmpty() ? nullptr : FWRItemRegistry::Get().Find(EquippedTalisman.ItemId))
	{
		Total += T->Worn.FindRef(Stat);
	}
	// Two of the same ring count once (Elden Ring's talisman rule).
	const FWRItemDef* R1 = EquippedRing.IsEmpty() ? nullptr : FWRItemRegistry::Get().Find(EquippedRing.ItemId);
	const FWRItemDef* R2 = EquippedRing2.IsEmpty() ? nullptr : FWRItemRegistry::Get().Find(EquippedRing2.ItemId);
	if (R1) Total += RingStat(*R1, Stat);
	if (R2 && R2 != R1) Total += RingStat(*R2, Stat);
	return Total;
}

float UWRInventoryComponent::GearStatFor(const AActor* Who, FName Stat)
{
	const UWRInventoryComponent* Inv = nullptr;
	if (const APlayerState* PS = Cast<APlayerState>(Who)) Inv = PS->FindComponentByClass<UWRInventoryComponent>();
	else if (const AController* C = Cast<AController>(Who)) Inv = C->PlayerState ? C->PlayerState->FindComponentByClass<UWRInventoryComponent>() : nullptr;
	else Inv = FindFor(Who);
	return Inv ? Inv->GearStat(Stat) : 0.f;
}

void UWRInventoryComponent::WearArmour(float Amount)
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}
	bool bBroke = false;
	for (const EWREquipSlot Slot : WRGearSlots::Armour)
	{
		FWRItemStack* Item = GetEquippedMutable(Slot);
		if (!Item || Item->IsEmpty() || Item->Durability <= 0.f) continue;
		Item->Durability = FMath::Max(0.f, Item->Durability - Amount);
		if (Item->Durability <= 0.f)
		{
			UE_LOG(LogWildreach, Log, TEXT("ITEM %s broke"), *Item->ItemId.ToString());
			bBroke = true;
		}
	}
	if (bBroke)
	{
		ApplyEquipment();
	}
}

float UWRInventoryComponent::GemStat(FName Stat) const
{
	float Total = 0.f;
	// A boss weapon's own effects (Cinderbrand's lava ring, Rimefang's freezing guard): always on, no socket.
	if (const FWRItemDef* W = EquippedWeapon.IsEmpty() ? nullptr : FWRItemRegistry::Get().Find(EquippedWeapon.ItemId))
	{
		Total += W->Innate.FindRef(Stat);
	}
	for (const FName& Gem : EquippedWeapon.Sockets)
	{
		if (const FWRItemDef* Def = FWRItemRegistry::Get().Find(Gem))
		{
			Total += Def->GemStats.FindRef(Stat);
		}
	}
	return Total;
}

float UWRInventoryComponent::GetArmorValue() const
{
	// Head, chest, hands and legs: each piece's armour by its quality, roll and upgrade; broken pieces give none.
	float Total = 0.f;
	for (const EWREquipSlot Slot : WRGearSlots::Armour)
	{
		const FWRItemStack& Piece = GetEquipped(Slot);
		const FWRItemDef* Def = Piece.IsEmpty() ? nullptr : FWRItemRegistry::Get().Find(Piece.ItemId);
		if (!Def || Piece.Durability <= 0.f) continue;
		const float QualityMult = FWRTuning::Get().Num(FString::Printf(TEXT("weapon.quality.%s"), *FString(FWRItemStack::QualityName(Piece.Quality)).ToLower()), 1.f);
		Total += Def->Armor * QualityMult * Piece.Roll * (1.f + Piece.Upgrade * WRTune(TEXT("weapon.upgradePerLevel"), 0.05f));
	}
	return Total;
}

float UWRInventoryComponent::GetRangedMultiplier() const
{
	const FWRItemDef* Def = EquippedRanged.IsEmpty() ? nullptr : FWRItemRegistry::Get().Find(EquippedRanged.ItemId);
	if (!Def || Def->IsThrowable())
	{
		return 1.f;
	}
	const float QualityMult = FWRTuning::Get().Num(FString::Printf(TEXT("weapon.quality.%s"), *FString(FWRItemStack::QualityName(EquippedRanged.Quality)).ToLower()), 1.f);
	const bool bBroken = Def->MaxDurability > 0.f && EquippedRanged.Durability <= 0.f;
	return QualityMult * EquippedRanged.Roll * (bBroken ? 0.5f : 1.f);
}

void UWRInventoryComponent::ApplyEquipment()
{
	AWRCharacterBase* Character = GetPawnCharacter();
	if (!Character || !GetOwner()->HasAuthority())
	{
		return;
	}
	FWRWeaponStats Stats;
	if (const FWRItemDef* Def = EquippedWeapon.IsEmpty() ? nullptr : FWRItemRegistry::Get().Find(EquippedWeapon.ItemId))
	{
		Stats.Metal = Def->Metal;
		Stats.Quality = FName(*FString(FWRItemStack::QualityName(EquippedWeapon.Quality)).ToLower());
		Stats.Upgrade = EquippedWeapon.Upgrade;
		Stats.Roll = EquippedWeapon.Roll;
		// Sockets, a boss weapon's own effects and the rings (GearStat).
		Stats.GemDamage = GearStat(TEXT("damage"));
		Stats.GemCrit = GearStat(TEXT("critChance"));
		Stats.bBroken = EquippedWeapon.Durability <= 0.f;
		Stats.Element = EquippedWeapon.Element.IsNone() ? Def->InnateElement : EquippedWeapon.Element;
		Stats.bStaggerShockwave = GemStat(TEXT("staggerShockwave")) > 0.f;
		Stats.bBlockFreeze = GemStat(TEXT("blockFreeze")) > 0.f;
		if (const FWRToolDef* Tool = FWRGathering::FindTool(EquippedWeapon.ItemId))
		{
			// A held tool swings as a modest weapon: its own flat damage (less when broken), not the sword table.
			Stats.FlatDamage = Tool->Damage * (Stats.bBroken ? WRTune(TEXT("gather.tools.brokenDamage"), 0.5f) : 1.f);
		}
	}
	else
	{
		// Bare hands: a weak copper-grade hit.
		Stats.Quality = TEXT("crude");
		Stats.Roll = 0.5f;
	}
	Character->Weapon = Stats;
	Character->ArmorValue = GetArmorValue();
	// The roll's tier by what's worn and held (replicates to the owner, whose roll predicts with it), and the poise
	// the worn armour gives.
	Character->EquipLoadPct = uint8(FMath::Clamp(FMath::RoundToInt(100.f * GetEquipLoadKg() / FMath::Max(1.f, GetMaxEquipLoad())), 0, 255));
	UWRCombatLibrary::RefreshPoise(Character);
	// The worn armour's look on the hero, piece by piece (hero.md 2.5, cooks/build-crafting.md 2.4): broken armour
	// still shows; an empty slot wears the wraps' piece.
	if (AWRCharacter* Hero = Cast<AWRCharacter>(Character))
	{
		FName Ids[4], Elements[4];
		for (int32 i = 0; i < 4; ++i)
		{
			const FWRItemStack& Piece = GetEquipped(WRGearSlots::Armour[i]);
			Ids[i] = Piece.IsEmpty() ? NAME_None : Piece.ItemId;
			Elements[i] = Piece.IsEmpty() ? NAME_None : Piece.Element;
		}
		Hero->SetWornArmor(Ids, Elements);
	}
	if (AWRCharacter* Survivor = Cast<AWRCharacter>(Character))
	{
		Survivor->SetRangedItem(EquippedRanged.IsEmpty() ? NAME_None : EquippedRanged.ItemId);
		Survivor->SetMeleeItem(EquippedWeapon.IsEmpty() ? NAME_None : EquippedWeapon.ItemId);
	}
	// The Light slot: a torch or lantern put in it comes on (WRSurvival).
	if (UWRSurvivalComponent* Survival = UWRSurvivalComponent::For(GetOwner()))
	{
		Survival->OnLightSlotChanged();
	}
}

FName UWRInventoryComponent::FindAmmo(FName Kind, FName Preferred) const
{
	return FWRInventoryRules::FindAmmo(Slots, Kind, Preferred);
}

FName UWRInventoryComponent::NextAmmo(FName Kind, FName Current, int32 Dir) const
{
	return FWRInventoryRules::NextAmmo(Slots, Kind, Current, Dir);
}

FName FWRInventoryRules::FindAmmo(const TArray<FWRItemStack>& Slots, FName Kind, FName Preferred)
{
	const FWRItemRegistry& Items = FWRItemRegistry::Get();
	const FWRItemDef* Pref = Preferred.IsNone() ? nullptr : Items.Find(Preferred);
	if (Pref && Pref->AmmoOf == Kind && CountOf(Slots, Preferred) > 0)
	{
		return Preferred;
	}
	for (const FWRItemStack& S : Slots)
	{
		const FWRItemDef* Def = S.IsEmpty() ? nullptr : Items.Find(S.ItemId);
		if (Def && Def->AmmoOf == Kind)
		{
			return S.ItemId;
		}
	}
	return NAME_None;
}

FName FWRInventoryRules::NextAmmo(const TArray<FWRItemStack>& Slots, FName Kind, FName Current, int32 Dir)
{
	TArray<FName> Kinds;
	for (const FWRItemStack& S : Slots)
	{
		const FWRItemDef* Def = S.IsEmpty() ? nullptr : FWRItemRegistry::Get().Find(S.ItemId);
		if (Def && Def->AmmoOf == Kind)
		{
			Kinds.AddUnique(S.ItemId);
		}
	}
	if (Kinds.Num() == 0)
	{
		return NAME_None;
	}
	const int32 At = Kinds.IndexOfByKey(Current);
	return Kinds[At == INDEX_NONE ? 0 : (At + Dir + Kinds.Num()) % Kinds.Num()];
}

void UWRInventoryComponent::SpendThrowable()
{
	if (EquippedRanged.IsEmpty())
	{
		return;
	}
	const FName Id = EquippedRanged.ItemId;
	if (--EquippedRanged.Count > 0)
	{
		return;
	}
	EquippedRanged = FWRItemStack();
	for (FWRItemStack& S : Slots)
	{
		if (S.ItemId == Id && !S.IsEmpty())
		{
			EquippedRanged = S;
			S = FWRItemStack();
			break;
		}
	}
	ApplyEquipment();
}

void UWRInventoryComponent::WearEquipped(EWREquipSlot Slot, float Amount)
{
	FWRItemStack* Item = GetEquippedMutable(Slot);
	if (!Item || Item->IsEmpty() || Item->Durability <= 0.f || !GetOwner()->HasAuthority())
	{
		return;
	}
	Item->Durability = FMath::Max(0.f, Item->Durability - Amount);
	if (Item->Durability <= 0.f)
	{
		UE_LOG(LogWildreach, Log, TEXT("ITEM %s broke"), *Item->ItemId.ToString());
		ApplyEquipment();
	}
}

FString UWRInventoryComponent::Unequip(EWREquipSlot Slot)
{
	FWRItemStack* Item = GetEquippedMutable(Slot);
	if (!Item || Item->IsEmpty())
	{
		return TEXT("nothing equipped");
	}
	if (!FWRInventoryRules::AddInstance(Slots, *Item))
	{
		return TEXT("inventory full");
	}
	*Item = FWRItemStack();
	ApplyEquipment();
	WRSound::Broadcast(GetPawnCharacter(), TEXT("equip"));
	return TEXT("unequipped");
}

FString UWRInventoryComponent::EquipTo(int32 SlotIndex, EWREquipSlot Slot)
{
	if (!Slots.IsValidIndex(SlotIndex) || Slots[SlotIndex].IsEmpty() || !GetOwner()->HasAuthority())
	{
		return TEXT("empty slot");
	}
	const FWRItemDef* Def = FWRItemRegistry::Get().Find(Slots[SlotIndex].ItemId);
	if (!Def || !Def->IsEquipment())
	{
		return TEXT("that isn't worn");
	}
	const bool bFits = Def->Slot == Slot || (WRGearSlots::IsRing(Def->Slot) && WRGearSlots::IsRing(Slot));
	if (!bFits)
	{
		return FString::Printf(TEXT("%s goes in the %s slot"), *Def->Name, *WRGearSlots::Label(Def->Slot).ToString().ToLower());
	}
	FWRItemStack* Equipped = GetEquippedMutable(Slot);
	const FWRItemStack Previous = *Equipped;
	*Equipped = Slots[SlotIndex];
	Slots[SlotIndex] = Previous;
	ApplyEquipment();
	WRSound::Broadcast(GetPawnCharacter(), TEXT("equip"));
	return FString::Printf(TEXT("Equipped %s"), *Def->Name);
}

FString UWRInventoryComponent::UseSlot(int32 SlotIndex)
{
	if (!Slots.IsValidIndex(SlotIndex) || Slots[SlotIndex].IsEmpty() || !GetOwner()->HasAuthority())
	{
		return TEXT("empty slot");
	}
	FWRItemStack& Stack = Slots[SlotIndex];
	const FWRItemDef* Def = FWRItemRegistry::Get().Find(Stack.ItemId);
	if (!Def)
	{
		return TEXT("unknown item");
	}

	if (Def->Slot == EWREquipSlot::Light && EquippedLight.ItemId == Stack.ItemId && !EquippedLight.IsEmpty())
	{
		// The same light again from the bag or belt: light it or put it out rather than swapping one for the other.
		UWRSurvivalComponent* Survival = UWRSurvivalComponent::For(GetOwner());
		const FString Said = Survival ? Survival->ToggleLight() : FString();
		return Said.IsEmpty() ? FString::Printf(TEXT("%s lit"), *Def->Name) : Said;
	}
	if (Def->Category == TEXT("blueprint"))
	{
		// Read it: the recipe is yours for good (WRGear).
		return FWRBlueprints::Learn(this, SlotIndex);
	}
	if (Def->IsEquipment())
	{
		// Swap with whatever is in that slot. A ring goes on the first free finger, else replaces the first ring.
		EWREquipSlot Into = Def->Slot;
		if (WRGearSlots::IsRing(Into))
		{
			Into = EquippedRing.IsEmpty() ? EWREquipSlot::Ring : EquippedRing2.IsEmpty() ? EWREquipSlot::Ring2 : EWREquipSlot::Ring;
		}
		FWRItemStack* Equipped = GetEquippedMutable(Into);
		const FWRItemStack Previous = *Equipped;
		*Equipped = Stack;
		Stack = Previous;
		ApplyEquipment();
		WRSound::Broadcast(GetPawnCharacter(), TEXT("equip"));
		return FString::Printf(TEXT("Equipped %s %s"), FWRItemStack::QualityName(Equipped->Quality), *Def->Name);
	}
	if (Def->Category == TEXT("gem") || Def->Category == TEXT("rune"))
	{
		// Socket into the equipped weapon if it has a free socket (quality sets the count). Runes sit in sockets like
		// gems, one of each rune to a weapon (cooks/build-crafting.md 2.7).
		if (EquippedWeapon.IsEmpty() || FWRGathering::FindTool(EquippedWeapon.ItemId))
		{
			return Def->Category == TEXT("rune") ? TEXT("equip a weapon to set a rune in it") : TEXT("equip a weapon to socket gems");
		}
		if (EquippedWeapon.Sockets.Num() >= FWRItemStack::SocketCount(EquippedWeapon.Quality))
		{
			return TEXT("no free socket (quality sets sockets: crude 0 ... masterwork 3)");
		}
		if (Def->Category == TEXT("rune") && EquippedWeapon.Sockets.Contains(Def->Id))
		{
			return FString::Printf(TEXT("this weapon already carries a %s"), *Def->Name);
		}
		EquippedWeapon.Sockets.Add(Def->Id);
		const FString Name = Def->Name;
		RemoveItem(Def->Id, 1);
		ApplyEquipment();
		return FString::Printf(TEXT("Socketed %s"), *Name);
	}
	if (Def->Category == TEXT("dart") || Def->Category == TEXT("egg"))
	{
		return UseTamingItem(*Def);
	}
	if (Def->Category == TEXT("potion") && Def->IsUsable())
	{
		// The very drink picked (a cure is not a healing potion): committed, on the shared belt cooldown.
		if (GetWorld()->GetTimeSeconds() < PotionReadyAt)
		{
			return TEXT("potion belt cooling down");
		}
		AWRCharacterBase* Drinker = GetPawnCharacter();
		if (!Drinker || !Drinker->IsAlive() || !Drinker->GetAbilitySystemComponent())
		{
			return TEXT("can't use items now");
		}
		return StartDrink(*Def);
	}
	// Farm things: a seed by an empty plot plants it, a recipe scrap is read, feed by a pen fills its trough.
	{
		const FString Farm = FWRFarm::UseItem(this, SlotIndex);
		if (!Farm.IsEmpty()) return Farm;
	}
	if (Def->IsUsable())
	{
		// This very stack (its freshness and worth), not the first of its kind.
		PreferSlot = SlotIndex;
		const FString Said = UseCategory(Def->Category);
		PreferSlot = INDEX_NONE;
		return Said;
	}
	return FString::Printf(TEXT("%s can't be used"), *Def->Name);
}

FString UWRInventoryComponent::StartDrink(const FWRItemDef& Def)
{
	// The drink (CAL 1.10, combat.md 2.16): committed for potions.drinkSeconds (no guard, roll or swing meanwhile, a slow
	// walk), the heal or cure lands at potions.healAt if it's still going down, and a stagger, a freeze or death before
	// then loses it. The potion is used up as it starts. Creatures that read actions answer the drink (State.Drinking).
	AWRCharacterBase* Character = GetPawnCharacter();
	const FWRTuning& T = FWRTuning::Get();
	if (Character->HasTag(WRTags::State_Drinking))
	{
		return TEXT("already drinking");
	}
	if (Character->HasTag(WRTags::State_Mounted))
	{
		return TEXT("not while riding");
	}
	if (Character->HasTag(WRTags::Status_Staggered) || Character->HasTag(WRTags::Status_Frozen) || Character->HasTag(WRTags::State_Dodging))
	{
		return TEXT("can't drink now");
	}
	const float Now = GetWorld()->GetTimeSeconds();
	PotionReadyAt = Now + T.Num(TEXT("potions.sharedCooldown"), 1.5f) * FMath::Max(0.2f, 1.f + UWRLifeSkillsComponent::StatFor(GetOwner(), TEXT("potionCooldown")));
	Character->GetAbilitySystemComponent()->CancelAbilities();
	Character->Status->ApplyStatus(WRTags::State_Drinking, T.Num(TEXT("potions.drinkSeconds"), 0.9f));
	if (AWRCharacter* Hero = Cast<AWRCharacter>(Character))
	{
		Hero->MulticastBodyOneShot(TEXT("drink"));
	}
	WRSound::Broadcast(Character, TEXT("potion"));
	const FName Id = Def.Id;
	const FString Name = Def.Name;
	RemoveItem(Id, 1);
	TWeakObjectPtr<UWRInventoryComponent> WeakThis(this);
	FTimerHandle Lands;
	GetWorld()->GetTimerManager().SetTimer(Lands, FTimerDelegate::CreateWeakLambda(this, [WeakThis, Id]()
	{
		UWRInventoryComponent* Self = WeakThis.Get();
		AWRCharacterBase* C = Self ? Self->GetPawnCharacter() : nullptr;
		const FWRItemDef* D = FWRItemRegistry::Get().Find(Id);
		if (!C || !D || !C->IsAlive() || !C->HasTag(WRTags::State_Drinking))
		{
			UE_LOG(LogWildreach, Log, TEXT("ITEM %s's %s was lost (interrupted)"), *GetNameSafe(C), *Id.ToString());
			return;
		}
		UAbilitySystemComponent* ASC = C->GetAbilitySystemComponent();
		const UWRAttributeSet* A = C->GetAttributes();
		if (D->Heal > 0.f)
		{
			// Rot and decay halve healing taken.
			const float Heal = D->Heal * (1.f + UWRLifeSkillsComponent::StatFor(Self->GetOwner(), TEXT("potionPotency"))) * C->Status->HealingTakenScale();
			ASC->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), A->GetHealth() + Heal);
		}
		if (D->bCleanse || UWRLifeSkillsComponent::StatFor(Self->GetOwner(), TEXT("potionCleanse")) > 0.f)
		{
			C->Status->ClearStatus(WRTags::Status_Burn);
			C->Status->ClearStatus(WRTags::Status_Frostbite);
			C->Status->ClearStatus(WRTags::Status_Slowed);
		}
		for (const FName& Cure : D->Cures)
		{
			C->Status->CureStatus(WRStatus::FromName(Cure));
		}
		WRFx::BroadcastOn(C, TEXT("potion.heal"), NAME_None, 1.f, NAME_None, 0.5f);
	}), T.Num(TEXT("potions.healAt"), 0.5f), false);
	UE_LOG(LogWildreach, Log, TEXT("ITEM %s drinks %s (%d left)"), *Character->GetName(), *Id.ToString(), CountOf(Id));
	return FString::Printf(TEXT("Drinking %s"), *Name);
}

FString UWRInventoryComponent::UseTamingItem(const FWRItemDef& Def)
{
	AWRCharacterBase* Character = GetPawnCharacter();
	if (!Character || !Character->IsAlive())
	{
		return TEXT("can't use items now");
	}
	const FName Used = Def.Id;
	const FString Name = Def.Name;
	if (Def.Category == TEXT("egg"))
	{
		// Hatch: the newborn imprints on you.
		const APlayerState* PS = Cast<APlayerState>(GetOwner());
		const FName Species(*Used.ToString().RightChop(4));
		if (!PS || AWRCreature::CountTames(GetWorld(), PS->GetPlayerName()) >= AWRCreature::TameLimitFor(PS))
		{
			return TEXT("You can't keep any more tames");
		}
		const FVector At = Character->GetActorLocation() + Character->GetActorForwardVector() * 200.f - FVector(0.f, 0.f, Character->GetSimpleCollisionHalfHeight());
		AWRCreature* Hatchling = AWRCreature::SpawnCreature(GetWorld(), Species, At, Character->GetActorRotation());
		if (!Hatchling)
		{
			return TEXT("The egg won't hatch");
		}
		Hatchling->CompleteTame(PS->GetPlayerName());
		RemoveItem(Used, 1);
		return FString::Printf(TEXT("%s hatched and imprinted on you"), *Name);
	}
	// Torpor dart: hitscan along the aim, knocks creatures toward unconsciousness.
	const AController* C = Character->GetController();
	const FRotator Aim = C ? C->GetControlRotation() : Character->GetActorRotation();
	const FVector Start = Character->GetActorLocation() + FVector(0.f, 0.f, 60.f);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(WRDart), false, Character);
	RemoveItem(Used, 1);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, Start + Aim.Vector() * WRTune(TEXT("taming.dartRange"), 2500.f), ECC_Pawn, Params))
	{
		if (AWRCharacterBase* Target = Cast<AWRCharacterBase>(Hit.GetActor()))
		{
			Target->AddTorpor(WRTune(TEXT("taming.dartTorpor"), 40.f) * (1.f + UWRLifeSkillsComponent::StatFor(GetOwner(), TEXT("torporDealt"))));
			Target->NotifyAttackedBy(Character);
			return FString::Printf(TEXT("Dart hit %s"), *Target->GetDisplayLabel());
		}
	}
	return TEXT("Dart missed");
}

FString UWRInventoryComponent::UseCategory(FName Category)
{
	AWRCharacterBase* Character = GetPawnCharacter();
	if (!Character || !Character->IsAlive() || !Character->GetAbilitySystemComponent() || !GetOwner()->HasAuthority())
	{
		return TEXT("can't use items now");
	}

	// A healing drink first when the category is potions (the quick heal), then anything else usable of it.
	const FWRItemStack* Stack = Slots.FindByPredicate([Category](const FWRItemStack& S)
	{
		const FWRItemDef* Def = S.IsEmpty() ? nullptr : FWRItemRegistry::Get().Find(S.ItemId);
		return Def && Def->Category == Category && Def->IsUsable() && (Category != TEXT("potion") || Def->Heal > 0.f);
	});
	if (!Stack)
	{
		Stack = Slots.FindByPredicate([Category](const FWRItemStack& S)
		{
			const FWRItemDef* Def = S.IsEmpty() ? nullptr : FWRItemRegistry::Get().Find(S.ItemId);
			return Def && Def->Category == Category && Def->IsUsable();
		});
	}
	if (Category == TEXT("food"))
	{
		// Food: the stack that would spoil first (fresh-keeping things last), so a bag eats its oldest.
		const double Clock = FWRSpoil::ServerNow;
		double Best = TNumericLimits<double>::Max();
		for (const FWRItemStack& S : Slots)
		{
			const FWRItemDef* D = S.IsEmpty() ? nullptr : FWRItemRegistry::Get().Find(S.ItemId);
			if (!D || D->Category != Category || !D->IsUsable()) continue;
			const double Left = FWRSpoil::SecondsLeft(S, Clock);
			const double Key = Left < 0.0 ? 1.0e12 : Left;
			if (Key < Best) { Best = Key; Stack = &S; }
		}
	}
	if (Slots.IsValidIndex(PreferSlot) && !Slots[PreferSlot].IsEmpty())
	{
		Stack = &Slots[PreferSlot];
	}
	if (!Stack)
	{
		return FString::Printf(TEXT("no %s left"), *Category.ToString());
	}
	const FWRItemDef* Def = FWRItemRegistry::Get().Find(Stack->ItemId);
	const int32 StackIndex = static_cast<int32>(Stack - Slots.GetData());
	// Stale food (food.staleBelow) gives half; a golden crop's dish is worth more (its stack's Roll).
	const float Value = Def->Category == TEXT("potion") ? 1.f : FWRSpoil::ValueMult(*Stack, FWRSpoil::ServerNow) * FMath::Max(0.1f, Stack->Roll);

	// Healing comes only from crafted potions, limited by the belt cooldown.
	const float Now = GetWorld()->GetTimeSeconds();
	if (Def->Category == TEXT("potion") && Now < PotionReadyAt)
	{
		return TEXT("potion belt cooling down");
	}
	if (Def->Category == TEXT("potion"))
	{
		return StartDrink(*Def);
	}

	UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
	const UWRAttributeSet* A = Character->GetAttributes();
	if (Def->Heal > 0.f)
	{
		ASC->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), A->GetHealth() + Def->Heal * (1.f + UWRLifeSkillsComponent::StatFor(GetOwner(), TEXT("potionPotency"))));
	}
	if (Def->bCleanse || (Def->Category == TEXT("potion") && UWRLifeSkillsComponent::StatFor(GetOwner(), TEXT("potionCleanse")) > 0.f))
	{
		Character->Status->ClearStatus(WRTags::Status_Burn);
		Character->Status->ClearStatus(WRTags::Status_Frostbite);
		Character->Status->ClearStatus(WRTags::Status_Slowed);
	}
	if (Def->Category == TEXT("potion"))
	{
		PotionReadyAt = Now + Def->UseCooldown * FMath::Max(0.2f, 1.f + UWRLifeSkillsComponent::StatFor(GetOwner(), TEXT("potionCooldown")));
	}
	const float Food = Def->Hunger * Value * (1.f + UWRLifeSkillsComponent::StatFor(GetOwner(), TEXT("foodValue")));
	const float Drink = (Def->Thirst + (Def->Hunger > 0.f && UWRLifeSkillsComponent::StatFor(GetOwner(), TEXT("mealsQuench")) > 0.f ? Def->Hunger * 0.5f : 0.f)) * Value;
	// Sated (cooks/build-farming.md 2.8): hunger holds still for the dish's minutes.
	if (Def->SatedMinutes > 0.f)
	{
		UWRFoodComponent::Sate(GetOwner(), Def->SatedMinutes * Value);
	}
	if (Food > 0.f) ASC->SetNumericAttributeBase(UWRAttributeSet::GetHungerAttribute(), A->GetHunger() + Food);
	if (Drink > 0.f) ASC->SetNumericAttributeBase(UWRAttributeSet::GetThirstAttribute(), A->GetThirst() + Drink);
	// Shared meal (cooking perk): a meal feeds your clan mates close by too, at a share of its worth.
	const float Share = Def->Category != TEXT("potion") && Food > 0.f ? UWRLifeSkillsComponent::StatFor(GetOwner(), TEXT("sharedMeal")) : 0.f;
	const APlayerState* MyPS = Character->GetPlayerState();
	if (Share > 0.f && MyPS)
	{
		const AGameStateBase* GS = Character->GetWorld()->GetGameState();
		for (const APlayerState* Other : GS ? GS->PlayerArray : TArray<TObjectPtr<APlayerState>>())
		{
			AWRCharacterBase* Mate = Other && Other != MyPS ? Cast<AWRCharacterBase>(Other->GetPawn()) : nullptr;
			if (!Mate || !Mate->IsAlive() || !FWRClans::Get().SameClan(MyPS->GetPlayerName(), Other->GetPlayerName())
				|| FVector::Dist(Mate->GetActorLocation(), Character->GetActorLocation()) > WRTune(TEXT("lifeSkills.sharedMealRange"), 1000.f))
			{
				continue;
			}
			UAbilitySystemComponent* MateASC = Mate->GetAbilitySystemComponent();
			if (MateASC && Mate->GetAttributes())
			{
				MateASC->SetNumericAttributeBase(UWRAttributeSet::GetHungerAttribute(), FMath::Min(100.f, Mate->GetAttributes()->GetHunger() + Food * Share));
				if (AWRPlayerController* MatePC = Cast<AWRPlayerController>(Mate->GetController()))
				{
					MatePC->ClientNotify(FString::Printf(TEXT("%s shared a meal with you"), *MyPS->GetPlayerName()));
				}
			}
		}
	}

	const FName Used = Def->Id;
	WRSound::Broadcast(Character, Def->Category == TEXT("potion") ? FName(TEXT("potion")) : Def->Thirst > Def->Hunger ? FName(TEXT("drink")) : FName(TEXT("eat")));
	if (Slots.IsValidIndex(StackIndex) && Slots[StackIndex].ItemId == Used)
	{
		if (--Slots[StackIndex].Count <= 0) Slots[StackIndex] = FWRItemStack();
	}
	else
	{
		RemoveItem(Used, 1);
	}
	UE_LOG(LogWildreach, Log, TEXT("ITEM %s used %s (%d left)"), *Character->GetName(), *Used.ToString(), CountOf(Used));
	return FString::Printf(TEXT("Used %s"), *Def->Name);
}

// ---------------------------------------------------------------- persistence

TSharedRef<FJsonObject> FWRInventoryRules::StackToJson(const FWRItemStack& S)
{
	TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
	O->SetStringField(TEXT("id"), S.IsEmpty() ? FString() : S.ItemId.ToString());
	O->SetNumberField(TEXT("count"), S.IsEmpty() ? 0 : S.Count);
	const FWRItemDef* Def = S.IsEmpty() ? nullptr : FWRItemRegistry::Get().Find(S.ItemId);
	if (Def && Def->IsEquipment())
	{
		O->SetNumberField(TEXT("quality"), static_cast<int32>(S.Quality));
		O->SetNumberField(TEXT("roll"), S.Roll);
		O->SetNumberField(TEXT("upgrade"), S.Upgrade);
		O->SetNumberField(TEXT("durability"), S.Durability);
		O->SetStringField(TEXT("element"), S.Element.IsNone() ? FString() : S.Element.ToString());
		TArray<TSharedPtr<FJsonValue>> Sockets;
		for (const FName& G : S.Sockets) Sockets.Add(MakeShared<FJsonValueString>(G.ToString()));
		O->SetArrayField(TEXT("sockets"), Sockets);
	}
	if (Def && Def->Perishes())
	{
		O->SetNumberField(TEXT("fresh"), FWRSpoil::FreshOf(S, FWRSpoil::ServerNow));
	}
	if (Def && !Def->IsEquipment() && !FMath::IsNearlyEqual(S.Roll, 1.f))
	{
		O->SetNumberField(TEXT("worth"), S.Roll);
	}
	return O;
}

FWRItemStack FWRInventoryRules::StackFromJson(const TSharedPtr<FJsonObject>& O)
{
	FWRItemStack S;
	const FString Id = O->GetStringField(TEXT("id"));
	if (Id.IsEmpty() || !FWRItemRegistry::Get().Find(FName(*Id)))
	{
		return S;
	}
	S.ItemId = FName(*Id);
	S.Count = static_cast<int32>(O->GetNumberField(TEXT("count")));
	double Num = 0.0;
	if (O->TryGetNumberField(TEXT("quality"), Num)) S.Quality = static_cast<EWRQuality>(FMath::Clamp(static_cast<int32>(Num), 0, 3));
	if (O->TryGetNumberField(TEXT("roll"), Num)) S.Roll = Num;
	if (O->TryGetNumberField(TEXT("upgrade"), Num)) S.Upgrade = static_cast<int32>(Num);
	if (O->TryGetNumberField(TEXT("durability"), Num)) S.Durability = Num;
	else if (const FWRItemDef* Def = FWRItemRegistry::Get().Find(S.ItemId)) S.Durability = Def->MaxDurability;  // saved before it had any (0.4 tools)
	FString Element;
	if (O->TryGetStringField(TEXT("element"), Element) && !Element.IsEmpty()) S.Element = FName(*Element);
	const TArray<TSharedPtr<FJsonValue>>* Sockets = nullptr;
	if (O->TryGetArrayField(TEXT("sockets"), Sockets))
	{
		for (const TSharedPtr<FJsonValue>& G : *Sockets) S.Sockets.Add(FName(*G->AsString()));
	}
	if (O->TryGetNumberField(TEXT("worth"), Num)) S.Roll = Num;
	if (FWRSpoil::Perishes(S.ItemId))
	{
		// Kept as it was when saved (a logged-out bag doesn't spoil); the sweep sets its container's pace.
		double Fresh = 1.0;
		O->TryGetNumberField(TEXT("fresh"), Fresh);
		FWRSpoil::StampNew(S, float(Fresh), FWRSpoil::ServerNow);
	}
	return S;
}

TSharedRef<FJsonObject> UWRInventoryComponent::ToJson() const
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Array;
	for (const FWRItemStack& S : Slots)
	{
		Array.Add(MakeShared<FJsonValueObject>(FWRInventoryRules::StackToJson(S)));
	}
	Root->SetArrayField(TEXT("slots"), Array);
	Root->SetObjectField(TEXT("weapon"), FWRInventoryRules::StackToJson(EquippedWeapon));
	Root->SetObjectField(TEXT("armor"), FWRInventoryRules::StackToJson(EquippedArmor));
	Root->SetObjectField(TEXT("ranged"), FWRInventoryRules::StackToJson(EquippedRanged));
	Root->SetObjectField(TEXT("light"), FWRInventoryRules::StackToJson(EquippedLight));
	// The gear slots (cooks/build-crafting.md 2.3); gearSlots 2 marks a save from after the armour split.
	Root->SetObjectField(TEXT("head"), FWRInventoryRules::StackToJson(EquippedHead));
	Root->SetObjectField(TEXT("hands"), FWRInventoryRules::StackToJson(EquippedHands));
	Root->SetObjectField(TEXT("legs"), FWRInventoryRules::StackToJson(EquippedLegs));
	Root->SetObjectField(TEXT("talisman"), FWRInventoryRules::StackToJson(EquippedTalisman));
	Root->SetObjectField(TEXT("ring"), FWRInventoryRules::StackToJson(EquippedRing));
	Root->SetObjectField(TEXT("ring2"), FWRInventoryRules::StackToJson(EquippedRing2));
	Root->SetNumberField(TEXT("gearSlots"), 2);
	Root->SetBoolField(TEXT("kitGiven"), bKitGiven);
	return Root;
}

void UWRInventoryComponent::FromJson(const TSharedPtr<FJsonObject>& Json)
{
	const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
	if (!Json.IsValid() || !Json->TryGetArrayField(TEXT("slots"), Array))
	{
		return;
	}
	Slots.Reset();
	Slots.SetNum(NumSlots);
	for (int32 i = 0; i < NumSlots && i < Array->Num(); ++i)
	{
		Slots[i] = FWRInventoryRules::StackFromJson((*Array)[i]->AsObject());
	}
	const TSharedPtr<FJsonObject>* Weapon = nullptr;
	if (Json->TryGetObjectField(TEXT("weapon"), Weapon)) EquippedWeapon = FWRInventoryRules::StackFromJson(*Weapon);
	const TSharedPtr<FJsonObject>* Armor = nullptr;
	if (Json->TryGetObjectField(TEXT("armor"), Armor)) EquippedArmor = FWRInventoryRules::StackFromJson(*Armor);
	const TSharedPtr<FJsonObject>* RangedJson = nullptr;
	if (Json->TryGetObjectField(TEXT("ranged"), RangedJson)) EquippedRanged = FWRInventoryRules::StackFromJson(*RangedJson);
	const TSharedPtr<FJsonObject>* LightJson = nullptr;
	if (Json->TryGetObjectField(TEXT("light"), LightJson)) EquippedLight = FWRInventoryRules::StackFromJson(*LightJson);
	const auto LoadSlot = [&Json](const TCHAR* Field, FWRItemStack& Into)
	{
		const TSharedPtr<FJsonObject>* O = nullptr;
		if (Json->TryGetObjectField(Field, O)) Into = FWRInventoryRules::StackFromJson(*O);
	};
	LoadSlot(TEXT("head"), EquippedHead);
	LoadSlot(TEXT("hands"), EquippedHands);
	LoadSlot(TEXT("legs"), EquippedLegs);
	LoadSlot(TEXT("talisman"), EquippedTalisman);
	LoadSlot(TEXT("ring"), EquippedRing);
	LoadSlot(TEXT("ring2"), EquippedRing2);
	// A save from before the armour split (cooks/build-crafting.md 2.3): the one armour is now the chest piece, and the
	// rest of its kit is made to match (its quality, roll and infusion; new durability) and put on.
	double GearSlots = 1.0;
	Json->TryGetNumberField(TEXT("gearSlots"), GearSlots);
	if (GearSlots < 2.0 && !EquippedArmor.IsEmpty())
	{
		const FString Chest = EquippedArmor.ItemId.ToString();
		if (Chest.EndsWith(TEXT("_armor")))
		{
			const FString Tier = Chest.LeftChop(6);
			const TPair<const TCHAR*, FWRItemStack*> Kit[] = { { TEXT("_head"), &EquippedHead }, { TEXT("_hands"), &EquippedHands }, { TEXT("_legs"), &EquippedLegs } };
			for (const TPair<const TCHAR*, FWRItemStack*>& Piece : Kit)
			{
				const FName Id(*(Tier + Piece.Key));
				if (!Piece.Value->IsEmpty() || !FWRItemRegistry::Get().Find(Id)) continue;
				*Piece.Value = FWRInventoryRules::MakeInstance(Id, EquippedArmor.Quality, EquippedArmor.Roll);
				Piece.Value->Element = EquippedArmor.Element;
			}
			UE_LOG(LogWildreach, Log, TEXT("ITEM save from before the armour split: %s is the chest, its kit made to match"), *Chest);
		}
	}
	Json->TryGetBoolField(TEXT("kitGiven"), bKitGiven);
}

float UWRInventoryComponent::GetWornPoise() const
{
	// Every intact armour piece's poise, socketed onyx and onyx rings (GearStat).
	float Armour = 0.f;
	for (const EWREquipSlot Slot : WRGearSlots::Armour)
	{
		const FWRItemStack& Piece = GetEquipped(Slot);
		const FWRItemDef* Def = Piece.IsEmpty() ? nullptr : FWRItemRegistry::Get().Find(Piece.ItemId);
		if (Def && Piece.Durability > 0.f) Armour += Def->Poise;
	}
	return Armour + GearStat(TEXT("poise"));
}

float UWRInventoryComponent::GetWornResist(FName Status) const
{
	float Total = 0.f;
	for (const EWREquipSlot Slot : WRGearSlots::Armour)
	{
		const FWRItemStack& Piece = GetEquipped(Slot);
		const FWRItemDef* Def = Piece.IsEmpty() ? nullptr : FWRItemRegistry::Get().Find(Piece.ItemId);
		if (Def && Piece.Durability > 0.f) Total += Def->Resists.FindRef(Status);
	}
	return Total;
}

float UWRInventoryComponent::GetEquipLoadKg() const
{
	const UWRLifeSkillsComponent* Skills = UWRLifeSkillsComponent::FindFor(GetOwner());
	const float ArmorKg = Skills ? FMath::Max(0.f, 1.f + Skills->Stat(TEXT("armorWeight"))) : 1.f;
	float Kg = 0.f;
	for (const EWREquipSlot Slot : WRGearSlots::All)
	{
		const FWRItemStack& S = GetEquipped(Slot);
		if (Slot == EWREquipSlot::Light) continue;
		if (const FWRItemDef* Def = S.IsEmpty() ? nullptr : FWRItemRegistry::Get().Find(S.ItemId))
		{
			Kg += Def->Weight * (WRGearSlots::IsArmour(Def->Slot) ? ArmorKg : 1.f);
		}
	}
	// The shield on the left arm (survivors carry it: weapons.json carried).
	if (const FWRGuard* Carried = FWRWeapons::Get().Offhand(FWRWeapons::Get().Carried()))
	{
		Kg += Carried->Weight;
	}
	return Kg;
}

float UWRInventoryComponent::GetMaxEquipLoad() const
{
	return WRTune(TEXT("defense.maxEquipLoad"), 60.f) + UWRPathComponent::Stat(GetOwner(), TEXT("equipLoad"))
		+ UWRLifeSkillsComponent::StatFor(GetOwner(), TEXT("equipLoad"));
}

float UWRInventoryComponent::GetCarryWeight() const
{
	// Strong back / Light load / Light weave (life-skill perks): ore and stone, wood, armor weigh less for you. The
	// skills replicate to the owner, so the owner's movement and the server agree on the load.
	const UWRLifeSkillsComponent* Skills = UWRLifeSkillsComponent::FindFor(GetOwner());
	const float Ore = Skills ? FMath::Max(0.f, 1.f + Skills->Stat(TEXT("oreWeight"))) : 1.f;
	const float Wood = Skills ? FMath::Max(0.f, 1.f + Skills->Stat(TEXT("woodWeight"))) : 1.f;
	const float ArmorKg = Skills ? FMath::Max(0.f, 1.f + Skills->Stat(TEXT("armorWeight"))) : 1.f;
	const auto Factor = [Ore, Wood, ArmorKg](const FName Id, const FWRItemDef& Def)
	{
		if (Id == TEXT("stone") || Id.ToString().EndsWith(TEXT("_ore"))) return Ore;
		if (Id == TEXT("wood")) return Wood;
		return WRGearSlots::IsArmour(Def.Slot) ? ArmorKg : 1.f;
	};
	float Kg = 0.f;
	for (const FWRItemStack& S : Slots)
	{
		if (const FWRItemDef* Def = S.IsEmpty() ? nullptr : FWRItemRegistry::Get().Find(S.ItemId))
		{
			Kg += Def->Weight * S.Count * Factor(S.ItemId, *Def);
		}
	}
	for (const FWRItemStack* Worn : { &EquippedWeapon, &EquippedArmor, &EquippedRanged, &EquippedHead, &EquippedHands, &EquippedLegs, &EquippedTalisman, &EquippedRing, &EquippedRing2 })
	{
		const FWRItemDef* Def = Worn->IsEmpty() ? nullptr : FWRItemRegistry::Get().Find(Worn->ItemId);
		if (Def) Kg += Def->Weight * FMath::Max(1, Worn->Count) * Factor(Worn->ItemId, *Def);
	}
	return Kg;
}
