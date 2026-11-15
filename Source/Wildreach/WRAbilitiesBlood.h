#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WRAbilities.h"
#include "WRAbilitiesBlood.generated.h"

/**
 * The Blood & curse path's abilities (cooks/biome-swamp.md 2.17, combat.md 2.12-2.13). Every cost is HP, paid through
 * UWRBloodPath::SpendBlood into the Spilled bar (WRBloodPath.h); a slot you can't pay greys and shakes. Numbers: tuning
 * spells.<key> (hpCost, damage, cooldown...), spells.shapes.<shape>, wildbrands.<node>. Registered in
 * UWRAbilityRegistry::ClassForNode under "blood".
 */

/** A Blood spell: the shape's wind-up (rooted or walking at half speed), then CastBlood on the server; costs HP. */
UCLASS(Abstract)
class WILDREACH_API UWRGA_BloodSpell : public UWRGA_AreaSpell
{
	GENERATED_BODY()

public:
	UWRGA_BloodSpell();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;

protected:
	/** Server: the spell goes off (after the wind-up). */
	virtual void CastBlood(AWRCharacterBase* Caster) {}
	/** spells.shapes.<ShapeName> (bolt, lance, nova, wall, buff). */
	FString ShapeName = TEXT("bolt");
	FName CastSound = TEXT("blood.cast");
	/** Num(spells.<key>.<Field>). */
	float Spell(const TCHAR* Field, float Fallback) const;
	/** A bolt along the aim (leading the lock), homing only on a locked creature (fire-frost RAISE 3: never at players). */
	void FireBolt(AWRCharacterBase* Caster, FWRHit Hit, const FLinearColor& Color) const;
};

/** Blood bolt (starter): 22 damage, bleed 30, 5 HP, 2,600 cm/s. */
UCLASS() class WILDREACH_API UWRGA_BloodBolt : public UWRGA_BloodSpell
{
	GENERATED_BODY()
public:
	UWRGA_BloodBolt();
protected:
	virtual void CastBlood(AWRCharacterBase* Caster) override;
};

/** Blood spear: a lance along the aim, 34 damage, bleed 50, through the first 3 bodies; 10 HP. */
UCLASS() class WILDREACH_API UWRGA_BloodSpear : public UWRGA_BloodSpell
{
	GENERATED_BODY()
public:
	UWRGA_BloodSpear();
protected:
	virtual void CastBlood(AWRCharacterBase* Caster) override;
};

/** Blood boil: a nova (600): every enemy with bleed built up (>= 30% of its threshold) or bleeding takes 20% of its bleed burst. */
UCLASS() class WILDREACH_API UWRGA_BloodBoil : public UWRGA_BloodSpell
{
	GENERATED_BODY()
public:
	UWRGA_BloodBoil();
protected:
	virtual void CastBlood(AWRCharacterBase* Caster) override;
};

/** Transfusion: a garnet stream to an ally within 12 m: 20 of your HP (into Spilled) gives them 30 over 1.5 s. */
UCLASS() class WILDREACH_API UWRGA_Transfusion : public UWRGA_BloodSpell
{
	GENERATED_BODY()
public:
	UWRGA_Transfusion();
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	/** The ally nearest the crosshair within range and cone (players, your own tames), or null. */
	static AWRCharacterBase* FindAlly(const AWRCharacterBase* Caster);
protected:
	virtual void CastBlood(AWRCharacterBase* Caster) override;
};

/** Crimson tide (capstone): a wall 600 cm wide that travels 1,500 cm at 900 cm/s: 45 damage, bleed 60; passes allies. */
UCLASS() class WILDREACH_API UWRGA_CrimsonTide : public UWRGA_BloodSpell
{
	GENERATED_BODY()
public:
	UWRGA_CrimsonTide();
protected:
	virtual void CastBlood(AWRCharacterBase* Caster) override;
};

/** Hex of weakness: a bolt with no damage, curse 80. */
UCLASS() class WILDREACH_API UWRGA_HexOfWeakness : public UWRGA_BloodSpell
{
	GENERATED_BODY()
public:
	UWRGA_HexOfWeakness();
protected:
	virtual void CastBlood(AWRCharacterBase* Caster) override;
};

/** Doom: a bolt whose mark bursts after 4 s for 60 + 25% of what the target took meanwhile (WRBloodPath MarkDoom). */
UCLASS() class WILDREACH_API UWRGA_Doom : public UWRGA_BloodSpell
{
	GENERATED_BODY()
public:
	UWRGA_Doom();
protected:
	virtual void CastBlood(AWRCharacterBase* Caster) override;
};

/** Sanguine lord (capstone): a self buff (10 s): 30% of all damage you deal heals you, through Spilled first. */
UCLASS() class WILDREACH_API UWRGA_SanguineLord : public UWRGA_BloodSpell
{
	GENERATED_BODY()
public:
	UWRGA_SanguineLord();
protected:
	virtual void CastBlood(AWRCharacterBase* Caster) override;
};

/** A Blood Wildbrand: the weapon art's row (wildbrands.<node>), paid in HP as well as stamina. */
UCLASS(Abstract)
class WILDREACH_API UWRGA_BloodBrand : public UWRGA_Melee
{
	GENERATED_BODY()
public:
	UWRGA_BloodBrand();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
};

/** Crimson slash (Wildbrand): x1.4, bleed 45, stance x1.5; 6 HP. */
UCLASS() class WILDREACH_API UWRGA_CrimsonSlash : public UWRGA_BloodBrand { GENERATED_BODY() public: UWRGA_CrimsonSlash(); };
/** Leech strike (Wildbrand): x1.2, bleed 30; heals 50% of the damage it deals; 4 HP. Dagger, sword, scythe, fists. */
UCLASS() class WILDREACH_API UWRGA_LeechStrike : public UWRGA_BloodBrand { GENERATED_BODY() public: UWRGA_LeechStrike(); };

/**
 * Crimson tide's wave on the server: a wall across the caster's aim moving forward, hitting each enemy once as it passes
 * (allies and the caster pass through it). What players see is broadcast by the ability, so it doesn't replicate.
 */
UCLASS()
class WILDREACH_API AWRBloodTide : public AActor
{
	GENERATED_BODY()

public:
	AWRBloodTide();
	virtual void Tick(float DeltaSeconds) override;

	static AWRBloodTide* SpawnTide(AWRCharacterBase* Caster, const FVector& From, const FVector& Dir, const FWRHit& Hit, float Width, float Length, float Speed, float Height);

	TWeakObjectPtr<AWRCharacterBase> Caster;
	UPROPERTY() FWRHit Hit;
	FVector Dir = FVector::ForwardVector;
	float Width = 600.f;
	float Length = 1500.f;
	float Speed = 900.f;
	float Height = 250.f;

private:
	float Travelled = 0.f;
	TSet<TWeakObjectPtr<AActor>> Struck;
};
