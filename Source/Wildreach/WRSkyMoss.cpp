// Levistone moss (Aaron 11-16, r-moss: "Yes add the moss to collect. Make it look good."; showcase-notes
// cooks/wild-answers-1115.md 2.3).
//
// Beside every keel outcrop's levistone a lump of the same stone floats on its own lift, out from the island's root over
// open air, and the moss has taken it: velvet cushions crowding its top, a ragged skirt creeping down its waist, beards
// of old-man's-beard hanging under it, and the stone's own blue crystal poking from its keel. The beard tips and the
// crystal glow cyan at night (the levistone feeds the moss; the same glow the island keels carry). Nothing touches the
// island, so nothing can float off a surface or pass through it: the lump rides its lift with a slow bob and turn, and the
// beards swing under it as one, eased, every frame, only near a camera.
//
// The moss spot itself (hold E, the gather and the per-survivor regrowth) is AWRSkySpot Kind "moss" (WRSkyWorld.cpp);
// this file is only its look, built on every machine that draws.

#include "WRSkyWorld.h"
#include "WRToolMesh.h"
#include "WRGameState.h"
#include "WRTuning.h"
#include "ProceduralMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"

namespace WRSkyMossLook
{
	float Tn(const TCHAR* Key, float Def) { return WRTune(*(FString(TEXT("skyWorld.moss.")) + Key), Def); }

	/** A repeatable 0..1 from a seed and two ints (every machine builds the same bed). */
	float H(uint32 Seed, int32 A, int32 B = 0)
	{
		uint32 X = Seed * 0x9E3779B1u ^ uint32(A) * 0x85EBCA77u ^ uint32(B) * 0xC2B2AE3Du;
		X ^= X >> 15; X *= 0x2C1B3C6Du; X ^= X >> 12; X *= 0x297A2D39u; X ^= X >> 15;
		return float(X & 0xFFFFFF) / float(0xFFFFFF);
	}

	/** The rock's profile (height above its middle, radius): a squat lump, widest a little below its middle. */
	struct FProfile { float Z, R; };
	const FProfile Rock[] = { { -40.f, 6.f }, { -36.f, 30.f }, { -27.f, 52.f }, { -14.f, 66.f }, { 2.f, 70.f }, { 16.f, 60.f }, { 26.f, 40.f }, { 31.f, 16.f }, { 32.f, 0.f } };

	/** The rock's surface height at a radius, on its top (bTop) or under it. */
	float SurfaceZ(float Rad, bool bTop)
	{
		const int32 N = UE_ARRAY_COUNT(Rock);
		const int32 Widest = 4;
		if (bTop)
		{
			for (int32 i = Widest; i + 1 < N; ++i)
			{
				if (Rad <= Rock[i].R && Rad >= Rock[i + 1].R)
				{
					const float T = (Rock[i].R - Rad) / FMath::Max(0.1f, Rock[i].R - Rock[i + 1].R);
					return FMath::Lerp(Rock[i].Z, Rock[i + 1].Z, T);
				}
			}
			return Rock[N - 1].Z;
		}
		for (int32 i = Widest; i > 0; --i)
		{
			if (Rad <= Rock[i].R && Rad >= Rock[i - 1].R)
			{
				const float T = (Rock[i].R - Rad) / FMath::Max(0.1f, Rock[i].R - Rock[i - 1].R);
				return FMath::Lerp(Rock[i].Z, Rock[i - 1].Z, T);
			}
		}
		return Rock[0].Z;
	}

	UProceduralMeshComponent* MakeMesh(AActor* Owner, USceneComponent* Under, const FWRToolMesh& M, UMaterialInterface* Mat)
	{
		UProceduralMeshComponent* C = NewObject<UProceduralMeshComponent>(Owner);
		C->SetupAttachment(Under);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCanEverAffectNavigation(false);
		C->bUseAsyncCooking = true;
		C->RegisterComponent();
		M.ApplyTo(C);
		if (Mat) C->SetMaterial(0, Mat);
		C->SetCastShadow(true);
		return C;
	}
}

void AWRSkySpot::BuildMossLook()
{
	using namespace WRSkyMossLook;
	const FVector P = GetActorLocation();
	const uint32 Seed = GetTypeHash(FIntVector(FMath::FloorToInt(P.X / 100.f), FMath::FloorToInt(P.Y / 100.f), FMath::FloorToInt(P.Z / 100.f)));
	MossPhase = H(Seed, 99) * 10.f;

	MossFloat = NewObject<USceneComponent>(this, TEXT("MossFloat"));
	MossFloat->SetupAttachment(RootComponent);
	MossFloat->RegisterComponent();
	MossBeards = NewObject<USceneComponent>(this, TEXT("MossBeards"));
	MossBeards->SetupAttachment(MossFloat);
	MossBeards->SetRelativeLocation(FVector(0.f, 0.f, -30.f));   // they swing from the rock's underside
	MossBeards->RegisterComponent();

	// ---- the lump of levistone: lofted rings with a hashed lump in each vertex (flat-shaded facets, pale blue-grey) ----
	FWRToolMesh StoneMesh;
	{
		const int32 Sides = 14;
		TArray<TArray<FVector>> Rings;
		for (int32 r = 0; r < UE_ARRAY_COUNT(Rock); ++r)
		{
			TArray<FVector> Ring;
			for (int32 s = 0; s < Sides; ++s)
			{
				const float A = 2.f * PI * s / Sides;
				const float Lump = 1.f + 0.16f * (H(Seed, r, s) - 0.5f) + 0.1f * FMath::Sin(A * 3.f + H(Seed, 7) * 6.f);
				const float Rad = Rock[r].R * Lump * (s % 2 ? 1.f : 0.96f);
				Ring.Add(FVector(FMath::Cos(A) * Rad, FMath::Sin(A) * Rad * 0.86f, Rock[r].Z + 3.f * (H(Seed, r + 40, s) - 0.5f)));
			}
			Rings.Add(Ring);
		}
		StoneMesh.Loft(Rings, FLinearColor(0.40f, 0.47f, 0.55f), 0.78f, true);
	}
	MakeMesh(this, MossFloat, StoneMesh, nullptr);

	// ---- the moss: velvet cushions crowding the top, a ragged skirt round the waist ----
	FWRToolMesh Moss;
	const FLinearColor Greens[] = { FLinearColor(0.16f, 0.24f, 0.09f), FLinearColor(0.2f, 0.28f, 0.11f), FLinearColor(0.13f, 0.2f, 0.08f), FLinearColor(0.27f, 0.32f, 0.19f) };
	const auto Dome = [](float T, float R) { return FVector(T, R, R); };
	for (int32 c = 0; c < 26; ++c)
	{
		// Rings of cushions from the crown out to the shoulder; bigger in the middle, smaller toward the edge.
		const float Rad = c == 0 ? 0.f : FMath::Sqrt(float(c) / 26.f) * 58.f;
		const float A = c * 2.39996f + H(Seed, c, 1) * 0.6f;   // the golden angle: an even crowd, no rows
		const float R = FMath::Lerp(30.f, 13.f, Rad / 58.f) * (0.8f + 0.4f * H(Seed, c, 2));
		const float Hgt = R * (0.42f + 0.2f * H(Seed, c, 3));
		const FVector Base(FMath::Cos(A) * Rad, FMath::Sin(A) * Rad * 0.86f, SurfaceZ(Rad, true) - R * 0.22f);
		// Each cushion leans out with the slope it sits on.
		const FVector Up = (FVector(Base.X, Base.Y, 0.f) * 0.012f + FVector::UpVector).GetSafeNormal();
		Moss.Lathe(Base, Up, Hgt, { Dome(0.f, R), Dome(0.35f, R * 0.95f), Dome(0.7f, R * 0.7f), Dome(0.92f, R * 0.32f), Dome(1.f, 0.1f) }, 12,
			Greens[(c + int32(H(Seed, c, 4) * 4.f)) % 4], 0.97f);
	}
	// The skirt: a moss band hugging the widest part, ragged at its lower edge.
	{
		const int32 Sides = 18;
		TArray<TArray<FVector>> Rings;
		for (int32 r = 0; r < 3; ++r)
		{
			TArray<FVector> Ring;
			const float Z = r == 0 ? 14.f : r == 1 ? 2.f : -10.f;
			for (int32 s = 0; s < Sides; ++s)
			{
				const float A = 2.f * PI * s / Sides;
				const float Ragged = r == 2 ? -8.f * H(Seed, s, 60) : 0.f;
				const float Rad = (r == 0 ? 63.f : r == 1 ? 73.f : 68.f) * (1.f + 0.05f * (H(Seed, s, 61) - 0.5f));
				Ring.Add(FVector(FMath::Cos(A) * Rad, FMath::Sin(A) * Rad * 0.86f, Z + Ragged));
			}
			Rings.Add(Ring);
		}
		Moss.Loft(Rings, Greens[1], 0.96f, false);
	}
	MakeMesh(this, MossFloat, Moss, nullptr);

	// ---- the beards: old-man's-beard strands from the underside, a slight twist in each, swinging together ----
	FWRToolMesh Beards;
	FWRToolMesh Glow;        // the beard tips, swinging with the beards (M_CrystalGlow: vertex alpha = how much it glows)
	FWRToolMesh StoneGlow;   // the flecks in the cushions and the keel crystal, still on the stone
	const FLinearColor Beard(0.34f, 0.39f, 0.29f), Tip(0.32f, 0.95f, 0.9f), Crystal(0.55f, 0.76f, 0.95f);
	const int32 NBeards = 22;
	for (int32 b = 0; b < NBeards; ++b)
	{
		const float Rad = 8.f + 50.f * FMath::Sqrt(H(Seed, b, 10));
		const float A = b * 2.39996f + H(Seed, b, 11);
		const FVector Root(FMath::Cos(A) * Rad, FMath::Sin(A) * Rad * 0.86f, SurfaceZ(Rad, false) + 4.f + 30.f);   // + the pivot's -30
		const float Len = (b % 3 == 0 ? 120.f : 60.f) + 60.f * H(Seed, b, 12);
		const float Wave = 2.f + 3.f * H(Seed, b, 13);
		TArray<FVector> Pts;
		for (int32 k = 0; k <= 12; ++k)
		{
			const float T = k / 12.f;
			Pts.Add(Root + FVector(FMath::Sin(T * 7.f + b) * Wave * T, FMath::Cos(T * 6.f + b * 1.3f) * Wave * T, -T * Len));
		}
		Beards.Tube(Pts, FMath::Lerp(2.2f, 1.2f, H(Seed, b, 14)), 5, Beard * (0.85f + 0.3f * H(Seed, b, 15)), 0.95f);
		// A short side strand off the longer ones.
		if (Len > 120.f)
		{
			const FVector From = Pts[5];
			TArray<FVector> Side;
			for (int32 k = 0; k <= 6; ++k) Side.Add(From + FVector(k * 2.5f, -k * 1.5f, -k * 6.f));
			Beards.Tube(Side, 1.0f, 4, Beard * 0.9f, 0.95f);
			Glow.Lathe(Side.Last() - FVector(0.f, 0.f, 1.6f), FVector::UpVector, 3.2f, { Dome(0.f, 0.3f), Dome(0.5f, 1.6f), Dome(1.f, 0.3f) }, 6, Tip, 1.f);
		}
		Glow.Lathe(Pts.Last() - FVector(0.f, 0.f, 2.4f), FVector::UpVector, 4.8f, { Dome(0.f, 0.4f), Dome(0.5f, 2.4f), Dome(1.f, 0.4f) }, 8, Tip, 1.f);
	}
	MakeMesh(this, MossBeards, Beards, nullptr);
	// Flecks of glow in the cushions' tops (few: the moss is green by day).
	for (int32 f = 0; f < 9; ++f)
	{
		const float Rad = 10.f + 40.f * H(Seed, f, 30);
		const float A = H(Seed, f, 31) * 2.f * PI;
		const FVector At(FMath::Cos(A) * Rad, FMath::Sin(A) * Rad * 0.86f, SurfaceZ(Rad, true) + 9.f);
		StoneGlow.Lathe(At, FVector::UpVector, 2.4f, { Dome(0.f, 0.3f), Dome(0.5f, 1.2f), Dome(1.f, 0.3f) }, 6, Tip, 0.6f);
	}
	// The stone's own keel: blue crystal points under it (as every island's keel has).
	for (int32 k = 0; k < 5; ++k)
	{
		const float A = k * 1.26f + H(Seed, k, 40);
		const FVector Base(FMath::Cos(A) * 16.f, FMath::Sin(A) * 14.f, -34.f);
		const FVector Dir = (FVector(FMath::Cos(A) * 0.35f, FMath::Sin(A) * 0.35f, -1.f)).GetSafeNormal();
		const float L = 18.f + 16.f * H(Seed, k, 41);
		StoneGlow.Lathe(Base, Dir, L, { Dome(0.f, 5.5f), Dome(0.6f, 3.6f), Dome(1.f, 0.2f) }, 6, Crystal, 0.8f);
	}
	UMaterialInterface* GlowBase = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_CrystalGlow.M_CrystalGlow"));
	if (GlowBase)
	{
		MossGlow = UMaterialInstanceDynamic::Create(GlowBase, this);
		MossGlow->SetVectorParameterValue(TEXT("GlowColor"), FLinearColor(0.55f, 1.f, 1.f));
		MossGlow->SetScalarParameterValue(TEXT("GlowScale"), 6.f);
		MossGlow->SetScalarParameterValue(TEXT("Rest"), Tn(TEXT("glowDay"), 0.12f));
	}
	// The tips swing with the beards; the flecks and the crystal stay on the stone.
	UMaterialInterface* GlowMat = MossGlow ? static_cast<UMaterialInterface*>(MossGlow) : nullptr;
	MakeMesh(this, MossBeards, Glow, GlowMat)->SetCastShadow(false);
	MakeMesh(this, MossFloat, StoneGlow, GlowMat)->SetCastShadow(false);

	// A dim cyan light under it at night: the beards throw it on the root above (and the player on the ledge).
	MossLight = NewObject<UPointLightComponent>(this, TEXT("MossLight"));
	MossLight->SetupAttachment(MossFloat);
	MossLight->SetRelativeLocation(FVector(0.f, 0.f, -90.f));
	MossLight->SetLightColor(FLinearColor(0.35f, 0.95f, 0.9f));
	MossLight->SetAttenuationRadius(Tn(TEXT("lightRadius"), 420.f));
	MossLight->SetIntensity(0.f);
	MossLight->SetCastShadows(false);
	MossLight->RegisterComponent();
}

void AWRSkySpot::TickMossLook(float DeltaSeconds)
{
	using namespace WRSkyMossLook;
	// Only near a camera (the bob and sway are a few centimetres): far away it holds still and costs nothing.
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const FVector Cam = PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : GetActorLocation();
	const bool bNear = FVector::DistSquared(Cam, GetActorLocation()) < FMath::Square(Tn(TEXT("lookRange"), 6000.f));
	// Night: the beard tips and the keel crystal light up (eased over a few seconds at dusk and dawn).
	const float WantNight = AWRGameState::IsNightIn(this) ? 1.f : 0.f;
	MossNight = MossNight < 0.f ? WantNight : FMath::FInterpConstantTo(MossNight, WantNight, DeltaSeconds, 0.2f);
	if (MossGlow) MossGlow->SetScalarParameterValue(TEXT("Rest"), FMath::Lerp(Tn(TEXT("glowDay"), 0.12f), Tn(TEXT("glowNight"), 0.9f), MossNight));
	if (MossLight) MossLight->SetIntensity(bNear ? Tn(TEXT("lightNight"), 900.f) * MossNight : 0.f);
	if (!bNear || !MossFloat) return;
	MossPhase += DeltaSeconds;
	const float Bob = FMath::Sin(MossPhase * 2.f * PI * Tn(TEXT("bobHz"), 0.18f)) * Tn(TEXT("bobCm"), 7.f);
	const float Turn = FMath::Sin(MossPhase * 2.f * PI * Tn(TEXT("bobHz"), 0.18f) * 0.37f + 1.3f) * Tn(TEXT("turnDeg"), 5.f);
	MossFloat->SetRelativeLocationAndRotation(FVector(0.f, 0.f, Bob), FRotator(0.f, Turn, 0.f));
	// The beards lag the bob a quarter beat and swing on two slow axes (never in step with the turn).
	const float Sw = Tn(TEXT("swayDeg"), 4.f), Hz = Tn(TEXT("swayHz"), 0.33f);
	MossBeards->SetRelativeRotation(FRotator(FMath::Sin(MossPhase * 2.f * PI * Hz) * Sw, 0.f, FMath::Sin(MossPhase * 2.f * PI * Hz * 0.71f + 2.1f) * Sw * 0.8f));
}
