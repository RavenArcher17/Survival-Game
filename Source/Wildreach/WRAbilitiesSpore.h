#pragma once

#include "CoreMinimal.h"
#include "WRAbilities.h"
#include "WRAbilitiesSpore.generated.h"

/**
 * The Spore path's abilities (cooks/biome-fungal.md 2.17; build-biome-fungal.md 2.17). Spores is the spender: each pays
 * spells.<key>.sporeCost as it goes off; a press you can't pay flashes the Spores bar (no floor). Numbers: tuning
 * spells.<key>, spells.shapes.<shape>, wildbrands.infect_strike; the path's rules are WRSporePath.h. Registered in
 * UWRAbilityRegistry::ClassForNode under "spore".
 */

/** A Spore spell: Spores, the shape's wind-up (rooted or walking), then CastSpore on the server. */
UCLASS(Abstract)
class WILDREACH_API UWRGA_SporeSpell : public UWRGameplayAbility
{
	GENERATED_BODY()

public:
	UWRGA_SporeSpell();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;

protected:
	virtual void CastSpore(AWRCharacterBase* Caster) {}
	FString SpellKey;
	FString ShapeName = TEXT("pool");
	float Spell(const TCHAR* Field, float Fallback) const;
	float SporeCost() const { return Spell(TEXT("sporeCost"), 0.f); }
	/** The ground the caster aims at within Range (a locked target's feet, the crosshair's point, else ahead). */
	FVector AimedGround(AWRCharacterBase* Caster, float Range) const;
};

/** Spore cloud (trunk starter): a 3 m cloud at the aimed ground (15 m) for 6 s that plants 1 Spore a second on every
 *  enemy in it (poison 10 a second, the cloud's own); 15 Spores, cd 6. A real spore cloud: wind pushes it, fire flashes it. */
UCLASS() class WILDREACH_API UWRGA_SporeCloud : public UWRGA_SporeSpell
{
	GENERATED_BODY()
public:
	UWRGA_SporeCloud();
protected:
	virtual void CastSpore(AWRCharacterBase* Caster) override;
};

/** Detonate (free with Spore cloud): every Spore you've planted within 20 m bursts (8 + rot 10 each); 0 Spores, 0.3 s
 *  while walking. */
UCLASS() class WILDREACH_API UWRGA_Detonate : public UWRGA_SporeSpell
{
	GENERATED_BODY()
public:
	UWRGA_Detonate();
protected:
	virtual void CastSpore(AWRCharacterBase* Caster) override;
};

/** Blight (Rot): a 4 m rot cloud that spreads 50 cm/s to 7 m, rot 15 a second, 10 s; 30 Spores, cd 18. */
UCLASS() class WILDREACH_API UWRGA_Blight : public UWRGA_SporeSpell
{
	GENERATED_BODY()
public:
	UWRGA_Blight();
protected:
	virtual void CastSpore(AWRCharacterBase* Caster) override;
};

/** Sporeling (Colony): a 60 HP knee-high fungal biter (bite 10, plants 1 Spore) for 25 s; 25 Spores, cd 10. */
UCLASS() class WILDREACH_API UWRGA_Sporeling : public UWRGA_SporeSpell
{
	GENERATED_BODY()
public:
	UWRGA_Sporeling();
protected:
	virtual void CastSpore(AWRCharacterBase* Caster) override;
};

/** Fungal behemoth (Colony capstone): a 500 HP fungal giant for 40 s whose slams plant 2 Spores; 60 Spores, cd 120. */
UCLASS() class WILDREACH_API UWRGA_FungalBehemoth : public UWRGA_SporeSpell
{
	GENERATED_BODY()
public:
	UWRGA_FungalBehemoth();
protected:
	virtual void CastSpore(AWRCharacterBase* Caster) override;
};

/** Spore armor (Symbiote): -15% damage taken for 20 s, a coat of cyan-tipped fuzz; 30 Spores, cd 30. */
UCLASS() class WILDREACH_API UWRGA_SporeArmor : public UWRGA_SporeSpell
{
	GENERATED_BODY()
public:
	UWRGA_SporeArmor();
protected:
	virtual void CastSpore(AWRCharacterBase* Caster) override;
};

/** Living colony (Symbiote capstone): 15 s a walking spore cloud (WRSporePath::TickOwner runs the aura); 80 Spores, cd 120. */
UCLASS() class WILDREACH_API UWRGA_LivingColony : public UWRGA_SporeSpell
{
	GENERATED_BODY()
public:
	UWRGA_LivingColony();
protected:
	virtual void CastSpore(AWRCharacterBase* Caster) override;
};

/** Infect strike (trunk Wildbrand): x1.2, rot 30, stance x1.5, plants 3 Spores on what it hits; 10 Spores, cd 2.5. */
UCLASS() class WILDREACH_API UWRGA_InfectStrike : public UWRGA_Melee
{
	GENERATED_BODY()
public:
	UWRGA_InfectStrike();
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};
