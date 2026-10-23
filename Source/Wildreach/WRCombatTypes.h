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
	UPROPERTY() FVector Origin = FVector::ZeroVector;
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
