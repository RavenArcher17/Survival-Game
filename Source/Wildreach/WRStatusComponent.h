#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "WRStatusComponent.generated.h"

class AWRCharacterBase;

/**
 * Every status in the game (COMBAT-AND-LESSONS 1.12, cooks/combat.md 2.11, cooks/build-combat-full.md 2.9). Each has a
 * buildup meter that procs at the target's threshold (status.thresholds by tier, x status.procGrowth after each proc),
 * decays when it isn't being hit, and a proc effect (status.defs.<id>). The order is the replicated meter order.
 */
namespace WRStatus
{
	enum EId : uint8 { Burn, Frost, Bleed, Poison, Rot, Shock, Soak, Blind, Decay, Mark, Curse, Count };
	WILDREACH_API FName Name(int32 Id);
	/** Count if unknown. "frostbite" is frost. */
	WILDREACH_API int32 FromName(FName Name);
	/** The meter's colour on bars (status.defs.<id>.color, sRGB hex). */
	WILDREACH_API FLinearColor Color(int32 Id);
	/** The status a path element builds by default (fire -> burn, frost -> frost); Count for none. */
	WILDREACH_API int32 ForElement(const FGameplayTag& Element);
}

/**
 * Server-side upkeep for one character: status effects and their timers, buildup meters,
 * stamina/poise regen, Heat/Chill decay, hunger and thirst. Statuses are replicated loose
 * gameplay tags on the owner's ASC, so every client can read them; the meters replicate as
 * percentages on the character (AWRCharacterBase::BuildupPct) for every machine's bars.
 */
UCLASS(ClassGroup=(Combat))
class WILDREACH_API UWRStatusComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWRStatusComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Players need food and water; creatures don't. */
	UPROPERTY(EditAnywhere, Category="Status") bool bSurvivalNeeds = false;

	// --- Server API ---
	void ApplyStatus(const FGameplayTag& Status, float Duration);
	void ClearStatus(const FGameplayTag& Status);
	bool HasStatus(const FGameplayTag& Status) const;
	void Stagger(float Duration);

	/** Burn / frost buildup from a hit (the element's status). A full meter triggers the status. */
	void AddBuildup(const FGameplayTag& Element, float Amount, AWRCharacterBase* Source);
	/** Any status's buildup (WRStatus::EId). HitDamage: the blow that carried it (shock's chain). */
	void AddStatusBuildup(int32 Id, float Amount, AWRCharacterBase* Source, float HitDamage = 0.f);
	/** The buildup that procs Id on this character right now (tier, procs so far, boss participants, worn resist). */
	float Threshold(int32 Id) const;
	/** Ends a status and empties its meter (cures, fire on frostbite). */
	void CureStatus(int32 Id);

	/** Players: a button press while Frozen breaks free sooner (status.defs.frost.mashSeconds per press). */
	void MashFrozen();

	void NotifyStaminaUsed() { SinceStaminaUse = 0.f; }
	void NotifyPoiseDamaged() { SincePoiseDamage = 0.f; }
	/** A stance hit: its regen waits max stance / stance.regenDelayDivisor s from now. */
	void NotifyStanceDamaged() { SinceStanceDamage = 0.f; }
	void NotifyResourceGained(const FGameplayTag& Element);
	/** Any blow taken (keeps the fight going: proc growth and Decay last until combat has been over a while). */
	void NotifyHurt() { SinceHurt = 0.f; }

	/** Set every tick by the owner while it is sprinting and moving. */
	void SetSprintDraining(bool bDraining) { bSprintDraining = bDraining; }

	/** Health drains once hunger or thirst has sat at zero for the grace period. */
	bool IsDeprived() const;

	/** Healing taken right now (rot, decay halve it). */
	float HealingTakenScale() const;

	/** Clears every status and meter (used on respawn); gives back health lost to Decay. */
	void ResetAll();

private:
	AWRCharacterBase* GetOwnerCharacter() const;
	void TickBuildup(float DeltaTime);
	void TickStatuses(float DeltaTime);
	void TickRegen(float DeltaTime);
	void TickSurvival(float DeltaTime);
	void Proc(int32 Id, AWRCharacterBase* Source, float HitDamage);
	void SetMeter(int32 Id, float Value);
	float GetMeter(int32 Id) const;
	void PublishMeters();
	/** A damage-over-time or burst hit of a status (no guard, no parry). */
	void StatusHit(AWRCharacterBase* Source, float Damage, const FGameplayTag& Element);
	void RestoreDecay();

	/** Status tag -> world time it ends. */
	TMap<FGameplayTag, double> StatusEnds;

	TWeakObjectPtr<AWRCharacterBase> BurnSource;
	TWeakObjectPtr<AWRCharacterBase> PoisonSource;
	TWeakObjectPtr<AWRCharacterBase> RotSource;
	float BurnTickAccumulator = 0.f;
	float PoisonTick = 0.f;
	float RotTick = 0.f;
	float SoakDryFor = 0.f;
	/** Max health taken by Decay, given back when the fight is over (creatures) or on rest / respawn (players). */
	float DecayLost = 0.f;

	/** Meters other than burn / frost (those live in their attributes for Raven's readers). */
	float Meters[WRStatus::Count] = {};
	int32 Procs[WRStatus::Count] = {};

	float SinceStaminaUse = 10.f;
	float SincePoiseDamage = 10.f;
	float SinceStanceDamage = 100.f;
	float SinceHurt = 100.f;
	bool bResetting = false;
	float SinceHeatGain = 10.f;
	float SinceChillGain = 10.f;
	float SinceBuildup = 10.f;
	float HungerEmptyFor = 0.f;
	float ThirstEmptyFor = 0.f;
	float PublishClock = 0.f;
	bool bSprintDraining = false;
};
