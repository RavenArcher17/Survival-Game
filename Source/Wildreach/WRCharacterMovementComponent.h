#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "WRCharacterMovementComponent.generated.h"

/**
 * Client-predicted movement extras. Sprint and the action slowdowns (attacking, blocking)
 * ride in the saved-move compressed flags, so the server replays them on exactly the same
 * moves the client applied them to and nobody rubber-bands.
 * Server-driven statuses (slowed, frozen, staggered) come from replicated tags instead.
 */
UCLASS()
class WILDREACH_API UWRCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category="Character Movement: Sprint")
	float SprintSpeed = 750.f;

	bool bWantsToSprint = false;
	/** Set by attack/spell abilities on the predicting machine. */
	bool bAttackSlow = false;
	/** Set by the block ability on the predicting machine. */
	bool bBlockSlow = false;

	bool IsSprinting() const;

	virtual void BeginPlay() override;
	virtual float GetMaxSpeed() const override;
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
};

class FSavedMove_WR : public FSavedMove_Character
{
public:
	typedef FSavedMove_Character Super;

	uint8 bSavedWantsToSprint : 1;
	uint8 bSavedAttackSlow : 1;
	uint8 bSavedBlockSlow : 1;

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
