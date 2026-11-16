// Hands-on crafting on the body (cooks/cookpass-crafting.md 3.1, 3.3), on every machine that draws, from the replicated
// AWRCharacter::Work: the pose layer's input (the beat's cycle, eased in and out), the tools in the hands (the hand hammer
// and tongs at the forge, a lighter hammer at the armorer, a plane on the bench, a pestle in the mortar, a hammerstone and
// flint core or a hank of fibre by hand), the beat's sound and fx where the blow lands, and on your own machine the walk to
// the station's work spot (real locomotion at a walk; your own stick takes over) and the turn to face the work.
#include "WRCharacter.h"
#include "WRCraftWork.h"
#include "WRCrafting.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRHeroAnimInstance.h"
#include "WRSound.h"
#include "WRToolMesh.h"
#include "WRTuning.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ProceduralMeshComponent.h"

namespace WRCraftWorkLookDetail
{
	const FLinearColor Ash(0.36f, 0.24f, 0.13f);
	const FLinearColor Iron(0.07f, 0.07f, 0.075f);
	const FLinearColor Flint(0.11f, 0.11f, 0.12f);
	const FLinearColor Beech(0.47f, 0.31f, 0.17f);
	const FLinearColor StoneGrey(0.3f, 0.29f, 0.27f);

	/** A box of rings along Y (the head of a hammer): each ring a rectangle (half X, half Z) round (CX, Y, 0). */
	TArray<FVector> Rect(float CX, float Y, float HX, float HZ)
	{
		return { FVector(CX - HX, Y, -HZ), FVector(CX + HX, Y, -HZ), FVector(CX + HX, Y, HZ), FVector(CX - HX, Y, HZ) };
	}

	/** The smith's hand hammer in fist space (X out of the fist to the head, Y the striking side): an ash haft, a forged
	 *  head with a square face toward +Y and a cross peen behind. Scale 0.8 is the armourer's planishing hammer. */
	FWRToolMesh Hammer(float S)
	{
		FWRToolMesh M;
		M.Lathe(FVector(-6.f * S, 0.f, 0.f), FVector::ForwardVector, 36.f * S, { FVector(0.f, 1.6f * S, 1.4f * S), FVector(0.3f, 1.5f * S, 1.3f * S), FVector(0.8f, 1.2f * S, 1.1f * S), FVector(1.f, 1.3f * S, 1.2f * S) }, 10, Ash, 0.75f);
		const float CX = 28.f * S;
		M.Loft({ Rect(CX, -7.f * S, 2.4f * S, 0.6f * S), Rect(CX, -4.f * S, 2.6f * S, 2.2f * S), Rect(CX, 0.f, 2.6f * S, 2.6f * S),
			Rect(CX, 5.f * S, 2.5f * S, 2.5f * S), Rect(CX, 8.f * S, 2.6f * S, 2.6f * S) }, Iron, 0.45f);
		return M;
	}

	/** A round hammerstone in the palm (fist space). */
	FWRToolMesh Hammerstone()
	{
		FWRToolMesh M;
		M.Lathe(FVector(-3.f, 2.f, 0.f), FVector::ForwardVector, 8.f, { FVector(0.f, 0.6f, 0.5f), FVector(0.25f, 3.2f, 2.8f), FVector(0.6f, 3.6f, 3.1f), FVector(1.f, 0.6f, 0.5f) }, 12, StoneGrey, 0.92f, 0.f, 3.f);
		return M;
	}

	/** Built along +X from the origin (placed in the world each frame, pointed where it goes). */
	FWRToolMesh Tongs()
	{
		// Two forged reins pinned near the jaws, the jaws gripping flat.
		FWRToolMesh M;
		for (const float Side : { -1.f, 1.f })
		{
			M.Tube({ FVector(0.f, Side * 1.6f, 0.f), FVector(20.f, Side * 1.3f, 0.f), FVector(36.f, Side * 0.8f, 0.f), FVector(40.f, Side * 0.5f, 0.f), FVector(46.f, Side * 0.9f, 0.f) }, 0.55f, 6, Iron, 0.5f);
		}
		M.Lathe(FVector(37.f, 0.f, -1.2f), FVector::UpVector, 2.4f, { FVector(0.f, 0.9f, 0.9f), FVector(1.f, 0.9f, 0.9f) }, 8, Iron, 0.5f);
		return M;
	}

	FWRToolMesh Core()
	{
		// A flint nodule with its chalky cortex worn off on the knapped side (two lathes: the dark flint, the pale skin).
		FWRToolMesh M;
		M.Lathe(FVector(-5.f, 0.f, 0.f), FVector::ForwardVector, 11.f, { FVector(0.f, 0.8f, 0.7f), FVector(0.3f, 3.6f, 2.6f), FVector(0.7f, 3.3f, 2.4f), FVector(1.f, 0.6f, 0.5f) }, 9, Flint, 0.35f, 0.f, 5.f);
		M.Lathe(FVector(-5.5f, 0.f, -0.6f), FVector::ForwardVector, 5.f, { FVector(0.f, 0.6f, 0.6f), FVector(0.5f, 2.9f, 2.2f), FVector(1.f, 2.4f, 1.8f) }, 9, FLinearColor(0.62f, 0.58f, 0.5f), 0.95f, 0.f, 9.f);
		return M;
	}

	FWRToolMesh Hank()
	{
		// A hank of fibre being twisted into cord: a loose helix of straw-coloured strands round its axis (+X).
		FWRToolMesh M;
		for (int32 Strand = 0; Strand < 2; ++Strand)
		{
			TArray<FVector> P;
			for (int32 i = 0; i <= 28; ++i)
			{
				const float T = i / 28.f;
				const float A = T * 6.f * PI + Strand * PI;
				P.Add(FVector(-8.f + 16.f * T, FMath::Cos(A) * 1.1f, FMath::Sin(A) * 1.1f));
			}
			M.Tube(P, 0.75f, 5, FLinearColor(0.55f, 0.45f, 0.24f), 0.95f);
		}
		return M;
	}

	FWRToolMesh Plane()
	{
		// A beech smoothing plane along +X: the stock, the iron's wedge rising out of its middle, the front knob and the
		// rear tote the hands hold.
		FWRToolMesh M;
		M.Loft({ { FVector(-12.f, -3.2f, 0.f), FVector(-12.f, 3.2f, 0.f), FVector(-12.f, 3.2f, 5.f), FVector(-12.f, -3.2f, 5.f) },
			{ FVector(12.f, -3.2f, 0.f), FVector(12.f, 3.2f, 0.f), FVector(12.f, 3.2f, 5.f), FVector(12.f, -3.2f, 5.f) } }, Beech, 0.7f);
		M.Lathe(FVector(9.f, 0.f, 5.f), FVector::UpVector, 5.f, { FVector(0.f, 1.6f, 1.6f), FVector(0.6f, 2.2f, 2.2f), FVector(1.f, 1.f, 1.f) }, 10, Beech * 0.8f, 0.6f);
		M.Loft({ { FVector(-1.f, -2.2f, 4.f), FVector(1.5f, -2.2f, 4.f), FVector(1.5f, 2.2f, 4.f), FVector(-1.f, 2.2f, 4.f) },
			{ FVector(-3.f, -2.2f, 9.f), FVector(-1.f, -2.2f, 9.f), FVector(-1.f, 2.2f, 9.f), FVector(-3.f, 2.2f, 9.f) } }, Iron, 0.4f);
		M.Lathe(FVector(-9.f, 0.f, 5.f), FVector(0.3f, 0.f, 1.f).GetSafeNormal(), 8.f, { FVector(0.f, 1.4f, 1.8f), FVector(0.5f, 1.3f, 1.7f), FVector(1.f, 1.6f, 2.f) }, 8, Beech * 0.8f, 0.6f);
		return M;
	}

	FWRToolMesh Pestle()
	{
		// A turned stone pestle along +X, the grinding end (at +X) round and heavy.
		FWRToolMesh M;
		M.Lathe(FVector::ZeroVector, FVector::ForwardVector, 20.f, { FVector(0.f, 1.2f, 1.2f), FVector(0.5f, 1.5f, 1.5f), FVector(0.85f, 2.6f, 2.6f), FVector(1.f, 1.4f, 1.4f) }, 12, FLinearColor(0.42f, 0.4f, 0.37f), 0.85f);
		return M;
	}

	UProceduralMeshComponent* NewTool(AWRCharacter* C, USceneComponent* Under, const TCHAR* Name)
	{
		UProceduralMeshComponent* P = NewObject<UProceduralMeshComponent>(C, Name);
		P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		P->SetCanEverAffectNavigation(false);
		P->SetupAttachment(Under);
		P->RegisterComponent();
		return P;
	}
}

void AWRCharacter::OnRep_Work()
{
	if (Work.Serial != WorkLookSerial)
	{
		WorkLookSerial = Work.Serial;
		WorkLookStart = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
		bWorkApproachDone = false;
	}
	if (Work.Kind != EWRWork::None)
	{
		WorkLookKind = Work.Kind;
	}
}

void AWRCharacter::FillWorkInput(FWRHeroAnimInput& H)
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	bool bActive = Work.Kind != EWRWork::None && IsAlive();
	if (bActive && Work.IsHand())
	{
		// Stopped by the server (a stagger, a freeze): the item-use tag went (a moment's grace for it to arrive).
		bActive = Now - WorkLookStart < 0.25 || HasTag(WRTags::State_Drinking);
	}
	else if (bActive)
	{
		bActive = FVector::Dist2D(GetActorLocation(), Work.Spot) <= WRTune(TEXT("craft.work.reach"), 60.f) + 20.f;
	}
	WorkBlend = FMath::FInterpConstantTo(WorkBlend, bActive ? 1.f : 0.f, World->GetDeltaSeconds(), 1.f / FMath::Max(0.05f, WRTune(TEXT("craft.work.easeSeconds"), 0.25f)));
	H.Work = WorkBlend * WorkBlend * (3.f - 2.f * WorkBlend);
	H.WorkKind = static_cast<int32>(WorkLookKind);
	H.WorkPhase = WorkPhase;
}

void AWRCharacter::TickWorkLook(float DeltaSeconds)
{
	using namespace WRCraftWorkLookDetail;
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// ---- Your own machine walks you to the station's work spot, then turns you to the work.
	if (IsLocallyControlled() && Work.IsStation() && IsAlive() && !bWorkApproachDone)
	{
		UCharacterMovementComponent* Move = GetCharacterMovement();
		const FVector Pending = GetPendingMovementInputVector();
		const FVector To = FVector(Work.Spot.X, Work.Spot.Y, GetActorLocation().Z) - GetActorLocation();
		if (Pending.SizeSquared() > 0.04f)
		{
			bWorkApproachDone = true;   // the stick / keys took over
		}
		else if (To.Size2D() > WRTune(TEXT("craft.work.arriveCm"), 14.f))
		{
			AddMovementInput(To.GetSafeNormal2D(), WRTune(TEXT("craft.work.walkInput"), 0.45f));
		}
		else if (Move && Move->Velocity.Size2D() < 30.f)
		{
			const FRotator Want(0.f, Work.Yaw, 0.f);
			const FRotator Now = FMath::RInterpConstantTo(GetActorRotation(), Want, DeltaSeconds, WRTune(TEXT("craft.work.turnRate"), 360.f));
			SetActorRotation(Now);
			if (FMath::Abs(FRotator::NormalizeAxis(Now.Yaw - Work.Yaw)) < 1.f) bWorkApproachDone = true;
		}
	}

	if (GetNetMode() == NM_DedicatedServer || !Body)
	{
		return;
	}

	// ---- The beat's cycle, and the blow's sound and fx where it lands.
	const float Hz = FWRCraftWork::Hz(WorkLookKind);
	const float Prev = WorkPhase;
	WorkPhase = FMath::Frac(static_cast<float>((World->GetTimeSeconds() - WorkLookStart) * Hz));
	const float Strike = FWRCraftWork::StrikeAt(WorkLookKind);
	const bool bCrossed = Prev <= WorkPhase ? (Prev < Strike && WorkPhase >= Strike) : (Prev < Strike || WorkPhase >= Strike);
	AWRStation* At = Work.At;
	if (bCrossed && WorkBlend > 0.7f && Work.Kind != EWRWork::None)
	{
		const FVector Palm = Body->GetSocketLocation(TEXT("hand_l"));
		switch (Work.Kind)
		{
		case EWRWork::Knap:
			WRSound::PlayAt(this, TEXT("craft.knap"), Palm);
			WRFx::SpawnAt(this, TEXT("craft.chips"), Palm, GetActorRotation());
			break;
		case EWRWork::Twist:
			WRSound::PlayAt(this, TEXT("gather.fiber"), Palm, 0.5f);
			break;
		case EWRWork::Forge:
			if (At)
			{
				WRSound::PlayAt(this, TEXT("craft.anvil"), At->WorkPieceAt());
				WRFx::SpawnAt(this, TEXT("craft.sparks"), At->WorkPieceAt(), GetActorRotation());
				At->OnWorkBeat();
			}
			break;
		case EWRWork::Armor:
			if (At)
			{
				WRSound::PlayAt(this, TEXT("craft.tap"), At->WorkPieceAt());
				At->OnWorkBeat();
			}
			break;
		case EWRWork::Plane:
			if (At)
			{
				WRSound::PlayAt(this, TEXT("craft.plane"), At->WorkPieceAt());
				WRFx::SpawnAt(this, TEXT("craft.shavings"), At->WorkPieceAt() + GetActorForwardVector() * 24.f, GetActorRotation());
			}
			break;
		case EWRWork::Pestle:
			if (At) WRSound::PlayAt(this, TEXT("craft.grind"), At->WorkPieceAt());
			break;
		default: break;
		}
	}

	// ---- The tools in the hands, the weapon and off hand set out of sight meanwhile.
	const bool bShow = WorkBlend > 0.3f && WorkLookKind != EWRWork::None && Body->IsVisible();
	if (bShow != bWorkStowed)
	{
		bWorkStowed = bShow;
		if (WeaponMesh) WeaponMesh->SetHiddenInGame(bShow);
		if (ToolMesh) ToolMesh->SetHiddenInGame(bShow);
		if (ShieldMesh) ShieldMesh->SetHiddenInGame(bShow);
		if (LightMesh) LightMesh->SetHiddenInGame(bShow, true);
	}
	if (!bShow)
	{
		if (WorkToolR) WorkToolR->SetVisibility(false);
		if (WorkToolL) WorkToolL->SetVisibility(false);
		return;
	}
	const FName Socket = WeaponMesh ? WeaponMesh->GetAttachSocketName() : NAME_None;
	if (!WorkToolR) WorkToolR = NewTool(this, Body, TEXT("WorkToolR"));
	if (!WorkToolL) WorkToolL = NewTool(this, Body, TEXT("WorkToolL"));
	if (WorkToolsFor != WorkLookKind)
	{
		WorkToolsFor = WorkLookKind;
		const EWRWork K = WorkLookKind;
		// Right: in the fist (hammers, the hammerstone) or placed in the world (plane, pestle). Left: tongs, the core, the hank.
		WorkToolR->SetUsingAbsoluteLocation(K == EWRWork::Plane || K == EWRWork::Pestle);
		WorkToolR->SetUsingAbsoluteRotation(K == EWRWork::Plane || K == EWRWork::Pestle);
		(K == EWRWork::Forge ? Hammer(1.f) : K == EWRWork::Armor ? Hammer(0.8f) : K == EWRWork::Knap ? Hammerstone() : K == EWRWork::Plane ? Plane()
			: K == EWRWork::Pestle ? Pestle() : FWRToolMesh()).ApplyTo(WorkToolR);
		WorkToolL->SetUsingAbsoluteLocation(true);
		WorkToolL->SetUsingAbsoluteRotation(true);
		(K == EWRWork::Forge ? Tongs() : K == EWRWork::Knap ? Core() : K == EWRWork::Twist ? Hank() : FWRToolMesh()).ApplyTo(WorkToolL);
		if (K != EWRWork::Plane && K != EWRWork::Pestle && !Socket.IsNone())
		{
			WorkToolR->AttachToComponent(Body, FAttachmentTransformRules::KeepRelativeTransform, Socket);
			WorkToolR->SetRelativeTransform(FistFrame());
		}
	}
	const EWRWork K = WorkLookKind;
	const FTransform HandL = Body->GetSocketTransform(TEXT("hand_l"));
	const FTransform HandR = Body->GetSocketTransform(TEXT("hand_r"));
	const FVector PalmL = HandL.GetLocation() + HandL.GetUnitAxis(EAxis::X) * 8.f;
	const FVector PalmR = HandR.GetLocation() + HandR.GetUnitAxis(EAxis::X) * 8.f;
	const FVector Fwd = GetActorForwardVector().GetSafeNormal2D();
	const FVector Piece = At ? At->WorkPieceAt() : PalmL + Fwd * 30.f;
	switch (K)
	{
	case EWRWork::Forge:
		// The tongs from the left fist to the piece on the anvil.
		WorkToolL->SetWorldLocationAndRotation(PalmL, FRotationMatrix::MakeFromXZ((Piece - PalmL).GetSafeNormal(), FVector::UpVector).Rotator());
		break;
	case EWRWork::Knap:
		WorkToolL->SetWorldLocationAndRotation(PalmL, FRotationMatrix::MakeFromXZ(GetActorRightVector(), FVector::UpVector).Rotator());
		break;
	case EWRWork::Twist:
		WorkToolL->SetWorldLocationAndRotation((PalmL + PalmR) * 0.5f, FRotationMatrix::MakeFromXZ((PalmR - PalmL).GetSafeNormal(), FVector::UpVector).Rotator());
		break;
	case EWRWork::Plane:
	{
		// On the board under the hands, along the stroke: its sole on the work's top.
		const FVector Mid = (PalmL + PalmR) * 0.5f;
		WorkToolR->SetWorldLocationAndRotation(FVector(Mid.X, Mid.Y, Piece.Z + 1.5f), FRotationMatrix::MakeFromXZ(Fwd, FVector::UpVector).Rotator());
		break;
	}
	case EWRWork::Pestle:
		// From the right fist down into the mortar's bowl.
		WorkToolR->SetWorldLocationAndRotation(PalmR, FRotationMatrix::MakeFromXZ((Piece - PalmR).GetSafeNormal(), FVector::UpVector).Rotator());
		break;
	default: break;
	}
	WorkToolR->SetVisibility(K != EWRWork::Twist);
	WorkToolL->SetVisibility(K == EWRWork::Forge || K == EWRWork::Knap || K == EWRWork::Twist);
}
