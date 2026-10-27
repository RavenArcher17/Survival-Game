#include "WRCharacterMovementComponent.h"
#include "WRAttributeSet.h"
#include "WRCharacterBase.h"
#include "WRCharacter.h"
#include "WRTuning.h"
#include "WRGameplayTags.h"

bool UWRCharacterMovementComponent::IsSprinting() const
{
	const AWRCharacterBase* WRChar = Cast<AWRCharacterBase>(CharacterOwner);
	if (!bWantsToSprint || bAttackSlow || bBlockSlow || !IsMovingOnGround() || !WRChar || !WRChar->GetAttributes())
	{
		return false;
	}
	return WRChar->GetAttributes()->GetStamina() > 1.f && WRChar->GetSpeedMultiplier() >= 1.f;
}

float UWRCharacterMovementComponent::GetMaxSpeed() const
{
	float Speed = IsSprinting() ? SprintSpeed : Super::GetMaxSpeed();
	if (MovementMode == MOVE_Falling)
	{
		return Speed;
	}
	if (bAttackSlow) Speed *= 0.35f;
	else if (bBlockSlow) Speed *= 0.5f;  // a swing out of a held block moves like any swing (the slows don't stack)
	if (const AWRCharacter* Survivor = Cast<AWRCharacter>(CharacterOwner); Survivor && Survivor->IsAimingRanged())
	{
		Speed *= WRTune(TEXT("ranged.aimSpeedMultiplier"), 0.5f);
	}
	const AWRCharacterBase* WRChar = Cast<AWRCharacterBase>(CharacterOwner);
	return WRChar ? Speed * WRChar->GetSpeedMultiplier() : Speed;
}

void UWRCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);
	bWantsToSprint = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
	bAttackSlow = (Flags & FSavedMove_Character::FLAG_Custom_1) != 0;
	bBlockSlow = (Flags & FSavedMove_Character::FLAG_Custom_2) != 0;
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
	bSavedAttackSlow = false;
	bSavedBlockSlow = false;
}

uint8 FSavedMove_WR::GetCompressedFlags() const
{
	uint8 Flags = Super::GetCompressedFlags();
	if (bSavedWantsToSprint) Flags |= FLAG_Custom_0;
	if (bSavedAttackSlow) Flags |= FLAG_Custom_1;
	if (bSavedBlockSlow) Flags |= FLAG_Custom_2;
	return Flags;
}

bool FSavedMove_WR::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const
{
	const FSavedMove_WR* Other = static_cast<const FSavedMove_WR*>(NewMove.Get());
	if (bSavedWantsToSprint != Other->bSavedWantsToSprint || bSavedAttackSlow != Other->bSavedAttackSlow || bSavedBlockSlow != Other->bSavedBlockSlow)
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
		bSavedAttackSlow = Move->bAttackSlow;
		bSavedBlockSlow = Move->bBlockSlow;
	}
}

void FSavedMove_WR::PrepMoveFor(ACharacter* C)
{
	Super::PrepMoveFor(C);
	if (UWRCharacterMovementComponent* Move = Cast<UWRCharacterMovementComponent>(C->GetCharacterMovement()))
	{
		Move->bWantsToSprint = bSavedWantsToSprint;
		Move->bAttackSlow = bSavedAttackSlow;
		Move->bBlockSlow = bSavedBlockSlow;
	}
}

FSavedMovePtr FNetworkPredictionData_Client_WR::AllocateNewMove()
{
	return FSavedMovePtr(new FSavedMove_WR());
}
