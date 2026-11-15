// The Crystal caves' surface scenery (cooks/biome-crystal.md 2.1-2.4, 2.20; cooks/build-biome-crystal.md 2.5): the
// camps at the heads of their geos with the cliff stairs down to the coves, the castaways' wrecks, the five landmarks'
// surface parts (the Prism Crown, the Hollowed Giantess, the Split Geode), the four descents with their kerbs, arches and
// stairs, the Organ's vents, the collapsed domes, the chime roads, the hand sites S1-S6, the surface kit and the plateau's
// columns. Every piece answers why it's here, what it connects to and how it meets what it touches (build cook 2.5).
// The caves' furniture is in WRCrystalSceneryCaves.cpp; dispatch at the end of this file.

#include "WRCrystalWorld.h"
#include "WRCrystalLook.h"
#include "WRCrystalTypes.h"
#include "Wildreach.h"
#include "Components/InstancedStaticMeshComponent.h"

namespace WRCrystalSurf
{
	using namespace WRCrystalLook;
	using FData = FWRCrystalData;

	/** Where a cliff face stands at height Z along a local line from (X0, Y) toward +X: the first x where the ground falls
	 *  under Z (the face), searched in 50 cm steps. */
	float FaceX(FBuild& B, float X0, float Y, float Z, float MaxRun = 6000.f)
	{
		for (float X = X0; X < X0 + MaxRun; X += 50.f) if (B.G(X, Y, -1.0e6f) < Z - 40.f) return X;
		return X0 + MaxRun;
	}

	/** A castaway's boat on its side (world-map newcomer rule: newer rope and canvas than any ruin): keel, stem and stern
	 *  posts, ribs, the strakes left on its low side, a mast stump with its stay, the sail dragged into a lean-to. On a
	 *  crystal reef when bReef (the Ringing Wreck: every wave rings the reef). Local frame: the hull along X. */
	void Wreck(FBuild& B, const FVector& At, float Yaw, bool bReef, int32 Seed)
	{
		const FRotator R(0.f, Yaw, 18.f);   // heeled over on its port side
		const auto P = [&](float X, float Y, float Z) { return At + R.RotateVector(FVector(X, Y, Z)); };
		const float Len = 900.f, Beam = 260.f;
		B.K.Rod(Tarred(), P(-Len * 0.5f, 0.f, 20.f), P(Len * 0.5f, 0.f, 20.f), 22.f, true);
		B.K.Rod(Tarred(), P(Len * 0.5f, 0.f, 20.f), P(Len * 0.5f + 60.f, 0.f, 210.f), 18.f, true);
		B.K.Rod(Tarred(), P(-Len * 0.5f, 0.f, 20.f), P(-Len * 0.5f - 30.f, 0.f, 190.f), 18.f, true);
		for (int32 i = 0; i < 9; ++i)
		{
			const float X = -Len * 0.42f + i * (Len * 0.84f / 8.f);
			const float W = Beam * 0.5f * FMath::Sin(PI * (0.15f + 0.7f * i / 8.f));
			const bool bBroken = Hash(Seed, i) > 0.65f;
			for (const float S : { -1.f, 1.f })
			{
				if (bBroken && S > 0.f) continue;
				// A bent rib: the turn of the bilge, then up to the gunwale.
				B.K.Rod(Timber(0.8f), P(X, 0.f, 25.f), P(X, S * W * 0.85f, 70.f), 12.f, true);
				B.K.Rod(Timber(0.8f), P(X, S * W * 0.85f, 70.f), P(X, S * W, 180.f), 12.f, true);
			}
		}
		// The strakes on the low side (the high side's are gone to the sea).
		for (int32 s = 0; s < 4; ++s)
		{
			const float Z = 45.f + s * 34.f, Yo = -Beam * 0.5f * (0.6f + 0.1f * s);
			B.K.Add(Timber(0.95f + 0.1f * Hash(Seed, s + 20)), FVector(Len * (0.8f - 0.08f * s), 4.f, 32.f), P(0.f, Yo, Z), R + FRotator(0.f, 0.f, -20.f + s * 8.f));
		}
		B.K.Rod(Timber(1.1f), P(80.f, 0.f, 20.f), P(80.f, -30.f, 260.f), 14.f);
		B.K.Rod(Rope(), P(80.f, -30.f, 250.f), P(Len * 0.5f + 40.f, 0.f, 200.f), 1.5f);
		// The sail dragged off it: a lean-to on two oars over the bedrolls on the dry side.
		const FVector Oa = At + FRotator(0.f, Yaw, 0.f).RotateVector(FVector(-100.f, 320.f, 0.f));
		B.K.Rod(Drift(), Oa + FVector(0, 0, 0), Oa + FVector(0, 0, 170.f), 6.f);
		B.K.Rod(Drift(), Oa + FRotator(0.f, Yaw, 0.f).RotateVector(FVector(300.f, 0.f, 0.f)), Oa + FRotator(0.f, Yaw, 0.f).RotateVector(FVector(300.f, 0.f, 170.f)), 6.f);
		B.K.Add(Canvas(), FVector(320.f, 230.f, 3.f), Oa + FRotator(0.f, Yaw, 0.f).RotateVector(FVector(150.f, 90.f, 95.f)), FRotator(-32.f, Yaw + 90.f, 0.f) + FRotator(0.f, -90.f, 0.f));
		B.Box(At + FVector(0, 0, 90.f), FVector(Len * 0.5f, Beam * 0.5f, 90.f), FRotator(0.f, Yaw, 0.f));
		if (bReef)
		{
			// The crystal reef it struck: violet points at the waterline all round, each wave rings them.
			for (int32 c = 0; c < 14; ++c)
			{
				const float A = 2.f * PI * c / 14.f + Hash(Seed, c + 40);
				const FVector Root = At + FVector(FMath::Cos(A) * (560.f + 120.f * Hash(Seed, c)), FMath::Sin(A) * (380.f + 100.f * Hash(Seed, c + 2)), -40.f);
				Cluster(B, Root, FVector(FMath::Cos(A) * 0.4f, FMath::Sin(A) * 0.4f, 1.f), 120.f + 80.f * Hash(Seed, c + 5), Seed + c, Amethyst(0.1f), AmethystPale(0.16f));
			}
		}
	}

	/** A camp at its geo's head (crystal 2.1; world-map camp rule): the fire ring and bedrolls on the cliff top, the
	 *  geo's head wall lined with the broken geode's amethyst, the plank stair hung on bronze pins down the head wall to
	 *  the shingle cove, a chime arch at the stair's head, the castaways' wreck on the cove (K2, K3; K1's is the Ringing
	 *  Wreck out on its reef, S2). The actor faces the sea (+X). */
	void Camp(FBuild& B, AWRCrystalScenery* A)
	{
		const FData& D = FData::Get();
		const FData::FSite* S = D.FindSite(A->SiteId);
		if (!S) return;
		const int32 Seed = int32(GetTypeHash(A->SiteId) % 997u);
		// The fire: a ring of beach stones, embers that glow at night, a driftwood spit.
		const float G0 = B.G(0.f, 0.f);
		for (int32 i = 0; i < 10; ++i)
		{
			const float Ang = 2.f * PI * i / 10.f;
			B.Rock(i, FVector(32.f, 26.f, 20.f), FVector(FMath::Cos(Ang) * 70.f, FMath::Sin(Ang) * 70.f, G0), FRotator(0.f, Ang * 57.3f, 0.f), Rgb(46, 48, 52) * 0.6f, false, 6.f);
		}
		B.K.Add(Basalt(0.5f), FVector(100.f, 100.f, 8.f), FVector(0.f, 0.f, G0 + 3.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		B.N.Add(Ember(1.2f), FVector(70.f, 70.f, 10.f), FVector(0.f, 0.f, G0 + 8.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		B.Lamp(FVector(0.f, 0.f, G0 + 70.f), FireLight(), 2600.f, 1400.f);
		for (const float Sx : { -1.f, 1.f }) B.K.Rod(Drift(), FVector(0.f, Sx * 90.f, G0), FVector(0.f, Sx * 90.f, G0 + 95.f), 5.f);
		B.K.Rod(Drift(), FVector(0.f, -95.f, G0 + 90.f), FVector(0.f, 95.f, G0 + 90.f), 4.f);
		// Bedrolls behind the fire (landward), and the sea-chest they carried up.
		for (int32 i = 0; i < 3; ++i)
		{
			const FVector P(-280.f, -220.f + i * 220.f, 0.f);
			B.K.Add(Wool(0.8f + 0.2f * Hash(Seed, i)), FVector(200.f, 80.f, 16.f), FVector(P.X, P.Y, B.G(P.X, P.Y) + 8.f), FRotator(0.f, -8.f + 8.f * i, 0.f));
			B.K.Add(Wool(0.6f), FVector(30.f, 80.f, 26.f), FVector(P.X + 90.f, P.Y, B.G(P.X, P.Y) + 13.f), FRotator(0.f, -8.f + 8.f * i, 0.f));
		}
		B.K.Add(Tarred(), FVector(90.f, 55.f, 50.f), FVector(-180.f, 330.f, B.G(-180.f, 330.f) + 25.f), FRotator(0.f, 12.f, 0.f));
		B.K.Add(Bronze(), FVector(94.f, 6.f, 4.f), FVector(-180.f, 330.f, B.G(-180.f, 330.f) + 48.f), FRotator(0.f, 12.f, 0.f));
		// The head wall: from the cliff's top edge down to the cove. Find the face at the top, the beach below.
		const float Top = B.G(0.f, 0.f);
		const FVector CoveL = S->Cove.IsZero() ? FVector(2600.f, 0.f, 0.f) : B.Local(S->Cove);
		const float Beach = FMath::Min(B.G(CoveL.X, CoveL.Y, CoveL.Z), Top - 500.f);
		const float Edge = FaceX(B, 0.f, 0.f, Top - 60.f);
		ChimeArch(B, FVector(Edge - 260.f, 520.f, B.G(Edge - 260.f, 520.f)), 0.f, 1.f, Seed);
		// The broken geode in the head wall: amethyst clusters out of the face at every height.
		for (int32 c = 0; c < 18; ++c)
		{
			const float Y = (Hash(Seed, c + 60) - 0.5f) * 2200.f;
			const float Z = FMath::Lerp(Beach + 250.f, Top - 300.f, Hash(Seed, c + 80));
			const float Fx = FaceX(B, -400.f, Y, Z, 5000.f);
			Cluster(B, FVector(Fx + 10.f, Y, Z), FVector(1.f, 0.f, 0.15f), 140.f + 120.f * Hash(Seed, c + 90), Seed + c * 7, Amethyst(0.12f), AmethystPale(0.2f));
		}
		B.Lamp(FVector(Edge + 900.f, 0.f, Beach + 600.f), VioletLight(), 1400.f, 3200.f);
		// The stair down: flights of planks on bronze pins across the head wall, 30 degrees, landings at each turn.
		{
			float Z = Top - 40.f, Y = 600.f, Dir = -1.f;
			FVector Prev(Edge + 60.f, Y, Z);
			int32 Flight = 0;
			while (Z > Beach + 60.f && Flight < 14)
			{
				const float Rise = FMath::Min(1100.f, Z - Beach - 30.f);
				const float Run = Rise / FMath::Tan(FMath::DegreesToRadians(30.f));
				const float Y1 = Y + Dir * FMath::Min(Run, 1200.f);
				const float Z1 = Z - Rise * FMath::Min(1.f, 1200.f / Run);
				const float X0 = FaceX(B, -200.f, Y, Z - 50.f, 6000.f) + 70.f, X1 = FaceX(B, -200.f, Y1, Z1 - 50.f, 6000.f) + 70.f;
				const FVector P0(X0, Y, Z), P1(X1, Y1, Z1);
				const int32 Treads = FMath::Max(4, int32(FVector::Dist(P0, P1) / 30.f));
				for (int32 t = 0; t <= Treads; ++t)
				{
					const FVector T = FMath::Lerp(P0, P1, float(t) / Treads);
					B.K.Add(Timber(0.85f + 0.3f * Hash(Seed, Flight * 50 + t)), FVector(28.f, 110.f, 6.f), T, FRotator(0.f, 0.f, 0.f));
					if (t % 3 == 0)
					{
						B.K.Rod(Bronze(), T + FVector(-60.f, 0.f, -8.f), T + FVector(25.f, 0.f, -8.f), 4.f);
						B.K.Rod(Rope(), T + FVector(48.f, 0.f, 0.f), T + FVector(48.f, 0.f, 95.f), 2.5f);
					}
				}
				B.K.Rod(Rope(), P0 + FVector(48.f, 0.f, 95.f), P1 + FVector(48.f, 0.f, 95.f), 3.f);
				B.SegBox(P0 - FVector(0, 0, 8.f), P1 - FVector(0, 0, 8.f), 55.f, 8.f);
				// The landing: a plank deck on brackets.
				B.K.Add(Timber(0.9f), FVector(140.f, 160.f, 8.f), P1 + FVector(0.f, Dir * 60.f, -4.f));
				B.Box(P1 + FVector(0.f, Dir * 60.f, -10.f), FVector(70.f, 80.f, 8.f));
				for (const float Yb : { -50.f, 50.f }) B.K.Rod(Bronze(0.8f), P1 + FVector(-70.f, Dir * 60.f + Yb, -10.f), P1 + FVector(-70.f, Dir * 60.f + Yb, -90.f) + FVector(60.f, 0.f, 0.f), 5.f);
				Prev = P1;
				Y = Y1 + Dir * 120.f;
				Z = Z1;
				Dir = -Dir;
				++Flight;
			}
		}
		// The castaways' wreck on the cove (K1's lies out on its reef: the Ringing Wreck).
		if (A->SiteId != TEXT("K1")) Wreck(B, FVector(CoveL.X + 300.f, CoveL.Y - 250.f, Beach + 20.f), 24.f, false, Seed);
	}

	/** L1 the Prism Crown: eight violet spires 40-60 m in a ring where the Prism Hall's dome breaks the surface, each the
	 *  top of a light shaft from the hall below, a bronze collar with a hexafoil at each base; at night a faint violet
	 *  column rises from each (the turntables' facing drives which: WRCrystalBosses; here the resting state). */
	void PrismCrown(FBuild& B, AWRCrystalScenery* A)
	{
		const FData::FSite* S = FData::Get().FindSite(TEXT("L1"));
		if (!S) return;
		int32 i = 0;
		for (const FData::FSpire& Sp : S->Spires)
		{
			const FVector Base = B.Local(Sp.At);
			const float Gz = B.G(Base.X, Base.Y, Base.Z);
			const FVector Lean = FRotator(0.f, Sp.Yaw, 0.f).RotateVector(FVector(1.f, 0.f, 0.f));
			const FVector Up = (FVector::UpVector * FMath::Cos(FMath::DegreesToRadians(Sp.Tilt)) + Lean * FMath::Sin(FMath::DegreesToRadians(Sp.Tilt))).GetSafeNormal();
			// Stacked prisms: each narrower, the last a pointed cap; each section a little turned (growth twins).
			float R = 340.f, Z = 0.f;
			const float Hs[4] = { 0.38f, 0.3f, 0.2f, 0.12f };
			FVector P(Base.X, Base.Y, Gz - 150.f);
			for (int32 s = 0; s < 4; ++s)
			{
				const float L = Sp.H * Hs[s];
				HexAlong(B, s % 2 ? Amethyst(0.1f) : Amethyst(0.06f), P, Up, R, L + 40.f, i * 11.f + s * 7.f);
				P += Up * L;
				R *= 0.74f;
				Z += L;
			}
			B.K.Spike(AmethystPale(0.2f), P - Up * 20.f, Up, R * 2.f, 600.f);
			// Small points at its foot, and the bronze collar binding it, its hexafoil facing the Severance (west).
			for (int32 c = 0; c < 5; ++c)
			{
				const float Ang = 2.f * PI * c / 5.f + i;
				Cluster(B, FVector(Base.X + FMath::Cos(Ang) * 420.f, Base.Y + FMath::Sin(Ang) * 420.f, Gz - 20.f), FVector(FMath::Cos(Ang) * 0.5f, FMath::Sin(Ang) * 0.5f, 1.f), 160.f + 90.f * Hash(i, c), i * 31 + c, Amethyst(0.08f), AmethystPale(0.14f));
			}
			B.K.Add(Bronze(), FVector(780.f, 780.f, 70.f), FVector(Base.X, Base.Y, Gz + 90.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			B.K.Add(Verdigris(), FVector(800.f, 800.f, 14.f), FVector(Base.X, Base.Y, Gz + 128.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			Hexafoil(B, Bronze(1.2f), FVector(Base.X - 400.f, Base.Y, Gz + 90.f), FVector(-1.f, 0.f, 0.f), 55.f, 4.f);
			B.Box(FVector(Base.X, Base.Y, Gz + Sp.H * 0.3f), FVector(300.f, 300.f, Sp.H * 0.3f), FRotationMatrix::MakeFromZ(Up).Rotator());
			// The night column: a faint violet beam straight up out of the spire's tip.
			B.N.Rod(HoleGlow(0.9f), P + Up * 600.f, P + Up * 600.f + FVector(0.f, 0.f, 18000.f), 140.f);
			B.Lamp(FVector(Base.X, Base.Y, Gz + 400.f), VioletLight(), 1800.f, 2500.f);
			++i;
		}
	}

	/** L2 the Hollowed Giantess: a 30 m crystal woman kneeling at the gorge's mouth facing west into the Severance, arms
	 *  spread as if holding a door; her back whole and lilac, her front a hollow full of grey bleached crystal (the Void
	 *  took her from the front); a Wardcarver ward cage of 16 bronze rods in four rings still round her, two rods bent
	 *  out on the east side where the Severance Shrine's brazier stands inside. Local +X is west (she faces it). */
	void Giantess(FBuild& B, AWRCrystalScenery* A)
	{
		const float G0 = B.G(0.f, 0.f);
		const FLinearColor Lilac = Rgb(196, 184, 230) * 0.55f;
		const auto L = [&](const FVector& P0, const FVector& P1, float R0, float R1) { B.Limb(P0 + FVector(0, 0, G0), P1 + FVector(0, 0, G0), R0, R1, Lilac, 10, 0.25f); };
		// Shins on the ground, knees forward, thighs up to the hips.
		for (const float S : { -1.f, 1.f })
		{
			L(FVector(-650.f, S * 300.f, 90.f), FVector(380.f, S * 330.f, 130.f), 110.f, 150.f);
			L(FVector(380.f, S * 330.f, 130.f), FVector(-60.f, S * 260.f, 900.f), 170.f, 230.f);
			B.Ball(FVector(380.f, S * 330.f, 140.f + G0), 165.f, Lilac, 0.3f);
			B.Box(FVector(-120.f, S * 310.f, G0 + 120.f), FVector(560.f, 160.f, 120.f));
		}
		// The body, leaning a little forward into the gorge; shoulders; the head bowed toward the gorge.
		L(FVector(-60.f, 0.f, 850.f), FVector(-20.f, 0.f, 1500.f), 360.f, 330.f);
		L(FVector(-20.f, 0.f, 1500.f), FVector(60.f, 0.f, 2050.f), 330.f, 380.f);
		L(FVector(60.f, -470.f, 2120.f), FVector(60.f, 470.f, 2120.f), 170.f, 170.f);
		L(FVector(60.f, 0.f, 2150.f), FVector(110.f, 0.f, 2400.f), 110.f, 100.f);
		B.Ball(FVector(170.f, 0.f, 2620.f + G0), 230.f, Lilac, 0.25f, 1.15f);
		B.Box(FVector(0.f, 0.f, G0 + 1500.f), FVector(380.f, 400.f, 700.f));
		// Arms spread wide, palms forward: holding a door that isn't there.
		for (const float S : { -1.f, 1.f })
		{
			L(FVector(60.f, S * 470.f, 2120.f), FVector(220.f, S * 1050.f, 2280.f), 150.f, 120.f);
			L(FVector(220.f, S * 1050.f, 2280.f), FVector(420.f, S * 1480.f, 2560.f), 120.f, 95.f);
			B.K.Add(FSurf::Glass(Lilac, 0.05f), FVector(60.f, 210.f, 300.f), FVector(470.f, S * 1530.f, G0 + 2640.f), FRotator(0.f, S * -15.f, 0.f));
		}
		// Her back's crystal: points out of the shoulder blades and the spine, lit lilac from inside at night.
		for (int32 c = 0; c < 14; ++c)
		{
			const float Z = 900.f + c * 95.f, Y = (c % 2 ? -1.f : 1.f) * (80.f + 40.f * Hash(7, c));
			Point(B, AmethystPale(0.1f), FVector(-330.f, Y, G0 + Z), FVector(-1.f, Y * 0.002f, 0.35f), 260.f + 120.f * Hash(9, c), 80.f, c * 13.f);
		}
		B.N.Add(FSurf::Lit(Rgb(216, 208, 255) * 0.5f, 0.6f), FVector(30.f, 640.f, 1200.f), FVector(-360.f, 0.f, G0 + 1500.f), FRotator(0.f, 0.f, 0.f));
		B.Lamp(FVector(-900.f, 0.f, G0 + 1500.f), LilacLight(), 2400.f, 2600.f);
		// The hollow front: the black of her emptied chest and belly, grey bleached crystal crowding it.
		B.K.Add(FSurf::Stone(Rgb(10, 10, 12), 0.95f), FVector(40.f, 560.f, 1100.f), FVector(330.f, 0.f, G0 + 1450.f));
		for (int32 c = 0; c < 22; ++c)
		{
			const FVector Root(340.f, (Hash(3, c) - 0.5f) * 520.f, G0 + 950.f + Hash(4, c) * 1000.f);
			Point(B, Bleached(0.85f + 0.3f * Hash(5, c)), Root, FVector(1.f, (Hash(6, c) - 0.5f) * 0.8f, (Hash(8, c) - 0.5f) * 0.8f), 120.f + 160.f * Hash(2, c), 40.f + 30.f * Hash(1, c), c * 17.f);
		}
		// The ward cage: 16 rods, 4 rings, two bent out on the east (her back) where the shrine stands inside.
		const float CR = 1300.f, CH = 3200.f;
		for (int32 r = 0; r < 16; ++r)
		{
			const float Ang = 2.f * PI * (r + 0.5f) / 16.f;
			const FVector Dir(FMath::Cos(Ang), FMath::Sin(Ang), 0.f);
			const FVector Foot = Dir * CR;
			const float Fz = B.G(Foot.X, Foot.Y, G0);
			const bool bBent = r == 7 || r == 8;   // facing -X (east): the way in to the shrine
			if (bBent)
			{
				B.K.Rod(Bronze(0.8f), FVector(Foot.X, Foot.Y, Fz), FVector(Foot.X, Foot.Y, Fz + 900.f) + Dir * 250.f, 14.f);
				B.K.Rod(Bronze(0.8f), FVector(Foot.X, Foot.Y, Fz + 900.f) + Dir * 250.f, FVector(Foot.X * 0.55f, Foot.Y * 0.55f, G0 + CH), 14.f);
				continue;
			}
			B.K.Rod(Bronze(), FVector(Foot.X, Foot.Y, Fz - 30.f), FVector(Foot.X, Foot.Y, G0 + CH * 0.8f), 16.f);
			B.K.Rod(Bronze(), FVector(Foot.X, Foot.Y, G0 + CH * 0.8f), FVector(Foot.X * 0.55f, Foot.Y * 0.55f, G0 + CH), 16.f);
			B.Box(FVector(Foot.X, Foot.Y, G0 + CH * 0.4f), FVector(14.f, 14.f, CH * 0.4f));
			B.K.Add(Basalt(), FVector(70.f, 70.f, 40.f), FVector(Foot.X, Foot.Y, Fz + 10.f));
		}
		for (int32 k = 0; k < 4; ++k)
		{
			const float Z = G0 + 500.f + k * 650.f;
			for (int32 r = 0; r < 16; ++r)
			{
				if (k < 2 && (r == 7)) continue;   // the bent pair's gap
				const float A0 = 2.f * PI * (r + 0.5f) / 16.f, A1 = 2.f * PI * (r + 1.5f) / 16.f;
				B.K.Rod(Bronze(0.9f), FVector(FMath::Cos(A0) * CR, FMath::Sin(A0) * CR, Z), FVector(FMath::Cos(A1) * CR, FMath::Sin(A1) * CR, Z), 10.f);
			}
		}
		Hexafoil(B, Bronze(1.2f), FVector(0.f, 0.f, G0 + CH + 30.f), FVector::UpVector, 260.f, 6.f);
	}

	/** L3 the Split Geode: an 80 m geode cut in half by the sea, its amethyst interior facing the ocean like a violet
	 *  cathedral wall, the sea washing its floor; agate bands at the cut edge; Chimecrab ledges. The bay it sits in is cut
	 *  into the cliff (crystal.mjs). Local +X faces the sea (east). */
	void SplitGeode(FBuild& B, AWRCrystalScenery* A)
	{
		const float R = 3900.f, Rz = 4300.f, Cz = 300.f;
		const FLinearColor Violet = Rgb(107, 63, 160) * 0.6f, Agate = Rgb(230, 216, 192) * 0.55f, Rust = Rgb(168, 90, 58) * 0.5f, Basal = Rgb(28, 28, 32) * 0.7f;
		// The half shell: its concave face toward the sea (a lathe of rings from the cut edge back to the crown).
		TArray<TArray<FVector>> Rings;
		const int32 NA = 24, NB = 18;
		for (int32 b = 0; b <= NB; ++b)
		{
			const float Phi = -0.15f + (PI * 0.5f + 0.15f) * b / NB;   // from just under the sea up over the crown
			TArray<FVector>& Ring = Rings.AddDefaulted_GetRef();
			for (int32 a = 0; a <= NA; ++a)
			{
				const float Th = PI * 0.5f + PI * a / NA;   // the back half (west)
				Ring.Add(FVector(FMath::Cos(Th) * FMath::Cos(Phi) * R, FMath::Sin(Th) * FMath::Cos(Phi) * R, Cz + FMath::Sin(Phi) * Rz));
			}
		}
		for (int32 b = 0; b < NB; ++b) for (int32 a = 0; a < NA; ++a)
		{
			const FVector P0 = Rings[b][a], P1 = Rings[b][a + 1], P2 = Rings[b + 1][a + 1], P3 = Rings[b + 1][a];
			const float Band = FMath::Frac((P0.Z + 5000.f) / 600.f);
			const FColor C = FWRToolMesh::Pack(b < 2 ? (Band < 0.5f ? Agate : Rust) : FMath::Lerp(Violet, Violet * 0.7f, Hash(a, b)), 0.45f);
			// Seen from inside (the sea side): the face's outward is toward the centre.
			const FVector In = (FVector(0, 0, Cz) - (P0 + P2) * 0.5f).GetSafeNormal();
			FVector Nrm = FVector::CrossProduct(P1 - P0, P2 - P0);
			if (FVector::DotProduct(Nrm, In) > 0.f) { B.M.Tri(P0, P1, P2, C, FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector); B.M.Tri(P0, P2, P3, C, FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector); }
			else { B.M.Tri(P0, P2, P1, C, FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector); B.M.Tri(P0, P3, P2, C, FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector); }
			// The outer skin (basalt) for whoever looks from the cliff top.
			if (FVector::DotProduct(Nrm, In) > 0.f) B.M.Tri(P0, P2, P1, FWRToolMesh::Pack(Basal, 0.8f), FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector);
			else B.M.Tri(P0, P1, P2, FWRToolMesh::Pack(Basal, 0.8f), FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector);
		}
		// The lining: points growing toward the middle, densest high up; the sea rings the low ones.
		for (int32 c = 0; c < 70; ++c)
		{
			const float Th = PI * 0.5f + PI * (0.05f + 0.9f * Hash(11, c)), Phi = (PI * 0.48f) * Hash(12, c);
			const FVector W(FMath::Cos(Th) * FMath::Cos(Phi) * R * 0.99f, FMath::Sin(Th) * FMath::Cos(Phi) * R * 0.99f, Cz + FMath::Sin(Phi) * Rz * 0.99f);
			if (W.Z < 100.f) continue;
			Cluster(B, W, (FVector(0, 0, Cz) - W).GetSafeNormal(), 260.f + 260.f * Hash(13, c), 500 + c, Amethyst(0.1f), AmethystPale(0.18f));
		}
		// Chimecrab ledges: three basalt shelves across the lower lining.
		for (int32 l = 0; l < 3; ++l)
		{
			const float Z = 700.f + l * 900.f, Th = PI * (0.7f + 0.3f * l);
			const FVector P(FMath::Cos(Th) * R * 0.9f, FMath::Sin(Th) * R * 0.9f, Z);
			B.K.Add(Basalt(), FVector(700.f, 260.f, 60.f), P, FRotator(0.f, FMath::RadiansToDegrees(Th) + 90.f, 0.f));
			B.Box(P, FVector(350.f, 130.f, 30.f), FRotator(0.f, FMath::RadiansToDegrees(Th) + 90.f, 0.f));
		}
		B.Lamp(FVector(-1500.f, 0.f, 2400.f), VioletLight(), 3500.f, 6000.f);
	}

	/** A descent (crystal 2.4): its kerb and chime arch at the lip, then its way down. D1 the North Dome: stone stairs
	 *  cut into the rind round the shaft; D2 the Bell Throat: a basalt beam across the throat with the Wardcarvers' 2 m
	 *  bronze bell, a plank stair hung on bronze pins; D3 the Sinking Geode: fallen rind blocks along its ramp; D4 the
	 *  South Steam Vent: stone steps spiralling its wall. Where a stair runs on below the shaft into the hall's open
	 *  dome, it hangs from bronze chains anchored in the roof (nothing floats). */
	void Descent(FBuild& B, AWRCrystalScenery* A)
	{
		const FData& D = FData::Get();
		const FData::FDescent* Dn = D.Descents.FindByPredicate([A](const FData::FDescent& X) { return X.Id == A->SiteId; });
		if (!Dn) return;
		const int32 Seed = int32(GetTypeHash(A->SiteId) % 991u);
		const FData::FHall* H = D.FindHall(Dn->Hall);
		const float Lip = B.G(Dn->R + 300.f, 0.f);
		if (Dn->How == TEXT("rind_ramp"))
		{
			// Rind blocks along the ramp's edges: big slabs of the collapsed dome, agate-banded on their broken faces.
			const FData::FTube* T = D.Tubes.FindByPredicate([](const FData::FTube& X) { return X.Descent == TEXT("D3"); });
			if (!T) return;
			for (int32 i = 0; i + 6 < T->Pts.Num(); i += 5)
			{
				const FVector P = B.Local(T->Pts[i]);
				const FVector Dir = (T->Pts[i + 5] - T->Pts[i]).GetSafeNormal2D();
				const FVector Side(-Dir.Y, Dir.X, 0.f);
				for (const float S : { -1.f, 1.f })
				{
					const FVector Q = P + Side * S * (T->R * 0.72f) + FVector(0, 0, 60.f);
					B.K.Add(Hash(Seed, i) > 0.5f ? AgateCream(0.8f) : Basalt(), FVector(260.f + 140.f * Hash(Seed, i + 3), 160.f, 120.f + 80.f * Hash(Seed, i + 7)), Q, FRotator(Hash(Seed, i + 11) * 25.f, FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)) + 20.f * S, Hash(Seed, i + 13) * 30.f - 15.f));
				}
			}
			ChimeArch(B, FVector(-900.f, -600.f, B.G(-900.f, -600.f)), 70.f, 1.15f, Seed);
			return;
		}
		Kerb(B, FVector(0.f, 0.f, Lip), Dn->R - 160.f, 120.f, Seed);
		ChimeArch(B, FVector(Dn->R + 420.f, 0.f, B.G(Dn->R + 420.f, 0.f)), 0.f, 1.f, Seed);
		const FData::FShaft* Sh = D.Shafts.FindByPredicate([A](const FData::FShaft& X) { return X.Id == A->SiteId; });
		const float ZTop = Lip - 80.f - A->GetActorLocation().Z;
		const float ZFloor = (H ? H->At.Z : A->GetActorLocation().Z - 4000.f) - A->GetActorLocation().Z;
		const float ZRoof = (Sh ? Sh->Z0 : H->At.Z + H->H * 0.6f) - A->GetActorLocation().Z;
		const FName Style = Dn->How == TEXT("bell_stair") ? FName(TEXT("rope")) : FName(TEXT("stone"));
		const float Width = Dn->How == TEXT("lip_stairs") ? 260.f : 200.f;
		SpiralStair(B, FVector::ZeroVector, Dn->R, ZTop, ZFloor + 10.f, Width, Style, Seed, 0.f);
		// Below the shaft the stair hangs in the hall's dome: bronze chains from the roof to every fourth tread.
		{
			const float Rm = FMath::Max(150.f, Dn->R - Width * 0.5f - 20.f);
			const float Rise = 2.f * PI * Rm * FMath::Tan(FMath::DegreesToRadians(30.f));
			const float Turns = FMath::Max(0.25f, (ZTop - ZFloor - 10.f) / Rise);
			const int32 Steps = FMath::Max(4, int32(Turns * 24.f));
			for (int32 s = 0; s <= Steps; s += 4)
			{
				const float Z = ZTop - (ZTop - ZFloor - 10.f) * s / Steps;
				if (Z > ZRoof - 50.f) continue;
				const float Ang = 2.f * PI * Turns * s / Steps;
				const FVector Pt(FMath::Cos(Ang) * (Rm + Width * 0.45f), FMath::Sin(Ang) * (Rm + Width * 0.45f), Z);
				// Up to the rock: the chain ends at the roof (the cave's own air says where).
				float Up = ZRoof;
				const FVector W0 = A->GetActorTransform().TransformPosition(Pt);
				for (float dz = 100.f; dz < 6000.f; dz += 100.f) { if (D.Air(W0 + FVector(0, 0, dz)) > 0.f) { Up = Z + dz; break; } }
				B.K.Rod(Bronze(0.75f), Pt, FVector(Pt.X, Pt.Y, Up + 20.f), 3.5f);
				B.K.Add(Bronze(0.6f), FVector(22.f, 22.f, 10.f), FVector(Pt.X, Pt.Y, Up + 6.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			}
		}
		if (Dn->How == TEXT("bell_stair"))
		{
			// The beam across the throat on the kerb, the bell under it on a bronze yoke, its clapper still.
			const float Span = Dn->R * 2.f + 300.f;
			B.K.Add(Basalt(1.1f), FVector(80.f, Span, 70.f), FVector(0.f, 0.f, Lip + 60.f));
			B.Box(FVector(0.f, 0.f, Lip + 60.f), FVector(40.f, Span * 0.5f, 35.f));
			B.K.Add(Bronze(), FVector(30.f, 120.f, 40.f), FVector(0.f, 0.f, Lip + 5.f));
			TArray<FVector> Prof;
			for (int32 i = 0; i <= 8; ++i) { const float T = i / 8.f; const float Rr = 40.f + 60.f * T + 20.f * FMath::Pow(T, 6.f); Prof.Add(FVector(T, Rr, Rr)); }
			B.M.Lathe(FVector(0.f, 0.f, Lip - 30.f), FVector(0.f, 0.f, -1.f), 200.f, Prof, 16, BronzeC(), 0.4f);
			B.K.Rod(Iron(), FVector(0.f, 0.f, Lip - 40.f), FVector(0.f, 0.f, Lip - 200.f), 8.f);
			B.Ball(FVector(0.f, 0.f, Lip - 210.f), 18.f, Rgb(30, 28, 26), 0.5f);
		}
		if (Dn->How == TEXT("vent_steps")) B.Lamp(FVector(0.f, 0.f, ZFloor + 400.f), Rgb(255, 236, 196), 900.f, 2400.f);
	}

	/** One of the Organ's eleven vents (crystal 2.3 L5): a collar of column stumps round the skylight, a bronze tripod
	 *  over it with three crystal bars the wind across the hole rings (the chord in the tube below). */
	void Vent(FBuild& B, AWRCrystalScenery* A)
	{
		const FData& D = FData::Get();
		if (!D.Vents.IsValidIndex(A->Index)) return;
		const FData::FShaft& V = D.Vents[A->Index];
		const int32 Seed = A->Index * 37 + 5;
		const float G0 = B.G(V.R + 200.f, 0.f);
		for (int32 i = 0; i < 9; ++i)
		{
			const float Ang = 2.f * PI * i / 9.f;
			const float X = FMath::Cos(Ang) * (V.R + 40.f), Y = FMath::Sin(Ang) * (V.R + 40.f);
			B.K.Hex(Basalt(0.85f + 0.3f * Hash(Seed, i)), X, Y, B.G(X, Y, G0) - 60.f, B.G(X, Y, G0) + 40.f + 50.f * Hash(Seed, i + 4), 45.f, Ang * 57.3f);
			B.K.Add(SaltRim(), FVector(60.f, 70.f, 4.f), FVector(X, Y, B.G(X, Y, G0) + 42.f + 50.f * Hash(Seed, i + 4)), FRotator(0.f, Ang * 57.3f, 0.f));
		}
		const FVector Apex(0.f, 0.f, G0 + 420.f);
		for (int32 l = 0; l < 3; ++l)
		{
			const float Ang = 2.f * PI * l / 3.f + 0.4f;
			const FVector Foot(FMath::Cos(Ang) * (V.R + 140.f), FMath::Sin(Ang) * (V.R + 140.f), 0.f);
			B.K.Rod(Bronze(0.8f), FVector(Foot.X, Foot.Y, B.G(Foot.X, Foot.Y, G0)), Apex, 6.f);
			Point(B, AmethystPale(0.16f), Apex + FVector(FMath::Cos(Ang) * 40.f, FMath::Sin(Ang) * 40.f, -30.f), FVector(0.f, 0.f, -1.f), 140.f + 40.f * l, 14.f, Ang * 57.3f);
		}
		// The ochre dots the Wardcarvers put at every resonant point: a ring of red marks on the collar's stones.
		for (int32 d = 0; d < 6; ++d)
		{
			const float Ang = 2.f * PI * d / 6.f;
			B.K.Add(Ochre(), FVector(10.f, 10.f, 2.f), FVector(FMath::Cos(Ang) * (V.R + 40.f), FMath::Sin(Ang) * (V.R + 40.f), B.G(FMath::Cos(Ang) * (V.R + 40.f), FMath::Sin(Ang) * (V.R + 40.f), G0) + 92.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		}
	}

	/** A collapsed geode dome (crystal 2.1: the surface reads by its holes): its bowl lined with violet points that glow
	 *  from below at night, a chime arch at its lip on the side the roads come from. */
	void Dome(FBuild& B, AWRCrystalScenery* A)
	{
		const FData& D = FData::Get();
		if (!D.Domes.IsValidIndex(A->Index)) return;
		const FData::FDome& Dm = D.Domes[A->Index];
		const int32 Seed = A->Index * 53 + 9;
		const int32 N = FMath::Clamp(int32(Dm.R / 140.f), 8, 26);
		for (int32 c = 0; c < N; ++c)
		{
			const float Ang = Hash(Seed, c) * 2.f * PI, Rr = FMath::Sqrt(Hash(Seed, c + 100)) * Dm.R * 0.85f;
			const float X = FMath::Cos(Ang) * Rr, Y = FMath::Sin(Ang) * Rr;
			const float Z = B.G(X, Y);
			// Points lean toward the bowl's middle and up, as they grew toward the geode's centre.
			const FVector Up = (FVector(-X, -Y, 0.f).GetSafeNormal() * (Rr / Dm.R) * 0.8f + FVector::UpVector).GetSafeNormal();
			Cluster(B, FVector(X, Y, Z), Up, 90.f + 160.f * Hash(Seed, c + 200) * (1.f - Rr / Dm.R * 0.5f), Seed + c, Amethyst(0.08f), AmethystPale(0.14f));
			if (c % 4 == 0) Cluster(B, FVector(X, Y, Z + 10.f), Up, 70.f, Seed + c + 900, HoleGlow(0.8f), HoleGlow(1.2f), true);
		}
		B.Lamp(FVector(0.f, 0.f, B.G(0.f, 0.f) + 250.f), VioletLight(), 1600.f + Dm.R * 0.6f, Dm.R * 2.2f);
		const float Ang = Hash(Seed, 7) * 2.f * PI;
		const FVector Lip(FMath::Cos(Ang) * (Dm.R + 350.f), FMath::Sin(Ang) * (Dm.R + 350.f), 0.f);
		ChimeArch(B, FVector(Lip.X, Lip.Y, B.G(Lip.X, Lip.Y)), FMath::RadiansToDegrees(Ang), 0.9f, Seed);
	}

	/** A waymark chime stone every 150 m of road and tube (world-map: the Wardcarvers' ringing chime stones). Surface: a
	 *  basalt post with a bronze arm and a hanging crystal bar; underground: a bronze tripod by the wall with its bar. */
	void Chime(FBuild& B, AWRCrystalScenery* A)
	{
		const int32 Seed = A->Index * 17 + 3;
		if (!A->bUnder)
		{
			const float G0 = B.G(0.f, 0.f);
			B.K.Hex(Basalt(0.9f), 0.f, 0.f, G0 - 50.f, G0 + 240.f, 22.f, Hash(Seed, 1) * 60.f);
			B.K.Add(SaltRim(), FVector(40.f, 46.f, 6.f), FVector(0.f, 0.f, G0 + 242.f));
			B.K.Rod(Bronze(), FVector(0.f, 0.f, G0 + 225.f), FVector(70.f, 0.f, G0 + 225.f), 4.f);
			B.K.Rod(Bronze(0.8f), FVector(68.f, 0.f, G0 + 225.f), FVector(68.f, 0.f, G0 + 195.f), 1.5f);
			Point(B, AmethystPale(0.16f), FVector(68.f, 0.f, G0 + 195.f), FVector(0.f, 0.f, -1.f), 85.f, 11.f, 15.f);
			Hexafoil(B, Bronze(), FVector(-23.f, 0.f, G0 + 170.f), FVector(-1.f, 0.f, 0.f), 12.f, 2.f);
			B.Box(FVector(0.f, 0.f, G0 + 120.f), FVector(20.f, 20.f, 120.f));
			return;
		}
		// The floor is the cave's (z 0 here): a tripod, its bar, three ochre dots on the wall behind it.
		for (int32 l = 0; l < 3; ++l)
		{
			const float Ang = 2.f * PI * l / 3.f;
			B.K.Rod(Bronze(0.8f), FVector(FMath::Cos(Ang) * 55.f, FMath::Sin(Ang) * 55.f, 0.f), FVector(0.f, 0.f, 190.f), 4.f);
		}
		Point(B, AmethystPale(0.16f), FVector(0.f, 0.f, 175.f), FVector(0.f, 0.f, -1.f), 90.f, 12.f, 0.f);
		for (int32 d = 0; d < 3; ++d) B.K.Add(Ochre(), FVector(3.f, 12.f, 12.f), FVector(0.f, 90.f + d * 4.f, 160.f + d * 22.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		B.Box(FVector(0.f, 0.f, 95.f), FVector(40.f, 40.f, 95.f));
	}

	/** S1 the Watch-Cairn Line: seven basalt cairns along the Spine's foot, each with a bronze sighting ring aimed west at
	 *  the Severance; the northern three whole, the southern four torn out, their stones scattered toward the gorge and
	 *  bleached grey. */
	void Cairns(FBuild& B, AWRCrystalScenery* A)
	{
		const FData::FSite* S = FData::Get().FindSite(TEXT("S1"));
		if (!S) return;
		for (int32 c = 0; c < S->Cairns.Num(); ++c)
		{
			const FData::FCairn& C = S->Cairns[c];
			const FVector P = B.Local(C.At);
			const float G0 = B.G(P.X, P.Y);
			const FVector Aim = FRotator(0.f, C.Yaw, 0.f).RotateVector(FVector(1.f, 0.f, 0.f));
			if (!C.bTorn)
			{
				float Z = G0;
				for (int32 k = 0; k < 6; ++k)
				{
					const float W = 180.f - k * 22.f, Hh = 45.f + 10.f * Hash(c, k);
					B.K.Add(Basalt(0.85f + 0.3f * Hash(c, k + 9)), FVector(W, W * 0.9f, Hh), FVector(P.X, P.Y, Z + Hh * 0.5f), FRotator(0.f, Hash(c, k) * 40.f, 0.f));
					Z += Hh - 4.f;
				}
				B.K.Rod(Bronze(), FVector(P.X, P.Y, Z), FVector(P.X, P.Y, Z + 90.f), 6.f);
				B.K.Disc(Bronze(1.1f), FVector(P.X, P.Y, Z + 130.f), Aim, 70.f, 6.f);
				B.K.Disc(Verdigris(), FVector(P.X, P.Y, Z + 130.f), Aim, 50.f, 7.f);
				B.Box(FVector(P.X, P.Y, G0 + (Z - G0) * 0.5f), FVector(85.f, 80.f, (Z - G0) * 0.5f));
			}
			else
			{
				// Torn out: the base course left, the rest flung west toward the gorge, grey.
				B.K.Add(Basalt(), FVector(180.f, 160.f, 40.f), FVector(P.X, P.Y, G0 + 20.f));
				for (int32 k = 0; k < 7; ++k)
				{
					const FVector Q = P + Aim * (200.f + 900.f * Hash(c, k + 30)) + FVector(-Aim.Y, Aim.X, 0.f) * (Hash(c, k + 40) - 0.5f) * 500.f;
					B.Rock(k + c, FVector(90.f, 80.f, 55.f), FVector(Q.X, Q.Y, B.G(Q.X, Q.Y)), FRotator(Hash(c, k) * 40.f, Hash(c, k + 2) * 360.f, 0.f), Rgb(142, 140, 146) * 0.55f, false, 15.f);
				}
				B.K.Add(Bronze(0.6f), FVector(70.f, 6.f, 70.f), FVector(P.X, P.Y, G0 + 4.f) + Aim * 500.f, FRotator(80.f, C.Yaw, 0.f), EWRLookMesh::Cylinder);
			}
		}
	}

	/** S2 the Ringing Wreck: K1's castaways' hull lodged on a crystal reef at the geo's mouth. */
	void RingingWreck(FBuild& B, AWRCrystalScenery* A) { Wreck(B, FVector(0.f, 0.f, 30.f), 0.f, true, 41); }

	/** S3 the Dome of the Dropped Hammer: one perfect hexafoil scribed across the dome's floor, 6 m across, the carver's
	 *  bronze compass still at its centre point and the mallet dropped beside it (the keepsake: WRCrystalWorld). */
	void DroppedHammer(FBuild& B, AWRCrystalScenery* A)
	{
		const float G0 = B.G(0.f, 0.f);
		Hexafoil(B, FSurf::Stone(Rgb(216, 208, 255) * 0.55f, 0.9f), FVector(0.f, 0.f, G0 + 2.f), FVector::UpVector, 300.f, 2.f);
		B.K.Rod(Bronze(1.2f), FVector(0.f, 0.f, G0 + 3.f), FVector(150.f, 30.f, G0 + 40.f), 2.f);
		B.K.Rod(Bronze(1.2f), FVector(0.f, 0.f, G0 + 3.f), FVector(150.f, -30.f, G0 + 3.f), 2.f);
		B.K.Rod(Timber(1.2f), FVector(60.f, 90.f, G0 + 5.f), FVector(60.f, 150.f, G0 + 5.f), 5.f);
		B.K.Add(Bronze(), FVector(26.f, 14.f, 14.f), FVector(60.f, 90.f, G0 + 8.f));
	}

	/** S4 the Wax Fields: a ruined chandlery in a collapsed tube's shade among wild candle rhubarb (its plants are the
	 *  crop nodes): low walls, vats of old wax, ear-plug moulds on a bench, a fallen roof beam. */
	void WaxFields(FBuild& B, AWRCrystalScenery* A)
	{
		const float G0 = B.G(0.f, 0.f);
		const float W = 700.f, L = 500.f;
		for (int32 s = 0; s < 4; ++s)
		{
			const bool bX = s < 2;
			const float H = s == 1 ? 90.f : 180.f + 40.f * Hash(3, s);
			const FVector C = bX ? FVector(0.f, (s == 0 ? -1.f : 1.f) * L * 0.5f, 0.f) : FVector((s == 2 ? -1.f : 1.f) * W * 0.5f, 0.f, 0.f);
			B.K.Add(Dressed(0.9f), bX ? FVector(W, 50.f, H) : FVector(50.f, L, H), FVector(C.X, C.Y, B.G(C.X, C.Y, G0) + H * 0.5f - 20.f));
			B.Box(FVector(C.X, C.Y, B.G(C.X, C.Y, G0) + H * 0.5f), bX ? FVector(W * 0.5f, 25.f, H * 0.5f) : FVector(25.f, L * 0.5f, H * 0.5f));
		}
		for (int32 v = 0; v < 3; ++v)
		{
			const FVector P(-200.f + v * 200.f, -100.f, B.G(-200.f + v * 200.f, -100.f));
			B.K.Add(Dressed(0.8f), FVector(110.f, 110.f, 80.f), P + FVector(0, 0, 40.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			B.K.Add(Wax(0.9f), FVector(92.f, 92.f, 6.f), P + FVector(0, 0, 70.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		}
		B.K.Add(Timber(), FVector(220.f, 70.f, 8.f), FVector(150.f, 160.f, G0 + 80.f));
		for (int32 m = 0; m < 6; ++m) B.K.Add(Bronze(0.7f), FVector(18.f, 18.f, 10.f), FVector(70.f + m * 30.f, 160.f, G0 + 89.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		B.K.Rod(Timber(0.7f), FVector(-W * 0.5f, L * 0.5f, G0 + 160.f), FVector(W * 0.3f, -L * 0.4f, G0 + 10.f), 16.f, true);
	}

	/** S5 the Alicanto Runs: bird tracks in the basalt grit along the cliff path; at the end, at the cliff's edge, a
	 *  newcomer's bones with his lantern and pick (he followed it openly; the lore page lies with him). */
	void AlicantoRuns(FBuild& B, AWRCrystalScenery* A)
	{
		const float G0 = B.G(0.f, 0.f);
		for (int32 t = 0; t < 40; ++t)
		{
			const float X = -2400.f + t * 60.f, Y = 80.f * FMath::Sin(t * 0.4f) + (t % 2 ? 14.f : -14.f);
			const float Gz = B.G(X, Y, G0);
			for (int32 toe = -1; toe <= 1; ++toe) B.K.Add(Basalt(0.45f), FVector(12.f, 2.5f, 1.f), FVector(X + 5.f, Y + toe * 4.f, Gz + 0.5f), FRotator(0.f, toe * 25.f, 0.f));
		}
		// The bones, the pick and the lantern.
		const FLinearColor Bn = BoneC();
		B.Limb(FVector(0.f, 0.f, G0 + 12.f), FVector(80.f, 10.f, G0 + 14.f), 14.f, 12.f, Bn);
		B.Ball(FVector(100.f, 12.f, G0 + 14.f), 11.f, Bn, 0.7f);
		for (int32 r = 0; r < 6; ++r) B.Limb(FVector(20.f + r * 9.f, 0.f, G0 + 16.f), FVector(20.f + r * 9.f, 26.f, G0 + 6.f), 2.f, 1.5f, Bn, 6);
		B.K.Rod(Timber(), FVector(-40.f, 60.f, G0 + 4.f), FVector(60.f, 90.f, G0 + 4.f), 4.f);
		B.K.Add(Iron(), FVector(46.f, 8.f, 8.f), FVector(60.f, 90.f, G0 + 6.f), FRotator(0.f, 20.f, 0.f));
		B.K.Add(Bronze(0.7f), FVector(22.f, 22.f, 30.f), FVector(-60.f, -50.f, G0 + 15.f), FRotator(0.f, 0.f, 70.f), EWRLookMesh::Cylinder);
	}

	/** S6 the Chime Garden: forty wind chimes of crystal and bronze on basalt posts in a circle by the cliff, half of them
	 *  broken; the hillside sings in a gale. */
	void ChimeGarden(FBuild& B, AWRCrystalScenery* A)
	{
		for (int32 c = 0; c < 40; ++c)
		{
			const float Ang = 2.f * PI * c / 40.f, R = 1400.f + 100.f * Hash(61, c);
			WindChime(B, FVector(FMath::Cos(Ang) * R, FMath::Sin(Ang) * R, 0.f), 220.f + 60.f * Hash(62, c), c * 13 + 7, Hash(63, c) > 0.5f);
		}
		B.Lamp(FVector(0.f, 0.f, B.G(0.f, 0.f) + 300.f), LilacLight(), 800.f, 2200.f);
	}

	/** Surface kit (crystal 2.4): a resonant outcrop's rock (its node is the gathering's: WRCrystalWorld), a Druseback
	 *  grazing ledge (lichen mats on a cliff shelf), a Lumenhorn pool (still water in a basalt hollow, moss at its edge),
	 *  the vista stack (steps cut up its side, a bronze sighting ring on top). */
	void SurfaceKit(FBuild& B, AWRCrystalScenery* A)
	{
		const FData& D = FData::Get();
		if (!D.Kit.IsValidIndex(A->Index)) return;
		const FData::FKit& K = D.Kit[A->Index];
		const int32 Seed = A->Index * 19 + 2;
		const float G0 = B.G(0.f, 0.f);
		if (K.Kind == TEXT("resonant_outcrop"))
		{
			for (int32 r = 0; r < 4; ++r) B.Rock(r + Seed, FVector(220.f, 180.f, 140.f), FVector((Hash(Seed, r) - 0.5f) * 500.f, (Hash(Seed, r + 4) - 0.5f) * 500.f, G0), FRotator(0.f, Hash(Seed, r + 8) * 360.f, 0.f), Rgb(28, 28, 32) * 0.7f, true, 30.f);
		}
		else if (K.Kind == TEXT("druseback_ledge"))
		{
			for (int32 m = 0; m < 9; ++m)
			{
				const FVector P((Hash(Seed, m) - 0.5f) * 1200.f, (Hash(Seed, m + 9) - 0.5f) * 900.f, 0.f);
				B.K.Add(FSurf::Cloth(Rgb(150, 160, 110) * 0.5f), FVector(160.f + 120.f * Hash(Seed, m + 3), 120.f, 6.f), FVector(P.X, P.Y, B.G(P.X, P.Y, G0) + 2.f), FRotator(0.f, Hash(Seed, m + 5) * 180.f, 0.f), EWRLookMesh::Cylinder);
			}
		}
		else if (K.Kind == TEXT("lumenhorn_pool"))
		{
			const FVector At = B.Local(D.PoolAt);
			B.K.Add(DarkWater(), FVector(D.PoolR.X * 2.f, D.PoolR.Y * 2.f, 6.f), FVector(At.X, At.Y, At.Z - 3.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			for (int32 m = 0; m < 16; ++m)
			{
				const float Ang = 2.f * PI * m / 16.f;
				const FVector P(At.X + FMath::Cos(Ang) * D.PoolR.X * 1.05f, At.Y + FMath::Sin(Ang) * D.PoolR.Y * 1.05f, 0.f);
				B.K.Add(Grass(0.9f), FVector(130.f, 80.f, 18.f), FVector(P.X, P.Y, B.G(P.X, P.Y, At.Z) + 6.f), FRotator(0.f, Ang * 57.3f, 0.f), EWRLookMesh::Cylinder);
			}
		}
		else if (K.Kind == TEXT("vista"))
		{
			// Steps spiralling up the stack's side to its top, a sighting ring there (it fills 400 m of the map).
			const FVector Top = B.Local(D.VistaAt);
			const float Base = B.G(1600.f, 0.f, G0);
			const int32 Steps = FMath::Max(8, int32((Top.Z - Base) / 25.f));
			for (int32 s = 0; s < Steps; ++s)
			{
				const float Ang = 2.f * PI * 1.1f * s / Steps;
				const float Z = Base + (Top.Z - Base) * s / Steps;
				const FVector P(Top.X + FMath::Cos(Ang) * 1250.f, Top.Y + FMath::Sin(Ang) * 1250.f, Z);
				B.K.Add(Dressed(0.9f + 0.2f * Hash(Seed, s)), FVector(70.f, 160.f, 40.f), P, FRotator(0.f, Ang * 57.3f, 0.f));
				if (s % 2 == 0 && s + 2 < Steps)
				{
					const float A2 = 2.f * PI * 1.1f * (s + 2) / Steps;
					B.SegBox(P, FVector(Top.X + FMath::Cos(A2) * 1250.f, Top.Y + FMath::Sin(A2) * 1250.f, Base + (Top.Z - Base) * (s + 2) / Steps), 80.f, 20.f);
				}
			}
			B.K.Rod(Bronze(), Top, Top + FVector(0.f, 0.f, 130.f), 6.f);
			B.K.Disc(Bronze(1.1f), Top + FVector(0.f, 0.f, 170.f), FVector(-1.f, 0.f, 0.f), 80.f, 6.f);
		}
	}

	/** The plateau's dress in a 200 m cell (crystal 2.1, 3.5): hexagonal column tops in clusters on the basalt pavement
	 *  (the Causeway read), salt-grass and sea campion in the cracks, heather on the high ground, basalt boulders, and
	 *  sea stacks of broken columns standing in the shallows under the cliffs. From the land's own samples, seeded. */
	void Cell(FBuild& B, AWRCrystalScenery* A)
	{
		const FWRLandDef* L = FData::Land();
		if (!L || !L->bHeightfield) return;
		const int32 Seed = A->Index;
		const FVector Wc = A->GetActorLocation();
		const FVector2D Mc = L->ToMap(Wc);
		int32 Made = 0;
		const int32 Budget = int32(WRTune(TEXT("crystalWorld.cellBudget"), 160.f));
		for (int32 q = 0; q < 140 && Made < Budget; ++q)
		{
			const FVector2D M = Mc + FVector2D((Hash(Seed, q) - 0.5f) * 200.f, (Hash(Seed, q + 500) - 0.5f) * 200.f);
			const int32 I = FMath::RoundToInt((M.X - L->HfOrigin.X) / L->HfStep), J = FMath::RoundToInt((M.Y - L->HfOrigin.Y) / L->HfStep);
			const uint8 Cls = L->HfSurfaceAt(I, J);
			if (Cls == FWRLandDef::HfHidden) continue;
			const float H = L->Height(M);
			const FVector P = B.Local(L->ToWorld(M, H));
			const float N1 = Hash(Seed, q + 900);
			if (Cls == 1 && N1 < 0.22f)
			{
				// A cluster of 7-30 column tops standing 10-60 cm proud, salt-white weathering on their rims.
				const int32 Cn = 7 + int32(Hash(Seed, q + 1300) * 23.f);
				const float Cr = 40.f + 25.f * Hash(Seed, q + 1500);
				for (int32 c = 0; c < Cn && Made < Budget; ++c, ++Made)
				{
					const float Ang = c * 2.39996f, Rr = Cr * 1.8f * FMath::Sqrt(float(c));
					const float X = P.X + FMath::Cos(Ang) * Rr, Y = P.Y + FMath::Sin(Ang) * Rr;
					const float Gz = B.G(X, Y, P.Z);
					const float Proud = 10.f + 50.f * Hash(Seed, q * 31 + c);
					B.K.Hex(Basalt(0.8f + 0.35f * Hash(Seed, q * 17 + c)), X, Y, Gz - 30.f, Gz + Proud, Cr * (0.85f + 0.3f * Hash(Seed, c + q)), Hash(Seed, q) * 60.f + c * 4.f);
					if (Hash(Seed, q * 7 + c) > 0.6f) B.K.Add(SaltRim(), FVector(Cr * 1.2f, Cr * 1.4f, 2.f), FVector(X, Y, Gz + Proud + 1.f), FRotator(0.f, Hash(Seed, q) * 60.f + c * 4.f, 0.f));
				}
			}
			else if (Cls == 2 && N1 < 0.5f)
			{
				// Salt-grass tufts, sea campion's white heads among them.
				for (int32 t = 0; t < 5 && Made < Budget; ++t, ++Made)
				{
					const float X = P.X + (Hash(Seed, q * 5 + t) - 0.5f) * 220.f, Y = P.Y + (Hash(Seed, q * 5 + t + 77) - 0.5f) * 220.f;
					const float Gz = B.G(X, Y, P.Z);
					B.K.Add(Grass(0.8f + 0.35f * Hash(Seed, t)), FVector(18.f, 18.f, 30.f + 30.f * Hash(Seed, q + t)), FVector(X, Y, Gz + 15.f), FRotator(Hash(Seed, t) * 20.f - 10.f, 0.f, Hash(Seed, t + 1) * 20.f - 10.f), EWRLookMesh::Cone);
					if (t == 0) B.K.Add(Campion(), FVector(10.f, 10.f, 6.f), FVector(X + 15.f, Y, Gz + 28.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
				}
			}
			else if (Cls == 3 && N1 < 0.4f)
			{
				for (int32 t = 0; t < 3 && Made < Budget; ++t, ++Made)
				{
					const float X = P.X + (Hash(Seed, q * 3 + t) - 0.5f) * 260.f, Y = P.Y + (Hash(Seed, q * 3 + t + 33) - 0.5f) * 260.f;
					B.K.Add(Heather(0.8f + 0.3f * Hash(Seed, t)), FVector(110.f, 90.f, 28.f), FVector(X, Y, B.G(X, Y, P.Z) + 8.f), FRotator(0.f, Hash(Seed, t) * 180.f, 0.f), EWRLookMesh::Cylinder);
				}
			}
			else if ((Cls == 4 || Cls == 9) && H < 3.f && H > -8.f && N1 < 0.25f)
			{
				// A sea stack: broken columns 2-12 m standing in the shallows under the cliff.
				const int32 Cn = 3 + int32(Hash(Seed, q + 2000) * 5.f);
				for (int32 c = 0; c < Cn && Made < Budget; ++c, ++Made)
				{
					const float Ang = c * 2.39996f;
					const float X = P.X + FMath::Cos(Ang) * 120.f * FMath::Sqrt(float(c)), Y = P.Y + FMath::Sin(Ang) * 120.f * FMath::Sqrt(float(c));
					const float Tall = 200.f + 1000.f * Hash(Seed, q * 13 + c);
					B.K.Hex(Basalt(0.7f + 0.3f * Hash(Seed, c)), X, Y, H * 100.f - 600.f + P.Z - H * 100.f, P.Z + Tall, 60.f + 20.f * Hash(Seed, c + 3), Hash(Seed, q) * 60.f);
				}
				B.Box(P + FVector(0, 0, 300.f), FVector(180.f, 180.f, 600.f));
			}
			else if (Cls == 1 && N1 > 0.93f)
			{
				B.Rock(q, FVector(150.f + 150.f * Hash(Seed, q + 3), 130.f, 90.f + 70.f * Hash(Seed, q + 4)), P, FRotator(0.f, Hash(Seed, q + 5) * 360.f, 0.f), Rgb(30, 30, 34) * 0.7f, true, 25.f);
				++Made;
			}
		}
	}
}

namespace WRCrystalUnder
{
	bool Build(WRCrystalLook::FBuild& B, AWRCrystalScenery* A);
}

void AWRCrystalScenery::BuildCrystal()
{
	using namespace WRCrystalSurf;
	const FName K = Kind;
	const bool bLandmark = K == TEXT("prism_crown") || K == TEXT("giantess") || K == TEXT("split_geode");
	const float Cull = bLandmark ? 0.f
		: bUnder ? WRTune(TEXT("crystalWorld.underCull"), 20000.f)
		: K == TEXT("cell") || K == TEXT("chime") ? WRTune(TEXT("crystalWorld.decorCull"), 26000.f)
		: WRTune(TEXT("crystalWorld.siteCull"), 60000.f);
	FBuild B(this, Cull);
	bool bBuilt = true;
	if (K == TEXT("camp")) Camp(B, this);
	else if (K == TEXT("prism_crown")) PrismCrown(B, this);
	else if (K == TEXT("giantess")) Giantess(B, this);
	else if (K == TEXT("split_geode")) SplitGeode(B, this);
	else if (K == TEXT("descent")) Descent(B, this);
	else if (K == TEXT("vent")) Vent(B, this);
	else if (K == TEXT("dome")) Dome(B, this);
	else if (K == TEXT("chime")) Chime(B, this);
	else if (K == TEXT("watch_cairns")) Cairns(B, this);
	else if (K == TEXT("ringing_wreck")) RingingWreck(B, this);
	else if (K == TEXT("dropped_hammer")) DroppedHammer(B, this);
	else if (K == TEXT("wax_fields")) WaxFields(B, this);
	else if (K == TEXT("alicanto_runs")) AlicantoRuns(B, this);
	else if (K == TEXT("chime_garden")) ChimeGarden(B, this);
	else if (K == TEXT("cell")) Cell(B, this);
	else if (!bUnder && (K == TEXT("resonant_outcrop") || K == TEXT("druseback_ledge") || K == TEXT("lumenhorn_pool") || K == TEXT("vista"))) SurfaceKit(B, this);
	else bBuilt = WRCrystalUnder::Build(B, this);
	if (!bBuilt)
	{
		UE_LOG(LogWildreach, Warning, TEXT("CRYSTAL scenery: no builder for kind %s (%s)"), *K.ToString(), *SiteId.ToString());
		return;
	}
	B.Finish(FLinearColor(0.3f, 0.28f, 0.3f));
	if (Cull > 0.f && GetNetMode() != NM_DedicatedServer)
	{
		for (UActorComponent* Cmp : GetComponents())
		{
			if (UInstancedStaticMeshComponent* I = Cast<UInstancedStaticMeshComponent>(Cmp)) I->SetCullDistances(0, int32(Cull));
		}
	}
}
