// The Graveyard's hand sites (GY 2.4 H1-H13) and kit POIs (GY 2.4 kit table; the crypt mouths and holes that are the
// catacombs' doors): each tells its part of the story with no words (GY 2.20). Built on the forest's FBuild with the
// Graveyard kit (WRGraveLook.h). Local space: the site's ground point, +X its facing.

#include "WRGraveWorld.h"
#include "WRGraveLook.h"
#include "WRGraveTypes.h"
#include "WRLand.h"
#include "Components/BoxComponent.h"

using namespace WRGraveLook;

namespace WRGraveSitesImpl
{
	/** A small stone chapel with a pitched slate roof (H1, intact; the shrine dais stands in its yard): walls, buttresses,
	 *  a west bellcote, a door, lancets, candles burning inside. Front (door) at +X. */
	void SmallChapel(FBuild& B, const FVector& Base, float Yaw, float Len, float Wd, float H, bool bRoof)
	{
		const FRotator R(0.f, Yaw, 0.f);
		const auto P = [&](float X, float Y, float Z) { return Base + R.RotateVector(FVector(X, Y, Z)); };
		const float T = 70.f;
		for (const float S : { -1.f, 1.f })
		{
			for (int32 b = 0; b < 3; ++b)
			{
				const float X0 = -Len * 0.5f + b * Len / 3.f, X1 = X0 + Len / 3.f, Xm = (X0 + X1) * 0.5f;
				B.K.Add(GreyStone(0.9f), FVector(Len / 3.f * 0.36f, T, H), P(X0 + Len / 3.f * 0.18f, S * (Wd * 0.5f), H * 0.5f), R);
				B.K.Add(GreyStone(0.9f), FVector(Len / 3.f * 0.36f, T, H), P(X1 - Len / 3.f * 0.18f, S * (Wd * 0.5f), H * 0.5f), R);
				B.K.Add(GreyStone(0.9f), FVector(Len / 3.f * 0.3f, T, 140.f), P(Xm, S * (Wd * 0.5f), 70.f), R);
				B.K.Add(GreyStone(0.9f), FVector(Len / 3.f * 0.3f, T, H - 380.f), P(Xm, S * (Wd * 0.5f), 380.f + (H - 380.f) * 0.5f), R);
				B.K.Add(Candle(1.2f), FVector(Len / 3.f * 0.26f, 6.f, 200.f), P(Xm, S * (Wd * 0.5f - 20.f), 260.f), R);
				B.K.Add(GreyStone(0.85f), FVector(60.f, 80.f, H * 0.8f), P(X1, S * (Wd * 0.5f + 60.f), H * 0.4f), R);
			}
			B.Box(P(0.f, S * Wd * 0.5f, H * 0.5f), FVector(Len * 0.5f, T * 0.5f, H * 0.5f), R);
		}
		// The gable ends: the front with its pointed door, the back solid; the gables stepped up under the roof.
		for (const float E : { -1.f, 1.f })
		{
			const float X = E * Len * 0.5f;
			if (E > 0.f)
			{
				for (const float S : { -1.f, 1.f }) B.K.Add(GreyStone(0.9f), FVector(T, Wd * 0.5f - 80.f, H), P(X, S * (Wd * 0.25f + 40.f), H * 0.5f), R);
				B.K.Add(GreyStone(0.9f), FVector(T, 160.f, H - 300.f), P(X, 0.f, 300.f + (H - 300.f) * 0.5f), R);
				PointedArch(B, P(X, 0.f, 0.f), Yaw, 160.f, 170.f, 18.f, T + 6.f, Limestone(1.f), 4);
				// The door, open a hand's width: candlelight on the floor inside.
				B.K.Add(Timber(0.7f), FVector(8.f, 150.f, 260.f), P(X - 30.f, 30.f, 130.f), FRotator(0.f, Yaw + 25.f, 0.f));
				for (const float S : { -1.f, 1.f }) B.Box(P(X, S * (Wd * 0.25f + 40.f), H * 0.5f), FVector(T * 0.5f, Wd * 0.25f - 40.f, H * 0.5f), R);
			}
			else
			{
				B.K.Add(GreyStone(0.9f), FVector(T, Wd + T, H), P(X, 0.f, H * 0.5f), R);
				B.Box(P(X, 0.f, H * 0.5f), FVector(T * 0.5f, Wd * 0.5f + T * 0.5f, H * 0.5f), R);
			}
			for (int32 s = 0; s < 6; ++s) B.K.Add(GreyStone(0.9f), FVector(T, Wd + T - s * (Wd / 6.f), 60.f), P(X, 0.f, H + 30.f + s * 60.f), R);
		}
		if (bRoof)
		{
			// Stone slates in courses on the two pitches (45 degrees), a ridge of capping stones.
			const float Half = (Wd + 2.f * T) * 0.5f, Pitch = 45.f, Slope = Half / FMath::Cos(FMath::DegreesToRadians(Pitch));
			for (const float S : { -1.f, 1.f })
			{
				for (int32 c = 0; c < 7; ++c)
				{
					const float D = (c + 0.5f) * Slope / 7.f;
					const FVector Pt = P(0.f, S * (Half - D * FMath::Cos(FMath::DegreesToRadians(Pitch))), H + D * FMath::Sin(FMath::DegreesToRadians(Pitch)));
					B.K.Add(Slate(0.85f + 0.1f * (c % 2)), FVector(Len + 80.f, Slope / 7.f + 8.f, 6.f), Pt, R + FRotator(0.f, 0.f, -S * Pitch));
				}
			}
			B.K.Add(GreyStone(0.95f), FVector(Len + 90.f, 30.f, 24.f), P(0.f, 0.f, H + Half + 6.f), R);
			// The bellcote on the west gable: a small arched opening with a little bell.
			B.K.Add(GreyStone(0.9f), FVector(T, 120.f, 220.f), P(-Len * 0.5f, 0.f, H + Half + 100.f), R);
			B.K.Add(Black(0.5f), FVector(T + 4.f, 60.f, 90.f), P(-Len * 0.5f, 0.f, H + Half + 110.f), R);
			B.K.Add(Bronze(1.f), FVector(36.f, 36.f, 40.f), P(-Len * 0.5f, 0.f, H + Half + 100.f), R, EWRLookMesh::Cone);
			B.Box(P(0.f, 0.f, H + Half * 0.5f), FVector(Len * 0.5f + 40.f, Half, Half * 0.5f), R);
		}
		// Inside: swept flags, a candle stand by the altar, the altar cloth (someone keeps it).
		B.K.Add(Limestone(0.8f), FVector(Len - T, Wd - T, 6.f), P(0.f, 0.f, 3.f), R);
		B.K.Add(GreyStone(0.9f), FVector(80.f, 180.f, 100.f), P(-Len * 0.5f + 140.f, 0.f, 50.f), R);
		B.K.Add(Linen(1.f), FVector(84.f, 190.f, 4.f), P(-Len * 0.5f + 140.f, 0.f, 102.f), R);
		for (int32 c = 0; c < 7; ++c)
		{
			const FVector Cp = P(-Len * 0.5f + 230.f, -150.f + c * 50.f, 120.f + 15.f * (c % 2));
			B.K.Add(Wax(1.f), FVector(6.f, 6.f, 22.f), Cp, R, EWRLookMesh::Cylinder);
			B.K.Add(Candle(6.f), FVector(3.f, 3.f, 6.f), Cp + FVector(0, 0, 14.f), R, EWRLookMesh::Cone);
		}
		B.Lamp(P(-Len * 0.5f + 250.f, 0.f, 200.f), CandleC(), 1400.f, 1100.f);
	}

	/** A roofed timber lychgate where the coffin rested (H1): two pairs of oak posts on stone pads, tie beams, a braced
	 *  frame, a stone-slate roof, the coffin stone (a low slab) under it. The path runs through along +X. */
	void Lychgate(FBuild& B, const FVector& Base, float Yaw)
	{
		const FRotator R(0.f, Yaw, 0.f);
		const auto P = [&](float X, float Y, float Z) { return Base + R.RotateVector(FVector(X, Y, Z)); };
		for (const float X : { -140.f, 140.f }) for (const float Y : { -150.f, 150.f })
		{
			B.K.Add(GreyStone(0.9f), FVector(50.f, 50.f, 30.f), P(X, Y, 15.f), R);
			B.K.Add(Timber(0.7f), FVector(24.f, 24.f, 260.f), P(X, Y, 160.f), R);
			B.Box(P(X, Y, 160.f), FVector(14.f, 14.f, 145.f), R);
		}
		for (const float Y : { -150.f, 150.f }) B.K.Add(Timber(0.7f), FVector(340.f, 26.f, 26.f), P(0.f, Y, 300.f), R);
		for (const float X : { -140.f, 140.f })
		{
			B.K.Add(Timber(0.7f), FVector(26.f, 360.f, 24.f), P(X, 0.f, 312.f), R);
			for (const float Y : { -1.f, 1.f }) B.K.Rod(Timber(0.65f), P(X, Y * 150.f, 250.f), P(X, Y * 90.f, 300.f), 10.f, true);
			// The king post and the collar under the ridge.
			B.K.Add(Timber(0.7f), FVector(18.f, 18.f, 120.f), P(X, 0.f, 380.f), R);
		}
		for (const float S : { -1.f, 1.f })
		{
			for (int32 c = 0; c < 5; ++c)
			{
				const float D = (c + 0.5f) * 240.f / 5.f;
				B.K.Add(Slate(0.8f + 0.1f * (c % 2)), FVector(420.f, 54.f, 5.f), P(0.f, S * (200.f - D * 0.71f), 310.f + D * 0.71f), R + FRotator(0.f, 0.f, -S * 45.f));
			}
		}
		B.K.Add(GreyStone(0.9f), FVector(430.f, 26.f, 20.f), P(0.f, 0.f, 450.f), R);
		B.K.Add(GreyStone(0.8f), FVector(200.f, 70.f, 50.f), P(0.f, 70.f, 25.f), R);
		B.Box(P(0.f, 70.f, 25.f), FVector(100.f, 35.f, 25.f), R);
	}

	/** A dug-out grave: the soil heaved up and out from below, the stone pushed over, a hole going down (H3, the Rising). */
	void DugFromBelow(FBuild& B, const FVector& Base, float Yaw, int32 Salt)
	{
		const FRotator R(0.f, Yaw, 0.f);
		B.K.Add(Black(0.35f), FVector(120.f, 70.f, 4.f), Base + R.RotateVector(FVector(80.f, 0.f, 1.f)), R);
		for (int32 k = 0; k < 7; ++k)
		{
			const float A = k * 2.f * PI / 7.f;
			B.K.Add(Soil(0.8f + 0.2f * B.Rand(Salt + k)), FVector(70.f, 50.f, 26.f), Base + R.RotateVector(FVector(80.f + FMath::Cos(A) * 90.f, FMath::Sin(A) * 60.f, 12.f)), R + FRotator(-20.f * FMath::Cos(A), FMath::RadiansToDegrees(A), 20.f * FMath::Sin(A)));
		}
		Headstone(B, Base + R.RotateVector(FVector(-30.f, 30.f, 0.f)), Yaw + 20.f, 0, 60.f, 0.f, true, false, false, Salt);
	}
}

namespace WRGraveSites
{
	using namespace WRGraveSitesImpl;

	// ---------------------------------------------------------------- H1 The Lychgate Chapel
	void LychgateChapel(FBuild& B)
	{
		// A small intact chapel beside camp 2, its roof sound, candles burning, the floor swept (GY H1): someone still keeps
		// it. The Great Shrine of the Lantern (its dais placed by the shrine rows) stands in its yard; Morwen keeps the
		// lantern rack along its south wall. The yard wall has the lychgate on the road side (west, toward the Coast Road).
		const float Z = GZ(B, 0.f, 0.f);
		SmallChapel(B, FVector(0.f, 0.f, Z), 0.f, 1200.f, 700.f, 520.f, true);
		Lychgate(B, GAt(B, -1700.f, 0.f), 0.f);
		const TArray<FVector2D> Yard = { FVector2D(-1700.f, 250.f), FVector2D(-1700.f, 1250.f), FVector2D(1300.f, 1250.f), FVector2D(1300.f, -1250.f), FVector2D(-1700.f, -1250.f), FVector2D(-1700.f, -250.f) };
		for (int32 i = 0; i + 1 < Yard.Num(); ++i) DryWall(B, Yard[i], Yard[i + 1], 110.f, 11 + i);
		// The lantern rack: a timber frame along the chapel's south wall, three shelves of lanterns, a trimming bench.
		for (int32 p = 0; p < 4; ++p) B.K.Add(Timber(0.7f), FVector(14.f, 14.f, 220.f), GAt(B, -500.f + p * 330.f, 620.f, 110.f));
		for (int32 s = 0; s < 3; ++s)
		{
			B.K.Add(Timber(0.65f), FVector(1000.f, 40.f, 5.f), GAt(B, 0.f, 620.f, 60.f + s * 60.f));
			for (int32 l = 0; l < 18; ++l)
			{
				const FVector L = GAt(B, -470.f + l * 55.f, 620.f, 78.f + s * 60.f);
				B.K.Add(Paper(0.9f), FVector(18.f, 18.f, 24.f), L, FRotator::ZeroRotator, EWRLookMesh::Cylinder);
				if ((l + s) % 3 != 0) B.N.Add(GraveLight(3.f), FVector(19.f, 19.f, 24.f), L, FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			}
		}
		B.K.Add(Timber(0.7f), FVector(200.f, 50.f, 60.f), GAt(B, 700.f, 760.f, 30.f));
		B.K.Add(Wax(1.f), FVector(30.f, 20.f, 8.f), GAt(B, 690.f, 760.f, 64.f));
		B.Lamp(GAt(B, 0.f, 700.f, 180.f), GraveLightC(), 900.f, 900.f);
		B.Box(GAt(B, 0.f, 620.f, 110.f), FVector(500.f, 25.f, 110.f));
	}

	// ---------------------------------------------------------------- H2 Row of the Named
	void RowOfNamed(FBuild& B)
	{
		// 120 headstones in perfect rows, each with a lantern on top and a name (GY H2): the Mourners' own, kept upright and
		// level. Dig any: no body, a lantern with a paper name tag inside (the cenotaph reveal). One is full (S2). Five dark
		// tōrō stand along the north side (Morwen's first quest lights them at night: the spots carry the flame).
		for (int32 r = 0; r < 10; ++r)
		{
			for (int32 c = 0; c < 12; ++c)
			{
				const float X = -1350.f + r * 300.f, Y = -1000.f + c * 180.f;
				const FVector Base = GAt(B, X, Y);
				Headstone(B, Base, 0.f, 2, 105.f, 0.f, false, true, true, r * 37 + c);
				// The name: a pale carved line on the pillar's face.
				B.K.Add(Paper(1.2f), FVector(2.f, 22.f, 6.f), Base + FVector(14.f, 0.f, 40.f));
			}
		}
		for (int32 i = 0; i < 5; ++i) Toro(B, GAt(B, -2400.f + 1200.f * i, -1150.f), 90.f, 1.f, false, 0.f, true);
	}

	// ---------------------------------------------------------------- H3 The Dog Graves
	void DogGraves(FBuild& B)
	{
		// Small graves beside human ones, a collar hung on each stone (GY H3; church grim, Xolotl): the dogs were buried
		// with their people. Two are dug out from below, the soil pushed up, not down: the Gravehounds rise from the
		// ground (a den lies here: the creature track's).
		for (int32 r = 0; r < 4; ++r)
		{
			for (int32 c = 0; c < 6; ++c)
			{
				const float X = -500.f + r * 300.f, Y = -550.f + c * 220.f;
				const FVector Base = GAt(B, X, Y);
				Headstone(B, Base, 0.f, c % 3 == 0 ? 1 : 0, 90.f, 2.f * B.Rand(r * 7 + c), false, false, true, r * 19 + c);
				// The dog's grave beside, small, with its own little stone and a collar on it.
				const FVector Dog = GAt(B, X + 20.f, Y + 100.f);
				if ((r * 6 + c) == 7 || (r * 6 + c) == 16) { DugFromBelow(B, Dog, 0.f, r * 31 + c); continue; }
				Headstone(B, Dog, 0.f, 0, 42.f, 0.f, false, false, false, r * 23 + c + 400);
				B.K.Add(Soil(0.85f), FVector(90.f, 40.f, 8.f), Dog + FVector(50.f, 0.f, 2.f));
				B.Limb(Dog + FVector(6.f, -14.f, 30.f), Dog + FVector(6.f, 14.f, 30.f), 1.4f, 1.4f, Rgb(80, 50, 30) * 0.5f, 5);
				B.Limb(Dog + FVector(6.f, -14.f, 30.f), Dog + FVector(8.f, 0.f, 12.f), 1.4f, 1.4f, Rgb(80, 50, 30) * 0.5f, 5);
				B.Limb(Dog + FVector(6.f, 14.f, 30.f), Dog + FVector(8.f, 0.f, 12.f), 1.4f, 1.4f, Rgb(80, 50, 30) * 0.5f, 5);
				B.Ball(Dog + FVector(9.f, 0.f, 11.f), 2.f, Rgb(160, 120, 50) * 0.6f, 0.4f);
			}
		}
	}

	// ---------------------------------------------------------------- H4 The Ford of Washing
	void FordOfWashing(FBuild& B)
	{
		// A chalk-stream ford where grave-clothes lie on the stones, half washed, a wooden washing bat left beside them (GY
		// H4; the washer at the ford): she was washing the clothes of the next to die, and stopped. Stepping stones cross.
		for (int32 s = -3; s <= 3; ++s)
		{
			const FVector P = GAt(B, s * 90.f, 20.f * FMath::Sin(s * 1.3f), -6.f);
			B.K.Add(GreyStone(0.8f + 0.1f * (s % 2)), FVector(80.f, 70.f, 40.f), P, FRotator(0.f, 20.f * s, 0.f));
			B.Box(P + FVector(0, 0, 4.f), FVector(38.f, 34.f, 20.f), FRotator(0.f, 20.f * s, 0.f));
		}
		// The washing stones on the bank, the cloth draped over them, folds trailing into the water.
		for (int32 k = 0; k < 3; ++k)
		{
			const FVector P = GAt(B, -150.f + k * 110.f, 380.f);
			B.K.Add(Limestone(0.85f), FVector(120.f, 80.f, 34.f), P + FVector(0, 0, 10.f), FRotator(0.f, 15.f * k, 0.f));
			B.K.Add(Linen(1.f), FVector(110.f, 90.f, 3.f), P + FVector(0, 0, 29.f), FRotator(4.f, 15.f * k + 5.f, 6.f));
			B.K.Add(Linen(0.95f), FVector(60.f, 80.f, 3.f), P + FVector(50.f, -40.f, 10.f), FRotator(-40.f, 15.f * k, 0.f));
		}
		// The bat (the keepsake spot sits on it).
		B.K.Add(OldWood(0.9f), FVector(46.f, 12.f, 3.f), GAt(B, 300.f, 380.f, 30.f), FRotator(0.f, 30.f, 0.f));
		B.K.Rod(OldWood(0.9f), GAt(B, 277.f, 367.f, 30.f), GAt(B, 252.f, 352.f, 30.f), 4.f);
		B.K.Add(Sackcloth(0.9f), FVector(70.f, 50.f, 30.f), GAt(B, 160.f, 520.f, 15.f));
	}

	// ---------------------------------------------------------------- H5 Smith's Row
	void SmithsRow(FBuild& B)
	{
		// Ash-graves on the Volcanic border (GY H5): slag headstones in three rows, a smith's hammer laid on each grave, an
		// anvil set up as a marker on the last (the iron in it still good: the cache). The Mourners buried other peoples'
		// dead too. Ash lies in drifts against the stones' north faces.
		for (int32 r = 0; r < 3; ++r)
		{
			for (int32 c = 0; c < 6; ++c)
			{
				const FVector Base = GAt(B, -600.f + r * 300.f, -700.f + c * 260.f);
				Headstone(B, Base, 0.f, 0, 80.f + 30.f * B.Rand(r * 11 + c), 3.f * B.Rand(c), false, false, true, r * 13 + c, true);
				B.K.Rod(OldWood(0.75f), Base + FVector(80.f, -12.f, 12.f), Base + FVector(140.f, 16.f, 12.f), 4.f);
				B.K.Add(Iron(), FVector(10.f, 22.f, 9.f), Base + FVector(144.f, 18.f, 14.f), FRotator(0.f, 25.f, 0.f));
				B.K.Add(FSurf::Cloth(Rgb(90, 86, 82) * 0.45f), FVector(30.f, 90.f, 14.f), Base + FVector(0.f, -60.f, 4.f), FRotator(0.f, 0.f, 10.f));
			}
		}
		// The anvil on its oak block.
		const FVector Av = GAt(B, 260.f, -340.f);
		B.K.Add(Timber(0.7f), FVector(70.f, 70.f, 60.f), Av + FVector(0, 0, 30.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		B.K.Add(Iron(), FVector(70.f, 28.f, 22.f), Av + FVector(0, 0, 72.f));
		B.K.Add(Iron(), FVector(40.f, 22.f, 18.f), Av + FVector(0, 0, 90.f));
		B.K.Spike(Iron(), Av + FVector(20.f, 0, 92.f), FVector::ForwardVector, 18.f, 44.f);
		B.Box(Av + FVector(0, 0, 50.f), FVector(38.f, 38.f, 50.f));
	}

	// ---------------------------------------------------------------- H6 The Seven Names
	void SevenNames(FBuild& B, AWRGraveScenery* A)
	{
		// A great mausoleum with seven name plates on its bronze door, six filled in, one blank, the chisel left on the step
		// beside it (GY H6; S4: bring the chisel to the plate and carve, and the door opens on the seventh crypt). The
		// G2 lesser shrine stands before it.
		const FVector Base = GAt(B, 0.f, 0.f);
		Mausoleum(B, Base, 0.f, 900.f, 1200.f, 700.f, 1, 61);
		// The plates: seven bronze plates in a column beside the door; six with names, the last blank.
		for (int32 p = 0; p < 7; ++p)
		{
			const FVector Pl = Base + FVector(612.f, 260.f, 140.f + p * 60.f);
			B.K.Add(Bronze(1.f), FVector(4.f, 110.f, 40.f), Pl);
			if (p < 6) for (int32 l = 0; l < 3; ++l) B.K.Add(Verdigris(0.8f), FVector(5.f, 70.f - l * 12.f, 4.f), Pl + FVector(1.f, 0.f, -10.f + l * 10.f));
		}
		// The chisel on the step (its keepsake spot).
		B.K.Rod(Iron(), Base + FVector(660.f, 180.f, 22.f), Base + FVector(676.f, 205.f, 22.f), 2.f);
		// The seventh crypt: the bronze leaves shut, or swung open on a dark stair down (S4, the state's flag).
		A->StateFlag = TEXT("seventh");
		A->StateOff = NewObject<USceneComponent>(A);
		A->StateOff->SetupAttachment(A->GetRootComponent());
		A->StateOff->RegisterComponent();
		A->StateOn = NewObject<USceneComponent>(A);
		A->StateOn->SetupAttachment(A->GetRootComponent());
		A->StateOn->RegisterComponent();
		if (!B.bServer)
		{
			TArray<FWRLookBlock> Off, On;
			WRLookKit::FKit KOff(B.T, Off), KOn(B.T, On);
			KOff.Add(Verdigris(1.f), FVector(10.f, 200.f, 300.f), Base + FVector(604.f, 0.f, 165.f));
			for (const float S : { -1.f, 1.f })
			{
				KOn.Add(Verdigris(1.f), FVector(10.f, 100.f, 300.f), Base + FVector(640.f, S * 140.f, 165.f), FRotator(0.f, S * 70.f, 0.f));
				KOn.Add(Black(0.4f), FVector(8.f, 190.f, 290.f), Base + FVector(596.f, 0.f, 160.f));
			}
			FWRBuildLook::Spawn(A, A->StateOff, Off, FLinearColor(0.25f, 0.2f, 0.15f), false);
			FWRBuildLook::Spawn(A, A->StateOn, On, FLinearColor(0.25f, 0.2f, 0.15f), false);
		}
		A->StateBox = B.Box(Base + FVector(604.f, 0.f, 165.f), FVector(10.f, 100.f, 150.f));
	}

	// ---------------------------------------------------------------- H7 The Broken Procession
	void BrokenProcession(FBuild& B)
	{
		// A funeral cart on the road with its coffin open and empty, the pall-bearers' lantern-poles dropped in a line
		// pointing east toward the Lantern Stair (GY H7): they set the dead down and walked to the Stair.
		const FVector C = GAt(B, 0.f, 0.f, 70.f);
		B.K.Add(OldWood(0.75f), FVector(260.f, 110.f, 10.f), C);
		for (const float S : { -1.f, 1.f })
		{
			B.K.Add(OldWood(0.7f), FVector(260.f, 6.f, 30.f), C + FVector(0.f, S * 55.f, 15.f));
			B.K.Disc(OldWood(0.65f), C + FVector(-30.f, S * 70.f, -30.f), FVector(0.f, 1.f, 0.f), 120.f, 10.f);
			for (int32 sp = 0; sp < 6; ++sp) B.K.Rod(OldWood(0.6f), C + FVector(-30.f, S * 70.f, -30.f), C + FVector(-30.f + FMath::Cos(sp * PI / 3.f) * 55.f, S * 70.f, -30.f + FMath::Sin(sp * PI / 3.f) * 55.f), 4.f);
			B.K.Rod(OldWood(0.7f), C + FVector(130.f, S * 40.f, 0.f), C + FVector(290.f, S * 30.f, -60.f), 6.f);
		}
		Coffin(B, C + FVector(0.f, 0.f, 30.f), FRotator(0.f, 0.f, 0.f), true, false, OldWood(0.8f));
		B.K.Add(Linen(0.9f), FVector(120.f, 70.f, 3.f), C + FVector(-60.f, 60.f, 12.f), FRotator(-30.f, 10.f, 20.f));
		B.Box(C, FVector(150.f, 70.f, 60.f));
		// Eight poles, each dropped where its bearer let go, all pointing east.
		for (int32 i = 0; i < 8; ++i)
		{
			const float X = 300.f + i * 160.f, Y = (i % 2 ? -1.f : 1.f) * (120.f + 30.f * B.Rand(i));
			const FVector F0 = GAt(B, X, Y, 4.f), F1 = GAt(B, X + 260.f, Y + 20.f * (B.Rand(i + 9) - 0.5f), 6.f);
			B.K.Rod(OldWood(0.75f), F0, F1, 5.f);
			B.K.Add(Paper(0.6f), FVector(24.f, 24.f, 26.f), F1 + FVector(20.f, 0.f, 6.f), FRotator(90.f, 0.f, 10.f * i), EWRLookMesh::Cylinder);
		}
	}

	// ---------------------------------------------------------------- H8 The Watch House
	void WatchHouse(FBuild& B)
	{
		// A three-storey stone watch tower over the bone pits (GY H8; the mortsafes and watch-houses of the resurrection men),
		// its door barred from inside, the watchers' bones at the windows, a tally of nights scratched by the door that
		// stops at 412. Iron mortsafes stacked in the yard. By day the Gravedigger sleeps against the door.
		const float Z = GZ(B, 0.f, 0.f);
		const float Half = 320.f, H = 1100.f, T = 70.f;
		for (int32 Face = 0; Face < 4; ++Face)
		{
			const float Ang = 90.f * Face;
			const FRotator R(0.f, Ang, 0.f);
			const FVector N = R.Vector(), Sd = FRotator(0.f, Ang + 90.f, 0.f).Vector();
			const FVector Mid = N * (Half - T * 0.5f) + FVector(0, 0, Z);
			for (int32 St = 0; St < 3; ++St)
			{
				const float Za = St * 360.f;
				// Each storey: wall with a small window in the middle (a door below on the front).
				const bool bDoor = Face == 0 && St == 0;
				const float Wh = bDoor ? 230.f : 90.f, Wy = bDoor ? 120.f : 60.f, Wz = bDoor ? 0.f : 150.f;
				B.K.Add(GreyStone(0.86f), FVector(T, Half - Wy, 360.f), Mid + Sd * (Wy + (Half - Wy) * 0.5f) + FVector(0, 0, Za + 180.f), R);
				B.K.Add(GreyStone(0.86f), FVector(T, Half - Wy, 360.f), Mid - Sd * (Wy + (Half - Wy) * 0.5f) + FVector(0, 0, Za + 180.f), R);
				if (Wz > 0.f) B.K.Add(GreyStone(0.86f), FVector(T, Wy * 2.f, Wz), Mid + FVector(0, 0, Za + Wz * 0.5f), R);
				B.K.Add(GreyStone(0.86f), FVector(T, Wy * 2.f, 360.f - Wz - Wh), Mid + FVector(0, 0, Za + Wz + Wh + (360.f - Wz - Wh) * 0.5f), R);
				if (!bDoor)
				{
					B.K.Add(Black(0.5f), FVector(T - 10.f, Wy * 2.f, Wh), Mid + FVector(-6.f, 0, Za + Wz + Wh * 0.5f), R);
					// The watchers at the windows: a skull on the sill looking out, a hand's bones on the ledge.
					if ((Face + St) % 2 == 0) { Skull(B, Mid + N * 10.f + FVector(0, 0, Za + Wz + 16.f), 1.f, Ang, BoneC()); LongBone(B, Mid + N * 20.f + Sd * 20.f + FVector(0, 0, Za + Wz + 4.f), Mid + N * 20.f + Sd * 40.f + FVector(0, 0, Za + Wz + 4.f), 1.4f, BoneC()); }
				}
				B.K.Add(GreyStone(1.05f), FVector(T + 16.f, Half * 2.f + 16.f, 16.f), Mid + FVector(0, 0, Za + 360.f), R);
			}
			// The parapet and the roof's lip.
			for (int32 m = -2; m <= 2; ++m) B.K.Add(GreyStone(0.9f), FVector(T, 70.f, 80.f), Mid + Sd * m * 130.f + FVector(0, 0, H + 40.f), R);
			B.Box(Mid + FVector(0, 0, H * 0.5f), FVector(T * 0.5f, Half, H * 0.5f), R);
		}
		B.K.Add(Slate(0.8f), FVector(Half * 2.f, Half * 2.f, 20.f), FVector(0, 0, Z + H));
		// The door, barred from inside: a heavy oak door shut; the bar's ends show through the frame.
		B.K.Add(Timber(0.6f), FVector(14.f, 220.f, 230.f), FVector(Half - 20.f, 0.f, Z + 115.f));
		for (const float Zb : { 60.f, 170.f }) B.K.Add(Iron(), FVector(16.f, 230.f, 8.f), FVector(Half - 12.f, 0.f, Z + Zb));
		// The tally: 412 marks scratched in rows on the stone by the door, the last only half a line.
		for (int32 row = 0; row < 8; ++row)
		{
			for (int32 m = 0; m < (row < 7 ? 52 : 48); ++m)
			{
				const float Hh = (row == 7 && m == 47) ? 4.f : 9.f;
				B.K.Add(Black(0.6f), FVector(1.f, 0.8f, Hh), FVector(Half + 1.f, 150.f + m * 3.2f, Z + 130.f + row * 13.f + (9.f - Hh) * 0.5f), FRotator(0.f, 0.f, (m % 5 == 4) ? 60.f : 0.f));
			}
		}
		// The yard: a wall round it, the mortsafes stacked against it.
		const TArray<FVector2D> Yard = { FVector2D(1400.f, 300.f), FVector2D(1400.f, 1300.f), FVector2D(-1100.f, 1300.f), FVector2D(-1100.f, -1300.f), FVector2D(1400.f, -1300.f), FVector2D(1400.f, -300.f) };
		for (int32 i = 0; i + 1 < Yard.Num(); ++i) DryWall(B, Yard[i], Yard[i + 1], 100.f, 70 + i);
		for (int32 m = 0; m < 6; ++m)
		{
			const FVector P = GAt(B, -700.f + (m % 3) * 230.f, 900.f, (m / 3) * 115.f);
			Mortsafe(B, P, 90.f, m == 4, 200 + m);
		}
	}

	// ---------------------------------------------------------------- H9 The Lime Kilns
	void LimeKilns(FBuild& B)
	{
		// Three draw kilns (GY H9; lime from the chalk itself): square stone towers 6 m tall, a pointed draw arch at each
		// front where the burnt lime was raked out, a ramp up the back to charge them; one still warm (a dull glow in its
		// arch, smoke). Barrows of quicklime raked out and never carted (the cache); the pit beside white with lime and
		// full of animal bones. The G3 lesser shrine stands before them.
		for (int32 k = 0; k < 3; ++k)
		{
			const FVector Base = GAt(B, -100.f, -900.f + k * 900.f);
			const float S = 250.f, H = 600.f;
			for (int32 c = 0; c < 6; ++c) B.K.Add(GreyStone(0.82f + 0.08f * (c % 2)), FVector(S * 2.f - c * 14.f, S * 2.f - c * 14.f, H / 6.f - 2.f), Base + FVector(0, 0, c * H / 6.f + H / 12.f));
			B.K.Add(Black(0.5f), FVector(10.f, 160.f, 200.f), Base + FVector(S + 2.f, 0.f, 100.f));
			PointedArch(B, Base + FVector(S + 4.f, 0.f, 0.f), 0.f, 160.f, 120.f, 22.f, 28.f, GreyStone(1.f), 4);
			// The charging ramp up the back (-X), a timber gangway on stone.
			B.K.Rod(GreyStone(0.85f), Base + FVector(-S - 700.f, 0.f, -20.f), Base + FVector(-S, 0.f, H - 40.f), 140.f, true);
			B.Box(Base + FVector(-S - 350.f, 0.f, H * 0.45f - 20.f), FVector(380.f, 70.f, 20.f), FRotator(FMath::RadiansToDegrees(FMath::Atan2(H - 20.f, 700.f)), 0.f, 0.f));
			// The pot at the top: a dark bowl of rubble and lime.
			B.K.Add(Lime(0.9f), FVector(S * 1.2f, S * 1.2f, 20.f), Base + FVector(0, 0, H + 6.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			if (k == 1)
			{
				B.K.Add(FSurf::Lit(Rgb(200, 90, 30) * 0.6f, 2.5f), FVector(8.f, 120.f, 120.f), Base + FVector(S - 4.f, 0.f, 70.f));
				B.Loop(TEXT("forest.chimney"), Base + FVector(0, 0, H + 20.f));
				B.Lamp(Base + FVector(S + 80.f, 0.f, 100.f), FLinearColor(1.f, 0.55f, 0.25f), 1600.f, 700.f);
			}
			B.Box(Base + FVector(0, 0, H * 0.5f), FVector(S, S, H * 0.5f));
		}
		// Barrows (two wheels, two handles, a tray heaped white).
		for (int32 b = 0; b < 3; ++b)
		{
			const FVector P = GAt(B, 600.f, 400.f + b * 140.f, 40.f);
			const FRotator R(0.f, -20.f + 15.f * b, 0.f);
			B.K.Add(OldWood(0.75f), FVector(120.f, 70.f, 30.f), P, R);
			B.K.Add(Lime(1.f), FVector(100.f, 60.f, 22.f), P + FVector(0, 0, 20.f), R, EWRLookMesh::Cylinder);
			B.K.Disc(Iron(), P + R.RotateVector(FVector(70.f, 0.f, -20.f)), R.RotateVector(FVector::RightVector), 44.f, 8.f);
			for (const float S : { -1.f, 1.f }) B.K.Rod(OldWood(0.7f), P + R.RotateVector(FVector(-40.f, S * 30.f, 0.f)), P + R.RotateVector(FVector(-140.f, S * 34.f, -20.f)), 4.f);
		}
	}

	// ---------------------------------------------------------------- H10 Where the Rows End
	void RowsEnd(FBuild& B)
	{
		// The southern rows run into grey dead ground and stop mid-row (GY H10; the Void border): dug graves waiting, no
		// stones, no lanterns. The rows were planned for more dead than came. The field "f_rows_end" lays the rows and the
		// dug graves past their last stones; here the surveyors' line: stakes and a cord running on past the last grave.
		for (int32 s = 0; s < 14; ++s)
		{
			const FVector P = GAt(B, -1400.f + s * 200.f, 1500.f);
			B.K.Add(OldWood(0.7f), FVector(6.f, 6.f, 70.f), P + FVector(0, 0, 25.f));
			if (s > 0) B.Line(GAt(B, -1600.f + s * 200.f, 1500.f, 55.f), P + FVector(0, 0, 55.f), 0.5f, 6.f, RopeC(), 4);
		}
		// A spade left in the last spoil heap.
		const FVector Sp = GAt(B, 1200.f, 900.f, 30.f);
		B.K.Rod(OldWood(0.8f), Sp, Sp + FVector(10.f, 0.f, 100.f), 3.f);
		B.K.Add(Iron(), FVector(4.f, 22.f, 30.f), Sp + FVector(0, 0, -10.f));
	}

	// ---------------------------------------------------------------- H11 The Lantern Pond
	void LanternPond(FBuild& B, AWRGraveScenery* A)
	{
		// A still pond before the chapel, its surface covered edge to edge in drifted paper lanterns, some still lit (GY H11;
		// tōrō nagashi). Ghost carp are fished here. A kerb of old stone along the chapel side.
		const FWRGraveData& D = FWRGraveData::Get();
		const FVector C = B.Local(D.PondAt);
		WaterPond(A, C + FVector(0, 0, 2.f), D.PondRadii.X, D.PondRadii.Y);
		for (int32 i = 0; i < 180; ++i)
		{
			const float Ang = B.Rand(i * 3) * 2.f * PI, R = FMath::Sqrt(B.Rand(i * 3 + 1)) * 0.94f;
			const FVector P = C + FVector(FMath::Cos(Ang) * D.PondRadii.X * R, FMath::Sin(Ang) * D.PondRadii.Y * R, 6.f);
			const bool bBox = i % 3 == 0;
			B.K.Add(Paper(0.75f + 0.25f * B.Rand(i)), bBox ? FVector(20.f, 20.f, 18.f) : FVector(18.f, 18.f, 18.f), P, FRotator(4.f * B.Rand(i + 5), 90.f * B.Rand(i + 7), 0.f), bBox ? EWRLookMesh::Cube : EWRLookMesh::Cylinder);
			B.K.Add(OldWood(0.7f), FVector(24.f, 24.f, 3.f), P - FVector(0, 0, 9.f));
			if (B.Rand(i + 11) < 0.4f) B.N.Add(B.Rand(i + 13) < 0.7f ? GraveLight(3.f) : Candle(3.f), FVector(19.f, 19.f, 18.f), P, FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		}
		for (int32 k = 0; k < 16; ++k)
		{
			const float Ang = PI * 0.15f + k * PI * 0.7f / 15.f;
			const FVector P(C.X + FMath::Cos(Ang) * (D.PondRadii.X + 60.f), C.Y + FMath::Sin(Ang) * (D.PondRadii.Y + 60.f), 0.f);
			B.K.Add(GreyStone(0.85f), FVector(120.f, 50.f, 30.f), FVector(P.X, P.Y, GZ(B, P.X, P.Y) + 8.f), FRotator(0.f, FMath::RadiansToDegrees(Ang) + 90.f, 0.f));
		}
		B.Lamp(C + FVector(0, 0, 150.f), GraveLightC(), 1600.f, 3000.f);
	}

	// ---------------------------------------------------------------- H12 The Root-Split Tombs
	void RootSplitTombs(FBuild& B)
	{
		// At Rootgrave's mouth the forest's roots have burst tombs open from below and lifted the coffins into the air (GY
		// H12; world-map: giant roots grip an old cemetery): the opposite biome pushing in. Five stone tomb-chests, split;
		// roots as thick as a body heaving out of them, two coffins held up in the roots at 2-4 m.
		for (int32 t = 0; t < 5; ++t)
		{
			const float X = -700.f + t * 360.f, Y = (t % 2 ? 1.f : -1.f) * 350.f;
			const FVector Base = GAt(B, X, Y);
			const float Yw = 20.f * (B.Rand(t) - 0.5f);
			const FRotator R(0.f, Yw, 0.f);
			// The tomb chest's two halves pushed apart, its lid tipped off.
			for (const float S : { -1.f, 1.f }) B.K.Add(GreyStone(0.85f), FVector(220.f, 50.f, 90.f), Base + R.RotateVector(FVector(0.f, S * (45.f + 25.f * B.Rand(t + 3)), 45.f)), R + FRotator(0.f, 0.f, S * 14.f));
			B.K.Add(GreyStone(0.9f), FVector(240.f, 110.f, 18.f), Base + R.RotateVector(FVector(40.f, 150.f, 30.f)), R + FRotator(-10.f, 15.f, 35.f));
			B.Box(Base + FVector(0, 0, 45.f), FVector(115.f, 75.f, 45.f), R);
			// The roots: three heaving up out of the split, arching over and back down.
			for (int32 r = 0; r < 3; ++r)
			{
				const FVector Dir = FRotator(0.f, Yw + 120.f * r + 30.f, 0.f).Vector();
				const float Up = 220.f + 180.f * B.Rand(t * 7 + r);
				TArray<FVector> Root = { Base + FVector(0, 0, -40.f), Base + Dir * 60.f + FVector(0, 0, Up * 0.7f), Base + Dir * 220.f + FVector(0, 0, Up), Base + Dir * 420.f + FVector(0, 0, Up * 0.5f), Base + Dir * 560.f + FVector(0, 0, GZ(B, Base.X + Dir.X * 560.f, Base.Y + Dir.Y * 560.f) - Base.Z - 20.f) };
				B.Root(Root, 42.f, 14.f, RootC(), 9);
				B.SegBox(Root[1], Root[2], 38.f, 38.f);
				B.SegBox(Root[2], Root[3], 32.f, 32.f);
				if (r == 0 && t % 2 == 0)
				{
					// A coffin held in the root's arch: tilted, its foot hanging out.
					Coffin(B, Root[2] + FVector(0, 0, 50.f), FRotator(-25.f, Yw + 120.f * r + 30.f, 10.f), t == 2, false, OldWood(0.75f));
				}
			}
			// White blossom on the roots (the forest's side).
			for (int32 f = 0; f < 10; ++f) B.Ball(Base + FVector((B.Rand(t * 13 + f) - 0.5f) * 400.f, (B.Rand(t * 17 + f) - 0.5f) * 400.f, 60.f + 200.f * B.Rand(t * 19 + f)), 6.f, FLinearColor(0.85f, 0.84f, 0.8f), 0.6f);
		}
	}

	// ---------------------------------------------------------------- H13 Foot of the Stair
	void StairFoot(FBuild& B)
	{
		// The bottom of the Lantern Stair: a stone gate (two square pillars, a carved lintel, a roof cap) and a pile of
		// travellers' sandals left at its foot, all one style (GY H13): everyone climbed, and nobody came down. The G4
		// lesser shrine stands beside it.
		for (const float S : { -1.f, 1.f })
		{
			const FVector P = GAt(B, 0.f, S * 220.f);
			B.K.Add(Limestone(0.9f), FVector(70.f, 70.f, 380.f), P + FVector(0, 0, 190.f));
			B.K.Add(Limestone(1.f), FVector(90.f, 90.f, 30.f), P + FVector(0, 0, 15.f));
			B.Box(P + FVector(0, 0, 190.f), FVector(35.f, 35.f, 190.f));
		}
		const float Zl = GZ(B, 0.f, 0.f) + 380.f;
		B.K.Add(Limestone(0.95f), FVector(80.f, 560.f, 50.f), FVector(0.f, 0.f, Zl + 25.f));
		B.K.Add(DarkStone(0.8f), FVector(82.f, 360.f, 20.f), FVector(0.f, 0.f, Zl + 25.f));
		B.K.Add(GreyStone(0.9f), FVector(120.f, 640.f, 20.f), FVector(0.f, 0.f, Zl + 60.f));
		for (const float S : { -1.f, 1.f }) B.K.Add(GreyStone(0.9f), FVector(40.f, 40.f, 30.f), FVector(0.f, S * 320.f, Zl + 74.f), FRotator(15.f, 0.f, S * 20.f));
		// The sandals: woven soles with straw ties, heaped against the left pillar and spilling out.
		for (int32 i = 0; i < 70; ++i)
		{
			const float Ang = B.Rand(i * 5) * 2.f * PI, R = FMath::Sqrt(B.Rand(i * 5 + 1)) * 130.f;
			const FVector P = GAt(B, -90.f + FMath::Cos(Ang) * R, -320.f + FMath::Sin(Ang) * R, 4.f + (130.f - R) * 0.18f);
			B.K.Add(Sackcloth(0.8f + 0.2f * B.Rand(i)), FVector(26.f, 10.f, 2.f), P, FRotator(30.f * (B.Rand(i + 7) - 0.5f), B.Rand(i + 9) * 360.f, 30.f * (B.Rand(i + 11) - 0.5f)));
			B.K.Add(FSurf::Cloth(Rgb(201, 168, 90) * 0.5f), FVector(2.f, 10.f, 2.f), P + FVector(0, 0, 2.f), FRotator(0.f, B.Rand(i + 9) * 360.f + 90.f, 0.f));
		}
	}

	// ================================================================ the kit
	/** A crypt mouth (GY 2.4 kit; 6 of them, the catacombs' doors): a stone hood over a stair cut down into the chalk, its
	 *  porch's apron covering the hole's edge, the flight running down Drop over Run along the mouth's facing (+X) to a
	 *  short level passage into its room. Iron grille gate standing open. */
	void CryptMouth(FBuild& B, AWRGraveScenery* A)
	{
		const FWRGraveData& D = FWRGraveData::Get();
		const FWRGraveData::FCrypt* C = D.Crypts.FindByPredicate([A](const FWRGraveData::FCrypt& Q) { return Q.Id == A->SiteId; });
		if (!C) return;
		const float W = 260.f, Hh = 300.f;
		const int32 N = FMath::Max(4, FMath::CeilToInt(C->Drop / 30.f));
		// The apron: flags round the hole on the ground, outward to cover the dropped triangles (hole + 4 m).
		const FVector2D Half = C->HoleHalf;
		const FVector Hc = B.Local(C->HoleCentre);
		for (float X = -Half.X - 100.f; X <= Half.X + 100.f; X += 100.f)
		{
			for (float Y = -Half.Y - 100.f; Y <= Half.Y + 100.f; Y += 100.f)
			{
				if (FMath::Abs(X) < 280.f + 50.f && FMath::Abs(Y) < W * 0.5f + 40.f) continue;   // the stairwell's mouth
				B.K.Add(GreyStone(0.75f + 0.15f * B.Rand(int32(X * 7 + Y))), FVector(96.f, 96.f, 14.f), FVector(Hc.X + X, Hc.Y + Y, GZ(B, Hc.X + X, Hc.Y + Y) + 4.f));
			}
		}
		// The stair: treads from the top (local 0) down along +X; side walls and a barrel vault once it's underground.
		for (int32 s = 0; s < N; ++s)
		{
			const float X0 = C->Run * s / N, X1 = C->Run * (s + 1) / N;
			const float Z = -C->Drop * (s + 1) / N;
			B.K.Add(Limestone(0.8f), FVector(X1 - X0 + 2.f, W, 40.f), FVector((X0 + X1) * 0.5f, 0.f, Z - 20.f));
			for (const float S : { -1.f, 1.f }) B.K.Add(Limestone(0.75f), FVector(X1 - X0 + 2.f, 40.f, Hh + 40.f), FVector((X0 + X1) * 0.5f, S * (W * 0.5f + 20.f), Z + Hh * 0.5f));
			if (Z < -280.f) for (int32 v = 0; v < 5; ++v)
			{
				const float Av = PI * (v + 0.5f) / 5.f;
				B.K.Add(Limestone(0.7f), FVector(X1 - X0 + 2.f, 30.f, 60.f), FVector((X0 + X1) * 0.5f, -FMath::Cos(Av) * W * 0.5f, Z + Hh - 30.f + FMath::Sin(Av) * 60.f), FRotator(0.f, 0.f, FMath::RadiansToDegrees(Av) - 90.f));
			}
		}
		const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(C->Drop, C->Run));
		B.Box(FVector(C->Run * 0.5f, 0.f, -C->Drop * 0.5f - 30.f), FVector(FMath::Sqrt(C->Run * C->Run + C->Drop * C->Drop) * 0.5f, W * 0.5f, 25.f), FRotator(-Pitch, 0.f, 0.f));
		for (const float S : { -1.f, 1.f }) B.Box(FVector(C->Run * 0.5f, S * (W * 0.5f + 20.f), -C->Drop * 0.5f + Hh * 0.5f), FVector(FMath::Sqrt(C->Run * C->Run + C->Drop * C->Drop) * 0.5f, 20.f, Hh * 0.5f + 60.f), FRotator(-Pitch, 0.f, 0.f));
		B.Box(FVector(C->Run * 0.5f + 150.f, 0.f, -C->Drop * 0.5f + Hh + 40.f), FVector(FMath::Sqrt(C->Run * C->Run + C->Drop * C->Drop) * 0.5f, W * 0.5f + 40.f, 30.f), FRotator(-Pitch, 0.f, 0.f));
		// The level passage on from the stair's foot to the room's door.
		if (const FWRGraveData::FRoom* Room = D.FindRoom(C->Room))
		{
			const FVector Rm = B.Local(Room->At);
			const float X0 = C->Run, X1 = FMath::Max(X0 + 100.f, Rm.X - Room->W * 0.5f + 20.f);
			const float Z = -C->Drop;
			B.K.Add(Limestone(0.75f), FVector(X1 - X0, W, 30.f), FVector((X0 + X1) * 0.5f, 0.f, Z - 15.f));
			for (const float S : { -1.f, 1.f }) B.K.Add(Limestone(0.72f), FVector(X1 - X0, 40.f, Hh), FVector((X0 + X1) * 0.5f, S * (W * 0.5f + 20.f), Z + Hh * 0.5f));
			B.K.Add(Limestone(0.7f), FVector(X1 - X0, W + 80.f, 40.f), FVector((X0 + X1) * 0.5f, 0.f, Z + Hh + 20.f));
			B.Box(FVector((X0 + X1) * 0.5f, 0.f, Z - 15.f), FVector((X1 - X0) * 0.5f, W * 0.5f, 15.f));
			for (const float S : { -1.f, 1.f }) B.Box(FVector((X0 + X1) * 0.5f, S * (W * 0.5f + 20.f), Z + Hh * 0.5f), FVector((X1 - X0) * 0.5f, 20.f, Hh * 0.5f));
			B.Box(FVector((X0 + X1) * 0.5f, 0.f, Z + Hh + 20.f), FVector((X1 - X0) * 0.5f, W * 0.5f + 40.f, 20.f));
			// A candle in a wall niche at the stair's foot (the Mourners lit the way down).
			B.N.Add(Candle(4.f), FVector(4.f, 4.f, 8.f), FVector(X0 + 40.f, W * 0.5f - 10.f, Z + 150.f));
		}
		// The hood: a gabled stone porch over the stair's head, pilasters, the open iron grille gate swung back.
		const FSurf S = GreyStone(0.9f);
		for (const float Sd : { -1.f, 1.f })
		{
			B.K.Add(S, FVector(320.f, 50.f, 260.f), FVector(80.f, Sd * (W * 0.5f + 45.f), GZ(B, 80.f, Sd * 175.f) + 130.f));
			B.Box(FVector(80.f, Sd * (W * 0.5f + 45.f), GZ(B, 80.f, Sd * 175.f) + 130.f), FVector(160.f, 25.f, 130.f));
		}
		const float Zt = GZ(B, 0.f, 0.f) + 260.f;
		for (const float Sd : { -1.f, 1.f }) B.K.Add(Slate(0.8f), FVector(360.f, 200.f, 14.f), FVector(80.f, Sd * 95.f, Zt + 70.f), FRotator(0.f, 0.f, -Sd * 35.f));
		B.K.Add(S, FVector(30.f, W + 120.f, 120.f), FVector(-70.f, 0.f, Zt + 30.f));
		B.K.Add(S, FVector(30.f, 30.f, 70.f), FVector(-70.f, 0.f, Zt + 120.f));
		B.K.Add(S, FVector(30.f, 80.f, 18.f), FVector(-70.f, 0.f, Zt + 140.f));
		for (float Y = -100.f; Y <= 100.f; Y += 14.f) B.K.Rod(Iron(), FVector(-80.f + 60.f, -W * 0.5f - 40.f + (Y + 100.f) * 0.15f, GZ(B, -20.f, -170.f)), FVector(-80.f + 60.f, -W * 0.5f - 40.f + (Y + 100.f) * 0.15f, GZ(B, -20.f, -170.f) + 220.f), 2.f);
		B.Lamp(FVector(200.f, 0.f, GZ(B, 0.f, 0.f) - 200.f), CandleC(), 300.f, 600.f);
		(void)A;
	}

	/** A collapsed subsidence pit with a rope down (GY 2.4 kit; a back door into the catacombs): the grave field fallen
	 *  into the quarry below, broken coffin boards in the slumped soil, a rope tied to a stake hanging to the room floor. */
	void SubsidenceHole(FBuild& B, AWRGraveScenery* A)
	{
		const FWRGraveData& D = FWRGraveData::Get();
		const FWRGraveData::FHole* H = D.Holes.FindByPredicate([A](const FWRGraveData::FHole& Q) { return Q.Id == A->SiteId; });
		if (!H) return;
		const float R = 240.f;
		// The shaft's chalk walls down to the room's ceiling (a ring of stones, rough).
		for (int32 k = 0; k < 10; ++k)
		{
			const float Ang = k * 2.f * PI / 10.f;
			const FVector Dir(FMath::Cos(Ang), FMath::Sin(Ang), 0.f);
			B.K.Add(Chalk(0.75f), FVector(170.f, 60.f, H->Drop), Dir * (R + 30.f) + FVector(0, 0, -H->Drop * 0.5f), FRotator(0.f, FMath::RadiansToDegrees(Ang) + 90.f, 0.f));
			B.Box(Dir * (R + 30.f) + FVector(0, 0, -H->Drop * 0.5f), FVector(85.f, 30.f, H->Drop * 0.5f), FRotator(0.f, FMath::RadiansToDegrees(Ang) + 90.f, 0.f));
		}
		// The slumped rim: soil, broken coffin boards, a stone gone in.
		for (int32 k = 0; k < 8; ++k) B.K.Add(OldWood(0.7f), FVector(150.f, 20.f, 4.f), GAt(B, FMath::Cos(k * 0.9f) * (R + 120.f), FMath::Sin(k * 0.9f) * (R + 120.f), 4.f), FRotator(15.f * (k % 3 - 1), 40.f * k, 10.f));
		Headstone(B, GAt(B, R + 60.f, 80.f), 160.f, 0, 80.f, 40.f, false, false, false, 77);
		// The rope: tied to a stake at the rim, down the shaft.
		const FVector Stake = GAt(B, -R - 160.f, 0.f);
		B.K.Add(OldWood(0.7f), FVector(14.f, 14.f, 90.f), Stake + FVector(0, 0, 20.f), FRotator(-15.f, 0.f, 0.f));
		B.Line(Stake + FVector(10.f, 0.f, 50.f), FVector(-R + 30.f, 0.f, -10.f), 2.f, 30.f, RopeC(), 6);
		B.Line(FVector(-R + 30.f, 0.f, -10.f), FVector(-R + 40.f, 0.f, -H->Drop + 50.f), 2.f, 0.f, RopeC(), 2);
	}

	void Mausoleum(FBuild& B, AWRGraveScenery* A)
	{
		// A family mausoleum (GY 2.4 kit: small, bronze-door, collapsed with a den, wax-crusted with a Wickbeetle nest),
		// in its railed plot with the family's own stones round it.
		const int32 V = A->Variant % 4;
		const FVector Base = GAt(B, 0.f, 0.f);
		WRGraveLook::Mausoleum(B, Base, 0.f, 400.f + 120.f * (V == 1), 500.f + 100.f * (V == 1), 380.f + 80.f * (V == 1), V, A->Variant * 7 + 3);
		const float Hx = 600.f, Hy = 500.f;
		Railing(B, FVector2D(-Hx, -Hy), FVector2D(Hx, -Hy));
		Railing(B, FVector2D(-Hx, Hy), FVector2D(Hx, Hy));
		Railing(B, FVector2D(-Hx, -Hy), FVector2D(-Hx, Hy));
		Railing(B, FVector2D(Hx, -Hy), FVector2D(Hx, -150.f));
		Railing(B, FVector2D(Hx, 150.f), FVector2D(Hx, Hy));
		for (int32 i = 0; i < 4; ++i) Headstone(B, GAt(B, 420.f, -350.f + i * 230.f), 0.f, i % 2, 80.f + 20.f * B.Rand(i), 4.f * B.Rand(i + 3), false, i == 2, true, A->Variant * 13 + i);
	}

	void MortsafeCluster(FBuild& B, AWRGraveScenery* A)
	{
		// Graves under iron cages (GY 2.4 kit: 3, 5 or 9 cages; one prised open): someone robbed these graves and someone
		// tried to stop it. The cages you can break for grave iron are the server's nodes (they stand on these graves);
		// the prised one is here, bent and broken.
		const int32 N = A->Variant % 5 == 2 ? 9 : A->Variant % 2 ? 5 : 3;
		for (int32 i = 0; i < N; ++i)
		{
			const float X = (i % 3 - 1) * 240.f, Y = (i / 3 - 1) * 320.f;
			const FVector Base = GAt(B, X - 120.f, Y);
			Headstone(B, Base, 0.f, 0, 85.f, 2.f * B.Rand(i), false, false, true, A->Variant * 17 + i);
			B.K.Add(Soil(0.85f), FVector(180.f, 80.f, 10.f), Base + FVector(100.f, 0.f, 2.f));
		}
		Mortsafe(B, GAt(B, 380.f, 0.f), 0.f, true, A->Variant * 3);
		B.K.Add(Black(0.35f), FVector(150.f, 60.f, 4.f), GAt(B, 380.f, 0.f, 1.f));
	}

	void SoulstoneOutcrop(FBuild& B, AWRGraveScenery* A)
	{
		// A grave slumped open on a slope, its bones crusted in soulstone (GY 2.4 kit; vivianite on bone): clear crystal
		// with a pale-green glow on the bone bed. The nodes are the server's; this is the slumped grave and the bed.
		const FVector Base = GAt(B, 0.f, 0.f);
		B.K.Add(Soil(0.8f), FVector(400.f, 260.f, 40.f), Base + FVector(0, 0, -10.f), FRotator(-12.f, 0.f, 4.f));
		Coffin(B, Base + FVector(-60.f, 40.f, 10.f), FRotator(-14.f, 20.f, 12.f), true, false, OldWood(0.6f));
		Headstone(B, Base + FVector(-260.f, 0.f, 0.f), 0.f, 0, 90.f, 0.f, true, false, false, A->Variant * 5);
		BonePile(B, Base + FVector(120.f, -60.f, 0.f), 70.f, A->Variant * 23, 1);
		for (int32 k = 0; k < 12; ++k)
		{
			const FVector P = Base + FVector(40.f + 160.f * B.Rand(k), -120.f + 240.f * B.Rand(k + 9), 10.f + 20.f * B.Rand(k + 3));
			B.K.Add(Soulstone(1.2f), FVector(8.f, 8.f, 16.f + 18.f * B.Rand(k + 5)), P, FRotator(30.f * (B.Rand(k + 7) - 0.5f), 0.f, 30.f * (B.Rand(k + 8) - 0.5f)), EWRLookMesh::Cone);
		}
		B.Lamp(Base + FVector(100.f, 0.f, 60.f), FLinearColor(FColor(159, 227, 192)), 500.f, 500.f);
	}

	void KillScene(FBuild& B, AWRGraveScenery* A)
	{
		// A predator's kill (GY 2.4 kit: Gravehounds on a Marrowgoat, Gloamcrows on a Palestrider, a Tombback burying a
		// Mournhare): the carcass left, the ground scored; the living part (the creatures) is the creature track's.
		const int32 V = A->Variant % 3;
		const FVector Base = GAt(B, 0.f, 0.f);
		const float Len = V == 1 ? 220.f : V == 0 ? 120.f : 50.f;
		// The ribcage, a long bone or two, a hide gone ragged.
		for (int32 r = 0; r < 7; ++r)
		{
			const float X = -Len * 0.4f + r * Len * 0.12f;
			for (const float S : { -1.f, 1.f }) B.Root({ Base + FVector(X, 0.f, Len * 0.18f), Base + FVector(X, S * Len * 0.22f, Len * 0.15f), Base + FVector(X + 6.f, S * Len * 0.25f, 4.f) }, 2.2f * Len / 120.f, 1.6f * Len / 120.f, BoneC(), 5);
		}
		LongBone(B, Base + FVector(Len * 0.5f, -30.f, 6.f), Base + FVector(Len * 0.9f, -20.f, 6.f), 2.5f * Len / 120.f, BoneC());
		B.K.Add(FSurf::Cloth(V == 1 ? Rgb(216, 212, 204) * 0.5f : Rgb(230, 225, 211) * 0.5f), FVector(Len * 0.7f, Len * 0.4f, 4.f), Base + FVector(-Len * 0.3f, Len * 0.3f, 3.f), FRotator(0.f, 30.f, 0.f));
		if (V == 1) Skull(B, Base + FVector(-Len * 0.7f, 0.f, 20.f), 2.4f, 190.f, BoneC());
		if (V == 2)
		{
			// The Tombback's mound: a grave slab lying over fresh soil where it buried its kill.
			B.K.Add(Soil(0.9f), FVector(160.f, 120.f, 40.f), Base + FVector(140.f, 0.f, 8.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			B.K.Add(GreyStone(0.8f), FVector(120.f, 70.f, 14.f), Base + FVector(140.f, 0.f, 30.f), FRotator(4.f, 10.f, 0.f));
		}
	}

	void WaysideToro(FBuild& B, AWRGraveScenery* A)
	{
		// A wayside tōrō shrine by the road (GY 2.4 kit: offerings of coins, a doll, a bowl, a lock of hair): a lantern in a
		// small stone niche, the offering on its sill. Faces the road (+X).
		const FVector Base = GAt(B, 0.f, 0.f);
		B.K.Add(GreyStone(0.85f), FVector(70.f, 180.f, 180.f), Base + FVector(-40.f, 0.f, 90.f));
		B.K.Add(GreyStone(0.85f), FVector(140.f, 40.f, 200.f), Base + FVector(0.f, -110.f, 100.f));
		B.K.Add(GreyStone(0.85f), FVector(140.f, 40.f, 200.f), Base + FVector(0.f, 110.f, 100.f));
		B.K.Add(Slate(0.8f), FVector(180.f, 270.f, 16.f), Base + FVector(0.f, 0.f, 208.f), FRotator(-6.f, 0.f, 0.f));
		Toro(B, Base + FVector(10.f, 0.f, 0.f), 0.f, 0.7f, true);
		const int32 V = A->Variant % 4;
		const FVector Sill = Base + FVector(40.f, 40.f, 22.f);
		if (V == 0) for (int32 c = 0; c < 6; ++c) B.K.Disc(Verdigris(1.1f), Sill + FVector(4.f * c, -6.f * c, c * 0.6f), FVector::UpVector, 7.f, 1.f);
		else if (V == 1) { B.Ball(Sill + FVector(0, 0, 16.f), 5.f, Rgb(220, 200, 180) * 0.5f, 0.8f); B.K.Add(Linen(0.8f), FVector(10.f, 8.f, 18.f), Sill + FVector(0, 0, 6.f)); }
		else if (V == 2) B.K.Add(FSurf::Glaze(Rgb(140, 120, 100) * 0.5f, 0.4f), FVector(22.f, 22.f, 8.f), Sill + FVector(0, 0, 4.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		else B.Root({ Sill, Sill + FVector(10.f, 4.f, 0.f), Sill + FVector(18.f, 10.f, 1.f) }, 1.4f, 0.6f, Rgb(40, 30, 24) * 0.6f, 4);
		B.Box(Base + FVector(-20.f, 0.f, 100.f), FVector(80.f, 130.f, 100.f));
		A->NearLamps = A->NightLights;
		A->NearPower = A->NightPower;
		A->NightLights.Reset();
		A->NightPower.Reset();
	}

	void BurnedCottage(FBuild& B, AWRGraveScenery* A)
	{
		// A burned cottage and its ergot field (survival-crafting KEPT): black rye gone to ergot in a fenced field (the wild
		// rye nodes are the server's), a crooked scarecrow, the cottage a blackened shell.
		B.Cottage(GAt(B, 0.f, 0.f), 0.f, 1, true);
		for (int32 k = 0; k < 12; ++k) B.K.Add(Black(0.5f), FVector(80.f, 60.f, 20.f), GAt(B, -200.f + 400.f * B.Rand(k), -200.f + 400.f * B.Rand(k + 5), 6.f), FRotator(0.f, 60.f * k, 10.f));
		const FVector Sc = GAt(B, 1700.f, 300.f);
		B.K.Rod(OldWood(0.7f), Sc, Sc + FVector(10.f, 0.f, 220.f), 6.f);
		B.K.Rod(OldWood(0.7f), Sc + FVector(0, -90.f, 160.f), Sc + FVector(10.f, 90.f, 175.f), 5.f);
		B.K.Add(Sackcloth(0.8f), FVector(30.f, 60.f, 80.f), Sc + FVector(5.f, 0.f, 150.f), FRotator(0.f, 0.f, 8.f));
		B.Ball(Sc + FVector(8.f, 0.f, 215.f), 16.f, Rgb(150, 132, 104) * 0.45f, 0.95f);
		for (int32 p = 0; p < 9; ++p) B.K.Add(OldWood(0.7f), FVector(8.f, 8.f, 110.f), GAt(B, 1300.f + p * 120.f, -500.f, 50.f), FRotator(4.f * (p % 3 - 1), 0.f, 0.f));
		B.Line(GAt(B, 1300.f, -500.f, 90.f), GAt(B, 2260.f, -500.f, 90.f), 1.f, 20.f, RopeC(), 8);
	}

	void AsphodelMeadow(FBuild& B, AWRGraveScenery* A)
	{
		// Pale lilies across a hollow and a half-built bone altar in the middle (survival-crafting KEPT; the bone altar
		// ritual's place): a stone slab on two stones, bones stacked on it in courses, the work stopped.
		const FVector Base = GAt(B, 0.f, 0.f);
		B.K.Add(GreyStone(0.85f), FVector(200.f, 120.f, 20.f), Base + FVector(0, 0, 70.f));
		for (const float S : { -1.f, 1.f }) B.K.Add(GreyStone(0.8f), FVector(60.f, 100.f, 70.f), Base + FVector(S * 70.f, 0.f, 35.f));
		for (int32 c = 0; c < 3; ++c) for (int32 k = 0; k < 6 - c; ++k) LongBone(B, Base + FVector(-80.f + k * 30.f + c * 15.f, -40.f, 86.f + c * 9.f), Base + FVector(-80.f + k * 30.f + c * 15.f, 40.f, 86.f + c * 9.f), 2.2f, BoneC());
		Skull(B, Base + FVector(60.f, 0.f, 120.f), 1.f, 0.f, BoneC());
		B.Box(Base + FVector(0, 0, 50.f), FVector(100.f, 60.f, 50.f));
		for (int32 f = 0; f < 60; ++f)
		{
			const float Ang = B.Rand(f * 3) * 2.f * PI, R = 200.f + 900.f * B.Rand(f * 3 + 1);
			const FVector P = GAt(B, FMath::Cos(Ang) * R, FMath::Sin(Ang) * R);
			B.Limb(P, P + FVector(0, 0, 70.f + 30.f * B.Rand(f)), 1.2f, 0.8f, Rgb(110, 122, 94) * 0.45f, 4);
			for (int32 b = 0; b < 5; ++b) B.Ball(P + FVector(0, 0, 55.f + b * 8.f), 3.5f, FLinearColor(0.82f, 0.8f, 0.76f), 0.6f);
		}
	}

	void OssuaryKitchen(FBuild& B, AWRGraveScenery* A)
	{
		// A crypt-keeper's hearth (survival-crafting KEPT): a half-sunk stone room open on one side, a hearth with a pot,
		// shelves of mourner's bread gone to stone, bones shelved by kind (the keepers sorted everything).
		const FVector Base = GAt(B, 0.f, 0.f);
		for (const float S : { -1.f, 1.f }) { B.K.Add(GreyStone(0.85f), FVector(500.f, 50.f, 260.f), Base + FVector(0.f, S * 250.f, 130.f)); B.Box(Base + FVector(0.f, S * 250.f, 130.f), FVector(250.f, 25.f, 130.f)); }
		B.K.Add(GreyStone(0.85f), FVector(50.f, 550.f, 260.f), Base + FVector(-250.f, 0.f, 130.f));
		B.Box(Base + FVector(-250.f, 0.f, 130.f), FVector(25.f, 275.f, 130.f));
		B.K.Add(Slate(0.75f), FVector(560.f, 580.f, 18.f), Base + FVector(0.f, 0.f, 268.f), FRotator(-5.f, 0.f, 0.f));
		// The hearth against the back wall, its chimney, a pot on a crane.
		B.K.Add(GreyStone(0.7f), FVector(80.f, 200.f, 160.f), Base + FVector(-200.f, 0.f, 80.f));
		B.K.Add(Black(0.5f), FVector(10.f, 130.f, 90.f), Base + FVector(-158.f, 0.f, 50.f));
		B.Ball(Base + FVector(-140.f, 0.f, 50.f), 26.f, Rgb(40, 38, 36) * 0.5f, 0.5f, 0.8f);
		for (int32 s = 0; s < 3; ++s)
		{
			B.K.Add(OldWood(0.7f), FVector(40.f, 300.f, 5.f), Base + FVector(-60.f + s * 0.f, 220.f, 60.f + s * 60.f));
			for (int32 k = 0; k < 6; ++k)
			{
				if (s == 1) B.Ball(Base + FVector(-60.f, 100.f + k * 45.f, 75.f + s * 60.f), 12.f, Rgb(120, 110, 96) * 0.5f, 0.95f, 0.6f);
				else LongBone(B, Base + FVector(-75.f, 90.f + k * 45.f, 66.f + s * 60.f), Base + FVector(-45.f, 90.f + k * 45.f, 66.f + s * 60.f), 2.f, BoneC());
			}
		}
	}

	void BoneCairn(FBuild& B, AWRGraveScenery* A)
	{
		// Skulls stacked as a waymark on a rise (GY 2.4 kit): a cone of stones capped with a ring of skulls facing out.
		const FVector Base = GAt(B, 0.f, 0.f);
		for (int32 c = 0; c < 4; ++c) for (int32 k = 0; k < 8 - c * 2; ++k)
		{
			const float Ang = (k + 0.5f * c) * 2.f * PI / (8 - c * 2);
			B.K.Add(GreyStone(0.75f + 0.1f * (k % 3)), FVector(40.f, 34.f, 26.f), Base + FVector(FMath::Cos(Ang) * (70.f - c * 16.f), FMath::Sin(Ang) * (70.f - c * 16.f), 12.f + c * 24.f), FRotator(0.f, FMath::RadiansToDegrees(Ang), 0.f));
		}
		for (int32 k = 0; k < 5; ++k) Skull(B, Base + FVector(FMath::Cos(k * 1.256f) * 18.f, FMath::Sin(k * 1.256f) * 18.f, 112.f + (k % 2) * 16.f), 1.f, FMath::RadiansToDegrees(k * 1.256f), BoneC());
		B.Box(Base + FVector(0, 0, 60.f), FVector(70.f, 70.f, 60.f));
		(void)A;
	}

	void StarfallMound(FBuild& B, AWRGraveScenery* A)
	{
		// The Starfall shaft, sealed long ago (GY C8): a low turf mound with a ring of kerb stones and a great capstone, a
		// soulstone-crusted iron band round it, under which the shaft drops to the meteorite in the quarry floor.
		const FVector Base = GAt(B, 0.f, 0.f);
		B.K.Add(Turf(0.9f), FVector(1600.f, 1600.f, 160.f), Base + FVector(0, 0, 10.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		for (int32 k = 0; k < 18; ++k) B.K.Add(GreyStone(0.8f), FVector(100.f, 60.f, 70.f), Base + FRotator(0.f, k * 20.f, 0.f).Vector() * 820.f + FVector(0, 0, 20.f), FRotator(0.f, k * 20.f + 90.f, 0.f));
		B.K.Add(DarkStone(0.9f), FVector(420.f, 360.f, 60.f), Base + FVector(0, 0, 120.f), FRotator(2.f, 18.f, 0.f));
		B.K.Add(Iron(), FVector(440.f, 380.f, 10.f), Base + FVector(0, 0, 112.f), FRotator(2.f, 18.f, 0.f));
		B.Box(Base + FVector(0, 0, 60.f), FVector(800.f, 800.f, 70.f));
		(void)A;
	}

	bool Build(FBuild& B, AWRGraveScenery* A)
	{
		const FName K = A->Kind;
		if (K == TEXT("lychgate_chapel")) LychgateChapel(B);
		else if (K == TEXT("row_of_named")) RowOfNamed(B);
		else if (K == TEXT("dog_graves")) DogGraves(B);
		else if (K == TEXT("ford_of_washing")) FordOfWashing(B);
		else if (K == TEXT("smiths_row")) SmithsRow(B);
		else if (K == TEXT("seven_names")) SevenNames(B, A);
		else if (K == TEXT("broken_procession")) BrokenProcession(B);
		else if (K == TEXT("watch_house")) WatchHouse(B);
		else if (K == TEXT("lime_kilns")) LimeKilns(B);
		else if (K == TEXT("rows_end")) RowsEnd(B);
		else if (K == TEXT("lantern_pond")) LanternPond(B, A);
		else if (K == TEXT("root_split_tombs")) RootSplitTombs(B);
		else if (K == TEXT("stair_foot")) StairFoot(B);
		else if (K == TEXT("crypt_mouth")) CryptMouth(B, A);
		else if (K == TEXT("subsidence_hole")) SubsidenceHole(B, A);
		else if (K == TEXT("starfall_mound")) StarfallMound(B, A);
		else if (K == TEXT("mausoleum")) Mausoleum(B, A);
		else if (K == TEXT("mortsafe_cluster")) MortsafeCluster(B, A);
		else if (K == TEXT("soulstone_outcrop")) SoulstoneOutcrop(B, A);
		else if (K == TEXT("kill_scene")) KillScene(B, A);
		else if (K == TEXT("wayside_toro")) WaysideToro(B, A);
		else if (K == TEXT("burned_cottage")) BurnedCottage(B, A);
		else if (K == TEXT("asphodel_meadow")) AsphodelMeadow(B, A);
		else if (K == TEXT("ossuary_kitchen")) OssuaryKitchen(B, A);
		else if (K == TEXT("bone_cairn")) BoneCairn(B, A);
		else return false;
		return true;
	}
}
