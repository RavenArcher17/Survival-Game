// The Desert's landmarks, shrines' settings, camps, caves, the Hourhold, brine crusts, flood wadis and the decoration
// (cooks/biome-desert.md 2.3-2.6, cooks/build-biome-desert.md 2.2-2.6), modelled in code at real scale on the forest's
// FBuild. Every piece's comment says its job in the land and what it meets.
#include "WRDesertSceneryParts.h"
#include "Wildreach.h"
#include "WRFx.h"
#include "WRTuning.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"

namespace WRDesertParts
{
	// ------------------------------------------------------------------------------------------------ helpers

	/** A rock chamber seen from inside (a cave room's walls and roof): rings of points round Centre up to a cap, jittered
	 *  like wind-cut sandstone, every triangle facing the middle. Floor not included. */
	void DsDomeInward(FBuild& B, const FVector& Centre, float R, float H, int32 Sides, const FLinearColor& Col, float Jitter)
	{
		const int32 Rings = 6;
		TArray<TArray<FVector>> P;
		for (int32 r = 0; r <= Rings; ++r)
		{
			const float A = float(r) / Rings * 0.5f * PI;
			TArray<FVector>& Ring = P.AddDefaulted_GetRef();
			for (int32 s = 0; s < Sides; ++s)
			{
				const float T = float(s) / Sides * 2.f * PI;
				const float J = 1.f + Jitter * (B.Rand(r * 97 + s * 13) - 0.5f);
				const float Rr = R * FMath::Cos(A) * J;
				Ring.Add(Centre + FVector(FMath::Cos(T) * Rr, FMath::Sin(T) * Rr, H * FMath::Sin(A) * (r == 0 ? 0.f : J)));
			}
		}
		const FColor C = FWRToolMesh::Pack(Col, 0.9f);
		const auto Tri = [&](const FVector& X, const FVector& Y, const FVector& Z)
		{
			const FVector N = FVector::CrossProduct(Y - X, Z - X);
			const bool bOut = FVector::DotProduct(N, (X + Y + Z) / 3.f - Centre) > 0.f;
			if (bOut) B.M.Tri(X, Y, Z, C, FVector2D(0, 0), FVector2D(1, 0), FVector2D(0, 1));
			else B.M.Tri(X, Z, Y, C, FVector2D(0, 0), FVector2D(0, 1), FVector2D(1, 0));
		};
		for (int32 r = 0; r < Rings; ++r) for (int32 s = 0; s < Sides; ++s)
		{
			const int32 s1 = (s + 1) % Sides;
			Tri(P[r][s], P[r][s1], P[r + 1][s1]);
			Tri(P[r][s], P[r + 1][s1], P[r + 1][s]);
		}
	}

	/** Collision for a round rock room: a ring of wall boxes and a roof slab. */
	void DsRoundRoomCollision(FBuild& B, const FVector& Centre, float R, float H)
	{
		const int32 N = 14;
		for (int32 i = 0; i < N; ++i)
		{
			const float A = (i + 0.5f) / N * 2.f * PI;
			B.Box(Centre + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * (R + 60.f) + FVector(0, 0, H * 0.5f), FVector(60.f, PI * R / N + 40.f, H * 0.5f + 60.f), FRotator(0.f, FMath::RadiansToDegrees(A), 0.f));
		}
		B.Box(Centre + FVector(0, 0, H + 60.f), FVector(R * 0.95f, R * 0.95f, 60.f));
		B.Box(Centre - FVector(0, 0, 40.f), FVector(R, R, 40.f));
	}

	/** A sloping gallery from A to Bt (both at floor level, local cm): a 4 m dressed passage (walls, ceiling slabs, a floor
	 *  of steps where it is steep), lit by sunstone sconces; collision walls, roof and floor. Rough: cut rock instead of
	 *  masonry (a cave's descent). */
	void DsGallery(FBuild& B, const FVector& A, const FVector& Bt, float W, float H, bool bRough, int32 Seed, bool bLamps = true)
	{
		const FVector D = Bt - A;
		const float L2 = D.Size2D();
		if (L2 < 50.f) return;
		const FVector Dir2 = FVector(D.X, D.Y, 0.f) / L2;
		const FVector Side(-Dir2.Y, Dir2.X, 0.f);
		const FRotator R(0.f, Dir2.Rotation().Yaw, 0.f);
		const float Slope = D.Z / L2;
		const bool bSteps = FMath::Abs(Slope) > 0.18f;
		const FSurf Wall = bRough ? DsRedRock(0.7f) : DsSandstone(0.8f);
		const FSurf Floor = bRough ? DsRedRock(0.55f) : DsSandstone(0.65f);
		const int32 Seg = FMath::Max(1, FMath::RoundToInt(L2 / 200.f));
		for (int32 s = 0; s < Seg; ++s)
		{
			const float T0 = float(s) / Seg, T1 = float(s + 1) / Seg;
			const FVector P0 = A + D * T0, P1 = A + D * T1, Mid = (P0 + P1) * 0.5f;
			const float SegL = L2 / Seg;
			if (bSteps) B.K.Add(Floor.Dim(0.95f + 0.1f * B.Rand(s + Seed)), FVector(SegL, W, 30.f), FVector(Mid.X, Mid.Y, FMath::Min(P0.Z, P1.Z) + 15.f + FMath::Abs(P1.Z - P0.Z) * 0.5f - 15.f), R);
			else B.K.Add(Floor.Dim(0.95f + 0.1f * B.Rand(s + Seed)), FVector(SegL, W, 20.f), Mid - FVector(0, 0, 10.f), FRotator(-FMath::RadiansToDegrees(FMath::Atan(Slope)), R.Yaw, 0.f));
			for (int32 sd = -1; sd <= 1; sd += 2)
			{
				B.K.Add(Wall.Dim(0.9f + 0.15f * B.Rand(s * 3 + sd + Seed)), FVector(SegL + 10.f, 50.f, H + 40.f), Mid + Side * sd * (W * 0.5f + 25.f) + FVector(0, 0, H * 0.5f - 10.f), R);
				if (bLamps && s % 4 == 2 && sd > 0)
				{
					const FVector Sc = Mid + Side * (W * 0.5f - 10.f) + FVector(0, 0, H * 0.6f);
					B.K.Add(DsBronze(), FVector(20.f, 14.f, 24.f), Sc, R);
					B.N.Add(DsSunstone(3.f), FVector(12.f, 10.f, 14.f), Sc + FVector(0, 0, 16.f), R);
					B.Lamp(Sc + Side * -40.f, DsRGB(0xF2, 0xA2, 0x3A), 700.f, 900.f);
				}
			}
			B.K.Add(Wall.Dim(0.7f), FVector(SegL + 10.f, W + 100.f, 40.f), Mid + FVector(0, 0, H + 10.f), R);
		}
		// Collision: two walls, the roof, the floor (a sloped box: the steps are drawn, the ramp is walked).
		for (int32 sd = -1; sd <= 1; sd += 2) B.SegBox(A + Side * sd * (W * 0.5f + 30.f) + FVector(0, 0, H * 0.5f), Bt + Side * sd * (W * 0.5f + 30.f) + FVector(0, 0, H * 0.5f), 30.f, H * 0.5f + 20.f);
		B.SegBox(A + FVector(0, 0, H + 30.f), Bt + FVector(0, 0, H + 30.f), W * 0.5f + 60.f, 30.f);
		B.SegBox(A - FVector(0, 0, 25.f), Bt - FVector(0, 0, 25.f), W * 0.5f + 10.f, 25.f);
	}

	// ------------------------------------------------------------------------------------------------ L1 the Haboob Wall

	void HaboobWall(FDsCtx& C)
	{
		// The sandstorm belt's face (cook 2.3 L1): a 300 m wall of moving dust along the D5 line that never moves, seen from
		// the whole desert, lightning flickering inside. Built as dust: rows of slow billowing emitters along its length (no
		// collision: it's air). The belt behind it is the hazard (WRDesertHazards).
		FBuild& B = C.B;
		C.A->SetActorRotation(FRotator::ZeroRotator);
		const float West = C.D.BeltWest, East = C.D.BeltEast - 15000.f;
		const FVector O = C.A->GetActorLocation();
		int32 k = 0;
		for (float X = West; X <= East; X += 14000.f, ++k)
		{
			const FVector Foot(X - O.X, 800.f * FMath::Sin(X * 0.0007f), 0.f);
			const float G = B.G(Foot.X, Foot.Y, 0.f);
			B.Loop(TEXT("desert.haboob"), FVector(Foot.X, Foot.Y, G + 6000.f), FRotator::ZeroRotator, 1.f);
			B.Loop(TEXT("desert.haboob_top"), FVector(Foot.X + 3000.f, Foot.Y - 3000.f, G + 24000.f), FRotator::ZeroRotator, 1.f);
			if (k % 3 == 1) B.Loop(TEXT("desert.haboob_flash"), FVector(Foot.X, Foot.Y - 6000.f, G + 15000.f));
		}
	}

	// ------------------------------------------------------------------------------------------------ L2 Ahrumet's Gnomon

	void Gnomon(FDsCtx& C)
	{
		// The Sundial Plaza (cook 2.14): the dial is the land's limestone disc (the ground's plaza surface); here its kerb
		// of dressed blocks half drowned in drift on the west, the radial joints, the hour marks on the 50 m ring, the
		// bronze meridian strip from the gnomon north to the throne, the throne itself, and the gnomon: a 30 m black basalt
		// obelisk with a bronze pyramidion and its sighting hole 3 m under the tip (the light shaft that wakes Ahrumet comes
		// through it). The 12 hour-stelae are the arena's (the boss beams break them): WRDesertBosses places them.
		FBuild& B = C.B;
		C.A->SetActorRotation(FRotator::ZeroRotator);
		const float G = B.G(0, 0, 0.f);
		const float Rd = C.D.ArenaNum(TEXT("ahrumet"), TEXT("radius"), 70.f) * 100.f;
		for (int32 s = 0; s < 72; ++s)
		{
			const FRotator R(0.f, s * 5.f, 0.f);
			const FVector P = R.RotateVector(FVector(Rd, 0, 0));
			const float Gz = B.G(P.X, P.Y, G);
			const bool bWest = FMath::Abs(FMath::FindDeltaAngleDegrees(s * 5.f, 180.f)) < 50.f;
			B.K.Add(DsLimestone(0.9f + 0.12f * B.Rand(s)), FVector(160.f, 590.f, bWest ? 40.f : 70.f), FVector(P.X, P.Y, Gz + (bWest ? 0.f : 20.f)), R);
			if (bWest) B.K.Add(DsSandFill(0.95f), FVector(500.f, 640.f, 90.f), R.RotateVector(FVector(Rd + 150.f, 0, 0)) + FVector(0, 0, Gz - 10.f), R, EWRLookMesh::Cylinder);
		}
		for (int32 j = 0; j < 24; ++j)
		{
			const FRotator R(0.f, j * 15.f, 0.f);
			B.K.Rod(DsLimestone(0.6f), R.RotateVector(FVector(600.f, 0, G + 2.f)), R.RotateVector(FVector(Rd - 120.f, 0, G + 2.f)), 4.f, true);
		}
		// The hour ring at 25 m: twelve inlaid bronze marks (the stelae stand on them).
		const float Ring = C.D.ArenaNum(TEXT("ahrumet"), TEXT("stelaRing"), 25.f) * 100.f;
		B.K.Ring(DsLimestone(0.75f), FVector(0, 0, G + 1.f), Ring, 4.f, 30.f, 48);
		for (int32 h = 0; h < 12; ++h) B.K.Add(DsBronze(), FVector(60.f, 220.f, 4.f), FRotator(0.f, h * 30.f, 0.f).RotateVector(FVector(Ring, 0, G + 2.f)), FRotator(0.f, h * 30.f, 0.f));
		// The meridian strip: bronze, 40 cm, from the gnomon's foot north to the throne, notched every 2 m.
		const float Throne = -(Rd - 600.f);
		B.K.Add(DsBronze(), FVector(40.f, FMath::Abs(Throne) - 300.f, 4.f), FVector(0, Throne * 0.5f - 150.f, G + 2.f));
		for (float Y = -400.f; Y > Throne + 200.f; Y -= 200.f) B.K.Add(DsBronzeDull(), FVector(70.f, 6.f, 5.f), FVector(0, Y, G + 3.f));
		// The gnomon: 3 m square at the foot tapering to 1.8 m at 27 m, the sighting hole (a bronze-lined frame) from 25.8 to
		// 27.0 m, a bronze-capped pyramidion to 30 m. Courses of black basalt with fine joints.
		const float H = 3000.f, HoleZ0 = 2580.f, HoleZ1 = 2700.f;
		for (int32 c = 0; c < 26; ++c)
		{
			const float Z0 = c * 100.f;
			const float W0 = FMath::Lerp(300.f, 180.f, Z0 / 2700.f);
			B.K.Add(DsBasalt(0.92f + 0.12f * B.Rand(c)), FVector(W0, W0, 97.f), FVector(0, 0, G + Z0 + 50.f));
		}
		const float WH = 182.f;
		for (int32 s = -1; s <= 1; s += 2) B.K.Add(DsBasalt(1.f), FVector(WH, 40.f, HoleZ1 - HoleZ0), FVector(0, s * (WH * 0.5f - 20.f), G + (HoleZ0 + HoleZ1) * 0.5f));
		B.K.Ring(DsBronze(), FVector(0, 0, G + (HoleZ0 + HoleZ1) * 0.5f), 60.f, WH * 0.5f, 10.f, 14, 0.f, 360.f, 1.f);
		B.K.Add(DsBasalt(1.f), FVector(WH, WH, 60.f), FVector(0, 0, G + HoleZ1 + 30.f));
		B.K.Add(DsGold(), FVector(WH, WH, H - HoleZ1 - 60.f), FVector(0, 0, G + HoleZ1 + 60.f + (H - HoleZ1 - 60.f) * 0.5f), FRotator::ZeroRotator, EWRLookMesh::Cone);
		B.Box(FVector(0, 0, G + H * 0.5f), FVector(140.f, 140.f, H * 0.5f));
		// The throne on the north edge, facing the gnomon: a block seat 8 m wide, its back 12 m, lion-paw feet, the sun
		// ring's socket behind.
		const FVector T(0, Throne, B.G(0, Throne, G));
		B.K.Add(DsSandstone(0.95f), FVector(800.f, 600.f, 450.f), T + FVector(0, -100.f, 225.f));
		B.K.Add(DsSandstone(1.f), FVector(820.f, 160.f, 1200.f), T + FVector(0, -380.f, 600.f));
		for (int32 s = -1; s <= 1; s += 2) B.K.Add(DsSandstone(0.9f), FVector(120.f, 560.f, 520.f), T + FVector(s * 460.f, -100.f, 260.f));
		B.K.Ring(DsBronzeDull(), T + FVector(0, -470.f, 1100.f), 300.f, 40.f, 60.f, 24);
		B.Box(T + FVector(0, -200.f, 600.f), FVector(460.f, 350.f, 600.f));
		// Night: the bronze catches the moon; the hole's lining a faint gold.
		B.N.Ring(FSurf::Lit(DsRGB(0xE8, 0xB2, 0x30) * 0.5f, 0.8f), FVector(0, 0, G + (HoleZ0 + HoleZ1) * 0.5f), 56.f, 20.f, 6.f, 14);
	}

	// ------------------------------------------------------------------------------------------------ L3 the Broken Sun Gate

	void SunGate(FDsCtx& C)
	{
		// The ruined sun-temple's gate (cook 2.3 L3; the Great Shrine stands in its court, SPEC "shrine in a ruined
		// sun-temple"): two 22 m pylons 18 m apart facing west down the wadi road, the lintel fallen across the gap in three
		// pieces, a winged sun disc carved on each; behind them the court's paving, its colonnade stumps, the brazier's
		// dais (the brazier itself is the shrine's own).
		FBuild& B = C.B;
		const float G = B.G(0, 0, 0.f);
		for (int32 s = -1; s <= 1; s += 2) DsPylon(B, FVector(-1500.f, s * 1400.f, B.G(-1500.f, s * 1400.f, G) - 60.f), 0.f, 1000.f, 600.f, 2200.f, s > 0 ? 0.08f : 0.f, 7 + s);
		// The lintel's three pieces lying in the gap, one leaning on the north pylon's foot.
		B.K.Add(DsSandstone(0.95f), FVector(400.f, 700.f, 220.f), FVector(-1400.f, -300.f, G + 60.f), FRotator(4.f, 12.f, 6.f));
		B.K.Add(DsSandstone(0.9f), FVector(420.f, 600.f, 220.f), FVector(-1700.f, 450.f, G + 70.f), FRotator(-8.f, -20.f, 3.f));
		B.K.Add(DsSandstone(1.f), FVector(380.f, 620.f, 220.f), FVector(-1500.f, -820.f, G + 280.f), FRotator(0.f, 6.f, -28.f));
		B.K.Disc(DsSandstone(1.15f), FVector(-1680.f, 450.f, G + 190.f), FVector(0.2f, 0.f, 1.f).GetSafeNormal(), 200.f, 12.f);    // its carved sun, face up
		B.Box(FVector(-1400.f, -300.f, G + 60.f), FVector(200.f, 350.f, 110.f), FRotator(4.f, 12.f, 6.f));
		B.Box(FVector(-1700.f, 450.f, G + 70.f), FVector(210.f, 300.f, 110.f), FRotator(-8.f, -20.f, 3.f));
		// The court: worn paving 30 x 30 m, the colonnade's stumps down both sides, the dais in the middle.
		for (int32 x = -6; x <= 6; ++x) for (int32 y = -6; y <= 6; ++y)
		{
			if (B.Rand(x * 31 + y * 7) < 0.15f) continue;                    // lifted, sanded over
			B.K.Add(DsSandstone(0.8f + 0.2f * B.Rand(x * 13 + y)), FVector(236.f, 236.f, 16.f), FVector(x * 240.f, y * 240.f, B.G(x * 240.f, y * 240.f, G) + 4.f));
		}
		for (int32 c = 0; c < 6; ++c) for (int32 s = -1; s <= 1; s += 2)
		{
			const FVector P(-1000.f + c * 400.f, s * 1250.f, 0.f);
			const float H = 120.f + 380.f * B.Rand(c * 2 + s + 9);
			B.K.Add(DsSandstone(0.95f), FVector(110.f, 110.f, H), FVector(P.X, P.Y, B.G(P.X, P.Y, G) + H * 0.5f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			B.Box(FVector(P.X, P.Y, B.G(P.X, P.Y, G) + H * 0.5f), FVector(55.f, 55.f, H * 0.5f));
		}
		for (int32 st = 0; st < 3; ++st) B.K.Add(DsSandstone(1.f - 0.05f * st), FVector(700.f - st * 160.f, 700.f - st * 160.f, 30.f), FVector(0, 0, G + 15.f + st * 30.f));
		B.Box(FVector(0, 0, G + 45.f), FVector(350.f, 350.f, 45.f));
	}

	// ------------------------------------------------------------------------------------------------ L4 the Mother Dune

	void MotherDune(FDsCtx& C)
	{
		// The 140 m star dune is the ground itself; at its east foot three ventifacts (rocks cut into keels by the
		// wind-blown sand, their faces fluted along the wind) mark where the boom is loudest and Ssarrok's Hollow opens.
		FBuild& B = C.B;
		C.A->SetActorRotation(FRotator::ZeroRotator);
		for (int32 v = 0; v < 3; ++v)
		{
			const FVector P = FRotator(0.f, -30.f + v * 25.f, 0.f).RotateVector(FVector(23000.f + v * 900.f, 0, 0));
			const float Gz = B.G(P.X, P.Y, 0.f);
			B.Rock(v, FVector(420.f, 220.f, 300.f), FVector(P.X, P.Y, Gz), FRotator(0.f, 90.f, 0.f), DsRockTint() * 0.9f, true, 40.f);
			for (int32 f = 0; f < 5; ++f) B.K.Add(DsRedRock(0.7f), FVector(400.f, 10.f, 14.f), FVector(P.X, P.Y + (f - 2) * 40.f, Gz + 80.f + f * 40.f), FRotator(0.f, 90.f, 0.f));
		}
		B.Loop(TEXT("desert.crest_stream"), FVector(0, 0, B.G(0, 0, 0.f) + 100.f), FRotator::ZeroRotator, 2.f);
	}

	// ------------------------------------------------------------------------------------------------ L5 the Whale Road

	void WhaleRoad(FDsCtx& C)
	{
		// The old sea (cook 1.2: Wadi Al-Hitan): nine whales of 20-40 m along a 600 m line, ribs up, half in the sand, all
		// lying the same way (the current that left them). The biggest's skull carries the Erg Shrine.
		FBuild& B = C.B;
		C.A->SetActorRotation(FRotator::ZeroRotator);
		if (C.D.WhaleRoad.Num() < 2) return;
		const FVector A = C.L(C.D.WhaleRoad[0]), Bt = C.L(C.D.WhaleRoad[1]);
		const FWRDesertData::FPlace* Shrine = C.D.FindSite(TEXT("shrine_erg"));
		const FVector S = Shrine ? C.L(Shrine->At) : A;
		for (int32 w = 0; w < 9; ++w)
		{
			const float T = (w + 0.5f) / 9.f;
			FVector P = FMath::Lerp(A, Bt, T) + FVector(0, (B.Rand(w * 7) - 0.5f) * 3000.f, 0.f);
			const bool bBig = w == 0;
			if (bBig) P = FVector(S.X - 1000.f, S.Y, 0.f);
			const float Len = bBig ? 4000.f : 2000.f + 1500.f * B.Rand(w + 3);
			DsWhale(B, FVector(P.X, P.Y, B.G(P.X, P.Y, 0.f)), 200.f + 12.f * (B.Rand(w) - 0.5f), Len, w * 17 + 3, 0.35f + 0.4f * B.Rand(w * 5));
		}
	}

	// ------------------------------------------------------------------------------------------------ L6 the Keelyard

	void Keelyard(FDsCtx& C)
	{
		// Twelve sun-barques stood on end in an arc like a fence (the Sunwatch dragged the sun-boats here when the sea left
		// and made a wall of them): cedar hulls 10-14 m tall, sunk 3 m, their curled posts on top.
		FBuild& B = C.B;
		for (int32 h = 0; h < 12; ++h)
		{
			const float A = FMath::DegreesToRadians(-60.f + h * 11.f);
			const FVector P(FMath::Cos(A) * 2600.f, FMath::Sin(A) * 2600.f, 0.f);
			const float Gz = B.G(P.X, P.Y, 0.f);
			const float Len = 1100.f + 300.f * B.Rand(h);
			DsHull(B, FVector(P.X, P.Y, Gz + Len * 0.5f - 300.f), FRotator(86.f, FMath::RadiansToDegrees(A) + 90.f, 0.f), Len, 300.f, 140.f, 1, 0.f, 0.3f * B.Rand(h + 5), h * 11 + 2);
			B.Box(FVector(P.X, P.Y, Gz + Len * 0.4f), FVector(90.f, 150.f, Len * 0.4f), FRotator(0.f, FMath::RadiansToDegrees(A) + 90.f, 0.f));
		}
	}

	// ------------------------------------------------------------------------------------------------ L7 the Tall Wreck

	void TallWreck(FDsCtx& C)
	{
		// A newcomer three-master (32 m) thrown 200 m inland onto a dune crest, heeled so its masts lean 30 degrees; the
		// rope ladder from its rail (S1 dresses the casks and the graves round it).
		FBuild& B = C.B;
		const float G = B.G(0, 0, 0.f);
		DsHull(B, FVector(0, 0, G), FRotator(4.f, 0.f, 30.f), 3200.f, 800.f, 420.f, 0, 120.f, 0.25f, 77);
		// The rope ladder hanging from the rail on the low side to the sand.
		const FVector Rail = FRotator(4.f, 0.f, 30.f).RotateVector(FVector(-200.f, -400.f, 420.f)) + FVector(0, 0, G - 120.f);
		const FVector Foot(Rail.X, Rail.Y - 60.f, B.G(Rail.X, Rail.Y - 60.f, G));
		for (int32 sd = -1; sd <= 1; sd += 2) B.Line(Rail + FVector(sd * 25.f, 0, 0), Foot + FVector(sd * 25.f, 0, 0), 2.f, 10.f, DsRGB(0x9C, 0x84, 0x60) * 0.45f, 8);
		for (int32 r = 1; r < 10; ++r) { const FVector P = FMath::Lerp(Rail, Foot, r / 10.f); B.K.Rod(DsSunBleach(0.7f), P - FVector(25.f, 0, 0), P + FVector(25.f, 0, 0), 4.f); }
		B.Lamp(FVector(0, 0, G + 900.f), DsRGB(0xFF, 0xC8, 0x80), 300.f, 800.f);   // a ship's lamp still hung in the rigging
	}

	// ------------------------------------------------------------------------------------------------ L8 the Fallen Hour

	void FallenHour(FDsCtx& C)
	{
		// Ahrumet's twin, pulled down on purpose (cook 2.20.4): a 12 m head on its side on the Pan, sand to the eyes, the
		// face broken; ropes still round its neck and the tow-sledges they hauled it with lying under the sand's edge. The
		// Jaw (H-E2) opens under its chin (the Hourhold's way).
		FBuild& B = C.B;
		const float G = B.G(0, 0, 0.f);
		DsColossusHead(B, FVector(0, 0, G - 350.f), 0.f, 1200.f, true, true, 31);
		B.K.Add(DsSandFill(0.95f), FVector(1600.f, 900.f, 260.f), FVector(0, 400.f, G - 20.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		for (int32 r = 0; r < 4; ++r)
		{
			const FVector Neck(-500.f + r * 30.f, -200.f + r * 120.f, G + 200.f);
			const FVector Out = Neck + FRotator(0.f, 200.f + r * 12.f, 0.f).Vector() * (2200.f + 400.f * B.Rand(r));
			B.Line(Neck, FVector(Out.X, Out.Y, B.G(Out.X, Out.Y, G) + 10.f), 4.f, 60.f, DsRGB(0x7A, 0x66, 0x48) * 0.45f, 10);
		}
		for (int32 s = 0; s < 3; ++s)
		{
			const FVector P(-1800.f - s * 300.f, -600.f + s * 500.f, 0.f);
			const float Gz = B.G(P.X, P.Y, G);
			for (int32 sd = -1; sd <= 1; sd += 2) B.K.Add(DsSunBleach(0.7f), FVector(500.f, 30.f, 26.f), FVector(P.X, P.Y + sd * 70.f, Gz + 6.f), FRotator(0.f, 20.f, 0.f));
			for (int32 x = 0; x < 4; ++x) B.K.Add(DsSunBleach(0.65f), FVector(26.f, 180.f, 16.f), FVector(P.X - 180.f + x * 120.f, P.Y, Gz + 20.f), FRotator(0.f, 20.f, 0.f));
		}
	}

	// ------------------------------------------------------------------------------------------------ L9 the Needles

	void Needles(FDsCtx& C)
	{
		// A forest of 30-80 m red hoodoos (cook 2.3 L9): the Carrion kites' roost, stick nests on the tops, two rope bridges
		// slung between the tallest (a Sunwatch way across the canyon maze), bones dropped below the nests.
		FBuild& B = C.B;
		C.A->SetActorRotation(FRotator::ZeroRotator);
		TArray<FVector> Tops;
		for (int32 n = 0; n < 34; ++n)
		{
			const float A = B.Rand(n * 3 + 1) * 2.f * PI, D = 2500.f + 22000.f * FMath::Sqrt(B.Rand(n * 7 + 2));
			const FVector P(FMath::Cos(A) * D, FMath::Sin(A) * D, 0.f);
			const float H = n < 4 ? 7000.f + 1000.f * B.Rand(n) : 3000.f + 3500.f * B.Rand(n * 11);
			const float Gz = B.G(P.X, P.Y, 0.f);
			DsHoodoo(B, FVector(P.X, P.Y, Gz), H, 500.f + 400.f * B.Rand(n + 5), n * 13 + 1);
			Tops.Add(FVector(P.X, P.Y, Gz + H + 60.f));
			if (B.Rand(n * 5) > 0.45f)
			{
				// A kite's nest: a broad mat of sticks on the cap, bones and a red-earth bath stain on the rock below.
				for (int32 s = 0; s < 10; ++s) B.Limb(Tops.Last() + FRotator(0.f, s * 36.f, 0.f).RotateVector(FVector(160.f, 0, 10.f)), Tops.Last() + FRotator(0.f, s * 36.f + 120.f, 0.f).RotateVector(FVector(180.f, 0, 20.f)), 5.f, 3.f, DsRGB(0x6A, 0x58, 0x44) * 0.5f, 4);
				B.K.Add(DsBone(0.9f), FVector(60.f, 14.f, 10.f), Tops.Last() + FVector(40.f, 20.f, 20.f), FRotator(0.f, n * 40.f, 0.f));
			}
		}
		for (int32 b = 0; b < 2; ++b)
		{
			const FVector A = Tops[b * 2] - FVector(0, 0, 300.f), Bt = Tops[b * 2 + 1] - FVector(0, 0, 300.f);
			const float Sag = FVector::Dist(A, Bt) * 0.06f;
			for (int32 sd = -1; sd <= 1; sd += 2) B.Line(A + FVector(0, sd * 70.f, 100.f), Bt + FVector(0, sd * 70.f, 100.f), 3.f, Sag, DsRGB(0x9C, 0x84, 0x60) * 0.45f, 16);
			const int32 N = FMath::Max(6, int32(FVector::Dist(A, Bt) / 60.f));
			for (int32 p = 1; p < N; ++p)
			{
				const FVector P = Sag > 0.f ? FMath::Lerp(A, Bt, float(p) / N) - FVector(0, 0, 4.f * Sag * (float(p) / N) * (1.f - float(p) / N)) : FMath::Lerp(A, Bt, float(p) / N);
				B.K.Add(DsSunBleach(0.75f), FVector(26.f, 130.f, 8.f), P, (Bt - A).Rotation() + FRotator(0.f, 0.f, 0.f));
				if (p % 3 == 0) B.Box(P, FVector(30.f, 65.f, 6.f), (Bt - A).Rotation());
			}
		}
	}

	// ------------------------------------------------------------------------------------------------ L10 the Glass Canyon

	void GlassCanyon(FDsCtx& C)
	{
		// The canyon mouth that flashes green and black (cook 2.3 L10): lightning off the Storm peaks fused its sand to
		// glass. Its walls are lined with glass sheets and runs of fulgurite tubes, its floor glassy; the Glass Empress's
		// lair is its inner bowl (the arena's).
		FBuild& B = C.B;
		C.A->SetActorRotation(FRotator::ZeroRotator);
		for (int32 s = 0; s < 40; ++s)
		{
			const float X = -18000.f + s * 500.f;
			for (int32 sd = -1; sd <= 1; sd += 2)
			{
				const float Y = sd * (1500.f + 300.f * B.Rand(s * 2 + sd)) - 0.15f * X * 0 + 0.15f * X;
				const float Gz = B.G(X, Y, 0.f);
				B.K.Add(B.Rand(s + sd) > 0.3f ? DsGlassBlack() : DsSunglass(), FVector(480.f, 40.f, 600.f + 500.f * B.Rand(s * 5 + sd)), FVector(X, Y, Gz + 300.f), FRotator(sd * 12.f, 8.f * (B.Rand(s) - 0.5f), 0.f));
				if (s % 3 == 0) for (int32 f = 0; f < 4; ++f) B.K.Spike(DsSunglass(), FVector(X + f * 60.f, Y - sd * 60.f, Gz), FVector(0.1f * f, -sd * 0.2f, 1.f).GetSafeNormal(), 12.f, 120.f + 80.f * B.Rand(f + s));
			}
		}
	}

	// ------------------------------------------------------------------------------------------------ L11 the Sevenfold Wells

	void SevenfoldWells(FDsCtx& C)
	{
		// The biggest oasis (cook 2.3 L11), the Mirage Matriarch's water garden: a crown of 40 date palms round a stone qanat
		// tower; the tower lifts water from the qanat with a bucket wheel turned by the wind's gear (it turns, 6 rpm), into a
		// stone header tank; channels run from the tank to seven stone-lipped pools; a low wall rings the garden with four
		// gates. The palms stand where the channels' seepage reaches (between the pools and the wall).
		FBuild& B = C.B;
		C.A->SetActorRotation(FRotator::ZeroRotator);
		const float G = B.G(0, 0, 0.f);
		// The tower: 12 m square-ish (4 m on a side), coursed, a stair up its side, the header tank on top.
		for (int32 c = 0; c < 26; ++c) B.K.Add(DsSandstone(0.9f + 0.12f * B.Rand(c)), FVector(400.f, 400.f, 44.f), FVector(0, 0, G + c * 46.f + 22.f));
		B.Box(FVector(0, 0, G + 600.f), FVector(200.f, 200.f, 600.f));
		B.K.Add(DsSandstone(1.05f), FVector(520.f, 520.f, 40.f), FVector(0, 0, G + 1220.f));
		B.K.Add(DsSandstone(0.95f), FVector(360.f, 200.f, 80.f), FVector(-60.f, 0, G + 1280.f));
		B.K.Add(DsWater(), FVector(320.f, 160.f, 6.f), FVector(-60.f, 0, G + 1318.f));
		for (int32 st = 0; st < 24; ++st) { const FVector P(250.f, -180.f + st * 16.f, G + 25.f + st * 50.f); B.K.Add(DsSandstone(0.85f), FVector(100.f, 60.f, 20.f), P); B.Box(P, FVector(50.f, 30.f, 10.f)); }
		// The bucket wheel (its own part: it turns): a 4 m spoked wheel on an axle over the shaft, clay pots on its rim.
		{
			TArray<FWRLookBlock> Bl;
			FKit K(FTransform::Identity, Bl);
			K.Ring(DsTimber(0.8f), FVector::ZeroVector, 200.f, 14.f, 16.f, 20, 0.f, 360.f, 1.f);
			for (int32 sp = 0; sp < 8; ++sp) K.Rod(DsTimber(0.75f), FVector::ZeroVector, FRotator(sp * 45.f, 0.f, 0.f).RotateVector(FVector(0, 0, 196.f)), 8.f);
			for (int32 p = 0; p < 12; ++p) K.Add(DsClay(0.9f), FVector(40.f, 34.f, 44.f), FRotator(p * 30.f, 0.f, 0.f).RotateVector(FVector(0, 0, 215.f)), FRotator(p * 30.f, 0.f, 0.f), EWRLookMesh::Cylinder);
			// Its frame is the K ring rotated into the X-Z plane: the wheel turns about the local Y axle.
			for (FWRLookBlock& Blk : Bl) { Blk.Centre = FRotator(0.f, 0.f, 90.f).RotateVector(Blk.Centre); Blk.Rot = (FRotator(0.f, 0.f, 90.f).Quaternion() * Blk.Rot.Quaternion()).Rotator(); }
			C.A->Wheel = DsPart(B, FVector(120.f, 0, G + 1500.f), FRotator::ZeroRotator, Bl);
		}
		for (int32 sd = -1; sd <= 1; sd += 2) B.K.Add(DsTimber(0.7f), FVector(30.f, 30.f, 360.f), FVector(120.f, sd * 140.f, G + 1400.f));
		B.K.Rod(DsTimber(0.7f), FVector(120.f, -160.f, G + 1500.f), FVector(120.f, 160.f, G + 1500.f), 14.f);
		// The pools (desert.json arenas.mirage_matriarch.pools) and the channels from the tower to each.
		const TArray<FVector> Pools = C.D.ArenaPoints(TEXT("mirage_matriarch"), TEXT("pools"));
		for (int32 p = 0; p < Pools.Num(); ++p)
		{
			const FVector Lp = C.L(FVector(Pools[p].X, Pools[p].Y, 0.f));
			const float R = Pools[p].Z;                                     // the pool's radius rides in z (m -> cm)
			const float Gz = B.G(Lp.X, Lp.Y, G);
			B.K.Add(DsWater(), FVector(R * 2.f, R * 2.f, 6.f), FVector(Lp.X, Lp.Y, Gz - 40.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			B.K.Add(FSurf::Stone(DsRGB(0x3A, 0x5A, 0x50) * 0.4f, 0.3f), FVector(R * 2.f - 20.f, R * 2.f - 20.f, 6.f), FVector(Lp.X, Lp.Y, Gz - 140.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			const int32 NS = FMath::Max(10, int32(R * 2.f * PI / 90.f));
			for (int32 s = 0; s < NS; ++s) { const FRotator Rr(0.f, s * 360.f / NS, 0.f); B.K.Add(DsSandstone(0.95f), FVector(50.f, 2.f * PI * R / NS + 4.f, 30.f), FVector(Lp.X, Lp.Y, Gz - 10.f) + Rr.RotateVector(FVector(R + 20.f, 0, 0)), Rr); }
			if (p > 0)
			{
				const FVector From(0, 0, G), Dir = FVector(Lp.X, Lp.Y, 0.f).GetSafeNormal2D();
				const float L = FVector(Lp.X, Lp.Y, 0.f).Size() - R - 220.f;
				for (int32 sd = -1; sd <= 1; sd += 2) B.K.Add(DsSandstone(0.85f), FVector(L, 20.f, 30.f), Dir * (220.f + L * 0.5f) + FVector(-Dir.Y, Dir.X, 0.f) * sd * 30.f + FVector(0, 0, G), Dir.Rotation());
				B.K.Add(DsWater(), FVector(L, 40.f, 4.f), Dir * (220.f + L * 0.5f) + FVector(0, 0, G + 6.f), Dir.Rotation());
			}
		}
		// The garden wall (1.4 m mudbrick, 70 m across) with four gates, the crown of palms between pools and wall.
		const float WR = C.D.ArenaNum(TEXT("mirage_matriarch"), TEXT("radius"), 35.f) * 100.f;
		for (int32 s = 0; s < 32; ++s)
		{
			if (s % 8 == 0) continue;                                       // the four gates
			const float A0 = s * 360.f / 32.f, A1 = (s + 1) * 360.f / 32.f;
			const FVector P0 = FRotator(0.f, A0, 0.f).RotateVector(FVector(WR, 0, 0)), P1 = FRotator(0.f, A1, 0.f).RotateVector(FVector(WR, 0, 0));
			DsCoursedWall(B, FVector(P0.X, P0.Y, B.G(P0.X, P0.Y, G) - 20.f), FVector(P1.X, P1.Y, B.G(P1.X, P1.Y, G) - 20.f), 140.f, 60.f, DsMudbrick(), 0.f, 60.f, 22.f);
		}
		for (int32 p = 0; p < 40; ++p)
		{
			const float A = (p * 9.f + 4.f * B.Rand(p)) * PI / 180.f, D = WR - 300.f - 500.f * B.Rand(p * 3);
			const FVector P(FMath::Cos(A) * D, FMath::Sin(A) * D, 0.f);
			DsPalm(B, FVector(P.X, P.Y, B.G(P.X, P.Y, G)), p * 23.f, 0.85f + 0.35f * B.Rand(p + 7), p + 5, p % 3 != 0);
		}
		B.Loop(TEXT("desert.qanat_trickle"), FVector(-60.f, 100.f, G + 1300.f));
	}

	// ------------------------------------------------------------------------------------------------ the Leviathan Ribs

	void LeviathanRibs(FDsCtx& C)
	{
		// The Saltkin's champion at Saltgate's west mouth (cook 2.20, LATER-COOKS 45: each champion lies on the wrong side):
		// a 90 m sea-serpent bleached chalk-white, its spine in an S through the pass mouth, ribs arching over the road,
		// Sunwatch bronze spear heads still in its vertebrae.
		FBuild& B = C.B;
		const int32 NV = 60;
		TArray<FVector> Spine;
		for (int32 v = 0; v <= NV; ++v)
		{
			const float T = float(v) / NV;
			const FVector P(-4500.f + 9000.f * T, 1600.f * FMath::Sin(T * 2.f * PI), 0.f);
			Spine.Add(FVector(P.X, P.Y, B.G(P.X, P.Y, 0.f) + 60.f));
		}
		for (int32 v = 0; v < NV; ++v)
		{
			const FVector P = Spine[v], Dir = (Spine[v + 1] - Spine[v]).GetSafeNormal();
			const float T = float(v) / NV;
			const float R = 70.f * (1.f - 0.6f * FMath::Abs(T - 0.35f));
			B.K.Disc(DsBone(1.05f), P, Dir, R * 2.f, R * 0.8f);
			B.K.Spike(DsBone(1.f), P + FVector(0, 0, R * 0.6f), FVector(0, 0, 1), R * 0.5f, R * 1.8f);
			if (v % 2 == 0 && T > 0.08f && T < 0.8f)
			{
				const FVector Side = FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal();
				for (int32 sd = -1; sd <= 1; sd += 2)
				{
					TArray<FVector> Arc;
					for (int32 k = 0; k <= 6; ++k) { const float A = k / 6.f * 2.1f; Arc.Add(P + Side * sd * FMath::Sin(A) * 420.f + FVector(0, 0, FMath::Cos(A) * 380.f + 100.f) - Dir * k * 10.f); }
					Arc.Last().Z = B.G(Arc.Last().X, Arc.Last().Y, 0.f) - 20.f;
					B.M.Tube(Arc, 14.f, 6, DsBoneC() * 1.05f, 0.7f);
				}
			}
			if (v % 7 == 3) B.K.Spike(DsVerdigris(), P + FVector(0, 0, 30.f), (FVector(0, 0, 0.6f) + FRotator(0.f, v * 47.f, 0.f).Vector()).GetSafeNormal(), 10.f, 160.f);   // a spear head
			if (v % 3 == 0) B.Box(P, FVector(90.f, 90.f, 70.f), Dir.Rotation());
		}
		// The skull: 6 m, jaws parted, facing out of the pass toward the desert.
		const FVector H = Spine[0] - FVector(300.f, 0, 0);
		B.M.Lathe(H + FVector(200.f, 0, 40.f), FVector(-1, 0, 0), 600.f, { FVector(0.f, 160.f, 140.f), FVector(0.5f, 140.f, 110.f), FVector(1.f, 40.f, 30.f) }, 12, DsBoneC() * 1.05f, 0.7f);
		B.M.Lathe(H + FVector(150.f, 0, -40.f), FVector(-1, 0, -0.15f).GetSafeNormal(), 500.f, { FVector(0.f, 110.f, 50.f), FVector(1.f, 30.f, 20.f) }, 10, DsBoneC(), 0.7f);
		for (int32 t = 0; t < 10; ++t) B.K.Spike(DsBone(1.1f), H + FVector(-80.f - t * 45.f, (t % 2 ? 60.f : -60.f), 0.f), FVector(0, 0, -1), 14.f, 60.f);
		B.Box(H, FVector(320.f, 160.f, 140.f));
	}

	// ------------------------------------------------------------------------------------------------ shrines' settings

	void Shrine(FDsCtx& C)
	{
		// The settings round each shrine's brazier (the brazier is the shrine's own, WRWorldSites): the Strand's ring of
		// shell-paved stones, the Wall's storm-scoured stelae (sand drifts on their east faces), the Saltgate Mouth's
		// stone fence against the pass wind, the Erg's skull (the Whale Road's biggest: the brazier stands on its crown),
		// the Canyon's spire with its stair.
		FBuild& B = C.B;
		const float G = B.G(0, 0, 0.f);
		const FName K = C.A->Kind;
		if (K == TEXT("shrine_great")) return;                              // the Sun Gate's court is the setting
		if (K == TEXT("shrine_skull"))
		{
			// The biggest whale's braincase as a platform: a broad dome of bone with the brazier's flat on top, steps cut.
			B.M.Lathe(FVector(0, 0, G - 100.f), FVector::UpVector, 480.f, { FVector(0.f, 520.f, 460.f), FVector(0.6f, 420.f, 380.f), FVector(1.f, 260.f, 240.f) }, 16, DsBoneC(), 0.75f);
			B.Box(FVector(0, 0, G + 140.f), FVector(380.f, 340.f, 240.f));
			for (int32 s = 0; s < 6; ++s) { const FVector P(520.f - s * 40.f, 0.f, G + 40.f + s * 60.f); B.K.Add(DsBone(0.85f), FVector(80.f, 140.f, 30.f), P); B.Box(P, FVector(40.f, 70.f, 15.f)); }
			return;
		}
		if (K == TEXT("shrine_spire"))
		{
			// A 22 m red spire on the canyon floor; a rock-cut stair winds up it (Sunwatch steps); the brazier on its flat top.
			const float H = 2200.f;
			DsHoodoo(B, FVector(0, 0, G), H, 700.f, 77, false);
			B.Box(FVector(0, 0, G + H * 0.5f), FVector(520.f, 520.f, H * 0.5f));
			for (int32 s = 0; s < 70; ++s)
			{
				const float A = s * 12.f * PI / 180.f, Z = G + 40.f + s * (H - 100.f) / 70.f;
				const float R = 700.f - 220.f * (Z - G) / H + 60.f;
				const FVector P(FMath::Cos(A) * R, FMath::Sin(A) * R, Z);
				B.K.Add(DsRedRock(0.8f), FVector(110.f, 70.f, 24.f), P, FRotator(0.f, s * 12.f + 90.f, 0.f));
				B.Box(P, FVector(55.f, 35.f, 12.f), FRotator(0.f, s * 12.f + 90.f, 0.f));
			}
			B.K.Add(DsCaprock(), FVector(900.f, 900.f, 60.f), FVector(0, 0, G + H), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			B.Box(FVector(0, 0, G + H), FVector(450.f, 450.f, 30.f));
			return;
		}
		if (C.A->SiteId == TEXT("shrine_strand"))
		{
			for (int32 s = 0; s < 14; ++s) { const FRotator R(0.f, s * 360.f / 14.f, 0.f); B.K.Add(DsBone(0.9f), FVector(50.f, 40.f, 30.f), R.RotateVector(FVector(330.f, 0, G + 10.f)), R); }
			for (int32 p = 0; p < 4; ++p) DsCross(B, FRotator(0.f, p * 90.f + 45.f, 0.f).RotateVector(FVector(560.f, 0, 0)) + FVector(0, 0, G), p * 90.f, p);
			return;
		}
		if (C.A->SiteId == TEXT("shrine_wall"))
		{
			for (int32 s = 0; s < 6; ++s) { const FVector P = FRotator(0.f, s * 60.f, 0.f).RotateVector(FVector(520.f, 0, 0)); DsStela(B, FVector(P.X, P.Y, B.G(P.X, P.Y, G)), s * 60.f + 90.f, 3.f * B.Rand(s), 1, 320.f); B.K.Add(DsSandFill(0.9f), FVector(180.f, 160.f, 80.f), FVector(P.X + 120.f, P.Y, B.G(P.X, P.Y, G)), FRotator::ZeroRotator, EWRLookMesh::Cylinder); }
			return;
		}
		// The Saltgate Mouth: a dry-stone fence on the pass side, a bronze spear rack (the Sunwatch held this mouth).
		for (int32 s = 0; s < 7; ++s)
		{
			const FVector P0 = FRotator(0.f, -60.f + s * 20.f, 0.f).RotateVector(FVector(600.f, 0, 0)), P1 = FRotator(0.f, -40.f + s * 20.f, 0.f).RotateVector(FVector(600.f, 0, 0));
			DsCoursedWall(B, FVector(P0.X, P0.Y, B.G(P0.X, P0.Y, G) - 10.f), FVector(P1.X, P1.Y, B.G(P1.X, P1.Y, G) - 10.f), 110.f, 60.f, DsSandstone(0.85f), 0.3f, 50.f, 25.f);
		}
		for (int32 sp = 0; sp < 5; ++sp) B.K.Rod(DsVerdigris(), FVector(-300.f + sp * 30.f, 200.f, G), FVector(-300.f + sp * 30.f, 230.f, G + 240.f), 4.f);
	}

	// ------------------------------------------------------------------------------------------------ camps

	void Camp(FDsCtx& C)
	{
		// A newcomers' camp (world-map: everyone came the same way, by ship): a curved windbreak of palm-frond panels on
		// driftwood posts on the windward (east) side, a fire ring of beach stones, water casks, a canvas awning; their
		// ship lies on the beach below (desert.json's ship point).
		FBuild& B = C.B;
		const float G = B.G(0, 0, 0.f);
		for (int32 p = 0; p < 9; ++p)
		{
			const float A = FMath::DegreesToRadians(-60.f + p * 15.f + 180.f);
			const FVector Pp(FMath::Cos(A) * 650.f, FMath::Sin(A) * 650.f, 0.f);
			const float Gz = B.G(Pp.X, Pp.Y, G);
			B.Limb(FVector(Pp.X, Pp.Y, Gz - 30.f), FVector(Pp.X, Pp.Y, Gz + 220.f), 9.f, 6.f, DsDriftC(), 6);
			if (p < 8)
			{
				const float A2 = FMath::DegreesToRadians(-60.f + (p + 0.5f) * 15.f + 180.f);
				const FVector Pm(FMath::Cos(A2) * 650.f, FMath::Sin(A2) * 650.f, Gz + 120.f);
				for (int32 f = 0; f < 6; ++f) B.K.Add(FSurf::Cloth(DsFrondDryC() * (0.9f + 0.2f * B.Rand(p * 6 + f))), FVector(6.f, 30.f, 200.f), Pm + FRotator(0.f, FMath::RadiansToDegrees(A2) + 90.f, 0.f).Vector() * (f - 2.5f) * 28.f, FRotator(0.f, FMath::RadiansToDegrees(A2), 4.f * (f - 2.5f)));
				B.Box(Pm, FVector(8.f, 90.f, 110.f), FRotator(0.f, FMath::RadiansToDegrees(A2), 0.f));
			}
		}
		for (int32 s = 0; s < 10; ++s) B.K.Add(DsSandstone(0.7f), FVector(36.f, 30.f, 24.f), FRotator(0.f, s * 36.f, 0.f).RotateVector(FVector(110.f, 0, G + 8.f)), FRotator(0.f, s * 36.f + 10.f, 0.f));
		B.K.Add(DsEmber(), FVector(120.f, 120.f, 8.f), FVector(0, 0, G + 4.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		B.Loop(TEXT("brazier.fire"), FVector(0, 0, G + 10.f));
		B.Lamp(FVector(0, 0, G + 120.f), DsRGB(0xFF, 0xA8, 0x60), 2000.f, 1500.f);
		for (int32 c = 0; c < 3; ++c) DsCask(B, FVector(300.f + c * 70.f, -250.f, B.G(300.f + c * 70.f, -250.f, G)), 0.f, true);
		// A canvas awning on four poles (the shade the camp lives in at noon).
		for (int32 cx = -1; cx <= 1; cx += 2) for (int32 cy = -1; cy <= 1; cy += 2) B.K.Rod(DsSunBleach(0.7f), FVector(-250.f + cx * 200.f, 300.f + cy * 150.f, G - 20.f), FVector(-250.f + cx * 200.f, 300.f + cy * 150.f, G + 250.f + (cy > 0 ? 0.f : 30.f)), 6.f);
		B.K.Add(DsLinen(0.85f), FVector(430.f, 330.f, 6.f), FVector(-250.f, 300.f, G + 265.f), FRotator(0.f, 0.f, 5.f));
		// The newcomer lantern (KEPT night rule: camps show their lanterns).
		B.K.Rod(DsSunBleach(0.7f), FVector(200.f, 350.f, G - 20.f), FVector(200.f, 350.f, G + 260.f), 6.f);
		B.N.Add(FSurf::Lit(DsRGB(0xFF, 0xC8, 0x80), 3.f), FVector(20.f, 20.f, 30.f), FVector(200.f, 380.f, G + 230.f));
		B.Lamp(FVector(200.f, 380.f, G + 220.f), DsRGB(0xFF, 0xC8, 0x80), 900.f, 1200.f);
		// The ship below on the beach.
		if (const FWRDesertData::FPlace* P = C.D.FindSite(C.A->SiteId))
		{
			if (!P->Ship.IsZero())
			{
				const FVector S = C.L(P->Ship);
				DsHull(B, FVector(S.X, S.Y, B.G(S.X, S.Y, S.Z)), FRotator(2.f, 100.f + 20.f * B.Rand(3), 14.f), 1600.f, 450.f, 230.f, 0, 60.f, 0.35f, 41 + C.A->Index);
			}
		}
	}

	void Runback(FDsCtx& C) {}

	void AmastanShade(FDsCtx& C)
	{
		// Amastan's day place (cook 2.11): an indigo awning stretched from the south pylon's foot on two poles, a woven mat,
		// his saddle and water skins, a brass tea pot on embers; the couched Dromad is his (a creature, later).
		FBuild& B = C.B;
		const float G = B.G(0, 0, 0.f);
		for (int32 p = 0; p < 2; ++p) B.K.Rod(DsSunBleach(0.75f), FVector(200.f, -200.f + p * 400.f, G - 20.f), FVector(200.f, -200.f + p * 400.f, G + 230.f), 6.f);
		B.K.Add(DsIndigo(), FVector(420.f, 460.f, 6.f), FVector(0, 0, G + 245.f), FRotator(0.f, 0.f, -8.f));
		B.K.Add(FSurf::Cloth(DsRGB(0xB0, 0x4A, 0x2E) * 0.5f), FVector(260.f, 180.f, 4.f), FVector(-40.f, 0, G + 2.f));
		B.K.Add(DsHide(1.f), FVector(80.f, 60.f, 50.f), FVector(-140.f, 120.f, G + 25.f));
		for (int32 w = 0; w < 2; ++w) B.Ball(FVector(-160.f, -100.f + w * 40.f, G + 25.f), 22.f, DsRGB(0x8A, 0x62, 0x3E) * 0.48f, 0.8f, 1.2f);
		B.K.Add(DsEmber(), FVector(40.f, 40.f, 6.f), FVector(60.f, 60.f, G + 3.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		B.K.Add(DsBronze(), FVector(22.f, 22.f, 26.f), FVector(60.f, 60.f, G + 18.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		B.Loop(TEXT("torch.fire"), FVector(60.f, 60.f, G + 4.f), FRotator::ZeroRotator, 0.4f);
	}

	// ------------------------------------------------------------------------------------------------ the caves

	void Cave(FDsCtx& C)
	{
		// The four caves (cook 2.6): rock rooms at their floor level under the ground, a passage down from the mouth to the
		// first room. The Nitre Caves' walls are white-crusted red rock with a bat roost; the Qanat is a hand-dug tunnel
		// (shaft 6's mouth and ladder) half flooded; Ssarrok's Hollow is a vast sand-floored hall under the Mother Dune's
		// foot (its arena); the Sieve Throats go down 40 m and open into the Fungal caverns' halls.
		FBuild& B = C.B;
		const FWRDesertData::FCave* Cv = C.D.Caves.IsValidIndex(C.A->Index) ? &C.D.Caves[C.A->Index] : nullptr;
		if (!Cv) return;
		C.A->SetActorRotation(FRotator::ZeroRotator);
		const float FloorZ = Cv->Floor - Cv->At.Z;
		const bool bNitre = Cv->Id == TEXT("nitre_caves"), bQanat = Cv->Id == TEXT("qanat"), bHollow = Cv->Id == TEXT("ssarroks_hollow");
		const FLinearColor Rock = bNitre ? DsRedRockC() * 0.8f : bHollow ? DsRGB(0x9A, 0x7A, 0x50) * 0.5f : DsRGB(0x7A, 0x5E, 0x48) * 0.5f;
		for (int32 i = 0; i < Cv->Rooms.Num(); ++i)
		{
			const FVector Rm = Cv->Rooms[i];
			const FVector Centre(Rm.X, Rm.Y, FloorZ);
			const float Rad = Rm.Z, H = Cv->Roof;
			if (bQanat)
			{
				// The qanat: a hand-dug tunnel 2.5 m wide with a vaulted roof, its walls tool-marked.
				DsGallery(B, i == 0 ? FVector(0, 0, FloorZ) : FVector(Cv->Rooms[i - 1].X, Cv->Rooms[i - 1].Y, FloorZ), Centre, 250.f, 300.f, true, i * 7, i % 2 == 0);
				continue;
			}
			DsDomeInward(B, Centre, Rad, H, 22, Rock, 0.2f);
			DsRoundRoomCollision(B, Centre, Rad, H);
			B.M.Lathe(Centre - FVector(0, 0, 40.f), FVector::UpVector, 40.f, { FVector(0.f, Rad * 1.02f, Rad * 1.02f), FVector(1.f, Rad, Rad) }, 22, Cv->bSandFloor ? DsRGB(0xD9, 0xA8, 0x5B) * 0.55f : Rock * 1.1f, 0.95f);
			for (int32 f = 0; f < 4; ++f) B.Rock(f, FVector(160.f, 140.f, 100.f) * (0.6f + B.Rand(f + i * 9)), Centre + FVector((B.Rand(f * 3 + i) - 0.5f) * Rad, (B.Rand(f * 5 + i) - 0.5f) * Rad, 0.f), FRotator(0.f, f * 70.f, 0.f), DsRockTint() * 0.6f, true);
			if (bNitre) for (int32 n = 0; n < 8; ++n) { const float A = n * 45.f * PI / 180.f; B.K.Add(DsSalt(), FVector(140.f, 20.f, 90.f), Centre + FVector(FMath::Cos(A) * (Rad - 40.f), FMath::Sin(A) * (Rad - 40.f), 120.f + 120.f * B.Rand(n)), FRotator(0.f, n * 45.f + 90.f, 0.f)); }
			if (bHollow)
			{
				// Sand pouring from cracks in the roof (the dune above), Dunejaw teeth and old harpoon heads in the sand.
				B.Loop(TEXT("desert.sand_stream"), Centre + FVector(Rad * 0.3f, 0, H - 50.f));
				for (int32 t = 0; t < 6; ++t) B.K.Spike(DsBone(1.f), Centre + FVector((B.Rand(t) - 0.5f) * Rad, (B.Rand(t + 9) - 0.5f) * Rad, 0.f), FVector(0.2f, 0.f, 1.f).GetSafeNormal(), 10.f, 30.f);
			}
			if (i + 1 < Cv->Rooms.Num())
			{
				const FVector N(Cv->Rooms[i + 1].X, Cv->Rooms[i + 1].Y, FloorZ);
				const FVector D = (N - Centre).GetSafeNormal2D();
				DsGallery(B, Centre + D * (Rad * 0.85f), N - D * (Cv->Rooms[i + 1].Z * 0.85f), 400.f, 420.f, true, i * 13, false);
			}
			// Light: none in the caves but the sunstone in the Hollow's walls (bring a lamp).
			if (bHollow) B.Lamp(Centre + FVector(0, 0, H * 0.5f), DsRGB(0xF2, 0xA2, 0x3A), 500.f, Rad * 1.4f);
		}
		// The way down: from the mouth (a hole in the ground the collar covers) to the first room.
		const FVector Mouth(Cv->MouthOffset.X, Cv->MouthOffset.Y, 0.f);
		const FVector First(Cv->Rooms[0].X, Cv->Rooms[0].Y, FloorZ);
		const FVector Toward = (First - Mouth).GetSafeNormal2D().IsNearlyZero() ? FVector(-1, 0, 0) : (First - Mouth).GetSafeNormal2D();
		const float Drop = -FloorZ;
		if (Cv->bShaft)
		{
			// Shaft 6's mouth: a stone lip on its spoil ring, a ladder 14 m down to the tunnel's floor.
			for (int32 s = 0; s < 12; ++s) { const FRotator R(0.f, s * 30.f, 0.f); B.K.Add(DsSandstone(0.85f), FVector(60.f, 80.f, 40.f), R.RotateVector(FVector(170.f, 0, 10.f)), R); B.Box(R.RotateVector(FVector(170.f, 0, 10.f)), FVector(30.f, 40.f, 20.f), R); }
			B.K.Add(DsPaleSand(0.9f), FVector(900.f, 900.f, 80.f), FVector(0, 0, -30.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			for (int32 sd = -1; sd <= 1; sd += 2) B.K.Rod(DsTimber(0.8f), FVector(-80.f, sd * 30.f, 60.f), FVector(-80.f, sd * 30.f, FloorZ), 6.f);
			for (int32 r = 0; r < int32(Drop / 35.f); ++r) B.K.Rod(DsTimber(0.75f), FVector(-80.f, -30.f, FloorZ + r * 35.f + 20.f), FVector(-80.f, 30.f, FloorZ + r * 35.f + 20.f), 4.f);
			for (int32 w = 0; w < 8; ++w) { const float A = w * 45.f; B.K.Add(DsRedRock(0.6f), FVector(40.f, 140.f, Drop + 60.f), FRotator(0.f, A, 0.f).RotateVector(FVector(150.f, 0, FloorZ * 0.5f)), FRotator(0.f, A, 0.f)); B.Box(FRotator(0.f, A, 0.f).RotateVector(FVector(150.f, 0, FloorZ * 0.5f)), FVector(20.f, 70.f, Drop * 0.5f + 30.f), FRotator(0.f, A, 0.f)); }
			// The flooded run: water you swim (a water volume the scenery shows; the hazards make it swimmable).
			for (const FVector4f& F : Cv->Flooded) B.K.Add(DsWater(), FVector(FMath::Abs(F.Z - F.X) + 200.f, 260.f, 4.f), FVector((F.X + F.Z) * 0.5f, (F.Y + F.W) * 0.5f, FloorZ + 160.f));
			return;
		}
		const float Run = FMath::Max(Drop / FMath::Tan(FMath::DegreesToRadians(22.f)), 800.f);
		const FVector Bottom = Mouth + Toward * Run + FVector(0, 0, FloorZ);
		DsGallery(B, Mouth + FVector(0, 0, -40.f), Bottom, Cv->MouthW * 0.7f, Cv->MouthH, true, C.A->Index * 3, false);
		DsGallery(B, Bottom, First - Toward * Cv->Rooms[0].Z * 0.85f, Cv->MouthW * 0.7f, Cv->MouthH, true, C.A->Index * 5, false);
		// The mouth's collar: weathered rock round the hole (covers the ground's edge).
		B.K.Ring(DsRedRock(0.75f), Mouth + FVector(0, 0, -30.f), Cv->MouthW * 0.5f + 60.f, 120.f, Cv->MouthH, 9, 0.f, 360.f, 1.f);
		for (int32 r = 0; r < 5; ++r) B.Rock(r + 3, FVector(220.f, 200.f, 160.f), Mouth + FRotator(0.f, r * 72.f, 0.f).RotateVector(FVector(Cv->MouthW * 0.5f + 160.f, 0, -40.f)), FRotator(0.f, r * 72.f, 0.f), DsRockTint() * 0.8f, true);
		if (Cv->Id == TEXT("sieve_throats")) B.Loop(TEXT("fungal.sandFall"), Mouth + FVector(0, 0, -200.f), FRotator::ZeroRotator, 0.4f);
	}

	// ------------------------------------------------------------------------------------------------ the Hourhold

	/** The doorways a room's walls need: one per link or way, on the wall facing the other end (axis-aligned rooms). */
	struct FDsDoor { int32 Side; float Along; };                            // Side 0 +X, 1 -X, 2 +Y, 3 -Y
	static TArray<FDsDoor> DsRoomDoors(const FWRDesertData& D, const FWRDesertData::FRoom& R)
	{
		TArray<FDsDoor> Out;
		const auto Toward = [&](const FVector& Other)
		{
			const FVector V = Other - R.At;
			const float Ax = FMath::Abs(V.X) / FMath::Max(1.f, R.W), Ay = FMath::Abs(V.Y) / FMath::Max(1.f, R.D);
			FDsDoor Dd;
			if (Ax >= Ay) { Dd.Side = V.X > 0.f ? 0 : 1; Dd.Along = FMath::Clamp(V.Y * 0.25f, -R.D * 0.3f, R.D * 0.3f); }
			else { Dd.Side = V.Y > 0.f ? 2 : 3; Dd.Along = FMath::Clamp(V.X * 0.25f, -R.W * 0.3f, R.W * 0.3f); }
			Out.Add(Dd);
		};
		for (const TPair<FName, FName>& L : D.Links)
		{
			if (L.Key == R.Id) { if (const FWRDesertData::FRoom* O = D.FindRoom(L.Value)) Toward(O->At); }
			else if (L.Value == R.Id) { if (const FWRDesertData::FRoom* O = D.FindRoom(L.Key)) Toward(O->At); }
		}
		for (const FWRDesertData::FWay& W : D.Ways) if (W.To == R.Id) Toward(W.From);
		return Out;
	}
	/** Where a door of a room stands (world cm, floor level). */
	static FVector DsDoorAt(const FWRDesertData::FRoom& R, const FDsDoor& Dd)
	{
		switch (Dd.Side)
		{
		case 0: return R.At + FVector(R.W * 0.5f, Dd.Along, 0.f);
		case 1: return R.At + FVector(-R.W * 0.5f, Dd.Along, 0.f);
		case 2: return R.At + FVector(Dd.Along, R.D * 0.5f, 0.f);
		default: return R.At + FVector(Dd.Along, -R.D * 0.5f, 0.f);
		}
	}
	/** The door of room R that faces Other. */
	static FVector DsDoorToward(const FWRDesertData& D, const FWRDesertData::FRoom& R, const FVector& Other)
	{
		const FVector V = Other - R.At;
		const float Ax = FMath::Abs(V.X) / FMath::Max(1.f, R.W), Ay = FMath::Abs(V.Y) / FMath::Max(1.f, R.D);
		FDsDoor Dd;
		if (Ax >= Ay) { Dd.Side = V.X > 0.f ? 0 : 1; Dd.Along = FMath::Clamp(V.Y * 0.25f, -R.D * 0.3f, R.D * 0.3f); }
		else { Dd.Side = V.Y > 0.f ? 2 : 3; Dd.Along = FMath::Clamp(V.X * 0.25f, -R.W * 0.3f, R.W * 0.3f); }
		return DsDoorAt(R, Dd);
	}

	void HourholdRoom(FDsCtx& C)
	{
		// One room of the Sunwatch's buried temple city (cook 2.5): dressed sandstone walls with doorways where its
		// galleries meet it, a slab floor, a roof of beams and slabs, sunstone sconces (always lit: it's dark down here),
		// then what the room holds.
		FBuild& B = C.B;
		C.A->SetActorRotation(FRotator::ZeroRotator);
		if (!C.D.Rooms.IsValidIndex(C.A->Index)) return;
		const FWRDesertData::FRoom& R = C.D.Rooms[C.A->Index];
		const float W = R.W, Dp = R.D, H = R.H;
		const TArray<FDsDoor> Doors = DsRoomDoors(C.D, R);
		const float DoorW = 420.f, DoorH = 480.f;
		// The four walls, each split round its doorways.
		for (int32 Side = 0; Side < 4; ++Side)
		{
			const bool bX = Side < 2;
			const float Len = bX ? Dp : W;
			const float Off = bX ? (Side == 0 ? W * 0.5f : -W * 0.5f) : (Side == 2 ? Dp * 0.5f : -Dp * 0.5f);
			TArray<float> Gaps;
			for (const FDsDoor& Dd : Doors) if (Dd.Side == Side) Gaps.Add(Dd.Along);
			Gaps.Sort();
			float From = -Len * 0.5f;
			const auto Run = [&](float A0, float A1)
			{
				if (A1 - A0 < 20.f) return;
				const FVector P0 = bX ? FVector(Off, A0, 0.f) : FVector(A0, Off, 0.f), P1 = bX ? FVector(Off, A1, 0.f) : FVector(A1, Off, 0.f);
				DsCoursedWall(B, P0, P1, H, 80.f, DsSandstone(0.78f), 0.f, 120.f, 50.f);
			};
			for (float Gp : Gaps)
			{
				Run(From, Gp - DoorW * 0.5f);
				// The doorway's lintel and jambs, a carved sun disc over it.
				const FVector Dc = bX ? FVector(Off, Gp, 0.f) : FVector(Gp, Off, 0.f);
				B.K.Add(DsSandstone(1.f), bX ? FVector(100.f, DoorW + 100.f, 80.f) : FVector(DoorW + 100.f, 100.f, 80.f), Dc + FVector(0, 0, DoorH + 40.f));
				DsCoursedWall(B, Dc + FVector(0, 0, DoorH + 80.f) + (bX ? FVector(0, -DoorW * 0.5f, 0) : FVector(-DoorW * 0.5f, 0, 0)), Dc + FVector(0, 0, DoorH + 80.f) + (bX ? FVector(0, DoorW * 0.5f, 0) : FVector(DoorW * 0.5f, 0, 0)), FMath::Max(0.f, H - DoorH - 80.f), 80.f, DsSandstone(0.78f), 0.f, 120.f, 50.f);
				B.K.Disc(DsBronzeDull(), Dc + FVector(0, 0, DoorH + 40.f) + (bX ? FVector(FMath::Sign(-Off) * 52.f, 0, 0) : FVector(0, FMath::Sign(-Off) * 52.f, 0)), bX ? FVector(1, 0, 0) : FVector(0, 1, 0), 60.f, 4.f);
				From = Gp + DoorW * 0.5f;
			}
			Run(From, Len * 0.5f);
		}
		// Floor slabs and the roof (beams across the short span, slabs over them).
		B.K.Add(DsSandstone(0.6f), FVector(W, Dp, 30.f), FVector(0, 0, -15.f));
		B.Box(FVector(0, 0, -20.f), FVector(W * 0.5f, Dp * 0.5f, 20.f));
		const bool bBeamsX = W < Dp;
		for (float s = -0.5f; s <= 0.5f; s += 400.f / (bBeamsX ? Dp : W)) B.K.Add(DsCedar(0.6f), bBeamsX ? FVector(W, 40.f, 50.f) : FVector(40.f, Dp, 50.f), bBeamsX ? FVector(0, s * Dp, H - 25.f) : FVector(s * W, 0, H - 25.f));
		const int32 Holes = R.Shafts;
		B.K.Add(DsSandstone(0.55f), FVector(W + 160.f, Dp + 160.f, 60.f), FVector(0, 0, H + 30.f));
		B.Box(FVector(0, 0, H + 40.f), FVector(W * 0.5f + 80.f, Dp * 0.5f + 80.f, 40.f));
		// Sconces along the long walls (sunstone: amber, always on).
		for (int32 s = 0; s < 4; ++s)
		{
			const bool bLongX = Dp > W;
			const FVector Sc = bLongX ? FVector((s % 2 ? 1.f : -1.f) * (W * 0.5f - 50.f), (s < 2 ? -0.25f : 0.25f) * Dp, H * 0.55f) : FVector((s < 2 ? -0.25f : 0.25f) * W, (s % 2 ? 1.f : -1.f) * (Dp * 0.5f - 50.f), H * 0.55f);
			B.K.Add(DsBronze(), FVector(26.f, 26.f, 30.f), Sc);
			B.K.Add(DsSunstone(3.f), FVector(16.f, 16.f, 20.f), Sc + FVector(0, 0, 22.f));
			B.Lamp(Sc + FVector(0, 0, 40.f), DsRGB(0xF2, 0xA2, 0x3A), 900.f, FMath::Max(W, Dp) * 0.6f);
		}
		const FName Id = R.Id;
		if (R.Pillars > 0 && Id == TEXT("hall_of_hours"))
		{
			// The Hall of Hours: 24 pillars in two rows, each carved with its hour (a disc and bands); roof holes over the
			// aisle throw shafts of real sun across the floor (they move with the sun: AWRDesertScenery::Shafts).
			for (int32 p = 0; p < 24; ++p)
			{
				const float X = (p % 2 ? 1.f : -1.f) * W * 0.28f, Y = -Dp * 0.46f + (p / 2) * Dp * 0.92f / 11.f;
				B.K.Add(DsSandstone(0.95f), FVector(140.f, 140.f, H - 60.f), FVector(X, Y, (H - 60.f) * 0.5f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
				B.K.Add(DsSandstone(1.05f), FVector(200.f, 200.f, 50.f), FVector(X, Y, H - 80.f));
				B.K.Disc(DsBronzeDull(), FVector(X, Y, 260.f) + FVector(p % 2 ? -72.f : 72.f, 0, 0), FVector(p % 2 ? -1.f : 1.f, 0, 0), 50.f, 4.f);
				for (int32 b = 0; b < (p % 12) + 1; ++b) B.K.Add(DsSandstone(0.6f), FVector(4.f, 40.f, 6.f), FVector(X + (p % 2 ? -71.f : 71.f), Y, 320.f + b * 14.f));
				B.Box(FVector(X, Y, (H - 60.f) * 0.5f), FVector(70.f, 70.f, (H - 60.f) * 0.5f));
			}
			for (int32 h = 0; h < Holes; ++h)
			{
				const FVector Hole(0, -Dp * 0.4f + h * Dp * 0.8f / FMath::Max(1, Holes - 1), H);
				B.K.Ring(DsSandstone(0.7f), Hole + FVector(0, 0, 30.f), 120.f, 60.f, 80.f, 12);
				if (!B.bServer)
				{
					USceneComponent* Sh = NewObject<USceneComponent>(C.A);
					Sh->SetupAttachment(C.A->GetRootComponent());
					Sh->SetRelativeLocation(Hole);
					Sh->RegisterComponent();
					if (USceneComponent* Fx = WRFx::Attach(Sh, TEXT("desert.light_shaft"), NAME_None, 1.f)) C.A->Loops.Add(Fx);
					C.A->Shafts.Add(Sh);
				}
			}
		}
		else if (Id == TEXT("street"))
		{
			// A street choked by the dune: house fronts down both sides, sand pouring in through the broken roof in a ramp
			// you slide down from the Lee Door.
			for (int32 h = 0; h < 6; ++h) for (int32 sd = -1; sd <= 1; sd += 2) B.K.Add(DsMudbrick(0.9f), FVector(80.f, Dp / 7.f, 500.f + 200.f * B.Rand(h + sd)), FVector(sd * (W * 0.5f - 60.f), -Dp * 0.4f + h * Dp / 7.f, 300.f));
			B.Loop(TEXT("desert.sand_stream"), FVector(0, -Dp * 0.35f, H - 40.f));
			B.K.Add(DsSandFill(0.85f), FVector(W - 100.f, Dp * 0.5f, 600.f), FVector(0, -Dp * 0.25f, 100.f), FRotator(-14.f, 90.f, 0.f));
		}
		else if (Id == TEXT("scribes"))
		{
			for (int32 d = 0; d < 3; ++d) { B.K.Add(DsSandstone(0.9f), FVector(200.f, 80.f, 90.f), FVector(-200.f + d * 200.f, -200.f, 45.f)); B.K.Add(DsClay(0.8f), FVector(30.f, 20.f, 4.f), FVector(-200.f + d * 200.f, -200.f, 92.f)); }
			for (int32 sh = 0; sh < 3; ++sh) B.K.Add(DsCedar(0.6f), FVector(30.f, Dp - 200.f, 10.f), FVector(W * 0.5f - 60.f, 0, 120.f + sh * 90.f));
		}
		else if (Id == TEXT("granary"))
		{
			for (int32 j = 0; j < 24; ++j) B.K.Add(DsClay(0.85f + 0.2f * B.Rand(j)), FVector(70.f, 70.f, 110.f), FVector(-W * 0.4f + (j % 6) * W * 0.16f, -Dp * 0.35f + (j / 6) * Dp * 0.23f, 55.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		}
		else if (Id == TEXT("cistern"))
		{
			// The Cistern Below: a pillared reservoir of dark still water (the Sandfather's well in its deepest corner), a
			// walkway round its edge.
			for (int32 p = 0; p < 16; ++p) { const FVector P(-W * 0.36f + (p % 4) * W * 0.24f, -Dp * 0.36f + (p / 4) * Dp * 0.24f, 0.f); B.K.Add(DsSandstone(0.85f), FVector(120.f, 120.f, H), P + FVector(0, 0, H * 0.5f - 300.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder); B.Box(P + FVector(0, 0, H * 0.5f - 300.f), FVector(60.f, 60.f, H * 0.5f)); }
			const float Wz = R.Water > -1.0e8f ? R.Water - R.At.Z : 350.f;
			B.K.Add(FSurf::Glaze(DsRGB(0x10, 0x1E, 0x22) * 0.5f, 0.02f), FVector(W - 600.f, Dp - 600.f, 4.f), FVector(0, 0, Wz));
			for (int32 sd = -1; sd <= 1; sd += 2) { B.K.Add(DsSandstone(0.8f), FVector(W, 280.f, 40.f), FVector(0, sd * (Dp * 0.5f - 140.f), Wz + 60.f)); B.Box(FVector(0, sd * (Dp * 0.5f - 140.f), Wz + 60.f), FVector(W * 0.5f, 140.f, 20.f)); }
			B.K.Ring(DsSandstone(0.9f), FVector(R.Well.X, R.Well.Y, Wz + 40.f), 200.f, 40.f, 80.f, 16);
		}
		else if (Id == TEXT("barque_dock"))
		{
			// A dry dock with a 30 m sun-barque on log rollers, its oars laid out beside it (cook 2.5 room 8).
			DsHull(B, FVector(0, 0, 120.f), FRotator(0.f, 90.f, 0.f), 3000.f, 520.f, 260.f, 1, 0.f, 0.f, 88);
			for (int32 r = 0; r < 8; ++r) B.K.Add(DsTimber(0.7f), FVector(36.f, 700.f, 36.f), FVector(0, -1200.f + r * 340.f, 18.f), FRotator(0.f, 90.f, 90.f), EWRLookMesh::Cylinder);
			for (int32 o = 0; o < 10; ++o) B.K.Rod(DsCedar(0.8f), FVector(W * 0.4f, -1200.f + o * 260.f, 10.f), FVector(W * 0.4f - 400.f, -1150.f + o * 260.f, 10.f), 8.f);
		}
		else if (Id == TEXT("ushabti_hall"))
		{
			// Two hundred servant figures in rows facing the door, blue faience over clay, a hoe on each shoulder (twelve of
			// them wake: WRDesertLife's Ushabti).
			for (int32 u = 0; u < 200; ++u)
			{
				const FVector P(-W * 0.44f + (u % 20) * W * 0.88f / 19.f, -Dp * 0.4f + (u / 20) * Dp * 0.8f / 9.f, 0.f);
				B.K.Add(DsFaience(), FVector(40.f, 30.f, 90.f), P + FVector(0, 0, 45.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
				B.K.Add(DsClay(1.f), FVector(22.f, 22.f, 26.f), P + FVector(0, 0, 103.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
				B.K.Rod(DsCedar(0.6f), P + FVector(-12.f, 16.f, 60.f), P + FVector(-12.f, 18.f, 120.f), 3.f);
			}
		}
		else if (Id == TEXT("weighing_room"))
		{
			// A great bronze balance: a post, its beam, two pans on chains; a feather on one pan (the portal puzzle's origin).
			B.K.Add(DsBronzeDull(), FVector(40.f, 40.f, 380.f), FVector(0, 0, 190.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			B.K.Rod(DsBronze(), FVector(-260.f, 0, 380.f), FVector(260.f, 0, 380.f), 14.f);
			for (int32 sd = -1; sd <= 1; sd += 2) { for (int32 ch = -1; ch <= 1; ch += 2) B.Line(FVector(sd * 250.f, 0, 380.f), FVector(sd * 250.f + ch * 60.f, 0, 130.f), 1.5f, 0.f, DsRGB(0x8C, 0x6A, 0x3A) * 0.6f, 4); B.K.Add(DsBronze(), FVector(160.f, 160.f, 14.f), FVector(sd * 250.f, 0, 125.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder); }
			B.K.Add(DsGold(), FVector(60.f, 10.f, 4.f), FVector(-250.f, 0, 136.f), FRotator(0.f, 30.f, 6.f));
			B.Box(FVector(0, 0, 190.f), FVector(30.f, 30.f, 190.f));
		}
		else if (Id == TEXT("glasstail_nest"))
		{
			for (int32 e = 0; e < 18; ++e) B.Ball(FVector((B.Rand(e) - 0.5f) * W * 0.6f, (B.Rand(e + 30) - 0.5f) * Dp * 0.6f, 12.f), 12.f, DsRGB(0xD8, 0xD0, 0xA0) * 0.55f, 0.4f, 0.8f);
			for (int32 c = 0; c < 10; ++c) B.K.Add(FSurf::Stone(DsRGB(0xB0, 0x98, 0x70) * 0.5f, 0.5f), FVector(60.f, 40.f, 14.f), FVector((B.Rand(c * 3) - 0.5f) * W * 0.8f, (B.Rand(c * 7) - 0.5f) * Dp * 0.8f, 6.f), FRotator(10.f * c, 37.f * c, 0.f));
		}
		else if (Id == TEXT("kennels"))
		{
			for (int32 k = 0; k < 8; ++k) { B.K.Add(DsGold(), FVector(10.f, 30.f, 30.f), FVector(-W * 0.4f + k * W * 0.11f, -Dp * 0.5f + 60.f, 150.f)); B.K.Add(FSurf::Cloth(DsRGB(0xC8, 0xA8, 0x60) * 0.45f), FVector(120.f, 90.f, 14.f), FVector(-W * 0.4f + k * W * 0.11f, -Dp * 0.2f, 7.f)); }
		}
		else if (Id == TEXT("mirror_gallery"))
		{
			// Four bronze mirrors on pivots carry the roof's light shaft round four corners to the Sun-Vault's lock: each is
			// its own part (they turn from the replicated state); the beam shows lit from mirror to mirror once aligned.
			const FVector Spots[4] = { FVector(-1200.f, -400.f, 0.f), FVector(-400.f, 400.f, 0.f), FVector(400.f, -400.f, 0.f), FVector(1200.f, 400.f, 0.f) };
			for (int32 m = 0; m < 4; ++m) C.A->Mirrors.Add(DsMirrorPart(B, Spots[m], 0.f, 70.f));
			B.K.Ring(DsSandstone(0.7f), FVector(-1200.f, -400.f, H + 30.f), 80.f, 60.f, 60.f, 10);           // the roof hole over the first
			B.Loop(TEXT("desert.light_shaft_fixed"), FVector(-1200.f, -400.f, H));
			TArray<FWRLookBlock> Bl;
			FKit K(FTransform::Identity, Bl);
			for (int32 m = 0; m < 4; ++m)
			{
				const FVector A = Spots[m] + FVector(0, 0, 230.f);
				const FVector Bt = m < 3 ? Spots[m + 1] + FVector(0, 0, 230.f) : FVector(1200.f, Dp * 0.5f + 300.f, 230.f);
				K.Rod(FSurf::Lit(DsRGB(0xFF, 0xE0, 0x90), 6.f), A, Bt, 14.f);
			}
			if (USceneComponent* Beam = DsPart(B, FVector::ZeroVector, FRotator::ZeroRotator, Bl)) { Beam->SetVisibility(false, true); C.A->Beam.Add(Beam); }
		}
		else if (Id == TEXT("sun_vault"))
		{
			// The sealed treasury: a stone door with the sun disc lock (it slides aside when the beam strikes it).
			TArray<FWRLookBlock> Bl;
			FKit K(FTransform::Identity, Bl);
			K.Add(DsSandstone(0.95f), FVector(60.f, 340.f, 470.f), FVector(0, 0, 235.f));
			K.Disc(DsGold(), FVector(-32.f, 0, 300.f), FVector(-1, 0, 0), 110.f, 6.f);
			const FVector DoorAt(0, -Dp * 0.5f, 0.f);
			if (USceneComponent* Door = DsPart(B, DoorAt, FRotator(0.f, 90.f, 0.f), Bl)) C.A->VaultDoor = Door;
			else B.Box(DoorAt + FVector(0, 0, 235.f), FVector(170.f, 30.f, 235.f));
			if (!B.bServer && C.A->VaultDoor)
			{
				UBoxComponent* Bx = B.Box(FVector(0, 0, 235.f), FVector(30.f, 170.f, 235.f));
				if (Bx) Bx->AttachToComponent(C.A->VaultDoor, FAttachmentTransformRules::KeepRelativeTransform);
			}
			for (int32 g = 0; g < 6; ++g) B.K.Add(DsGold(), FVector(40.f, 30.f, 20.f), FVector((B.Rand(g) - 0.5f) * W * 0.6f, (B.Rand(g + 5) - 0.5f) * Dp * 0.4f + 200.f, 10.f), FRotator(0.f, g * 40.f, 0.f));
		}
		else if (Id == TEXT("seventh_vault"))
		{
			B.K.Add(DsSandstone(0.95f), FVector(60.f, 340.f, 470.f), FVector(0, -Dp * 0.5f + 30.f, 235.f), FRotator(0.f, 90.f, 0.f));
			B.K.Disc(DsGold(), FVector(0, -Dp * 0.5f, 300.f), FVector(0, -1, 0), 90.f, 6.f);
			B.K.Add(DsSandstone(0.9f), FVector(W * 0.8f, Dp * 0.8f, 20.f), FVector(0, 0, 10.f), FRotator(0.f, 0.f, 4.f));     // the tilted floor
		}
		else if (Id == TEXT("drowned_stair"))
		{
			for (int32 s = 0; s < 12; ++s) B.K.Add(DsSandstone(0.8f), FVector(W - 100.f, 90.f, 30.f), FVector(0, -Dp * 0.45f + s * 90.f, H * 0.5f - s * 30.f));
			B.K.Add(FSurf::Stone(DsRGB(0x7A, 0x64, 0x46) * 0.45f, 0.22f), FVector(W - 40.f, Dp * 0.5f, 6.f), FVector(0, Dp * 0.25f, 40.f));    // the flowing sand's skin
		}
		else if (Id == TEXT("collapse"))
		{
			// The city fell into a sinkhole here: broken walls tilted on a rubble slope, a knotted rope down from the gallery.
			for (int32 b = 0; b < 12; ++b) B.Rock(b, FVector(300.f, 260.f, 200.f) * (0.6f + B.Rand(b)), FVector((B.Rand(b) - 0.5f) * W * 0.8f, (B.Rand(b + 9) - 0.5f) * Dp * 0.8f, 0.f), FRotator(0.f, b * 30.f, 0.f), DsRockTint() * 0.7f, true);
			B.Line(FVector(-W * 0.5f + 80.f, 0, H - 100.f), FVector(-W * 0.5f + 80.f, 40.f, 0.f), 3.f, 0.f, DsRGB(0x9C, 0x84, 0x60) * 0.45f, 10);
		}
		else if (Id == TEXT("faces"))
		{
			for (int32 f = 0; f < 3; ++f) DsColossusHead(B, FVector(-W * 0.3f + f * W * 0.3f, -Dp * 0.2f, 0.f), 90.f, 380.f + 60.f * f, false, f == 1, 300 + f);
		}
		else if (Id == TEXT("lens_forge"))
		{
			for (int32 w = 0; w < 2; ++w) B.K.Add(DsSandstone(0.8f), FVector(160.f, 160.f, 40.f), FVector(-200.f + w * 400.f, -200.f, 80.f), FRotator(0.f, 0.f, 90.f), EWRLookMesh::Cylinder);
			B.K.Add(DsBronzeDull(), FVector(320.f, 320.f, 50.f), FVector(0, 200.f, 25.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);    // the great eye mould
			B.K.Add(DsSandstone(0.4f), FVector(240.f, 240.f, 8.f), FVector(0, 200.f, 52.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		}
		else if (Id == TEXT("spring"))
		{
			// The living spring: clear water rising out of the rock into a basin, the dead river's source; a brazier (the
			// run-back point is a stone the server sets beside it).
			B.K.Add(DsWater(), FVector(700.f, 700.f, 6.f), FVector(0, 0, 20.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			B.K.Ring(DsSandstone(0.85f), FVector(0, 0, 0.f), 380.f, 60.f, 60.f, 18);
			B.Loop(TEXT("desert.spring"), FVector(0, 0, 30.f));
			DsBrazier(B, FVector(-500.f, 0, 0.f), 1.f, true);
		}
		else if (Id == TEXT("copper_deep") || Id == TEXT("iron_deep"))
		{
			for (int32 t = 0; t < 4; ++t) B.K.Add(DsTimber(0.6f), FVector(30.f, 30.f, H - 40.f), FVector((t % 2 ? 1.f : -1.f) * (W * 0.5f - 80.f), (t < 2 ? -0.2f : 0.2f) * Dp, (H - 40.f) * 0.5f));
		}
	}

	void HourholdLink(FDsCtx& C)
	{
		// A gallery between two of the Hourhold's rooms, door to door (cook 2.5).
		FBuild& B = C.B;
		C.A->SetActorRotation(FRotator::ZeroRotator);
		if (!C.D.Links.IsValidIndex(C.A->Index)) return;
		const FWRDesertData::FRoom* A = C.D.FindRoom(C.D.Links[C.A->Index].Key);
		const FWRDesertData::FRoom* Bt = C.D.FindRoom(C.D.Links[C.A->Index].Value);
		if (!A || !Bt) return;
		const FVector Da = DsDoorToward(C.D, *A, Bt->At), Db = DsDoorToward(C.D, *Bt, A->At);
		// Out of each door 3 m straight, then the run between (so the gallery leaves the wall square).
		const auto Out = [](const FWRDesertData::FRoom& R, const FVector& D) { const FVector V = D - R.At; return D + (FMath::Abs(V.X) / R.W > FMath::Abs(V.Y) / R.D ? FVector(FMath::Sign(V.X), 0, 0) : FVector(0, FMath::Sign(V.Y), 0)) * 300.f; };
		const FVector Oa = Out(*A, Da), Ob = Out(*Bt, Db);
		DsGallery(B, C.L(Da), C.L(Oa), 400.f, 460.f, false, C.A->Index * 3, false);
		DsGallery(B, C.L(Oa), C.L(Ob), 400.f, 460.f, false, C.A->Index * 7, true);
		DsGallery(B, C.L(Ob), C.L(Db), 400.f, 460.f, false, C.A->Index * 11, false);
	}

	void HourholdWay(FDsCtx& C)
	{
		// A way into the Hourhold from the surface (cook 2.5): H-E1 the Lee Door (a carved lintel in the Mother Dune's lee,
		// buried by the sand except for 3 game hours after a storm: its sand bank is a part that slumps away), H-E2 the Jaw
		// (under the Fallen Hour's chin, past a quicksand floor), H-E3 the Dig (the Lost Expedition's pit: packed sand
		// until someone clears it). A sloping stair gallery from the mouth down to its room.
		FBuild& B = C.B;
		C.A->SetActorRotation(FRotator::ZeroRotator);
		if (!C.D.Ways.IsValidIndex(C.A->Index)) return;
		const FWRDesertData::FWay& W = C.D.Ways[C.A->Index];
		const FWRDesertData::FRoom* R = C.D.FindRoom(W.To);
		if (!R) return;
		const FVector Door = DsDoorToward(C.D, *R, W.From);
		const FVector Mouth(0, 0, -60.f);
		const FVector Dir = (C.L(Door) - Mouth).GetSafeNormal2D();
		const FVector Landing = Mouth + Dir * 600.f + FVector(0, 0, -250.f);
		DsGallery(B, Mouth, Landing, 400.f, 460.f, false, C.A->Index, false);
		DsGallery(B, Landing, C.L(Door), 400.f, 460.f, false, C.A->Index + 9, true);
		// The mouth: a carved lintel and jambs standing in the ground's hole (it covers the hole's edge).
		const FRotator Ry = Dir.Rotation();
		for (int32 sd = -1; sd <= 1; sd += 2) B.K.Add(DsSandstone(0.95f), FVector(120.f, 80.f, 520.f), Mouth + Ry.RotateVector(FVector(0, sd * 250.f, 200.f)), Ry);
		B.K.Add(DsSandstone(1.05f), FVector(140.f, 620.f, 90.f), Mouth + FVector(0, 0, 500.f), Ry);
		B.K.Disc(DsSandstone(1.15f), Mouth + Ry.RotateVector(FVector(-72.f, 0, 500.f)), Ry.RotateVector(FVector(-1, 0, 0)), 70.f, 6.f);
		B.K.Ring(DsSandFill(0.9f), Mouth + FVector(0, 0, 20.f), 420.f, 160.f, 120.f, 14);
		// The sand plug (Lee Door, Dig): a mound filling the mouth with its collision, a part so it can sink away.
		if (W.Kind == TEXT("lee_door") || W.Kind == TEXT("dig"))
		{
			TArray<FWRLookBlock> Bl;
			FKit K(FTransform::Identity, Bl);
			K.Add(DsSandFill(1.f), FVector(560.f, 560.f, 520.f), FVector(0, 0, 0.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			K.Add(DsSandFill(0.92f), FVector(760.f, 760.f, 120.f), FVector(0, 0, 220.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			USceneComponent* Plug = DsPart(B, Mouth + FVector(0, 0, 0.f), FRotator::ZeroRotator, Bl);
			UBoxComponent* Bx = B.Box(Mouth + FVector(0, 0, 0.f), FVector(280.f, 280.f, 260.f));
			if (Plug && Bx) Bx->AttachToComponent(Plug, FAttachmentTransformRules::KeepWorldTransform);
			USceneComponent* Holder = Plug ? Plug : Bx;
			if (W.Kind == TEXT("lee_door")) C.A->LeeSand = Holder;
			else C.A->DigSand = Holder;
			if (W.Kind == TEXT("dig"))
			{
				// The expedition's spoil heap and the shovel left upright in it.
				B.K.Add(DsPaleSand(0.95f), FVector(400.f, 300.f, 120.f), Mouth + FVector(-500.f, 200.f, 80.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
				B.K.Rod(DsTimber(0.7f), Mouth + FVector(-480.f, 200.f, 200.f), Mouth + FVector(-470.f, 210.f, 330.f), 4.f);
				B.K.Add(DsBronzeDull(), FVector(4.f, 26.f, 34.f), Mouth + FVector(-482.f, 198.f, 180.f));
			}
		}
		if (W.Kind == TEXT("jaw")) B.K.Add(FSurf::Stone(DsRGB(0x7A, 0x64, 0x46) * 0.45f, 0.22f), FVector(600.f, 600.f, 6.f), Mouth + FVector(0, 0, 70.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
	}

	// ------------------------------------------------------------------------------------------------ hazards' looks

	void BrineCrust(FDsCtx& C)
	{
		// A brine crust patch on the sabkha (cook 2.8): a faint wet-grey sheen on the white with polygon ridges of salt;
		// under it brine 1.5 m deep (the break is the hazard's: you drop through it). Drawn as the crust's skin.
		FBuild& B = C.B;
		C.A->SetActorRotation(FRotator::ZeroRotator);
		const float R = C.A->Size;
		const float G = B.G(0, 0, 0.f);
		B.K.Add(FSurf::Glaze(DsRGB(0xC8, 0xC4, 0xB8) * 0.6f, 0.35f), FVector(R * 2.f, R * 2.f, 4.f), FVector(0, 0, G + 3.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		for (int32 r = 0; r < 10; ++r) { const float A = B.Rand(r) * 2.f * PI; B.K.Add(DsSalt(), FVector(R * 0.8f, 10.f, 6.f), FVector(FMath::Cos(A) * R * 0.3f, FMath::Sin(A) * R * 0.3f, G + 5.f), FRotator(0.f, B.Rand(r + 3) * 180.f, 0.f)); }
	}

	void FloodWadi(FDsCtx& C)
	{
		// A flood wadi's ledges and rope points (cook 2.8: every slot canyon has ledges and rope points at least every 40 m),
		// and the flood debris lodged at the high-water line (the tree and the wrack the Flood Line shows).
		FBuild& B = C.B;
		C.A->SetActorRotation(FRotator::ZeroRotator);
		if (!C.D.Floods.IsValidIndex(C.A->Index)) return;
		const FWRDesertData::FFlood& F = C.D.Floods[C.A->Index];
		for (int32 l = 0; l < F.Ledges.Num(); ++l)
		{
			const FVector P = C.L(F.Ledges[l]);
			B.K.Add(DsRedRock(0.75f), FVector(180.f, 120.f, 40.f), P, FRotator(0.f, l * 37.f, 0.f));
			B.Box(P, FVector(90.f, 60.f, 20.f), FRotator(0.f, l * 37.f, 0.f));
			// A bronze ring and a knotted rope down to the floor.
			B.K.Ring(DsVerdigris(), P + FVector(0, 0, 160.f), 12.f, 6.f, 4.f, 8);
			B.Line(P + FVector(0, 0, 150.f), P + FVector(30.f, 0, -300.f), 2.f, 0.f, DsRGB(0x9C, 0x84, 0x60) * 0.45f, 6);
			// Wrack at the 2 m line: twigs and a bleached branch jammed in a crack.
			if (l % 2 == 0) B.Limb(P + FVector(-60.f, 0, -100.f), P + FVector(80.f, 30.f, -90.f), 6.f, 3.f, DsDriftC(), 5);
		}
	}

	// ------------------------------------------------------------------------------------------------ decoration cells

	void Decor(FDsCtx& C)
	{
		// One 250 m cell of the decoration rule (build plan 2.2): sea-grass, driftwood, shells, saltbush, palms, reeds,
		// umbrella thorns, prickly pear, red rocks, pavement pebbles, hoodoo stubs. The actor isn't turned.
		FBuild& B = C.B;
		C.A->SetActorRotation(FRotator::ZeroRotator);
		const float Cell = C.A->Size;
		const FVector O = C.A->GetActorLocation();
		const auto In = [&](const FVector& W) { return FMath::Abs(W.X - O.X) <= Cell * 0.5f && FMath::Abs(W.Y - O.Y) <= Cell * 0.5f; };
		const auto Lp = [&](const FVector& W) { return W - O; };
		int32 n = 0;
		for (const auto& D : C.D.Grass) if (In(D.At)) DsGrassTuft(B, Lp(D.At), D.Scale, n++);
		for (const auto& D : C.D.Driftwood)
		{
			if (!In(D.At)) continue;
			const FVector P = Lp(D.At);
			const FVector Dir = FRotator(0.f, D.Yaw, 0.f).Vector();
			B.Limb(P - Dir * 170.f * D.Scale + FVector(0, 0, 12.f), P + Dir * 170.f * D.Scale + FVector(0, 0, 10.f), 16.f * D.Scale, 11.f * D.Scale, DsDriftC(), 7);
			B.SegBox(P - Dir * 160.f * D.Scale + FVector(0, 0, 14.f), P + Dir * 160.f * D.Scale + FVector(0, 0, 12.f), 14.f * D.Scale, 12.f * D.Scale);
		}
		for (const auto& D : C.D.Shells) if (In(D.At)) { const FVector P = Lp(D.At); for (int32 s = 0; s < 6; ++s) B.K.Add(DsBone(1.05f), FVector(12.f, 9.f, 4.f), P + FVector((B.Rand(s + n) - 0.5f) * 60.f, (B.Rand(s * 3 + n) - 0.5f) * 60.f, 2.f), FRotator(70.f, s * 60.f, 0.f), EWRLookMesh::Cone); ++n; }
		for (const auto& D : C.D.Saltbush) if (In(D.At)) { const FVector P = Lp(D.At); for (int32 b = 0; b < 4; ++b) B.Ball(P + FVector((b - 1.5f) * 30.f * D.Scale, (b % 2) * 25.f * D.Scale, 25.f * D.Scale), 40.f * D.Scale, DsRGB(0x8A, 0x9A, 0x86) * 0.45f, 0.95f, 0.6f); }
		for (const auto& D : C.D.Palms) if (In(D.At)) DsPalm(B, Lp(D.At), D.Yaw, D.Scale, n++, (n % 3) == 0);
		for (const auto& D : C.D.Reeds) if (In(D.At)) { const FVector P = Lp(D.At); for (int32 b = 0; b < 6; ++b) B.K.Spike(FSurf::Cloth(DsRGB(0x6E, 0x86, 0x46) * 0.45f), P + FVector(b * 5.f, (b % 2) * 6.f, 0.f), FVector(0.05f * (b - 3), 0.03f * b, 1.f), 3.f, 130.f * D.Scale); }
		for (const auto& D : C.D.Acacias) if (In(D.At)) DsAcacia(B, Lp(D.At), D.Yaw, D.Scale, n++);
		for (const auto& D : C.D.Pears) if (In(D.At)) DsPear(B, Lp(D.At), D.Yaw, D.Scale, n++);
		int32 r = 0;
		for (const auto& D : C.D.Rocks) if (In(D.At)) B.Rock(r++, FVector(160.f, 140.f, 110.f) * D.Scale, Lp(D.At), FRotator(0.f, D.Yaw, 0.f), DsRockTint(), D.Scale > 1.6f);
		for (const auto& D : C.D.Pebbles) if (In(D.At)) { const FVector P = Lp(D.At); for (int32 s = 0; s < 8; ++s) B.K.Add(DsVarnish(1.2f), FVector(16.f, 12.f, 6.f), P + FVector((B.Rand(s + n) - 0.5f) * 120.f, (B.Rand(s * 5 + n) - 0.5f) * 120.f, 2.f), FRotator(0.f, s * 40.f, 0.f)); ++n; }
		for (const auto& D : C.D.Hoodoos) if (In(D.At)) DsHoodoo(B, Lp(D.At), 600.f + 900.f * D.Scale, 160.f + 120.f * D.Scale, n++);
	}

	// ------------------------------------------------------------------------------------------------ dispatch

	void Build(AWRDesertScenery* A)
	{
		const FName K = A->Kind;
		const bool bDecor = K == TEXT("decor");
		const bool bLandmark = K == TEXT("haboob_wall") || K == TEXT("gnomon") || K == TEXT("sun_gate") || K == TEXT("needles") || K == TEXT("tall_wreck") || K == TEXT("fallen_hour") || K == TEXT("keelyard") || K == TEXT("sevenfold_wells") || K == TEXT("whale_road") || K == TEXT("leviathan_ribs");
		const bool bUnder = K == TEXT("hh_room") || K == TEXT("hh_link") || K == TEXT("hh_way") || K == TEXT("cave");
		const float Cull = bDecor ? WRTune(TEXT("desertWorld.decorCull"), 22000.f) : bLandmark ? WRTune(TEXT("desertWorld.landmarkCull"), 400000.f) : bUnder ? WRTune(TEXT("desertWorld.underCull"), 25000.f) : WRTune(TEXT("desertWorld.siteCull"), 40000.f);
		FDsCtx C(A, Cull);
		if (K == TEXT("haboob_wall")) HaboobWall(C);
		else if (K == TEXT("gnomon")) Gnomon(C);
		else if (K == TEXT("sun_gate")) SunGate(C);
		else if (K == TEXT("mother_dune")) MotherDune(C);
		else if (K == TEXT("whale_road")) WhaleRoad(C);
		else if (K == TEXT("keelyard")) Keelyard(C);
		else if (K == TEXT("tall_wreck")) TallWreck(C);
		else if (K == TEXT("fallen_hour")) FallenHour(C);
		else if (K == TEXT("needles")) Needles(C);
		else if (K == TEXT("glass_canyon")) GlassCanyon(C);
		else if (K == TEXT("sevenfold_wells")) SevenfoldWells(C);
		else if (K == TEXT("leviathan_ribs")) LeviathanRibs(C);
		else if (K.ToString().StartsWith(TEXT("shrine"))) Shrine(C);
		else if (K == TEXT("camp")) Camp(C);
		else if (K == TEXT("runback")) Runback(C);
		else if (K == TEXT("amastan_shade")) AmastanShade(C);
		else if (K == TEXT("cave")) Cave(C);
		else if (K == TEXT("hh_room")) HourholdRoom(C);
		else if (K == TEXT("hh_link")) HourholdLink(C);
		else if (K == TEXT("hh_way")) HourholdWay(C);
		else if (K == TEXT("brine")) BrineCrust(C);
		else if (K == TEXT("flood_wadi")) FloodWadi(C);
		else if (bDecor) Decor(C);
		else if (!Site(C) && !Kit(C))
		{
			UE_LOG(LogWildreach, Warning, TEXT("DESERT scenery: no builder for %s (%s)"), *K.ToString(), *A->SiteId.ToString());
		}
		C.B.Finish();
	}
}
