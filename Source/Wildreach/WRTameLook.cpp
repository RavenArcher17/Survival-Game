// What taming looks like on the body (cooks/build-taming.md 2.3, 2.6): saddles, the pack saddle and barding modelled in
// code from their real parts and fitted to each creature's back (creatures.json seat / back), a mutation's morph as an
// overlay over the creature's own texture (M_TameMorph), a baby's size, and an incubator's egg and warmth.
//
// Tack is built in a "barrel" frame: X along the spine (forward), Y to the right, Z up, the origin on the back's top
// where the saddle sits. The back is modelled as a barrel of radius Rb (a little over the back's half width) whose top
// line is the spine, so every panel drapes over it the way leather lies on a horse: the seat and blanket on top, the
// skirts down the flanks, the girth closing round the belly. Leather and wood go in M_HeldTool's vertex colour (its
// grain runs along U, which the panels lay along the spine: leather grain, wood grain).

#include "WRTaming.h"
#include "WRCreature.h"
#include "WRBuilding.h"
#include "WRFarm.h"
#include "WRToolMesh.h"
#include "WRSealBody.h"
#include "WRTuning.h"
#include "ProceduralMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace WRTameLookDetail
{
	FColor Pack(const FLinearColor& C, float Rough)
	{
		return FColor(FMath::Clamp(FMath::RoundToInt(C.R * 255.f), 0, 255), FMath::Clamp(FMath::RoundToInt(C.G * 255.f), 0, 255),
			FMath::Clamp(FMath::RoundToInt(C.B * 255.f), 0, 255), FMath::Clamp(FMath::RoundToInt(Rough * 255.f), 0, 255));
	}

	/** A smooth-shaded triangle facing Out (M_HeldTool's winding: the engine draws the side its cross points away from). */
	void SmoothTri(FWRToolMesh& M, int32 A, int32 B, int32 C, const FVector& Out)
	{
		const FVector N = FVector::CrossProduct(M.Verts[B] - M.Verts[A], M.Verts[C] - M.Verts[A]);
		if (FVector::DotProduct(N, Out) > 0.f) M.Tris.Append({ A, C, B });
		else M.Tris.Append({ A, B, C });
	}

	/**
	 * A sheet with thickness from a grid of points on its outer face (Rows x Cols, Rows along U): smooth outer and inner
	 * faces, flat edges. Colour per vertex (rim rows can be darker: a stitched or rolled edge).
	 */
	void Panel(FWRToolMesh& M, const TArray<TArray<FVector>>& Grid, float Thick, TFunctionRef<FLinearColor(int32, int32)> Colour, float Rough, const FVector& OutHint)
	{
		const int32 R = Grid.Num(), C = R > 0 ? Grid[0].Num() : 0;
		if (R < 2 || C < 2) return;
		TArray<FVector> N;
		N.SetNum(R * C);
		for (int32 r = 0; r < R; ++r)
		{
			for (int32 c = 0; c < C; ++c)
			{
				const FVector DU = Grid[FMath::Min(r + 1, R - 1)][c] - Grid[FMath::Max(r - 1, 0)][c];
				const FVector DV = Grid[r][FMath::Min(c + 1, C - 1)] - Grid[r][FMath::Max(c - 1, 0)];
				FVector Nn = FVector::CrossProduct(DU, DV).GetSafeNormal();
				if (FVector::DotProduct(Nn, OutHint.IsNearlyZero() ? Grid[r][c] : OutHint) < 0.f && !OutHint.IsNearlyZero()) Nn = -Nn;
				N[r * C + c] = Nn;
			}
		}
		// Outward: the side facing OutHint (or away from the panel's middle when none).
		const auto Face = [&](bool bOuter)
		{
			const int32 Base = M.Verts.Num();
			for (int32 r = 0; r < R; ++r)
			{
				for (int32 c = 0; c < C; ++c)
				{
					const FVector& Nn = N[r * C + c];
					M.Verts.Add(bOuter ? Grid[r][c] : Grid[r][c] - Nn * Thick);
					M.Normals.Add(bOuter ? Nn : -Nn);
					M.UVs.Add(FVector2D(r / 4.f, c / static_cast<float>(C - 1)));
					M.Colors.Add(Pack(Colour(r, c) * (bOuter ? 1.f : 0.7f), Rough));
				}
			}
			for (int32 r = 0; r + 1 < R; ++r)
			{
				for (int32 c = 0; c + 1 < C; ++c)
				{
					const int32 A = Base + r * C + c, B = A + 1, D = A + C, E = D + 1;
					const FVector Out = (bOuter ? 1.f : -1.f) * (N[r * C + c] + N[(r + 1) * C + c + 1]);
					SmoothTri(M, A, B, E, Out);
					SmoothTri(M, A, E, D, Out);
				}
			}
			return Base;
		};
		Face(true);
		Face(false);
		// The edges: a flat strip round the rim.
		TArray<TPair<int32, int32>> Rim;
		for (int32 c = 0; c < C; ++c) Rim.Add({ 0, c });
		for (int32 r = 1; r < R; ++r) Rim.Add({ r, C - 1 });
		for (int32 c = C - 2; c >= 0; --c) Rim.Add({ R - 1, c });
		for (int32 r = R - 2; r > 0; --r) Rim.Add({ r, 0 });
		FVector Mid = FVector::ZeroVector;
		for (const TArray<FVector>& Row : Grid) for (const FVector& P : Row) Mid += P;
		Mid /= float(R * C);
		const FColor Edge = Pack(Colour(0, 0) * 0.6f, Rough);
		for (int32 i = 0; i < Rim.Num(); ++i)
		{
			const TPair<int32, int32> P0 = Rim[i], P1 = Rim[(i + 1) % Rim.Num()];
			const FVector A = Grid[P0.Key][P0.Value], B = Grid[P1.Key][P1.Value];
			const FVector A2 = A - N[P0.Key * C + P0.Value] * Thick, B2 = B - N[P1.Key * C + P1.Value] * Thick;
			const FVector Out = ((A + B) * 0.5f - Mid).GetSafeNormal();
			const int32 Base = M.Verts.Num();
			M.Verts.Append({ A, B, B2, A2 });
			M.Normals.Append({ Out, Out, Out, Out });
			M.UVs.Append({ FVector2D(0, 0), FVector2D(1, 0), FVector2D(1, 1), FVector2D(0, 1) });
			M.Colors.Append({ Edge, Edge, Edge, Edge });
			SmoothTri(M, Base, Base + 1, Base + 2, Out);
			SmoothTri(M, Base, Base + 2, Base + 3, Out);
		}
	}

	/** The back a saddle sits on: a barrel of radius Rb whose top line is the spine (the origin on top). */
	struct FBack
	{
		float W = 40.f;     // half width of the back under the saddle
		float Rb = 48.f;    // the barrel's radius
		float Dz = 90.f;    // from the seat down to the belly
		float Chest = 80.f; // from the seat forward to the chest front
		float Rump = 80.f;  // from the seat back to the rump
		FVector At(float X, float Theta, float Off) const
		{
			return FVector(X, (Rb + Off) * FMath::Sin(Theta), (Rb + Off) * FMath::Cos(Theta) - Rb);
		}
	};

	/** A panel draped over the back from X0..X1 and Theta0..Theta1, Off(x, theta) above it. */
	void Drape(FWRToolMesh& M, const FBack& B, float X0, float X1, float T0, float T1, int32 Rows, int32 Cols, TFunctionRef<float(float, float)> Off, float Thick,
		TFunctionRef<FLinearColor(int32, int32)> Colour, float Rough)
	{
		TArray<TArray<FVector>> Grid;
		for (int32 r = 0; r < Rows; ++r)
		{
			TArray<FVector> Row;
			const float X = FMath::Lerp(X0, X1, r / float(Rows - 1));
			for (int32 c = 0; c < Cols; ++c)
			{
				const float T = FMath::Lerp(T0, T1, c / float(Cols - 1));
				Row.Add(B.At(X, T, Off(X, T)));
			}
			Grid.Add(Row);
		}
		Panel(M, Grid, Thick, Colour, Rough, FVector::ZeroVector);
	}

	/** A rivet / stud: a small dome on the surface. */
	void Stud(FWRToolMesh& M, const FVector& At, const FVector& Normal, float R, const FLinearColor& Metal)
	{
		M.Lathe(At - Normal * 0.4f, Normal, R * 1.1f, { FVector(0.f, R, R), FVector(0.5f, R * 0.85f, R * 0.85f), FVector(1.f, 0.f, 0.f) }, 6, Metal, 0.35f);
	}

	struct FTackLook
	{
		FLinearColor Leather = FLinearColor(0.2f, 0.11f, 0.05f);
		FLinearColor Blanket = FLinearColor(0.32f, 0.07f, 0.05f);
		FLinearColor Stitch = FLinearColor(0.55f, 0.45f, 0.3f);
		FLinearColor Metal = FLinearColor(0.32f, 0.33f, 0.35f);
		float MetalRough = 0.4f;
		bool bMetalFittings = false;
		FLinearColor Band = FLinearColor::Black;   // biome barding's set colour (0 = none)
	};

	FTackLook LookOf(FName Item)
	{
		FTackLook L;
		const FString S = Item.ToString();
		if (S == TEXT("saddle_hide") || S == TEXT("saddle_pack")) { L.Leather = FLinearColor(0.3f, 0.19f, 0.09f); L.Blanket = FLinearColor(0.36f, 0.32f, 0.24f); L.Metal = FLinearColor(0.3f, 0.22f, 0.12f); L.MetalRough = 0.8f; }
		else if (S == TEXT("saddle_bronze")) { L.Blanket = FLinearColor(0.12f, 0.16f, 0.3f); L.Metal = FLinearColor(0.62f, 0.4f, 0.14f); L.MetalRough = 0.3f; L.bMetalFittings = true; }
		else if (S == TEXT("saddle_iron")) { L.Blanket = FLinearColor(0.34f, 0.06f, 0.05f); L.Metal = FLinearColor(0.2f, 0.2f, 0.21f); L.MetalRough = 0.45f; L.bMetalFittings = true; }
		else if (S == TEXT("saddle_steel")) { L.Leather = FLinearColor(0.07f, 0.045f, 0.03f); L.Blanket = FLinearColor(0.08f, 0.09f, 0.12f); L.Metal = FLinearColor(0.62f, 0.64f, 0.68f); L.MetalRough = 0.25f; L.bMetalFittings = true; }
		else if (S == TEXT("barding_iron")) { L.Metal = FLinearColor(0.2f, 0.2f, 0.21f); L.MetalRough = 0.45f; }
		else if (S.StartsWith(TEXT("barding_")))
		{
			L.Metal = FLinearColor(0.58f, 0.6f, 0.64f);
			L.MetalRough = 0.28f;
			static const TMap<FString, FLinearColor> Bands = {
				{ TEXT("forgeplate"), FLinearColor(0.9f, 0.28f, 0.05f) }, { TEXT("rimeguard"), FLinearColor(0.55f, 0.8f, 0.95f) },
				{ TEXT("stormweave"), FLinearColor(0.3f, 0.45f, 0.9f) }, { TEXT("dunewrap"), FLinearColor(0.85f, 0.65f, 0.3f) },
				{ TEXT("heartguard"), FLinearColor(0.2f, 0.45f, 0.15f) }, { TEXT("skysilk"), FLinearColor(0.85f, 0.88f, 0.95f) },
				{ TEXT("wardcloth"), FLinearColor(0.55f, 0.3f, 0.75f) }, { TEXT("mirehide"), FLinearColor(0.35f, 0.08f, 0.07f) },
				{ TEXT("reefmail"), FLinearColor(0.1f, 0.55f, 0.55f) }, { TEXT("gravecloth"), FLinearColor(0.35f, 0.35f, 0.32f) },
				{ TEXT("sporeweave"), FLinearColor(0.5f, 0.6f, 0.25f) }, { TEXT("hollowplate"), FLinearColor(0.3f, 0.12f, 0.45f) } };
			if (const FLinearColor* B = Bands.Find(S.RightChop(8))) L.Band = *B;
			if (S == TEXT("barding_forgeplate")) L.Metal = FLinearColor(0.09f, 0.085f, 0.08f);
		}
		return L;
	}

	/** The girth: down from the skirts' edges, round under the belly. */
	void Girth(FWRToolMesh& M, const FBack& B, float X, float SkirtTheta, const FTackLook& L)
	{
		const FVector Edge = B.At(X, SkirtTheta, 2.5f);
		const float A = B.W * 1.08f;
		const float Zc = FMath::Lerp(Edge.Z, -B.Dz, 0.45f);
		const float Bot = -B.Dz;
		TArray<FVector> Pts;
		Pts.Add(FVector(X, -Edge.Y, Edge.Z));
		Pts.Add(FVector(X, -FMath::Lerp(Edge.Y, A, 0.6f), FMath::Lerp(Edge.Z, Zc, 0.5f)));
		for (int32 i = 0; i <= 10; ++i)
		{
			const float P = -HALF_PI + PI * i / 10.f;
			Pts.Add(FVector(X, A * FMath::Sin(P), Zc - (Zc - Bot) * FMath::Cos(P)));
		}
		Pts.Add(FVector(X, FMath::Lerp(Edge.Y, A, 0.6f), FMath::Lerp(Edge.Z, Zc, 0.5f)));
		Pts.Add(FVector(X, Edge.Y, Edge.Z));
		M.Tube(Pts, 2.6f, 6, L.Leather * 0.85f, 0.75f);
		// Its buckle on the left side, where it's done up.
		M.Lathe(FVector(X - 4.f, -A - 1.f, Zc), FVector(1.f, 0.f, 0.f), 8.f, { FVector(0.f, 4.5f, 6.f), FVector(1.f, 4.5f, 6.f) }, 4, L.Metal, L.MetalRough);
	}

	/** A stirrup on its leather, hanging from the seat's side. */
	void Stirrup(FWRToolMesh& M, const FBack& B, float Side, const FTackLook& L)
	{
		const FVector Top = B.At(0.f, Side * 1.0f, 3.5f);
		const FVector Foot(Top.X, Top.Y + Side * 4.f, Top.Z - FMath::Max(25.f, (B.Dz + Top.Z) * 0.55f));
		M.Tube({ Top, FMath::Lerp(Top, Foot, 0.5f) + FVector(0.f, Side * 2.f, 0.f), Foot + FVector(0.f, 0.f, 9.f) }, 1.4f, 5, L.Leather, 0.7f);
		// The iron: a D of round bar, the tread across its foot.
		TArray<FVector> D;
		for (int32 i = 0; i <= 12; ++i)
		{
			const float A = PI * i / 12.f;
			D.Add(Foot + FVector(FMath::Cos(A) * 7.f, 0.f, 9.f - FMath::Sin(A) * 0.f - (1.f - FMath::Sin(A)) * 9.f));
		}
		M.Tube(D, 1.1f, 6, L.Metal, L.MetalRough, false);
		M.Lathe(Foot + FVector(-7.f, 0.f, 0.f), FVector(1.f, 0.f, 0.f), 14.f, { FVector(0.f, 3.5f, 1.2f), FVector(1.f, 3.5f, 1.2f) }, 4, L.Metal, L.MetalRough);
	}

	/** A riding saddle: blanket, seat with pommel and cantle, skirts, girth, stirrups, breast collar, its tier's metal. */
	FWRToolMesh RidingSaddle(const FBack& B, const FTackLook& L)
	{
		FWRToolMesh M;
		const float Len = FMath::Clamp(B.W * 1.5f, 42.f, 80.f);
		const float Seat = 0.45f, Skirt = 1.15f;
		// The blanket under it, a hand's width bigger all round, its edge bound in a darker braid.
		Drape(M, B, -Len * 0.62f, Len * 0.62f, -Skirt - 0.12f, Skirt + 0.12f, 9, 17, [](float, float) { return 0.6f; }, 1.2f,
			[&L](int32 r, int32 c) { return (r == 0 || r == 8 || c == 0 || c == 16) ? L.Blanket * 0.45f : L.Blanket; }, 0.95f);
		// The seat and skirts: one hide over the tree, dipping in the middle, rising to the pommel and the cantle.
		const auto SeatOff = [Len, Seat, Skirt](float X, float T)
		{
			const float In = FMath::Clamp(1.f - (FMath::Abs(T) - Seat * 0.6f) / (Seat * 0.6f), 0.f, 1.f);
			const float Front = FMath::SmoothStep(0.25f, 0.5f, X / Len) * 9.f, Back = FMath::SmoothStep(0.22f, 0.5f, -X / Len) * 14.f;
			const float Dip = -2.f * FMath::Cos(X / Len * PI);
			const float Flare = FMath::Max(0.f, FMath::Abs(T) - 0.9f) * 6.f;
			return 2.2f + In * (Front + Back + Dip) + Flare;
		};
		Drape(M, B, -Len * 0.5f, Len * 0.5f, -Skirt, Skirt, 13, 21, SeatOff, 3.f,
			[&L](int32 r, int32 c) { return (c == 6 || c == 14) ? L.Stitch : L.Leather; }, 0.7f);
		// The cantle's rolled back edge and the pommel's horn.
		TArray<FVector> Roll;
		for (int32 i = 0; i <= 8; ++i) { const float T = FMath::Lerp(-Seat, Seat, i / 8.f); Roll.Add(B.At(-Len * 0.5f + 2.f, T, SeatOff(-Len * 0.5f + 2.f, T) + 1.5f)); }
		M.Tube(Roll, 2.8f, 7, L.bMetalFittings ? L.Metal : L.Leather * 0.8f, L.bMetalFittings ? L.MetalRough : 0.7f);
		const FVector Horn = B.At(Len * 0.5f - 5.f, 0.f, SeatOff(Len * 0.5f - 5.f, 0.f));
		M.Lathe(Horn - FVector(0.f, 0.f, 2.f), FVector(0.f, 0.f, 1.f), 10.f, { FVector(0.f, 3.4f, 3.4f), FVector(0.6f, 2.4f, 2.4f), FVector(0.85f, 3.6f, 3.6f), FVector(1.f, 0.f, 0.f) }, 10,
			L.bMetalFittings ? L.Metal : L.Leather * 0.7f, L.bMetalFittings ? L.MetalRough : 0.6f);
		// Metal tiers: a plate on the cantle's face and studs down the skirts' edges.
		if (L.bMetalFittings)
		{
			TArray<TArray<FVector>> Plate;
			for (int32 r = 0; r < 3; ++r)
			{
				TArray<FVector> Row;
				for (int32 c = 0; c < 7; ++c) { const float T = FMath::Lerp(-Seat * 0.8f, Seat * 0.8f, c / 6.f); Row.Add(B.At(-Len * 0.5f + 4.f + r * 1.5f, T, SeatOff(-Len * 0.5f + 4.f, T) - 4.f + r * 3.f)); }
				Plate.Add(Row);
			}
			Panel(M, Plate, 0.8f, [&L](int32, int32) { return L.Metal; }, L.MetalRough, FVector(1.f, 0.f, 0.f));
			for (const float Side : { -1.f, 1.f })
			{
				for (int32 k = 0; k < 6; ++k)
				{
					const float X = FMath::Lerp(-Len * 0.42f, Len * 0.42f, k / 5.f);
					const FVector P = B.At(X, Side * (Skirt - 0.05f), SeatOff(X, Side * (Skirt - 0.05f)) + 0.2f);
					Stud(M, P, (P - FVector(X, 0.f, -B.Rb)).GetSafeNormal(), 1.1f, L.Metal);
				}
			}
		}
		Girth(M, B, Len * 0.12f, Skirt - 0.05f, L);
		Stirrup(M, B, -1.f, L);
		Stirrup(M, B, 1.f, L);
		// The breast collar: from the skirts' front corners round the front of the chest, a ring at the middle.
		const FVector LeftCorner = B.At(Len * 0.45f, -1.0f, 3.f), RightCorner = B.At(Len * 0.45f, 1.0f, 3.f);
		const FVector Front(B.Chest, 0.f, -B.Dz * 0.35f);
		M.Tube({ LeftCorner, FVector(FMath::Lerp(LeftCorner.X, Front.X, 0.6f), -B.W * 0.85f, FMath::Lerp(LeftCorner.Z, Front.Z, 0.6f)), Front + FVector(0.f, -6.f, 0.f), Front,
			Front + FVector(0.f, 6.f, 0.f), FVector(FMath::Lerp(RightCorner.X, Front.X, 0.6f), B.W * 0.85f, FMath::Lerp(RightCorner.Z, Front.Z, 0.6f)), RightCorner }, 2.2f, 6, L.Leather * 0.9f, 0.7f);
		TArray<FVector> Ring;
		for (int32 i = 0; i <= 10; ++i) Ring.Add(Front + FVector(1.5f, FMath::Cos(i * 0.628f) * 4.f, FMath::Sin(i * 0.628f) * 4.f));
		M.Tube(Ring, 0.9f, 5, L.Metal, L.MetalRough, true);
		return M;
	}

	/** The pack saddle: a sawbuck frame on side bars, two laced panniers, a bedroll, lash ropes, girth and breeching. */
	FWRToolMesh PackSaddle(const FBack& B, const FTackLook& L)
	{
		FWRToolMesh M;
		const FLinearColor Wood(0.32f, 0.2f, 0.1f), Rope(0.48f, 0.4f, 0.26f), Hide(0.34f, 0.22f, 0.12f), Wool(0.42f, 0.38f, 0.3f);
		const float Len = FMath::Clamp(B.W * 1.9f, 60.f, 140.f);
		Drape(M, B, -Len * 0.55f, Len * 0.55f, -1.2f, 1.2f, 9, 15, [](float, float) { return 0.6f; }, 1.6f,
			[&Wool](int32 r, int32 c) { return (r == 0 || r == 8 || c == 0 || c == 14) ? Wool * 0.5f : Wool; }, 0.95f);
		// Side bars: two boards along the back either side of the spine.
		for (const float Side : { -1.f, 1.f })
		{
			Drape(M, B, -Len * 0.48f, Len * 0.48f, Side * 0.38f, Side * 0.72f, 6, 4, [](float, float) { return 2.4f; }, 3.f, [&Wood](int32, int32) { return Wood; }, 0.8f);
		}
		// The bucks: an X of two bars at the front and the back, their tops crossing over the spine.
		const float Up = FMath::Clamp(B.W * 0.45f, 14.f, 30.f);
		for (const float X : { -Len * 0.32f, Len * 0.32f })
		{
			for (const float Side : { -1.f, 1.f })
			{
				const FVector Foot = B.At(X, Side * 0.6f, 5.f);
				M.Tube({ Foot, FVector(X, -Side * Up * 0.35f, Up), FVector(X, -Side * Up * 0.55f, Up * 1.3f) }, 2.6f, 6, Wood, 0.75f);
			}
			M.Tube({ FVector(X - 3.f, 0.f, Up * 0.95f), FVector(X + 3.f, 0.f, Up * 0.95f) }, 3.2f, 6, Rope, 0.9f);   // lashed where they cross
		}
		// The panniers: a laced hide box hanging down each flank from the bucks.
		const float PH = FMath::Clamp(B.Dz * 0.5f, 30.f, 80.f), PD = FMath::Clamp(B.W * 0.45f, 16.f, 34.f);
		for (const float Side : { -1.f, 1.f })
		{
			// It hangs straight down from where the bucks' feet meet the flank, standing a finger off the hide.
			// (Its inner face clear of the barrel's widest point, so it never sinks into the flank.)
			FVector Top = B.At(0.f, Side * 1.05f, 6.f);
			Top.Y = Side * (B.Rb + 4.f);
			const FVector Out(0.f, Side, 0.f);
			const FVector Dn(0.f, 0.f, -1.f);
			TArray<TArray<FVector>> Box;
			for (int32 r = 0; r < 4; ++r)
			{
				const float X = FMath::Lerp(-Len * 0.36f, Len * 0.36f, r / 3.f);
				TArray<FVector> Ring;
				const FVector T = FVector(X, Top.Y, Top.Z);
				Ring.Add(T);
				Ring.Add(T + Out * PD);
				Ring.Add(T + Out * PD * 1.1f + Dn * PH);
				Ring.Add(T + Dn * PH * 1.02f);
				Box.Add(Ring);
			}
			M.Loft(Box, Hide, 0.75f, true);
			// Lacing zig-zag along its top edge, a strap from each buck.
			TArray<FVector> Lace;
			for (int32 k = 0; k <= 12; ++k) Lace.Add(FVector(FMath::Lerp(-Len * 0.34f, Len * 0.34f, k / 12.f), Top.Y, Top.Z) + Out * (PD * (k % 2 ? 0.95f : 0.15f)) + FVector(0.f, 0.f, 0.8f));
			M.Tube(Lace, 0.6f, 4, Rope, 0.9f);
			for (const float X : { -Len * 0.32f, Len * 0.32f })
			{
				M.Tube({ FVector(X, -Side * Up * 0.35f, Up), FVector(X, Top.Y, Top.Z) + Out * PD * 0.5f + Dn * PH * 0.3f }, 1.3f, 5, L.Leather, 0.7f);
			}
		}
		// The bedroll, strapped across the top.
		M.Lathe(FVector(0.f, -B.W * 0.7f, Up * 0.8f + 10.f), FVector(0.f, 1.f, 0.f), B.W * 1.4f, { FVector(0.f, 9.f, 9.f), FVector(0.05f, 10.f, 10.f), FVector(0.95f, 10.f, 10.f), FVector(1.f, 9.f, 9.f) }, 12, Wool * 0.8f, 0.95f);
		for (const float Y : { -B.W * 0.4f, B.W * 0.4f })
		{
			TArray<FVector> Strap;
			for (int32 i = 0; i <= 10; ++i) Strap.Add(FVector(FMath::Cos(i * 0.628f) * 10.6f, Y, Up * 0.8f + 10.f + FMath::Sin(i * 0.628f) * 10.6f));
			M.Tube(Strap, 0.9f, 4, L.Leather, 0.7f, true);
		}
		Girth(M, B, Len * 0.1f, 1.15f, L);
		// The breeching: round the back of the rump so a load can't ride forward downhill.
		const FVector LB = B.At(-Len * 0.5f, -1.0f, 3.f), RB = B.At(-Len * 0.5f, 1.0f, 3.f);
		const FVector Rear(-B.Rump, 0.f, -B.Dz * 0.4f);
		M.Tube({ LB, FVector(FMath::Lerp(LB.X, Rear.X, 0.6f), -B.W * 0.9f, FMath::Lerp(LB.Z, Rear.Z, 0.6f)), Rear, FVector(FMath::Lerp(RB.X, Rear.X, 0.6f), B.W * 0.9f, FMath::Lerp(RB.Z, Rear.Z, 0.6f)), RB }, 2.f, 6, L.Leather * 0.85f, 0.7f);
		return M;
	}

	/** A plate: a curved sheet with a rolled rim and rivets (barding), on a frame (origin, X along it, Y across, Z out). */
	void Plate(FWRToolMesh& M, const FTransform& Frame, float Length, float Width0, float Width1, float Arch, const FTackLook& L, bool bRidge)
	{
		TArray<TArray<FVector>> Grid;
		const int32 R = 6, C = 9;
		for (int32 r = 0; r < R; ++r)
		{
			TArray<FVector> Row;
			const float U = r / float(R - 1);
			const float W = FMath::Lerp(Width0, Width1, U);
			for (int32 c = 0; c < C; ++c)
			{
				const float V = c / float(C - 1) * 2.f - 1.f;
				const float Z = Arch * (1.f - V * V) + (bRidge ? 1.5f * FMath::Max(0.f, 1.f - FMath::Abs(V) * 4.f) : 0.f) + ((c == 0 || c == C - 1 || r == 0 || r == R - 1) ? 0.9f : 0.f);
				Row.Add(Frame.TransformPosition(FVector(U * Length, V * W, Z)));
			}
			Grid.Add(Row);
		}
		Panel(M, Grid, 0.9f, [&L](int32 r, int32 c) { return (L.Band.R + L.Band.G + L.Band.B > 0.f && (c == 1 || c == 7)) ? L.Band : L.Metal; }, L.MetalRough, Frame.TransformVector(FVector(0.f, 0.f, 1.f)));
		for (const float V : { -0.82f, 0.82f })
		{
			for (int32 k = 0; k < 3; ++k)
			{
				const float U = 0.15f + 0.35f * k;
				const float W = FMath::Lerp(Width0, Width1, U);
				Stud(M, Frame.TransformPosition(FVector(U * Length, V * W, Arch * (1.f - V * V) + 0.9f)), Frame.TransformVector(FVector(0.f, 0.f, 1.f)), 0.9f, L.Metal * 0.8f);
			}
		}
	}

	/** Barding round the barrel: the peytral over the chest and a flanchard down each flank behind the saddle. */
	FWRToolMesh BardingBody(const FBack& B, const FTackLook& L)
	{
		FWRToolMesh M;
		// Peytral: a plate curving round the front of the chest, a padded hide backing under it.
		const float H = FMath::Clamp(B.Dz * 0.55f, 25.f, 110.f);
		TArray<TArray<FVector>> Peytral, Pad;
		for (int32 r = 0; r < 6; ++r)
		{
			TArray<FVector> Row, PRow;
			const float Z = -B.Dz * 0.1f - H * r / 5.f;
			for (int32 c = 0; c < 11; ++c)
			{
				const float A = FMath::Lerp(-1.25f, 1.25f, c / 10.f);
				const float Rad = B.W * 1.12f;
				Row.Add(FVector(B.Chest - Rad + Rad * FMath::Cos(A) + 2.f, Rad * FMath::Sin(A), Z));
				PRow.Add(FVector(B.Chest - Rad + (Rad - 1.5f) * FMath::Cos(A) + 2.f, (Rad - 1.5f) * FMath::Sin(A), Z - 2.f));
			}
			Peytral.Add(Row);
			Pad.Add(PRow);
		}
		Panel(M, Pad, 1.5f, [](int32, int32) { return FLinearColor(0.22f, 0.13f, 0.06f); }, 0.85f, FVector(1.f, 0.f, 0.f));
		Panel(M, Peytral, 1.f, [&L](int32 r, int32 c) { return (L.Band.R + L.Band.G + L.Band.B > 0.f && (r == 0 || r == 5)) ? L.Band : L.Metal; }, L.MetalRough, FVector(1.f, 0.f, 0.f));
		// Flanchards: plates down each flank behind the seat, a strap over the top joining them.
		const float Len = FMath::Clamp(B.W * 1.5f, 42.f, 80.f);
		for (const float Side : { -1.f, 1.f })
		{
			TArray<TArray<FVector>> Fl;
			for (int32 r = 0; r < 6; ++r)
			{
				TArray<FVector> Row;
				const float X = FMath::Lerp(-Len * 0.55f, -Len * 0.55f - FMath::Clamp(B.Rump * 0.55f, 30.f, 110.f), r / 5.f);
				for (int32 c = 0; c < 7; ++c) Row.Add(B.At(X, Side * FMath::Lerp(0.85f, 1.65f, c / 6.f), 4.f + (c == 0 || c == 6 ? 0.8f : 0.f)));
				Fl.Add(Row);
			}
			Panel(M, Fl, 1.f, [&L](int32 r, int32 c) { return (L.Band.R + L.Band.G + L.Band.B > 0.f && c == 6) ? L.Band : L.Metal; }, L.MetalRough, FVector::ZeroVector);
		}
		TArray<FVector> Strap;
		for (int32 i = 0; i <= 8; ++i) Strap.Add(B.At(-Len * 0.6f, FMath::Lerp(-0.9f, 0.9f, i / 8.f), 3.5f));
		M.Tube(Strap, 2.f, 5, L.Leather, 0.7f);
		return M;
	}

	UMaterialInterface* MorphMaterial()
	{
		static TWeakObjectPtr<UMaterialInterface> Cached;
		if (!Cached.IsValid()) Cached = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Creatures/M_TameMorph.M_TameMorph"));
		return Cached.Get();
	}
}
using namespace WRTameLookDetail;

void AWRCreature::RefreshTameLook()
{
	const FWRCreatureDef* Def = GetDef();
	if (!Def || GetNetMode() == NM_DedicatedServer) return;

	// A body built in code (the Floeback).
	if (Def->ProcBody == TEXT("seal") && !SealBody)
	{
		SealBody = NewObject<UWRSealBody>(this, TEXT("SealBody"));
		SealBody->SetupAttachment(GetRootComponent());
		SealBody->RegisterComponent();
		SealBody->Init(this);
		BodyMesh->SetVisibility(false);
		FaceMarker->SetVisibility(false);
	}
	USceneComponent* BodyComp = SealBody ? static_cast<USceneComponent*>(SealBody) : (Skel && Skel->GetSkeletalMeshAsset() ? static_cast<USceneComponent*>(Skel) : nullptr);

	// A baby's size (the actor's scale doesn't replicate: every machine sets it from its growth).
	if (IsTamed()) SetActorScale3D(FVector(FMath::Clamp(TameRep.Growth, 0.05f, 1.f)));

	// Tack: rebuilt only when what's fitted changes.
	const uint32 Key = HashCombine(HashCombine(GetTypeHash(TameRep.Saddle.ItemId), GetTypeHash(TameRep.Barding.ItemId)), GetTypeHash(BodyComp != nullptr));
	if (Key != TackLookKey)
	{
		TackLookKey = Key;
		for (UProceduralMeshComponent* Old : { SaddleMesh.Get(), BardingMesh.Get() }) if (Old) Old->DestroyComponent();
		for (UProceduralMeshComponent* Old : BardingPlates) if (Old) Old->DestroyComponent();
		SaddleMesh = nullptr;
		BardingMesh = nullptr;
		BardingPlates.Reset();
		const float R = Def->Radius, H = Def->HalfHeight;
		FBack Back;
		Back.W = FMath::Max(12.f, Def->Back * R);
		Back.Rb = Back.W * 1.2f;
		Back.Dz = FMath::Max(25.f, (Def->SeatUp + 0.25f) * H);
		Back.Chest = FMath::Max(Back.W, R * 0.95f - Def->SeatFwd * R);
		Back.Rump = FMath::Max(Back.W, R * 0.95f + Def->SeatFwd * R);
		const FName Bone = SealBody ? NAME_None : FindBone(TEXT("spine2"));
		const auto Fit = [this, BodyComp, Bone](UProceduralMeshComponent* Comp, const FVector& WorldAt, const FRotator& WorldRot)
		{
			Comp->RegisterComponent();
			if (BodyComp) Comp->AttachToComponent(BodyComp, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, false), Bone);
			else Comp->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			Comp->SetWorldScale3D(GetActorScale3D());
			Comp->SetWorldLocationAndRotation(WorldAt, WorldRot);
		};
		if (!TameRep.Saddle.IsEmpty())
		{
			const FWRTackDef* Tk = FWRTaming::Tack(TameRep.Saddle.ItemId);
			const FTackLook L = LookOf(TameRep.Saddle.ItemId);
			SaddleMesh = NewObject<UProceduralMeshComponent>(this);
			(Tk && Tk->Kind == TEXT("pack") ? PackSaddle(Back, L) : RidingSaddle(Back, L)).ApplyTo(SaddleMesh);
			Fit(SaddleMesh, SeatLocation(), GetActorRotation());
			if (SealBody) SealBody->CarrySaddle(SaddleMesh);
		}
		if (!TameRep.Barding.IsEmpty())
		{
			const FTackLook L = LookOf(TameRep.Barding.ItemId);
			BardingMesh = NewObject<UProceduralMeshComponent>(this);
			BardingBody(Back, L).ApplyTo(BardingMesh);
			Fit(BardingMesh, SeatLocation(), GetActorRotation());
			if (SealBody) SealBody->CarrySaddle(BardingMesh);
			// The neck lames and the face plate ride the neck and head bones where the skeleton has them.
			const FName NeckBone = FindBone(TEXT("neck")), HeadBone = FindBone(TEXT("head"));
			if (Skel && !SealBody && !NeckBone.IsNone() && !HeadBone.IsNone())
			{
				const FVector Neck = Skel->GetSocketLocation(NeckBone), Head = Skel->GetSocketLocation(HeadBone);
				const FVector Along = (Head - Neck).GetSafeNormal();
				const float NeckLen = FVector::Dist(Head, Neck);
				const float Rn = FMath::Clamp(Back.W * 0.5f, 8.f, 60.f);
				for (int32 k = 0; k < 3; ++k)
				{
					// Crinet: three overlapping arched lames over the top of the neck.
					FWRToolMesh Lame;
					// Up across the neck: world up with its part along the neck taken out.
					const FVector Up = (FVector::UpVector - Along * FVector::DotProduct(FVector::UpVector, Along)).GetSafeNormal();
					const FTransform Frame(FRotationMatrix::MakeFromXZ(Along, Up.IsNearlyZero() ? FVector::UpVector : Up).Rotator(), FVector::ZeroVector);
					Plate(Lame, Frame, NeckLen * 0.36f, Rn, Rn * 0.9f, Rn * 0.45f, L, false);
					UProceduralMeshComponent* P = NewObject<UProceduralMeshComponent>(this);
					Lame.ApplyTo(P);
					P->RegisterComponent();
					P->AttachToComponent(Skel, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, false), NeckBone);
					P->SetWorldScale3D(FVector(1.f));
					P->SetWorldLocationAndRotation(Neck + Along * NeckLen * (0.05f + 0.3f * k) + (Up.IsNearlyZero() ? FVector::UpVector : Up) * Rn * 0.55f, FRotator::ZeroRotator);
					BardingPlates.Add(P);
				}
				// Chanfron: the face plate down the front of the head, a ridge down its middle.
				FWRToolMesh Face;
				const FVector Fwd = Skel->GetSocketRotation(HeadBone).Vector();
				const FVector HeadAlong = FVector::DotProduct(Fwd, Along) > 0.3f ? Fwd : Along;
				const FTransform HF(FRotationMatrix::MakeFromXZ(HeadAlong, FVector::UpVector).Rotator(), FVector::ZeroVector);
				Plate(Face, HF, Rn * 2.4f, Rn * 0.75f, Rn * 0.4f, Rn * 0.35f, L, true);
				UProceduralMeshComponent* P = NewObject<UProceduralMeshComponent>(this);
				Face.ApplyTo(P);
				P->RegisterComponent();
				P->AttachToComponent(Skel, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, false), HeadBone);
				P->SetWorldScale3D(FVector(1.f));
				P->SetWorldLocationAndRotation(Head + FVector::UpVector * Rn * 0.6f - HeadAlong * Rn * 0.3f, FRotator::ZeroRotator);
				BardingPlates.Add(P);
			}
		}
	}

	// Mutations: one overlay over its own texture, every morph it carries mixed in (pale, dark, bands, a glow line).
	UMeshComponent* Body = SealBody ? static_cast<UMeshComponent*>(SealBody) : (Skel && Skel->GetSkeletalMeshAsset() ? static_cast<UMeshComponent*>(Skel) : nullptr);
	if (Body && TameRep.MorphKinds.Num() > 0 && MorphMaterial())
	{
		if (!MorphMat)
		{
			MorphMat = UMaterialInstanceDynamic::Create(MorphMaterial(), this);
			Body->SetOverlayMaterial(MorphMat);
		}
		float Amounts[5] = { 0.f, 0.f, 0.f, 0.f, 0.f };
		FLinearColor Tint = FLinearColor::White;
		for (int32 i = 0; i < TameRep.MorphKinds.Num(); ++i)
		{
			const uint8 K = TameRep.MorphKinds[i];
			if (K > 0 && K < 5) Amounts[K] = 1.f;
			if (TameRep.MorphColors.IsValidIndex(i)) Tint = FLinearColor(TameRep.MorphColors[i]);
		}
		MorphMat->SetVectorParameterValue(TEXT("Tint"), Tint);
		MorphMat->SetScalarParameterValue(TEXT("Pale"), Amounts[WRTameMorph::Pale]);
		MorphMat->SetScalarParameterValue(TEXT("Dark"), Amounts[WRTameMorph::Dark]);
		MorphMat->SetScalarParameterValue(TEXT("Bands"), Amounts[WRTameMorph::Bands]);
		MorphMat->SetScalarParameterValue(TEXT("GlowLine"), Amounts[WRTameMorph::GlowLine]);
	}
	else if (Body && MorphMat && TameRep.MorphKinds.Num() == 0)
	{
		Body->SetOverlayMaterial(nullptr);
		MorphMat = nullptr;
	}
}

// ---------------------------------------------------------------- the incubator's egg and warmth

void FWRTameLook::RefreshPiece(AWRBuildPiece* Piece)
{
	const FWRPieceDef* Def = Piece ? Piece->GetDef() : nullptr;
	if (!Def || Def->Kind != TEXT("incubator") || Piece->bGhost) return;
	UProceduralMeshComponent* Egg = nullptr;
	UPointLightComponent* Warm = nullptr;
	for (UActorComponent* C : Piece->GetComponents())
	{
		if (C->GetFName() == TEXT("IncubatorEgg")) Egg = Cast<UProceduralMeshComponent>(C);
		if (C->GetFName() == TEXT("IncubatorWarm")) Warm = Cast<UPointLightComponent>(C);
	}
	if (!Egg)
	{
		Egg = NewObject<UProceduralMeshComponent>(Piece, TEXT("IncubatorEgg"));
		Egg->SetupAttachment(Piece->GetRootComponent());
		Egg->RegisterComponent();
		// On the straw of the tray, under the hood (WRBuildLookDecor's incubator: the tray's straw at 78 cm).
		Egg->SetRelativeLocation(FVector(-4.f, 0.f, 77.f));
	}
	if (!Warm)
	{
		Warm = NewObject<UPointLightComponent>(Piece, TEXT("IncubatorWarm"));
		Warm->SetupAttachment(Piece->GetRootComponent());
		Warm->RegisterComponent();
		// In the firebox's mouth: it lights the coals and the clay from inside, and the egg above through the grate.
		Warm->SetRelativeLocation(FVector(22.f, 0.f, 26.f));
		Warm->SetAttenuationRadius(260.f);
		Warm->SetCastShadows(false);
	}
	const FWRItemStack* Stack = Piece->Storage.Num() > 0 && !Piece->Storage[0].IsEmpty() ? &Piece->Storage[0] : nullptr;
	const bool bWarm = (Piece->FarmFlags & WRFarmFlag::Working) != 0;
	Warm->SetVisibility(bWarm);
	Warm->SetIntensity(bWarm ? 1600.f : 0.f);
	Warm->SetLightColor(FLinearColor(1.f, 0.42f, 0.12f));
	if (!Stack)
	{
		Egg->ClearAllMeshSections();
		return;
	}
	// The egg's shell: its species' colour, lightened and speckled; a frost egg rimed, a fire egg mottled dark.
	FName Species;
	FWRTameGenes::FromSockets(Stack->Sockets, &Species);
	if (Species.IsNone()) Species = FName(*Stack->ItemId.ToString().Replace(TEXT("fertile_"), TEXT("")).Replace(TEXT("egg_"), TEXT("")));
	const FWRCreatureDef* CDef = FWRCreatureRegistry::Get().Find(Species);
	const FLinearColor Shell = FMath::Lerp(CDef ? CDef->Color : FLinearColor(0.7f, 0.65f, 0.55f), FLinearColor(0.85f, 0.8f, 0.7f), 0.5f);
	FWRToolMesh M;
	TArray<FVector> Profile;
	for (int32 i = 0; i <= 12; ++i)
	{
		const float t = i / 12.f;
		const float Rr = FMath::Sqrt(FMath::Max(0.f, FMath::Sin(t * PI))) * (10.f - 2.4f * t);
		Profile.Add(FVector(t, Rr, Rr));
	}
	// Stood broad end down in the straw, the way eggs are set.
	M.Lathe(FVector(0.f, 0.f, 0.f), FVector(0.f, 0.f, 1.f), 26.f, Profile, 18, Shell, 0.35f);
	for (int32 k = 0; k < 12; ++k)
	{
		const float t = 0.12f + 0.065f * k, A = k * 2.3f;
		const float Rr = FMath::Sqrt(FMath::Max(0.f, FMath::Sin(t * PI))) * (10.f - 2.4f * t);
		const FVector P(FMath::Cos(A) * Rr, FMath::Sin(A) * Rr, 26.f * t);
		M.Tube({ P, P + FVector(0.f, 0.f, 0.8f) }, 1.f, 5, CDef && CDef->Element == TEXT("frost") ? FLinearColor(0.85f, 0.92f, 1.f) : Shell * 0.5f, 0.45f);
	}
	M.ApplyTo(Egg);
}
