#include "WRCharacterMovementComponent.h"
#include "WRRaiding.h"
#include "WRCharacterBase.h"
#include "WRGameplayTags.h"
#include "WRTuning.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "WRAttributeSet.h"
#include "WRCharacterBase.h"
#include "WRCharacter.h"
#include "WRTuning.h"
#include "WRGameplayTags.h"
#include "WRLifeSkills.h"

bool UWRCharacterMovementComponent::IsSprinting() const
{
	const AWRCharacterBase* WRChar = Cast<AWRCharacterBase>(CharacterOwner);
	// On the ground, or in the water: Sprint there is the fast stroke (WRWater), with the same stamina rule.
	if (!bWantsToSprint || bAttackSlow || bBlockSlow || bAttackRoot || !(IsMovingOnGround() || IsSwimming()) || !WRChar || !WRChar->GetAttributes())
	{
		return false;
	}
	return WRChar->GetAttributes()->GetStamina() > 0.f && WRChar->GetSpeedMultiplier() >= 1.f;
}

void UWRCharacterMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	// Swimming in the sea (WRWater): how high a body floats (1 = just under, more = higher) and how fast it swims; the
	// water's drag stops it (any braking would also stop a body under the surface from floating up, since the engine
	// zeroes slow speeds under braking). Same numbers on every machine, so prediction agrees.
	Buoyancy = WRTune(TEXT("water.buoyancy"), 1.6f);
	MaxSwimSpeed = WRTune(TEXT("water.swimSpeed"), 300.f);
	BrakingDecelerationSwimming = WRTune(TEXT("water.braking"), 0.f);
}

float UWRCharacterMovementComponent::GetMaxSpeed() const
{
	if (IsOnLadder())
	{
		return WRTune(TEXT("raiding.ladder.climbSpeed"), 180.f);
	}
	if (IsSwimming())
	{
		// Swimming: the swimming skill's every-level speed, and Sprint is the fast stroke. The skills replicate to the
		// owner, so its prediction and the server agree.
		float Swim = MaxSwimSpeed * (1.f + UWRLifeSkillsComponent::StatFor(CharacterOwner, TEXT("swimSpeed")));
		if (IsSprinting()) Swim *= WRTune(TEXT("water.fastSwimMultiplier"), 1.6f);
		const AWRCharacterBase* Swimmer = Cast<AWRCharacterBase>(CharacterOwner);
		return Swimmer ? Swim * Swimmer->GetSpeedMultiplier() : Swim;
	}
	float Speed = IsSprinting() ? SprintSpeed : Super::GetMaxSpeed();
	if (MovementMode == MOVE_Falling)
	{
		return Speed;
	}
	// Lights keep the legs walking (combat.lightMoveMultiplier); heavies root you (COMBAT-AND-LESSONS C2).
	if (bAttackRoot) Speed = 0.f;
	else if (bAttackSlow) Speed *= WRTune(TEXT("combat.lightMoveMultiplier"), 0.35f);
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
	bAttackRoot = (Flags & FSavedMove_Character::FLAG_Custom_3) != 0;
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
	bSavedAttackRoot = false;
}

uint8 FSavedMove_WR::GetCompressedFlags() const
{
	uint8 Flags = Super::GetCompressedFlags();
	if (bSavedWantsToSprint) Flags |= FLAG_Custom_0;
	if (bSavedAttackSlow) Flags |= FLAG_Custom_1;
	if (bSavedBlockSlow) Flags |= FLAG_Custom_2;
	if (bSavedAttackRoot) Flags |= FLAG_Custom_3;
	return Flags;
}

void UWRCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
	if (IsOnLadder() || !CharacterOwner || !(IsMovingOnGround() || IsFalling()))
	{
		return;
	}
	const FVector Accel = GetCurrentAcceleration();
	if (Accel.SizeSquared2D() < 1.f)
	{
		return;
	}
	// A ladder whose lane we're on, that we're pushing into (toward the wall): climb it.
	const float Radius = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const FVector Feet = UpdatedComponent->GetComponentLocation() - FVector(0.f, 0.f, CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	for (TActorIterator<AWRSiegeLadder> It(GetWorld()); It; ++It)
	{
		float Along = 0.f;
		if (!It->OnLane(Feet, Radius, Along) || Along > 0.9f) continue;
		if (FVector::DotProduct(Accel.GetSafeNormal2D(), (-It->Out()).GetSafeNormal2D()) < 0.6f) continue;
		Ladder = *It;
		Velocity = FVector::ZeroVector;
		SetMovementMode(MOVE_Custom, CMOVE_Ladder);
		return;
	}
}

void UWRCharacterMovementComponent::PhysCustom(float DeltaTime, int32 Iterations)
{
	if (CustomMovementMode != CMOVE_Ladder)
	{
		Super::PhysCustom(DeltaTime, Iterations);
		return;
	}
	AWRSiegeLadder* L = Ladder.Get();
	const float Radius = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const float Half = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Loc = UpdatedComponent->GetComponentLocation();
	const FVector Feet = Loc - FVector(0.f, 0.f, Half);
	float Along = 0.f;
	const AWRCharacterBase* Who = Cast<AWRCharacterBase>(CharacterOwner);
	// Knocked off (a stagger, a freeze), the ladder gone, or off its lane: fall.
	if (!L || !L->OnLane(Feet, Radius, Along) || (Who && (Who->HasTag(WRTags::Status_Staggered) || Who->HasTag(WRTags::Status_Frozen))))
	{
		Ladder = nullptr;
		SetMovementMode(MOVE_Falling);
		StartNewPhysics(DeltaTime, Iterations);
		return;
	}
	const FVector Up = L->Up();
	const FVector Into = (-L->Out()).GetSafeNormal2D();
	if (CharacterOwner->bPressedJump)
	{
		// Jump off backwards.
		Ladder = nullptr;
		Velocity = -Into * 350.f + FVector(0.f, 0.f, 250.f);
		CharacterOwner->bPressedJump = false;
		SetMovementMode(MOVE_Falling);
		StartNewPhysics(DeltaTime, Iterations);
		return;
	}
	const FVector Accel = GetCurrentAcceleration();
	const float Push = Accel.SizeSquared2D() > 1.f ? FVector::DotProduct(Accel.GetSafeNormal2D(), Into) : 0.f;
	const float Speed = WRTune(TEXT("raiding.ladder.climbSpeed"), 180.f);
	const float Climb = Push > 0.3f ? 1.f : Push < -0.3f ? -1.f : 0.f;
	// At the top: a hop forward over the hooks onto the wall's top or the walkway behind it.
	if (Climb > 0.f && Along >= 0.96f)
	{
		Ladder = nullptr;
		const float Hop = WRTune(TEXT("raiding.ladder.topHopCm"), 120.f);
		Velocity = Into * (Hop / 0.35f) + FVector(0.f, 0.f, 420.f);
		SetMovementMode(MOVE_Falling);
		StartNewPhysics(DeltaTime, Iterations);
		return;
	}
	// At the bottom, climbing down: step off onto the ground.
	if (Climb < 0.f && Along <= 0.02f)
	{
		Ladder = nullptr;
		SetMovementMode(MOVE_Walking);
		StartNewPhysics(DeltaTime, Iterations);
		return;
	}
	// Along the rails, held on the lane in front of the rungs (eased to it, never snapped).
	const FVector Side = L->GetActorRightVector();
	const FVector Face = FVector::CrossProduct(Side, Up).GetSafeNormal();
	const FVector Ideal = L->Foot() + Up * (Along * L->Length) + Face * (Radius + 8.f) + FVector(0.f, 0.f, Half);
	const FVector Correct = (Ideal - Loc) * FMath::Min(1.f, 10.f * DeltaTime);
	Velocity = Up * Speed * Climb;
	const FVector Delta = Velocity * DeltaTime + Correct;
	FHitResult Hit;
	SafeMoveUpdatedComponent(Delta, Into.Rotation().Quaternion(), true, Hit);
	if (Hit.IsValidBlockingHit())
	{
		SlideAlongSurface(Delta, 1.f - Hit.Time, Hit.Normal, Hit, true);
	}
}

bool FSavedMove_WR::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const
{
	const FSavedMove_WR* Other = static_cast<const FSavedMove_WR*>(NewMove.Get());
	if (bSavedWantsToSprint != Other->bSavedWantsToSprint || bSavedAttackSlow != Other->bSavedAttackSlow || bSavedBlockSlow != Other->bSavedBlockSlow || bSavedAttackRoot != Other->bSavedAttackRoot)
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
		bSavedAttackRoot = Move->bAttackRoot;
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
		Move->bAttackRoot = bSavedAttackRoot;
	}
}

FSavedMovePtr FNetworkPredictionData_Client_WR::AllocateNewMove()
{
	return FSavedMovePtr(new FSavedMove_WR());
}
