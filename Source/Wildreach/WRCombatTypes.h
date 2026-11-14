#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "WRCombatTypes.generated.h"

UENUM(BlueprintType)
enum class EWRHitResult : uint8
{
	Hit,
	Blocked,
	GuardBroken,
	Parried,
	Dodged,
	Ignored,
};

/** How a landed blow feels (tuning fx.hitStop / weight / flash by tier). */
UENUM(BlueprintType)
enum class EWRHitTier : uint8
{
	Light,
	Heavy,
	Counter,   // charged heavies, guard counters
	Crit,      // ripostes, backstabs, stance crits
	BossKill,
};

/** Ability input ids (bound to GAS specs so held abilities like Block can wait for release). */
UENUM(BlueprintType)
enum class EWRAbilityInput : uint8
{
	None,
	Light,
	Heavy,
	Dodge,
	Block,
	Slot1, Slot2, Slot3, Slot4, Slot5, Slot6, Slot7, Slot8,
};

/** One instance of damage, built by an ability/projectile/status and resolved by UWRCombatLibrary on the server. */
USTRUCT(BlueprintType)
struct FWRHit
{
	GENERATED_BODY()

	UPROPERTY() TObjectPtr<AActor> Instigator = nullptr;
	UPROPERTY() float Damage = 0.f;
	UPROPERTY() float PoiseDamage = 0.f;
	UPROPERTY() FGameplayTag Element;
	UPROPERTY() float BurnBuildup = 0.f;
	UPROPERTY() float FrostBuildup = 0.f;
	/** Path that produced the hit (awards that path XP); empty for weapon attacks without a path. */
	UPROPERTY() FName PathId;
	UPROPERTY() bool bMelee = false;
	UPROPERTY() bool bHeavy = false;
	/** Path weapon art (Flame slash, Ice slash...): Heated blade boosts these. */
	UPROPERTY() bool bWeaponArt = false;
	/** Status ticks (burn) skip block/parry/riposte/resource gain. */
	UPROPERTY() bool bDamageOverTime = false;
	/** Extra knockout torpor (tranq arrows, bolts, darts). */
	UPROPERTY() float Torpor = 0.f;
	UPROPERTY() FVector Origin = FVector::ZeroVector;

	// --- Combat core (cooks/build-combat-core.md 3.2) ---
	/** Stance damage to creatures and bosses (< 0: worked out from the damage by the combat library). */
	UPROPERTY() float StanceDamage = -1.f;
	/** What blocking it costs before the guard boost (< 0: worked out from the damage). */
	UPROPERTY() float StaminaDamage = -1.f;
	/** How hard it hits a player whose poise it breaks: 1 flinch, 2 stagger, 3 heavy stagger (tuning poise.reactions). */
	UPROPERTY() uint8 Impact = 2;
	/** How it feels when it lands (hit-stop, flash, shake by tier). */
	UPROPERTY() EWRHitTier Tier = EWRHitTier::Light;
	/** Only these can be parried (creature / boss rows, player melee). */
	UPROPERTY() bool bParryable = false;
	/** Passes under a jump (ground waves, tail sweeps, stomps). */
	UPROPERTY() bool bLow = false;
	/** Goes through any guard. */
	UPROPERTY() bool bUnblockable = false;
	/** A grab: ignores hyperarmor and guards. */
	UPROPERTY() bool bGrab = false;
	UPROPERTY() bool bIgnoreHyperarmor = false;
	/** A critical attack (riposte, backstab, stance crit): already multiplied, never parried. */
	UPROPERTY() bool bCritical = false;
	/** A projectile: never parried. */
	UPROPERTY() bool bProjectile = false;
	/** Server time the blow is judged at (0 = now): a deferred blow on a remote player is judged when it saw it. */
	double JudgeTime = 0.0;
};

/** Weapon the character currently swings. Damage = base x metal x quality x (1 + upgrade x 5%). */
USTRUCT(BlueprintType)
struct FWRWeaponStats
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere) FName Metal = TEXT("copper");
	UPROPERTY(EditAnywhere) FName Quality = TEXT("common");
	UPROPERTY(EditAnywhere) int32 Upgrade = 0;
	/** Creatures use a flat bite damage instead of the sword table (0 = use the sword table). */
	UPROPERTY(EditAnywhere) float FlatDamage = 0.f;
	/** Per-instance stat roll within the quality (~0.9..1.1). */
	UPROPERTY() float Roll = 1.f;
	/** Socketed gem bonuses. */
	UPROPERTY() float GemDamage = 0.f;
	UPROPERTY() float GemCrit = 0.f;
	/** Durability hit zero: half damage until repaired. */
	UPROPERTY() bool bBroken = false;
	/** Path material infused at craft time ("fire"/"frost"): plain attacks carry that element. */
	UPROPERTY() FName Element;
	/** Boss parts: Giant's ember (stagger -> lava shockwave), Wyrm scale (blocks can freeze the attacker). */
	UPROPERTY() bool bStaggerShockwave = false;
	UPROPERTY() bool bBlockFreeze = false;
};
