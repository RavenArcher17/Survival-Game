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
	/** A torch (off hand) or lantern (hip) that lights you (WRSurvival). */
	Light,
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
	/** Worn poise (armour, COMBAT-AND-LESSONS 1.5): what a blow's poise damage has to get through before you react. */
	float Poise = 0.f;
	/** Weapon class in Data/weapons.json (its moves); none = sword. */
	FName WeaponClass;
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

	/** Food (cooks/build-farming.md 2.8, 2.9): how many minutes it keeps fresh in a bag (0 = never spoils), and how many
	 *  minutes eating it holds hunger off (use.sated: hunger doesn't drain; never a stat buff). */
	float KeepsMinutes = 0.f;
	float SatedMinutes = 0.f;
	bool Perishes() const { return KeepsMinutes > 0.f; }

	/** Statuses a drink ends (use.cures: bleed, shock, blind, curse, burn, frostbite, poison, rot). */
	TArray<FName> Cures;
	/** Worn status resistance by status id (equip.resist). */
	TMap<FName, float> Resists;
	bool IsUsable() const { return Heal > 0.f || Hunger > 0.f || Thirst > 0.f || bCleanse || Cures.Num() > 0; }
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
	/** Food that spoils (FWRSpoil): freshness 0..1 as it was at server time FreshAt, falling FreshRate a second (its
	 *  container's pace). No ticking: freshness now is worked out from these three. Food's Roll is its worth (a golden
	 *  crop's dish: 1.5), so stacks of a different worth don't merge. */
	UPROPERTY() float Fresh = 1.f;
	UPROPERTY() float FreshAt = 0.f;
	UPROPERTY() float FreshRate = 0.f;

	bool IsEmpty() const { return ItemId.IsNone() || Count <= 0; }
	static int32 SocketCount(EWRQuality Q) { return static_cast<int32>(Q); }
	static const TCHAR* QualityName(EWRQuality Q);
};

/** Pure slot logic (no world) so tests can cover stacking. */
struct WILDREACH_API FWRInventoryRules
{
	/** Adds as much as fits (fills existing stacks first). Returns how many didn't fit. */
	static int32 Add(TArray<FWRItemStack>& Slots, FName ItemId, int32 Count);
	/** The same, as food of this freshness and worth (stacks only join ones of the same worth; freshness averages). */
	static int32 AddFood(TArray<FWRItemStack>& Slots, FName ItemId, int32 Count, float Fresh, float Worth = 1.f);
	/** Moves up to N from a stack into a slot list, carrying its freshness, worth and (gear) the instance. Returns how many moved. */
	static int32 MoveFrom(FWRItemStack& From, TArray<FWRItemStack>& To, int32 N);
	/** Adds one prepared equipment instance. False if no free slot. */
	static bool AddInstance(TArray<FWRItemStack>& Slots, const FWRItemStack& Instance);
	/** Removes up to Count across stacks. Returns how many were removed. */
	static int32 Remove(TArray<FWRItemStack>& Slots, FName ItemId, int32 Count);
	static int32 CountOf(const TArray<FWRItemStack>& Slots, FName ItemId);
	/** Ammo of a kind (arrow, bolt, dart) to fire: the preferred one while it lasts, else the first in the bag. */
	static FName FindAmmo(const TArray<FWRItemStack>& Slots, FName Kind, FName Preferred);
	/** The next / previous kind of that ammo in the bag after Current (the mouse wheel). */
	static FName NextAmmo(const TArray<FWRItemStack>& Slots, FName Kind, FName Current, int32 Dir);
	static float TotalWeight(const TArray<FWRItemStack>& Slots);
	/** A stack in a save (its freshness kept as it is now; the instance fields for gear). */
	static TSharedRef<class FJsonObject> StackToJson(const FWRItemStack& S);
	static FWRItemStack StackFromJson(const TSharedPtr<class FJsonObject>& O);
	/** A fresh equipment instance with full durability. */
	static FWRItemStack MakeInstance(FName ItemId, EWRQuality Quality = EWRQuality::Common, float Roll = 1.f);
};

/**
 * Spoilage (cooks/build-farming.md 2.9; DST's freshness bar): timestamps, never a per-stack tick. A stack's freshness
 * is Fresh - (now - FreshAt) x FreshRate; the farm subsystem's 10 s sweep re-stamps a stack when its container's pace
 * changes (a bag, a chest, a tundra chest, an ice box with or without ice) and turns empty ones into Spoiled food.
 */
struct WILDREACH_API FWRSpoil
{
	/** Server seconds the rules stamp with (the farm subsystem keeps it current on the server). */
	static double ServerNow;
	/** The server's clock as this machine sees it (the farm subsystem keeps it current everywhere): what the UI reads. */
	static double DisplayNow;
	/** The server's clock as this machine knows it (clients: the game state's server time). */
	static double NowFor(const UObject* WorldContext);
	static bool Perishes(FName ItemId);
	/** 0..1 now (1 for food that never spoils and for non-food). */
	static float FreshOf(const FWRItemStack& S, double Now);
	/** Seconds until it's spoiled at its current pace (< 0: never). */
	static double SecondsLeft(const FWRItemStack& S, double Now);
	/** Its pace a second when kept at Mult (1 = a bag). */
	static float RateFor(FName ItemId, float Mult);
	/** Keep its freshness as it is now and go on at Mult's pace. */
	static void Restamp(FWRItemStack& S, double Now, float Mult);
	/** A new stack of food: Fresh at Now, a bag's pace. */
	static void StampNew(FWRItemStack& S, float Fresh, double Now);
	/** Re-stamps every perishable stack whose pace isn't Mult's and turns spoiled ones into Spoiled food. Returns how
	 *  many stacks it changed. */
	static int32 Sweep(TArray<FWRItemStack>& Slots, double Now, float Mult);
	/** Eating it now: x1, or x0.5 at food.staleBelow (25%) and under. */
	static float ValueMult(const FWRItemStack& S, double Now);
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
	/** Food of a given freshness and worth (a dish from a golden crop, stale catch...). */
	int32 AddFood(FName ItemId, int32 Count, float Fresh, float Worth = 1.f);
	bool AddInstance(const FWRItemStack& Instance);
	int32 RemoveItem(FName ItemId, int32 Count);
	/** Use the first usable item of a category ("potion", "food", "drink"). Returns a message. */
	FString UseCategory(FName Category);
	/** Server: a potion or cure starts going down (committed; the heal lands part way: combat.md 2.16). */
	FString StartDrink(const FWRItemDef& Def);
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
	const FWRItemStack& GetEquipped(EWREquipSlot Slot) const { return Slot == EWREquipSlot::Armor ? EquippedArmor : Slot == EWREquipSlot::Ranged ? EquippedRanged : Slot == EWREquipSlot::Light ? EquippedLight : EquippedWeapon; }
	/** Ammo of a kind to fire: Preferred if we still have it, else the first we carry. None if out. */
	FName FindAmmo(FName Kind, FName Preferred) const;
	/** The next (Dir +1) or previous (-1) ammo type of a kind we carry, after Current. */
	FName NextAmmo(FName Kind, FName Current, int32 Dir) const;
	/** Server: one thrown from the equipped spears / bolas; the next stack of them takes its place when it runs out. */
	void SpendThrowable();
	int32 CountOf(FName ItemId) const { return FWRInventoryRules::CountOf(Slots, ItemId); }
	float GetPotionCooldownRemaining() const;
	/** Everything carried: the bag plus what's worn and held (kg). */
	float GetCarryWeight() const;
	/** Sum of a gem stat over the equipped weapon's sockets ("damage", "critChance", ...). */
	float GemStat(FName Stat) const;
	/** Worn poise: the armour's (items.json equip.poise, while it holds) and socketed poise (COMBAT-AND-LESSONS 1.5). */
	float GetWornPoise() const;
	/** Worn status resistance (items.json equip.resist): added to the wearer's buildup threshold for that status. */
	float GetWornResist(FName Status) const;
	/** Equip load, kg: the worn armour, the weapon and ranged weapon in their slots and the carried shield (weapons.json
	 *  carried); not the bag (COMBAT-AND-LESSONS 1.7). Light weave lightens armour as it does in the bag. */
	float GetEquipLoadKg() const;
	/** What the equip load may reach before the roll gets heavier: defense.maxEquipLoad + the equipLoad stat. */
	float GetMaxEquipLoad() const;
	float GetArmorValue() const;
	/** The equipped bow / crossbow / blowgun's quality x roll (x0.5 worn out): scales its own damage and torpor. */
	float GetRangedMultiplier() const;

	static constexpr int32 NumSlots = 40;

	TSharedRef<class FJsonObject> ToJson() const;
	void FromJson(const TSharedPtr<class FJsonObject>& Json);
	bool bKitGiven = false;

private:
	AWRCharacterBase* GetPawnCharacter() const;

	UPROPERTY(Replicated) TArray<FWRItemStack> Slots;
	UPROPERTY(Replicated) FWRItemStack EquippedWeapon;
	UPROPERTY(Replicated) FWRItemStack EquippedArmor;
	UPROPERTY(Replicated) FWRItemStack EquippedRanged;
	UPROPERTY(Replicated) FWRItemStack EquippedLight;
	/** Server: UseSlot's stack, so UseCategory eats that one. */
	int32 PreferSlot = INDEX_NONE;
	/** Server time the potion belt is ready again (replicated so the HUD can show it). */
	UPROPERTY(Replicated) float PotionReadyAt = 0.f;
};
