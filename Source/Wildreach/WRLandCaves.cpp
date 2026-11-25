// The underground caves' rock (see WRLandCaves.h).

#include "WRLandCaves.h"
#include "Wildreach.h"
#include "WRLand.h"
#include "WRGameState.h"
#include "WRToolMesh.h"
#include "WRBuildLookKit.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "WRTuning.h"
#include "Async/ParallelFor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"

namespace WRLandCavesImpl
{
	FLinearColor LcLin(uint8 R, uint8 G, uint8 B) { return FLinearColor(FColor(R, G, B)); }
	float LcSm(float E0, float E1, float X) { const float T = FMath::Clamp((X - E0) / (E1 - E0), 0.f, 1.f); return T * T * (3.f - 2.f * T); }
	float LcN2(float X, float Y)
	{
		const int32 X0 = FMath::FloorToInt(X), Y0 = FMath::FloorToInt(Y);
		const float FX = X - X0, FY = Y - Y0;
		const auto H = [](int32 A, int32 B) { uint32 V = uint32(A) * 0x8da6b343u ^ uint32(B) * 0xd8163841u ^ 0x2c1b3u; V ^= V >> 13; V *= 0x5bd1e995u; V ^= V >> 15; return float(V & 0xffff) / 32767.5f - 1.f; };
		const float SX = FX * FX * (3.f - 2.f * FX), SY = FY * FY * (3.f - 2.f * FY);
		return FMath::Lerp(FMath::Lerp(H(X0, Y0), H(X0 + 1, Y0), SX), FMath::Lerp(H(X0, Y0 + 1), H(X0 + 1, Y0 + 1), SX), SY);
	}
	float LcN3(const FVector& P) { return LcN2(P.X + P.Z * 0.37f, P.Y - P.Z * 0.61f) * 0.6f + LcN2(P.Y * 0.73f + 11.f, P.Z + P.X * 0.29f) * 0.4f; }

	/** Formation meshes (stalactites, stalagmites, columns, frozen falls), grown rather than placed: a lathe along an
	 *  axis whose radius follows a profile, its section never round (two slow waves round it, phased per ring), bent a
	 *  little, in the cave's own material (M_CaveRock / M_CaveIce: colour and roughness in the vertex colour, UV1-2 as
	 *  the walls'). */
	struct FLcGrown
	{
		TArray<FVector> V, N;
		TArray<int32> T;
		TArray<FVector2D> UV0, UV1, UV2;
		TArray<FColor> C;
	};
	float LcHash(uint32 Seed, int32 A, int32 B)
	{
		uint32 X = Seed * 0x9e3779b9u ^ uint32(A) * 0x85ebca6bu ^ uint32(B) * 0xc2b2ae35u;
		X ^= X >> 15; X *= 0x2c1b3c6du; X ^= X >> 12; X *= 0x297a2d39u; X ^= X >> 15;
		return float(X & 0xffffff) / float(0xffffff);
	}
	/** Radius(t) along Length from Base toward Dir (t 0..1); Lump = how far from round (0.1 smooth .. 0.3 knobbly). */
	void LcLathe(FLcGrown& M, const FVector& Base, const FVector& Dir, float Length, TFunctionRef<float(float)> Radius, int32 Rings, int32 Sides,
		uint32 Seed, float Bend, float Lump, const FColor& Col, const FVector2D& U1, const FVector2D& U2)
	{
		const FVector Ax = Dir.GetSafeNormal();
		const FVector Ref = FMath::Abs(Ax.Z) > 0.9f ? FVector::ForwardVector : FVector::UpVector;
		const FVector AY = FVector::CrossProduct(Ax, Ref).GetSafeNormal(), AZ = FVector::CrossProduct(Ax, AY);
		const float BendA = LcHash(Seed, 7, 7) * 2.f * PI;
		const FVector BendDir = AY * FMath::Cos(BendA) + AZ * FMath::Sin(BendA);
		const int32 First = M.V.Num();
		for (int32 r = 0; r <= Rings; ++r)
		{
			const float T = float(r) / Rings;
			const float Rad = FMath::Max(0.f, Radius(T));
			const float dT = 0.5f / Rings;
			const float Slope = (Radius(FMath::Min(1.f, T + dT)) - Radius(FMath::Max(0.f, T - dT))) / (2.f * dT * Length);
			const FVector Centre = Base + Ax * (T * Length) + BendDir * (Bend * T * T);
			const float P1 = LcHash(Seed, r, 1) * 6.28f, P2 = LcHash(Seed, r / 2, 2) * 6.28f;
			for (int32 k = 0; k < Sides; ++k)
			{
				const float A = 2.f * PI * k / Sides;
				const float Wob = 1.f + Lump * (0.6f * FMath::Sin(2.f * A + P1) + 0.4f * FMath::Sin(3.f * A + P2));
				const FVector Out = AY * FMath::Cos(A) + AZ * FMath::Sin(A);
				M.V.Add(Centre + Out * (Rad * Wob));
				M.N.Add((Out - Ax * Slope).GetSafeNormal());
				M.UV0.Add(FVector2D(float(k) / Sides, T * Length / 100.f));
				M.UV1.Add(U1);
				M.UV2.Add(U2);
				M.C.Add(Col);
			}
		}
		for (int32 r = 0; r < Rings; ++r) for (int32 k = 0; k < Sides; ++k)
		{
			const int32 A = First + r * Sides + k, B = First + r * Sides + (k + 1) % Sides, C = A + Sides, D = B + Sides;
			// Facing out (Unreal's winding: clockwise seen from the front).
			M.T.Append({ A, B, C, B, D, C });
		}
	}

	/** The ground (world cm) at a world point's column: on the land's own triangles (each grid square split on its
	 *  flatter diagonal, as WRLand builds its chunks), so the skin's ground lies exactly on the land's. */
	float LcGroundZ(const FWRLandDef* L, float X, float Y)
	{
		const FVector2D M = L->ToMap(FVector(X, Y, 0.f));
		const float FX = (M.X - L->HfOrigin.X) / L->HfStep, FY = (M.Y - L->HfOrigin.Y) / L->HfStep;
		const int32 I = FMath::FloorToInt(FX), J = FMath::FloorToInt(FY);
		const float TX = FX - I, TY = FY - J;
		const float A = L->HfSample(I, J), B = L->HfSample(I + 1, J), C = L->HfSample(I, J + 1), E = L->HfSample(I + 1, J + 1);
		float H;
		if (FMath::Abs(A - E) <= FMath::Abs(B - C)) H = TX >= TY ? A + (B - A) * TX + (E - B) * TY : A + (C - A) * TY + (E - C) * TX;
		else H = TX + TY <= 1.f ? A + (B - A) * TX + (C - A) * TY : E + (C - E) * (1.f - TX) + (B - E) * (1.f - TY);
		return L->ToWorld(M, H).Z;
	}
	/** The sample whose ground a lip shows at sample (I, J): itself, or, where it is cut (a mouth's sample has no surface
	 *  of its own), the nearest drawn one. (The cut samples fell back to the zone's bare ground: in the Blue Below's
	 *  corridor a room's doorway showed squares of it in the corridor's ice floor.) */
	FIntPoint LcGroundSample(const FWRLandDef* L, int32 I, int32 J)
	{
		if (L->HfSurfaceAt(I, J) != FWRLandDef::HfHidden) return FIntPoint(I, J);
		for (int32 R = 1; R <= 4; ++R)
		{
			int32 BestD = MAX_int32;
			FIntPoint Best(I, J);
			for (int32 dj = -R; dj <= R; ++dj) for (int32 di = -R; di <= R; ++di)
			{
				if (FMath::Max(FMath::Abs(di), FMath::Abs(dj)) != R || L->HfSurfaceAt(I + di, J + dj) == FWRLandDef::HfHidden) continue;
				if (di * di + dj * dj < BestD) { BestD = di * di + dj * dj; Best = FIntPoint(I + di, J + dj); }
			}
			if (BestD < MAX_int32) return Best;
		}
		return FIntPoint(I, J);
	}
	/** The brows' rock at a world point (cm, negative inside; huge where there's none): a rough ellipsoid each, the rock
	 *  noise on it. */
	float LcBrow(const FWRLandDef* L, const TArray<int32>& Caves, const FVector& P)
	{
		float Best = 1.0e6f;
		const FVector2D M = L->ToMap(P);
		const FVector Pm(M.X, M.Y, (P.Z - L->Origin.Z) / 100.f);
		for (int32 c : Caves)
		{
			const FWRLandCave& Cv = L->Caves[c];
			if (!Cv.bHasBrow) continue;
			const float A = FMath::DegreesToRadians(Cv.BrowYaw), Ca = FMath::Cos(A), Sa = FMath::Sin(A);
			// The crag and a smaller lump beside it, up its back, joined (never one smooth egg).
			const auto Egg = [&](const FVector& C, const FVector& R)
			{
				const FVector D = Pm - C;
				const FVector Q(float(D.X * Ca + D.Y * Sa) / R.X, float(-D.X * Sa + D.Y * Ca) / R.Y, float(D.Z) / R.Z);
				return (float(Q.Size()) - 1.f) * FMath::Min3(R.X, R.Y, R.Z);
			};
			const float Side = FMath::Frac(Cv.BrowYaw * 0.1f) < 0.5f ? 1.f : -1.f;
			const FVector Lump = Cv.BrowC + FVector(Ca, Sa, 0.f) * (Cv.BrowR.X * 0.45f) + FVector(-Sa, Ca, 0.f) * (Cv.BrowR.Y * 0.55f * Side) + FVector(0.f, 0.f, Cv.BrowR.Z * 0.35f);
			const float E1 = Egg(Cv.BrowC, Cv.BrowR), E2 = Egg(Lump, Cv.BrowR * 0.62f);
			const float Hj = FMath::Max(1.f - FMath::Abs(E1 - E2), 0.f);
			// Broken, not smooth: the rock noise, and creases where a ridged noise folds (the faces of a crag, its joints).
			const float Sd = FMath::Min(E1, E2) - Hj * Hj * 0.25f + 0.5f * LcN3(P / 200.f) + 0.25f * LcN3(P / 60.f + FVector(3.f, 9.f, 2.f))
				- 0.4f * FMath::Abs(LcN3(P / 120.f + FVector(11.f, 4.f, 7.f)));
			Best = FMath::Min(Best, Sd * 100.f);
		}
		return Best;
	}
	/** The Tundra's ice walls round a cube (round 14 of the look pass: the ice shelves', the glacier's and the cirque's
	 *  faces were panels of fluted ice, SM_IceFace, stood along them: rows of striped slabs from the air, a lone one like a
	 *  door on a wall). Now the steep ice itself grows out of the land, as the caves' mouths do: per heightfield sample
	 *  round the cube, how much of a wall it is (steep glacier, ice or lake ice in the Tundra, spread a sample so its top
	 *  and foot are pushed too) and the way out of it (down its slope). The rock's field takes the ground pushed out that
	 *  way (LcWallPush): the face stands proud of the land in fluted ice and its top and foot roll back into the ground. */
	struct FLcWalls
	{
		int32 I0 = 0, J0 = 0, W = 0, H = 0;
		TArray<float> Wt;
		TArray<FVector2f> Out;
		float MinZ = 0.f, MaxZ = 0.f;   // the ground's lowest and highest sample over the footprint (m)
		bool bAny = false;
		bool Valid(int32 I, int32 J) const { return I >= 0 && J >= 0 && I < W && J < H; }
	};
	// (set on the game thread before any cube is queued; the workers only read them)
	float GWallFrom = 44.f, GWallTo = 56.f;
	bool GWalls = true;
	float LcWallFrom() { return GWallFrom; }
	float LcWallTo() { return GWallTo; }
	/** A sample's own wall weight (before the spread): steep ice of the Tundra. (Round 14 grew the Volcanic's steep ground
	 *  as rock walls the same way: 10,000 more cubes, +12 GB on a client, and little to see on its dark ridges. Its
	 *  cliffs are carved gullies in the land and layered rock in the ground's material instead.) */
	float LcWallWeight0(const FWRLandDef* L, int32 I, int32 J, float From, float To)
	{
		const uint8 Cls = L->HfSurfaceAt(I, J);
		if (Cls == FWRLandDef::HfHidden || !L->SlopeIceClasses.Contains(Cls)) return 0.f;
		const float Gx = (L->HfSample(I + 1, J) - L->HfSample(I - 1, J)) / (2.f * L->HfStep);
		const float Gy = (L->HfSample(I, J + 1) - L->HfSample(I, J - 1)) / (2.f * L->HfStep);
		const float Slope = FMath::RadiansToDegrees(FMath::Atan(FMath::Sqrt(Gx * Gx + Gy * Gy)));
		if (Slope < From) return 0.f;
		return L->BiomeAtMap(L->HfOrigin + FVector2D(float(I), float(J)) * L->HfStep) == TEXT("tundra") ? LcSm(From, To, Slope) : 0.f;
	}
	/** The walls over a map box (metres). */
	void LcBuildWalls(const FWRLandDef* L, const FVector2D& Lo, const FVector2D& Hi, FLcWalls& Out)
	{
		const float From = LcWallFrom(), To = LcWallTo();
		Out.I0 = FMath::FloorToInt((Lo.X - L->HfOrigin.X) / L->HfStep) - 1;
		Out.J0 = FMath::FloorToInt((Lo.Y - L->HfOrigin.Y) / L->HfStep) - 1;
		Out.W = FMath::CeilToInt((Hi.X - L->HfOrigin.X) / L->HfStep) + 2 - Out.I0;
		Out.H = FMath::CeilToInt((Hi.Y - L->HfOrigin.Y) / L->HfStep) + 2 - Out.J0;
		// Own weights one sample wider all round (the spread reads its neighbours).
		const int32 W2 = Out.W + 2, H2 = Out.H + 2;
		TArray<float> W0;
		W0.SetNumZeroed(W2 * H2);
		Out.MinZ = 1.0e9f; Out.MaxZ = -1.0e9f;
		for (int32 j = 0; j < H2; ++j) for (int32 i = 0; i < W2; ++i)
		{
			const int32 I = Out.I0 - 1 + i, J = Out.J0 - 1 + j;
			W0[j * W2 + i] = LcWallWeight0(L, I, J, From, To);
			const float Z = L->HfSample(I, J);
			Out.MinZ = FMath::Min(Out.MinZ, Z);
			Out.MaxZ = FMath::Max(Out.MaxZ, Z);
		}
		Out.Wt.SetNumZeroed(Out.W * Out.H);
		Out.Out.SetNumZeroed(Out.W * Out.H);
		for (int32 j = 0; j < Out.H; ++j) for (int32 i = 0; i < Out.W; ++i)
		{
			const auto At = [&](int32 di, int32 dj) { return W0[(j + 1 + dj) * W2 + (i + 1 + di)]; };
			const float Wt = FMath::Max(At(0, 0), 0.75f * FMath::Max(FMath::Max(At(1, 0), At(-1, 0)), FMath::Max(At(0, 1), At(0, -1))));
			if (Wt <= 0.f) continue;
			// Down the slope, from the gradient over the samples round it (smoother than one difference).
			const int32 I = Out.I0 + i, J = Out.J0 + j;
			float Gx = 0.f, Gy = 0.f;
			for (int32 d = -1; d <= 1; ++d)
			{
				const float K = d == 0 ? 2.f : 1.f;
				Gx += K * (L->HfSample(I + 1, J + d) - L->HfSample(I - 1, J + d));
				Gy += K * (L->HfSample(I + d, J + 1) - L->HfSample(I + d, J - 1));
			}
			const FVector2f D(-Gx, -Gy);
			if (D.SizeSquared() < 1.0e-4f) continue;
			Out.Wt[j * Out.W + i] = Wt;
			Out.Out[j * Out.W + i] = D.GetSafeNormal();
			Out.bAny = true;
		}
	}
	/** The walls' weight at a world point and the way out of the wall there (unit, world XY). */
	float LcWallAt(const FWRLandDef* L, const FLcWalls& Wl, float X, float Y, FVector2f& OutDir)
	{
		OutDir = FVector2f::ZeroVector;
		if (!Wl.bAny) return 0.f;
		const FVector2D M = L->ToMap(FVector(X, Y, 0.f));
		const float FX = (M.X - L->HfOrigin.X) / L->HfStep - Wl.I0, FY = (M.Y - L->HfOrigin.Y) / L->HfStep - Wl.J0;
		const int32 I = FMath::FloorToInt(FX), J = FMath::FloorToInt(FY);
		const float TX = FX - I, TY = FY - J;
		float Wt = 0.f;
		FVector2f D = FVector2f::ZeroVector;
		for (int32 c = 0; c < 4; ++c)
		{
			const int32 ci = I + (c & 1), cj = J + (c >> 1);
			if (!Wl.Valid(ci, cj)) continue;
			const float K = ((c & 1) ? TX : 1.f - TX) * ((c >> 1) ? TY : 1.f - TY);
			const float V = Wl.Wt[cj * Wl.W + ci];
			Wt += V * K;
			D += Wl.Out[cj * Wl.W + ci] * (V * K);
		}
		if (Wt <= 0.f) return 0.f;
		OutDir = D.GetSafeNormal();
		return Wt;
	}
	/** The ribs down a wall (0 in a runnel .. 1 on a rib): noise stretched up the face, so its ridges run top to bottom. */
	float LcWallRib(const FVector& P)
	{
		const float R1 = 1.f - FMath::Abs(LcN3(FVector(P.X / 360.f, P.Y / 360.f, P.Z / 2600.f)));
		const float R2 = 1.f - FMath::Abs(LcN3(FVector(P.X / 130.f + 5.f, P.Y / 130.f + 2.f, P.Z / 1100.f)));
		return FMath::Clamp(0.7f * R1 * R1 + 0.3f * R2 * R2, 0.f, 1.f);
	}
	/** How far the ice stands out of the land at a point of a wall of weight Wt (cm): over half a metre everywhere (the
	 *  land's far detail never shows through), the ribs up to a metre and a half more, set back in steps where its layers
	 *  end, lumpy. */
	float LcWallPush(const FVector& P, float Wt)
	{
		const float Step = LcSm(-0.2f, 0.2f, LcN2(P.Z / 460.f, (P.X + P.Y) / 3400.f));
		return Wt * (70.f + 120.f * LcWallRib(P) - 30.f * Step + 16.f * LcN3(P / 85.f + FVector(2.f, 9.f, 4.f)));
	}
	/** Every rock cube (S cm) the Tundra's walls reach: steep ice samples, a cube's height either side of each. */
	void LcWallCubes(const FWRLandDef* L, float S, TSet<FIntVector>& Keys)
	{
		const float From = LcWallFrom() - 2.f, To = LcWallTo();
		for (int32 J = 1; J < L->HfN - 1; ++J) for (int32 I = 1; I < L->HfN - 1; ++I)
		{
			if (LcWallWeight0(L, I, J, From, To) <= 0.f) continue;
			// (a sample's face reaches 2 m either side of it and its bulge 2 m out: 3 m and 4 m round it take in the cubes it
			// touches; at 5 m and 7 m the walls queued half again as many cubes, empty)
			const FVector W = L->ToWorld(L->HfOrigin + FVector2D(float(I), float(J)) * L->HfStep, L->HfSample(I, J));
			for (int32 dy = -1; dy <= 1; ++dy) for (int32 dx = -1; dx <= 1; ++dx) for (const float dz : { -400.f, 0.f, 400.f })
			{
				Keys.Add(FIntVector(FMath::FloorToInt((W.X + dx * 300.f) / S), FMath::FloorToInt((W.Y + dy * 300.f) / S), FMath::FloorToInt((W.Z + dz) / S)));
			}
		}
	}
	/** The open crevasses' air at a world point (cm, negative inside; huge where there's none; round 14: the Crevasse
	 *  Maze cut into the glacier). Each slot a trench down its field's slope: its walls lean in a little with depth and are
	 *  fluted, its floor Depth under the ground's surface, climbing to it over Ramp at each end. GroundZ: the ground's
	 *  height there (world cm). */
	float LcSlotAir(const FWRLandDef* L, const FVector& P, float GroundZ)
	{
		float Best = 1.0e4f;
		if (L->Crevasses.Num() == 0) return 1.0e6f;
		const FVector2D M = L->ToMap(P);
		const float Below = (GroundZ - float(P.Z)) / 100.f;
		for (const FWRLandCrevasses& F : L->Crevasses)
		{
			if (FVector2D::DistSquared(M, F.At) > 70.f * 70.f) continue;
			for (const FWRLandCrevasses::FSlot& S : F.Slots)
			{
				const FVector2D D = M - S.C;
				const float A = float(FVector2D::DotProduct(D, F.V)), Lat = float(FVector2D::DotProduct(D, F.U));
				const float Out = LcSm(S.HalfLen, S.HalfLen + S.Ramp, FMath::Abs(A));
				const float Floor = S.Depth * (1.f - Out);
				const float W = S.HalfW * (1.f - 0.2f * FMath::Clamp(Below / FMath::Max(0.1f, S.Depth), 0.f, 1.f)) * (1.f - 0.3f * Out);
				const float Wn = 0.2f * LcN3(P / 150.f) + 0.07f * LcN3(P / 45.f + FVector(5.f, 2.f, 9.f));
				Best = FMath::Min(Best, FMath::Max(FMath::Abs(Lat) - W + Wn, Below - Floor));
			}
		}
		return Best * 100.f;
	}
	/** A crevasse field reaches this cube (world cm box). */
	bool LcSlotsNear(const FWRLandDef* L, const FBox& B)
	{
		for (const FWRLandCrevasses& F : L->Crevasses)
		{
			const FVector W = L->ToWorld(F.At, 0.f);
			if (FMath::Abs(W.X - B.GetCenter().X) < B.GetExtent().X + 6000.f && FMath::Abs(W.Y - B.GetCenter().Y) < B.GetExtent().Y + 6000.f) return true;
		}
		return false;
	}
	/** The land draws its own ground over this point (no corner of its grid square is cut). */
	bool LcLandDraws(const FWRLandDef* L, float X, float Y)
	{
		const FVector2D M = L->ToMap(FVector(X, Y, 0.f));
		const FVector2D G = (M - L->HfOrigin) / L->HfStep;
		const int32 I = FMath::FloorToInt(G.X), J = FMath::FloorToInt(G.Y);
		for (int32 dj = 0; dj <= 1; ++dj) for (int32 di = 0; di <= 1; ++di)
		{
			if (L->HfSurfaceAt(I + di, J + dj) == FWRLandDef::HfHidden) return false;
		}
		return true;
	}
	/** The caves' air at a world point (cm, negative inside), the rock's noise on the walls (less on the floor). */
	float LcAir(const FWRLandDef* L, const TArray<int32>& Caves, const FVector& P, int32* OutCave = nullptr, float* OutFloorCm = nullptr)
	{
		const FVector2D M = L->ToMap(P);
		const FVector Pm(M.X, M.Y, (P.Z - L->Origin.Z) / 100.f);
		float Best = TNumericLimits<float>::Max(), Floor = 0.f, Width = 1.f;
		int32 Which = INDEX_NONE;
		for (int32 c : Caves)
		{
			float F = 0.f, W = 1.f;
			const float A = L->Caves[c].Air(Pm, &F, &W);
			if (A < Best) { Best = A; Floor = F; Width = W; Which = c; }
		}
		if (Which == INDEX_NONE) return 1.0e6f;
		// The rock's noise: up to 0.7 m on a big chamber's walls, less in a narrow passage (a squeeze stays passable), a
		// little on the floors.
		const float Wall = FMath::Clamp(0.22f * Width, 0.18f, 0.7f);
		const float Amp = FMath::Lerp(0.12f, Wall, LcSm(0.4f, 1.6f, float(Pm.Z) - Floor));
		Best += Amp * (LcN3(P / 220.f) + 0.35f * LcN3(P / 70.f + FVector(5.f, 1.f, 3.f)));
		if (OutCave) *OutCave = Which;
		if (OutFloorCm) *OutFloorCm = L->Origin.Z + Floor * 100.f;
		return Best * 100.f;
	}
}

AWRLandCaves::AWRLandCaves()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRLandCaves::BeginPlay()
{
	Super::BeginPlay();
	// 0.6 m cells (24 m cubes): at 1 m the walls showed their facets.
	Cell = FMath::Max(40.f, WRTune(TEXT("landCaves.cell"), 60.f));
	Cells = FMath::Max(8, int32(WRTune(TEXT("landCaves.cells"), 40.f)));
	for (const FWRLandDef& D : WRLand::All())
	{
		if (D.bHeightfield && D.Caves.Num() > 0 && WRLand::IsLive(D)) { Land = &D; break; }
	}
	if (!Land) { SetActorTickEnabled(false); return; }
	const float S = Cell * Cells;
	TSet<FIntVector> Keys;
	for (const FWRLandCave& C : Land->Caves)
	{
		// The cave's air and the ground round its mouth (the lip's skin), in game cm.
		const FBox Bm = C.Bounds(9.f);
		const FVector A = Land->ToWorld(FVector2D(Bm.Min), Bm.Min.Z), B = Land->ToWorld(FVector2D(Bm.Max), Bm.Max.Z + 3.f);
		const FBox Bw(A.ComponentMin(B) - FVector(Cell * 2.f), A.ComponentMax(B) + FVector(Cell * 2.f));
		const FIntVector Lo(FMath::FloorToInt(Bw.Min.X / S), FMath::FloorToInt(Bw.Min.Y / S), FMath::FloorToInt(Bw.Min.Z / S));
		const FIntVector Hi(FMath::FloorToInt(Bw.Max.X / S), FMath::FloorToInt(Bw.Max.Y / S), FMath::FloorToInt(Bw.Max.Z / S));
		for (int32 z = Lo.Z; z <= Hi.Z; ++z) for (int32 y = Lo.Y; y <= Hi.Y; ++y) for (int32 x = Lo.X; x <= Hi.X; ++x) Keys.Add(FIntVector(x, y, z));
	}
	// The Tundra's ice walls (round 14): the cubes round its steep ice; built after every cube that stands in for ground
	// the land doesn't draw (the caves' mouths, the crevasses: until theirs is built there is nothing to stand on there;
	// queued in no order behind two thousand walls, a bot crossing the maze fell through the world).
	TSet<FIntVector> WallKeys;
	{
		using namespace WRLandCavesImpl;
		GWalls = WRTune(TEXT("landCaves.walls"), 1.f) > 0.f;
		// (from 44 degrees: at 40 the walls took 2,400 cubes, 2.7 GB on a client; the faces worth growing are steeper)
		GWallFrom = WRTune(TEXT("landCaves.wallFrom"), 44.f);
		GWallTo = FMath::Max(GWallFrom + 1.f, WRTune(TEXT("landCaves.wallTo"), 56.f));
		if (GWalls) LcWallCubes(Land, S, WallKeys);
		// and round each open crevasse field: its ground from 9 m under to 5 m over
		for (const FWRLandCrevasses& F : Land->Crevasses)
		{
			float Lo = 1.0e9f, Hi = -1.0e9f;
			for (int32 dy = -4; dy <= 4; ++dy) for (int32 dx = -4; dx <= 4; ++dx)
			{
				const float H = Land->Height(F.At + FVector2D(dx * 10.f, dy * 10.f));
				Lo = FMath::Min(Lo, H); Hi = FMath::Max(Hi, H);
			}
			const FVector A = Land->ToWorld(F.At - FVector2D(48.f, 48.f), Lo - 9.f), B = Land->ToWorld(F.At + FVector2D(48.f, 48.f), Hi + 5.f);
			for (int32 z = FMath::FloorToInt(A.Z / S); z <= FMath::FloorToInt(B.Z / S); ++z)
				for (int32 y = FMath::FloorToInt(A.Y / S); y <= FMath::FloorToInt(B.Y / S); ++y)
					for (int32 x = FMath::FloorToInt(A.X / S); x <= FMath::FloorToInt(B.X / S); ++x) Keys.Add(FIntVector(x, y, z));
		}
	}
	Queue = Keys.Array();
	int32 WallCubes = 0;
	for (const FIntVector& K : WallKeys) if (!Keys.Contains(K)) { Queue.Add(K); ++WallCubes; }
	UE_LOG(LogWildreach, Log, TEXT("LAND CAVES walls: %d more rock cubes round the Tundra's steep ice, after the %d the caves and crevasses need"), WallCubes, Keys.Num());
	bQueued = true;
	BuildFormations();
	UE_LOG(LogWildreach, Log, TEXT("LAND CAVES %s: %d caves, %d rock cubes of %.0f m to build"), *Land->Id.ToString(), Land->Caves.Num(), Queue.Num(), S / 100.f);
}

void AWRLandCaves::EndPlay(const EEndPlayReason::Type Reason)
{
	// The workers touch only their own result and the land's static data: wait for them so none outlives the world.
	for (FJob& J : InFlight) J.Task.Wait();
	InFlight.Reset();
	Super::EndPlay(Reason);
}

void AWRLandCaves::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bQueued) return;
	// The ice's thin places glow with the day (light through the glacier); the skylights throw the day's light about.
	{
		const AWRGameState* GS = GetWorld()->GetGameState<AWRGameState>();
		const float Day = GS ? GS->GetDaylight() : 1.f;
		if (IceLook) IceLook->SetScalarParameterValue(TEXT("Glow"), Day * WRTune(TEXT("landCaves.iceGlow"), 0.35f));
		for (int32 i = 0; i < DayLights.Num(); ++i) if (DayLights[i]) DayLights[i]->SetIntensity(DayLightFull[i] * Day * WRTune(TEXT("landCaves.skyBounce"), 1.f));
	}
	if (bBuiltLogged) return;
	const double Budget = WRTune(TEXT("landCaves.msPerFrame"), 4.f) / 1000.0;
	const double Start = FPlatformTime::Seconds();
	for (int32 i = 0; i < InFlight.Num() && FPlatformTime::Seconds() - Start < Budget;)
	{
		if (!InFlight[i].Task.IsCompleted()) { ++i; continue; }
		const FCubePtr C = InFlight[i].Cube;
		InFlight.RemoveAtSwap(i);
		if (C.IsValid()) FinishCube(*C);
	}
	const int32 MaxInFlight = FMath::Max(1, int32(WRTune(TEXT("landCaves.inFlight"), 4.f)));
	const float GroundSkin = WRTune(TEXT("landCaves.groundSkin"), 120.f);
	while (Next < Queue.Num() && InFlight.Num() < MaxInFlight)
	{
		FCubePtr C = MakeShared<FCube, ESPMode::ThreadSafe>();
		C->Mesh.Key = Queue[Next++];
		const float Ce = Cell;
		const int32 N = Cells;
		const FVector Loc = GetActorLocation();
		const FWRLandDef* L = Land;
		FJob& J = InFlight.AddDefaulted_GetRef();
		J.Cube = C;
		J.Task = UE::Tasks::Launch(UE_SOURCE_LOCATION, [C, L, Ce, N, Loc, GroundSkin]() { ComputeCube(L, C->Mesh.Key, Ce, N, Loc, GroundSkin, *C); });
	}
	if (IsBuilt())
	{
		bBuiltLogged = true;
		SetActorTickInterval(1.f);
		UE_LOG(LogWildreach, Log, TEXT("LAND CAVES built: %d rock meshes"), Meshes);
	}
}

void AWRLandCaves::ComputeCube(const FWRLandDef* L, const FIntVector& Key, float Cell, int32 Cells, const FVector& ActorLoc, float GroundSkin, FCube& Out)
{
	using namespace WRLandCavesImpl;
	const float S = Cell * Cells;
	const FVector O(Key.X * S, Key.Y * S, Key.Z * S);
	// The caves whose air comes near this cube.
	TArray<int32> Caves;
	for (int32 c = 0; c < L->Caves.Num(); ++c)
	{
		const FBox Bm = L->Caves[c].Bounds(9.f);
		const FVector A = L->ToWorld(FVector2D(Bm.Min), Bm.Min.Z), B = L->ToWorld(FVector2D(Bm.Max), Bm.Max.Z + 3.f);
		if (FBox(A.ComponentMin(B), A.ComponentMax(B)).Intersect(FBox(O - FVector(Cell * 2.f), O + FVector(S + Cell * 2.f)))) Caves.Add(c);
	}
	// The Tundra's ice walls round it (round 14), when the cube reaches the ground there.
	FLcWalls Wl;
	if (GWalls)
	{
		const float M = 450.f + Cell * 2.f;
		LcBuildWalls(L, L->ToMap(O - FVector(M, M, 0.f)), L->ToMap(O + FVector(S + M, S + M, 0.f)), Wl);
		if (Wl.bAny && (O.Z > L->ToWorld(FVector2D::ZeroVector, Wl.MaxZ).Z + 500.f || O.Z + S < L->ToWorld(FVector2D::ZeroVector, Wl.MinZ).Z - 500.f)) Wl.bAny = false;
	}
	// The open crevasses (round 14).
	const bool bSlots = LcSlotsNear(L, FBox(O - FVector(Cell * 2.f), O + FVector(S + Cell * 2.f)));
	if (Caves.Num() == 0 && !Wl.bAny && !bSlots) { Out.Mesh.bSkipped = true; return; }
	Out.Biome = Caves.Num() > 0 ? L->Caves[Caves[0]].Biome : L->BiomeAtMap(L->ToMap(O + FVector(S * 0.5f)));
	if (Out.Biome != TEXT("volcanic")) Out.Biome = TEXT("tundra");
	const bool bIce = Out.Biome == TEXT("tundra");
	const int32 NS = Cells + 3, NC = Cells + 2;
	TArray<float> Ground;
	Ground.SetNumUninitialized(NS * NS);
	for (int32 j = 0; j < NS; ++j) for (int32 i = 0; i < NS; ++i) Ground[j * NS + i] = LcGroundZ(L, O.X + (i - 1) * Cell, O.Y + (j - 1) * Cell);
	// Each column's wall weight and way out (they don't change with height; the push does).
	TArray<float> WallW;
	TArray<FVector2f> WallD;
	if (Wl.bAny)
	{
		WallW.SetNumZeroed(NS * NS);
		WallD.SetNumZeroed(NS * NS);
		for (int32 j = 0; j < NS; ++j) for (int32 i = 0; i < NS; ++i) WallW[j * NS + i] = LcWallAt(L, Wl, O.X + (i - 1) * Cell, O.Y + (j - 1) * Cell, WallD[j * NS + i]);
	}
	// The field: the caves' air unioned with the air over the ground (negative in either).
	TArray<float> F;
	F.SetNumUninitialized(NS * NS * NS);
	ParallelFor(NS, [&](int32 k)
	{
		for (int32 j = 0; j < NS; ++j) for (int32 i = 0; i < NS; ++i)
		{
			const FVector P = O + FVector((i - 1) * Cell, (j - 1) * Cell, (k - 1) * Cell);
			// The crags at the mouths are rock added to the ground (joined to the hillside smoothly), and the cave's air is
			// carved through both: the way in is an arch through the crag, rock down to the ground either side. (Laid
			// over the air instead, a crag sat on the hillside like a balanced boulder over a hole.)
			// (an ice wall: the ground pushed out of the face, LcWallGroundZ)
			float Gz = Ground[j * NS + i];
			if (Wl.bAny && WallW[j * NS + i] > 0.f)
			{
				const FVector2f D = WallD[j * NS + i];
				const float Push = LcWallPush(P, WallW[j * NS + i]);
				Gz = FMath::Max(Gz, LcGroundZ(L, P.X - D.X * Push, P.Y - D.Y * Push));
			}
			const float G = Gz - float(P.Z), Bw = -LcBrow(L, Caves, P);
			const float Hk = FMath::Max(50.f - FMath::Abs(G - Bw), 0.f) / 50.f;
			const float Air = bSlots ? FMath::Min(LcAir(L, Caves, P), LcSlotAir(L, P, Ground[j * NS + i])) : LcAir(L, Caves, P);
			F[(k * NS + j) * NS + i] = FMath::Min(Air, FMath::Max(G, Bw) + Hk * Hk * 50.f * 0.25f);
		}
	});
	const auto Fv = [&](int32 i, int32 j, int32 k) { return F[((k + 1) * NS + (j + 1)) * NS + (i + 1)]; };
	// Nothing crosses zero in this cube (all rock or all air): no skin.
	{
		bool bNeg = false, bPos = false;
		for (const float V : F) { bNeg |= V < 0.f; bPos |= V >= 0.f; if (bNeg && bPos) break; }
		if (!bNeg || !bPos) { Out.Mesh.bSkipped = true; return; }
	}
	TArray<int32> CellVert;
	CellVert.Init(INDEX_NONE, NC * NC * NC);
	TArray<FVector> Verts, Normals;
	TArray<FColor> Colours;
	TArray<FVector2D> UVs;
	TArray<bool> UnderLand;   // the land draws all round this vertex: a quad with all four such is dropped
	TArray<float> Depth;   // each vertex's depth under the ground's surface (cm)
	static const int32 Corner[8][3] = { {0,0,0},{1,0,0},{0,1,0},{1,1,0},{0,0,1},{1,0,1},{0,1,1},{1,1,1} };
	static const int32 Edge[12][2] = { {0,1},{2,3},{4,5},{6,7},{0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7} };
	for (int32 k = -1; k <= Cells; ++k) for (int32 j = -1; j <= Cells; ++j) for (int32 i = -1; i <= Cells; ++i)
	{
		float Cv[8];
		int32 Neg = 0;
		for (int32 c = 0; c < 8; ++c) { Cv[c] = Fv(i + Corner[c][0], j + Corner[c][1], k + Corner[c][2]); Neg += Cv[c] < 0.f; }
		if (Neg == 0 || Neg == 8) continue;
		FVector Sum = FVector::ZeroVector;
		int32 Cnt = 0;
		for (int32 e = 0; e < 12; ++e)
		{
			const float A = Cv[Edge[e][0]], B = Cv[Edge[e][1]];
			if ((A < 0.f) == (B < 0.f)) continue;
			const float T = A / (A - B);
			Sum += FMath::Lerp(FVector(Corner[Edge[e][0]][0], Corner[Edge[e][0]][1], Corner[Edge[e][0]][2]), FVector(Corner[Edge[e][1]][0], Corner[Edge[e][1]][1], Corner[Edge[e][1]][2]), T);
			++Cnt;
		}
		FVector P = O + (FVector(i, j, k) + Sum / FMath::Max(1, Cnt)) * Cell;
		const FVector G((Cv[1] - Cv[0]) + (Cv[3] - Cv[2]) + (Cv[5] - Cv[4]) + (Cv[7] - Cv[6]), (Cv[2] - Cv[0]) + (Cv[3] - Cv[1]) + (Cv[6] - Cv[4]) + (Cv[7] - Cv[5]),
			(Cv[4] - Cv[0]) + (Cv[5] - Cv[1]) + (Cv[6] - Cv[2]) + (Cv[7] - Cv[3]));
		const FVector Nrm = (-G).GetSafeNormal();
		// The ground's skin (its term the nearer to zero: not the cave's wall or thin roof), and of that, the part on ground
		// the land draws itself (sunk 4 cm under the land's, which wins where both are). A floor lying at the ground's own
		// level is the ground's too: a passage's first metres run in at the ground's height, and drawn as the cave's rock
		// they showed as squares of ice in a tube corridor's floor at each room's doorway.
		const float GroundTerm = LcGroundZ(L, P.X, P.Y) - float(P.Z);
		const float AirHere = bSlots ? FMath::Min(LcAir(L, Caves, P), LcSlotAir(L, P, GroundTerm + float(P.Z))) : LcAir(L, Caves, P);
		const float BrowHere = -LcBrow(L, Caves, P);
		// An ice wall's face (the pushed ground well over the land's own here) is the wall's, never the ground's skin.
		float WallHere = 0.f, WallLift = 0.f;
		if (Wl.bAny)
		{
			FVector2f D;
			WallHere = LcWallAt(L, Wl, P.X, P.Y, D);
			if (WallHere > 0.f)
			{
				const float Push = LcWallPush(P, WallHere);
				WallLift = LcGroundZ(L, P.X - D.X * Push, P.Y - D.Y * Push) - (GroundTerm + float(P.Z));
			}
		}
		// (at 15 cm the bulge's thin edge along a wall's foot counted as ground and was dropped in a ragged line: a sheet
		// lying a few cm over the snow with a cracked edge)
		const bool bWall = WallLift > 2.f;
		const bool bBrowSkin = BrowHere >= GroundTerm - 5.f && BrowHere <= AirHere + 5.f;
		const bool bGroundSkin = !bBrowSkin && !bWall && (AirHere > GroundTerm + 5.f || (Nrm.Z > 0.7f && FMath::Abs(GroundTerm) < 30.f));
		// (round 13: a brow's skin lying on the ground, away from the cave's air, is the ground's too: drawn as the lip over
		// ground the land draws as well, the two fought, a patchy grey square on the snow in front of each Tundra mouth)
		const bool bOnGround = !bWall && FMath::Abs(GroundTerm) < 20.f && Nrm.Z > 0.5f && AirHere > 60.f;
		const bool bUnderLand = (bGroundSkin || bOnGround) && FMath::Abs(GroundTerm) < GroundSkin && LcLandDraws(L, P.X, P.Y);
		// Dropped only where the land draws all round it, as far as a quad of the skin reaches (two cells): a quad whose
		// corners all lay on drawn ground could still reach over the corner of a cut square between them, and was dropped
		// with it, leaving a sliver of nothing along the cut's edge (round 14: hairlines across the snow in front of the
		// Hollow Ice's mouth).
		bool bLandAround = bUnderLand;
		for (int32 d = 0; d < 8 && bLandAround; ++d)
		{
			static const int32 Off[8][2] = { {1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1} };
			bLandAround = LcLandDraws(L, P.X + Off[d][0] * 2.f * Cell, P.Y + Off[d][1] * 2.f * Cell);
		}
		if (bUnderLand) P.Z -= 4.f;
		CellVert[((k + 1) * NC + (j + 1)) * NC + (i + 1)] = Verts.Num();
		Verts.Add(P - ActorLoc);
		UnderLand.Add(bLandAround);
		Depth.Add(GroundTerm);
		Normals.Add(Nrm);
		// The rock's UV: slowly along the ground, height up the wall (M_HeldTool's grain runs along U, so the ice's grain
		// lies in level strata; projected from above it ran down every wall in streaks). The lip's are made again triangle by
		// triangle below, as the land's.
		const FVector2D M = L->ToMap(P);
		UVs.Add(FVector2D((M.X + M.Y) / 40.f, float(P.Z) / 400.f));
		// The land's own vertex data here, bilinear from its four samples.
		const float GX = (M.X - L->HfOrigin.X) / L->HfStep, GY = (M.Y - L->HfOrigin.Y) / L->HfStep;
		const int32 I0 = FMath::FloorToInt(GX), J0 = FMath::FloorToInt(GY);
		const float TX = GX - I0, TY = GY - J0;
		FLinearColor C(0.f, 0.f, 0.f, 0.f);
		float Mix[6] = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
		for (int32 c = 0; c < 4; ++c)
		{
			const FIntPoint GS = LcGroundSample(L, I0 + (c & 1), J0 + (c >> 1));
			const int32 SI = GS.X, SJ = GS.Y;
			const float Wt = ((c & 1) ? TX : 1.f - TX) * ((c >> 1) ? TY : 1.f - TY);
			float Rough = 0.9f;
			FLinearColor Gc = L->HfGround(SI, SJ, Nrm, Rough);
			Gc.A = 1.f - Rough;
			C += Gc * Wt;
			if (L->bLayered) { float Lm[6]; L->HfLayers(SI, SJ, Lm); for (int32 m = 0; m < 6; ++m) Mix[m] += Lm[m] * Wt; }
		}
		Out.Colours.Add(C);
		{
			// The cave's own rock. Only smooth values per vertex (the ice's strata and the basalt's grain are drawn per pixel:
			// a pattern that wraps, worked out per vertex, broke into sharp triangles).
			float FloorCm = 0.f;
			int32 InCave = INDEX_NONE;
			LcAir(L, Caves, P, &InCave, &FloorCm);
			// Moss where a skylight's light falls on the floor and its debris cone (the floor round the hole, facing up).
			float Moss = 0.f;
			if (InCave != INDEX_NONE)
			{
				const FVector2D Mv = L->ToMap(P);
				for (const FWRLandCaveShape& Sh : L->Caves[InCave].Shapes)
				{
					if (Sh.Kind != 2 || Sh.Talus.Y <= 0.f) continue;
					const float Dm = float(FVector2D::Distance(Mv, FVector2D(Sh.C)));
					Moss = FMath::Max(Moss, (1.f - LcSm(float(Sh.R.X) + 0.3f, float(Sh.Talus.Z) + 0.8f, Dm)) * LcSm(0.35f, 0.75f, Nrm.Z) * (0.6f + 0.4f * LcSm(-0.4f, 0.4f, LcN3(P / 90.f))));
				}
			}
			const float Above = (float(P.Z) - FloorCm) / 100.f;
			// 1 at the ground's surface, 0 by 2.5 m under it, and 0 again by 1.5 m over it (a crag's face is rock, not ground).
			const float Lip = GroundTerm >= 0.f ? LcSm(-250.f, -60.f, -GroundTerm) : 1.f - LcSm(40.f, 150.f, -GroundTerm);
			FLinearColor Rc;
			float RRough;
			FVector2D U1(0.f, 0.f), U2(0.f, 0.f);
			if (bIce)
			{
				// Snow drifted on the floors; ice elsewhere. Thin cover (under 2.5-6.5 m) lets the day through; the
				// deeper the darker.
				// A crag's outside, out in the weather away from the cave's air, is old ice plastered with snow and rime,
				// the blue showing through in places; the clean blue is the arch's inside and the cave's (round 13: the
				// mouths were blue jelly on the snow).
				const float Weather = LcSm(60.f, 220.f, AirHere) * LcSm(20.f, 120.f, -GroundTerm) * (0.6f + 0.35f * LcSm(-0.5f, 0.5f, LcN3(P / 160.f + FVector(3.f, 1.f, 5.f))));
				const float Snow = FMath::Max(LcSm(0.45f, 0.75f, Nrm.Z), Weather);
				const FLinearColor SnowC = FMath::Lerp(LcLin(214, 226, 238), LcLin(190, 206, 224), LcSm(-0.3f, 0.5f, LcN3(P / 120.f)));
				Rc = FMath::Lerp(SnowC, C, Lip);
				RRough = FMath::Lerp(0.8f, 1.f - C.A, Lip);
				U1 = FVector2D((1.f - Snow) * (1.f - Lip), GroundTerm > 0.f ? LcSm(450.f, 150.f, GroundTerm) : 0.f);
				U2 = FVector2D(LcSm(500.f, 1500.f, GroundTerm), 0.f);
				// An ice wall out in the open (round 14): weathered white-blue ice, the glacier's strata faint over it and
				// bluer down the runnels between its ribs; snow on its ledges and top, rime plastered on in patches.
				const float K = LcSm(0.f, 0.3f, WallHere) * LcSm(150.f, 400.f, AirHere);
				if (K > 0.f)
				{
					const float Rib = LcWallRib(P);
					const float Up = LcSm(0.5f, 0.8f, Nrm.Z);
					const float Rime = 0.45f * LcSm(0.05f, 0.6f, LcN3(P / 320.f + FVector(4.f, 7.f, 1.f)));
					const float WSnow = FMath::Max(Up, Rime);
					const FLinearColor Weathered = FMath::Lerp(LcLin(214, 226, 236), LcLin(178, 200, 222), LcSm(-0.4f, 0.4f, LcN3(P / 170.f)));
					const FLinearColor WRc = FMath::Lerp(FMath::Lerp(Weathered, SnowC, WSnow), C, Lip);
					const float WRough = FMath::Lerp(FMath::Lerp(0.5f, 0.8f, WSnow), 1.f - C.A, Lip);
					// (at 0.3-0.85 the strata ran across every face in bold blue and white stripes a metre apart)
					const float WIce = (0.08f + 0.4f * FMath::Square(1.f - Rib)) * (1.f - WSnow) * (1.f - Lip);
					Rc = FMath::Lerp(Rc, WRc, K);
					RRough = FMath::Lerp(RRough, WRough, K);
					U1 = FVector2D(FMath::Lerp(float(U1.X), WIce, K), U1.Y * (1.f - K));
					U2 *= 1.f - K;
				}
			}
			else
			{
				// Basalt: dark, rusty where the iron weathered, glazed (darker, glossy) up the walls and over the roof where
				// the flow ran, dustier and rougher on the floor.
				const float N1 = LcN3(P / 520.f), N2 = LcN3(P / 170.f + FVector(7.f, 3.f, 1.f));
				FLinearColor B = FMath::Lerp(LcLin(46, 43, 41), LcLin(64, 60, 56), LcSm(-0.4f, 0.6f, N2));
				B = FMath::Lerp(B, LcLin(86, 54, 36), 0.7f * LcSm(0.35f, 0.75f, N1));
				const float OnFloor = LcSm(0.55f, 0.85f, Nrm.Z), Glaze = LcSm(0.9f, 2.2f, Above) * (1.f - OnFloor);
				B = FMath::Lerp(B, LcLin(34, 34, 38), 0.7f * Glaze);
				B = FMath::Lerp(B, LcLin(72, 66, 60), 0.6f * OnFloor);
				B = FMath::Lerp(B, FMath::Lerp(LcLin(38, 52, 22), LcLin(58, 66, 30), LcSm(-0.3f, 0.5f, LcN3(P / 45.f))), 0.85f * Moss);
				// Seep water: a puddle stands level in the floor's hollows (its waterline from cave_layout: the floor's lowest
				// 40% under its footprint), the rock round it damp and darker. UV1.y = how far under the waterline, +0.3 (m,
				// 0..0.6: 0 dry, over 0.3 under water). It changes linearly across each triangle, so the material finds the
				// exact line where the level water meets the rock, as ragged as the floor. Past the footprint the waterline
				// falls away.
				// A seam still glowing (crack): the floor round its zig-zag hot (UV2.x = heat: M_CaveRock draws the lava's crack
				// pattern glowing in it), the crust there black and glassy. Sulfur round a gas vent: a yellow and white crust.
				float Water = 0.f, Damp = 0.f, Heat = 0.f, Sulf = 0.f, SeamD = 1.f, Seam = 1.f;
				if (InCave != INDEX_NONE)
				{
					const FVector2D Mv = L->ToMap(P);
					const float Vz = (float(P.Z) - L->Origin.Z) / 100.f;
					for (const FWRLandCaveFeat& Pf : L->Caves[InCave].Feats)
					{
						const float Dx = float(Mv.X - Pf.At.X), Dy = float(Mv.Y - Pf.At.Y);
						if (Pf.Kind == TEXT("puddle"))
						{
							const float Ya = FMath::DegreesToRadians(Pf.Yaw);
							const float E = FMath::Sqrt(FMath::Square((Dx * FMath::Cos(Ya) + Dy * FMath::Sin(Ya)) / FMath::Max(0.2f, Pf.A))
								+ FMath::Square((-Dx * FMath::Sin(Ya) + Dy * FMath::Cos(Ya)) / FMath::Max(0.2f, Pf.B))) + 0.25f * LcN3(P / 70.f);
							const float Under = float(Pf.At.Z) - Vz - 0.5f * FMath::Max(0.f, E - 0.8f);
							Water = FMath::Max(Water, FMath::Clamp(Under + 0.3f, 0.f, 0.6f));
							Damp = FMath::Max(Damp, (1.f - LcSm(0.9f, 1.7f, E)) * OnFloor);
						}
						else if (Pf.Kind == TEXT("crack"))
						{
							const FVector2f Dir(FMath::Cos(FMath::DegreesToRadians(Pf.Yaw)), FMath::Sin(FMath::DegreesToRadians(Pf.Yaw)));
							const float Along = FMath::Clamp(Dx * Dir.X + Dy * Dir.Y, 0.f, Pf.A);
							const float D = FMath::Sqrt(FMath::Square(Dx - Dir.X * Along) + FMath::Square(Dy - Dir.Y * Along));
							Heat = FMath::Max(Heat, (1.f - LcSm(0.1f, 0.8f, D)) * OnFloor * (1.f - LcSm(0.6f, 1.2f, FMath::Abs(Vz - float(Pf.At.Z)))));
							// The fissure itself: the signed distance across the nearest leg of the zig-zag (UV2.y; linear across
							// each triangle, so the material finds its line exactly).
							if (D < SeamD) { SeamD = D; Seam = Dx * Dir.Y - Dy * Dir.X >= 0.f ? D : -D; }
						}
						else if (Pf.Kind == TEXT("sulfur"))
						{
							const float D = FMath::Sqrt(Dx * Dx + Dy * Dy) + 0.2f * LcN3(P / 40.f);
							Sulf = FMath::Max(Sulf, (1.f - LcSm(Pf.A * 0.5f, Pf.A * 1.6f + 0.4f, D)) * OnFloor);
						}
					}
				}
				B = FMath::Lerp(B, LcLin(20, 19, 19), 0.75f * Heat);
				B = FMath::Lerp(B, FMath::Lerp(LcLin(170, 146, 52), LcLin(178, 172, 150), LcSm(-0.2f, 0.5f, LcN3(P / 25.f + FVector(3.f, 1.f, 0.f)))), 0.8f * Sulf);
				B *= 1.f - 0.45f * Damp;
				Rc = FMath::Lerp(B, C, Lip);
				RRough = FMath::Lerp(FMath::Lerp(FMath::Lerp(FMath::Lerp(FMath::Lerp(0.78f, 0.36f, Glaze), 0.92f, OnFloor), 0.97f, Moss), 0.5f, Heat), 1.f - C.A, Lip);
				U1 = FVector2D(Above, Water * (1.f - Lip));
				U2 = FVector2D(Heat * (1.f - Lip), FMath::Clamp(Seam, -1.f, 1.f));
			}
			Out.RockColours.Add(FWRToolMesh::Pack(Rc, RRough));
			Out.RockUV1.Add(U1);
			Out.RockUV2.Add(U2);
			Out.LipTangentX.Add(FVector(1.f, 0.f, 0.f));
		}
		Out.UV1.Add(FVector2D(Mix[0], Mix[1]));
		Out.UV2.Add(FVector2D(Mix[2], Mix[3]));
		Out.UV3.Add(FVector2D(Mix[4], Mix[5]));
		Colours.Add(FWRToolMesh::Pack(C, 1.f - C.A));
	}
	const auto CV = [&](int32 i, int32 j, int32 k) { return CellVert[((k + 1) * NC + (j + 1)) * NC + (i + 1)]; };
	TArray<int32> Tris, Lip;
	// The lip (drawn as the land) is the skin less than LipDepth under the ground's surface: the ground round a mouth or a
	// doorway and the first of its rim; deeper is the cave's rock. A triangle across that depth is cut where it crosses
	// it, so the land's look gives way to the rock's along a smooth line (taken quad by quad, it changed in 1 m steps, and
	// the Tundra's mouths showed a stair of snow against the ice). The ice's colours fade to the ground's by then.
	constexpr float LipDepth = 90.f, LipAbove = 50.f;
	// > 0 inside the lip's band (from LipAbove over the ground's surface to LipDepth under it).
	const auto LipS = [&](float D) { return FMath::Min(LipDepth - D, D + LipAbove); };
	TMap<uint64, int32> Mids;
	const auto LerpPacked = [](const FColor& A, const FColor& B, float T)
	{
		return FColor(uint8(FMath::RoundToInt(FMath::Lerp(float(A.R), float(B.R), T))), uint8(FMath::RoundToInt(FMath::Lerp(float(A.G), float(B.G), T))),
			uint8(FMath::RoundToInt(FMath::Lerp(float(A.B), float(B.B), T))), uint8(FMath::RoundToInt(FMath::Lerp(float(A.A), float(B.A), T))));
	};
	// The vertex where an edge crosses LipDepth (made once per edge, so the triangles either side share it: no crack).
	const auto Mid = [&](int32 X, int32 Y)
	{
		const uint64 Key = (uint64(FMath::Min(X, Y)) << 32) | uint64(FMath::Max(X, Y));
		if (const int32* Found = Mids.Find(Key)) return *Found;
		const int32 Lo = FMath::Min(X, Y), Hi = FMath::Max(X, Y);
		const float SL = LipS(Depth[Lo]), SH = LipS(Depth[Hi]);
		const float T = FMath::Clamp(SL / (SL - SH), 0.f, 1.f);
		const int32 N = Verts.Num();
		const FVector V = FMath::Lerp(Verts[Lo], Verts[Hi], T), Nm = FMath::Lerp(Normals[Lo], Normals[Hi], T).GetSafeNormal();
		const FVector2D U = FMath::Lerp(UVs[Lo], UVs[Hi], T);
		const FColor Cp = LerpPacked(Colours[Lo], Colours[Hi], T);
		const FLinearColor Cl = FMath::Lerp(Out.Colours[Lo], Out.Colours[Hi], T);
		const FVector2D U1 = FMath::Lerp(Out.UV1[Lo], Out.UV1[Hi], T), U2 = FMath::Lerp(Out.UV2[Lo], Out.UV2[Hi], T), U3 = FMath::Lerp(Out.UV3[Lo], Out.UV3[Hi], T);
		Verts.Add(V);
		Normals.Add(Nm);
		UVs.Add(U);
		Colours.Add(Cp);
		Out.Colours.Add(Cl);
		Out.UV1.Add(U1);
		Out.UV2.Add(U2);
		Out.UV3.Add(U3);
		{
			const FColor Rc = LerpPacked(Out.RockColours[Lo], Out.RockColours[Hi], T);
			const FVector2D R1 = FMath::Lerp(Out.RockUV1[Lo], Out.RockUV1[Hi], T), R2 = FMath::Lerp(Out.RockUV2[Lo], Out.RockUV2[Hi], T);
			Out.RockColours.Add(Rc);
			Out.RockUV1.Add(R1);
			Out.RockUV2.Add(R2);
			Out.LipTangentX.Add(FVector(1.f, 0.f, 0.f));
		}
		UnderLand.Add(false);
		Depth.Add(FMath::Lerp(Depth[Lo], Depth[Hi], T));
		Mids.Add(Key, N);
		return N;
	};
	// A triangle (in its facing order) into the lip, the rock, or cut between them.
	const auto Emit = [&](int32 X, int32 Y, int32 Z)
	{
		const int32 V[3] = { X, Y, Z };
		bool bLip[3];
		for (int32 n = 0; n < 3; ++n) bLip[n] = LipS(Depth[V[n]]) >= 0.f;
		if (bLip[0] && bLip[1] && bLip[2]) { Lip.Append({ X, Y, Z }); return; }
		if (!bLip[0] && !bLip[1] && !bLip[2]) { Tris.Append({ X, Y, Z }); return; }
		int32 Lone = 0;
		for (int32 n = 0; n < 3; ++n) if (bLip[n] != bLip[(n + 1) % 3] && bLip[n] != bLip[(n + 2) % 3]) Lone = n;
		const int32 A = V[Lone], B = V[(Lone + 1) % 3], C = V[(Lone + 2) % 3];
		const int32 AB = Mid(A, B), AC = Mid(A, C);
		TArray<int32>& Own = bLip[Lone] ? Lip : Tris;
		TArray<int32>& Rest = bLip[Lone] ? Tris : Lip;
		Own.Append({ A, AB, AC });
		Rest.Append({ AB, B, C });
		Rest.Append({ AB, C, AC });
	};
	const auto Quad = [&](int32 A, int32 B, int32 C, int32 E, const FVector& RockDir)
	{
		if (A < 0 || B < 0 || C < 0 || E < 0) return;
		// The skin lying on ground the land draws itself: the land's.
		if (UnderLand[A] && UnderLand[B] && UnderLand[C] && UnderLand[E]) return;
		// Each triangle faces the air on its own, judged by the surface's own normal at its corners (toward the air): the
		// grid axis of its edge misjudged triangles lying edge-on to it, and those showed as holes you saw the sky through.
		const auto Tri = [&](int32 X, int32 Y, int32 Z)
		{
			const FVector Air = Normals[X] + Normals[Y] + Normals[Z];
			const FVector Toward = Air.IsNearlyZero() ? -RockDir : Air;
			if (FVector::DotProduct(FVector::CrossProduct(Verts[Y] - Verts[X], Verts[Z] - Verts[X]), -Toward) >= 0.f) Emit(X, Y, Z);
			else Emit(X, Z, Y);
		};
		Tri(A, B, C);
		Tri(A, C, E);
	};
	for (int32 k = 0; k < Cells; ++k) for (int32 j = 0; j < Cells; ++j) for (int32 i = 0; i < Cells; ++i)
	{
		const float V0 = Fv(i, j, k);
		const float Sign = V0 < 0.f ? 1.f : -1.f;
		if ((V0 < 0.f) != (Fv(i + 1, j, k) < 0.f)) Quad(CV(i, j - 1, k - 1), CV(i, j, k - 1), CV(i, j, k), CV(i, j - 1, k), FVector(Sign, 0.f, 0.f));
		if ((V0 < 0.f) != (Fv(i, j + 1, k) < 0.f)) Quad(CV(i - 1, j, k - 1), CV(i - 1, j, k), CV(i, j, k), CV(i, j, k - 1), FVector(0.f, Sign, 0.f));
		if ((V0 < 0.f) != (Fv(i, j, k + 1) < 0.f)) Quad(CV(i - 1, j - 1, k), CV(i, j - 1, k), CV(i, j, k), CV(i - 1, j, k), FVector(0.f, 0.f, Sign));
	}
	// The lip's UVs, triangle by triangle: from above on ground, from the side on a steep face (map metres / 4, as the
	// land's), each triangle with corners of its own. (With the projection chosen per corner, a triangle whose corners
	// were projected two ways smeared the ground's texture across it in streaks; down a steep face projected from above
	// it ran in streaks too.)
	if (Lip.Num() > 0)
	{
		const auto Copy = [&](int32 V, const FVector2D& U, const FVector& TanX)
		{
			const FVector Vp = Verts[V], Nm = Normals[V];
			const FColor Cp = Colours[V];
			const FLinearColor Cl = Out.Colours[V];
			const FVector2D U1 = Out.UV1[V], U2 = Out.UV2[V], U3 = Out.UV3[V];
			const FColor Rc = Out.RockColours[V];
			const FVector2D R1 = Out.RockUV1[V], R2 = Out.RockUV2[V];
			const float Dp = Depth[V];
			const int32 N = Verts.Num();
			Verts.Add(Vp);
			Normals.Add(Nm);
			UVs.Add(U);
			Colours.Add(Cp);
			Out.Colours.Add(Cl);
			Out.UV1.Add(U1);
			Out.UV2.Add(U2);
			Out.UV3.Add(U3);
			Out.RockColours.Add(Rc);
			Out.RockUV1.Add(R1);
			Out.RockUV2.Add(R2);
			Out.LipTangentX.Add(TanX);
			UnderLand.Add(false);
			Depth.Add(Dp);
			return N;
		};
		TArray<int32> Own;
		Own.Reserve(Lip.Num());
		for (int32 t = 0; t + 2 < Lip.Num(); t += 3)
		{
			const int32 Tri[3] = { Lip[t], Lip[t + 1], Lip[t + 2] };
			const FVector Fn = FVector::CrossProduct(Verts[Tri[1]] - Verts[Tri[0]], Verts[Tri[2]] - Verts[Tri[0]]).GetSafeNormal();
			const int32 Proj = FMath::Abs(Fn.Z) >= 0.6f ? 0 : FMath::Abs(Fn.X) > FMath::Abs(Fn.Y) ? 1 : 2;
			for (const int32 V : Tri)
			{
				const FVector P = Verts[V] + ActorLoc;
				const FVector2D M = L->ToMap(P);
				const float Zm = float(P.Z) / 100.f;
				// (The tangent runs along U as projected: the ground's normal map lit the right way round.) Where the land
				// keeps its shore's wetness in the first UV's X (WRLand HfWet, M_IslandGround: the round island), the lip
				// keeps it there too: given map metres, every lip east of the island's middle drew as wet ground (dark and
				// glossy), a grey square on the snow in front of each Tundra mouth (round 13).
				const float Wet = L->HfWet(FMath::RoundToInt((M.X - L->HfOrigin.X) / L->HfStep), FMath::RoundToInt((M.Y - L->HfOrigin.Y) / L->HfStep));
				const FVector2D U = Wet >= 0.f ? FVector2D(Wet, 0.f) : Proj == 0 ? M / 4.f : Proj == 1 ? FVector2D(M.Y, Zm) / 4.f : FVector2D(M.X, Zm) / 4.f;
				Own.Add(Copy(V, U, Proj == 1 ? FVector(0.f, 1.f, 0.f) : FVector(1.f, 0.f, 0.f)));
			}
		}
		Lip = MoveTemp(Own);
	}
	if (Tris.Num() == 0 && Lip.Num() == 0) return;
	// Only the vertices a triangle uses, renumbered (round 14: a cube kept every vertex of the ground's skin it dropped,
	// with the walls most of its memory).
	{
		TArray<int32> Map;
		Map.Init(INDEX_NONE, Verts.Num());
		int32 Used = 0;
		for (const int32 I : Tris) if (Map[I] == INDEX_NONE) Map[I] = Used++;
		for (const int32 I : Lip) if (Map[I] == INDEX_NONE) Map[I] = Used++;
		if (Used < Verts.Num())
		{
			const auto Pack = [&Map, Used](auto& Arr)
			{
				using FElem = typename std::remove_reference_t<decltype(Arr)>::ElementType;
				TArray<FElem> Kept;
				Kept.SetNum(Used);
				for (int32 i = 0; i < Map.Num() && i < Arr.Num(); ++i) if (Map[i] != INDEX_NONE) Kept[Map[i]] = Arr[i];
				Arr = MoveTemp(Kept);
			};
			Pack(Verts); Pack(Normals); Pack(UVs); Pack(Colours);
			Pack(Out.Colours); Pack(Out.UV1); Pack(Out.UV2); Pack(Out.UV3);
			Pack(Out.RockColours); Pack(Out.RockUV1); Pack(Out.RockUV2); Pack(Out.LipTangentX);
			for (int32& I : Tris) I = Map[I];
			for (int32& I : Lip) I = Map[I];
		}
	}
	Out.Mesh.Verts = MoveTemp(Verts);
	Out.Mesh.Tris = MoveTemp(Tris);
	Out.Lip = MoveTemp(Lip);
	Out.Mesh.Normals = MoveTemp(Normals);
	Out.Mesh.UVs = MoveTemp(UVs);
	Out.Mesh.Colours = MoveTemp(Colours);
}

void AWRLandCaves::BuildFormations()
{
	using namespace WRLookKit;
	using namespace WRLandCavesImpl;
	const bool bDraw = GetNetMode() != NM_DedicatedServer;
	TArray<FWRLookBlock> Blocks;
	FKit K(GetActorTransform(), Blocks);
	int32 Count = 0, Solid = 0;
	// A pillar you can't walk through (stalagmites, columns, frozen falls): a capsule, every machine.
	const auto Block = [this, &Solid](const FVector& Base, float RadiusCm, float HeightCm)
	{
		if (RadiusCm < 20.f || HeightCm < 60.f) return;
		UCapsuleComponent* Cap = NewObject<UCapsuleComponent>(this);
		Cap->SetupAttachment(RootComponent);
		Cap->InitCapsuleSize(RadiusCm, FMath::Max(RadiusCm, HeightCm * 0.5f));
		Cap->SetRelativeLocation(Base + FVector(0.f, 0.f, HeightCm * 0.5f));
		Cap->SetCollisionProfileName(TEXT("BlockAll"));
		Cap->SetCanEverAffectNavigation(false);
		Cap->RegisterComponent();
		++Solid;
	};
	// A rock of the set, its bottom at Base (sunk a fifth), Size (cm) along its own axes, with collision.
	static const TCHAR* RockNames[] = { TEXT("01A"), TEXT("02A"), TEXT("05A"), TEXT("08A"), TEXT("09A"), TEXT("12A") };
	const auto SetRock = [this](int32 Which, const FVector& Size, const FVector& Base, float Yaw, UMaterialInterface* Mat)
	{
		const TCHAR* N = RockNames[((Which % 6) + 6) % 6];
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/SimpleRocks/3D_Assets/Meshes/Rocks/SM_SimpleRock_%s.SM_SimpleRock_%s"), N, N));
		if (!Mesh) return;
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
		C->SetStaticMesh(Mesh);
		C->SetupAttachment(RootComponent);
		const FBox B = Mesh->GetBoundingBox();
		const FVector S = B.GetSize(), Scale(Size.X / FMath::Max(1.f, float(S.X)), Size.Y / FMath::Max(1.f, float(S.Y)), Size.Z / FMath::Max(1.f, float(S.Z)));
		const FRotator Rot(0.f, Yaw, 0.f);
		C->SetRelativeScale3D(Scale);
		C->SetRelativeRotation(Rot);
		C->SetRelativeLocation(Base + Rot.RotateVector(FVector(-B.GetCenter().X * Scale.X, -B.GetCenter().Y * Scale.Y, -B.Min.Z * Scale.Z - Size.Z * 0.2f)));
		if (Mat) C->SetMaterial(0, Mat);
		C->SetCollisionProfileName(TEXT("BlockAll"));
		C->SetCanEverAffectNavigation(false);
		C->RegisterComponent();
	};
	FLcGrown Grown[2];   // [0] the basalt's (M_CaveRock), [1] the ice's (M_CaveIce)
	// Gravel: one instanced mesh per rock of the set and biome, no collision (small enough to walk over).
	TMap<int32, TObjectPtr<UInstancedStaticMeshComponent>> PebbleSets;
	const auto Pebble = [this, &PebbleSets](int32 Which, int32 BiomeKey, const FVector& Size, const FVector& Base, float Yaw, UMaterialInterface* Mat)
	{
		const int32 V = ((Which % 6) + 6) % 6;
		TObjectPtr<UInstancedStaticMeshComponent>& I = PebbleSets.FindOrAdd(BiomeKey * 8 + V);
		if (!I)
		{
			const TCHAR* N = RockNames[V];
			UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/SimpleRocks/3D_Assets/Meshes/Rocks/SM_SimpleRock_%s.SM_SimpleRock_%s"), N, N));
			if (!Mesh) return;
			I = NewObject<UInstancedStaticMeshComponent>(this);
			I->SetStaticMesh(Mesh);
			I->SetupAttachment(RootComponent);
			I->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			I->SetCanEverAffectNavigation(false);
			if (Mat) I->SetMaterial(0, Mat);
			I->SetCullDistances(0, 6000);
			I->RegisterComponent();
		}
		const FBox B = I->GetStaticMesh()->GetBoundingBox();
		const FVector S = B.GetSize(), Scale(Size.X / FMath::Max(1.f, float(S.X)), Size.Y / FMath::Max(1.f, float(S.Y)), Size.Z / FMath::Max(1.f, float(S.Z)));
		const FRotator Rot(0.f, Yaw, 0.f);
		I->AddInstance(FTransform(Rot, Base + Rot.RotateVector(FVector(-B.GetCenter().X * Scale.X, -B.GetCenter().Y * Scale.Y, -B.Min.Z * Scale.Z - Size.Z * 0.3f)), Scale));
	};
	int32 CaveIndex = -1;
	for (const FWRLandCave& Cave : Land->Caves)
	{
		++CaveIndex;
		// The cave's floor under a point (actor-local cm): from a little over it down to the rock, the walls' noise and
		// all (set on the bare number, pebbles hung over the hollows). Points not in the cave's air (on the ground over it)
		// stay where they are.
		const TArray<int32> ThisCave = { CaveIndex };
		const FVector Here = GetActorLocation();
		const auto FloorAt = [&](const FVector& Local)
		{
			const FVector Wp = Local + Here;
			if (LcAir(Land, ThisCave, Wp + FVector(0.f, 0.f, 30.f)) >= 0.f) return Local;
			for (float Z = Wp.Z + 30.f; Z > Wp.Z - 120.f; Z -= 3.f)
			{
				if (LcAir(Land, ThisCave, FVector(Wp.X, Wp.Y, Z)) >= 0.f) return FVector(Local.X, Local.Y, Z - Here.Z + 2.f);
			}
			return Local;
		};
		const bool bIce = Cave.Biome == TEXT("tundra");
		UMaterialInterface* RockMat = FormationMaterial(Cave.Biome);
		FLcGrown& G = Grown[bIce ? 1 : 0];
		// Their surface: the basalt's glazed dark (no flow lines: UV1.x 0); the ice clear (UV1.x 1, not thin, not dark).
		const FColor Col = bIce ? FWRToolMesh::Pack(FLinearColor(0.75f, 0.82f, 0.9f), 0.2f) : FWRToolMesh::Pack(LcLin(38, 36, 38), 0.45f);
		const FVector2D U1 = bIce ? FVector2D(1.f, 0.f) : FVector2D(0.f, 0.f), U2 = bIce ? FVector2D(0.f, 1.f) : FVector2D(0.f, 0.f);
		const FSurf Ice = FSurf::Glass(FLinearColor(0.62f, 0.8f, 0.95f), 0.06f), DeepIce = FSurf::Glass(FLinearColor(0.42f, 0.65f, 0.9f), 0.1f);
		const FSurf Sulfur = FSurf::Stone(FLinearColor(0.5f, 0.44f, 0.16f), 0.6f);
		const FSurf Bone = FSurf::Stone(FLinearColor(0.72f, 0.66f, 0.55f), 0.6f), Pool = FSurf::Glass(FLinearColor(0.5f, 0.72f, 0.88f), 0.04f);
		int32 Seed = 0;
		for (const FWRLandCaveFeat& F : Cave.Feats)
		{
			++Seed;
			const FName Kd = F.Kind;
			const bool bOnFloor = Kd == TEXT("rock") || Kd == TEXT("pebble") || Kd == TEXT("puddle") || Kd == TEXT("mite") || Kd == TEXT("icemite")
				|| Kd == TEXT("column") || Kd == TEXT("icecolumn") || Kd == TEXT("crack") || Kd == TEXT("sulfur") || Kd == TEXT("bench") || Kd == TEXT("fern");
			const FVector At0 = Land->ToWorld(FVector2D(F.At), F.At.Z) - GetActorLocation();
			const FVector At = bOnFloor ? FloorAt(At0) : At0;
			const float A = F.A * 100.f, B = F.B * 100.f;
			const uint32 S0 = uint32(Seed * 7919 + Cave.Id.GetComparisonIndex().ToUnstableInt());
			if (Kd == TEXT("drip") || Kd == TEXT("icicle"))
			{
				// A cluster hanging from the ceiling (roots sunk into the rock): one long, a few shorter round it. The lava
				// tubes' are lavacicles: short, knobbly, dark and glazed; the ice's long, smooth, ringed where they dripped.
				if (bDraw)
				{
					const int32 Many = 2 + int32(LcHash(S0, 1, 1) * 4.f);
					for (int32 n = 0; n < Many; ++n)
					{
						const float Sc = n == 0 ? 1.f : 0.3f + 0.5f * LcHash(S0, n, 2);
						const float Off = n == 0 ? 0.f : B * (1.5f + 2.5f * LcHash(S0, n, 3));
						const FVector P = At + FRotator(0.f, 360.f * LcHash(S0, n, 4), 0.f).Vector() * Off;
						const float Root = 90.f, Len = bIce ? Root + (A - Root) * Sc : Root + (A - Root) * Sc * 0.55f;
						const float R0 = (bIce ? B * 1.3f : B * 1.5f) * (0.6f + 0.4f * Sc);
						LcLathe(G, P, -FVector::UpVector, Len, [R0, Root, Len, bIce](float T)
						{
							const float D = T * Len;
							if (D < Root) return R0 * 1.1f;                                 // in the rock
							const float U = (D - Root) / FMath::Max(1.f, Len - Root);      // 0 at the ceiling .. 1 the tip
							const float Taper = FMath::Pow(FMath::Max(0.f, 1.f - U), bIce ? 0.8f : 0.65f);
							const float Rings2 = 1.f + (bIce ? 0.06f : 0.14f) * FMath::Sin(U * (bIce ? 22.f : 13.f));
							return FMath::Max(U > 0.97f ? 0.f : 1.5f, R0 * (0.12f + 0.88f * Taper) * Rings2);
						}, 10, bIce ? 9 : 7, S0 + n * 31, Len * 0.06f, bIce ? 0.06f : 0.2f, Col, U1, U2);
					}
				}
			}
			else if (Kd == TEXT("mite") || Kd == TEXT("icemite"))
			{
				// A stalagmite (in the lava tubes a dribble spire): a broad foot, lumpy rings up it, a rounded top.
				if (bDraw)
				{
					const float H = A, R0 = B * 1.15f;
					LcLathe(G, At - FVector(0.f, 0.f, 25.f), FVector::UpVector, H + 25.f, [R0](float T)
					{
						const float Foot = 1.f + 1.1f * FMath::Pow(1.f - FMath::Min(T * 3.f, 1.f), 2.f);
						const float Top = T > 0.82f ? FMath::Sqrt(FMath::Max(0.f, 1.f - FMath::Square((T - 0.82f) / 0.18f))) : 1.f;
						return R0 * (1.f - 0.55f * T) * Foot * Top * (1.f + 0.1f * FMath::Sin(T * 17.f));
					}, 12, 10, S0, H * 0.05f, bIce ? 0.08f : 0.22f, Col, U1, U2);
				}
				Block(At, B * 0.8f, A);
			}
			else if (Kd == TEXT("column") || Kd == TEXT("icecolumn"))
			{
				// A column: a stalagmite and a stalactite grown together, flared at both ends, waisted where they met.
				if (bDraw)
				{
					const float H = A, R0 = B;
					LcLathe(G, At - FVector(0.f, 0.f, 30.f), FVector::UpVector, H + 60.f, [R0](float T)
					{
						const float Flare = 1.f + 1.3f * FMath::Pow(1.f - T, 5.f) + 0.9f * FMath::Pow(T, 4.f);
						const float Waist = 1.f - 0.3f * FMath::Exp(-FMath::Square((T - 0.55f) / 0.12f));
						return R0 * Flare * Waist * (1.f + 0.08f * FMath::Sin(T * 23.f));
					}, 16, 12, S0, 0.f, bIce ? 0.07f : 0.16f, Col, U1, U2);
				}
				Block(At, B, A);
			}
			else if (Kd == TEXT("rock") || Kd == TEXT("rimrock"))
			{
				SetRock(Seed, FVector(A, A * 0.85f, B), At, F.Yaw, RockMat);
			}
			else if (Kd == TEXT("pebble"))
			{
				if (bDraw) Pebble(Seed, bIce ? 1 : 0, FVector(A, A * 0.85f, B), At, F.Yaw, RockMat);
			}
			// (A puddle is the floor itself, wet: the cave's rock, ComputeCube. Discs laid on it read as metal plates.)
			else if (Kd == TEXT("bench"))
			{
				// A lava bench: the old flow's shelf along the wall, long and flat-topped.
				SetRock(Seed + 3, FVector(A, 130.f, B), At, F.Yaw, RockMat);
			}
			// (A seam still glowing is the floor itself, hot: ComputeCube's heat, M_CaveRock's cracks. A lit strip laid on
			// the floor read as a neon tube.)
			else if (Kd == TEXT("sulfur") && bDraw)
			{
				// A sulfur crust (the floor's, ComputeCube) and on it a scatter of small pale-yellow needles round the vent.
				// (Four fat cones of it read as candy corn.)
				for (int32 k = 0; k < 9; ++k)
				{
					const float Ang = 360.f * LcHash(S0, k, 20), Off = A * 0.5f * FMath::Sqrt(LcHash(S0, k, 21));
					const FVector O = FRotator(0.f, Ang, 0.f).Vector() * Off;
					const float Len = A * (0.1f + 0.15f * LcHash(S0, k, 22));
					K.Spike(Sulfur, At + O - FVector(0.f, 0.f, 2.f), FRotator(55.f + 30.f * LcHash(S0, k, 23), 360.f * LcHash(S0, k, 24), 0.f).Vector(), Len * 0.22f, Len);
				}
			}
			else if (Kd == TEXT("fern") && bDraw)
			{
				// Ferns where a skylight's light falls on its debris cone (the forest's, Megascans: none if the pack isn't in).
				static const TCHAR* Ferns[] = { TEXT("/Game/Megascans/3D_Plants/Fern/SM_Fern_01.SM_Fern_01"), TEXT("/Game/Megascans/3D_Plants/Fern/SM_Fern_03.SM_Fern_03"),
					TEXT("/Game/Megascans/3D_Plants/Fern/SM_Fern_05.SM_Fern_05") };
				if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, Ferns[Seed % 3]))
				{
					UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
					C->SetStaticMesh(Mesh);
					C->SetupAttachment(RootComponent);
					C->SetRelativeLocation(At - FVector(0.f, 0.f, 4.f));
					C->SetRelativeRotation(FRotator(0.f, F.Yaw, 0.f));
					C->SetRelativeScale3D(FVector(F.A));
					C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					C->SetCanEverAffectNavigation(false);
					C->RegisterComponent();
				}
			}
			else if (Kd == TEXT("icefall"))
			{
				// A frozen fall: ice poured down the wall and set, a few fused ropes of it, swelling where it pooled at the
				// foot and in bulges on the way down, the ropes' ends hanging free.
				if (bDraw)
				{
					const float H = A;
					for (int32 n = 0; n < 3; ++n)
					{
						const FVector P = At + FRotator(0.f, 360.f * LcHash(S0, n, 9), 0.f).Vector() * (B * 0.6f * n);
						const float R0 = B * (0.55f + 0.35f * LcHash(S0, n, 10)), Ph = LcHash(S0, n, 11) * 6.28f;
						LcLathe(G, P - FVector(0.f, 0.f, 20.f), FVector::UpVector, H + 20.f, [R0, Ph](float T)
						{
							const float Foot = 1.f + 0.9f * FMath::Pow(1.f - FMath::Min(T * 4.f, 1.f), 2.f);
							return R0 * Foot * (0.75f + 0.25f * FMath::Sin(T * 9.f + Ph)) * (1.f + 0.12f * FMath::Sin(T * 31.f + Ph));
						}, 18, 10, S0 + n * 13, H * 0.04f, 0.1f, Col, U1, U2);
					}
				}
				Block(At, B * 0.9f, A);
			}
			else if (Kd == TEXT("pool") && bDraw)
			{
				K.Add(Pool, FVector(A * 2.f, A * 1.7f, 5.f), At, FRotator(0.f, float(Seed * 29 % 360), 0.f), EWRLookMesh::Cylinder);
			}
			else if (Kd == TEXT("trapped") && bDraw)
			{
				// A wall of clear ice with something old in it: a great tusk and ribs half out of the ice.
				const FRotator R(0.f, F.Yaw, 0.f);
				const FVector Fwd = R.Vector(), Side = FVector::CrossProduct(FVector::UpVector, Fwd);
				K.Add(DeepIce, FVector(90.f, 420.f, 300.f), At + FVector(0.f, 0.f, 150.f), R);
				K.Rod(Bone, At + Fwd * 20.f + Side * -120.f + FVector(0.f, 0.f, 60.f), At + Fwd * 110.f + Side * 40.f + FVector(0.f, 0.f, 150.f), 22.f);
				K.Rod(Bone, At + Fwd * 110.f + Side * 40.f + FVector(0.f, 0.f, 150.f), At + Fwd * 150.f + Side * 120.f + FVector(0.f, 0.f, 120.f), 16.f);
				for (int32 r = 0; r < 4; ++r) K.Rod(Bone, At + Fwd * 10.f + Side * (60.f + 30.f * r) + FVector(0.f, 0.f, 40.f), At + Fwd * 70.f + Side * (60.f + 30.f * r) + FVector(0.f, 0.f, 120.f + 15.f * r), 8.f);
			}
			++Count;
		}
		if (bDraw)
		{
			for (const FWRLandCaveLight& Lt : Cave.Lights)
			{
				UPointLightComponent* P = NewObject<UPointLightComponent>(this);
				P->SetupAttachment(RootComponent);
				P->SetRelativeLocation(Land->ToWorld(FVector2D(Lt.At), Lt.At.Z) - GetActorLocation());
				P->SetLightColor(Lt.Color);
				P->SetIntensity(Lt.Intensity);
				P->SetAttenuationRadius(Lt.Radius * 100.f);
				P->SetCastShadows(false);
				// (No glow in a skylit chamber's dust: lit by these, the dust stood round each as a bright ball of fog. The
				// embers' a faint haze.)
				P->SetVolumetricScatteringIntensity(Lt.bDay ? 0.f : WRTune(TEXT("landCaves.lightHaze"), 0.12f));
				P->RegisterComponent();
				if (Lt.bDay) { DayLights.Add(P); DayLightFull.Add(Lt.Intensity); }
			}
		}
	}
	if (bDraw)
	{
		FWRBuildLook::Spawn(this, RootComponent, Blocks, FLinearColor(0.3f, 0.25f, 0.2f), false);
		UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this);
		Mesh->SetupAttachment(RootComponent);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);   // (the pillars' capsules stop you)
		Mesh->SetCastShadow(true);
		int32 Section = 0;
		for (int32 g = 0; g < 2; ++g)
		{
			if (Grown[g].T.Num() == 0) continue;
			Mesh->CreateMeshSection(Section, Grown[g].V, Grown[g].T, Grown[g].N, Grown[g].UV0, Grown[g].UV1, Grown[g].UV2, Grown[g].UV0, Grown[g].C, TArray<FProcMeshTangent>(), false);
			Mesh->SetMaterial(Section++, WallMaterial(g ? FName(TEXT("tundra")) : FName(TEXT("volcanic"))));
		}
		Mesh->SetCullDistance(WRTune(TEXT("landCaves.cullDistance"), 60000.f));
		Mesh->RegisterComponent();
		UE_LOG(LogWildreach, Log, TEXT("LAND CAVES grown formations: %d + %d triangles"), Grown[0].T.Num() / 3, Grown[1].T.Num() / 3);
	}
	UE_LOG(LogWildreach, Log, TEXT("LAND CAVES formations: %d (%d look blocks, %d pillars)"), Count, Blocks.Num(), Solid);
}

UMaterialInterface* AWRLandCaves::WallMaterial(FName Biome)
{
	if (const TObjectPtr<UMaterialInterface>* Found = Walls.Find(Biome)) return *Found;
	const bool bIce = Biome == TEXT("tundra");
	UMaterialInterface* Out = nullptr;
	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, bIce ? TEXT("/Game/Terrain/M_CaveIce.M_CaveIce") : TEXT("/Game/Terrain/M_CaveRock.M_CaveRock")))
	{
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, this);
		if (bIce) IceLook = M;
		Out = M;
	}
	else
	{
		UE_LOG(LogWildreach, Error, TEXT("LAND CAVES: /Game/Terrain/M_CaveRock or M_CaveIce is missing (Tools/terrain/build_terrain_materials.py, WR_MATERIALS=caverock,caveice)"));
		Out = FormationMaterial(Biome);
	}
	Walls.Add(Biome, Out);
	return Out;
}

UMaterialInterface* AWRLandCaves::FormationMaterial(FName Biome)
{
	if (const TObjectPtr<UMaterialInterface>* Found = FormationMats.Find(Biome)) return *Found;
	UMaterialInterface* Out = FWRToolMesh::Material();
	if (Biome == TEXT("tundra"))
	{
		// Ice: the seracs' and the Blue Below's blue-white.
		if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
		{
			UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, this);
			M->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.5f, 0.68f, 0.84f));
			Out = M;
		}
	}
	else if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_Rock.M_Rock")))
	{
		// The world-aligned rock, dark basalt (the Volcanic's site rock).
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, this);
		M->SetVectorParameterValue(TEXT("Tint"), FLinearColor(0.3f, 0.27f, 0.25f));
		Out = M;
	}
	FormationMats.Add(Biome, Out);
	return Out;
}

void AWRLandCaves::FinishCube(const FCube& C)
{
	if (C.Mesh.bSkipped || (C.Mesh.Tris.Num() == 0 && C.Lip.Num() == 0)) return;
	UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this);
	Mesh->SetupAttachment(RootComponent);
	Mesh->bUseAsyncCooking = true;
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	Mesh->SetCollisionObjectType(ECC_WorldStatic);
	Mesh->SetCanEverAffectNavigation(false);
	int32 Section = 0;
	if (C.Mesh.Tris.Num() > 0)
	{
		// The cave's own rock: the Volcanic's basalt, the Tundra's ice (their surface in the vertex colour and UV1-2).
		Mesh->CreateMeshSection(Section, C.Mesh.Verts, C.Mesh.Tris, C.Mesh.Normals, C.Mesh.UVs, C.RockUV1, C.RockUV2, C.Mesh.UVs, C.RockColours, TArray<FProcMeshTangent>(), true);
		Mesh->SetMaterial(Section++, WallMaterial(C.Biome));
	}
	if (C.Lip.Num() > 0)
	{
		// The lip as the land draws its ground: its material and vertex data (WRLand BuildChunk).
		if (!GroundLook)
		{
			for (TActorIterator<AWRLand> It(GetWorld()); It; ++It) if (It->LandId == Land->Id) { GroundLook = It->GroundLook(); break; }
		}
		TArray<FProcMeshTangent> Tangents;
		Tangents.Reserve(C.LipTangentX.Num());
		for (const FVector& T : C.LipTangentX) Tangents.Add(FProcMeshTangent(T, false));
		if (Land->bLayered) Mesh->CreateMeshSection_LinearColor(Section, C.Mesh.Verts, C.Lip, C.Mesh.Normals, C.Mesh.UVs, C.UV1, C.UV2, C.UV3, C.Colours, Tangents, true);
		else Mesh->CreateMeshSection_LinearColor(Section, C.Mesh.Verts, C.Lip, C.Mesh.Normals, C.Mesh.UVs, C.Colours, Tangents, true);
		Mesh->SetMaterial(Section, GroundLook ? GroundLook.Get() : FWRToolMesh::Material());
	}
	Mesh->SetCullDistance(WRTune(TEXT("landCaves.cullDistance"), 60000.f));
	Mesh->RegisterComponent();
	Rock.Add(Mesh);
	++Meshes;
}
