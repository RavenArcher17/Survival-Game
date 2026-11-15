#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WRAbilities.h"
#include "WRAbilitiesNature.generated.h"

class AWRPlantSummon;

/**
 * The Nature path's abilities (cooks/biome-forest.md 2.17, combat.md 2.12-2.13). Growth is a spender: every ability pays
 * its spells.<key>.growthCost when it goes off (refused, the gauge flashes). Numbers: tuning spells.<key>,
 * spells.shapes.<shape>, wildbrands.<node>; the path's rules (Growth, Rooted, Entangled, procs) are in WRNaturePath.h,
 * the plants in WRPlantSummon.h. Registered in UWRAbilityRegistry::ClassForNode under "nature".
 */

/** A Nature spell: the shape's wind-up (rooted or walking at half speed), then Cast on the server. */
UCLASS(Abstract)
class WILDREACH_API UWRGA_NatureSpell : public UWRGA_AreaSpell
{
	GENERATED_BODY()

public:
	UWRGA_NatureSpell();
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	/** Server: the spell goes off (after the wind-up). */
	virtual void CastNature(AWRCharacterBase* Caster) {}
	/** spells.shapes.<ShapeName> (bolt, pool, ally, summon, target). */
	FString ShapeName = TEXT("bolt");
	/** Num(spells.<key>.<Field>). */
	float Spell(const TCHAR* Field, float Fallback) const;
	/** Where a ground spell lands: under the lock (clamped to Range), else where the crosshair meets the ground. */
	FVector AimedGround(AWRCharacterBase* Caster, float Range) const;
	/** The enemy the spell goes for: the lock in Range, else the one nearest the crosshair in the cone, in sight. */
	AWRCharacterBase* AimedEnemy(AWRCharacterBase* Caster, float Range, float ConeDegrees) const;
};

/** Thorn shot: the starter bolt (15 damage, bleed 20, 2,400 cm/s, cd 1.0, 6 Growth): a real thorn, never homing. */
UCLASS() class WILDREACH_API UWRGA_ThornShot : public UWRGA_NatureSpell
{
	GENERATED_BODY()
public:
	UWRGA_ThornShot();
protected:
	virtual void CastNature(AWRCharacterBase* Caster) override;
};

/** Regrowth: 30 health over 10 s on the ally in your aim (yourself with none), cd 8, 15 Growth. */
UCLASS() class WILDREACH_API UWRGA_Regrowth : public UWRGA_NatureSpell
{
	GENERATED_BODY()
public:
	UWRGA_Regrowth();
protected:
	virtual void CastNature(AWRCharacterBase* Caster) override;
};

/** Sanctuary: a pool (radius 450) that heals allies inside 4 a second for 10 s, cd 30, 30 Growth. */
UCLASS() class WILDREACH_API UWRGA_Sanctuary : public UWRGA_NatureSpell
{
	GENERATED_BODY()
public:
	UWRGA_Sanctuary();
protected:
	virtual void CastNature(AWRCharacterBase* Caster) override;
};

/** Root snare: a pool (radius 250, Briar spread) that Entangles enemies 2.5 s (bosses: slow x0.7 3 s), cd 14, 20 Growth. */
UCLASS() class WILDREACH_API UWRGA_RootSnare : public UWRGA_NatureSpell
{
	GENERATED_BODY()
public:
	UWRGA_RootSnare();
protected:
	virtual void CastNature(AWRCharacterBase* Caster) override;
};

/** Strangling vines (Thorn capstone): vines hold the target 4 s and crush it 12 a second; a major boss isn't held but
 *  takes the crush, stance 10 a second and a slow x0.6. cd 60, 60 Growth. */
UCLASS() class WILDREACH_API UWRGA_StranglingVines : public UWRGA_NatureSpell
{
	GENERATED_BODY()
public:
	UWRGA_StranglingVines();
protected:
	virtual void CastNature(AWRCharacterBase* Caster) override;
};

/** A summon spell: plants its kind where you aim (within spells.<key>.plantRange). */
UCLASS(Abstract) class WILDREACH_API UWRGA_PlantSpell : public UWRGA_NatureSpell
{
	GENERATED_BODY()
protected:
	virtual void CastNature(AWRCharacterBase* Caster) override;
	/** The plant it puts down (a class, set in the constructor). */
	UClass* PlantClass = nullptr;
};

/** Sprout turret (Bloom): a thorn-spitting plant within 800 cm, 60 health, 20 s, cd 24, 25 Growth. */
UCLASS() class WILDREACH_API UWRGA_SproutTurret : public UWRGA_PlantSpell { GENERATED_BODY() public: UWRGA_SproutTurret(); };
/** World tree (Grove capstone): a healing, shielding tree, 15 s, cd 90, 50 Growth. */
UCLASS() class WILDREACH_API UWRGA_WorldTree : public UWRGA_PlantSpell { GENERATED_BODY() public: UWRGA_WorldTree(); };
/** Ancient guardian (Bloom capstone): a walking tree, 400 health, 30 s, cd 120, 60 Growth. */
UCLASS() class WILDREACH_API UWRGA_AncientGuardian : public UWRGA_PlantSpell { GENERATED_BODY() public: UWRGA_AncientGuardian(); };

/** Briar strike (Wildbrand, trunk): x1.3, bleed 30, stance x1.5, roots a lowland creature 0.6 s. cd 3.0, 15 Growth. */
UCLASS() class WILDREACH_API UWRGA_BriarStrike : public UWRGA_Melee
{
	GENERATED_BODY()
public:
	UWRGA_BriarStrike();
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};

/**
 * Vine lash (Wildbrand, Thorn): a whip of thorned vine from the weapon, reach 700 cm, x1.2, bleed 25 (cd 5, 20 Growth).
 * Pressed again within 0.6 s of the lash landing, it pulls the body it caught 300 cm to you (never a boss), free.
 */
UCLASS() class WILDREACH_API UWRGA_VineLash : public UWRGameplayAbility
{
	GENERATED_BODY()
public:
	UWRGA_VineLash();
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual bool CheckCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
private:
	/** The follow-up window is open (the same clock on the owner and the server: the lash's timing, not who it caught). */
	bool FollowUpReady() const;
	void Lash(AWRCharacterBase* Caster);
	void Pull(AWRCharacterBase* Caster);
	double FollowFrom = -1.0, FollowUntil = -1.0;
	bool bThisFollowUp = false;
	/** Server: the body the last lash caught first. */
	TWeakObjectPtr<AWRCharacterBase> Caught;
};

/**
 * Server-side Nature on the ground: Sanctuary's healing pool, Root snare's grasping roots, Regrowth's heal riding on a
 * body, Strangling vines' crush on one. What players see is broadcast (WRFx) by it, so it doesn't replicate.
 */
UCLASS()
class WILDREACH_API AWRNatureField : public AActor
{
	GENERATED_BODY()

public:
	AWRNatureField();
	virtual void Tick(float DeltaSeconds) override;

	enum class EKind : uint8 { Sanctuary, Snare, Regrowth, Strangle };
	static AWRNatureField* Spawn(EKind Kind, AWRCharacterBase* Caster, const FVector& At, AWRCharacterBase* On, float Radius, float Seconds);

	EKind Kind = EKind::Sanctuary;
	TWeakObjectPtr<AWRCharacterBase> Caster;
	/** Regrowth's ally, Strangling vines' victim: the field rides on it. */
	TWeakObjectPtr<AWRCharacterBase> On;
	float Radius = 450.f;
	float Seconds = 10.f;
	/** Health a second (Sanctuary, Regrowth) or damage a second (Strangle). */
	float PerSecond = 4.f;
	/** Strangle on a major boss: stance a second, and the slow. */
	float StancePerSecond = 0.f;
	bool bBossCrush = false;
	/** Snare: hold time and the boss slow. */
	float HoldSeconds = 2.5f;
	float BossSlow = 0.7f, BossSlowSeconds = 3.f;

private:
	float Clock = 0.f;
	float Age = 0.f;
	TSet<TWeakObjectPtr<AWRCharacterBase>> Done;
};
