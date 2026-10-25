#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WRInventory.generated.h"

class AWRCharacterBase;

UENUM()
enum class EWREquipSlot : uint8
{
	None,
	Weapon,
	Armor,
	Ranged,
};

/** One item definition from Data/items.json. */
struct FWRItemDef
{
	FName Id;
	FString Name;
	FName Category;
	int32 StackSize = 1;
	float Weight = 0.f;
	// Consumables
	float Heal = 0.f;
	float Hunger = 0.f;
	float Thirst = 0.f;
	float UseCooldown = 0.f;
	bool bCleanse = false;
	// Equipment
	EWREquipSlot Slot = EWREquipSlot::None;
	FName Metal;
	float Armor = 0.f;
	float MaxDurability = 0.f;
	// Ranged weapons ("ranged": {...}): bow, crossbow, blowgun; spear and bola are thrown (no ammo kind).
	FName RangedKind;
	FName AmmoKind;               // what it fires: arrow, bolt, dart
	float RangedDamage = 0.f;
	float ProjectileSpeed = 3000.f;
	float ProjectileGravity = 1.f;
	float Reload = 1.f;           // seconds between shots
	float TorporMult = 1.f;       // times the ammo's torpor
	FString HeldMesh;             // static mesh in the hand (also what a thrown weapon flies as)
	FName HeldSocket;
	FName HeldAlong;              // its long axis in the hand: forward / fingers / up / out / hands (level, toward the other hand)
	FName HeldFace;               // and its face: out / up / forward
	FName HeldPose;               // body clip whose pose those are measured in (none = the skeleton's reference pose)
	FRotator HeldRot = FRotator::ZeroRotator;
	FVector HeldOffset = FVector::ZeroVector;
	FString MeshAlong, MeshFace;  // the mesh's own axes ("+X", "-Y"...); empty = worked out from its bounds
	// Ammo ("ammo": {...})
	FName AmmoOf;                 // arrow, bolt, dart
	float AmmoDamage = 0.f;
	float AmmoTorpor = 0.f;
	FString ProjectileMesh;
	bool IsRanged() const { return !RangedKind.IsNone(); }
	bool IsThrowable() const { return IsRanged() && AmmoKind.IsNone(); }
	// Gems / boss parts: stat -> amount
	TMap<FName, float> GemStats;

	bool IsUsable() const { return Heal > 0.f || Hunger > 0.f || Thirst > 0.f || bCleanse; }
	bool IsEquipment() const { return Slot != EWREquipSlot::None; }
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

/** Quality tiers (Crafting tab): sockets 0/1/2/3, damage x0.9/1.0/1.1/1.2 (Starting numbers). */
UENUM()
enum class EWRQuality : uint8
{
	Crude,
	Common,
	Fine,
	Masterwork,
};

USTRUCT()
struct FWRItemStack
{
	GENERATED_BODY()

	UPROPERTY() FName ItemId;
	UPROPERTY() int32 Count = 0;
	// Per-instance data for equipment (stack size 1).
	UPROPERTY() EWRQuality Quality = EWRQuality::Common;
	/** Random stat roll within the quality, ~0.9..1.1. */
	UPROPERTY() float Roll = 1.f;
	UPROPERTY() int32 Upgrade = 0;
	UPROPERTY() float Durability = 0.f;
	UPROPERTY() TArray<FName> Sockets;
	/** Path material infused at craft time ("fire"/"frost"), or None. */
	UPROPERTY() FName Element;

	bool IsEmpty() const { return ItemId.IsNone() || Count <= 0; }
	static int32 SocketCount(EWRQuality Q) { return static_cast<int32>(Q); }
	static const TCHAR* QualityName(EWRQuality Q);
};

/** Pure slot logic (no world) so tests can cover stacking. */
struct WILDREACH_API FWRInventoryRules
{
	/** Adds as much as fits (fills existing stacks first). Returns how many didn't fit. */
	static int32 Add(TArray<FWRItemStack>& Slots, FName ItemId, int32 Count);
	/** Adds one prepared equipment instance. False if no free slot. */
	static bool AddInstance(TArray<FWRItemStack>& Slots, const FWRItemStack& Instance);
	/** Removes up to Count across stacks. Returns how many were removed. */
	static int32 Remove(TArray<FWRItemStack>& Slots, FName ItemId, int32 Count);
	static int32 CountOf(const TArray<FWRItemStack>& Slots, FName ItemId);
	static float TotalWeight(const TArray<FWRItemStack>& Slots);
	/** A fresh equipment instance with full durability. */
	static FWRItemStack MakeInstance(FName ItemId, EWRQuality Quality = EWRQuality::Common, float Roll = 1.f);
};

/**
 * A survivor's inventory + equipment: fixed slots, server-authoritative, replicated to the owner.
 * Lives on the PlayerState. Equipped weapon/armor stats are pushed onto the pawn.
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
	bool AddInstance(const FWRItemStack& Instance);
	int32 RemoveItem(FName ItemId, int32 Count);
	/** Use the first usable item of a category ("potion", "food", "drink"). Returns a message. */
	FString UseCategory(FName Category);
	/** Torpor darts and creature eggs. */
	FString UseTamingItem(const struct FWRItemDef& Def);
	/** Use / equip / socket the item in a slot. Returns a message. */
	FString UseSlot(int32 SlotIndex);
	FString Unequip(EWREquipSlot Slot);
	void GiveStartingKit();
	/** Wear on equipped gear (weapon on hit, armor when hit). Breaks at 0 (stats off until repaired). */
	void WearEquipped(EWREquipSlot Slot, float Amount);
	/** Push equipment stats onto the current pawn. */
	void ApplyEquipment();
	FWRItemStack* GetEquippedMutable(EWREquipSlot Slot);

	// --- Queries ---
	/** Server: direct access (death penalty, loot bags). Call ApplyEquipment after changing equipped gear. */
	TArray<FWRItemStack>& GetSlotsMutable() { return Slots; }
	const TArray<FWRItemStack>& GetSlots() const { return Slots; }
	const FWRItemStack& GetEquipped(EWREquipSlot Slot) const { return Slot == EWREquipSlot::Armor ? EquippedArmor : Slot == EWREquipSlot::Ranged ? EquippedRanged : EquippedWeapon; }
	/** Ammo of a kind to fire: Preferred if we still have it, else the first we carry. None if out. */
	FName FindAmmo(FName Kind, FName Preferred) const;
	/** The next (Dir +1) or previous (-1) ammo type of a kind we carry, after Current. */
	FName NextAmmo(FName Kind, FName Current, int32 Dir) const;
	/** Server: one thrown from the equipped spears / bolas; the next stack of them takes its place when it runs out. */
	void SpendThrowable();
	int32 CountOf(FName ItemId) const { return FWRInventoryRules::CountOf(Slots, ItemId); }
	float GetPotionCooldownRemaining() const;
	/** Sum of a gem stat over the equipped weapon's sockets ("damage", "critChance", ...). */
	float GemStat(FName Stat) const;
	float GetArmorValue() const;
	/** The equipped bow / crossbow / blowgun's quality x roll (x0.5 worn out): scales its own damage and torpor. */
	float GetRangedMultiplier() const;

	static constexpr int32 NumSlots = 30;

	TSharedRef<class FJsonObject> ToJson() const;
	void FromJson(const TSharedPtr<class FJsonObject>& Json);
	bool bKitGiven = false;

private:
	AWRCharacterBase* GetPawnCharacter() const;

	UPROPERTY(Replicated) TArray<FWRItemStack> Slots;
	UPROPERTY(Replicated) FWRItemStack EquippedWeapon;
	UPROPERTY(Replicated) FWRItemStack EquippedArmor;
	UPROPERTY(Replicated) FWRItemStack EquippedRanged;
	/** Server time the potion belt is ready again (replicated so the HUD can show it). */
	UPROPERTY(Replicated) float PotionReadyAt = 0.f;
};
