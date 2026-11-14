#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "WRCharacterBase.h"
#include "WRCombatRules.h"
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

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
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
	/** >= 0: this activation's stamina cost instead of StaminaKey's (a move row's). Set before CommitAbility. */
	float CostOverride = -1.f;
	/** The commitment rule it obeys (WRCombatRules): attacks wait for the combo window, rolls and guards for the cancel
	 *  window. bRules false: none (creature bites keep their own AI timing). */
	WRCombatRules::EAct RuleAct = WRCombatRules::EAct::Attack;
	bool bRules = false;
	/** Usable in the saddle (bolts, lances; Wildbrands then check their own mounted flag). */
	bool bAllowedMounted = false;
	/** Predicted root: no movement at all (heavies; a CMC saved-move flag). */
	void SetMoveRoot(bool bOn);
	/** Tuning key for the cooldown in seconds; empty = no cooldown. */
	FString CooldownKey;
	float CooldownFallback = 0.f;

private:
	mutable double LastCommitTime = -1000.0;
	TArray<FTimerHandle> Timers;
	bool bAppliedAttackSlow = false;
	bool bAppliedBlockSlow = false;
	bool bAppliedRoot = false;
};

/** Melee swing: windup -> active sweep window -> recovery. Used by weapon attacks, weapon arts and creature bites. */
UCLASS()
class WILDREACH_API UWRGA_Melee : public UWRGameplayAbility
{
	GENERATED_BODY()

public:
	UWRGA_Melee();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual bool CheckCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;

	/** Can Who use the Wildbrand Node now (its fits list vs the held class; riding needs its mounted flag)? OutWhy says
	 *  why not ("needs a spear", "not while riding") for the slot's greyed label. */
	static bool WildbrandFits(const AWRCharacterBase* Who, FName Node, FString& OutWhy);

protected:
	bool bHeavy = false;
	EWRSwingKind SwingKind = EWRSwingKind::Light;
	/** "weapon.lightTiming" / "weapon.heavyTiming"; windup/active/recovery read from it (weapon arts, bites). */
	FString TimingKey = TEXT("weapon.lightTiming");
	float DamageMultiplier = 1.f;
	FGameplayTag Element;
	float BurnBuildup = 0.f;
	float FrostBuildup = 0.f;
	/** Weapon arts: stance damage x the class's light stance (tuning spells.<art>.stance overrides). */
	float StanceMultiplier = 1.f;

	/** Per-ability tuning block (e.g. "spells.flameSlash") overriding multiplier/buildup/cooldown. */
	FString ArtKey;

	/** Light / Heavy: the swing is a move row of the held weapon's class (Data/weapons.json): the chain, the guard counter. */
	bool bUsesMoves = false;
	/** Which row this activation plays (picked at activation, the same on the server and the owner). */
	FName PickMove(const AWRCharacterBase* Character) const;

private:
	void Sweep();
	void SetWindows(AWRCharacterBase* Character, double Now, float Windup, float Active, float Recovery, bool bHyperarmor);
	void ClearWindows(AWRCharacterBase* Character, bool bEnded);
	/** The step-in target: the lock, else the nearest enemy in the swing's cone (COMBAT-AND-LESSONS 1.17). */
	AWRCharacterBase* StepTarget(AWRCharacterBase* Character, float Reach) const;
	TSet<TWeakObjectPtr<AActor>> HitThisSwing;
	FTimerHandle SweepTimer;
	double ActiveEnd = 0.0;
	bool bFromGuard = false;  // this swing came out of a held block (server, for the log)
	FName MoveId;             // this activation's move row (bUsesMoves)
	float MoveActive = 0.18f;
	bool bWindowsSet = false;

	// --- Full attack set (cooks/build-combat-full.md 2.2-2.3, 2.13) ---
	/** The swing proper, after any charge: clip, movement, windows, sweeps, end. */
	void StartSwing(float Windup, float Active, float Recovery, bool bHyperarmor, uint8 Variant, bool bStepping);
	/** A held heavy / chargeable Wildbrand: hold the coil until release or the cap. */
	void BeginCharge(float Windup, float Active, float Recovery, uint8 Variant);
	UFUNCTION() void OnChargeReleased(float TimeHeld);
	/** A jump attack's blade stays live until it lands. */
	void JumpTick();
	FString WildbrandKey() const;
	bool FollowUpReady() const;
	float ChargeFactor = 1.f;     // damage x from the charge (1 = none)
	bool bCharging = false;
	double ChargeStartedAt = 0.0;
	uint8 PendingVariant = 0;
	bool bThisFollowUp = false;   // this activation is a Wildbrand's follow-up
	bool bJumpSwing = false;
	double JumpActiveSince = 0.0;
	double FollowUpFrom = -1.0, FollowUpUntil = -1.0;
};

UCLASS() class WILDREACH_API UWRGA_LightAttack : public UWRGA_Melee { GENERATED_BODY() public: UWRGA_LightAttack(); };
UCLASS() class WILDREACH_API UWRGA_HeavyAttack : public UWRGA_Melee { GENERATED_BODY() public: UWRGA_HeavyAttack(); };
UCLASS() class WILDREACH_API UWRGA_FlameSlash : public UWRGA_Melee { GENERATED_BODY() public: UWRGA_FlameSlash(); };
UCLASS() class WILDREACH_API UWRGA_IceSlash : public UWRGA_Melee { GENERATED_BODY() public: UWRGA_IceSlash(); };
UCLASS() class WILDREACH_API UWRGA_MoltenEdge : public UWRGA_Melee { GENERATED_BODY() public: UWRGA_MoltenEdge(); };
UCLASS() class WILDREACH_API UWRGA_Flurry : public UWRGA_Melee { GENERATED_BODY() public: UWRGA_Flurry(); };
/** Capstone heavy weapon art that leaves lava behind. */
UCLASS() class WILDREACH_API UWRGA_MoltenCleave : public UWRGA_Melee
{
	GENERATED_BODY()
public:
	UWRGA_MoltenCleave();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};

/** Creature bite: flat damage from the creature's weapon stats, slower telegraphed windup. */
UCLASS() class WILDREACH_API UWRGA_Bite : public UWRGA_Melee { GENERATED_BODY() public: UWRGA_Bite(); };

/**
 * Dodge: a roll in the stick's direction whose distance, i-frames and recovery come from the equip load tier
 * (defense.load), or a backstep with the stick at rest (defense.backstep). Overloaded: a slow step with no i-frames.
 * Stops at a ledge unless the stick still holds toward it. Fires on press (COMBAT-AND-LESSONS 1.7).
 */
UCLASS()
class WILDREACH_API UWRGA_Dodge : public UWRGameplayAbility
{
	GENERATED_BODY()

public:
	UWRGA_Dodge();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** The tier a load share falls in: light, medium, heavy, overloaded. */
	static FName LoadTier(float Ratio);

private:
	void SetInvulnerable(bool bOn);
	bool bInvulnerable = false;
	bool bWasBackstep = false;
	UPROPERTY() TObjectPtr<class UAbilityTask_ApplyRootMotionConstantForce> Travel;
};

/**
 * Guard (Block input): pressed, the guard is up at once and the carried off-hand's parry window opens after its startup
 * (a press soon after the last shrinks it); let go before defense.parryTapSeconds and it was a committed parry (its
 * recovery plays); held longer it's a plain guard with nothing to pay on release (COMBAT-AND-LESSONS 1.2-1.3).
 */
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
	bool bRecovering = false;
	double PressedAt = 0.0;
	double LastPressAt = -100.0;
};

/**
 * A critical attack from Light (COMBAT-AND-LESSONS 1.3-1.4): a riposte on a parried, guard-broken or stance-broken target
 * in front, or a backstab from behind. The attacker warps to the sync point, plays the paired stab and is untouchable
 * until its damage frame; the target is held through it. The server finds the target again on its own (rewound) view.
 */
UCLASS()
class WILDREACH_API UWRGA_Critical : public UWRGameplayAbility
{
	GENERATED_BODY()

public:
	UWRGA_Critical();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	enum class EKind : uint8 { None, Riposte, Backstab };
	/** Who Attacker can crit right now and how (Riposte covers parries, guard breaks and stance breaks). bRewound: the
	 *  server judges against bodies where the attacker saw them. */
	static AWRCharacterBase* FindTarget(const AWRCharacterBase* Attacker, EKind& OutKind, bool bRewound);

private:
	void SetInvulnerable(bool bOn);
	void Hold(bool bOn, float Seconds);
	TWeakObjectPtr<AWRCharacterBase> Victim;
	EKind Kind = EKind::None;
	bool bInvulnerable = false;
	bool bHolding = false;
};

/** Spell that fires a projectile along the aim. */
UCLASS()
class WILDREACH_API UWRGA_Spell : public UWRGameplayAbility
{
	GENERATED_BODY()

public:
	UWRGA_Spell();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** e.g. "spells.fireBolt" with damage/burnBuildup/frostBuildup/speed/cooldown/heat/chill. */
	FString SpellKey;
	FGameplayTag Element;
	FLinearColor Color = FLinearColor::White;
};

UCLASS() class WILDREACH_API UWRGA_FireBolt : public UWRGA_Spell { GENERATED_BODY() public: UWRGA_FireBolt(); };
UCLASS() class WILDREACH_API UWRGA_FrostShard : public UWRGA_Spell { GENERATED_BODY() public: UWRGA_FrostShard(); };

UENUM()
enum class EWRAreaShape : uint8
{
	Wall,   // a line of fire across your aim
	Pool,   // a lingering pool where you aim
	Nova,   // a burst around you
	Lance,  // a piercing line along your aim
	Meteor, // a big delayed strike where you aim
	IceWall,// blocking ice pillars across your aim
	Storm,  // a lasting frost storm around you
};

/** Tree spells past the trunk: Fire wall, Lava pool (fire); Frost nova, Ice lance (frost). Numbers: tuning spells.<key>.*. */
UCLASS()
class WILDREACH_API UWRGA_AreaSpell : public UWRGameplayAbility
{
	GENERATED_BODY()

public:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	void Execute();
	/** The cast proper (after any charge): the shape's wind-up, rooted or walking, then Execute. */
	void CastNow();
	UFUNCTION() void OnSpellChargeReleased(float TimeHeld);
	/** tuning spells.shapes.<shape> for this spell's shape (combat.md 2.13). */
	FString ShapeKey() const;
	FString SpellKey;
	EWRAreaShape Shape = EWRAreaShape::Nova;
	FGameplayTag Element;
	float SpellCharge = 1.f;
	bool bSpellCharging = false;
	double SpellChargeStarted = 0.0;
};

UCLASS() class WILDREACH_API UWRGA_FireWall : public UWRGA_AreaSpell { GENERATED_BODY() public: UWRGA_FireWall(); };
UCLASS() class WILDREACH_API UWRGA_LavaPool : public UWRGA_AreaSpell { GENERATED_BODY() public: UWRGA_LavaPool(); };
UCLASS() class WILDREACH_API UWRGA_FrostNova : public UWRGA_AreaSpell { GENERATED_BODY() public: UWRGA_FrostNova(); };
UCLASS() class WILDREACH_API UWRGA_IceLance : public UWRGA_AreaSpell { GENERATED_BODY() public: UWRGA_IceLance(); };
UCLASS() class WILDREACH_API UWRGA_Meteor : public UWRGA_AreaSpell { GENERATED_BODY() public: UWRGA_Meteor(); };
UCLASS() class WILDREACH_API UWRGA_IceWall : public UWRGA_AreaSpell { GENERATED_BODY() public: UWRGA_IceWall(); };
UCLASS() class WILDREACH_API UWRGA_Blizzard : public UWRGA_AreaSpell { GENERATED_BODY() public: UWRGA_Blizzard(); };

/** Maps skill-tree nodes to ability classes. Nodes without an entry aren't implemented yet. */
UCLASS()
class WILDREACH_API UWRAbilityRegistry : public UObject
{
	GENERATED_BODY()

public:
	static TSubclassOf<UGameplayAbility> ClassForNode(FName PathId, FName NodeId);
	/** Implemented: an ability class, or a passive the combat code checks with UWRPathComponent::HasNode. */
	static bool IsBuilt(FName PathId, FName NodeId);
};
