// The Desert's 35 hand-made story sites and its kit POIs (cooks/biome-desert.md 2.4, cooks/build-biome-desert.md 2.2):
// every site tells its story by placement only (DD SPEC: no actors, no staged scenes), built exactly as the cook's table
// says; what you take from them is a server spot or cache (desert.json). The kit's nine kinds stand where desert.mjs put
// them (Poisson 150 m, the route rule, the density fill), each fitted to the ground it stands on.
#include "WRDesertSceneryParts.h"
#include "Wildreach.h"
#include "WRFx.h"
#include "WRTuning.h"

namespace WRDesertParts
{
	namespace
	{
		/** A newcomer's tent half under sand: ridge pole, canvas sides, guy lines to pegs, sand drifted over one side. */
		void DsTent(FBuild& B, const FVector& Base, float Yaw, float Bury, int32 Seed)
		{
			const FRotator R(0.f, Yaw, 0.f);
			const FVector O = Base - FVector(0, 0, Bury);
			for (int32 e = -1; e <= 1; e += 2) B.K.Rod(DsSunBleach(0.7f), O + R.RotateVector(FVector(e * 160.f, 0, 0)), O + R.RotateVector(FVector(e * 160.f, 0, 190.f)), 5.f);
			B.K.Rod(DsSunBleach(0.7f), O + R.RotateVector(FVector(-170.f, 0, 190.f)), O + R.RotateVector(FVector(170.f, 0, 190.f)), 5.f);
			for (int32 sd = -1; sd <= 1; sd += 2) B.K.Add(DsLinen(0.75f + 0.1f * B.Rand(Seed + sd)), FVector(330.f, 6.f, 230.f), O + R.RotateVector(FVector(0, sd * 80.f, 95.f)), R + FRotator(0.f, 0.f, sd * 34.f));
			for (int32 g = 0; g < 4; ++g) B.Line(O + R.RotateVector(FVector(g < 2 ? -170.f : 170.f, 0, 190.f)), Base + R.RotateVector(FVector(g < 2 ? -320.f : 320.f, g % 2 ? 160.f : -160.f, 0.f)), 1.f, 6.f, DsRGB(0x9C, 0x84, 0x60) * 0.45f, 4);
			B.K.Add(DsSandFill(0.95f), FVector(360.f, 200.f, 120.f), Base + R.RotateVector(FVector(0, 140.f, -20.f)), R, EWRLookMesh::Cylinder);
		}
		/** A Dromad (or a horse) sunk to the neck in a dune: the head and the top of the pack showing, the sand drifted round. */
		void DsSunkDromad(FBuild& B, const FVector& Base, float Yaw, int32 Seed)
		{
			const FRotator R(0.f, Yaw, 0.f);
			B.K.Add(DsHide(0.9f), FVector(140.f, 110.f, 60.f), Base + FVector(0, 0, 10.f), R);                               // the pack's top
			B.Limb(Base + R.RotateVector(FVector(80.f, 0, 0.f)), Base + R.RotateVector(FVector(140.f, 0, 90.f)), 20.f, 14.f, DsRGB(0xB8, 0x98, 0x70) * 0.5f, 7);
			B.M.Lathe(Base + R.RotateVector(FVector(140.f, 0, 90.f)), R.RotateVector(FVector(1, 0, -0.4f)).GetSafeNormal(), 60.f, { FVector(0.f, 16.f, 18.f), FVector(0.7f, 12.f, 14.f), FVector(1.f, 8.f, 9.f) }, 8, DsRGB(0xB8, 0x98, 0x70) * 0.48f, 0.8f);
			B.K.Add(DsSandFill(0.95f), FVector(320.f, 260.f, 70.f), Base + FVector(0, 0, -10.f), R, EWRLookMesh::Cylinder);
		}
	}

	bool Site(FDsCtx& C)
	{
		FBuild& B = C.B;
		const FName K = C.A->Kind;
		const float G = B.G(0, 0, 0.f);
		// ---------------------------------------------------------------- D1 the Wreck Strand
		if (K == TEXT("tall_wreck_site"))
		{
			// S1: the castaways' ship (the landmark builds the hull). The hold's empty casks spilled on the sand, every bung
			// out; a grave line of six driftwood crosses facing the sea; the cask labels match the camps'.
			for (int32 c = 0; c < 9; ++c) DsCask(B, FVector(400.f + (c % 3) * 110.f, -600.f + (c / 3) * 90.f, B.G(400.f + (c % 3) * 110.f, -600.f + (c / 3) * 90.f, G)), c * 40.f, c % 4 == 0);
			for (int32 g = 0; g < 6; ++g) { const FVector P(-900.f, -500.f + g * 200.f, 0.f); DsCross(B, FVector(P.X, P.Y, B.G(P.X, P.Y, G)), 180.f, g); }
			return true;
		}
		if (K == TEXT("salt_cutters"))
		{
			// S2: salt cakes stacked to dry; one stack dragged in a line toward a Dunejaw trough, ending in a snapped harness.
			for (int32 s = 0; s < 4; ++s) for (int32 h = 0; h < 4; ++h) B.K.Add(DsSalt(), FVector(60.f, 40.f, 30.f), FVector(-200.f + s * 120.f, -100.f, B.G(-200.f + s * 120.f, -100.f, G) + 15.f + h * 31.f), FRotator(0.f, s * 7.f + h * 13.f, 0.f));
			for (int32 d = 0; d < 9; ++d) B.K.Add(DsSalt(), FVector(60.f, 40.f, 30.f), FVector(300.f + d * 140.f, 200.f + d * 40.f, B.G(300.f + d * 140.f, 200.f + d * 40.f, G) + 12.f), FRotator(10.f * d, d * 23.f, 6.f));
			B.K.Add(DsSunBleach(0.6f), FVector(240.f, 80.f, 14.f), FVector(1600.f, 560.f, B.G(1600.f, 560.f, G) + 8.f), FRotator(0.f, 16.f, 0.f));       // the sledge
			B.Line(FVector(1700.f, 560.f, B.G(1700.f, 560.f, G) + 20.f), FVector(1900.f, 640.f, B.G(1900.f, 640.f, G) + 4.f), 2.5f, 0.f, DsRGB(0x7A, 0x5E, 0x40) * 0.5f, 4);    // the snapped harness
			for (int32 t = 0; t < 5; ++t) B.K.Add(DsSandFill(0.85f), FVector(500.f, 160.f, 40.f), FVector(2200.f + t * 200.f, 700.f + t * 50.f, B.G(2200.f + t * 200.f, 700.f + t * 50.f, G) - 25.f), FRotator(0.f, 16.f, 0.f), EWRLookMesh::Cylinder);
			B.K.Rod(DsTimber(0.8f), FVector(-300.f, 100.f, G), FVector(-260.f, 110.f, G + 110.f), 4.f);
			B.K.Add(DsBronzeDull(), FVector(30.f, 4.f, 40.f), FVector(-302.f, 98.f, G + 4.f));                            // a salt saw
			return true;
		}
		if (K == TEXT("signal_fire"))
		{
			// S3: a driftwood beacon pyre, never lit, a Saltkin shell-chime tied on top (contact with the other side long ago).
			for (int32 l = 0; l < 16; ++l) { const float A = l * 22.5f * PI / 180.f; B.Limb(FVector(FMath::Cos(A) * 220.f, FMath::Sin(A) * 220.f, G - 10.f), FVector(FMath::Cos(A) * 30.f, FMath::Sin(A) * 30.f, G + 420.f), 12.f, 8.f, DsDriftC() * (0.9f + 0.2f * B.Rand(l)), 6); }
			for (int32 s = 0; s < 8; ++s) { B.K.Rod(DsRope(), FVector(0, 0, G + 440.f), FVector(FMath::Cos(s * 0.8f) * 30.f, FMath::Sin(s * 0.8f) * 30.f, G + 380.f), 1.f); B.K.Add(DsBone(1.05f), FVector(10.f, 8.f, 4.f), FVector(FMath::Cos(s * 0.8f) * 30.f, FMath::Sin(s * 0.8f) * 30.f, G + 375.f), FRotator(70.f, s * 45.f, 0.f), EWRLookMesh::Cone); }
			B.Box(FVector(0, 0, G + 200.f), FVector(150.f, 150.f, 200.f));
			return true;
		}
		if (K == TEXT("brine_pit"))
		{
			// S4: a Dromad skeleton half out of a brine crust, its pack still on, salt crystals grown over the copper cargo.
			DsDromadBones(B, FVector(0, 0, G), C.A->GetActorRotation().Yaw * 0.f, true, 9, 70.f);
			B.K.Add(FSurf::Glaze(DsRGB(0xC8, 0xC4, 0xB8) * 0.6f, 0.35f), FVector(700.f, 600.f, 4.f), FVector(0, 0, G + 2.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			for (int32 x = 0; x < 12; ++x) B.K.Add(DsSalt(), FVector(14.f, 14.f, 20.f), FVector((B.Rand(x) - 0.5f) * 140.f, (B.Rand(x + 5) - 0.5f) * 120.f, G + 90.f), FRotator(30.f * x, 20.f * x, 0.f));
			return true;
		}
		if (K == TEXT("caravan_oasis"))
		{
			// S5 (KEPT farm set-piece): date palms round a stone well; a broken salt caravan beside it, its guards' spears
			// still stuck in the sand pointing outward.
			for (int32 s = 0; s < 14; ++s) { const FRotator R(0.f, s * 360.f / 14.f, 0.f); B.K.Add(DsSandstone(0.9f), FVector(60.f, 70.f, 70.f), R.RotateVector(FVector(130.f, 0, G + 30.f)), R); B.Box(R.RotateVector(FVector(130.f, 0, G + 30.f)), FVector(30.f, 35.f, 35.f), R); }
			B.K.Add(FSurf::Glaze(DsRGB(0x10, 0x20, 0x22) * 0.5f, 0.02f), FVector(220.f, 220.f, 4.f), FVector(0, 0, G - 120.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			for (int32 sd = -1; sd <= 1; sd += 2) B.K.Rod(DsTimber(0.75f), FVector(0, sd * 140.f, G), FVector(0, sd * 140.f, G + 260.f), 8.f);
			B.K.Rod(DsTimber(0.75f), FVector(0, -150.f, G + 250.f), FVector(0, 150.f, G + 250.f), 8.f);
			B.Line(FVector(0, 0, G + 250.f), FVector(0, 0, G + 60.f), 2.f, 0.f, DsRGB(0x9C, 0x84, 0x60) * 0.45f, 3);
			for (int32 p = 0; p < 7; ++p) { const FVector P = FRotator(0.f, p * 51.f + 20.f, 0.f).RotateVector(FVector(700.f + 300.f * B.Rand(p), 0, 0)); DsPalm(B, FVector(P.X, P.Y, B.G(P.X, P.Y, G)), p * 40.f, 0.8f + 0.3f * B.Rand(p + 3), p + 61, p % 2 == 0); }
			DsCaravan(B, FVector(1000.f, -400.f, B.G(1000.f, -400.f, G)), 30.f, 0, 21);
			for (int32 sp = 0; sp < 5; ++sp) { const FVector P = FVector(1000.f, -400.f, 0.f) + FRotator(0.f, sp * 72.f, 0.f).RotateVector(FVector(420.f, 0, 0)); const float Gz = B.G(P.X, P.Y, G); B.K.Rod(DsTimber(0.7f), FVector(P.X, P.Y, Gz - 30.f), FVector(P.X, P.Y, Gz) + FRotator(-50.f, sp * 72.f, 0.f).Vector() * 220.f, 4.f); B.K.Spike(DsBronzeDull(), FVector(P.X, P.Y, Gz) + FRotator(-50.f, sp * 72.f, 0.f).Vector() * 220.f, FRotator(-50.f, sp * 72.f, 0.f).Vector(), 6.f, 30.f); }
			return true;
		}
		if (K == TEXT("shell_shrine"))
		{
			// S6: a ring of shells and a carved driftwood sun round a cold fire: someone tried to pray to what they found here.
			for (int32 s = 0; s < 30; ++s) { const FRotator R(0.f, s * 12.f, 0.f); B.K.Add(DsBone(1.05f), FVector(14.f, 10.f, 6.f), R.RotateVector(FVector(260.f, 0, G + 3.f)), R + FRotator(70.f, 0.f, 0.f), EWRLookMesh::Cone); }
			for (int32 s = 0; s < 8; ++s) B.K.Add(DsSandstone(0.5f), FVector(30.f, 24.f, 16.f), FRotator(0.f, s * 45.f, 0.f).RotateVector(FVector(70.f, 0, G + 6.f)));
			B.K.Add(FSurf::Stone(DsRGB(0x2A, 0x24, 0x20) * 0.6f, 0.95f), FVector(110.f, 110.f, 4.f), FVector(0, 0, G + 2.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			B.K.Rod(DsSunBleach(0.8f), FVector(-300.f, 0, G - 20.f), FVector(-300.f, 0, G + 200.f), 8.f);
			B.K.Disc(DsSunBleach(0.95f), FVector(-300.f, 0, G + 210.f), FVector(1, 0, 0), 90.f, 8.f);
			for (int32 r = 0; r < 10; ++r) B.K.Add(DsSunBleach(0.9f), FVector(6.f, 10.f, 40.f), FVector(-300.f, 0, G + 210.f) + FRotator(0.f, 0.f, r * 36.f).RotateVector(FVector(0, 0, 66.f)), FRotator(0.f, 0.f, r * 36.f));
			return true;
		}
		// ---------------------------------------------------------------- D2 the Erg
		if (K == TEXT("ship_in_dune"))
		{
			// S7: an ancient sea-going hull with oar ports, a Sunwatch sun disc nailed over its sea-god figurehead (the sea
			// people's ship, taken by the sun people).
			DsHull(B, FVector(0, 0, G), FRotator(-6.f, 0.f, 12.f), 2600.f, 520.f, 300.f, 2, 220.f, 0.2f, 117);
			B.K.Disc(DsBronze(), FVector(1340.f, 0, G + 130.f), FVector(1, 0, 0.2f).GetSafeNormal(), 110.f, 8.f);
			B.K.Add(DsSandFill(0.95f), FVector(2000.f, 900.f, 260.f), FVector(-400.f, 500.f, G - 60.f), FRotator(0.f, 10.f, 0.f), EWRLookMesh::Cylinder);
			return true;
		}
		if (K == TEXT("jackal_tomb"))
		{
			// S8: a tiny rock-cut tomb whose door has been dug out from inside the dune (the spoil fans outward); Duskjackal
			// pups' prints and gilt dust on the floor of the burial niche.
			B.Rock(4, FVector(700.f, 600.f, 400.f), FVector(-200.f, 0, G - 60.f), FRotator(0.f, 0.f, 0.f), DsRockTint() * 0.85f, true, 80.f);
			B.K.Add(DsSandstone(1.f), FVector(80.f, 200.f, 40.f), FVector(180.f, 0, G + 230.f));
			for (int32 sd = -1; sd <= 1; sd += 2) B.K.Add(DsSandstone(0.95f), FVector(70.f, 50.f, 220.f), FVector(180.f, sd * 80.f, G + 110.f));
			B.K.Add(FSurf::Stone(DsRGB(0x10, 0x0C, 0x0A), 0.95f), FVector(10.f, 110.f, 200.f), FVector(170.f, 0, G + 100.f));
			B.K.Add(DsSandFill(1.f), FVector(500.f, 300.f, 50.f), FVector(500.f, 0, G - 10.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			B.K.Add(DsGold(), FVector(120.f, 80.f, 2.f), FVector(100.f, 0, G + 2.f));
			return true;
		}
		if (K == TEXT("swallowed_caravan"))
		{
			// S9: eight Dromads and their loads in a perfect line, each sunk to the neck; the last one gone, a 6 m trough where
			// Ssarrok pulled it under.
			for (int32 d = 0; d < 8; ++d) { const FVector P(d * 380.f, 0, 0); DsSunkDromad(B, FVector(P.X, P.Y, B.G(P.X, P.Y, G) - 60.f), 0.f, d); }
			for (int32 t = 0; t < 6; ++t) B.K.Add(DsSandFill(0.75f), FVector(140.f, 600.f, 30.f), FVector(8.f * 380.f + 200.f, -200.f + t * 80.f, B.G(8.f * 380.f + 200.f, 0.f, G) - 60.f), FRotator(0.f, 5.f, 0.f), EWRLookMesh::Cylinder);
			B.Line(FVector(7.f * 380.f, 0, G + 40.f), FVector(8.f * 380.f + 150.f, 0, B.G(8.f * 380.f + 150.f, 0.f, G) - 30.f), 2.f, 20.f, DsRGB(0x7A, 0x5E, 0x40) * 0.5f, 6);    // its lead rope, into the sand
			return true;
		}
		if (K == TEXT("navigators_stones"))
		{
			// S10: three ventifacts with sun lines cut into them; a Sunwatch lens holder on top, empty (the lens recipe).
			for (int32 s = 0; s < 3; ++s)
			{
				const FVector P = FRotator(0.f, s * 120.f, 0.f).RotateVector(FVector(260.f, 0, 0));
				B.Rock(s + 1, FVector(240.f, 160.f, 260.f), FVector(P.X, P.Y, B.G(P.X, P.Y, G)), FRotator(0.f, s * 120.f + 90.f, 0.f), DsRockTint() * 0.85f, true, 30.f);
				for (int32 l = 0; l < 3; ++l) B.K.Add(DsVarnish(), FVector(4.f, 120.f, 4.f), FVector(P.X, P.Y, B.G(P.X, P.Y, G) + 120.f + l * 30.f) + FRotator(0.f, s * 120.f, 0.f).Vector() * 70.f, FRotator(0.f, s * 120.f + 90.f, l * 12.f - 12.f));
			}
			B.K.Add(DsSandstone(0.95f), FVector(80.f, 80.f, 260.f), FVector(0, 0, G + 130.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			B.K.Ring(DsBronze(), FVector(0, 0, G + 280.f), 34.f, 8.f, 10.f, 14);
			B.Box(FVector(0, 0, G + 130.f), FVector(40.f, 40.f, 130.f));
			return true;
		}
		if (K == TEXT("stelae_avenue"))
		{
			// S11: gnomon stelae marching into a dune and out the other side 200 m later (the dune walked over the road).
			for (int32 s = 0; s < 6; ++s) { const FVector P(-2600.f + s * 300.f, 0, 0); DsStela(B, FVector(P.X, P.Y, B.G(P.X, P.Y, G) - s * 40.f), 90.f, 2.f * s, s % 2); }
			for (int32 s = 0; s < 5; ++s) { const FVector P(2000.f + s * 300.f, 0, 0); DsStela(B, FVector(P.X, P.Y, B.G(P.X, P.Y, G) - (4 - s) * 40.f), 90.f, 2.f * (4 - s), s % 2); }
			return true;
		}
		if (K == TEXT("kite_feast"))
		{
			// S12: a fresh Dunejaw carcass on a dune crest (a living set-piece: the kites are the creatures' own, and the
			// carcass a different one each time it respawns: WRDesertLife).
			B.K.Add(DsHide(0.75f), FVector(500.f, 140.f, 90.f), FVector(0, 0, G + 40.f), FRotator(0.f, 30.f, 8.f));
			B.Limb(FVector(250.f, 120.f, G + 50.f), FVector(500.f, 200.f, G + 20.f), 30.f, 8.f, DsRGB(0x8A, 0x6A, 0x48) * 0.5f, 7);
			for (int32 r = 0; r < 8; ++r) { TArray<FVector> Arc; for (int32 k = 0; k <= 4; ++k) { const float A = k / 4.f * 1.6f; Arc.Add(FVector(-150.f + r * 40.f, FMath::Sin(A) * 70.f, G + 40.f + FMath::Cos(A) * 60.f)); } B.M.Tube(Arc, 3.f, 4, DsRGB(0xC8, 0x40, 0x30) * 0.5f, 0.5f); }
			B.K.Add(FSurf::Glaze(DsRGB(0x5A, 0x10, 0x0C) * 0.5f, 0.4f), FVector(400.f, 300.f, 2.f), FVector(0, 0, G + 1.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			return true;
		}
		if (K == TEXT("lost_expedition"))
		{
			// S13: newcomer tents half under sand, a shovel stuck in a dug pit that exposed a carved lintel (the Hourhold's
			// Dig, H-E3: its own way), notes in a lore item (the spot).
			for (int32 t = 0; t < 3; ++t) DsTent(B, FVector(-800.f + t * 500.f, 600.f + t * 120.f, B.G(-800.f + t * 500.f, 600.f + t * 120.f, G)), t * 30.f, 90.f + 40.f * t, t);
			for (int32 c = 0; c < 3; ++c) DsCask(B, FVector(500.f + c * 80.f, 700.f, B.G(500.f + c * 80.f, 700.f, G)), c * 50.f, c == 0);
			B.K.Add(DsSunBleach(0.75f), FVector(160.f, 100.f, 80.f), FVector(600.f, 400.f, B.G(600.f, 400.f, G) + 30.f));     // a field desk
			return true;
		}
		if (K == TEXT("booming_slope"))
		{
			// S14: handprints in the crest's crust, and a slide track down the slipface: someone slid and kept sliding. The
			// boom is loudest here (the hazard's).
			for (int32 h = 0; h < 6; ++h) B.K.Add(FSurf::Stone(DsRGB(0xB0, 0x80, 0x48) * 0.5f, 0.95f), FVector(20.f, 14.f, 2.f), FVector(h * 30.f, (h % 2) * 20.f, B.G(h * 30.f, (h % 2) * 20.f, G) + 1.f), FRotator(0.f, h * 10.f, 0.f));
			for (int32 s = 0; s < 14; ++s) B.K.Add(DsSandFill(0.8f), FVector(160.f, 60.f, 6.f), FVector(-200.f - s * 150.f, 40.f, B.G(-200.f - s * 150.f, 40.f, G) + 1.f), FRotator(0.f, 180.f, 0.f));
			B.Loop(TEXT("desert.crest_stream"), FVector(0, 0, G + 50.f));
			return true;
		}
		// ---------------------------------------------------------------- D3 Oasis Reach and the White Pan
		if (K == TEXT("waystation"))
		{
			// S15: a roofed Sunwatch rest house with stone benches facing east, a water channel still trickling through it.
			const FVector C0(-400.f, -300.f, G - 20.f), C1(400.f, -300.f, G - 20.f), C2(400.f, 300.f, G - 20.f), C3(-400.f, 300.f, G - 20.f);
			DsCoursedWall(B, C0, C1, 300.f, 60.f, DsSandstone(0.9f));
			DsCoursedWall(B, C3, C2, 300.f, 60.f, DsSandstone(0.9f));
			DsCoursedWall(B, C0, C3, 300.f, 60.f, DsSandstone(0.9f));
			for (int32 c = 0; c < 3; ++c) B.K.Add(DsSandstone(1.f), FVector(60.f, 60.f, 300.f), FVector(400.f, -300.f + c * 300.f, G + 130.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			B.K.Add(DsSandstone(0.85f), FVector(900.f, 700.f, 40.f), FVector(0, 0, G + 300.f));
			B.Box(FVector(0, 0, G + 300.f), FVector(450.f, 350.f, 20.f));
			for (int32 b = 0; b < 3; ++b) { B.K.Add(DsSandstone(0.95f), FVector(60.f, 300.f, 50.f), FVector(-200.f + b * 150.f, 0, G + 25.f)); B.Box(FVector(-200.f + b * 150.f, 0, G + 25.f), FVector(30.f, 150.f, 25.f)); }
			B.K.Add(DsSandstone(0.7f), FVector(1600.f, 40.f, 20.f), FVector(0, 250.f, G + 4.f));
			B.K.Add(DsWater(), FVector(1600.f, 24.f, 4.f), FVector(0, 250.f, G + 14.f));
			B.K.Add(DsBronze(), FVector(20.f, 20.f, 30.f), FVector(-360.f, 0, G + 200.f));
			B.N.Add(DsSunstone(3.f), FVector(12.f, 12.f, 16.f), FVector(-360.f, 0, G + 222.f));
			B.Lamp(FVector(-330.f, 0, G + 220.f), DsRGB(0xF2, 0xA2, 0x3A), 900.f, 900.f);
			return true;
		}
		if (K == TEXT("qanat_line"))
		{
			// S16: nine shaft mouths every 30 m along the hidden water's line, each in its ring of spoil with a stone lip;
			// shaft 6 open into the flooded tunnel (C2: the cave's own mouth).
			for (int32 s = 0; s < 9; ++s)
			{
				if (s == 0) continue;                                           // the first is the cave's (C2) at the site's origin
				const FVector P(s * 3000.f, 0, 0);
				const float Gz = B.G(P.X, P.Y, G);
				B.K.Ring(DsPaleSand(0.85f), FVector(P.X, P.Y, Gz - 30.f), 300.f, 160.f, 100.f, 14);
				for (int32 l = 0; l < 10; ++l) { const FRotator R(0.f, l * 36.f, 0.f); B.K.Add(DsSandstone(0.85f), FVector(50.f, 60.f, 30.f), FVector(P.X, P.Y, Gz + 30.f) + R.RotateVector(FVector(110.f, 0, 0)), R); }
				B.K.Add(FSurf::Stone(DsRGB(0x08, 0x06, 0x05), 0.95f), FVector(180.f, 180.f, 4.f), FVector(P.X, P.Y, Gz + 20.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
				B.Box(FVector(P.X, P.Y, Gz + 30.f), FVector(110.f, 110.f, 15.f));
			}
			return true;
		}
		if (K == TEXT("gardeners_row"))
		{
			// S17: a walled water garden whose channel is blocked by one fallen stone: everything below it dead (brown
			// stalks), everything above it green (the take: heave the stone).
			DsCoursedWall(B, FVector(-1200.f, -500.f, G - 20.f), FVector(1200.f, -500.f, G - 20.f), 120.f, 50.f, DsMudbrick(), 0.2f, 60.f, 22.f);
			DsCoursedWall(B, FVector(-1200.f, 500.f, G - 20.f), FVector(1200.f, 500.f, G - 20.f), 120.f, 50.f, DsMudbrick(), 0.3f, 60.f, 22.f);
			B.K.Add(DsSandstone(0.8f), FVector(2400.f, 40.f, 20.f), FVector(0, 0, G + 4.f));
			B.K.Add(DsWater(), FVector(1200.f, 24.f, 4.f), FVector(-600.f, 0, G + 14.f));
			B.K.Add(DsSandstone(0.95f), FVector(120.f, 100.f, 90.f), FVector(0, 20.f, G + 45.f), FRotator(8.f, 30.f, 12.f));
			B.Box(FVector(0, 20.f, G + 45.f), FVector(60.f, 50.f, 45.f));
			for (int32 p = 0; p < 16; ++p)
			{
				const bool bAlive = p < 8;
				const FVector P(-1100.f + p * 140.f, (p % 2 ? 1.f : -1.f) * 200.f, G);
				if (bAlive) DsPear(B, P, p * 30.f, 0.7f, p);
				else for (int32 s = 0; s < 5; ++s) B.Limb(P, P + FRotator(70.f + 10.f * s, s * 72.f, 0.f).Vector() * 70.f, 3.f, 1.f, DsRGB(0x7A, 0x5E, 0x3A) * 0.5f, 4);
			}
			return true;
		}
		if (K == TEXT("measuring_wall"))
		{
			// S18: a 40 m wall of carved marks (sun angles over years); its last third blank, the chisel still in the groove.
			DsCoursedWall(B, FVector(-2000.f, 0, G - 20.f), FVector(2000.f, 0, G - 20.f), 320.f, 80.f, DsSandstone(1.f));
			for (int32 m = 0; m < 160; ++m)
			{
				const float X = -1950.f + m * 16.f;
				if (X > 650.f) break;
				const float Hm = 60.f + 180.f * (0.5f + 0.5f * FMath::Sin(m * 0.24f));
				B.K.Add(DsSandstone(0.55f), FVector(4.f, 4.f, Hm), FVector(X, -42.f, G + 40.f + Hm * 0.5f));
			}
			B.K.Rod(DsBronzeDull(), FVector(670.f, -44.f, G + 160.f), FVector(690.f, -70.f, G + 170.f), 3.f);
			return true;
		}
		if (K == TEXT("pilgrims_ring"))
		{
			// S19: a circle of 30 kneeling stone figures facing Ahrumet; every head broken off and laid facing away.
			const FWRDesertData::FPlace* Gn = C.D.FindLandmark(TEXT("gnomon"));
			const FVector To = Gn ? C.L(Gn->At) : FVector(10000.f, 0, 0);
			const float Face = FMath::RadiansToDegrees(FMath::Atan2(To.Y, To.X));
			for (int32 p = 0; p < 30; ++p) { const FVector P = FRotator(0.f, p * 12.f, 0.f).RotateVector(FVector(900.f, 0, 0)); DsPilgrim(B, FVector(P.X, P.Y, B.G(P.X, P.Y, G)), Face, true, p); }
			return true;
		}
		if (K == TEXT("salt_cathedral"))
		{
			// S20: a salt dome eroded into arches on the Pan, cut with tool marks; a lens of salt crystal in its roof throws a
			// moving spot of sun.
			for (int32 a = 0; a < 6; ++a)
			{
				const FRotator R(0.f, a * 60.f, 0.f);
				TArray<FVector> Arch;
				for (int32 k = 0; k <= 8; ++k) { const float T = k / 8.f * PI; Arch.Add(R.RotateVector(FVector(FMath::Cos(T) * 700.f, 300.f, G + FMath::Sin(T) * 900.f))); }
				B.M.Tube(Arch, 120.f, 8, DsRGB(0xF2, 0xEF, 0xE6) * 0.72f, 0.55f);
				B.Box(R.RotateVector(FVector(700.f, 300.f, G + 300.f)), FVector(120.f, 120.f, 300.f), R);
				B.Box(R.RotateVector(FVector(-700.f, 300.f, G + 300.f)), FVector(120.f, 120.f, 300.f), R);
			}
			B.K.Add(FSurf::Glass(DsRGB(0xF4, 0xF0, 0xE8) * 0.7f, 0.4f), FVector(160.f, 160.f, 40.f), FVector(0, 0, G + 900.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			for (int32 t = 0; t < 20; ++t) B.K.Add(DsSalt(), FVector(4.f, 40.f, 4.f), FVector(600.f, (t - 10) * 30.f, G + 100.f + t * 15.f), FRotator(0.f, 60.f, 20.f));
			return true;
		}
		if (K == TEXT("avenue_of_hours"))
		{
			// S21: a 300 m processional road of 24 stelae to Ahrumet's plaza (12 a side), the run-back stone at its foot (a
			// spot of the server's).
			const FWRDesertData::FPlace* Gn = C.D.FindLandmark(TEXT("gnomon"));
			const FVector To = Gn ? C.L(Gn->At) : FVector(0, -30000.f, 0);
			const FVector Dir = To.GetSafeNormal2D(), Side(-Dir.Y, Dir.X, 0.f);
			const float L = FMath::Min(To.Size2D() - 7500.f, 30000.f);
			for (int32 s = 0; s < 12; ++s) for (int32 sd = -1; sd <= 1; sd += 2)
			{
				const FVector P = Dir * (s * L / 11.f) + Side * sd * 600.f;
				DsStela(B, FVector(P.X, P.Y, B.G(P.X, P.Y, G)), FMath::RadiansToDegrees(FMath::Atan2(Side.Y, Side.X)) + (sd > 0 ? 180.f : 0.f), 0.f, (s + (sd > 0)) % 4 == 3 ? 1 : 0, 420.f);
			}
			return true;
		}
		if (K == TEXT("sunken_greenhouse"))
		{
			// S22 (KEPT farm set-piece): a glass dome buried to its shoulders, dead off-biome plants inside, one living.
			for (int32 r = 0; r < 10; ++r)
			{
				const FRotator R(0.f, r * 18.f, 0.f);
				TArray<FVector> Rib;
				for (int32 k = 0; k <= 8; ++k) { const float T = k / 8.f * PI; Rib.Add(R.RotateVector(FVector(FMath::Cos(T) * 600.f, 0.f, G - 300.f + FMath::Sin(T) * 650.f))); }
				B.M.Tube(Rib, 5.f, 4, DsRGB(0x4A, 0x46, 0x40) * 0.55f, 0.5f);
			}
			B.K.Add(FSurf::Glass(DsRGB(0xC8, 0xD8, 0xD0) * 0.5f, 0.f), FVector(1100.f, 1100.f, 300.f), FVector(0, 0, G + 200.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			for (int32 p = 0; p < 6; ++p) B.Limb(FVector(-300.f + p * 120.f, 0, G), FVector(-300.f + p * 120.f, 30.f, G + 120.f), 3.f, 1.f, DsRGB(0x6A, 0x50, 0x38) * 0.5f, 4);
			DsPear(B, FVector(300.f, 120.f, G), 0.f, 0.6f, 5);
			return true;
		}
		if (K == TEXT("cartographers_table"))
		{
			// S23: a stone table with the old coastline carved into it: the sea where the Pan is.
			B.K.Add(DsSandstone(1.f), FVector(400.f, 260.f, 30.f), FVector(0, 0, G + 100.f));
			for (int32 l = 0; l < 4; ++l) B.K.Add(DsSandstone(0.85f), FVector(40.f, 40.f, 100.f), FVector(l < 2 ? -170.f : 170.f, l % 2 ? 100.f : -100.f, G + 50.f));
			for (int32 c = 0; c < 12; ++c) B.K.Add(DsSandstone(0.55f), FVector(30.f, 4.f, 3.f), FVector(-160.f + c * 28.f, -20.f + 40.f * FMath::Sin(c * 0.7f), G + 116.f), FRotator(0.f, 40.f * FMath::Cos(c * 0.7f), 0.f));
			for (int32 w = 0; w < 4; ++w) B.K.Add(DsSandstone(0.65f), FVector(24.f, 3.f, 2.f), FVector(-120.f + w * 70.f, 60.f, G + 116.f), FRotator(0.f, 20.f, 0.f));
			B.Box(FVector(0, 0, G + 60.f), FVector(200.f, 130.f, 60.f));
			return true;
		}
		// ---------------------------------------------------------------- D4 the Red Canyons
		if (K == TEXT("nitre_canyon"))
		{
			// S24 (KEPT farm set-piece): white nitre crusts on the red walls, a scraper left in a crust, a bat roost above.
			for (int32 n = 0; n < 14; ++n) B.K.Add(DsSalt(), FVector(200.f + 200.f * B.Rand(n), 10.f, 100.f + 120.f * B.Rand(n + 3)), FVector(-700.f + n * 100.f, (n % 2 ? 1.f : -1.f) * 500.f, G + 200.f + 300.f * B.Rand(n + 5)), FRotator(0.f, 0.f, n % 2 ? 6.f : -6.f));
			B.K.Rod(DsTimber(0.75f), FVector(100.f, -480.f, G + 260.f), FVector(160.f, -440.f, G + 200.f), 4.f);
			B.K.Add(DsBronzeDull(), FVector(4.f, 30.f, 20.f), FVector(98.f, -486.f, G + 266.f));
			B.Loop(TEXT("desert.bats"), FVector(0, 0, G + 900.f));
			return true;
		}
		if (K == TEXT("hands_wall"))
		{
			// S25: hundreds of ochre hand stencils on the cliff; one newer set of small hands in blue (indigo), at child height
			// (Amastan's people's mark: quest 3's rubbing).
			for (int32 h = 0; h < 120; ++h)
			{
				const FVector P(-1200.f + (h % 30) * 80.f + 20.f * B.Rand(h), -60.f, G + 120.f + (h / 30) * 70.f + 30.f * B.Rand(h + 9));
				B.K.Disc(FSurf::Stone(DsRGB(0xB8, 0x5A, 0x2E) * 0.5f, 0.95f), P, FVector(0, -1, 0), 22.f, 1.f);
				for (int32 f = 0; f < 4; ++f) B.K.Add(FSurf::Stone(DsRGB(0xB8, 0x5A, 0x2E) * 0.5f, 0.95f), FVector(4.f, 1.f, 14.f), P + FVector(-9.f + f * 6.f, 0, 18.f), FRotator(0.f, 0.f, (f - 1.5f) * 8.f));
			}
			for (int32 h = 0; h < 2; ++h) { const FVector P(300.f + h * 40.f, -62.f, G + 90.f); B.K.Disc(FSurf::Stone(DsRGB(0x2A, 0x34, 0x6E) * 0.6f, 0.9f), P, FVector(0, -1, 0), 14.f, 1.f); for (int32 f = 0; f < 4; ++f) B.K.Add(FSurf::Stone(DsRGB(0x2A, 0x34, 0x6E) * 0.6f, 0.9f), FVector(3.f, 1.f, 9.f), P + FVector(-6.f + f * 4.f, 0, 12.f), FRotator(0.f, 0.f, (f - 1.5f) * 8.f)); }
			B.K.Add(DsRedRock(0.95f), FVector(2800.f, 100.f, 800.f), FVector(-100.f, 0, G + 380.f));
			B.Box(FVector(-100.f, 0, G + 380.f), FVector(1400.f, 50.f, 400.f));
			return true;
		}
		if (K == TEXT("copper_drift"))
		{
			// S26: a Sunwatch copper mine: green malachite banded walls, wooden ladders down a shaft, a cage of bones where a
			// Glasstail nest took the miners.
			for (int32 b = 0; b < 10; ++b) B.K.Add(FSurf::Stone(DsRGB(0x3A, 0x8A, 0x5A) * 0.5f, 0.7f), FVector(600.f, 20.f, 30.f), FVector(-300.f, -400.f, G + 100.f + b * 50.f), FRotator(0.f, 0.f, 3.f * b));
			for (int32 sd = -1; sd <= 1; sd += 2) B.K.Rod(DsTimber(0.7f), FVector(200.f, sd * 30.f, G - 200.f), FVector(200.f, sd * 30.f, G + 300.f), 6.f);
			for (int32 r = 0; r < 14; ++r) B.K.Rod(DsTimber(0.65f), FVector(200.f, -30.f, G - 180.f + r * 35.f), FVector(200.f, 30.f, G - 180.f + r * 35.f), 4.f);
			for (int32 c = 0; c < 8; ++c) B.K.Rod(DsBronzeDull(), FVector(-100.f + FMath::Cos(c * 0.785f) * 80.f, 300.f + FMath::Sin(c * 0.785f) * 80.f, G), FVector(-100.f + FMath::Cos(c * 0.785f) * 80.f, 300.f + FMath::Sin(c * 0.785f) * 80.f, G + 200.f), 3.f);
			for (int32 b = 0; b < 3; ++b) DsDromadBones(B, FVector(-100.f, 300.f, G), b * 120.f, false, b, 80.f);
			return true;
		}
		if (K == TEXT("flood_line"))
		{
			// S27: a slot canyon with a dead tree jammed 6 m up its walls and debris lodged at the same height: this shows how
			// high the flood comes.
			B.Limb(FVector(-400.f, -400.f, G + 600.f), FVector(400.f, 380.f, G + 620.f), 20.f, 10.f, DsDriftC(), 7);
			for (int32 r = 0; r < 5; ++r) B.Limb(FVector(-400.f, -400.f, G + 600.f), FVector(-400.f, -400.f, G + 600.f) + FRotator(r * 50.f - 100.f, -130.f + r * 20.f, 0.f).Vector() * 150.f, 8.f, 2.f, DsDriftC() * 0.9f, 5);
			for (int32 d = 0; d < 20; ++d) B.Limb(FVector(-600.f + d * 60.f, (d % 2 ? 1.f : -1.f) * 420.f, G + 600.f), FVector(-560.f + d * 60.f, (d % 2 ? 1.f : -1.f) * 400.f, G + 610.f + 20.f * B.Rand(d)), 4.f, 2.f, DsDriftC() * 0.85f, 4);
			B.SegBox(FVector(-400.f, -400.f, G + 600.f), FVector(400.f, 380.f, G + 620.f), 20.f, 20.f);
			return true;
		}
		if (K == TEXT("kite_cliffs"))
		{
			// S28: a cliff face of Carrion kite nests, bones dropped below in a fan (lammergeiers drop bones to break them).
			B.K.Add(DsRedRock(0.9f), FVector(2000.f, 200.f, 2400.f), FVector(0, -400.f, G + 1100.f));
			B.Box(FVector(0, -400.f, G + 1100.f), FVector(1000.f, 100.f, 1200.f));
			for (int32 n = 0; n < 6; ++n) { const FVector P(-800.f + n * 320.f, -300.f, G + 900.f + 900.f * B.Rand(n)); for (int32 s = 0; s < 8; ++s) B.Limb(P + FRotator(0.f, s * 45.f, 0.f).RotateVector(FVector(120.f, 0, 0)), P + FRotator(0.f, s * 45.f + 100.f, 0.f).RotateVector(FVector(130.f, 0, 10.f)), 5.f, 3.f, DsRGB(0x6A, 0x58, 0x44) * 0.5f, 4); B.K.Add(FSurf::Stone(DsRGB(0xB8, 0x4A, 0x2E) * 0.5f, 0.95f), FVector(300.f, 10.f, 500.f), P + FVector(0, 10.f, -300.f)); }
			for (int32 b = 0; b < 40; ++b) { const float A = (-60.f + 120.f * B.Rand(b)) * PI / 180.f, D = 200.f + 800.f * B.Rand(b + 7); B.K.Add(DsBone(0.95f), FVector(60.f, 10.f, 8.f), FVector(FMath::Sin(A) * D, FMath::Cos(A) * D * 0.6f, B.G(FMath::Sin(A) * D, FMath::Cos(A) * D * 0.6f, G) + 4.f), FRotator(0.f, b * 37.f, 0.f)); }
			return true;
		}
		if (K == TEXT("lookout"))
		{
			// S29: a spire-top Sunwatch watchpost: a hoodoo with a stair, a small stone hut, and a bronze signal mirror on a
			// pivot pointing at Saltgate (it turns with the sun: the moving part).
			const float H = 1600.f;
			DsHoodoo(B, FVector(0, 0, G), H, 600.f, 12, false);
			B.Box(FVector(0, 0, G + H * 0.5f), FVector(450.f, 450.f, H * 0.5f));
			for (int32 s = 0; s < 50; ++s) { const float A = s * 14.f * PI / 180.f, Z = G + 40.f + s * (H - 80.f) / 50.f; const float R = 620.f - 180.f * (Z - G) / H + 40.f; const FVector P(FMath::Cos(A) * R, FMath::Sin(A) * R, Z); B.K.Add(DsRedRock(0.8f), FVector(100.f, 70.f, 24.f), P, FRotator(0.f, s * 14.f + 90.f, 0.f)); B.Box(P, FVector(50.f, 35.f, 12.f), FRotator(0.f, s * 14.f + 90.f, 0.f)); }
			B.K.Add(DsCaprock(), FVector(800.f, 800.f, 50.f), FVector(0, 0, G + H), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			B.Box(FVector(0, 0, G + H), FVector(400.f, 400.f, 25.f));
			DsCoursedWall(B, FVector(-250.f, -200.f, G + H + 20.f), FVector(-250.f, 200.f, G + H + 20.f), 180.f, 40.f, DsSandstone(0.9f), 0.2f, 60.f, 30.f);
			C.A->Signal = DsMirrorPart(B, FVector(150.f, 0, G + H + 20.f), 90.f, 70.f);
			return true;
		}
		// ---------------------------------------------------------------- D5 the Haboob Wall
		if (K == TEXT("last_caravan"))
		{
			// S30: a full caravan in the storm, sand-blasted to bare wood and metal, its beasts' bones roped in a ring round it.
			DsCaravan(B, FVector(0, 0, G), 0.f, 0, 3);
			DsCaravan(B, FVector(500.f, 400.f, B.G(500.f, 400.f, G)), 40.f, 0, 4);
			for (int32 d = 0; d < 7; ++d) { const FVector P = FRotator(0.f, d * 51.f, 0.f).RotateVector(FVector(900.f, 0, 0)); DsDromadBones(B, FVector(P.X, P.Y, B.G(P.X, P.Y, G)), d * 51.f + 90.f, d % 3 == 0, d, 40.f); }
			TArray<FVector> Ring;
			for (int32 d = 0; d <= 14; ++d) { const FVector P = FRotator(0.f, d * 25.7f, 0.f).RotateVector(FVector(820.f, 0, 0)); Ring.Add(FVector(P.X, P.Y, B.G(P.X, P.Y, G) + 60.f)); }
			for (int32 d = 0; d < 14; ++d) B.Line(Ring[d], Ring[d + 1], 2.f, 20.f, DsRGB(0x9C, 0x84, 0x60) * 0.4f, 4);
			return true;
		}
		if (K == TEXT("sieves"))
		{
			// S31: sinkholes with dunes pouring into the Fungal caverns' skylights (the KEPT border; the holes are the Fungal
			// land's): the sand's fall and the glow from below at night.
			for (int32 s = 0; s < 3; ++s) { const FVector P(-350.f + s * 380.f, 200.f * (s % 2), G - 400.f); B.Loop(TEXT("fungal.sandFall"), P, FRotator::ZeroRotator, 0.25f); }
			B.N.Add(FSurf::Lit(DsRGB(0x39, 0xE6, 0xC2) * 0.6f, 2.f), FVector(400.f, 400.f, 4.f), FVector(0, 100.f, G - 1200.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			return true;
		}
		if (K == TEXT("storm_tower"))
		{
			// S32: a Sunwatch tower whose top 8 m stick out of the sand; its windward windows packed solid with sand, the lee
			// ones open (the lens recipe lies inside on the lee side: the spot).
			for (int32 c = 0; c < 18; ++c) B.K.Add(DsSandstone(0.8f + 0.1f * B.Rand(c)), FVector(600.f, 600.f, 46.f), FVector(0, 0, G - 40.f + c * 48.f));
			for (int32 w = 0; w < 4; ++w)
			{
				const FRotator R(0.f, w * 90.f, 0.f);
				const bool bWindward = w == 0;                                  // east
				B.K.Add(bWindward ? DsSandFill(0.95f) : FSurf::Stone(DsRGB(0x10, 0x0C, 0x0A), 0.95f), FVector(10.f, 120.f, 180.f), R.RotateVector(FVector(302.f, 0, 0)) + FVector(0, 0, G + 500.f), R);
			}
			B.K.Add(DsSandstone(0.9f), FVector(680.f, 680.f, 40.f), FVector(0, 0, G + 840.f));
			B.K.Add(DsSandFill(1.f), FVector(1600.f, 1200.f, 260.f), FVector(500.f, 0, G - 60.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			B.Box(FVector(0, 0, G + 400.f), FVector(300.f, 300.f, 440.f));
			return true;
		}
		if (K == TEXT("glass_field"))
		{
			// S33: fulgurite tubes where the Wall's lightning hits the sand: branching glass roots sticking up (the sunglass
			// nodes stand among them).
			for (int32 f = 0; f < 40; ++f)
			{
				const float A = B.Rand(f) * 2.f * PI, D = 3400.f * FMath::Sqrt(B.Rand(f + 11));
				const FVector P(FMath::Cos(A) * D, FMath::Sin(A) * D, 0.f);
				const float Gz = B.G(P.X, P.Y, G);
				for (int32 b = 0; b < 3; ++b) B.K.Spike(b == 0 ? DsSunglass() : DsGlassBlack(), FVector(P.X, P.Y, Gz - 10.f), FVector(0.3f * (b - 1), 0.2f * (b % 2), 1.f).GetSafeNormal(), 10.f + 6.f * B.Rand(f + b), 60.f + 80.f * B.Rand(f * 3 + b));
			}
			return true;
		}
		if (K == TEXT("faceless_sentinels"))
		{
			// S34: two 8 m guardian statues sand-blasted smooth, no faces left.
			for (int32 s = -1; s <= 1; s += 2)
			{
				const FVector P(0, s * 600.f, 0.f);
				const float Gz = B.G(P.X, P.Y, G);
				B.K.Add(DsSandstone(0.8f), FVector(220.f, 220.f, 120.f), FVector(P.X, P.Y, Gz + 60.f));
				B.M.Lathe(FVector(P.X, P.Y, Gz + 120.f), FVector::UpVector, 680.f, { FVector(0.f, 90.f, 70.f), FVector(0.4f, 100.f, 70.f), FVector(0.75f, 110.f, 80.f), FVector(0.85f, 50.f, 50.f), FVector(1.f, 60.f, 60.f) }, 10, DsRGB(0xC8, 0xA0, 0x70) * 0.52f, 0.95f);
				B.Ball(FVector(P.X, P.Y, Gz + 830.f), 70.f, DsRGB(0xC8, 0xA0, 0x70) * 0.5f, 0.95f, 1.2f);
				B.Box(FVector(P.X, P.Y, Gz + 450.f), FVector(110.f, 90.f, 450.f));
			}
			return true;
		}
		if (K == TEXT("storm_wreck"))
		{
			// S35: a newcomer skiff dragged 1 km inland by storms, its sail wrapped round a stela.
			DsHull(B, FVector(0, 0, G), FRotator(0.f, 20.f, 40.f), 900.f, 260.f, 140.f, 0, 40.f, 0.3f, 35);
			DsStela(B, FVector(500.f, 300.f, B.G(500.f, 300.f, G)), 90.f, 6.f, 1, 380.f);
			for (int32 w = 0; w < 4; ++w) B.K.Add(DsLinen(0.6f + 0.1f * w), FVector(140.f, 60.f, 70.f), FVector(500.f, 300.f, B.G(500.f, 300.f, G) + 120.f + w * 60.f), FRotator(0.f, w * 70.f, 10.f), EWRLookMesh::Cylinder);
			return true;
		}
		return false;
	}

	bool Kit(FDsCtx& C)
	{
		FBuild& B = C.B;
		const FName K = C.A->Kind;
		const float G = B.G(0, 0, 0.f);
		const int32 V = C.A->Variant;
		const int32 Seed = C.A->Index * 7 + 3;
		const FWRDesertData::FPlace* P = C.D.Kit.IsValidIndex(C.A->Index) ? &C.D.Kit[C.A->Index] : nullptr;
		if (K == TEXT("wreck_fragment"))
		{
			// A wreck fragment: a newcomer's broken bow on the strand (0), an ancient hull's ribs in the Erg (1), a keel and a
			// few strakes in the storm (2).
			const bool bStrand = C.A->Zone == TEXT("D1");
			if (bStrand || V == 0) DsHull(B, FVector(0, 0, G), FRotator(-8.f, 0.f, 25.f), 1100.f + 300.f * B.Rand(Seed), 380.f, 200.f, 0, 120.f, 0.6f, Seed);
			else DsHull(B, FVector(0, 0, G), FRotator(-4.f, 0.f, 10.f), 1600.f, 420.f, 240.f, V == 1 ? 2 : 1, 220.f, 0.9f, Seed);
			return true;
		}
		if (K == TEXT("caravan")) { DsCaravan(B, FVector(0, 0, G), C.A->GetActorRotation().Yaw * 0.f, V, Seed); return true; }
		if (K == TEXT("stela"))
		{
			// A gnomon stela on an old road (world-map's Sunwatch waymark), facing along the road; the seven tilted ones lean
			// 4 degrees (the Long Noon's shadows).
			DsStela(B, FVector(0, 0, G), 90.f, P && P->bTilted ? 4.f : 0.f, V);
			return true;
		}
		if (K == TEXT("ruin")) { DsRuin(B, FVector(0, 0, G), 0.f, V % 3, Seed); return true; }
		if (K == TEXT("outcrop")) { DsOutcrop(B, FVector(0, 0, G), 0.f, P && !P->Ore.IsNone() ? P->Ore : FName(TEXT("sunstone")), Seed); return true; }
		if (K == TEXT("creature_set"))
		{
			// A creature set-piece: a kill (0), a burrow mound with a Scarab's sand ball (1), a nest of sticks (2), a herd's
			// water trough at a seep (3). The living creatures are the populations'.
			if (V == 0) { DsDromadBones(B, FVector(0, 0, G), 0.f, false, Seed, 20.f); B.K.Add(FSurf::Glaze(DsRGB(0x5A, 0x10, 0x0C) * 0.5f, 0.4f), FVector(300.f, 220.f, 2.f), FVector(0, 0, G + 1.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder); }
			else if (V == 1) { B.K.Add(DsSandFill(0.9f), FVector(400.f, 400.f, 120.f), FVector(0, 0, G), FRotator::ZeroRotator, EWRLookMesh::Cylinder); B.Ball(FVector(200.f, 0, G + 50.f), 50.f, DsRGB(0xB8, 0x8A, 0x50) * 0.5f, 0.95f); B.K.Add(FSurf::Stone(DsRGB(0x10, 0x0C, 0x0A), 0.95f), FVector(70.f, 70.f, 4.f), FVector(0, 0, G + 62.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder); }
			else if (V == 2) { for (int32 s = 0; s < 12; ++s) B.Limb(FRotator(0.f, s * 30.f, 0.f).RotateVector(FVector(120.f, 0, G + 10.f)), FRotator(0.f, s * 30.f + 110.f, 0.f).RotateVector(FVector(140.f, 0, G + 20.f)), 5.f, 3.f, DsRGB(0x6A, 0x58, 0x44) * 0.5f, 4); for (int32 e = 0; e < 3; ++e) B.Ball(FVector(e * 20.f - 20.f, 10.f, G + 20.f), 10.f, DsRGB(0xE8, 0xD8, 0xB0) * 0.6f, 0.5f, 1.2f); }
			else { DsSeep(B, FVector(0, 0, G), 0.f, 0, Seed); B.K.Add(DsSandstone(0.8f), FVector(400.f, 80.f, 60.f), FVector(400.f, 200.f, G + 30.f)); }
			return true;
		}
		if (K == TEXT("seep")) { DsSeep(B, FVector(0, 0, G), 0.f, V, Seed); return true; }
		if (K == TEXT("cave_mouth"))
		{
			// A slot's entrance in a red wall (no way through: a shallow overhang the wind cut, shade at noon, a cache of
			// someone's sheltering).
			B.K.Add(DsRedRock(0.9f), FVector(800.f, 300.f, 900.f), FVector(0, -300.f, G + 400.f));
			B.K.Add(DsRedRock(0.7f), FVector(800.f, 500.f, 120.f), FVector(0, -100.f, G + 500.f));
			B.K.Add(DsVarnish(), FVector(600.f, 10.f, 500.f), FVector(0, -148.f, G + 600.f));
			B.Box(FVector(0, -300.f, G + 400.f), FVector(400.f, 150.f, 450.f));
			B.Box(FVector(0, -100.f, G + 500.f), FVector(400.f, 250.f, 60.f));
			B.K.Add(FSurf::Stone(DsRGB(0x2A, 0x24, 0x20) * 0.6f, 0.95f), FVector(90.f, 90.f, 4.f), FVector(100.f, 0, G + 2.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			return true;
		}
		if (K == TEXT("bones"))
		{
			// Bones: a whale (0), a Dromad (1), a giant's (2: a ribcage 6 m tall).
			if (V == 0) DsWhale(B, FVector(0, 0, G), 200.f, 1800.f + 600.f * B.Rand(Seed), Seed, 0.6f);
			else if (V == 1) DsDromadBones(B, FVector(0, 0, G), 0.f, false, Seed, 30.f);
			else for (int32 r = 0; r < 10; ++r) { TArray<FVector> Arc; for (int32 k = 0; k <= 6; ++k) { const float A = k / 6.f * 2.f; Arc.Add(FVector(-450.f + r * 100.f, FMath::Sin(A) * 300.f * (r % 2 ? 1.f : -1.f), G - 40.f + FMath::Cos(A) * 500.f + 100.f)); } B.M.Tube(Arc, 12.f, 6, DsBoneC(), 0.75f); }
			return true;
		}
		return false;
	}
}
