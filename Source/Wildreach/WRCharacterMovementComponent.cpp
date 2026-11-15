#include "WRCharacterMovementComponent.h"
#include "WROceanHazards.h"
#include "WRVoid.h"
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
#include "WRSkyAir.h"
#include "WRSkyIsleTypes.h"
#include "WRUnderwater.h"
#include "GameFramework/PhysicsVolume.h"
#include "WRInventory.h"

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
	if (IsGliding())
	{
		FWRGlideParams P;
		return WRSkyAir::GlideParams(CharacterOwner, P) ? P.Forward * 1.5f : 1000.f;
	}
	if (IsOnLadder())
	{
		return WRTune(TEXT("raiding.ladder.climbSpeed"), 180.f);
	}
	if (IsSwimming())
	{
		// Swimming: the swimming skill's every-level speed, and Sprint is the fast stroke. The skills replicate to the
		// owner, so its prediction and the server agree. Under the surface (cooks/biome-ocean.md 2.6): 300, fast 450;
		// Gills x1.25 (the Tide root passive); swim fins and the Tyrant fin add (worn gear, replicated to the owner).
		float Swim = MaxSwimSpeed * (1.f + UWRLifeSkillsComponent::StatFor(CharacterOwner, TEXT("swimSpeed")) + UWRInventoryComponent::GearStatFor(CharacterOwner, TEXT("swimSpeed")));
		if (IsSprinting()) Swim *= IsUnderwater() ? WRTune(TEXT("underwater.fastSwim"), 450.f) / FMath::Max(1.f, WRTune(TEXT("underwater.swimSpeed"), 300.f)) : WRTune(TEXT("water.fastSwimMultiplier"), 1.6f);
		if (IsUnderwater() && WRUnderwater::HasGills(CharacterOwner)) Swim *= WRTune(TEXT("underwater.gillsSpeed"), 1.25f);
		const AWRCharacterBase* Swimmer = Cast<AWRCharacterBase>(CharacterOwner);
		return Swimmer ? Swim * Swimmer->GetSpeedMultiplier() : Swim;
	}
	float Speed = IsSprinting() ? SprintSpeed : Super::GetMaxSpeed();
	if (MovementMode == MOVE_Falling)
	{
		return Speed;
	}
	// Wading (cooks/biome-ocean.md 2.6): water 60-140 cm deep at the feet slows a run to x0.85, and over 110 cm there's
	// no sprinting. Worn swim fins slow you on land. Both from the same data on both machines.
	if (IsMovingOnGround())
	{
		const float Wade = WadeDepth();
		if (Wade >= WRTune(TEXT("underwater.wadeFrom"), 60.f))
		{
			if (Wade > WRTune(TEXT("underwater.noSprintOver"), 110.f)) Speed = FMath::Min(Speed, Super::GetMaxSpeed());
			Speed *= WRTune(TEXT("underwater.wadeRun"), 0.85f);
		}
		Speed *= 1.f + UWRInventoryComponent::GearStatFor(CharacterOwner, TEXT("landSpeed"));
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
	bWantsToDive = (Flags & FSavedMove_Character::FLAG_Reserved_1) != 0;
	bWantsToRise = (Flags & FSavedMove_Character::FLAG_Reserved_2) != 0;
}

bool UWRCharacterMovementComponent::IsUnderwater() const
{
	return IsSwimming() && ImmersionDepth() >= 0.999f;
}

float UWRCharacterMovementComponent::WadeDepth() const
{
	// Standing in water: how deep it is over the feet (the water this capsule is in, at its feet).
	const APhysicsVolume* V = GetPhysicsVolume();
	if (!CharacterOwner || !V || !V->bWaterVolume) return 0.f;
	const FVector Feet = UpdatedComponent->GetComponentLocation() - FVector(0.f, 0.f, CharacterOwner->GetSimpleCollisionHalfHeight());
	return FMath::Max(0.f, WRUnderwater::SurfaceZ(V, Feet) - float(Feet.Z));
}

void UWRCharacterMovementComponent::PhysSwimming(float DeltaTime, int32 Iterations)
{
	// Underwater (cooks/biome-ocean.md 2.6): wholly under, a body hangs at neutral buoyancy, so it holds its depth;
	// Dive sinks it, Rise (Jump) lifts it. At the surface it floats as before unless Dive pulls it under. The input's
	// own pitch (Move swims where you look under the surface) rides in Acceleration, which the engine keeps in 3D while
	// swimming. Survivors only: creatures and mounts keep their own buoyancy.
	const float Saved = Buoyancy;
	if (CharacterOwner && (CharacterOwner->GetPlayerState() || bHoldDepth))
	{
		const bool bUnder = ImmersionDepth() >= 0.999f;
		if (bWantsToRise) Buoyancy = WRTune(TEXT("underwater.riseBuoyancy"), 1.35f);
		else if (bUnder || bWantsToDive) Buoyancy = WRTune(TEXT("underwater.neutralBuoyancy"), 1.f);
		const float Vertical = (bWantsToDive ? -1.f : 0.f) + (bWantsToRise ? 1.f : 0.f);
		if (Vertical != 0.f)
		{
			const float MaxA = GetMaxAcceleration();
			Acceleration.Z = FMath::Clamp(Acceleration.Z + Vertical * MaxA, -MaxA, MaxA);
			Acceleration = Acceleration.GetClampedToMaxSize(MaxA);
		}
	}
	Super::PhysSwimming(DeltaTime, Iterations);
	Buoyancy = Saved;
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
	bSavedDive = false;
	bSavedRise = false;
}

uint8 FSavedMove_WR::GetCompressedFlags() const
{
	uint8 Flags = Super::GetCompressedFlags();
	if (bSavedWantsToSprint) Flags |= FLAG_Custom_0;
	if (bSavedAttackSlow) Flags |= FLAG_Custom_1;
	if (bSavedBlockSlow) Flags |= FLAG_Custom_2;
	if (bSavedAttackRoot) Flags |= FLAG_Custom_3;
	if (bSavedDive) Flags |= FLAG_Reserved_1;
	if (bSavedRise) Flags |= FLAG_Reserved_2;
	return Flags;
}

void UWRCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
	// The Ocean's rips (WROceanHazards): a seaward current on a swimmer, a pure function of the tide and where you swim,
	// so the owner and the server move the same. Survivors only (a mount's rider rides; creatures know the water).
	if (CharacterOwner && IsSwimming() && CharacterOwner->GetPlayerState() && CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)
	{
		FVector Current = UWROceanHazards::CurrentAt(CharacterOwner, UpdatedComponent->GetComponentLocation());
		if (!Current.IsNearlyZero() && UWRLifeSkillsComponent::StatFor(CharacterOwner, TEXT("currentRider")) > 0.f)
		{
			// Current rider (Swimming 75): the water carries you half again as fast, and pushes half as hard against you.
			Current *= FVector::DotProduct(GetCurrentAcceleration().GetSafeNormal2D(), Current.GetSafeNormal()) >= 0.f ? 1.5f : 0.5f;
		}
		if (!Current.IsNearlyZero())
		{
			FHitResult CurHit;
			SafeMoveUpdatedComponent(Current * DeltaSeconds, UpdatedComponent->GetComponentQuat(), true, CurHit);
			if (CurHit.IsValidBlockingHit()) SlideAlongSurface(Current * DeltaSeconds, 1.f - CurHit.Time, CurHit.Normal, CurHit, true);
		}
	}
	if (IsOnLadder() || !CharacterOwner || !(IsMovingOnGround() || IsFalling() || IsGliding()))
	{
		return;
	}
	// The Sky's air (WRSkyAir): gusts on their lanes, a Gale's gusts, a lee rotor's down-gust. Pure functions of the
	// server time and where you stand, so the server and the owning client push the same and it never corrects. Survivors
	// on both machines; creatures on the server only (their clients just watch).
	const bool bOwnerOrServer = CharacterOwner->GetPlayerState() ? CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy : CharacterOwner->HasAuthority();
	if (bOwnerOrServer && WRSkyIsles::Contains(UpdatedComponent->GetComponentLocation()))
	{
		const FVector Push = WRSkyAir::WindPushAt(CharacterOwner, UpdatedComponent->GetComponentLocation(), IsMovingOnGround());
		if (!Push.IsNearlyZero())
		{
			FHitResult PushHit;
			SafeMoveUpdatedComponent(Push * DeltaSeconds, UpdatedComponent->GetComponentQuat(), true, PushHit);
			if (PushHit.IsValidBlockingHit()) SlideAlongSurface(Push * DeltaSeconds, 1.f - PushHit.Time, PushHit.Normal, PushHit, true);
		}
	}
	// Open a glide: Jump pressed while already falling (not the press that jumped), 2 m of air under the feet.
	if (IsFalling() && CharacterOwner->GetPlayerState() && CharacterOwner->bPressedJump && !CharacterOwner->bWasJumping)
	{
		FWRGlideParams Glide;
		if (WRSkyAir::GlideParams(CharacterOwner, Glide))
		{
			const float Half = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			const FVector Feet = UpdatedComponent->GetComponentLocation() - FVector(0.f, 0.f, Half);
			FHitResult Under;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(WRGlideOpen), false, CharacterOwner);
			const float Need = WRTune(TEXT("glide.openAbove"), 200.f);
			if (!GetWorld()->LineTraceSingleByChannel(Under, Feet, Feet - FVector(0.f, 0.f, Need), ECC_Visibility, Q))
			{
				GlideTime = 0.f;
				SetMovementMode(MOVE_Custom, CMOVE_Glide);
				return;
			}
		}
	}
	if (IsGliding())
	{
		return;
	}
	// A rift's pull (WRVoid): a drift toward the tear, the same on the server and the owning client (a pure function of
	// where you stand), so it never corrects. Survivors only: void creatures don't feel it, tames brace.
	if (CharacterOwner->GetPlayerState() && CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)
	{
		const FVector Pull = WRVoid::PullAt(GetWorld(), UpdatedComponent->GetComponentLocation());
		if (!Pull.IsNearlyZero())
		{
			FHitResult PullHit;
			SafeMoveUpdatedComponent(Pull * DeltaSeconds, UpdatedComponent->GetComponentQuat(), true, PullHit);
		}
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
	if (CustomMovementMode == CMOVE_Glide)
	{
		PhysGlide(DeltaTime, Iterations);
		return;
	}
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
	if (bSavedWantsToSprint != Other->bSavedWantsToSprint || bSavedAttackSlow != Other->bSavedAttackSlow || bSavedBlockSlow != Other->bSavedBlockSlow || bSavedAttackRoot != Other->bSavedAttackRoot || bSavedDive != Other->bSavedDive || bSavedRise != Other->bSavedRise)
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
		bSavedDive = Move->bWantsToDive;
		bSavedRise = Move->bWantsToRise;
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
		Move->bWantsToDive = bSavedDive;
		Move->bWantsToRise = bSavedRise;
	}
}

FSavedMovePtr FNetworkPredictionData_Client_WR::AllocateNewMove()
{
	return FSavedMovePtr(new FSavedMove_WR());
}

void UWRCharacterMovementComponent::PhysGlide(float DeltaTime, int32 Iterations)
{
	if (DeltaTime < MIN_TICK_TIME || !CharacterOwner)
	{
		return;
	}
	FWRGlideParams G;
	const AWRCharacterBase* Who = Cast<AWRCharacterBase>(CharacterOwner);
	const UWRAttributeSet* Attr = Who ? Who->GetAttributes() : nullptr;
	GlideTime += DeltaTime;
	const FVector Loc = UpdatedComponent->GetComponentLocation();
	const float Lift = WRSkyIsles::LiftAt(GetWorld(), Loc);
	// Close: no glider any more (torn, dropped), Skysilk's seconds up, out of stamina outside a column, a stagger or a
	// freeze, or Jump pressed again (not the press that opened it).
	const bool bWantsClose = !WRSkyAir::GlideParams(CharacterOwner, G)
		|| (G.MaxSeconds > 0.f && GlideTime > G.MaxSeconds)
		|| (G.StaminaPerSecond > 0.f && Lift <= 50.f && Attr && Attr->GetStamina() <= 0.f)
		|| (Who && (Who->HasTag(WRTags::Status_Staggered) || Who->HasTag(WRTags::Status_Frozen)))
		|| (CharacterOwner->bPressedJump && GlideTime > WRTune(TEXT("glide.closeGuard"), 0.25f));
	if (bWantsClose)
	{
		CharacterOwner->bPressedJump = false;
		SetMovementMode(MOVE_Falling);
		StartNewPhysics(DeltaTime, Iterations);
		return;
	}
	// Heading: the move input steers (the camera's frame, as on foot), turning at the glider's rate; speed eases to its
	// forward speed (a push or a dive's extra speed bleeds off, a slow entry picks up).
	FVector2D H(Velocity.X, Velocity.Y);
	FVector2D Dir = H.SizeSquared() > 100.f ? H.GetSafeNormal() : FVector2D(UpdatedComponent->GetForwardVector());
	const FVector Accel = GetCurrentAcceleration();
	if (Accel.SizeSquared2D() > 1.f)
	{
		const FVector2D Want = FVector2D(Accel).GetSafeNormal();
		const float Cur = FMath::Atan2(Dir.Y, Dir.X), Tgt = FMath::Atan2(Want.Y, Want.X);
		const float Delta = FMath::FindDeltaAngleRadians(Cur, Tgt);
		const float Step = FMath::DegreesToRadians(G.TurnRate) * DeltaTime;
		const float A = Cur + FMath::Clamp(Delta, -Step, Step);
		Dir = FVector2D(FMath::Cos(A), FMath::Sin(A));
	}
	const float Speed = FMath::FInterpTo(H.Size(), G.Forward, DeltaTime, WRTune(TEXT("glide.speedEase"), 1.6f));
	// Drift on the Sky's wind (x0.15 crude, x0.1 levistone), the rotors' shove, the sink and the lift.
	FVector Shove = FVector::ZeroVector;
	const float ExtraSink = WRSkyIsles::SinkAt(GetWorld(), Loc, Shove);
	const FVector2D Wind = WRSkyIsles::Contains(Loc) ? WRSkyIsles::WindDir(GetWorld()) * WRTune(TEXT("glide.windSpeed"), 800.f) * WRSkyIsles::WindStrength(GetWorld()) * G.Drift : FVector2D::ZeroVector;
	const float WantZ = -G.Sink - ExtraSink + Lift;
	const float Vz = FMath::FInterpTo(Velocity.Z, WantZ, DeltaTime, WRTune(TEXT("glide.verticalEase"), 2.5f));
	const FVector2D Horizontal = Dir * Speed;
	Velocity = FVector(Horizontal.X, Horizontal.Y, Vz);
	const FVector Move = (Velocity + FVector(Wind.X, Wind.Y, 0.f) + Shove) * DeltaTime;
	const FQuat Face = FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)), 0.f).Quaternion();
	FHitResult Hit;
	SafeMoveUpdatedComponent(Move, FQuat::Slerp(UpdatedComponent->GetComponentQuat(), Face, FMath::Min(1.f, 8.f * DeltaTime)), true, Hit);
	if (Hit.IsValidBlockingHit())
	{
		// Touching down on walkable ground ends the glide (the cloth folds as you land).
		if (IsValidLandingSpot(UpdatedComponent->GetComponentLocation(), Hit))
		{
			ProcessLanded(Hit, DeltaTime * (1.f - Hit.Time), Iterations);
			return;
		}
		SlideAlongSurface(Move, 1.f - Hit.Time, Hit.Normal, Hit, true);
	}
}
