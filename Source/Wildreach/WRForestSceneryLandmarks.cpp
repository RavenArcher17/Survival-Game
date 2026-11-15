// The Ancient forest's landmarks the world track owns (cooks/biome-forest.md 2.3, 2.20; cooks/build-biome-forest.md 2.5):
// L2 the Eldest, L3 the Great Falls (and every waterfall's falling sheet), L4 the Rotted Treant, L8 the Hermit's Oak,
// L9 the Strangler Arch, L10 the Antler Barrow, L11 the Bell-Tree, L12 the Ringfort. Each is the skyline read from a
// kilometre away by day and has its own read at night (DC 2.3's last column). The tree each stands on comes from
// forest.json "giants" (the generator's row of that kind near the site) or, if the row is missing, from tuning.
// FBuild (WRForestSceneryKit.h) works in the actor's local space: the actor stands on the site's ground point, turned by
// the site's yaw.

#include "WRForestSceneryKit.h"

namespace WRForestLmImpl
{
	using WRForestLook::FBuild;

	/** A landmark's tree in the actor's space: base, height, radius at breast height, turn and variant. */
	struct FLandTree
	{
		FVector Base = FVector::ZeroVector;
		float Height = 0.f;
		float Radius = 0.f;
		float Yaw = 0.f;
		int32 Var = 0;
		bool bRow = false;
	};

	/** The generator's tree of this kind near the site (forest.json "giants"), else one of DefHeight at the site. */
	FLandTree FindTree(FBuild& B, FName TreeKind, FName Species, float DefHeight, float Range = 3000.f)
	{
		FLandTree Out;
		const FVector Here = B.Act->GetActorLocation();
		if (const FWRForestData::FTree* Row = WRForestShape::GiantNear(Here, Range, TreeKind))
		{
			Out.Base = B.Local(Row->Base);
			Out.Height = Row->Height;
			Out.Radius = Row->Radius;
			Out.Yaw = Row->Yaw - B.Act->GetActorRotation().Yaw;
			Out.Var = Row->Variant;
			Out.bRow = true;
		}
		else
		{
			Out.Base = B.At(0.f, 0.f);
			Out.Height = DefHeight;
			Out.Radius = WRForestShape::TrunkRadius(Species, DefHeight);
			Out.Var = WRTreeKit::VariantAt(Here);
		}
		Out.Var = ((Out.Var % 4) + 4) % 4;
		Out.Radius = FMath::Max(Out.Radius, 60.f);
		return Out;
	}

	/** Draws a giant of the kit and gives it the kit's trunk capsules (the build's own trunk box only reaches 25 m). */
	UStaticMeshComponent* Giant(FBuild& B, FName Species, const FLandTree& Tr, const FRotator& Rot)
	{
		UStaticMeshComponent* C = B.Tree(Species, Tr.Var, Tr.Base, Tr.Height, Rot, 0.f);
		const float Ref = FMath::Max(1.f, WRTreeKit::ReferenceHeight(Species));
		WRTreeKit::AddCollision(B.Act, Species, Tr.Var, FTransform(Rot, Tr.Base, FVector(Tr.Height / Ref)) * B.T);
		return C;
	}

	/** A named point of the kit's tree, in the actor's space (zero when the kit has none). */
	FVector AnchorAt(FName Species, const FLandTree& Tr, const FRotator& Rot, FName Point)
	{
		const FVector A0 = WRTreeKit::Anchor(Species, Tr.Var, Point);
		if (A0.IsNearlyZero()) return FVector::ZeroVector;
		const float Ref = FMath::Max(1.f, WRTreeKit::ReferenceHeight(Species));
		return Tr.Base + Rot.RotateVector(A0 * (Tr.Height / Ref));
	}

	/** Lights the tree's "Glow" strips (sap, foxfire). */
	void SetGlow(FBuild& B, UStaticMeshComponent* C, const FLinearColor& Tint, float Strength)
	{
		if (!C || B.bServer) return;
		if (UMaterialInstanceDynamic* Mi = WRTreeKit::GlowMaterial(B.Act, Tint, Strength)) C->SetMaterialByName(TEXT("Glow"), Mi);
	}

	FVector Dir2(float YawDeg) { return FRotator(0.f, YawDeg, 0.f).Vector(); }

	/** A tsuri-dōrō: the Lantern Mourners' hanging lantern, bronze gone green, hung from Top (its hook) and unlit (they
	 *  are lit only when the Mourners come: world-map). Tilt: how far it hangs out of true. */
	void HangLantern(FBuild& B, const FVector& Top, float Yaw, float Tilt)
	{
		const FRotator R(Tilt, Yaw, Tilt * 0.5f);
		const auto P = [&](float X, float Y, float Z) { return Top + R.RotateVector(FVector(X, Y, Z)); };
		const WRLookKit::FSurf Verdigris = WRLookKit::FSurf::Steel(WRForestLook::Rgb(78, 104, 86) * 0.5f, 0.7f);
		const WRLookKit::FSurf Dark = WRLookKit::FSurf::Steel(WRForestLook::Rgb(40, 44, 40) * 0.5f, 0.8f);
		B.K.Disc(Verdigris, P(0, 0, -4.f), R.RotateVector(FVector::RightVector), 16.f, 3.f);   // the hook ring
		B.K.Spike(Verdigris, P(0, 0, -30.f), R.RotateVector(FVector::UpVector), 12.f, 22.f);   // finial
		// The roof: a broad six-sided hat, its eaves turned up at the corners.
		B.K.Hex(Verdigris, float(P(0, 0, 0).X), float(P(0, 0, 0).Y), float(P(0, 0, -44.f).Z), float(P(0, 0, -34.f).Z), 36.f, Yaw);
		for (int32 c = 0; c < 6; ++c)
		{
			const FVector Corner = P(0, 0, -40.f) + R.RotateVector(FRotator(0.f, 60.f * c, 0.f).Vector() * 40.f);
			B.K.Spike(Verdigris, Corner, R.RotateVector(FRotator(30.f, 60.f * c, 0.f).Vector()), 7.f, 12.f);
		}
		// The light box: six posts, dark fretted panels between, the base plate and its little feet.
		for (int32 c = 0; c < 6; ++c)
		{
			const FVector Dir = R.RotateVector(FRotator(0.f, 60.f * c + 30.f, 0.f).Vector());
			B.K.Add(Verdigris, FVector(4.f, 4.f, 46.f), P(0, 0, -70.f) + Dir * 24.f, R);
			B.K.Add(Dark, FVector(3.f, 22.f, 38.f), P(0, 0, -70.f) + R.RotateVector(FRotator(0.f, 60.f * c, 0.f).Vector()) * 21.f, FRotator(R.Pitch, Yaw + 60.f * c, R.Roll));
		}
		B.K.Hex(Verdigris, float(P(0, 0, 0).X), float(P(0, 0, 0).Y), float(P(0, 0, -98.f).Z), float(P(0, 0, -92.f).Z), 28.f, Yaw);
		for (int32 c = 0; c < 3; ++c) B.K.Add(Verdigris, FVector(5.f, 5.f, 8.f), P(0, 0, -102.f) + R.RotateVector(FRotator(0.f, 120.f * c, 0.f).Vector() * 20.f), R);
	}

	/** The nearest point of a road (forest.json routes, kind road) to At within Range, and its direction there. */
	bool NearestRoad(const FVector& At, float Range, FVector& OutPoint, FVector& OutDir, float& OutWidth)
	{
		float Best = Range;
		bool bFound = false;
		for (const FWRForestData::FPath& Pa : FWRForestData::Get().Routes)
		{
			if (Pa.Kind != TEXT("road")) continue;
			for (int32 i = 0; i + 1 < Pa.Points.Num(); ++i)
			{
				const FVector C = FMath::ClosestPointOnSegment(At, Pa.Points[i], Pa.Points[i + 1]);
				const float Dd = FVector::Dist2D(C, At);
				if (Dd < Best)
				{
					Best = Dd;
					OutPoint = C;
					OutDir = (Pa.Points[i + 1] - Pa.Points[i]).GetSafeNormal2D();
					OutWidth = Pa.Width;
					bFound = true;
				}
			}
		}
		return bFound;
	}
}

// ================================================================================================ L2 the Eldest

void WRForestLook::FBuild::Eldest()
{
	// The oldest tree, 130 m, its crown broken off at 110 m by some old blow (DC 2.3 L2). It stands over the Hollow Root:
	// its Door (a crack at its foot) is the first way down, so the Door is left open here and the hazards track puts the
	// thornbrake in front of it. What it pays the eye: a shelf of bracket fungus 8 m round in a spiral band at 6-9 m (a
	// tree this old is being eaten from the inside: that is how the Hollow Root got hollow), and at night foxfire glowing
	// out of the cracks at its base: the mycelium that lines the rooms below shows at the surface.
	namespace Lm = WRForestLmImpl;
	const Lm::FLandTree Tr = Lm::FindTree(*this, TEXT("eldest"), TEXT("eldest"), WRTune(TEXT("forestWorld.eldestHeight"), 13000.f));
	const FRotator Rot(0.f, Tr.Yaw, 0.f);
	UStaticMeshComponent* Trunk = Lm::Giant(*this, TEXT("eldest"), Tr, Rot);
	Lm::SetGlow(*this, Trunk, Rgb(127, 224, 122), WRTune(TEXT("forestWorld.eldestGlow"), 2.5f));
	const float R = Tr.Radius;
	const FVector Base = Tr.Base;
	FVector Door = Lm::AnchorAt(TEXT("eldest"), Tr, Rot, TEXT("door"));
	if (Door.IsZero()) Door = Base + Rot.RotateVector(FVector(R * 1.05f, 0.f, 0.f));
	const float DoorYaw = (Door - Base).IsNearlyZero() ? Tr.Yaw : (Door - Base).GetSafeNormal2D().Rotation().Yaw;

	// The bracket fungus: shelves of a giant polypore climbing round the bole in a spiral from 6 m to 9 m, never over the
	// Door (a shelf there would roof the crack and hide it). Cream on top with a paler growing rim, rust-brown beneath;
	// two or three tiers per bracket. You can stand on the big ones (a perch to look down on the Door's thornbrake).
	const FSurf Cap = FSurf::Stone(Rgb(214, 198, 160) * 0.6f, 0.85f);
	const FSurf Rim = FSurf::Stone(Rgb(236, 226, 200) * 0.65f, 0.8f);
	const FSurf Under = FSurf::Stone(Rgb(120, 80, 50) * 0.45f, 0.95f);
	const int32 Shelves = 18;
	for (int32 i = 0; i < Shelves; ++i)
	{
		const float U = float(i) / (Shelves - 1);
		const float Ang = DoorYaw + 35.f + 290.f * U;
		const FVector Dir = Lm::Dir2(Ang);
		const float Z = Base.Z + WRTune(TEXT("forestWorld.eldestFungusLow"), 600.f) + 300.f * U + 40.f * Rand(i);
		const float Out = 180.f + 220.f * FMath::Sin(PI * U) + 60.f * Rand(i + 40);
		const float Wide = 300.f + 160.f * Rand(i + 80);
		const float TrunkR = R * 0.9f;
		for (int32 Tier = 0; Tier < 3; ++Tier)
		{
			const float Scale = 1.f - 0.28f * Tier;
			const FVector C = Base + Dir * (TrunkR + Out * 0.25f * Scale) + FVector(0.f, 0.f, Z - Base.Z + 55.f * Tier);
			const FVector Axis = (FVector::UpVector + Dir * 0.12f).GetSafeNormal();
			K.Disc(Under, C - FVector(0, 0, 12.f), Axis, Out * 1.35f * Scale, 10.f);
			K.Disc(Cap, C, Axis, Out * 1.3f * Scale, 26.f);
			K.Disc(Rim, C + Dir * (Out * 0.3f * Scale) + FVector(0, 0, 4.f), Axis, Out * 0.7f * Scale, 18.f);
		}
		Box(Base + Dir * (TrunkR + Out * 0.42f) + FVector(0, 0, Z - Base.Z + 10.f), FVector(Out * 0.42f, Wide * 0.3f, 14.f), FRotator(0.f, Ang, 0.f));
	}

	// Foxfire in the cracks between the buttresses (night only): thin green seams up the bark, glowing moss on the
	// ground at the root crown, and three low green lights so the base reads from a distance as a ring of embers.
	const FSurf Fox = Foxfire(1.6f);
	for (int32 i = 0; i < 14; ++i)
	{
		const float Ang = DoorYaw + 360.f * i / 14.f + 8.f * Rand(i + 120);
		const FVector Dir = Lm::Dir2(Ang);
		const float H = 140.f + 260.f * Rand(i + 140);
		const FVector At0 = Base + Dir * R * 1.12f;
		const float Gz = G(float(At0.X), float(At0.Y), float(Base.Z));
		N.Add(Fox, FVector(8.f, 20.f, H), FVector(At0.X, At0.Y, Gz + H * 0.5f), FRotator(0.f, Ang, 4.f * (Rand(i) - 0.5f)));
		N.Disc(Foxfire(1.1f), FVector(At0.X, At0.Y, Gz + 2.f) + Dir * 80.f, FVector::UpVector, 70.f + 60.f * Rand(i + 160), 3.f);
	}
	for (int32 i = 0; i < 3; ++i)
	{
		const FVector Dir = Lm::Dir2(DoorYaw + 120.f * i + 60.f);
		Lamp(Base + Dir * (R * 1.3f) + FVector(0, 0, 120.f), Rgb(127, 224, 122), WRTune(TEXT("forestWorld.eldestFoxfireLight"), 1800.f), 1000.f);
	}
	// The Door breathes a faint green out of the dark (the Hollow Root's foxfire inside), at night.
	Lamp(Door + Lm::Dir2(DoorYaw) * -60.f + FVector(0, 0, 150.f), Rgb(127, 224, 122), WRTune(TEXT("forestWorld.eldestDoorLight"), 1200.f), 600.f);

	// The ground of a tree this old: moss over the root crown, shed bark slabs leaning on the buttresses, a scatter of
	// old fungus shelves fallen from above.
	for (int32 i = 0; i < 12; ++i)
	{
		const FVector Dir = Lm::Dir2(DoorYaw + 30.f * i + 15.f * Rand(i + 200));
		const FVector P0 = Base + Dir * (R * 1.4f + 250.f * Rand(i + 220));
		const float Gz = G(float(P0.X), float(P0.Y), float(Base.Z));
		K.Add(DeepMoss(0.9f + 0.3f * Rand(i + 240)), FVector(260.f + 200.f * Rand(i), 180.f + 160.f * Rand(i + 1), 8.f), FVector(P0.X, P0.Y, Gz + 2.f), FRotator(0.f, DoorYaw + 30.f * i, 0.f));
		if (i % 3 == 1) K.Add(Bark(0.8f), FVector(14.f, 120.f, 260.f), FVector(P0.X, P0.Y, Gz + 110.f) - Dir * 60.f, FRotator(-18.f, DoorYaw + 30.f * i, 0.f));
		if (i % 4 == 2) K.Disc(Cap.Dim(0.8f), FVector(P0.X, P0.Y, Gz + 8.f) + Dir * 90.f, FVector(0.2f, 0.f, 1.f).GetSafeNormal(), 140.f, 16.f);
	}
}

// ================================================================================================ L3 the Great Falls

void WRForestLook::FBuild::GreatFalls()
{
	// The Alder drops 45 m off a basalt scarp into a round pool (DC 2.3 L3). The water sheet itself is the "fall" kind
	// (Fall() below, one per forest.json "falls" row). This draws what makes it read as the Great Falls: the columnar
	// basalt either side of the curtain (the scarp is old lava: the forest grows on the same island as the volcano), the
	// lip's overhang with the glowworm curtain under it (the grotto behind the falls, cave C2, is lit by them at night),
	// the pool's rim of mossy boulders (open where the river leaves), and the mist that hangs in the bowl. The Grovekin
	// fishing platform on stilts is the falls pool's site.
	namespace Lm = WRForestLmImpl;
	const FWRForestData& D = FWRForestData::Get();
	const FVector Here = Act->GetActorLocation();
	const FWRForestData::FFall* Best = nullptr;
	float BestD = WRTune(TEXT("forestWorld.greatFallsRange"), 8000.f);
	for (const FWRForestData::FFall& F : D.Falls)
	{
		const float Dd = FMath::Min(FVector::Dist2D(F.Top, Here), FVector::Dist2D(F.Bottom, Here));
		if (Dd < BestD) { BestD = Dd; Best = &F; }
	}
	FVector Top = FVector::ZeroVector, Bot = FVector::ZeroVector;
	float W = 1500.f;
	if (Best) { Top = Local(Best->Top); Bot = Local(Best->Bottom); W = Best->Width; }
	else { Bot = At(0.f, 0.f); Top = Bot + FVector(-1200.f, 0.f, 4500.f); W = 1500.f; }
	FVector Flow = (Bot - Top).GetSafeNormal2D();
	if (Flow.IsNearlyZero()) Flow = FVector::ForwardVector;
	const FVector Side = FVector::CrossProduct(Flow, FVector::UpVector).GetSafeNormal();
	const float FlowYaw = Flow.Rotation().Yaw;
	const float Drop = FMath::Max(500.f, float(Top.Z - Bot.Z));

	// Columnar basalt flanking the curtain: hexagonal columns standing from the pool's floor up the scarp, their tops
	// stepped like organ pipes, set just in front of the terrain face so the landscape's rock reads as basalt.
	for (const float S : { -1.f, 1.f })
	{
		for (int32 j = 0; j < 12; ++j)
		{
			const int32 Salt = j + (S > 0.f ? 0 : 50);
			const float Off = W * 0.55f + j * 150.f + 40.f * Rand(Salt);
			const float Rc = 70.f + 35.f * Rand(Salt + 7);
			const FVector C = Top + Side * S * Off - Flow * (80.f + 140.f * Rand(Salt + 11) + j * 25.f);
			const float Z0 = float(Bot.Z) - 300.f;
			const float Z1 = float(Top.Z) - Drop * (0.04f + 0.3f * Rand(Salt + 13)) - j * 60.f;
			if (Z1 - Z0 < 200.f) continue;
			K.Hex(Basalt(0.8f + 0.3f * Rand(Salt + 17)), float(C.X), float(C.Y), Z0, Z1, Rc, 60.f * Rand(Salt + 19));
			Box(FVector(C.X, C.Y, (Z0 + Z1) * 0.5f), FVector(Rc * 0.8f, Rc * 0.8f, (Z1 - Z0) * 0.5f));
		}
		// Tumbled column drums at the foot, lying in the shallows where they broke off.
		for (int32 j = 0; j < 5; ++j)
		{
			const int32 Salt = 100 + j + (S > 0.f ? 0 : 20);
			const FVector C = Bot + Side * S * (W * 0.6f + 250.f + 220.f * j) + Flow * (200.f + 300.f * Rand(Salt));
			const float Gz = G(float(C.X), float(C.Y), float(Bot.Z));
			K.Add(Basalt(0.75f), FVector(260.f + 120.f * Rand(Salt + 1), 130.f, 120.f), FVector(C.X, C.Y, Gz + 40.f), FRotator(10.f * (Rand(Salt + 2) - 0.5f), FlowYaw + 70.f * Rand(Salt + 3), 30.f));
			Box(FVector(C.X, C.Y, Gz + 40.f), FVector(140.f, 65.f, 60.f), FRotator(0.f, FlowYaw + 70.f * Rand(Salt + 3), 0.f));
		}
	}

	// The lip's overhang: a hard basalt cap the water has undercut (why falls retreat), jutting out over the grotto mouth.
	const FVector Lip = Top + Flow * 40.f - FVector(0, 0, 90.f);
	K.Add(Basalt(0.7f), FVector(520.f, W * 1.45f, 150.f), Lip - Flow * 120.f, FRotator(0.f, FlowYaw, 0.f));
	K.Add(Moss(0.9f), FVector(400.f, W * 1.2f, 10.f), Lip - Flow * 160.f + FVector(0, 0, 78.f), FRotator(0.f, FlowYaw, 0.f));
	Box(Lip - Flow * 120.f, FVector(260.f, W * 0.72f, 75.f), FRotator(0.f, FlowYaw, 0.f));

	// The glowworm curtain under the overhang (night only): hundreds of sticky silk threads, each with its blue-green
	// bead of light (Arachnocampa), hung from the dripping underside behind the water.
	const FSurf Glowworm = FSurf::Lit(Rgb(120, 210, 235) * 0.6f, 1.6f);
	for (int32 i = 0; i < 64; ++i)
	{
		const float Y = (Rand(i + 300) - 0.5f) * W * 1.35f;
		const float X = -260.f + 260.f * Rand(i + 400);
		const FVector Hang = Lip + Side * Y + Flow * X - FVector(0, 0, 76.f);
		const float Len = 30.f + 110.f * Rand(i + 500);
		N.Rod(Glowworm.Dim(0.5f), Hang, Hang - FVector(0, 0, Len), 0.8f);
		N.Add(Glowworm, FVector(4.f), Hang - FVector(0, 0, Len));
	}
	Lamp(Lip - Flow * 150.f - FVector(0, 0, 260.f), Rgb(120, 210, 235), WRTune(TEXT("forestWorld.glowwormLight"), 1400.f), 1600.f);

	// The pool's rim: mossy boulders round the plunge pool, open downstream where the Alder leaves it.
	const float PoolR = FMath::Max(W * 0.9f, WRTune(TEXT("forestWorld.greatFallsPool"), 1300.f));
	const FVector PoolC = Bot + Flow * PoolR * 0.4f;
	for (int32 i = 0; i < 18; ++i)
	{
		const float Ang = FlowYaw + 30.f + 300.f * i / 17.f;
		const FVector P0 = PoolC + Lm::Dir2(Ang) * (PoolR + 80.f * Rand(i + 600));
		const float Gz = G(float(P0.X), float(P0.Y), float(Bot.Z));
		const FVector Sz(200.f + 180.f * Rand(i + 610), 170.f + 130.f * Rand(i + 620), 110.f + 120.f * Rand(i + 630));
		Rock(i, Sz, FVector(P0.X, P0.Y, Gz), FRotator(0.f, Ang + 90.f * Rand(i + 640), 0.f), FLinearColor(0.07f, 0.085f, 0.06f), true, 30.f);
		if (i % 2 == 0) K.Add(Moss(1.f), FVector(Sz.X * 0.6f, Sz.Y * 0.5f, 6.f), FVector(P0.X, P0.Y, Gz + Sz.Z - 34.f), FRotator(0.f, Ang, 0.f));
	}
	Loop(TEXT("forest.falls_mist"), PoolC + FVector(0, 0, 300.f), FRotator(0.f, FlowYaw, 0.f), W / 1500.f);
}

// ================================================================================================ a waterfall's sheet

void WRForestLook::FBuild::Fall()
{
	// One forest.json "falls" row: the water from the lip (Extra) down the scarp face into the plunge pool (Extra2). The
	// sheet hugs the rock where the rock is (traced in from the open air at each height) but never comes in closer than
	// the arc a jet of water leaving the lip would draw, so it reads as falling, not running down a wall. The river
	// material's flow runs along U, so the water scrolls down the sheet. Two sheets: the main curtain and a thinner,
	// narrower one in front of it (the falls has depth). Spray and mist at the foot; foam on the pool. Walk-through (the
	// grotto is behind it); the push is the hazards track's spate.
	const FVector LipW = Act->Extra.IsNearlyZero() ? Act->GetActorLocation() + FVector(-1200.f, 0.f, 4500.f) : Act->Extra;
	const FVector PlungeW = Act->Extra2.IsNearlyZero() ? Act->GetActorLocation() : Act->Extra2;
	const FVector Lip = Local(LipW), Plunge = Local(PlungeW);
	const float Wd = Act->Size > 0.f ? Act->Size : 1500.f;
	FVector Flow = (Plunge - Lip).GetSafeNormal2D();
	if (Flow.IsNearlyZero()) Flow = FVector::ForwardVector;
	const FVector Side = FVector::CrossProduct(Flow, FVector::UpVector).GetSafeNormal();
	const float Drop = FMath::Max(200.f, float(Lip.Z - Plunge.Z));
	const FVector FlowW = T.TransformVectorNoScale(Flow);

	// The profile: (distance out from the lip along the flow, height).
	TArray<FVector> Prof;
	Prof.Add(Lip - Flow * 80.f);
	Prof.Add(Lip + Flow * 60.f - FVector(0, 0, 25.f));
	const float Throw = WRTune(TEXT("forestWorld.fallThrow"), 6.f);
	const float StandOff = WRTune(TEXT("forestWorld.fallStandOff"), 120.f);
	const int32 Steps = FMath::Clamp(FMath::CeilToInt(Drop / 300.f), 3, 40);
	float LastOut = 60.f;
	for (int32 i = 1; i <= Steps; ++i)
	{
		const float Z = FMath::Lerp(float(Lip.Z) - 60.f, float(Plunge.Z) - 30.f, float(i) / Steps);
		const float Fallen = float(Lip.Z) - Z;
		// A jet leaving the lip: out = v t, drop = g t^2 / 2, so out grows with the root of the drop.
		float Out = 60.f + Throw * FMath::Sqrt(FMath::Max(0.f, Fallen));
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRForestFall), false, Act);
		const FVector Probe = T.TransformPosition(FVector(Lip.X, Lip.Y, Z));
		if (Act->GetWorld()->LineTraceSingleByChannel(Hit, Probe + FlowW * 6000.f, Probe - FlowW * 2000.f, ECC_WorldStatic, Q))
		{
			const float RockOut = float(FVector::DotProduct(Local(Hit.ImpactPoint) - Lip, Flow));
			Out = FMath::Max(Out, RockOut + StandOff);
		}
		Out = FMath::Max(Out, LastOut);   // water doesn't fall back under itself
		LastOut = Out;
		const FVector P0 = Lip + Flow * Out;
		Prof.Add(FVector(P0.X, P0.Y, Z));
	}

	// Foam on the pool where the water hits, spreading downstream.
	const FSurf Foam = FSurf::Glaze(FLinearColor(0.7f, 0.74f, 0.76f), 0.6f);
	const FVector Hit0 = Prof.Last();
	for (int32 i = 0; i < 4; ++i)
	{
		K.Disc(Foam.Dim(1.f - 0.12f * i), FVector(Hit0.X, Hit0.Y, Plunge.Z + 4.f) + Flow * (90.f + 160.f * i) + Side * (Rand(i + 700) - 0.5f) * Wd * 0.3f,
			FVector::UpVector, Wd * (1.0f - 0.18f * i), 5.f);
	}
	Loop(TEXT("forest.falls_spray"), FVector(Hit0.X, Hit0.Y, Plunge.Z + 60.f) + Flow * 150.f, FRotator(0.f, Flow.Rotation().Yaw, 0.f), Wd / 1500.f);
	Loop(TEXT("forest.falls_mist"), FVector(Hit0.X, Hit0.Y, Plunge.Z + 250.f) + Flow * 700.f, FRotator(0.f, Flow.Rotation().Yaw, 0.f), Wd / 1500.f);
	// A white curtain in the moonlight (DC 2.3 L3 night read): a cool light in front of the water's middle.
	Lamp(FMath::Lerp(Lip, Hit0, 0.5f) + Flow * 600.f, FLinearColor(0.75f, 0.85f, 1.f), WRTune(TEXT("forestWorld.fallMoonLight"), 1500.f), Wd * 2.f);

	// The noon rainbow (11:00-13:00 on a clear day: UWRForestWorld::StepLights) stands in the spray of a big fall, its
	// arc across the flow, facing anyone looking up the river at the falls.
	if (Wd >= WRTune(TEXT("forestWorld.rainbowMinWidth"), 1200.f) || Drop >= WRTune(TEXT("forestWorld.rainbowMinDrop"), 3000.f))
	{
		Act->bRainbow = true;
		Act->RainbowAt = FTransform(FRotator(0.f, Flow.Rotation().Yaw, 0.f), FVector(Hit0.X, Hit0.Y, Plunge.Z + 350.f) + Flow * 500.f, FVector(Wd / 1500.f));
	}

	if (bServer) return;
	// The sheets: two faces each (the river material is one-sided), three columns across so the middle bows forward.
	TArray<FVector> Verts, Normals;
	TArray<int32> Tris;
	TArray<FVector2D> UVs;
	TArray<FColor> Cols;
	const float UvScale = WRTune(TEXT("forestWorld.fallUvScale"), 600.f);
	const auto Sheet = [&](float Forward, float WidthK)
	{
		for (int32 Face = 0; Face < 2; ++Face)
		{
			const int32 Start = Verts.Num();
			float Along = 0.f;
			for (int32 i = 0; i < Prof.Num(); ++i)
			{
				const FVector Prev = Prof[FMath::Max(0, i - 1)], Next = Prof[FMath::Min(Prof.Num() - 1, i + 1)];
				const FVector Tan = (Next - Prev).GetSafeNormal();
				FVector Nrm = FVector::CrossProduct(Side, Tan).GetSafeNormal();
				if (FVector::DotProduct(Nrm, Flow) < 0.f) Nrm = -Nrm;
				if (i > 0) Along += FVector::Dist(Prof[i], Prof[i - 1]);
				const float Wi = Wd * WidthK * FMath::Lerp(0.85f, 1.15f, float(i) / FMath::Max(1, Prof.Num() - 1));
				for (int32 c = 0; c < 3; ++c)
				{
					const float S = (c - 1) * 0.5f * Wi;
					const float Bow = c == 1 ? 25.f : 0.f;
					Verts.Add(Prof[i] + Side * S + Nrm * (Forward + Bow));
					Normals.Add(Face == 0 ? Nrm : -Nrm);
					UVs.Add(FVector2D(Along / UvScale, c * 0.5f));
					Cols.Add(FColor::White);
				}
			}
			for (int32 i = 0; i + 1 < Prof.Num(); ++i)
			{
				for (int32 c = 0; c < 2; ++c)
				{
					const int32 V0 = Start + i * 3 + c, V1 = V0 + 1, V2 = V0 + 3, V3 = V0 + 4;
					if (Face == 0) Tris.Append({ V0, V2, V1, V1, V2, V3 });
					else Tris.Append({ V0, V1, V2, V1, V3, V2 });
				}
			}
		}
	};
	Sheet(0.f, 1.f);
	Sheet(40.f, 0.6f);
	UProceduralMeshComponent* Water = NewObject<UProceduralMeshComponent>(Act);
	Water->SetupAttachment(Act->GetRootComponent());
	Water->RegisterComponent();
	Water->CreateMeshSection(0, Verts, Tris, Normals, UVs, Cols, TArray<FProcMeshTangent>(), false);
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, *FWRTuning::Get().Str(TEXT("forestWorld.fallMaterial"), TEXT("/Game/WaterMaterials/Materials/M_River.M_River")));
	if (!Base) Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/WaterMaterials/Materials/M_Lake.M_Lake"));
	if (Base) Water->SetMaterial(0, UMaterialInstanceDynamic::Create(Base, Act));
	Water->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Water->SetCastShadow(false);
	Water->SetCullDistance(0.f);
}

// ================================================================================================ L4 the Rotted Treant

void WRForestLook::FBuild::RottedTreant()
{
	// The Lantern Mourners' champion (world-map SPEC): a treant they raised and sent through Rootgrave, which fell on the
	// forest side, its roots rotted black where the forest refused it (DC 2.3 L4, 2.20 the pass war). It lies with its
	// head toward the pass and one arm still reaching west for it. The Mourners hung their tōrō lanterns from its dead
	// branches; they are unlit (they light only when the Mourners come). At night foxfire crawls over the black wood:
	// the forest is eating it.
	namespace Lm = WRForestLmImpl;
	Lm::FLandTree Tr = Lm::FindTree(*this, TEXT("treant"), TEXT("dead_giant"), WRTune(TEXT("forestWorld.treantLength"), 6000.f));
	const float R = Tr.Radius;
	// Toward the pass: the row's yaw if it has one, else due west.
	const float LieYaw = Tr.bRow ? Tr.Yaw : 180.f - Act->GetActorRotation().Yaw;
	const FRotator Rot(-84.f, LieYaw, 0.f);           // laid down along LieYaw, the crown propped 6 degrees on its limbs
	Tr.Base = Tr.Base + FVector(0, 0, R * 0.85f);       // the root plate tipped up, the bole resting on the ground
	UStaticMeshComponent* Body = Lm::Giant(*this, TEXT("dead_giant"), Tr, Rot);
	if (Body && !bServer)
	{
		// Rotted black (#1E1A16): the whole carcass darkened.
		if (UMaterialInterface* Mat = WRTreeKit::Material(false))
		{
			UMaterialInstanceDynamic* Mi = UMaterialInstanceDynamic::Create(Mat, Act);
			Mi->SetVectorParameterValue(TEXT("Tint"), FLinearColor(0.35f, 0.32f, 0.3f));
			Body->SetMaterialByName(TEXT("Bark"), Mi);
		}
	}
	const FVector Axis = Rot.RotateVector(FVector::UpVector);
	const FVector Dir = FVector(Axis.X, Axis.Y, 0.f).GetSafeNormal();
	const FVector Side = FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal();
	const FVector Up = FVector::UpVector;
	const auto Along = [&](float U) { return Tr.Base + Axis * (U * Tr.Height); };
	const FLinearColor Blk = BlackC();

	// The arm: a great limb heaved up from the upper bole, crooked at the elbow, the hand open toward the pass.
	{
		const FVector A0 = Along(0.62f) + Up * R * 0.4f + Side * R * 0.3f;
		const FVector A1 = A0 + Up * 650.f + Side * 280.f;
		const FVector A2 = A1 + Dir * 950.f + Up * 260.f;
		const FVector A3 = A2 + Dir * 800.f - Up * 220.f;
		Root({ A0, A1, A2, A3 }, R * 0.38f, R * 0.13f, Blk, 10);
		SegBox(A0, A1, R * 0.3f, R * 0.3f);
		SegBox(A1, A2, R * 0.25f, R * 0.25f);
		SegBox(A2, A3, R * 0.18f, R * 0.18f);
		for (int32 f = 0; f < 5; ++f)
		{
			const FVector FDir = (Dir + Side * (f - 2) * 0.35f - Up * (0.25f + 0.1f * Rand(f + 10))).GetSafeNormal();
			const FVector F1 = A3 + FDir * (240.f + 80.f * Rand(f + 20));
			Root({ A3, F1, F1 + (FDir - Up * 0.6f).GetSafeNormal() * 140.f }, R * 0.09f, R * 0.02f, Blk, 6);
		}
	}
	// The root plate standing on edge: black roots splayed in a disc round the base, soil still caught in them.
	for (int32 i = 0; i < 12; ++i)
	{
		const float Ang = 360.f * i / 12.f + 12.f * Rand(i + 30);
		const FVector Rd = (Side * FMath::Cos(FMath::DegreesToRadians(Ang)) + Up * FMath::Sin(FMath::DegreesToRadians(Ang))).GetSafeNormal();
		const FVector P1 = Tr.Base + Rd * R * 1.4f - Axis * 120.f;
		const FVector P2 = Tr.Base + Rd * R * (2.4f + 0.6f * Rand(i + 40)) - Axis * (260.f + 120.f * Rand(i + 50)) - Up * 60.f;
		Root({ Tr.Base - Axis * 40.f, P1, P2 }, R * 0.26f, R * 0.04f, Blk, 7);
		if (i % 3 == 0) K.Add(Earth(0.8f), FVector(120.f, 90.f, 70.f), P1 - Axis * 30.f, FRotator(Ang, LieYaw, 0.f));
	}
	Box(Tr.Base - Axis * 100.f, FVector(120.f, R * 1.6f, R * 1.6f), FRotator(0.f, LieYaw, 0.f));

	// Dead branches up off the bole, each with a Mourner lantern hung on a chain from its crook.
	const FLinearColor ChainC(0.045f, 0.042f, 0.04f);
	for (int32 i = 0; i < 6; ++i)
	{
		const float U = 0.28f + 0.11f * i + 0.04f * Rand(i + 60);
		const float S = (i % 2 ? 1.f : -1.f);
		const FVector B0 = Along(U) + Up * R * 0.75f + Side * S * R * 0.3f;
		const FVector B1 = B0 + Up * (380.f + 220.f * Rand(i + 70)) + Side * S * (160.f + 120.f * Rand(i + 80));
		const FVector B2 = B1 + Side * S * (180.f + 60.f * Rand(i + 90)) + Dir * (Rand(i + 100) - 0.5f) * 160.f + Up * 60.f;
		Root({ B0, B1, B2 }, R * 0.14f, R * 0.05f, Blk, 7);
		const float ChainLen = 120.f + 80.f * Rand(i + 110);
		const FVector Hook = B2 - Up * R * 0.04f;
		Line(Hook, Hook - Up * ChainLen, 1.8f, 0.f, ChainC, 4);
		WRForestLmImpl::HangLantern(*this, Hook - Up * ChainLen, LieYaw + 40.f * i, 6.f * (Rand(i + 120) - 0.5f));
	}

	// Foxfire crawling over the black wood (night): green seams along the top and flanks of the bole and the arm.
	const FSurf Fox = Foxfire(1.4f);
	for (int32 i = 0; i < 28; ++i)
	{
		const float U = 0.05f + 0.85f * Rand(i + 130);
		const float Ang = 20.f + 140.f * Rand(i + 140);
		const FVector Radial = (Side * FMath::Cos(FMath::DegreesToRadians(Ang)) + Up * FMath::Sin(FMath::DegreesToRadians(Ang))).GetSafeNormal();
		const float Taper = FMath::Lerp(1.f, 0.45f, U);
		const FVector P0 = Along(U) + Radial * R * Taper * 1.02f;
		N.Add(Fox, FVector(120.f + 200.f * Rand(i + 150), 16.f, 5.f), P0, FRotationMatrix::MakeFromXZ(Axis, Radial).Rotator());
	}
	for (int32 i = 0; i < 3; ++i) Lamp(Along(0.2f + 0.3f * i) + Up * R * 1.4f, Rgb(127, 224, 122), WRTune(TEXT("forestWorld.treantFoxfireLight"), 1200.f), 800.f);
}

// ================================================================================================ L8 the Hermit's Oak

void WRForestLook::FBuild::HermitsOak()
{
	// The path master's home (DC 2.3 L8, 2.11, 2.20): a living oak with a Grovekin round house in its fork 12 m up, a
	// spiral stair of planks pegged round the trunk, smoke from a clay chimney. Rowan Ashby moved into a house the
	// Grovekin grew and left; his shelves hold their carvings and the one thing he brought off his ship, its lantern. His
	// herb garden at the foot copies the Herb Terraces (he learned from their gardens). Night read: warm window light and
	// lanterns down the stair. Rowan's night spot (WRForestShape::HermitHouseFloor) is on this floor, kept clear.
	namespace Lm = WRForestLmImpl;
	const FWRForestData::FSite* Site = FWRForestData::Get().FindSite(Act->SiteId);
	FWRForestData::FSite Fallback;
	Fallback.At = Act->GetActorLocation();
	Fallback.Yaw = Act->GetActorRotation().Yaw;
	const FWRForestData::FSite& SiteRow = Site ? *Site : Fallback;
	float H = 0.f, R = 0.f;
	WRForestShape::HermitOak(SiteRow, H, R);
	const FWRForestData::FTree* Row = WRForestShape::GiantNear(SiteRow.At, 1500.f, TEXT("hermit_oak"));
	Lm::FLandTree Tr;
	Tr.Base = Local(Row ? Row->Base : SiteRow.At);
	Tr.Height = H;
	Tr.Radius = R;
	Tr.Var = Row ? ((Row->Variant % 4) + 4) % 4 : WRTreeKit::VariantAt(SiteRow.At);
	Lm::Giant(*this, TEXT("hermit_oak"), Tr, FRotator::ZeroRotator);
	const FVector Base = Tr.Base;
	const float FloorZ = float(Base.Z) + WRTune(TEXT("forestWorld.hermitHouseZ"), 1200.f);
	const float Out = WRTune(TEXT("forestWorld.hermitHouseOut"), 380.f);
	const float NotchHalf = 38.f;
	const float HouseYaw = 150.f;                       // the stair's hatch at 150 degrees, the door at 200; Rowan's spot near -30
	GroveHouse(Base, R, FloorZ, HouseYaw, Out, true, false, NotchHalf);
	const auto Ring = [&](float AngDeg, float Rad, float Z) { return Base + Lm::Dir2(AngDeg) * Rad + FVector(0, 0, Z - Base.Z); };

	// The stair: planks pegged into the bark, climbing clockwise round the trunk; the last eleven steep steps come up
	// through the hatch in the floor (the turn below them passes 2.4 m under the floor, clear of a head).
	const float Rise = 20.f;
	const float StairZ0 = G(float(Base.X + R + 150.f), float(Base.Y), float(Base.Z));
	const int32 Steps = FMath::Max(12, FMath::CeilToInt((FloorZ - StairZ0) / Rise));
	const int32 Final = 11;
	const float HatchA0 = HouseYaw - NotchHalf + 4.f, HatchA1 = HouseYaw + NotchHalf - 4.f;
	const float SpiralStep = FMath::RadiansToDegrees(95.f / (R + 60.f));
	TArray<FVector> RailPts;
	FVector LastPost = FVector::ZeroVector;
	for (int32 i = 0; i < Steps; ++i)
	{
		const bool bHatch = i >= Steps - Final;
		const float Ang = bHatch ? HatchA0 + (HatchA1 - HatchA0) * float(i - (Steps - Final)) / (Final - 1) : HatchA0 - float(Steps - Final - i) * SpiralStep;
		const float TopZ = FMath::Min(FloorZ, StairZ0 + (i + 1) * Rise);
		const float Inner = R * 0.96f, Outer = bHatch ? R + Out - 60.f : R + 115.f;
		const float Mid = (Inner + Outer) * 0.5f;
		const float Wide = FMath::DegreesToRadians(bHatch ? (HatchA1 - HatchA0) / (Final - 1) : SpiralStep) * Mid * 1.12f;
		const FRotator Face(0.f, Ang, 0.f);
		K.Add(Plank(0.85f + 0.3f * Rand(i + 900)), FVector(Outer - Inner, Wide, 7.f), Ring(Ang, Mid, TopZ - 3.5f), Face);
		Box(Ring(Ang, Mid, TopZ - 5.f), FVector((Outer - Inner) * 0.5f, Wide * 0.5f, 5.f), Face);
		// The pegs: two oak trenails driven into the bark under each tread.
		if (!bHatch)
		{
			for (const float Off : { -0.25f, 0.25f })
			{
				K.Rod(Timber(0.8f), Ring(Ang + Off * SpiralStep, R * 0.8f, TopZ - 12.f), Ring(Ang + Off * SpiralStep, R + 70.f, TopZ - 12.f), 6.f);
			}
		}
		// Rail posts on the open side every third step, a rope run through their tops; collision so no one walks off.
		if (i % 3 == 0)
		{
			const FVector Post = Ring(Ang, Outer - 6.f, TopZ);
			K.Rod(Timber(), Post, Post + FVector(0, 0, 100.f), 9.f);
			RailPts.Add(Post + FVector(0, 0, 95.f));
			if (!LastPost.IsZero() && !bHatch) SegBox(LastPost + FVector(0, 0, 50.f), Post + FVector(0, 0, 50.f), 3.f, 50.f);
			LastPost = Post;
			// A lantern on every twelfth step's post, lit at night: the stair reads as a spiral of warm lights.
			if (i % 12 == 0 && i > 0)
			{
				const FVector L0 = Post + FVector(0, 0, 112.f);
				K.Add(Bronze(), FVector(18.f, 18.f, 4.f), L0 + FVector(0, 0, 16.f));
				K.Add(Bronze(), FVector(14.f, 14.f, 4.f), L0 - FVector(0, 0, 16.f));
				N.Add(Window(0.95f), FVector(12.f, 12.f, 26.f), L0);
				Lamp(L0, FLinearColor(1.f, 0.6f, 0.28f), WRTune(TEXT("forestWorld.stairLantern"), 900.f), 600.f);
			}
		}
	}
	if (RailPts.Num() > 1) M.Tube(RailPts, 2.4f, 5, RopeC(), 0.95f);

	// Inside: the clay hearth and chimney (at 60 degrees, against the wall), up through the moss roof, smoking.
	{
		const float Ang = HouseYaw - 90.f;              // 60: clear of the door (200) and the night spot (about -30)
		const float Rad = R + Out - 70.f;
		const FRotator Face(0.f, Ang, 0.f);
		K.Add(Clay(), FVector(110.f, 130.f, 70.f), Ring(Ang, Rad, FloorZ + 35.f), Face);
		K.Add(Black(0.6f), FVector(60.f, 80.f, 40.f), Ring(Ang, Rad - 40.f, FloorZ + 50.f), Face);   // the fire mouth
		const float RoofOut = R + Out + 70.f, RoofIn = R + 10.f;
		const float ZOut = FloorZ + 230.f - 20.f, ZIn = FloorZ + 230.f + 170.f;
		const float RoofZ = ZOut + (ZIn - ZOut) * (RoofOut - Rad) / (RoofOut - RoofIn);
		for (float Z = FloorZ + 70.f; Z < RoofZ + 120.f; Z += 30.f) K.Add(Clay().Dim(0.9f + 0.2f * FMath::Frac(Z * 0.013f)), FVector(56.f, 56.f, 30.f), Ring(Ang, Rad, Z + 15.f), Face);
		Loop(TEXT("forest.chimney"), Ring(Ang, Rad, RoofZ + 150.f));
		Box(Ring(Ang, Rad, FloorZ + 35.f), FVector(55.f, 65.f, 35.f), Face);
	}
	// The window: a lit square in the woven wall facing the meadow side (the warm light walkers see), its shutter open.
	{
		const float Ang = HouseYaw + 120.f;             // 270
		const FRotator Face(0.f, Ang, 0.f);
		N.Add(Window(0.9f), FVector(6.f, 70.f, 55.f), Ring(Ang, R + Out + 6.f, FloorZ + 140.f), Face);
		K.Add(Timber(0.85f), FVector(8.f, 80.f, 8.f), Ring(Ang, R + Out + 8.f, FloorZ + 172.f), Face);
		K.Add(Timber(0.85f), FVector(8.f, 80.f, 8.f), Ring(Ang, R + Out + 8.f, FloorZ + 108.f), Face);
		K.Add(Bark(0.9f), FVector(4.f, 40.f, 60.f), Ring(Ang - 7.f, R + Out + 30.f, FloorZ + 140.f), FRotator(0.f, Ang - 60.f, 0.f));
	}
	// His shelves (at 90-120 degrees): Grovekin carvings he found (small hulder figures, a bark-backed bowl, a carved
	// loaf) and the newcomer's ship lantern that's the one thing of his own.
	{
		const float Ang = HouseYaw - 45.f;              // 105
		const FRotator Face(0.f, Ang + 90.f, 0.f);
		for (int32 s = 0; s < 3; ++s)
		{
			const float Z = FloorZ + 70.f + 50.f * s;
			K.Add(Plank(0.9f), FVector(26.f, 140.f, 5.f), Ring(Ang, R + Out - 20.f, Z), FRotator(0.f, Ang, 0.f));
			for (int32 c = 0; c < 3; ++c)
			{
				const FVector Spot = Ring(Ang + (c - 1) * 6.f, R + Out - 22.f, Z + 3.f);
				if ((s + c) % 3 == 0) { Limb(Spot, Spot + FVector(0, 0, 26.f), 5.f, 3.f, PaleWoodC(), 8); Ball(Spot + FVector(0, 0, 30.f), 4.f, PaleWoodC()); }
				else if ((s + c) % 3 == 1) K.Disc(Carved(0.8f), Spot + FVector(0, 0, 5.f), FVector::UpVector, 22.f, 10.f);
				else Ball(Spot + FVector(0, 0, 7.f), 8.f, Rgb(176, 140, 96) * 0.55f, 0.8f, 0.6f);
			}
		}
		// The ship lantern hangs from a rafter beside the shelves: brass, four glass sides, a bail; lit at night.
		const FVector Hang = Ring(Ang - 18.f, R + 120.f, FloorZ + 205.f);
		Line(Hang + FVector(0, 0, 40.f), Hang + FVector(0, 0, 12.f), 1.2f, 0.f, Rgb(140, 106, 58) * 0.6f, 2);
		K.Add(Bronze(), FVector(24.f, 24.f, 5.f), Hang + FVector(0, 0, 10.f));
		for (int32 c = 0; c < 4; ++c) K.Add(Bronze(), FVector(3.f, 3.f, 28.f), Hang + FVector(c % 2 ? 10.f : -10.f, c < 2 ? -10.f : 10.f, -6.f));
		K.Add(Bronze(), FVector(24.f, 24.f, 5.f), Hang - FVector(0, 0, 22.f));
		N.Add(Window(1.f), FVector(16.f, 16.f, 22.f), Hang - FVector(0, 0, 6.f));
		Lamp(Hang - FVector(0, 0, 6.f), FLinearColor(1.f, 0.62f, 0.3f), WRTune(TEXT("forestWorld.shipLantern"), 700.f), 500.f);
	}
	// A bough bed with hides (at 250 degrees), herb bundles drying from the rafters, a table with his resin mortar.
	{
		const float Ang = HouseYaw + 100.f;
		const FRotator Face(0.f, Ang + 90.f, 0.f);
		K.Add(Timber(0.9f), FVector(200.f, 90.f, 30.f), Ring(Ang, R + Out - 70.f, FloorZ + 15.f), Face);
		K.Add(Hide(1.f), FVector(190.f, 84.f, 10.f), Ring(Ang, R + Out - 70.f, FloorZ + 35.f), Face);
		Box(Ring(Ang, R + Out - 70.f, FloorZ + 20.f), FVector(100.f, 45.f, 20.f), Face);
		for (int32 b = 0; b < 6; ++b)
		{
			const FVector Herb = Ring(HouseYaw + 60.f + 18.f * b, R + Out * 0.5f, FloorZ + 210.f);
			Line(Herb + FVector(0, 0, 20.f), Herb, 0.8f, 0.f, RopeC(), 2);
			K.Add(Leaf(0.8f + 0.1f * (b % 3)), FVector(12.f, 12.f, 34.f), Herb - FVector(0, 0, 16.f));
		}
		const float TA = HouseYaw - 110.f;              // 40: between the hearth and the night spot, against the wall
		K.Add(Plank(0.8f), FVector(70.f, 120.f, 6.f), Ring(TA, R + Out - 60.f, FloorZ + 75.f), FRotator(0.f, TA, 0.f));
		for (int32 c = 0; c < 4; ++c) K.Rod(Timber(0.8f), Ring(TA + (c < 2 ? -6.f : 6.f), R + Out - 60.f + (c % 2 ? 25.f : -25.f), FloorZ), Ring(TA + (c < 2 ? -6.f : 6.f), R + Out - 60.f + (c % 2 ? 25.f : -25.f), FloorZ + 72.f), 6.f);
		K.Disc(OldStone(0.8f), Ring(TA, R + Out - 60.f, FloorZ + 86.f), FVector::UpVector, 24.f, 14.f);
		K.Disc(Amber(0.3f), Ring(TA, R + Out - 60.f, FloorZ + 94.f), FVector::UpVector, 14.f, 3.f);   // ground resin in the mortar
		Box(Ring(TA, R + Out - 60.f, FloorZ + 40.f), FVector(35.f, 60.f, 40.f), FRotator(0.f, TA, 0.f));
	}

	// A hulder post at the stair's foot (the Grovekin carved one at every gate), facing out to the meadow path.
	{
		const float Ang = HatchA0 - float(Steps - Final) * SpiralStep - 20.f;
		const FVector Foot = Ring(Ang, R + 260.f, 0.f);
		Hulder(FVector(Foot.X, Foot.Y, G(float(Foot.X), float(Foot.Y), float(Base.Z))), Ang);
	}
	// His herb garden at the foot (copied from the Herb Terraces): three stone-edged beds stepped down the slope, herbs in
	// rows, a straw skep on a stone stand, a wattle fence against the deer.
	{
		const FVector Dir = Lm::Dir2(-60.f);
		const FVector Sd = Lm::Dir2(30.f);
		for (int32 b = 0; b < 3; ++b)
		{
			const FVector C = Base + Dir * (R + 600.f + 260.f * b);
			const float Gz = G(float(C.X), float(C.Y), float(Base.Z));
			const FRotator Face(0.f, -60.f, 0.f);
			for (const float S : { -1.f, 1.f })
			{
				K.Add(OldStone(0.85f), FVector(220.f, 20.f, 40.f), FVector(C.X, C.Y, Gz + 10.f) + Sd * S * 140.f, Face);
				K.Add(OldStone(0.8f), FVector(20.f, 300.f, 40.f), FVector(C.X, C.Y, Gz + 10.f) + Dir * S * 110.f, Face);
			}
			K.Add(Earth(0.9f), FVector(210.f, 270.f, 20.f), FVector(C.X, C.Y, Gz + 18.f), Face);
			for (int32 h = 0; h < 8; ++h)
			{
				const FVector Pl = FVector(C.X, C.Y, Gz + 28.f) + Dir * ((h % 2) * 80.f - 40.f) + Sd * ((h / 2) * 60.f - 90.f);
				const FSurf Herb = (h + b) % 4 == 0 ? FSurf::Cloth(Rgb(150, 120, 190) * 0.5f) : (h + b) % 4 == 1 ? FSurf::Cloth(Rgb(210, 190, 70) * 0.5f) : Leaf(0.9f + 0.1f * (h % 3));
				K.Add(Herb, FVector(34.f, 34.f, 26.f + 10.f * Rand(h + b * 10)), Pl);
			}
		}
		const FVector SkepAt = Base + Dir * (R + 500.f) + Sd * 300.f;
		const float Gz = G(float(SkepAt.X), float(SkepAt.Y), float(Base.Z));
		K.Add(OldStone(0.8f), FVector(60.f, 60.f, 50.f), FVector(SkepAt.X, SkepAt.Y, Gz + 25.f));
		M.Lathe(FVector(SkepAt.X, SkepAt.Y, Gz + 50.f), FVector::UpVector, 55.f, { FVector(0.f, 30.f, 30.f), FVector(0.4f, 29.f, 29.f), FVector(0.75f, 22.f, 22.f), FVector(1.f, 4.f, 4.f) }, 12, Rgb(201, 168, 90) * 0.5f, 0.95f);
		for (int32 f = 0; f < 14; ++f)
		{
			const FVector Stake = Base + Dir * (R + 400.f + 70.f * f) + Sd * 220.f;
			const float Sz = G(float(Stake.X), float(Stake.Y), float(Base.Z));
			K.Rod(Bark(0.9f), FVector(Stake.X, Stake.Y, Sz - 10.f), FVector(Stake.X, Stake.Y, Sz + 90.f), 6.f);
		}
		const FVector F0 = Base + Dir * (R + 400.f) + Sd * 220.f, F1 = Base + Dir * (R + 1310.f) + Sd * 220.f;
		for (const float Z : { 30.f, 60.f, 85.f }) Line(F0 + FVector(0, 0, G(float(F0.X), float(F0.Y), float(Base.Z)) - Base.Z + Z), F1 + FVector(0, 0, G(float(F1.X), float(F1.Y), float(Base.Z)) - Base.Z + Z), 2.5f, 6.f, RootC(), 10);
	}
}

// ================================================================================================ L9 the Strangler Arch

void WRForestLook::FBuild::StranglerArch()
{
	// A strangler-fig tower 60 m tall whose host rotted away (DC 2.3 L9, 2.20: the same hollow lattice as Sylvaroth's
	// body, a warning the player can climb). The kit's strangler carries the spiral root ramp inside and the vista deck on
	// top as walkable collision. Its lower roots arch over the main village road: the road was always meant to pass under
	// it (the Grovekin trained the roots over it). Night read: foxfire inside the lattice, like a lantern of roots.
	namespace Lm = WRForestLmImpl;
	const Lm::FLandTree Tr = Lm::FindTree(*this, TEXT("strangler"), TEXT("strangler"), WRTune(TEXT("forestWorld.stranglerHeight"), 6000.f));
	const FRotator Rot(0.f, Tr.Yaw, 0.f);
	UStaticMeshComponent* Body = Lm::Giant(*this, TEXT("strangler"), Tr, Rot);
	Lm::SetGlow(*this, Body, Rgb(127, 224, 122), WRTune(TEXT("forestWorld.stranglerGlow"), 1.5f));
	const float R = Tr.Radius;
	// If the kit gives the lattice no ramp of its own (only its columns), a spiral ramp of root boxes inside it.
	if (WRTreeKit::Colliders(TEXT("strangler"), Tr.Var).Num() < 8)
	{
		const float RampR = R * 0.55f;
		const float Climb = Tr.Height * 0.9f;
		const int32 Segs = FMath::Max(24, FMath::RoundToInt(Climb / 60.f));
		for (int32 i = 0; i < Segs; ++i)
		{
			const float A0 = Tr.Yaw + i * 22.f, A1 = Tr.Yaw + (i + 1) * 22.f;
			const FVector P0 = Tr.Base + Lm::Dir2(A0) * RampR + FVector(0, 0, Climb * i / Segs);
			const FVector P1 = Tr.Base + Lm::Dir2(A1) * RampR + FVector(0, 0, Climb * (i + 1) / Segs);
			SegBox(P0, P1, 70.f, 8.f);
		}
		Box(Tr.Base + FVector(0, 0, Climb + 10.f), FVector(RampR + 60.f, RampR + 60.f, 10.f));
	}
	// The vista: a rope rail round the top deck and a carved Grovekin marker post pointing at the Four Hearts.
	FVector Vista = Lm::AnchorAt(TEXT("strangler"), Tr, Rot, TEXT("vista"));
	if (Vista.IsZero()) Vista = Tr.Base + FVector(0, 0, Tr.Height * 0.9f + 20.f);
	{
		const float RailR = FMath::Max(140.f, R * 0.6f);
		TArray<FVector> Rail;
		for (int32 i = 0; i <= 16; ++i) Rail.Add(Vista + Lm::Dir2(22.5f * i) * RailR + FVector(0, 0, 100.f));
		M.Tube(Rail, 2.4f, 5, RopeC(), 0.95f);
		for (int32 i = 0; i < 8; ++i) K.Rod(Timber(0.85f), Vista + Lm::Dir2(45.f * i) * RailR, Vista + Lm::Dir2(45.f * i) * RailR + FVector(0, 0, 105.f), 8.f);
		const FWRForestData::FArena* Hearts = FWRForestData::Get().Arenas.Find(FName(TEXT("sylvaroth")));
		const float PointYaw = Hearts ? (Local(Hearts->Centre) - Vista).GetSafeNormal2D().Rotation().Yaw : 180.f;
		const FVector Post = Vista + Lm::Dir2(PointYaw) * (RailR - 30.f);
		K.Rod(Carved(), Post, Post + FVector(0, 0, 150.f), 14.f, true);
		K.Add(Carved(0.85f), FVector(70.f, 10.f, 14.f), Post + FVector(0, 0, 140.f) + Lm::Dir2(PointYaw) * 30.f, FRotator(0.f, PointYaw, 0.f));
		for (int32 i = 0; i < 8; ++i) Box(Vista + Lm::Dir2(45.f * i + 22.5f) * RailR + FVector(0, 0, 50.f), FVector(4.f, RailR * 0.39f, 50.f), FRotator(0.f, 45.f * i + 22.5f, 0.f));
	}

	// The root arches over the village road: three living roots springing from the tower's foot across the road.
	{
		FVector RoadAt, RoadDir;
		float RoadW = 600.f;
		FVector Across;
		FVector Mid;
		if (Lm::NearestRoad(T.TransformPosition(Tr.Base), WRTune(TEXT("forestWorld.stranglerRoadRange"), 4000.f), RoadAt, RoadDir, RoadW))
		{
			Mid = Local(RoadAt);
			const FVector LocalDir = T.InverseTransformVectorNoScale(RoadDir);
			Across = FVector::CrossProduct(LocalDir, FVector::UpVector).GetSafeNormal();
		}
		else
		{
			Across = Lm::Dir2(Tr.Yaw + 90.f);
			Mid = Tr.Base + Lm::Dir2(Tr.Yaw) * (R + 700.f);
		}
		const FVector Along = FVector::CrossProduct(FVector::UpVector, Across).GetSafeNormal();
		const float Span = RoadW + WRTune(TEXT("forestWorld.stranglerArchSpare"), 700.f);
		for (int32 i = 0; i < 3; ++i)
		{
			const FVector C = Mid + Along * (i - 1) * 380.f;
			const FVector E0 = C - Across * Span * 0.5f, E1 = C + Across * Span * 0.5f;
			const FVector G0(E0.X, E0.Y, G(float(E0.X), float(E0.Y), float(C.Z)) - 30.f), G1(E1.X, E1.Y, G(float(E1.X), float(E1.Y), float(C.Z)) - 30.f);
			const float Girth = (70.f + 25.f * Rand(i + 20)) * FMath::Clamp(Tr.Height / 6000.f, 0.7f, 1.5f);
			Log(TEXT("root_arch"), (Tr.Var + i) % 4, G0, G1, Girth, false);
			// The feet are solid where they enter the ground; the arch itself is high over the road's head room.
			Box(G0 + FVector(0, 0, 80.f), FVector(Girth, Girth, 90.f));
			Box(G1 + FVector(0, 0, 80.f), FVector(Girth, Girth, 90.f));
		}
	}

	// Foxfire inside the lattice (night): green seams up the inner columns all the way up the ramp, four low lights.
	const FSurf Fox = Foxfire(1.5f);
	for (int32 i = 0; i < 40; ++i)
	{
		const float Ang = Tr.Yaw + 360.f * Rand(i + 300);
		const float Z = Tr.Height * 0.85f * Rand(i + 310);
		N.Add(Fox, FVector(8.f, 18.f, 80.f + 160.f * Rand(i + 320)), Tr.Base + Lm::Dir2(Ang) * R * 0.78f + FVector(0, 0, Z), FRotator(0.f, Ang, 10.f * (Rand(i) - 0.5f)));
	}
	for (int32 i = 0; i < 4; ++i) Lamp(Tr.Base + FVector(0, 0, 300.f + Tr.Height * 0.22f * i), Rgb(127, 224, 122), WRTune(TEXT("forestWorld.stranglerFoxfireLight"), 2200.f), R * 2.2f);
}

// ================================================================================================ L10 the Antler Barrow

void WRForestLook::FBuild::AntlerBarrow()
{
	// A long barrow on a meadow hill (DC 2.3 L10, 2.4 #9): the Grovekin hunters' tomb, its door between standing stones
	// facing +X, the mound running back along -X, an elk skull 6 m across its antlers set on top (its pale outline against
	// the sky is the night read; the Hornwarden's route starts here and he rests on it at noon). Inside, a passage to a
	// burial chamber: a hunter laid out on a bier of antlers with an antler crown and a bow of heartwood (the barrow's
	// cache lands on the bier beside him: UWRForestWorld places it at local (-1500, 0) by tracing down). Enclosed, walkable,
	// dark: daylight from the door is all there is.
	namespace Lm = WRForestLmImpl;
	const float FloorZ = FMath::Clamp(G(220.f, 0.f, 0.f), -40.f, 0.f);
	const float Len = WRTune(TEXT("forestWorld.barrowLength"), 5000.f);
	const auto Height = [](float X, float L) { return FMath::Lerp(480.f, 260.f, FMath::Clamp(-X / L, 0.f, 1.f)); };
	const auto HalfW = [](float X, float L) { return FMath::Lerp(820.f, 600.f, FMath::Clamp(-X / L, 0.f, 1.f)); };

	// The mound: a turf shell lofted slice by slice over whatever the ground does, closed under the ground's line.
	{
		const int32 Slices = 14;
		TArray<TArray<FVector>> Rings;
		for (int32 s = 0; s <= Slices; ++s)
		{
			const float X = 150.f - (Len + 150.f) * s / Slices;
			const float Hh = Height(X, Len) * (s == 0 || s == Slices ? 0.35f : 1.f);
			const float Ww = HalfW(X, Len) * (s == Slices ? 0.6f : 1.f);
			TArray<FVector> Ring;
			Ring.Add(FVector(X, Ww, G(X, Ww, FloorZ) - 120.f));
			for (int32 k = 0; k <= 12; ++k)
			{
				const float Ang = PI * k / 12.f;
				const float Y = Ww * FMath::Cos(Ang);
				const float Gz = G(X, Y, FloorZ);
				Ring.Add(FVector(X, Y, Gz + Hh * FMath::Pow(FMath::Sin(Ang), 0.7f)));
			}
			Ring.Add(FVector(X, -Ww, G(X, -Ww, FloorZ) - 120.f));
			Rings.Add(Ring);
		}
		for (int32 s = 0; s + 1 < Rings.Num(); ++s) M.Loft({ Rings[s], Rings[s + 1] }, Rgb(78, 104, 46) * 0.45f, 0.95f, s == 0 || s + 2 == Rings.Num());
		// Its collision: solid either side of the passage and over its roof, solid all through behind the chamber.
		for (float X = 0.f; X > -Len; X -= 500.f)
		{
			const float Xc = X - 250.f;
			const float Hh = Height(Xc, Len), Ww = HalfW(Xc, Len) * 0.9f;
			const float Gz = G(Xc, 0.f, FloorZ);
			const bool bHollow = Xc > -2200.f;
			const float Inner = Xc > -1050.f ? 150.f : 340.f;
			if (bHollow)
			{
				for (const float S : { -1.f, 1.f }) Box(FVector(Xc, S * (Inner + Ww) * 0.5f, Gz + Hh * 0.4f), FVector(250.f, (Ww - Inner) * 0.5f, Hh * 0.4f));
				Box(FVector(Xc, 0.f, FloorZ + 290.f + (Gz + Hh * 0.85f - FloorZ - 290.f) * 0.5f), FVector(250.f, Inner, FMath::Max(10.f, (Gz + Hh * 0.85f - FloorZ - 290.f) * 0.5f)));
			}
			else Box(FVector(Xc, 0.f, Gz + Hh * 0.4f), FVector(250.f, Ww, Hh * 0.4f));
		}
	}

	// The passage (0..-1000, 2.2 m wide) and the chamber (-1000..-2100, 6 m wide): flagged floor, orthostat walls,
	// capstone roof; all solid.
	const FSurf Wall = OldStone(0.75f);
	K.Add(OldStone(0.6f), FVector(2100.f, 600.f, 20.f), FVector(-1050.f, 0.f, FloorZ - 10.f));
	for (int32 f = 0; f < 18; ++f) K.Add(OldStone(0.55f + 0.2f * Rand(f + 10)), FVector(110.f, 90.f + 60.f * Rand(f), 4.f), FVector(-60.f - 115.f * f, (Rand(f + 30) - 0.5f) * 80.f, FloorZ + 1.f), FRotator(0.f, 20.f * Rand(f + 40), 0.f));
	Box(FVector(-1050.f, 0.f, FloorZ - 10.f), FVector(1050.f, 300.f, 10.f));
	for (const float S : { -1.f, 1.f })
	{
		for (int32 o = 0; o < 7; ++o) K.Add(Wall.Dim(0.85f + 0.3f * Rand(o + (S > 0 ? 0 : 20))), FVector(140.f, 34.f, 250.f + 30.f * Rand(o + 50)), FVector(-75.f - 145.f * o, S * 128.f, FloorZ + 120.f), FRotator(0.f, 0.f, S * 3.f));
		Box(FVector(-500.f, S * 130.f, FloorZ + 140.f), FVector(500.f, 18.f, 140.f));
		for (int32 o = 0; o < 8; ++o) K.Add(Wall.Dim(0.8f + 0.3f * Rand(o + 70 + (S > 0 ? 0 : 20))), FVector(140.f, 40.f, 280.f), FVector(-1070.f - 140.f * o, S * 318.f, FloorZ + 130.f), FRotator(0.f, 0.f, S * 2.f));
		Box(FVector(-1550.f, S * 320.f, FloorZ + 150.f), FVector(550.f, 20.f, 150.f));
		K.Add(Wall, FVector(40.f, 190.f, 280.f), FVector(-1000.f, S * 225.f, FloorZ + 130.f));   // the step out from passage to chamber
		Box(FVector(-1000.f, S * 225.f, FloorZ + 140.f), FVector(20.f, 95.f, 140.f));
	}
	K.Add(Wall, FVector(40.f, 660.f, 300.f), FVector(-2100.f, 0.f, FloorZ + 140.f));
	Box(FVector(-2100.f, 0.f, FloorZ + 150.f), FVector(20.f, 330.f, 150.f));
	for (int32 c = 0; c < 7; ++c) K.Add(OldStone(0.65f + 0.2f * Rand(c + 90)), FVector(150.f, 330.f, 45.f), FVector(-80.f - 150.f * c, 0.f, FloorZ + 270.f), FRotator(0.f, 6.f * (Rand(c + 95) - 0.5f), 0.f));
	for (int32 c = 0; c < 8; ++c) K.Add(OldStone(0.6f + 0.2f * Rand(c + 100)), FVector(150.f, 720.f, 60.f), FVector(-1075.f - 140.f * c, 0.f, FloorZ + 300.f), FRotator(0.f, 4.f * (Rand(c + 105) - 0.5f), 0.f));
	Box(FVector(-500.f, 0.f, FloorZ + 272.f), FVector(520.f, 165.f, 22.f));
	Box(FVector(-1550.f, 0.f, FloorZ + 300.f), FVector(560.f, 360.f, 30.f));

	// The façade: two portal stones and a lintel over the door, the stone door slab rolled aside, a shallow forecourt arc
	// of standing stones (the Grovekin's door stones: the only made thing on a meadow hill that reads from the coast).
	for (const float S : { -1.f, 1.f })
	{
		K.Add(Wall.Dim(0.95f), FVector(70.f, 90.f, 330.f), FVector(10.f, S * 175.f, FloorZ + 155.f), FRotator(0.f, 0.f, S * -2.f));
		Box(FVector(10.f, S * 175.f, FloorZ + 165.f), FVector(35.f, 45.f, 165.f));
	}
	K.Add(Wall.Dim(0.9f), FVector(90.f, 480.f, 70.f), FVector(10.f, 0.f, FloorZ + 330.f));
	Box(FVector(10.f, 0.f, FloorZ + 330.f), FVector(45.f, 240.f, 35.f));
	K.Add(Wall.Dim(0.8f), FVector(30.f, 230.f, 250.f), FVector(70.f, 330.f, FloorZ + 120.f), FRotator(-8.f, 15.f, 0.f));
	Box(FVector(70.f, 330.f, FloorZ + 125.f), FVector(15.f, 115.f, 125.f), FRotator(-8.f, 15.f, 0.f));
	for (int32 i = 0; i < 6; ++i)
	{
		const float Ang = -62.f + 124.f * i / 5.f;
		const FVector P0 = FVector(250.f, 0.f, 0.f) + Lm::Dir2(Ang) * 650.f;
		const float Gz = G(float(P0.X), float(P0.Y), FloorZ);
		Rock(i, FVector(90.f + 40.f * Rand(i + 120), 70.f + 30.f * Rand(i + 130), 240.f + 120.f * Rand(i + 140)), FVector(P0.X, P0.Y, Gz), FRotator(0.f, Ang + 90.f, 4.f * (Rand(i + 150) - 0.5f)), FLinearColor(0.12f, 0.12f, 0.11f), true, 40.f);
	}

	// The bier of antlers at the chamber's heart (top 45 cm over the door's ground: the cache sits on it beside him).
	const FVector Bier(-1500.f, 0.f, 0.f);
	K.Add(OldStone(0.7f), FVector(280.f, 300.f, 45.f), Bier + FVector(0, 0, 22.5f));
	Box(Bier + FVector(0, 0, 22.5f), FVector(140.f, 150.f, 22.5f));
	for (int32 a = 0; a < 8; ++a)
	{
		const float S = a % 2 ? 1.f : -1.f;
		const FVector P0 = Bier + FVector(-120.f + 70.f * (a / 2), S * 150.f, 40.f);
		Limb(P0, P0 + FVector(30.f, S * 25.f, 30.f), 6.f, 4.f, BoneC(), 6);
		Limb(P0 + FVector(15.f, S * 12.f, 15.f), P0 + FVector(-10.f, S * 30.f, 50.f), 3.f, 1.5f, BoneC(), 5);
	}
	{
		// The hunter: wrapped in hide from shoulder to foot, his skull bare under the antler crown, the bow on his chest.
		const FVector Body = Bier + FVector(0.f, 85.f, 45.f);
		M.Lathe(Body + FVector(-90.f, 0, 12.f), FVector::ForwardVector, 160.f, { FVector(0.f, 10.f, 8.f), FVector(0.2f, 17.f, 12.f), FVector(0.55f, 22.f, 15.f), FVector(0.85f, 24.f, 15.f), FVector(1.f, 16.f, 12.f) }, 10, Rgb(120, 86, 56) * 0.4f, 0.95f, 0.f, 7.f);
		const FVector Skull = Body + FVector(86.f, 0.f, 20.f);
		Ball(Skull, 12.f, BoneC(), 0.75f, 0.9f);
		Ball(Skull + FVector(10.f, -4.f, 3.f), 3.f, BlackC());
		Ball(Skull + FVector(10.f, 4.f, 3.f), 3.f, BlackC());
		for (int32 t = 0; t < 7; ++t)
		{
			const FVector Dir = Lm::Dir2(-90.f + 30.f * t);
			const FVector P0 = Skull + Dir * 12.f + FVector(0, 0, 6.f);
			Limb(P0, P0 + Dir * 8.f + FVector(-14.f, 0, 22.f), 2.2f, 0.6f, BoneC(), 5);
		}
		TArray<FVector> Bow;
		for (int32 i = 0; i <= 10; ++i)
		{
			const float U = i / 10.f;
			Bow.Add(Body + FVector(-70.f + 150.f * U, 18.f * FMath::Sin(PI * U) - 8.f, 30.f));
		}
		M.Tube(Bow, 2.8f, 6, Rgb(150, 92, 40) * 0.45f, 0.6f);
		Line(Bow[0], Bow.Last(), 0.5f, 0.f, RopeC(), 2);
	}

	// The elk skull on the mound, facing the door's way: cranium, long snout, dark sockets, and the palmate antlers 6 m
	// across, bleached pale.
	{
		const float Sx = -1300.f;
		const FVector Top(Sx, 0.f, G(Sx, 0.f, FloorZ) + Height(Sx, Len) - 20.f);
		const FLinearColor Bone = BoneC();
		Ball(Top + FVector(0, 0, 60.f), 60.f, Bone, 0.75f, 0.85f);
		M.Lathe(Top + FVector(40.f, 0.f, 50.f), FVector(0.97f, 0.f, -0.24f), 150.f, { FVector(0.f, 40.f, 34.f), FVector(0.5f, 30.f, 26.f), FVector(0.9f, 20.f, 18.f), FVector(1.f, 12.f, 12.f) }, 10, Bone, 0.75f);
		for (const float S : { -1.f, 1.f })
		{
			Ball(Top + FVector(30.f, S * 42.f, 70.f), 14.f, BlackC());
			Limb(Top + FVector(-20.f, S * 30.f, 20.f), Top + FVector(150.f, S * 18.f, 0.f), 9.f, 5.f, Bone, 6);   // the jaw
			Antler(Top + FVector(-20.f, S * 40.f, 105.f), FVector(-0.1f, S, 0.45f), WRTune(TEXT("forestWorld.barrowAntlerSpan"), 520.f), S < 0.f, Bone);
		}
		Ball(Top + FVector(175.f, 0.f, 18.f), 10.f, BlackC());   // the nasal hollow
		Box(Top + FVector(30.f, 0.f, 60.f), FVector(120.f, 70.f, 60.f));
	}
}

// ================================================================================================ L11 the Bell-Tree

void WRForestLook::FBuild::BellTree()
{
	// A dead giant on the Crystal border with a Grovekin bronze bell hung 40 m up under a horizontal limb (DC 2.3 L11;
	// F-S5: ring it at night and the Grey Matron comes for the bell). A great root has grown up under the leaning trunk
	// and holds it (the forest keeps its bell). Crystal veins grow through its roots from the south (the neighbours
	// touched); at night they glow faint violet. The ringing is the hazards track's; this is the look.
	namespace Lm = WRForestLmImpl;
	const Lm::FLandTree Tr = Lm::FindTree(*this, TEXT("bell_tree"), TEXT("bell_tree"), WRTune(TEXT("forestWorld.bellTreeHeight"), 7000.f));
	const FRotator Rot(0.f, Tr.Yaw, 0.f);
	Lm::Giant(*this, TEXT("bell_tree"), Tr, Rot);
	const float R = Tr.Radius;
	FVector BellAt = Lm::AnchorAt(TEXT("bell_tree"), Tr, Rot, TEXT("bell"));
	if (BellAt.IsZero()) BellAt = Tr.Base + Rot.RotateVector(FVector(R + 600.f, 0.f, WRTune(TEXT("forestWorld.bellHeight"), 4000.f)));
	const FVector Out = (BellAt - Tr.Base).GetSafeNormal2D().IsNearlyZero() ? Rot.Vector() : (BellAt - Tr.Base).GetSafeNormal2D();
	const FVector Across = FVector::CrossProduct(Out, FVector::UpVector).GetSafeNormal();

	// The headstock: an oak beam lashed and iron-strapped under the limb, the bell's yoke hanging from it on two straps.
	const FVector Beam = BellAt + FVector(0, 0, 35.f);
	K.Add(Timber(), FVector(60.f, 300.f, 40.f), Beam, FRotator(0.f, Across.Rotation().Yaw - 90.f, 0.f));
	for (const float S : { -1.f, 1.f })
	{
		K.Add(Iron(), FVector(66.f, 8.f, 70.f), Beam + Across * S * 120.f + FVector(0, 0, 20.f), FRotator(0.f, Across.Rotation().Yaw - 90.f, 0.f));
		Line(Beam + Across * S * 135.f + FVector(0, 0, 50.f), Beam + Across * S * 135.f + FVector(0, 0, 110.f), 2.f, 0.f, RopeC(), 2);
		K.Rod(Iron(), Beam + Across * S * 40.f - FVector(0, 0, 18.f), BellAt - FVector(0, 0, 30.f) + Across * S * 22.f, 6.f, true);
	}
	// The bell: a bronze lathe from the crown down to the lip (2 m across, 1.7 m tall), crown loops, clapper below the lip.
	{
		const FVector Crown = BellAt - FVector(0, 0, 30.f);
		const FLinearColor BronzeC = Rgb(140, 106, 58) * 0.6f;
		M.Lathe(Crown, -FVector::UpVector, 170.f, {
			FVector(0.f, 30.f, 30.f), FVector(0.06f, 46.f, 46.f), FVector(0.18f, 54.f, 54.f), FVector(0.45f, 57.f, 57.f), FVector(0.68f, 66.f, 66.f),
			FVector(0.82f, 80.f, 80.f), FVector(0.92f, 96.f, 96.f), FVector(1.f, 100.f, 100.f) }, 16, BronzeC, 0.45f);
		// Verdigris bands, a raised inscription band.
		K.Disc(FSurf::Steel(Rgb(78, 120, 100) * 0.5f, 0.7f), Crown - FVector(0, 0, 70.f), FVector::UpVector, 118.f, 8.f);
		K.Disc(FSurf::Steel(BronzeC * 0.9f, 0.5f), Crown - FVector(0, 0, 150.f), FVector::UpVector, 184.f, 6.f);
		for (int32 l = 0; l < 4; ++l)
		{
			TArray<FVector> Loop0;
			const FVector Dir = Lm::Dir2(45.f + 90.f * l);
			for (int32 i = 0; i <= 8; ++i) Loop0.Add(Crown + Dir * 12.f + FVector(0, 0, 4.f + 18.f * FMath::Sin(PI * i / 8.f)) + Dir * (FMath::Cos(PI * i / 8.f) * 12.f));
			M.Tube(Loop0, 3.f, 6, BronzeC, 0.5f);
		}
		Limb(Crown - FVector(0, 0, 10.f), Crown - FVector(0, 0, 175.f), 4.f, 3.f, FLinearColor(0.05f, 0.045f, 0.04f), 6);
		Ball(Crown - FVector(0, 0, 182.f), 14.f, FLinearColor(0.05f, 0.045f, 0.04f), 0.6f, 1.2f);
	}

	// The root that holds the trunk up: a living root (from a sapling of its own seed) climbing the down-lean side from the
	// ground to 8 m and gripping the bark, with two lesser props.
	for (int32 i = 0; i < 3; ++i)
	{
		const FVector Dir = Lm::Dir2(Tr.Yaw + 180.f + (i - 1) * 35.f);
		const FVector Foot = Tr.Base + Dir * (R * 2.6f + 150.f * i);
		const FVector P0(Foot.X, Foot.Y, G(float(Foot.X), float(Foot.Y), float(Tr.Base.Z)) - 40.f);
		const FVector Grip = Tr.Base + Dir * R * 0.95f + FVector(0, 0, (i == 1 ? 800.f : 450.f));
		Root({ P0, FMath::Lerp(P0, Grip, 0.5f) + Dir * 80.f + FVector(0, 0, 60.f), Grip }, (i == 1 ? 0.45f : 0.28f) * R, 0.12f * R, RootC(), 9);
		SegBox(P0, Grip, (i == 1 ? 0.35f : 0.22f) * R, (i == 1 ? 0.35f : 0.22f) * R);
	}

	// Crystal veins through the roots: violet spikes grown out of the root crown on the south side, glowing at night.
	const FSurf Cry = Crystal();
	for (int32 i = 0; i < 16; ++i)
	{
		const float Ang = Tr.Yaw + 90.f + 180.f * Rand(i + 800) - 90.f;
		const FVector Dir = Lm::Dir2(Ang);
		const FVector P0 = Tr.Base + Dir * (R * (1.05f + 0.5f * Rand(i + 810)));
		const float Gz = G(float(P0.X), float(P0.Y), float(Tr.Base.Z));
		const float Len = 40.f + 120.f * Rand(i + 820);
		const FVector Up = (FVector::UpVector + Dir * (0.3f + 0.4f * Rand(i + 830))).GetSafeNormal();
		K.Spike(Cry, FVector(P0.X, P0.Y, Gz - 10.f), Up, Len * 0.35f, Len);
		N.Spike(Violet(1.4f), FVector(P0.X, P0.Y, Gz - 10.f) + Up * Len * 0.15f, Up, Len * 0.18f, Len * 0.7f);
	}
	Lamp(Tr.Base + Lm::Dir2(Tr.Yaw + 90.f) * R * 1.4f + FVector(0, 0, 80.f), Rgb(138, 107, 176), WRTune(TEXT("forestWorld.bellCrystalLight"), 1600.f), 900.f);
}

// ================================================================================================ L12 the Ringfort

void WRForestLook::FBuild::Ringfort()
{
	// A Grovekin hillfort on a coastal hill (DC 2.3 L12, 2.4 #36): its wall a ring of pleached beeches grown into one
	// hedge 12 m tall, 60 m across, the gate (+X) under a living lintel of two grafted trunks with hulder posts either
	// side. Grown, never cut (DC 2.20): the Grovekin's walls are trees. Inside: a well, a newcomer merchant's stall (the
	// caravan stop) and a fire whose light is the night read.
	namespace Lm = WRForestLmImpl;
	const float Rh = WRTune(TEXT("forestWorld.ringfortRadius"), 3000.f);
	const float HedgeH = WRTune(TEXT("forestWorld.ringfortHedge"), 1200.f);
	const float GateHalf = FMath::RadiansToDegrees(260.f / Rh);
	const int32 Trunks = FMath::Max(24, FMath::RoundToInt(2.f * PI * Rh / 340.f));
	const float StepA = 360.f / Trunks;
	const FLinearColor Beech = BeechC();
	const auto PosAt = [&](float AngDeg) { const FVector P0 = Lm::Dir2(AngDeg) * Rh; return FVector(P0.X, P0.Y, G(float(P0.X), float(P0.Y), 0.f)); };
	const auto InGate = [&](float AngDeg) { return FMath::Abs(FRotator::NormalizeAxis(AngDeg)) < GateHalf + StepA * 0.5f; };
	for (int32 i = 0; i < Trunks; ++i)
	{
		const float Ang = i * StepA;
		if (InGate(Ang)) continue;
		const FVector P0 = PosAt(Ang);
		const float Lean = (i % 2 ? 1.f : -1.f) * 0.04f;
		const FVector Top = P0 + FVector(0, 0, HedgeH) + Lm::Dir2(Ang + 90.f) * HedgeH * Lean;
		Limb(P0 - FVector(0, 0, 30.f), Top, 30.f, 16.f, Beech, 8);
		const float NextA = Ang + StepA;
		if (InGate(NextA)) continue;
		const FVector P1 = PosAt(NextA);
		// Pleaching: the side branches bent along the line and woven into the next trunk's, a diamond lattice of living
		// wood that grew into one wall.
		for (float Z = 250.f; Z < HedgeH - 100.f; Z += 300.f)
		{
			M.Tube({ P0 + FVector(0, 0, Z), P1 + FVector(0, 0, Z + 160.f) }, 7.f, 5, Beech, 0.85f);
			M.Tube({ P0 + FVector(0, 0, Z + 160.f), P1 + FVector(0, 0, Z) }, 7.f, 5, Beech, 0.85f);
		}
		// The foliage: two overlapping slabs of beech leaf from 3.5 m up, a ragged top; the low wall shows the lattice.
		const FVector Mid = (P0 + P1) * 0.5f;
		const float Chord = FVector::Dist2D(P0, P1) * 1.1f;
		const FRotator Face(0.f, Ang + StepA * 0.5f + 90.f, 0.f);
		K.Add(Leaf(0.85f + 0.25f * Rand(i)), FVector(Chord, 220.f, HedgeH - 350.f), Mid + FVector(0, 0, 350.f + (HedgeH - 350.f) * 0.5f), Face);
		K.Add(Leaf(0.75f + 0.25f * Rand(i + 400)), FVector(Chord * 0.8f, 160.f, 180.f), Mid + FVector(0, 0, HedgeH + 40.f + 60.f * Rand(i + 500)), FRotator(0.f, Face.Yaw, 6.f * (Rand(i + 600) - 0.5f)));
		Box(Mid + FVector(0, 0, HedgeH * 0.5f), FVector(Chord * 0.5f, 90.f, HedgeH * 0.5f), Face);
	}
	// The gate: two thick trunks trained up either side and grafted into one over the way, with leaves on the lintel.
	{
		const FVector L0 = PosAt(-GateHalf), L1 = PosAt(GateHalf);
		Limb(L0 - FVector(0, 0, 40.f), L0 + FVector(0, 0, 900.f), 48.f, 34.f, Beech, 10);
		Limb(L1 - FVector(0, 0, 40.f), L1 + FVector(0, 0, 900.f), 48.f, 34.f, Beech, 10);
		const FVector Apex = (L0 + L1) * 0.5f + FVector(0, 0, 1150.f);
		Root({ L0 + FVector(0, 0, 880.f), FMath::Lerp(L0, Apex, 0.55f) + FVector(0, 0, 120.f), Apex }, 34.f, 26.f, Beech, 10);
		Root({ L1 + FVector(0, 0, 880.f), FMath::Lerp(L1, Apex, 0.55f) + FVector(0, 0, 120.f), Apex }, 34.f, 26.f, Beech, 10);
		Ball(Apex, 42.f, Beech, 0.85f, 0.8f);   // the graft's swollen union
		K.Add(Leaf(0.9f), FVector(200.f, FVector::Dist2D(L0, L1) + 200.f, 260.f), Apex + FVector(0, 0, 160.f), FRotator(0.f, 0.f, 0.f));
		Box(L0 + FVector(0, 0, 450.f), FVector(50.f, 50.f, 450.f));
		Box(L1 + FVector(0, 0, 450.f), FVector(50.f, 50.f, 450.f));
		for (const float S : { -1.f, 1.f })
		{
			const FVector Post = Lm::Dir2(S * (GateHalf + 4.f)) * (Rh + 160.f);
			Hulder(FVector(Post.X, Post.Y, G(float(Post.X), float(Post.Y), 0.f)), 0.f);
		}
	}

	// The fire in the middle: a ring of hearth stones, crossed logs, log seats round it; it burns for the caravan stop.
	{
		const float Gz = G(0.f, 0.f, 0.f);
		for (int32 s = 0; s < 10; ++s) K.Add(OldStone(0.75f + 0.25f * Rand(s + 700)), FVector(45.f, 35.f, 28.f), FVector(0, 0, Gz + 10.f) + Lm::Dir2(36.f * s) * 110.f, FRotator(0.f, 36.f * s, 0.f));
		for (int32 l = 0; l < 4; ++l) Limb(FVector(0, 0, Gz + 12.f) + Lm::Dir2(45.f * l) * 80.f, FVector(0, 0, Gz + 40.f) - Lm::Dir2(45.f * l) * 20.f, 9.f, 7.f, BarkC(), 7);
		K.Disc(Black(0.7f), FVector(0, 0, Gz + 3.f), FVector::UpVector, 170.f, 4.f);
		Loop(TEXT("forest.campfire"), FVector(0, 0, Gz + 25.f));
		Lamp(FVector(0, 0, Gz + 130.f), FLinearColor(1.f, 0.55f, 0.22f), WRTune(TEXT("forestWorld.ringfortFire"), 6000.f), 2400.f);
		for (int32 s = 0; s < 3; ++s)
		{
			const FVector C = Lm::Dir2(30.f + 120.f * s) * 360.f;
			const float Cz = G(float(C.X), float(C.Y), Gz);
			const FVector Tan = Lm::Dir2(120.f + 120.f * s);
			Limb(FVector(C.X, C.Y, Cz + 22.f) - Tan * 110.f, FVector(C.X, C.Y, Cz + 22.f) + Tan * 110.f, 24.f, 22.f, BarkC(), 8);
			SegBox(FVector(C.X, C.Y, Cz + 22.f) - Tan * 110.f, FVector(C.X, C.Y, Cz + 22.f) + Tan * 110.f, 22.f, 22.f);
		}
	}
	// The well: a ring of dressed stones, dark water below, a windlass with its rope and bucket.
	{
		const FVector C(-1200.f, 900.f, 0.f);
		const float Gz = G(float(C.X), float(C.Y), 0.f);
		const FVector W0(C.X, C.Y, Gz);
		for (int32 s = 0; s < 14; ++s) K.Add(OldStone(0.8f + 0.2f * Rand(s + 720)), FVector(44.f, 30.f, 80.f), W0 + Lm::Dir2(360.f / 14.f * s) * 92.f + FVector(0, 0, 40.f), FRotator(0.f, 360.f / 14.f * s, 0.f));
		for (int32 s = 0; s < 8; ++s) Box(W0 + Lm::Dir2(45.f * s) * 92.f + FVector(0, 0, 40.f), FVector(18.f, 38.f, 40.f), FRotator(0.f, 45.f * s, 0.f));
		K.Disc(Water(), W0 - FVector(0, 0, 70.f), FVector::UpVector, 150.f, 4.f);
		K.Disc(Black(0.4f), W0 - FVector(0, 0, 30.f), FVector::UpVector, 158.f, 2.f);
		for (const float S : { -1.f, 1.f }) K.Rod(Timber(), W0 + FVector(0, S * 115.f, 0.f), W0 + FVector(0, S * 115.f, 190.f), 12.f, true);
		K.Rod(Timber(0.9f), W0 + FVector(0, -125.f, 170.f), W0 + FVector(0, 125.f, 170.f), 12.f);
		K.Add(Timber(0.8f), FVector(4.f, 40.f, 6.f), W0 + FVector(0, 140.f, 150.f));
		K.Disc(Rope(), W0 + FVector(0, 0, 170.f), FVector::RightVector, 24.f, 60.f);
		Line(W0 + FVector(0, 0, 160.f), W0 + FVector(0, 0, 70.f), 1.5f, 0.f, RopeC(), 2);
		M.Lathe(W0 + FVector(0, 0, 40.f), FVector::UpVector, 30.f, { FVector(0.f, 13.f, 13.f), FVector(1.f, 16.f, 16.f) }, 10, Rgb(90, 70, 54) * 0.45f, 0.7f);
	}
	// The merchant's stall: a timber frame, a canvas awning, a counter of goods (crates, a barrel, jars, hides on a pole)
	// and a lantern on the front post, lit at night.
	{
		const FVector C(900.f, -1200.f, 0.f);
		const float Gz = G(float(C.X), float(C.Y), 0.f);
		const float SYaw = (-C).Rotation().Yaw;
		const FRotator Face(0.f, SYaw, 0.f);
		const auto P = [&](float X, float Y, float Z) { return FVector(C.X, C.Y, Gz) + Face.RotateVector(FVector(X, Y, Z)); };
		for (int32 c = 0; c < 4; ++c)
		{
			const float X = c < 2 ? 100.f : -100.f, Y = c % 2 ? 150.f : -150.f;
			K.Rod(Timber(), P(X, Y, -20.f), P(X, Y, X > 0.f ? 230.f : 260.f), 14.f, true);
			Box(P(X, Y, 120.f), FVector(8.f, 8.f, 130.f), Face);
		}
		K.Add(Canvas(1.f), FVector(260.f, 340.f, 5.f), P(0.f, 0.f, 255.f), FRotator(-8.f, SYaw, 0.f));
		K.Add(Timber(0.9f), FVector(60.f, 290.f, 100.f), P(110.f, 0.f, 50.f), Face);
		K.Add(Plank(0.9f), FVector(70.f, 300.f, 6.f), P(110.f, 0.f, 103.f), Face);
		Box(P(110.f, 0.f, 50.f), FVector(30.f, 145.f, 50.f), Face);
		for (int32 g = 0; g < 4; ++g) K.Add(Plank(0.75f + 0.1f * g), FVector(50.f, 50.f, 40.f), P(-40.f, -110.f + 70.f * g, 20.f + 40.f * (g % 2)), FRotator(0.f, SYaw + 8.f * g, 0.f));
		K.Add(Timber(0.7f), FVector(60.f, 60.f, 90.f), P(-60.f, 120.f, 45.f), Face, EWRLookMesh::Cylinder);
		for (int32 j = 0; j < 4; ++j) K.Add(Clay(), FVector(18.f, 18.f, 26.f), P(115.f, -100.f + 60.f * j, 119.f), Face, EWRLookMesh::Cylinder);
		K.Rod(Timber(0.85f), P(-100.f, -150.f, 200.f), P(-100.f, 150.f, 200.f), 8.f);
		for (int32 h = 0; h < 3; ++h) K.Add(Hide(0.9f + 0.1f * h), FVector(4.f, 70.f, 110.f), P(-100.f, -100.f + 100.f * h, 140.f), Face);
		const FVector Lantern = P(100.f, 160.f, 200.f);
		K.Add(Bronze(), FVector(18.f, 18.f, 4.f), Lantern + FVector(0, 0, 16.f));
		N.Add(Window(0.95f), FVector(12.f, 12.f, 24.f), Lantern);
		Lamp(Lantern, FLinearColor(1.f, 0.6f, 0.28f), WRTune(TEXT("forestWorld.stallLantern"), 2500.f), 900.f);
	}
}
