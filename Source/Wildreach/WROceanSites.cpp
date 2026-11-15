// The Ocean's hand-made sites and kit POIs (cooks/biome-ocean.md 2.4 tables; cooks/build-biome-ocean.md 2.2), each built
// to tell its part of the coast's story with no words (cook 2.20), on the forest's FBuild.
#include "WROceanSceneryParts.h"
#include "Wildreach.h"
#include "WRTide.h"
#include "WRTuning.h"

namespace WROceanParts
{
	namespace
	{
		float Gz(FBuild& B, float X, float Y) { return B.G(X, Y, 0.f); }
		/** A row of rakes leaning on a frame (the salt pans, the kelp pits). */
		void Rakes(FBuild& B, const FVector& At, float Yaw, int32 N)
		{
			const FRotator R(0.f, Yaw, 0.f);
			B.K.Rod(OcDrift(0.8f), At + R.RotateVector(FVector(0, -N * 25.f, 110.f)), At + R.RotateVector(FVector(0, N * 25.f, 110.f)), 6.f);
			for (int32 k = 0; k < N; ++k)
			{
				const FVector Foot = At + R.RotateVector(FVector(70.f, -N * 25.f + 25.f + k * 50.f, 0.f));
				const FVector Top = At + R.RotateVector(FVector(0.f, -N * 25.f + 25.f + k * 50.f, 120.f));
				B.K.Rod(OcDrift(0.9f), Foot, Top, 4.f);
				B.K.Add(OcTimber(0.6f), FVector(10.f, 50.f, 12.f), Foot + FVector(0, 0, 6.f), R + FRotator(0.f, 90.f, 0.f));
			}
		}
		/** A Saltkin salt pan: a shallow rectangular bed edged in stone, white crust, a sluice board at its lower end. */
		void SaltPan(FBuild& B, const FVector& At, float Yaw, float W, float D)
		{
			const FRotator R(0.f, Yaw, 0.f);
			for (int32 s = 0; s < 4; ++s)
			{
				const bool bX = s % 2 == 0;
				const FVector E = R.RotateVector(bX ? FVector((s == 0 ? -1 : 1) * W * 0.5f, 0, 10.f) : FVector(0, (s == 1 ? -1 : 1) * D * 0.5f, 10.f));
				B.K.Add(OcOchre(0.95f), FVector(bX ? 30.f : W, bX ? D : 30.f, 24.f), At + E, R);
			}
			B.K.Add(OcSalt(), FVector(W - 30.f, D - 30.f, 4.f), At + FVector(0, 0, 2.f), R);
			B.K.Add(OcTimber(0.6f), FVector(8.f, 80.f, 30.f), At + R.RotateVector(FVector(W * 0.5f + 6.f, 0, 16.f)), R);
		}
		/** A harpoon rack (a frame of two posts and two rails, harpoons standing in it, points one way). */
		void HarpoonRack(FBuild& B, const FVector& At, float Yaw, float PointYaw, int32 N, bool bRust)
		{
			const FRotator R(0.f, Yaw, 0.f);
			for (int32 s = -1; s <= 1; s += 2) B.K.Rod(OcTimber(0.6f), At + R.RotateVector(FVector(0, s * N * 20.f, 0)), At + R.RotateVector(FVector(0, s * N * 20.f, 200.f)), 9.f);
			B.K.Rod(OcTimber(0.6f), At + R.RotateVector(FVector(0, -N * 20.f, 60.f)), At + R.RotateVector(FVector(0, N * 20.f, 60.f)), 6.f);
			B.K.Rod(OcTimber(0.6f), At + R.RotateVector(FVector(0, -N * 20.f, 170.f)), At + R.RotateVector(FVector(0, N * 20.f, 170.f)), 6.f);
			const FVector Lean = FRotator(-70.f, PointYaw, 0.f).Vector();
			for (int32 k = 0; k < N; ++k) OcHarpoon(B, At + R.RotateVector(FVector(-30.f, -N * 20.f + 20.f + k * 40.f, 0.f)), Lean, 240.f, bRust);
		}
		/** A pool on the shelf: a dished rim of weed-dark rock round still water, anemones and urchins in it. */
		void TidePool(FBuild& B, const FVector& At, float R, int32 Seed)
		{
			for (int32 s = 0; s < 10; ++s)
			{
				const FRotator Rr(0.f, s * 36.f + Seed * 7.f, 0.f);
				B.K.Add(FSurf::Stone(OcSRGB(0x2A, 0x2A, 0x28) * 0.55f, 0.4f), FVector(R * 0.5f, R * 0.7f, 30.f), At + Rr.RotateVector(FVector(R, 0, 0)), Rr);
			}
			B.K.Add(FSurf::Glaze(OcSRGB(0x3E, 0x8C, 0x7A) * 0.45f, 0.05f), FVector(R * 1.8f, R * 1.8f, 4.f), At + FVector(0, 0, -8.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			for (int32 a = 0; a < 5; ++a)
			{
				const FVector P = At + FRotator(0.f, a * 72.f + Seed, 0.f).RotateVector(FVector(R * 0.6f, 0, -10.f));
				B.K.Add(FSurf::Cloth(OcSRGB(0xE8, 0x7A, 0x6A) * 0.55f), FVector(14.f, 14.f, 12.f), P, FRotator::ZeroRotator, EWRLookMesh::Cylinder);
				for (int32 t = 0; t < 6; ++t) B.K.Spike(FSurf::Cloth(OcSRGB(0xF0, 0xA8, 0x98) * 0.55f), P + FVector(0, 0, 6.f), FRotator(50.f, t * 60.f, 0.f).Vector(), 2.f, 10.f);
			}
		}
	}

	bool Site(FOcCtx& C)
	{
		FBuild& B = C.B;
		const FName K = C.A->Kind;
		const float G = Gz(B, 0, 0);
		const float Sea = C.SeaZ();
		if (K == TEXT("tsunami_line"))
		{
			// The line's story beat (cook 2.20 #6): below it, wave-scoured house floors and a boat dropped in a terrace by
			// the sea; above it, whole houses (the Upper Town). The stones themselves are their own pieces.
			for (int32 h = 0; h < 4; ++h) OcSaltkinHouse(B, FVector(600.f + h * 900.f, -600.f + (h % 2) * 1100.f, Gz(B, 600.f + h * 900.f, -600.f + (h % 2) * 1100.f)), 90.f + h * 7.f, false, h);
			OcHull(B, FVector(1800.f, 500.f, Gz(B, 1800.f, 500.f)), 60.f, 900.f, 35.f, 0.55f, OcTimber(0.6f));
			return true;
		}
		if (K == TEXT("upper_town"))
		{
			// Terraced stone houses above the line, every door to the sea, lookout ledges, nets on racks, salt jars, shell
			// roofs; a meal still set on a table outside one door.
			for (int32 h = 0; h < 9; ++h)
			{
				const float X = (h % 3) * 1100.f - 1100.f, Y = (h / 3) * 1200.f - 1200.f;
				OcSaltkinHouse(B, FVector(X, Y, Gz(B, X, Y)), 0.f, true, h);
				if (h % 2 == 0)
				{
					const FVector Rack(X + 600.f, Y + 400.f, Gz(B, X + 600.f, Y + 400.f));
					for (int32 s = -1; s <= 1; s += 2) B.K.Rod(OcDrift(), Rack + FVector(0, s * 150.f, 0), Rack + FVector(0, s * 150.f, 200.f), 6.f);
					B.K.Rod(OcDrift(), Rack + FVector(0, -160.f, 190.f), Rack + FVector(0, 160.f, 190.f), 5.f);
					B.K.Add(OcNet(), FVector(6.f, 300.f, 160.f), Rack + FVector(0, 0, 110.f));
				}
				for (int32 j = 0; j < 3; ++j) B.K.Add(OcClay(), FVector(40.f, 40.f, 60.f), FVector(X - 380.f + j * 50.f, Y + 300.f, Gz(B, X, Y) + 30.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			}
			const FVector Tb(0.f, 700.f, Gz(B, 0.f, 700.f));
			B.K.Add(OcTimber(0.5f), FVector(200.f, 90.f, 10.f), Tb + FVector(0, 0, 80.f));
			for (int32 l = 0; l < 4; ++l) B.K.Add(OcTimber(0.45f), FVector(10.f, 10.f, 80.f), Tb + FVector((l % 2) * 180.f - 90.f, (l / 2) * 70.f - 35.f, 40.f));
			for (int32 b = 0; b < 3; ++b) { B.K.Add(OcClay(), FVector(30.f, 30.f, 10.f), Tb + FVector(-60.f + b * 60.f, 0, 90.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder); B.K.Add(OcBone(), FVector(14.f, 4.f, 3.f), Tb + FVector(-60.f + b * 60.f, 0, 97.f)); }
			B.Box(Tb + FVector(0, 0, 45.f), FVector(100.f, 45.f, 45.f));
			return true;
		}
		if (K == TEXT("salt_pans"))
		{
			// Forty terraced evaporation pans stepping down the lower benches, wooden rakes leaning in rows, a sluice board
			// on each (the brine let down from pan to pan).
			for (int32 i = 0; i < 40; ++i)
			{
				const float X = -800.f + (i % 5) * 400.f, Y = -1600.f + (i / 5) * 420.f;
				SaltPan(B, FVector(X, Y, Gz(B, X, Y) + 4.f), 0.f, 360.f, 380.f);
			}
			Rakes(B, FVector(1300.f, -400.f, Gz(B, 1300.f, -400.f)), 90.f, 8);
			Rakes(B, FVector(1300.f, 800.f, Gz(B, 1300.f, 800.f)), 90.f, 6);
			return true;
		}
		if (K == TEXT("harpooners_hall"))
		{
			// A Saltkin longhouse (24 x 9 m): stone walls, a turf-on-timber roof, a wall of harpoon racks inside, every
			// point turned to the Pound (east-north-east), a frieze round the beams of a whale-serpent with men on its back and
			// chains in its flesh, tally scratches in their hundreds by the door.
			const float L = 2400.f, W = 900.f, H = 330.f;
			for (int32 sd = 0; sd < 4; ++sd)
			{
				const bool bLong = sd % 2 == 0;
				const FVector A = sd == 0 ? FVector(-L * 0.5f, -W * 0.5f, G) : sd == 1 ? FVector(L * 0.5f, -W * 0.5f, G) : sd == 2 ? FVector(L * 0.5f, W * 0.5f, G) : FVector(-L * 0.5f, W * 0.5f, G);
				const FVector Bt = sd == 0 ? FVector(L * 0.5f, -W * 0.5f, G) : sd == 1 ? FVector(L * 0.5f, W * 0.5f, G) : sd == 2 ? FVector(-L * 0.5f, W * 0.5f, G) : FVector(-L * 0.5f, -W * 0.5f, G);
				if (sd == 1)
				{
					// The door end (east): two wall pieces either side of a 2 m door.
					OcCoursedWall(B, A, A + FVector(0, W * 0.5f - 100.f, 0), H, 70.f, 100.f, 55.f);
					OcCoursedWall(B, Bt - FVector(0, W * 0.5f - 100.f, 0), Bt, H, 70.f, 100.f, 55.f);
					continue;
				}
				OcCoursedWall(B, A, Bt, H, 70.f, 100.f, 55.f);
				(void)bLong;
			}
			for (int32 s = -1; s <= 1; s += 2) B.K.Add(FSurf::Cloth(OcSRGB(0x6E, 0x7A, 0x4A) * 0.45f), FVector(L + 120.f, W * 0.62f, 30.f), FVector(0, s * W * 0.27f, G + H + 160.f), FRotator(0.f, 0.f, s * 28.f));
			B.Box(FVector(0, 0, G + H + 150.f), FVector(L * 0.5f + 60.f, W * 0.5f + 40.f, 30.f));
			for (int32 bm = 0; bm < 9; ++bm)
			{
				const float X = -L * 0.5f + 150.f + bm * 260.f;
				B.K.Add(OcTimber(0.5f), FVector(30.f, W, 30.f), FVector(X, 0, G + H));
				// The frieze carved along the beam: the beast's coils, a little man standing on it, a chain.
				for (int32 c = 0; c < 4; ++c) B.K.Ring(OcTimber(0.35f), FVector(X + 16.f, -W * 0.35f + c * W * 0.22f, G + H - 10.f), 22.f, 6.f, 4.f, 5, 180.f, 360.f, 0.5f);
			}
			for (int32 r = 0; r < 4; ++r) HarpoonRack(B, FVector(-L * 0.5f + 300.f + r * 520.f, -W * 0.5f + 60.f, G), 0.f, 60.f, 8, r % 2 == 1);
			for (int32 t = 0; t < 30; ++t) B.K.Add(FSurf::Stone(OcSRGB(0x5A, 0x4A, 0x3A) * 0.5f, 0.9f), FVector(3.f, 2.f, 20.f), FVector(L * 0.5f + 36.f, W * 0.5f - 80.f - (t % 10) * 8.f, G + 120.f + (t / 10) * 26.f), FRotator(0.f, 0.f, (t % 5 == 4) ? 60.f : 0.f));
			return true;
		}
		if (K == TEXT("chain_anchors"))
		{
			// Four bronze bollards 4 m tall on the Pound's rim, green with age, each with a broken chain link the size of a
			// door running over the wall's parapet (on a bronze saddle) into the lagoon. The breaks are fresh-bright inside.
			const float R = C.D.PoundRadius() + 500.f + 230.f;
			for (int32 b = 0; b < 4; ++b)
			{
				const float A = b / 4.f * 360.f + 23.f;
				const FRotator Ra(0.f, A, 0.f);
				const FVector Bp = Ra.RotateVector(FVector(R, 0, 0));
				OcBollard(B, FVector(Bp.X, Bp.Y, Gz(B, Bp.X, Bp.Y)), A, 1.f);
				const FVector Eye = Ra.RotateVector(FVector(R - 62.f, 0, 0)) + FVector(0, 0, Gz(B, Bp.X, Bp.Y) + 330.f);
				const float WallTopZ = C.D.PoundAt().Z - float(C.A->GetActorLocation().Z) + C.D.ArenaNum(TEXT("thalassor"), TEXT("wallTop"), 30.f) * 100.f;
				const FVector Saddle = Ra.RotateVector(FVector(C.D.PoundRadius() + 470.f, 0, 0)) + FVector(0, 0, WallTopZ + 120.f);
				B.K.Add(OcVerdigris(), FVector(140.f, 160.f, 40.f), Saddle - FVector(0, 0, 30.f), Ra);
				OcChain(B, Eye, Saddle, 110.f, 40.f, OcVerdigris());
				const FVector Over = Ra.RotateVector(FVector(C.D.PoundRadius() - 150.f, 0, 0)) + FVector(0, 0, WallTopZ - 600.f);
				OcChain(B, Saddle, Over, 110.f, 0.f, OcVerdigris());
				B.K.Add(OcBright(), FVector(40.f, 70.f, 20.f), Over + FVector(0, 0, -20.f), Ra);           // the fresh-bright break
			}
			return true;
		}
		if (K == TEXT("sunwatch_mirrors"))
		{
			// A Sunwatch war camp at Saltgate's east mouth: sun-disc standards, a burnt tent's frame, and three bronze signal
			// mirrors on tripods all aimed at the Pound (the sun people fought the beast with sunlight: the clue).
			const FVector Pound = C.L(C.D.PoundAt());
			for (int32 m = 0; m < 3; ++m)
			{
				const FVector Tp((m - 1) * 900.f, -600.f + (m % 2) * 400.f, 0.f);
				const FVector Base(Tp.X, Tp.Y, Gz(B, Tp.X, Tp.Y));
				for (int32 l = 0; l < 3; ++l) B.K.Rod(OcCedar(), Base + FRotator(0.f, l * 120.f, 0.f).RotateVector(FVector(90.f, 0, 0)), Base + FVector(0, 0, 190.f), 7.f);
				const FVector Aim = (Pound - (Base + FVector(0, 0, 210.f))).GetSafeNormal();
				B.K.Disc(OcBronze(), Base + FVector(0, 0, 220.f), Aim, 140.f, 8.f);
				B.K.Disc(OcGold(), Base + FVector(0, 0, 220.f) + Aim * 5.f, Aim, 120.f, 2.f);
			}
			for (int32 s = 0; s < 2; ++s)
			{
				const FVector Sp(-1500.f + s * 3000.f, 600.f, Gz(B, -1500.f + s * 3000.f, 600.f));
				B.K.Rod(OcCedar(), Sp, Sp + FVector(0, 0, 420.f), 8.f);
				B.K.Disc(OcGold(), Sp + FVector(0, 0, 440.f), FVector(1, 0, 0), 90.f, 6.f);
				for (int32 r = 0; r < 12; ++r) B.K.Rod(OcGold(), Sp + FVector(0, 0, 440.f), Sp + FVector(0, 0, 440.f) + FRotator(0.f, 0.f, r * 30.f).RotateVector(FVector(0, 70.f, 0)), 3.f);
			}
			// The burnt tent: its frame, the charred cloth in rags, a cold fire.
			const FVector Tn(300.f, 1100.f, Gz(B, 300.f, 1100.f));
			for (int32 p = 0; p < 4; ++p) B.K.Rod(OcTar(), Tn + FVector((p % 2) * 400.f - 200.f, (p / 2) * 300.f - 150.f, 0), Tn + FVector((p % 2) * 400.f - 200.f, (p / 2) * 300.f - 150.f, 240.f - 60.f * (p == 3)), 7.f);
			B.K.Add(FSurf::Cloth(FLinearColor(0.02f, 0.018f, 0.015f)), FVector(380.f, 160.f, 6.f), Tn + FVector(0, -40.f, 220.f), FRotator(0.f, 0.f, 18.f));
			return true;
		}
		if (K == TEXT("sluiceway_bridge"))
		{
			// A Saltkin stone bridge over the Sluiceway: two abutments, two piers in the channel, three spans with the
			// middle one fallen into the water (its blocks in the channel bed). Tidecoils nest in the piers' gaps.
			const float Deck = G + 400.f;
			for (int32 p = -2; p <= 2; ++p)
			{
				const float Y = p * 700.f;
				const float Gp = Gz(B, 0.f, Y);
				const float Hh = Deck - Gp;
				B.K.Add(OcLime(0.9f), FVector(400.f, FMath::Abs(p) == 2 ? 300.f : 200.f, Hh), FVector(0, Y, Gp + Hh * 0.5f));
				B.Box(FVector(0, Y, Gp + Hh * 0.5f), FVector(200.f, FMath::Abs(p) == 2 ? 150.f : 100.f, Hh * 0.5f));
				if (p < 2 && p != -1 && p != 0)
				{
					B.K.Add(OcLime(1.0f), FVector(400.f, 700.f, 50.f), FVector(0, Y + 350.f, Deck));
					B.Box(FVector(0, Y + 350.f, Deck), FVector(200.f, 350.f, 25.f));
				}
				if (FMath::Abs(p) <= 1) for (int32 h = 0; h < 3; ++h) B.K.Disc(FSurf::Stone(FLinearColor(0.01f, 0.01f, 0.01f), 0.9f), FVector(205.f, Y + (h - 1) * 50.f, Sea - 120.f + h * 40.f), FVector(1, 0, 0), 40.f, 4.f);
			}
			B.K.Add(OcLime(1.0f), FVector(400.f, 700.f, 50.f), FVector(0, -350.f, Deck));
			B.Box(FVector(0, -350.f, Deck), FVector(200.f, 350.f, 25.f));
			// The fallen middle span's blocks in the channel, nails rusting in a split beam among them.
			for (int32 f = 0; f < 6; ++f) B.K.Add(OcLime(0.8f), FVector(160.f, 120.f, 90.f), FVector((f - 2.5f) * 60.f, 350.f + (f % 3) * 120.f, Gz(B, 0.f, 350.f) + 45.f), FRotator(0.f, f * 35.f, f * 9.f));
			B.K.Add(OcTimber(0.5f), FVector(500.f, 30.f, 30.f), FVector(80.f, 420.f, Gz(B, 0.f, 420.f) + 100.f), FRotator(12.f, 70.f, 0.f));
			return true;
		}
		if (K == TEXT("barnacle_cove"))
		{
			// A rocky cove 70 m wide where a newcomer longboat hull sits upright on the sand like a shell, barnacle-
			// crusted with claw gouges round it (Old Barnacle's lair; the hull he wears is his own). On the beach the two
			// spare "shells" lie in plain view from the cove's mouth: a Saltkin bronze bell on its side and a Driftback
			// carapace. The bell and carapace are the boss's own props in the fight (built here at rest).
			OcHull(B, FVector(0, 0, G), 30.f, 1100.f, 0.f, 0.15f, OcTimber(0.55f));
			for (int32 b = 0; b < 30; ++b) B.K.Add(OcBarnacle(), FVector(40.f, 30.f, 20.f), FVector(-500.f + b * 34.f, (b % 2 ? 1 : -1) * 130.f, G + 60.f + (b % 3) * 30.f), FRotator(0.f, b * 21.f, 0.f), EWRLookMesh::Cone);
			for (int32 g = 0; g < 8; ++g) B.K.Add(FSurf::Stone(OcSRGB(0x5A, 0x50, 0x40) * 0.5f, 0.8f), FVector(160.f, 12.f, 6.f), FRotator(0.f, g * 45.f, 0.f).RotateVector(FVector(800.f, 0, G + 2.f)), FRotator(0.f, g * 45.f + 20.f, 0.f));
			for (const auto& R : { FVector(-2600.f, -800.f, 0.f), FVector(2200.f, 1800.f, 0.f), FVector(-600.f, -2800.f, 0.f) }) B.Rock(int32(R.X) & 7, FVector(600.f, 500.f, 380.f), FVector(R.X, R.Y, Gz(B, R.X, R.Y)), FRotator(0.f, R.Y * 0.01f, 0.f), OcRockTint() * 0.6f, true);
			return true;
		}
		if (K == TEXT("ladle_rack"))
		{
			// A fishermen's shore charm: a rope between two driftwood posts hung with thirty bronze ladles, each with its
			// bottom knocked out (the funayurei ask for a ladle to sink you: give them one that holds no water).
			const FVector P0(0, -600.f, Gz(B, 0, -600.f)), P1(0, 600.f, Gz(B, 0, 600.f));
			B.Limb(P0 - FVector(0, 0, 30.f), P0 + FVector(0, 0, 260.f), 12.f, 9.f, OcSRGB(0xA8, 0xA3, 0x9A) * 0.5f, 7);
			B.Limb(P1 - FVector(0, 0, 30.f), P1 + FVector(0, 0, 260.f), 12.f, 9.f, OcSRGB(0xA8, 0xA3, 0x9A) * 0.5f, 7);
			B.Line(P0 + FVector(0, 0, 250.f), P1 + FVector(0, 0, 250.f), 2.f, 50.f, OcSRGB(0x8C, 0x7A, 0x5A) * 0.45f, 16);
			for (int32 l = 0; l < 30; ++l)
			{
				const float T = (l + 0.5f) / 30.f;
				const FVector H = Sag(P0 + FVector(0, 0, 250.f), P1 + FVector(0, 0, 250.f), T, 50.f);
				B.K.Rod(OcNet(), H, H - FVector(0, 0, 30.f + 20.f * B.Rand(l)), 1.f);
				const FVector Bowl = H - FVector(0, 0, 45.f + 20.f * B.Rand(l));
				B.K.Ring(OcVerdigris(), Bowl, 9.f, 4.f, 8.f, 8);
				B.K.Rod(OcVerdigris(), Bowl + FVector(0, 0, 10.f), Bowl + FVector(0, 0, 40.f), 2.5f);
			}
			B.Loop(TEXT("ocean.ladles_clink"), FVector(0, 0, G + 200.f));
			return true;
		}
		if (K == TEXT("gullery"))
		{
			// A seabird cliff on the Sky border, white with guano: burrow mouths in rows, Gullraider nests on the ledges
			// lined with stolen things (rings, a compass, a child's carved boat).
			for (int32 l = 0; l < 5; ++l)
			{
				const float Z = G + 300.f + l * 400.f;
				B.K.Add(OcLime(0.8f), FVector(220.f, 2400.f, 40.f), FVector(-200.f + l * 30.f, 0, Z));
				B.K.Add(FSurf::Stone(OcSRGB(0xF2, 0xEF, 0xE6) * 0.8f, 0.9f), FVector(30.f, 2300.f, 260.f), FVector(-90.f + l * 30.f, 0, Z - 140.f));
				for (int32 b = 0; b < 10; ++b) B.K.Disc(FSurf::Stone(FLinearColor(0.012f, 0.01f, 0.01f), 0.9f), FVector(-75.f + l * 30.f, -1100.f + b * 240.f, Z + 60.f), FVector(1, 0, 0), 45.f, 4.f);
			}
			const FVector Nest(-140.f, 300.f, G + 1540.f);
			B.Ball(Nest, 60.f, OcSRGB(0x6E, 0x5E, 0x4A) * 0.5f, 0.95f, 0.35f);
			B.K.Disc(OcGold(), Nest + FVector(10.f, 0, 22.f), FVector::UpVector, 6.f, 1.5f);
			B.K.Disc(OcBronze(), Nest + FVector(-14.f, 12.f, 22.f), FVector::UpVector, 12.f, 3.f);
			B.K.Add(OcTimber(0.9f), FVector(26.f, 8.f, 6.f), Nest + FVector(0, -20.f, 24.f));
			B.Loop(TEXT("ocean.gull_colony"), FVector(0, 0, G + 1200.f), FRotator::ZeroRotator, 1.5f);
			return true;
		}
		if (K == TEXT("tide_clock"))
		{
			// A Saltkin tide gauge: a 6 m stone pillar standing in a shelf pool, notched every 30 cm, the King Tide's notch
			// cut deep with a wave sign; a cedar float in a slot cut down its face rides the real tide (Float).
			const FVector Base(0, 0, G - 200.f);
			for (int32 c = 0; c < 16; ++c) B.K.Add(OcLime(0.95f + 0.06f * B.Rand(c)), FVector(70.f, 70.f, 50.f), Base + FVector(0, 0, 25.f + c * 52.f));
			B.Box(Base + FVector(0, 0, 420.f), FVector(35.f, 35.f, 420.f));
			const float MeanL = WRTide::MeanZ() - float(C.A->GetActorLocation().Z);
			for (int32 n = -6; n <= 8; ++n)
			{
				const bool bKing = n == -20;
				B.K.Add(OcLimeDark(), FVector(6.f, n % 4 == 0 ? 40.f : 22.f, 3.f), FVector(37.f, 0, MeanL + n * 30.f));
				(void)bKing;
			}
			// The King Tide's notch (6 m below mean): deep-cut, a wave over it.
			B.K.Add(OcLimeDark(), FVector(14.f, 60.f, 8.f), FVector(37.f, 0, MeanL - 600.f));
			B.K.Ring(OcLimeDark(), FVector(38.f, 0, MeanL - 560.f), 12.f, 4.f, 4.f, 6, 0.f, 180.f, 0.5f);
			B.K.Add(FSurf::Stone(FLinearColor(0.02f, 0.02f, 0.02f), 0.9f), FVector(10.f, 26.f, 700.f), FVector(37.f, 0, MeanL));   // the slot
			if (C.A->GetNetMode() != NM_DedicatedServer)
			{
				USceneComponent* F = NewObject<USceneComponent>(C.A);
				F->SetupAttachment(C.A->GetRootComponent());
				F->SetRelativeLocation(FVector(44.f, 0, MeanL));
				F->RegisterComponent();
				TArray<FWRLookBlock> Fl;
				FKit FK(FTransform::Identity, Fl);
				FK.Add(OcCedar(), FVector(28.f, 22.f, 40.f), FVector(0, 0, 0));
				FK.Add(OcCedar(), FVector(10.f, 10.f, 40.f), FVector(-6.f, 0, 40.f));
				FK.Add(FSurf::Cloth(OcSRGB(0xD8, 0x50, 0x2A) * 0.6f), FVector(12.f, 4.f, 10.f), FVector(-6.f, 0, 64.f));
				FWRBuildLook::Spawn(C.A, F, Fl, FLinearColor::Black, false);
				C.A->Float = F;
				C.A->FloatBaseZ = MeanL;
			}
			TidePool(B, FVector(0, 0, G - 30.f), 450.f, 3);
			return true;
		}
		if (K == TEXT("turtle_beach"))
		{
			// A gentle beach scored with flipper tracks; fenced Saltkin egg pits, the posts carved with turtles;
			// Spinecrab burrows ring the pits.
			for (int32 t = 0; t < 6; ++t)
			{
				const float Y = -900.f + t * 360.f;
				for (int32 s = 0; s < 14; ++s) for (int32 f = -1; f <= 1; f += 2) B.K.Add(FSurf::Stone(OcSRGB(0xCF, 0xC7, 0xB5) * 0.55f, 0.95f), FVector(40.f, 14.f, 2.f), FVector(-700.f + s * 90.f, Y + f * 60.f, Gz(B, -700.f + s * 90.f, Y) + 1.f), FRotator(0.f, f * 35.f, 0.f));
			}
			for (int32 p = 0; p < 4; ++p)
			{
				const FVector Pc(200.f + (p % 2) * 600.f, -500.f + (p / 2) * 1000.f, 0.f);
				const float Gp = Gz(B, Pc.X, Pc.Y);
				B.K.Add(FSurf::Stone(OcSRGB(0xD9, 0xCD, 0xB4) * 0.5f, 0.95f), FVector(240.f, 240.f, 10.f), FVector(Pc.X, Pc.Y, Gp - 4.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
				for (int32 f = 0; f < 10; ++f)
				{
					const FVector Fp = FVector(Pc.X, Pc.Y, Gp) + FRotator(0.f, f * 36.f, 0.f).RotateVector(FVector(180.f, 0, 0));
					B.K.Rod(OcDrift(0.9f), Fp - FVector(0, 0, 20.f), Fp + FVector(0, 0, 90.f + (f % 3 == 0 ? 30.f : 0.f)), 6.f);
					if (f % 3 == 0) B.K.Add(OcTimber(0.6f), FVector(8.f, 18.f, 12.f), Fp + FVector(0, 0, 110.f));   // a carved turtle
				}
				for (int32 h = 0; h < 4; ++h) B.K.Disc(FSurf::Stone(FLinearColor(0.02f, 0.02f, 0.018f), 0.9f), FVector(Pc.X, Pc.Y, Gp) + FRotator(0.f, h * 90.f + 45.f, 0.f).RotateVector(FVector(300.f, 0, 2.f)), FVector::UpVector, 26.f, 2.f);
			}
			return true;
		}
		if (K == TEXT("rip_beach"))
		{
			// The hazard teacher (cook 2.8, beat 3): a driftwood post at the tide line holding a line of white shells strung
			// sideways (parallel to the shore: swim that way out of a rip); a newcomer's sandal at its foot.
			const FVector Post(0, 0, G);
			B.Limb(Post - FVector(0, 0, 40.f), Post + FVector(0, 0, 280.f), 12.f, 9.f, OcSRGB(0xA8, 0xA3, 0x9A) * 0.5f, 7);
			B.Line(Post + FVector(0, -300.f, 160.f), Post + FVector(0, 300.f, 160.f), 1.5f, 8.f, OcSRGB(0x8C, 0x7A, 0x5A) * 0.45f, 12);
			for (int32 s = 0; s < 12; ++s) B.K.Add(OcShell(), FVector(16.f, 12.f, 5.f), Post + FVector(0, -280.f + s * 50.f, 150.f), FRotator(0.f, 90.f, 70.f), EWRLookMesh::Cone);
			for (int32 s = -1; s <= 1; s += 2) B.Limb(Post + FVector(0, s * 300.f, -20.f), Post + FVector(0, s * 300.f, 180.f), 7.f, 5.f, OcSRGB(0xA8, 0xA3, 0x9A) * 0.5f, 6);
			B.K.Add(OcTimber(0.5f), FVector(26.f, 10.f, 3.f), Post + FVector(40.f, 20.f, 2.f), FRotator(0.f, 30.f, 0.f));
			B.K.Rod(FSurf::Cloth(OcSRGB(0x6E, 0x4E, 0x34) * 0.5f), Post + FVector(30.f, 15.f, 4.f), Post + FVector(50.f, 26.f, 4.f), 2.f);
			return true;
		}
		if (K == TEXT("driftwood_chapel"))
		{
			// A chapel of ship timbers: ribs for rafters, planks for walls; votive model boats hanging from the rafters,
			// each with a tiny harpoon in its bow; the altar a whale vertebra with a tide-worn bronze bowl of sea-glass.
			OcHull(B, FVector(0, 0, G + 520.f), 0.f, 1200.f, 180.f, 0.f, OcTimber(0.6f), false, false);   // the roof: an upturned hull
			for (int32 s = -1; s <= 1; s += 2) for (int32 p = 0; p < 9; ++p) B.K.Add(OcDrift(0.8f + 0.2f * B.Rand(p + s * 9)), FVector(120.f, 14.f, 380.f), FVector(-540.f + p * 130.f, s * 150.f, G + 190.f));
			B.Box(FVector(0, -160.f, G + 200.f), FVector(600.f, 15.f, 200.f));
			B.Box(FVector(0, 160.f, G + 200.f), FVector(600.f, 15.f, 200.f));
			for (int32 v = 0; v < 7; ++v)
			{
				const FVector Hang(-450.f + v * 150.f, 0, G + 420.f - 40.f * (v % 2));
				B.K.Rod(OcNet(), Hang, Hang + FVector(0, 0, 100.f), 1.f);
				B.K.Add(OcTimber(0.8f), FVector(50.f, 12.f, 10.f), Hang - FVector(0, 0, 5.f));
				B.K.Rod(OcBronze(), Hang + FVector(25.f, 0, 0.f), Hang + FVector(40.f, 0, 4.f), 1.f);
				B.K.Rod(OcSail(), Hang, Hang + FVector(0, 0, 30.f), 1.f);
			}
			B.K.Add(OcBone(), FVector(120.f, 100.f, 70.f), FVector(500.f, 0, G + 35.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			B.K.Add(OcVerdigris(), FVector(60.f, 60.f, 16.f), FVector(500.f, 0, G + 78.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			for (int32 g = 0; g < 7; ++g) B.K.Add(OcSeaGlass(), FVector(8.f, 6.f, 4.f), FVector(495.f + (g % 3) * 8.f, -10.f + (g / 3) * 8.f, G + 88.f));
			B.Box(FVector(500.f, 0, G + 40.f), FVector(60.f, 50.f, 40.f));
			return true;
		}
		if (K == TEXT("kelp_pits"))
		{
			// Stone-lined kelp-burning pits (the real Orkney kelp-ash trade): black ash crust in each, rakes, cooled lumps
			// of soda ash on a flat stone, a cairn of drying kelp beside.
			for (int32 p = 0; p < 4; ++p)
			{
				const FVector Pc(-500.f + p * 330.f, (p % 2) * 300.f - 150.f, 0.f);
				const float Gp = Gz(B, Pc.X, Pc.Y);
				for (int32 s = 0; s < 12; ++s) B.K.Add(OcLime(0.8f), FVector(70.f, 50.f, 40.f), FVector(Pc.X, Pc.Y, Gp + 10.f) + FRotator(0.f, s * 30.f, 0.f).RotateVector(FVector(130.f, 0, 0)), FRotator(0.f, s * 30.f, 0.f));
				B.K.Add(FSurf::Stone(FLinearColor(0.02f, 0.018f, 0.016f), 0.95f), FVector(220.f, 220.f, 6.f), FVector(Pc.X, Pc.Y, Gp - 10.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			}
			Rakes(B, FVector(300.f, 700.f, Gz(B, 300.f, 700.f)), 0.f, 5);
			for (int32 k = 0; k < 9; ++k) B.K.Add(FSurf::Cloth(OcKelp() * 0.8f), FVector(160.f, 30.f, 10.f), FVector(-600.f, 500.f, Gz(B, -600.f, 500.f) + 6.f + k * 9.f), FRotator(0.f, k * 40.f, 0.f));
			return true;
		}
		if (K == TEXT("shipwrights_slip"))
		{
			// A stone boat slip into the Gate Inlet with the ribs of a half-built Saltkin hull on its cradle; tidecoral
			// fittings scattered and hardened white (the shipwright never finished).
			for (int32 k = 0; k < 18; ++k)
			{
				const float X = -800.f + k * 100.f;
				B.K.Add(OcLime(0.85f), FVector(100.f, 500.f, 40.f), FVector(X, 0, Gz(B, X, 0) - 10.f), FRotator(0.f, 0.f, 0.f));
			}
			OcHull(B, FVector(-200.f, 0, Gz(B, -200.f, 0) + 60.f), 0.f, 1300.f, 0.f, 0.9f, OcTimber(0.65f), true);
			for (int32 cr = 0; cr < 4; ++cr) B.K.Add(OcTimber(0.5f), FVector(40.f, 380.f, 50.f), FVector(-700.f + cr * 330.f, 0, Gz(B, -700.f + cr * 330.f, 0) + 25.f));
			for (int32 f = 0; f < 7; ++f) B.K.Add(FSurf::Stone(OcSRGB(0xF2, 0xEF, 0xE6) * 0.75f, 0.85f), FVector(30.f, 18.f, 14.f), FVector(300.f + f * 60.f, 300.f - (f % 3) * 60.f, Gz(B, 300.f, 300.f) + 7.f), FRotator(0.f, f * 50.f, 0.f));
			return true;
		}
		if (K == TEXT("whalebone_arch"))
		{
			// Two whale jawbones 9 m tall set as a gate over the coast road (the real Whitby arch).
			for (int32 s = -1; s <= 1; s += 2)
			{
				TArray<FVector> Pts;
				for (int32 i = 0; i <= 8; ++i)
				{
					const float T = i / 8.f;
					Pts.Add(FVector(0, s * FMath::Lerp(260.f, 30.f, T * T), G + T * 900.f));
				}
				for (int32 i = 0; i < Pts.Num() - 1; ++i) B.Limb(Pts[i], Pts[i + 1], FMath::Lerp(34.f, 14.f, i / 8.f), FMath::Lerp(34.f, 14.f, (i + 1) / 8.f), OcSRGB(0xE8, 0xE0, 0xCC) * 0.62f, 8);
				B.Box(FVector(0, s * 200.f, G + 300.f), FVector(30.f, 30.f, 300.f));
			}
			return true;
		}
		if (K == TEXT("mangrove_mouth"))
		{
			// The Swamp border: oyster reefs on mangrove roots (the mangroves themselves are decoration), a sunken Coven
			// punt in the brackish water, a Saltkin shell chime hung from a mangrove (neighbours touched).
			for (int32 o = 0; o < 14; ++o) B.K.Add(FSurf::Stone(OcSRGB(0xBD, 0xB6, 0xA6) * 0.5f, 0.7f), FVector(60.f, 40.f, 20.f), FVector(-600.f + o * 90.f, (o % 3 - 1) * 200.f, Gz(B, -600.f + o * 90.f, 0.f) + 8.f), FRotator(0.f, o * 41.f, 12.f), EWRLookMesh::Cone);
			OcHull(B, FVector(400.f, -300.f, Gz(B, 400.f, -300.f) - 40.f), 140.f, 500.f, 12.f, 0.4f, FSurf::Wood(OcSRGB(0x3A, 0x30, 0x24) * 0.5f));
			OcChimeBeacon(B, FVector(-300.f, 400.f, Gz(B, -300.f, 400.f)), 90.f, 10.f, 1);
			return true;
		}
		if (K == TEXT("gate_of_shells"))
		{
			// The Saltkin pass fort at Saltgate's east mouth: driftwood-and-coral-block walls across the valley, harpoon
			// racks and arrow slits facing west down the pass (the Sunwatch came that way), a gate.
			for (int32 s = -1; s <= 1; s += 2)
			{
				OcCoursedWall(B, FVector(0, s * 400.f, Gz(B, 0, s * 400.f)), FVector(0, s * 2400.f, Gz(B, 0, s * 2400.f)), 500.f, 160.f, 120.f, 70.f, 0.95f);
				for (int32 sl = 0; sl < 6; ++sl) B.K.Add(FSurf::Stone(FLinearColor(0.02f, 0.02f, 0.02f), 0.9f), FVector(170.f, 14.f, 80.f), FVector(-10.f, s * (700.f + sl * 280.f), Gz(B, 0, s * 400.f) + 360.f));
				HarpoonRack(B, FVector(300.f, s * 900.f, Gz(B, 300.f, s * 900.f)), 0.f, 180.f, 6, false);
				for (int32 dp = 0; dp < 14; ++dp) B.K.Add(OcDrift(0.85f), FVector(18.f, 160.f, 140.f), FVector(-90.f, s * (500.f + dp * 140.f), Gz(B, 0, s * 400.f) + 570.f), FRotator(0.f, 0.f, 6.f * (B.Rand(dp) - 0.5f)));
			}
			for (int32 s = -1; s <= 1; s += 2) B.K.Add(OcTimber(0.55f), FVector(30.f, 380.f, 420.f), FVector(0, s * 200.f, Gz(B, 0, 0) + 210.f), FRotator(0.f, s * 35.f, 0.f));
			return true;
		}
		if (K == TEXT("village"))
		{
			// Saltkin Steps: the newcomers' shore village above the line (world-map's village site): houses re-roofed with
			// sailcloth, a market square of a dozen stalls under awnings, a fish-drying rack, a well head.
			for (int32 h = 0; h < 6; ++h) OcSaltkinHouse(B, FVector(-1200.f + (h % 3) * 1200.f, -900.f + (h / 3) * 1800.f, Gz(B, -1200.f + (h % 3) * 1200.f, -900.f + (h / 3) * 1800.f)), 0.f, true, h + 4);
			for (int32 s = 0; s < 6; ++s)
			{
				const FVector St(-500.f + s * 200.f, 0.f, Gz(B, -500.f + s * 200.f, 0.f));
				for (int32 p = 0; p < 4; ++p) B.K.Rod(OcDrift(), St + FVector((p % 2) * 160.f - 80.f, (p / 2) * 140.f - 70.f, 0.f), St + FVector((p % 2) * 160.f - 80.f, (p / 2) * 140.f - 70.f, 220.f), 5.f);
				B.K.Add(s % 2 ? OcSail() : FSurf::Cloth(OcSRGB(0x3E, 0x8C, 0x86) * 0.5f), FVector(190.f, 170.f, 4.f), St + FVector(0, 0, 225.f), FRotator(0.f, 0.f, 6.f));
				B.K.Add(OcTimber(0.6f), FVector(160.f, 60.f, 70.f), St + FVector(0, -40.f, 35.f));
			}
			const FVector Well(0.f, 600.f, Gz(B, 0.f, 600.f));
			for (int32 r = 0; r < 10; ++r) B.K.Add(OcLime(0.9f), FVector(60.f, 40.f, 80.f), Well + FRotator(0.f, r * 36.f, 0.f).RotateVector(FVector(90.f, 0, 40.f)), FRotator(0.f, r * 36.f, 0.f));
			B.Box(Well + FVector(0, 0, 40.f), FVector(110.f, 110.f, 40.f));
			return true;
		}
		// ---- the sea sites ----
		if (K == TEXT("sunken_market"))
		{
			// The city's market on the reef flat (dry only in the King Tide): stone stall frames round a paved square,
			// amphorae in rows, a fountain with a carved fish; the Low Gate at its east edge, sealed by sand.
			for (int32 px = -6; px <= 6; ++px) for (int32 py = -6; py <= 6; ++py) if ((px + py) % 2 == 0) B.K.Add(OcLime(0.75f + 0.1f * B.Rand(px * 13 + py)), FVector(196.f, 196.f, 14.f), FVector(px * 200.f, py * 200.f, Gz(B, px * 200.f, py * 200.f) + 2.f));
			for (int32 s = 0; s < 10; ++s)
			{
				const FVector St = FRotator(0.f, s * 36.f, 0.f).RotateVector(FVector(1100.f, 0, 0));
				const float Gs = Gz(B, St.X, St.Y);
				for (int32 p = 0; p < 2; ++p) B.K.Add(OcLime(0.8f), FVector(40.f, 40.f, 200.f), FVector(St.X, St.Y, Gs) + FRotator(0.f, s * 36.f + 90.f, 0.f).RotateVector(FVector(p ? 90.f : -90.f, 0, 100.f)));
				B.K.Add(OcLime(0.8f), FVector(60.f, 230.f, 30.f), FVector(St.X, St.Y, Gs + 215.f), FRotator(0.f, s * 36.f, 0.f));
				for (int32 a = 0; a < 4; ++a) B.K.Add(OcClay(), FVector(36.f, 36.f, 70.f), FVector(St.X, St.Y, Gs + 35.f) + FRotator(0.f, s * 36.f, 0.f).RotateVector(FVector(-70.f, -90.f + a * 60.f, 0.f)), FRotator(a == 2 ? 70.f : 0.f, 0.f, 0.f), EWRLookMesh::Cylinder);
			}
			B.K.Add(OcLime(0.9f), FVector(400.f, 400.f, 60.f), FVector(0, 0, G + 30.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			B.K.Add(OcLime(1.0f), FVector(60.f, 120.f, 60.f), FVector(0, 0, G + 110.f), FRotator(0.f, 0.f, 30.f));
			for (int32 c = 0; c < 8; ++c) OcCoralHead(B, FVector(-900.f + c * 260.f, 900.f - (c % 3) * 400.f, G), 0.6f + 0.3f * B.Rand(c), c % 6, c);
			return true;
		}
		if (K == TEXT("kelp_cathedral"))
		{
			// The tallest grove (30 m), its sunbeams in columns; the anchor stone Hae-won sends you to touch (a Saltkin
			// mooring block with a rotted iron ring) at its heart; the Wavecharger herd anchors here at night.
			for (int32 k = 0; k < 40; ++k)
			{
				const float A = B.Rand(k * 3) * 2.f * PI, D = 300.f + 2200.f * FMath::Sqrt(B.Rand(k * 7));
				const FVector F(FMath::Cos(A) * D, FMath::Sin(A) * D, 0.f);
				const float Gf = Gz(B, F.X, F.Y);
				OcKelpStipe(B, FVector(F.X, F.Y, Gf), FMath::Max(200.f, Sea - Gf + 150.f), A * 57.3f, k);
			}
			B.K.Add(OcLime(0.8f), FVector(200.f, 160.f, 120.f), FVector(0, 0, G + 50.f));
			B.K.Ring(OcRust(), FVector(102.f, 0, G + 80.f), 26.f, 6.f, 8.f, 8);
			B.Box(FVector(0, 0, G + 50.f), FVector(100.f, 80.f, 60.f));
			for (int32 sb = 0; sb < 6; ++sb) B.Loop(TEXT("ocean.sunbeam"), FVector(-1200.f + sb * 480.f, (sb % 2) * 600.f - 300.f, Sea - 200.f), FRotator(-80.f, 0.f, 0.f), 3.f);
			return true;
		}
		if (K == TEXT("otter_barren"))
		{
			// A purple urchin barren in a kelp gap (the otters keep the urchins down: where they don't, the kelp goes).
			for (int32 u = 0; u < 30; ++u)
			{
				const FVector P((B.Rand(u) - 0.5f) * 2400.f, (B.Rand(u + 50) - 0.5f) * 2400.f, 0.f);
				B.Ball(FVector(P.X, P.Y, Gz(B, P.X, P.Y) + 8.f), 9.f, OcUrchin(), 0.6f);
				for (int32 s = 0; s < 6; ++s) B.K.Spike(FSurf::Stone(OcUrchin() * 1.2f, 0.5f), FVector(P.X, P.Y, Gz(B, P.X, P.Y) + 8.f), FRotator(25.f + s * 10.f, s * 60.f, 0.f).Vector(), 2.f, 14.f);
			}
			for (int32 k = 0; k < 14; ++k)
			{
				const float A = k / 14.f * 2.f * PI;
				const FVector F(FMath::Cos(A) * 1600.f, FMath::Sin(A) * 1600.f, 0.f);
				OcKelpStipe(B, FVector(F.X, F.Y, Gz(B, F.X, F.Y)), FMath::Max(200.f, Sea - Gz(B, F.X, F.Y) + 100.f), A * 57.3f + 90.f, k + 7);
			}
			return true;
		}
		if (K == TEXT("mooring_stones"))
		{
			// A ring of eight Saltkin bollard stones in the shallows (1-2 m deep), iron rings set in each, old rope rotted
			// in the rings: where the Saltkin tied off their catch (and where the Reef Tyrant rests: tie it off here).
			for (int32 m = 0; m < 8; ++m)
			{
				const FRotator R(0.f, m * 45.f, 0.f);
				const FVector P = R.RotateVector(FVector(1800.f, 0, 0));
				const float Gm = Gz(B, P.X, P.Y);
				B.K.Add(OcLime(0.8f), FVector(110.f, 110.f, Sea - Gm + 120.f), FVector(P.X, P.Y, (Sea + Gm + 120.f) * 0.5f), R, EWRLookMesh::Cylinder);
				B.K.Ring(OcRust(), FVector(P.X, P.Y, Sea + 60.f) - R.Vector() * 58.f, 22.f, 6.f, 8.f, 8);
				B.K.Rod(OcNet(), FVector(P.X, P.Y, Sea + 50.f) - R.Vector() * 60.f, FVector(P.X, P.Y, Sea - 40.f) - R.Vector() * 110.f, 3.f);
				B.K.Add(OcBarnacle(), FVector(116.f, 116.f, 60.f), FVector(P.X, P.Y, Sea - 30.f), R, EWRLookMesh::Cylinder);
				B.Box(FVector(P.X, P.Y, (Sea + Gm + 120.f) * 0.5f), FVector(55.f, 55.f, (Sea - Gm + 120.f) * 0.5f));
			}
			return true;
		}
		if (K == TEXT("impaled_ship"))
		{
			// A newcomer ship run up onto the reef crest: its bow 4 m out of the surf, its stern hanging over the slope; a
			// broken mast; the captain's chest in the forecastle. Tidecoils in the hull.
			OcHull(B, FVector(0, 0, Sea - 150.f), 0.f, 2600.f, -4.f, 0.5f, OcTar());
			B.K.Rod(OcTimber(0.6f), FVector(200.f, 0, Sea + 100.f), FVector(600.f, 120.f, Sea + 900.f), 22.f);
			B.K.Add(OcSail(), FVector(10.f, 300.f, 400.f), FVector(500.f, 200.f, Sea + 500.f), FRotator(0.f, 20.f, 30.f));
			B.Box(FVector(900.f, 0, Sea + 250.f), FVector(400.f, 220.f, 30.f), FRotator(8.f, 0.f, 0.f));
			B.Loop(TEXT("ocean.surf_crest"), FVector(-300.f, 0, Sea), FRotator::ZeroRotator, 2.f);
			return true;
		}
		if (K == TEXT("sunwatch_barque"))
		{
			// A Sunwatch sun-barque (cedar, the high curved prow) sunk on the reef slope at 18 m, its sun-disc prow coral-
			// crusted: the war came by sea too.
			OcHull(B, FVector(0, 0, G + 40.f), 0.f, 2000.f, 18.f, 0.35f, OcCedar());
			TArray<FVector> Prow;
			for (int32 i = 0; i <= 6; ++i) Prow.Add(FVector(1000.f + i * 60.f, 0, G + 120.f + i * i * 18.f));
			B.Root(Prow, 26.f, 14.f, OcSRGB(0x9A, 0x58, 0x36) * 0.5f, 7);
			B.K.Disc(OcGold(), Prow.Last() + FVector(20.f, 0, 40.f), FVector(1, 0, 0), 120.f, 8.f);
			for (int32 c = 0; c < 6; ++c) OcCoralHead(B, Prow.Last() + FVector(0, (c - 2.5f) * 30.f, 30.f + c * 10.f), 0.4f, c % 6, c);
			return true;
		}
		if (K == TEXT("clam_garden"))
		{
			// Forty giant clams (1-2 m, mantles glowing blue and green): the clams themselves are the Ocean's nodes; here
			// the garden's bed of sand and broken shell.
			for (int32 s = 0; s < 40; ++s) B.K.Add(OcShell(), FVector(20.f, 14.f, 4.f), FVector((B.Rand(s) - 0.5f) * 3000.f, (B.Rand(s + 40) - 0.5f) * 3000.f, Gz(B, 0, 0) + 2.f), FRotator(0.f, s * 23.f, 10.f), EWRLookMesh::Cone);
			for (int32 c = 0; c < 40; ++c)
			{
				const float A = B.Rand(c * 3 + 2) * 2.f * PI, R = 200.f + 2000.f * B.Rand(c * 5 + 1);
				OcClam(B, FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, Gz(B, FMath::Cos(A) * R, FMath::Sin(A) * R)), A * 57.3f, 1.f + B.Rand(c));
			}
			return true;
		}
		if (K == TEXT("whale_fall"))
		{
			// A whale skeleton on a ledge of the Blue Wall at 45 m: skull, jaw, the spine's vertebrae laid out, ribs
			// arching, furred with bone-worms (a rust-red fuzz); hagfish drift through the ribs; a newcomer diver's helmet.
			const FVector Sk(0, 0, G + 60.f);
			B.Ball(Sk, 120.f, OcSRGB(0xE8, 0xE0, 0xCC) * 0.6f, 0.7f, 0.6f);
			B.Limb(Sk + FVector(100.f, 0, -20.f), Sk + FVector(500.f, 60.f, -40.f), 26.f, 18.f, OcSRGB(0xE8, 0xE0, 0xCC) * 0.6f, 8);
			for (int32 v = 0; v < 26; ++v)
			{
				const FVector Vp = Sk + FVector(-200.f - v * 55.f, 20.f * FMath::Sin(v * 0.4f), -20.f);
				B.K.Add(OcBone(), FVector(40.f, 50.f, 44.f), Vp, FRotator(0.f, 10.f * FMath::Sin(v * 0.4f), 0.f), EWRLookMesh::Cylinder);
				if (v < 14) for (int32 s = -1; s <= 1; s += 2) B.Root({ Vp + FVector(0, s * 20.f, 10.f), Vp + FVector(0, s * 160.f, 80.f), Vp + FVector(0, s * 220.f, -30.f) }, 9.f, 6.f, OcSRGB(0xD8, 0xD0, 0xBE) * 0.55f, 5);
				if (v % 3 == 0) B.K.Add(FSurf::Cloth(OcSRGB(0x9A, 0x3E, 0x2A) * 0.5f), FVector(30.f, 40.f, 8.f), Vp + FVector(0, 0, 24.f));
			}
			B.Ball(FVector(300.f, -200.f, G + 30.f), 30.f, OcSRGB(0x8C, 0x6A, 0x3A) * 0.5f, 0.3f);           // the helmet
			B.K.Disc(OcSeaGlass(), FVector(330.f, -200.f, G + 34.f), FVector(1, 0, 0), 30.f, 3.f);
			return true;
		}
		if (K == TEXT("colossus_hand"))
		{
			// A Sunwatch colossus's severed stone hand, 8 m long, on the reef, fingers half-curled; tidecoral grows thickest
			// on it (its nodes are the richest field). It belongs to a second statue that fell into the sea.
			const FLinearColor Sand = OcSRGB(0xC8, 0xA0, 0x70) * 0.5f;
			B.Ball(FVector(0, 0, G + 120.f), 220.f, Sand, 0.9f, 0.6f);
			for (int32 f = 0; f < 4; ++f) B.Root({ FVector(150.f, -150.f + f * 100.f, G + 120.f), FVector(450.f, -170.f + f * 110.f, G + 160.f), FVector(650.f, -170.f + f * 110.f, G + 60.f) }, 50.f, 40.f, Sand, 10);
			B.Root({ FVector(0, 200.f, G + 130.f), FVector(220.f, 380.f, G + 120.f), FVector(330.f, 300.f, G + 70.f) }, 52.f, 40.f, Sand, 10);
			B.Limb(FVector(-200.f, 0, G + 120.f), FVector(-400.f, 0, G + 100.f), 200.f, 210.f, Sand * 0.9f, 12);
			B.Box(FVector(150.f, 0, G + 120.f), FVector(400.f, 260.f, 120.f));
			for (int32 c = 0; c < 14; ++c) OcCoralHead(B, FVector(-200.f + (c % 7) * 120.f, (c / 7) * 200.f - 100.f, G + 260.f), 0.5f + 0.4f * B.Rand(c), c % 6, c + 3);
			return true;
		}
		return false;
	}

	// ------------------------------------------------------------------------------------------------ the kit

	bool Kit(FOcCtx& C)
	{
		FBuild& B = C.B;
		const FName K = C.A->Kind;
		const int32 V = C.A->Variant;
		const float G = Gz(B, 0, 0);
		const float Sea = C.SeaZ();
		if (K == TEXT("chime_beacon")) { OcChimeBeacon(B, FVector(0, 0, G), 0.f, C.A->Size > 0.f ? C.A->Size : 4.f, V); return true; }
		if (K == TEXT("castaway_hull"))
		{
			// A newcomer skiff or boat broken on the sand (the castaways' way here), half-buried, a sea chest beside it.
			OcHull(B, FVector(0, 0, G - 30.f), 0.f, 500.f + V * 250.f, V == 2 ? 170.f : 20.f + 15.f * V, 0.35f + 0.2f * V, OcTimber(0.6f));
			if (V != 2) B.K.Rod(OcTimber(0.6f), FVector(50.f, 0, G + 40.f), FVector(150.f, 40.f, G + 380.f), 10.f);
			B.K.Add(OcTimber(0.5f), FVector(80.f, 50.f, 50.f), FVector(120.f, 0, G + 25.f));
			return true;
		}
		if (K == TEXT("tidepool_cluster"))
		{
			for (int32 p = 0; p < 6 + V * 2; ++p) TidePool(B, FVector((B.Rand(p) - 0.5f) * 2400.f, (B.Rand(p + 9) - 0.5f) * 2400.f, Gz(B, (B.Rand(p) - 0.5f) * 2400.f, (B.Rand(p + 9) - 0.5f) * 2400.f) - 20.f), 180.f + 120.f * B.Rand(p + 3), p);
			return true;
		}
		if (K == TEXT("blowhole_field"))
		{
			// 3-6 vents in the shelf (each a dark hole ringed with wet black rock and white salt crust); their blasts are
			// the hazard's (AWRBlowhole).
			for (int32 v = 0; v < 3 + V * 2; ++v)
			{
				const FVector P((B.Rand(v) - 0.5f) * 3600.f, (B.Rand(v + 5) - 0.5f) * 3600.f, 0.f);
				const float Gv = Gz(B, P.X, P.Y);
				for (int32 r = 0; r < 8; ++r) B.K.Add(OcWetBlack(), FVector(60.f, 70.f, 30.f), FVector(P.X, P.Y, Gv) + FRotator(0.f, r * 45.f, 0.f).RotateVector(FVector(110.f, 0, 0)), FRotator(0.f, r * 45.f, 0.f));
				B.K.Add(FSurf::Stone(FLinearColor(0.01f, 0.01f, 0.012f), 0.3f), FVector(150.f, 150.f, 6.f), FVector(P.X, P.Y, Gv - 2.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
				B.K.Add(OcSalt(), FVector(320.f, 320.f, 2.f), FVector(P.X, P.Y, Gv - 3.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			}
			return true;
		}
		if (K == TEXT("fish_weir"))
		{
			// A Saltkin stone tidal trap: a low curved wall of stones on the foreshore, open to the land, that the ebb
			// strands fish in.
			const int32 N = 30;
			for (int32 s = 0; s < N; ++s)
			{
				const float A = FMath::DegreesToRadians(-75.f + 150.f * s / (N - 1));
				const FVector P(FMath::Cos(A) * 1200.f, FMath::Sin(A) * 1200.f, 0.f);
				B.K.Add(OcLime(0.7f), FVector(70.f, 60.f, 50.f + 20.f * (V == 1)), FVector(P.X, P.Y, Gz(B, P.X, P.Y) + 20.f), FRotator(0.f, FMath::RadiansToDegrees(A), 0.f));
			}
			return true;
		}
		if (K == TEXT("saltpan_terrace"))
		{
			// A terrace of salt pans with its stone hut (a rake shed).
			for (int32 i = 0; i < 8; ++i) SaltPan(B, FVector(-600.f + (i % 4) * 400.f, -200.f + (i / 4) * 420.f, Gz(B, -600.f + (i % 4) * 400.f, -200.f + (i / 4) * 420.f) + 4.f), 0.f, 360.f, 380.f);
			OcSaltkinHouse(B, FVector(1200.f, 0, Gz(B, 1200.f, 0)), 180.f, V == 0, V + 2);
			Rakes(B, FVector(800.f, 700.f, Gz(B, 800.f, 700.f)), 90.f, 4);
			return true;
		}
		if (K == TEXT("tsunami_outlier")) { OcTsunamiStone(B, FVector(0, 0, G), C.A->GetActorRotation().Yaw, true); return true; }
		if (K == TEXT("kill_scene"))
		{
			// Variant 0: a Reefjaw stranded in a pool on a Kelpgrazer (a ribcage and a spine in the pool, a shark's
			// jaw beside: the sea's food chain). Variant 1: Gullraiders mobbing a Tidefowl colony (feathers, broken eggs).
			if (V == 0)
			{
				TidePool(B, FVector(0, 0, G - 30.f), 500.f, 9);
				for (int32 r = 0; r < 10; ++r) for (int32 s = -1; s <= 1; s += 2) B.Root({ FVector(-300.f + r * 60.f, s * 20.f, G), FVector(-300.f + r * 60.f, s * 140.f, G + 80.f), FVector(-300.f + r * 60.f, s * 170.f, G - 10.f) }, 8.f, 5.f, OcSRGB(0xD8, 0xD0, 0xBE) * 0.5f, 5);
				B.K.Ring(OcBone(), FVector(400.f, 0, G + 40.f), 40.f, 12.f, 14.f, 10);
			}
			else
			{
				for (int32 f = 0; f < 30; ++f) B.K.Add(FSurf::Cloth(f % 3 ? OcSRGB(0xF0, 0xF0, 0xF0) * 0.7f : OcSRGB(0x20, 0x20, 0x20)), FVector(14.f, 4.f, 1.f), FVector((B.Rand(f) - 0.5f) * 600.f, (B.Rand(f + 30) - 0.5f) * 600.f, Gz(B, 0, 0) + 1.f), FRotator(0.f, f * 37.f, 0.f));
				for (int32 e = 0; e < 6; ++e) B.Ball(FVector((B.Rand(e + 60) - 0.5f) * 300.f, (B.Rand(e + 70) - 0.5f) * 300.f, G + 4.f), 5.f, OcSRGB(0xE8, 0xE0, 0xC8) * 0.7f, 0.5f, 0.6f);
			}
			return true;
		}
		if (K == TEXT("driftwood_shelter"))
		{
			// A newcomer lean-to: driftwood poles against a log, a sailcloth roof, a fire ring with cold ash.
			const FVector Log(0, 0, G + 30.f);
			B.Limb(Log + FVector(-250.f, 0, 0), Log + FVector(250.f, 0, 0), 25.f, 20.f, OcSRGB(0xA8, 0xA3, 0x9A) * 0.5f, 8);
			for (int32 p = 0; p < 6; ++p) B.K.Rod(OcDrift(), FVector(-220.f + p * 88.f, 300.f, G), FVector(-220.f + p * 88.f, 0.f, G + 230.f), 6.f);
			B.K.Add(OcSail(), FVector(520.f, 380.f, 5.f), FVector(0, 150.f, G + 120.f), FRotator(0.f, 0.f, -36.f));
			for (int32 s = 0; s < 8; ++s) B.K.Add(OcLime(0.7f), FVector(30.f, 26.f, 20.f), FRotator(0.f, s * 45.f, 0.f).RotateVector(FVector(90.f, 0, 0)) + FVector(0, 500.f, G + 8.f));
			B.K.Add(FSurf::Stone(OcSRGB(0x30, 0x2C, 0x28) * 0.5f, 0.95f), FVector(120.f, 120.f, 4.f), FVector(0, 500.f, G + 2.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			B.SegBox(Log + FVector(-250.f, 0, 0), Log + FVector(250.f, 0, 0), 25.f, 25.f);
			return true;
		}
		if (K == TEXT("coral_head"))
		{
			// A coral bommie on the reef or the kelp shelf's deep edge (its tidecoral nodes are the Ocean's nodes).
			for (int32 c = 0; c < 6 + V * 2; ++c) OcCoralHead(B, FVector((B.Rand(c) - 0.5f) * 700.f, (B.Rand(c + 7) - 0.5f) * 700.f, Gz(B, (B.Rand(c) - 0.5f) * 700.f, (B.Rand(c + 7) - 0.5f) * 700.f)), 0.8f + 0.9f * B.Rand(c + 3), (c + V) % 6, c + V * 11);
			B.Rock(V, FVector(500.f, 420.f, 260.f), FVector(0, 0, G - 60.f), FRotator(0.f, V * 50.f, 0.f), OcRockTint() * 0.5f, true);
			return true;
		}
		if (K == TEXT("drowned_house"))
		{
			// A Saltkin house's roof ridge in the kelp, the rest buried in sand; a jar inside.
			OcSaltkinHouse(B, FVector(0, 0, G - 150.f), C.A->GetActorRotation().Yaw, V != 2, V + 5);
			for (int32 k = 0; k < 8; ++k) OcKelpStipe(B, FVector((B.Rand(k) - 0.5f) * 1400.f, (B.Rand(k + 9) - 0.5f) * 1400.f, Gz(B, (B.Rand(k) - 0.5f) * 1400.f, (B.Rand(k + 9) - 0.5f) * 1400.f)), FMath::Max(200.f, Sea - G), k * 40.f, k);
			return true;
		}
		if (K == TEXT("manowar_fleet")) return true;   // drawn by the hazard (AWRManOWar), which drifts
		if (K == TEXT("anchor_chain"))
		{
			// A newcomer ship's anchor (an iron stock and flukes) dragged into the sand, its chain running off into the
			// blue, a sunk barrel tied to it (the barrel holds a cache).
			B.K.Rod(OcRust(), FVector(0, 0, G), FVector(0, 0, G + 260.f), 20.f);
			B.K.Rod(OcRust(), FVector(0, -140.f, G + 240.f), FVector(0, 140.f, G + 240.f), 14.f);
			for (int32 s = -1; s <= 1; s += 2) B.K.Rod(OcRust(), FVector(0, 0, G + 20.f), FVector(0, s * 120.f, G + 90.f), 14.f);
			OcChain(B, FVector(0, 0, G + 260.f), FVector(1800.f, 600.f, Gz(B, 1800.f, 600.f) + 10.f), 40.f, -60.f, OcRust());
			B.K.Add(OcTimber(0.5f), FVector(90.f, 90.f, 120.f), FVector(120.f, 0, G + 50.f), FRotator(0.f, 0.f, 80.f), EWRLookMesh::Cylinder);
			B.Box(FVector(0, 0, G + 130.f), FVector(20.f, 20.f, 130.f));
			return true;
		}
		if (K == TEXT("sea_stack"))
		{
			// A climbable stack offshore (8-22 m) with a Tidefowl colony on its top: ledges up its weather side.
			const float H = 800.f + 600.f * V + 300.f * B.Rand(1);
			const float Top = Sea + H;
			B.M.Lathe(FVector(0, 0, G - 100.f), FVector::UpVector, Top - G + 100.f, { FVector(0.f, 600.f, 540.f), FVector(0.5f, 460.f, 420.f), FVector(1.f, 330.f, 300.f) }, 14, OcSRGB(0xC9, 0xA4, 0x6A) * 0.48f, 0.9f, 0.f, float(V + 2));
			for (int32 b = 0; b < 4; ++b) B.Box(FVector(0, 0, (G + Top) * 0.5f), FVector(460.f, 220.f, (Top - G) * 0.5f), FRotator(0.f, b * 45.f, 0.f));
			for (int32 k = 0; k < int32(H / 150.f); ++k)
			{
				const FVector Lp = FRotator(0.f, -50.f + k * 18.f, 0.f).RotateVector(FVector(560.f - k * 15.f, 0, Sea + 80.f + k * 150.f));
				B.K.Add(OcLime(0.9f), FVector(140.f, 120.f, 36.f), Lp, FRotator(0.f, -50.f + k * 18.f, 0.f));
				B.Box(Lp, FVector(70.f, 60.f, 18.f), FRotator(0.f, -50.f + k * 18.f, 0.f));
			}
			B.K.Add(FSurf::Cloth(OcSRGB(0x8F, 0xA6, 0x5A) * 0.45f), FVector(600.f, 600.f, 30.f), FVector(0, 0, Top), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			B.Box(FVector(0, 0, Top), FVector(300.f, 300.f, 15.f));
			B.Loop(TEXT("ocean.gull_colony"), FVector(0, 0, Top + 200.f));
			return true;
		}
		if (K == TEXT("cleaning_station"))
		{
			// A coral bommie where Reefjaws circle slowly to be cleaned (real cleaner-fish stations): a big brain coral
			// mound with small cleaner shrimp-red flecks.
			OcCoralHead(B, FVector(0, 0, G), 3.f + V, 1, 3);
			for (int32 c = 0; c < 10; ++c) OcCoralHead(B, FVector((B.Rand(c) - 0.5f) * 900.f, (B.Rand(c + 4) - 0.5f) * 900.f, G), 0.6f + 0.6f * B.Rand(c), c % 6, c + 21);
			for (int32 f = 0; f < 16; ++f) B.K.Add(FSurf::Lit(OcSRGB(0xE8, 0x50, 0x40), 0.3f), FVector(6.f, 3.f, 2.f), FVector((B.Rand(f) - 0.5f) * 300.f, (B.Rand(f + 3) - 0.5f) * 300.f, G + 160.f + 80.f * B.Rand(f + 8)));
			return true;
		}
		return false;
	}
}
