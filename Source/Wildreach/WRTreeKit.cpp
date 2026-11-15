// The Ancient forest's trees, generated in code (see WRTreeKit.h; cooks/build-biome-forest.md 1.2, 1.4, 2.5-2.6,
// cooks/biome-forest.md 2.1-2.3). Each species is a grammar run once per variant into a skeleton (bole and its flare,
// buttress fins and surface roots, forks, primary limbs on the golden angle, secondaries, a crown envelope filled with
// leaf sprays, glow strips, the special parts of the landmarks), then meshed three times at falling detail into the
// LODs of one runtime UStaticMesh. Real-world proportions:
//   oak     short thick bole forking at a quarter of its height into 3 leaders, wide sinuous crown (pedunculate oak)
//   beech   smooth silver bole to 38%, a dense dome, tips drooping (Fagus sylvatica)
//   lime    tall narrow dome, fissured bark, the burr sprouts round its foot (Tilia cordata)
//   alder   thin, conical, dark leaves, by water; two variants lean over the bank (Alnus glutinosa)
//   ash     pale diamond-furrowed bark, open oval crown, upswept limbs, pinnate leaves (Fraxinus excelsior)
//   hazel   coppice stool of 7-9 rods, round leaves; holly a dark glossy cone with berries; sapling a young beech/oak
//   giant   coast-redwood scale (Hyperion 116 m, trunks 4-8 m): bole to 58%, flare x1.8 over the lowest 6 m with 5-7
//           buttress fins running out into surface roots, crown pads 56-100%, mossed to the crown (Hoh, Bialowieza)
//   heart   140 m, trunk r 4.5 m, 7 buttresses, amber sap veins (#D08A1E) up the bark cracks
//   eldest  130 m, its bole snapped at 110 m with three reiterated tops still green beside the break, a spiral shelf
//           of bracket fungus at 6-9 m, foxfire (#7FE07A) in the cracks at its base and the Door crack at +X
//   strangler  a strangler-fig tower whose host rotted away: a lattice of fused roots round a hollow you walk into,
//           a spiral root ramp inside to a vista deck at 52 m, the fig's crown round it (Ficus watkinsiana)
//   fallen / nurse_log  a fallen giant: the upturned root plate, the trunk with moss along its back, the snapped end
//           showing amber heartwood; the nurse log carries a row of saplings with roots straddling it (Hoh valley)
// Vertex colour is the surface (RGB, linear here, sRGB-encoded by the static-mesh build) with roughness in A; UV U runs
// along a trunk / root / log in 1.5 m units so M_TreeKit's grain follows the wood.
#include "WRTreeKit.h"
#include "Wildreach.h"
#include "WRToolMesh.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Actor.h"
#include "Misc/App.h"
#include "Misc/CoreMisc.h"
#include "Misc/Crc.h"
#include "UObject/Package.h"

namespace WRTreeKitImpl
{
	enum : int32 { SBark = 0, SLeaf = 1, SGlow = 2 };

	static FLinearColor Hex(uint32 RGB)
	{
		return FLinearColor(FColor(uint8((RGB >> 16) & 255u), uint8((RGB >> 8) & 255u), uint8(RGB & 255u)));
	}

	/** Deterministic 0..1 from three integers (the same on every machine). */
	static float Hash3(int32 X, int32 Y, int32 Z)
	{
		uint32 H = (uint32(X) * 73856093u) ^ (uint32(Y) * 19349663u) ^ (uint32(Z) * 83492791u);
		H ^= H >> 13;
		H *= 0x5bd1e995u;
		H ^= H >> 15;
		return float(H & 0xFFFFFFu) / 16777215.f;
	}

	/** Smooth value noise 0..1 (trilinear over the integer lattice). */
	static float Noise(const FVector& P)
	{
		const int32 X = FMath::FloorToInt32(P.X), Y = FMath::FloorToInt32(P.Y), Z = FMath::FloorToInt32(P.Z);
		const float FX = float(P.X - X), FY = float(P.Y - Y), FZ = float(P.Z - Z);
		const float SX = FX * FX * (3.f - 2.f * FX), SY = FY * FY * (3.f - 2.f * FY), SZ = FZ * FZ * (3.f - 2.f * FZ);
		const float A = FMath::Lerp(Hash3(X, Y, Z), Hash3(X + 1, Y, Z), SX), B = FMath::Lerp(Hash3(X, Y + 1, Z), Hash3(X + 1, Y + 1, Z), SX);
		const float C = FMath::Lerp(Hash3(X, Y, Z + 1), Hash3(X + 1, Y, Z + 1), SX), D = FMath::Lerp(Hash3(X, Y + 1, Z + 1), Hash3(X + 1, Y + 1, Z + 1), SX);
		return FMath::Lerp(FMath::Lerp(A, B, SY), FMath::Lerp(C, D, SY), SZ);
	}

	/** A float-returning wrapper round FRandomStream (its ranges are doubles). */
	struct FRng
	{
		FRandomStream S;
		explicit FRng(int32 Seed) : S(Seed) {}
		float F() { return S.FRand(); }
		float R(float A, float B) { return A + (B - A) * S.FRand(); }
		int32 I(int32 A, int32 B) { return S.RandRange(A, B); }
	};

	// ------------------------------------------------------------------------------------------------ geometry

	struct FSec
	{
		TArray<FVector3f> P, N, T;
		TArray<FVector2f> UV;
		TArray<FVector4f> C;
		TArray<int32> I;
	};

	struct FGeo
	{
		FSec S[3];
		int32 Tris() const { return (S[0].I.Num() + S[1].I.Num() + S[2].I.Num()) / 3; }
		int32 V(int32 Slot, const FVector& P, const FVector& N, const FVector& T, float U, float W, const FLinearColor& C, float Rough)
		{
			FSec& X = S[Slot];
			FVector Nn = N.GetSafeNormal();
			if (Nn.IsNearlyZero()) Nn = FVector::UpVector;
			FVector Tn = (T - Nn * FVector::DotProduct(T, Nn)).GetSafeNormal();
			if (Tn.IsNearlyZero()) Tn = FVector::CrossProduct(Nn, FMath::Abs(Nn.Z) > 0.9f ? FVector::XAxisVector : FVector::ZAxisVector).GetSafeNormal();
			X.P.Add(FVector3f(P));
			X.N.Add(FVector3f(Nn));
			X.T.Add(FVector3f(Tn));
			X.UV.Add(FVector2f(U, W));
			X.C.Add(FVector4f(C.R, C.G, C.B, FMath::Clamp(Rough, 0.f, 1.f)));
			return X.P.Num() - 1;
		}
		/** A triangle shown on the side Facing points to. Unreal shows the side (B - A) x (C - A) points away from
		 *  (FWRToolMesh::Tri), so the order is chosen against Facing. */
		void Tri(int32 Slot, int32 A, int32 B, int32 C, const FVector& Facing)
		{
			FSec& X = S[Slot];
			const FVector3f N = FVector3f::CrossProduct(X.P[B] - X.P[A], X.P[C] - X.P[A]);
			if (N.IsNearlyZero(1e-6f)) return;   // degenerate (a closed crack, a collapsed tip)
			if (FVector3f::DotProduct(N, FVector3f(Facing)) > 0.f) X.I.Append({ A, C, B });
			else X.I.Append({ A, B, C });
		}
		void Quad(int32 Slot, int32 A, int32 B, int32 C, int32 D, const FVector& Facing)
		{
			Tri(Slot, A, B, C, Facing);
			Tri(Slot, A, C, D, Facing);
		}
	};

	/** Two axes across a direction; U starts toward +X (so a standing trunk's angle 0 faces +X: the Eldest's Door). */
	static void Frame(const FVector& T, FVector& U, FVector& V)
	{
		const FVector Ref = FMath::Abs(T.X) > 0.95f ? FVector::YAxisVector : FVector::XAxisVector;
		U = (Ref - T * FVector::DotProduct(Ref, T)).GetSafeNormal();
		V = FVector::CrossProduct(T, U).GetSafeNormal();
	}

	// ------------------------------------------------------------------------------------------------ skeleton

	struct FBark
	{
		FLinearColor Base = FLinearColor(0.1f, 0.08f, 0.06f), Dark = FLinearColor(0.04f, 0.03f, 0.02f), Moss = FLinearColor(0.07f, 0.13f, 0.03f);
		float Rough = 0.9f;
		float MossUp = 0.6f;     // moss on faces turned up
		float MossBand = 0.f;    // cm: moss climbs the trunk this high from the ground
		float MossPatch = 0.f;   // epiphyte patches anywhere
		float Ridges = 9.f;      // furrows round the trunk
		float Furrow = 0.35f;    // how dark the furrows run
	};

	static FLinearColor BarkAt(const FBark& B, const FVector& P, const FVector& N, float Ang, float Along, float& OutRough)
	{
		const float Wob = Noise(P * 0.011) * 3.f;
		const float F = 0.5f + 0.5f * FMath::Sin(Ang * B.Ridges + Along * 0.0035f + Wob);
		FLinearColor C = FMath::Lerp(B.Base, B.Dark, B.Furrow * F * F);
		C *= 0.9f + 0.2f * Noise(P * 0.03 + FVector(17.0));
		const float Up = FMath::SmoothStep(0.2f, 0.85f, float(N.Z)) * B.MossUp;
		const float Band = B.MossBand > 0.f ? FMath::Clamp(1.f - float(P.Z) / B.MossBand, 0.f, 1.f) * 0.85f : 0.f;
		const float Patch = B.MossPatch * FMath::SmoothStep(0.52f, 0.78f, Noise(P * 0.0045 + FVector(5.0)));
		const float M = FMath::Clamp(FMath::Max3(Up, Band * (0.6f + 0.4f * Noise(P * 0.02)), Patch), 0.f, 1.f);
		OutRough = FMath::Lerp(B.Rough, 0.96f, M);
		return FMath::Lerp(C, B.Moss * (0.8f + 0.4f * Noise(P * 0.02 + FVector(9.0))), M);
	}

	/** A standing bole: centreline from below the ground up, nominal radius, the flare and its fins, a crack at +X. */
	struct FTrunk
	{
		TArray<FVector> Pts;
		TArray<float> Rad;
		float Sides = 12.f;
		float FlareMult = 1.f, FlareH = 0.f;
		TArray<float> Fins;          // buttress azimuths (radians)
		float CrackHalf = 0.f, CrackTop = 0.f, CrackCore = 0.f;
		float Jag = 0.f;             // broken top: the rim's splinter height (cm); 0 = a tip
		int32 Bark = 0;
	};

	/** A limb, root, rod, cane or log. Cap: 0 none, 1 tip, 2 broken (splinters), 3 heartwood end (amber), 4 sawn. */
	struct FLimb
	{
		TArray<FVector> Pts;
		TArray<float> Rad;
		TArray<float> RadV;          // optional: the radius across V (a buttress root's tall fin)
		float Sides = 6.f;
		int32 Level = 1;             // 0 never dropped, 1 primary, 2 secondary (dropped at low detail), 3 root (dropped lowest)
		int32 Bark = 0;
		int32 Cap = 1;
		int32 CapStart = 0;          // the same at the first point (0 none)
	};

	struct FCluster
	{
		FVector C = FVector::ZeroVector, Out = FVector::UpVector, Up = FVector::UpVector;
		float Size = 100.f, Tone = 0.5f, Spin = 0.f;
	};

	struct FStrip
	{
		TArray<FVector> Pts, Nrm;
		float Width = 10.f;
		FLinearColor Col;
	};

	struct FDetail
	{
		int32 Lod = 0;
		float Q = 1.f;
	};

	enum ELeaf : int32 { LeafOak, LeafBeech, LeafLime, LeafAlder, LeafAsh, LeafHolly, LeafHazel, LeafGiant, LeafFig, LeafTrefoil };

	struct FSkel
	{
		TArray<FBark> Barks;
		TArray<FTrunk> Trunks;
		TArray<FLimb> Limbs;
		TArray<FCluster> Clusters;   // in priority order: lower LODs draw a prefix, bigger
		int32 LeafShape = LeafOak;
		FLinearColor LeafLo = FLinearColor(0.03f, 0.07f, 0.02f), LeafHi = FLinearColor(0.1f, 0.2f, 0.04f);
		float LeafRough = 0.7f;
		TArray<FStrip> Strips;
		TArray<TFunction<void(FGeo&, const FDetail&)>> Extras;
		TArray<FWRTreeCollider> Colliders;
		TMap<FName, FVector> Anchors;
	};

	// ------------------------------------------------------------------------------------------------ meshing

	static void Resample(const TArray<FVector>& P, const TArray<float>& R, const TArray<float>* RV, int32 N, TArray<FVector>& OutP, TArray<float>& OutR, TArray<float>& OutRV)
	{
		const int32 M = P.Num();
		N = FMath::Clamp(N, 2, M);
		OutP.Reset();
		OutR.Reset();
		OutRV.Reset();
		for (int32 k = 0; k < N; ++k)
		{
			const float F = float(k) * float(M - 1) / float(N - 1);
			const int32 I0 = FMath::Min(FMath::FloorToInt32(F), M - 1), I1 = FMath::Min(I0 + 1, M - 1);
			const float T = F - float(I0);
			OutP.Add(FMath::Lerp(P[I0], P[I1], T));
			OutR.Add(FMath::Lerp(R[I0], R[I1], T));
			if (RV && RV->Num() == M) OutRV.Add(FMath::Lerp((*RV)[I0], (*RV)[I1], T));
		}
	}

	/** Rings of a cap closing a tube end: Rim (vertex indices, the seam column excluded) toward Centre. */
	static void CapEnd(FGeo& G, const TArray<int32>& Rim, const FVector& Centre, const FVector& Out, float Radius, int32 Kind, float Seed)
	{
		if (Rim.Num() < 3) return;
		const FSec& X = G.S[SBark];
		TArray<FVector> RimP;
		for (int32 Idx : Rim) RimP.Add(FVector(X.P[Idx]));
		// Kind 2 broken: pale splinters to a rotten heart; 3 heartwood: sapwood ring, amber heartwood, a dark core;
		// 4 sawn: flat growth rings.
		TArray<float> Fr;
		TArray<FLinearColor> Col;
		TArray<float> Sink;
		if (Kind == 3)
		{
			Fr = { 0.82f, 0.55f, 0.28f };
			Col = { Hex(0xB8A07A), Hex(0xD8A050), Hex(0xC07A2A), Hex(0x8A4A18) };
			Sink = { 0.f, 0.06f, 0.12f, 0.2f };
		}
		else if (Kind == 4)
		{
			Fr = { 0.9f, 0.72f, 0.52f, 0.3f };
			Col = { Hex(0x6A5040), Hex(0xC8A878), Hex(0xB08C5C), Hex(0xD0B080), Hex(0xA07A4A) };
			Sink = { 0.f, 0.f, 0.f, 0.f, 0.f };
		}
		else
		{
			Fr = { 0.6f };
			Col = { Hex(0xB5A27E), Hex(0x6E5A40) };
			Sink = { 0.f, 0.25f };
		}
		TArray<int32> Prev = Rim;
		TArray<FVector> PrevP = RimP;
		for (int32 r = 0; r < Fr.Num(); ++r)
		{
			TArray<int32> Ring;
			TArray<FVector> RingP;
			for (int32 i = 0; i < RimP.Num(); ++i)
			{
				const float J = Kind == 4 ? 0.f : (Hash3(i, r, int32(Seed)) - 0.5f) * 0.12f;
				const FVector P = FMath::Lerp(Centre, RimP[i], Fr[r] + J) - Out * (Sink[r + 1] * Radius);
				RingP.Add(P);
				Ring.Add(G.V(SBark, P, Out, RimP[(i + 1) % RimP.Num()] - RimP[i], float(i) / RimP.Num(), Fr[r], Col[r + 1], 0.8f));
			}
			for (int32 i = 0; i < Ring.Num(); ++i)
			{
				const int32 j = (i + 1) % Ring.Num();
				G.Quad(SBark, Prev[i], Prev[j], Ring[j], Ring[i], Out);
			}
			Prev = Ring;
			PrevP = RingP;
		}
		const int32 C = G.V(SBark, Centre - Out * (Sink.Last() * Radius), Out, FVector::XAxisVector, 0.5f, 0.f, Col.Last(), 0.85f);
		for (int32 i = 0; i < Prev.Num(); ++i) G.Tri(SBark, Prev[i], Prev[(i + 1) % Prev.Num()], C, Out);
	}

	static void MeshTube(FGeo& G, const FSkel& K, const FLimb& L, const FDetail& D)
	{
		if ((L.Level == 2 && D.Q < 0.45f) || (L.Level == 3 && D.Q < 0.3f) || L.Pts.Num() < 2) return;
		const FBark& B = K.Barks[FMath::Clamp(L.Bark, 0, K.Barks.Num() - 1)];
		const int32 Sides = FMath::Max(3, FMath::RoundToInt32(L.Sides * D.Q));
		TArray<FVector> P;
		TArray<float> R, RV;
		Resample(L.Pts, L.Rad, &L.RadV, FMath::Max(2, FMath::RoundToInt32((L.Pts.Num() - 1) * FMath::Clamp(D.Q * 1.25f, 0.3f, 1.f)) + 1), P, R, RV);
		const int32 N = P.Num();
		const bool bOval = RV.Num() == N;
		TArray<TArray<int32>> RingIx;
		RingIx.SetNum(N);
		FVector T0 = (P[1] - P[0]).GetSafeNormal(), U, V;
		Frame(T0, U, V);
		float Along = 0.f;
		const float Seed = float(L.Pts[0].X * 0.37 + L.Pts[0].Y * 0.11 + L.Pts[0].Z * 0.07);
		TArray<FVector> Tangents;
		for (int32 i = 0; i < N; ++i)
		{
			const FVector Tg = (P[FMath::Min(N - 1, i + 1)] - P[FMath::Max(0, i - 1)]).GetSafeNormal();
			U = (U - Tg * FVector::DotProduct(U, Tg)).GetSafeNormal();
			V = FVector::CrossProduct(Tg, U).GetSafeNormal();
			Tangents.Add(Tg);
			if (i > 0) Along += FVector::Dist(P[i], P[i - 1]);
			const float Span = FMath::Max(1.f, float(FVector::Dist(P[FMath::Min(N - 1, i + 1)], P[FMath::Max(0, i - 1)])));
			const float Slope = (R[FMath::Min(N - 1, i + 1)] - R[FMath::Max(0, i - 1)]) / Span;
			const float Ru = FMath::Max(0.3f, R[i]), Rv = FMath::Max(0.3f, bOval ? RV[i] : R[i]);
			// A broken end: the last ring's rim splintered along the limb.
			const bool bJagEnd = (i == N - 1 && (L.Cap == 2 || L.Cap == 3)) || (i == 0 && L.CapStart == 2);
			for (int32 s = 0; s <= Sides; ++s)
			{
				const float A = 2.f * PI * float(s % Sides) / float(Sides);
				const float Ca = FMath::Cos(A), Sa = FMath::Sin(A);
				FVector Pos = P[i] + U * (Ca * Ru) + V * (Sa * Rv);
				if (bJagEnd)
				{
					const float J = (s % 2 == 0 ? 0.55f : 0.1f) + 0.35f * Hash3(s % Sides, i, int32(Seed));
					Pos += Tg * (J * Ru * (L.Cap == 3 && i == N - 1 ? 0.45f : 0.8f) * (i == 0 ? -1.f : 1.f));
				}
				const FVector Nrm = ((U * (Ca / Ru) + V * (Sa / Rv)).GetSafeNormal() - Tg * Slope).GetSafeNormal();
				float Rough = B.Rough;
				const FLinearColor C = BarkAt(B, Pos, Nrm, A, Along, Rough);
				RingIx[i].Add(G.V(SBark, Pos, Nrm, Tg, Along / 150.f, float(s) / float(Sides) * 2.f * PI * Ru / 150.f, C, Rough));
			}
		}
		for (int32 i = 0; i + 1 < N; ++i)
		{
			for (int32 s = 0; s < Sides; ++s)
			{
				const FVector Facing = FVector(G.S[SBark].N[RingIx[i][s]]) + FVector(G.S[SBark].N[RingIx[i + 1][s + 1]]);
				G.Quad(SBark, RingIx[i][s], RingIx[i][s + 1], RingIx[i + 1][s + 1], RingIx[i + 1][s], Facing);
			}
		}
		// Ends.
		const auto Rim = [&](int32 i) { TArray<int32> Out = RingIx[i]; Out.Pop(); return Out; };
		if (L.Cap == 1)
		{
			const FVector Tip = P.Last() + Tangents.Last() * (R.Last() * 1.6f);
			float Rough = B.Rough;
			const FLinearColor C = BarkAt(B, Tip, Tangents.Last(), 0.f, Along, Rough);
			const int32 TipI = G.V(SBark, Tip, Tangents.Last(), U, Along / 150.f, 0.f, C, Rough);
			const TArray<int32> Last = Rim(N - 1);
			for (int32 s = 0; s < Last.Num(); ++s) G.Tri(SBark, Last[s], Last[(s + 1) % Last.Num()], TipI, Tangents.Last());
		}
		else if (L.Cap >= 2)
		{
			CapEnd(G, Rim(N - 1), P.Last() + Tangents.Last() * (R.Last() * 0.15f), Tangents.Last(), R.Last(), L.Cap, Seed);
		}
		if (L.CapStart >= 2)
		{
			CapEnd(G, Rim(0), P[0] - Tangents[0] * (R[0] * 0.15f), -Tangents[0], R[0], L.CapStart, Seed + 3.f);
		}
	}

	static float FlareAt(const FTrunk& T, float Z, float A)
	{
		if (T.FlareMult <= 1.001f || T.FlareH <= 0.f || Z >= T.FlareH) return 1.f;
		const float F = FMath::Square(1.f - FMath::Max(0.f, Z) / T.FlareH);
		float Fin = 1.f;
		if (T.Fins.Num() > 0)
		{
			float Best = 0.f;
			for (const float FA : T.Fins)
			{
				const float Dl = FMath::FindDeltaAngleRadians(A, FA);
				Best = FMath::Max(Best, FMath::Exp(-FMath::Square(Dl / 0.2f)));
			}
			Fin = 0.5f + 1.3f * Best;
		}
		return 1.f + (T.FlareMult - 1.f) * F * Fin;
	}

	/** The bole surface's radius at a height and azimuth (for strips laid on it). */
	static float TrunkRadiusAt(const FTrunk& T, float Z, float A)
	{
		float R = T.Rad[0];
		for (int32 i = 0; i + 1 < T.Pts.Num(); ++i)
		{
			if (Z <= T.Pts[i + 1].Z)
			{
				const float Z0 = float(T.Pts[i].Z), Z1 = float(T.Pts[i + 1].Z);
				R = FMath::Lerp(T.Rad[i], T.Rad[i + 1], Z1 > Z0 ? FMath::Clamp((Z - Z0) / (Z1 - Z0), 0.f, 1.f) : 0.f);
				return R * FlareAt(T, Z, A);
			}
		}
		return T.Rad.Last();
	}

	static void MeshTrunk(FGeo& G, const FSkel& K, const FTrunk& T, const FDetail& D)
	{
		const FBark& B = K.Barks[FMath::Clamp(T.Bark, 0, K.Barks.Num() - 1)];
		TArray<FVector> P;
		TArray<float> R, RV;
		Resample(T.Pts, T.Rad, nullptr, FMath::Max(3, FMath::RoundToInt32((T.Pts.Num() - 1) * FMath::Max(D.Q, 0.2f)) + 1), P, R, RV);
		const int32 N = P.Num();
		const int32 Sides = FMath::Max(5, FMath::RoundToInt32(T.Sides * D.Q));
		// Columns round the bole (angle, and whether it is the back of the crack at +X).
		struct FCol { float A; bool bBack; };
		TArray<FCol> Cols, Left, Right;
		const bool bCrack = T.CrackHalf > 0.f;
		for (int32 s = 0; s < Sides; ++s)
		{
			const float A = -PI + 2.f * PI * (float(s) + 0.5f) / float(Sides);
			if (bCrack && FMath::Abs(A) <= T.CrackHalf * 1.3f) continue;
			(A < 0.f ? Left : Right).Add({ A, false });
		}
		Cols = Left;
		if (bCrack)
		{
			// The walls of the crack are radial: the same angle twice, once on the bark, once at the back.
			const int32 Inner = D.Q > 0.7f ? 4 : D.Q > 0.35f ? 2 : 1;
			Cols.Add({ -T.CrackHalf, false });
			for (int32 i = 0; i <= Inner; ++i) Cols.Add({ -T.CrackHalf + 2.f * T.CrackHalf * float(i) / float(Inner), true });
			Cols.Add({ T.CrackHalf, false });
		}
		Cols.Append(Right);
		const int32 NC = Cols.Num();
		Cols.Add({ Cols[0].A + 2.f * PI, Cols[0].bBack });   // the seam (UV)
		// Positions.
		TArray<TArray<FVector>> Pos;
		TArray<TArray<float>> Inset;
		Pos.SetNum(N);
		Inset.SetNum(N);
		FVector U, V;
		Frame((P[1] - P[0]).GetSafeNormal(), U, V);
		TArray<FVector> Tg;
		for (int32 i = 0; i < N; ++i)
		{
			const FVector Ti = (P[FMath::Min(N - 1, i + 1)] - P[FMath::Max(0, i - 1)]).GetSafeNormal();
			U = (U - Ti * FVector::DotProduct(U, Ti)).GetSafeNormal();
			V = FVector::CrossProduct(Ti, U).GetSafeNormal();
			Tg.Add(Ti);
			const float Z = float(P[i].Z);
			for (int32 c = 0; c <= NC; ++c)
			{
				const float A = Cols[c].A;
				float Rad = R[i] * FlareAt(T, Z, A);
				float In = 0.f;
				if (Cols[c].bBack)
				{
					const float Edge = FMath::Abs(FMath::FindDeltaAngleRadians(0.f, A)) / FMath::Max(T.CrackHalf, 0.01f);
					const float Top = T.CrackTop * (1.f - 0.3f * Edge * Edge);
					In = FMath::Clamp((Top - Z) / FMath::Max(60.f, T.CrackTop * 0.12f), 0.f, 1.f);
					Rad = FMath::Lerp(Rad, FMath::Min(Rad, T.CrackCore), In);
				}
				FVector At = P[i] + (U * FMath::Cos(A) + V * FMath::Sin(A)) * Rad;
				if (i == N - 1 && T.Jag > 0.f)
				{
					At += Ti * (T.Jag * ((c % 2 == 0 ? 0.6f : 0.12f) + 0.4f * Hash3(c % NC, 7, Sides)));
				}
				Pos[i].Add(At);
				Inset[i].Add(In);
			}
		}
		// Normals from the grid, colours, vertices.
		TArray<TArray<int32>> Ix;
		Ix.SetNum(N);
		float Along = 0.f;
		for (int32 i = 0; i < N; ++i)
		{
			if (i > 0) Along += FVector::Dist(P[i], P[i - 1]);
			for (int32 c = 0; c <= NC; ++c)
			{
				const int32 cp = c == 0 ? NC - 1 : (c == NC ? NC - 1 : c - 1);
				const int32 cn = c == NC ? 1 : c + 1;
				const FVector DA = Pos[i][cn] - Pos[i][cp];
				const FVector DU = Pos[FMath::Min(N - 1, i + 1)][c] - Pos[FMath::Max(0, i - 1)][c];
				FVector Nrm = FVector::CrossProduct(DA, DU).GetSafeNormal();
				const FVector Radial = (Pos[i][c] - P[i]).GetSafeNormal();
				if (Nrm.IsNearlyZero()) Nrm = Radial;
				if (!Cols[c].bBack && FVector::DotProduct(Nrm, Radial) < 0.f) Nrm = -Nrm;
				float Rough = B.Rough;
				FLinearColor Col = BarkAt(B, Pos[i][c], Nrm, Cols[c].A, Along, Rough);
				Col = FMath::Lerp(Col, Hex(0x1A120C), Inset[i][c] * 0.85f);
				const float Vv = (Cols[c].A + PI) * R[FMath::Min(1, N - 1)] / 150.f;
				Ix[i].Add(G.V(SBark, Pos[i][c], Nrm, Tg[i], Along / 150.f, Vv, Col, Rough));
			}
		}
		for (int32 i = 0; i + 1 < N; ++i)
		{
			for (int32 c = 0; c < NC; ++c)
			{
				FVector Facing = FVector(G.S[SBark].N[Ix[i][c]]) + FVector(G.S[SBark].N[Ix[i + 1][c + 1]]);
				// A crack wall (same angle twice): face it across the crack, toward the side the hollow is on.
				if (FMath::IsNearlyEqual(Cols[c].A, Cols[c + 1].A, 1e-4f))
				{
					const FVector Side = U.RotateAngleAxis(FMath::RadiansToDegrees(Cols[c].A) + 90.f, Tg[i]);
					Facing = Cols[c + 1].bBack ? Side : -Side;
				}
				G.Quad(SBark, Ix[i][c], Ix[i][c + 1], Ix[i + 1][c + 1], Ix[i + 1][c], Facing);
			}
		}
		// The top: a tip, or a broken crater.
		TArray<int32> Rim = Ix[N - 1];
		Rim.Pop();
		if (T.Jag > 0.f)
		{
			CapEnd(G, Rim, P.Last() + Tg.Last() * (T.Jag * 0.1f), Tg.Last(), R.Last(), 2, float(Sides));
		}
		else
		{
			float Rough = B.Rough;
			const FVector Tip = P.Last() + Tg.Last() * (R.Last() * 2.5f);
			const int32 TipI = G.V(SBark, Tip, Tg.Last(), FVector::XAxisVector, Along / 150.f, 0.f, BarkAt(B, Tip, Tg.Last(), 0.f, Along, Rough), Rough);
			for (int32 c = 0; c < Rim.Num(); ++c) G.Tri(SBark, Rim[c], Rim[(c + 1) % Rim.Num()], TipI, Tg.Last());
		}
	}

	static void LeafOutline(int32 Shape, int32 Level, TArray<FVector2D>& Out)
	{
		int32 Lobes = 7;
		float Depth = 0.3f, Aspect = 1.f;
		switch (Shape)
		{
		case LeafOak:     Lobes = 7; Depth = 0.34f; Aspect = 1.f; break;
		case LeafBeech:   Lobes = 8; Depth = 0.15f; Aspect = 0.95f; break;
		case LeafLime:    Lobes = 6; Depth = 0.22f; Aspect = 1.05f; break;
		case LeafAlder:   Lobes = 7; Depth = 0.2f; Aspect = 0.9f; break;
		case LeafAsh:     Lobes = 9; Depth = 0.42f; Aspect = 0.5f; break;
		case LeafHolly:   Lobes = 9; Depth = 0.44f; Aspect = 0.85f; break;
		case LeafHazel:   Lobes = 6; Depth = 0.18f; Aspect = 1.f; break;
		case LeafGiant:   Lobes = 7; Depth = 0.3f; Aspect = 1.25f; break;
		case LeafFig:     Lobes = 6; Depth = 0.26f; Aspect = 1.f; break;
		case LeafTrefoil: Lobes = 3; Depth = 0.58f; Aspect = 1.f; break;
		default: break;
		}
		if (Level == 1) Lobes = FMath::Max(3, (Lobes + 1) / 2 + 1);
		else if (Level >= 2) Lobes = 3;
		const int32 M = Lobes * 2;
		Out.Reset();
		for (int32 k = 0; k < M; ++k)
		{
			const float A = 0.5f * PI + 2.f * PI * float(k) / float(M);
			const bool bPeak = k % 2 == 0;
			const float J = Hash3(Shape, k, Level);
			const float Rr = bPeak ? 0.5f * (0.9f + 0.18f * J) : 0.5f * (1.f - Depth) * (0.88f + 0.24f * J);
			Out.Add(FVector2D(FMath::Cos(A) * Rr * Aspect, FMath::Sin(A) * Rr));
		}
	}

	/** One spray: crossed cards cut to the outline (no alpha), both faces, shaded as a soft volume (normals lean out). */
	static void MeshCluster(FGeo& G, const FCluster& Cl, const TArray<FVector2D>& Outline, float SizeMul, const FLinearColor& Lo, const FLinearColor& Hi, float Rough)
	{
		const float W = Cl.Size * SizeMul, H = Cl.Size * SizeMul * 0.85f;
		const FVector Up = Cl.Up.GetSafeNormal();
		FVector Side0 = FVector::CrossProduct(Up, FMath::Abs(Up.Z) > 0.95f ? FVector::XAxisVector : FVector::ZAxisVector).GetSafeNormal();
		Side0 = Side0.RotateAngleAxis(FMath::RadiansToDegrees(Cl.Spin), Up);
		const FLinearColor Base = FMath::Lerp(Lo, Hi, FMath::Clamp(Cl.Tone, 0.f, 1.f));
		for (int32 Plane = 0; Plane < 2; ++Plane)
		{
			const FVector X = Plane == 0 ? Side0 : FVector::CrossProduct(Up, Side0).GetSafeNormal();
			const FVector Np = FVector::CrossProduct(X, Up).GetSafeNormal();
			for (int32 Face = 0; Face < 2; ++Face)
			{
				const FVector Fn = Face == 0 ? Np : -Np;
				const FVector Shade = (Fn * 0.35f + Cl.Out * 0.65f + FVector::UpVector * 0.15f).GetSafeNormal();
				const int32 C = G.V(SLeaf, Cl.C, Shade, X, 0.5f, 0.5f, Base * 0.78f, Rough);
				TArray<int32> R;
				for (int32 k = 0; k < Outline.Num(); ++k)
				{
					const FVector P = Cl.C + X * (Outline[k].X * W) + Up * (Outline[k].Y * H);
					const float J = 0.92f + 0.16f * Hash3(k, Plane, FMath::FloorToInt32(Cl.C.X * 0.1));
					R.Add(G.V(SLeaf, P, Shade, X, float(Outline[k].X) + 0.5f, float(Outline[k].Y) + 0.5f, Base * J, Rough));
				}
				for (int32 k = 0; k < R.Num(); ++k) G.Tri(SLeaf, C, R[k], R[(k + 1) % R.Num()], Fn);
			}
		}
	}

	static void MeshStrip(FGeo& G, const FStrip& S, const FDetail& D)
	{
		if (S.Pts.Num() < 2) return;
		const int32 Step = D.Q > 0.7f ? 1 : D.Q > 0.35f ? 2 : 4;
		TArray<int32> Keep;
		for (int32 i = 0; i < S.Pts.Num(); i += Step) Keep.Add(i);
		if (Keep.Last() != S.Pts.Num() - 1) Keep.Add(S.Pts.Num() - 1);
		TArray<int32> L, R;
		for (int32 k = 0; k < Keep.Num(); ++k)
		{
			const int32 i = Keep[k];
			const FVector T = (S.Pts[Keep[FMath::Min(k + 1, Keep.Num() - 1)]] - S.Pts[Keep[FMath::Max(k - 1, 0)]]).GetSafeNormal();
			const FVector N = S.Nrm[i];
			const FVector Side = FVector::CrossProduct(N, T).GetSafeNormal();
			const float F = float(k) / float(FMath::Max(1, Keep.Num() - 1));
			const float W = S.Width * (0.35f + 0.65f * FMath::Sin(PI * FMath::Clamp(F * 1.1f, 0.f, 1.f)));
			const FVector C = S.Pts[i] + N * 3.f;
			L.Add(G.V(SGlow, C - Side * (W * 0.5f), N, T, F, 0.f, S.Col, 0.45f));
			R.Add(G.V(SGlow, C + Side * (W * 0.5f), N, T, F, 1.f, S.Col, 0.45f));
		}
		for (int32 k = 0; k + 1 < L.Num(); ++k) G.Quad(SGlow, L[k], R[k], R[k + 1], L[k + 1], S.Nrm[Keep[k]]);
	}

	// ------------------------------------------------------------------------------------------------ species

	struct FTreeP
	{
		float H = 2000.f, R = 40.f;
		float BoleTop = 0.3f, LeaderTop = 0.85f;
		int32 Forks = 0;
		float ForkSpread = 0.45f;
		float CrownBase = 0.3f, CrownTop = 1.f, CrownR = 0.3f;
		int32 Shape = 0;
		int32 Branches = 10;
		float BranchAngle = 55.f, Arch = 0.1f, TipDrop = 0.f, Kink = 0.03f, BranchR = 0.42f;
		int32 Secondaries = 2;
		float Flare = 1.3f, FlareH = 0.05f;
		int32 Fins = 0, Roots = 0;
		float Lean = 0.f;
		/** Lean along +X instead of a random way: the dressing yaws these toward the water (forest_alder_lean). */
		bool bLeanX = false;
		float TrunkSides = 12.f, LimbSides = 6.f, TwigSides = 4.f;
		int32 Rings = 12;
		bool bDead = false;
		float Broken = 0.f, Jag = 0.f;
		float LeafSize = 250.f;
		int32 Fill = 100;
		float PadUp = 0.f;
		int32 Reiterate = 0;     // tops grown again beside a broken bole (the Eldest)
		float CrackHalf = 0.f, CrackTop = 0.f, CrackCore = 0.f;
	};

	static float Env(int32 Shape, float T)
	{
		T = FMath::Clamp(T, 0.f, 1.f);
		switch (Shape)
		{
		case 1: return FMath::Sqrt(FMath::Max(0.f, 1.f - FMath::Square((T - 0.42f) / 0.62f)));                        // broad (oak)
		case 2: return FMath::Sqrt(FMath::Max(0.f, 1.f - FMath::Square((T - 0.35f) / 0.67f)));                        // tall dome (lime)
		case 3: return FMath::Pow(1.f - T, 0.85f) * FMath::Min(1.f, 0.55f + T * 3.f);                                 // cone (alder, holly)
		case 4: return FMath::Sqrt(FMath::Max(0.f, 1.f - FMath::Square((T - 0.5f) / 0.56f)));                         // open oval (ash)
		case 5: return FMath::Sqrt(FMath::Max(0.f, 1.f - FMath::Pow(FMath::Abs((T - 0.4f) / 0.62f), 3.f)));          // flat-topped pads (giants)
		default: return FMath::Sqrt(FMath::Max(0.f, 1.f - FMath::Square((T - 0.38f) / 0.64f)));                       // dome (beech)
		}
	}

	struct FAxis { TArray<FVector> Pts; TArray<float> Rad; float Phi = 0.f; };

	static FVector AxisAt(const FAxis& Ax, float Z, float* OutR = nullptr)
	{
		const int32 N = Ax.Pts.Num();
		if (Z <= Ax.Pts[0].Z) { if (OutR) *OutR = Ax.Rad[0]; return Ax.Pts[0]; }
		for (int32 i = 0; i + 1 < N; ++i)
		{
			const float Z0 = float(Ax.Pts[i].Z), Z1 = float(Ax.Pts[i + 1].Z);
			if (Z <= Z1)
			{
				const float T = Z1 - Z0 > 1e-3f ? (Z - Z0) / (Z1 - Z0) : 0.f;
				if (OutR) *OutR = FMath::Lerp(Ax.Rad[i], Ax.Rad[i + 1], T);
				return FMath::Lerp(Ax.Pts[i], Ax.Pts[i + 1], T);
			}
		}
		if (OutR) *OutR = Ax.Rad.Last();
		return Ax.Pts.Last();
	}

	static FVector PolyAt(const TArray<FVector>& P, const TArray<float>& R, float S, float* OutR = nullptr)
	{
		const float F = FMath::Clamp(S, 0.f, 1.f) * float(P.Num() - 1);
		const int32 I0 = FMath::Min(FMath::FloorToInt32(F), P.Num() - 1), I1 = FMath::Min(I0 + 1, P.Num() - 1);
		if (OutR) *OutR = FMath::Lerp(R[I0], R[I1], F - float(I0));
		return FMath::Lerp(P[I0], P[I1], F - float(I0));
	}

	/** A standing tree: bole, flare, fins and roots, forks, limbs, crown, colliders. Returns the main trunk index. */
	static void Standing(FSkel& K, const FTreeP& P, FRng& Rs, int32 BarkIx)
	{
		const float H = P.H;
		const float TopZ = P.Forks > 0 ? P.BoleTop * H * 1.06f : (P.Broken > 0.f ? P.Broken * H : P.LeaderTop * H);
		const float FlareAbs = P.FlareH * H;
		const float LeanDraw = Rs.R(0.f, 2.f * PI);
		const float LeanA = P.bLeanX ? 0.f : LeanDraw;
		const FVector LeanDir(FMath::Cos(LeanA), FMath::Sin(LeanA), 0.f);
		const float LeanT = FMath::Tan(FMath::DegreesToRadians(P.Lean));
		const float WigA = Rs.R(0.f, 2.f * PI);
		const float Wig = P.R * 0.35f;
		const auto Centre = [&](float Z) -> FVector
		{
			const float Ramp = FMath::Clamp(Z / (H * 0.1f), 0.f, 1.f);
			const float Zc = FMath::Max(0.f, Z);
			return LeanDir * (LeanT * Zc * (0.5f + 0.5f * FMath::Clamp(Zc / FMath::Max(TopZ, 1.f), 0.f, 1.5f)))
				+ FVector(FMath::Cos(WigA + Z / (H * 0.18f)), FMath::Sin(WigA * 1.3f + Z / (H * 0.21f)), 0.f) * (Wig * Ramp)
				+ FVector(0.f, 0.f, Z);
		};
		const float TaperTop = P.Broken > 0.f ? P.Broken * H / 0.75f : TopZ * 1.06f;
		const auto RadAt = [&](float Z) -> float
		{
			if (P.Forks > 0) return P.R * FMath::Lerp(1.06f, 0.8f, FMath::Clamp(Z / TopZ, 0.f, 1.f));
			const float BH = FMath::Min(130.f, TaperTop * 0.2f);
			const float Num = FMath::Max(0.f, 1.f - FMath::Max(0.f, Z) / TaperTop), Den = 1.f - BH / TaperTop;
			return P.R * FMath::Max(0.05f, FMath::Pow(Num / Den, 0.8f));
		};
		// The bole's rings: close together through the flare (the fins need them), then even to the top.
		TArray<float> Zs;
		Zs.Add(-FMath::Max(60.f, P.R * P.Flare * 0.45f));
		Zs.Add(0.f);
		if (FlareAbs > 1.f)
		{
			for (const float F : { 0.06f, 0.16f, 0.3f, 0.48f, 0.7f, 1.f }) Zs.Add(FlareAbs * F);
		}
		if (P.CrackTop > 0.f)
		{
			Zs.Add(P.CrackTop * 0.5f);
			Zs.Add(P.CrackTop * 0.85f);
			Zs.Add(P.CrackTop);
			Zs.Add(P.CrackTop * 1.15f);
		}
		const float From = FMath::Max(FlareAbs, P.CrackTop * 1.15f);
		for (int32 k = 1; k <= P.Rings; ++k) Zs.Add(From + (TopZ - From) * float(k) / float(P.Rings));
		Zs.Sort();
		TArray<float> Zu;
		for (const float Z : Zs) if (Zu.Num() == 0 || Z - Zu.Last() > 8.f) Zu.Add(Z);
		FTrunk& T = K.Trunks.AddDefaulted_GetRef();
		T.Bark = BarkIx;
		T.Sides = P.TrunkSides;
		T.FlareMult = P.Flare;
		T.FlareH = FlareAbs;
		T.Jag = P.Broken > 0.f ? P.Jag : 0.f;
		T.CrackHalf = P.CrackHalf;
		T.CrackTop = P.CrackTop;
		T.CrackCore = P.CrackCore;
		for (const float Z : Zu)
		{
			T.Pts.Add(Centre(Z));
			T.Rad.Add(RadAt(Z));
		}
		if (P.Forks > 0)
		{
			T.Pts.Last() = Centre(TopZ);   // hidden inside the forks' bases
		}
		// Fins and surface roots.
		const int32 NR = P.Fins > 0 ? P.Fins : P.Roots;
		const float RootA = Rs.R(0.f, 2.f * PI);
		for (int32 f = 0; f < NR; ++f)
		{
			const float A = RootA + 2.f * PI * float(f) / float(NR) + Rs.R(-0.25f, 0.25f);
			if (P.Fins > 0) T.Fins.Add(FMath::UnwindRadians(A));
			const FVector Dir(FMath::Cos(A), FMath::Sin(A), 0.f);
			const FVector Side(-Dir.Y, Dir.X, 0.f);
			FLimb Rt;
			Rt.Level = 3;
			Rt.Bark = BarkIx;
			Rt.Sides = FMath::Max(4.f, P.LimbSides * 0.8f);
			const float Reach = P.R * P.Flare * Rs.R(2.4f, 3.3f) + (P.Fins > 0 ? P.R * 1.2f : 0.f);
			const float R0 = P.R * (P.Fins > 0 ? 0.42f : 0.3f);
			const float Wv = Rs.R(-0.25f, 0.25f);
			for (int32 k = 0; k <= 5; ++k)
			{
				const float S = float(k) / 5.f;
				const float D = FMath::Lerp(P.R * P.Flare * 0.75f, Reach, S);
				const float Z = FMath::Lerp(FMath::Max(FlareAbs * 0.18f, P.R * 0.25f), -R0 * 0.5f, FMath::Pow(S, 0.7f));
				Rt.Pts.Add(Centre(0.f) + Dir * D + Side * (Wv * D * S * 0.5f) + FVector(0.f, 0.f, Z));
				Rt.Rad.Add(FMath::Max(1.f, R0 * (1.f - 0.82f * S)));
			}
			K.Limbs.Add(Rt);
			if (P.Fins > 0)
			{
				FWRTreeCollider C;
				C.A = Centre(0.f) + Dir * P.R + FVector(0.f, 0.f, FlareAbs * 0.2f);
				C.B = Centre(0.f) + Dir * (P.R * P.Flare * 1.6f) + FVector(0.f, 0.f, P.R * 0.15f);
				C.Radius = P.R * 0.24f;
				K.Colliders.Add(C);
			}
		}
		// Collision: the bole to the crown (two pieces when it leans) and the flare.
		{
			const float Top = FMath::Clamp(P.CrownBase * H, FMath::Min(TopZ, 400.f), TopZ);
			FWRTreeCollider C;
			C.Radius = P.R * 0.95f;
			if (P.Lean > 4.f)
			{
				C.A = Centre(0.f); C.B = Centre(Top * 0.5f); K.Colliders.Add(C);
				C.A = C.B; C.B = Centre(Top); C.Radius = P.R * 0.85f; K.Colliders.Add(C);
			}
			else
			{
				C.A = Centre(0.f); C.B = Centre(Top); K.Colliders.Add(C);
			}
			if (P.Flare >= 1.25f && FlareAbs > 1.f)
			{
				C.A = Centre(0.f); C.B = Centre(FlareAbs * 0.45f);
				C.Radius = P.R * (1.f + (P.Flare - 1.f) * 0.7f) * 0.95f;
				K.Colliders.Add(C);
			}
		}
		// Axes the limbs grow from: the leader, or the forks of a decurrent crown.
		TArray<FAxis> Axes;
		const float CB = P.CrownBase * H, CT = P.CrownTop * H, RMax = P.CrownR * H;
		const float Phi0 = Rs.R(0.f, 2.f * PI);
		if (P.Forks > 0)
		{
			for (int32 f = 0; f < P.Forks; ++f)
			{
				const float Phi = Phi0 + 2.f * PI * float(f) / float(P.Forks) + Rs.R(-0.3f, 0.3f);
				const FVector Dir(FMath::Cos(Phi), FMath::Sin(Phi), 0.f);
				const FVector Start = Centre(TopZ * 0.96f);
				const FVector End = Centre(CT * Rs.R(0.82f, 0.92f)) + Dir * (RMax * P.ForkSpread * Rs.R(0.85f, 1.1f));
				FAxis Ax;
				Ax.Phi = Phi;
				FLimb Fk;
				Fk.Level = 0;
				Fk.Bark = BarkIx;
				Fk.Sides = FMath::Max(5.f, P.TrunkSides * 0.6f);
				for (int32 k = 0; k <= 5; ++k)
				{
					const float S = float(k) / 5.f;
					FVector Pt = FMath::Lerp(Start, End, S);
					Pt += Dir * (FMath::Sin(PI * S) * RMax * 0.12f) - FVector(0.f, 0.f, FMath::Sin(PI * S) * (End.Z - Start.Z) * 0.08f);
					if (k > 0 && k < 5) Pt += FVector(Rs.R(-1.f, 1.f), Rs.R(-1.f, 1.f), 0.f) * (P.R * 0.4f);
					const float Rr = P.R * 0.74f * (1.f - 0.78f * S) + 2.f;
					Fk.Pts.Add(Pt);
					Fk.Rad.Add(Rr);
				}
				Ax.Pts = Fk.Pts;
				Ax.Rad = Fk.Rad;
				K.Limbs.Add(Fk);
				Axes.Add(Ax);
			}
		}
		else
		{
			FAxis Ax;
			Ax.Pts = T.Pts;
			Ax.Rad = T.Rad;
			Axes.Add(Ax);
		}
		// Limbs and their sprays.
		TArray<FVector> Pool;
		TArray<FVector> Tips, SecondTips;
		const int32 NB = FMath::Max(3, P.Branches + Rs.I(-2, 2));
		for (int32 j = 0; j < NB; ++j)
		{
			const FAxis& Ax = Axes[j % Axes.Num()];
			const float AxLo = float(Ax.Pts[0].Z), AxHi = float(Ax.Pts.Last().Z);
			const float ZLo = FMath::Max(CB, AxLo + (P.Forks > 0 ? (AxHi - AxLo) * 0.2f : 0.f));
			const float ZHi = FMath::Max(ZLo + 1.f, FMath::Min(CT * 0.93f, AxHi * 0.98f));
			const float Ut = FMath::Clamp((float(j) + 0.5f + Rs.R(-0.35f, 0.35f)) / float(NB), 0.f, 1.f);
			const float Za = FMath::Lerp(ZLo, ZHi, FMath::Pow(Ut, 0.85f));
			float Ra = 0.f;
			const FVector A = AxisAt(Ax, Za, &Ra);
			const float Phi = P.Forks > 0 ? Ax.Phi + Rs.R(-1.3f, 1.3f) : Phi0 + float(j) * 2.39996f + Rs.R(0.f, 0.5f);
			const float Alpha = FMath::DegreesToRadians(FMath::Clamp(P.BranchAngle + Rs.R(-10.f, 10.f) + (1.f - Ut) * 8.f, 5.f, 100.f));
			const FVector Dir(FMath::Cos(Phi) * FMath::Sin(Alpha), FMath::Sin(Phi) * FMath::Sin(Alpha), FMath::Cos(Alpha));
			FVector E = A + Dir * (RMax * 0.25f);
			for (int32 s = 1; s <= 30; ++s)
			{
				const FVector Q = A + Dir * (RMax * 1.7f * float(s) / 30.f);
				const float Tq = (float(Q.Z) - CB) / FMath::Max(1.f, CT - CB);
				const float Lim = Env(P.Shape, Tq) * RMax * 0.94f;
				if (Tq > 1.02f || FVector::Dist2D(Q, Centre(float(Q.Z))) > Lim) break;
				E = Q;
			}
			if (FVector::Dist(A, E) < RMax * 0.25f) E = A + Dir * (RMax * 0.25f);
			if (P.bDead) E = A + (E - A) * Rs.R(0.25f, 0.65f);
			const float L = FVector::Dist(A, E);
			const FVector LatDir = FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal();
			FLimb Lb;
			Lb.Level = 1;
			Lb.Bark = BarkIx;
			Lb.Sides = P.LimbSides;
			Lb.Cap = P.bDead ? 2 : 1;
			const float Rb = FMath::Max(2.f, Ra * P.BranchR * (1.15f - 0.45f * Ut));
			for (int32 k = 0; k <= 4; ++k)
			{
				const float S = float(k) / 4.f;
				FVector Pt = FMath::Lerp(A, E, S) + FVector(0.f, 0.f, L * (P.Arch * FMath::Sin(PI * S) + P.TipDrop * S * S));
				if (k > 0 && k < 4) Pt += LatDir * (L * P.Kink * Rs.R(-1.f, 1.f)) + FVector(0.f, 0.f, L * P.Kink * Rs.R(-0.6f, 0.6f));
				Lb.Pts.Add(Pt);
				Lb.Rad.Add(FMath::Max(P.bDead ? Rb * 0.3f : 1.2f, Rb * (1.f - 0.8f * S)));
			}
			for (int32 k = 1; k < Lb.Pts.Num(); ++k) Pool.Add(Lb.Pts[k]);
			if (!P.bDead) Tips.Add(Lb.Pts.Last());
			// Secondaries.
			const int32 NS = P.bDead ? (Rs.F() < 0.3f ? 1 : 0) : P.Secondaries;
			for (int32 k = 0; k < NS; ++k)
			{
				const float S0 = Rs.R(0.35f, 0.85f);
				float R0 = 0.f;
				const FVector St = PolyAt(Lb.Pts, Lb.Rad, S0, &R0);
				const FVector D2 = Dir.RotateAngleAxis(Rs.R(25.f, 55.f) * (Rs.F() < 0.5f ? -1.f : 1.f), FVector::UpVector).RotateAngleAxis(-Rs.R(5.f, 25.f), LatDir).GetSafeNormal();
				const float L2 = L * Rs.R(0.28f, 0.45f);
				FVector E2 = St + D2 * L2 + FVector(0.f, 0.f, L2 * P.TipDrop * 0.5f);
				const float T2 = (float(E2.Z) - CB) / FMath::Max(1.f, CT - CB);
				const float Lim2 = Env(P.Shape, T2) * RMax * 1.02f;
				const FVector C2 = Centre(float(E2.Z));
				if (FVector::Dist2D(E2, C2) > Lim2 && Lim2 > 1.f)
				{
					const FVector Off = (E2 - C2) * FVector(1.f, 1.f, 0.f);
					E2 = C2 + Off.GetSafeNormal() * Lim2 + FVector(0.f, 0.f, E2.Z - C2.Z);
				}
				FLimb S2;
				S2.Level = 2;
				S2.Bark = BarkIx;
				S2.Sides = P.TwigSides;
				S2.Cap = P.bDead ? 2 : 1;
				const FVector Mid = (St + E2) * 0.5f + FVector(0.f, 0.f, L2 * P.Arch * 0.8f);
				S2.Pts = { St, Mid, E2 };
				S2.Rad = { FMath::Max(1.f, R0 * 0.55f), FMath::Max(0.8f, R0 * 0.32f), FMath::Max(0.6f, R0 * 0.1f) };
				K.Limbs.Add(S2);
				Pool.Add(Mid);
				Pool.Add(E2);
				if (!P.bDead) SecondTips.Add(E2);
			}
			K.Limbs.Add(Lb);
		}
		// Reiterated tops beside a broken bole.
		for (int32 r = 0; r < P.Reiterate; ++r)
		{
			const float Zs0 = TopZ * Rs.R(0.82f, 0.95f);
			float Rr = 0.f;
			const FVector St = AxisAt(Axes[0], Zs0, &Rr);
			const float Phi = Phi0 + 2.f * PI * float(r) / float(P.Reiterate) + Rs.R(-0.4f, 0.4f);
			const FVector Out(FMath::Cos(Phi), FMath::Sin(Phi), 0.f);
			const float Zt = FMath::Min(H * Rs.R(0.9f, 0.985f), H);
			FLimb Re;
			Re.Level = 0;
			Re.Bark = BarkIx;
			Re.Sides = FMath::Max(6.f, P.LimbSides * 1.2f);
			for (int32 k = 0; k <= 5; ++k)
			{
				const float S = float(k) / 5.f;
				const FVector Pt = St + Out * (Rr * 0.9f + FMath::Sin(PI * 0.5f * S) * Rr * 2.2f) + FVector(0.f, 0.f, (Zt - Zs0) * S);
				Re.Pts.Add(Pt);
				Re.Rad.Add(FMath::Max(3.f, Rr * 0.32f * (1.f - 0.85f * S)));
			}
			for (int32 k = 1; k < Re.Pts.Num(); ++k) Pool.Add(Re.Pts[k]);
			Tips.Add(Re.Pts.Last());
			for (int32 k = 0; k < 4; ++k)
			{
				const float S0 = Rs.R(0.45f, 0.9f);
				float R0 = 0.f;
				const FVector Sb = PolyAt(Re.Pts, Re.Rad, S0, &R0);
				const float Pb = Phi + Rs.R(-1.6f, 1.6f);
				const FVector D2 = FVector(FMath::Cos(Pb), FMath::Sin(Pb), Rs.R(0.2f, 0.7f)).GetSafeNormal();
				const float L2 = RMax * Rs.R(0.25f, 0.45f);
				FLimb S2;
				S2.Level = 1;
				S2.Bark = BarkIx;
				S2.Sides = P.LimbSides;
				const FVector E2 = Sb + D2 * L2;
				S2.Pts = { Sb, (Sb + E2) * 0.5f + FVector(0.f, 0.f, L2 * 0.1f), E2 };
				S2.Rad = { FMath::Max(1.f, R0 * 0.5f), FMath::Max(1.f, R0 * 0.3f), FMath::Max(1.f, R0 * 0.08f) };
				K.Limbs.Add(S2);
				Pool.Add(E2);
				Tips.Add(E2);
			}
			K.Limbs.Add(Re);
		}
		if (P.bDead) return;
		// Sprays: limb tips first, then the secondaries', then the crown envelope filled on a golden-ratio sequence (any
		// prefix spreads evenly), each pulled to the wood within reach.
		const auto Add = [&](const FVector& C, float Size)
		{
			FCluster Cl;
			Cl.C = C;
			const FVector Ctr = Centre(float(C.Z));
			FVector Out = (C - Ctr) * FVector(1.f, 1.f, 0.f);
			const float Tz = FMath::Clamp((float(C.Z) - CB) / FMath::Max(1.f, CT - CB), 0.f, 1.f);
			const float Lim = FMath::Max(1.f, Env(P.Shape, Tz) * RMax);
			const float Rf = FMath::Clamp(float(Out.Size()) / Lim, 0.f, 1.f);
			Out = (Out.GetSafeNormal() + FVector(0.f, 0.f, (Tz - 0.35f) * 1.2f)).GetSafeNormal();
			if (Out.IsNearlyZero()) Out = FVector::UpVector;
			Cl.Out = Out;
			Cl.Up = (FVector::UpVector * (1.f - P.PadUp) + Out * P.PadUp).GetSafeNormal();
			Cl.Tone = FMath::Clamp(0.12f + 0.6f * Tz + 0.28f * Rf + Rs.R(-0.08f, 0.08f), 0.f, 1.f);
			Cl.Size = Size * Rs.R(0.8f, 1.2f);
			Cl.Spin = Rs.R(0.f, 2.f * PI);
			K.Clusters.Add(Cl);
		};
		for (const FVector& C : Tips) Add(C, P.LeafSize * 1.15f);
		for (const FVector& C : SecondTips) Add(C, P.LeafSize);
		for (int32 j = 0; j < P.Fill && Pool.Num() > 0; ++j)
		{
			const float Tt = FMath::Frac(0.5f + float(j) * 0.618034f);
			const float Phi = Phi0 + float(j) * 2.399963f;
			const float Rf = 0.62f + 0.38f * FMath::Frac(0.3f + float(j) * 0.7548777f);
			const float Z = FMath::Lerp(CB, CT, Tt);
			FVector Q = Centre(Z) + FVector(FMath::Cos(Phi), FMath::Sin(Phi), 0.f) * (Env(P.Shape, Tt) * RMax * Rf);
			float Best = TNumericLimits<float>::Max();
			FVector Near = Pool[0];
			for (const FVector& W : Pool)
			{
				const float D = float(FVector::DistSquared(W, Q));
				if (D < Best) { Best = D; Near = W; }
			}
			const float Reach = P.LeafSize * 1.5f;
			if (FMath::Sqrt(Best) > Reach) Q = Near + (Q - Near).GetSafeNormal() * Reach;
			Add(Q, P.LeafSize);
		}
	}

	/** Heart-tree sap veins: amber strips up the bark cracks, with short side veins. */
	static void SapVeins(FSkel& K, const FTrunk& T, FRng& Rs, int32 Count, float ToZ, const FLinearColor& Col, float Width)
	{
		for (int32 v = 0; v < Count; ++v)
		{
			float A = 2.f * PI * float(v) / float(Count) + Rs.R(-0.2f, 0.2f);
			FStrip S;
			S.Col = Col;
			S.Width = Width * Rs.R(0.8f, 1.2f);
			const int32 Steps = 40;
			const float Z0 = Rs.R(60.f, 250.f), Z1 = ToZ * Rs.R(0.75f, 1.f);
			FVector2D Branch(-1.f, -1.f);
			for (int32 k = 0; k <= Steps; ++k)
			{
				const float Z = FMath::Lerp(Z0, Z1, float(k) / float(Steps));
				A += Rs.R(-0.05f, 0.05f) + 0.012f;
				const FVector C = FMath::Lerp(T.Pts[0], T.Pts.Last(), 0.f);   // the bole's base (lean is small on these)
				float Rr = TrunkRadiusAt(T, Z, FMath::UnwindRadians(A));
				const FVector Dir(FMath::Cos(A), FMath::Sin(A), 0.f);
				// Follow the bole's own centreline.
				FVector Cz = C;
				for (int32 i = 0; i + 1 < T.Pts.Num(); ++i)
				{
					if (Z <= T.Pts[i + 1].Z)
					{
						const float Za = float(T.Pts[i].Z), Zb = float(T.Pts[i + 1].Z);
						Cz = FMath::Lerp(T.Pts[i], T.Pts[i + 1], Zb > Za ? FMath::Clamp((Z - Za) / (Zb - Za), 0.f, 1.f) : 0.f);
						break;
					}
				}
				S.Pts.Add(FVector(Cz.X, Cz.Y, Z) + Dir * (Rr * 1.01f));
				S.Nrm.Add(Dir);
				if (k == Steps / 3 || k == (2 * Steps) / 3) Branch = FVector2D(A, Z);
				if (Branch.X > -0.5f && (k == Steps / 3 + 1 || k == (2 * Steps) / 3 + 1))
				{
					// A side vein forking off up and across.
					FStrip Sv;
					Sv.Col = Col;
					Sv.Width = S.Width * 0.55f;
					float Ab = float(Branch.X);
					const float Sign = Rs.F() < 0.5f ? -1.f : 1.f;
					for (int32 q = 0; q <= 8; ++q)
					{
						const float Zq = float(Branch.Y) + float(q) * Rs.R(60.f, 120.f) * (ToZ / 14000.f + 0.3f);
						Ab += Sign * 0.04f;
						const FVector Dq(FMath::Cos(Ab), FMath::Sin(Ab), 0.f);
						Sv.Pts.Add(FVector(Cz.X, Cz.Y, Zq) + Dq * (TrunkRadiusAt(T, Zq, FMath::UnwindRadians(Ab)) * 1.01f));
						Sv.Nrm.Add(Dq);
					}
					K.Strips.Add(Sv);
					Branch = FVector2D(-1.f, -1.f);
				}
			}
			K.Strips.Add(S);
		}
	}

	/** Bracket fungus shelves (half-discs out of the bark): cream tops with darker growth bands, tan undersides. */
	static void AddShelves(FSkel& K, const FTrunk& T, FRng& Rs, int32 Count, float Z0, float Z1, float Reach0, float Reach1)
	{
		struct FShelf { FVector At, Out; float Reach, Width, Thick; };
		TArray<FShelf> Shelves;
		const float A0 = Rs.R(0.f, 2.f * PI);
		for (int32 i = 0; i < Count; ++i)
		{
			const float F = float(i) / float(FMath::Max(1, Count - 1));
			const float A = A0 + F * 2.f * PI * 1.25f + Rs.R(-0.15f, 0.15f);   // a spiral band climbing round the bole
			if (T.CrackHalf > 0.f && FMath::Abs(FMath::FindDeltaAngleRadians(0.f, A)) < T.CrackHalf * 1.6f) continue;
			const float Z = FMath::Lerp(Z0, Z1, F) + Rs.R(-40.f, 40.f);
			const FVector Dir(FMath::Cos(A), FMath::Sin(A), 0.f);
			FShelf S;
			S.Out = Dir;
			S.At = FVector(T.Pts[0].X, T.Pts[0].Y, Z) + Dir * (TrunkRadiusAt(T, Z, FMath::UnwindRadians(A)) * 0.93f);
			S.Reach = Rs.R(Reach0, Reach1);
			S.Width = S.Reach * Rs.R(1.5f, 2.1f);
			S.Thick = S.Reach * Rs.R(0.1f, 0.16f);
			Shelves.Add(S);
		}
		K.Extras.Add([Shelves](FGeo& G, const FDetail& D)
		{
			const int32 M = D.Q > 0.7f ? 10 : D.Q > 0.35f ? 6 : 4;
			for (int32 i = 0; i < Shelves.Num(); ++i)
			{
				if (D.Q < 0.35f && i % 2 == 1) continue;
				const FShelf& S = Shelves[i];
				const FVector Side(-S.Out.Y, S.Out.X, 0.f);
				TArray<FVector> Rim;
				for (int32 k = 0; k <= M; ++k)
				{
					const float A = -0.5f * PI + PI * float(k) / float(M);
					const float W = 0.9f + 0.2f * Hash3(i, k, 3);
					Rim.Add(S.At + S.Out * (FMath::Cos(A) * S.Reach * W) + Side * (FMath::Sin(A) * S.Width * 0.5f * W) - FVector(0.f, 0.f, FMath::Cos(A) * S.Reach * 0.08f));
				}
				// Top (a low dome), bottom, and the rounded lip.
				const int32 Ct = G.V(SBark, S.At + FVector(0.f, 0.f, S.Thick * 0.6f), FVector::UpVector, S.Out, 0.5f, 0.5f, Hex(0xC9B78E), 0.75f);
				const int32 Cb = G.V(SBark, S.At - FVector(0.f, 0.f, S.Thick * 0.5f), -FVector::UpVector, S.Out, 0.5f, 0.5f, Hex(0x7A6044), 0.9f);
				TArray<int32> Top, Mid, Bot;
				for (int32 k = 0; k <= M; ++k)
				{
					const FVector Up = FVector(0.f, 0.f, S.Thick * 0.25f);
					const FVector Mp = FMath::Lerp(S.At, Rim[k], 0.55f) + FVector(0.f, 0.f, S.Thick * 0.45f);
					Mid.Add(G.V(SBark, Mp, (FVector::UpVector * 2.f + (Rim[k] - S.At).GetSafeNormal()).GetSafeNormal(), S.Out, 0.5f, 0.f, Hex(0xE8DCC0), 0.7f));
					Top.Add(G.V(SBark, Rim[k] + Up, ((Rim[k] - S.At).GetSafeNormal() + FVector::UpVector).GetSafeNormal(), S.Out, 1.f, 0.f, Hex(0xB8945C), 0.75f));
					Bot.Add(G.V(SBark, Rim[k] - Up, ((Rim[k] - S.At).GetSafeNormal() - FVector::UpVector).GetSafeNormal(), S.Out, 1.f, 1.f, Hex(0x9A7E58), 0.9f));
				}
				for (int32 k = 0; k < M; ++k)
				{
					G.Tri(SBark, Ct, Mid[k], Mid[k + 1], FVector::UpVector);
					G.Quad(SBark, Mid[k], Mid[k + 1], Top[k + 1], Top[k], FVector::UpVector);
					G.Tri(SBark, Cb, Bot[k], Bot[k + 1], -FVector::UpVector);
					const FVector Lip = ((Rim[k] + Rim[k + 1]) * 0.5f - S.At).GetSafeNormal();
					G.Quad(SBark, Top[k], Top[k + 1], Bot[k + 1], Bot[k], Lip);
				}
			}
		});
	}

	/** Crystals grown through roots (the Bell-Tree, the Crystal border): hexagonal prisms with a point, in the Glow slot. */
	static void AddCrystals(FSkel& K, FRng& Rs, const FVector& Centre, float R0, float R1, int32 Count, const FLinearColor& Col)
	{
		struct FCr { FVector At, Dir; float Len, Rad; };
		TArray<FCr> Cr;
		for (int32 i = 0; i < Count; ++i)
		{
			const float A = Rs.R(0.f, 2.f * PI);
			const FVector Out(FMath::Cos(A), FMath::Sin(A), 0.f);
			FCr C;
			C.At = Centre + Out * Rs.R(R0, R1) - FVector(0.f, 0.f, 15.f);
			C.Dir = (Out * Rs.R(0.2f, 0.7f) + FVector::UpVector).GetSafeNormal();
			C.Len = Rs.R(35.f, 95.f);
			C.Rad = C.Len * Rs.R(0.13f, 0.2f);
			Cr.Add(C);
		}
		K.Extras.Add([Cr, Col](FGeo& G, const FDetail& D)
		{
			for (int32 i = 0; i < Cr.Num(); ++i)
			{
				if (D.Q < 0.35f && i % 2 == 1) continue;
				const FCr& C = Cr[i];
				FVector U, V;
				Frame(C.Dir, U, V);
				const FVector Tip = C.At + C.Dir * C.Len;
				const FVector Sh = C.At + C.Dir * (C.Len * 0.72f);
				TArray<int32> Lo, Hi;
				for (int32 s = 0; s < 6; ++s)
				{
					const float A = PI / 3.f * float(s);
					const FVector Rad = (U * FMath::Cos(A) + V * FMath::Sin(A)) * C.Rad;
					Lo.Add(G.V(SGlow, C.At + Rad, Rad, C.Dir, 0.f, float(s) / 6.f, Col * 0.6f, 0.15f));
					Hi.Add(G.V(SGlow, Sh + Rad, Rad, C.Dir, 1.f, float(s) / 6.f, Col, 0.12f));
				}
				const int32 T = G.V(SGlow, Tip, C.Dir, U, 1.f, 0.5f, Col * 1.2f, 0.1f);
				for (int32 s = 0; s < 6; ++s)
				{
					const int32 n = (s + 1) % 6;
					const FVector Out = FVector(G.S[SGlow].N[Lo[s]]) + FVector(G.S[SGlow].N[Lo[n]]);
					G.Quad(SGlow, Lo[s], Lo[n], Hi[n], Hi[s], Out);
					G.Tri(SGlow, Hi[s], Hi[n], T, Out + C.Dir);
				}
			}
		});
	}

	static FBark MakeBark(uint32 Base, uint32 Dark, uint32 MossHex, float Rough, float MossUp, float Band, float Patch, float Ridges, float Furrow)
	{
		FBark B;
		B.Base = Hex(Base);
		B.Dark = Hex(Dark);
		B.Moss = Hex(MossHex);
		B.Rough = Rough;
		B.MossUp = MossUp;
		B.MossBand = Band;
		B.MossPatch = Patch;
		B.Ridges = Ridges;
		B.Furrow = Furrow;
		return B;
	}

	/** A fallen giant along +X: root plate at 0, the trunk sunk a little into the litter, the snapped amber end at Len. */
	static void FallenLog(FSkel& K, FRng& Rs, float Len, float R0, bool bNurse)
	{
		K.Barks.Add(MakeBark(0x4A3A2A, 0x241A12, 0x4E6B2A, 0.92f, 1.f, 0.f, 0.45f, 20.f, 0.55f));
		K.Barks.Add(MakeBark(0x3A2C20, 0x1E160E, 0x3A5222, 0.95f, 0.8f, 0.f, 0.2f, 6.f, 0.3f));   // roots and soil
		const float Sag = Rs.R(-1.f, 1.f) * Len * 0.015f;
		const float AxisZ = R0 * 0.82f;
		const auto RAt = [&](float S) { return R0 * FMath::Lerp(1.05f, 0.78f, S); };
		const auto AxAt = [&](float S) { return FVector(Len * S, FMath::Sin(PI * S) * Sag, AxisZ - RAt(S) * 0.0f + (RAt(S) - R0) * 0.82f); };
		FLimb Lg;
		Lg.Level = 0;
		Lg.Bark = 0;
		Lg.Sides = 22.f;
		Lg.Cap = 3;
		for (int32 k = 0; k <= 10; ++k)
		{
			const float S = float(k) / 10.f;
			Lg.Pts.Add(AxAt(S));
			Lg.Rad.Add(RAt(S) * (1.f + 0.04f * FMath::Sin(S * 23.f + Sag)));
		}
		K.Limbs.Add(Lg);
		// The root plate: an upturned disc of soil and roots standing across the log's foot, sunk into its pit.
		const float Rp = R0 * Rs.R(2.1f, 2.5f);
		const FVector Pc(-R0 * 0.25f, 0.f, AxisZ + R0 * 0.1f);
		{
			TArray<float> Edge;
			for (int32 k = 0; k < 24; ++k) Edge.Add(Rp * Rs.R(0.82f, 1.1f));
			K.Extras.Add([Pc, Edge, R0](FGeo& G, const FDetail& D)
			{
				const int32 Step = D.Q > 0.7f ? 1 : D.Q > 0.35f ? 2 : 4;
				const float Th = R0 * 0.35f;
				TArray<int32> Fr, Bk;
				const int32 Cf = G.V(SBark, Pc - FVector(Th, 0.f, 0.f), -FVector::XAxisVector, FVector::YAxisVector, 0.5f, 0.5f, Hex(0x2E2218), 0.97f);
				const int32 Cb = G.V(SBark, Pc + FVector(Th * 0.5f, 0.f, 0.f), FVector::XAxisVector, FVector::YAxisVector, 0.5f, 0.5f, Hex(0x3A2C20), 0.97f);
				for (int32 k = 0; k < 24; k += Step)
				{
					const float A = 2.f * PI * float(k) / 24.f;
					const FVector Dir(0.f, FMath::Cos(A), FMath::Sin(A));
					const FVector P = Pc + Dir * Edge[k];
					const float Up = FMath::Max(0.f, float(Dir.Z));
					const FLinearColor Soil = FMath::Lerp(Hex(0x3A2C20), Hex(0x4E6B2A), Up * 0.7f);
					Fr.Add(G.V(SBark, P - FVector(Th * 0.6f, 0.f, 0.f), (Dir - FVector::XAxisVector).GetSafeNormal(), Dir, 1.f, float(k) / 24.f, Soil * 0.85f, 0.96f));
					Bk.Add(G.V(SBark, P + FVector(Th * 0.4f, 0.f, 0.f), (Dir + FVector::XAxisVector).GetSafeNormal(), Dir, 1.f, float(k) / 24.f, Soil, 0.96f));
				}
				for (int32 k = 0; k < Fr.Num(); ++k)
				{
					const int32 n = (k + 1) % Fr.Num();
					G.Tri(SBark, Cf, Fr[k], Fr[n], -FVector::XAxisVector);
					G.Tri(SBark, Cb, Bk[k], Bk[n], FVector::XAxisVector);
					const FVector Out = FVector(G.S[SBark].P[Fr[k]]) + FVector(G.S[SBark].P[Fr[n]]) - Pc * 2.f;
					G.Quad(SBark, Fr[k], Fr[n], Bk[n], Bk[k], Out * FVector(0.f, 1.f, 1.f));
				}
			});
		}
		// Torn roots radiating out of the plate's face, past its rim, some hanging down to the ground.
		for (int32 k = 0; k < 13; ++k)
		{
			const float A = 2.f * PI * float(k) / 13.f + Rs.R(-0.2f, 0.2f);
			const FVector Dir(0.f, FMath::Cos(A), FMath::Sin(A));
			FLimb Rt;
			Rt.Level = k % 3 == 0 ? 1 : 3;
			Rt.Bark = 1;
			Rt.Sides = 6.f;
			Rt.Cap = 2;
			const float Reach = Rp * Rs.R(1.05f, 1.45f);
			for (int32 q = 0; q <= 4; ++q)
			{
				const float S = float(q) / 4.f;
				FVector P = Pc + Dir * (Reach * FMath::Lerp(0.25f, 1.f, S)) - FVector(R0 * 0.4f * S + R0 * 0.3f, 0.f, 0.f);
				if (Dir.Z < 0.2f) P.Z -= S * S * R0 * 0.6f;   // the lower ones droop to the ground
				P.Z = FMath::Max(P.Z, -R0 * 0.3);
				Rt.Pts.Add(P);
				Rt.Rad.Add(FMath::Max(2.f, R0 * 0.13f * (1.f - 0.75f * S)));
			}
			K.Limbs.Add(Rt);
		}
		// A few snapped limb stubs along its back.
		for (int32 k = 0; k < 4; ++k)
		{
			const float S = Rs.R(0.25f, 0.9f);
			const float A = Rs.R(0.4f, 2.7f) * (Rs.F() < 0.5f ? -1.f : 1.f);
			const FVector Dir(Rs.R(0.1f, 0.4f), FMath::Cos(A), FMath::Abs(FMath::Sin(A)) + 0.3f);
			const FVector B0 = AxAt(S) + FVector(0.f, FMath::Cos(A), FMath::Sin(A)) * (RAt(S) * 0.8f);
			FLimb St;
			St.Level = 2;
			St.Bark = 0;
			St.Sides = 7.f;
			St.Cap = 2;
			const float L = R0 * Rs.R(0.8f, 1.8f);
			St.Pts = { B0, B0 + Dir.GetSafeNormal() * L };
			St.Rad = { R0 * 0.22f, R0 * 0.15f };
			K.Limbs.Add(St);
		}
		// Collision: the log in three pieces, the plate.
		for (const FVector2D& Seg : { FVector2D(0.0, 0.36), FVector2D(0.36, 0.7), FVector2D(0.7, 1.0) })
		{
			FWRTreeCollider C;
			C.A = AxAt(float(Seg.X));
			C.B = AxAt(float(Seg.Y));
			C.Radius = RAt(float(Seg.Y)) * 0.95f;
			K.Colliders.Add(C);
		}
		{
			FWRTreeCollider C;
			C.Radius = R0 * 0.35f;
			C.A = Pc + FVector(0.f, -Rp * 0.7f, 0.f);
			C.B = Pc + FVector(0.f, Rp * 0.7f, 0.f);
			K.Colliders.Add(C);
			C.A = Pc + FVector(0.f, 0.f, -Rp * 0.3f);
			C.B = Pc + FVector(0.f, 0.f, Rp * 0.75f);
			K.Colliders.Add(C);
		}
		K.Anchors.Add(TEXT("broken_end"), AxAt(1.f));
		K.Anchors.Add(TEXT("root_plate"), Pc);
		if (bNurse)
		{
			// Saplings in a row along its back, roots straddling it down to the ground (DC: "nurse logs with trees in a row").
			K.Barks.Add(MakeBark(0x6A5A48, 0x3E3226, 0x4E6B2A, 0.8f, 0.4f, 0.f, 0.f, 6.f, 0.25f));
			K.LeafShape = LeafBeech;
			K.LeafLo = Hex(0x3E5E22);
			K.LeafHi = Hex(0x8AAA40);
			const int32 N = Rs.I(5, 7);
			for (int32 i = 0; i < N; ++i)
			{
				const float S = FMath::Lerp(0.2f, 0.85f, (float(i) + Rs.R(0.2f, 0.8f)) / float(N));
				const FVector Top = AxAt(S) + FVector(0.f, 0.f, RAt(S) * 0.97f);
				const float Hh = Rs.R(250.f, 520.f);
				FLimb Stem;
				Stem.Level = 1;
				Stem.Bark = 2;
				Stem.Sides = 5.f;
				for (int32 q = 0; q <= 3; ++q)
				{
					const float F = float(q) / 3.f;
					Stem.Pts.Add(Top + FVector(Rs.R(-8.f, 8.f) * F, Rs.R(-8.f, 8.f) * F, Hh * F));
					Stem.Rad.Add(FMath::Max(0.8f, Rs.R(5.f, 8.f) * (1.f - 0.85f * F)));
				}
				K.Limbs.Add(Stem);
				for (const float Side : { -1.f, 1.f })
				{
					FLimb Rt;
					Rt.Level = 2;
					Rt.Bark = 2;
					Rt.Sides = 4.f;
					for (int32 q = 0; q <= 4; ++q)
					{
						const float A = FMath::Lerp(0.5f * PI, 0.5f * PI + Side * PI * 0.62f, float(q) / 4.f);
						Rt.Pts.Add(AxAt(S) + FVector(Rs.R(-10.f, 10.f), FMath::Cos(A) * RAt(S) * 1.03f, FMath::Sin(A) * RAt(S) * 1.03f));
						Rt.Rad.Add(FMath::Max(1.f, 4.f * (1.f - 0.6f * float(q) / 4.f)));
					}
					Rt.Pts.Add(Rt.Pts.Last() + FVector(0.f, Side * 20.f, -AxisZ));
					Rt.Rad.Add(1.2f);
					K.Limbs.Add(Rt);
				}
				for (int32 c = 0; c < 7; ++c)
				{
					FCluster Cl;
					const float Ph = 2.f * PI * float(c) / 7.f + Rs.R(-0.3f, 0.3f);
					const float Z = Hh * Rs.R(0.45f, 1.f);
					const FVector Out(FMath::Cos(Ph), FMath::Sin(Ph), 0.3f);
					Cl.C = Top + FVector(0.f, 0.f, Z) + Out * (Hh * 0.18f * (1.15f - Z / Hh));
					Cl.Out = Out.GetSafeNormal();
					Cl.Size = Rs.R(80.f, 120.f);
					Cl.Tone = Z / Hh;
					Cl.Spin = Rs.R(0.f, 2.f * PI);
					K.Clusters.Add(Cl);
				}
			}
		}
	}

	/** The top of the strangler's crown (cm, reference size): 60 m. */
	static float H_StranglerTop() { return 6000.f; }

	/** The strangler fig tower: lattice of fused roots, a spiral root ramp inside, the vista deck, the fig's crown. */
	static void Strangler(FSkel& K, FRng& Rs)
	{
		K.Barks.Add(MakeBark(0x6E6252, 0x3E352A, 0x4E6B2A, 0.85f, 0.75f, 300.f, 0.35f, 5.f, 0.3f));
		K.LeafShape = LeafFig;
		K.LeafLo = Hex(0x1E3A1A);
		K.LeafHi = Hex(0x3E6A2E);
		K.LeafRough = 0.45f;
		const float RingR = 600.f, DeckZ = 5200.f, Rc = 360.f, HalfW = 110.f;
		const float Pitch = Rc * FMath::Tan(FMath::DegreesToRadians(28.f));   // cm rise per radian: a 28 degree climb
		const float TotalA = (DeckZ - 30.f) / Pitch;
		const float EndA = FMath::Fmod(TotalA, 2.f * PI);
		const float HatchLo = EndA - FMath::DegreesToRadians(100.f), HatchHi = EndA + FMath::DegreesToRadians(10.f);
		const auto InHatch = [HatchLo, HatchHi](float A)
		{
			const float D = FMath::FindDeltaAngleRadians((HatchLo + HatchHi) * 0.5f, A);
			return FMath::Abs(D) < (HatchHi - HatchLo) * 0.5f;
		};
		// The lattice: strands spiralling both ways (so they cross and fuse), none through the doorway at +X below 3.6 m.
		const int32 NS = 14;
		for (int32 s = 0; s < NS; ++s)
		{
			const float Sign = s % 2 == 0 ? 1.f : -1.f;
			const float A0 = FMath::DegreesToRadians(34.f + 292.f * (float(s) + 0.5f) / float(NS) + Rs.R(-5.f, 5.f));
			const float W = Sign * 2.f * PI / 5000.f * Rs.R(0.8f, 1.2f);
			FLimb St;
			St.Level = 0;
			St.Bark = 0;
			St.Sides = 7.f;
			St.Cap = 1;
			for (int32 k = 0; k <= 22; ++k)
			{
				const float Z = -50.f + (DeckZ + 50.f) * float(k) / 22.f;
				const float A = A0 + W * FMath::Max(0.f, Z);
				const float Foot = Z < 400.f ? FMath::Square(1.f - FMath::Max(0.f, Z) / 400.f) * 260.f : 0.f;
				const float Rr = RingR * (1.f + 0.05f * FMath::Sin(Z / 700.f + float(s))) + Foot;
				St.Pts.Add(FVector(FMath::Cos(A) * Rr, FMath::Sin(A) * Rr, Z));
				St.Rad.Add(FMath::Lerp(48.f, 26.f, FMath::Clamp(Z / DeckZ, 0.f, 1.f)) * (0.85f + 0.3f * Hash3(s, k, 1)) + Foot * 0.12f);
			}
			// Collision along the strand in five pieces.
			for (int32 c = 0; c < 5; ++c)
			{
				FWRTreeCollider C;
				C.A = St.Pts[c * 22 / 5];
				C.B = St.Pts[(c + 1) * 22 / 5];
				C.Radius = St.Rad[(c * 22 / 5 + (c + 1) * 22 / 5) / 2] * 0.95f;
				K.Colliders.Add(C);
			}
			K.Limbs.Add(St);
		}
		// Fused bands round the tower (where strands grew together), above the doorway.
		for (const float Z : { 1200.f, 2400.f, 3600.f, 4800.f })
		{
			float A = Rs.R(0.f, 2.f * PI);
			for (int32 Arc = 0; Arc < 3; ++Arc)
			{
				const float Span = Rs.R(0.9f, 2.f);
				FLimb B;
				B.Level = 2;
				B.Bark = 0;
				B.Sides = 5.f;
				for (int32 k = 0; k <= 6; ++k)
				{
					const float Aa = A + Span * float(k) / 6.f;
					B.Pts.Add(FVector(FMath::Cos(Aa) * RingR, FMath::Sin(Aa) * RingR, Z + FMath::Sin(Aa * 3.f) * 60.f));
					B.Rad.Add(20.f + 6.f * FMath::Sin(PI * float(k) / 6.f));
				}
				K.Limbs.Add(B);
				A += Span + Rs.R(0.4f, 1.f);
			}
		}
		// The ramp: a woven root ribbon spiralling up inside, held to the lattice by struts.
		const auto RampAt = [Rc, Pitch](float A, float Off) { return FVector(FMath::Cos(A) * (Rc + Off), FMath::Sin(A) * (Rc + Off), 30.f + A * Pitch); };
		{
			const FBark Bk = K.Barks[0];
			K.Extras.Add([RampAt, TotalA, HalfW, Bk](FGeo& G, const FDetail& D)
			{
				const float Step = D.Q > 0.7f ? 0.3f : D.Q > 0.35f ? 0.6f : 1.2f;
				const int32 N = FMath::Max(2, FMath::CeilToInt32(TotalA / Step));
				const float Thick = 35.f;
				TArray<int32> TopIn, TopOut, BotIn, BotOut;
				for (int32 k = 0; k <= N; ++k)
				{
					const float A = TotalA * float(k) / float(N);
					const FVector In = RampAt(A, -HalfW), Out = RampAt(A, HalfW);
					const FVector Tg = (RampAt(A + 0.01f, 0.f) - RampAt(A, 0.f)).GetSafeNormal();
					float Rough = 0.9f;
					const FLinearColor Ct = BarkAt(Bk, (In + Out) * 0.5f, FVector::UpVector, A, A * 300.f, Rough);
					TopIn.Add(G.V(SBark, In, FVector::UpVector, Tg, A * 2.4f, 0.f, Ct, Rough));
					TopOut.Add(G.V(SBark, Out, FVector::UpVector, Tg, A * 2.4f, 1.5f, Ct, Rough));
					BotIn.Add(G.V(SBark, In - FVector(0.f, 0.f, Thick), -FVector::UpVector, Tg, A * 2.4f, 0.f, Bk.Base * 0.7f, 0.95f));
					BotOut.Add(G.V(SBark, Out - FVector(0.f, 0.f, Thick), -FVector::UpVector, Tg, A * 2.4f, 1.5f, Bk.Base * 0.7f, 0.95f));
				}
				for (int32 k = 0; k < N; ++k)
				{
					G.Quad(SBark, TopIn[k], TopOut[k], TopOut[k + 1], TopIn[k + 1], FVector::UpVector);
					G.Quad(SBark, BotIn[k], BotOut[k], BotOut[k + 1], BotIn[k + 1], -FVector::UpVector);
				}
			});
			// Edge roots (a kerb inside, a rail root outside) and the struts to the lattice.
			for (const float Off : { -HalfW, HalfW })
			{
				FLimb E;
				E.Level = 1;
				E.Bark = 0;
				E.Sides = 5.f;
				E.Cap = 1;
				const int32 N = 40;
				for (int32 k = 0; k <= N; ++k)
				{
					const float A = TotalA * float(k) / float(N);
					E.Pts.Add(RampAt(A, Off) + FVector(0.f, 0.f, Off < 0.f ? 14.f : 22.f));
					E.Rad.Add(Off < 0.f ? 22.f : 28.f);
				}
				K.Limbs.Add(E);
			}
			for (float A = 1.2f; A < TotalA - 0.5f; A += 1.5f)
			{
				FLimb S;
				S.Level = 2;
				S.Bark = 0;
				S.Sides = 5.f;
				const FVector P0 = RampAt(A, HalfW) - FVector(0.f, 0.f, 20.f);
				const FVector P1 = FVector(FMath::Cos(A) * 590.f, FMath::Sin(A) * 590.f, P0.Z + 120.f);
				S.Pts = { P0, (P0 + P1) * 0.5f - FVector(0.f, 0.f, 40.f), P1 };
				S.Rad = { 18.f, 14.f, 20.f };
				K.Limbs.Add(S);
			}
			// Ramp collision: two capsules side by side per piece, their tops flush with the walking surface.
			const float CapR = 62.f;
			for (float A = 0.f; A < TotalA - 0.01f; A += 0.5f)
			{
				const float A1 = FMath::Min(TotalA, A + 0.5f);
				for (const float Off : { -HalfW * 0.48f, HalfW * 0.48f })
				{
					FWRTreeCollider C;
					C.A = RampAt(A, Off) - FVector(0.f, 0.f, CapR);
					C.B = RampAt(A1, Off) - FVector(0.f, 0.f, CapR);
					C.Radius = CapR;
					K.Colliders.Add(C);
				}
			}
		}
		// The vista deck: a disc of woven roots, open where the ramp comes up through it.
		{
			const float DeckR = 540.f;
			const FBark Bk = K.Barks[0];
			K.Extras.Add([DeckZ, DeckR, InHatch, Bk](FGeo& G, const FDetail& D)
			{
				const int32 Seg = D.Q > 0.7f ? 36 : D.Q > 0.35f ? 18 : 10;
				const TArray<float> Radii = { 0.f, 200.f, 380.f, DeckR };
				for (int32 r = 0; r + 1 < Radii.Num(); ++r)
				{
					for (int32 s = 0; s < Seg; ++s)
					{
						const float A0 = 2.f * PI * float(s) / float(Seg), A1 = 2.f * PI * float(s + 1) / float(Seg);
						if (Radii[r] >= 199.f && InHatch((A0 + A1) * 0.5f)) continue;
						for (const int32 Face : { 0, 1 })
						{
							const float Z = DeckZ - (Face == 1 ? 30.f : 0.f);
							const FVector N = Face == 0 ? FVector::UpVector : -FVector::UpVector;
							const FVector Q[4] = {
								FVector(FMath::Cos(A0) * Radii[r], FMath::Sin(A0) * Radii[r], Z), FVector(FMath::Cos(A1) * Radii[r], FMath::Sin(A1) * Radii[r], Z),
								FVector(FMath::Cos(A1) * Radii[r + 1], FMath::Sin(A1) * Radii[r + 1], Z), FVector(FMath::Cos(A0) * Radii[r + 1], FMath::Sin(A0) * Radii[r + 1], Z) };
							int32 I[4];
							for (int32 q = 0; q < 4; ++q)
							{
								float Rough = 0.9f;
								const FLinearColor C = Face == 0 ? BarkAt(Bk, Q[q], FVector::UpVector, A0, float(Radii[r]), Rough) : Bk.Base * 0.6f;
								I[q] = G.V(SBark, Q[q], N, FVector::XAxisVector, float(Q[q].X) / 150.f, float(Q[q].Y) / 150.f, C, Rough);
							}
							G.Quad(SBark, I[0], I[1], I[2], I[3], N);
						}
					}
				}
			});
			// Woven ribs over the deck surface and a rail root round its rim.
			for (int32 k = 0; k < 12; ++k)
			{
				const float A = 2.f * PI * float(k) / 12.f;
				if (InHatch(A)) continue;
				FLimb Rb;
				Rb.Level = 2;
				Rb.Bark = 0;
				Rb.Sides = 4.f;
				Rb.Pts = { FVector(FMath::Cos(A) * 40.f, FMath::Sin(A) * 40.f, DeckZ + 4.f), FVector(FMath::Cos(A) * 300.f, FMath::Sin(A) * 300.f, DeckZ + 10.f), FVector(FMath::Cos(A) * DeckR, FMath::Sin(A) * DeckR, DeckZ + 4.f) };
				Rb.Rad = { 9.f, 12.f, 14.f };
				K.Limbs.Add(Rb);
			}
			{
				FLimb Rail;
				Rail.Level = 1;
				Rail.Bark = 0;
				Rail.Sides = 6.f;
				Rail.Cap = 1;
				for (int32 k = 0; k <= 40; ++k)
				{
					const float A = 2.f * PI * float(k) / 40.f;
					Rail.Pts.Add(FVector(FMath::Cos(A) * (DeckR + 10.f), FMath::Sin(A) * (DeckR + 10.f), DeckZ + 95.f + 20.f * FMath::Sin(A * 5.f)));
					Rail.Rad.Add(18.f);
				}
				K.Limbs.Add(Rail);
				FWRTreeCollider C;
				C.Radius = 18.f;
				for (int32 k = 0; k < 16; ++k)
				{
					const float A0 = 2.f * PI * float(k) / 16.f, A1 = 2.f * PI * float(k + 1) / 16.f;
					C.A = FVector(FMath::Cos(A0) * (DeckR + 10.f), FMath::Sin(A0) * (DeckR + 10.f), DeckZ + 95.f);
					C.B = FVector(FMath::Cos(A1) * (DeckR + 10.f), FMath::Sin(A1) * (DeckR + 10.f), DeckZ + 95.f);
					K.Colliders.Add(C);
				}
			}
			// Deck collision: rings of capsules (overlapping) with their tops at the deck, open over the hatch.
			const float CapR = 58.f;
			for (const float Rr : { 60.f, 170.f, 280.f, 390.f, 500.f })
			{
				const int32 Seg = FMath::Max(4, FMath::CeilToInt32(2.f * PI * Rr / 160.f));
				for (int32 s = 0; s < Seg; ++s)
				{
					const float A0 = 2.f * PI * float(s) / float(Seg), A1 = 2.f * PI * float(s + 1) / float(Seg);
					if (Rr >= 199.f && InHatch((A0 + A1) * 0.5f)) continue;
					FWRTreeCollider C;
					C.Radius = CapR;
					C.A = FVector(FMath::Cos(A0) * Rr, FMath::Sin(A0) * Rr, DeckZ - CapR);
					C.B = FVector(FMath::Cos(A1) * Rr, FMath::Sin(A1) * Rr, DeckZ - CapR);
					K.Colliders.Add(C);
				}
			}
		}
		// The fig's crown: limbs out of the strand tops, sprays in a ring round the deck (open over it, the vista).
		TArray<FVector> Pool;
		for (int32 b = 0; b < 10; ++b)
		{
			const float A = 2.f * PI * float(b) / 10.f + Rs.R(-0.2f, 0.2f);
			const FVector St(FMath::Cos(A) * RingR, FMath::Sin(A) * RingR, DeckZ - Rs.R(100.f, 600.f));
			const FVector En = St + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * Rs.R(250.f, 450.f) + FVector(0.f, 0.f, Rs.R(300.f, 800.f));
			FLimb L;
			L.Level = 1;
			L.Bark = 0;
			L.Sides = 6.f;
			L.Pts = { St, (St + En) * 0.5f + FVector(0.f, 0.f, 80.f), En };
			L.Rad = { 30.f, 20.f, 6.f };
			K.Limbs.Add(L);
			Pool.Add(L.Pts[1]);
			Pool.Add(En);
		}
		for (int32 j = 0; j < 190; ++j)
		{
			const float Tt = FMath::Frac(0.5f + float(j) * 0.618034f);
			const float Phi = float(j) * 2.399963f;
			const float Rr = FMath::Lerp(470.f, 1050.f, FMath::Frac(0.3f + float(j) * 0.7548777f)) * (1.f - 0.35f * Tt);
			const FVector Q(FMath::Cos(Phi) * Rr, FMath::Sin(Phi) * Rr, FMath::Lerp(DeckZ - 500.f, H_StranglerTop(), Tt));
			FCluster Cl;
			Cl.C = Q;
			Cl.Out = (FVector(FMath::Cos(Phi), FMath::Sin(Phi), 0.f) + FVector(0.f, 0.f, Tt - 0.3f)).GetSafeNormal();
			Cl.Up = FVector::UpVector;
			Cl.Size = Rs.R(220.f, 320.f);
			Cl.Tone = FMath::Clamp(0.2f + 0.7f * Tt + Rs.R(-0.1f, 0.1f), 0.f, 1.f);
			Cl.Spin = Rs.R(0.f, 2.f * PI);
			K.Clusters.Add(Cl);
		}
		K.Anchors.Add(TEXT("vista"), FVector(0.f, 0.f, DeckZ));
		K.Anchors.Add(TEXT("entrance"), FVector(RingR, 0.f, 0.f));
	}

	static void Fern(FSkel& K, FRng& Rs)
	{
		struct FFrond { FVector Dir; float Len, Width, Lift, Roll; };
		TArray<FFrond> Fronds;
		const int32 N = Rs.I(8, 10);
		const float A0 = Rs.R(0.f, 2.f * PI);
		for (int32 i = 0; i < N; ++i)
		{
			const float A = A0 + 2.f * PI * float(i) / float(N) + Rs.R(-0.2f, 0.2f);
			FFrond F;
			F.Dir = FVector(FMath::Cos(A), FMath::Sin(A), 0.f);
			F.Len = Rs.R(95.f, 135.f);
			F.Width = F.Len * Rs.R(0.13f, 0.17f);
			F.Lift = Rs.R(55.f, 75.f);
			F.Roll = Rs.R(-15.f, 15.f);
			Fronds.Add(F);
		}
		const FLinearColor Lo = Hex(0x3E6A30), Hi = Hex(0x6E9A4A), Stem = Hex(0x3A4A22);
		K.Extras.Add([Fronds, Lo, Hi, Stem](FGeo& G, const FDetail& D)
		{
			const int32 Use = D.Q > 0.7f ? Fronds.Num() : FMath::Min(5, Fronds.Num());
			const int32 Seg = D.Q > 0.7f ? 4 : 2;
			for (int32 f = 0; f < Use; ++f)
			{
				const FFrond& F = Fronds[f * Fronds.Num() / Use];
				// The rachis: rising at Lift degrees, arching over to droop at the tip.
				TArray<FVector> Rc;
				TArray<FVector> Tg;
				FVector P = F.Dir * 6.f;
				const int32 Fine = Seg * 4;
				TArray<FVector> Dense;
				Dense.Add(P);
				for (int32 k = 0; k < Fine; ++k)
				{
					const float S = (float(k) + 0.5f) / float(Fine);
					const float El = FMath::DegreesToRadians(FMath::Lerp(F.Lift, -25.f, S * S));
					P += (F.Dir * FMath::Cos(El) + FVector::UpVector * FMath::Sin(El)) * (F.Len / float(Fine));
					Dense.Add(P);
				}
				for (int32 k = 0; k <= Seg; ++k) Rc.Add(Dense[k * Fine / Seg]);
				for (int32 k = 0; k <= Seg; ++k) Tg.Add((Rc[FMath::Min(Seg, k + 1)] - Rc[FMath::Max(0, k - 1)]).GetSafeNormal());
				for (const float Side : { -1.f, 1.f })
				{
					for (int32 Face = 0; Face < 2; ++Face)
					{
						TArray<int32> R, Pk, Vl;
						for (int32 k = 0; k <= Seg; ++k)
						{
							const FVector Sd = FVector::CrossProduct(Tg[k], FVector::UpVector).GetSafeNormal().RotateAngleAxis(F.Roll, Tg[k]);
							const FVector Up = FVector::CrossProduct(Sd, Tg[k]).GetSafeNormal() * (FVector::DotProduct(FVector::CrossProduct(Sd, Tg[k]), FVector::UpVector) < 0.f ? -1.f : 1.f);
							const FVector Nf = (Face == 0 ? Up : -Up) * 0.6f + FVector::UpVector * 0.4f;
							const float S = float(k) / float(Seg);
							R.Add(G.V(SLeaf, Rc[k], Nf, Tg[k], S, 0.5f, FMath::Lerp(Stem, Hi, S * 0.5f), 0.7f));
							if (k < Seg)
							{
								const float Sm = (float(k) + 0.5f) / float(Seg);
								const FVector Mid = FMath::Lerp(Rc[k], Rc[k + 1], 0.5f);
								const float W = F.Width * FMath::Pow(FMath::Sin(PI * FMath::Min(1.f, Sm * 1.1f)), 0.6f) * (1.f - 0.3f * Sm);
								// A pinna: forward-swept point; then the notch between pinnae.
								Pk.Add(G.V(SLeaf, Mid + Sd * (Side * W) + Tg[k] * (W * 0.35f) - FVector(0.f, 0.f, W * 0.15f), Nf, Tg[k], Sm, Side > 0.f ? 1.f : 0.f, FMath::Lerp(Lo, Hi, Sm), 0.72f));
								const FVector Nx = Rc[k + 1];
								Vl.Add(G.V(SLeaf, Nx + Sd * (Side * W * 0.3f), Nf, Tg[k], S, 0.5f, FMath::Lerp(Lo, Hi, S) * 0.9f, 0.72f));
							}
						}
						const FVector Facing = (Face == 0 ? 1.f : -1.f) * FVector::UpVector;
						for (int32 k = 0; k < Seg; ++k)
						{
							const int32 Prev = k == 0 ? R[0] : Vl[k - 1];
							G.Tri(SLeaf, R[k], Prev, Pk[k], Facing);
							G.Tri(SLeaf, R[k], Pk[k], R[k + 1], Facing);
							G.Tri(SLeaf, R[k + 1], Pk[k], Vl[k], Facing);
						}
					}
				}
			}
		});
	}

	static void Bramble(FSkel& K, FRng& Rs)
	{
		K.Barks.Add(MakeBark(0x5A2A22, 0x3A1812, 0x4E5A2A, 0.55f, 0.1f, 0.f, 0.f, 2.f, 0.1f));
		K.LeafShape = LeafTrefoil;
		K.LeafLo = Hex(0x1E3418);
		K.LeafHi = Hex(0x3A5A26);
		K.LeafRough = 0.55f;
		struct FThorn { FVector At, Out, Back; float Len, Rad; };
		TArray<FThorn> Thorns;
		const int32 N = Rs.I(6, 8);
		const float A0 = Rs.R(0.f, 2.f * PI);
		for (int32 c = 0; c < N; ++c)
		{
			const float A = A0 + 2.f * PI * float(c) / float(N) + Rs.R(-0.3f, 0.3f);
			const FVector Dir(FMath::Cos(A), FMath::Sin(A), 0.f);
			const FVector Side(-Dir.Y, Dir.X, 0.f);
			const float Reach = Rs.R(150.f, 300.f), Hc = Rs.R(110.f, 180.f), Wv = Rs.R(-0.3f, 0.3f);
			FLimb Cn;
			Cn.Level = 0;
			Cn.Bark = 0;
			Cn.Sides = 4.f;
			Cn.Cap = 1;
			for (int32 k = 0; k <= 8; ++k)
			{
				const float S = float(k) / 8.f;
				const float Z = Hc * FMath::Pow(FMath::Sin(PI * FMath::Min(S * 0.98f, 0.98f)), 0.8f) * (1.f - 0.15f * S) + 3.f;
				Cn.Pts.Add(Dir * (12.f + Reach * S) + Side * (Wv * Reach * S * (1.f - S)) + FVector(0.f, 0.f, Z));
				Cn.Rad.Add(FMath::Lerp(1.7f, 0.5f, S));
			}
			// Thorns down the cane, pointing back; leaves (three leaflets) on the upper half.
			for (int32 t = 0; t < 22; ++t)
			{
				const float S = (float(t) + 0.5f) / 22.f;
				float Rr = 0.f;
				const FVector P = PolyAt(Cn.Pts, Cn.Rad, S, &Rr);
				const FVector Tg = (PolyAt(Cn.Pts, Cn.Rad, FMath::Min(1.f, S + 0.02f)) - PolyAt(Cn.Pts, Cn.Rad, FMath::Max(0.f, S - 0.02f))).GetSafeNormal();
				const float Ang = float(t) * 2.4f;
				FVector U, V;
				Frame(Tg, U, V);
				const FVector Out = U * FMath::Cos(Ang) + V * FMath::Sin(Ang);
				Thorns.Add({ P + Out * Rr, Out, -Tg, Rs.R(0.9f, 1.6f), Rr * 0.6f });
				if (t % 3 == 1 && S > 0.25f)
				{
					FCluster Cl;
					Cl.C = P + Out * 9.f + FVector(0.f, 0.f, 4.f);
					Cl.Out = (Out + FVector::UpVector).GetSafeNormal();
					Cl.Up = (FVector::UpVector + Out * 0.6f).GetSafeNormal();
					Cl.Size = Rs.R(16.f, 24.f);
					Cl.Tone = Rs.R(0.2f, 0.9f);
					Cl.Spin = Rs.R(0.f, 2.f * PI);
					K.Clusters.Add(Cl);
				}
			}
			K.Limbs.Add(Cn);
		}
		const FLinearColor Cane = Hex(0x5A2A22), Tip = Hex(0xE0D8C0);
		K.Extras.Add([Thorns, Cane, Tip](FGeo& G, const FDetail& D)
		{
			if (D.Q < 0.35f) return;
			const int32 Step = D.Q > 0.7f ? 1 : 3;
			for (int32 i = 0; i < Thorns.Num(); i += Step)
			{
				const FThorn& T = Thorns[i];
				FVector U, V;
				Frame(T.Out, U, V);
				const FVector Pt = T.At + (T.Out + T.Back * 0.35f).GetSafeNormal() * T.Len;
				const int32 Tp = G.V(SBark, Pt, T.Out, U, 1.f, 0.5f, Tip, 0.4f);
				int32 B[3];
				for (int32 s = 0; s < 3; ++s)
				{
					const float A = 2.f * PI * float(s) / 3.f;
					B[s] = G.V(SBark, T.At + (U * FMath::Cos(A) + V * FMath::Sin(A)) * T.Rad, U * FMath::Cos(A) + V * FMath::Sin(A), T.Out, 0.f, float(s) / 3.f, Cane, 0.55f);
				}
				for (int32 s = 0; s < 3; ++s)
				{
					const FVector Out = FVector(G.S[SBark].N[B[s]]) + FVector(G.S[SBark].N[B[(s + 1) % 3]]);
					G.Tri(SBark, B[s], B[(s + 1) % 3], Tp, Out);
				}
			}
		});
	}

	/** A mossy boulder: a lumpy stone on a flattened base, moss on its top faces, lichen spots. */
	static void Boulder(FSkel& K, FRng& Rs)
	{
		const float Sx = Rs.R(105.f, 130.f), Sy = Rs.R(90.f, 110.f), Sz = Rs.R(70.f, 85.f), Seed = Rs.R(0.f, 100.f);
		K.Extras.Add([Sx, Sy, Sz, Seed](FGeo& G, const FDetail& D)
		{
			// An icosphere subdivided by detail, displaced by noise.
			TArray<FVector> V = { FVector(-1, 1.618, 0), FVector(1, 1.618, 0), FVector(-1, -1.618, 0), FVector(1, -1.618, 0), FVector(0, -1, 1.618), FVector(0, 1, 1.618),
				FVector(0, -1, -1.618), FVector(0, 1, -1.618), FVector(1.618, 0, -1), FVector(1.618, 0, 1), FVector(-1.618, 0, -1), FVector(-1.618, 0, 1) };
			for (FVector& P : V) P.Normalize();
			TArray<FIntVector> F = { {0,11,5},{0,5,1},{0,1,7},{0,7,10},{0,10,11},{1,5,9},{5,11,4},{11,10,2},{10,7,6},{7,1,8},
				{3,9,4},{3,4,2},{3,2,6},{3,6,8},{3,8,9},{4,9,5},{2,4,11},{6,2,10},{8,6,7},{9,8,1} };
			const int32 Levels = D.Q > 0.7f ? 3 : D.Q > 0.35f ? 2 : 1;
			for (int32 L = 0; L < Levels; ++L)
			{
				TMap<uint64, int32> Mid;
				const auto MidOf = [&V, &Mid](int32 A, int32 B)
				{
					const uint64 Key = (uint64(FMath::Min(A, B)) << 32) | uint64(FMath::Max(A, B));
					if (const int32* Found = Mid.Find(Key)) return *Found;
					const int32 I = V.Add(((V[A] + V[B]) * 0.5f).GetSafeNormal());
					Mid.Add(Key, I);
					return I;
				};
				TArray<FIntVector> Nf;
				for (const FIntVector& T : F)
				{
					const int32 A = MidOf(T.X, T.Y), B = MidOf(T.Y, T.Z), C = MidOf(T.Z, T.X);
					Nf.Add({ T.X, A, C });
					Nf.Add({ T.Y, B, A });
					Nf.Add({ T.Z, C, B });
					Nf.Add({ A, B, C });
				}
				F = Nf;
			}
			TArray<FVector> P;
			for (const FVector& U : V)
			{
				const float Disp = 1.f + 0.2f * (Noise(U * 1.6f + FVector(Seed)) - 0.5f) * 2.f + 0.07f * (Noise(U * 5.f + FVector(Seed + 3.f)) - 0.5f) * 2.f;
				FVector Q(U.X * Sx * Disp, U.Y * Sy * Disp, U.Z * Sz * Disp + Sz * 0.75f);
				if (Q.Z < 0.f) Q.Z *= 0.25f;   // the base sits flat, a little into the ground
				P.Add(Q);
			}
			TArray<FVector> N;
			N.Init(FVector::ZeroVector, P.Num());
			for (const FIntVector& T : F)
			{
				const FVector Fn = FVector::CrossProduct(P[T.Y] - P[T.X], P[T.Z] - P[T.X]);
				N[T.X] += Fn; N[T.Y] += Fn; N[T.Z] += Fn;
			}
			const FVector Ctr(0.f, 0.f, Sz * 0.75f);
			TArray<int32> Ix;
			for (int32 i = 0; i < P.Num(); ++i)
			{
				FVector Nn = N[i].GetSafeNormal();
				if (FVector::DotProduct(Nn, P[i] - Ctr) < 0.f) Nn = -Nn;
				const float MossAmt = FMath::SmoothStep(0.25f, 0.75f, float(Nn.Z)) * FMath::SmoothStep(0.3f, 0.6f, Noise(P[i] * 0.02f + FVector(Seed)));
				FLinearColor C = FMath::Lerp(Hex(0x7A7870), Hex(0x55534D), Noise(P[i] * 0.05f + FVector(Seed * 2.f)));
				if (Hash3(i, 5, int32(Seed)) > 0.93f) C = Hex(0xA8A890);   // lichen spots
				C = FMath::Lerp(C, FMath::Lerp(Hex(0x3A5222), Hex(0x5E8C31), Noise(P[i] * 0.04f)), MossAmt);
				const float A = FMath::Atan2(float(P[i].Y), float(P[i].X));
				Ix.Add(G.V(SBark, P[i], Nn, FVector::CrossProduct(FVector::UpVector, Nn), A * 150.f / 150.f, float(P[i].Z) / 150.f, C, FMath::Lerp(0.85f, 0.96f, MossAmt)));
			}
			for (const FIntVector& T : F) G.Tri(SBark, Ix[T.X], Ix[T.Y], Ix[T.Z], (P[T.X] + P[T.Y] + P[T.Z]) / 3.f - Ctr);
		});
		FWRTreeCollider C;
		C.Radius = Sz * 0.95f;
		C.A = FVector(-Sx + C.Radius, 0.f, Sz * 0.75f);
		C.B = FVector(Sx - C.Radius, 0.f, Sz * 0.75f);
		K.Colliders.Add(C);
		C.A = FVector(0.f, -Sy + C.Radius, Sz * 0.75f);
		C.B = FVector(0.f, Sy - C.Radius, Sz * 0.75f);
		K.Colliders.Add(C);
	}

	// ------------------------------------------------------------------------------------------------ registry

	enum class EBudget : uint8 { Giant, Broad, Shrub, Fern };

	struct FSpeciesInfo { const TCHAR* Name; float H; float R; EBudget Budget; };

	static const FSpeciesInfo GTreeSpecies[] = {
		{ TEXT("oak"), 2200.f, 55.f, EBudget::Broad }, { TEXT("beech"), 2800.f, 45.f, EBudget::Broad }, { TEXT("lime"), 3000.f, 42.f, EBudget::Broad },
		{ TEXT("alder"), 1800.f, 22.f, EBudget::Broad }, { TEXT("ash"), 2600.f, 35.f, EBudget::Broad }, { TEXT("hazel"), 600.f, 6.f, EBudget::Shrub },
		{ TEXT("holly"), 900.f, 14.f, EBudget::Broad }, { TEXT("sapling"), 500.f, 6.f, EBudget::Shrub }, { TEXT("giant"), 8000.f, 300.f, EBudget::Giant },
		{ TEXT("heart"), 14000.f, 450.f, EBudget::Giant }, { TEXT("eldest"), 13000.f, 400.f, EBudget::Giant }, { TEXT("dead_giant"), 6000.f, 220.f, EBudget::Giant },
		{ TEXT("bower_oak"), 7000.f, 200.f, EBudget::Giant }, { TEXT("bell_tree"), 6000.f, 240.f, EBudget::Giant }, { TEXT("strangler"), 6000.f, 650.f, EBudget::Giant },
		{ TEXT("stump"), 100.f, 50.f, EBudget::Shrub }, { TEXT("trail_tree"), 1200.f, 20.f, EBudget::Broad }, { TEXT("fallen"), 6000.f, 200.f, EBudget::Giant },
		{ TEXT("nurse_log"), 5000.f, 180.f, EBudget::Giant }, { TEXT("fern"), 120.f, 40.f, EBudget::Fern }, { TEXT("bramble"), 180.f, 60.f, EBudget::Shrub },
		{ TEXT("foxfire_log"), 800.f, 40.f, EBudget::Shrub }, { TEXT("root_arch"), 1200.f, 60.f, EBudget::Shrub }, { TEXT("root_mat"), 3000.f, 40.f, EBudget::Giant },
		{ TEXT("buttress_root"), 600.f, 80.f, EBudget::Shrub }, { TEXT("hermit_oak"), 2800.f, 120.f, EBudget::Giant }, { TEXT("boulder"), 150.f, 110.f, EBudget::Shrub },
	};

	static const FSpeciesInfo& InfoFor(FName Species)
	{
		FString S = Species.ToString().ToLower();
		// forest.json tree kinds drawn as a kit species.
		if (S == TEXT("hanging_oak")) S = TEXT("oak");
		else if (S == TEXT("paper_oak") || S == TEXT("treant") || S == TEXT("dead")) S = TEXT("dead_giant");
		else if (S == TEXT("ringfort_beech")) S = TEXT("beech");
		else if (S == TEXT("mossy_boulder") || S == TEXT("rock")) S = TEXT("boulder");
		for (const FSpeciesInfo& I : GTreeSpecies)
		{
			if (S == I.Name) return I;
		}
		static TSet<FString> Warned;
		if (!Warned.Contains(S))
		{
			Warned.Add(S);
			UE_LOG(LogWildreach, Warning, TEXT("TREEKIT unknown species '%s': drawn as an oak"), *S);
		}
		return GTreeSpecies[0];
	}

	static void BuildSkel(FSkel& K, const FSpeciesInfo& Info, int32 Variant)
	{
		FRng Rs(int32(FCrc::StrCrc32(Info.Name) & 0x7fffffffu) ^ (Variant * 7919 + 17));
		const FString S = Info.Name;
		FTreeP P;
		P.H = Info.H * Rs.R(0.95f, 1.05f);
		P.R = Info.R;
		const auto Leaves = [&K](int32 Shape, uint32 Lo, uint32 Hi, float Rough) { K.LeafShape = Shape; K.LeafLo = Hex(Lo); K.LeafHi = Hex(Hi); K.LeafRough = Rough; };
		if (S == TEXT("oak") || S == TEXT("hermit_oak") || S == TEXT("bower_oak"))
		{
			K.Barks.Add(MakeBark(0x5E4E3E, 0x2E241C, 0x4E6B2A, 0.92f, 0.65f, 120.f, 0.12f, 14.f, 0.55f));
			Leaves(LeafOak, 0x2F4A1C, 0x6A8A30, 0.7f);
			P.BoleTop = 0.26f; P.Forks = 3; P.ForkSpread = 0.5f; P.CrownBase = 0.24f; P.CrownTop = 1.f; P.CrownR = 0.42f; P.Shape = 1;
			P.Branches = 9; P.BranchAngle = 66.f; P.Arch = 0.12f; P.TipDrop = -0.02f; P.Kink = 0.1f; P.Secondaries = 3;
			P.Flare = 1.35f; P.FlareH = 0.07f; P.Roots = 4; P.TrunkSides = 14.f; P.LimbSides = 7.f; P.Rings = 6; P.LeafSize = 260.f; P.Fill = 110;
			P.Lean = Rs.R(0.f, 3.f);
			if (S == TEXT("hermit_oak"))
			{
				// Forks at 12 m into two great leaders; the house sits in the fork (WRForestWorld).
				P.BoleTop = 1200.f / P.H; P.Forks = 2; P.ForkSpread = 0.55f; P.CrownBase = 0.4f; P.CrownR = 0.46f; P.Branches = 12; P.Roots = 5;
				P.Flare = 1.45f; P.FlareH = 0.05f; P.LeafSize = 320.f; P.Fill = 220; P.TrunkSides = 20.f; P.Lean = 0.f;
				K.Anchors.Add(TEXT("fork"), FVector(0.f, 0.f, 1200.f));
			}
			else if (S == TEXT("bower_oak"))
			{
				// A hollow old oak 70 m: an open hollow at its foot, stout limbs where the walkways socket in.
				P.BoleTop = 0.3f; P.Forks = 4; P.CrownBase = 0.28f; P.CrownR = 0.36f; P.Branches = 16; P.Roots = 6; P.Flare = 1.6f; P.FlareH = 450.f / P.H;
				P.LeafSize = 650.f; P.Fill = 240; P.TrunkSides = 24.f; P.LimbSides = 8.f; P.Lean = 0.f;
				P.CrackHalf = FMath::DegreesToRadians(11.f); P.CrackTop = 420.f; P.CrackCore = P.R * 0.45f;
				K.Barks[0].MossPatch = 0.4f; K.Barks[0].MossBand = 600.f;
			}
			Standing(K, P, Rs, 0);
		}
		else if (S == TEXT("beech") || S == TEXT("sapling"))
		{
			K.Barks.Add(MakeBark(0xA8A49A, 0x7E7A70, 0x5E7A35, 0.55f, 0.4f, 60.f, 0.05f, 3.f, 0.15f));
			Leaves(LeafBeech, 0x3E5E22, 0x8AAA40, 0.65f);
			P.BoleTop = 0.38f; P.LeaderTop = 0.82f; P.CrownBase = 0.3f; P.CrownR = 0.3f; P.Shape = 0;
			P.Branches = 14; P.BranchAngle = 50.f; P.Arch = 0.1f; P.TipDrop = -0.12f; P.Kink = 0.02f; P.Secondaries = 3;
			P.Flare = 1.3f; P.FlareH = 0.05f; P.Roots = 4; P.TrunkSides = 12.f; P.LimbSides = 6.f; P.Rings = 10; P.LeafSize = 240.f; P.Fill = 120;
			if (S == TEXT("sapling"))
			{
				P.BoleTop = 0.3f; P.LeaderTop = 0.95f; P.CrownR = 0.24f; P.Branches = 7; P.Secondaries = 1; P.Flare = 1.1f; P.Roots = 0;
				P.TrunkSides = 7.f; P.LimbSides = 4.f; P.TwigSides = 3.f; P.Rings = 5; P.LeafSize = 110.f; P.Fill = 34; P.Lean = Rs.R(0.f, 6.f);
				K.Barks[0].MossBand = 0.f;
			}
			Standing(K, P, Rs, 0);
		}
		else if (S == TEXT("lime"))
		{
			K.Barks.Add(MakeBark(0x6E655A, 0x3E372E, 0x4E6B2A, 0.88f, 0.5f, 80.f, 0.08f, 18.f, 0.45f));
			Leaves(LeafLime, 0x4A6A24, 0xA8C060, 0.7f);
			P.BoleTop = 0.4f; P.LeaderTop = 0.88f; P.CrownBase = 0.32f; P.CrownR = 0.22f; P.Shape = 2;
			P.Branches = 16; P.BranchAngle = 45.f; P.Arch = 0.06f; P.TipDrop = -0.18f; P.Secondaries = 2;
			P.Flare = 1.35f; P.FlareH = 0.05f; P.Roots = 3; P.TrunkSides = 12.f; P.Rings = 10; P.LeafSize = 230.f; P.Fill = 120;
			Standing(K, P, Rs, 0);
			// The burr sprouts round a lime's foot (epicormic shoots), the last sprays drawn.
			for (int32 i = 0; i < 6; ++i)
			{
				const float A = Rs.R(0.f, 2.f * PI);
				FCluster Cl;
				Cl.C = FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * (P.R * 1.4f) + FVector(0.f, 0.f, Rs.R(30.f, 150.f));
				Cl.Out = FVector(FMath::Cos(A), FMath::Sin(A), 0.2f).GetSafeNormal();
				Cl.Size = Rs.R(70.f, 110.f);
				Cl.Tone = 0.6f;
				Cl.Spin = Rs.R(0.f, 2.f * PI);
				K.Clusters.Add(Cl);
			}
		}
		else if (S == TEXT("alder"))
		{
			K.Barks.Add(MakeBark(0x7A7468, 0x4A453D, 0x4E6B2A, 0.85f, 0.5f, 100.f, 0.1f, 10.f, 0.4f));
			Leaves(LeafAlder, 0x263F1A, 0x4E6E2E, 0.68f);
			P.BoleTop = 0.2f; P.LeaderTop = 0.92f; P.CrownBase = 0.2f; P.CrownR = 0.2f; P.Shape = 3;
			P.Branches = 14; P.BranchAngle = 62.f; P.Arch = 0.04f; P.Secondaries = 2; P.Flare = 1.25f; P.FlareH = 0.04f; P.Roots = 3;
			P.TrunkSides = 10.f; P.LimbSides = 5.f; P.Rings = 10; P.LeafSize = 180.f; P.Fill = 110;
			// Two of the four lean out over the water (DC 2.2: "willow-like alder lean over water").
			// Along +X, so the dressing can turn them to face the water (forest_alder_lean in WRWorldDressing).
			P.Lean = Variant >= 2 ? Rs.R(8.f, 14.f) : Rs.R(0.f, 3.f);
			P.bLeanX = Variant >= 2;
			Standing(K, P, Rs, 0);
		}
		else if (S == TEXT("ash"))
		{
			K.Barks.Add(MakeBark(0x8A8478, 0x5A554C, 0x5E7A35, 0.85f, 0.45f, 80.f, 0.06f, 16.f, 0.5f));
			Leaves(LeafAsh, 0x3A5A26, 0x7A9A48, 0.7f);
			P.BoleTop = 0.42f; P.LeaderTop = 0.8f; P.CrownBase = 0.38f; P.CrownR = 0.27f; P.Shape = 4;
			P.Branches = 10; P.BranchAngle = 40.f; P.Arch = 0.15f; P.TipDrop = 0.12f; P.Secondaries = 3; P.Flare = 1.3f; P.FlareH = 0.05f; P.Roots = 3;
			P.TrunkSides = 12.f; P.Rings = 10; P.LeafSize = 230.f; P.Fill = 90;
			Standing(K, P, Rs, 0);
		}
		else if (S == TEXT("holly"))
		{
			K.Barks.Add(MakeBark(0x8A877A, 0x6A675C, 0x4E6B2A, 0.6f, 0.3f, 40.f, 0.f, 4.f, 0.1f));
			Leaves(LeafHolly, 0x142A12, 0x2A4A22, 0.35f);
			P.BoleTop = 0.12f; P.LeaderTop = 0.9f; P.CrownBase = 0.1f; P.CrownR = 0.24f; P.Shape = 3;
			P.Branches = 16; P.BranchAngle = 60.f; P.Arch = 0.05f; P.Secondaries = 2; P.Flare = 1.15f; P.FlareH = 0.04f; P.Roots = 0;
			P.TrunkSides = 9.f; P.LimbSides = 5.f; P.TwigSides = 3.f; P.Rings = 8; P.LeafSize = 120.f; P.Fill = 140;
			Standing(K, P, Rs, 0);
			// Berries: tight clusters of red drupes among the leaves.
			TArray<FVector> Berries;
			for (int32 i = 0; i < K.Clusters.Num(); i += 4) Berries.Add(K.Clusters[i].C + FVector(Rs.R(-15.f, 15.f), Rs.R(-15.f, 15.f), -10.f));
			K.Extras.Add([Berries](FGeo& G, const FDetail& D)
			{
				if (D.Q < 0.7f) return;
				for (int32 b = 0; b < Berries.Num(); ++b)
				{
					for (int32 k = 0; k < 3; ++k)
					{
						const FVector C = Berries[b] + FVector(Hash3(b, k, 1) - 0.5f, Hash3(b, k, 2) - 0.5f, Hash3(b, k, 3) - 0.5f) * 6.f;
						const float R = 0.9f;
						const FVector Ax[6] = { FVector(R, 0, 0), FVector(-R, 0, 0), FVector(0, R, 0), FVector(0, -R, 0), FVector(0, 0, R), FVector(0, 0, -R) };
						int32 I[6];
						for (int32 q = 0; q < 6; ++q) I[q] = G.V(SLeaf, C + Ax[q], Ax[q], FVector::XAxisVector, 0.f, 0.f, Hex(0xB01E1E), 0.3f);
						const int32 Faces[8][3] = { {0,2,4},{2,1,4},{1,3,4},{3,0,4},{2,0,5},{1,2,5},{3,1,5},{0,3,5} };
						for (const auto& Fc : Faces) G.Tri(SLeaf, I[Fc[0]], I[Fc[1]], I[Fc[2]], Ax[Fc[0]] + Ax[Fc[1]] + Ax[Fc[2]]);
					}
				}
			});
		}
		else if (S == TEXT("hazel"))
		{
			// A coppice stool: rods from one root crown, a broad round crown of soft round leaves.
			K.Barks.Add(MakeBark(0x7A5A40, 0x4A3628, 0x4E6B2A, 0.6f, 0.3f, 40.f, 0.f, 3.f, 0.15f));
			Leaves(LeafHazel, 0x4E6E2A, 0x7E9E3E, 0.7f);
			const int32 N = Rs.I(7, 9);
			TArray<FVector> Pool;
			for (int32 i = 0; i < N; ++i)
			{
				const float A = 2.f * PI * float(i) / float(N) + Rs.R(-0.3f, 0.3f);
				const FVector Out(FMath::Cos(A), FMath::Sin(A), 0.f);
				const float Tilt = FMath::DegreesToRadians(Rs.R(8.f, 22.f));
				const float Hh = P.H * Rs.R(0.75f, 1.f);
				FLimb Rod;
				Rod.Level = 0;
				Rod.Bark = 0;
				Rod.Sides = 5.f;
				for (int32 k = 0; k <= 5; ++k)
				{
					const float Sk = float(k) / 5.f;
					Rod.Pts.Add(Out * (Rs.R(4.f, 25.f) * (k == 0 ? 1.f : 0.f) + FMath::Sin(Tilt) * Hh * Sk * (0.8f + 0.4f * Sk)) + FVector(0.f, 0.f, -20.f + Hh * Sk * FMath::Cos(Tilt)));
					Rod.Rad.Add(FMath::Lerp(5.f, 1.f, Sk));
				}
				for (int32 k = 2; k <= 5; ++k) Pool.Add(Rod.Pts[k]);
				for (int32 b = 0; b < 3; ++b)
				{
					float R0 = 0.f;
					const FVector St = PolyAt(Rod.Pts, Rod.Rad, Rs.R(0.4f, 0.85f), &R0);
					const FVector D2 = (Out.RotateAngleAxis(Rs.R(-60.f, 60.f), FVector::UpVector) + FVector(0.f, 0.f, Rs.R(0.2f, 0.8f))).GetSafeNormal();
					FLimb Tw;
					Tw.Level = 2;
					Tw.Bark = 0;
					Tw.Sides = 3.f;
					Tw.Pts = { St, St + D2 * Rs.R(60.f, 120.f) };
					Tw.Rad = { R0 * 0.6f, 0.5f };
					K.Limbs.Add(Tw);
					Pool.Add(Tw.Pts[1]);
				}
				K.Limbs.Add(Rod);
			}
			for (int32 j = 0; j < 60; ++j)
			{
				const float Tt = FMath::Frac(0.5f + float(j) * 0.618034f);
				const float Phi = float(j) * 2.399963f;
				const float Rr = P.H * 0.45f * Env(0, Tt) * (0.65f + 0.35f * FMath::Frac(0.3f + float(j) * 0.7548777f));
				FVector Q(FMath::Cos(Phi) * Rr, FMath::Sin(Phi) * Rr, FMath::Lerp(P.H * 0.25f, P.H, Tt));
				float Best = TNumericLimits<float>::Max();
				FVector Near = Pool[0];
				for (const FVector& W : Pool) { const float D = float(FVector::DistSquared(W, Q)); if (D < Best) { Best = D; Near = W; } }
				if (FMath::Sqrt(Best) > 220.f) Q = Near + (Q - Near).GetSafeNormal() * 220.f;
				FCluster Cl;
				Cl.C = Q;
				Cl.Out = (FVector(FMath::Cos(Phi), FMath::Sin(Phi), 0.f) + FVector(0.f, 0.f, Tt - 0.3f)).GetSafeNormal();
				Cl.Size = Rs.R(130.f, 180.f);
				Cl.Tone = FMath::Clamp(0.2f + 0.7f * Tt + Rs.R(-0.1f, 0.1f), 0.f, 1.f);
				Cl.Spin = Rs.R(0.f, 2.f * PI);
				K.Clusters.Add(Cl);
			}
		}
		else if (S == TEXT("giant") || S == TEXT("heart") || S == TEXT("eldest"))
		{
			const bool bHeart = S == TEXT("heart"), bEldest = S == TEXT("eldest");
			K.Barks.Add(bHeart ? MakeBark(0x5A4A3A, 0x2A2018, 0x3A5222, 0.92f, 0.8f, 1800.f, 0.45f, 26.f, 0.6f)
				: bEldest ? MakeBark(0x4A4038, 0x221C16, 0x3A5222, 0.94f, 0.85f, 2000.f, 0.6f, 24.f, 0.65f)
				: MakeBark(0x4A3A2A, 0x261C14, 0x4E6B2A, 0.92f, 0.8f, 1500.f, 0.45f, 22.f, 0.6f));
			if (bHeart) Leaves(LeafGiant, 0x2E4A20, 0x8A9A40, 0.68f);
			else if (bEldest) Leaves(LeafGiant, 0x22381A, 0x4A6E32, 0.7f);
			else Leaves(LeafGiant, 0x22381A, 0x4E7A3A, 0.7f);
			P.BoleTop = 0.58f; P.LeaderTop = 0.94f; P.CrownBase = 0.56f; P.CrownR = 0.2f; P.Shape = 5;
			P.Branches = 18; P.BranchAngle = 62.f; P.Arch = 0.18f; P.TipDrop = 0.06f; P.Kink = 0.05f; P.BranchR = 0.3f; P.Secondaries = 3;
			P.Flare = 1.8f; P.FlareH = 600.f / P.H; P.Fins = Rs.I(5, 7); P.TrunkSides = 30.f; P.LimbSides = 9.f; P.TwigSides = 6.f; P.Rings = 22;
			P.LeafSize = 900.f; P.Fill = 300; P.PadUp = 0.55f;
			if (bHeart)
			{
				P.Fins = 7; P.FlareH = 900.f / P.H; P.CrownBase = 0.62f; P.Branches = 22; P.LeafSize = 1200.f;
			}
			if (bEldest)
			{
				// Snapped at 110 m by some old blow; three tops grown again beside the break; the Door at +X.
				P.Broken = 11000.f / P.H; P.LeaderTop = P.Broken; P.Jag = 700.f; P.Reiterate = 3; P.Fins = 6; P.FlareH = 700.f / P.H;
				P.CrownBase = 0.52f; P.CrownTop = 0.98f; P.CrownR = 0.16f; P.Branches = 12; P.LeafSize = 1000.f; P.Fill = 200;
				P.CrackHalf = FMath::DegreesToRadians(14.f); P.CrackTop = 650.f; P.CrackCore = P.R * 0.95f;
				// The fins stay clear of the Door.
			}
			Standing(K, P, Rs, 0);
			FTrunk& T = K.Trunks[0];
			if (bEldest)
			{
				for (float& A : T.Fins) if (FMath::Abs(FMath::FindDeltaAngleRadians(0.f, A)) < 0.6f) A = FMath::UnwindRadians(A + (A >= 0.f ? 0.6f : -0.6f));
				// The Door: the crack's floor at its back (players stand here; the hazards track takes them down).
				K.Anchors.Add(TEXT("door"), FVector(T.CrackCore + 60.f, 0.f, 0.f));
				// Its ring of collision leaves the Door open: replace the bole/flare capsules below the crack's top.
				K.Colliders.RemoveAll([&T](const FWRTreeCollider& C) { return C.A.Z < 1.f && C.Radius > T.Rad[0] * 0.8f; });
				const float Rb = TrunkRadiusAt(T, 150.f, PI);
				for (int32 c = 0; c < 10; ++c)
				{
					const float A = FMath::DegreesToRadians(36.f * float(c) + 18.f);
					if (FMath::Abs(FMath::FindDeltaAngleRadians(0.f, A)) < T.CrackHalf + 0.25f) continue;
					FWRTreeCollider C;
					const FVector Dir(FMath::Cos(A), FMath::Sin(A), 0.f);
					C.Radius = Rb * 0.32f;
					C.A = Dir * (Rb * 0.7f);
					C.B = Dir * (P.R * 0.75f) + FVector(0.f, 0.f, P.CrackTop * 1.1f);
					K.Colliders.Add(C);
				}
				FWRTreeCollider Core;
				Core.Radius = T.CrackCore * 0.92f;
				Core.A = FVector(0.f, 0.f, 0.f);
				Core.B = FVector(0.f, 0.f, P.CrackTop * 1.1f);
				K.Colliders.Add(Core);
				FWRTreeCollider Bole;
				Bole.Radius = P.R * 0.95f;
				Bole.A = FVector(0.f, 0.f, P.CrackTop * 1.1f + Bole.Radius);
				Bole.B = FVector(0.f, 0.f, P.CrownBase * P.H);
				K.Colliders.Add(Bole);
				// Bracket fungus: an 8 m shelf spiralling round the bole at 6-9 m.
				AddShelves(K, T, Rs, 16, 600.f, 900.f, 110.f, 260.f);
				// Foxfire in the cracks at its base, and down both edges of the Door.
				for (int32 c = 0; c < 9; ++c)
				{
					float A = Rs.R(0.f, 2.f * PI);
					if (FMath::Abs(FMath::FindDeltaAngleRadians(0.f, A)) < T.CrackHalf * 1.5f) A += PI;
					FStrip St;
					St.Col = Hex(0x7FE07A);
					St.Width = Rs.R(12.f, 26.f);
					const float Top = Rs.R(200.f, 480.f);
					for (int32 k = 0; k <= 10; ++k)
					{
						const float Z = Top * float(k) / 10.f;
						const float Aa = FMath::UnwindRadians(A + 0.04f * FMath::Sin(float(k) * 1.7f + float(c)));
						const FVector Dir(FMath::Cos(Aa), FMath::Sin(Aa), 0.f);
						St.Pts.Add(Dir * TrunkRadiusAt(T, Z, Aa) * 1.01f + FVector(0.f, 0.f, Z));
						St.Nrm.Add(Dir);
					}
					K.Strips.Add(St);
				}
				for (const float Sgn : { -1.f, 1.f })
				{
					FStrip St;
					St.Col = Hex(0x7FE07A);
					St.Width = 14.f;
					const float A = Sgn * (T.CrackHalf + 0.03f);
					const FVector Dir(FMath::Cos(A), FMath::Sin(A), 0.f);
					for (int32 k = 0; k <= 8; ++k)
					{
						const float Z = P.CrackTop * 0.9f * float(k) / 8.f;
						St.Pts.Add(Dir * TrunkRadiusAt(T, Z, A) * 1.01f + FVector(0.f, 0.f, Z));
						St.Nrm.Add(Dir);
					}
					K.Strips.Add(St);
				}
			}
			if (bHeart)
			{
				SapVeins(K, T, Rs, 9, P.H * 0.72f, Hex(0xD08A1E), P.R * 0.07f);
			}
		}
		else if (S == TEXT("dead_giant") || S == TEXT("bell_tree"))
		{
			const bool bBell = S == TEXT("bell_tree");
			K.Barks.Add(MakeBark(0x8A8275, 0x4E473E, 0x5A6B3A, 0.85f, 0.5f, 300.f, 0.25f, 18.f, 0.55f));
			P.bDead = true;
			P.BoleTop = 0.55f; P.LeaderTop = 0.88f; P.Broken = bBell ? 0.92f : 0.86f; P.Jag = 300.f;
			P.CrownBase = 0.45f; P.CrownR = 0.18f; P.Shape = 5; P.Branches = bBell ? 6 : 9; P.BranchAngle = 55.f; P.BranchR = 0.3f;
			P.Flare = 1.6f; P.FlareH = 500.f / P.H; P.Fins = 5; P.TrunkSides = 24.f; P.LimbSides = 8.f; P.Rings = 16;
			Standing(K, P, Rs, 0);
			if (bBell)
			{
				// The limb at 40 m the Grovekin hung their bell from: out along +X, its end turned up a little.
				FLimb L;
				L.Level = 0;
				L.Bark = 0;
				L.Sides = 10.f;
				L.Cap = 2;
				const float Z = 4000.f;
				const float Rt = TrunkRadiusAt(K.Trunks[0], Z, 0.f);
				L.Pts = { FVector(Rt * 0.3f, 0.f, Z - 120.f), FVector(Rt + 150.f, 0.f, Z), FVector(Rt + 480.f, 0.f, Z + 30.f), FVector(Rt + 820.f, 0.f, Z + 140.f) };
				L.Rad = { 90.f, 70.f, 55.f, 42.f };
				K.Limbs.Add(L);
				FWRTreeCollider C;
				C.A = L.Pts[0];
				C.B = L.Pts.Last();
				C.Radius = 60.f;
				K.Colliders.Add(C);
				K.Anchors.Add(TEXT("bell"), FVector(Rt + 560.f, 0.f, Z + 40.f - 58.f));
				// Crystal veins grown through its roots, violet (the Crystal border).
				AddCrystals(K, Rs, FVector::ZeroVector, P.R * 1.4f, P.R * 3.2f, 14, Hex(0x9A6BE0));
			}
		}
		else if (S == TEXT("strangler"))
		{
			Strangler(K, Rs);
		}
		else if (S == TEXT("stump"))
		{
			// A felled tree's stump (the newcomers' clear-cut): a slanted axe cut showing growth rings, a little flare.
			K.Barks.Add(MakeBark(0x5E4E3E, 0x2E241C, 0x4E6B2A, 0.9f, 0.5f, 40.f, 0.1f, 14.f, 0.5f));
			FLimb St;
			St.Level = 0;
			St.Bark = 0;
			St.Sides = 14.f;
			St.Cap = 4;
			const float Tilt = Rs.R(-6.f, 6.f);
			St.Pts = { FVector(0.f, 0.f, -30.f), FVector(0.f, 0.f, 0.f), FVector(0.f, 0.f, 15.f), FVector(0.f, 0.f, 45.f), FVector(Tilt, 0.f, 100.f) };
			St.Rad = { 70.f, 66.f, 58.f, 52.f, 50.f };
			K.Limbs.Add(St);
			for (int32 r = 0; r < 4; ++r)
			{
				const float A = 2.f * PI * float(r) / 4.f + Rs.R(-0.4f, 0.4f);
				const FVector Dir(FMath::Cos(A), FMath::Sin(A), 0.f);
				FLimb Rt;
				Rt.Level = 3;
				Rt.Bark = 0;
				Rt.Sides = 6.f;
				Rt.Pts = { Dir * 40.f + FVector(0.f, 0.f, 20.f), Dir * 85.f + FVector(0.f, 0.f, 5.f), Dir * 140.f + FVector(0.f, 0.f, -12.f) };
				Rt.Rad = { 22.f, 14.f, 5.f };
				K.Limbs.Add(Rt);
			}
			FWRTreeCollider C;
			C.A = FVector::ZeroVector;
			C.B = FVector(0.f, 0.f, 60.f);
			C.Radius = 52.f;
			K.Colliders.Add(C);
		}
		else if (S == TEXT("trail_tree"))
		{
			// A trail marker tree: bent as a sapling to grow along the ground at knee height, then up again. Its knee
			// points the way (+X).
			K.Barks.Add(MakeBark(0x5E4E3E, 0x2E241C, 0x4E6B2A, 0.88f, 0.8f, 60.f, 0.1f, 12.f, 0.45f));
			Leaves(LeafOak, 0x2F4A1C, 0x6A8A30, 0.7f);
			FLimb Tr;
			Tr.Level = 0;
			Tr.Bark = 0;
			Tr.Sides = 10.f;
			Tr.Pts = { FVector(0, 0, -40), FVector(0, 0, 25), FVector(18, 0, 50), FVector(55, 0, 58), FVector(110, 0, 55), FVector(150, 0, 52), FVector(172, 0, 62),
				FVector(184, 0, 100), FVector(190, 0, 220), FVector(194, 0, 450), FVector(198, 0, 750), FVector(200, 0, 1050) };
			Tr.Rad = { 24.f, 22.f, 21.f, 20.f, 20.f, 21.f, 24.f, 20.f, 18.f, 15.f, 10.f, 4.f };
			K.Limbs.Add(Tr);
			TArray<FVector> Pool;
			for (int32 j = 0; j < 8; ++j)
			{
				const float Z = FMath::Lerp(500.f, 1050.f, (float(j) + 0.5f) / 8.f);
				const float Phi = float(j) * 2.39996f + Rs.R(0.f, 0.4f);
				const FVector A(196.f, 0.f, Z);
				const FVector E = A + FVector(FMath::Cos(Phi), FMath::Sin(Phi), 0.5f).GetSafeNormal() * Rs.R(180.f, 330.f) * (1.1f - (Z - 500.f) / 700.f);
				FLimb B;
				B.Level = 1;
				B.Bark = 0;
				B.Sides = 5.f;
				B.Pts = { A, (A + E) * 0.5f + FVector(0.f, 0.f, 25.f), E };
				B.Rad = { 7.f, 4.f, 1.5f };
				K.Limbs.Add(B);
				Pool.Add(E);
				Pool.Add(B.Pts[1]);
			}
			for (int32 j = 0; j < 44; ++j)
			{
				const FVector& Near = Pool[j % Pool.Num()];
				FCluster Cl;
				Cl.C = Near + FVector(Hash3(j, 1, 9) - 0.5f, Hash3(j, 2, 9) - 0.5f, Hash3(j, 3, 9) - 0.3f) * 200.f;
				Cl.Out = (Cl.C - FVector(196.f, 0.f, Cl.C.Z)).GetSafeNormal();
				Cl.Size = Rs.R(150.f, 210.f);
				Cl.Tone = FMath::Clamp(float(Cl.C.Z - 450.0) / 700.f, 0.f, 1.f);
				Cl.Spin = Rs.R(0.f, 2.f * PI);
				K.Clusters.Add(Cl);
			}
			FWRTreeCollider C;
			C.Radius = 22.f;
			C.A = FVector(0, 0, 0); C.B = FVector(0, 0, 40); K.Colliders.Add(C);
			C.A = FVector(20, 0, 52); C.B = FVector(165, 0, 55); K.Colliders.Add(C);
			C.A = FVector(184, 0, 80); C.B = FVector(196, 0, 600); C.Radius = 18.f; K.Colliders.Add(C);
			K.Anchors.Add(TEXT("points"), FVector(172.f, 0.f, 58.f));
		}
		else if (S == TEXT("fallen") || S == TEXT("nurse_log"))
		{
			FallenLog(K, Rs, Info.H, Info.R, S == TEXT("nurse_log"));
		}
		else if (S == TEXT("foxfire_log"))
		{
			// A rotten log glowing with foxfire (honey fungus) along its cracks; both ends broken and soft.
			K.Barks.Add(MakeBark(0x3A3026, 0x1E1812, 0x4E6B2A, 0.95f, 0.95f, 0.f, 0.5f, 8.f, 0.6f));
			FLimb Lg;
			Lg.Level = 0;
			Lg.Bark = 0;
			Lg.Sides = 12.f;
			Lg.Cap = 2;
			Lg.CapStart = 2;
			const float Sag = Rs.R(-20.f, 20.f);
			for (int32 k = 0; k <= 8; ++k)
			{
				const float S2 = float(k) / 8.f;
				Lg.Pts.Add(FVector(800.f * S2, FMath::Sin(PI * S2) * Sag, 32.f + 3.f * FMath::Sin(S2 * 9.f)));
				Lg.Rad.Add(40.f * (1.f - 0.15f * S2) * (0.9f + 0.2f * Hash3(k, 4, Variant)));
			}
			K.Limbs.Add(Lg);
			for (int32 s = 0; s < 6; ++s)
			{
				FStrip St;
				St.Col = Hex(0x7FE07A);
				St.Width = Rs.R(5.f, 10.f);
				const float A = Rs.R(-1.9f, 1.9f);   // up the sides and over the back, not underneath
				const float X0 = Rs.R(40.f, 400.f), X1 = X0 + Rs.R(150.f, 380.f);
				for (int32 k = 0; k <= 8; ++k)
				{
					const float X = FMath::Lerp(X0, FMath::Min(X1, 780.f), float(k) / 8.f);
					const float Aa = A + 0.15f * FMath::Sin(X * 0.03f + float(s));
					float Rr = 0.f;
					const FVector C = PolyAt(Lg.Pts, Lg.Rad, X / 800.f, &Rr);
					const FVector Dir(0.f, FMath::Sin(Aa), FMath::Cos(Aa));
					St.Pts.Add(C + Dir * Rr * 1.01f);
					St.Nrm.Add(Dir);
				}
				K.Strips.Add(St);
			}
			FWRTreeCollider C;
			C.A = FVector(20.f, 0.f, 32.f);
			C.B = FVector(780.f, 0.f, 32.f);
			C.Radius = 36.f;
			K.Colliders.Add(C);
		}
		else if (S == TEXT("root_arch"))
		{
			K.Barks.Add(MakeBark(0x5A4A3A, 0x2E241C, 0x4E6B2A, 0.9f, 0.85f, 0.f, 0.3f, 10.f, 0.45f));
			FLimb Ar;
			Ar.Level = 0;
			Ar.Bark = 0;
			Ar.Sides = 10.f;
			Ar.Cap = 0;
			const float Wv = Rs.R(-40.f, 40.f), Peak = Rs.R(380.f, 460.f);
			for (int32 k = 0; k <= 12; ++k)
			{
				const float S2 = float(k) / 12.f;
				Ar.Pts.Add(FVector(1200.f * S2, Wv * FMath::Sin(2.f * PI * S2), Peak * FMath::Pow(FMath::Sin(PI * S2), 0.9f) - 50.f));
				Ar.Rad.Add(FMath::Lerp(70.f, 45.f, FMath::Sin(PI * S2)));
			}
			K.Limbs.Add(Ar);
			for (const float End : { 0.f, 1200.f })
			{
				for (int32 r = 0; r < 3; ++r)
				{
					const float A = Rs.R(0.f, 2.f * PI);
					const FVector Dir(FMath::Cos(A) * 0.6f + (End > 0.f ? 0.6f : -0.6f), FMath::Sin(A), 0.f);
					FLimb Rt;
					Rt.Level = 3;
					Rt.Bark = 0;
					Rt.Sides = 5.f;
					const FVector B0(End, 0.f, 0.f);
					Rt.Pts = { B0 + FVector(0.f, 0.f, 20.f), B0 + Dir.GetSafeNormal() * 120.f, B0 + Dir.GetSafeNormal() * 230.f - FVector(0.f, 0.f, 20.f) };
					Rt.Rad = { 24.f, 14.f, 4.f };
					K.Limbs.Add(Rt);
				}
			}
			for (int32 c = 0; c < 5; ++c)
			{
				FWRTreeCollider C;
				C.A = Ar.Pts[c * 12 / 5];
				C.B = Ar.Pts[(c + 1) * 12 / 5];
				C.Radius = 45.f;
				K.Colliders.Add(C);
			}
		}
		else if (S == TEXT("buttress_root"))
		{
			// One buttress fin: a tall thin plank of root running out from a trunk (x 0) down into the ground (x 600).
			K.Barks.Add(MakeBark(0x4A3A2A, 0x261C14, 0x4E6B2A, 0.92f, 0.85f, 0.f, 0.35f, 16.f, 0.55f));
			FLimb Fin;
			Fin.Level = 0;
			Fin.Bark = 0;
			Fin.Sides = 12.f;
			Fin.Cap = 1;
			const float Wv = Rs.R(-30.f, 30.f);
			for (int32 k = 0; k <= 8; ++k)
			{
				const float S2 = float(k) / 8.f;
				const float FinH = 280.f * FMath::Pow(1.f - S2, 1.3f) + 20.f;
				Fin.Pts.Add(FVector(600.f * S2, Wv * FMath::Sin(PI * S2), FinH * 0.5f - 30.f));
				Fin.Rad.Add(30.f * (1.f - S2) + 8.f);
				Fin.RadV.Add(FinH * 0.5f);
			}
			K.Limbs.Add(Fin);
			FWRTreeCollider C;
			C.A = FVector(0.f, 0.f, 90.f); C.B = FVector(320.f, 0.f, 50.f); C.Radius = 40.f; K.Colliders.Add(C);
			C.A = FVector(0.f, 0.f, 190.f); C.B = FVector(150.f, 0.f, 140.f); C.Radius = 35.f; K.Colliders.Add(C);
			C.A = FVector(320.f, 0.f, 25.f); C.B = FVector(560.f, 0.f, 5.f); C.Radius = 22.f; K.Colliders.Add(C);
		}
		else if (S == TEXT("root_mat"))
		{
			// A woven roof of roots (the Hollow Root's rooms under the forest floor): roots both ways over and under each
			// other, a dark mat of soil and fine roots under them, its edge dipping into the ground. Walkable on top.
			K.Barks.Add(MakeBark(0x4A3A2C, 0x241A12, 0x3A5222, 0.92f, 0.9f, 0.f, 0.3f, 8.f, 0.45f));
			const auto Dome = [](float X, float Y) { return 120.f * (1.f - FMath::Square(X / 1500.f)) * (1.f - FMath::Square(Y / 1500.f)) - 80.f * FMath::Max(FMath::Square(X / 1500.f), FMath::Square(Y / 1500.f)); };
			for (int32 Dir = 0; Dir < 2; ++Dir)
			{
				for (int32 r = 0; r < 14; ++r)
				{
					const float Off = -1430.f + 2860.f * float(r) / 13.f + Rs.R(-40.f, 40.f);
					FLimb Rt;
					Rt.Level = r % 2 == 0 ? 0 : 2;
					Rt.Bark = 0;
					Rt.Sides = 6.f;
					Rt.Cap = 0;
					for (int32 k = 0; k <= 10; ++k)
					{
						const float T = -1550.f + 3100.f * float(k) / 10.f;
						const float Weave = 30.f * FMath::Sin(PI * float(k + r + Dir));
						const float X = Dir == 0 ? T : Off, Y = Dir == 0 ? Off : T;
						Rt.Pts.Add(FVector(X, Y, Dome(FMath::Clamp(X, -1500.f, 1500.f), FMath::Clamp(Y, -1500.f, 1500.f)) + Weave + 20.f));
						Rt.Rad.Add(Rs.R(32.f, 55.f));
					}
					K.Limbs.Add(Rt);
				}
			}
			K.Extras.Add([Dome](FGeo& G, const FDetail& D)
			{
				const int32 N = D.Q > 0.7f ? 10 : D.Q > 0.35f ? 6 : 3;
				TArray<int32> Top, Bot;
				for (int32 j = 0; j <= N; ++j)
				{
					for (int32 i = 0; i <= N; ++i)
					{
						const float X = -1520.f + 3040.f * float(i) / float(N), Y = -1520.f + 3040.f * float(j) / float(N);
						const float Z = Dome(FMath::Clamp(X, -1500.f, 1500.f), FMath::Clamp(Y, -1500.f, 1500.f)) - 10.f;
						const FLinearColor C = FMath::Lerp(Hex(0x2A2018), Hex(0x3A5222), Noise(FVector(X, Y, 0.f) * 0.004f) * 0.6f);
						Top.Add(G.V(SBark, FVector(X, Y, Z), FVector::UpVector, FVector::XAxisVector, X / 150.f, Y / 150.f, C, 0.97f));
						Bot.Add(G.V(SBark, FVector(X, Y, Z - 25.f), -FVector::UpVector, FVector::XAxisVector, X / 150.f, Y / 150.f, Hex(0x1E160E), 0.97f));
					}
				}
				for (int32 j = 0; j < N; ++j)
				{
					for (int32 i = 0; i < N; ++i)
					{
						const int32 a = j * (N + 1) + i, b = a + 1, c = a + N + 2, d = a + N + 1;
						G.Quad(SBark, Top[a], Top[b], Top[c], Top[d], FVector::UpVector);
						G.Quad(SBark, Bot[a], Bot[b], Bot[c], Bot[d], -FVector::UpVector);
					}
				}
			});
			for (int32 r = 0; r < 16; ++r)
			{
				const float Y = -1425.f + 2850.f * float(r) / 15.f;
				FWRTreeCollider C;
				C.Radius = 100.f;
				C.A = FVector(-1400.f, Y, Dome(-700.f, Y) - 100.f);
				C.B = FVector(1400.f, Y, Dome(700.f, Y) - 100.f);
				K.Colliders.Add(C);
			}
		}
		else if (S == TEXT("fern"))
		{
			Fern(K, Rs);
		}
		else if (S == TEXT("bramble"))
		{
			Bramble(K, Rs);
		}
		else if (S == TEXT("boulder"))
		{
			Boulder(K, Rs);
		}
		if (K.Barks.Num() == 0) K.Barks.Add(FBark());
	}

	static FSkel& SkelFor(const FSpeciesInfo& Info, int32 Variant)
	{
		static TMap<uint32, TUniquePtr<FSkel>> Cache;
		const uint32 Key = HashCombine(FCrc::StrCrc32(Info.Name), uint32(Variant));
		if (TUniquePtr<FSkel>* Found = Cache.Find(Key)) return **Found;
		TUniquePtr<FSkel> K = MakeUnique<FSkel>();
		BuildSkel(*K, Info, Variant);
		FSkel& Out = *K;
		Cache.Add(Key, MoveTemp(K));
		return Out;
	}

	static void Budgets(EBudget B, TArray<int32>& Out, TArray<float>& Screen)
	{
		switch (B)
		{
		case EBudget::Giant: Out = { 14000, 3500, 800 }; Screen = { 1.f, 0.22f, 0.07f }; break;
		case EBudget::Broad: Out = { 7000, 1800, 400 }; Screen = { 1.f, 0.2f, 0.07f }; break;
		case EBudget::Shrub: Out = { 2500, 600, 150 }; Screen = { 1.f, 0.2f, 0.06f }; break;
		default:             Out = { 400, 120 }; Screen = { 1.f, 0.12f }; break;
		}
	}

	/** One LOD within its triangle budget: wood first (coarser until it leaves room), then as many sprays as fit. */
	static FGeo MeshAt(const FSkel& K, int32 Budget, int32 Lod, int32& InOutN0)
	{
		float Q = Lod == 0 ? 1.f : Lod == 1 ? 0.5f : 0.28f;
		FGeo G;
		for (int32 Try = 0; Try < 8; ++Try, Q *= 0.82f)
		{
			G = FGeo();
			FDetail D;
			D.Lod = Lod;
			D.Q = Q;
			for (const FTrunk& T : K.Trunks) MeshTrunk(G, K, T, D);
			for (const FLimb& L : K.Limbs) MeshTube(G, K, L, D);
			for (const FStrip& S : K.Strips) MeshStrip(G, S, D);
			for (const TFunction<void(FGeo&, const FDetail&)>& E : K.Extras) E(G, D);
			const int32 Fixed = G.Tris();
			if (K.Clusters.Num() == 0)
			{
				if (Fixed > Budget && Try < 7) continue;
				return G;
			}
			TArray<FVector2D> Outline;
			LeafOutline(K.LeafShape, D.Q > 0.75f ? 0 : D.Q > 0.4f ? 1 : 2, Outline);
			const int32 Per = 2 * Outline.Num() * 2;
			const int32 Room = Budget - Fixed;
			if (Room < Per * FMath::Min(K.Clusters.Num(), 8) && Try < 7) continue;
			const int32 N = FMath::Clamp(Room / FMath::Max(1, Per), 0, K.Clusters.Num());
			if (Lod == 0) InOutN0 = N;
			const float SizeMul = FMath::Clamp(FMath::Sqrt(float(FMath::Max(InOutN0, 1)) / float(FMath::Max(N, 1))), 1.f, 3.f);
			for (int32 c = 0; c < N; ++c) MeshCluster(G, K.Clusters[c], Outline, SizeMul, K.LeafLo, K.LeafHi, K.LeafRough);
			return G;
		}
		return G;
	}

	static void ToDescription(const FGeo& G, FMeshDescription& MD)
	{
		FStaticMeshAttributes Attr(MD);
		Attr.Register();
		TVertexAttributesRef<FVector3f> Pos = Attr.GetVertexPositions();
		TVertexInstanceAttributesRef<FVector3f> Nrm = Attr.GetVertexInstanceNormals();
		TVertexInstanceAttributesRef<FVector3f> Tan = Attr.GetVertexInstanceTangents();
		TVertexInstanceAttributesRef<float> Sgn = Attr.GetVertexInstanceBinormalSigns();
		TVertexInstanceAttributesRef<FVector4f> Col = Attr.GetVertexInstanceColors();
		TVertexInstanceAttributesRef<FVector2f> UVs = Attr.GetVertexInstanceUVs();
		TPolygonGroupAttributesRef<FName> Slots = Attr.GetPolygonGroupMaterialSlotNames();
		UVs.SetNumChannels(1);
		int32 NV = 0, NT = 0;
		for (const FSec& S : G.S) { NV += S.P.Num(); NT += S.I.Num() / 3; }
		MD.ReserveNewVertices(NV);
		MD.ReserveNewVertexInstances(NV);
		MD.ReserveNewTriangles(NT);
		MD.ReserveNewPolygons(NT);
		MD.ReserveNewEdges(NT * 2);
		static const FName TreeSlots[3] = { FName(TEXT("Bark")), FName(TEXT("Leaves")), FName(TEXT("Glow")) };
		for (int32 s = 0; s < 3; ++s)
		{
			const FSec& S = G.S[s];
			const FPolygonGroupID Grp = MD.CreatePolygonGroup();
			Slots[Grp] = TreeSlots[s];
			TArray<FVertexInstanceID> Ids;
			Ids.Reserve(S.P.Num());
			for (int32 i = 0; i < S.P.Num(); ++i)
			{
				const FVertexID V = MD.CreateVertex();
				Pos[V] = S.P[i];
				const FVertexInstanceID I = MD.CreateVertexInstance(V);
				Nrm[I] = S.N[i];
				Tan[I] = S.T[i];
				Sgn[I] = 1.f;
				Col[I] = S.C[i];
				UVs.Set(I, 0, S.UV[i]);
				Ids.Add(I);
			}
			for (int32 t = 0; t + 2 < S.I.Num(); t += 3)
			{
				const FVertexInstanceID Tri[3] = { Ids[S.I[t]], Ids[S.I[t + 1]], Ids[S.I[t + 2]] };
				MD.CreateTriangle(Grp, Tri);
			}
		}
	}

	static UMaterialInterface* LoadRooted(const TCHAR* Path)
	{
		UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, Path);
		if (M) M->AddToRoot();
		return M;
	}

	static UMaterialInterface* GlowBase()
	{
		static UMaterialInterface* Mat = nullptr;
		static bool bTried = false;
		if (!bTried)
		{
			bTried = true;
			Mat = LoadRooted(TEXT("/Game/Wildreach/Materials/M_TreeKitGlow.M_TreeKitGlow"));
			if (!Mat) UE_LOG(LogWildreach, Warning, TEXT("TREEKIT M_TreeKitGlow is missing (Tools/art/make_tree_kit_material.py): glow strips draw in the bark material"));
		}
		return Mat ? Mat : WRTreeKit::Material(false);
	}
}

// ================================================================================================ public

float WRTreeKit::ReferenceHeight(FName Species)
{
	return WRTreeKitImpl::InfoFor(Species).H;
}

float WRTreeKit::ReferenceRadius(FName Species)
{
	return WRTreeKitImpl::InfoFor(Species).R;
}

int32 WRTreeKit::VariantAt(const FVector& Ground)
{
	// By the metre the decoration stands on: the generator writes whole centimetres, so every machine agrees.
	const int32 X = FMath::FloorToInt32(Ground.X / 100.0), Y = FMath::FloorToInt32(Ground.Y / 100.0);
	return FMath::Min(3, FMath::FloorToInt32(WRTreeKitImpl::Hash3(X, Y, 911) * 4.f));
}

UMaterialInterface* WRTreeKit::Material(bool bLeaves)
{
	static UMaterialInterface* Bark = nullptr;
	static UMaterialInterface* Leaf = nullptr;
	static bool bTried = false;
	if (!bTried)
	{
		bTried = true;
		Bark = WRTreeKitImpl::LoadRooted(TEXT("/Game/Wildreach/Materials/M_TreeKit.M_TreeKit"));
		Leaf = WRTreeKitImpl::LoadRooted(TEXT("/Game/Wildreach/Materials/M_TreeKitLeaf.M_TreeKitLeaf"));
		if (!Bark || !Leaf)
		{
			UE_LOG(LogWildreach, Warning, TEXT("TREEKIT M_TreeKit / M_TreeKitLeaf missing (Tools/art/make_tree_kit_material.py): trees draw in the held-tool material"));
		}
		if (!Bark) Bark = FWRToolMesh::Material();
		if (!Leaf) Leaf = Bark;
	}
	return bLeaves ? Leaf : Bark;
}

UMaterialInstanceDynamic* WRTreeKit::GlowMaterial(UObject* Outer, const FLinearColor& Tint, float Strength)
{
	UMaterialInterface* Base = WRTreeKitImpl::GlowBase();
	if (!Base) return nullptr;
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, Outer ? Outer : GetTransientPackage());
	MID->SetVectorParameterValue(TEXT("Tint"), Tint);
	MID->SetScalarParameterValue(TEXT("Glow"), Strength);
	return MID;
}

const TArray<FWRTreeCollider>& WRTreeKit::Colliders(FName Species, int32 Variant)
{
	return WRTreeKitImpl::SkelFor(WRTreeKitImpl::InfoFor(Species), FMath::Clamp(Variant, 0, 3)).Colliders;
}

FVector WRTreeKit::Anchor(FName Species, int32 Variant, FName Point)
{
	const FVector* Found = WRTreeKitImpl::SkelFor(WRTreeKitImpl::InfoFor(Species), FMath::Clamp(Variant, 0, 3)).Anchors.Find(Point);
	return Found ? *Found : FVector::ZeroVector;
}

int32 WRTreeKit::AddCollision(AActor* Owner, FName Species, int32 Variant, const FTransform& At)
{
	USceneComponent* Root = Owner ? Owner->GetRootComponent() : nullptr;
	if (!Root) return 0;
	const FVector S = At.GetScale3D().GetAbs();
	int32 Added = 0;
	for (const FWRTreeCollider& C : Colliders(Species, Variant))
	{
		// The radius scales with the axes across the capsule (uneven scale: logs and roots).
		const FVector Axis = (C.B - C.A).GetSafeNormal();
		const FVector Across = FVector(1.0) - Axis * Axis;
		const float Sum = float(Across.X + Across.Y + Across.Z);
		const float Rs = Sum > 0.01f ? float((S.X * Across.X + S.Y * Across.Y + S.Z * Across.Z) / Sum) : float(S.X);
		const FVector A = At.TransformPosition(C.A), B = At.TransformPosition(C.B);
		const float R = C.Radius * Rs;
		const float Len = float(FVector::Dist(A, B));
		UCapsuleComponent* Cap = NewObject<UCapsuleComponent>(Owner);
		Cap->SetupAttachment(Root);
		Cap->InitCapsuleSize(R, Len * 0.5f + R);
		Cap->SetCollisionProfileName(TEXT("BlockAll"));
		Cap->SetHiddenInGame(true);
		Cap->RegisterComponent();
		const FRotator Rot = Len > 1.f ? FRotationMatrix::MakeFromZ(B - A).Rotator() : FRotator::ZeroRotator;
		Cap->SetWorldLocationAndRotation((A + B) * 0.5f, Rot);
		Cap->SetWorldScale3D(FVector::OneVector);
		++Added;
	}
	return Added;
}

UStaticMesh* WRTreeKit::Mesh(UObject* Outer, FName Species, int32 Variant)
{
	if (IsRunningDedicatedServer() || !FApp::CanEverRender()) return nullptr;
	const WRTreeKitImpl::FSpeciesInfo& Info = WRTreeKitImpl::InfoFor(Species);
	Variant = FMath::Clamp(Variant, 0, 3);
	static TMap<uint32, UStaticMesh*> Cache;
	const uint32 Key = HashCombine(FCrc::StrCrc32(Info.Name), uint32(Variant));
	if (UStaticMesh** Found = Cache.Find(Key)) return *Found;
	const WRTreeKitImpl::FSkel& K = WRTreeKitImpl::SkelFor(Info, Variant);
	TArray<int32> Budget;
	TArray<float> Screen;
	WRTreeKitImpl::Budgets(Info.Budget, Budget, Screen);
	TArray<FMeshDescription> Descs;
	Descs.SetNum(Budget.Num());
	TArray<int32> Counts;
	int32 N0 = 0;
	for (int32 Lod = 0; Lod < Budget.Num(); ++Lod)
	{
		const WRTreeKitImpl::FGeo G = WRTreeKitImpl::MeshAt(K, Budget[Lod], Lod, N0);
		Counts.Add(G.Tris());
		WRTreeKitImpl::ToDescription(G, Descs[Lod]);
	}
	// One mesh for everyone (Outer is only who asked): transient, rooted for the session.
	UPackage* Pkg = GetTransientPackage();
	UStaticMesh* M = NewObject<UStaticMesh>(Pkg, MakeUniqueObjectName(Pkg, UStaticMesh::StaticClass(), FName(*FString::Printf(TEXT("SM_TreeKit_%s_%d"), Info.Name, Variant))), RF_Transient);
	M->AddToRoot();
	TArray<FStaticMaterial> Mats;
	Mats.Add(FStaticMaterial(Material(false), TEXT("Bark")));
	Mats.Add(FStaticMaterial(Material(true), TEXT("Leaves")));
	Mats.Add(FStaticMaterial(WRTreeKitImpl::GlowBase(), TEXT("Glow")));
	M->SetStaticMaterials(Mats);
	UStaticMesh::FBuildMeshDescriptionsParams Params;
	Params.bFastBuild = true;
	Params.bCommitMeshDescription = false;
	Params.bMarkPackageDirty = false;
	Params.bBuildSimpleCollision = false;
	TArray<const FMeshDescription*> Ptrs;
	for (const FMeshDescription& D : Descs) Ptrs.Add(&D);
	M->BuildFromMeshDescriptions(Ptrs, Params);
	if (FStaticMeshRenderData* RD = M->GetRenderData())
	{
		for (int32 i = 0; i < RD->LODResources.Num() && i < Screen.Num(); ++i) RD->ScreenSize[i].Default = Screen[i];
	}
	Cache.Add(Key, M);
	UE_LOG(LogWildreach, Log, TEXT("TREEKIT built %s v%d: %s triangles (outer %s)"), Info.Name, Variant,
		*FString::JoinBy(Counts, TEXT(" / "), [](int32 C) { return FString::FromInt(C); }), Outer ? *Outer->GetName() : TEXT("none"));
	return M;
}
