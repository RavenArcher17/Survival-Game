// The Ancient forest's scenery, part 3 (cooks/biome-forest.md 2.4 #22-#36, 2.6, 2.11, 2.20; cooks/build-biome-forest.md
// 2.5): the Grovekin villages' sites (the Long-hall round the Grove Circle, the Cradle Wood, the Herb Terraces,
// Bread-stone Lane, the Root-Mine head, the Sunken Orchard, the Crystal Roots), the Elder Canopy's sites (the Rootgrave
// Fort, the Lantern Line, the Fern Stair, the Canopy Lift's foot, the Sky Larder, the Sinkroot), the three coastal camps,
// the newcomers' village and the five cave mouths. Everything in the actor's local space (the site's ground point, the
// site's yaw); the server keeps the collision (FBuild::Box / Tree / SegBox), every machine else draws.
//
// The story these tell without words (DC 2.20): the Grovekin grew everything and cut nothing (house-trees, pleached
// walls, root tables), left bread on stones, laid their dead in bark cradles the roots took, and fought the Lantern
// Mourners at Rootgrave; the newcomers came after with saws, nails and thatch. Grovekin places are empty and unlit
// (foxfire only); newcomer places are lived in and lit.

#include "WRForestSceneryKit.h"

namespace WRForestLook
{
	namespace WRForestVillImpl
	{
		inline FLinearColor StrawC() { return Rgb(201, 168, 90) * 0.5f; }
		inline FLinearColor LeafC(float Kf = 1.f) { return Rgb(78, 122, 58) * 0.45f * Kf; }
		inline FLinearColor SandC() { return Rgb(196, 178, 140) * 0.55f; }
		inline FLinearColor StoneC() { return Rgb(150, 142, 128) * 0.5f; }
		inline FLinearColor HeartwoodC() { return Rgb(150, 92, 40) * 0.45f; }
		inline FSurf Heartwood(float Kf = 1.f) { return FSurf::Wood(Rgb(150, 92, 40) * 0.45f * Kf); }
		inline FSurf Shingle(float Kf = 1.f) { return FSurf::Wood(Rgb(96, 78, 60) * 0.4f * Kf); }
		inline FLinearColor FoxfireLight() { return FLinearColor(FColor(127, 224, 122)); }
		inline FLinearColor WarmLight() { return FLinearColor(1.f, 0.6f, 0.28f); }

		/** A straw skep (a coiled-straw beehive dome) on a board: the Grovekin kept bees in the wall's boles. */
		inline void Skep(FBuild& B, const FVector& Base, float Scale)
		{
			B.K.Add(Timber(0.85f), FVector(70.f, 70.f, 6.f) * FVector(Scale, Scale, 1.f), Base + FVector(0, 0, 3.f));
			const float H = 62.f * Scale;
			B.M.Lathe(Base + FVector(0, 0, 6.f), FVector::UpVector, H, { FVector(0.f, 30.f * Scale, 30.f * Scale), FVector(0.3f, 32.f * Scale, 32.f * Scale), FVector(0.65f, 25.f * Scale, 25.f * Scale), FVector(0.88f, 14.f * Scale, 14.f * Scale), FVector(1.f, 2.f, 2.f) }, 12, StrawC(), 0.95f);
			// The coils that show it's wound straw, bound with bramble: a ring every 8 cm up the dome.
			const float Radii[5] = { 31.f, 31.5f, 28.f, 22.f, 13.f };
			for (int32 c = 0; c < 5; ++c)
			{
				TArray<FVector> Coil;
				for (int32 s = 0; s <= 16; ++s) Coil.Add(Base + FVector(0, 0, 6.f + H * (0.08f + 0.18f * c)) + FRotator(0.f, 22.5f * s, 0.f).Vector() * Radii[c] * Scale);
				B.M.Tube(Coil, 2.2f * Scale, 4, StrawC() * 0.85f, 0.95f);
			}
			// The flight hole at the foot, facing out.
			B.K.Add(Black(0.6f), FVector(4.f, 12.f * Scale, 5.f * Scale), Base + FVector(30.f * Scale, 0, 10.f));
		}

		/** A barrel: staves (a lathe), iron hoops. */
		inline void Barrel(FBuild& B, const FVector& Base, float Scale = 1.f)
		{
			const float H = 90.f * Scale;
			B.M.Lathe(Base, FVector::UpVector, H, { FVector(0.f, 26.f * Scale, 26.f * Scale), FVector(0.5f, 31.f * Scale, 31.f * Scale), FVector(1.f, 26.f * Scale, 26.f * Scale) }, 12, Rgb(110, 82, 56) * 0.45f, 0.8f);
			for (const float Zf : { 0.12f, 0.88f }) B.K.Disc(Iron(), Base + FVector(0, 0, H * Zf), FVector::UpVector, 57.f * Scale, 5.f);
			B.K.Disc(Timber(0.75f), Base + FVector(0, 0, H - 1.f), FVector::UpVector, 50.f * Scale, 3.f);
			B.Box(Base + FVector(0, 0, H * 0.5f), FVector(28.f * Scale, 28.f * Scale, H * 0.5f));
		}

		/** A split-rail fence from P0 to P1 (ground points): posts every 2.4 m, two rails, set into the ground. */
		inline void Fence(FBuild& B, const FVector& P0, const FVector& P1)
		{
			const float Len = FVector::Dist2D(P0, P1);
			const int32 Posts = FMath::Max(1, FMath::CeilToInt(Len / 240.f));
			TArray<FVector> Tops;
			for (int32 i = 0; i <= Posts; ++i)
			{
				const FVector Xy = FMath::Lerp(P0, P1, float(i) / Posts);
				const FVector Gp = B.At(float(Xy.X), float(Xy.Y));
				B.K.Rod(Timber(0.85f + 0.1f * B.Rand(i + 91)), Gp - FVector(0, 0, 30.f), Gp + FVector(0, 0, 115.f), 14.f, true);
				Tops.Add(Gp);
			}
			for (int32 i = 0; i < Posts; ++i)
			{
				for (const float Zr : { 50.f, 100.f }) B.K.Rod(Timber(0.9f), Tops[i] + FVector(0, 0, Zr), Tops[i + 1] + FVector(0, 0, Zr - 6.f), 9.f);
				B.SegBox(Tops[i] + FVector(0, 0, 60.f), Tops[i + 1] + FVector(0, 0, 60.f), 6.f, 60.f);
			}
		}

		/** A dry-stone field wall (the newcomers' fresh walls: square-cut, unmossed) from P0 to P1, about H tall. */
		inline void DryWall(FBuild& B, const FVector& P0, const FVector& P1, float H, float MossK)
		{
			const float Len = FVector::Dist2D(P0, P1);
			const FVector Dir = (P1 - P0).GetSafeNormal2D();
			const float Yaw = Dir.Rotation().Yaw;
			const int32 Stones = FMath::Max(1, FMath::RoundToInt(Len / 55.f));
			for (int32 i = 0; i < Stones; ++i)
			{
				const FVector Xy = P0 + Dir * (Len * (i + 0.5f) / Stones);
				const float Gz = B.G(float(Xy.X), float(Xy.Y));
				for (int32 c = 0; c * 28.f < H; ++c)
				{
					const float Jit = B.Rand(i * 7 + c) - 0.5f;
					const float Sz = 52.f + 10.f * Jit;
					B.K.Add(OldStone(0.8f + 0.25f * B.Rand(i * 13 + c)), FVector(Sz, 50.f - c * 4.f, 28.f), FVector(Xy.X, Xy.Y, Gz + 10.f + c * 27.f), FRotator(0.f, Yaw + 4.f * Jit, 3.f * Jit));
				}
				if (MossK > 0.f && B.Rand(i + 301) < MossK) B.K.Add(Moss(), FVector(40.f, 46.f, 3.f), FVector(Xy.X, Xy.Y, Gz + 10.f + H), FRotator(0.f, Yaw, 0.f));
				// The cap stones on edge, as a waller sets them.
				B.K.Add(OldStone(0.9f), FVector(14.f, 44.f, 26.f), FVector(Xy.X, Xy.Y, Gz + 22.f + H), FRotator(0.f, Yaw, 8.f * (B.Rand(i + 71) - 0.5f)));
			}
			const FVector Mid = (P0 + P1) * 0.5f;
			B.Box(FVector(Mid.X, Mid.Y, B.G(float(Mid.X), float(Mid.Y)) + H * 0.5f + 10.f), FVector(Len * 0.5f, 26.f, H * 0.5f + 12.f), FRotator(0.f, Yaw, 0.f));
		}

		/** A bark cradle (DC 2.20): a trough of folded bark the length of a newborn, bound to a root with two rope bands;
		 *  bGrown: the root has grown over it and moss filled it. Centre at its floor, along Yaw. */
		inline void Cradle(FBuild& B, const FVector& Centre, float Yaw, float Tilt, bool bGrown)
		{
			const FRotator R(Tilt, Yaw, 0.f);
			const auto Pt = [&](float X, float Y, float Z) { return Centre + R.RotateVector(FVector(X, Y, Z)); };
			const FSurf Sb = Bark(1.1f);
			B.K.Add(Sb, FVector(70.f, 26.f, 4.f), Pt(0, 0, 2.f), R);
			for (const float S : { -1.f, 1.f }) B.K.Add(Sb, FVector(70.f, 4.f, 18.f), Pt(0, S * 15.f, 10.f), FRotator(Tilt, Yaw, S * 18.f));    // the sides curl out, as bark does drying
			B.K.Add(Sb.Dim(0.9f), FVector(4.f, 30.f, 22.f), Pt(-34.f, 0, 11.f), R);                                                          // the head board, higher
			B.K.Add(Sb.Dim(0.9f), FVector(4.f, 28.f, 14.f), Pt(34.f, 0, 7.f), R);
			for (const float X : { -18.f, 18.f }) B.K.Add(Rope(), FVector(3.f, 36.f, 26.f), Pt(X, 0, 6.f), R);
			if (bGrown)
			{
				B.K.Add(Moss(1.1f), FVector(62.f, 22.f, 5.f), Pt(0, 0, 10.f), R);
				B.Limb(Pt(-60.f, 14.f, -6.f), Pt(0.f, -4.f, 26.f), 9.f, 7.f, RootC(), 6);
				B.Limb(Pt(0.f, -4.f, 26.f), Pt(58.f, -16.f, -8.f), 7.f, 5.f, RootC(), 6);
			}
		}

		/** A small orchard tree in code (an apple: a short crooked trunk, four or five limbs, leaf clouds): bFruit hangs
		 *  gold-red apples (the Idunn tree), bBare leaves it half dead; charms of woven straw on strings. */
		inline void AppleTree(FBuild& B, const FVector& Base, float Height, int32 Var, bool bFruit, bool bBare, bool bCharms)
		{
			const float Kh = Height / 500.f;
			const FVector Fork = Base + FVector(14.f * ((Var % 3) - 1), 10.f * ((Var % 2) * 2 - 1), 170.f * Kh);
			B.Limb(Base - FVector(0, 0, 20.f), Fork, 20.f * Kh, 14.f * Kh, BarkC() * 1.1f, 8);
			B.Box(Base + FVector(0, 0, 90.f * Kh), FVector(16.f * Kh, 16.f * Kh, 90.f * Kh));
			const int32 Limbs = 4 + Var % 2;
			for (int32 l = 0; l < Limbs; ++l)
			{
				const float Ang = 360.f * l / Limbs + 23.f * Var;
				const FVector Dir = FRotator(0.f, Ang, 0.f).Vector();
				const FVector Mid = Fork + Dir * 90.f * Kh + FVector(0, 0, 80.f * Kh);
				const FVector Tip = Fork + Dir * 180.f * Kh + FVector(0, 0, (130.f + 40.f * (l % 2)) * Kh);
				B.Limb(Fork, Mid, 11.f * Kh, 8.f * Kh, BarkC() * 1.05f, 7);
				B.Limb(Mid, Tip, 8.f * Kh, 3.f * Kh, BarkC() * 1.05f, 6);
				if (!bBare || l % 2 == 0)
				{
					for (int32 c = 0; c < 3; ++c)
					{
						const FVector Cl = FMath::Lerp(Mid, Tip, 0.4f + 0.3f * c) + FVector(0, 0, 30.f * Kh) + FRotator(0.f, Ang + 70.f * c, 0.f).Vector() * 25.f * Kh;
						B.Ball(Cl, (bBare ? 38.f : 58.f) * Kh, LeafC(0.85f + 0.12f * c), 0.9f, 0.7f);
					}
				}
				if (bFruit)
				{
					for (int32 a = 0; a < 5; ++a)
					{
						const FVector Ap = FMath::Lerp(Mid, Tip, 0.25f + 0.17f * a) + FRotator(0.f, Ang + 90.f * a, 0.f).Vector() * 45.f * Kh - FVector(0, 0, 18.f * Kh);
						// Gold blushing red: the apples of youth (Idunn's): bright enough to read against the leaves.
						B.Ball(Ap, 5.5f, a % 2 ? Rgb(214, 160, 40) * 0.7f : Rgb(190, 60, 30) * 0.65f, 0.45f, 1.f);
					}
				}
				if (bCharms && l % 2 == 1)
				{
					// A straw charm (a corn dolly: a plaited knot, a skirt of straw) on a string from the limb.
					const FVector Hang = FMath::Lerp(Mid, Tip, 0.6f);
					B.M.Tube({ Hang, Hang - FVector(0, 0, 45.f) }, 0.6f, 4, RopeC(), 0.95f);
					B.Ball(Hang - FVector(0, 0, 50.f), 5.f, StrawC(), 0.95f, 1.f);
					B.M.Lathe(Hang - FVector(0, 0, 80.f), FVector::UpVector, 26.f, { FVector(0.f, 9.f, 9.f), FVector(0.6f, 5.f, 5.f), FVector(1.f, 2.f, 2.f) }, 8, StrawC(), 0.95f);
				}
			}
		}

		/** A herb plant: a mound of small leaves with flower heads (sage, thyme, lavender, tansy, by Var). */
		inline void Herb(FBuild& B, const FVector& Base, int32 Var)
		{
			static const FColor Flowers[4] = { FColor(150, 120, 190), FColor(220, 200, 80), FColor(240, 236, 220), FColor(200, 110, 140) };
			B.Ball(Base + FVector(0, 0, 14.f), 22.f + 4.f * (Var % 3), LeafC(0.8f + 0.1f * (Var % 3)), 0.9f, 0.6f);
			for (int32 f = 0; f < 5; ++f)
			{
				const FVector Fp = Base + FRotator(0.f, 72.f * f + 13.f * Var, 0.f).Vector() * 12.f;
				B.Limb(Fp + FVector(0, 0, 10.f), Fp + FVector(0, 0, 38.f), 1.2f, 0.8f, LeafC(0.7f), 4);
				B.Ball(Fp + FVector(0, 0, 40.f), 3.f, FLinearColor(Flowers[Var % 4]) * 0.6f, 0.8f, 1.6f);
			}
		}

		/** An ore cart: a plank box on an iron-shod frame, four wheels; bGrown: roots have taken it into the wall. */
		inline void Cart(FBuild& B, const FVector& Base, float Yaw, bool bGrown)
		{
			const FRotator R(0.f, Yaw, bGrown ? 6.f : 0.f);
			const auto Pt = [&](float X, float Y, float Z) { return Base + R.RotateVector(FVector(X, Y, Z)); };
			for (const float Y : { -40.f, 40.f })
			{
				for (const float X : { -45.f, 45.f })
				{
					B.K.Disc(Timber(0.8f), Pt(X, Y * 1.15f, 26.f), R.RotateVector(FVector(0, 1, 0)), 52.f, 7.f);
					B.K.Disc(Iron(), Pt(X, Y * 1.18f, 26.f), R.RotateVector(FVector(0, 1, 0)), 54.f, 2.f);
				}
				B.K.Add(Timber(), FVector(150.f, 10.f, 10.f), Pt(0, Y, 30.f), R);
			}
			B.K.Add(Plank(0.85f), FVector(140.f, 84.f, 6.f), Pt(0, 0, 40.f), R);
			for (const float Y : { -42.f, 42.f }) B.K.Add(Plank(0.8f), FVector(140.f, 5.f, 50.f), Pt(0, Y, 66.f), R);
			for (const float X : { -70.f, 70.f }) B.K.Add(Plank(0.8f), FVector(5.f, 84.f, 50.f), Pt(X, 0, 66.f), R);
			for (const float X : { -70.f, 0.f, 70.f }) B.K.Add(Iron(), FVector(6.f, 90.f, 4.f), Pt(X, 0, 90.f), R);
			// The last load: dark iron ore, still in it.
			for (int32 o = 0; o < 6; ++o) B.Ball(Pt(-45.f + 18.f * o, 10.f * ((o % 3) - 1), 82.f), 14.f, Rgb(70, 58, 52) * 0.5f, 0.85f, 0.7f);
			B.Box(Pt(0, 0, 55.f), FVector(75.f, 48.f, 40.f), R);
			if (bGrown)
			{
				// Roots out of the wall over the box and through the wheels: it has stood here a long time.
				B.Root({ Pt(-130.f, -60.f, 180.f), Pt(-40.f, -30.f, 110.f), Pt(30.f, 10.f, 95.f), Pt(100.f, 60.f, 20.f), Pt(130.f, 80.f, -20.f) }, 16.f, 7.f, RootC(), 7);
				B.Root({ Pt(-110.f, 70.f, 200.f), Pt(-45.f, 50.f, 60.f), Pt(-45.f, 50.f, 10.f) }, 12.f, 6.f, RootC(), 6);
				B.K.Add(Moss(), FVector(120.f, 70.f, 4.f), Pt(0, 0, 92.f), R);
			}
		}

		/** A fern clump: arching fronds of flat leaf blades round a crown. */
		inline void Fern(FBuild& B, const FVector& Base, float Scale, int32 Var)
		{
			for (int32 f = 0; f < 7; ++f)
			{
				const float Ang = 360.f * f / 7.f + 17.f * Var;
				const FVector Dir = FRotator(0.f, Ang, 0.f).Vector();
				const FVector P0 = Base + FVector(0, 0, 4.f);
				const FVector P1 = Base + Dir * 40.f * Scale + FVector(0, 0, 55.f * Scale);
				const FVector P2 = Base + Dir * 85.f * Scale + FVector(0, 0, 30.f * Scale);
				B.K.Add(Leaf(0.95f + 0.1f * (f % 2)), FVector((P1 - P0).Size(), 22.f * Scale, 2.f), (P0 + P1) * 0.5f, (P1 - P0).Rotation());
				B.K.Add(Leaf(0.85f), FVector((P2 - P1).Size(), 16.f * Scale, 2.f), (P1 + P2) * 0.5f, (P2 - P1).Rotation());
			}
		}

		/** Ground torn apart by roots (the Sinkroot, DC 2.4 #35): sods levered up round a hole of Radius at Centre (the
		 *  cave's shaft or the terrain's pit below), the great roots that did it arching across and gripping both lips, and
		 *  a rope someone tied off to a root and went down on. */
		inline void Tear(FBuild& B, const FVector& Centre, float Radius)
		{
			const int32 Sods = 18;
			for (int32 s = 0; s < Sods; ++s)
			{
				const float Ang = 360.f * s / Sods + 7.f * B.Rand(s);
				const FVector Dir = FRotator(0.f, Ang, 0.f).Vector();
				const FVector Lip = Centre + Dir * Radius * (0.95f + 0.15f * B.Rand(s + 40));
				const float Gz = B.G(float(Lip.X), float(Lip.Y), float(Centre.Z));
				// A slab of turf and soil levered up toward the hole, its torn earth face showing under the grass.
				const float Lift = 25.f + 30.f * B.Rand(s + 3);
				const FRotator Tilt(-Lift * 0.6f, Ang, 0.f);
				B.K.Add(Turf(), FVector(150.f, 120.f, 10.f), FVector(Lip.X, Lip.Y, Gz + Lift + 20.f), Tilt);
				B.K.Add(Earth(0.9f), FVector(150.f, 116.f, 40.f), FVector(Lip.X, Lip.Y, Gz + Lift - 8.f), Tilt);
				B.Box(FVector(Lip.X, Lip.Y, Gz + Lift), FVector(75.f, 60.f, 25.f), Tilt);
				// Hair roots hanging out of the torn faces into the dark.
				if (s % 2 == 0) B.Limb(FVector(Lip.X, Lip.Y, Gz - 10.f) - Dir * 60.f, FVector(Lip.X, Lip.Y, Gz - 180.f) - Dir * 90.f, 3.f, 0.8f, RootC(), 4);
			}
			// Three great roots across the hole, each out of the ground on one lip, arching over and under the far lip: they
			// pulled the ground apart growing. Walkable: you can cross on them (and slip).
			for (int32 r = 0; r < 3; ++r)
			{
				const float Ang = 40.f + 120.f * r + 20.f * B.Rand(r + 9);
				const FVector Dir = FRotator(0.f, Ang, 0.f).Vector();
				const FVector Side = FRotator(0.f, Ang + 90.f, 0.f).Vector() * Radius * (0.25f * (r - 1));
				const FVector A0 = Centre + Side - Dir * Radius * 1.6f, A3 = Centre + Side + Dir * Radius * 1.6f;
				const FVector G0(A0.X, A0.Y, B.G(float(A0.X), float(A0.Y), float(Centre.Z)));
				const FVector G3(A3.X, A3.Y, B.G(float(A3.X), float(A3.Y), float(Centre.Z)));
				const FVector A1 = FMath::Lerp(G0, G3, 0.3f) + FVector(0, 0, 70.f), A2 = FMath::Lerp(G0, G3, 0.7f) + FVector(0, 0, 40.f);
				const float Rr = 55.f - 10.f * r;
				B.Root({ G0 - FVector(0, 0, 40.f), A1, A2, G3 - FVector(0, 0, 60.f) }, Rr, Rr * 0.7f, RootC() * (0.9f + 0.1f * r), 9);
				B.SegBox(A1, A2, Rr * 0.7f, Rr * 0.7f);
				B.K.Add(Moss(), FVector((A2 - A1).Size() * 0.8f, Rr * 1.2f, 4.f), (A1 + A2) * 0.5f + FVector(0, 0, Rr * 0.85f), (A2 - A1).Rotation());
			}
			// The rope: tied off round the first root, hanging down the hole. Someone went down. It's frayed at the lip.
			const FVector Tie = Centre + FVector(0, 0, 60.f) - FRotator(0.f, 40.f, 0.f).Vector() * Radius * 0.55f;
			B.M.Tube({ Tie, Tie + FVector(18.f, 0, -30.f), Tie - FVector(0, 0, 1200.f) }, 2.2f, 5, RopeC(), 0.95f);
			B.M.Tube({ Tie + FVector(-30.f, -30.f, 10.f), Tie + FVector(30.f, -30.f, 10.f), Tie + FVector(30.f, 30.f, 10.f), Tie + FVector(-30.f, 30.f, 10.f), Tie + FVector(-30.f, -30.f, 10.f) }, 2.4f, 5, RopeC(), 0.95f);
		}

		/** Where a cave mouth stands from the cave's origin (AWRCave: chamber +-900, tunnel out along +X). */
		inline float MouthX() { return WRTune(TEXT("forestWorld.caveMouthX"), 1150.f); }

		/** The dressing outside a cave's mouth (DC 2.6), the mouth at local (X0, 0) facing +X (AWRCave's tunnel runs out
		 *  along +X from its chamber; the rock mound itself is AWRCave's). */
		inline void Mouth(FBuild& B, FName Theme, float X0)
		{
			if (Theme == TEXT("badger_sett"))
			{
				// C1, the newcomers' first cave: a sett dug into a meadow hill. The badgers' spoil heaps of yellow sand fan
				// out from the entrance, raked with claw marks, old bedding (dry grass) dragged out to air, two side holes
				// in the bank, rabbit bones. Dry and friendly: nothing here warns you.
				for (int32 h = 0; h < 4; ++h)
				{
					const FVector Hp = B.At(X0 + 250.f + 160.f * h, (h % 2 ? 1.f : -1.f) * (180.f + 90.f * h));
					B.Ball(Hp, 160.f - 20.f * h, SandC(), 0.98f, 0.3f);
					for (int32 c = 0; c < 4; ++c) B.K.Add(Earth(0.7f), FVector(70.f, 4.f, 2.f), Hp + FVector(-30.f + 18.f * c, 0, 46.f - 10.f * h), FRotator(0.f, 75.f + 4.f * c, 0.f));
				}
				for (int32 s = 0; s < 6; ++s) B.K.Add(Straw(), FVector(40.f, 14.f, 4.f), B.At(X0 + 120.f + 50.f * s, -60.f + 25.f * s, 3.f), FRotator(0.f, 30.f * s, 0.f));
				for (const float Y : { -650.f, 600.f })
				{
					B.K.Disc(Black(0.4f), B.At(X0 - 150.f, Y, 40.f), FVector(1.f, Y > 0.f ? -0.3f : 0.3f, 0.2f), 60.f, 6.f);
					B.Ball(B.At(X0 - 60.f, Y), 70.f, SandC() * 0.9f, 0.98f, 0.3f);
				}
				for (int32 b = 0; b < 3; ++b) B.Limb(B.At(X0 + 400.f + 20.f * b, 140.f, 3.f), B.At(X0 + 430.f + 20.f * b, 160.f + 10.f * b, 3.f), 1.6f, 1.2f, BoneC(), 5);
				return;
			}
			if (Theme == TEXT("alder_grotto"))
			{
				// C2 is behind the Great Falls' curtain: the mouth is a wet ledge, dark basalt slick with spray, moss and
				// liverwort, ferns dripping. The falls' own sheet hides it; glowworm threads hang under the lip at night.
				for (int32 r = 0; r < 5; ++r) B.Rock(r, FVector(220.f + 60.f * r, 180.f, 120.f + 30.f * (r % 2)), B.At(X0 + 150.f + 90.f * r, (r % 2 ? 1.f : -1.f) * (260.f + 40.f * r)), FRotator(0.f, 40.f * r, 0.f), FLinearColor(0.07f, 0.075f, 0.08f), true);
				for (int32 m = 0; m < 6; ++m) B.K.Add(Moss(1.1f), FVector(140.f, 90.f, 5.f), B.At(X0 + 100.f + 70.f * m, (m % 2 ? 1.f : -1.f) * 200.f, 3.f), FRotator(0.f, 20.f * m, 4.f));
				for (int32 f = 0; f < 4; ++f) Fern(B, B.At(X0 + 200.f + 120.f * f, (f % 2 ? 1.f : -1.f) * 330.f), 0.9f, f);
				const float Gz = B.G(X0, 0.f);
				for (int32 g = 0; g < 14; ++g)
				{
					// A glowworm's fishing line: a silk thread beaded with sticky drops that shine blue-green.
					const FVector Top(X0 + 20.f + 22.f * g, -150.f + 23.f * g, Gz + 330.f);
					const float Len = 40.f + 60.f * B.Rand(g + 5);
					B.N.Add(FSurf::Lit(Rgb(120, 210, 230) * 0.6f, 0.9f), FVector(0.8f, 0.8f, Len), Top - FVector(0, 0, Len * 0.5f));
					B.N.Add(FSurf::Lit(Rgb(150, 230, 240) * 0.7f, 1.f), FVector(3.f), Top - FVector(0, 0, Len));
				}
				B.Lamp(FVector(X0 + 120.f, 0.f, Gz + 260.f), FLinearColor(0.45f, 0.8f, 0.9f), 600.f, 700.f);
				return;
			}
			if (Theme == TEXT("root_mine"))
			{
				// C3, the Grovekin mine: its portal is timbered with living roots, not sawn props (the pact: nothing cut).
				// Two root pillars trained up either side and a root lintel grafted between them; a rail of split heartwood
				// out of the mouth to the spoil heap; an ore cart grown into the wall; a windlass over the old shaft.
				const float Gz = B.G(X0, 0.f);
				for (const float Y : { -190.f, 190.f })
				{
					B.Root({ FVector(X0 + 40.f, Y * 1.3f, Gz - 30.f), FVector(X0, Y, Gz + 80.f), FVector(X0 - 10.f, Y * 0.98f, Gz + 260.f), FVector(X0 - 20.f, Y * 0.8f, Gz + 330.f) }, 34.f, 24.f, RootC(), 9);
					B.Box(FVector(X0, Y, Gz + 160.f), FVector(30.f, 30.f, 170.f));
				}
				B.Root({ FVector(X0 - 20.f, -170.f, Gz + 325.f), FVector(X0, -60.f, Gz + 350.f), FVector(X0, 60.f, Gz + 350.f), FVector(X0 - 20.f, 170.f, Gz + 325.f) }, 28.f, 28.f, RootC() * 1.05f, 9);
				// Side roots braced into the rock face like props.
				for (int32 b = 0; b < 4; ++b)
				{
					const float Sy = b < 2 ? -1.f : 1.f;
					B.Limb(FVector(X0 - 120.f, Sy * (260.f + 60.f * (b % 2)), Gz + 380.f), FVector(X0 + 30.f, Sy * 200.f, Gz + 120.f * (b % 2)), 16.f, 10.f, RootC() * 0.9f, 7);
				}
				// The rails: sleepers and two rails of split heartwood out 12 m to the spoil heap.
				for (int32 s = 0; s < 20; ++s) B.K.Add(Timber(0.75f), FVector(24.f, 130.f, 12.f), B.At(X0 - 300.f + 70.f * s, 0.f, 4.f));
				for (const float Y : { -45.f, 45.f })
				{
					for (int32 s = 0; s < 6; ++s) B.K.Rod(Heartwood(0.8f), B.At(X0 - 300.f + 230.f * s, Y, 14.f), B.At(X0 - 70.f + 230.f * s, Y, 14.f), 9.f, true);
				}
				for (int32 r = 0; r < 6; ++r) B.Rock(r, FVector(160.f, 140.f, 70.f + 10.f * r), B.At(X0 + 1100.f + 70.f * (r % 3), -220.f + 80.f * r), FRotator(0.f, 30.f * r, 0.f), FLinearColor(0.14f, 0.13f, 0.12f), r % 2 == 0, 20.f);
				Cart(B, B.At(X0 - 60.f, 330.f), 90.f, true);
				// The windlass over the old shaft beside the portal: two posts, a drum and its crank; planks over the shaft.
				const FVector Wl = B.At(X0 + 400.f, -520.f);
				for (const float Y : { -90.f, 90.f }) B.K.Rod(Timber(0.8f), Wl + FVector(0, Y, -20.f), Wl + FVector(0, Y, 140.f), 16.f, true);
				B.K.Rod(Timber(0.85f), Wl + FVector(0, -90.f, 120.f), Wl + FVector(0, 90.f, 120.f), 26.f);
				B.K.Rod(Iron(), Wl + FVector(0, 90.f, 120.f), Wl + FVector(0, 120.f, 120.f), 4.f);
				B.K.Rod(Iron(), Wl + FVector(0, 120.f, 120.f), Wl + FVector(0, 120.f, 160.f), 4.f);
				for (int32 p = 0; p < 8; ++p) B.K.Add(Timber(0.7f), FVector(200.f, 18.f, 10.f), Wl + FVector(0, -90.f + 26.f * p, -6.f));
				B.Box(Wl + FVector(0, 0, -6.f), FVector(100.f, 110.f, 8.f));
				// Foxfire in the old root timbers at night: the only light the Grovekin left.
				for (int32 f = 0; f < 5; ++f) B.N.Add(Foxfire(0.85f), FVector(30.f, 6.f, 50.f), FVector(X0 + 8.f, (f % 2 ? 1.f : -1.f) * 205.f, Gz + 60.f + 55.f * f));
				B.Lamp(FVector(X0 + 60.f, 0.f, Gz + 200.f), FoxfireLight(), 350.f, 600.f);
				return;
			}
			if (Theme == TEXT("foxfire_cellar"))
			{
				// C4 in the Clear-cut's edge: its ceiling is a mat of foxfire and it spills out over the lip: rotten wood and
				// fungus mat glowing green at night, bootlace laces hanging out of the roof like black string.
				const float Gz = B.G(X0, 0.f);
				for (int32 l = 0; l < 5; ++l)
				{
					const FVector P0 = B.At(X0 - 100.f + 60.f * l, -300.f + 150.f * l, 10.f);
					B.Log(TEXT("foxfire_log"), l % 4, P0, P0 + FRotator(0.f, 70.f + 40.f * l, 0.f).Vector() * (260.f + 50.f * l), 28.f + 6.f * (l % 2), l % 2 == 0);
				}
				for (int32 m = 0; m < 12; ++m)
				{
					const FVector Mp = B.At(X0 + 40.f + 60.f * (m % 4), -260.f + 47.f * m, 4.f);
					B.K.Add(Bark(0.7f), FVector(110.f, 80.f, 8.f), Mp, FRotator(0.f, 30.f * m, 0.f));
					B.N.Add(Foxfire(0.6f + 0.1f * (m % 3)), FVector(90.f, 60.f, 2.f), Mp + FVector(0, 0, 5.f), FRotator(0.f, 30.f * m, 0.f));
				}
				for (int32 r = 0; r < 9; ++r) B.Limb(FVector(X0 - 60.f + 10.f * r, -200.f + 50.f * r, Gz + 340.f), FVector(X0 - 40.f + 10.f * r, -205.f + 50.f * r, Gz + 220.f - 80.f * B.Rand(r)), 2.5f, 1.5f, BlackC(), 5);
				B.Lamp(FVector(X0 + 120.f, 0.f, Gz + 100.f), FoxfireLight(), 1400.f, 900.f);
				return;
			}
			if (Theme == TEXT("sinkroot"))
			{
				// C5 is a fall, not a walk-in: the ground torn open at its mouth.
				Tear(B, B.At(X0, 0.f), WRTune(TEXT("forestWorld.sinkrootRadius"), 520.f));
				B.Lamp(B.At(X0, 0.f, -300.f), FoxfireLight(), 700.f, 900.f);
				return;
			}
			// Any other cave in the forest: a mossy rock lip and ferns.
			for (int32 r = 0; r < 3; ++r) B.Rock(r, FVector(200.f, 160.f, 110.f), B.At(X0 + 150.f, (r - 1) * 300.f), FRotator(0.f, 50.f * r, 0.f), FLinearColor(0.13f, 0.13f, 0.12f), true);
			for (int32 f = 0; f < 3; ++f) Fern(B, B.At(X0 + 250.f, (f - 1) * 260.f), 0.9f, f);
		}

		/** A newcomer lantern on a post, lit at night. */
		inline void LanternPost(FBuild& B, const FVector& Base)
		{
			B.K.Rod(Timber(0.85f), Base - FVector(0, 0, 40.f), Base + FVector(0, 0, 260.f), 16.f, true);
			B.K.Add(Timber(0.8f), FVector(70.f, 8.f, 10.f), Base + FVector(30.f, 0, 250.f));
			B.K.Rod(Iron(), Base + FVector(62.f, 0, 245.f), Base + FVector(62.f, 0, 225.f), 1.5f);
			B.K.Add(Bronze(), FVector(26.f, 26.f, 6.f), Base + FVector(62.f, 0, 222.f));
			B.K.Add(FSurf::Glass(Rgb(200, 170, 110) * 0.4f), FVector(22.f, 22.f, 30.f), Base + FVector(62.f, 0, 204.f));
			B.N.Add(Window(1.f), FVector(12.f, 12.f, 20.f), Base + FVector(62.f, 0, 204.f));
			B.K.Add(Bronze(), FVector(26.f, 26.f, 5.f), Base + FVector(62.f, 0, 187.f));
			B.Lamp(Base + FVector(62.f, 0, 200.f), WarmLight(), WRTune(TEXT("forestWorld.lanternLight"), 1800.f), 900.f);
			B.Box(Base + FVector(0, 0, 110.f), FVector(10.f, 10.f, 150.f));
		}

		/** A wattle hurdle (woven withies on stakes) from P0 to P1 (ground points), H tall: the Grovekin's walls. */
		inline void Hurdle(FBuild& B, const FVector& P0, const FVector& P1, float H)
		{
			const float Len = FVector::Dist2D(P0, P1);
			if (Len < 30.f) return;
			const int32 Stakes = FMath::Max(2, FMath::RoundToInt(Len / 45.f));
			const FVector Across = FVector::CrossProduct((P1 - P0).GetSafeNormal2D(), FVector::UpVector);
			for (int32 s = 0; s <= Stakes; ++s)
			{
				const FVector Sp = FMath::Lerp(P0, P1, float(s) / Stakes);
				B.M.Lathe(Sp - FVector(0, 0, 20.f), FVector::UpVector, H + 20.f, { FVector(0.f, 4.f, 4.f), FVector(1.f, 3.f, 3.f) }, 5, BarkC(), 0.9f, 0.f, float(s));
			}
			const int32 Courses = FMath::Max(1, int32(H / 18.f));
			for (int32 c = 0; c < Courses; ++c)
			{
				TArray<FVector> Weave;
				for (int32 s = 0; s <= Stakes; ++s) Weave.Add(FMath::Lerp(P0, P1, float(s) / Stakes) + FVector(0, 0, 12.f + 18.f * c) + Across * (((s + c) % 2) ? 4.f : -4.f));
				B.M.Tube(Weave, 2.4f, 4, RootC() * (0.9f + 0.1f * (c % 2)), 0.9f);
			}
			B.SegBox(P0 + FVector(0, 0, H * 0.5f), P1 + FVector(0, 0, H * 0.5f), 8.f, H * 0.5f);
		}

		/** A bread stone by a Grovekin door (DC 2.20: they left bread for the forest): a flat stone with a loaf carved on it. */
		inline void BreadStone(FBuild& B, const FVector& Ground, float Yaw)
		{
			B.K.Add(OldStone(0.95f), FVector(70.f, 50.f, 22.f), Ground + FVector(0, 0, 6.f), FRotator(0.f, Yaw, 1.5f));
			B.Ball(Ground + FVector(0, 0, 22.f), 16.f, StoneC() * 1.05f, 0.8f, 0.45f);
			for (int32 s = 0; s < 3; ++s) B.K.Add(OldStone(0.6f), FVector(3.f, 18.f, 2.f), Ground + FVector(0, 0, 29.f) + FRotator(0.f, Yaw, 0.f).RotateVector(FVector(-8.f + 8.f * s, 0, 0)), FRotator(0.f, Yaw + 20.f, 0.f));
			B.K.Add(Moss(0.9f), FVector(30.f, 50.f, 2.f), Ground + FVector(0, 0, 17.f) + FRotator(0.f, Yaw, 0.f).RotateVector(FVector(-25.f, 0, 0)), FRotator(0.f, Yaw, 0.f));
			B.Box(Ground + FVector(0, 0, 8.f), FVector(35.f, 25.f, 11.f), FRotator(0.f, Yaw, 0.f));
		}

		/** A giant of the tree kit standing at Base (local ground) at Height, with its full trunk collision (the kit's capsules). */
		inline void Giant(FBuild& B, int32 Var, const FVector& Base, float Height)
		{
			const FRotator R(0.f, 37.f * Var, 0.f);
			B.Tree(TEXT("giant"), Var, Base, Height, R, 0.f);
			const float Ref = FMath::Max(1.f, WRTreeKit::ReferenceHeight(TEXT("giant")));
			WRTreeKit::AddCollision(B.Act, TEXT("giant"), Var, FTransform(R, Base, FVector(Height / Ref)) * B.T);
		}

		/** A square-cut heartwood post with steel pins at its head (the Heartwood lodge set's joint). */
		inline void LodgePost(FBuild& B, const FVector& Foot, float H, float Yaw)
		{
			B.K.Add(Heartwood(0.95f), FVector(30.f, 30.f, H), Foot + FVector(0, 0, H * 0.5f), FRotator(0.f, Yaw, 0.f));
			for (const float Zp : { H - 18.f, H - 46.f }) B.K.Disc(Iron(), Foot + FVector(0, 0, Zp) + FRotator(0.f, Yaw + 90.f, 0.f).Vector() * 16.f, FRotator(0.f, Yaw + 90.f, 0.f).Vector(), 6.f, 3.f);
		}
	}
}

// ================================================================================================ the Grovekin villages

void WRForestLook::FBuild::LongHall()
{
	using namespace WRForestVillImpl;
	// DC 2.4 #23: the Grovekin Long-hall, a hall GROWN, not built: eight trees in a ring whose crowns were trained in and
	// grafted to meet as one roof. The Great Shrine (the Grove Circle: nine pleached limes round a stone bowl) stands in
	// its middle, so the hall's centre stays clear 8 m round (the kit's ShrineGroveCircle draws it; Rowan works there by
	// day). A cold hearth, carved hulder posts at the gate, long tables of living root. The way in faces +X.
	const float Ring = WRTune(TEXT("forestWorld.longHall.radius"), 1500.f);
	const float TreeH = WRTune(TEXT("forestWorld.longHall.treeHeight"), 2600.f);
	const float Z0 = G(0.f, 0.f);
	const FVector Apex(0.f, 0.f, Z0 + 2350.f);
	FVector Bases[8], Tops[8];
	FRotator Leans[8];
	for (int32 i = 0; i < 8; ++i)
	{
		// The trees stand at 22.5 + 45 i: +X falls between two of them, the gate.
		const float Ang = 22.5f + 45.f * i;
		const FVector Dir = FRotator(0.f, Ang, 0.f).Vector();
		Bases[i] = At(float(Dir.X) * Ring, float(Dir.Y) * Ring, -10.f);
		// Each leans 7 degrees in (pitch tips its top toward the middle): two hundred years of training.
		Leans[i] = FRotator(7.f, Ang, 0.f);
		Tree(i % 2 ? FName(TEXT("lime")) : FName(TEXT("beech")), i % 4, Bases[i], TreeH, Leans[i], 900.f);
		Tops[i] = Bases[i] + Leans[i].RotateVector(FVector(0, 0, 1750.f));
	}
	for (int32 i = 0; i < 8; ++i)
	{
		const int32 j = (i + 1) % 8;
		// The trained limbs: each tree sends two up and in to the apex, where they've grafted into one knot.
		const FVector Mid = FMath::Lerp(Tops[i], Apex, 0.5f) + FVector(0, 0, 160.f);
		Limb(Tops[i], Mid, 30.f, 22.f, BarkC(), 8);
		Limb(Mid, Apex + FRotator(0.f, 45.f * i, 0.f).Vector() * 60.f, 22.f, 12.f, BarkC(), 8);
		const FVector Mid2 = FMath::Lerp(Tops[i], Tops[j], 0.5f) + FVector(0, 0, 120.f);
		Limb(Tops[i] - FVector(0, 0, 200.f), Mid2, 18.f, 14.f, BarkC() * 1.05f, 7);
		Limb(Mid2, Apex + FRotator(0.f, 45.f * i + 22.5f, 0.f).Vector() * 90.f, 14.f, 8.f, BarkC() * 1.05f, 7);
		// The ring beam: branches of neighbours grafted into one arch round the top of the walls.
		TArray<FVector> Beam;
		for (int32 s = 0; s <= 6; ++s) Beam.Add(Arch(Tops[i] - FVector(0, 0, 300.f), Tops[j] - FVector(0, 0, 300.f), s / 6.f, 120.f));
		Root(Beam, 20.f, 20.f, BarkC(), 7);
		// Pleaching: the lower branches of each pair crossed and woven into a lattice over the wattle.
		Limb(Bases[i] + Leans[i].RotateVector(FVector(0, 0, 500.f)), Bases[j] + Leans[j].RotateVector(FVector(0, 0, 1200.f)), 11.f, 7.f, BarkC(), 6);
		Limb(Bases[j] + Leans[j].RotateVector(FVector(0, 0, 500.f)), Bases[i] + Leans[i].RotateVector(FVector(0, 0, 1200.f)), 11.f, 7.f, BarkC(), 6);
		if (i != 7)
		{
			// The walls between the trunks: wattle hurdles 2.1 m, the roof's shade above. The gap between 7 and 0 is the gate.
			const FVector Dir = (Bases[j] - Bases[i]).GetSafeNormal2D();
			Hurdle(*this, Bases[i] + Dir * 110.f, Bases[j] - Dir * 110.f, 210.f);
		}
	}
	// The roof: the trained crowns' leaves, thick as thatch, sloping from the eaves (17.5 m) to the knot (23.5 m) with a
	// smoke hole over the middle. Slabs run outward and down from the hole.
	const float ZEave = Z0 + 1700.f;
	const float Inner = 220.f, Outer = Ring + 300.f;
	const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(Apex.Z - ZEave, Ring));
	for (int32 s = 0; s < 24; ++s)
	{
		const float Ang = 15.f * s + 7.5f;
		const FVector Dir = FRotator(0.f, Ang, 0.f).Vector();
		const float Rm = (Inner + Outer) * 0.5f;
		const float Zm = float(Apex.Z) - (float(Apex.Z) - ZEave) * (Rm / Ring);
		const float SlopeLen = FMath::Sqrt(FMath::Square(Outer - Inner) + FMath::Square((float(Apex.Z) - ZEave) * (Outer - Inner) / Ring));
		const float Chord = 2.f * Outer * FMath::Sin(FMath::DegreesToRadians(7.5f)) * 1.1f;
		const FRotator Slope(-Pitch, Ang, 0.f);
		K.Add(Leaf(0.8f + 0.25f * FMath::Frac(Ang * 0.13f)), FVector(SlopeLen, Chord, 34.f), Dir * Rm + FVector(0, 0, Zm + 30.f), Slope);
		K.Add(DeepMoss(0.9f), FVector(SlopeLen, Chord * 0.95f, 10.f), Dir * Rm + FVector(0, 0, Zm + 6.f), Slope);
		// Leaf clouds bulging over the slabs: it's a living crown, not thatch.
		if (s % 2 == 0) Ball(Dir * (Rm + 200.f) + FVector(0, 0, Zm + 120.f), 210.f, LeafC(0.9f + 0.1f * (s % 4)), 0.9f, 0.55f);
	}
	// The hulder posts flank the gate, and two stand inside by the hearth (DC 2.20: at every gate, the Grovekin's guardians).
	for (const float S : { -1.f, 1.f })
	{
		const FVector Gp = At(Ring * 0.98f, S * Ring * 0.24f);
		Hulder(Gp, S * 10.f, 1.f);
		Hulder(At(-1250.f, S * 380.f), 0.f, 1.f);
	}
	// The hearth, cold a long time: a kerb of stones, a bed of old ash, two charred logs and iron dogs.
	const FVector Hc = At(-1000.f, 0.f);
	for (int32 s = 0; s < 10; ++s)
	{
		const float Ang = 36.f * s;
		K.Add(OldStone(0.75f + 0.1f * (s % 3)), FVector(52.f, 34.f, 26.f), Hc + FRotator(0.f, Ang, 0.f).Vector() * 115.f + FVector(0, 0, 9.f), FRotator(0.f, Ang + 90.f, 0.f));
	}
	K.Disc(Black(0.45f), Hc + FVector(0, 0, 3.f), FVector::UpVector, 200.f, 4.f);
	K.Disc(FSurf::Stone(Rgb(120, 116, 110) * 0.5f, 0.98f), Hc + FVector(0, 0, 6.f), FVector::UpVector, 120.f, 3.f);   // pale ash
	Limb(Hc + FVector(-60.f, -40.f, 12.f), Hc + FVector(60.f, 30.f, 16.f), 12.f, 9.f, BlackC(), 7);
	Limb(Hc + FVector(-50.f, 45.f, 14.f), Hc + FVector(55.f, -35.f, 22.f), 11.f, 8.f, BlackC(), 7);
	for (const float Y : { -55.f, 55.f })
	{
		K.Rod(Iron(), Hc + FVector(-40.f, Y, 4.f), Hc + FVector(40.f, Y, 4.f), 4.f, true);
		K.Rod(Iron(), Hc + FVector(40.f, Y, 4.f), Hc + FVector(46.f, Y, 40.f), 4.f, true);
	}
	Box(Hc + FVector(0, 0, 13.f), FVector(130.f, 130.f, 13.f));
	// The long tables: roots led along the floor and up, grown flat on top and polished by elbows; root benches either side.
	for (const float S : { -1.f, 1.f })
	{
		const float Y = S * 1000.f;
		const float Tz = G(50.f, Y) + 78.f;
		Root({ At(-650.f, Y, -20.f), FVector(-470.f, Y, Tz - 18.f), FVector(570.f, Y, Tz - 18.f), At(750.f, Y, -20.f) }, 24.f, 20.f, RootC(), 8);
		for (const float X : { -200.f, 250.f }) Limb(At(X, Y, -15.f), FVector(X + 20.f, Y, Tz - 20.f), 18.f, 14.f, RootC(), 7);
		K.Add(Carved(0.8f), FVector(1040.f, 72.f, 10.f), FVector(50.f, Y, Tz));
		Box(FVector(50.f, Y, Tz - 38.f), FVector(520.f, 36.f, 43.f));
		for (const float Off : { -80.f, 80.f })
		{
			const float Bz = G(50.f, Y + Off) + 44.f;
			Root({ At(-560.f, Y + Off, -20.f), FVector(-420.f, Y + Off, Bz - 12.f), FVector(520.f, Y + Off, Bz - 12.f), At(660.f, Y + Off, -20.f) }, 15.f, 13.f, RootC() * 0.95f, 7);
			K.Add(Carved(0.75f), FVector(940.f, 30.f, 8.f), FVector(50.f, Y + Off, Bz));
			Box(FVector(50.f, Y + Off, Bz - 22.f), FVector(470.f, 16.f, 24.f));
		}
		// Wooden bowls and a cup left out, as if they meant to come back.
		for (int32 b = 0; b < 4; ++b) K.Disc(Carved(0.7f), FVector(-350.f + 260.f * b, Y + 10.f * (b % 2), Tz + 8.f), FVector::UpVector, 24.f, 7.f);
	}
	// Flagstones from the gate to the shrine's ring (a path worn by feet; moss between).
	for (int32 f = 0; f < 10; ++f)
	{
		const float X = Ring + 260.f - 95.f * f;
		if (X < 850.f) break;
		K.Add(OldStone(0.85f + 0.1f * (f % 3)), FVector(80.f, 120.f - 10.f * (f % 2), 10.f), At(X, 15.f * ((f % 3) - 1), 3.f), FRotator(0.f, 6.f * ((f % 3) - 1), 0.f));
		K.Add(Moss(), FVector(12.f, 110.f, 3.f), At(X - 48.f, 0.f, 4.f));
	}
	// At night: foxfire in the old trained wood of the roof, a faint green glow over the shrine's fire.
	for (int32 i = 0; i < 8; i += 2) N.Add(Foxfire(0.7f), FVector(60.f, 10.f, 10.f), FMath::Lerp(Tops[i], Apex, 0.35f), (Apex - Tops[i]).Rotation());
	Lamp(FVector(0.f, 0.f, Z0 + 1900.f), FoxfireLight(), WRTune(TEXT("forestWorld.foxfireLight"), 500.f), 1800.f);
}

void WRForestLook::FBuild::CradleWood()
{
	using namespace WRForestVillImpl;
	// DC 2.4 #24 and 2.20: where the Grovekin laid their dead. Beeches with their roots spread over the ground, hundreds of
	// small bark cradles bound to the roots, all empty; some grown over. The roots are fat where the cradles are: the
	// trees took them. Calm, not horror. The lore cradle is the cache at (150, 80): the trees stand round it.
	const float Ring = WRTune(TEXT("forestWorld.cradleWood.radius"), 1500.f);
	int32 Count = 0;
	for (int32 t = 0; t < 6; ++t)
	{
		const float Ang = 30.f + 60.f * t + 12.f * (Rand(t) - 0.5f);
		const FVector Tb = At(FRotator(0.f, Ang, 0.f).Vector().X * Ring, FRotator(0.f, Ang, 0.f).Vector().Y * Ring, -10.f);
		const float H = 2400.f + 400.f * Rand(t + 20);
		Tree(TEXT("beech"), t % 4, Tb, H, FRotator(0.f, 70.f * t, 0.f));
		const float Tr = FMath::Max(50.f, WRForestShape::TrunkRadius(TEXT("beech"), H));
		const int32 Roots = 8;
		for (int32 r = 0; r < Roots; ++r)
		{
			const float Ra = 360.f * r / Roots + 30.f * Rand(t * 10 + r);
			const FVector Dir = FRotator(0.f, Ra, 0.f).Vector();
			const float Len = 650.f + 300.f * Rand(t * 17 + r);
			FVector Pts[4];
			const float Ds[4] = { Tr * 0.8f, Len * 0.35f, Len * 0.7f, Len };
			const float Hs[4] = { 30.f, 28.f, 14.f, -25.f };
			for (int32 p = 0; p < 4; ++p)
			{
				const FVector Xy = Tb + Dir * Ds[p] + FRotator(0.f, Ra + 90.f, 0.f).Vector() * 60.f * FMath::Sin(p * 1.7f + r);
				Pts[p] = At(float(Xy.X), float(Xy.Y), Hs[p]);
			}
			Root({ Pts[0], Pts[1], Pts[2], Pts[3] }, 42.f, 12.f, RootC() * (0.92f + 0.08f * (r % 2)), 8);
			K.Add(Moss(0.9f), FVector(Ds[2] - Ds[0], 40.f, 4.f), (Pts[0] + Pts[2]) * 0.5f + FVector(0, 0, 32.f), (Pts[2] - Pts[0]).Rotation());
			// The cradles along the root, each where the root swells: the root grew fat on what lay in them.
			for (int32 c = 0; c < 4; ++c)
			{
				const float Tc = 0.22f + 0.17f * c;
				const FVector OnRoot = Tc < 0.5f ? FMath::Lerp(Pts[0], Pts[1], Tc * 2.f) : FMath::Lerp(Pts[1], Pts[2], (Tc - 0.5f) * 2.f);
				const float Sd = (c % 2 ? 1.f : -1.f);
				const FVector Side = FRotator(0.f, Ra + 90.f, 0.f).Vector() * Sd * 48.f;
				Ball(OnRoot + FVector(0, 0, 4.f), 46.f - 6.f * c, RootC() * 0.95f, 0.85f, 0.7f);
				const FVector Cp = OnRoot + Side;
				Cradle(*this, FVector(Cp.X, Cp.Y, G(float(Cp.X), float(Cp.Y), float(OnRoot.Z)) + 6.f), Ra + 8.f * Sd, 4.f * Sd, (t + r + c) % 3 == 0);
				++Count;
			}
		}
		// The oldest cradles high on the buttresses, nearly bark themselves.
		for (int32 c = 0; c < 4; ++c)
		{
			const FVector Dir = FRotator(0.f, 90.f * c + 45.f, 0.f).Vector();
			Cradle(*this, Tb + Dir * (Tr + 20.f) + FVector(0, 0, 90.f + 60.f * c), 90.f * c + 135.f, -35.f, true);
			++Count;
		}
		// Foxfire on the fattest root of each tree at night.
		N.Add(Foxfire(0.6f), FVector(140.f, 18.f, 6.f), Tb + FRotator(0.f, 45.f * t, 0.f).Vector() * (Tr + 150.f) + FVector(0, 0, 40.f), FRotator(0.f, 45.f * t, 0.f));
	}
	// A last few on stray roots in the open between the trees, round the cache's cradle.
	for (int32 c = 0; c < 6; ++c)
	{
		const FVector Cp = At(-300.f + 120.f * c, -260.f + 60.f * (c % 3));
		Limb(Cp + FVector(-90.f, -30.f, -10.f), Cp + FVector(90.f, 30.f, 10.f), 22.f, 14.f, RootC(), 7);
		Cradle(*this, Cp + FVector(0, 40.f, 6.f), 18.f, 3.f, c % 2 == 0);
		++Count;
	}
	Lamp(At(0.f, 0.f, 60.f), FoxfireLight(), WRTune(TEXT("forestWorld.foxfireLight"), 500.f), 1600.f);
	UE_LOG(LogWildreach, Verbose, TEXT("FOREST cradle wood: %d cradles"), Count);
}

void WRForestLook::FBuild::HerbTerraces()
{
	using namespace WRForestVillImpl;
	// DC 2.4 #25: dry-stone terraces of wild herbs climbing the slope (+Y), an apiary wall of skeps in bee boles (the
	// honey node at (-700, -600) is its hive: smoke it first), a copper still gone green. The Grovekin's garden: Rowan's
	// own garden copies it (DC 2.20). Herbs x20 in the cache at (150, 80), in front of the first wall.
	const float Zf = G(100.f, 120.f);
	float LastTop = Zf;
	for (int32 i = 0; i < 4; ++i)
	{
		const float Y = 280.f + 300.f * i;
		float Gmax = -1e9f, Gmin = 1e9f;
		for (int32 s = 0; s < 5; ++s) { const float Gs = G(-500.f + 300.f * s, Y); Gmax = FMath::Max(Gmax, Gs); Gmin = FMath::Min(Gmin, Gs); }
		const float Top = FMath::Max(LastTop + 55.f, Gmax + 30.f);
		LastTop = Top;
		// The retaining wall along the terrace's front, stone on stone, from the ground below up to its top.
		for (int32 s = 0; s < 20; ++s)
		{
			const float X = -540.f + 60.f * s;
			const float Gw = G(X, Y - 140.f);
			const float Hh = FMath::Max(30.f, Top - Gw + 10.f);
			for (int32 c = 0; c * 26.f < Hh; ++c)
			{
				const float Ch = FMath::Min(26.f, Hh - c * 26.f);
				K.Add(OldStone(0.75f + 0.25f * FMath::Frac(X * 0.031f + c * 0.37f)), FVector(56.f, 40.f, Ch), FVector(X + 8.f * (c % 2), Y - 140.f, Gw - 10.f + c * 26.f + Ch * 0.5f), FRotator(0.f, 3.f * ((s + c) % 3 - 1), 0.f));
			}
			if ((s + i) % 4 == 0) K.Add(Moss(), FVector(40.f, 44.f, 3.f), FVector(X, Y - 140.f, Top + 2.f));
		}
		K.Add(Earth(0.85f), FVector(1200.f, 270.f, 14.f), FVector(60.f, Y, Top - 6.f));
		Box(FVector(60.f, Y, (Top + Gmin) * 0.5f - 5.f), FVector(600.f, 140.f, FMath::Max(10.f, (Top - Gmin) * 0.5f + 5.f)));
		// Two rows of herbs gone wild on each terrace (sage, thyme, lavender, tansy).
		for (int32 h = 0; h < 18; ++h) Herb(*this, FVector(-480.f + 115.f * (h % 9) + 30.f * (h / 9), Y - 60.f + 120.f * (h / 9), Top), h + i);
		// The steps at the west end, one flight per terrace.
		for (int32 s = 0; s < 3; ++s) { K.Add(OldStone(0.9f), FVector(80.f, 90.f, 20.f), FVector(-600.f, Y - 230.f + 45.f * s, Top - 55.f + 20.f * s)); Box(FVector(-600.f, Y - 230.f + 45.f * s, Top - 55.f + 20.f * s), FVector(40.f, 45.f, 10.f)); }
	}
	// The apiary wall: a free-standing dry wall with three bee boles (niches) and a straw skep in each, two more on a
	// bench in front. Bees work it all day (the honey node is here).
	const FVector Ap = At(-700.f, -600.f);
	for (int32 s = 0; s < 8; ++s)
	{
		for (int32 c = 0; c < 6; ++c)
		{
			const bool bBole = (c == 2 || c == 3) && (s == 1 || s == 4 || s == 6);
			if (bBole) continue;
			K.Add(OldStone(0.8f + 0.2f * FMath::Frac(s * 0.41f + c * 0.29f)), FVector(52.f, 60.f, 28.f), Ap + FVector(-200.f + 55.f * s + 10.f * (c % 2), 0.f, 14.f + 28.f * c));
		}
	}
	K.Add(OldStone(0.95f), FVector(460.f, 70.f, 12.f), Ap + FVector(0, 0, 176.f));
	for (const float X : { -145.f, 20.f, 130.f })
	{
		K.Add(Black(0.3f), FVector(48.f, 30.f, 52.f), Ap + FVector(X, 18.f, 84.f));
		Skep(*this, Ap + FVector(X, -4.f, 58.f), 0.8f);
	}
	Box(Ap + FVector(0, 0, 90.f), FVector(225.f, 32.f, 92.f));
	for (const float X : { -80.f, 80.f }) { K.Add(Timber(0.8f), FVector(70.f, 50.f, 8.f), Ap + FVector(X, -110.f, 45.f)); K.Rod(Timber(0.75f), Ap + FVector(X, -110.f, 0.f), Ap + FVector(X, -110.f, 41.f), 10.f, true); Skep(*this, Ap + FVector(X, -110.f, 49.f), 1.f); }
	Loop(TEXT("forest.bees"), Ap + FVector(0, -60.f, 120.f));
	// The still: a stone firebox, a copper pot gone green, its swan neck and the worm coiling down into a cooling tub.
	const FVector St = At(620.f, -460.f);
	for (int32 c = 0; c < 3; ++c) for (int32 s = 0; s < 4; ++s) K.Add(OldStone(0.75f), FVector(36.f, 110.f, 26.f), St + FRotator(0.f, 90.f * s, 0.f).Vector() * 55.f + FVector(0, 0, 13.f + 26.f * c), FRotator(0.f, 90.f * s, 0.f));
	K.Add(Black(0.4f), FVector(50.f, 4.f, 40.f), St + FVector(60.f, 0, 30.f));
	const FLinearColor Verdigris = Rgb(88, 140, 120) * 0.45f;
	Ball(St + FVector(0, 0, 118.f), 46.f, Verdigris, 0.55f, 0.85f);
	M.Lathe(St + FVector(0, 0, 150.f), FVector::UpVector, 40.f, { FVector(0.f, 16.f, 16.f), FVector(1.f, 8.f, 8.f) }, 10, Verdigris, 0.55f);
	TArray<FVector> Neck = { St + FVector(0, 0, 188.f), St + FVector(40.f, 0, 205.f), St + FVector(120.f, 0, 170.f), St + FVector(160.f, 0, 140.f) };
	M.Tube(Neck, 5.f, 8, Verdigris, 0.55f);
	TArray<FVector> Worm;
	for (int32 w = 0; w <= 32; ++w) Worm.Add(St + FVector(200.f, 0, 0) + FVector(FMath::Cos(w * 0.785f) * 22.f, FMath::Sin(w * 0.785f) * 22.f, 140.f - 3.f * w));
	M.Tube(Worm, 3.5f, 6, Verdigris, 0.55f);
	Barrel(*this, St + FVector(200.f, 0, -2.f), 1.1f);
	Box(St + FVector(0, 0, 50.f), FVector(80.f, 80.f, 50.f));
}

void WRForestLook::FBuild::BreadStoneLane()
{
	using namespace WRForestVillImpl;
	// DC 2.4 #26: a cobbled lane between house-trees (Grovekin homes grown round living trunks), a bread stone at every
	// door. One door is barred from inside and its house-tree has grown shut over it: someone stayed. The rune on its
	// lintel is on every lintel in the lane (the cache at (150, 80) holds one prised off: "Door rune").
	const float HalfLen = WRTune(TEXT("forestWorld.breadLane.halfLength"), 2600.f);
	// The lane: setts laid in rows across, kerbs of larger stones, moss creeping in from the edges.
	for (float X = -HalfLen; X <= HalfLen; X += 50.f)
	{
		for (int32 c = 0; c < 6; ++c)
		{
			const float Y = -125.f + 50.f * c + (int32(X / 50.f) % 2 ? 12.f : -12.f);
			const float J = FMath::Frac(X * 0.017f + c * 0.31f);
			K.Add(OldStone(0.78f + 0.25f * J), FVector(46.f, 46.f, 14.f), At(X, Y, 1.f), FRotator(0.f, 8.f * (J - 0.5f), 0.f));
			if ((c == 0 || c == 5) && J > 0.6f) K.Add(Moss(), FVector(40.f, 30.f, 3.f), At(X, Y + (c == 0 ? -10.f : 10.f), 8.f));
		}
		for (const float Y : { -175.f, 175.f }) K.Add(OldStone(0.7f), FVector(48.f, 30.f, 22.f), At(X, Y, 4.f));
	}
	const float Xs[4] = { -1950.f, -650.f, 650.f, 1950.f };
	int32 Idx = 0;
	for (const float S : { -1.f, 1.f })
	{
		for (int32 h = 0; h < 4; ++h, ++Idx)
		{
			const FVector Tk = At(Xs[h] + 120.f * S, S * 1050.f, -10.f);
			const float H = 2800.f + 300.f * Rand(Idx);
			const float Tr = FMath::Max(70.f, WRForestShape::TrunkRadius(TEXT("lime"), H));
			Tree(TEXT("lime"), Idx % 4, Tk, H, FRotator(0.f, 40.f * Idx, 0.f));
			const float DoorYaw = S > 0.f ? -90.f : 90.f;   // the door faces the lane
			const float Floor = float(Tk.Z) + 30.f;
			const bool bBarred = Idx == 6;
			GroveHouse(Tk, Tr, Floor, DoorYaw, 330.f, false, !bBarred && Idx % 3 == 1, 0.f);
			const FVector DoorDir = FRotator(0.f, DoorYaw, 0.f).Vector();
			const FVector DoorAt = Tk + DoorDir * (Tr + 330.f) + FVector(0, 0, Floor - Tk.Z);
			BreadStone(*this, At(float(DoorAt.X + DoorDir.X * 90.f), float(DoorAt.Y + DoorDir.Y * 90.f)), DoorYaw);
			// A step of flat stones from the lane up to the floor.
			K.Add(OldStone(0.9f), FVector(70.f, 120.f, 18.f), DoorAt + DoorDir * 45.f - FVector(0, 0, 12.f), FRotator(0.f, DoorYaw, 0.f));
			// The lintel rune every house carries (three strokes, a ring: the Grovekin mark for "held by the wood").
			const FVector Lintel = DoorAt + FVector(0, 0, 222.f) + DoorDir * 10.f;
			for (int32 r = 0; r < 3; ++r) K.Add(Black(0.5f), FVector(2.f, 3.f, 22.f), Lintel + FRotator(0.f, DoorYaw + 90.f, 0.f).Vector() * (-14.f + 14.f * r), FRotator(0.f, DoorYaw, (r - 1) * 20.f));
			if (bBarred)
			{
				// Barred from inside, then the tree grew shut over it: a closed leaf, bark creeping across, roots like bars.
				const FRotator Face(0.f, DoorYaw, 0.f);
				K.Add(Timber(0.7f), FVector(8.f, 128.f, 205.f), DoorAt + FVector(0, 0, 103.f), Face);
				K.Add(Bark(0.95f), FVector(12.f, 90.f, 150.f), DoorAt + FVector(0, 0, 120.f) + DoorDir * 6.f + FRotator(0.f, DoorYaw + 90.f, 0.f).Vector() * 30.f, Face);
				for (int32 r = 0; r < 4; ++r)
				{
					const FVector Side = FRotator(0.f, DoorYaw + 90.f, 0.f).Vector();
					Limb(DoorAt + DoorDir * 14.f + Side * (-80.f) + FVector(0, 0, 30.f + 50.f * r), DoorAt + DoorDir * 14.f + Side * 80.f + FVector(0, 0, 60.f + 45.f * r), 7.f, 5.f, RootC(), 6);
				}
				SegBox(DoorAt - FRotator(0.f, DoorYaw + 90.f, 0.f).Vector() * 70.f + FVector(0, 0, 105.f), DoorAt + FRotator(0.f, DoorYaw + 90.f, 0.f).Vector() * 70.f + FVector(0, 0, 105.f), 10.f, 105.f);
				N.Add(Foxfire(0.6f), FVector(6.f, 60.f, 10.f), DoorAt + DoorDir * 18.f + FVector(0, 0, 40.f), Face);
				Lamp(DoorAt + DoorDir * 80.f + FVector(0, 0, 60.f), FoxfireLight(), WRTune(TEXT("forestWorld.foxfireLight"), 500.f) * 0.6f, 500.f);
			}
		}
	}
}

void WRForestLook::FBuild::RootMine()
{
	using namespace WRForestVillImpl;
	// DC 2.4 #27 and 2.6 C3: the Root-Mine head. When this site is the cave's own (Theme set), the cave's chamber is at
	// the origin and its mouth at MouthX: the root-timbered portal stands there. Without a cave, the portal stands at the
	// origin, fallen in (the shaft is elsewhere): rubble plugs it.
	const bool bCave = !Act->Theme.IsNone();
	const float X0 = bCave ? MouthX() : 0.f;
	Mouth(*this, TEXT("root_mine"), X0);
	if (!bCave)
	{
		for (int32 r = 0; r < 4; ++r) Rock(r, FVector(160.f, 120.f, 100.f), At(X0 - 40.f, -120.f + 80.f * r), FRotator(10.f * r, 30.f * r, 0.f), FLinearColor(0.13f, 0.12f, 0.11f), true);
		Limb(At(X0 - 20.f, -200.f, 40.f), At(X0 + 30.f, 180.f, 140.f), 24.f, 20.f, RootC() * 0.8f, 8);   // the lintel root torn down
	}
	// A second cart still on the rails, its ore spilled: whoever pushed it last didn't finish.
	Cart(*this, At(X0 + 700.f, 0.f, 14.f), 0.f, false);
	for (int32 o = 0; o < 7; ++o) Ball(At(X0 + 800.f + 25.f * o, 60.f + 15.f * (o % 3), 6.f), 12.f + 3.f * (o % 2), Rgb(70, 58, 52) * 0.5f, 0.85f, 0.7f);
	// The pick rack: Grovekin picks of antler tine hafted on heartwood, three on pegs, one gone.
	const FVector Rk = At(X0 + 250.f, -360.f);
	for (const float Y : { -70.f, 70.f }) K.Rod(Timber(0.8f), Rk + FVector(0, Y, -20.f), Rk + FVector(0, Y, 150.f), 12.f, true);
	K.Add(Timber(0.85f), FVector(12.f, 170.f, 12.f), Rk + FVector(0, 0, 140.f));
	for (int32 p = 0; p < 3; ++p)
	{
		const FVector Peg = Rk + FVector(8.f, -50.f + 50.f * p, 132.f);
		Limb(Peg, Peg - FVector(0, 0, 90.f), 3.f, 3.f, HeartwoodC(), 6);
		Limb(Peg + FVector(0, -20.f, -6.f), Peg + FVector(6.f, 26.f, 4.f), 4.f, 1.f, BoneC(), 6);
	}
	Box(Rk + FVector(0, 0, 70.f), FVector(10.f, 80.f, 80.f));
	Hulder(At(X0 + 250.f, 340.f), 0.f, 1.f);
}

void WRForestLook::FBuild::SunkenOrchard()
{
	using namespace WRForestVillImpl;
	// DC 2.4 #28: an orchard in a hollow, every old tree hung with charms of woven straw (someone begged it to keep
	// fruiting); only one Idunn apple tree still bears, at (400, 300), where its node is. The rest stand half dead, a
	// few fallen. Windfalls rot under the living one.
	const float Xs[5] = { -800.f, -400.f, 0.f, 400.f, 800.f };
	const float Ys[3] = { -500.f, -100.f, 300.f };
	int32 Idx = 0;
	for (int32 r = 0; r < 3; ++r)
	{
		for (int32 c = 0; c < 5; ++c, ++Idx)
		{
			const bool bIdunn = r == 2 && c == 3;
			const bool bFallen = !bIdunn && (Idx == 2 || Idx == 9);
			const FVector Tb = At(Xs[c] + 30.f * (Rand(Idx) - 0.5f), Ys[r] + 30.f * (Rand(Idx + 50) - 0.5f), -5.f);
			if (bFallen)
			{
				// Down in a winter storm, roots up: a dead trunk on its side, the charm still tied to a branch.
				Limb(Tb, Tb + FVector(260.f, 60.f, 30.f), 20.f, 12.f, BarkC() * 0.85f, 8);
				Limb(Tb + FVector(150.f, 40.f, 25.f), Tb + FVector(230.f, 150.f, 110.f), 9.f, 3.f, BarkC() * 0.85f, 6);
				Ball(Tb - FVector(30.f, 0, -40.f), 70.f, Rgb(74, 56, 40) * 0.42f, 0.95f, 0.8f);   // the root plate's soil
				SegBox(Tb + FVector(0, 0, 20.f), Tb + FVector(260.f, 60.f, 40.f), 20.f, 20.f);
				Ball(Tb + FVector(230.f, 150.f, 60.f), 5.f, StrawC(), 0.95f, 1.f);
				continue;
			}
			AppleTree(*this, Tb, bIdunn ? 560.f : 460.f + 60.f * Rand(Idx + 7), Idx, bIdunn, !bIdunn, true);
			// A straw garland on a string from this tree to the next in the row.
			if (c < 4) Line(Tb + FVector(0, 0, 230.f), FVector(Xs[c + 1], Ys[r], G(Xs[c + 1], Ys[r]) + 230.f), 0.7f, 50.f, StrawC(), 8);
		}
	}
	// Windfalls under the Idunn tree, some bitten.
	const FVector Idunn = At(400.f, 300.f);
	for (int32 a = 0; a < 9; ++a) Ball(Idunn + FRotator(0.f, 40.f * a, 0.f).Vector() * (90.f + 40.f * (a % 3)) + FVector(0, 0, 5.f), 5.f, a % 3 ? Rgb(120, 70, 30) * 0.5f : Rgb(190, 60, 30) * 0.6f, 0.7f, 0.8f);
	// A harvest ladder left against the living tree, and a basket.
	K.Rod(Timber(0.85f), Idunn + FVector(-120.f, -30.f, 0.f), Idunn + FVector(-30.f, -30.f, 300.f), 7.f);
	K.Rod(Timber(0.85f), Idunn + FVector(-120.f, 30.f, 0.f), Idunn + FVector(-30.f, 30.f, 300.f), 7.f);
	for (int32 g = 1; g < 8; ++g) K.Rod(Timber(0.8f), Idunn + FVector(-120.f + 90.f * g / 8.f, -30.f, 300.f * g / 8.f), Idunn + FVector(-120.f + 90.f * g / 8.f, 30.f, 300.f * g / 8.f), 4.f);
	M.Lathe(Idunn + FVector(-200.f, 120.f, 0.f), FVector::UpVector, 34.f, { FVector(0.f, 18.f, 18.f), FVector(1.f, 26.f, 26.f) }, 12, StrawC() * 0.8f, 0.95f);
	// The hollow's lip: a bank of old field stones round the orchard, mossed (it's older than the newcomers).
	for (int32 s = 0; s < 4; ++s)
	{
		const float A0 = 90.f * s + 15.f, A1 = 90.f * s + 75.f;
		const FVector P0 = FRotator(0.f, A0, 0.f).Vector() * 1300.f, P1 = FRotator(0.f, A1, 0.f).Vector() * 1300.f;
		DryWall(*this, P0, P1, 80.f, 0.7f);
	}
}

void WRForestLook::FBuild::CrystalRoots()
{
	using namespace WRForestVillImpl;
	// DC 2.4 #29: a beech whose roots split a crystal boulder at the Crystal border (the neighbours touched). One shard
	// to take, at (180, 0) between the halves (the cache: one, never again). Violet light at night.
	const float H = 2600.f;
	const FVector Tb = At(-380.f, 0.f, -10.f);
	Tree(TEXT("beech"), 2, Tb, H, FRotator(0.f, 20.f, 0.f));
	const float Tr = FMath::Max(55.f, WRForestShape::TrunkRadius(TEXT("beech"), H));
	// The boulder in two halves, pushed 90 cm apart by the roots grown into the crack.
	const FVector Bc = At(180.f, 0.f);
	const FLinearColor Grey(0.16f, 0.15f, 0.17f);
	Rock(0, FVector(230.f, 150.f, 190.f), Bc + FVector(0, -125.f, 0), FRotator(0.f, 8.f, -6.f), Grey, true, 20.f);
	Rock(1, FVector(220.f, 150.f, 175.f), Bc + FVector(0, 125.f, 0), FRotator(0.f, -10.f, 7.f), Grey, true, 20.f);
	// Crystal growing in the faces of the split: violet points, clear of the middle where the shard lies.
	for (int32 c = 0; c < 12; ++c)
	{
		const float Sy = c % 2 ? 1.f : -1.f;
		const FVector Base = Bc + FVector(-60.f + 22.f * (c / 2), Sy * (48.f + 10.f * (c % 3)), 30.f + 25.f * (c % 4));
		const FVector Dir = FVector(0.2f * ((c % 3) - 1), -Sy * 0.6f, 0.8f).GetSafeNormal();
		const float Len = 40.f + 18.f * (c % 4);
		K.Spike(Crystal(), Base, Dir, 14.f + 3.f * (c % 3), Len);
		N.Add(Violet(0.9f), FVector(5.f, 5.f, Len * 0.7f), Base + Dir * Len * 0.35f, FRotationMatrix::MakeFromZ(Dir).Rotator());
	}
	// The roots: from the beech's flare along the ground and into the crack, wedging it apart; one over the top.
	for (int32 r = 0; r < 3; ++r)
	{
		const float Sy = float(r - 1);
		Root({ Tb + FVector(Tr * 0.8f, Sy * 40.f, 20.f), At(-120.f, Sy * 60.f, 25.f), Bc + FVector(-90.f, Sy * 20.f, 40.f), Bc + FVector(40.f, Sy * 15.f, -30.f) }, 34.f - 6.f * FMath::Abs(Sy), 10.f, RootC(), 8);
	}
	Root({ Tb + FVector(Tr * 0.6f, -60.f, 40.f), Bc + FVector(-70.f, -130.f, 170.f), Bc + FVector(60.f, -140.f, 160.f), Bc + FVector(120.f, -200.f, -20.f) }, 20.f, 8.f, RootC(), 7);
	K.Add(Moss(), FVector(160.f, 120.f, 5.f), Bc + FVector(0, -120.f, 175.f), FRotator(0.f, 8.f, -6.f));
	Lamp(Bc + FVector(0, 0, 90.f), FLinearColor(FColor(150, 110, 220)), WRTune(TEXT("forestWorld.crystalLight"), 1500.f), 900.f);
}

// ================================================================================================ the Elder Canopy

void WRForestLook::FBuild::RootgraveFort()
{
	using namespace WRForestVillImpl;
	// DC 2.4 #30 and 2.20: the Grovekin pass fort facing west into Rootgrave. Its wall is a ring of pleached trees, and
	// every bent tree in it points at the pass. Its gate is broken OUTWARD (they went out at the Mourners), and grave
	// salt crusts its west face (salt kills roots: the Mourners salted them; those trees are dead). An archer's stand
	// inside on the west side, a vista down Rootgrave; the archer's rack is the cache at (150, 80).
	const float Ring = WRTune(TEXT("forestWorld.rootgraveFort.radius"), 1500.f);
	const FVector West = T.InverseTransformVectorNoScale(FVector(-1.f, 0.f, 0.f)).GetSafeNormal2D();
	const float WestYaw = West.Rotation().Yaw;
	const int32 Trees = 40;
	TArray<FVector> Bases;
	TArray<bool> Gap;
	for (int32 i = 0; i < Trees; ++i)
	{
		const float Ang = WestYaw + 360.f * i / Trees;
		Bases.Add(At(FRotator(0.f, Ang, 0.f).Vector().X * Ring, FRotator(0.f, Ang, 0.f).Vector().Y * Ring, -10.f));
		Gap.Add(i == 0);   // the gate is at the west, tree 0's place
	}
	for (int32 i = 0; i < Trees; ++i)
	{
		const FVector Dir = Bases[i].GetSafeNormal2D();
		const float Facing = FVector::DotProduct(Dir, West);
		const bool bSalted = Facing > 0.35f;
		if (Gap[i]) continue;
		// A pleached beech: a trunk 6.5 m, its top bent over toward the pass (the warning every tree gives).
		const FVector Top = Bases[i] + FVector(0, 0, 650.f);
		const FVector Tip = Top + West * 200.f + FVector(0, 0, 110.f);
		const FLinearColor Bark0 = bSalted ? BarkC() * 0.7f : BeechC();
		Limb(Bases[i] - FVector(0, 0, 30.f), Top, 22.f, 14.f, Bark0, 8);
		Limb(Top, Tip, 14.f, 5.f, Bark0, 7);
		if (!bSalted)
		{
			Ball(Tip + FVector(0, 0, 20.f), 120.f, LeafC(0.95f), 0.9f, 0.6f);
			Ball(FMath::Lerp(Top, Tip, 0.4f) + FVector(0, 0, 40.f), 90.f, LeafC(0.85f), 0.9f, 0.6f);
		}
		else
		{
			// The salted ones are dead: bare twigs, a white crust up the west face, salt drifted at the foot.
			for (int32 c = 0; c < 4; ++c) K.Add(Salt(), FVector(14.f, 40.f - 5.f * c, 60.f), Bases[i] + Dir * 22.f + FVector(0, 0, 40.f + 70.f * c), FRotator(0.f, Dir.Rotation().Yaw, 0.f));
			K.Add(Salt(), FVector(120.f, 110.f, 6.f), Bases[i] + Dir * 90.f + FVector(0, 0, 2.f), FRotator(0.f, Dir.Rotation().Yaw, 0.f));
			Limb(Tip, Tip + FVector(0, 30.f, 70.f), 4.f, 1.f, Bark0, 4);
		}
		// Pleaching: woven to the next tree, crossing branches, low and high.
		const int32 j = (i + 1) % Trees;
		if (!Gap[j])
		{
			Limb(Bases[i] + FVector(0, 0, 120.f), Bases[j] + FVector(0, 0, 420.f), 8.f, 6.f, Bark0, 5);
			Limb(Bases[j] + FVector(0, 0, 120.f), Bases[i] + FVector(0, 0, 420.f), 8.f, 6.f, Bark0, 5);
			Limb(Bases[i] + FVector(0, 0, 280.f), Bases[j] + FVector(0, 0, 280.f), 7.f, 7.f, Bark0, 5);
			SegBox(Bases[i] + FVector(0, 0, 230.f), Bases[j] + FVector(0, 0, 230.f), 14.f, 230.f);
		}
	}
	// The gate: its two posts splintered, both leaves torn off their hinges and lying OUTSIDE, pointing at the pass.
	const FVector Gate = Bases[0];
	const FVector GSide = FVector::CrossProduct(West, FVector::UpVector);
	for (const float S : { -1.f, 1.f })
	{
		const FVector Post = Gate + GSide * S * 170.f;
		K.Rod(Timber(0.8f), Post - FVector(0, 0, 30.f), Post + FVector(0, 0, 330.f - 60.f * (S > 0.f ? 1.f : 0.f)), 34.f, true);
		K.Spike(Timber(0.7f), Post + FVector(0, 0, 330.f - 60.f * (S > 0.f ? 1.f : 0.f)), FVector(0.2f, 0.1f, 1.f), 26.f, 40.f);
		K.Add(Iron(), FVector(10.f, 20.f, 14.f), Post + West * 14.f + FVector(0, 0, 90.f));
		K.Add(Iron(), FVector(10.f, 20.f, 14.f), Post + West * 14.f + FVector(0, 0, 230.f));
		Box(Post + FVector(0, 0, 150.f), FVector(18.f, 18.f, 180.f));
		// The leaf: woven heartwood on a frame, flat on the ground outside, its hinge end toward the gate.
		const FVector Leaf0 = Gate + GSide * S * 120.f + West * (240.f + 30.f * S);
		const FVector Lg = At(float(Leaf0.X), float(Leaf0.Y), 10.f);
		K.Add(Heartwood(0.8f), FVector(260.f, 170.f, 14.f), Lg, FRotator(4.f * S, WestYaw + 10.f * S, 3.f));
		for (int32 b = 0; b < 3; ++b) K.Add(Timber(0.7f), FVector(16.f, 175.f, 10.f), Lg + West * (-100.f + 100.f * b) + FVector(0, 0, 10.f), FRotator(0.f, WestYaw + 10.f * S, 0.f));
		Box(Lg, FVector(130.f, 85.f, 10.f), FRotator(0.f, WestYaw + 10.f * S, 0.f));
	}
	K.Add(Salt(), FVector(380.f, 520.f, 5.f), Gate + West * 200.f + FVector(0, 0, 3.f), FRotator(0.f, WestYaw, 0.f));
	// The archer's stand inside the west wall: a deck on four posts 2.5 m up, plank steps from the inside; from it you
	// see down Rootgrave.
	const FVector Sc = West * (Ring - 450.f);
	const float Sz = G(float(Sc.X), float(Sc.Y)) + 250.f;
	TArray<FVector> NoTrunks;
	TArray<float> NoRadii;
	Deck(Sc, FVector2D(150.0, 260.0), Sz, WestYaw, NoTrunks, NoRadii, true);
	for (const float X : { -130.f, 130.f })
	{
		for (const float Y : { -240.f, 240.f })
		{
			const FVector Pp = Sc + FRotator(0.f, WestYaw, 0.f).RotateVector(FVector(X, Y, 0.f));
			K.Rod(Timber(0.85f), At(float(Pp.X), float(Pp.Y), -30.f), FVector(Pp.X, Pp.Y, Sz - 10.f), 20.f, true);
			Box(FVector(Pp.X, Pp.Y, (Sz + G(float(Pp.X), float(Pp.Y))) * 0.5f), FVector(10.f, 10.f, FMath::Max(10.f, (Sz - G(float(Pp.X), float(Pp.Y))) * 0.5f)));
		}
	}
	for (int32 s = 0; s < 10; ++s)
	{
		const FVector Sp = Sc - West * (180.f + 40.f * s);
		const float Z = Sz - 25.f * (s + 1);
		if (Z < G(float(Sp.X), float(Sp.Y))) break;
		K.Add(Plank(0.85f), FVector(38.f, 110.f, 6.f), FVector(Sp.X, Sp.Y, Z), FRotator(0.f, WestYaw, 0.f));
		Box(FVector(Sp.X, Sp.Y, Z - 3.f), FVector(19.f, 55.f, 4.f), FRotator(0.f, WestYaw, 0.f));
	}
}

void WRForestLook::FBuild::LanternLine()
{
	using namespace WRForestVillImpl;
	// DC 2.4 #31 (world-map KEPT: the Mourners' lanterns on the forest side): tōrō pushed deep into the forest along the
	// Mourners' road (+X from the pass), each one wrapped and split by roots, tilted, its light box cracked. Unlit (they
	// light only before an invasion); foxfire crawls on the roots that hold them.
	const int32 Count = FMath::Clamp(int32(WRTune(TEXT("forestWorld.lanternLine.count"), 7.f)), 2, 20);
	const float Step = WRTune(TEXT("forestWorld.lanternLine.spacing"), 600.f);
	for (int32 i = 0; i < Count; ++i)
	{
		const float X = Step * i;
		const float Y = 260.f * FMath::Sin(i * 1.3f);
		const FVector Bp = At(X, Y, -8.f);
		const float Tilt = 4.f + 14.f * Rand(i + 3);
		const float Split = i % 2 ? 10.f + 20.f * Rand(i + 11) : 0.f;
		const float Yaw = 15.f * (Rand(i + 19) - 0.5f);
		Toro(Bp, Yaw, Tilt, Split);
		Box(Bp + FVector(0, 0, 100.f), FVector(40.f, 40.f, 100.f), FRotator(Tilt, Yaw, Tilt * 0.4f));
		// The roots: two or three out of the ground, up over the roof and down the far side, gripping it.
		for (int32 r = 0; r < 2 + i % 2; ++r)
		{
			const float Ra = 360.f * r / 3.f + 40.f * i;
			const FVector Dir = FRotator(0.f, Ra, 0.f).Vector();
			const FVector G0 = At(float(Bp.X + Dir.X * 140.f), float(Bp.Y + Dir.Y * 140.f), -10.f);
			const FVector G1 = At(float(Bp.X - Dir.X * 150.f + Dir.Y * 60.f), float(Bp.Y - Dir.Y * 150.f - Dir.X * 60.f), -10.f);
			Root({ G0, Bp + Dir * 55.f + FVector(0, 0, 120.f), Bp + FVector(0, 0, 215.f), Bp - Dir * 60.f + FVector(0, 0, 140.f), G1 }, 13.f, 8.f, RootC(), 7);
			if (r == 0) N.Add(Foxfire(0.7f), FVector(50.f, 6.f, 6.f), Bp + Dir * 55.f + FVector(0, 0, 122.f), (FVector(0, 0, 95.f) - Dir * 55.f).Rotation());
		}
		if (Split > 0.f) Limb(At(X, Y, -20.f), Bp + FVector(0, 0, 200.f), 16.f, 6.f, RootC(), 7);   // the root that split it, up through the box
		if (i % 2 == 0) Lamp(Bp + FVector(0, 0, 120.f), FoxfireLight(), WRTune(TEXT("forestWorld.foxfireLight"), 500.f) * 0.5f, 450.f);
	}
}

void WRForestLook::FBuild::FernStair()
{
	using namespace WRForestVillImpl;
	// DC 2.4 #32: a stone stair up from the forest floor to the Four Hearts' clearing (Sylvaroth's door). Old Grovekin
	// work: worn treads, moss on every edge, ferns either side. It climbs whichever way the ground rises along the
	// site's +-X (the run-back stone 50 m east is the boss track's).
	const float Za = G(-900.f, 0.f), Zb = G(900.f, 0.f);
	const float Sx = Zb >= Za ? 1.f : -1.f;
	const float Bot = FMath::Min(Za, Zb), Top = FMath::Max(Za, Zb);
	const float Rise = FMath::Max(Top - Bot, 150.f);
	const int32 Steps = FMath::Clamp(FMath::CeilToInt(Rise / 18.f), 6, 160);
	const float Run = 1800.f / Steps;
	for (int32 i = 0; i < Steps; ++i)
	{
		const float X = Sx * (-900.f + Run * (i + 0.5f));
		const float Tz = Bot + Rise * (i + 1) / Steps;
		const float Gz = G(X, 0.f);
		const float Hh = FMath::Max(20.f, Tz - Gz + 30.f);
		const float J = FMath::Frac(i * 0.37f);
		K.Add(OldStone(0.78f + 0.2f * J), FVector(Run + 6.f, 280.f - 20.f * J, Hh), FVector(X, 8.f * (J - 0.5f), Tz - Hh * 0.5f), FRotator(0.f, 2.f * (J - 0.5f), 0.f));
		K.Add(Moss(0.9f + 0.2f * J), FVector(10.f, 240.f * J + 30.f, 3.f), FVector(X + Sx * Run * 0.45f, 0.f, Tz + 1.f));
		Box(FVector(X, 0.f, Tz - Hh * 0.5f), FVector(Run * 0.5f + 2.f, 140.f, Hh * 0.5f));
		// Big side stones every few steps, and ferns crowding in.
		if (i % 4 == 0)
		{
			for (const float S : { -1.f, 1.f }) Rock(i + (S > 0.f ? 1 : 0), FVector(130.f, 90.f, 110.f), FVector(X, S * 200.f, G(X, S * 200.f)), FRotator(0.f, 20.f * i, 0.f), FLinearColor(0.15f, 0.15f, 0.13f), true);
		}
		if (i % 2 == 1) for (const float S : { -1.f, 1.f }) Fern(*this, At(X, S * (290.f + 40.f * J)), 1.f + 0.3f * J, i);
	}
	// The landing at the top: flags, and the clearing beyond.
	const float Lx = Sx * (900.f + 150.f);
	for (int32 f = 0; f < 9; ++f) K.Add(OldStone(0.85f + 0.05f * (f % 3)), FVector(95.f, 95.f, 14.f), FVector(Lx + Sx * 100.f * (f % 3 - 1), 100.f * (f / 3 - 1), Top - 4.f));
	Box(FVector(Lx, 0.f, Top - 6.f), FVector(150.f, 150.f, 8.f));
}

void WRForestLook::FBuild::CanopyLiftFoot()
{
	using namespace WRForestVillImpl;
	// DC 2.4 #33: the foot of the Grovekin counterweight lift (AWRCanopyLift, the server's machine, hangs on the giant
	// nearest the site; this matches its placement in UWRForestWorld::SpawnServerActors). Here: the giant if the land has
	// none there, the path worn to the basket, a mounting block at the sill, a coil of spare rope, a hulder at the way in.
	const FWRForestData::FSite* Row = FWRForestData::Get().FindSite(Act->SiteId);
	const FVector SiteAt = Row ? Row->At : Act->GetActorLocation();
	const float SiteYaw = Row ? Row->Yaw : Act->GetActorRotation().Yaw;
	const FWRForestData::FTree* Gt = WRForestShape::GiantNear(SiteAt, 3000.f);
	const FVector TrunkW = Gt ? Gt->Base : SiteAt + FRotator(0.f, SiteYaw, 0.f).RotateVector(FVector(WRTune(TEXT("forestWorld.lift.ownGiantOffset"), 900.f), 0, 0));
	const float TrunkR = Gt ? Gt->Radius : WRForestShape::TrunkRadius(TEXT("giant"), 9000.f);
	if (!Gt)
	{
		const FVector Tl = Local(TrunkW);
		Giant(*this, 1, FVector(Tl.X, Tl.Y, G(float(Tl.X), float(Tl.Y))), 9000.f);
	}
	const float R = WRTune(TEXT("forestWorld.lift.wheelRadius"), 110.f);
	FVector Away = (SiteAt - TrunkW).GetSafeNormal2D();
	if (Away.IsNearlyZero()) Away = FRotator(0.f, SiteYaw + 180.f, 0.f).Vector();
	const FVector FootW = TrunkW + Away * (TrunkR + 2.f * R + WRTune(TEXT("forestWorld.lift.stoneClear"), 160.f));
	// Local directions: Out (away from the trunk), Side (the lift's door side: its right vector).
	const FVector Out = T.InverseTransformVectorNoScale(Away).GetSafeNormal2D();
	const FVector Side = FRotator(0.f, (-Out).Rotation().Yaw + 90.f, 0.f).Vector();
	const FVector Fl = Local(FootW);
	const FVector Foot(Fl.X, Fl.Y, G(float(Fl.X), float(Fl.Y)));
	// The worn path: packed earth from the sill out 18 m, a little wider where people waited.
	for (int32 s = 0; s < 9; ++s)
	{
		const FVector Pp = Foot + Out * (220.f + 200.f * s) + Side * 40.f * FMath::Sin(s * 0.9f);
		K.Add(Earth(0.85f), FVector(210.f, s < 2 ? 200.f : 140.f, 6.f), At(float(Pp.X), float(Pp.Y), 1.f), FRotator(0.f, Out.Rotation().Yaw, 0.f));
	}
	// The mounting block beside the sill on the door side: three dressed stones as steps.
	for (int32 s = 0; s < 3; ++s)
	{
		const FVector Mb = Foot + Side * (260.f + 50.f * s);
		const float Hh = 40.f - 12.f * s;
		K.Add(OldStone(0.85f), FVector(70.f, 90.f, Hh), FVector(Mb.X, Mb.Y, G(float(Mb.X), float(Mb.Y)) + Hh * 0.5f - 4.f), FRotator(0.f, Side.Rotation().Yaw, 0.f));
		Box(FVector(Mb.X, Mb.Y, G(float(Mb.X), float(Mb.Y)) + Hh * 0.5f - 4.f), FVector(25.f, 45.f, Hh * 0.5f), FRotator(0.f, Side.Rotation().Yaw, 0.f));
	}
	// The spare rope: a flat coil on a board under a bark lean-to (a long fall needs a spare).
	const FVector Cp = Foot - Side * 280.f;
	const FVector Cg(Cp.X, Cp.Y, G(float(Cp.X), float(Cp.Y)));
	K.Add(Plank(0.8f), FVector(120.f, 120.f, 6.f), Cg + FVector(0, 0, 3.f));
	TArray<FVector> Coil;
	for (int32 w = 0; w <= 96; ++w) Coil.Add(Cg + FVector(FMath::Cos(w * 0.39f) * (20.f + 0.35f * w), FMath::Sin(w * 0.39f) * (20.f + 0.35f * w), 10.f + 3.f * (w / 16)));
	M.Tube(Coil, 3.f, 5, RopeC(), 0.95f);
	for (const float S : { -1.f, 1.f }) K.Rod(Timber(0.8f), Cg + Side * S * 70.f - Out * 70.f, Cg + Side * S * 70.f - Out * 70.f + FVector(0, 0, 140.f), 10.f, true);
	K.Add(Bark(0.85f), FVector(160.f, 170.f, 6.f), Cg + FVector(0, 0, 130.f) - Out * 20.f, FRotator(-15.f, Out.Rotation().Yaw, 0.f));
	// A hulder post at the path's start, looking out the way you come.
	const FVector Hp = Foot + Out * 600.f + Side * 220.f;
	Hulder(At(float(Hp.X), float(Hp.Y)), Out.Rotation().Yaw, 1.f);
	// Foxfire low on the giant's bark by the sill at night, so the lift reads in the dark.
	N.Add(Foxfire(0.7f), FVector(8.f, 60.f, 90.f), Foot - Out * (2.f * R + 140.f) + FVector(0, 0, 80.f), FRotator(0.f, Out.Rotation().Yaw, 0.f));
	Lamp(Foot - Out * 120.f + FVector(0, 0, 120.f), FoxfireLight(), WRTune(TEXT("forestWorld.foxfireLight"), 500.f), 700.f);

	// The gallery at the top (plan 2.5, R7): where the basket arrives the canopy walkways begin, so a ring walk of planks
	// goes round the giant at that height, carried like the Grovekin house floors on joists let into the bark with knee
	// braces under them, a rope rail at hand height on posts, and an iron ring where each walkway's ropes are tied off.
	// It leaves a wedge open on the lift's side: the stone and its rope run 1.1 m off the bark there, the basket beyond it,
	// and the lift's own landing deck (AWRCanopyLift, on its door side) meets the gallery's end across one step. Its
	// height is the same number the server gives the machine's rise (WRForestShape::LiftTopZ), so the two meet.
	const float FootWorldZ = float(Act->GetActorLocation().Z) + float(Foot.Z);
	const float TopL = WRForestShape::LiftTopZ(TrunkW, TrunkR, FootWorldZ) - float(Act->GetActorLocation().Z);
	const FVector TrunkL = Local(TrunkW);
	const FVector Hub(TrunkL.X, TrunkL.Y, TopL);
	const float GalOut = WRTune(TEXT("forestWorld.lift.galleryWidth"), 260.f);
	const float GalIn = TrunkR - 15.f, GalRim = TrunkR + GalOut;
	const float AwayYaw = float(Out.Rotation().Yaw);
	// The lift's door side (its local +Y) is Away turned -90: the landing spans Away-8..-50 degrees, the stone +-12.
	const float GapFrom = AwayYaw + WRTune(TEXT("forestWorld.lift.galleryGapAfter"), 14.f);
	const float GapSpan = 360.f - WRTune(TEXT("forestWorld.lift.galleryGapAfter"), 14.f) - WRTune(TEXT("forestWorld.lift.galleryGapBefore"), 60.f);
	// Where the walkways meet the rim (their ends near this giant): the rail opens there.
	TArray<float> Openings;
	for (const FWRForestData::FWalkway& Wk : FWRForestData::Get().Walkways)
	{
		if (!Wk.Arena.IsNone()) continue;
		for (const FVector& End : { FVector(Wk.A), FVector(Wk.B) })
		{
			if (FVector::Dist2D(End, TrunkW) < TrunkR + WRTune(TEXT("forestWorld.lift.walkwayReach"), 800.f)) Openings.Add(float((Local(End) - TrunkL).Rotation().Yaw));
		}
	}
	const auto IsOpen = [&Openings](float Ang)
	{
		for (const float O : Openings) if (FMath::Abs(FRotator::NormalizeAxis(Ang - O)) < 9.f) return true;
		return false;
	};
	const int32 Boards = 44;
	for (int32 b = 0; b < Boards; ++b)
	{
		const float Ang = GapFrom + GapSpan * (b + 0.5f) / Boards;
		const FVector Dir = FRotator(0.f, Ang, 0.f).Vector();
		const float Chord = 2.f * GalRim * FMath::Sin(FMath::DegreesToRadians(GapSpan / Boards * 0.5f));
		K.Add(Plank(0.86f + 0.2f * Rand(b + 300)), FVector(GalRim - GalIn, Chord * 0.94f, 6.f), Hub + Dir * ((GalIn + GalRim) * 0.5f) - FVector(0, 0, 3.f), FRotator(0.f, Ang, 0.f));
		if (b % 4 == 1)
		{
			// A joist let into the bark (an iron band where it goes in), a knee brace under it back to the trunk.
			K.Add(Timber(), FVector(GalOut + 50.f, 18.f, 22.f), Hub + Dir * (TrunkR + GalOut * 0.5f - 25.f) - FVector(0, 0, 17.f), FRotator(0.f, Ang, 0.f));
			K.Add(Iron(), FVector(8.f, 26.f, 28.f), Hub + Dir * (TrunkR + 6.f) - FVector(0, 0, 17.f), FRotator(0.f, Ang, 0.f));
			K.Rod(Timber(0.9f), Hub + Dir * (TrunkR - 8.f) - FVector(0, 0, 230.f), Hub + Dir * (TrunkR + GalOut * 0.7f) - FVector(0, 0, 26.f), 12.f);
		}
	}
	for (int32 c = 0; c < 11; ++c)
	{
		// The walking surface's collision, in eleven flat pieces round the ring.
		const float Ang = GapFrom + GapSpan * (c + 0.5f) / 11.f;
		const FVector Dir = FRotator(0.f, Ang, 0.f).Vector();
		const float Chord = 2.f * GalRim * FMath::Sin(FMath::DegreesToRadians(GapSpan / 11.f * 0.5f));
		Box(Hub + Dir * ((GalIn + GalRim) * 0.5f) - FVector(0, 0, 6.f), FVector((GalRim - GalIn) * 0.5f, Chord * 0.5f, 6.f), FRotator(0.f, Ang, 0.f));
	}
	// The rail: a post every 22 degrees on the rim, a rope between at 1.05 m (a gap where a walkway comes in, a ring
	// there for its ropes), the posts at the gap's ends too so nobody walks off the open wedge.
	const int32 Posts = FMath::Max(2, FMath::RoundToInt(GapSpan / 22.f));
	for (int32 p = 0; p <= Posts; ++p)
	{
		const float Ang = GapFrom + GapSpan * p / Posts;
		const FVector Pp = Hub + FRotator(0.f, Ang, 0.f).Vector() * (GalRim - 12.f);
		K.Rod(Timber(), Pp - FVector(0, 0, 20.f), Pp + FVector(0, 0, 115.f), 11.f);
		if (p == Posts) break;
		const float Mid = GapFrom + GapSpan * (p + 0.5f) / Posts;
		const FVector Pn = Hub + FRotator(0.f, GapFrom + GapSpan * (p + 1) / Posts, 0.f).Vector() * (GalRim - 12.f);
		if (IsOpen(Mid))
		{
			K.Disc(Iron(), (Pp + Pn) * 0.5f + FVector(0, 0, 100.f), FRotator(0.f, Mid, 0.f).Vector(), 16.f, 3.f);
			continue;
		}
		Line(Pp + FVector(0, 0, 105.f), Pn + FVector(0, 0, 105.f), 2.5f, 6.f, RopeC(), 6);
		SegBox(Pp + FVector(0, 0, 60.f), Pn + FVector(0, 0, 60.f), 3.f, 55.f);
	}
	// The lift's side of the gallery: a rope across the open wedge's far end at hip height, so the step to the lift's
	// landing is the only way off there.
	{
		const FVector EndA = Hub + FRotator(0.f, GapFrom, 0.f).Vector() * (GalIn + 20.f);
		const FVector EndB = Hub + FRotator(0.f, GapFrom, 0.f).Vector() * (GalRim - 12.f);
		Line(EndA + FVector(0, 0, 100.f), EndB + FVector(0, 0, 100.f), 2.5f, 4.f, RopeC(), 4);
		SegBox(EndA + FVector(0, 0, 55.f), EndB + FVector(0, 0, 55.f), 3.f, 50.f);
	}
	// A foxfire glow hung over the middle of the gallery at night: it reads from the forest floor 30 m below.
	Lamp(Hub + FRotator(0.f, GapFrom + GapSpan * 0.5f, 0.f).Vector() * (GalRim - 20.f) + FVector(0, 0, 140.f), FoxfireLight(), WRTune(TEXT("forestWorld.foxfireLight"), 500.f), 900.f);
}

void WRForestLook::FBuild::SkyLarder()
{
	using namespace WRForestVillImpl;
	// DC 2.4 #34: a Grovekin hunters' platform house 35 m up between two giants: dried meat and hides still hung (the
	// cache at local (0, 220) on the deck: UWRForestWorld places it at forest.skyLarderZ + 40). Reached by a plank stair
	// spiralling round the first giant (the game has no climbing). The Grey Matron is seen from here (the Bower is near).
	const FWRForestData::FSite* Row = FWRForestData::Get().FindSite(Act->SiteId);
	const FVector SiteAt = Row ? Row->At : Act->GetActorLocation();
	const float DeckTop = WRTune(TEXT("forestWorld.skyLarderZ"), 3500.f);
	const FVector C(0.f, 220.f, DeckTop);
	const float OwnR = WRForestShape::TrunkRadius(TEXT("giant"), 9000.f);
	FVector A;
	float RA;
	const FWRForestData::FTree* Ga = WRForestShape::GiantNear(SiteAt, 1500.f);
	const FVector GaL = Ga ? Local(Ga->Base) : FVector::ZeroVector;
	if (Ga && FVector::Dist2D(GaL, C) > Ga->Radius + 400.f)
	{
		A = FVector(GaL.X, GaL.Y, G(float(GaL.X), float(GaL.Y)));
		RA = Ga->Radius;
	}
	else
	{
		A = At(-950.f, 220.f);
		RA = OwnR;
		Giant(*this, 0, A, 9000.f);
	}
	const FVector Dir = (FVector(C.X, C.Y, 0.f) - FVector(A.X, A.Y, 0.f)).GetSafeNormal2D();
	const float Span = FVector::Dist2D(A, C);
	const FVector Bxy = C + Dir * Span;
	const FVector Bt(Bxy.X, Bxy.Y, G(float(Bxy.X), float(Bxy.Y)));
	const float RB = OwnR;
	Giant(*this, 2, Bt, 9000.f);
	const float HalfX = FMath::Max(250.f, Span - FMath::Max(RA, RB) - 60.f);
	const float HalfY = 380.f;
	const float DeckYaw = Dir.Rotation().Yaw;
	Deck(C, FVector2D(HalfX, HalfY), DeckTop, DeckYaw, { A, Bt }, { RA, RB }, true);
	const FVector Side = FRotator(0.f, DeckYaw + 90.f, 0.f).Vector();
	// The house on the deck's far side (the cache's spot at the middle stays clear): plank walls on three sides, a bark
	// roof, the open side to the deck; shelves inside.
	const FVector Hc = C + Side * 240.f;
	const float Hw = FMath::Min(HalfX * 1.4f, 360.f);
	const FRotator Hr(0.f, DeckYaw, 0.f);
	const auto HP = [&](float X, float Y, float Z) { return Hc + Hr.RotateVector(FVector(X, Y, Z)); };
	K.Add(Plank(0.8f), FVector(Hw, 8.f, 220.f), HP(0, 130.f, 110.f), Hr);
	for (const float X : { -Hw * 0.5f, Hw * 0.5f }) K.Add(Plank(0.78f), FVector(8.f, 260.f, 220.f), HP(X, 0.f, 110.f), Hr);
	for (const float X : { -Hw * 0.5f, Hw * 0.5f }) for (const float Y : { -130.f, 130.f }) K.Rod(Timber(), HP(X, Y, 0.f), HP(X, Y, 240.f), 14.f, true);
	K.Add(Bark(0.9f), FVector(Hw + 60.f, 330.f, 10.f), HP(0, 0, 255.f), FRotator(0.f, DeckYaw, -12.f));
	for (int32 s = 0; s < 2; ++s) K.Add(Plank(0.85f), FVector(Hw - 40.f, 30.f, 4.f), HP(0, 110.f, 80.f + 60.f * s), Hr);
	Box(HP(0, 130.f, 110.f), FVector(Hw * 0.5f, 6.f, 110.f), Hr);
	for (const float X : { -Hw * 0.5f, Hw * 0.5f }) Box(HP(X, 0.f, 110.f), FVector(6.f, 130.f, 110.f), Hr);
	// Hides pegged out on two frames at the deck's other side, and strips of meat on a line between them.
	const FVector Fc = C - Side * 270.f;
	for (const float S : { -1.f, 1.f })
	{
		const FVector Fp = Fc + Dir * S * 140.f;
		for (const float Y : { -60.f, 60.f }) K.Rod(Timber(0.85f), Fp + Side * Y * 0.f + Dir * Y, Fp + Dir * Y + FVector(0, 0, 170.f), 8.f);
		K.Rod(Timber(0.85f), Fp - Dir * 60.f + FVector(0, 0, 165.f), Fp + Dir * 60.f + FVector(0, 0, 165.f), 7.f);
		K.Add(Hide(0.9f + 0.1f * S), FVector(110.f, 4.f, 140.f), Fp + FVector(0, 0, 90.f) + Side * 2.f, FRotator(0.f, DeckYaw, 0.f));
		for (int32 c = 0; c < 4; ++c) K.Rod(Rope(), Fp + Dir * (-55.f + 36.f * c) + FVector(0, 0, 160.f), Fp + Dir * (-50.f + 33.f * c) + FVector(0, 0, 150.f), 1.2f);
	}
	Line(Fc - Dir * 140.f + FVector(0, 0, 160.f), Fc + Dir * 140.f + FVector(0, 0, 160.f), 0.8f, 12.f, RopeC(), 8);
	for (int32 m = 0; m < 10; ++m) K.Add(FSurf::Cloth(Rgb(90, 40, 30) * 0.45f), FVector(6.f, 2.f, 38.f + 8.f * (m % 3)), Fc + Dir * (-120.f + 27.f * m) + FVector(0, 0, 132.f - 4.f * (m % 3)));
	// The stair: planks let into the first giant on iron-shod brackets, spiralling up 35 m to end facing the deck, a rope
	// rail on posts at the outer edge. Each tread is its own collision.
	const float Rise = 26.f;
	const int32 Treads = FMath::Clamp(FMath::CeilToInt((DeckTop - float(A.Z)) / Rise), 4, 400);
	const float StepA = 12.f;
	const float EndA = DeckYaw;
	const float StartA = EndA - StepA * (Treads - 1);
	TArray<FVector> Rail;
	for (int32 i = 0; i < Treads; ++i)
	{
		const float Ang = StartA + StepA * i;
		const FVector Rd = FRotator(0.f, Ang, 0.f).Vector();
		const float Z = FMath::Lerp(float(A.Z) + Rise, DeckTop, Treads > 1 ? float(i) / (Treads - 1) : 1.f);
		const FVector Mid = FVector(A.X, A.Y, Z) + Rd * (RA + 65.f);
		K.Add(Plank(0.82f + 0.18f * FMath::Frac(i * 0.37f)), FVector(130.f, 50.f, 6.f), Mid - FVector(0, 0, 3.f), FRotator(0.f, Ang, 0.f));
		K.Rod(Timber(0.85f), FVector(A.X, A.Y, Z - 70.f) + Rd * (RA - 10.f), Mid + Rd * 40.f - FVector(0, 0, 8.f), 9.f);
		if (i % 6 == 0) K.Add(Iron(), FVector(16.f, 20.f, 20.f), FVector(A.X, A.Y, Z - 70.f) + Rd * (RA - 6.f), FRotator(0.f, Ang, 0.f));
		Box(Mid - FVector(0, 0, 4.f), FVector(65.f, 26.f, 4.f), FRotator(0.f, Ang, 0.f));
		const FVector Outer = FVector(A.X, A.Y, Z) + Rd * (RA + 125.f);
		Rail.Add(Outer + FVector(0, 0, 100.f));
		if (i % 4 == 0) { K.Rod(Timber(0.8f), Outer, Outer + FVector(0, 0, 105.f), 8.f); Box(Outer + FVector(0, 0, 55.f), FVector(5.f, 5.f, 55.f)); }
	}
	if (Rail.Num() > 1) M.Tube(Rail, 2.5f, 5, RopeC(), 0.95f);
	// The landing: from the last tread across to the deck's edge.
	const FVector L0 = FVector(A.X, A.Y, DeckTop) + Dir * (RA + 130.f);
	const FVector L1 = C - Dir * HalfX;
	if (FVector::DotProduct(L1 - L0, Dir) > 10.f)
	{
		const float Ll = FVector::Dist2D(L0, L1);
		K.Add(Plank(0.85f), FVector(Ll + 20.f, 110.f, 6.f), (L0 + L1) * 0.5f - FVector(0, 0, 3.f), FRotator(0.f, DeckYaw, 0.f));
		for (const float S : { -1.f, 1.f }) K.Add(Timber(), FVector(Ll + 40.f, 16.f, 22.f), (L0 + L1) * 0.5f + Side * S * 45.f - FVector(0, 0, 17.f), FRotator(0.f, DeckYaw, 0.f));
		SegBox(L0 - FVector(0, 0, 6.f), L1 - FVector(0, 0, 6.f), 55.f, 6.f);
	}
}

void WRForestLook::FBuild::Sinkroot()
{
	using namespace WRForestVillImpl;
	// DC 2.4 #35 and 2.6 C5: the deepest cave, reached by a fall: a hole where roots have pulled the ground apart. When
	// this site is the cave's own (Theme set) the hole is at the cave's mouth; else at the origin over the terrain's pit.
	const bool bCave = !Act->Theme.IsNone();
	const FVector Hole = bCave ? At(MouthX(), 0.f) : At(0.f, 0.f);
	const float Radius = WRTune(TEXT("forestWorld.sinkrootRadius"), 520.f);
	Tear(*this, Hole, Radius);
	// A young beech slid in and wedged, roots in the air; ferns hanging over the lip; bones of something that fell.
	Limb(Hole + FVector(-Radius * 0.9f, 80.f, 20.f), Hole + FVector(-Radius * 0.2f, 40.f, -260.f), 16.f, 8.f, BeechC(), 7);
	Ball(Hole + FVector(-Radius * 1.0f, 90.f, 50.f), 60.f, Rgb(74, 56, 40) * 0.42f, 0.95f, 0.8f);
	for (int32 f = 0; f < 6; ++f)
	{
		const FVector Dir = FRotator(0.f, 60.f * f + 20.f, 0.f).Vector();
		Fern(*this, At(float(Hole.X + Dir.X * (Radius + 150.f)), float(Hole.Y + Dir.Y * (Radius + 150.f))), 1.1f, f);
	}
	for (int32 b = 0; b < 4; ++b) Limb(Hole + FVector(Radius * 1.05f, -40.f + 25.f * b, 30.f), Hole + FVector(Radius * 1.05f + 50.f, -30.f + 25.f * b, 34.f), 2.f, 1.5f, BoneC(), 5);
	Ball(Hole + FVector(Radius * 1.1f, 70.f, 38.f), 9.f, BoneC(), 0.8f, 0.9f);
	// A Grovekin warning cairn: three stacked stones with a hand print in ochre (don't).
	const FVector Cn = At(float(Hole.X) + Radius + 280.f, -200.f);
	for (int32 s = 0; s < 3; ++s) K.Add(OldStone(0.85f), FVector(60.f - 12.f * s, 50.f - 10.f * s, 30.f), Cn + FVector(0, 0, 15.f + 30.f * s), FRotator(0.f, 25.f * s, 0.f));
	K.Add(FSurf::Cloth(Rgb(160, 70, 40) * 0.5f), FVector(2.f, 16.f, 18.f), Cn + FVector(-28.f, 0, 50.f));
	Box(Cn + FVector(0, 0, 45.f), FVector(30.f, 25.f, 45.f));
	// Foxfire light up out of the hole at night: you see it from 30 m.
	if (!bCave) Lamp(Hole - FVector(0, 0, 300.f), FoxfireLight(), 700.f, 900.f);
}

// ================================================================================================ camps, the village, caves

void WRForestLook::FBuild::Camp()
{
	using namespace WRForestVillImpl;
	// A coastal camp (world-map: 3, where the newcomers come ashore; PlayerStarts and starter stations are placed at
	// import and stand round the origin, so 6 m round it stays clear): a ring fire with a pot on a tripod, log seats, a
	// lean-to of oak poles and hides facing the fire, a drying rack for fish and meat, a woodpile. Lit at night.
	const FVector Fire = At(900.f, 0.f);
	for (int32 s = 0; s < 9; ++s)
	{
		const float Ang = 40.f * s;
		K.Add(OldStone(0.7f + 0.1f * (s % 3)), FVector(36.f, 28.f, 22.f), Fire + FRotator(0.f, Ang, 0.f).Vector() * 70.f + FVector(0, 0, 8.f), FRotator(0.f, Ang + 90.f, 0.f));
	}
	K.Disc(Black(0.45f), Fire + FVector(0, 0, 3.f), FVector::UpVector, 120.f, 4.f);
	for (int32 l = 0; l < 4; ++l) Limb(Fire + FRotator(0.f, 90.f * l + 20.f, 0.f).Vector() * 50.f + FVector(0, 0, 6.f), Fire + FVector(0, 0, 30.f), 6.f, 4.f, l % 2 ? BlackC() : BarkC(), 6);
	Loop(TEXT("forest.campfire"), Fire + FVector(0, 0, 10.f));
	Lamp(Fire + FVector(0, 0, 70.f), WarmLight(), WRTune(TEXT("forestWorld.campLight"), 3500.f), 1600.f);
	// The tripod and pot.
	for (int32 l = 0; l < 3; ++l) K.Rod(Timber(0.75f), Fire + FRotator(0.f, 120.f * l, 0.f).Vector() * 80.f, Fire + FVector(0, 0, 150.f), 5.f);
	K.Rod(Iron(), Fire + FVector(0, 0, 150.f), Fire + FVector(0, 0, 95.f), 1.f);
	Ball(Fire + FVector(0, 0, 75.f), 20.f, Rgb(40, 38, 36) * 0.5f, 0.5f, 0.85f);
	Box(Fire + FVector(0, 0, 12.f), FVector(80.f, 80.f, 12.f));
	// Log seats round the fire.
	for (int32 s = 0; s < 3; ++s)
	{
		const float Ang = 70.f + 110.f * s;
		const FVector Sp = Fire + FRotator(0.f, Ang, 0.f).Vector() * 230.f;
		const FVector Sd = FRotator(0.f, Ang + 90.f, 0.f).Vector() * 90.f;
		const FVector P0(Sp.X - Sd.X, Sp.Y - Sd.Y, G(float(Sp.X - Sd.X), float(Sp.Y - Sd.Y)) + 18.f);
		const FVector P1(Sp.X + Sd.X, Sp.Y + Sd.Y, G(float(Sp.X + Sd.X), float(Sp.Y + Sd.Y)) + 18.f);
		Limb(P0, P1, 22.f, 20.f, BarkC(), 9);
		SegBox(P0, P1, 20.f, 20.f);
	}
	// The lean-to: two forked posts, a ridge pole, poles raked back to the ground, hides lashed over; bedding inside.
	const FVector Lt = At(1350.f, -520.f);
	const float LYaw = (Fire - Lt).Rotation().Yaw;
	const FRotator Lr(0.f, LYaw, 0.f);
	const auto LP = [&](float X, float Y, float Z) { return Lt + Lr.RotateVector(FVector(X, Y, Z)); };
	for (const float Y : { -150.f, 150.f })
	{
		K.Rod(Timber(0.8f), LP(60.f, Y, -20.f), LP(60.f, Y, 170.f), 12.f);
		K.Rod(Timber(0.75f), LP(60.f, Y, 150.f), LP(70.f, Y + 12.f, 185.f), 6.f);
	}
	K.Rod(Timber(0.85f), LP(60.f, -180.f, 168.f), LP(60.f, 180.f, 168.f), 10.f);
	for (int32 p = 0; p < 5; ++p) K.Rod(Timber(0.75f), LP(62.f, -140.f + 70.f * p, 168.f), LP(-170.f, -140.f + 70.f * p, 0.f), 7.f);
	const float Slope = FMath::RadiansToDegrees(FMath::Atan2(168.f, 232.f));
	K.Add(Hide(), FVector(290.f, 330.f, 4.f), LP(-55.f, 0.f, 88.f), FRotator(Slope, LYaw, 0.f));
	for (const float Y : { -165.f, 165.f }) K.Add(Hide(0.85f), FVector(230.f, 4.f, 150.f), LP(-50.f, Y, 60.f), Lr);
	K.Add(Straw(), FVector(180.f, 240.f, 10.f), LP(-60.f, 0.f, 5.f), Lr);
	K.Add(Canvas(0.9f), FVector(160.f, 90.f, 12.f), LP(-60.f, -50.f, 14.f), Lr);
	Box(LP(-55.f, 0.f, 88.f), FVector(145.f, 165.f, 4.f), FRotator(Slope, LYaw, 0.f));
	// The drying rack: two A-frames and a pole, fish and meat strips on it, a hide stretched on a hoop beside.
	const FVector Dr = At(1150.f, 560.f);
	for (const float Y : { -120.f, 120.f })
	{
		K.Rod(Timber(0.8f), Dr + FVector(-50.f, Y, 0.f), Dr + FVector(0.f, Y, 170.f), 7.f);
		K.Rod(Timber(0.8f), Dr + FVector(50.f, Y, 0.f), Dr + FVector(0.f, Y, 170.f), 7.f);
	}
	K.Rod(Timber(0.85f), Dr + FVector(0, -140.f, 165.f), Dr + FVector(0, 140.f, 165.f), 6.f);
	for (int32 f = 0; f < 8; ++f)
	{
		const FVector Hang = Dr + FVector(0, -105.f + 30.f * f, 160.f);
		if (f % 2) K.Add(FSurf::Stone(Rgb(140, 140, 130) * 0.5f, 0.5f), FVector(8.f, 3.f, 45.f), Hang - FVector(0, 0, 24.f));   // split fish
		else K.Add(FSurf::Cloth(Rgb(90, 40, 30) * 0.45f), FVector(6.f, 2.f, 40.f), Hang - FVector(0, 0, 22.f));             // meat strip
	}
	Box(Dr + FVector(0, 0, 85.f), FVector(30.f, 130.f, 85.f));
	// The woodpile under a bark slab.
	const FVector Wp = At(600.f, -650.f);
	for (int32 r = 0; r < 3; ++r) for (int32 l = 0; l < 6 - r; ++l) Limb(Wp + FVector(-60.f, -75.f + 30.f * l + 15.f * r, 14.f + 26.f * r), Wp + FVector(60.f, -75.f + 30.f * l + 15.f * r, 14.f + 26.f * r), 13.f, 12.f, BarkC() * (0.9f + 0.1f * (l % 2)), 7);
	K.Add(Bark(0.8f), FVector(150.f, 200.f, 6.f), Wp + FVector(0, 0, 95.f), FRotator(0.f, 0.f, 6.f));
	Box(Wp + FVector(0, 0, 45.f), FVector(62.f, 95.f, 45.f));
}

void WRForestLook::FBuild::Village()
{
	using namespace WRForestVillImpl;
	// The newcomers' village (world-map: the largest settlement; its safe zone is UWRForestWorld's, VillageRadius round
	// the Village point): thatched cottages round a plaza, lanes worn in, fresh field walls, a well, a market stall, a
	// smith's lean-to with an armour stand, and the meeting hall built in the Heartwood lodge style (squared heartwood on
	// a stone sill, steel-pinned: the building set the forest teaches, standing in the world). They came off ships: a
	// keel is the hall's ridge, an upturned hull is a shed. Lit at night. The plaza (6 m round the origin) stays clear.
	const float VR = FMath::Max(4000.f, FWRForestData::Get().VillageRadius);
	const float Rin = VR * 0.22f, Rout = VR * 0.42f;
	// The plaza: flagstones in two rings, worn in the middle.
	for (int32 rr = 0; rr < 2; ++rr)
	{
		const int32 Count = 14 + 10 * rr;
		for (int32 f = 0; f < Count; ++f)
		{
			const float Ang = 360.f * f / Count + 7.f * rr;
			const float Rf = 260.f + 220.f * rr;
			K.Add(OldStone(0.82f + 0.12f * FMath::Frac(f * 0.37f)), FVector(170.f, 120.f, 8.f), At(FRotator(0.f, Ang, 0.f).Vector().X * Rf, FRotator(0.f, Ang, 0.f).Vector().Y * Rf, 2.f), FRotator(0.f, Ang, 0.f));
		}
	}
	for (int32 l = 0; l < 4; ++l) LanternPost(*this, At(FRotator(0.f, 45.f + 90.f * l, 0.f).Vector().X * 700.f, FRotator(0.f, 45.f + 90.f * l, 0.f).Vector().Y * 700.f));
	// The cottages: an inner ring of four and an outer ring of seven, doors to the plaza, each with a fenced yard behind.
	const float InnerA[4] = { 45.f, 115.f, 245.f, 315.f };
	const float OuterA[7] = { 20.f, 70.f, 140.f, 200.f, 250.f, 300.f, 340.f };
	int32 Var = 0;
	const auto House = [&](float Ang, float Rr)
	{
		const FVector Dir = FRotator(0.f, Ang, 0.f).Vector();
		const FVector Hp = At(float(Dir.X) * Rr, float(Dir.Y) * Rr);
		Cottage(Hp, Ang + 180.f, Var, false);
		// The lane from the plaza to the door, worn bare.
		const float Door = Rr - 480.f;
		const int32 Segs = FMath::Max(1, FMath::RoundToInt((Door - 700.f) / 300.f));
		for (int32 s = 0; s < Segs; ++s)
		{
			const float Rs = 700.f + (Door - 700.f) * (s + 0.5f) / Segs;
			K.Add(Earth(0.85f + 0.1f * (s % 2)), FVector((Door - 700.f) / Segs + 20.f, 200.f, 6.f), At(float(Dir.X) * Rs, float(Dir.Y) * Rs, 1.f), FRotator(0.f, Ang, 0.f));
		}
		// The yard behind: a split-rail fence round a kitchen garden, and a water butt by the back wall.
		const FVector Side = FRotator(0.f, Ang + 90.f, 0.f).Vector();
		const FVector Back0 = Dir * (Rr + 420.f), Back1 = Dir * (Rr + 1300.f);
		Fence(*this, Back0 - Side * 500.f, Back1 - Side * 500.f);
		Fence(*this, Back1 - Side * 500.f, Back1 + Side * 500.f);
		Fence(*this, Back1 + Side * 500.f, Back0 + Side * 500.f);
		for (int32 h = 0; h < 6; ++h) Herb(*this, At(float(Dir.X * (Rr + 700.f + 150.f * (h / 3)) + Side.X * (-250.f + 250.f * (h % 3))), float(Dir.Y * (Rr + 700.f + 150.f * (h / 3)) + Side.Y * (-250.f + 250.f * (h % 3)))), h + Var);
		Barrel(*this, At(float(Dir.X * (Rr + 380.f) + Side.X * 300.f), float(Dir.Y * (Rr + 380.f) + Side.Y * 300.f)), 1.f);
		++Var;
	};
	for (const float Ang : InnerA) House(Ang, Rin);
	for (const float Ang : OuterA) House(Ang, Rout);

	// The meeting hall, west of the plaza, its doors to it: the Heartwood lodge style.
	{
		const FVector Hb = At(-Rin, 0.f);
		const float L = 1400.f, W = 800.f, H = 380.f;
		const auto HP = [&](float X, float Y, float Z) { return Hb + FVector(X, Y, Z); };
		// The sill: dressed stone in courses, levelled on the slope.
		float Gmin = 1e9f, Gmax = -1e9f;
		for (int32 s = 0; s < 9; ++s) { const float Gs = G(float(Hb.X) - L * 0.5f + L * 0.5f * (s % 3), -W * 0.5f + W * 0.5f * (s / 3)); Gmin = FMath::Min(Gmin, Gs); Gmax = FMath::Max(Gmax, Gs); }
		const float Floor = Gmax + 45.f;
		const float Zs = Floor - float(Hb.Z);
		K.Add(OldStone(0.9f), FVector(L + 40.f, W + 40.f, Floor - Gmin + 30.f), HP(0, 0, Zs - (Floor - Gmin + 30.f) * 0.5f));
		K.Add(Plank(0.8f), FVector(L - 40.f, W - 40.f, 6.f), HP(0, 0, Zs + 3.f));
		Box(HP(0, 0, Zs - (Floor - Gmin) * 0.5f), FVector(L * 0.5f + 20.f, W * 0.5f + 20.f, (Floor - Gmin) * 0.5f + 4.f));
		// The frame: squared heartwood posts every 2.8 m, sill and wall plates, tie beams, steel pins at every joint.
		for (const float Y : { -W * 0.5f, W * 0.5f })
		{
			for (int32 p = 0; p <= 5; ++p)
			{
				const float X = -L * 0.5f + L * p / 5.f;
				LodgePost(*this, HP(X, Y, Zs), H, 0.f);
			}
			K.Add(Heartwood(0.85f), FVector(L + 30.f, 32.f, 32.f), HP(0, Y, Zs + H + 16.f));
			K.Add(Heartwood(0.85f), FVector(L + 30.f, 30.f, 24.f), HP(0, Y, Zs + 12.f));
			// The infill: heartwood planks set vertical between the posts, windows lit at night in each bay but the ends.
			for (int32 b = 0; b < 5; ++b)
			{
				const float Xc = -L * 0.5f + L * (b + 0.5f) / 5.f;
				K.Add(Heartwood(0.7f + 0.08f * (b % 2)), FVector(L / 5.f - 34.f, 10.f, H - 30.f), HP(Xc, Y, Zs + H * 0.5f + 6.f));
				if (b > 0 && b < 4)
				{
					K.Add(Heartwood(0.6f), FVector(90.f, 14.f, 80.f), HP(Xc, Y, Zs + 220.f));
					N.Add(Window(0.9f), FVector(70.f, 16.f, 60.f), HP(Xc, Y, Zs + 220.f));
				}
			}
			Box(HP(0, Y, Zs + H * 0.5f), FVector(L * 0.5f, 12.f, H * 0.5f));
		}
		for (int32 p = 0; p <= 5; ++p) K.Add(Heartwood(0.8f), FVector(28.f, W + 30.f, 28.f), HP(-L * 0.5f + L * p / 5.f, 0, Zs + H + 46.f));
		// The gables: the back closed; the front (+X, toward the plaza) with double doors standing open.
		K.Add(Heartwood(0.72f), FVector(10.f, W - 30.f, H - 30.f), HP(-L * 0.5f, 0, Zs + H * 0.5f + 6.f));
		Box(HP(-L * 0.5f, 0, Zs + H * 0.5f), FVector(12.f, W * 0.5f, H * 0.5f));
		for (const float S : { -1.f, 1.f })
		{
			K.Add(Heartwood(0.72f), FVector(10.f, W * 0.5f - 110.f, H - 30.f), HP(L * 0.5f, S * (W * 0.25f + 55.f), Zs + H * 0.5f + 6.f));
			Box(HP(L * 0.5f, S * (W * 0.25f + 55.f), Zs + H * 0.5f), FVector(12.f, W * 0.25f - 55.f, H * 0.5f));
			K.Add(Heartwood(0.65f), FVector(100.f, 8.f, 250.f), HP(L * 0.5f + 50.f, S * 110.f, Zs + 125.f), FRotator(0.f, S * 70.f, 0.f));
			for (const float Zp : { 50.f, 200.f }) K.Add(Iron(), FVector(60.f, 10.f, 8.f), HP(L * 0.5f + 25.f, S * 105.f, Zs + Zp), FRotator(0.f, S * 70.f, 0.f));
		}
		for (int32 s = 0; s < 4; ++s) K.Add(Heartwood(0.6f + 0.05f * s), FVector(10.f, W - 40.f - 120.f * s, 60.f), HP(-L * 0.5f, 0, Zs + H + 60.f + 60.f * s));
		for (int32 s = 0; s < 4; ++s) K.Add(Heartwood(0.6f + 0.05f * s), FVector(10.f, W - 40.f - 120.f * s, 60.f), HP(L * 0.5f, 0, Zs + H + 60.f + 60.f * s));
		// The roof: shingles at 45 degrees, and the ridge a ship's keel (the newcomers' ship, carried up from the wreck),
		// its stem post curling up over the front gable and a figurehead under it.
		const float RoofPitch = 45.f;
		const float SlopeLen = (W * 0.5f + 70.f) / FMath::Cos(FMath::DegreesToRadians(RoofPitch));
		for (const float S : { -1.f, 1.f }) K.Add(Shingle(0.9f + 0.1f * S), FVector(L + 120.f, SlopeLen, 22.f), HP(0, S * (W * 0.25f + 25.f), Zs + H + 40.f + (W * 0.25f + 25.f)), FRotator(0.f, 0.f, -S * RoofPitch));
		const float Ridge = Zs + H + 40.f + W * 0.5f + 70.f;
		K.Add(Timber(0.55f), FVector(L + 160.f, 30.f, 40.f), HP(0, 0, Ridge));
		Root({ HP(L * 0.5f + 80.f, 0, Ridge), HP(L * 0.5f + 160.f, 0, Ridge + 40.f), HP(L * 0.5f + 200.f, 0, Ridge + 140.f), HP(L * 0.5f + 180.f, 0, Ridge + 210.f) }, 18.f, 12.f, Rgb(70, 52, 38) * 0.5f, 8);
		// The figurehead: a carved woman leaning forward, salt-bleached, her paint gone.
		const FVector Fh = HP(L * 0.5f + 150.f, 0, Ridge - 90.f);
		M.Lathe(Fh, FVector(0.4f, 0.f, 1.f).GetSafeNormal(), 110.f, { FVector(0.f, 14.f, 12.f), FVector(0.35f, 18.f, 16.f), FVector(0.55f, 13.f, 10.f), FVector(0.72f, 6.f, 6.f), FVector(0.86f, 10.f, 10.f), FVector(1.f, 2.f, 2.f) }, 10, BoneC() * 0.85f, 0.7f);
		for (int32 l = 0; l < 2; ++l) N.Add(Window(0.8f), FVector(30.f, 30.f, 30.f), HP(-L * 0.25f + L * 0.5f * l, 0, Zs + 260.f));
		Lamp(HP(-L * 0.2f, 0, Zs + 250.f), WarmLight(), WRTune(TEXT("forestWorld.villageLight"), 2800.f), 1200.f);
		Lamp(HP(L * 0.25f, 0, Zs + 250.f), WarmLight(), WRTune(TEXT("forestWorld.villageLight"), 2800.f), 1200.f);
		Loop(TEXT("forest.chimney"), HP(-L * 0.3f, 0, Ridge + 60.f));
	}
	// The well: a stone ring, two posts and a roof, the windlass and its bucket.
	{
		const FVector Wc = At(900.f, -900.f);
		for (int32 s = 0; s < 12; ++s)
		{
			const float Ang = 30.f * s;
			for (int32 c = 0; c < 3; ++c) K.Add(OldStone(0.8f + 0.15f * ((s + c) % 2)), FVector(48.f, 30.f, 28.f), Wc + FRotator(0.f, Ang + 15.f * c, 0.f).Vector() * 95.f + FVector(0, 0, 14.f + 28.f * c), FRotator(0.f, Ang + 15.f * c + 90.f, 0.f));
		}
		K.Disc(Water(), Wc + FVector(0, 0, -40.f), FVector::UpVector, 160.f, 2.f);
		Box(Wc + FVector(0, 0, 42.f), FVector(110.f, 110.f, 42.f));
		for (const float Y : { -110.f, 110.f }) K.Rod(Timber(0.85f), Wc + FVector(0, Y, 0.f), Wc + FVector(0, Y, 230.f), 14.f, true);
		K.Rod(Timber(0.8f), Wc + FVector(0, -120.f, 150.f), Wc + FVector(0, 130.f, 150.f), 18.f);
		K.Rod(Iron(), Wc + FVector(0, 130.f, 150.f), Wc + FVector(30.f, 140.f, 170.f), 3.f);
		K.Rod(Rope(), Wc + FVector(0, 0, 141.f), Wc + FVector(0, 0, 70.f), 2.f);
		M.Lathe(Wc + FVector(0, 0, 40.f), FVector::UpVector, 32.f, { FVector(0.f, 13.f, 13.f), FVector(1.f, 16.f, 16.f) }, 10, Rgb(110, 82, 56) * 0.45f, 0.8f);
		for (const float S : { -1.f, 1.f }) K.Add(Thatch(), FVector(70.f, 170.f, 16.f), Wc + FVector(S * 30.f, 0, 250.f), FRotator(0.f, 0.f, 0.f) + FRotator(S * -35.f, 0.f, 0.f));
	}
	// The market stall (the merchants' stop): a counter under a striped awning, baskets of apples, barrels and crates.
	{
		const FVector Mc = At(1000.f, 1000.f);
		const float MYaw = (FVector::ZeroVector - Mc).Rotation().Yaw;
		const FRotator Mr(0.f, MYaw, 0.f);
		const auto MP = [&](float X, float Y, float Z) { return Mc + Mr.RotateVector(FVector(X, Y, Z)); };
		for (const float X : { -80.f, 80.f }) for (const float Y : { -160.f, 160.f }) K.Rod(Timber(0.85f), MP(X, Y, -20.f), MP(X, Y, X > 0.f ? 230.f : 260.f), 12.f, true);
		K.Add(Plank(0.85f), FVector(70.f, 340.f, 8.f), MP(70.f, 0, 95.f), Mr);
		K.Add(Plank(0.75f), FVector(8.f, 330.f, 90.f), MP(102.f, 0, 50.f), Mr);
		for (int32 s = 0; s < 8; ++s) K.Add(s % 2 ? Canvas(1.1f) : FSurf::Cloth(Rgb(150, 50, 40) * 0.45f), FVector(210.f, 44.f, 4.f), MP(0, -154.f + 44.f * s, 247.f), FRotator(-8.f, MYaw, 0.f));
		for (int32 b = 0; b < 3; ++b)
		{
			const FVector Bk = MP(70.f, -110.f + 110.f * b, 99.f);
			M.Lathe(Bk, FVector::UpVector, 22.f, { FVector(0.f, 18.f, 18.f), FVector(1.f, 24.f, 24.f) }, 12, StrawC() * 0.8f, 0.95f);
			for (int32 a = 0; a < 5; ++a) Ball(Bk + FVector(8.f * (a % 3 - 1), 8.f * (a / 3), 26.f), 5.f, b == 1 ? Rgb(214, 160, 40) * 0.7f : Rgb(170, 50, 30) * 0.6f, 0.5f, 1.f);
		}
		Barrel(*this, MP(-60.f, 200.f, 0.f), 1.f);
		Barrel(*this, MP(-60.f, 260.f, 0.f), 0.9f);
		K.Add(Timber(0.7f), FVector(70.f, 70.f, 60.f), MP(-40.f, -220.f, 30.f), FRotator(0.f, MYaw + 12.f, 0.f));
		Box(MP(70.f, 0, 50.f), FVector(35.f, 170.f, 50.f), Mr);
		LanternPost(*this, MP(-100.f, -230.f, 0.f));
	}
	// The smith's lean-to: thatch on four posts, a stone forge with a hood and bellows, an anvil on an oak stump, the
	// quench tub, and an armour stand wearing a Heartguard cuirass (bark plates of lacquered heartwood, amber cores).
	{
		const FVector Sm = At(0.f, -Rin);
		const FRotator Sr(0.f, 90.f, 0.f);
		const auto SP = [&](float X, float Y, float Z) { return Sm + Sr.RotateVector(FVector(X, Y, Z)); };
		for (const float X : { -200.f, 200.f }) for (const float Y : { -150.f, 150.f }) K.Rod(Timber(0.85f), SP(X, Y, -20.f), SP(X, Y, Y < 0.f ? 300.f : 230.f), 16.f, true);
		K.Add(Thatch(), FVector(480.f, 380.f, 28.f), SP(0, 0, 275.f), FRotator(0.f, 90.f, -10.f));
		const FVector Fg = SP(-120.f, -90.f, 0.f);
		for (int32 c = 0; c < 4; ++c) K.Add(OldStone(0.7f + 0.05f * c), FVector(130.f, 110.f, 25.f), Fg + FVector(0, 0, 12.f + 25.f * c));
		K.Disc(Black(0.4f), Fg + FVector(0, 0, 101.f), FVector::UpVector, 70.f, 2.f);
		N.Add(FSurf::Lit(FLinearColor(1.f, 0.35f, 0.08f), 1.f), FVector(50.f, 50.f, 4.f), Fg + FVector(0, 0, 103.f));
		for (int32 c = 0; c < 6; ++c) K.Add(OldStone(0.65f), FVector(100.f - 12.f * c, 90.f - 10.f * c, 30.f), Fg + FVector(0, 0, 150.f + 30.f * c));
		K.Add(Hide(0.8f), FVector(60.f, 40.f, 25.f), Fg + FVector(90.f, 40.f, 90.f), FRotator(10.f, 0.f, 0.f));   // the bellows
		Box(Fg + FVector(0, 0, 55.f), FVector(65.f, 55.f, 55.f));
		Loop(TEXT("forest.chimney"), Fg + FVector(0, 0, 330.f));
		Lamp(Fg + FVector(0, 0, 140.f), FLinearColor(1.f, 0.45f, 0.15f), WRTune(TEXT("forestWorld.forgeLight"), 2500.f), 900.f);
		const FVector Av = SP(60.f, 40.f, 0.f);
		Limb(Av, Av + FVector(0, 0, 60.f), 30.f, 28.f, BarkC(), 10);
		K.Add(Iron(), FVector(70.f, 26.f, 22.f), Av + FVector(0, 0, 76.f));
		K.Add(Iron(), FVector(30.f, 20.f, 16.f), Av + FVector(0, 0, 63.f));
		K.Spike(Iron(), Av + FVector(35.f, 0, 80.f), FVector(1.f, 0, 0.1f), 18.f, 30.f);
		Box(Av + FVector(0, 0, 44.f), FVector(30.f, 30.f, 44.f));
		Barrel(*this, SP(150.f, 100.f, 0.f), 1.f);
		// The armour stand.
		const FVector As = SP(160.f, -100.f, 0.f);
		K.Rod(Timber(0.8f), As, As + FVector(0, 0, 170.f), 8.f);
		K.Rod(Timber(0.8f), As + FVector(0, -40.f, 145.f), As + FVector(0, 40.f, 145.f), 6.f);
		M.Lathe(As + FVector(0, 0, 95.f), FVector::UpVector, 55.f, { FVector(0.f, 22.f, 14.f), FVector(0.5f, 26.f, 17.f), FVector(1.f, 24.f, 15.f) }, 10, HeartwoodC() * 1.1f, 0.4f);
		for (const float S : { -1.f, 1.f })
		{
			K.Add(Heartwood(1.1f), FVector(26.f, 34.f, 10.f), As + FVector(0, S * 32.f, 152.f), FRotator(0.f, 0.f, S * 20.f));   // pauldrons
			K.Add(Heartwood(1.f), FVector(22.f, 30.f, 8.f), As + FVector(0, S * 36.f, 143.f), FRotator(0.f, 0.f, S * 30.f));
		}
		K.Disc(Amber(0.25f), As + FVector(18.f, 0, 128.f), FVector(1, 0, 0), 14.f, 3.f);
		for (int32 f = 0; f < 5; ++f) K.Add(Leaf(0.8f), FVector(4.f, 12.f, 34.f), As + FVector(12.f, -24.f + 12.f * f, 82.f));   // leaf-strip skirt
		Box(As + FVector(0, 0, 85.f), FVector(25.f, 40.f, 85.f));
		Box(SP(0, 0, 275.f), FVector(240.f, 190.f, 14.f), FRotator(0.f, 90.f, -10.f));
	}
	// The upturned boat: a ship's boat hauled up and turned over for a shed, a door cut in its side.
	{
		const FVector Bt = At(Rin * 0.9f, Rin * 0.75f);
		const FRotator Br(0.f, 30.f, 0.f);
		M.Lathe(Bt - Br.Vector() * 300.f, Br.Vector(), 600.f, { FVector(0.f, 10.f, 10.f), FVector(0.15f, 90.f, 70.f), FVector(0.5f, 120.f, 95.f), FVector(0.85f, 90.f, 70.f), FVector(1.f, 10.f, 10.f) }, 14, Rgb(56, 46, 38) * 0.5f, 0.75f);
		Limb(Bt - Br.Vector() * 310.f + FVector(0, 0, 95.f), Bt + Br.Vector() * 310.f + FVector(0, 0, 95.f), 9.f, 9.f, Rgb(70, 52, 38) * 0.5f, 6);
		K.Add(Black(0.4f), FVector(90.f, 8.f, 80.f), Bt + Br.RotateVector(FVector(0, 118.f, 30.f)), Br);
		K.Add(Timber(0.7f), FVector(8.f, 90.f, 80.f), Bt + Br.RotateVector(FVector(60.f, 150.f, 30.f)), FRotator(0.f, 30.f + 60.f, 0.f));
		Box(Bt + FVector(0, 0, 45.f), FVector(300.f, 115.f, 60.f), Br);
	}
	// The field walls: new dry-stone walls in four runs round the village (the newcomers' fields), a gap on each road.
	for (int32 q = 0; q < 4; ++q)
	{
		const float R0 = VR * 0.58f;
		for (int32 s = 0; s < 6; ++s)
		{
			const float A0 = 90.f * q + 12.f + 11.f * s, A1 = A0 + 11.f;
			DryWall(*this, FRotator(0.f, A0, 0.f).Vector() * R0, FRotator(0.f, A1, 0.f).Vector() * R0, 100.f, 0.f);
		}
	}
}

void WRForestLook::FBuild::CaveMouth()
{
	using namespace WRForestVillImpl;
	// A cave of the forest (DC 2.6): AWRCave builds its mound and chamber here; this dresses the ground at its mouth by
	// the cave's theme (badger_sett, alder_grotto, root_mine, foxfire_cellar, sinkroot).
	Mouth(*this, Act->Theme, MouthX());
}
