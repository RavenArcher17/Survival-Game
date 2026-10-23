#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "WRCharacterBase.h"
#include "WRAbilities.generated.h"

class AWRCharacterBase;
class AWRProjectile;

/**
 * Base for every Wildreach ability: stamina cost, simple cooldown, path XP on use,
 * blocked while dead/frozen/staggered. Local-predicted, instanced per actor.
 * Numbers come from Data/tuning.json via the *Key properties.
 */
UCLASS(Abstract)
class WILDREACH_API UWRGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UWRGameplayAbility();

	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
	virtual bool CheckCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
	virtual bool CommitAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** Path that owns this ability (None for basic weapon moves) and its tree node. */
	FName PathId;
	FName NodeId;

	/** Short name for the HUD. */
	FString DisplayName;

	float GetCooldownRemaining() const;
	float GetCooldownSeconds() const;

protected:
	AWRCharacterBase* GetCharacter() const;
	bool IsServer() const;

	/** Run Fn after Delay seconds unless the ability ends first. */
	void After(float Delay, TFunction<void()> Fn);
	void EndSelf(bool bCancelled = false);

	/** Predicted movement slowdown for the ability's duration (cleared automatically on end). */
	void SetMoveSlow(bool bBlock, bool bOn);

	/** Tuning key for the stamina cost (e.g. "stamina.lightAttack") and its fallback. */
	FString StaminaKey;
	float StaminaFallback = 0.f;
	/** Tuning key for the cooldown in seconds; empty = no cooldown. */
	FString CooldownKey;
	float CooldownFallback = 0.f;

private:
	mutable double LastCommitTime = -1000.0;
	TArray<FTimerHandle> Timers;
	bool bAppliedAttackSlow = false;
	bool bAppliedBlockSlow = false;
};

/** Melee swing: windup -> active sweep window -> recovery. Used by weapon attacks, weapon arts and creature bites. */
UCLASS()
class WILDREACH_API UWRGA_Melee : public UWRGameplayAbility
{
	GENERATED_BODY()

public:
	UWRGA_Melee();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	bool bHeavy = false;
	EWRSwingKind SwingKind = EWRSwingKind::Light;
	/** "weapon.lightTiming" / "weapon.heavyTiming"; windup/active/recovery read from it. */
	FString TimingKey = TEXT("weapon.lightTiming");
	float DamageMultiplier = 1.f;
	FGameplayTag Element;
	float BurnBuildup = 0.f;
	float FrostBuildup = 0.f;

	/** Per-ability tuning block (e.g. "spells.flameSlash") overriding multiplier/buildup/cooldown. */
	FString ArtKey;

private:
	void Sweep();
	TSet<TWeakObjectPtr<AActor>> HitThisSwing;
	FTimerHandle SweepTimer;
	double ActiveEnd = 0.0;
};

UCLASS() class WILDREACH_API UWRGA_LightAttack : public UWRGA_Melee { GENERATED_BODY() public: UWRGA_LightAttack(); };
UCLASS() class WILDREACH_API UWRGA_HeavyAttack : public UWRGA_Melee { GENERATED_BODY() public: UWRGA_HeavyAttack(); };
UCLASS() class WILDREACH_API UWRGA_FlameSlash : public UWRGA_Melee { GENERATED_BODY() public: UWRGA_FlameSlash(); };
UCLASS() class WILDREACH_API UWRGA_IceSlash : public UWRGA_Melee { GENERATED_BODY() public: UWRGA_IceSlash(); };

/** Creature bite: flat damage from the creature's weapon stats, slower telegraphed windup. */
UCLASS() class WILDREACH_API UWRGA_Bite : public UWRGA_Melee { GENERATED_BODY() public: UWRGA_Bite(); };

/** Dodge roll: root-motion burst in the input direction with an i-frame window. */
UCLASS()
class WILDREACH_API UWRGA_Dodge : public UWRGameplayAbility
{
	GENERATED_BODY()

public:
	UWRGA_Dodge();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	void SetInvulnerable(bool bOn);
	bool bInvulnerable = false;
};

/** Hold to block; the first moments of a block are a parry. */
UCLASS()
class WILDREACH_API UWRGA_Block : public UWRGameplayAbility
{
	GENERATED_BODY()

public:
	UWRGA_Block();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	UFUNCTION() void OnReleased(float TimeHeld);
	void SetParrying(bool bOn);
	bool bParrying = false;
};

/** Spell that fires a projectile along the aim. */
UCLASS()
class WILDREACH_API UWRGA_Spell : public UWRGameplayAbility
{
	GENERATED_BODY()

public:
	UWRGA_Spell();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	/** e.g. "spells.fireBolt" with damage/burnBuildup/frostBuildup/speed/cooldown/heat/chill. */
	FString SpellKey;
	FGameplayTag Element;
	FLinearColor Color = FLinearColor::White;
};

UCLASS() class WILDREACH_API UWRGA_FireBolt : public UWRGA_Spell { GENERATED_BODY() public: UWRGA_FireBolt(); };
UCLASS() class WILDREACH_API UWRGA_FrostShard : public UWRGA_Spell { GENERATED_BODY() public: UWRGA_FrostShard(); };

/** Maps skill-tree nodes to ability classes. Nodes without an entry aren't implemented yet. */
UCLASS()
class WILDREACH_API UWRAbilityRegistry : public UObject
{
	GENERATED_BODY()

public:
	static TSubclassOf<UGameplayAbility> ClassForNode(FName PathId, FName NodeId);
};
