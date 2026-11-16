// The Volcanic places (cooks/cookpass-world.md 3.3): the landmarks and story sites of the smiths' side, composed from
// the look kit and the packs' rocks and trees at full scale. Each place is in its own frame: X its facing, Z up, the
// origin on the ground at its centre; S.Gz(x, y) is the ground there. Palette (fire-frost 3.3.1): basalt #1E1B19, ash
// #6E6A66, bone #D8D2C4, rust #8A4B2A, glass black #0D0D10, sulfur #D9C230, ember #FF6A1A, blue fire #3A6CFF.
#include "WRIslandPlacesLook.h"
#include "WRIslandPlaces.h"
#include "WRTuning.h"
#include "WRWorldDressing.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Dom/JsonObject.h"

using WRLookKit::FSurf;

namespace WRIsleV
{
	using WRIsleLook::Lin;

	struct FPal
	{
		FSurf Basalt = FSurf::Stone(Lin(30, 27, 25), 0.9f);
		FSurf BasaltAlt = FSurf::Stone(Lin(52, 44, 38), 0.85f);
		FSurf Dressed = FSurf::Stone(Lin(46, 42, 40), 0.72f);
		FSurf Ash = FSurf::Stone(Lin(110, 106, 102), 0.95f);
		FSurf AshDark = FSurf::Stone(Lin(74, 70, 67), 0.95f);
		FSurf Bone = FSurf::Stone(Lin(200, 192, 172), 0.7f);
		FSurf BoneSoot = FSurf::Stone(Lin(132, 124, 110), 0.8f);
		FSurf Rust = FSurf::Steel(Lin(138, 75, 42), 0.8f);
		FSurf Iron = FSurf::Steel(Lin(42, 38, 36), 0.55f);
		FSurf Glass = FSurf::Glass(Lin(13, 13, 16), 0.f);
		FSurf GlassEdge = FSurf::Glaze(Lin(59, 52, 69), 0.12f);
		FSurf Sulfur = FSurf::Glaze(Lin(217, 194, 48), 0.6f);
		FSurf SulfurOrange = FSurf::Glaze(Lin(224, 140, 40), 0.4f);
		FSurf Slag = FSurf::Glaze(Lin(43, 37, 34), 0.35f);
		FSurf Ember = FSurf::Lit(Lin(255, 106, 26), 0.9f);
		FSurf EmberDim = FSurf::Lit(Lin(200, 70, 20), 0.35f);
		FSurf Char = FSurf::Wood(Lin(36, 30, 27));
		FSurf Wood = FSurf::Wood(Lin(110, 90, 68));
		FSurf Canvas = FSurf::Cloth(Lin(168, 154, 128));
		FSurf Rope = FSurf::Cloth(Lin(120, 104, 80));
		FSurf Clay = FSurf::Stone(Lin(138, 90, 58), 0.8f);
		FSurf RimeBlack = FSurf::Glass(Lin(28, 36, 48), 0.f);     // the Rime-folk's blue-black spear stone
		FSurf Moss = FSurf::Cloth(Lin(92, 107, 78));
		FSurf Dark = FSurf::Stone(Lin(8, 7, 7), 0.98f);
		FSurf Teal = FSurf::Glaze(Lin(79, 184, 168), 0.06f);
	};

	/** A lump of welded spatter: a squashed block, tilted, its size and lean from where it sits. */
	void Lump(FWRIsleSite& S, const FSurf& Surf, const FVector& At, float W, float H, int32 Salt)
	{
		S.K.Add(Surf.Dim(0.85f + 0.3f * S.R(At, Salt)), FVector(W * (0.8f + 0.4f * S.R(At, Salt + 1)), W * (0.6f + 0.3f * S.R(At, Salt + 2)), H),
			At, FRotator(12.f * S.J(At, Salt + 3), 360.f * S.R(At, Salt + 4), 12.f * S.J(At, Salt + 5)));
	}

	/** A hornito: rings of spatter narrowing to a glowing throat (H tall, R across the base, in cm), at local Base. */
	void Hornito(FWRIsleSite& S, const FPal& P, const FVector& Base, float H, float R, bool bBig)
	{
		const int32 Rings = FMath::Max(3, FMath::RoundToInt(H / (bBig ? 105.f : 60.f)));
		const float RingH = H / Rings;
		for (int32 i = 0; i < Rings; ++i)
		{
			const float T = float(i) / Rings;
			const float Rad = FMath::Lerp(R, R * 0.36f, FMath::Pow(T, 0.8f));
			// Each ring is off the one below by a hand's width: welded clots, not a lathe.
			const FVector C = Base + FVector(18.f * S.J(Base + FVector(i), 3), 18.f * S.J(Base + FVector(i), 4), RingH * (i + 0.5f));
			const int32 Lumps = bBig ? 9 : 6;
			for (int32 k = 0; k < Lumps; ++k)
			{
				const float A = (360.f / Lumps) * (k + 0.5f * (i % 2)) + 14.f * S.J(C + FVector(k), 5);
				const FVector D = FRotator(0.f, A, 0.f).Vector();
				const FVector At = C + D * Rad * 0.86f;
				S.K.Add((i % 3 == 2 ? P.BasaltAlt : P.Basalt).Dim(0.8f + 0.35f * S.R(At, 6)),
					FVector(Rad * 2.f * PI / Lumps * 1.25f, Rad * 0.42f, RingH * (1.2f + 0.3f * S.R(At, 7))), At,
					FRotator(6.f * S.J(At, 8), A + 90.f, 10.f * S.J(At, 9)));
			}
			// The cone's dark heart (so no gap between the clots shows daylight through it).
			S.K.Add(P.Dark, FVector(Rad * 1.7f, Rad * 1.7f, RingH * 1.05f), C, FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		}
		// The throat: a glowing lip of fresh spatter and the hot core under it.
		const FVector Top = Base + FVector(0.f, 0.f, H);
		S.K.Add(P.Ember, FVector(R * 0.55f, R * 0.55f, 14.f), Top - FVector(0.f, 0.f, 10.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		for (int32 k = 0; k < 7; ++k)
		{
			const FVector D = FRotator(0.f, 51.4f * k, 0.f).Vector();
			S.K.Add(P.EmberDim, FVector(R * 0.2f, 18.f, 22.f), Top + D * R * 0.3f, FRotator(-25.f, 51.4f * k + 90.f, 0.f));
		}
		// The skirt of spatter the cone threw down round its foot.
		for (int32 k = 0; k < (bBig ? 14 : 7); ++k)
		{
			const float A = (bBig ? 25.7f : 51.4f) * k + 9.f * S.J(Base + FVector(k), 11);
			const float Dd = R * (1.05f + 0.45f * S.R(Base + FVector(k), 12));
			const FVector At(Base.X + FMath::Cos(FMath::DegreesToRadians(A)) * Dd, Base.Y + FMath::Sin(FMath::DegreesToRadians(A)) * Dd, 0.f);
			Lump(S, P.Slag, FVector(At.X, At.Y, S.Gz(At.X, At.Y) + 8.f), R * 0.32f, R * 0.12f, 13 + k);
		}
	}

	/** A dry-stone cairn of slag and basalt, H cm tall, an iron ember cup on its cap (lit as you pass at night). */
	void SlagCairn(FWRIsleSite& S, const FPal& P, float H, bool bCup)
	{
		const int32 Courses = 4;
		float Z = S.Gz(0, 0) - 10.f;
		for (int32 c = 0; c < Courses; ++c)
		{
			const float W = FMath::Lerp(H * 0.62f, H * 0.3f, float(c) / Courses);
			const float Ch = H * (c == 0 ? 0.3f : 0.22f);
			const int32 N = c == 0 ? 5 : c == 1 ? 4 : 3;
			for (int32 k = 0; k < N; ++k)
			{
				const float A = 360.f / N * k + 40.f * c;
				const FVector At = FVector(FMath::Cos(FMath::DegreesToRadians(A)), FMath::Sin(FMath::DegreesToRadians(A)), 0.f) * W * 0.32f + FVector(0, 0, Z + Ch * 0.5f);
				S.K.Add((k + c) % 3 == 0 ? P.Slag : P.Basalt, FVector(W * 0.6f, W * 0.48f, Ch), At, FRotator(4.f * S.J(At, 1), A + 30.f * S.J(At, 2), 4.f * S.J(At, 3)));
			}
			Z += Ch * 0.92f;
		}
		// The cap: one glassy slag cake, and the iron cup on three stubby feet.
		S.K.Add(P.Slag, FVector(H * 0.3f, H * 0.28f, H * 0.1f), FVector(0, 0, Z + H * 0.05f), FRotator(0.f, 25.f, 0.f), EWRLookMesh::Cylinder);
		if (bCup)
		{
			const FVector Cup(0, 0, Z + H * 0.1f + 14.f);
			S.K.Add(P.Iron, FVector(34.f, 34.f, 16.f), Cup, FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			for (int32 k = 0; k < 3; ++k) S.K.Rod(P.Iron, Cup + FRotator(0.f, 120.f * k, 0.f).Vector() * 12.f - FVector(0, 0, 8.f), Cup + FRotator(0.f, 120.f * k, 0.f).Vector() * 14.f - FVector(0, 0, 20.f), 4.f);
			S.K.Add(P.EmberDim.Dim(0.5f), FVector(26.f, 26.f, 4.f), Cup + FVector(0, 0, 6.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			S.PassCup(Cup + FVector(0, 0, 30.f), Lin(255, 150, 70), WRTune(TEXT("places.cupLux"), 2400.f), 1200.f);
		}
	}

	/** A charred wagon: bed, sides, four spoked wheels (one off and lying), a broken pole. Local, at At facing Yaw. */
	void Wagon(FWRIsleSite& S, const FPal& P, const FVector& At, float Yaw, bool bTipped, int32 Salt)
	{
		const FTransform Was = S.K.Frame;
		S.K.Frame = FTransform(FRotator(0.f, Yaw, bTipped ? 70.f : 4.f * S.J(At, Salt)), At + FVector(0, 0, bTipped ? 60.f : 70.f));
		S.K.Add(P.Char, FVector(360.f, 170.f, 14.f), FVector(0, 0, 40.f));
		for (const float Sd : { -1.f, 1.f })
		{
			for (int32 k = 0; k < 5; ++k) S.K.Add(P.Char.Dim(0.8f + 0.3f * S.R(At + FVector(k, Sd, 0), Salt + k)), FVector(70.f, 8.f, 50.f - 12.f * (k % 2)), FVector(-140.f + 70.f * k, Sd * 82.f, 72.f));
			for (const float Xw : { -110.f, 110.f })
			{
				const bool bOff = !bTipped && Sd > 0.f && Xw > 0.f;
				if (bOff) continue;
				const FVector W(Xw, Sd * 98.f, 0.f);
				S.K.Disc(P.Char, W, FVector(0, 1, 0), 130.f, 10.f);
				S.K.Add(P.Iron, FVector(24.f, 24.f, 16.f), W + FVector(0, Sd * 8.f, 0), FRotator(0.f, 0.f, 90.f), EWRLookMesh::Cylinder);
				for (int32 sp = 0; sp < 6; ++sp) S.K.Rod(P.Char, W, W + FRotator(30.f * sp, 0.f, 0.f).RotateVector(FVector(0, 0, 60.f)), 6.f);
			}
		}
		S.K.Rod(P.Char, FVector(180.f, 0, 40.f), FVector(330.f, 20.f, -30.f), 12.f);
		S.K.Frame = Was;
		if (!bTipped)
		{
			// The wheel that came off lies flat beside it.
			const FVector Off = At + FRotator(0.f, Yaw, 0.f).RotateVector(FVector(150.f, 230.f, 0.f));
			S.K.Disc(P.Char, FVector(Off.X, Off.Y, S.Gz(Off.X, Off.Y) + 6.f), FVector(0.1f, 0, 1), 130.f, 10.f);
		}
	}

	// ======================================================================================================= landmarks

	void CraterPlume(FWRIsleSite& S)
	{
		// The island's compass: smoke rising off the crater all day and night, and the forge's glow under it after dark.
		S.Fx(FVector(0, 0, S.Gz(0, 0) + 600.f), TEXT("crater.plume"), 1.f);
		S.Lamp(FVector(0, 0, S.Gz(0, 0) + 1800.f), Lin(255, 120, 40), WRTune(TEXT("places.craterGlowLux"), 60000.f), 9000.f);
	}

	void BellowsStacks(FWRIsleSite& S, const FPal& P)
	{
		// Three hornito chimneys in a triangle, each with the smiths' stone vent arch at its foot facing the old road; their
		// throats breathe fire in turn, 6 s apart.
		const float Heights[3] = { 2200.f, 1800.f, 2000.f };
		const float Period = WRTune(TEXT("places.bellowsPeriod"), 18.f);
		for (int32 i = 0; i < 3; ++i)
		{
			const float A = 120.f * i;
			const FVector D = FRotator(0.f, A, 0.f).Vector();
			const FVector Base(D.X * 700.f, D.Y * 700.f, 0.f);
			const FVector Foot(Base.X, Base.Y, S.Gz(Base.X, Base.Y) - 30.f);
			const float R = 300.f - 30.f * i;
			Hornito(S, P, Foot, Heights[i], R, true);
			S.Solid(Foot + FVector(0, 0, Heights[i] * 0.5f), FVector(R * 0.62f, R * 0.62f, Heights[i] * 0.5f));
			// The vent arch on the outward side: two dressed jambs, a lintel and its keystone, a dark mouth behind.
			const FVector Out = D;
			const FVector Arch = Foot + Out * (R * 0.95f);
			const FRotator Face(0.f, A, 0.f);
			for (const float Sd : { -1.f, 1.f })
			{
				S.K.Add(P.Dressed, FVector(70.f, 60.f, 150.f), Arch + Face.RotateVector(FVector(0, Sd * 85.f, 75.f)), Face);
				S.K.Add(P.Dressed.Dim(0.9f), FVector(76.f, 66.f, 26.f), Arch + Face.RotateVector(FVector(0, Sd * 85.f, 163.f)), Face);
			}
			S.K.Add(P.Dressed, FVector(80.f, 250.f, 46.f), Arch + Face.RotateVector(FVector(0, 0, 196.f)), Face);
			S.K.Add(P.Dressed.Dim(1.1f), FVector(84.f, 46.f, 60.f), Arch + Face.RotateVector(FVector(4.f, 0, 222.f)), Face);
			S.K.Add(P.Dark, FVector(40.f, 110.f, 150.f), Arch + Face.RotateVector(FVector(-24.f, 0, 75.f)), Face);
			S.K.Add(P.EmberDim, FVector(10.f, 90.f, 30.f), Arch + Face.RotateVector(FVector(-8.f, 0, 18.f)), Face);
			// Its steam: a fumarole in the arch mouth (the hazard: hiss, then the column).
			FWRIsleHazard& H = S.Hazard(FWRIsleHazard::EKind::Fumarole, Arch + Out * 140.f + FVector(0, 0, 10.f), 200.f);
			H.Rot = Face;
			S.Fx(Arch + Out * 40.f + FVector(0, 0, 60.f), TEXT("ff.vent.wisp"), 0.8f);
			// The breath at the throat, each stack a third of the period after the last.
			S.Breath(Foot + FVector(0, 0, Heights[i] + 120.f), TEXT("ff.bellows"), Period, Period / 3.f * i, 2.4f,
				WRTune(TEXT("places.bellowsLux"), 9000.f), 3500.f, Lin(255, 128, 50));
		}
		// The old road the smiths walked between the arches: worn flags round the triangle.
		for (int32 k = 0; k < 18; ++k)
		{
			const float A = 20.f * k;
			const FVector At(FMath::Cos(FMath::DegreesToRadians(A)) * 980.f, FMath::Sin(FMath::DegreesToRadians(A)) * 980.f, 0.f);
			S.K.Add(P.Dressed.Dim(0.8f), FVector(150.f, 110.f, 12.f), FVector(At.X, At.Y, S.Gz(At.X, At.Y) + 2.f), FRotator(0.f, A + 90.f + 8.f * S.J(At, 1), 0.f));
		}
	}

	void FallenSmith(FWRIsleSite& S, const FPal& P)
	{
		// The forge giant lies along the lava's south bank, face up, half-sunk in clinker; his right arm reaches north and
		// his hammer's haft spans the river (the only dry crossing west of the crater). Frost split him: a blue-black crack
		// through the skull and the Rime-folk's spearheads still in it. Data: bridge.from / bridge.to (metres).
		FVector2D From2 = FVector2D::ZeroVector, To2 = FVector2D::ZeroVector;
		const bool bBridge = WRIsleLook::RowPoint(S, TEXT("bridge"), TEXT("from"), From2) && WRIsleLook::RowPoint(S, TEXT("bridge"), TEXT("to"), To2);
		FVector From = bBridge ? S.Local(FVector(From2, S.World.GetLocation().Z)) : FVector(0, -2600, 0);
		FVector To = bBridge ? S.Local(FVector(To2, S.World.GetLocation().Z)) : FVector(0, -7000, 0);
		From.Z = S.Gz(From.X, From.Y) + 70.f;
		To.Z = S.Gz(To.X, To.Y) + 70.f;
		// The body's axis: 26 m south of the haft's south end, along the bank (local X).
		const float Ay = From.Y + 2600.f;
		const FSurf& B = P.Bone;
		// Skull at the east end, turned up, the jaw dropped.
		const FVector Sk(2600.f, Ay, S.Gz(2600.f, Ay) + 150.f);
		S.K.Add(B, FVector(520.f, 430.f, 380.f), Sk, FRotator(-12.f, 0.f, 8.f));
		S.K.Add(B.Dim(0.92f), FVector(300.f, 380.f, 160.f), Sk + FVector(240.f, 0, -150.f), FRotator(-30.f, 0.f, 0.f));   // the brow and nasal shelf
		S.K.Add(B.Dim(0.88f), FVector(380.f, 340.f, 90.f), Sk + FVector(260.f, 0, -260.f), FRotator(18.f, 0.f, 6.f));     // the dropped jaw
		for (const float Sd : { -1.f, 1.f }) S.K.Add(P.Dark, FVector(60.f, 110.f, 100.f), Sk + FVector(250.f, Sd * 110.f, 40.f), FRotator(-12.f, 0.f, 0.f));
		for (int32 t = 0; t < 8; ++t) S.K.Add(B.Dim(1.05f), FVector(40.f, 34.f, 70.f), Sk + FVector(380.f, -140.f + 40.f * t, -170.f), FRotator(10.f, 0.f, 0.f));
		// The frost crack: a seam of blue-black ice through the crown, and three spearheads driven into it.
		S.K.Add(FSurf::Glass(Lin(70, 120, 160), 0.05f), FVector(40.f, 430.f, 22.f), Sk + FVector(-40.f, 0, 190.f), FRotator(-12.f, 18.f, 8.f));
		for (int32 k = 0; k < 3; ++k)
		{
			const FVector At = Sk + FVector(-80.f + 70.f * k, -90.f + 90.f * k, 170.f);
			S.K.Spike(P.RimeBlack, At, FVector(0.3f * S.J(At, 1), 0.3f * S.J(At, 2), -1.f), 18.f, 70.f);
			S.K.Rod(P.Wood.Dim(0.5f), At, At + FVector(40.f * S.J(At, 3), 40.f * S.J(At, 4), 160.f), 6.f);   // the shaft's burnt stub
		}
		S.Solid(Sk, FVector(260.f, 215.f, 190.f), FRotator(-12.f, 0.f, 8.f));
		// Spine: eighteen vertebrae from the neck to the pelvis, each a body and a dorsal spine, half in the clinker.
		for (int32 v = 0; v < 18; ++v)
		{
			const float X = 2150.f - 250.f * v;
			const FVector At(X, Ay, S.Gz(X, Ay) + 40.f);
			S.K.Add(B.Dim(0.9f + 0.1f * (v % 2)), FVector(180.f, 170.f, 150.f), At, FRotator(0.f, 0.f, 4.f * S.J(At, 5)), EWRLookMesh::Cylinder);
			S.K.Add(B.Dim(0.85f), FVector(60.f, 40.f, 170.f), At + FVector(0, 0, 120.f), FRotator(-10.f, 0.f, 0.f));
			for (const float Sd : { -1.f, 1.f }) S.K.Add(B.Dim(0.85f), FVector(50.f, 110.f, 40.f), At + FVector(0, Sd * 120.f, 30.f), FRotator(0.f, 0.f, Sd * 20.f));
		}
		S.Solid(FVector(0.f, Ay, S.Gz(0, Ay) + 60.f), FVector(2300.f, 110.f, 90.f));
		// Ribs: nine pairs arching up and out from the spine and down into the ground; the middle ones broken by the fall.
		for (int32 r = 0; r < 9; ++r)
		{
			const float X = 1750.f - 240.f * r;
			for (const float Sd : { -1.f, 1.f })
			{
				const bool bBroken = (r >= 3 && r <= 5 && Sd > 0.f) || (r == 4);
				const float Span = 560.f + 40.f * FMath::Sin(r * 0.6f), Rise = 680.f + 90.f * FMath::Sin(r * 0.5f);
				FVector Prev(X, Ay + Sd * 90.f, S.Gz(X, Ay) + 120.f);
				const int32 Segs = bBroken ? 3 : 7;
				for (int32 k = 1; k <= Segs; ++k)
				{
					const float T = k / 7.f;
					const float Yy = Ay + Sd * (90.f + Span * FMath::Sin(T * PI * 0.55f));
					const FVector Pt(X - 60.f * T, Yy, S.Gz(X, Ay) + 120.f + Rise * FMath::Sin(T * PI) - 140.f * T * T);
					S.K.Rod(B.Dim(0.85f + 0.1f * S.R(Pt, 9)), Prev, Pt, FMath::Lerp(60.f, 38.f, T));
					Prev = Pt;
				}
				if (bBroken)
				{
					// The broken end lies on the clinker beside its stub.
					const FVector Lie(X - 120.f, Ay + Sd * 640.f, 0.f);
					S.K.Rod(B.Dim(0.8f), FVector(Lie.X, Lie.Y, S.Gz(Lie.X, Lie.Y) + 25.f), FVector(Lie.X + 260.f, Lie.Y + Sd * 180.f, S.Gz(Lie.X + 260.f, Lie.Y + Sd * 180.f) + 15.f), 44.f);
				}
			}
		}
		// Pelvis and the legs running west into the flow.
		const FVector Pv(-2350.f, Ay, S.Gz(-2350.f, Ay) + 80.f);
		for (const float Sd : { -1.f, 1.f }) S.K.Add(B.Dim(0.9f), FVector(420.f, 260.f, 70.f), Pv + FVector(0, Sd * 220.f, 40.f), FRotator(0.f, Sd * 20.f, Sd * -35.f));
		S.Solid(Pv, FVector(260.f, 380.f, 120.f));
		for (const float Sd : { -1.f, 1.f })
		{
			const FVector Hip = Pv + FVector(-100.f, Sd * 300.f, 0.f);
			const FVector Knee(Hip.X - 1500.f, Hip.Y + Sd * 250.f, S.Gz(Hip.X - 1500.f, Hip.Y + Sd * 250.f) + 30.f);
			const FVector Ankle(Knee.X - 1300.f, Knee.Y - Sd * 100.f, S.Gz(Knee.X - 1300.f, Knee.Y - Sd * 100.f) - 30.f);
			S.K.Rod(B, Hip, Knee, 130.f);
			S.K.Add(B.Dim(0.9f), FVector(180.f, 170.f, 150.f), Knee, FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			S.K.Rod(B.Dim(0.9f), Knee, Ankle, 100.f);
		}
		// The right arm reaches north to the bank: humerus, elbow, forearm, the hand's bones round the haft.
		const FVector Shoulder(1650.f, Ay - 300.f, S.Gz(1650.f, Ay - 300.f) + 140.f);
		const FVector Hand = From + FVector(0, 0, 10.f);
		const FVector Elbow = (Shoulder + Hand) * 0.5f + FVector(500.f, 0.f, 120.f);
		S.K.Rod(B, Shoulder, Elbow, 120.f);
		S.K.Add(B.Dim(0.9f), FVector(170.f, 170.f, 150.f), Elbow, FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		S.K.Rod(B.Dim(0.95f), Elbow, Hand + FVector(0, 260.f, 30.f), 90.f);
		for (int32 f = 0; f < 4; ++f)
		{
			const FVector Kn = Hand + FVector(-120.f + 80.f * f, 120.f, 120.f);
			S.K.Rod(B.Dim(0.9f), Hand + FVector(-90.f + 60.f * f, 220.f, 40.f), Kn, 40.f);
			S.K.Rod(B.Dim(0.85f), Kn, Hand + FVector(-120.f + 80.f * f, -60.f, 90.f), 34.f);
		}
		// The hammer: the haft spanning the river (walkable), iron-banded every 3 m; the head sunk in the far bank.
		const FVector Dir = (To - From).GetSafeNormal();
		const float Len = FVector::Dist(From, To);
		S.K.Rod(P.Wood.Dim(0.45f), From - Dir * 200.f, To, 130.f);
		for (float d = 100.f; d < Len; d += 300.f) S.K.Disc(P.Iron, From + Dir * d, Dir, 146.f, 26.f);
		const FRotator HaftRot = Dir.Rotation();
		S.Solid((From + To) * 0.5f + FVector(0, 0, 5.f), FVector(Len * 0.5f + 150.f, 75.f, 62.f), HaftRot);
		const FVector Head = To + Dir * 160.f - FVector(0, 0, 90.f);
		S.K.Add(P.Iron, FVector(420.f, 360.f, 330.f), Head, FRotator(0.f, HaftRot.Yaw, 10.f));
		S.K.Add(P.Rust, FVector(436.f, 120.f, 340.f), Head + FVector(0, 0, 4.f), FRotator(0.f, HaftRot.Yaw + 90.f, 10.f));
		S.K.Add(P.EmberDim.Dim(0.6f), FVector(10.f, 300.f, 20.f), Head + FVector(0, 0, 120.f), FRotator(0.f, HaftRot.Yaw + 90.f, 10.f));
		S.Solid(Head, FVector(210.f, 180.f, 165.f), FRotator(0.f, HaftRot.Yaw, 10.f));
		// The lava glows up under the haft and through the ribs (lava gives its own light by day and night).
		S.Lamp((From + To) * 0.5f - FVector(0, 0, 60.f), Lin(255, 110, 40), 9000.f, 1800.f, true);
		S.Lamp(FVector(600.f, Ay, S.Gz(600.f, Ay) + 260.f), Lin(255, 120, 50), WRTune(TEXT("places.ribGlowLux"), 4000.f), 1600.f);
		// Clinker heaped against the body where it sank.
		for (int32 k = 0; k < 26; ++k)
		{
			const float X = 2400.f - 200.f * k;
			const float Sd = (k % 2) ? 1.f : -1.f;
			const FVector At(X, Ay + Sd * (260.f + 90.f * S.R(FVector(X, 0, 0), 4)), 0.f);
			S.Rock(k, FVector(220.f, 180.f, 90.f), FVector(At.X, At.Y, S.Gz(At.X, At.Y)), FRotator(0.f, 37.f * k, 6.f), Lin(30, 27, 25) * 0.6f, false, 40.f);
		}
	}

	void Glasswall(FWRIsleSite& S, const FPal& P)
	{
		// A 46 m cliff of obsidian behind the shrine's valley: fifteen black glass slabs leaning out, curved faces, razor rims;
		// a ledge 9 m up holds the Mirror Room's door, reached by riding the geyser at its foot.
		const float Back = -600.f;
		for (int32 i = 0; i < 15; ++i)
		{
			const float Y = -2300.f + 330.f * i + 40.f * S.J(FVector(i, 0, 0), 1);
			const float W = 300.f + 180.f * S.R(FVector(i, 1, 0), 2);
			const float H = 1800.f + 800.f * S.R(FVector(i, 2, 0), 3) * (1.f - FMath::Abs(i - 7.f) / 10.f);
			const float Th = 260.f + 140.f * S.R(FVector(i, 3, 0), 4);
			const float Lean = 4.f + 5.f * S.R(FVector(i, 4, 0), 5);
			const float Gz = S.Gz(Back, Y);
			const FRotator Rot(-Lean, 6.f * S.J(FVector(i, 5, 0), 6), 0.f);
			const FVector C(Back, Y, Gz + H * 0.5f - 60.f);
			S.K.Add(P.Glass, FVector(Th, W, H), C, Rot);
			// The conchoidal face: a second, curved-in slab proud of the first, and its razor rim catching the light.
			S.K.Add(P.Glass.Dim(1.1f), FVector(Th * 0.4f, W * 0.8f, H * 0.6f), C + Rot.RotateVector(FVector(Th * 0.45f, 0.f, H * 0.12f)), Rot + FRotator(-4.f, 10.f * S.J(C, 7), 0.f));
			S.K.Add(P.GlassEdge, FVector(18.f, W * 0.95f, 20.f), C + Rot.RotateVector(FVector(Th * 0.5f, 0.f, H * 0.5f)), Rot);
			S.Solid(C, FVector(Th * 0.5f, W * 0.5f, H * 0.5f), Rot);
			// Fallen blades at the foot.
			if (i % 2 == 0) S.K.Add(P.Glass, FVector(200.f, 60.f, 30.f), FVector(Back + 420.f, Y + 100.f, S.Gz(Back + 420.f, Y + 100.f) + 12.f), FRotator(8.f, 40.f * S.J(C, 8), 20.f));
		}
		// The ledge (walkable) and the Mirror Room's door: a glass arch over a black mouth, an ember cup on the sill.
		const float LedgeZ = S.Gz(Back, 0.f) + 900.f;
		S.K.Add(P.Glass.Dim(1.2f), FVector(360.f, 900.f, 60.f), FVector(Back + 300.f, 0.f, LedgeZ - 30.f));
		S.Solid(FVector(Back + 300.f, 0.f, LedgeZ - 30.f), FVector(180.f, 450.f, 30.f));
		for (const float Sd : { -1.f, 1.f }) S.K.Add(P.GlassEdge, FVector(80.f, 70.f, 330.f), FVector(Back + 170.f, Sd * 170.f, LedgeZ + 165.f));
		S.K.Add(P.GlassEdge, FVector(90.f, 420.f, 70.f), FVector(Back + 170.f, 0.f, LedgeZ + 360.f));
		S.K.Add(P.Dark, FVector(40.f, 270.f, 320.f), FVector(Back + 130.f, 0.f, LedgeZ + 160.f));
		S.Lamp(FVector(Back + 230.f, 0.f, LedgeZ + 40.f), Lin(255, 140, 60), 1800.f, 700.f);
		S.K.Add(P.Iron, FVector(30.f, 30.f, 14.f), FVector(Back + 230.f, 0.f, LedgeZ + 8.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		// The geyser in front of the ledge: its spout leans a little toward the wall so its burst carries you up onto it.
		const FVector Spout(Back + 680.f, 0.f, S.Gz(Back + 680.f, 0.f));
		FWRIsleHazard& G = S.Hazard(FWRIsleHazard::EKind::Geyser, Spout, 150.f);
		G.Rot = FRotator(0.f, S.World.GetRotation().Rotator().Yaw + 180.f, 0.f);
		G.Half = FVector(WRTune(TEXT("hazards.ff.geyser.ledgeLean"), 260.f), 0.f, 0.f);
		for (int32 k = 0; k < 9; ++k) S.K.Add(P.Sulfur.Dim(0.7f), FVector(70.f, 50.f, 12.f), Spout + FRotator(0.f, 40.f * k, 0.f).Vector() * 110.f + FVector(0, 0, 4.f), FRotator(0.f, 40.f * k, 0.f));
		S.K.Add(P.Teal.Dim(0.6f), FVector(140.f, 140.f, 6.f), Spout + FVector(0, 0, 2.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
	}

	void BlueFire(FWRIsleSite& S, const FPal& P)
	{
		// Sulfur vents on a yellow crust; by night their gas burns blue (the only blue in the Volcanic). A clay condensing
		// pipe the miners laid runs from the big vent to a pan where the sulfur drips orange and cools yellow.
		for (int32 k = 0; k < 12; ++k)
		{
			const float A = 30.f * k + 10.f * S.J(FVector(k), 1), D = (k % 3) * 520.f + 200.f;
			const FVector At(FMath::Cos(FMath::DegreesToRadians(A)) * D, FMath::Sin(FMath::DegreesToRadians(A)) * D, 0.f);
			S.K.Add(P.Sulfur.Dim(0.75f + 0.3f * S.R(At, 2)), FVector(900.f, 700.f, 12.f), FVector(At.X, At.Y, S.Gz(At.X, At.Y) + 2.f), FRotator(0.f, A, 0.f), EWRLookMesh::Cylinder);
		}
		const FVector Pits[5] = { FVector(0, 0, 0), FVector(520, 260, 0), FVector(-380, 420, 0), FVector(-120, -560, 0), FVector(600, -380, 0) };
		for (int32 i = 0; i < 5; ++i)
		{
			const FVector C(Pits[i].X, Pits[i].Y, S.Gz(Pits[i].X, Pits[i].Y));
			const float R = i == 0 ? 120.f : 70.f + 20.f * S.R(C, 3);
			for (int32 k = 0; k < 9; ++k)
			{
				const FVector D = FRotator(0.f, 40.f * k, 0.f).Vector();
				Lump(S, k % 2 ? P.Basalt : P.Slag, C + D * R + FVector(0, 0, 18.f), 70.f, 44.f, 20 + k);
				S.K.Add(P.SulfurOrange.Dim(0.8f), FVector(36.f, 30.f, 16.f), C + D * (R + 50.f) + FVector(0, 0, 10.f), FRotator(0.f, 40.f * k, 0.f));
			}
			S.K.Add(P.Dark, FVector(R * 1.7f, R * 1.7f, 20.f), C + FVector(0, 0, 4.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			S.NightFx(C + FVector(0, 0, 20.f), TEXT("ff.blue_fire"), i == 0 ? 1.4f : 0.9f);
			S.Lamp(C + FVector(0, 0, 90.f), Lin(58, 108, 255), WRTune(TEXT("places.blueFireLux"), 3200.f) * (i == 0 ? 1.5f : 1.f), 1100.f);
			S.Fx(C + FVector(0, 0, 30.f), TEXT("ff.vent.wisp"), 0.6f);
		}
		// The condensing pipe: eight clay sections stepping down from the big vent, each socket sealed with clay.
		FVector Prev(110.f, -40.f, S.Gz(110.f, -40.f) + 60.f);
		for (int32 k = 1; k <= 8; ++k)
		{
			const FVector Pt(110.f + 70.f * k, -40.f - 60.f * k, S.Gz(110.f + 70.f * k, -40.f - 60.f * k) + 60.f - 4.f * k);
			S.K.Rod(P.Clay.Dim(0.85f + 0.2f * (k % 2)), Prev, Pt, 50.f);
			S.K.Disc(P.Clay.Dim(0.7f), Pt, (Pt - Prev).GetSafeNormal(), 62.f, 14.f);
			if (k % 2 == 0) S.K.Rod(P.Wood, Pt - FVector(0, 0, 70.f), Pt - FVector(0, 0, 20.f), 10.f);   // its prop
			Prev = Pt;
		}
		const FVector Pan = Prev + FVector(50.f, -40.f, -40.f);
		S.K.Add(P.Iron, FVector(170.f, 170.f, 20.f), FVector(Pan.X, Pan.Y, S.Gz(Pan.X, Pan.Y) + 10.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		S.K.Add(P.Sulfur, FVector(150.f, 150.f, 6.f), FVector(Pan.X, Pan.Y, S.Gz(Pan.X, Pan.Y) + 20.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		S.K.Rod(FSurf::Lit(Lin(255, 150, 40), 0.6f), Prev + FVector(20.f, -15.f, -20.f), FVector(Pan.X, Pan.Y, S.Gz(Pan.X, Pan.Y) + 24.f), 6.f);
	}

	void SlagLight(FWRIsleSite& S, const FPal& P)
	{
		// A smiths' beacon on the west headland: four tapering drums of dressed basalt, a spiral stair of cantilevered
		// steps outside, the top course fallen. Its fire-basket burns once the Kipuka's shrine is kindled.
		const float G0 = S.Gz(0, 0) - 40.f;
		const int32 Courses = 30;
		const float CourseH = 55.f;
		for (int32 c = 0; c < Courses; ++c)
		{
			const float T = float(c) / Courses;
			const float R = FMath::Lerp(270.f, 190.f, T);
			const int32 N = 12;
			for (int32 k = 0; k < N; ++k)
			{
				// The top course has lost three stones to the sea wind; they lie at the foot.
				if (c == Courses - 1 && (k == 5 || k == 6 || k == 10)) continue;   // the stair comes up through the gap at 180
				const float A = 30.f * k + (c % 2) * 15.f;
				const FVector D = FRotator(0.f, A, 0.f).Vector();
				S.K.Add(P.Dressed.Dim(0.85f + 0.3f * S.R(FVector(c, k, 0), 1)), FVector(2.f * PI * R / N * 1.04f, 60.f, CourseH - 3.f),
					FVector(D.X * R, D.Y * R, G0 + CourseH * (c + 0.5f)), FRotator(0.f, A + 90.f, 0.f));
			}
			// Each drum ends with a projecting band.
			if (c % 8 == 7) S.K.Add(P.Dressed.Dim(0.75f), FVector(2.f * R + 50.f, 2.f * R + 50.f, 20.f), FVector(0, 0, G0 + CourseH * (c + 1)), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		}
		const float Top = G0 + CourseH * Courses;
		S.K.Add(P.Dark, FVector(330.f, 330.f, Top - G0 - 40.f), FVector(0, 0, (Top + G0) * 0.5f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		S.Solid(FVector(0, 0, (Top + G0) * 0.5f), FVector(190.f, 190.f, (Top - G0) * 0.5f));
		S.K.Add(P.Dressed, FVector(400.f, 400.f, 30.f), FVector(0, 0, Top - 50.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		S.Solid(FVector(0, 0, Top - 50.f), FVector(190.f, 190.f, 15.f));
		// The stair: 40 steps (41 cm risers) round the outside, a turn and a quarter, each slab bedded in the wall; it comes
		// up through the gap in the fallen top course onto the platform.
		const int32 Steps = 40;
		for (int32 s = 0; s < Steps; ++s)
		{
			const float T = float(s) / (Steps - 1);
			const float A = 90.f + 450.f * T;
			const float Z = G0 + 70.f + (Top - G0 - 120.f) * T;
			const float R = FMath::Lerp(270.f, 190.f, (Z - G0) / (Top - G0));
			const FVector D = FRotator(0.f, A, 0.f).Vector();
			const FVector C(D.X * (R + 60.f), D.Y * (R + 60.f), Z);
			S.K.Add(P.Dressed.Dim(0.95f), FVector(130.f, 70.f, 18.f), C, FRotator(0.f, A, 0.f));
			S.Solid(C, FVector(70.f, 40.f, 10.f), FRotator(0.f, A, 0.f));
		}
		// The fallen stones at the foot.
		for (int32 k = 0; k < 3; ++k)
		{
			const FVector At = FRotator(0.f, 70.f + 50.f * k, 0.f).Vector() * (420.f + 60.f * k);
			S.K.Add(P.Dressed.Dim(0.8f), FVector(140.f, 60.f, 52.f), FVector(At.X, At.Y, S.Gz(At.X, At.Y) + 20.f), FRotator(12.f, 40.f * k, 25.f));
		}
		// The fire-basket: eight iron staves in a ring on a collar, its fire the beacon.
		const FVector Bk(0, 0, Top + 10.f);
		S.K.Add(P.Iron, FVector(160.f, 160.f, 16.f), Bk, FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		for (int32 k = 0; k < 8; ++k)
		{
			const FVector D = FRotator(0.f, 45.f * k, 0.f).Vector();
			S.K.Rod(P.Iron, Bk + D * 70.f, Bk + D * 110.f + FVector(0, 0, 140.f), 8.f);
		}
		S.K.Disc(P.Iron, Bk + FVector(0, 0, 140.f), FVector::UpVector, 230.f, 10.f);
		S.ShrineLight(Bk + FVector(0, 0, 90.f), TEXT("kipuka"), WRTune(TEXT("places.beaconLux"), 24000.f), 4500.f);
	}

	void Kipuka(FWRIsleSite& S, const FPal& P)
	{
		// An island of the old forest the flow went round: living pines and green scrub on moss, glowmoth cocoons in the low
		// branches. Nothing grows inside the lava, and the shrine at its east edge keeps its clearing.
		FVector2D ShrineAt(0.f, 0.f);
		const bool bClearing = WRIsleLook::RowPoint(S, nullptr, TEXT("clearing"), ShrineAt);
		const FVector ShrineL = bClearing ? S.Local(FVector(ShrineAt, 0.f)) : FVector(1e7f);
		TArray<FTransform> Pines[4], Shrubs;
		const TCHAR* PineMesh[4] = { TEXT("/Game/Props/PineA/pine_a_cropped/StaticMeshes/pine_a_cropped.pine_a_cropped"), TEXT("/Game/Props/PineB/pine_b_textured/StaticMeshes/pine_b_textured.pine_b_textured"),
			TEXT("/Game/Props/PineC/pine_c_textured/StaticMeshes/pine_c_textured.pine_c_textured"), TEXT("/Game/Props/FirA/fir_a_textured/StaticMeshes/fir_a_textured.fir_a_textured") };
		int32 Placed = 0, Moths = 0;
		for (int32 i = 0; i < 90 && Placed < 34; ++i)
		{
			// A sunflower spiral: dense in the middle, thinning to the edge.
			const float R = 5600.f * FMath::Sqrt((i + 0.5f) / 90.f);
			const float A = i * 137.508f;
			const FVector L(FMath::Cos(FMath::DegreesToRadians(A)) * R, FMath::Sin(FMath::DegreesToRadians(A)) * R, 0.f);
			if (FVector::Dist2D(L, ShrineL) < 1800.f) continue;
			if (FWRBiomeHazards::KindAt(S.ToWorld(FVector(L.X, L.Y, S.Gz(L.X, L.Y)))) == TEXT("lava")) continue;
			if (S.R(L, 1) < 0.35f) continue;
			const FVector G(L.X, L.Y, S.Gz(L.X, L.Y) - 20.f);
			Pines[Placed % 4].Emplace(FRotator(2.f * S.J(L, 2), 360.f * S.R(L, 3), 2.f * S.J(L, 4)), G, FVector(0.85f + 0.4f * S.R(L, 5)));
			S.Solid(G + FVector(0, 0, 300.f), FVector(30.f, 30.f, 300.f));
			if (Moths < 12 && S.R(L, 6) > 0.5f)
			{
				// A cocoon hung under a low branch and its glowmoth.
				const FVector C = G + FVector(90.f * S.J(L, 7), 90.f * S.J(L, 8), 320.f + 160.f * S.R(L, 9));
				S.K.Add(FSurf::Lit(Lin(214, 240, 190), 0.15f), FVector(16.f, 16.f, 34.f), C, FRotator::ZeroRotator, EWRLookMesh::Cylinder);
				S.K.Rod(P.Rope, C + FVector(0, 0, 16.f), C + FVector(0, 0, 70.f), 2.f);
				S.Glowmoth(C, S.R(L, 10) * 3.f);
				++Moths;
			}
			++Placed;
		}
		for (int32 i = 0; i < 60; ++i)
		{
			const float R = 5200.f * FMath::Sqrt((i + 0.3f) / 60.f);
			const float A = i * 137.508f + 61.f;
			const FVector L(FMath::Cos(FMath::DegreesToRadians(A)) * R, FMath::Sin(FMath::DegreesToRadians(A)) * R, 0.f);
			if (FVector::Dist2D(L, ShrineL) < 1200.f) continue;
			if (FWRBiomeHazards::KindAt(S.ToWorld(FVector(L.X, L.Y, S.Gz(L.X, L.Y)))) == TEXT("lava")) continue;
			Shrubs.Emplace(FRotator(0.f, 360.f * S.R(L, 11), 0.f), FVector(L.X, L.Y, S.Gz(L.X, L.Y) - 5.f), FVector(0.7f + 0.6f * S.R(L, 12)));
			// Moss underfoot where the needles fall.
			S.K.Add(P.Moss.Dim(0.7f + 0.4f * S.R(L, 13)), FVector(420.f, 300.f, 6.f), FVector(L.X, L.Y, S.Gz(L.X, L.Y) + 1.f), FRotator(0.f, A, 0.f), EWRLookMesh::Cylinder);
		}
		for (int32 m = 0; m < 4; ++m) S.Meshes(PineMesh[m], Pines[m], 1300.f, WRTune(TEXT("places.treeCull"), 40000.f));
		S.Meshes(TEXT("/Game/GV_FreeShrubsPack/Meshes/Shrubs/Wind/Shrub_A/GV_Vol7_Shrub_A_full_type1.GV_Vol7_Shrub_A_full_type1"), Shrubs, 240.f, 15000.f);
	}

	void WispSpire(FWRIsleSite& S, const FPal& P)
	{
		// A spire of fused rock behind Ignessa's ring: nine hexagonal basalt drums each turned 7 degrees on the last, three
		// glassy ribs spiralling up it, a split crown; her wisps rise off its top in a slow halo.
		float Z = S.Gz(0, 0) - 40.f;
		for (int32 d = 0; d < 9; ++d)
		{
			const float R = FMath::Lerp(300.f, 80.f, d / 8.f);
			const float H = 270.f;
			S.K.Hex(d % 2 ? P.BasaltAlt : P.Basalt, 0.f, 0.f, Z, Z + H, R, 7.f * d);
			Z += H;
		}
		S.Solid(FVector(0, 0, S.Gz(0, 0) + 1100.f), FVector(200.f, 200.f, 1150.f));
		for (int32 r = 0; r < 3; ++r)
		{
			for (int32 k = 0; k < 20; ++k)
			{
				const float T = k / 20.f;
				const float A = 120.f * r + 400.f * T;
				const float R = FMath::Lerp(320.f, 100.f, T);
				const FVector At(FMath::Cos(FMath::DegreesToRadians(A)) * R, FMath::Sin(FMath::DegreesToRadians(A)) * R, S.Gz(0, 0) + 2300.f * T + 40.f);
				S.K.Add(P.Glass, FVector(46.f, 70.f, 150.f), At, FRotator(0.f, A, 20.f));
			}
		}
		const FVector Crown(0, 0, Z);
		S.K.Spike(P.Basalt, Crown, FVector(0.25f, 0.1f, 1.f), 120.f, 360.f);
		S.K.Spike(P.BasaltAlt, Crown + FVector(-30.f, 20.f, 0), FVector(-0.35f, -0.1f, 1.f), 90.f, 260.f);
		S.Fx(Crown + FVector(0, 0, 200.f), TEXT("ff.wisp_halo"), 1.f);
		S.Lamp(Crown + FVector(0, 0, 250.f), Lin(255, 228, 176), WRTune(TEXT("places.wispLux"), 5000.f), 2200.f);
		// Scorched rings on the ground round its foot.
		for (int32 k = 0; k < 3; ++k) S.K.Ring(P.AshDark.Dim(0.6f), FVector(0, 0, S.Gz(0, 0) + 2.f), 500.f + 260.f * k, 6.f, 40.f, 18, 0.f, 360.f, 30.f);
	}

	// ======================================================================================================= story sites

	void PloughTrench(FWRIsleSite& S, const FPal& P)
	{
		// Cinderhorn's furrow: two ash berms either side of a ploughed strip along his loop's west leg; the giant plough
		// smashed at the north end; the yoke-maker's burnt hut at the south. Data: from / to (metres).
		FVector2D A2, B2;
		if (!WRIsleLook::RowPoint(S, nullptr, TEXT("from"), A2) || !WRIsleLook::RowPoint(S, nullptr, TEXT("to"), B2)) return;
		const FVector A = S.Local(FVector(A2, 0.f)), B = S.Local(FVector(B2, 0.f));
		const FVector Dir = (B - A).GetSafeNormal2D();
		const FVector Side(-Dir.Y, Dir.X, 0.f);
		const float Len = FVector::Dist2D(A, B);
		const float Yaw = Dir.Rotation().Yaw;
		for (float d = 0.f; d < Len; d += 280.f)
		{
			for (const float Sd : { -1.f, 1.f })
			{
				const FVector At = A + Dir * d + Side * Sd * 380.f;
				S.K.Add(P.Ash.Dim(0.9f + 0.15f * S.R(At, 1)), FVector(320.f, 260.f, 110.f + 30.f * S.R(At, 2)), FVector(At.X, At.Y, S.Gz(At.X, At.Y) + 20.f), FRotator(0.f, Yaw, Sd * 14.f), EWRLookMesh::Cylinder);
			}
			// The furrow's floor: turned dark ash in ridges.
			const FVector F = A + Dir * d;
			for (int32 r = -2; r <= 2; ++r) S.K.Add(P.AshDark, FVector(280.f, 40.f, 18.f), FVector(F.X, F.Y, S.Gz(F.X, F.Y) + 4.f) + Side * (r * 90.f), FRotator(0.f, Yaw, 0.f));
		}
		// The plough at the north end: a 7 m iron share driven into the ash, the beam snapped in two, a chain off it.
		const FVector Pl = B + Dir * 300.f;
		const FVector PlG(Pl.X, Pl.Y, S.Gz(Pl.X, Pl.Y));
		S.K.Add(P.Iron, FVector(700.f, 80.f, 260.f), PlG + FVector(0, 0, 60.f), FRotator(-18.f, Yaw, 6.f));
		S.K.Add(P.Rust, FVector(300.f, 90.f, 200.f), PlG + Dir * 260.f + FVector(0, 0, 40.f), FRotator(-35.f, Yaw, 0.f));
		S.K.Rod(P.Char, PlG + FVector(0, 0, 200.f), PlG - Dir * 900.f + FVector(0, 0, 340.f), 70.f);
		S.K.Rod(P.Char, PlG - Dir * 1150.f + Side * 200.f + FVector(0, 0, 30.f), PlG - Dir * 1700.f + Side * 400.f, 66.f);
		for (int32 c = 0; c < 10; ++c) S.K.Disc(P.Iron, PlG - Dir * (60.f * c) + Side * (40.f * FMath::Sin(c * 0.8f)) + FVector(0, 0, 14.f), c % 2 ? Dir : Side, 44.f, 10.f);
		S.Solid(PlG + FVector(0, 0, 120.f), FVector(350.f, 60.f, 130.f), FRotator(0.f, Yaw, 0.f));
		// The yoke-maker's hut at the south end: four charred posts, the fallen roof, a rack of half-shaped yokes.
		const FVector Hu = A - Dir * 600.f + Side * 300.f;
		const FVector HuG(Hu.X, Hu.Y, S.Gz(Hu.X, Hu.Y));
		for (int32 k = 0; k < 4; ++k)
		{
			const FVector C = HuG + FVector((k % 2) ? 220.f : -220.f, (k / 2) ? 180.f : -180.f, 0.f);
			S.K.Rod(P.Char, C - FVector(0, 0, 40.f), C + FVector(0, 0, k == 3 ? 120.f : 230.f), 22.f);
		}
		S.K.Add(P.Char, FVector(520.f, 440.f, 16.f), HuG + FVector(40.f, 0, 90.f), FRotator(14.f, 8.f, -6.f));
		for (int32 y = 0; y < 3; ++y)
		{
			const FVector Y0 = HuG + FVector(-320.f, -120.f + 120.f * y, 70.f);
			S.K.Add(P.Wood.Dim(0.5f), FVector(40.f, 220.f, 30.f), Y0 + FVector(0, 0, 40.f));
			S.K.Add(P.Wood.Dim(0.6f), FVector(26.f, 26.f, 90.f), Y0 + FVector(0, -80.f, 0), FRotator(0.f, 0.f, 15.f), EWRLookMesh::Cylinder);
			S.K.Add(P.Wood.Dim(0.6f), FVector(26.f, 26.f, 90.f), Y0 + FVector(0, 80.f, 0), FRotator(0.f, 0.f, -15.f), EWRLookMesh::Cylinder);
		}
	}

	void MinersGrave(FWRIsleSite& S, const FPal& P)
	{
		// Twelve miners died running for the shore when the vents blew: baskets on shoulder poles lying in a line toward
		// the coast (local +X), each body an ash mound beside its load; one pole still stands upright in the ash.
		for (int32 i = 0; i < 12; ++i)
		{
			const FVector L(-900.f + 160.f * i + 60.f * S.J(FVector(i), 1), 220.f * S.J(FVector(i), 2), 0.f);
			const FVector G(L.X, L.Y, S.Gz(L.X, L.Y));
			const float Yaw = 6.f * S.J(L, 3);
			// The body under ash, lying forward toward the shore.
			S.K.Add(P.Ash.Dim(0.85f), FVector(180.f, 60.f, 40.f), G + FVector(0, 0, 12.f), FRotator(0.f, Yaw, 0.f), EWRLookMesh::Cylinder);
			S.K.Add(P.Ash.Dim(0.9f), FVector(46.f, 46.f, 34.f), G + FRotator(0.f, Yaw, 0.f).RotateVector(FVector(110.f, 0, 14.f)), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			// The pole across the shoulders and its two baskets spilled beside.
			const FVector Pole = G + FRotator(0.f, Yaw, 0.f).RotateVector(FVector(40.f, 0, 20.f));
			if (i == 6) S.K.Rod(P.Wood, G + FVector(0, 80.f, -30.f), G + FVector(10.f, 90.f, 190.f), 7.f);
			else S.K.Rod(P.Wood, Pole + FRotator(0.f, Yaw + 90.f, 0.f).Vector() * 110.f, Pole - FRotator(0.f, Yaw + 90.f, 0.f).Vector() * 110.f, 7.f);
			for (const float Sd : { -1.f, 1.f })
			{
				const FVector Bk = Pole + FRotator(0.f, Yaw + 90.f, 0.f).Vector() * Sd * 130.f;
				S.K.Add(P.Rope.Dim(0.8f), FVector(60.f, 60.f, 50.f), FVector(Bk.X, Bk.Y, S.Gz(Bk.X, Bk.Y) + 20.f), FRotator(70.f * Sd, Yaw, 0.f), EWRLookMesh::Cylinder);
				S.K.Add(P.Sulfur, FVector(40.f, 34.f, 16.f), FVector(Bk.X, Bk.Y, S.Gz(Bk.X, Bk.Y)) + FRotator(0.f, Yaw + 90.f, 0.f).Vector() * Sd * 50.f + FVector(0, 0, 6.f), FRotator(0.f, 30.f * i, 0.f));
			}
		}
	}

	void MinersCamp(FWRIsleSite& S, const FPal& P)
	{
		// The camp they came from: two canvas lean-tos facing the cookfire (its fire is still kept by someone), a pot on a
		// tripod, the tally board of baskets carried, empty baskets stacked.
		for (const float Sd : { -1.f, 1.f })
		{
			const FVector T0(-200.f, Sd * 320.f, 0.f);
			const FVector G(T0.X, T0.Y, S.Gz(T0.X, T0.Y));
			const FRotator Face(0.f, Sd > 0.f ? -90.f : 90.f, 0.f);
			for (const float Ex : { -170.f, 170.f })
			{
				const FVector Post = G + Face.RotateVector(FVector(70.f, Ex, 0));
				S.K.Rod(P.Wood, Post - FVector(0, 0, 30.f), Post + FVector(0, 0, 170.f), 9.f);
			}
			S.K.Rod(P.Wood, G + Face.RotateVector(FVector(70.f, -190.f, 170.f)), G + Face.RotateVector(FVector(70.f, 190.f, 170.f)), 8.f);
			S.K.Add(P.Canvas, FVector(240.f, 380.f, 4.f), G + Face.RotateVector(FVector(-30.f, 0, 90.f)), Face + FRotator(-40.f, 0.f, 0.f));
			S.K.Add(P.Canvas.Dim(0.7f), FVector(200.f, 90.f, 10.f), G + Face.RotateVector(FVector(-20.f, 0, 6.f)), Face);
		}
		const FVector Fire(150.f, 0.f, S.Gz(150.f, 0.f));
		for (int32 k = 0; k < 9; ++k) S.K.Add(P.Basalt, FVector(34.f, 28.f, 22.f), Fire + FRotator(0.f, 40.f * k, 0.f).Vector() * 70.f + FVector(0, 0, 8.f), FRotator(0.f, 40.f * k, 0.f));
		for (int32 k = 0; k < 3; ++k) S.K.Rod(P.Wood, Fire + FRotator(0.f, 120.f * k, 0.f).Vector() * 90.f, Fire + FVector(0, 0, 150.f), 5.f);
		S.K.Add(P.Iron, FVector(46.f, 46.f, 36.f), Fire + FVector(0, 0, 70.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		S.Fx(Fire + FVector(0, 0, 10.f), TEXT("brazier.fire"), 0.6f);
		S.Lamp(Fire + FVector(0, 0, 60.f), Lin(255, 150, 70), 3500.f, 1200.f);
		// The tally board: a plank on two stakes with thirty notches burned in.
		const FVector Tb(-50.f, -520.f, S.Gz(-50.f, -520.f));
		for (const float Sd : { -1.f, 1.f }) S.K.Rod(P.Wood, Tb + FVector(Sd * 90.f, 0, -30.f), Tb + FVector(Sd * 90.f, 0, 140.f), 8.f);
		S.K.Add(P.Wood.Dim(1.1f), FVector(220.f, 8.f, 46.f), Tb + FVector(0, 0, 115.f));
		for (int32 n = 0; n < 30; ++n) S.K.Add(P.Char, FVector(3.f, 10.f, 26.f), Tb + FVector(-95.f + 6.5f * n, -4.f, 115.f), FRotator(0.f, 0.f, n % 5 == 4 ? 40.f : 0.f));
		for (int32 b = 0; b < 6; ++b) S.K.Add(P.Rope.Dim(0.85f), FVector(64.f, 64.f, 52.f), FVector(-350.f + (b % 3) * 70.f, 0.f + (b / 3) * 70.f, S.Gz(-350.f, 0.f) + 26.f + (b >= 4 ? 52.f : 0.f)), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
	}

	void CharredCaravan(FWRIsleSite& S, const FPal& P)
	{
		// Three smith wagons burnt where they stood; the guards' blades lie pointing east (local +X), toward the ridge and
		// the pass, the way the attack came.
		Wagon(S, P, S.G(-200.f, -260.f), 100.f, false, 1);
		Wagon(S, P, S.G(300.f, 220.f), 70.f, true, 2);
		Wagon(S, P, S.G(-650.f, 300.f), 125.f, false, 3);
		for (int32 i = 0; i < 7; ++i)
		{
			const FVector L(150.f + 120.f * i, -500.f + 160.f * i + 60.f * S.J(FVector(i), 1), 0.f);
			const FVector G(L.X, L.Y, S.Gz(L.X, L.Y) + 4.f);
			const float Yaw = 12.f * S.J(L, 2);
			if (i % 2 == 0) { S.K.Add(P.Iron, FVector(110.f, 10.f, 4.f), G, FRotator(0.f, Yaw, 0.f)); S.K.Add(P.Iron, FVector(8.f, 30.f, 6.f), G - FRotator(0.f, Yaw, 0.f).Vector() * 50.f, FRotator(0.f, Yaw, 0.f)); }
			else { S.K.Rod(P.Char, G - FRotator(0.f, Yaw, 0.f).Vector() * 110.f, G + FRotator(0.f, Yaw, 0.f).Vector() * 90.f, 5.f); S.K.Spike(P.Iron, G + FRotator(0.f, Yaw, 0.f).Vector() * 90.f, FRotator(0.f, Yaw, 0.f).Vector(), 9.f, 30.f); }
			S.K.Add(P.Ash.Dim(0.85f), FVector(150.f, 55.f, 30.f), G + FRotator(0.f, Yaw + 90.f, 0.f).Vector() * 70.f, FRotator(0.f, Yaw, 0.f), EWRLookMesh::Cylinder);
		}
	}

	void TreeMolds(FWRIsleSite& S, const FPal& P)
	{
		// Where the flow ran through living forest: trunk-shaped holes with chilled rims, one holding a miner's bones.
		for (int32 i = 0; i < 18; ++i)
		{
			const float A = i * 137.5f, R = 2200.f * FMath::Sqrt((i + 0.5f) / 18.f);
			const FVector L(FMath::Cos(FMath::DegreesToRadians(A)) * R, FMath::Sin(FMath::DegreesToRadians(A)) * R, 0.f);
			const FVector G(L.X, L.Y, S.Gz(L.X, L.Y));
			const float D = 40.f + 50.f * S.R(L, 1);
			S.K.Ring(P.Basalt.Dim(0.8f), G + FVector(0, 0, 8.f), D * 0.5f, 22.f, 30.f, 10, 0.f, 360.f, 2.f);
			S.K.Add(P.Basalt.Dim(0.7f), FVector(D * 2.f + 40.f, D * 2.f + 40.f, 18.f), G + FVector(0, 0, 4.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			S.K.Add(P.Dark, FVector(D, D, 8.f), G + FVector(0, 0, 14.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			if (i == 11)
			{
				S.K.Add(P.Bone, FVector(30.f, 26.f, 26.f), G + FVector(10.f, 0, 22.f));
				S.K.Add(P.Rope.Dim(0.6f), FVector(40.f, 32.f, 24.f), G + FVector(-14.f, 12.f, 24.f), FRotator(0.f, 30.f, 20.f));
			}
		}
	}

	void CraterStair(FWRIsleSite& S, const FPal& P)
	{
		// Smith-cut steps up the crater wall toward its rim (local +X), in three flights, broken in two places; two headless
		// smith statues flank the foot.
		float X = 0.f;
		for (int32 s = 0; s < 36; ++s)
		{
			if (s == 13 || s == 14 || s == 25) { X += 130.f; continue; }   // the breaks you jump
			const float Z = S.Gz(X, 0.f);
			S.K.Add(P.Dressed.Dim(0.85f + 0.2f * S.R(FVector(s), 1)), FVector(120.f, 300.f, 40.f), FVector(X, 0.f, Z + 4.f), FRotator(0.f, 0.f, 2.f * S.J(FVector(s), 2)));
			S.Solid(FVector(X, 0.f, Z + 4.f), FVector(60.f, 150.f, 20.f));
			X += 130.f;
		}
		for (const float Sd : { -1.f, 1.f })
		{
			const FVector B(-150.f, Sd * 280.f, S.Gz(-150.f, Sd * 280.f));
			S.K.Add(P.Dressed, FVector(120.f, 120.f, 60.f), B + FVector(0, 0, 30.f));
			S.K.Add(P.Basalt, FVector(80.f, 110.f, 170.f), B + FVector(0, 0, 145.f));                                     // the body
			S.K.Add(P.Basalt, FVector(60.f, 160.f, 40.f), B + FVector(0, 0, 225.f));                                      // the shoulders
			S.K.Rod(P.Basalt, B + FVector(10.f, 0, 120.f), B + FVector(60.f, 0, 230.f), 30.f);                           // the arm holding the hammer
			S.K.Add(P.Iron, FVector(60.f, 34.f, 34.f), B + FVector(60.f, 0, 250.f));
			S.K.Add(P.Basalt.Dim(0.9f), FVector(40.f, 40.f, 30.f), B + FVector(90.f, Sd * 60.f, 15.f), FRotator(20.f, 0.f, 30.f));  // the struck-off head
			S.Solid(B + FVector(0, 0, 120.f), FVector(60.f, 60.f, 120.f));
		}
	}

	void WardensQuarry(FWRIsleSite& S, const FPal& P)
	{
		// Where the Warden was cut: half-cut obsidian blocks with chisel grooves, a second golem left half-carved on its back,
		// a stone sledge on log rollers. It stands outside the Warden's ring, facing it.
		for (int32 i = 0; i < 5; ++i)
		{
			const FVector G = S.G(-200.f - 180.f * (i % 3), -500.f + 260.f * i);
			S.K.Add(P.Glass, FVector(160.f, 140.f, 120.f + 40.f * (i % 2)), G + FVector(0, 0, 60.f), FRotator(0.f, 15.f * S.J(G, 1), 0.f));
			for (int32 g = 0; g < 3; ++g) S.K.Add(P.GlassEdge, FVector(164.f, 6.f, 4.f), G + FVector(0, -40.f + 40.f * g, 125.f + 40.f * (i % 2)), FRotator(0.f, 15.f * S.J(G, 1), 0.f));
		}
		const FVector Gl = S.G(-500.f, 300.f);
		S.K.Add(P.Glass, FVector(220.f, 150.f, 90.f), Gl + FVector(0, 0, 50.f));                    // the torso, face up
		S.K.Add(P.Glass, FVector(110.f, 100.f, 80.f), Gl + FVector(170.f, 0, 50.f));                // head
		for (const float Sd : { -1.f, 1.f }) S.K.Rod(P.Glass.Dim(1.1f), Gl + FVector(60.f, Sd * 90.f, 50.f), Gl + FVector(-60.f, Sd * 200.f, 30.f), 60.f);
		S.K.Add(P.Basalt, FVector(420.f, 160.f, 70.f), Gl + FVector(-120.f, 0, 10.f));              // the uncut block it lies on
		S.Solid(Gl + FVector(0, 0, 50.f), FVector(260.f, 110.f, 70.f));
		const FVector Sl = S.G(-300.f, -900.f);
		S.K.Add(P.Wood.Dim(0.7f), FVector(300.f, 140.f, 20.f), Sl + FVector(0, 0, 40.f));
		for (int32 r = 0; r < 3; ++r) S.K.Rod(P.Wood.Dim(0.6f), Sl + FVector(-100.f + 100.f * r, -90.f, 14.f), Sl + FVector(-100.f + 100.f * r, 90.f, 14.f), 26.f);
		S.K.Add(P.Glass, FVector(150.f, 120.f, 100.f), Sl + FVector(0, 0, 100.f));
	}

	void ShardField(FWRIsleSite& S, const FPal& P)
	{
		// Obsidian blades set point-up in the ground over 80 m, with two clear paths through (the shrine's road and the
		// shore's).
		for (int32 i = 0; i < 64; ++i)
		{
			const float A = i * 137.5f, R = 4000.f * FMath::Sqrt((i + 0.5f) / 64.f);
			const FVector L(FMath::Cos(FMath::DegreesToRadians(A)) * R, FMath::Sin(FMath::DegreesToRadians(A)) * R, 0.f);
			if (FMath::Abs(L.Y - 0.25f * L.X) < 220.f || FMath::Abs(L.X + 0.4f * L.Y) < 200.f) continue;
			const FVector G(L.X, L.Y, S.Gz(L.X, L.Y));
			const float H = 80.f + 160.f * S.R(L, 1);
			const FVector Dir(0.3f * S.J(L, 2), 0.3f * S.J(L, 3), 1.f);
			S.K.Add(P.Glass, FVector(14.f + 10.f * S.R(L, 4), 70.f + 60.f * S.R(L, 5), H), G + Dir.GetSafeNormal() * H * 0.4f, FRotationMatrix::MakeFromZ(Dir).Rotator() + FRotator(0.f, 180.f * S.R(L, 6), 0.f));
		}
	}

	void RidgeWatch(FWRIsleSite& S, const FPal& P)
	{
		// The smiths' watchtower over the north pass road (local +X faces the pass): a square stone tower two floors high,
		// arrow slits toward the pass, a ladder inside up through a hatch to the roof.
		const float G0 = S.Gz(0, 0) - 30.f;
		const float H = 900.f, W = 400.f, T = 60.f;
		for (int32 c = 0; c < 15; ++c)
		{
			const float Z = G0 + 60.f * c + 30.f;
			for (int32 side = 0; side < 4; ++side)
			{
				const FRotator R(0.f, 90.f * side, 0.f);
				// An arrow slit on the pass side at both floors (a gap in the course).
				const bool bSlit = side == 0 && (c == 5 || c == 6 || c == 11 || c == 12);
				if (bSlit)
				{
					S.K.Add(P.Dressed.Dim(0.9f), FVector(T, W * 0.45f, 58.f), FVector(0, 0, Z) + R.RotateVector(FVector(W * 0.5f - T * 0.5f, -W * 0.275f, 0)), R);
					S.K.Add(P.Dressed.Dim(0.9f), FVector(T, W * 0.45f, 58.f), FVector(0, 0, Z) + R.RotateVector(FVector(W * 0.5f - T * 0.5f, W * 0.275f, 0)), R);
					continue;
				}
				// The door on the far side at ground level: the course breaks round a 1.6 m opening.
				if (side == 2 && c < 4)
				{
					for (const float Sd : { -1.f, 1.f }) S.K.Add(P.Dressed.Dim(0.9f), FVector(T, 120.f, 58.f), FVector(0, 0, Z) + R.RotateVector(FVector(W * 0.5f - T * 0.5f, Sd * 140.f, 0)), R);
					continue;
				}
				S.K.Add(P.Dressed.Dim(0.85f + 0.25f * S.R(FVector(c, side, 0), 1)), FVector(T, W, 58.f), FVector(0, 0, Z) + R.RotateVector(FVector(W * 0.5f - T * 0.5f, 0, 0)), R);
			}
		}
		for (int32 side = 0; side < 4; ++side)
		{
			const FRotator R(0.f, 90.f * side, 0.f);
			if (side != 2)
			{
				S.Solid(FVector(0, 0, G0 + H * 0.5f) + R.RotateVector(FVector(W * 0.5f - T * 0.5f, 0.f, 0.f)), FVector(T * 0.5f, W * 0.5f, H * 0.5f), R);
				continue;
			}
			// The door wall: the two jambs up to the lintel, and the wall above it.
			for (const float Sd : { -1.f, 1.f }) S.Solid(FVector(0, 0, G0 + 120.f) + R.RotateVector(FVector(W * 0.5f - T * 0.5f, Sd * 140.f, 0.f)), FVector(T * 0.5f, 60.f, 120.f), R);
			S.Solid(FVector(0, 0, G0 + 240.f + (H - 240.f) * 0.5f) + R.RotateVector(FVector(W * 0.5f - T * 0.5f, 0.f, 0.f)), FVector(T * 0.5f, W * 0.5f, (H - 240.f) * 0.5f), R);
		}
		// The floors and the ladder.
		for (const float Fz : { 450.f, H })
		{
			S.K.Add(P.Wood.Dim(0.7f), FVector(W - 2.f * T, W - 2.f * T, 16.f), FVector(-40.f, 0, G0 + Fz));
			S.Solid(FVector(-40.f, 0, G0 + Fz), FVector(W * 0.5f - T - 40.f, W * 0.5f - T, 8.f));
		}
		for (const float Sd : { -1.f, 1.f }) S.K.Rod(P.Wood, FVector(110.f, Sd * 30.f, G0), FVector(110.f, Sd * 30.f, G0 + H + 40.f), 7.f);
		for (int32 r = 0; r < 24; ++r) S.K.Rod(P.Wood, FVector(110.f, -30.f, G0 + 40.f * r + 30.f), FVector(110.f, 30.f, G0 + 40.f * r + 30.f), 5.f);
		// The roof's battlement (alternate merlons) and a brazier the watch kept.
		for (int32 k = 0; k < 8; ++k)
		{
			const FRotator R(0.f, 90.f * (k / 2), 0.f);
			S.K.Add(P.Dressed, FVector(T, 90.f, 70.f), FVector(0, 0, G0 + H + 35.f) + R.RotateVector(FVector(W * 0.5f - T * 0.5f, (k % 2) ? 120.f : -120.f, 0)), R);
		}
		S.K.Add(P.Iron, FVector(70.f, 70.f, 40.f), FVector(-60.f, 0, G0 + H + 28.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		S.Lamp(FVector(-60.f, 0, G0 + H + 90.f), Lin(255, 150, 70), 4000.f, 1600.f);
		S.NightFx(FVector(-60.f, 0, G0 + H + 50.f), TEXT("brazier.fire"), 0.8f);
	}

	void KilnmiteHill(FWRIsleSite& S, const FPal& P)
	{
		// A 6 m mound of ore the Kilnmites have chewed and spat into cemented galleries; four egg-chamber mouths crusted green
		// with copper.
		for (int32 t = 0; t < 5; ++t) S.K.Add(P.Rust.Dim(0.5f + 0.1f * t), FVector(1400.f - 240.f * t, 1300.f - 230.f * t, 130.f), FVector(0, 0, S.Gz(0, 0) + 40.f + 120.f * t), FRotator(0.f, 25.f * t, 0.f), EWRLookMesh::Cylinder);
		S.Solid(FVector(0, 0, S.Gz(0, 0) + 250.f), FVector(450.f, 450.f, 250.f));
		for (int32 m = 0; m < 4; ++m)
		{
			const FVector D = FRotator(0.f, 90.f * m + 20.f, 0.f).Vector();
			const FVector M = D * (620.f - 60.f * (m % 2)) + FVector(0, 0, S.Gz(0, 0) + 120.f + 90.f * (m % 2));
			S.K.Ring(FSurf::Glaze(Lin(78, 125, 106), 0.5f), M, 70.f, 30.f, 40.f, 10, 0.f, 360.f, 4.f);
			S.K.Add(P.Dark, FVector(30.f, 120.f, 120.f), M, FRotator(0.f, 90.f * m + 20.f, 0.f));
		}
		for (int32 k = 0; k < 14; ++k) Lump(S, P.Rust.Dim(0.6f), S.G(FMath::Cos(k * 0.9f) * (850.f + 80.f * (k % 3)), FMath::Sin(k * 0.9f) * (850.f + 80.f * (k % 3)), 10.f), 60.f, 30.f, 40 + k);
	}

	void AcidPool(FWRIsleSite& S, const FPal& P)
	{
		// A milky teal pool in a hollow, ringed with yellow crust; dead birds at the rim; a rusted cart rolled in and
		// half-eaten. Wading it burns and eats your gear.
		const float R = 900.f;
		float Low = 1e9f;
		for (int32 k = 0; k < 12; ++k) Low = FMath::Min(Low, S.Gz(FMath::Cos(k * 0.52f) * R * 0.6f, FMath::Sin(k * 0.52f) * R * 0.6f));
		Low = FMath::Min(Low, S.Gz(0, 0));
		const FVector C(0, 0, Low + 25.f);
		S.K.Add(P.Teal, FVector(R * 2.f, R * 2.f, 10.f), C, FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		for (int32 k = 0; k < 22; ++k)
		{
			const FVector D = FRotator(0.f, 16.36f * k, 0.f).Vector();
			const FVector At = D * (R + 40.f * S.J(D, 1));
			S.K.Add(P.Sulfur.Dim(0.7f + 0.3f * S.R(At, 2)), FVector(200.f, 120.f, 16.f), FVector(At.X, At.Y, FMath::Max(S.Gz(At.X, At.Y), C.Z - 6.f) + 4.f), FRotator(0.f, 16.36f * k + 90.f, 0.f), EWRLookMesh::Cylinder);
		}
		for (int32 b = 0; b < 5; ++b)
		{
			const FVector At = FRotator(0.f, 70.f * b + 15.f, 0.f).Vector() * (R + 140.f);
			const FVector G(At.X, At.Y, S.Gz(At.X, At.Y) + 6.f);
			S.K.Add(P.AshDark, FVector(26.f, 14.f, 10.f), G, FRotator(0.f, 40.f * b, 0.f));
			S.K.Add(P.AshDark, FVector(30.f, 4.f, 2.f), G + FVector(0, 10.f, 2.f), FRotator(0.f, 40.f * b + 30.f, 0.f));
			S.K.Add(P.AshDark, FVector(30.f, 4.f, 2.f), G + FVector(0, -10.f, 2.f), FRotator(0.f, 40.f * b - 30.f, 0.f));
		}
		Wagon(S, P, FVector(R * 0.55f, -R * 0.2f, C.Z - 40.f), 200.f, false, 7);
		S.Fx(C + FVector(0, 0, 10.f), TEXT("ff.acid"), R / 900.f);
		FWRIsleHazard& H = S.Hazard(FWRIsleHazard::EKind::Acid, C, R);
		H.Half = FVector(R, R, 10.f);
	}

	void SteamGround(FWRIsleSite& S, const FPal& P, bool bTundra)
	{
		// Bare warm ground breathing steam through vents; on the tundra side of the south pass it's the one place the snow
		// can't lie (Fire's heat leaking under the ridge).
		const FSurf Earth = bTundra ? FSurf::Stone(Lin(70, 58, 48), 0.95f) : P.AshDark;
		for (int32 k = 0; k < 10; ++k)
		{
			const FVector At = FRotator(0.f, 36.f * k, 0.f).Vector() * (300.f + 500.f * S.R(FVector(k), 1));
			S.K.Add(Earth.Dim(0.85f + 0.25f * S.R(At, 2)), FVector(800.f, 600.f, 10.f), FVector(At.X, At.Y, S.Gz(At.X, At.Y) + 2.f), FRotator(0.f, 36.f * k, 0.f), EWRLookMesh::Cylinder);
		}
		for (int32 v = 0; v < 6; ++v)
		{
			const FVector D = FRotator(0.f, 60.f * v + 20.f * S.J(FVector(v), 3), 0.f).Vector() * (200.f + 160.f * (v % 3));
			const FVector G(D.X, D.Y, S.Gz(D.X, D.Y));
			for (int32 k = 0; k < 6; ++k) Lump(S, P.Sulfur.Dim(bTundra ? 0.5f : 0.8f), G + FRotator(0.f, 60.f * k, 0.f).Vector() * 45.f + FVector(0, 0, 10.f), 40.f, 22.f, 60 + k);
			S.K.Add(P.Dark, FVector(50.f, 50.f, 6.f), G + FVector(0, 0, 4.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			S.Fx(G + FVector(0, 0, 20.f), TEXT("ff.vent.wisp"), 0.7f);
			if (v % 2 == 0) S.Hazard(FWRIsleHazard::EKind::Fumarole, G + FVector(0, 0, 4.f), 200.f);
		}
	}
}

namespace WRIsleV
{
	/** The dispatch, inside this file's namespace so its names win over any other file's in a unity build. */
	bool BuildImpl(FWRIsleSite& S, const FString& Kind)
	{
		const FPal P;
		const float Landmark = WRTune(TEXT("places.landmarkCull"), 500000.f), Site = WRTune(TEXT("places.siteCull"), 60000.f);
		if (Kind == TEXT("crater_plume")) { CraterPlume(S); return true; }
		if (Kind == TEXT("bellows_stacks")) { BellowsStacks(S, P); S.Done(Landmark); return true; }
		if (Kind == TEXT("fallen_smith")) { FallenSmith(S, P); S.Done(Landmark); return true; }
		if (Kind == TEXT("glasswall")) { Glasswall(S, P); S.Done(Landmark); return true; }
		if (Kind == TEXT("blue_fire")) { BlueFire(S, P); S.Done(Site); return true; }
		if (Kind == TEXT("slag_light")) { SlagLight(S, P); S.Done(Landmark); return true; }
		if (Kind == TEXT("kipuka")) { Kipuka(S, P); S.Done(Site); return true; }
		if (Kind == TEXT("wisp_spire")) { WispSpire(S, P); S.Done(Landmark); return true; }
		if (Kind == TEXT("plough_trench")) { PloughTrench(S, P); S.Done(Site); return true; }
		if (Kind == TEXT("miners_grave")) { MinersGrave(S, P); S.Done(Site); return true; }
		if (Kind == TEXT("miners_camp")) { MinersCamp(S, P); S.Done(Site); return true; }
		if (Kind == TEXT("charred_caravan")) { CharredCaravan(S, P); S.Done(Site); return true; }
		if (Kind == TEXT("tree_molds")) { TreeMolds(S, P); S.Done(Site); return true; }
		if (Kind == TEXT("crater_stair")) { CraterStair(S, P); S.Done(Site); return true; }
		if (Kind == TEXT("wardens_quarry")) { WardensQuarry(S, P); S.Done(Site); return true; }
		if (Kind == TEXT("shard_field")) { ShardField(S, P); S.Done(Site); return true; }
		if (Kind == TEXT("ridge_watch")) { RidgeWatch(S, P); S.Done(Site); return true; }
		if (Kind == TEXT("kilnmite_hill")) { KilnmiteHill(S, P); S.Done(Site); return true; }
		if (Kind == TEXT("acid_pool")) { AcidPool(S, P); S.Done(Site); return true; }
		if (Kind == TEXT("steam_ground")) { SteamGround(S, P, S.World.GetLocation().X > 0.f); S.Done(Site); return true; }
		if (Kind == TEXT("slag_cairn")) { SlagCairn(S, P, 170.f + 30.f * S.R(FVector::ZeroVector, 3), true); S.Done(WRTune(TEXT("places.kitCull"), 40000.f)); return true; }
		return false;
	}
}

bool WRIsleLook::BuildVolcanic(FWRIsleSite& S, const FString& Kind)
{
	return WRIsleV::BuildImpl(S, Kind);
}

namespace WRIsleLook
{
	/** The kit's hornito and cairn reuse the Volcanic's (WRIslandPlacesKit.cpp). */
	void VolcanicHornito(FWRIsleSite& S, const FVector& Base, float H, float R)
	{
		const WRIsleV::FPal P;
		WRIsleV::Hornito(S, P, Base, H, R, false);
	}
	void VolcanicWagon(FWRIsleSite& S, const FVector& At, float Yaw, bool bTipped, int32 Salt)
	{
		const WRIsleV::FPal P;
		WRIsleV::Wagon(S, P, At, Yaw, bTipped, Salt);
	}
}
