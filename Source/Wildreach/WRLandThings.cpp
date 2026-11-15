// A runtime land's live things (cooks/build-biome-void.md 2.1): the Grey Crossing. The newcomers came to the Broken End
// by sea (world-map rule 5), so until the 12-wedge map joins the lands, that is the way in: a longboat at a landing on
// the island's south shore, a beached one at Lantern Cove under the Black Beacon. E at either: the screen goes dark as
// you push off and comes back as you step ashore at the other end. Server authority: the server places both ends,
// checks reach, moves the rower; clients draw the landings from the replicated state.
#include "WRLandThings.h"
#include "WRVoidUnmade.h"
#include "WRVoidBosses.h"
#include "WRVoidWorld.h"
#include "WRLand.h"
#include "Wildreach.h"
#include "WRBuildLook.h"
#include "WRBuildLookKit.h"
#include "WRFx.h"
#include "WRPlayerController.h"
#include "WRPortalsInternal.h"
#include "WRSound.h"
#include "WRTuning.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

using WRLookKit::FSurf;
using WRLookKit::FKit;

namespace
{
	FLinearColor LC(uint8 R, uint8 G, uint8 B) { return FLinearColor(FColor(R, G, B)); }
	float CNum(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, float Fallback)
	{
		double V = 0.0;
		return O.IsValid() && O->TryGetNumberField(Key, V) ? float(V) : Fallback;
	}
	const float CrossReach = 650.f;

	AWRCrossing* CrossingNear(const UWorld* World, const FVector& At)
	{
		for (TActorIterator<AWRCrossing> It(World); It; ++It)
		{
			if (It->bReady && FVector::Dist(It->GetActorLocation(), At) < CrossReach) return *It;
		}
		return nullptr;
	}
}

// ================================================================================================ the crossing

AWRCrossing::AWRCrossing()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRCrossing::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRCrossing, LandId);
	DOREPLIFETIME(AWRCrossing, bLandEnd);
	DOREPLIFETIME(AWRCrossing, StepOff);
	DOREPLIFETIME(AWRCrossing, FaceYaw);
	DOREPLIFETIME(AWRCrossing, bReady);
}

void AWRCrossing::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && !bReady) Settle();
	if (bReady) BuildLook();
}

void AWRCrossing::OnRep_Ready()
{
	if (bReady) BuildLook();
}

void AWRCrossing::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || bReady)
	{
		SetActorTickEnabled(!bReady);
		return;
	}
	// The island's ground streams in after the world starts: try again each second until the shore is found.
	if (GetWorld()->GetTimeSeconds() >= RetryAt)
	{
		RetryAt = GetWorld()->GetTimeSeconds() + 1.f;
		if (Settle()) BuildLook();
	}
}

bool AWRCrossing::Settle()
{
	const FWRLandDef* D = WRLand::Find(LandId);
	const TSharedPtr<FJsonObject> C = D ? D->Section(TEXT("crossing")) : nullptr;
	if (!C) return false;
	if (bLandEnd)
	{
		// The cove: the land's own ground (no trace needed), the boat drawn up on the black sand facing the sea.
		const FVector2D At = WRLand::Vec2(C->TryGetField(TEXT("land")));
		const float Face = CNum(C, TEXT("landFace"), 180.f);
		const FVector2D Sea(FMath::Cos(FMath::DegreesToRadians(Face + 180.f)), FMath::Sin(FMath::DegreesToRadians(Face + 180.f)));
		SetActorLocationAndRotation(D->GroundPoint(At), FRotator(0.f, Face + 180.f, 0.f));
		StepOff = D->GroundPoint(At + FVector2D(FMath::Cos(FMath::DegreesToRadians(Face)), FMath::Sin(FMath::DegreesToRadians(Face))) * 6.f);
		FaceYaw = Face;
		bReady = true;
		return true;
	}
	// The island's south shore: from the data's point, traced down; in the sea, step toward the island's middle until it's
	// land. The landing is that first dry ground, the boat a little further out, facing out to sea (toward the land).
	const FVector2D Want = WRLand::Vec2(C->TryGetField(TEXT("island"))) * 100.f;
	const FVector2D Out = Want.GetSafeNormal().IsNearlyZero() ? FVector2D(0.f, 1.f) : Want.GetSafeNormal();
	const float Sea = WRTune(TEXT("sea.level"), -20.f) + 120.f;
	FVector2D P = Want;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(WRCrossingSettle), false, this);
	for (int32 Step = 0; Step < 120; ++Step)
	{
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByChannel(Hit, FVector(P, 150000.f), FVector(P, -20000.f), ECC_WorldStatic, Q)) return false;
		if (Hit.ImpactPoint.Z > Sea)
		{
			const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Out.Y, Out.X));
			SetActorLocationAndRotation(Hit.ImpactPoint, FRotator(0.f, Yaw, 0.f));
			FHitResult In;
			const FVector2D InP = P - Out * 600.f;
			StepOff = GetWorld()->LineTraceSingleByChannel(In, FVector(InP, 150000.f), FVector(InP, -20000.f), ECC_WorldStatic, Q) ? In.ImpactPoint : Hit.ImpactPoint;
			FaceYaw = Yaw + 180.f;
			bReady = true;
			UE_LOG(LogWildreach, Log, TEXT("LAND %s: the island's landing at %s"), *LandId.ToString(), *GetActorLocation().ToCompactString());
			return true;
		}
		P -= Out * 1000.f;
	}
	return false;
}

FString AWRCrossing::PlaceName() const
{
	const FWRLandDef* D = WRLand::Find(LandId);
	const TSharedPtr<FJsonObject> C = D ? D->Section(TEXT("crossing")) : nullptr;
	FString Name = TEXT("the Grey Crossing");
	if (C) C->TryGetStringField(TEXT("name"), Name);
	return Name;
}

FString AWRCrossing::PromptVerb() const
{
	const FWRLandDef* D = WRLand::Find(LandId);
	return bLandEnd ? FString(TEXT("Row back to the island")) : FString::Printf(TEXT("Row across to %s"), D ? *D->Name : TEXT("the far shore"));
}

void AWRCrossing::BuildLook()
{
	if (bBuilt || !bReady || GetNetMode() == NM_DedicatedServer) return;
	bBuilt = true;
	// Local X points out to sea. A greywood jetty from the shore out over the water (on the island's end; the cove's boat is
	// drawn up on the sand), the longboat moored to its post, a lantern on a pole so the landing reads at night.
	TArray<FWRLookBlock> B;
	FKit K(GetActorTransform(), B);
	const FSurf Wood = FSurf::Wood(LC(142, 138, 132));
	const FSurf Dark = FSurf::Wood(LC(96, 92, 88));
	const FSurf Rope = FSurf::Cloth(LC(120, 104, 80));
	const FSurf Iron = FSurf::Steel(LC(32, 30, 30), 0.6f);
	const FSurf Flame = FSurf::Lit(LC(255, 170, 90), 1.1f);
	const float SeaZ = WRTune(TEXT("sea.level"), -20.f) - GetActorLocation().Z;
	const float JetLen = bLandEnd ? 0.f : 1400.f;
	if (!bLandEnd)
	{
		for (float X = 0.f; X < JetLen; X += 40.f)
		{
			K.Add(Wood.Dim(0.85f + 0.2f * K.Rand(FVector(X, 0, 0), 3)), FVector(36.f, 260.f, 8.f), FVector(X + 20.f, 0.f, 60.f + X * 0.0f));
		}
		for (float X = 100.f; X < JetLen; X += 300.f)
		{
			for (const float Y : { -120.f, 120.f }) K.Rod(Dark, FVector(X, Y, SeaZ - 200.f), FVector(X, Y, 70.f), 22.f);
		}
		WRFoldLook::Block(this, RootComponent, FVector(JetLen * 0.5f, 0.f, 58.f), FVector(JetLen * 0.5f, 130.f, 8.f));
	}
	// The longboat: keel, ribs, planking, oars shipped across the thwarts.
	const FVector Boat(bLandEnd ? 200.f : JetLen + 150.f, bLandEnd ? 0.f : 320.f, bLandEnd ? 30.f : SeaZ + 30.f);
	const FRotator BR(0.f, bLandEnd ? 0.f : 0.f, bLandEnd ? -8.f : 0.f);
	const auto L = [&](float X, float Y, float Z) { return Boat + BR.RotateVector(FVector(X, Y, Z)); };
	K.Add(Dark, FVector(800.f, 18.f, 22.f), L(0.f, 0.f, 0.f), BR);
	for (int32 i = 0; i <= 8; ++i)
	{
		const float X = -380.f + i * 95.f;
		const float W = 110.f * (1.f - FMath::Square(X / 420.f) * 0.8f);
		for (int32 k = 0; k < 4; ++k)
		{
			const float A0 = PI * k / 4.f, A1 = PI * (k + 1) / 4.f;
			K.Rod(Dark, L(X, -FMath::Cos(A0) * W, FMath::Sin(A0) * W * 0.6f + 10.f), L(X, -FMath::Cos(A1) * W, FMath::Sin(A1) * W * 0.6f + 10.f), 7.f, true);
		}
		if (i > 0 && i < 8 && i % 2 == 0) K.Add(Wood, FVector(16.f, W * 1.8f, 5.f), L(X, 0.f, W * 0.45f), BR);   // thwarts
	}
	for (const float Side : { -1.f, 1.f })
	{
		for (int32 s = 0; s < 3; ++s)
		{
			K.Add(Wood.Dim(0.9f - 0.05f * s), FVector(760.f - s * 60.f, 4.f, 24.f), L(0.f, Side * (60.f + s * 26.f), 14.f + s * 22.f), BR + FRotator(0.f, 0.f, Side * (30.f + s * 20.f)));
		}
		K.Rod(Wood, L(-200.f, Side * 50.f, 66.f), L(260.f, Side * 140.f, 60.f), 6.f);   // the oars
		K.Add(Dark, FVector(70.f, 4.f, 16.f), L(270.f, Side * 146.f, 58.f), BR + FRotator(0.f, Side * 12.f, 0.f));
	}
	// The mooring post and its rope to the bow.
	const FVector Post(bLandEnd ? -150.f : JetLen - 60.f, bLandEnd ? 120.f : 150.f, bLandEnd ? 0.f : 60.f);
	K.Rod(Dark, Post - FVector(0, 0, bLandEnd ? 40.f : 260.f), Post + FVector(0, 0, 110.f), 20.f);
	K.Rod(Rope, Post + FVector(0, 0, 80.f), L(380.f, 0.f, 30.f), 3.f);
	// The lantern pole and its lantern (a light to find the landing by at night).
	const FVector Pole(bLandEnd ? -300.f : 40.f, bLandEnd ? -200.f : -160.f, bLandEnd ? 0.f : 60.f);
	K.Rod(Dark, Pole, Pole + FVector(0, 0, 320.f), 12.f);
	K.Rod(Dark, Pole + FVector(0, 0, 300.f), Pole + FVector(60.f, 0, 300.f), 6.f);
	K.Add(Iron, FVector(26.f, 26.f, 36.f), Pole + FVector(60.f, 0, 262.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
	K.Add(Flame, FVector(12.f, 12.f, 18.f), Pole + FVector(60.f, 0, 262.f), FRotator::ZeroRotator, EWRLookMesh::Cone);
	WRFoldLook::Light(this, RootComponent, Pole + FVector(60.f, 0, 262.f), LC(255, 170, 90), 2600.f, 1400.f);
	FWRBuildLook::Spawn(this, RootComponent, B, LC(163, 158, 149), false);
}

// ================================================================================================ start, prompt, act

void WRLandThings::Start(UWorld* World)
{
	if (!World || World->GetNetMode() == NM_Client) return;
	for (const FWRLandDef& D : WRLand::All())
	{
		if (!WRLand::IsLive(D) || !D.Section(TEXT("crossing"))) continue;
		AWRCrossing* Ends[2] = { nullptr, nullptr };
		for (int32 e = 0; e < 2; ++e)
		{
			AWRCrossing* C = World->SpawnActorDeferred<AWRCrossing>(AWRCrossing::StaticClass(), FTransform(D.Origin), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (!C) continue;
			C->LandId = D.Id;
			C->bLandEnd = e == 1;
			C->FinishSpawning(FTransform(D.Origin));
			Ends[e] = C;
		}
		if (Ends[0] && Ends[1]) { Ends[0]->Other = Ends[1]; Ends[1]->Other = Ends[0]; }
	}
}

bool WRLandThings::Prompt(const AWRPlayerController* PC, FString& OutTarget, FString& OutVerb)
{
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	if (!Me) return false;
	if (const AWRCrossing* C = CrossingNear(PC->GetWorld(), Me->GetActorLocation()))
	{
		OutTarget = C->PlaceName();
		OutVerb = C->PromptVerb();
		return true;
	}
	if (const AWRDescentLift* Lift = AWRDescentLift::Find(PC->GetWorld()))
	{
		FString V;
		if (Lift->PromptFor(Me->GetActorLocation(), V)) { OutTarget = TEXT("The Descent lift"); OutVerb = V; return true; }
	}
	if (const AWRNullGate* G = AWRNullGate::Find(PC->GetWorld()); G && G->CrankNear(Me->GetActorLocation()) && G->Openness() < 0.99f)
	{
		OutTarget = TEXT("The Null Gate's winch");
		OutVerb = TEXT("Hold E: crank the gate up (8 s)");
		return true;
	}
	return WRVoidWorld::Prompt(PC, OutTarget, OutVerb);
}

bool WRLandThings::ClientInteract(AWRPlayerController* PC)
{
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	if (!Me) return false;
	if (const AWRCrossing* C = CrossingNear(PC->GetWorld(), Me->GetActorLocation()))
	{
		// The screen darkens as you push off; the server moves you under it and the dark lifts at the other shore.
		const FWRLandDef* D = WRLand::Find(C->LandId);
		const float Fade = D ? CNum(D->Section(TEXT("crossing")), TEXT("fadeSeconds"), 1.2f) : 1.2f;
		if (PC->PlayerCameraManager) PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, Fade * 0.6f, FLinearColor::Black, true, true);
		PC->SendCommand(TEXT("land cross"));
		FTimerHandle H;
		TWeakObjectPtr<AWRPlayerController> Weak(PC);
		PC->GetWorldTimerManager().SetTimer(H, [Weak, Fade]()
		{
			if (Weak.IsValid() && Weak->PlayerCameraManager) Weak->PlayerCameraManager->StartCameraFade(1.f, 0.f, Fade, FLinearColor::Black, true, false);
		}, Fade * 0.6f + 1.4f, false);
		return true;
	}
	if (const AWRDescentLift* Lift = AWRDescentLift::Find(PC->GetWorld()))
	{
		FString V;
		if (Lift->PromptFor(Me->GetActorLocation(), V)) { PC->SendCommand(TEXT("land lift")); return true; }
	}
	if (const AWRNullGate* G = AWRNullGate::Find(PC->GetWorld()); G && G->CrankNear(Me->GetActorLocation()) && G->Openness() < 0.99f)
	{
		PC->SendCommand(TEXT("land winch"));
		return true;
	}
	return WRVoidWorld::ClientInteract(PC);
}

FString WRLandThings::Command(AWRPlayerController* PC, const TArray<FString>& Args)
{
	APawn* Me = PC ? PC->GetPawn() : nullptr;
	if (!Me || !PC->HasAuthority()) return FString();
	const FString Verb = Args.IsValidIndex(1) ? Args[1] : FString();
	if (Verb == TEXT("cross"))
	{
		AWRCrossing* C = CrossingNear(PC->GetWorld(), Me->GetActorLocation());
		if (!C) return TEXT("Too far from the boat.");
		AWRCrossing* To = C->Other.Get();
		if (!To || !To->bReady) return TEXT("The far landing isn't ready yet.");
		const FWRLandDef* D = WRLand::Find(C->LandId);
		const float Fade = D ? CNum(D->Section(TEXT("crossing")), TEXT("fadeSeconds"), 1.2f) : 1.2f;
		// Moved under the dark (the client's fade is already down), onto the other landing's ground.
		TWeakObjectPtr<APawn> WeakPawn(Me);
		TWeakObjectPtr<AWRCrossing> WeakTo(To);
		FTimerHandle H;
		PC->GetWorldTimerManager().SetTimer(H, [WeakPawn, WeakTo]()
		{
			APawn* P = WeakPawn.Get();
			AWRCrossing* T = WeakTo.Get();
			if (!P || !T) return;
			const FRotator Face(0.f, T->FaceYaw, 0.f);
			const FVector Dest = T->StepOff + FVector(0.f, 0.f, P->GetSimpleCollisionHalfHeight() + 40.f);
			if (!P->TeleportTo(Dest, Face, false, false)) P->TeleportTo(Dest + FVector(0, 0, 200.f), Face, false, true);
			if (AController* Ctl = P->GetController()) Ctl->SetControlRotation(Face);
			if (ACharacter* Ch = Cast<ACharacter>(P)) Ch->GetCharacterMovement()->Velocity = FVector::ZeroVector;
		}, Fade * 0.6f + 0.1f, false);
		UE_LOG(LogWildreach, Log, TEXT("LAND %s crosses to the %s end"), *Me->GetName(), To->bLandEnd ? TEXT("land") : TEXT("island"));
		return To->bLandEnd ? FString::Printf(TEXT("You row across the Broken Race to %s."), D ? *D->Name : TEXT("the far shore")) : FString(TEXT("You row back across the Broken Race."));
	}
	if (Verb == TEXT("lift"))
	{
		AWRDescentLift* Lift = AWRDescentLift::Find(PC->GetWorld());
		return Lift ? Lift->Use(PC) : FString(TEXT("No lift here."));
	}
	if (Verb == TEXT("winch"))
	{
		AWRNullGate* G = AWRNullGate::Find(PC->GetWorld());
		if (!G || !G->CrankNear(Me->GetActorLocation())) return TEXT("Too far from the winch.");
		G->Crank(PC);
		return TEXT("You set your back to the crank. Keep at it.");
	}
	return WRVoidWorld::Command(PC, Args);
}
