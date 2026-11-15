// The Fungal caverns' furniture (cooks/biome-fungal.md 2.2-2.5, 2.20; cooks/build-biome-fungal.md 2.5-2.6): every hall
// by its sub-zone (the Gardens' giant caps in their daylight, the Halls' stalagmites, flowstone and glow-moss, the Webs'
// rhizomorph cables and mycelium sheets, the Grottos' rimstone and lakes, the Heart Grotto's grown city), the cavern
// landmarks (the False Sky, the Shoestring Bridge, the Drowned Granary, the Sand Falls, the Fall Field and its champion),
// the cavern hand sites C1-C17, the Heart Grotto's ten rooms and the Heart chamber's terraces, the cave kit and the
// tubes' furniture (the Long Stair's treads and knocker idols, the city's lamps). Each hall's far light stands at its far
// end (fungal 2.1 read rules: you always have a direction). Things stand on the cavern's own floor (local z 0 is the
// hall's floor), never on a trace up to the plateau; walls and ceilings are found in the caverns' air (FgWall).

#include "WRFungalWorld.h"
#include "WRFungalLook.h"
#include "WRFungalTypes.h"
#include "Wildreach.h"

namespace WRFungalUnder
{
	bool Build(WRFungalLook::FFg& F, AWRFungalScenery* A);
}

namespace WRFungalUnderImpl
{
	using namespace WRFungalLook;
	using FData = FWRFungalData;

	/** Where the rock begins from P along Dir (searched to MaxDist in 60 cm steps, then bisected), and its normal into
	 *  the air. False when there is no rock that way within reach. */
	bool FgWall(const FData& D, const FVector& P, const FVector& Dir, float MaxDist, FVector& Out, FVector* OutN = nullptr)
	{
		const FVector U = Dir.GetSafeNormal();
		float Lo = 0.f, Hi = -1.f;
		for (float T = 30.f; T < MaxDist; T += 60.f)
		{
			if (D.Air(P + U * T) >= 0.f) { Hi = T; break; }
			Lo = T;
		}
		if (Hi < 0.f) return false;
		for (int32 b = 0; b < 7; ++b) { const float M = (Lo + Hi) * 0.5f; (D.Air(P + U * M) >= 0.f ? Hi : Lo) = M; }
		Out = P + U * Lo;
		if (OutN)
		{
			const FVector G(D.Air(Out + FVector(30, 0, 0)) - D.Air(Out - FVector(30, 0, 0)), D.Air(Out + FVector(0, 30, 0)) - D.Air(Out - FVector(0, 30, 0)), D.Air(Out + FVector(0, 0, 30)) - D.Air(Out - FVector(0, 0, 30)));
			*OutN = (-G).GetSafeNormal();
		}
		return true;
	}

	const FData::FHall* HallOf(const AWRFungalScenery* A) { const FData& D = FData::Get(); return D.Halls.IsValidIndex(A->Index) && A->Kind == TEXT("hall") ? &D.Halls[A->Index] : D.FindHall(A->SiteId); }

	/** A stalagmite (and its stalactite above when Pair): a lathed cone of flowstone with growth rings. */
	void Stalagmite(FFg& F, const FVector& Base, float H, float R, int32 Seed)
	{
		F.B.M.Lathe(Base - FVector(0, 0, 20.f), FVector::UpVector, H + 20.f, { FVector(0.f, R * 1.3f, R * 1.3f), FVector(0.15f, R, R), FVector(0.5f, R * 0.7f, R * 0.7f), FVector(0.85f, R * 0.3f, R * 0.3f), FVector(1.f, R * 0.08f, R * 0.08f) }, 10, Rgb(216, 203, 176) * 0.58f, 0.5f, (FgH(Seed, 1) - 0.5f) * 0.15f, float(Seed % 17 + 1));
		if (H > 150.f) F.B.SegBox(Base, Base + FVector(0, 0, H * 0.6f), R * 0.7f, R * 0.7f);
	}

	/** A glow-moss mound on a hall floor (a cushion of moss, cyan at night). */
	void MossMound(FFg& F, const FVector& At, float R, int32 Seed)
	{
		F.Glow.Lathe(At - FVector(0, 0, 8.f), FVector::UpVector, R * 0.28f, { FVector(0.f, R, R * 0.8f), FVector(0.5f, R * 0.8f, R * 0.62f), FVector(1.f, R * 0.15f, R * 0.12f) }, 12, CyanC(), 0.65f, 0.f, float(Seed % 11 + 1));
	}

	/** A mycelium sheet like a torn sail between points (fungal 2.2 F4): a see-through quad strip. */
	void Sheet(FFg& F, const FVector& A, const FVector& B, const FVector& C, const FVector& E)
	{
		TArray<TArray<FVector>> Rings;
		for (int32 r = 0; r <= 4; ++r)
		{
			const float T = r / 4.f;
			TArray<FVector> Ring;
			const FVector L = FMath::Lerp(A, E, T) - FVector(0, 0, 60.f * FMath::Sin(T * PI)), Rr = FMath::Lerp(B, C, T) - FVector(0, 0, 60.f * FMath::Sin(T * PI));
			for (int32 s = 0; s <= 4; ++s) Ring.Add(FMath::Lerp(L, Rr, s / 4.f) - FVector(0, 0, 90.f * FMath::Sin(s / 4.f * PI) * FMath::Sin(T * PI)));
			Rings.Add(Ring);
		}
		F.Veil.Loft(Rings, Rgb(232, 228, 218), 0.15f, false);
	}

	/** A far light: one point light at the hall's far end (fungal 2.1: every hall has one bright source). */
	void FarLight(FFg& F, const FData::FHall& H, const FVector& Local, const FLinearColor& C, float Power)
	{
		F.B.Lamp(Local, C, Power, FMath::Clamp(FMath::Max(H.Rx, H.Ry) * 1.6f, 1500.f, 24000.f));
	}

	// ---------------------------------------------------------------- the halls by zone

	void GardenHall(FFg& F, AWRFungalScenery* A, const FData::FHall& H)
	{
		// F2: a jungle of giant caps 4-25 m tall where the daylight lands (parasol, ink cap, honey clusters), ferns and moss
		// where the light falls, oyster shelves on the walls; springcaps in the Grove are actors (the hazards).
		const FData& D = FData::Get();
		const int32 Sd = A->Index * 17 + 3;
		const float R = FMath::Min(H.Rx, H.Ry);
		const int32 N = H.Site == TEXT("C3") ? 6 : FMath::Clamp(int32(R / 300.f), 5, 18);
		for (int32 c = 0; c < N; ++c)
		{
			const float Ang = 2.f * PI * (c + 0.4f * FgH(Sd, c)) / N, Rr = R * (0.35f + 0.45f * FgH(Sd, c + 30));
			const FVector P(FMath::Cos(Ang) * Rr, FMath::Sin(Ang) * Rr, -8.f);
			const float Ht = 400.f + 2100.f * FMath::Square(FgH(Sd, c + 60));
			const ECap K = c % 4 == 0 ? ECap::Ink : c % 4 == 1 ? ECap::Honey : ECap::Parasol;
			if (K == ECap::Honey)
			{
				// A honey cluster: five caps from one root.
				for (int32 h = 0; h < 5; ++h) Mushroom(F, P + FVector(FMath::Cos(h * 1.3f) * 60.f, FMath::Sin(h * 1.3f) * 60.f, 0.f), FVector(FMath::Cos(h * 1.3f) * 0.2f, FMath::Sin(h * 1.3f) * 0.2f, 1.f), Ht * (0.5f + 0.12f * h), Ht * 0.12f, ECap::Honey, 0.35f, Sd + c * 7 + h, h == 0);
			}
			else Mushroom(F, P, FVector::UpVector, Ht, Ht * (K == ECap::Ink ? 0.14f : 0.3f), K, 0.5f, Sd + c, true);
		}
		for (int32 f = 0; f < 18; ++f)
		{
			const float Ang = 2.f * PI * FgH(Sd, f + 100), Rr = R * 0.85f * FMath::Sqrt(FgH(Sd, f + 130));
			const FVector P(FMath::Cos(Ang) * Rr, FMath::Sin(Ang) * Rr, -6.f);
			if (f % 2) F.B.Tree(TEXT("fern"), f, P, 80.f + 60.f * FgH(Sd, f + 160), FRotator(0.f, FgH(Sd, f) * 360.f, 0.f), 0.f);
			else Mushroom(F, P, FVector::UpVector, 25.f + 40.f * FgH(Sd, f + 190), 15.f + 20.f * FgH(Sd, f + 220), ECap::Glowcap, 0.6f, Sd + f + 300, false);
		}
		// Oyster shelves round the walls (the cook's "oyster shelves"), at 2-12 m.
		for (int32 s = 0; s < 10; ++s)
		{
			const float Ang = 2.f * PI * FgH(Sd, s + 400);
			FVector W, Nr;
			if (FgWall(D, H.At + FVector(0, 0, 200.f + 1000.f * FgH(Sd, s + 430)), FVector(FMath::Cos(Ang), FMath::Sin(Ang), 0.f), R * 1.3f, W, &Nr))
				Shelf(F, F.B.Local(W), Nr, 60.f + 90.f * FgH(Sd, s + 460), false, true, Sd + s);
		}
		if (H.IsDoline())
		{
			// Daylight down the shaft: a warm light by day (a night light at the mouth goes cyan from below: the doline glow).
			F.B.Lamp(FVector(0.f, 0.f, (H.Top - H.At.Z) * 0.5f), CyanC(), 2500.f, R * 2.5f);
			F.B.Loop(TEXT("fungal.motes"), FVector(0.f, 0.f, 800.f), FRotator::ZeroRotator, R / 1500.f);
		}
		else FarLight(F, H, FVector(R * 0.8f, 0.f, H.H * 0.4f), GoldC(), 4000.f);
	}

	void HallsHall(FFg& F, AWRFungalScenery* A, const FData::FHall& H)
	{
		// F3: stalagmite forests, shelf-fungus stairs up the walls, glow-moss mounds on the carpet, Glowgrub colonies'
		// silk on the ceilings. The far light is the hall's own: the Sand Falls' sun, a lamp in the Archive, moss.
		const FData& D = FData::Get();
		const int32 Sd = A->Index * 19 + 5;
		const float R = FMath::Min(H.Rx, H.Ry);
		const int32 NS = FMath::Clamp(int32(H.Rx * H.Ry / 400000.f), 4, 70);
		for (int32 s = 0; s < NS; ++s)
		{
			const FVector2D L(FMath::Cos(2.f * PI * FgH(Sd, s)) * H.Rx * 0.85f * FMath::Sqrt(FgH(Sd, s + 70)), FMath::Sin(2.f * PI * FgH(Sd, s)) * H.Ry * 0.85f * FMath::Sqrt(FgH(Sd, s + 70)));
			const FVector P(L.X, L.Y, -10.f);
			if (H.Landmark == TEXT("L4") && FMath::Abs(L.Y) < 900.f) continue;   // the False Sky's route down its middle stays clear
			const float Hs = 120.f + 900.f * FMath::Square(FgH(Sd, s + 140));
			Stalagmite(F, P, Hs, Hs * 0.16f + 20.f, Sd + s);
			if (s % 3 == 0) MossMound(F, P + FVector(160.f, 40.f, 0.f), 120.f + 120.f * FgH(Sd, s + 210), Sd + s);
		}
		// Shelf-fungus stairs up one wall to a ledge (the Halls' climbing way: shelves you jump up, 1.1 m apart).
		{
			const float Ang = 2.f * PI * FgH(Sd, 500);
			for (int32 k = 0; k < 9; ++k)
			{
				FVector W, Nr;
				const float Z = 110.f * (k + 1);
				if (Z > H.H * 0.6f) break;
				if (FgWall(D, H.At + FVector(0, 0, Z), FVector(FMath::Cos(Ang + k * 0.06f), FMath::Sin(Ang + k * 0.06f), 0.f), FMath::Max(H.Rx, H.Ry) * 1.3f, W, &Nr))
					Shelf(F, F.B.Local(W), Nr, 90.f, true, false, Sd + k);
			}
		}
		const FLinearColor Far = H.Light == TEXT("sand") ? GoldC() : H.Light == TEXT("lamp") ? FireLight() : CyanC();
		FarLight(F, H, FVector(H.Rx * 0.8f, 0.f, FMath::Min(H.H * 0.4f, 800.f)), Far, H.Light == TEXT("none") ? 0.f : 3500.f);
	}

	void WebsHall(FFg& F, AWRFungalScenery* A, const FData::FHall& H)
	{
		// F4: black rhizomorph cables crossing the space (scenery: the walkable ones are the bridges), white mycelium sheets
		// like torn sails between them, silk; the floor's talus far below a chasm's walkway. Amber light leaks up from the
		// Heart Grotto's fissures: the far light is amber, low.
		const FData& D = FData::Get();
		const int32 Sd = A->Index * 23 + 7;
		const float WalkZ = H.Walk != 0.f ? H.Walk - H.At.Z : 0.f;
		const int32 NC = H.IsChasm() ? 10 : 6;
		TArray<TPair<FVector, FVector>> Spans;
		for (int32 c = 0; c < NC; ++c)
		{
			const float Ang = 2.f * PI * FgH(Sd, c);
			const float Z = WalkZ + (H.IsChasm() ? 300.f + 2800.f * FgH(Sd, c + 20) : 200.f + H.H * 0.6f * FgH(Sd, c + 20));
			FVector W0, W1;
			if (!FgWall(D, H.At + FVector(0, 0, Z), FVector(FMath::Cos(Ang), FMath::Sin(Ang), 0.f), FMath::Max(H.Rx, H.Ry) * 1.4f, W0)) continue;
			if (!FgWall(D, H.At + FVector(0, 0, Z + 300.f * (FgH(Sd, c + 40) - 0.5f)), -FVector(FMath::Cos(Ang + 0.4f * (FgH(Sd, c + 60) - 0.5f)), FMath::Sin(Ang + 0.4f * (FgH(Sd, c + 60) - 0.5f)), 0.f), FMath::Max(H.Rx, H.Ry) * 1.4f, W1)) continue;
			const FVector A0 = F.B.Local(W0), A1 = F.B.Local(W1);
			TArray<FVector> Pts;
			const float Sag = FVector::Dist(A0, A1) * 0.08f;
			for (int32 k = 0; k <= 10; ++k) Pts.Add(WRForestLook::Sag(A0, A1, k / 10.f, Sag));
			Cable(F, Pts, 30.f + 90.f * FgH(Sd, c + 80), false, FgH(Sd, c + 100) > 0.5f, Sd + c);
			Spans.Add(TPair<FVector, FVector>(A0, A1));
		}
		for (int32 s = 0; s + 1 < Spans.Num(); s += 2)
		{
			const FVector A0 = FMath::Lerp(Spans[s].Key, Spans[s].Value, 0.3f), A1 = FMath::Lerp(Spans[s].Key, Spans[s].Value, 0.6f);
			const FVector B0 = FMath::Lerp(Spans[s + 1].Key, Spans[s + 1].Value, 0.3f), B1 = FMath::Lerp(Spans[s + 1].Key, Spans[s + 1].Value, 0.6f);
			if (FVector::Dist(A0, B0) < 4000.f) Sheet(F, A0, A1, B1, B0);
		}
		// Hanging rhizomorphs from the ceiling (the Root Ceiling's forest is its own site; a few here).
		for (int32 h = 0; h < 8; ++h)
		{
			const FVector P = H.At + FVector((FgH(Sd, h + 200) - 0.5f) * H.Rx * 1.4f, (FgH(Sd, h + 230) - 0.5f) * H.Ry * 1.4f, WalkZ + 200.f);
			FVector Top;
			if (!FgWall(D, P, FVector::UpVector, H.H * 1.5f, Top)) continue;
			const FVector T = F.B.Local(Top);
			const float Len = 400.f + 1500.f * FgH(Sd, h + 260);
			F.B.M.Tube({ T + FVector(0, 0, 40.f), T - FVector(0, 0, Len * 0.5f) + FVector(30.f, 0, 0), T - FVector(0, 0, Len) }, 10.f + 14.f * FgH(Sd, h + 290), 6, RhizoC(), 0.35f);
		}
		FarLight(F, H, FVector(0.f, 0.f, -200.f), AmberC(), 3000.f);
	}

	void GrottoHall(FFg& F, AWRFungalScenery* A, const FData::FHall& H)
	{
		// F5: rimstone dams terracing the floor (cream rings of calcite, water standing behind each), clear teal water over
		// white sand (the lake's surface: a glaze sheet with the teal glow under it), the waterline's glowing band.
		const int32 Sd = A->Index * 29 + 9;
		const float R = FMath::Min(H.Rx, H.Ry);
		if (H.Lake > 0.f)
		{
			// The lake's surface: an ellipse at the water's level, a lit teal sheet just under it (Glowfin light).
			TArray<FVector> Prof = { FVector(0.f, H.Rx * 0.98f, H.Ry * 0.98f), FVector(1.f, H.Rx * 0.98f, H.Ry * 0.98f) };
			const FVector Wz(0, 0, H.Lake);
			F.B.M.Lathe(Wz - FVector(0, 0, 1.f), FVector::UpVector, 2.f, Prof, 32, Rgb(30, 140, 134) * 0.35f, 0.04f);
			F.Glow.Lathe(Wz - FVector(0, 0, 60.f), FVector::UpVector, 2.f, { FVector(0.f, H.Rx * 0.8f, H.Ry * 0.8f), FVector(1.f, H.Rx * 0.8f, H.Ry * 0.8f) }, 24, TealC(), 0.35f);
		}
		for (int32 d = 0; d < FMath::Clamp(int32(R / 500.f), 3, 10); ++d)
		{
			// A rimstone pool: a scalloped cream ring standing 30-60 cm, still water inside it.
			const float Ang = 2.f * PI * FgH(Sd, d), Rr = R * (0.5f + 0.35f * FgH(Sd, d + 20));
			const FVector C(FMath::Cos(Ang) * Rr * (H.Rx / R), FMath::Sin(Ang) * Rr * (H.Ry / R), H.Lake > 0.f ? H.Lake - 20.f : -4.f);
			const float PR = 150.f + 250.f * FgH(Sd, d + 40);
			F.B.M.Lathe(C - FVector(0, 0, 10.f), FVector::UpVector, 50.f, { FVector(0.f, PR * 1.1f, PR * 1.1f), FVector(0.7f, PR, PR), FVector(1.f, PR * 0.96f, PR * 0.96f) }, 18, Rgb(224, 212, 188) * 0.62f, 0.4f, 0.f, float(Sd % 13 + d));
			F.B.M.Lathe(C + FVector(0, 0, 32.f), FVector::UpVector, 2.f, { FVector(0.f, PR * 0.9f, PR * 0.9f), FVector(1.f, PR * 0.9f, PR * 0.9f) }, 18, Rgb(30, 140, 134) * 0.35f, 0.04f);
		}
		FarLight(F, H, FVector(H.Rx * 0.7f, 0.f, 300.f), TealC(), 3000.f);
	}

	// ---------------------------------------------------------------- the cavern landmarks

	void FalseSky(FFg& F, AWRFungalScenery* A)
	{
		// L4 (fungal 2.3): Starfall Hall's ceiling 70 m up covered in glowing beaded lure threads, a "milky way" along a
		// ceiling crack; the dark star patch (F-S3) has none. The low threads that grab are the hazard's (WRFungalHazards);
		// these never hang below 9 m. One glow mesh: each thread a hair-thin strand with three beads.
		const FData& D = FData::Get();
		const FData::FHall* H = D.FindHall(TEXT("H1"));
		if (!H) return;
		const FData::FLineSet* LS = D.Lines.FindByPredicate([](const FData::FLineSet& L) { return L.Hall == TEXT("H1"); });
		const int32 Stars = LS ? LS->Stars : 4000;
		const FData::FSecret* Dark = D.Secrets.FindByPredicate([](const FData::FSecret& S) { return S.Id == TEXT("F-S3"); });
		FRandomStream R(8821);
		int32 Made = 0;
		for (int32 s = 0; s < Stars * 3 && Made < Stars; ++s)
		{
			const float U = R.FRandRange(-1.f, 1.f), V = R.FRandRange(-1.f, 1.f);
			// The milky way: a band along a crack (local x) is three times denser.
			const bool bBand = FMath::Abs(V - 0.25f * FMath::Sin(U * 3.f)) < 0.12f;
			if (!bBand && R.FRand() > 0.38f) continue;
			if (U * U + V * V > 0.92f) continue;
			const FVector P = H->At + FRotator(0.f, H->Yaw, 0.f).RotateVector(FVector(U * H->Rx * 0.95f, V * H->Ry * 0.95f, 400.f));
			if (Dark && FVector::Dist2D(P, Dark->At) < 1800.f) continue;
			FVector Top;
			if (!FgWall(D, P, FVector::UpVector, H->H * 1.3f, Top)) continue;
			if (Top.Z - H->At.Z < 1400.f) continue;
			const float Len = FMath::Min(R.FRandRange(150.f, 1100.f), Top.Z - H->At.Z - 900.f);
			const FVector T = F.B.Local(Top) + FVector(0, 0, 20.f);
			const FVector Bt = T - FVector(0, 0, Len);
			const FLinearColor C = StarC();
			F.Glow.Tri(T + FVector(1.2f, 0.f, 0.f), T - FVector(1.2f, 0.f, 0.f), Bt, FWRToolMesh::Pack(C, 0.6f), FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector);
			for (int32 b = 0; b < 3; ++b)
			{
				const FVector Bd = FMath::Lerp(T, Bt, 0.4f + 0.3f * b);
				const float Bs = 3.f + 2.f * b;
				const FColor Bc = FWRToolMesh::Pack(C, 1.f);
				F.Glow.Tri(Bd + FVector(0, 0, Bs), Bd + FVector(Bs, 0, 0), Bd - FVector(0, 0, Bs), Bc, FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector);
				F.Glow.Tri(Bd + FVector(0, 0, Bs), Bd + FVector(0, Bs, 0), Bd - FVector(0, 0, Bs), Bc, FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector);
			}
			++Made;
		}
		// The dark star's way up (F-S3): shelf-fungus stairs up the wall nearest it to its dry ledge.
		if (Dark)
		{
			FVector W, Nr;
			const FVector Toward = (Dark->At - H->At).GetSafeNormal2D();
			for (int32 k = 0; k < 46; ++k)
			{
				const float Z = 110.f * (k + 1);
				if (Z > Dark->At.Z - H->At.Z) break;
				const FVector Dir = Toward.RotateAngleAxis(-25.f + k * 1.2f, FVector::UpVector);
				if (FgWall(D, H->At + FVector(0, 0, Z) + Toward * H->Rx * 0.5f, Dir, H->Rx, W, &Nr)) Shelf(F, F.B.Local(W), Nr, 100.f, true, k % 4 == 0, 600 + k);
			}
			const FVector L = F.B.Local(Dark->At);
			F.B.Box(L - FVector(0, 0, 20.f), FVector(260.f, 200.f, 20.f));
			F.B.K.Add(Flowstone(0.9f), FVector(520.f, 400.f, 40.f), L - FVector(0, 0, 20.f));
		}
	}

	void Shoestring(FFg& F, AWRFungalScenery* A)
	{
		// L5 (fungal 2.3): one black rhizomorph cable 3 m thick spanning the chasm in a sagging curve, wrapped in silk, the
		// smaller cables fanning off it to the walls like a suspension bridge's hangers; the walkway along its top.
		const FData& D = FData::Get();
		const FData::FHall* H = D.FindHall(TEXT("W1"));
		if (!H) return;
		const float Walk = H->Walk;
		const FVector W0 = F.B.Local(FVector(H->At.X - H->Rx * 0.97f, H->At.Y, Walk - 120.f)), W1 = F.B.Local(FVector(H->At.X + H->Rx * 0.97f, H->At.Y, Walk - 120.f));
		TArray<FVector> Pts;
		for (int32 k = 0; k <= 24; ++k) Pts.Add(WRForestLook::Sag(W0, W1, k / 24.f, 600.f));
		Cable(F, Pts, 150.f, true, true, 71);
		for (int32 h = 1; h < 12; ++h)
		{
			const FVector P = Pts[h * 2];
			for (const float S : { -1.f, 1.f })
			{
				FVector Wl;
				if (!FgWall(D, F.B.T.TransformPosition(P) + FVector(0, 0, 400.f), FVector(0.f, S, 0.4f), 6000.f, Wl)) continue;
				const FVector L = F.B.Local(Wl);
				F.B.M.Tube({ P + FVector(0, 0, 100.f), FMath::Lerp(P, L, 0.5f) - FVector(0, 0, 80.f), L }, 18.f, 6, RhizoC(), 0.35f);
			}
		}
		// The anchors: root plates where the cable grows out of each wall, the Cable Shrine's brazier lashed on the west one.
		for (const FVector& E : { W0, W1 }) F.B.Ball(E, 300.f, RhizoC(), 0.4f, 0.6f);
		FarLight(F, *H, FVector(0.f, 0.f, -(Walk - H->At.Z)), AmberC(), 6000.f);
	}

	void DrownedGranary(FFg& F, AWRFungalScenery* A)
	{
		// L7 (fungal 2.3): nine round silo-towers grown from shelf fungus, standing half-sunk in the lake, rope bridges joining
		// their tops; their grain gone to black dust (the open tops show it). Glowfin shoals light the water round them.
		const FData& D = FData::Get();
		const FData::FHall* H = D.FindHall(TEXT("R1"));
		if (!H) return;
		TArray<FVector> Tops;
		for (int32 s = 0; s < 9; ++s)
		{
			const float X = (-0.75f + s * 0.19f) * H->Rx, Y = (s % 2 ? 0.3f : -0.25f) * H->Ry;
			const FVector B(X, Y, -200.f);
			const float Ht = 1400.f + 800.f * FgH(81, s), R = 360.f + 120.f * FgH(81, s + 9);
			F.B.M.Lathe(B, FVector::UpVector, Ht, { FVector(0.f, R * 1.1f, R * 1.1f), FVector(0.3f, R, R), FVector(0.8f, R * 0.96f, R * 0.96f), FVector(1.f, R * 1.02f, R * 1.02f) }, 18, Rgb(170, 140, 100) * 0.55f, 0.85f, 0.f, float(s + 3));
			for (int32 b = 0; b < 6; ++b) Shelf(F, B + FVector(0, 0, 400.f + b * (Ht - 500.f) / 6.f) + FVector(FMath::Cos(b * 1.1f), FMath::Sin(b * 1.1f), 0.f) * R, FVector(FMath::Cos(b * 1.1f), FMath::Sin(b * 1.1f), 0.f), 120.f, false, false, s * 10 + b);
			F.B.K.Disc(Grain(), B + FVector(0, 0, Ht - 30.f), FVector::UpVector, R * 1.8f, 10.f);
			F.B.Box(B + FVector(0, 0, Ht * 0.5f), FVector(R * 0.8f, R * 0.8f, Ht * 0.5f));
			Tops.Add(B + FVector(0, 0, Ht));
		}
		for (int32 s = 0; s + 1 < Tops.Num(); ++s)
		{
			// A rope bridge: two hand ropes, planks between two foot ropes, sagging.
			const FVector A0 = Tops[s], A1 = Tops[s + 1];
			const float Sag = FVector::Dist(A0, A1) * 0.05f;
			for (int32 p = 1; p < 16; ++p)
			{
				const FVector Q = WRForestLook::Sag(A0, A1, p / 16.f, Sag);
				F.B.K.Add(OldWood(0.9f), FVector(26.f, 110.f, 5.f), Q, (A1 - A0).Rotation());
			}
			const FVector Side = FVector::CrossProduct(FVector::UpVector, (A1 - A0).GetSafeNormal()) * 55.f;
			F.B.Line(A0 + Side + FVector(0, 0, 90.f), A1 + Side + FVector(0, 0, 90.f), 2.f, Sag, RopeC(), 12);
			F.B.Line(A0 - Side + FVector(0, 0, 90.f), A1 - Side + FVector(0, 0, 90.f), 2.f, Sag, RopeC(), 12);
			for (int32 k = 0; k < 8; ++k) F.B.SegBox(WRForestLook::Sag(A0, A1, k / 8.f, Sag), WRForestLook::Sag(A0, A1, (k + 1) / 8.f, Sag), 55.f, 6.f);
		}
		for (int32 g = 0; g < 6; ++g) F.B.Loop(TEXT("fungal.glowfin"), FVector((-0.6f + g * 0.24f) * H->Rx, 0.f, H->Lake - 120.f));
	}

	void FallField(FFg& F, AWRFungalScenery* A)
	{
		// L6 (fungal 2.3): the Flue's floor: everything that ever fell from the sky lies here, furred white: birds, sky beasts,
		// kites and people. A ring of fallen kite spars round the Flue Floor Shrine. The floor is the Sky lane's; things lie
		// on it at local 0.
		for (int32 b = 0; b < 14; ++b)
		{
			const float Ang = 2.f * PI * FgH(91, b), R = 1500.f + 9500.f * FMath::Sqrt(FgH(91, b + 20));
			Bones(F, FVector(FMath::Cos(Ang) * R, FMath::Sin(Ang) * R, 4.f), FgH(91, b + 40) * 360.f, 0.6f + 2.2f * FMath::Square(FgH(91, b + 60)), 91 + b);
		}
		for (int32 k = 0; k < 4; ++k)
		{
			const float Ang = 2.f * PI * (k + 0.3f) / 4.f;
			Kite(F, FVector(FMath::Cos(Ang) * 7000.f, FMath::Sin(Ang) * 7000.f, 150.f), FgH(93, k) * 360.f, 700.f + 300.f * FgH(93, k + 5), true, 93 + k);
		}
		// The spar ring round the shrine (east of the champion).
		const FData& D = FData::Get();
		const FData::FShrine* Sh = D.Shrines.FindByPredicate([](const FData::FShrine& S) { return S.Id == TEXT("flue_floor_shrine"); });
		if (Sh)
		{
			const FVector C = F.B.Local(Sh->At);
			for (int32 s = 0; s < 10; ++s)
			{
				const float Ang = 2.f * PI * s / 10.f;
				F.B.K.Rod(Bamboo(0.7f), C + FVector(FMath::Cos(Ang) * 420.f, FMath::Sin(Ang) * 420.f, 0.f), C + FVector(FMath::Cos(Ang) * 380.f, FMath::Sin(Ang) * 380.f, 260.f + 80.f * FgH(95, s)), 12.f);
			}
		}
		F.B.Lamp(FVector(0, 0, 3000.f), FLinearColor(0.87f, 0.91f, 0.94f), 20000.f, 14000.f);
	}

	void Champion(FFg& F, AWRFungalScenery* A)
	{
		// C18 the Fallen Champion (world-map SPEC): a winged skeleton 9 m tall in a great kite rig, spread-eagled on its back,
		// wrapped in mycelium cords, a Deepfarmer bronze sickle in its ribs; its wings point up the shaft. Its chest is
		// climbable (boxes on the ribs); the jade kite-token is in its fist (the cache).
		const FLinearColor C = FMath::Lerp(BoneC(), MycelC(), 0.45f);
		const float S = 100.f;
		const FVector Pelvis(-2.f * S, 0.f, 0.4f * S), Chest(1.5f * S, 0.f, 0.8f * S), Skull(4.f * S, 0.f, 0.7f * S);
		F.B.M.Tube({ Pelvis, (Pelvis + Chest) * 0.5f + FVector(0, 0, 30.f), Chest, Skull - FVector(0.6f * S, 0, 0) }, 16.f, 8, C, 0.8f);
		for (int32 r = 0; r < 8; ++r)
		{
			for (const float Sd : { -1.f, 1.f })
			{
				TArray<FVector> Rib;
				for (int32 k = 0; k <= 5; ++k) { const float T = k / 5.f; Rib.Add(Chest - FVector(r * 0.32f * S, 0.f, 0.f) + FVector(0.f, Sd * (0.3f * S + 1.2f * S * FMath::Sin(T * 1.7f)), 0.9f * S * FMath::Sin(T * PI) * (1.f - 0.3f * T))); }
				F.B.M.Tube(Rib, 7.f, 5, C, 0.8f);
			}
		}
		F.B.Ball(Skull, 0.75f * S, C, 0.8f, 0.85f);
		F.B.K.Spike(FSurf::Stone(Rgb(200, 190, 170) * 0.6f, 0.7f), Skull + FVector(0.6f * S, 0, 0), FVector(1.f, 0, 0.1f), 30.f, 1.2f * S);   // the beak
		// The wings in the kite rig: long finger bones fanned up the shaft, the rig's bamboo spars lashed along them, torn cloth.
		for (const float Sd : { -1.f, 1.f })
		{
			const FVector Sh = Chest + FVector(0, Sd * 0.6f * S, 0.4f * S);
			const FVector El = Sh + FVector(-0.4f * S, Sd * 2.4f * S, 1.2f * S), Wr = El + FVector(0.6f * S, Sd * 2.f * S, 1.6f * S);
			F.B.Limb(Sh, El, 14.f, 11.f, C);
			F.B.Limb(El, Wr, 11.f, 8.f, C);
			for (int32 f = 0; f < 4; ++f) F.B.Limb(Wr, Wr + FVector(-0.8f * S + f * 0.5f * S, Sd * (1.6f * S - f * 0.2f * S), 1.4f * S + f * 0.3f * S), 6.f, 3.f, C);
			F.B.K.Rod(Bamboo(0.6f), Sh, Wr + FVector(0, Sd * 1.4f * S, 1.6f * S), 9.f);
			F.B.K.Add(FSurf::Cloth(Rgb(150, 140, 120) * 0.45f), FVector(2.4f * S, 2.f * S, 2.f), (El + Wr) * 0.5f + FVector(-0.6f * S, Sd * 0.4f * S, 0.4f * S), FRotator(30.f, Sd * 70.f, Sd * 20.f));
		}
		for (int32 l = 0; l < 2; ++l) F.B.Limb(Pelvis + FVector(0, (l ? 1 : -1) * 0.4f * S, 0), Pelvis + FVector(-3.f * S, (l ? 1 : -1) * 1.2f * S, 0.2f * S), 12.f, 8.f, C);
		// Mycelium cords over it (the Deep took it), and the sickle in the ribs.
		for (int32 c = 0; c < 10; ++c) F.B.Line(FVector(-4.f * S + c * 0.9f * S, -3.f * S, 0.f), FVector(-4.5f * S + c * 0.9f * S, 3.f * S, 0.f), 3.f, -80.f, MycelC(), 10);
		Sickle(F, Chest - FVector(0.6f * S, 0.f, -0.5f * S), FVector(0.2f, 0.4f, 0.9f), 2.2f);
		// The climb up its chest: the ribs as steps.
		for (int32 r = 0; r < 4; ++r) F.B.Box(Chest - FVector(r * 0.6f * S, 0.f, -0.2f * S - r * 10.f), FVector(0.3f * S, 1.1f * S, 0.25f * S));
	}

	void SandFallsHall(FFg& F, const FData::FHall& H)
	{
		// L8 / C6: dunes inside the cave under the three Sieves (the falls themselves are the shafts' furniture), the caravan
		// that fell through: a Dromad skeleton, pots of sunstone, a Sunwatch gnomon; F-S4's ledge behind the middle fall.
		const FData& D = FData::Get();
		for (int32 d = 0; d < 9; ++d)
		{
			const FVector C((FgH(111, d) - 0.5f) * H.Rx * 1.2f, (FgH(111, d + 10) - 0.5f) * H.Ry * 1.2f, -30.f);
			const float R = 600.f + 900.f * FgH(111, d + 20);
			F.B.M.Lathe(C, FVector::UpVector, R * 0.28f, { FVector(0.f, R, R * 0.7f), FVector(0.6f, R * 0.6f, R * 0.4f), FVector(1.f, R * 0.08f, R * 0.06f) }, 16, Rgb(216, 192, 138) * 0.6f, 0.95f, 0.f, float(d + 1));
		}
		const FVector Cv(1800.f, 1400.f, 0.f);
		Bones(F, Cv, 30.f, 2.6f, 113);
		for (int32 p = 0; p < 5; ++p) Jar(F, Cv + FVector(-300.f + p * 90.f, 300.f + 40.f * FgH(113, p), -10.f), 50.f + 20.f * FgH(113, p + 5), 113 + p);
		F.B.K.Disc(Bronze(0.8f), Cv + FVector(400.f, -200.f, 6.f), FVector(0.1f, 0.f, 1.f), 120.f, 6.f);
		F.B.K.Add(Bronze(0.8f), FVector(4.f, 70.f, 50.f), Cv + FVector(400.f, -200.f, 34.f), FRotator(0.f, 0.f, 0.f));
		if (const FData::FSecret* S = D.Secrets.FindByPredicate([](const FData::FSecret& Q) { return Q.Id == TEXT("F-S4"); }))
		{
			// The ledge path behind the middle fall: a shelf of rock 2 m wide along the wall, from the floor up to a niche.
			const FVector L = F.B.Local(S->At);
			for (int32 k = 0; k < 10; ++k) F.B.SegBox(L + FVector(-900.f + k * 90.f, 0.f, k * 60.f - 600.f), L + FVector(-810.f + k * 90.f, 0.f, k * 60.f - 540.f), 110.f, 25.f);
			for (int32 k = 0; k < 10; ++k) F.B.K.Add(Limestone(0.85f), FVector(100.f, 220.f, 50.f), L + FVector(-855.f + k * 90.f, 0.f, k * 60.f - 595.f));
			// The trading post: Sunwatch sunstone and Deepfarmer deepcap jars side by side, the two peoples met here.
			for (int32 j = 0; j < 4; ++j) Jar(F, L + FVector(-100.f + j * 60.f, 60.f, 0.f), 40.f, 120 + j);
			F.B.K.Add(FSurf::Lit(Rgb(232, 180, 80) * 0.6f, 0.6f), FVector(30.f, 30.f, 30.f), L + FVector(150.f, 60.f, 15.f), FRotator(0.f, 45.f, 45.f));
		}
	}

	// ---------------------------------------------------------------- the cavern hand sites

	void OfferingNiches(FFg& F, AWRFungalScenery* A)
	{
		// C1: a wall of 200 niches under the Great Shrine, each holding the folded bones of one person, furred with mycelium
		// and crowned with a glowing deepcap: the first deepcap the player sees grows on the dead.
		const FData& D = FData::Get();
		const FData::FHall* H = D.FindHall(TEXT("G1"));
		if (!H) return;
		const FVector Dir = (A->GetActorLocation() - H->At).GetSafeNormal2D();
		FVector W, Nr;
		if (!FgWall(D, A->GetActorLocation() + FVector(0, 0, 400.f), Dir, 5000.f, W, &Nr)) return;
		const FVector Face = F.B.Local(W);
		const FVector N = F.B.T.InverseTransformVectorNoScale(Nr).GetSafeNormal2D();
		const FVector Side = FVector::CrossProduct(FVector::UpVector, N);
		const FVector Base = Face + N * 60.f - FVector(0, 0, 400.f);
		F.B.K.Add(Carved(0.9f), FVector(80.f, 2200.f, 850.f), Base + FVector(0, 0, 425.f) - N * 40.f, N.Rotation());
		for (int32 r = 0; r < 10; ++r) for (int32 c = 0; c < 20; ++c)
		{
			const FVector P = Base + Side * ((c - 9.5f) * 105.f) + FVector(0, 0, 40.f + r * 80.f);
			F.B.K.Add(FSurf::Stone(Rgb(30, 26, 22) * 0.6f, 0.95f), FVector(10.f, 80.f, 60.f), P + N * 2.f, N.Rotation());
			F.B.Ball(P + N * 18.f, 18.f, FMath::Lerp(BoneC(), MycelC(), 0.6f), 0.9f, 0.6f);
			if (FgH(201, r * 20 + c) < 0.7f) Mushroom(F, P + N * 22.f + FVector(0, 0, 10.f), FVector::UpVector, 16.f, 9.f, ECap::Deep, 0.9f, r * 20 + c, false);
		}
		F.B.Lamp(Base + N * 300.f + FVector(0, 0, 400.f), CyanC(), 2400.f, 1800.f);
	}

	void GardenFarm(FFg& F, AWRFungalScenery* A)
	{
		// C2 (survival-crafting KEPT set-piece): log beds in rows, a Capling pen with its gate open, a spore mask on a hook.
		for (int32 r = 0; r < 4; ++r) for (int32 c = 0; c < 2; ++c) LogBed(F, FVector(-700.f + r * 330.f, -600.f + c * 380.f, -6.f), 0.f, 211 + r * 2 + c);
		// The pen: a ring of posts with rails, the gate standing open on its hinge.
		const FVector Pc(700.f, 400.f, 0.f);
		for (int32 p = 0; p < 14; ++p)
		{
			const float Ang = 2.f * PI * p / 14.f;
			if (p == 0) continue;
			const FVector Q = Pc + FVector(FMath::Cos(Ang) * 500.f, FMath::Sin(Ang) * 500.f, 0.f);
			F.B.K.Rod(OldWood(), Q, Q + FVector(0, 0, 110.f), 10.f);
			const FVector Q2 = Pc + FVector(FMath::Cos(Ang + 2.f * PI / 14.f) * 500.f, FMath::Sin(Ang + 2.f * PI / 14.f) * 500.f, 0.f);
			if (p < 13) { F.B.K.Rod(OldWood(0.9f), Q + FVector(0, 0, 90.f), Q2 + FVector(0, 0, 90.f), 6.f); F.B.K.Rod(OldWood(0.9f), Q + FVector(0, 0, 45.f), Q2 + FVector(0, 0, 45.f), 6.f); F.B.SegBox(Q + FVector(0, 0, 55.f), Q2 + FVector(0, 0, 55.f), 6.f, 55.f); }
		}
		const FVector Gh = Pc + FVector(500.f, 0.f, 0.f);
		F.B.K.Add(OldWood(0.85f), FVector(220.f, 6.f, 90.f), Gh + FVector(30.f, 100.f, 55.f), FRotator(0.f, 80.f, 0.f));
		// The mask on its hook: a leather half-mask with two round filters (a newcomer's design copied from the jars).
		const FVector Hk(-900.f, 700.f, 0.f);
		F.B.K.Rod(OldWood(), Hk, Hk + FVector(0, 0, 170.f), 10.f);
		F.B.K.Rod(Bronze(), Hk + FVector(0, 0, 160.f), Hk + FVector(20.f, 0.f, 160.f), 3.f);
		F.B.Ball(Hk + FVector(24.f, 0.f, 140.f), 12.f, Rgb(110, 80, 56) * 0.5f, 0.8f, 0.7f);
		for (const float S : { -1.f, 1.f }) F.B.K.Disc(Bronze(0.8f), Hk + FVector(32.f, S * 8.f, 134.f), FVector(1.f, 0.f, 0.f), 9.f, 5.f);
		for (int32 r = 0; r < 2; ++r) DryingRack(F, FVector(-200.f + r * 320.f, 900.f, -4.f), 90.f, 320.f, 215 + r);
	}

	void GroveLedge(FFg& F, AWRFungalScenery* A)
	{
		// C3: the Springcap Grove's ledge 40 m up the doline wall (five bounces up), a Deepfarmer cache on it (the cache).
		const FData& D = FData::Get();
		const FData::FHall* H = D.FindHall(TEXT("G4"));
		if (!H) return;
		const float Ang = FMath::DegreesToRadians(200.f + 4.f * 26.f);
		FVector W, Nr;
		if (!FgWall(D, H->At + FVector(0, 0, 4000.f), FVector(FMath::Cos(Ang), FMath::Sin(Ang), 0.f), H->Rx * 1.4f, W, &Nr)) return;
		const FVector L = F.B.Local(W);
		const FVector N = F.B.T.InverseTransformVectorNoScale(Nr).GetSafeNormal2D();
		F.B.K.Add(Limestone(0.9f), FVector(320.f, 520.f, 60.f), L + N * 140.f - FVector(0, 0, 30.f), N.Rotation());
		F.B.Box(L + N * 140.f - FVector(0, 0, 30.f), FVector(160.f, 260.f, 30.f), N.Rotation());
		F.B.K.Add(Timber(0.9f), FVector(80.f, 50.f, 46.f), L + N * 120.f + FVector(0, 0, 23.f), N.Rotation());
		Knocker(F, L + N * 200.f + FVector(0, 120.f, 0.f), N, 0.8f, 221);
	}

	void CloudRoom(FFg& F, AWRFungalScenery* A)
	{
		// C4 (fungal 2.4, Son Doong): warm river air meets the doline's cool air: a real cloud hangs under the ceiling and rains
		// on a cap jungle; rainbows in the daylight shaft (the fx rows). Lion's mane on fallen logs (the nodes).
		const FData::FHall* H = FData::Get().FindHall(TEXT("G5"));
		if (!H) return;
		for (int32 c = 0; c < 5; ++c) F.B.Loop(TEXT("fungal.cloudRoom"), FVector((c - 2) * 1800.f, (c % 2 ? 600.f : -600.f), H->H * 0.72f), FRotator::ZeroRotator, 2.f);
		for (int32 r = 0; r < 4; ++r) F.B.Loop(TEXT("fungal.caveRain"), FVector((r - 1.5f) * 2200.f, 0.f, H->H * 0.6f), FRotator::ZeroRotator, 1.5f);
		F.B.Loop(TEXT("fungal.rainbow"), F.B.Local(FVector(FWRFungalData::Get().CloudMouth.X, FWRFungalData::Get().CloudMouth.Y, H->At.Z + 1500.f)));
		for (int32 l = 0; l < 3; ++l) F.B.Limb(FVector(-1000.f + l * 1200.f, 900.f, 30.f), FVector(-200.f + l * 1200.f, 1100.f, 40.f), 40.f, 34.f, TimberC());
	}

	void LureGallery(FFg& F, AWRFungalScenery* A)
	{
		// C5: hung in silk at different heights: Glowgrubs, a Capling, a Desert jackal, a newcomer's satchel. The lines that
		// grab are the hazard's; these are the larder's catches, wrapped and hanging.
		const FData& D = FData::Get();
		const FData::FHall* H = D.FindHall(TEXT("H2"));
		if (!H) return;
		for (int32 c = 0; c < 9; ++c)
		{
			const FVector P = H->At + FRotator(0.f, H->Yaw, 0.f).RotateVector(FVector((c - 4) * H->Rx * 0.2f, (c % 2 ? 300.f : -300.f), 200.f));
			FVector Top;
			if (!FgWall(D, P, FVector::UpVector, H->H * 2.f, Top)) continue;
			const FVector T = F.B.Local(Top);
			const FVector B = T - FVector(0, 0, 300.f + 250.f * FgH(231, c));
			F.Glow.Tri(T + FVector(1.f, 0, 0), T - FVector(1.f, 0, 0), B, FWRToolMesh::Pack(StarC(), 0.5f), FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector);
			const float L = c < 4 ? 70.f : c == 4 ? 60.f : c == 5 ? 110.f : 50.f;
			F.Veil.Lathe(B - FVector(0, 0, L), FVector::UpVector, L, { FVector(0.f, 4.f, 4.f), FVector(0.4f, L * 0.32f, L * 0.3f), FVector(1.f, 6.f, 6.f) }, 10, Rgb(220, 220, 212), 0.2f);
			if (c < 4) F.Glow.Lathe(B - FVector(0, 0, L * 0.7f), FVector::UpVector, L * 0.4f, { FVector(0.f, 8.f, 8.f), FVector(1.f, 5.f, 5.f) }, 6, GrubC(), 0.9f);
			if (c == 8) F.B.K.Add(FSurf::Cloth(Rgb(120, 90, 60) * 0.5f), FVector(36.f, 20.f, 30.f), B - FVector(0, 0, 30.f));
		}
	}

	void KnockersJunction(FFg& F, AWRFungalScenery* A)
	{
		// C7: a four-way junction with a clay knocker idol facing down each tunnel; a Truffle mole nest in the wall (the fifth
		// tunnel's blank wall is the mole's to find: F-S2).
		const FData& D = FData::Get();
		const FData::FHall* H = D.FindHall(TEXT("H3"));
		if (!H) return;
		int32 k = 0;
		for (const FData::FTube& T : D.Tubes)
		{
			if (T.Pts.Num() < 2 || T.bSealed) continue;
			const FVector E0 = T.Pts[0], E1 = T.Pts.Last();
			const bool b0 = H->Contains(E0, 300.f), b1 = H->Contains(E1, 300.f);
			if (!b0 && !b1) continue;
			const FVector Mouth = b0 ? E0 : E1, Away = b0 ? T.Pts[FMath::Min(6, T.Pts.Num() - 1)] : T.Pts[FMath::Max(0, T.Pts.Num() - 7)];
			const FVector Dir = (Away - Mouth).GetSafeNormal2D();
			const FVector L = F.B.Local(Mouth - Dir * 200.f) + FVector::CrossProduct(FVector::UpVector, F.B.T.InverseTransformVectorNoScale(Dir)) * 250.f;
			Knocker(F, FVector(L.X, L.Y, -4.f), F.B.T.InverseTransformVectorNoScale(Dir), 1.2f, 241 + k++);
		}
		F.B.Ball(FVector(-700.f, 900.f, 0.f), 150.f, Rgb(70, 56, 40) * 0.5f, 0.95f, 0.5f);
	}

	void SporeArchive(FFg& F, AWRFungalScenery* A)
	{
		// C8: a room of shelves holding thousands of sealed jars, each a spore print pressed on its lid (the farmers' library).
		for (int32 u = 0; u < 6; ++u)
		{
			const float X = -1500.f + u * 600.f;
			for (const float Y : { -800.f, 800.f })
			{
				F.B.K.Add(OldWood(0.8f), FVector(500.f, 50.f, 8.f), FVector(X, Y, 2.f));
				for (int32 s = 0; s < 5; ++s)
				{
					F.B.K.Add(OldWood(0.85f), FVector(500.f, 50.f, 6.f), FVector(X, Y, 50.f + s * 55.f));
					for (int32 j = 0; j < 12; ++j) Jar(F, FVector(X - 225.f + j * 41.f, Y, 53.f + s * 55.f), 30.f + 8.f * FgH(251, u * 100 + s * 12 + j), 251 + u * 100 + s * 12 + j);
				}
				for (const float Px : { -245.f, 245.f }) F.B.K.Rod(OldWood(0.9f), FVector(X + Px, Y, 0.f), FVector(X + Px, Y, 330.f), 8.f, true);
				F.B.Box(FVector(X, Y, 165.f), FVector(250.f, 25.f, 165.f));
			}
		}
		F.B.Lamp(FVector(0.f, 0.f, 500.f), FireLight() * 0.6f, 900.f, 2200.f);
	}

	void SaltpetreWalls(FFg& F, AWRFungalScenery* A)
	{
		// C9: white nitre crust on the walls, Deepfarmer scraping tools and baskets left mid-work.
		const FData& D = FData::Get();
		const FData::FHall* H = D.FindHall(TEXT("H5"));
		if (!H) return;
		for (int32 c = 0; c < 26; ++c)
		{
			const float Ang = 2.f * PI * FgH(261, c);
			FVector W, Nr;
			if (!FgWall(D, H->At + FVector(0, 0, 100.f + 600.f * FgH(261, c + 30)), FVector(FMath::Cos(Ang), FMath::Sin(Ang), 0.f), H->Rx * 1.3f, W, &Nr)) continue;
			F.B.K.Add(FSurf::Stone(Rgb(240, 238, 230) * 0.66f, 0.85f), FVector(30.f, 200.f + 300.f * FgH(261, c + 60), 120.f + 200.f * FgH(261, c + 90)), F.B.Local(W) + F.B.T.InverseTransformVectorNoScale(Nr) * 6.f, F.B.T.InverseTransformVectorNoScale(Nr).Rotation());
		}
		for (int32 b = 0; b < 4; ++b)
		{
			const FVector P(-600.f + b * 400.f, 500.f * (b % 2 ? 1.f : -1.f), 0.f);
			F.B.M.Lathe(P, FVector::UpVector, 50.f, { FVector(0.f, 22.f, 22.f), FVector(1.f, 32.f, 32.f) }, 12, Rgb(176, 146, 80) * 0.45f, 0.95f);
			F.B.K.Disc(FSurf::Stone(Rgb(240, 238, 230) * 0.6f, 0.9f), P + FVector(0, 0, 44.f), FVector::UpVector, 56.f, 6.f);
			F.B.K.Rod(Bronze(0.8f), P + FVector(60.f, 0.f, 0.f), P + FVector(120.f, 30.f, 6.f), 5.f);
		}
	}

	void SilkLarders(FFg& F, AWRFungalScenery* A)
	{
		// C10: a Webspinner larder: cocoons hung from cables, and you can see what's in them: a Rotmaw, a Puffball, a sky
		// Mistlark, a Kitewright glider with its pilot.
		const FData& D = FData::Get();
		const FData::FHall* H = D.FindHall(TEXT("W2"));
		if (!H) return;
		for (int32 c = 0; c < 8; ++c)
		{
			const FVector P = H->At + FVector((c % 4 - 1.5f) * 900.f, (c / 4 - 0.5f) * 900.f, 600.f);
			FVector Top;
			if (!FgWall(D, P, FVector::UpVector, H->H * 2.f, Top)) continue;
			const FVector T = F.B.Local(Top);
			const float L = 120.f + 60.f * (c % 3);
			const FVector B = T - FVector(0, 0, 500.f + 300.f * FgH(271, c));
			F.B.M.Tube({ T, B }, 5.f, 5, Rgb(184, 184, 176) * 0.6f, 0.9f);
			F.Veil.Lathe(B - FVector(0, 0, L * 2.f), FVector::UpVector, L * 2.f, { FVector(0.f, 10.f, 10.f), FVector(0.3f, L * 0.55f, L * 0.5f), FVector(0.7f, L * 0.6f, L * 0.55f), FVector(1.f, 14.f, 14.f) }, 12, Rgb(226, 226, 218), 0.18f);
			// What's inside (dark shapes through the silk): a mound of caps, a ball, a bird, a glider's frame and its pilot.
			const FVector In = B - FVector(0, 0, L);
			if (c == 0) F.B.Ball(In, L * 0.45f, UmberC(), 0.9f, 0.7f);
			else if (c == 1) F.B.Ball(In, L * 0.4f, Rgb(237, 230, 214) * 0.5f, 0.8f);
			else if (c == 2) { F.B.Ball(In, L * 0.25f, Rgb(140, 150, 160) * 0.5f, 0.8f, 1.4f); F.B.K.Spike(FSurf::Stone(Rgb(60, 60, 60) * 0.5f, 0.6f), In + FVector(L * 0.25f, 0, 0), FVector(1, 0, 0), 8.f, 20.f); }
			else if (c == 3) { F.B.K.Rod(Bamboo(0.7f), In - FVector(0, L * 0.5f, 0), In + FVector(0, L * 0.5f, 0), 5.f); F.B.Ball(In - FVector(0, 0, 20.f), 15.f, SkinC(), 0.9f); }
		}
	}

	void RootCeiling(FFg& F, AWRFungalScenery* A)
	{
		// C11: a chasm whose ceiling is a forest of hanging black rhizomorphs; a Deepfarmer climbed one to its very tip and
		// clamped there, summited, over the drop. The walkway across is a rhizomorph that grew from wall to wall.
		const FData& D = FData::Get();
		const FData::FHall* H = D.FindHall(TEXT("W3"));
		if (!H) return;
		const FVector Wn = F.B.Local(FVector(H->At.X, H->At.Y - H->Rx * 0.96f, H->Walk - 100.f)), Ws = F.B.Local(FVector(H->At.X, H->At.Y + H->Rx * 0.96f, H->Walk - 100.f));
		TArray<FVector> Pts;
		for (int32 k = 0; k <= 16; ++k) Pts.Add(WRForestLook::Sag(Wn, Ws, k / 16.f, 300.f));
		Cable(F, Pts, 110.f, true, false, 281);
		for (int32 h = 0; h < 40; ++h)
		{
			const FVector P = H->At + FVector((FgH(281, h) - 0.5f) * H->Ry * 1.5f, (FgH(281, h + 50) - 0.5f) * H->Rx * 1.5f, H->Walk - H->At.Z + 600.f + H->At.Z - H->At.Z);
			FVector Top;
			if (!FgWall(D, FVector(P.X, P.Y, H->Walk + 400.f), FVector::UpVector, H->H, Top)) continue;
			const FVector T = F.B.Local(Top);
			const float Len = 600.f + 3800.f * FgH(281, h + 100);
			const FVector Tip = T - FVector(0, 0, Len);
			F.B.M.Tube({ T + FVector(0, 0, 40.f), T - FVector(0, 0, Len * 0.4f) + FVector(40.f * FgH(281, h), 0, 0), T - FVector(0, 0, Len * 0.75f), Tip }, 14.f + 20.f * FgH(281, h + 150), 6, RhizoC(), 0.35f);
			if (h == 7) { const FData::FHall* HC = D.FindHall(TEXT("HC")); Body(F, Tip + FVector(0, 0, 40.f), F.B.T.InverseTransformVectorNoScale(((HC ? HC->At : Top) - Top).GetSafeNormal2D()), EPose::CableTip, 281); }
		}
	}

	void TitanNursery(FFg& F, AWRFungalScenery* A)
	{
		// C12: wooden scaffolds round three Deepfarmer titans half-grown inside clay moulds, each mould built round a stone
		// bier: they grew their guardians from their honoured dead. One mould is broken open from the inside, and empty.
		for (int32 m = 0; m < 3; ++m)
		{
			const FVector C(-1200.f + m * 1200.f, 0.f, 0.f);
			F.B.K.Add(Carved(0.9f), FVector(500.f, 200.f, 80.f), C + FVector(0, 0, 40.f));
			const bool bBroken = m == 2;
			const float H = 1600.f, R = 420.f;
			if (!bBroken) F.B.M.Lathe(C + FVector(0, 0, 80.f), FVector::UpVector, H, { FVector(0.f, R, R * 0.6f), FVector(0.4f, R * 1.1f, R * 0.7f), FVector(0.8f, R * 0.7f, R * 0.5f), FVector(1.f, R * 0.3f, R * 0.3f) }, 16, ClayC(), 0.9f);
			else
			{
				// The broken one: shards of the mould flung outward from the inside, the bier bare.
				for (int32 s = 0; s < 12; ++s)
				{
					const float Ang = 2.f * PI * s / 12.f;
					F.B.K.Add(Clay(0.9f), FVector(260.f, 30.f, 400.f + 300.f * FgH(291, s)), C + FVector(FMath::Cos(Ang) * (R + 300.f + 400.f * FgH(291, s + 12)), FMath::Sin(Ang) * (R + 300.f), 120.f), FRotator(60.f + 30.f * FgH(291, s), FMath::RadiansToDegrees(Ang), 0.f));
				}
			}
			for (const float X : { -R - 60.f, R + 60.f }) for (const float Y : { -R * 0.7f - 60.f, R * 0.7f + 60.f })
			{
				F.B.K.Rod(Timber(0.85f), C + FVector(X, Y, 0.f), C + FVector(X, Y, H + 200.f), 14.f, true);
				for (float Z = 300.f; Z < H; Z += 400.f) F.B.K.Rod(Timber(0.8f), C + FVector(X, Y, Z), C + FVector(-X, Y, Z), 10.f, true);
			}
			if (!bBroken) F.B.Box(C + FVector(0, 0, 80.f + H * 0.5f), FVector(R * 0.9f, R * 0.6f, H * 0.5f));
		}
	}

	void SealedGateFrame(FFg& F, AWRFungalScenery* A)
	{
		// C13: the round stone door into the Heart Grotto, carved with spiralling mycelium; its web sheet is the hazard's
		// (it burns or tears).
		const FData& D = FData::Get();
		const FData::FTube* T = D.FindTube(TEXT("W5-HG1"));
		if (!T || T->Pts.Num() < 2) return;
		const FVector M = F.B.Local(T->Pts[0]);
		const FVector Dir = F.B.T.InverseTransformVectorNoScale((T->Pts[FMath::Min(4, T->Pts.Num() - 1)] - T->Pts[0]).GetSafeNormal2D());
		const FRotator R = Dir.Rotation();
		for (int32 s = 0; s < 16; ++s)
		{
			const float A0 = 2.f * PI * s / 16.f;
			F.B.K.Add(Carved(0.85f + 0.1f * (s % 2)), FVector(80.f, 120.f, 160.f), M + FVector(0, 0, 600.f) + R.RotateVector(FVector(0.f, FMath::Cos(A0) * 650.f, FMath::Sin(A0) * 650.f)), R + FRotator(0.f, 0.f, FMath::RadiansToDegrees(A0)));
		}
	}

	void GrottoWeir(FFg& F, AWRFungalScenery* A)
	{
		// C14 (survival-crafting KEPT set-piece): a fish weir of woven wattle fences across the water, posts and a trap basket.
		for (int32 p = 0; p < 12; ++p)
		{
			const FVector P(-1200.f + p * 220.f, 200.f * FMath::Sin(p * 0.7f), -40.f);
			F.B.K.Rod(OldWood(0.8f), P, P + FVector(0, 0, 420.f), 12.f);
			if (p > 0) F.B.K.Add(FSurf::Wood(Rgb(120, 98, 70) * 0.45f), FVector(220.f, 8.f, 200.f), P + FVector(-110.f, -100.f * FMath::Sin(p * 0.7f) + 100.f * FMath::Sin((p - 1) * 0.7f), 160.f), FRotator(0.f, -10.f, 0.f));
		}
		F.B.M.Lathe(FVector(600.f, 500.f, 200.f), FVector(1.f, 0.f, 0.f), 160.f, { FVector(0.f, 50.f, 50.f), FVector(0.7f, 40.f, 40.f), FVector(1.f, 8.f, 8.f) }, 10, Rgb(150, 120, 80) * 0.45f, 0.95f);
	}

	void AirBell(FFg& F, AWRFungalScenery* A)
	{
		// C15 (Movile): a dome of trapped air over a sump; a Deepfarmer bronze diving bell rests on the ledge, a cache and the
		// diver's bones inside. Foul air here: the hazard.
		const FVector B(200.f, 0.f, 0.f);
		F.B.M.Lathe(B, FVector::UpVector, 240.f, { FVector(0.f, 130.f, 130.f), FVector(0.6f, 115.f, 115.f), FVector(0.9f, 70.f, 70.f), FVector(1.f, 14.f, 14.f) }, 18, Rgb(84, 140, 122) * 0.5f, 0.6f);
		for (int32 p = 0; p < 3; ++p) F.B.K.Disc(FSurf::Glass(Rgb(160, 200, 190) * 0.5f, 0.f), B + FVector(FMath::Cos(p * 2.1f) * 110.f, FMath::Sin(p * 2.1f) * 110.f, 150.f), FVector(FMath::Cos(p * 2.1f), FMath::Sin(p * 2.1f), 0.f), 26.f, 4.f);
		F.B.K.Rod(Bronze(), B + FVector(0, 0, 240.f), B + FVector(0, 0, 300.f), 12.f);
		Bones(F, B + FVector(-20.f, 0.f, 4.f), 0.f, 0.45f, 301);
		F.B.Box(B + FVector(0, 0, 120.f), FVector(110.f, 110.f, 120.f));
	}

	void VatRoom(FFg& F, AWRFungalScenery* A)
	{
		// C16 (survival-crafting KEPT set-piece): a cracked mycelium vat 4 m across, its pipes still pulsing with cyan; the
		// way on to the Steeping Vats.
		const FVector V(0.f, 0.f, 0.f);
		F.B.M.Lathe(V, FVector::UpVector, 260.f, { FVector(0.f, 180.f, 180.f), FVector(0.5f, 210.f, 210.f), FVector(1.f, 200.f, 200.f) }, 20, ClayC(), 0.9f);
		F.Glow.Lathe(V + FVector(0, 0, 230.f), FVector::UpVector, 4.f, { FVector(0.f, 190.f, 190.f), FVector(1.f, 186.f, 186.f) }, 20, CyanC(), 0.6f);
		F.B.K.Add(FSurf::Stone(Rgb(20, 18, 16) * 0.6f, 0.9f), FVector(14.f, 30.f, 220.f), V + FVector(205.f, 0.f, 120.f), FRotator(0.f, 0.f, 8.f));
		for (int32 p = 0; p < 3; ++p)
		{
			const float Ang = 2.f * PI * p / 3.f;
			TArray<FVector> Pipe = { V + FVector(FMath::Cos(Ang) * 200.f, FMath::Sin(Ang) * 200.f, 180.f), V + FVector(FMath::Cos(Ang) * 500.f, FMath::Sin(Ang) * 500.f, 260.f), V + FVector(FMath::Cos(Ang) * 900.f, FMath::Sin(Ang) * 900.f, 120.f) };
			F.B.M.Tube(Pipe, 22.f, 8, ClayC(), 0.9f);
			F.Glow.Tube(Pipe, 6.f, 4, CyanC(), 0.8f);
		}
		F.B.Box(V + FVector(0, 0, 130.f), FVector(200.f, 200.f, 130.f));
	}

	void ToadLeaps(FFg& F, AWRFungalScenery* A)
	{
		// C17: a chain of five chasms 10-12 m wide over the river, crossable only by a Tunnel toad's leap or by springcaps on
		// the far walls: six rock pillars at the walkway's level (-10 m) with the gaps between; a cache on the last.
		const FData::FHall* H = FWRFungalData::Get().FindHall(TEXT("R6"));
		if (!H) return;
		const float Walk = H->Walk - H->At.Z;
		float X = -H->Rx * 0.92f;
		for (int32 p = 0; p < 6; ++p)
		{
			const float W = 700.f + 300.f * FgH(311, p);
			const FVector C(X + W * 0.5f, 0.f, Walk);
			F.B.M.Lathe(C - FVector(0, 0, Walk + 300.f), FVector::UpVector, Walk + 300.f, { FVector(0.f, W * 0.7f, 900.f), FVector(0.7f, W * 0.55f, 820.f), FVector(1.f, W * 0.5f, 800.f) }, 12, Rgb(200, 196, 180) * 0.6f, 0.6f, 0.f, float(p + 1));
			F.B.Box(C - FVector(0, 0, Walk * 0.5f + 150.f), FVector(W * 0.45f, 750.f, Walk * 0.5f + 150.f));
			X += W + 1000.f + 200.f * FgH(311, p + 10);
			if (X > H->Rx * 0.95f) break;
		}
	}

	// ---------------------------------------------------------------- the Heart Grotto (fungal 2.5)

	FVector HeartDir(const FBuild& B, const FVector& LocalAt)
	{
		const FData::FHall* HC = FData::Get().FindHall(TEXT("HC"));
		if (!HC) return FVector(1, 0, 0);
		return (B.Local(HC->At) - LocalAt).GetSafeNormal2D();
	}

	void HeartRoom(FFg& F, AWRFungalScenery* A)
	{
		const FData& D = FData::Get();
		const FData::FHall* H = D.FindHall(A->SiteId);
		if (!H) return;
		const FName Id = H->Id;
		const int32 Sd = int32(GetTypeHash(Id) % 997u);
		const FVector Tw = HeartDir(F.B, FVector::ZeroVector);
		// The city's light: amber point lights at the room's corners (lit like a town at dusk, fungal 2.5).
		F.B.Lamp(FVector(0.f, 0.f, FMath::Min(H->H * 0.6f, 900.f)), AmberC(), 3500.f, FMath::Max(H->Rx, H->Ry) * 1.6f);
		if (Id == TEXT("HG1"))
		{
			// The Lantern Street: cap-houses on both sides, a summited Deepfarmer on every roof facing the Heart's dome.
			for (int32 h = 0; h < 8; ++h)
			{
				const float X = -H->Rx * 0.8f + h * H->Rx * 0.23f, Y = (h % 2 ? 1.f : -1.f) * H->Ry * 0.55f;
				const FVector Roof = CapHouse(F, FVector(X, Y, -6.f), h % 2 ? -90.f : 90.f, 220.f + 60.f * FgH(Sd, h), 380.f + 120.f * FgH(Sd, h + 8), true, Sd + h);
				Body(F, Roof + FVector(0, 0, -10.f), HeartDir(F.B, Roof), EPose::Roof, Sd + h);
				if (h % 2 == 0) Body(F, Roof + FVector(140.f, 60.f, -60.f), HeartDir(F.B, Roof), EPose::Roof, Sd + h + 50);
			}
		}
		else if (Id == TEXT("HG2"))
		{
			// The Market of Hands: stone stalls, on each a Deepfarmer's tools laid out neatly with a deepcap growing from the handle.
			for (int32 s = 0; s < 8; ++s)
			{
				const float Ang = 2.f * PI * s / 8.f;
				const FVector P(FMath::Cos(Ang) * H->Rx * 0.6f, FMath::Sin(Ang) * H->Ry * 0.6f, 0.f);
				F.B.K.Add(Carved(0.9f), FVector(200.f, 80.f, 90.f), P + FVector(0, 0, 45.f), FRotator(0.f, FMath::RadiansToDegrees(Ang) + 90.f, 0.f));
				F.B.Box(P + FVector(0, 0, 45.f), FVector(100.f, 40.f, 45.f), FRotator(0.f, FMath::RadiansToDegrees(Ang) + 90.f, 0.f));
				for (int32 t = 0; t < 3; ++t)
				{
					const FVector Q = P + FRotator(0.f, FMath::RadiansToDegrees(Ang) + 90.f, 0.f).RotateVector(FVector(-60.f + t * 60.f, 0.f, 92.f));
					Sickle(F, Q, FVector(0.f, 0.f, 0.1f) + FRotator(0.f, FMath::RadiansToDegrees(Ang), 0.f).Vector(), 0.7f);
					Mushroom(F, Q + FVector(0, 0, 4.f), FVector::UpVector, 14.f, 8.f, ECap::Deep, 0.9f, Sd + s * 3 + t, false);
				}
			}
		}
		else if (Id == TEXT("HG3"))
		{
			// The Feeding Hall: graves mounded in rows in the mycelium floor, each sprouting caps (the oldest only caps now).
			for (int32 r = 0; r < 3; ++r) for (int32 g = 0; g < 9; ++g)
			{
				const FVector P(-H->Rx * 0.8f + g * H->Rx * 0.2f, (r - 1) * 380.f, 0.f);
				F.B.M.Lathe(P - FVector(0, 0, 10.f), FVector::UpVector, 50.f, { FVector(0.f, 110.f, 60.f), FVector(1.f, 20.f, 10.f) }, 12, MycelC() * 0.9f, 0.95f);
				for (int32 c = 0; c < 3; ++c) Mushroom(F, P + FVector(-40.f + c * 40.f, 10.f * c, 30.f), FVector::UpVector, 20.f + 15.f * c, 12.f + 6.f * c, g < 3 ? ECap::Honey : ECap::Glowcap, 0.5f, Sd + r * 30 + g * 3 + c, false);
				if (g >= 3) Body(F, P + FVector(0, 0, 20.f), FVector(1, 0, 0), EPose::Lying, Sd + r * 9 + g);
			}
		}
		else if (Id == TEXT("HG4"))
		{
			// The Elders' Tower: a shelf-fungus tower 40 m tall round a stipe column; a stair of shelves spirals up it; at the
			// top seven elders summited in a ring round a bronze bowl, facing the Heart.
			const float TH = FMath::Min(4000.f, H->H - 600.f);
			F.B.M.Lathe(FVector(0, 0, -20.f), FVector::UpVector, TH + 20.f, { FVector(0.f, 420.f, 420.f), FVector(0.5f, 320.f, 320.f), FVector(1.f, 380.f, 380.f) }, 20, StipeC() * 0.9f, 0.85f);
			F.B.SegBox(FVector::ZeroVector, FVector(0, 0, TH), 300.f, 300.f);
			float Ang = 0.f;
			for (float Z = 110.f; Z < TH; Z += 110.f, Ang += 0.42f)
			{
				const FVector Dir(FMath::Cos(Ang), FMath::Sin(Ang), 0.f);
				Shelf(F, Dir * 330.f + FVector(0, 0, Z), Dir, 160.f, true, false, Sd + int32(Z));
			}
			const FVector Top(0, 0, TH + 6.f);
			F.B.K.Disc(Mycel(0.9f), Top, FVector::UpVector, 1000.f, 12.f);
			F.B.Box(Top - FVector(0, 0, 6.f), FVector(480.f, 480.f, 8.f));
			F.B.M.Lathe(Top, FVector::UpVector, 50.f, { FVector(0.f, 30.f, 30.f), FVector(1.f, 70.f, 70.f) }, 16, Rgb(140, 106, 58) * 0.6f, 0.45f);
			for (int32 e = 0; e < 7; ++e)
			{
				const float Ae = 2.f * PI * e / 7.f;
				const FVector P = Top + FVector(FMath::Cos(Ae) * 300.f, FMath::Sin(Ae) * 300.f, 60.f);
				Body(F, P, (FVector::ZeroVector - P).GetSafeNormal2D() * 0.5f + Tw, EPose::Elder, Sd + e);
			}
		}
		else if (Id == TEXT("HG8"))
		{
			// The Last House: the only house barred from inside (planks across its door; its back wall fallen in so you can see
			// in). In it, one who didn't climb: lying down, mouth bound with cord, wrists bound by their own hand. No stalk.
			CapHouse(F, FVector::ZeroVector, FMath::RadiansToDegrees(FMath::Atan2(Tw.Y, Tw.X)), 300.f, 420.f, false, Sd);
			for (int32 p = 0; p < 4; ++p) F.B.K.Add(OldWood(0.85f), FVector(10.f, 130.f, 14.f), Tw * 300.f + FVector(0, 0, 40.f + p * 45.f), Tw.Rotation() + FRotator(0.f, 0.f, p % 2 ? 12.f : -10.f));
			Body(F, FVector(-60.f, 0.f, 0.f), -Tw, EPose::Bound, Sd);
		}
		else if (Id == TEXT("HG5"))
		{
			// The Mite Warren: collapsed houses full of Mycomite nests (egg balls clustered in the rubble).
			for (int32 h = 0; h < 4; ++h)
			{
				const FVector P((h - 1.5f) * H->Rx * 0.4f, (h % 2 ? 1.f : -1.f) * H->Ry * 0.4f, 0.f);
				F.B.K.Add(Clay(0.8f), FVector(400.f, 60.f, 260.f), P + FVector(0, 0, 80.f), FRotator(28.f, h * 50.f, 0.f));
				for (int32 e = 0; e < 8; ++e) F.B.Ball(P + FVector(FMath::Cos(float(e)) * 120.f, FMath::Sin(float(e)) * 120.f, 20.f + 20.f * (e % 3)), 16.f, Rgb(220, 214, 196) * 0.6f, 0.6f);
			}
		}
		else if (Id == TEXT("HG6"))
		{
			// The Koi Pool: the lake (the grotto's surface) and a stone fishing ledge out over it.
			GrottoHall(F, A, *H);
			F.B.K.Add(Carved(0.9f), FVector(500.f, 260.f, 40.f), FVector(-H->Rx * 0.5f, 0.f, H->Lake + 20.f));
			F.B.Box(FVector(-H->Rx * 0.5f, 0.f, H->Lake + 20.f), FVector(250.f, 130.f, 20.f));
		}
		else if (Id == TEXT("HG10"))
		{
			// The Heart Wall: the chamber's living membrane over the door, its glow pulsing at 35 beats a minute (the boss
			// pulses it); here its frame of grown ribs.
			for (int32 r = 0; r < 9; ++r)
			{
				const float Ang = PI * (r / 8.f);
				F.B.M.Tube({ Tw * 900.f + FVector(0, 0, 0.f) + FVector::CrossProduct(FVector::UpVector, Tw) * FMath::Cos(Ang) * 700.f, Tw * 950.f + FVector::CrossProduct(FVector::UpVector, Tw) * FMath::Cos(Ang) * 650.f + FVector(0, 0, FMath::Sin(Ang) * 800.f + 100.f) }, 40.f, 8, Rgb(214, 196, 160) * 0.6f, 0.7f);
			}
		}
		else if (Id == TEXT("HG7"))
		{
			// The Seed Vault's round door (its seal opens only in the Sporefall: the event's door).
			F.B.K.Add(Carved(0.8f), FVector(60.f, 420.f, 420.f), FVector(0, 0, 210.f) + Tw * 0.f, FRotator(0.f, 0.f, 0.f));
		}
		else if (Id == TEXT("HG9"))
		{
			// The Heart Gate: the one-way stone door up to the river and the Fall Field (it opens from this side).
			for (int32 s = 0; s < 12; ++s) { const float A0 = 2.f * PI * s / 12.f; F.B.K.Add(Carved(0.85f), FVector(70.f, 100.f, 120.f), FVector(0, 0, 450.f) + FVector(0.f, FMath::Cos(A0) * 420.f, FMath::Sin(A0) * 420.f), FRotator(0.f, 0.f, FMath::RadiansToDegrees(A0))); }
		}
		// Caplings' grazing caps along every room's walls.
		for (int32 c = 0; c < 10; ++c)
		{
			const float Ang = 2.f * PI * c / 10.f;
			Mushroom(F, FVector(FMath::Cos(Ang) * H->Rx * 0.85f, FMath::Sin(Ang) * H->Ry * 0.85f, -4.f), FVector::UpVector, 40.f + 60.f * FgH(Sd, c), 25.f + 30.f * FgH(Sd, c + 10), ECap::Honey, 0.6f, Sd + c, false);
		}
	}

	void HeartChamber(FFg& F, const FData::FHall& H)
	{
		// The Heart chamber (fungal 2.14): six Deepfarmer stone terraces round the edge (1.5 m high, 10 m wide), each with a
		// summited elder statue-body at its back; the Heart pool of ankle-deep amber sap (no slow); the four Draught gates'
		// stone frames (their plugs are the boss's); the amber membrane is the rock's own glow.
		const FData& D = FData::Get();
		for (const FData::FAngled& T : D.Terraces)
		{
			const FVector C = F.B.Local(T.At);
			const FRotator R(0.f, T.Angle + 90.f, 0.f);
			F.B.K.Add(Carved(0.9f), FVector(1000.f, 600.f, 150.f), C + FVector(0, 0, 75.f), R);
			F.B.Box(C + FVector(0, 0, 75.f), FVector(500.f, 300.f, 75.f), R);
			for (int32 s = 0; s < 3; ++s) F.B.K.Add(Carved(0.85f), FVector(300.f, 60.f, 50.f * (s + 1)), C - R.RotateVector(FVector(0.f, 330.f + s * 60.f, 0.f)) + FVector(0, 0, 25.f * (s + 1)), R);
			const FVector Back = C + FRotator(0.f, T.Angle, 0.f).Vector() * 280.f + FVector(0, 0, 150.f);
			Body(F, Back + FVector(0, 0, 120.f), -FRotator(0.f, T.Angle, 0.f).Vector(), EPose::Elder, 401 + T.I);
		}
		const FVector Pool = F.B.Local(D.PoolAt);
		F.B.M.Lathe(Pool + FVector(0, 0, 10.f), FVector::UpVector, 3.f, { FVector(0.f, D.PoolR, D.PoolR), FVector(1.f, D.PoolR, D.PoolR) }, 40, Rgb(232, 160, 64) * 0.45f, 0.1f);
		F.Glow.Lathe(Pool + FVector(0, 0, 6.f), FVector::UpVector, 2.f, { FVector(0.f, D.PoolR * 0.95f, D.PoolR * 0.95f), FVector(1.f, D.PoolR * 0.9f, D.PoolR * 0.9f) }, 32, AmberC(), 0.5f);
		for (const FData::FAngled& G : D.Gates)
		{
			const FVector C = F.B.Local(G.At);
			const FRotator R(0.f, G.Angle, 0.f);
			for (const float S : { -1.f, 1.f }) F.B.K.Add(Carved(0.85f), FVector(140.f, 120.f, 900.f), C + R.RotateVector(FVector(0.f, S * 360.f, 450.f)), R);
			F.B.K.Add(Carved(0.8f), FVector(160.f, 860.f, 120.f), C + FVector(0, 0, 900.f), R);
		}
		F.B.Lamp(FVector(0, 0, 3000.f), AmberC(), 25000.f, 9000.f);
	}

	// ---------------------------------------------------------------- the cave kit and the tubes

	void CaveKit(FFg& F, AWRFungalScenery* A)
	{
		const FData& D = FData::Get();
		if (!D.Kit.IsValidIndex(A->Index)) return;
		const FData::FKit& K = D.Kit[A->Index];
		const int32 Sd = A->Index * 11 + 9;
		if (K.Kind == TEXT("glowcap_ring")) GlowRing(F, FVector(0, 0, -4.f), Sd);
		else if (K.Kind == TEXT("host_cluster")) Bones(F, FVector(0, 0, 2.f), FgH(Sd, 1) * 360.f, 0.8f + 0.5f * K.V, Sd);
		else if (K.Kind == TEXT("puffball_field")) { for (int32 t = 0; t < 6; ++t) { const FVector P((FgH(Sd, t) - 0.5f) * 1000.f, (FgH(Sd, t + 9) - 0.5f) * 1000.f, -4.f); Mushroom(F, P, FVector::UpVector, 30.f, 50.f + 20.f * FgH(Sd, t), ECap::Puff, 0.f, Sd + t, false); } }
		else if (K.Kind == TEXT("ore_pocket")) { for (int32 r = 0; r < 4; ++r) F.B.Rock(r + Sd, FVector(150.f, 130.f, 100.f) * (0.6f + FgH(Sd, r)), FVector((r - 1.5f) * 200.f, 150.f, 0.f), FRotator(0.f, FgH(Sd, r + 4) * 360.f, 0.f), Rgb(110, 104, 96) * 0.6f, true, 20.f); }
		else if (K.Kind == TEXT("summited_niche"))
		{
			// A Deepfarmer clamped high on a ledge (on a root grown out of the wall), a cache below.
			FVector W, Nr;
			if (FgWall(D, A->GetActorLocation() + FVector(0, 0, 500.f), FVector(FMath::Cos(Sd * 0.3f), FMath::Sin(Sd * 0.3f), 0.f), 4000.f, W, &Nr))
			{
				const FVector L = F.B.Local(W);
				const FVector N = F.B.T.InverseTransformVectorNoScale(Nr).GetSafeNormal2D();
				F.B.M.Tube({ L - N * 30.f, L + N * 150.f + FVector(0, 0, 30.f), L + N * 260.f + FVector(0, 0, 10.f) }, 18.f, 7, RhizoC(), 0.4f);
				{ const FData::FHall* HC = D.FindHall(TEXT("HC")); Body(F, L + N * 230.f + FVector(0, 0, 14.f), F.B.T.InverseTransformVectorNoScale(((HC ? HC->At : W) - W).GetSafeNormal2D()), EPose::Beam, Sd); }
			}
		}
		else if (K.Kind == TEXT("mite_nest")) { for (int32 e = 0; e < 12; ++e) F.B.Ball(FVector(FMath::Cos(e * 0.8f) * 140.f, FMath::Sin(e * 0.8f) * 140.f, 20.f + 25.f * (e % 3)), 14.f + 8.f * FgH(Sd, e), Rgb(220, 214, 196) * 0.6f, 0.6f, 0.9f); }
		else if (K.Kind == TEXT("web_pocket")) { for (int32 c = 0; c < 3; ++c) F.Veil.Lathe(FVector((c - 1) * 200.f, 100.f, 300.f), FVector::UpVector, 120.f, { FVector(0.f, 10.f, 10.f), FVector(0.5f, 40.f, 40.f), FVector(1.f, 8.f, 8.f) }, 10, Rgb(226, 226, 218), 0.18f); }
		else if (K.Kind == TEXT("glowgrub_colony")) { for (int32 g = 0; g < 10; ++g) F.Glow.Lathe(FVector(FMath::Cos(g * 0.6f) * 200.f, FMath::Sin(g * 0.6f) * 200.f, 40.f + 30.f * (g % 3)), FVector::UpVector, 30.f, { FVector(0.f, 10.f, 10.f), FVector(1.f, 4.f, 4.f) }, 6, GrubC(), 0.9f); F.B.Lamp(FVector(0, 0, 150.f), GrubC(), 600.f, 1200.f); }
		else if (K.Kind == TEXT("capling_meadow")) { for (int32 c = 0; c < 20; ++c) Mushroom(F, FVector((FgH(Sd, c) - 0.5f) * 1400.f, (FgH(Sd, c + 30) - 0.5f) * 1400.f, -4.f), FVector::UpVector, 14.f + 18.f * FgH(Sd, c + 60), 8.f + 10.f * FgH(Sd, c + 90), ECap::Honey, 0.45f, Sd + c, false); }
	}

	void TubeFurniture(FFg& F, AWRFungalScenery* A)
	{
		const FData& D = FData::Get();
		if (!D.Tubes.IsValidIndex(A->Index)) return;
		const FData::FTube& T = D.Tubes[A->Index];
		const int32 Sd = A->Index * 7 + 1;
		if (T.Kind == TEXT("stair"))
		{
			// The Long Stair: steps worn into troughs by bare feet going down (each tread a slab whose middle is lower), a
			// knocker idol at every landing (every 40 m).
			float Acc = 0.f, NextIdol = 1000.f;
			for (int32 i = 0; i + 1 < T.Pts.Num(); ++i)
			{
				const FVector P0 = F.B.Local(T.Pts[i]), P1 = F.B.Local(T.Pts[i + 1]);
				const FVector Dir = (P1 - P0).GetSafeNormal2D();
				const FRotator R = Dir.Rotation();
				for (int32 s = 0; s < 3; ++s)
				{
					const FVector Q = FMath::Lerp(P0, P1, s / 3.f);
					F.B.K.Add(Limestone(0.85f + 0.08f * FgH(Sd, i * 3 + s)), FVector(90.f, T.W * 0.42f, 14.f), Q + R.RotateVector(FVector(0.f, -T.W * 0.22f, 3.f)), R);
					F.B.K.Add(Limestone(0.85f + 0.08f * FgH(Sd, i * 3 + s + 1)), FVector(90.f, T.W * 0.42f, 14.f), Q + R.RotateVector(FVector(0.f, T.W * 0.22f, 3.f)), R);
				}
				Acc += FVector::Dist2D(P0, P1);
				if (Acc > NextIdol)
				{
					NextIdol += 4000.f;
					Knocker(F, P0 + R.RotateVector(FVector(0.f, T.W * 0.38f, 0.f)), -Dir, 1.f, Sd + i);
				}
			}
		}
		else if (T.bStreet)
		{
			// The city's streets: amber lamps in grown niches every 30 m, mycelium mats underfoot.
			float Acc = 0.f;
			for (int32 i = 0; i + 1 < T.Pts.Num(); ++i)
			{
				Acc += FVector::Dist2D(T.Pts[i], T.Pts[i + 1]);
				if (Acc < 3000.f) continue;
				Acc = 0.f;
				const FVector P = F.B.Local(T.Pts[i]);
				const FVector Side = FVector::CrossProduct(FVector::UpVector, F.B.T.InverseTransformVectorNoScale((T.Pts[i + 1] - T.Pts[i]).GetSafeNormal2D()));
				F.Glow.Lathe(P + Side * T.R * 0.8f + FVector(0, 0, 220.f), FVector::UpVector, 50.f, { FVector(0.f, 14.f, 14.f), FVector(0.5f, 22.f, 22.f), FVector(1.f, 10.f, 10.f) }, 8, AmberC(), 1.f);
				F.B.Lamp(P + Side * T.R * 0.5f + FVector(0, 0, 250.f), AmberC(), 1400.f, 2200.f);
			}
		}
		else if (T.bFlue)
		{
			// The river's mouth into the Fall Field: the shaft's daylight falling in on wet gravel.
			F.B.Lamp(F.B.Local(T.Pts.Last()) + FVector(0, 0, 500.f), FLinearColor(0.87f, 0.91f, 0.94f), 3000.f, 2500.f);
		}
		else if (T.bRiver || T.Flooded > 0.f)
		{
			// Gravel bars and a teal glow along the water (Glowfin, the waterline).
			for (int32 i = 6; i + 6 < T.Pts.Num(); i += 18) F.Glow.Lathe(F.B.Local(T.Pts[i]) + FVector(0, 0, 40.f), FVector::UpVector, 2.f, { FVector(0.f, 220.f, 100.f), FVector(1.f, 200.f, 90.f) }, 10, TealC(), 0.3f);
		}
		else
		{
			// Ordinary tubes: glow-caps on the walls and floor edges every 25 m so a tube is never black.
			for (int32 i = 4; i + 1 < T.Pts.Num(); i += 8)
			{
				const FVector P = F.B.Local(T.Pts[i]);
				const FVector Side = FVector::CrossProduct(FVector::UpVector, F.B.T.InverseTransformVectorNoScale((T.Pts[i + 1] - T.Pts[i]).GetSafeNormal2D()));
				const float S = (i / 8) % 2 ? 1.f : -1.f;
				Mushroom(F, P + Side * S * T.Half() * 0.62f + FVector(0, 0, -4.f), FVector::UpVector + Side * S * -0.2f, 30.f + 20.f * FgH(Sd, i), 18.f + 10.f * FgH(Sd, i + 3), ECap::Glowcap, 0.7f, Sd + i, false);
			}
		}
	}

	void ShaftFurniture(FFg& F, AWRFungalScenery* A)
	{
		const FData& D = FData::Get();
		if (!D.Shafts.IsValidIndex(A->Index)) return;
		const FData::FShaft& S = D.Shafts[A->Index];
		if (S.Kind == TEXT("sieve"))
		{
			// The Sand Falls (L8): dunes pour through the Desert's Sieves as slow golden falls of sand to the hall's floor.
			const float Drop = S.Z1 - S.Z0 + 2000.f;
			for (int32 k = 0; k < 6; ++k) F.B.Loop(TEXT("fungal.sandFall"), FVector(0, 0, -Drop * (k + 0.5f) / 6.f), FRotator::ZeroRotator, S.R / 700.f);
			F.B.Lamp(FVector(0, 0, -Drop * 0.5f), GoldC(), 4000.f, 3000.f);
		}
		else if (S.Kind == TEXT("stair"))
		{
			// The ring's door's stair (F-S1): narrow steps spiralling down under the altar.
			float Ang = 0.f;
			for (float Z = -60.f; Z > S.Z0 - S.Z1; Z -= 30.f, Ang += 0.35f)
			{
				const FVector Dir(FMath::Cos(Ang), FMath::Sin(Ang), 0.f);
				F.B.K.Add(Carved(0.9f), FVector(S.R * 0.9f, 70.f, 20.f), Dir * S.R * 0.45f + FVector(0, 0, Z), FRotator(0.f, FMath::RadiansToDegrees(Ang), 0.f));
				F.B.Box(Dir * S.R * 0.45f + FVector(0, 0, Z), FVector(S.R * 0.45f, 35.f, 10.f), FRotator(0.f, FMath::RadiansToDegrees(Ang), 0.f));
			}
		}
	}

	void HallFurniture(FFg& F, AWRFungalScenery* A)
	{
		const FData& D = FData::Get();
		if (!D.Halls.IsValidIndex(A->Index)) return;
		const FData::FHall& H = D.Halls[A->Index];
		if (H.Id == TEXT("HC")) { HeartChamber(F, H); return; }
		if (H.bRoom) return;   // the city's rooms are their sites
		if (H.Id == TEXT("H6")) SandFallsHall(F, H);
		if (H.Zone == TEXT("F2")) GardenHall(F, A, H);
		else if (H.Zone == TEXT("F3")) HallsHall(F, A, H);
		else if (H.Zone == TEXT("F4")) WebsHall(F, A, H);
		else if (H.Zone == TEXT("F5")) GrottoHall(F, A, H);
		// The shrines in this hall: their dressing (the great one's ring of caps round the basin where a deepcap grows).
		for (const FData::FShrine& S : D.Shrines)
		{
			if (S.Hall != H.Id) continue;
			const FVector L = F.B.Local(S.At);
			if (S.bGreat)
			{
				for (int32 c = 0; c < 7; ++c) { const float Ang = 2.f * PI * c / 7.f; Mushroom(F, L + FVector(FMath::Cos(Ang) * 520.f, FMath::Sin(Ang) * 520.f, -4.f), FVector::UpVector, 400.f, 160.f, ECap::Parasol, 0.7f, 500 + c, true); }
				F.B.M.Lathe(L + FVector(300.f, 0.f, 0.f), FVector::UpVector, 60.f, { FVector(0.f, 60.f, 60.f), FVector(1.f, 110.f, 110.f) }, 16, StoneC(), 0.85f);
				F.B.M.Lathe(L + FVector(300.f, 0.f, 50.f), FVector::UpVector, 2.f, { FVector(0.f, 100.f, 100.f), FVector(1.f, 100.f, 100.f) }, 16, Rgb(30, 140, 134) * 0.35f, 0.04f);
				Mushroom(F, L + FVector(300.f, 0.f, 50.f), FVector::UpVector, 28.f, 16.f, ECap::Deep, 1.f, 510, false);
			}
			else
			{
				ColdBrazier(F, L + FVector(0, 0, 0.f), 1.f);
			}
		}
	}
}

bool WRFungalUnder::Build(WRFungalLook::FFg& F, AWRFungalScenery* A)
{
	using namespace WRFungalUnderImpl;
	const FName K = A->Kind;
	const FWRFungalData& D = FWRFungalData::Get();
	if (K == TEXT("hall")) HallFurniture(F, A);
	else if (K == TEXT("tube")) TubeFurniture(F, A);
	else if (K == TEXT("shaft")) ShaftFurniture(F, A);
	else if (K == TEXT("false_sky")) FalseSky(F, A);
	else if (K == TEXT("shoestring")) Shoestring(F, A);
	else if (K == TEXT("drowned_granary")) DrownedGranary(F, A);
	else if (K == TEXT("sand_falls")) {}   // the hall's own furniture and the Sieves' falls
	else if (K == TEXT("fall_field")) FallField(F, A);
	else if (K == TEXT("fallen_champion")) Champion(F, A);
	else if (K == TEXT("offering_niches")) OfferingNiches(F, A);
	else if (K == TEXT("garden_farm")) GardenFarm(F, A);
	else if (K == TEXT("springcap_grove")) GroveLedge(F, A);
	else if (K == TEXT("cloud_room")) CloudRoom(F, A);
	else if (K == TEXT("lure_gallery")) LureGallery(F, A);
	else if (K == TEXT("dunes_within")) {}   // the Sand Falls hall builds the dunes and the caravan
	else if (K == TEXT("knockers_junction")) KnockersJunction(F, A);
	else if (K == TEXT("spore_archive")) SporeArchive(F, A);
	else if (K == TEXT("saltpetre_walls")) SaltpetreWalls(F, A);
	else if (K == TEXT("silk_larders")) SilkLarders(F, A);
	else if (K == TEXT("root_ceiling")) RootCeiling(F, A);
	else if (K == TEXT("titan_nursery")) TitanNursery(F, A);
	else if (K == TEXT("sealed_gate")) SealedGateFrame(F, A);
	else if (K == TEXT("grotto_weir")) GrottoWeir(F, A);
	else if (K == TEXT("air_bell")) AirBell(F, A);
	else if (K == TEXT("vat_room")) VatRoom(F, A);
	else if (K == TEXT("toad_leaps")) ToadLeaps(F, A);
	else if (K.ToString().StartsWith(TEXT("hg_"))) HeartRoom(F, A);
	else if (D.Kit.IsValidIndex(A->Index) && D.Kit[A->Index].Id == A->SiteId) CaveKit(F, A);
	else return false;
	return true;
}
