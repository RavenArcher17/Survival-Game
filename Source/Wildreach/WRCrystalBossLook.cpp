// The Crystal bosses' machines and bodies as every machine draws them (see WRCrystalBosses.h; cooks/biome-crystal.md
// 2.13-2.14, build-biome-crystal.md Part 2 P2.4-P2.7). Everything that moves is a pure function of the server clock and a
// replicated start time: the gimbal's quarter turn eases over 1 s and its ring swings and settles, a spare lens rides the
// magazine's chain up over the pulley and along the ceiling track for 20 s while the counterweight rises in its slot, a
// struck chime swings on its shackle and dies away, a turntable's disc eases its 45 degrees as the lever swings and
// springs back. Parts are code meshes (FWRToolMesh): bronze and lead on M_HeldTool, crystal on M_CrystalGlow (so it
// lights by sound like the halls), hard light on M_ArcaneWard.

#include "WRCrystalBosses.h"
#include "Wildreach.h"
#include "WRBoss.h"
#include "WRCreature.h"
#include "WRCrystalWorld.h"
#include "WRFx.h"
#include "WRSound.h"
#include "WRToolMesh.h"
#include "WRTuning.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"

namespace WRCrystalBossLookImpl
{
	const FLinearColor ClBronze = FLinearColor(FColor(140, 106, 58)) * 0.6f;
	const FLinearColor ClBronzeDark = FLinearColor(FColor(96, 72, 40)) * 0.55f;
	const FLinearColor ClLead = FLinearColor(FColor(90, 92, 98)) * 0.55f;
	const FLinearColor ClIron = FLinearColor(0.03f, 0.028f, 0.027f);
	const FLinearColor ClBasalt = FLinearColor(FColor(28, 28, 32)) * 0.8f;
	const FLinearColor ClViolet = FLinearColor(FColor(107, 63, 160));
	const FLinearColor ClLilac = FLinearColor(FColor(216, 208, 255));
	const FLinearColor ClSpar = FLinearColor(FColor(236, 240, 248));
	const FLinearColor ClPlateC = FLinearColor(FColor(184, 160, 232));

	float ClNow(const UObject* Ctx)
	{
		const UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
		const AGameStateBase* GS = W ? W->GetGameState() : nullptr;
		return GS ? float(GS->GetServerWorldTimeSeconds()) : (W ? W->GetTimeSeconds() : 0.f);
	}
	float ClEase(float A) { A = FMath::Clamp(A, 0.f, 1.f); return A * A * (3.f - 2.f * A); }
	void ClHitShape(UPrimitiveComponent* C)
	{
		C->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		C->SetCollisionObjectType(ECC_WorldDynamic);
		C->SetCollisionResponseToAllChannels(ECR_Ignore);
		C->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
		C->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		C->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		C->SetGenerateOverlapEvents(false);
		C->SetCanEverAffectNavigation(false);
	}
	UMaterialInstanceDynamic* ClGlow(UObject* Outer, const FLinearColor& Col, float Rest)
	{
		UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_CrystalGlow.M_CrystalGlow"));
		if (!Mat) return nullptr;
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Mat, Outer);
		M->SetVectorParameterValue(TEXT("GlowColor"), Col);
		M->SetScalarParameterValue(TEXT("GlowScale"), WRTune(TEXT("crystalWorld.glow.scale"), 6.f));
		M->SetScalarParameterValue(TEXT("Rest"), Rest);
		return M;
	}
	UMaterialInstanceDynamic* ClWard(UObject* Outer, const FLinearColor& Col, float Glow)
	{
		UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_ArcaneWard.M_ArcaneWard"));
		if (!Mat) Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_VoidLight.M_VoidLight"));
		if (!Mat) return nullptr;
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Mat, Outer);
		M->SetVectorParameterValue(TEXT("Color"), Col);
		M->SetScalarParameterValue(TEXT("Glow"), Glow);
		return M;
	}
	UProceduralMeshComponent* ClMesh(AActor* Owner, USceneComponent* Under, const FWRToolMesh& M, TArray<TObjectPtr<USceneComponent>>& Track)
	{
		UProceduralMeshComponent* P = NewObject<UProceduralMeshComponent>(Owner);
		P->SetupAttachment(Under ? Under : Owner->GetRootComponent());
		P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		P->SetCanEverAffectNavigation(false);
		P->RegisterComponent();
		M.ApplyTo(P);
		Track.Add(P);
		return P;
	}
	USceneComponent* ClPivot(AActor* Owner, USceneComponent* Under, const FVector& Rel, TArray<TObjectPtr<USceneComponent>>& Track)
	{
		USceneComponent* S = NewObject<USceneComponent>(Owner);
		S->SetupAttachment(Under ? Under : Owner->GetRootComponent());
		S->SetRelativeLocation(Rel);
		S->RegisterComponent();
		Track.Add(S);
		return S;
	}
	/** A ring (a closed tube) of radius R round Axis at C. */
	void ClRing(FWRToolMesh& M, const FVector& C, const FVector& Axis, float R, float Thick, const FLinearColor& Col, float Rough, int32 Seg = 24)
	{
		FVector U, V;
		Axis.GetSafeNormal().FindBestAxisVectors(U, V);
		TArray<FVector> P;
		for (int32 k = 0; k < Seg; ++k) { const float A = 2.f * PI * k / Seg; P.Add(C + (U * FMath::Cos(A) + V * FMath::Sin(A)) * R); }
		M.Tube(P, Thick, 6, Col, Rough, true);
	}
	/** A solid disc (a short lathe) of radius R and thickness T along Axis at C. */
	void ClDisc(FWRToolMesh& M, const FVector& C, const FVector& Axis, float R, float T, const FLinearColor& Col, float Rough, int32 Sides = 24)
	{
		M.Lathe(C - Axis.GetSafeNormal() * T * 0.5f, Axis.GetSafeNormal(), T, { FVector(0.f, R * 0.96f, R * 0.96f), FVector(0.15f, R, R), FVector(0.85f, R, R), FVector(1.f, R * 0.96f, R * 0.96f) }, Sides, Col, Rough);
	}
	/** One quartz point (six sides and a pyramid tip), glow in vertex alpha (tip bright). */
	void ClPoint(FWRToolMesh& M, const FVector& Root, const FVector& Dir, float Len, float Thick, const FLinearColor& Base, const FLinearColor& Tip)
	{
		FVector AY, AZ;
		Dir.GetSafeNormal().FindBestAxisVectors(AY, AZ);
		const FVector D = Dir.GetSafeNormal();
		const FVector Sh = Root + D * Len * 0.8f, Tp = Root + D * Len;
		FVector R0[6], R1[6];
		for (int32 k = 0; k < 6; ++k) { const float A = k * PI / 3.f; const FVector O = (AY * FMath::Cos(A) + AZ * FMath::Sin(A)) * Thick * 0.5f; R0[k] = Root + O * 0.9f; R1[k] = Sh + O; }
		const FColor C0 = FWRToolMesh::Pack(Base, 0.3f), C1 = FWRToolMesh::Pack(Tip, 1.f);
		const FVector2D Z = FVector2D::ZeroVector;
		for (int32 k = 0; k < 6; ++k) { const int32 n = (k + 1) % 6; M.Tri(R0[k], R1[n], R1[k], C0, Z, Z, Z); M.Tri(R0[k], R0[n], R1[n], C0, Z, Z, Z); M.Tri(R1[k], R1[n], Tp, C1, Z, Z, Z); }
	}
	/** A flat plate (a lozenge with thickness). */
	void ClPlate(FWRToolMesh& M, const FVector& C, const FVector& Along, const FVector& Normal, float Long, float Wide, float Thick, const FLinearColor& Face)
	{
		const FVector Side = FVector::CrossProduct(Normal, Along).GetSafeNormal();
		TArray<TArray<FVector>> Rings;
		for (const float T : { -0.5f, 0.5f })
		{
			const FVector O = C + Normal * Thick * T;
			Rings.Add({ O + Along * Long * 0.5f, O + Side * Wide * 0.5f, O - Along * Long * 0.5f, O - Side * Wide * 0.5f });
		}
		M.Loft(Rings, Face, 0.15f, true);
	}
	/** A hexafoil of thin tubes in the plane (Right, Up) round C. */
	void ClHexafoil(FWRToolMesh& M, const FVector& C, const FVector& Right, const FVector& Up, float R, float Thick, const FLinearColor& Col)
	{
		const auto Ring = [&](const FVector& At, float Rr)
		{
			TArray<FVector> P;
			for (int32 k = 0; k < 24; ++k) { const float A = k * 2.f * PI / 24.f; P.Add(At + (Right * FMath::Cos(A) + Up * FMath::Sin(A)) * Rr); }
			M.Tube(P, Thick, 4, Col, 1.f, true);
		};
		Ring(C, R);
		for (int32 k = 0; k < 6; ++k) { const float A = k * PI / 3.f; Ring(C + (Right * FMath::Cos(A) + Up * FMath::Sin(A)) * R * 0.5f, R * 0.5f); }
	}
	/** A chain: oval links along a polyline, each turned 90 degrees to the last (real chain). */
	void ClChain(FWRToolMesh& M, const TArray<FVector>& Path, float Link, const FLinearColor& Col)
	{
		float Carry = 0.f;
		int32 n = 0;
		for (int32 i = 0; i + 1 < Path.Num(); ++i)
		{
			const FVector A = Path[i], B = Path[i + 1];
			const float L = float(FVector::Dist(A, B));
			const FVector D = (B - A).GetSafeNormal();
			FVector U, V;
			D.FindBestAxisVectors(U, V);
			for (float T = Carry; T < L; T += Link * 0.8f, ++n)
			{
				const FVector C = A + D * T;
				const FVector Across = (n % 2 ? U : V);
				TArray<FVector> P;
				for (int32 k = 0; k < 10; ++k) { const float Ang = 2.f * PI * k / 10.f; P.Add(C + D * FMath::Cos(Ang) * Link * 0.55f + Across * FMath::Sin(Ang) * Link * 0.3f); }
				M.Tube(P, Link * 0.09f, 4, Col, 0.5f, true);
				Carry = T + Link * 0.8f - L;
			}
		}
	}
	/** A Spar Lens: a water-clear disc of doubling spar (its two images faintly visible as a second rim), its edge cut in
	 *  rhombs (twelve facets) and set in a bronze bezel. Axis = its optical axis. */
	void ClLensGlass(FWRToolMesh& Glass, FWRToolMesh& Bezel, const FVector& C, const FVector& Axis, float R)
	{
		FVector U, V;
		Axis.GetSafeNormal().FindBestAxisVectors(U, V);
		const FVector2D Z = FVector2D::ZeroVector;
		const int32 Facets = 12;
		const FVector Front = Axis.GetSafeNormal() * 7.f, Back = -Front;
		for (int32 k = 0; k < Facets; ++k)
		{
			const float A0 = 2.f * PI * k / Facets, A1 = 2.f * PI * (k + 1) / Facets, Am = (A0 + A1) * 0.5f;
			const FVector E0 = C + (U * FMath::Cos(A0) + V * FMath::Sin(A0)) * R, E1 = C + (U * FMath::Cos(A1) + V * FMath::Sin(A1)) * R;
			const FVector Em = C + (U * FMath::Cos(Am) + V * FMath::Sin(Am)) * R * 1.03f;
			const FColor Mid = FWRToolMesh::Pack(ClSpar, 0.35f), Rim = FWRToolMesh::Pack(ClSpar, 0.9f);
			// The two faces (domed a little: the centre stands proud) and the rhomb-cut rim between them.
			Glass.Tri(C + Front * 1.4f, E0 + Front, E1 + Front, Mid, Z, Z, Z);
			Glass.Tri(C + Back * 1.4f, E1 + Back, E0 + Back, Mid, Z, Z, Z);
			Glass.Tri(E0 + Front, Em, E1 + Front, Rim, Z, Z, Z);
			Glass.Tri(E0 + Back, E1 + Back, Em, Rim, Z, Z, Z);
			Glass.Tri(E0 + Front, E0 + Back, Em, Rim, Z, Z, Z);
			Glass.Tri(E1 + Back, E1 + Front, Em, Rim, Z, Z, Z);
		}
		// The doubled image: a second faint rim inside, offset (spar's double refraction).
		ClRing(Glass, C + U * R * 0.06f, Axis, R * 0.82f, 1.2f, ClSpar * 0.8f, 0.2f, 32);
		ClRing(Bezel, C, Axis, R + 6.f, 5.f, ClBronze, 0.45f, 32);
	}
}
using namespace WRCrystalBossLookImpl;

// ================================================================================================ AWRCrystalBossProp

AWRCrystalBossProp::AWRCrystalBossProp()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	SetNetUpdateFrequency(4.f);
	SetNetCullDistanceSquared(FMath::Square(25000.f));
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	HitShape = CreateDefaultSubobject<USphereComponent>(TEXT("HitShape"));
	HitShape->SetupAttachment(RootComponent);
	HitShape->InitSphereRadius(80.f);
	ClHitShape(HitShape);
	Solid = CreateDefaultSubobject<UBoxComponent>(TEXT("Solid"));
	Solid->SetupAttachment(RootComponent);
	Solid->SetBoxExtent(FVector(10.f));
	Solid->SetCollisionProfileName(TEXT("BlockAll"));
	Solid->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Solid->SetCanEverAffectNavigation(false);
}

void AWRCrystalBossProp::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRCrystalBossProp, Kind);
	DOREPLIFETIME(AWRCrystalBossProp, State);
	DOREPLIFETIME(AWRCrystalBossProp, Index);
	DOREPLIFETIME(AWRCrystalBossProp, Clicks);
	DOREPLIFETIME(AWRCrystalBossProp, TurnAt);
	DOREPLIFETIME(AWRCrystalBossProp, LowerAt);
	DOREPLIFETIME(AWRCrystalBossProp, Value);
	DOREPLIFETIME(AWRCrystalBossProp, Size);
}

TArray<TWeakObjectPtr<AWRCrystalBossProp>>& AWRCrystalBossProp::All()
{
	static TArray<TWeakObjectPtr<AWRCrystalBossProp>> List;
	List.RemoveAll([](const TWeakObjectPtr<AWRCrystalBossProp>& P) { return !P.IsValid(); });
	return List;
}

float AWRCrystalBossProp::ServerNow() const
{
	return ClNow(this);
}

void AWRCrystalBossProp::BeginPlay()
{
	Super::BeginPlay();
	All().AddUnique(this);
	Build();
}

void AWRCrystalBossProp::EndPlay(const EEndPlayReason::Type Reason)
{
	All().Remove(this);
	Super::EndPlay(Reason);
}

void AWRCrystalBossProp::SetState(uint8 NewState)
{
	if (!HasAuthority() || State == NewState) return;
	State = NewState;
	OnRep_Look();
	ForceNetUpdate();
}

void AWRCrystalBossProp::OnRep_Look()
{
	if (Kind != BuiltKind || State != BuiltState) Build();
}

float AWRCrystalBossProp::FacingNow(float Now) const
{
	if (Kind == TEXT("turntable"))
	{
		// Home facing: its own light shaft outward (index x 45 + 22.5 from the hall's yaw, built into the actor's yaw);
		// each click is 45 degrees clockwise from above, eased over the 0.8 s the disc takes to come round.
		const float Turn = WRTune(TEXT("bosses.lyrathis.turnSeconds"), 0.8f);
		const float A = ClEase((Now - TurnAt) / FMath::Max(0.05f, Turn));
		return GetActorRotation().Yaw + (Clicks > 0 ? 45.f * (float(Clicks) - 1.f + A) : 0.f);
	}
	if (Kind == TEXT("lens"))
	{
		const float A = ClEase((Now - TurnAt) / FMath::Max(0.05f, WRTune(TEXT("bosses.mirrored_twins.flareSeconds"), 1.f)));
		return GetActorRotation().Yaw + (Clicks > 0 ? 90.f * (float(Clicks) - 1.f + A) : 0.f);
	}
	return GetActorRotation().Yaw;
}

FVector AWRCrystalBossProp::HitPoint() const
{
	if (Kind == TEXT("lens")) return GetActorLocation() - FVector(0.f, 0.f, 260.f);
	if (Kind == TEXT("chime")) return GetActorLocation() + GetActorForwardVector() * 95.f + FVector(0.f, 0.f, Size * 0.62f);
	if (Kind == TEXT("turntable")) return GetActorTransform().TransformPosition(FVector(0.f, 260.f, 150.f));
	if (Kind == TEXT("lance")) return GetActorLocation() + FVector(0.f, 0.f, 120.f);
	return GetActorLocation();
}

FVector AWRCrystalBossProp::PrismCentre() const
{
	return GetActorLocation() + FVector(0.f, 0.f, 60.f + Size * 0.5f);
}

void AWRCrystalBossProp::Build()
{
	BuiltKind = Kind;
	BuiltState = State;
	for (USceneComponent* C : LookComps) if (C) C->DestroyComponent();
	LookComps.Reset();
	Glows.Reset();
	PartA = PartB = PartC = PartD = nullptr;
	Glow = nullptr;
	HitShape->SetRelativeLocation(GetActorTransform().InverseTransformPosition(HitPoint()));
	HitShape->SetSphereRadius(Kind == TEXT("lens") ? 90.f : Kind == TEXT("chime") ? 95.f : Kind == TEXT("turntable") ? 45.f : 60.f);
	ClHitShape(HitShape);
	if (Kind == TEXT("chime") && State >= 2) HitShape->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Kind == TEXT("lens") && State != 0) HitShape->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (GetNetMode() == NM_DedicatedServer) return;

	if (Kind == TEXT("lens"))
	{
		// The light vent's bronze collar (fixed to the ceiling), the sprocket and its chain at its rim, and the pale
		// shaft of light that comes down through the lens onto the water.
		FWRToolMesh Fixed;
		ClRing(Fixed, FVector(0.f, 0.f, 20.f), FVector::UpVector, 120.f, 7.f, ClBronze, 0.45f, 40);
		ClRing(Fixed, FVector(0.f, 0.f, 34.f), FVector::UpVector, 128.f, 4.f, ClBronzeDark, 0.5f, 40);
		for (int32 k = 0; k < 6; ++k)
		{
			const float A = k * PI / 3.f;
			Fixed.Tube({ FVector(FMath::Cos(A) * 120.f, FMath::Sin(A) * 120.f, 20.f), FVector(FMath::Cos(A) * 128.f, FMath::Sin(A) * 128.f, 60.f) }, 4.f, 5, ClBronze, 0.45f);
		}
		// The sprocket (a toothed wheel on the collar's rim) and the drive chain looped round the yoke's hub pulley.
		const FVector Spr(150.f, 0.f, 10.f);
		ClDisc(Fixed, Spr, FVector::UpVector, 26.f, 6.f, ClBronze, 0.45f, 18);
		for (int32 t = 0; t < 12; ++t) { const float A = t * PI / 6.f; Fixed.Tube({ Spr + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * 24.f, Spr + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * 31.f }, 2.5f, 4, ClBronze, 0.45f); }
		Fixed.Tube({ Spr + FVector(0.f, 0.f, 4.f), Spr + FVector(0.f, 0.f, 60.f) }, 4.f, 6, ClIron, 0.6f);
		ClChain(Fixed, { Spr + FVector(0.f, 28.f, 0.f), FVector(0.f, 22.f, 0.f), FVector(0.f, -22.f, 0.f), Spr + FVector(0.f, -28.f, 0.f) }, 8.f, ClIron);
		UProceduralMeshComponent* FixedMesh = ClMesh(this, nullptr, Fixed, LookComps);
		(void)FixedMesh;
		// The shaft of light down from the vent through the lens to the floor (Size = the hall's height).
		FWRToolMesh Shaft;
		Shaft.Lathe(FVector(0.f, 0.f, 30.f), FVector(0.f, 0.f, -1.f), Size, { FVector(0.f, 60.f, 60.f), FVector(1.f, 150.f, 150.f) }, 20, ClLilac, 0.f);
		if (UProceduralMeshComponent* S = ClMesh(this, nullptr, Shaft, LookComps)) if (UMaterialInstanceDynamic* W = ClWard(S, ClLilac, 0.08f)) { S->SetMaterial(0, W); Glows.Add(W); }
		// The yoke (turns about the vertical), its outer ring (pivots across the yoke's arms), the lens (pivots across the ring).
		PartA = ClPivot(this, nullptr, FVector::ZeroVector, LookComps);
		FWRToolMesh Yoke;
		Yoke.Lathe(FVector(0.f, 0.f, 0.f), FVector(0.f, 0.f, -1.f), 70.f, { FVector(0.f, 22.f, 22.f), FVector(0.4f, 16.f, 16.f), FVector(1.f, 12.f, 12.f) }, 12, ClBronze, 0.45f);
		ClDisc(Yoke, FVector(0.f, 0.f, -4.f), FVector::UpVector, 24.f, 10.f, ClBronzeDark, 0.5f, 16);   // the hub pulley the chain runs on
		Yoke.Tube({ FVector(0.f, -110.f, -78.f), FVector(0.f, 110.f, -78.f) }, 9.f, 8, ClBronze, 0.45f);
		for (const float S : { -1.f, 1.f })
		{
			Yoke.Tube({ FVector(0.f, S * 110.f, -78.f), FVector(0.f, S * 110.f, -260.f) }, 8.f, 8, ClBronze, 0.45f);
			ClDisc(Yoke, FVector(0.f, S * 110.f, -260.f), FVector(0.f, 1.f, 0.f), 16.f, 14.f, ClBronzeDark, 0.5f, 12);
		}
		ClMesh(this, PartA, Yoke, LookComps);
		PartB = ClPivot(this, PartA, FVector(0.f, 0.f, -260.f), LookComps);
		FWRToolMesh Ring;
		ClRing(Ring, FVector::ZeroVector, FVector(1.f, 0.f, 0.f), 96.f, 6.f, ClBronze, 0.45f, 36);
		for (const float S : { -1.f, 1.f })
		{
			Ring.Tube({ FVector(0.f, S * 96.f, 0.f), FVector(0.f, S * 104.f, 0.f) }, 5.f, 6, ClBronze, 0.45f);
			Ring.Tube({ FVector(0.f, 0.f, S * 96.f), FVector(0.f, 0.f, S * 90.f) }, 4.f, 6, ClBronze, 0.45f);
		}
		ClMesh(this, PartB, Ring, LookComps);
		PartC = ClPivot(this, PartB, FVector::ZeroVector, LookComps);
		if (State == 0)
		{
			FWRToolMesh Glass, Bezel;
			ClLensGlass(Glass, Bezel, FVector::ZeroVector, FVector(1.f, 0.f, 0.f), 75.f);
			for (const float S : { -1.f, 1.f }) Bezel.Tube({ FVector(0.f, 0.f, S * 81.f), FVector(0.f, 0.f, S * 90.f) }, 3.5f, 6, ClBronze, 0.45f);
			if (UProceduralMeshComponent* G = ClMesh(this, PartC, Glass, LookComps)) if (UMaterialInstanceDynamic* M = ClGlow(G, FLinearColor(0.85f, 0.85f, 1.f), 0.25f)) { G->SetMaterial(0, M); Glows.Add(M); }
			ClMesh(this, PartC, Bezel, LookComps);
		}
		Glow = NewObject<UPointLightComponent>(this);
		Glow->SetupAttachment(PartC);
		Glow->SetLightColor(ClLilac);
		Glow->SetIntensity(0.f);
		Glow->SetAttenuationRadius(2500.f);
		Glow->SetCastShadows(false);
		Glow->RegisterComponent();
		LookComps.Add(Glow);
	}
	else if (Kind == TEXT("magazine"))
	{
		// The lens magazine (crystal 2.13.1): a bronze frame on the north wall with three cradles stacked up it, a chain
		// from the top cradle over a pulley at the ceiling down to a lead counterweight running in a slot cut in the wall
		// beside it, and a bronze track along the ceiling to the lens's yoke. Local +Y is into the room.
		const float H = Size;
		FWRToolMesh Frame;
		for (const float X : { -80.f, 80.f })
		{
			Frame.Tube({ FVector(X, 14.f, 0.f), FVector(X, 14.f, H * 0.55f) }, 7.f, 8, ClBronze, 0.45f);
			for (const float Z : { 30.f, H * 0.3f, H * 0.53f }) Frame.Tube({ FVector(X, 0.f, Z), FVector(X, 16.f, Z) }, 5.f, 6, ClBronzeDark, 0.5f);   // brackets into the wall
		}
		for (int32 c = 0; c < 3; ++c)
		{
			// A cradle: a bronze crescent the lens's rim sits in, its two horns lipped so it can't roll out.
			const float Z = 150.f + c * 110.f;
			TArray<FVector> Cres;
			for (int32 k = 0; k <= 12; ++k) { const float A = PI * (1.1f + 0.8f * k / 12.f); Cres.Add(FVector(FMath::Cos(A) * 82.f, 40.f, Z + 80.f + FMath::Sin(A) * 82.f)); }
			Frame.Tube(Cres, 5.f, 6, ClBronze, 0.45f);
			Frame.Tube({ FVector(-80.f, 14.f, Z), FVector(-58.f, 40.f, Z + 30.f) }, 4.f, 6, ClBronze, 0.45f);
			Frame.Tube({ FVector(80.f, 14.f, Z), FVector(58.f, 40.f, Z + 30.f) }, 4.f, 6, ClBronze, 0.45f);
		}
		// The ceiling pulley on its axle into the wall, and the track along the ceiling to the room's middle.
		const FVector Pul(0.f, 40.f, H - 70.f);
		ClDisc(Frame, Pul, FVector(1.f, 0.f, 0.f), 40.f, 12.f, ClBronze, 0.45f, 20);
		Frame.Tube({ Pul + FVector(-12.f, 0.f, 0.f), Pul + FVector(-12.f, -40.f, 0.f) }, 5.f, 6, ClIron, 0.6f);
		Frame.Tube({ FVector(0.f, 20.f, H - 20.f), Pul + FVector(0.f, 0.f, 40.f) }, 5.f, 6, ClIron, 0.6f);
		const float RoomMid = WRTune(TEXT("bosses.mirrored_twins.magazineReach"), 1970.f);
		Frame.Tube({ FVector(0.f, 40.f, H - 18.f), FVector(0.f, RoomMid, H - 18.f) }, 4.f, 6, ClBronze, 0.45f);
		Frame.Tube({ FVector(0.f, 40.f, H - 28.f), FVector(0.f, RoomMid, H - 28.f) }, 2.5f, 6, ClBronzeDark, 0.5f);
		for (float Y = 300.f; Y < RoomMid; Y += 300.f) Frame.Tube({ FVector(0.f, Y, H - 18.f), FVector(0.f, Y, H) }, 3.f, 6, ClBronzeDark, 0.5f);   // hangers
		// The slot cut in the wall for the counterweight (a dark groove the weight runs in) and the chain's fixed run.
		Frame.Tube({ FVector(160.f, 4.f, 20.f), FVector(160.f, 4.f, H - 90.f) }, 24.f, 4, ClBasalt * 0.3f, 0.9f);
		ClChain(Frame, { Pul + FVector(0.f, 0.f, 40.f), Pul + FVector(0.f, 0.f, 40.f) + FVector(160.f, -30.f, 0.f), FVector(160.f, 10.f, H - 120.f) }, 9.f, ClIron);
		ClMesh(this, nullptr, Frame, LookComps);
		// The spares in their cradles (Value), the one travelling (PartA) and the counterweight (PartB).
		for (int32 c = 0; c < 3; ++c)
		{
			USceneComponent* Sp = ClPivot(this, nullptr, FVector(0.f, 40.f, 230.f + c * 110.f), LookComps);
			FWRToolMesh Glass, Bezel;
			ClLensGlass(Glass, Bezel, FVector::ZeroVector, FVector(0.f, 1.f, 0.f), 75.f);
			if (UProceduralMeshComponent* G = ClMesh(this, Sp, Glass, LookComps)) if (UMaterialInstanceDynamic* M = ClGlow(G, FLinearColor(0.85f, 0.85f, 1.f), 0.18f)) { G->SetMaterial(0, M); Glows.Add(M); }
			ClMesh(this, Sp, Bezel, LookComps);
			Sp->ComponentTags.Add(*FString::Printf(TEXT("spare%d"), c));
		}
		PartA = ClPivot(this, nullptr, FVector(0.f, 40.f, 230.f + 2 * 110.f), LookComps);
		{
			FWRToolMesh Glass, Bezel;
			ClLensGlass(Glass, Bezel, FVector::ZeroVector, FVector(0.f, 1.f, 0.f), 75.f);
			ClChain(Bezel, { FVector(0.f, 0.f, 81.f), FVector(0.f, 0.f, 140.f) }, 9.f, ClIron);
			if (UProceduralMeshComponent* G = ClMesh(this, PartA, Glass, LookComps)) if (UMaterialInstanceDynamic* M = ClGlow(G, FLinearColor(0.85f, 0.85f, 1.f), 0.18f)) { G->SetMaterial(0, M); Glows.Add(M); }
			ClMesh(this, PartA, Bezel, LookComps);
		}
		PartB = ClPivot(this, nullptr, FVector(160.f, 4.f, 60.f), LookComps);
		{
			FWRToolMesh W;
			W.Lathe(FVector(0.f, 0.f, 0.f), FVector::UpVector, 80.f, { FVector(0.f, 20.f, 20.f), FVector(0.1f, 22.f, 22.f), FVector(0.9f, 22.f, 22.f), FVector(1.f, 18.f, 18.f) }, 8, ClLead, 0.6f);
			W.Tube({ FVector(0.f, 0.f, 80.f), FVector(0.f, 0.f, 96.f) }, 3.f, 6, ClIron, 0.6f);
			ClMesh(this, PartB, W, LookComps);
		}
	}
	else if (Kind == TEXT("chime"))
	{
		// A chime stone (crystal 2.13.2): a bronze post on a stone foot, an arm out toward the hall's middle (+X), the
		// 2 m crystal bell on a shackle under its end. Broken: the bell lies shattered at the post's foot.
		FWRToolMesh Post;
		Post.Lathe(FVector::ZeroVector, FVector::UpVector, 40.f, { FVector(0.f, 46.f, 46.f), FVector(1.f, 38.f, 38.f) }, 8, ClBasalt * 1.3f, 0.85f);
		Post.Lathe(FVector(0.f, 0.f, 40.f), FVector::UpVector, Size - 40.f, { FVector(0.f, 12.f, 12.f), FVector(0.95f, 10.f, 10.f), FVector(1.f, 12.f, 12.f) }, 10, ClBronze, 0.45f);
		Post.Tube({ FVector(0.f, 0.f, Size - 10.f), FVector(95.f, 0.f, Size - 10.f) }, 7.f, 8, ClBronze, 0.45f);
		Post.Tube({ FVector(0.f, 0.f, Size - 70.f), FVector(60.f, 0.f, Size - 14.f) }, 4.f, 6, ClBronze, 0.45f);   // the brace
		ClRing(Post, FVector(95.f, 0.f, Size - 22.f), FVector(1.f, 0.f, 0.f), 9.f, 2.5f, ClIron, 0.6f, 12);
		ClMesh(this, nullptr, Post, LookComps);
		if (State < 2)
		{
			PartA = ClPivot(this, nullptr, FVector(95.f, 0.f, Size - 30.f), LookComps);
			FWRToolMesh Bell;
			// The bell: a crystal lathe, mouth down (2 m), its lip thickened; cracked: dark lines up its side.
			TArray<FVector> Prof = { FVector(0.f, 8.f, 8.f), FVector(0.08f, 26.f, 26.f), FVector(0.35f, 36.f, 36.f), FVector(0.7f, 46.f, 46.f), FVector(0.92f, 62.f, 62.f), FVector(1.f, 66.f, 66.f) };
			Bell.Lathe(FVector::ZeroVector, FVector(0.f, 0.f, -1.f), Size * 0.6f, Prof, 18, ClPlateC, 0.12f);
			if (State == 1)
			{
				for (int32 k = 0; k < 3; ++k)
				{
					const float A = 0.6f + k * 1.9f;
					Bell.Tube({ FVector(FMath::Cos(A) * 40.f, FMath::Sin(A) * 40.f, -Size * 0.25f), FVector(FMath::Cos(A + 0.2f) * 55.f, FMath::Sin(A + 0.2f) * 55.f, -Size * 0.45f), FVector(FMath::Cos(A + 0.1f) * 64.f, FMath::Sin(A + 0.1f) * 64.f, -Size * 0.58f) }, 1.4f, 3, ClIron, 0.9f);
				}
			}
			ClRing(Bell, FVector(0.f, 0.f, 6.f), FVector(1.f, 0.f, 0.f), 8.f, 2.5f, ClIron, 0.6f, 10);
			if (UProceduralMeshComponent* B = ClMesh(this, PartA, Bell, LookComps)) if (UMaterialInstanceDynamic* M = ClGlow(B, FLinearColor(0.75f, 0.65f, 1.f), State == 1 ? 0.12f : 0.28f)) { B->SetMaterial(0, M); Glows.Add(M); if (UWRCrystalWorld* W = UWRCrystalWorld::Get(this)) W->RegisterGlow(M, GetActorLocation()); }
		}
		else
		{
			FWRToolMesh Shards;
			FRandomStream Rng(Index * 31 + 7);
			for (int32 k = 0; k < 18; ++k)
			{
				const float A = Rng.FRandRange(0.f, 2.f * PI), D = Rng.FRandRange(40.f, 190.f);
				ClPoint(Shards, FVector(FMath::Cos(A) * D + 60.f, FMath::Sin(A) * D, 2.f), FVector(Rng.FRandRange(-1.f, 1.f), Rng.FRandRange(-1.f, 1.f), Rng.FRandRange(0.f, 0.4f)), Rng.FRandRange(25.f, 70.f), Rng.FRandRange(10.f, 22.f), ClPlateC, ClLilac);
			}
			if (UProceduralMeshComponent* S = ClMesh(this, nullptr, Shards, LookComps)) if (UMaterialInstanceDynamic* M = ClGlow(S, FLinearColor(0.7f, 0.6f, 1.f), 0.08f)) S->SetMaterial(0, M);
		}
	}
	else
	{
		// The Prism Hall's machines are built in WRCrystalLyrathis.cpp.
		USceneComponent *A = nullptr, *B = nullptr, *C = nullptr, *D = nullptr;
		WRCrystalBuildPrismProp(this, LookComps, A, B, C, D, Glows);
		PartA = A; PartB = B; PartC = C; PartD = D;
	}
	ShownClicks = -1;
}

void AWRCrystalBossProp::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (GetNetMode() == NM_DedicatedServer) return;
	TickLook(ServerNow(), DeltaSeconds);
}

void AWRCrystalBossProp::TickLook(float Now, float Dt)
{
	if (Kind == TEXT("lens") && PartA)
	{
		// The yoke comes round a quarter on each swap (eased 1 s) and the ring swings on its pivots and settles.
		PartA->SetWorldRotation(FRotator(0.f, FacingNow(Now), 0.f));
		const float Since = Now - TurnAt;
		const float Swing = Since > 0.f && Since < 6.f ? 14.f * FMath::Exp(-Since * 1.4f) * FMath::Sin(Since * 2.f * PI / 1.3f) : 0.f;
		if (PartB) PartB->SetRelativeRotation(FRotator(0.f, 0.f, Swing));
		if (PartC) PartC->SetRelativeRotation(FRotator(20.f + Swing * 0.6f, 0.f, 0.f));
		const float Flare = Since >= 0.f && Since < WRTune(TEXT("bosses.mirrored_twins.flareSeconds"), 1.f) ? FMath::Sin(PI * Since / WRTune(TEXT("bosses.mirrored_twins.flareSeconds"), 1.f)) : 0.f;
		if (Glow) Glow->SetIntensity(State == 0 ? 800.f + Flare * 20000.f : 0.f);
		for (UMaterialInstanceDynamic* M : Glows) if (M) M->SetScalarParameterValue(TEXT("Rest"), 0.25f + Flare * 2.5f);
		return;
	}
	if (Kind == TEXT("magazine") && PartA)
	{
		// A spare lowering: up the chain to the pulley (40% of its 20 s), then along the ceiling track to the yoke; the
		// counterweight rises in its slot as the spare goes (they balance); the cradles show what's left.
		const float Spare = WRTune(TEXT("bosses.mirrored_twins.spareSeconds"), 20.f);
		const float A = LowerAt > 0.f ? FMath::Clamp((Now - LowerAt) / Spare, 0.f, 1.f) : 0.f;
		const bool bMoving = LowerAt > 0.f && A < 1.f;
		const float H = Size;
		const float RoomMid = WRTune(TEXT("bosses.mirrored_twins.magazineReach"), 1970.f);
		PartA->SetVisibility(bMoving, true);
		if (bMoving)
		{
			const FVector From(0.f, 40.f, 230.f + 2 * 110.f), Pul(0.f, 40.f, H - 160.f), Mid(0.f, RoomMid, H - 160.f);
			const FVector P = A < 0.4f ? FMath::Lerp(From, Pul, ClEase(A / 0.4f)) : FMath::Lerp(Pul, Mid, ClEase((A - 0.4f) / 0.6f));
			PartA->SetRelativeLocation(P);
			PartA->SetRelativeRotation(FRotator(0.f, 0.f, FMath::Sin(Now * 3.1f) * 3.f));   // it sways on its chain
		}
		if (PartB) PartB->SetRelativeLocation(FVector(160.f, 4.f, FMath::Lerp(60.f, H - 260.f, bMoving ? ClEase(A) : 0.f)));
		TArray<USceneComponent*> Kids;
		GetRootComponent()->GetChildrenComponents(false, Kids);
		for (USceneComponent* K : Kids)
		{
			for (int32 c = 0; c < 3; ++c) if (K->ComponentHasTag(*FString::Printf(TEXT("spare%d"), c))) K->SetVisibility(c < FMath::RoundToInt(Value), true);
		}
		return;
	}
	if (Kind == TEXT("chime") && PartA)
	{
		// Struck or hammered, the bell swings on its shackle and the swing dies away (a damped pendulum, 1.6 s period).
		const float Since = Now - TurnAt;
		const float Swing = Since >= 0.f && Since < 8.f ? 16.f * FMath::Exp(-Since * 0.9f) * FMath::Sin(Since * 2.f * PI / 1.6f) : 0.f;
		PartA->SetRelativeRotation(FRotator(Swing, 0.f, 0.f));
		const float Ring = Since >= 0.f && Since < 3.f ? FMath::Exp(-Since * 1.5f) : 0.f;
		for (UMaterialInstanceDynamic* M : Glows) if (M) M->SetScalarParameterValue(TEXT("Rest"), (State == 1 ? 0.12f : 0.28f) + Ring * 2.f);
		return;
	}
	WRCrystalTickPrismProp(this, Now, Dt, PartA.Get(), PartB.Get(), PartC.Get(), PartD.Get(), Glows);
}

// ================================================================================================ UWRCrystalBossBody

UWRCrystalBossBody::UWRCrystalBossBody()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UWRCrystalBossBody::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UWRCrystalBossBody, Pose);
	DOREPLIFETIME(UWRCrystalBossBody, Broken);
	DOREPLIFETIME(UWRCrystalBossBody, Point);
	DOREPLIFETIME(UWRCrystalBossBody, Level);
	DOREPLIFETIME(UWRCrystalBossBody, Gem);
	DOREPLIFETIME(UWRCrystalBossBody, bMirrored);
}

UWRCrystalBossBody* UWRCrystalBossBody::Get(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UWRCrystalBossBody>() : nullptr;
}

void UWRCrystalBossBody::Attach(AWRCreature* Boss)
{
	if (!Boss || !Boss->HasAuthority() || Get(Boss)) return;
	UWRCrystalBossBody* B = NewObject<UWRCrystalBossBody>(Boss, TEXT("CrystalBossBody"));
	B->bMirrored = Boss->CreatureId == TEXT("mirrored_twins");
	B->RegisterComponent();
}

void UWRCrystalBossBody::EndPlay(const EEndPlayReason::Type Reason)
{
	ClearLook();
	Super::EndPlay(Reason);
}

void UWRCrystalBossBody::ClearLook()
{
	for (USceneComponent* C : LookComps) if (C) C->DestroyComponent();
	LookComps.Reset();
	Parts.Reset();
	Glows.Reset();
	if (Reflection) { Reflection->DestroyComponent(); Reflection = nullptr; }
	if (Lamp) { Lamp->DestroyComponent(); Lamp = nullptr; }
}

FName UWRCrystalBossBody::BoneLike(const TCHAR* Hint) const
{
	const AWRCreature* C = Cast<AWRCreature>(GetOwner());
	const USkeletalMeshComponent* S = C ? C->GetCreatureSkel() : nullptr;
	if (!S || !S->GetSkeletalMeshAsset()) return NAME_None;
	const FReferenceSkeleton& Ref = S->GetSkeletalMeshAsset()->GetRefSkeleton();
	const FString H = FString(Hint).ToLower();
	for (int32 i = 0; i < Ref.GetNum(); ++i) if (Ref.GetBoneName(i).ToString().ToLower() == H) return Ref.GetBoneName(i);
	for (int32 i = 0; i < Ref.GetNum(); ++i) if (Ref.GetBoneName(i).ToString().ToLower().Contains(H)) return Ref.GetBoneName(i);
	return NAME_None;
}

USceneComponent* UWRCrystalBossBody::AddPart(FName Bone, const FTransform& Local, FWRToolMesh& M, bool bGlow, int32 Tag)
{
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	USkeletalMeshComponent* S = C ? C->GetCreatureSkel() : nullptr;
	if (!S) return nullptr;
	UProceduralMeshComponent* P = NewObject<UProceduralMeshComponent>(C);
	P->SetupAttachment(S, Bone);
	P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	P->SetUsingAbsoluteScale(true);
	P->RegisterComponent();
	// Modelled in the actor's frame at the boss's size: placed in world space from the bone, upright, then it follows.
	const FVector BoneAt = Bone.IsNone() ? C->GetActorLocation() : S->GetSocketLocation(Bone);
	P->SetWorldLocationAndRotation(BoneAt + C->GetActorRotation().RotateVector(Local.GetLocation()), C->GetActorRotation() + Local.Rotator());
	P->SetWorldScale3D(FVector::OneVector);
	M.ApplyTo(P);
	if (bGlow)
	{
		if (UMaterialInstanceDynamic* G = ClGlow(P, FLinearColor(0.62f, 0.42f, 1.f), 0.35f)) { P->SetMaterial(0, G); Glows.Add(G); }
	}
	LookComps.Add(P);
	FPart Part;
	Part.Comp = P;
	Part.Tag = Tag;
	Part.BaseRot = P->GetRelativeRotation();
	Part.BaseLoc = P->GetRelativeLocation();
	Parts.Add(Part);
	return P;
}

void UWRCrystalBossBody::ApplyTint()
{
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	USkeletalMeshComponent* Skel = C ? C->GetCreatureSkel() : nullptr;
	UMaterialInterface* Master = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Creatures/M_CreatureTint.M_CreatureTint"));
	if (!Skel || !Master) return;
	const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("creatures.json"));
	const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
	TSharedPtr<FJsonObject> Pick;
	if (Root.IsValid() && Root->TryGetArrayField(TEXT("creatures"), List))
	{
		for (const TSharedPtr<FJsonValue>& V : *List)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			const TSharedPtr<FJsonObject>* Look = nullptr;
			const TArray<TSharedPtr<FJsonValue>>* Tints = nullptr;
			FString Id;   // (the list's "_bosses"-style notes have no id)
			if (O.IsValid() && O->TryGetStringField(TEXT("id"), Id) && Id == C->CreatureId.ToString() && O->TryGetObjectField(TEXT("look"), Look) && (*Look)->TryGetArrayField(TEXT("tint"), Tints) && Tints->Num()) Pick = (*Tints)[0]->AsObject();
		}
	}
	if (!Pick.IsValid()) return;
	const auto Col = [&](const TCHAR* F, const FLinearColor& Def) { FString S; return Pick->TryGetStringField(F, S) ? FLinearColor(FColor::FromHex(S)) : Def; };
	const auto Num = [&](const TCHAR* F, float Def) { double V; return Pick->TryGetNumberField(F, V) ? float(V) : Def; };
	for (int32 Slot = 0; Slot < Skel->GetNumMaterials(); ++Slot)
	{
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Master, C);
		FString Tex;
		if (Pick->TryGetStringField(TEXT("tex"), Tex)) if (UTexture* T = LoadObject<UTexture>(nullptr, *Tex)) M->SetTextureParameterValue(TEXT("BaseColorTex"), T);
		if (UTexture* N = LoadObject<UTexture>(nullptr, TEXT("/Engine/EngineMaterials/DefaultNormal.DefaultNormal"))) M->SetTextureParameterValue(TEXT("NormalTex"), N);
		if (UTexture* B = LoadObject<UTexture>(nullptr, TEXT("/Engine/EngineResources/Black.Black"))) M->SetTextureParameterValue(TEXT("GlowTex"), B);
		M->SetScalarParameterValue(TEXT("Desaturation"), Num(TEXT("desat"), 1.f));
		M->SetScalarParameterValue(TEXT("Brightness"), Num(TEXT("bright"), 1.f));
		M->SetScalarParameterValue(TEXT("TwoTone"), Num(TEXT("twoTone"), 0.f));
		M->SetScalarParameterValue(TEXT("LightFrom"), Num(TEXT("lightFrom"), 0.f));
		M->SetScalarParameterValue(TEXT("Roughness"), Num(TEXT("rough"), 0.75f));
		M->SetScalarParameterValue(TEXT("GlowStrength"), Num(TEXT("glowStrength"), 0.f));
		M->SetVectorParameterValue(TEXT("Tint"), Col(TEXT("tint"), FLinearColor::White));
		M->SetVectorParameterValue(TEXT("TintLight"), Col(TEXT("light"), Col(TEXT("tint"), FLinearColor::White)));
		M->SetVectorParameterValue(TEXT("GlowColor"), Col(TEXT("glow"), FLinearColor(0.85f, 0.8f, 1.f)));
		Skel->SetMaterial(Slot, M);
	}
}

void UWRCrystalBossBody::BuildLook()
{
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	USkeletalMeshComponent* Skel = C ? C->GetCreatureSkel() : nullptr;
	const FWRCreatureDef* D = C ? C->GetDef() : nullptr;
	if (!C || !Skel || !Skel->GetSkeletalMeshAsset() || !D) return;
	ClearLook();
	LookFor = C->CreatureId;
	ApplyTint();
	const float R = D->Radius, H = D->HalfHeight;
	const FName Id = C->CreatureId;
	if (Id == TEXT("mirrored_twins") || Id == TEXT("mirrored_twin_double"))
	{
		const float K = H / 130.f;
		// The faceless helm: a hexafoil visor plate over the face (Tag 1 glows in the Ward guard).
		FWRToolMesh Helm;
		ClPlate(Helm, FVector(14.f, 0.f, 6.f) * K, FVector::UpVector, FVector(1.f, 0.f, 0.f), 34.f * K, 30.f * K, 3.f, ClPlateC);
		ClHexafoil(Helm, FVector(16.5f, 0.f, 6.f) * K, FVector(0.f, 1.f, 0.f), FVector::UpVector, 9.f * K, 0.9f, ClLilac);
		for (int32 k = 0; k < 5; ++k) ClPoint(Helm, FVector(-2.f, (k - 2) * 7.f, 18.f) * K, FVector(-0.2f, (k - 2) * 0.15f, 1.f), (24.f - FMath::Abs(k - 2) * 5.f) * K, 7.f * K, ClViolet, ClLilac);
		AddPart(BoneLike(TEXT("head")), FTransform::Identity, Helm, true, 0);
		// Pauldrons: crystal grown over the bronze, three overlapping plates a shoulder.
		for (const TCHAR* B : { TEXT("leftarm"), TEXT("rightarm") })
		{
			FWRToolMesh P;
			const float S = FString(B).StartsWith(TEXT("left")) ? -1.f : 1.f;
			for (int32 k = 0; k < 3; ++k) ClPlate(P, FVector(-4.f + k * 6.f, S * (4.f + k * 4.f), 10.f - k * 7.f) * K, FVector(1.f, 0.f, 0.f), FVector(0.f, S * 0.6f, 1.f).GetSafeNormal(), 34.f * K, 24.f * K, 3.f, ClPlateC);
			AddPart(BoneLike(B), FTransform::Identity, P, true, 0);
		}
		// The crystal longsword in the right hand (a 1.2 m blade of six-sided crystal, bronze guard and grip).
		FWRToolMesh Sword;
		Sword.Tube({ FVector(0.f, 0.f, -14.f) * K, FVector(0.f, 0.f, 10.f) * K }, 2.6f * K, 8, ClBronze, 0.45f);
		Sword.Tube({ FVector(0.f, -14.f, 10.f) * K, FVector(0.f, 14.f, 10.f) * K }, 2.4f * K, 8, ClBronze, 0.45f);
		ClPoint(Sword, FVector(0.f, 0.f, 12.f) * K, FVector::UpVector, 120.f * K, 9.f * K, ClPlateC, ClLilac);
		AddPart(BoneLike(TEXT("righthand")), FTransform::Identity, Sword, true, 0);
		// The kite shield on the left forearm: crystal over a bronze rim, a hexafoil in its face (Tag 1: lit in the guard).
		FWRToolMesh Shield;
		TArray<TArray<FVector>> Kite;
		for (const float T : { -2.f, 2.f })
		{
			Kite.Add({ FVector(T, 0.f, 46.f) * K, FVector(T, 30.f, 20.f) * K, FVector(T, 0.f, -54.f) * K, FVector(T, -30.f, 20.f) * K });
		}
		Shield.Loft(Kite, ClPlateC, 0.12f, true);
		Shield.Tube({ FVector(0.f, 0.f, 46.f) * K, FVector(0.f, 30.f, 20.f) * K, FVector(0.f, 0.f, -54.f) * K, FVector(0.f, -30.f, 20.f) * K, FVector(0.f, 0.f, 46.f) * K }, 1.6f * K, 5, ClBronze, 0.45f);
		AddPart(BoneLike(TEXT("leftforearm")), FTransform(FVector(-12.f, 0.f, 0.f) * K), Shield, true, 0);
		FWRToolMesh Foil;
		ClHexafoil(Foil, FVector(-14.5f, 0.f, 4.f) * K, FVector(0.f, 1.f, 0.f), FVector::UpVector, 14.f * K, 1.1f, ClLilac);
		if (USceneComponent* F = AddPart(BoneLike(TEXT("leftforearm")), FTransform::Identity, Foil, false, 1))
		{
			if (UMaterialInstanceDynamic* W = ClWard(F, ClLilac, 0.f)) { Cast<UProceduralMeshComponent>(F)->SetMaterial(0, W); Glows.Add(W); }
		}
		// The real knight's reflection in the Archive's still water: its own pose, mirrored under the floor, shown only
		// where you look at the floor (M_MirrorWater compares the scene's depth). The double has none.
		if (bMirrored)
		{
			Reflection = NewObject<UPoseableMeshComponent>(C, TEXT("TwinReflection"));
			Reflection->SetupAttachment(C->GetRootComponent());
			Reflection->SetUsingAbsoluteLocation(true);
			Reflection->SetUsingAbsoluteRotation(true);
			Reflection->SetUsingAbsoluteScale(true);
			Reflection->SetCastShadow(false);
			Reflection->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Reflection->RegisterComponent();
			Reflection->SetSkinnedAssetAndUpdate(Skel->GetSkeletalMeshAsset());
			if (UMaterialInterface* Mirror = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_MirrorWater.M_MirrorWater")))
			{
				for (int32 Slot = 0; Slot < Reflection->GetNumMaterials(); ++Slot)
				{
					UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Mirror, Reflection);
					if (UTexture* T = LoadObject<UTexture>(nullptr, TEXT("/Game/Creatures/Obsidian_warden/obsidian_warden_Image_0.obsidian_warden_Image_0"))) M->SetTextureParameterValue(TEXT("Tex"), T);
					M->SetVectorParameterValue(TEXT("Tint"), FLinearColor(0.42f, 0.36f, 0.6f));
					M->SetScalarParameterValue(TEXT("Strength"), WRTune(TEXT("bosses.mirrored_twins.reflection"), 0.55f));
					Reflection->SetMaterial(Slot, M);
					Glows.Add(M);
				}
			}
		}
	}
	else if (Id == TEXT("echo_warden"))
	{
		const float K = H / 250.f;
		// The great tuning fork for a head: a bronze stem, a U-bend, two tines 2.5 m high (it rings at one pitch).
		FWRToolMesh Fork;
		Fork.Tube({ FVector(0.f, 0.f, 0.f), FVector(0.f, 0.f, 50.f) * K }, 9.f * K, 10, ClBronze, 0.4f);
		TArray<FVector> Bend;
		for (int32 k = 0; k <= 10; ++k) { const float A = PI * k / 10.f; Bend.Add(FVector(0.f, FMath::Cos(A) * 34.f, 50.f - FMath::Sin(A) * 30.f) * K); }
		Fork.Tube(Bend, 8.f * K, 10, ClBronze, 0.4f);
		for (const float S : { -1.f, 1.f }) Fork.Tube({ FVector(0.f, S * 34.f, 50.f) * K, FVector(0.f, S * 34.f, 300.f) * K }, 7.f * K, 10, ClBronze, 0.35f);
		AddPart(BoneLike(TEXT("head")), FTransform::Identity, Fork, false, 2);
		// Two flared ear-horns on the shoulders that swivel to every sound (Tag 3: turned to Point).
		for (const float S : { -1.f, 1.f })
		{
			FWRToolMesh Horn;
			Horn.Lathe(FVector::ZeroVector, FVector(1.f, 0.f, 0.f), 130.f * K, { FVector(0.f, 10.f, 10.f), FVector(0.5f, 14.f, 14.f), FVector(0.85f, 30.f, 30.f), FVector(1.f, 54.f, 54.f) }, 16, ClBronze, 0.4f);
			Horn.Tube({ FVector(0.f, 0.f, -10.f) * K, FVector(0.f, 0.f, -40.f) * K }, 6.f * K, 8, ClBronzeDark, 0.5f);
			AddPart(BoneLike(TEXT("spine")), FTransform(FVector(R * 0.2f, S * R * 0.6f, H * 0.35f)), Horn, false, 3);
		}
		// Humming crystal plates on its back (they pulse with its hum), bronze plates over its chest.
		FWRToolMesh Back;
		for (int32 k = 0; k < 4; ++k) ClPlate(Back, FVector(-R * 0.6f + k * R * 0.35f, 0.f, H * 0.25f), FVector(0.3f, 0.f, 1.f).GetSafeNormal(), FVector(-1.f, 0.f, 0.3f).GetSafeNormal(), 90.f * K, 60.f * K, 5.f, ClPlateC);
		AddPart(BoneLike(TEXT("spine")), FTransform::Identity, Back, true, 4);
	}
	else if (Id == TEXT("geode_tyrant"))
	{
		// Six rows of split geodes along its back (Tag 10 + row; a broken row is hidden): basalt rind outside, violet
		// druse in the open face, the biggest 3 m over the shoulders; a clouded left eye.
		for (int32 Row = 0; Row < 6; ++Row)
		{
			const float S = Row < 3 ? -1.f : 1.f;
			const int32 Sec = Row % 3;
			const float X = R * (0.55f - Sec * 0.6f), Big = (Sec == 0 ? 150.f : 120.f - Sec * 15.f);
			FWRToolMesh Rind, Druse;
			for (int32 g = 0; g < 2; ++g)
			{
				const FVector C0(X - g * 70.f, S * (40.f + g * 15.f), H * 0.55f);
				const FVector Out = FVector(0.f, S, 0.9f).GetSafeNormal();
				Rind.Lathe(C0 - Out * 30.f, Out, Big * 0.5f, { FVector(0.f, 4.f, 4.f), FVector(0.3f, Big * 0.45f, Big * 0.36f), FVector(0.9f, Big * 0.62f, Big * 0.5f), FVector(1.f, Big * 0.6f, Big * 0.48f) }, 12, ClBasalt, 0.85f);
				for (int32 p = 0; p < 9; ++p)
				{
					FVector U, V;
					Out.FindBestAxisVectors(U, V);
					const float A = p * 0.7f, Dd = 10.f + (p % 3) * Big * 0.12f;
					ClPoint(Druse, C0 + Out * Big * 0.18f + (U * FMath::Cos(A) + V * FMath::Sin(A)) * Dd, (Out + (U * FMath::Cos(A) + V * FMath::Sin(A)) * 0.4f).GetSafeNormal(), Big * 0.3f, Big * 0.12f, ClViolet, ClLilac);
				}
			}
			AddPart(BoneLike(TEXT("spine2")), FTransform::Identity, Rind, false, 10 + Row);
			AddPart(BoneLike(TEXT("spine2")), FTransform::Identity, Druse, true, 10 + Row);
		}
		FWRToolMesh Eye;
		ClDisc(Eye, FVector(R * 0.2f, -R * 0.12f, 6.f), FVector(0.f, -1.f, 0.f), 9.f, 2.f, FLinearColor(0.78f, 0.78f, 0.8f), 0.3f, 14);
		AddPart(BoneLike(TEXT("head")), FTransform::Identity, Eye, false, 0);
	}
	else
	{
		WRCrystalBuildLyrathisBody(this, C, [this](FName B, const FTransform& L, FWRToolMesh& M, bool bG, int32 T) { return AddPart(B, L, M, bG, T); }, [this](const TCHAR* Hint) { return BoneLike(Hint); });
	}
	Lamp = NewObject<UPointLightComponent>(C);
	Lamp->SetupAttachment(C->GetRootComponent());
	Lamp->SetRelativeLocation(FVector(0.f, 0.f, H * 0.6f));
	Lamp->SetLightColor(ClLilac);
	Lamp->SetIntensity(Id == TEXT("echo_warden") ? 0.f : 600.f);
	Lamp->SetAttenuationRadius(FMath::Max(600.f, R * 6.f));
	Lamp->SetCastShadows(false);
	Lamp->RegisterComponent();
	ShownBroken = 255;
	ShownPose = 255;
}

void UWRCrystalBossBody::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	if (!C || C->GetNetMode() == NM_DedicatedServer) return;
	if (LookFor != C->CreatureId)
	{
		if (++Tries < 600 && C->GetCreatureSkel() && C->GetCreatureSkel()->GetSkeletalMeshAsset()) BuildLook();
		return;
	}
	Clock += DeltaTime;
	USkeletalMeshComponent* Skel = C->GetCreatureSkel();
	const float H = C->GetSimpleCollisionHalfHeight();
	// Parts that show state: broken rows hidden, the Ward guard's hexafoil lit, the horns turned to the noise, the
	// plates humming.
	if (ShownBroken != Broken || ShownPose != Pose)
	{
		ShownBroken = Broken;
		ShownPose = Pose;
		for (const FPart& P : Parts)
		{
			if (P.Tag >= 10 && P.Tag < 16 && P.Comp.IsValid()) P.Comp->SetVisibility(!(Broken & (1 << (P.Tag - 10))), true);
		}
	}
	for (const FPart& P : Parts)
	{
		USceneComponent* Comp = P.Comp.Get();
		if (!Comp) continue;
		if (P.Tag == 1)
		{
			if (UProceduralMeshComponent* M = Cast<UProceduralMeshComponent>(Comp)) if (UMaterialInstanceDynamic* W = Cast<UMaterialInstanceDynamic>(M->GetMaterial(0))) W->SetScalarParameterValue(TEXT("Glow"), Pose == 1 ? 2.2f + 0.6f * FMath::Sin(Clock * 9.f) : 0.f);
		}
		if (P.Tag == 3 && Point.Z > -1.e6f)
		{
			// The horns swivel toward what it heard (eased 0.3 s; they sweep slowly while it listens).
			const FVector To = (Point - Comp->GetComponentLocation()).GetSafeNormal2D();
			const float Want = Pose == 2 ? C->GetActorRotation().Yaw + 50.f * FMath::Sin(Clock * 1.8f) : To.Rotation().Yaw;
			const float Cur = Comp->GetComponentRotation().Yaw;
			Comp->SetWorldRotation(FRotator(0.f, FMath::FixedTurn(Cur, Want, DeltaTime * 240.f), 0.f));
		}
	}
	for (UMaterialInstanceDynamic* G : Glows)
	{
		if (!G) continue;
		if (C->CreatureId == TEXT("echo_warden")) G->SetScalarParameterValue(TEXT("Rest"), 0.3f + 0.25f * (0.5f + 0.5f * FMath::Sin(Clock * 6.f)) + (Pose == 1 ? 0.8f : 0.f));
	}
	// The reflection: the knight's pose, mirrored in the water's plane under its feet.
	if (Reflection && Skel)
	{
		Reflection->CopyPoseFromSkeletalComponent(Skel);
		const float FloorZ = C->GetActorLocation().Z - H;
		FTransform T = Skel->GetComponentTransform();
		FVector L = T.GetLocation();
		L.Z = FloorZ - (L.Z - FloorZ);
		T.SetLocation(L);
		FVector Sc = T.GetScale3D();
		Sc.Z = -Sc.Z;
		T.SetScale3D(Sc);
		Reflection->SetWorldTransform(T);
		for (int32 Slot = 0; Slot < Reflection->GetNumMaterials(); ++Slot)
		{
			if (UMaterialInstanceDynamic* M = Cast<UMaterialInstanceDynamic>(Reflection->GetMaterial(Slot))) M->SetScalarParameterValue(TEXT("FloorZ"), FloorZ + 1.f);
		}
		// Its steps ring the water (light doesn't; the double leaves none).
		RippleClock -= DeltaTime;
		if (RippleClock <= 0.f && C->GetVelocity().Size2D() > 60.f)
		{
			RippleClock = 0.42f;
			WRFx::SpawnAt(C, TEXT("crystal.water_ripple"), FVector(C->GetActorLocation().X, C->GetActorLocation().Y, FloorZ + 2.f));
		}
	}
	if (C->CreatureId == TEXT("lyrathis")) WRCrystalTickLyrathisBody(this, C, DeltaTime, Clock);
}
