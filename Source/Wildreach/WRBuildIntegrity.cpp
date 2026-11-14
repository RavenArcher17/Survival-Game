// Structural integrity past the grid: freeform beams and posts, braces, stability colours and collapse rubble
// (cooks/build-building.md 2.3; WRBuildIntegrity.h).

#include "WRBuildIntegrity.h"
#include "WRBuilding.h"
#include "WRBuildLook.h"
#include "WRBuildSets.h"
#include "WRCharacterBase.h"
#include "WRCombatLibrary.h"
#include "WRFx.h"
#include "WRSound.h"
#include "WRTuning.h"
#include "Wildreach.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"

namespace WRIntegrityDetail
{
	/** Does the segment A..B pass through Box (both in the same space)? Slab test. */
	bool SegmentHitsBox(const FVector& A, const FVector& B, const FBox& Box)
	{
		float T0 = 0.f, T1 = 1.f;
		const FVector D = B - A;
		for (int32 i = 0; i < 3; ++i)
		{
			if (FMath::Abs(D[i]) < KINDA_SMALL_NUMBER)
			{
				if (A[i] < Box.Min[i] || A[i] > Box.Max[i]) return false;
				continue;
			}
			float Ta = (Box.Min[i] - A[i]) / D[i], Tb = (Box.Max[i] - A[i]) / D[i];
			if (Ta > Tb) Swap(Ta, Tb);
			T0 = FMath::Max(T0, Ta);
			T1 = FMath::Min(T1, Tb);
			if (T0 > T1) return false;
		}
		return true;
	}

	/** Is a world point on (inside, or within Slack of) a piece? */
	bool Touches(const FWRPieceDef& Def, const FTransform& T, const FVector& World, float Slack)
	{
		if (Def.IsBeam())
		{
			FVector A, B;
			FWRIntegrity::Ends(Def, T, A, B);
			return FMath::PointDistToSegment(World, A, B) <= Slack + 12.f;
		}
		const FVector Local = T.InverseTransformPositionNoScale(World);
		return FWRIntegrity::BoxOf(Def).ExpandBy(Slack).IsInside(Local);
	}

	bool IsVertical(const FWRPieceDef& Def, const FTransform& T)
	{
		return Def.IsBeam() && T.GetRotation().GetForwardVector().Z > 0.85f;
	}

	bool IsPillarLike(const FWRPieceDef& Def, const FTransform& T)
	{
		return Def.Shape == TEXT("pillar") || IsVertical(Def, T);
	}
}

FBox FWRIntegrity::BoxOf(const FWRPieceDef& Def)
{
	const FName S = Def.Shape;
	if (Def.IsBeam()) return FBox(FVector(0.f, -12.f, -12.f), FVector(FMath::Max(50.f, Def.FreeLength), 12.f, 12.f));
	if (S == TEXT("foundation")) return FBox(FVector(-200, -200, -150), FVector(200, 200, 100));
	if (S == TEXT("floor")) return FBox(FVector(-200, -200, 0), FVector(200, 200, 20));
	if (S == TEXT("roof")) return FBox(FVector(-240, -210, 0), FVector(210, 210, 215));
	if (S == TEXT("pillar")) return FBox(FVector(-20, -20, 0), FVector(20, 20, 300));
	if (S == TEXT("stairs")) return FBox(FVector(-250, -104, 0), FVector(250, 104, 300));
	if (S == TEXT("halfwall")) return FBox(FVector(-210, -10, 0), FVector(210, 10, 150));
	if (S == TEXT("upperwall") || S == TEXT("gable")) return FBox(FVector(-210, -10, 0), FVector(210, 10, 210));
	if (S == TEXT("palisade")) return FBox(FVector(-220, -20, 0), FVector(220, 20, 400));
	if (S == TEXT("gate")) return FBox(FVector(-220, -10, 0), FVector(220, 10, 400));
	if (S == TEXT("tower")) return FBox(FVector(-100, -100, 0), FVector(100, 100, 520));
	return FBox(FVector(-210, -10, 0), FVector(210, 10, 300));   // wall, door, doorway, window
}

void FWRIntegrity::Ends(const FWRPieceDef& Def, const FTransform& T, FVector& OutA, FVector& OutB)
{
	OutA = T.GetLocation();
	OutB = T.TransformPositionNoScale(FVector(FMath::Max(50.f, Def.FreeLength), 0.f, 0.f));
}

int32 FWRIntegrity::LengthSteps(const FWRPieceDef& Def, const FTransform& T)
{
	if (!Def.IsBeam()) return 0;
	FVector A, B;
	Ends(Def, T, A, B);
	const float Per = WRTune(TEXT("building.beamStepLength"), 200.f);
	return FMath::CeilToInt(FVector::Dist2D(A, B) / Per - 0.05f);
}

int32 FWRIntegrity::LinkCost(const FWRPieceDef& FromDef, const FTransform& From, const FWRPieceDef& ToDef, const FTransform& To)
{
	using namespace WRIntegrityDetail;
	if (!FromDef.IsStructural() || !ToDef.IsStructural()) return INDEX_NONE;
	if (!FromDef.IsBeam() && !ToDef.IsBeam())
	{
		return FWRBuildRules::SupportCost(From.GetLocation(), FromDef.Shape, To.GetLocation(), FWRBuildRegistry::Get().GridSize);
	}
	const float Slack = WRTune(TEXT("building.beamTouch"), 20.f);
	if (ToDef.IsBeam())
	{
		// A beam hangs on what one of its ends touches, and costs its own reach.
		FVector A, B;
		Ends(ToDef, To, A, B);
		if (Touches(FromDef, From, A, Slack) || Touches(FromDef, From, B, Slack)) return LengthSteps(ToDef, To);
		return INDEX_NONE;
	}
	// A snapped piece on a beam: straight on a post's top bears like a pillar; a beam under or through it holds it a step out.
	FVector A, B;
	Ends(FromDef, From, A, B);
	const FVector Top = A.Z > B.Z ? A : B;
	if (IsVertical(FromDef, From) && FVector::Dist2D(Top, To.GetLocation()) < 35.f && FMath::Abs(Top.Z - To.GetLocation().Z) < 35.f) return 0;
	const FVector LA = To.InverseTransformPositionNoScale(A), LB = To.InverseTransformPositionNoScale(B);
	return SegmentHitsBox(LA, LB, BoxOf(ToDef).ExpandBy(WRTune(TEXT("building.beamCarry"), 25.f))) ? 1 : INDEX_NONE;
}

bool FWRIntegrity::BeamGrounded(const UWorld* World, const FWRPieceDef& Def, const FTransform& T, const AActor* Ignore)
{
	if (!World || !Def.IsBeam()) return false;
	FVector Ends2[2];
	Ends(Def, T, Ends2[0], Ends2[1]);
	for (const FVector& E : Ends2)
	{
		FCollisionQueryParams Query(SCENE_QUERY_STAT(WRBeamGround), false, Ignore);
		FHitResult H;
		for (int32 Try = 0; Try < 3 && World->LineTraceSingleByChannel(H, E + FVector(0.f, 0.f, 30.f), E - FVector(0.f, 0.f, 45.f), ECC_WorldStatic, Query); ++Try)
		{
			if (H.GetActor() && !H.GetActor()->IsA<AWRBuildPiece>()) return true;
			Query.AddIgnoredActor(H.GetActor());
		}
	}
	return false;
}

int32 FWRIntegrity::Allowed(const UWorld* World, const FWRPieceDef& Def, const FTransform& T, const AActor* Ignore)
{
	using namespace WRIntegrityDetail;
	const int32 Span = FWRBuildRules::SpanFor(Def.Material);
	if (!World || FWRBuildRules::SlotOf(Def.Shape) != TEXT("edge")) return Span;
	// A brace from this wall to a pillar or post: one step further.
	const float Slack = WRTune(TEXT("building.beamTouch"), 20.f);
	for (TActorIterator<AWRBuildPiece> It(World); It; ++It)
	{
		const FWRPieceDef* B = It->GetDef();
		if (*It == Ignore || It->bGhost || !B || B->Shape != TEXT("brace") || It->IsActorBeingDestroyed()) continue;
		if (FVector::DistSquared(It->GetActorLocation(), T.GetLocation()) > 800.f * 800.f) continue;
		FVector A, E;
		Ends(*B, It->GetActorTransform(), A, E);
		const bool bA = Touches(Def, T, A, Slack), bE = Touches(Def, T, E, Slack);
		if (!bA && !bE) continue;
		const FVector Other = bA ? E : A;
		for (TActorIterator<AWRBuildPiece> P(World); P; ++P)
		{
			const FWRPieceDef* PD = P->GetDef();
			if (*P == *It || P->bGhost || !PD || !IsPillarLike(*PD, P->GetActorTransform())) continue;
			if (Touches(*PD, P->GetActorTransform(), Other, Slack)) return Span + FMath::RoundToInt(WRTune(TEXT("building.braceBonus"), 1.f));
		}
	}
	return Span;
}

int32 FWRIntegrity::StepIndex(int32 Support, int32 Allowed)
{
	if (Support <= 0) return 0;                       // blue: on the ground
	if (Support >= Allowed) return 4;                 // red: the last step
	const float R = static_cast<float>(Support) / FMath::Max(1, Allowed);
	return R <= 0.34f ? 1 : R <= 0.67f ? 2 : 3;       // green, yellow, orange
}

FLinearColor FWRIntegrity::StepColor(int32 Support, int32 Allowed)
{
	static const FLinearColor Steps[5] = {
		FLinearColor(0.08f, 0.32f, 1.f), FLinearColor(0.12f, 0.85f, 0.2f), FLinearColor(1.f, 0.82f, 0.08f),
		FLinearColor(1.f, 0.38f, 0.03f), FLinearColor(1.f, 0.04f, 0.03f) };
	return Steps[StepIndex(Support, Allowed)];
}

void FWRIntegrity::Collapse(AWRBuildPiece* Piece)
{
	if (!Piece || !Piece->HasAuthority() || Piece->IsActorBeingDestroyed()) return;
	UWorld* World = Piece->GetWorld();
	const FWRPieceDef* Def = Piece->GetDef();
	FVector Origin, Extent;
	Piece->GetActorBounds(true, Origin, Extent);
	if (Extent.IsNearlyZero()) { Origin = Piece->GetActorLocation(); Extent = FVector(100.f); }
	if (Def && World)
	{
		const FTransform At(FRotator::ZeroRotator, Origin);
		AWRRubble* Rubble = World->SpawnActorDeferred<AWRRubble>(AWRRubble::StaticClass(), At, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Rubble)
		{
			Rubble->Setup(Def->Id, Extent, FMath::Rand());
			Rubble->FinishSpawning(At);
		}
		const bool bWood = Def->Material == TEXT("wood") || Def->Tier == TEXT("heartwood_lodge") || Def->Tier == TEXT("rime_lodge") || Def->Tier == TEXT("stilt_house");
		WRFx::Broadcast(Piece, TEXT("build.collapse"), Origin, FRotator::ZeroRotator, FMath::Clamp(Extent.Size() / 150.f, 0.5f, 3.f),
			bWood ? TEXT("build.collapse.wood") : TEXT("build.collapse.stone"));
	}
	UE_LOG(LogWildreach, Log, TEXT("BUILD %s (%s) collapsed: no support"), *Piece->GetName(), *Piece->PieceId.ToString());
	Piece->Destroy();
}

// ---------------------------------------------------------------- rubble

AWRRubble::AWRRubble()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(false);
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	InitialLifeSpan = 0.f;
}

void AWRRubble::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AWRRubble, PieceId, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AWRRubble, Extent, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AWRRubble, Seed, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AWRRubble, StartTime, COND_InitialOnly);
}

void AWRRubble::Setup(FName InPieceId, const FVector& InExtent, int32 InSeed)
{
	PieceId = InPieceId;
	Extent = InExtent;
	Seed = InSeed;
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	StartTime = GS ? GS->GetServerWorldTimeSeconds() : 0.f;
}

float AWRRubble::Now() const
{
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	return (GS ? GS->GetServerWorldTimeSeconds() : 0.f) - StartTime;
}

void AWRRubble::BeginPlay()
{
	Super::BeginPlay();
	MakeChunks();
	SetLifeSpan(WRTune(TEXT("building.rubbleSeconds"), 3.f) + 0.5f);
}

void AWRRubble::MakeChunks()
{
	const FWRPieceDef* Def = FWRBuildRegistry::Get().Find(PieceId);
	FRandomStream R(Seed);
	// 4 to 8 by size: a pillar breaks into a few, a wall or a floor into more.
	const float Volume = Extent.X * Extent.Y * Extent.Z * 8.f;
	const int32 N = FMath::Clamp(4 + FMath::RoundToInt(Volume / (400.f * 300.f * 40.f) * 2.f), 4, 8);
	const float Gravity = 980.f;
	const float Life = WRTune(TEXT("building.rubbleSeconds"), 3.f);
	const FVector Centre = GetActorLocation();
	for (int32 i = 0; i < N; ++i)
	{
		FChunk C;
		C.Start = Centre + FVector(R.FRandRange(-Extent.X, Extent.X) * 0.7f, R.FRandRange(-Extent.Y, Extent.Y) * 0.7f, R.FRandRange(-Extent.Z, Extent.Z) * 0.6f);
		const float Big = FMath::Clamp(FMath::Max3(Extent.X, Extent.Y, Extent.Z) * 0.45f, 22.f, 75.f);
		C.Size = FVector(Big * R.FRandRange(0.55f, 1.f), Big * R.FRandRange(0.45f, 0.9f), Big * R.FRandRange(0.35f, 0.75f));
		FVector Out = C.Start - Centre;
		Out.Z = 0.f;
		Out = Out.GetSafeNormal();
		if (Out.IsNearlyZero()) Out = FVector(R.FRandRange(-1.f, 1.f), R.FRandRange(-1.f, 1.f), 0.f).GetSafeNormal();
		C.Velocity = Out * R.FRandRange(60.f, 200.f) + FVector(0.f, 0.f, R.FRandRange(0.f, 160.f));
		C.Spin = FRotator(R.FRandRange(-220.f, 220.f), R.FRandRange(-220.f, 220.f), R.FRandRange(-220.f, 220.f));
		C.Shade = R.FRand();
		// Where it lands: the ground (or a floor) under its fall.
		FHitResult H;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRRubble), false, this);
		if (GetWorld()->LineTraceSingleByChannel(H, C.Start, C.Start - FVector(0.f, 0.f, 5000.f), ECC_WorldStatic, Q))
		{
			C.GroundZ = H.ImpactPoint.Z + C.Size.Z * 0.4f;
			// z(t) = z0 + vz t - g t^2 / 2 = ground: the later root.
			const float A = 0.5f * Gravity, B = -C.Velocity.Z, Cc = C.GroundZ - C.Start.Z;
			const float Disc = B * B - 4.f * A * Cc;
			C.LandAt = Disc >= 0.f ? FMath::Min(Life, (-B + FMath::Sqrt(Disc)) / (2.f * A)) : Life;
		}
		else
		{
			C.LandAt = Life;
		}
		Chunks.Add(C);
	}
	if (GetNetMode() == NM_DedicatedServer || !Def) return;

	// Drawn as one instanced mesh of the piece's own material (planks, coursed stone, or its tier's tinted surface).
	Pieces = NewObject<UInstancedStaticMeshComponent>(this);
	Pieces->SetupAttachment(GetRootComponent());
	Pieces->SetUsingAbsoluteLocation(true);
	Pieces->SetUsingAbsoluteRotation(true);
	Pieces->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Pieces->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	const FWRTierDef* Tier = FWRBuildSets::Get().Tier(Def->Tier);
	const bool bTinted = Tier && Tier->Look.Style != TEXT("wood") && Tier->Look.Style != TEXT("stone");
	const TCHAR* MatPath = bTinted ? TEXT("/Game/Building/M_BuildTinted.M_BuildTinted")
		: Def->Material == TEXT("wood") ? TEXT("/Game/Building/M_BuildWood.M_BuildWood") : TEXT("/Game/Building/M_BuildStone.M_BuildStone");
	Pieces->SetMaterial(0, LoadObject<UMaterialInterface>(nullptr, MatPath));
	Pieces->SetNumCustomDataFloats(bTinted ? 10 : 3);
	Pieces->RegisterComponent();
	for (int32 i = 0; i < Chunks.Num(); ++i)
	{
		Pieces->AddInstance(FTransform(FRotator::ZeroRotator, Chunks[i].Start, Chunks[i].Size / 100.f), true);
		if (bTinted)
		{
			const FLinearColor Col = i % 3 == 2 ? Tier->Look.B : Tier->Look.A;
			const float Data[10] = { Chunks[i].Shade, 0.f, 1.f, Col.R, Col.G, Col.B, Tier->Look.Metal, Tier->Look.Rough, 0.f, Tier->Look.Metal > 0.5f ? WRLookPattern::Brushed : WRLookPattern::Mottle };
			Pieces->SetCustomData(i, Data, false);
		}
		else
		{
			const float Data[3] = { Chunks[i].Shade, 0.f, 0.85f };
			Pieces->SetCustomData(i, Data, false);
		}
	}
	Pieces->MarkRenderStateDirty();
}

void AWRRubble::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float T = FMath::Max(0.f, Now());
	const float Life = WRTune(TEXT("building.rubbleSeconds"), 3.f);
	const float Gravity = 980.f;
	TArray<FTransform> Xf;
	Xf.Reserve(Chunks.Num());
	for (const FChunk& C : Chunks)
	{
		const float Tf = FMath::Min(T, C.LandAt);
		FVector P = C.Start + C.Velocity * Tf + FVector(0.f, 0.f, -0.5f * Gravity * Tf * Tf);
		P.Z = FMath::Max(P.Z, C.GroundZ);
		const float Shrink = FMath::Clamp((Life - T) / 0.3f, 0.f, 1.f);
		const FRotator Rot = C.Spin * Tf;
		Xf.Emplace(Rot, P, C.Size / 100.f * Shrink);

		// Server: a chunk still falling hurts whoever it lands on, once each (the building, not a trap: it's falling).
		if (HasAuthority() && T < C.LandAt)
		{
			const float Reach = C.Size.GetMax() * 0.6f + 35.f;
			for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
			{
				if (Hurt.Contains(*It) || !It->IsAlive()) continue;
				const FVector Body = It->GetActorLocation();
				if (FVector::Dist2D(Body, P) > Reach || P.Z < Body.Z - 100.f || P.Z > Body.Z + 120.f) continue;
				Hurt.Add(*It);
				FWRHit Hit;
				Hit.Instigator = this;
				Hit.Damage = WRTune(TEXT("building.rubbleDamage"), 50.f);
				Hit.Origin = P + FVector(0.f, 0.f, 100.f);
				Hit.ImpactPoint = P;
				Hit.Impact = 2;
				Hit.Tier = EWRHitTier::Heavy;
				Hit.bProjectile = true;
				Hit.PhysType = 2;
				UWRCombatLibrary::ApplyHit(*It, Hit);
			}
		}
	}
	if (Pieces) Pieces->BatchUpdateInstancesTransforms(0, Xf, true, true, false);
}
