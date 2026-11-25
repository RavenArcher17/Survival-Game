// The Tundra places (cooks/cookpass-world.md 3.4): the Rime-folk's landmarks and the story sites of the frozen east,
// from the look kit at full scale. Palette (fire-frost 3.4.1): snow #EEF2F5, shadow blue #9DB2C8, ice teal #9FD4E0,
// glacier blue #5FA8D3, black thin ice #1B2A33, granite #B9C4CE, ivory #D8C9A3, the Rime-folk's band red #C62828,
// blue #2E5AA8, yellow #E2B13C, hide #7A6048, pale pine bone-white #E6E2D8.
#include "WRIslandPlacesLook.h"
#include "WRIslandPlaces.h"
#include "WRTuning.h"
#include "WRWorldDressing.h"
#include "WRGroundTrail.h"
#include "WRLand.h"
#include "Wildreach.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"

using WRLookKit::FSurf;

namespace WRIsleT
{
	using WRIsleLook::Lin;

	struct FPal
	{
		FSurf Snow = FSurf::Stone(Lin(226, 232, 238), 0.9f).WithPattern(WRLookPattern::Plaster);
		FSurf SnowShade = FSurf::Stone(Lin(200, 214, 228), 0.9f);
		FSurf Ice = FSurf::Ice(Lin(196, 222, 236));
		FSurf DeepIce = FSurf::Ice(Lin(150, 194, 222));
		FSurf IceGlow = FSurf::Lit(Lin(120, 190, 235), 0.18f);
		FSurf ThinIce = FSurf::Glass(Lin(27, 42, 51), 0.f);
		FSurf Water = FSurf::Glaze(Lin(10, 22, 30), 0.04f);
		FSurf Granite = FSurf::Stone(Lin(185, 196, 206), 0.85f);
		FSurf GraniteDark = FSurf::Stone(Lin(120, 132, 146), 0.88f);
		FSurf Lichen = FSurf::Cloth(Lin(184, 145, 59));
		FSurf Ivory = FSurf::Stone(Lin(216, 201, 163), 0.55f);
		FSurf IvoryDark = FSurf::Stone(Lin(168, 150, 112), 0.6f);
		FSurf Hide = FSurf::Cloth(Lin(122, 96, 72));
		FSurf HideDark = FSurf::Cloth(Lin(84, 64, 48));
		FSurf Willow = FSurf::Wood(Lin(138, 122, 98));
		FSurf WoodDark = FSurf::Wood(Lin(70, 56, 44));
		FSurf PaleWood = FSurf::Wood(Lin(230, 226, 216));
		FSurf Red = FSurf::Cloth(Lin(198, 40, 40));
		FSurf Blue = FSurf::Cloth(Lin(46, 90, 168));
		FSurf Yellow = FSurf::Cloth(Lin(226, 177, 60));
		FSurf Iron = FSurf::Steel(Lin(44, 42, 42), 0.6f);
		FSurf Bone = FSurf::Stone(Lin(214, 208, 190), 0.6f);
		FSurf Dark = FSurf::Stone(Lin(8, 9, 11), 0.98f);
		FSurf Gouge = FSurf::Stone(Lin(60, 66, 76), 0.9f);
		FSurf Fur = FSurf::Cloth(Lin(150, 132, 108));
	};

	/** A stacked stone in the Rime-folk's way: flat-bedded, a little proud of the one under it. */
	FVector Stack(FWRIsleSite& S, const FSurf& Surf, const FVector& Base, const FVector& Size, int32 Salt, float Turn = 0.f)
	{
		S.K.Add(Surf.Dim(0.85f + 0.3f * S.R(Base, Salt)), Size, Base + FVector(6.f * S.J(Base, Salt + 1), 6.f * S.J(Base, Salt + 2), Size.Z * 0.5f),
			FRotator(2.f * S.J(Base, Salt + 3), Turn + 10.f * S.J(Base, Salt + 4), 2.f * S.J(Base, Salt + 5)));
		return Base + FVector(0, 0, Size.Z);
	}

	/** An inuksuk H cm tall, its arm pointing along local +X; lichen on its weather side. */
	void Inuksuk(FWRIsleSite& S, const FPal& P, float H, bool bLamp)
	{
		const float U = H / 200.f;
		const float G0 = S.Gz(0, 0) - 8.f;
		FVector L = FVector(0, -28.f * U, G0), R = FVector(0, 28.f * U, G0);
		for (int32 k = 0; k < 2; ++k)
		{
			L = Stack(S, P.Granite, L, FVector(36.f, 30.f, 26.f) * U, 10 + k);
			R = Stack(S, P.Granite, R, FVector(36.f, 30.f, 26.f) * U, 20 + k);
		}
		FVector C(0, 0, FMath::Max(L.Z, R.Z));
		C = Stack(S, P.GraniteDark, C, FVector(46.f, 96.f, 20.f) * U, 30);           // the hips
		C = Stack(S, P.Granite, C, FVector(40.f, 54.f, 40.f) * U, 31);               // the body
		C = Stack(S, P.GraniteDark, C, FVector(150.f, 30.f, 18.f) * U, 32);           // the arm, its long side along +X
		S.K.Out.Last().Centre += S.K.Frame.GetRotation().RotateVector(FVector(36.f * U, 0, 0));
		C = Stack(S, P.Granite, C, FVector(30.f, 34.f, 34.f) * U, 33);               // the head
		S.K.Add(P.Lichen.Dim(0.8f), FVector(30.f, 4.f, 26.f) * U, FVector(-10.f * U, -50.f * U, G0 + 70.f * U));
		S.Solid(FVector(0, 0, G0 + H * 0.4f), FVector(26.f, 46.f, H * 0.4f) * FVector(U, U, 1.f));
		if (bLamp)
		{
			// The lamp bowl at its foot: soapstone, a moss wick (lit as you pass at night).
			const FVector Bw(30.f * U, 0, S.Gz(30.f * U, 0) + 10.f);
			S.K.Add(P.GraniteDark, FVector(40.f, 40.f, 14.f), Bw, FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			S.PassCup(Bw + FVector(0, 0, 25.f), Lin(255, 196, 120), WRTune(TEXT("places.cupLux"), 2400.f), 1200.f);
		}
	}

	/** A sledge: two upturned runners, cross slats lashed on, a load or none; local At, facing Yaw. */
	void Sledge(FWRIsleSite& S, const FPal& P, const FVector& At, float Yaw, int32 Salt, bool bLoad, float Tilt = 0.f)
	{
		const FTransform Was = S.K.Frame;
		S.K.Frame = FTransform(FRotator(0.f, Yaw, Tilt), At);
		for (const float Sd : { -1.f, 1.f })
		{
			S.K.Add(P.Willow, FVector(320.f, 8.f, 14.f), FVector(0, Sd * 55.f, 7.f));
			S.K.Rod(P.Willow, FVector(160.f, Sd * 55.f, 7.f), FVector(205.f, Sd * 55.f, 60.f), 8.f);      // the runner's upturned toe
			for (int32 u = 0; u < 4; ++u) S.K.Rod(P.Willow, FVector(-120.f + 80.f * u, Sd * 55.f, 10.f), FVector(-120.f + 80.f * u, Sd * 55.f, 34.f), 5.f);
		}
		for (int32 s = 0; s < 9; ++s) S.K.Add(P.Willow.Dim(0.9f), FVector(14.f, 130.f, 4.f), FVector(-140.f + 33.f * s, 0, 36.f));
		if (bLoad)
		{
			S.K.Add(P.Fur, FVector(130.f, 90.f, 50.f), FVector(-30.f, 0, 62.f), FRotator(0.f, 6.f, 0.f), EWRLookMesh::Cylinder);
			S.K.Add(P.HideDark, FVector(80.f, 70.f, 40.f), FVector(60.f, 10.f, 58.f), FRotator(0.f, -10.f, 4.f));
		}
		S.K.Frame = Was;
	}

	/** A tusk from Base sweeping up along a curve to Tip (ringed ivory, thick to thin). */
	void Tusk(FWRIsleSite& S, const FPal& P, const FVector& Base, const FVector& Ctrl, const FVector& Tip, float Thick)
	{
		FVector Prev = Base;
		const int32 N = 14;
		for (int32 k = 1; k <= N; ++k)
		{
			const float T = float(k) / N;
			const FVector Pt = (1 - T) * (1 - T) * Base + 2 * (1 - T) * T * Ctrl + T * T * Tip;
			const float D = FMath::Lerp(Thick, Thick * 0.38f, T);
			S.K.Rod(k % 3 == 0 ? P.IvoryDark : P.Ivory, Prev, Pt, D);
			if (k % 2 == 0) S.K.Disc(P.IvoryDark.Dim(0.9f), Pt, (Pt - Prev).GetSafeNormal(), D * 1.06f, 4.f);
			Prev = Pt;
		}
	}

	/** A dead pine: a tapered pale trunk, branch stubs angled down, the top snapped on some (lying at its foot). */
	void DeadPine(FWRIsleSite& S, const FPal& P, const FVector& G, float H, int32 Salt, bool bGnawed)
	{
		// (round 14: five straight rods with pencil-thin stubs read as a ring of telephone poles) A pine snag: a trunk that
		// bends a little as it climbs and tapers hard, its roots flared into the snow, thick broken-off branch stubs up it,
		// a few long dead limbs still hanging on, the top snapped into a splintered spike; leaning, a few of them hard.
		const bool bSnapped = S.R(G, Salt) < 0.5f;
		const float Top = bSnapped ? H * (0.55f + 0.2f * S.R(G, Salt + 9)) : H;
		const float LeanAmt = (S.R(G, Salt + 5) < 0.3f ? 0.14f : 0.045f) * (0.5f + S.R(G, Salt + 6));
		const FVector LeanDir = FRotator(0.f, 360.f * S.R(G, Salt + 7), 0.f).Vector();
		const FVector Bow(S.J(G, Salt + 11), S.J(G, Salt + 12), 0.f);
		const float Girth = 0.8f + 0.45f * S.R(G, Salt + 8);
		const float R0 = 34.f * Girth * (H / 1200.f + 0.45f);
		const auto TrunkAt = [&](float T) { return G + FVector(0.f, 0.f, Top * T) + LeanDir * (Top * T * LeanAmt) + Bow * (70.f * FMath::Sin(PI * T * 1.3f)); };
		const auto RadAt = [&](float T) { return R0 * FMath::Lerp(1.f, 0.2f, FMath::Pow(T, 0.75f)); };
		FVector Prev = G - FVector(0, 0, 40.f);
		for (int32 k = 1; k <= 8; ++k)
		{
			const float T = k / 8.f;
			const FVector Pt = TrunkAt(T);
			S.K.Rod(P.PaleWood.Dim(0.72f + 0.035f * k), Prev, Pt, RadAt(T - 0.0625f) * (k == 1 ? 1.15f : 1.f));
			Prev = Pt;
		}
		{
			// the top: a splintered spike where it snapped, or the leader's last thin stick
			const FVector T0 = TrunkAt(1.f);
			const FVector Tip = T0 + FVector(30.f * S.J(G, Salt + 13), 30.f * S.J(G, Salt + 14), bSnapped ? 70.f : 160.f);
			S.K.Rod(P.PaleWood.Dim(0.68f), T0, Tip, RadAt(1.f) * (bSnapped ? 0.6f : 0.4f));
			if (bSnapped) S.K.Rod(P.PaleWood.Dim(0.6f), T0, T0 + FVector(-20.f * S.J(G, Salt + 14), 25.f * S.J(G, Salt + 13), 110.f), RadAt(1.f) * 0.35f);
		}
		// the root flare
		for (int32 r = 0; r < 4; ++r)
		{
			const FVector D = FRotator(0.f, 90.f * r + 30.f * S.J(G, Salt + 20 + r), 0.f).Vector();
			S.K.Rod(P.PaleWood.Dim(0.66f), G + FVector(0, 0, 70.f) + D * (R0 * 0.3f), G + D * (R0 * 2.7f) - FVector(0, 0, 35.f), R0 * 0.4f);
		}
		// branch stubs in loose whorls, a few long limbs still on below the middle, drooping
		for (int32 b = 0; b < 16; ++b)
		{
			const float T = 0.16f + 0.052f * b + 0.02f * S.J(G, Salt + 30 + b);
			if (T * Top > Top - 70.f) break;
			const FVector D = FRotator(0.f, 137.5f * b + 360.f * S.R(G, Salt + 3), 0.f).Vector();
			const FVector At = TrunkAt(T);
			const bool bLimb = (b % 5 == 2) && T < 0.65f;
			const float Len = bLimb ? 240.f + 120.f * S.R(G, Salt + 50 + b) : 45.f + 70.f * S.R(G, Salt + 50 + b);
			const FVector End = At + D * Len + FVector(0, 0, bLimb ? -70.f : 20.f + 25.f * S.R(G, Salt + 70 + b));
			S.K.Rod(P.PaleWood.Dim(0.64f), At, End, bLimb ? 9.f * Girth : (8.f + 5.f * S.R(G, Salt + 90 + b)) * Girth);
			if (bLimb) S.K.Rod(P.PaleWood.Dim(0.6f), End, End + D * 90.f + FVector(40.f * S.J(G, Salt + 110 + b), 0.f, -50.f), 4.5f * Girth);
		}
		if (bGnawed) for (int32 g = 0; g < 3; ++g) S.K.Add(P.WoodDark, FVector(10.f, 30.f, 50.f), G + FVector(R0 * 0.85f, 10.f * g, 80.f + 30.f * g), FRotator(0.f, 30.f * g, 0.f));
		if (bSnapped)
		{
			// the fallen top lying in the snow beside it
			const FVector D = FRotator(0.f, 360.f * S.R(G, Salt + 4), 0.f).Vector();
			const FVector A = G + D * 220.f, B = G + D * (220.f + H * 0.35f);
			S.K.Rod(P.PaleWood.Dim(0.68f), FVector(A.X, A.Y, S.Gz(A.X, A.Y) + 14.f), FVector(B.X, B.Y, S.Gz(B.X, B.Y) + 8.f), RadAt(0.6f));
		}
		S.Solid(G + FVector(0, 0, Top * 0.5f), FVector(R0 * 0.6f, R0 * 0.6f, Top * 0.5f));
	}

	// ======================================================================================================= landmarks

	void TuskGate(FWRIsleSite& S, const FPal& P)
	{
		// The Rime-folk's front door on Rimehold's road toward the glacier (local +X): two pairs of mammoth tusks in stone
		// sockets, crossing 9 m up and lashed with hide; four lamps hang from the crossing.
		for (const float Xo : { -160.f, 160.f })
		{
			for (const float Sd : { -1.f, 1.f })
			{
				const FVector Base(Xo, Sd * 460.f, S.Gz(Xo, Sd * 460.f) - 20.f);
				S.K.Add(P.GraniteDark, FVector(150.f, 150.f, 90.f), Base + FVector(0, 0, 25.f));
				S.K.Add(P.Granite, FVector(120.f, 120.f, 30.f), Base + FVector(0, 0, 80.f));
				S.Solid(Base + FVector(0, 0, 45.f), FVector(75.f, 75.f, 50.f));
				const float Top = S.Gz(0, 0) + 920.f;
				Tusk(S, P, Base + FVector(0, 0, 90.f), FVector(Xo - 40.f, Sd * 560.f, Base.Z + 700.f), FVector(Xo + 30.f, -Sd * 110.f, Top), 74.f);
			}
			// The lashing where each pair crosses.
			const FVector X(Xo + 30.f, 0.f, S.Gz(0, 0) + 900.f);
			for (int32 w = 0; w < 6; ++w) S.K.Disc(w % 2 ? P.HideDark : P.Hide, X + FVector(0, 0, -30.f + 12.f * w), FVector(0.2f * (w % 2 ? 1 : -1), 0, 1), 90.f, 10.f);
		}
		// A crossbar of driftwood joins the pairs, and the lamps hang from it on thongs.
		const float BarZ = S.Gz(0, 0) + 880.f;
		S.K.Rod(P.WoodDark, FVector(-260.f, 0, BarZ), FVector(260.f, 0, BarZ), 18.f);
		for (int32 l = 0; l < 4; ++l)
		{
			const FVector Hang(-200.f + 133.f * l, 20.f * ((l % 2) ? 1 : -1), BarZ);
			const FVector Lamp = Hang - FVector(0, 0, 150.f + 30.f * (l % 2));
			S.K.Rod(P.HideDark, Hang, Lamp + FVector(0, 0, 14.f), 2.f);
			S.K.Add(P.Iron, FVector(26.f, 26.f, 30.f), Lamp, FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			S.K.Add(FSurf::Lit(Lin(255, 200, 130), 0.8f), FVector(18.f, 18.f, 18.f), Lamp + FVector(0, 0, 2.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			S.Lamp(Lamp, Lin(255, 196, 120), WRTune(TEXT("places.gateLampLux"), 2500.f), 1200.f);
		}
		// Prayer strips in the band colours tied to the tusks.
		const FSurf Bands[3] = { P.Red, P.Blue, P.Yellow };
		for (int32 k = 0; k < 6; ++k)
		{
			const float Sd = k % 2 ? 1.f : -1.f;
			const FVector At(-160.f + 320.f * (k / 3), Sd * (380.f - 30.f * (k % 3)), S.Gz(0, 0) + 260.f + 120.f * (k % 3));
			S.K.Add(Bands[k % 3], FVector(4.f, 10.f, 70.f), At - FVector(0, 0, 35.f), FRotator(0.f, 0.f, 8.f * Sd));
		}
	}

	void GreatInuksuk(FWRIsleSite& S, const FPal& P)
	{
		// Nine metres of stacked granite at the Rime-folk road's heart, its arm pointing at the glacier bowl (local +X).
		const float U = 4.4f;
		const float G0 = S.Gz(0, 0) - 30.f;
		FVector L(0, -60.f * U, G0), R(0, 60.f * U, G0);
		for (int32 k = 0; k < 3; ++k)
		{
			L = Stack(S, P.Granite, L, FVector(34.f, 30.f, 22.f) * U, 10 + k);
			R = Stack(S, P.Granite, R, FVector(34.f, 30.f, 22.f) * U, 20 + k);
		}
		S.Solid(FVector(0, -60.f * U, (G0 + L.Z) * 0.5f), FVector(17.f * U, 15.f * U, (L.Z - G0) * 0.5f));
		S.Solid(FVector(0, 60.f * U, (G0 + R.Z) * 0.5f), FVector(17.f * U, 15.f * U, (R.Z - G0) * 0.5f));
		FVector C(0, 0, FMath::Max(L.Z, R.Z));
		const float BodyZ0 = C.Z;
		C = Stack(S, P.GraniteDark, C, FVector(40.f, 170.f, 20.f) * U, 30);
		C = Stack(S, P.Granite, C, FVector(36.f, 60.f, 40.f) * U, 31);
		C = Stack(S, P.GraniteDark, C, FVector(170.f, 28.f, 18.f) * U, 32);
		S.K.Out.Last().Centre += S.K.Frame.GetRotation().RotateVector(FVector(50.f * U, 0, 0));
		C = Stack(S, P.Granite, C, FVector(28.f, 32.f, 34.f) * U, 33);
		S.Solid(FVector(0, 0, (BodyZ0 + C.Z) * 0.5f), FVector(18.f * U, 30.f * U, (C.Z - BodyZ0) * 0.5f));
		for (int32 k = 0; k < 6; ++k) S.K.Add(P.Lichen.Dim(0.7f + 0.2f * (k % 2)), FVector(4.f, 40.f, 30.f) * U * 0.6f, FVector(-17.f * U, -60.f * U + 24.f * U * k, G0 + 30.f * U + 9.f * U * k));
		// The fire bowl at its feet on the road side: a ring of stones round a soapstone bowl.
		const FVector Bowl(130.f * U * 0.5f + 160.f, 0.f, 0.f);
		const FVector BG(Bowl.X, Bowl.Y, S.Gz(Bowl.X, Bowl.Y));
		for (int32 k = 0; k < 10; ++k) S.K.Add(P.GraniteDark, FVector(40.f, 30.f, 26.f), BG + FRotator(0.f, 36.f * k, 0.f).Vector() * 95.f + FVector(0, 0, 10.f), FRotator(0.f, 36.f * k, 0.f));
		S.K.Add(P.GraniteDark.Dim(0.7f), FVector(110.f, 110.f, 26.f), BG + FVector(0, 0, 14.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		S.NightFx(BG + FVector(0, 0, 30.f), TEXT("brazier.fire"), 1.f);
		S.Lamp(BG + FVector(0, 0, 90.f), Lin(255, 170, 90), WRTune(TEXT("places.inuksukFireLux"), 6000.f), 2200.f);
	}

	void FrozenWhaler(FWRIsleSite& S, const FPal& P)
	{
		// A Rime-folk longboat from when the shelf was sea, locked in the ice: clinker strakes, the stern sunk in the shelf, the
		// bow heaved up on a pressure ridge, slabs of ice shoved up against its sides; the serac behind it; a lantern left in
		// the crow's nest. (11-22 round 5: pitched 34 degrees with its middle 4 m up, it hung in the air over two seracs.)
		const float G0 = S.Gz(0, 0);
		// The serac behind its stern (the stern runs into its foot), and a second leaning on it.
		S.Ice(0, FVector(1250.f, 1000.f, 1340.f), FVector(-1250.f, 0.f, S.Gz(-1250.f, 0.f) - 90.f), FRotator(-6.f, 8.f, 4.f), false, 0.f);
		S.Ice(1, FVector(800.f, 680.f, 820.f), FVector(-1000.f, 900.f, S.Gz(-1000.f, 900.f) - 60.f), FRotator(8.f, 30.f, -6.f), false, 0.f);
		S.Solid(FVector(-1250.f, 0.f, S.Gz(-1250.f, 0.f) + 560.f), FVector(550.f, 450.f, 650.f), FRotator(-6.f, 8.f, 4.f));
		S.Solid(FVector(-1000.f, 900.f, S.Gz(-1000.f, 900.f) + 340.f), FVector(350.f, 300.f, 400.f), FRotator(8.f, 30.f, -6.f));
		// The hull (SM_Longboat_A, 11-22 round 4: a clinker hull with its strakes lapped, ribs and thwarts inside, the stems
		// curling up; it was boards stuck out like fins) in weathered wood, its grain along the keel; pitched up 12 degrees and
		// listing, its keel half a metre down amidships: the stern sunk to the gunwale, the bow's forefoot clear on the ridge.
		// The carving, the mast and the crow's nest go on in its frame.
		const FRotator HullRot(12.f, 0.f, 7.f);
		const FVector HullAt(150.f, -40.f, G0 - 50.f);
		// Weathered: centuries in the ice have greyed the tarred planks.
		const FSurf Weathered = FSurf::Wood(Lin(118, 104, 90));
		S.ShapeKit(TEXT("/Game/Art/Gen/SM_Longboat_A.SM_Longboat_A"), HullAt, HullRot, FVector(1.f), true, Weathered, 0.f);
		const FTransform Was = S.K.Frame;
		S.K.Frame = FTransform(HullRot, HullAt);
		// A carved head on the prow's scroll.
		S.K.Add(Weathered.Dim(0.9f), FVector(70.f, 26.f, 50.f), FVector(985.f, 0.f, 600.f), FRotator(-20.f, 0.f, 0.f));
		// The mast stub and the crow's nest with its lantern.
		const FVector Mast0(120.f, 0, 40.f), Mast1(120.f, 0, 720.f);
		S.K.Rod(Weathered.Dim(0.85f), Mast0, Mast1, 30.f);
		S.K.Disc(Weathered.Dim(0.85f), Mast1 - FVector(0, 0, 60.f), FVector::UpVector, 150.f, 10.f);
		for (int32 k = 0; k < 8; ++k)
		{
			const FVector D = FRotator(0.f, 45.f * k, 0.f).Vector();
			S.K.Rod(Weathered.Dim(0.85f), Mast1 - FVector(0, 0, 60.f) + D * 70.f, Mast1 + D * 70.f + FVector(0, 0, 20.f), 5.f);
		}
		const FVector Lantern = S.K.Frame.TransformPosition(Mast1 - FVector(0, 0, 20.f));
		S.K.Frame = Was;
		S.K.Add(P.Iron, FVector(24.f, 24.f, 34.f), Lantern, FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		S.K.Add(FSurf::Lit(Lin(255, 200, 130), 0.6f), FVector(16.f, 16.f, 20.f), Lantern, FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		S.Lamp(Lantern + FVector(0, 0, 10.f), Lin(255, 196, 120), WRTune(TEXT("places.whalerLux"), 2600.f), 1600.f);
		// The pressure ridge the bow rides on, and slabs of the shelf shoved up against both sides of the hull, tipped toward it
		// (glacier ice blocks, snow on their tops; serac towers stood round it like posts).
		static const TCHAR* Blocks[3] = { TEXT("/Game/Art/Gen/SM_IceBlock_A.SM_IceBlock_A"), TEXT("/Game/Art/Gen/SM_IceBlock_B.SM_IceBlock_B"), TEXT("/Game/Art/Gen/SM_IceBlock_C.SM_IceBlock_C") };
		const float Tan = FMath::Tan(FMath::DegreesToRadians(HullRot.Pitch));
		const auto KeelZ = [&](float X) { return G0 - 50.f + (X - 150.f) * Tan; };
		for (int32 k = 0; k < 2; ++k)
		{
			// Under the bow: its top just under the keel there.
			const float X = 650.f + 260.f * k;
			const float Top = KeelZ(X) - 10.f - S.Gz(X, -40.f);
			S.Shape(Blocks[k], FVector(X, -40.f, S.Gz(X, -40.f) - 60.f), FRotator(0.f, 20.f + 35.f * k, 0.f), FVector(360.f / 400.f, 330.f / 400.f, (Top + 60.f) / 400.f), false, false, FLinearColor::White, 0.55f);
		}
		for (int32 k = 0; k < 8; ++k)
		{
			const float Sd = k % 2 ? 1.f : -1.f;
			const float X = -700.f + 230.f * (k / 2) + 60.f * S.J(FVector(k, 1, 0), 1);
			const FVector Seed(k, 2, 0);
			const float Y = -40.f + Sd * (300.f + 50.f * S.R(Seed, 2));
			const float H = 140.f + 120.f * S.R(Seed, 3);
			// Tipped in toward the hull, each its own size and turn.
			S.Shape(Blocks[k % 3], FVector(X, Y, S.Gz(X, Y) - 70.f), FRotator(0.f, 15.f * S.J(Seed, 4), -Sd * (14.f + 10.f * S.R(Seed, 5))),
				FVector((260.f + 120.f * S.R(Seed, 6)) / 400.f, (110.f + 50.f * S.R(Seed, 7)) / 400.f, (H + 70.f) / 400.f), false, false, FLinearColor::White, 0.6f);
		}
	}

	void WeepingSeracs(FWRIsleSite& S, const FPal& P)
	{
		// A 50 m wall of blue ice towers leaning toward the bowl (local +X), cracked; the blocks that fell before lie at the
		// foot. Six drop points in front are the serac-fall hazard.
		// A ragged, fused wall (11-22: eleven towers of one size in an even row read as totems): seventeen towers set close
		// and out of line, of every height, leaning out over the bowl, a lower mass of ice fused behind them.
		// (round 14: still a palisade of slim prisms; now broad chunks shouldering into each other, lower, the tallest
		// toward the middle; seeded a metre apart: whole indices all fell in one 5 cm cell of the kit's hash)
		for (int32 i = 0; i < 17; ++i)
		{
			const FVector Seed(i * 100.f, 300.f, 0.f);
			const float Y = -2600.f + 325.f * i + 110.f * S.J(Seed, 1);
			const float X = -300.f + 260.f * S.J(Seed, 8) - (i % 3 == 1 ? 220.f : 0.f);
			const float W = 440.f + 260.f * S.R(Seed, 2), D = 400.f + 220.f * S.R(Seed, 3);
			const float H = 420.f + 820.f * FMath::Pow(S.R(Seed, 4), 0.8f) * (0.5f + 0.5f * FMath::Sin(PI * (i + 0.5f) / 17.f));
			const float Lean = 4.f + 8.f * S.R(Seed, 5);
			const FVector C(X, Y, S.Gz(X, Y) + H * 0.5f - 60.f);
			const FRotator Rot(-Lean, 25.f * S.J(Seed, 6), 5.f * S.J(Seed, 7));
			S.Ice(i, FVector(D * 1.15f, W * 1.15f, H), FVector(X, Y, S.Gz(X, Y)), Rot, false, 60.f, 0.35f);
			S.Solid(C, FVector(D * 0.45f, W * 0.45f, H * 0.5f), Rot);
			if (i % 3 == 1 && i > 0 && i < 16)
			{
				FWRIsleHazard& H2 = S.Hazard(FWRIsleHazard::EKind::Serac, FVector(250.f, Y, S.Gz(250.f, Y)), 400.f);
				H2.Half = FVector(0, 0, H);
			}
		}
		for (int32 i = 0; i < 7; ++i)
		{
			const FVector Seed(i * 100.f, 900.f, 0.f);
			const float Y = -2700.f + 900.f * i + 150.f * S.J(Seed, 1), X = -800.f + 120.f * S.J(Seed, 2);
			S.Ice(40 + i, FVector(800.f + 300.f * S.R(Seed, 3), 950.f + 250.f * S.R(Seed, 4), 380.f + 260.f * S.R(Seed, 5)), FVector(X, Y, S.Gz(X, Y)), FRotator(-3.f, 90.f + 15.f * S.J(Seed, 6), 0.f), false, 80.f, 0.6f);
		}
		for (int32 k = 0; k < 10; ++k)
		{
			const FVector At(150.f + 300.f * S.R(FVector(k * 100.f), 8), -2300.f + 480.f * k, 0.f);
			S.Ice(k + 1, FVector(160.f + 120.f * S.R(At, 9), 140.f + 100.f * S.R(At, 10), 120.f + 90.f * S.R(At, 11)), FVector(At.X, At.Y, S.Gz(At.X, At.Y)), FRotator(20.f * S.J(At, 12), 360.f * S.R(At, 13), 15.f * S.J(At, 14)), true, 20.f);
		}
		S.Lamp(FVector(-100.f, 0.f, S.Gz(-100.f, 0.f) + 300.f), Lin(110, 180, 240), 2000.f, 2000.f);
		S.Fx(FVector(-120.f, 0.f, S.Gz(-120.f, 0.f) + 600.f), TEXT("ff.serac.drip"), 1.f);
	}

	void FoxPingo(FWRIsleSite& S, const FPal& P)
	{
		// An ice-cored mound: stacked snow terraces, a star crack in the top showing blue ice, one flank broken open to the
		// clear core. The aurora foxes gather here.
		const float G0 = S.Gz(0, 0) - 60.f;
		// One mound (SM_Pingo_A: a snow dome with a five-rayed crack in its crown and its flank broken open toward 210 degrees,
		// the ice showing on the steep faces; it collides as its shape). 11-22: stacked discs read as a flying saucer.
		if (S.Shape(TEXT("/Game/Art/Gen/SM_Pingo_A.SM_Pingo_A"), FVector(0.f, 0.f, G0), FRotator::ZeroRotator, FVector(1.f), true, false, FLinearColor::White, 0.5f))
		{
			const FVector Flank = FRotator(0.f, 210.f, 0.f).Vector() * 900.f + FVector(0, 0, G0 + 380.f);
			S.Lamp(Flank, Lin(120, 190, 255), 2200.f, 1400.f);
			S.Lamp(FVector(0, 0, G0 + 1100.f), Lin(140, 200, 255), 1500.f, 1000.f);
			for (int32 k = 0; k < 8; ++k)
			{
				const FVector At = FRotator(0.f, 45.f * k + 10.f, 0.f).Vector() * 1700.f;
				S.K.Add(P.SnowShade.Dim(0.85f), FVector(80.f, 50.f, 8.f), FVector(At.X, At.Y, S.Gz(At.X, At.Y) + 2.f), FRotator(0.f, 45.f * k, 0.f), EWRLookMesh::Cylinder);
			}
			return;
		}
		for (int32 t = 0; t < 6; ++t)
		{
			const float D = FMath::Lerp(2600.f, 700.f, t / 5.f);
			S.K.Add(t % 2 ? P.SnowShade : P.Snow, FVector(D, D * 0.92f, 190.f), FVector(0, 0, G0 + 95.f + 170.f * t), FRotator(0.f, 13.f * t, 0.f), EWRLookMesh::Cylinder);
		}
		S.Solid(FVector(0, 0, G0 + 250.f), FVector(900.f, 850.f, 250.f));
		S.Solid(FVector(0, 0, G0 + 650.f), FVector(500.f, 460.f, 200.f));
		S.Solid(FVector(0, 0, G0 + 950.f), FVector(260.f, 240.f, 110.f));
		const FVector Top(0, 0, G0 + 1040.f);
		for (int32 k = 0; k < 5; ++k) S.K.Add(P.DeepIce, FVector(360.f, 26.f, 50.f), Top + FRotator(0.f, 72.f * k, 0.f).Vector() * 150.f, FRotator(0.f, 72.f * k, 0.f));
		S.K.Add(P.IceGlow, FVector(140.f, 140.f, 30.f), Top, FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		const FVector Flank = FRotator(0.f, 210.f, 0.f).Vector() * 980.f + FVector(0, 0, G0 + 260.f);
		S.K.Add(P.Ice, FVector(260.f, 420.f, 380.f), Flank, FRotator(0.f, 210.f, -8.f));
		S.K.Add(P.IceGlow, FVector(60.f, 300.f, 260.f), Flank + FRotator(0.f, 210.f, 0.f).Vector() * 110.f, FRotator(0.f, 210.f, -8.f));
		S.Lamp(Flank + FRotator(0.f, 210.f, 0.f).Vector() * 200.f, Lin(120, 190, 255), 2200.f, 1400.f);
		S.Lamp(Top + FVector(0, 0, 60.f), Lin(140, 200, 255), 1500.f, 1000.f);
		// Fox scrapes round the foot.
		for (int32 k = 0; k < 8; ++k)
		{
			const FVector At = FRotator(0.f, 45.f * k + 10.f, 0.f).Vector() * 1500.f;
			S.K.Add(P.SnowShade.Dim(0.85f), FVector(80.f, 50.f, 8.f), FVector(At.X, At.Y, S.Gz(At.X, At.Y) + 2.f), FRotator(0.f, 45.f * k, 0.f), EWRLookMesh::Cylinder);
		}
	}

	void PalePines(FWRIsleSite& S, const FPal& P)
	{
		// Dead white pines round the Pale Hunter's ground, outside his ring: stripped bark, broken stubs, gnaw scars low
		// down. No light at all here: its threat is the dark.
		for (int32 i = 0; i < 18; ++i)
		{
			const float A = 20.f * i + 9.f * S.J(FVector(i * 100.f), 1);
			const float R = 3300.f + 2400.f * S.R(FVector(i * 100.f), 2);
			const FVector G(FMath::Cos(FMath::DegreesToRadians(A)) * R, FMath::Sin(FMath::DegreesToRadians(A)) * R, 0.f);
			DeadPine(S, P, FVector(G.X, G.Y, S.Gz(G.X, G.Y)), 900.f + 500.f * S.R(G, 3), 10 + i, i % 3 == 0);
		}
	}

	void IceArch(FWRIsleSite& S, const FPal& P)
	{
		// A natural ice bridge over Grimjaw's den: two buttresses 36 m apart outside his ring, the arch of ice blocks peaking
		// 13 m up (walkable on top), blue light inside the ice. The den beneath stays open.
		const float Span = 3600.f;
		const float Z0 = S.Gz(0, -Span), Z1 = S.Gz(0, Span);
		// One sweep of ice (SM_IceArch_A: blocky, split faces, icicles under it; walkable, it collides as its shape); 11-22:
		// 18 boxes end to end read as one flat slab from below.
		const bool bShape = S.Shape(TEXT("/Game/Art/Gen/SM_IceArch_A.SM_IceArch_A"), FVector(0.f, 0.f, (Z0 + Z1) * 0.5f), FRotator::ZeroRotator, FVector(1.f), true, false, FLinearColor::White, 0.55f) != nullptr;
		const int32 N = bShape ? 0 : 18;
		for (int32 i = 0; i < N; ++i)
		{
			const float T0 = float(i) / N, T1 = float(i + 1) / N;
			const auto At = [&](float T) { return FVector(0.f, -Span + 2.f * Span * T, FMath::Lerp(Z0, Z1, T) + 1300.f * FMath::Sin(PI * T)); };
			const FVector A = At(T0), B = At(T1);
			const FVector C = (A + B) * 0.5f;
			const FVector D = (B - A);
			const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(D.Z, D.Y));
			const FRotator Rot(0.f, 0.f, Pitch);
			const float Thick = 520.f - 160.f * FMath::Sin(PI * (T0 + T1) * 0.5f);
			S.K.Add(i % 3 ? P.DeepIce : P.Ice, FVector(620.f, D.Size() + 60.f, Thick), C - FVector(0, 0, Thick * 0.3f), Rot);
			S.K.Add(P.Snow, FVector(640.f, D.Size() + 40.f, 40.f), C + Rot.RotateVector(FVector(0, 0, Thick * 0.2f + 20.f)), Rot);
			S.Solid(C - FVector(0, 0, Thick * 0.3f), FVector(310.f, D.Size() * 0.5f + 30.f, Thick * 0.5f), Rot);
			if (i % 4 == 2) S.K.Add(P.IceGlow, FVector(400.f, D.Size() * 0.6f, 20.f), C - FVector(0, 0, Thick * 0.6f), Rot);
		}
		for (const float Sd : { -1.f, 1.f })
		{
			if (bShape) break;
			const FVector B(0.f, Sd * (Span + 200.f), (Sd < 0.f ? Z0 : Z1));
			S.K.Add(P.DeepIce, FVector(900.f, 700.f, 700.f), B + FVector(0, 0, 250.f), FRotator(0.f, 10.f * Sd, 4.f * Sd));
			S.K.Add(P.Snow, FVector(920.f, 720.f, 60.f), B + FVector(0, 0, 610.f), FRotator(0.f, 10.f * Sd, 4.f * Sd));
			S.Solid(B + FVector(0, 0, 250.f), FVector(450.f, 350.f, 350.f), FRotator(0.f, 10.f * Sd, 4.f * Sd));
		}
		S.Lamp(FVector(0.f, 0.f, (Z0 + Z1) * 0.5f + 1000.f), Lin(110, 180, 240), WRTune(TEXT("places.archGlowLux"), 3500.f), 2600.f);
		// The troll's den under the middle: a nest of hides, picked bones, snapped Rime-folk spears.
		const FVector Den(-600.f, 300.f, S.Gz(-600.f, 300.f));
		for (int32 h = 0; h < 5; ++h) S.K.Add(h % 2 ? P.Hide : P.HideDark, FVector(220.f, 160.f, 20.f), Den + FVector(60.f * S.J(FVector(h), 1), 60.f * S.J(FVector(h), 2), 10.f + 8.f * h), FRotator(4.f * S.J(FVector(h), 3), 37.f * h, 0.f));
		for (int32 b = 0; b < 8; ++b) S.K.Rod(P.Bone, Den + FVector(-300.f + 70.f * b, 260.f, 10.f), Den + FVector(-260.f + 70.f * b, 330.f + 30.f * (b % 2), 18.f), 12.f);
		for (int32 s = 0; s < 4; ++s)
		{
			const FVector At = Den + FVector(250.f + 60.f * s, -200.f + 90.f * s, 0.f);
			S.K.Rod(P.Willow.Dim(0.8f), At + FVector(0, 0, 8.f), At + FVector(150.f, 40.f * s, 14.f), 7.f);
			S.K.Rod(P.Willow.Dim(0.8f), At + FVector(170.f, 40.f * s, 10.f), At + FVector(260.f, 80.f * s, 8.f), 7.f);
		}
	}

	// ======================================================================================================= story sites

	void TuskerWallow(FWRIsleSite& S, const FPal& P)
	{
		// Old Tusker's lair, on his loop: a trampled ring of churned snow, tusk gouges in the ground, spear shafts standing
		// where the hunters lost them, a heap of chewed lichen-rock.
		for (int32 k = 0; k < 26; ++k)
		{
			const float A = 13.85f * k;
			const FVector At = FRotator(0.f, A, 0.f).Vector() * (1100.f + 200.f * S.R(FVector(k), 1));
			S.K.Add(P.SnowShade.Dim(0.9f + 0.15f * S.R(At, 2)), FVector(260.f, 180.f, 50.f), FVector(At.X, At.Y, S.Gz(At.X, At.Y) + 10.f), FRotator(0.f, A, 0.f), EWRLookMesh::Cylinder);
		}
		for (int32 g = 0; g < 6; ++g)
		{
			const FVector A = FRotator(0.f, 60.f * g, 0.f).Vector() * 500.f;
			const FVector D = FRotator(0.f, 60.f * g + 70.f, 0.f).Vector();
			S.K.Add(P.Gouge, FVector(420.f, 30.f, 8.f), FVector(A.X, A.Y, S.Gz(A.X, A.Y) + 2.f) + D * 100.f, D.Rotation());
		}
		for (int32 s = 0; s < 5; ++s)
		{
			const FVector At = FRotator(0.f, 72.f * s + 20.f, 0.f).Vector() * (700.f + 150.f * (s % 2));
			const FVector G(At.X, At.Y, S.Gz(At.X, At.Y));
			const FVector Dir(0.4f * S.J(G, 3), 0.4f * S.J(G, 4), 1.f);
			S.K.Rod(P.Willow.Dim(0.8f), G - Dir * 40.f, G + Dir.GetSafeNormal() * (120.f + 60.f * (s % 3)), 8.f);
		}
		for (int32 k = 0; k < 9; ++k)
		{
			const FVector At(-300.f + 60.f * (k % 3), 200.f + 60.f * (k / 3), 0.f);
			S.Rock(k, FVector(90.f, 80.f, 60.f), FVector(At.X, At.Y, S.Gz(At.X, At.Y) + 20.f * (k / 3)), FRotator(0.f, 40.f * k, 0.f), Lin(150, 140, 110), false, 10.f);
		}
	}

	void TuskMidden(FWRIsleSite& S, const FPal& P)
	{
		// Tusks stacked by the dozen in a frozen bank, its face exposed toward the shore (local +X).
		for (int32 r = 0; r < 3; ++r)
		{
			for (int32 k = 0; k < 5; ++k)
			{
				const FVector Base(-100.f + 40.f * r, -400.f + 200.f * k, S.Gz(0, -400.f + 200.f * k) + 60.f + 90.f * r);
				Tusk(S, P, Base, Base + FVector(160.f, 60.f, 120.f - 40.f * r), Base + FVector(380.f, 140.f * S.J(Base, 1), 40.f), 42.f - 6.f * r);
			}
		}
		S.K.Add(P.Snow, FVector(500.f, 1200.f, 380.f), FVector(-320.f, 0, S.Gz(0, 0) + 150.f), FRotator(0.f, 0.f, 0.f));
		S.K.Add(P.Ice, FVector(80.f, 1100.f, 300.f), FVector(-60.f, 0, S.Gz(0, 0) + 140.f), FRotator(-10.f, 0.f, 0.f));
		S.Solid(FVector(-320.f, 0, S.Gz(0, 0) + 150.f), FVector(250.f, 600.f, 190.f));
	}

	void SledgeLine(FWRIsleSite& S, const FPal& P)
	{
		// Three sledges abandoned in a line toward the coast (local +X), the harness straps lying cut, not torn.
		for (int32 i = 0; i < 3; ++i)
		{
			const FVector At = S.G(-900.f + 900.f * i, 120.f * S.J(FVector(i), 1), 0.f);
			Sledge(S, P, At, 6.f * S.J(At, 2), 10 + i, i != 1, i == 2 ? 14.f : 0.f);
			for (const float Sd : { -1.f, 1.f })
			{
				const FVector Strap = At + FVector(260.f, Sd * 40.f, 4.f);
				S.K.Add(P.HideDark, FVector(160.f, 6.f, 2.f), Strap, FRotator(0.f, Sd * 6.f, 0.f));
				S.K.Add(P.HideDark, FVector(70.f, 6.f, 2.f), Strap + FVector(140.f, Sd * 20.f, 0), FRotator(0.f, Sd * 30.f, 0.f));   // the cut end, clean
			}
		}
	}

	void FishingCamp(FWRIsleSite& S, const FPal& P)
	{
		// Out on the lake ice: three holes cut round (dark water in an ice rim), a fish-drying rack, a hide windbreak, a
		// dead fire.
		for (int32 h = 0; h < 3; ++h)
		{
			const FVector G = S.G(-300.f + 300.f * h, -200.f + 180.f * (h % 2), 0.f);
			S.K.Ring(P.Ice, G + FVector(0, 0, 6.f), 45.f, 20.f, 14.f, 12, 0.f, 360.f, 2.f);
			S.K.Add(P.Water, FVector(90.f, 90.f, 4.f), G + FVector(0, 0, 4.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			S.K.Rod(P.Willow, G + FVector(60.f, 0, 4.f), G + FVector(110.f, 30.f, 60.f), 3.f);
			S.K.Rod(P.HideDark, G + FVector(110.f, 30.f, 60.f), G + FVector(10.f, 5.f, 0.f), 1.f);
		}
		const FVector Rk = S.G(300.f, 300.f);
		for (const float Sd : { -1.f, 1.f }) S.K.Rod(P.Willow, Rk + FVector(0, Sd * 150.f, 0), Rk + FVector(0, Sd * 150.f, 190.f), 8.f);
		S.K.Rod(P.Willow, Rk + FVector(0, -160.f, 180.f), Rk + FVector(0, 160.f, 180.f), 6.f);
		for (int32 f = 0; f < 7; ++f)
		{
			S.K.Rod(P.HideDark, Rk + FVector(0, -120.f + 40.f * f, 180.f), Rk + FVector(0, -120.f + 40.f * f, 150.f), 1.f);
			S.K.Add(FSurf::Stone(Lin(150, 160, 168), 0.4f), FVector(10.f, 8.f, 50.f), Rk + FVector(0, -120.f + 40.f * f, 122.f));
		}
		const FVector Wb = S.G(-200.f, 420.f);
		for (int32 k = 0; k < 3; ++k) S.K.Rod(P.Willow, Wb + FVector(-150.f + 150.f * k, 0, 0), Wb + FVector(-150.f + 150.f * k, 0, 170.f), 6.f);
		S.K.Add(P.Hide, FVector(320.f, 6.f, 150.f), Wb + FVector(0, 0, 90.f), FRotator(0.f, 0.f, 6.f));
		const FVector Fire = S.G(-100.f, 200.f);
		for (int32 k = 0; k < 7; ++k) S.K.Add(P.GraniteDark, FVector(26.f, 22.f, 16.f), Fire + FRotator(0.f, 51.f * k, 0.f).Vector() * 50.f + FVector(0, 0, 8.f), FRotator(0.f, 51.f * k, 0.f));
		S.K.Add(P.Dark, FVector(70.f, 70.f, 6.f), Fire + FVector(0, 0, 4.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		// (Round 13: three holes and a rack on bare ice, sticks on a grey patch from any way off.) A camp people come back
		// to: a ridge tent in the lee of the windbreak, the sledge they haul it out on, a log to sit on at each hole and
		// the morning's catch frozen on the ice beside it, and the snow trodden between them, a track out toward the shore.
		const FVector Tent = S.G(-200.f, 600.f);
		S.K.Add(P.Hide, FVector(105.f, 100.f, 110.f), Tent - FVector(0, 0, 4.f), FRotator(0.f, -70.f, 0.f), EWRLookMesh::Tent);   // (a scale: 100 = the mesh's own 2.8 m)
		Sledge(S, P, S.G(180.f, 560.f), 25.f, 5, true);
		const FSurf Log = FSurf::Wood(Lin(92, 74, 56));
		const FSurf Fish = FSurf::Stone(Lin(118, 128, 136), 0.3f);
		for (int32 h = 0; h < 3; ++h)
		{
			const FVector2D Hc(-300.f + 300.f * h, -200.f + 180.f * (h % 2));
			const FVector Seat = S.G(Hc.X - 20.f, Hc.Y + 95.f);
			S.K.Add(Log, FVector(26.f, 26.f, 80.f), Seat + FVector(0, 0, 12.f), FRotator(90.f, 15.f * h - 10.f, 0.f), EWRLookMesh::Cylinder);
			for (int32 k = 0; k < 2 + h % 2; ++k)
			{
				const FVector At = S.G(Hc.X + 70.f + 18.f * k, Hc.Y - 60.f + 26.f * k, 4.f);
				S.K.Add(Fish, FVector(36.f, 9.f, 7.f), At, FRotator(0.f, 70.f + 35.f * k + 20.f * h, 0.f), EWRLookMesh::Sphere);
			}
		}
		if (S.bDraw)
		{
			const auto W = [&](float X, float Y) { return FVector2D(S.ToWorld(FVector(X, Y, 0.f))); };
			const FWRTrailLook Trod = FWRTrailLook::Snow();
			WRGroundTrail::Patch(S.Owner, S.Root, W(-100.f, 200.f), 170.f, 330.f, Trod, 21);
			for (int32 h = 0; h < 3; ++h)
			{
				const FVector2D Hc(-300.f + 300.f * h, -200.f + 180.f * (h % 2));
				WRGroundTrail::Path(S.Owner, S.Root, { W(-100.f, 200.f), W((Hc.X - 100.f) * 0.5f + 30.f * (h - 1), (Hc.Y + 200.f) * 0.5f), W(Hc.X, Hc.Y + 60.f) }, 55.f, Trod, 30 + h);
			}
			WRGroundTrail::Path(S.Owner, S.Root, { W(-100.f, 200.f), W(100.f, 280.f), W(280.f, 300.f) }, 55.f, Trod, 34);
			WRGroundTrail::Path(S.Owner, S.Root, { W(-100.f, 200.f), W(-160.f, 380.f), W(-200.f, 470.f) }, 60.f, Trod, 35);
			WRGroundTrail::Path(S.Owner, S.Root, { W(-100.f, 200.f), W(60.f, 420.f), W(170.f, 520.f) }, 60.f, Trod, 36);
			WRGroundTrail::Path(S.Owner, S.Root, { W(-260.f, 160.f), W(-900.f, 260.f), W(-1700.f, 180.f), W(-2600.f, 320.f) }, 70.f, Trod, 37);
		}
	}

	void DrownedSledge(FWRIsleSite& S, const FPal& P)
	{
		// A sledge and its dogs under a window of clear ice: seen from above, they went through. Points to the thin ice.
		const FVector C = S.G(0, 0, 0);
		Sledge(S, P, C - FVector(0, 0, 70.f), 20.f, 3, true, 8.f);
		for (int32 d = 0; d < 4; ++d)
		{
			const FVector At = C + FRotator(0.f, 20.f, 0.f).RotateVector(FVector(260.f + 90.f * (d / 2), (d % 2 ? 50.f : -50.f), -60.f));
			S.K.Add(P.HideDark, FVector(90.f, 30.f, 34.f), At, FRotator(0.f, 20.f + 15.f * S.J(At, 1), 70.f * (d % 2 ? 1 : -1)), EWRLookMesh::Cylinder);
		}
		S.K.Add(FSurf::Glass(Lin(150, 200, 220), 0.f), FVector(700.f, 420.f, 10.f), C + FVector(100.f, 0, 4.f), FRotator(0.f, 20.f, 0.f), EWRLookMesh::Cylinder);
	}

	void HunterCamp(FWRIsleSite& S, const FPal& P)
	{
		// One tent. A hunter's boots stand upright at the flap, facing out into the pines (local +X, toward the Pale Hunter's
		// ground); prints lead away and after six steps become claw prints. Gnawed bones in the fire ring.
		const FVector T = S.G(-200.f, 0.f);
		for (int32 k = 0; k < 7; ++k)
		{
			const FVector D = FRotator(0.f, 51.4f * k, 0.f).Vector();
			S.K.Rod(P.Willow, T + D * 170.f - FVector(0, 0, 20.f), T + FVector(0, 0, 300.f) + D * 12.f, 7.f);
		}
		for (int32 k = 0; k < 7; ++k)
		{
			if (k == 0) continue;   // the flap's open on the +X side
			const float A0 = 51.4f * k;
			const FVector Mid = T + FRotator(0.f, A0, 0.f).Vector() * 95.f + FVector(0, 0, 140.f);
			S.K.Add(k % 2 ? P.Hide : P.HideDark, FVector(4.f, 120.f, 300.f), Mid, FRotator(-30.f, A0, 0.f));
		}
		const FVector Boots = T + FVector(190.f, 0, 0);
		for (const float Sd : { -1.f, 1.f })
		{
			S.K.Add(P.HideDark, FVector(26.f, 12.f, 34.f), Boots + FVector(0, Sd * 12.f, 17.f));
			S.K.Add(P.HideDark, FVector(30.f, 12.f, 10.f), Boots + FVector(8.f, Sd * 12.f, 5.f));
		}
		for (int32 s = 0; s < 14; ++s)
		{
			const FVector At = Boots + FVector(80.f + 70.f * s, (s % 2 ? 16.f : -16.f) + 3.f * s, 0.f);
			const FVector G(At.X, At.Y, S.Gz(At.X, At.Y) + 2.f);
			if (s < 6) S.K.Add(P.SnowShade.Dim(0.7f), FVector(28.f, 11.f, 3.f), G, FRotator(0.f, 0.f, 0.f));
			else
			{
				S.K.Add(P.SnowShade.Dim(0.65f), FVector(24.f, 22.f, 3.f), G);
				for (int32 c = 0; c < 4; ++c) S.K.Add(P.Dark.Dim(3.f), FVector(10.f, 3.f, 3.f), G + FVector(18.f, -9.f + 6.f * c, 0), FRotator(0.f, -20.f + 13.f * c, 0.f));
			}
		}
		const FVector Fire = T + FVector(-60.f, 260.f, 0);
		for (int32 k = 0; k < 8; ++k) S.K.Add(P.GraniteDark, FVector(26.f, 22.f, 16.f), Fire + FRotator(0.f, 45.f * k, 0.f).Vector() * 55.f + FVector(0, 0, 8.f), FRotator(0.f, 45.f * k, 0.f));
		for (int32 b = 0; b < 5; ++b) S.K.Rod(P.Bone, Fire + FVector(-30.f + 15.f * b, -20.f, 10.f), Fire + FVector(-10.f + 12.f * b, 25.f, 14.f), 5.f);
	}

	void HuntersBlind(FWRIsleSite& S, const FPal& P)
	{
		// A hide of poles and pine boughs over a game trail (local +X looks down the trail), a bow rack inside.
		const FVector C = S.G(0, 0);
		for (int32 k = 0; k < 6; ++k)
		{
			const FVector D = FRotator(0.f, 180.f + 36.f * (k - 2.5f), 0.f).Vector();
			S.K.Rod(P.Willow, C + D * 170.f - FVector(0, 0, 20.f), C + FVector(40.f, 0, 200.f), 8.f);
			S.K.Add(FSurf::Cloth(Lin(40, 56, 46)), FVector(10.f, 140.f, 200.f), C + D * 100.f + FVector(20.f, 0, 100.f), FRotator(-35.f, D.Rotation().Yaw, 0.f));
		}
		S.K.Rod(P.Willow, C + FVector(-60.f, -50.f, 120.f), C + FVector(-60.f, 50.f, 120.f), 5.f);
		S.K.Rod(P.WoodDark, C + FVector(-50.f, -40.f, 40.f), C + FVector(-50.f, -10.f, 160.f), 4.f);
		for (int32 t = 0; t < 10; ++t) S.K.Add(P.SnowShade.Dim(0.75f), FVector(16.f, 10.f, 3.f), S.G(300.f + 80.f * t, 30.f * FMath::Sin(t * 0.7f), 2.f), FRotator(0.f, 10.f * FMath::Cos(t * 0.7f), 0.f));
	}

	void GiantPine(FWRIsleSite& S, const FPal& P)
	{
		// A 34 m pine lying across the hollow (local +X along it), its root plate standing 6 m tall at the west end; you can
		// walk its trunk.
		const FVector A = S.G(-1700.f, 0.f, 70.f), B = S.G(1700.f, 0.f, 50.f);
		S.K.Rod(P.WoodDark, A, B, 150.f);
		const FVector Dir = (B - A).GetSafeNormal();
		S.Solid((A + B) * 0.5f, FVector(FVector::Dist(A, B) * 0.5f, 70.f, 70.f), Dir.Rotation());
		for (int32 b = 0; b < 14; ++b)
		{
			const FVector At = A + Dir * (500.f + 200.f * b);
			const float Sd = b % 2 ? 1.f : -1.f;
			S.K.Rod(P.WoodDark.Dim(0.9f), At, At + FVector(80.f, Sd * (220.f - 10.f * b), -60.f + 40.f * (b % 3)), 18.f);
		}
		const FVector Root = A - Dir * 80.f;
		S.K.Add(P.WoodDark.Dim(0.7f), FVector(140.f, 620.f, 600.f), Root + FVector(0, 0, 160.f), FRotator(0.f, 0.f, 0.f), EWRLookMesh::Cylinder);
		S.K.Add(FSurf::Stone(Lin(80, 70, 60), 0.95f), FVector(150.f, 560.f, 520.f), Root + FVector(-30.f, 0, 150.f), FRotator(0.f, 0.f, 0.f), EWRLookMesh::Cylinder);
		for (int32 r = 0; r < 9; ++r) S.K.Rod(P.WoodDark, Root + FRotator(0.f, 0.f, 40.f * r).RotateVector(FVector(-60.f, 0, 260.f)) + FVector(0, 0, 160.f), Root + FRotator(0.f, 0.f, 40.f * r).RotateVector(FVector(-120.f, 0, 400.f)) + FVector(0, 0, 160.f), 16.f);
		S.Solid(Root + FVector(0, 0, 160.f), FVector(70.f, 300.f, 300.f));
	}

	void GlacimawBoneyard(FWRIsleSite& S, const FPal& P)
	{
		// A ring of ice-polished bones round churned snow where the Glacimaw surfaces.
		for (int32 b = 0; b < 20; ++b)
		{
			const float A = 18.f * b;
			const FVector At = FRotator(0.f, A, 0.f).Vector() * (650.f + 120.f * S.R(FVector(b), 1));
			const FVector G(At.X, At.Y, S.Gz(At.X, At.Y));
			if (b % 3 == 0) S.K.Rod(P.Bone, G + FVector(0, 0, -20.f), G + FVector(30.f * S.J(G, 2), 30.f * S.J(G, 3), 120.f + 80.f * S.R(G, 4)), 22.f);
			else S.K.Rod(P.Bone.Dim(0.9f), G + FVector(0, 0, 10.f), G + FRotator(0.f, A + 90.f, 0.f).Vector() * (100.f + 60.f * S.R(G, 5)) + FVector(0, 0, 14.f), 16.f);
		}
		for (int32 k = 0; k < 12; ++k)
		{
			const FVector At = FRotator(0.f, 30.f * k, 0.f).Vector() * (200.f + 220.f * S.R(FVector(k), 6));
			S.K.Add(P.SnowShade.Dim(0.9f), FVector(200.f, 140.f, 60.f), FVector(At.X, At.Y, S.Gz(At.X, At.Y) + 15.f), FRotator(10.f * S.J(At, 7), 30.f * k, 0.f), EWRLookMesh::Cylinder);
		}
	}

	void ClawScars(FWRIsleSite& S, const FPal& P)
	{
		// Four gouges 6 m long down a rock face where Skaldrith lands, ice in their floors; the rock leans toward the bowl.
		S.Rock(1, FVector(1400.f, 900.f, 800.f), S.G(-300.f, 0.f, 0.f), FRotator(0.f, 90.f, 0.f), Lin(120, 132, 146), true, 120.f);
		for (int32 g = 0; g < 4; ++g)
		{
			const FVector Top(250.f, -240.f + 160.f * g, S.Gz(250.f, 0.f) + 640.f);
			const FVector Bot = Top + FVector(80.f, 60.f, -560.f);
			S.K.Rod(P.Gouge, Top, Bot, 40.f, true);
			S.K.Rod(P.Ice, Top + FVector(8.f, 0, -40.f), Bot + FVector(8.f, 0, 40.f), 16.f);
		}
	}

	void WatchCairn(FWRIsleSite& S, const FPal& P)
	{
		// A Rime-folk watch post toward the north pass (local +X): a ring wall of stacked stone, a spear rack facing the pass.
		for (int32 k = 0; k < 18; ++k)
		{
			if (k == 9) continue;   // the gap you step in by
			const float A = 20.f * k;
			const FVector D = FRotator(0.f, A, 0.f).Vector();
			for (int32 c = 0; c < 3; ++c)
			{
				const FVector At = D * 300.f;
				S.K.Add(P.Granite.Dim(0.85f + 0.25f * S.R(FVector(k, c, 0), 1)), FVector(100.f, 60.f, 40.f), FVector(At.X, At.Y, S.Gz(At.X, At.Y) + 20.f + 40.f * c), FRotator(0.f, A + 90.f + 6.f * S.J(FVector(k, c, 0), 2), 0.f));
			}
			if (k % 3 == 0) S.Solid(FVector(D.X * 300.f, D.Y * 300.f, S.Gz(D.X * 300.f, D.Y * 300.f) + 60.f), FVector(55.f, 60.f, 60.f), FRotator(0.f, A + 90.f, 0.f));
		}
		const FVector Rk(200.f, 0.f, S.Gz(200.f, 0.f));
		S.K.Rod(P.Willow, Rk + FVector(0, -110.f, 0), Rk + FVector(0, -110.f, 150.f), 8.f);
		S.K.Rod(P.Willow, Rk + FVector(0, 110.f, 0), Rk + FVector(0, 110.f, 150.f), 8.f);
		S.K.Rod(P.Willow, Rk + FVector(0, -120.f, 140.f), Rk + FVector(0, 120.f, 140.f), 6.f);
		for (int32 s = 0; s < 4; ++s)
		{
			const FVector B = Rk + FVector(-30.f, -80.f + 55.f * s, 0.f);
			S.K.Rod(P.Willow.Dim(0.9f), B, B + FVector(60.f, 0, 260.f), 5.f);
			S.K.Spike(P.Iron, B + FVector(60.f, 0, 260.f), FVector(0.23f, 0, 1.f), 8.f, 30.f);
		}
	}

	void Moulin(FWRIsleSite& S, const FPal& P)
	{
		// A meltwater shaft into the glacier: a ring of blue ice blocks round a black mouth, channels running in. You hear its
		// roar long before you see it.
		const FVector C = S.G(0, 0);
		for (int32 k = 0; k < 14; ++k)
		{
			const float A = 25.7f * k;
			const FVector D = FRotator(0.f, A, 0.f).Vector();
			S.K.Add(k % 2 ? P.DeepIce : P.Ice, FVector(140.f, 120.f, 90.f + 40.f * S.R(D, 1)), C + D * 260.f + FVector(0, 0, 20.f), FRotator(-15.f, A, 0.f));
		}
		S.K.Add(P.Dark, FVector(400.f, 400.f, 20.f), C + FVector(0, 0, 4.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		S.K.Add(P.IceGlow, FVector(440.f, 440.f, 6.f), C - FVector(0, 0, 4.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		for (int32 c = 0; c < 3; ++c)
		{
			const FVector D = FRotator(0.f, 120.f * c + 30.f, 0.f).Vector();
			for (int32 s = 0; s < 6; ++s) S.K.Add(P.Water, FVector(130.f, 40.f, 4.f), S.G(D.X * (360.f + 120.f * s), D.Y * (360.f + 120.f * s), 3.f), D.Rotation());
		}
		S.Fx(C + FVector(0, 0, 50.f), TEXT("ff.moulin.mist"), 1.f);
		S.Lamp(C - FVector(0, 0, 60.f), Lin(110, 180, 240), 1800.f, 900.f, true);
	}

	void CrevasseMaze(FWRIsleSite& S, const FPal& P)
	{
		// Five crevasses 4.4 m wide and 4.5 m deep (too far to jump), running down the glacier's slope 8.4 m apart, crossed
		// by bridges. The Rime-folk's safe line runs along column 1: its crossings are old ice bridges that hold, marked by
		// willow poles with red rags. Everywhere else the crossings are snow bridges: one creaks and drops 0.9 s after the
		// first step (keep running and you're over; stop and you fall into the crevasse, and walk out along its floor, which
		// climbs to the surface at each end).
		// (Round 12, Raven: "The maze looks like shit": it was separate cubes, a pile of dice from the air; then rows of
		// fluted ice stood on a plane fitted to the slope, from the air a raised slab whose edge stepped along the 4 m
		// ground. Round 14: the crevasses are cut into the glacier itself (round_ground.mjs "crevasses", drawn by
		// WRLandCaves as the caves' mouths are: their ice walls, the ground's own snow over the rims); the glacier between
		// them is the ground. Here: the bridges, the poles, ice fallen in, the seracs round the field.)
		const FWRLandDef* L = WRLand::At(S.World.GetLocation());
		const FWRLandCrevasses* Field = nullptr;
		if (L) for (const FWRLandCrevasses& F : L->Crevasses) if (F.Id == TEXT("crevasse_maze")) { Field = &F; break; }
		if (!Field || Field->Slots.Num() == 0)
		{
			UE_LOG(LogWildreach, Warning, TEXT("ISLE PLACES crevasse maze: no crevasses in the land (round_ground.mjs)"));
			return;
		}
		// The field's frame in the place's: U across the crevasses, V down them; its middle.
		const FVector U = S.World.InverseTransformVectorNoScale(FVector(Field->U, 0.f)).GetSafeNormal2D();
		const FVector V = S.World.InverseTransformVectorNoScale(FVector(Field->V, 0.f)).GetSafeNormal2D();
		const FVector Mid = S.Local(L->ToWorld(Field->At, 0.f)) * FVector(1.f, 1.f, 0.f);
		const auto At2 = [&](float Ux, float Vy) { const FVector Q = Mid + U * Ux + V * Vy; return FVector(Q.X, Q.Y, S.Gz(Q.X, Q.Y)); };
		const float YawU = FMath::RadiansToDegrees(FMath::Atan2(U.Y, U.X));
		const int32 Cols = 5, Safe = 1;
		const float PitchY = 520.f, Y0 = -1040.f;
		const auto YAt = [&](int32 c) { return Y0 + PitchY * c; };
		// Seeded 0..1 / -1..1 for a crevasse, a column, a piece (the kit hashes 5 cm cells of a place: indices a metre apart).
		const auto Rn = [&](int32 A, int32 B, int32 C, int32 Salt) { return S.R(FVector(A * 100.f, B * 100.f, C * 100.f), Salt); };
		const auto Jn = [&](int32 A, int32 B, int32 C, int32 Salt) { return S.J(FVector(A * 100.f, B * 100.f, C * 100.f), Salt); };
		const int32 NSlots = Field->Slots.Num();
		TArray<float> SlotU;
		for (const FWRLandCrevasses::FSlot& Sl : Field->Slots) SlotU.Add(float(FVector2D::DotProduct(Sl.C - Field->At, Field->U)) * 100.f);
		// The safe line's willow poles: on the ice between the crevasses (and beyond the outer two), beside its crossings.
		for (int32 r = 0; r <= NSlots; ++r)
		{
			const float Ux = r == 0 ? SlotU[0] - 420.f : r == NSlots ? SlotU[NSlots - 1] + 420.f : (SlotU[r - 1] + SlotU[r]) * 0.5f;
			const FVector Pole = At2(Ux + 120.f, YAt(Safe) + 140.f);
			S.K.Rod(P.Willow, Pole - FVector(0, 0, 25.f), Pole + FVector(0, 0, 210.f), 5.f);
			S.K.Add(P.Red, FVector(4.f, 24.f, 40.f), Pole + FVector(4.f, 12.f, 190.f), FRotator(0.f, YawU + 90.f, 10.f));
		}
		static const TCHAR* Chunks[3] = { TEXT("/Game/Art/Gen/SM_IceBlock_A.SM_IceBlock_A"), TEXT("/Game/Art/Gen/SM_IceBlock_B.SM_IceBlock_B"), TEXT("/Game/Art/Gen/SM_IceBlock_C.SM_IceBlock_C") };
		for (int32 r = 0; r < NSlots; ++r)
		{
			const FWRLandCrevasses::FSlot& Sl = Field->Slots[r];
			const float Reach = Sl.HalfW * 100.f + 80.f;   // a bridge's ends rest 80 cm past each lip
			// The crossings: the safe line's ice bridge, and snow bridges (two on every other crevasse, one between), each
			// at its own place along it: none line up across the field but the safe line's.
			static const int32 Others[4] = { 0, 2, 3, 4 };
			const int32 Da = FMath::Min(3, int32(4.f * Rn(r, 0, 92, 93)));
			const int32 Decoy0 = Others[Da];
			const int32 Decoy1 = r % 2 == 0 ? Others[(Da + 1 + FMath::Min(2, int32(3.f * Rn(r, 0, 94, 95)))) % 4] : -1;
			for (int32 c = 0; c < Cols; ++c)
			{
				const bool bSafe = c == Safe;
				if (!bSafe && c != Decoy0 && c != Decoy1) continue;
				const float Yb = YAt(c) + (bSafe ? 0.f : 120.f * Jn(r, c, 90, 91));
				const FVector Ea = At2(SlotU[r] - Reach, Yb) + FVector(0, 0, 6.f), Eb = At2(SlotU[r] + Reach, Yb) + FVector(0, 0, 6.f);
				const FVector At = (Ea + Eb) * 0.5f;
				const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(Eb.Z - Ea.Z, FVector::Dist2D(Ea, Eb)));
				// (the bridge's own X runs across the crevasse; pitched to its two lips)
				const FQuat QB = FRotator(Pitch, YawU, 0.f).Quaternion();
				const FVector Half(300.f, bSafe ? 145.f : 118.f, 25.f);   // (a snow bridge's narrow middle)
				const FVector BoxAt = At + QB.RotateVector(FVector(0.f, 0.f, -Half.Z - 4.f));
				const TCHAR* Mesh = bSafe ? TEXT("/Game/Art/Gen/SM_IceBridge_A.SM_IceBridge_A") : (c + r) % 4 == 0 ? TEXT("/Game/Art/Gen/SM_SnowBridge_A.SM_SnowBridge_A") : TEXT("/Game/Art/Gen/SM_SnowBridge_B.SM_SnowBridge_B");
				// (ice: its snow only on its top, the blue under it shows; snow: white through)
				UStaticMeshComponent* Look = S.Shape(Mesh, At, QB.Rotator(), FVector(1.f), false, false, FLinearColor::White, bSafe ? 0.f : 1.f);
				if (bSafe)
				{
					S.Solid(BoxAt, Half, QB.Rotator());
					continue;
				}
				FWRIsleHazard& H2 = S.Hazard(FWRIsleHazard::EKind::SnowBridge, BoxAt, 160.f);
				H2.Half = Half;
				H2.Rot = (S.World.GetRotation() * QB).Rotator();
				if (Look) H2.Whole.Add(Look);
				H2.Solid.Add(S.Solid(BoxAt, Half, QB.Rotator()));
			}
			// A few blocks of ice broken off the walls, lying on the crevasse's floor.
			for (int32 k = 0; k < 2; ++k)
			{
				const float Y = (-0.75f + 1.5f * Rn(r, k, 70, 71)) * Sl.HalfLen * 100.f;
				const FVector G0 = At2(SlotU[r] + 70.f * Jn(r, k, 72, 73), Y);
				const float Sc = 0.22f + 0.14f * Rn(r, k, 74, 75);
				S.Shape(Chunks[(r + k) % 3], FVector(G0.X, G0.Y, G0.Z - Sl.Depth * 100.f - 20.f), FRotator(25.f * Jn(r, k, 76, 77), 360.f * Rn(r, k, 78, 79), 30.f * Jn(r, k, 80, 81)), FVector(Sc), false, false, FLinearColor::White, 0.f);
			}
		}
		// Round the field the glacier breaks into a few seracs, most below it (down the slope), one or two off its sides.
		static const TCHAR* Seracs[4] = { TEXT("/Game/Art/Gen/SM_Serac_A.SM_Serac_A"), TEXT("/Game/Art/Gen/SM_Serac_B.SM_Serac_B"), TEXT("/Game/Art/Gen/SM_Serac_C.SM_Serac_C"), TEXT("/Game/Art/Gen/SM_Serac_D.SM_Serac_D") };
		const FVector2D SeracAt[6] = { { -1800.f, 4300.f }, { 700.f, 4700.f }, { 2900.f, 4200.f }, { -3300.f, -900.f }, { 3300.f, 1400.f }, { 400.f, -4500.f } };
		for (int32 i = 0; i < 6; ++i)
		{
			const FVector A = At2(SeracAt[i].X + 150.f * Jn(i, 0, 60, 61), SeracAt[i].Y + 150.f * Jn(i, 0, 62, 63));
			const float Sc = 0.6f + 0.35f * Rn(i, 0, 64, 65);
			S.Shape(Seracs[i % 4], FVector(A.X, A.Y, A.Z - 60.f), FRotator(0.f, 360.f * Rn(i, 0, 66, 67), 0.f), FVector(Sc), false, false, FLinearColor::White, 0.75f);
			S.Solid(FVector(A.X, A.Y, A.Z + 260.f * Sc), FVector(200.f * Sc, 160.f * Sc, 280.f * Sc));
		}
	}

	void Sastrugi(FWRIsleSite& S, const FPal& P)
	{
		// Wind-carved snow ridges: low, sharp-crested, their long axis along the wind off the glacier (local +X points at
		// the bowl), the blunt end upwind and the tail tapering away. Read them and you know which way the glacier lies.
		for (int32 i = 0; i < 48; ++i)
		{
			const float A = i * 137.508f, R = 3000.f * FMath::Sqrt((i + 0.5f) / 48.f);
			const FVector L(FMath::Cos(FMath::DegreesToRadians(A)) * R, FMath::Sin(FMath::DegreesToRadians(A)) * R, 0.f);
			const float Len = 220.f + 260.f * S.R(L, 1), Wid = 50.f + 40.f * S.R(L, 2), Ht = 18.f + 22.f * S.R(L, 3);
			const float Yaw = 6.f * S.J(L, 4);
			const FVector G(L.X, L.Y, S.Gz(L.X, L.Y));
			const FRotator Rot(0.f, Yaw, 0.f);
			// The body, its crest, and the tapering tail (downwind is local -X).
			S.K.Add(P.Snow, FVector(Len, Wid, Ht), G + FVector(0, 0, Ht * 0.35f), Rot + FRotator(0.f, 0.f, 4.f * S.J(L, 5)));
			S.K.Add(P.SnowShade.Dim(1.02f), FVector(Len * 0.8f, Wid * 0.28f, Ht * 0.5f), G + FVector(0, 0, Ht * 0.8f) + Rot.RotateVector(FVector(Len * 0.05f, Wid * 0.1f, 0.f)), Rot);
			S.K.Add(P.Snow.Dim(0.97f), FVector(Len * 0.5f, Wid * 0.6f, Ht * 0.5f), G + FVector(0, 0, Ht * 0.18f) + Rot.RotateVector(FVector(-Len * 0.6f, 0.f, 0.f)), Rot + FRotator(-3.f, 0.f, 0.f));
		}
	}

	void ThinIcePatch(FWRIsleSite& S, const FPal& P)
	{
		// A dark patch on the lake: thin ice heaved a little proud, ringed by white pressure ridges. Too much weight on it
		// for too long (or a heavy landing) breaks it: you drop into icy water and must climb its rim.
		const float R = FMath::Max(400.f, float(S.Row.IsValid() ? S.Row->GetNumberField(TEXT("r")) : 7.0) * 100.f);
		const float G0 = S.Gz(0, 0);
		const float Rise = 30.f;
		FWRIsleHazard& H = S.Hazard(FWRIsleHazard::EKind::ThinIce, FVector(0, 0, G0), R);
		H.Half = FVector(R, R, Rise);
		// Its whole look: the dark sheet with faint white lines frozen in.
		if (S.bDraw)
		{
			TArray<FWRLookBlock> Whole, Broken;
			{
				const int32 B0 = S.Blocks.Num();
				S.K.Add(P.ThinIce, FVector(R * 2.f, R * 2.f, Rise), FVector(0, 0, G0 + Rise * 0.5f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
				for (int32 l = 0; l < 5; ++l) S.K.Add(FSurf::Stone(Lin(150, 170, 182), 0.3f), FVector(R * (0.6f + 0.3f * S.R(FVector(l), 1)), 6.f, 2.f), FVector(R * 0.2f * S.J(FVector(l), 2), R * 0.2f * S.J(FVector(l), 3), G0 + Rise + 1.f), FRotator(0.f, 36.f * l, 0.f));
				for (int32 b = B0; b < S.Blocks.Num(); ++b) Whole.Add(S.Blocks[b]);
				S.Blocks.SetNum(B0);
			}
			{
				const int32 B0 = S.Blocks.Num();
				S.K.Add(P.Water, FVector(R * 1.9f, R * 1.9f, 6.f), FVector(0, 0, G0 + Rise * 0.6f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
				for (int32 k = 0; k < 12; ++k)
				{
					const FVector D = FRotator(0.f, 30.f * k + 9.f * S.J(FVector(k), 4), 0.f).Vector();
					S.K.Add(P.ThinIce, FVector(R * 0.42f, R * 0.3f, 10.f), FVector(0, 0, G0 + Rise * 0.7f) + D * R * 0.72f, FRotator(14.f * S.J(D, 5), 30.f * k, 18.f * S.J(D, 6)));
				}
				for (int32 b = B0; b < S.Blocks.Num(); ++b) Broken.Add(S.Blocks[b]);
				S.Blocks.SetNum(B0);
			}
			for (int32 Which = 0; Which < 2; ++Which)
			{
				USceneComponent* Part = NewObject<USceneComponent>(S.Owner);
				Part->SetupAttachment(S.Root);
				Part->RegisterComponent();
				FWRBuildLook::Spawn(S.Owner, Part, Which == 0 ? Whole : Broken, Lin(200, 214, 228), false);
				(Which == 0 ? H.Whole : H.Broken).Add(Part);
				if (Which == 1) Part->SetVisibility(false, true);
			}
		}
		// The ice you stand on (an octagon of two squares), and the rim you climb out over once it's broken.
		const float A = R * 0.95f / 1.0824f * 1.0f;
		H.Solid.Add(S.Solid(FVector(0, 0, G0 + Rise * 0.5f), FVector(A, A, Rise * 0.5f)));
		H.Solid.Add(S.Solid(FVector(0, 0, G0 + Rise * 0.5f), FVector(A, A, Rise * 0.5f), FRotator(0.f, 45.f, 0.f)));
		for (int32 k = 0; k < 12; ++k)
		{
			const FVector D = FRotator(0.f, 30.f * k, 0.f).Vector();
			UBoxComponent* Rim = S.Solid(FVector(0, 0, G0) + D * (R + 30.f) + FVector(0, 0, 40.f), FVector(40.f, R * 0.27f, 45.f), FRotator(0.f, 30.f * k, 0.f));
			if (Rim) Rim->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			H.RimSolid.Add(Rim);
		}
		// The pressure ridges round it (always there: the tell before any crack).
		for (int32 k = 0; k < 16; ++k)
		{
			const FVector D = FRotator(0.f, 22.5f * k + 6.f * S.J(FVector(k), 7), 0.f).Vector();
			S.K.Add(k % 2 ? P.Snow : P.Ice, FVector(140.f + 80.f * S.R(D, 8), 90.f, 50.f + 40.f * S.R(D, 9)), FVector(0, 0, G0 + 25.f) + D * (R + 70.f), FRotator(20.f * S.J(D, 10), 22.5f * k + 90.f, 15.f * S.J(D, 11)));
		}
	}
}

namespace WRIsleT
{
	/** The dispatch, inside this file's namespace so its names win over any other file's in a unity build. */
	bool BuildImpl(FWRIsleSite& S, const FString& Kind)
	{
		const FPal P;
		const float Landmark = WRTune(TEXT("places.landmarkCull"), 500000.f), Site = WRTune(TEXT("places.siteCull"), 60000.f);
		if (Kind == TEXT("tusk_gate")) { TuskGate(S, P); S.Done(Landmark); return true; }
		if (Kind == TEXT("great_inuksuk")) { GreatInuksuk(S, P); S.Done(Landmark); return true; }
		if (Kind == TEXT("frozen_whaler")) { FrozenWhaler(S, P); S.Done(Landmark); return true; }
		if (Kind == TEXT("weeping_seracs")) { WeepingSeracs(S, P); S.Done(Landmark); return true; }
		if (Kind == TEXT("fox_pingo")) { FoxPingo(S, P); S.Done(Landmark); return true; }
		if (Kind == TEXT("pale_pines")) { PalePines(S, P); S.Done(Landmark); return true; }
		if (Kind == TEXT("ice_arch")) { IceArch(S, P); S.Done(Landmark); return true; }
		if (Kind == TEXT("tusker_wallow")) { TuskerWallow(S, P); S.Done(Site); return true; }
		if (Kind == TEXT("tusk_midden")) { TuskMidden(S, P); S.Done(Site); return true; }
		if (Kind == TEXT("sledge_line")) { SledgeLine(S, P); S.Done(Site); return true; }
		if (Kind == TEXT("fishing_camp")) { FishingCamp(S, P); S.Done(Site); return true; }
		if (Kind == TEXT("drowned_sledge")) { DrownedSledge(S, P); S.Done(Site); return true; }
		if (Kind == TEXT("hunter_camp")) { HunterCamp(S, P); S.Done(Site); return true; }
		if (Kind == TEXT("hunters_blind")) { HuntersBlind(S, P); S.Done(Site); return true; }
		if (Kind == TEXT("giant_pine")) { GiantPine(S, P); S.Done(Site); return true; }
		if (Kind == TEXT("glacimaw_boneyard")) { GlacimawBoneyard(S, P); S.Done(Site); return true; }
		if (Kind == TEXT("claw_scars")) { ClawScars(S, P); S.Done(Site); return true; }
		if (Kind == TEXT("watch_cairn")) { WatchCairn(S, P); S.Done(Site); return true; }
		if (Kind == TEXT("moulin")) { Moulin(S, P); S.Done(Site); return true; }
		if (Kind == TEXT("crevasse_maze")) { CrevasseMaze(S, P); S.Done(Site); return true; }
		if (Kind == TEXT("thin_ice")) { ThinIcePatch(S, P); S.Done(Site); return true; }
		if (Kind == TEXT("sastrugi")) { Sastrugi(S, P); S.Done(WRTune(TEXT("places.kitCull"), 40000.f)); return true; }
		if (Kind == TEXT("inuksuk")) { Inuksuk(S, P, 170.f + 40.f * S.R(FVector::ZeroVector, 5), true); S.Done(WRTune(TEXT("places.kitCull"), 40000.f)); return true; }
		return false;
	}
}

bool WRIsleLook::BuildTundra(FWRIsleSite& S, const FString& Kind)
{
	return WRIsleT::BuildImpl(S, Kind);
}
