#pragma once

// The Sky islands' scenery kit (cooks/biome-sky.md 2.1 palette, 2.2 sub-zone palettes, 2.3-2.4; cooks/build-biome-sky.md
// 2.3-2.5): the Kitewrights' materials and the pieces every Sky place shares, on the forest's FBuild (one site's build in
// its actor's local space). Private to the Sky's scenery files; every function is inline (unity builds join them).
//
// Real reference: lung ta wind-horse flags (5 colours, faded, on 4.5 m poles), Hebridean blackhouses and Mediterranean
// round tholos houses (the round houses: a dry limestone drum, a conical roof), the Luding / Ji Hong chain bridges
// (decks on iron chains between stone tower-houses), Kitewright kites as Japanese rokkaku and Guatemalan barrilete
// death-kites (paper and cloth on bamboo frames, flown over graves).

#include "CoreMinimal.h"
#include "WRForestSceneryKit.h"
#include "WRSkyIsleTypes.h"

namespace WRSkyLook
{
	using FSurf = WRLookKit::FSurf;
	using WRForestLook::FBuild;

	/** sRGB hex parts to a linear colour (the cook's palette is sRGB). */
	inline FLinearColor SRGB(uint8 R, uint8 G, uint8 B) { return FLinearColor(FColor(R, G, B)); }

	// ---- surfaces (dimmed as the build kit's tinted blocks want them) ----
	inline FSurf Limestone(float K = 1.f) { return FSurf::Stone(SRGB(0xE9, 0xE4, 0xD8) * 0.62f * K, 0.9f); }
	inline FSurf TempleWhite(float K = 1.f) { return FSurf::Stone(SRGB(0xF0, 0xED, 0xE6) * 0.68f * K, 0.8f); }
	inline FSurf Granite(float K = 1.f) { return FSurf::Stone(SRGB(0xB7, 0xB3, 0xAA) * 0.55f * K, 0.88f); }
	inline FSurf CliffGranite(float K = 1.f) { return FSurf::Stone(SRGB(0x9A, 0x96, 0x90) * 0.55f * K, 0.88f); }
	inline FSurf WetStone(float K = 1.f) { return FSurf::Stone(SRGB(0x6E, 0x73, 0x78) * 0.55f * K, 0.55f); }
	inline FSurf JadeTile(float K = 1.f) { return FSurf::Glaze(SRGB(0x2F, 0x8F, 0x7A) * 0.55f * K, 0.35f); }
	inline FSurf Cedar(float K = 1.f) { return FSurf::Wood(SRGB(0x6B, 0x54, 0x42) * 0.5f * K); }
	inline FSurf Plank(float K = 1.f) { return FSurf::Wood(SRGB(0x8E, 0x86, 0x79) * 0.48f * K); }
	inline FSurf Shingle(float K = 1.f) { return FSurf::Wood(SRGB(0x5E, 0x52, 0x46) * 0.5f * K); }
	inline FSurf ChainIron() { return FSurf::Steel(SRGB(0x5C, 0x4A, 0x3C) * 0.5f, 0.75f); }
	inline FSurf RustIron() { return FSurf::Steel(SRGB(0x7A, 0x4A, 0x2E) * 0.5f, 0.85f); }
	inline FSurf Bronze() { return FSurf::Steel(SRGB(0x9A, 0x74, 0x3E) * 0.6f, 0.45f); }
	inline FSurf Bone() { return FSurf::Stone(SRGB(0xD8, 0xD2, 0xC4) * 0.7f, 0.8f); }
	inline FSurf Paper() { return FSurf::Cloth(SRGB(0xB9, 0xB9, 0xAE) * 0.55f); }
	inline FSurf Skysilk() { return FSurf::Cloth(SRGB(0xE8, 0xEE, 0xF2) * 0.7f); }
	inline FSurf Teal() { return FSurf::Cloth(SRGB(0x9F, 0xD6, 0xCF) * 0.6f); }
	inline FSurf Rope() { return FSurf::Cloth(SRGB(0xA8, 0x94, 0x6E) * 0.5f); }
	inline FSurf Wax() { return FSurf::Glaze(SRGB(0xD8, 0xB2, 0x6A) * 0.55f, 0.4f); }
	inline FSurf Ember(float Glow) { return FSurf::Lit(SRGB(0xFF, 0x9A, 0x3C) * 0.7f, Glow); }
	inline FSurf LampGlow(float Glow) { return FSurf::Lit(SRGB(0xFF, 0xC8, 0x7A) * 0.7f, Glow); }
	inline FSurf Levistone(float Glow) { return FSurf::Glass(SRGB(0xA9, 0xC4, 0xD9) * 0.6f, Glow); }
	inline FSurf LiftGlow(float Glow) { return FSurf::Lit(SRGB(0xCF, 0xE9, 0xFF) * 0.7f, Glow); }
	inline FSurf Spore(float Glow) { return FSurf::Lit(SRGB(0x8A, 0x8A, 0x80) * 0.55f, Glow); }
	inline FSurf SporeGlow(float Glow) { return FSurf::Lit(SRGB(0x39, 0xE6, 0xC2) * 0.6f, Glow); }
	inline FSurf SailViolet(float Glow) { return FSurf::Lit(SRGB(0x8A, 0x6B, 0xB0) * 0.6f, Glow); }
	inline FSurf LagoonWater() { return FSurf::Glaze(SRGB(0x4E, 0x7A, 0x8A) * 0.45f, 0.04f); }
	inline FSurf Turf() { return FSurf::Cloth(SRGB(0x8F, 0xA7, 0x7A) * 0.45f); }
	inline FSurf Flax() { return FSurf::Cloth(SRGB(0x6F, 0x8F, 0xC9) * 0.5f); }
	inline FSurf Berry() { return FSurf::Cloth(SRGB(0xE0, 0xA0, 0x40) * 0.5f); }
	/** Lung ta cloth, faded (the cook's five: red, yellow, green, blue, and white). */
	inline FSurf Cloth(int32 i, float K = 1.f)
	{
		switch (((i % 5) + 5) % 5)
		{
		case 0: return FSurf::Cloth(SRGB(0x4F, 0x7F, 0xAF) * 0.55f * K);
		case 1: return FSurf::Cloth(SRGB(0xE8, 0xE6, 0xDE) * 0.6f * K);
		case 2: return FSurf::Cloth(SRGB(0xB5, 0x52, 0x3B) * 0.55f * K);
		case 3: return FSurf::Cloth(SRGB(0x6E, 0x9A, 0x5A) * 0.55f * K);
		default: return FSurf::Cloth(SRGB(0xD8, 0xB2, 0x4A) * 0.55f * K);
		}
	}
	/** The zone's rock (the island rims, the cave walls). */
	inline FLinearColor RimColour(TCHAR Zone, float Fine)
	{
		switch (Zone)
		{
		case '2': return FMath::Lerp(SRGB(0xE9, 0xE4, 0xD8), SRGB(0xC9, 0xC2, 0xB0), Fine) * 0.85f;   // pale limestone
		case '3': return FMath::Lerp(SRGB(0xB7, 0xB3, 0xAA), SRGB(0x8E, 0x8A, 0x80), Fine) * 0.85f;   // pale granite
		case '4': return FMath::Lerp(SRGB(0xF0, 0xED, 0xE6), SRGB(0xC8, 0xC4, 0xBA), Fine) * 0.8f;    // temple-white rock
		case '5': return FMath::Lerp(SRGB(0x6E, 0x73, 0x78), SRGB(0x56, 0x5A, 0x5E), Fine);          // wet grey
		default: return FMath::Lerp(SRGB(0x9A, 0x96, 0x90), SRGB(0xB0, 0xAC, 0xA4), Fine) * 0.85f;   // cliff granite
		}
	}
	inline FSurf RimStone(TCHAR Zone, float K = 1.f) { return FSurf::Stone(RimColour(Zone, 0.4f) * 0.62f * K, 0.88f); }
	inline FLinearColor ChainC() { return SRGB(0x5C, 0x4A, 0x3C) * 0.5f; }
	inline FLinearColor RopeC() { return SRGB(0xA8, 0x94, 0x6E) * 0.5f; }
	inline FLinearColor CedarC() { return SRGB(0x6B, 0x54, 0x42) * 0.5f; }
	inline FLinearColor BoneC() { return SRGB(0xD8, 0xD2, 0xC4) * 0.7f; }

	// ---- shared pieces ----

	/** A chain between two points, as linked iron where it's short and near (Limit links), a heavy rope-like run beyond. */
	inline void Chain(FBuild& B, const FVector& P0, const FVector& P1, float SagCm, float LinkCm = 22.f, int32 Limit = 60)
	{
		const float L = FVector::Dist(P0, P1);
		const int32 N = FMath::Max(2, int32(L / LinkCm));
		if (N > Limit)
		{
			B.Line(P0, P1, LinkCm * 0.16f, SagCm, ChainC(), FMath::Clamp(int32(L / 300.f), 8, 40));
			return;
		}
		for (int32 i = 0; i < N; ++i)
		{
			const FVector A = WRForestLook::Sag(P0, P1, float(i) / N, SagCm), C = WRForestLook::Sag(P0, P1, float(i + 1) / N, SagCm);
			const FVector D = (C - A).GetSafeNormal();
			const FRotator R = FRotationMatrix::MakeFromXZ(D, (i % 2) ? FVector::UpVector : FVector::CrossProduct(D, FVector::UpVector).GetSafeNormal()).Rotator();
			B.K.Add(ChainIron(), FVector(LinkCm * 1.15f, LinkCm * 0.18f, LinkCm * 0.55f), (A + C) * 0.5f, R);
		}
	}

	/** A lung ta wind-pole (SKY 2.4): a 4.5 m cedar pole on a cairn, guy strings of five-colour flags to three stakes, and
	 *  a long streamer on a swivel at the top that always points downwind (the actor's Cloths turn it). */
	inline void WindPole(FBuild& B, const FVector& Base, int32 Var, bool bCairn = true)
	{
		const float H = 450.f;
		if (bCairn)
		{
			for (int32 k = 0; k < 7; ++k)
			{
				const float A = k * 0.9f + B.Rand(k);
				const FVector P = Base + FVector(FMath::Cos(A) * (40.f - k * 4.f), FMath::Sin(A) * (40.f - k * 4.f), 15.f + k * 14.f);
				B.Ball(P, 20.f - k * 1.2f, SRGB(0xB7, 0xB3, 0xAA) * (0.5f + 0.1f * B.Rand(k + 9)), 0.9f, 0.6f);
			}
		}
		B.K.Rod(Cedar(0.9f), Base, Base + FVector(0, 0, H), 12.f);
		B.K.Disc(Bronze(), Base + FVector(0, 0, H + 4.f), FVector::UpVector, 18.f, 8.f);
		B.Box(Base + FVector(0, 0, H * 0.5f), FVector(10.f, 10.f, H * 0.5f));
		// Guy strings of flags (each string's flags hang along its sag, faded).
		for (int32 g = 0; g < 3; ++g)
		{
			const float A = (g * 120.f + Var * 37.f) * PI / 180.f;
			const FVector Stake = Base + FVector(FMath::Cos(A) * 520.f, FMath::Sin(A) * 520.f, 0.f);
			const FVector StakeG = FVector(Stake.X, Stake.Y, B.G(Stake.X, Stake.Y, Base.Z));
			B.K.Rod(Cedar(0.8f), StakeG - FVector(0, 0, 20.f), StakeG + FVector(0, 0, 50.f), 6.f);
			const FVector Top = Base + FVector(0, 0, H - 10.f);
			const FVector Lo = StakeG + FVector(0, 0, 45.f);
			B.Line(Top, Lo, 0.6f, 40.f, RopeC(), 10);
			for (int32 f = 1; f < 12; ++f)
			{
				const float T = f / 12.f;
				const FVector P = WRForestLook::Sag(Top, Lo, T, 40.f);
				const FVector Along = (Lo - Top).GetSafeNormal();
				B.K.Add(Cloth(f + g, 0.85f + 0.15f * B.Rand(f * 7 + g)), FVector(26.f, 1.5f, 30.f), P - FVector(0, 0, 16.f), FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(Along.Y, Along.X)), 0.f));
			}
		}
	}

	/** The streamer at a pole's top (its own component: the actor turns it downwind and flaps it). Local: X along it. */
	inline void Streamer(FBuild& B, int32 Var)
	{
		for (int32 s = 0; s < 5; ++s)
		{
			const float X = 30.f + s * 52.f;
			B.K.Add(Cloth(s + Var, 0.9f), FVector(50.f, 2.f, 46.f - s * 5.f), FVector(X, 0, -30.f), FRotator(0.f, 0.f, (s % 2) ? 4.f : -4.f));
		}
		B.K.Rod(Bronze(), FVector(0, 0, -5.f), FVector(8.f, 0, -5.f), 6.f);
	}

	/** A Kitewright round house (SKY 2.4 kit): a dry limestone drum 7 m across and 3 m high on a plinth, a conical cedar-
	 *  shingle roof on rafters with a smoke hole, the door to the lee (-X: the scenery yaws it downwind), small deep
	 *  windows, a kite rack on the wall. Var 0 roofed, 1 roofless, 2 toppled half over (the scenery tips it), 3 two-storey. */
	inline void RoundHouse(FBuild& B, const FVector& C, int32 Var, bool bLit = false)
	{
		const float R = 350.f, H = Var == 3 ? 560.f : 300.f;
		const int32 Courses = int32(H / 30.f);
		B.K.Disc(Limestone(0.85f), C + FVector(0, 0, 10.f), FVector::UpVector, 2.f * R + 60.f, 30.f);
		for (int32 c = 0; c < Courses; ++c)
		{
			const int32 Stones = 22;
			for (int32 s = 0; s < Stones; ++s)
			{
				const float A = (s + (c % 2) * 0.5f) * 2.f * PI / Stones;
				const float Ad = FMath::RadiansToDegrees(A);
				// The door gap (-X) and two windows (+-Y) at sill to lintel height.
				const float Rel = FMath::Abs(FMath::FindDeltaAngleDegrees(Ad, 180.f));
				if (Rel < 14.f && c < 7) continue;
				const float RelW = FMath::Min(FMath::Abs(FMath::FindDeltaAngleDegrees(Ad, 90.f)), FMath::Abs(FMath::FindDeltaAngleDegrees(Ad, 270.f)));
				if (RelW < 8.f && c >= 3 && c < 6) continue;
				// A roofless ruin's wall breaks down unevenly toward the top.
				if (Var == 1 && c > Courses - 4 && B.Rand(c * 31 + s) < 0.5f + (c - (Courses - 4)) * 0.15f) continue;
				const FVector P = C + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 25.f + c * 30.f + 15.f);
				B.K.Add(Limestone(0.85f + 0.2f * B.Rand(c * 22 + s)), FVector(2.f * PI * R / Stones - 4.f, 50.f, 28.f), P, FRotator(0.f, Ad + 90.f, 0.f));
			}
		}
		// The wall's collision: a ring of boxes with the door open.
		for (int32 s = 0; s < 12; ++s)
		{
			const float Ad = s * 30.f + 15.f;
			if (FMath::Abs(FMath::FindDeltaAngleDegrees(Ad, 180.f)) < 20.f) continue;
			const float A = FMath::DegreesToRadians(Ad);
			B.Box(C + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 25.f + H * 0.5f), FVector(R * 0.27f, 28.f, H * 0.5f), FRotator(0.f, Ad + 90.f, 0.f));
		}
		// Door lintel and jambs (dressed white stone).
		B.K.Add(TempleWhite(0.9f), FVector(40.f, 160.f, 30.f), C + FVector(-R, 0, 25.f + 7 * 30.f + 15.f), FRotator::ZeroRotator);
		if (Var == 3)
		{
			// The floor between storeys on cedar joists, and a stair up the inside wall.
			B.K.Disc(Plank(0.9f), C + FVector(0, 0, 300.f), FVector::UpVector, 2.f * R - 40.f, 12.f);
			B.Box(C + FVector(80.f, 0, 300.f), FVector(R - 120.f, R - 40.f, 6.f));
			for (int32 st = 0; st < 10; ++st)
			{
				const float A = FMath::DegreesToRadians(20.f + st * 13.f);
				const FVector P = C + FVector(FMath::Cos(A) * (R - 70.f), FMath::Sin(A) * (R - 70.f), 30.f + st * 28.f);
				B.K.Add(Limestone(0.8f), FVector(60.f, 90.f, 20.f), P, FRotator(0.f, FMath::RadiansToDegrees(A) + 90.f, 0.f));
				B.Box(P, FVector(30.f, 45.f, 10.f), FRotator(0.f, FMath::RadiansToDegrees(A) + 90.f, 0.f));
			}
		}
		if (Var != 1)
		{
			// The roof: 16 cedar rafters to a ring at the smoke hole, shingles in courses on them.
			const FVector Apex = C + FVector(0, 0, 25.f + H + 280.f);
			for (int32 r = 0; r < 16; ++r)
			{
				const float A = r * 2.f * PI / 16.f;
				const FVector Foot = C + FVector(FMath::Cos(A) * (R + 50.f), FMath::Sin(A) * (R + 50.f), 25.f + H);
				B.K.Rod(Cedar(0.8f), Foot, FMath::Lerp(Foot, Apex, 0.9f), 12.f, true);
			}
			for (int32 c = 0; c < 7; ++c)
			{
				const float T = (c + 0.5f) / 7.f;
				const float Rr = FMath::Lerp(R + 50.f, 45.f, T);
				const float Z = 25.f + H + 280.f * T;
				for (int32 s = 0; s < 18; ++s)
				{
					const float A = (s + (c % 2) * 0.5f) * 2.f * PI / 18.f;
					const FVector P = C + FVector(FMath::Cos(A) * Rr, FMath::Sin(A) * Rr, Z + 8.f);
					B.K.Add(Shingle(0.85f + 0.25f * B.Rand(c * 18 + s)), FVector(2.f * PI * Rr / 18.f + 6.f, 46.f, 4.f), P, FRotator(-38.f, FMath::RadiansToDegrees(A) + 90.f, 0.f) );
				}
			}
			B.Box(C + FVector(0, 0, 25.f + H + 120.f), FVector(R * 0.7f, R * 0.7f, 120.f));
		}
		// A kite rack on the outside wall: three frames hung, cloth faded.
		for (int32 k = 0; k < 3; ++k)
		{
			const float A = FMath::DegreesToRadians(40.f + k * 22.f);
			const FVector P = C + FVector(FMath::Cos(A) * (R + 30.f), FMath::Sin(A) * (R + 30.f), 170.f);
			B.K.Add(Cloth(k * 2 + B.Seed, 0.8f), FVector(4.f, 80.f, 110.f), P, FRotator(0.f, FMath::RadiansToDegrees(A), 45.f));
			B.K.Rod(Cedar(0.9f), P - FVector(0, 0, 60.f), P + FVector(0, 0, 60.f), 3.f);
		}
		if (bLit)
		{
			B.N.Add(LampGlow(4.f), FVector(20.f, 60.f, 50.f), C + FVector(FMath::Cos(PI * 0.5f) * (R + 2.f), FMath::Sin(PI * 0.5f) * (R + 2.f), 140.f), FRotator(0.f, 90.f, 0.f));
			B.Lamp(C + FVector(0, 0, 160.f), SRGB(0xFF, 0xC8, 0x7A), 1600.f, 900.f);
		}
	}

	/** A diamond (rokkaku-ish) Kitewright kite: a cedar cross frame, cloth in two faded colours, a bone tube at its foot. */
	inline void Kite(FBuild& B, const FVector& C, float Size, float Yaw, float Tilt, int32 Var, bool bRotted)
	{
		const FRotator R(Tilt, Yaw, 0.f);
		const FVector Up = R.RotateVector(FVector(0, 0, 1)), Side = R.RotateVector(FVector(0, 1, 0));
		B.K.Rod(Cedar(0.8f), C - Up * Size * 0.6f, C + Up * Size * 0.6f, 3.f);
		B.K.Rod(Cedar(0.8f), C - Side * Size * 0.45f + Up * Size * 0.15f, C + Side * Size * 0.45f + Up * Size * 0.15f, 3.f);
		if (!bRotted || B.Rand(Var) > 0.4f)
		{
			for (int32 h = 0; h < 2; ++h)
			{
				const FVector Mid = C + Side * Size * 0.22f * (h ? 1.f : -1.f) + Up * Size * 0.05f;
				B.K.Add(Cloth(Var + h * 2, bRotted ? 0.65f : 0.95f), FVector(1.5f, Size * 0.44f, Size * (bRotted ? 0.6f : 1.05f)), Mid, R);
			}
		}
	}

	/** A brazier on a stone foot: a bronze bowl, its fire lit at night (a real light near the viewer). */
	inline void Brazier(FBuild& B, const FVector& Base, float Power = 2600.f)
	{
		B.K.Add(TempleWhite(0.85f), FVector(50.f, 50.f, 90.f), Base + FVector(0, 0, 45.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		B.K.Add(Bronze(), FVector(90.f, 90.f, 30.f), Base + FVector(0, 0, 105.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		B.N.Add(Ember(8.f), FVector(64.f, 64.f, 26.f), Base + FVector(0, 0, 120.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		B.N.Add(Ember(10.f), FVector(30.f, 30.f, 50.f), Base + FVector(0, 0, 140.f), FRotator::ZeroRotator, EWRLookMesh::Cone);
		B.Lamp(Base + FVector(0, 0, 170.f), SRGB(0xFF, 0xA8, 0x5C), Power, 1800.f);
		B.Box(Base + FVector(0, 0, 60.f), FVector(30.f, 30.f, 60.f));
	}

	/** A bone tube (the Kitewrights' grave letter): bone with wax seals, a rolled kite inside. */
	inline void BoneTube(FBuild& B, const FVector& At, float Yaw, float Tilt = 0.f)
	{
		const FRotator R(Tilt, Yaw, 0.f);
		const FVector Ax = R.RotateVector(FVector(1, 0, 0));
		B.K.Rod(Bone(), At - Ax * 22.f, At + Ax * 22.f, 9.f);
		B.K.Disc(Wax(), At - Ax * 23.f, Ax, 10.f, 3.f);
		B.K.Disc(Wax(), At + Ax * 23.f, Ax, 10.f, 3.f);
	}

	/** A Kitewright stone tower-house anchoring a bridge's chains (SKY W3): a square tower 5 x 5 m, 9 m high, chain eyes. */
	inline void TollTower(FBuild& B, const FVector& Base, const FVector& Along, TCHAR Zone, bool bBroken)
	{
		const FRotator Yaw(0.f, FMath::RadiansToDegrees(FMath::Atan2(Along.Y, Along.X)), 0.f);
		const float H = bBroken ? 560.f : 900.f;
		for (int32 c = 0; c < int32(H / 45.f); ++c)
		{
			for (int32 sd = 0; sd < 4; ++sd)
			{
				// An arch through the tower along the bridge (the deck passes under it).
				if ((sd == 1 || sd == 3) && c < 7) continue;
				const FRotator Ry = Yaw + FRotator(0.f, sd * 90.f, 0.f);
				const FVector P = Base + Ry.RotateVector(FVector(230.f, 0.f, 0.f)) + FVector(0, 0, 22.f + c * 45.f);
				B.K.Add(RimStone(Zone, 0.95f + 0.15f * B.Rand(c * 4 + sd)), FVector(50.f, 500.f, 44.f), P, Ry);
			}
		}
		for (int32 sd = 0; sd < 4; sd += 2)
		{
			const FRotator Ry = Yaw + FRotator(0.f, sd * 90.f, 0.f);
			B.Box(Base + Ry.RotateVector(FVector(230.f, 0.f, 0.f)) + FVector(0, 0, H * 0.5f), FVector(25.f, 250.f, H * 0.5f), Ry);
		}
		for (int32 sd = 1; sd < 4; sd += 2)
		{
			const FRotator Ry = Yaw + FRotator(0.f, sd * 90.f, 0.f);
			B.Box(Base + Ry.RotateVector(FVector(230.f, 0.f, 0.f)) + FVector(0, 0, 315.f + (H - 315.f) * 0.5f), FVector(25.f, 250.f, (H - 315.f) * 0.5f), Ry);
		}
		// Chain eyes on the bridge face, a brazier on top (lit at night: L6's towers).
		for (int32 e = -1; e <= 1; e += 2) B.K.Disc(RustIron(), Base + Yaw.RotateVector(FVector(255.f, e * 180.f, 120.f)), Along, 26.f, 8.f);
		if (!bBroken) Brazier(B, Base + FVector(0, 0, H), 3200.f);
	}
}
