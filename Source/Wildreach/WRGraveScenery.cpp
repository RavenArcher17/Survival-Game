// The Graveyard's scenery (cooks/biome-graveyard.md 2.1-2.5, 2.14, 2.20; cooks/build-biome-graveyard.md 2.3-2.6): the
// dispatch from a place's kind to its builder, and the builders of the landmarks (L1 the Chapel of Nhal, L2 the Great
// Yew, L3 the Belfry, L4 the Bone Giant of Rootgrave, L5 the Lantern Stair), the camps, the lantern roads, the grave
// fields, the trees, the chalk stream and the pits. The hand sites and the kit are in WRGraveScenerySites.cpp, the
// catacombs in WRGraveCatacombs.cpp. Every place says why it stands where it does and how it meets the ground, the
// water and the player; built on every machine (no network cost), a dedicated server keeps the collision.

#include "WRGraveWorld.h"
#include "WRGraveLook.h"
#include "WRGraveTypes.h"
#include "WRGameState.h"
#include "WRLand.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

using namespace WRGraveLook;

namespace WRGraveSceneryImpl
{
	/** The places drawn at any range (the skyline, GY 2.3: read from 1 km). */
	bool IsLandmark(FName K)
	{
		return K == TEXT("chapel") || K == TEXT("great_yew") || K == TEXT("belfry") || K == TEXT("bone_giant") || K == TEXT("lantern_stair") || K == TEXT("stair") || K == TEXT("road");
	}
	bool IsKit(FName K)
	{
		return K == TEXT("mausoleum") || K == TEXT("mortsafe_cluster") || K == TEXT("soulstone_outcrop") || K == TEXT("kill_scene") || K == TEXT("wayside_toro")
			|| K == TEXT("bone_cairn") || K == TEXT("field") || K == TEXT("trees") || K == TEXT("bone_pit") || K == TEXT("lime_pit");
	}
}

// Builders in the other files.
namespace WRGraveSites
{
	bool Build(FBuild& B, AWRGraveScenery* A);
}
namespace WRGraveCatacombs
{
	bool Build(FBuild& B, AWRGraveScenery* A);
}

namespace WRGraveLandmarks
{
	using namespace WRGraveSceneryImpl;

	// ---------------------------------------------------------------- L1 the Chapel of Nhal
	/** A clustered pier: a core column ringed by four shafts (the Gothic pier), base and capital, rising H. */
	void Pier(FBuild& B, const FVector& Base, float H, float R)
	{
		const FSurf S = Limestone(0.9f);
		B.K.Add(S, FVector(R * 2.6f, R * 2.6f, 50.f), Base + FVector(0, 0, 25.f));
		B.K.Add(S, FVector(R * 2.f, R * 2.f, H), Base + FVector(0, 0, H * 0.5f + 50.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		for (int32 k = 0; k < 4; ++k)
		{
			const FVector O = FRotator(0.f, 45.f + 90.f * k, 0.f).Vector() * R * 0.95f;
			B.K.Add(Limestone(0.95f), FVector(R * 0.7f, R * 0.7f, H), Base + O + FVector(0, 0, H * 0.5f + 50.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		}
		B.K.Add(S, FVector(R * 2.8f, R * 2.8f, 40.f), Base + FVector(0, 0, H + 70.f));
		B.Box(Base + FVector(0, 0, (H + 90.f) * 0.5f), FVector(R * 1.2f, R * 1.2f, (H + 90.f) * 0.5f));
	}

	/** One bay of a wall along X (from X0 to X1 at Y, outward sign Out): a lancet window (Gothic, 1.2 x 5 m) with a
	 *  mullion, the wall round it in courses, a buttress outside at X1. bBroken: the upper wall fallen to Fall height. */
	void WallBay(FBuild& B, float X0, float X1, float Y, float Out, float H, bool bWindow, float Fall, int32 Salt)
	{
		using namespace WRGraveChapel;
		const float Len = X1 - X0, Xm = (X0 + X1) * 0.5f;
		const float Top = Fall > 0.f ? Fall : H;
		const FSurf S = Limestone(0.88f + 0.12f * B.Rand(Salt));
		const float Z0 = 0.f;
		const float Win = 120.f, Sill = 260.f, Spring = 650.f, Apex = Spring + Win * 0.87f;
		if (!bWindow)
		{
			B.K.Add(S, FVector(Len, Wall, Top), FVector(Xm, Y, Z0 + Top * 0.5f));
		}
		else
		{
			const float Side = (Len - Win) * 0.5f;
			B.K.Add(S, FVector(Side, Wall, Top), FVector(X0 + Side * 0.5f, Y, Top * 0.5f));
			B.K.Add(S, FVector(Side, Wall, Top), FVector(X1 - Side * 0.5f, Y, Top * 0.5f));
			B.K.Add(S, FVector(Win, Wall, Sill), FVector(Xm, Y, Sill * 0.5f));
			if (Top > Apex + 30.f)
			{
				B.K.Add(S, FVector(Win, Wall, Top - Apex), FVector(Xm, Y, (Top + Apex) * 0.5f));
				PointedArch(B, FVector(Xm, Y, 0.f), 90.f, Win, Spring, 22.f, Wall + 6.f, Limestone(1.f), 4);
				// Tracery: the mullion and a quatrefoil at the head; the glass gone, the lanterns inside show green at night.
				B.K.Add(Limestone(1.f), FVector(10.f, 16.f, Spring - Sill), FVector(Xm, Y, (Spring + Sill) * 0.5f));
				B.K.Disc(Limestone(1.f), FVector(Xm, Y, Spring + 50.f), FVector(0.f, 1.f, 0.f), 34.f, 14.f);
				B.N.Add(GraveLight(1.6f), FVector(Win - 14.f, 4.f, Apex - Sill - 20.f), FVector(Xm, Y - Out * 30.f, (Apex + Sill) * 0.5f));
			}
		}
		// Weathering: a string course at the sill, moss at the foot, lichen on the stone.
		B.K.Add(Limestone(1.05f), FVector(Len, Wall + 14.f, 14.f), FVector(Xm, Y, Sill - 7.f));
		B.K.Add(Moss(0.9f), FVector(Len * 0.8f, Wall + 4.f, 40.f), FVector(Xm, Y, 20.f));
		// The buttress at the bay's end: three stepped offsets outward, sloped caps.
		const float Bx = X1;
		for (int32 s = 0; s < 3; ++s)
		{
			const float Hb = Top * (0.9f - 0.27f * s), D = 140.f - 35.f * s;
			B.K.Add(S, FVector(70.f, D, Hb), FVector(Bx, Y + Out * (Wall * 0.5f + D * 0.5f), Hb * 0.5f));
			B.K.Add(Limestone(1.05f), FVector(76.f, D + 8.f, 18.f), FVector(Bx, Y + Out * (Wall * 0.5f + D * 0.5f), Hb + 4.f), FRotator(0.f, 0.f, -Out * 25.f));
		}
		B.Box(FVector(Xm, Y, Top * 0.5f), FVector(Len * 0.5f, Wall * 0.5f, Top * 0.5f));
		B.Box(FVector(Bx, Y + Out * (Wall * 0.5f + 70.f), Top * 0.45f), FVector(35.f, 70.f, Top * 0.45f));
	}

	void Chapel(FBuild& B, AWRGraveScenery* A)
	{
		using namespace WRGraveChapel;
		// The roofless Gothic chapel on the heights (GY 2.14 arena; L1): the Mourners' greatest building and where Nhal
		// still mourns. 12 bays of nave with lancets, a transept, an apse to the east holding the lantern rack and the
		// subclass altar, the broken bell tower at the west end where you come in from the Funeral Road. Only the rib
		// arches of the vault survive, so the bruise sky shows through and the Keen carries. The terrace it stands on
		// was flattened by the generator; the floor's flags sit 3 cm proud of it.
		const float H = WallH;
		// Floor: flagstones 1.4 x 1 m in running bond over the nave, the transept and the apse.
		for (float X = -NaveHalfX + 70.f; X < NaveHalfX + ApseR; X += 140.f)
		{
			for (float Y = -TransHalfY + 50.f; Y < TransHalfY; Y += 100.f)
			{
				const bool bNave = FMath::Abs(Y) < NaveHalfY && X < NaveHalfX;
				const bool bTrans = X > TransX0 && X < TransX1;
				const bool bApse = X >= NaveHalfX && FVector2D(X - NaveHalfX, Y).Size() < ApseR;
				if (!bNave && !bTrans && !bApse) continue;
				const float Off = (int32((Y + 3000.f) / 100.f) % 2) * 70.f;
				B.K.Add(Limestone(0.72f + 0.2f * B.Rand(int32(X * 3 + Y))), FVector(136.f, 96.f, 8.f), FVector(X + Off - 35.f, Y, GZ(B, X, Y) + 1.f));
			}
		}
		// The nave's long walls, bay by bay (the transept's bays open), their tops broken lower toward the west.
		for (int32 k = 0; k < 12; ++k)
		{
			const float X0 = -NaveHalfX + k * Bay, X1 = X0 + Bay;
			const bool bTrans = X0 >= TransX0 - 10.f && X1 <= TransX1 + 10.f;
			for (const float Side : { -1.f, 1.f })
			{
				if (bTrans) continue;
				const float Fall = (k < 3 && B.Rand(k * 7 + int32(Side)) < 0.6f) ? H * (0.45f + 0.3f * B.Rand(k + 40)) : 0.f;
				WallBay(B, X0, X1, Side * (NaveHalfY + Wall * 0.5f), Side, H, true, Fall, k * 3 + int32(Side));
			}
		}
		// The transept: its end walls with a great window, its side walls.
		for (const float Side : { -1.f, 1.f })
		{
			const float Y = Side * (TransHalfY + Wall * 0.5f);
			WallBay(B, TransX0, TransX1, Y, Side, H, true, 0.f, 90 + int32(Side));
			for (const float X : { TransX0, TransX1 })
			{
				const float L = TransHalfY - NaveHalfY;
				B.K.Add(Limestone(0.9f), FVector(Wall, L, H), FVector(X + (X == TransX0 ? -Wall * 0.5f : Wall * 0.5f), Side * (NaveHalfY + L * 0.5f), H * 0.5f));
				B.Box(FVector(X + (X == TransX0 ? -Wall * 0.5f : Wall * 0.5f), Side * (NaveHalfY + L * 0.5f), H * 0.5f), FVector(Wall * 0.5f, L * 0.5f, H * 0.5f));
			}
		}
		// The apse: a half ring of wall with five lancets, round the east end.
		for (int32 s = 0; s < 9; ++s)
		{
			const float A0 = -90.f + 20.f * s, A1 = A0 + 20.f, Am = FMath::DegreesToRadians((A0 + A1) * 0.5f);
			const float Rw = ApseR + Wall * 0.5f;
			const FVector C(NaveHalfX + FMath::Cos(Am) * Rw, FMath::Sin(Am) * Rw, 0.f);
			const float Chord = 2.f * Rw * FMath::Sin(FMath::DegreesToRadians(10.f)) + 6.f;
			const FRotator R(0.f, FMath::RadiansToDegrees(Am) + 90.f, 0.f);
			const bool bWin = s % 2 == 1;
			B.K.Add(Limestone(0.9f), FVector(Chord, Wall, bWin ? 260.f : H), C + FVector(0, 0, (bWin ? 260.f : H) * 0.5f), R);
			if (bWin)
			{
				B.K.Add(Limestone(0.9f), FVector(Chord, Wall, H - 820.f), C + FVector(0, 0, 820.f + (H - 820.f) * 0.5f), R);
				B.N.Add(GraveLight(1.8f), FVector(Chord * 0.7f, 4.f, 520.f), C + FVector(0, 0, 540.f) - FVector(FMath::Cos(Am), FMath::Sin(Am), 0.f) * 40.f, R);
			}
			B.Box(C + FVector(0, 0, H * 0.5f), FVector(Chord * 0.5f, Wall * 0.5f, H * 0.5f), R);
		}
		// The west wall with the tower: the doorway through the tower's base is the way in.
		for (const float Side : { -1.f, 1.f })
		{
			const float L = NaveHalfY - TowerHalf;
			B.K.Add(Limestone(0.88f), FVector(Wall, L, H * 0.8f), FVector(-NaveHalfX - Wall * 0.5f, Side * (TowerHalf + L * 0.5f), H * 0.4f));
			B.Box(FVector(-NaveHalfX - Wall * 0.5f, Side * (TowerHalf + L * 0.5f), H * 0.4f), FVector(Wall * 0.5f, L * 0.5f, H * 0.4f));
		}
		{
			// The tower: 7 m square, its walls hollow round the stair; it leans 8 degrees to the south-west and its top
			// is broken off at 30 m. The door arch (3 m by 6 m) through both its east and west faces.
			const FRotator Lean(0.f, 0.f, -TowerLean * 0.6f);
			const FVector Foot(TowerX, 0.f, 0.f);
			const auto P = [&](float X, float Y, float Z) { return Foot + FRotator(TowerLean * 0.8f, 0.f, -TowerLean * 0.6f).RotateVector(FVector(X, Y, Z)); };
			const FRotator R(TowerLean * 0.8f, 0.f, -TowerLean * 0.6f);
			const float T = 110.f;
			for (int32 Face = 0; Face < 4; ++Face)
			{
				const float Ang = 90.f * Face;
				const FVector N = FRotator(0.f, Ang, 0.f).Vector();
				const FVector Sd = FRotator(0.f, Ang + 90.f, 0.f).Vector();
				const bool bDoor = Face == 0 || Face == 2;
				const float Top = TowerH - (Face == 1 ? 420.f : Face == 3 ? 900.f : 200.f);
				if (bDoor)
				{
					for (const float S2 : { -1.f, 1.f }) B.K.Add(Limestone(0.85f), FVector(T, (TowerHalf * 2.f - 300.f) * 0.5f, Top), P(N.X * (TowerHalf - T * 0.5f) + Sd.X * S2 * (150.f + (TowerHalf * 2.f - 300.f) * 0.25f), N.Y * (TowerHalf - T * 0.5f) + Sd.Y * S2 * (150.f + (TowerHalf * 2.f - 300.f) * 0.25f), Top * 0.5f), R + FRotator(0.f, Ang, 0.f));
					B.K.Add(Limestone(0.85f), FVector(T, 300.f, Top - 760.f), P(N.X * (TowerHalf - T * 0.5f), N.Y * (TowerHalf - T * 0.5f), 760.f + (Top - 760.f) * 0.5f), R + FRotator(0.f, Ang, 0.f));
					PointedArch(B, P(N.X * (TowerHalf - T * 0.5f), N.Y * (TowerHalf - T * 0.5f), 0.f), Ang, 300.f, 480.f, 28.f, T + 8.f, Limestone(1.f), 5);
				}
				else
				{
					B.K.Add(Limestone(0.85f), FVector(T, TowerHalf * 2.f, Top), P(N.X * (TowerHalf - T * 0.5f), N.Y * (TowerHalf - T * 0.5f), Top * 0.5f), R + FRotator(0.f, Ang, 0.f));
					// Belfry openings near the top on the solid faces, louvres long gone.
					B.K.Add(Black(0.6f), FVector(T + 4.f, 120.f, 300.f), P(N.X * (TowerHalf - T * 0.5f), N.Y * (TowerHalf - T * 0.5f), Top - 500.f), R + FRotator(0.f, Ang, 0.f));
				}
				// String courses at each stage, angle buttresses at the corners.
				for (const float Zs : { 800.f, 1600.f, 2400.f }) if (Zs < Top) B.K.Add(Limestone(1.05f), FVector(T + 20.f, TowerHalf * 2.f + 20.f, 18.f), P(N.X * (TowerHalf - T * 0.5f), N.Y * (TowerHalf - T * 0.5f), Zs), R + FRotator(0.f, Ang, 0.f));
				const FVector Corner = N * TowerHalf + Sd * TowerHalf;
				B.K.Add(Limestone(0.9f), FVector(90.f, 90.f, Top * 0.7f), P(Corner.X, Corner.Y, Top * 0.35f), R + FRotator(0.f, Ang + 45.f, 0.f));
			}
			// The broken top: jagged stones round the ruined belfry stage, one fallen on the ground at its foot.
			for (int32 k = 0; k < 7; ++k) B.K.Add(Limestone(0.8f), FVector(70.f + 40.f * B.Rand(k), 50.f, 60.f + 90.f * B.Rand(k + 9)), P((k % 3 - 1) * 200.f, (k / 3 - 1) * 200.f, TowerH - 180.f + 60.f * B.Rand(k + 2)), R + FRotator(10.f * B.Rand(k + 5), 30.f * k, 0.f));
			B.K.Add(Limestone(0.8f), FVector(160.f, 110.f, 90.f), Foot + FVector(-700.f, 400.f, GZ(B, TowerX - 700.f, 400.f) + 40.f), FRotator(12.f, 34.f, 8.f));
			B.Box(Foot + FVector(-700.f, 400.f, GZ(B, TowerX - 700.f, 400.f) + 45.f), FVector(80.f, 55.f, 45.f), FRotator(0.f, 34.f, 0.f));
			// Collision: the four walls (a box each, the doors' gaps between the jambs).
			for (int32 Face = 0; Face < 4; ++Face)
			{
				const float Ang = 90.f * Face;
				const FVector N = FRotator(0.f, Ang, 0.f).Vector(), Sd = FRotator(0.f, Ang + 90.f, 0.f).Vector();
				if (Face == 0 || Face == 2) for (const float S2 : { -1.f, 1.f }) B.Box(P(N.X * (TowerHalf - T * 0.5f) + Sd.X * S2 * 250.f, N.Y * (TowerHalf - T * 0.5f) + Sd.Y * S2 * 250.f, TowerH * 0.45f), FVector(T * 0.5f, 100.f, TowerH * 0.45f), R + FRotator(0.f, Ang, 0.f));
				else B.Box(P(N.X * (TowerHalf - T * 0.5f), N.Y * (TowerHalf - T * 0.5f), TowerH * 0.45f), FVector(T * 0.5f, TowerHalf, TowerH * 0.45f), R + FRotator(0.f, Ang, 0.f));
			}
		}
		// The vault's ribs: transverse pointed arches across the nave every bay from the wall tops (the webbing is gone),
		// three fallen (their stones in heaps on the floor), diagonal ribs over the crossing.
		for (int32 k = 1; k < 12; ++k)
		{
			const float X = -NaveHalfX + k * Bay;
			if (k == 2 || k == 5 || k == 10)
			{
				for (int32 s = 0; s < 6; ++s) B.K.Add(Limestone(0.8f), FVector(60.f, 40.f, 40.f), FVector(X + 80.f * (B.Rand(k * 11 + s) - 0.5f), (s - 2.5f) * 260.f, GZ(B, X, (s - 2.5f) * 260.f) + 20.f), FRotator(20.f * B.Rand(s), 50.f * s, 10.f));
				continue;
			}
			PointedArch(B, FVector(X, 0.f, H - 300.f), 0.f, NaveHalfY * 2.f, 0.f, 46.f, 36.f, Limestone(0.95f), 9);
		}
		for (const float S2 : { -1.f, 1.f })
		{
			const FVector A0(TransX0, -S2 * TransHalfY * 0.6f, H), A1(TransX1, S2 * TransHalfY * 0.6f, H);
			for (int32 s = 0; s < 10; ++s)
			{
				const float t0 = s / 10.f, t1 = (s + 1) / 10.f;
				const FVector P0 = WRForestLook::Arch(A0, A1, t0, 800.f), P1 = WRForestLook::Arch(A0, A1, t1, 800.f);
				B.K.Rod(Limestone(0.95f), P0, P1, 40.f, true);
			}
		}
		// The 8 piers (cover from the Keen) and the broken pews (low: they block nothing).
		for (int32 i = 0; i < 8; ++i) { const FVector2D P = Pillar(i); Pier(B, FVector(P.X, P.Y, GZ(B, P.X, P.Y)), H - 400.f, 55.f); }
		for (int32 r = 0; r < 9; ++r)
		{
			for (const float S2 : { -1.f, 1.f })
			{
				if (B.Rand(r * 5 + int32(S2) + 77) < 0.3f) continue;
				const float X = -NaveHalfX + 300.f + r * 260.f, Y = S2 * 330.f;
				const float Z = GZ(B, X, Y);
				const float Broke = B.Rand(r * 13 + int32(S2)) < 0.4f ? 25.f : 0.f;
				B.K.Add(Timber(0.7f), FVector(40.f, 420.f, 6.f), FVector(X, Y, Z + 44.f), FRotator(0.f, 0.f, Broke));
				B.K.Add(Timber(0.65f), FVector(6.f, 420.f, 60.f), FVector(X - 22.f, Y, Z + 70.f), FRotator(0.f, 0.f, Broke * 0.5f));
				for (const float E : { -190.f, 190.f }) B.K.Add(Timber(0.6f), FVector(44.f, 6.f, 90.f), FVector(X, Y + E, Z + 45.f));
			}
		}
		// The road's staked graves come up to the door; the run-back stone stands by the pond (the boss track).
		// The apse's lantern rack: five tiers of shelves on a timber frame, hundreds of lanterns (lit, and their light).
		{
			const FVector2D Rk = Rack();
			const float Z = GZ(B, Rk.X, Rk.Y);
			for (int32 t = 0; t < 5; ++t)
			{
				for (int32 s = -6; s <= 6; ++s)
				{
					const float Ang = FMath::DegreesToRadians(180.f + s * 12.f);
					const float Rr = 500.f + t * 60.f;
					const FVector C(Rk.X - 400.f + FMath::Cos(Ang) * -Rr * 0.3f + 200.f, FMath::Sin(Ang) * Rr * 1.1f, Z + 80.f + t * 90.f);
					B.K.Add(Timber(0.6f), FVector(30.f, 120.f, 5.f), C, FRotator(0.f, FMath::RadiansToDegrees(Ang), 0.f));
					for (int32 l = -1; l <= 1; ++l)
					{
						const FVector L = C + FRotator(0.f, FMath::RadiansToDegrees(Ang) + 90.f, 0.f).Vector() * (l * 36.f) + FVector(0, 0, 16.f);
						B.K.Add(Paper(0.8f), FVector(16.f, 16.f, 22.f), L, FRotator::ZeroRotator, EWRLookMesh::Cylinder);
						B.N.Add(GraveLight(3.f), FVector(17.f, 17.f, 22.f), L, FRotator::ZeroRotator, EWRLookMesh::Cylinder);
					}
				}
			}
			B.Lamp(FVector(Rk.X - 300.f, 0.f, Z + 300.f), GraveLightC(), 9000.f, 3200.f);
			B.Lamp(FVector(0.f, 0.f, Z + 600.f), GraveLightC(), 5000.f, 4200.f);
			B.Lamp(FVector(-2200.f, 0.f, Z + 500.f), GraveLightC(), 4000.f, 3000.f);
		}
		// The altar (the Necromancy subclass altar, BO): a bone-faced stone table on three steps.
		{
			const FVector2D Al = Altar();
			const float Z = GZ(B, Al.X, Al.Y);
			for (int32 s = 0; s < 3; ++s) B.K.Add(Limestone(0.85f), FVector(380.f - s * 60.f, 520.f - s * 60.f, 18.f), FVector(Al.X, Al.Y, Z + 9.f + s * 18.f));
			B.K.Add(DarkStone(0.9f), FVector(120.f, 260.f, 100.f), FVector(Al.X, Al.Y, Z + 104.f));
			B.K.Add(Limestone(1.f), FVector(140.f, 280.f, 14.f), FVector(Al.X, Al.Y, Z + 160.f));
			for (int32 k = 0; k < 7; ++k) Skull(B, FVector(Al.X - 62.f, Al.Y - 105.f + k * 35.f, Z + 120.f), 0.9f, 180.f, BoneC());
			B.Box(FVector(Al.X, Al.Y, Z + 85.f), FVector(70.f, 140.f, 85.f));
		}
		// The crypt house on the chapel's north side (C12's stair, GY 2.5): a gabled stone house 9 m square over the
		// spiral down to the Chapel Crypt; its iron door stays shut until Nhal's first kill on this server, then stands
		// open (the state's flag "crypt"). Its walls cover the 8 m hole in the ground the spiral comes up through.
		{
			const FWRGraveData& D = FWRGraveData::Get();
			const FVector Cs = B.Local(D.ChapelStair);
			const float Z = GZ(B, Cs.X, Cs.Y), Hw = 460.f, Hh = 380.f, T = 60.f;
			for (int32 Face = 0; Face < 4; ++Face)
			{
				const float Ang = 90.f * Face;
				const FRotator R(0.f, Ang, 0.f);
				const FVector N = R.Vector(), Sd = FRotator(0.f, Ang + 90.f, 0.f).Vector();
				const FVector Mid = FVector(Cs.X, Cs.Y, Z) + N * (Hw - T * 0.5f);
				const bool bDoor = Face == 2;   // west, toward the tower and the way in
				if (bDoor)
				{
					for (const float S2 : { -1.f, 1.f })
					{
						B.K.Add(Limestone(0.85f), FVector(T, Hw - 110.f, Hh + 150.f), Mid + Sd * S2 * (110.f + (Hw - 110.f) * 0.5f) + FVector(0, 0, (Hh + 150.f) * 0.5f - 150.f), R);
						B.Box(Mid + Sd * S2 * (110.f + (Hw - 110.f) * 0.5f) + FVector(0, 0, Hh * 0.5f), FVector(T * 0.5f, (Hw - 110.f) * 0.5f, Hh * 0.5f), R);
					}
					B.K.Add(Limestone(0.85f), FVector(T, 220.f, Hh - 250.f), Mid + FVector(0, 0, 250.f + (Hh - 250.f) * 0.5f), R);
					PointedArch(B, Mid, Ang, 220.f, 180.f, 20.f, T + 6.f, Limestone(1.f), 4);
				}
				else
				{
					B.K.Add(Limestone(0.85f), FVector(T, Hw * 2.f, Hh + 150.f), Mid + FVector(0, 0, (Hh + 150.f) * 0.5f - 150.f), R);
					B.Box(Mid + FVector(0, 0, Hh * 0.5f), FVector(T * 0.5f, Hw, Hh * 0.5f), R);
				}
			}
			for (const float S2 : { -1.f, 1.f }) for (int32 c = 0; c < 6; ++c)
			{
				const float D2 = (c + 0.5f) * 650.f / 6.f;
				B.K.Add(Slate(0.8f + 0.1f * (c % 2)), FVector(Hw * 2.f + 60.f, 112.f, 7.f), FVector(Cs.X, Cs.Y + S2 * (Hw + 30.f - D2 * 0.71f), Z + Hh + D2 * 0.71f), FRotator(0.f, 0.f, -S2 * 45.f));
			}
			B.K.Add(Limestone(0.9f), FVector(Hw * 2.f + 70.f, 40.f, 30.f), FVector(Cs.X, Cs.Y, Z + Hh + Hw * 0.72f));
			// Inside: a floor apron round the stairwell's round mouth (it covers the hole's edge), a lantern on a hook.
			for (float X = -Hw + 50.f; X < Hw; X += 100.f) for (float Y = -Hw + 50.f; Y < Hw; Y += 100.f)
			{
				if (FVector2D(X, Y).Size() < 230.f) continue;
				B.K.Add(Limestone(0.72f + 0.15f * B.Rand(int32(X + Y * 7))), FVector(98.f, 98.f, 12.f), FVector(Cs.X + X, Cs.Y + Y, Z + 2.f));
			}
			B.K.Add(Iron(), FVector(10.f, 10.f, 40.f), FVector(Cs.X + Hw - 80.f, Cs.Y, Z + 280.f));
			A->StateFlag = TEXT("crypt");
			A->StateOff = NewObject<USceneComponent>(A);
			A->StateOff->SetupAttachment(A->GetRootComponent());
			A->StateOff->RegisterComponent();
			A->StateOn = NewObject<USceneComponent>(A);
			A->StateOn->SetupAttachment(A->GetRootComponent());
			A->StateOn->RegisterComponent();
			const FVector Door(Cs.X - Hw + T * 0.5f, Cs.Y, Z);
			if (!B.bServer)
			{
				TArray<FWRLookBlock> Off, On;
				WRLookKit::FKit KOff(B.T, Off), KOn(B.T, On);
				KOff.Add(Iron(), FVector(10.f, 210.f, 250.f), Door + FVector(0, 0, 125.f));
				for (int32 r = 0; r < 4; ++r) KOff.Add(Rust(), FVector(12.f, 212.f, 10.f), Door + FVector(0, 0, 30.f + r * 60.f));
				KOn.Add(Iron(), FVector(10.f, 105.f, 250.f), Door + FVector(-60.f, -150.f, 125.f), FRotator(0.f, 75.f, 0.f));
				KOn.Add(Iron(), FVector(10.f, 105.f, 250.f), Door + FVector(-60.f, 150.f, 125.f), FRotator(0.f, -75.f, 0.f));
				FWRBuildLook::Spawn(A, A->StateOff, Off, FLinearColor(0.25f, 0.2f, 0.15f), false);
				FWRBuildLook::Spawn(A, A->StateOn, On, FLinearColor(0.25f, 0.2f, 0.15f), false);
			}
			A->StateBox = B.Box(Door + FVector(0, 0, 125.f), FVector(10.f, 110.f, 125.f));
		}
	}

	// ---------------------------------------------------------------- L2 the Great Yew
	void GreatYew(FBuild& B, AWRGraveScenery* A)
	{
		// The oldest tree in the Graveyard on the only hill in the grave fields (GY L2): a hollow yew 9 m across with a
		// walk-in room split open on its east side (the tree kit's crack at +X, turned to face east whatever the site's
		// yaw), roots gripping a ring of headstones and lifting them off the ground, bones in the root crotches. Inside, in
		// the dark: a grave-slab in the floor among the roots (S1: dig it). Gloamcrows roost here at dusk.
		const float Yaw = -A->GetActorRotation().Yaw;           // the crack faces world east
		const float Z0 = GZ(B, 0.f, 0.f);
		const float H = 2600.f;
		B.Tree(TEXT("great_yew"), 0, FVector(0.f, 0.f, Z0 - 30.f), H, FRotator(0.f, Yaw, 0.f), 0.f);
		// The trunk's collision: a ring of boxes round the bole, open where the crack is (east, 34 degrees).
		const float R = 450.f;
		for (int32 k = 0; k < 16; ++k)
		{
			const float Ang = 360.f * (k + 0.5f) / 16.f;
			const float World = FMath::UnwindDegrees(Ang);
			if (FMath::Abs(World) < 22.f) continue;
			const FRotator Rk(0.f, Ang + Yaw, 0.f);
			B.Box(FVector(0.f, 0.f, Z0) + Rk.Vector() * (R - 40.f) + FVector(0, 0, 450.f), FVector(70.f, R * 0.22f, 450.f), Rk);
		}
		B.Box(FVector(0.f, 0.f, Z0 + 1400.f), FVector(R * 0.8f, R * 0.8f, 500.f));
		// The hollow's floor: litter, the grave-slab, roots across it.
		const FVector In = FRotator(0.f, Yaw, 0.f).RotateVector(FVector(-30.f, 0.f, 0.f)) + FVector(0, 0, Z0);
		B.K.Add(Soil(0.6f), FVector(520.f, 520.f, 6.f), In);
		B.K.Add(GreyStone(0.7f), FVector(190.f, 90.f, 10.f), In + FRotator(0.f, Yaw, 0.f).RotateVector(FVector(350.f, 150.f, 5.f)), FRotator(2.f, Yaw + 8.f, 0.f));
		B.K.Add(Lichen(0.6f), FVector(150.f, 60.f, 2.f), In + FRotator(0.f, Yaw, 0.f).RotateVector(FVector(350.f, 150.f, 11.f)), FRotator(2.f, Yaw + 8.f, 0.f));
		// The ring of nine headstones the roots took, each lifted and tilted on a root that runs out from the bole under it.
		for (int32 i = 0; i < 9; ++i)
		{
			const float Ang = 360.f * i / 9.f + 15.f;
			const FVector Dir = FRotator(0.f, Ang, 0.f).Vector();
			const float Rr = 820.f + 90.f * B.Rand(i);
			const FVector Stone = Dir * Rr + FVector(0, 0, GZ(B, Dir.X * Rr, Dir.Y * Rr));
			const float Lift = 60.f + 80.f * B.Rand(i + 9);
			TArray<FVector> Root = { Dir * 380.f + FVector(0, 0, Z0 + 40.f), Dir * 600.f + FVector(0, 0, GZ(B, Dir.X * 600.f, Dir.Y * 600.f) + 30.f), Stone + FVector(0, 0, Lift - 25.f), Dir * (Rr + 260.f) + FVector(0, 0, GZ(B, Dir.X * (Rr + 260.f), Dir.Y * (Rr + 260.f)) - 20.f) };
			B.Root(Root, 46.f, 16.f, YewC() * 0.9f, 8);
			Headstone(B, Stone + FVector(0, 0, Lift), Ang + 180.f, i % 3 == 1 ? 1 : 0, 95.f, 18.f + 14.f * B.Rand(i + 3), false, false, false, i * 11 + 3);
			B.SegBox(Root[1], Root[2], 40.f, 40.f);
			// Bones caught in the root's crotch.
			if (i % 3 == 0) LongBone(B, Root[1] + FVector(0, 30.f, 40.f), Root[1] + FVector(30.f, -20.f, 50.f), 2.4f, BoneC());
			if (i % 4 == 1) Skull(B, Root[1] + FVector(10.f, 0, 52.f), 1.f, Ang + 180.f, BoneC());
		}
		// More roots running down the hill without stones.
		for (int32 i = 0; i < 6; ++i)
		{
			const FVector Dir = FRotator(0.f, 30.f + 60.f * i, 0.f).Vector();
			B.Root({ Dir * 400.f + FVector(0, 0, Z0 + 30.f), Dir * 900.f + FVector(0, 0, GZ(B, Dir.X * 900.f, Dir.Y * 900.f) + 10.f), Dir * 1500.f + FVector(0, 0, GZ(B, Dir.X * 1500.f, Dir.Y * 1500.f) - 15.f) }, 34.f, 8.f, YewC() * 0.85f, 7);
		}
	}

	// ---------------------------------------------------------------- L3 the Belfry
	void Belfry(FBuild& B, AWRGraveScenery* A)
	{
		using namespace WRGraveBelfry;
		// The Bell-Ringer's yard (GY 2.13.1; L3): a free-standing square tower 35 m high in a walled churchyard 50 m
		// across, the great bronze bell visible in its open top against the sky. The bell, its headstock and wheel and the
		// rope are the boss's machine (it swings on a server clock: WRGraveBosses); this is the stone. The rope comes
		// down the tower's hollow shaft to the pointed arch in its east face, into the yard. The yard's gate is on the west
		// side toward the Smiths' Road and the run-back stone.
		const float Z0 = GZ(B, 0.f, 0.f);
		const float T = 90.f;
		for (int32 Face = 0; Face < 4; ++Face)
		{
			const float Ang = 90.f * Face;
			const FRotator R(0.f, Ang, 0.f);
			const FVector N = R.Vector(), Sd = FRotator(0.f, Ang + 90.f, 0.f).Vector();
			const FVector Mid = N * (TowerHalf - T * 0.5f) + FVector(0, 0, Z0);
			const bool bArch = Face == 0;  // east: the rope's arch
			// Four stages: 0-1000, 1000-2000, 2000-2800 (string courses between), the belfry 2800-3500 with an opening.
			for (int32 St = 0; St < 4; ++St)
			{
				const float Za = St == 0 ? 0.f : St == 1 ? 1000.f : St == 2 ? 2000.f : 2800.f;
				const float Zb = St == 0 ? 1000.f : St == 1 ? 2000.f : St == 2 ? 2800.f : TowerH;
				if (St == 0 && bArch)
				{
					for (const float S2 : { -1.f, 1.f }) B.K.Add(GreyStone(0.88f), FVector(T, (TowerHalf * 2.f - 240.f) * 0.5f, Zb - Za), Mid + Sd * S2 * (120.f + (TowerHalf * 2.f - 240.f) * 0.25f) + FVector(0, 0, (Za + Zb) * 0.5f), R);
					B.K.Add(GreyStone(0.88f), FVector(T, 240.f, Zb - 520.f), Mid + FVector(0, 0, 520.f + (Zb - 520.f) * 0.5f), R);
					PointedArch(B, Mid, Ang, 240.f, 300.f, 24.f, T + 6.f, GreyStone(1.f), 5);
				}
				else if (St == 3)
				{
					// The belfry stage: a pointed opening 2.4 x 4 m in each face; the corners as piers.
					for (const float S2 : { -1.f, 1.f }) B.K.Add(GreyStone(0.88f), FVector(T, 220.f, Zb - Za), Mid + Sd * S2 * (TowerHalf - 110.f) + FVector(0, 0, (Za + Zb) * 0.5f), R);
					B.K.Add(GreyStone(0.88f), FVector(T, 300.f, 140.f), Mid + FVector(0, 0, Za + 70.f), R);
					PointedArch(B, Mid, Ang, 260.f, Za + 260.f, 26.f, T + 6.f, GreyStone(1.f), 5);
					B.K.Add(GreyStone(0.88f), FVector(T, 300.f, Zb - (Za + 260.f + 230.f)), Mid + FVector(0, 0, (Zb + Za + 490.f) * 0.5f), R);
				}
				else B.K.Add(GreyStone(0.82f + 0.08f * St), FVector(T, TowerHalf * 2.f, Zb - Za), Mid + FVector(0, 0, (Za + Zb) * 0.5f), R);
				if (St > 0) B.K.Add(GreyStone(1.05f), FVector(T + 24.f, TowerHalf * 2.f + 24.f, 20.f), Mid + FVector(0, 0, Za), R);
			}
			// The parapet round the open top, its merlons.
			for (int32 m = -2; m <= 2; ++m) B.K.Add(GreyStone(0.9f), FVector(T, 80.f, 100.f), Mid + Sd * m * 150.f + FVector(0, 0, TowerH + 50.f), R);
			// Clasping buttresses at the corners, stepped.
			const FVector Corner = N * TowerHalf + Sd * TowerHalf + FVector(0, 0, Z0);
			B.K.Add(GreyStone(0.85f), FVector(110.f, 110.f, 1900.f), Corner + FVector(0, 0, 950.f), FRotator(0.f, Ang + 45.f, 0.f));
			B.K.Add(GreyStone(0.85f), FVector(80.f, 80.f, 900.f), Corner + FVector(0, 0, 2350.f), FRotator(0.f, Ang + 45.f, 0.f));
			if (bArch) for (const float S2 : { -1.f, 1.f }) B.Box(Mid + Sd * S2 * (TowerHalf * 0.5f + 60.f) + FVector(0, 0, TowerH * 0.5f), FVector(T * 0.5f, TowerHalf * 0.5f - 60.f, TowerH * 0.5f), R);
			else B.Box(Mid + FVector(0, 0, TowerH * 0.5f), FVector(T * 0.5f, TowerHalf, TowerH * 0.5f), R);
		}
		// The bell frame's beams across the belfry (the bell hangs on them: the boss track's machine sits here).
		for (const float S2 : { -1.f, 1.f }) B.K.Add(Timber(0.7f), FVector(TowerHalf * 2.f - 2.f * T, 34.f, 40.f), FVector(0.f, S2 * 120.f, Z0 + Wheel().Z + 60.f));
		for (const float S2 : { -1.f, 1.f }) B.K.Add(Timber(0.65f), FVector(30.f, 30.f, 420.f), FVector(S2 * 200.f, 0.f, Z0 + Wheel().Z - 150.f));
		// The yard: a low churchyard wall (1.2 m, coursed, coped) round 50 m with the gate gap west; inside 16 grave mounds
		// (where the Toll's Risen climb out) and 6 tall headstones (cover from the thrown stones).
		const float Yh = YardHalf;
		const TArray<FVector2D> Corners = { FVector2D(Yh, -Yh), FVector2D(Yh, Yh), FVector2D(-Yh, Yh), FVector2D(-Yh, 250.f), FVector2D(-Yh, -250.f), FVector2D(-Yh, -Yh), FVector2D(Yh, -Yh) };
		for (int32 i = 0; i + 1 < Corners.Num(); ++i)
		{
			if (i == 3) continue;  // the gate
			DryWall(B, Corners[i], Corners[i + 1], 120.f, 300 + i * 37);
		}
		for (const float S2 : { -1.f, 1.f })
		{
			B.K.Add(GreyStone(0.9f), FVector(70.f, 70.f, 220.f), FVector(-Yh, S2 * 290.f, GZ(B, -Yh, S2 * 290.f) + 110.f));
			B.K.Spike(GreyStone(1.f), FVector(-Yh, S2 * 290.f, GZ(B, -Yh, S2 * 290.f) + 220.f), FVector::UpVector, 50.f, 40.f);
			B.Box(FVector(-Yh, S2 * 290.f, GZ(B, -Yh, S2 * 290.f) + 110.f), FVector(35.f, 35.f, 110.f));
		}
		for (int32 i = 0; i < 16; ++i)
		{
			const FVector2D M = Mound(i);
			const FVector P = GAt(B, M.X, M.Y);
			const float Yw = FMath::RadiansToDegrees(FMath::Atan2(M.Y, M.X));
			B.K.Add(Soil(0.85f + 0.2f * B.Rand(i)), FVector(200.f, 90.f, 30.f), P + FVector(0, 0, 4.f), FRotator(0.f, Yw + 90.f, 0.f), EWRLookMesh::Cylinder).Size = FVector(200.f, 90.f, 30.f);
			B.K.Add(Turf(0.9f), FVector(190.f, 84.f, 6.f), P + FVector(0, 0, 18.f), FRotator(0.f, Yw + 90.f, 0.f));
			Headstone(B, P + FRotator(0.f, Yw + 90.f, 0.f).RotateVector(FVector(-110.f, 0.f, 0.f)), Yw + 90.f, i % 4 == 2 ? 3 : 0, 60.f + 25.f * B.Rand(i + 30), 4.f + 10.f * B.Rand(i + 31), false, false, true, i * 5 + 1);
		}
		for (int32 i = 0; i < 6; ++i)
		{
			const float Ang = FMath::DegreesToRadians(30.f + 60.f * i);
			const FVector P = GAt(B, FMath::Cos(Ang) * 1100.f, FMath::Sin(Ang) * 1100.f);
			Headstone(B, P, FMath::RadiansToDegrees(Ang) + 180.f, 0, 230.f, 3.f, false, false, true, 900 + i);
			B.Box(P + FVector(0, 0, 110.f), FVector(12.f, 60.f, 110.f), FRotator(0.f, FMath::RadiansToDegrees(Ang) + 180.f, 0.f));
		}
		B.Lamp(FVector(0.f, 0.f, Z0 + 3000.f), GraveLightC(), 1500.f, 1800.f);
		(void)A;
	}

	// ---------------------------------------------------------------- L4 the Bone Giant of Rootgrave
	void BoneGiant(FBuild& B, AWRGraveScenery* A)
	{
		// The Grovekin's champion (world-map: a bone giant on the Graveyard side of Rootgrave, grown through by roots): 40 m
		// of human-like skeleton slumped against the Spine where the Mourners killed it, ribs the height of houses, the
		// forest's roots grown up through its chest, Mourner lantern-poles driven through its bones like spears. Local +X
		// runs up the Spine's slope (east): the pelvis on the ground, the spine and skull lying up the slope, the legs out
		// west down the hill, one arm flung across the ground, the other draped over its ribs.
		const FLinearColor Bc = BoneC() * 0.92f;
		const float S = 1.f;
		const float Zp = GZ(B, 0.f, 0.f);
		const FVector Pelvis(0.f, 0.f, Zp + 180.f);
		// Spine: vertebrae up the slope to the skull at 15 m along and ~8 m up the slope.
		TArray<FVector> Spine;
		for (int32 v = 0; v <= 12; ++v)
		{
			const float X = 150.f + v * 115.f;
			Spine.Add(FVector(X, 40.f * FMath::Sin(v * 0.4f), FMath::Max(GZ(B, X, 0.f) + 160.f, Zp + 180.f + v * 55.f)));
		}
		for (int32 v = 0; v < Spine.Num(); ++v)
		{
			B.Ball(Spine[v], 70.f * S, Bc, 0.8f, 0.7f);
			B.Limb(Spine[v], Spine[v] + FVector(0.f, 0.f, 95.f), 30.f, 18.f, Bc, 6);   // spinous process
			if (v + 1 < Spine.Num()) B.Limb(Spine[v], Spine[v + 1], 46.f, 46.f, Bc, 8);
		}
		B.SegBox(Spine[0], Spine.Last(), 90.f, 90.f);
		// Ribs: twelve pairs arching from the spine's middle, closing toward a sternum on the up side (a cage 10 m tall
		// you can stand inside).
		for (int32 r = 0; r < 12; ++r)
		{
			const FVector Root = Spine[FMath::Clamp(3 + r * 8 / 12, 0, Spine.Num() - 1)];
			for (const float Side : { -1.f, 1.f })
			{
				TArray<FVector> Rib;
				const float Span = 380.f + 120.f * FMath::Sin(PI * (r + 1) / 13.f);
				for (int32 k = 0; k <= 6; ++k)
				{
					const float A2 = PI * k / 6.f;
					Rib.Add(Root + FVector(-60.f * k / 6.f, Side * FMath::Sin(A2) * Span, FMath::Cos(A2) * -Span * 0.2f + FMath::Sin(A2 * 0.5f) * Span * 1.6f));
				}
				// Broken ribs: a few end short.
				if (B.Rand(r * 3 + int32(Side)) < 0.2f) Rib.SetNum(4);
				B.Root(Rib, 26.f, 18.f, Bc, 7);
				for (int32 k = 0; k + 1 < Rib.Num(); k += 2) B.SegBox(Rib[k], Rib[k + 1], 24.f, 24.f);
			}
		}
		// The skull at the top of the spine: a cranium 5 m long, the jaw fallen open, the dark orbits.
		const FVector Sk = Spine.Last() + FVector(260.f, 0.f, 120.f);
		Skull(B, Sk, 30.f, 0.f, Bc);
		B.Box(Sk, FVector(260.f, 220.f, 240.f));
		// The pelvis, the legs out down the hill, the arms.
		B.Ball(Pelvis, 220.f, Bc, 0.8f, 0.6f);
		for (const float Side : { -1.f, 1.f })
		{
			const FVector Hip = Pelvis + FVector(-80.f, Side * 200.f, -40.f);
			const FVector Knee(-1400.f, Side * (420.f + 120.f * B.Rand(int32(Side) + 5)), GZ(B, -1400.f, Side * 420.f) + 160.f);
			const FVector Foot(-2600.f, Side * 560.f, GZ(B, -2600.f, Side * 560.f) + 70.f);
			LongBone(B, Hip, Knee, 70.f, Bc);
			LongBone(B, Knee, Foot, 56.f, Bc);
			B.SegBox(Hip, Knee, 70.f, 70.f);
			B.SegBox(Knee, Foot, 56.f, 56.f);
			for (int32 t = 0; t < 5; ++t) B.Limb(Foot, Foot + FVector(-180.f, (t - 2) * 45.f, -40.f), 14.f, 9.f, Bc, 5);
			const FVector Shoulder = Spine[9] + FVector(0.f, Side * 380.f, 120.f);
			const FVector Elbow = Side < 0.f ? FVector(300.f, -1500.f, GZ(B, 300.f, -1500.f) + 90.f) : Spine[6] + FVector(-200.f, 600.f, 700.f);
			const FVector Hand = Side < 0.f ? FVector(-600.f, -2300.f, GZ(B, -600.f, -2300.f) + 50.f) : FVector(-500.f, 900.f, GZ(B, -500.f, 900.f) + 60.f);
			LongBone(B, Shoulder, Elbow, 50.f, Bc);
			LongBone(B, Elbow, Hand, 40.f, Bc);
			B.SegBox(Shoulder, Elbow, 50.f, 50.f);
			B.SegBox(Elbow, Hand, 40.f, 40.f);
			for (int32 f = 0; f < 5; ++f) B.Limb(Hand, Hand + FRotator(0.f, (Side < 0.f ? 200.f : 160.f) + f * 12.f, 0.f).Vector() * 220.f + FVector(0, 0, -30.f), 12.f, 7.f, Bc, 5);
		}
		// The forest's roots up through the ribcage from the ground (Rootgrave: the Grovekin's ground pushing in).
		for (int32 i = 0; i < 9; ++i)
		{
			const float Y = (B.Rand(i + 50) - 0.5f) * 700.f, X = 300.f + i * 110.f;
			B.Root({ FVector(X, Y, GZ(B, X, Y) - 20.f), FVector(X + 40.f, Y * 0.6f, GZ(B, X, Y) + 400.f), FVector(X - 30.f, Y * 1.2f, GZ(B, X, Y) + 900.f), FVector(X + 60.f, Y * 1.4f + 100.f, GZ(B, X, Y) + 1250.f) }, 38.f, 6.f, RootC(), 7);
		}
		// Moss on the bones' upper sides, white blossom on the roots (the forest's side of the war).
		for (int32 v = 2; v < Spine.Num(); v += 3) B.K.Add(Moss(1.f), FVector(160.f, 120.f, 20.f), Spine[v] + FVector(0, 0, 65.f), FRotator(0.f, 20.f * v, 0.f));
		// Lantern-poles driven through its bones (the Mourners killed it here): long poles at spear angles, tatters of
		// paper lantern still on two of them.
		const FVector Poles[6][2] = { { FVector(-400.f, -500.f, Zp), Spine[4] + FVector(0, 0, 120.f) }, { FVector(200.f, 700.f, Zp), Spine[7] + FVector(0, 0, 180.f) },
			{ FVector(-900.f, 200.f, Zp), Pelvis + FVector(0, 0, 150.f) }, { FVector(800.f, -700.f, Zp), Spine[10] + FVector(0, 0, 160.f) },
			{ FVector(-1700.f, -200.f, Zp), FVector(-1300.f, -420.f, Zp + 380.f) }, { FVector(1600.f, 500.f, Zp), Sk + FVector(-100.f, 0.f, 80.f) } };
		for (int32 i = 0; i < 6; ++i)
		{
			FVector P0 = Poles[i][0];
			P0.Z = GZ(B, P0.X, P0.Y);
			const FVector Dir = (Poles[i][1] - P0).GetSafeNormal();
			const FVector End = Poles[i][1] + Dir * 260.f;
			B.K.Rod(OldWood(0.7f), P0, End, 12.f);
			if (i % 3 == 0)
			{
				B.K.Add(Paper(0.6f), FVector(30.f, 30.f, 22.f), End + FVector(0, 0, -20.f), FRotator(20.f, 0.f, 15.f), EWRLookMesh::Cylinder);
			}
		}
		(void)A;
	}

	// ---------------------------------------------------------------- L5 the Lantern Stair
	void LanternStair(FBuild& B, AWRGraveScenery* A)
	{
		// The Mourners' stair toward the Crown (world-map: every stair breaks off): a 1.6 m stair cut into the Spine's face,
		// flight after flight (Data/graveyard.json "stair", the generator kept every tread a metre into the rock), tōrō
		// both sides every 12 m. The last 50 m of lanterns are unlit; at the break, where the last flight stops in the
		// air, one lantern stands on the edge facing up (S3: light it and every lantern on the Stair burns gold).
		const FWRGraveData& D = FWRGraveData::Get();
		if (D.Stair.Num() < 2) return;
		const float BreakZ = D.Stair.Last().Z;
		TArray<FWRLookBlock> Gold;
		WRLookKit::FKit KG(B.T, Gold);
		float SinceToro = 0.f;
		for (int32 i = 0; i + 1 < D.Stair.Num(); ++i)
		{
			const FVector P0 = B.Local(D.Stair[i]), P1 = B.Local(D.Stair[i + 1]);
			const FVector Run = P1 - P0;
			const float Rise = Run.Z;
			const FVector2D Flat(Run.X, Run.Y);
			const float Len = Flat.Size();
			if (Len < 10.f) continue;
			const FVector Dir = FVector(Flat.X, Flat.Y, 0.f) / Len;
			const FVector Side(-Dir.Y, Dir.X, 0.f);
			const FRotator R(0.f, FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)), 0.f);
			if (FMath::Abs(Rise) < 5.f)
			{
				// A landing: flags on a block, a low parapet on the open side.
				B.K.Add(Limestone(0.85f), FVector(Len + 160.f, 220.f, 60.f), (P0 + P1) * 0.5f + FVector(0, 0, -30.f), R);
				B.Box((P0 + P1) * 0.5f + FVector(0, 0, -30.f), FVector(Len * 0.5f + 80.f, 110.f, 30.f), R);
				continue;
			}
			const int32 N = FMath::Max(1, FMath::CeilToInt(Rise / 30.f));
			const bool bLast = i + 2 == D.Stair.Num();
			for (int32 s = 0; s < N; ++s)
			{
				const float T0 = float(s) / N, T1 = float(s + 1) / N;
				const FVector Tread = FMath::Lerp(P0, P1, (T0 + T1) * 0.5f);
				const float Top = FMath::Lerp(P0.Z, P1.Z, T1);
				// The broken end: the last flight's last treads crumble off.
				if (bLast && s > N - 3) { B.K.Add(Limestone(0.75f), FVector(Len / N * 0.7f, 120.f, 26.f), FVector(Tread.X, Tread.Y, Top - 20.f) + Side * 30.f, R + FRotator(6.f, 0.f, 8.f)); continue; }
				B.K.Add(Limestone(0.8f + 0.12f * B.Rand(i * 31 + s)), FVector(Len / N + 4.f, 160.f, 140.f), FVector(Tread.X, Tread.Y, Top - 70.f), R);
				B.K.Add(Limestone(1.f), FVector(Len / N + 6.f, 164.f, 4.f), FVector(Tread.X, Tread.Y, Top + 1.f), R);
			}
			// One sloped collision box per flight under the treads (walk it like a ramp).
			const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(Rise, Len));
			B.Box((P0 + P1) * 0.5f + FVector(0, 0, -30.f), FVector(FMath::Sqrt(Len * Len + Rise * Rise) * 0.5f, 80.f, 30.f), R + FRotator(Pitch, 0.f, 0.f));
			// The outer parapet (the face is on the inner side): a low wall, broken in places.
			for (int32 s = 0; s < N; s += 2)
			{
				if (B.Rand(i * 17 + s) < 0.25f) continue;
				const FVector Tread = FMath::Lerp(P0, P1, (s + 1.f) / N);
				B.K.Add(Limestone(0.8f), FVector(Len / N * 2.f, 30.f, 70.f), FVector(Tread.X, Tread.Y, Tread.Z + 30.f) - Side * 95.f, R + FRotator(Pitch, 0.f, 0.f));
			}
			// Tōrō both sides every 12 m along the climb: unlit in the last 50 m; the gold copies for S3.
			const float Climb = FMath::Sqrt(Len * Len + Rise * Rise);
			for (float d = 600.f - SinceToro; d < Climb; d += 1200.f)
			{
				const FVector P = FMath::Lerp(P0, P1, d / Climb);
				const bool bUnlit = P.Z > BreakZ - 5000.f;
				for (const float S2 : { -1.f, 1.f })
				{
					const FVector Base = P + Side * S2 * 120.f + FVector(0, 0, -6.f);
					Toro(B, Base, R.Yaw + 90.f * S2, 0.7f, !bUnlit && S2 > 0.f, 0.f, bUnlit);
					if (!bUnlit) KG.Add(FSurf::Lit(Rgb(255, 196, 96) * 0.6f, 6.f), FVector(22.f, 22.f, 22.f), Base + FVector(0, 0, 128.f * 0.7f), FRotator(0.f, R.Yaw, 0.f));
				}
			}
			SinceToro = FMath::Fmod(SinceToro + Climb, 1200.f);
		}
		// The last lantern on the edge at the break, facing up (S3's spot is here; its own light when lit).
		const FVector Brk = B.Local(D.Stair.Last());
		Toro(B, Brk + FVector(0, 0, -4.f), 0.f, 0.75f, false, -18.f, true);
		// The gold night look and the stair's lamps (near lamps: the world's light budget).
		A->NearLamps = A->NightLights;
		A->NearPower = A->NightPower;
		A->NightLights.Reset();
		A->NightPower.Reset();
		if (!B.bServer && Gold.Num())
		{
			A->AltNightRoot = NewObject<USceneComponent>(A);
			A->AltNightRoot->SetupAttachment(A->GetRootComponent());
			A->AltNightRoot->RegisterComponent();
			FWRBuildLook::Spawn(A, A->AltNightRoot, Gold, FLinearColor(0.25f, 0.2f, 0.15f), false);
			A->AltNightRoot->SetVisibility(false, true);
		}
	}

	// ---------------------------------------------------------------- the camps
	void Camp(FBuild& B, AWRGraveScenery* A)
	{
		// A castaway camp in a coombe's mouth over its shingle cove (world-map: three on the outer coast): the newcomers'
		// fire and lean-to (the forest camp's own build), and below on the shingle the wreck of the ship that brought
		// them, broken-backed, ribs showing, a snapped mast. A tōrō someone carried down from the fields stands at the
		// camp's edge, unlit: they don't know the Mourners' ways yet.
		B.Camp();
		const float Yaw = 180.f;   // the camp faces west, to the sea
		const FVector Wp = FRotator(0.f, 0.f, 0.f).RotateVector(FVector(3800.f + 400.f * B.Rand(4), 900.f * (B.Rand(5) - 0.5f), 0.f));
		Wreck(B, FVector(-Wp.X * 0.f - 0.f, 0.f, 0.f) + FVector(Wp.X, Wp.Y, GZ(B, Wp.X, Wp.Y) - 40.f), Yaw + 70.f + 30.f * B.Rand(6), 1800.f, A->Variant * 13 + 5);
		Toro(B, GAt(B, -600.f, 900.f), 0.f, 0.8f, false, 3.f, true);
	}

	// ---------------------------------------------------------------- the lantern roads
	void Road(FBuild& B, AWRGraveScenery* A)
	{
		// A tōrō every 30 m along every old road, alternating sides (world-map's readable night: from 1 km the roads read
		// as dotted green lines). Their light boxes glow by night; the nearest few also cast light (the world's budget).
		const FWRGraveData& D = FWRGraveData::Get();
		if (!D.Roads.IsValidIndex(A->Index)) return;
		const FName RoadId = D.Roads[A->Index].Id;
		for (const FWRGraveData::FToro& T : D.Toro)
		{
			// Each tōrō belongs to the road whose line passes nearest it.
			float Best = TNumericLimits<float>::Max();
			int32 BestRoad = INDEX_NONE;
			for (int32 r = 0; r < D.Roads.Num(); ++r)
			{
				const TArray<FVector>& P = D.Roads[r].Points;
				for (int32 i = 0; i + 1 < P.Num(); ++i)
				{
					const float Dd = FMath::PointDistToSegment(FVector(T.At.X, T.At.Y, 0.f), FVector(P[i].X, P[i].Y, 0.f), FVector(P[i + 1].X, P[i + 1].Y, 0.f));
					if (Dd < Best) { Best = Dd; BestRoad = r; }
				}
			}
			if (BestRoad != A->Index || T.bStair) continue;
			Toro(B, B.Local(T.At) + FVector(0, 0, -8.f), T.Yaw, 1.f, true, 2.f * (B.Rand(int32(T.At.X)) - 0.5f));
			B.Box(B.Local(T.At) + FVector(0, 0, 90.f), FVector(30.f, 30.f, 90.f));
		}
		A->NearLamps = A->NightLights;
		A->NearPower = A->NightPower;
		A->NightLights.Reset();
		A->NightPower.Reset();
		(void)RoadId;
	}

	// ---------------------------------------------------------------- the grave fields
	void Field(FBuild& B, AWRGraveScenery* A)
	{
		// Rows north to south, a stone at each grave's head (west end) facing east to the sunrise (GY 2.2 G1: four
		// styles; build-biome-graveyard 1: 8% leaning back, 5% fallen, kerbs on 30%), a small tōrō every 6th stone.
		// Smith's fields carry slag stones; the chapel road's are sealed with heartwood stakes, roots and white blossom
		// (the counter shown before it is needed, GY 2.14); the Rows End stop part-way and the rest are dug and empty.
		const FWRGraveData& D = FWRGraveData::Get();
		if (!D.Fields.IsValidIndex(A->Index)) return;
		const FWRGraveData::FField& F = D.Fields[A->Index];
		int32 Salt = A->Index * 1009;
		for (const FWRGraveData::FRow& Row : F.Rows)
		{
			const FVector2D Step = Row.N > 1 ? (Row.B - Row.A) / float(Row.N - 1) : FVector2D::ZeroVector;
			for (int32 i = 0; i < Row.N; ++i)
			{
				++Salt;
				const FVector2D Wp = Row.A + Step * float(i);
				const FVector L = B.Local(FVector(Wp, 0.f));
				const FVector Base = GAt(B, L.X, L.Y);
				const float R0 = B.Rand(Salt);
				const int32 Style = F.bSmiths ? 0 : R0 < 0.45f ? 0 : R0 < 0.65f ? 1 : R0 < 0.8f ? 2 : 3;
				const bool bFallen = B.Rand(Salt + 7) < 0.05f;
				const float Lean = B.Rand(Salt + 9) < 0.08f ? 6.f + 8.f * B.Rand(Salt + 11) : 1.5f * B.Rand(Salt + 13);
				const bool bKerb = B.Rand(Salt + 15) < 0.3f;
				const float H = Style == 3 ? 70.f + 30.f * B.Rand(Salt + 17) : 60.f + 60.f * B.Rand(Salt + 19);
				Headstone(B, Base, 0.f, Style, H, Lean, bFallen, bKerb, true, Salt, F.bSmiths);
				// The grave itself: a low mound (older ones sunk flat), a slab for some.
				if (!bKerb) B.K.Add(B.Rand(Salt + 21) < 0.3f ? Soil(0.8f) : Turf(0.85f + 0.2f * B.Rand(Salt + 23)), FVector(180.f, 80.f, 12.f), Base + FVector(100.f, 0.f, 2.f));
				if (i % 6 == 5 && !F.bSmiths) Toro(B, Base + FVector(-60.f, 110.f, 0.f), 0.f, 0.55f, false);
				if (F.bSmiths && B.Rand(Salt + 25) < 0.7f)
				{
					// The smith's hammer laid on the grave.
					B.K.Rod(OldWood(0.8f), Base + FVector(80.f, -10.f, 14.f), Base + FVector(140.f, 18.f, 14.f), 4.f);
					B.K.Add(Iron(), FVector(10.f, 22.f, 9.f), Base + FVector(144.f, 20.f, 16.f), FRotator(0.f, 25.f, 0.f));
				}
				if (F.bStaked)
				{
					// A heartwood stake driven in at the grave's heart; roots bind the slab and white blossom over it.
					B.K.Rod(FSurf::Wood(Rgb(196, 150, 96) * 0.5f), Base + FVector(100.f, 0.f, -20.f), Base + FVector(104.f, 4.f, 70.f), 9.f);
					for (int32 r = 0; r < 4; ++r) B.Root({ Base + FVector(40.f + r * 40.f, -50.f, 2.f), Base + FVector(60.f + r * 40.f, 0.f, 16.f), Base + FVector(80.f + r * 40.f, 50.f, 2.f) }, 4.f, 2.f, RootC(), 5);
					for (int32 f = 0; f < 6; ++f) B.Ball(Base + FVector(70.f + 20.f * f, (B.Rand(Salt + f) - 0.5f) * 60.f, 18.f), 5.f, FLinearColor(0.85f, 0.84f, 0.8f), 0.6f);
				}
			}
			if (F.bEnds)
			{
				// Past the last stone: the graves they dug for the dead who never came: open holes with spoil heaps.
				const FVector2D Dir = Row.N > 1 ? (Row.B - Row.A).GetSafeNormal() : FVector2D(0.f, 1.f);
				for (int32 k = 1; k <= 3; ++k)
				{
					const FVector2D Wp = Row.B + Dir * (200.f * k);
					const FVector L = B.Local(FVector(Wp, 0.f));
					const FVector Base = GAt(B, L.X, L.Y);
					B.K.Add(Black(0.4f), FVector(190.f, 80.f, 4.f), Base + FVector(100.f, 0.f, 1.f));
					B.K.Add(Soil(0.9f), FVector(160.f, 50.f, 40.f), Base + FVector(100.f, 70.f, 12.f), FRotator(0.f, 0.f, 10.f));
				}
			}
		}
		// The field's walls (a gate gap east).
		for (const FWRGraveData::FWall& W : D.Walls)
		{
			if (W.Field != F.Id) continue;
			for (int32 i = 0; i + 1 < W.Points.Num(); ++i)
			{
				const FVector L0 = B.Local(FVector(W.Points[i], 0.f)), L1 = B.Local(FVector(W.Points[i + 1], 0.f));
				DryWall(B, FVector2D(L0.X, L0.Y), FVector2D(L1.X, L1.Y), 90.f, A->Index * 71 + i);
			}
		}
	}

	// ---------------------------------------------------------------- trees (one 200 m cell)
	void Trees(FBuild& B, AWRGraveScenery* A)
	{
		// Leaning grey hawthorns (G1, G3; pushed east by the sea wind) and dead elms (G3). The yews are the server's
		// nodes (they can be cut for yew logs and grow back).
		const FWRGraveData& D = FWRGraveData::Get();
		const int32 Cx = A->Index / 1000 - (A->Index < 0 ? 1 : 0), Cy = A->Index - (A->Index / 1000) * 1000;
		for (const FWRGraveData::FTree& T : D.Trees)
		{
			if (T.Kind == TEXT("yew")) continue;
			if (FMath::FloorToInt(T.Base.X / 20000.f) * 1000 + FMath::FloorToInt(T.Base.Y / 20000.f) != A->Index) continue;
			const FVector L = B.Local(T.Base);
			const FVector Base = GAt(B, L.X, L.Y, -8.f);
			if (T.Kind == TEXT("hawthorn"))
			{
				// Lean along +X (the kit's lean axis) = east, turned a little either way.
				B.Tree(TEXT("hawthorn"), int32(T.Yaw) % 4, Base, T.Height, FRotator(0.f, -A->GetActorRotation().Yaw + 20.f * (B.Rand(int32(T.Yaw)) - 0.5f), 0.f), 180.f, 14.f);
			}
			else B.Tree(TEXT("dead_elm"), int32(T.Yaw) % 4, Base, T.Height, FRotator(0.f, T.Yaw, 0.f), 600.f);
		}
		(void)Cx; (void)Cy;
	}

	// ---------------------------------------------------------------- the chalk stream
	void Stream(FBuild& B, AWRGraveScenery* A)
	{
		// The winterbourne in the ford vale (GY H4): a clear chalk stream 2-5 m wide over pale flint gravel from a spring at
		// the Chapel Heights' foot to the sea. A plank footbridge where the Coast Road crosses it; the Ford of Washing's
		// stepping stones are the hand site's own.
		const FWRGraveData& D = FWRGraveData::Get();
		TArray<FVector> Pts;
		TArray<float> W;
		for (int32 i = 0; i < D.Stream.Num(); ++i)
		{
			const FVector4f& P = D.Stream[i];
			const FVector L = B.Local(FVector(P.X, P.Y, P.Z));
			// Densify between points so the ribbon follows the carved bed.
			if (i > 0)
			{
				const FVector Prev = Pts.Last();
				const int32 N = FMath::Max(1, FMath::CeilToInt(float(FVector::Dist2D(Prev, L)) / 300.f));
				for (int32 k = 1; k < N; ++k) { Pts.Add(FMath::Lerp(Prev, L, float(k) / N)); W.Add(FMath::Lerp(W.Last(), P.W + 60.f, float(k) / N)); }
			}
			Pts.Add(L);
			W.Add(P.W + 60.f);
		}
		WaterRibbon(A, Pts, W);
		// The gravel bed showing through: pale flint stones scattered along it.
		for (int32 i = 0; i < Pts.Num(); i += 2) for (int32 k = 0; k < 3; ++k) B.K.Add(Chalk(0.8f + 0.2f * B.Rand(i * 3 + k)), FVector(18.f, 14.f, 8.f), Pts[i] + FVector((B.Rand(i * 7 + k) - 0.5f) * 200.f, (B.Rand(i * 9 + k) - 0.5f) * W[i] * 0.7f, -30.f), FRotator(0.f, 60.f * k, 0.f));
		// The Coast Road's footbridge: where the road's line crosses the stream.
		if (const FWRGraveData::FPath* Road = D.FindRoad(TEXT("coast_road")))
		{
			for (int32 i = 0; i + 1 < Road->Points.Num(); ++i)
			{
				const FVector A0 = B.Local(Road->Points[i]), A1 = B.Local(Road->Points[i + 1]);
				for (int32 s = 0; s + 1 < Pts.Num(); ++s)
				{
					FVector Hit, Hit2;
					FMath::SegmentDistToSegment(FVector(A0.X, A0.Y, 0.f), FVector(A1.X, A1.Y, 0.f), FVector(Pts[s].X, Pts[s].Y, 0.f), FVector(Pts[s + 1].X, Pts[s + 1].Y, 0.f), Hit, Hit2);
					if (FVector::Dist2D(Hit, Hit2) > 50.f) continue;
					const FVector Dir = (A1 - A0).GetSafeNormal2D();
					const FRotator R(0.f, FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)), 0.f);
					const FVector C(Hit.X, Hit.Y, Pts[s].Z + 70.f);
					const float Span = W[s] + 300.f;
					for (int32 p = -4; p <= 4; ++p) B.K.Add(OldWood(0.8f + 0.1f * (p % 2)), FVector(26.f, 200.f, 7.f), C + R.RotateVector(FVector(p * 30.f * Span / 270.f, 0.f, 0.f)), R);
					for (const float S2 : { -1.f, 1.f }) B.K.Add(Timber(0.7f), FVector(Span, 16.f, 22.f), C + R.RotateVector(FVector(0.f, S2 * 80.f, -14.f)), R);
					B.Box(C, FVector(Span * 0.5f, 100.f, 8.f), R);
					s = Pts.Num();
				}
			}
		}
	}

	// ---------------------------------------------------------------- the Bone Pits and the lime pits
	void BonePit(FBuild& B, AWRGraveScenery* A)
	{
		// An open burial pit of the Bone Pits (G3): bones heaped on its floor where the overflow was laid, lime-white at
		// the edges. The first pit has the timber jib crane that lowered them, its rope and bone cradle swaying; the third
		// a cart tipped at the lip.
		const FWRGraveData& D = FWRGraveData::Get();
		if (!D.BonePits.IsValidIndex(A->Index)) return;
		const float R = D.BonePits[A->Index].W, Dep = D.BonePitDepth[A->Index];
		BonePile(B, GAt(B, 0.f, 0.f), R * 0.55f, A->Index * 97, 6);
		for (int32 k = 0; k < 4; ++k) BonePile(B, GAt(B, FMath::Cos(k * 1.7f) * R * 0.5f, FMath::Sin(k * 1.7f) * R * 0.5f), R * 0.25f, A->Index * 97 + k * 13, 2);
		if (A->Index == 0)
		{
			const FVector Foot = GAt(B, R + 300.f, 0.f);
			B.K.Add(Timber(0.7f), FVector(60.f, 60.f, 700.f), Foot + FVector(0, 0, 350.f));
			for (int32 l = 0; l < 3; ++l) B.K.Rod(Timber(0.65f), Foot + FRotator(0.f, 120.f * l + 30.f, 0.f).Vector() * 220.f, Foot + FVector(0, 0, 300.f), 18.f);
			const FVector Tip = Foot + FVector(-R - 200.f, 0.f, 650.f);
			B.K.Rod(Timber(0.7f), Foot + FVector(0, 0, 600.f), Tip, 20.f);
			B.K.Rod(Timber(0.65f), Foot + FVector(0, 0, 200.f), Foot + FVector(-260.f, 0.f, 600.f), 14.f);
			B.Line(Tip, Tip - FVector(0, 0, 500.f + Dep), 2.f, 0.f, RopeC(), 2);
			BonePile(B, Tip - FVector(0, 0, 520.f + Dep * 0.5f), 70.f, 333, 1);
			B.Box(Foot + FVector(0, 0, 350.f), FVector(40.f, 40.f, 350.f));
		}
		if (A->Index == 2)
		{
			const FVector C = GAt(B, R + 150.f, 200.f, 50.f);
			B.K.Add(OldWood(0.7f), FVector(240.f, 130.f, 70.f), C, FRotator(-25.f, 200.f, 0.f));
			for (const float S2 : { -1.f, 1.f }) B.K.Disc(OldWood(0.6f), C + FRotator(0.f, 200.f, 0.f).RotateVector(FVector(40.f, S2 * 80.f, -30.f)), FRotator(0.f, 290.f, 0.f).Vector(), 120.f, 10.f);
			B.Box(C, FVector(120.f, 70.f, 50.f), FRotator(-25.f, 200.f, 0.f));
		}
	}

	void LimePit(FBuild& B, AWRGraveScenery* A)
	{
		// A quicklime slurry basin (G3; the hazard is WRGraveHazards'): a milky white surface sitting a hand below the rim,
		// a cracked white crust round it, a few animal bones gone chalky at its edge. Two have plank bridges across.
		const FWRGraveData& D = FWRGraveData::Get();
		if (!D.Lime.IsValidIndex(A->Index)) return;
		const float R = D.Lime[A->Index].Radius;
		B.K.Disc(Lime(0.95f), FVector(0, 0, 0.f), FVector::UpVector, R * 2.f - 40.f, 6.f);
		B.K.Disc(FSurf::Glaze(Rgb(236, 234, 226) * 0.66f, 0.3f), FVector(0, 0, 3.f), FVector::UpVector, R * 1.6f, 2.f);
		for (int32 k = 0; k < 14; ++k)
		{
			const float Ang = k * 2.f * PI / 14.f;
			B.K.Add(Lime(1.f), FVector(90.f + 40.f * B.Rand(k), 50.f, 8.f), GAt(B, FMath::Cos(Ang) * (R + 30.f), FMath::Sin(Ang) * (R + 30.f), 2.f), FRotator(0.f, FMath::RadiansToDegrees(Ang) + 90.f, 0.f));
		}
		if (A->Index % 3 == 0) LongBone(B, FVector(R * 0.6f, -40.f, 4.f), FVector(R * 0.6f + 40.f, 10.f, 6.f), 2.2f, Rgb(237, 235, 224) * 0.7f);
		if (A->Index == 0 || A->Index == 3)
		{
			const float Span = R * 2.f + 200.f;
			for (int32 p = 0; p < 3; ++p) B.K.Add(OldWood(0.75f + 0.1f * p), FVector(Span, 34.f, 8.f), FVector(0.f, -36.f + p * 36.f, GZ(B, -R - 100.f, 0.f) + 12.f), FRotator(0.f, 0.f, 0.f));
			B.Box(FVector(0.f, 0.f, GZ(B, -R - 100.f, 0.f) + 12.f), FVector(Span * 0.5f, 55.f, 6.f));
		}
	}
}

// ================================================================================================ the dispatch

void AWRGraveScenery::ApplyState(bool bOn)
{
	bStateShown = bOn;
	if (StateOn) StateOn->SetVisibility(bOn, true);
	if (StateOff) StateOff->SetVisibility(!bOn, true);
	if (StateBox) StateBox->SetCollisionEnabled(bOn ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
}

void AWRGraveScenery::BuildGrave()
{
	using namespace WRGraveSceneryImpl;
	const FName K = Kind;
	const float Cull = IsLandmark(K) ? 0.f
		: K == TEXT("room") || K == TEXT("link") ? WRTune(TEXT("graveyardWorld.underCull"), 16000.f)
		: IsKit(K) ? WRTune(TEXT("graveyardWorld.kitCull"), 30000.f)
		: WRTune(TEXT("graveyardWorld.siteCull"), 60000.f);
	FBuild B(this, Cull);
	bool bBuilt = true;
	if (K == TEXT("chapel")) WRGraveLandmarks::Chapel(B, this);
	else if (K == TEXT("great_yew")) WRGraveLandmarks::GreatYew(B, this);
	else if (K == TEXT("belfry")) WRGraveLandmarks::Belfry(B, this);
	else if (K == TEXT("bone_giant")) WRGraveLandmarks::BoneGiant(B, this);
	else if (K == TEXT("stair")) WRGraveLandmarks::LanternStair(B, this);
	else if (K == TEXT("lantern_stair")) { /* the site row: the Stair itself is the "stair" piece */ }
	else if (K == TEXT("camp")) WRGraveLandmarks::Camp(B, this);
	else if (K == TEXT("road")) WRGraveLandmarks::Road(B, this);
	else if (K == TEXT("field")) WRGraveLandmarks::Field(B, this);
	else if (K == TEXT("trees")) WRGraveLandmarks::Trees(B, this);
	else if (K == TEXT("stream")) WRGraveLandmarks::Stream(B, this);
	else if (K == TEXT("bone_pit")) WRGraveLandmarks::BonePit(B, this);
	else if (K == TEXT("lime_pit")) WRGraveLandmarks::LimePit(B, this);
	else if (K == TEXT("room") || K == TEXT("link")) bBuilt = WRGraveCatacombs::Build(B, this);
	else bBuilt = WRGraveSites::Build(B, this);
	if (!bBuilt)
	{
		UE_LOG(LogWildreach, Warning, TEXT("GRAVEYARD scenery: no builder for kind %s (%s)"), *K.ToString(), *SiteId.ToString());
		return;
	}
	B.Finish(FLinearColor(0.32f, 0.3f, 0.27f));
	if (Cull > 0.f && GetNetMode() != NM_DedicatedServer)
	{
		for (UActorComponent* Cmp : GetComponents())
		{
			if (UInstancedStaticMeshComponent* I = Cast<UInstancedStaticMeshComponent>(Cmp)) I->SetCullDistances(0, int32(Cull));
		}
	}
	// A state part (the chapel crypt's slab, the seventh door) shows as the Graveyard's state is now.
	if (!StateFlag.IsNone()) ApplyState(false);
}
