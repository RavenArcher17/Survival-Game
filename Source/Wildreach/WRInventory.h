#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WRInventory.generated.h"

/** One item definition from Data/items.json. */
struct FWRItemDef
{
	FName Id;
	FString Name;
	FName Category;
	int32 StackSize = 1;
	float Weight = 0.f;
	float Heal = 0.f;
	float Hunger = 0.f;
	float Thirst = 0.f;
	float UseCooldown = 0.f;
	bool IsUsable() const { return Heal > 0.f || Hunger > 0.f || Thirst > 0.f; }
};

class WILDREACH_API FWRItemRegistry
{
public:
	static FWRItemRegistry& Get();
	const FWRItemDef* Find(FName Id) const;
	const TArray<TPair<FName, int32>>& StartingKit() const { return Kit; }

private:
	FWRItemRegistry();
	TArray<FWRItemDef> Defs;
	TArray<TPair<FName, int32>> Kit;
};

USTRUCT()
struct FWRItemStack
{
	GENERATED_BODY()

	UPROPERTY() FName ItemId;
	UPROPERTY() int32 Count = 0;

	bool IsEmpty() const { return ItemId.IsNone() || Count <= 0; }
};

/** Pure slot logic (no world) so tests can cover stacking. */
struct WILDREACH_API FWRInventoryRules
{
	/** Adds as much as fits (fills existing stacks first). Returns how many didn't fit. */
	static int32 Add(TArray<FWRItemStack>& Slots, FName ItemId, int32 Count);
	/** Removes up to Count across stacks. Returns how many were removed. */
	static int32 Remove(TArray<FWRItemStack>& Slots, FName ItemId, int32 Count);
	static int32 CountOf(const TArray<FWRItemStack>& Slots, FName ItemId);
	static float TotalWeight(const TArray<FWRItemStack>& Slots);
};

/**
 * A survivor's inventory: fixed slots, server-authoritative, replicated to the owner.
 * Lives on the PlayerState (Souls-like death penalty keeps items; other penalties come with M7 settings).
 */
UCLASS(ClassGroup=(Inventory))
class WILDREACH_API UWRInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWRInventoryComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	static UWRInventoryComponent* FindFor(const AActor* Character);

	// --- Server ---
	int32 AddItem(FName ItemId, int32 Count);
	int32 RemoveItem(FName ItemId, int32 Count);
	/** Use the first usable item of a category ("potion", "food", "drink"). Returns a message. */
	FString UseCategory(FName Category);
	void GiveStartingKit();

	// --- Queries ---
	const TArray<FWRItemStack>& GetSlots() const { return Slots; }
	int32 CountOf(FName ItemId) const { return FWRInventoryRules::CountOf(Slots, ItemId); }
	float GetPotionCooldownRemaining() const;

	static constexpr int32 NumSlots = 30;

	TSharedRef<class FJsonObject> ToJson() const;
	void FromJson(const TSharedPtr<class FJsonObject>& Json);
	bool bKitGiven = false;

private:
	UPROPERTY(Replicated) TArray<FWRItemStack> Slots;
	/** Server time the potion belt is ready again (replicated so the HUD can show it). */
	UPROPERTY(Replicated) float PotionReadyAt = 0.f;
};
