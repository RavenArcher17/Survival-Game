#include "WRGroundTrail.h"
#include "WRLand.h"
#include "EngineUtils.h"
#include "ProceduralMeshComponent.h"

FWRTrailLook FWRTrailLook::Ash()
{
	FWRTrailLook L;
	// (at 58, 52, 48 the paths read from the air as dark spokes drawn on the ash)
	L.Colour = FLinearColor(FColor(76, 70, 64));
	const float Mix[6] = { 0.f, 0.f, 0.55f, 0.f, 0.3f, 0.15f };
	for (int32 k = 0; k < 6; ++k) L.Layers[k] = Mix[k];
	L.Rough = 0.92f;
	L.Strength = 0.8f;
	return L;
}

FWRTrailLook FWRTrailLook::Snow()
{
	FWRTrailLook L;
	L.Colour = FLinearColor(FColor(176, 180, 186));
	const float Mix[6] = { 0.f, 0.f, 0.22f, 0.f, 0.18f, 0.6f };
	for (int32 k = 0; k < 6; ++k) L.Layers[k] = Mix[k];
	L.Rough = 0.82f;
	L.Strength = 0.85f;
	return L;
}

namespace WRGroundTrailImpl
{
	struct FGround
	{
		const FWRLandDef* D = nullptr;
		UMaterialInterface* Mat = nullptr;
	};

	FGround Find(AActor* Owner, const FVector2D& At)
	{
		FGround G;
		if (!Owner) return G;
		const FWRLandDef* D = WRLand::At(FVector(At, 0.f));
		if (!D || !D->bHeightfield) return G;
		for (TActorIterator<AWRLand> It(Owner->GetWorld()); It; ++It)
		{
			if (It->LandId == D->Id) { G.Mat = It->GroundLook(); break; }
		}
		if (G.Mat) G.D = D;
		return G;
	}

	/** The ground's height (cm) under a world point; false where the land draws none (a cave's mouth). */
	bool Height(const FVector2D& P, float& Z) { return WRLand::GroundUnder(FVector(P, 0.f), Z); }

	/** One vertex of worn ground: the land's own look at the point (bilinear from its four samples, as the land draws
	 *  it), moved toward the trail's by W. */
	struct FVtx
	{
		FVector P;
		FVector N;
		FLinearColor C;
		FVector2D UV0, UV1, UV2, UV3;
	};

	bool Make(const FGround& G, const FVector2D& P, float W, const FWRTrailLook& Look, float Lift, FVtx& Out)
	{
		float Z;
		if (!Height(P, Z)) return false;
		float Zx0, Zx1, Zy0, Zy1;
		if (!Height(P - FVector2D(50.f, 0.f), Zx0) || !Height(P + FVector2D(50.f, 0.f), Zx1) || !Height(P - FVector2D(0.f, 50.f), Zy0) || !Height(P + FVector2D(0.f, 50.f), Zy1))
		{
			Zx0 = Zx1 = Zy0 = Zy1 = Z;
		}
		const FVector Nrm = FVector(-(Zx1 - Zx0), -(Zy1 - Zy0), 100.f).GetSafeNormal();
		const FWRLandDef* D = G.D;
		const FVector2D M = D->ToMap(FVector(P, 0.f));
		const float GX = (M.X - D->HfOrigin.X) / D->HfStep, GY = (M.Y - D->HfOrigin.Y) / D->HfStep;
		const int32 I0 = FMath::FloorToInt(GX), J0 = FMath::FloorToInt(GY);
		const float TX = GX - I0, TY = GY - J0;
		FLinearColor C(0.f, 0.f, 0.f, 0.f);
		float Mix[6] = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
		float Wet = 0.f;
		bool bWet = true;
		for (int32 c = 0; c < 4; ++c)
		{
			const int32 SI = I0 + (c & 1), SJ = J0 + (c >> 1);
			const float Wt = ((c & 1) ? TX : 1.f - TX) * ((c >> 1) ? TY : 1.f - TY);
			float Rough = 0.9f;
			FLinearColor Gc = D->HfGround(SI, SJ, Nrm, Rough);
			Gc.A = 1.f - Rough;
			C += Gc * Wt;
			if (D->bLayered) { float Lm[6]; D->HfLayers(SI, SJ, Lm); for (int32 m = 0; m < 6; ++m) Mix[m] += Lm[m] * Wt; }
			const float Ws = D->HfWet(SI, SJ);
			if (Ws < 0.f) bWet = false;
			Wet += FMath::Max(0.f, Ws) * Wt;
		}
		const float T = FMath::Clamp(W * Look.Strength, 0.f, 1.f);
		FLinearColor Tc = Look.Colour;
		Tc.A = 1.f - Look.Rough;
		Out.C = FMath::Lerp(C, Tc, T);
		for (int32 m = 0; m < 6; ++m) Mix[m] = FMath::Lerp(Mix[m], Look.Layers[m], T);
		Out.UV1 = FVector2D(Mix[0], Mix[1]);
		Out.UV2 = FVector2D(Mix[2], Mix[3]);
		Out.UV3 = FVector2D(Mix[4], Mix[5]);
		// (as the land: its shore's wetness in the first UV's X where it keeps one, else map metres / 4)
		Out.UV0 = bWet ? FVector2D(Wet, 0.f) : M / 4.f;
		Out.P = FVector(P, Z + Lift);
		Out.N = Nrm;
		return true;
	}

	/** Each triangle faces up (the procedural mesh's front: cross(b - a, c - a) pointing down, as the land's). */
	void Tri(TArray<int32>& Tris, const TArray<FVtx>& V, int32 A, int32 B, int32 C)
	{
		if (A < 0 || B < 0 || C < 0) return;
		const FVector X = FVector::CrossProduct(V[B].P - V[A].P, V[C].P - V[A].P);
		if (X.Z <= 0.f) Tris.Append({ A, B, C });
		else Tris.Append({ A, C, B });
	}

	UProceduralMeshComponent* Emit(AActor* Owner, USceneComponent* Parent, const FGround& G, const TArray<FVtx>& V, const TArray<int32>& Tris)
	{
		if (Tris.Num() == 0) return nullptr;
		UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(Owner);
		Mesh->SetupAttachment(Parent ? Parent : Owner->GetRootComponent());
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCanEverAffectNavigation(false);
		Mesh->SetCastShadow(false);
		Mesh->RegisterComponent();
		const FTransform Xf = Mesh->GetComponentTransform();
		TArray<FVector> P, N;
		TArray<FVector2D> U0, U1, U2, U3;
		TArray<FLinearColor> C;
		TArray<FProcMeshTangent> Tn;
		for (const FVtx& X : V)
		{
			P.Add(Xf.InverseTransformPosition(X.P));
			N.Add(Xf.InverseTransformVectorNoScale(X.N));
			U0.Add(X.UV0); U1.Add(X.UV1); U2.Add(X.UV2); U3.Add(X.UV3);
			C.Add(X.C);
			Tn.Add(FProcMeshTangent(Xf.InverseTransformVectorNoScale(FVector(1.f, 0.f, 0.f)), false));
		}
		if (G.D->bLayered) Mesh->CreateMeshSection_LinearColor(0, P, Tris, N, U0, U1, U2, U3, C, Tn, false);
		else Mesh->CreateMeshSection_LinearColor(0, P, Tris, N, U0, C, Tn, false);
		Mesh->SetMaterial(0, G.Mat);
		Mesh->SetCullDistance(30000.f);
		Mesh->SetCachedMaxDrawDistance(30000.f);
		return Mesh;
	}

	float Noise(float X, int32 Seed) { return FMath::PerlinNoise1D(X + float(Seed % 997) * 3.17f); }
}

UProceduralMeshComponent* WRGroundTrail::Path(AActor* Owner, USceneComponent* Parent, const TArray<FVector2D>& WorldPts, float HalfWidth, const FWRTrailLook& Look, int32 Seed)
{
	using namespace WRGroundTrailImpl;
	if (WorldPts.Num() < 2) return nullptr;
	const FGround G = Find(Owner, WorldPts[0]);
	if (!G.D) return nullptr;
	// Through the points, smoothly (Catmull-Rom), every 40 cm.
	TArray<FVector2D> Line;
	for (int32 i = 0; i + 1 < WorldPts.Num(); ++i)
	{
		const FVector2D P0 = WorldPts[FMath::Max(0, i - 1)], P1 = WorldPts[i], P2 = WorldPts[i + 1], P3 = WorldPts[FMath::Min(WorldPts.Num() - 1, i + 2)];
		const int32 Steps = FMath::Max(1, FMath::CeilToInt(FVector2D::Distance(P1, P2) / 40.f));
		for (int32 s = 0; s < Steps; ++s)
		{
			const float T = float(s) / Steps, T2 = T * T, T3 = T2 * T;
			Line.Add(0.5f * ((2.f * P1) + (P2 - P0) * T + (2.f * P0 - 5.f * P1 + 4.f * P2 - P3) * T2 + (3.f * P1 - P0 - 3.f * P2 + P3) * T3));
		}
	}
	Line.Add(WorldPts.Last());
	// Across it: nine points, its middle the trail's, its edges the ground's; its width wanders, its wear comes in
	// patches, and its ends fade out.
	constexpr int32 NA = 9;
	TArray<FVtx> V;
	TArray<int32> Index;
	float S = 0.f;
	for (int32 i = 0; i < Line.Num(); ++i)
	{
		if (i > 0) S += FVector2D::Distance(Line[i - 1], Line[i]);
		const FVector2D Tg = (Line[FMath::Min(i + 1, Line.Num() - 1)] - Line[FMath::Max(i - 1, 0)]).GetSafeNormal();
		const FVector2D Side(-Tg.Y, Tg.X);
		const float Hw = HalfWidth * (1.f + 0.22f * Noise(S / 230.f, Seed));
		const float Wear = 0.78f + 0.22f * Noise(S / 170.f, Seed + 7);
		const float Ends = FMath::SmoothStep(0.f, 120.f, S) * FMath::SmoothStep(0.f, 120.f, float(Line.Num() - 1 - i) * 40.f);
		for (int32 a = 0; a < NA; ++a)
		{
			const float O = -1.f + 2.f * a / (NA - 1);
			const float W = (1.f - FMath::SmoothStep(0.35f, 1.f, FMath::Abs(O))) * Wear * Ends;
			FVtx X;
			if (Make(G, Line[i] + Side * (O * Hw), W, Look, 2.f + 2.f * W, X)) { Index.Add(V.Num()); V.Add(X); }
			else Index.Add(INDEX_NONE);
		}
	}
	TArray<int32> Tris;
	for (int32 i = 0; i + 1 < Line.Num(); ++i)
	{
		for (int32 a = 0; a + 1 < NA; ++a)
		{
			const int32 A = Index[i * NA + a], B = Index[i * NA + a + 1], C = Index[(i + 1) * NA + a], E = Index[(i + 1) * NA + a + 1];
			Tri(Tris, V, A, C, E);
			Tri(Tris, V, A, E, B);
		}
	}
	return Emit(Owner, Parent, G, V, Tris);
}

UProceduralMeshComponent* WRGroundTrail::Patch(AActor* Owner, USceneComponent* Parent, const FVector2D& WorldCentre, float Inner, float Outer, const FWRTrailLook& Look, int32 Seed)
{
	using namespace WRGroundTrailImpl;
	const FGround G = Find(Owner, WorldCentre);
	if (!G.D) return nullptr;
	// Rings out from the middle, every ~60 cm, 40 round; the edge ragged (each spoke its own reach).
	constexpr int32 NS = 40;
	const int32 NR = FMath::Max(4, FMath::CeilToInt(Outer * 1.25f / 60.f));
	TArray<FVtx> V;
	TArray<int32> Index;
	FVtx Mid;
	const int32 MidI = Make(G, WorldCentre, 1.f, Look, 4.f, Mid) ? 0 : INDEX_NONE;
	if (MidI == 0) V.Add(Mid);
	for (int32 r = 1; r <= NR; ++r)
	{
		for (int32 s = 0; s < NS; ++s)
		{
			const float A = 2.f * PI * s / NS;
			const float Reach = 1.f + 0.18f * Noise(float(s) * 0.37f, Seed) + 0.08f * Noise(float(s) * 1.3f, Seed + 3);
			const float R = Outer * 1.25f * Reach * r / NR;
			const float W = 1.f - FMath::SmoothStep(Inner * Reach, Outer * Reach, R);
			FVtx X;
			if (Make(G, WorldCentre + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R, W, Look, 2.f + 2.f * W, X)) { Index.Add(V.Num()); V.Add(X); }
			else Index.Add(INDEX_NONE);
		}
	}
	TArray<int32> Tris;
	for (int32 s = 0; s < NS; ++s) if (MidI == 0) Tri(Tris, V, 0, Index[s], Index[(s + 1) % NS]);
	for (int32 r = 0; r + 1 < NR; ++r)
	{
		for (int32 s = 0; s < NS; ++s)
		{
			const int32 A = Index[r * NS + s], B = Index[r * NS + (s + 1) % NS], C = Index[(r + 1) * NS + s], E = Index[(r + 1) * NS + (s + 1) % NS];
			Tri(Tris, V, A, C, E);
			Tri(Tris, V, A, E, B);
		}
	}
	return Emit(Owner, Parent, G, V, Tris);
}
