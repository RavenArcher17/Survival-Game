#include "WRInventory.h"
#include "WRCreature.h"
#include "WRLifeSkills.h"
#include "Wildreach.h"
#include "WRAttributeSet.h"
#include "WRCharacterBase.h"
#include "WRCharacter.h"
#include "WRGameplayTags.h"
#include "WRStatusComponent.h"
#include "WRTuning.h"
#include "AbilitySystemComponent.h"
#include "Dom/JsonObject.h"
#include "GameFramework/GameStateBase.h"
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
			const TSharedPtr<FJsonObject>* Use = nullptr;
			if (O->TryGetObjectField(TEXT("use"), Use))
			{
				(*Use)->TryGetNumberField(TEXT("heal"), D.Heal);
				(*Use)->TryGetNumberField(TEXT("hunger"), D.Hunger);
				(*Use)->TryGetNumberField(TEXT("thirst"), D.Thirst);
				(*Use)->TryGetNumberField(TEXT("cooldown"), D.UseCooldown);
				double Cleanse = 0.0;
				D.bCleanse = (*Use)->TryGetNumberField(TEXT("cleanse"), Cleanse) && Cleanse > 0.0;
			}
			const TSharedPtr<FJsonObject>* Equip = nullptr;
			if (O->TryGetObjectField(TEXT("equip"), Equip))
			{
				const FString Slot = (*Equip)->GetStringField(TEXT("slot"));
				D.Slot = Slot == TEXT("weapon") ? EWREquipSlot::Weapon : Slot == TEXT("armor") ? EWREquipSlot::Armor
					: Slot == TEXT("ranged") ? EWREquipSlot::Ranged : EWREquipSlot::None;
				FString Metal;
				if ((*Equip)->TryGetStringField(TEXT("metal"), Metal)) D.Metal = FName(*Metal);
				(*Equip)->TryGetNumberField(TEXT("armor"), D.Armor);
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
	for (FWRItemStack& S : Slots)
	{
		if (Count > 0 && S.ItemId == ItemId && S.Count < Def->StackSize)
		{
			const int32 Moved = FMath::Min(Count, Def->StackSize - S.Count);
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
			Count -= S.Count;
		}
	}
	return Count;
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
	return Slot == EWREquipSlot::Armor ? &EquippedArmor : Slot == EWREquipSlot::Weapon ? &EquippedWeapon : Slot == EWREquipSlot::Ranged ? &EquippedRanged : nullptr;
}

float UWRInventoryComponent::GemStat(FName Stat) const
{
	float Total = 0.f;
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
	const FWRItemDef* Def = EquippedArmor.IsEmpty() ? nullptr : FWRItemRegistry::Get().Find(EquippedArmor.ItemId);
	if (!Def || EquippedArmor.Durability <= 0.f)
	{
		return 0.f;
	}
	const float QualityMult = FWRTuning::Get().Num(FString::Printf(TEXT("weapon.quality.%s"), *FString(FWRItemStack::QualityName(EquippedArmor.Quality)).ToLower()), 1.f);
	return Def->Armor * QualityMult * EquippedArmor.Roll * (1.f + EquippedArmor.Upgrade * WRTune(TEXT("weapon.upgradePerLevel"), 0.05f));
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
		Stats.GemDamage = GemStat(TEXT("damage"));
		Stats.GemCrit = GemStat(TEXT("critChance"));
		Stats.bBroken = EquippedWeapon.Durability <= 0.f;
		Stats.Element = EquippedWeapon.Element;
		Stats.bStaggerShockwave = GemStat(TEXT("staggerShockwave")) > 0.f;
		Stats.bBlockFreeze = GemStat(TEXT("blockFreeze")) > 0.f;
	}
	else
	{
		// Bare hands: a weak copper-grade hit.
		Stats.Quality = TEXT("crude");
		Stats.Roll = 0.5f;
	}
	Character->Weapon = Stats;
	Character->ArmorValue = GetArmorValue();
	if (AWRCharacter* Survivor = Cast<AWRCharacter>(Character))
	{
		Survivor->SetRangedItem(EquippedRanged.IsEmpty() ? NAME_None : EquippedRanged.ItemId);
	}
}

FName UWRInventoryComponent::FindAmmo(FName Kind, FName Preferred) const
{
	const FWRItemRegistry& Items = FWRItemRegistry::Get();
	const FWRItemDef* Pref = Preferred.IsNone() ? nullptr : Items.Find(Preferred);
	if (Pref && Pref->AmmoOf == Kind && CountOf(Preferred) > 0)
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

FName UWRInventoryComponent::NextAmmo(FName Kind, FName Current, int32 Dir) const
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
	return TEXT("unequipped");
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

	if (Def->IsEquipment())
	{
		// Swap with whatever is in that slot.
		FWRItemStack* Equipped = GetEquippedMutable(Def->Slot);
		const FWRItemStack Previous = *Equipped;
		*Equipped = Stack;
		Stack = Previous;
		ApplyEquipment();
		return FString::Printf(TEXT("Equipped %s %s"), FWRItemStack::QualityName(Equipped->Quality), *Def->Name);
	}
	if (Def->Category == TEXT("gem"))
	{
		// Socket into the equipped weapon if it has a free socket (quality sets the count).
		if (EquippedWeapon.IsEmpty())
		{
			return TEXT("equip a weapon to socket gems");
		}
		if (EquippedWeapon.Sockets.Num() >= FWRItemStack::SocketCount(EquippedWeapon.Quality))
		{
			return TEXT("no free socket (quality sets sockets: crude 0 ... masterwork 3)");
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
	if (Def->IsUsable())
	{
		return UseCategory(Def->Category);
	}
	return FString::Printf(TEXT("%s can't be used"), *Def->Name);
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

	const FWRItemStack* Stack = Slots.FindByPredicate([Category](const FWRItemStack& S)
	{
		const FWRItemDef* Def = S.IsEmpty() ? nullptr : FWRItemRegistry::Get().Find(S.ItemId);
		return Def && Def->Category == Category && Def->IsUsable();
	});
	if (!Stack)
	{
		return FString::Printf(TEXT("no %s left"), *Category.ToString());
	}
	const FWRItemDef* Def = FWRItemRegistry::Get().Find(Stack->ItemId);

	// Healing comes only from crafted potions, limited by the belt cooldown.
	const float Now = GetWorld()->GetTimeSeconds();
	if (Def->Category == TEXT("potion") && Now < PotionReadyAt)
	{
		return TEXT("potion belt cooling down");
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
	const float Food = Def->Hunger * (1.f + UWRLifeSkillsComponent::StatFor(GetOwner(), TEXT("foodValue")));
	const float Drink = Def->Thirst + (Def->Hunger > 0.f && UWRLifeSkillsComponent::StatFor(GetOwner(), TEXT("mealsQuench")) > 0.f ? Def->Hunger * 0.5f : 0.f);
	if (Food > 0.f) ASC->SetNumericAttributeBase(UWRAttributeSet::GetHungerAttribute(), A->GetHunger() + Food);
	if (Drink > 0.f) ASC->SetNumericAttributeBase(UWRAttributeSet::GetThirstAttribute(), A->GetThirst() + Drink);

	const FName Used = Def->Id;
	RemoveItem(Used, 1);
	UE_LOG(LogWildreach, Log, TEXT("ITEM %s used %s (%d left)"), *Character->GetName(), *Used.ToString(), CountOf(Used));
	return FString::Printf(TEXT("Used %s"), *Def->Name);
}

// ---------------------------------------------------------------- persistence

static TSharedRef<FJsonObject> StackToJson(const FWRItemStack& S)
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
	return O;
}

static FWRItemStack StackFromJson(const TSharedPtr<FJsonObject>& O)
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
	FString Element;
	if (O->TryGetStringField(TEXT("element"), Element) && !Element.IsEmpty()) S.Element = FName(*Element);
	const TArray<TSharedPtr<FJsonValue>>* Sockets = nullptr;
	if (O->TryGetArrayField(TEXT("sockets"), Sockets))
	{
		for (const TSharedPtr<FJsonValue>& G : *Sockets) S.Sockets.Add(FName(*G->AsString()));
	}
	return S;
}

TSharedRef<FJsonObject> UWRInventoryComponent::ToJson() const
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Array;
	for (const FWRItemStack& S : Slots)
	{
		Array.Add(MakeShared<FJsonValueObject>(StackToJson(S)));
	}
	Root->SetArrayField(TEXT("slots"), Array);
	Root->SetObjectField(TEXT("weapon"), StackToJson(EquippedWeapon));
	Root->SetObjectField(TEXT("armor"), StackToJson(EquippedArmor));
	Root->SetObjectField(TEXT("ranged"), StackToJson(EquippedRanged));
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
		Slots[i] = StackFromJson((*Array)[i]->AsObject());
	}
	const TSharedPtr<FJsonObject>* Weapon = nullptr;
	if (Json->TryGetObjectField(TEXT("weapon"), Weapon)) EquippedWeapon = StackFromJson(*Weapon);
	const TSharedPtr<FJsonObject>* Armor = nullptr;
	if (Json->TryGetObjectField(TEXT("armor"), Armor)) EquippedArmor = StackFromJson(*Armor);
	const TSharedPtr<FJsonObject>* RangedJson = nullptr;
	if (Json->TryGetObjectField(TEXT("ranged"), RangedJson)) EquippedRanged = StackFromJson(*RangedJson);
	Json->TryGetBoolField(TEXT("kitGiven"), bKitGiven);
}
