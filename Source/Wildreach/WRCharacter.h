#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "WRCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class UInputAction;
class UInputMappingContext;
class UWRSurvivalComponent;
struct FInputActionValue;

/**
 * Player survivor. One pawn, two views (first/third person, toggled with V).
 * Input is built in code (no input assets) so the project stays text-diffable.
 */
UCLASS()
class WILDREACH_API AWRCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AWRCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category="Survival")
	UWRSurvivalComponent* GetSurvival() const { return Survival; }

	UFUNCTION(BlueprintPure, Category="Camera")
	bool IsFirstPerson() const { return bFirstPerson; }

	bool CanSprint() const;

	UFUNCTION(BlueprintCallable, Category="Camera")
	void SetFirstPerson(bool bNewFirstPerson);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category="Camera") TObjectPtr<USpringArmComponent> ThirdPersonBoom;
	UPROPERTY(VisibleAnywhere, Category="Camera") TObjectPtr<UCameraComponent> ThirdPersonCamera;
	UPROPERTY(VisibleAnywhere, Category="Camera") TObjectPtr<UCameraComponent> FirstPersonCamera;

	/** Placeholder body until we have a real skeletal mesh. */
	UPROPERTY(VisibleAnywhere, Category="Mesh") TObjectPtr<UStaticMeshComponent> BodyMesh;
	UPROPERTY(VisibleAnywhere, Category="Mesh") TObjectPtr<UStaticMeshComponent> FaceMarker;

	UPROPERTY(VisibleAnywhere, Category="Survival") TObjectPtr<UWRSurvivalComponent> Survival;

	/** Replicated so the server (and other clients) use the right rotation mode. */
	UPROPERTY(ReplicatedUsing=OnRep_FirstPerson) bool bFirstPerson = false;

private:
	UFUNCTION() void OnRep_FirstPerson();
	UFUNCTION(Server, Reliable) void ServerSetFirstPerson(bool bNewFirstPerson);
	void ApplyViewMode();

	UFUNCTION() void HandleDied();

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
};
