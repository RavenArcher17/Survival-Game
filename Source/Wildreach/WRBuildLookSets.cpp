// How metal and the 12 biome building sets look, and every tier's beams, posts and braces (cooks/build-building.md
// 2.2 / 2.3). Each tier style draws the 12 grid shapes over their unchanged collision, as WRBuildLook draws wood and
// stone: walls from -200 to 200 along X (corner members on the cell corner, shared by the wall that meets it), 20 thick,
// 300 / 150 / 210 tall; a window's opening X +-80, Z 100..220; a door's X +-70 up to 240; a floor's top at 20, a
// foundation's at 100; a roof's underside Z = (X + 200) / 2 from the eave at X -240; a gable low at -X, 210 at +X.

#include "WRBuildLookKit.h"
#include "WRBuilding.h"
#include "WRBuildSets.h"

namespace WRBuildLookSetsDetail
{
	using namespace WRLookKit;
	static constexpr float Corner = 200.f;

	struct FStyle : FKit
	{
		FStyle(const FTransform& InWorld, TArray<FWRLookBlock>& InOut, const FWRTierDef& InTier)
			: FKit(InWorld, InOut), Tier(InTier), Style(InTier.Look.Style), P(InTier.Look) {}
		const FWRTierDef& Tier;
		FName Style;
		FPalette P;
		bool Is(const TCHAR* S) const { return Style == FName(S); }
		/** Styles framed in timber or stems (the frame is drawn by the wall, the fill is infill). */
		bool Framed() const { return Is(TEXT("timberframe")) || Is(TEXT("reeds")) || Is(TEXT("felt")); }
		/** Styles whose openings are arches (round, pointed, the porthole). */
		bool Arched() const { return Is(TEXT("sandstone")) || Is(TEXT("whitestone")) || Is(TEXT("chapel")) || Is(TEXT("coral")) || Is(TEXT("felt")); }
		bool Pointed() const { return Is(TEXT("chapel")); }
		bool TimberStairs() const { return Is(TEXT("timberframe")) || Is(TEXT("logs")) || Is(TEXT("reeds")) || Is(TEXT("felt")); }

		// ---------------------------------------------------------------- shared fills

		/** Ashlar in running bond (X0..X1 x Z0..Z1, Thick round Y 0): courses Course tall from CourseZ0, blocks MinLen + up
		 *  to VarLen long, gaps, every AltEvery-th course in Alt (crag slate bands). A dark backing shows in the joints. */
		void Courses(const FSurf& S, float X0, float X1, float Z0, float Z1, float Thick, float Course, float MinLen, float VarLen, float Gap,
			float CourseZ0 = 0.f, int32 AltEvery = 0, const FSurf* Alt = nullptr)
		{
			if (X1 - X0 < 3.f || Z1 - Z0 < 3.f) return;
			for (int32 k = FMath::FloorToInt((Z0 - CourseZ0) / Course + 0.001f); CourseZ0 + k * Course < Z1 - 0.5f; ++k)
			{
				const float CZ0 = FMath::Max(Z0, CourseZ0 + k * Course), CZ1 = FMath::Min(Z1, CourseZ0 + (k + 1) * Course);
				if (CZ1 - CZ0 < Gap + 1.5f) continue;
				const bool bAlt = Alt && AltEvery > 0 && ((k % AltEvery) + AltEvery) % AltEvery == AltEvery - 1;
				float X = -Corner - ((k & 1) ? MinLen * 0.55f : 0.f) - Rand(FVector(0.f, 0.f, CourseZ0 + k * Course), 5) * 18.f;
				for (int32 b = 0; X < X1 && b < 64; ++b)
				{
					const float Len = MinLen + Rand(FVector(X, 0.f, CourseZ0 + k * Course), 6) * VarLen;
					const float BX0 = FMath::Max(X, X0), BX1 = FMath::Min(X + Len - Gap, X1);
					if (BX1 - BX0 > 2.5f)
					{
						const FVector C((BX0 + BX1) * 0.5f, 0.f, (CZ0 + CZ1 - Gap) * 0.5f);
						Add(bAlt ? *Alt : S, FVector(BX1 - BX0, Thick + Jit(C, 7) * 1.2f, CZ1 - CZ0 - Gap), C + FVector(0.f, Jit(C, 8) * 0.5f, 0.f));
					}
					X += Len;
				}
			}
			Box(P.Dark, FVector(X0, -Thick * 0.25f, Z0), FVector(X1, Thick * 0.25f, Z1));
		}

		/** Boards along X filling X0..X1 x Z0..Z1 of a wall (horizontal), with gaps over a dark backing. */
		void Boards(const FSurf& S, float X0, float X1, float Z0, float Z1, float Thick, float Board = 24.f)
		{
			if (X1 - X0 < 3.f || Z1 - Z0 < 3.f) return;
			const int32 N = FMath::Max(1, FMath::RoundToInt((Z1 - Z0) / (Board + 2.f)));
			const float Pitch = (Z1 - Z0) / N;
			for (int32 k = 0; k < N; ++k)
			{
				const FVector C((X0 + X1) * 0.5f, 0.f, Z0 + k * Pitch + (Pitch - 2.f) * 0.5f);
				Add(S, FVector(X1 - X0, Thick + Jit(C, 3) * 0.8f, Pitch - 2.f), C);
			}
			Box(P.Dark, FVector(X0, -Thick * 0.25f, Z0), FVector(X1, Thick * 0.25f, Z1));
		}

		/** Flat stones over X0..X1 x Y0..Y1 with tops at ZTop (a floor). */
		void Flags(const FSurf& S, float X0, float X1, float Y0, float Y1, float ZTop, float Thick, float MinSize = 45.f, float VarSize = 40.f, float Gap = 2.f)
		{
			for (float Y = Y0; Y < Y1 - 3.f;)
			{
				const float Row = FMath::Min(MinSize + Rand(FVector(0.f, Y, ZTop), 31) * VarSize * 0.8f, Y1 - Y);
				for (float X = X0 - Rand(FVector(1.f, Y, ZTop), 32) * 40.f; X < X1 - 3.f;)
				{
					const float Len = MinSize + Rand(FVector(X, Y, ZTop), 33) * VarSize;
					const float BX0 = FMath::Max(X, X0), BX1 = FMath::Min(X + Len - Gap, X1);
					if (BX1 - BX0 > 3.f)
					{
						const FVector C((BX0 + BX1) * 0.5f, Y + (Row - Gap) * 0.5f, ZTop - Thick * 0.5f);
						Add(S, FVector(BX1 - BX0, Row - Gap, Thick), C + FVector(0.f, 0.f, Jit(C, 34) * 0.4f));
					}
					X += Len;
				}
				Y += Row;
			}
		}

		/** Floorboards along X over a cell, tops at ZTop, joints staggered. */
		void Floorboards(const FSurf& S, float ZTop, float Thick, float Board)
		{
			const int32 N = FMath::Max(1, FMath::RoundToInt(400.f / (Board + 1.5f)));
			const float Pitch = 400.f / N;
			for (int32 r = 0; r < N; ++r)
			{
				const float YC = -Corner + r * Pitch + Pitch * 0.5f;
				const float Split = -Corner + (0.25f + 0.5f * Rand(FVector(0.f, YC, ZTop), 21)) * 400.f;
				for (int32 Half = 0; Half < 2; ++Half)
				{
					const float BX0 = Half ? Split + 0.75f : -Corner, BX1 = Half ? Corner : Split - 0.75f;
					const FVector C((BX0 + BX1) * 0.5f, YC, ZTop - Thick * 0.5f);
					Add(S, FVector(BX1 - BX0, Pitch - 1.5f, Thick), C + FVector(0.f, 0.f, Jit(C, 22) * 0.3f));
				}
			}
		}

		/** Small square studs (rivets, bolt heads) along a line on a face. */
		void Studs(const FSurf& S, const FVector& From, const FVector& To, float Every, float Size)
		{
			const float Len = FVector::Dist(From, To);
			const int32 N = FMath::Max(1, FMath::FloorToInt(Len / Every));
			for (int32 i = 0; i <= N; ++i)
			{
				Add(S, FVector(Size, Size, Size), FMath::Lerp(From, To, N > 0 ? static_cast<float>(i) / N : 0.f));
			}
		}

		// ---------------------------------------------------------------- the wall's fill, corners and top

		/** The body of a wall between X0..X1, Z0..Z1 (Thick round Y 0), in this style. */
		void Fill(float X0, float X1, float Z0, float Z1, float Thick = 20.f)
		{
			if (X1 - X0 < 3.f || Z1 - Z0 < 3.f) return;
			const float H = Z1 - Z0;
			if (Is(TEXT("panels")))
			{
				// A stone footing to 45, then riveted steel panels on a dark core, both faces.
				const float Foot = 45.f;
				if (Z0 < Foot)
				{
					Courses(FSurf::Stone(P.B.Color), X0, X1, Z0, FMath::Min(Z1, Foot), Thick + 4.f, 22.5f, 50.f, 30.f, 1.6f);
					Z0 = Foot;
					if (Z1 - Z0 < 3.f) return;
				}
				Box(P.Dark, FVector(X0, -Thick * 0.3f, Z0), FVector(X1, Thick * 0.3f, Z1));
				const int32 N = FMath::Max(1, FMath::RoundToInt((X1 - X0) / 105.f));
				const float Pitch = (X1 - X0) / N;
				const FSurf Rivet = FSurf::Steel(P.A.Color * 0.8f, 0.35f);
				for (const float Face : { -1.f, 1.f })
				{
					const float Y0 = Face * Thick * 0.3f, Y1 = Face * (Thick * 0.5f - 0.5f);
					for (int32 i = 0; i < N; ++i)
					{
						const float A = X0 + i * Pitch + 1.f, B = X0 + (i + 1) * Pitch - 1.f;
						Box(P.A, FVector(A, FMath::Min(Y0, Y1), Z0 + 1.f), FVector(B, FMath::Max(Y0, Y1), Z1 - 1.f), 0.92f + 0.16f * Rand(FVector(A, Face, Z0), 41));
						const float YR = Face * (Thick * 0.5f + 0.3f);
						Studs(Rivet, FVector(A + 4.f, YR, Z0 + 6.f), FVector(A + 4.f, YR, Z1 - 6.f), 20.f, 2.2f);
						Studs(Rivet, FVector(A + 12.f, YR, Z1 - 5.f), FVector(B - 12.f, YR, Z1 - 5.f), 20.f, 2.2f);
						Studs(Rivet, FVector(A + 12.f, YR, Z0 + 5.f), FVector(B - 12.f, YR, Z0 + 5.f), 20.f, 2.2f);
					}
					Studs(Rivet, FVector(X1 - 4.f, Face * (Thick * 0.5f + 0.3f), Z0 + 6.f), FVector(X1 - 4.f, Face * (Thick * 0.5f + 0.3f), Z1 - 6.f), 20.f, 2.2f);
				}
				return;
			}
			if (Is(TEXT("columns")))
			{
				// Basalt: hexagonal columns packed along the wall, split into drums at uneven joints.
				Box(P.Dark, FVector(X0, -6.f, Z0), FVector(X1, 6.f, Z1));
				const float R = 15.f, Step = 26.f;
				for (float X = X0 + R * 0.85f; X <= X1 - R * 0.55f; X += Step)
				{
					float Z = Z0;
					for (int32 d = 0; Z < Z1 - 1.f && d < 8; ++d)
					{
						const float Hd = FMath::Min(65.f + Rand(FVector(X, 0.f, Z), 51) * 70.f, Z1 - Z);
						Hex(P.A, X + Jit(FVector(X, 0.f, Z), 52) * 0.8f, Jit(FVector(X, 1.f, Z), 53) * 1.2f, Z, Z + Hd - (Z + Hd < Z1 - 1.f ? 1.4f : 0.f), R, Jit(FVector(X, 2.f, 0.f), 54) * 6.f);
						Z += Hd;
					}
				}
				return;
			}
			if (Is(TEXT("granite")))
			{
				const FSurf Slate = FSurf::Stone(P.B.Color, 0.75f);
				Courses(P.A, X0, X1, Z0, Z1, Thick, 37.5f, 50.f, 45.f, 1.8f, 0.f, 3, &Slate);
				return;
			}
			if (Is(TEXT("sandstone")))
			{
				Courses(P.A, X0, X1, Z0, Z1, Thick, 50.f, 70.f, 45.f, 1.2f);
				// The sunstone band at 2 m: amber diamonds set between two terracotta string lines, both faces.
				const float Band = 203.f;
				if (Band > Z0 + 10.f && Band < Z1 - 10.f)
				{
					for (const float Face : { -1.f, 1.f })
					{
						Box(P.B, FVector(X0, Face * 10.f - 1.f, 192.f), FVector(X1, Face * 10.f + 1.f, 195.f));
						Box(P.B, FVector(X0, Face * 10.f - 1.f, 211.f), FVector(X1, Face * 10.f + 1.f, 214.f));
						for (float X = X0 + 20.f; X < X1 - 6.f; X += 40.f)
						{
							Add(FSurf::Lit(Tier.Look.Glow, 0.55f), FVector(8.f, 2.f, 8.f), FVector(X, Face * 10.4f, Band), FRotator(45.f, 0.f, 0.f));
						}
					}
				}
				return;
			}
			if (Is(TEXT("timberframe")))
			{
				// Infill between the frame: heartwood boards set back from the frame's faces.
				Boards(FSurf::Wood(Tier.Look.B), X0, X1, Z0, Z1, 12.f, 22.f);
				return;
			}
			if (Is(TEXT("whitestone")))
			{
				Courses(FSurf::Stone(Tier.Look.A, 0.7f), X0, X1, Z0, Z1, Thick, 45.f, 60.f, 40.f, 0.8f);
				const float B0 = 250.f, B1 = 262.f;
				if (Z0 <= B0 && Z1 >= B1)
				{
					for (const float Face : { -1.f, 1.f })
					{
						for (float X = X0 + 1.f; X < X1 - 4.f; X += 12.8f)
						{
							Box(FSurf::Glaze(Tier.Look.B), FVector(X, Face * 10.f - 0.6f, B0), FVector(FMath::Min(X1, X + 12.f), Face * 10.f + 0.6f, B1), 0.9f + 0.2f * Rand(FVector(X, Face, B0), 61));
						}
					}
				}
				return;
			}
			if (Is(TEXT("geode")))
			{
				Courses(P.A, X0, X1, Z0, Z1, Thick, 37.5f, 45.f, 40.f, 1.6f);
				// Druses: violet crystal clusters grown in the dark stone here and there, both faces.
				for (const float Face : { -1.f, 1.f })
				{
					for (float X = X0 + 25.f; X < X1 - 20.f; X += 55.f)
					{
						for (float Z = Z0 + 25.f; Z < Z1 - 20.f; Z += 45.f)
						{
							const FVector At(X + Jit(FVector(X, Face, Z), 71) * 12.f, Face * 10.f, Z + Jit(FVector(X, Face, Z), 72) * 10.f);
							if (Rand(At, 73) > 0.14f) continue;
							Disc(P.Dark, At, FVector(0.f, Face, 0.f), 18.f, 2.f);
							for (int32 c = 0; c < 5; ++c)
							{
								const FVector Dir = FVector(Jit(At, 80 + c) * 0.55f, Face, Jit(At, 90 + c) * 0.55f).GetSafeNormal();
								Spike(FSurf::Lit(Tier.Look.C, 0.65f), At + FVector(Jit(At, 100 + c) * 4.f, 0.f, Jit(At, 110 + c) * 4.f), Dir, 3.f + Rand(At, 120 + c) * 2.5f, 6.f + Rand(At, 130 + c) * 8.f);
							}
						}
					}
				}
				return;
			}
			if (Is(TEXT("logs")))
			{
				// Rime lodge: a stone sill to 1 m, pine logs above (their ends cross past the corner: WallLogs).
				const float Sill = 100.f;
				if (Z0 < Sill) Courses(FSurf::Stone(Tier.Look.B), X0, X1, Z0, FMath::Min(Z1, Sill), Thick + 4.f, 33.3f, 45.f, 40.f, 1.8f);
				if (Z1 > Sill) Logs(X0, X1, FMath::Max(Z0, Sill), Z1);
				return;
			}
			if (Is(TEXT("reeds")))
			{
				// Woven reed panels: bundles in two staggered layers, bound with cord at three heights.
				Box(P.Dark, FVector(X0, -4.f, Z0), FVector(X1, 4.f, Z1));
				const FSurf Reed = FSurf::Stone(Tier.Look.B, 0.92f);
				for (const float Layer : { -4.5f, 4.5f })
				{
					for (float X = X0 + 3.5f + (Layer > 0.f ? 3.2f : 0.f); X < X1 - 2.f; X += 6.4f)
					{
						const FVector C(X, Layer, (Z0 + Z1) * 0.5f);
						Add(Reed, FVector(7.f, 7.f, H - 1.f), C + FVector(0.f, Jit(C, 141) * 0.8f, 0.f), FRotator(0.f, 0.f, Jit(C, 142) * 0.8f), EWRLookMesh::Cylinder, 0.85f + 0.3f * Rand(C, 143));
					}
				}
				for (const float Z : { Z0 + 30.f, (Z0 + Z1) * 0.5f, Z1 - 30.f })
				{
					if (Z < Z0 + 8.f || Z > Z1 - 8.f) continue;
					for (const float Face : { -1.f, 1.f }) Box(FSurf::Stone(Tier.Look.A * 1.4f, 0.9f), FVector(X0, Face * 8.5f - 1.f, Z - 1.2f), FVector(X1, Face * 8.5f + 1.f, Z + 1.2f));
				}
				return;
			}
			if (Is(TEXT("coral")))
			{
				Courses(FSurf::Stone(Tier.Look.A, 0.95f), X0, X1, Z0, Z1, Thick + 2.f, 30.f, 28.f, 34.f, 2.4f);
				// Coral grown on the rag: branching sprigs and brain-coral domes here and there.
				for (const float Face : { -1.f, 1.f })
				{
					for (float X = X0 + 22.f; X < X1 - 15.f; X += 48.f)
					{
						for (float Z = Z0 + 20.f; Z < Z1 - 15.f; Z += 40.f)
						{
							const FVector At(X + Jit(FVector(X, Face, Z), 151) * 14.f, Face * 11.f, Z + Jit(FVector(X, Face, Z), 152) * 12.f);
							const float Pick = Rand(At, 153);
							if (Pick < 0.09f)
							{
								for (int32 c = 0; c < 4; ++c)
								{
									const FVector Dir = FVector(Jit(At, 160 + c) * 0.6f, Face, 0.5f + Rand(At, 170 + c) * 0.6f).GetSafeNormal();
									Rod(FSurf::Stone(Tier.Look.C, 0.8f), At, At + Dir * (8.f + Rand(At, 180 + c) * 8.f), 3.f);
								}
							}
							else if (Pick < 0.16f)
							{
								Disc(FSurf::Stone(FMath::Lerp(Tier.Look.C, Tier.Look.B, 0.5f), 0.85f), At, FVector(0.f, Face, 0.f), 10.f + Rand(At, 190) * 8.f, 5.f);
							}
						}
					}
				}
				return;
			}
			if (Is(TEXT("chapel")))
			{
				Courses(P.A, X0, X1, Z0, Z1, Thick, 37.5f, 50.f, 40.f, 1.5f);
				// The string course at 2 m, proud of both faces.
				if (Z0 < 196.f && Z1 > 206.f)
				{
					for (const float Face : { -1.f, 1.f }) Box(P.B, FVector(X0, Face * 11.f - 2.f, 196.f), FVector(X1, Face * 11.f + 2.f, 204.f));
				}
				return;
			}
			if (Is(TEXT("felt")))
			{
				// Quilted mycelium felt: soft cream pads in rows between the stems.
				Box(P.Dark, FVector(X0, -5.f, Z0), FVector(X1, 5.f, Z1));
				const FSurf Felt = FSurf::Cloth(Tier.Look.A);
				for (float Z = Z0; Z < Z1 - 2.f; Z += 60.f)
				{
					const float Z2 = FMath::Min(Z1, Z + 60.f);
					for (float X = X0 + ((static_cast<int32>(Z / 60.f) & 1) ? -25.f : 0.f); X < X1 - 2.f; X += 50.f)
					{
						const float A = FMath::Max(X, X0), B = FMath::Min(X + 50.f, X1);
						if (B - A < 3.f) continue;
						const FVector C((A + B) * 0.5f, 0.f, (Z + Z2) * 0.5f);
						Add(Felt, FVector(B - A - 1.6f, Thick - 2.f + Rand(C, 201) * 3.f, Z2 - Z - 1.6f), C, FRotator::ZeroRotator, EWRLookMesh::Cube, 0.9f + 0.2f * Rand(C, 202));
					}
				}
				return;
			}
			if (Is(TEXT("voidstone")))
			{
				Courses(P.A, X0, X1, Z0, Z1, Thick, 37.5f, 55.f, 40.f, 1.6f);
				return;
			}
			// Any other: plain ashlar in its colour.
			Courses(P.A, X0, X1, Z0, Z1, Thick, 37.5f, 50.f, 40.f, 1.8f);
		}

		/** Horizontal pine logs from Z0 to Z1 between X0 and X1; logs that reach a corner run past it and cross the
		 *  neighbour's (saddle notches: the rows of walls along world X and world Y sit half a log apart). */
		void Logs(float X0, float X1, float Z0, float Z1)
		{
			const float D = 26.f, Pitch = 24.f;
			const FVector AlongWorld = World.TransformVectorNoScale(Frame.TransformVectorNoScale(FVector::XAxisVector)).GetAbs();
			const float Offset = AlongWorld.Y > AlongWorld.X ? Pitch * 0.5f : 0.f;
			const float LX0 = X0 <= -Corner + 14.f ? -Corner - 18.f : X0, LX1 = X1 >= Corner - 14.f ? Corner + 18.f : X1;
			Box(FSurf::Stone(Tier.Look.C * 0.55f, 0.95f), FVector(X0, -6.f, Z0), FVector(X1, 6.f, Z1));   // chinking (moss and clay)
			for (float Z = 100.f + Offset + Pitch * 0.5f; Z <= Z1 - Pitch * 0.5f + 2.f; Z += Pitch)
			{
				if (Z < Z0 + Pitch * 0.5f - 2.f) continue;
				const FVector C((LX0 + LX1) * 0.5f, 0.f, Z);
				Add(FSurf::Wood(Tier.Look.A), FVector(D + Jit(C, 211) * 1.5f, D, LX1 - LX0), C, FRotator(90.f, 0.f, 0.f), EWRLookMesh::Cylinder, 0.9f + 0.2f * Rand(C, 212));
			}
		}

		/** What stands on a cell corner of a wall (posts, quoins, buttresses), Z0..Z1. */
		void CornerAt(float X, float Z0, float Z1)
		{
			if (Is(TEXT("panels")))
			{
				// An H-section steel post: two flanges and a web, on a base plate.
				const FSurf S = FSurf::Steel(Tier.Look.A * 0.7f, 0.38f);
				Box(S, FVector(X - 11.f, -11.f, Z0), FVector(X + 11.f, -8.f, Z1));
				Box(S, FVector(X - 11.f, 8.f, Z0), FVector(X + 11.f, 11.f, Z1));
				Box(S, FVector(X - 1.5f, -8.f, Z0), FVector(X + 1.5f, 8.f, Z1));
				if (Z0 <= 0.5f) Box(S, FVector(X - 15.f, -15.f, 0.f), FVector(X + 15.f, 15.f, 2.f));
				return;
			}
			if (Is(TEXT("columns")))
			{
				Hex(P.A, X, 0.f, Z0, Z1, 17.f, 30.f);
				return;
			}
			if (Is(TEXT("timberframe")))
			{
				Box(FSurf::Wood(Tier.Look.A), FVector(X - 12.f, -12.f, Z0), FVector(X + 12.f, 12.f, Z1));
				return;
			}
			if (Is(TEXT("reeds")))
			{
				Add(FSurf::Wood(Tier.Look.A), FVector(20.f, 20.f, Z1 - Z0), FVector(X, 0.f, (Z0 + Z1) * 0.5f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
				for (const float Z : { 60.f, 240.f })
				{
					if (Z > Z1 - 10.f) continue;
					Add(FSurf::Glaze(Tier.Look.C, 0.3f), FVector(22.f, 22.f, 6.f), FVector(X, 0.f, Z), FRotator::ZeroRotator, EWRLookMesh::Cylinder);   // red lacquer band
				}
				// Two lacquered bone pegs crossed through the post.
				const float Zb = FMath::Min(Z1 - 30.f, 180.f);
				Rod(FSurf::Stone(Tier.Look.Trim, 0.5f), FVector(X - 16.f, -16.f, Zb - 10.f), FVector(X + 16.f, 16.f, Zb + 10.f), 3.f);
				Rod(FSurf::Stone(Tier.Look.Trim, 0.5f), FVector(X - 16.f, 16.f, Zb - 10.f), FVector(X + 16.f, -16.f, Zb + 10.f), 3.f);
				return;
			}
			if (Is(TEXT("felt")))
			{
				Add(FSurf::Wood(Tier.Look.B), FVector(22.f, 22.f, Z1 - Z0), FVector(X, 0.f, (Z0 + Z1) * 0.5f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
				if (Z0 <= 0.5f) for (int32 r = 0; r < 4; ++r)
				{
					const FVector Dir(FMath::Cos(r * HALF_PI + 0.4f), FMath::Sin(r * HALF_PI + 0.4f), 0.f);
					Rod(FSurf::Wood(Tier.Look.B * 0.85f), FVector(X, 0.f, 30.f), FVector(X, 0.f, 0.f) + Dir * 26.f, 8.f);   // root flare
				}
				return;
			}
			if (Is(TEXT("logs")))
			{
				// The sill's quoins; the logs above cross past the corner by themselves.
				for (float Z = Z0; Z < FMath::Min(Z1, 100.f) - 1.f; Z += 33.3f) Box(FSurf::Stone(Tier.Look.B), FVector(X - 14.f, -14.f, Z), FVector(X + 14.f, 14.f, FMath::Min(100.f, Z + 31.5f)));
				return;
			}
			if (Is(TEXT("chapel")))
			{
				// A buttress: a wide plinth with a sloped weathering, then quoins.
				if (Z0 <= 0.5f)
				{
					Box(P.A, FVector(X - 20.f, -20.f, 0.f), FVector(X + 20.f, 20.f, 80.f));
					Add(P.A, FVector(40.f, 40.f, 10.f), FVector(X, 0.f, 84.f), FRotator(0.f, 0.f, 0.f));
				}
				for (float Z = FMath::Max(Z0, Z0 <= 0.5f ? 89.f : Z0); Z < Z1 - 1.f; Z += 37.5f)
				{
					const float W = static_cast<int32>(Z / 37.5f) % 2 ? 13.f : 16.f;
					Box(P.A, FVector(X - W, -W, Z), FVector(X + W, W, FMath::Min(Z1, Z + 35.5f)));
				}
				return;
			}
			// Stone styles: quoins, one per course, a little proud and shared with the neighbour.
			const float Course = Is(TEXT("sandstone")) ? 50.f : Is(TEXT("whitestone")) ? 45.f : Is(TEXT("coral")) ? 30.f : 37.5f;
			const FSurf& S = Is(TEXT("coral")) ? P.B : P.A;
			for (float Z = Z0; Z < Z1 - 1.f; Z += Course)
			{
				Box(S, FVector(X - 13.f, -13.f, Z), FVector(X + 13.f, 13.f, FMath::Min(Z1, Z + Course) - 1.5f));
			}
		}

		/** The top member of a wall of height H across X0..X1. */
		void Cap(float X0, float X1, float H)
		{
			if (Is(TEXT("panels"))) { Box(FSurf::Steel(Tier.Look.A * 0.75f), FVector(X0, -11.f, H - 8.f), FVector(X1, 11.f, H)); return; }
			if (Is(TEXT("columns"))) { Box(FSurf::Glaze(Tier.Look.B, 0.12f), FVector(X0, -12.f, H - 16.f), FVector(X1, 12.f, H)); return; }   // obsidian lintel
			if (Is(TEXT("granite")))
			{
				// Copper flashing weathered to verdigris, with a drip lip down each face.
				Box(FSurf::Stone(Tier.Look.C, 0.6f), FVector(X0 - 2.f, -13.f, H - 3.f), FVector(X1 + 2.f, 13.f, H));
				for (const float Face : { -1.f, 1.f }) Box(FSurf::Stone(Tier.Look.C, 0.6f), FVector(X0 - 2.f, Face * 12.f - 1.f, H - 8.f), FVector(X1 + 2.f, Face * 12.f + 1.f, H - 3.f));
				return;
			}
			if (Is(TEXT("timberframe"))) { Box(FSurf::Wood(Tier.Look.A), FVector(X0, -12.f, H - 16.f), FVector(X1, 12.f, H)); return; }
			if (Is(TEXT("whitestone")))
			{
				Box(FSurf::Stone(Tier.Look.A, 0.65f), FVector(X0, -14.f, H - 8.f), FVector(X1, 14.f, H));
				Box(FSurf::Glaze(Tier.Look.B), FVector(X0, -12.5f, H - 11.f), FVector(X1, 12.5f, H - 8.f));
				return;
			}
			if (Is(TEXT("logs")) || Is(TEXT("reeds")) || Is(TEXT("felt")))
			{
				const FSurf S = Is(TEXT("felt")) ? FSurf::Wood(Tier.Look.B) : FSurf::Wood(Tier.Look.A);
				if (!Is(TEXT("logs"))) Add(S, FVector(16.f, 16.f, X1 - X0), FVector((X0 + X1) * 0.5f, 0.f, H - 8.f), FRotator(90.f, 0.f, 0.f), EWRLookMesh::Cylinder);
				return;
			}
			if (Is(TEXT("voidstone")))
			{
				Box(P.A, FVector(X0, -12.f, H - 8.f), FVector(X1, 12.f, H));
				for (const float Face : { -1.f, 1.f }) Box(FSurf::Lit(Tier.Look.Glow, 0.8f), FVector(X0, Face * 12.f - 0.6f, H - 10.f), FVector(X1, Face * 12.f + 0.6f, H - 8.6f));
				return;
			}
			// Coping: a dressed slab a little proud of both faces.
			Box(Is(TEXT("coral")) ? P.B : P.A, FVector(X0, -12.5f, H - 7.f), FVector(X1, 12.5f, H), 1.08f);
		}

		/** A framed style's frame across a wall bay (posts, rails, braces): timber, mangrove or stems. */
		void TimberFrame(float H, bool bMiddlePost)
		{
			const FSurf S = Is(TEXT("felt")) ? FSurf::Wood(Tier.Look.B) : FSurf::Wood(Tier.Look.A);
			if (Is(TEXT("timberframe")))
			{
				Box(S, FVector(-Corner + 12.f, -12.f, 0.f), FVector(Corner - 12.f, 12.f, 16.f));   // sill beam
				if (H > 200.f) Box(S, FVector(-Corner + 12.f, -11.f, 142.f), FVector(Corner - 12.f, 11.f, 156.f));   // mid rail
				if (bMiddlePost) Box(S, FVector(-11.f, -11.f, 16.f), FVector(11.f, 11.f, H - 16.f));
				// Braces in the lower bays, from the sill at the corner posts up to the rail.
				const float Top = H > 200.f ? 142.f : H - 16.f;
				const float Run = bMiddlePost ? 70.f : 110.f;
				for (const float Side : { -1.f, 1.f })
				{
					const FVector A(Side * (Corner - 12.f), 0.f, 16.f + 70.f), B(Side * (Corner - 12.f - Run), 0.f, Top);
					Rod(S, A, B, 12.f, true);
				}
				// Amber in the carved notches where rails meet posts: a faint glow.
				for (const float Face : { -1.f, 1.f })
				{
					for (const float X : { -Corner + 14.f, Corner - 14.f })
					{
						for (const float Z : { 16.f, H - 16.f }) Add(FSurf::Lit(Tier.Look.Glow, 0.35f), FVector(4.f, 1.f, 4.f), FVector(X, Face * 12.3f, Z), FRotator(45.f, 0.f, 0.f));
					}
				}
				return;
			}
			if (Is(TEXT("reeds")))
			{
				Add(S, FVector(14.f, 14.f, 400.f), FVector(0.f, 0.f, 8.f), FRotator(90.f, 0.f, 0.f), EWRLookMesh::Cylinder);
				if (bMiddlePost) Add(S, FVector(16.f, 16.f, H), FVector(0.f, 0.f, H * 0.5f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
				return;
			}
			if (Is(TEXT("felt")))
			{
				if (bMiddlePost) Add(S, FVector(18.f, 18.f, H), FVector(0.f, 0.f, H * 0.5f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
				// A root runs along the foot of the wall.
				Add(S, FVector(14.f, 14.f, 400.f), FVector(0.f, 0.f, 7.f), FRotator(90.f, 0.f, 0.f), EWRLookMesh::Cylinder);
			}
		}

		// ---------------------------------------------------------------- whole pieces

		void Wall(float H)
		{
			CornerAt(-Corner, 0.f, H);
			CornerAt(Corner, 0.f, H);
			const bool bTop = !Is(TEXT("logs"));
			Fill(-Corner + 13.f, Corner - 13.f, 0.f, bTop ? H - 7.f : H);
			if (Framed()) TimberFrame(H, H > 200.f);
			if (Is(TEXT("voidstone")) && H > 200.f)
			{
				// A voidglass slab set in the stone, its edges lit violet (the air bends there).
				for (const float Face : { -1.f, 1.f })
				{
					Box(FSurf::Glaze(Tier.Look.B, 0.1f), FVector(-135.f, Face * 10.f - 2.f, 55.f), FVector(135.f, Face * 10.f + 2.f, H - 50.f));
					const FSurf Rim = FSurf::Lit(Tier.Look.Glow, 0.9f);
					Box(Rim, FVector(-137.f, Face * 12.2f - 0.5f, 53.f), FVector(137.f, Face * 12.2f + 0.5f, 55.f));
					Box(Rim, FVector(-137.f, Face * 12.2f - 0.5f, H - 50.f), FVector(137.f, Face * 12.2f + 0.5f, H - 48.f));
					Box(Rim, FVector(-137.f, Face * 12.2f - 0.5f, 55.f), FVector(-135.f, Face * 12.2f + 0.5f, H - 50.f));
					Box(Rim, FVector(135.f, Face * 12.2f - 0.5f, 55.f), FVector(137.f, Face * 12.2f + 0.5f, H - 50.f));
				}
			}
			if (Is(TEXT("panels")) && H > 200.f)
			{
				for (const float Face : { -1.f, 1.f }) Box(FSurf::Steel(Tier.Look.A * 0.72f), FVector(-Corner + 11.f, Face * 10.f - 2.f, 145.f), FVector(Corner - 11.f, Face * 10.f + 2.f, 151.f));   // mid rail
			}
			if (bTop) Cap(-Corner, Corner, H);
		}

		/** An arch's voussoirs and the spandrels that fill the opening's top corners round it. Opening |X| < HalfW up to
		 *  Top; it springs at Spring. Round (or pointed: two arcs of radius R meeting at the top). */
		void Arch(float HalfW, float Spring, float Top, float Thick)
		{
			const FSurf& Ring1 = Is(TEXT("coral")) || Is(TEXT("whitestone")) ? P.B : P.A;
			if (Pointed())
			{
				const float Rise = Top - Spring;
				// Two arcs through the springs meeting at Top: centre offset c with R = HalfW + c, R^2 = c^2 + Rise^2.
				const float C = (Rise * Rise - HalfW * HalfW) / (2.f * HalfW);
				const float R = HalfW + C;
				const float Apex = FMath::RadiansToDegrees(FMath::Atan2(Rise, -C));   // where the left arc (centred at +C) reaches X = 0
				Ring(Ring1, FVector(C, 0.f, Spring), R, 16.f, Thick + 2.f, 6, Apex, 180.f);
				Ring(Ring1, FVector(-C, 0.f, Spring), R, 16.f, Thick + 2.f, 6, 0.f, 180.f - Apex);
				for (float Z = Spring; Z < Top + 16.f; Z += 8.f)
				{
					const float Dz = Z + 4.f - Spring;
					const float Inner = R * R - Dz * Dz > 0.f ? FMath::Sqrt(R * R - Dz * Dz) - C : 0.f;
					const float From = FMath::Max(0.f, Inner + 14.f);
					if (From < HalfW + 6.f)
					{
						Box(P.A, FVector(From, -Thick * 0.5f, Z), FVector(HalfW + 6.f, Thick * 0.5f, Z + 8.f));
						Box(P.A, FVector(-HalfW - 6.f, -Thick * 0.5f, Z), FVector(-From, Thick * 0.5f, Z + 8.f));
					}
				}
				return;
			}
			const float R = FMath::Min(HalfW, Top - Spring);
			Ring(Ring1, FVector(0.f, 0.f, Spring), R, 15.f, Thick + 2.f, 9, 0.f, 180.f);
			// A levistone keystone that floats a hand's width over the arch (Cloud spire).
			if (Is(TEXT("whitestone")))
			{
				Add(FSurf::Lit(Tier.Look.Glow, 0.35f), FVector(16.f, Thick + 4.f, 20.f), FVector(0.f, 0.f, Spring + R + 15.f + 16.f));
			}
			for (float Z = Spring; Z < Top + 15.f; Z += 8.f)
			{
				const float Dz = Z + 4.f - Spring;
				const float Inner = R * R - Dz * Dz > 0.f ? FMath::Sqrt(R * R - Dz * Dz) : 0.f;
				const float From = Inner + 14.f;
				if (From < HalfW + 6.f)
				{
					Box(P.A, FVector(From, -Thick * 0.5f, Z), FVector(HalfW + 6.f, Thick * 0.5f, Z + 8.f));
					Box(P.A, FVector(-HalfW - 6.f, -Thick * 0.5f, Z), FVector(-From, Thick * 0.5f, Z + 8.f));
				}
			}
		}

		/** Glazing in the opening |X| < HalfW from Z0 up (under an arch it narrows with it). */
		void Glazing(float HalfW, float Z0, float Spring, float Top)
		{
			const FSurf Glass = P.Glass;
			const FSurf Bar = Is(TEXT("panels")) ? FSurf::Steel(Tier.Look.A * 0.6f) : Is(TEXT("chapel")) ? FSurf::Stone(FLinearColor(0.03f, 0.03f, 0.035f), 0.4f) : P.B;
			if (Is(TEXT("geode")))
			{
				// Faceted crystal: diamond panes in a dark lattice.
				Box(P.Dark, FVector(-HalfW, -1.f, Z0), FVector(HalfW, 1.f, Top));
				for (float X = -HalfW + 16.f; X < HalfW - 10.f; X += 32.f)
				{
					for (float Z = Z0 + 16.f; Z < Top - 10.f; Z += 32.f)
					{
						Add(FSurf::Glass(FMath::Lerp(Tier.Look.Glass, Tier.Look.C, Rand(FVector(X, 0.f, Z), 221)), Tier.Look.GlassGlow), FVector(21.f, 3.f, 21.f), FVector(X, 0.f, Z), FRotator(45.f, 0.f, 0.f));
					}
				}
				return;
			}
			const bool bArch = Arched();
			for (float Z = Z0; Z < Top - 0.5f; Z += 10.f)
			{
				float W = HalfW;
				if (bArch && Z + 5.f > Spring)
				{
					const float Dz = Z + 5.f - Spring;
					if (Pointed())
					{
						const float Rise = Top - Spring, C = (Rise * Rise - HalfW * HalfW) / (2.f * HalfW), R = HalfW + C;
						W = R * R - Dz * Dz > 0.f ? FMath::Sqrt(R * R - Dz * Dz) - C : 0.f;
					}
					else
					{
						const float R = FMath::Min(HalfW, Top - Spring);
						W = R * R - Dz * Dz > 0.f ? FMath::Sqrt(R * R - Dz * Dz) : 0.f;
					}
				}
				if (W < 2.f) break;
				const float Z1 = FMath::Min(Top, Z + 10.f);
				if (Is(TEXT("chapel")))
				{
					// Stained glass in bruise colours: three panes a row, lead cames between.
					static const FLinearColor Bruise[3] = { FLinearColor(0.28f, 0.06f, 0.36f), FLinearColor(0.08f, 0.22f, 0.12f), FLinearColor(0.42f, 0.24f, 0.08f) };
					for (int32 i = 0; i < 3; ++i)
					{
						const float A = -W + 2.f * W * i / 3.f, B = -W + 2.f * W * (i + 1) / 3.f;
						const int32 Pick = (i + static_cast<int32>(Z / 10.f) + static_cast<int32>(Rand(FVector(i, 0.f, Z), 231) * 3.f)) % 3;
						Box(FSurf::Glass(Bruise[Pick], Tier.Look.GlassGlow), FVector(A + 0.6f, -1.f, Z + 0.6f), FVector(B - 0.6f, 1.f, Z1 - 0.6f));
					}
					Box(Bar, FVector(-W, -1.4f, Z1 - 0.6f), FVector(W, 1.4f, Z1 + 0.6f));
					continue;
				}
				Box(Glass, FVector(-W, -1.f, Z), FVector(W, 1.f, Z1));
			}
			if (!Is(TEXT("chapel")) && !bArch)
			{
				// Glazing bars: a mullion and a transom.
				Box(Bar, FVector(-2.f, -2.5f, Z0), FVector(2.f, 2.5f, Top));
				Box(Bar, FVector(-HalfW, -2.5f, (Z0 + Top) * 0.5f - 2.f), FVector(HalfW, 2.5f, (Z0 + Top) * 0.5f + 2.f));
			}
		}

		void Window(float H)
		{
			CornerAt(-Corner, 0.f, H);
			CornerAt(Corner, 0.f, H);
			const float Top = Is(TEXT("logs")) ? H : H - 7.f;
			if (Is(TEXT("coral")))
			{
				// A porthole: a ring of dressed coralstone round a sea-glass disc.
				Fill(-Corner + 13.f, Corner - 13.f, 0.f, 100.f);
				Fill(-Corner + 13.f, Corner - 13.f, 220.f, Top);
				Fill(-Corner + 13.f, -80.f, 100.f, 220.f);
				Fill(80.f, Corner - 13.f, 100.f, 220.f);
				for (float Z = 100.f; Z < 220.f; Z += 8.f)
				{
					const float Dz = Z + 4.f - 160.f;
					const float Inner = 3600.f - Dz * Dz > 0.f ? FMath::Sqrt(3600.f - Dz * Dz) + 12.f : 0.f;
					if (Inner < 80.f)
					{
						Box(P.A, FVector(Inner, -10.f, Z), FVector(80.f, 10.f, Z + 8.f));
						Box(P.A, FVector(-80.f, -10.f, Z), FVector(-Inner, 10.f, Z + 8.f));
					}
				}
				Ring(P.B, FVector(0.f, 0.f, 160.f), 56.f, 13.f, 22.f, 14);
				Disc(P.Glass, FVector(0.f, 0.f, 160.f), FVector(0.f, 1.f, 0.f), 114.f, 3.f);
				Cap(-Corner, Corner, H);
				return;
			}
			Fill(-Corner + 13.f, Corner - 13.f, 0.f, 92.f);
			Fill(-Corner + 13.f, Corner - 13.f, 244.f, Top);
			Fill(-Corner + 13.f, -86.f, 92.f, 244.f);
			Fill(86.f, Corner - 13.f, 92.f, 244.f);
			if (Framed()) TimberFrame(H, false);
			const FSurf& Trim = Is(TEXT("panels")) ? P.A : (Is(TEXT("timberframe")) || Is(TEXT("logs")) || Is(TEXT("reeds"))) ? (Is(TEXT("logs")) ? FSurf::Wood(Tier.Look.A) : FSurf::Wood(Is(TEXT("reeds")) ? Tier.Look.C : Tier.Look.A)) : P.B;
			Box(Trim, FVector(-98.f, -16.f, 90.f), FVector(98.f, 16.f, 100.f));   // sill, proud of the wall
			if (Arched())
			{
				Box(P.A, FVector(-86.f, -10.f, 100.f), FVector(-80.f, 10.f, 140.f));
				Box(P.A, FVector(80.f, -10.f, 100.f), FVector(86.f, 10.f, 140.f));
				Arch(80.f, Pointed() ? 120.f : 140.f, 220.f, 20.f);
				Fill(-86.f, 86.f, 236.f, 244.f);
				Glazing(80.f, 100.f, Pointed() ? 120.f : 140.f, 220.f);
			}
			else
			{
				Box(Trim, FVector(-92.f, -12.f, 100.f), FVector(-80.f, 12.f, 220.f));   // jambs
				Box(Trim, FVector(80.f, -12.f, 100.f), FVector(92.f, 12.f, 220.f));
				Box(Trim, FVector(-96.f, -13.f, 220.f), FVector(96.f, 13.f, 244.f));    // head / lintel
				Glazing(80.f, 100.f, 220.f, 220.f);
				if (Is(TEXT("voidstone")))
				{
					const FSurf Rim = FSurf::Lit(Tier.Look.Glow, 0.9f);
					for (const float Face : { -1.f, 1.f })
					{
						Box(Rim, FVector(-80.f, Face * 12.5f - 0.5f, 100.f), FVector(-78.5f, Face * 12.5f + 0.5f, 220.f));
						Box(Rim, FVector(78.5f, Face * 12.5f - 0.5f, 100.f), FVector(80.f, Face * 12.5f + 0.5f, 220.f));
					}
				}
				if (Is(TEXT("granite")))
				{
					// Storm shutters hung open on the outer face: boarded leaves on iron straps.
					for (const float Side : { -1.f, 1.f })
					{
						const float X0 = Side < 0.f ? -176.f : 94.f, X1 = Side < 0.f ? -94.f : 176.f;
						for (float X = X0; X < X1 - 1.f; X += 16.4f) Box(FSurf::Wood(Tier.Look.Trim), FVector(X + 0.6f, 11.f, 104.f), FVector(FMath::Min(X1, X + 16.4f) - 0.6f, 15.f, 218.f));
						for (const float Z : { 120.f, 200.f }) Box(FSurf::Stone(FLinearColor(0.02f, 0.02f, 0.022f), 0.5f), FVector(X0, 15.f, Z - 2.f), FVector(X1, 16.2f, Z + 2.f));
					}
				}
			}
			Cap(-Corner, Corner, H);
		}

		void Doorway(float H)
		{
			CornerAt(-Corner, 0.f, H);
			CornerAt(Corner, 0.f, H);
			const float Top = Is(TEXT("logs")) ? H : H - 7.f;
			Fill(-Corner + 13.f, -86.f, 0.f, Top);
			Fill(86.f, Corner - 13.f, 0.f, Top);
			if (Framed()) TimberFrame(H, false);
			if (Arched())
			{
				const float Spring = Pointed() ? 150.f : 170.f;
				for (float Z = 0.f; Z < Spring - 1.f; Z += 40.f)
				{
					const float W = static_cast<int32>(Z / 40.f) % 2 ? 12.f : 17.f;
					Box(P.A, FVector(-70.f - W, -12.f, Z), FVector(-70.f, 12.f, FMath::Min(Spring, Z + 38.f)));
					Box(P.A, FVector(70.f, -12.f, Z), FVector(70.f + W, 12.f, FMath::Min(Spring, Z + 38.f)));
				}
				Arch(70.f, Spring, 240.f, 20.f);
				Fill(-86.f, 86.f, 256.f, Top);
			}
			else
			{
				const FSurf Trim = Is(TEXT("panels")) ? P.A : Is(TEXT("timberframe")) || Is(TEXT("logs")) ? FSurf::Wood(Tier.Look.A) : Is(TEXT("reeds")) ? FSurf::Glaze(Tier.Look.C, 0.3f) : P.B;
				Box(Trim, FVector(-84.f, -13.f, 0.f), FVector(-70.f, 13.f, 254.f));   // jambs
				Box(Trim, FVector(70.f, -13.f, 0.f), FVector(84.f, 13.f, 254.f));
				Box(Is(TEXT("columns")) ? FSurf::Glaze(Tier.Look.B, 0.12f) : Trim, FVector(-96.f, -13.f, 240.f), FVector(96.f, 13.f, 270.f));   // lintel
				Fill(-86.f, 86.f, 270.f, Top);
			}
			Cap(-Corner, Corner, H);
		}

		void Gable()
		{
			// Rows cut where the slope crosses their top (nothing pokes above the roof line), a coping or barge board
			// along the slope under the roof's own deck, the high end's corner member.
			const float H = 210.f;
			const auto Edge = [](float Z) { return 2.f * Z - 210.f; };
			const int32 N = 8;
			const float Pitch = H / N;
			for (int32 k = 0; k < N; ++k)
			{
				const float Z0 = k * Pitch, Z1 = Z0 + Pitch;
				const float X0 = FMath::Max(-Corner, Edge(Z1));
				Fill(X0, Corner - 13.f, Z0, Z1);
			}
			CornerAt(Corner, 0.f, H);
			const float Slope = FMath::RadiansToDegrees(FMath::Atan2(210.f, 420.f));
			const FVector Normal = FVector(-210.f, 0.f, 420.f).GetSafeNormal();
			const float Cover = Pitch * FMath::Cos(FMath::DegreesToRadians(Slope)) + 4.f;
			const FSurf Coping = Is(TEXT("granite")) ? FSurf::Stone(Tier.Look.C, 0.6f) : TimberStairs() ? FSurf::Wood(Tier.Look.A) : Is(TEXT("panels")) ? FSurf::Steel(Tier.Look.A * 0.75f) : P.B;
			Add(Coping, FVector(472.f, 26.f, Cover), FVector(0.f, 0.f, 105.f) - Normal * (Cover * 0.5f - 2.f), FRotator(Slope, 0.f, 0.f));
			if (Is(TEXT("logs")))
			{
				// A carved prow at the peak: two crossed boards rising past the ridge, curling at the tips.
				for (const float Side : { -1.f, 1.f })
				{
					const FVector Base(Corner - 30.f, Side * 2.f, H - 20.f);
					const FVector Tip = Base + FVector(50.f, 0.f, 75.f) + FVector(0.f, 0.f, 0.f);
					Rod(FSurf::Wood(Tier.Look.A * 0.85f), Base, Tip, 9.f, true);
					Spike(FSurf::Wood(Tier.Look.A * 0.8f), Tip, FVector(1.f, 0.f, -0.6f), 10.f, 22.f);   // the beast's snout
				}
			}
			if (Is(TEXT("geode")))
			{
				for (int32 c = 0; c < 4; ++c) Spike(FSurf::Lit(Tier.Look.C, 0.7f), FVector(Corner - 6.f, 0.f, H - 2.f), FVector(Jit(FVector(c, 0.f, 0.f), 241) * 0.4f, Jit(FVector(c, 1.f, 0.f), 242) * 0.4f, 1.f), 5.f, 20.f + 8.f * c);
			}
		}

		void Floor()
		{
			Surface(20.f, 12.f);
			// Joists under it (the ceiling of the room below).
			const FSurf J = Is(TEXT("panels")) ? FSurf::Steel(Tier.Look.A * 0.65f) : TimberStairs() ? FSurf::Wood(Tier.Look.A * 0.8f) : P.B;
			for (const float X : { -100.f, 0.f, 100.f }) Box(J, FVector(X - 6.f, -199.f, -16.f), FVector(X + 6.f, 199.f, 0.f), 0.85f);
		}

		/** A walking surface over the cell with its top at ZTop on a Base-thick slab. */
		void Surface(float ZTop, float Base)
		{
			Box(P.Dark, FVector(-199.f, -199.f, ZTop - 20.f), FVector(199.f, 199.f, ZTop - 20.f + Base));
			if (Is(TEXT("panels")))
			{
				// Riveted deck plates, two by four, on the dark subframe.
				const FSurf Rivet = FSurf::Steel(Tier.Look.A * 0.8f, 0.35f);
				for (int32 i = 0; i < 2; ++i) for (int32 j = 0; j < 4; ++j)
				{
					const float X0 = -Corner + i * 200.f + 0.8f, X1 = -Corner + (i + 1) * 200.f - 0.8f, Y0 = -Corner + j * 100.f + 0.8f, Y1 = -Corner + (j + 1) * 100.f - 0.8f;
					Box(P.A, FVector(X0, Y0, ZTop - 8.f), FVector(X1, Y1, ZTop), 0.9f + 0.2f * Rand(FVector(X0, Y0, ZTop), 251));
					Studs(Rivet, FVector(X0 + 6.f, Y0 + 6.f, ZTop + 0.5f), FVector(X1 - 6.f, Y0 + 6.f, ZTop + 0.5f), 32.f, 2.f);
					Studs(Rivet, FVector(X0 + 6.f, Y1 - 6.f, ZTop + 0.5f), FVector(X1 - 6.f, Y1 - 6.f, ZTop + 0.5f), 32.f, 2.f);
				}
				return;
			}
			if (Is(TEXT("columns")))
			{
				// Causeway pavers: the tops of hexagonal columns in a honeycomb, an obsidian kerb round the edge.
				const float R = 22.f, Dx = R * 1.5f, Dy = R * 1.7320508f;
				int32 Col = 0;
				for (float X = -Corner + R * 0.8f; X < Corner - R * 0.5f; X += Dx, ++Col)
				{
					for (float Y = -Corner + Dy * 0.5f + ((Col & 1) ? Dy * 0.5f : 0.f); Y < Corner - Dy * 0.35f; Y += Dy)
					{
						Hex(P.A, X, Y, ZTop - 8.f, ZTop - Rand(FVector(X, Y, 0.f), 261) * 1.6f, R - 1.6f);
					}
				}
				const FSurf Kerb = FSurf::Glaze(Tier.Look.B, 0.15f);
				Box(Kerb, FVector(-Corner, -Corner, ZTop - 8.f), FVector(Corner, -Corner + 9.f, ZTop));
				Box(Kerb, FVector(-Corner, Corner - 9.f, ZTop - 8.f), FVector(Corner, Corner, ZTop));
				Box(Kerb, FVector(-Corner, -Corner + 9.f, ZTop - 8.f), FVector(-Corner + 9.f, Corner - 9.f, ZTop));
				Box(Kerb, FVector(Corner - 9.f, -Corner + 9.f, ZTop - 8.f), FVector(Corner, Corner - 9.f, ZTop));
				return;
			}
			if (Is(TEXT("timberframe"))) { Floorboards(FSurf::Wood(Tier.Look.B), ZTop, 7.f, 30.f); return; }
			if (Is(TEXT("logs"))) { Floorboards(FSurf::Wood(Tier.Look.A * 1.1f), ZTop, 7.f, 24.f); return; }
			if (Is(TEXT("reeds")))
			{
				// Split mangrove poles side by side across the joists.
				for (float Y = -Corner + 6.f; Y < Corner - 5.f; Y += 12.f)
				{
					const FVector C(0.f, Y, ZTop - 6.f);
					Add(FSurf::Wood(Tier.Look.A * (1.1f + 0.3f * Rand(C, 271))), FVector(12.f, 11.5f, 400.f), C, FRotator(90.f, 0.f, 0.f), EWRLookMesh::Cylinder);
				}
				return;
			}
			if (Is(TEXT("felt")))
			{
				// Shelf fungus: broad brackets overlapping, banded in brown and cream.
				for (int32 i = 0; i < 9; ++i)
				{
					const FVector C(-130.f + (i % 3) * 130.f + Jit(FVector(i, 0.f, 0.f), 281) * 18.f, -130.f + (i / 3) * 130.f + Jit(FVector(i, 1.f, 0.f), 282) * 18.f, ZTop - 4.f - (i % 2) * 1.5f);
					const float D = 150.f + Rand(C, 283) * 30.f;
					Disc(FSurf::Stone(Tier.Look.C, 0.9f), C, FVector::UpVector, D, 7.f);
					Disc(FSurf::Stone(Tier.Look.A * 0.9f, 0.9f), C + FVector(0.f, 0.f, 3.8f), FVector::UpVector, D * 0.7f, 0.6f);
					Disc(FSurf::Stone(Tier.Look.B, 0.9f), C + FVector(0.f, 0.f, 4.f), FVector::UpVector, D * 0.42f, 0.6f);
				}
				return;
			}
			if (Is(TEXT("coral")))
			{
				// Shell tiles: rounded scallops in rows, white and pale coral.
				for (float Y = -Corner + 12.f; Y < Corner - 6.f; Y += 22.f)
				{
					for (float X = -Corner + 12.f + (static_cast<int32>((Y + 400.f) / 22.f) & 1) * 11.f; X < Corner - 6.f; X += 22.f)
					{
						const FVector C(X, Y, ZTop - 2.f);
						Disc(FSurf::Glaze(FMath::Lerp(Tier.Look.B, Tier.Look.C, Rand(C, 291) * 0.35f), 0.45f), C, FVector::UpVector, 23.f, 4.f);
					}
				}
				return;
			}
			const FSurf& S = Is(TEXT("granite")) ? P.B : P.A;
			Flags(S, -Corner, Corner, -Corner, Corner, ZTop, 8.f, Is(TEXT("sandstone")) ? 60.f : 45.f, 40.f, Is(TEXT("whitestone")) ? 1.f : 2.2f);
			if (Is(TEXT("whitestone")))
			{
				const FSurf Blue = FSurf::Glaze(Tier.Look.B);
				Box(Blue, FVector(-Corner, -Corner, ZTop - 1.f), FVector(Corner, -Corner + 10.f, ZTop + 0.4f));
				Box(Blue, FVector(-Corner, Corner - 10.f, ZTop - 1.f), FVector(Corner, Corner, ZTop + 0.4f));
			}
			if (Is(TEXT("geode")))
			{
				for (int32 v = 0; v < 3; ++v)
				{
					const FVector A(-180.f + Rand(FVector(v, 0.f, 0.f), 301) * 360.f, -180.f, ZTop + 0.1f), B(-180.f + Rand(FVector(v, 1.f, 0.f), 302) * 360.f, 180.f, ZTop + 0.1f);
					Rod(FSurf::Lit(Tier.Look.C, 0.5f), A, B, 1.4f, true);   // crystal veins
				}
			}
			if (Is(TEXT("voidstone")))
			{
				Box(FSurf::Glaze(Tier.Look.B, 0.1f), FVector(-60.f, -60.f, ZTop - 1.f), FVector(60.f, 60.f, ZTop + 0.3f));
				for (const float S2 : { -1.f, 1.f })
				{
					Box(FSurf::Lit(Tier.Look.Glow, 0.8f), FVector(-61.f, S2 * 61.f - 0.6f, ZTop - 1.f), FVector(61.f, S2 * 61.f + 0.6f, ZTop + 0.35f));
					Box(FSurf::Lit(Tier.Look.Glow, 0.8f), FVector(S2 * 61.f - 0.6f, -61.f, ZTop - 1.f), FVector(S2 * 61.f + 0.6f, 61.f, ZTop + 0.35f));
				}
			}
		}

		void Foundation()
		{
			if (Is(TEXT("reeds")))
			{
				// Mangrove stilts at the corners with cross bracing, a joist frame, split poles on top.
				Box(P.Dark, FVector(-190.f, -190.f, -150.f), FVector(190.f, 190.f, 60.f));
				for (const float X : { -Corner + 14.f, Corner - 14.f }) for (const float Y : { -Corner + 14.f, Corner - 14.f })
				{
					Add(FSurf::Wood(Tier.Look.A), FVector(28.f, 28.f, 250.f), FVector(X, Y, -40.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
					Add(FSurf::Glaze(Tier.Look.C, 0.3f), FVector(30.f, 30.f, 6.f), FVector(X, Y, 60.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
				}
				for (int32 Side = 0; Side < 4; ++Side)
				{
					Frame = FTransform(FRotator(0.f, 90.f * Side, 0.f));
					Rod(FSurf::Wood(Tier.Look.A * 0.9f), FVector(-Corner + 14.f, -Corner + 14.f, -60.f), FVector(Corner - 14.f, -Corner + 14.f, 70.f), 10.f);
					Box(FSurf::Wood(Tier.Look.A), FVector(-Corner, -Corner, 72.f), FVector(Corner, -Corner + 14.f, 88.f));
				}
				Frame = FTransform::Identity;
				Surface(100.f, 12.f);
				return;
			}
			Box(P.Dark, FVector(-198.f, -198.f, -150.f), FVector(198.f, 198.f, 92.f));
			for (int32 Side = 0; Side < 4; ++Side)
			{
				Frame = FTransform(FRotator(0.f, 90.f * Side, 0.f), FRotator(0.f, 90.f * Side, 0.f).RotateVector(FVector(0.f, -191.f, 0.f)));
				if (Is(TEXT("logs")) || Is(TEXT("timberframe")) || Is(TEXT("felt")) || Is(TEXT("panels")))
				{
					// Timber, felt and metal stand on a stone plinth: coursed stone faces.
					Courses(FSurf::Stone(Is(TEXT("panels")) || Is(TEXT("logs")) ? Tier.Look.B : Tier.Look.C * 0.6f), -Corner + 14.f, Corner - 14.f, -150.f, 92.f, 14.f, 40.f, 50.f, 40.f, 2.f, -150.f);
					if (Is(TEXT("panels"))) Box(FSurf::Steel(Tier.Look.A * 0.7f), FVector(-Corner, -9.f, 84.f), FVector(Corner, 9.f, 100.f));   // the steel kerb angle
				}
				else
				{
					Fill(-Corner + 14.f, Corner - 14.f, -150.f, 92.f, 14.f);
				}
			}
			Frame = FTransform::Identity;
			for (const float X : { -Corner, Corner }) for (const float Y : { -Corner, Corner })
			{
				Frame = FTransform(FVector(0.f, Y, 0.f));
				if (Is(TEXT("columns"))) Hex(P.A, X, 0.f, -150.f, 100.f, 18.f, 30.f);
				else for (float Z = -150.f; Z < 90.f; Z += 40.f) Box(Is(TEXT("coral")) ? P.B : TimberStairs() || Is(TEXT("panels")) ? FSurf::Stone(Tier.Look.B) : P.A, FVector(X - 14.f, -14.f, Z), FVector(X + 14.f, 14.f, FMath::Min(92.f, Z + 38.f)));
			}
			Frame = FTransform::Identity;
			Surface(100.f, 12.f);
		}

		void Pillar()
		{
			const float H = 300.f;
			if (Is(TEXT("panels")))
			{
				Courses(FSurf::Stone(Tier.Look.B), -22.f, 22.f, 0.f, 40.f, 44.f, 20.f, 50.f, 0.f, 1.5f);
				const FSurf S = FSurf::Steel(Tier.Look.A * 0.75f);
				Box(S, FVector(-16.f, -16.f, 40.f), FVector(16.f, -12.f, H));
				Box(S, FVector(-16.f, 12.f, 40.f), FVector(16.f, 16.f, H));
				Box(S, FVector(-2.f, -12.f, 40.f), FVector(2.f, 12.f, H));
				Box(S, FVector(-20.f, -20.f, 40.f), FVector(20.f, 20.f, 43.f));
				for (const float X : { -15.f, 15.f }) for (const float Y : { -15.f, 15.f }) Box(S, FVector(X - 1.8f, Y - 1.8f, 43.f), FVector(X + 1.8f, Y + 1.8f, 46.f));
				Box(S, FVector(-20.f, -20.f, H - 3.f), FVector(20.f, 20.f, H));
				return;
			}
			if (Is(TEXT("columns"))) { for (float Z = 0.f; Z < H - 1.f; Z += 75.f) Hex(P.A, 0.f, 0.f, Z, FMath::Min(H, Z + 73.5f), 23.f, Jit(FVector(Z), 311) * 8.f); return; }
			if (Is(TEXT("timberframe")) || Is(TEXT("logs")) || Is(TEXT("reeds")) || Is(TEXT("felt")))
			{
				const FSurf W = Is(TEXT("felt")) ? FSurf::Wood(Tier.Look.B) : FSurf::Wood(Tier.Look.A);
				if (Is(TEXT("logs"))) Courses(FSurf::Stone(Tier.Look.B), -22.f, 22.f, 0.f, 40.f, 44.f, 20.f, 50.f, 0.f, 1.5f);
				const float Z0 = Is(TEXT("logs")) ? 40.f : 0.f;
				if (Is(TEXT("timberframe")))
				{
					Box(W, FVector(-16.f, -16.f, Z0), FVector(16.f, 16.f, H));
					Add(W, FVector(26.f, 26.f, H - 40.f), FVector(0.f, 0.f, H * 0.5f), FRotator(0.f, 45.f, 0.f), EWRLookMesh::Cube, 0.92f);   // chamfered corners
					for (const float Z : { 40.f, H - 40.f }) for (int32 f = 0; f < 4; ++f)
					{
						const FVector Dir = FRotator(0.f, 90.f * f, 0.f).RotateVector(FVector(16.3f, 0.f, 0.f));
						Add(FSurf::Lit(Tier.Look.Glow, 0.35f), FVector(1.f, 5.f, 5.f), Dir + FVector(0.f, 0.f, Z), FRotator(0.f, 90.f * f, 45.f));
					}
					return;
				}
				Add(W, FVector(Is(TEXT("reeds")) ? 24.f : 32.f, Is(TEXT("reeds")) ? 24.f : 32.f, H - Z0), FVector(0.f, 0.f, (H + Z0) * 0.5f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
				if (Is(TEXT("reeds"))) for (const float Z : { 60.f, 240.f }) Add(FSurf::Glaze(Tier.Look.C, 0.3f), FVector(26.f, 26.f, 6.f), FVector(0.f, 0.f, Z), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
				if (Is(TEXT("felt")))
				{
					for (int32 r = 0; r < 5; ++r)
					{
						const FVector Dir(FMath::Cos(r * 1.2566f), FMath::Sin(r * 1.2566f), 0.f);
						Rod(W, FVector(0.f, 0.f, 40.f), Dir * 30.f, 10.f);
					}
					Disc(FSurf::Stone(Tier.Look.C, 0.9f), FVector(0.f, 0.f, H - 6.f), FVector::UpVector, 56.f, 12.f);
					Disc(FSurf::Lit(Tier.Look.Glow, 0.6f), FVector(0.f, 0.f, H - 13.f), FVector::UpVector, 44.f, 2.f);   // the gills under its cap
				}
				return;
			}
			if (Is(TEXT("geode")))
			{
				Courses(P.A, -20.f, 20.f, 0.f, H, 40.f, 37.5f, 40.f, 0.f, 1.5f);
				for (int32 f = 0; f < 4; ++f)
				{
					Frame = FTransform(FRotator(0.f, 90.f * f, 0.f));
					Box(FSurf::Lit(Tier.Look.C, 0.75f), FVector(20.f, -3.f, 30.f), FVector(21.5f, 3.f, H - 30.f));   // the humming crystal core
				}
				Frame = FTransform::Identity;
				return;
			}
			if (Is(TEXT("voidstone")))
			{
				Box(P.A, FVector(-22.f, -22.f, 0.f), FVector(22.f, 22.f, 40.f));
				Box(P.A, FVector(-22.f, -22.f, H - 30.f), FVector(22.f, 22.f, H));
				Add(FSurf::Glaze(Tier.Look.B, 0.1f), FVector(24.f, 30.f, H - 70.f), FVector(0.f, 0.f, H * 0.5f + 5.f), FRotator(0.f, 30.f, 0.f));
				for (const float S2 : { -1.f, 1.f }) Add(FSurf::Lit(Tier.Look.Glow, 0.85f), FVector(1.2f, 1.2f, H - 70.f), FRotator(0.f, 30.f, 0.f).RotateVector(FVector(S2 * 12.f, 15.f, 0.f)) + FVector(0.f, 0.f, H * 0.5f + 5.f));
				return;
			}
			if (Is(TEXT("chapel")))
			{
				// A clustered column: a core with four shafts, a moulded base and capital.
				Box(P.A, FVector(-24.f, -24.f, 0.f), FVector(24.f, 24.f, 22.f));
				Add(P.A, FVector(28.f, 28.f, H - 44.f), FVector(0.f, 0.f, H * 0.5f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
				for (int32 f = 0; f < 4; ++f)
				{
					const FVector D = FRotator(0.f, 90.f * f + 45.f, 0.f).RotateVector(FVector(15.f, 0.f, 0.f));
					Add(P.A, FVector(13.f, 13.f, H - 44.f), D + FVector(0.f, 0.f, H * 0.5f), FRotator::ZeroRotator, EWRLookMesh::Cylinder, 0.95f);
				}
				Box(P.B, FVector(-24.f, -24.f, H - 22.f), FVector(24.f, 24.f, H));
				return;
			}
			// A round column in drums with a base and capital (granite, sandstone, white stone, coral).
			const FSurf& S = Is(TEXT("coral")) ? P.B : P.A;
			Box(S, FVector(-23.f, -23.f, 0.f), FVector(23.f, 23.f, 18.f));
			for (float Z = 18.f; Z < H - 22.f; Z += 52.f)
			{
				Add(S, FVector(36.f, 36.f, FMath::Min(50.5f, H - 22.f - Z)), FVector(0.f, 0.f, Z + FMath::Min(50.5f, H - 22.f - Z) * 0.5f), FRotator::ZeroRotator, EWRLookMesh::Cylinder, 0.9f + 0.2f * Rand(FVector(0.f, 0.f, Z), 321));
			}
			Box(Is(TEXT("whitestone")) ? FSurf::Glaze(Tier.Look.B) : Is(TEXT("granite")) ? FSurf::Stone(Tier.Look.C, 0.6f) : S, FVector(-24.f, -24.f, H - 22.f), FVector(24.f, 24.f, H));
			if (Is(TEXT("sandstone"))) Box(FSurf::Lit(Tier.Look.Glow, 0.5f), FVector(-18.5f, -18.5f, H - 40.f), FVector(18.5f, 18.5f, H - 36.f));
			if (Is(TEXT("coral"))) for (int32 c = 0; c < 4; ++c) Rod(FSurf::Stone(Tier.Look.C, 0.8f), FVector(17.f, 0.f, 80.f + c * 50.f), FVector(28.f, Jit(FVector(c), 331) * 8.f, 90.f + c * 50.f), 3.f);
		}

		void Stairs()
		{
			// Eight steps a storey up across the cell toward +X, as the plain treads stand.
			const float Rise = 37.5f;
			for (int32 k = 1; k <= 8; ++k)
			{
				const float X = -225.f + 50.f * k;
				if (Is(TEXT("panels")))
				{
					// A steel tread with grip ribs on the stringers.
					Box(P.A, FVector(X - 24.f, -98.f, Rise * k - 4.f), FVector(X + 24.f, 98.f, Rise * k));
					for (float Y = -90.f; Y < 91.f; Y += 15.f) Box(FSurf::Steel(Tier.Look.A * 0.8f), FVector(X - 22.f, Y - 0.6f, Rise * k), FVector(X + 22.f, Y + 0.6f, Rise * k + 0.8f));
				}
				else if (TimberStairs())
				{
					const FSurf W = Is(TEXT("felt")) ? FSurf::Stone(Tier.Look.C, 0.9f) : FSurf::Wood(Is(TEXT("timberframe")) ? Tier.Look.B : Tier.Look.A);
					for (const float D : { -12.75f, 12.75f }) Box(W, FVector(X + D - 12.f, -100.f, Rise * k - 6.f), FVector(X + D + 12.f, 100.f, Rise * k));
				}
				else
				{
					const FSurf& S = Is(TEXT("coral")) || Is(TEXT("granite")) ? P.B : P.A;
					Box(P.Dark, FVector(X - 24.f, -99.f, 0.f), FVector(X + 24.f, 99.f, Rise * k - 2.f));
					for (int32 j = 0; j < k; ++j)
					{
						const float Split = -30.f + 60.f * Rand(FVector(X, 0.f, j * Rise), 61);
						for (int32 Half = 0; Half < 2; ++Half)
						{
							const float Y0 = Half ? Split + 1.f : -100.f, Y1 = Half ? 100.f : Split - 1.f;
							Box(S, FVector(X - 24.5f, Y0, j * Rise), FVector(X + 24.5f, Y1, (j + 1) * Rise - 1.5f));
						}
					}
				}
			}
			if (Is(TEXT("panels")) || TimberStairs())
			{
				const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(300.f, 400.f));
				const FSurf S = Is(TEXT("panels")) ? FSurf::Steel(Tier.Look.A * 0.7f) : Is(TEXT("felt")) ? FSurf::Wood(Tier.Look.B) : FSurf::Wood(Tier.Look.A);
				Add(S, FVector(500.f, 8.f, 30.f), FVector(0.f, -104.f, 135.f), FRotator(Pitch, 0.f, 0.f));
				Add(S, FVector(500.f, 8.f, 30.f), FVector(0.f, 104.f, 135.f), FRotator(Pitch, 0.f, 0.f));
			}
		}

		void Roof(float Len)
		{
			// The collision slab rises 200 over the cell toward +X; its underside is Z = (X + 200) / 2 from the eave at
			// X -240 to 210. A deck on rafters, then the tier's covering in courses, each tipped proud of the one below.
			const float Th = FMath::Atan2(200.f, 400.f);
			const float Pitch = FMath::RadiansToDegrees(Th);
			const FVector Along(FMath::Cos(Th), 0.f, FMath::Sin(Th)), Up(-FMath::Sin(Th), 0.f, FMath::Cos(Th));
			const float Slope = 450.f / FMath::Cos(Th);
			const FVector Eave(-240.f, 0.f, -20.f);
			const float Deck = 10.f;
			const FSurf DeckS = Is(TEXT("panels")) ? FSurf::Steel(Tier.Look.A * 0.5f) : FSurf::Wood(TimberStairs() ? Tier.Look.A * 0.7f : FLinearColor(0.12f, 0.08f, 0.05f));
			Add(DeckS, FVector(Slope, Len, Deck), Eave + Along * (Slope * 0.5f) + Up * (Deck * 0.5f), FRotator(Pitch, 0.f, 0.f), EWRLookMesh::Cube, 0.6f);
			for (const float Y : { -150.f, -50.f, 50.f, 150.f })
			{
				Add(Is(TEXT("panels")) ? FSurf::Steel(Tier.Look.A * 0.6f) : FSurf::Wood(TimberStairs() ? Tier.Look.A : FLinearColor(0.16f, 0.1f, 0.06f)), FVector(Slope, 12.f, 14.f),
					Eave + Along * (Slope * 0.5f) - Up * 7.f + FVector(0.f, Y, 0.f), FRotator(Pitch, 0.f, 0.f));
			}
			const auto OnDeck = [&](float S, float Y, float Lift) { return Eave + Along * S + Up * (Deck + Lift) + FVector(0.f, Y, 0.f); };
			const auto Courses2 = [&](const FSurf& S, float T, float L, float Step, float WMin, float WMax, float Gap, float Var, EWRLookMesh Mesh)
			{
				const float Tip = FMath::RadiansToDegrees(FMath::Atan2(T, L));
				for (int32 i = 0; i * Step < Slope - 4.f; ++i)
				{
					const float S0 = i * Step, S1 = FMath::Min(S0 + L, Slope);
					for (float Y = -Len * 0.5f - Rand(FVector(S0, 0.f, 0.f), 11) * WMax; Y < Len * 0.5f;)
					{
						const float W = WMin + Rand(FVector(S0, Y, 1.f), 12) * (WMax - WMin);
						const float Y0 = FMath::Max(Y, -Len * 0.5f), Y1 = FMath::Min(Y + W - Gap, Len * 0.5f);
						if (Y1 - Y0 > 4.f)
						{
							const FVector C = OnDeck((S0 + S1) * 0.5f, (Y0 + Y1) * 0.5f, T);
							Add(S, FVector(S1 - S0, Y1 - Y0, T), C, FRotator(Pitch - Tip, 0.f, 0.f), Mesh, 1.f - Var * 0.5f + Var * Rand(C, 13));
						}
						Y += W;
					}
				}
			};
			if (Is(TEXT("panels")))
			{
				// Standing seam: full-length steel sheets with a raised seam every 50 cm.
				for (float Y = -Len * 0.5f; Y < Len * 0.5f - 1.f; Y += 50.f)
				{
					const float Y1 = FMath::Min(Len * 0.5f, Y + 50.f);
					Add(P.A, FVector(Slope, Y1 - Y - 0.6f, 1.5f), OnDeck(Slope * 0.5f, (Y + Y1) * 0.5f, 0.75f), FRotator(Pitch, 0.f, 0.f), EWRLookMesh::Cube, 0.92f + 0.16f * Rand(FVector(Y), 341));
					Add(FSurf::Steel(Tier.Look.A * 0.85f), FVector(Slope, 1.6f, 4.f), OnDeck(Slope * 0.5f, Y1, 2.f), FRotator(Pitch, 0.f, 0.f));
				}
			}
			else if (Is(TEXT("sandstone")))
			{
				// Barrel tiles: pans flat on the deck, half-round caps over each joint.
				for (float Y = -Len * 0.5f + 9.f; Y < Len * 0.5f - 4.f; Y += 18.f)
				{
					Add(FSurf::Stone(Tier.Look.B, 0.8f), FVector(Slope, 14.f, 1.6f), OnDeck(Slope * 0.5f, Y, 0.8f), FRotator(Pitch, 0.f, 0.f));
					for (float S = 0.f; S < Slope - 4.f; S += 40.f)
					{
						const FVector C = OnDeck(S + 21.f, Y + 9.f, 2.f);
						Add(FSurf::Stone(Tier.Look.B * (0.9f + 0.2f * Rand(C, 351)), 0.75f), FVector(10.f, 10.f, FMath::Min(42.f, Slope - S)), C, FRotator(Pitch - 90.f, 0.f, 0.f) + FRotator(0.f, 0.f, 0.f), EWRLookMesh::Cylinder);
					}
				}
			}
			else if (Is(TEXT("reeds")))
			{
				// Thick reed thatch in four layers, the eave cut square and darker.
				for (int32 l = 0; l < 4; ++l)
				{
					for (float S = l * 20.f; S < Slope - 10.f; S += 80.f)
					{
						Add(FSurf::Stone(Tier.Look.B * (0.85f - 0.08f * l), 0.95f), FVector(FMath::Min(110.f, Slope - S), Len + 10.f, 8.f), OnDeck(S + 55.f, 0.f, 4.f + l * 7.f), FRotator(Pitch - 4.f, 0.f, 0.f));
					}
				}
				Add(FSurf::Stone(Tier.Look.B * 0.75f, 0.95f), FVector(22.f, 22.f, Len + 10.f), OnDeck(Slope - 6.f, 0.f, 20.f), FRotator(0.f, 0.f, 90.f), EWRLookMesh::Cylinder);   // the ridge bundle
			}
			else if (Is(TEXT("coral")))
			{
				// Overlapping shell scallops, white and pale coral.
				int32 Row = 0;
				for (float S = 0.f; S < Slope - 6.f; S += 16.f, ++Row)
				{
					for (float Y = -Len * 0.5f + 12.f + (Row & 1) * 10.f; Y < Len * 0.5f - 4.f; Y += 20.f)
					{
						const FVector C = OnDeck(S + 10.f, Y, 2.5f);
						Add(FSurf::Glaze(FMath::Lerp(Tier.Look.B, Tier.Look.C, Rand(C, 361) * 0.45f), 0.4f), FVector(22.f, 22.f, 3.f), C, FRotator(Pitch - 8.f, 0.f, 0.f) + FRotator(0.f, 0.f, 0.f), EWRLookMesh::Cylinder);
					}
				}
			}
			else if (Is(TEXT("felt")))
			{
				// A smooth fungal cap, rolled at the eave, glowing gills under it.
				Add(FSurf::Stone(Tier.Look.C, 0.9f), FVector(Slope, Len + 20.f, 8.f), OnDeck(Slope * 0.5f, 0.f, 4.f), FRotator(Pitch, 0.f, 0.f));
				Add(FSurf::Stone(Tier.Look.C * 0.9f, 0.9f), FVector(18.f, 18.f, Len + 20.f), OnDeck(2.f, 0.f, 2.f), FRotator(0.f, 0.f, 90.f), EWRLookMesh::Cylinder);
				for (float Y = -Len * 0.5f + 3.f; Y < Len * 0.5f; Y += 6.f)
				{
					Add(FSurf::Lit(Tier.Look.Glow, 0.7f), FVector(26.f, 0.8f, 6.f), Eave + Along * 18.f - Up * 4.f + FVector(0.f, Y, 0.f), FRotator(Pitch, 0.f, 0.f));
				}
			}
			else
			{
				const bool bShingle = Is(TEXT("timberframe")) || Is(TEXT("logs"));
				const FSurf S = Is(TEXT("whitestone")) ? FSurf::Glaze(Tier.Look.B, 0.22f) : bShingle ? FSurf::Wood(Is(TEXT("logs")) ? Tier.Look.A * 0.75f : Tier.Look.B * 0.8f)
					: Is(TEXT("granite")) ? FSurf::Stone(Tier.Look.B, 0.6f) : Is(TEXT("voidstone")) ? FSurf::Glaze(Tier.Look.B, 0.12f) : Is(TEXT("columns")) ? FSurf::Stone(Tier.Look.A * 0.8f, 0.7f) : P.A;
				if (bShingle) Courses2(S, 2.6f, 52.f, 40.f, 26.f, 42.f, 1.5f, 0.2f, EWRLookMesh::Cube);
				else Courses2(S, 2.2f, 36.f, 26.f, 20.f, 32.f, 1.2f, 0.35f, EWRLookMesh::Cube);
			}
			// What lies on top or runs along the ridge (the high edge, X 210).
			const FVector Ridge = OnDeck(Slope - 4.f, 0.f, 6.f);
			if (Is(TEXT("timberframe")))
			{
				// Living moss in soft mounds over the shingles.
				for (int32 m = 0; m < 14; ++m)
				{
					const FVector C = OnDeck(30.f + Rand(FVector(m, 0.f, 0.f), 371) * (Slope - 60.f), -Len * 0.5f + 25.f + Rand(FVector(m, 1.f, 0.f), 372) * (Len - 50.f), 6.f);
					Add(FSurf::Stone(Tier.Look.C * (0.8f + 0.4f * Rand(C, 373)), 1.f), FVector(40.f + Rand(C, 374) * 40.f, 40.f + Rand(C, 375) * 40.f, 5.f + Rand(C, 376) * 5.f), C, FRotator(Pitch, Rand(C, 377) * 90.f, 0.f), EWRLookMesh::Cylinder);
				}
			}
			else if (Is(TEXT("logs")))
			{
				// Packed snow over the upper slope, its lower edge in soft lumps.
				Add(FSurf::Stone(Tier.Look.C, 0.92f), FVector(Slope * 0.78f, Len + 4.f, 9.f), OnDeck(Slope * 0.6f, 0.f, 8.f), FRotator(Pitch, 0.f, 0.f));
				for (float Y = -Len * 0.5f + 15.f; Y < Len * 0.5f; Y += 30.f)
				{
					Add(FSurf::Stone(Tier.Look.C, 0.92f), FVector(28.f + Rand(FVector(Y), 381) * 16.f, 30.f, 9.f), OnDeck(Slope * 0.21f, Y, 6.f), FRotator(Pitch, 0.f, 0.f), EWRLookMesh::Cylinder);
				}
			}
			else if (Is(TEXT("granite")))
			{
				Add(FSurf::Stone(Tier.Look.C, 0.6f), FVector(20.f, Len + 4.f, 4.f), Ridge, FRotator(Pitch, 0.f, 0.f));   // verdigris copper ridge
			}
			else if (Is(TEXT("columns")))
			{
				Add(FSurf::Glaze(Tier.Look.B, 0.12f), FVector(22.f, Len + 4.f, 8.f), Ridge, FRotator(Pitch, 0.f, 0.f));   // obsidian ridge
			}
			else if (Is(TEXT("chapel")))
			{
				// Iron cresting along the ridge: a bar with spear finials.
				const FSurf Iron = FSurf::Stone(FLinearColor(0.02f, 0.02f, 0.025f), 0.5f);
				Add(Iron, FVector(3.f, Len, 3.f), Ridge + FVector(0.f, 0.f, 8.f));
				for (float Y = -Len * 0.5f + 10.f; Y < Len * 0.5f; Y += 25.f)
				{
					Rod(Iron, Ridge + FVector(0.f, Y, 0.f), Ridge + FVector(0.f, Y, 22.f), 1.6f, true);
					Spike(Iron, Ridge + FVector(0.f, Y, 22.f), FVector::UpVector, 4.f, 8.f);
				}
			}
			else if (Is(TEXT("voidstone")))
			{
				Add(FSurf::Lit(Tier.Look.Glow, 0.9f), FVector(2.f, Len, 1.5f), Ridge + FVector(0.f, 0.f, 2.f), FRotator(Pitch, 0.f, 0.f));
			}
			else if (Is(TEXT("whitestone")))
			{
				Add(FSurf::Stone(Tier.Look.A, 0.65f), FVector(18.f, Len + 4.f, 6.f), Ridge, FRotator(Pitch, 0.f, 0.f));
			}
		}

		/** A tier's door leaf in the leaf's space (centred, hinge at -X), W x H, about 10 thick. */
		void Leaf(float W, float H)
		{
			const FSurf Iron = FSurf::Stone(FLinearColor(0.025f, 0.024f, 0.024f), 0.45f);
			if (Is(TEXT("panels")))
			{
				Box(P.A, FVector(-W * 0.5f, -4.f, -H * 0.5f), FVector(W * 0.5f, 4.f, H * 0.5f));
				const FSurf Frame2 = FSurf::Steel(Tier.Look.A * 0.7f);
				for (const float Face : { -1.f, 1.f })
				{
					Box(Frame2, FVector(-W * 0.5f, Face * 4.f - 1.f, -H * 0.5f), FVector(-W * 0.5f + 8.f, Face * 4.f + 1.f, H * 0.5f));
					Box(Frame2, FVector(W * 0.5f - 8.f, Face * 4.f - 1.f, -H * 0.5f), FVector(W * 0.5f, Face * 4.f + 1.f, H * 0.5f));
					for (const float Z : { -H * 0.42f, 0.f, H * 0.42f }) Box(Frame2, FVector(-W * 0.5f + 8.f, Face * 4.f - 1.f, Z - 5.f), FVector(W * 0.5f - 8.f, Face * 4.f + 1.f, Z + 5.f));
					Studs(FSurf::Steel(Tier.Look.A * 0.85f), FVector(-W * 0.5f + 4.f, Face * 5.4f, -H * 0.5f + 8.f), FVector(-W * 0.5f + 4.f, Face * 5.4f, H * 0.5f - 8.f), 18.f, 2.f);
					Studs(FSurf::Steel(Tier.Look.A * 0.85f), FVector(W * 0.5f - 4.f, Face * 5.4f, -H * 0.5f + 8.f), FVector(W * 0.5f - 4.f, Face * 5.4f, H * 0.5f - 8.f), 18.f, 2.f);
				}
				Add(Frame2, FVector(4.f, 4.f, 20.f), FVector(W * 0.5f - 18.f, 7.f, 0.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);   // the handle
				return;
			}
			// A heavy door of boards in the tier's wood or a dark oak, its bands in the tier's metal.
			const FSurf Boards2 = TimberStairs() ? FSurf::Wood(Is(TEXT("felt")) ? Tier.Look.B : Tier.Look.A) : FSurf::Wood(FLinearColor(0.11f, 0.065f, 0.035f));
			const FSurf Band = Is(TEXT("granite")) ? FSurf::Stone(Tier.Look.C, 0.6f) : Is(TEXT("sandstone")) || Is(TEXT("whitestone")) ? FSurf::Steel(FLinearColor(0.5f, 0.32f, 0.12f), 0.35f)
				: Is(TEXT("reeds")) ? FSurf::Glaze(Tier.Look.C, 0.3f) : Is(TEXT("voidstone")) ? FSurf::Lit(Tier.Look.Glow, 0.6f) : Is(TEXT("geode")) ? FSurf::Lit(Tier.Look.C, 0.5f) : Iron;
			const int32 N = FMath::Max(3, FMath::RoundToInt(W / 23.f));
			const float Pitch = W / N;
			for (int32 i = 0; i < N; ++i) Add(Boards2, FVector(Pitch - 1.5f, 6.f, H - 2.f), FVector(-W * 0.5f + (i + 0.5f) * Pitch, 1.f, 0.f));
			for (const float Z : { -H * 0.4f, -H * 0.13f, H * 0.13f, H * 0.4f }) Box(Band, FVector(-W * 0.5f - 1.f, 4.f, Z - 4.f), FVector(W * 0.5f, 5.5f, Z + 4.f));
			for (const float Z : { -H * 0.33f, H * 0.33f }) Box(Boards2, FVector(-W * 0.5f + 5.f, -5.f, Z - 7.f), FVector(W * 0.5f - 5.f, -2.f, Z + 7.f));
			Add(Iron, FVector(3.f, 3.f, 14.f), FVector(W * 0.5f - 16.f, 7.f, 0.f));
			if (Is(TEXT("chapel")) || Is(TEXT("columns")))
			{
				for (const float X : { -W * 0.25f, W * 0.25f }) for (const float Z : { -H * 0.3f, 0.f, H * 0.3f }) Add(Iron, FVector(4.f, 4.f, 4.f), FVector(X, 6.f, Z), FRotator(45.f, 0.f, 45.f));   // studs
			}
		}

		/** A freeform beam, post or brace: one member from 0 to L along +X. */
		void Beam(float L)
		{
			if (Is(TEXT("wood")))
			{
				Add(FSurf::Wood(FLinearColor(0.28f, 0.17f, 0.085f)), FVector(L, 22.f, 22.f), FVector(L * 0.5f, 0.f, 0.f));
				for (const float X : { 2.f, L - 2.f }) Add(FSurf::Wood(FLinearColor(0.2f, 0.12f, 0.06f)), FVector(4.f, 22.5f, 22.5f), FVector(X, 0.f, 0.f));   // end grain
				return;
			}
			if (Is(TEXT("panels")))
			{
				const FSurf S = FSurf::Steel(Tier.Look.A * 0.8f);
				Box(S, FVector(0.f, -11.f, 9.f), FVector(L, 11.f, 12.f));
				Box(S, FVector(0.f, -11.f, -12.f), FVector(L, 11.f, -9.f));
				Box(S, FVector(0.f, -1.5f, -9.f), FVector(L, 1.5f, 9.f));
				for (const float X : { 1.f, L - 1.f })
				{
					Box(S, FVector(X - 1.f, -12.f, -12.f), FVector(X + 1.f, 12.f, 12.f));   // end plates
					for (const float Y : { -7.f, 7.f }) for (const float Z : { -6.f, 6.f }) Box(S, FVector(X - 2.f, Y - 1.5f, Z - 1.5f), FVector(X + 2.f, Y + 1.5f, Z + 1.5f));
				}
				return;
			}
			const bool bTimber = TimberStairs();
			if (bTimber)
			{
				const FSurf S = Is(TEXT("felt")) ? FSurf::Wood(Tier.Look.B) : FSurf::Wood(Tier.Look.A);
				if (Is(TEXT("logs")) || Is(TEXT("reeds")) || Is(TEXT("felt"))) Add(S, FVector(24.f, 24.f, L), FVector(L * 0.5f, 0.f, 0.f), FRotator(90.f, 0.f, 0.f), EWRLookMesh::Cylinder);
				else Add(S, FVector(L, 22.f, 22.f), FVector(L * 0.5f, 0.f, 0.f));
				// Bands every metre: lacquer (stilts), amber notches (heartwood), iron (the rest).
				for (float X = 50.f; X < L - 20.f; X += 100.f)
				{
					if (Is(TEXT("reeds"))) Add(FSurf::Glaze(Tier.Look.C, 0.3f), FVector(26.f, 26.f, 5.f), FVector(X, 0.f, 0.f), FRotator(90.f, 0.f, 0.f), EWRLookMesh::Cylinder);
					else if (Is(TEXT("timberframe"))) for (const float F : { -1.f, 1.f }) Add(FSurf::Lit(Tier.Look.Glow, 0.35f), FVector(4.f, 1.f, 4.f), FVector(X, F * 11.3f, 0.f), FRotator(45.f, 0.f, 0.f));
					else if (Is(TEXT("logs"))) Add(FSurf::Stone(FLinearColor(0.025f, 0.024f, 0.024f), 0.45f), FVector(26.f, 26.f, 4.f), FVector(X, 0.f, 0.f), FRotator(90.f, 0.f, 0.f), EWRLookMesh::Cylinder);
				}
				return;
			}
			// Stone-like tiers: dressed blocks along it (a stone lintel run), the tier's accent at the joints.
			const FSurf& S = Is(TEXT("voidstone")) ? P.A : Is(TEXT("coral")) ? P.B : Is(TEXT("granite")) ? P.A : P.A;
			for (float X = 0.f; X < L - 1.f; X += 50.f)
			{
				const float X1 = FMath::Min(L, X + 50.f);
				if (Is(TEXT("columns"))) Add(S, FVector(X1 - X - 1.2f, 24.f, 22.f), FVector((X + X1) * 0.5f, 0.f, 0.f), FRotator(0.f, 0.f, 0.f));
				else Box(S, FVector(X + 0.6f, -12.f, -12.f), FVector(X1 - 0.6f, 12.f, 12.f), 0.92f + 0.16f * Rand(FVector(X), 391));
			}
			Box(P.Dark, FVector(0.f, -9.f, -9.f), FVector(L, 9.f, 9.f));
			if (Is(TEXT("voidstone"))) for (const float F : { -1.f, 1.f }) Box(FSurf::Lit(Tier.Look.Glow, 0.85f), FVector(0.f, F * 12.3f - 0.5f, -1.f), FVector(L, F * 12.3f + 0.5f, 1.f));
			if (Is(TEXT("geode"))) for (float X = 25.f; X < L; X += 50.f) Add(FSurf::Lit(Tier.Look.C, 0.6f), FVector(5.f, 25.f, 5.f), FVector(X, 0.f, 0.f), FRotator(45.f, 0.f, 0.f));
			if (Is(TEXT("granite"))) for (float X = 0.f; X <= L; X += 100.f) Box(FSurf::Stone(Tier.Look.C, 0.6f), FVector(X - 2.f, -12.5f, -12.5f), FVector(X + 2.f, 12.5f, 12.5f));
			if (Is(TEXT("sandstone"))) Box(FSurf::Lit(Tier.Look.Glow, 0.45f), FVector(0.f, -12.4f, -1.f), FVector(L, 12.4f, 1.f));
			if (Is(TEXT("whitestone"))) Box(FSurf::Glaze(Tier.Look.B), FVector(0.f, -12.4f, -2.f), FVector(L, 12.4f, 2.f));
		}
	};
}

TArray<FWRLookBlock> FWRBuildLook::TierBlocks(const FWRPieceDef& Def, const FTransform& PieceToWorld)
{
	using namespace WRBuildLookSetsDetail;
	TArray<FWRLookBlock> Out;
	const FWRTierDef* Tier = FWRBuildSets::Get().Tier(Def.Tier);
	if (!Tier) return Out;
	FStyle S(PieceToWorld, Out, *Tier);
	if (Def.IsBeam())
	{
		if (S.Is(TEXT("stone")))
		{
			// A stone beam: dressed blocks along it in the building kit's own stone.
			for (float X = 0.f; X < Def.FreeLength - 1.f; X += 50.f)
			{
				S.Box(FSurf::Stone(FLinearColor(0.25f, 0.24f, 0.22f)), FVector(X + 0.6f, -12.f, -12.f), FVector(FMath::Min(Def.FreeLength, X + 50.f) - 0.6f, 12.f, 12.f), 0.9f + 0.2f * S.Rand(FVector(X), 401));
			}
			return Out;
		}
		S.Beam(FMath::Max(50.f, Def.FreeLength));
		return Out;
	}
	if (S.Is(TEXT("wood")) || S.Is(TEXT("stone"))) return Out;   // wood and stone's grid shapes are WRBuildLook's own
	const FName Sh = Def.Shape;
	if (Sh == TEXT("foundation")) S.Foundation();
	else if (Sh == TEXT("wall")) S.Wall(300.f);
	else if (Sh == TEXT("halfwall")) S.Wall(150.f);
	else if (Sh == TEXT("upperwall")) S.Wall(210.f);
	else if (Sh == TEXT("floor")) S.Floor();
	else if (Sh == TEXT("roof")) S.Roof(420.f);
	else if (Sh == TEXT("gable")) S.Gable();
	else if (Sh == TEXT("pillar")) S.Pillar();
	else if (Sh == TEXT("stairs")) S.Stairs();
	else if (Sh == TEXT("window")) S.Window(300.f);
	else if (Sh == TEXT("door") || Sh == TEXT("doorway")) S.Doorway(300.f);
	return Out;
}

TArray<FWRLookBlock> FWRBuildLook::TierLeafBlocks(const FWRPieceDef& Def, const FVector& LeafSize, const FTransform& PieceToWorld)
{
	using namespace WRBuildLookSetsDetail;
	TArray<FWRLookBlock> Out;
	const FWRTierDef* Tier = FWRBuildSets::Get().Tier(Def.Tier);
	if (!Tier || Tier->Look.Style == TEXT("wood") || Tier->Look.Style == TEXT("stone")) return Out;
	const FTransform LeafToWorld = FTransform(FVector(0.f, 0.f, LeafSize.Z * 0.5f)) * PieceToWorld;
	FStyle S(LeafToWorld, Out, *Tier);
	S.Leaf(LeafSize.X, LeafSize.Z);
	return Out;
}

FBox FWRBuildLook::BeamBox(const FWRPieceDef& Def)
{
	return FBox(FVector(0.f, -12.f, -12.f), FVector(FMath::Max(50.f, Def.FreeLength), 12.f, 12.f));
}

FLinearColor FWRBuildLook::TierCardColor(FName Tier)
{
	const FWRTierDef* T = FWRBuildSets::Get().Tier(Tier);
	if (!T) return FLinearColor(0.55f, 0.55f, 0.58f);
	if (T->Look.Style == TEXT("wood")) return FLinearColor(FColor(0xA8, 0x74, 0x3F));
	if (T->Look.Style == TEXT("stone")) return FLinearColor(FColor(0x8B, 0x8F, 0x94));
	// The cards draw in Slate's space: lift the linear colour back toward what the eye sees.
	const FLinearColor A = T->Look.A;
	return FLinearColor(FMath::Pow(A.R, 1.f / 2.2f), FMath::Pow(A.G, 1.f / 2.2f), FMath::Pow(A.B, 1.f / 2.2f));
}
