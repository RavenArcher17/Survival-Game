#pragma once

#include "CoreMinimal.h"
#include "WRAbilities.h"
#include "WRAbilitiesNecro.generated.h"

/**
 * The Necromancy path's abilities (cooks/biome-graveyard.md 2.17, combat.md 2.12-2.13). Souls are the spender: every
 * ability pays spells.<key>.soulCost as it goes off (free in Lich form); a press you can't pay flashes the Souls row.
 * Numbers: tuning spells.<key>, spells.shapes.<shape>, wildbrands.<node>; the path's rules are WRNecroPath.h.
 * Registered in UWRAbilityRegistry::ClassForNode under "necromancy".
 */

/** A Necromancy spell: the shape's wind-up (rooted or walking at half speed), then CastNecro on the server. */
UCLASS(Abstract)
class WILDREACH_API UWRGA_NecroSpell : public UWRGA_AreaSpell
{
	GENERATED_BODY()

public:
	UWRGA_NecroSpell();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;

protected:
	/** Server: the spell goes off (after the wind-up). */
	virtual void CastNecro(AWRCharacterBase* Caster) {}
	FString ShapeName = TEXT("bolt");
	float Spell(const TCHAR* Field, float Fallback) const;
	float SoulCost() const;
	/** Where a ground spell lands: under the lock (within Range), else where the crosshair meets the ground. */
	FVector AimedGround(AWRCharacterBase* Caster, float Range) const;
	/** A bolt along the aim (leading the lock), never homing on a player. */
	void FireBolt(AWRCharacterBase* Caster, FWRHit Hit, const FLinearColor& Color) const;
	/** Server: a cast that found nothing gives its Souls back and says why. */
	void Refund(AWRCharacterBase* Caster, const TCHAR* Why) const;
};

/** Raise dead (starter): the body nearest your aim within 10 m stands up for you at 60% for 30 s (two with Second
 *  wave); 2 Souls, cd 6. */
UCLASS() class WILDREACH_API UWRGA_RaiseDead : public UWRGA_NecroSpell
{
	GENERATED_BODY()
public:
	UWRGA_RaiseDead();
protected:
	virtual void CastNecro(AWRCharacterBase* Caster) override;
};

/** Corpse explosion (Horde): the minion nearest your aim blows apart: 40 in 4 m, decay 50 (Funeral pyre: burn 60). */
UCLASS() class WILDREACH_API UWRGA_CorpseExplosion : public UWRGA_NecroSpell
{
	GENERATED_BODY()
public:
	UWRGA_CorpseExplosion();
protected:
	virtual void CastNecro(AWRCharacterBase* Caster) override;
};

/** Army of the dead (Horde capstone): every body within 12 m rises for 20 s, beyond the cap; 6 Souls, cd 120. */
UCLASS() class WILDREACH_API UWRGA_ArmyOfTheDead : public UWRGA_NecroSpell
{
	GENERATED_BODY()
public:
	UWRGA_ArmyOfTheDead();
protected:
	virtual void CastNecro(AWRCharacterBase* Caster) override;
};

/** Shadow bolt (Lich): 30 damage, decay 40, 1,800 cm/s, cd 3, 2 Souls. */
UCLASS() class WILDREACH_API UWRGA_ShadowBolt : public UWRGA_NecroSpell
{
	GENERATED_BODY()
public:
	UWRGA_ShadowBolt();
protected:
	virtual void CastNecro(AWRCharacterBase* Caster) override;
};

/** Curse of weakness (Lich): a bolt with no damage that builds curse 80 (creatures cursed deal 15% less); cd 10. */
UCLASS() class WILDREACH_API UWRGA_CurseOfWeakness : public UWRGA_NecroSpell
{
	GENERATED_BODY()
public:
	UWRGA_CurseOfWeakness();
protected:
	virtual void CastNecro(AWRCharacterBase* Caster) override;
};

/** Life drain (Lich): a beam to one enemy within 12 m, 2 s channel, 12 a second, heals you half of it; cd 12. */
UCLASS() class WILDREACH_API UWRGA_LifeDrain : public UWRGA_NecroSpell
{
	GENERATED_BODY()
public:
	UWRGA_LifeDrain();
protected:
	virtual void CastNecro(AWRCharacterBase* Caster) override;
private:
	void DrainTick(int32 Left);
	TWeakObjectPtr<AWRCharacterBase> Drained;
};

/** Lich form (Lich capstone): 15 s where every Necromancy spell costs no Souls; the full ash look, a crown of
 *  grave-light; cd 120. */
UCLASS() class WILDREACH_API UWRGA_LichForm : public UWRGA_NecroSpell
{
	GENERATED_BODY()
public:
	UWRGA_LichForm();
protected:
	virtual void CastNecro(AWRCharacterBase* Caster) override;
};

/** Grave strike (Wildbrand, trunk): any melee weapon, x1.3, decay 40, stance x1.5; 1 Soul, cd 2.5. */
UCLASS() class WILDREACH_API UWRGA_GraveStrike : public UWRGA_Melee
{
	GENERATED_BODY()
public:
	UWRGA_GraveStrike();
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};

/** Scythe sweep (Wildbrand, Reaper): scythe, greatsword or spear; a 300 degree sweep 3.2 m round you, x1.5, decay 50,
 *  stance x2, hyperarmor; 2 Souls, cd 6. */
UCLASS() class WILDREACH_API UWRGA_ScytheSweep : public UWRGameplayAbility
{
	GENERATED_BODY()
public:
	UWRGA_ScytheSweep();
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
private:
	/** Server: one sweep (Arc 0: the tuning's 300 degrees) or a cyclone's turn (Reaper's cyclone: 360, x Mult). */
	void Reap(AWRCharacterBase* Caster, float Arc = 0.f, float Mult = 1.f, int32 Spin = 0);
};
