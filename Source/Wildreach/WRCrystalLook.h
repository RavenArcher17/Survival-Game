#pragma once

// The Crystal caves' scenery kit (shared by WRCrystalScenery*.cpp): the palette (crystal 2.2: basalt black #1C1C20,
// salt-grass #7E8C5A, sea slate #4E5F66, violet hole-glow #7A3FD1, amethyst #6B3FA0, agate cream / rust, quartz white,
// selenite #F2F0E8, steam teal #4FA8A0, mineral gold #C8A048, lava glaze #121216, ochre #A8402A, hewn basalt #2A2A30,
// ward violet, crystal dust #D8D0FF, bronze #8C6A3A) and the pieces every place is built from, on the forest's FBuild
// (WRForestSceneryKit.h). Real measures (build cook 1, crystal 1.2): quartz grows as a six-sided prism capped by a
// six-faced pyramid; basalt columns are 0.5-1.4 m across, hexagonal; a hexafoil is six petals drawn with one compass
// setting, the Wardcarvers' mark on every door and keystone. Every free function is inline and lives in WRCrystalLook.

#include "WRForestSceneryKit.h"
#include "WRCrystalTypes.h"
#include "WRLand.h"
#include "WRTuning.h"

namespace WRCrystalLook
{
	using WRForestLook::FBuild;
	using FSurf = WRLookKit::FSurf;
	using WRForestLook::Rgb;

	// ---- the palette (linear, dimmed as the look kit's tinted blocks want it) ----
	inline FSurf Basalt(float K = 1.f) { return FSurf::Stone(Rgb(28, 28, 32) * 0.8f * K, 0.82f); }
	inline FSurf Dressed(float K = 1.f) { return FSurf::Stone(Rgb(42, 42, 48) * 0.8f * K, 0.78f); }
	inline FSurf Polished(float K = 1.f) { return FSurf::Glaze(Rgb(22, 22, 26) * 0.8f * K, 0.18f); }
	inline FSurf SaltRim(float K = 1.f) { return FSurf::Stone(Rgb(216, 212, 204) * 0.62f * K, 0.9f); }
	inline FSurf Bronze(float K = 1.f) { return FSurf::Steel(Rgb(140, 106, 58) * 0.6f * K, 0.45f); }
	inline FSurf Verdigris(float K = 1.f) { return FSurf::Steel(Rgb(84, 140, 122) * 0.5f * K, 0.62f); }
	inline FSurf Lead(float K = 1.f) { return FSurf::Steel(Rgb(90, 92, 98) * 0.55f * K, 0.6f); }
	inline FSurf Iron(float K = 1.f) { return FSurf::Steel(FLinearColor(0.03f, 0.028f, 0.027f) * K, 0.7f); }
	inline FSurf Timber(float K = 1.f) { return FSurf::Wood(Rgb(84, 68, 54) * 0.45f * K); }
	inline FSurf Drift(float K = 1.f) { return FSurf::Wood(Rgb(148, 140, 126) * 0.45f * K); }
	inline FSurf Tarred(float K = 1.f) { return FSurf::Wood(Rgb(36, 30, 26) * 0.6f * K); }
	inline FSurf Canvas(float K = 1.f) { return FSurf::Cloth(Rgb(196, 186, 160) * 0.48f * K); }
	inline FSurf Rope(float K = 1.f) { return FSurf::Cloth(Rgb(156, 132, 96) * 0.5f * K); }
	inline FSurf Wool(float K = 1.f) { return FSurf::Cloth(Rgb(150, 138, 120) * 0.5f * K); }
	inline FSurf Wax(float K = 1.f) { return FSurf::Glaze(Rgb(232, 221, 176) * 0.62f * K, 0.55f); }
	inline FSurf Ochre(float K = 1.f) { return FSurf::Cloth(Rgb(168, 64, 42) * 0.55f * K); }
	inline FSurf Guano(float K = 1.f) { return FSurf::Cloth(Rgb(140, 134, 120) * 0.5f * K); }
	inline FSurf Bone(float K = 1.f) { return FSurf::Stone(Rgb(217, 207, 184) * 0.66f * K, 0.78f); }
	inline FSurf Grass(float K = 1.f) { return FSurf::Cloth(Rgb(126, 140, 90) * 0.48f * K); }
	inline FSurf Heather(float K = 1.f) { return FSurf::Cloth(Rgb(110, 76, 96) * 0.48f * K); }
	inline FSurf Campion(float K = 1.f) { return FSurf::Cloth(Rgb(236, 230, 236) * 0.6f * K); }
	inline FSurf AgateCream(float K = 1.f) { return FSurf::Glaze(Rgb(230, 216, 192) * 0.6f * K, 0.35f); }
	inline FSurf AgateRust(float K = 1.f) { return FSurf::Glaze(Rgb(168, 90, 58) * 0.55f * K, 0.35f); }
	inline FSurf Quartz(float K = 1.f) { return FSurf::Glass(Rgb(237, 234, 244) * 0.6f * K, 0.f); }
	/** Amethyst: violet glass with a little of its own light (the Wardcarvers' sound-lit crystal at rest). */
	inline FSurf Amethyst(float Glow = 0.08f) { return FSurf::Glass(Rgb(107, 63, 160) * 0.6f, Glow); }
	inline FSurf AmethystPale(float Glow = 0.12f) { return FSurf::Glass(Rgb(205, 184, 242) * 0.55f, Glow); }
	/** The violet hole-glow at night (#7A3FD1). */
	inline FSurf HoleGlow(float Glow) { return FSurf::Lit(Rgb(122, 63, 209) * 0.6f, Glow); }
	inline FSurf WardGlow(float Glow) { return FSurf::Lit(Rgb(150, 110, 230) * 0.6f, Glow); }
	inline FSurf Selenite(float Glow = 0.05f) { return FSurf::Glass(Rgb(242, 240, 232) * 0.62f, Glow); }
	inline FSurf SeleniteBand() { return FSurf::Stone(Rgb(184, 188, 192) * 0.6f, 0.6f); }
	inline FSurf Mineral(float K = 1.f) { return FSurf::Glaze(Rgb(200, 160, 72) * 0.55f * K, 0.4f); }
	inline FSurf SteamWater() { return FSurf::Glaze(Rgb(79, 168, 160) * 0.5f, 0.04f); }
	inline FSurf DarkWater() { return FSurf::Glaze(Rgb(14, 16, 22) * 0.6f, 0.03f); }
	inline FSurf Ember(float Glow) { return FSurf::Lit(Rgb(255, 140, 60) * 0.6f, Glow); }
	inline FSurf Bleached(float K = 1.f) { return FSurf::Glass(Rgb(185, 180, 196) * 0.55f * K, 0.f); }
	inline FLinearColor VioletLight() { return FLinearColor(FColor(150, 100, 255)); }
	inline FLinearColor LilacLight() { return FLinearColor(FColor(216, 208, 255)); }
	inline FLinearColor FireLight() { return FLinearColor(FColor(255, 170, 90)); }
	inline FLinearColor TimberC() { return Rgb(84, 68, 54) * 0.45f; }
	inline FLinearColor RopeC() { return Rgb(156, 132, 96) * 0.5f; }
	inline FLinearColor BronzeC() { return Rgb(140, 106, 58) * 0.6f; }
	inline FLinearColor BoneC() { return Rgb(217, 207, 184) * 0.66f; }
	inline FLinearColor LilacC() { return Rgb(216, 208, 255) * 0.6f; }
	inline FLinearColor BleachC() { return Rgb(185, 180, 196) * 0.5f; }

	inline float Hash(int32 Seed, int32 Salt) { return FMath::Frac(FMath::Sin(float(Seed * 131 + Salt * 977) * 0.731f) * 43758.5453f); }

	/** A hexagonal prism along Dir from Base, R across its flats' half, L long (three boxes turned 60 degrees apart). */
	inline void HexAlong(FBuild& B, const FSurf& S, const FVector& Base, const FVector& Dir, float R, float L, float Turn = 0.f, bool bNight = false)
	{
		const FVector D = Dir.GetSafeNormal();
		const FQuat Q = FRotationMatrix::MakeFromZ(D).ToQuat();
		for (int32 k = 0; k < 3; ++k)
		{
			const FQuat Spin(FVector::UpVector, FMath::DegreesToRadians(Turn + 60.f * k));
			(bNight ? B.N : B.K).Add(S, FVector(R, R * 1.7320508f, L), Base + D * (L * 0.5f), (Q * Spin).Rotator());
		}
	}
	/** One quartz point: the prism and its pyramid tip (a cone at block scale). */
	inline void Point(FBuild& B, const FSurf& S, const FVector& Root, const FVector& Dir, float Len, float Thick, float Turn = 0.f, bool bNight = false)
	{
		const FVector D = Dir.GetSafeNormal();
		HexAlong(B, S, Root, D, Thick * 0.5f, Len * 0.8f, Turn, bNight);
		(bNight ? B.N : B.K).Spike(S, Root + D * (Len * 0.8f - 2.f), D, Thick * 0.98f, Len * 0.2f + 2.f);
	}
	/** A cluster of 3-9 points round a common root, pointing along Up within 25 degrees (crystal 2.2 linings). */
	inline void Cluster(FBuild& B, const FVector& Root, const FVector& Up, float Size, int32 Seed, const FSurf& S, const FSurf& Tip, bool bNight = false)
	{
		const int32 K = 3 + int32(Hash(Seed, 1) * 6.99f);
		FVector T1, T2;
		Up.GetSafeNormal().FindBestAxisVectors(T1, T2);
		for (int32 p = 0; p < K; ++p)
		{
			const float A = Hash(Seed, 10 + p) * 2.f * PI, Tilt = (p == 0 ? 0.1f : 0.25f + 0.6f * Hash(Seed, 20 + p)) * 25.f;
			const FVector Lean = (T1 * FMath::Cos(A) + T2 * FMath::Sin(A));
			const FVector Dir = (Up.GetSafeNormal() * FMath::Cos(FMath::DegreesToRadians(Tilt)) + Lean * FMath::Sin(FMath::DegreesToRadians(Tilt))).GetSafeNormal();
			const float Len = Size * (p == 0 ? 1.f : 0.35f + 0.5f * Hash(Seed, 30 + p));
			Point(B, p == 0 ? Tip : S, Root + Lean * Size * 0.12f * (p > 0) - Up.GetSafeNormal() * 10.f, Dir, Len, Len * (0.2f + 0.08f * Hash(Seed, 40 + p)), Hash(Seed, 50 + p) * 60.f, bNight);
		}
	}
	/** A hexafoil (six petals drawn with one compass setting, in a ring) carved or inlaid on a face: Centre, the face's
	 *  normal, its radius. Raised Depth cm off the face in Surf. */
	inline void Hexafoil(FBuild& B, const FSurf& S, const FVector& Centre, const FVector& Normal, float R, float Depth = 3.f, bool bNight = false)
	{
		const FVector Nn = Normal.GetSafeNormal();
		FVector U, V;
		Nn.FindBestAxisVectors(U, V);
		WRLookKit::FKit& Kit = bNight ? B.N : B.K;
		const int32 Seg = 14;
		// The outer ring.
		for (int32 i = 0; i < 24; ++i)
		{
			const float A0 = 2.f * PI * i / 24.f, A1 = 2.f * PI * (i + 1) / 24.f;
			const FVector P0 = Centre + (U * FMath::Cos(A0) + V * FMath::Sin(A0)) * R, P1 = Centre + (U * FMath::Cos(A1) + V * FMath::Sin(A1)) * R;
			Kit.Add(S, FVector(FVector::Dist(P0, P1) + 1.f, FMath::Max(2.f, R * 0.05f), Depth), (P0 + P1) * 0.5f + Nn * Depth * 0.5f, FRotationMatrix::MakeFromXZ(P1 - P0, Nn).Rotator());
		}
		// Six petals: arcs of radius R/... each centred on the ring of R/2, through the middle (the compass rosette).
		for (int32 p = 0; p < 6; ++p)
		{
			const float Ac = 2.f * PI * p / 6.f;
			const FVector C = Centre + (U * FMath::Cos(Ac) + V * FMath::Sin(Ac)) * (R * 0.5f);
			for (int32 i = 0; i < Seg; ++i)
			{
				const float A0 = Ac + PI + (-PI / 3.f) + (2.f * PI / 3.f) * i / Seg, A1 = Ac + PI + (-PI / 3.f) + (2.f * PI / 3.f) * (i + 1) / Seg;
				const FVector P0 = C + (U * FMath::Cos(A0) + V * FMath::Sin(A0)) * (R * 0.5f), P1 = C + (U * FMath::Cos(A1) + V * FMath::Sin(A1)) * (R * 0.5f);
				Kit.Add(S, FVector(FVector::Dist(P0, P1) + 1.f, FMath::Max(1.5f, R * 0.035f), Depth), (P0 + P1) * 0.5f + Nn * Depth * 0.5f, FRotationMatrix::MakeFromXZ(P1 - P0, Nn).Rotator());
			}
		}
	}
	/** A kerb of dressed basalt blocks round a hole's lip (it covers the join of the ground and the rock below). */
	inline void Kerb(FBuild& B, const FVector& Centre, float R, float Width, int32 Seed)
	{
		const int32 N = FMath::Max(10, int32(2.f * PI * R / 140.f));
		for (int32 i = 0; i < N; ++i)
		{
			const float A = 2.f * PI * (i + 0.5f) / N;
			const FVector Dir(FMath::Cos(A), FMath::Sin(A), 0.f);
			const FVector P = Centre + Dir * (R + Width * 0.3f);
			const float Gz = B.G(P.X, P.Y, Centre.Z);
			const float Len = 2.f * PI * (R + Width * 0.3f) / N + 4.f;
			B.K.Add(Basalt(0.85f + 0.3f * Hash(Seed, i)), FVector(Len, Width, 60.f + 25.f * Hash(Seed, i + 50)), FVector(P.X, P.Y, Gz + 10.f), FRotator(0.f, FMath::RadiansToDegrees(A) + 90.f, (Hash(Seed, i + 90) - 0.5f) * 6.f));
			if (Hash(Seed, i + 130) > 0.7f) B.K.Add(SaltRim(), FVector(Len * 0.6f, Width * 0.3f, 6.f), FVector(P.X, P.Y, Gz + 44.f + 25.f * Hash(Seed, i + 50) * 0.5f) - Dir * Width * 0.2f, FRotator(0.f, FMath::RadiansToDegrees(A) + 90.f, 0.f));
		}
	}
	/** A Wardcarver chime-stone arch (world-map waymark): two dressed basalt posts, a lintel with a hexafoil, a crystal
	 *  chime bar hung on a bronze chain under it. Base on the ground, facing Yaw (local frame of the build). */
	inline void ChimeArch(FBuild& B, const FVector& Base, float Yaw, float Scale, int32 Seed)
	{
		const FRotator R(0.f, Yaw, 0.f);
		const FVector Side = R.RotateVector(FVector(0.f, 1.f, 0.f)), Fwd = R.RotateVector(FVector(1.f, 0.f, 0.f));
		const float H = 320.f * Scale, Span = 110.f * Scale;
		for (const float S : { -1.f, 1.f })
		{
			const FVector P = Base + Side * Span * S;
			const float Gz = B.G(P.X, P.Y, Base.Z);
			B.K.Add(Dressed(), FVector(40.f, 40.f, H + 30.f) * FVector(Scale, Scale, 1.f), FVector(P.X, P.Y, Gz + (H - 30.f) * 0.5f), R);
			B.Box(FVector(P.X, P.Y, Gz + H * 0.5f), FVector(22.f, 22.f, H * 0.5f) * FVector(Scale, Scale, 1.f), R);
		}
		const FVector Top = Base + FVector(0.f, 0.f, H);
		B.K.Add(Dressed(1.1f), FVector(50.f, Span * 2.f + 70.f, 36.f) * FVector(Scale, 1.f, Scale), Top + FVector(0.f, 0.f, 18.f), R);
		Hexafoil(B, Bronze(), Top + FVector(0.f, 0.f, 18.f) + Fwd * 26.f * Scale, Fwd, 14.f * Scale, 2.f);
		// The chime: a crystal bar on a bronze chain (two links), hanging still.
		B.K.Rod(Bronze(0.8f), Top, Top - FVector(0.f, 0.f, 60.f * Scale), 3.f);
		Point(B, AmethystPale(0.18f), Top - FVector(0.f, 0.f, 60.f * Scale), FVector(0.f, 0.f, -1.f), 110.f * Scale, 16.f * Scale, Hash(Seed, 3) * 60.f);
	}
	/** A spiral stair round a shaft's wall from ZTop down to ZBottom (local cm; Centre the shaft's axis): treads hugging
	 *  the wall at Radius (the wall), Width wide, falling at 30 degrees; collision as sloped slabs every 15 degrees.
	 *  Style: stone (cut into the rind: solid steps), rope (planks on bronze pins with a rope rail: the Bell Throat). */
	inline void SpiralStair(FBuild& B, const FVector& Centre, float Radius, float ZTop, float ZBottom, float Width, FName Style, int32 Seed, float StartAngle = 0.f)
	{
		const float Rm = FMath::Max(150.f, Radius - Width * 0.5f - 20.f);
		const float Rise = 2.f * PI * Rm * FMath::Tan(FMath::DegreesToRadians(30.f));   // per turn
		const float Turns = FMath::Max(0.25f, (ZTop - ZBottom) / Rise);
		const int32 Steps = FMath::Max(4, int32(Turns * 24.f));
		const bool bRope = Style == TEXT("rope");
		const float StepRise = (ZTop - ZBottom) / Steps;
		FVector Prev = FVector::ZeroVector;
		for (int32 s = 0; s <= Steps; ++s)
		{
			const float A = FMath::DegreesToRadians(StartAngle) + 2.f * PI * Turns * s / Steps;
			const FVector Dir(FMath::Cos(A), FMath::Sin(A), 0.f), Tan(-FMath::Sin(A), FMath::Cos(A), 0.f);
			const float Z = ZTop - StepRise * s;
			const FVector Mid = Centre + Dir * Rm + FVector(0.f, 0.f, Z - Centre.Z);
			const float TreadLen = 2.f * PI * Rm / 24.f + 6.f;
			if (bRope)
			{
				// A plank on two bronze pins drilled into the wall; the rope rail on the inner edge.
				B.K.Add(Timber(0.9f + 0.2f * Hash(Seed, s)), FVector(Width, TreadLen, 7.f), Mid - FVector(0, 0, 4.f), FRotator(0.f, FMath::RadiansToDegrees(A), 0.f));
				for (const float Pz : { -0.3f, 0.3f }) B.K.Rod(Bronze(), Mid + Tan * TreadLen * Pz + Dir * (Width * 0.5f + 25.f) - FVector(0, 0, 10.f), Mid + Tan * TreadLen * Pz + Dir * (Width * 0.5f - 15.f) - FVector(0, 0, 10.f), 4.f);
				if (s % 2 == 0) B.K.Rod(Rope(), Mid - Dir * (Width * 0.5f - 4.f), Mid - Dir * (Width * 0.5f - 4.f) + FVector(0, 0, 95.f), 2.5f);
				if (s > 0) B.K.Rod(Rope(), Prev - Dir * (Width * 0.5f - 4.f) + FVector(0, 0, 95.f), Mid - Dir * (Width * 0.5f - 4.f) + FVector(0, 0, 95.f), 3.f);
			}
			else
			{
				// A step cut into the rind: solid to the wall.
				B.K.Add(Dressed(0.85f + 0.25f * Hash(Seed, s)), FVector(Width + 40.f, TreadLen, StepRise + 30.f), Mid - FVector(0, 0, (StepRise + 30.f) * 0.5f) + Dir * 20.f, FRotator(0.f, FMath::RadiansToDegrees(A), 0.f));
			}
			if (s > 0 && s % 2 == 0)
			{
				// Walkable: a slab along the flight's slope, under the treads' line.
				B.SegBox(Prev - FVector(0, 0, 12.f), Mid - FVector(0, 0, 12.f), Width * 0.5f, 10.f);
			}
			if (s % 2 == 0) Prev = Mid;
		}
	}
	/** A Wardcarver wind chime: a bronze ring on a post, crystal bars and bronze tubes hanging from it (S6, the vents). */
	inline void WindChime(FBuild& B, const FVector& Base, float H, int32 Seed, bool bBroken)
	{
		const float Gz = B.G(Base.X, Base.Y, Base.Z);
		const FVector Foot(Base.X, Base.Y, Gz);
		B.K.Hex(Basalt(0.9f), Base.X, Base.Y, Gz - 40.f, Gz + H, 14.f, Hash(Seed, 1) * 60.f);
		const FVector Top = Foot + FVector(0, 0, H);
		B.K.Rod(Bronze(), Top, Top + FVector(55.f, 0.f, 0.f), 4.f);
		const FVector Ring = Top + FVector(55.f, 0.f, -18.f);
		B.K.Disc(Bronze(0.9f), Ring, FVector::UpVector, 46.f, 4.f);
		const int32 Bars = bBroken ? 3 : 7;
		for (int32 i = 0; i < Bars; ++i)
		{
			const float A = 2.f * PI * i / 7.f;
			const FVector P = Ring + FVector(FMath::Cos(A) * 18.f, FMath::Sin(A) * 18.f, -4.f);
			B.K.Rod(Bronze(0.7f), P, P - FVector(0, 0, 14.f), 1.2f);
			if (i % 2) B.K.Rod(Bronze(1.1f), P - FVector(0, 0, 14.f), P - FVector(0, 0, 60.f + 20.f * Hash(Seed, i)), 3.f);
			else Point(B, AmethystPale(0.14f), P - FVector(0, 0, 14.f), FVector(0, 0, -1), 50.f + 25.f * Hash(Seed, i + 9), 7.f);
		}
		if (bBroken)
		{
			// The fallen bars in the grass under it.
			for (int32 i = 0; i < 3; ++i) Point(B, AmethystPale(0.06f), Foot + FVector(40.f + 30.f * Hash(Seed, i + 20), (Hash(Seed, i + 30) - 0.5f) * 80.f, 4.f), FVector(Hash(Seed, i) - 0.5f, Hash(Seed, i + 3) - 0.5f, 0.05f), 45.f, 7.f);
		}
	}
}
