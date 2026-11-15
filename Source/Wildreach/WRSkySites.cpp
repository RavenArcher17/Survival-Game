// The Sky's places (cooks/biome-sky.md 2.4 hand sites H1-H16 and kit, 2.7 the Undercloud U1-U8, 2.13 shrine settings,
// 2.14 the story the land tells; world-map camps). Each place is built round its one idea; nothing says it out loud.
// Local space: the actor at the site's point, +X its yaw.

#include "WRSkySceneryParts.h"
#include "WRSkyWorld.h"
#include "WRTuning.h"
#include "Engine/World.h"

using namespace WRSkyLook;

namespace WRSkySitesImpl
{
	using WRSkyParts::FCtx;

	/** A skeleton lying (or sitting, or kneeling) as it fell: skull, ribs, spine, the long bones, the hands. */
	void Skeleton(FCtx& C, const FVector& Pelvis, float Yaw, int32 Pose, float Lean = 0.f)
	{
		const FRotator R(Lean, Yaw, 0.f);
		const auto P = [&](float X, float Y, float Z) { return Pelvis + R.RotateVector(FVector(X, Y, Z)); };
		const FLinearColor Bc = BoneC();
		if (Pose == 0)
		{
			// Lying: head toward +X.
			C.B.Ball(P(62, 0, 10), 11.f, Bc, 0.8f);
			C.B.Limb(P(0, 0, 6), P(50, 0, 8), 3.f, 3.f, Bc, 5);
			for (int32 r = 0; r < 5; ++r) C.B.Limb(P(26 + r * 5, -14, 8), P(26 + r * 5, 14, 8), 1.6f, 1.6f, Bc, 4);
			C.B.Limb(P(0, -8, 4), P(-44, -12, 4), 2.6f, 2.f, Bc, 5);
			C.B.Limb(P(0, 8, 4), P(-44, 14, 4), 2.6f, 2.f, Bc, 5);
			C.B.Limb(P(-44, -12, 4), P(-86, -14, 3), 2.f, 1.6f, Bc, 5);
			C.B.Limb(P(-44, 14, 4), P(-86, 18, 3), 2.f, 1.6f, Bc, 5);
			C.B.Limb(P(48, -16, 6), P(20, -26, 4), 1.8f, 1.6f, Bc, 4);
			C.B.Limb(P(48, 16, 6), P(22, 24, 4), 1.8f, 1.6f, Bc, 4);
		}
		else if (Pose == 1)
		{
			// Sitting at a table (the Gardener's Grave): skull bowed, arms on the board.
			C.B.Limb(P(0, 0, 0), P(4, 0, 50), 3.f, 3.f, Bc, 5);
			C.B.Ball(P(12, 0, 62), 11.f, Bc, 0.8f);
			for (int32 r = 0; r < 5; ++r) C.B.Limb(P(0, -14, 22 + r * 5), P(10, -12, 22 + r * 5), 1.6f, 1.6f, Bc, 4);
			C.B.Limb(P(0, -9, 0), P(40, -10, 0), 2.6f, 2.f, Bc, 5);
			C.B.Limb(P(0, 9, 0), P(40, 10, 0), 2.6f, 2.f, Bc, 5);
			C.B.Limb(P(40, -10, 0), P(40, -10, -42), 2.f, 1.6f, Bc, 5);
			C.B.Limb(P(40, 10, 0), P(40, 10, -42), 2.f, 1.6f, Bc, 5);
			C.B.Limb(P(4, -16, 44), P(34, -14, 30), 1.8f, 1.6f, Bc, 4);
			C.B.Limb(P(4, 16, 44), P(34, 14, 30), 1.8f, 1.6f, Bc, 4);
		}
		else
		{
			// Face-down at a bed (the Spore Scar's gardeners), a cloth mask still tied over the skull.
			C.B.Ball(P(60, 0, 8), 11.f, Bc, 0.8f);
			C.B.K.Add(FSurf::Cloth(SRGB(0xC8, 0xC0, 0xA8) * 0.5f), FVector(10.f, 24.f, 14.f), P(70, 0, 8), R);
			C.B.Limb(P(0, 0, 4), P(50, 0, 6), 3.f, 3.f, Bc, 5);
			C.B.Limb(P(0, -8, 3), P(-40, -6, 3), 2.6f, 2.f, Bc, 5);
			C.B.Limb(P(0, 8, 3), P(-40, 10, 3), 2.6f, 2.f, Bc, 5);
			C.B.Limb(P(46, -16, 5), P(80, -30, 3), 1.8f, 1.6f, Bc, 4);
			C.B.Limb(P(46, 16, 5), P(84, 28, 3), 1.8f, 1.6f, Bc, 4);
			C.B.K.Add(FSurf::Cloth(SRGB(0x8A, 0x8A, 0x80) * 0.45f), FVector(90.f, 50.f, 6.f), P(20, 0, 10), R);   // grey crust over the back
		}
	}
	/** A broken glider frame staked upright like a grave marker (H12), or lying snapped. */
	void BrokenGlider(FCtx& C, const FVector& Base, float Yaw, int32 Var, bool bStaked)
	{
		const FRotator R(bStaked ? 0.f : 80.f, Yaw, 0.f);
		const auto P = [&](float X, float Y, float Z) { return Base + R.RotateVector(FVector(X, Y, Z)); };
		C.B.K.Rod(Cedar(0.8f), P(0, 0, 0), P(0, 0, 220), 6.f);
		C.B.K.Rod(Cedar(0.75f), P(0, -180, 170), P(0, 160 - Var * 30, 200), 4.f);
		C.B.K.Add(Cloth(Var * 2 + 1, 0.6f), FVector(3.f, 140.f, 70.f), P(2, -80, 150), R + FRotator(0.f, 0.f, -10.f));
		if (Var % 2) C.B.K.Add(Cloth(Var, 0.55f), FVector(3.f, 60.f, 50.f), P(2, 90, 170), R + FRotator(0.f, 0.f, 25.f));
	}
	/** A Puffwing carcass (stout seabird body, oversized wings): impaled on a thorn, or lying. */
	void PuffwingBody(FCtx& C, const FVector& At, float Yaw, bool bImpaled)
	{
		const FRotator R(bImpaled ? -30.f : 0.f, Yaw, 0.f);
		C.B.Ball(At, 18.f, SRGB(0x2A, 0x2A, 0x30) * 0.5f, 0.8f, 1.2f);
		C.B.Ball(At + R.RotateVector(FVector(16, 0, 6)), 10.f, SRGB(0x4F, 0x7F, 0xAF) * 0.45f, 0.8f);
		C.B.K.Spike(FSurf::Glaze(SRGB(0xC8, 0x40, 0x30) * 0.6f, 0.4f), At + R.RotateVector(FVector(26, 0, 6)), R.RotateVector(FVector(1, 0, 0)), 9.f, 14.f);
		for (int32 w = -1; w <= 1; w += 2) C.B.K.Add(FSurf::Cloth(SRGB(0x2E, 0x2E, 0x34) * 0.5f), FVector(30.f, 46.f, 3.f), At + R.RotateVector(FVector(-4, 30.f * w, 0)), R + FRotator(0.f, 0.f, 30.f * w));
	}
	/** A wind harp (aeolian harp) on a ruin wall: a cedar box with gut strings that sing in the wind. */
	void WindHarp(FCtx& C, const FVector& At, float Yaw)
	{
		const FRotator R(0.f, Yaw, 0.f);
		C.B.K.Add(Cedar(0.9f), FVector(100.f, 20.f, 14.f), At, R);
		for (int32 s = 0; s < 6; ++s) C.B.K.Rod(Rope(), At + R.RotateVector(FVector(-46, -6 + s * 2.4f, 8)), At + R.RotateVector(FVector(46, -6 + s * 2.4f, 8)), 0.6f);
		C.B.Loop(TEXT("sky.harpHum"), At);
	}
	/** A drift frame (a fog-net for gossamer): two posts with a net strung between, silk caught in it, flashing. */
	void DriftFrame(FCtx& C, const FVector& Base, float Yaw)
	{
		const FRotator R(0.f, Yaw, 0.f);
		for (int32 e = -1; e <= 1; e += 2) C.B.K.Rod(Cedar(0.85f), Base + R.RotateVector(FVector(0, 220.f * e, 0)), Base + R.RotateVector(FVector(0, 220.f * e, 320)), 8.f);
		C.B.K.Add(FSurf::Cloth(SRGB(0xE8, 0xEE, 0xF2) * 0.55f), FVector(2.f, 430.f, 240.f), Base + FVector(0, 0, 190.f), R);
		for (int32 l = 0; l < 5; ++l) C.B.K.Rod(Rope(), Base + R.RotateVector(FVector(0, -220, 80 + l * 55)), Base + R.RotateVector(FVector(0, 220, 80 + l * 55)), 1.f);
	}
	/** A Kitewright drying rack of kites (frames on a pole rail). */
	void KiteRack(FCtx& C, const FVector& Base, float Yaw, int32 N, bool bRotted)
	{
		const FRotator R(0.f, Yaw, 0.f);
		C.B.K.Rod(Cedar(0.8f), Base + R.RotateVector(FVector(0, -N * 70.f, 220)), Base + R.RotateVector(FVector(0, N * 70.f, 220)), 7.f);
		for (int32 e = -1; e <= 1; e += 2) C.B.K.Rod(Cedar(0.8f), Base + R.RotateVector(FVector(0, e * N * 70.f, 0)), Base + R.RotateVector(FVector(0, e * N * 70.f, 230)), 9.f);
		for (int32 k = 0; k < N; ++k) Kite(C.B, Base + R.RotateVector(FVector(6, (k - (N - 1) * 0.5f) * 140.f, 150)), 110.f, Yaw + 90.f, 0.f, k + C.A->Index, bRotted);
	}
	/** A row of beds (terraced, walled, gone wild). */
	void Beds(FCtx& C, const FVector& At, float Len, float Wd, int32 Rows, bool bRotted)
	{
		for (int32 r = 0; r < Rows; ++r)
		{
			const FVector P = At + FVector(0, (r - (Rows - 1) * 0.5f) * (Wd + 60.f), 0);
			for (int32 e = -1; e <= 1; e += 2)
			{
				const FVector Wall = P + FVector(0, (Wd * 0.5f + 15.f) * e, 0);
				C.B.K.Add(Limestone(0.75f), FVector(Len, 26.f, 36.f), FVector(Wall.X, Wall.Y, C.G(Wall.X, Wall.Y) + 14.f), FRotator::ZeroRotator);
			}
			for (int32 p = 0; p < int32(Len / 40.f); ++p)
			{
				const float X = P.X - Len * 0.5f + p * 40.f + 20.f;
				const FVector Ground(X, P.Y + (C.Rand(r * 50 + p) - 0.5f) * Wd * 0.6f, 0);
				const FVector G2(Ground.X, Ground.Y, C.G(Ground.X, Ground.Y));
				if (bRotted) { C.B.K.Add(FSurf::Cloth(SRGB(0x8A, 0x8A, 0x80) * 0.45f), FVector(22.f, 22.f, 18.f), G2 + FVector(0, 0, 9.f), FRotator::ZeroRotator, EWRLookMesh::Cone); continue; }
				const bool bBerry = (r + p) % 3 == 0;
				C.B.K.Spike(bBerry ? FSurf::Cloth(SRGB(0x5E, 0x7A, 0x3A) * 0.5f) : FSurf::Cloth(SRGB(0x7E, 0x9A, 0x6A) * 0.5f), G2, FVector(-0.25f, 0, 1).GetSafeNormal(), 10.f, bBerry ? 26.f : 60.f);
				C.B.K.Add(bBerry ? Berry() : Flax(), FVector(9.f, 9.f, 6.f), G2 + FVector(-6.f, 0, bBerry ? 22.f : 58.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			}
		}
	}
}

using namespace WRSkySitesImpl;

bool WRSkyParts::Site(FCtx& C)
{
	const FName K = C.A->Kind;
	AWRSkyScenery* A = C.A;
	const FWRSkyIsleData::FSite* S = C.D.FindSite(A->SiteId);
	const int32 Var = A->Variant;
	const TCHAR Zone = C.I ? C.I->Zone : '1';

	// ---------------------------------------------------------------- camps (world-map: three on the cliff tops)
	if (K == TEXT("camp"))
	{
		// A Kitewright lift-keepers' camp: a crescent of dry-stone windbreak open to the lee (west), a hide-and-pole lean-to,
		// a fire ring with a kettle on a tripod, a lung ta pole, coils of spare chain and a crate of lift parts.
		for (int32 s = 0; s < 11; ++s)
		{
			const float Ang = FMath::DegreesToRadians(-75.f + s * 15.f);
			const FVector P(FMath::Cos(Ang) * 900.f, FMath::Sin(Ang) * 900.f, 0.f);
			for (int32 c = 0; c < 4; ++c)
			{
				C.B.K.Add(CliffGranite(0.9f + 0.2f * C.Rand(s * 4 + c)), FVector(100.f, 70.f, 32.f), FVector(P.X, P.Y, C.G(P.X, P.Y) + 16.f + c * 32.f), FRotator(0.f, FMath::RadiansToDegrees(Ang) + 90.f + (C.Rand(s + c) - 0.5f) * 10.f, 0.f));
			}
			C.B.Box(FVector(P.X, P.Y, C.G(P.X, P.Y) + 64.f), FVector(55.f, 40.f, 64.f), FRotator(0.f, FMath::RadiansToDegrees(Ang) + 90.f, 0.f));
		}
		const FVector Fire = C.At(100.f, 0.f);
		for (int32 k = 0; k < 9; ++k) C.B.Ball(Fire + FVector(FMath::Cos(k * 0.7f) * 70.f, FMath::Sin(k * 0.7f) * 70.f, 6.f), 16.f, SRGB(0x6E, 0x6A, 0x62) * 0.5f, 0.9f, 0.6f);
		C.B.N.Add(Ember(6.f), FVector(70.f, 70.f, 16.f), Fire + FVector(0, 0, 8.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		C.B.Lamp(Fire + FVector(0, 0, 80.f), SRGB(0xFF, 0xA8, 0x5C), 2400.f, 1400.f);
		for (int32 t = 0; t < 3; ++t) C.B.K.Rod(Cedar(0.8f), Fire + FVector(FMath::Cos(t * 2.1f) * 60.f, FMath::Sin(t * 2.1f) * 60.f, 0), Fire + FVector(0, 0, 130.f), 4.f);
		C.B.K.Add(FSurf::Steel(SRGB(0x2A, 0x26, 0x22) * 0.5f, 0.7f), FVector(36.f, 36.f, 30.f), Fire + FVector(0, 0, 70.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		// The lean-to against the windbreak.
		const FVector Lt = C.At(500.f, 250.f);
		for (int32 e = -1; e <= 1; e += 2) C.B.K.Rod(Cedar(0.8f), Lt + FVector(-150.f, 150.f * e, 0), Lt + FVector(-150.f, 150.f * e, 200.f), 8.f);
		C.B.K.Add(FSurf::Cloth(SRGB(0x8A, 0x6A, 0x4A) * 0.5f), FVector(340.f, 330.f, 4.f), Lt + FVector(40.f, 0, 120.f), FRotator(-35.f, 0.f, 0.f));
		C.B.K.Add(FSurf::Cloth(SRGB(0x6A, 0x5A, 0x48) * 0.5f), FVector(190.f, 80.f, 10.f), Lt + FVector(60.f, -60.f, 6.f), FRotator::ZeroRotator);
		// Spare chain coiled, a crate of lift parts (a sheave pin, pawl teeth).
		for (int32 r = 0; r < 4; ++r)
		{
			TArray<FVector> Coil;
			for (int32 k = 0; k <= 16; ++k) { const float Ang = 2.f * PI * k / 16.f; Coil.Add(C.At(-300.f, -350.f) + FVector(FMath::Cos(Ang) * (50.f - r * 6.f), FMath::Sin(Ang) * (50.f - r * 6.f), 8.f + r * 9.f)); }
			C.B.M.Tube(Coil, 6.f, 6, ChainC(), 0.7f);
		}
		C.B.K.Add(Plank(0.85f), FVector(110.f, 70.f, 60.f), C.At(-150.f, -420.f, 30.f), FRotator(0.f, 20.f, 0.f));
		C.B.Box(C.At(-150.f, -420.f, 30.f), FVector(55.f, 35.f, 30.f), FRotator(0.f, 20.f, 0.f));
		C.B.K.Disc(RustIron(), C.At(-150.f, -420.f, 66.f), FVector::UpVector, 60.f, 8.f);
		WindPole(C.B, C.At(-500.f, 100.f), Var, true);
		WRSkyParts::AddStreamer(A, C.At(-500.f, 100.f, 455.f), Var);
		return true;
	}

	// ---------------------------------------------------------------- hand sites
	if (K == TEXT("shard_fall"))
	{
		// H1: a small island fell here. Its keel lies shattered at the cliff foot in a field of blue shards; its round
		// house sits upside down on the beach, the table and bowls still nailed to the floor (now the ceiling).
		for (int32 s = 0; s < 40; ++s)
		{
			const float Ang = C.Rand(s) * 2.f * PI, Rr = 300.f + 2200.f * FMath::Sqrt(C.Rand(s + 100));
			const FVector P = C.At(FMath::Cos(Ang) * Rr, FMath::Sin(Ang) * Rr);
			const FVector Dir = FVector(C.Rand(s + 200) - 0.5f, C.Rand(s + 300) - 0.5f, 0.6f).GetSafeNormal();
			const float Len = 80.f + 500.f * C.Rand(s + 400) * (1.f - Rr / 3000.f);
			C.B.K.Spike(Levistone(0.7f), P - Dir * 20.f, Dir, Len * 0.35f, Len);
			if (s % 3 == 0) C.B.N.Spike(LiftGlow(4.f), P - Dir * 10.f, Dir, Len * 0.18f, Len * 1.02f);
		}
		// The keel's broken root: a great tilted rock mass, crystals still in it.
		C.B.Rock(1, FVector(1600.f, 1100.f, 900.f), C.At(-600.f, 400.f), FRotator(25.f, 40.f, 15.f), SRGB(0x9A, 0x96, 0x90) * 0.6f, true, 200.f);
		// The house, roof down in the shingle, its drum wall in the air, the floor above with a table and bowls on it.
		const FVector H = C.At(900.f, -500.f);
		for (int32 c = 0; c < 9; ++c)
		{
			for (int32 s = 0; s < 18; ++s)
			{
				const float Ang = s * 2.f * PI / 18.f;
				if (FMath::Abs(FMath::FindDeltaAngleDegrees(FMath::RadiansToDegrees(Ang), 0.f)) < 15.f && c > 5) continue;
				C.B.K.Add(Limestone(0.8f + 0.2f * C.Rand(c * 18 + s)), FVector(118.f, 50.f, 28.f), H + FVector(FMath::Cos(Ang) * 340.f, FMath::Sin(Ang) * 340.f, 220.f + c * 30.f), FRotator(0.f, FMath::RadiansToDegrees(Ang) + 90.f, 0.f));
			}
		}
		for (int32 r = 0; r < 12; ++r) { const float Ang = r * 2.f * PI / 12.f; C.B.K.Rod(Cedar(0.6f), H + FVector(FMath::Cos(Ang) * 360.f, FMath::Sin(Ang) * 360.f, 220.f), H + FVector(FMath::Cos(Ang) * 40.f, FMath::Sin(Ang) * 40.f, 0.f), 10.f, true); }
		C.B.K.Disc(Plank(0.8f), H + FVector(0, 0, 500.f), FVector::UpVector, 660.f, 14.f);
		C.B.K.Add(Cedar(0.85f), FVector(160.f, 90.f, 8.f), H + FVector(0, 0, 420.f), FRotator::ZeroRotator);   // the table, hanging
		for (int32 l = 0; l < 4; ++l) C.B.K.Rod(Cedar(0.8f), H + FVector((l % 2 ? 70.f : -70.f), (l / 2 ? 40.f : -40.f), 492.f), H + FVector((l % 2 ? 70.f : -70.f), (l / 2 ? 40.f : -40.f), 424.f), 6.f);
		for (int32 b = 0; b < 4; ++b) C.B.K.Add(FSurf::Glaze(SRGB(0xB8, 0x8A, 0x5A) * 0.55f, 0.5f), FVector(22.f, 22.f, 8.f), H + FVector(-50.f + b * 34.f, 0, 412.f), FRotator(180.f, 0.f, 0.f), EWRLookMesh::Cylinder);
		C.B.Box(H + FVector(0, 0, 300.f), FVector(350.f, 350.f, 30.f));
		return true;
	}
	if (K == TEXT("lift_wreck"))
	{
		// H2: a cage lift snapped its chain and fell 300 m. The cage lies crumpled; inside, two skeletons hold each other;
		// a bone tube in one's hand holds a rolled kite of yellow cloth. The snapped chain lies in loops up the slope.
		const FVector Cg = C.At(0.f, 0.f);
		const FRotator Tilt(18.f, 30.f, 24.f);
		const auto P = [&](float X, float Y, float Z) { return Cg + Tilt.RotateVector(FVector(X, Y, Z)); };
		for (int32 e = 0; e < 4; ++e)
		{
			const float X = (e % 2 ? 1.f : -1.f) * 120.f, Y = (e / 2 ? 1.f : -1.f) * 120.f;
			C.B.K.Rod(RustIron(), P(X, Y, 0), P(X * 0.8f, Y * 1.1f, 190.f - 60.f * (e == 1)), 9.f, true);
		}
		for (int32 r = 0; r < 3; ++r)
		{
			C.B.K.Rod(RustIron(), P(-120, -120, 60 + r * 50), P(120, -120, 50 + r * 55), 5.f);
			C.B.K.Rod(RustIron(), P(-120, 120, 70 + r * 50), P(120, 120, 40 + r * 45), 5.f);
		}
		C.B.K.Add(Plank(0.7f), FVector(250.f, 250.f, 8.f), P(0, 0, 0), Tilt);
		C.B.Box(P(0, 0, 0), FVector(125.f, 125.f, 6.f), Tilt);
		Skeleton(C, P(10, -20, 12), 30.f, 0, 18.f);
		Skeleton(C, P(-10, 25, 14), 210.f, 0, 18.f);
		BoneTube(C.B, P(20, 0, 22), 120.f, 10.f);
		C.B.K.Add(FSurf::Cloth(SRGB(0xD8, 0xB2, 0x4A) * 0.5f), FVector(30.f, 8.f, 8.f), P(34, 4, 22), Tilt);
		// The chain, snapped, in loose loops up toward the cliff.
		for (int32 l = 0; l < 5; ++l) C.B.Line(C.At(-300.f - l * 260.f, (l % 2 ? 80.f : -80.f), 10.f), C.At(-560.f - l * 260.f, (l % 2 ? -80.f : 80.f), 10.f), 5.f, -20.f, ChainC(), 8);
		return true;
	}
	if (K == TEXT("larder_tree"))
	{
		// H3: a wind-sculpted cedar hung with impaled Puffwings and a Galeharrier, a newcomer's backpack on the highest
		// thorn. The Gallowshrike pack lives here.
		C.B.Tree(TEXT("cloud_cedar"), 2, C.At(0, 0, -15.f), 1700.f, FRotator(0.f, 180.f - A->GetActorRotation().Yaw, 0.f));
		// Thorns: black spikes on the lower limbs (Gallowshrikes cut and set them) with the larder on them.
		for (int32 t = 0; t < 14; ++t)
		{
			const float Ang = t * 0.9f;
			const FVector At = C.At(FMath::Cos(Ang) * (160.f + 30.f * t), FMath::Sin(Ang) * (160.f + 30.f * t), 220.f + 55.f * t);
			C.B.K.Spike(FSurf::Wood(SRGB(0x2A, 0x22, 0x1C) * 0.6f), At, FVector(FMath::Cos(Ang), FMath::Sin(Ang), 0.5f).GetSafeNormal(), 6.f, 45.f);
			if (t % 2 == 0) PuffwingBody(C, At + FVector(FMath::Cos(Ang), FMath::Sin(Ang), 0.3f) * 20.f, FMath::RadiansToDegrees(Ang), true);
		}
		// The Galeharrier, wings hanging open.
		const FVector Gh = C.At(-200.f, 140.f, 700.f);
		C.B.Ball(Gh, 30.f, SRGB(0x1E, 0x1E, 0x22) * 0.5f, 0.8f, 1.4f);
		for (int32 w = -1; w <= 1; w += 2) C.B.K.Add(FSurf::Cloth(SRGB(0x3A, 0x3A, 0x40) * 0.5f), FVector(40.f, 130.f, 3.f), Gh + FVector(0, 70.f * w, -30.f), FRotator(0.f, 0.f, 60.f * w));
		// The backpack on the highest thorn: hide, a rolled bedroll, a copper glider part tied on.
		const FVector Bp = C.At(60.f, -40.f, 1500.f);
		C.B.K.Add(FSurf::Cloth(SRGB(0x7A, 0x5A, 0x3A) * 0.5f), FVector(40.f, 30.f, 55.f), Bp, FRotator(10.f, 30.f, 0.f));
		C.B.K.Rod(FSurf::Steel(SRGB(0xB8, 0x73, 0x33) * 0.6f, 0.5f), Bp + FVector(-20, -30, 10), Bp + FVector(20, 30, 20), 4.f);
		// Pellets and bones under the tree.
		for (int32 b = 0; b < 12; ++b) C.B.Ball(C.At(FMath::Cos(b * 1.3f) * 300.f, FMath::Sin(b * 1.3f) * 300.f, 4.f), 6.f, BoneC(), 0.8f, 0.5f);
		return true;
	}
	if (K == TEXT("feather_rookery"))
	{
		// H4: Puffwing burrows in the turf (dark mouths with spoil mounds), a plucking hut collapsed under a fallen wind-pole,
		// feathers everywhere.
		for (int32 b = 0; b < 16; ++b)
		{
			const float Ang = b * 2.39996f, Rr = 300.f + 1500.f * FMath::Sqrt(C.Rand(b + 7));
			const FVector P = C.At(FMath::Cos(Ang) * Rr, FMath::Sin(Ang) * Rr);
			C.B.K.Disc(FSurf::Cloth(SRGB(0x14, 0x12, 0x10) * 0.5f), P + FVector(0, 0, 3.f), FVector(FMath::Cos(Ang), FMath::Sin(Ang), 2.f).GetSafeNormal(), 55.f, 4.f);
			C.B.Ball(P + FVector(FMath::Cos(Ang) * 70.f, FMath::Sin(Ang) * 70.f, 0.f), 40.f, SRGB(0x6A, 0x56, 0x40) * 0.5f, 0.95f, 0.35f);
			for (int32 f = 0; f < 3; ++f) C.B.K.Add(FSurf::Cloth(SRGB(0xE8, 0xE6, 0xDE) * 0.6f), FVector(12.f, 3.f, 1.f), P + FVector(C.Rand(b * 3 + f) * 120.f - 60.f, C.Rand(b * 5 + f) * 120.f - 60.f, 2.f), FRotator(0.f, C.Rand(b + f) * 360.f, 0.f));
		}
		// The hut: corner posts, a half-fallen roof, a plucking bench, the pole lying across it.
		const FVector H = C.At(-400.f, 300.f);
		for (int32 c = 0; c < 4; ++c) C.B.K.Rod(Cedar(0.75f), H + FVector((c % 2 ? 1 : -1) * 180.f, (c / 2 ? 1 : -1) * 140.f, 0), H + FVector((c % 2 ? 1 : -1) * 180.f, (c / 2 ? 1 : -1) * 140.f, c == 3 ? 80.f : 210.f), 9.f);
		C.B.K.Add(Shingle(0.8f), FVector(400.f, 320.f, 6.f), H + FVector(0, 0, 160.f), FRotator(-20.f, 0.f, 12.f));
		C.B.K.Add(Cedar(0.8f), FVector(160.f, 50.f, 70.f), H + FVector(40.f, 0, 35.f), FRotator::ZeroRotator);
		C.B.Box(H + FVector(40.f, 0, 35.f), FVector(80.f, 25.f, 35.f));
		C.B.K.Rod(Cedar(0.85f), H + FVector(-600.f, -200.f, 10.f), H + FVector(250.f, 120.f, 200.f), 12.f);
		C.B.K.Add(Cloth(2, 0.6f), FVector(40.f, 3.f, 30.f), H + FVector(0.f, 30.f, 140.f), FRotator(0.f, 20.f, 0.f));
		return true;
	}
	if (K == TEXT("lift_station"))
	{
		// H5: a broken updraft-lift platform and its twin on the next island: a timber deck on four levistone pads (the
		// stones that once lifted it, gone dull), a winch house where the Updraft lift's blueprint lies on the bench.
		const FVector Pd = C.At(0, 0);
		C.B.K.Add(Plank(0.85f), FVector(500.f, 500.f, 16.f), Pd + FVector(0, 0, 70.f), FRotator(0.f, 0.f, 4.f));
		C.B.Box(Pd + FVector(0, 0, 70.f), FVector(250.f, 250.f, 8.f), FRotator(0.f, 0.f, 4.f));
		for (int32 c = 0; c < 4; ++c)
		{
			const FVector Cn = Pd + FVector((c % 2 ? 1 : -1) * 200.f, (c / 2 ? 1 : -1) * 200.f, 30.f);
			C.B.K.Add(Levistone(c == 2 ? 0.f : 0.25f), FVector(70.f, 70.f, 60.f), Cn, FRotator(0.f, 45.f, 0.f), EWRLookMesh::Cylinder);
		}
		// The winch house: a small round stone hut with a drum and a broken chain running to the platform.
		const FVector Wh = C.At(-700.f, 0.f);
		RoundHouse(C.B, Wh, 0, false);
		C.B.K.Add(Cedar(0.85f), FVector(120.f, 120.f, 160.f), Wh + FVector(0, 0, 100.f), FRotator(90.f, 0.f, 0.f), EWRLookMesh::Cylinder);
		C.B.K.Add(Plank(0.9f), FVector(30.f, 40.f, 2.f), Wh + FVector(120.f, 140.f, 92.f), FRotator(0.f, 10.f, 0.f));   // the blueprint's sheet
		Chain(C.B, Wh + FVector(300.f, 0, 100.f), Pd + FVector(-250.f, 0, 80.f), 30.f);
		if (S && !S->To.IsZero())
		{
			// The chain to its twin across the gap, snapped and hanging from this side.
			const FVector To = C.L(S->To);
			C.B.Line(Pd + FVector(250.f, 0, 80.f), FMath::Lerp(Pd, To, 0.2f) - FVector(0, 0, 1500.f), 5.f, 300.f, ChainC(), 12);
		}
		return true;
	}
	if (K == TEXT("cloud_garden"))
	{
		// H6: terraced Windflax and Cloudberry beds gone wild; a gardener's glider snapped in a cedar, the pilot's hat on a
		// branch below.
		Beds(C, C.At(0, 0), 900.f, 140.f, 5, false);
		C.B.Tree(TEXT("cloud_cedar"), 1, C.At(700.f, -400.f, -15.f), 1300.f, FRotator(0.f, 180.f - A->GetActorRotation().Yaw, 0.f));
		BrokenGlider(C, C.At(700.f, -400.f, 700.f), 40.f, 1, false);
		C.B.K.Add(FSurf::Cloth(SRGB(0x7A, 0x6A, 0x50) * 0.5f), FVector(40.f, 40.f, 14.f), C.At(760.f, -380.f, 320.f), FRotator(10.f, 0.f, 0.f), EWRLookMesh::Cylinder);
		C.B.K.Add(FSurf::Cloth(SRGB(0x7A, 0x6A, 0x50) * 0.5f), FVector(24.f, 24.f, 14.f), C.At(760.f, -380.f, 334.f), FRotator(10.f, 0.f, 0.f), EWRLookMesh::Cylinder);
		WindHarp(C, C.At(-500.f, 500.f, 160.f), 30.f);
		return true;
	}
	if (K == TEXT("silk_house"))
	{
		// H7: a round house with spindles and looms, its walls hung with gossamer sheets; drift frames outside still catch
		// silk; one loom holds a half-woven Skysilk scarf (teal).
		RoundHouse(C.B, C.At(0, 0), 3, true);
		for (int32 s = 0; s < 6; ++s)
		{
			const float Ang = FMath::DegreesToRadians(30.f + s * 50.f);
			C.B.K.Add(Skysilk(), FVector(4.f, 120.f, 200.f), C.At(FMath::Cos(Ang) * 300.f, FMath::Sin(Ang) * 300.f, 150.f), FRotator(0.f, FMath::RadiansToDegrees(Ang), 0.f));
		}
		// The loom: an upright frame, warp threads, the scarf part-woven.
		const FVector Lm = C.At(120.f, 0.f);
		for (int32 e = -1; e <= 1; e += 2) C.B.K.Rod(Cedar(0.9f), Lm + FVector(0, 80.f * e, 0), Lm + FVector(0, 80.f * e, 190.f), 7.f);
		C.B.K.Rod(Cedar(0.9f), Lm + FVector(0, -80, 185), Lm + FVector(0, 80, 185), 6.f);
		C.B.K.Rod(Cedar(0.9f), Lm + FVector(0, -80, 40), Lm + FVector(0, 80, 40), 6.f);
		for (int32 w = 0; w < 12; ++w) C.B.K.Rod(Skysilk(), Lm + FVector(0, -66 + w * 12, 40), Lm + FVector(0, -66 + w * 12, 185), 0.5f);
		C.B.K.Add(Teal(), FVector(2.f, 130.f, 70.f), Lm + FVector(1.f, 0, 80.f), FRotator::ZeroRotator);
		C.B.Box(Lm + FVector(0, 0, 95.f), FVector(10.f, 85.f, 95.f));
		// Spindles on a bench, wound with silk.
		for (int32 s = 0; s < 4; ++s) C.B.K.Add(Skysilk(), FVector(12.f, 12.f, 30.f), C.At(-120.f, -60.f + s * 40.f, 92.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		C.B.K.Add(Cedar(0.8f), FVector(60.f, 200.f, 8.f), C.At(-120.f, 0.f, 80.f), FRotator::ZeroRotator);
		DriftFrame(C, C.At(700.f, 300.f), 90.f);
		DriftFrame(C, C.At(650.f, -350.f), 70.f);
		return true;
	}
	if (K == TEXT("kite_field"))
	{
		// H8: hundreds of rotted kite frames on stakes up the hill, each with a bone tube at its foot (the death-kites);
		// at the top a single new kite, crude, sewn from a newcomer's shirt, still flying. Someone tried to call someone.
		for (int32 k = 0; k < 160; ++k)
		{
			const float Ang = k * 2.39996f, Rr = 300.f + 6500.f * FMath::Sqrt(C.Rand(k + 11));
			const float X = FMath::Cos(Ang) * Rr, Y = FMath::Sin(Ang) * Rr * 0.7f;
			if (!C.OnTop(X, Y, 600.f)) continue;
			const FVector G = C.At(X, Y);
			const float Lean = (C.Rand(k + 99) - 0.5f) * 30.f;
			C.B.K.Rod(Cedar(0.6f + 0.3f * C.Rand(k)), G - FVector(0, 0, 20.f), G + FRotator(Lean, 0.f, 0.f).RotateVector(FVector(0, 0, 200.f)), 5.f);
			if (C.Rand(k + 5) < 0.8f) Kite(C.B, G + FVector(0, 0, 200.f), 70.f + 40.f * C.Rand(k + 6), 180.f - A->GetActorRotation().Yaw + Lean, 30.f + 30.f * C.Rand(k + 7), k, true);
			BoneTube(C.B, G + FVector(15.f, 0, 6.f), C.Rand(k + 8) * 360.f, -10.f);
		}
		// The newcomer's kite at the top: sticks, fibre, a shirt; flying on its line.
		const float Hx = C.I ? C.I->R() * 0.2f : 0.f;
		const FVector Top = C.At(Hx, 0.f);
		C.B.K.Rod(FSurf::Wood(SRGB(0x8A, 0x70, 0x50) * 0.5f), Top, Top + FVector(0, 0, 90.f), 4.f);
		USceneComponent* Fly = WRSkyParts::Pivot(A, Top + FVector(0, 0, 90.f));
		A->Swing = Fly;
		A->SetActorTickEnabled(true);
		WRSkyParts::SpawnUnder(A, Fly, [](WRLookKit::FKit& Kk)
		{
			// Out downwind (+X of the pivot, which turns with the wind) on a fibre line, the shirt-kite and its rag tail.
			const FVector KiteAt(1500.f, 0.f, 900.f);
			for (int32 s2 = 0; s2 < 12; ++s2)
			{
				const float T0 = s2 / 12.f, T1 = (s2 + 1) / 12.f;
				Kk.Rod(Rope(), FMath::Lerp(FVector::ZeroVector, KiteAt, T0) - FVector(0, 0, 4.f * 120.f * T0 * (1.f - T0)), FMath::Lerp(FVector::ZeroVector, KiteAt, T1) - FVector(0, 0, 4.f * 120.f * T1 * (1.f - T1)), 0.6f);
			}
			Kk.Rod(FSurf::Wood(SRGB(0x8A, 0x70, 0x50) * 0.5f), KiteAt - FVector(0, 0, 70.f), KiteAt + FVector(0, 0, 70.f), 2.f);
			Kk.Rod(FSurf::Wood(SRGB(0x8A, 0x70, 0x50) * 0.5f), KiteAt - FVector(0, 55.f, -15.f), KiteAt + FVector(0, 55.f, 15.f), 2.f);
			Kk.Add(FSurf::Cloth(SRGB(0xB8, 0xA8, 0x88) * 0.5f), FVector(2.f, 110.f, 130.f), KiteAt, FRotator(50.f, 0.f, 0.f));
			for (int32 t = 0; t < 5; ++t) Kk.Add(FSurf::Cloth(SRGB(0x9A, 0x8A, 0x70) * 0.5f), FVector(2.f, 18.f, 30.f), KiteAt + FVector(40.f * t, 0, -80.f - 30.f * t), FRotator(0.f, 0.f, 25.f * (t % 2 ? 1 : -1)));
		});
		return true;
	}
	if (K == TEXT("gulpers_bight"))
	{
		// H9: the rain lagoon's shore is a white ring of Puffwing bones; one capsized Kitewright skiff (they had boats on
		// the lagoons). Old Gulper floats over it (WRSkyBosses).
		if (C.I)
		{
			for (int32 b = 0; b < 90; ++b)
			{
				const float Ang = b * 2.f * PI / 90.f + C.Rand(b) * 0.05f;
				const float Rr = C.I->EdgeRadius(Ang) * (0.74f + 0.06f * C.Rand(b + 9));
				const FVector2D L(FMath::Cos(Ang) * Rr, FMath::Sin(Ang) * Rr);
				const FVector W(C.I->World2D(L), 0.f);
				const FVector Lc = C.L(W);
				C.B.K.Rod(Bone(), C.At(Lc.X, Lc.Y, 4.f), C.At(Lc.X + 30.f * FMath::Cos(b * 1.7f), Lc.Y + 30.f * FMath::Sin(b * 1.7f), 6.f), 3.f);
				if (b % 6 == 0) C.B.Ball(C.At(Lc.X, Lc.Y, 6.f), 7.f, BoneC(), 0.8f);
			}
		}
		const FVector Sk = C.At(C.I ? C.I->RMin() * 0.8f : 1500.f, 0.f);
		C.B.M.Lathe(Sk + FVector(-260.f, 0, 50.f), FVector(1, 0, 0), 520.f, { FVector(0, 10, 10), FVector(0.15f, 80, 50), FVector(0.5f, 110, 60), FVector(0.85f, 80, 50), FVector(1, 8, 8) }, 12, Cedar(0.8f).Color, 0.8f);
		C.B.Box(Sk + FVector(0, 0, 50.f), FVector(260.f, 100.f, 55.f));
		return true;
	}
	if (K == TEXT("toll_house"))
	{
		// H10: a toll tower and gate where the old bridge began; beside the gate, offering bowls overflowing with copper
		// rings (taking them wakes the Sentinel). The Sentinel walks out from here.
		TollTower(C.B, C.At(0, 0), FVector(1, 0, 0), Zone, false);
		for (int32 g = -1; g <= 1; g += 2)
		{
			const FVector Gp = C.At(500.f, 260.f * g);
			C.B.K.Add(RimStone(Zone, 0.9f), FVector(80.f, 80.f, 300.f), Gp + FVector(0, 0, 150.f), FRotator::ZeroRotator);
			C.B.Box(Gp + FVector(0, 0, 150.f), FVector(40.f, 40.f, 150.f));
			C.B.K.Rod(RustIron(), Gp + FVector(0, -10.f * g, 220.f), Gp + FVector(0, -230.f * g, 210.f), 6.f);
		}
		for (int32 b = 0; b < 3; ++b)
		{
			const FVector Bw = C.At(380.f, 460.f + b * 70.f);
			C.B.K.Add(Bronze(), FVector(50.f, 50.f, 18.f), Bw + FVector(0, 0, 60.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			C.B.K.Add(RimStone(Zone, 0.8f), FVector(40.f, 40.f, 55.f), Bw + FVector(0, 0, 26.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			for (int32 r = 0; r < 7; ++r) C.B.K.Disc(FSurf::Steel(SRGB(0xB8, 0x73, 0x33) * 0.6f, 0.4f), Bw + FVector(FMath::Cos(r * 0.9f) * 12.f, FMath::Sin(r * 0.9f) * 12.f, 72.f + r * 2.f), FVector(0.2f, 0.1f, 1.f).GetSafeNormal(), 10.f, 1.5f);
		}
		return true;
	}
	if (K == TEXT("harriers_eyrie"))
	{
		// H11: a nest 15 m across built of bridge planks, chain and kite cloth; Windmane skulls round its rim. Kyrra rests.
		const FVector N0 = C.At(0, 0);
		for (int32 l = 0; l < 60; ++l)
		{
			const float Ang = l * 0.61f, Rr = 450.f + 250.f * C.Rand(l);
			const FVector A0 = N0 + FVector(FMath::Cos(Ang) * Rr, FMath::Sin(Ang) * Rr, 30.f + 70.f * C.Rand(l + 9));
			const FVector A1 = N0 + FVector(FMath::Cos(Ang + 1.1f) * Rr, FMath::Sin(Ang + 1.1f) * Rr, 40.f + 70.f * C.Rand(l + 19));
			if (l % 5 == 0) C.B.Line(A0, A1, 4.f, 20.f, ChainC(), 6);
			else C.B.K.Rod(l % 3 ? Plank(0.7f) : Cedar(0.6f), A0, A1, 12.f, true);
			if (l % 7 == 0) C.B.K.Add(Cloth(l, 0.6f), FVector(80.f, 3.f, 60.f), (A0 + A1) * 0.5f + FVector(0, 0, 20.f), (A1 - A0).Rotation());
		}
		C.B.K.Disc(FSurf::Cloth(SRGB(0x7A, 0x6A, 0x58) * 0.5f), N0 + FVector(0, 0, 20.f), FVector::UpVector, 1000.f, 30.f);
		C.B.Box(N0 + FVector(0, 0, 25.f), FVector(500.f, 500.f, 25.f));
		for (int32 s = 0; s < 6; ++s)
		{
			const float Ang = s * 2.f * PI / 6.f;
			const FVector Sk = N0 + FVector(FMath::Cos(Ang) * 720.f, FMath::Sin(Ang) * 720.f, 110.f);
			C.B.Ball(Sk, 22.f, BoneC(), 0.8f, 1.3f);
			C.B.K.Spike(Bone(), Sk + FVector(0, 0, 18.f), FVector(FMath::Cos(Ang) * 0.3f, FMath::Sin(Ang) * 0.3f + 0.4f, 1.f).GetSafeNormal(), 8.f, 70.f);
			C.B.K.Spike(Bone(), Sk + FVector(0, 0, 18.f), FVector(FMath::Cos(Ang) * 0.3f, FMath::Sin(Ang) * 0.3f - 0.4f, 1.f).GetSafeNormal(), 8.f, 70.f);
		}
		return true;
	}
	if (K == TEXT("glider_graves"))
	{
		// H12: a ledge under the Weeping Isle's view where broken gliders are lined up and staked like graves, each with a
		// name-stone carved by a different hand. The last stake is empty.
		for (int32 g = 0; g < 9; ++g)
		{
			const FVector P = C.At(-600.f + g * 150.f, 0.f);
			if (g < 8) BrokenGlider(C, P, 90.f, g, true);
			else C.B.K.Rod(Cedar(0.85f), P, P + FVector(0, 0, 220.f), 6.f);
			C.B.K.Add(Granite(0.85f + 0.1f * C.Rand(g)), FVector(18.f, 46.f, 40.f + 10.f * C.Rand(g + 5)), P + FVector(70.f, 0, 18.f), FRotator(0.f, 0.f, (C.Rand(g + 9) - 0.5f) * 8.f));
		}
		return true;
	}
	if (K == TEXT("wind_wheel"))
	{
		// H13: the Kitewright wind-wheel's hub on its tower (70 m across once, over the Flue, blowing the spores down). The
		// hub still turns a little on the wind; three of its vanes lie across Vane Rocks; its drive chain trails off the
		// island toward the Ring (the chain walk). The tower: a stone drum 10 m across, 18 m tall, a bronze axle.
		const FVector Tw = C.At(0, 0);
		for (int32 c = 0; c < 36; ++c)
		{
			for (int32 s = 0; s < 24; ++s)
			{
				const float Ang = (s + (c % 2) * 0.5f) * 2.f * PI / 24.f;
				C.B.K.Add(TempleWhite(0.85f + 0.15f * C.Rand(c * 24 + s)), FVector(2.f * PI * 500.f / 24.f - 6.f, 60.f, 48.f), Tw + FVector(FMath::Cos(Ang) * 500.f, FMath::Sin(Ang) * 500.f, 25.f + c * 50.f), FRotator(0.f, FMath::RadiansToDegrees(Ang) + 90.f, 0.f));
			}
		}
		C.B.Box(Tw + FVector(0, 0, 900.f), FVector(500.f, 500.f, 900.f));
		A->Swing = WRSkyParts::Pivot(A, Tw + FVector(0, 0, 1900.f), FRotator(0.f, 0.f, 0.f));
		A->SetActorTickEnabled(true);
		WRSkyParts::SpawnUnder(A, A->Swing, [](WRLookKit::FKit& Kk)
		{
			Kk.Add(Bronze(), FVector(500.f, 500.f, 600.f), FVector(0, 0, 0), FRotator(90.f, 0.f, 0.f), EWRLookMesh::Cylinder);
			for (int32 v = 0; v < 9; ++v)
			{
				// Stub spars of the twelve vanes (three gone to the Vane Rocks): broken off a few metres out.
				if (v == 2 || v == 5 || v == 7) continue;
				const float Ang = v * 2.f * PI / 9.f;
				const FVector D(0, FMath::Cos(Ang), FMath::Sin(Ang));
				Kk.Rod(Cedar(0.75f), D * 250.f, D * (900.f + 300.f * (v % 3)), 60.f, true);
				Kk.Add(Cloth(v, 0.55f), FVector(6.f, 400.f, 250.f + 60.f * (v % 2)), D * 700.f + FVector(20.f, 0, 0), FRotator(0.f, 0.f, FMath::RadiansToDegrees(Ang) + 90.f));
			}
		});
		// The drive chain from the hub's sprocket down the tower and off the lip (its walk is a bridge piece).
		C.B.Line(Tw + FVector(300.f, 0, 1700.f), Tw + FVector(900.f, 0, 40.f), 9.f, 60.f, ChainC(), 10);
		return true;
	}
	if (K == TEXT("wheel_vanes"))
	{
		// H13b: three of the wheel's vanes lie across the two Vane Rocks like fallen sails: 30 m spars with their cloth
		// still on them, bridging the gap (you can walk a spar from one rock to the other).
		for (int32 v = 0; v < 3; ++v)
		{
			const FVector P0 = C.At(-1600.f + v * 220.f, -500.f + v * 450.f, 60.f), P1 = C.At(1500.f + v * 160.f, -300.f + v * 380.f, 80.f + v * 30.f);
			C.B.K.Rod(Cedar(0.7f), P0, P1, 70.f, true);
			C.B.SegBox(P0, P1, 40.f, 35.f);
			for (int32 s = 0; s < 6; ++s)
			{
				const FVector M = FMath::Lerp(P0, P1, (s + 0.5f) / 6.f);
				C.B.K.Add(Cloth(v * 2 + s, 0.55f), FVector(FVector::Dist(P0, P1) / 6.f - 10.f, 420.f, 4.f), M + FVector(0, 210.f, -10.f), FRotator(0.f, (P1 - P0).Rotation().Yaw, -14.f + 6.f * v));
			}
		}
		return true;
	}
	if (K == TEXT("spore_scar"))
	{
		// H14: the island nearest the Flue, its gardens rotted grey, gardeners face-down at their beds in cloth masks.
		Beds(C, C.At(0, 0), 1000.f, 140.f, 6, true);
		for (int32 g = 0; g < 5; ++g) Skeleton(C, C.At(-300.f + g * 150.f, (g % 2 ? 1 : -1) * 240.f, 4.f), 160.f + g * 17.f, 2);
		C.B.Loop(TEXT("sky.sporeDrift"), C.At(0, 0, 100.f), FRotator::ZeroRotator, 2.f);
		return true;
	}
	if (K == TEXT("serpent_door"))
	{
		// H16: a round white temple whose door is a serpent's open jaw (Tower of the Winds' eight winds round its wall), a
		// serpent coiled round a wheel inside. The arena's run-back stone and the Wind altar stand here (WRSkyBosses).
		const FVector Tc = C.At(0, 0);
		const float R = 900.f;
		for (int32 c = 0; c < 20; ++c)
		{
			for (int32 s = 0; s < 32; ++s)
			{
				const float Ang = (s + (c % 2) * 0.5f) * 2.f * PI / 32.f;
				const float Deg = FMath::RadiansToDegrees(Ang);
				if (FMath::Abs(FMath::FindDeltaAngleDegrees(Deg, 0.f)) < 16.f && c < 13) continue;   // the jaw door faces +X
				C.B.K.Add(TempleWhite(0.9f + 0.1f * C.Rand(c * 32 + s)), FVector(2.f * PI * R / 32.f - 6.f, 70.f, 48.f), Tc + FVector(FMath::Cos(Ang) * R, FMath::Sin(Ang) * R, 25.f + c * 50.f), FRotator(0.f, Deg + 90.f, 0.f));
			}
		}
		for (int32 s = 0; s < 16; ++s)
		{
			const float Deg = s * 22.5f + 11.25f;
			if (FMath::Abs(FMath::FindDeltaAngleDegrees(Deg, 0.f)) < 18.f) continue;
			const float Ang = FMath::DegreesToRadians(Deg);
			C.B.Box(Tc + FVector(FMath::Cos(Ang) * R, FMath::Sin(Ang) * R, 500.f), FVector(R * 0.2f, 35.f, 500.f), FRotator(0.f, Deg + 90.f, 0.f));
		}
		// The eight winds carved round the inner wall (relief figures: a band of jade tile under each).
		for (int32 w = 0; w < 8; ++w)
		{
			const float Ang = FMath::DegreesToRadians(w * 45.f + 22.5f);
			const FVector P = Tc + FVector(FMath::Cos(Ang) * (R - 50.f), FMath::Sin(Ang) * (R - 50.f), 600.f);
			C.B.K.Add(TempleWhite(0.75f), FVector(20.f, 200.f, 150.f), P, FRotator(0.f, FMath::RadiansToDegrees(Ang), 0.f));
			C.B.K.Add(JadeTile(), FVector(10.f, 220.f, 20.f), P - FVector(0, 0, 100.f), FRotator(0.f, FMath::RadiansToDegrees(Ang), 0.f));
		}
		// The jaw: upper and lower jaw stones round the door, fangs, the serpent's eyes.
		const FVector Door = Tc + FVector(R, 0, 0);
		for (int32 f = 0; f < 7; ++f)
		{
			C.B.K.Spike(TempleWhite(1.f), Door + FVector(20.f, -150.f + f * 50.f, 640.f), FVector(0.3f, 0, -1).GetSafeNormal(), 30.f, 90.f);
			C.B.K.Spike(TempleWhite(1.f), Door + FVector(20.f, -150.f + f * 50.f, 30.f), FVector(0.3f, 0, 1).GetSafeNormal(), 26.f, 60.f);
		}
		C.B.K.Add(TempleWhite(0.95f), FVector(200.f, 420.f, 90.f), Door + FVector(60.f, 0, 690.f), FRotator(-15.f, 0.f, 0.f));
		for (int32 e = -1; e <= 1; e += 2) C.B.K.Add(JadeTile(1.2f), FVector(20.f, 40.f, 30.f), Door + FVector(120.f, 140.f * e, 760.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		// The roof: a shallow dome of white stone on the drum.
		C.B.M.Lathe(Tc + FVector(0, 0, 1020.f), FVector::UpVector, 380.f, { FVector(0, R + 60.f, R + 60.f), FVector(0.4f, R * 0.85f, R * 0.85f), FVector(0.8f, R * 0.45f, R * 0.45f), FVector(1, 60, 60) }, 32, SRGB(0xF0, 0xED, 0xE6) * 0.65f, 0.8f);
		// The serpent round the wheel (a relief in the floor's middle: a stone wheel, the body coiled through its spokes).
		C.B.K.Disc(TempleWhite(0.8f), Tc + FVector(0, 0, 8.f), FVector::UpVector, 500.f, 16.f);
		TArray<FVector> Coil;
		for (int32 k = 0; k <= 40; ++k) { const float Ang = k * 0.4f; Coil.Add(Tc + FVector(FMath::Cos(Ang) * (230.f - k * 3.f), FMath::Sin(Ang) * (230.f - k * 3.f), 22.f + 6.f * FMath::Sin(Ang * 3.f))); }
		C.B.M.Tube(Coil, 16.f, 8, SRGB(0x2F, 0x8F, 0x7A) * 0.5f, 0.4f);
		Brazier(C.B, Tc + FVector(R * 0.6f, -R * 0.5f, C.G(R * 0.6f, -R * 0.5f)), 2800.f);
		Brazier(C.B, Tc + FVector(R * 0.6f, R * 0.5f, C.G(R * 0.6f, R * 0.5f)), 2800.f);
		return true;
	}

	// ---------------------------------------------------------------- shrine settings (SKY 2.13)
	if (K == TEXT("great_shrine"))
	{
		// The Great Shrine of the Still Pool: Yara Kesh's open workshop (glider frames on every wall, silk on spindles,
		// her one-winged glider over the bench), the Wind Rose (eight carved winds round the floor, the secret), the
		// Aethercarp pool below the bench.
		const FVector Ws = C.At(-600.f, 0.f);
		// An open pavilion: six white columns, a cedar roof frame, a stone floor.
		for (int32 c = 0; c < 6; ++c)
		{
			const float Ang = c * 2.f * PI / 6.f;
			const FVector P = Ws + FVector(FMath::Cos(Ang) * 600.f, FMath::Sin(Ang) * 600.f, 0.f);
			C.B.K.Add(TempleWhite(0.9f), FVector(70.f, 70.f, 420.f), P + FVector(0, 0, 210.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			C.B.Box(P + FVector(0, 0, 210.f), FVector(35.f, 35.f, 210.f));
			C.B.K.Rod(Cedar(0.85f), P + FVector(0, 0, 430.f), Ws + FVector(0, 0, 640.f), 16.f, true);
		}
		C.B.K.Disc(TempleWhite(0.8f), Ws + FVector(0, 0, 8.f), FVector::UpVector, 1300.f, 18.f);
		C.B.Box(Ws + FVector(0, 0, 8.f), FVector(640.f, 640.f, 9.f));
		C.B.K.Add(Shingle(0.85f), FVector(1400.f, 1400.f, 10.f), Ws + FVector(0, 0, 520.f), FRotator::ZeroRotator, EWRLookMesh::Cone);
		// The bench, tools, glider frames racked round, the one-winged glider hung over it.
		C.B.K.Add(Cedar(0.9f), FVector(300.f, 90.f, 10.f), Ws + FVector(200.f, 0, 90.f), FRotator::ZeroRotator);
		C.B.Box(Ws + FVector(200.f, 0, 45.f), FVector(150.f, 45.f, 45.f));
		for (int32 l = 0; l < 4; ++l) C.B.K.Rod(Cedar(0.8f), Ws + FVector(200.f + (l % 2 ? 140.f : -140.f), (l / 2 ? 40.f : -40.f), 0), Ws + FVector(200.f + (l % 2 ? 140.f : -140.f), (l / 2 ? 40.f : -40.f), 86.f), 7.f);
		for (int32 g = 0; g < 5; ++g) BrokenGlider(C, Ws + FVector(FMath::Cos(g * 1.2f + 2.f) * 520.f, FMath::Sin(g * 1.2f + 2.f) * 520.f, 0.f), g * 69.f, g, true);
		C.B.K.Rod(Cedar(0.85f), Ws + FVector(120.f, -300.f, 330.f), Ws + FVector(280.f, 260.f, 340.f), 5.f);
		C.B.K.Add(Skysilk(), FVector(3.f, 280.f, 120.f), Ws + FVector(200.f, -150.f, 300.f), FRotator(0.f, 5.f, -8.f));
		for (int32 s = 0; s < 5; ++s) C.B.K.Add(s % 2 ? Teal() : Skysilk(), FVector(12.f, 12.f, 30.f), Ws + FVector(120.f + s * 30.f, 30.f, 110.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		// The Wind Rose: eight carved winds in a ring of floor stones, the turning plate in the middle (secret).
		const FVector Rose = C.At(300.f, 0.f);
		C.B.K.Disc(TempleWhite(0.85f), Rose + FVector(0, 0, 4.f), FVector::UpVector, 900.f, 10.f);
		for (int32 w = 0; w < 8; ++w)
		{
			const float Ang = FMath::DegreesToRadians(w * 45.f);
			C.B.K.Spike(JadeTile(), Rose + FVector(FMath::Cos(Ang) * 120.f, FMath::Sin(Ang) * 120.f, 10.f), FVector(FMath::Cos(Ang), FMath::Sin(Ang), 0.f), 70.f, 290.f);
			C.B.K.Add(TempleWhite(0.7f), FVector(80.f, 80.f, 6.f), Rose + FVector(FMath::Cos(Ang) * 360.f, FMath::Sin(Ang) * 360.f, 10.f), FRotator(0.f, w * 45.f, 0.f), EWRLookMesh::Cylinder);
		}
		C.B.K.Disc(Bronze(), Rose + FVector(0, 0, 12.f), FVector::UpVector, 150.f, 8.f);
		for (int32 b = 0; b < 4; ++b) Brazier(C.B, Rose + FVector(FMath::Cos(b * PI * 0.5f + 0.78f) * 560.f, FMath::Sin(b * PI * 0.5f + 0.78f) * 560.f, 0.f), 2400.f);
		return true;
	}
	if (K == TEXT("shrine_gardens") || K == TEXT("shrine_toll") || K == TEXT("shrine_rose") || K == TEXT("shrine_barrow"))
	{
		// A lesser shrine's setting: a ring of seven wind-poles round the stone (the Gardens: flax beds; the Toll: a
		// toll-tower's stump; the Rose: a wind rose inlaid in white paving; the Barrow: kites on stakes), two braziers.
		const bool bBarrow = K == TEXT("shrine_barrow");
		for (int32 p = 0; p < 7; ++p)
		{
			const float Ang = p * 2.f * PI / 7.f;
			const FVector P = C.At(FMath::Cos(Ang) * 800.f, FMath::Sin(Ang) * 800.f);
			if (bBarrow) { C.B.K.Rod(Cedar(0.6f), P, P + FVector(0, 0, 260.f), 6.f); Kite(C.B, P + FVector(0, 0, 260.f), 100.f, Ang * 57.f, 20.f, p, true); }
			else WindPole(C.B, P, p + Var, false);
		}
		if (K == TEXT("shrine_gardens")) Beds(C, C.At(0, 1300.f), 700.f, 120.f, 3, false);
		if (K == TEXT("shrine_toll")) TollTower(C.B, C.At(-1200.f, 0.f), FVector(1, 0, 0), Zone, true);
		if (K == TEXT("shrine_rose"))
		{
			for (int32 w = 0; w < 8; ++w) { const float Ang = FMath::DegreesToRadians(w * 45.f); C.B.K.Spike(JadeTile(), C.At(FMath::Cos(Ang) * 150.f, FMath::Sin(Ang) * 150.f, 6.f), FVector(FMath::Cos(Ang), FMath::Sin(Ang), 0.f), 60.f, 300.f); }
		}
		Brazier(C.B, C.At(500.f, -350.f), 2200.f);
		Brazier(C.B, C.At(500.f, 350.f), 2200.f);
		return true;
	}

	// ---------------------------------------------------------------- the Undercloud (SKY 2.7)
	if (K == TEXT("kite_graveyard"))
	{
		// U1: thousands of death-kites on lines tied to stakes, flying unseen into the fog above, their cloth flapping
		// all round you; a path of stakes through them. (Inside the fog the far ones vanish: the near ones are built.)
		for (int32 k = 0; k < 320; ++k)
		{
			const float X = -4000.f + 8000.f * C.Rand(k * 2 + 1), Y = -3000.f + 6000.f * C.Rand(k * 2 + 2);
			if (FMath::Abs(Y - 300.f * FMath::Sin(X / 1200.f)) < 180.f) continue;   // the path through them
			if (!C.OnTop(X, Y, 300.f)) continue;
			const FVector G = C.At(X, Y);
			C.B.K.Rod(Cedar(0.55f), G - FVector(0, 0, 15.f), G + FVector(0, 0, 60.f), 5.f);
			const FVector KiteAt = G + FVector(-600.f - 600.f * C.Rand(k + 600), (C.Rand(k + 700) - 0.5f) * 300.f, 1600.f + 1800.f * C.Rand(k + 800));
			C.B.K.Rod(Rope(), G + FVector(0, 0, 55.f), KiteAt, 0.7f);
			Kite(C.B, KiteAt, 100.f + 60.f * C.Rand(k + 900), 180.f - A->GetActorRotation().Yaw, 50.f, k, C.Rand(k + 1000) < 0.4f);
		}
		for (int32 p = 0; p < 30; ++p) { const float X = -4000.f + p * 270.f; const FVector G = C.At(X, 300.f * FMath::Sin(X / 1200.f) + 220.f); C.B.K.Rod(Cedar(0.6f), G, G + FVector(0, 0, 140.f), 7.f); }
		C.B.Loop(TEXT("sky.kiteFlutter"), C.At(0, 0, 400.f), FRotator::ZeroRotator, 3.f);
		return true;
	}
	if (K == TEXT("bone_tubes"))
	{
		// U2: racks of bone tubes, each sealed with wax, each holding a rolled kite of a dead person's colours.
		for (int32 r = 0; r < 6; ++r)
		{
			const FVector Rk = C.At(-500.f + r * 200.f, 0.f);
			for (int32 e = -1; e <= 1; e += 2) C.B.K.Rod(Cedar(0.6f), Rk + FVector(0, 250.f * e, 0), Rk + FVector(0, 250.f * e, 220.f), 8.f);
			for (int32 sh = 0; sh < 4; ++sh)
			{
				C.B.K.Add(Plank(0.6f), FVector(50.f, 520.f, 5.f), Rk + FVector(0, 0, 40.f + sh * 50.f), FRotator::ZeroRotator);
				for (int32 t = 0; t < 9; ++t) BoneTube(C.B, Rk + FVector(0, -220.f + t * 55.f, 52.f + sh * 50.f), 0.f);
			}
			C.B.Box(Rk + FVector(0, 0, 110.f), FVector(28.f, 260.f, 110.f));
		}
		return true;
	}
	if (K == TEXT("bonebell"))
	{
		// U3: a bell cast of bronze and bone in a round frame (strike it: every kite in the bank shakes; WRSkyWorld).
		const FVector B0 = C.At(0, 0);
		for (int32 p = 0; p < 6; ++p)
		{
			const float Ang = p * PI / 3.f;
			C.B.K.Rod(WetStone(0.9f), B0 + FVector(FMath::Cos(Ang) * 260.f, FMath::Sin(Ang) * 260.f, 0), B0 + FVector(FMath::Cos(Ang) * 200.f, FMath::Sin(Ang) * 200.f, 420.f), 26.f);
		}
		C.B.K.Add(WetStone(0.9f), FVector(520.f, 520.f, 40.f), B0 + FVector(0, 0, 440.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		C.B.M.Lathe(B0 + FVector(0, 0, 80.f), FVector::UpVector, 320.f, { FVector(0, 150, 150), FVector(0.08f, 160, 160), FVector(0.3f, 120, 120), FVector(0.8f, 95, 95), FVector(1, 60, 60) }, 20, SRGB(0x9A, 0x74, 0x3E) * 0.6f, 0.45f);
		for (int32 b = 0; b < 10; ++b) C.B.K.Rod(Bone(), B0 + FVector(FMath::Cos(b * 0.63f) * 125.f, FMath::Sin(b * 0.63f) * 125.f, 120.f), B0 + FVector(FMath::Cos(b * 0.63f) * 100.f, FMath::Sin(b * 0.63f) * 100.f, 330.f), 9.f);
		C.B.K.Rod(Cedar(0.7f), B0 + FVector(-350.f, 0, 260.f), B0 + FVector(-120.f, 0, 250.f), 18.f);
		C.B.Box(B0 + FVector(0, 0, 230.f), FVector(160.f, 160.f, 160.f));
		return true;
	}
	if (K == TEXT("hushwing_roost"))
	{
		// U4: a shelf under an overhang lined with roosting Hushwings by day; pellets of Puffwing and Galeharrier bones.
		const FVector Sh = C.At(0, 0);
		C.B.K.Add(WetStone(0.8f), FVector(900.f, 600.f, 120.f), Sh + FVector(0, 0, 620.f), FRotator(0.f, 0.f, 6.f));
		C.B.Box(Sh + FVector(0, 0, 620.f), FVector(450.f, 300.f, 60.f), FRotator(0.f, 0.f, 6.f));
		for (int32 p = 0; p < 3; ++p) C.B.K.Rod(WetStone(0.75f), Sh + FVector(-350.f + p * 350.f, -260.f, 0), Sh + FVector(-350.f + p * 350.f, -260.f, 600.f), 60.f);
		for (int32 b = 0; b < 40; ++b) C.B.Ball(Sh + FVector(-400.f + 800.f * C.Rand(b), -200.f + 400.f * C.Rand(b + 50), 6.f), 7.f, b % 3 ? SRGB(0x5A, 0x52, 0x48) * 0.5f : BoneC(), 0.9f, 0.6f);
		return true;
	}
	if (K == TEXT("gardeners_grave"))
	{
		// U5: a round house where a family's bones sit at a table set for five, five kites on the wall and a sixth frame
		// unfinished; the island tilts 30 degrees, its beds sliding into the fog.
		RoundHouse(C.B, C.At(0, 0), 0, false);
		const FVector T0 = C.At(0, 0);
		C.B.K.Add(Cedar(0.8f), FVector(220.f, 110.f, 8.f), T0 + FVector(0, 0, 80.f), FRotator::ZeroRotator);
		C.B.Box(T0 + FVector(0, 0, 40.f), FVector(110.f, 55.f, 40.f));
		for (int32 s = 0; s < 5; ++s)
		{
			const float Ang = s * 2.f * PI / 5.f;
			Skeleton(C, T0 + FVector(FMath::Cos(Ang) * 150.f, FMath::Sin(Ang) * 100.f, 45.f), FMath::RadiansToDegrees(Ang) + 180.f, 1);
			C.B.K.Add(FSurf::Glaze(SRGB(0xB8, 0x8A, 0x5A) * 0.55f, 0.5f), FVector(22.f, 22.f, 6.f), T0 + FVector(FMath::Cos(Ang) * 70.f, FMath::Sin(Ang) * 40.f, 88.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			Kite(C.B, T0 + FVector(FMath::Cos(FMath::DegreesToRadians(-60.f + s * 30.f)) * 320.f, FMath::Sin(FMath::DegreesToRadians(-60.f + s * 30.f)) * 320.f, 200.f), 90.f, -60.f + s * 30.f, 0.f, s, false);
		}
		// The sixth frame, unfinished: the spars, no cloth.
		const FVector Six = T0 + FVector(FMath::Cos(FMath::DegreesToRadians(90.f)) * 320.f, FMath::Sin(FMath::DegreesToRadians(90.f)) * 320.f, 200.f);
		C.B.K.Rod(Cedar(0.85f), Six - FVector(0, 0, 54.f), Six + FVector(0, 0, 54.f), 3.f);
		return true;
	}
	if (K == TEXT("first_kite"))
	{
		// U6: one huge old kite, 20 m, tied to a stone anchor carved with a serpent: the Kitewrights' first, flown for
		// whoever bound Aeolith. Its line rises through the cloud (the climb out: AWRSkyLine).
		const FVector An = C.At(0, 0);
		C.B.K.Add(WetStone(0.9f), FVector(220.f, 220.f, 160.f), An + FVector(0, 0, 80.f), FRotator(0.f, 15.f, 0.f));
		C.B.Box(An + FVector(0, 0, 80.f), FVector(110.f, 110.f, 80.f), FRotator(0.f, 15.f, 0.f));
		TArray<FVector> Coil;
		for (int32 k = 0; k <= 30; ++k) { const float Ang = k * 0.42f; Coil.Add(An + FVector(FMath::Cos(Ang) * 116.f, FMath::Sin(Ang) * 116.f, 20.f + k * 4.5f)); }
		C.B.M.Tube(Coil, 14.f, 8, SRGB(0x2F, 0x8F, 0x7A) * 0.45f, 0.5f);
		C.B.K.Disc(RustIron(), An + FVector(0, 0, 165.f), FVector::UpVector, 50.f, 12.f);
		Kite(C.B, An + FVector(-900.f, 0, 2400.f), 2000.f, 180.f - A->GetActorRotation().Yaw, 60.f, 7, false);
		return true;
	}
	if (K == TEXT("newcomers_kite"))
	{
		// U7: a fresh kite of a newcomer's shirt and sticks tied with fibre, beside a camp of three bedrolls, one with a body.
		const FVector Cp = C.At(0, 0);
		for (int32 b = 0; b < 3; ++b) C.B.K.Add(FSurf::Cloth(SRGB(0x7A, 0x5A, 0x3A) * 0.5f), FVector(190.f, 70.f, 14.f), Cp + FVector(-120.f + b * 120.f, 0, 7.f), FRotator(0.f, 80.f + b * 15.f, 0.f));
		Skeleton(C, Cp + FVector(120.f, 0, 16.f), 90.f, 0);
		for (int32 k = 0; k < 6; ++k) C.B.Ball(Cp + FVector(250.f + FMath::Cos(float(k)) * 40.f, FMath::Sin(float(k)) * 40.f, 5.f), 9.f, SRGB(0x4A, 0x46, 0x42) * 0.5f, 0.9f, 0.6f);
		C.B.K.Rod(FSurf::Wood(SRGB(0x8A, 0x70, 0x50) * 0.5f), Cp + FVector(-300.f, 100.f, 0), Cp + FVector(-300.f, 100.f, 80.f), 4.f);
		C.B.K.Rod(Rope(), Cp + FVector(-300.f, 100.f, 78.f), Cp + FVector(-900.f, 200.f, 1400.f), 0.5f);
		Kite(C.B, Cp + FVector(-900.f, 200.f, 1400.f), 90.f, 180.f, 45.f, 1, false);
		return true;
	}
	if (K == TEXT("keel_vault"))
	{
		// U8: a cache cut into Tube Isle's keel, reached only by a ledge under the island (the chain bolts over the lip).
		const FVector V = C.At(0, 0) - FVector(0, 0, 2200.f);
		C.B.K.Add(WetStone(0.8f), FVector(500.f, 260.f, 30.f), V, FRotator::ZeroRotator);
		C.B.Box(V, FVector(250.f, 130.f, 15.f));
		C.B.K.Add(WetStone(0.6f), FVector(30.f, 260.f, 300.f), V + FVector(-250.f, 0, 150.f), FRotator::ZeroRotator);
		for (int32 c = 0; c < 8; ++c) C.B.K.Spike(Levistone(0.9f), V + FVector(-230.f, -110.f + c * 30.f, 60.f + 30.f * (c % 3)), FVector(1, 0, 0.2f).GetSafeNormal(), 30.f, 70.f);
		return true;
	}
	if (K == TEXT("weeping_cave")) return true;   // built into the island (WRSkyIsland.cpp)

	// ---------------------------------------------------------------- kit (SKY 2.4)
	if (K == TEXT("round_house"))
	{
		RoundHouse(C.B, C.At(0, 0), Var % 4, false);
		// The toppled variant: the house tipped over 14 degrees as the ground under one side went, half sunk in the turf.
		if (Var % 4 == 2)
		{
			A->SetActorRotation(A->GetActorRotation() + FRotator(0.f, 0.f, 14.f));
			A->AddActorWorldOffset(FVector(0, 0, -90.f));
		}
		return true;
	}
	if (K == TEXT("wind_cairn"))
	{
		const int32 N = Var % 3 == 0 ? 1 : Var % 3 == 1 ? 3 : 5;
		for (int32 p = 0; p < N; ++p)
		{
			const FVector P = C.At(p == 0 ? 0.f : FMath::Cos(p * 1.26f) * 280.f, p == 0 ? 0.f : FMath::Sin(p * 1.26f) * 280.f);
			WindPole(C.B, P, p + Var * 3, p == 0);
			WRSkyParts::AddStreamer(A, P + FVector(0, 0, 455.f), p + Var * 3);
		}
		return true;
	}
	if (K == TEXT("keel_outcrop"))
	{
		// Under an island (the node itself hangs from the root): a hanging ledge of Kitewright planks on chains under the
		// rim to stand on while you mine, and a chain ladder down to it.
		const FVector Ld = C.At(0, 0) - FVector(0, 0, 1600.f);
		C.B.K.Add(Plank(0.8f), FVector(400.f, 180.f, 10.f), Ld, FRotator::ZeroRotator);
		C.B.Box(Ld, FVector(200.f, 90.f, 6.f));
		for (int32 e = 0; e < 4; ++e) Chain(C.B, Ld + FVector((e % 2 ? 190.f : -190.f), (e / 2 ? 80.f : -80.f), 0), C.At((e % 2 ? 190.f : -190.f), (e / 2 ? 80.f : -80.f)) - FVector(0, 0, 300.f), 0.f, 22.f, 80);
		return true;
	}
	if (K == TEXT("gossamer"))
	{
		// A gossamer drift: cedars hung with silver sheets that flash in the sun, or a fog-net frame (the node is a spot).
		if (Var % 2 == 0)
		{
			for (int32 t = 0; t < 3; ++t)
			{
				const FVector Tb = C.At(-300.f + t * 300.f, (t % 2) * 200.f, -15.f);
				C.B.Tree(TEXT("cloud_cedar"), t, Tb, 1000.f + 200.f * t, FRotator(0.f, 180.f - A->GetActorRotation().Yaw, 0.f));
				for (int32 s = 0; s < 4; ++s) C.B.K.Add(Skysilk(), FVector(2.f, 160.f, 220.f), Tb + FVector(-150.f - s * 60.f, (s - 1.5f) * 70.f, 450.f + s * 80.f), FRotator(0.f, C.Rand(t * 4 + s) * 50.f, 10.f));
			}
		}
		else DriftFrame(C, C.At(0, 0), 0.f);
		C.B.Loop(TEXT("sky.gossamerGlint"), C.At(0, 0, 400.f));
		return true;
	}
	if (K == TEXT("harrier_nest"))
	{
		const FVector N0 = C.At(0, 0, Var == 1 ? 0.f : 0.f);
		for (int32 l = 0; l < 24; ++l)
		{
			const float Ang = l * 0.9f, Rr = 160.f + 60.f * C.Rand(l);
			C.B.K.Rod(Cedar(0.6f), N0 + FVector(FMath::Cos(Ang) * Rr, FMath::Sin(Ang) * Rr, 20.f + 25.f * C.Rand(l + 3)), N0 + FVector(FMath::Cos(Ang + 1.4f) * Rr, FMath::Sin(Ang + 1.4f) * Rr, 20.f + 25.f * C.Rand(l + 4)), 6.f);
		}
		C.B.K.Disc(FSurf::Cloth(SRGB(0x6A, 0x5A, 0x48) * 0.5f), N0 + FVector(0, 0, 14.f), FVector::UpVector, 320.f, 18.f);
		for (int32 f = 0; f < 6; ++f) C.B.K.Add(FSurf::Cloth(SRGB(0x2A, 0x2A, 0x30) * 0.5f), FVector(30.f, 4.f, 1.f), N0 + FVector(C.Rand(f) * 300.f - 150.f, C.Rand(f + 9) * 300.f - 150.f, 4.f), FRotator(0.f, C.Rand(f + 3) * 360.f, 0.f));
		return true;
	}
	if (K == TEXT("mane_ledge"))
	{
		// A Windmane ledge on the flank: a shelf of turf under an overhang, a nest scrape of torn grass and mane hair.
		const FVector L0 = C.At(0, 0);
		C.B.K.Disc(FSurf::Cloth(SRGB(0xC8, 0xC4, 0xB0) * 0.5f), L0 + FVector(0, 0, 10.f), FVector::UpVector, 340.f, 22.f);
		for (int32 h = 0; h < 10; ++h) C.B.K.Add(FSurf::Cloth(SRGB(0xE0, 0xE2, 0xE6) * 0.6f), FVector(60.f, 2.f, 2.f), L0 + FVector(C.Rand(h) * 260.f - 130.f, C.Rand(h + 5) * 260.f - 130.f, 24.f), FRotator(0.f, C.Rand(h + 7) * 180.f, 0.f));
		// The clutch: two pale speckled eggs the size of a head, lying in the scrape (the herd's; the nest spot's egg).
		for (int32 e = 0; e < 2; ++e)
		{
			const FVector E0 = L0 + FVector(e ? 30.f : -24.f, e ? -12.f : 16.f, 30.f);
			const float Prof[6] = { 0.55f, 0.85f, 1.f, 0.95f, 0.75f, 0.4f };
			for (int32 r = 0; r < 6; ++r) C.B.K.Disc(FSurf::Glaze(SRGB(0xE9, 0xE6, 0xDC) * 0.62f, 0.45f), E0 + FVector(0, 0, r * 7.f), FVector(e ? 0.25f : -0.2f, 0.1f, 1.f), 34.f * Prof[r], 7.5f);
			for (int32 sp = 0; sp < 7; ++sp) C.B.K.Add(FSurf::Cloth(SRGB(0x7A, 0x76, 0x70) * 0.5f), FVector(3.f, 3.f, 1.f), E0 + FVector(FMath::Cos(sp * 0.9f) * 15.f, FMath::Sin(sp * 0.9f) * 15.f, 10.f + sp * 3.f));
		}
		return true;
	}
	if (K == TEXT("fall_pool"))
	{
		// A lip pool where an island's spring gathers before it falls: a stone-rimmed basin, cloud minnows (fishing).
		const FVector P0 = C.At(0, 0);
		C.B.K.Disc(LagoonWater(), P0 + FVector(0, 0, -20.f), FVector::UpVector, 700.f, 6.f);
		for (int32 s = 0; s < 16; ++s) { const float Ang = s * PI / 8.f; C.B.Rock(s, FVector(110.f, 80.f, 60.f), P0 + FVector(FMath::Cos(Ang) * 380.f, FMath::Sin(Ang) * 380.f, -10.f), FRotator(0.f, FMath::RadiansToDegrees(Ang), 0.f), RimColour(Zone, 0.5f) * 0.6f, true, 25.f); }
		return true;
	}
	if (K == TEXT("kill_scene"))
	{
		// A kill set-piece: a Galeharrier on a Puffwing (W1) or Gallowshrikes at a larder (W2): a fresh carcass, feathers
		// in a burst round it, blood on the turf (the creature itself is spawned there by WRSkyWorld's spawner).
		const FVector P0 = C.At(0, 0, 6.f);
		PuffwingBody(C, P0, C.Rand(1) * 360.f, false);
		for (int32 f = 0; f < 14; ++f) C.B.K.Add(FSurf::Cloth(SRGB(0xE8, 0xE6, 0xDE) * 0.6f), FVector(12.f, 3.f, 1.f), P0 + FVector(FMath::Cos(f * 0.45f) * (60.f + 120.f * C.Rand(f)), FMath::Sin(f * 0.45f) * (60.f + 120.f * C.Rand(f)), -4.f), FRotator(0.f, f * 25.f, 0.f));
		C.B.K.Disc(FSurf::Glaze(SRGB(0x60, 0x10, 0x10) * 0.5f, 0.4f), P0 - FVector(0, 0, 5.f), FVector::UpVector, 90.f, 1.f);
		if (Var == 1) { C.B.K.Rod(FSurf::Wood(SRGB(0x2A, 0x22, 0x1C) * 0.6f), C.At(200.f, 0), C.At(200.f, 0, 180.f), 6.f); PuffwingBody(C, C.At(200.f, 0, 160.f), 0.f, true); }
		return true;
	}
	if (K == TEXT("vista"))
	{
		// A high lung ta stone: a standing stone 4 m tall, prayer-carved, cloth tied round it, five poles; it reveals 400 m.
		C.B.K.Add(Granite(0.9f), FVector(120.f, 70.f, 400.f), C.At(0, 0, 200.f), FRotator(0.f, 0.f, 3.f));
		C.B.Box(C.At(0, 0, 200.f), FVector(60.f, 35.f, 200.f));
		for (int32 b = 0; b < 3; ++b) C.B.K.Add(Cloth(b + Var, 0.8f), FVector(124.f, 74.f, 20.f), C.At(0, 0, 120.f + b * 40.f), FRotator(0.f, 0.f, 3.f));
		for (int32 p = 0; p < 5; ++p) { const FVector P = C.At(FMath::Cos(p * 1.26f) * 400.f, FMath::Sin(p * 1.26f) * 400.f); WindPole(C.B, P, p, false); }
		return true;
	}
	if (K == TEXT("teacher_gust"))
	{
		// The W1 hazard teacher: a single gust lane along the cliff lip with three wind-poles, and a short drop (5 m) onto
		// turf: a step cut in the lip where the gust's push lands you hurt, not dead. The lane is WRSkyAir's.
		for (int32 p = -1; p <= 1; ++p) { const FVector P = C.At(-200.f, p * 1800.f); WindPole(C.B, P, p + 2, true); WRSkyParts::AddStreamer(A, P + FVector(0, 0, 455.f), p + 2); }
		const float Down = C.G(0, 0) - 500.f;
		C.B.K.Add(CliffGranite(0.9f), FVector(600.f, 4200.f, 500.f), FVector(500.f, 0, Down - 250.f), FRotator::ZeroRotator);
		C.B.Box(FVector(500.f, 0, Down - 250.f), FVector(300.f, 2100.f, 250.f));
		C.B.K.Add(Turf(), FVector(600.f, 4200.f, 20.f), FVector(500.f, 0, Down + 8.f), FRotator::ZeroRotator);
		return true;
	}
	if (K == TEXT("teacher_stone")) return true;   // the stone itself is AWRSkyStones (teacher line)
	return false;
}
