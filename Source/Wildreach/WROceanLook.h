// The Ocean's look kit (cooks/biome-ocean.md 2.1-2.4, cooks/build-biome-ocean.md 2.2): its palette and the Saltkin
// pieces every site shares, on the forest's FBuild (WRForestSceneryKit.h). Every free function is inline and named Oc...
// (unity builds join the scenery files with the Sky's and the Crystal's kits).
#pragma once

#include "CoreMinimal.h"
#include "WRForestSceneryKit.h"

namespace WROceanLook
{
	using namespace WRForestLook;
	using FSurf = WRLookKit::FSurf;

	inline FLinearColor OcSRGB(uint8 R, uint8 G, uint8 B) { return FLinearColor(FColor(R, G, B)); }
	// ---- the palette (cook 2.2's hex values; linear, a little under the albedo so it sits in the sun like stone does)
	inline FSurf OcLime(float K = 1.f) { return FSurf::Stone(OcSRGB(0xC9, 0xA4, 0x6A) * 0.55f * K, 0.9f); }        // coral limestone
	inline FSurf OcLimeDark(float K = 1.f) { return FSurf::Stone(OcSRGB(0x8E, 0x74, 0x4E) * 0.5f * K, 0.9f); }
	inline FSurf OcOchre(float K = 1.f) { return FSurf::Stone(OcSRGB(0xB9, 0x8A, 0x52) * 0.52f * K, 0.92f); }      // terrace walls
	inline FSurf OcSalt() { return FSurf::Stone(OcSRGB(0xF2, 0xEF, 0xE6) * 0.78f, 0.7f); }
	inline FSurf OcWetBlack() { return FSurf::Glaze(OcSRGB(0x2A, 0x2A, 0x28) * 0.5f, 0.3f); }
	inline FSurf OcBronze() { return FSurf::Steel(OcSRGB(0x8C, 0x6A, 0x3A) * 0.55f, 0.5f); }
	inline FSurf OcVerdigris() { return FSurf::Steel(OcSRGB(0x4E, 0x8A, 0x72) * 0.5f, 0.75f); }                   // sea-aged bronze
	inline FSurf OcBright() { return FSurf::Steel(OcSRGB(0xC8, 0x9A, 0x50) * 0.7f, 0.3f); }                       // fresh bronze (a new break)
	inline FSurf OcRust() { return FSurf::Steel(OcSRGB(0x6A, 0x3A, 0x22) * 0.5f, 0.85f); }
	inline FSurf OcIron() { return FSurf::Steel(OcSRGB(0x2E, 0x2A, 0x26) * 0.55f, 0.7f); }
	inline FSurf OcDrift(float K = 1.f) { return FSurf::Wood(OcSRGB(0xA8, 0xA3, 0x9A) * 0.5f * K); }             // driftwood silver
	inline FSurf OcTimber(float K = 1.f) { return FSurf::Wood(OcSRGB(0x6E, 0x54, 0x3A) * 0.48f * K); }           // ship timber
	inline FSurf OcTar() { return FSurf::Wood(OcSRGB(0x2C, 0x24, 0x1C) * 0.55f); }                                // tarred hull
	inline FSurf OcCedar() { return FSurf::Wood(OcSRGB(0x9A, 0x58, 0x36) * 0.5f); }                               // a Sunwatch barque
	inline FSurf OcShell() { return FSurf::Glaze(OcSRGB(0xEA, 0xDF, 0xC8) * 0.7f, 0.35f); }
	inline FSurf OcShellPink() { return FSurf::Glaze(OcSRGB(0xE8, 0xA0, 0xA8) * 0.6f, 0.35f); }
	inline FSurf OcShellTile() { return FSurf::Glaze(OcSRGB(0xD8, 0xC8, 0xB0) * 0.62f, 0.4f); }
	inline FSurf OcPhosphor() { return FSurf::Lit(OcSRGB(0x5A, 0xE0, 0xC8), 3.0f); }                             // phosphor paint (night)
	inline FSurf OcSandstone(float K = 1.f) { return FSurf::Stone(OcSRGB(0xC8, 0xA0, 0x70) * 0.55f * K, 0.9f); }  // the Sunwatch's stone
	inline FSurf OcGold() { return FSurf::Steel(OcSRGB(0xD8, 0xB0, 0x40) * 0.8f, 0.25f); }
	inline FSurf OcBone() { return FSurf::Stone(OcSRGB(0xE8, 0xE0, 0xCC) * 0.7f, 0.75f); }
	inline FSurf OcNet() { return FSurf::Cloth(OcSRGB(0x8C, 0x7A, 0x5A) * 0.45f); }
	inline FSurf OcSail() { return FSurf::Cloth(OcSRGB(0xD8, 0xCC, 0xAE) * 0.55f); }
	inline FSurf OcBarnacle() { return FSurf::Stone(OcSRGB(0xD8, 0xD0, 0xBE) * 0.6f, 0.95f); }
	inline FSurf OcWeed() { return FSurf::Cloth(OcSRGB(0x5A, 0x6A, 0x2E) * 0.45f); }
	inline FSurf OcClay() { return FSurf::Stone(OcSRGB(0xA8, 0x6A, 0x46) * 0.5f, 0.85f); }
	inline FSurf OcSeaGlass() { return FSurf::Glass(OcSRGB(0x7F, 0xD8, 0xD0) * 0.6f, 0.f); }
	inline FSurf OcEmber() { return FSurf::Lit(OcSRGB(0xFF, 0x9A, 0x48), 6.f); }
	inline FLinearColor OcCoralOrange() { return OcSRGB(0xE0, 0x7A, 0x4F) * 0.6f; }
	inline FLinearColor OcCoralPink() { return OcSRGB(0xE8, 0xA0, 0xA8) * 0.6f; }
	inline FLinearColor OcCoralCream() { return OcSRGB(0xEA, 0xDF, 0xC8) * 0.62f; }
	inline FLinearColor OcKelp() { return OcSRGB(0x8A, 0x7A, 0x2E) * 0.5f; }
	inline FLinearColor OcUrchin() { return OcSRGB(0x5A, 0x2E, 0x6A) * 0.5f; }
	inline FLinearColor OcRockTint() { return OcSRGB(0xC9, 0xA4, 0x6A); }

	// ---- shared Saltkin pieces ----

	/** Coursed limestone blocks along a straight run (a wall, a dyke, a terrace edge): faces Bw x Bh, staggered every
	 *  other course; collision one box. From A to B along the ground, Height tall, Thick through. */
	inline void OcCoursedWall(FBuild& B, const FVector& A, const FVector& Bt, float Height, float Thick, float Bw = 120.f, float Bh = 60.f, float K = 1.f, bool bCollide = true)
	{
		const FVector D = Bt - A;
		const float L = D.Size2D();
		if (L < 10.f) return;
		const FVector Dir = FVector(D.X, D.Y, 0.f) / L;
		const FRotator R(0.f, FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)), 0.f);
		const int32 Courses = FMath::Max(1, FMath::RoundToInt(Height / Bh));
		for (int32 c = 0; c < Courses; ++c)
		{
			const float Off = (c % 2) * Bw * 0.5f;
			for (float s = -Off; s < L; s += Bw)
			{
				const float s0 = FMath::Max(0.f, s), s1 = FMath::Min(L, s + Bw - 4.f);
				if (s1 - s0 < 8.f) continue;
				const float Z = FMath::Lerp(A.Z, Bt.Z, (s0 + s1) * 0.5f / L);
				const float Shade = 0.88f + 0.22f * B.Rand(c * 131 + int32(s));
				B.K.Add(OcLime(K * Shade), FVector(s1 - s0, Thick - 6.f * B.Rand(c + int32(s) * 7), Bh - 4.f), A + Dir * ((s0 + s1) * 0.5f) + FVector(0, 0, Z - A.Z + Bh * (c + 0.5f)), R);
			}
		}
		if (bCollide) B.SegBox(A + FVector(0, 0, Height * 0.5f), Bt + FVector(0, 0, Height * 0.5f), Thick * 0.5f, Height * 0.5f);
	}

	/** A bronze chain bollard (the Pound's rim, the flat): a squat bronze post 4 m (Scale 1) on a stepped stone plinth,
	 *  an eye cast near its top, green with age. */
	inline void OcBollard(FBuild& B, const FVector& Base, float Yaw, float Scale = 1.f, bool bCollide = true)
	{
		const FRotator R(0.f, Yaw, 0.f);
		B.K.Add(OcLime(0.9f), FVector(220.f, 220.f, 50.f) * Scale, Base + FVector(0, 0, 25.f * Scale), R);
		B.K.Add(OcLime(1.0f), FVector(170.f, 170.f, 40.f) * Scale, Base + FVector(0, 0, 70.f * Scale), R);
		B.K.Add(OcVerdigris(), FVector(110.f, 110.f, 300.f) * Scale, Base + FVector(0, 0, 240.f * Scale), R, EWRLookMesh::Cylinder);
		B.K.Add(OcBronze(), FVector(140.f, 140.f, 40.f) * Scale, Base + FVector(0, 0, 395.f * Scale), R, EWRLookMesh::Cylinder);
		B.K.Ring(OcVerdigris(), Base + R.RotateVector(FVector(62.f, 0.f, 330.f)) * Scale, 22.f * Scale, 14.f * Scale, 12.f * Scale, 10);
		if (bCollide) B.Box(Base + FVector(0, 0, 210.f * Scale), FVector(80.f, 80.f, 210.f) * Scale, R);
	}

	/** A chain of big links from P0 to P1 (each link a flattened ring, alternate links turned 90 degrees), sagging. */
	inline void OcChain(FBuild& B, const FVector& P0, const FVector& P1, float Link, float SagCm, const FSurf& S)
	{
		const float L = FVector::Dist(P0, P1);
		const int32 N = FMath::Max(2, FMath::RoundToInt(L / (Link * 0.82f)));
		for (int32 i = 0; i < N; ++i)
		{
			const float T0 = float(i) / N, T1 = float(i + 1) / N;
			const FVector A = Sag(P0, P1, T0, SagCm), Bp = Sag(P0, P1, T1, SagCm);
			const FVector D = (Bp - A).GetSafeNormal();
			const FRotator R = FRotationMatrix::MakeFromXZ(D, (i % 2) ? FVector::UpVector : FVector::CrossProduct(D, FVector::UpVector)).Rotator();
			B.K.Add(S, FVector(Link, Link * 0.36f, Link * 0.18f), (A + Bp) * 0.5f, R);
			B.K.Add(S, FVector(Link * 0.24f, Link * 0.6f, Link * 0.18f), A + D * Link * 0.12f, R);
		}
	}

	/** A shell-chime beacon (the Saltkin waymark, world-map): a driftwood pole 4 m leaning Lean degrees seaward, a cross-bar,
	 *  a hoop hung with shells and bottomless ladles on cords, phosphor paint in rings that glows at night. */
	inline void OcChimeBeacon(FBuild& B, const FVector& Base, float Yaw, float Lean, int32 Variant)
	{
		const FRotator R(0.f, Yaw, 0.f);
		const FVector Up = FRotator(-Lean, Yaw, 0.f).RotateVector(FVector(0, 0, 1)).GetSafeNormal();
		const float H = 380.f + 60.f * Variant;
		const FVector Top = Base + Up * H;
		B.Limb(Base - FVector(0, 0, 40.f), Top, 11.f, 7.f, OcSRGB(0xA8, 0xA3, 0x9A) * 0.5f, 7);
		B.K.Rod(OcDrift(0.9f), Top - Up * 40.f + R.RotateVector(FVector(0, -70.f, 0)), Top - Up * 40.f + R.RotateVector(FVector(0, 70.f, 0)), 7.f);
		// The hoop and its strings: shells (cream, pink), a ladle or two with the bottom knocked out.
		for (int32 s = 0; s < 9; ++s)
		{
			const float A = s / 9.f * 2.f * PI;
			const FVector Hang = Top - Up * 55.f + R.RotateVector(FVector(FMath::Cos(A) * 45.f, FMath::Sin(A) * 45.f, 0.f));
			const float Len = 40.f + 30.f * B.Rand(s + Variant * 13);
			B.K.Rod(OcNet(), Hang, Hang - FVector(0, 0, Len), 1.2f);
			if (s % 4 == 3) { B.K.Disc(OcBronze(), Hang - FVector(0, 0, Len + 4.f), FVector(1, 0, 0.3f), 14.f, 3.f); B.K.Rod(OcBronze(), Hang - FVector(0, 0, Len), Hang - FVector(0, 0, Len - 16.f), 2.f); }
			else B.K.Add(s % 2 ? OcShellPink() : OcShell(), FVector(12.f, 9.f, 4.f), Hang - FVector(0, 0, Len + 3.f), FRotator(70.f, A * 57.3f, 0.f), EWRLookMesh::Cone);
		}
		B.K.Rod(OcIron(), Top - Up * 55.f + FVector(-45, 0, 0), Top - Up * 55.f + FVector(45, 0, 0), 3.f);
		B.K.Rod(OcIron(), Top - Up * 55.f + FVector(0, -45, 0), Top - Up * 55.f + FVector(0, 45, 0), 3.f);
		// Phosphor paint: three rings round the pole that glow blue-green at night.
		for (int32 p = 0; p < 3; ++p) B.N.Disc(OcPhosphor(), Base + Up * (150.f + p * 70.f), Up, 26.f, 6.f);
		B.Box(Base + Up * (H * 0.5f), FVector(14.f, 14.f, H * 0.5f), FRotator(-Lean, Yaw, 0.f));
	}

	/** A tsunami stone: a 2 m dressed slab on a footing, a carved wave and one line across its face (the line is a groove
	 *  of darker stone). Older (Variant 1) leans and is weathered and lower. */
	inline void OcTsunamiStone(FBuild& B, const FVector& Base, float Yaw, bool bOld)
	{
		const FRotator R(bOld ? 6.f : 0.f, Yaw, bOld ? -4.f : 0.f);
		const float H = bOld ? 150.f : 210.f;
		B.K.Add(OcLime(0.85f), FVector(130.f, 90.f, 30.f), Base + FVector(0, 0, 10.f), FRotator(0.f, Yaw, 0.f));
		B.K.Add(OcLime(bOld ? 0.78f : 1.02f), FVector(36.f, 80.f, H), Base + R.RotateVector(FVector(0, 0, 25.f + H * 0.5f)), R);
		// The wave: three stacked arcs cut into the face; the line under them.
		const FVector Face = R.RotateVector(FVector(19.f, 0.f, 0.f));
		for (int32 w = 0; w < 3; ++w) B.K.Ring(OcLimeDark(), Base + Face + R.RotateVector(FVector(0.f, -18.f + w * 18.f, 25.f + H * 0.66f)), 8.f, 3.f, 3.f, 6, 0.f, 180.f, 0.6f);
		B.K.Add(OcLimeDark(), FVector(4.f, 64.f, 5.f), Base + Face + R.RotateVector(FVector(0, 0, 25.f + H * 0.42f)), R);
		B.Box(Base + R.RotateVector(FVector(0, 0, 25.f + H * 0.5f)), FVector(18.f, 40.f, H * 0.5f + 12.f), R);
	}

	/** A Saltkin terraced stone house (the Upper Town): walls of ochre courses, a shell-tile roof pitched low, a door to
	 *  the sea and a lookout ledge on the seaward side; Whole false = a ruin below the line (wave-scoured floor and stubs). */
	inline void OcSaltkinHouse(FBuild& B, const FVector& Base, float Yaw, bool bWhole, int32 Variant)
	{
		const FRotator R(0.f, Yaw, 0.f);
		const float W = 600.f + 80.f * (Variant % 3), Dp = 480.f, H = bWhole ? 330.f : 60.f + 50.f * (Variant % 2);
		const auto P = [&](float X, float Y, float Z) { return Base + R.RotateVector(FVector(X, Y, Z)); };
		B.K.Add(OcLime(0.8f), FVector(W + 60.f, Dp + 60.f, 30.f), P(0, 0, 0.f), R);                    // the floor slab
		for (int32 sd = 0; sd < 4; ++sd)
		{
			const bool bAlongX = sd % 2 == 0;
			const float Len = bAlongX ? W : Dp;
			const FVector C = sd == 0 ? P(0, -Dp * 0.5f, 0) : sd == 1 ? P(W * 0.5f, 0, 0) : sd == 2 ? P(0, Dp * 0.5f, 0) : P(-W * 0.5f, 0, 0);
			const FRotator Rw = R + FRotator(0.f, bAlongX ? 0.f : 90.f, 0.f);
			const float WallH = bWhole ? H : H * (0.6f + 0.6f * B.Rand(sd + Variant * 5));
			// The seaward wall (+X) has the door: two pieces either side of a 120 cm gap.
			if (sd == 1)
			{
				for (int32 s = -1; s <= 1; s += 2)
				{
					const float Seg = (Len - 120.f) * 0.5f;
					OcCoursedWall(B, C + Rw.RotateVector(FVector(s * (60.f + Seg * 0.5f) - Seg * 0.5f, 0, 15.f)), C + Rw.RotateVector(FVector(s * (60.f + Seg * 0.5f) + Seg * 0.5f, 0, 15.f)), WallH, 50.f, 90.f, 45.f, 0.9f);
				}
				if (bWhole) B.K.Add(OcTimber(0.8f), FVector(120.f, 8.f, 210.f), C + Rw.RotateVector(FVector(0, 4.f, 120.f)), Rw);
				continue;
			}
			OcCoursedWall(B, C + Rw.RotateVector(FVector(-Len * 0.5f, 0, 15.f)), C + Rw.RotateVector(FVector(Len * 0.5f, 0, 15.f)), WallH, 50.f, 90.f, 45.f, 0.9f);
		}
		if (!bWhole) return;
		// The roof: two low pitches of shell tile on timber, the ridge along X; the lookout ledge on corbels to seaward.
		for (int32 s = -1; s <= 1; s += 2)
		{
			const FRotator Rr = R + FRotator(0.f, 0.f, s * 14.f);
			B.K.Add(OcShellTile(), FVector(W + 80.f, Dp * 0.56f, 14.f), P(0, s * Dp * 0.25f, H + 70.f), Rr);
		}
		B.K.Add(OcTimber(0.7f), FVector(W + 90.f, 16.f, 16.f), P(0, 0, H + 128.f), R);
		B.K.Add(OcLime(0.95f), FVector(110.f, Dp * 0.7f, 18.f), P(W * 0.5f + 55.f, 0, H * 0.7f), R);
		for (int32 c = -1; c <= 1; ++c) B.K.Add(OcLime(0.9f), FVector(60.f, 24.f, 40.f), P(W * 0.5f + 30.f, c * Dp * 0.25f, H * 0.7f - 28.f), R);
		B.Box(P(W * 0.5f + 55.f, 0, H * 0.7f), FVector(55.f, Dp * 0.35f, 9.f), R);
		B.Box(P(0, 0, H + 70.f), FVector(W * 0.5f + 40.f, Dp * 0.5f, 18.f), R);
	}

	/** A boat hull of planks on ribs (a newcomer longboat, a castaway skiff, the Shipwright's half-built hull): Length cm,
	 *  open to the sky, lying on Base at Roll degrees, Broken 0..1 (planks missing toward one end). */
	inline void OcHull(FBuild& B, const FVector& Base, float Yaw, float Length, float Roll, float Broken, const FSurf& Plank, bool bRibsOnly = false, bool bCollide = true)
	{
		const FRotator R(0.f, Yaw, Roll);
		const float Beam = Length * 0.26f, Depth = Length * 0.1f;
		const int32 Ribs = FMath::Max(4, int32(Length / 70.f));
		for (int32 i = 0; i <= Ribs; ++i)
		{
			const float T = float(i) / Ribs;
			const float X = (T - 0.5f) * Length;
			const float Half = Beam * 0.5f * FMath::Sqrt(FMath::Max(0.f, 1.f - FMath::Square(2.f * T - 1.f) * 0.85f));
			// A rib: a U of three rods (keel side to gunwale) at this station.
			const FVector K = Base + R.RotateVector(FVector(X, 0, 12.f));
			const FVector L0 = Base + R.RotateVector(FVector(X, -Half * 0.7f, Depth * 0.35f)), L1 = Base + R.RotateVector(FVector(X, -Half, Depth));
			const FVector R0 = Base + R.RotateVector(FVector(X, Half * 0.7f, Depth * 0.35f)), R1 = Base + R.RotateVector(FVector(X, Half, Depth));
			B.K.Rod(OcTimber(0.75f), K, L0, 9.f); B.K.Rod(OcTimber(0.75f), L0, L1, 8.f);
			B.K.Rod(OcTimber(0.75f), K, R0, 9.f); B.K.Rod(OcTimber(0.75f), R0, R1, 8.f);
			if (bRibsOnly || i == Ribs) continue;
			// Strakes between this station and the next, missing toward the broken end.
			for (int32 st = 0; st < 4; ++st)
			{
				if (B.Rand(i * 17 + st) < Broken * T) continue;
				const float Z = Depth * (0.15f + st * 0.27f);
				const float Hf = Half * (0.55f + st * 0.15f);
				for (int32 s = -1; s <= 1; s += 2) B.K.Add(Plank, FVector(Length / Ribs + 6.f, 4.f, Depth * 0.27f), Base + R.RotateVector(FVector(X + Length / Ribs * 0.5f, s * Hf, Z)), R + FRotator(0.f, 0.f, s * (20.f + st * 12.f)));
			}
		}
		B.K.Rod(OcTimber(0.6f), Base + R.RotateVector(FVector(-Length * 0.5f, 0, 10.f)), Base + R.RotateVector(FVector(Length * 0.5f, 0, 10.f)), 14.f, true);    // the keel
		B.K.Rod(OcTimber(0.6f), Base + R.RotateVector(FVector(Length * 0.5f, 0, 10.f)), Base + R.RotateVector(FVector(Length * 0.56f, 0, Depth * 1.4f)), 12.f);  // the stem
		if (bCollide) B.Box(Base + R.RotateVector(FVector(0, 0, Depth * 0.5f)), FVector(Length * 0.45f, Beam * 0.42f, Depth * 0.5f), R);
	}

	/** A Saltkin harpoon: an ash shaft with a bronze toggle head and a line ring. */
	inline void OcHarpoon(FBuild& B, const FVector& Butt, const FVector& Dir, float Length, bool bRust = false)
	{
		const FVector D = Dir.GetSafeNormal();
		B.K.Rod(OcTimber(1.1f), Butt, Butt + D * Length, 5.f);
		B.K.Spike(bRust ? OcRust() : OcBronze(), Butt + D * Length, D, 10.f, 34.f);
		B.K.Add(bRust ? OcRust() : OcBronze(), FVector(26.f, 4.f, 6.f), Butt + D * (Length + 6.f), FRotationMatrix::MakeFromX(D).Rotator());
		B.K.Disc(OcIron(), Butt + D * (Length * 0.85f), D, 9.f, 3.f);
	}

	/** Giant kelp: a holdfast on the floor and a stipe rising Height cm to the surface, gas floats and blades along it,
	 *  the last 2 m lying along the surface (the canopy mat). Code mesh (organic). */
	inline void OcKelpStipe(FBuild& B, const FVector& Floor, float Height, float Yaw, int32 Seed)
	{
		const FLinearColor Col = OcKelp();
		TArray<FVector> Pts;
		const int32 Segs = FMath::Clamp(int32(Height / 250.f), 3, 14);
		const FVector Drift = FRotator(0.f, Yaw, 0.f).Vector();
		for (int32 i = 0; i <= Segs; ++i)
		{
			const float T = float(i) / Segs;
			const float Sway = 60.f * FMath::Sin(T * 3.f + Seed) * T;
			Pts.Add(Floor + FVector(0, 0, Height * T) + Drift * (Sway + 120.f * T * T));
		}
		// The canopy: the stipe lies along the surface for 2 m.
		Pts.Add(Pts.Last() + Drift * 200.f + FVector(0, 0, 10.f));
		B.M.Tube(Pts, 3.f, 5, Col, 0.6f);
		B.Ball(Floor + FVector(0, 0, 12.f), 22.f, OcKelp() * 0.6f, 0.9f, 0.5f);                          // the holdfast
		for (int32 i = 1; i < Pts.Num(); ++i)
		{
			const FVector P = Pts[i];
			B.Ball(P, 6.f, Col * 1.15f, 0.4f);                                                               // a gas float
			const FVector Side = FRotator(0.f, Yaw + 90.f + (i % 2) * 180.f, 0.f).Vector();
			B.M.Lathe(P, (Side + FVector(0, 0, 0.4f)).GetSafeNormal(), 150.f, { FVector(0.f, 1.f, 1.f), FVector(0.4f, 22.f, 2.f), FVector(1.f, 4.f, 1.f) }, 4, Col * 1.05f, 0.5f);
		}
	}

	/** A coral head: Variant 0 branching (staghorn), 1 brain (a ridged dome), 2 table (a plate on a stalk), 3 pillar, 4 fan,
	 *  5 a knot of finger coral. Colour by its seed among the reef's orange, pink and cream (tidecoral alive). */
	inline void OcCoralHead(FBuild& B, const FVector& Base, float Scale, int32 Variant, int32 Seed)
	{
		const FLinearColor Cols[3] = { OcCoralOrange(), OcCoralPink(), OcCoralCream() };
		const FLinearColor C = Cols[(Seed + Variant) % 3];
		const float S = 100.f * Scale;
		switch (Variant % 6)
		{
		case 0:
			for (int32 b = 0; b < 7; ++b)
			{
				const FVector Dir = FRotator(35.f + 30.f * B.Rand(b + Seed), b * 51.f + Seed, 0.f).Vector();
				const FVector Mid = Base + Dir * S * 0.6f;
				B.Limb(Base, Mid, 7.f * Scale, 5.f * Scale, C, 6);
				B.Limb(Mid, Mid + (Dir + FVector(0, 0, 0.6f)).GetSafeNormal() * S * 0.5f, 5.f * Scale, 2.f * Scale, C * 1.1f, 6);
			}
			break;
		case 1:
			B.Ball(Base + FVector(0, 0, S * 0.3f), S * 0.55f, C, 0.85f, 0.6f);
			for (int32 g = 0; g < 5; ++g) B.Limb(Base + FVector(-S * 0.4f, (g - 2) * S * 0.18f, S * 0.55f), Base + FVector(S * 0.4f, (g - 2) * S * 0.18f + S * 0.08f, S * 0.55f), 3.f * Scale, 3.f * Scale, C * 0.8f, 5);
			break;
		case 2:
			B.Limb(Base, Base + FVector(0, 0, S * 0.7f), 10.f * Scale, 7.f * Scale, C * 0.9f, 6);
			B.M.Lathe(Base + FVector(0, 0, S * 0.7f), FVector::UpVector, 12.f * Scale, { FVector(0.f, S * 0.9f, S * 0.8f), FVector(1.f, S * 1.0f, S * 0.9f) }, 12, C, 0.8f);
			break;
		case 3:
			for (int32 p = 0; p < 4; ++p) B.Limb(Base + FVector((p - 1.5f) * 12.f * Scale, (p % 2) * 14.f * Scale, 0.f), Base + FVector((p - 1.5f) * 14.f * Scale, (p % 2) * 16.f * Scale, S * (0.8f + 0.5f * B.Rand(p + Seed))), 9.f * Scale, 6.f * Scale, C, 7);
			break;
		case 4:
			for (int32 r = 0; r < 9; ++r) B.Limb(Base, Base + FRotator(10.f + r * 18.f, Seed * 13.f, 0.f).RotateVector(FVector(0, S * 0.9f, 0)).RotateAngleAxis(90.f, FVector(1, 0, 0)), 3.f * Scale, 1.f * Scale, OcCoralPink() * 0.9f, 4);
			break;
		default:
			for (int32 f = 0; f < 12; ++f) B.Limb(Base + FVector(FMath::Cos(float(f)) * S * 0.2f, FMath::Sin(float(f)) * S * 0.2f, 0.f), Base + FVector(FMath::Cos(float(f)) * S * 0.3f, FMath::Sin(float(f)) * S * 0.3f, S * (0.3f + 0.3f * B.Rand(f + Seed))), 4.f * Scale, 3.f * Scale, C * 1.05f, 5);
			break;
		}
	}

	/** A giant clam (1-2 m): two fluted valves gaping a little, the mantle glowing blue-green at the lip. */
	inline void OcClam(FBuild& B, const FVector& Base, float Yaw, float Scale)
	{
		const FRotator R(0.f, Yaw, 0.f);
		const float S = 50.f * Scale;
		for (int32 v = -1; v <= 1; v += 2)
		{
			const FRotator Rv = R + FRotator(0.f, 0.f, v * 22.f);
			B.M.Lathe(Base + Rv.RotateVector(FVector(-S, v * S * 0.25f, S * 0.4f)), Rv.RotateVector(FVector(1, 0, 0)), 2.f * S,
				{ FVector(0.f, 4.f, 4.f), FVector(0.25f, S * 0.7f, S * 0.35f), FVector(0.5f, S * 0.85f, S * 0.42f), FVector(0.75f, S * 0.7f, S * 0.35f), FVector(1.f, 4.f, 4.f) }, 10, OcSRGB(0xCF, 0xC6, 0xB0) * 0.6f, 0.8f);
		}
		B.N.Add(FSurf::Lit(OcSRGB(0x3A, 0xC8, 0xB8), 2.f), FVector(S * 1.7f, S * 0.2f, S * 0.12f), Base + FVector(0, 0, S * 0.75f), R);
		B.K.Add(FSurf::Glaze(OcSRGB(0x2E, 0x6E, 0x8A) * 0.6f, 0.3f), FVector(S * 1.6f, S * 0.22f, S * 0.1f), Base + FVector(0, 0, S * 0.72f), R);
	}

	/** A bronze brazier bowl on a stone post (the Saltkin's, every shrine): an ember bed, the flame lit at night. */
	inline void OcBrazier(FBuild& B, const FVector& Base, float PostH, bool bSeaGreen)
	{
		B.K.Add(OcLime(0.95f), FVector(50.f, 50.f, PostH), Base + FVector(0, 0, PostH * 0.5f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		B.K.Add(OcVerdigris(), FVector(110.f, 110.f, 30.f), Base + FVector(0, 0, PostH + 15.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		B.K.Add(OcEmber(), FVector(80.f, 80.f, 8.f), Base + FVector(0, 0, PostH + 28.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		B.Loop(bSeaGreen ? FName(TEXT("ocean.whale_oil_flame")) : FName(TEXT("brazier.fire")), Base + FVector(0, 0, PostH + 34.f));
		B.Lamp(Base + FVector(0, 0, PostH + 90.f), bSeaGreen ? OcSRGB(0x7A, 0xF0, 0xD8) : OcSRGB(0xFF, 0xB0, 0x60), 2600.f, 1600.f);
		B.Box(Base + FVector(0, 0, (PostH + 30.f) * 0.5f), FVector(40.f, 40.f, (PostH + 30.f) * 0.5f));
	}
}
