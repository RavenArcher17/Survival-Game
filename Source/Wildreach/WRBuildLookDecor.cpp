// Modelled furniture: the comfort decor of survival-crafting 2.2 and the 12 biome sets' pieces (cooks/build-building.md
// 2.4 / 2.5). Every piece is built at real furniture sizes (a seat at 45 cm, a table top at 76, a bed's top at 55) in its
// set's colours, facing +X (a sitter looks along +X, a back stands at -X), standing on Z = 0. It collides as the box
// round what it draws (FWRBuildLook::DecorCollision) and a lamp's light sits in its flame (DecorLightAt).

#include "WRBuildLookKit.h"
#include "WRBuilding.h"
#include "WRBuildSets.h"

namespace WRBuildLookDecorDetail
{
	using namespace WRLookKit;

	struct FDecor : FKit
	{
		FDecor(const FTransform& InWorld, TArray<FWRLookBlock>& InOut, const FWRPieceDef& InDef)
			: FKit(InWorld, InOut), Def(InDef), Arch(InDef.Arch), Var(InDef.Variant)
		{
			Tier = FWRBuildSets::Get().Tier(InDef.Tier);
			if (Tier)
			{
				const FPalette P(Tier->Look);
				Main = P.A; Second = P.B; Accent = P.C; Glass = P.Glass; Dark = P.Dark;
				Glow = FSurf::Lit(Tier->Look.Glow.GetLuminance() > 0.01f ? Tier->Look.Glow : FLinearColor(1.f, 0.55f, 0.2f), 0.9f);
				Cloth = FSurf::Cloth(FMath::Lerp(Tier->Look.C, Tier->Look.B, 0.3f));
				Wood = FSurf::Wood(Tier->Look.Style == TEXT("timberframe") || Tier->Look.Style == TEXT("logs") || Tier->Look.Style == TEXT("reeds") ? Tier->Look.A : FLinearColor(0.2f, 0.12f, 0.06f));
			}
			else
			{
				Main = FSurf::Wood(FLinearColor(0.28f, 0.17f, 0.085f));
				Wood = Main;
				Second = FSurf::Stone(FLinearColor(0.25f, 0.24f, 0.22f));
				Accent = FSurf::Stone(FLinearColor(0.035f, 0.033f, 0.03f), 0.45f);
				Glass = FSurf::Glass(FLinearColor(0.35f, 0.42f, 0.45f));
				Dark = FSurf::Stone(FLinearColor(0.03f, 0.025f, 0.02f), 0.95f);
				Glow = FSurf::Lit(FLinearColor(1.f, 0.55f, 0.2f), 0.9f);
				Cloth = FSurf::Cloth(FLinearColor(0.32f, 0.12f, 0.08f));
			}
			Iron = FSurf::Stone(FLinearColor(0.025f, 0.024f, 0.024f), 0.45f);
			Brass = FSurf::Steel(FLinearColor(0.55f, 0.36f, 0.12f), 0.32f);
			Copper = FSurf::Steel(FLinearColor(0.55f, 0.24f, 0.1f), 0.38f);
			Water = FSurf::Glass(FLinearColor(0.08f, 0.2f, 0.24f), 0.04f);
			Hide = FSurf::Cloth(FLinearColor(0.2f, 0.13f, 0.07f));
			Fur = FSurf::Cloth(FLinearColor(0.32f, 0.3f, 0.28f));
			Leaf = FSurf::Cloth(FLinearColor(0.07f, 0.16f, 0.04f));
		}
		const FWRPieceDef& Def;
		FName Arch, Var;
		const FWRTierDef* Tier = nullptr;
		FSurf Main, Second, Accent, Glass, Dark, Glow, Cloth, Wood, Iron, Brass, Copper, Water, Hide, Fur, Leaf;
		bool V(const TCHAR* S) const { return Var == FName(S); }
		bool T(const TCHAR* S) const { return Tier && Tier->Id == FName(S); }

		/** Something round (an orb, a puffball, a jar's body) from three crossed cylinders. */
		void Ball(const FSurf& S, const FVector& C, float D)
		{
			Add(S, FVector(D, D, D * 0.62f), C, FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			Add(S, FVector(D * 0.82f, D * 0.82f, D * 0.86f), C, FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			Add(S, FVector(D * 0.52f, D * 0.52f, D), C, FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		}
		/** Four legs under a top from X0..X1 x Y0..Y1, H tall, Thick square. */
		void Legs(const FSurf& S, float X0, float X1, float Y0, float Y1, float H, float Thick)
		{
			for (const float X : { X0, X1 }) for (const float Y : { Y0, Y1 }) Box(S, FVector(X - Thick * 0.5f, Y - Thick * 0.5f, 0.f), FVector(X + Thick * 0.5f, Y + Thick * 0.5f, H));
		}
		/** A candle on a holder: wax, wick and its flame. */
		void Candle(const FVector& Base, float H)
		{
			Add(FSurf::Stone(FLinearColor(0.75f, 0.68f, 0.52f), 0.6f), FVector(4.f, 4.f, H), Base + FVector(0.f, 0.f, H * 0.5f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			Spike(FSurf::Lit(FLinearColor(1.f, 0.62f, 0.22f), 1.4f), Base + FVector(0.f, 0.f, H + 0.5f), FVector::UpVector, 2.4f, 5.f);
		}
		/** A cushion: a soft block a little rounded at the ends. */
		void Cushion(const FSurf& S, const FVector& Min, const FVector& Max)
		{
			Box(S, Min + FVector(2.f, 0.f, 0.f), Max - FVector(2.f, 0.f, 0.f));
			const FVector Size = Max - Min;
			for (const float X : { Min.X + 2.f, Max.X - 2.f }) Add(S, FVector(Size.Z, Size.Z, Size.Y), FVector(X, (Min.Y + Max.Y) * 0.5f, (Min.Z + Max.Z) * 0.5f), FRotator(0.f, 0.f, 90.f), EWRLookMesh::Cylinder);
		}
		/** A frame of two posts and a beam (bells, chimes, racks, hammocks hang from it). */
		void Gallows(const FSurf& S, float HalfW, float H, float Thick)
		{
			for (const float Y : { -HalfW, HalfW })
			{
				Box(S, FVector(-Thick * 0.5f, Y - Thick * 0.5f, 0.f), FVector(Thick * 0.5f, Y + Thick * 0.5f, H));
				Box(S, FVector(-22.f, Y - Thick * 0.6f, 0.f), FVector(22.f, Y + Thick * 0.6f, 6.f));   // feet
			}
			Box(S, FVector(-Thick * 0.5f, -HalfW - Thick * 0.5f, H - Thick), FVector(Thick * 0.5f, HalfW + Thick * 0.5f, H));
		}

		void Build()
		{
			const FName A = Arch;
			if (A == TEXT("throne")) Throne();
			else if (A == TEXT("bench")) Bench();
			else if (A == TEXT("stool")) Stool();
			else if (A == TEXT("table")) Table();
			else if (A == TEXT("anvil_table")) AnvilTable();
			else if (A == TEXT("desk")) Desk();
			else if (A == TEXT("lamp_stand")) LampStand();
			else if (A == TEXT("rod_lamp")) RodLamp();
			else if (A == TEXT("lantern_hang")) LanternHang();
			else if (A == TEXT("lamp_float")) LampFloat();
			else if (A == TEXT("crystal_lamp")) CrystalLamp();
			else if (A == TEXT("glowcap")) Glowcap();
			else if (A == TEXT("grub_jar")) GrubJar();
			else if (A == TEXT("toad_lamp")) ToadLamp();
			else if (A == TEXT("lantern_tree")) LanternTree();
			else if (A == TEXT("chandelier")) Chandelier();
			else if (A == TEXT("candle_rack")) CandleRack();
			else if (A == TEXT("candles")) Candles();
			else if (A == TEXT("lantern")) Lantern();
			else if (A == TEXT("fireplace")) Fireplace();
			else if (A == TEXT("cauldron")) Cauldron();
			else if (A == TEXT("bath")) Bath();
			else if (A == TEXT("fountain")) Fountain();
			else if (A == TEXT("bed")) Bed();
			else if (A == TEXT("hammock")) Hammock();
			else if (A == TEXT("divan")) Divan();
			else if (A == TEXT("shelf")) Shelf();
			else if (A == TEXT("chimes")) Chimes();
			else if (A == TEXT("bell")) Bell();
			else if (A == TEXT("drum")) Drum();
			else if (A == TEXT("harp")) Harp();
			else if (A == TEXT("horn_rack")) HornRack();
			else if (A == TEXT("herb_rack")) HerbRack();
			else if (A == TEXT("kite_rack")) KiteRack();
			else if (A == TEXT("map_table")) MapTable();
			else if (A == TEXT("globe")) Globe();
			else if (A == TEXT("alcove")) Alcove();
			else if (A == TEXT("window_seat")) WindowSeat();
			else if (A == TEXT("pelt_rug")) PeltRug();
			else if (A == TEXT("rug")) Rug();
			else if (A == TEXT("tapestry")) Tapestry();
			else if (A == TEXT("planter")) Planter();
			else if (A == TEXT("wheel_table")) WheelTable();
			else if (A == TEXT("aquarium")) Aquarium();
			else if (A == TEXT("stump_table")) StumpTable();
			else if (A == TEXT("puffballs")) Puffballs();
			else if (A == TEXT("rubble_table")) RubbleTable();
			else if (A == TEXT("crown_stand")) CrownStand();
			else if (A == TEXT("pew")) Pew();
			else if (A == TEXT("lectern")) Lectern();
			else if (A == TEXT("mirror")) Mirror();
			else if (A == TEXT("loom")) Loom();
			else if (A == TEXT("sundial")) Sundial();
		}

		// ---------------------------------------------------------------- seats

		void Throne()
		{
			if (V(TEXT("root")))
			{
				// Roots braided into a seat and a high back that rises into a crown of twigs.
				Add(Wood, FVector(70.f, 70.f, 46.f), FVector(0.f, 0.f, 23.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
				Box(Cloth, FVector(-28.f, -30.f, 46.f), FVector(30.f, 30.f, 54.f));
				for (int32 i = 0; i < 6; ++i)
				{
					const float Y = -30.f + i * 12.f;
					const FVector Base(-30.f, Y, 30.f), Mid(-38.f, Y * 1.1f, 120.f), Top(-34.f, Y * 1.4f, 190.f + (i % 2) * 18.f);
					Rod(Wood, Base, Mid, 9.f);
					Rod(Wood, Mid, Top, 7.f);
					Spike(Leaf, Top, (Top - Mid).GetSafeNormal(), 6.f, 14.f);
				}
				for (int32 r = 0; r < 5; ++r)
				{
					const FVector Dir(FMath::Cos(r * 1.25f + 0.3f), FMath::Sin(r * 1.25f + 0.3f), 0.f);
					Rod(Wood, Dir * 30.f + FVector(0.f, 0.f, 18.f), Dir * 58.f, 8.f);   // roots into the floor
				}
				for (const float Y : { -38.f, 38.f }) Rod(Wood, FVector(-28.f, Y, 46.f), FVector(28.f, Y, 66.f), 9.f);   // arms
				return;
			}
			if (V(TEXT("crystal")))
			{
				Box(Dark, FVector(-34.f, -38.f, 0.f), FVector(36.f, 38.f, 44.f));
				Box(Main, FVector(-32.f, -34.f, 44.f), FVector(32.f, 34.f, 48.f));
				for (int32 i = 0; i < 9; ++i)
				{
					const float Y = -32.f + i * 8.f;
					Spike(FSurf::Lit(Accent.Color, 0.7f), FVector(-34.f, Y, 44.f), FVector(-0.15f, Y * 0.006f, 1.f), 7.f + (i % 3), 110.f + 60.f * FMath::Sin(i / 8.f * PI));
				}
				for (const float Y : { -40.f, 40.f }) Spike(FSurf::Lit(Accent.Color, 0.6f), FVector(20.f, Y, 44.f), FVector(0.3f, 0.f, 1.f), 6.f, 30.f);
				return;
			}
			if (V(TEXT("coral")))
			{
				Box(Second, FVector(-34.f, -38.f, 0.f), FVector(36.f, 38.f, 44.f));
				Box(Cloth, FVector(-30.f, -34.f, 44.f), FVector(32.f, 34.f, 52.f));
				for (int32 i = 0; i < 7; ++i)
				{
					const float A2 = -60.f + i * 20.f;
					const FVector Dir = FRotator(0.f, 0.f, A2).RotateVector(FVector::UpVector);
					const FVector Base(-34.f, 0.f, 44.f), Tip = Base + FVector(-8.f, Dir.Y * 70.f, Dir.Z * 150.f);
					Rod(FSurf::Stone(Accent.Color, 0.8f), Base, Tip, 8.f);
					Rod(FSurf::Stone(Accent.Color, 0.8f), FMath::Lerp(Base, Tip, 0.6f), FMath::Lerp(Base, Tip, 0.6f) + FVector(0.f, Dir.Y * 20.f + 8.f, 30.f), 5.f);
				}
				return;
			}
			if (V(TEXT("null")))
			{
				// A slab of void-black glass, its back broken into pieces that hang in the air above it.
				const FSurf Void = FSurf::Glaze(Second.Color, 0.1f);
				Box(Void, FVector(-34.f, -38.f, 0.f), FVector(36.f, 38.f, 46.f));
				Box(Void, FVector(-38.f, -38.f, 46.f), FVector(-26.f, 38.f, 130.f));
				for (int32 i = 0; i < 5; ++i)
				{
					const FVector C(-32.f + i * 1.5f, -20.f + i * 10.f, 150.f + i * 16.f);
					Add(Void, FVector(10.f, 24.f - i * 2.f, 14.f), C, FRotator(Jit(C, 411) * 15.f, Jit(C, 412) * 25.f, Jit(C, 413) * 20.f));
				}
				for (const float Y : { -38.f, 38.f }) Box(FSurf::Lit(Glow.Color, 0.9f), FVector(-34.f, Y - 0.6f, 44.f), FVector(36.f, Y + 0.6f, 46.f));
				return;
			}
			// The obsidian throne (and the carved one): a solid seat, armrests, a tall back with an inlaid glow.
			const bool bCarved = V(TEXT("carved")) || !Tier;
			const FSurf Body = bCarved ? Wood : FSurf::Glaze(Second.Color, 0.12f);
			Box(Body, FVector(-32.f, -38.f, 0.f), FVector(36.f, 38.f, 44.f));
			Box(Cloth, FVector(-28.f, -32.f, 44.f), FVector(32.f, 32.f, 52.f));
			Box(Body, FVector(-40.f, -38.f, 0.f), FVector(-28.f, 38.f, 170.f));
			for (const float Y : { -38.f, 38.f }) Box(Body, FVector(-28.f, Y - 6.f, 44.f), FVector(32.f, Y + 6.f, 68.f));
			for (const float Y : { -34.f, 34.f }) Spike(Body, FVector(-34.f, Y, 170.f), FVector::UpVector, 8.f, 18.f);
			Box(bCarved ? Brass : FSurf::Lit(Glow.Color, 0.6f), FVector(-28.4f, -2.f, 70.f), FVector(-27.f, 2.f, 160.f));
			Box(bCarved ? Brass : FSurf::Lit(Glow.Color, 0.6f), FVector(-28.4f, -22.f, 140.f), FVector(-27.f, 22.f, 144.f));
		}

		void Bench()
		{
			const float L = 160.f;
			if (V(TEXT("carved")))
			{
				Box(Main, FVector(-20.f, -L * 0.5f, 42.f), FVector(20.f, L * 0.5f, 47.f));
				for (const float Y : { -L * 0.5f + 8.f, L * 0.5f - 8.f })
				{
					Box(Main, FVector(-20.f, Y - 4.f, 0.f), FVector(20.f, Y + 4.f, 42.f));
					Box(Main, FVector(-24.f, Y - 4.f, 47.f), FVector(-18.f, Y + 4.f, 100.f));
					Spike(Main, FVector(-21.f, Y, 100.f), FVector::UpVector, 7.f, 12.f);
				}
				for (float Y = -L * 0.5f + 20.f; Y < L * 0.5f - 15.f; Y += 16.f) Box(Main, FVector(-23.f, Y - 4.f, 55.f), FVector(-19.f, Y + 4.f, 94.f));   // carved slats
				Box(Main, FVector(-24.f, -L * 0.5f, 94.f), FVector(-18.f, L * 0.5f, 100.f));
				return;
			}
			const FSurf Seat = Tier && T(TEXT("basalt_hall")) ? FSurf::Wood(FLinearColor(0.35f, 0.33f, 0.3f)) : Wood;   // ash wood: pale grey
			Box(Seat, FVector(-20.f, -L * 0.5f, 42.f), FVector(20.f, L * 0.5f, 47.f));
			for (const float Y : { -L * 0.5f + 18.f, L * 0.5f - 18.f })
			{
				Rod(Seat, FVector(-16.f, Y, 0.f), FVector(-4.f, Y, 42.f), 6.f, true);
				Rod(Seat, FVector(16.f, Y, 0.f), FVector(4.f, Y, 42.f), 6.f, true);
				Box(Seat, FVector(-18.f, Y - 3.f, 22.f), FVector(18.f, Y + 3.f, 27.f));
			}
			if (V(TEXT("fur")))
			{
				// A pelt thrown over it, hanging down both sides.
				Box(Fur, FVector(-22.f, -L * 0.35f, 47.f), FVector(22.f, L * 0.35f, 50.f));
				for (const float X : { -23.f, 23.f }) Box(Fur, FVector(X - 1.5f, -L * 0.33f, 25.f), FVector(X + 1.5f, L * 0.33f, 50.f));
			}
		}

		void Stool()
		{
			Add(Wood, FVector(40.f, 40.f, 6.f), FVector(0.f, 0.f, 44.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			for (int32 i = 0; i < 3; ++i)
			{
				const FVector Dir(FMath::Cos(i * 2.094f), FMath::Sin(i * 2.094f), 0.f);
				Rod(Wood, Dir * 12.f + FVector(0.f, 0.f, 41.f), Dir * 20.f, 5.f);
			}
			Add(FSurf::Glaze(Accent.Color, 0.3f), FVector(41.f, 41.f, 1.5f), FVector(0.f, 0.f, 41.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);   // lacquered rim
			Ball(FSurf::Stone(FLinearColor(0.75f, 0.7f, 0.58f), 0.5f), FVector(0.f, 0.f, 50.f), 5.f);   // a knucklebone set in the seat
		}

		void Pew()
		{
			const float L = 200.f;
			Box(Wood, FVector(-22.f, -L * 0.5f, 42.f), FVector(22.f, L * 0.5f, 47.f));
			Box(Wood, FVector(-26.f, -L * 0.5f, 47.f), FVector(-20.f, L * 0.5f, 110.f));
			for (const float Y : { -L * 0.5f, L * 0.5f })
			{
				Box(Wood, FVector(-26.f, Y - 4.f, 0.f), FVector(26.f, Y + 4.f, 90.f));
				Spike(Wood, FVector(-23.f, Y, 110.f), FVector::UpVector, 8.f, 16.f);
				Add(Wood, FVector(18.f, 2.f, 18.f), FVector(10.f, Y + (Y > 0.f ? 4.5f : -4.5f), 70.f), FRotator(45.f, 0.f, 0.f));   // carved quatrefoil
			}
			Box(Cloth, FVector(-20.f, -L * 0.5f + 5.f, 47.f), FVector(20.f, L * 0.5f - 5.f, 51.f));
		}

		void Divan()
		{
			if (V(TEXT("chaise")))
			{
				// A velvet chaise: a long seat, a back that scrolls up at one end, a carved dark frame.
				Box(Wood, FVector(-30.f, -90.f, 10.f), FVector(30.f, 90.f, 36.f));
				Legs(Wood, -26.f, 26.f, -86.f, 86.f, 12.f, 6.f);
				Box(Cloth, FVector(-28.f, -88.f, 36.f), FVector(28.f, 88.f, 46.f));
				for (int32 i = 0; i < 8; ++i)
				{
					const float A2 = i / 7.f * 1.4f;
					Add(Cloth, FVector(56.f, 12.f, 10.f), FVector(0.f, -88.f + FMath::Sin(A2) * 6.f, 46.f + i * 9.f), FRotator(0.f, 0.f, -10.f * i));
				}
				Box(Cloth, FVector(-34.f, -88.f, 40.f), FVector(-26.f, 30.f, 80.f));
				return;
			}
			// A low cushioned divan with bolsters.
			Box(Wood, FVector(-40.f, -100.f, 0.f), FVector(40.f, 100.f, 24.f));
			Cushion(Cloth, FVector(-38.f, -98.f, 24.f), FVector(38.f, 98.f, 38.f));
			Box(FSurf::Cloth(Cloth.Color * 0.8f), FVector(-44.f, -100.f, 24.f), FVector(-30.f, 100.f, 70.f));
			for (const float Y : { -92.f, 92.f }) Add(FSurf::Cloth(Accent.Color), FVector(20.f, 20.f, 70.f), FVector(-4.f, Y, 46.f), FRotator(90.f, 0.f, 0.f), EWRLookMesh::Cylinder);
			for (int32 i = 0; i < 3; ++i) Box(FSurf::Cloth(Second.Color), FVector(-36.f, -60.f + i * 42.f, 38.f), FVector(-24.f, -26.f + i * 42.f, 66.f));
		}

		void Alcove()
		{
			// A reading nook: a cushioned seat under an arched canopy, shelves of books each side.
			Box(Main, FVector(-40.f, -90.f, 0.f), FVector(40.f, 90.f, 42.f));
			Box(Cloth, FVector(-38.f, -80.f, 42.f), FVector(38.f, 80.f, 52.f));
			for (const float Y : { -95.f, 95.f })
			{
				Box(Dark, FVector(-42.f, Y - 6.f, 0.f), FVector(42.f, Y + 6.f, 220.f));
				for (float Z = 70.f; Z < 210.f; Z += 45.f)
				{
					Box(Main, FVector(-38.f, Y - 14.f, Z), FVector(38.f, Y + 14.f, Z + 3.f));
					for (float X = -34.f; X < 32.f; X += 7.f)
					{
						const FVector C(X, Y, Z + 3.f);
						Box(FSurf::Cloth(FMath::Lerp(Accent.Color, Cloth.Color, Rand(C, 421))), FVector(X, Y - 10.f, Z + 3.f), FVector(X + 5.5f, Y + 10.f, Z + 22.f + Rand(C, 422) * 10.f));
					}
				}
			}
			Frame = FTransform(FRotator(0.f, 90.f, 0.f));
			Ring(Main, FVector(0.f, 0.f, 220.f), 92.f, 12.f, 60.f, 9, 0.f, 180.f);   // the canopy arching over the seat
			Frame = FTransform::Identity;
			Spike(FSurf::Lit(Accent.Color, 0.6f), FVector(0.f, 0.f, 318.f), FVector::UpVector, 8.f, 18.f);
		}

		void WindowSeat()
		{
			// A cushioned seat before a standing frame of ice-clear glass.
			Box(Main, FVector(-30.f, -80.f, 0.f), FVector(30.f, 80.f, 42.f));
			Cushion(Fur, FVector(-28.f, -78.f, 42.f), FVector(28.f, 78.f, 52.f));
			Box(Main, FVector(-40.f, -85.f, 0.f), FVector(-32.f, 85.f, 210.f));
			Box(Glass, FVector(-37.f, -70.f, 60.f), FVector(-35.f, 70.f, 195.f));
			for (const float Y : { -24.f, 24.f }) Box(Main, FVector(-38.f, Y - 2.f, 60.f), FVector(-34.f, Y + 2.f, 195.f));
			Box(Main, FVector(-38.f, -70.f, 126.f), FVector(-34.f, 70.f, 130.f));
			for (int32 i = 0; i < 2; ++i) Box(FSurf::Cloth(Fur.Color * 0.8f), FVector(-28.f, -70.f + i * 120.f, 52.f), FVector(-14.f, -50.f + i * 120.f, 80.f));
		}

		void Puffballs()
		{
			const FSurf Puff = FSurf::Cloth(FLinearColor(0.82f, 0.78f, 0.68f));
			Ball(Puff, FVector(0.f, -45.f, 24.f), 52.f);
			Ball(Puff, FVector(20.f, 30.f, 21.f), 46.f);
			Ball(Puff, FVector(-30.f, 10.f, 17.f), 36.f);
			for (const FVector& C : { FVector(0.f, -45.f, 0.f), FVector(20.f, 30.f, 0.f), FVector(-30.f, 10.f, 0.f) }) Disc(FSurf::Stone(Accent.Color * 0.6f, 0.95f), C, FVector::UpVector, 30.f, 4.f);
		}

		// ---------------------------------------------------------------- tables

		void Table()
		{
			const bool bLong = V(TEXT("long")), bLow = V(TEXT("low"));
			const float L = bLong ? 260.f : bLow ? 120.f : 150.f, W = bLow ? 80.f : 90.f, H = bLow ? 38.f : 76.f;
			const FSurf Top = T(TEXT("crag_hold")) ? FSurf::Stone(Second.Color, 0.6f) : Tier && !bLong ? Main : Wood;
			Box(Top, FVector(-W * 0.5f, -L * 0.5f, H - 6.f), FVector(W * 0.5f, L * 0.5f, H));
			if (bLow)
			{
				Box(Wood, FVector(-W * 0.5f + 6.f, -L * 0.5f + 6.f, 0.f), FVector(W * 0.5f - 6.f, L * 0.5f - 6.f, 10.f));
				for (const float Y : { -L * 0.5f + 8.f, L * 0.5f - 8.f }) Box(Wood, FVector(-W * 0.5f + 6.f, Y - 4.f, 10.f), FVector(W * 0.5f - 6.f, Y + 4.f, H - 6.f));
				// Tiled top: a border of glazed tiles.
				Box(FSurf::Glaze(Second.Color), FVector(-W * 0.5f, -L * 0.5f, H), FVector(W * 0.5f, -L * 0.5f + 8.f, H + 0.6f));
				Box(FSurf::Glaze(Second.Color), FVector(-W * 0.5f, L * 0.5f - 8.f, H), FVector(W * 0.5f, L * 0.5f, H + 0.6f));
				for (const float X : { -46.f, 46.f }) Cushion(Cloth, FVector(X - 16.f, -30.f, 0.f), FVector(X + 16.f, 30.f, 12.f));
				return;
			}
			const int32 Pairs = bLong ? 3 : 2;
			for (int32 i = 0; i < Pairs; ++i)
			{
				const float Y = -L * 0.5f + 12.f + i * (L - 24.f) / (Pairs - 1);
				for (const float X : { -W * 0.5f + 8.f, W * 0.5f - 8.f }) Box(Wood, FVector(X - 4.f, Y - 4.f, 0.f), FVector(X + 4.f, Y + 4.f, H - 6.f));
			}
			for (const float X : { -W * 0.5f + 8.f, W * 0.5f - 8.f }) Box(Wood, FVector(X - 2.f, -L * 0.5f + 12.f, H - 16.f), FVector(X + 2.f, L * 0.5f - 12.f, H - 6.f));
			if (bLong)
			{
				// Carved: a running knot along the apron.
				for (float Y = -L * 0.5f + 25.f; Y < L * 0.5f - 20.f; Y += 22.f) for (const float X : { -W * 0.5f + 5.5f, W * 0.5f - 5.5f }) Add(Wood, FVector(1.5f, 8.f, 8.f), FVector(X, Y, H - 11.f), FRotator(0.f, 0.f, 45.f));
			}
		}

		void AnvilTable()
		{
			// A smith's anvil carrying a thick basalt top: its horn points out under the slab.
			const FSurf Anvil = FSurf::Steel(FLinearColor(0.08f, 0.08f, 0.085f), 0.5f);
			Box(Anvil, FVector(-22.f, -26.f, 0.f), FVector(22.f, 26.f, 12.f));
			Box(Anvil, FVector(-12.f, -16.f, 12.f), FVector(12.f, 16.f, 40.f));
			Box(Anvil, FVector(-18.f, -32.f, 40.f), FVector(18.f, 32.f, 62.f));
			Spike(Anvil, FVector(0.f, 32.f, 51.f), FVector(0.f, 1.f, 0.f), 18.f, 28.f);
			Box(Main, FVector(-45.f, -70.f, 62.f), FVector(45.f, 70.f, 76.f));
			Box(FSurf::Lit(Glow.Color, 0.5f), FVector(-45.5f, -70.5f, 66.f), FVector(45.5f, 70.5f, 67.f));   // an ember-glass seam round the slab
		}

		void Desk()
		{
			Box(Main, FVector(-35.f, -65.f, 70.f), FVector(35.f, 65.f, 76.f));
			Box(Main, FVector(-33.f, 20.f, 0.f), FVector(33.f, 63.f, 70.f));
			for (const float Y : { -60.f }) Box(Main, FVector(-33.f, Y - 3.f, 0.f), FVector(33.f, Y + 3.f, 70.f));
			for (float Z = 12.f; Z < 66.f; Z += 19.f) Box(Second, FVector(33.f, 23.f, Z), FVector(34.5f, 60.f, Z + 16.f));   // drawers
			// Runes cut in its front, glowing.
			for (int32 r = 0; r < 5; ++r)
			{
				const float Y = -40.f + r * 10.f;
				Box(FSurf::Lit(Accent.Color, 0.6f), FVector(34.f, Y - 0.8f, 30.f), FVector(34.6f, Y + 0.8f, 52.f));
				Add(FSurf::Lit(Accent.Color, 0.6f), FVector(0.6f, 7.f, 1.6f), FVector(34.3f, Y + 2.f, 44.f - (r % 2) * 8.f), FRotator(0.f, 0.f, 35.f));
			}
			Box(FSurf::Cloth(FLinearColor(0.35f, 0.3f, 0.22f)), FVector(-10.f, -30.f, 76.f), FVector(14.f, 4.f, 79.f));   // an open book
			Candle(FVector(-20.f, 45.f, 76.f), 12.f);
		}

		void MapTable()
		{
			Box(Main, FVector(-50.f, -70.f, 70.f), FVector(50.f, 70.f, 76.f));
			Legs(Main, -42.f, 42.f, -62.f, 62.f, 70.f, 8.f);
			Box(FSurf::Cloth(FLinearColor(0.05f, 0.09f, 0.2f)), FVector(-46.f, -66.f, 76.f), FVector(46.f, 66.f, 77.f));
			for (int32 s = 0; s < 18; ++s)
			{
				const FVector C(-40.f + Rand(FVector(s), 431) * 80.f, -60.f + Rand(FVector(s, 1.f, 0.f), 432) * 120.f, 77.4f);
				Box(FSurf::Lit(FLinearColor(0.8f, 0.85f, 1.f), 1.2f), C - FVector(0.8f, 0.8f, 0.3f), C + FVector(0.8f, 0.8f, 0.3f));
			}
			for (int32 m = 0; m < 3; ++m) Add(FSurf::Lit(Glow.Color, 0.4f), FVector(5.f, 5.f, 5.f), FVector(-20.f + m * 20.f, -10.f + m * 15.f, 86.f + m * 3.f), FRotator(45.f, 45.f, 0.f));   // levistone markers, floating
		}

		void WheelTable()
		{
			// A ship's wheel laid flat on a capstan: spokes, rim, and the handles round it.
			Add(Wood, FVector(36.f, 36.f, 60.f), FVector(0.f, 0.f, 30.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			Add(FSurf::Glass(FLinearColor(0.2f, 0.3f, 0.3f)), FVector(110.f, 110.f, 3.f), FVector(0.f, 0.f, 75.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			for (int32 i = 0; i < 8; ++i)
			{
				const FVector Dir(FMath::Cos(i * PI / 4.f), FMath::Sin(i * PI / 4.f), 0.f);
				Rod(Wood, FVector(0.f, 0.f, 70.f), Dir * 52.f + FVector(0.f, 0.f, 70.f), 4.f);
				Rod(Wood, Dir * 52.f + FVector(0.f, 0.f, 70.f), Dir * 66.f + FVector(0.f, 0.f, 70.f), 5.f);
			}
			for (int32 i = 0; i < 16; ++i)
			{
				const float A1 = i * PI / 8.f, A2 = (i + 1) * PI / 8.f;
				Rod(Wood, FVector(FMath::Cos(A1), FMath::Sin(A1), 0.f) * 50.f + FVector(0.f, 0.f, 70.f), FVector(FMath::Cos(A2), FMath::Sin(A2), 0.f) * 50.f + FVector(0.f, 0.f, 70.f), 6.f, true);
			}
			Disc(Brass, FVector(0.f, 0.f, 72.f), FVector::UpVector, 14.f, 4.f);
		}

		void StumpTable()
		{
			Add(Wood, FVector(56.f, 56.f, 60.f), FVector(0.f, 0.f, 30.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			for (int32 r = 0; r < 5; ++r)
			{
				const FVector Dir(FMath::Cos(r * 1.26f), FMath::Sin(r * 1.26f), 0.f);
				Rod(Wood, Dir * 24.f + FVector(0.f, 0.f, 20.f), Dir * 48.f, 10.f);
			}
			// A shelf fungus grown flat across the top: the table.
			Disc(FSurf::Stone(Accent.Color, 0.9f), FVector(0.f, 0.f, 64.f), FVector::UpVector, 120.f, 8.f);
			Disc(FSurf::Stone(Main.Color, 0.9f), FVector(0.f, 0.f, 68.2f), FVector::UpVector, 84.f, 0.6f);
			Disc(FSurf::Lit(Glow.Color, 0.45f), FVector(0.f, 0.f, 59.8f), FVector::UpVector, 110.f, 0.6f);   // gills underneath
		}

		void RubbleTable()
		{
			// A slab of grey stone held up by rubble that doesn't touch: each piece floats a little apart.
			for (int32 i = 0; i < 6; ++i)
			{
				const FVector C(Jit(FVector(i), 441) * 18.f, Jit(FVector(i, 1.f, 0.f), 442) * 30.f, 8.f + i * 11.f);
				Add(Main, FVector(22.f + Rand(C, 443) * 12.f, 26.f, 9.f), C, FRotator(Jit(C, 444) * 12.f, Jit(C, 445) * 40.f, Jit(C, 446) * 12.f));
			}
			Box(Main, FVector(-45.f, -70.f, 70.f), FVector(45.f, 70.f, 78.f));
			Box(FSurf::Lit(Glow.Color, 0.9f), FVector(-46.f, -71.f, 68.f), FVector(46.f, 71.f, 69.f));
		}

		// ---------------------------------------------------------------- lights

		void LampStand()
		{
			if (V(TEXT("pearl")))
			{
				// A great pearl in a cup of coral on a twisted stand.
				Disc(Second, FVector(0.f, 0.f, 3.f), FVector::UpVector, 40.f, 6.f);
				for (int32 i = 0; i < 3; ++i) Rod(FSurf::Stone(Accent.Color, 0.8f), FVector(FMath::Cos(i * 2.1f) * 6.f, FMath::Sin(i * 2.1f) * 6.f, 6.f), FVector(FMath::Cos(i * 2.1f + 1.f) * 6.f, FMath::Sin(i * 2.1f + 1.f) * 6.f, 130.f), 5.f);
				for (int32 i = 0; i < 6; ++i) Rod(FSurf::Stone(Accent.Color, 0.8f), FVector(0.f, 0.f, 128.f), FVector(FMath::Cos(i * 1.05f) * 16.f, FMath::Sin(i * 1.05f) * 16.f, 146.f), 3.f);
				Ball(FSurf::Lit(FLinearColor(0.9f, 0.92f, 0.95f), 0.45f), FVector(0.f, 0.f, 148.f), 26.f);
				return;
			}
			if (V(TEXT("sun")))
			{
				Disc(Brass, FVector(0.f, 0.f, 3.f), FVector::UpVector, 36.f, 6.f);
				Rod(Brass, FVector(0.f, 0.f, 6.f), FVector(0.f, 0.f, 130.f), 4.f);
				for (int32 i = 0; i < 4; ++i) Rod(Brass, FVector(0.f, 0.f, 128.f), FVector(FMath::Cos(i * HALF_PI) * 12.f, FMath::Sin(i * HALF_PI) * 12.f, 150.f), 2.f);
				Add(FSurf::Lit(FLinearColor(1.f, 0.62f, 0.14f), 1.f), FVector(16.f, 16.f, 22.f), FVector(0.f, 0.f, 145.f), FRotator(0.f, 45.f, 0.f));
				return;
			}
			// Lava glass: a glowing orange globe in a cage of black iron on a basalt foot.
			Hex(Main, 0.f, 0.f, 0.f, 18.f, 22.f);
			Rod(Iron, FVector(0.f, 0.f, 18.f), FVector(0.f, 0.f, 130.f), 5.f);
			Ball(FSurf::Lit(Glow.Color, 1.f), FVector(0.f, 0.f, 150.f), 30.f);
			for (int32 i = 0; i < 6; ++i)
			{
				const FVector Dir(FMath::Cos(i * 1.047f), FMath::Sin(i * 1.047f), 0.f);
				Rod(Iron, FVector(0.f, 0.f, 130.f) + Dir * 8.f, FVector(0.f, 0.f, 150.f) + Dir * 17.f, 1.6f);
				Rod(Iron, FVector(0.f, 0.f, 150.f) + Dir * 17.f, FVector(0.f, 0.f, 170.f) + Dir * 6.f, 1.6f);
			}
		}

		void RodLamp()
		{
			// A copper lightning rod on a slate foot, a coil round it, a stormglass bulb crackling blue at the top.
			Box(FSurf::Stone(Second.Color, 0.6f), FVector(-18.f, -18.f, 0.f), FVector(18.f, 18.f, 10.f));
			Rod(Copper, FVector(0.f, 0.f, 10.f), FVector(0.f, 0.f, 160.f), 4.f);
			for (float Z = 40.f; Z < 120.f; Z += 6.f) Add(Copper, FVector(12.f, 12.f, 1.6f), FVector(0.f, 0.f, Z), FRotator(4.f, 0.f, 0.f), EWRLookMesh::Cylinder);
			Ball(FSurf::Lit(FLinearColor(0.35f, 0.55f, 1.f), 1.f), FVector(0.f, 0.f, 168.f), 18.f);
			Spike(Copper, FVector(0.f, 0.f, 176.f), FVector::UpVector, 3.f, 22.f);
		}

		void LanternHang()
		{
			// A shepherd's-hook stand and a brass lantern with glass sides hanging from it.
			Disc(Iron, FVector(0.f, 0.f, 2.f), FVector::UpVector, 36.f, 4.f);
			Rod(Iron, FVector(0.f, 0.f, 4.f), FVector(0.f, 0.f, 190.f), 3.f);
			Rod(Iron, FVector(0.f, 0.f, 190.f), FVector(20.f, 0.f, 200.f), 3.f);
			Rod(Iron, FVector(20.f, 0.f, 200.f), FVector(34.f, 0.f, 190.f), 3.f);
			Rod(Iron, FVector(34.f, 0.f, 190.f), FVector(34.f, 0.f, 176.f), 1.2f);
			Box(Brass, FVector(24.f, -10.f, 172.f), FVector(44.f, 10.f, 176.f));
			Box(Brass, FVector(24.f, -10.f, 138.f), FVector(44.f, 10.f, 142.f));
			for (const float X : { 25.f, 43.f }) for (const float Y : { -9.f, 9.f }) Box(Brass, FVector(X - 1.f, Y - 1.f, 142.f), FVector(X + 1.f, Y + 1.f, 172.f));
			Box(FSurf::Glass(FLinearColor(0.7f, 0.5f, 0.25f), 0.3f), FVector(25.f, -9.f, 142.f), FVector(43.f, 9.f, 172.f));
			Candle(FVector(34.f, 0.f, 142.f), 14.f);
			Spike(Brass, FVector(34.f, 0.f, 176.f), FVector::UpVector, 16.f, 8.f);
		}

		void LampFloat()
		{
			const bool bRift = V(TEXT("rift"));
			Box(Main, FVector(-20.f, -20.f, 0.f), FVector(20.f, 20.f, 16.f));
			Box(Main, FVector(-14.f, -14.f, 16.f), FVector(14.f, 14.f, 60.f));
			if (bRift)
			{
				// A ring of black glass hanging in the air, a violet light caught inside it.
				Ring(FSurf::Glaze(Second.Color, 0.1f), FVector(0.f, 0.f, 130.f), 22.f, 6.f, 6.f, 12);
				Disc(FSurf::Lit(Glow.Color, 1.2f), FVector(0.f, 0.f, 130.f), FVector(0.f, 1.f, 0.f), 40.f, 1.f);
				return;
			}
			// A levistone orb floating over its plinth, three small stones circling it.
			Ball(FSurf::Lit(Glow.Color, 0.8f), FVector(0.f, 0.f, 120.f), 28.f);
			for (int32 i = 0; i < 3; ++i) Add(Main, FVector(7.f, 7.f, 7.f), FVector(FMath::Cos(i * 2.1f) * 30.f, FMath::Sin(i * 2.1f) * 30.f, 112.f + i * 8.f), FRotator(30.f * i, 45.f, 0.f));
		}

		void CrystalLamp()
		{
			Box(Dark, FVector(-22.f, -22.f, 0.f), FVector(22.f, 22.f, 30.f));
			for (int32 i = 0; i < 7; ++i)
			{
				const FVector Dir = FVector(Jit(FVector(i), 451) * 0.4f, Jit(FVector(i, 1.f, 0.f), 452) * 0.4f, 1.f).GetSafeNormal();
				Spike(FSurf::Lit(Tier ? Accent.Color : FLinearColor(0.55f, 0.35f, 0.95f), 0.9f), FVector(Jit(FVector(i), 453) * 8.f, Jit(FVector(i, 2.f, 0.f), 454) * 8.f, 28.f), Dir, 8.f + (i % 3) * 3.f, 40.f + (i % 4) * 18.f);
			}
		}

		void Glowcap()
		{
			Add(FSurf::Stone(FLinearColor(0.75f, 0.72f, 0.62f), 0.9f), FVector(14.f, 14.f, 120.f), FVector(0.f, 0.f, 60.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			Disc(FSurf::Stone(Accent.Color, 0.85f), FVector(0.f, 0.f, 128.f), FVector::UpVector, 80.f, 14.f);
			Disc(FSurf::Stone(Accent.Color * 0.9f, 0.85f), FVector(0.f, 0.f, 138.f), FVector::UpVector, 54.f, 8.f);
			for (int32 g = 0; g < 24; ++g)
			{
				const float A2 = g * PI / 12.f;
				Add(FSurf::Lit(Glow.Color, 0.9f), FVector(30.f, 0.8f, 5.f), FVector(FMath::Cos(A2) * 22.f, FMath::Sin(A2) * 22.f, 119.f), FRotator(0.f, FMath::RadiansToDegrees(A2), 0.f));
			}
		}

		void GrubJar()
		{
			Add(Wood, FVector(40.f, 40.f, 50.f), FVector(0.f, 0.f, 25.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);   // a stump to stand on
			Add(Glass, FVector(30.f, 30.f, 40.f), FVector(0.f, 0.f, 70.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			Add(Wood, FVector(22.f, 22.f, 6.f), FVector(0.f, 0.f, 93.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);   // its cork
			for (int32 g = 0; g < 6; ++g)
			{
				const FVector C(Jit(FVector(g), 461) * 8.f, Jit(FVector(g, 1.f, 0.f), 462) * 8.f, 56.f + g * 4.f);
				Add(FSurf::Lit(Glow.Color, 1.2f), FVector(3.f, 3.f, 7.f), C, FRotator(70.f, g * 60.f, 0.f), EWRLookMesh::Cylinder);
			}
		}

		void ToadLamp()
		{
			// A squat carved toad holding a lantern in its wide mouth.
			const FSurf Toad = FSurf::Stone(FLinearColor(0.12f, 0.14f, 0.06f), 0.7f);
			Add(Toad, FVector(70.f, 60.f, 40.f), FVector(0.f, 0.f, 22.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			Add(Toad, FVector(46.f, 50.f, 26.f), FVector(20.f, 0.f, 50.f), FRotator(10.f, 0.f, 0.f), EWRLookMesh::Cylinder);
			for (const float Y : { -16.f, 16.f }) Ball(FSurf::Glaze(FLinearColor(0.6f, 0.4f, 0.05f), 0.2f), FVector(22.f, Y, 64.f), 9.f);
			for (const float Y : { -26.f, 26.f }) Box(Toad, FVector(14.f, Y - 6.f, 0.f), FVector(40.f, Y + 6.f, 10.f));
			Rod(Iron, FVector(36.f, 0.f, 52.f), FVector(36.f, 0.f, 70.f), 1.5f);
			Box(FSurf::Glass(FLinearColor(0.5f, 0.6f, 0.2f), 0.6f), FVector(28.f, -8.f, 70.f), FVector(44.f, 8.f, 90.f));
			Candle(FVector(36.f, 0.f, 70.f), 12.f);
		}

		void LanternTree()
		{
			// A little carved tree with small amber lanterns on its branches.
			Add(Wood, FVector(14.f, 14.f, 140.f), FVector(0.f, 0.f, 70.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			for (int32 b = 0; b < 6; ++b)
			{
				const float A2 = b * 1.05f;
				const FVector From(0.f, 0.f, 90.f + (b % 3) * 25.f), To = From + FVector(FMath::Cos(A2) * 45.f, FMath::Sin(A2) * 45.f, 30.f);
				Rod(Wood, From, To, 5.f);
				Rod(Iron, To, To - FVector(0.f, 0.f, 12.f), 0.8f);
				Box(FSurf::Glass(FLinearColor(0.9f, 0.55f, 0.15f), 0.9f), To - FVector(5.f, 5.f, 24.f), To - FVector(-5.f, -5.f, 12.f));
			}
			for (int32 r = 0; r < 4; ++r) Rod(Wood, FVector(0.f, 0.f, 18.f), FVector(FMath::Cos(r * HALF_PI) * 30.f, FMath::Sin(r * HALF_PI) * 30.f, 0.f), 7.f);
		}

		void Chandelier()
		{
			// A tripod of iron carrying a ring of antlers set with candles.
			for (int32 i = 0; i < 3; ++i)
			{
				const FVector Foot(FMath::Cos(i * 2.094f) * 70.f, FMath::Sin(i * 2.094f) * 70.f, 0.f);
				Rod(Iron, Foot, FVector(0.f, 0.f, 240.f), 4.f);
			}
			Rod(Iron, FVector(0.f, 0.f, 240.f), FVector(0.f, 0.f, 180.f), 1.5f);
			const FSurf Antler = FSurf::Stone(FLinearColor(0.55f, 0.48f, 0.38f), 0.7f);
			for (int32 i = 0; i < 6; ++i)
			{
				const float A1 = i * 1.047f, A2 = (i + 1) * 1.047f;
				const FVector P1(FMath::Cos(A1) * 40.f, FMath::Sin(A1) * 40.f, 172.f), P2(FMath::Cos(A2) * 40.f, FMath::Sin(A2) * 40.f, 172.f);
				Rod(Antler, P1, P2, 5.f);
				Rod(Antler, P1, P1 * FVector(1.3f, 1.3f, 1.f) + FVector(0.f, 0.f, 22.f), 3.f);   // tines
				Rod(Antler, P1, FVector(0.f, 0.f, 180.f), 1.2f);
				Candle(P1 + FVector(0.f, 0.f, 2.f), 10.f);
			}
		}

		void CandleRack()
		{
			// Black iron tiers of candles stepped like a hearse, wax pooled on the bars.
			for (const float Y : { -60.f, 60.f }) Box(Iron, FVector(-3.f, Y - 3.f, 0.f), FVector(3.f, Y + 3.f, 130.f));
			for (int32 t = 0; t < 3; ++t)
			{
				const float Z = 70.f + t * 25.f, X = 12.f - t * 12.f;
				Box(Iron, FVector(X - 2.f, -60.f, Z - 2.f), FVector(X + 2.f, 60.f, Z));
				for (float Y = -52.f + t * 6.f; Y < 54.f; Y += 15.f) Candle(FVector(X, Y, Z), 8.f + Rand(FVector(Y, Z, 0.f), 471) * 10.f);
			}
			Box(Iron, FVector(-20.f, -66.f, 0.f), FVector(20.f, 66.f, 4.f));
		}

		void Candles()
		{
			Disc(Iron, FVector(0.f, 0.f, 2.f), FVector::UpVector, 28.f, 4.f);
			Rod(Iron, FVector(0.f, 0.f, 4.f), FVector(0.f, 0.f, 110.f), 2.5f);
			Rod(Iron, FVector(-24.f, 0.f, 110.f), FVector(24.f, 0.f, 110.f), 2.f);
			for (const float X : { -24.f, 0.f, 24.f }) { Disc(Iron, FVector(X, 0.f, 112.f), FVector::UpVector, 8.f, 2.f); Candle(FVector(X, 0.f, 113.f), X == 0.f ? 16.f : 12.f); }
		}

		void Lantern()
		{
			// A horn lantern on an iron post.
			Disc(Iron, FVector(0.f, 0.f, 2.f), FVector::UpVector, 34.f, 4.f);
			Rod(Iron, FVector(0.f, 0.f, 4.f), FVector(0.f, 0.f, 140.f), 4.f);
			Box(Iron, FVector(-10.f, -10.f, 140.f), FVector(10.f, 10.f, 143.f));
			Box(FSurf::Glass(FLinearColor(0.75f, 0.55f, 0.3f), 0.5f), FVector(-9.f, -9.f, 143.f), FVector(9.f, 9.f, 168.f));
			for (const float X : { -9.f, 9.f }) for (const float Y : { -9.f, 9.f }) Box(Iron, FVector(X - 1.f, Y - 1.f, 143.f), FVector(X + 1.f, Y + 1.f, 168.f));
			Spike(Iron, FVector(0.f, 0.f, 168.f), FVector::UpVector, 22.f, 10.f);
			Candle(FVector(0.f, 0.f, 143.f), 12.f);
		}

		void Fireplace()
		{
			// A chimney breast: a stone firebox with an arched mouth, a hearthstone, a mantel, the flue up to 2.6 m, and a fire.
			const bool bForge = T(TEXT("basalt_hall"));
			const FSurf Stone = Tier ? Main : Second;
			Box(Stone, FVector(-40.f, -80.f, 0.f), FVector(10.f, -45.f, 120.f));
			Box(Stone, FVector(-40.f, 45.f, 0.f), FVector(10.f, 80.f, 120.f));
			Box(Stone, FVector(-40.f, -80.f, 120.f), FVector(10.f, 80.f, 140.f));
			Box(Dark, FVector(-40.f, -45.f, 0.f), FVector(-30.f, 45.f, 120.f));
			Frame = FTransform(FRotator(0.f, 90.f, 0.f), FVector(4.f, 0.f, 0.f));
			Ring(Stone, FVector(0.f, 0.f, 80.f), 45.f, 10.f, 14.f, 7, 0.f, 180.f);
			Frame = FTransform::Identity;
			Box(FSurf::Wood(Wood.Color), FVector(-44.f, -88.f, 140.f), FVector(16.f, 88.f, 150.f));   // mantel
			Box(Stone, FVector(-40.f, -55.f, 150.f), FVector(0.f, 55.f, 260.f));                         // the flue
			Box(Stone, FVector(-10.f, -85.f, 0.f), FVector(40.f, 85.f, 6.f), 0.9f);                       // hearthstone
			for (int32 l = 0; l < 3; ++l) Add(Wood, FVector(10.f, 10.f, 60.f), FVector(-10.f + l * 4.f, -10.f + l * 10.f, 14.f + l * 4.f), FRotator(0.f, 20.f * l - 20.f, 90.f), EWRLookMesh::Cylinder);
			Box(FSurf::Lit(FLinearColor(1.f, 0.35f, 0.08f), 1.2f), FVector(-20.f, -28.f, 6.f), FVector(4.f, 28.f, 10.f));   // embers
			for (int32 f = 0; f < 4; ++f) Spike(FSurf::Lit(FLinearColor(1.f, 0.5f, 0.12f), 1.4f), FVector(-10.f + f * 3.f, -18.f + f * 12.f, 10.f), FVector(0.f, 0.f, 1.f), 10.f, 28.f + (f % 2) * 12.f);
			if (bForge)
			{
				// Forge-hearth: a lava-glass grate and a pair of bellows hung on the breast.
				for (int32 g = 0; g < 5; ++g) Box(FSurf::Lit(Glow.Color, 0.9f), FVector(-22.f, -30.f + g * 15.f, 10.f), FVector(2.f, -28.f + g * 15.f, 13.f));
				Box(Hide, FVector(8.f, 50.f, 60.f), FVector(14.f, 72.f, 110.f));
				Spike(Iron, FVector(11.f, 61.f, 60.f), FVector(0.f, 0.f, -1.f), 6.f, 14.f);
			}
		}

		void Cauldron()
		{
			// An iron cauldron on a tripod over a fire, green brew bubbling.
			for (int32 i = 0; i < 3; ++i) Rod(Iron, FVector(FMath::Cos(i * 2.094f) * 50.f, FMath::Sin(i * 2.094f) * 50.f, 0.f), FVector(0.f, 0.f, 120.f), 4.f);
			Rod(Iron, FVector(0.f, 0.f, 120.f), FVector(0.f, 0.f, 80.f), 1.5f);
			Ball(Iron, FVector(0.f, 0.f, 58.f), 60.f);
			Add(FSurf::Lit(FLinearColor(0.25f, 0.7f, 0.15f), 0.7f), FVector(46.f, 46.f, 2.f), FVector(0.f, 0.f, 76.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			Add(Iron, FVector(50.f, 50.f, 4.f), FVector(0.f, 0.f, 76.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder, 0.9f);
			for (int32 s = 0; s < 8; ++s) Add(FSurf::Stone(FLinearColor(0.2f, 0.19f, 0.17f)), FVector(14.f, 12.f, 10.f), FVector(FMath::Cos(s * 0.785f) * 34.f, FMath::Sin(s * 0.785f) * 34.f, 5.f));
			Box(FSurf::Lit(FLinearColor(1.f, 0.35f, 0.08f), 1.1f), FVector(-16.f, -16.f, 0.f), FVector(16.f, 16.f, 6.f));
			for (int32 b = 0; b < 3; ++b) Ball(FSurf::Stone(FLinearColor(0.85f, 0.8f, 0.7f), 0.5f), FVector(28.f, -20.f + b * 20.f, 4.f), 8.f);   // bones by the fire
		}

		// ---------------------------------------------------------------- water

		void Bath()
		{
			const bool bTub = V(TEXT("tub")), bMoss = V(TEXT("moss")), bOpen = T(TEXT("cloud_spire"));
			const bool bSpring = !bTub && !bMoss && !bOpen;
			if (bTub)
			{
				// A wooden tub of staves in three iron hoops.
				for (int32 s = 0; s < 18; ++s)
				{
					const float A2 = s * 2.f * PI / 18.f;
					Add(Wood, FVector(5.f, 18.f, 64.f), FVector(FMath::Cos(A2) * 58.f, FMath::Sin(A2) * 58.f, 32.f), FRotator(0.f, FMath::RadiansToDegrees(A2), 0.f));
				}
				for (const float Z : { 10.f, 32.f, 56.f }) for (int32 s = 0; s < 18; ++s)
				{
					const float A2 = s * 2.f * PI / 18.f;
					Add(Iron, FVector(1.2f, 21.f, 3.f), FVector(FMath::Cos(A2) * 61.f, FMath::Sin(A2) * 61.f, Z), FRotator(0.f, FMath::RadiansToDegrees(A2), 0.f));
				}
				Add(Water, FVector(112.f, 112.f, 2.f), FVector(0.f, 0.f, 50.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
				Add(Wood, FVector(112.f, 112.f, 4.f), FVector(0.f, 0.f, 2.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
				return;
			}
			// A stone basin set into a raised rim, steps in, water to 45.
			const FSurf Rim = bOpen ? Main : bMoss ? FSurf::Stone(FLinearColor(0.2f, 0.19f, 0.17f)) : Tier ? Main : Second;
			Box(Rim, FVector(-100.f, -70.f, 0.f), FVector(100.f, -55.f, 60.f));
			Box(Rim, FVector(-100.f, 55.f, 0.f), FVector(100.f, 70.f, 60.f));
			Box(Rim, FVector(-100.f, -55.f, 0.f), FVector(-85.f, 55.f, 60.f));
			Box(Rim, FVector(85.f, -55.f, 0.f), FVector(100.f, 55.f, 60.f));
			Box(Rim, FVector(-85.f, -55.f, 0.f), FVector(85.f, 55.f, 10.f));
			Box(Rim, FVector(100.f, -30.f, 0.f), FVector(120.f, 30.f, 30.f));   // a step up
			Box(Water, FVector(-85.f, -55.f, 10.f), FVector(85.f, 55.f, 46.f));
			Box(FSurf::Glass(bSpring ? FLinearColor(0.12f, 0.28f, 0.26f) : FLinearColor(0.1f, 0.24f, 0.3f), bSpring ? 0.12f : 0.03f), FVector(-85.f, -55.f, 45.f), FVector(85.f, 55.f, 46.f));
			if (bOpen) { Box(FSurf::Glaze(Second.Color), FVector(-101.f, -71.f, 56.f), FVector(101.f, 71.f, 60.5f)); }
			if (bMoss) for (int32 m = 0; m < 10; ++m)
			{
				const FVector C(-90.f + Rand(FVector(m), 481) * 180.f, (m % 2 ? 1.f : -1.f) * 62.f, 60.f);
				Add(FSurf::Stone(FLinearColor(0.12f, 0.22f, 0.06f), 1.f), FVector(30.f, 22.f, 6.f), C, FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			}
			if (bSpring)
			{
				// Mineral crust where the hot water spills over the rim.
				for (int32 c = 0; c < 6; ++c) Add(FSurf::Stone(FLinearColor(0.55f, 0.42f, 0.24f), 0.7f), FVector(18.f, 14.f, 3.f), FVector(-90.f + c * 30.f, -70.f, 58.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			}
		}

		void Fountain()
		{
			if (V(TEXT("shell")))
			{
				// A giant scallop shell basin on a coral stem.
				Disc(Second, FVector(0.f, 0.f, 3.f), FVector::UpVector, 50.f, 6.f);
				Add(FSurf::Stone(Accent.Color, 0.8f), FVector(18.f, 18.f, 70.f), FVector(0.f, 0.f, 40.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
				for (int32 r = 0; r < 9; ++r)
				{
					const float A2 = -70.f + r * 17.5f;
					const FVector Dir = FRotator(0.f, A2, 0.f).RotateVector(FVector(1.f, 0.f, 0.f));
					Add(FSurf::Glaze(Second.Color, 0.4f), FVector(70.f, 13.f, 6.f), FVector(0.f, 0.f, 80.f) + Dir * 30.f + FVector(0.f, 0.f, FMath::Abs(A2) * 0.12f), FRotator(10.f, A2, 0.f));
				}
				Add(Water, FVector(70.f, 70.f, 2.f), FVector(30.f, 0.f, 84.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
				return;
			}
			// A tiled octagonal basin with a column and an upper bowl.
			for (int32 s = 0; s < 8; ++s)
			{
				const float A2 = s * PI / 4.f;
				Add(Main, FVector(10.f, 62.f, 45.f), FVector(FMath::Cos(A2) * 72.f, FMath::Sin(A2) * 72.f, 22.5f), FRotator(0.f, FMath::RadiansToDegrees(A2), 0.f));
				Add(FSurf::Glaze(Second.Color), FVector(1.f, 50.f, 26.f), FVector(FMath::Cos(A2) * 77.5f, FMath::Sin(A2) * 77.5f, 22.f), FRotator(0.f, FMath::RadiansToDegrees(A2), 0.f));   // tiles
			}
			Add(Main, FVector(140.f, 140.f, 8.f), FVector(0.f, 0.f, 4.f), FRotator(0.f, 22.5f, 0.f), EWRLookMesh::Cylinder);
			Add(Water, FVector(136.f, 136.f, 30.f), FVector(0.f, 0.f, 24.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			Add(Main, FVector(16.f, 16.f, 90.f), FVector(0.f, 0.f, 45.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			Add(Main, FVector(56.f, 56.f, 10.f), FVector(0.f, 0.f, 92.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			Add(Water, FVector(50.f, 50.f, 2.f), FVector(0.f, 0.f, 97.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			Spike(Main, FVector(0.f, 0.f, 97.f), FVector::UpVector, 8.f, 20.f);
			Add(FSurf::Lit(Glow.Color, 0.5f), FVector(6.f, 6.f, 6.f), FVector(0.f, 0.f, 118.f), FRotator(45.f, 45.f, 0.f));   // a sunstone at its tip
		}

		void Aquarium()
		{
			// A glass tank on a stand: water, sand, coral and small fish.
			Box(Wood, FVector(-35.f, -70.f, 0.f), FVector(35.f, 70.f, 70.f));
			Box(Glass, FVector(-30.f, -65.f, 70.f), FVector(30.f, 65.f, 72.f));
			for (const float X : { -30.f, 30.f }) Box(Glass, FVector(X - 1.f, -65.f, 72.f), FVector(X + 1.f, 65.f, 130.f));
			for (const float Y : { -65.f, 65.f }) Box(Glass, FVector(-30.f, Y - 1.f, 72.f), FVector(30.f, Y + 1.f, 130.f));
			Box(FSurf::Glass(FLinearColor(0.06f, 0.22f, 0.28f), 0.15f), FVector(-29.f, -64.f, 72.f), FVector(29.f, 64.f, 124.f));
			Box(FSurf::Stone(FLinearColor(0.6f, 0.52f, 0.38f)), FVector(-29.f, -64.f, 72.f), FVector(29.f, 64.f, 78.f));
			for (int32 c = 0; c < 4; ++c) Rod(FSurf::Stone(Accent.Color, 0.8f), FVector(-10.f + c * 6.f, -40.f + c * 25.f, 78.f), FVector(-6.f + c * 6.f, -36.f + c * 25.f, 100.f + c * 3.f), 4.f);
			for (int32 f = 0; f < 5; ++f) Add(FSurf::Glaze(f % 2 ? FLinearColor(0.8f, 0.4f, 0.05f) : FLinearColor(0.2f, 0.5f, 0.9f), 0.3f), FVector(6.f, 2.f, 3.f), FVector(Jit(FVector(f), 491) * 18.f, -50.f + f * 24.f, 95.f + f * 4.f), FRotator(0.f, f * 40.f, 0.f));
			Box(Wood, FVector(-31.f, -66.f, 128.f), FVector(31.f, 66.f, 132.f));
		}

		// ---------------------------------------------------------------- beds

		void Bed()
		{
			const bool bFur = V(TEXT("fur")), bMoss = V(TEXT("moss"));
			const FSurf Frame2 = bMoss ? FSurf::Wood(FLinearColor(0.12f, 0.09f, 0.06f)) : Tier ? Wood : Main;
			for (const float X : { -104.f, 104.f }) for (const float Y : { -54.f, 54.f }) Box(Frame2, FVector(X - 5.f, Y - 5.f, 0.f), FVector(X + 5.f, Y + 5.f, 34.f));
			Box(Frame2, FVector(-108.f, -58.f, 18.f), FVector(108.f, -52.f, 34.f));
			Box(Frame2, FVector(-108.f, 52.f, 18.f), FVector(108.f, 58.f, 34.f));
			Box(Frame2, FVector(-112.f, -58.f, 0.f), FVector(-104.f, 58.f, bMoss ? 70.f : 100.f));   // headboard
			Box(Frame2, FVector(104.f, -58.f, 0.f), FVector(110.f, 58.f, 50.f));
			const FSurf Mattress = bMoss ? FSurf::Stone(FLinearColor(0.1f, 0.2f, 0.05f), 1.f) : FSurf::Cloth(FLinearColor(0.7f, 0.66f, 0.56f));
			Box(Mattress, FVector(-104.f, -52.f, 24.f), FVector(104.f, 52.f, 46.f));
			if (bFur)
			{
				// Pelts heaped over it, one hanging off the foot.
				Box(Fur, FVector(-50.f, -55.f, 46.f), FVector(104.f, 55.f, 52.f));
				Box(FSurf::Cloth(Fur.Color * 0.7f), FVector(-10.f, -50.f, 52.f), FVector(80.f, 40.f, 55.f));
				Box(Fur, FVector(104.f, -40.f, 20.f), FVector(108.f, 40.f, 50.f));
			}
			else if (bMoss)
			{
				for (int32 m = 0; m < 6; ++m) Add(FSurf::Stone(FLinearColor(0.14f, 0.26f, 0.07f), 1.f), FVector(40.f, 34.f, 8.f), FVector(-70.f + m * 30.f, Jit(FVector(m), 501) * 20.f, 48.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
				for (int32 f = 0; f < 3; ++f) Spike(FSurf::Lit(Glow.Color, 0.4f), FVector(-100.f, -40.f + f * 40.f, 70.f), FVector::UpVector, 6.f, 14.f);
			}
			else
			{
				// A feather bed: a deep quilt in plump channels.
				for (int32 q = 0; q < 6; ++q) Add(FSurf::Cloth(FLinearColor(0.82f, 0.82f, 0.86f)), FVector(30.f, 106.f, 12.f), FVector(-60.f + q * 31.f, 0.f, 52.f), FRotator(0.f, 0.f, 0.f), EWRLookMesh::Cube);
				for (int32 q = 0; q < 6; ++q) Add(FSurf::Cloth(FLinearColor(0.82f, 0.82f, 0.86f)), FVector(12.f, 12.f, 104.f), FVector(-45.f + q * 31.f, 0.f, 58.f), FRotator(0.f, 0.f, 90.f), EWRLookMesh::Cylinder);
			}
			Cushion(FSurf::Cloth(FLinearColor(0.85f, 0.83f, 0.78f)), FVector(-100.f, -36.f, 46.f), FVector(-74.f, 36.f, 58.f));
		}

		void Hammock()
		{
			// Two posts and a sling hanging between them in a curve (net or spore silk).
			const bool bSilk = V(TEXT("silk"));
			for (const float Y : { -130.f, 130.f })
			{
				Add(Wood, FVector(16.f, 16.f, 170.f), FVector(0.f, Y, 85.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
				Rod(Wood, FVector(0.f, Y, 30.f), FVector(0.f, Y + (Y > 0.f ? 40.f : -40.f), 0.f), 8.f);   // braces
			}
			const FSurf Sling = bSilk ? FSurf::Cloth(FLinearColor(0.85f, 0.82f, 0.74f)) : FSurf::Cloth(FLinearColor(0.42f, 0.36f, 0.24f));
			const int32 N = 12;
			for (int32 i = 0; i < N; ++i)
			{
				const float Y0 = -110.f + i * 220.f / N, Y1 = Y0 + 220.f / N;
				const auto Sag = [](float Y) { return 70.f + 0.0028f * Y * Y; };
				const FVector P0(0.f, Y0, Sag(Y0)), P1(0.f, Y1, Sag(Y1));
				const FVector D = P1 - P0;
				Add(Sling, FVector(70.f, D.Size() + 1.f, 2.f), (P0 + P1) * 0.5f, FRotator(0.f, 0.f, FMath::RadiansToDegrees(FMath::Atan2(D.Z, D.Y))));
			}
			for (const float Y : { -1.f, 1.f }) for (const float X : { -30.f, 0.f, 30.f }) Rod(FSurf::Cloth(FLinearColor(0.4f, 0.34f, 0.22f)), FVector(X, Y * 110.f, 104.f), FVector(0.f, Y * 130.f, 140.f), 1.2f);
			if (bSilk) for (int32 g = 0; g < 4; ++g) Add(FSurf::Lit(Glow.Color, 0.4f), FVector(3.f, 3.f, 3.f), FVector(Jit(FVector(g), 511) * 20.f, -60.f + g * 40.f, 75.f), FRotator(45.f, 0.f, 45.f));
		}

		// ---------------------------------------------------------------- racks and shelves

		void Shelf()
		{
			const bool bOssuary = V(TEXT("ossuary"));
			const FSurf Body = bOssuary ? Main : FSurf::Glaze(Accent.Color * 0.8f, 0.3f);
			Box(Body, FVector(-22.f, -60.f, 0.f), FVector(22.f, -54.f, 190.f));
			Box(Body, FVector(-22.f, 54.f, 0.f), FVector(22.f, 60.f, 190.f));
			Box(Body, FVector(-22.f, -60.f, 0.f), FVector(-18.f, 60.f, 190.f));
			const FSurf Bone = FSurf::Stone(FLinearColor(0.72f, 0.67f, 0.56f), 0.6f);
			for (int32 s = 0; s < 4; ++s)
			{
				const float Z = 10.f + s * 45.f;
				Box(Body, FVector(-20.f, -54.f, Z), FVector(22.f, 54.f, Z + 4.f));
				for (int32 i = 0; i < 4; ++i)
				{
					const float Y = -40.f + i * 27.f;
					if (bOssuary)
					{
						// Skulls in a row and long bones stacked under them.
						Ball(Bone, FVector(4.f, Y, Z + 15.f), 18.f);
						Box(Bone, FVector(-2.f, Y - 6.f, Z + 4.f), FVector(10.f, Y + 6.f, Z + 9.f));
						for (const float D : { -4.f, 4.f }) Box(FSurf::Stone(FLinearColor(0.05f, 0.04f, 0.03f), 0.9f), FVector(12.f, Y + D - 2.f, Z + 15.f), FVector(13.5f, Y + D + 2.f, Z + 19.f));   // eye holes
					}
					else
					{
						// Reliquaries: little lacquered caskets and bone-stoppered jars.
						if ((i + s) % 2) { Box(FSurf::Glaze(Accent.Color, 0.3f), FVector(-8.f, Y - 9.f, Z + 4.f), FVector(12.f, Y + 9.f, Z + 18.f)); Box(Brass, FVector(-8.5f, Y - 2.f, Z + 4.f), FVector(12.5f, Y + 2.f, Z + 18.5f)); }
						else { Add(FSurf::Stone(FLinearColor(0.35f, 0.28f, 0.18f), 0.7f), FVector(14.f, 14.f, 20.f), FVector(2.f, Y, Z + 14.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder); Ball(Bone, FVector(2.f, Y, Z + 26.f), 7.f); }
					}
				}
			}
			Box(Body, FVector(-22.f, -60.f, 186.f), FVector(22.f, 60.f, 192.f));
		}

		void HornRack()
		{
			Box(Wood, FVector(-6.f, -50.f, 0.f), FVector(6.f, -44.f, 180.f));
			Box(Wood, FVector(-6.f, 44.f, 0.f), FVector(6.f, 50.f, 180.f));
			for (const float Z : { 80.f, 140.f })
			{
				Box(Wood, FVector(-4.f, -44.f, Z), FVector(4.f, 44.f, Z + 8.f));
				for (const float Y : { -25.f, 0.f, 25.f })
				{
					// A drinking horn on two pegs, curving up toward its tip.
					const FSurf Horn = FSurf::Glaze(FLinearColor(0.35f, 0.28f, 0.18f), 0.35f);
					Rod(Horn, FVector(8.f, Y - 10.f, Z + 14.f), FVector(10.f, Y + 6.f, Z + 16.f), 9.f);
					Spike(Horn, FVector(10.f, Y + 6.f, Z + 16.f), FVector(0.1f, 0.6f, 0.8f), 7.f, 18.f);
					Disc(Copper, FVector(8.f, Y - 10.f, Z + 14.f), FVector(0.f, 1.f, 0.f), 10.f, 2.f);
				}
			}
			for (const float Y : { -47.f, 47.f }) Box(Wood, FVector(-20.f, Y - 4.f, 0.f), FVector(20.f, Y + 4.f, 6.f));
		}

		void HerbRack()
		{
			// An A-frame drying rack with bunches of herbs hanging head-down from its bars.
			for (const float Y : { -60.f, 60.f })
			{
				Rod(Wood, FVector(-35.f, Y, 0.f), FVector(0.f, Y, 170.f), 6.f, true);
				Rod(Wood, FVector(35.f, Y, 0.f), FVector(0.f, Y, 170.f), 6.f, true);
			}
			for (const float Z : { 165.f, 110.f })
			{
				const float X = (170.f - Z) * 35.f / 170.f;
				for (const float S2 : { -1.f, 1.f })
				{
					if (Z > 160.f && S2 > 0.f) continue;
					Rod(Wood, FVector(S2 * X, -64.f, Z), FVector(S2 * X, 64.f, Z), 4.f);
					for (float Y = -50.f; Y < 51.f; Y += 20.f)
					{
						const FVector Top(S2 * X, Y, Z - 2.f);
						Rod(FSurf::Cloth(FLinearColor(0.4f, 0.32f, 0.18f)), Top, Top - FVector(0.f, 0.f, 8.f), 1.f);
						Spike(FSurf::Cloth(FMath::Lerp(Leaf.Color, FLinearColor(0.3f, 0.25f, 0.1f), Rand(Top, 521))), Top - FVector(0.f, 0.f, 8.f), FVector(0.f, 0.f, -1.f), 12.f, 26.f);
					}
				}
			}
		}

		void KiteRack()
		{
			Box(Main, FVector(-6.f, -70.f, 0.f), FVector(6.f, -64.f, 200.f));
			Box(Main, FVector(-6.f, 64.f, 0.f), FVector(6.f, 70.f, 200.f));
			Box(Main, FVector(-6.f, -70.f, 194.f), FVector(6.f, 70.f, 200.f));
			for (int32 k = 0; k < 3; ++k)
			{
				const FVector C(6.f, -40.f + k * 40.f, 140.f - (k % 2) * 20.f);
				const FLinearColor Col = k == 0 ? FLinearColor(0.7f, 0.15f, 0.1f) : k == 1 ? Second.Color : FLinearColor(0.8f, 0.7f, 0.3f);
				Add(FSurf::Cloth(Col), FVector(1.f, 34.f, 34.f), C, FRotator(0.f, 0.f, 45.f));
				Rod(Wood, C - FVector(0.f, 0.f, 24.f), C + FVector(0.f, 0.f, 24.f), 1.2f);
				Rod(Wood, C - FVector(0.f, 24.f, 0.f), C + FVector(0.f, 24.f, 0.f), 1.2f);
				for (int32 t = 0; t < 4; ++t) Add(FSurf::Cloth(Col * 0.8f), FVector(1.f, 6.f, 8.f), C - FVector(0.f, 0.f, 30.f + t * 12.f) + FVector(0.f, FMath::Sin(t * 1.3f) * 5.f, 0.f), FRotator(0.f, 0.f, 20.f * FMath::Sin(t * 1.5f)));
				Rod(Iron, FVector(6.f, C.Y, 194.f), C + FVector(0.f, 0.f, 24.f), 0.6f);
			}
		}

		void Chimes()
		{
			const bool bBone = V(TEXT("bone")), bTree = V(TEXT("tree"));
			if (bTree)
			{
				// A crystal tree: a dark trunk branching, pendants of crystal hanging from every branch.
				Add(Dark, FVector(12.f, 12.f, 150.f), FVector(0.f, 0.f, 75.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
				for (int32 b = 0; b < 7; ++b)
				{
					const float A2 = b * 0.9f;
					const FVector From(0.f, 0.f, 80.f + b * 12.f), To = From + FVector(FMath::Cos(A2) * 40.f, FMath::Sin(A2) * 40.f, 25.f);
					Rod(Dark, From, To, 4.f);
					Rod(Iron, To, To - FVector(0.f, 0.f, 10.f), 0.5f);
					Add(FSurf::Lit(Accent.Color, 0.8f), FVector(4.f, 4.f, 14.f), To - FVector(0.f, 0.f, 17.f), FRotator(0.f, 45.f, 0.f), EWRLookMesh::Cone);
				}
				for (int32 r = 0; r < 4; ++r) Rod(Dark, FVector(0.f, 0.f, 16.f), FVector(FMath::Cos(r * HALF_PI) * 26.f, FMath::Sin(r * HALF_PI) * 26.f, 0.f), 6.f);
				return;
			}
			// A post and arm, a disc hung from it, tubes (or bones) on cords round the disc's rim.
			Box(Wood, FVector(-6.f, -6.f, 0.f), FVector(6.f, 6.f, 200.f));
			Box(Wood, FVector(-20.f, -20.f, 0.f), FVector(20.f, 20.f, 6.f));
			Box(Wood, FVector(0.f, -4.f, 192.f), FVector(50.f, 4.f, 200.f));
			Rod(Iron, FVector(42.f, 0.f, 192.f), FVector(42.f, 0.f, 175.f), 0.6f);
			Disc(bBone ? FSurf::Stone(FLinearColor(0.7f, 0.64f, 0.52f), 0.6f) : Wood, FVector(42.f, 0.f, 174.f), FVector::UpVector, 26.f, 3.f);
			for (int32 t = 0; t < 6; ++t)
			{
				const float A2 = t * 1.047f;
				const FVector Top(42.f + FMath::Cos(A2) * 11.f, FMath::Sin(A2) * 11.f, 172.f);
				const float L = 26.f + t * 6.f;
				Rod(Iron, Top, Top - FVector(0.f, 0.f, 6.f), 0.5f);
				if (bBone) { Rod(FSurf::Stone(FLinearColor(0.72f, 0.66f, 0.54f), 0.6f), Top - FVector(0.f, 0.f, 6.f), Top - FVector(0.f, 0.f, 6.f + L), 2.4f); Ball(FSurf::Glaze(Accent.Color, 0.3f), Top - FVector(0.f, 0.f, 6.f), 3.f); }
				else Add(Copper, FVector(3.f, 3.f, L), Top - FVector(0.f, 0.f, 6.f + L * 0.5f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			}
		}

		void Bell()
		{
			const bool bSilence = V(TEXT("silence"));
			Gallows(bSilence ? Main : Wood, 50.f, 200.f, 12.f);
			// The bell: a flared shell of stacked rings, a crown, a clapper (the silence bell has none).
			const FSurf Metal = bSilence ? FSurf::Glaze(Second.Color, 0.12f) : FSurf::Steel(FLinearColor(0.42f, 0.3f, 0.14f), 0.35f);
			const float Top = 188.f;
			for (int32 r = 0; r < 6; ++r)
			{
				const float D = 34.f + r * 6.f + (r == 5 ? 6.f : 0.f);
				Add(Metal, FVector(D, D, 10.f), FVector(0.f, 0.f, Top - 14.f - r * 9.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			}
			Add(Metal, FVector(14.f, 14.f, 8.f), FVector(0.f, 0.f, Top - 4.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			if (bSilence) Add(FSurf::Lit(Glow.Color, 1.f), FVector(78.f, 78.f, 1.f), FVector(0.f, 0.f, Top - 64.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);   // the violet lip
			else { Rod(Iron, FVector(0.f, 0.f, Top - 10.f), FVector(0.f, 0.f, Top - 62.f), 2.f); Ball(Iron, FVector(0.f, 0.f, Top - 64.f), 8.f); }
			Rod(FSurf::Cloth(FLinearColor(0.45f, 0.36f, 0.22f)), FVector(0.f, 30.f, Top - 4.f), FVector(0.f, 34.f, 40.f), 1.6f);   // the rope
		}

		void Drum()
		{
			if (V(TEXT("hand")))
			{
				// A frame drum on a little three-legged stand, its beater beside it.
				for (int32 i = 0; i < 3; ++i) Rod(Wood, FVector(FMath::Cos(i * 2.094f) * 20.f, FMath::Sin(i * 2.094f) * 20.f, 0.f), FVector(0.f, 0.f, 60.f), 3.f);
				Disc(Wood, FVector(0.f, 0.f, 70.f), FVector(1.f, 0.f, 0.6f), 46.f, 9.f);
				Disc(Hide, FVector(0.f, 0.f, 70.f) + FVector(1.f, 0.f, 0.6f).GetSafeNormal() * 5.f, FVector(1.f, 0.f, 0.6f), 44.f, 0.8f);
				Rod(Wood, FVector(20.f, 22.f, 0.f), FVector(20.f, 60.f, 2.f), 2.f);
				return;
			}
			// A big storm drum: a hide-headed barrel on a stand, laced with cord, copper studs, two beaters.
			for (const float Y : { -40.f, 40.f }) Rod(Wood, FVector(-30.f, Y, 0.f), FVector(0.f, Y * 0.8f, 70.f), 6.f, true);
			for (const float Y : { -40.f, 40.f }) Rod(Wood, FVector(30.f, Y, 0.f), FVector(0.f, Y * 0.8f, 70.f), 6.f, true);
			Add(Wood, FVector(80.f, 80.f, 70.f), FVector(0.f, 0.f, 100.f), FRotator(90.f, 0.f, 0.f), EWRLookMesh::Cylinder);
			for (const float X : { -35.f, 35.f }) Disc(Hide, FVector(X, 0.f, 100.f), FVector(1.f, 0.f, 0.f), 80.f, 1.f);
			for (int32 l = 0; l < 10; ++l)
			{
				const float A2 = l * 0.628f;
				const FVector R(0.f, FMath::Cos(A2) * 40.5f, 100.f + FMath::Sin(A2) * 40.5f), R2(0.f, FMath::Cos(A2 + 0.314f) * 40.5f, 100.f + FMath::Sin(A2 + 0.314f) * 40.5f);
				Rod(FSurf::Cloth(FLinearColor(0.5f, 0.4f, 0.25f)), R + FVector(-34.f, 0.f, 0.f), R2 + FVector(34.f, 0.f, 0.f), 1.f);
				Add(Copper, FVector(2.5f, 2.5f, 2.5f), R + FVector(-30.f, 0.f, 0.f));
			}
			for (const float Y : { -15.f, 15.f }) Rod(Wood, FVector(40.f, Y, 140.f), FVector(55.f, Y, 110.f), 2.5f);
		}

		void Harp()
		{
			const bool bWind = V(TEXT("wind")), bLyre = V(TEXT("lyre"));
			if (bLyre)
			{
				// A lyre on a small stand: a sound box, two curved arms, a yoke and strings.
				Box(Wood, FVector(-15.f, -15.f, 0.f), FVector(15.f, 15.f, 40.f));
				Box(Wood, FVector(-4.f, -16.f, 40.f), FVector(4.f, 16.f, 60.f));
				for (const float S2 : { -1.f, 1.f }) { Rod(Wood, FVector(0.f, S2 * 14.f, 58.f), FVector(0.f, S2 * 20.f, 85.f), 3.f); Rod(Wood, FVector(0.f, S2 * 20.f, 85.f), FVector(0.f, S2 * 15.f, 100.f), 3.f); }
				Rod(Wood, FVector(0.f, -16.f, 98.f), FVector(0.f, 16.f, 98.f), 3.f);
				for (int32 s = 0; s < 7; ++s) Rod(Brass, FVector(4.5f, -9.f + s * 3.f, 58.f), FVector(1.5f, -9.f + s * 3.f, 97.f), 0.4f);
				return;
			}
			if (bWind)
			{
				// An aeolian harp: a tall frame on a post, strings stretched for the wind to play.
				Box(Main, FVector(-20.f, -20.f, 0.f), FVector(20.f, 20.f, 10.f));
				Rod(Main, FVector(0.f, 0.f, 10.f), FVector(0.f, 0.f, 120.f), 8.f);
				Box(Wood, FVector(-6.f, -40.f, 120.f), FVector(6.f, 40.f, 128.f));
				Box(Wood, FVector(-6.f, -40.f, 220.f), FVector(6.f, 40.f, 228.f));
				for (const float Y : { -40.f, 40.f }) Box(Wood, FVector(-6.f, Y - 4.f, 120.f), FVector(6.f, Y + 4.f, 228.f));
				for (int32 s = 0; s < 10; ++s) Rod(Brass, FVector(0.f, -34.f + s * 7.5f, 128.f), FVector(0.f, -34.f + s * 7.5f, 220.f), 0.5f);
				Spike(FSurf::Lit(Glow.Color, 0.4f), FVector(0.f, 0.f, 228.f), FVector::UpVector, 8.f, 16.f);
				return;
			}
			// A floor harp: the column at the front, the curved neck, the sound box sloping back, strings between.
			Box(Wood, FVector(-30.f, -18.f, 0.f), FVector(30.f, 18.f, 8.f));
			Rod(Wood, FVector(20.f, 0.f, 8.f), FVector(22.f, 0.f, 160.f), 7.f);
			Rod(Wood, FVector(-25.f, 0.f, 8.f), FVector(-20.f, 0.f, 120.f), 16.f, true);
			Rod(Wood, FVector(-20.f, 0.f, 120.f), FVector(0.f, 0.f, 152.f), 7.f);
			Rod(Wood, FVector(0.f, 0.f, 152.f), FVector(22.f, 0.f, 160.f), 7.f);
			for (int32 s = 0; s < 12; ++s)
			{
				const float T2 = s / 11.f;
				const FVector Bottom = FMath::Lerp(FVector(-25.f, 0.f, 20.f), FVector(-20.f, 0.f, 116.f), T2);
				const FVector Top = FMath::Lerp(FVector(20.f, 0.f, 158.f), FVector(-18.f, 0.f, 124.f), 1.f - T2);
				Rod(Brass, Bottom + FVector(4.f, 0.f, 0.f), FVector(Bottom.X + 4.f + (Top.X - Bottom.X) * 0.85f, 0.f, Top.Z - 2.f), 0.5f);
			}
			if (Tier) Add(FSurf::Lit(Glow.Color, 0.35f), FVector(5.f, 3.f, 5.f), FVector(22.f, 0.f, 163.f), FRotator(45.f, 0.f, 0.f));
		}

		void Globe()
		{
			// A star globe: an armillary of brass rings round a dark sphere pricked with lit stars, on a stand.
			Box(Main, FVector(-24.f, -24.f, 0.f), FVector(24.f, 24.f, 10.f));
			Add(Main, FVector(12.f, 12.f, 80.f), FVector(0.f, 0.f, 50.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			const FVector C(0.f, 0.f, 125.f);
			Ball(FSurf::Stone(FLinearColor(0.02f, 0.025f, 0.05f), 0.4f), C, 50.f);
			for (int32 s = 0; s < 16; ++s)
			{
				const FVector Dir = FVector(Jit(FVector(s), 531), Jit(FVector(s, 1.f, 0.f), 532), Jit(FVector(s, 2.f, 0.f), 533)).GetSafeNormal();
				Add(FSurf::Lit(FLinearColor(0.85f, 0.9f, 1.f), 1.2f), FVector(1.6f, 1.6f, 1.6f), C + Dir * 25.f);
			}
			Ring(Brass, C, 31.f, 2.f, 2.f, 20);
			Frame = FTransform(FRotator(0.f, 90.f, 0.f), C);
			Ring(Brass, FVector::ZeroVector, 33.f, 2.f, 2.f, 20);
			Frame = FTransform(FRotator(0.f, 0.f, 90.f), C);
			Ring(Brass, FVector::ZeroVector, 35.f, 2.f, 2.f, 20);
			Frame = FTransform::Identity;
			Rod(Brass, C - FVector(0.f, 0.f, 40.f), C + FVector(0.f, 0.f, 40.f), 1.5f);
		}

		void CrownStand()
		{
			// A pedestal, a cushion, a broken crown: a ring of points with a third of it missing.
			Box(Main, FVector(-26.f, -26.f, 0.f), FVector(26.f, 26.f, 12.f));
			Box(Main, FVector(-16.f, -16.f, 12.f), FVector(16.f, 16.f, 100.f));
			Box(Main, FVector(-24.f, -24.f, 100.f), FVector(24.f, 24.f, 108.f));
			Cushion(FSurf::Cloth(FLinearColor(0.12f, 0.03f, 0.16f)), FVector(-20.f, -20.f, 108.f), FVector(20.f, 20.f, 118.f));
			const FSurf Gold = FSurf::Steel(FLinearColor(0.6f, 0.45f, 0.15f), 0.3f);
			for (int32 i = 0; i < 12; ++i)
			{
				if (i >= 8) continue;   // broken away
				const float A1 = i * PI / 6.f, A2 = (i + 1) * PI / 6.f;
				const FVector P1(FMath::Cos(A1) * 14.f, FMath::Sin(A1) * 14.f, 124.f), P2(FMath::Cos(A2) * 14.f, FMath::Sin(A2) * 14.f, 124.f);
				if (i < 7) Rod(Gold, P1, P2, 3.f, true);
				Spike(Gold, P1, FVector::UpVector, 3.f, i % 2 ? 8.f : 13.f);
			}
			Add(Gold, FVector(4.f, 6.f, 3.f), FVector(-16.f, -6.f, 119.f), FRotator(20.f, 30.f, 60.f));   // a fallen piece
			Add(FSurf::Lit(Glow.Color, 0.8f), FVector(3.f, 3.f, 3.f), FVector(14.f, 0.f, 126.f), FRotator(45.f, 45.f, 0.f));
		}

		void Lectern()
		{
			Box(Main, FVector(-24.f, -24.f, 0.f), FVector(24.f, 24.f, 10.f));
			Add(Main, FVector(16.f, 16.f, 100.f), FVector(0.f, 0.f, 60.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			Add(Wood, FVector(50.f, 60.f, 4.f), FVector(4.f, 0.f, 118.f), FRotator(-25.f, 0.f, 0.f));
			Add(FSurf::Cloth(FLinearColor(0.68f, 0.62f, 0.5f)), FVector(36.f, 50.f, 3.f), FVector(5.f, 0.f, 121.5f), FRotator(-25.f, 0.f, 0.f));   // the open book
			Add(FSurf::Cloth(FLinearColor(0.25f, 0.04f, 0.06f)), FVector(1.f, 1.f, 30.f), FVector(16.f, 2.f, 108.f), FRotator(-25.f, 0.f, 0.f));   // its ribbon
			Candle(FVector(-10.f, 26.f, 112.f), 10.f);
		}

		void Mirror()
		{
			// A cheval mirror: a tall frame on two feet, the glass in it (black glass in a void-rimmed frame).
			const bool bBlack = V(TEXT("black"));
			const FSurf Frame2 = bBlack ? FSurf::Glaze(Second.Color, 0.12f) : Tier ? Main : Wood;
			for (const float Y : { -38.f, 38.f })
			{
				Box(Frame2, FVector(-22.f, Y - 4.f, 0.f), FVector(22.f, Y + 4.f, 6.f));
				Box(Frame2, FVector(-3.f, Y - 3.f, 6.f), FVector(3.f, Y + 3.f, 180.f));
			}
			Box(Frame2, FVector(-3.f, -34.f, 30.f), FVector(3.f, 34.f, 36.f));
			Box(Frame2, FVector(-3.f, -34.f, 174.f), FVector(3.f, 34.f, 180.f));
			Box(bBlack ? FSurf::Glass(FLinearColor(0.01f, 0.008f, 0.015f)) : FSurf::Steel(FLinearColor(0.85f, 0.87f, 0.9f), 0.03f), FVector(-1.f, -34.f, 36.f), FVector(1.f, 34.f, 174.f));
			if (bBlack) for (const float Y : { -34.f, 34.f }) Box(FSurf::Lit(Glow.Color, 0.9f), FVector(-3.5f, Y - 0.6f, 36.f), FVector(3.5f, Y + 0.6f, 174.f));
		}

		void Loom()
		{
			// An upright rug loom: two posts, beams top and bottom, the warp strung, a rug half woven up from the bottom.
			for (const float Y : { -70.f, 70.f }) { Box(Wood, FVector(-6.f, Y - 5.f, 0.f), FVector(6.f, Y + 5.f, 200.f)); Box(Wood, FVector(-30.f, Y - 5.f, 0.f), FVector(30.f, Y + 5.f, 8.f)); }
			for (const float Z : { 30.f, 190.f }) Add(Wood, FVector(10.f, 10.f, 150.f), FVector(0.f, 0.f, Z), FRotator(0.f, 0.f, 90.f), EWRLookMesh::Cylinder);
			for (float Y = -60.f; Y < 61.f; Y += 5.f) Rod(FSurf::Cloth(FLinearColor(0.75f, 0.7f, 0.6f)), FVector(0.f, Y, 34.f), FVector(0.f, Y, 186.f), 0.6f);
			Box(Cloth, FVector(-1.5f, -60.f, 34.f), FVector(1.5f, 60.f, 110.f));
			for (int32 b = 0; b < 4; ++b) Box(FSurf::Cloth(Second.Color), FVector(-1.8f, -60.f, 44.f + b * 18.f), FVector(1.8f, 60.f, 48.f + b * 18.f));
			Add(Wood, FVector(6.f, 6.f, 100.f), FVector(4.f, 0.f, 114.f), FRotator(0.f, 0.f, 90.f), EWRLookMesh::Cylinder);   // the shuttle bar
		}

		void Sundial()
		{
			// A pedestal, a dial of sandstone with sunstone hour marks, a bronze gnomon.
			Box(Main, FVector(-22.f, -22.f, 0.f), FVector(22.f, 22.f, 10.f));
			Add(Main, FVector(26.f, 26.f, 80.f), FVector(0.f, 0.f, 50.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			Disc(Main, FVector(0.f, 0.f, 93.f), FVector::UpVector, 70.f, 6.f);
			for (int32 h = 0; h < 12; ++h)
			{
				const float A2 = PI * (0.1f + h * 0.8f / 11.f);
				Add(FSurf::Lit(Glow.Color, 0.6f), FVector(4.f, 1.5f, 1.f), FVector(FMath::Cos(A2) * 28.f, FMath::Sin(A2) * 28.f, 96.4f), FRotator(0.f, FMath::RadiansToDegrees(A2), 0.f));
			}
			Add(Brass, FVector(30.f, 1.5f, 22.f), FVector(0.f, 0.f, 105.f), FRotator(30.f, 90.f, 0.f));
		}

		// ---------------------------------------------------------------- floor and wall cloth, plants

		void Rug()
		{
			// A woven rug in the set's colours: a field, a border, an inner band, a motif, fringe at both ends.
			const FSurf Field = FSurf::Cloth(Tier ? FMath::Lerp(Tier->Look.B, Tier->Look.C, 0.25f) : FLinearColor(0.5f, 0.1f, 0.1f));
			const FSurf Border = FSurf::Cloth(Tier ? Tier->Look.A : FLinearColor(0.56f, 0.45f, 0.27f));
			const FSurf Motif = Tier && Tier->Look.Glow.GetLuminance() > 0.01f ? FSurf::Cloth(Tier->Look.Glow * 0.6f) : FSurf::Cloth(FLinearColor(0.56f, 0.45f, 0.27f));
			Box(Field, FVector(-130.f, -90.f, 0.f), FVector(130.f, 90.f, 1.6f));
			Box(Border, FVector(-130.f, -90.f, 1.6f), FVector(130.f, -78.f, 2.f));
			Box(Border, FVector(-130.f, 78.f, 1.6f), FVector(130.f, 90.f, 2.f));
			Box(Border, FVector(-130.f, -78.f, 1.6f), FVector(-118.f, 78.f, 2.f));
			Box(Border, FVector(118.f, -78.f, 1.6f), FVector(130.f, 78.f, 2.f));
			Box(Motif, FVector(-95.f, -55.f, 1.6f), FVector(95.f, -51.f, 2.f));
			Box(Motif, FVector(-95.f, 51.f, 1.6f), FVector(95.f, 55.f, 2.f));
			for (const float X : { -60.f, 0.f, 60.f }) Add(Motif, FVector(28.f, 28.f, 0.4f), FVector(X, 0.f, 1.8f), FRotator(0.f, 45.f, 0.f));
			for (const float End : { -1.f, 1.f }) for (int32 t = 0; t < 18; ++t)
			{
				const float Y = -85.f + t * 10.f;
				Box(Border, FVector(End > 0.f ? 130.f : -137.f, Y - 1.5f, 0.f), FVector(End > 0.f ? 137.f : -130.f, Y + 1.5f, 0.8f));
			}
		}

		void PeltRug()
		{
			// A wolf pelt laid flat: the hide spread, four legs, the tail, the head with its ears up.
			const FSurf Grey = FSurf::Cloth(FLinearColor(0.34f, 0.33f, 0.31f)), Pale = FSurf::Cloth(FLinearColor(0.55f, 0.53f, 0.5f));
			Add(Grey, FVector(170.f, 90.f, 2.f), FVector(0.f, 0.f, 1.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			Add(Pale, FVector(120.f, 50.f, 2.2f), FVector(0.f, 0.f, 1.2f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			for (const float X : { -55.f, 55.f }) for (const float Y : { -1.f, 1.f })
			{
				Add(Grey, FVector(60.f, 18.f, 1.8f), FVector(X + (X > 0.f ? 10.f : -10.f), Y * 55.f, 0.9f), FRotator(0.f, Y * (X > 0.f ? 30.f : -30.f), 0.f));
			}
			Add(Grey, FVector(70.f, 14.f, 1.8f), FVector(-115.f, 0.f, 0.9f), FRotator::ZeroRotator);
			Add(Grey, FVector(40.f, 34.f, 10.f), FVector(100.f, 0.f, 5.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			Spike(Grey, FVector(115.f, 0.f, 6.f), FVector(1.f, 0.f, 0.f), 16.f, 24.f);
			for (const float Y : { -9.f, 9.f }) Spike(Grey, FVector(96.f, Y, 9.f), FVector(0.f, Y * 0.04f, 1.f), 7.f, 11.f);
		}

		void Tapestry()
		{
			// A woven hanging on a standing frame: a rod, the cloth, a border, a motif, tassels.
			for (const float Y : { -80.f, 80.f }) { Box(Wood, FVector(-4.f, Y - 4.f, 0.f), FVector(4.f, Y + 4.f, 240.f)); Box(Wood, FVector(-28.f, Y - 4.f, 0.f), FVector(28.f, Y + 4.f, 6.f)); }
			Add(Wood, FVector(6.f, 6.f, 172.f), FVector(4.f, 0.f, 232.f), FRotator(0.f, 0.f, 90.f), EWRLookMesh::Cylinder);
			const FSurf Field = FSurf::Cloth(Tier ? Tier->Look.B : FLinearColor(0.3f, 0.08f, 0.06f));
			const FSurf Border = FSurf::Cloth(Tier ? Tier->Look.C * 0.8f : FLinearColor(0.56f, 0.45f, 0.27f));
			Box(Field, FVector(5.f, -72.f, 70.f), FVector(7.f, 72.f, 228.f));
			Box(Border, FVector(7.f, -72.f, 70.f), FVector(7.5f, 72.f, 78.f));
			Box(Border, FVector(7.f, -72.f, 220.f), FVector(7.5f, 72.f, 228.f));
			Box(Border, FVector(7.f, -72.f, 78.f), FVector(7.5f, -64.f, 220.f));
			Box(Border, FVector(7.f, 64.f, 78.f), FVector(7.5f, 72.f, 220.f));
			// A wolf and a tree (the Rime lodge), or a sunburst of the set's colour.
			Add(Border, FVector(0.5f, 40.f, 40.f), FVector(7.5f, 0.f, 150.f), FRotator(0.f, 0.f, 45.f));
			for (int32 r = 0; r < 8; ++r) Add(Border, FVector(0.5f, 4.f, 30.f), FVector(7.5f, 0.f, 150.f) + FRotator(0.f, 0.f, r * 45.f).RotateVector(FVector(0.f, 0.f, 36.f)), FRotator(0.f, 0.f, r * 45.f));
			for (int32 t = 0; t < 12; ++t) Box(Border, FVector(5.5f, -66.f + t * 12.f - 1.2f, 58.f), FVector(7.f, -66.f + t * 12.f + 1.2f, 70.f));
		}

		void Planter()
		{
			const bool bTrough = V(TEXT("trough")), bFlower = V(TEXT("flower"));
			if (bTrough)
			{
				// A carved trough of soil with a row of flowering plants.
				Box(Wood, FVector(-22.f, -80.f, 0.f), FVector(22.f, 80.f, 40.f));
				Box(FSurf::Cloth(FLinearColor(0.07f, 0.045f, 0.025f)), FVector(-18.f, -76.f, 40.f), FVector(18.f, 76.f, 41.f));
				for (int32 p = 0; p < 6; ++p)
				{
					const FVector Base(Jit(FVector(p), 541) * 8.f, -65.f + p * 26.f, 41.f);
					Rod(Leaf, Base, Base + FVector(0.f, 0.f, 22.f + p % 3 * 6.f), 1.5f);
					Spike(Leaf, Base, FVector(0.3f, 0.f, 1.f), 10.f, 16.f);
					Ball(FSurf::Cloth(p % 2 ? FLinearColor(0.7f, 0.2f, 0.25f) : FLinearColor(0.85f, 0.7f, 0.2f)), Base + FVector(0.f, 0.f, 24.f + p % 3 * 6.f), 8.f);
				}
				return;
			}
			// A clay pot of soil with a leafy plant (or one in flower).
			const FSurf Clay = FSurf::Stone(FLinearColor(0.38f, 0.17f, 0.08f), 0.85f);
			Add(Clay, FVector(34.f, 34.f, 34.f), FVector(0.f, 0.f, 17.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			Add(Clay, FVector(40.f, 40.f, 5.f), FVector(0.f, 0.f, 33.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			Add(FSurf::Cloth(FLinearColor(0.07f, 0.045f, 0.025f)), FVector(32.f, 32.f, 1.f), FVector(0.f, 0.f, 35.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			for (int32 l = 0; l < 7; ++l)
			{
				const FVector Dir = FVector(FMath::Cos(l * 0.9f) * 0.6f, FMath::Sin(l * 0.9f) * 0.6f, 1.f).GetSafeNormal();
				Spike(Leaf, FVector(0.f, 0.f, 35.f), Dir, 12.f, 30.f + (l % 3) * 10.f);
			}
			if (bFlower) for (int32 f = 0; f < 4; ++f)
			{
				const FVector C(FMath::Cos(f * 1.57f) * 10.f, FMath::Sin(f * 1.57f) * 10.f, 72.f + f * 4.f);
				Rod(Leaf, FVector(0.f, 0.f, 35.f), C, 1.2f);
				for (int32 p = 0; p < 5; ++p) Add(FSurf::Cloth(FLinearColor(0.75f, 0.22f, 0.45f)), FVector(6.f, 3.f, 0.8f), C + FRotator(0.f, p * 72.f, 0.f).RotateVector(FVector(3.f, 0.f, 0.f)), FRotator(0.f, p * 72.f, 0.f));
				Ball(FSurf::Cloth(FLinearColor(0.9f, 0.8f, 0.2f)), C + FVector(0.f, 0.f, 0.6f), 2.5f);
			}
		}
	};

	/** The light's place in a lamp (its flame, globe or crystal), the model's own space. */
	FVector LightOf(const FWRPieceDef& Def)
	{
		const FName A = Def.Arch;
		if (A == TEXT("lamp_stand")) return FVector(0.f, 0.f, 148.f);
		if (A == TEXT("rod_lamp")) return FVector(0.f, 0.f, 168.f);
		if (A == TEXT("lantern_hang")) return FVector(34.f, 0.f, 156.f);
		if (A == TEXT("lamp_float")) return FVector(0.f, 0.f, 125.f);
		if (A == TEXT("crystal_lamp")) return FVector(0.f, 0.f, 70.f);
		if (A == TEXT("glowcap")) return FVector(0.f, 0.f, 110.f);
		if (A == TEXT("grub_jar")) return FVector(0.f, 0.f, 70.f);
		if (A == TEXT("toad_lamp")) return FVector(36.f, 0.f, 82.f);
		if (A == TEXT("lantern_tree")) return FVector(0.f, 0.f, 120.f);
		if (A == TEXT("chandelier")) return FVector(0.f, 0.f, 186.f);
		if (A == TEXT("candle_rack")) return FVector(0.f, 0.f, 110.f);
		if (A == TEXT("candles")) return FVector(0.f, 0.f, 128.f);
		if (A == TEXT("lantern")) return FVector(0.f, 0.f, 156.f);
		if (A == TEXT("fireplace")) return FVector(10.f, 0.f, 40.f);
		if (A == TEXT("cauldron")) return FVector(0.f, 0.f, 30.f);
		return FVector(0.f, 0.f, 120.f);
	}
}

TArray<FWRLookBlock> FWRBuildLook::DecorBlocks(const FWRPieceDef& Def, const FTransform& PieceToWorld)
{
	using namespace WRBuildLookDecorDetail;
	TArray<FWRLookBlock> Out;
	FDecor D(PieceToWorld, Out, Def);
	D.Build();
	return Out;
}

TArray<TPair<FVector, FVector>> FWRBuildLook::DecorCollision(const FWRPieceDef& Def)
{
	// The box round what it draws (each block's turned corners), thinner for rugs: you walk over a rug.
	TArray<TPair<FVector, FVector>> Out;
	FBox Bounds(ForceInit);
	for (const FWRLookBlock& B : DecorBlocks(Def, FTransform::Identity))
	{
		const FVector H = B.Size * 0.5f;
		for (int32 i = 0; i < 8; ++i)
		{
			const FVector Corner((i & 1) ? H.X : -H.X, (i & 2) ? H.Y : -H.Y, (i & 4) ? H.Z : -H.Z);
			Bounds += B.Centre + B.Rot.RotateVector(Corner);
		}
	}
	if (!Bounds.IsValid) Bounds = FBox(FVector(-40.f, -40.f, 0.f), FVector(40.f, 40.f, 80.f));
	if (Def.Arch == TEXT("rug") || Def.Arch == TEXT("pelt_rug")) Bounds.Max.Z = FMath::Min(Bounds.Max.Z, 2.f);
	Bounds.Min.Z = FMath::Max(0.f, Bounds.Min.Z);
	Out.Add({ Bounds.GetSize().ComponentMax(FVector(2.f)), Bounds.GetCenter() });
	return Out;
}

FVector FWRBuildLook::DecorLightAt(const FWRPieceDef& Def)
{
	return WRBuildLookDecorDetail::LightOf(Def);
}
