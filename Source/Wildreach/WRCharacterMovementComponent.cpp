#include "WRCharacterMovementComponent.h"
#include "WRCharacter.h"

bool UWRCharacterMovementComponent::IsSprinting() const
{
	const AWRCharacter* WRChar = Cast<AWRCharacter>(CharacterOwner);
	return bWantsToSprint && IsMovingOnGround() && WRChar && WRChar->CanSprint();
}

float UWRCharacterMovementComponent::GetMaxSpeed() const
{
	return IsSprinting() ? SprintSpeed : Super::GetMaxSpeed();
}

void UWRCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);
	bWantsToSprint = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
}

FNetworkPredictionData_Client* UWRCharacterMovementComponent::GetPredictionData_Client() const
{
	if (ClientPredictionData == nullptr)
	{
		UWRCharacterMovementComponent* MutableThis = const_cast<UWRCharacterMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_WR(*this);
	}
	return ClientPredictionData;
}

void FSavedMove_WR::Clear()
{
	Super::Clear();
	bSavedWantsToSprint = false;
}

uint8 FSavedMove_WR::GetCompressedFlags() const
{
	uint8 Flags = Super::GetCompressedFlags();
	if (bSavedWantsToSprint)
	{
		Flags |= FLAG_Custom_0;
	}
	return Flags;
}

bool FSavedMove_WR::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const
{
	if (bSavedWantsToSprint != static_cast<const FSavedMove_WR*>(NewMove.Get())->bSavedWantsToSprint)
	{
		return false;
	}
	return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
}

void FSavedMove_WR::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);
	if (const UWRCharacterMovementComponent* Move = Cast<UWRCharacterMovementComponent>(C->GetCharacterMovement()))
	{
		bSavedWantsToSprint = Move->bWantsToSprint;
	}
}

void FSavedMove_WR::PrepMoveFor(ACharacter* C)
{
	Super::PrepMoveFor(C);
	if (UWRCharacterMovementComponent* Move = Cast<UWRCharacterMovementComponent>(C->GetCharacterMovement()))
	{
		Move->bWantsToSprint = bSavedWantsToSprint;
	}
}

FSavedMovePtr FNetworkPredictionData_Client_WR::AllocateNewMove()
{
	return FSavedMovePtr(new FSavedMove_WR());
}
