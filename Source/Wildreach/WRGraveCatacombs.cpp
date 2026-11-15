// The catacombs under the grave fields (cooks/biome-graveyard.md 2.5; build-biome-graveyard.md 2.7): the Mourners' old
// limestone quarry, its worked-out galleries stacked with the bones carried down when the surface graves filled. Three
// levels: L1 the Crypt Gates (family vaults, niche walls, the Watchers' Vault), L2 the Ossuary (galleries walled with
// skulls, bone chapels, a sunken river), L3 the Old Quarry and Cisterns (pick-marked halls, the Brood Gallery, the
// Starfall Pit, the three flooded cisterns). Every room has one idea. Built from one stone kit on every machine:
// flagstone floors, coursed limestone walls with door gaps where the galleries come in, barrel vaults ribbed every 2 m,
// candles in wall niches (the only light the Mourners left; the world lights them always: it's always night below).
// Data/graveyard.json "catacombs" (rooms and links) is the layout; the generator kept every ceiling under chalk.

#include "WRGraveWorld.h"
#include "WRGraveLook.h"
#include "WRGraveTypes.h"
#include "Components/BoxComponent.h"

using namespace WRGraveLook;

namespace WRGraveCatacombsImpl
{
	const float WallT = 60.f;

	/** The door gaps in a room's walls: where each link that ends in it leaves its box. Local to the room (+X along W).
	 *  Wall 0 +X, 1 -X, 2 +Y, 3 -Y; for a round room, Pos is the angle (degrees). */
	struct FGap { int32 Wall = 0; float Pos = 0.f, Half = 160.f; float Floor = 0.f; };

	TArray<FGap> Gaps(const FWRGraveData::FRoom& R)
	{
		TArray<FGap> Out;
		const FWRGraveData& D = FWRGraveData::Get();
		const FTransform T(FRotator(0.f, R.Yaw, 0.f), R.At);
		const float HX = R.W * 0.5f, HY = R.bRound ? R.W * 0.5f : R.L * 0.5f;
		for (const FWRGraveData::FLink& L : D.Links)
		{
			if (L.Points.Num() < 2) continue;
			// Which end is in this room (kit stubs end at the room's middle; corridors at both rooms' middles).
			for (int32 End = 0; End < 2; ++End)
			{
				const FVector In = End == 0 ? L.Points[0] : L.Points.Last();
				const FVector LocIn = T.InverseTransformPosition(In);
				const bool bInside = R.bRound ? FVector2D(LocIn.X, LocIn.Y).Size() < HX + 10.f : FMath::Abs(LocIn.X) < HX + 10.f && FMath::Abs(LocIn.Y) < HY + 10.f;
				if (!bInside || FMath::Abs(LocIn.Z) > 600.f) continue;
				// Walk outward to the first point outside, then the crossing.
				for (int32 k = 1; k < L.Points.Num(); ++k)
				{
					const FVector P = T.InverseTransformPosition(End == 0 ? L.Points[k] : L.Points[L.Points.Num() - 1 - k]);
					const bool bOut = R.bRound ? FVector2D(P.X, P.Y).Size() > HX : FMath::Abs(P.X) > HX || FMath::Abs(P.Y) > HY;
					if (!bOut) continue;
					FGap G;
					G.Half = L.Width * 0.5f + 30.f;
					G.Floor = P.Z;
					if (R.bRound) { G.Wall = 0; G.Pos = FMath::RadiansToDegrees(FMath::Atan2(P.Y, P.X)); }
					else
					{
						const float Tx = FMath::Abs(P.X) - HX, Ty = FMath::Abs(P.Y) - HY;
						if (Tx > Ty) { G.Wall = P.X > 0.f ? 0 : 1; G.Pos = FMath::Clamp(P.Y, -HY + G.Half, HY - G.Half); }
						else { G.Wall = P.Y > 0.f ? 2 : 3; G.Pos = FMath::Clamp(P.X, -HX + G.Half, HX - G.Half); }
					}
					Out.Add(G);
					break;
				}
			}
		}
		return Out;
	}

	/** Coursed limestone wall from A to B (local floor points), H tall, its gaps (positions along it from A, half widths)
	 *  left open with a lintel over each; collision on the solid parts. Faces Inward (+1: the room's inside on the left). */
	void WallRun(FBuild& B, const FVector& A, const FVector& E, float H, TArray<TPair<float, float>> GapsAlong, int32 Salt, bool bQuarry = false)
	{
		const FVector D = E - A;
		const float Len = D.Size2D();
		if (Len < 10.f) return;
		const FVector U = FVector(D.X, D.Y, 0.f) / Len;
		const FRotator R(0.f, FMath::RadiansToDegrees(FMath::Atan2(U.Y, U.X)), 0.f);
		GapsAlong.Sort([](const TPair<float, float>& X, const TPair<float, float>& Y) { return X.Key < Y.Key; });
		float From = 0.f;
		const auto Solid = [&](float S0, float S1)
		{
			if (S1 - S0 < 5.f) return;
			const FVector Mid = A + U * ((S0 + S1) * 0.5f);
			// Courses of 60 cm with staggered joints (blocks 80-140 cm long); quarry walls are one face with pick marks.
			if (bQuarry)
			{
				B.K.Add(Limestone(0.8f + 0.1f * B.Rand(Salt)), FVector(S1 - S0, WallT, H), Mid + FVector(0, 0, H * 0.5f), R);
				for (float S = S0 + 40.f; S < S1; S += 70.f) B.K.Add(Limestone(0.65f), FVector(3.f, 2.f, 40.f + 30.f * B.Rand(Salt + int32(S))), A + U * S + FVector(0, 0, 60.f + (H - 120.f) * B.Rand(Salt + int32(S) + 3)) - R.RotateVector(FVector(0.f, WallT * 0.5f + 1.f, 0.f)), R + FRotator(0.f, 0.f, 20.f));
			}
			else
			{
				int32 Course = 0;
				for (float Z = 0.f; Z < H - 1.f; Z += 60.f, ++Course)
				{
					const float Ch = FMath::Min(60.f, H - Z);
					float S = S0 + (Course % 2) * 40.f;
					if (Course % 2) B.K.Add(Limestone(0.75f + 0.15f * B.Rand(Salt + Course)), FVector(40.f, WallT, Ch - 2.f), A + U * (S0 + 20.f) + FVector(0, 0, Z + Ch * 0.5f), R);
					while (S < S1 - 1.f)
					{
						const float L = FMath::Min(S1 - S, 80.f + 60.f * B.Rand(Salt + int32(S) + Course * 97));
						B.K.Add(Limestone(0.72f + 0.2f * B.Rand(Salt + int32(S * 3) + Course)), FVector(L - 2.f, WallT, Ch - 2.f), A + U * (S + L * 0.5f) + FVector(0, 0, Z + Ch * 0.5f), R);
						S += L;
					}
				}
			}
			B.Box(Mid + FVector(0, 0, H * 0.5f), FVector((S1 - S0) * 0.5f, WallT * 0.5f, H * 0.5f), R);
		};
		for (const TPair<float, float>& G : GapsAlong)
		{
			Solid(From, G.Key - G.Value);
			// The lintel over the doorway (doors are 2.6 m high), the wall above it.
			const FVector Mid = A + U * G.Key;
			if (H > 280.f)
			{
				B.K.Add(Limestone(0.9f), FVector(G.Value * 2.f + 40.f, WallT + 10.f, 30.f), Mid + FVector(0, 0, 275.f), R);
				B.K.Add(Limestone(0.75f), FVector(G.Value * 2.f, WallT, H - 290.f), Mid + FVector(0, 0, 290.f + (H - 290.f) * 0.5f), R);
				B.Box(Mid + FVector(0, 0, 290.f + (H - 290.f) * 0.5f), FVector(G.Value, WallT * 0.5f, (H - 290.f) * 0.5f), R);
			}
			From = G.Key + G.Value;
		}
		Solid(From, Len);
	}

	/** A floor of flags over a rectangle (or a disc), and its collision; a ceiling slab (collision) and the vault's ribs. */
	void FloorAndRoof(FBuild& B, float HX, float HY, float H, bool bRound, bool bVault, int32 Salt, bool bQuarry = false)
	{
		const float Step = 200.f;
		for (float X = -HX + Step * 0.5f; X < HX; X += Step)
		{
			for (float Y = -HY + Step * 0.5f; Y < HY; Y += Step)
			{
				if (bRound && FVector2D(X, Y).Size() > HX - 40.f) continue;
				B.K.Add(bQuarry ? Limestone(0.6f + 0.15f * B.Rand(Salt + int32(X + Y * 3))) : Limestone(0.7f + 0.18f * B.Rand(Salt + int32(X * 3 + Y))), FVector(Step - 6.f, Step - 6.f, 10.f), FVector(X, Y, -5.f), FRotator(0.f, bQuarry ? 0.f : 2.f * (B.Rand(Salt + int32(X - Y)) - 0.5f), 0.f));
			}
		}
		B.Box(FVector(0, 0, -30.f), FVector(HX + WallT, HY + WallT, 25.f));
		// The ceiling: a slab of chalk (dark, unlit side) and, under it, the vault.
		B.K.Add(Limestone(0.45f), FVector(HX * 2.f + WallT * 2.f, HY * 2.f + WallT * 2.f, 40.f), FVector(0, 0, H + 20.f));
		B.Box(FVector(0, 0, H + 30.f), FVector(HX + WallT, HY + WallT, 30.f));
		if (bVault)
		{
			// Transverse ribs every 2 m across the short span.
			const bool bAlongX = HX >= HY;
			const float Span = bAlongX ? HY : HX, Long = bAlongX ? HX : HY;
			for (float S = -Long + 100.f; S < Long; S += 200.f)
			{
				for (int32 v = 0; v < 7; ++v)
				{
					const float A0 = PI * v / 7.f, A1 = PI * (v + 1) / 7.f, Am = (A0 + A1) * 0.5f;
					const float Rise = FMath::Min(Span * 0.5f, 120.f);
					const FVector Pt = bAlongX ? FVector(S, -FMath::Cos(Am) * Span, H - Rise + FMath::Sin(Am) * Rise) : FVector(-FMath::Cos(Am) * Span, S, H - Rise + FMath::Sin(Am) * Rise);
					const float Chord = 2.f * Span * FMath::Sin((A1 - A0) * 0.5f);
					const FRotator Rr = bAlongX ? FRotator(0.f, 90.f, FMath::RadiansToDegrees(Am) - 90.f) : FRotator(0.f, 0.f, FMath::RadiansToDegrees(Am) - 90.f);
					B.K.Add(Limestone(0.8f), FVector(Chord, 26.f, 22.f), Pt, (FRotationMatrix::MakeFromXZ(bAlongX ? FVector(0.f, FMath::Sin(Am), FMath::Cos(Am) * Rise / Span) : FVector(FMath::Sin(Am), 0.f, FMath::Cos(Am) * Rise / Span), FVector::UpVector).Rotator()));
					(void)Rr;
				}
			}
		}
	}

	/** A candle in a wall niche (the Mourners lit the way): a small recess and a lit stub; a lamp now and then. */
	void NicheCandle(FBuild& B, const FVector& At, const FRotator& Facing, bool bLamp)
	{
		B.K.Add(Black(0.4f), FVector(18.f, 26.f, 30.f), At, Facing);
		B.K.Add(Wax(1.f), FVector(5.f, 5.f, 12.f), At + FVector(0, 0, -6.f), Facing, EWRLookMesh::Cylinder);
		B.K.Add(WRGraveLook::Candle(6.f), FVector(2.5f, 2.5f, 5.f), At + FVector(0, 0, 3.f), Facing, EWRLookMesh::Cone);
		// Wax run down the niche's sill.
		B.K.Add(Wax(0.9f), FVector(14.f, 10.f, 3.f), At + FVector(0, 0, -14.f), Facing);
		if (bLamp) B.Lamp(At + Facing.Vector() * 30.f, CandleC(), 700.f, 900.f);
	}

	/** A loculus niche wall: rows of 0.5 m niches along a wall run (A to B, inside face toward Inside), Rows high,
	 *  each with Fill: 0 empty, 1 bones, 2 a lantern lit, 3 a coffin end. */
	void Niches(FBuild& B, const FVector& A, const FVector& E, const FVector& Inside, int32 Rows, int32 Fill, int32 Salt, float Every = 120.f)
	{
		const float Len = float(FVector::Dist2D(A, E));
		const FVector U = (E - A).GetSafeNormal2D();
		const FRotator Face = Inside.Rotation();
		for (float S = 60.f; S < Len - 40.f; S += Every)
		{
			for (int32 r = 0; r < Rows; ++r)
			{
				const FVector C = A + U * S + Inside * 2.f + FVector(0, 0, 50.f + r * 62.f);
				B.K.Add(Black(0.35f), FVector(10.f, 90.f, 46.f), C, Face + FRotator(0.f, 90.f, 0.f));
				const int32 What = Fill == 1 ? (B.Rand(Salt + int32(S) + r) < 0.7f ? 1 : 0) : Fill;
				if (What == 1)
				{
					if (B.Rand(Salt + int32(S) * 3 + r) < 0.5f) Skull(B, C + Inside * 12.f + FVector(0, 0, -12.f), 0.8f, Face.Yaw, BoneC());
					else LongBone(B, C + Inside * 10.f + U * -30.f + FVector(0, 0, -16.f), C + Inside * 10.f + U * 30.f + FVector(0, 0, -16.f), 1.8f, BoneC());
				}
				else if (What == 2)
				{
					B.K.Add(Paper(0.9f), FVector(14.f, 14.f, 18.f), C + Inside * 12.f + FVector(0, 0, -12.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
					B.K.Add(GraveLight(3.f), FVector(15.f, 15.f, 18.f), C + Inside * 12.f + FVector(0, 0, -12.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
				}
				else if (What == 3) B.K.Add(OldWood(0.6f), FVector(6.f, 50.f, 38.f), C + Inside * 6.f, Face + FRotator(0.f, 90.f, 0.f));
			}
		}
	}

	/** A skull band (the ossuary's hagues): a course of skulls between two courses of long-bone ends, along a wall run. */
	void SkullBand(FBuild& B, const FVector& A, const FVector& E, const FVector& Inside, float Z, int32 Salt)
	{
		const float Len = float(FVector::Dist2D(A, E));
		const FVector U = (E - A).GetSafeNormal2D();
		const float Yaw = Inside.Rotation().Yaw;
		for (float S = 20.f; S < Len - 10.f; S += 22.f)
		{
			const FVector P = A + U * S + Inside * 6.f + FVector(0, 0, Z);
			Skull(B, P, 1.05f, Yaw, BoneC() * (0.85f + 0.2f * B.Rand(Salt + int32(S))));
			B.K.Disc(Bone(0.9f), P + FVector(0, 0, -22.f), Inside, 8.f, 6.f);
			B.K.Disc(Bone(0.9f), P + FVector(0, 0, 22.f), Inside, 8.f, 6.f);
		}
	}

	/** A spiral stair in a round shaft from Top down to Bottom (local Z): the Draught Stair, the Quarry Stair, the
	 *  chapel crypt's. Steps wind round a newel, 20 degrees and 30 cm a step, the shaft's wall with door gaps. */
	void Spiral(FBuild& B, float Top, float Bottom, float R, const TArray<FGap>& TopGaps, const TArray<FGap>& BottomGaps, int32 Salt)
	{
		const float Newel = 45.f;
		const int32 Steps = FMath::Max(4, FMath::CeilToInt((Top - Bottom) / 30.f));
		for (int32 s = 0; s < Steps; ++s)
		{
			const float Ang = 20.f * s;
			const float Z = Top - 30.f * (s + 1);
			const FVector Mid = FRotator(0.f, Ang, 0.f).Vector() * ((Newel + R) * 0.5f);
			B.K.Add(Limestone(0.75f + 0.15f * B.Rand(Salt + s)), FVector(R - Newel, 70.f + R * 0.18f, 30.f), Mid + FVector(0, 0, Z - 15.f), FRotator(0.f, Ang + 0.f, 0.f) + FRotator(0.f, 0.f, 0.f));
			B.Box(Mid + FVector(0, 0, Z - 15.f), FVector((R - Newel) * 0.5f, 35.f + R * 0.09f, 15.f), FRotator(0.f, Ang, 0.f));
		}
		B.K.Add(Limestone(0.85f), FVector(Newel * 2.f, Newel * 2.f, Top - Bottom + 300.f), FVector(0, 0, (Top + Bottom) * 0.5f + 150.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		B.Box(FVector(0, 0, (Top + Bottom) * 0.5f + 150.f), FVector(Newel, Newel, (Top - Bottom) * 0.5f + 150.f));
		// The shaft's wall: a ring of blocks in bands, open where the galleries come in at the top and the bottom.
		const int32 Ring = 18;
		for (float Z = Bottom; Z < Top + 300.f; Z += 300.f)
		{
			for (int32 k = 0; k < Ring; ++k)
			{
				const float Ang = 360.f * (k + 0.5f) / Ring;
				bool bOpen = false;
				for (const FGap& G : (Z < Bottom + 290.f ? BottomGaps : Z > Top - 10.f ? TopGaps : TArray<FGap>())) bOpen |= FMath::Abs(FMath::FindDeltaAngleDegrees(Ang, G.Pos)) < 18.f;
				if (bOpen) continue;
				const FVector Dir = FRotator(0.f, Ang, 0.f).Vector();
				const float Chord = 2.f * (R + WallT * 0.5f) * FMath::Sin(PI / Ring) + 4.f;
				B.K.Add(Limestone(0.7f + 0.15f * B.Rand(Salt + k + int32(Z))), FVector(WallT, Chord, 298.f), Dir * (R + WallT * 0.5f) + FVector(0, 0, Z + 150.f), FRotator(0.f, Ang, 0.f));
				B.Box(Dir * (R + WallT * 0.5f) + FVector(0, 0, Z + 150.f), FVector(WallT * 0.5f, Chord * 0.5f, 150.f), FRotator(0.f, Ang, 0.f));
				if (k % 6 == 0 && FMath::Fmod(Z - Bottom, 900.f) < 1.f) NicheCandle(B, Dir * (R - 4.f) + FVector(0, 0, Z + 160.f), FRotator(0.f, Ang + 180.f, 0.f), k == 0);
			}
		}
		B.K.Add(Limestone(0.45f), FVector(R * 2.f + 120.f, R * 2.f + 120.f, 40.f), FVector(0, 0, Top + 320.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		B.Box(FVector(0, 0, Top + 330.f), FVector(R + 60.f, R + 60.f, 30.f));
		B.Box(FVector(0, 0, Bottom - 30.f), FVector(R + 60.f, R + 60.f, 25.f));
	}
}

namespace WRGraveCatacombs
{
	using namespace WRGraveCatacombsImpl;

	void Room(FBuild& B, AWRGraveScenery* A)
	{
		const FWRGraveData& D = FWRGraveData::Get();
		if (!D.Rooms.IsValidIndex(A->Index)) return;
		const FWRGraveData::FRoom& R = D.Rooms[A->Index];
		const FName K = R.Kind;
		const int32 Salt = A->Index * 131;
		const TArray<FGap> G = Gaps(R);
		const bool bQuarry = R.Level == 3;
		if (R.bSpiral)
		{
			// The Draught Stair (C3, L1 to L2) and the Quarry Stair (Q1, L2 to L3): the gaps at the top and the bottom.
			TArray<FGap> Top, Bot;
			for (const FGap& Gp : G) (FMath::Abs(Gp.Floor) < 300.f ? Top : Bot).Add(Gp);
			// Links to the bottom level attach at the bottom point: find them by their floor height.
			Top.Reset(); Bot.Reset();
			for (const FWRGraveData::FLink& L : D.Links)
			{
				if (L.Points.Num() < 2) continue;
				const bool bA = L.A == R.Id, bB = L.B == R.Id;
				if (!bA && !bB) continue;
				const FVector In = bA ? L.Points[0] : L.Points.Last();
				const FVector Out = bA ? L.Points[FMath::Min(3, L.Points.Num() - 1)] : L.Points[FMath::Max(0, L.Points.Num() - 4)];
				FGap Gp;
				const FVector Lo = B.Local(Out);
				Gp.Pos = FMath::RadiansToDegrees(FMath::Atan2(Lo.Y, Lo.X));
				(FMath::Abs(B.Local(In).Z) < 200.f ? Top : Bot).Add(Gp);
			}
			Spiral(B, 0.f, R.Bottom - R.At.Z, FMath::Max(200.f, R.W * 0.5f - 40.f), Top, Bot, Salt);
			return;
		}
		const float HX = R.W * 0.5f, HY = R.bRound ? R.W * 0.5f : R.L * 0.5f, H = R.H;
		FloorAndRoof(B, HX, HY, H, R.bRound, !R.bRound && FMath::Min(HX, HY) < 700.f, Salt, bQuarry);
		// Walls with their door gaps.
		if (R.bRound)
		{
			const int32 N = FMath::Clamp(int32(R.W / 160.f), 16, 48);
			for (int32 k = 0; k < N; ++k)
			{
				const float Ang = 360.f * (k + 0.5f) / N;
				bool bOpen = false;
				for (const FGap& Gp : G) bOpen |= FMath::Abs(FMath::FindDeltaAngleDegrees(Ang, Gp.Pos)) < FMath::RadiansToDegrees(Gp.Half / HX) + 180.f / N;
				if (bOpen) continue;
				const FVector Dir = FRotator(0.f, Ang, 0.f).Vector();
				const float Chord = 2.f * (HX + WallT * 0.5f) * FMath::Sin(PI / N) + 4.f;
				B.K.Add(Limestone(0.72f + 0.15f * B.Rand(Salt + k)), FVector(WallT, Chord, H), Dir * (HX + WallT * 0.5f) + FVector(0, 0, H * 0.5f), FRotator(0.f, Ang, 0.f));
				B.Box(Dir * (HX + WallT * 0.5f) + FVector(0, 0, H * 0.5f), FVector(WallT * 0.5f, Chord * 0.5f, H * 0.5f), FRotator(0.f, Ang, 0.f));
			}
		}
		else
		{
			TArray<TPair<float, float>> W[4];
			for (const FGap& Gp : G)
			{
				// Wall runs go +Y for walls 0/1 (from -HY), +X for walls 2/3 (from -HX).
				W[Gp.Wall].Add({ (Gp.Wall < 2 ? Gp.Pos + HY : Gp.Pos + HX), Gp.Half });
			}
			const float O = WallT * 0.5f;
			WallRun(B, FVector(HX + O, -HY - WallT, 0.f), FVector(HX + O, HY + WallT, 0.f), H, W[0], Salt + 1, bQuarry);
			WallRun(B, FVector(-HX - O, -HY - WallT, 0.f), FVector(-HX - O, HY + WallT, 0.f), H, W[1], Salt + 2, bQuarry);
			WallRun(B, FVector(-HX - WallT, HY + O, 0.f), FVector(HX + WallT, HY + O, 0.f), H, W[2], Salt + 3, bQuarry);
			WallRun(B, FVector(-HX - WallT, -HY - O, 0.f), FVector(HX + WallT, -HY - O, 0.f), H, W[3], Salt + 4, bQuarry);
		}
		// Every room: candles in the corners (the dark beyond them is the hazard; a lantern is the key).
		if (!R.bRound) for (int32 c = 0; c < 4; ++c) NicheCandle(B, FVector((c % 2 ? 1.f : -1.f) * (HX - 30.f), (c < 2 ? -1.f : 1.f) * (HY - 30.f), 170.f), FRotator(0.f, 45.f + 90.f * c + 180.f, 0.f), c == 0 && K != TEXT("brood_gallery"));

		// ---- the room's one idea ----
		if (K == TEXT("watchers_vault"))
		{
			// C1, the G5 lesser shrine: a great iron door (standing open on the north wall's gallery), chairs set facing
			// it where the watchers sat, lanterns on a table, the spent ration crate (the cache). The shrine stands at the
			// room's middle (the shrine rows place it).
			for (int32 c = 0; c < 6; ++c)
			{
				const FVector Ch(-HX + 300.f + c * 160.f, -HY + 260.f, 0.f);
				B.K.Add(OldWood(0.7f), FVector(46.f, 46.f, 6.f), Ch + FVector(0, 0, 46.f));
				for (int32 l = 0; l < 4; ++l) B.K.Add(OldWood(0.65f), FVector(5.f, 5.f, 46.f), Ch + FVector(l % 2 ? 18.f : -18.f, l < 2 ? -18.f : 18.f, 23.f));
				B.K.Add(OldWood(0.7f), FVector(46.f, 5.f, 50.f), Ch + FVector(0.f, -20.f, 74.f));
			}
			B.K.Add(OldWood(0.65f), FVector(200.f, 90.f, 6.f), FVector(HX - 300.f, HY - 200.f, 80.f));
			for (int32 l = 0; l < 3; ++l) { const FVector Lp(HX - 360.f + l * 60.f, HY - 200.f, 98.f); B.K.Add(Black(0.6f), FVector(18.f, 18.f, 30.f), Lp); B.K.Add(WRGraveLook::Candle(3.f), FVector(12.f, 12.f, 18.f), Lp); }
			B.Lamp(FVector(HX - 300.f, HY - 200.f, 160.f), CandleC(), 1200.f, 1200.f);
			for (const float S : { -1.f, 1.f })
			{
				B.K.Add(Iron(), FVector(12.f, 160.f, 300.f), FVector(-HX + 60.f + S * 0.f, S * 220.f, 150.f), FRotator(0.f, S * 60.f, 0.f));
				for (int32 r = 0; r < 4; ++r) B.K.Add(Rust(), FVector(14.f, 162.f, 10.f), FVector(-HX + 60.f, S * 220.f, 40.f + r * 75.f), FRotator(0.f, S * 60.f, 0.f));
			}
		}
		else if (K == TEXT("niches_unnamed"))
		{
			// C2: a 200 m gallery of empty niches, each with a lantern and no name: the only lit corridor of L1.
			Niches(B, FVector(-HX, -HY + 2.f, 0.f), FVector(HX, -HY + 2.f, 0.f), FVector(0, 1, 0), 3, 2, Salt, 130.f);
			Niches(B, FVector(-HX, HY - 2.f, 0.f), FVector(HX, HY - 2.f, 0.f), FVector(0, -1, 0), 3, 2, Salt + 7, 130.f);
			for (float X = -HX + 1000.f; X < HX; X += 2500.f) B.Lamp(FVector(X, 0.f, 200.f), GraveLightC(), 900.f, 1600.f);
		}
		else if (K == TEXT("chapel_crypt"))
		{
			// C12: twelve coffins, open, each holding only a lantern (the cenotaphs again, under Nhal's floor).
			for (int32 c = 0; c < 12; ++c)
			{
				const FVector P(-HX + 250.f + (c % 6) * 330.f, (c < 6 ? -1.f : 1.f) * 300.f, 30.f);
				B.K.Add(Limestone(0.8f), FVector(220.f, 80.f, 30.f), P + FVector(0, 0, -15.f));
				Coffin(B, P + FVector(0, 0, 22.f), FRotator(0.f, 0.f, 0.f), true, true, OldWood(0.55f));
			}
			B.Lamp(FVector(0.f, 0.f, 300.f), GraveLightC(), 900.f, 1600.f);
		}
		else if (K == TEXT("family_vault"))
		{
			// A family's vault: coffin shelves (three high) on the long walls, a stone sarcophagus in the middle, the
			// family's grave goods (the cache by the end wall).
			Niches(B, FVector(-HX + 40.f, -HY + 2.f, 0.f), FVector(HX - 40.f, -HY + 2.f, 0.f), FVector(0, 1, 0), 3, 3, Salt, 160.f);
			Niches(B, FVector(-HX + 40.f, HY - 2.f, 0.f), FVector(HX - 40.f, HY - 2.f, 0.f), FVector(0, -1, 0), 3, 3, Salt + 5, 160.f);
			B.K.Add(GreyStone(0.85f), FVector(240.f, 100.f, 90.f), FVector(0, 0, 45.f));
			B.K.Add(GreyStone(0.95f), FVector(260.f, 116.f, 16.f), FVector(0, 0, 98.f));
			B.K.Add(Bone(0.9f), FVector(150.f, 50.f, 8.f), FVector(0, 0, 108.f));
			B.Box(FVector(0, 0, 50.f), FVector(130.f, 58.f, 50.f));
		}
		else if (K == TEXT("niche_corridor"))
		{
			Niches(B, FVector(-HX, -HY + 2.f, 0.f), FVector(HX, -HY + 2.f, 0.f), FVector(0, 1, 0), 4, 1, Salt);
			Niches(B, FVector(-HX, HY - 2.f, 0.f), FVector(HX, HY - 2.f, 0.f), FVector(0, -1, 0), 4, 1, Salt + 3);
		}
		else if (K == TEXT("wickbeetle_nest"))
		{
			// Wax crusting the walls and floor in drips and domes, the wax cells (the eggs you steal to tame one) in
			// hexagonal combs by the back wall, a soft yellow glow from the beetles' own lights.
			for (int32 k = 0; k < 40; ++k) B.Ball(FVector((B.Rand(Salt + k) - 0.5f) * R.W * 0.9f, (B.Rand(Salt + k + 50) - 0.5f) * HY * 1.8f, B.Rand(Salt + k + 90) < 0.5f ? 4.f : 40.f + 200.f * B.Rand(Salt + k + 7)), 10.f + 22.f * B.Rand(Salt + k + 3), Rgb(232, 221, 176) * 0.62f, 0.5f, 0.7f);
			for (int32 c = 0; c < 14; ++c) B.K.Hex(Wax(1.05f), -HX + 60.f, -HY + 100.f + c * 34.f, 20.f + (c % 3) * 34.f, 50.f + (c % 3) * 34.f, 16.f, 0.f);
			B.Lamp(FVector(0, 0, 80.f), FLinearColor(FColor(255, 200, 112)), 500.f, 700.f);
		}
		else if (K == TEXT("cryptstalker_lair"))
		{
			// A lair with no web: husks strewn (moulted shells), bones picked clean, long scratches in the chalk up the
			// walls and across the ceiling where it climbs.
			for (int32 k = 0; k < 6; ++k)
			{
				const FVector P((B.Rand(Salt + k) - 0.5f) * R.W * 0.8f, (B.Rand(Salt + k + 9) - 0.5f) * HY * 1.6f, 8.f);
				B.K.Add(FSurf::Glaze(Rgb(180, 175, 164) * 0.45f, 0.5f), FVector(140.f, 110.f, 30.f), P, FRotator(10.f, 60.f * k, 15.f), EWRLookMesh::Cylinder);
				for (int32 l = 0; l < 4; ++l) B.Limb(P + FVector(0, 0, 10.f), P + FRotator(0.f, 45.f + 90.f * l + 60.f * k, 0.f).Vector() * 160.f, 3.f, 1.5f, Rgb(90, 82, 72) * 0.5f, 5);
			}
			BonePile(B, FVector(HX * 0.4f, 0.f, 0.f), 80.f, Salt, 1);
			for (int32 s = 0; s < 12; ++s) B.K.Add(Black(0.5f), FVector(3.f, 260.f, 2.f), FVector((B.Rand(Salt + s) - 0.5f) * R.W, (B.Rand(Salt + s + 3) - 0.5f) * HY * 1.5f, H - 4.f), FRotator(0.f, 30.f * s, 0.f));
		}
		else if (K == TEXT("flooded_stair"))
		{
			// A stair going down into black water: the lower half of the room is flooded 80 cm deep.
			for (int32 s = 0; s < 5; ++s) B.K.Add(Limestone(0.75f), FVector(80.f, HY * 2.f - 40.f, 26.f), FVector(-HX + 80.f + s * 80.f, 0.f, 13.f - s * 0.f + (4 - s) * 0.f));
			WaterPond(A, FVector(HX * 0.25f, 0.f, 60.f), HX * 0.7f, HY - 20.f, 0.f, true);
		}
		else if (K == TEXT("collapsed_squeeze"))
		{
			// The gallery's roof came down: rubble fills it, a 1 m squeeze left along one wall.
			for (int32 k = 0; k < 18; ++k)
			{
				const FVector P((B.Rand(Salt + k) - 0.5f) * R.W * 0.8f, -HY + 160.f + B.Rand(Salt + k + 5) * (HY * 2.f - 260.f), 40.f + 100.f * B.Rand(Salt + k + 9));
				B.K.Add(Limestone(0.65f), FVector(90.f + 60.f * B.Rand(k), 80.f, 70.f + 50.f * B.Rand(k + 2)), P, FRotator(20.f * B.Rand(k + 3), 50.f * k, 15.f));
			}
			B.Box(FVector(0.f, 60.f, H * 0.4f), FVector(HX - 20.f, HY - 160.f, H * 0.4f));
		}
		else if (K == TEXT("grave_seam") || K == TEXT("silver_vein"))
		{
			// A seam: bones packed in the chalk of the far wall, crusted with soulstone (it grows on old bones in wet grave
			// soil), or a silver vein through the quarry's stone. The nodes are the server's.
			const bool bSilver = K == TEXT("silver_vein");
			for (int32 k = 0; k < 24; ++k)
			{
				const FVector P(HX - 10.f, (B.Rand(Salt + k) - 0.5f) * HY * 1.8f, 30.f + (H - 60.f) * B.Rand(Salt + k + 3));
				if (bSilver) B.K.Add(FSurf::Steel(FLinearColor(0.75f, 0.76f, 0.78f), 0.3f), FVector(6.f, 60.f + 80.f * B.Rand(k), 6.f), P, FRotator(0.f, 90.f, 50.f * (B.Rand(k + 5) - 0.5f)));
				else
				{
					LongBone(B, P + FVector(0, -20.f, 0), P + FVector(-6.f, 20.f, 4.f), 2.f, BoneC());
					B.K.Add(Soulstone(1.4f), FVector(7.f, 7.f, 16.f), P + FVector(-10.f, 0, 10.f), FRotator(-60.f, 0.f, 20.f * (B.Rand(k + 7) - 0.5f)), EWRLookMesh::Cone);
				}
			}
			if (!bSilver) B.Lamp(FVector(HX - 120.f, 0.f, 150.f), FLinearColor(FColor(159, 227, 192)), 400.f, 600.f);
		}
		else if (K == TEXT("skull_wall_puzzle") || K == TEXT("skull_wheel"))
		{
			// Skull walls: the back wall's face laid in skulls. The puzzle rooms show three patterns; C4 a twelve-spoked
			// wheel with one spoke missing (the twelve peoples: GY C4).
			for (float Y = -HY + 40.f; Y < HY - 30.f; Y += 26.f) for (float Z = 30.f; Z < H - 30.f; Z += 30.f)
			{
				bool bSkull = true;
				const FVector2D Q(Y, Z - H * 0.5f);
				if (K == TEXT("skull_wheel"))
				{
					const float Rr = Q.Size(), Ang = FMath::RadiansToDegrees(FMath::Atan2(Q.Y, Q.X));
					const int32 Spoke = FMath::RoundToInt(FMath::Fmod(Ang + 360.f, 360.f) / 30.f) % 12;
					const bool bOnSpoke = FMath::Abs(FMath::FindDeltaAngleDegrees(Ang, Spoke * 30.f)) < 7.f;
					bSkull = (Rr < 40.f) || (Rr > 180.f && Rr < 215.f) || (bOnSpoke && Rr < 200.f && Spoke != 7);
				}
				else bSkull = (int32(FMath::Abs(Y) / 26.f + Z / 30.f) % 3) != 0;
				if (bSkull) Skull(B, FVector(HX - 12.f, Y, Z), 1.f, 180.f, BoneC() * (0.85f + 0.15f * B.Rand(int32(Y * 3 + Z))));
				else B.K.Disc(Bone(0.85f), FVector(HX - 10.f, Y, Z), FVector(-1.f, 0.f, 0.f), 8.f, 8.f);
			}
		}
		else if (K == TEXT("chandelier_hall"))
		{
			// C5, the Ossuary Lord's hall (Sedlec): a round bone chapel 40 m across, its wall garlanded with bands of
			// skulls, festoons of bones from the vault, four bone pyramids at the edge (cover). The chandelier hangs on its
			// chain from the vault's crown: the boss's machine (WRGraveBosses), the pulley here.
			for (int32 Band = 0; Band < 3; ++Band)
			{
				const int32 N = 72;
				for (int32 k = 0; k < N; ++k)
				{
					const float A0 = 360.f * k / N, A1 = 360.f * (k + 1) / N;
					const FVector P0 = FRotator(0.f, A0, 0.f).Vector() * (HX - 20.f), P1 = FRotator(0.f, A1, 0.f).Vector() * (HX - 20.f);
					SkullBand(B, P0, P1, -FRotator(0.f, (A0 + A1) * 0.5f, 0.f).Vector(), 250.f + Band * 380.f, Salt + k + Band * 100);
				}
			}
			for (int32 f = 0; f < 16; ++f)
			{
				const float Ang = 22.5f * f;
				const FVector Wall = FRotator(0.f, Ang, 0.f).Vector() * (HX - 40.f) + FVector(0, 0, H - 200.f);
				const FVector Mid = FRotator(0.f, Ang, 0.f).Vector() * (HX * 0.55f) + FVector(0, 0, H - 400.f);
				for (int32 b = 0; b < 10; ++b) LongBone(B, FMath::Lerp(Wall, Mid, b / 10.f) - FVector(0, 0, 60.f * FMath::Sin(PI * b / 10.f)), FMath::Lerp(Wall, Mid, (b + 1) / 10.f) - FVector(0, 0, 60.f * FMath::Sin(PI * (b + 1) / 10.f)), 2.4f, BoneC());
			}
			for (int32 p = 0; p < 4; ++p)
			{
				const FVector C = FRotator(0.f, 45.f + 90.f * p, 0.f).Vector() * (HX - 500.f);
				for (int32 t = 0; t < 5; ++t)
				{
					const float S = 300.f - t * 55.f;
					for (int32 e = 0; e < 4; ++e) for (float u = -S * 0.5f; u <= S * 0.5f; u += 24.f)
					{
						const FVector Dir = FRotator(0.f, 90.f * e, 0.f).Vector(), Sd = FRotator(0.f, 90.f * e + 90.f, 0.f).Vector();
						Skull(B, C + Dir * S * 0.5f + Sd * u + FVector(0, 0, 14.f + t * 30.f), 1.f, 90.f * e, BoneC());
					}
				}
				B.Box(C + FVector(0, 0, 80.f), FVector(150.f, 150.f, 80.f));
			}
			// The vault: ribs from the wall to a crown boss, the pulley the chandelier's chain runs over.
			for (int32 r = 0; r < 12; ++r)
			{
				const FVector Wall = FRotator(0.f, 30.f * r, 0.f).Vector() * HX + FVector(0, 0, H - 300.f);
				for (int32 s = 0; s < 8; ++s)
				{
					const FVector P0 = FMath::Lerp(Wall, FVector(0, 0, H), s / 8.f) + FVector(0, 0, 200.f * FMath::Sin(PI * 0.5f * s / 8.f));
					const FVector P1 = FMath::Lerp(Wall, FVector(0, 0, H), (s + 1) / 8.f) + FVector(0, 0, 200.f * FMath::Sin(PI * 0.5f * (s + 1) / 8.f));
					B.K.Rod(Limestone(0.8f), P0, P1, 40.f, true);
				}
			}
			B.K.Disc(Iron(), FVector(0, 0, H + 120.f), FVector(1, 0, 0), 120.f, 20.f);
			for (int32 l = 0; l < 6; ++l) B.Lamp(FRotator(0.f, 60.f * l, 0.f).Vector() * (HX - 300.f) + FVector(0, 0, 400.f), CandleC(), 1500.f, 1800.f);
		}
		else if (K == TEXT("bone_river"))
		{
			// C6: an underground stream whose bed is all small bones washed white, soulstone glowing on the bottom. Wading is
			// safe; a Cryptstalker hunts the far bank.
			for (int32 k = 0; k < 160; ++k)
			{
				const FVector P((B.Rand(Salt + k) - 0.5f) * R.W * 0.95f, (B.Rand(Salt + k + 3) - 0.5f) * 360.f, 6.f);
				B.K.Add(Bone(0.95f + 0.15f * B.Rand(k)), FVector(14.f, 4.f, 3.f), P, FRotator(0.f, 360.f * B.Rand(Salt + k + 7), 0.f));
				if (k % 9 == 0) B.K.Add(Soulstone(2.f), FVector(6.f, 6.f, 10.f), P + FVector(0, 0, 4.f), FRotator::ZeroRotator, EWRLookMesh::Cone);
			}
			WaterPond(A, FVector(0, 0, 45.f), HX - 20.f, 200.f, 0.f, true);
			for (int32 l = 0; l < 3; ++l) B.Lamp(FVector(-HX + 1000.f + l * 2500.f, 0.f, 40.f), FLinearColor(FColor(159, 227, 192)), 600.f, 900.f);
		}
		else if (K == TEXT("sorting_room"))
		{
			// C7: stone tables with bones sorted by kind (skulls here, femurs there), a tally board, creature bones too,
			// shelved with the same care. The Ossuary ledger lies on the end table (its keepsake).
			for (int32 t = 0; t < 4; ++t)
			{
				const FVector C(-HX + 300.f + t * 380.f, 0.f, 0.f);
				B.K.Add(GreyStone(0.85f), FVector(260.f, 120.f, 12.f), C + FVector(0, 0, 90.f));
				for (const float S : { -1.f, 1.f }) B.K.Add(GreyStone(0.8f), FVector(30.f, 100.f, 84.f), C + FVector(S * 100.f, 0.f, 42.f));
				B.Box(C + FVector(0, 0, 50.f), FVector(130.f, 60.f, 50.f));
				for (int32 k = 0; k < 8; ++k)
				{
					const FVector P = C + FVector(-100.f + (k % 4) * 66.f, -30.f + (k / 4) * 60.f, 100.f);
					if (t == 0) Skull(B, P + FVector(0, 0, 8.f), 1.f, 0.f, BoneC());
					else if (t == 1) LongBone(B, P + FVector(-25.f, 0, 3.f), P + FVector(25.f, 0, 3.f), 2.2f, BoneC());
					else if (t == 2) Skull(B, P + FVector(0, 0, 6.f), 0.8f, 90.f, BoneC() * 0.9f);   // hounds' skulls
					else LongBone(B, P + FVector(-18.f, 0, 3.f), P + FVector(18.f, 0, 3.f), 1.4f, BoneC() * 0.9f);
				}
			}
			B.K.Add(OldWood(0.7f), FVector(6.f, 300.f, 180.f), FVector(-HX + 8.f, 0.f, 160.f));
			for (int32 m = 0; m < 30; ++m) B.K.Add(Black(0.6f), FVector(2.f, 1.f, 12.f), FVector(-HX + 12.f, -120.f + m * 8.f, 200.f + (m / 10) * 30.f));
			B.K.Add(FSurf::Cloth(Rgb(110, 70, 40) * 0.45f), FVector(30.f, 22.f, 6.f), FVector(-HX + 300.f + 3 * 380.f, 0.f, 99.f));
		}
		else if (K == TEXT("starfall_pit"))
		{
			// C8: the shaft from the surface sealed long ago (its mound above), at the bottom a black meteorite fused into
			// the quarry floor; star iron (the nodes) round it. The deepest point in the biome.
			for (float Z = H; Z < H + 2400.f; Z += 300.f)
			{
				for (int32 k = 0; k < 24; ++k)
				{
					const float Ang = 15.f * k;
					B.K.Add(Limestone(0.6f), FVector(WallT, 2.f * PI * HX / 24.f + 6.f, 298.f), FRotator(0.f, Ang, 0.f).Vector() * (HX + WallT * 0.5f) + FVector(0, 0, Z + 150.f), FRotator(0.f, Ang, 0.f));
				}
			}
			B.K.Add(Limestone(0.4f), FVector(HX * 2.f, HX * 2.f, 200.f), FVector(0, 0, H + 2500.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			B.Rock(3, FVector(420.f, 360.f, 220.f), FVector(0, 0, -60.f), FRotator(8.f, 30.f, 5.f), FLinearColor(0.02f, 0.02f, 0.025f), true, 40.f);
			for (int32 k = 0; k < 10; ++k) B.K.Add(FSurf::Steel(Rgb(150, 160, 190) * 0.6f, 0.2f), FVector(10.f, 10.f, 4.f), FVector((B.Rand(k) - 0.5f) * 300.f, (B.Rand(k + 4) - 0.5f) * 260.f, 120.f + 60.f * B.Rand(k + 8)));
			B.Lamp(FVector(0, 0, 500.f), FLinearColor(0.6f, 0.65f, 0.9f), 500.f, 1200.f);
		}
		else if (K == TEXT("brood_gallery"))
		{
			// C9, the Crypt Mother's gallery: a quarry hall 60 x 30 m, 8 m to the roof, six square pillars left standing, full
			// dark but for soulstone glowing in the floor's cracks (you bring the light). The egg sacs are the boss's.
			for (int32 p = 0; p < 6; ++p)
			{
				const FVector C(-HX + 1000.f + (p % 3) * 2000.f, (p < 3 ? -1.f : 1.f) * 750.f, 0.f);
				B.K.Add(Limestone(0.65f), FVector(300.f, 300.f, H), C + FVector(0, 0, H * 0.5f));
				for (float Z = 60.f; Z < H; Z += 90.f) B.K.Add(Limestone(0.55f), FVector(302.f, 3.f, 40.f), C + FVector(0.f, 151.f, Z), FRotator(0.f, 0.f, 20.f));
				B.Box(C + FVector(0, 0, H * 0.5f), FVector(150.f, 150.f, H * 0.5f));
			}
			for (int32 c = 0; c < 30; ++c)
			{
				const FVector P((B.Rand(Salt + c) - 0.5f) * R.W * 0.9f, (B.Rand(Salt + c + 3) - 0.5f) * HY * 1.8f, 1.f);
				B.K.Add(Soulstone(1.6f), FVector(200.f + 200.f * B.Rand(c), 4.f, 2.f), P, FRotator(0.f, 360.f * B.Rand(Salt + c + 5), 0.f));
			}
		}
		else if (K == TEXT("three_cisterns") || K == TEXT("cistern_bank"))
		{
			// C10: three flooded quarry halls joined by a sunken passage (swim 25 m), a real bronze bell on the bottom of the
			// third (S5: its mouth holds a dry air pocket). Ghost carp, lamprey and the Drowned Bell are fished here.
			const bool bThree = K == TEXT("three_cisterns");
			WaterPond(A, FVector(0, 0, 120.f), HX - 40.f, HY - (bThree ? 160.f : HY * 0.5f), 0.f, true);
			if (bThree)
			{
				for (const float X : { -HX / 3.f, HX / 3.f }) for (const float S : { -1.f, 1.f })
				{
					B.K.Add(Limestone(0.6f), FVector(120.f, HY - 220.f, H), FVector(X, S * (HY - (HY - 220.f) * 0.5f), H * 0.5f));
					B.Box(FVector(X, S * (HY - (HY - 220.f) * 0.5f), H * 0.5f), FVector(60.f, (HY - 220.f) * 0.5f, H * 0.5f));
				}
				// The bell, tilted on the bottom of the third at the east end, its mouth toward the room.
				const FVector Bell(HX - 600.f, 0.f, 60.f);
				B.K.Add(Bronze(0.9f), FVector(160.f, 160.f, 150.f), Bell, FRotator(-40.f, 0.f, 0.f), EWRLookMesh::Cone);
				B.K.Add(Verdigris(1.f), FVector(166.f, 166.f, 16.f), Bell + FRotator(-40.f, 0.f, 0.f).RotateVector(FVector(0, 0, -70.f)), FRotator(-40.f, 0.f, 0.f), EWRLookMesh::Cylinder);
			}
			// The banks: a walkway of flags along one side above the water.
			B.K.Add(Limestone(0.7f), FVector(R.W - 100.f, 140.f, 160.f), FVector(0.f, -HY + 70.f, 80.f));
			B.Box(FVector(0.f, -HY + 70.f, 80.f), FVector(R.W * 0.5f - 50.f, 70.f, 80.f));
		}
		else if (K == TEXT("quarry_face"))
		{
			// C11: half-cut limestone blocks still in the wall, tools dropped mid-swing, a lunch bundle turned to dust: work
			// stopped all at once.
			for (int32 b = 0; b < 6; ++b)
			{
				const FVector P(HX - 60.f, -HY + 200.f + b * ((HY * 2.f - 400.f) / 5.f), 60.f + (b % 2) * 120.f);
				B.K.Add(Limestone(0.85f), FVector(90.f + 40.f * (b % 3), 110.f, 110.f), P - FVector(30.f * (b % 3), 0.f, 0.f));
				B.K.Add(Black(0.4f), FVector(4.f, 112.f, 112.f), P - FVector(30.f * (b % 3) + 50.f, 0.f, 0.f));
			}
			B.K.Rod(OldWood(0.8f), FVector(HX - 260.f, 100.f, 4.f), FVector(HX - 330.f, 160.f, 4.f), 4.f);
			B.K.Add(Iron(), FVector(30.f, 8.f, 8.f), FVector(HX - 330.f, 160.f, 6.f), FRotator(0.f, 45.f, 0.f));
			for (int32 w = 0; w < 3; ++w) B.K.Spike(Iron(), FVector(HX - 300.f + w * 20.f, -100.f, 6.f), FVector(1.f, 0.f, 0.2f), 5.f, 18.f);
			B.K.Add(Sackcloth(0.7f), FVector(36.f, 30.f, 16.f), FVector(HX - 420.f, -260.f, 8.f));
			B.K.Add(FSurf::Cloth(Rgb(150, 140, 120) * 0.4f), FVector(60.f, 50.f, 2.f), FVector(HX - 420.f, -260.f, 1.f));
		}
		else if (K == TEXT("draught_corridor"))
		{
			// A draught corridor (GY 2.8): the candles here have all gone out, their wax blown into long drips one way.
			for (float X = -HX + 200.f; X < HX; X += 500.f) for (const float S : { -1.f, 1.f })
			{
				const FVector P(X, S * (HY - 12.f), 170.f);
				B.K.Add(Black(0.4f), FVector(18.f, 26.f, 30.f), P, FRotator(0.f, S > 0 ? -90.f : 90.f, 0.f));
				B.K.Add(Wax(0.9f), FVector(30.f, 5.f, 4.f), P + FVector(12.f, 0.f, -10.f), FRotator(0.f, 0.f, 0.f));
			}
		}
	}

	void Link(FBuild& B, AWRGraveScenery* A)
	{
		// A gallery between rooms (or a stub to a kit room): flags, coursed walls, a ribbed barrel vault, 2.8 m wide and
		// 3 m high; candles in niches every 16 m; the segments inside a room are the room's own. Slopes ramp; steeper
		// than 1 in 5 steps.
		const FWRGraveData& D = FWRGraveData::Get();
		if (!D.Links.IsValidIndex(A->Index)) return;
		const FWRGraveData::FLink& L = D.Links[A->Index];
		const FWRGraveData::FRoom* RA = D.FindRoom(L.A);
		const FWRGraveData::FRoom* RB = L.B.IsNone() ? nullptr : D.FindRoom(L.B);
		const float W = L.Width, H = L.Height;
		const int32 Salt = A->Index * 71;
		float Since = 0.f;
		// A stub runs from the main gallery's wall to its kit room's wall (the room is along the stub's line).
		TArray<FVector> Pts = L.Points;
		if (L.bStub && Pts.Num() >= 2 && RA)
		{
			const FVector Dir = (Pts.Last() - Pts[0]).GetSafeNormal2D();
			const float MainHalf = 140.f + 60.f;
			const float RoomHalf = RA->W * 0.5f;
			const FVector P0 = Pts[0] + Dir * MainHalf, P1 = Pts.Last() - Dir * RoomHalf;
			Pts = { P0, FVector(P1.X, P1.Y, P0.Z) };
			if (FVector::Dist2D(P0, P1) < 30.f || FVector::DotProduct(P1 - P0, Dir) <= 0.f) return;
			RA = nullptr;
		}
		// Where a stub leaves this gallery: its first point (on this gallery's line) opens the wall on its side.
		TArray<TPair<FVector, FVector>> Doors;   // where, which way out
		if (!L.bStub)
		{
			for (const FWRGraveData::FLink& S2 : D.Links)
			{
				if (!S2.bStub || S2.Points.Num() < 2) continue;
				const FVector St = S2.Points[0];
				for (const FVector& P : L.Points) if (FVector::Dist2D(P, St) < 250.f && FMath::Abs(P.Z - St.Z) < 200.f) { Doors.Add({ St, (S2.Points.Last() - St).GetSafeNormal2D() }); break; }
			}
		}
		for (int32 i = 0; i + 1 < Pts.Num(); ++i)
		{
			const FVector Wa = Pts[i], Wb = Pts[i + 1];
			const FVector Mw = (Wa + Wb) * 0.5f;
			// Skip what lies inside either room (its walls have the door).
			if ((RA && RA->Contains(Mw + FVector(0, 0, 50.f), -40.f)) || (RB && RB->Contains(Mw + FVector(0, 0, 50.f), -40.f))) continue;
			bool bInOther = false;
			// Stubs end in their kit room: the stub's far end lies in the room named by A.
			if (L.bStub && RA && RA->Contains(Mw + FVector(0, 0, 50.f), -40.f)) bInOther = true;
			if (bInOther) continue;
			const FVector Pa = B.Local(Wa), Pb = B.Local(Wb);
			const FVector Dl = Pb - Pa;
			const float Len = Dl.Size2D();
			if (Len < 1.f) continue;
			const float Rise = Dl.Z;
			const FRotator R(0.f, FMath::RadiansToDegrees(FMath::Atan2(Dl.Y, Dl.X)), 0.f);
			const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(Rise, Len));
			const FVector M = (Pa + Pb) * 0.5f;
			const FRotator Rp = R + FRotator(Pitch, 0.f, 0.f);
			const float SegL = FMath::Sqrt(Len * Len + Rise * Rise) + 8.f;
			if (FMath::Abs(Rise) / Len > 0.2f)
			{
				// Steps: 30 cm risers.
				const int32 N = FMath::CeilToInt(FMath::Abs(Rise) / 30.f);
				for (int32 s = 0; s < N; ++s)
				{
					const FVector P = FMath::Lerp(Pa, Pb, (s + 0.5f) / N);
					B.K.Add(Limestone(0.7f + 0.15f * B.Rand(Salt + i * 7 + s)), FVector(Len / N + 2.f, W, 30.f), FVector(P.X, P.Y, FMath::Lerp(Pa.Z, Pb.Z, (s + (Rise > 0 ? 1.f : 0.f)) / N) - 15.f), R);
				}
			}
			else B.K.Add(Limestone(0.68f + 0.18f * B.Rand(Salt + i)), FVector(SegL, W, 10.f), M + FVector(0, 0, -5.f), Rp);
			B.Box(M + FVector(0, 0, -25.f), FVector(SegL * 0.5f, W * 0.5f + 40.f, 22.f), Rp);
			for (const float S : { -1.f, 1.f })
			{
				// A kit room's door in this wall here: leave it open (a lintel over it).
				bool bDoor = false;
				const FVector SideW = (B.T.TransformVector(R.RotateVector(FVector(0.f, S, 0.f)))).GetSafeNormal2D();
				for (const TPair<FVector, FVector>& Dr : Doors) bDoor |= FVector::Dist2D(Dr.Key, Mw) < 220.f && FVector::DotProduct(Dr.Value, SideW) > 0.5f;
				if (bDoor)
				{
					B.K.Add(Limestone(0.85f), FVector(SegL, 60.f, 30.f), M + R.RotateVector(FVector(0.f, S * (W * 0.5f + 25.f), 0.f)) + FVector(0, 0, 275.f), Rp);
					continue;
				}
				const FVector Wc = M + R.RotateVector(FVector(0.f, S * (W * 0.5f + 25.f), 0.f));
				B.K.Add(Limestone(0.66f + 0.14f * B.Rand(Salt + i * 3 + int32(S))), FVector(SegL, 50.f, H * 0.62f), Wc + FVector(0, 0, H * 0.31f), Rp);
				B.K.Add(Limestone(0.6f + 0.14f * B.Rand(Salt + i * 5 + int32(S))), FVector(SegL, 50.f, H * 0.4f), Wc + FVector(0, 0, H * 0.62f + H * 0.2f), Rp);
				B.Box(Wc + FVector(0, 0, H * 0.5f), FVector(SegL * 0.5f, 25.f, H * 0.5f + 20.f), Rp);
			}
			// The vault: five voussoirs in a half-round over the gallery, every segment (a rib each 2 m).
			for (int32 v = 0; v < 5; ++v)
			{
				const float Av = PI * (v + 0.5f) / 5.f;
				const FVector Off(0.f, -FMath::Cos(Av) * W * 0.5f, H * 0.78f + FMath::Sin(Av) * W * 0.22f);
				B.K.Add(Limestone(0.58f + 0.1f * (v % 2)), FVector(SegL, W * 0.34f, 30.f), M + R.RotateVector(Off), Rp + FRotator(0.f, 0.f, FMath::RadiansToDegrees(Av) - 90.f));
			}
			B.Box(M + FVector(0, 0, H + 30.f), FVector(SegL * 0.5f, W * 0.5f + 40.f, 25.f), Rp);
			// A candle niche every 16 m, alternating walls; every other one casts light.
			Since += Len;
			if (Since > 1600.f)
			{
				Since = 0.f;
				const float S = (i % 2) ? 1.f : -1.f;
				NicheCandle(B, M + R.RotateVector(FVector(0.f, S * (W * 0.5f - 4.f), 160.f)), R + FRotator(0.f, S > 0 ? -90.f : 90.f, 0.f), (i / 4) % 2 == 0);
			}
			// Bones stacked along the walls in places (hagues): the Ossuary's galleries are walled with them.
			if (L.Level == 2 && B.Rand(Salt + i * 11) < 0.6f)
			{
				const float S = B.Rand(Salt + i * 13) < 0.5f ? 1.f : -1.f;
				const FVector Wc = M + R.RotateVector(FVector(0.f, S * (W * 0.5f - 18.f), 0.f));
				for (int32 c = 0; c < 4; ++c)
				{
					for (float u = -Len * 0.5f + 12.f; u < Len * 0.5f; u += 24.f)
					{
						const FVector P = Wc + R.RotateVector(FVector(u, 0.f, 20.f + c * 26.f));
						if (c == 2) Skull(B, P, 0.95f, R.Yaw + (S > 0 ? -90.f : 90.f), BoneC());
						else B.K.Disc(Bone(0.85f + 0.1f * (c % 2)), P, R.RotateVector(FVector(0.f, -S, 0.f)), 9.f, 8.f);
					}
				}
			}
		}
	}

	bool Build(FBuild& B, AWRGraveScenery* A)
	{
		if (A->Kind == TEXT("room")) Room(B, A);
		else if (A->Kind == TEXT("link")) Link(B, A);
		else return false;
		return true;
	}
}
