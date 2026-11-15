#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WRAbilities.h"
#include "WRAbilitiesWind.generated.h"

class UAbilityTask_ApplyRootMotionConstantForce;
class AWRWindTornado;

/**
 * The Wind path's abilities (cooks/biome-sky.md 2.18.3, combat.md 2.12-2.13). Momentum is a spender with no dead button:
 * every ability pays its spells.<key>.momentumCost when it goes off, and short of it goes off at half effect (half damage,
 * half push) for its stamina alone (WRWind::PayMomentum). Numbers: tuning spells.<key>, spells.shapes.<shape>,
 * wildbrands.<node>, knockback, passives.wind; the path's rules are in WRWindPath.h. Registered in
 * UWRAbilityRegistry::ClassForNode under "wind". All damage goes through UWRCombatLibrary::ApplyHit; every push through
 * WRWind::Knock (WRSkyAir::Push).
 */

/** A Wind spell: the shape's wind-up (rooted or walking at half speed), then Cast on the server at the paid scale. */
UCLASS(Abstract)
class WILDREACH_API UWRGA_WindSpell : public UWRGA_AreaSpell
{
	GENERATED_BODY()

public:
	UWRGA_WindSpell();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** Server: the spell goes off (after the wind-up). Scale: 1, or the half effect when Momentum was short. */
	virtual void CastWind(AWRCharacterBase* Caster, float Scale) {}
	/** Both machines at the cast moment (a self push the owner predicts). */
	virtual void CastBoth(AWRCharacterBase* Caster, float Scale) {}
	/** spells.shapes.<ShapeName> (bolt, nova, strike). */
	FString ShapeName = TEXT("bolt");
	FName CastSound = TEXT("wind.cast");
	/** Free (and full) while Sky lord hovers (Gust). */
	bool bFreeInSkyLord = false;
	/** Cast at once, no wind-up, while Whirlwind spins (Vortex). */
	bool bInstantWhileSpinning = false;
	/** Spinning while it winds up (Vortex: Eye of the storm counts the cast). */
	bool bSpinsWhileCasting = false;
	float Spell(const TCHAR* Field, float Fallback) const;
	FVector AimedGround(AWRCharacterBase* Caster, float Range) const;
	UPROPERTY() TObjectPtr<UAbilityTask_ApplyRootMotionConstantForce> SelfPush;
private:
	bool bSpinTagSet = false;
};

/** Gust: a 40-degree cone that throws enemies 600 cm (6 damage, stance x1.5, cd 4, 8 Momentum); airborne, you go 300 cm
 *  the other way. Free while Sky lord hovers. Firestorm burns, Sand blast blinds, Sea spray soaks what it throws. */
UCLASS() class WILDREACH_API UWRGA_Gust : public UWRGA_WindSpell
{
	GENERATED_BODY()
public:
	UWRGA_Gust();
protected:
	virtual void CastWind(AWRCharacterBase* Caster, float Scale) override;
	virtual void CastBoth(AWRCharacterBase* Caster, float Scale) override;
};

/** Vortex: pulls everything hostile within 600 cm 400 cm toward you (cd 14, 20 Momentum); cast while Whirlwind spins it
 *  comes out at once and pulls them into the spin. */
UCLASS() class WILDREACH_API UWRGA_Vortex : public UWRGA_WindSpell
{
	GENERATED_BODY()
public:
	UWRGA_Vortex();
protected:
	virtual void CastWind(AWRCharacterBase* Caster, float Scale) override;
};

/** Storm volley (Windblade capstone): drawn 1.0 s rooted with hyperarmor, 30 arrows rain over 600 cm where you aim from
 *  0.9 s after the release over 1.5 s (8 damage each, cd 50, 60 Momentum). Needs a bow or crossbow in the ranged slot. */
UCLASS() class WILDREACH_API UWRGA_StormVolley : public UWRGA_WindSpell
{
	GENERATED_BODY()
public:
	UWRGA_StormVolley();
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
protected:
	virtual void CastWind(AWRCharacterBase* Caster, float Scale) override;
};

/** Air dash: 400 cm through the air in 0.18 s, untouchable on the way, cd 3, 10 Momentum; once each airtime (a landing or
 *  an updraft gives it back). Jet stream (Lightning + Wind 50): right after a Bolt dash it's free and readies Bolt dash. */
UCLASS() class WILDREACH_API UWRGA_AirDash : public UWRGameplayAbility
{
	GENERATED_BODY()
public:
	UWRGA_AirDash();
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
private:
	void SetIFrames(bool bOn);
	bool bIFrames = false;
	UPROPERTY() TObjectPtr<UAbilityTask_ApplyRootMotionConstantForce> Travel;
};

/** Wind cut (Wildbrand, trunk): x1.1, stance x1.5, then a blade of wind 900 cm ahead at 2,000 cm/s (x0.8 of the light
 *  hit, knocks back). cd 2.5, 15 Momentum; free while Sky lord hovers. */
UCLASS() class WILDREACH_API UWRGA_WindCut : public UWRGA_Melee
{
	GENERATED_BODY()
public:
	UWRGA_WindCut();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};

/** Plunge (Wildbrand, Skyborne): airborne only. 0.20 s wind-up held in the air with hyperarmor, then a dive blade-first
 *  until you land: x1.5 on what you land on and a ring x0.8 over 300 cm, +0.2 a metre fallen (x2 at most); 0.45 s of
 *  recovery. cd 4, 20 Momentum. The dive's own landing never hurts you. Avalanche plunge (Wind + Frost 50) shatters the
 *  frozen in the ring and chills the rest. */
UCLASS() class WILDREACH_API UWRGA_Plunge : public UWRGameplayAbility
{
	GENERATED_BODY()
public:
	UWRGA_Plunge();
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
private:
	void DiveTick();
	void Land();
	float StartZ = 0.f;
	float Scale = 1.f;
	double DiveStartedAt = 0.0;
	bool bLanded = false;
	UPROPERTY() TObjectPtr<UAbilityTask_ApplyRootMotionConstantForce> Travel;
};

/** Whirlwind (Wildbrand, Cyclone; greatsword, scythe, axe, great hammer): 0.30 s, then 4 spins of 0.3 s hitting everything
 *  within 280 cm (x0.7 a spin, hyperarmor), 0.50 s recovery; held, one more spin per 8 Momentum. cd 8, 30 Momentum.
 *  Debris throws stones out of each spin; Lingering gale leaves a gale where you spun. */
UCLASS() class WILDREACH_API UWRGA_Whirlwind : public UWRGameplayAbility
{
	GENERATED_BODY()
public:
	UWRGA_Whirlwind();
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
private:
	UFUNCTION() void OnReleased(float TimeHeld);
	void Spin(int32 Index);
	void Finish();
	bool bHeld = true;
	bool bSpinning = false;
	int32 ExtraSpins = 0;
	float Scale = 1.f;
};

/** Wind arrow (Wildbrand, Windblade; bow, crossbow): draw 0.35 s, a shot x1.6 on the wind that knocks back and goes through
 *  one body (Piercing gale: two), 0.30 s after. cd 2.0, 15 Momentum, one arrow or bolt. Ice arrows freeze, Bleeding arrows
 *  bleed, Sunshot (Sun & sand + Wind 50) makes it a beam of light through everything in line. */
UCLASS() class WILDREACH_API UWRGA_WindArrow : public UWRGameplayAbility
{
	GENERATED_BODY()
public:
	UWRGA_WindArrow();
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
private:
	void Release();
};

/** Sky lord (Skyborne capstone): 1.0 s rooted with hyperarmor, then you rise to 6 m over the ground (or hold your height in
 *  the air) and hover 8 s at 600 cm/s (hold Jump to climb); light and heavy become air strikes, dodge is free, Gust and
 *  Wind cut cost nothing; no fall damage from its end. cd 90, 60 Momentum (short: 4 s). */
UCLASS() class WILDREACH_API UWRGA_SkyLord : public UWRGameplayAbility
{
	GENERATED_BODY()
public:
	UWRGA_SkyLord();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
private:
	void Hover();
	bool bHovering = false;
	float HoverZ = 0.f;
	float SavedFlySpeed = 0.f;
	float SavedBraking = 0.f;
	double EndsAt = 0.0;
};

/** Tornado (Cyclone capstone): a tornado where you aim that goes at 400 cm/s for 10 s (10 a second, radius 250): it lifts
 *  lowland creatures, drags midland ones in at 150 cm/s, only hurts bosses. Hold the slot to steer it at the crosshair;
 *  let go and it runs straight. cd 60, 50 Momentum (short: half its damage and time). */
UCLASS() class WILDREACH_API UWRGA_Tornado : public UWRGameplayAbility
{
	GENERATED_BODY()
public:
	UWRGA_Tornado();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
private:
	UFUNCTION() void OnReleased(float TimeHeld);
	/** It lives on while you hold to steer, until you let go or it blows out. */
	void PollHold();
	TWeakObjectPtr<AWRWindTornado> Spawned;
	bool bReleased = false;
};

/** A blade of wind (Wind cut): flies straight along the ground, never homing, and strikes each body once on its way. */
UCLASS(NotPlaceable)
class WILDREACH_API AWRWindWave : public AActor
{
	GENERATED_BODY()
public:
	AWRWindWave();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	static AWRWindWave* Launch(AWRCharacterBase* By, const FVector& From, const FVector& Dir, float Damage, float Stance, float Knock, float Speed, float Range, float Width);
	TWeakObjectPtr<AWRCharacterBase> By;
	FVector Dir = FVector::ForwardVector;
	float Damage = 10.f, Stance = 1.5f, Knock = 300.f, Speed = 2000.f, Range = 900.f, Width = 160.f, Travelled = 0.f;
private:
	TSet<TWeakObjectPtr<AActor>> Struck;
};

/** The tornado (Tornado): replicated so every machine sees the funnel go; the server moves it, steers it, lifts and drags. */
UCLASS(NotPlaceable)
class WILDREACH_API AWRWindTornado : public AActor
{
	GENERATED_BODY()
public:
	AWRWindTornado();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;
	TWeakObjectPtr<AWRCharacterBase> Caster;
	bool bSteering = true;
	float Speed = 400.f, Dps = 10.f, Radius = 250.f;
	FVector Heading = FVector::ForwardVector;
private:
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Funnel;
	UPROPERTY(Transient) TObjectPtr<class UAudioComponent> Roar;
	TMap<TWeakObjectPtr<AActor>, float> Lifted;
};

/** A patch of wind or fire on the ground (Lingering gale, Ember rain): server only; what it looks like is broadcast. */
UCLASS(NotPlaceable)
class WILDREACH_API AWRWindPatch : public AActor
{
	GENERATED_BODY()
public:
	AWRWindPatch();
	virtual void Tick(float DeltaSeconds) override;
	static AWRWindPatch* Lay(AWRCharacterBase* By, const FVector& At, float Radius, float Seconds, float Dps, float Push, float BurnPerSecond);
	TWeakObjectPtr<AWRCharacterBase> By;
	float Radius = 300.f, Dps = 5.f, Push = 100.f, BurnPerSecond = 0.f;
};
