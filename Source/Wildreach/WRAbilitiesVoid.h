#pragma once

// The Void path's abilities (cooks/biome-void.md 2.17.2; rules in WRVoidPath): Void bolt, Null strike, Void rift,
// Collapse, Black hole, Blink, Phase, Shadow step, Nowhere, Consume, Sacrifice, Maw of the void. Each pays through
// WRVoidPath (its Void into the bar; past your tolerance its max HP into the band; the self-cost limit refuses it).
// Numbers: tuning spells.<camelNode>, wildbrands.null_strike, spells.shapes.

#include "CoreMinimal.h"
#include "WRAbilities.h"
#include "GameFramework/Actor.h"
#include "WRAbilitiesVoid.generated.h"

class UProceduralMeshComponent;
class UAbilityTask_ApplyRootMotionConstantForce;

/** A void spell: its shape's wind-up (the hand closing on a point of black), then CastVoid on the server. */
UCLASS(Abstract)
class WILDREACH_API UWRGA_VoidSpell : public UWRGameplayAbility
{
	GENERATED_BODY()

public:
	UWRGA_VoidSpell();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;

protected:
	virtual void CastVoid(AWRCharacterBase* Caster) {}
	FString ShapeName = TEXT("bolt");
	FName CastSound = TEXT("void.cast");
	/** Sacrifice: tolerance doesn't apply (it always costs). */
	bool bAlwaysCost = false;
	float Spell(const TCHAR* Field, float Fallback) const;
	/** Where the aim meets the ground within Range (the crosshair, else straight ahead). */
	FVector AimGround(const AWRCharacterBase* Caster, float Range) const;
};

UCLASS() class WILDREACH_API UWRGA_VoidBolt : public UWRGA_VoidSpell { GENERATED_BODY() public: UWRGA_VoidBolt(); protected: virtual void CastVoid(AWRCharacterBase* Caster) override; };
UCLASS() class WILDREACH_API UWRGA_VoidRift : public UWRGA_VoidSpell { GENERATED_BODY() public: UWRGA_VoidRift(); protected: virtual void CastVoid(AWRCharacterBase* Caster) override; };
UCLASS() class WILDREACH_API UWRGA_Collapse : public UWRGA_VoidSpell { GENERATED_BODY() public: UWRGA_Collapse(); protected: virtual void CastVoid(AWRCharacterBase* Caster) override; };
UCLASS() class WILDREACH_API UWRGA_BlackHole : public UWRGA_VoidSpell { GENERATED_BODY() public: UWRGA_BlackHole(); protected: virtual void CastVoid(AWRCharacterBase* Caster) override; };
UCLASS() class WILDREACH_API UWRGA_MawOfTheVoid : public UWRGA_VoidSpell { GENERATED_BODY() public: UWRGA_MawOfTheVoid(); protected: virtual void CastVoid(AWRCharacterBase* Caster) override; };
UCLASS() class WILDREACH_API UWRGA_Phase : public UWRGA_VoidSpell { GENERATED_BODY() public: UWRGA_Phase(); protected: virtual void CastVoid(AWRCharacterBase* Caster) override; };
UCLASS() class WILDREACH_API UWRGA_Nowhere : public UWRGA_VoidSpell { GENERATED_BODY() public: UWRGA_Nowhere(); protected: virtual void CastVoid(AWRCharacterBase* Caster) override; };
UCLASS() class WILDREACH_API UWRGA_Sacrifice : public UWRGA_VoidSpell { GENERATED_BODY() public: UWRGA_Sacrifice(); protected: virtual void CastVoid(AWRCharacterBase* Caster) override; };

/** Blink (and Shadow step): a short teleport, untouchable through the travel (+Fade), the body smearing away and
 *  back with a lensing ring. Shadow step goes 1.2 m behind the lock target, facing its back. */
UCLASS() class WILDREACH_API UWRGA_VoidBlink : public UWRGameplayAbility
{
	GENERATED_BODY()

public:
	UWRGA_VoidBlink();
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	bool bBehindLock = false;
	FString Key = TEXT("spells.blink");

private:
	void SetIFrames(bool bOn);
	void SetPassThrough(bool bOn);
	bool bIFrames = false;
	bool bPassing = false;
	ECollisionResponse WasPawn = ECR_Block;
	UPROPERTY() TObjectPtr<UAbilityTask_ApplyRootMotionConstantForce> Travel;
};
UCLASS() class WILDREACH_API UWRGA_ShadowStep : public UWRGA_VoidBlink { GENERATED_BODY() public: UWRGA_ShadowStep(); };

/** Null strike (Wildbrand): the blade cuts through 30% of armour. */
UCLASS() class WILDREACH_API UWRGA_NullStrike : public UWRGA_Melee
{
	GENERATED_BODY()

public:
	UWRGA_NullStrike();
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};

/** Consume: the finisher that eats a weakened foe in front of you and wins back max HP (Void -20). */
UCLASS() class WILDREACH_API UWRGA_Consume : public UWRGameplayAbility
{
	GENERATED_BODY()

public:
	UWRGA_Consume();
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	/** The body in front of Who that Consume can take now (under its threshold; a boss only stance-broken), or null. */
	static AWRCharacterBase* Prey(const AWRCharacterBase* Who);
private:
	TWeakObjectPtr<AWRCharacterBase> Victim;
};

/** A void field on the ground: a Void rift (dps inside) or a Black hole (pull, then the crush). Its centre, kind,
 *  radius and clock replicate; every machine draws the tear and its motes; the server judges. */
UCLASS(NotPlaceable)
class WILDREACH_API AWRVoidField : public AActor
{
	GENERATED_BODY()

public:
	AWRVoidField();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(Replicated) uint8 Kind = 0;        // 0 rift, 1 black hole
	UPROPERTY(Replicated) float Radius = 300.f;
	UPROPERTY(Replicated) float Seconds = 6.f;
	UPROPERTY(Replicated) double Born = 0.0;
	TWeakObjectPtr<AWRCharacterBase> Caster;
	float Dps = 14.f, Pull = 600.f, Crush = 80.f, BossSlow = 0.3f;
	bool bSwallow = false, bSpark = false;

protected:
	virtual void BeginPlay() override;

private:
	void BuildLook();
	double Now() const;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Tear;
	float TickAcc = 0.f, SparkAcc = 0.f;
	bool bCrushed = false;
};

/** Maw of the void's real geometry: four black-glass petals rising out of the ground in front of you and closing
 *  (0.8 s), every machine animating them from the same clock. Visual only (the ability judges the bite). */
UCLASS(NotPlaceable)
class WILDREACH_API AWRVoidMaw : public AActor
{
	GENERATED_BODY()

public:
	AWRVoidMaw();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;
	UPROPERTY(Replicated) double Born = 0.0;
	UPROPERTY(Replicated) float Range = 800.f;
	UPROPERTY(Replicated) float Close = 0.8f;

protected:
	virtual void BeginPlay() override;

private:
	TArray<TWeakObjectPtr<USceneComponent>> Petals;
	double Now() const;
};
