#pragma once

#include "CoreMinimal.h"
#include "WRCharacterBase.h"
#include "WRCombatTypes.h"
#include "WRCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/**
 * Player survivor. One pawn, two views (first/third person, V). Input is built in code
 * (no input assets) and routed to GAS by ability input id.
 */
UCLASS()
class WILDREACH_API AWRCharacter : public AWRCharacterBase
{
	GENERATED_BODY()

public:
	AWRCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category="Camera") bool IsFirstPerson() const { return bFirstPerson; }
	UFUNCTION(BlueprintCallable, Category="Camera") void SetFirstPerson(bool bNewFirstPerson);

	virtual void Tick(float DeltaSeconds) override;
	/** Standing on slick ice without ice resistance (set on the server and the owning client). */
	bool IsOnIce() const { return bOnIce; }

	/** Press/release an ability input (used by real input and by the test bot). */
	void PressAbilityInput(EWRAbilityInput Input);
	void ReleaseAbilityInput(EWRAbilityInput Input);

protected:
	virtual void BeginPlay() override;
	virtual void OnDeath(AActor* Killer) override;

	UPROPERTY(VisibleAnywhere, Category="Camera") TObjectPtr<USpringArmComponent> ThirdPersonBoom;
	UPROPERTY(VisibleAnywhere, Category="Camera") TObjectPtr<UCameraComponent> ThirdPersonCamera;
	UPROPERTY(VisibleAnywhere, Category="Camera") TObjectPtr<UCameraComponent> FirstPersonCamera;

	UPROPERTY(ReplicatedUsing=OnRep_FirstPerson) bool bFirstPerson = false;

private:
	void InitAbilityActorInfo();
	void ClearStaleTags();

	bool bOnIce = false;
	float DefaultFriction = 8.f;
	float DefaultBraking = 2048.f;
	float DefaultAcceleration = 2048.f;

	UFUNCTION() void OnRep_FirstPerson();
	UFUNCTION(Server, Reliable) void ServerSetFirstPerson(bool bNewFirstPerson);
	void ApplyViewMode();

	void BuildInput();
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void StartSprint();
	void StopSprint();
	void ToggleView();

	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> MappingContext;
	UPROPERTY(Transient) TObjectPtr<UInputAction> MoveAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> LookAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> JumpAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> SprintAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> ToggleViewAction;
	UPROPERTY(Transient) TMap<EWRAbilityInput, TObjectPtr<UInputAction>> AbilityActions;
};
