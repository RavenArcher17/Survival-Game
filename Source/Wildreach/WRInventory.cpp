#include "WRInventory.h"
#include "Wildreach.h"
#include "WRAttributeSet.h"
#include "WRCharacterBase.h"
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

// ---------------------------------------------------------------- rules

int32 FWRInventoryRules::Add(TArray<FWRItemStack>& Slots, FName ItemId, int32 Count)
{
	const FWRItemDef* Def = FWRItemRegistry::Get().Find(ItemId);
	if (!Def || Count <= 0)
	{
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
			S.ItemId = ItemId;
			S.Count = FMath::Min(Count, Def->StackSize);
			Count -= S.Count;
		}
	}
	return Count;
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
	DOREPLIFETIME_CONDITION(UWRInventoryComponent, PotionReadyAt, COND_OwnerOnly);
}

UWRInventoryComponent* UWRInventoryComponent::FindFor(const AActor* Character)
{
	const APawn* Pawn = Cast<APawn>(Character);
	const APlayerState* PS = Pawn ? Pawn->GetPlayerState() : nullptr;
	return PS ? PS->FindComponentByClass<UWRInventoryComponent>() : nullptr;
}

int32 UWRInventoryComponent::AddItem(FName ItemId, int32 Count)
{
	return GetOwner()->HasAuthority() ? FWRInventoryRules::Add(Slots, ItemId, Count) : Count;
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
}

float UWRInventoryComponent::GetPotionCooldownRemaining() const
{
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	return GS ? FMath::Max(0.f, PotionReadyAt - static_cast<float>(GS->GetServerWorldTimeSeconds())) : 0.f;
}

FString UWRInventoryComponent::UseCategory(FName Category)
{
	const APlayerState* PS = Cast<APlayerState>(GetOwner());
	AWRCharacterBase* Character = PS ? Cast<AWRCharacterBase>(PS->GetPawn()) : nullptr;
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
	if (Def->Heal > 0.f && Now < PotionReadyAt)
	{
		return TEXT("potion belt cooling down");
	}

	UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
	const UWRAttributeSet* A = Character->GetAttributes();
	if (Def->Heal > 0.f)
	{
		ASC->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), A->GetHealth() + Def->Heal);
		PotionReadyAt = Now + Def->UseCooldown;
	}
	if (Def->Hunger > 0.f) ASC->SetNumericAttributeBase(UWRAttributeSet::GetHungerAttribute(), A->GetHunger() + Def->Hunger);
	if (Def->Thirst > 0.f) ASC->SetNumericAttributeBase(UWRAttributeSet::GetThirstAttribute(), A->GetThirst() + Def->Thirst);

	const FName Used = Def->Id;
	RemoveItem(Used, 1);
	UE_LOG(LogWildreach, Log, TEXT("ITEM %s used %s (%d left)"), *Character->GetName(), *Used.ToString(), CountOf(Used));
	return FString::Printf(TEXT("Used %s"), *Def->Name);
}

TSharedRef<FJsonObject> UWRInventoryComponent::ToJson() const
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Array;
	for (const FWRItemStack& S : Slots)
	{
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("id"), S.IsEmpty() ? FString() : S.ItemId.ToString());
		O->SetNumberField(TEXT("count"), S.IsEmpty() ? 0 : S.Count);
		Array.Add(MakeShared<FJsonValueObject>(O));
	}
	Root->SetArrayField(TEXT("slots"), Array);
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
		const TSharedPtr<FJsonObject> O = (*Array)[i]->AsObject();
		const FString Id = O->GetStringField(TEXT("id"));
		if (!Id.IsEmpty() && FWRItemRegistry::Get().Find(FName(*Id)))
		{
			Slots[i].ItemId = FName(*Id);
			Slots[i].Count = static_cast<int32>(O->GetNumberField(TEXT("count")));
		}
	}
	Json->TryGetBoolField(TEXT("kitGiven"), bKitGiven);
}
