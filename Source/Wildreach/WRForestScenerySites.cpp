// The Ancient forest's hand-made sites of the meadows, the streams and the old growth (cooks/biome-forest.md 2.4 rows
// #1-#5 and #9-#21, the story in 2.20; cooks/build-biome-forest.md 2.5): the Wreck Meadow, the abandoned farmstead, the
// Mill Race, the truffle glade, the Woodcutters' Camp, the Hanging Oak, Herne's Ford, the Great Falls' fishing platform,
// the Root Bridge, Otter Steps, the Frosted Ferns, the Nurse Log's clearing, the Bear's Larder, Wasp Ravine, the Coil
// Hollow and the Offering Stones. Each one is a story told with no words; each piece says why it is there.
// Local space: the actor stands on the site's ground point, turned to the site's yaw (WRForestSceneryKit.h).
// Where the server puts a cache or a node (WRForestWorld.cpp SpawnServerActors), the drawing makes room for it and
// gives it something to rest on.

#include "WRForestSceneryKit.h"

namespace WRForestSitesImpl
{
	using WRForestLook::FBuild;
	using WRLookKit::FSurf;

	/** A point turned by a local yaw about Origin. */
	FVector Turn(const FVector& Origin, float Yaw, float X, float Y, float Z)
	{
		return Origin + FRotator(0.f, Yaw, 0.f).RotateVector(FVector(X, Y, Z));
	}

	/** A cut stump (the newcomers' felling), its top sawn flat and pale, its rings showing; an axe left in it if bAxe. */
	void Stump(FBuild& Bd, const FVector& Base, float Radius, float Height, float Yaw, bool bAxe)
	{
		Bd.Tree(TEXT("stump"), int32(FMath::Abs(Base.X + Base.Y)) % 4, Base, Height, FRotator(0.f, Yaw, 0.f), Height, Radius);
		// The saw face: a pale disc with two darker rings, the bark lip round it.
		Bd.K.Disc(WRForestLook::Carved(1.05f), Base + FVector(0, 0, Height + 1.f), FVector::UpVector, Radius * 1.8f, 3.f);
		Bd.K.Disc(WRForestLook::Carved(0.8f), Base + FVector(0, 0, Height + 2.5f), FVector::UpVector, Radius * 1.1f, 1.f);
		Bd.K.Disc(WRForestLook::Carved(0.7f), Base + FVector(0, 0, Height + 3.f), FVector::UpVector, Radius * 0.45f, 1.f);
		if (!bAxe) return;
		// An axe bitten into the top at a slant, its haft up: whoever swung it last never came back for it.
		const FRotator R(0.f, Yaw, 0.f);
		const FVector Bite = Base + FVector(0, 0, Height + 2.f) + R.RotateVector(FVector(Radius * 0.3f, 0, 0));
		const FVector HaftEnd = Bite + R.RotateVector(FVector(-30.f, 0, 62.f));
		Bd.K.Rod(WRForestLook::Timber(1.2f), Bite, HaftEnd, 4.5f);
		Bd.K.Add(WRForestLook::Iron(), FVector(20.f, 3.f, 16.f), Bite + FVector(0, 0, 2.f), FRotator(-25.f, Yaw, 0.f));
	}

	/** A human skull: the cranium dome, the cheekbones and the jaw (the woodcutters, held in the roots). */
	void Skull(FBuild& Bd, const FVector& At, float Yaw, float Tilt)
	{
		const FRotator R(Tilt, Yaw, 0.f);
		Bd.Ball(At, 9.f, WRForestLook::BoneC(), 0.75f, 1.05f);
		Bd.K.Add(WRForestLook::BoneS(), FVector(9.f, 12.f, 6.f), At + R.RotateVector(FVector(7.f, 0, -6.f)), R);
		Bd.K.Add(WRForestLook::Black(0.5f), FVector(2.f, 3.5f, 3.f), At + R.RotateVector(FVector(8.5f, -3.5f, -2.f)), R);   // the eye sockets
		Bd.K.Add(WRForestLook::Black(0.5f), FVector(2.f, 3.5f, 3.f), At + R.RotateVector(FVector(8.5f, 3.5f, -2.f)), R);
		Bd.K.Add(WRForestLook::BoneS(), FVector(8.f, 10.f, 3.f), At + R.RotateVector(FVector(6.f, 0, -13.f)), FRotator(Tilt - 12.f, Yaw, 0.f));
	}

	/** A rib cage lying on its side or in a root's grip: a spine and six pairs of curved ribs. Faces +X of Yaw. */
	void Ribs(FBuild& Bd, const FVector& At, float Yaw, float Scale)
	{
		const FLinearColor C = WRForestLook::BoneC();
		const FVector Fwd = FRotator(0.f, Yaw, 0.f).Vector(), Side = FRotator(0.f, Yaw + 90.f, 0.f).Vector();
		Bd.Limb(At - Fwd * 28.f * Scale, At + Fwd * 28.f * Scale, 2.4f * Scale, 2.f * Scale, C, 6);
		for (int32 i = 0; i < 6; ++i)
		{
			const FVector Root = At + Fwd * (-22.f + 9.f * i) * Scale;
			for (const float Sd : { -1.f, 1.f })
			{
				Bd.Root({ Root, Root + Side * Sd * 12.f * Scale + FVector(0, 0, -3.f) * Scale, Root + Side * Sd * 15.f * Scale + FVector(0, 0, -16.f) * Scale,
					Root + Side * Sd * 8.f * Scale + FVector(0, 0, -26.f) * Scale }, 1.6f * Scale, 1.f * Scale, C, 5);
			}
		}
	}

	/** A root that rises out of the ground at P0, arches over and dives back in at P1 (Height over the chord). */
	void RootArc(FBuild& Bd, const FVector& P0, const FVector& P1, float Height, float R0, float R1, const FLinearColor& C, int32 Steps = 6)
	{
		TArray<FVector> Pts;
		for (int32 i = 0; i <= Steps; ++i) Pts.Add(WRForestLook::Arch(P0 - FVector(0, 0, 30.f), P1 - FVector(0, 0, 30.f), float(i) / Steps, Height + 30.f));
		Bd.Root(Pts, R0, R1, C, 8);
	}

	/** A straw skep: a coiled straw dome on a board (the Grovekin and the newcomers kept bees in them). */
	void Skep(FBuild& Bd, const FVector& Base, float Size)
	{
		TArray<FVector> Prof;
		for (int32 i = 0; i <= 6; ++i)
		{
			const float U = i / 6.f;
			const float Rr = Size * (U < 0.15f ? 1.f : FMath::Cos((U - 0.15f) / 0.85f * UE_HALF_PI * 0.98f)) + 1.f;
			Prof.Add(FVector(U, Rr, Rr));
		}
		Bd.M.Lathe(Base, FVector::UpVector, Size * 1.25f, Prof, 12, WRForestLook::Rgb(190, 156, 86) * 0.5f, 0.95f);
		// The coils: a few straw bands round it.
		for (int32 c = 1; c < 5; ++c)
		{
			const float Z = Size * 1.25f * c / 5.5f;
			const float Rr = Size * FMath::Cos(FMath::Clamp((c / 5.5f - 0.15f) / 0.85f, 0.f, 1.f) * UE_HALF_PI * 0.98f) + 2.f;
			TArray<FVector> Ring;
			for (int32 s = 0; s <= 16; ++s) Ring.Add(Base + FVector(FMath::Cos(s / 16.f * UE_TWO_PI) * Rr, FMath::Sin(s / 16.f * UE_TWO_PI) * Rr, Z));
			Bd.M.Tube(Ring, 1.6f, 4, WRForestLook::Rgb(170, 136, 70) * 0.45f, 0.95f);
		}
		Bd.K.Add(WRForestLook::Black(0.7f), FVector(3.f, 9.f, 5.f), Base + FVector(Size * 0.98f, 0, 3.f));   // the flight hole
	}

	/** A fern clump (tree kit fern, no collision). */
	void Fern(FBuild& Bd, const FVector& Base, float Height, float Yaw)
	{
		Bd.Tree(TEXT("fern"), int32(FMath::Abs(Base.X * 0.37f + Base.Y * 0.61f)) % 2, Base, Height, FRotator(0.f, Yaw, 0.f), 0.f);
	}

	/** A field-stone dry wall from P0 to P1 (local, on the ground): two faces of stones, a coping of stones on edge. */
	void DryWall(FBuild& Bd, const FVector& P0, const FVector& P1, float Height, int32 Salt, bool bGap)
	{
		const FVector D = P1 - P0;
		const float Len = D.Size2D();
		if (Len < 50.f) return;
		const float Yaw = D.Rotation().Yaw;
		const int32 Count = FMath::Max(2, FMath::RoundToInt(Len / 70.f));
		for (int32 i = 0; i < Count; ++i)
		{
			if (bGap && FMath::Abs(i - Count / 2) <= 1) continue;   // the gate gap
			const float U = (i + 0.5f) / Count;
			const FVector C = FMath::Lerp(P0, P1, U);
			const float Gz = Bd.G(C.X, C.Y, C.Z);
			const float J = Bd.Rand(Salt + i * 7);
			for (int32 c = 0; c < 3; ++c)
			{
				const float Zc = Gz + 18.f + c * (Height - 30.f) / 3.f;
				Bd.K.Add(WRForestLook::OldStone(0.75f + 0.3f * Bd.Rand(Salt + i * 7 + c)), FVector(70.f - 10.f * c, 50.f - 6.f * c, 34.f), FVector(C.X, C.Y, Zc), FRotator(J * 6.f - 3.f, Yaw + J * 10.f, 0.f));
			}
			Bd.K.Add(WRForestLook::OldStone(0.7f), FVector(18.f, 46.f, 30.f), FVector(C.X, C.Y, Gz + Height - 4.f), FRotator(0.f, Yaw, 8.f * (J - 0.5f)));
			Bd.K.Add(WRForestLook::Moss(0.9f), FVector(30.f + 30.f * J, 40.f, 3.f), FVector(C.X, C.Y, Gz + Height + 8.f), FRotator(0.f, Yaw, 0.f));
		}
		const FVector Mid = (P0 + P1) * 0.5f;
		Bd.SegBox(FVector(P0.X, P0.Y, Bd.G(P0.X, P0.Y) + Height * 0.5f), FVector(P1.X, P1.Y, Bd.G(P1.X, P1.Y) + Height * 0.5f), 28.f, Height * 0.5f);
		(void)Mid;
	}

	/** Wild oats: a tuft of stalks with drooping panicles, gone to seed (straw gold, a few still green). */
	void OatTuft(FBuild& Bd, const FVector& Base, int32 Salt)
	{
		for (int32 s = 0; s < 4; ++s)
		{
			const float Ang = 90.f * s + 40.f * Bd.Rand(Salt + s);
			const FVector Lean = FRotator(0.f, Ang, 0.f).Vector() * (14.f + 10.f * Bd.Rand(Salt + s + 9));
			const float H = 70.f + 40.f * Bd.Rand(Salt + s + 3);
			const FVector Top = Base + Lean + FVector(0, 0, H);
			Bd.K.Rod(s == 0 ? WRForestLook::Leaf(1.1f) : WRForestLook::Straw(), Base, Top, 1.2f);
			Bd.K.Add(WRForestLook::Straw(), FVector(4.f, 4.f, 22.f), Top + Lean.GetSafeNormal() * 6.f - FVector(0, 0, 8.f), FRotator(30.f, Ang, 0.f), EWRLookMesh::Cone);
		}
	}

	/** A wicker basket, woven: a lathe body with withy rings. */
	void Basket(FBuild& Bd, const FVector& Base, float Radius, float Height, float TiltDeg)
	{
		const FVector Axis = FRotator(90.f - TiltDeg, 0.f, 0.f).Vector();
		Bd.M.Lathe(Base, Axis, Height, { FVector(0.f, Radius * 0.75f, Radius * 0.75f), FVector(0.5f, Radius * 0.92f, Radius * 0.92f), FVector(1.f, Radius, Radius) }, 12, WRForestLook::Rgb(150, 116, 72) * 0.45f, 0.95f);
		for (int32 c = 1; c <= 3; ++c)
		{
			TArray<FVector> Ring;
			const FVector Cen = Base + Axis * Height * c / 3.3f;
			const FVector U = FVector::CrossProduct(Axis, FVector::RightVector).GetSafeNormal(), V = FVector::CrossProduct(Axis, U);
			const float Rr = FMath::Lerp(Radius * 0.75f, Radius, c / 3.3f) + 1.f;
			for (int32 s = 0; s <= 14; ++s) Ring.Add(Cen + (U * FMath::Cos(s / 14.f * UE_TWO_PI) + V * FMath::Sin(s / 14.f * UE_TWO_PI)) * Rr);
			Bd.M.Tube(Ring, 1.4f, 4, WRForestLook::Rgb(120, 90, 54) * 0.45f, 0.95f);
		}
	}

	/** A pig's skull: long snout, flared cheeks, the boar's tusks curling out. Faces +X of Yaw. */
	void PigSkull(FBuild& Bd, const FVector& At, float Yaw)
	{
		const FRotator R(0.f, Yaw, 0.f);
		const FLinearColor C = WRForestLook::BoneC() * 0.9f;
		Bd.Ball(At, 11.f, C, 0.7f, 0.85f);
		Bd.Limb(At + R.RotateVector(FVector(6.f, 0, -2.f)), At + R.RotateVector(FVector(34.f, 0, -8.f)), 8.f, 4.f, C, 8);
		for (const float Sd : { -1.f, 1.f })
		{
			Bd.Root({ At + R.RotateVector(FVector(22.f, Sd * 5.f, -9.f)), At + R.RotateVector(FVector(26.f, Sd * 11.f, -3.f)), At + R.RotateVector(FVector(22.f, Sd * 13.f, 5.f)) }, 1.8f, 0.6f, WRForestLook::Rgb(230, 220, 190) * 0.7f, 5);
			Bd.K.Add(WRForestLook::Black(0.5f), FVector(3.f, 4.f, 4.f), At + R.RotateVector(FVector(6.f, Sd * 7.f, 3.f)), R);
		}
	}
}

// ================================================================================================ the meadows (F1)

void WRForestLook::FBuild::WreckMeadow()
{
	using namespace WRForestSitesImpl;
	// DC 2.4 #1, 2.20 "the newcomers cleared the meadows": the ship that brought the newcomers, its keel broken on the
	// dunes. The bow half lies along -X, the stern half (and its cabin) at +X, the break between them at x = -200; the
	// cache (wood, nails, rope, the ship's lantern) lies at the hen-house door (150, 80).
	const float KeelYaw = 8.f;
	const FRotator KR(0.f, KeelYaw, 0.f);
	// The dunes the ship drove up: low drifts of sand round the hull, marram grass on their crests.
	for (int32 d = 0; d < 7; ++d)
	{
		const FVector C(-1800.f + 520.f * d + 140.f * Rand(d), (d % 2 ? 520.f : -560.f) + 120.f * Rand(d + 20), 0.f);
		const float Gz = G(C.X, C.Y);
		M.Lathe(FVector(C.X, C.Y, Gz - 40.f), FVector::UpVector, 120.f + 60.f * Rand(d + 3), { FVector(0.f, 420.f, 300.f), FVector(0.5f, 330.f, 230.f), FVector(1.f, 40.f, 30.f) }, 14, Rgb(196, 178, 140) * 0.55f, 0.98f);
		for (int32 g = 0; g < 6; ++g)
		{
			const FVector Gp(C.X - 120.f + 50.f * g, C.Y + 40.f * (Rand(d * 9 + g) - 0.5f), Gz + 90.f + 20.f * Rand(g));
			K.Add(Leaf(0.9f), FVector(3.f, 3.f, 70.f), Gp + FVector(0, 0, 30.f), FRotator(15.f * (Rand(g + d) - 0.5f), 0.f, 18.f * (Rand(g * 3 + d) - 0.5f)), EWRLookMesh::Cone);
		}
	}
	// The keel: one oak timber 26 m long, snapped; the bow half heeled over and half buried, the stern half upright.
	const FVector Break = FVector(-200.f, 0.f, G(-200.f, 0.f));
	const FVector BowEnd = Turn(FVector::ZeroVector, KeelYaw, -1500.f, -60.f, 0.f);
	const FVector SternEnd = Turn(FVector::ZeroVector, KeelYaw, 1100.f, 40.f, 0.f);
	const FVector Bow0(BowEnd.X, BowEnd.Y, G(BowEnd.X, BowEnd.Y) + 60.f), Stern0(SternEnd.X, SternEnd.Y, G(SternEnd.X, SternEnd.Y) + 20.f);
	K.Rod(Timber(0.85f), Bow0, Break + FVector(-30.f, 0, 10.f), 44.f, true);
	K.Rod(Timber(0.85f), Break + FVector(30.f, 0, -10.f), Stern0, 44.f, true);
	SegBox(Bow0, Break + FVector(-30.f, 0, 10.f), 24.f, 24.f);
	SegBox(Break + FVector(30.f, 0, -10.f), Stern0, 24.f, 24.f);
	// The splintered ends at the break: shards of oak standing out of both halves.
	for (int32 s = 0; s < 5; ++s)
	{
		K.Spike(Timber(0.9f), Break + FVector(-30.f, (s - 2) * 8.f, 10.f), FVector(1.f, (s - 2) * 0.2f, 0.4f), 8.f, 30.f + 15.f * Rand(s));
		K.Spike(Timber(0.9f), Break + FVector(30.f, (s - 2) * 8.f, -5.f), FVector(-1.f, (s - 2) * 0.25f, 0.5f), 8.f, 25.f + 20.f * Rand(s + 5));
	}
	// The stem post rising at the bow, the sternpost at the stern.
	K.Rod(Timber(0.8f), Bow0, Bow0 + KR.RotateVector(FVector(-160.f, 0.f, 380.f)), 34.f, true);
	K.Rod(Timber(0.8f), Stern0, Stern0 + KR.RotateVector(FVector(60.f, 0.f, 420.f)), 34.f, true);
	// The ribs: pairs of curved futtocks rising from the keel, half buried in sand on the low side (the bow heels to -Y),
	// some broken short, a few strakes of planking still nailed to them.
	for (int32 r = 0; r < 14; ++r)
	{
		const float X = -1400.f + 190.f * r;
		if (FMath::Abs(X + 200.f) < 90.f) continue;
		const bool bBow = X < -200.f;
		const float Heel = bBow ? -22.f : -6.f;
		const FVector OnKeel = Turn(FVector::ZeroVector, KeelYaw, X, 0.f, 0.f);
		const FVector Base(OnKeel.X, OnKeel.Y, G(OnKeel.X, OnKeel.Y) + (bBow ? 30.f : 10.f));
		for (const float Sd : { -1.f, 1.f })
		{
			const float Len = (Rand(r * 3 + int32(Sd + 1.f)) < 0.25f) ? 0.55f : 1.f;     // some ribs broke off short
			const float Bury = Sd < 0.f ? 0.7f : 1.f;                                     // the low side is under the sand
			const FRotator HR(0.f, KeelYaw, Heel);
			TArray<FVector> Pts;
			for (int32 i = 0; i <= 4; ++i)
			{
				const float U = Len * i / 4.f;
				Pts.Add(Base + HR.RotateVector(FVector(0.f, Sd * (60.f + 230.f * FMath::Sin(U * 1.35f)), 320.f * U * Bury)));
			}
			Root(Pts, 13.f, 9.f, Rgb(84, 64, 46) * 0.45f, 6);
			if (Len > 0.9f && r % 3 == 1) SegBox(Pts[0], Pts[2], 10.f, 10.f);
		}
	}
	// What's left of the planking: three strakes along the stern's upright side.
	for (int32 s = 0; s < 3; ++s)
	{
		const FVector A0 = Turn(FVector::ZeroVector, KeelYaw, -100.f, 250.f + 15.f * s, 0.f), A1 = Turn(FVector::ZeroVector, KeelYaw, 900.f, 240.f + 10.f * s, 0.f);
		const FVector P0(A0.X, A0.Y, G(A0.X, A0.Y) + 90.f + 34.f * s), P1(A1.X, A1.Y, G(A1.X, A1.Y) + 70.f + 34.f * s);
		K.Rod(Plank(0.7f + 0.1f * s), P0, P1, 30.f, true);
	}
	// The mast: down across the meadow toward the bow, its top broken, the yard beside it, rope rotting off it.
	{
		const FVector Foot = Turn(FVector::ZeroVector, KeelYaw, 300.f, -500.f, 0.f), Top = Turn(FVector::ZeroVector, KeelYaw, -900.f, -1500.f, 0.f);
		const FVector P0(Foot.X, Foot.Y, G(Foot.X, Foot.Y) + 22.f), P1(Top.X, Top.Y, G(Top.X, Top.Y) + 14.f);
		Limb(P0, P1, 24.f, 15.f, Rgb(98, 76, 52) * 0.45f, 10);
		SegBox(P0, P1, 20.f, 20.f);
		const FVector Y0 = FMath::Lerp(P0, P1, 0.6f) + FVector(60.f, 140.f, -8.f), Y1 = FMath::Lerp(P0, P1, 0.6f) + FVector(-160.f, -360.f, -8.f);
		Limb(Y0, Y1, 10.f, 8.f, Rgb(98, 76, 52) * 0.42f, 8);
		Line(FMath::Lerp(P0, P1, 0.3f) + FVector(0, 0, 20.f), Y0 + FVector(0, 0, 8.f), 2.f, 30.f, RopeC());
		Line(FMath::Lerp(P0, P1, 0.8f) + FVector(0, 0, 14.f), FMath::Lerp(P0, P1, 0.8f) + FVector(120.f, 200.f, -12.f), 2.f, 10.f, RopeC());
	}
	// The cabin from the stern, dragged off by the later castaways and set on stones as a hen house (its door to -X, at
	// the cache): planked walls, a sloping roof of hull boards, nest boxes of straw inside, a perch ladder up to a pop
	// hole, a wattle run with a gap, feathers and shells in the trodden ground. Nobody keeps hens here now.
	{
		const FVector C(500.f, 120.f, G(500.f, 120.f));
		const FRotator R(0.f, 6.f, 0.f);
		const auto P = [&C, &R](float X, float Y, float Z) { return C + R.RotateVector(FVector(X, Y, Z)); };
		for (int32 s = 0; s < 4; ++s) K.Add(OldStone(0.8f), FVector(40.f, 40.f, 30.f), P(s % 2 ? 160.f : -160.f, s < 2 ? -120.f : 120.f, 5.f), R);
		K.Add(Timber(0.8f), FVector(340.f, 260.f, 10.f), P(0, 0, 25.f), R);                                   // floor
		for (const float Y : { -125.f, 125.f })
		{
			for (int32 b = 0; b < 6; ++b) K.Add(Plank(0.62f + 0.08f * (b % 3)), FVector(340.f, 6.f, 26.f), P(0, Y, 44.f + 27.f * b), R);
		}
		for (int32 b = 0; b < 6; ++b)
		{
			K.Add(Plank(0.6f + 0.08f * (b % 2)), FVector(6.f, 250.f, 26.f), P(165.f, 0, 44.f + 27.f * b), R);   // the back
			if (b < 5) { K.Add(Plank(0.65f), FVector(6.f, 80.f, 26.f), P(-165.f, -85.f, 44.f + 27.f * b), R); K.Add(Plank(0.65f), FVector(6.f, 80.f, 26.f), P(-165.f, 85.f, 44.f + 27.f * b), R); }
		}
		// The roof: hull boards still curved, lapped, sloping to the back.
		for (int32 b = 0; b < 7; ++b) K.Add(Timber(0.75f + 0.05f * (b % 3)), FVector(44.f, 300.f, 6.f), P(-170.f + 52.f * b, 0, 215.f - 7.f * b), FRotator(-8.f, R.Yaw, 0.f));
		// The ship's name board nailed over the door, the paint gone.
		K.Add(Carved(0.7f), FVector(6.f, 120.f, 20.f), P(-170.f, 0, 205.f), R);
		// Nest boxes along the back wall, straw in each; a cracked egg.
		for (int32 n = 0; n < 4; ++n)
		{
			K.Add(Timber(0.7f), FVector(50.f, 50.f, 4.f), P(140.f, -90.f + 60.f * n, 80.f), R);
			K.Add(Straw(), FVector(40.f, 40.f, 10.f), P(140.f, -90.f + 60.f * n, 88.f), R);
		}
		Ball(P(140.f, 30.f, 96.f), 3.f, Rgb(225, 214, 190) * 0.7f, 0.6f, 1.25f);
		// The perch ladder up to the pop hole in the side.
		K.Rod(Timber(0.8f), P(-120.f, -150.f, 0.f), P(-60.f, -128.f, 120.f), 5.f);
		K.Rod(Timber(0.8f), P(-90.f, -150.f, 0.f), P(-30.f, -128.f, 120.f), 5.f);
		for (int32 r = 1; r < 5; ++r) K.Rod(Timber(0.75f), P(-120.f + 15.f * r, -150.f + 5.5f * r, 24.f * r), P(-90.f + 15.f * r, -150.f + 5.5f * r, 24.f * r), 3.f);
		// The run: a wattle fence on stakes round the yard, sagging, one panel down.
		for (int32 f = 0; f < 9; ++f)
		{
			const float Ang = -60.f + 15.f * f;
			const FVector S0 = P(-170.f, 0, 0) + FRotator(0.f, R.Yaw + 180.f + Ang, 0.f).Vector() * 300.f;
			const float Gz = G(S0.X, S0.Y);
			K.Rod(Timber(0.7f), FVector(S0.X, S0.Y, Gz - 10.f), FVector(S0.X, S0.Y, Gz + 100.f), 6.f);
			if (f == 5) { K.Add(Bark(0.8f), FVector(80.f, 70.f, 6.f), FVector(S0.X, S0.Y, Gz + 4.f), FRotator(0.f, R.Yaw + Ang, 4.f)); continue; }
			if (f < 8) K.Add(Bark(0.75f), FVector(75.f, 5.f, 80.f), FVector(S0.X, S0.Y, Gz + 50.f) + FRotator(0.f, R.Yaw + 180.f + Ang + 7.5f + 90.f, 0.f).Vector() * 0.f, FRotator(0.f, R.Yaw + Ang + 7.5f + 90.f + 180.f, 0.f));
		}
		Box(P(0, 0, 120.f), FVector(170.f, 130.f, 100.f), R);
	}
	// The castaways' fire, long cold, a ring of ballast stones; the cache waits in front of the hen-house door.
	for (int32 s = 0; s < 8; ++s)
	{
		const FVector Sp = FVector(-150.f, 420.f, 0.f) + FRotator(0.f, 45.f * s, 0.f).Vector() * 70.f;
		K.Add(OldStone(0.7f), FVector(30.f, 26.f, 20.f), FVector(Sp.X, Sp.Y, G(Sp.X, Sp.Y) + 6.f), FRotator(0.f, 45.f * s, 0.f));
	}
	K.Add(Black(0.6f), FVector(90.f, 90.f, 4.f), FVector(-150.f, 420.f, G(-150.f, 420.f) + 2.f));
}

void WRForestLook::FBuild::Farmstead()
{
	using namespace WRForestSitesImpl;
	// DC 2.4 #2 (KEPT set-piece): a newcomer farm left in a hurry. The house at the origin's north, the farmer's tools hung
	// neatly on the gable (someone meant to come back), the oat field gone wild (server oat nodes at -1400..-900,
	// 1100..1800), the beehive tree (its hive node at 900, -1300) and the orchard with the one Idunn apple still fruiting
	// (its node at 1600, 900). The seed chest is the cache at (150, 80).
	// The house: an oak-framed cottage, roof still on but one wall's daub fallen in, no fire, no light (Var 2, ruined:
	// 9 m long, its back gable at local x -650 with the chimney on it).
	Cottage(FVector(-200.f, -500.f, G(-200.f, -500.f)), 0.f, 2, true);
	// The tool wall on the back gable, beside the chimney (the chimney spans y -55..55 off the house's axis): scythe,
	// hay rake, hoe, pitchfork, each on its own peg, the hafts hanging straight.
	{
		const float Gx = -200.f - 450.f - 14.f, Gy0 = -500.f + 70.f;
		const float Gz = G(Gx + 30.f, Gy0 + 80.f);
		const float Ys[4] = { Gy0 + 10.f, Gy0 + 60.f, Gy0 + 110.f, Gy0 + 150.f };
		for (int32 t2 = 0; t2 < 4; ++t2)
		{
			K.Add(Timber(1.1f), FVector(14.f, 4.f, 4.f), FVector(Gx + 6.f, Ys[t2], Gz + 190.f));   // the peg
			K.Rod(Timber(1.15f), FVector(Gx - 6.f, Ys[t2], Gz + 30.f), FVector(Gx - 6.f, Ys[t2], Gz + 188.f), 4.f);
		}
		K.Add(Iron(), FVector(4.f, 60.f, 8.f), FVector(Gx - 8.f, Ys[0] + 14.f, Gz + 186.f), FRotator(0.f, 0.f, -12.f));          // the scythe blade
		for (int32 tn = 0; tn < 7; ++tn) K.Add(Timber(1.1f), FVector(3.f, 3.f, 12.f), FVector(Gx - 8.f, Ys[1] - 18.f + 6.f * tn, Gz + 26.f));   // rake teeth
		K.Add(Timber(1.1f), FVector(5.f, 42.f, 5.f), FVector(Gx - 8.f, Ys[1], Gz + 32.f));
		K.Add(Iron(), FVector(4.f, 18.f, 12.f), FVector(Gx - 8.f, Ys[2], Gz + 26.f));                                       // the hoe
		for (int32 tn = 0; tn < 3; ++tn) K.Add(Iron(), FVector(2.f, 2.f, 28.f), FVector(Gx - 8.f, Ys[3] + 8.f * (tn - 1), Gz + 18.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
	}
	// A field-stone yard wall round the house and the field's walls: the newcomers' fresh field walls (2.20).
	DryWall(*this, FVector(-1700.f, 800.f, 0.f), FVector(-600.f, 800.f, 0.f), 110.f, 11, true);
	DryWall(*this, FVector(-600.f, 800.f, 0.f), FVector(-600.f, 2100.f, 0.f), 110.f, 31, false);
	DryWall(*this, FVector(-1700.f, 800.f, 0.f), FVector(-1700.f, 2100.f, 0.f), 110.f, 51, false);
	DryWall(*this, FVector(-1700.f, 2100.f, 0.f), FVector(-600.f, 2100.f, 0.f), 110.f, 71, false);
	// The oats gone wild: tufts thick in the old drills, thinning into the meadow grass, a fallen scarecrow in them.
	for (int32 r = 0; r < 9; ++r)
	{
		for (int32 c = 0; c < 9; ++c)
		{
			const float X = -1620.f + 110.f * c + 40.f * (Rand(r * 13 + c) - 0.5f), Y = 880.f + 135.f * r + 30.f * (Rand(r * 7 + c * 3) - 0.5f);
			if (Rand(r * 31 + c * 17) < 0.2f) continue;
			OatTuft(*this, FVector(X, Y, G(X, Y) - 4.f), r * 50 + c);
		}
	}
	{
		const FVector Sc(-1100.f, 1500.f, G(-1100.f, 1500.f) + 20.f);
		K.Rod(Timber(0.8f), Sc, Sc + FVector(180.f, 30.f, 30.f), 8.f);
		K.Rod(Timber(0.8f), Sc + FVector(140.f, -70.f, 22.f), Sc + FVector(140.f, 80.f, 26.f), 6.f);
		K.Add(Canvas(0.8f), FVector(70.f, 60.f, 20.f), Sc + FVector(110.f, 0.f, 22.f), FRotator(0.f, 10.f, 0.f));
		K.Add(Straw(), FVector(30.f, 30.f, 28.f), Sc + FVector(190.f, 30.f, 34.f));
		K.Add(Thatch(), FVector(54.f, 54.f, 10.f), Sc + FVector(196.f, 30.f, 52.f), FRotator(0.f, 0.f, 12.f), EWRLookMesh::Cone);
	}
	// The beehive tree: an old field oak, hollow at the foot where a Hiveback colony lives; comb shows in the hollow and
	// two old skeps on a board beside it (the farmer kept bees off the wild colony). The honey node is at (900, -1300).
	{
		const float OakH = 1900.f;
		const float Rr = WRTreeKit::ReferenceRadius(TEXT("oak")) * OakH / FMath::Max(1.f, WRTreeKit::ReferenceHeight(TEXT("oak")));
		const FVector Trunk(900.f - FMath::Max(60.f, Rr) - 20.f, -1300.f, G(900.f - Rr - 20.f, -1300.f));
		Tree(TEXT("oak"), 2, Trunk, OakH, FRotator(0.f, 30.f, 0.f));
		const FVector Hollow = Trunk + FVector(FMath::Max(60.f, Rr) * 0.9f, 0.f, 110.f);
		K.Add(Black(0.4f), FVector(14.f, 50.f, 90.f), Hollow);
		for (int32 c = 0; c < 4; ++c) K.Add(FSurf::Lit(Rgb(208, 150, 50) * 0.55f, 0.08f), FVector(4.f, 34.f, 50.f - 6.f * c), Hollow + FVector(-4.f, -18.f + 12.f * c, -6.f));
		K.Add(Timber(0.8f), FVector(70.f, 140.f, 8.f), FVector(Trunk.X + 60.f, Trunk.Y + 260.f, G(Trunk.X + 60.f, Trunk.Y + 260.f) + 60.f));
		for (const float Ys : { 220.f, 300.f })
		{
			K.Rod(Timber(0.8f), FVector(Trunk.X + 60.f, Trunk.Y + Ys, G(Trunk.X + 60.f, Trunk.Y + Ys) - 10.f), FVector(Trunk.X + 60.f, Trunk.Y + Ys, G(Trunk.X + 60.f, Trunk.Y + Ys) + 58.f), 8.f);
			Skep(*this, FVector(Trunk.X + 60.f, Trunk.Y + Ys, G(Trunk.X + 60.f, Trunk.Y + 260.f) + 64.f), 28.f);
		}
		Loop(TEXT("forest.bees"), Hollow + FVector(40.f, 0.f, 20.f));
	}
	// The orchard: three rows of old apple trees, knotted and unpruned, windfalls rotting under them; one tree (1600, 900)
	// still fruiting gold-red: the Idunn apple, the apples of youth (its node).
	for (int32 r = 0; r < 3; ++r)
	{
		for (int32 c = 0; c < 3; ++c)
		{
			const float X = 1200.f + 400.f * c, Y = 500.f + 400.f * r;
			const bool bIdunn = c == 1 && r == 1;
			const FVector Base(X, Y, G(X, Y));
			Tree(TEXT("oak"), (r + c) % 4, Base, bIdunn ? 620.f : 480.f + 80.f * Rand(r * 3 + c), FRotator(0.f, 70.f * (r * 3 + c), 0.f));
			const int32 Apples = bIdunn ? 14 : 3;
			for (int32 a = 0; a < Apples; ++a)
			{
				const float Ang = a * 137.5f;
				const FVector Ap = Base + FRotator(0.f, Ang, 0.f).Vector() * (bIdunn ? 120.f + 60.f * Rand(a) : 90.f + 80.f * Rand(a + 40)) + FVector(0, 0, bIdunn ? 260.f + 120.f * Rand(a + 9) : 0.f);
				if (bIdunn) Ball(Ap, 4.5f, Rgb(214, 140, 40) * 0.6f, 0.45f);                                           // gold-red, on the boughs
				else Ball(FVector(Ap.X, Ap.Y, G(Ap.X, Ap.Y) + 3.f), 4.f, Rgb(110, 70, 36) * 0.45f, 0.8f, 0.6f);       // windfalls, brown
			}
			if (bIdunn) Lamp(Base + FVector(0, 0, 300.f), FLinearColor(1.f, 0.72f, 0.35f), 250.f, 350.f);          // its apples catch the moon
		}
	}
}

// ================================================================================================ the streams (F2)

void WRForestLook::FBuild::MillRace()
{
	using namespace WRForestSitesImpl;
	// DC 2.4 #3 (KEPT set-piece), plan 2.5: the newcomers' overshot mill on a side stream of the Alder. The race (a stone
	// leat feeding a timber flume on trestles) brings the stream along +X to the top of the wheel; the wheel turned on an
	// oak shaft (axle along +Y) through a stone bearing into the walled gear pit, where the pit wheel drove the stones in
	// the mill house. The shaft has snapped at the pit wall; the wheel stands dead, buckets gone, and the two bronze
	// pinions are still on their oak arbors in the pit: the salvage (cache at -150, 560 on the pit floor).
	const float WheelR = WRTune(TEXT("forestWorld.millWheelRadius"), 230.f);
	const float WheelW = 90.f;
	const FVector2f WheelXY(-150.f, 150.f);
	const float Gw = FMath::Min(G(WheelXY.X, WheelXY.Y), G(WheelXY.X - 200.f, WheelXY.Y));
	const FVector Hub(WheelXY.X, WheelXY.Y, Gw + 40.f + WheelR);
	// The tailrace the wheel stands in: two stone walls either side of it, a flagged bed it drains along.
	for (const float Y : { WheelXY.Y - WheelW * 0.5f - 60.f, WheelXY.Y + WheelW * 0.5f + 40.f })
	{
		for (int32 c = 0; c < 4; ++c) K.Add(OldStone(0.75f + 0.08f * (c % 2)), FVector(WheelR * 2.6f, 36.f, 40.f), FVector(WheelXY.X, Y, Gw - 10.f + 40.f * c));
		Box(FVector(WheelXY.X, Y, Gw + 70.f), FVector(WheelR * 1.3f, 18.f, 80.f));
	}
	K.Add(OldStone(0.6f), FVector(WheelR * 2.6f, WheelW + 60.f, 10.f), FVector(WheelXY.X, WheelXY.Y, Gw - 2.f));
	K.Add(Water(), FVector(WheelR * 2.6f, WheelW + 50.f, 4.f), FVector(WheelXY.X, WheelXY.Y, Gw + 10.f));
	// The bearing pillar on the outer side: a dressed stone with a half-round seat, the stub of the shaft in it.
	const float OuterY = WheelXY.Y - WheelW * 0.5f - 60.f;
	K.Add(OldStone(0.85f), FVector(60.f, 60.f, Hub.Z - Gw + 20.f), FVector(WheelXY.X, OuterY - 40.f, (Hub.Z + Gw) * 0.5f - 10.f));
	K.Add(OldStone(0.9f), FVector(70.f, 70.f, 16.f), FVector(WheelXY.X, OuterY - 40.f, Hub.Z - 22.f));
	Box(FVector(WheelXY.X, OuterY - 40.f, (Hub.Z + Gw) * 0.5f), FVector(30.f, 30.f, (Hub.Z - Gw) * 0.5f + 10.f));
	// The shaft: oak, 36 cm, iron-banded, from the outer bearing through the hub to the pit wall's bearing (y 300).
	const float PitWallY = 300.f;
	K.Rod(Timber(0.85f), FVector(Hub.X, OuterY - 60.f, Hub.Z), FVector(Hub.X, PitWallY + 10.f, Hub.Z), 36.f);
	for (const float Y : { OuterY - 50.f, WheelXY.Y - 60.f, WheelXY.Y + 60.f, PitWallY - 30.f }) K.Disc(Iron(), FVector(Hub.X, Y, Hub.Z), FVector(0, 1, 0), 40.f, 8.f);
	// The wheel: two rims of curved felloes (FKit::Ring stands in XZ), 8 arms each into the hub, 24 buckets between the
	// rims; seven of them gone, one hanging by a nail.
	for (const float Y : { WheelXY.Y - WheelW * 0.5f, WheelXY.Y + WheelW * 0.5f })
	{
		K.Ring(Timber(0.8f), FVector(Hub.X, Y, Hub.Z), WheelR - 18.f, 18.f, 10.f, 16);
		for (int32 s = 0; s < 8; ++s)
		{
			if (Y > WheelXY.Y && s == 5) continue;    // an arm broken out of the far rim
			const float Ang = FMath::DegreesToRadians(22.5f + 45.f * s);
			K.Rod(Timber(0.75f), FVector(Hub.X, Y, Hub.Z) + FVector(FMath::Cos(Ang), 0.f, FMath::Sin(Ang)) * 30.f, FVector(Hub.X, Y, Hub.Z) + FVector(FMath::Cos(Ang), 0.f, FMath::Sin(Ang)) * (WheelR - 20.f), 11.f, true);
		}
	}
	K.Disc(Timber(0.7f), FVector(Hub.X, WheelXY.Y, Hub.Z), FVector(0, 1, 0), 70.f, WheelW + 30.f);   // the hub boss on the shaft
	for (int32 b = 0; b < 24; ++b)
	{
		if (b % 4 == 1 || b == 10 || b == 22) continue;
		const float Ang = 360.f * b / 24.f;
		const float Rad = FMath::DegreesToRadians(Ang);
		const FVector Rim = FVector(Hub.X, WheelXY.Y, Hub.Z) + FVector(FMath::Cos(Rad), 0.f, FMath::Sin(Rad)) * (WheelR - 10.f);
		K.Add(Plank(0.75f), FVector(46.f, WheelW - 6.f, 4.f), Rim, FRotator(Ang + 60.f, 0.f, 0.f));
		K.Add(Plank(0.7f), FVector(24.f, WheelW - 6.f, 4.f), Rim + FVector(FMath::Cos(Rad), 0.f, FMath::Sin(Rad)) * 12.f, FRotator(Ang + 10.f, 0.f, 0.f));
	}
	{
		const FVector Hang = FVector(Hub.X + WheelR * 0.7f, WheelXY.Y, Hub.Z - WheelR * 0.75f);
		K.Add(Plank(0.7f), FVector(46.f, WheelW - 6.f, 4.f), Hang + FVector(30.f, 0.f, -30.f), FRotator(60.f, 0.f, 25.f));
	}
	Box(FVector(Hub.X, WheelXY.Y, Hub.Z), FVector(WheelR, WheelW * 0.5f + 6.f, WheelR));
	// The flume: a timber trough on trestles from the leat's end (x -1500) to just over the wheel's crown, where the
	// water dropped into the buckets. Dry now but for a trickle; the stream runs the old leat beside it.
	{
		const float FlumeZ = Hub.Z + WheelR + 25.f;
		const float X0 = -1500.f, X1 = WheelXY.X + 20.f;
		const float Y = WheelXY.Y;
		K.Add(Timber(0.75f), FVector(X1 - X0, WheelW + 20.f, 8.f), FVector((X0 + X1) * 0.5f, Y, FlumeZ));
		for (const float Sd : { -1.f, 1.f }) K.Add(Plank(0.7f), FVector(X1 - X0, 6.f, 46.f), FVector((X0 + X1) * 0.5f, Y + Sd * (WheelW * 0.5f + 7.f), FlumeZ + 23.f));
		K.Add(Water(), FVector(X1 - X0 - 40.f, 30.f, 3.f), FVector((X0 + X1) * 0.5f, Y, FlumeZ + 6.f));
		// The sluice gate at the flume's head, its paddle down.
		K.Add(Timber(0.8f), FVector(8.f, WheelW + 10.f, 70.f), FVector(X0 + 30.f, Y, FlumeZ + 40.f));
		K.Rod(Timber(0.8f), FVector(X0 + 30.f, Y, FlumeZ + 70.f), FVector(X0 + 30.f, Y, FlumeZ + 160.f), 6.f);
		for (float X = X0 + 120.f; X < X1 - 60.f; X += 260.f)
		{
			const float Gz = G(X, Y);
			for (const float Sd : { -1.f, 1.f })
			{
				K.Rod(Timber(0.7f), FVector(X, Y + Sd * 80.f, Gz - 20.f), FVector(X, Y + Sd * 55.f, FlumeZ - 6.f), 14.f, true);
			}
			K.Add(Timber(0.75f), FVector(16.f, 200.f, 16.f), FVector(X, Y, FlumeZ - 14.f));
			K.Rod(Timber(0.65f), FVector(X, Y - 75.f, Gz + 20.f), FVector(X, Y + 60.f, FlumeZ - 40.f), 8.f, true);   // the cross-brace
		}
		SegBox(FVector(X0, Y, FlumeZ), FVector(X1, Y, FlumeZ), WheelW * 0.5f + 12.f, 25.f);
		// The leat at its head: a stone channel from the stream's bank.
		for (const float Sd : { -1.f, 1.f })
		{
			for (int32 s = 0; s < 6; ++s)
			{
				const float X = X0 - 90.f - 120.f * s;
				K.Add(OldStone(0.7f + 0.1f * (s % 2)), FVector(118.f, 30.f, 50.f), FVector(X, Y + Sd * 70.f, G(X, Y + Sd * 70.f) + 10.f));
			}
		}
		Loop(TEXT("forest.trickle"), FVector(X1 - 10.f, Y, FlumeZ - 20.f));
	}
	// The gear pit: a stone-walled chamber on the far side of the wheel's bearing wall, flagged, stepped down into by
	// three stone steps on the mill-house side. The shaft broke where it came through the wall: its inner length lies
	// across the pit floor, the iron-toothed pit wheel still on it, cracked. Two bronze pinions on their oak arbors stand
	// in the corners: the "gears x2" a smith would carry off. The cache lies on the flags at (-150, 560).
	{
		const float PX0 = -440.f, PX1 = 140.f, PY0 = PitWallY, PY1 = 780.f;
		const float Gp = G(-150.f, 560.f);
		const float FloorZ = Gp + 6.f;
		for (float X = PX0 + 50.f; X < PX1; X += 100.f)
		{
			for (float Y = PY0 + 50.f; Y < PY1; Y += 100.f) K.Add(OldStone(0.65f + 0.15f * Rand(int32(X + Y))), FVector(96.f, 96.f, 10.f), FVector(X, Y, FloorZ - 5.f));
		}
		Box(FVector((PX0 + PX1) * 0.5f, (PY0 + PY1) * 0.5f, FloorZ - 20.f), FVector((PX1 - PX0) * 0.5f, (PY1 - PY0) * 0.5f, 20.f));
		const float WallH = 170.f;
		// Walls: the bearing wall (y 300) with the shaft's seat, the far wall, the outer wall, and the house side with steps.
		for (int32 c = 0; c < 5; ++c)
		{
			const float Z = FloorZ + 18.f + 34.f * c;
			K.Add(OldStone(0.7f + 0.06f * (c % 3)), FVector(PX1 - PX0 + 60.f, 50.f, 34.f), FVector((PX0 + PX1) * 0.5f, PY0 - 20.f, Z));
			K.Add(OldStone(0.72f + 0.05f * (c % 2)), FVector(PX1 - PX0 + 60.f, 50.f, 34.f), FVector((PX0 + PX1) * 0.5f, PY1 + 20.f, Z));
			K.Add(OldStone(0.7f + 0.07f * (c % 2)), FVector(50.f, PY1 - PY0, 34.f), FVector(PX0 - 20.f, (PY0 + PY1) * 0.5f, Z));
			if (c < 3) K.Add(OldStone(0.74f), FVector(50.f, PY1 - PY0 - 200.f, 34.f), FVector(PX1 + 20.f, (PY0 + PY1) * 0.5f + 100.f, Z));
		}
		Box(FVector((PX0 + PX1) * 0.5f, PY0 - 20.f, FloorZ + WallH * 0.5f), FVector((PX1 - PX0) * 0.5f + 30.f, 25.f, WallH * 0.5f));
		Box(FVector((PX0 + PX1) * 0.5f, PY1 + 20.f, FloorZ + WallH * 0.5f), FVector((PX1 - PX0) * 0.5f + 30.f, 25.f, WallH * 0.5f));
		Box(FVector(PX0 - 20.f, (PY0 + PY1) * 0.5f, FloorZ + WallH * 0.5f), FVector(25.f, (PY1 - PY0) * 0.5f, WallH * 0.5f));
		Box(FVector(PX1 + 20.f, (PY0 + PY1) * 0.5f + 100.f, FloorZ + 50.f), FVector(25.f, (PY1 - PY0) * 0.5f - 100.f, 50.f));
		// The steps down from the mill house (the gap in the house-side wall at y 300..500).
		for (int32 s = 0; s < 3; ++s) K.Add(OldStone(0.8f), FVector(60.f, 180.f, 20.f), FVector(PX1 + 60.f + 50.f * s, PY0 + 100.f, FloorZ + 10.f + 20.f * s));
		// The shaft's seat in the bearing wall: a stone block with a worn half-round, the shaft's broken face just inside.
		K.Add(OldStone(0.95f), FVector(80.f, 60.f, 50.f), FVector(Hub.X, PY0 - 20.f, Hub.Z - 30.f));
		for (int32 s = 0; s < 6; ++s) K.Spike(Timber(0.85f), FVector(Hub.X + (s - 2.5f) * 6.f, PY0 + 10.f, Hub.Z + (s % 3 - 1) * 8.f), FVector(0.f, 1.f, (s % 2) * 0.3f - 0.15f), 7.f, 20.f + 10.f * (s % 3));
		// The inner length of the shaft fallen into the pit, one end on the floor, one propped on the pit wheel's rim.
		const FVector S0(-400.f, PY0 + 60.f, FloorZ + 18.f), S1(-270.f, PY1 - 60.f, FloorZ + 80.f);   // along the outer wall, clear of the flags at (-150, 560)
		K.Rod(Timber(0.8f), S0, S1, 34.f);
		K.Disc(Iron(), FMath::Lerp(S0, S1, 0.15f), S1 - S0, 38.f, 8.f);
		// The pit wheel: an oak spur wheel with iron-shod cogs, cracked and leaning on the far wall, the shaft through it.
		const FVector PitWheel = FMath::Lerp(S0, S1, 0.82f);
		{
			const FRotator Face = (S1 - S0).Rotation();
			K.Disc(Timber(0.72f), PitWheel, S1 - S0, 170.f, 16.f);
			K.Disc(Timber(0.6f), PitWheel, S1 - S0, 60.f, 30.f);
			for (int32 c = 0; c < 20; ++c)
			{
				if (c == 6 || c == 7) continue;   // two cogs sheared off where it cracked
				const FVector Dir = FRotationMatrix(Face).TransformVector(FVector(0.f, FMath::Cos(c * UE_TWO_PI / 20.f), FMath::Sin(c * UE_TWO_PI / 20.f)));
				K.Add(Iron(), FVector(8.f, 10.f, 14.f), PitWheel + Dir * 90.f, FRotationMatrix::MakeFromZX(Dir, S1 - S0).Rotator());
			}
			K.Add(Black(0.7f), FVector(2.f, 4.f, 80.f), PitWheel + FVector(0.f, 0.f, 40.f), Face);   // the crack
		}
		// The two bronze pinions, each a lantern pinion on a short oak arbor: one still standing in its floor bearing,
		// one tipped over against the house-side wall. Green with age.
		const FSurf BronzeGreen = FSurf::Steel(Rgb(90, 120, 90) * 0.45f, 0.6f);
		{
			const FVector A0(20.f, PY1 - 90.f, FloorZ);
			K.Add(OldStone(0.9f), FVector(50.f, 50.f, 14.f), A0 + FVector(0, 0, 7.f));
			K.Rod(Timber(0.75f), A0, A0 + FVector(0, 0, 150.f), 14.f);
			K.Disc(BronzeGreen, A0 + FVector(0, 0, 100.f), FVector::UpVector, 64.f, 10.f);
			K.Disc(BronzeGreen, A0 + FVector(0, 0, 125.f), FVector::UpVector, 64.f, 6.f);
			for (int32 s = 0; s < 10; ++s) K.Rod(BronzeGreen, A0 + FVector(FMath::Cos(s * UE_TWO_PI / 10.f) * 26.f, FMath::Sin(s * UE_TWO_PI / 10.f) * 26.f, 100.f), A0 + FVector(FMath::Cos(s * UE_TWO_PI / 10.f) * 26.f, FMath::Sin(s * UE_TWO_PI / 10.f) * 26.f, 125.f), 5.f);
			Box(A0 + FVector(0, 0, 75.f), FVector(34.f, 34.f, 75.f));
		}
		{
			const FVector A0(60.f, 380.f, FloorZ + 30.f);
			const FVector Lean = FVector(0.85f, 0.1f, 0.5f).GetSafeNormal();
			K.Rod(Timber(0.72f), A0 - Lean * 70.f, A0 + Lean * 70.f, 14.f);
			K.Disc(BronzeGreen, A0 + Lean * 10.f, Lean, 64.f, 10.f);
			K.Disc(BronzeGreen, A0 + Lean * 35.f, Lean, 64.f, 6.f);
			Box(A0, FVector(40.f, 40.f, 30.f));
		}
	}
	// The mill house beside the pit: two walls standing to the eaves, the others fallen to the sill, the roof gone; the
	// runner stone split in half on the floor, the bed stone still set in its frame, a hopper's frame askew.
	{
		const float HX0 = 200.f, HX1 = 900.f, HY0 = 200.f, HY1 = 900.f;
		const float Gh = G((HX0 + HX1) * 0.5f, (HY0 + HY1) * 0.5f);
		for (int32 c = 0; c < 9; ++c)
		{
			const float Z = Gh + 20.f + 40.f * c;
			K.Add(OldStone(0.72f + 0.06f * (c % 3)), FVector(HX1 - HX0, 50.f, 40.f), FVector((HX0 + HX1) * 0.5f, HY1, Z));
			K.Add(OldStone(0.7f + 0.06f * (c % 2)), FVector(50.f, HY1 - HY0, 40.f), FVector(HX1, (HY0 + HY1) * 0.5f, Z));
			if (c < 2) K.Add(OldStone(0.7f), FVector(HX1 - HX0 - 200.f, 50.f, 40.f), FVector((HX0 + HX1) * 0.5f + 100.f, HY0, Z));
		}
		Box(FVector((HX0 + HX1) * 0.5f, HY1, Gh + 180.f), FVector((HX1 - HX0) * 0.5f, 25.f, 180.f));
		Box(FVector(HX1, (HY0 + HY1) * 0.5f, Gh + 180.f), FVector(25.f, (HY1 - HY0) * 0.5f, 180.f));
		// Rubble where the other walls fell.
		for (int32 r = 0; r < 10; ++r) K.Add(OldStone(0.65f), FVector(50.f, 40.f, 30.f), FVector(HX0 + 40.f * r + 20.f * Rand(r), HY0 + 70.f + 60.f * Rand(r + 7), Gh + 10.f), FRotator(10.f * Rand(r + 3), 50.f * r, 0.f));
		// The bed stone in its timber hurst frame, the runner stone broken in two beside it, the hopper fallen.
		const FVector Hurst(600.f, 600.f, Gh);
		for (const float Y : { -90.f, 90.f }) K.Add(Timber(0.75f), FVector(240.f, 22.f, 22.f), Hurst + FVector(0, Y, 80.f));
		for (const float X : { -110.f, 110.f }) for (const float Y : { -90.f, 90.f }) K.Rod(Timber(0.75f), Hurst + FVector(X, Y, 0.f), Hurst + FVector(X, Y, 90.f), 18.f, true);
		K.Disc(OldStone(1.f), Hurst + FVector(0, 0, 100.f), FVector::UpVector, 150.f, 22.f);
		K.Disc(OldStone(0.6f), Hurst + FVector(0, 0, 112.f), FVector::UpVector, 20.f, 4.f);
		K.Add(OldStone(0.95f), FVector(140.f, 70.f, 22.f), Hurst + FVector(-170.f, 120.f, 12.f), FRotator(0.f, 20.f, 3.f), EWRLookMesh::Cylinder);
		K.Add(OldStone(0.95f), FVector(140.f, 70.f, 22.f), Hurst + FVector(-150.f, 200.f, 22.f), FRotator(12.f, 70.f, 0.f), EWRLookMesh::Cylinder);
		K.Add(Timber(0.7f), FVector(60.f, 60.f, 50.f), Hurst + FVector(80.f, -120.f, 30.f), FRotator(30.f, 15.f, 10.f), EWRLookMesh::Cone);
		Box(Hurst + FVector(0, 0, 55.f), FVector(120.f, 100.f, 55.f));
		// A sack of grain gone to seedlings in the corner: oats sprouting through the rotted cloth.
		K.Add(Canvas(0.6f), FVector(60.f, 45.f, 40.f), FVector(HX1 - 80.f, HY1 - 80.f, Gh + 20.f));
		OatTuft(*this, FVector(HX1 - 80.f, HY1 - 80.f, Gh + 30.f), 900);
	}
}

void WRForestLook::FBuild::HernesFord()
{
	using namespace WRForestSitesImpl;
	// DC 2.4 #11: where the elk trails cross the Alder: a wide gravel ford, the banks hoof-churned to mud, a Grovekin fish
	// weir of stakes downstream (a fishing spot), an old shed antler bleached on the bank and a fresh one in the
	// shallows (the server's node at 300, 200). On the Hornwarden's route; the river runs along local Y.
	// The gravel bar: pebbles and cobbles in a broad fan across the crossing.
	for (int32 i = 0; i < 70; ++i)
	{
		const float X = -500.f + 1000.f * Rand(i * 3), Y = -700.f + 1400.f * Rand(i * 3 + 1);
		const float Sz = 12.f + 20.f * Rand(i * 3 + 2);
		K.Add(OldStone(0.6f + 0.35f * Rand(i + 70)), FVector(Sz, Sz * 0.8f, Sz * 0.45f), FVector(X, Y, G(X, Y) + Sz * 0.15f), FRotator(0.f, 37.f * i, 0.f));
	}
	// The churned mud on both banks where the trails come down: a dark trodden apron, hoof slots pressed in it.
	for (const float Bank : { -1.f, 1.f })
	{
		const float X0 = Bank * 650.f;
		for (int32 p = 0; p < 6; ++p) K.Add(Earth(0.7f + 0.1f * (p % 2)), FVector(220.f, 260.f, 4.f), FVector(X0 + Bank * 70.f * (p / 3), -260.f + 170.f * (p % 3), G(X0 + Bank * 70.f * (p / 3), -260.f + 170.f * (p % 3)) + 1.f), FRotator(0.f, 20.f * p, 0.f));
		for (int32 h = 0; h < 30; ++h)
		{
			const float X = X0 + Bank * (-80.f + 260.f * Rand(h + int32(Bank * 50.f) + 200)), Y = -380.f + 760.f * Rand(h * 5 + int32(Bank * 9.f) + 300);
			const float Yaw = Bank > 0.f ? 180.f : 0.f;
			for (const float Sd : { -1.f, 1.f }) K.Add(Black(0.45f), FVector(9.f, 4.f, 2.f), FVector(X, Y + Sd * 3.5f, G(X, Y) + 2.f), FRotator(0.f, Yaw + Sd * 8.f + 30.f * (Rand(h) - 0.5f), 0.f));
		}
		// The elk trail running up the bank into the trees: a worn strip of bare earth.
		for (int32 s = 0; s < 6; ++s)
		{
			const float X = X0 + Bank * (250.f + 200.f * s), Y = 60.f * FMath::Sin(s * 0.9f);
			K.Add(Earth(0.85f), FVector(210.f, 90.f, 3.f), FVector(X, Y, G(X, Y) + 1.f), FRotator(0.f, 15.f * FMath::Cos(s * 0.9f), 0.f));
		}
	}
	// The old antler on the bank, bleached, half in the grass (the fresh one is the node in the shallows).
	Antler(FVector(-720.f, 260.f, G(-720.f, 260.f) + 8.f), FVector(0.3f, 1.f, -0.35f), 140.f, false, BoneC());
	// The Grovekin fish weir downstream (+Y): a V of hazel stakes pointing downstream to a wicker trap.
	for (int32 s = 0; s < 14; ++s)
	{
		const float U = (s % 7) / 6.f;
		const float Sd = s < 7 ? -1.f : 1.f;
		const float X = Sd * (400.f * (1.f - U)), Y = 900.f + 300.f * U;
		const float Gz = G(X, Y);
		K.Rod(Bark(1.1f), FVector(X, Y, Gz - 30.f), FVector(X, Y, Gz + 90.f + 20.f * Rand(s)), 7.f);
		if (s % 7 < 6) K.Add(Bark(0.9f), FVector(400.f / 6.f + 10.f, 4.f, 40.f), FVector(X - Sd * 33.f, Y + 25.f, Gz + 40.f), FRotator(0.f, Sd > 0.f ? 143.f : 37.f, 0.f));
	}
	Basket(*this, FVector(0.f, 1230.f, G(0.f, 1230.f) + 30.f), 30.f, 90.f, 82.f);
	// Where the river is: a sheen of shallow water over the gravel in the middle of the crossing.
	K.Add(Water(), FVector(500.f, 1600.f, 3.f), FVector(0.f, 0.f, G(0.f, 0.f) + 12.f));
}

void WRForestLook::FBuild::FallsPool()
{
	using namespace WRForestSitesImpl;
	// DC 2.4 #12, 2.11: the Grovekin fishing platform on stilts in the Great Falls' pool, where Grandfather Sturgeon
	// lives. A plank deck on oak stilts 60-80 cm over the water (the Falls Shrine stands on its middle: the shrine's
	// footing is drawn by its own builder at (0,0)), a duckboard walk to the bank (-X), racks of fishing spears, a fish
	// drying rack, a net hung to dry. The spray of the falls is its own (WRForestSceneryLandmarks).
	const FWRForestData& D = FWRForestData::Get();
	float Surface = G(0.f, 0.f) + 40.f;
	{
		float Best = 8000.f;
		for (const FWRForestData::FFall& F : D.Falls)
		{
			const float Dd = FVector::Dist2D(F.Bottom, Act->GetActorLocation());
			if (Dd < Best) { Best = Dd; Surface = Local(F.Bottom).Z; }
		}
	}
	const float DeckZ = Surface + WRTune(TEXT("forestWorld.fallsDeckLift"), 70.f);
	const float HX = 320.f, HY = 240.f;
	Deck(FVector::ZeroVector, FVector2D(HX, HY), DeckZ, 0.f, {}, {}, true);
	// Stilts: oak poles down to the pool bed, in pairs with cross braces lashed on; a mossy waterline.
	for (int32 xi = 0; xi < 4; ++xi)
	{
		for (const float Y : { -HY + 20.f, HY - 20.f })
		{
			const float X = -HX + 20.f + (2.f * HX - 40.f) * xi / 3.f;
			const float Bed = FMath::Min(G(X, Y, Surface - 150.f), Surface - 60.f);
			K.Rod(Bark(0.9f), FVector(X, Y, Bed - 30.f), FVector(X, Y, DeckZ - 20.f), 22.f);
			K.Add(DeepMoss(1.1f), FVector(25.f, 25.f, 18.f), FVector(X, Y, Surface + 4.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			Box(FVector(X, Y, (Bed + DeckZ) * 0.5f), FVector(11.f, 11.f, (DeckZ - Bed) * 0.5f));
		}
		const float X = -HX + 20.f + (2.f * HX - 40.f) * xi / 3.f;
		K.Rod(Timber(0.75f), FVector(X, -HY + 20.f, Surface + 10.f), FVector(X, HY - 20.f, DeckZ - 40.f), 9.f);
		Line(FVector(X, -HY + 20.f, Surface + 12.f), FVector(X, -HY + 20.f, Surface + 30.f), 2.5f, 0.f, RopeC(), 2);
	}
	// The duckboard walk to the bank: planks on short stakes, rising to the bank's height.
	{
		const float BankX = -HX - 900.f;
		const FVector W0(-HX, 0.f, DeckZ), W1(BankX, 0.f, FMath::Max(G(BankX, 0.f) + 10.f, DeckZ - 40.f));
		const int32 Planks = 30;
		for (int32 p = 0; p < Planks; ++p)
		{
			const FVector C = FMath::Lerp(W0, W1, (p + 0.5f) / Planks);
			K.Add(Plank(0.8f + 0.15f * Rand(p)), FVector(26.f, 110.f, 5.f), C - FVector(0, 0, 3.f), FRotator((W1 - W0).Rotation().Pitch, 0.f, 0.f));
			if (p % 5 == 0)
			{
				const float Bed = FMath::Min(G(C.X, C.Y, Surface - 100.f), C.Z - 40.f);
				for (const float Sd : { -1.f, 1.f }) K.Rod(Bark(0.85f), FVector(C.X, C.Y + Sd * 50.f, Bed - 20.f), FVector(C.X, C.Y + Sd * 50.f, C.Z - 6.f), 12.f);
			}
		}
		SegBox(W0 - FVector(0, 0, 6.f), W1 - FVector(0, 0, 6.f), 55.f, 6.f);
	}
	// The fishing gear: a rack of leister spears (three-pronged), a fish drying frame with split trout on it, a net
	// stretched on a frame to dry. Kept to the deck's edges: the middle is the shrine's.
	for (int32 s = 0; s < 4; ++s)
	{
		const FVector Foot(240.f, -200.f + 20.f * s, DeckZ);
		K.Rod(Bark(1.1f), Foot, Foot + FVector(-30.f, 0, 280.f), 4.f);
		for (int32 p = 0; p < 3; ++p) K.Spike(Iron(), Foot + FVector(-30.f, (p - 1) * 4.f, 280.f), FVector(-0.1f, (p - 1) * 0.08f, 1.f), 2.f, 22.f);
	}
	K.Add(Timber(0.8f), FVector(10.f, 100.f, 10.f), FVector(225.f, -170.f, DeckZ + 190.f));
	{
		const FVector F0(-260.f, 150.f, DeckZ);
		for (const float X : { 0.f, 160.f }) K.Rod(Bark(1.f), F0 + FVector(X, 0, 0), F0 + FVector(X, 0, 170.f), 6.f);
		for (int32 r = 0; r < 3; ++r)
		{
			K.Rod(Bark(0.9f), F0 + FVector(0, 0, 80.f + 40.f * r), F0 + FVector(160.f, 0, 80.f + 40.f * r), 3.f);
			for (int32 f = 0; f < 4; ++f) K.Add(FSurf::Cloth(Rgb(170, 120, 90) * 0.45f), FVector(4.f, 12.f, 30.f), F0 + FVector(20.f + 38.f * f, 0, 64.f + 40.f * r), FRotator(0.f, 90.f, 0.f));
		}
	}
	{
		const FVector N0(100.f, 225.f, DeckZ);
		for (const float X : { 0.f, 180.f }) K.Rod(Bark(1.f), N0 + FVector(X, 0, 0), N0 + FVector(X, 0, 180.f), 6.f);
		K.Rod(Bark(1.f), N0 + FVector(0, 0, 175.f), N0 + FVector(180.f, 0, 175.f), 5.f);
		for (int32 g = 0; g < 7; ++g) Line(N0 + FVector(10.f + 26.f * g, 0, 172.f), N0 + FVector(10.f + 26.f * g + 6.f, 4.f, 30.f), 0.8f, 6.f, RopeC(), 4);
		for (int32 g = 0; g < 6; ++g) Line(N0 + FVector(8.f, 0, 150.f - 22.f * g), N0 + FVector(172.f, 0, 150.f - 22.f * g), 0.8f, 10.f, RopeC(), 6);
	}
	// A creel and a gutting board by the walk's head.
	Basket(*this, FVector(-280.f, -150.f, DeckZ), 22.f, 40.f, 0.f);
	K.Add(Timber(0.9f), FVector(70.f, 30.f, 6.f), FVector(-200.f, -190.f, DeckZ + 3.f));
}

void WRForestLook::FBuild::RootBridge()
{
	using namespace WRForestSitesImpl;
	// DC 2.4 #13 (the Meghalaya root bridges): the main crossing from the streams (F2) to the Elder Canopy (F5). Two old
	// beeches stand on the gorge rims; the Grovekin trained their roots out over a 15 m gorge on a hazel frame long since
	// rotted, until they met and grew into one woven deck: still alive, still thickening. Moss rails at hand height,
	// tied down by living root hangers. The actor stands at the bridge's middle, +X along it.
	const float Span = WRTune(TEXT("forestWorld.rootBridgeSpan"), 1800.f);
	// The rims: the highest ground on each side of the gorge, sought along the bridge line.
	FVector Rim[2];
	for (int32 s = 0; s < 2; ++s)
	{
		const float Sg = s == 0 ? -1.f : 1.f;
		float BestX = Sg * Span * 0.5f, BestZ = -1e9f;
		for (int32 i = 0; i <= 12; ++i)
		{
			const float X = Sg * FMath::Lerp(Span * 0.3f, Span * 0.75f, i / 12.f);
			const float Z = G(X, 0.f, -1e9f);
			if (Z > BestZ + 30.f) { BestZ = Z; BestX = X; }
		}
		if (BestZ < -1e8f) BestZ = 0.f;
		Rim[s] = FVector(BestX, 0.f, BestZ + 15.f);
	}
	const float Len = FVector::Dist(Rim[0], Rim[1]);
	const float SagD = Len * WRTune(TEXT("forestWorld.rootBridgeSag"), 0.05f);
	const FVector Dir = (Rim[1] - Rim[0]).GetSafeNormal();
	const FVector Side = FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal();
	const FLinearColor RootCol = RootC(), Mossy = MossC();
	// The deck: nine living roots side by side along the sag, thick at their tree's end and thin where they meet the
	// other tree's, each root reaching most of the way across from its own rim (they overlap in the middle where they
	// fused); flat stones pressed into the weave at the walking line (the Grovekin's paving, now grown over).
	for (int32 r = 0; r < 9; ++r)
	{
		const float Y = -100.f + 25.f * r;
		const bool bFromA = r % 2 == 0;
		TArray<FVector> Pts;
		for (int32 i = 0; i <= 14; ++i)
		{
			const float U = bFromA ? FMath::Lerp(-0.05f, 0.9f, i / 14.f) : FMath::Lerp(1.05f, 0.1f, i / 14.f);
			const float Uc = FMath::Clamp(U, 0.f, 1.f);
			Pts.Add(Sag(Rim[0], Rim[1], Uc, SagD) + Side * Y + FVector(0, 0, -10.f - 4.f * FMath::Sin(i * 1.3f + r)) + (U < 0.f || U > 1.f ? -FVector(0, 0, 40.f) : FVector::ZeroVector));
		}
		Root(Pts, 15.f + 3.f * (r % 3), 6.f, RootCol * (0.9f + 0.1f * (r % 3)), 7);
	}
	// The cross-weave every 45 cm: thinner roots looped over and under the long ones.
	const int32 Weaves = FMath::Max(8, FMath::RoundToInt(Len / 45.f));
	for (int32 w = 1; w < Weaves; ++w)
	{
		const float U = float(w) / Weaves;
		const FVector C = Sag(Rim[0], Rim[1], U, SagD);
		Limb(C - Side * 112.f - FVector(0, 0, 8.f), C + Side * 112.f - FVector(0, 0, 8.f + 6.f * (w % 2)), 4.5f, 4.5f, RootCol * 0.85f, 6);
		if (w % 3 == 0) K.Add(OldStone(0.7f), FVector(36.f, 50.f, 8.f), C + Side * (20.f * FMath::Sin(w * 2.1f)) + FVector(0, 0, 4.f), FRotator(0.f, Dir.Rotation().Yaw + 20.f * w, 0.f));
		if (w % 2 == 1) K.Add(DeepMoss(1.f), FVector(40.f, 70.f + 40.f * Rand(w), 3.f), C + Side * (70.f * (Rand(w + 5) - 0.5f)) + FVector(0, 0, 6.f), FRotator(0.f, Dir.Rotation().Yaw, 0.f));
	}
	// The rails: a root each side at hand height, mossed, held up by living hangers looping down to the deck edge; they
	// rise from each tree's base like arms.
	for (const float Sd : { -1.f, 1.f })
	{
		TArray<FVector> Rail;
		for (int32 i = 0; i <= 16; ++i) Rail.Add(Sag(Rim[0] + FVector(0, 0, 105.f), Rim[1] + FVector(0, 0, 105.f), i / 16.f, SagD * 0.7f) + Side * Sd * 120.f);
		Root(Rail, 9.f, 9.f, RootCol, 7);
		for (int32 i = 1; i < 16; i += 2) K.Add(Moss(1.f), FVector(80.f, 22.f, 8.f), Rail[i] + FVector(0, 0, 8.f), FRotator((Rail[i + 1] - Rail[i - 1]).Rotation().Pitch, Dir.Rotation().Yaw, 0.f));
		const int32 Hangers = FMath::Max(4, FMath::RoundToInt(Len / 120.f));
		for (int32 h = 1; h < Hangers; ++h)
		{
			const float U = float(h) / Hangers;
			const FVector Hi = Sag(Rim[0] + FVector(0, 0, 105.f), Rim[1] + FVector(0, 0, 105.f), U, SagD * 0.7f) + Side * Sd * 120.f;
			const FVector Lo = Sag(Rim[0], Rim[1], U, SagD) + Side * Sd * 108.f;
			Root({ Hi, (Hi + Lo) * 0.5f + Side * Sd * 10.f, Lo }, 4.f, 5.f, RootCol * 0.9f, 5);
		}
		// Ferns and moss hang under the deck edge (a living bridge: it grows).
		for (int32 f = 1; f < 6; ++f) K.Add(Moss(0.85f), FVector(30.f, 4.f, 60.f + 30.f * Rand(f + int32(Sd * 3.f))), Sag(Rim[0], Rim[1], f / 6.f, SagD) + Side * Sd * 118.f - FVector(0, 0, 50.f), FRotator(0.f, Dir.Rotation().Yaw, 0.f));
	}
	// Collision: the deck follows the sag in short boxes; the rails keep you on it.
	const int32 Segs = FMath::Max(4, FMath::RoundToInt(Len / 200.f));
	for (int32 i = 0; i < Segs; ++i)
	{
		const FVector A0 = Sag(Rim[0], Rim[1], float(i) / Segs, SagD), A1 = Sag(Rim[0], Rim[1], float(i + 1) / Segs, SagD);
		SegBox(A0 - FVector(0, 0, 8.f), A1 - FVector(0, 0, 8.f), 115.f, 8.f);
		for (const float Sd : { -1.f, 1.f })
		{
			const FVector R0 = Sag(Rim[0] + FVector(0, 0, 105.f), Rim[1] + FVector(0, 0, 105.f), float(i) / Segs, SagD * 0.7f) + Side * Sd * 120.f;
			const FVector R1 = Sag(Rim[0] + FVector(0, 0, 105.f), Rim[1] + FVector(0, 0, 105.f), float(i + 1) / Segs, SagD * 0.7f) + Side * Sd * 120.f;
			SegBox(R0 - FVector(0, 0, 50.f), R1 - FVector(0, 0, 50.f), 6.f, 55.f);
		}
	}
	// The two trees on the rims, and the masses of root that pour from their feet over the edge into the deck: the
	// bridge's two ends. Each tree stands 5 m back from its rim and a little to the side.
	for (int32 s = 0; s < 2; ++s)
	{
		const float Sg = s == 0 ? -1.f : 1.f;
		const FVector TreeAt = Rim[s] + Dir * Sg * 520.f + Side * (s == 0 ? 160.f : -180.f);
		const FVector TreeBase(TreeAt.X, TreeAt.Y, G(TreeAt.X, TreeAt.Y));
		Tree(TEXT("beech"), s + 1, TreeBase, 3200.f + 400.f * s, FRotator(0.f, 40.f + 120.f * s, 0.f));
		// The mass: a dozen roots from the bole's flare running over the rim, gathering into the deck's strands.
		for (int32 r = 0; r < 12; ++r)
		{
			const float Off = -110.f + 20.f * r;
			const FVector From = TreeBase + FRotator(0.f, (-Dir * Sg).Rotation().Yaw + (r - 5.5f) * 9.f, 0.f).Vector() * 90.f + FVector(0, 0, 60.f + 20.f * (r % 3));
			const FVector Mid = Rim[s] + Dir * Sg * 220.f + Side * Off * 1.3f + FVector(0, 0, G(Rim[s].X + Dir.X * Sg * 220.f, Rim[s].Y + Dir.Y * Sg * 220.f, Rim[s].Z) - Rim[s].Z + 15.f);
			const FVector To = Rim[s] - Dir * Sg * 60.f + Side * Off - FVector(0, 0, 8.f);
			Root({ From, Mid, To }, 26.f - (r % 4) * 3.f, 14.f, RootCol * (0.85f + 0.05f * (r % 3)), 8);
			if (r % 3 == 0) K.Add(Moss(1.1f), FVector(70.f, 30.f, 6.f), Mid + FVector(0, 0, 18.f), FRotator(0.f, Dir.Rotation().Yaw, 0.f));
		}
		SegBox(Rim[s] + FVector(0, 0, -8.f), Rim[s] + Dir * Sg * 300.f + FVector(0, 0, G(Rim[s].X + Dir.X * Sg * 300.f, Rim[s].Y + Dir.Y * Sg * 300.f, Rim[s].Z) - Rim[s].Z), 120.f, 10.f);
	}
}

void WRForestLook::FBuild::OtterSteps()
{
	using namespace WRForestSitesImpl;
	// DC 2.4 #14: a stair of small falls where the stream drops down mossy ledges; the otters slide the banks and leave
	// fish bones on the flat rocks. A brook trout spot. The stream runs along +X (downhill); its water ribbon is the
	// hazards track's (AWRForestWater); this is the rock it falls over. The baited eel trap is the hazards track's too.
	const int32 Steps = 5;
	for (int32 s = 0; s < Steps; ++s)
	{
		const float X = -800.f + 400.f * s;
		const float Gz = G(X, 0.f);
		// The ledge: a lip of slabs across the stream, mossed on top, darker where the water runs over.
		for (int32 b = 0; b < 6; ++b)
		{
			const float Y = -300.f + 120.f * b;
			K.Add(Basalt(1.6f + 0.2f * Rand(s * 9 + b)), FVector(90.f, 118.f, 70.f), FVector(X, Y, Gz + 10.f), FRotator(4.f * (Rand(b) - 0.5f), 3.f * b, 0.f));
			K.Add(b == 2 || b == 3 ? DeepMoss(0.7f) : Moss(1.f), FVector(70.f, 110.f, 4.f), FVector(X - 10.f, Y, Gz + 46.f));
		}
		Box(FVector(X, 0.f, Gz + 10.f), FVector(45.f, 360.f, 35.f));
		// The water over the lip: a curling sheet, white where it breaks at the foot.
		K.Add(Water(), FVector(8.f, 220.f, 70.f), FVector(X + 50.f, 0.f, Gz + 10.f), FRotator(-15.f, 0.f, 0.f));
		K.Add(FSurf::Glaze(Rgb(220, 230, 232) * 0.7f, 0.3f), FVector(60.f, 230.f, 6.f), FVector(X + 80.f, 0.f, Gz - 18.f));
		if (s % 2 == 0) Loop(TEXT("forest.spray"), FVector(X + 60.f, 0.f, Gz - 10.f), FRotator::ZeroRotator, 0.35f);
	}
	// The otter slides: smooth mud chutes worn down the banks into the pools, glossy, with paw prints at their heads.
	for (int32 o = 0; o < 3; ++o)
	{
		const float X = -650.f + 450.f * o;
		const float Sd = o % 2 ? 1.f : -1.f;
		const FVector Top(X, Sd * 650.f, G(X, Sd * 650.f) + 2.f), Foot(X + 60.f, Sd * 330.f, G(X + 60.f, Sd * 330.f) + 2.f);
		K.Add(FSurf::Glaze(Rgb(74, 58, 42) * 0.4f, 0.15f), FVector(FVector::Dist(Top, Foot), 50.f, 4.f), (Top + Foot) * 0.5f, (Foot - Top).Rotation());
		for (int32 p = 0; p < 6; ++p) K.Add(Black(0.4f), FVector(5.f, 5.f, 2.f), Top + FVector(-20.f + 8.f * p, Sd * (20.f + 12.f * (p % 2)), 1.f));
	}
	// A flat rock by the third pool, the otters' table: fish bones, a trout's head, scales glinting.
	{
		const FVector Tbl(150.f, -460.f, G(150.f, -460.f));
		Rock(2, FVector(160.f, 120.f, 50.f), Tbl, FRotator(0.f, 30.f, 0.f), FLinearColor(0.12f, 0.12f, 0.11f), true);
		for (int32 b = 0; b < 3; ++b)
		{
			const FVector C = Tbl + FVector(-30.f + 30.f * b, 10.f * b, 42.f);
			Limb(C - FVector(12.f, 0, 0), C + FVector(12.f, 0, 0), 0.8f, 0.5f, BoneC(), 4);
			for (int32 r = 0; r < 6; ++r) for (const float Sd : { -1.f, 1.f }) Limb(C + FVector(-10.f + 4.f * r, 0, 0), C + FVector(-12.f + 4.f * r, Sd * 6.f, -0.5f), 0.4f, 0.2f, BoneC(), 3);
		}
		Ball(Tbl + FVector(40.f, -20.f, 46.f), 3.5f, Rgb(140, 130, 100) * 0.5f, 0.4f, 0.7f);
		for (int32 sc = 0; sc < 8; ++sc) K.Add(FSurf::Glass(Rgb(200, 200, 190) * 0.6f), FVector(1.5f, 1.5f, 0.4f), Tbl + FVector(-40.f + 11.f * sc, 25.f * (Rand(sc) - 0.5f), 43.f));
	}
	// Ferns lean over the steps from both banks.
	for (int32 f = 0; f < 10; ++f)
	{
		const float X = -900.f + 200.f * f, Y = (f % 2 ? 1.f : -1.f) * (430.f + 80.f * Rand(f));
		Fern(*this, FVector(X, Y, G(X, Y)), 90.f + 50.f * Rand(f + 3), 36.f * f);
	}
}

void WRForestLook::FBuild::FrostedFerns()
{
	using namespace WRForestSitesImpl;
	// DC 2.4 #15: on the tundra neck the forest's ferns are rimed with frost at dawn, and a Rime-folk inuksuk stands inside
	// a Grovekin herb ring: the neighbours touched. The inuksuk faces the neck (local -X, toward the tundra); the herb
	// ring is a circle of kerb stones with herbs planted in it; the wild oats grow at (600, 0) (a server node).
	const FSurf Rime = FSurf::Stone(Rgb(232, 240, 246) * 0.8f, 0.6f);
	// The herb ring: 14 kerb stones in a 3.5 m circle, thyme and sage tufts between them, a low turf bank behind.
	for (int32 s = 0; s < 14; ++s)
	{
		const float Ang = 360.f * s / 14.f;
		const FVector P = FRotator(0.f, Ang, 0.f).Vector() * 350.f;
		const float Gz = G(P.X, P.Y);
		K.Add(OldStone(0.8f + 0.1f * Rand(s)), FVector(40.f, 26.f, 30.f), FVector(P.X, P.Y, Gz + 8.f), FRotator(0.f, Ang, 0.f));
		K.Add(Rime, FVector(32.f, 22.f, 2.f), FVector(P.X, P.Y, Gz + 24.f), FRotator(0.f, Ang, 0.f));
		const FVector H = FRotator(0.f, Ang + 12.f, 0.f).Vector() * 300.f;
		for (int32 t = 0; t < 4; ++t) K.Add(Leaf(1.2f + 0.2f * (t % 2)), FVector(4.f, 4.f, 26.f), FVector(H.X + (t - 1.5f) * 7.f, H.Y + (t % 2) * 6.f, G(H.X, H.Y) + 12.f), FRotator(10.f * (t - 1.5f), Ang, 0.f), EWRLookMesh::Cone);
	}
	// The inuksuk: a figure of stacked flat stones, legs apart, arms out, the Rime-folk's "someone was here".
	{
		const float Gz = G(0.f, 0.f);
		const FSurf Gr = FSurf::Stone(Rgb(185, 196, 206) * 0.55f, 0.9f);
		for (const float Y : { -40.f, 40.f })
		{
			K.Add(Gr, FVector(40.f, 34.f, 48.f), FVector(0.f, Y, Gz + 24.f), FRotator(0.f, 4.f * Y / 40.f, 0.f));
			K.Add(Gr, FVector(36.f, 30.f, 40.f), FVector(0.f, Y * 0.95f, Gz + 68.f), FRotator(0.f, -3.f, 0.f));
		}
		K.Add(Gr, FVector(50.f, 150.f, 22.f), FVector(0.f, 0.f, Gz + 100.f));                 // the hips' lintel
		K.Add(Gr, FVector(46.f, 70.f, 50.f), FVector(0.f, 0.f, Gz + 136.f), FRotator(0.f, 6.f, 0.f));
		K.Add(Gr, FVector(40.f, 210.f, 18.f), FVector(0.f, 0.f, Gz + 170.f), FRotator(0.f, -4.f, 0.f));   // the arms
		K.Add(Gr, FVector(36.f, 40.f, 40.f), FVector(0.f, 0.f, Gz + 200.f), FRotator(0.f, 10.f, 0.f));    // the head
		for (const float Zr : { 113.f, 181.f, 222.f }) K.Add(Rime, FVector(40.f, Zr < 150.f ? 140.f : Zr < 200.f ? 200.f : 34.f, 2.f), FVector(0.f, 0.f, Gz + Zr));
		Box(FVector(0.f, 0.f, Gz + 110.f), FVector(25.f, 75.f, 110.f));
		// The Rime-folk's offering at its foot: lichen tied with forest grass (the cache's lore line).
		K.Add(FSurf::Cloth(Rgb(150, 160, 120) * 0.45f), FVector(22.f, 22.f, 8.f), FVector(-40.f, 0.f, Gz + 4.f));
	}
	// The ferns: a bed of them round the ring, every frond white-edged with rime crystals (thin pale cards on top).
	for (int32 f = 0; f < 26; ++f)
	{
		const float Ang = f * 137.5f;
		const float Rr = 500.f + 600.f * Rand(f + 30);
		const FVector P = FRotator(0.f, Ang, 0.f).Vector() * Rr;
		const float H = 80.f + 70.f * Rand(f + 60);
		const FVector Base(P.X, P.Y, G(P.X, P.Y));
		Fern(*this, Base, H, Ang);
		for (int32 c = 0; c < 5; ++c)
		{
			const FVector Tip = Base + FRotator(0.f, Ang + 72.f * c, 0.f).Vector() * H * 0.55f + FVector(0, 0, H * 0.55f);
			K.Add(Rime, FVector(H * 0.4f, 8.f, 1.f), (Base + FVector(0, 0, H * 0.4f) + Tip) * 0.5f, FRotator(-25.f, Ang + 72.f * c, 0.f));
		}
	}
	// Hoarfrost on the grass between them: pale patches that thin out toward the warm side (+X, the forest).
	for (int32 p = 0; p < 14; ++p)
	{
		const float X = -900.f + 900.f * Rand(p + 90), Y = -900.f + 1800.f * Rand(p + 120);
		K.Add(Rime, FVector(120.f + 80.f * Rand(p), 90.f + 60.f * Rand(p + 1), 1.f), FVector(X, Y, G(X, Y) + 1.f), FRotator(0.f, 30.f * p, 0.f));
	}
}

// ================================================================================================ the old growth (F4)

void WRForestLook::FBuild::TruffleGlade()
{
	using namespace WRForestSitesImpl;
	// DC 2.4 #4 (KEPT set-piece): a glade under old oaks where the ground is rooted up (a Truffler herd feeds here), a
	// pig's skull on a stump, a basket of truffles gone black and soft that someone dropped and never came back for.
	// The truffles themselves are found by the Truffler (creature track); this is the place that says "dig here".
	for (int32 o = 0; o < 3; ++o)
	{
		const FVector Spot = FRotator(0.f, 40.f + 120.f * o, 0.f).Vector() * (1100.f + 200.f * Rand(o));
		Tree(TEXT("oak"), o, FVector(Spot.X, Spot.Y, G(Spot.X, Spot.Y)), 2800.f + 500.f * Rand(o + 4), FRotator(0.f, 70.f * o, 0.f));
	}
	// Rooted-up ground: dark overturned clods in arcs toward the oak roots, fresh earth paler on top.
	for (int32 c = 0; c < 60; ++c)
	{
		const float Ang = c * 47.f;
		const float Rr = 250.f + 750.f * Rand(c + 10);
		const FVector P = FRotator(0.f, Ang, 0.f).Vector() * Rr;
		const float Sz = 18.f + 22.f * Rand(c + 20);
		K.Add(Earth(0.6f + 0.3f * Rand(c + 30)), FVector(Sz * 1.6f, Sz, Sz * 0.6f), FVector(P.X, P.Y, G(P.X, P.Y) + Sz * 0.15f), FRotator(20.f * (Rand(c) - 0.5f), Ang, 25.f * (Rand(c + 1) - 0.5f)));
	}
	for (int32 f = 0; f < 8; ++f)
	{
		const FVector P = FRotator(0.f, f * 45.f + 10.f, 0.f).Vector() * (500.f + 200.f * Rand(f));
		K.Add(Earth(0.5f), FVector(140.f, 80.f, 3.f), FVector(P.X, P.Y, G(P.X, P.Y) + 1.f), FRotator(0.f, f * 45.f, 0.f));   // the scraped furrows
	}
	// The stump with the pig skull on it (a warning? a joke? it faces the way you came in).
	Stump(*this, FVector(-200.f, 200.f, G(-200.f, 200.f)), 45.f, 55.f, 0.f, false);
	PigSkull(*this, FVector(-200.f, 200.f, G(-200.f, 200.f) + 66.f), 200.f);
	// The basket on its side, its truffles spilled: black knobbly lumps gone soft, a fly cloud over them.
	{
		const FVector Bk(260.f, -180.f, G(260.f, -180.f) + 22.f);
		Basket(*this, Bk, 28.f, 40.f, 80.f);
		for (int32 t = 0; t < 7; ++t) Ball(FVector(Bk.X + 30.f + 14.f * t, Bk.Y - 10.f + 12.f * FMath::Sin(t * 1.7f), G(Bk.X + 30.f + 14.f * t, Bk.Y) + 4.f), 4.f + 1.5f * Rand(t), Rgb(36, 28, 22) * 0.5f, 0.9f, 0.8f);
		Loop(TEXT("forest.flies"), FVector(Bk.X + 70.f, Bk.Y, G(Bk.X + 70.f, Bk.Y) + 20.f), FRotator::ZeroRotator, 0.5f);
	}
}

void WRForestLook::FBuild::WoodcuttersCamp()
{
	using namespace WRForestSitesImpl;
	// DC 2.4 #5, 2.20 "the newcomers went too far": a logging camp at the Clear-cut's edge. The crosscut saw, a sledge
	// loaded with oak (the cache on its load at -300, 420), three tents flattened, the bunks inside them; and roots grown
	// THROUGH the tents and the bunks, the woodcutters' bones held inside the roots like something kept. Their axes are
	// still in the stumps round the camp. Nothing here is lit; nobody came back.
	// The sledge: two runners curving up at the front, a bed of cross-bearers, oak logs chained on, the tally board.
	{
		const FVector S0(-300.f, 420.f, G(-300.f, 420.f));
		const FRotator R(0.f, 15.f, 0.f);
		const auto P = [&S0, &R](float X, float Y, float Z) { return S0 + R.RotateVector(FVector(X, Y, Z)); };
		for (const float Y : { -70.f, 70.f })
		{
			K.Add(Timber(0.75f), FVector(360.f, 14.f, 18.f), P(-20.f, Y, 9.f), R);
			K.Rod(Timber(0.75f), P(160.f, Y, 9.f), P(220.f, Y, 50.f), 16.f, true);    // the runner's upturned nose
			for (int32 k = 0; k < 4; ++k) K.Rod(Timber(0.7f), P(-160.f + 100.f * k, Y, 18.f), P(-160.f + 100.f * k, Y, 42.f), 10.f, true);
		}
		for (int32 k = 0; k < 4; ++k) K.Add(Timber(0.72f), FVector(16.f, 170.f, 12.f), P(-160.f + 100.f * k, 0.f, 48.f), R);
		// The load: five oak logs, bark on, ends sawn pale, in two layers.
		const float LogR = 24.f;
		for (int32 l = 0; l < 5; ++l)
		{
			const bool bTop = l >= 3;
			const float Y = bTop ? -26.f + 52.f * (l - 3) : -52.f + 52.f * l;
			const float Z = 54.f + LogR + (bTop ? LogR * 1.75f : 0.f);
			K.Rod(Bark(1.f), P(-200.f, Y, Z), P(180.f, Y, Z), LogR * 2.f);
			K.Disc(Carved(1.f), P(-201.f, Y, Z), R.RotateVector(FVector(-1, 0, 0)), LogR * 1.8f, 2.f);
			K.Disc(Carved(1.f), P(181.f, Y, Z), R.RotateVector(FVector(1, 0, 0)), LogR * 1.8f, 2.f);
		}
		// The chain over the load, rusted, its hook still on.
		for (const float X : { -120.f, 100.f }) Line(P(X, -80.f, 50.f), P(X, 80.f, 50.f), 2.f, -120.f, Rgb(70, 40, 24) * 0.5f, 10);
		// The tally board nailed to the sledge's side: fives scored on it, hundreds of them (the lore item is a copy).
		K.Add(Carved(0.9f), FVector(70.f, 3.f, 34.f), P(-60.f, -78.f, 60.f), R);
		for (int32 m = 0; m < 12; ++m) K.Add(Black(0.6f), FVector(1.f, 1.f, 22.f), P(-90.f + 5.f * m, -80.f, 60.f), FRotator(m % 5 == 4 ? 60.f : 0.f, R.Yaw, 0.f));
		// The load's collision: the server's chest sits on it (load top ~ 150 over the ground).
		Box(P(-10.f, 0.f, 75.f), FVector(200.f, 80.f, 75.f), R);
	}
	// The crosscut saw, two-man, leaning on a stump with its handles up, rust bloom on the teeth.
	Stump(*this, FVector(300.f, 500.f, G(300.f, 500.f)), 50.f, 60.f, 30.f, false);
	{
		const FVector Sb(300.f, 500.f, G(300.f, 500.f) + 60.f);
		K.Add(Iron(), FVector(170.f, 2.f, 24.f), Sb + FVector(0.f, 55.f, -20.f), FRotator(-15.f, 0.f, 70.f));
		for (int32 t = 0; t < 14; ++t) K.Spike(Iron(), Sb + FVector(-75.f + 11.f * t, 46.f, -38.f + 3.f * t * 0.2f), FVector(0.f, -0.3f, -1.f), 3.f, 5.f);
		for (const float X : { -95.f, 95.f }) K.Rod(Timber(0.9f), Sb + FVector(X, 52.f, -14.f), Sb + FVector(X, 58.f, 30.f), 5.f);
	}
	// The three tents: A-frame canvas on ridge poles, flattened (poles snapped, canvas down over the bunks), and roots
	// come up out of the ground beside each, arch over and through the canvas and dive back in, closing round the bunks
	// and round what lies in them.
	for (int32 t = 0; t < 3; ++t)
	{
		const FVector C = FRotator(0.f, 150.f + 55.f * t, 0.f).Vector() * 650.f;
		const float Gz = G(C.X, C.Y);
		const float Yaw = 240.f + 55.f * t;
		const FRotator R(0.f, Yaw, 0.f);
		const auto P = [&C, &R, Gz](float X, float Y, float Z) { return FVector(C.X, C.Y, Gz) + R.RotateVector(FVector(X, Y, Z)); };
		// The ridge pole down on the canvas, one upright still standing, the other broken.
		K.Rod(Timber(0.8f), P(-160.f, 0.f, 0.f), P(-160.f, 0.f, 150.f), 7.f);
		K.Rod(Timber(0.8f), P(160.f, 0.f, 0.f), P(150.f, 30.f, 60.f), 7.f);
		K.Rod(Timber(0.8f), P(-160.f, 0.f, 150.f), P(160.f, 10.f, 46.f), 6.f);
		// The canvas: two slabs draped from the ridge to the ground, sagging over the bunks under them, torn where the
		// roots came through.
		for (const float Sd : { -1.f, 1.f })
		{
			K.Add(Canvas(0.75f + 0.05f * t), FVector(330.f, 150.f, 4.f), P(0.f, Sd * 70.f, 55.f), FRotator(0.f, Yaw, Sd * 22.f + 4.f * t));
			K.Add(Canvas(0.6f), FVector(90.f, 50.f, 3.f), P(-60.f + 70.f * t, Sd * 140.f, 6.f), FRotator(0.f, Yaw + 15.f, Sd * 5.f));
		}
		// The bunk under the canvas: a log frame on the ground with a rotted straw tick.
		K.Add(Timber(0.7f), FVector(200.f, 12.f, 16.f), P(0.f, -45.f, 10.f), R);
		K.Add(Timber(0.7f), FVector(200.f, 12.f, 16.f), P(0.f, 45.f, 10.f), R);
		K.Add(Straw(), FVector(190.f, 80.f, 8.f), P(0.f, 0.f, 16.f), R);
		// The sleeper: bones under what's left of a blanket; the skull turned to the side; a rib cage in the roots' hold.
		Skull(*this, P(-80.f, 4.f, 34.f), Yaw + 60.f + 30.f * t, 70.f);
		Ribs(*this, P(-20.f, 0.f, 30.f), Yaw, 1.f);
		Limb(P(30.f, -10.f, 22.f), P(90.f, -14.f, 20.f), 2.5f, 2.f, BoneC(), 5);
		Limb(P(30.f, 10.f, 22.f), P(95.f, 18.f, 20.f), 2.5f, 2.f, BoneC(), 5);
		// The roots: four big ones up through the canvas and over the body, two finer ones wound round the skull and ribs.
		for (int32 r = 0; r < 4; ++r)
		{
			const float X = -110.f + 70.f * r;
			RootArc(*this, P(X, -170.f, 0.f), P(X + 30.f, 170.f, 0.f), 70.f + 15.f * (r % 2), 14.f, 10.f, RootC() * (0.9f + 0.05f * r));
		}
		Root({ P(-120.f, -60.f, 0.f), P(-90.f, -20.f, 40.f), P(-75.f, 10.f, 46.f), P(-60.f, 30.f, 20.f), P(-50.f, 60.f, -10.f) }, 6.f, 3.f, RootC(), 6);
		Root({ P(-40.f, 60.f, 0.f), P(-25.f, 20.f, 40.f), P(-10.f, -15.f, 42.f), P(5.f, -50.f, 18.f), P(20.f, -70.f, -10.f) }, 6.f, 3.f, RootC(), 6);
		Box(P(0.f, 0.f, 40.f), FVector(170.f, 150.f, 40.f), R);
	}
	// The cold fire: a ring of stones, charred stubs, a kettle on its side.
	{
		const FVector F(100.f, -100.f, G(100.f, -100.f));
		for (int32 s = 0; s < 9; ++s) K.Add(OldStone(0.7f), FVector(28.f, 24.f, 20.f), F + FRotator(0.f, 40.f * s, 0.f).Vector() * 65.f + FVector(0, 0, 6.f), FRotator(0.f, 40.f * s, 0.f));
		K.Add(Black(0.5f), FVector(100.f, 100.f, 4.f), F + FVector(0, 0, 2.f));
		for (int32 s = 0; s < 4; ++s) K.Rod(Black(0.8f), F + FRotator(0.f, 90.f * s, 0.f).Vector() * 40.f + FVector(0, 0, 4.f), F + FRotator(0.f, 90.f * s + 20.f, 0.f).Vector() * 8.f + FVector(0, 0, 10.f), 6.f);
		M.Lathe(F + FVector(80.f, 60.f, 16.f), FVector(1.f, 0.3f, 0.f).GetSafeNormal(), 30.f, { FVector(0.f, 14.f, 14.f), FVector(0.5f, 17.f, 17.f), FVector(1.f, 12.f, 12.f) }, 10, Rgb(40, 36, 32) * 0.6f, 0.5f);
	}
	// The stumps round the camp, the newcomers' last morning's work, and the axes still in them (one in four).
	for (int32 s = 0; s < 12; ++s)
	{
		const float Ang = 360.f * s / 12.f + 13.f * Rand(s);
		const FVector P = FRotator(0.f, Ang, 0.f).Vector() * (1200.f + 500.f * Rand(s + 40));
		Stump(*this, FVector(P.X, P.Y, G(P.X, P.Y)), 45.f + 35.f * Rand(s + 5), 50.f + 50.f * Rand(s + 8), Ang + 180.f, s % 3 == 0);
	}
}

void WRForestLook::FBuild::HangingOak()
{
	using namespace WRForestSitesImpl;
	// DC 2.4 #10: a lone oak in a meadow on the Hornwarden's route. On its one big low bough an old rope, frayed, hangs
	// over nothing; above it, nailed to the trunk, a stag's antlers on their skull plate: Herne's tree (Herne hanged
	// himself from an oak and came back antlered). The Hornwarden passes here at dusk.
	const float H = 2400.f;
	const FWRForestData::FTree* Own = WRForestShape::GiantNear(Act->GetActorLocation(), 1500.f, TEXT("hanging_oak"));
	const FVector Base = Own ? Local(Own->Base) : FVector(0.f, 0.f, G(0.f, 0.f));
	const float Ht = Own ? Own->Height : H;
	const float Rr = Own ? Own->Radius : WRForestShape::TrunkRadius(TEXT("oak"), Ht);
	Tree(TEXT("oak"), 3, Base, Ht, FRotator(0.f, 20.f, 0.f), -1.f, Rr);
	// The bough: a great horizontal limb out of the trunk at 5 m toward +X, rising a little at its end.
	const FVector B0 = Base + FVector(Rr * 0.6f, 0.f, 480.f), B1 = Base + FVector(Rr + 700.f, 60.f, 560.f), B2 = Base + FVector(Rr + 1100.f, 120.f, 660.f);
	Limb(B0, B1, Rr * 0.38f, Rr * 0.26f, BarkC(), 10);
	Limb(B1, B2, Rr * 0.26f, Rr * 0.12f, BarkC(), 8);
	K.Add(Moss(1.f), FVector(500.f, 40.f, 5.f), (B0 + B1) * 0.5f + FVector(0, 0, Rr * 0.32f), (B1 - B0).Rotation());
	// The rope: knotted round the bough, one fall hanging, frayed to strands at 2.2 m (no noose: it was cut).
	const FVector Knot = FMath::Lerp(B0, B1, 0.62f) + FVector(0, 0, -Rr * 0.2f);
	{
		const FVector Ax = (B1 - B0).GetSafeNormal();
		const FVector U0 = FVector::CrossProduct(Ax, FVector::UpVector).GetSafeNormal(), V0 = FVector::CrossProduct(Ax, U0);
		const FVector Cb = FMath::Lerp(B0, B1, 0.62f);
		const float Rb = FMath::Lerp(Rr * 0.38f, Rr * 0.26f, 0.62f) + 3.f;
		for (int32 w = 0; w < 3; ++w)
		{
			TArray<FVector> Wrap;
			for (int32 s = 0; s <= 14; ++s) Wrap.Add(Cb + Ax * (w * 7.f - 7.f) + (U0 * FMath::Cos(s / 14.f * UE_TWO_PI) + V0 * FMath::Sin(s / 14.f * UE_TWO_PI)) * Rb);
			M.Tube(Wrap, 2.4f, 5, RopeC(), 0.95f);
		}
	}
	Line(Knot, Knot - FVector(-6.f, 4.f, 250.f), 2.4f, 0.f, RopeC(), 3);
	for (int32 s = 0; s < 6; ++s) Limb(Knot - FVector(-6.f, 4.f, 250.f), Knot - FVector(-6.f + 6.f * FMath::Cos(s * 1.05f), 4.f + 6.f * FMath::Sin(s * 1.05f), 280.f + 10.f * Rand(s)), 0.8f, 0.3f, RopeC(), 3);
	// The antlers nailed above, on the bough side: the frontlet bone, both beams spread; iron nails through the plate.
	{
		const FVector Plate = Base + FVector(Rr * 0.97f, 0.f, 360.f);
		K.Add(BoneS(), FVector(10.f, 24.f, 20.f), Plate + FVector(4.f, 0, 0));
		for (const float Sd : { -1.f, 1.f })
		{
			const FVector Pedicle = Plate + FVector(10.f, Sd * 9.f, 8.f);
			const FVector Out = FVector(0.35f, Sd, 0.9f);
			// A red deer's branching beam (not the elk's palm): main beam, brow, bez, trez tines, the crown.
			const FVector Mid = Pedicle + Out.GetSafeNormal() * 45.f, Top = Mid + FVector(-10.f, Sd * 25.f, 45.f);
			Limb(Pedicle, Mid, 3.5f, 3.f, BoneC(), 6);
			Limb(Mid, Top, 3.f, 2.f, BoneC(), 6);
			Limb(Pedicle + Out.GetSafeNormal() * 8.f, Pedicle + FVector(25.f, Sd * 10.f, 8.f), 2.2f, 0.7f, BoneC(), 5);
			Limb(Pedicle + Out.GetSafeNormal() * 22.f, Pedicle + FVector(28.f, Sd * 22.f, 22.f), 2.f, 0.6f, BoneC(), 5);
			Limb(Mid, Mid + FVector(22.f, Sd * 6.f, 10.f), 2.f, 0.6f, BoneC(), 5);
			for (int32 c = 0; c < 3; ++c) Limb(Top, Top + FVector(-8.f + 10.f * c, Sd * (6.f + 4.f * c), 16.f), 1.8f, 0.5f, BoneC(), 5);
		}
		for (int32 n = 0; n < 3; ++n) K.Add(Iron(), FVector(4.f, 3.f, 3.f), Plate + FVector(10.f, -8.f + 8.f * n, -4.f + 4.f * (n % 2)));
	}
	// Under the bough the grass is worn in a ring where the Hornwarden's herd stops: trodden earth, a scatter of dung.
	for (int32 p = 0; p < 10; ++p)
	{
		const FVector P = Base + FVector(Rr + 500.f, 60.f, 0.f) + FRotator(0.f, 36.f * p, 0.f).Vector() * 350.f;
		K.Add(Earth(0.8f), FVector(150.f, 80.f, 3.f), FVector(P.X, P.Y, G(P.X, P.Y) + 1.f), FRotator(0.f, 36.f * p + 90.f, 0.f));
	}
}

void WRForestLook::FBuild::NurseLog()
{
	using namespace WRForestSitesImpl;
	// DC 2.4 #17: the Nurse Log's clearing. Its 90 m trunk and the young trees growing out of its back are the log's own
	// (forest.json "fallen" with nurse: drawn by FallenLog). Here: the light gap the fall tore in the canopy (a bright
	// floor of ferns and foxgloves), the root plate's crater, and the amber heartwood at its broken end: the richest
	// heartwood in F4 (its node is the server's, 12 hits).
	const FWRForestData& D = FWRForestData::Get();
	const FWRForestData::FLog* Best = nullptr;
	float BestD = 15000.f;
	for (const FWRForestData::FLog& L : D.Fallen)
	{
		if (!L.bNurse) continue;
		const float Dd = FVector::Dist2D((L.A + L.B) * 0.5f, Act->GetActorLocation());
		if (Dd < BestD) { BestD = Dd; Best = &L; }
	}
	const FVector A0 = Best ? Local(Best->A) : FVector(-4500.f, 0.f, G(-4500.f, 0.f));
	const FVector B0 = Best ? Local(Best->B) : FVector(4500.f, 0.f, G(4500.f, 0.f));
	const float Rad = Best ? Best->Radius : 200.f;
	const FVector Dir = (B0 - A0).GetSafeNormal();
	const FVector Side = FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal();
	// The light gap's floor: ferns thick either side of the log, foxgloves standing in the sun.
	for (int32 f = 0; f < 40; ++f)
	{
		const float U = Rand(f + 3);
		const float Sd = f % 2 ? 1.f : -1.f;
		const FVector P = FMath::Lerp(A0, B0, U) + Side * Sd * (Rad + 150.f + 900.f * Rand(f + 50));
		Fern(*this, FVector(P.X, P.Y, G(P.X, P.Y)), 80.f + 70.f * Rand(f + 9), 50.f * f);
		if (f % 4 == 0)
		{
			const FVector Fg(P.X + 60.f, P.Y + 40.f, G(P.X + 60.f, P.Y + 40.f));
			K.Rod(Leaf(1.1f), Fg, Fg + FVector(0, 0, 140.f), 2.f);
			for (int32 b = 0; b < 8; ++b) K.Add(FSurf::Cloth(Rgb(176, 90, 150) * 0.5f), FVector(6.f, 6.f, 9.f), Fg + FVector(4.f * ((b % 2) * 2 - 1), 0.f, 70.f + 8.f * b), FRotator(160.f, 45.f * b, 0.f), EWRLookMesh::Cone);
		}
	}
	// The root plate's crater: the hole the roots tore out of the ground, a pool of dark water in it, soil hanging off the
	// plate's underside in clods.
	{
		const FVector Cr = A0 - Dir * (Rad * 1.6f);
		const float Gz = G(Cr.X, Cr.Y);
		K.Add(Water(), FVector(Rad * 2.4f, Rad * 3.f, 3.f), FVector(Cr.X, Cr.Y, Gz + 3.f), FRotator(0.f, Dir.Rotation().Yaw, 0.f));
		for (int32 c = 0; c < 10; ++c) K.Add(Earth(0.6f + 0.2f * Rand(c)), FVector(70.f, 60.f, 40.f), FVector(Cr.X, Cr.Y, Gz) + Side * (Rad * 1.5f) * (Rand(c + 4) * 2.f - 1.f) + Dir * 60.f * Rand(c + 8) + FVector(0, 0, 20.f + 150.f * Rand(c + 12)), FRotator(20.f * c, 33.f * c, 0.f));
	}
	// The broken end: the amber heartwood core showing in the split, the growth rings round it, splinters standing out;
	// at night the sap glows faintly (the amber the Grovekin called the forest's blood).
	{
		const FVector End = B0 + FVector(0, 0, Rad * 0.15f);
		K.Disc(Carved(0.85f), End + Dir * 4.f, Dir, Rad * 1.75f, 6.f);
		K.Disc(Carved(0.7f), End + Dir * 7.f, Dir, Rad * 1.3f, 3.f);
		K.Disc(Amber(0.25f), End + Dir * 9.f, Dir, Rad * 0.85f, 4.f);
		K.Disc(Amber(0.4f), End + Dir * 11.f, Dir, Rad * 0.4f, 3.f);
		N.Add(Amber(1.2f), FVector(4.f, Rad * 0.6f, Rad * 0.6f), End + Dir * 13.f, Dir.Rotation(), EWRLookMesh::Cylinder);
		for (int32 s = 0; s < 9; ++s)
		{
			const float Ang = s * 40.f;
			const FVector Off = (Side * FMath::Cos(FMath::DegreesToRadians(Ang)) + FVector::UpVector * FMath::Sin(FMath::DegreesToRadians(Ang))) * Rad * (0.5f + 0.4f * Rand(s));
			K.Spike(Carved(0.8f), End + Off, Dir + Off.GetSafeNormal() * 0.15f, 14.f, 40.f + 60.f * Rand(s + 3));
		}
		// Sap weeping down the bark below the split.
		for (int32 w = 0; w < 4; ++w) K.Add(Amber(0.15f), FVector(5.f, 8.f, 60.f + 30.f * w), End - Dir * 20.f + Side * (Rad * (w - 1.5f) * 0.4f) - FVector(0, 0, Rad * 0.5f), FRotator(0.f, Dir.Rotation().Yaw, 0.f));
		Lamp(End + Dir * 60.f, FLinearColor(1.f, 0.6f, 0.2f), 600.f, 600.f);
	}
}

void WRForestLook::FBuild::BearsLarder()
{
	using namespace WRForestSitesImpl;
	// DC 2.4 #18 (bears cache their kills): an old oak whose buttress roots make a hollow at its foot facing +X. The hollow
	// is full of bones; a Burlbeast's carcass lies half buried under a quilt of moss the Mossmaul raked over it; claw
	// gouges score the bark 4 m up (where it stood to mark). A Mossmaul fight (its spawn is the creature track's); the
	// hide and bone are the cache (150, 80).
	const float OakH = 3600.f;
	const FVector Trunk(-450.f, 0.f, G(-450.f, 0.f));
	const float Rr = FMath::Max(140.f, WRForestShape::TrunkRadius(TEXT("oak"), OakH));
	Tree(TEXT("oak"), 1, Trunk, OakH, FRotator(0.f, 0.f, 0.f), -1.f, Rr);
	// The two buttress roots that wall the hollow, flaring from the bole out along +X.
	for (const float Sd : { -1.f, 1.f })
	{
		Root({ Trunk + FVector(Rr * 0.6f, Sd * Rr * 0.5f, 260.f), Trunk + FVector(Rr + 120.f, Sd * (Rr * 0.7f + 40.f), 110.f), Trunk + FVector(Rr + 330.f, Sd * (Rr * 0.8f + 90.f), -20.f) }, 70.f, 30.f, BarkC(), 10);
		SegBox(Trunk + FVector(Rr * 0.6f, Sd * Rr * 0.6f, 120.f), Trunk + FVector(Rr + 300.f, Sd * (Rr * 0.8f + 80.f), 40.f), 45.f, 60.f);
	}
	// The hollow floor: bones heaped (long bones, a ribcage, a skull of a deer), dark earth scraped.
	const FVector Hollow = Trunk + FVector(Rr + 120.f, 0.f, 0.f);
	K.Add(Earth(0.5f), FVector(300.f, Rr * 1.2f, 4.f), FVector(Hollow.X, Hollow.Y, G(Hollow.X, Hollow.Y) + 2.f));
	for (int32 b = 0; b < 16; ++b)
	{
		const FVector P = Hollow + FVector(-100.f + 200.f * Rand(b), -Rr * 0.4f + Rr * 0.8f * Rand(b + 20), 0.f);
		const FVector Q = P + FRotator(0.f, 60.f * b, 0.f).Vector() * (30.f + 40.f * Rand(b + 7));
		Limb(FVector(P.X, P.Y, G(P.X, P.Y) + 5.f + 3.f * (b % 3)), FVector(Q.X, Q.Y, G(Q.X, Q.Y) + 5.f), 3.f, 2.5f, BoneC() * (0.8f + 0.2f * Rand(b)), 5);
		if (b % 4 == 0) Ball(FVector(P.X, P.Y, G(P.X, P.Y) + 6.f), 4.f, BoneC(), 0.75f);
	}
	Ribs(*this, Hollow + FVector(40.f, 30.f, G(Hollow.X, Hollow.Y) - Hollow.Z + 25.f), 40.f, 2.f);
	Antler(Hollow + FVector(-60.f, -50.f, G(Hollow.X - 60.f, Hollow.Y - 50.f) - Hollow.Z + 10.f), FVector(0.4f, -1.f, -0.2f), 90.f, false, BoneC());
	// The carcass outside the hollow (+X, -Y side): a Burlbeast's bulk, hide split, the haunch and ribs bare, and over
	// most of it a raked quilt of moss and leaf litter.
	{
		const FVector Cc(250.f, -260.f, G(250.f, -260.f));
		M.Lathe(Cc + FVector(-160.f, 0.f, 50.f), FVector(1.f, 0.15f, 0.f).GetSafeNormal(), 320.f, { FVector(0.f, 40.f, 30.f), FVector(0.3f, 85.f, 60.f), FVector(0.7f, 80.f, 58.f), FVector(1.f, 35.f, 30.f) }, 12, Rgb(92, 66, 46) * 0.45f, 0.95f);
		Ribs(*this, Cc + FVector(60.f, -50.f, 70.f), 10.f, 3.2f);
		K.Add(Blood(), FVector(80.f, 50.f, 3.f), Cc + FVector(60.f, -60.f, 6.f));
		for (int32 q = 0; q < 9; ++q) K.Add(Moss(0.75f + 0.2f * Rand(q)), FVector(110.f, 90.f, 22.f), Cc + FVector(-180.f + 50.f * q, 30.f * FMath::Sin(q * 1.4f), 70.f + 20.f * FMath::Sin(q * 0.8f)), FRotator(10.f * (Rand(q) - 0.5f), 25.f * q, 15.f * (Rand(q + 3) - 0.5f)));
		for (int32 q = 0; q < 6; ++q) K.Add(Earth(0.55f), FVector(70.f, 50.f, 14.f), Cc + FVector(-200.f + 80.f * q, 90.f, 10.f), FRotator(0.f, 40.f * q, 8.f));
		Box(Cc + FVector(0.f, 0.f, 50.f), FVector(170.f, 80.f, 50.f), FRotator(0.f, 8.f, 0.f));
		Loop(TEXT("forest.flies"), Cc + FVector(60.f, -50.f, 110.f), FRotator::ZeroRotator, 0.7f);
	}
	// The claw gouges on the bark, 3.8-4.4 m up on the clearing side: two sets of four parallel grooves, raw pale wood.
	for (int32 s = 0; s < 2; ++s)
	{
		const float Ang = -30.f + 50.f * s;
		const FVector Face = FRotator(0.f, Ang, 0.f).Vector();
		for (int32 c = 0; c < 4; ++c)
		{
			const FVector P = Trunk + Face * (Rr * 0.98f) + FRotator(0.f, Ang + 90.f, 0.f).Vector() * (c * 9.f - 13.f) + FVector(0, 0, 410.f - 10.f * s);
			K.Add(Carved(1.15f), FVector(3.f, 3.5f, 70.f), P, FRotator(-8.f, Ang, 12.f));
		}
	}
}

void WRForestLook::FBuild::WaspRavine()
{
	using namespace WRForestSitesImpl;
	// DC 2.4 #19: a ravine whose rock overhangs carry eight paper nests (the hazards track hangs them; here are the
	// ledges they hang from). The ravine runs along local X; its two walls stand at Y = +-WallOff, each a row of
	// overhanging rock shelves at 3-7 m with dead wood and roots trailing off them.
	const float WallOff = WRTune(TEXT("forestWorld.waspRavineHalfWidth"), 700.f);
	const FLinearColor RockTint(0.13f, 0.14f, 0.12f);
	for (const float Sd : { -1.f, 1.f })
	{
		for (int32 r = 0; r < 6; ++r)
		{
			const float X = -1500.f + 600.f * r + 150.f * Rand(r + int32(Sd * 7.f));
			const float Y = Sd * WallOff;
			const float Gz = G(X, Y);
			// The wall's foot: a big boulder; on it a shelf jutting into the ravine (the overhang); a smaller cap.
			Rock(r, FVector(500.f, 380.f, 420.f), FVector(X, Y + Sd * 80.f, Gz), FRotator(0.f, 30.f * r, 0.f), RockTint, true, 60.f);
			const float ShelfZ = Gz + 330.f + 200.f * Rand(r * 3 + int32(Sd * 3.f));
			Rock(r + 2, FVector(520.f, 340.f, 130.f), FVector(X, Y - Sd * 60.f, ShelfZ), FRotator(Sd * 6.f, 20.f * r, 0.f), RockTint * 1.1f, true, 0.f);
			// Moss on the shelf top; roots and dead branches trailing off its lip into the ravine.
			K.Add(DeepMoss(0.9f), FVector(380.f, 240.f, 6.f), FVector(X, Y - Sd * 60.f, ShelfZ + 128.f));
			for (int32 t = 0; t < 4; ++t)
			{
				const FVector Lip(X - 150.f + 100.f * t, Y - Sd * 220.f, ShelfZ + 20.f);
				Root({ Lip, Lip + FVector(10.f, -Sd * 30.f, -90.f), Lip + FVector(-10.f, -Sd * 40.f, -180.f - 60.f * Rand(t + r)) }, 4.f, 1.5f, RootC(), 5);
			}
		}
	}
	// The ravine floor: stones washed down, a dead branch across it, wasps' dead under the nests (dark specks).
	for (int32 s = 0; s < 18; ++s)
	{
		const float X = -1500.f + 3000.f * Rand(s + 200), Y = -WallOff * 0.6f + WallOff * 1.2f * Rand(s + 230);
		K.Add(OldStone(0.6f + 0.2f * Rand(s)), FVector(40.f + 40.f * Rand(s + 1), 30.f + 30.f * Rand(s + 2), 20.f), FVector(X, Y, G(X, Y) + 6.f), FRotator(0.f, 40.f * s, 0.f));
	}
	Log(TEXT("fallen"), 1, FVector(-600.f, -WallOff * 0.9f, G(-600.f, -WallOff * 0.9f) + 30.f), FVector(200.f, WallOff * 0.8f, G(200.f, WallOff * 0.8f) + 60.f), 35.f);
	for (int32 w = 0; w < 30; ++w)
	{
		const float X = -1200.f + 2400.f * Rand(w + 300), Y = (w % 2 ? 1.f : -1.f) * WallOff * (0.4f + 0.3f * Rand(w + 330));
		K.Add(Black(0.5f), FVector(3.f, 1.5f, 1.f), FVector(X, Y, G(X, Y) + 1.f), FRotator(0.f, 60.f * w, 0.f));
	}
}

void WRForestLook::FBuild::CoilHollow()
{
	using namespace WRForestSitesImpl;
	// DC 2.4 #20: a tangle of exposed roots like a nest of logs; one of the "logs" is a Rootcoil (creature track). Shed
	// skins 12 m long hang in the roots like washing (the pay: Rootcoil shed, the cache). The roots belong to an old oak
	// on the hollow's -X side whose bank washed out; they run out across the ground, over each other, as thick as a man.
	const float OakH = 3200.f;
	const FVector Trunk(-900.f, 0.f, G(-900.f, 0.f) - 120.f);
	Tree(TEXT("oak"), 2, Trunk, OakH, FRotator(0.f, 150.f, 0.f));
	struct FRootPath { TArray<FVector> Pts; float R0; };
	TArray<FRootPath> Paths;
	for (int32 r = 0; r < 11; ++r)
	{
		const float Ang = -70.f + 14.f * r;
		const FVector Out = FRotator(0.f, Ang, 0.f).Vector();
		const float Len = 900.f + 600.f * Rand(r + 11);
		TArray<FVector> Pts;
		for (int32 i = 0; i <= 6; ++i)
		{
			const float U = i / 6.f;
			const FVector P = Trunk + FVector(0, 0, 120.f) + Out * Len * U + FRotator(0.f, Ang + 90.f, 0.f).Vector() * 120.f * FMath::Sin(U * 4.f + r);
			const float Gz = G(P.X, P.Y);
			// Each root lifts clear of the ground over another and sinks between: a tangle you climb over.
			const float Lift = (i == 0) ? 120.f : 25.f + 70.f * FMath::Max(0.f, FMath::Sin(U * 6.f + r * 1.7f));
			Pts.Add(FVector(P.X, P.Y, Gz + Lift - (i == 6 ? 40.f : 0.f)));
		}
		const float R0 = 55.f - 2.5f * FMath::Abs(r - 5);
		Root(Pts, R0, 14.f, RootC() * (0.85f + 0.08f * (r % 3)), 9);
		Paths.Add({ Pts, R0 });
		for (int32 i = 0; i + 1 < 5; ++i) SegBox(Pts[i], Pts[i + 1], R0 * 0.7f * (1.f - i * 0.12f), R0 * 0.7f * (1.f - i * 0.12f));
		if (r % 3 == 0) K.Add(Moss(1.f), FVector(200.f, R0 * 1.2f, 6.f), (Pts[1] + Pts[2]) * 0.5f + FVector(0, 0, R0 * 0.9f), (Pts[2] - Pts[1]).Rotation());
	}
	// The shed skins: two of them, 12 m and 9 m, pale and papery with the scale pattern, looped over the roots and
	// hanging down between them, one torn in two.
	for (int32 s = 0; s < 2; ++s)
	{
		const FRootPath& R1 = Paths[2 + 5 * s];
		const FRootPath& R2 = Paths[3 + 5 * s];
		TArray<FVector> Skin;
		const int32 Count = s == 0 ? 12 : 9;
		for (int32 i = 0; i < Count; ++i)
		{
			const float U = float(i) / (Count - 1);
			const FVector Over = FMath::Lerp(R1.Pts[1], R1.Pts[5], U) + FVector(0, 0, R1.R0 * 0.8f);
			const FVector Under = FMath::Lerp(R2.Pts[1], R2.Pts[5], U);
			const FVector Mid = (Over + Under) * 0.5f;
			Skin.Add(i % 2 == 0 ? Over : FVector(Mid.X, Mid.Y, FMath::Max(G(Mid.X, Mid.Y) + 30.f, Mid.Z - 80.f)));
		}
		M.Tube(Skin, 22.f - 4.f * s, 8, Rgb(198, 194, 168) * 0.6f, 0.6f);
		// The scale rows: thin darker bands round it every half metre.
		for (int32 i = 0; i + 1 < Skin.Num(); ++i)
		{
			const FVector Dd = Skin[i + 1] - Skin[i];
			for (int32 b = 1; b < 4; ++b) K.Disc(FSurf::Stone(Rgb(150, 146, 120) * 0.5f, 0.7f), Skin[i] + Dd * (b / 4.f), Dd, (22.f - 4.f * s) * 2.06f, 1.f);
		}
	}
	// The hollow's floor between the roots: dark leaf litter, a sloughed patch of a third skin under it.
	for (int32 p = 0; p < 10; ++p)
	{
		const FVector P = Trunk + FRotator(0.f, -60.f + 12.f * p, 0.f).Vector() * (500.f + 300.f * Rand(p));
		K.Add(Earth(0.55f), FVector(180.f, 140.f, 3.f), FVector(P.X, P.Y, G(P.X, P.Y) + 1.f), FRotator(0.f, 30.f * p, 0.f));
	}
}

void WRForestLook::FBuild::OfferingStones()
{
	using namespace WRForestSitesImpl;
	// DC 2.4 #21, 2.20 "they left bread on stones for the forest": a trail tree bent at the knee (at the origin) points at a
	// flat stone (at 240, 0) with a carved bread loaf on it. Coins and bone beads are pressed into the moss round it.
	// Leave food on the loaf once a game day for +50 Growth (UWRForestWorld::Offer, reach from (240, 0, 50)); the cache
	// (copper, the beads) lies beside the stone at (240, 60).
	Tree(TEXT("trail_tree"), 1, FVector(0.f, 0.f, G(0.f, 0.f)), 900.f, FRotator::ZeroRotator);
	const float Gz = G(240.f, 0.f);
	// The stone: a flat slab on two packing stones, its top ~50 cm, worn smooth where hands rested.
	for (const float Y : { -25.f, 25.f }) K.Add(OldStone(0.7f), FVector(60.f, 30.f, 34.f), FVector(240.f, Y, Gz + 15.f), FRotator(0.f, 5.f, 0.f));
	K.Add(OldStone(0.95f), FVector(150.f, 96.f, 16.f), FVector(240.f, 0.f, Gz + 40.f), FRotator(0.f, 4.f, 1.5f));
	Box(FVector(240.f, 0.f, Gz + 24.f), FVector(75.f, 48.f, 24.f), FRotator(0.f, 4.f, 0.f));
	// The loaf: a round cob carved in the slab's own stone, its crust scored with the sun cross.
	Ball(FVector(240.f, 0.f, Gz + 48.f), 22.f, Rgb(150, 142, 128) * 0.55f, 0.85f, 0.45f);
	for (int32 s = 0; s < 2; ++s) K.Add(OldStone(0.4f), FVector(32.f, 1.5f, 1.5f), FVector(240.f, 0.f, Gz + 68.f), FRotator(0.f, 45.f + 90.f * s, 0.f));
	// Coins and beads in the moss round the stone (the newcomers learnt the custom too: copper among the bone).
	K.Add(Moss(1.1f), FVector(280.f, 220.f, 3.f), FVector(240.f, 0.f, Gz + 1.f));
	for (int32 c = 0; c < 18; ++c)
	{
		const FVector P = FVector(240.f, 0.f, 0.f) + FRotator(0.f, c * 61.f, 0.f).Vector() * (90.f + 40.f * Rand(c));
		if (c % 3 == 0) Ball(FVector(P.X, P.Y, G(P.X, P.Y) + 3.f), 1.6f, BoneC(), 0.6f);
		else K.Disc(FSurf::Steel(Rgb(140, 90, 50) * 0.55f, 0.45f), FVector(P.X, P.Y, G(P.X, P.Y) + 3.f), FVector(0.1f * (c % 3), 0.f, 1.f), 3.f, 0.4f);
	}
	// A crust of real bread, recent: someone still leaves it.
	K.Add(FSurf::Cloth(Rgb(180, 130, 70) * 0.5f), FVector(10.f, 6.f, 4.f), FVector(225.f, 15.f, Gz + 50.f), FRotator(0.f, 30.f, 0.f));
}
