// The Ocean's creature bodies built in code (cooks/biome-ocean.md 2.12.1; build-biome-ocean.md 2.12). Every vertex is
// stored at rest with its part and its place along the body, and re-placed every frame from how the real animal moves:
// a shark's tail beats side to side (the wave grows toward the tail), the goblin-shark jaw shoots forward on the bite; a
// moray's ribbon undulates and its inner jaw shows; a crab's legs step in alternating fours with planted feet and its
// club cocks back; wings beat; a manta's wings ripple root to tip; a turtle rows; an otter floats on its back.
#include "WROceanCreatures.h"
#include "Wildreach.h"
#include "WRCreature.h"
#include "WRGameState.h"
#include "WRToolMesh.h"
#include "WRTuning.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace WROceanBeastImpl
{
	enum EPart : uint8 { PBody = 0, PJaw, PTail, PFinL, PFinR, PPouch, PEye, PClawCrush, PClawClub, PGlow, PLeg0 = 20, PFish0 = 60 };
	FLinearColor Rgb(uint8 R, uint8 G, uint8 B, float K = 0.6f) { return FLinearColor(FColor(R, G, B)) * K; }
	/** Copy what a tool mesh built since From into a section, tagging the new vertices with Part (A, B worked out later). */
	void Take(FWRToolMesh& M, int32 FromV, int32 FromT, UWROceanBeast::FSection& S, uint8 Part, float B = 0.f)
	{
		const int32 Base = S.Rest.Num();
		for (int32 i = FromV; i < M.Verts.Num(); ++i)
		{
			UWROceanBeast::FV V; V.Rest = M.Verts[i]; V.Part = Part; V.B = B;
			S.Rest.Add(V);
			S.Colors.Add(M.Colors.IsValidIndex(i) ? M.Colors[i] : FColor::White);
			S.UVs.Add(M.UVs.IsValidIndex(i) ? M.UVs[i] : FVector2D::ZeroVector);
			S.Normals.Add(M.Normals.IsValidIndex(i) ? M.Normals[i] : FVector::UpVector);
		}
		for (int32 t = FromT; t < M.Tris.Num(); ++t) S.Tris.Add(M.Tris[t] - FromV + Base);
	}
	struct FMark { int32 V = 0, T = 0; };
	FMark MarkOf(const FWRToolMesh& M) { return { M.Verts.Num(), M.Tris.Num() }; }
	/** A fish-like spindle along +X from Tail to Nose, its section an ellipse (half-width Y, half-height Z) along a profile. */
	void Spindle(FWRToolMesh& M, float Tail, float Nose, const TArray<FVector>& Profile, const FLinearColor& C, float Rough, int32 Sides = 14)
	{
		M.Lathe(FVector(Tail, 0, 0), FVector(1, 0, 0), Nose - Tail, Profile, Sides, C, Rough);
	}
	void Limb(FWRToolMesh& M, const FVector& A, const FVector& B, float R0, float R1, const FLinearColor& C, float Rough = 0.7f, int32 Sides = 7)
	{
		const FVector D = B - A; const float L = D.Size(); if (L < 0.5f) return;
		M.Lathe(A, D / L, L, { FVector(0.f, R0, R0), FVector(1.f, R1, R1) }, Sides, C, Rough);
	}
	void Ball(FWRToolMesh& M, const FVector& C, float R, const FLinearColor& Col, float Rough = 0.6f, float Squash = 1.f, int32 Sides = 9)
	{
		TArray<FVector> P;
		for (int32 i = 0; i <= 6; ++i) { const float A = PI * i / 6.f; P.Add(FVector(i / 6.f, FMath::Max(0.4f, R * FMath::Sin(A)), FMath::Max(0.4f, R * FMath::Sin(A)))); }
		M.Lathe(C - FVector(0, 0, R * Squash), FVector::UpVector, 2.f * R * Squash, P, Sides, Col, Rough);
	}
	/** A flat fin: a thin wedge from Root (chord along X) out to Tip. */
	void Fin(FWRToolMesh& M, const FVector& RootA, const FVector& RootB, const FVector& Tip, float Thick, const FLinearColor& C)
	{
		const FColor P = FWRToolMesh::Pack(C, 0.55f);
		const FVector Up(0, 0, Thick * 0.5f);
		const FVector N = FVector::CrossProduct(RootB - RootA, Tip - RootA).GetSafeNormal() * Thick * 0.5f;
		M.Tri(RootA + N, RootB + N, Tip + N, P, FVector2D(0, 0), FVector2D(1, 0), FVector2D(0, 1));
		M.Tri(RootA - N, Tip - N, RootB - N, P, FVector2D(0, 0), FVector2D(0, 1), FVector2D(1, 0));
		(void)Up;
	}
}
using namespace WROceanBeastImpl;

UWROceanBeast::UWROceanBeast(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetCastShadow(true);
}

void UWROceanBeast::Init(AWRCreature* InOwner, FName InKind, float InScale, int32 InSeed)
{
	Creature = InOwner;
	Kind = InKind;
	Scale = InScale;
	Seed = InSeed;
	Build();
	Commit(true);
	SetMaterial(0, FWRToolMesh::Material());
	if (Glow.Rest.Num() > 0)
	{
		if (UMaterialInterface* G = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Creatures/M_Glow.M_Glow")))
		{
			GlowMat = UMaterialInstanceDynamic::Create(G, this);
			const FLinearColor Col = Kind == TEXT("eel") ? FLinearColor(0.55f, 0.75f, 1.f) : FLinearColor(0.35f, 0.9f, 0.78f);
			GlowMat->SetVectorParameterValue(TEXT("Color"), Col);
			GlowMat->SetScalarParameterValue(TEXT("Glow"), 0.2f);
			SetMaterial(1, GlowMat);
		}
	}
	bBuilt = true;
}

void UWROceanBeast::Build()
{
	FWRToolMesh M;
	FRandomStream R(Seed);
	const float S = Scale;
	if (Kind == TEXT("shark"))
	{
		// A 6 m tiger shark: bronze-grey back with faded bars, bone-white belly, the long flat snout; the slingshot jaw a
		// part of its own under the snout; a scythe tail; two pectorals and a tall dorsal.
		FMark K = MarkOf(M);
		const FLinearColor Back = Rgb(0x5E, 0x5A, 0x4E, 0.55f), Belly = Rgb(0xEA, 0xE4, 0xD8, 0.65f);
		Spindle(M, -300.f * S, 300.f * S, { FVector(0.f, 8.f * S, 6.f * S), FVector(0.12f, 22.f * S, 26.f * S), FVector(0.45f, 62.f * S, 70.f * S), FVector(0.72f, 58.f * S, 60.f * S), FVector(0.9f, 30.f * S, 22.f * S), FVector(1.f, 10.f * S, 6.f * S) }, Back, 0.45f, 16);
		// Belly paint: the lower verts whitened; bars across the back.
		for (int32 i = K.V; i < M.Verts.Num(); ++i)
		{
			const FVector& P = M.Verts[i];
			FLinearColor C = P.Z < -10.f * S ? Belly : Back;
			if (P.Z > 0.f && FMath::Fmod(FMath::Abs(P.X) / (40.f * S), 2.f) < 0.5f) C *= 0.8f;
			M.Colors[i] = FWRToolMesh::Pack(C, 0.45f);
		}
		Take(M, K.V, K.T, Body, PBody); K = MarkOf(M);
		Fin(M, FVector(30.f * S, 0, 55.f * S), FVector(-60.f * S, 0, 55.f * S), FVector(-70.f * S, 0, 150.f * S), 6.f * S, Back * 0.9f);
		Fin(M, FVector(80.f * S, 30.f * S, -30.f * S), FVector(20.f * S, 40.f * S, -30.f * S), FVector(-40.f * S, 150.f * S, -60.f * S), 5.f * S, Back);
		Fin(M, FVector(80.f * S, -30.f * S, -30.f * S), FVector(20.f * S, -40.f * S, -30.f * S), FVector(-40.f * S, -150.f * S, -60.f * S), 5.f * S, Back);
		Take(M, K.V, K.T, Body, PBody); K = MarkOf(M);
		Fin(M, FVector(-270.f * S, 0, 8.f * S), FVector(-300.f * S, 0, 0), FVector(-420.f * S, 0, 130.f * S), 6.f * S, Back * 0.9f);
		Fin(M, FVector(-270.f * S, 0, -4.f * S), FVector(-300.f * S, 0, 0), FVector(-390.f * S, 0, -80.f * S), 6.f * S, Back * 0.9f);
		Take(M, K.V, K.T, Body, PTail); K = MarkOf(M);
		// The slingshot jaw: a U of jawbone with rows of teeth, pale gums; it sits under the snout and shoots out.
		Spindle(M, 180.f * S, 270.f * S, { FVector(0.f, 30.f * S, 10.f * S), FVector(0.6f, 26.f * S, 12.f * S), FVector(1.f, 8.f * S, 6.f * S) }, Rgb(0xE8, 0xB0, 0xB0, 0.6f), 0.4f, 10);
		for (int32 t = 0; t < 14; ++t)
		{
			const float A = (t / 13.f - 0.5f) * 1.6f;
			M.Lathe(FVector(250.f * S - FMath::Abs(A) * 40.f * S, FMath::Sin(A) * 24.f * S, -2.f * S), FVector(0, 0, 1), 10.f * S, { FVector(0.f, 2.5f * S, 2.5f * S), FVector(1.f, 0.3f, 0.3f) }, 4, Rgb(0xF4, 0xF0, 0xE4, 0.8f), 0.2f);
		}
		for (int32 i = K.V; i < M.Verts.Num(); ++i) M.Verts[i].Z -= 26.f * S;
		Take(M, K.V, K.T, Body, PJaw); K = MarkOf(M);
		Ball(M, FVector(230.f * S, 40.f * S, 14.f * S), 6.f * S, FLinearColor(0.01f, 0.01f, 0.01f), 0.1f);
		Ball(M, FVector(230.f * S, -40.f * S, 14.f * S), 6.f * S, FLinearColor(0.01f, 0.01f, 0.01f), 0.1f);
		Take(M, K.V, K.T, Body, PBody);
	}
	else if (Kind == TEXT("eel"))
	{
		// A 3.5 m moray ribbon (the Siren's scale x5): honeycomb olive and yellow, a fat gaping head; its banded organs
		// along the flanks (lit on the shock pulse, section 1); the pharyngeal inner jaw (pale) in the throat.
		const float L = 350.f * S;
		FMark K = MarkOf(M);
		Spindle(M, -L * 0.5f, L * 0.5f, { FVector(0.f, 3.f * S, 10.f * S), FVector(0.2f, 14.f * S, 20.f * S), FVector(0.7f, 18.f * S, 22.f * S), FVector(0.9f, 20.f * S, 22.f * S), FVector(1.f, 9.f * S, 8.f * S) }, Rgb(0x6A, 0x6A, 0x2E, 0.5f), 0.35f, 12);
		for (int32 i = K.V; i < M.Verts.Num(); ++i) if (FMath::Frac(M.Verts[i].X / (18.f * S) + M.Verts[i].Z / (30.f * S)) < 0.35f) M.Colors[i] = FWRToolMesh::Pack(Rgb(0xC8, 0xB8, 0x60, 0.55f), 0.35f);
		Take(M, K.V, K.T, Body, PBody); K = MarkOf(M);
		Fin(M, FVector(L * 0.4f, 0, 18.f * S), FVector(-L * 0.5f, 0, 8.f * S), FVector(-L * 0.1f, 0, 26.f * S), 3.f * S, Rgb(0x5A, 0x5A, 0x26, 0.5f));
		Take(M, K.V, K.T, Body, PBody); K = MarkOf(M);
		Spindle(M, L * 0.42f, L * 0.5f + 6.f * S, { FVector(0.f, 10.f * S, 8.f * S), FVector(1.f, 4.f * S, 3.f * S) }, Rgb(0xF0, 0xE8, 0xD8, 0.7f), 0.3f, 8);
		for (int32 i = K.V; i < M.Verts.Num(); ++i) M.Verts[i].Z -= 6.f * S;
		Take(M, K.V, K.T, Body, PJaw); K = MarkOf(M);
		Ball(M, FVector(L * 0.46f, 14.f * S, 12.f * S), 2.5f * S, FLinearColor(0.02f, 0.02f, 0.01f), 0.1f);
		Ball(M, FVector(L * 0.46f, -14.f * S, 12.f * S), 2.5f * S, FLinearColor(0.02f, 0.02f, 0.01f), 0.1f);
		Take(M, K.V, K.T, Body, PBody);
		// The organs: a row of pale bands each side (the glow section).
		FWRToolMesh G;
		for (int32 b = 0; b < 12; ++b)
		{
			const float X = -L * 0.4f + b * L * 0.065f;
			for (int32 Sd = -1; Sd <= 1; Sd += 2)
			{
				const int32 From = G.Verts.Num();
				Spindle(G, X, X + 8.f * S, { FVector(0.f, 3.f * S, 6.f * S), FVector(1.f, 3.f * S, 6.f * S) }, FLinearColor::White, 0.3f, 6);
				for (int32 i = From; i < G.Verts.Num(); ++i) G.Verts[i].Y += Sd * 16.f * S;
			}
		}
		Take(G, 0, 0, Glow, PGlow);
	}
	else if (Kind == TEXT("crab"))
	{
		// A spiky rust-red dome on stilt legs (3 m span), cream spines, one huge crusher, one blue-green mantis club cocked,
		// eyestalks on blue stalks. Old Barnacle is this at x2.5 with his hull (the boss draws that).
		const FLinearColor Shell = Rgb(0x9A, 0x3E, 0x2A, 0.55f), Spine = Rgb(0xEA, 0xDF, 0xC8, 0.65f);
		FMark K = MarkOf(M);
		Ball(M, FVector(0, 0, 70.f * S), 70.f * S, Shell, 0.6f, 0.55f, 14);
		for (int32 sp = 0; sp < 22; ++sp)
		{
			const float A = R.FRandRange(0.f, 2.f * PI), E = R.FRandRange(0.2f, 1.2f);
			const FVector Dir(FMath::Cos(A) * FMath::Cos(E), FMath::Sin(A) * FMath::Cos(E), FMath::Sin(E));
			const FVector Base = FVector(0, 0, 70.f * S) + FVector(Dir.X * 66.f * S, Dir.Y * 66.f * S, Dir.Z * 38.f * S);
			M.Lathe(Base, Dir, 22.f * S, { FVector(0.f, 4.f * S, 4.f * S), FVector(1.f, 0.4f, 0.4f) }, 5, Spine, 0.5f);
		}
		Take(M, K.V, K.T, Body, PBody); K = MarkOf(M);
		for (int32 e = -1; e <= 1; e += 2)
		{
			Limb(M, FVector(55.f * S, e * 18.f * S, 90.f * S), FVector(70.f * S, e * 24.f * S, 130.f * S), 3.f * S, 2.5f * S, Rgb(0x3E, 0x6E, 0xA8, 0.55f));
			Ball(M, FVector(70.f * S, e * 24.f * S, 134.f * S), 5.f * S, FLinearColor(0.02f, 0.02f, 0.02f), 0.1f);
		}
		Take(M, K.V, K.T, Body, PEye); K = MarkOf(M);
		// Eight walking legs, four a side: three segments each, the tip planted; each leg its own part (it steps).
		for (int32 l = 0; l < 8; ++l)
		{
			const int32 Side = l < 4 ? 1 : -1;
			const float A = FMath::DegreesToRadians(-50.f + (l % 4) * 33.f);
			const FVector Hip(FMath::Cos(A) * 55.f * S, Side * FMath::Sin(A + PI * 0.5f) * 55.f * S, 60.f * S);
			const FVector Knee = Hip + FVector(FMath::Cos(A) * 70.f * S, Side * 90.f * S, 60.f * S);
			const FVector Foot = Hip + FVector(FMath::Cos(A) * 130.f * S, Side * 150.f * S, -60.f * S);
			const FMark L = MarkOf(M);
			Limb(M, Hip, Knee, 8.f * S, 6.f * S, Shell);
			Limb(M, Knee, Foot, 6.f * S, 2.f * S, Rgb(0xC8, 0x60, 0x40, 0.55f));
			Take(M, L.V, L.T, Body, uint8(PLeg0 + l));
			// B along the leg (0 hip .. 1 foot) for the step lift.
			for (int32 i = Body.Rest.Num() - (M.Verts.Num() - L.V); i < Body.Rest.Num(); ++i) Body.Rest[i].B = FMath::Clamp((Hip.Z - Body.Rest[i].Rest.Z + 70.f * S) / (130.f * S), 0.f, 1.f);
		}
		K = MarkOf(M);
		// The crusher (left): a heavy two-fingered claw. The club (right): a smooth hammer on a jointed arm.
		Limb(M, FVector(60.f * S, 40.f * S, 70.f * S), FVector(140.f * S, 60.f * S, 60.f * S), 14.f * S, 16.f * S, Shell);
		Ball(M, FVector(170.f * S, 62.f * S, 60.f * S), 30.f * S, Shell * 1.05f, 0.5f, 0.7f);
		Limb(M, FVector(190.f * S, 62.f * S, 64.f * S), FVector(240.f * S, 66.f * S, 70.f * S), 12.f * S, 3.f * S, Rgb(0x2A, 0x24, 0x20, 0.5f));
		Take(M, K.V, K.T, Body, PClawCrush); K = MarkOf(M);
		Limb(M, FVector(60.f * S, -40.f * S, 70.f * S), FVector(110.f * S, -60.f * S, 100.f * S), 9.f * S, 8.f * S, Shell);
		Limb(M, FVector(110.f * S, -60.f * S, 100.f * S), FVector(150.f * S, -60.f * S, 70.f * S), 8.f * S, 7.f * S, Shell);
		Ball(M, FVector(160.f * S, -60.f * S, 66.f * S), 14.f * S, Rgb(0x3E, 0x8C, 0x8A, 0.6f), 0.25f, 1.1f);
		Take(M, K.V, K.T, Body, PClawClub);
	}
	else if (Kind == TEXT("gull") || Kind == TEXT("puffin"))
	{
		const bool bGull = Kind == TEXT("gull");
		// Gullraider: a dark skua body, white wing flashes, a hooked bill, the scarlet-orange throat pouch (never crimson).
		// Tidefowl: a round black-and-white seabird, the puffin's striped bill.
		const FLinearColor Dark = bGull ? Rgb(0x4A, 0x3A, 0x2E, 0.5f) : Rgb(0x14, 0x14, 0x14, 0.5f);
		FMark K = MarkOf(M);
		Spindle(M, -40.f * S, 40.f * S, { FVector(0.f, 4.f * S, 3.f * S), FVector(0.4f, 16.f * S, 17.f * S), FVector(0.8f, 13.f * S, 14.f * S), FVector(1.f, 8.f * S, 8.f * S) }, Dark, 0.8f, 10);
		if (!bGull) for (int32 i = K.V; i < M.Verts.Num(); ++i) if (M.Verts[i].Z < 0.f) M.Colors[i] = FWRToolMesh::Pack(Rgb(0xF2, 0xF0, 0xEC, 0.7f), 0.8f);
		Ball(M, FVector(44.f * S, 0, 8.f * S), 10.f * S, bGull ? Dark : Rgb(0xE8, 0xE6, 0xE0, 0.7f), 0.8f);
		const FLinearColor Bill = bGull ? Rgb(0x5A, 0x50, 0x40, 0.5f) : Rgb(0xE8, 0x70, 0x30, 0.65f);
		Limb(M, FVector(52.f * S, 0, 8.f * S), FVector(68.f * S, 0, 4.f * S), 4.f * S, 1.f * S, Bill);
		if (bGull) Limb(M, FVector(66.f * S, 0, 4.f * S), FVector(70.f * S, 0, -1.f * S), 1.5f * S, 0.4f * S, Bill);
		else { Limb(M, FVector(52.f * S, 0, 7.f * S), FVector(60.f * S, 0, 6.f * S), 5.f * S, 4.f * S, Rgb(0x3E, 0x5A, 0xA8, 0.6f)); Limb(M, FVector(60.f * S, 0, 6.f * S), FVector(64.f * S, 0, 5.f * S), 4.f * S, 3.f * S, Rgb(0xE8, 0xC8, 0x30, 0.65f)); }
		Limb(M, FVector(-38.f * S, 0, 0), FVector(-56.f * S, 0, -2.f * S), 7.f * S, 1.f * S, Dark);
		Take(M, K.V, K.T, Body, PBody); K = MarkOf(M);
		if (bGull) { Ball(M, FVector(40.f * S, 0, -6.f * S), 8.f * S, Rgb(0xD8, 0x50, 0x2A, 0.65f), 0.4f); Take(M, K.V, K.T, Body, PPouch); K = MarkOf(M); }
		const float Span = (bGull ? 100.f : 30.f) * S;
		for (int32 s = -1; s <= 1; s += 2)
		{
			const FMark W = MarkOf(M);
			Fin(M, FVector(15.f * S, s * 10.f * S, 6.f * S), FVector(-15.f * S, s * 10.f * S, 6.f * S), FVector(-20.f * S, s * Span, 8.f * S), 3.f * S, Dark);
			if (bGull) Fin(M, FVector(-5.f * S, s * Span * 0.6f, 8.f * S), FVector(-12.f * S, s * Span * 0.7f, 8.f * S), FVector(-18.f * S, s * Span * 0.95f, 8.f * S), 3.5f * S, Rgb(0xF0, 0xF0, 0xEC, 0.7f));
			Take(M, W.V, W.T, Body, s > 0 ? PFinL : PFinR);
		}
		K = MarkOf(M);
		for (int32 s = -1; s <= 1; s += 2) Limb(M, FVector(0, s * 6.f * S, -14.f * S), FVector(4.f * S, s * 7.f * S, -26.f * S), 2.f * S, 1.5f * S, bGull ? FLinearColor(0.02f, 0.02f, 0.02f) : Rgb(0xE8, 0x70, 0x30, 0.6f));
		Take(M, K.V, K.T, Body, PBody);
	}
	else if (Kind == TEXT("seahorse"))
	{
		// The Wavecharger: an upright horse head on ring-plated armour, leafy-seadragon fronds, a dolphin's driving tail
		// stock curling at the tip; coral orange to gold, olive-gold fronds, black eyes. +X forward, the body upright.
		const FLinearColor Coral = Rgb(0xE0, 0x7A, 0x4F, 0.6f), Gold = Rgb(0xD8, 0xA8, 0x40, 0.6f), Frond = Rgb(0x9A, 0x8A, 0x3A, 0.55f);
		FMark K = MarkOf(M);
		// Trunk: rings from the belly (low) up to the neck (high), leaning forward.
		TArray<FVector> Spine;
		for (int32 i = 0; i <= 10; ++i) { const float T = i / 10.f; Spine.Add(FVector(20.f * S * FMath::Sin(T * 2.4f), 0, -100.f * S + T * 220.f * S)); }
		for (int32 i = 0; i < 10; ++i)
		{
			const float R0 = (32.f + 10.f * FMath::Sin(i / 10.f * PI)) * S;
			M.Lathe(Spine[i], (Spine[i + 1] - Spine[i]).GetSafeNormal(), FVector::Dist(Spine[i], Spine[i + 1]) + 2.f, { FVector(0.f, R0, R0 * 0.8f), FVector(0.85f, R0 * 0.96f, R0 * 0.78f), FVector(1.f, R0 * 0.88f, R0 * 0.7f) }, 10, FMath::Lerp(Coral, Gold, i / 10.f), 0.5f);
		}
		// The head: a long snout forward from the neck, the crest, the eyes.
		Limb(M, Spine.Last() + FVector(0, 0, 10.f * S), Spine.Last() + FVector(110.f * S, 0, -10.f * S), 26.f * S, 7.f * S, Coral, 0.5f, 10);
		Ball(M, Spine.Last() + FVector(10.f * S, 0, 14.f * S), 30.f * S, Coral, 0.5f);
		for (int32 e = -1; e <= 1; e += 2) Ball(M, Spine.Last() + FVector(30.f * S, e * 24.f * S, 22.f * S), 6.f * S, FLinearColor(0.01f, 0.01f, 0.01f), 0.1f);
		Take(M, K.V, K.T, Body, PBody); K = MarkOf(M);
		// The tail stock going down and back, curling at the tip (it wraps a kelp stipe to rest).
		TArray<FVector> Tail;
		for (int32 i = 0; i <= 12; ++i) { const float T = i / 12.f; Tail.Add(Spine[0] + FVector(-T * 140.f * S + FMath::Sin(T * PI * 1.5f) * 40.f * S, 0, -T * 120.f * S + (T > 0.7f ? (T - 0.7f) * 200.f * S : 0.f))); }
		M.Tube(Tail, 16.f * S, 8, Gold, 0.5f);
		Take(M, K.V, K.T, Body, PTail); K = MarkOf(M);
		// The fronds: leafy streamers off the neck, back and tail.
		for (int32 f = 0; f < 9; ++f)
		{
			const FVector Root = f < 6 ? Spine[2 + f] + FVector(-28.f * S, 0, 0) : Tail[3 + (f - 6) * 3];
			const FVector Dir = FRotator(R.FRandRange(-30.f, 30.f), 180.f + R.FRandRange(-60.f, 60.f), 0.f).Vector();
			M.Lathe(Root, Dir, R.FRandRange(50.f, 90.f) * S, { FVector(0.f, 3.f * S, 2.f * S), FVector(0.4f, 14.f * S, 2.f * S), FVector(1.f, 2.f * S, 1.f * S) }, 4, Frond, 0.6f);
		}
		Take(M, K.V, K.T, Body, PFinL);
	}
	else if (Kind == TEXT("turtle"))
	{
		// The Driftback: a leatherback's ridged leathery dome (5 m), slate-black with white spots, a barnacle-and-weed garden
		// on its back, long fore flippers, a small beaked head.
		const FLinearColor Shell = Rgb(0x1E, 0x20, 0x22, 0.55f);
		FMark K = MarkOf(M);
		Spindle(M, -250.f * S, 220.f * S, { FVector(0.f, 30.f * S, 20.f * S), FVector(0.25f, 170.f * S, 80.f * S), FVector(0.6f, 190.f * S, 90.f * S), FVector(0.9f, 120.f * S, 60.f * S), FVector(1.f, 40.f * S, 30.f * S) }, Shell, 0.6f, 18);
		for (int32 i = K.V; i < M.Verts.Num(); ++i)
		{
			const FVector P = M.Verts[i];
			if (P.Z < -20.f * S) M.Verts[i].Z *= 0.4f;   // a flat plastron
			if (R.FRand() < 0.06f) M.Colors[i] = FWRToolMesh::Pack(Rgb(0xE8, 0xE8, 0xE4, 0.7f), 0.6f);
		}
		// The ridges: seven raised keels along the back.
		for (int32 r = -3; r <= 3; ++r) Limb(M, FVector(-220.f * S, r * 45.f * S, 70.f * S - FMath::Abs(r) * 14.f * S), FVector(180.f * S, r * 40.f * S, 72.f * S - FMath::Abs(r) * 14.f * S), 6.f * S, 4.f * S, Shell * 1.1f);
		// The garden: barnacle clusters and weed.
		for (int32 b = 0; b < 26; ++b) Ball(M, FVector(R.FRandRange(-180.f, 150.f) * S, R.FRandRange(-120.f, 120.f) * S, 80.f * S), R.FRandRange(6.f, 14.f) * S, b % 3 ? Rgb(0xD8, 0xD0, 0xBE, 0.6f) : Rgb(0x5A, 0x6A, 0x2E, 0.5f), 0.9f, 0.6f, 6);
		Ball(M, FVector(260.f * S, 0, 10.f * S), 34.f * S, Shell, 0.6f);
		Limb(M, FVector(285.f * S, 0, 6.f * S), FVector(300.f * S, 0, -4.f * S), 14.f * S, 4.f * S, Rgb(0x3A, 0x34, 0x30, 0.5f));
		Take(M, K.V, K.T, Body, PBody); K = MarkOf(M);
		Fin(M, FVector(140.f * S, 120.f * S, -10.f * S), FVector(60.f * S, 140.f * S, -10.f * S), FVector(-40.f * S, 420.f * S, -40.f * S), 12.f * S, Shell);
		Take(M, K.V, K.T, Body, PFinL); K = MarkOf(M);
		Fin(M, FVector(140.f * S, -120.f * S, -10.f * S), FVector(60.f * S, -140.f * S, -10.f * S), FVector(-40.f * S, -420.f * S, -40.f * S), 12.f * S, Shell);
		Take(M, K.V, K.T, Body, PFinR); K = MarkOf(M);
		Fin(M, FVector(-200.f * S, 80.f * S, -10.f * S), FVector(-240.f * S, 60.f * S, -10.f * S), FVector(-300.f * S, 150.f * S, -20.f * S), 10.f * S, Shell);
		Fin(M, FVector(-200.f * S, -80.f * S, -10.f * S), FVector(-240.f * S, -60.f * S, -10.f * S), FVector(-300.f * S, -150.f * S, -20.f * S), 10.f * S, Shell);
		Take(M, K.V, K.T, Body, PTail);
	}
	else if (Kind == TEXT("manta"))
	{
		// The Glowray: a 4 m manta, midnight blue over a white belly, two curled cephalic horns, a barbed tail; its light
		// spots (blue-green) in a pattern on its back (section 1).
		const FLinearColor Back = Rgb(0x1A, 0x2A, 0x4A, 0.5f);
		FMark K = MarkOf(M);
		Spindle(M, -80.f * S, 100.f * S, { FVector(0.f, 10.f * S, 6.f * S), FVector(0.5f, 50.f * S, 22.f * S), FVector(1.f, 30.f * S, 10.f * S) }, Back, 0.4f, 12);
		Take(M, K.V, K.T, Body, PBody); K = MarkOf(M);
		for (int32 s = -1; s <= 1; s += 2)
		{
			const FMark W = MarkOf(M);
			Fin(M, FVector(80.f * S, s * 30.f * S, 0), FVector(-60.f * S, s * 30.f * S, 0), FVector(-30.f * S, s * 200.f * S, 4.f * S), 8.f * S, Back);
			Take(M, W.V, W.T, Body, s > 0 ? PFinL : PFinR);
			const FMark H = MarkOf(M);
			Limb(M, FVector(95.f * S, s * 26.f * S, 0), FVector(130.f * S, s * 34.f * S, -10.f * S), 7.f * S, 3.f * S, Back);
			Take(M, H.V, H.T, Body, PJaw);
		}
		K = MarkOf(M);
		Limb(M, FVector(-80.f * S, 0, 0), FVector(-260.f * S, 0, 6.f * S), 6.f * S, 1.f * S, Back);
		M.Lathe(FVector(-200.f * S, 0, 10.f * S), FVector(-1, 0, 0.3f).GetSafeNormal(), 24.f * S, { FVector(0.f, 3.f * S, 3.f * S), FVector(1.f, 0.4f, 0.4f) }, 5, Rgb(0xD8, 0xD8, 0xD0, 0.6f), 0.2f);
		Take(M, K.V, K.T, Body, PTail);
		FWRToolMesh G;
		for (int32 sp = 0; sp < 24; ++sp)
		{
			const float X = R.FRandRange(-60.f, 80.f) * S, Y = R.FRandRange(-160.f, 160.f) * S * (1.f - FMath::Abs(X) / (120.f * S));
			Ball(G, FVector(X, Y, 10.f * S), 4.f * S, FLinearColor::White, 0.3f, 0.4f, 6);
		}
		Take(G, 0, 0, Glow, PGlow);
		for (FV& V : Glow.Rest) V.B = FMath::Abs(V.Rest.Y) / (200.f * S);
	}
	else if (Kind == TEXT("otter"))
	{
		// The Pearldiver: a sleek otter floating on its back (belly up, +Z), chocolate with a frosted-silver head, its stone
		// on its chest.
		const FLinearColor Fur = Rgb(0x4A, 0x34, 0x26, 0.5f), Silver = Rgb(0xC8, 0xC4, 0xBC, 0.6f);
		FMark K = MarkOf(M);
		Spindle(M, -70.f * S, 50.f * S, { FVector(0.f, 8.f * S, 8.f * S), FVector(0.5f, 24.f * S, 20.f * S), FVector(1.f, 18.f * S, 16.f * S) }, Fur, 0.9f, 10);
		Ball(M, FVector(62.f * S, 0, 6.f * S), 15.f * S, Silver, 0.9f);
		for (int32 e = -1; e <= 1; e += 2) Ball(M, FVector(72.f * S, e * 7.f * S, 14.f * S), 2.f * S, FLinearColor(0.01f, 0.01f, 0.01f), 0.1f);
		Limb(M, FVector(-70.f * S, 0, 0), FVector(-120.f * S, 0, 4.f * S), 9.f * S, 3.f * S, Fur);
		Take(M, K.V, K.T, Body, PBody); K = MarkOf(M);
		for (int32 s = -1; s <= 1; s += 2) Limb(M, FVector(30.f * S, s * 18.f * S, 10.f * S), FVector(40.f * S, s * 6.f * S, 24.f * S), 5.f * S, 4.f * S, Fur);
		Ball(M, FVector(36.f * S, 0, 26.f * S), 7.f * S, Rgb(0x8A, 0x86, 0x7E, 0.6f), 0.7f, 0.7f);
		Take(M, K.V, K.T, Body, PFinL);
	}
	else if (Kind == TEXT("seacow"))
	{
		// The Kelpgrazer: a 7 m barrel, bark-grey wrinkled hide with pale scars, a small whiskered head, a forked tail.
		const FLinearColor Hide = Rgb(0x6A, 0x66, 0x5E, 0.5f);
		FMark K = MarkOf(M);
		Spindle(M, -320.f * S, 320.f * S, { FVector(0.f, 30.f * S, 26.f * S), FVector(0.25f, 120.f * S, 110.f * S), FVector(0.6f, 130.f * S, 120.f * S), FVector(0.9f, 70.f * S, 60.f * S), FVector(1.f, 40.f * S, 36.f * S) }, Hide, 0.95f, 16);
		for (int32 i = K.V; i < M.Verts.Num(); ++i) if (R.FRand() < 0.04f) M.Colors[i] = FWRToolMesh::Pack(Rgb(0xB8, 0xB0, 0xA4, 0.6f), 0.9f);
		for (int32 w = 0; w < 8; ++w) Limb(M, FVector(320.f * S, (w - 3.5f) * 6.f * S, -14.f * S), FVector(350.f * S, (w - 3.5f) * 12.f * S, -24.f * S), 1.f * S, 0.5f * S, Rgb(0xD8, 0xD0, 0xC0, 0.6f), 0.5f, 4);
		Take(M, K.V, K.T, Body, PBody); K = MarkOf(M);
		for (int32 s = -1; s <= 1; s += 2) Fin(M, FVector(150.f * S, s * 100.f * S, -40.f * S), FVector(100.f * S, s * 110.f * S, -40.f * S), FVector(80.f * S, s * 220.f * S, -80.f * S), 10.f * S, Hide);
		Take(M, K.V, K.T, Body, PFinL); K = MarkOf(M);
		Fin(M, FVector(-300.f * S, 0, 0), FVector(-330.f * S, 0, 0), FVector(-470.f * S, 160.f * S, 10.f * S), 10.f * S, Hide);
		Fin(M, FVector(-300.f * S, 0, 0), FVector(-330.f * S, 0, 0), FVector(-470.f * S, -160.f * S, 10.f * S), 10.f * S, Hide);
		Take(M, K.V, K.T, Body, PTail);
	}
	else if (Kind == TEXT("school"))
	{
		// A Ripplefin school: thirty mackerel (silver-blue, green tiger lines, ripple fins), each its own part so it darts.
		for (int32 f = 0; f < 30; ++f)
		{
			const FMark F = MarkOf(M);
			const FVector C(R.FRandRange(-150.f, 150.f), R.FRandRange(-150.f, 150.f), R.FRandRange(-100.f, 100.f));
			Spindle(M, C.X - 30.f, C.X + 30.f, { FVector(0.f, 1.f, 1.f), FVector(0.4f, 5.f, 7.f), FVector(1.f, 1.f, 1.f) }, Rgb(0xA8, 0xB8, 0xC8, 0.65f), 0.15f, 6);
			for (int32 i = F.V; i < M.Verts.Num(); ++i) { M.Verts[i].Y += C.Y; M.Verts[i].Z += C.Z; if (M.Verts[i].Z > C.Z + 3.f) M.Colors[i] = FWRToolMesh::Pack(Rgb(0x3E, 0x6E, 0x5A, 0.55f), 0.2f); }
			Fin(M, FVector(C.X - 30.f, C.Y, C.Z), FVector(C.X - 26.f, C.Y, C.Z), FVector(C.X - 40.f, C.Y, C.Z + 9.f), 1.f, Rgb(0xA8, 0xB8, 0xC8, 0.6f));
			Take(M, F.V, F.T, Body, uint8(PFish0 + f));
		}
	}
	// A along the body (0 tail .. 1 nose) for the wave.
	MinX = 1.0e9f; MaxX = -1.0e9f;
	for (const FV& V : Body.Rest) { MinX = FMath::Min(MinX, V.Rest.X); MaxX = FMath::Max(MaxX, V.Rest.X); }
	for (FV& V : Body.Rest) V.A = FMath::Clamp((V.Rest.X - MinX) / FMath::Max(1.f, MaxX - MinX), 0.f, 1.f);
	for (FV& V : Glow.Rest) V.A = FMath::Clamp((V.Rest.X - MinX) / FMath::Max(1.f, MaxX - MinX), 0.f, 1.f);
}

FVector UWROceanBeast::Place(const FV& V) const
{
	FVector P = V.Rest;
	const float Phase = Clock * 2.f * PI;
	if (Kind == TEXT("shark") || Kind == TEXT("eel") || Kind == TEXT("seacow"))
	{
		// The body wave: a shark's sideways beat growing toward the tail; an eel's whole-body ribbon; the sea cow's slow
		// up-down fluke stroke.
		const float Toward = 1.f - V.A;
		if (Kind == TEXT("seacow")) P.Z += Amp * 60.f * Scale * FMath::Pow(Toward, 2.f) * FMath::Sin(Phase - V.A * 2.f);
		else P.Y += Amp * (Kind == TEXT("eel") ? 26.f : 45.f) * Scale * FMath::Pow(Toward, Kind == TEXT("eel") ? 1.f : 2.2f) * FMath::Sin(Phase - V.A * (Kind == TEXT("eel") ? 9.f : 4.f));
		if (V.Part == PJaw)
		{
			// The slingshot: the jaw shoots forward 120 cm (the Tyrant's 300) and drops; the moray's inner jaw shows.
			if (Kind == TEXT("shark")) { P.X += JawOut * 120.f * Scale; P.Z -= (GapeAmt * 14.f + JawOut * 8.f) * Scale; }
			else P.X += (GapeAmt * 8.f + JawOut * 14.f) * Scale;
		}
		if (Kind == TEXT("eel") && V.Part != PJaw && V.A > 0.85f) P.Z += GapeAmt * (V.Rest.Z < 0.f ? -6.f : 3.f) * Scale;
	}
	else if (Kind == TEXT("crab"))
	{
		// Legs step in alternating fours (1,3 one side with 2,4 the other): the foot lifts and swings on an arc.
		if (V.Part >= PLeg0 && V.Part < PLeg0 + 8)
		{
			const int32 L = V.Part - PLeg0;
			const float Group = ((L % 4) + (L / 4)) % 2 ? 0.f : 0.5f;
			const float Step = FMath::Max(0.f, FMath::Sin((Clock + Group) * 2.f * PI));
			P.Z += Step * 30.f * Scale * Amp * V.B;
			P.X += FMath::Cos((Clock + Group) * 2.f * PI) * 20.f * Scale * Amp * V.B;
		}
		if (V.Part == PClawClub) P.X -= Cock * 30.f * Scale;                  // cocked back (the ratchet)
		if (V.Part == PClawCrush || V.Part == PClawClub) { P.Z += Guard * 30.f * Scale; P.Y *= 1.f - Guard * 0.4f; }
		// Buried: everything but the eyestalks sinks under the sand.
		if (V.Part != PEye) P.Z -= Bury * 140.f * Scale;
		else P.Z -= Bury * 40.f * Scale;
		if (Flip > 0.f) P = FRotator(0.f, 0.f, 180.f * Flip).RotateVector(P - FVector(0, 0, 70.f * Scale)) + FVector(0, 0, 70.f * Scale);
	}
	else if (Kind == TEXT("gull") || Kind == TEXT("puffin"))
	{
		if (V.Part == PFinL || V.Part == PFinR)
		{
			const float Out = FMath::Abs(P.Y);
			P.Z += FMath::Sin(Phase) * Out * 0.7f * Amp;
		}
		if (V.Part == PPouch) P *= FVector(1.f, 1.f + PouchAmt * 0.8f, 1.f + PouchAmt * 0.8f);
	}
	else if (Kind == TEXT("seahorse"))
	{
		// The tail stock drives (a dolphin's up-down stroke), the fronds trail and stream.
		if (V.Part == PTail) P.Z += Amp * 30.f * Scale * FMath::Sin(Phase - V.Rest.Z / (60.f * Scale));
		if (V.Part == PFinL) { P.Y += FMath::Sin(Time * 2.f + V.Rest.X * 0.02f) * 6.f * Scale; P.X -= Amp * 10.f * Scale; }
		if (V.Part == PBody && V.Rest.Z > 90.f * Scale) P.Z -= JawOut * 30.f * Scale * (V.Rest.X > 0.f ? 1.f : 0.f);   // the head tilts back
	}
	else if (Kind == TEXT("turtle"))
	{
		if (V.Part == PFinL || V.Part == PFinR) P.Z += FMath::Sin(Phase) * FMath::Abs(P.Y) * 0.35f * Amp;
		if (V.Part == PTail) P.Z += FMath::Sin(Phase + 1.f) * 10.f * Scale * Amp;
	}
	else if (Kind == TEXT("manta"))
	{
		// The manta's wave runs from the wing root to its tip.
		if (V.Part == PFinL || V.Part == PFinR) { const float Out = FMath::Abs(P.Y) / (200.f * Scale); P.Z += Amp * 90.f * Scale * Out * FMath::Sin(Phase - Out * 2.2f); }
		if (V.Part == PTail) P.Z += Cock * 60.f * Scale * (1.f - V.A);         // the barb arches over its back
	}
	else if (Kind == TEXT("otter"))
	{
		P.Z += FMath::Sin(Time * 1.4f) * 3.f * Scale;
		if (V.Part == PFinL) P.Z += FMath::Abs(FMath::Sin(Time * 6.f)) * 8.f * Scale * Amp;   // cracking a shell on its chest
	}
	else if (Kind == TEXT("school"))
	{
		if (V.Part >= PFish0)
		{
			const int32 F = V.Part - PFish0;
			const float Ph = Time * 3.f + F * 1.7f;
			const FVector Off(FMath::Sin(Ph) * 60.f, FMath::Cos(Ph * 0.8f) * 60.f, FMath::Sin(Ph * 1.3f) * 30.f);
			// Bait ball: the fish pull into a tight sphere and spin.
			const FVector Ball = FRotator(0.f, Time * 120.f + F * 12.f, 0.f).RotateVector(FVector(70.f, 0, (F % 5 - 2) * 25.f));
			P = FMath::Lerp(P + Off, Ball + (P - FVector(V.Rest.X, V.Rest.Y, V.Rest.Z)) * 0.2f + V.Rest * 0.05f, PouchAmt);
			P.Y += FMath::Sin(Time * 18.f + F) * 2.f;
		}
	}
	// Dead: belly up and still (the Glowray sinks flat).
	if (Dead > 0.f && Kind != TEXT("crab")) P = FRotator(0.f, 0.f, 180.f * Dead).RotateVector(P);
	return P;
}

void UWROceanBeast::Pose(float Dt)
{
	AWRCreature* C = Creature.Get();
	if (!C) return;
	const UWROceanBody* B = UWROceanBody::Get(C);
	const float Speed = C->GetVelocity().Size();
	const float Max = FMath::Max(100.f, C->GetCharacterMovement() ? C->GetCharacterMovement()->GetMaxSpeed() : 400.f);
	const float Want = FMath::Clamp(0.25f + Speed / Max, 0.15f, 1.3f);
	Amp = FMath::FInterpTo(Amp, Want, Dt, 2.f);
	Freq = Kind == TEXT("gull") || Kind == TEXT("puffin") ? 2.2f + Amp * 2.f : Kind == TEXT("crab") ? 0.4f + Speed / 200.f : Kind == TEXT("manta") ? 0.25f + Amp * 0.4f : 0.5f + Amp * 1.2f;
	Clock += Dt * Freq;
	Time += Dt;
	const auto Ease = [Dt](float& V, bool bOn, float Rate) { V = FMath::FInterpTo(V, bOn ? 1.f : 0.f, Dt, Rate); };
	if (B)
	{
		Ease(JawOut, B->Has(WROceanFlag::Jaw), 18.f);
		Ease(GapeAmt, B->Has(WROceanFlag::Gape), 6.f);
		Ease(Bury, B->Has(WROceanFlag::Buried), 3.f);
		Ease(Guard, B->Has(WROceanFlag::Guard), 8.f);
		Ease(Flip, B->Has(WROceanFlag::Flipped), 4.f);
		Ease(Cock, B->Has(WROceanFlag::Cocked), 10.f);
		Ease(Bands, B->Has(WROceanFlag::Bands), 8.f);
		Ease(Flare, B->Has(WROceanFlag::Flare), 8.f);
		Ease(PouchAmt, B->Has(WROceanFlag::Pouch) || B->Has(WROceanFlag::BaitBall), 4.f);
	}
	Ease(Dead, !C->IsAlive(), 1.5f);
	// Lights: brighter at night; the eel's organs on the pulse; the Glowray's flare and its ring's synced pulse.
	if (GlowMat)
	{
		const bool bNight = AWRGameState::IsNightIn(this);
		float G = Kind == TEXT("eel") ? Bands * 4.f : (bNight ? 1.2f : 0.25f) * (0.7f + 0.3f * FMath::Sin(Time * 2.f)) + Flare * 6.f;
		if (B && B->Has(WROceanFlag::Ring)) G *= 0.6f + 0.4f * FMath::Sin(UGameplayStatics::GetRealTimeSeconds(this) * 2.5f);
		if (!FMath::IsNearlyEqual(G, GlowNow, 0.02f)) { GlowNow = G; GlowMat->SetScalarParameterValue(TEXT("Glow"), G); }
	}
}

void UWROceanBeast::Commit(bool bCreate)
{
	for (FSection* S : { &Body, &Glow })
	{
		if (S->Rest.Num() == 0) continue;
		S->Verts.SetNum(S->Rest.Num());
		for (int32 i = 0; i < S->Rest.Num(); ++i) S->Verts[i] = Place(S->Rest[i]);
		const int32 Index = S == &Body ? 0 : 1;
		if (bCreate) CreateMeshSection(Index, S->Verts, S->Tris, S->Normals, S->UVs, S->Colors, TArray<FProcMeshTangent>(), false);
		else UpdateMeshSection(Index, S->Verts, S->Normals, S->UVs, S->Colors, TArray<FProcMeshTangent>());
	}
}

void UWROceanBeast::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bBuilt) return;
	Pose(DeltaTime);
	// Far from every camera it re-poses twice a second.
	PoseClock -= DeltaTime;
	const bool bNear = WasRecentlyRendered(0.2f);
	if (!bNear && PoseClock > 0.f) return;
	PoseClock = bNear ? 0.f : 0.5f;
	Commit(false);
}
