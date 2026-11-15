// The Fungal caverns' surface scenery (cooks/biome-fungal.md 2.1-2.4, 2.20; cooks/build-biome-fungal.md 2.5): the camps on
// their sinkhole rims with the newcomers' wrecks in the coves below, the surface landmarks (the Ancestor Cap over the
// Great Doline and the Breath, the Kneeling Titan, the Ring of Caps), the six descents and their ways down, the hand
// sites S1-S8, the surface kit and the plateau's juniper, boulders and tufts. Every piece answers why it's here, what it
// connects to and how it meets what it touches (build cook 2.5). The caverns' furniture is in WRFungalSceneryCaves.cpp;
// the dispatch is at the end of this file.

#include "WRFungalWorld.h"
#include "WRFungalLook.h"
#include "WRFungalTypes.h"
#include "Wildreach.h"
#include "Components/InstancedStaticMeshComponent.h"

namespace WRFungalSurf
{
	using namespace WRFungalLook;
	using FData = FWRFungalData;

	const FData::FSite* SiteOf(const AWRFungalScenery* A) { return FData::Get().FindSite(A->SiteId); }

	/** A helical way down a round shaft's wall (D1's path cut in the doline, D3's rope-and-plank stair hung on bronze pins,
	 *  D6's ledge, D4's shelves): from local height Z1 at angle A0 down to Z0, R from the axis, Width wide, Slope (rise
	 *  over run). Every segment is walkable collision; the look by Style (0 carved ledge, 1 planks on pins, 2 shelf fungus
	 *  steps). Returns the angle it ends at. */
	float Helix(FFg& F, const FVector& Axis, float R, float Z0, float Z1, float A0, float Width, float Slope, int32 Style, int32 Seed, TArray<FVector>* OutTurns = nullptr)
	{
		const float Seg = Style == 2 ? 360.f : 300.f;
		const float Drop = Style == 2 ? 140.f : Seg * Slope;
		float A = A0, Z = Z1;
		int32 i = 0;
		float Turned = 0.f;
		while (Z > Z0 + 1.f && i < 2000)
		{
			const float DA = Seg / R;
			const float Zn = FMath::Max(Z0, Z - Drop);
			const FVector P0 = Axis + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, Z);
			const FVector P1 = Axis + FVector(FMath::Cos(A + DA) * R, FMath::Sin(A + DA) * R, Zn);
			const FVector In0 = FVector(-FMath::Cos(A), -FMath::Sin(A), 0.f);
			if (Style == 2)
			{
				// A bracket fungus every step: you drop (or jump) shelf to shelf.
				Shelf(F, P1 + In0 * -0.f, In0, Width, true, (i % 3) == 0, Seed + i);
			}
			else
			{
				const FVector Mid = (P0 + P1) * 0.5f;
				const FVector Out = -FVector(FMath::Cos(A + DA * 0.5f), FMath::Sin(A + DA * 0.5f), 0.f);
				F.B.SegBox(P0 + Out * (Width * 0.5f) - FVector(0, 0, 18.f), P1 + Out * (Width * 0.5f) - FVector(0, 0, 18.f), Width * 0.5f, 18.f);
				if (Style == 0)
				{
					// A ledge cut into the rock: a rough stone tread, its lip worn, a low kerb on the drop side.
					F.B.K.Add(Limestone(0.95f + 0.1f * FgH(Seed, i)), FVector((P1 - P0).Size2D() + 20.f, Width, 36.f), Mid + Out * (Width * 0.5f) - FVector(0, 0, 18.f), FRotator(FMath::RadiansToDegrees(FMath::Atan2(Zn - Z, Seg)), FMath::RadiansToDegrees(A + DA * 0.5f) + 90.f, 0.f));
					if (i % 2 == 0) F.B.K.Add(Carved(0.85f), FVector(Seg * 0.8f, 26.f, 34.f), Mid + Out * (Width - 10.f) + FVector(0, 0, 8.f), FRotator(0.f, FMath::RadiansToDegrees(A + DA * 0.5f) + 90.f, 0.f));
				}
				else
				{
					// Planks across two rope stringers, hung on bronze pins driven into the wall every segment.
					for (int32 p = 0; p < 6; ++p)
					{
						const FVector Q = FMath::Lerp(P0, P1, (p + 0.5f) / 6.f) + Out * (Width * 0.5f);
						F.B.K.Add(OldWood(0.9f + 0.2f * FgH(Seed, i * 6 + p)), FVector(36.f, Width, 6.f), Q - FVector(0, 0, 4.f), FRotator(0.f, FMath::RadiansToDegrees(A + DA * (p + 0.5f) / 6.f) + 90.f, 0.f));
					}
					F.B.K.Rod(Bronze(), P0 + FVector(0, 0, -10.f), P0 - In0 * 0.f + Out * 10.f + FVector(0, 0, -10.f), 6.f);
					F.B.Line(P0 + Out * Width + FVector(0, 0, 90.f), P1 + Out * Width + FVector(0, 0, 90.f), 2.2f, 20.f, RopeC());
					F.B.K.Rod(Timber(), P0 + Out * Width - FVector(0, 0, 4.f), P0 + Out * Width + FVector(0, 0, 92.f), 6.f);
				}
			}
			Turned += DA;
			if (OutTurns && Turned > 2.f * PI) { OutTurns->Add(P1); Turned = 0.f; }
			A += DA;
			Z = Zn;
			++i;
		}
		return A;
	}

	/** A castaway's boat on its side in a cove (world-map newcomer rule: newer rope and canvas than any ruin). */
	void Wreck(FFg& F, const FVector& At, float Yaw, int32 Seed)
	{
		const FRotator R(0.f, Yaw, 16.f);
		const auto P = [&](float X, float Y, float Z) { return At + R.RotateVector(FVector(X, Y, Z)); };
		const float Len = 860.f, Beam = 250.f;
		F.B.K.Rod(Tarred(), P(-Len * 0.5f, 0.f, 20.f), P(Len * 0.5f, 0.f, 20.f), 22.f, true);
		F.B.K.Rod(Tarred(), P(Len * 0.5f, 0.f, 20.f), P(Len * 0.5f + 60.f, 0.f, 200.f), 18.f, true);
		F.B.K.Rod(Tarred(), P(-Len * 0.5f, 0.f, 20.f), P(-Len * 0.5f - 30.f, 0.f, 180.f), 18.f, true);
		for (int32 i = 0; i < 9; ++i)
		{
			const float X = -Len * 0.42f + i * (Len * 0.84f / 8.f);
			const float W = Beam * 0.5f * FMath::Sin(PI * (0.15f + 0.7f * i / 8.f));
			for (const float S : { -1.f, 1.f })
			{
				if (FgH(Seed, i) > 0.65f && S > 0.f) continue;
				F.B.K.Rod(Timber(0.8f), P(X, 0.f, 25.f), P(X, S * W * 0.85f, 70.f), 12.f, true);
				F.B.K.Rod(Timber(0.8f), P(X, S * W * 0.85f, 70.f), P(X, S * W, 170.f), 12.f, true);
			}
		}
		for (int32 s = 0; s < 4; ++s) F.B.K.Add(Timber(0.95f + 0.1f * FgH(Seed, s + 20)), FVector(Len * (0.8f - 0.08f * s), 4.f, 32.f), P(0.f, -Beam * 0.5f * (0.6f + 0.1f * s), 45.f + s * 34.f), R + FRotator(0.f, 0.f, -20.f + s * 8.f));
		F.B.K.Rod(Timber(1.1f), P(80.f, 0.f, 20.f), P(80.f, -30.f, 250.f), 14.f);
		F.B.K.Rod(FSurf::Cloth(Rgb(156, 132, 96) * 0.5f), P(80.f, -30.f, 240.f), P(Len * 0.5f + 40.f, 0.f, 190.f), 1.5f);
		const FVector Oa = At + FRotator(0.f, Yaw, 0.f).RotateVector(FVector(-100.f, 330.f, 0.f));
		F.B.K.Rod(OldWood(), Oa, Oa + FVector(0, 0, 160.f), 6.f);
		F.B.K.Add(Sailcloth(), FVector(300.f, 4.f, 220.f), Oa + FRotator(0.f, Yaw, 0.f).RotateVector(FVector(0.f, -90.f, 70.f)), FRotator(0.f, Yaw, -52.f));
		F.B.SegBox(P(-Len * 0.5f, 0.f, 60.f), P(Len * 0.5f, 0.f, 60.f), Beam * 0.4f, 60.f);
	}

	/** A camp fire on its stone ring with the bedrolls round it (lit at night: the camp's light). */
	void Fire(FFg& F, const FVector& At, int32 Seed)
	{
		for (int32 s = 0; s < 9; ++s)
		{
			const float A = 2.f * PI * s / 9.f;
			F.B.Rock(s, FVector(32.f, 28.f, 20.f), At + FVector(FMath::Cos(A) * 62.f, FMath::Sin(A) * 62.f, 0.f), FRotator(0.f, FMath::RadiansToDegrees(A), 0.f), Rgb(150, 146, 134) * 0.6f, false, 6.f);
		}
		for (int32 l = 0; l < 4; ++l) F.B.K.Rod(Tarred(0.9f), At + FVector(FMath::Cos(l * 1.6f) * 40.f, FMath::Sin(l * 1.6f) * 40.f, 6.f), At + FVector(0, 0, 26.f), 9.f);
		F.B.N.Add(HearthGlow(1.2f), FVector(50.f, 50.f, 14.f), At + FVector(0, 0, 10.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		F.B.Lamp(At + FVector(0, 0, 80.f), FireLight(), 4200.f, 1500.f);
		F.B.Loop(TEXT("world.campfire_smoke"), At + FVector(0, 0, 60.f));
		for (int32 b = 0; b < 2; ++b)
		{
			const float A = 1.2f + b * 2.4f;
			F.B.K.Add(FSurf::Cloth(Rgb(120, 96, 70) * 0.45f), FVector(190.f, 80.f, 12.f), At + FVector(FMath::Cos(A) * 190.f, FMath::Sin(A) * 190.f, 6.f), FRotator(0.f, FMath::RadiansToDegrees(A) + 90.f, 0.f));
		}
	}

	void Camp(FFg& F, AWRFungalScenery* A)
	{
		const FData::FSite* S = SiteOf(A);
		if (!S) return;
		const int32 Sd = int32(GetTypeHash(A->SiteId) % 997u);
		// The camp on the rim of its sinkhole (the bowl glows at night: a cap meadow in its floor), facing the cove's gully.
		Fire(F, F.B.At(0.f, 0.f), Sd);
		// A driftwood lean-to with sailcloth over it, a sea chest, a water skin on a post.
		const FVector Le = F.B.At(-260.f, 180.f);
		F.B.K.Rod(Drift(), Le, Le + FVector(0, 0, 210.f), 9.f);
		F.B.K.Rod(Drift(), Le + FVector(0, 300.f, 0.f), Le + FVector(0, 300.f, 210.f), 9.f);
		F.B.K.Rod(Drift(), Le + FVector(0, 0, 210.f), Le + FVector(0, 300.f, 210.f), 8.f);
		F.B.K.Add(Sailcloth(), FVector(240.f, 330.f, 3.f), Le + FVector(-100.f, 150.f, 120.f), FRotator(-52.f, 0.f, 0.f));
		F.B.K.Add(Timber(0.9f), FVector(90.f, 54.f, 50.f), F.B.At(-160.f, -150.f, 25.f), FRotator(0.f, 20.f, 0.f));
		F.B.K.Add(Bronze(0.7f), FVector(92.f, 6.f, 6.f), F.B.At(-160.f, -150.f, 50.f), FRotator(0.f, 20.f, 0.f));
		F.B.Box(F.B.At(-160.f, -150.f, 25.f), FVector(45.f, 27.f, 25.f), FRotator(0.f, 20.f, 0.f));
		// The waymark at the bowl's lip: a glow-cap ring; the bowl's floor: a cap meadow (caps and ferns, glowing at night).
		if (S->Bowl.Z > 0.f)
		{
			const FVector Bowl = F.B.Local(FVector(S->Bowl.X, S->Bowl.Y, 0.f));
			const FVector Lip = Bowl + (FVector(0, 0, 0) - Bowl).GetSafeNormal2D() * (S->Bowl.Z * 0.95f);
			GlowRing(F, F.B.At(Lip.X, Lip.Y), Sd);
			for (int32 c = 0; c < 22; ++c)
			{
				const float Aa = 2.f * PI * FgH(Sd, c), Rr = S->Bowl.Z * 0.75f * FMath::Sqrt(FgH(Sd, c + 40));
				const FVector P = F.B.At(Bowl.X + FMath::Cos(Aa) * Rr, Bowl.Y + FMath::Sin(Aa) * Rr);
				if (c % 3 == 0) F.B.Tree(TEXT("fern"), c, P, 70.f + 40.f * FgH(Sd, c + 7), FRotator(0.f, FgH(Sd, c + 9) * 360.f, 0.f), 0.f);
				else Mushroom(F, P, FVector::UpVector, 18.f + 30.f * FgH(Sd, c + 11), 10.f + 14.f * FgH(Sd, c + 13), c % 3 == 1 ? ECap::Honey : ECap::Glowcap, 0.55f, Sd + c, false);
			}
			F.B.Lamp(F.B.At(Bowl.X, Bowl.Y, 120.f), CyanC(), 900.f, S->Bowl.Z * 2.4f);
		}
		// The cove below: the wreck on the shingle, its timbers and a chest washed up by it.
		if (!S->Cove.IsZero())
		{
			const FVector C = F.B.Local(S->Cove);
			const FVector Wr = C + FVector(0, 0, 40.f);
			Wreck(F, Wr, FMath::RadiansToDegrees(FMath::Atan2(-C.Y, -C.X)) + 70.f, Sd);
			for (int32 t = 0; t < 5; ++t) F.B.K.Add(Drift(0.9f), FVector(260.f + 120.f * FgH(Sd, t), 20.f, 14.f), C + FVector(FMath::Cos(t * 1.3f) * 700.f, FMath::Sin(t * 1.3f) * 500.f, 8.f), FRotator(0.f, FgH(Sd, t + 3) * 180.f, 0.f));
			// Cairns up the gully: a stone stacked every 40 m on the way up to the camp (the newcomers marked their way).
			const FVector Up = (FVector(0, 0, 0) - C).GetSafeNormal2D();
			const float Run = FMath::Min(22000.f, C.Size2D() - 2000.f);
			for (float T = 1500.f; T < Run; T += 4000.f)
			{
				const FVector Q = C + Up * T + FVector(-Up.Y, Up.X, 0.f) * 600.f;
				const FVector G = F.B.At(Q.X, Q.Y);
				for (int32 k = 0; k < 4; ++k) F.B.K.Add(Limestone(0.9f + 0.05f * k), FVector(46.f - k * 8.f, 40.f - k * 7.f, 20.f), G + FVector(0, 0, 10.f + k * 19.f), FRotator(0.f, k * 37.f, 0.f));
			}
		}
	}

	void AncestorCap(FFg& F, AWRFungalScenery* A)
	{
		// L1 (fungal 2.3): a single honey-gold mushroom 90 m tall out of the Great Doline's floor, its 60 m cap glowing
		// from the gills; fissures round its foot breathe a column of glowing spores 400 m up (the Breath). Its stem's foot
		// is a buttress of root flanges spreading into the doline's floor so it stands, not sits.
		const FData& D = FData::Get();
		const float H = 9000.f, StemR = 450.f, CapR = 3000.f;
		const FVector Base(0.f, 0.f, -60.f);
		F.B.M.Lathe(Base, FVector::UpVector, H, { FVector(0.f, StemR * 2.1f, StemR * 2.1f), FVector(0.04f, StemR * 1.45f, StemR * 1.45f), FVector(0.12f, StemR * 1.1f, StemR * 1.1f), FVector(0.5f, StemR, StemR), FVector(0.86f, StemR * 0.92f, StemR * 0.92f), FVector(0.97f, StemR * 1.3f, StemR * 1.3f), FVector(1.f, StemR * 1.7f, StemR * 1.7f) }, 28, Rgb(226, 206, 160) * 0.6f, 0.75f, 0.04f, 7.f);
		for (int32 f = 0; f < 9; ++f)
		{
			const float Ang = 2.f * PI * (f + 0.3f * FgH(9, f)) / 9.f;
			const FVector Dir(FMath::Cos(Ang), FMath::Sin(Ang), 0.f);
			TArray<FVector> Root;
			for (int32 k = 0; k <= 6; ++k) { const float T = k / 6.f; Root.Add(Dir * (StemR * 0.8f + T * StemR * 3.2f) + FVector(0, 0, 900.f * FMath::Square(1.f - T) - 80.f)); }
			F.B.M.Tube(Root, StemR * 0.32f, 9, Rgb(200, 176, 128) * 0.55f, 0.8f);
		}
		// The cap: honey-gold above, a deep fan of cyan gills below; a torn ring under it.
		const FVector CapBase = Base + FVector(0, 0, H - 600.f);
		F.B.M.Lathe(CapBase, FVector::UpVector, 1700.f, { FVector(0.f, CapR, CapR), FVector(0.25f, CapR * 0.95f, CapR * 0.95f), FVector(0.6f, CapR * 0.66f, CapR * 0.66f), FVector(0.88f, CapR * 0.3f, CapR * 0.3f), FVector(1.f, 10.f, 10.f) }, 40, Rgb(214, 162, 72) * 0.6f, 0.7f);
		F.Glow.Lathe(CapBase - FVector(0, 0, 420.f), FVector::UpVector, 440.f, { FVector(0.f, StemR * 1.6f, StemR * 1.6f), FVector(0.55f, CapR * 0.62f, CapR * 0.62f), FVector(1.f, CapR * 0.97f, CapR * 0.97f) }, 40, CyanC(), 0.95f);
		for (int32 g = 0; g < 72; ++g)
		{
			const float Ang = 2.f * PI * g / 72.f;
			const FVector Dir(FMath::Cos(Ang), FMath::Sin(Ang), 0.f);
			F.Glow.Lathe(CapBase - FVector(0, 0, 300.f) + Dir * StemR * 1.7f, Dir, CapR * 0.9f, { FVector(0.f, 6.f, 160.f), FVector(1.f, 4.f, 30.f) }, 4, CyanC() * 0.9f, 1.f);
		}
		F.B.M.Lathe(Base + FVector(0, 0, H * 0.78f), FVector::UpVector, 500.f, { FVector(0.f, StemR * 1.02f, StemR * 1.02f), FVector(0.3f, StemR * 2.2f, StemR * 2.2f), FVector(1.f, StemR * 1.05f, StemR * 1.05f) }, 24, Rgb(236, 226, 206) * 0.6f, 0.9f);
		F.B.SegBox(Base, Base + FVector(0, 0, H), StemR * 0.9f, StemR * 0.9f);
		F.B.Lamp(CapBase - FVector(0, 0, 900.f), CyanC(), 60000.f, CapR * 2.6f);
		F.B.Lamp(CapBase + FVector(0, 0, 1600.f), GoldC(), 30000.f, CapR * 1.8f);
		// The Breath: fissures round the foot and the column of drifting light over the cap.
		for (int32 k = 0; k < 7; ++k)
		{
			const float Ang = 2.f * PI * (k + 0.5f) / 7.f;
			const FVector P = F.B.At(FMath::Cos(Ang) * StemR * 4.f, FMath::Sin(Ang) * StemR * 4.f, -6.f);
			F.B.K.Add(FSurf::Stone(Rgb(16, 16, 14) * 0.6f, 0.95f), FVector(320.f, 46.f, 30.f), P, FRotator(0.f, FMath::RadiansToDegrees(Ang) + 90.f + 20.f * FgH(5, k), 0.f));
			F.Glow.Lathe(P + FVector(0, 0, 4.f), FVector::UpVector, 4.f, { FVector(0.f, 30.f, 150.f), FVector(1.f, 20.f, 140.f) }, 6, CyanC(), 0.6f);
			F.B.Loop(TEXT("fungal.breathVent"), P + FVector(0, 0, 30.f));
		}
		for (int32 c = 0; c < 6; ++c) F.B.Loop(TEXT("fungal.breath"), FVector(0, 0, H + 600.f + c * 6000.f), FRotator::ZeroRotator, 3.f);
		// The doline's floor round it: moss, glowing caps, ferns; old Deepfarmer cap-terraces stepped into its west side.
		for (int32 c = 0; c < 40; ++c)
		{
			const float Ang = 2.f * PI * FgH(13, c), Rr = StemR * 5.f + (D.GreatR * 0.5f - StemR * 5.f) * FMath::Sqrt(FgH(13, c + 50));
			const FVector P = F.B.At(FMath::Cos(Ang) * Rr, FMath::Sin(Ang) * Rr);
			if (c % 4 == 0) F.B.Tree(TEXT("fern"), c, P, 90.f, FRotator(0.f, FgH(13, c) * 360.f, 0.f), 0.f);
			else Mushroom(F, P, FVector::UpVector, 60.f + 260.f * FgH(13, c + 3), 30.f + 120.f * FgH(13, c + 5), c % 3 ? ECap::Honey : ECap::Glowcap, 0.5f, c, true);
		}
		for (int32 t = 0; t < 4; ++t)
		{
			const float Ang = FMath::DegreesToRadians(160.f + t * 14.f);
			const FVector P = F.B.At(FMath::Cos(Ang) * D.GreatR * 0.72f, FMath::Sin(Ang) * D.GreatR * 0.72f);
			LogBed(F, P, FMath::RadiansToDegrees(Ang) + 90.f, t + 30);
		}
	}

	void KneelingTitan(FFg& F, AWRFungalScenery* A)
	{
		// L2 (fungal 2.3): a Deepfarmer titan 35 m tall, dead and stone-hard, kneeling with one arm sunk to the shoulder in
		// a doline (the actor faces it); gill-ribs open like a broken birdcage, its cap split by a 12 m Kitewright spear.
		// Foxfire glows along its cracks at night. Built from limbs (it was grown, so its parts are fungal forms: a stipe
		// trunk, a cap head, gill-plate ribs).
		const FLinearColor Stone = Rgb(120, 112, 100) * 0.55f, Dark = Rgb(80, 74, 66) * 0.55f;
		const float S = 100.f;
		const FVector Hip(-6.f * S, 0.f, 9.f * S), Chest(-2.f * S, 0.f, 20.f * S), Neck(1.f * S, 0.f, 27.f * S), Head(3.f * S, 0.f, 31.f * S);
		F.B.M.Lathe(Hip, (Chest - Hip).GetSafeNormal(), (Chest - Hip).Size() + 200.f, { FVector(0.f, 3.f * S, 2.4f * S), FVector(0.5f, 4.2f * S, 3.f * S), FVector(1.f, 3.4f * S, 2.6f * S) }, 18, Stone, 0.92f, 0.f, 3.f);
		F.B.Limb(Chest, Neck, 2.2f * S, 1.5f * S, Stone, 12);
		// The cap head, split by the spear.
		F.B.M.Lathe(Head - FVector(0, 0, 1.f * S), FVector::UpVector, 3.4f * S, { FVector(0.f, 4.6f * S, 4.6f * S), FVector(0.4f, 4.2f * S, 4.2f * S), FVector(0.8f, 2.4f * S, 2.4f * S), FVector(1.f, 20.f, 20.f) }, 22, Stone, 0.9f);
		F.B.K.Add(FSurf::Stone(Rgb(20, 18, 16) * 0.6f, 0.95f), FVector(9.5f * S, 40.f, 3.f * S), Head + FVector(0, 0, 1.f * S), FRotator(0.f, 25.f, 8.f));
		F.B.K.Rod(Bamboo(0.6f), Head + FVector(-2.f * S, -5.f * S, 6.f * S), Head + FVector(2.5f * S, 4.f * S, -1.f * S), 28.f);
		F.B.K.Spike(Bronze(0.8f), Head + FVector(2.5f * S, 4.f * S, -1.f * S), FVector(0.45f, 0.9f, -0.7f), 60.f, 180.f);
		// Gill-ribs open on its chest: curved plates standing out like a broken birdcage.
		for (int32 r = 0; r < 9; ++r)
		{
			const float Z = 13.f * S + r * 0.9f * S;
			for (const float Sd : { -1.f, 1.f })
			{
				if (FgH(4, r * 2 + int32(Sd > 0)) < 0.2f) continue;
				TArray<FVector> Rib;
				for (int32 k = 0; k <= 5; ++k) { const float T = k / 5.f; Rib.Add(FVector(-2.f * S + 4.f * S * FMath::Sin(T * 1.6f) + T * 2.f * S, Sd * (2.2f * S + 2.f * S * FMath::Sin(T * PI)), Z + T * 0.4f * S)); }
				F.B.M.Tube(Rib, 18.f, 6, Dark, 0.9f);
			}
		}
		// Legs folded under it (kneeling): thighs forward, shins back along the ground.
		for (const float Sd : { -1.f, 1.f })
		{
			const FVector Kn(2.f * S, Sd * 3.f * S, 1.2f * S), Ft(-7.f * S, Sd * 3.4f * S, 0.6f * S);
			F.B.Limb(Hip + FVector(0, Sd * 2.f * S, 0.f), Kn, 2.f * S, 1.6f * S, Stone, 12);
			F.B.Limb(Kn, Ft, 1.6f * S, 1.1f * S, Stone, 12);
			F.B.SegBox(Kn, Ft, 1.2f * S, 1.2f * S);
		}
		// One arm resting on its knee, the other reaching forward into the doline to the shoulder.
		F.B.Limb(Chest + FVector(0, -3.4f * S, 2.f * S), FVector(1.f * S, -4.f * S, 9.f * S), 1.4f * S, 1.1f * S, Stone, 10);
		F.B.Limb(FVector(1.f * S, -4.f * S, 9.f * S), FVector(3.f * S, -3.5f * S, 2.f * S), 1.1f * S, 0.9f * S, Stone, 10);
		const FVector Sh = Chest + FVector(1.f * S, 3.4f * S, 2.f * S);
		F.B.Limb(Sh, FVector(14.f * S, 3.f * S, -10.f * S), 1.5f * S, 1.2f * S, Stone, 12);
		F.B.SegBox(Hip, Chest, 3.f * S, 2.4f * S);
		F.B.SegBox(Chest, Head, 2.f * S, 2.f * S);
		F.B.SegBox(Sh, FVector(14.f * S, 3.f * S, -10.f * S), 1.2f * S, 1.2f * S);
		// Foxfire in its cracks (night).
		for (int32 c = 0; c < 14; ++c)
		{
			const FVector P = FMath::Lerp(Hip, Head, FgH(8, c)) + FVector(FgH(8, c + 20) * 200.f - 100.f, (FgH(8, c + 40) > 0.5f ? 1.f : -1.f) * 2.6f * S, 0.f);
			F.B.N.Add(FoxfireGlow(1.6f), FVector(160.f + 200.f * FgH(8, c + 60), 10.f, 8.f), P, FRotator(FgH(8, c + 80) * 80.f - 40.f, 90.f, 0.f));
		}
		F.B.Lamp(Chest + FVector(0, 0, 400.f), FoxC(), 2400.f, 2600.f);
	}

	void RingOfCaps(FFg& F, AWRFungalScenery* A)
	{
		// L3 (fungal 2.3): a fairy ring 200 m across of 64 white caps 6-12 m tall in a perfect circle, gills glowing
		// cyan at night: a halo on the plateau. The ring grew outward from one mycelium (the inner turf is greener).
		const FData& D = FData::Get();
		const int32 N = 64;
		for (int32 c = 0; c < N; ++c)
		{
			const float Ang = 2.f * PI * c / N + 0.01f * FgH(21, c);
			const float R = D.RingR + 120.f * (FgH(21, c + 100) - 0.5f);
			const FVector P = F.B.At(FMath::Cos(Ang) * R, FMath::Sin(Ang) * R, -10.f);
			const float H = 600.f + 600.f * FgH(21, c + 200);
			Mushroom(F, P, FVector(FMath::Cos(Ang) * 0.06f, FMath::Sin(Ang) * 0.06f, 1.f), H, H * 0.32f, c % 5 == 0 ? ECap::Ink : ECap::Parasol, 0.55f, c, true, false, nullptr);
		}
		F.B.Lamp(F.B.At(0.f, 0.f, 1200.f), CyanC(), 6000.f, D.RingR * 1.6f);
	}

	void Ringstones(FFg& F, AWRFungalScenery* A)
	{
		// S7: seven standing stones carved with spiralling mycelium round a flat altar (the Ring Shrine's brazier stands on
		// the altar's east end: the shrine row). F-S1's altar slides on the ring's door (the secret's actor moves the slab).
		for (int32 s = 0; s < 7; ++s)
		{
			const float Ang = 2.f * PI * s / 7.f;
			Standing(F, F.B.At(FMath::Cos(Ang) * 1200.f, FMath::Sin(Ang) * 1200.f), 260.f + 80.f * FgH(31, s), FMath::RadiansToDegrees(Ang) + 90.f, s + 31);
		}
		F.B.K.Add(Carved(0.9f), FVector(320.f, 180.f, 60.f), F.B.At(-200.f, 0.f, 30.f));
		F.B.Box(F.B.At(-200.f, 0.f, 30.f), FVector(160.f, 90.f, 30.f));
		for (int32 g = 0; g < 5; ++g) F.B.K.Add(Carved(0.6f), FVector(280.f, 4.f, 3.f), F.B.At(-200.f, -60.f + g * 30.f, 61.f), FRotator(0.f, 0.f, 0.f));
	}

	void Descent(FFg& F, AWRFungalScenery* A)
	{
		const FData& D = FData::Get();
		const FData::FSite* S = SiteOf(A);
		if (!S) return;
		const FData::FHall* H = D.FindHall(S->Hall);
		const int32 Sd = int32(GetTypeHash(A->SiteId) % 997u);
		const FName How = S->How;
		// Every descent's waymark: a glow-cap ring at its lip (the Deepfarmer kind).
		const auto RingAt = [&](float R) { const FVector P = F.B.At(R, 0.f); GlowRing(F, P, Sd); };
		if (How == TEXT("spiral_path") && H)
		{
			// D1 the Garden Doline: a spiral path cut into the wall, cap-terraces on every turn (fungal 2.4).
			const float Top = F.B.G(H->Rx + 400.f, 0.f) - 60.f;
			TArray<FVector> Turns;
			Helix(F, FVector::ZeroVector, H->Rx - 30.f, H->At.Z - S->At.Z, Top, 0.f, 260.f, 0.42f, 0, Sd, &Turns);
			for (int32 t = 0; t < Turns.Num(); ++t) LogBed(F, Turns[t] + (FVector(0, 0, 0) - Turns[t]).GetSafeNormal2D() * 130.f + FVector(0, 0, 4.f), FMath::RadiansToDegrees(FMath::Atan2(Turns[t].Y, Turns[t].X)) + 90.f, Sd + t);
			RingAt(H->Rx + 600.f);
			F.B.Lamp(FVector(0.f, 0.f, (H->At.Z - S->At.Z) + 900.f), CyanC(), 3000.f, H->Rx * 2.2f);
		}
		else if (How == TEXT("rope_stair") && H)
		{
			// D3 the Brood Doline: a rope-and-plank stair hung on bronze pins spiralling down its wall.
			const float Top = F.B.G(H->Rx + 400.f, 0.f) - 40.f;
			Helix(F, FVector::ZeroVector, H->Rx - 20.f, H->At.Z - S->At.Z, Top, PI * 0.5f, 160.f, 0.45f, 1, Sd);
			RingAt(H->Rx + 500.f);
		}
		else if (How == TEXT("shelf_shaft"))
		{
			// D4 the Springcap Shaft: shelf fungus down the shaft's wall, shelf to shelf; springcaps ride you up.
			const FData::FShaft* Sh = D.Shafts.FindByPredicate([](const FData::FShaft& Q) { return Q.Id == TEXT("D4"); });
			if (Sh) Helix(F, FVector::ZeroVector, Sh->R - 10.f, Sh->Z0 - S->At.Z + 60.f, -200.f, 0.f, 150.f, 0.f, 2, Sd);
			RingAt(1500.f);
		}
		else if (How == TEXT("great_spiral"))
		{
			// D6 the Great Doline's shaft: a carved ledge spiralling down to the Webs at -80 (a spore cloud every third ledge).
			const FData::FShaft* Sh = D.Shafts.FindByPredicate([](const FData::FShaft& Q) { return Q.Id == TEXT("D6"); });
			if (Sh) Helix(F, FVector::ZeroVector, Sh->R - 25.f, Sh->Z0 - S->At.Z + 40.f, -120.f, PI, 220.f, 0.4f, 0, Sd);
			RingAt(2200.f);
		}
		else if (How == TEXT("long_stair"))
		{
			// D2 the Long Stair's mouth: the fissure's lip, its first treads; the treads down are the tube's furniture.
			F.B.K.Add(Limestone(0.8f), FVector(140.f, 600.f, 80.f), F.B.At(-60.f, -420.f, -20.f), FRotator(0.f, 0.f, 0.f));
			F.B.K.Add(Limestone(0.8f), FVector(140.f, 600.f, 80.f), F.B.At(60.f, -420.f, -20.f), FRotator(0.f, 0.f, 0.f));
			RingAt(700.f);
		}
		else if (How == TEXT("sea_cave"))
		{
			// D5 the Sea Cave: the river leaving the caverns across a shingle bar; drift logs jammed in the mouth.
			for (int32 t = 0; t < 6; ++t) F.B.K.Add(Drift(0.9f), FVector(380.f + 200.f * FgH(Sd, t), 26.f, 22.f), FVector(400.f + t * 140.f, (FgH(Sd, t + 3) - 0.5f) * 700.f, 40.f), FRotator(0.f, 60.f + FgH(Sd, t + 6) * 60.f, 8.f));
			for (int32 r = 0; r < 10; ++r) F.B.Rock(r, FVector(120.f, 100.f, 70.f) * (0.6f + FgH(Sd, r + 10)), FVector(-200.f + r * 120.f, (FgH(Sd, r + 20) - 0.5f) * 1200.f, 0.f), FRotator(0.f, FgH(Sd, r) * 360.f, 0.f), Rgb(150, 146, 134) * 0.55f, true, 20.f);
		}
	}

	// ---------------------------------------------------------------- the hand sites S1-S8

	void Terraces(FFg& F, AWRFungalScenery* A)
	{
		// S1 the Down-Stair Terraces: a farm left in perfect order: log beds in rows, tools hung neatly, doors propped open,
		// a meal laid and never eaten; in the path's flowstone a crowd of bare footprints set hard, every one pointing down.
		const int32 Sd = 41;
		for (int32 r = 0; r < 3; ++r) for (int32 b = 0; b < 2; ++b) LogBed(F, F.B.At(-400.f + r * 320.f, -300.f + b * 600.f), 0.f, Sd + r * 2 + b);
		// The tool rack: sickles, a hoe, baskets, each on its peg.
		const FVector Rk = F.B.At(500.f, -500.f);
		F.B.K.Rod(OldWood(), Rk, Rk + FVector(0, 0, 200.f), 10.f, true);
		F.B.K.Rod(OldWood(), Rk + FVector(0, 260.f, 0.f), Rk + FVector(0, 260.f, 200.f), 10.f, true);
		F.B.K.Rod(OldWood(), Rk + FVector(0, 0, 180.f), Rk + FVector(0, 260.f, 180.f), 8.f);
		for (int32 t = 0; t < 4; ++t) Sickle(F, Rk + FVector(8.f, 40.f + t * 60.f, 176.f), FVector(0, 0, -1), 0.9f);
		CapHouse(F, F.B.At(700.f, 300.f), 200.f, 300.f, 420.f, false, Sd);
		// The meal: a plank table, bowls, a loaf, cups (all whole, untouched, furred faintly).
		const FVector Tb = F.B.At(250.f, 450.f);
		F.B.K.Add(OldWood(), FVector(220.f, 90.f, 8.f), Tb + FVector(0, 0, 78.f));
		for (const float X : { -95.f, 95.f }) for (const float Y : { -35.f, 35.f }) F.B.K.Rod(OldWood(0.9f), Tb + FVector(X, Y, 0.f), Tb + FVector(X, Y, 74.f), 7.f, true);
		F.B.Box(Tb + FVector(0, 0, 40.f), FVector(110.f, 45.f, 40.f));
		for (int32 b = 0; b < 4; ++b)
		{
			F.B.M.Lathe(Tb + FVector(-75.f + b * 50.f, -15.f + (b % 2) * 30.f, 82.f), FVector::UpVector, 8.f, { FVector(0.f, 5.f, 5.f), FVector(1.f, 10.f, 10.f) }, 10, ClayC(), 0.9f);
			F.B.Ball(Tb + FVector(-75.f + b * 50.f, -15.f + (b % 2) * 30.f, 88.f), 5.f, Rgb(120, 96, 60) * 0.5f, 0.95f, 0.5f);
		}
		F.B.Ball(Tb + FVector(0.f, 0.f, 90.f), 12.f, Rgb(170, 130, 80) * 0.5f, 0.9f, 0.6f);
		// The footprints: a flowstone patch on the path down to the doline, every print pointing at it (local -X: the doline).
		for (int32 p = 0; p < 40; ++p)
		{
			const FVector P = F.B.At(-600.f - FgH(Sd, p) * 500.f, -200.f + FgH(Sd, p + 50) * 400.f, 1.f);
			F.B.K.Add(FSurf::Glaze(Rgb(216, 203, 176) * 0.5f, 0.4f), FVector(30.f, 12.f, 2.f), P, FRotator(0.f, 180.f + (FgH(Sd, p + 90) - 0.5f) * 14.f, 0.f));
			F.B.K.Add(FSurf::Stone(Rgb(120, 110, 96) * 0.5f, 0.9f), FVector(22.f, 8.f, 1.f), P + FVector(0, 0, 1.2f), FRotator(0.f, 180.f + (FgH(Sd, p + 90) - 0.5f) * 14.f, 0.f));
		}
	}

	void KnockerCairn(FFg& F, AWRFungalScenery* A)
	{
		// S2: a cairn of clay knocker figures with stick eyes at the head of the Long Stair, all facing down into it (local
		// +Y points down the stair), as if set there to keep something below.
		for (int32 k = 0; k < 24; ++k)
		{
			const int32 Row = k / 8, Col = k % 8;
			const FVector P = F.B.At(-280.f + Col * 80.f, 200.f - Row * 70.f, Row * 26.f);
			Knocker(F, P, FVector(0.f, -1.f, 0.f), 0.9f + 0.25f * FgH(57, k), 57 + k);
		}
		for (int32 s = 0; s < 3; ++s) F.B.K.Add(Limestone(0.9f), FVector(700.f - s * 60.f, 90.f, 26.f), F.B.At(0.f, 230.f - s * 70.f, s * 26.f + 13.f));
	}

	void DryingYard(FFg& F, AWRFungalScenery* A)
	{
		// S3: racks 5 m tall, caps dried to grey paper; at the top of the tallest the first of the summited, jaw locked on
		// the crossbar, a stalk out of the skull, face toward the Ancestor Cap (the actor's yaw faces it).
		for (int32 r = 0; r < 6; ++r)
		{
			const float H = r == 0 ? 560.f : 380.f + 100.f * FgH(61, r);
			const FVector Top = DryingRack(F, F.B.At(-300.f + (r % 3) * 340.f, -260.f + (r / 3) * 520.f), 0.f, H, 61 + r);
			if (r == 0) Body(F, Top + FVector(110.f, 0.f, -8.f), FVector(1.f, 0.f, 0.f), EPose::Rack, 61);
		}
	}

	void GrainTowers(FFg& F, AWRFungalScenery* A)
	{
		// S4: three round granaries on stone feet, empty, their ladders taken down and laid neatly beside them.
		for (int32 g = 0; g < 3; ++g) Granary(F, F.B.At(-500.f + g * 500.f, (g % 2) * 300.f), 220.f, 380.f, 90.f * g, true, 71 + g);
	}

	void PuffballCliffs(FFg& F, AWRFungalScenery* A)
	{
		// S5: the rookery's tethers: cords from the cliff-top fungus shelves out over the sea updraft (the Puffballs
		// themselves are creatures, tethered here: their den). Shelves on the cliff lip, stakes, the cords.
		for (int32 t = 0; t < 10; ++t)
		{
			const FVector P = F.B.At(-200.f + t * 90.f, 400.f * FgH(81, t) - 200.f);
			Shelf(F, P + FVector(0, 0, 20.f), FVector(-1.f, 0.f, 0.f), 60.f + 40.f * FgH(81, t + 10), false, false, 81 + t);
			F.B.K.Rod(OldWood(), P, P + FVector(0, 0, 90.f), 7.f);
			F.B.Line(P + FVector(0, 0, 80.f), P + FVector(-900.f - 500.f * FgH(81, t + 20), 100.f * FgH(81, t + 30), 1400.f), 1.f, 60.f, RopeC(), 12);
		}
	}

	void FallenKite(FFg& F, AWRFungalScenery* A)
	{
		// S6: a Kitewright war-kite crashed nose-first on the pavement, the flyer's bones still strapped in, a Deepfarmer
		// bronze sickle through the ribs: the war reached the surface too.
		Kite(F, F.B.At(0.f, 0.f, 180.f), 0.f, 900.f, true, 91);
		const FVector Ch = F.B.At(-120.f, 0.f, 120.f);
		Bones(F, Ch, 0.f, 0.55f, 91);
		Sickle(F, Ch + FVector(0, 0, 30.f), FVector(0.3f, 0.6f, 0.8f), 1.1f);
		F.B.Line(Ch + FVector(-30.f, -40.f, 20.f), Ch + FVector(60.f, 0.f, 80.f), 1.6f, 4.f, Rgb(120, 96, 70) * 0.5f);
		F.B.Box(F.B.At(0.f, 0.f, 120.f), FVector(300.f, 200.f, 120.f));
	}

	void BreathVents(FFg& F, AWRFungalScenery* A)
	{
		// S8: a field of fissures round the Great Doline breathing warm, spore-laden air out and in on a slow cycle (60 s
		// out, 60 s in: real cave breathing); bones in the fissures where deepcaps grow (the nodes). The crust is the land's.
		for (int32 k = 0; k < 30; ++k)
		{
			const float Ang = 2.f * PI * FgH(101, k), R = 5200.f * FMath::Sqrt(FgH(101, k + 40));
			const FVector P = F.B.At(FMath::Cos(Ang) * R, FMath::Sin(Ang) * R, -4.f);
			const float L = 120.f + 300.f * FgH(101, k + 80);
			F.B.K.Add(FSurf::Stone(Rgb(14, 14, 12) * 0.6f, 0.95f), FVector(L, 30.f + 60.f * FgH(101, k + 120), 26.f), P, FRotator(0.f, FgH(101, k + 160) * 180.f, 0.f));
			F.Glow.Lathe(P + FVector(0, 0, 3.f), FVector::UpVector, 3.f, { FVector(0.f, 18.f, L * 0.48f), FVector(1.f, 12.f, L * 0.44f) }, 6, CyanC(), 0.4f);
			if (k % 3 == 0) F.B.Loop(TEXT("fungal.breathVent"), P + FVector(0, 0, 20.f), FRotator::ZeroRotator, 0.7f);
		}
		for (int32 b = 0; b < 3; ++b) Bones(F, F.B.At(FMath::Cos(b * 2.3f) * 3000.f, FMath::Sin(b * 2.3f) * 3000.f, 2.f), b * 70.f, 0.8f + 0.3f * b, 103 + b);
	}

	// ---------------------------------------------------------------- the surface kit and the plateau's decoration

	void SurfaceKit(FFg& F, AWRFungalScenery* A)
	{
		const FData& D = FData::Get();
		if (!D.Kit.IsValidIndex(A->Index)) return;
		const FData::FKit& K = D.Kit[A->Index];
		const int32 Sd = A->Index * 13 + 5;
		if (K.Kind == TEXT("glowcap_ring")) GlowRing(F, F.B.At(0.f, 0.f), Sd);
		else if (K.Kind == TEXT("host_cluster")) Bones(F, F.B.At(0.f, 0.f, 2.f), FgH(Sd, 1) * 360.f, 0.9f + 0.4f * K.V, Sd);
		else if (K.Kind == TEXT("puffball_field"))
		{
			for (int32 t = 0; t < 8; ++t) { const FVector P = F.B.At((FgH(Sd, t) - 0.5f) * 1400.f, (FgH(Sd, t + 9) - 0.5f) * 1400.f); F.B.K.Rod(OldWood(), P, P + FVector(0, 0, 70.f), 6.f); Mushroom(F, P + FVector(40.f, 0.f, 0.f), FVector::UpVector, 30.f, 50.f, ECap::Puff, 0.f, Sd + t, false); }
		}
		else if (K.Kind == TEXT("summited_niche"))
		{
			// A Deepfarmer clamped on a root over a doline's lip, face toward the Heart; a cache below it (the world cache).
			const FVector Root = F.B.At(0.f, 0.f, 260.f);
			F.B.Root({ F.B.At(-300.f, 0.f, -20.f), F.B.At(-120.f, 0.f, 120.f), Root, Root + FVector(160.f, 0.f, -40.f) }, 26.f, 10.f, RhizoC());
			Body(F, Root + FVector(60.f, 0.f, -10.f), FVector(1.f, 0.f, 0.f), EPose::Beam, Sd);
		}
		else if (K.Kind == TEXT("mite_nest"))
		{
			// Egg balls on a boulder's shaded face: pale clusters of beads 20-40 cm, the Mycomites' (steal one to tame).
			F.B.Rock(Sd, FVector(380.f, 300.f, 220.f), F.B.At(0.f, 0.f), FRotator(0.f, FgH(Sd, 1) * 360.f, 0.f), Rgb(150, 146, 134) * 0.55f, true, 30.f);
			for (int32 e = 0; e < 9; ++e) F.B.Ball(F.B.At(-160.f + (e % 3) * 30.f, -60.f + (e / 3) * 50.f, 60.f + (e % 2) * 40.f), 14.f + 8.f * FgH(Sd, e), Rgb(220, 214, 196) * 0.6f, 0.6f, 0.9f);
		}
		else if (K.Kind == TEXT("capling_meadow"))
		{
			for (int32 c = 0; c < 26; ++c) { const FVector P = F.B.At((FgH(Sd, c) - 0.5f) * 1600.f, (FgH(Sd, c + 30) - 0.5f) * 1600.f); if (c % 4 == 0) F.B.Tree(TEXT("fern"), c, P, 60.f + 40.f * FgH(Sd, c), FRotator(0.f, FgH(Sd, c) * 360.f, 0.f), 0.f); else Mushroom(F, P, FVector::UpVector, 12.f + 20.f * FgH(Sd, c + 60), 8.f + 12.f * FgH(Sd, c + 90), ECap::Honey, 0.3f, Sd + c, false, false, nullptr); }
		}
		else if (K.Kind == TEXT("shelf_deadwood"))
		{
			// A fallen juniper furred with shelf fungus (amadou), foxfire glowing in its rotten heart at night (fungal 1.2).
			F.B.Tree(TEXT("foxfire_log"), Sd, F.B.At(0.f, 0.f), 600.f, FRotator(0.f, FgH(Sd, 1) * 360.f, 0.f), 0.f);
			for (int32 s = 0; s < 6; ++s) Shelf(F, F.B.At(-200.f + s * 80.f, 30.f, 40.f), FVector(0.f, 1.f, 0.f), 25.f + 15.f * FgH(Sd, s), false, false, Sd + s);
			for (int32 s = 0; s < 5; ++s) F.B.N.Add(FoxfireGlow(1.4f), FVector(120.f, 12.f, 8.f), F.B.At(-220.f + s * 100.f, -10.f, 30.f), FRotator(0.f, FgH(Sd, s + 20) * 30.f, 0.f));
			F.B.Lamp(F.B.At(0.f, 0.f, 80.f), FoxC(), 500.f, 800.f);
		}
		else if (K.Kind == TEXT("doline_lookout"))
		{
			// A rim ledge with a view straight down: a carved stone seat and a cairn at the very lip.
			F.B.K.Add(Carved(), FVector(180.f, 50.f, 45.f), F.B.At(0.f, 0.f, 22.f));
			F.B.Box(F.B.At(0.f, 0.f, 22.f), FVector(90.f, 25.f, 22.f));
			for (int32 k = 0; k < 5; ++k) F.B.K.Add(Limestone(0.9f), FVector(50.f - k * 8.f, 44.f - k * 7.f, 22.f), F.B.At(-150.f, 120.f, 11.f + k * 21.f), FRotator(0.f, k * 41.f, 0.f));
		}
	}

	void Cell(FFg& F, AWRFungalScenery* A)
	{
		// The plateau's decoration (build cook 2.1.7): wind-bent juniper leaning away from the west wind, mossy limestone
		// boulders, salt-grass tufts, shelf-fungus deadwood; none on the paths, the camps, the sites or a doline's throat.
		const FWRLandDef* L = FData::Land();
		const FData& D = FData::Get();
		if (!L) return;
		const int32 Sd = A->Index * 7 + 3;
		const FVector C0 = A->GetActorLocation();
		int32 Made = 0;
		for (int32 q = 0; q < 70 && Made < 40; ++q)
		{
			const FVector W = C0 + FVector((FgH(Sd, q) - 0.5f) * 20000.f, (FgH(Sd, q + 300) - 0.5f) * 20000.f, 0.f);
			const FVector2D M = L->ToMap(W);
			if (L->HfHiddenAt(M) || L->CoastDistance(M) < 15.f) continue;
			const TCHAR Z = L->GridLetter(M);
			if (Z != TEXT('1') && Z != TEXT('d')) continue;
			bool bClear = true;
			for (const FData::FHall& H : D.Halls) if (H.IsDoline() && FVector2D::Distance(FVector2D(W), FVector2D(H.At)) < H.Rx + 900.f) { bClear = false; break; }
			for (const FData::FSite& S : D.Sites) if (!S.bCave && FVector2D::Distance(FVector2D(W), FVector2D(S.At)) < (S.bLandmark ? FMath::Max(S.R, 3000.f) : 2500.f)) { bClear = false; break; }
			for (const FData::FRoute& R : D.Routes) if (WRLand::DistToPolyline(FVector2D(W), R.Pts) < 400.f) { bClear = false; break; }
			if (!bClear) continue;
			if (FVector2D::Distance(FVector2D(W), FVector2D(D.GreatAt)) < D.GreatR + 1500.f) continue;
			const FVector G = F.B.At(F.B.Local(W).X, F.B.Local(W).Y);
			const float N = FgH(Sd, q + 600);
			if (Z == TEXT('d'))
			{
				if (N > 0.7f) F.B.Rock(q, FVector(160.f, 140.f, 80.f) * (0.6f + N), G, FRotator(0.f, N * 360.f, 0.f), Rgb(196, 170, 120) * 0.6f, true, 30.f);
				continue;
			}
			if (N < 0.22f)
			{
				// Juniper, bent from the west (the sea wind): lean 10-25 degrees toward +X.
				F.B.Tree(TEXT("juniper"), q, G, 280.f + 380.f * FgH(Sd, q + 900), FRotator(-(10.f + 15.f * FgH(Sd, q + 1200)), 0.f, 0.f), -1.f);
			}
			else if (N < 0.4f)
			{
				F.B.Tree(TEXT("mossy_boulder"), q, G, 120.f + 180.f * FgH(Sd, q + 900), FRotator(0.f, FgH(Sd, q + 1500) * 360.f, 0.f), 100.f);
			}
			else if (N < 0.5f)
			{
				F.B.Rock(q, FVector(200.f, 170.f, 110.f) * (0.6f + FgH(Sd, q + 700)), G, FRotator(0.f, FgH(Sd, q + 800) * 360.f, 0.f), Rgb(167, 164, 152) * 0.62f, true, 30.f);
			}
			else if (N < 0.56f)
			{
				F.B.Tree(TEXT("fallen"), q, G, 500.f, FRotator(0.f, FgH(Sd, q + 1100) * 360.f, 0.f), 0.f);
				for (int32 s = 0; s < 3; ++s) Shelf(F, G + FVector(-100.f + s * 90.f, 20.f, 30.f), FVector(0.f, 1.f, 0.f), 22.f, false, false, q + s);
				F.B.N.Add(FoxfireGlow(1.2f), FVector(140.f, 10.f, 8.f), G + FVector(0, 0, 24.f), FRotator(0.f, FgH(Sd, q) * 180.f, 0.f));
			}
			else
			{
				F.B.Tree(TEXT("fern"), q, G, 50.f + 40.f * FgH(Sd, q + 900), FRotator(0.f, FgH(Sd, q + 1500) * 360.f, 0.f), 0.f);
			}
			++Made;
		}
	}
}

namespace WRFungalUnder
{
	bool Build(WRFungalLook::FFg& F, AWRFungalScenery* A);
}

void AWRFungalScenery::BuildFungal()
{
	using namespace WRFungalSurf;
	const FName K = Kind;
	const bool bLandmark = K == TEXT("ancestor_cap") || K == TEXT("kneeling_titan") || K == TEXT("ring_of_caps");
	const float Cull = bLandmark ? 0.f
		: bUnder ? WRTune(TEXT("fungalWorld.underCull"), 24000.f)
		: K == TEXT("cell") || K == TEXT("glowcap_ring") ? WRTune(TEXT("fungalWorld.decorCull"), 26000.f)
		: WRTune(TEXT("fungalWorld.siteCull"), 60000.f);
	WRForestLook::FBuild B(this, Cull);
	WRFungalLook::FFg F(B);
	bool bBuilt = true;
	if (K == TEXT("camp")) Camp(F, this);
	else if (K == TEXT("ancestor_cap")) AncestorCap(F, this);
	else if (K == TEXT("kneeling_titan")) KneelingTitan(F, this);
	else if (K == TEXT("ring_of_caps")) RingOfCaps(F, this);
	else if (K == TEXT("ringstones")) Ringstones(F, this);
	else if (K == TEXT("descent")) Descent(F, this);
	else if (K == TEXT("down_stair_terraces")) Terraces(F, this);
	else if (K == TEXT("knocker_cairn")) KnockerCairn(F, this);
	else if (K == TEXT("drying_yard")) DryingYard(F, this);
	else if (K == TEXT("grain_towers")) GrainTowers(F, this);
	else if (K == TEXT("puffball_cliffs")) PuffballCliffs(F, this);
	else if (K == TEXT("fallen_kite")) FallenKite(F, this);
	else if (K == TEXT("breath_vents")) BreathVents(F, this);
	else if (K == TEXT("cell")) Cell(F, this);
	else if (!bUnder && FWRFungalData::Get().Kit.IsValidIndex(Index) && FWRFungalData::Get().Kit[Index].bSurface && FWRFungalData::Get().Kit[Index].Id == SiteId) SurfaceKit(F, this);
	else bBuilt = WRFungalUnder::Build(F, this);
	if (!bBuilt)
	{
		UE_LOG(LogWildreach, Warning, TEXT("FUNGAL scenery: no builder for kind %s (%s)"), *K.ToString(), *SiteId.ToString());
		return;
	}
	F.Finish();
	if (Cull > 0.f && GetNetMode() != NM_DedicatedServer)
	{
		for (UActorComponent* Cmp : GetComponents())
		{
			if (UInstancedStaticMeshComponent* I = Cast<UInstancedStaticMeshComponent>(Cmp)) I->SetCullDistances(0, int32(Cull));
		}
	}
}
