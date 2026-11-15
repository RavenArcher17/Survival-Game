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
	enum EPart : uint8 { PBody = 0, PJaw, PTail, PFinL, PFinR, PPouch, PEye, PClawCrush, PClawClub, PGlow, PHull, PCrest, PCoil, PLeg0 = 20, PFish0 = 60 };
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
			const bool bSiren = Creature.IsValid() && Creature->CreatureId == TEXT("siren_eel");
			const FLinearColor Col = bSiren ? FLinearColor(1.f, 0.42f, 0.16f) : Kind == TEXT("eel") ? FLinearColor(0.55f, 0.75f, 1.f) : FLinearColor(0.35f, 0.9f, 0.78f);
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
	const FName Who = Creature.IsValid() ? Creature->CreatureId : NAME_None;
	const bool bTyrant = Who == TEXT("reef_tyrant"), bSiren = Who == TEXT("siren_eel"), bBarnacle = Who == TEXT("old_barnacle");
	if (Kind == TEXT("shark"))
	{
		// A 6 m tiger shark: bronze-grey back with faded bars, bone-white belly, the long flat snout; the slingshot jaw a
		// part of its own under the snout; a scythe tail; two pectorals and a tall dorsal.
		FMark K = MarkOf(M);
		const FLinearColor Back = bTyrant ? Rgb(0x6A, 0x68, 0x60, 0.55f) : Rgb(0x5E, 0x5A, 0x4E, 0.55f), Belly = Rgb(0xEA, 0xE4, 0xD8, 0.65f);
		Spindle(M, -300.f * S, 300.f * S, { FVector(0.f, 8.f * S, 6.f * S), FVector(0.12f, 22.f * S, 26.f * S), FVector(0.45f, 62.f * S, 70.f * S), FVector(0.72f, 58.f * S, 60.f * S), FVector(0.9f, 30.f * S, 22.f * S), FVector(1.f, 10.f * S, 6.f * S) }, Back, 0.45f, 16);
		// Belly paint: the lower verts whitened; bars across the back.
		for (int32 i = K.V; i < M.Verts.Num(); ++i)
		{
			const FVector& P = M.Verts[i];
			FLinearColor C = P.Z < -10.f * S ? Belly : Back;
			if (P.Z > 0.f && FMath::Fmod(FMath::Abs(P.X) / (40.f * S), 2.f) < 0.5f) C *= bTyrant ? 0.93f : 0.8f;
			// The Tyrant's snout scarred white in long seams.
			if (bTyrant && P.X > 200.f * S && FMath::Frac(P.Y / (9.f * S) + P.X / (60.f * S)) < 0.12f) C = Rgb(0xE8, 0xE4, 0xDA, 0.7f);
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
		if (bTyrant)
		{
			// A Saltkin harpoon still in the jaw hinge, its shaft snapped, a length of bronze chain trailing from it.
			Limb(M, FVector(200.f * S, 38.f * S, -14.f * S), FVector(150.f * S, 70.f * S, 30.f * S), 3.f * S, 2.5f * S, Rgb(0x5A, 0x44, 0x2E, 0.5f));
			Limb(M, FVector(205.f * S, 36.f * S, -16.f * S), FVector(215.f * S, 30.f * S, -18.f * S), 4.f * S, 0.5f * S, Rgb(0x9A, 0x6E, 0x3A, 0.4f));
			TArray<FVector> Chain;
			for (int32 c = 0; c <= 14; ++c) { const float A = c / 14.f; Chain.Add(FVector(200.f * S - A * 300.f * S, 40.f * S + FMath::Sin(A * 5.f) * 10.f * S, -20.f * S - A * 60.f * S + FMath::Sin(A * PI) * -20.f * S)); }
			M.Tube(Chain, 2.5f * S, 5, Rgb(0x8A, 0x6A, 0x3A, 0.45f), 0.35f);
			// The saw-notched dorsal: notches cut in its trailing edge (dark).
			for (int32 n = 0; n < 5; ++n) Ball(M, FVector(-40.f * S - n * 8.f * S, 0, 90.f * S + n * 10.f * S), 4.f * S, Back * 0.4f, 0.8f, 1.f, 5);
		}
		Take(M, K.V, K.T, Body, PBody);
	}
	else if (Kind == TEXT("eel"))
	{
		// A 3.5 m moray ribbon (the Siren's scale x5): honeycomb olive and yellow, a fat gaping head; its banded organs
		// along the flanks (lit on the shock pulse, section 1); the pharyngeal inner jaw (pale) in the throat.
		const float L = 350.f * S;
		FMark K = MarkOf(M);
		Spindle(M, -L * 0.5f, L * 0.5f, { FVector(0.f, 3.f * S, 10.f * S), FVector(0.2f, 14.f * S, 20.f * S), FVector(0.7f, 18.f * S, 22.f * S), FVector(0.9f, 20.f * S, 22.f * S), FVector(1.f, 9.f * S, 8.f * S) }, bSiren ? Rgb(0xC8, 0xCC, 0xD4, 0.75f) : Rgb(0x6A, 0x6A, 0x2E, 0.5f), bSiren ? 0.12f : 0.35f, 12);
		if (bSiren)
		{
			// An oarfish: flat as a blade (squeezed across), silver mirror scales, dark bars down the silver.
			for (int32 i = K.V; i < M.Verts.Num(); ++i)
			{
				M.Verts[i].Y *= 0.45f;
				if (FMath::Frac(M.Verts[i].X / (40.f * S)) < 0.12f) M.Colors[i] = FWRToolMesh::Pack(Rgb(0x3A, 0x3E, 0x48, 0.6f), 0.2f);
			}
		}
		else for (int32 i = K.V; i < M.Verts.Num(); ++i) if (FMath::Frac(M.Verts[i].X / (18.f * S) + M.Verts[i].Z / (30.f * S)) < 0.35f) M.Colors[i] = FWRToolMesh::Pack(Rgb(0xC8, 0xB8, 0x60, 0.55f), 0.35f);
		Take(M, K.V, K.T, Body, PBody); K = MarkOf(M);
		Fin(M, FVector(L * 0.4f, 0, 18.f * S), FVector(-L * 0.5f, 0, 8.f * S), FVector(-L * 0.1f, 0, 26.f * S), 3.f * S, Rgb(0x5A, 0x5A, 0x26, 0.5f));
		Take(M, K.V, K.T, Body, PBody); K = MarkOf(M);
		Spindle(M, L * 0.42f, L * 0.5f + 6.f * S, { FVector(0.f, 10.f * S, 8.f * S), FVector(1.f, 4.f * S, 3.f * S) }, Rgb(0xF0, 0xE8, 0xD8, 0.7f), 0.3f, 8);
		for (int32 i = K.V; i < M.Verts.Num(); ++i) M.Verts[i].Z -= 6.f * S;
		Take(M, K.V, K.T, Body, PJaw); K = MarkOf(M);
		Ball(M, FVector(L * 0.46f, 14.f * S, 12.f * S), 2.5f * S, FLinearColor(0.02f, 0.02f, 0.01f), 0.1f);
		Ball(M, FVector(L * 0.46f, -14.f * S, 12.f * S), 2.5f * S, FLinearColor(0.02f, 0.02f, 0.01f), 0.1f);
		Take(M, K.V, K.T, Body, PBody);
		// The organs: a row of pale bands each side (the glow section). The Siren has her crest there instead: long
		// scarlet-orange rays rising from the head like a mane, blazing when she sings.
		FWRToolMesh G;
		if (bSiren)
		{
			for (int32 r = 0; r < 11; ++r)
			{
				const float X = L * 0.47f - r * L * 0.012f;
				const FVector Dir = FRotator(70.f - r * 4.f, 180.f, 0.f).Vector();
				const FMark Cr = MarkOf(G);
				G.Lathe(FVector(X, 0, 18.f * S), Dir, (60.f - r * 2.5f) * S, { FVector(0.f, 1.6f * S, 1.6f * S), FVector(0.85f, 1.f * S, 1.f * S), FVector(1.f, 2.5f * S, 2.5f * S) }, 5, FLinearColor::White, 0.3f);
				(void)Cr;
			}
			Take(G, 0, 0, Glow, PCrest);
		}
		for (int32 b = 0; b < (bSiren ? 0 : 12); ++b)
		{
			const float X = -L * 0.4f + b * L * 0.065f;
			for (int32 Sd = -1; Sd <= 1; Sd += 2)
			{
				const int32 From = G.Verts.Num();
				Spindle(G, X, X + 8.f * S, { FVector(0.f, 3.f * S, 6.f * S), FVector(1.f, 3.f * S, 6.f * S) }, FLinearColor::White, 0.3f, 6);
				for (int32 i = From; i < G.Verts.Num(); ++i) G.Verts[i].Y += Sd * 16.f * S;
			}
		}
		if (!bSiren) Take(G, 0, 0, Glow, PGlow);
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
		if (bBarnacle)
		{
			// (His club is the crusher's twin here: a hermit crab has two crushers; the boss draws the big one.)
			K = MarkOf(M);
			const FLinearColor Timber = Rgb(0x5A, 0x44, 0x30, 0.5f);
			M.Lathe(FVector(-180.f, 0, 120.f), FVector(1, 0, 0), 380.f, { FVector(0.f, 30.f, 30.f), FVector(0.15f, 110.f, 110.f), FVector(0.6f, 130.f, 120.f), FVector(0.9f, 90.f, 90.f), FVector(1.f, 20.f, 30.f) }, 16, Timber, 0.8f);
			for (int32 i = K.V; i < M.Verts.Num(); ++i)
			{
				// Keel up: the lower half flattened to the gunwale (the hull rides over his back).
				FVector& P = M.Verts[i];
				if (P.Z < 120.f) P.Z = 120.f - (120.f - P.Z) * 0.08f;
				// Planks: darker seams every 22 cm; barnacles crusting the lower strakes.
				if (FMath::Frac((P.Z - 120.f) / 22.f) < 0.12f) M.Colors[i] = FWRToolMesh::Pack(Timber * 0.6f, 0.9f);
				if (P.Z < 150.f && R.FRand() < 0.25f) M.Colors[i] = FWRToolMesh::Pack(Rgb(0xD8, 0xD0, 0xBE, 0.6f), 0.9f);
			}
			// The keel, the broken mast stub out of its side, a dangling anchor on a rope, a torn net.
			Limb(M, FVector(-190.f, 0, 250.f), FVector(200.f, 0, 252.f), 8.f, 8.f, Timber * 0.8f);
			Limb(M, FVector(20.f, 70.f, 180.f), FVector(60.f, 170.f, 160.f), 10.f, 8.f, Timber * 0.9f);
			Limb(M, FVector(120.f, -110.f, 130.f), FVector(130.f, -120.f, 40.f), 1.5f, 1.5f, Rgb(0xA8, 0x98, 0x70, 0.6f));
			Limb(M, FVector(110.f, -120.f, 40.f), FVector(150.f, -120.f, 40.f), 4.f, 4.f, Rgb(0x3A, 0x34, 0x30, 0.4f));
			for (int32 b = 0; b < 40; ++b) Ball(M, FVector(R.FRandRange(-170.f, 190.f), R.FRandRange(-120.f, 120.f), R.FRandRange(125.f, 200.f)), R.FRandRange(5.f, 11.f), Rgb(0xD8, 0xD0, 0xBE, 0.6f), 0.9f, 0.6f, 6);
			// Built at the crab's own size, scaled with him (x2.5).
			for (int32 i = K.V; i < M.Verts.Num(); ++i) M.Verts[i] *= S;
			Take(M, K.V, K.T, Body, PHull);
			// His long red-and-cream striped legs.
			for (int32 i = 0; i < Body.Rest.Num(); ++i) if (Body.Rest[i].Part >= PLeg0 && Body.Rest[i].Part < PLeg0 + 8 && FMath::Frac(Body.Rest[i].Rest.Size() / (30.f * S)) < 0.4f) Body.Colors[i] = FWRToolMesh::Pack(Rgb(0xEA, 0xDF, 0xC8, 0.65f), 0.6f);
		}
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
	else if (Kind == TEXT("leviathan"))
	{
		// Thalassor (OCEAN 2.14): 52 m. The front 14 m a sperm whale's head (a battering-ram block of a skull, an underslung
		// jaw of 40 ivory cones, a small eye far back, the blowhole off to the left); behind the pectorals a serpent's 38 m
		// with a low ridge bristling with broken Saltkin harpoons; a 9 m horizontal fluke. Slate grey-black corrugated skin,
		// white circular sucker scars and long pale harpoon scars on the head; bronze collar chains; 14 photophores a side.
		const FLinearColor Skin = Rgb(0x2A, 0x2C, 0x30, 0.5f), Scar = Rgb(0xD8, 0xD6, 0xCE, 0.6f);
		FMark K = MarkOf(M);
		// The head: a squared block (wide lathe, flattened sides), 14 m long, 5 m tall.
		M.Lathe(FVector(-100.f, 0, 0), FVector(1, 0, 0), 1400.f, { FVector(0.f, 200.f, 220.f), FVector(0.2f, 230.f, 250.f), FVector(0.7f, 240.f, 260.f), FVector(0.95f, 220.f, 240.f), FVector(1.f, 150.f, 180.f) }, 18, Skin, 0.75f);
		for (int32 i = K.V; i < M.Verts.Num(); ++i)
		{
			FVector& P = M.Verts[i];
			// Squared off (a block, not a barrel), wrinkled, scarred.
			P.Y = FMath::Sign(P.Y) * FMath::Pow(FMath::Abs(P.Y) / 240.f, 0.6f) * 240.f;
			P.Z += FMath::Sin(P.X * 0.05f) * 6.f;
			const float Ring = FMath::Frac(P.X / 170.f + P.Z / 130.f);
			if (P.X > 200.f && (Ring < 0.06f || (R.FRand() < 0.03f))) M.Colors[i] = FWRToolMesh::Pack(Scar, 0.7f);
		}
		// The blowhole (left of centre), the small eyes far back.
		Ball(M, FVector(1150.f, -60.f, 250.f), 30.f, Skin * 0.5f, 0.6f, 0.5f);
		for (int32 e = -1; e <= 1; e += 2) Ball(M, FVector(300.f, e * 235.f, 40.f), 14.f, FLinearColor(0.02f, 0.02f, 0.02f), 0.1f);
		// Collar rings and their snapped bronze chains behind the head.
		for (int32 c = -1; c <= 1; c += 2)
		{
			TArray<FVector> Ch;
			for (int32 k = 0; k <= 12; ++k) { const float A = k / 12.f; Ch.Add(FVector(-60.f - A * 400.f, c * (230.f + A * 40.f), 120.f - A * 300.f + FMath::Sin(A * PI) * -60.f)); }
			M.Tube(Ch, 8.f, 6, Rgb(0x8A, 0x6A, 0x3A, 0.45f), 0.35f);
		}
		Take(M, K.V, K.T, Body, PBody); K = MarkOf(M);
		// The underslung lower jaw (narrow), 40 ivory cones.
		M.Lathe(FVector(250.f, 0, -200.f), FVector(1, 0, 0), 1000.f, { FVector(0.f, 90.f, 50.f), FVector(0.8f, 70.f, 40.f), FVector(1.f, 30.f, 25.f) }, 12, Skin * 0.85f, 0.7f);
		for (int32 t = 0; t < 40; ++t)
		{
			const int32 Sd = t % 2 ? 1 : -1;
			M.Lathe(FVector(350.f + (t / 2) * 45.f, Sd * 55.f, -165.f), FVector(0, 0, 1), 40.f, { FVector(0.f, 9.f, 9.f), FVector(1.f, 1.f, 1.f) }, 5, Rgb(0xF2, 0xEC, 0xDC, 0.8f), 0.3f);
		}
		Take(M, K.V, K.T, Body, PJaw); K = MarkOf(M);
		// The pectorals.
		Fin(M, FVector(100.f, 220.f, -120.f), FVector(-50.f, 220.f, -120.f), FVector(-150.f, 520.f, -200.f), 20.f, Skin);
		Fin(M, FVector(100.f, -220.f, -120.f), FVector(-50.f, -220.f, -120.f), FVector(-150.f, -520.f, -200.f), 20.f, Skin);
		Take(M, K.V, K.T, Body, PBody); K = MarkOf(M);
		// The serpent: 38 m tapering, its ridge of broken harpoons (dark shafts, bronze heads).
		Spindle(M, -3900.f, -80.f, { FVector(0.f, 40.f, 40.f), FVector(0.3f, 110.f, 120.f), FVector(0.8f, 180.f, 200.f), FVector(1.f, 210.f, 230.f) }, Skin, 0.7f, 16);
		for (int32 h = 0; h < 26; ++h)
		{
			const float X = -300.f - h * 135.f;
			const float Rr = FMath::Lerp(220.f, 60.f, h / 26.f);
			Limb(M, FVector(X, 0, Rr - 10.f), FVector(X - 40.f, (h % 3 - 1) * 20.f, Rr + 140.f + (h % 4) * 20.f), 5.f, 4.f, Rgb(0x4A, 0x3A, 0x2C, 0.5f));
			if (h % 2 == 0) Limb(M, FVector(X - 40.f, (h % 3 - 1) * 20.f, Rr + 140.f + (h % 4) * 20.f), FVector(X - 50.f, (h % 3 - 1) * 20.f, Rr + 175.f + (h % 4) * 20.f), 9.f, 0.5f, Rgb(0x9A, 0x6E, 0x3A, 0.4f));
		}
		Take(M, K.V, K.T, Body, PCoil); K = MarkOf(M);
		// The fluke: 9 m across, horizontal.
		Fin(M, FVector(-3880.f, 0, 0), FVector(-3950.f, 0, 0), FVector(-4350.f, 450.f, 0), 30.f, Skin);
		Fin(M, FVector(-3880.f, 0, 0), FVector(-3950.f, 0, 0), FVector(-4350.f, -450.f, 0), 30.f, Skin);
		Take(M, K.V, K.T, Body, PTail);
		// The photophores: 14 a side along the flanks (the glow section).
		FWRToolMesh G;
		for (int32 p = 0; p < 14; ++p)
		{
			const float X = -200.f - p * 260.f;
			const float Rr = FMath::Lerp(200.f, 60.f, p / 14.f);
			for (int32 Sd = -1; Sd <= 1; Sd += 2) Ball(G, FVector(X, Sd * Rr * 0.95f, -Rr * 0.2f), 14.f, FLinearColor::White, 0.3f, 0.6f, 6);
		}
		Take(G, 0, 0, Glow, PGlow);
		for (FV& V : Body.Rest) V.Rest *= S;
		for (FV& V : Glow.Rest) V.Rest *= S;
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
		// The Siren's crest streams back, and stands up flaring when she sings.
		if (V.Part == PCrest) { P.Z += Flare * 18.f * Scale * FMath::Sin(Time * 9.f + V.Rest.Z * 0.1f); P.X += Flare * 6.f * Scale; }
	}
	else if (Kind == TEXT("leviathan"))
	{
		const float Behind = FMath::Clamp((-V.Rest.X / Scale - 100.f) / 4300.f, 0.f, 1.f);   // 0 at the pectorals, 1 at the fluke
		P.Y += Amp * 260.f * Scale * Behind * FMath::Sin(Phase - Behind * 6.f);
		if (V.Part == PTail) P.Z += Amp * 160.f * Scale * FMath::Sin(Phase * 0.8f);
		if (V.Part == PJaw) P = FRotator(-(GapeAmt * 28.f + JawOut * 12.f), 0.f, 0.f).RotateVector(P - FVector(250.f * Scale, 0, -180.f * Scale)) + FVector(250.f * Scale, 0, -180.f * Scale);
		// Rearing (the Sluice Breaks' tell, the Flare): the head and the front of the trunk lift 16 m.
		if (Flare > 0.f && V.Rest.X > -1500.f * Scale) P.Z += Flare * 1100.f * Scale * FMath::Clamp((V.Rest.X / Scale + 1500.f) / 2700.f, 0.f, 1.f);
		// Beached: rolled on its side.
		if (Flip > 0.f) P = FRotator(0.f, 0.f, 70.f * Flip).RotateVector(P);
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
		// Withdrawn into the hull (Hull fortress): legs and claws drawn up under it.
		if (Guard > 0.f && V.Part >= PLeg0 && V.Part < PLeg0 + 8) P.Z += Guard * 60.f * Scale * V.B;
		// Out of the hull (naked) the hull's gone from his back (the boss drops the hull part: Bands flag).
		if (V.Part == PHull && Bands > 0.5f) P = FVector(0.f, 0.f, -100000.f);
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
	Freq = Kind == TEXT("leviathan") ? 0.12f + Amp * 0.15f : Kind == TEXT("gull") || Kind == TEXT("puffin") ? 2.2f + Amp * 2.f : Kind == TEXT("crab") ? 0.4f + Speed / 200.f : Kind == TEXT("manta") ? 0.25f + Amp * 0.4f : 0.5f + Amp * 1.2f;
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
		const bool bSirenGlow = Creature.IsValid() && Creature->CreatureId == TEXT("siren_eel");
		float G = bSirenGlow ? 0.4f + Flare * 7.f : Kind == TEXT("eel") ? Bands * 4.f : Kind == TEXT("leviathan") ? 1.0f + 0.4f * FMath::Sin(Time * 0.8f) + Flare * 8.f
			: (bNight ? 1.2f : 0.25f) * (0.7f + 0.3f * FMath::Sin(Time * 2.f)) + Flare * 6.f;
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
