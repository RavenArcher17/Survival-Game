#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "WRCharacterMovementComponent.generated.h"

/**
 * Adds client-predicted sprinting: the sprint flag rides in the saved-move
 * compressed flags, so the server replays it and doesn't rubber-band the client.
 */
UCLASS()
class WILDREACH_API UWRCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category="Character Movement: Sprint")
	float SprintSpeed = 750.f;

	bool bWantsToSprint = false;

	bool IsSprinting() const;

	virtual float GetMaxSpeed() const override;
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
};

class FSavedMove_WR : public FSavedMove_Character
{
public:
	typedef FSavedMove_Character Super;

	uint8 bSavedWantsToSprint : 1;

	virtual void Clear() override;
	virtual uint8 GetCompressedFlags() const override;
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override;
	virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override;
	virtual void PrepMoveFor(ACharacter* C) override;
};

class FNetworkPredictionData_Client_WR : public FNetworkPredictionData_Client_Character
{
public:
	typedef FNetworkPredictionData_Client_Character Super;

	explicit FNetworkPredictionData_Client_WR(const UCharacterMovementComponent& ClientMovement) : Super(ClientMovement) {}

	virtual FSavedMovePtr AllocateNewMove() override;
};
