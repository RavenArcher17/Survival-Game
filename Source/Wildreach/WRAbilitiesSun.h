#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WRAbilities.h"
#include "WRAbilitiesSun.generated.h"

class UAbilityTask_ApplyRootMotionConstantForce;

/**
 * The Sun & sand path's abilities (cooks/biome-desert.md 2.17; build-biome-desert.md 2.16). Radiance is the spender:
 * each pays spells.<key>.radianceCost as it goes off; a press you can't pay flashes the Radiance bar. Numbers: tuning
 * spells.<key>, spells.shapes.<shape>, wildbrands.<node>, sun.path.*; the path's rules are WRSunPath.h. Registered in
 * UWRAbilityRegistry::ClassForNode under "sun". Light is the element of the light spells (blind is their buildup), sand
 * of the storms.
 */

/** A Sun spell: Radiance, the shape's wind-up (rooted or walking), then CastSun on the server. */
UCLASS(Abstract)
class WILDREACH_API UWRGA_SunSpell : public UWRGameplayAbility
{
	GENERATED_BODY()

public:
	UWRGA_SunSpell();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;

protected:
	virtual void CastSun(AWRCharacterBase* Caster) {}
	virtual void TakeAim(AWRCharacterBase* Caster);
	FString SpellKey;
	FString ShapeName = TEXT("bolt");
	float Spell(const TCHAR* Field, float Fallback) const;
	float RadianceCost() const;
	FWRHit SunHit(AWRCharacterBase* Caster, float Damage, float Blind) const;
	/** The aimed point on the ground (the lock, the crosshair within Range, else Ahead cm in front). */
	FVector AimedGround(AWRCharacterBase* Caster, float Range, float Ahead) const;
	FVector AimDir = FVector::ForwardVector;
	TWeakObjectPtr<AWRCharacterBase> AimLock;
};

/** Sunbeam (trunk starter): a bolt of light 16, blind 30, 3,400 cm/s, 8 Radiance, cd 1.0; homes only on creatures. */
UCLASS() class WILDREACH_API UWRGA_Sunbeam : public UWRGA_SunSpell
{
	GENERATED_BODY()
public:
	UWRGA_Sunbeam();
protected:
	virtual void CastSun(AWRCharacterBase* Caster) override;
};

/** Solar flare (Radiant): a flash round you, radius 6 m: 10 and blind 80 to everything near. 30 Radiance, cd 14. */
UCLASS() class WILDREACH_API UWRGA_SolarFlare : public UWRGA_SunSpell
{
	GENERATED_BODY()
public:
	UWRGA_SolarFlare();
protected:
	virtual void CastSun(AWRCharacterBase* Caster) override;
};

/** Beacon (Radiant): a 5 m pool of light for 10 s at the aimed ground: 10 a second to undead and void creatures, a 10 m
 *  light, and it counts as a sunstone lamp for your Radiance at night. 25 Radiance, cd 20. */
UCLASS() class WILDREACH_API UWRGA_SunBeacon : public UWRGA_SunSpell
{
	GENERATED_BODY()
public:
	UWRGA_SunBeacon();
protected:
	virtual void CastSun(AWRCharacterBase* Caster) override;
};

/** Sunburst (Radiant capstone): a column of sunlight the whole party sees 1.0 s ahead at the aimed point: 70 and blind 100
 *  in 6 m. 70 Radiance, cd 50. */
UCLASS() class WILDREACH_API UWRGA_Sunburst : public UWRGA_SunSpell
{
	GENERATED_BODY()
public:
	UWRGA_Sunburst();
protected:
	virtual void CastSun(AWRCharacterBase* Caster) override;
};

/** Sandstorm (Dune): a storm 7 m round you for 8 s that moves with you: blind 10 a second, slow x0.8 (+Scour's damage,
 *  Shifting sands' radius; Solar storm's lightning). 30 Radiance, cd 20. */
UCLASS() class WILDREACH_API UWRGA_Sandstorm : public UWRGA_SunSpell
{
	GENERATED_BODY()
public:
	UWRGA_Sandstorm();
protected:
	virtual void CastSun(AWRCharacterBase* Caster) override;
};

/** Desert's wrath (Dune capstone): a 10 m vortex round you for 10 s pulling enemies in at 2 m/s, 14 a second (bosses not
 *  pulled). 80 Radiance, cd 60. */
UCLASS() class WILDREACH_API UWRGA_DesertsWrath : public UWRGA_SunSpell
{
	GENERATED_BODY()
public:
	UWRGA_DesertsWrath();
protected:
	virtual void CastSun(AWRCharacterBase* Caster) override;
};

/** Quicksand (Dune): a 3 m pool at the aimed ground for 6 s that turns any ground to sand: lowland and midland rooted 2 s,
 *  everything slowed x0.5 (Frozen sand: frost 30 a second on the stuck). 20 Radiance, cd 16. */
UCLASS() class WILDREACH_API UWRGA_Quicksand : public UWRGA_SunSpell
{
	GENERATED_BODY()
public:
	UWRGA_Quicksand();
protected:
	virtual void CastSun(AWRCharacterBase* Caster) override;
};

/** Burrow (Dune): dive under the sand and run 8 m beneath it, untouchable the whole way: a fin of sand cuts the ground over
 *  you, a burst where you come up. Sand only (any land's); greyed "needs sand" elsewhere. 10 Radiance, cd 4. */
UCLASS() class WILDREACH_API UWRGA_Burrow : public UWRGameplayAbility
{
	GENERATED_BODY()
public:
	UWRGA_Burrow();
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
private:
	void SetUnder(bool bOn);
	bool bUnder = false;
	UPROPERTY() TObjectPtr<UAbilityTask_ApplyRootMotionConstantForce> Travel;
};

/** Sun strike (trunk Wildbrand): x1.35, blind 40, stance x2, hyperarmor; 15 Radiance. */
UCLASS() class WILDREACH_API UWRGA_SunStrike : public UWRGA_Melee
{
	GENERATED_BODY()
public:
	UWRGA_SunStrike();
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
protected:
	float RadianceCost = 15.f;
	FName Fx = TEXT("sun.strike");
};

/** Blazing arc (Solar blade): a wide sun-infused sweep, 220 degrees and 3.8 m: x1.6, blind 50, stance x2.5; 25 Radiance. */
UCLASS() class WILDREACH_API UWRGA_BlazingArc : public UWRGA_SunStrike
{
	GENERATED_BODY()
public:
	UWRGA_BlazingArc();
};

/** Noon strike (Solar blade capstone): an overhead blow, x2.2, blind 80, stance x6, and a column of sunlight falls on the
 *  struck point 0.9 s later (50 more); 60 Radiance. */
UCLASS() class WILDREACH_API UWRGA_NoonStrike : public UWRGA_SunStrike
{
	GENERATED_BODY()
public:
	UWRGA_NoonStrike();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};

/**
 * A Sun field (Beacon, Sandstorm, Desert's wrath, Quicksand): a replicated actor whose look every machine draws from its
 * kind and radius; the server runs its effect every 0.25 s (all damage through ApplyHit). The storms ride on their
 * caster.
 */
UCLASS(NotPlaceable)
class WILDREACH_API AWRSunField : public AActor
{
	GENERATED_BODY()

public:
	AWRSunField();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	UPROPERTY(Replicated) FName Kind;        // beacon | sandstorm | wrath | quicksand
	UPROPERTY(Replicated) float Radius = 500.f;
	UPROPERTY(Replicated) float EndAt = 0.f;
	UPROPERTY(Replicated) TObjectPtr<class AWRCharacterBase> Caster;
	/** The fields of this kind a caster has out (a Beacon's light, the storms' Solar storm). */
	static TArray<AWRSunField*> Of(const AActor* InCaster, FName InKind);

private:
	void Pulse();
	float PulseClock = 0.f, StrikeClock = 0.f;
	TSet<TWeakObjectPtr<AActor>> Rooted;
	UPROPERTY(Transient) TObjectPtr<class UPointLightComponent> Light;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Look;
};

/** A column of sunlight (Sunburst, Noon strike): its gold shaft shows on every machine the moment it's placed (the tell),
 *  and Delay later the server strikes everything within Radius. */
UCLASS(NotPlaceable)
class WILDREACH_API AWRSunColumn : public AActor
{
	GENERATED_BODY()

public:
	AWRSunColumn();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	/** Server: place one. */
	static AWRSunColumn* Drop(AWRCharacterBase* InCaster, const FVector& At, float InRadius, float InDelay, float InDamage, float InBlind, float InStance, FName InNode);

	UPROPERTY(Replicated) float Radius = 600.f;
	UPROPERTY(Replicated) float StrikeAt = 0.f;
	TWeakObjectPtr<AWRCharacterBase> Caster;
	float Damage = 0.f, Blind = 0.f, Stance = 0.f;
	FName Node;
	bool bStruck = false;
};
