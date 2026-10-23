#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "WRStatusComponent.generated.h"

class AWRCharacterBase;

/**
 * Server-side upkeep for one character: status effects and their timers, buildup meters,
 * stamina/poise regen, Heat/Chill decay, hunger and thirst. Statuses are replicated loose
 * gameplay tags on the owner's ASC, so every client can read them.
 */
UCLASS(ClassGroup=(Combat))
class WILDREACH_API UWRStatusComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWRStatusComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Players need food and water; creatures don't. */
	UPROPERTY(EditAnywhere, Category="Status") bool bSurvivalNeeds = false;

	// --- Server API ---
	void ApplyStatus(const FGameplayTag& Status, float Duration);
	void ClearStatus(const FGameplayTag& Status);
	bool HasStatus(const FGameplayTag& Status) const;
	void Stagger(float Duration);

	/** Burn/frost buildup from a hit. A full meter triggers the status. */
	void AddBuildup(const FGameplayTag& Element, float Amount, AWRCharacterBase* Source);

	void NotifyStaminaUsed() { SinceStaminaUse = 0.f; }
	void NotifyPoiseDamaged() { SincePoiseDamage = 0.f; }
	void NotifyResourceGained(const FGameplayTag& Element);

	/** Set every tick by the owner while it is sprinting and moving. */
	void SetSprintDraining(bool bDraining) { bSprintDraining = bDraining; }

	/** Health drains once hunger or thirst has sat at zero for the grace period. */
	bool IsDeprived() const;

	/** Clears every status and meter (used on respawn). */
	void ResetAll();

private:
	AWRCharacterBase* GetOwnerCharacter() const;
	void TickBuildup(float DeltaTime);
	void TickStatuses(float DeltaTime);
	void TickRegen(float DeltaTime);
	void TickSurvival(float DeltaTime);

	/** Status tag -> world time it ends. */
	TMap<FGameplayTag, double> StatusEnds;

	TWeakObjectPtr<AWRCharacterBase> BurnSource;
	float BurnTickAccumulator = 0.f;

	float SinceStaminaUse = 10.f;
	float SincePoiseDamage = 10.f;
	float SinceHeatGain = 10.f;
	float SinceChillGain = 10.f;
	float SinceBuildup = 10.f;
	float HungerEmptyFor = 0.f;
	float ThirstEmptyFor = 0.f;
	bool bSprintDraining = false;
};
