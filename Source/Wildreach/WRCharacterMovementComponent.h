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
	/** Set by heavy swings on the predicting machine: rooted (COMBAT-AND-LESSONS C2, the waist split's other half). */
	bool bAttackRoot = false;

	bool IsSprinting() const;
	/** Underwater (cooks/build-biome-ocean.md 2.7): Dive held sinks, Rise (Jump) held rises, neither holds your depth.
	 *  Both ride the saved move's two spare engine bits (FLAG_Reserved_1 / _2), so the server replays them exactly. */
	/** A sea creature that hangs at neutral buoyancy under the water (holds its depth; WROceanCreatures steers it). */
	bool bHoldDepth = false;
	bool bWantsToDive = false;
	bool bWantsToRise = false;
	/** Swimming with the capsule wholly under the surface. */
	bool IsUnderwater() const;
	/** cm of water over the feet (0 out of water): wading. */
	float WadeDepth() const;
	virtual void PhysSwimming(float DeltaTime, int32 Iterations) override;

	/** Custom movement modes (MOVE_Custom + this). */
	static constexpr uint8 CMOVE_Ladder = 1;
	/** On a siege ladder (WRRaiding): walk into one facing it and you climb it; back off or jump to drop; at the top you
	 *  step over onto the wall. Decided from the same input on the predicting client and the server, so it's predicted
	 *  like walking. */
	bool IsOnLadder() const { return MovementMode == MOVE_Custom && CustomMovementMode == CMOVE_Ladder; }
	TWeakObjectPtr<class AWRSiegeLadder> Ladder;
	/** Gliding (WRSkyAir, cooks/build-biome-sky.md 2.6): Jump while falling with 2 m of air under you and a glider in
	 *  the bag (or the Wind path's Path Glide, or Skysilk's four pieces) opens it; Jump again, landing, a hit, running out
	 *  of stamina or durability closes it. Decided from the same input and state on the predicting client and the
	 *  server, so it's predicted like walking. */
	static constexpr uint8 CMOVE_Glide = 2;
	bool IsGliding() const { return MovementMode == MOVE_Custom && CustomMovementMode == CMOVE_Glide; }
	/** Seconds this glide has carried you (Skysilk's 6 s limit; the reopen guard). */
	float GlideTime = 0.f;
	void PhysGlide(float DeltaTime, int32 Iterations);

	virtual void BeginPlay() override;
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual void PhysCustom(float DeltaTime, int32 Iterations) override;
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
	uint8 bSavedAttackRoot : 1;
	uint8 bSavedDive : 1;
	uint8 bSavedRise : 1;

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
