// The Sky's code-built bodies (see WRSkyBeast.h; cooks/biome-sky.md 2.9, build-biome-sky.md 2.11).

#include "WRSkyBeast.h"
#include "Wildreach.h"
#include "WRCreature.h"
#include "WRGameState.h"
#include "WRSkyCreatures.h"
#include "WRToolMesh.h"
#include "WRTuning.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace WRSkyBeastImpl
{
	FColor BeC(const TCHAR* Hex, float Rough) { return FWRToolMesh::Pack(FLinearColor(FColor::FromHex(Hex)), Rough); }
	FColor BeCL(const FLinearColor& L, float Rough) { return FWRToolMesh::Pack(L, Rough); }
	/** A smooth hash noise in 0..1 (value noise over a 2D lattice, seeded). */
	float BeHash(int32 X, int32 Y, int32 S) { return FMath::Frac(FMath::Sin(float(X) * 127.1f + float(Y) * 311.7f + float(S) * 74.7f) * 43758.5453f); }
	float BeNoise(float X, float Y, int32 S)
	{
		const int32 X0 = FMath::FloorToInt(X), Y0 = FMath::FloorToInt(Y);
		const float Fx = FMath::SmoothStep(0.f, 1.f, X - X0), Fy = FMath::SmoothStep(0.f, 1.f, Y - Y0);
		const float A = FMath::Lerp(BeHash(X0, Y0, S), BeHash(X0 + 1, Y0, S), Fx);
		const float B = FMath::Lerp(BeHash(X0, Y0 + 1, S), BeHash(X0 + 1, Y0 + 1, S), Fx);
		return FMath::Lerp(A, B, Fy);
	}
	float BeTn(const TCHAR* Key, float Fallback) { return FWRTuning::Get().Num(FString::Printf(TEXT("skyLife.beast.%s"), Key), Fallback); }

	// Ray parts.
	enum : uint8 { RBody = 0, RLobeL = 1, RLobeR = 2, RTail = 3, RBarb = 4, RRim = 5 };
	// Whale parts.
	enum : uint8 { WHull = 0, WJaw = 1, WFinL = 2, WFinR = 3, WFluke = 4, WThroat = 5, WBarnacle = 6 };
	// Bell parts.
	enum : uint8 { BFloat = 0, BCrest = 1, BStrand = 2, BBead = 3 };
	// Serpent parts: the body ring (A = 0 neck .. 1 tail), the head (skull, mask), the jaw, the throat sac, wings, plumes,
	// the tail fan, the eyes.
	enum : uint8 { SBody = 0, SHead = 1, SJaw = 2, SSac = 3, SWingL = 4, SWingR = 5, SPlume = 6, SFan = 7, SEye = 8 };
	/** Its girth (radius cm) at t (0 the neck behind the head .. 1 the tail's tip): thickest at the chest (2.2 m across). */
	float BeSerpentR(float T) { return T < 0.12f ? FMath::Lerp(85.f, 110.f, T / 0.12f) : T < 0.25f ? 110.f : FMath::Lerp(110.f, 14.f, FMath::Pow((T - 0.25f) / 0.75f, 0.9f)); }

	// The ray's outline: its half-width at t (0 the tail root .. 1 the nose), and where its back sweeps.
	constexpr float RayNose = 230.f, RayRear = -200.f, RaySpan = 450.f;
	float BeRayHalf(float T) { return RaySpan * FMath::Pow(FMath::Max(0.f, FMath::Sin(PI * FMath::Pow(T, 1.15f))), 1.2f) + 55.f * FMath::Pow(T, 4.f) + 22.f * FMath::Pow(1.f - T, 6.f); }
	float BeRayThick(float T) { return 72.f * FMath::Pow(FMath::Max(0.f, FMath::Sin(PI * FMath::Clamp(T * 0.95f + 0.03f, 0.f, 1.f))), 0.7f); }

	// The whale's girth at t (0 the fluke's root .. 1 the snout).
	constexpr float WhaleTail = -350.f, WhaleNose = 350.f;
	float BeWhaleR(float T)
	{
		const float Body = FMath::Pow(FMath::Max(0.f, FMath::Sin(PI * FMath::Clamp(T / 1.06f + 0.02f, 0.f, 1.f))), 0.6f);
		const float Snout = T > 0.93f ? FMath::Lerp(1.f, 0.62f, (T - 0.93f) / 0.07f) : 1.f;
		return 16.f + 120.f * Body * Snout;
	}
	constexpr float JawHinge = 0.70f;
}
using namespace WRSkyBeastImpl;

UWRSkyBeast::UWRSkyBeast(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	bUseAsyncCooking = true;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetCastShadow(true);
}

int32 UWRSkyBeast::Add(FSection& S, const FVector& P, uint8 Part, float A, float B, const FColor& Col)
{
	FV V;
	V.Rest = P;
	V.Part = Part;
	V.A = A;
	V.B = B;
	S.Rest.Add(V);
	S.Colors.Add(Col);
	S.UVs.Add(FVector2D(A, B));
	return S.Rest.Num() - 1;
}

int32 UWRSkyBeast::Quad(FSection& S, int32 A, int32 B, int32 Cc, int32 D)
{
	// A B / D BeC (counter-clockwise seen from outside).
	S.Tris.Append({ A, B, Cc, A, Cc, D });
	return A;
}

void UWRSkyBeast::Tube(FSection& S, const TArray<FVector>& Points, const TArray<float>& Radii, int32 Sides, uint8 Part, const FColor& Col, float AStart, float AStep)
{
	const int32 Base = S.Rest.Num();
	for (int32 i = 0; i < Points.Num(); ++i)
	{
		const FVector Dir = (Points[FMath::Min(i + 1, Points.Num() - 1)] - Points[FMath::Max(i - 1, 0)]).GetSafeNormal();
		const FVector Side = FVector::CrossProduct(Dir, FMath::Abs(Dir.Z) > 0.9f ? FVector::XAxisVector : FVector::UpVector).GetSafeNormal();
		const FVector Up = FVector::CrossProduct(Side, Dir);
		for (int32 k = 0; k < Sides; ++k)
		{
			const float Ang = 2.f * PI * k / Sides;
			Add(S, Points[i] + (Side * FMath::Cos(Ang) + Up * FMath::Sin(Ang)) * Radii[i], Part, AStart + AStep * i, float(k) / Sides, Col);
		}
	}
	for (int32 i = 0; i + 1 < Points.Num(); ++i)
	{
		for (int32 k = 0; k < Sides; ++k)
		{
			const int32 A = Base + i * Sides + k, B = Base + i * Sides + (k + 1) % Sides;
			Quad(S, A, A + Sides, B + Sides, B);
		}
	}
}

void UWRSkyBeast::Init(AWRCreature* InOwner, FName InKind, float InScale, int32 InSeed, bool bScarred)
{
	Creature = InOwner;
	Kind = InKind;
	Scale = FMath::Max(0.1f, InScale);
	Seed = InSeed;
	bScar = bScarred;
	Body = FSection();
	Glow = FSection();
	if (Kind == TEXT("ray")) BuildRay();
	else if (Kind == TEXT("whale")) BuildWhale();
	else if (Kind == TEXT("bell")) BuildBell();
	else if (Kind == TEXT("serpent"))
	{
		// Drawn in the world's frame: the body lies along the path the head flew, not along the actor.
		SetUsingAbsoluteLocation(true);
		SetUsingAbsoluteRotation(true);
		SetUsingAbsoluteScale(true);
		SetWorldTransform(FTransform::Identity);
		if (InOwner) { Path.Reset(); Path.Add(InOwner->GetActorLocation()); }
		BuildSerpent();
	}
	SetMaterial(0, FWRToolMesh::Material());
	if (UMaterialInterface* G = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Creatures/M_Glow.M_Glow")))
	{
		GlowMat = UMaterialInstanceDynamic::Create(G, this);
		const FLinearColor Col = Kind == TEXT("whale") ? FLinearColor(FColor(127, 230, 255)) : Kind == TEXT("serpent") ? FLinearColor(FColor(0xB2, 0x3A, 0x2A)) : FLinearColor(FColor(150, 100, 255));
		GlowMat->SetVectorParameterValue(TEXT("Color"), Col);
		GlowMat->SetScalarParameterValue(TEXT("Glow"), 0.4f);
		SetMaterial(1, GlowMat);
	}
	if (InOwner) LastYaw = InOwner->GetActorRotation().Yaw;
	Pose(0.f);
	Commit(true);
	bBuilt = true;
}

// ================================================================================================ the Skyray

void UWRSkyBeast::BuildRay()
{
	// A manta diamond seen from above: nose +X; the wings swept back at their tips; thick over the spine, knife-thin at
	// the rim. Top and bottom are two grids that meet at the rim.
	const int32 NU = 28, NV = 35;
	const FColor Back = BeC(TEXT("2C3034"), 0.55f), Fleck = BeC(TEXT("A9AEB0"), 0.6f), Belly = BeC(TEXT("E4E4E0"), 0.5f);
	const FColor Blot = BeC(TEXT("1B1B1B"), 0.5f), Mouth = BeC(TEXT("121212"), 0.8f), Eye = BeC(TEXT("0A0A0A"), 0.2f), Gill = BeC(TEXT("6B6F72"), 0.6f);
	const auto At = [&](float T, float V, bool bTop) -> FVector
	{
		const float X = FMath::Lerp(RayRear, RayNose, T) - 120.f * V * V;
		const float W = BeRayHalf(T);
		const float Th = BeRayThick(T) * FMath::Pow(FMath::Max(0.f, 1.f - V * V), 0.9f);
		return FVector(X, V * W, bTop ? Th * 0.6f : -Th * 0.4f);
	};
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const bool bTop = Side == 0;
		const int32 Base = Body.Rest.Num();
		for (int32 i = 0; i < NU; ++i)
		{
			const float T = float(i) / (NU - 1);
			for (int32 j = 0; j < NV; ++j)
			{
				const float V = -1.f + 2.f * j / (NV - 1);
				FColor Col;
				if (bTop)
				{
					// Pale flecks scattered over the charcoal, denser on the shoulders; the eyes at the head's sides.
					const float N = BeNoise(T * 22.f, V * 26.f, Seed);
					Col = N > 0.83f ? Fleck : Back;
					if (T > 0.9f && FMath::Abs(FMath::Abs(V) - 0.32f) < 0.06f && T < 0.96f) Col = Eye;
				}
				else
				{
					// The belly: white with black blotches unique to this ray (its seed), a mouth slot across the front,
					// five gill slits each side.
					const float N = BeNoise(T * 9.f + 3.1f, V * 11.f, Seed + 91);
					Col = (N > 0.72f && T > 0.28f && T < 0.88f && FMath::Abs(V) < 0.55f) ? Blot : Belly;
					if (T > 0.95f && FMath::Abs(V) < 0.42f) Col = Mouth;
					for (int32 g = 0; g < 5; ++g) if (FMath::Abs(T - (0.70f + g * 0.03f)) < 0.006f && FMath::Abs(V) > 0.26f && FMath::Abs(V) < 0.4f) Col = Gill;
				}
				Add(Body, At(T, V, bTop), RBody, T, V, Col);
			}
		}
		for (int32 i = 0; i + 1 < NU; ++i)
		{
			for (int32 j = 0; j + 1 < NV; ++j)
			{
				const int32 A = Base + i * NV + j, B = A + 1, D = A + NV, Cc = D + 1;
				if (bTop) Quad(Body, A, D, Cc, B); else Quad(Body, A, B, Cc, D);
			}
		}
	}
	// The violet rim (night glow): a thin strip just inside the wing edge, both sides.
	{
		for (const float S : { -1.f, 1.f })
		{
			const int32 Base = Glow.Rest.Num();
			const int32 N = 26;
			for (int32 i = 0; i < N; ++i)
			{
				const float T = 0.08f + 0.86f * i / (N - 1);
				const FVector Outer = At(T, S * 0.995f, true) + FVector(0, 0, 0.6f);
				const FVector Inner = At(T, S * 0.955f, true) + FVector(0, 0, 0.8f);
				Add(Glow, Outer, RRim, T, S * 0.995f, FColor::White);
				Add(Glow, Inner, RRim, T, S * 0.955f, FColor::White);
			}
			for (int32 i = 0; i + 1 < N; ++i)
			{
				const int32 A = Base + i * 2, B = A + 1, Cc = A + 3, D = A + 2;
				if (S > 0.f) Quad(Glow, A, D, Cc, B); else Quad(Glow, A, B, Cc, D);
				// Seen from below too.
				if (S > 0.f) Quad(Glow, A, B, Cc, D); else Quad(Glow, A, D, Cc, B);
			}
		}
	}
	// The horn-lobes: two flaps off the head's front corners, rolled up into spirals (unrolled before the Undertow).
	for (int32 L = 0; L < 2; ++L)
	{
		const float S = L == 0 ? -1.f : 1.f;
		const FVector Root(RayNose - 14.f, S * 50.f, 2.f);
		const int32 Base = Body.Rest.Num();
		const int32 NL = 10, NW = 3;
		for (int32 i = 0; i < NL; ++i)
		{
			for (int32 j = 0; j < NW; ++j)
			{
				const float A = float(i) / (NL - 1);
				const float W = (float(j) / (NW - 1) - 0.5f) * 30.f * (1.f - 0.4f * A);
				Add(Body, Root + FVector(A * 110.f, S * (W + 4.f), 0.f), L == 0 ? RLobeL : RLobeR, A, W, i == NL - 1 ? BeC(TEXT("3A3E42"), 0.55f) : Back);
			}
		}
		for (int32 i = 0; i + 1 < NL; ++i)
		{
			for (int32 j = 0; j + 1 < NW; ++j)
			{
				const int32 A = Base + i * NW + j, B = A + 1, D = A + NW, Cc = D + 1;
				Quad(Body, A, D, Cc, B);
				Quad(Body, A, B, Cc, D);   // two-sided
			}
		}
	}
	// The whip tail with its barb.
	{
		TArray<FVector> P;
		TArray<float> R;
		const int32 N = 16;
		for (int32 i = 0; i < N; ++i)
		{
			const float S = float(i) / (N - 1);
			P.Add(FVector(RayRear + 6.f - S * 480.f, 0.f, 2.f - S * 10.f));
			R.Add(FMath::Lerp(13.f, 2.5f, FMath::Pow(S, 0.7f)));
		}
		Tube(Body, P, R, 6, RTail, Back, 0.f, 1.f / (N - 1));
		// The barb: a serrated spine on top at a third of the way, pointing back.
		const int32 B0 = Body.Rest.Num();
		const FVector Bb(RayRear - 150.f, 0.f, 8.f);
		const float S0 = 150.f / 480.f;
		const FColor BarbC = BeC(TEXT("5C5149"), 0.35f);
		Add(Body, Bb + FVector(0, -5, 0), RBarb, S0, 0, BarbC);
		Add(Body, Bb + FVector(0, 5, 0), RBarb, S0, 0, BarbC);
		Add(Body, Bb + FVector(-46.f, 0, 12.f), RBarb, S0 + 0.1f, 0, BarbC);
		Add(Body, Bb + FVector(8.f, 0, 4.f), RBarb, S0, 0, BarbC);
		Body.Tris.Append({ B0, B0 + 2, B0 + 1, B0, B0 + 3, B0 + 2, B0 + 1, B0 + 2, B0 + 3 });
	}
}

// ================================================================================================ the Cloudback

void UWRSkyBeast::BuildWhale()
{
	const int32 NS = 30, Sides = 18;
	const FColor Top = BeC(bScar ? TEXT("566676") : TEXT("5F7486"), 0.55f), Flank = BeC(bScar ? TEXT("6E7C88") : TEXT("7C8E9C"), 0.55f);
	const FColor Groove = BeC(TEXT("E6E6E2"), 0.5f), GrooveDark = BeC(TEXT("B3B9BE"), 0.55f), Scar = BeC(TEXT("C9CDD0"), 0.6f), Dark = BeC(TEXT("1A1D20"), 0.4f);
	const FColor Lining = BeC(TEXT("5A1E24"), 0.6f);
	const int32 Base = Body.Rest.Num();
	for (int32 i = 0; i < NS; ++i)
	{
		const float T = float(i) / (NS - 1);
		const float X = FMath::Lerp(WhaleTail, WhaleNose, T);
		const float R = BeWhaleR(T);
		// The hump: the back rises over its middle where the levistone barnacles crust it (it floats on them).
		const float Hump = 34.f * FMath::Exp(-FMath::Square((T - 0.52f) / 0.09f));
		for (int32 k = 0; k < Sides; ++k)
		{
			const float Ang = 2.f * PI * k / Sides;
			const float Cy = FMath::Cos(Ang), Sz = FMath::Sin(Ang);
			const float Zr = Sz < 0.f ? R * 0.82f : R * 0.92f;
			FVector P(X, Cy * R, Sz * Zr + (Sz > 0.3f ? Hump * Sz : 0.f));
			FColor Col = Sz > 0.15f ? Top : Flank;
			// Throat grooves: long pale pleats from the chin back to the navel, alternating light and dark.
			if (Sz < -0.45f && T > 0.5f && T < 0.99f) Col = (k % 2) ? Groove : GrooveDark;
			// Old Gulper's harpoon scars: pale lines across its flanks.
			if (bScar && BeNoise(T * 7.f, Ang * 2.f, Seed) > 0.8f && FMath::Abs(Sz) < 0.6f) Col = Scar;
			// The eye and the blowhole.
			if (FMath::Abs(T - 0.86f) < 0.012f && FMath::Abs(Sz + 0.05f) < 0.1f && FMath::Abs(Cy) > 0.9f) Col = Dark;
			if (FMath::Abs(T - 0.8f) < 0.01f && Sz > 0.96f) Col = Dark;
			// The lower jaw: front, below the mouth line.
			const bool bJaw = T > JawHinge && Sz < -0.12f;
			Add(Body, P, bJaw ? WJaw : WHull, T, Ang, Col);
		}
	}
	for (int32 i = 0; i + 1 < NS; ++i)
	{
		for (int32 k = 0; k < Sides; ++k)
		{
			const int32 A = Base + i * Sides + k, B = Base + i * Sides + (k + 1) % Sides;
			Quad(Body, A, B, B + Sides, A + Sides);
		}
	}
	// Close the snout (a fan to its tip) and the peduncle.
	{
		const int32 Tip = Add(Body, FVector(WhaleNose + 8.f, 0.f, -4.f), WHull, 1.f, 0.f, Top);
		const int32 Last = Base + (NS - 1) * Sides;
		for (int32 k = 0; k < Sides; ++k) Body.Tris.Append({ Last + k, Last + (k + 1) % Sides, Tip });
		const int32 Tail = Add(Body, FVector(WhaleTail - 6.f, 0.f, 0.f), WHull, 0.f, 0.f, Flank);
		for (int32 k = 0; k < Sides; ++k) Body.Tris.Append({ Base + (k + 1) % Sides, Base + k, Tail });
	}
	// The throat's lining (seen when the jaw drops): a dark red sheet across the mouth inside.
	{
		const int32 B0 = Body.Rest.Num();
		const float X0 = FMath::Lerp(WhaleTail, WhaleNose, JawHinge), X1 = WhaleNose - 10.f;
		const float Rr = BeWhaleR(0.85f) * 0.75f;
		for (int32 i = 0; i < 6; ++i)
		{
			const float S = float(i) / 5.f;
			for (int32 j = 0; j < 5; ++j)
			{
				const float Y = (float(j) / 4.f - 0.5f) * 2.f * Rr * (1.f - 0.5f * S);
				Add(Body, FVector(FMath::Lerp(X0, X1, S), Y, -BeWhaleR(0.85f) * 0.15f), WThroat, FMath::Lerp(JawHinge, 1.f, S), Y, Lining);
			}
		}
		for (int32 i = 0; i < 5; ++i) for (int32 j = 0; j < 4; ++j)
		{
			const int32 A = B0 + i * 5 + j, B = A + 1, D = A + 5, Cc = D + 1;
			Quad(Body, A, B, Cc, D);
			Quad(Body, A, D, Cc, B);
		}
	}
	// Stubby pectoral fins and the fluke.
	for (int32 F = 0; F < 3; ++F)
	{
		const bool bFluke = F == 2;
		const float S = F == 0 ? -1.f : 1.f;
		const int32 B0 = Body.Rest.Num();
		const int32 NL = 6, NW = 4;
		for (int32 i = 0; i < NL; ++i)
		{
			for (int32 j = 0; j < NW; ++j)
			{
				const float A = float(i) / (NL - 1), W = float(j) / (NW - 1);
				FVector P;
				if (bFluke)
				{
					// Two lobes swept back from the peduncle, notched in the middle: A runs out from the centre line.
					const float Y = (W - 0.5f) * 2.f * 150.f;
					const float Chord = 70.f * (1.f - 0.6f * FMath::Abs(W - 0.5f) * 2.f);
					P = FVector(WhaleTail - 10.f - A * Chord - FMath::Abs(Y) * 0.28f + (FMath::Abs(Y) < 12.f ? 12.f : 0.f), Y, 0.f);
				}
				else
				{
					const float Root = FMath::Lerp(WhaleTail, WhaleNose, 0.62f);
					P = FVector(Root - A * 40.f - W * 30.f, S * (BeWhaleR(0.62f) * 0.9f + A * 90.f), -BeWhaleR(0.62f) * 0.45f - A * 22.f);
				}
				Add(Body, P, bFluke ? WFluke : (S < 0.f ? WFinL : WFinR), A, W, Flank);
			}
		}
		for (int32 i = 0; i + 1 < NL; ++i) for (int32 j = 0; j + 1 < NW; ++j)
		{
			const int32 A = B0 + i * NW + j, B = A + 1, D = A + NW, Cc = D + 1;
			Quad(Body, A, B, Cc, D);
			Quad(Body, A, D, Cc, B);
		}
	}
	// The barnacles: a reef of levistone cones on the hump (Old Gulper's is a reef), glowing cyan.
	{
		const int32 N = bScar ? 70 : 38;
		for (int32 b = 0; b < N; ++b)
		{
			const float T = 0.52f + (BeHash(b, 1, Seed) - 0.5f) * 0.26f;
			const float Ang = PI * 0.5f + (BeHash(b, 2, Seed) - 0.5f) * 1.6f;
			const float R = BeWhaleR(T);
			const float Hump = 34.f * FMath::Exp(-FMath::Square((T - 0.52f) / 0.09f));
			const FVector Foot(FMath::Lerp(WhaleTail, WhaleNose, T), FMath::Cos(Ang) * R, FMath::Sin(Ang) * (R * 0.92f + Hump));
			const FVector Out = FVector(0.f, FMath::Cos(Ang), FMath::Sin(Ang)).GetSafeNormal();
			const float H = 8.f + 14.f * BeHash(b, 3, Seed), Wd = 4.f + 5.f * BeHash(b, 4, Seed);
			const FVector Side = FVector::CrossProduct(Out, FVector::XAxisVector).GetSafeNormal();
			const FVector Fwd = FVector::CrossProduct(Side, Out);
			const int32 B0 = Glow.Rest.Num();
			Add(Glow, Foot + Side * Wd, WBarnacle, T, Ang, FColor::White);
			Add(Glow, Foot - Side * Wd * 0.5f + Fwd * Wd * 0.87f, WBarnacle, T, Ang, FColor::White);
			Add(Glow, Foot - Side * Wd * 0.5f - Fwd * Wd * 0.87f, WBarnacle, T, Ang, FColor::White);
			Add(Glow, Foot + Out * H, WBarnacle, T, Ang, FColor::White);
			Glow.Tris.Append({ B0, B0 + 1, B0 + 3, B0 + 1, B0 + 2, B0 + 3, B0 + 2, B0, B0 + 3 });
		}
	}
}

// ================================================================================================ the Sailbell

void UWRSkyBeast::BuildBell()
{
	// The float: a gas bell 2 m long, longer than tall, flattened below; the crest-sail along its top.
	const int32 NS = 14, Sides = 14;
	const FColor Float = BeC(TEXT("9FD3F0"), 0.12f), FloatDeep = BeC(TEXT("6FB0DA"), 0.15f), Crest = BeC(TEXT("8E5BD8"), 0.3f), CrestEdge = BeC(TEXT("C9A6FF"), 0.3f);
	const int32 Base = Body.Rest.Num();
	for (int32 i = 0; i < NS; ++i)
	{
		const float T = float(i) / (NS - 1);
		const float X = FMath::Lerp(-100.f, 100.f, T);
		const float R = FMath::Sqrt(FMath::Max(0.f, 1.f - FMath::Square(T * 2.f - 1.f)));
		for (int32 k = 0; k < Sides; ++k)
		{
			const float Ang = 2.f * PI * k / Sides;
			const float Sz = FMath::Sin(Ang);
			Add(Body, FVector(X, FMath::Cos(Ang) * 48.f * R, Sz * (Sz > 0.f ? 58.f : 30.f) * R), BFloat, T, Ang, Sz < -0.3f ? FloatDeep : Float);
		}
	}
	for (int32 i = 0; i + 1 < NS; ++i) for (int32 k = 0; k < Sides; ++k)
	{
		const int32 A = Base + i * Sides + k, B = Base + i * Sides + (k + 1) % Sides;
		Quad(Body, A, B, B + Sides, A + Sides);
	}
	// The crest-sail: a scalloped fin standing on the float's top, set at an angle (its handedness).
	{
		const int32 B0 = Body.Rest.Num();
		const int32 N = 12;
		for (int32 i = 0; i < N; ++i)
		{
			const float T = 0.12f + 0.76f * i / (N - 1);
			const float X = FMath::Lerp(-100.f, 100.f, T);
			const float Topz = 58.f * FMath::Sqrt(FMath::Max(0.f, 1.f - FMath::Square(T * 2.f - 1.f)));
			const float H = 70.f * FMath::Sin(PI * (T - 0.12f) / 0.76f) * (0.82f + 0.18f * FMath::Abs(FMath::Sin(i * 1.7f)));
			Add(Body, FVector(X, 0.f, Topz - 4.f), BCrest, T, 0.f, Crest);
			Add(Body, FVector(X, 0.f, Topz + H), BCrest, T, 1.f, CrestEdge);
		}
		for (int32 i = 0; i + 1 < N; ++i)
		{
			const int32 A = B0 + i * 2, B = A + 1, D = A + 2, Cc = A + 3;
			Quad(Body, A, B, Cc, D);
			Quad(Body, A, D, Cc, B);
		}
	}
	// The curtain: fourteen strands hanging from an oval under the float; each joint a ring of four (the strand's tube),
	// a bead every third joint (glow).
	StrandRoot.Reset();
	StrandPhase.Reset();
	const int32 NStr = 14;
	const FColor Ten = BeC(TEXT("D9D3E6"), 0.4f);
	for (int32 s = 0; s < NStr; ++s)
	{
		const float Ang = 2.f * PI * s / NStr + 0.2f * BeHash(s, 5, Seed);
		const FVector Root(FMath::Cos(Ang) * 70.f, FMath::Sin(Ang) * 30.f, -26.f);
		StrandRoot.Add(Root);
		StrandPhase.Add(BeHash(s, 7, Seed) * 6.28f);
		const int32 B0 = Body.Rest.Num();
		for (int32 k = 0; k <= StrandJointsN; ++k)
		{
			const float R = FMath::Lerp(3.2f, 1.2f, float(k) / StrandJointsN);
			for (int32 q = 0; q < 4; ++q)
			{
				const float A = PI * 0.5f * q;
				Add(Body, FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 0.f), BStrand, float(s), float(k), Ten);
			}
		}
		for (int32 k = 0; k < StrandJointsN; ++k) for (int32 q = 0; q < 4; ++q)
		{
			const int32 A = B0 + k * 4 + q, B = B0 + k * 4 + (q + 1) % 4;
			Quad(Body, A, B, B + 4, A + 4);
		}
		// Beads (the stinging batteries, violet at night): an octahedron every third joint.
		for (int32 k = 3; k <= StrandJointsN; k += 3)
		{
			const float Bs = 5.f + 2.f * BeHash(s, k, Seed);
			const int32 G0 = Glow.Rest.Num();
			const FVector O[6] = { FVector(Bs, 0, 0), FVector(-Bs, 0, 0), FVector(0, Bs, 0), FVector(0, -Bs, 0), FVector(0, 0, Bs * 1.3f), FVector(0, 0, -Bs * 1.3f) };
			for (int32 o = 0; o < 6; ++o) Add(Glow, O[o], BBead, float(s), float(k), FColor::White);
			const int32 F[8][3] = { {0,2,4},{2,1,4},{1,3,4},{3,0,4},{2,0,5},{1,2,5},{3,1,5},{0,3,5} };
			for (const auto& Tri : F) Glow.Tris.Append({ G0 + Tri[0], G0 + Tri[1], G0 + Tri[2] });
		}
	}
}

// ================================================================================================ Aeolith

void UWRSkyBeast::BuildSerpent()
{
	const FColor Jade = BeC(TEXT("2F8F7A"), 0.45f), JadeDeep = BeC(TEXT("23705F"), 0.45f), Ivory = BeC(TEXT("E9E4D8"), 0.5f), Bone = BeC(TEXT("D8D2C4"), 0.7f);
	const FColor Gold = BeC(TEXT("C9A04A"), 0.35f), Feather = BeC(TEXT("EDE8DC"), 0.6f), Dark = BeC(TEXT("101010"), 0.3f), Amber = BeC(TEXT("E0A030"), 0.15f), Mouth = BeC(TEXT("5A1A1E"), 0.5f);
	// The body: rings along the spine, jade above fading to the ivory belly, a row of darker scales down the back.
	const int32 Sides = 12;
	for (int32 i = 0; i < SerpentSegs; ++i)
	{
		const float T = float(i) / (SerpentSegs - 1);
		const float R = BeSerpentR(T);
		for (int32 k = 0; k < Sides; ++k)
		{
			const float Ang = 2.f * PI * k / Sides;
			const float Sz = FMath::Sin(Ang);
			const FColor Col = Sz < -0.35f ? Ivory : (Sz > 0.9f && i % 2 == 0) ? JadeDeep : Jade;
			Add(Body, FVector(0.f, FMath::Cos(Ang) * R, Sz * R * 0.92f), SBody, T, Ang, Col);
		}
	}
	for (int32 i = 0; i + 1 < SerpentSegs; ++i) for (int32 k = 0; k < Sides; ++k)
	{
		const int32 A = i * Sides + k, B = i * Sides + (k + 1) % Sides;
		Quad(Body, A, B, B + Sides, A + Sides);
	}
	// The head (its own frame: the actor's): a long skull, the bone wind-mask over the snout (a duck-billed plate with
	// two flared nostril vents), amber eyes with slit pupils, the jaw (hinged at the back, opens on the Gape).
	{
		const int32 NS = 12, NR = 10;
		const int32 B0 = Body.Rest.Num();
		for (int32 i = 0; i < NS; ++i)
		{
			const float S = float(i) / (NS - 1);
			const float X = FMath::Lerp(-160.f, 340.f, S);
			const float W = FMath::Lerp(95.f, 40.f, FMath::Pow(S, 1.3f)) * (S < 0.15f ? FMath::Lerp(0.9f, 1.f, S / 0.15f) : 1.f);
			const float Hh = FMath::Lerp(85.f, 28.f, FMath::Pow(S, 1.1f));
			for (int32 k = 0; k < NR; ++k)
			{
				const float Ang = 2.f * PI * k / NR;
				const float Sz = FMath::Sin(Ang);
				const bool bJaw = Sz < -0.2f && S > 0.12f;
				const FColor Col = Sz > 0.2f && S > 0.62f ? Bone : (Sz < -0.35f ? Ivory : Jade);
				Add(Body, FVector(X, FMath::Cos(Ang) * W, Sz * Hh + (Sz > 0.f ? 10.f : 0.f)), bJaw ? SJaw : SHead, S, Ang, Col);
			}
		}
		for (int32 i = 0; i + 1 < NS; ++i) for (int32 k = 0; k < NR; ++k)
		{
			const int32 A = B0 + i * NR + k, B = B0 + i * NR + (k + 1) % NR;
			Quad(Body, A, B, B + NR, A + NR);
		}
		// The mouth's lining (seen open) and a row of hooked needle teeth along the jaw's edge.
		const int32 M0 = Body.Rest.Num();
		for (int32 i = 0; i < 6; ++i) for (int32 j = 0; j < 3; ++j)
		{
			const float S = float(i) / 5.f;
			Add(Body, FVector(FMath::Lerp(-40.f, 320.f, S), (j - 1) * FMath::Lerp(70.f, 30.f, S), -6.f), SJaw, 0.2f + S * 0.8f, float(j), Mouth);
		}
		for (int32 i = 0; i < 5; ++i) for (int32 j = 0; j < 2; ++j)
		{
			const int32 A = M0 + i * 3 + j, B = A + 1, D = A + 3, Cc = D + 1;
			Quad(Body, A, B, Cc, D);
			Quad(Body, A, D, Cc, B);
		}
		for (int32 t = 0; t < 14; ++t)
		{
			const float X = FMath::Lerp(0.f, 300.f, t / 13.f);
			const float Yy = (t % 2 ? 1.f : -1.f) * FMath::Lerp(62.f, 30.f, t / 13.f);
			const int32 T0 = Body.Rest.Num();
			Add(Body, FVector(X, Yy, -6.f), SJaw, 0.5f, 0.f, Feather);
			Add(Body, FVector(X + 10.f, Yy, -6.f), SJaw, 0.5f, 0.f, Feather);
			Add(Body, FVector(X - 4.f, Yy * 0.95f, 16.f), SJaw, 0.5f, 0.f, Feather);
			Body.Tris.Append({ T0, T0 + 1, T0 + 2, T0, T0 + 2, T0 + 1 });
		}
		// The nostril vents on the mask and the eyes.
		for (const float Sd : { -1.f, 1.f })
		{
			const int32 V0 = Body.Rest.Num();
			const FVector N0(300.f, Sd * 22.f, 34.f);
			Add(Body, N0, SHead, 1.f, 0.f, Dark);
			Add(Body, N0 + FVector(30.f, Sd * 10.f, 6.f), SHead, 1.f, 0.f, Dark);
			Add(Body, N0 + FVector(10.f, Sd * 16.f, 18.f), SHead, 1.f, 0.f, Bone);
			Body.Tris.Append({ V0, V0 + 1, V0 + 2, V0, V0 + 2, V0 + 1 });
			const int32 E0 = Body.Rest.Num();
			const FVector Ey(150.f, Sd * 66.f, 46.f);
			Add(Body, Ey + FVector(-18.f, 0, -10.f), SEye, 0.f, 0.f, Amber);
			Add(Body, Ey + FVector(18.f, 0, -10.f), SEye, 0.f, 0.f, Amber);
			Add(Body, Ey + FVector(18.f, Sd * 4.f, 10.f), SEye, 0.f, 0.f, Amber);
			Add(Body, Ey + FVector(-18.f, Sd * 4.f, 10.f), SEye, 0.f, 0.f, Amber);
			Add(Body, Ey + FVector(-2.f, Sd * 6.f, -12.f), SEye, 0.f, 0.f, Dark);
			Add(Body, Ey + FVector(2.f, Sd * 6.f, -12.f), SEye, 0.f, 0.f, Dark);
			Add(Body, Ey + FVector(2.f, Sd * 6.f, 12.f), SEye, 0.f, 0.f, Dark);
			Add(Body, Ey + FVector(-2.f, Sd * 6.f, 12.f), SEye, 0.f, 0.f, Dark);
			Quad(Body, E0, E0 + 1, E0 + 2, E0 + 3); Quad(Body, E0, E0 + 3, E0 + 2, E0 + 1);
			Quad(Body, E0 + 4, E0 + 5, E0 + 6, E0 + 7); Quad(Body, E0 + 4, E0 + 7, E0 + 6, E0 + 5);
		}
	}
	// The throat sac (glow: dull red, brighter at night): a pouch under the throat that swells to twice the head.
	{
		const int32 NS = 8, NR = 10;
		const int32 B0 = Glow.Rest.Num();
		for (int32 i = 0; i < NS; ++i)
		{
			const float S = float(i) / (NS - 1);
			const float R = 70.f * FMath::Sin(PI * S);
			for (int32 k = 0; k < NR; ++k)
			{
				const float Ang = 2.f * PI * k / NR;
				Add(Glow, FVector(FMath::Lerp(-120.f, 120.f, S), FMath::Cos(Ang) * R, FMath::Sin(Ang) * R * 0.8f), SSac, S, Ang, FColor::White);
			}
		}
		for (int32 i = 0; i + 1 < NS; ++i) for (int32 k = 0; k < NR; ++k)
		{
			const int32 A = B0 + i * NR + k, B = B0 + i * NR + (k + 1) % NR;
			Quad(Glow, A, B, B + NR, A + NR);
		}
	}
	// The wings: one pair 34 m across set just behind the head, ivory feathers tipped jade and gold, barred.
	for (int32 Wg = 0; Wg < 2; ++Wg)
	{
		const float Sd = Wg == 0 ? -1.f : 1.f;
		const int32 NL = 10, NW = 6;
		const int32 B0 = Body.Rest.Num();
		for (int32 i = 0; i < NL; ++i)
		{
			const float U = float(i) / (NL - 1);   // root .. tip
			const float Span = 1700.f * U;
			const float Chord = FMath::Lerp(650.f, 260.f, FMath::Pow(U, 1.4f));
			for (int32 j = 0; j < NW; ++j)
			{
				const float V = float(j) / (NW - 1);   // leading .. trailing edge
				const FColor Col = V > 0.75f ? ((i % 2) ? Gold : BeC(TEXT("5FA890"), 0.5f)) : (V > 0.45f && (i + j) % 2 ? BeC(TEXT("CFC8B8"), 0.6f) : Feather);
				Add(Body, FVector(-V * Chord - U * 220.f, Sd * (90.f + Span), 40.f - 30.f * V), Wg == 0 ? SWingL : SWingR, U, V, Col);
			}
		}
		for (int32 i = 0; i + 1 < NL; ++i) for (int32 j = 0; j + 1 < NW; ++j)
		{
			const int32 A = B0 + i * NW + j, B = A + 1, D = A + NW, Cc = D + 1;
			Quad(Body, A, B, Cc, D);
			Quad(Body, A, D, Cc, B);
		}
	}
	// Quetzal plumes along the spine (longest at the neck), and the 6 m tail fan.
	for (int32 p = 0; p < 22; ++p)
	{
		const float T = 0.02f + 0.9f * p / 21.f;
		const float L = FMath::Lerp(300.f, 70.f, FMath::Pow(T, 0.6f));
		const int32 B0 = Body.Rest.Num();
		const FColor Tip = (p % 2) ? Gold : BeC(TEXT("2F8F7A"), 0.5f);
		Add(Body, FVector(0.f, -10.f, 0.f), SPlume, T, 0.f, Feather);
		Add(Body, FVector(0.f, 10.f, 0.f), SPlume, T, 0.f, Feather);
		Add(Body, FVector(-L * 0.8f, 16.f, L * 0.55f), SPlume, T, 1.f, Tip);
		Add(Body, FVector(-L * 0.8f, -16.f, L * 0.55f), SPlume, T, 1.f, Tip);
		Quad(Body, B0, B0 + 1, B0 + 2, B0 + 3);
		Quad(Body, B0, B0 + 3, B0 + 2, B0 + 1);
	}
	for (int32 f = 0; f < 9; ++f)
	{
		const float Ang = FMath::DegreesToRadians(-50.f + 12.5f * f);
		const int32 B0 = Body.Rest.Num();
		const FVector Dir(-FMath::Cos(Ang), FMath::Sin(Ang), 0.f);
		const FVector Side(FMath::Sin(Ang), FMath::Cos(Ang), 0.f);
		for (int32 s = 0; s < 5; ++s)
		{
			const float L = 600.f * s / 4.f;
			const float W = 30.f + 30.f * FMath::Sin(PI * s / 4.f);
			const FColor Col = s == 4 ? Gold : (s == 3 ? BeC(TEXT("2F8F7A"), 0.5f) : Feather);
			Add(Body, Dir * L + Side * W, SFan, 1.f, float(f), Col);
			Add(Body, Dir * L - Side * W, SFan, 1.f, float(f), Col);
		}
		for (int32 s = 0; s < 4; ++s)
		{
			const int32 A = B0 + s * 2;
			Quad(Body, A, A + 1, A + 3, A + 2);
			Quad(Body, A, A + 2, A + 3, A + 1);
		}
	}
}

void UWRSkyBeast::RecordTrail()
{
	const AWRCreature* Cr = Creature.Get();
	if (!Cr) return;
	const FVector Head = Cr->GetActorLocation();
	if (Path.Num() == 0 || FVector::Dist(Path[0], Head) > 60.f) Path.Insert(Head, 0);
	// Keep a little more than the body's length of path.
	float L = 0.f;
	for (int32 i = 1; i < Path.Num(); ++i)
	{
		L += FVector::Dist(Path[i - 1], Path[i]);
		if (L > SerpentLength * Scale + 1500.f) { Path.SetNum(i + 1); break; }
	}
}

void UWRSkyBeast::SpineAt(float Dist, FVector& OutAt, FQuat& OutFrame) const
{
	// Behind the head along the flown path; where the path runs out (a fresh spawn), straight back from its end.
	const AWRCreature* Cr = Creature.Get();
	const FVector Head = Cr ? Cr->GetActorLocation() : FVector::ZeroVector;
	FVector Dir = Cr ? Cr->GetActorForwardVector() : FVector(1, 0, 0);
	FVector Prev = Head;
	float Walked = 0.f;
	FVector At = Head - Dir * Dist;
	bool bFound = false;
	for (int32 i = 0; i < Path.Num(); ++i)
	{
		const FVector P = Path[i];
		const float Seg = FVector::Dist(Prev, P);
		if (Seg > 0.1f)
		{
			Dir = (Prev - P).GetSafeNormal();
			if (Walked + Seg >= Dist)
			{
				At = FMath::Lerp(Prev, P, (Dist - Walked) / Seg);
				bFound = true;
				break;
			}
		}
		Walked += Seg;
		Prev = P;
	}
	if (!bFound) At = Prev - Dir * (Dist - Walked);
	// A slow travelling wave down the body (its coils breathe), and the barrel roll's twist.
	const float T = Dist / FMath::Max(1.f, SerpentLength * Scale);
	FVector Side = FVector::CrossProduct(FVector::UpVector, Dir).GetSafeNormal();
	if (Side.IsNearlyZero()) Side = FVector::RightVector;
	At += Side * FMath::Sin(2.f * PI * (0.35f * Time - 2.2f * T)) * 60.f * Scale * T;
	const FVector Up0 = FVector::CrossProduct(Dir, Side).GetSafeNormal();
	const FVector Up = FQuat(Dir, FMath::DegreesToRadians(Roll360 * (1.f - T * 0.5f))).RotateVector(Up0.IsNearlyZero() ? FVector::UpVector : Up0);
	OutAt = At;
	OutFrame = FRotationMatrix::MakeFromXZ(Dir.IsNearlyZero() ? FVector::XAxisVector : Dir, Up).ToQuat();
}

FVector UWRSkyBeast::PlaceSerpent(const FV& V) const
{
	const float L = SerpentLength * Scale;
	if (V.Part == SBody || V.Part == SPlume)
	{
		// Body rings and the plumes on them ride the spine; the neck starts just behind the skull.
		const float Dist = 140.f * Scale + V.A * (L - 140.f * Scale);
		FVector At; FQuat Fr;
		SpineAt(Dist, At, Fr);
		FVector Off = V.Rest * Scale;
		if (V.Part == SPlume)
		{
			// Standing up out of the top of the ring, swaying.
			const float R = BeSerpentR(V.A) * Scale;
			Off = FVector(V.Rest.X * Scale, V.Rest.Y * Scale + V.B * 20.f * Scale * FMath::Sin(Time * 2.f + V.A * 9.f), V.Rest.Z * Scale + R * 0.9f);
		}
		return At + Fr.RotateVector(Off);
	}
	if (V.Part == SFan)
	{
		FVector At; FQuat Fr;
		SpineAt(L, At, Fr);
		const FVector Off = FQuat(FVector::XAxisVector, FMath::DegreesToRadians(8.f * FMath::Sin(Time * 1.6f))).RotateVector(V.Rest * Scale);
		return At + Fr.RotateVector(Off);
	}
	if (V.Part == SWingL || V.Part == SWingR)
	{
		// The wings ride the frame 3 m behind the head; they beat (deeper climbing) and spread forward like a hood on a tell.
		FVector At; FQuat Fr;
		SpineAt(300.f * Scale, At, Fr);
		const float Sd = V.Part == SWingL ? -1.f : 1.f;
		const FVector Root(0.f, Sd * 90.f * Scale, 40.f * Scale);
		FVector Off = V.Rest * Scale - Root;
		const float Beat = (1.f - Hood) * WaveAmp * FMath::Sin(2.f * PI * Clock) * (0.4f + 0.6f * V.A);
		Off = FQuat(FVector::XAxisVector, FMath::DegreesToRadians(Sd * (Beat - 10.f + Hood * 25.f))).RotateVector(Off);
		Off = FQuat(FVector::UpVector, FMath::DegreesToRadians(-Sd * Hood * 55.f)).RotateVector(Off);
		return At + Fr.RotateVector(Root + Off);
	}
	// The head and its parts in the head's own frame (the actor's).
	FVector P = V.Rest;
	if (V.Part == SJaw)
	{
		const FVector Hinge(-60.f, 0.f, -10.f);
		P = Hinge + FQuat(FVector::YAxisVector, FMath::DegreesToRadians(Jaw * 70.f)).RotateVector(P - Hinge);
	}
	else if (V.Part == SSac)
	{
		// Under the throat, swelling to twice the head's size before a breath.
		P = FVector(-40.f, 0.f, -90.f) + V.Rest * (0.8f + 1.3f * Sac);
	}
	return HeadXf.TransformPosition(P * Scale);
}

// ================================================================================================ the pose

void UWRSkyBeast::StrandJoints(int32 Strand, TArray<FVector>& Out) const
{
	// A chain hanging from its root: each joint's direction a little behind the one above (sway travels down), trailing
	// opposite the drift, swept out radially in the Lash, lying out flat when deflated.
	Out.Reset();
	const FVector Root = StrandRoot.IsValidIndex(Strand) ? StrandRoot[Strand] : FVector::ZeroVector;
	const float Ph = StrandPhase.IsValidIndex(Strand) ? StrandPhase[Strand] : 0.f;
	const FVector Radial = FVector(Root.X, Root.Y, 0.f).GetSafeNormal();
	const float Seg = BeTn(TEXT("bell.curtain"), 1200.f) / StrandJointsN;
	FVector P = Root;
	Out.Add(P);
	for (int32 k = 1; k <= StrandJointsN; ++k)
	{
		const float F = float(k) / StrandJointsN;
		const float Sway = 0.2f * F * FMath::Sin(2.f * PI * 0.28f * Time - 0.35f * k + Ph);
		const float SwayB = 0.12f * F * FMath::Sin(2.f * PI * 0.19f * Time - 0.3f * k + Ph * 1.7f);
		FVector Dir(Sway, SwayB, -1.f);
		Dir += Trail * F;
		Dir += Radial * (Lash * (0.9f - 0.4f * F));
		Dir = Dir.GetSafeNormal();
		Dir = FMath::Lerp(Dir, (Radial + FVector(0.f, 0.f, -0.08f)).GetSafeNormal(), FMath::Clamp(Deflate, 0.f, 1.f)).GetSafeNormal();
		P += Dir * Seg;
		Out.Add(P);
	}
}

FVector UWRSkyBeast::Place(const FV& V) const
{
	FVector P = V.Rest;
	if (Kind == TEXT("ray"))
	{
		if (V.Part == RBody || V.Part == RRim)
		{
			// The stroke: a wave running out from the root to the tip, deeper the further out; a bank lifts the outer wing.
			const float W = FMath::Max(0.f, FMath::Abs(V.B) - 0.12f) / 0.88f;
			const float Wave = WaveAmp * FMath::Pow(W, 1.5f) * FMath::Sin(2.f * PI * Clock - 2.6f * FMath::Abs(V.B) + 0.004f * V.Rest.X);
			P.Z += Wave + Bank * V.B * 70.f * W - Dead * 60.f * W * W;
		}
		else if (V.Part == RLobeL || V.Part == RLobeR)
		{
			// Rolled into a spiral (curvature high), unrolled flat before the Undertow.
			const float S = -1.f + 2.f * float(V.Part == RLobeR);
			const FVector Root(RayNose - 14.f, S * 50.f, 2.f);
			const float Along = V.A * 110.f;
			const float K = FMath::Max(0.0005f, (1.f - Lobes) * 0.075f + 0.003f);
			const FVector L = V.Rest - Root;
			P = Root + FVector(FMath::Sin(K * Along) / K, L.Y, -(1.f - FMath::Cos(K * Along)) / K + L.Z);
		}
		else if (V.Part == RTail || V.Part == RBarb)
		{
			const float S = V.A;
			P.Y += 26.f * FMath::Pow(S, 1.5f) * FMath::Sin(2.f * PI * 0.45f * Time - 3.f * S);
			P.Z += 10.f * S * FMath::Sin(2.f * PI * 0.3f * Time - 2.f * S) + WaveAmp * 0.1f * S;
		}
	}
	else if (Kind == TEXT("whale"))
	{
		const float T = V.Part == WFluke ? 0.f : V.A;
		// The stroke runs up and down the body, strongest at the tail; the fluke rides the end of it, pitched with it.
		const float Wave = WaveAmp * FMath::Pow(1.f - FMath::Clamp(T, 0.f, 1.f), 2.2f) * FMath::Sin(2.f * PI * Clock - 2.4f * (1.f - T));
		if (V.Part == WJaw || V.Part == WThroat)
		{
			const FVector Hinge(FMath::Lerp(WhaleTail, WhaleNose, JawHinge), 0.f, -BeWhaleR(JawHinge) * 0.1f);
			const float Ang = FMath::DegreesToRadians(Jaw * (V.Part == WThroat ? 17.f : 34.f));
			P = Hinge + FQuat(FVector::YAxisVector, Ang).RotateVector(P - Hinge);
		}
		else if (V.Part == WFinL || V.Part == WFinR)
		{
			const float S = V.Part == WFinL ? -1.f : 1.f;
			const FVector Root(FMath::Lerp(WhaleTail, WhaleNose, 0.62f), S * BeWhaleR(0.62f) * 0.9f, -BeWhaleR(0.62f) * 0.45f);
			const float Ang = FMath::DegreesToRadians(14.f * FMath::Sin(2.f * PI * 0.5f * Time + (S > 0.f ? 0.f : 1.4f)) - 10.f);
			P = Root + FQuat(FVector::XAxisVector, S * Ang).RotateVector(P - Root);
		}
		else if (V.Part == WFluke)
		{
			const float Tip = FMath::Abs(V.Rest.Y) / 150.f;
			const float Lead = WaveAmp * FMath::Sin(2.f * PI * Clock - 2.4f);
			P.Z += Lead + WaveAmp * 0.6f * FMath::Sin(2.f * PI * Clock - 3.0f) * (0.5f + Tip * 0.5f) * (V.A + 0.2f);
		}
		if (V.Part != WFluke) P.Z += Wave;
	}
	else if (Kind == TEXT("bell"))
	{
		if (V.Part == BFloat || V.Part == BCrest)
		{
			const float Puff = 1.f + 0.07f * FMath::Sin(2.f * PI * 0.33f * Time) + Pulse * 0.12f;
			P *= FVector(Puff, Puff, Puff * FMath::Lerp(1.f, 0.25f, Deflate));
			if (V.Part == BCrest && V.B > 0.5f) P.Y += 8.f * FMath::Sin(2.f * PI * 0.4f * Time + V.A * 3.f) * (1.f - Deflate);
			P.Z -= Deflate * 18.f;
		}
	}
	return P * Scale;
}

void UWRSkyBeast::Pose(float Dt)
{
	const AWRCreature* Cr = Creature.Get();
	const UWRSkyBody* SB = Cr ? UWRSkyBody::Get(Cr) : nullptr;
	const bool bAlive = Cr && Cr->IsAlive();
	const FVector Vel = Cr ? Cr->GetVelocity() : FVector::ZeroVector;
	const float Speed = Vel.Size();
	// What it's doing, eased (nothing snaps).
	const bool bFlap = SB && (SB->Has(WRSkyFlag::Flap) || SB->Has(WRSkyFlag::Undertow) || SB->Has(WRSkyFlag::Breach));
	float WantAmp = 0.f, WantFreq = 0.2f;
	if (Kind == TEXT("ray")) { WantAmp = bFlap ? 135.f : FMath::Lerp(40.f, 80.f, FMath::Clamp(Speed / 1000.f, 0.f, 1.f)); WantFreq = bFlap ? 0.65f : FMath::Lerp(0.2f, 0.4f, FMath::Clamp(Speed / 1000.f, 0.f, 1.f)); }
	else if (Kind == TEXT("whale")) { WantAmp = FMath::Lerp(14.f, 46.f, FMath::Clamp(Speed / 800.f, 0.f, 1.f)); WantFreq = FMath::Lerp(0.15f, 0.4f, FMath::Clamp(Speed / 800.f, 0.f, 1.f)); }
	if (!bAlive) { WantAmp = 0.f; }
	WaveAmp = FMath::FInterpTo(WaveAmp, WantAmp, Dt, 2.f);
	WaveFreq = FMath::FInterpTo(WaveFreq, WantFreq, Dt, 1.5f);
	Clock += Dt * WaveFreq;
	Time += Dt;
	Lobes = FMath::FInterpTo(Lobes, SB && (SB->Has(WRSkyFlag::Lobes) || SB->Has(WRSkyFlag::Undertow)) ? 1.f : 0.f, Dt, 3.f);
	Jaw = FMath::FInterpTo(Jaw, SB && SB->Has(WRSkyFlag::Gape) ? 1.f : 0.f, Dt, 2.5f);
	Lash = FMath::FInterpTo(Lash, SB && SB->Has(WRSkyFlag::Lash) ? 1.f : 0.f, Dt, 4.f);
	Pulse = FMath::FInterpTo(Pulse, SB && SB->Has(WRSkyFlag::Lash) ? 1.f : 0.f, Dt, 6.f);
	Deflate = FMath::FInterpTo(Deflate, (SB && SB->Has(WRSkyFlag::Deflated)) || (!bAlive && Kind == TEXT("bell")) ? 1.f : 0.f, Dt, 0.8f);
	Dead = FMath::FInterpTo(Dead, bAlive ? 0.f : 1.f, Dt, 0.6f);
	// The bell's curtain trails opposite its drift (in its own frame).
	if (Kind == TEXT("bell") && Cr)
	{
		const FVector LocalVel = Cr->GetActorTransform().InverseTransformVectorNoScale(Vel);
		Trail = FMath::VInterpTo(Trail, (-LocalVel / 900.f).GetClampedToMaxSize(0.5f) * FVector(1.f, 1.f, 0.f), Dt, 1.f);
	}
	// Bank into turns, pitch with the climb; the whale rolls lazily; dead, belly-up.
	if (Cr)
	{
		const float Yaw = Cr->GetActorRotation().Yaw;
		const float Turn = Dt > 0.f ? FMath::FindDeltaAngleDegrees(LastYaw, Yaw) / Dt : 0.f;
		LastYaw = Yaw;
		const float WantBank = Kind == TEXT("bell") ? 0.f : FMath::Clamp(-Turn / 60.f, -1.f, 1.f);
		Bank = FMath::FInterpTo(Bank, WantBank, Dt, 2.f);
		const float WantPitch = Kind == TEXT("bell") ? 0.f : FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(Vel.Z, FMath::Max(200.f, Vel.Size2D()))), -35.f, 55.f);
		Pitch = FMath::FInterpTo(Pitch, SB && SB->Has(WRSkyFlag::Breach) ? 60.f : WantPitch, Dt, 2.f);
		const float WantRoll = Kind == TEXT("whale") ? 6.f * FMath::Sin(Time * 0.35f) + Bank * 20.f : Bank * 28.f;
		Roll = FMath::FInterpTo(Roll, WantRoll, Dt, 1.5f);
		if (Kind != TEXT("serpent")) SetRelativeRotation(FRotator(Pitch, 0.f, Roll + Dead * 170.f));
	}
	// The glow comes up at night (and in the Undercloud's fog, where it's always dim).
	if (GlowMat && Cr)
	{
		const bool bNight = AWRGameState::IsNightIn(Cr);
		const float Want = (bNight ? BeTn(TEXT("glowNight"), 6.f) : BeTn(TEXT("glowDay"), 0.5f)) * (1.f - Dead);
		GlowNow = FMath::FInterpTo(GlowNow, Want, Dt, 0.5f);
		GlowMat->SetScalarParameterValue(TEXT("Glow"), GlowNow);
	}
	// Aeolith: its path recorded, the head's frame, the sac, the hood, the spore-grey, the barrel roll.
	if (Kind == TEXT("serpent") && Cr)
	{
		RecordTrail();
		HeadXf = Cr->GetActorTransform();
		HeadXf.SetScale3D(FVector(1.f));
		Sac = FMath::FInterpTo(Sac, SB && SB->Has(WRSkyFlag::Lobes) ? 1.f : 0.f, Dt, 2.f);
		Hood = FMath::FInterpTo(Hood, SB && SB->Has(WRSkyFlag::Hood) ? 1.f : 0.f, Dt, 3.f);
		Grey = FMath::FInterpTo(Grey, SB && SB->Has(WRSkyFlag::Deflated) ? 1.f : 0.f, Dt, 0.8f);
		Roll360 = SB && SB->Has(WRSkyFlag::Breach) ? FMath::Fmod(Roll360 + Dt * 180.f, 360.f) : FMath::FInterpTo(Roll360 > 180.f ? Roll360 - 360.f : Roll360, 0.f, Dt, 3.f);
		WaveAmp = FMath::FInterpTo(WaveAmp, bAlive ? FMath::Lerp(18.f, 42.f, FMath::Clamp(Speed / 2400.f + (Vel.Z > 200.f ? 0.5f : 0.f), 0.f, 1.f)) : 0.f, Dt, 2.f);
		WaveFreq = FMath::Lerp(0.25f, 0.7f, FMath::Clamp(Speed / 2400.f, 0.f, 1.f));
		// Spore-sick: grey spreads over the feathers and scales.
		const FColor GreyC = FWRToolMesh::Pack(FLinearColor(FColor(0x8A, 0x8A, 0x80)), 0.8f);
		if (FMath::Abs(Grey - LastGrey) > 0.05f)
		{
			LastGrey = Grey;
			if (BaseColors.Num() != Body.Colors.Num()) BaseColors = Body.Colors;
			for (int32 i = 0; i < Body.Colors.Num(); ++i)
			{
				const FV& V = Body.Rest[i];
				const bool bGreys = (V.Part == SBody && V.A < Grey * 1.2f) || V.Part == SWingL || V.Part == SWingR || V.Part == SPlume || V.Part == SFan;
				Body.Colors[i] = bGreys && Grey > 0.1f ? FColor(uint8(FMath::Lerp(float(BaseColors[i].R), float(GreyC.R), Grey)), uint8(FMath::Lerp(float(BaseColors[i].G), float(GreyC.G), Grey)), uint8(FMath::Lerp(float(BaseColors[i].B), float(GreyC.B), Grey)), BaseColors[i].A) : BaseColors[i];
			}
		}
	}
	// Place every vertex.
	TArray<FVector> Joints;
	int32 JointsFor = -1;
	for (FSection* S : { &Body, &Glow })
	{
		S->Verts.SetNum(S->Rest.Num());
		for (int32 i = 0; i < S->Rest.Num(); ++i)
		{
			const FV& V = S->Rest[i];
			if (Kind == TEXT("bell") && (V.Part == BStrand || V.Part == BBead))
			{
				const int32 Strand = int32(V.A);
				if (Strand != JointsFor) { StrandJoints(Strand, Joints); JointsFor = Strand; }
				const int32 K = FMath::Clamp(int32(V.B), 0, Joints.Num() - 1);
				S->Verts[i] = (Joints[K] + V.Rest) * Scale;
				continue;
			}
			// The whale's barnacles ride its back (the same stroke).
			if (Kind == TEXT("whale") && V.Part == WBarnacle)
			{
				FV Hull = V;
				Hull.Part = WHull;
				const float T = V.A;
				const float Wave = WaveAmp * FMath::Pow(1.f - T, 2.2f) * FMath::Sin(2.f * PI * Clock - 2.4f * (1.f - T));
				S->Verts[i] = (V.Rest + FVector(0, 0, Wave)) * Scale;
				continue;
			}
			S->Verts[i] = Kind == TEXT("serpent") ? PlaceSerpent(V) : Place(V);
		}
		// Flat-ish shading: normals from the faces around each vertex.
		S->Normals.Init(FVector::ZeroVector, S->Verts.Num());
		for (int32 t = 0; t + 2 < S->Tris.Num(); t += 3)
		{
			const int32 A = S->Tris[t], B = S->Tris[t + 1], Cc = S->Tris[t + 2];
			const FVector N = FVector::CrossProduct(S->Verts[Cc] - S->Verts[A], S->Verts[B] - S->Verts[A]);
			S->Normals[A] += N; S->Normals[B] += N; S->Normals[Cc] += N;
		}
		for (FVector& N : S->Normals) N = N.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	}
}

void UWRSkyBeast::Commit(bool bCreate)
{
	const TArray<FProcMeshTangent> NoTangents;
	for (int32 Si = 0; Si < 2; ++Si)
	{
		FSection& S = Si == 0 ? Body : Glow;
		if (S.Verts.Num() == 0) continue;
		if (bCreate) CreateMeshSection(Si, S.Verts, S.Tris, S.Normals, S.UVs, S.Colors, NoTangents, false);
		else UpdateMeshSection(Si, S.Verts, S.Normals, S.UVs, S.Colors, NoTangents);
	}
}

FVector UWRSkyBeast::MouthWorld() const
{
	if (Kind == TEXT("serpent")) return HeadXf.TransformPosition(FVector(330.f, 0.f, 0.f) * Scale);
	const FVector Local = Kind == TEXT("ray") ? FVector(RayNose, 0.f, -10.f) : Kind == TEXT("whale") ? FVector(WhaleNose, 0.f, -BeWhaleR(0.9f) * 0.3f) : FVector::ZeroVector;
	return GetComponentTransform().TransformPosition(Local * Scale);
}

void UWRSkyBeast::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bBuilt) return;
	// Far from every camera it moves only twice a second (its wave keeps its phase).
	PoseClock += DeltaTime;
	const AActor* Owner = GetOwner();
	const bool bSeen = Owner && Owner->WasRecentlyRendered(0.5f);
	if (!bSeen && PoseClock < 0.5f) return;
	const float Dt = PoseClock;
	PoseClock = 0.f;
	Pose(Dt);
	Commit(false);
}
