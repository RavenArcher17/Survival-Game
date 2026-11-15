// The Desert's creature bodies built in code (cooks/biome-desert.md 2.12.1; build-biome-desert.md 2.12). Every vertex is
// stored at rest with its part and its place along the body, and re-placed every frame from how the real animal moves:
// a deathstalker's eight legs step in alternating fours while its tail arches over its back and cocks the sunglass bulb;
// a sandfish skink swims with a sideways S-wave and dives until only its grit fin cuts the sand; a thorny devil rocks in
// its jerky walk, tucks its real head under the false-head hump and curls into a spiked ball; a dromedary paces (both legs
// of a side together) with its long neck bobbing and blows its pink dulla; an oryx trots on diagonal pairs; a dung beetle
// walks backwards pushing its ball with its hind legs; a sandgrouse bobs and flaps; a water-holding frog squats and hops.
#include "WRDesertCreatures.h"
#include "Wildreach.h"
#include "WRCreature.h"
#include "WRGameState.h"
#include "WRToolMesh.h"
#include "WRTuning.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace WRDesertBeastImpl
{
	enum EPart : uint8 { PBody = 0, PHead, PJaw, PFin, PHump, PSac, PHorns, PBall, PWingL, PWingR, PNeck, PBulb, PPincerL, PPincerR, PFingerL, PFingerR, PSpines, PTail0 = 20, PLeg0 = 40 };
	FLinearColor Dbc(uint8 R, uint8 G, uint8 B, float K = 0.6f) { return FLinearColor(FColor(R, G, B)) * K; }
	void Take(FWRToolMesh& M, int32 FromV, int32 FromT, UWRDesertBeast::FSection& S, uint8 Part, float B = 0.f)
	{
		const int32 Base = S.Rest.Num();
		for (int32 i = FromV; i < M.Verts.Num(); ++i)
		{
			UWRDesertBeast::FV V; V.Rest = M.Verts[i]; V.Part = Part; V.B = B;
			S.Rest.Add(V);
			S.Colors.Add(M.Colors.IsValidIndex(i) ? M.Colors[i] : FColor::White);
			S.UVs.Add(M.UVs.IsValidIndex(i) ? M.UVs[i] : FVector2D::ZeroVector);
			S.Normals.Add(M.Normals.IsValidIndex(i) ? M.Normals[i] : FVector::UpVector);
		}
		for (int32 t = FromT; t < M.Tris.Num(); ++t) S.Tris.Add(M.Tris[t] - FromV + Base);
	}
	struct FMk { int32 V = 0, T = 0; };
	FMk Mk(const FWRToolMesh& M) { return { M.Verts.Num(), M.Tris.Num() }; }
	void Seg(FWRToolMesh& M, const FVector& A, const FVector& B, float R0, float R1, const FLinearColor& C, float Rough = 0.6f, int32 Sides = 7)
	{
		const FVector D = B - A; const float L = D.Size(); if (L < 0.5f) return;
		M.Lathe(A, D / L, L, { FVector(0.f, R0, R0), FVector(1.f, R1, R1) }, Sides, C, Rough);
	}
	void Blob(FWRToolMesh& M, const FVector& C, float Rx, float Ry, float Rz, const FLinearColor& Col, float Rough = 0.6f, int32 Sides = 10)
	{
		TArray<FVector> P;
		for (int32 i = 0; i <= 6; ++i) { const float A = PI * i / 6.f; P.Add(FVector(i / 6.f, FMath::Max(0.4f, Ry * FMath::Sin(A)), FMath::Max(0.4f, Rz * FMath::Sin(A)))); }
		M.Lathe(C - FVector(Rx, 0, 0), FVector(1, 0, 0), 2.f * Rx, P, Sides, Col, Rough);
	}
	void Cone(FWRToolMesh& M, const FVector& Base, const FVector& Tip, float R, const FLinearColor& C, float Rough = 0.4f)
	{
		const FVector D = Tip - Base; const float L = D.Size(); if (L < 0.5f) return;
		M.Lathe(Base, D / L, L, { FVector(0.f, R, R), FVector(1.f, 0.3f, 0.3f) }, 5, C, Rough);
	}
	/** A jointed leg from Hip through Knee to Foot (three tapered segments), every vertex tagged with the leg and B = how
	 *  far down the leg (0 hip .. 1 foot) so the step lifts the lower leg most. */
	void Leg(FWRToolMesh& M, UWRDesertBeast::FSection& S, uint8 Part, const FVector& Hip, const FVector& Knee, const FVector& Ankle, const FVector& Foot, float R, const FLinearColor& C)
	{
		FMk K = Mk(M);
		Seg(M, Hip, Knee, R, R * 0.8f, C, 0.55f, 6);
		Take(M, K.V, K.T, S, Part, 0.25f); K = Mk(M);
		Seg(M, Knee, Ankle, R * 0.8f, R * 0.6f, C, 0.55f, 6);
		Take(M, K.V, K.T, S, Part, 0.6f); K = Mk(M);
		Seg(M, Ankle, Foot, R * 0.6f, R * 0.35f, C * 0.9f, 0.55f, 5);
		Take(M, K.V, K.T, S, Part, 1.f);
	}
}
using namespace WRDesertBeastImpl;

UWRDesertBeast::UWRDesertBeast(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetCastShadow(true);
}

void UWRDesertBeast::Init(AWRCreature* InOwner, FName InKind, float InScale, int32 InSeed)
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
			// The Glasstail's moonlit cyan; the Gildback's amber spines.
			const FLinearColor Col = Kind == TEXT("scorpion") ? FLinearColor(0.35f, 1.f, 0.85f) : FLinearColor(1.f, 0.62f, 0.2f);
			GlowMat->SetVectorParameterValue(TEXT("Color"), Col);
			GlowMat->SetScalarParameterValue(TEXT("Glow"), 0.f);
			SetMaterial(1, GlowMat);
		}
	}
	bBuilt = true;
}

void UWRDesertBeast::InitMirage(AWRCreature* Of, FName InKind, float InScale, int32 InSeed, float Seconds)
{
	bMirage = true;
	MirageLeft = Seconds;
	Init(Of, InKind, InScale, InSeed);
	Creature = nullptr;              // it never follows its creature: a still image where it stood
	SetCastShadow(false);            // only the real one casts a shadow (cook 2.13.2)
	Commit(false);
}

void UWRDesertBeast::Build()
{
	FWRToolMesh M;
	FRandomStream R(Seed);
	const float S = Scale;
	FMk K = Mk(M);
	if (Kind == TEXT("scorpion"))
	{
		// A Glasstail (cook 2.12.1: a deathstalker 2.2 m long, 0.9 m tall): low and wide, sand-coloured armour in plates,
		// pincers held high, the segmented tail arched over its back with a gold-green sunglass bulb at the sting.
		const FLinearColor Arm = Dbc(0xC8, 0xA8, 0x78, 0.55f), Dark = Dbc(0x6E, 0x58, 0x3E, 0.55f), Glass = Dbc(0xC8, 0xC0, 0x60, 0.65f);
		const float H = 42.f;
		// The cephalothorax and the seven plates of the mesosoma (each a little wider, then narrowing).
		Blob(M, FVector(60.f, 0, H), 34.f, 30.f, 14.f, Arm, 0.45f);
		for (int32 p = 0; p < 7; ++p) Blob(M, FVector(28.f - p * 14.f, 0, H + 2.f), 9.f, 34.f - FMath::Abs(p - 2) * 3.f, 12.f, p % 2 ? Arm : Arm * 0.92f, 0.45f);
		Blob(M, FVector(86.f, 14.f, H + 12.f), 3.f, 3.f, 3.f, FLinearColor(0.01f, 0.01f, 0.01f), 0.1f);
		Blob(M, FVector(86.f, -14.f, H + 12.f), 3.f, 3.f, 3.f, FLinearColor(0.01f, 0.01f, 0.01f), 0.1f);
		Take(M, K.V, K.T, Body, PBody); K = Mk(M);
		// The night stripes along the plates' edges (fluorescence: the glow section).
		FWRToolMesh G;
		for (int32 p = 0; p < 7; ++p) Seg(G, FVector(28.f - p * 14.f, -32.f, H + 8.f), FVector(28.f - p * 14.f, 32.f, H + 8.f), 1.6f, 1.6f, FLinearColor::White, 0.3f, 4);
		Take(G, 0, 0, Glow, PBody);
		// The tail: six segments from the last plate up over the back, each its own part so it arches and strikes.
		FVector Root(-74.f, 0, H + 6.f);
		for (int32 t = 0; t < 6; ++t)
		{
			const float A = FMath::DegreesToRadians(70.f + t * 28.f);
			const FVector Next = Root + FVector(-FMath::Cos(A) * 24.f, 0.f, FMath::Sin(A) * 24.f) * (t < 5 ? 1.f : 0.8f);
			Seg(M, Root, Next, 12.f - t * 1.2f, 11.f - t * 1.2f, t % 2 ? Arm : Arm * 0.92f, 0.45f, 8);
			Take(M, K.V, K.T, Body, uint8(PTail0 + t), float(t) / 5.f); K = Mk(M);
			Root = Next;
		}
		// The bulb (sunglass, a sun-catching glint) and the barb.
		Blob(M, Root + FVector(4.f, 0, 0), 11.f, 9.f, 9.f, Glass, 0.08f);
		Cone(M, Root + FVector(12.f, 0, -4.f), Root + FVector(26.f, 0, -18.f), 3.5f, Dark, 0.3f);
		Take(M, K.V, K.T, Body, PBulb, 1.f); K = Mk(M);
		// Pincers: an arm out and forward to a heavy chela with a movable finger, held high.
		for (int32 sd = -1; sd <= 1; sd += 2)
		{
			const FVector Sh(80.f, sd * 22.f, H + 2.f), El(112.f, sd * 52.f, H + 14.f), Wr(138.f, sd * 40.f, H + 20.f);
			Seg(M, Sh, El, 6.f, 5.f, Arm, 0.5f); Seg(M, El, Wr, 5.f, 6.f, Arm, 0.5f);
			Blob(M, Wr + FVector(14.f, 0, 0), 16.f, 9.f, 8.f, Arm * 0.95f, 0.45f);
			Cone(M, Wr + FVector(26.f, sd * -3.f, -2.f), Wr + FVector(46.f, sd * -6.f, -2.f), 4.f, Dark, 0.4f);   // the fixed finger
			Take(M, K.V, K.T, Body, sd < 0 ? PPincerL : PPincerR); K = Mk(M);
			Cone(M, Wr + FVector(24.f, sd * 5.f, 2.f), Wr + FVector(44.f, sd * 8.f, 2.f), 3.6f, Dark, 0.4f);      // the movable finger
			Take(M, K.V, K.T, Body, sd < 0 ? PFingerL : PFingerR);
			K = Mk(M);
		}
		// Eight walking legs, four a side, each jointed down to the sand.
		for (int32 l = 0; l < 8; ++l)
		{
			const int32 sd = l < 4 ? -1 : 1, i = l % 4;
			const float X = 54.f - i * 22.f;
			Leg(M, Body, uint8(PLeg0 + l), FVector(X, sd * 24.f, H - 4.f), FVector(X + 4.f, sd * 56.f, H + 14.f), FVector(X + 2.f - i * 4.f, sd * 76.f, H - 10.f), FVector(X - i * 6.f, sd * 86.f, 0.f), 3.6f, Arm * 0.9f);
			K = Mk(M);
		}
	}
	else if (Kind == TEXT("skink"))
	{
		// A Dunejaw (cook 2.12.1: a sandfish skink 6 m long with a shark's grin): a flat body on four paddle limbs, a shovel
		// snout, eyes high on the head, a wide grin of needle teeth, a tapering tail, and the sail-fin of grit scales.
		const FLinearColor Hide = Dbc(0xC8, 0x9A, 0x60, 0.55f), Belly = Dbc(0xE8, 0xD8, 0xB8, 0.62f), Grit = Dbc(0x8A, 0x6E, 0x4A, 0.55f);
		const float H = 50.f;
		M.Lathe(FVector(-300.f, 0, H), FVector(1, 0, 0), 600.f, { FVector(0.f, 3.f, 2.f), FVector(0.25f, 24.f, 16.f), FVector(0.5f, 52.f, 34.f), FVector(0.7f, 58.f, 36.f), FVector(0.86f, 44.f, 26.f), FVector(0.96f, 30.f, 14.f), FVector(1.f, 18.f, 6.f) }, 14, Hide, 0.5f);
		for (int32 i = K.V; i < M.Verts.Num(); ++i)
		{
			const FVector& P = M.Verts[i];
			FLinearColor C = P.Z < H - 8.f ? Belly : Hide;
			if (P.Z > H && FMath::Frac(P.X / 38.f) < 0.18f) C *= 0.82f;   // dark bands across the back
			M.Colors[i] = FWRToolMesh::Pack(C, 0.5f);
		}
		Blob(M, FVector(250.f, 22.f, H + 30.f), 6.f, 6.f, 6.f, Dbc(0x10, 0x0C, 0x08, 0.6f), 0.1f);
		Blob(M, FVector(250.f, -22.f, H + 30.f), 6.f, 6.f, 6.f, Dbc(0x10, 0x0C, 0x08, 0.6f), 0.1f);
		Take(M, K.V, K.T, Body, PBody); K = Mk(M);
		// The lower jaw (opens on the bite) and its needle teeth in a wide grin.
		M.Lathe(FVector(200.f, 0, H - 14.f), FVector(1, 0, 0), 100.f, { FVector(0.f, 34.f, 8.f), FVector(0.7f, 26.f, 7.f), FVector(1.f, 10.f, 4.f) }, 10, Belly * 0.95f, 0.5f);
		for (int32 t = 0; t < 22; ++t) { const float A = (t / 21.f - 0.5f) * 2.4f; Cone(M, FVector(282.f - FMath::Abs(A) * 46.f, FMath::Sin(A) * 26.f, H - 8.f), FVector(282.f - FMath::Abs(A) * 46.f, FMath::Sin(A) * 26.f, H + 2.f), 2.f, Dbc(0xF4, 0xF0, 0xE4, 0.8f), 0.2f); }
		Take(M, K.V, K.T, Body, PJaw); K = Mk(M);
		// The sail-fin: grit scales in a ragged crest along the back, tallest over the shoulders.
		for (int32 f = 0; f < 14; ++f)
		{
			const float X = 140.f - f * 22.f;
			const float FH = 30.f + 70.f * FMath::Sin(PI * (1.f - f / 14.f)) * (f < 2 ? 0.6f : 1.f);
			Cone(M, FVector(X, 0, H + 30.f), FVector(X - 14.f, 0, H + 30.f + FH), 9.f, Grit * (0.85f + 0.3f * R.FRand()), 0.9f);
			Cone(M, FVector(X - 10.f, 0, H + 28.f), FVector(X - 20.f, 0, H + 20.f + FH * 0.7f), 7.f, Grit, 0.9f);
		}
		Take(M, K.V, K.T, Body, PFin); K = Mk(M);
		// Four paddle limbs (short, flattened), splayed.
		for (int32 l = 0; l < 4; ++l)
		{
			const int32 sd = l % 2 ? 1 : -1;
			const float X = l < 2 ? 130.f : -90.f;
			Leg(M, Body, uint8(PLeg0 + l), FVector(X, sd * 40.f, H - 6.f), FVector(X + 10.f, sd * 80.f, H - 14.f), FVector(X + 20.f, sd * 104.f, H - 34.f), FVector(X + 34.f, sd * 116.f, 0.f), 10.f, Hide * 0.95f);
			K = Mk(M);
		}
	}
	else if (Kind == TEXT("devil"))
	{
		// A Gildback (cook 2.12.1: a thorny devil 3 m long, 1.6 m tall): squat and armoured, covered in amber spines that
		// store sunlight, a false-head hump behind its neck, a small real head, four sprawled legs, a thick tail.
		const FLinearColor Hide = Dbc(0xB8, 0x86, 0x4E, 0.55f), Spine = Dbc(0xE8, 0xA0, 0x3A, 0.6f);
		const float H = 70.f;
		Blob(M, FVector(0, 0, H), 110.f, 70.f, 50.f, Hide, 0.6f);
		Take(M, K.V, K.T, Body, PBody); K = Mk(M);
		Blob(M, FVector(95.f, 0, H + 44.f), 34.f, 32.f, 30.f, Hide * 1.05f, 0.6f);      // the false-head hump
		Take(M, K.V, K.T, Body, PHump); K = Mk(M);
		Blob(M, FVector(130.f, 0, H + 2.f), 26.f, 20.f, 18.f, Hide * 0.95f, 0.6f);      // the real head
		Blob(M, FVector(146.f, 12.f, H + 10.f), 3.5f, 3.5f, 3.5f, Dbc(0x20, 0x18, 0x10, 0.6f), 0.1f);
		Blob(M, FVector(146.f, -12.f, H + 10.f), 3.5f, 3.5f, 3.5f, Dbc(0x20, 0x18, 0x10, 0.6f), 0.1f);
		Take(M, K.V, K.T, Body, PHead); K = Mk(M);
		// Spines all over the back and the hump (the glow section copies their tips: they shine amber at night).
		FWRToolMesh G;
		for (int32 s = 0; s < 70; ++s)
		{
			const float U = R.FRandRange(-0.95f, 0.95f), V = R.FRandRange(0.f, PI);
			const FVector N(U, FMath::Cos(V) * FMath::Sqrt(1.f - U * U), FMath::Sin(V) * FMath::Sqrt(1.f - U * U));
			const FVector Base = FVector(0, 0, H) + FVector(N.X * 105.f, N.Y * 66.f, N.Z * 48.f);
			const float L = R.FRandRange(14.f, 26.f);
			Cone(M, Base, Base + N * L, 4.5f, Spine * R.FRandRange(0.85f, 1.1f), 0.35f);
			Cone(G, Base + N * L * 0.55f, Base + N * L, 2.4f, FLinearColor::White, 0.3f);
		}
		for (int32 s = 0; s < 14; ++s) { const float A = R.FRandRange(0.f, PI); const FVector Base = FVector(95.f, FMath::Cos(A) * 30.f, H + 44.f + FMath::Sin(A) * 28.f); Cone(M, Base, Base + FVector(0, FMath::Cos(A), FMath::Sin(A)) * 18.f, 4.f, Spine, 0.35f); }
		Take(M, K.V, K.T, Body, PSpines); K = Mk(M);
		Take(G, 0, 0, Glow, PSpines);
		Seg(M, FVector(-100.f, 0, H - 10.f), FVector(-190.f, 0, 20.f), 22.f, 6.f, Hide, 0.6f, 8);
		Take(M, K.V, K.T, Body, PTail0); K = Mk(M);
		for (int32 l = 0; l < 4; ++l)
		{
			const int32 sd = l % 2 ? 1 : -1;
			const float X = l < 2 ? 70.f : -60.f;
			Leg(M, Body, uint8(PLeg0 + l), FVector(X, sd * 50.f, H - 20.f), FVector(X + 10.f, sd * 100.f, H - 10.f), FVector(X + 14.f, sd * 116.f, H - 50.f), FVector(X + 24.f, sd * 120.f, 0.f), 9.f, Hide * 0.9f);
			K = Mk(M);
		}
	}
	else if (Kind == TEXT("camel"))
	{
		// A Dromad (cook 2.12.1: a dromedary 3 m at the hump): long legs with knobbed knees and splayed pads, a barrel body,
		// the tall hump, a long arched neck to a long-muzzled head with lashes, the bull's pink dulla sac.
		const FLinearColor Coat = Dbc(0xC8, 0xA0, 0x6E, 0.55f), Pad = Dbc(0x5A, 0x48, 0x38, 0.55f);
		const float H = 190.f;
		Blob(M, FVector(0, 0, H), 110.f, 46.f, 52.f, Coat, 0.85f);
		Take(M, K.V, K.T, Body, PBody); K = Mk(M);
		M.Lathe(FVector(-10.f, 0, H + 30.f), FVector(0, 0, 1), 80.f, { FVector(0.f, 54.f, 34.f), FVector(0.55f, 40.f, 26.f), FVector(1.f, 8.f, 6.f) }, 12, Coat * 1.04f, 0.85f);
		Take(M, K.V, K.T, Body, PHump); K = Mk(M);
		// The neck: an S out and up from the chest, then the head.
		TArray<FVector> Neck = { FVector(96.f, 0, H + 10.f), FVector(150.f, 0, H - 20.f), FVector(190.f, 0, H + 30.f), FVector(205.f, 0, H + 100.f) };
		M.Tube(Neck, 20.f, 8, Coat, 0.85f);
		Take(M, K.V, K.T, Body, PNeck); K = Mk(M);
		M.Lathe(FVector(200.f, 0, H + 104.f), FVector(1, 0, -0.25f).GetSafeNormal(), 66.f, { FVector(0.f, 18.f, 20.f), FVector(0.6f, 14.f, 14.f), FVector(1.f, 9.f, 9.f) }, 10, Coat * 0.98f, 0.85f);
		for (int32 sd = -1; sd <= 1; sd += 2) { Cone(M, FVector(205.f, sd * 12.f, H + 122.f), FVector(198.f, sd * 18.f, H + 136.f), 4.f, Coat, 0.85f); Blob(M, FVector(226.f, sd * 15.f, H + 112.f), 3.f, 3.f, 3.f, Dbc(0x10, 0x0C, 0x08, 0.6f), 0.1f); }
		Take(M, K.V, K.T, Body, PHead); K = Mk(M);
		Blob(M, FVector(236.f, 14.f, H + 80.f), 18.f, 10.f, 22.f, Dbc(0xE8, 0x90, 0x9A, 0.6f), 0.35f);      // the dulla
		Take(M, K.V, K.T, Body, PSac); K = Mk(M);
		Seg(M, FVector(-108.f, 0, H + 10.f), FVector(-118.f, 0, H - 60.f), 5.f, 4.f, Coat, 0.85f);
		Take(M, K.V, K.T, Body, PTail0); K = Mk(M);
		for (int32 l = 0; l < 4; ++l)
		{
			const int32 sd = l % 2 ? 1 : -1;
			const float X = l < 2 ? 80.f : -80.f;
			Leg(M, Body, uint8(PLeg0 + l), FVector(X, sd * 28.f, H - 20.f), FVector(X + (l < 2 ? 6.f : -10.f), sd * 30.f, H - 90.f), FVector(X + (l < 2 ? 0.f : 4.f), sd * 30.f, 30.f), FVector(X + 8.f, sd * 32.f, 0.f), 9.f, Coat * 0.95f);
			K = Mk(M);
			Blob(M, FVector(X + 10.f, sd * 32.f, 5.f), 14.f, 12.f, 5.f, Pad, 0.9f);
			Take(M, K.V, K.T, Body, uint8(PLeg0 + l), 1.f); K = Mk(M);
		}
	}
	else if (Kind == TEXT("oryx"))
	{
		// A Shimmerbuck (cook 2.12.1: an Arabian oryx 1.5 m at the shoulder): a white body, black face mask and leg bands,
		// two long straight spiral horns swept back (one in profile: the unicorn), slender legs, a dark tail.
		const FLinearColor White = Dbc(0xF0, 0xEC, 0xE4, 0.68f), Black = Dbc(0x22, 0x1E, 0x1C, 0.6f);
		const float H = 115.f;
		Blob(M, FVector(0, 0, H), 70.f, 26.f, 32.f, White, 0.85f);
		Take(M, K.V, K.T, Body, PBody); K = Mk(M);
		M.Tube(TArray<FVector>{ FVector(56.f, 0, H + 6.f), FVector(80.f, 0, H + 36.f), FVector(96.f, 0, H + 62.f) }, 13.f, 8, White, 0.85f);
		Take(M, K.V, K.T, Body, PNeck); K = Mk(M);
		M.Lathe(FVector(94.f, 0, H + 66.f), FVector(1, 0, -0.6f).GetSafeNormal(), 46.f, { FVector(0.f, 12.f, 13.f), FVector(0.6f, 9.f, 10.f), FVector(1.f, 6.f, 6.f) }, 10, Black, 0.8f);
		for (int32 i = K.V; i < M.Verts.Num(); ++i) if (M.Verts[i].Z > H + 64.f && FMath::Abs(M.Verts[i].Y) > 7.f) M.Colors[i] = FWRToolMesh::Pack(White, 0.8f);   // the white mask over the black
		Take(M, K.V, K.T, Body, PHead); K = Mk(M);
		for (int32 sd = -1; sd <= 1; sd += 2)
		{
			const FVector Root(98.f, sd * 5.f, H + 76.f), Tip = Root + FVector(-62.f, sd * 6.f, 34.f) * 1.25f;
			M.Lathe(Root, (Tip - Root).GetSafeNormal(), (Tip - Root).Size(), { FVector(0.f, 3.4f, 3.4f), FVector(1.f, 0.6f, 0.6f) }, 6, Black, 0.35f, 0.f, 3.f);
			for (int32 r = 0; r < 6; ++r) Seg(M, FMath::Lerp(Root, Tip, r / 8.f), FMath::Lerp(Root, Tip, r / 8.f + 0.03f), 3.8f - r * 0.3f, 3.8f - r * 0.3f, Black * 0.8f, 0.4f, 6);   // the ridged spiral
		}
		Take(M, K.V, K.T, Body, PHorns); K = Mk(M);
		Seg(M, FVector(-70.f, 0, H + 10.f), FVector(-82.f, 0, H - 40.f), 3.f, 4.f, Black, 0.85f);
		Take(M, K.V, K.T, Body, PTail0); K = Mk(M);
		for (int32 l = 0; l < 4; ++l)
		{
			const int32 sd = l % 2 ? 1 : -1;
			const float X = l < 2 ? 48.f : -50.f;
			const FMk L0 = Mk(M);
			Leg(M, Body, uint8(PLeg0 + l), FVector(X, sd * 16.f, H - 16.f), FVector(X + (l < 2 ? 4.f : -10.f), sd * 16.f, H - 60.f), FVector(X, sd * 16.f, 24.f), FVector(X + 4.f, sd * 17.f, 0.f), 5.f, White);
			// The black bands on the upper legs (the oryx's markings).
			for (int32 i = L0.V; i < M.Verts.Num(); ++i) if (M.Verts[i].Z > 40.f && M.Verts[i].Z < 75.f) M.Colors[i] = FWRToolMesh::Pack(Black, 0.8f);
			for (int32 i = Body.Rest.Num() - (M.Verts.Num() - L0.V); i < Body.Rest.Num(); ++i) Body.Colors[i] = M.Colors[L0.V + (i - (Body.Rest.Num() - (M.Verts.Num() - L0.V)))];
			K = Mk(M);
		}
	}
	else if (Kind == TEXT("scarab"))
	{
		// A Scarab digger (cook 2.12.1: a dung beetle 1.2 m long): a domed metallic gold-green shell, rake-toothed front
		// legs, fan antennae, and its ball of sand bigger than itself, which it pushes walking backwards.
		const FLinearColor Shell = Dbc(0x6E, 0x8A, 0x3A, 0.6f), Gold = Dbc(0xC8, 0xA0, 0x40, 0.7f), Leg0 = Dbc(0x2A, 0x26, 0x1E, 0.6f);
		const float H = 36.f;
		M.Lathe(FVector(-50.f, 0, H - 14.f), FVector(1, 0, 0), 90.f, { FVector(0.f, 10.f, 8.f), FVector(0.25f, 34.f, 28.f), FVector(0.6f, 38.f, 32.f), FVector(0.85f, 30.f, 24.f), FVector(1.f, 14.f, 10.f) }, 14, Shell, 0.2f);
		for (int32 i = K.V; i < M.Verts.Num(); ++i) if (FMath::Abs(M.Verts[i].Y) < 2.5f && M.Verts[i].Z > H) M.Colors[i] = FWRToolMesh::Pack(Gold, 0.2f);   // the elytra's seam
		Blob(M, FVector(48.f, 0, H - 6.f), 18.f, 26.f, 16.f, Shell * 1.1f, 0.2f);
		Blob(M, FVector(66.f, 0, H - 12.f), 10.f, 18.f, 8.f, Leg0, 0.4f);
		for (int32 t = 0; t < 6; ++t) Cone(M, FVector(72.f, -15.f + t * 6.f, H - 14.f), FVector(82.f, -18.f + t * 7.2f, H - 18.f), 2.2f, Leg0, 0.4f);   // the head's rake
		for (int32 sd = -1; sd <= 1; sd += 2) for (int32 f = 0; f < 4; ++f) Cone(M, FVector(70.f, sd * 10.f, H - 6.f), FVector(80.f + f * 2.f, sd * (16.f + f * 3.f), H + 2.f + f * 3.f), 1.6f, Gold * 0.9f, 0.3f);   // fan antennae
		Take(M, K.V, K.T, Body, PBody); K = Mk(M);
		for (int32 l = 0; l < 6; ++l)
		{
			const int32 sd = l < 3 ? -1 : 1, i = l % 3;
			const float X = 30.f - i * 32.f;
			Leg(M, Body, uint8(PLeg0 + l), FVector(X, sd * 18.f, H - 16.f), FVector(X + 6.f, sd * 44.f, H - 4.f), FVector(X + (i == 0 ? 20.f : -6.f), sd * 54.f, H - 22.f), FVector(X + (i == 0 ? 30.f : -14.f), sd * 56.f, 0.f), 3.2f, Leg0);
			K = Mk(M);
		}
		// The ball: packed sand 1.4 m across, behind it (it pushes walking backwards: the ball is at its hind legs).
		Blob(M, FVector(-120.f, 0, 70.f), 70.f, 70.f, 70.f, Dbc(0xC8, 0x98, 0x58, 0.55f), 0.95f, 12);
		Take(M, K.V, K.T, Body, PBall); K = Mk(M);
	}
	else if (Kind == TEXT("grouse"))
	{
		// A Sandhen (cook 2.12.1: a sandgrouse 0.6 m): plump and short-legged, sand-barred plumage, a black chest band, a
		// small head and stubby bill; the long pointed wings fold along its back.
		const FLinearColor Sand = Dbc(0xC8, 0xA8, 0x78, 0.58f), Bar = Dbc(0x6E, 0x58, 0x3E, 0.55f);
		const float H = 24.f;
		Blob(M, FVector(0, 0, H), 22.f, 14.f, 13.f, Sand, 0.9f);
		for (int32 i = K.V; i < M.Verts.Num(); ++i) { const FVector& P = M.Verts[i]; if (FMath::Frac(P.X / 5.f) < 0.25f && P.Z > H) M.Colors[i] = FWRToolMesh::Pack(Bar, 0.9f); if (P.X > 10.f && P.X < 15.f) M.Colors[i] = FWRToolMesh::Pack(Dbc(0x18, 0x14, 0x10, 0.6f), 0.9f); }
		Blob(M, FVector(22.f, 0, H + 10.f), 7.f, 6.f, 6.f, Sand * 1.05f, 0.9f);
		Cone(M, FVector(28.f, 0, H + 9.f), FVector(33.f, 0, H + 8.f), 1.6f, Dbc(0x4A, 0x40, 0x34, 0.6f), 0.5f);
		Take(M, K.V, K.T, Body, PBody); K = Mk(M);
		for (int32 sd = -1; sd <= 1; sd += 2)
		{
			M.Lathe(FVector(8.f, sd * 12.f, H + 6.f), FVector(-1, sd * 0.15f, 0.05f).GetSafeNormal(), 34.f, { FVector(0.f, 3.f, 8.f), FVector(0.5f, 2.f, 7.f), FVector(1.f, 0.5f, 1.f) }, 5, Bar, 0.9f);
			Take(M, K.V, K.T, Body, sd < 0 ? PWingL : PWingR); K = Mk(M);
		}
		for (int32 l = 0; l < 2; ++l) { Leg(M, Body, uint8(PLeg0 + l), FVector(2.f, l ? 5.f : -5.f, H - 10.f), FVector(4.f, l ? 6.f : -6.f, 10.f), FVector(2.f, l ? 6.f : -6.f, 4.f), FVector(6.f, l ? 6.f : -6.f, 0.f), 1.4f, Bar); K = Mk(M); }
	}
	else if (Kind == TEXT("frog"))
	{
		// A Wellfrog (cook 2.12.1: a water-holding frog 0.4 m, round): a pale, translucent, water-swollen body, gold eyes
		// high on the head, short folded legs.
		const FLinearColor Skin = Dbc(0xD8, 0xD4, 0xB8, 0.62f), Gold = Dbc(0xE8, 0xB2, 0x30, 0.8f);
		const float H = 18.f;
		Blob(M, FVector(0, 0, H), 20.f, 18.f, 17.f, Skin, 0.2f);
		for (int32 sd = -1; sd <= 1; sd += 2) { Blob(M, FVector(12.f, sd * 8.f, H + 14.f), 4.f, 4.f, 4.f, Gold, 0.1f); Blob(M, FVector(14.f, sd * 8.5f, H + 14.5f), 1.8f, 1.8f, 1.8f, Dbc(0x08, 0x06, 0x04, 0.6f), 0.1f); }
		Take(M, K.V, K.T, Body, PBody); K = Mk(M);
		for (int32 l = 0; l < 4; ++l)
		{
			const int32 sd = l % 2 ? 1 : -1;
			const float X = l < 2 ? 10.f : -10.f;
			Leg(M, Body, uint8(PLeg0 + l), FVector(X, sd * 12.f, H - 6.f), FVector(X + (l < 2 ? 6.f : -10.f), sd * 20.f, H - 4.f), FVector(X + (l < 2 ? 8.f : 2.f), sd * 18.f, 4.f), FVector(X + 10.f, sd * 20.f, 0.f), 3.f, Skin * 0.95f);
			K = Mk(M);
		}
	}
	else if (Kind == TEXT("colossus"))
	{
		// Ahrumet (cook 2.14): a 16 m sandstone colossus of a king: broad shoulders, a pleated stone kilt, arms banded with
		// bronze; a face worn smooth (no nose, no mouth) with one great sunstone eye in the brow; a 6 m sun-disc ring behind
		// its head; cracks through the chest that pour sand. Built standing at full size (the pose seats it).
		const FLinearColor Stone = Dbc(0xC8, 0xA0, 0x70, 0.5f), Worn = Dbc(0xB0, 0x8A, 0x5E, 0.5f), Bronze = Dbc(0x8C, 0x5A, 0x2C, 0.6f), Basalt = Dbc(0x22, 0x20, 0x1E, 0.6f);
		// Hips and the pleated kilt (a flared skirt of vertical pleats), the torso, the chest.
		M.Lathe(FVector(0, 0, 640.f), FVector(0, 0, 1), 260.f, { FVector(0.f, 230.f, 170.f), FVector(1.f, 175.f, 130.f) }, 18, Worn, 0.75f);
		for (int32 p = 0; p < 14; ++p) { const float A = p * PI / 7.f; Seg(M, FVector(FMath::Cos(A) * 215.f, FMath::Sin(A) * 160.f, 900.f), FVector(FMath::Cos(A) * 250.f, FMath::Sin(A) * 190.f, 640.f), 14.f, 16.f, Stone * 0.9f, 0.75f, 4); }
		M.Lathe(FVector(0, 0, 900.f), FVector(0, 0, 1), 480.f, { FVector(0.f, 170.f, 125.f), FVector(0.45f, 230.f, 150.f), FVector(0.8f, 290.f, 165.f), FVector(1.f, 120.f, 95.f) }, 18, Stone, 0.7f);
		Blob(M, FVector(70.f, 0, 1220.f), 70.f, 210.f, 110.f, Stone * 1.05f, 0.7f);
		M.Lathe(FVector(0, 0, 1380.f), FVector(0, 0, 1), 60.f, { FVector(0.f, 95.f, 80.f), FVector(1.f, 85.f, 75.f) }, 12, Worn, 0.7f);
		Take(M, K.V, K.T, Body, PBody); K = Mk(M);
		// The head: smooth and worn, a royal head-cloth's lappets, and the sun-disc ring standing behind it.
		Blob(M, FVector(10.f, 0, 1530.f), 95.f, 90.f, 115.f, Worn, 0.8f);
		for (int32 sd = -1; sd <= 1; sd += 2) Seg(M, FVector(-10.f, sd * 95.f, 1560.f), FVector(10.f, sd * 120.f, 1380.f), 30.f, 40.f, Stone, 0.75f, 5);
		M.Lathe(FVector(-90.f, 0, 1560.f), FVector(-1, 0, 0), 26.f, { FVector(0.f, 300.f, 300.f), FVector(1.f, 300.f, 300.f) }, 28, Bronze, 0.35f);
		M.Lathe(FVector(-90.f, 0, 1560.f), FVector(-1, 0, 0), 30.f, { FVector(0.f, 250.f, 250.f), FVector(1.f, 250.f, 250.f) }, 28, Bronze * 0.7f, 0.35f);
		Take(M, K.V, K.T, Body, PHead); K = Mk(M);
		// Arms: shoulder to elbow to a stone fist, bronze bands at the biceps and the wrists.
		for (int32 sd = -1; sd <= 1; sd += 2)
		{
			const FVector Sh(0, sd * 300.f, 1300.f), El(30.f, sd * 340.f, 960.f), Wr(70.f, sd * 330.f, 640.f);
			Seg(M, Sh, El, 85.f, 70.f, Stone, 0.7f, 10); Seg(M, El, Wr, 70.f, 58.f, Stone, 0.7f, 10);
			Seg(M, FMath::Lerp(Sh, El, 0.45f), FMath::Lerp(Sh, El, 0.6f), 90.f, 90.f, Bronze, 0.35f, 12);
			Seg(M, FMath::Lerp(El, Wr, 0.75f), FMath::Lerp(El, Wr, 0.9f), 66.f, 66.f, Bronze, 0.35f, 12);
			Blob(M, Wr + FVector(10.f, 0, -70.f), 70.f, 60.f, 80.f, Worn, 0.75f);
			Take(M, K.V, K.T, Body, sd < 0 ? PWingL : PWingR); K = Mk(M);
		}
		// Legs: hip to knee to a squared foot.
		for (int32 sd = -1; sd <= 1; sd += 2)
		{
			const FVector Hip(0, sd * 130.f, 660.f), Knee(30.f, sd * 140.f, 340.f), Ank(0, sd * 140.f, 60.f);
			Seg(M, Hip, Knee, 110.f, 90.f, Stone, 0.7f, 10); Take(M, K.V, K.T, Body, uint8(PLeg0 + (sd < 0 ? 0 : 1)), 0.4f); K = Mk(M);
			Seg(M, Knee, Ank, 90.f, 75.f, Stone, 0.7f, 10);
			Blob(M, Ank + FVector(60.f, 0, -20.f), 150.f, 80.f, 40.f, Worn, 0.8f);
			Take(M, K.V, K.T, Body, uint8(PLeg0 + (sd < 0 ? 0 : 1)), 1.f); K = Mk(M);
		}
		// The gnomon as a club (phase 2): carried in the right fist, shown only once it is torn from the dial.
		M.Lathe(FVector(70.f, 330.f, 560.f), FVector(1, 0, 0), 2800.f, { FVector(0.f, 150.f, 150.f), FVector(0.9f, 90.f, 90.f), FVector(1.f, 10.f, 10.f) }, 4, Basalt, 0.4f);
		Take(M, K.V, K.T, Body, PHorns); K = Mk(M);
		// The glow: the sunstone eye, and the cracks through the chest, arms and knees that glow sun-hot.
		FWRToolMesh G;
		Blob(G, FVector(95.f, 0, 1580.f), 22.f, 34.f, 26.f, FLinearColor::White, 0.1f);
		Take(G, 0, 0, Glow, PBulb);
		FMk GK = Mk(G);
		for (int32 c = 0; c < 6; ++c) Seg(G, FVector(130.f, -120.f + c * 45.f, 1290.f - c * 30.f), FVector(135.f, -90.f + c * 40.f, 1160.f - c * 20.f), 7.f, 5.f, FLinearColor::White, 0.2f, 4);
		Take(G, GK.V, GK.T, Glow, PBody); GK = Mk(G);
		for (int32 sd = -1; sd <= 1; sd += 2)
		{
			Seg(G, FVector(95.f, sd * 330.f, 760.f), FVector(110.f, sd * 330.f, 880.f), 6.f, 5.f, FLinearColor::White, 0.2f, 4);
			Take(G, GK.V, GK.T, Glow, sd < 0 ? PWingL : PWingR); GK = Mk(G);
			Seg(G, FVector(115.f, sd * 140.f, 320.f), FVector(120.f, sd * 140.f, 400.f), 6.f, 5.f, FLinearColor::White, 0.2f, 4);
			Take(G, GK.V, GK.T, Glow, uint8(PLeg0 + (sd < 0 ? 0 : 1)), 0.4f); GK = Mk(G);
		}
	}
	for (FV& V : Body.Rest) V.Rest *= S;
	for (FV& V : Glow.Rest) V.Rest *= S;
	MinX = 1.0e9f; MaxX = -1.0e9f;
	for (const FV& V : Body.Rest) { MinX = FMath::Min(MinX, V.Rest.X); MaxX = FMath::Max(MaxX, V.Rest.X); }
	for (FV& V : Body.Rest) V.A = FMath::Clamp((V.Rest.X - MinX) / FMath::Max(1.f, MaxX - MinX), 0.f, 1.f);
	for (FV& V : Glow.Rest) V.A = FMath::Clamp((V.Rest.X - MinX) / FMath::Max(1.f, MaxX - MinX), 0.f, 1.f);
}

FVector UWRDesertBeast::Place(const FV& V) const
{
	FVector P = V.Rest;
	const float S = Scale;
	const float Phase = Clock * 2.f * PI;
	const bool bLeg = V.Part >= PLeg0 && V.Part < PLeg0 + 8;
	const int32 L = bLeg ? V.Part - PLeg0 : 0;
	if (Kind == TEXT("scorpion"))
	{
		// Legs step in alternating fours (1,3 one side with 2,4 the other): the foot lifts and swings forward on an arc.
		if (bLeg)
		{
			const float Group = ((L % 4) + (L / 4)) % 2 ? 0.f : 0.5f;
			const float Step = FMath::Max(0.f, FMath::Sin((Clock + Group) * 2.f * PI));
			P.Z += Step * 14.f * S * Amp * V.B;
			P.X += FMath::Cos((Clock + Group) * 2.f * PI) * 10.f * S * Amp * V.B;
		}
		// The tail arches higher and brings the bulb forward over the back to sting (Raised); idle, it sways.
		if ((V.Part >= PTail0 && V.Part < PTail0 + 6) || V.Part == PBulb)
		{
			const FVector Pivot(-74.f * S, 0.f, 48.f * S);
			const float T = V.Part == PBulb ? 1.f : V.B;
			const float Curl = RaiseAmt * 26.f * T + FMath::Sin(Time * 1.3f + Seed) * 3.f * T;
			P = FRotator(-Curl, FMath::Sin(Time * 0.9f) * 4.f * T, 0.f).RotateVector(P - Pivot) + Pivot;
		}
		// Pincers forward on a snap; the movable fingers close on a snap or a grip.
		if (V.Part == PPincerL || V.Part == PPincerR || V.Part == PFingerL || V.Part == PFingerR) P.X += JawOut * 24.f * S;
		if (V.Part == PFingerL || V.Part == PFingerR) P.Y += (V.Part == PFingerL ? 1.f : -1.f) * FMath::Max(JawOut, GripAmt) * 6.f * S;
		P.Z -= Bury * 80.f * S * (V.Part == PBulb ? 0.6f : 1.f);
	}
	else if (Kind == TEXT("skink"))
	{
		// The sideways S-wave (sandfish swimming), stronger under the sand; the paddles row; the jaw drops on a bite.
		const float Wave = Amp * (16.f + Dive * 18.f) * S;
		P.Y += Wave * FMath::Sin(Phase - V.A * 7.f) * (0.4f + (1.f - V.A));
		if (bLeg) P.X += FMath::Sin(Phase + L * PI * 0.5f) * 18.f * S * V.B * (1.f + SprayAmt * 2.f);
		if (V.Part == PJaw) P = FRotator(-(JawOut * 26.f), 0.f, 0.f).RotateVector(P - FVector(200.f * S, 0, 36.f * S)) + FVector(200.f * S, 0, 36.f * S);
		// Diving: all of it goes under the sand but the grit fin's top (it cuts the surface).
		if (V.Part == PFin) P.Z -= Dive * 90.f * S;
		else P.Z -= Dive * 190.f * S;
		// Beached: rolled belly up.
		if (Beach > 0.f) P = FRotator(0.f, 0.f, 150.f * Beach).RotateVector(P - FVector(0, 0, 50.f * S)) + FVector(0, 0, 50.f * S);
	}
	else if (Kind == TEXT("devil"))
	{
		// The thorny devil's jerky rocking walk: steps in pauses, the body rocking fore and aft.
		const float Jerk = FMath::Sin(Phase) > 0.2f ? 1.f : 0.f;
		if (bLeg)
		{
			const float Group = (L % 2) == (L / 2) ? 0.f : 0.5f;
			P.Z += FMath::Max(0.f, FMath::Sin((Clock + Group) * 2.f * PI)) * 16.f * S * Amp * V.B * Jerk;
		}
		else P = FRotator(FMath::Sin(Phase * 2.f) * 4.f * Amp, 0.f, 0.f).RotateVector(P - FVector(0, 0, 70.f * S)) + FVector(0, 0, 70.f * S);
		// The real head tucks under the hump (Ducked): the hump is what you hit.
		if (V.Part == PHead) { P.Z -= Duck * 30.f * S; P.X -= Duck * 26.f * S; }
		// Curled into a spiked ball (Rolling): everything drawn in toward a sphere and spinning.
		if (Roll > 0.f)
		{
			const FVector C(0, 0, 75.f * S);
			FVector D = P - C;
			const float Len = D.Size();
			const FVector OnBall = Len > 1.f ? D / Len * FMath::Min(Len, 80.f * S) : D;
			P = FMath::Lerp(P, C + FRotator(-RollSpin, 0.f, 0.f).RotateVector(OnBall), Roll);
		}
	}
	else if (Kind == TEXT("camel"))
	{
		// The pace (both legs of a side together), the neck and head bobbing, the dulla blown out; couched asleep.
		if (bLeg)
		{
			const float Group = L % 2 ? 0.5f : 0.f;
			const float Sw = FMath::Sin((Clock + Group) * 2.f * PI);
			P.X += Sw * 30.f * S * Amp * V.B;
			P.Z += FMath::Max(0.f, Sw) * 18.f * S * Amp * V.B;
			// Couched: the legs fold under.
			P.Z = FMath::Lerp(P.Z, FMath::Min(P.Z, 30.f * S) * V.B + P.Z * (1.f - V.B) * 0.3f, Sleep);
		}
		else P.Z -= Sleep * 120.f * S;
		if (V.Part == PNeck || V.Part == PHead || V.Part == PSac) P.Z += FMath::Sin(Phase * 2.f) * 6.f * S * Amp;
		if (V.Part == PSac) { const FVector C(236.f * S, 14.f * S, 270.f * S); P = C + (P - C) * (0.2f + DullaAmt * 1.4f); }
	}
	else if (Kind == TEXT("oryx"))
	{
		// The trot: diagonal pairs; the head lowers into a horn thrust; lying down asleep.
		if (bLeg)
		{
			const float Group = (L == 0 || L == 3) ? 0.f : 0.5f;
			const float Sw = FMath::Sin((Clock + Group) * 2.f * PI);
			P.X += Sw * 24.f * S * Amp * V.B;
			P.Z += FMath::Max(0.f, Sw) * 16.f * S * Amp * V.B;
			P.Z = FMath::Lerp(P.Z, P.Z * (1.f - V.B * 0.85f), Sleep);
		}
		else P.Z -= Sleep * 80.f * S;
		if (V.Part == PHead || V.Part == PHorns || V.Part == PNeck)
		{
			const FVector Pivot(56.f * S, 0, 121.f * S);
			P = FRotator(-JawOut * 34.f + FMath::Sin(Phase * 2.f) * 3.f * Amp, 0.f, 0.f).RotateVector(P - Pivot) + Pivot;
		}
	}
	else if (Kind == TEXT("scarab"))
	{
		if (bLeg)
		{
			const float Group = ((L % 3) + (L / 3)) % 2 ? 0.f : 0.5f;
			P.Z += FMath::Max(0.f, FMath::Sin((Clock + Group) * 2.f * PI)) * 8.f * S * Amp * V.B;
			P.X += FMath::Cos((Clock + Group) * 2.f * PI) * 8.f * S * Amp * V.B;
		}
		// The ball turns by the ground it covers (it rolls), and shows only while it's rolling one.
		if (V.Part == PBall)
		{
			const FVector C(-120.f * S, 0, 70.f * S);
			P = C + FRotator(-RollSpin, 0.f, 0.f).RotateVector(P - C);
		}
	}
	else if (Kind == TEXT("grouse"))
	{
		P.Z += FMath::Abs(FMath::Sin(Phase)) * 2.f * S * Amp;
		if (V.Part == PWingL || V.Part == PWingR)
		{
			const float Sd = V.Part == PWingL ? -1.f : 1.f;
			const FVector Sh(8.f * S, Sd * 12.f * S, 30.f * S);
			P = FRotator(0.f, 0.f, Sd * Fly * (40.f + 50.f * FMath::Sin(Time * 18.f))).RotateVector(P - Sh) + Sh;
		}
		if (bLeg) P.X += FMath::Sin(Phase + L * PI) * 4.f * S * Amp * V.B;
	}
	else if (Kind == TEXT("frog"))
	{
		// The throat breathes; a hop arcs the whole body; in its clay it sinks to its eyes.
		P *= FVector(1.f, 1.f + 0.03f * FMath::Sin(Time * 3.f), 1.f);
		P.Z += FMath::Abs(FMath::Sin(Phase)) * 10.f * S * Amp;
		P.Z -= Bury * 26.f * S;
	}
	else if (Kind == TEXT("colossus"))
	{
		// Seated on its throne (Asleep): the hips drop, the thighs come level, the shins hang. Walking: a slow heavy stride.
		// The arms swing; a blow (Jaw) brings both down; a grab (Grip) reaches the right one forward; the eye charging
		// (Raised) bows the head forward. The gnomon club shows in the right fist once it is torn up (Dulla).
		const bool bArmL = V.Part == PWingL, bArmR = V.Part == PWingR || V.Part == PHorns;
		const float Stride = FMath::Sin(Phase) * Amp;
		if (bLeg)
		{
			const float Sd = L == 0 ? 1.f : -1.f;
			const FVector HipP(0, 0, 660.f * S);
			P = FRotator(-Stride * 22.f * Sd, 0.f, 0.f).RotateVector(P - HipP) + HipP;
			if (Sleep > 0.f)
			{
				// Seated: the thigh swings level, the shin hangs from the knee.
				const FVector Hip(0, P.Y, 660.f * S);
				const FVector Knee = Hip + FVector(320.f * S, 0, 0);
				const FVector Seated = V.B < 0.5f ? Hip + FRotator(90.f, 0.f, 0.f).RotateVector(P - Hip) : Knee + FVector(0, 0, P.Z - 340.f * S);
				P = FMath::Lerp(P, Seated, Sleep);
			}
		}
		else if (bArmL || bArmR)
		{
			const float Sd = bArmL ? -1.f : 1.f;
			const FVector Sh(0, Sd * 300.f * S, 1300.f * S);
			float Pitch = Stride * 18.f * Sd - JawOut * 95.f;
			if (bArmR) Pitch -= GripAmt * 80.f;
			P = FRotator(Pitch, 0.f, 0.f).RotateVector(P - Sh) + Sh;
		}
		if (V.Part == PHead || V.Part == PBulb) { const FVector N(0, 0, 1400.f * S); P = FRotator(-RaiseAmt * 25.f, 0.f, 0.f).RotateVector(P - N) + N; }
		if (V.Part == PHorns && DullaAmt < 0.5f) P.Z -= 100000.f;   // not torn up yet: hidden under the dial
		P.Z -= Sleep * 300.f * S;
		P.Z += FMath::Abs(Stride) * 10.f * S;
	}
	if (Dead > 0.f) P = Kind == TEXT("colossus") ? FRotator(-80.f * Dead, 0.f, 0.f).RotateVector(P) : FRotator(0.f, 0.f, 175.f * Dead).RotateVector(P - FVector(0, 0, 30.f * S)) + FVector(0, 0, 30.f * S);
	return P;
}

void UWRDesertBeast::Pose(float Dt)
{
	if (bMirage)
	{
		MirageLeft -= Dt;
		// The mirage shimmers out over its last second.
		if (MirageLeft < 1.f) SetWorldScale3D(FVector(1.f, 1.f, FMath::Max(0.05f, MirageLeft)));
		return;
	}
	AWRCreature* C = Creature.Get();
	if (!C) return;
	const UWRDesertBody* B = UWRDesertBody::Get(C);
	const float Speed = C->GetVelocity().Size2D();
	const float Max = FMath::Max(100.f, C->GetCharacterMovement() ? C->GetCharacterMovement()->GetMaxSpeed() : 400.f);
	Amp = FMath::FInterpTo(Amp, FMath::Clamp(Speed / Max * 1.2f, 0.f, 1.3f), Dt, 4.f);
	Freq = Kind == TEXT("scorpion") ? 0.6f + Speed / 120.f : Kind == TEXT("grouse") ? 1.2f + Speed / 60.f : Kind == TEXT("camel") ? 0.35f + Speed / 400.f
		: Kind == TEXT("skink") ? 0.5f + Speed / 250.f : Kind == TEXT("devil") ? 0.5f + Speed / 150.f : 0.5f + Speed / 220.f;
	Clock += Dt * Freq;
	Time += Dt;
	const auto Ease = [Dt](float& V, bool bOn, float Rate) { V = FMath::FInterpTo(V, bOn ? 1.f : 0.f, Dt, Rate); };
	if (B)
	{
		Ease(JawOut, B->Has(WRDesertFlag::Jaw), 14.f);
		Ease(RaiseAmt, B->Has(WRDesertFlag::Raised), 6.f);
		Ease(GripAmt, B->Has(WRDesertFlag::Grip), 10.f);
		Ease(Bury, B->Has(WRDesertFlag::Buried), 3.f);
		Ease(Dive, B->Has(WRDesertFlag::Diving), 3.f);
		Ease(Beach, B->Has(WRDesertFlag::Beached), 5.f);
		Ease(Duck, B->Has(WRDesertFlag::Ducked), 6.f);
		Ease(Roll, B->Has(WRDesertFlag::Rolling), 6.f);
		Ease(FlareAmt, B->Has(WRDesertFlag::Flaring), 8.f);
		Ease(DullaAmt, B->Has(WRDesertFlag::Dulla), 3.f);
		Ease(Sleep, B->Has(WRDesertFlag::Asleep), 1.5f);
		Ease(SprayAmt, B->Has(WRDesertFlag::Spray), 10.f);
	}
	Ease(Fly, Kind == TEXT("grouse") && (Speed > 300.f || (C->GetCharacterMovement() && C->GetCharacterMovement()->IsFalling())), 6.f);
	// The ball and the curled devil roll by the ground they cover.
	RollSpin += Speed * Dt / FMath::Max(1.f, (Kind == TEXT("scarab") ? 70.f : 80.f) * Scale) * 57.3f;
	Ease(Dead, !C->IsAlive(), 1.5f);
	if (GlowMat)
	{
		const bool bGlowing = B && B->Has(WRDesertFlag::Glowing);
		// (The colossus's eye burns amber while it is awake; sun-hot, its cracks glow; charging, it brightens to white.)
		const float Awake = Kind == TEXT("colossus") && C->IsAlive() && Sleep < 0.5f ? 1.f : 0.f;
		const float G = (bGlowing ? (Kind == TEXT("scorpion") ? 1.4f : 2.0f) : 0.f) + FlareAmt * 8.f + Awake + (Kind == TEXT("colossus") ? RaiseAmt * 4.f : 0.f);
		if (!FMath::IsNearlyEqual(G, GlowNow, 0.02f)) { GlowNow = G; GlowMat->SetScalarParameterValue(TEXT("Glow"), G); }
	}
}

void UWRDesertBeast::Commit(bool bCreate)
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

void UWRDesertBeast::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bBuilt) return;
	Pose(DeltaTime);
	if (bMirage) return;
	PoseClock -= DeltaTime;
	const bool bNear = WasRecentlyRendered(0.2f);
	if (!bNear && PoseClock > 0.f) return;
	PoseClock = bNear ? 0.f : 0.5f;
	Commit(false);
}
