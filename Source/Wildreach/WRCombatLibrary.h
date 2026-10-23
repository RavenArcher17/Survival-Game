#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WRCombatTypes.h"
#include "WRCombatLibrary.generated.h"

class AWRCharacterBase;

/**
 * The single damage pipeline. Every source (weapons, spells, burns, starvation, hazards)
 * resolves here, on the server only. Pure math lives in static helpers so tests can hit it
 * without a world.
 */
UCLASS()
class WILDREACH_API UWRCombatLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Server: resolve a hit on Target. Returns what happened. */
	static EWRHitResult ApplyHit(AWRCharacterBase* Target, const FWRHit& Hit);

	/** Weapon damage from the sword table: base x metal x quality x (1 + upgrade * per-level). */
	static float WeaponDamage(const FWRWeaponStats& Weapon, bool bHeavy);

	/** Multiplier from the attacker's Heat on fire damage (0..heatDamageBonusAtFull). */
	static float HeatBonus(float Heat, float MaxHeat);

	/** Stamina a block costs for a hit of this size (blockMin..blockMax). */
	static float BlockStaminaCost(float Damage);

	/** Is Attacker inside Defender's block cone? */
	static bool IsInFront(const AActor* Defender, const FVector& AttackOrigin, float HalfAngleDegrees);

	/** Server: add Heat or Chill to a character, applying the Balance combo. */
	static void GainResource(AWRCharacterBase* Who, const FGameplayTag& Element, float Amount);

	/** Server: spend all of a character's Chill; returns the fraction of max spent (0..1). Balance turns some into Heat. */
	static float SpendAllChill(AWRCharacterBase* Who);

	static const TCHAR* ResultName(EWRHitResult Result);
};
