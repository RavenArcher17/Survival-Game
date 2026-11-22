// The kit POIs (cooks/cookpass-world.md 3.6): the small places the generator deals along every route so no stretch goes
// 250 m without one (Tools/terrain/island_places.mjs), each a 10-30 m story, its variant (v 0..3) from the file; and the
// fumarole and geyser spots of the Solfatara. Volcanic kinds: sulfur_vent, hornito, smith_grave, giant_tool,
// glowmoth_roost, basalt_ring, miner_leanto. Tundra kinds: tent_ring, erratic, snowpuff_nest, cache_pole,
// frozen_carcass, small_pingo.
#include "WRIslandPlacesLook.h"
#include "WRIslandPlaces.h"
#include "WRTuning.h"
#include "WRLand.h"

using WRLookKit::FSurf;

namespace WRIsleK
{
	using WRIsleLook::Lin;

	struct FPal
	{
		FSurf Basalt = FSurf::Stone(Lin(30, 27, 25), 0.9f);
		FSurf BasaltAlt = FSurf::Stone(Lin(52, 44, 38), 0.85f);
		FSurf Slag = FSurf::Glaze(Lin(43, 37, 34), 0.35f);
		FSurf Ash = FSurf::Stone(Lin(110, 106, 102), 0.95f);
		FSurf Iron = FSurf::Steel(Lin(42, 38, 36), 0.55f);
		FSurf Rust = FSurf::Steel(Lin(138, 75, 42), 0.8f);
		FSurf Sulfur = FSurf::Glaze(Lin(217, 194, 48), 0.6f);
		FSurf Char = FSurf::Wood(Lin(36, 30, 27));
		FSurf Wood = FSurf::Wood(Lin(110, 90, 68));
		FSurf Canvas = FSurf::Cloth(Lin(168, 154, 128));
		FSurf Rope = FSurf::Cloth(Lin(120, 104, 80));
		FSurf Dark = FSurf::Stone(Lin(8, 7, 7), 0.98f);
		FSurf Cocoon = FSurf::Lit(Lin(214, 240, 190), 0.15f);
		FSurf Snow = FSurf::Stone(Lin(226, 232, 238), 0.9f).WithPattern(WRLookPattern::Plaster);
		FSurf SnowShade = FSurf::Stone(Lin(200, 214, 228), 0.9f);
		FSurf Ice = FSurf::Ice(Lin(196, 222, 236));
		FSurf DeepIce = FSurf::Ice(Lin(150, 194, 222));
		FSurf Granite = FSurf::Stone(Lin(185, 196, 206), 0.85f);
		FSurf GraniteDark = FSurf::Stone(Lin(120, 132, 146), 0.88f);
		FSurf Carve = FSurf::Stone(Lin(70, 78, 90), 0.9f);
		FSurf Hide = FSurf::Cloth(Lin(122, 96, 72));
		FSurf HideDark = FSurf::Cloth(Lin(84, 64, 48));
		FSurf Willow = FSurf::Wood(Lin(138, 122, 98));
		FSurf Bone = FSurf::Stone(Lin(214, 208, 190), 0.6f);
		FSurf Grass = FSurf::Cloth(Lin(150, 128, 82));
		FSurf Feather = FSurf::Cloth(Lin(235, 235, 228));
		FSurf Red = FSurf::Cloth(Lin(198, 40, 40));
		FSurf Lichen = FSurf::Cloth(Lin(184, 145, 59));
		FSurf Fur = FSurf::Cloth(Lin(150, 132, 108));
	};

	// --------------------------------------------------------------------------------------------------- Volcanic

	void SulfurVent(FWRIsleSite& S, const FPal& P)
	{
		// Three or four vent mouths on a yellow crust; every other cluster has one that bursts (a fumarole); blue at night.
		const int32 N = 3 + (S.Variant % 2);
		for (int32 k = 0; k < 5; ++k)
		{
			const FVector At = FRotator(0.f, 72.f * k, 0.f).Vector() * (180.f + 120.f * (k % 2));
			S.K.Add(P.Sulfur.Dim(0.7f + 0.3f * S.R(At, 1)), FVector(500.f, 360.f, 10.f), S.G(At.X, At.Y, 2.f), FRotator(0.f, 72.f * k, 0.f), EWRLookMesh::Cylinder);
		}
		for (int32 v = 0; v < N; ++v)
		{
			const FVector D = FRotator(0.f, 360.f / N * v + 25.f * S.Variant, 0.f).Vector() * (120.f + 50.f * v);
			const FVector G = S.G(D.X, D.Y);
			for (int32 k = 0; k < 7; ++k)
			{
				const FVector L = G + FRotator(0.f, 51.4f * k, 0.f).Vector() * 48.f + FVector(0, 0, 12.f);
				S.K.Add((k % 2 ? P.Basalt : P.Sulfur).Dim(0.8f + 0.3f * S.R(L, 2)), FVector(46.f, 34.f, 26.f), L, FRotator(10.f * S.J(L, 3), 51.4f * k, 10.f * S.J(L, 4)));
			}
			S.K.Add(P.Dark, FVector(56.f, 56.f, 6.f), G + FVector(0, 0, 6.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			S.Fx(G + FVector(0, 0, 20.f), TEXT("ff.vent.wisp"), 0.5f);
			if (v == 0)
			{
				S.NightFx(G + FVector(0, 0, 14.f), TEXT("ff.blue_fire"), 0.6f);
				S.Lamp(G + FVector(0, 0, 70.f), Lin(58, 108, 255), WRTune(TEXT("places.blueFireLux"), 3200.f) * 0.6f, 800.f);
				if (S.Variant % 2 == 0) S.Hazard(FWRIsleHazard::EKind::Fumarole, G + FVector(0, 0, 4.f), 200.f);
			}
		}
	}

	void SmithGrave(FWRIsleSite& S, const FPal& P)
	{
		// A smith's grave: a long low mound of slag and ash, a stake at its head with an iron hammer head lashed to it; a
		// sword thrust in the mound on one in three.
		const FVector G = S.G(0, 0);
		S.K.Add(P.Ash.Dim(0.85f), FVector(220.f, 110.f, 50.f), G + FVector(0, 0, 10.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		for (int32 k = 0; k < 8; ++k)
		{
			const FVector At = G + FVector(-90.f + 26.f * k, 60.f * ((k % 2) ? 1 : -1), 16.f);
			S.K.Add(P.Slag, FVector(40.f, 34.f, 24.f), At, FRotator(0.f, 40.f * k, 10.f * S.J(At, 1)));
		}
		const FVector Stake = G + FVector(140.f, 0, 0);
		S.K.Rod(P.Char, Stake - FVector(0, 0, 40.f), Stake + FVector(0, 0, 180.f), 10.f);
		S.K.Add(P.Iron, FVector(30.f, 60.f, 28.f), Stake + FVector(0, 0, 170.f), FRotator(0.f, 90.f, 0.f));
		S.K.Add(P.Rope.Dim(0.7f), FVector(16.f, 16.f, 30.f), Stake + FVector(0, 0, 150.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		if (S.Variant % 3 == 0)
		{
			S.K.Add(P.Iron, FVector(6.f, 14.f, 110.f), G + FVector(-20.f, 0, 70.f), FRotator(6.f, 0.f, 4.f));
			S.K.Add(P.Iron, FVector(10.f, 46.f, 6.f), G + FVector(-20.f, 0, 120.f), FRotator(6.f, 0.f, 4.f));
		}
	}

	void GiantTool(FWRIsleSite& S, const FPal& P)
	{
		// One of the forge giant's tools thrown down and half-sunk: tongs, a rasp or a chisel, 6-9 m long.
		const int32 V = S.Variant % 3;
		const FVector G = S.G(0, 0);
		if (V == 0)
		{
			// Tongs: two arms crossing at a rivet, one jaw buried.
			const FVector Pin = G + FVector(0, 0, 60.f);
			S.K.Rod(P.Iron, Pin + FVector(-420.f, -70.f, 60.f), Pin + FVector(350.f, 40.f, -100.f), 46.f, true);
			S.K.Rod(P.Iron, Pin + FVector(-420.f, 70.f, 50.f), Pin + FVector(350.f, -40.f, -90.f), 46.f, true);
			S.K.Add(P.Rust, FVector(70.f, 70.f, 100.f), Pin, FRotator(90.f, 0.f, 0.f), EWRLookMesh::Cylinder);
			S.Solid(Pin, FVector(390.f, 60.f, 50.f));
		}
		else if (V == 1)
		{
			// A rasp: a long flat file with cross-cut teeth, its tang in the air.
			S.K.Add(P.Iron, FVector(700.f, 90.f, 26.f), G + FVector(0, 0, 40.f), FRotator(-8.f, 0.f, 5.f));
			for (int32 t = 0; t < 16; ++t) S.K.Add(P.Rust, FVector(10.f, 92.f, 6.f), G + FVector(-320.f + 40.f * t, 0, 54.f + 5.6f * t * 0.f), FRotator(-8.f, 20.f, 5.f));
			S.K.Rod(P.Iron, G + FVector(350.f, 0, 90.f), G + FVector(560.f, 0, 120.f), 34.f, true);
			S.Solid(G + FVector(0, 0, 40.f), FVector(350.f, 45.f, 20.f), FRotator(-8.f, 0.f, 5.f));
		}
		else
		{
			// A chisel driven point-down into the rock as if the giant left it mid-cut.
			S.K.Rod(P.Iron, G - FVector(0, 0, 150.f), G + FVector(60.f, 30.f, 520.f), 70.f, true);
			S.K.Add(P.Rust, FVector(110.f, 110.f, 70.f), G + FVector(62.f, 31.f, 540.f), FRotator(6.f, 0.f, 3.f), EWRLookMesh::Cylinder);
			S.Solid(G + FVector(30.f, 15.f, 200.f), FVector(40.f, 40.f, 220.f));
			for (int32 k = 0; k < 6; ++k) S.Rock(k, FVector(110.f, 90.f, 50.f), G + FRotator(0.f, 60.f * k, 0.f).Vector() * 140.f, FRotator(0.f, 60.f * k, 0.f), Lin(30, 27, 25) * 0.6f, false, 20.f);
		}
	}

	void GlowmothRoost(FWRIsleSite& S, const FPal& P)
	{
		// A dead charred tree with glowmoth cocoons hung from its branches: they glow softly at night.
		const FVector G = S.G(0, 0);
		const float H = 700.f + 200.f * (S.Variant % 2);
		S.K.Rod(P.Char, G - FVector(0, 0, 40.f), G + FVector(20.f, 10.f, H), 44.f);
		const int32 N = 6 + S.Variant;
		for (int32 b = 0; b < 5; ++b)
		{
			const FVector At = G + FVector(0, 0, H * (0.4f + 0.12f * b));
			const FVector End = At + FRotator(0.f, 72.f * b + 30.f * S.Variant, 0.f).Vector() * (260.f - 30.f * b) + FVector(0, 0, 60.f);
			S.K.Rod(P.Char.Dim(0.9f), At, End, 14.f - 2.f * b);
		}
		for (int32 c = 0; c < N; ++c)
		{
			const FVector At = G + FRotator(0.f, 360.f / N * c + 17.f, 0.f).Vector() * (110.f + 50.f * (c % 3)) + FVector(0, 0, H * 0.4f + 90.f * (c % 4));
			S.K.Add(P.Cocoon, FVector(14.f, 14.f, 30.f), At, FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			S.K.Rod(P.Rope, At + FVector(0, 0, 15.f), At + FVector(0, 0, 60.f), 2.f);
			if (c % 2 == 0) S.Glowmoth(At, float(c) * 0.7f + S.Variant);
		}
		S.Solid(G + FVector(0, 0, H * 0.5f), FVector(26.f, 26.f, H * 0.5f));
	}

	void BasaltRing(FWRIsleSite& S, const FPal& P)
	{
		// Hexagonal basalt columns standing in a ring (the cooled flow cracked into colonnades), 1.5-4 m.
		const int32 N = 7 + 2 * (S.Variant % 3);
		for (int32 c = 0; c < N; ++c)
		{
			const float A = 360.f / N * c;
			const FVector D = FRotator(0.f, A, 0.f).Vector() * (350.f + 80.f * S.J(FVector(c), 1));
			const float H = 150.f + 250.f * S.R(D, 2);
			const float Z0 = S.Gz(D.X, D.Y) - 30.f;
			S.K.Hex(c % 3 ? P.Basalt : P.BasaltAlt, D.X, D.Y, Z0, Z0 + H, 26.f, 30.f * c);
			S.K.Add(P.BasaltAlt.Dim(0.8f), FVector(52.f, 52.f, 6.f), FVector(D.X, D.Y, Z0 + H + 3.f), FRotator(0.f, 30.f * c, 0.f), EWRLookMesh::Cylinder);
			if (H > 250.f) S.Solid(FVector(D.X, D.Y, Z0 + H * 0.5f), FVector(28.f, 28.f, H * 0.5f));
		}
		// One fallen across the middle.
		S.K.Hex(P.BasaltAlt, 0.f, 0.f, S.Gz(0, 0) + 20.f, S.Gz(0, 0) + 70.f, 120.f, 10.f * S.Variant);
	}

	void MinerLeanto(FWRIsleSite& S, const FPal& P)
	{
		// A miner's lean-to: a canvas sheet on two poles, a basket of sulfur, a pick and a pole in a rack, a small fire.
		const FVector G = S.G(0, 0);
		for (const float Sd : { -1.f, 1.f }) S.K.Rod(P.Wood, G + FVector(60.f, Sd * 150.f, -30.f), G + FVector(60.f, Sd * 150.f, 170.f), 8.f);
		S.K.Rod(P.Wood, G + FVector(60.f, -170.f, 170.f), G + FVector(60.f, 170.f, 170.f), 7.f);
		S.K.Add(P.Canvas, FVector(220.f, 330.f, 4.f), G + FVector(-30.f, 0, 90.f), FRotator(-40.f, 0.f, 0.f));
		S.K.Add(P.Rope.Dim(0.85f), FVector(60.f, 60.f, 50.f), G + FVector(0, -90.f, 25.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		S.K.Add(P.Sulfur, FVector(50.f, 50.f, 14.f), G + FVector(0, -90.f, 52.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		S.K.Rod(P.Wood, G + FVector(30.f, 80.f, 0.f), G + FVector(40.f, 100.f, 120.f), 5.f);
		S.K.Add(P.Iron, FVector(50.f, 8.f, 8.f), G + FVector(40.f, 100.f, 120.f), FRotator(0.f, 90.f, 20.f));
		const FVector F = G + FVector(200.f, 0, 0);
		for (int32 k = 0; k < 7; ++k) S.K.Add(P.Basalt, FVector(26.f, 22.f, 16.f), F + FRotator(0.f, 51.f * k, 0.f).Vector() * 45.f + FVector(0, 0, 8.f), FRotator(0.f, 51.f * k, 0.f));
		S.NightFx(F + FVector(0, 0, 10.f), TEXT("brazier.fire"), 0.45f);
		S.Lamp(F + FVector(0, 0, 50.f), Lin(255, 150, 70), 2200.f, 900.f);
	}

	// --------------------------------------------------------------------------------------------------- Tundra

	void TentRing(FWRIsleSite& S, const FPal& P)
	{
		// Where a Rime-folk lavvu stood: the hearth ring of stones, the stubs of its poles in a 4 m circle, the hide fallen in,
		// a drying rack beside.
		const FVector G = S.G(0, 0);
		for (int32 k = 0; k < 9; ++k) S.K.Add(P.GraniteDark, FVector(36.f, 30.f, 22.f), G + FRotator(0.f, 40.f * k, 0.f).Vector() * 60.f + FVector(0, 0, 8.f), FRotator(0.f, 40.f * k, 0.f));
		S.K.Add(P.Dark, FVector(80.f, 80.f, 4.f), G + FVector(0, 0, 3.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		for (int32 p = 0; p < 10; ++p)
		{
			const FVector At = G + FRotator(0.f, 36.f * p, 0.f).Vector() * 200.f;
			const float H = 30.f + 60.f * S.R(At, 1) + (p == 3 ? 180.f : 0.f);
			S.K.Rod(P.Willow.Dim(0.8f), FVector(At.X, At.Y, S.Gz(At.X - G.X * 0.f, At.Y) - 20.f), FVector(At.X, At.Y, S.Gz(At.X, At.Y) + H) - FRotator(0.f, 36.f * p, 0.f).Vector() * (H * 0.3f), 8.f);
		}
		S.K.Add(P.Hide.Dim(0.85f), FVector(260.f, 200.f, 8.f), G + FVector(-40.f, 30.f, 14.f), FRotator(6.f, 25.f * S.Variant, -4.f));
		S.K.Add(P.Snow, FVector(240.f, 160.f, 20.f), G + FVector(-60.f, 40.f, 22.f), FRotator(6.f, 25.f * S.Variant + 10.f, -4.f), EWRLookMesh::Cylinder);
		const FVector R = G + FVector(320.f, 0, 0);
		for (const float Sd : { -1.f, 1.f }) S.K.Rod(P.Willow, R + FVector(0, Sd * 120.f, -20.f), R + FVector(0, Sd * 120.f, 150.f), 6.f);
		S.K.Rod(P.Willow, R + FVector(0, -130.f, 145.f), R + FVector(0, 130.f, 145.f), 5.f);
		if (S.Variant % 2 == 0) S.K.Add(P.HideDark, FVector(4.f, 120.f, 80.f), R + FVector(0, 0, 100.f));
	}

	void Erratic(FWRIsleSite& S, const FPal& P)
	{
		// A house-sized boulder the ice left here, a spiral carved on its face (a Rime-folk mark), snow on its back.
		const FVector G = S.G(0, 0);
		S.Rock(S.Variant, FVector(520.f + 120.f * (S.Variant % 2), 420.f, 360.f + 80.f * (S.Variant % 3)), G, FRotator(0.f, 30.f * S.Variant, 4.f), Lin(150, 160, 172), true, 40.f);
		const FVector Face = G + FVector(270.f, 0, 170.f);
		for (int32 k = 0; k < 18; ++k)
		{
			const float T = k / 18.f;
			const float A = 720.f * T;
			const float R = 10.f + 60.f * T;
			S.K.Add(P.Carve, FVector(4.f, 14.f, 8.f), Face + FVector(0, FMath::Cos(FMath::DegreesToRadians(A)) * R, FMath::Sin(FMath::DegreesToRadians(A)) * R), FRotator(0.f, 0.f, A));
		}
		S.K.Add(P.Snow, FVector(380.f, 300.f, 50.f), G + FVector(-30.f, 0, 330.f + 80.f * (S.Variant % 3)), FRotator(4.f, 30.f * S.Variant, 0.f), EWRLookMesh::Cylinder);
		S.K.Add(P.Lichen.Dim(0.8f), FVector(4.f, 80.f, 50.f), G + FVector(-260.f, 40.f, 120.f));
	}

	void SnowpuffNest(FWRIsleSite& S, const FPal& P)
	{
		// Grass-lined scrapes inside a ring of stones; feathers in the snow.
		const FVector G = S.G(0, 0);
		for (int32 k = 0; k < 12; ++k) S.K.Add(P.GraniteDark, FVector(40.f, 30.f, 24.f), G + FRotator(0.f, 30.f * k, 0.f).Vector() * 260.f + FVector(0, 0, 10.f), FRotator(0.f, 30.f * k, 0.f));
		for (int32 n = 0; n < 4; ++n)
		{
			const FVector N = G + FRotator(0.f, 90.f * n + 20.f * S.Variant, 0.f).Vector() * 120.f;
			S.K.Ring(P.Grass, N + FVector(0, 0, 6.f), 22.f, 14.f, 12.f, 8, 0.f, 360.f, 2.f);
			S.K.Add(P.SnowShade.Dim(0.85f), FVector(44.f, 44.f, 4.f), N + FVector(0, 0, 3.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			if (n % 2 == 0) S.K.Add(P.Feather, FVector(10.f, 8.f, 9.f), N + FVector(4.f, 3.f, 8.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		}
		for (int32 f = 0; f < 8; ++f) S.K.Add(P.Feather, FVector(14.f, 3.f, 1.f), S.G(-200.f + 50.f * f, 60.f * FMath::Sin(f * 1.3f), 1.f), FRotator(0.f, 40.f * f, 0.f));
	}

	void CachePole(FWRIsleSite& S, const FPal& P)
	{
		// A Rime-folk meat cache: a stone platform, a tall pole with a bundle hung out of the foxes' reach, a lamp at the top
		// for finding it in the dark.
		const FVector G = S.G(0, 0);
		for (int32 k = 0; k < 6; ++k) S.K.Add(P.Granite.Dim(0.9f + 0.2f * S.R(FVector(k), 1)), FVector(90.f, 70.f, 40.f), G + FRotator(0.f, 60.f * k, 0.f).Vector() * 70.f + FVector(0, 0, 18.f), FRotator(0.f, 60.f * k, 0.f));
		S.K.Add(P.Granite, FVector(160.f, 150.f, 30.f), G + FVector(0, 0, 50.f));
		S.K.Rod(P.Willow, G + FVector(0, 0, 30.f), G + FVector(10.f, 0, 520.f), 14.f);
		S.K.Rod(P.Willow, G + FVector(10.f, 0, 500.f), G + FVector(150.f, 0, 520.f), 8.f);
		S.K.Rod(P.HideDark, G + FVector(140.f, 0, 515.f), G + FVector(140.f, 0, 430.f), 2.f);
		S.K.Add(P.Hide, FVector(70.f, 60.f, 90.f), G + FVector(140.f, 0, 390.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		S.K.Add(P.Red, FVector(4.f, 20.f, 40.f), G + FVector(12.f, 8.f, 460.f));
		const FVector Lamp = G + FVector(-20.f, 0, 510.f);
		S.K.Add(P.Iron, FVector(22.f, 22.f, 26.f), Lamp, FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		S.Lamp(Lamp, Lin(255, 196, 120), 1600.f, 1000.f);
		S.Solid(G + FVector(0, 0, 40.f), FVector(110.f, 110.f, 40.f));
	}

	void FrozenCarcass(FWRIsleSite& S, const FPal& P)
	{
		// What the cold kept: a beast's ribs arching out of the snow, a stiff hide, its skull a little way off.
		const FVector G = S.G(0, 0);
		const float Size = 1.f + 0.4f * (S.Variant % 3);
		for (int32 r = 0; r < 8; ++r)
		{
			const float X = (-120.f + 34.f * r) * Size;
			for (const float Sd : { -1.f, 1.f })
			{
				FVector Prev = G + FVector(X, Sd * 10.f, 30.f * Size);
				for (int32 k = 1; k <= 4; ++k)
				{
					const float T = k / 4.f;
					const FVector Pt = G + FVector(X, Sd * (10.f + 70.f * Size * FMath::Sin(T * PI * 0.6f)), (30.f + 90.f * FMath::Sin(T * PI) - 60.f * T) * Size);
					S.K.Rod(P.Bone.Dim(0.85f), Prev, Pt, 7.f * Size);
					Prev = Pt;
				}
			}
		}
		S.K.Rod(P.Bone, G + FVector(-150.f * Size, 0, 30.f), G + FVector(150.f * Size, 0, 30.f), 14.f * Size);
		S.K.Add(P.Fur, FVector(180.f, 110.f, 12.f) * Size, G + FVector(40.f, 120.f * Size, 8.f), FRotator(0.f, 20.f, 8.f));
		S.K.Add(P.Bone, FVector(60.f, 40.f, 40.f) * Size, G + FVector(260.f * Size, 60.f, 20.f), FRotator(10.f, 30.f, 0.f));
		S.K.Add(P.Snow, FVector(380.f, 200.f, 30.f) * Size, G + FVector(-20.f, 0, 8.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
	}

	void SmallPingo(FWRIsleSite& S, const FPal& P)
	{
		// A small ice-cored mound, its top cracked open to blue. On a map with lands of its own the mound is the ground's
		// (round_ground.mjs raises it at the kit's spot): only the cracks.
		const float G0 = S.Gz(0, 0) - 30.f;
		const float Sc = 1.f + 0.25f * (S.Variant % 3);
		if (WRLand::OwnLandMap())
		{
			for (int32 k = 0; k < 4; ++k)
			{
				const FRotator R(0.f, 90.f * k + 20.f, 0.f);
				const FVector Mid = R.Vector() * 80.f * Sc;
				S.K.Add(P.DeepIce, FVector(170.f, 18.f, 30.f) * Sc, FVector(Mid.X, Mid.Y, S.Gz(Mid.X, Mid.Y) + 4.f), R);
			}
			return;
		}
		for (int32 t = 0; t < 4; ++t)
		{
			const float D = FMath::Lerp(1300.f, 380.f, t / 3.f) * Sc;
			S.K.Add(t % 2 ? P.SnowShade : P.Snow, FVector(D, D * 0.9f, 140.f), FVector(0, 0, G0 + 70.f + 120.f * t), FRotator(0.f, 21.f * t, 0.f), EWRLookMesh::Cylinder);
		}
		S.Solid(FVector(0, 0, G0 + 160.f), FVector(460.f, 420.f, 160.f) * FVector(Sc, Sc, 1.f));
		const FVector Top(0, 0, G0 + 450.f);
		for (int32 k = 0; k < 4; ++k) S.K.Add(P.DeepIce, FVector(160.f, 16.f, 30.f) * Sc, Top + FRotator(0.f, 90.f * k + 20.f, 0.f).Vector() * 70.f * Sc, FRotator(0.f, 90.f * k + 20.f, 0.f));
	}

	// --------------------------------------------------------------------------------------------------- hazard spots

	void FumaroleSpot(FWRIsleSite& S, const FPal& P)
	{
		// A fumarole: a crusted cone round a dark throat that wisps all day and bursts on its own clock.
		const FVector G = S.G(0, 0);
		for (int32 k = 0; k < 8; ++k)
		{
			const FVector At = G + FRotator(0.f, 45.f * k, 0.f).Vector() * 70.f + FVector(0, 0, 16.f);
			S.K.Add((k % 3 ? P.Sulfur : P.Slag).Dim(0.8f + 0.3f * S.R(At, 1)), FVector(60.f, 44.f, 36.f), At, FRotator(-20.f, 45.f * k, 0.f));
		}
		for (int32 k = 0; k < 6; ++k) S.K.Add(P.Sulfur.Dim(0.6f), FVector(260.f, 160.f, 8.f), S.G(FMath::Cos(k * 1.05f) * 200.f, FMath::Sin(k * 1.05f) * 200.f, 2.f), FRotator(0.f, 60.f * k, 0.f), EWRLookMesh::Cylinder);
		S.K.Add(P.Dark, FVector(60.f, 60.f, 8.f), G + FVector(0, 0, 8.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		S.Fx(G + FVector(0, 0, 30.f), TEXT("ff.vent.wisp"), 0.8f);
		S.Hazard(FWRIsleHazard::EKind::Fumarole, G + FVector(0, 0, 4.f), 200.f);
	}

	void GeyserSpot(FWRIsleSite& S, const FPal& P)
	{
		// A geyser: a terraced sinter mound round a teal pool that bulges and bursts on its clock.
		const FVector G = S.G(0, 0);
		for (int32 t = 0; t < 3; ++t) S.K.Add(FSurf::Stone(Lin(210, 200, 176), 0.6f).Dim(1.f - 0.1f * t), FVector(700.f - 180.f * t, 640.f - 170.f * t, 30.f), G + FVector(0, 0, 10.f + 22.f * t), FRotator(0.f, 25.f * t, 0.f), EWRLookMesh::Cylinder);
		S.K.Add(FSurf::Glaze(Lin(79, 184, 168), 0.04f), FVector(220.f, 220.f, 6.f), G + FVector(0, 0, 76.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		for (int32 k = 0; k < 10; ++k) S.K.Add(P.Sulfur.Dim(0.6f), FVector(70.f, 40.f, 10.f), G + FRotator(0.f, 36.f * k, 0.f).Vector() * 130.f + FVector(0, 0, 76.f), FRotator(0.f, 36.f * k + 90.f, 0.f));
		S.Fx(G + FVector(0, 0, 90.f), TEXT("ff.vent.wisp"), 0.6f);
		FWRIsleHazard& H = S.Hazard(FWRIsleHazard::EKind::Geyser, G + FVector(0, 0, 70.f), 150.f);
		H.Half = FVector::ZeroVector;
	}
}

namespace WRIsleK
{
	/** The dispatch, inside this file's namespace so its names win over any other file's in a unity build. */
	bool BuildImpl(FWRIsleSite& S, const FString& Kind)
	{
		const FPal P;
		const float Cull = WRTune(TEXT("places.kitCull"), 40000.f);
		if (Kind == TEXT("sulfur_vent")) SulfurVent(S, P);
		else if (Kind == TEXT("hornito"))
		{
			// A spatter cone that spits embers in its turn (each on its own phase of the clock).
			const float H = 300.f + 160.f * (S.Variant % 3);
			WRIsleLook::VolcanicHornito(S, S.G(0, 0, -20.f), H, 160.f);
			S.Breath(S.G(0, 0, H + 20.f), TEXT("ff.hornito.puff"), 7.f + 2.f * (S.Variant % 3), 9.f * S.R(FVector(S.Variant), 7), 1.2f, 1800.f, 700.f, Lin(255, 120, 40));
		}
		else if (Kind == TEXT("smith_grave")) SmithGrave(S, P);
		else if (Kind == TEXT("giant_tool")) GiantTool(S, P);
		else if (Kind == TEXT("glowmoth_roost")) GlowmothRoost(S, P);
		else if (Kind == TEXT("basalt_ring")) BasaltRing(S, P);
		else if (Kind == TEXT("miner_leanto")) MinerLeanto(S, P);
		else if (Kind == TEXT("tent_ring")) TentRing(S, P);
		else if (Kind == TEXT("erratic")) Erratic(S, P);
		else if (Kind == TEXT("snowpuff_nest")) SnowpuffNest(S, P);
		else if (Kind == TEXT("cache_pole")) CachePole(S, P);
		else if (Kind == TEXT("frozen_carcass")) FrozenCarcass(S, P);
		else if (Kind == TEXT("small_pingo")) SmallPingo(S, P);
		else if (Kind == TEXT("fumarole")) FumaroleSpot(S, P);
		else if (Kind == TEXT("geyser")) GeyserSpot(S, P);
		else return false;
		S.Done(Cull);
		return true;
	}
}

bool WRIsleLook::BuildKit(FWRIsleSite& S, const FString& Kind)
{
	return WRIsleK::BuildImpl(S, Kind);
}
