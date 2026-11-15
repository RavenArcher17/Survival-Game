// The Crystal caves' furniture under the ground (cooks/biome-crystal.md 2.2-2.5, 2.7, 2.20; cooks/build-biome-crystal.md
// 2.3-2.5): every hall's keystone chime frame and its Wardcarver ladders, the pools' water and mineral rims, the Hall of
// Beams' selenite (its three great beams walkable, with the stairs up to them), the Ward Chambers' doorways and their
// hexafoils, the Severance stair, the cave hand sites H1-H8, the Silent Vault's six rooms and the alcoves' kit. The
// caves' floors are their halls' (z 0 at each piece: positions come from the data, never a trace: the rock's collision
// may still be cooking when these are built).

#include "WRCrystalWorld.h"
#include "WRCrystalLook.h"
#include "WRCrystalTypes.h"
#include "Wildreach.h"

namespace WRCrystalUnder
{
	using namespace WRCrystalLook;
	using FData = FWRCrystalData;

	/** The wall of the caves' air from a point along a direction (world cm): marched out until the air ends. */
	FVector WallFrom(const FVector& From, const FVector& Dir, float Max = 6000.f)
	{
		const FData& D = FData::Get();
		const FVector U = Dir.GetSafeNormal();
		float Lo = 0.f, Hi = -1.f;
		for (float T = 50.f; T < Max; T += 50.f) { if (D.Air(From + U * T) >= 0.f) { Hi = T; break; } Lo = T; }
		if (Hi < 0.f) return From + U * Max;
		for (int32 b = 0; b < 6; ++b) { const float M = (Lo + Hi) * 0.5f; (D.Air(From + U * M) >= 0.f ? Hi : Lo) = M; }
		return From + U * Lo;
	}

	/** The keystone chime (crystal 2.1): the Wardcarvers' hall bell at the far end under the crystal cluster: two bronze
	 *  uprights on a basalt plinth, a crossbar with its hexafoil, a 2.2 m crystal bar hung from two chains, the striker on
	 *  its hook. Struck, it lights the hall for 20 game minutes (WRCrystalWorld). */
	void KeystoneFrame(FBuild& B, const FVector& At, float Yaw)
	{
		const FRotator R(0.f, Yaw, 0.f);
		const FVector Side = R.RotateVector(FVector(0.f, 1.f, 0.f)), Fwd = R.RotateVector(FVector(1.f, 0.f, 0.f));
		B.K.Add(Dressed(), FVector(120.f, 260.f, 40.f), At + FVector(0.f, 0.f, 20.f), R);
		for (const float S : { -1.f, 1.f }) B.K.Rod(Bronze(), At + Side * 110.f * S + FVector(0, 0, 40.f), At + Side * 110.f * S + FVector(0, 0, 360.f), 9.f);
		B.K.Rod(Bronze(1.1f), At - Side * 125.f + FVector(0, 0, 355.f), At + Side * 125.f + FVector(0, 0, 355.f), 10.f);
		Hexafoil(B, Bronze(1.25f), At + FVector(0, 0, 355.f) + Fwd * 9.f, Fwd, 26.f, 2.f);
		for (const float S : { -0.5f, 0.5f }) B.K.Rod(Bronze(0.7f), At + Side * 100.f * S + FVector(0, 0, 350.f), At + Side * 100.f * S + FVector(0, 0, 290.f), 2.f);
		HexAlong(B, AmethystPale(0.25f), At - Side * 110.f + FVector(0, 0, 280.f), Side, 14.f, 220.f, 0.f);
		B.K.Rod(Timber(1.2f), At + Side * 125.f + FVector(0, 0, 200.f) + Fwd * 20.f, At + Side * 125.f + FVector(0, 0, 120.f) + Fwd * 20.f, 4.f);
		B.K.Add(Wool(0.9f), FVector(16.f, 16.f, 22.f), At + Side * 125.f + FVector(0, 0, 112.f) + Fwd * 20.f, FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		B.Box(At + FVector(0, 0, 180.f), FVector(60.f, 130.f, 180.f), R);
	}

	/** Water in a basin: its surface (teal and steaming when scalding, black and still in a lake, brown in a wallow) and
	 *  the mineral rim it laid down at its edge (gold rimstone round the hot ones). Local to the piece. */
	void PoolLook(FBuild& B, const FData::FPool& P)
	{
		const FVector C = B.Local(P.At);
		const bool bHot = P.Kind == TEXT("scald");
		const FSurf Water = bHot ? SteamWater() : P.Kind == TEXT("wallow") ? FSurf::Glaze(Rgb(72, 56, 40) * 0.5f, 0.2f) : DarkWater();
		B.K.Add(Water, FVector(P.Rx * 2.f, P.Ry * 2.f, 6.f), C - FVector(0, 0, 3.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		const int32 N = FMath::Max(10, int32((P.Rx + P.Ry) / 60.f));
		for (int32 i = 0; i < N; ++i)
		{
			const float A = 2.f * PI * i / N;
			const FVector Q = C + FVector(FMath::Cos(A) * P.Rx, FMath::Sin(A) * P.Ry, 0.f);
			B.K.Add(bHot ? Mineral(0.9f + 0.2f * Hash(i, 3)) : Dressed(0.7f), FVector(2.f * PI * (P.Rx + P.Ry) * 0.5f / N + 10.f, 40.f, 22.f), Q + FVector(0, 0, 5.f), FRotator(0.f, FMath::RadiansToDegrees(A) + 90.f, 0.f));
		}
		if (bHot) B.Lamp(C + FVector(0, 0, 120.f), FLinearColor(FColor(140, 230, 220)), 300.f, FMath::Max(P.Rx, P.Ry) * 2.f);
	}

	/** A Wardcarver ladder up a geode's wall (decor): bronze rails on pins drilled into the rock, rope rungs. */
	void Ladder(FBuild& B, const FVector& Foot, const FVector& Out, float H)
	{
		const FVector Side = FVector::CrossProduct(Out, FVector::UpVector).GetSafeNormal();
		for (const float S : { -1.f, 1.f }) B.K.Rod(Bronze(0.8f), Foot + Side * 25.f * S + Out * 15.f, Foot + Side * 25.f * S + Out * 15.f + FVector(0, 0, H), 3.f);
		for (float Z = 30.f; Z < H; Z += 35.f) B.K.Rod(Rope(), Foot + Side * -25.f + Out * 15.f + FVector(0, 0, Z), Foot + Side * 25.f + Out * 15.f + FVector(0, 0, Z), 2.f);
		for (float Z = 60.f; Z < H; Z += 180.f) for (const float S : { -1.f, 1.f }) B.K.Rod(Bronze(0.6f), Foot + Side * 25.f * S + FVector(0, 0, Z), Foot + Side * 25.f * S + Out * 18.f + FVector(0, 0, Z), 2.5f);
	}

	/** The Hall of Beams' selenite (crystal 2.3 L4): a hundred beams up to 14 m crossing at angles from wall to wall,
	 *  white with grey growth bands; the three great beams bridge the whole width 20 m over the pools and are walkable,
	 *  each reached by a stair cut up the wall at its north end; the Beam Shrine's deck on the middle one. */
	void BeamHall(FBuild& B, AWRCrystalScenery* A, const FData::FHall& H)
	{
		const FData& D = FData::Get();
		const FVector C = H.Centre();
		FRandomStream R(4242);
		for (int32 i = 0; i < 90; ++i)
		{
			// Two wall points: a random chord of the hall, kept off the floor's walking band and short of 16 m.
			const FVector U1 = FVector(R.FRandRange(-1.f, 1.f), R.FRandRange(-1.f, 1.f), R.FRandRange(-0.1f, 0.9f)).GetSafeNormal();
			const FVector W1 = WallFrom(C, U1, 12000.f);
			const FVector Dir = (FVector(R.FRandRange(-1.f, 1.f), R.FRandRange(-1.f, 1.f), R.FRandRange(-0.6f, 0.6f)) - U1 * 0.8f).GetSafeNormal();
			const float Len = R.FRandRange(400.f, 1400.f);
			const FVector W2 = W1 + Dir * Len;
			if (W2.Z < H.At.Z + 450.f || W1.Z < H.At.Z + 250.f || D.Air(W2) > 0.f) continue;
			const float Th = R.FRandRange(40.f, 140.f);
			const FVector L1 = B.Local(W1 - U1 * 60.f), L2 = B.Local(W2);
			HexAlong(B, Selenite(0.04f), L1, L2 - L1, Th * 0.5f, (L2 - L1).Size(), R.FRandRange(0.f, 60.f));
			// Grey growth bands round it every metre or so.
			for (float T = 100.f; T < (L2 - L1).Size() - 50.f; T += R.FRandRange(80.f, 160.f)) HexAlong(B, SeleniteBand(), L1 + (L2 - L1).GetSafeNormal() * T, L2 - L1, Th * 0.52f, 6.f, 0.f);
		}
		for (int32 b = 0; b < D.Beams.Num(); ++b)
		{
			const FData::FBeam& Bm = D.Beams[b];
			if (Bm.Hall != H.Id) continue;
			const FVector P0 = B.Local(Bm.A), P1 = B.Local(Bm.B);
			HexAlong(B, Selenite(0.06f), P0 - (P1 - P0).GetSafeNormal() * 400.f, P1 - P0, Bm.Thick * 0.5f, (P1 - P0).Size() + 800.f, 30.f);
			for (float T = 0.f; T < (P1 - P0).Size(); T += 140.f) HexAlong(B, SeleniteBand(), P0 + (P1 - P0).GetSafeNormal() * T, P1 - P0, Bm.Thick * 0.51f, 5.f, 30.f);
			// The walking face: its top flat (a worn crest), the slope gentle enough to walk.
			B.SegBox(P0 + FVector(0, 0, Bm.Thick * 0.42f), P1 + FVector(0, 0, Bm.Thick * 0.42f), Bm.Thick * 0.38f, 10.f);
			// The stair up to it, cut into the north wall from the floor (it rests on the rock; 35 degrees).
			const FVector Foot(P0.X, P0.Y + 2600.f, 10.f);
			const FVector Wall0 = B.Local(WallFrom(A->GetActorTransform().TransformPosition(FVector(P0.X, 0.f, 150.f)), FVector(0.f, -1.f, 0.f), 9000.f));
			const FVector Head(P0.X, P0.Y, P0.Z + Bm.Thick * 0.42f);
			const FVector Base(Wall0.X + 100.f, FMath::Min(Head.Y + 2600.f, Wall0.Y + 1600.f), 0.f);
			const int32 Steps = FMath::Max(8, int32(Head.Z / 22.f));
			for (int32 s = 0; s <= Steps; ++s)
			{
				const FVector T = FMath::Lerp(Base, Head, float(s) / Steps);
				B.K.Add(Dressed(0.85f + 0.25f * Hash(b, s)), FVector(140.f, 50.f, T.Z + 30.f), FVector(T.X, T.Y, (T.Z - 30.f) * 0.5f), FRotator::ZeroRotator);
			}
			B.SegBox(Base, Head, 70.f, 10.f);
			(void)Foot;
			if (b == 1)
			{
				// The Beam Shrine's deck lashed onto the middle beam over the steam (its brazier: the shrine).
				const FVector Mid = (P0 + P1) * 0.5f + FVector(0, 0, Bm.Thick * 0.45f);
				B.K.Add(Timber(), FVector(380.f, 380.f, 12.f), Mid + FVector(0, 0, 6.f));
				B.Box(Mid + FVector(0, 0, 6.f), FVector(190.f, 190.f, 6.f));
				for (const float S : { -1.f, 1.f }) B.K.Rod(Rope(), Mid + FVector(-190.f, 190.f * S, 0.f), Mid + FVector(190.f, 190.f * S, 0.f) + FVector(0, 0, -Bm.Thick * 0.9f), 3.f);
			}
		}
	}

	void HallFurniture(FBuild& B, AWRCrystalScenery* A)
	{
		const FData& D = FData::Get();
		if (!D.Halls.IsValidIndex(A->Index)) return;
		const FData::FHall& H = D.Halls[A->Index];
		const int32 Seed = A->Index * 29 + 11;
		if (H.bKeystone)
		{
			const FVector K = B.Local(H.Keystone);
			KeystoneFrame(B, FVector(K.X, K.Y, 0.f), FMath::RadiansToDegrees(FMath::Atan2(-K.Y, -K.X)));
		}
		for (const FData::FPool& P : D.Pools) if (P.Hall == H.Id) PoolLook(B, P);
		if (H.Zone == TEXT("C2") && H.Kind == TEXT("geode") && !H.bCup)
		{
			const FVector Out = FRotator(0.f, H.Yaw + 90.f + 60.f * Hash(Seed, 1), 0.f).RotateVector(FVector(1.f, 0.f, 0.f));
			const FVector Wall = WallFrom(H.At + FVector(0, 0, 120.f), Out, 8000.f);
			Ladder(B, B.Local(Wall) - Out * 30.f - FVector(0, 0, 120.f), -Out, FMath::Min(900.f, H.H * 0.45f));
		}
		if (H.Kind == TEXT("beams")) BeamHall(B, A, H);
		if (H.Arena == TEXT("mirrored_twins"))
		{
			// The Archive of Wards: four pillars, and the archive itself: lead keystone plates shelved along the walls.
			for (int32 p = 0; p < 4; ++p)
			{
				const FVector P((p % 2 ? 1.f : -1.f) * H.Rx * 0.45f, (p < 2 ? 1.f : -1.f) * H.Ry * 0.45f, 0.f);
				B.K.Add(Dressed(), FVector(180.f, 180.f, H.H), P + FVector(0, 0, H.H * 0.5f));
				B.Box(P + FVector(0, 0, H.H * 0.5f), FVector(90.f, 90.f, H.H * 0.5f));
				Hexafoil(B, WardGlow(0.25f), P + FVector(91.f, 0.f, 260.f), FVector(1.f, 0.f, 0.f), 40.f, 2.f);
			}
			for (int32 s = 0; s < 6; ++s)
			{
				const float Y = -H.Ry * 0.7f + s * H.Ry * 0.28f;
				for (int32 r = 0; r < 4; ++r)
				{
					B.K.Add(Dressed(0.8f), FVector(60.f, 300.f, 8.f), FVector(-H.Rx + 40.f, Y, 80.f + r * 80.f));
					for (int32 k = 0; k < 6; ++k) B.K.Add(Lead(0.8f + 0.3f * Hash(s * 10 + r, k)), FVector(40.f, 4.f, 46.f), FVector(-H.Rx + 40.f, Y - 120.f + k * 48.f, 108.f + r * 80.f), FRotator(0.f, 0.f, 6.f * (Hash(s, k) - 0.5f)));
				}
			}
		}
		if (H.Arena == TEXT("lyrathis"))
		{
			// The Prism Hall's dais and its Arcane altar (the path unlocks there), four broken pillars (cover from the
			// Sundering Flare). The eight turntables and their prisms are the fight's machines (WRCrystalBosses).
			B.K.Add(Polished(1.1f), FVector(1600.f, 1600.f, 60.f), FVector(0, 0, 30.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			B.K.Add(Bronze(), FVector(1640.f, 1640.f, 10.f), FVector(0, 0, 62.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			B.K.Add(Polished(1.3f), FVector(1500.f, 1500.f, 6.f), FVector(0, 0, 66.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			B.Box(FVector(0, 0, 33.f), FVector(560.f, 560.f, 33.f));
			B.K.Add(Dressed(1.2f), FVector(180.f, 120.f, 110.f), FVector(-500.f, 0.f, 120.f));
			Hexafoil(B, WardGlow(0.4f), FVector(-500.f, 0.f, 176.f), FVector::UpVector, 50.f, 2.f);
			for (int32 p = 0; p < 4; ++p)
			{
				const float Ang = 2.f * PI * (p + 0.5f) / 4.f;
				const FVector P(FMath::Cos(Ang) * 2000.f, FMath::Sin(Ang) * 2000.f, 0.f);
				const float Hh = 500.f + 600.f * Hash(Seed, p);
				B.K.Add(Dressed(), FVector(260.f, 260.f, Hh), P + FVector(0, 0, Hh * 0.5f), FRotator(0.f, Ang * 57.3f, 0.f));
				B.Box(P + FVector(0, 0, Hh * 0.5f), FVector(130.f, 130.f, Hh * 0.5f), FRotator(0.f, Ang * 57.3f, 0.f));
				for (int32 r = 0; r < 4; ++r) B.Rock(r + p * 4, FVector(140.f, 110.f, 90.f), P + FVector((Hash(p, r) - 0.5f) * 500.f, (Hash(p, r + 5) - 0.5f) * 500.f, 0.f), FRotator(0.f, Hash(p, r) * 360.f, 0.f), Rgb(42, 42, 48) * 0.8f, false, 10.f);
			}
		}
		if (H.Arena == TEXT("echo_warden"))
		{
			// The Hall of the Door: heaps of throwing stones behind the four chime stones at the compass points (the chimes
			// themselves are the Echo Warden's machines, AWRCrystalBossProp "chime": they ring, swing and break), and the
			// Vault Door's lead ring in the south-west wall.
			for (int32 c = 0; c < 4; ++c)
			{
				const float Ang = c * PI * 0.5f;
				const FVector P(FMath::Cos(Ang) * H.Rx * 0.62f, FMath::Sin(Ang) * H.Ry * 0.62f, 0.f);
				for (int32 s = 0; s < 8; ++s) B.Rock(s + c * 8, FVector(30.f, 26.f, 22.f), P * 1.3f + FVector((Hash(c, s) - 0.5f) * 160.f, (Hash(c, s + 9) - 0.5f) * 160.f, 0.f), FRotator(0.f, Hash(c, s) * 360.f, 0.f), Rgb(40, 40, 46) * 0.7f, false, 3.f);
			}
		}
		if (H.Shrine == TEXT("great"))
		{
			// The Great Hexafoil: the floor one hexafoil 8 m across inlaid in bronze; Saga Venn's bench by the wall with
			// the keystone plate she is carving.
			Hexafoil(B, Bronze(1.15f), FVector(0.f, 0.f, 1.f), FVector::UpVector, 400.f, 1.5f);
			B.K.Add(Timber(), FVector(220.f, 90.f, 10.f), FVector(-H.Rx + 120.f, 300.f, 85.f));
			for (const float S : { -1.f, 1.f }) B.K.Add(Dressed(), FVector(30.f, 80.f, 80.f), FVector(-H.Rx + 120.f + S * 90.f, 300.f, 40.f));
			B.K.Add(Lead(), FVector(70.f, 70.f, 6.f), FVector(-H.Rx + 120.f, 300.f, 94.f));
			Hexafoil(B, Bronze(), FVector(-H.Rx + 120.f, 300.f, 97.f), FVector::UpVector, 28.f, 1.f);
			B.K.Rod(Bronze(1.2f), FVector(-H.Rx + 160.f, 330.f, 94.f), FVector(-H.Rx + 180.f, 350.f, 96.f), 2.f);
		}
		if (H.IsHewn() && H.Zone == TEXT("C5")) B.Lamp(FVector(0, 0, H.H * 0.7f), LilacLight(), 180.f, FMath::Max(H.Rx, H.Ry) * 2.2f);
	}

	/** A Ward Chamber doorway at a passage's end (crystal 2.2 C5): dressed jambs and lintel, the hexafoil carved over it
	 *  and glowing faintly ward-violet (it brightens in a field's tell: WRCrystalHazards). */
	void Doorway(FBuild& B, const FVector& At, const FVector& Along, float W, float H)
	{
		const FVector Side = FVector::CrossProduct(Along, FVector::UpVector).GetSafeNormal();
		const FRotator R = Along.Rotation();
		for (const float S : { -1.f, 1.f }) B.K.Add(Dressed(1.1f), FVector(60.f, 50.f, H), At + Side * (W * 0.5f - 25.f) * S + FVector(0, 0, H * 0.5f), FRotator(0.f, R.Yaw, 0.f));
		B.K.Add(Dressed(1.15f), FVector(60.f, W, 50.f), At + FVector(0, 0, H - 25.f), FRotator(0.f, R.Yaw, 0.f));
		Hexafoil(B, WardGlow(0.3f), At + FVector(0, 0, H - 25.f) - Along * 31.f, -Along, FMath::Min(70.f, W * 0.18f), 2.f);
	}

	void TubeFurniture(FBuild& B, AWRCrystalScenery* A)
	{
		const FData& D = FData::Get();
		if (!D.Tubes.IsValidIndex(A->Index)) return;
		const FData::FTube& T = D.Tubes[A->Index];
		if (T.Pts.Num() < 3) return;
		if (T.Kind == TEXT("passage"))
		{
			const int32 N = T.Pts.Num();
			Doorway(B, B.Local(T.Pts[0]), (T.Pts[2] - T.Pts[0]).GetSafeNormal2D(), T.W, T.H);
			Doorway(B, B.Local(T.Pts[N - 1]), (T.Pts[N - 3] - T.Pts[N - 1]).GetSafeNormal2D(), T.W, T.H);
			// Bronze lamp brackets (the Wardcarvers lit nothing: their light was sound) every 30 m on the left wall.
			for (int32 i = 10; i + 10 < N; i += 10)
			{
				const FVector P = B.Local(T.Pts[i]), Dir = (T.Pts[i + 1] - T.Pts[i]).GetSafeNormal2D();
				const FVector Side = FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal();
				const FVector Wall = P + Side * (T.W * 0.5f - 6.f) + FVector(0, 0, 220.f);
				B.K.Rod(Bronze(0.8f), Wall, Wall - Side * 40.f, 3.f);
				Point(B, AmethystPale(0.1f), Wall - Side * 40.f, FVector(0, 0, 1.f), 40.f, 9.f, 0.f);
			}
			return;
		}
		if (T.Id == TEXT("organ"))
		{
			// Under each vent the moonlight comes down in a pale column at night; by each, the ochre dots of its resonant
			// point on the wall (crystal 2.3 L5: eleven).
			for (const FData::FShaft& V : D.Vents)
			{
				float Fl = 0.f, Tt = 0.f; int32 Seg = 0; FVector Dir;
				const float Lat = T.Nearest(V.At, Fl, Seg, Tt, Dir);
				(void)Lat;
				const FVector Floor = B.Local(FVector(V.At.X, V.At.Y, Fl));
				B.N.Rod(FSurf::Lit(Rgb(170, 190, 230) * 0.4f, 0.25f), Floor, Floor + FVector(0, 0, T.R * 1.5f), V.R * 1.4f);
				const FVector Side = FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal();
				for (int32 d = 0; d < 7; ++d)
				{
					const float Ang = 2.f * PI * d / 7.f;
					B.K.Add(Ochre(0.9f + 0.2f * Hash(d, V.Z1)), FVector(4.f, 13.f, 13.f), Floor + Side * (T.R * 0.86f) + FVector(0, FMath::Cos(Ang) * 30.f, 260.f + FMath::Sin(Ang) * 30.f), FRotator(0.f, Side.Rotation().Yaw, 0.f), EWRLookMesh::Cylinder);
				}
			}
			return;
		}
	}

	/** The Severance stair (W1s): the Wardcarvers' newel stair from the guard room's floor down 138 m to the Prism Hall's
	 *  level, its treads from a dressed newel to the shaft's wall, a hexafoil on the newel every turn. */
	void SeveranceStair(FBuild& B, AWRCrystalScenery* A)
	{
		const FData& D = FData::Get();
		if (!D.Shafts.IsValidIndex(A->Index)) return;
		const FData::FShaft& S = D.Shafts[A->Index];
		const float Top = 0.f, Bottom = S.Z0 - S.Z1;
		B.K.Add(Dressed(), FVector(280.f, 280.f, Top - Bottom + 200.f), FVector(0, 0, (Top + Bottom) * 0.5f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		B.Box(FVector(0, 0, (Top + Bottom) * 0.5f), FVector(140.f, 140.f, (Top - Bottom) * 0.5f));
		SpiralStair(B, FVector::ZeroVector, S.R, Top - 10.f, Bottom + 10.f, S.R - 170.f, TEXT("stone"), 77, 90.f);
		for (float Z = Top - 600.f; Z > Bottom + 200.f; Z -= 1400.f) Hexafoil(B, WardGlow(0.22f), FVector(141.f, 0.f, Z), FVector(1.f, 0.f, 0.f), 40.f, 2.f);
	}

	// ---- the cave hand sites (crystal 2.4 H1-H8) ----

	/** H1 the Singing School: benches in rows facing a chime wall of 21 tuned crystal bars (long to short, low to high);
	 *  ochre hand-prints on the benches at a child's height. Struck in order they light the hall and open the wall's
	 *  cache (WRCrystalWorld). */
	void SingingSchool(FBuild& B, const FData::FHall& H)
	{
		const float WallX = H.Rx * 0.66f;
		for (int32 r = 0; r < 5; ++r) for (const float S : { -1.f, 1.f })
		{
			const FVector P(-H.Rx * 0.35f + r * 220.f, S * 330.f, 0.f);
			B.K.Add(Dressed(0.9f + 0.15f * Hash(r, int32(S))), FVector(70.f, 420.f, 45.f), P + FVector(0, 0, 22.f));
			B.Box(P + FVector(0, 0, 22.f), FVector(35.f, 210.f, 22.f));
			for (int32 h = 0; h < 3; ++h) B.K.Add(Ochre(), FVector(14.f, 10.f, 1.f), P + FVector(-10.f + 14.f * h, -150.f + 140.f * h, 46.f), FRotator(0.f, 25.f * h, 0.f));
		}
		// The chime frame: a bronze beam on two basalt posts, 21 bars hung from it, longest (lowest) on the left.
		const float Span = 1300.f;
		for (const float S : { -1.f, 1.f }) B.K.Add(Dressed(), FVector(70.f, 70.f, 420.f), FVector(WallX, S * Span * 0.5f, 210.f));
		B.K.Rod(Bronze(), FVector(WallX, -Span * 0.5f, 400.f), FVector(WallX, Span * 0.5f, 400.f), 10.f);
		for (int32 b = 0; b < 21; ++b)
		{
			const float Y = -Span * 0.45f + b * (Span * 0.9f / 20.f);
			const float Len = 300.f - b * 10.f;
			B.K.Rod(Bronze(0.7f), FVector(WallX, Y, 400.f), FVector(WallX, Y, 380.f), 1.5f);
			HexAlong(B, AmethystPale(0.15f), FVector(WallX, Y, 380.f), FVector(0, 0, -1.f), 9.f, Len, 0.f);
		}
		B.Box(FVector(WallX, 0.f, 210.f), FVector(40.f, Span * 0.5f, 210.f));
	}

	/** H2 the Broken Nest: a geode cracked open on purpose, a thousand Geodemite husks round it all facing in, and the
	 *  Wardcarver who opened it dead in the middle, his mallet by his hand. */
	void BrokenNest(FBuild& B)
	{
		const FLinearColor Basal = Rgb(30, 30, 34) * 0.7f;
		for (const float S : { -1.f, 1.f })
		{
			B.Ball(FVector(S * 90.f, 0.f, 180.f), 200.f, Basal, 0.8f);
			Cluster(B, FVector(S * 40.f, 0.f, 180.f), FVector(-S, 0.f, 0.3f), 150.f, 9 + int32(S), Amethyst(0.12f), AmethystPale(0.2f));
		}
		B.Box(FVector(0, 0, 180.f), FVector(280.f, 200.f, 180.f));
		for (int32 h = 0; h < 160; ++h)
		{
			const float Ang = h * 2.39996f, R = 400.f + 30.f * FMath::Sqrt(float(h)) * 4.f;
			const FVector P(FMath::Cos(Ang) * R, FMath::Sin(Ang) * R, 6.f);
			B.K.Add(FSurf::Stone(Rgb(120, 110, 130) * 0.45f, 0.6f), FVector(26.f, 18.f, 12.f), P, FRotator(0.f, FMath::RadiansToDegrees(Ang) + 180.f, 0.f), EWRLookMesh::Cylinder);
		}
		const FLinearColor Bn = BoneC();
		B.Limb(FVector(320.f, 0.f, 15.f), FVector(420.f, 30.f, 15.f), 14.f, 12.f, Bn);
		B.Ball(FVector(440.f, 34.f, 18.f), 11.f, Bn, 0.7f);
		B.K.Add(Wool(0.6f), FVector(110.f, 70.f, 20.f), FVector(360.f, 10.f, 14.f));
		B.K.Rod(Timber(1.2f), FVector(330.f, -60.f, 6.f), FVector(330.f, -130.f, 6.f), 5.f);
		B.K.Add(Bronze(), FVector(28.f, 16.f, 16.f), FVector(330.f, -60.f, 8.f));
	}

	/** H3 the Steam Baths: carved stone tubs round the hot pools, bronze pipes leading the steam to a sweat room built
	 *  against the wall, one tub broken from below where the scalding pool came up into it. */
	void SteamBaths(FBuild& B, const FData::FHall& H)
	{
		for (int32 t = 0; t < 4; ++t)
		{
			const float Ang = PI * 0.25f + t * PI * 0.5f;
			const FVector P(FMath::Cos(Ang) * H.Rx * 0.55f, FMath::Sin(Ang) * H.Ry * 0.55f, 0.f);
			const FRotator R(0.f, FMath::RadiansToDegrees(Ang), 0.f);
			for (const FVector& Wl : { FVector(0.f, -110.f, 0.f), FVector(0.f, 110.f, 0.f) }) B.K.Add(Dressed(1.1f), FVector(280.f, 30.f, 70.f), P + R.RotateVector(Wl) + FVector(0, 0, 35.f), R);
			for (const FVector& Wl : { FVector(-140.f, 0.f, 0.f), FVector(140.f, 0.f, 0.f) }) if (!(t == 2 && Wl.X > 0.f)) B.K.Add(Dressed(1.1f), FVector(30.f, 250.f, 70.f), P + R.RotateVector(Wl) + FVector(0, 0, 35.f), R);
			B.K.Add(t == 2 ? SteamWater() : DarkWater(), FVector(250.f, 190.f, 4.f), P + FVector(0, 0, 50.f), R);
			B.Box(P + FVector(0, 0, 35.f), FVector(140.f, 110.f, 35.f), R);
			B.K.Rod(Bronze(0.85f), P + R.RotateVector(FVector(-140.f, 0.f, 60.f)), FVector(-H.Rx * 0.7f, 0.f, 260.f), 8.f);
		}
		// The sweat room: a hewn hut against the west wall, its door, the pipes entering under its roof.
		const FVector Hut(-H.Rx * 0.72f, 0.f, 0.f);
		for (const FVector& Wl : { FVector(0.f, -260.f, 0.f), FVector(0.f, 260.f, 0.f) }) B.K.Add(Dressed(0.9f), FVector(400.f, 40.f, 300.f), Hut + Wl + FVector(0, 0, 150.f));
		B.K.Add(Dressed(0.9f), FVector(40.f, 160.f, 300.f), Hut + FVector(200.f, -150.f, 150.f));
		B.K.Add(Dressed(0.9f), FVector(40.f, 160.f, 300.f), Hut + FVector(200.f, 150.f, 150.f));
		B.K.Add(Dressed(1.1f), FVector(440.f, 560.f, 30.f), Hut + FVector(0.f, 0.f, 315.f));
		B.Box(Hut + FVector(0.f, -260.f, 150.f), FVector(200.f, 20.f, 150.f));
		B.Box(Hut + FVector(0.f, 260.f, 150.f), FVector(200.f, 20.f, 150.f));
		B.Box(Hut + FVector(0.f, 0.f, 315.f), FVector(220.f, 280.f, 15.f));
	}

	/** H4 the Fallen Chandelier: a 6 m chandelier shattered on a crushed Lumenhorn's bones, a newcomer's broken lute beside
	 *  it (he played under it). The gallery's other chandeliers still hang (WRCrystalHazards). */
	void FallenChandelier(FBuild& B, AWRCrystalScenery* A)
	{
		const FData& D = FData::Get();
		const FData::FChandelier* C = D.Chandeliers.FindByPredicate([](const FData::FChandelier& X) { return X.bFallen; });
		if (!C) return;
		const FVector At = B.Local(FVector(C->Top.X, C->Top.Y, C->Floor));
		B.K.Add(Bronze(0.7f), FVector(380.f, 380.f, 20.f), At + FVector(40.f, 0.f, 30.f), FRotator(14.f, 0.f, 9.f), EWRLookMesh::Cylinder);
		for (int32 s = 0; s < 40; ++s)
		{
			const float Ang = Hash(4, s) * 2.f * PI, R = 60.f + 320.f * Hash(5, s);
			Point(B, Selenite(0.05f), At + FVector(FMath::Cos(Ang) * R, FMath::Sin(Ang) * R, 4.f), FVector(FMath::Cos(Ang), FMath::Sin(Ang), 0.1f + Hash(6, s) * 0.5f), 60.f + 140.f * Hash(7, s), 14.f + 14.f * Hash(8, s), s * 9.f);
		}
		const FLinearColor Bn = BoneC();
		for (int32 r = 0; r < 9; ++r) B.Limb(At + FVector(-150.f + r * 30.f, -20.f, 10.f), At + FVector(-150.f + r * 30.f, 120.f, 40.f), 6.f, 4.f, Bn, 6);
		B.Limb(At + FVector(-200.f, 50.f, 25.f), At + FVector(150.f, 50.f, 25.f), 14.f, 10.f, Bn);
		B.Ball(At + FVector(-260.f, 60.f, 35.f), 40.f, Bn, 0.7f, 1.4f);
		for (int32 t = 0; t < 4; ++t) Point(B, AmethystPale(0.06f), At + FVector(-300.f, 60.f + (t - 1.5f) * 40.f, 50.f), FVector(-0.5f, (t - 1.5f) * 0.4f, 0.6f), 140.f, 22.f, t * 20.f);
		B.K.Add(Timber(1.3f), FVector(70.f, 40.f, 10.f), At + FVector(260.f, -180.f, 6.f), FRotator(0.f, 30.f, 0.f), EWRLookMesh::Cylinder);
		B.K.Rod(Timber(1.1f), At + FVector(260.f, -180.f, 8.f), At + FVector(330.f, -230.f, 10.f), 4.f);
	}

	/** H6 the Guano Mines: the Wardcarvers' saltpetre works under the big roost: leaching vats, bronze evaporating pans on
	 *  stone hearths, guano heaps, a crystal ladder up into the roost vent. */
	void GuanoMines(FBuild& B, const FData::FHall& H)
	{
		for (int32 v = 0; v < 3; ++v)
		{
			const FVector P(-H.Rx * 0.4f + v * 300.f, -H.Ry * 0.45f, 0.f);
			B.K.Add(Dressed(), FVector(220.f, 160.f, 110.f), P + FVector(0, 0, 55.f));
			B.K.Add(FSurf::Glaze(Rgb(110, 100, 80) * 0.45f, 0.2f), FVector(190.f, 130.f, 4.f), P + FVector(0, 0, 106.f));
			B.Box(P + FVector(0, 0, 55.f), FVector(110.f, 80.f, 55.f));
		}
		for (int32 p = 0; p < 2; ++p)
		{
			const FVector P(H.Rx * 0.2f + p * 280.f, H.Ry * 0.4f, 0.f);
			B.K.Add(Dressed(0.8f), FVector(160.f, 160.f, 60.f), P + FVector(0, 0, 30.f));
			B.K.Add(Bronze(0.9f), FVector(190.f, 190.f, 14.f), P + FVector(0, 0, 66.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			B.K.Add(SaltRim(1.1f), FVector(160.f, 160.f, 3.f), P + FVector(0, 0, 72.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		}
		for (int32 g = 0; g < 7; ++g) B.K.Add(Guano(0.9f + 0.2f * Hash(g, 2)), FVector(220.f + 160.f * Hash(g, 3), 200.f, 70.f + 60.f * Hash(g, 4)), FVector((Hash(g, 5) - 0.5f) * H.Rx, (Hash(g, 6) - 0.5f) * H.Ry * 0.6f, 20.f), FRotator(0.f, Hash(g, 7) * 180.f, 0.f), EWRLookMesh::Cone);
		// The crystal ladder: two selenite rails up to the roof's roost vent, rungs of bronze.
		for (const float S : { -1.f, 1.f }) HexAlong(B, Selenite(0.06f), FVector(0.f, S * 30.f, 0.f), FVector(0.f, 0.f, 1.f), 6.f, H.H * 0.9f, 0.f);
		for (float Z = 40.f; Z < H.H * 0.88f; Z += 40.f) B.K.Rod(Bronze(0.8f), FVector(0.f, -30.f, Z), FVector(0.f, 30.f, Z), 2.f);
	}

	/** H7 the Lens Grinder's Room: a bronze lathe with a half-ground doubling-spar lens in its chuck, a rack of grinding
	 *  laps, the lens's twin cracked on the floor, a crystal on the bench whose light through the lens throws two images
	 *  of the bench on the wall (the Twins' lens explained without words). */
	void LensRoom(FBuild& B, const FData::FHall& H)
	{
		B.K.Add(Timber(), FVector(260.f, 90.f, 10.f), FVector(0.f, -H.Ry + 90.f, 90.f));
		for (const float S : { -1.f, 1.f }) B.K.Add(Dressed(), FVector(30.f, 80.f, 85.f), FVector(S * 110.f, -H.Ry + 90.f, 42.f));
		B.K.Add(Bronze(), FVector(160.f, 40.f, 40.f), FVector(-40.f, -H.Ry + 90.f, 118.f));
		B.K.Disc(Bronze(1.1f), FVector(50.f, -H.Ry + 90.f, 125.f), FVector(1.f, 0.f, 0.f), 30.f, 12.f);
		B.K.Disc(FSurf::Glass(Rgb(230, 240, 250) * 0.6f, 0.05f), FVector(62.f, -H.Ry + 90.f, 125.f), FVector(1.f, 0.f, 0.f), 24.f, 6.f);
		Point(B, AmethystPale(0.4f), FVector(110.f, -H.Ry + 90.f, 96.f), FVector(0, 0, 1.f), 26.f, 8.f, 0.f);
		B.Lamp(FVector(110.f, -H.Ry + 90.f, 120.f), LilacLight(), 120.f, 700.f);
		for (const float Off : { -50.f, 50.f }) B.K.Add(FSurf::Lit(Rgb(216, 208, 255) * 0.4f, 0.35f), FVector(4.f, 80.f, 40.f), FVector(H.Rx - 4.f, -H.Ry + 90.f + Off, 160.f + Off * 0.2f));
		for (int32 l = 0; l < 6; ++l) B.K.Disc(Dressed(0.8f + 0.1f * l), FVector(-H.Rx + 20.f, -100.f + l * 40.f, 140.f), FVector(1.f, 0.f, 0.f), 26.f + l * 3.f, 5.f);
		B.K.Disc(FSurf::Glass(Rgb(230, 240, 250) * 0.6f, 0.f), FVector(30.f, 60.f, 2.f), FVector(0.1f, 0.f, 1.f), 24.f, 4.f);
		B.K.Disc(FSurf::Glass(Rgb(230, 240, 250) * 0.6f, 0.f), FVector(55.f, 72.f, 2.f), FVector(-0.1f, 0.1f, 1.f), 18.f, 4.f);
		B.Box(FVector(0.f, -H.Ry + 90.f, 50.f), FVector(130.f, 45.f, 50.f));
	}

	/** H8 the Severance Gate: the great hexafoil door hewn into the gorge's south wall (its ward still live: the keystone
	 *  high above it), and behind it the Wardcarvers' last guard room, weapons racked facing the door, never drawn. */
	void SeveranceGate(FBuild& B, const FData::FHall& H)
	{
		const FData& D = FData::Get();
		for (const float S : { -1.f, 1.f })
		{
			B.K.Add(Timber(0.9f), FVector(40.f, 600.f, 12.f), FVector(-H.Rx + 80.f, S * 200.f, 160.f));
			for (int32 w = 0; w < 8; ++w)
			{
				const FVector P(-H.Rx + 70.f, S * 200.f - 260.f + w * 74.f, 0.f);
				B.K.Rod(w % 3 ? Timber(1.1f) : Iron(), P + FVector(0, 0, 4.f), P + FVector(18.f, 0.f, 230.f), w % 3 ? 4.f : 3.f);
				if (w % 3 == 0) B.K.Add(Iron(), FVector(10.f, 4.f, 60.f), P + FVector(20.f, 0.f, 250.f), FRotator(0.f, 0.f, 0.f));
				else B.K.Spike(Iron(), P + FVector(18.f, 0.f, 228.f), FVector(0.08f, 0.f, 1.f), 7.f, 30.f);
			}
		}
		for (const float S : { -1.f, 1.f }) B.K.Add(Dressed(0.9f), FVector(60.f, 260.f, 45.f), FVector(H.Rx * 0.3f, S * (H.Ry - 60.f), 22.f));
		// The gate in the gorge's wall: at the passage's mouth, facing the gorge (north).
		if (const FData::FTube* T = D.FindTube(TEXT("gate")))
		{
			const FVector M = B.Local(T->Pts[0]);
			const FVector Along = (T->Pts[0] - T->Pts[FMath::Min(2, T->Pts.Num() - 1)]).GetSafeNormal2D();
			const FVector Side = FVector::CrossProduct(Along, FVector::UpVector).GetSafeNormal();
			for (const float S : { -1.f, 1.f }) B.K.Add(Dressed(1.15f), FVector(120.f, 120.f, T->H + 200.f), M + Side * (T->W * 0.5f + 40.f) * S + FVector(0, 0, (T->H + 200.f) * 0.5f - 20.f), FRotator(0.f, Along.Rotation().Yaw, 0.f));
			B.K.Add(Dressed(1.2f), FVector(140.f, T->W + 320.f, 120.f), M + FVector(0, 0, T->H + 60.f), FRotator(0.f, Along.Rotation().Yaw, 0.f));
			Hexafoil(B, Bronze(1.2f), M + FVector(0, 0, T->H + 380.f) + Along * 40.f, Along, 260.f, 6.f);
			Hexafoil(B, WardGlow(0.35f), M + FVector(0, 0, T->H + 380.f) + Along * 46.f, Along, 230.f, 3.f, true);
			// The keystone plate high over the door on the gorge face (an arrow or a spell drops the ward: crystal 2.4 H8).
			for (const FData::FWard& W : D.Wards) if (W.bGate)
			{
				const FVector K = B.Local(W.Key);
				B.K.Add(Lead(1.1f), FVector(14.f, 90.f, 90.f), K, FRotator(0.f, Along.Rotation().Yaw, 0.f));
				Hexafoil(B, Bronze(1.2f), K + Along * 8.f, Along, 36.f, 2.f);
			}
		}
	}

	/** H5 the Tally Wall: 40 m of the tube wall covered in tally marks in groups of six (hexafoil petals), each group
	 *  crossed through, getting shakier toward the end and stopping; below, a stone bowl of a hundred wax earplugs. */
	void TallyWall(FBuild& B, AWRCrystalScenery* A)
	{
		const FData::FTube* T = FData::Get().FindTube(TEXT("organ"));
		if (!T) return;
		float Fl = 0.f, Tt = 0.f; int32 Seg = 0; FVector Dir;
		T->Nearest(A->GetActorLocation(), Fl, Seg, Tt, Dir);
		const FVector Side = FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal();
		const FVector Wall0 = Side * (T->R * 0.88f);
		const FSurf Mark = FSurf::Stone(Rgb(200, 196, 204) * 0.6f, 0.9f);
		int32 g = 0;
		for (float X = -2000.f; X < 2000.f; X += 70.f, ++g)
		{
			const float Shake = FMath::Clamp((X + 2000.f) / 4000.f, 0.f, 1.f);
			for (int32 Row = 0; Row < 5; ++Row)
			{
				if (Shake > 0.92f && Row > 1) continue;
				const FVector C = Wall0 + Dir * X + FVector(0, 0, 160.f + Row * 70.f);
				for (int32 m = 0; m < 6; ++m)
				{
					const float J = (Hash(g * 7 + Row, m) - 0.5f) * 14.f * Shake;
					B.K.Add(Mark, FVector(2.f, 3.f, 34.f), C + Dir * (-25.f + m * 9.f) + FVector(0, 0, J) - Side * 2.f, FRotator(0.f, Dir.Rotation().Yaw, J * 0.8f));
				}
				B.K.Add(Mark, FVector(2.f, 64.f, 3.f), C - Side * 3.f, FRotator(0.f, Dir.Rotation().Yaw, -28.f + (Hash(g, Row) - 0.5f) * 20.f * Shake));
			}
		}
		B.K.Add(Dressed(), FVector(110.f, 110.f, 40.f), Wall0 - Side * 120.f + FVector(0, 0, 20.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		for (int32 w = 0; w < 30; ++w) B.K.Add(Wax(0.9f + 0.2f * Hash(w, 1)), FVector(6.f, 6.f, 9.f), Wall0 - Side * 120.f + FVector((Hash(w, 2) - 0.5f) * 70.f, (Hash(w, 3) - 0.5f) * 70.f, 44.f), FRotator(Hash(w, 4) * 80.f, 0.f, 0.f), EWRLookMesh::Cylinder);
	}

	// ---- the Silent Vault (crystal 2.5) ----
	void SeatedCarver(FBuild& B, const FVector& At, float Yaw, int32 Seed, bool bWaxed)
	{
		const FRotator R(0.f, Yaw, 0.f);
		const auto P = [&](float X, float Y, float Z) { return At + R.RotateVector(FVector(X, Y, Z)); };
		const FLinearColor Dry = Rgb(120, 104, 86) * 0.5f, Robe = Rgb(70, 64, 76) * 0.5f;
		B.Limb(P(0.f, 0.f, 45.f), P(-8.f, 0.f, 110.f), 18.f, 16.f, Robe);
		B.Ball(P(-4.f, 0.f, 125.f), 12.f, Dry, 0.8f);
		for (const float S : { -1.f, 1.f })
		{
			B.Limb(P(0.f, S * 10.f, 45.f), P(40.f, S * 12.f, 45.f), 8.f, 7.f, Robe);
			B.Limb(P(40.f, S * 12.f, 45.f), P(42.f, S * 12.f, 0.f), 6.f, 5.f, Robe);
			B.Limb(P(-5.f, S * 18.f, 100.f), P(25.f, S * 8.f, 55.f), 5.f, 4.f, Robe);
		}
		if (bWaxed) B.K.Add(Wax(), FVector(4.f, 10.f, 4.f), P(8.f, 0.f, 118.f));
		// Fine crystal grown on the shoulders like frost.
		for (int32 c = 0; c < 4; ++c) Point(B, AmethystPale(0.f), P(-8.f, (c < 2 ? -1.f : 1.f) * 14.f, 112.f), FVector((Hash(Seed, c) - 0.5f), (c < 2 ? -1.f : 1.f) * 0.5f, 1.f), 12.f + 8.f * Hash(Seed, c + 4), 3.f, c * 20.f);
	}

	void VaultRoom(FBuild& B, AWRCrystalScenery* A)
	{
		const FData::FSite* S = FData::Get().FindSite(A->SiteId);
		const FData::FHall* H = S ? FData::Get().FindHall(S->Hall) : nullptr;
		if (!H) return;
		const FName Id = H->Id;
		// The room runs along local +Y (its yaw 90: the line south); the far wall at +Rx along its own X.
		const FRotator R(0.f, H->Yaw, 0.f);
		const auto L = [&](float X, float Y, float Z) { return R.RotateVector(FVector(X, Y, Z)); };
		if (Id == TEXT("V1"))
		{
			B.K.Add(Dressed(), FVector(60.f, 300.f, 50.f), L(-H->Rx + 60.f, 0.f, 25.f), R);
			for (int32 t = 0; t < 6; ++t)
			{
				B.K.Rod(Timber(1.1f), L(-H->Rx + 60.f, -120.f + t * 48.f, 52.f), L(-H->Rx + 60.f, -120.f + t * 48.f + 30.f, 52.f), 3.f);
				B.K.Add(Wool(), FVector(18.f, 14.f, 14.f), L(-H->Rx + 60.f, -120.f + t * 48.f, 56.f), R);
			}
			for (int32 b = 0; b < 3; ++b)
			{
				const FVector P = L(H->Rx * 0.4f, -200.f + b * 200.f, 0.f);
				B.K.Add(Dressed(0.9f), FVector(60.f, 60.f, 30.f), P + FVector(0, 0, 15.f), R, EWRLookMesh::Cylinder);
				for (int32 w = 0; w < 12; ++w) B.K.Add(Wax(), FVector(5.f, 5.f, 8.f), P + FVector((Hash(b, w) - 0.5f) * 36.f, (Hash(b, w + 9) - 0.5f) * 36.f, 32.f), R, EWRLookMesh::Cylinder);
			}
		}
		else if (Id == TEXT("V2"))
		{
			// Forty on stone benches in five rows, facing the far wall, wax over their mouths, unafraid.
			for (int32 r = 0; r < 5; ++r)
			{
				const float X = -H->Rx * 0.6f + r * 200.f;
				B.K.Add(Dressed(0.85f), FVector(60.f, H->Ry * 1.6f, 45.f), L(X, 0.f, 22.f), R);
				for (int32 s = 0; s < 8; ++s) SeatedCarver(B, L(X, -H->Ry * 0.75f + s * (H->Ry * 1.5f / 7.f), 45.f), H->Yaw, r * 8 + s, true);
			}
		}
		else if (Id == TEXT("V3"))
		{
			// Shelves of six hundred bronze tuning forks, each with its little lead tag.
			for (const float S2 : { -1.f, 1.f }) for (int32 sh = 0; sh < 5; ++sh)
			{
				const float Y = S2 * (H->Ry - 40.f);
				B.K.Add(Dressed(0.8f), FVector(H->Rx * 1.6f, 40.f, 6.f), L(0.f, Y, 60.f + sh * 70.f), R);
				for (int32 f = 0; f < 60; ++f)
				{
					const FVector P = L(-H->Rx * 0.78f + f * (H->Rx * 1.56f / 59.f), Y, 66.f + sh * 70.f);
					B.K.Rod(Bronze(0.9f + 0.2f * Hash(f, sh)), P, P + FVector(0, 0, 30.f), 1.2f);
				}
			}
		}
		else if (Id == TEXT("V5"))
		{
			// The Sovereigns' Mould: a casting pit shaped like a woman lying down, two bronze name-plates at its head.
			for (int32 k = 0; k < 9; ++k)
			{
				const float T = k / 8.f;
				const float W = 90.f + 120.f * FMath::Sin(PI * FMath::Clamp(T * 1.1f, 0.f, 1.f)) + (k == 1 ? 60.f : 0.f);
				B.K.Add(Polished(0.5f), FVector(100.f, W, 4.f), L(-H->Rx * 0.7f + T * H->Rx * 1.4f, 0.f, 1.f), R);
			}
			B.K.Add(Dressed(1.1f), FVector(400.f, 40.f, 6.f), L(0.f, 140.f, 3.f), R);
			B.K.Add(Dressed(1.1f), FVector(400.f, 40.f, 6.f), L(0.f, -140.f, 3.f), R);
			B.K.Add(Bronze(1.3f), FVector(6.f, 60.f, 30.f), L(-H->Rx * 0.85f, -50.f, 60.f), R);
			B.K.Add(Bronze(0.55f), FVector(6.f, 60.f, 30.f), L(-H->Rx * 0.85f, 50.f, 60.f), R);
			for (int32 sc = 0; sc < 5; ++sc) B.K.Add(Dressed(0.3f), FVector(7.f, 40.f, 1.f), L(-H->Rx * 0.85f + 3.f, 50.f, 52.f + sc * 4.f), R + FRotator(0.f, 0.f, 15.f * (Hash(sc, 1) - 0.5f)));
		}
		else if (Id == TEXT("V6"))
		{
			// The last carver kneels at the far wall, her chisel still in the last stroke of a hexafoil one petal short.
			const FVector Wall = L(H->Rx - 6.f, 0.f, 0.f);
			const FVector In = L(-1.f, 0.f, 0.f).GetSafeNormal();
			const FLinearColor Robe = Rgb(70, 64, 76) * 0.5f, Dry = Rgb(120, 104, 86) * 0.5f;
			const FVector K = Wall + In * 70.f;
			B.Limb(K + FVector(0, 0, 40.f), K + FVector(0, 0, 105.f) - In * 18.f, 17.f, 15.f, Robe);
			B.Ball(K + FVector(0, 0, 122.f) - In * 22.f, 12.f, Dry, 0.8f);
			B.Limb(K + FVector(0, 0, 95.f) - In * 10.f, Wall + In * 4.f + FVector(0, 0, 150.f), 5.f, 4.f, Robe);
			B.K.Rod(Bronze(1.3f), Wall + In * 4.f + FVector(0, 0, 150.f), Wall + In * 1.f + FVector(0, 0, 158.f), 1.5f);
			// The unfinished hexafoil: five petals cut, the sixth begun.
			const FVector C = Wall + In * 1.f + FVector(0, 0, 170.f);
			FVector U, V;
			In.FindBestAxisVectors(U, V);
			for (int32 p = 0; p < 6; ++p)
			{
				const int32 Segs = p < 5 ? 10 : 3;
				const float Ac = 2.f * PI * p / 6.f;
				const FVector Pc = C + (U * FMath::Cos(Ac) + V * FMath::Sin(Ac)) * 30.f;
				for (int32 i = 0; i < Segs; ++i)
				{
					const float A0 = Ac + PI - PI / 3.f + (2.f * PI / 3.f) * i / 10.f;
					const FVector Q = Pc + (U * FMath::Cos(A0) + V * FMath::Sin(A0)) * 30.f;
					B.K.Add(FSurf::Stone(Rgb(216, 208, 255) * 0.5f, 0.9f), FVector(2.f, 4.f, 4.f), Q, FRotator::ZeroRotator);
				}
			}
		}
		// V4's lake is its pool (PoolLook via the hall); its ceiling glitters with the lining.
	}

	/** A cave kit POI in its alcove or hall (crystal 2.4): what each kind shows (its gathering, caches and dens are the
	 *  world's and the creatures' rows). */
	void CaveKit(FBuild& B, AWRCrystalScenery* A)
	{
		const FData& D = FData::Get();
		if (!D.Kit.IsValidIndex(A->Index)) return;
		const FData::FKit& K = D.Kit[A->Index];
		const int32 Seed = A->Index * 23 + 5;
		const FName Kind = K.Kind;
		if (Kind == TEXT("nest_geode"))
		{
			// A sealed Geodemite nest: a basalt-skinned geode 2.2 m round, its seam glowing faint violet.
			B.Ball(FVector(0, 0, 110.f), 110.f, Rgb(30, 30, 34) * 0.7f, 0.8f);
			B.K.Add(HoleGlow(0.35f), FVector(4.f, 200.f, 160.f), FVector(0, 0, 110.f), FRotator(0.f, 30.f, 10.f));
			B.Box(FVector(0, 0, 110.f), FVector(100.f, 100.f, 100.f));
		}
		else if (Kind == TEXT("roost"))
		{
			for (int32 g = 0; g < 5; ++g) B.K.Add(Guano(0.8f + 0.3f * Hash(Seed, g)), FVector(180.f, 160.f, 50.f + 40.f * Hash(Seed, g + 3)), FVector((Hash(Seed, g + 6) - 0.5f) * 500.f, (Hash(Seed, g + 9) - 0.5f) * 400.f, 15.f), FRotator(0.f, Hash(Seed, g) * 180.f, 0.f), EWRLookMesh::Cone);
			for (int32 c = 0; c < 6; ++c) Point(B, FSurf::Glass(Rgb(216, 208, 255) * 0.55f, 0.12f), FVector((Hash(Seed, c + 20) - 0.5f) * 600.f, (Hash(Seed, c + 30) - 0.5f) * 500.f, 450.f + 150.f * Hash(Seed, c)), FVector(0, 0, -1.f), 50.f + 40.f * Hash(Seed, c + 40), 10.f, c * 10.f);
		}
		else if (Kind == TEXT("chime_cache"))
		{
			// A niche cut in the wall with a stone shelf, the cache on it (WRCrystalWorld), a hexafoil over it.
			B.K.Add(Dressed(), FVector(200.f, 120.f, 20.f), FVector(0, 0, 90.f));
			B.K.Add(Dressed(), FVector(200.f, 20.f, 240.f), FVector(0, -60.f, 120.f));
			Hexafoil(B, WardGlow(0.25f), FVector(0, -48.f, 220.f), FVector(0, 1.f, 0.f), 30.f, 2.f);
			B.Box(FVector(0, 0, 90.f), FVector(100.f, 60.f, 10.f));
		}
		else if (Kind == TEXT("scald_basin"))
		{
			// Rimstone pools: mineral dams built up on the floor by the hot water, teal water held behind them.
			for (int32 p = 0; p < 3; ++p)
			{
				const FVector C((p - 1) * 260.f, (Hash(Seed, p) - 0.5f) * 200.f, 0.f);
				const float R = 110.f + 50.f * Hash(Seed, p + 4);
				for (int32 i = 0; i < 14; ++i)
				{
					const float Ang = 2.f * PI * i / 14.f;
					B.K.Add(Mineral(0.9f + 0.2f * Hash(p, i)), FVector(2.f * PI * R / 14.f + 6.f, 22.f, 26.f), C + FVector(FMath::Cos(Ang) * R, FMath::Sin(Ang) * R, 13.f), FRotator(0.f, FMath::RadiansToDegrees(Ang) + 90.f, 0.f));
				}
				B.K.Add(SteamWater(), FVector(R * 2.f - 20.f, R * 2.f - 20.f, 4.f), C + FVector(0, 0, 20.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			}
			B.Lamp(FVector(0, 0, 100.f), FLinearColor(FColor(140, 230, 220)), 260.f, 900.f);
		}
		else if (Kind == TEXT("chimecrab_ledge"))
		{
			for (int32 l = 0; l < 3; ++l)
			{
				const FVector P((l - 1) * 220.f, 0.f, 120.f + l * 90.f);
				B.K.Add(Basalt(), FVector(220.f, 120.f, 40.f), P);
				B.Box(P, FVector(110.f, 60.f, 20.f));
			}
		}
		else if (Kind == TEXT("druseback_wallow"))
		{
			B.K.Add(FSurf::Glaze(Rgb(72, 56, 40) * 0.5f, 0.25f), FVector(700.f, 520.f, 6.f), FVector(0, 0, 4.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			for (int32 i = 0; i < 16; ++i)
			{
				const float Ang = 2.f * PI * i / 16.f;
				B.K.Add(FSurf::Cloth(Rgb(90, 74, 56) * 0.5f), FVector(140.f, 60.f, 22.f), FVector(FMath::Cos(Ang) * 360.f, FMath::Sin(Ang) * 270.f, 10.f), FRotator(0.f, FMath::RadiansToDegrees(Ang) + 90.f, 0.f));
			}
		}
		else if (Kind == TEXT("lumenhorn_hall"))
		{
			for (int32 m = 0; m < 12; ++m) B.K.Add(FSurf::Cloth(Rgb(96, 120, 70) * 0.5f), FVector(200.f + 140.f * Hash(Seed, m), 140.f, 8.f), FVector((Hash(Seed, m + 5) - 0.5f) * 1400.f, (Hash(Seed, m + 9) - 0.5f) * 1100.f, 4.f), FRotator(0.f, Hash(Seed, m + 2) * 180.f, 0.f), EWRLookMesh::Cylinder);
		}
		else if (Kind == TEXT("vista_window"))
		{
			// A ledge with a bronze rail at the alcove's lip, looking out over the Hall of Beams' steam.
			for (int32 p = 0; p < 5; ++p) B.K.Rod(Bronze(0.8f), FVector(300.f, -200.f + p * 100.f, 0.f), FVector(300.f, -200.f + p * 100.f, 100.f), 4.f);
			B.K.Rod(Bronze(), FVector(300.f, -220.f, 100.f), FVector(300.f, 220.f, 100.f), 5.f);
			B.Box(FVector(300.f, 0.f, 50.f), FVector(10.f, 220.f, 50.f));
		}
		else if (Kind == TEXT("resonant_outcrop"))
		{
			for (int32 r = 0; r < 3; ++r) B.Rock(r + Seed, FVector(200.f, 160.f, 120.f), FVector((Hash(Seed, r) - 0.5f) * 400.f, (Hash(Seed, r + 4) - 0.5f) * 400.f, 0.f), FRotator(0.f, Hash(Seed, r + 8) * 360.f, 0.f), Rgb(36, 32, 44) * 0.7f, true, 30.f);
		}
		// cave_lake: its pool row draws the water (PoolLook through its alcove hall).
	}

	bool Build(FBuild& B, AWRCrystalScenery* A)
	{
		const FName K = A->Kind;
		if (K == TEXT("hall")) { HallFurniture(B, A); return true; }
		if (K == TEXT("tube")) { TubeFurniture(B, A); return true; }
		if (K == TEXT("severance_stair")) { SeveranceStair(B, A); return true; }
		const FData& D = FData::Get();
		const FData::FSite* S = D.FindSite(A->SiteId);
		const FData::FHall* H = S ? D.FindHall(S->Hall) : nullptr;
		if (K == TEXT("h1") && H) { SingingSchool(B, *H); return true; }
		if (K == TEXT("h2")) { BrokenNest(B); return true; }
		if (K == TEXT("h3") && H) { SteamBaths(B, *H); return true; }
		if (K == TEXT("h4")) { FallenChandelier(B, A); return true; }
		if (K == TEXT("h6") && H) { GuanoMines(B, *H); return true; }
		if (K == TEXT("h7") && H) { LensRoom(B, *H); return true; }
		if (K == TEXT("h8") && H) { SeveranceGate(B, *H); return true; }
		if (K == TEXT("tally_wall")) { TallyWall(B, A); return true; }
		if (K.ToString().StartsWith(TEXT("vault_"))) { VaultRoom(B, A); return true; }
		if (A->bUnder) { CaveKit(B, A); return true; }
		return false;
	}
}
