// The Ancient forest's kit POIs, fallen giants, canopy walkways and the five shrines' footings (cooks/biome-forest.md
// 2.4 kit table, 2.7, 2.11, 2.20; cooks/build-biome-forest.md 2.5, 2.6). The generator places about 97 kit rows; each
// kind here has its variants, sits on the ground it lands on, and says why it is there. Shared pieces and the palette:
// WRForestSceneryKit.h. Dispatch: WRForestScenery.cpp.

#include "WRForestSceneryKit.h"

namespace WRForestKitImpl
{
	using WRForestLook::FBuild;
	using WRLookKit::FSurf;

	/** This actor's row in a forest.json list: by id, else the nearest within 5 m. */
	const FWRForestData::FSite* RowOf(const AWRForestScenery* S, const TArray<FWRForestData::FSite>& List)
	{
		const FWRForestData::FSite* Best = nullptr;
		float BestD = 500.f;
		for (const FWRForestData::FSite& R : List)
		{
			if (!S->SiteId.IsNone() && R.Id == S->SiteId) return &R;
			const float Dd = FVector::Dist2D(R.At, S->GetActorLocation());
			if (Dd < BestD) { BestD = Dd; Best = &R; }
		}
		return Best;
	}

	/** The water surface near a point (forest.json rivers carry their surface Z) within Range, and the flow direction. */
	bool RiverSurface(const FVector& WorldAt, float Range, float& OutZ, FVector& OutAlong)
	{
		const FWRForestData& D = FWRForestData::Get();
		float Best = Range;
		bool bFound = false;
		for (const FWRForestData::FPath& P : D.Rivers)
		{
			for (int32 i = 0; i + 1 < P.Points.Num(); ++i)
			{
				const FVector A0 = P.Points[i], A1 = P.Points[i + 1];
				const FVector2D C = FMath::ClosestPointOnSegment2D(FVector2D(WorldAt), FVector2D(A0), FVector2D(A1));
				const float Dd = float(FVector2D::Distance(C, FVector2D(WorldAt)));
				if (Dd >= Best) continue;
				Best = Dd;
				const float Seg = FMath::Max(1.f, float(FVector2D::Distance(FVector2D(A0), FVector2D(A1))));
				OutZ = FMath::Lerp(float(A0.Z), float(A1.Z), float(FVector2D::Distance(FVector2D(A0), C)) / Seg);
				OutAlong = (A1 - A0).GetSafeNormal2D();
				bFound = true;
			}
		}
		return bFound;
	}

	/** A turned body (an animal's barrel, a snake's length, a jar) along Dir from From: radii Ry across, Rz up. */
	void Body(FBuild& B, const FVector& From, const FVector& Dir, float Len, float Ry, float Rz, const FLinearColor& C, float Rough = 0.9f)
	{
		B.M.Lathe(From, Dir.GetSafeNormal(), Len, { FVector(0.f, Ry * 0.25f, Rz * 0.25f), FVector(0.12f, Ry * 0.8f, Rz * 0.8f), FVector(0.45f, Ry, Rz),
			FVector(0.8f, Ry * 0.85f, Rz * 0.8f), FVector(1.f, Ry * 0.3f, Rz * 0.3f) }, 10, C, Rough);
	}

	/** A leg from the hip to the hoof (a knee bend; the hoof darker). */
	void Leg(FBuild& B, const FVector& Hip, const FVector& Hoof, float R, const FLinearColor& C, float KneeOut)
	{
		const FVector Knee = FMath::Lerp(Hip, Hoof, 0.5f) + FVector(KneeOut, 0.f, 0.f);
		B.Limb(Hip, Knee, R, R * 0.7f, C, 6);
		B.Limb(Knee, Hoof, R * 0.7f, R * 0.5f, C, 6);
		B.Ball(Hoof, R * 0.6f, C * 0.4f, 0.7f, 0.7f);
	}

	/** Ribs bared on a carcass's flank: arcs from the spine down, Count along the body. */
	void Ribs(FBuild& B, const FVector& Spine0, const FVector& Spine1, float Drop, float Out, int32 Count, const FLinearColor& C)
	{
		const FVector Side = FVector::CrossProduct((Spine1 - Spine0).GetSafeNormal(), FVector::UpVector).GetSafeNormal();
		for (int32 i = 0; i < Count; ++i)
		{
			const FVector Sp = FMath::Lerp(Spine0, Spine1, (i + 0.5f) / Count);
			B.Root({ Sp, Sp + Side * Out * 0.8f - FVector(0, 0, Drop * 0.2f), Sp + Side * Out - FVector(0, 0, Drop * 0.7f), Sp + Side * Out * 0.6f - FVector(0, 0, Drop) }, 3.f, 1.6f, C, 6);
		}
	}

	/** One rough field stone in a wall course, toned stone to stone. */
	void Stone(FBuild& B, const FVector& Centre, const FVector& Size, float Yaw, int32 Salt)
	{
		const float Tone = 0.75f + 0.3f * B.Rand(Salt);
		B.K.Add(WRForestLook::OldStone(Tone), Size, Centre, FRotator(2.f * (B.Rand(Salt + 3) - 0.5f), Yaw + 4.f * (B.Rand(Salt + 5) - 0.5f), 3.f * (B.Rand(Salt + 7) - 0.5f)));
	}

	/** A coursed field-stone wall from P0 to P1 (local, Z ignored: it sits on the ground along its run), Height tall (a
	 *  ragged top when ruined), with collision. The newcomers' walls: dry courses of grey field stone, copes on edge. */
	void StoneWall(FBuild& B, const FVector& P0, const FVector& P1, float Height, float Thick, bool bRuin, int32 Salt)
	{
		const FVector D = (P1 - P0) * FVector(1, 1, 0);
		const float Len = D.Size();
		if (Len < 20.f) return;
		const FVector Dir = D / Len;
		const float Yaw = Dir.Rotation().Yaw;
		float MinTop = Height, GroundSum = 0.f;
		int32 Samples = 0;
		for (float X = 0.f; X < Len; )
		{
			const float StoneL = FMath::Min(50.f + 60.f * B.Rand(Salt + int32(X)), Len - X);
			const float Here = bRuin ? Height * (0.3f + 0.7f * FMath::Abs(FMath::Sin(X * 0.004f + Salt))) : Height;
			MinTop = FMath::Min(MinTop, Here);
			const FVector Foot = P0 + Dir * (X + StoneL * 0.5f);
			const float Gz = B.G(float(Foot.X), float(Foot.Y), float(P0.Z));
			GroundSum += Gz;
			++Samples;
			int32 Course = 0;
			for (float Z = 0.f; Z < Here; Z += 38.f, ++Course)
			{
				const float Off = (Course % 2) ? StoneL * 0.25f : -StoneL * 0.1f;
				Stone(B, FVector(Foot.X, Foot.Y, Gz + Z + 15.f) + Dir * Off, FVector(StoneL * 0.95f, Thick * (0.88f + 0.12f * B.Rand(Course + Salt)), 36.f), Yaw, Salt + Course * 31 + int32(X));
			}
			if (!bRuin) B.K.Add(WRForestLook::OldStone(0.7f), FVector(StoneL * 0.9f, 28.f, Thick * 0.55f), FVector(Foot.X, Foot.Y, Gz + Here + 14.f), FRotator(0.f, Yaw, 90.f));
			X += StoneL;
		}
		const float Gm = Samples > 0 ? GroundSum / Samples : float(P0.Z);
		B.SegBox(FVector(P0.X, P0.Y, Gm + MinTop * 0.5f), FVector(P1.X, P1.Y, Gm + MinTop * 0.5f), Thick * 0.5f, MinTop * 0.5f);
	}

	/** A world transform for a kit mesh grown to Height at a local base (for WRTreeKit::AddCollision). */
	FTransform TreeWorld(const FBuild& B, FName Species, const FVector& LocalBase, float Height, float Yaw)
	{
		const float Sc = Height / FMath::Max(1.f, WRTreeKit::ReferenceHeight(Species));
		return FTransform(FRotator(0.f, Yaw, 0.f), LocalBase, FVector(Sc)) * B.T;
	}

	/** A canopy giant of our own where the generator put none: drawn and given its kit collision. Returns its radius. */
	float OwnGiant(FBuild& B, const FVector& LocalBase, float Height, int32 Var)
	{
		B.Tree(TEXT("giant"), Var, LocalBase, Height, FRotator::ZeroRotator, 0.f);
		WRTreeKit::AddCollision(B.Act, TEXT("giant"), Var, TreeWorld(B, TEXT("giant"), LocalBase, Height, 0.f));
		return WRForestShape::TrunkRadius(TEXT("giant"), Height);
	}

	/**
	 * A fallen giant from its root plate P0 to its broken end P1 (local), girth R: it rests on the ground along its length
	 * (lifted where a rise would push through it, propped on its own broken limbs over hollows), the pit its root plate
	 * tore out behind it, moss down its back, the amber heartwood core bared at the broken end (the heartwood node
	 * stands there: DC 2.7 "fallen giants only"), a resin weep where it split. The Nurse Log's saplings straddle it with
	 * roots like legs.
	 */
	void DrawLog(FBuild& B, FVector P0, FVector P1, float R, bool bNurse, int32 Var)
	{
		const FVector Dir = (P1 - P0).GetSafeNormal2D();
		const FVector Side = FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal();
		const float Len = FVector::Dist2D(P0, P1);
		if (Len < 100.f) return;
		P0.Z = B.G(float(P0.X), float(P0.Y), float(P0.Z)) + R * 0.7f;
		P1.Z = B.G(float(P1.X), float(P1.Y), float(P1.Z)) + R * 0.8f;
		// Rest it: no sample of ground may rise into the trunk.
		const int32 Samples = 9;
		TArray<float> Gz;
		float Lift = 0.f;
		for (int32 i = 0; i <= Samples; ++i)
		{
			const FVector P = FMath::Lerp(P0, P1, float(i) / Samples);
			const float G0 = B.G(float(P.X), float(P.Y), float(P.Z - R));
			Gz.Add(G0);
			Lift = FMath::Max(Lift, G0 + R * 0.75f - float(P.Z));
		}
		P0.Z += Lift;
		P1.Z += Lift;
		B.Log(bNurse ? FName(TEXT("nurse_log")) : FName(TEXT("fallen")), Var, P0, P1, R, true);
		const FLinearColor Bark = WRForestLook::BarkC();
		for (int32 i = 1; i < Samples; ++i)
		{
			const FVector P = FMath::Lerp(P0, P1, float(i) / Samples);
			const float Under = float(P.Z) - R - Gz[i];
			if (Under > R * 0.5f)
			{
				// Over a hollow: a broken limb it came down on props it (the log never floats).
				const float S = (i % 2) ? 1.f : -1.f;
				const FVector Top = P + Side * S * R * 0.6f - FVector(0, 0, R * 0.3f);
				const FVector Foot(P.X + Side.X * S * (R + Under * 0.6f), P.Y + Side.Y * S * (R + Under * 0.6f), Gz[i] - 15.f);
				B.Limb(Top, Foot, R * 0.22f, R * 0.14f, Bark, 8);
				B.SegBox(Top, Foot, R * 0.12f, R * 0.12f);
			}
			// Snapped branch stubs along the top: the silhouette of a tree that came down whole.
			if (i % 3 == 1)
			{
				const FVector Up = P + FVector(0, 0, R * 0.8f);
				B.Limb(Up, Up + (FVector::UpVector * 0.8f + Side * ((i % 2) ? 0.5f : -0.5f) + Dir * 0.2f).GetSafeNormal() * R * (1.4f + B.Rand(i)), R * 0.2f, R * 0.1f, Bark, 7);
			}
			// Moss down its back, thickest on the north-facing upper side: slabs laid on the curve.
			B.K.Add(WRForestLook::Moss(0.8f + 0.4f * B.Rand(i + 40)), FVector(Len / Samples * 0.9f, R * 0.9f, 6.f), P + FVector(0, 0, R * 0.92f) + Side * R * 0.15f, FRotator(0.f, Dir.Rotation().Yaw, 8.f));
		}
		// The pit the root plate tore out of the ground: a dark hollow behind the plate with stones in its lip.
		const FVector Pit = P0 - Dir * R * 1.2f;
		const float PitZ = B.G(float(Pit.X), float(Pit.Y), float(P0.Z - R));
		B.K.Add(WRForestLook::Earth(0.55f), FVector(R * 2.2f, R * 3.2f, 24.f), FVector(Pit.X, Pit.Y, PitZ - 8.f), FRotator(0.f, Dir.Rotation().Yaw, 0.f));
		for (int32 s = 0; s < 3; ++s)
		{
			const FVector Rk = Pit + Side * R * (-1.2f + 1.2f * s) - Dir * R * 0.6f * B.Rand(s + 60);
			B.Rock(s, FVector(70.f, 60.f, 45.f) * (0.7f + 0.6f * B.Rand(s + 61)), FVector(Rk.X, Rk.Y, B.G(float(Rk.X), float(Rk.Y), PitZ)), FRotator(0.f, 70.f * s, 0.f), FLinearColor(0.12f, 0.11f, 0.1f), false);
		}
		// Hanging soil and torn root ends on the plate's face.
		for (int32 r = 0; r < 7; ++r)
		{
			const float Ang = 360.f * r / 7.f;
			const FVector Out = Side * FMath::Cos(FMath::DegreesToRadians(Ang)) + FVector(0, 0, FMath::Sin(FMath::DegreesToRadians(Ang)));
			const FVector From = P0 + Out * R * 1.6f - Dir * 30.f;
			B.Limb(From, From - Dir * (R * 0.6f) + Out * R * 0.5f - FVector(0, 0, R * 0.3f), R * 0.12f, R * 0.03f, WRForestLook::RootC(), 6);
		}
		// The broken end: splintered rim, the amber core bared (heartwood is amber-cored: the signature resource's look).
		const FVector End = P1 + Dir * 4.f;
		B.K.Disc(WRForestLook::Amber(0.22f), End, Dir, R * 1.1f, 8.f);
		B.K.Disc(WRForestLook::Bark(0.6f), End - Dir * 2.f, Dir, R * 1.7f, 6.f);
		for (int32 s = 0; s < 10; ++s)
		{
			const float Ang = 36.f * s + 11.f * B.Rand(s + 80);
			const FVector Rim = Side * FMath::Cos(FMath::DegreesToRadians(Ang)) + FVector(0, 0, FMath::Sin(FMath::DegreesToRadians(Ang)));
			B.K.Spike(WRForestLook::Carved(0.7f), End + Rim * R * 0.78f, (Dir + Rim * 0.15f).GetSafeNormal(), R * 0.18f, R * (0.4f + 0.7f * B.Rand(s + 81)));
		}
		for (int32 c = 0; c < 4; ++c)
		{
			// Amber cracks running back from the core (night: they hold the last light).
			const FVector Crack = End - Dir * (60.f + 80.f * c) + FVector(0, 0, R * 0.95f) + Side * R * 0.2f * (c - 1.5f);
			B.K.Add(WRForestLook::Amber(0.12f), FVector(140.f, 6.f, 5.f), Crack, FRotator(0.f, Dir.Rotation().Yaw + 12.f * (c - 1.5f), 0.f));
		}
		// A resin weep where it split, halfway along on its side: amber drips down the bark to a beaded pool.
		const FVector Weep = FMath::Lerp(P0, P1, 0.55f) + Side * R * 0.98f;
		for (int32 d = 0; d < 5; ++d)
		{
			B.K.Add(WRForestLook::Amber(0.08f), FVector(10.f, 4.f, 30.f + 25.f * B.Rand(d + 90)), Weep + Dir * (d - 2) * 14.f - FVector(0, 0, 10.f * d), FRotator(0.f, Dir.Rotation().Yaw, 0.f));
		}
		B.K.Add(WRForestLook::Amber(0.06f), FVector(40.f, 26.f, 3.f), FVector(Weep.X + Side.X * 20.f, Weep.Y + Side.Y * 20.f, B.G(float(Weep.X), float(Weep.Y), float(Weep.Z - R)) + 2.f), FRotator(0.f, Dir.Rotation().Yaw, 0.f));
		if (bNurse)
		{
			// The young trees on its back grip it with roots down both flanks into the soil, like legs straddling it.
			for (int32 k = 0; k < 6; ++k)
			{
				const FVector Topk = FMath::Lerp(P0, P1, 0.15f + 0.13f * k) + FVector(0, 0, R * 0.9f);
				for (const float S : { -1.f, 1.f })
				{
					const FVector Mid = Topk + Side * S * R * 1.05f - FVector(0, 0, R * 0.6f);
					const FVector Foot(Topk.X + Side.X * S * R * 1.5f, Topk.Y + Side.Y * S * R * 1.5f, B.G(float(Topk.X + Side.X * S * R * 1.5f), float(Topk.Y + Side.Y * S * R * 1.5f), float(Topk.Z - R * 2.f)) - 20.f);
					B.Root({ Topk, Topk + Side * S * R * 0.7f, Mid, Foot }, 16.f, 7.f, WRForestLook::RootC(), 7);
				}
			}
		}
	}

	/** A Grovekin foxfire jar hung on a hook: cold green light that never goes out (DC 2.19 foxfire lantern's origin). */
	void FoxfireJar(FBuild& B, const FVector& Hook)
	{
		B.Line(Hook, Hook - FVector(0, 0, 40.f), 1.2f, 0.f, WRForestLook::RopeC(), 2);
		B.K.Add(WRForestLook::Clay(), FVector(26.f, 26.f, 8.f), Hook - FVector(0, 0, 44.f));
		B.K.Add(FSurf::Glass(WRForestLook::Rgb(150, 190, 140) * 0.3f), FVector(22.f, 22.f, 30.f), Hook - FVector(0, 0, 63.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		B.N.Add(WRForestLook::Foxfire(1.2f), FVector(14.f, 14.f, 20.f), Hook - FVector(0, 0, 63.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		B.Lamp(Hook - FVector(0, 0, 63.f), WRForestLook::Rgb(127, 224, 122), WRTune(TEXT("forestWorld.foxfireJarLight"), 900.f), 700.f);
	}
}

// ================================================================================================ the kit

void WRForestLook::FBuild::TrailTree()
{
	// A trail tree (world-map SPEC waymark): a sapling the Grovekin bent at knee height and tied down until it grew that
	// way, its knee pointing along the road (+X). They stand 120-200 m apart on every Grovekin road; and they all bend
	// AWAY from the Four Hearts (DC 2.20: a warning). Follow the knees, not the foxfire (the Lost Path, F-S1).
	const int32 Var = Act->Variant % 3;
	const float Height = 650.f + 150.f * Var + 100.f * Rand(1);
	const float Sc = Height / FMath::Max(1.f, WRTreeKit::ReferenceHeight(TEXT("trail_tree")));
	const FVector Base = At(0.f, 0.f);
	Tree(TEXT("trail_tree"), Var, Base, Height, FRotator::ZeroRotator, 60.f);
	FVector Knee = WRTreeKit::Anchor(TEXT("trail_tree"), Var, TEXT("points")) * Sc;
	if (Knee.IsNearlyZero()) Knee = FVector(140.f, 0.f, 45.f) * Sc;
	// The bend you could sit on: collision along the knee.
	SegBox(Base + FVector(0, 0, 45.f), Base + FVector(Knee.X, Knee.Y, 45.f), 10.f, 10.f);
	// The old tie-down: a peg in the ground at the knee's end and the rotten withy still round the trunk.
	const FVector Peg = At(float(Knee.X) + 25.f, float(Knee.Y));
	K.Rod(Timber(0.7f), Peg - FVector(0, 0, 15.f), Peg + FVector(0, 0, 35.f), 6.f);
	Line(Peg + FVector(0, 0, 30.f), Base + FVector(Knee.X * 0.9f, Knee.Y * 0.9f, 48.f), 1.5f, 6.f, RopeC(), 4);
	// A blaze carved where the bend turns up: the Grovekin's road mark, a rune like a little tree.
	K.Add(Carved(1.1f), FVector(18.f, 3.f, 26.f), Base + FVector(Knee.X * 0.55f, -9.f, 58.f), FRotator(0.f, 0.f, 0.f));
	// Moss on the bend's back, a ring of field stones round the foot.
	K.Add(Moss(1.1f), FVector(Knee.Size2D() * 0.8f, 14.f, 4.f), Base + FVector(Knee.X * 0.45f, Knee.Y * 0.45f, 58.f), FRotator(0.f, FVector(Knee.X, Knee.Y, 0.f).Rotation().Yaw, 0.f));
	for (int32 s = 0; s < 6 + Var; ++s)
	{
		const FVector Dir = FRotator(0.f, 60.f * s + 20.f * Rand(s), 0.f).Vector();
		WRForestKitImpl::Stone(*this, At(float(Dir.X) * 90.f, float(Dir.Y) * 90.f, 8.f), FVector(28.f, 22.f, 16.f), 60.f * s, s);
	}
	if (Var == 1)
	{
		// A bread stone at its foot (DC 2.20: the pact: bread left for the forest), the loaf carved in it.
		const FVector Bs = At(-80.f, 70.f);
		K.Add(OldStone(0.9f), FVector(60.f, 50.f, 18.f), Bs + FVector(0, 0, 9.f), FRotator(0.f, 20.f, 0.f));
		Ball(Bs + FVector(0, 0, 26.f), 13.f, PaleWoodC() * 0.9f, 0.9f, 0.6f);
	}
	else if (Var == 2)
	{
		// Strips of bark cloth tied on the knee by travellers: the road is still walked.
		for (int32 r = 0; r < 3; ++r) K.Add(Canvas(0.8f + 0.2f * r), FVector(4.f, 6.f, 40.f), Base + FVector(Knee.X * (0.5f + 0.15f * r), Knee.Y * 0.5f, 30.f), FRotator(0.f, 0.f, 6.f * r));
	}
}

void WRForestLook::FBuild::FallenGiant()
{
	// A fallen giant the generator placed as a kit POI with no "fallen" row of its own: 40 m of trunk along the site's
	// yaw, root plate at the origin, broken end at +X, where the heartwood node is (UWRForestWorld). Fallen giants are
	// the only heartwood (DC 2.7): every one is a reason to stop.
	const float Len = WRTune(TEXT("forestWorld.kitLogLength"), 4000.f);
	const float R = 170.f + 40.f * (Act->Variant % 3);
	WRForestKitImpl::DrawLog(*this, FVector(0.f, 0.f, 0.f), FVector(Len, 0.f, 0.f), R, false, Act->Variant % 4);
}

void WRForestLook::FBuild::FallenLog(bool bNurse)
{
	// A fallen giant from forest.json "fallen" (Extra = root plate, Extra2 = broken amber end, Size = girth).
	const float R = Act->Size > 10.f ? Act->Size : 150.f;
	WRForestKitImpl::DrawLog(*this, Local(Act->Extra), Local(Act->Extra2), R, bNurse, WRTreeKit::VariantAt(Act->Extra));
}

void WRForestLook::FBuild::HedgerowGap()
{
	// A gap in an F1 hedgerow (newcomer fields): the hedge runs across the way (local Y) on an earth bank, a gap with a
	// stile where the path crosses (+X is the way through), and a berry patch at the gap where the walkers pick.
	const int32 Var = Act->Variant % 3;
	const float Half = 1500.f;
	for (const float S : { -1.f, 1.f })
	{
		// The bank: turf over earth, 1 m wide, a ditch on the field side.
		for (float Y = 160.f; Y < Half; Y += 300.f)
		{
			const FVector P = At(0.f, S * (Y + 150.f));
			K.Add(Turf(), FVector(140.f, 310.f, 60.f), P + FVector(0, 0, 15.f), FRotator(0.f, 0.f, 0.f));
			K.Add(Earth(0.6f), FVector(60.f, 310.f, 20.f), P + FVector(-110.f, 0, -6.f));
		}
		// Hazel and holly laid along the bank, with an oak standard every so often.
		for (int32 i = 0; i < 8; ++i)
		{
			const float Y = S * (220.f + 165.f * i);
			const FName Sp = (i + Var) % 3 == 0 ? FName(TEXT("holly")) : FName(TEXT("hazel"));
			Tree(Sp, (i + Var) % 4, At(10.f * Rand(i), Y, 25.f), 300.f + 120.f * Rand(i + 10), FRotator(0.f, 70.f * i, 0.f), 0.f);
		}
		Box(FVector(0.f, S * (160.f + Half) * 0.5f, G(0.f, S * Half * 0.5f) + 120.f), FVector(70.f, (Half - 160.f) * 0.5f, 120.f));
	}
	Tree(TEXT("oak"), Var, At(-120.f, (Var == 1 ? -1.f : 1.f) * 1150.f), 1500.f + 300.f * Rand(3), FRotator(0.f, 30.f * Var, 0.f));
	// The stile: the three kinds the field country uses.
	const FVector O = At(0.f, 0.f);
	if (Var == 0)
	{
		// A step stile: two posts, a top rail, a footboard each side to step over on.
		for (const float Y : { -90.f, 90.f }) K.Rod(Timber(), O + FVector(0, Y, -30.f), O + FVector(0, Y, 120.f), 14.f, true);
		K.Add(Timber(0.9f), FVector(12.f, 200.f, 12.f), O + FVector(0, 0, 100.f));
		K.Add(Timber(0.9f), FVector(12.f, 200.f, 12.f), O + FVector(0, 0, 60.f));
		for (const float X : { -45.f, 45.f })
		{
			K.Add(Plank(0.9f), FVector(28.f, 120.f, 6.f), O + FVector(X, 0, 38.f));
			K.Rod(Timber(0.8f), O + FVector(X, -50.f, -10.f), O + FVector(X, -50.f, 36.f), 8.f);
			K.Rod(Timber(0.8f), O + FVector(X, 50.f, -10.f), O + FVector(X, 50.f, 36.f), 8.f);
			Box(O + FVector(X, 0, 36.f), FVector(14.f, 60.f, 4.f));
		}
		Box(O + FVector(0, 0, 80.f), FVector(6.f, 100.f, 30.f));
	}
	else if (Var == 1)
	{
		// A field gate on hinged posts, swung half open (left that way: the path's walkers don't shut it).
		for (const float Y : { -150.f, 150.f }) K.Rod(Timber(), O + FVector(0, Y, -40.f), O + FVector(0, Y, 140.f), 18.f, true);
		const FRotator Swing(0.f, 55.f, 0.f);
		const FVector Hinge = O + FVector(0, -150.f, 0);
		for (int32 r = 0; r < 4; ++r) K.Add(Timber(0.85f), FVector(8.f, 290.f, 10.f), Hinge + Swing.RotateVector(FVector(0, 145.f, 25.f + 30.f * r)), Swing);
		K.Add(Timber(0.8f), FVector(8.f, 320.f, 10.f), Hinge + Swing.RotateVector(FVector(0, 145.f, 70.f)), FRotator(Swing.Pitch, Swing.Yaw, 22.f));
		K.Add(Iron(), FVector(10.f, 10.f, 14.f), Hinge + FVector(0, 0, 40.f));
		K.Add(Iron(), FVector(10.f, 10.f, 14.f), Hinge + FVector(0, 0, 110.f));
	}
	else
	{
		// A squeeze stile: two standing slabs a body-width apart (sheep can't pass, people can).
		for (const float Y : { -55.f, 55.f })
		{
			K.Add(OldStone(0.85f), FVector(30.f, 70.f, 130.f), O + FVector(0, Y + (Y > 0 ? 35.f : -35.f), 50.f), FRotator(0.f, 0.f, Y > 0 ? 4.f : -4.f));
			Box(O + FVector(0, Y + (Y > 0 ? 35.f : -35.f), 55.f), FVector(15.f, 35.f, 65.f));
		}
	}
	// The berry patch at the gap: brambles in fruit, a worn patch of trampled ground where people pick.
	for (int32 b = 0; b < 3; ++b)
	{
		const FVector Bp = At(180.f + 90.f * b, (b % 2 ? 1.f : -1.f) * (240.f + 60.f * b));
		Tree(TEXT("bramble"), b, Bp, 140.f + 40.f * Rand(b + 20), FRotator(0.f, 90.f * b, 0.f), 0.f);
		for (int32 r = 0; r < 9; ++r)
		{
			const FVector Berry = Bp + FRotator(0.f, 40.f * r, 0.f).Vector() * (40.f + 20.f * Rand(r + b * 9)) + FVector(0, 0, 50.f + 60.f * Rand(r + 30));
			Ball(Berry, 2.4f, (r % 3 == 0) ? Rgb(150, 30, 40) * 0.5f : Rgb(30, 14, 30) * 0.6f, 0.4f);
		}
	}
	K.Add(Earth(0.75f), FVector(500.f, 160.f, 4.f), O + FVector(0, 0, 1.f));
}

void WRForestLook::FBuild::BeehiveTree()
{
	// A beehive tree (DC 2.4 kit, F1/F3): an old oak or lime with a wild Hiveback colony in a hollow on its +X face, a
	// metre and a half up. The honey node lies at the foot under the hollow, comb that fell out (UWRForestWorld, local
	// (110, 0)). Rob it in smoke (a torch held 3 s or a smoke pot) or its Hivebacks come out: Rowan's "Smoke and honey".
	const int32 Var = Act->Variant % 2;
	const FName Sp = Var == 0 ? FName(TEXT("oak")) : FName(TEXT("lime"));
	const float Height = 1800.f + 300.f * Rand(1);
	const float R = FMath::Max(70.f, WRForestShape::TrunkRadius(Sp, Height));
	// The trunk stands back so its face is just behind the hollow and the comb at the foot is clear of it.
	const FVector Base = At(-R - 20.f, 0.f);
	Tree(Sp, Var + 1, Base, Height, FRotator::ZeroRotator);
	const FVector Face = Base + FVector(R * 0.95f, 0.f, 140.f);
	// The hollow: a dark oval in the bark, its lips rolled callus, comb plates showing inside, honey run down the bark.
	K.Add(Black(0.5f), FVector(10.f, 46.f, 78.f), Face, FRotator::ZeroRotator, EWRLookMesh::Cylinder);
	for (int32 i = 0; i < 10; ++i)
	{
		const float Ang = 36.f * i;
		const FVector Rim(0.f, FMath::Cos(FMath::DegreesToRadians(Ang)) * 30.f, FMath::Sin(FMath::DegreesToRadians(Ang)) * 46.f);
		K.Add(Bark(1.1f), FVector(16.f, 18.f, 18.f), Face + Rim + FVector(4.f, 0, 0), FRotator(0.f, 0.f, Ang));
	}
	for (int32 c = 0; c < 4; ++c) K.Add(FSurf::Stone(Rgb(214, 170, 82) * 0.55f, 0.6f), FVector(30.f, 3.f, 50.f - 8.f * c), Face + FVector(-8.f, -18.f + 12.f * c, -4.f), FRotator(0.f, 90.f, 0.f));
	for (int32 d = 0; d < 3; ++d) K.Add(Amber(0.05f), FVector(4.f, 7.f, 40.f + 20.f * d), Face + FVector(6.f, -10.f + 10.f * d, -50.f - 12.f * d));
	Loop(TEXT("forest.bees"), Face + FVector(40.f, 0, 0));
	// The fallen comb at the foot, and the beekeepers' old tally: a notched peg driven in the bark (someone robbed it yearly).
	K.Add(FSurf::Stone(Rgb(214, 170, 82) * 0.5f, 0.6f), FVector(40.f, 28.f, 8.f), At(110.f, 20.f, 4.f), FRotator(0.f, 30.f, 12.f));
	K.Rod(Timber(0.7f), Base + FVector(R * 0.7f, R * 0.6f, 90.f), Base + FVector(R * 1.2f, R * 0.8f, 92.f), 5.f);
	if (Var == 1)
	{
		// A lime: a second, smaller swarm cluster hanging off a branch (the colony is strong: it swarmed).
		Ball(Base + FVector(R + 160.f, -120.f, 420.f), 22.f, Rgb(70, 52, 20) * 0.5f, 0.9f, 1.5f);
		Loop(TEXT("forest.bees"), Base + FVector(R + 160.f, -120.f, 400.f), FRotator::ZeroRotator, 0.6f);
	}
	// Ferns and nettles at the root flare where nothing grazes it.
	for (int32 f = 0; f < 4; ++f) Tree(TEXT("fern"), f, At(float(Base.X) + FMath::Cos(f * 1.7f) * (R + 140.f), FMath::Sin(f * 1.7f) * (R + 140.f)), 90.f + 30.f * Rand(f + 5), FRotator(0.f, 90.f * f, 0.f), 0.f);
}

void WRForestLook::FBuild::HouseTree()
{
	// A house-tree (DC 2.4 kit, F3/F5): a Grovekin home grown inside a living trunk. The Grovekin never cut a house
	// (DC 2.20): they trained a ring of saplings to fuse into one hollow bole round a room, and its crown still grows
	// from the top. Variants: 0 empty (sealed jars on the shelf grown out of the back wall: the server's cache sits on
	// it, local (-120, 0)), 1 looted (door torn off, jars smashed), 2 a Truffler den (rooted floor, a burrow under the wall).
	const int32 Var = Act->Variant % 3;
	const float Ro = 270.f, WallH = 900.f;
	const float DoorHalf = 24.f;
	const FVector O = At(0.f, 0.f);
	const FLinearColor BarkCol = Var == 2 ? BarkC() * 0.85f : BeechC() * 0.8f;
	// The fused columns: fourteen once-saplings, flared at the foot, leaning in as they rise, bark filling between.
	const int32 Cols = 14;
	for (int32 i = 0; i < Cols; ++i)
	{
		const float Ang = 360.f * i / Cols;
		if (FMath::Abs(FRotator::NormalizeAxis(Ang)) < DoorHalf - 4.f) continue;
		const FVector Dir = FRotator(0.f, Ang, 0.f).Vector();
		const FVector Foot = FVector(Dir.X * (Ro + 70.f), Dir.Y * (Ro + 70.f), G(float(Dir.X) * (Ro + 70.f), float(Dir.Y) * (Ro + 70.f), float(O.Z)) - 20.f);
		Root({ Foot, O + Dir * (Ro + 10.f) + FVector(0, 0, 120.f), O + Dir * Ro * 0.92f + FVector(0, 0, 450.f), O + Dir * Ro * 0.62f + FVector(0, 0, WallH) }, 52.f, 34.f, BarkCol, 8);
	}
	for (int32 i = 0; i < Cols; ++i)
	{
		const float Ang = 360.f * (i + 0.5f) / Cols;
		if (FMath::Abs(FRotator::NormalizeAxis(Ang)) < DoorHalf) continue;
		const FRotator Face(0.f, Ang, 0.f);
		const float Chord = 2.f * Ro * FMath::Sin(PI / Cols);
		for (int32 z = 0; z < 3; ++z)
		{
			const float Zc = 150.f + 300.f * z;
			const float Rz = FMath::Lerp(Ro, Ro * 0.62f, Zc / WallH);
			K.Add(BeechBark(Var == 2 ? 0.75f : 0.95f), FVector(28.f, Chord * (Rz / Ro) * 1.05f, 310.f), O + Face.Vector() * Rz + FVector(0, 0, Zc), FRotator(-8.f * z, Ang, 0.f));
		}
		if (FMath::Abs(FRotator::NormalizeAxis(Ang)) > DoorHalf + 6.f) Box(O + Face.Vector() * Ro + FVector(0, 0, 170.f), FVector(16.f, Chord * 0.5f, 170.f), Face);
	}
	// The crown: the bole closes over the room into the living trunk and a lime grows on from it, 12 m up.
	M.Lathe(O + FVector(0, 0, WallH - 60.f), FVector::UpVector, 420.f, { FVector(0.f, Ro * 0.66f, Ro * 0.66f), FVector(0.5f, Ro * 0.45f, Ro * 0.45f), FVector(1.f, Ro * 0.28f, Ro * 0.28f) }, 12, BarkCol, 0.9f, 0.f, 4.f);
	Tree(TEXT("lime"), Var, O + FVector(0, 0, WallH + 300.f), 1700.f + 300.f * Rand(2), FRotator(0.f, 40.f * Var, 0.f), 0.f);
	// The door: a carved lintel on two posts, a hulder post at its right (DC 2.20: at every gate).
	const FVector Door = O + FVector(Ro, 0.f, 0.f);
	for (const float S : { -1.f, 1.f }) K.Rod(Carved(), Door + FVector(0, S * 70.f, 0.f), Door + FVector(0, S * 70.f, 215.f), 16.f, true);
	K.Add(Carved(0.9f), FVector(30.f, 170.f, 22.f), Door + FVector(0, 0, 222.f));
	Hulder(At(Ro + 90.f, 150.f), 0.f, 0.95f);
	if (Var == 0) K.Add(Bark(0.9f), FVector(6.f, 120.f, 195.f), Door + FVector(45.f, -95.f, 98.f), FRotator(0.f, 65.f, 0.f));          // door leaf standing open
	else K.Add(Bark(0.75f), FVector(120.f, 195.f, 6.f), At(Ro + 160.f, 40.f, 3.f), FRotator(0.f, 15.f, 4.f));                              // torn off, in the moss
	// Inside: a floor of packed earth, a hearth ring in the middle, a bracken bed, the shelf grown from the back wall.
	K.Add(Earth(Var == 2 ? 0.7f : 0.9f), FVector(Ro * 1.8f, Ro * 1.8f, 6.f), O + FVector(0, 0, 2.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
	for (int32 s = 0; s < 8; ++s)
	{
		const FVector Dir = FRotator(0.f, 45.f * s, 0.f).Vector();
		WRForestKitImpl::Stone(*this, O + Dir * 45.f + FVector(0, 0, 8.f), FVector(22.f, 18.f, 14.f), 45.f * s, s + 3);
	}
	K.Add(Black(0.7f), FVector(60.f, 60.f, 3.f), O + FVector(0, 0, 5.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
	K.Add(Thatch(), FVector(200.f, 90.f, 18.f), O + FVector(-30.f, 150.f, 10.f), FRotator(0.f, 75.f, 0.f));
	// The shelf: a ledge of living wood grown out of the back wall at hip height (the cache on variant 0 rests on it).
	const FVector Shelf = O + FVector(-140.f, 0.f, 85.f);
	Root({ O + FVector(-Ro + 10.f, -110.f, 40.f), Shelf + FVector(10.f, -90.f, -6.f), Shelf + FVector(10.f, 90.f, -6.f), O + FVector(-Ro + 10.f, 110.f, 40.f) }, 14.f, 14.f, BarkCol, 7);
	K.Add(BeechBark(1.1f), FVector(120.f, 220.f, 10.f), Shelf, FRotator::ZeroRotator);
	Box(Shelf, FVector(60.f, 110.f, 6.f));
	if (Var == 0)
	{
		// Sealed jars on the shelf's ends (the middle is the cache's): wax-stoppered, as the Grovekin left them.
		for (const float Y : { -85.f, 85.f })
		{
			WRForestKitImpl::Body(*this, Shelf + FVector(0, Y, 6.f), FVector::UpVector, 36.f, 13.f, 13.f, Rgb(150, 96, 62) * 0.5f, 0.8f);
			K.Disc(FSurf::Stone(Rgb(214, 180, 100) * 0.55f, 0.5f), Shelf + FVector(0, Y, 42.f), FVector::UpVector, 12.f, 4.f);
		}
	}
	else if (Var == 1)
	{
		// Looted: the jars smashed on the floor, sherds and a dark stain where honey ran; slashes on the wall.
		for (int32 s = 0; s < 9; ++s) K.Add(Clay(), FVector(10.f + 8.f * Rand(s), 8.f, 3.f), O + FVector(-90.f + 20.f * Rand(s + 9), -100.f + 25.f * s, 6.f), FRotator(0.f, 40.f * s, 20.f * Rand(s)));
		K.Add(Amber(0.03f), FVector(80.f, 50.f, 2.f), O + FVector(-110.f, 30.f, 5.f), FRotator(0.f, 30.f, 0.f));
		for (int32 c = 0; c < 3; ++c) K.Add(Carved(1.2f), FVector(4.f, 3.f, 60.f), O + FVector(-Ro * 0.9f + 20.f, 40.f + 12.f * c, 160.f), FRotator(0.f, 0.f, 30.f));
	}
	else
	{
		// A Truffler den: the floor rooted into furrows, a bed of dead leaves, a burrow dug out under the back wall,
		// truffle peel and a sow's droppings. Your Truffler will find truffles; this one lives here.
		for (int32 f = 0; f < 6; ++f) K.Add(Earth(0.55f + 0.1f * (f % 2)), FVector(160.f, 26.f, 14.f), O + FVector(-60.f + 30.f * f, -40.f + 15.f * f, 6.f), FRotator(0.f, 20.f * f, 0.f));
		K.Add(Leaf(0.6f), FVector(170.f, 130.f, 24.f), O + FVector(40.f, -120.f, 12.f), FRotator(0.f, 20.f, 0.f), EWRLookMesh::Cylinder);
		K.Add(Black(0.4f), FVector(12.f, 90.f, 70.f), O + FVector(-Ro + 30.f, -60.f, 20.f), FRotator(0.f, 0.f, 0.f), EWRLookMesh::Cylinder);
		K.Add(Earth(0.8f), FVector(110.f, 100.f, 36.f), O + FVector(-Ro + 110.f, -60.f, 10.f), FRotator(0.f, 0.f, 0.f), EWRLookMesh::Cylinder);
	}
}

void WRForestLook::FBuild::KillScene()
{
	// The food chain set-pieces (DC 2.4 kit, 2.12.3 living-world beats): what a predator killed and left here. The killer
	// itself is the creature track's (a living-world beat spawns it); here is only the body and the ground's record:
	// 0 a Mossmaul's cache: a Burlbeast half eaten under a quilt of pulled moss (bears cache kills), claw gouges on a trunk;
	// 1 a Thornlynx kill: a Dappledoe dragged under a trunk, throat torn, drag marks back to where the lynx dropped on it;
	// 2 a Rootcoil that choked swallowing a Truffler: the snake dead, its jaws round the pig's hindquarters.
	const int32 Var = Act->Variant % 3;
	const FLinearColor BloodC = Rgb(70, 12, 10) * 0.5f;
	if (Var == 0)
	{
		// The Burlbeast: a barrel 2.2 m long, low; legs out stiff; the near flank opened to the ribs; tusks.
		const FVector B0 = At(-110.f, 0.f, 45.f), B1 = At(110.f, 0.f, 50.f);
		WRForestKitImpl::Body(*this, B0, B1 - B0, 230.f, 70.f, 58.f, Rgb(88, 66, 44) * 0.45f);
		WRForestKitImpl::Ribs(*this, B0 + FVector(40.f, 0, 50.f), B1 + FVector(-50.f, 0, 50.f), 70.f, 55.f, 8, BoneC());
		K.Add(WRLookKit::FSurf::Glaze(BloodC, 0.3f), FVector(150.f, 60.f, 30.f), (B0 + B1) * 0.5f + FVector(0, 50.f, 15.f), FRotator(0.f, 0.f, -30.f));
		for (int32 l = 0; l < 4; ++l)
		{
			const FVector Hip = FMath::Lerp(B0, B1, l < 2 ? 0.2f : 0.85f) + FVector(0, l % 2 ? 30.f : -30.f, -10.f);
			WRForestKitImpl::Leg(*this, Hip, Hip + FVector(l < 2 ? -40.f : 40.f, l % 2 ? 90.f : -40.f, -30.f), 13.f, Rgb(88, 66, 44) * 0.45f, 10.f);
		}
		Ball(B1 + FVector(55.f, 0, -5.f), 40.f, Rgb(80, 60, 40) * 0.45f, 0.85f, 0.8f);
		for (const float S : { -1.f, 1.f }) K.Spike(BoneS(), B1 + FVector(80.f, S * 20.f, -10.f), FVector(0.6f, S * 0.3f, 0.6f), 9.f, 38.f);
		// The moss quilt the Mossmaul raked over it, torn up in strips from the ground round about (bare earth scars).
		for (int32 q = 0; q < 7; ++q) K.Add(Moss(0.9f + 0.1f * (q % 3)), FVector(90.f, 70.f, 10.f), FMath::Lerp(B0, B1, q / 6.f) + FVector(-20.f, -40.f + 10.f * q, 40.f), FRotator(10.f * (q % 2), 25.f * q, 20.f));
		for (int32 s = 0; s < 5; ++s) K.Add(Earth(0.6f), FVector(120.f, 40.f, 3.f), At(-300.f + 150.f * s, -250.f - 40.f * (s % 2), 1.f), FRotator(0.f, 70.f + 15.f * s, 0.f));
		// Claw gouges 4 m up the nearest trunk side: a tree trunk of our own at the edge (the bear marks its larder).
		const FVector Tk = At(-60.f, 420.f);
		Tree(TEXT("oak"), 2, Tk, 2200.f, FRotator(0.f, 20.f, 0.f));
		const float Tr = WRForestShape::TrunkRadius(TEXT("oak"), 2200.f);
		for (int32 c = 0; c < 4; ++c) K.Add(Carved(1.15f), FVector(5.f, 4.f, 70.f), Tk + FVector(8.f * c - 12.f, -Tr * 0.98f, 360.f + 6.f * c), FRotator(0.f, 0.f, 8.f));
		Loop(TEXT("forest.flies"), (B0 + B1) * 0.5f + FVector(0, 0, 80.f));
	}
	else if (Var == 1)
	{
		// The Dappledoe: a slender deer on its side, neck back, throat torn, dappled coat (pale spots in the hide).
		const FVector B0 = At(-70.f, 0.f, 32.f), B1 = At(70.f, 0.f, 34.f);
		const FLinearColor Coat = Rgb(150, 104, 64) * 0.5f;
		WRForestKitImpl::Body(*this, B0, B1 - B0, 140.f, 32.f, 36.f, Coat);
		for (int32 s = 0; s < 9; ++s) Ball(FMath::Lerp(B0, B1, 0.1f + 0.1f * s) + FVector(0, -12.f + 6.f * (s % 3), 26.f), 4.f, Rgb(214, 196, 160) * 0.55f, 0.9f, 0.4f);
		const FVector Neck = B1 + FVector(20.f, 0, 10.f), Head = Neck + FVector(10.f, 55.f, 10.f);
		Limb(Neck, Head, 13.f, 9.f, Coat, 8);
		WRForestKitImpl::Body(*this, Head, FVector(0.4f, 1.f, -0.1f), 34.f, 9.f, 10.f, Coat);
		K.Add(WRLookKit::FSurf::Glaze(BloodC, 0.3f), FVector(40.f, 26.f, 8.f), Neck + FVector(10.f, 20.f, -6.f));
		for (int32 l = 0; l < 4; ++l)
		{
			const FVector Hip = FMath::Lerp(B0, B1, l < 2 ? 0.15f : 0.85f) + FVector(0, -10.f, -12.f);
			WRForestKitImpl::Leg(*this, Hip, Hip + FVector(l % 2 ? 30.f : -20.f, -95.f, -10.f), 6.f, Coat, -5.f);
		}
		// Drag marks back to the trunk the lynx dropped from (a perch 6 m up; scratches where it landed), a blood trail.
		for (int32 d = 0; d < 6; ++d)
		{
			K.Add(Earth(0.55f), FVector(70.f, 14.f, 3.f), At(-160.f - 70.f * d, -20.f + (d % 2) * 30.f, 1.f), FRotator(0.f, 5.f * d, 0.f));
			if (d % 2 == 0) K.Add(WRLookKit::FSurf::Glaze(BloodC, 0.4f), FVector(14.f, 10.f, 2.f), At(-180.f - 70.f * d, 10.f, 2.f));
		}
		const FVector Perch = At(-620.f, -60.f);
		Tree(TEXT("beech"), 1, Perch, 2400.f, FRotator(0.f, 60.f, 0.f));
		K.Add(Leaf(0.8f), FVector(160.f, 120.f, 4.f), At(-560.f, 0.f, 2.f), FRotator(0.f, 10.f, 0.f));   // leaves shed where it dropped
		Loop(TEXT("forest.flies"), (B0 + B1) * 0.5f + FVector(0, 0, 60.f), FRotator::ZeroRotator, 0.6f);
	}
	else
	{
		// The Rootcoil: 9 m of root-barked snake in loose bends across the ground, dead; its jaws unhinged round the
		// Truffler's hindquarters, the pig's back legs and curly tail sticking out. Greed killed it (a story for the eye).
		const FLinearColor Coil = Rgb(90, 74, 58) * 0.5f;
		TArray<FVector> Pts;
		for (int32 i = 0; i <= 10; ++i)
		{
			const float X = -420.f + 84.f * i;
			const float Y = 110.f * FMath::Sin(i * 0.9f);
			Pts.Add(At(X, Y, 22.f));
		}
		Root(Pts, 16.f, 26.f, Coil, 10);
		const FVector Jaw = Pts.Last() + FVector(30.f, 0, 4.f);
		WRForestKitImpl::Body(*this, Jaw - FVector(10.f, 0, 0), FVector::ForwardVector, 90.f, 34.f, 32.f, Coil);          // the stretched head round the prey
		// Moss blotches on its back (it lies on logs and passes for one: DC 2.12).
		for (int32 m = 0; m < 6; ++m) K.Add(Moss(1.f), FVector(30.f, 18.f, 4.f), Pts[1 + m] + FVector(0, 0, 20.f), FRotator(0.f, 30.f * m, 0.f));
		const FVector Pig = Jaw + FVector(70.f, 0, 4.f);
		WRForestKitImpl::Body(*this, Pig, FVector::ForwardVector, 60.f, 28.f, 30.f, Rgb(120, 92, 70) * 0.5f);
		for (const float S : { -1.f, 1.f }) WRForestKitImpl::Leg(*this, Pig + FVector(50.f, S * 14.f, 0.f), Pig + FVector(80.f, S * 30.f, -24.f), 6.f, Rgb(120, 92, 70) * 0.5f, 8.f);
		Root({ Pig + FVector(62.f, 0, 10.f), Pig + FVector(72.f, 6.f, 22.f), Pig + FVector(70.f, -6.f, 30.f) }, 2.f, 1.f, Rgb(120, 92, 70) * 0.5f, 5);
		for (int32 r = 0; r < 5; ++r) SegBox(Pts[r * 2], Pts[FMath::Min(10, r * 2 + 2)], 22.f, 20.f);
		Loop(TEXT("forest.flies"), Jaw + FVector(0, 0, 50.f), FRotator::ZeroRotator, 0.5f);
	}
}

void WRForestLook::FBuild::Ford()
{
	// A crossing on an F2 stream (DC 2.4 kit): the way across runs +X, the water along Y. 0: a gravel ford where elk
	// trails cross, a Grovekin fish weir of stakes downstream and the fish run below it; 1: stepping stones set by the
	// Grovekin, tops a hand over the water, the fish run in the riffle. In spate (after Rain) the current pushes here
	// (hazards track): the stones get wet, the ford goes deep.
	const int32 Var = Act->Variant % 2;
	float SurfZ = 0.f;
	FVector Along(0, 1, 0);
	const FVector Me = Act->GetActorLocation();
	const bool bRiver = WRForestKitImpl::RiverSurface(Me, 4000.f, SurfZ, Along);
	const float WaterZ = bRiver ? SurfZ - float(Me.Z) : G(0.f, 0.f) + 40.f;
	const FVector Down = T.InverseTransformVectorNoScale(Along);
	if (Var == 0)
	{
		// Gravel bars either side of the wade, pale stones rounded by the water; hoof-churned mud at the banks.
		for (int32 g = 0; g < 26; ++g)
		{
			const float X = -500.f + 1000.f * Rand(g);
			const float Y = -250.f + 500.f * Rand(g + 40);
			K.Add(OldStone(0.9f + 0.3f * Rand(g + 7)), FVector(22.f + 18.f * Rand(g + 3), 16.f + 12.f * Rand(g + 4), 8.f), At(X, Y, 3.f), FRotator(0.f, 90.f * Rand(g + 9), 0.f), EWRLookMesh::Cylinder);
		}
		for (const float X : { -620.f, 620.f }) K.Add(Earth(0.5f), FVector(220.f, 400.f, 6.f), At(X, 0.f, 1.f), FRotator(0.f, 0.f, 0.f));
		// The weir: a V of stakes pointing downstream with a wicker trap at its apex.
		const FVector Apex = FVector(0, 0, 0) + Down * 900.f;
		const FVector Side = FVector::CrossProduct(Down, FVector::UpVector);
		for (int32 s = 0; s < 18; ++s)
		{
			const float Tt = (s % 9) / 8.f;
			const float Sd = s < 9 ? -1.f : 1.f;
			const FVector P = Apex - Down * 500.f * (1.f - Tt) + Side * Sd * 450.f * (1.f - Tt);
			K.Rod(Timber(0.75f), FVector(P.X, P.Y, G(float(P.X), float(P.Y), WaterZ - 50.f) - 10.f), FVector(P.X, P.Y, WaterZ + 35.f + 10.f * Rand(s)), 8.f);
		}
		WRForestKitImpl::Body(*this, FVector(Apex.X, Apex.Y, WaterZ - 10.f), Down, 140.f, 32.f, 32.f, RopeC());
		Loop(TEXT("forest.fish_run"), FVector(Apex.X, Apex.Y, WaterZ) - Down * 250.f);
		// A marker pole on the near bank: a pole with antlers (the elk trail's sign).
		const FVector Pole = At(-700.f, 180.f);
		K.Rod(Timber(), Pole - FVector(0, 0, 30.f), Pole + FVector(0, 0, 220.f), 14.f);
		Antler(Pole + FVector(0, 0, 215.f), FVector(0, 1, 0.3f), 80.f, false, BoneC());
		Antler(Pole + FVector(0, 0, 215.f), FVector(0, -1, 0.3f), 80.f, true, BoneC());
	}
	else
	{
		// Stepping stones: flat-topped, an easy stride apart (85 cm), across 12 m of water; the near and far ones bedded
		// into the banks. Each is a block with its own collision.
		const int32 Count = 14;
		for (int32 s = 0; s < Count; ++s)
		{
			const float X = -600.f + 1200.f * s / (Count - 1);
			const float Y = 30.f * FMath::Sin(s * 1.3f);
			const float Bed = G(X, Y, WaterZ - 60.f);
			const float Top = FMath::Max(WaterZ + 15.f, Bed + 20.f);
			const FVector Size(70.f + 15.f * Rand(s), 60.f + 15.f * Rand(s + 20), Top - Bed + 30.f);
			K.Add(OldStone(0.8f + 0.2f * Rand(s + 4)), Size, FVector(X, Y, (Top + Bed - 30.f) * 0.5f), FRotator(0.f, 30.f * Rand(s + 8), 0.f), EWRLookMesh::Cylinder);
			K.Add(Moss(0.9f), FVector(Size.X * 0.6f, Size.Y * 0.4f, 3.f), FVector(X - 10.f, Y + 8.f, Top + 1.f), FRotator(0.f, 30.f * Rand(s + 8), 0.f), EWRLookMesh::Cylinder);
			Box(FVector(X, Y, (Top + Bed - 30.f) * 0.5f), FVector(Size.X * 0.42f, Size.Y * 0.42f, Size.Z * 0.5f));
		}
		Loop(TEXT("forest.fish_run"), FVector(0.f, 0.f, WaterZ) + Down * 300.f);
	}
}

void WRForestLook::FBuild::FoxfireBank()
{
	// A foxfire bank (DC 2.4 kit, F4/F5): a rotting log whose wood glows cold green with honey-fungus mycelium. By day a
	// pale shimmer in the dark of the canopy; at night one of the forest's lights (DC 2.1 night read). Foxfire wood comes
	// off it by hand (the server's node, local (0, 160)): the Foxfire lantern's fuel.
	const int32 Var = Act->Variant % 2;
	const float Len = 900.f + 300.f * Rand(1);
	const float R = 60.f + 15.f * Var;
	const FVector P0 = At(-Len * 0.5f, -20.f, R * 0.7f), P1 = At(Len * 0.5f, 20.f, R * 0.6f);
	UStaticMeshComponent* LogMesh = Log(TEXT("foxfire_log"), Var, P0, P1, R, true);
	if (LogMesh) LogMesh->SetMaterialByName(TEXT("Glow"), WRTreeKit::GlowMaterial(Act, Rgb(127, 224, 122), WRTune(TEXT("forestWorld.foxfireGlow"), 1.5f)));
	// Bracket fungi on its side and the soft-rot cracks: by night they light (N blocks), by day they're pale.
	const FVector Dir = (P1 - P0).GetSafeNormal();
	for (int32 f = 0; f < 9; ++f)
	{
		const FVector At0 = FMath::Lerp(P0, P1, 0.1f + 0.1f * f) + FVector(0, f % 2 ? R * 0.95f : -R * 0.95f, R * (0.1f + 0.3f * Rand(f)));
		K.Add(FSurf::Stone(Rgb(200, 196, 170) * 0.5f, 0.8f), FVector(28.f, 18.f, 6.f), At0, FRotator(0.f, Dir.Rotation().Yaw, 0.f), EWRLookMesh::Cylinder);
		N.Add(Foxfire(1.4f), FVector(24.f, 14.f, 3.f), At0 - FVector(0, 0, 4.f), FRotator(0.f, Dir.Rotation().Yaw, 0.f), EWRLookMesh::Cylinder);
	}
	for (int32 c = 0; c < 6; ++c)
	{
		N.Add(Foxfire(1.f), FVector(120.f, 5.f, 4.f), FMath::Lerp(P0, P1, 0.15f + 0.13f * c) + FVector(0, 0, R * 0.95f), FRotator(0.f, Dir.Rotation().Yaw + 10.f * (c % 3 - 1), 0.f));
	}
	// The mycelium runs out into the leaf litter: glowing threads on the ground round the log.
	for (int32 t = 0; t < 10; ++t)
	{
		const FVector From = FMath::Lerp(P0, P1, Rand(t + 30));
		const FVector To = From + FRotator(0.f, 360.f * Rand(t + 31), 0.f).Vector() * (120.f + 160.f * Rand(t + 32));
		N.Add(Foxfire(0.6f), FVector((To - From).Size2D(), 3.f, 2.f), FVector((From.X + To.X) * 0.5f, (From.Y + To.Y) * 0.5f, G(float(From.X + To.X) * 0.5f, float(From.Y + To.Y) * 0.5f) + 2.f), FRotator(0.f, (To - From).Rotation().Yaw, 0.f));
	}
	if (Var == 1)
	{
		// A bank of rotten wood: a stump the log broke from and chunks of punk wood, glowing in their breaks.
		const FVector Stump = At(-Len * 0.5f - 120.f, -60.f);
		Tree(TEXT("stump"), 1, Stump, 120.f, FRotator(0.f, 30.f, 0.f));
		for (int32 c = 0; c < 4; ++c)
		{
			const FVector Ch = At(-200.f + 140.f * c, 130.f + 30.f * (c % 2), 15.f);
			K.Add(Bark(0.6f), FVector(50.f, 35.f, 30.f), Ch, FRotator(10.f * c, 50.f * c, 0.f));
			N.Add(Foxfire(1.2f), FVector(30.f, 4.f, 20.f), Ch + FVector(0, 18.f, 0), FRotator(10.f * c, 50.f * c, 0.f));
		}
	}
	for (int32 f = 0; f < 3; ++f) Tree(TEXT("fern"), f, At(-300.f + 300.f * f, -R - 120.f), 100.f, FRotator(0.f, 70.f * f, 0.f), 0.f);
	Lamp(FVector(0.f, 0.f, P0.Z + R + 60.f), Rgb(127, 224, 122), WRTune(TEXT("forestWorld.foxfireBankLight"), 1400.f), WRTune(TEXT("forestWorld.foxfireBankRadius"), 1100.f));
}

void WRForestLook::FBuild::FieldRuin()
{
	// A field ruin (DC 2.4 kit, F1): the newcomers' farm buildings that the meadows took back: 0 a long stone barn, roofless,
	// one gable standing, a side wall fallen into a rubble ramp; 1 a byre with stalls, half its turf roof still on the
	// timbers (shelter from Rain); 2 a corn-drying kiln and its shed, the kiln bowl intact. Elder and nettles growing in.
	const int32 Var = Act->Variant % 3;
	if (Var == 0)
	{
		const float L = 1400.f, W = 700.f;
		WRForestKitImpl::StoneWall(*this, FVector(-L * 0.5f, -W * 0.5f, 0), FVector(L * 0.5f, -W * 0.5f, 0), 320.f, 60.f, true, 11);
		WRForestKitImpl::StoneWall(*this, FVector(-L * 0.5f, -W * 0.5f, 0), FVector(-L * 0.5f, W * 0.5f, 0), 380.f, 60.f, false, 12);   // the standing gable's wall
		WRForestKitImpl::StoneWall(*this, FVector(L * 0.5f, -W * 0.5f, 0), FVector(L * 0.5f, -120.f, 0), 260.f, 60.f, true, 13);         // door gap at +X
		WRForestKitImpl::StoneWall(*this, FVector(L * 0.5f, 120.f, 0), FVector(L * 0.5f, W * 0.5f, 0), 240.f, 60.f, true, 14);
		// The gable triangle over the west wall: stepped courses up to the ridge.
		const float Gz = G(-L * 0.5f, 0.f);
		for (int32 s = 0; s < 6; ++s) K.Add(OldStone(0.85f), FVector(60.f, W * (1.f - s / 6.f), 38.f), FVector(-L * 0.5f, 0.f, Gz + 380.f + 38.f * s + 19.f));
		Box(FVector(-L * 0.5f, 0.f, Gz + 500.f), FVector(30.f, W * 0.3f, 120.f));
		// The fallen north wall: a rubble ramp of stones and a few still-coursed lumps.
		for (int32 r = 0; r < 24; ++r) WRForestKitImpl::Stone(*this, At(-L * 0.45f + L * 0.9f * Rand(r), W * 0.5f + 40.f + 160.f * Rand(r + 50), 15.f), FVector(50.f, 40.f, 30.f) * (0.7f + 0.6f * Rand(r + 70)), 360.f * Rand(r + 90), r);
		// Its roof timbers, fallen in and rotting; the elder tree that grew in the middle.
		for (int32 b = 0; b < 4; ++b) K.Rod(Timber(0.6f), At(-L * 0.4f + 300.f * b, -W * 0.45f, 300.f - 60.f * (b % 2)), At(-L * 0.35f + 300.f * b, W * 0.3f, 10.f), 22.f, true);
		Tree(TEXT("hazel"), 2, At(100.f, 50.f), 450.f, FRotator(0.f, 20.f, 0.f));
	}
	else if (Var == 1)
	{
		// The byre: low walls, three stalls, a turf roof on rafters, half still on (the west half): shelter.
		const float L = 1000.f, W = 560.f, H = 200.f;
		WRForestKitImpl::StoneWall(*this, FVector(-L * 0.5f, -W * 0.5f, 0), FVector(L * 0.5f, -W * 0.5f, 0), H, 55.f, false, 21);
		WRForestKitImpl::StoneWall(*this, FVector(-L * 0.5f, W * 0.5f, 0), FVector(L * 0.5f, W * 0.5f, 0), H, 55.f, false, 22);
		WRForestKitImpl::StoneWall(*this, FVector(-L * 0.5f, -W * 0.5f, 0), FVector(-L * 0.5f, W * 0.5f, 0), H + 60.f, 55.f, false, 23);
		WRForestKitImpl::StoneWall(*this, FVector(L * 0.5f, -W * 0.5f, 0), FVector(L * 0.5f, -100.f, 0), H, 55.f, true, 24);
		const float Gz = G(0.f, 0.f);
		for (int32 s = 0; s < 3; ++s)
		{
			// Stall partitions of slabs on edge, a manger trough.
			const float X = -L * 0.35f + 230.f * s;
			K.Add(OldStone(0.8f), FVector(14.f, W * 0.45f, 120.f), FVector(X, -W * 0.25f, Gz + 60.f));
			K.Add(Timber(0.7f), FVector(140.f, 40.f, 30.f), FVector(X + 110.f, -W * 0.5f + 60.f, Gz + 70.f));
		}
		// The roof: a ridge beam on the gables, rafters, turf slabs over the west half only; the east half rafters bare.
		K.Add(Timber(0.75f), FVector(L, 24.f, 24.f), FVector(0.f, 0.f, Gz + H + 210.f));
		for (int32 r = 0; r < 8; ++r)
		{
			const float X = -L * 0.45f + L * 0.9f * r / 7.f;
			for (const float S : { -1.f, 1.f }) K.Rod(Timber(0.7f), FVector(X, S * W * 0.5f, Gz + H + 20.f), FVector(X, 0.f, Gz + H + 210.f), 14.f, true);
		}
		const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(190.f, W * 0.5f));
		const float Slope = FMath::Sqrt(FMath::Square(W * 0.5f) + FMath::Square(190.f)) + 40.f;
		for (const float S : { -1.f, 1.f })
		{
			K.Add(Turf(), FVector(L * 0.5f, Slope, 35.f), FVector(-L * 0.25f, S * W * 0.25f, Gz + H + 125.f), FRotator(0.f, 0.f, -S * Pitch));
			Box(FVector(-L * 0.25f, S * W * 0.25f, Gz + H + 125.f), FVector(L * 0.25f, Slope * 0.5f, 18.f), FRotator(0.f, 0.f, -S * Pitch));
		}
		K.Add(Thatch(), FVector(240.f, 160.f, 30.f), FVector(-L * 0.3f, W * 0.2f, Gz + 15.f));   // old hay in the corner: a dry bed
	}
	else
	{
		// The corn-drying kiln: a round stone bowl set into a bank with its flue, and the barn-shed it served.
		const FVector Kiln = At(-300.f, 0.f);
		for (int32 c = 0; c < 4; ++c)
		{
			for (int32 i = 0; i < 14; ++i)
			{
				const float Ang = 360.f * i / 14.f;
				const FVector Dir = FRotator(0.f, Ang, 0.f).Vector();
				WRForestKitImpl::Stone(*this, Kiln + Dir * 170.f + FVector(0, 0, 18.f + 38.f * c), FVector(70.f, 45.f, 36.f), Ang + 90.f, i + c * 14);
			}
		}
		K.Add(Black(0.6f), FVector(280.f, 280.f, 4.f), Kiln + FVector(0, 0, 4.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		for (int32 f = 0; f < 6; ++f) WRForestKitImpl::Stone(*this, Kiln + FVector(170.f + 60.f * f, 0.f, 20.f), FVector(60.f, 90.f, 40.f), 0.f, f + 60);   // the flue to the fire pit
		for (int32 i = 0; i < 6; ++i) Box(Kiln + FRotator(0.f, 60.f * i, 0.f).Vector() * 170.f + FVector(0, 0, 75.f), FVector(30.f, 90.f, 75.f), FRotator(0.f, 60.f * i + 90.f, 0.f));
		WRForestKitImpl::StoneWall(*this, FVector(200.f, -300.f, 0), FVector(900.f, -300.f, 0), 210.f, 50.f, true, 31);
		WRForestKitImpl::StoneWall(*this, FVector(900.f, -300.f, 0), FVector(900.f, 300.f, 0), 240.f, 50.f, false, 32);
		WRForestKitImpl::StoneWall(*this, FVector(200.f, 300.f, 0), FVector(900.f, 300.f, 0), 160.f, 50.f, true, 33);
		Tree(TEXT("holly"), 1, At(560.f, 0.f), 380.f, FRotator(0.f, 40.f, 0.f));
	}
	// Nettles and ferns round any ruin (nitrogen where animals were kept), never in the doorway.
	for (int32 f = 0; f < 5; ++f) Tree(TEXT("fern"), f, At(-600.f + 300.f * f, -520.f - 60.f * Rand(f)), 80.f + 30.f * Rand(f + 3), FRotator(0.f, 50.f * f, 0.f), 0.f);
}

void WRForestLook::FBuild::CanopyPlatform()
{
	// A canopy platform (DC 2.4 kit, F5; plan 2.5 canopy layer): a Grovekin hunter's deck 20-40 m up on a giant, where the
	// walkways meet. It holds a hunter's cache (UWRForestWorld, at local (160, 0) on the deck). 0: a bare deck with a rope
	// ladder hanging and a haul pulley; 1: a hide shelter on the deck (where the hunters slept).
	const int32 Var = Act->Variant % 2;
	const FWRForestData::FSite* Row = WRForestKitImpl::RowOf(Act, FWRForestData::Get().Kit);
	const float DeckZ = (Row ? WRForestShape::CanopyDeckZ(*Row) : float(Act->GetActorLocation().Z) + 2500.f) - float(Act->GetActorLocation().Z);
	FVector Trunk;
	float TrunkR;
	if (const FWRForestData::FTree* Gt = WRForestShape::GiantNear(Act->GetActorLocation(), 1500.f))
	{
		Trunk = Local(Gt->Base);
		TrunkR = Gt->Radius;
	}
	else
	{
		TrunkR = WRForestShape::TrunkRadius(TEXT("giant"), 9000.f);
		Trunk = At(-(TrunkR + 300.f), 0.f);
		TrunkR = WRForestKitImpl::OwnGiant(*this, Trunk, 9000.f, Act->Variant % 4);
	}
	const FVector Centre(160.f, 0.f, DeckZ);
	const float Yaw = (Centre - Trunk).GetSafeNormal2D().Rotation().Yaw;
	const float Hx = 300.f, Hy = 240.f;
	Deck(Centre, FVector2D(Hx, Hy), DeckZ, Yaw, { Trunk }, { TrunkR }, true);
	const FRotator R(0.f, Yaw, 0.f);
	const auto P = [&](float X, float Y, float Z) { return FVector(Centre.X, Centre.Y, DeckZ) + R.RotateVector(FVector(X, Y, Z)); };
	if (Var == 0)
	{
		// The rope ladder down the deck's outer edge to the forest floor (decoration: the way up is the Lift and walkways).
		const FVector Top = P(Hx - 10.f, 60.f, 0.f);
		const float Ground = G(float(Top.X), float(Top.Y), DeckZ - 2500.f);
		for (const float S : { -1.f, 1.f }) Line(Top + R.RotateVector(FVector(0, S * 22.f, 0)), FVector(Top.X, Top.Y, Ground + 40.f) + R.RotateVector(FVector(30.f, S * 22.f, 0)), 2.f, 30.f, RopeC(), 12);
		for (float Z = DeckZ - 40.f; Z > Ground + 60.f; Z -= 45.f)
		{
			const float Tt = (DeckZ - Z) / FMath::Max(1.f, DeckZ - Ground);
			K.Add(Timber(0.8f), FVector(5.f, 44.f, 5.f), FVector(Top.X, Top.Y, Z) + R.RotateVector(FVector(30.f * Tt, 0, 0)), R);
		}
		// The haul pulley on a gallows arm, its rope down to a basket left on the ground.
		K.Rod(Timber(), P(-Hx + 30.f, -Hy + 30.f, 0.f), P(-Hx + 30.f, -Hy + 30.f, 260.f), 16.f, true);
		K.Rod(Timber(), P(-Hx + 30.f, -Hy + 30.f, 250.f), P(-Hx - 90.f, -Hy + 30.f, 250.f), 14.f, true);
		K.Disc(Timber(0.8f), P(-Hx - 80.f, -Hy + 30.f, 225.f), R.RotateVector(FVector(0, 1, 0)), 40.f, 8.f);
		const FVector Drop = P(-Hx - 100.f, -Hy + 30.f, 0.f);
		Line(P(-Hx - 100.f, -Hy + 30.f, 220.f), FVector(Drop.X, Drop.Y, G(float(Drop.X), float(Drop.Y), DeckZ - 2500.f) + 60.f), 1.6f, 0.f, RopeC(), 2);
		WRForestKitImpl::Body(*this, FVector(Drop.X, Drop.Y, G(float(Drop.X), float(Drop.Y), DeckZ - 2500.f)), FVector::UpVector, 60.f, 30.f, 30.f, RopeC());
	}
	else
	{
		// The hide shelter: an A-frame of poles at the deck's back with deer hides pegged over it, a bed of bracken.
		for (const float X : { -Hx + 40.f, -Hx + 220.f })
		{
			K.Rod(Timber(0.85f), P(X, -Hy + 20.f, 0.f), P(X, -60.f, 170.f), 8.f);
			K.Rod(Timber(0.85f), P(X, 100.f, 0.f), P(X, -60.f, 170.f), 8.f);
		}
		K.Rod(Timber(0.85f), P(-Hx + 40.f, -60.f, 170.f), P(-Hx + 220.f, -60.f, 170.f), 8.f);
		for (const float S : { -1.f, 1.f }) K.Add(Hide(0.9f + 0.1f * S), FVector(200.f, 190.f, 4.f), P(-Hx + 130.f, -60.f + S * 70.f - 20.f, 90.f), FRotator(0.f, Yaw, S * 55.f));
		K.Add(Thatch(), FVector(160.f, 140.f, 10.f), P(-Hx + 130.f, -40.f, 6.f), R);
	}
	// A Grovekin foxfire jar on a peg: the deck reads at night from the floor below.
	WRForestKitImpl::FoxfireJar(*this, P(Hx - 20.f, -Hy + 20.f, 160.f));
	K.Rod(Timber(), P(Hx - 20.f, -Hy + 20.f, 115.f), P(Hx - 20.f, -Hy + 20.f, 165.f), 6.f);
}

void WRForestLook::FBuild::WalkwayKind()
{
	// A canopy walkway (plan 2.5): a sagging Grovekin plank way between two platforms 20-40 m up. Its ropes end in
	// lashings round a post pair at each end with an iron ring (nothing just stops in the air).
	const FVector P0 = Local(Act->Extra), P1 = Local(Act->Extra2);
	if (FVector::Dist(P0, P1) < 100.f) return;
	Walkway(P0, P1);
	const FVector Dir = (P1 - P0).GetSafeNormal2D();
	const FVector Side = FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal();
	for (int32 e = 0; e < 2; ++e)
	{
		const FVector End = e == 0 ? P0 : P1;
		const FVector Back = e == 0 ? -Dir : Dir;
		for (const float S : { -1.f, 1.f })
		{
			const FVector Post = End + Back * 30.f + Side * S * 70.f;
			K.Rod(Timber(), Post - FVector(0, 0, 60.f), Post + FVector(0, 0, 125.f), 14.f, true);
			// The rail and stringer ropes wrap the post (three turns) and end in a half-hitch on the ring.
			for (int32 w = 0; w < 3; ++w) K.Disc(Rope(), Post + FVector(0, 0, 100.f + 6.f * w), FVector::UpVector, 20.f, 4.f);
			for (int32 w = 0; w < 2; ++w) K.Disc(Rope(), Post + FVector(0, 0, -4.f + 6.f * w), FVector::UpVector, 20.f, 4.f);
			K.Disc(Iron(), Post + Back * 12.f + FVector(0, 0, 80.f), Back, 18.f, 3.f);
			Line(Post + Back * 12.f + FVector(0, 0, 80.f), Post + FVector(0, 0, 100.f), 1.6f, 4.f, RopeC(), 3);
			Box(Post + FVector(0, 0, 30.f), FVector(8.f, 8.f, 95.f));
		}
	}
}

// ================================================================================================ the shrines' footings

void WRForestLook::FBuild::ShrineGroveCircle()
{
	// The Great Shrine: the Grove Circle (DC 2.11), inside the Long-hall: nine pleached limes in a ring, their lower limbs
	// trained sideways and woven into one living hedge-wall with gaps between trunks, their crowns plaited overhead. In
	// the middle, the stone bowl (the AWRShrine) stands on a pedestal of living roots; the brazier burns heartwood resin
	// with an amber smoke. Rowan works here by day (his spot at local (250, 520): a gap in the ring is left for him).
	const float Ring = 750.f;
	TArray<FVector> Trunks;
	for (int32 i = 0; i < 9; ++i)
	{
		const float Ang = 44.f + 40.f * i;
		const FVector Dir = FRotator(0.f, Ang, 0.f).Vector();
		const FVector Base = At(float(Dir.X) * Ring, float(Dir.Y) * Ring);
		Tree(TEXT("lime"), i % 4, Base, 1300.f + 100.f * Rand(i), FRotator(0.f, Ang + 180.f, 0.f));
		Trunks.Add(Base);
	}
	// The pleaching: limbs from each trunk to the next at 2.5 m and 4 m, two withies twisted together; a third at 7 m
	// where the crowns are plaited.
	for (int32 i = 0; i < 9; ++i)
	{
		const FVector A0 = Trunks[i], A1 = Trunks[(i + 1) % 9];
		for (const float Z : { 250.f, 400.f, 700.f })
		{
			for (int32 w = 0; w < 2; ++w)
			{
				TArray<FVector> Pts;
				for (int32 s = 0; s <= 8; ++s)
				{
					const float Tt = s / 8.f;
					const FVector Mid = FMath::Lerp(A0, A1, Tt) + FVector(0, 0, Z - 30.f * FMath::Sin(PI * Tt));
					const FVector Sd = FVector::CrossProduct((A1 - A0).GetSafeNormal(), FVector::UpVector) * 8.f * FMath::Sin(Tt * 2.f * PI * 3.f + w * PI);
					Pts.Add(Mid + Sd + FVector(0, 0, 6.f * FMath::Cos(Tt * 2.f * PI * 3.f + w * PI)));
				}
				M.Tube(Pts, Z > 500.f ? 7.f : 5.f, 6, BarkC(), 0.9f);
			}
			if (Z < 500.f)
			{
				// Leaf sprays along the woven limbs (the living wall's green).
				for (int32 l = 1; l < 4; ++l) K.Add(Leaf(1.f + 0.1f * l), FVector(110.f, 30.f, 60.f), FMath::Lerp(A0, A1, l / 4.f) + FVector(0, 0, Z + 25.f), FRotator(0.f, (A1 - A0).Rotation().Yaw, 10.f * l));
			}
		}
	}
	// The root pedestal: roots from the ground at 1.2-1.6 m out climbing and curling in under the bowl's rim (the bowl
	// and its brazier are the AWRShrine's: nothing here crosses its middle 2 m above 0.8 m).
	for (int32 r = 0; r < 7; ++r)
	{
		const FVector Dir = FRotator(0.f, 360.f * r / 7.f + 15.f, 0.f).Vector();
		const FVector Side = FVector::CrossProduct(Dir, FVector::UpVector);
		Root({ At(float(Dir.X) * 170.f, float(Dir.Y) * 170.f, -10.f), At(float(Dir.X) * 135.f, float(Dir.Y) * 135.f, 25.f) + Side * 15.f, At(0.f, 0.f, 55.f) + Dir * 112.f + Side * 25.f, At(0.f, 0.f, 75.f) + Dir * 102.f + Side * 30.f }, 16.f, 7.f, RootC(), 7);
	}
	// A ring kerb of worn stones round the circle's floor and raked earth inside (Rowan keeps it).
	for (int32 s = 0; s < 28; ++s)
	{
		const float Ang = 360.f * s / 28.f;
		const FVector Dir = FRotator(0.f, Ang, 0.f).Vector();
		WRForestKitImpl::Stone(*this, At(float(Dir.X) * 420.f, float(Dir.Y) * 420.f, 8.f), FVector(70.f, 30.f, 18.f), Ang + 90.f, s);
	}
	K.Add(Earth(0.9f), FVector(820.f, 820.f, 3.f), At(0.f, 0.f, 1.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
	// Resin cakes stacked by the pedestal (Rowan grinds resin here by day) and the grinding stone.
	K.Add(OldStone(0.9f), FVector(70.f, 70.f, 20.f), At(250.f, 420.f, 10.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
	K.Add(OldStone(0.8f), FVector(40.f, 40.f, 14.f), At(250.f, 420.f, 27.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
	for (int32 c = 0; c < 5; ++c) K.Add(Amber(0.05f), FVector(16.f, 16.f, 8.f), At(300.f, 380.f + 18.f * c, 5.f + 8.f * (c % 2)), FRotator(0.f, 30.f * c, 0.f), EWRLookMesh::Cylinder);
	Loop(TEXT("forest.resin_smoke"), FVector(0.f, 0.f, G(0.f, 0.f) + 190.f));
	Lamp(FVector(0.f, 0.f, G(0.f, 0.f) + 350.f), Rgb(208, 138, 30), WRTune(TEXT("forestWorld.groveCircleLight"), 2500.f), 1400.f);
}

void WRForestLook::FBuild::ShrineHedge()
{
	// The Hedge Shrine (DC 2.11, F1): a stone niche built into an ancient hedgerow, a carved hulder face looking out of
	// the lintel. The hedge runs across behind it (local Y at x -300); the niche's back and jambs frame the brazier.
	for (int32 i = -6; i <= 6; ++i)
	{
		if (FMath::Abs(i) < 1) continue;
		const float Y = 190.f * i;
		Tree((i % 3 == 0) ? FName(TEXT("holly")) : FName(TEXT("hazel")), (i + 8) % 4, At(-300.f + 30.f * Rand(i + 9), Y, 30.f), 360.f + 160.f * Rand(i + 20), FRotator(0.f, 50.f * i, 0.f), 0.f);
		K.Add(Turf(), FVector(160.f, 200.f, 70.f), At(-300.f, Y, 15.f));
	}
	Tree(TEXT("oak"), 3, At(-380.f, -1300.f), 2000.f, FRotator(0.f, 120.f, 0.f));   // the hedge's old standard oak
	Box(FVector(-300.f, -700.f, G(-300.f, -700.f) + 150.f), FVector(80.f, 480.f, 150.f));
	Box(FVector(-300.f, 700.f, G(-300.f, 700.f) + 150.f), FVector(80.f, 480.f, 150.f));
	// The niche: back wall, two jambs, a lintel stone; mossed, older than the hedge's oak.
	const float Gz = G(-200.f, 0.f);
	for (int32 c = 0; c < 7; ++c) WRForestKitImpl::Stone(*this, FVector(-220.f, 0.f, Gz + 20.f + 38.f * c), FVector(50.f, 360.f, 36.f), 0.f, c);
	for (const float S : { -1.f, 1.f })
	{
		for (int32 c = 0; c < 6; ++c) WRForestKitImpl::Stone(*this, FVector(-140.f, S * 175.f, Gz + 20.f + 38.f * c), FVector(160.f, 50.f, 36.f), 0.f, c + 10 * int32(S + 2.f));
		Box(FVector(-140.f, S * 175.f, Gz + 120.f), FVector(80.f, 25.f, 120.f));
	}
	K.Add(OldStone(0.75f), FVector(190.f, 420.f, 50.f), FVector(-160.f, 0.f, Gz + 265.f));
	K.Add(Moss(1.f), FVector(170.f, 400.f, 6.f), FVector(-160.f, 0.f, Gz + 292.f));
	Box(FVector(-200.f, 0.f, Gz + 140.f), FVector(30.f, 180.f, 140.f));
	// The hulder's face carved in the lintel's front: an oval face, hair falling down both jambs (she watches the field).
	const FVector Face(-60.f, 0.f, Gz + 262.f);
	Ball(Face, 20.f, PaleWoodC() * 0.75f, 0.85f, 1.25f);
	for (int32 h = 0; h < 6; ++h)
	{
		const float S = h < 3 ? -1.f : 1.f;
		Root({ Face + FVector(-2.f, S * 14.f, 18.f), Face + FVector(0.f, S * (30.f + 8.f * (h % 3)), 0.f), Face + FVector(-4.f, S * (40.f + 10.f * (h % 3)), -60.f - 20.f * (h % 3)) }, 3.f, 1.5f, PaleWoodC() * 0.7f, 5);
	}
	// Offerings on the niche's sill: a bread stone, a twist of straw, wildflowers gone to seed.
	K.Add(OldStone(0.9f), FVector(40.f, 34.f, 12.f), FVector(-170.f, -110.f, Gz + 6.f));
	Ball(FVector(-170.f, -110.f, Gz + 20.f), 9.f, PaleWoodC() * 0.9f, 0.9f, 0.6f);
	K.Add(Straw(), FVector(30.f, 6.f, 6.f), FVector(-170.f, 110.f, Gz + 5.f), FRotator(0.f, 40.f, 0.f));
}

void WRForestLook::FBuild::ShrineFalls()
{
	// The Falls Shrine (DC 2.11, F2): it stands on the Grovekin fishing platform under the Great Falls' spray (the platform
	// itself is the falls pool's site build). Here only what's round the brazier: a wet rail on the pool side, a rack of
	// fish drying in the spray-smoke, an eel basket, a coil of wet net. Everything sits on the platform (traced).
	const float Z0 = G(0.f, 0.f);
	// Stood on the falls pool's own platform (WRForestShape::ShrineSpot puts it there, in the deck's middle): the deck
	// already has its rails, racks, net and creel (WRForestScenerySites FallsPool), so here only what the shrine adds:
	// the spray's puddles round the brazier, and the morning's catch laid on a board at its foot for the forest.
	if (const FWRForestData::FSite* Pool = FWRForestData::Get().FindSite(TEXT("falls_pool")); Pool && FVector::Dist2D(Pool->At, Act->GetActorLocation()) < 150.f)
	{
		for (int32 w = 0; w < 4; ++w)
		{
			const FVector Pd = FRotator(0.f, 45.f + 90.f * w, 0.f).Vector() * (150.f + 20.f * Rand(w));
			K.Add(Water(), FVector(60.f + 25.f * Rand(w + 7), 44.f, 1.f), FVector(Pd.X, Pd.Y, Z0 + 1.f), FRotator(0.f, 40.f * w, 0.f), EWRLookMesh::Cylinder);
		}
		K.Add(Carved(0.8f), FVector(80.f, 34.f, 5.f), FVector(-120.f, 0.f, Z0 + 2.5f));
		for (int32 f = 0; f < 3; ++f) WRForestKitImpl::Body(*this, FVector(-140.f + 22.f * f, -6.f + 6.f * f, Z0 + 9.f), FVector(1.f, 0.15f * f, 0.f), 34.f + 4.f * f, 5.f, 3.f, Rgb(150, 158, 160) * 0.55f, 0.35f);
		return;
	}
	// The rail along the pool side (+X): posts and a pole, slick with spray.
	for (int32 p = 0; p < 4; ++p) K.Rod(Timber(0.6f), FVector(230.f, -300.f + 200.f * p, Z0 - 20.f), FVector(230.f, -300.f + 200.f * p, Z0 + 105.f), 12.f);
	K.Rod(Timber(0.55f), FVector(230.f, -320.f, Z0 + 100.f), FVector(230.f, 320.f, Z0 + 100.f), 9.f);
	Box(FVector(230.f, 0.f, Z0 + 55.f), FVector(6.f, 320.f, 50.f));
	// The drying rack: an A-frame each end, a pole across, trout hung by the gills.
	const FVector Rk(-120.f, -260.f, Z0);
	for (const float X : { -110.f, 110.f })
	{
		K.Rod(Timber(0.7f), Rk + FVector(X, -50.f, 0.f), Rk + FVector(X, 0.f, 190.f), 7.f);
		K.Rod(Timber(0.7f), Rk + FVector(X, 50.f, 0.f), Rk + FVector(X, 0.f, 190.f), 7.f);
	}
	K.Rod(Timber(0.7f), Rk + FVector(-120.f, 0.f, 185.f), Rk + FVector(120.f, 0.f, 185.f), 6.f);
	for (int32 f = 0; f < 6; ++f)
	{
		const FVector Hook = Rk + FVector(-90.f + 36.f * f, 0.f, 182.f);
		Line(Hook, Hook - FVector(0, 0, 14.f), 0.8f, 0.f, RopeC(), 2);
		WRForestKitImpl::Body(*this, Hook - FVector(0, 0, 14.f), -FVector::UpVector, 40.f + 6.f * (f % 2), 6.f, 3.f, Rgb(150, 158, 160) * 0.55f, 0.35f);
	}
	// The eel basket (a long wicker funnel trap) and the wet net heaped by the rail.
	WRForestKitImpl::Body(*this, FVector(150.f, 220.f, Z0 + 22.f), FVector(1.f, 0.3f, 0.f), 120.f, 24.f, 24.f, RopeC() * 0.8f);
	K.Add(Rope(), FVector(90.f, 70.f, 20.f), FVector(150.f, -200.f, Z0 + 10.f), FRotator(0.f, 20.f, 0.f), EWRLookMesh::Cylinder);
	// Spray puddles on the boards.
	for (int32 w = 0; w < 4; ++w) K.Add(Water(), FVector(70.f + 30.f * Rand(w), 50.f, 1.f), FVector(-200.f + 140.f * w, 120.f - 60.f * (w % 2), Z0 + 1.f), FRotator(0.f, 40.f * w, 0.f), EWRLookMesh::Cylinder);
}

void WRForestLook::FBuild::ShrineNurse()
{
	// The Nurse Shrine (DC 2.11, F4): a brazier in the hollow of the Nurse Log's broken end. The log's end is rotted into
	// a hollow you could stand in: a half-shell of bark round the shrine on the log's side, its inner wood soot-black from
	// years of fire, amber resin crusting the lip, steps cut in its rim, moss curtains hanging.
	const FWRForestData& D = FWRForestData::Get();
	FVector Into(-1.f, 0.f, 0.f);   // toward the log (local)
	float R = 260.f;
	const FWRForestData::FLog* Nurse = nullptr;
	float Best = 4000.f;
	for (const FWRForestData::FLog& L : D.Fallen)
	{
		const float Dd = FVector::Dist2D(L.B, Act->GetActorLocation());
		if ((L.bNurse || !Nurse) && Dd < Best) { Best = Dd; Nurse = &L; }
	}
	if (Nurse)
	{
		Into = T.InverseTransformVectorNoScale((Nurse->A - Nurse->B).GetSafeNormal2D());
		R = FMath::Max(R, Nurse->Radius * 1.25f);
	}
	const FVector Side = FVector::CrossProduct(Into, FVector::UpVector);
	const float Gz = G(0.f, 0.f);
	// The shell: bark staves in a half ring on the log's side, from ground to over head height.
	for (int32 s = 0; s < 12; ++s)
	{
		const float Ang = -100.f + 200.f * s / 11.f;
		const FVector Dir = (Into * FMath::Cos(FMath::DegreesToRadians(Ang)) + Side * FMath::Sin(FMath::DegreesToRadians(Ang))).GetSafeNormal();
		const float H = 320.f + 60.f * Rand(s);
		const FVector Mid = FVector(0, 0, Gz) + Dir * R + FVector(0, 0, H * 0.5f);
		K.Add(Bark(0.85f), FVector(40.f, 2.f * PI * R / 11.f * 0.6f, H), Mid, Dir.Rotation());
		K.Add(Black(0.6f), FVector(6.f, 2.f * PI * R / 11.f * 0.6f, H * 0.9f), Mid - Dir * 22.f, Dir.Rotation());   // soot inside
		if (s % 3 == 0) K.Add(Amber(0.1f), FVector(14.f, 24.f, 30.f), FVector(0, 0, Gz) + Dir * (R - 10.f) + FVector(0, 0, H - 10.f), Dir.Rotation());   // resin on the lip
		if (s % 2 == 0) K.Add(Moss(0.9f), FVector(8.f, 40.f, 120.f), FVector(0, 0, Gz) + Dir * (R - 26.f) + FVector(0, 0, H - 60.f), Dir.Rotation());   // hanging moss
		Box(Mid, FVector(20.f, PI * R / 11.f * 0.6f, H * 0.5f), Dir.Rotation());
	}
	// Steps cut into the rim at one side (people climbed up to tend the saplings on its back).
	for (int32 st = 0; st < 4; ++st)
	{
		const FVector Step = FVector(0, 0, Gz) + Side * (R + 40.f) + Into * (40.f * st) + FVector(0, 0, 40.f + 70.f * st);
		K.Add(Carved(0.8f), FVector(60.f, 70.f, 12.f), Step, Side.Rotation());
		Box(Step, FVector(30.f, 35.f, 6.f), Side.Rotation());
	}
	// Heartwood offcuts stacked as offerings (fallen wood only: the pact), ash in a ring round the brazier's foot.
	for (int32 o = 0; o < 4; ++o) K.Add(FSurf::Wood(Rgb(150, 92, 40) * 0.5f), FVector(70.f, 14.f, 14.f), FVector(0, 0, Gz + 7.f + 14.f * o) + Into * 170.f + Side * 120.f, FRotator(0.f, (Side.Rotation().Yaw) + 90.f * (o % 2), 0.f));
	K.Add(FSurf::Stone(Rgb(120, 116, 110) * 0.5f, 0.95f), FVector(260.f, 260.f, 2.f), FVector(0, 0, Gz + 1.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
	Lamp(FVector(0, 0, Gz + 260.f) + Into * 60.f, Rgb(208, 138, 30), WRTune(TEXT("forestWorld.nurseShrineLight"), 1200.f), 900.f);
}

void WRForestLook::FBuild::ShrineCanopy()
{
	// The Canopy Shrine (DC 2.11, F5): a brazier on a walkway platform 30 m up on a giant, reached by the Canopy Lift and
	// its walkways. The deck's top is WRForestShape::CanopyDeckZ of the shrine's row (the shrine is placed there too).
	const FWRForestData::FSite* Row = WRForestKitImpl::RowOf(Act, FWRForestData::Get().Shrines);
	const float DeckZ = (Row ? WRForestShape::CanopyDeckZ(*Row) : float(Act->GetActorLocation().Z) + WRTune(TEXT("forestWorld.canopyShrineZ"), 3000.f)) - float(Act->GetActorLocation().Z);
	FVector Trunk;
	float TrunkR;
	if (const FWRForestData::FTree* Gt = WRForestShape::GiantNear(Act->GetActorLocation(), 2000.f))
	{
		Trunk = Local(Gt->Base);
		TrunkR = Gt->Radius;
	}
	else
	{
		TrunkR = WRForestShape::TrunkRadius(TEXT("giant"), 10000.f);
		Trunk = At(-(TrunkR + 400.f), 0.f);
		TrunkR = WRForestKitImpl::OwnGiant(*this, Trunk, 10000.f, 2);
	}
	const float Yaw = (FVector(0, 0, 0) - Trunk).GetSafeNormal2D().Rotation().Yaw;
	const float Hx = 380.f, Hy = 360.f;
	Deck(FVector(0.f, 0.f, DeckZ), FVector2D(Hx, Hy), DeckZ, Yaw, { Trunk }, { TrunkR }, true);
	const FRotator R(0.f, Yaw, 0.f);
	const auto P = [&](float X, float Y, float Z) { return FVector(0.f, 0.f, DeckZ) + R.RotateVector(FVector(X, Y, Z)); };
	// A hearth-stone under the brazier (fire on a plank deck sits on stone), a hulder post at the deck's trunk side.
	K.Add(OldStone(0.8f), FVector(240.f, 240.f, 10.f), P(0.f, 0.f, 5.f), R, EWRLookMesh::Cylinder);
	Hulder(P(-Hx + 40.f, Hy - 60.f, 0.f), Yaw, 0.9f);
	// Foxfire jars at two corners: the brazier's deck reads at night from the forest floor.
	for (const float S : { -1.f, 1.f })
	{
		const FVector Peg = P(Hx - 20.f, S * (Hy - 20.f), 0.f);
		K.Rod(Timber(), Peg + FVector(0, 0, 110.f), Peg + FVector(0, 0, 170.f), 6.f);
		WRForestKitImpl::FoxfireJar(*this, Peg + FVector(0, 0, 165.f));
	}
	// Ribbons of bark cloth tied on the rail (prayers left by those who climbed up).
	for (int32 r = 0; r < 6; ++r) K.Add(Canvas(0.7f + 0.1f * r), FVector(4.f, 8.f, 50.f), P(-Hx + 120.f * r, -Hy, 80.f), FRotator(0.f, Yaw, 8.f * (r % 3)));
}
