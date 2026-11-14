#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "WRCombatTypes.h"
#include "WRCharacterBase.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class UWRAttributeSet;
class UWRStatusComponent;

/** Fires on every machine when a character shows a hit (HUD damage numbers). */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FWROnHitShown, const AActor* /*Target*/, float /*Damage*/, EWRHitResult /*Result*/);

UENUM()
enum class EWRSwingKind : uint8
{
	None, Light, Heavy, WeaponArt, Cast, Bite,
	// Creature moves, so the attack clip can be timed to each (the strike lands when the damage does): a snap at
	// running prey, and the special attacks (creatureAI.<kind> in tuning.json).
	Snap, Lunge, Charge, Stomp, Spit,
	// A creature attack row's strike (Data/attacks.json): the clip's strike lands at the end of Duration (wind-up + hold).
	Strike
};

/**
 * Shared base for survivors and creatures: ability system access, attributes, status
 * upkeep, a placeholder body + weapon, swing visuals and death.
 */
UCLASS(Abstract)
class WILDREACH_API AWRCharacterBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AWRCharacterBase(const FObjectInitializer& ObjectInitializer);

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UWRAttributeSet* GetAttributes() const;

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	bool IsAlive() const;
	bool HasTag(const FGameplayTag& Tag) const;

	virtual bool IsCreature() const { return false; }
	/** Name of the player who tamed this creature (empty for wild creatures and survivors). */
	virtual FString GetTameOwner() const { return FString(); }
	/** Wild creatures don't fight each other; tames fight for their owner; survivors can hurt anyone except their own tames. */
	virtual bool IsHostileTo(const AWRCharacterBase* Other) const;

	/** Server: taming hooks (creatures implement them). */
	virtual void AddTorpor(float Amount) {}
	/** Server: a same-element path hit calms instead of hurting. True if it was consumed. */
	virtual bool TryPathMagic(const struct FWRHit& Hit) { return false; }

	/** Server: who this character is fighting (tames attack their owner's target). */
	TWeakObjectPtr<AWRCharacterBase> CombatTarget;

	/** Movement speed multiplier from statuses and actions (read by the movement component on every machine). */
	float GetSpeedMultiplier() const;
	/** Server-driven extra speed (creature AI: a charge, running for its life). */
	float SpeedBoost = 1.f;
	/** Biting cold (frost stat node): how much deeper the current Slowed status is, in percent (server sets, replicates
	 *  so the slowed player's own movement prediction agrees). */
	UPROPERTY(Replicated) uint8 SlowExtraPct = 0;

	/** Server: health hit zero. */
	virtual void Die(AActor* Killer);

	/** Server: tell every machine a hit landed (damage numbers, flash, impact effects). FxFlags are WRFx::EHitFlags;
	 *  Source and Origin (where the blow came from) place the impact and pick who feels the hit-stop and shake. */
	void ReportHit(float Damage, EWRHitResult Result, uint16 FxFlags = 0, AActor* Source = nullptr, const FVector& Origin = FVector::ZeroVector);

	/** Server: last chance to change damage before it lands (boss shells, airborne bosses, weaknesses). */
	virtual float ModifyIncomingDamage(const struct FWRHit& Hit, float Damage) { return Damage; }
	/** Server: damage actually taken (boss damage ledger). */
	virtual void OnDamageTaken(const struct FWRHit& Hit, float Damage) {}

	/** Server: someone attacked us (creatures use it for aggro). */
	virtual void NotifyAttackedBy(AActor* Attacker) {}

	/** Name for HUD labels. */
	virtual FString GetDisplayLabel() const;

	/** Play a swing locally and, on the server, replicate it to everyone else. Variant picks the clip (a light chain's
	 *  1-2-3); Windup / Active / Recovery (> 0) time the clip so its strike lands on the active window on every machine. */
	void PlaySwing(EWRSwingKind Kind, float Duration, uint8 Variant = 0, float Windup = -1.f, float Active = -1.f, float Recovery = -1.f);

	// --- Combat core (cooks/build-combat-core.md 3.2-3.5) ---
	/** One swing's timing on this machine (the server's timers, the owner's prediction). Start < 0: not swinging. */
	struct FSwingWindows
	{
		double Start = -1.0, ActiveAt = 0.0, ActiveEnd = 0.0, ComboAt = 0.0, CancelAt = 0.0, End = 0.0;
		FName Move;
		bool bLight = false;
		/** Its hyperarmor's poise multiplier (0: tuning poise.hyperarmor.heavy) and whether it's a great weapon's
		 *  (damage taken poise.hyperarmorDamageTakenGreat). */
		float HyperarmorMult = 0.f;
		bool bGreat = false;
		/** A spear's guard poke: the raised guard still counts through it. */
		bool bKeepsGuard = false;
	};
	FSwingWindows Swing;
	/** The last swing that ended: its move, when, and whether it got to its combo window (a light chain goes on). */
	FName LastSwingMove;
	double LastSwingEnd = -100.0;
	bool bLastSwingChains = false;
	bool IsSwinging() const;
	/** A combat window / state tag: the server's replicates (TagOnly); the owning client adds its own copy at once. */
	void SetCombatTag(const FGameplayTag& Tag, bool bOn);
	/** This machine: the last time a press was refused for stamina (the HUD flashes the bar). */
	double StaminaRefusedAt = -100.0;
	/** Worn + held kg over the equip load max, x100 (server sets it; the owner's roll and HUD read it). */
	UPROPERTY(Replicated) uint8 EquipLoadPct = 0;
	float GetEquipLoadRatio() const { return EquipLoadPct / 100.f; }
	/** Server: when the current roll began, back-dated for a remote player (the perfect dodge). */
	double DodgeStartedAt = -100.0;
	/** Server: parries taken toward a break (cleared when its stance comes back). */
	int32 ParriesTaken = 0;

	// --- Full combat (cooks/build-combat-full.md 2) ---
	/** Two-handing the held weapon (survivors choose it; 2h / great weapons always are): damage and stance up, the guard
	 *  is the weapon's, the shield comes off the arm. */
	UPROPERTY(ReplicatedUsing=OnRep_TwoHanded) bool bTwoHanded = false;
	UFUNCTION() virtual void OnRep_TwoHanded() {}
	/** When the last roll / backstep ended (server and the owner stamp it): the rolling / backstep light picks from it. */
	double LastDodgeEnd = -100.0;
	bool bLastDodgeBackstep = false;
	/** Status meters for every machine's bars, 0-100 each, in WRStatus::EId order (the server's buildup / threshold). */
	UPROPERTY(Replicated) TArray<uint8> BuildupPct;
	/** Server: the last element that struck it and who (element reactions, 4 s window). */
	FGameplayTag AuraElement;
	TWeakObjectPtr<AActor> AuraBy;
	double AuraAt = -100.0;
	/** Server: max health taken by Decay (given back when the fight ends / on rest). */
	float DecayLost = 0.f;
	/** Every machine: the attacker's charged heavy is held (0..1 of the full charge) for the weapon's glint. */
	float ChargeShown = -1.f;
	/** Every machine: its stance broke (true) or came back (false): creatures slump and wear the gold weak-point mark. */
	UFUNCTION(NetMulticast, Reliable) void MulticastStanceBreak(bool bBroken);
	/** The swing being shown (set before OnSwingVisual): its clip variant and the times its clip is warped to. */
	uint8 SwingVariantShown = 0;
	float SwingWindupShown = -1.f;
	float SwingActiveShown = -1.f;
	float SwingRecoveryShown = -1.f;
	/** The flags of the hit being shown (set before OnHitVisual): its reaction level (WRFx::ReactionOf). */
	uint16 HitFlagsShown = 0;

	/** Server: a sound event on this character for everyone nearby (WRSound::Broadcast: eating, equipping...). */
	UFUNCTION(NetMulticast, Unreliable) void MulticastSound(FName Event);

	/** Grant an ability on the server; returns its spec handle. */
	FGameplayAbilitySpecHandle GrantAbility(TSubclassOf<UGameplayAbility> AbilityClass, EWRAbilityInput Input = EWRAbilityInput::None, int32 Level = 1);

	/** Body tint (placeholder art). */
	void SetBodyColor(const FLinearColor& Color);

	static FWROnHitShown OnHitShown;

	UPROPERTY(VisibleAnywhere, Category="Combat") TObjectPtr<UWRStatusComponent> Status;

	UPROPERTY(EditAnywhere, Replicated, Category="Combat") FWRWeaponStats Weapon;
	/** Server: armor from equipped gear; incoming hits are scaled by 100 / (100 + ArmorValue). */
	float ArmorValue = 0.f;

	/** Max health / stamina / poise to start with (players get these from character level). */
	UPROPERTY(EditAnywhere, Category="Combat") float BaseHealth = 100.f;
	UPROPERTY(EditAnywhere, Category="Combat") float BasePoise = 40.f;

protected:
	virtual void BeginPlay() override;

	/** Server: fill attributes from Base* values. */
	void InitVitals(float MaxHealth, float MaxStamina, float MaxPoise);

	virtual void OnDeath(AActor* Killer) {}
	/** Every machine: an attack swing started / this character died (real meshes play animations here). */
	virtual void OnSwingVisual(EWRSwingKind Kind, float Duration) {}
	virtual void OnDeathVisual() {}
	/** Every machine: a stance break starts / ends (MulticastStanceBreak). */
	virtual void OnStanceBreakVisual(bool bBroken) {}
	/** Clients: a hit landed on us (after the hit flash starts). */
	virtual void OnHitVisual(EWRHitResult Result) {}

	/** Owned by creatures; players point this at their PlayerState's ASC. */
	UPROPERTY() TObjectPtr<UAbilitySystemComponent> AbilitySystem;
	UPROPERTY() TObjectPtr<UWRAttributeSet> Attributes;

	UPROPERTY(VisibleAnywhere, Category="Mesh") TObjectPtr<UStaticMeshComponent> BodyMesh;
	UPROPERTY(VisibleAnywhere, Category="Mesh") TObjectPtr<UStaticMeshComponent> FaceMarker;
	UPROPERTY(VisibleAnywhere, Category="Mesh") TObjectPtr<UStaticMeshComponent> WeaponMesh;
	/** The weapon is held by an animated body: don't swing the placeholder pivot. */
	bool bArtWeapon = false;

	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> BodyMaterial;
	FLinearColor BaseBodyColor = FLinearColor(0.6f, 0.6f, 0.65f);

private:
	UFUNCTION(NetMulticast, Unreliable) void MulticastHit(float Damage, EWRHitResult Result, uint16 FxFlags, AActor* Source, FVector_NetQuantize Origin);
	UFUNCTION(NetMulticast, Reliable) void MulticastDied();

	UPROPERTY(ReplicatedUsing=OnRep_Swing) uint8 SwingSeq = 0;
	UPROPERTY(Replicated) EWRSwingKind SwingKindRep = EWRSwingKind::None;
	UPROPERTY(Replicated) float SwingDurationRep = 0.f;
	UPROPERTY(Replicated) uint8 SwingVariantRep = 0;
	UPROPERTY(Replicated) float SwingWindupRep = -1.f;
	UPROPERTY(Replicated) float SwingActiveRep = -1.f;
	UPROPERTY(Replicated) float SwingRecoveryRep = -1.f;
	/** Tags this owning client added itself (SetCombatTag), so it removes only its own. */
	TSet<FGameplayTag> LocalCombatTags;
	UFUNCTION() void OnRep_Swing();

	void StartSwingVisual(EWRSwingKind Kind, float Duration);
	void UpdateVisuals(float DeltaSeconds);

	EWRSwingKind VisualSwing = EWRSwingKind::None;
	float VisualSwingTime = 0.f;
	float VisualSwingDuration = 0.f;
	float HitFlash = 0.f;
};
