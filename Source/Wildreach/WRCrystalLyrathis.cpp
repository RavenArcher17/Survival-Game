// Lyrathis, the Prism Sovereign, and the Prism Hall's machines (cooks/biome-crystal.md 2.14; build-biome-crystal.md Part 2
// P2.7). See WRCrystalBosses.h.
//
// The hall: eight prisms on bronze turntables 22 m round the dais (a disc on sixteen rollers in a carved race, a ring gear
// on its rim meshing a pinion on a vertical shaft up to a lever with a hexafoil hand-plate and a counterweight; a ratchet
// and pawl let it turn one way only). Strike a hand-plate and the disc comes round exactly 45 degrees over 0.8 s; the
// lever swings out and the counterweight brings it back; the pawl drops home with a clack.
//
// Her light: a beam from her brow gem into the prism nearest her (a pale guide thread through the windup, chosen at its
// start and never changed), then three rays fanned out of the prism's far face, sweeping across the active time. The same
// rays are traced on every machine from the replicated prism, its click and the beam's times (AWRPrismBeam); the server
// deals the contacts. A ray that meets her dome eats it (400 a second). A prism left facing its own light shaft throws
// the light up the shaft and its spire on the surface beams. Phase 2: copies stand where the line from her through a
// prism meets open floor 15 m past it; they repeat her moves half a second later. Phase 3 (artifacts): one beam turns round
// her at 30 degrees a second through every prism; her dome stays up until three prisms are turned to face her within 10 s.

#include "WRCrystalBosses.h"
#include "WRCrystalBossState.h"
#include "Wildreach.h"
#include "WRAltar.h"
#include "WRArcanePath.h"
#include "WRAttributeSet.h"
#include "WRBoss.h"
#include "WRCharacter.h"
#include "WRCombatLibrary.h"
#include "WRCreature.h"
#include "WRCrystalTypes.h"
#include "WRCrystalWorld.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRPlayerController.h"
#include "WRProjectile.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRToolMesh.h"
#include "WRTuning.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"

namespace WRLyrathisImpl
{
	const FLinearColor LyBronze = FLinearColor(FColor(140, 106, 58)) * 0.6f;
	const FLinearColor LyBronzeDark = FLinearColor(FColor(96, 72, 40)) * 0.55f;
	const FLinearColor LyIron = FLinearColor(0.03f, 0.028f, 0.027f);
	const FLinearColor LyStone = FLinearColor(FColor(42, 42, 48)) * 0.8f;
	const FLinearColor LyViolet = FLinearColor(FColor(107, 63, 160));
	const FLinearColor LyLilac = FLinearColor(FColor(216, 208, 255));
	const FLinearColor LyClearC = FLinearColor(FColor(236, 232, 250));
	const FLinearColor LySelenite = FLinearColor(FColor(242, 240, 232)) * 0.62f;

	float LyT(const TCHAR* Key, float Fallback) { return FWRTuning::Get().Num(FString::Printf(TEXT("bosses.lyrathis.%s"), Key), Fallback); }
	float LyNow(const UObject* Ctx)
	{
		const UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
		const AGameStateBase* GS = W ? W->GetGameState() : nullptr;
		return GS ? float(GS->GetServerWorldTimeSeconds()) : (W ? W->GetTimeSeconds() : 0.f);
	}
	float LyEase(float A) { A = FMath::Clamp(A, 0.f, 1.f); return A * A * (3.f - 2.f * A); }
	UMaterialInstanceDynamic* LyGlow(UObject* Outer, const FLinearColor& Col, float Rest)
	{
		UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_CrystalGlow.M_CrystalGlow"));
		if (!Mat) return nullptr;
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Mat, Outer);
		M->SetVectorParameterValue(TEXT("GlowColor"), Col);
		M->SetScalarParameterValue(TEXT("GlowScale"), WRTune(TEXT("crystalWorld.glow.scale"), 6.f));
		M->SetScalarParameterValue(TEXT("Rest"), Rest);
		return M;
	}
	UMaterialInstanceDynamic* LyWard(UObject* Outer, const FLinearColor& Col, float Glow)
	{
		UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_ArcaneWard.M_ArcaneWard"));
		if (!Mat) Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_VoidLight.M_VoidLight"));
		if (!Mat) return nullptr;
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Mat, Outer);
		M->SetVectorParameterValue(TEXT("Color"), Col);
		M->SetScalarParameterValue(TEXT("Glow"), Glow);
		return M;
	}
	UProceduralMeshComponent* LyMesh(AActor* Owner, USceneComponent* Under, const FWRToolMesh& M, TArray<TObjectPtr<USceneComponent>>& Track)
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
	USceneComponent* LyPivot(AActor* Owner, USceneComponent* Under, const FVector& Rel, TArray<TObjectPtr<USceneComponent>>& Track)
	{
		USceneComponent* S = NewObject<USceneComponent>(Owner);
		S->SetupAttachment(Under ? Under : Owner->GetRootComponent());
		S->SetRelativeLocation(Rel);
		S->RegisterComponent();
		Track.Add(S);
		return S;
	}
	void LyRing(FWRToolMesh& M, const FVector& C, const FVector& Axis, float R, float Thick, const FLinearColor& Col, float Rough, int32 Seg = 32)
	{
		FVector U, V;
		Axis.GetSafeNormal().FindBestAxisVectors(U, V);
		TArray<FVector> P;
		for (int32 k = 0; k < Seg; ++k) { const float A = 2.f * PI * k / Seg; P.Add(C + (U * FMath::Cos(A) + V * FMath::Sin(A)) * R); }
		M.Tube(P, Thick, 6, Col, Rough, true);
	}
	void LyDisc(FWRToolMesh& M, const FVector& C, const FVector& Axis, float R, float T, const FLinearColor& Col, float Rough, int32 Sides = 32)
	{
		M.Lathe(C - Axis.GetSafeNormal() * T * 0.5f, Axis.GetSafeNormal(), T, { FVector(0.f, R * 0.97f, R * 0.97f), FVector(0.12f, R, R), FVector(0.88f, R, R), FVector(1.f, R * 0.97f, R * 0.97f) }, Sides, Col, Rough);
	}
	/** A gear's teeth round a rim: N trapezoid teeth of depth Dp on radius R, thickness T, in the plane normal to Axis. */
	void LyTeeth(FWRToolMesh& M, const FVector& C, float R, float Dp, float T, int32 N, const FLinearColor& Col)
	{
		for (int32 k = 0; k < N; ++k)
		{
			const float A = 2.f * PI * k / N;
			const FVector D(FMath::Cos(A), FMath::Sin(A), 0.f);
			M.Tube({ C + D * (R - 1.f), C + D * (R + Dp) }, Dp * 0.38f, 4, Col, 0.45f);
			(void)T;
		}
	}
	void LyHexafoil(FWRToolMesh& M, const FVector& C, const FVector& Right, const FVector& Up, float R, float Thick, const FLinearColor& Col)
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
	/** A calcite prism: a six-sided column with rhomb-cut ends (each end three slanted faces meeting off-centre, the
	 *  rhombohedron's habit), clear violet-white, a phantom of an earlier crystal inside. Vertex alpha: how much glows. */
	void LyCalcite(FWRToolMesh& M, const FVector& Base, float R, float H)
	{
		const FVector2D Z = FVector2D::ZeroVector;
		FVector Lo[6], Hi[6];
		for (int32 k = 0; k < 6; ++k)
		{
			const float A = k * PI / 3.f + PI / 6.f;
			const FVector D(FMath::Cos(A) * R, FMath::Sin(A) * R, 0.f);
			// The rhomb cut: alternate corners sit higher / lower (three faces at each end).
			Lo[k] = Base + D + FVector(0.f, 0.f, (k % 2 ? 0.f : 22.f));
			Hi[k] = Base + D + FVector(0.f, 0.f, H - 70.f + (k % 2 ? 26.f : 0.f));
		}
		const FVector TopApex = Base + FVector(R * 0.08f, 0.f, H), BotApex = Base + FVector(-R * 0.08f, 0.f, -10.f);
		const FColor Side = FWRToolMesh::Pack(LyClearC, 0.4f), Cap = FWRToolMesh::Pack(LyLilac, 0.9f);
		for (int32 k = 0; k < 6; ++k)
		{
			const int32 n = (k + 1) % 6;
			M.Tri(Lo[k], Hi[n], Hi[k], Side, Z, Z, Z);
			M.Tri(Lo[k], Lo[n], Hi[n], Side, Z, Z, Z);
			M.Tri(Hi[k], Hi[n], TopApex, Cap, Z, Z, Z);
			M.Tri(Lo[n], Lo[k], BotApex, Side, Z, Z, Z);
		}
		// The phantom: a smaller ghost of the same crystal inside, a little brighter.
		for (int32 k = 0; k < 6; ++k)
		{
			const int32 n = (k + 1) % 6;
			const FVector a = Base + (Lo[k] - Base) * 0.45f + FVector(0.f, 0.f, H * 0.2f), b = Base + (Lo[n] - Base) * 0.45f + FVector(0.f, 0.f, H * 0.2f);
			const FVector Tip = Base + FVector(0.f, 0.f, H * 0.68f);
			M.Tri(a, b, Tip, FWRToolMesh::Pack(LyLilac, 0.7f), Z, Z, Z);
		}
	}
	/** One quartz point. */
	void LyPoint(FWRToolMesh& M, const FVector& Root, const FVector& Dir, float Len, float Thick, const FLinearColor& Base, const FLinearColor& Tip)
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
}
using namespace WRLyrathisImpl;

// ================================================================================================ the turntable and the lance

void WRCrystalBuildPrismProp(AWRCrystalBossProp* Prop, TArray<TObjectPtr<USceneComponent>>& Track, USceneComponent*& A, USceneComponent*& B, USceneComponent*& C, USceneComponent*& D, TArray<TObjectPtr<UMaterialInstanceDynamic>>& Glows)
{
	if (!Prop) return;
	const bool bServer = Prop->GetNetMode() == NM_DedicatedServer;
	if (Prop->Kind == TEXT("turntable"))
	{
		// The prism stands solid on its disc (it blocks a body, a beam and the Flare: cover).
		if (UBoxComponent* Solid = Prop->FindComponentByClass<UBoxComponent>())
		{
			Solid->SetBoxExtent(FVector(80.f, 80.f, Prop->Size * 0.5f));
			Solid->SetRelativeLocation(FVector(0.f, 0.f, 60.f + Prop->Size * 0.5f));
			Solid->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}
		if (bServer) return;
		// The race: a ring trough cut in the floor's stone, its kerb dressed; sixteen bronze rollers sit in it under the
		// disc's rim (each on its own axle pin), so the disc rides on them.
		FWRToolMesh Race;
		LyRing(Race, FVector(0.f, 0.f, 4.f), FVector::UpVector, 214.f, 9.f, LyStone, 0.85f, 48);
		LyRing(Race, FVector(0.f, 0.f, 4.f), FVector::UpVector, 176.f, 9.f, LyStone, 0.85f, 48);
		for (int32 r = 0; r < 16; ++r)
		{
			const float Ang = r * PI / 8.f;
			const FVector P(FMath::Cos(Ang) * 195.f, FMath::Sin(Ang) * 195.f, 10.f);
			const FVector Tan(-FMath::Sin(Ang), FMath::Cos(Ang), 0.f);
			Race.Lathe(P - FVector(FMath::Cos(Ang), FMath::Sin(Ang), 0.f) * 9.f, FVector(FMath::Cos(Ang), FMath::Sin(Ang), 0.f), 18.f, { FVector(0.f, 9.f, 9.f), FVector(1.f, 9.f, 9.f) }, 10, LyBronze, 0.45f);
			(void)Tan;
		}
		// The fixed works at the rim (outboard, +Y of the actor): the pinion's bearing block on a basalt pedestal, the
		// vertical shaft's upper bearing on a bronze bracket, the ratchet's pawl post.
		Race.Lathe(FVector(0.f, 232.f, 0.f), FVector::UpVector, 40.f, { FVector(0.f, 34.f, 34.f), FVector(1.f, 28.f, 28.f) }, 8, LyStone * 1.2f, 0.85f);
		Race.Tube({ FVector(0.f, 262.f, 40.f), FVector(0.f, 262.f, 120.f), FVector(0.f, 238.f, 128.f) }, 5.f, 6, LyBronzeDark, 0.5f);
		LyRing(Race, FVector(0.f, 232.f, 128.f), FVector::UpVector, 9.f, 3.5f, LyBronze, 0.45f, 12);
		Race.Tube({ FVector(28.f, 222.f, 40.f), FVector(28.f, 222.f, 78.f) }, 3.f, 6, LyBronzeDark, 0.5f);
		LyMesh(Prop, nullptr, Race, Track);
		// A: the disc and its prism (turns), with its ring gear, the engraved facing arrow and the centre boss.
		A = LyPivot(Prop, nullptr, FVector(0.f, 0.f, 22.f), Track);
		FWRToolMesh Disc;
		LyDisc(Disc, FVector::ZeroVector, FVector::UpVector, 200.f, 14.f, LyBronze, 0.42f, 48);
		LyTeeth(Disc, FVector(0.f, 0.f, 0.f), 200.f, 9.f, 14.f, 48, LyBronzeDark);
		LyRing(Disc, FVector(0.f, 0.f, 8.f), FVector::UpVector, 150.f, 2.f, LyBronzeDark, 0.5f, 40);
		// The arrow cut in the disc's face, pointing the way the prism throws its light (+X).
		Disc.Tube({ FVector(30.f, 0.f, 8.f), FVector(185.f, 0.f, 8.f) }, 3.f, 4, LyBronzeDark * 0.6f, 0.6f);
		Disc.Tube({ FVector(160.f, -20.f, 8.f), FVector(188.f, 0.f, 8.f), FVector(160.f, 20.f, 8.f) }, 3.f, 4, LyBronzeDark * 0.6f, 0.6f);
		LyHexafoil(Disc, FVector(0.f, 0.f, 8.5f), FVector(1.f, 0.f, 0.f), FVector(0.f, 1.f, 0.f), 46.f, 1.4f, LyBronzeDark * 0.7f);
		Disc.Lathe(FVector(0.f, 0.f, 6.f), FVector::UpVector, 34.f, { FVector(0.f, 92.f, 92.f), FVector(1.f, 84.f, 84.f) }, 6, LyBronze, 0.42f);
		LyMesh(Prop, A, Disc, Track);
		FWRToolMesh Prism;
		LyCalcite(Prism, FVector(0.f, 0.f, 40.f), 78.f, Prop->Size);
		if (UProceduralMeshComponent* P = LyMesh(Prop, A, Prism, Track)) if (UMaterialInstanceDynamic* M = LyGlow(P, FLinearColor(0.82f, 0.74f, 1.f), 0.22f)) { P->SetMaterial(0, M); Glows.Add(M); if (UWRCrystalWorld* W = UWRCrystalWorld::Get(Prop)) W->RegisterGlow(M, Prop->GetActorLocation()); }
		// C: the pinion (meshing the ring gear at the rim) on its vertical shaft, with the ratchet wheel on the shaft.
		C = LyPivot(Prop, nullptr, FVector(0.f, 232.f, 22.f), Track);
		FWRToolMesh Pin;
		LyDisc(Pin, FVector::ZeroVector, FVector::UpVector, 22.f, 14.f, LyBronze, 0.42f, 16);
		LyTeeth(Pin, FVector::ZeroVector, 22.f, 9.f, 14.f, 12, LyBronze);
		Pin.Tube({ FVector(0.f, 0.f, 0.f), FVector(0.f, 0.f, 128.f) }, 5.f, 8, LyIron, 0.6f);
		LyDisc(Pin, FVector(0.f, 0.f, 58.f), FVector::UpVector, 16.f, 6.f, LyBronzeDark, 0.5f, 12);
		for (int32 t = 0; t < 8; ++t) { const float Ang = t * PI / 4.f; Pin.Tube({ FVector(FMath::Cos(Ang) * 15.f, FMath::Sin(Ang) * 15.f, 58.f), FVector(FMath::Cos(Ang + 0.3f) * 21.f, FMath::Sin(Ang + 0.3f) * 21.f, 58.f) }, 2.f, 4, LyBronzeDark, 0.5f); }
		LyMesh(Prop, C, Pin, Track);
		// B: the lever on the shaft's top (the hexafoil hand-plate out beyond the rim, the counterweight on its far arm).
		B = LyPivot(Prop, nullptr, FVector(0.f, 232.f, 150.f), Track);
		FWRToolMesh Lever;
		Lever.Tube({ FVector(0.f, -60.f, 0.f), FVector(0.f, 28.f, 0.f) }, 5.f, 8, LyBronze, 0.42f);
		LyDisc(Lever, FVector(0.f, 40.f, 0.f), FVector(0.f, 1.f, 0.f), 22.f, 4.f, LyBronze, 0.4f, 6);
		LyHexafoil(Lever, FVector(0.f, 43.f, 0.f), FVector(1.f, 0.f, 0.f), FVector::UpVector, 9.f, 1.f, LyLilac * 0.6f);
		Lever.Lathe(FVector(0.f, -60.f, -14.f), FVector(0.f, -1.f, 0.f), 28.f, { FVector(0.f, 13.f, 13.f), FVector(1.f, 13.f, 13.f) }, 8, LyIron, 0.6f);
		LyMesh(Prop, B, Lever, Track);
		// D: the pawl on its post, dropping into the ratchet's teeth at each step's end.
		D = LyPivot(Prop, nullptr, FVector(28.f, 222.f, 64.f), Track);
		FWRToolMesh Pawl;
		Pawl.Tube({ FVector::ZeroVector, FVector(-14.f, 12.f, -6.f) }, 2.4f, 5, LyBronzeDark, 0.5f);
		LyMesh(Prop, D, Pawl, Track);
		return;
	}
	if (Prop->Kind == TEXT("lance"))
	{
		// A Glint lance standing where it struck: a 2 m prism of her crystal driven into the floor at a slant (a small
		// prism: her beam splits through it, a copy can stand on its line).
		if (UBoxComponent* Solid = Prop->FindComponentByClass<UBoxComponent>())
		{
			Solid->SetBoxExtent(FVector(30.f, 30.f, 90.f));
			Solid->SetRelativeLocation(FVector(0.f, 0.f, 90.f));
			Solid->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}
		if (bServer) return;
		FWRToolMesh L;
		LyPoint(L, FVector(-40.f, 0.f, -30.f), FVector(0.35f, 0.f, 1.f), 260.f, 34.f, LyViolet, LyLilac);
		for (int32 k = 0; k < 6; ++k) { const float Ang = k * PI / 3.f; LyPoint(L, FVector(FMath::Cos(Ang) * 40.f, FMath::Sin(Ang) * 40.f, 0.f), FVector(FMath::Cos(Ang) * 0.5f, FMath::Sin(Ang) * 0.5f, 0.3f), 30.f, 10.f, LyViolet, LyLilac); }
		if (UProceduralMeshComponent* P = LyMesh(Prop, nullptr, L, Track)) if (UMaterialInstanceDynamic* M = LyGlow(P, FLinearColor(0.8f, 0.7f, 1.f), 0.5f)) { P->SetMaterial(0, M); Glows.Add(M); }
	}
}

void WRCrystalTickPrismProp(AWRCrystalBossProp* Prop, float Now, float Dt, USceneComponent* A, USceneComponent* B, USceneComponent* C, USceneComponent* D, const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Glows)
{
	if (!Prop || Prop->Kind != TEXT("turntable") || !A) return;
	// The disc eases its 45 degrees; the ring gear's 48 teeth against the pinion's 12 turn the pinion 180 degrees each
	// step the other way; the lever swings out 40 degrees and the counterweight returns it; the pawl lifts and drops home.
	const float Turn = LyT(TEXT("turnSeconds"), 0.8f);
	const float S = FMath::Clamp((Now - Prop->TurnAt) / FMath::Max(0.05f, Turn), 0.f, 1.f);
	A->SetWorldRotation(FRotator(0.f, Prop->FacingNow(Now), 0.f));
	const float Step = Prop->Clicks > 0 ? float(Prop->Clicks) - 1.f + LyEase(S) : 0.f;
	if (C) C->SetRelativeRotation(FRotator(0.f, -180.f * Step, 0.f));
	if (B) B->SetRelativeRotation(FRotator(0.f, 40.f * FMath::Sin(PI * FMath::Clamp(S * 1.25f, 0.f, 1.f)) * (S < 1.f ? 1.f : 0.f), 0.f));
	if (D) D->SetRelativeRotation(FRotator(S < 0.9f && S > 0.f ? 18.f : 0.f, 0.f, 0.f));
	(void)Dt; (void)Glows;
}

// ================================================================================================ AWRPrismBeam

AWRPrismBeam::AWRPrismBeam()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = true;
	SetNetUpdateFrequency(10.f);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRPrismBeam::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRPrismBeam, Source);
	DOREPLIFETIME(AWRPrismBeam, Prism);
	DOREPLIFETIME(AWRPrismBeam, Kind);
	DOREPLIFETIME(AWRPrismBeam, ThreadAt);
	DOREPLIFETIME(AWRPrismBeam, FireAt);
	DOREPLIFETIME(AWRPrismBeam, EndAt);
	DOREPLIFETIME(AWRPrismBeam, Sweep);
	DOREPLIFETIME(AWRPrismBeam, Bias);
}

float AWRPrismBeam::ServerNow() const
{
	return LyNow(this);
}

void AWRPrismBeam::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer) return;
	Mat = LyWard(this, FLinearColor(0.75f, 0.6f, 1.f), 2.f);
	// A pool of beam segments: a unit hex rod along +X (scaled to each ray's length).
	for (int32 i = 0; i < 40; ++i)
	{
		FWRToolMesh M;
		M.Lathe(FVector::ZeroVector, FVector(1.f, 0.f, 0.f), 100.f, { FVector(0.f, 1.f, 1.f), FVector(1.f, 1.f, 1.f) }, 6, FLinearColor(0.85f, 0.75f, 1.f), 1.f);
		UProceduralMeshComponent* P = NewObject<UProceduralMeshComponent>(this);
		P->SetupAttachment(RootComponent);
		P->SetUsingAbsoluteLocation(true);
		P->SetUsingAbsoluteRotation(true);
		P->SetUsingAbsoluteScale(true);
		P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		P->SetCastShadow(false);
		P->RegisterComponent();
		M.ApplyTo(P);
		if (Mat) P->SetMaterial(0, Mat);
		P->SetVisibility(false);
		Shafts.Add(P);
	}
	Light = NewObject<UPointLightComponent>(this);
	Light->SetupAttachment(RootComponent);
	Light->SetUsingAbsoluteLocation(true);
	Light->SetLightColor(LyLilac);
	Light->SetIntensity(0.f);
	Light->SetAttenuationRadius(2500.f);
	Light->SetCastShadows(false);
	Light->RegisterComponent();
}

namespace WRLyrathisImpl
{
	/** Her brow gem (the beam's source): high on her face, a little forward. */
	FVector LyBrow(const AActor* Source)
	{
		return Source->GetActorLocation() + Source->GetActorForwardVector() * Source->GetSimpleCollisionRadius() * 0.35f + FVector(0.f, 0.f, Source->GetSimpleCollisionHalfHeight() * 0.86f);
	}
	/** A ray from A along Dir up to Range, stopped by the hall (walls, pillars, the dais, prisms) and by her dome. */
	FVector LyTrace(const UWorld* World, const FVector& A, const FVector& Dir, float Range, const AActor* IgnoreA, const AActor* IgnoreB, const FVector& Dome, float DomeR, bool* bOnDome = nullptr)
	{
		FCollisionObjectQueryParams Obj;
		Obj.AddObjectTypesToQuery(ECC_WorldStatic);
		Obj.AddObjectTypesToQuery(ECC_WorldDynamic);
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRPrismRay), false, IgnoreA);
		if (IgnoreB) Q.AddIgnoredActor(IgnoreB);
		FHitResult Hit;
		FVector End = A + Dir * Range;
		float Len = Range;
		if (World->LineTraceSingleByObjectType(Hit, A, End, Obj, Q)) { End = Hit.ImpactPoint; Len = Hit.Distance; }
		if (bOnDome) *bOnDome = false;
		if (DomeR > 0.f)
		{
			// Ray / sphere (the dome's centre at her feet; only its upper half stands).
			const FVector O = A - Dome;
			const float b = float(FVector::DotProduct(O, Dir)), c = float(O.SizeSquared()) - DomeR * DomeR;
			const float Disc = b * b - c;
			if (c > 0.f && Disc > 0.f)
			{
				const float T = -b - FMath::Sqrt(Disc);
				if (T > 0.f && T < Len && (A + Dir * T).Z >= Dome.Z - 10.f) { End = A + Dir * T; if (bOnDome) *bOnDome = true; }
			}
		}
		return End;
	}
}

void AWRPrismBeam::Rays(float Now, TArray<TPair<FVector, FVector>>& Out, TArray<int32>* OutStruckProp) const
{
	Out.Reset();
	const AActor* Src = Source;
	if (!Src || Now > EndAt) return;
	const UWorld* World = GetWorld();
	const UWRCrystalBossBody* Body = UWRCrystalBossBody::Get(Src);
	const bool bDome = Body && Body->Point.Z > -1.e6f;
	const FVector DomeAt = bDome ? Body->Point : FVector::ZeroVector;
	const float DomeR = bDome ? LyT(TEXT("domeRadius"), 600.f) : 0.f;
	const FVector Brow = LyBrow(Src);
	const float Range = LyT(TEXT("beamRange"), 4000.f);
	const float Exit = LyT(TEXT("beamExitHeight"), 140.f);
	// The fan out of one prism's far face at Facing (degrees): three rays, the outer pair 30 degrees off.
	const auto Fan = [&](const AWRCrystalBossProp* P, float Facing, float Along, int32 Index)
	{
		const FVector From = P->GetActorLocation() + FVector(0.f, 0.f, Exit);
		for (int32 k = -1; k <= 1; ++k)
		{
			const float Yaw = Facing + k * LyT(TEXT("beamFan"), 30.f) + Along;
			const FVector Dir = FRotator(0.f, Yaw, 0.f).Vector();
			const FVector End = LyTrace(World, From + Dir * 90.f, Dir, Range, P, Src, DomeAt, DomeR);
			Out.Add({ From, End });
			if (OutStruckProp) OutStruckProp->Add(Index);
		}
		// A prism left facing its own light shaft throws part of the light up it: the spire on the surface beams.
		if (P->Kind == TEXT("turntable") && (((P->Clicks % 8) + 8) % 8) == 0 && Now - P->TurnAt > LyT(TEXT("turnSeconds"), 0.8f))
		{
			const FWRCrystalData& D = FWRCrystalData::Get();
			const FName ShaftId(*FString::Printf(TEXT("light%d"), P->Index + 1));
			for (const FWRCrystalData::FShaft& S : D.Shafts)
			{
				if (S.Id != ShaftId) continue;
				const FVector Foot(S.At.X, S.At.Y, S.Z0);
				Out.Add({ P->PrismCentre(), Foot });
				Out.Add({ Foot, FVector(S.At.X, S.At.Y, S.Z1 + LyT(TEXT("spireBeam"), 9000.f)) });
				if (OutStruckProp) { OutStruckProp->Add(-1); OutStruckProp->Add(-1); }
			}
		}
	};
	if (Kind == 0)
	{
		const AWRCrystalBossProp* P = Prism;
		if (!P) return;
		if (Now < FireAt)
		{
			// The guide thread through the windup: brow gem to the chosen prism.
			if (Now >= ThreadAt) { Out.Add({ Brow, P->PrismCentre() }); if (OutStruckProp) OutStruckProp->Add(-2); }
			return;
		}
		const float A = FMath::Clamp((Now - FireAt) / FMath::Max(0.1f, EndAt - FireAt), 0.f, 1.f);
		Out.Add({ Brow, P->PrismCentre() });
		if (OutStruckProp) OutStruckProp->Add(-3);   // the light into the prism, overhead (it burns nobody)
		Fan(P, P->FacingNow(Now) + Bias, Sweep * (A - 0.5f), P->Index);
		return;
	}
	// Phase 3: one beam turning round her at 30 degrees a second; each prism it passes throws its fan.
	if (Now < FireAt) return;
	const float Yaw = Bias + LyT(TEXT("p3TurnRate"), 30.f) * (Now - FireAt);
	const FVector Dir = FRotator(0.f, Yaw, 0.f).Vector();
	const FVector From = Src->GetActorLocation() - FVector(0.f, 0.f, Src->GetSimpleCollisionHalfHeight() - Exit - 40.f);
	Out.Add({ Brow, LyTrace(World, From, Dir, Range, Src, nullptr, FVector::ZeroVector, 0.f) });
	if (OutStruckProp) OutStruckProp->Add(-1);
	// All eight shafts blaze in this phase: every prism throws light up its shaft to its spire on the surface.
	for (const FWRCrystalData::FShaft& Sh : FWRCrystalData::Get().Shafts)
	{
		if (Sh.Kind != TEXT("light")) continue;
		Out.Add({ FVector(Sh.At.X, Sh.At.Y, Sh.Z0), FVector(Sh.At.X, Sh.At.Y, Sh.Z1 + LyT(TEXT("spireBeam"), 9000.f)) });
		if (OutStruckProp) OutStruckProp->Add(-1);
	}
	for (const TWeakObjectPtr<AWRCrystalBossProp>& W : AWRCrystalBossProp::All())
	{
		const AWRCrystalBossProp* P = W.Get();
		if (!P || (P->Kind != TEXT("turntable") && P->Kind != TEXT("lance")) || FVector::Dist2D(P->GetActorLocation(), Src->GetActorLocation()) > Range) continue;
		const float To = (P->GetActorLocation() - Src->GetActorLocation()).Rotation().Yaw;
		if (FMath::Abs(FMath::FindDeltaAngleDegrees(To, Yaw)) > 8.f) continue;
		Fan(P, P->FacingNow(Now), 0.f, P->Index);
	}
}

void AWRPrismBeam::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Now = ServerNow();
	if (HasAuthority())
	{
		if (!Source || Now > EndAt + 0.2f) { Destroy(); return; }
		if (Now >= FireAt) Strike(Now, DeltaSeconds);
	}
	if (GetNetMode() != NM_DedicatedServer) Draw(Now);
}

void AWRPrismBeam::Draw(float Now)
{
	TArray<TPair<FVector, FVector>> R;
	TArray<int32> Kinds;
	Rays(Now, R, &Kinds);
	const bool bFiring = Now >= FireAt;
	for (int32 i = 0; i < Shafts.Num(); ++i)
	{
		UProceduralMeshComponent* P = Shafts[i];
		if (!P) continue;
		if (!R.IsValidIndex(i)) { P->SetVisibility(false); continue; }
		const FVector A = R[i].Key, B = R[i].Value;
		const float L = float(FVector::Dist(A, B));
		const bool bThread = Kinds.IsValidIndex(i) && Kinds[i] == -2;
		const float Thick = bThread ? 1.5f : (Kinds.IsValidIndex(i) && Kinds[i] < 0 ? 9.f : 7.f) * (0.85f + 0.15f * FMath::Sin(Now * 40.f + i));
		P->SetWorldLocationAndRotation(A, (B - A).Rotation());
		P->SetWorldScale3D(FVector(L / 100.f, Thick, Thick));
		P->SetVisibility(L > 1.f);
	}
	if (Mat) Mat->SetScalarParameterValue(TEXT("Glow"), bFiring ? 3.f : 0.9f);
	if (Light)
	{
		Light->SetIntensity(bFiring && R.Num() ? 12000.f : 0.f);
		if (R.Num()) Light->SetWorldLocation(R.Last().Key);
	}
}

void AWRPrismBeam::Strike(float Now, float Dt)
{
	// Server: a contact on everyone a ray crosses (35, mark 30, every 0.5 s), blockable; a ray on her dome eats it.
	TArray<TPair<FVector, FVector>> R;
	TArray<int32> Kinds;
	Rays(Now, R, &Kinds);
	AWRBoss* Boss = Cast<AWRBoss>(Source);
	if (!Boss) return;
	const double T = GetWorld()->GetTimeSeconds();
	const float Every = LyT(TEXT("beamEvery"), 0.5f);
	const UWRCrystalBossBody* Body = UWRCrystalBossBody::Get(Boss);
	const bool bDome = Body && Body->Point.Z > -1.e6f;
	for (int32 i = 0; i < R.Num(); ++i)
	{
		if (Kinds.IsValidIndex(i) && (Kinds[i] <= -2 || (Kinds[i] < 0 && FMath::Abs(R[i].Value.Z - R[i].Key.Z) > 1000.f))) continue;   // the thread, the light into the prism, the light up a shaft
		const FVector A = R[i].Key, B = R[i].Value;
		if (bDome && FVector::Dist(B, Body->Point) < LyT(TEXT("domeRadius"), 600.f) + 30.f)
		{
			FWRCrystalBoss::DomeBurn(Boss, LyT(TEXT("domeBeamDps"), 400.f) * Dt / FMath::Max(1, R.Num() / 3));
		}
		for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
		{
			AWRCharacterBase* C = *It;
			if (!C->IsAlive() || !Boss->IsHostileTo(C) || FWRCrystalBoss::IsDouble(C)) continue;
			const FVector P = FMath::ClosestPointOnSegment(C->GetActorLocation(), A, B);
			const FVector L = C->GetActorLocation() - P;
			if (L.Size2D() > C->GetSimpleCollisionRadius() + 25.f || FMath::Abs(L.Z) > C->GetSimpleCollisionHalfHeight() + 25.f) continue;
			double& Last = HitAt.FindOrAdd(C);
			if (T - Last < Every) continue;
			Last = T;
			FWRHit Hit;
			Hit.Instigator = Boss;
			Hit.Damage = Damage * WRTune(TEXT("bosses.damageScale"), 1.f) * (Boss->Phase >= 3 ? LyT(TEXT("phase3DamageScale"), 1.4f) : 1.f);
			Hit.PoiseDamage = 40.f;
			Hit.Impact = 2;
			Hit.Element = WRTags::Element_Arcane;
			Hit.Origin = A;
			Hit.ImpactPoint = P;
			Hit.Buildups.Add(TEXT("mark"), Mark);
			UWRCombatLibrary::ApplyHit(C, Hit);
			WRFx::Broadcast(this, TEXT("crystal.beam_contact"), P, FRotator::ZeroRotator, 1.f, TEXT("crystal.boss.beam_contact"));
		}
	}
}

// ================================================================================================ her body

void WRCrystalBuildLyrathisBody(UWRCrystalBossBody* Body, AWRCreature* C, TFunctionRef<USceneComponent*(FName, const FTransform&, FWRToolMesh&, bool, int32)> Add, TFunctionRef<FName(const TCHAR*)> Bone)
{
	if (!C) return;
	const float K = C->GetSimpleCollisionHalfHeight() / 450.f;
	// The brow gem (Tag 40: it brightens through a beam's windup).
	FWRToolMesh Gem;
	LyPoint(Gem, FVector(14.f, 0.f, 10.f) * K, FVector(1.f, 0.f, 0.15f), 22.f * K, 18.f * K, LyViolet, LyLilac);
	LyPoint(Gem, FVector(14.f, 0.f, 10.f) * K, FVector(-1.f, 0.f, -0.15f), 6.f * K, 18.f * K, LyViolet, LyLilac);
	Add(Bone(TEXT("head")), FTransform::Identity, Gem, true, 40);
	// The crown of eight prism spires round her head, the front ones tallest.
	FWRToolMesh Crown;
	for (int32 s = 0; s < 8; ++s)
	{
		const float A = s * PI / 4.f;
		const FVector At(FMath::Cos(A) * 16.f * K, FMath::Sin(A) * 16.f * K, 26.f * K);
		const float L = (34.f + 14.f * FMath::Max(0.f, FMath::Cos(A))) * K;
		LyPoint(Crown, At, FVector(FMath::Cos(A) * 0.25f, FMath::Sin(A) * 0.25f, 1.f), L, 8.f * K, LyViolet, LyLilac);
	}
	Add(Bone(TEXT("head")), FTransform::Identity, Crown, true, 0);
	// Her hair: about forty selenite needles from the crown of her head falling to the shoulders (they shiver and chime).
	FWRToolMesh Hair;
	FRandomStream Rng(77);
	for (int32 n = 0; n < 40; ++n)
	{
		const float A = PI * 0.35f + Rng.FRandRange(0.f, PI * 1.3f);
		const FVector Root(FMath::Cos(A) * 14.f * K - 6.f * K, FMath::Sin(A) * 15.f * K, 18.f * K);
		const FVector Dir = FVector(-0.5f + FMath::Cos(A) * 0.2f, FMath::Sin(A) * 0.35f, -1.f).GetSafeNormal();
		LyPoint(Hair, Root, Dir, Rng.FRandRange(60.f, 110.f) * K, Rng.FRandRange(2.5f, 4.f) * K, LySelenite, LyClearC);
	}
	Add(Bone(TEXT("head")), FTransform::Identity, Hair, true, 41);
	// The heart crystal turning inside her chest (Tag 42: it turns; it flares in the Pulse and the Flare).
	FWRToolMesh Heart;
	LyPoint(Heart, FVector(0.f, 0.f, 0.f), FVector(0.f, 0.f, 1.f), 26.f * K, 22.f * K, LyViolet, LyLilac);
	LyPoint(Heart, FVector(0.f, 0.f, 0.f), FVector(0.f, 0.f, -1.f), 26.f * K, 22.f * K, LyViolet, LyLilac);
	Add(Bone(TEXT("spine2")), FTransform(FVector(10.f, 0.f, 0.f) * K), Heart, true, 42);
	// The mantle: overlapping selenite plates over her shoulders (where the rig's wings were folded away).
	for (const float S : { -1.f, 1.f })
	{
		FWRToolMesh Mantle;
		for (int32 p = 0; p < 5; ++p)
		{
			const FVector At(-6.f * K - p * 5.f * K, S * (14.f + p * 4.f) * K, 8.f * K - p * 9.f * K);
			LyPoint(Mantle, At, FVector(-0.3f, S * 0.5f, -1.f), (48.f + p * 6.f) * K, 16.f * K, LySelenite, LyClearC);
		}
		Add(Bone(S < 0.f ? TEXT("leftshoulder") : TEXT("rightshoulder")), FTransform::Identity, Mantle, true, 0);
	}
	// The wrist blades (Tags 43 left / 44 right: gone when that part breaks).
	for (const float S : { -1.f, 1.f })
	{
		FWRToolMesh Blade;
		LyPoint(Blade, FVector(10.f, 0.f, 0.f) * K, FVector(1.f, 0.f, -0.05f), 120.f * K, 12.f * K, LyViolet, LyLilac);
		LyPoint(Blade, FVector(6.f, S * 3.f, 4.f) * K, FVector(1.f, S * 0.2f, 0.25f), 50.f * K, 7.f * K, LyViolet, LyLilac);
		Add(Bone(S < 0.f ? TEXT("lefthand") : TEXT("righthand")), FTransform::Identity, Blade, true, S < 0.f ? 43 : 44);
	}
	// The Prism ward (Tag 45: a faceted dome over her, the eight-armed stave on its face; shown while it stands).
	FWRToolMesh Dome;
	const float R = WRTune(TEXT("bosses.lyrathis.domeRadius"), 600.f);
	const FVector2D Z = FVector2D::ZeroVector;
	auto At = [R](int32 i, int32 j) { const float Th = (PI * 0.5f) * i / 6.f, Ph = 2.f * PI * (j + (i % 2) * 0.5f) / 16.f; return FVector(FMath::Cos(Ph) * FMath::Cos(Th) * R, FMath::Sin(Ph) * FMath::Cos(Th) * R, FMath::Sin(Th) * R); };
	const FColor Face = FWRToolMesh::Pack(LyLilac, 0.15f);
	for (int32 i = 0; i < 6; ++i) for (int32 j = 0; j < 16; ++j) { Dome.Tri(At(i, j), At(i + 1, j), At(i, j + 1), Face, Z, Z, Z); if (i < 5) Dome.Tri(At(i, j + 1), At(i + 1, j), At(i + 1, j + 1), Face, Z, Z, Z); }
	for (int32 k = 0; k < 8; ++k)
	{
		const float A = k * PI / 4.f;
		TArray<FVector> Arm;
		for (int32 i = 0; i <= 6; ++i) { const float Th = (PI * 0.5f) * i / 6.f; Arm.Add(FVector(FMath::Cos(A) * FMath::Cos(Th) * R * 1.005f, FMath::Sin(A) * FMath::Cos(Th) * R * 1.005f, FMath::Sin(Th) * R * 1.005f)); }
		Dome.Tube(Arm, 3.f, 4, LyLilac, 1.f);
	}
	if (USceneComponent* D = Add(NAME_None, FTransform(FVector(0.f, 0.f, -C->GetSimpleCollisionHalfHeight())), Dome, false, 45))
	{
		if (UProceduralMeshComponent* PM = Cast<UProceduralMeshComponent>(D)) if (UMaterialInstanceDynamic* W = LyWard(PM, LyLilac, 1.2f)) PM->SetMaterial(0, W);
		D->SetVisibility(false);
	}
	// A copy is all light: her shape in hard lilac.
	if (C->CreatureId == TEXT("lyrathis_copy"))
	{
		if (USkeletalMeshComponent* Skel = C->GetCreatureSkel())
		{
			for (int32 Slot = 0; Slot < Skel->GetNumMaterials(); ++Slot) if (UMaterialInstanceDynamic* W = LyWard(Skel, LyLilac, 0.7f)) Skel->SetMaterial(Slot, W);
		}
	}
	(void)Body;
}

void WRCrystalTickLyrathisBody(UWRCrystalBossBody* Body, AWRCreature* C, float Dt, float Clock)
{
	if (!Body || !C) return;
	const bool bDome = Body->Point.Z > -1.e6f;
	// The dome: standing over her while it holds, its glow the share of its HP left (the split beam dims it).
	Body->ForTagged(45, [&](USceneComponent* P)
	{
		P->SetVisibility(bDome, true);
		if (bDome) P->SetWorldLocationAndRotation(Body->Point, FRotator(0.f, Clock * 4.f, 0.f));
		if (UProceduralMeshComponent* M = Cast<UProceduralMeshComponent>(P)) if (UMaterialInstanceDynamic* W = Cast<UMaterialInstanceDynamic>(M->GetMaterial(0))) W->SetScalarParameterValue(TEXT("Glow"), 0.5f + 1.4f * Body->Level + 0.15f * FMath::Sin(Clock * 3.f));
	});
	// The brow gem brightens through a beam's windup; the heart turns, faster and brighter in the Flare (Pose 2).
	Body->ForTagged(40, [&](USceneComponent* P)
	{
		if (UProceduralMeshComponent* M = Cast<UProceduralMeshComponent>(P)) if (UMaterialInstanceDynamic* G = Cast<UMaterialInstanceDynamic>(M->GetMaterial(0))) G->SetScalarParameterValue(TEXT("Rest"), 0.4f + Body->Gem * 4.f);
	});
	Body->ForTagged(42, [&](USceneComponent* P)
	{
		P->AddRelativeRotation(FRotator(0.f, Dt * (Body->Pose == 2 ? 240.f : 24.f), 0.f));
		if (UProceduralMeshComponent* M = Cast<UProceduralMeshComponent>(P)) if (UMaterialInstanceDynamic* G = Cast<UMaterialInstanceDynamic>(M->GetMaterial(0))) G->SetScalarParameterValue(TEXT("Rest"), Body->Pose == 2 ? 3.f : 0.6f + 0.3f * FMath::Sin(Clock * 2.f));
	});
	// Her selenite hair shivers when she moves fast (and chimes: the sound row).
	const float Speed = C->GetVelocity().Size();
	Body->ForTagged(41, [&](USceneComponent* P) { P->SetRelativeRotation(FRotator(FMath::Sin(Clock * 23.f) * FMath::Min(4.f, Speed / 150.f), 0.f, FMath::Sin(Clock * 19.f) * FMath::Min(3.f, Speed / 200.f))); });
	// A broken wrist blade is gone.
	Body->ForTagged(43, [&](USceneComponent* P) { P->SetVisibility(!(Body->Broken & 1), true); });
	Body->ForTagged(44, [&](USceneComponent* P) { P->SetVisibility(!(Body->Broken & 2), true); });
}

// ================================================================================================ her fight (server)


namespace WRLyrathisImpl
{
	double LyClock(const UObject* W) { return W->GetWorld()->GetTimeSeconds(); }
	void LyTell(const AActor* Who, const FString& Text)
	{
		const APawn* P = Cast<APawn>(Who);
		if (AWRPlayerController* PC = P ? Cast<AWRPlayerController>(P->GetController()) : nullptr) PC->ClientNotify(Text);
	}
	TArray<AWRCharacterBase*> LyFoes(const AWRBoss* Boss, float Range)
	{
		TArray<AWRCharacterBase*> Out;
		for (TActorIterator<AWRCharacterBase> It(Boss->GetWorld()); It; ++It)
		{
			if (*It == Boss || !It->IsAlive() || !Boss->IsHostileTo(*It) || FWRCrystalBoss::IsDouble(*It)) continue;
			if (FVector::Dist2D(It->GetActorLocation(), Boss->GetActorLocation()) > Range) continue;
			Out.Add(*It);
		}
		return Out;
	}
	void LyHit(AWRBoss* Boss, AWRCharacterBase* Victim, float Damage, float Poise, uint8 Impact, bool bUnblockable, bool bLow, const FVector& Origin, float Mark = 0.f, bool bDot = false)
	{
		if (!Victim || !Victim->IsAlive()) return;
		FWRHit Hit;
		Hit.Instigator = Boss;
		Hit.Damage = Damage * WRTune(TEXT("bosses.damageScale"), 1.f) * FMath::Max(0.1f, WRTune(TEXT("server.creatureDifficulty"), 1.f)) * (Boss->Phase >= 3 ? LyT(TEXT("phase3DamageScale"), 1.4f) : 1.f);
		Hit.PoiseDamage = Poise;
		Hit.Impact = Impact;
		Hit.bUnblockable = bUnblockable;
		Hit.bLow = bLow;
		Hit.bDamageOverTime = bDot;
		Hit.bHeavy = Damage >= 30.f;
		Hit.Tier = Damage >= 30.f ? EWRHitTier::Heavy : EWRHitTier::Light;
		Hit.Origin = Origin;
		Hit.Element = Mark > 0.f ? WRTags::Element_Arcane : WRTags::Element_Physical;
		if (Mark > 0.f) Hit.Buildups.Add(TEXT("mark"), Mark);
		UWRCombatLibrary::ApplyHit(Victim, Hit);
	}
	FVector LyHeart(const AActor* Boss) { return Boss->GetActorLocation() + Boss->GetActorForwardVector() * Boss->GetSimpleCollisionRadius() * 0.3f + FVector(0.f, 0.f, Boss->GetSimpleCollisionHalfHeight() * 0.45f); }
	bool LyClear(const UWorld* World, const FVector& A, const FVector& B, const AActor* IgnoreA, const AActor* IgnoreB)
	{
		FCollisionObjectQueryParams Obj;
		Obj.AddObjectTypesToQuery(ECC_WorldStatic);
		Obj.AddObjectTypesToQuery(ECC_WorldDynamic);
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRLyrathisSight), false, IgnoreA);
		if (IgnoreB) Q.AddIgnoredActor(IgnoreB);
		FHitResult Hit;
		return !World->LineTraceSingleByObjectType(Hit, A, B, Obj, Q);
	}
	/** Where a prism's light lands a copy: 15 m out along the prism's facing, if that's open floor inside the hall. */
	bool LyCopySpot(const AWRBoss* Boss, const FWRCrystalBossState& St, const AWRCrystalBossProp* P, FVector& Out)
	{
		const float Now = P->ServerNow();
		const FVector Dir = FRotator(0.f, P->FacingNow(Now), 0.f).Vector();
		const float Dist = LyT(TEXT("copyDistance"), 1500.f);
		const FVector From = P->GetActorLocation() + FVector(0.f, 0.f, LyT(TEXT("beamExitHeight"), 140.f)) + Dir * 100.f;
		const FVector To = From + Dir * (Dist + 200.f);
		if (!LyClear(Boss->GetWorld(), From, To, P, Boss)) return false;
		// Another prism standing in the line takes the light (no copy).
		for (const TWeakObjectPtr<AWRCrystalBossProp>& W : AWRCrystalBossProp::All())
		{
			const AWRCrystalBossProp* O = W.Get();
			if (!O || O == P || (O->Kind != TEXT("turntable") && O->Kind != TEXT("lance"))) continue;
			const FVector C = FMath::ClosestPointOnSegment(O->GetActorLocation() + FVector(0.f, 0.f, 140.f), From, To);
			if (FVector::Dist2D(C, O->GetActorLocation()) < 110.f) return false;
		}
		const FVector Spot = P->GetActorLocation() + Dir * Dist;
		if (FVector::Dist2D(Spot, St.Home) > St.ArenaR - 300.f) return false;
		Out = Spot;
		return true;
	}
}

AWRCrystalBossProp* FWRCrystalBoss::NearestPrism(const UWorld* World, const FVector& At, const AWRCrystalBossProp* Except)
{
	AWRCrystalBossProp* Best = nullptr;
	float BestD = 1.e12f;
	for (const TWeakObjectPtr<AWRCrystalBossProp>& W : AWRCrystalBossProp::All())
	{
		AWRCrystalBossProp* P = W.Get();
		if (!P || P == Except || P->GetWorld() != World || (P->Kind != TEXT("turntable") && P->Kind != TEXT("lance"))) continue;
		const float D = float(FVector::DistSquared2D(P->GetActorLocation(), At));
		if (D < BestD) { BestD = D; Best = P; }
	}
	return Best;
}

void FWRCrystalBoss::DomeBurn(AWRBoss* Boss, float Amount)
{
	FWRCrystalBossState* St = WRCrystalBossFind(Boss);
	if (!St || !St->bDome || Amount <= 0.f) return;
	St->DomeHP -= Amount;
	if (UWRCrystalBossBody* Body = UWRCrystalBossBody::Get(Boss)) Body->Level = FMath::Clamp(St->DomeHP / FMath::Max(1.f, LyT(TEXT("domeHealth"), 1200.f)), 0.f, 1.f);
	if (St->DomeHP <= 0.f) BreakDome(Boss, *St, TEXT("her own light, turned onto it"));
}

void FWRCrystalBoss::RaiseDome(AWRBoss* Boss, FWRCrystalBossState& St, bool bPermanent)
{
	const double T = LyClock(Boss);
	St.bDome = true;
	St.bDomePermanent = bPermanent;
	St.DomeHP = LyT(TEXT("domeHealth"), 1200.f);
	St.DomeRaisedAt = T;
	St.DomeUntil = bPermanent ? T + 1.e7 : T + LyT(TEXT("domeSeconds"), 25.f);
	// Her next Prism beam comes within 6 s of the dome rising (so the room can be used against it).
	St.NextBeamDue = bPermanent ? 0.0 : T + FMath::FRandRange(2.f, LyT(TEXT("beamAfterDome"), 6.f));
	if (UWRCrystalBossBody* Body = UWRCrystalBossBody::Get(Boss))
	{
		Body->Point = Boss->GetActorLocation() - FVector(0.f, 0.f, Boss->GetSimpleCollisionHalfHeight());
		Body->Level = 1.f;
		Body->Pose = bPermanent ? 3 : 1;
	}
	WRFx::Broadcast(Boss, TEXT("crystal.dome_rise"), Boss->GetActorLocation(), FRotator::ZeroRotator, 3.f, TEXT("crystal.boss.dome_rise"));
	for (AWRCharacterBase* C : LyFoes(Boss, St.ArenaR + 500.f)) LyTell(C, bPermanent ? TEXT("Her dome stands for good: turn three prisms to face her") : TEXT("She raises her dome: turn a prism so her own beam strikes it"));
	UE_LOG(LogWildreach, Log, TEXT("CRYSTAL Lyrathis raises her dome%s"), bPermanent ? TEXT(" (permanent)") : TEXT(""));
}

void FWRCrystalBoss::BreakDome(AWRBoss* Boss, FWRCrystalBossState& St, const TCHAR* Why)
{
	if (!St.bDome) return;
	const double T = LyClock(Boss);
	const bool bTimeout = FCString::Strcmp(Why, TEXT("time")) == 0;
	St.bDome = false;
	if (UWRCrystalBossBody* Body = UWRCrystalBossBody::Get(Boss)) { Body->Point = FVector(0.f, 0.f, -1.e7f); Body->Level = 0.f; if (Body->Pose == 1) Body->Pose = 0; }
	if (bTimeout)
	{
		WRFx::Broadcast(Boss, TEXT("crystal.dome_fade"), Boss->GetActorLocation(), FRotator::ZeroRotator, 3.f, TEXT("crystal.boss.dome_fade"));
		return;
	}
	// Broken: she staggers, her stance open.
	const float Stagger = St.bDomePermanent ? LyT(TEXT("p3Stagger"), 8.f) : LyT(TEXT("domeStagger"), 3.f);
	if (Boss->Status) Boss->Status->Stagger(Stagger);
	Boss->StanceOpenUntil = T + Stagger;
	St.StaggerUntil = T + Stagger;
	if (St.bDomePermanent) { St.P3NextDomeAt = T + Stagger; for (double& F : St.FacedAt) F = -100.0; }
	WRFx::Broadcast(Boss, TEXT("crystal.dome_break"), Boss->GetActorLocation(), FRotator::ZeroRotator, 3.f, TEXT("crystal.boss.dome_break"));
	for (AWRCharacterBase* C : LyFoes(Boss, St.ArenaR + 500.f)) LyTell(C, FString::Printf(TEXT("The dome shatters (%s): she staggers"), Why));
	UE_LOG(LogWildreach, Log, TEXT("CRYSTAL Lyrathis's dome breaks: %s"), Why);
}

void FWRCrystalBoss::FireBeam(AWRBoss* Boss, FWRCrystalBossState& St, AWRCrystalBossProp* Through, float Windup, float Active, float SweepDeg, float Bias)
{
	UWorld* World = Boss->GetWorld();
	const float Now = LyNow(Boss);
	FActorSpawnParameters Params;
	Params.Owner = Boss;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWRPrismBeam* B = World->SpawnActor<AWRPrismBeam>(AWRPrismBeam::StaticClass(), Boss->GetActorLocation(), FRotator::ZeroRotator, Params);
	if (!B) return;
	B->Source = Boss;
	B->Prism = Through;
	B->Kind = Through ? 0 : 1;
	B->ThreadAt = Now;
	B->FireAt = Now + Windup;
	B->EndAt = Now + Windup + Active;
	B->Sweep = SweepDeg;
	B->Bias = Bias;
	B->Damage = LyT(TEXT("beamDamage"), 35.f);
	B->Mark = LyT(TEXT("beamMark"), 30.f);
	if (!St.Beam.IsValid() || St.Beam->EndAt < Now) St.Beam = B;
	else St.Beam2 = B;
	WRSound::PlayAt(Boss, TEXT("crystal.boss.beam_rise"), Boss->GetActorLocation());
}

void FWRCrystalBoss::StepCopies(AWRBoss* Boss, FWRCrystalBossState& St, float Dt)
{
	// Phase 2's copies: one on each of up to three prisms' lines (15 m out along its facing, open floor), moving with
	// the line as the prism is turned; a line that meets a wall or another prism puts its copy out for 20 s.
	const double T = LyClock(Boss);
	if (St.CopyBack.Num() < 16) St.CopyBack.Init(-100.0, 16);
	TArray<AWRCrystalBossProp*> Prisms;
	for (const TWeakObjectPtr<AWRCrystalBossProp>& W : St.Turntables) if (W.IsValid()) Prisms.Add(W.Get());
	// Keep or dissolve the copies there are (each knows its prism by its index tag).
	TSet<int32> Taken;
	for (int32 i = St.Copies.Num() - 1; i >= 0; --i)
	{
		AWRCreature* Cp = St.Copies[i].Get();
		if (!Cp) { St.Copies.RemoveAt(i); continue; }
		const int32 Pi = Cp->Tags.Num() ? FCString::Atoi(*Cp->Tags[0].ToString()) : -1;
		AWRCrystalBossProp* P = Prisms.IsValidIndex(Pi) ? Prisms[Pi] : nullptr;
		FVector Spot;
		if (!P || !LyCopySpot(Boss, St, P, Spot))
		{
			WRFx::Broadcast(Boss, TEXT("crystal.copy_dissolve"), Cp->GetActorLocation(), FRotator::ZeroRotator, 3.f, TEXT("crystal.boss.copy_dissolve"));
			Cp->Destroy();
			St.Copies.RemoveAt(i);
			if (St.CopyBack.IsValidIndex(Pi)) St.CopyBack[Pi] = T + LyT(TEXT("copyReform"), 20.f);
			continue;
		}
		Taken.Add(Pi);
		// It walks (light, but it walks: never pops) to where its line now lands, facing her foes.
		AWRCharacterBase* Face = Boss->AttackTarget.Get();
		const FVector From = Cp->GetActorLocation();
		FVector Next = FMath::VInterpConstantTo(From, FVector(Spot.X, Spot.Y, From.Z), Dt, 900.f);
		Cp->SetActorLocation(Next, false, nullptr, ETeleportType::None);
		if (UCharacterMovementComponent* M = Cp->GetCharacterMovement()) M->Velocity = Dt > 0.f ? (Next - From) / Dt : FVector::ZeroVector;
		if (Face) Cp->SetActorRotation(FRotator(0.f, (Face->GetActorLocation() - Next).Rotation().Yaw, 0.f));
	}
	// Form new ones on the nearest free prisms whose lines land on open floor.
	const int32 Want = int32(LyT(TEXT("copies"), 3.f));
	if (St.Copies.Num() >= Want) return;
	TArray<int32> Order;
	for (int32 i = 0; i < Prisms.Num(); ++i) Order.Add(i);
	Order.Sort([&](int32 A, int32 B) { return FVector::DistSquared2D(Prisms[A]->GetActorLocation(), Boss->GetActorLocation()) < FVector::DistSquared2D(Prisms[B]->GetActorLocation(), Boss->GetActorLocation()); });
	for (int32 Pi : Order)
	{
		if (St.Copies.Num() >= Want) break;
		if (Taken.Contains(Pi) || (St.CopyBack.IsValidIndex(Pi) && T < St.CopyBack[Pi])) continue;
		FVector Spot;
		if (!LyCopySpot(Boss, St, Prisms[Pi], Spot)) continue;
		FVector G = Spot;
		{
			FHitResult H;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(WRCopyGround), false, Boss);
			if (Boss->GetWorld()->LineTraceSingleByChannel(H, Spot + FVector(0.f, 0.f, 600.f), Spot - FVector(0.f, 0.f, 1500.f), ECC_WorldStatic, Q)) G = H.ImpactPoint;
		}
		AWRCreature* Cp = AWRCreature::SpawnCreature(Boss->GetWorld(), TEXT("lyrathis_copy"), G, Boss->GetActorRotation());
		if (!Cp) continue;
		if (AController* Ctl = Cp->GetController()) { Ctl->UnPossess(); Ctl->Destroy(); }
		if (UCharacterMovementComponent* M = Cp->GetCharacterMovement()) { M->bRunPhysicsWithNoController = true; M->SetMovementMode(MOVE_Walking); }
		Cp->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		Cp->Tags.Add(*FString::FromInt(Pi));
		UWRCrystalBossBody::Attach(Cp);
		St.Copies.Add(Cp);
		WRFx::Broadcast(Boss, TEXT("crystal.copy_form"), G, FRotator::ZeroRotator, 3.f, TEXT("crystal.boss.copy_form"));
	}
}

void FWRCrystalBoss::Echo(AWRBoss* Boss, FWRCrystalBossState& St, FName AttackId)
{
	// The copies repeat her move half a second later from where they stand (dodge her, then the echo). Her beams, the
	// rain, the grasp, the lance and the Flare are hers alone.
	const int32 Idx = Boss->Attacks.IndexOfByPredicate([AttackId](const FWRBossAttack& A) { return A.Id == AttackId; });
	if (!Boss->Attacks.IsValidIndex(Idx) || St.Copies.Num() == 0) return;
	const FWRBossAttack& A = Boss->Attacks[Idx];
	const float Delay = LyT(TEXT("copyEcho"), 0.5f);
	const float Strike = A.Windup + (A.bRow ? A.Row.Hold : 0.f);
	const FVector Point = Boss->AttackPoint;
	TWeakObjectPtr<AWRBoss> WeakB(Boss);
	for (const TWeakObjectPtr<AWRCreature>& W : St.Copies)
	{
		TWeakObjectPtr<AWRCreature> WeakC = W;
		FTimerHandle H1, H2;
		Boss->GetWorldTimerManager().SetTimer(H1, FTimerDelegate::CreateLambda([WeakC, Strike]()
		{
			if (AWRCreature* C = WeakC.Get()) C->PlaySwing(EWRSwingKind::Heavy, Strike + 0.2f);
		}), Delay, false);
		const EWRBossShape Shape = A.Shape;
		const float Range = A.Range, Size = A.Size, Dmg = A.Damage * LyT(TEXT("copyDamage"), 0.6f), Poise = A.Poise;
		const uint8 Impact = A.Impact;
		const bool bLow = A.bLow;
		Boss->GetWorldTimerManager().SetTimer(H2, FTimerDelegate::CreateLambda([WeakB, WeakC, Shape, Range, Size, Dmg, Poise, Impact, bLow, Point]()
		{
			AWRBoss* B = WeakB.Get();
			AWRCreature* C = WeakC.Get();
			if (!B || !C || !B->IsAlive()) return;
			const FVector At = Shape == EWRBossShape::TargetCircle ? Point : C->GetActorLocation();
			const FVector Fwd = C->GetActorForwardVector().GetSafeNormal2D();
			WRFx::Broadcast(B, TEXT("crystal.copy_echo"), At, Fwd.Rotation(), Size / 150.f, TEXT("crystal.boss.copy_echo"));
			for (AWRCharacterBase* V : LyFoes(B, 100000.f))
			{
				const FVector To = V->GetActorLocation() - At;
				if (Shape == EWRBossShape::TargetCircle || Shape == EWRBossShape::SelfCircle) { if (To.Size2D() > Size + V->GetSimpleCollisionRadius()) continue; }
				else if (Shape == EWRBossShape::Cone)
				{
					if (To.Size2D() > Range + C->GetSimpleCollisionRadius() || FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(float(FVector::DotProduct(To.GetSafeNormal2D(), Fwd)), -1.f, 1.f))) > Size) continue;
				}
				else continue;
				LyHit(B, V, Dmg, Poise, Impact, false, bLow, At);
			}
		}), Strike + Delay, false);
	}
}

void FWRCrystalBoss::TickGrasp(AWRBoss* Boss, FWRCrystalBossState& St, float Dt)
{
	AWRCharacterBase* V = St.Held.Get();
	if (!V) return;
	const double T = LyClock(Boss);
	if (!V->IsAlive() || !Boss->IsAlive()) { ReleaseGrasp(Boss, St, false); return; }
	if (T >= St.HeldTickAt)
	{
		// The crush: 22 a second through any guard.
		St.HeldTickAt = T + 0.5;
		LyHit(Boss, V, LyT(TEXT("grabDps"), 22.f) * 0.5f, 0.f, 0, true, false, Boss->GetActorLocation(), 0.f, true);
	}
	if (St.HeldFree >= LyT(TEXT("grabFree"), 150.f)) { LyTell(V, TEXT("Her wrist gives: she drops you")); ReleaseGrasp(Boss, St, false); return; }
	if (T >= St.HeldUntil) ReleaseGrasp(Boss, St, true);
	(void)Dt;
}

void FWRCrystalBoss::ReleaseGrasp(AWRBoss* Boss, FWRCrystalBossState& St, bool bThrow)
{
	AWRCharacterBase* V = St.Held.Get();
	St.Held = nullptr;
	St.HeldFree = 0.f;
	if (!V) return;
	V->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	V->SetActorEnableCollision(true);
	V->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	if (V->Status) V->Status->ClearStatus(WRTags::Status_Staggered);
	if (bThrow && V->IsAlive())
	{
		// Thrown 9 m away from her.
		const FVector Dir = (V->GetActorLocation() - Boss->GetActorLocation()).GetSafeNormal2D();
		V->LaunchCharacter(Dir * LyT(TEXT("grabThrow"), 900.f) + FVector(0.f, 0.f, 380.f), true, true);
		WRFx::Broadcast(Boss, TEXT("crystal.grasp_throw"), V->GetActorLocation(), Dir.Rotation(), 1.f, TEXT("crystal.boss.grasp_throw"));
	}
}

bool FWRCrystalBoss::TickLyrathis(AWRBoss* Boss, FWRCrystalBossState& St, float Dt)
{
	const double T = LyClock(Boss);
	TickGrasp(Boss, St, Dt);
	UWRCrystalBossBody* Body = UWRCrystalBossBody::Get(Boss);
	if (Body)
	{
		// The brow gem through a beam's windup; the Flare's pose.
		const AWRPrismBeam* B = St.Beam.Get();
		const float Now = LyNow(Boss);
		Body->Gem = B && Now < B->FireAt ? FMath::Clamp((Now - B->ThreadAt) / FMath::Max(0.1f, B->FireAt - B->ThreadAt), 0.f, 1.f) : (B && Now < B->EndAt ? 1.f : 0.f);
		if (Body->Pose == 2 && !(Boss->Attacks.IsValidIndex(Boss->Current) && Boss->Attacks[Boss->Current].Id == TEXT("sundering_flare"))) Body->Pose = 0;
		Body->Broken = (Boss->IsPartBroken(TEXT("blade_left")) ? 1 : 0) | (Boss->IsPartBroken(TEXT("blade_right")) ? 2 : 0);
	}
	if (St.bDome && !St.bDomePermanent && T >= St.DomeUntil) BreakDome(Boss, St, TEXT("time"));
	// Phase 2: the copies.
	if (Boss->Phase >= 2) StepCopies(Boss, St, Dt);
	// Phase 3 (artifacts): on the dais, the permanent dome; three prisms turned to face her within 10 s break it.
	if (Boss->Phase >= 3)
	{
		Boss->SetHome(St.Home + FVector(0.f, 0.f, 66.f));
		if (!St.bDome && T >= St.P3NextDomeAt && T >= St.StaggerUntil && FVector::Dist2D(Boss->GetActorLocation(), St.Home) < 400.f) RaiseDome(Boss, St, true);
		if (St.bDome && St.bDomePermanent)
		{
			const float Now = LyNow(Boss);
			int32 Faced = 0;
			for (int32 i = 0; i < St.Turntables.Num() && i < St.FacedAt.Num(); ++i)
			{
				const AWRCrystalBossProp* P = St.Turntables[i].Get();
				if (!P) continue;
				const float ToHer = (Boss->GetActorLocation() - P->GetActorLocation()).Rotation().Yaw;
				const bool bFacing = FMath::Abs(FMath::FindDeltaAngleDegrees(P->FacingNow(Now), ToHer)) <= LyT(TEXT("faceCone"), 15.f);
				if (bFacing && St.FacedAt[i] < 0.0) St.FacedAt[i] = T;
				if (!bFacing) St.FacedAt[i] = -100.0;
				if (St.FacedAt[i] >= 0.0 && T - St.FacedAt[i] <= LyT(TEXT("p3FaceWindow"), 10.f)) ++Faced;
			}
			if (Faced >= int32(LyT(TEXT("p3FaceNeeded"), 3.f)))
			{
				// Three fans converge on her: 1,200 in a second, and the dome comes down.
				if (AWRPrismBeam* B = St.Beam.Get()) B->Destroy();
				BreakDome(Boss, St, TEXT("three prisms turned on her"));
				return true;
			}
		}
		if (FVector::Dist2D(Boss->GetActorLocation(), St.Home) > 300.f && Boss->Current == INDEX_NONE)
		{
			Boss->AddMovementInput((St.Home - Boss->GetActorLocation()).GetSafeNormal2D(), 1.f);
			return true;
		}
		return false;
	}
	// The Prism ward at 85% and 65% (when she's free).
	const float Frac = Boss->GetAttributes() ? Boss->GetAttributes()->GetHealth() / FMath::Max(1.f, Boss->GetAttributes()->GetMaxHealth()) : 1.f;
	AWRCharacterBase* Target = Boss->AttackTarget.IsValid() && Boss->AttackTarget->IsAlive() ? Boss->AttackTarget.Get() : Boss->PickBossTarget();
	if (Boss->Current == INDEX_NONE && Target && !St.bDome && Boss->Phase == 1)
	{
		if (!St.bWard85 && Frac < LyT(TEXT("domeAt1"), 0.85f)) { St.bWard85 = true; if (Force(Boss, TEXT("prism_ward"), Target)) return true; }
		if (!St.bWard65 && Frac < LyT(TEXT("domeAt2"), 0.65f)) { St.bWard65 = true; if (Force(Boss, TEXT("prism_ward"), Target)) return true; }
	}
	// Kneeling under her dome: she doesn't walk; the beam comes when it's due, the Pulse when someone's close.
	if (St.bDome && !St.bDomePermanent)
	{
		Boss->GetCharacterMovement()->StopMovementImmediately();
		if (Boss->Current != INDEX_NONE) return false;
		if (Target && St.NextBeamDue > 0.0 && T >= St.NextBeamDue)
		{
			St.NextBeamDue = 0.0;
			if (Force(Boss, TEXT("prism_beam"), Target)) return true;
		}
		const int32 Pulse = Boss->Attacks.IndexOfByPredicate([](const FWRBossAttack& A) { return A.Id == TEXT("sovereigns_pulse"); });
		if (Target && Boss->Attacks.IsValidIndex(Pulse) && T >= Boss->Attacks[Pulse].ReadyAt && LyFoes(Boss, 700.f).Num() > 0) Force(Boss, TEXT("sovereigns_pulse"), Target);
		return true;
	}
	return false;
}

bool FWRCrystalBoss::LyrathisAllowed(const AWRBoss* Boss, const FWRBossAttack& Attack, const AWRCharacterBase* Target)
{
	const FWRCrystalBossState* St = WRCrystalBossFind(Boss);
	const FName Id = Attack.Id;
	if (!Target || !St) return true;
	const float Dist = float(FVector::Dist2D(Target->GetActorLocation(), Boss->GetActorLocation()));
	if (Id == TEXT("prism_ward")) return false;                          // forced at 85% / 65%
	if (Id == TEXT("sovereigns_pulse")) return St->bDome;                // only while the dome stands
	if (St->bDome && !St->bDomePermanent) return Id == TEXT("prism_beam");   // kneeling: the beam only
	if (Id == TEXT("glint_lance") || Id == TEXT("shard_rain")) return Dist >= 400.f;   // nothing lands inside 4 m
	if (Id == TEXT("prism_beam") || Id == TEXT("double_prism_beam")) return FWRCrystalBoss::NearestPrism(Boss->GetWorld(), Boss->GetActorLocation()) != nullptr && !(St->Beam.IsValid() && St->Beam->EndAt > LyNow(Boss)) && (Boss->Phase >= 2 || !St->Beam2.IsValid() || St->Beam2->EndAt < LyNow(Boss));
	if (Id == TEXT("all_prisms")) return Boss->Phase >= 3 && FVector::Dist2D(Boss->GetActorLocation(), St->Home) < 400.f;
	// A broken wrist blade takes the blade moves on that arm away (Shardblade uses the other arm).
	const bool bLeftGone = Boss->IsPartBroken(TEXT("blade_left")), bRightGone = Boss->IsPartBroken(TEXT("blade_right"));
	if (bLeftGone && bRightGone && (Id.ToString().Contains(TEXT("shardblade")) || Id.ToString().Contains(TEXT("waltz")))) return false;
	return true;
}

void FWRCrystalBoss::LyrathisStarted(AWRBoss* Boss, FWRCrystalBossState& St, FWRBossAttack& Attack, AWRCharacterBase* Target)
{
	const FName Id = Attack.Id;
	const float Windup = Attack.Windup + (Attack.bRow ? Attack.Row.Hold : 0.f);
	const float Active = Attack.bRow ? Attack.Row.Active : 1.6f;
	if (Id == TEXT("prism_beam") || Id == TEXT("double_prism_beam"))
	{
		// The prism nearest her, chosen now and never changed (the guide thread shows which); the double beam the two
		// nearest. The fan's centre ray bends toward the furthest tame (the tame-punish rule).
		AWRCrystalBossProp* P1 = NearestPrism(Boss->GetWorld(), Boss->GetActorLocation());
		AWRCrystalBossProp* P2 = Id == TEXT("double_prism_beam") ? NearestPrism(Boss->GetWorld(), Boss->GetActorLocation(), P1) : nullptr;
		for (AWRCrystalBossProp* P : { P1, P2 })
		{
			if (!P) continue;
			float Bias = 0.f;
			AWRCharacterBase* Far = nullptr;
			float FarD = -1.f;
			for (AWRCharacterBase* C : LyFoes(Boss, St.ArenaR + 1500.f))
			{
				const AWRCreature* Cr = Cast<AWRCreature>(C);
				if (!Cr || !Cr->IsTamed()) continue;
				const float D = float(FVector::Dist2D(C->GetActorLocation(), Boss->GetActorLocation()));
				if (D > FarD) { FarD = D; Far = C; }
			}
			if (Far)
			{
				const float Want = (Far->GetActorLocation() - P->GetActorLocation()).Rotation().Yaw;
				Bias = FMath::Clamp(FMath::FindDeltaAngleDegrees(P->FacingNow(P->ServerNow()), Want), -LyT(TEXT("beamTameBend"), 30.f), LyT(TEXT("beamTameBend"), 30.f));
			}
			FireBeam(Boss, St, P, Windup, Active, (FMath::RandBool() ? 1.f : -1.f) * LyT(TEXT("beamSweep"), 40.f), Bias);
			if (P2 == nullptr) break;
		}
	}
	if (Id == TEXT("sundering_flare"))
	{
		St.FlareStart = LyClock(Boss);
		if (UWRCrystalBossBody* Body = UWRCrystalBossBody::Get(Boss)) Body->Pose = 2;
		// The hall goes dark but the prisms: her light drains into her chest.
		WRFx::Broadcast(Boss, TEXT("crystal.flare_gather"), LyHeart(Boss), FRotator::ZeroRotator, 3.f, TEXT("crystal.boss.flare_swell"), Windup);
		for (AWRCharacterBase* C : LyFoes(Boss, St.ArenaR + 500.f)) LyTell(C, TEXT("Her light drains into her chest: get behind a prism, a pillar or the dais"));
	}
	if (Id == TEXT("all_prisms"))
	{
		// Phase 3: the turning beam from the dais.
		FireBeam(Boss, St, nullptr, Windup, Active, 0.f, Boss->GetActorRotation().Yaw);
	}
	WRSound::PlayAt(Boss, Id == TEXT("sovereigns_grasp") ? TEXT("crystal.boss.lyrathis_grasp") : Id.ToString().Contains(TEXT("beam")) ? TEXT("crystal.boss.lyrathis_beam") : TEXT("crystal.boss.lyrathis_tell"), Boss->GetActorLocation());
	// The copies echo her (phase 2): melee and slams, half a second later.
	if (Boss->Phase >= 2 && (Attack.Shape == EWRBossShape::Cone || Attack.Shape == EWRBossShape::SelfCircle || (Attack.Shape == EWRBossShape::TargetCircle && Id == TEXT("held_facet"))) && Id != TEXT("shard_rain") && Id != TEXT("sundering_flare") && Id != TEXT("prism_ward") && Id != TEXT("all_prisms"))
	{
		Echo(Boss, St, Id);
	}
	(void)Target;
}

bool FWRCrystalBoss::LyrathisStrike(AWRBoss* Boss, FWRCrystalBossState& St, FWRBossAttack& Attack)
{
	const FName Id = Attack.Id;
	UWorld* World = Boss->GetWorld();
	if (Id == TEXT("prism_beam") || Id == TEXT("double_prism_beam") || Id == TEXT("all_prisms")) return true;   // the beam actor deals it
	if (Id == TEXT("prism_ward")) { RaiseDome(Boss, St, false); return true; }
	if (Id == TEXT("sundering_flare"))
	{
		// A radial burst from her heart crystal: everyone with a clear line to her chest within 30 m takes it.
		const FVector Heart = LyHeart(Boss);
		WRFx::Broadcast(Boss, TEXT("crystal.flare_burst"), Heart, FRotator::ZeroRotator, 6.f, TEXT("crystal.boss.flare_burst"));
		for (AWRCharacterBase* C : LyFoes(Boss, Attack.Size))
		{
			if (!LyClear(World, Heart, C->GetActorLocation() + FVector(0.f, 0.f, 40.f), Boss, C)) continue;
			LyHit(Boss, C, Attack.Damage, Attack.Poise, Attack.Impact, true, false, Heart, Attack.bRow ? Attack.Row.Buildups.FindRef(TEXT("mark")) : 60.f);
		}
		return true;
	}
	if (Id == TEXT("shard_rain"))
	{
		// Eight shards break from the ceiling over 2 s round every survivor (one where they stand), picked now; each
		// shard's real shadow grows on the floor for 1 s before it lands.
		const int32 N = int32(LyT(TEXT("rainShards"), 8.f));
		const float Over = LyT(TEXT("rainOver"), 2.f), Shadow = LyT(TEXT("rainShadow"), 1.f), R = LyT(TEXT("rainRadius"), 150.f);
		TWeakObjectPtr<AWRBoss> WeakB(Boss);
		for (AWRCharacterBase* C : LyFoes(Boss, St.ArenaR + 500.f))
		{
			for (int32 k = 0; k < N; ++k)
			{
				FVector P = C->GetActorLocation() - FVector(0.f, 0.f, C->GetSimpleCollisionHalfHeight());
				if (k > 0) { const float A = FMath::FRandRange(0.f, 2.f * PI); P += FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * FMath::FRandRange(150.f, LyT(TEXT("rainSpread"), 300.f) * 1.6f); }
				FHitResult H;
				FCollisionQueryParams Q(SCENE_QUERY_STAT(WRRainGround), false, Boss);
				if (World->LineTraceSingleByChannel(H, P + FVector(0.f, 0.f, 400.f), P - FVector(0.f, 0.f, 1500.f), ECC_WorldStatic, Q)) P = H.ImpactPoint;
				const float Start = Over * k / FMath::Max(1, N);
				FTimerHandle S1, S2;
				Boss->GetWorldTimerManager().SetTimer(S1, FTimerDelegate::CreateLambda([WeakB, P, Shadow, R]()
				{
					AWRBoss* B = WeakB.Get();
					if (!B) return;
					AWRGroundMarker::Spawn(B->GetWorld(), P + FVector(0.f, 0.f, 3.f), FRotator::ZeroRotator, FVector2D(R, R), false, FLinearColor(0.02f, 0.02f, 0.025f), Shadow + 0.15f);
					WRFx::Broadcast(B, TEXT("crystal.shard_fall"), P + FVector(0.f, 0.f, 2600.f), FRotator::ZeroRotator, 1.f, TEXT("crystal.boss.shard_crack"), Shadow);
				}), FMath::Max(0.01f, Start), false);
				Boss->GetWorldTimerManager().SetTimer(S2, FTimerDelegate::CreateLambda([WeakB, P, R, Dmg = Attack.Damage, Poise = Attack.Poise, Imp = Attack.Impact]()
				{
					AWRBoss* B = WeakB.Get();
					if (!B) return;
					WRFx::Broadcast(B, TEXT("crystal.shard_land"), P, FRotator::ZeroRotator, R / 100.f, TEXT("crystal.boss.shard_land"));
					for (AWRCharacterBase* V : LyFoes(B, 100000.f))
					{
						if (FVector::Dist2D(V->GetActorLocation(), P) > R + V->GetSimpleCollisionRadius()) continue;
						LyHit(B, V, Dmg, Poise, Imp, false, false, P + FVector(0.f, 0.f, 600.f));
					}
				}), Start + Shadow, false);
			}
		}
		return true;
	}
	if (Id == TEXT("glint_lance"))
	{
		// A 6 m crystal javelin thrown straight at where you stand as the hold ends; it never homes; it stays in the floor
		// 20 s as a small prism (her beam splits through it, a copy can stand on its line); three hits shatter it.
		AWRCharacterBase* Tgt = Boss->AttackTarget.Get();
		FVector To = Tgt ? Tgt->GetActorLocation() - FVector(0.f, 0.f, Tgt->GetSimpleCollisionHalfHeight()) : Boss->AttackPoint;
		const FVector From = Boss->GetActorLocation() + Boss->GetActorForwardVector() * Boss->GetSimpleCollisionRadius() + FVector(0.f, 0.f, Boss->GetSimpleCollisionHalfHeight() * 0.7f);
		const float Speed = Attack.bRow ? Attack.Row.ProjectileSpeed : 1600.f;
		const float Grav = LyT(TEXT("lanceGravity"), 0.2f);
		const FVector Vel = AWRProjectile::LobVelocity(From, To, Speed, Grav);
		FActorSpawnParameters Params;
		Params.Owner = Boss;
		Params.Instigator = Boss;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AWRProjectile* P = World->SpawnActor<AWRProjectile>(AWRProjectile::StaticClass(), From, Vel.Rotation(), Params))
		{
			FWRHit Hit;
			Hit.Instigator = Boss;
			Hit.Damage = Attack.Damage * WRTune(TEXT("bosses.damageScale"), 1.f);
			Hit.PoiseDamage = Attack.Poise;
			Hit.Impact = Attack.Impact;
			Hit.bHeavy = true;
			Hit.Tier = EWRHitTier::Heavy;
			Hit.bProjectile = true;
			Hit.bReflectable = true;   // a lance flies straight enough to be thrown back
			Hit.Element = WRTags::Element_Arcane;
			P->InitLob(Hit, Vel, Grav, 120.f, FString());
			P->SetTrail(TEXT("crystal.lance_trail"));
			const float Yaw = Vel.Rotation().Yaw;
			TWeakObjectPtr<AWRBoss> WeakB(Boss);
			P->OnLanded = [WeakB, Yaw](const FVector& At)
			{
				AWRBoss* B = WeakB.Get();
				if (!B) return;
				const FTransform Tr(FRotator(0.f, Yaw, 0.f), At);
				if (AWRCrystalBossProp* L = B->GetWorld()->SpawnActorDeferred<AWRCrystalBossProp>(AWRCrystalBossProp::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
				{
					L->Kind = TEXT("lance");
					L->Size = 200.f;
					L->Index = 8;
					L->OwnerBoss = B;
					L->FinishSpawning(Tr);
					L->SetLifeSpan(LyT(TEXT("lanceSeconds"), 20.f));
				}
				WRFx::Broadcast(B, TEXT("crystal.shard_land"), At, FRotator::ZeroRotator, 1.5f, TEXT("crystal.boss.lance_land"));
			};
		}
		return true;
	}
	return false;
}

float FWRCrystalBoss::LyrathisIncoming(AWRBoss* Boss, FWRCrystalBossState& St, const FWRHit& Hit, float Damage)
{
	AWRCharacterBase* By = Cast<AWRCharacterBase>(Hit.Instigator.Get());
	// Weak to the Void (x1.5, crystal 2.14).
	if (Hit.Element == WRTags::Element_Void) Damage *= LyT(TEXT("voidWeak"), 1.5f);
	// A blow on her while she holds someone counts toward her wrist giving (any but the held one's).
	if (St.Held.IsValid() && By && By != St.Held.Get()) St.HeldFree += Damage;
	if (St.bDome)
	{
		const float R = LyT(TEXT("domeRadius"), 600.f);
		const UWRCrystalBossBody* Body = UWRCrystalBossBody::Get(Boss);
		const FVector DomeAt = Body ? Body->Point : Boss->GetActorLocation();
		const bool bOutside = By && FVector::Dist2D(By->GetActorLocation(), DomeAt) > R;
		if (bOutside)
		{
			// Shots and spells stop on the dome and wear it (Mana burn x2, the Void x3; a void rift eats it at once);
			// a blade reaches through at a quarter.
			if (Hit.bProjectile || Hit.bSpell)
			{
				if (Hit.PathId == TEXT("void") && !Hit.bProjectile)
				{
					BreakDome(Boss, St, TEXT("a void rift ate it"));
					return 0.f;
				}
				float Wear = Damage * (WRArcane::BurnsShields(By, Hit) ? LyT(TEXT("domeManaBurn"), 2.f) : 1.f) * (Hit.Element == WRTags::Element_Void ? LyT(TEXT("domeVoid"), 3.f) : 1.f);
				if (!St.bDomePermanent) DomeBurn(Boss, Wear);
				WRFx::Broadcast(Boss, TEXT("arcane.dome_stop"), !Hit.ImpactPoint.IsZero() ? Hit.ImpactPoint : DomeAt + (By->GetActorLocation() - DomeAt).GetSafeNormal() * R, FRotator::ZeroRotator, 1.f, TEXT("arcane.dome_stop"));
				return 0.f;
			}
			return Damage * LyT(TEXT("domeMeleeThrough"), 0.25f);
		}
	}
	return Damage;
}

void FWRCrystalBoss::LyrathisLanded(AWRBoss* Boss, FWRCrystalBossState& St, AWRCharacterBase* Victim, FName AttackId, EWRHitResult Result)
{
	if (AttackId != TEXT("sovereigns_grasp") || Result != EWRHitResult::Hit || !Victim || !Victim->IsAlive() || St.Held.IsValid()) return;
	// The grasp: lifted in her right hand, crushed 2 s, then thrown; 150 from the others on her wrist drops you early.
	const double T = LyClock(Boss);
	St.Held = Victim;
	St.HeldUntil = T + LyT(TEXT("grabSeconds"), 2.f);
	St.HeldTickAt = T + 0.5;
	St.HeldFree = 0.f;
	Victim->SetActorEnableCollision(false);
	Victim->GetCharacterMovement()->DisableMovement();
	Victim->AttachToActor(Boss, FAttachmentTransformRules::KeepWorldTransform);
	Victim->SetActorRelativeLocation(FVector(Boss->GetSimpleCollisionRadius() * 1.1f, Boss->GetSimpleCollisionRadius() * 0.35f, Boss->GetSimpleCollisionHalfHeight() * 0.55f));
	if (Victim->Status) Victim->Status->Stagger(LyT(TEXT("grabSeconds"), 2.f) + 0.4f);
	if (Boss->Attacks.IsValidIndex(Boss->Current) && Boss->StrikeState == AWRBoss::EStrike::Strike) Boss->StrikeEnds = FMath::Max(Boss->StrikeEnds, St.HeldUntil);
	LyTell(Victim, TEXT("In her hand: your friends can break her grip"));
}
