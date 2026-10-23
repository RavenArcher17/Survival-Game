#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WRSurvivalComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWROnDied);

/**
 * Server-authoritative survival stats. Only the server ticks and mutates them;
 * Health replicates to everyone, the rest only to the owning client.
 */
UCLASS(ClassGroup=(Survival), meta=(BlueprintSpawnableComponent))
class WILDREACH_API UWRSurvivalComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWRSurvivalComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Survival")
	void ApplyDamage(float Amount);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Survival")
	void Eat(float Amount);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Survival")
	void Drink(float Amount);

	/** Server: set each tick by the owner while stamina is being spent (sprinting). */
	void SetStaminaDraining(bool bDraining) { bStaminaDraining = bDraining; }

	UFUNCTION(BlueprintPure, Category="Survival") bool IsAlive() const { return Health > 0.f; }
	UFUNCTION(BlueprintPure, Category="Survival") bool HasStamina() const { return Stamina > 1.f; }

	UPROPERTY(BlueprintAssignable, Category="Survival")
	FWROnDied OnDied;

	UPROPERTY(EditDefaultsOnly, Category="Survival|Max") float MaxHealth = 100.f;
	UPROPERTY(EditDefaultsOnly, Category="Survival|Max") float MaxHunger = 100.f;
	UPROPERTY(EditDefaultsOnly, Category="Survival|Max") float MaxThirst = 100.f;
	UPROPERTY(EditDefaultsOnly, Category="Survival|Max") float MaxStamina = 100.f;

	/** Full -> empty in ~40 min. */
	UPROPERTY(EditDefaultsOnly, Category="Survival|Rates") float HungerPerSecond = 100.f / (40.f * 60.f);
	/** Full -> empty in ~25 min. */
	UPROPERTY(EditDefaultsOnly, Category="Survival|Rates") float ThirstPerSecond = 100.f / (25.f * 60.f);
	/** Health lost per second for each of hunger/thirst sitting at zero. */
	UPROPERTY(EditDefaultsOnly, Category="Survival|Rates") float DeprivationDamagePerSecond = 1.f;
	UPROPERTY(EditDefaultsOnly, Category="Survival|Rates") float StaminaDrainPerSecond = 20.f;
	UPROPERTY(EditDefaultsOnly, Category="Survival|Rates") float StaminaRegenPerSecond = 12.f;
	UPROPERTY(EditDefaultsOnly, Category="Survival|Rates") float StaminaRegenDelay = 1.f;

	UPROPERTY(ReplicatedUsing=OnRep_Health, BlueprintReadOnly, Category="Survival") float Health = 100.f;
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Survival") float Hunger = 100.f;
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Survival") float Thirst = 100.f;
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Survival") float Stamina = 100.f;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void OnRep_Health(float OldHealth);

	bool bStaminaDraining = false;
	float TimeSinceStaminaUse = 0.f;
};
