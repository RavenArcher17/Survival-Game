#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WRAbilities.h"
#include "WRAbilitiesLightning.generated.h"

class UAbilityTask_ApplyRootMotionConstantForce;

/**
 * The Lightning path's abilities (cooks/biome-lightning.md 2.18.3, combat.md 2.12-2.13). Numbers: tuning spells.<key>,
 * spells.shapes.<shape>, wildbrands.<node>; the path's rules (Charge, procs, Grounded) are in WRLightningPath.h.
 * Registered in UWRAbilityRegistry::ClassForNode under "lightning".
 */

/** A lightning spell: the shape's wind-up (rooted or walking at half speed), then Cast on the server, Charge gained. */
UCLASS(Abstract)
class WILDREACH_API UWRGA_LightningSpell : public UWRGA_AreaSpell
{
	GENERATED_BODY()

public:
	UWRGA_LightningSpell();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	/** Server: the spell goes off (after the wind-up). */
	virtual void CastLightning(AWRCharacterBase* Caster) {}
	/** spells.shapes.<ShapeName> (bolt, chain, nova, pool, storm, buff). */
	FString ShapeName = TEXT("bolt");
	/** The hand glow while casting and its sound. */
	FName CastSound = TEXT("storm.charge_whine");
	/** Num(spells.<key>.<Field>). */
	float Spell(const TCHAR* Field, float Fallback) const;
	/** Where a ground spell lands: under the lock (clamped to Range), else where the crosshair meets the ground. */
	FVector AimedGround(AWRCharacterBase* Caster, float Range) const;
};

/** Spark: the starter bolt (16 damage, shock 35, 3,200 cm/s). Never curves at another player (lockHomingPlayers 0). */
UCLASS() class WILDREACH_API UWRGA_Spark : public UWRGA_LightningSpell
{
	GENERATED_BODY()
public:
	UWRGA_Spark();
protected:
	virtual void CastLightning(AWRCharacterBase* Caster) override;
};

/** Chain lightning: hits the target, then jumps to the nearest unhit enemy within 800 cm (2 jumps + Arc ranks). */
UCLASS() class WILDREACH_API UWRGA_ChainLightning : public UWRGA_LightningSpell
{
	GENERATED_BODY()
public:
	UWRGA_ChainLightning();
protected:
	virtual void CastLightning(AWRCharacterBase* Caster) override;
};

/** Thunderclap: a rooted nova (radius 450, shock 60, stance x2). */
UCLASS() class WILDREACH_API UWRGA_Thunderclap : public UWRGA_LightningSpell
{
	GENERATED_BODY()
public:
	UWRGA_Thunderclap();
protected:
	virtual void CastLightning(AWRCharacterBase* Caster) override;
};

/** Static field: a pool of static where you aim (radius 400, 6 dps, shock 15/s, 8 s; Field strength grows it). */
UCLASS() class WILDREACH_API UWRGA_StaticField : public UWRGA_LightningSpell
{
	GENERATED_BODY()
public:
	UWRGA_StaticField();
protected:
	virtual void CastLightning(AWRCharacterBase* Caster) override;
};

/** Tempest (capstone): a storm round you (radius 900, 10 s): a strike every 0.8 s on a random enemy inside. */
UCLASS() class WILDREACH_API UWRGA_Tempest : public UWRGA_LightningSpell
{
	GENERATED_BODY()
public:
	UWRGA_Tempest();
protected:
	virtual void CastLightning(AWRCharacterBase* Caster) override;
};

/** Thunder god (capstone): a self buff (12 s); every weapon hit calls a bolt down on the target (WRLightning::AfterHit). */
UCLASS() class WILDREACH_API UWRGA_ThunderGod : public UWRGA_LightningSpell
{
	GENERATED_BODY()
public:
	UWRGA_ThunderGod();
protected:
	virtual void CastLightning(AWRCharacterBase* Caster) override;
};

/** Static strike (Wildbrand): a thrust that arcs to one more enemy within 4 m at 40% (Arc: +1 a rank). +15 Charge. */
UCLASS() class WILDREACH_API UWRGA_StaticStrike : public UWRGA_Melee
{
	GENERATED_BODY()
public:
	UWRGA_StaticStrike();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};

/**
 * Charged weapon (Wildbrand): spends all Charge (at least 30) in one overhead blow, damage x (1 + 1.5 x Charge%), shock
 * 30 + 50 x Charge%, hyperarmor. Held, it stores more first (+20 Charge a second, up to 1 s).
 */
UCLASS() class WILDREACH_API UWRGA_ChargedWeapon : public UWRGA_Melee
{
	GENERATED_BODY()
public:
	UWRGA_ChargedWeapon();
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
private:
	void HoldTick();
	void Release();
	bool bReleased = false;
};

/** Bolt dash: 600 cm in 0.18 s through enemies (12 damage, shock 40 to each passed), i-frames, spends 20 Charge. */
UCLASS() class WILDREACH_API UWRGA_BoltDash : public UWRGameplayAbility
{
	GENERATED_BODY()
public:
	UWRGA_BoltDash();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
protected:
	/** One dash; bSpendCharge: this one takes the Charge (Lightning step: only the first of the set). */
	void Dash(bool bSpendCharge);
	FString Key = TEXT("spells.boltDash");
private:
	void SetPassThrough(bool bOn);
	void SetIFrames(bool bOn);
	void Strike(const FVector& From, const FVector& To);
	bool bPassing = false;
	bool bIFrames = false;
	ECollisionResponse WasPawnResponse = ECR_Block;
	FVector DashFrom = FVector::ZeroVector;
	FVector DashDir = FVector::ForwardVector;
	UPROPERTY() TObjectPtr<UAbilityTask_ApplyRootMotionConstantForce> Travel;
};

/** Lightning step (capstone): Bolt dash up to 3 times within 2.0 s; the second and third cost no Charge; 12 s for the set. */
UCLASS() class WILDREACH_API UWRGA_LightningStep : public UWRGA_BoltDash
{
	GENERATED_BODY()
public:
	UWRGA_LightningStep();
	virtual bool CheckCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
private:
	/** Inside a set: started within the window and dashes left. */
	bool InSet() const;
	double SetStartedAt = -100.0;
	int32 StepsUsed = 0;
};

/**
 * Server-side lightning on the ground: Static field (damage and shock to the enemies standing in it) or Tempest (moves
 * with its caster and calls a strike on a random enemy inside every few tenths of a second). What players see is
 * broadcast by the ability (WRFx), so this actor doesn't replicate.
 */
UCLASS()
class WILDREACH_API AWRLightningField : public AActor
{
	GENERATED_BODY()

public:
	AWRLightningField();
	virtual void Tick(float DeltaSeconds) override;

	static AWRLightningField* SpawnField(AWRCharacterBase* Caster, const FVector& At, float Radius, float Seconds, float Dps, float ShockPerSecond);
	static AWRLightningField* SpawnTempest(AWRCharacterBase* Caster, float Radius, float Seconds, float Every, float Damage, float Shock, float Tell, float StrikeRadius);

	TWeakObjectPtr<AWRCharacterBase> Caster;
	bool bTempest = false;
	float Radius = 400.f;
	float Dps = 6.f;
	float ShockPerSecond = 15.f;
	float Every = 0.8f;
	float StrikeDamage = 30.f;
	float StrikeShock = 40.f;
	float Tell = 1.f;
	float StrikeRadius = 200.f;

private:
	float Clock = 0.f;
};
