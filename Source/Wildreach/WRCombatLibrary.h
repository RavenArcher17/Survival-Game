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
	/**
	 * Server: resolve a hit on Target. Returns what happened. A creature's or boss's blow on a remote player that isn't
	 * defended right now is held for that player's lag (UWRLagSubsystem) and returns Ignored at once; pass OnResolved to
	 * act on the real result (it's called with it in every case, held or not).
	 */
	static EWRHitResult ApplyHit(AWRCharacterBase* Target, const FWRHit& Hit, TFunction<void(EWRHitResult)> OnResolved = nullptr);

	/**
	 * Where to aim so a shot leaving From at Speed meets a target moving at Velocity. A target that can't be caught gets
	 * the point it'll have reached by the time a shot gets to where it is now.
	 */
	static FVector LeadPoint(const FVector& From, const FVector& Target, const FVector& Velocity, float Speed);

	/** Weapon damage from the sword table: base x metal x quality x (1 + upgrade * per-level). */
	static float WeaponDamage(const FWRWeaponStats& Weapon, bool bHeavy);

	/** Multiplier from the attacker's Heat on fire damage (0..heatDamageBonusAtFull). */
	static float HeatBonus(float Heat, float MaxHeat);

	/** Stamina a blocked blow costs: its stamina damage x (1 - guard boost / 100) (COMBAT-AND-LESSONS 1.2). */
	static float GuardStaminaCost(float StaminaDamage, float GuardBoost);

	/** Server: break a creature's stance (or open a parried / guard-broken fighter) for Seconds: the crit opens. */
	static void BreakStance(AWRCharacterBase* Who, float Seconds, AActor* By);
	/** The stance a creature or boss has at full (creatures.json stance, else its tier: tuning stance.tiers). */
	static float MaxStanceFor(const AWRCharacterBase* Who);
	/** How long its stance stays broken (tuning stance.breakSeconds by tier). */
	static float StanceBreakSeconds(const AWRCharacterBase* Who);
	/** Server: a survivor's poise at full: player.poise + Tempered (path stat maxPoise) + Reinforced (perk armorPoise) +
	 *  the worn armour and socketed poise. Called when armour or nodes change. */
	static void RefreshPoise(AWRCharacterBase* Who);
	/** "normal", "mini" or "major": what parries and stance count it as. */
	static FName StanceKind(const AWRCharacterBase* Who);

	/** Is Attacker inside Defender's block cone? */
	static bool IsInFront(const AActor* Defender, const FVector& AttackOrigin, float HalfAngleDegrees);

	/** Server: add Heat or Chill to a character, applying the Balance combo. */
	static void GainResource(AWRCharacterBase* Who, const FGameplayTag& Element, float Amount);

	/** Server: spend all of a character's Chill; returns the fraction of max spent (0..1). Balance turns some into Heat. */
	static float SpendAllChill(AWRCharacterBase* Who);

	static const TCHAR* ResultName(EWRHitResult Result);
};
