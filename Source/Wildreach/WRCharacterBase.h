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
	None, Light, Heavy, WeaponArt, Cast, Bite
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

	/** Server: health hit zero. */
	virtual void Die(AActor* Killer);

	/** Server: tell every machine a hit landed (damage numbers, flash). */
	void ReportHit(float Damage, EWRHitResult Result);

	/** Server: last chance to change damage before it lands (boss shells, airborne bosses, weaknesses). */
	virtual float ModifyIncomingDamage(const struct FWRHit& Hit, float Damage) { return Damage; }
	/** Server: damage actually taken (boss damage ledger). */
	virtual void OnDamageTaken(const struct FWRHit& Hit, float Damage) {}

	/** Server: someone attacked us (creatures use it for aggro). */
	virtual void NotifyAttackedBy(AActor* Attacker) {}

	/** Name for HUD labels. */
	virtual FString GetDisplayLabel() const;

	/** Play a swing locally and, on the server, replicate it to everyone else. */
	void PlaySwing(EWRSwingKind Kind, float Duration);

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

	/** Owned by creatures; players point this at their PlayerState's ASC. */
	UPROPERTY() TObjectPtr<UAbilitySystemComponent> AbilitySystem;
	UPROPERTY() TObjectPtr<UWRAttributeSet> Attributes;

	UPROPERTY(VisibleAnywhere, Category="Mesh") TObjectPtr<UStaticMeshComponent> BodyMesh;
	UPROPERTY(VisibleAnywhere, Category="Mesh") TObjectPtr<UStaticMeshComponent> FaceMarker;
	UPROPERTY(VisibleAnywhere, Category="Mesh") TObjectPtr<UStaticMeshComponent> WeaponMesh;

	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> BodyMaterial;
	FLinearColor BaseBodyColor = FLinearColor(0.6f, 0.6f, 0.65f);

private:
	UFUNCTION(NetMulticast, Unreliable) void MulticastHit(float Damage, EWRHitResult Result);
	UFUNCTION(NetMulticast, Reliable) void MulticastDied();

	UPROPERTY(ReplicatedUsing=OnRep_Swing) uint8 SwingSeq = 0;
	UPROPERTY(Replicated) EWRSwingKind SwingKindRep = EWRSwingKind::None;
	UPROPERTY(Replicated) float SwingDurationRep = 0.f;
	UFUNCTION() void OnRep_Swing();

	void StartSwingVisual(EWRSwingKind Kind, float Duration);
	void UpdateVisuals(float DeltaSeconds);

	EWRSwingKind VisualSwing = EWRSwingKind::None;
	float VisualSwingTime = 0.f;
	float VisualSwingDuration = 0.f;
	float HitFlash = 0.f;
};
