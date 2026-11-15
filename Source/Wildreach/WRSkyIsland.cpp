// One floating island of the Sky (cooks/biome-sky.md 2.2, cooks/build-biome-sky.md 2.2): built in code from its row in
// Data/sky.json, on every machine, the same collision everywhere.
//
// Shape (real reference: the Hallelujah / Zhangjiajie pillars, the Faroe sea cliffs): a domed turf top (terraced on the
// Kitewrights' farm islands, a basin on the rain lagoon, a still pool, the Kitefield's hill, the Drowned Garden's 30-degree
// tilt), the turf rolling over the lip; a sheer rim 8-25 m of bare rock in strata ledges; then the rock root tapering
// 1.5-3x the radius down to the levistone keel, its lower third bearded with pale blue crystal that glows cyan at night
// (the islands read as floating shapes in the dark). Islands nearest the Flue have their undersides crusted with the
// Deepfarmers' mycelium and a cyan spore glow. The top, the rim and the root are one ground-material mesh with
// collision (walking off the lip is a real fall); the crystals are look blocks (day: pale glass; night: lit cores).

#include "WRSkyWorld.h"
#include "WRSkyIsleTypes.h"
#include "WRSkyLook.h"
#include "WRForestHazards.h"
#include "WRLand.h"
#include "WRTuning.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"

namespace WRSkyIslandImpl
{
	using namespace WRSkyLook;

	float N2(float X, float Y, int32 Seed)
	{
		// Smooth value noise (-1..1), seeded.
		const int32 X0 = FMath::FloorToInt(X), Y0 = FMath::FloorToInt(Y);
		const float FX = X - X0, FY = Y - Y0;
		auto H = [Seed](int32 A, int32 B) { return FMath::Frac(FMath::Sin(float(A) * 127.1f + float(B) * 311.7f + float(Seed) * 74.7f) * 43758.5453f); };
		const float SX = FX * FX * (3.f - 2.f * FX), SY = FY * FY * (3.f - 2.f * FY);
		const float A = FMath::Lerp(H(X0, Y0), H(X0 + 1, Y0), SX), B = FMath::Lerp(H(X0, Y0 + 1), H(X0 + 1, Y0 + 1), SX);
		return FMath::Lerp(A, B, SY) * 2.f - 1.f;
	}

	struct FMeshOut
	{
		TArray<FVector> V;
		TArray<int32> T;
		TArray<FVector> N;
		TArray<FVector2D> UV;
		TArray<FLinearColor> C;
		TArray<FProcMeshTangent> Tan;
		int32 Add(const FVector& P, const FVector& Nrm, const FLinearColor& Col, const FVector& World)
		{
			V.Add(P);
			N.Add(Nrm);
			C.Add(Col);
			UV.Add(FVector2D(World.X, World.Y) / 400.f + FVector2D(0.f, World.Z / 400.f));
			Tan.Add(FProcMeshTangent(FVector(1.f, 0.f, 0.f), false));
			return V.Num() - 1;
		}
		void Quad(int32 A, int32 B, int32 C2, int32 D) { T.Append({ A, B, C2, A, C2, D }); }
	};

	/** The top's colour at a world point: the zone's turf, the kind's dressing (beds, paving, bone-white shore, wet grey),
	 *  rock showing at the lip, spore crust near the Flue. Alpha carries 1 - roughness for M_LandGround. */
	FLinearColor TopColour(const FWRSkyIsleData::FIsland& I, const FVector& W, float Rho, float Slope, bool bNearFlue)
	{
		const float Nz = N2(W.X / 900.f, W.Y / 900.f, I.Seed) * 0.5f + 0.5f;
		const float Fine = N2(W.X / 140.f, W.Y / 140.f, I.Seed + 7) * 0.5f + 0.5f;
		FLinearColor Turf = FMath::Lerp(SRGB(0x8F, 0xA7, 0x7A), SRGB(0x9D, 0xB5, 0x9A), Nz);     // wind-combed silver-green
		float Rough = 0.9f;
		switch (I.Zone)
		{
		case '2': Turf = FMath::Lerp(SRGB(0x86, 0xA8, 0x6A), SRGB(0xB9, 0xA5, 0x5A), Nz * 0.45f); break;   // moss gold
		case '3': Turf = FMath::Lerp(SRGB(0x8E, 0x9A, 0x78), SRGB(0xB7, 0xB3, 0xAA), 0.35f + 0.4f * Nz); break;  // thin grass on granite
		case '4': Turf = FMath::Lerp(SRGB(0x9A, 0xAE, 0x8A), SRGB(0xD8, 0xD6, 0xCC), 0.25f * Nz); break;   // cold, short
		case '5': Turf = FMath::Lerp(SRGB(0x6E, 0x7A, 0x66), SRGB(0x7E, 0x86, 0x7E), Nz); Rough = 0.6f; break; // wet grey
		default: break;
		}
		FLinearColor Col = Turf;
		if (I.Kind == TEXT("rock")) Col = FMath::Lerp(SRGB(0xB7, 0xB3, 0xAA), Turf, 0.35f * Fine);
		if (I.Kind == TEXT("temple"))
		{
			// White paving in 1.8 m slabs with dark joints, grass in the cracks toward the rim.
			const float Jx = FMath::Abs(FMath::Fmod(W.X + 100000.f, 180.f) - 90.f), Jy = FMath::Abs(FMath::Fmod(W.Y + 100000.f, 180.f) - 90.f);
			const bool bJoint = Jx > 85.f || Jy > 85.f;
			Col = Rho < 0.82f ? (bJoint ? SRGB(0x8E, 0x8A, 0x80) : FMath::Lerp(SRGB(0xF0, 0xED, 0xE6), SRGB(0xD8, 0xD3, 0xC8), Fine)) : FMath::Lerp(Turf, SRGB(0xE9, 0xE4, 0xD8), 0.4f * Fine);
			Rough = 0.75f;
		}
		if (I.Kind == TEXT("garden") || I.Terraces > 0)
		{
			// Old beds gone wild: flax-blue drifts and cloudberry amber in rows along the terraces.
			const float Row = FMath::Abs(FMath::Sin(Rho * 40.f));
			if (Row > 0.86f) Col = FMath::Lerp(Col, SRGB(0x6F, 0x8F, 0xC9), 0.55f * Nz);
			else if (Row < 0.12f && Fine > 0.6f) Col = FMath::Lerp(Col, SRGB(0xE0, 0xA0, 0x40), 0.45f);
		}
		if (I.bLagoon && Rho > 0.62f && Rho < 0.8f) Col = FMath::Lerp(Col, SRGB(0xE8, 0xE2, 0xD8), 0.75f);   // the bone-white ring
		// Bare granite through the turf where it's steep and at the lip (the turf overhangs it).
		const float Rock = FMath::Clamp((Slope - 0.45f) * 2.5f, 0.f, 1.f) + FMath::Clamp((Rho - 0.95f) * 12.f, 0.f, 0.6f);
		Col = FMath::Lerp(Col, RimColour(I.Zone, Fine), FMath::Clamp(Rock, 0.f, 1.f));
		if (bNearFlue || I.bSpore)
		{
			// The spore crust: grey over everything nearest the Flue, cyan specks.
			Col = FMath::Lerp(Col, SRGB(0x8A, 0x8A, 0x80), I.bSpore ? 0.7f : 0.3f);
			if (Fine > 0.93f) Col = SRGB(0x39, 0xE6, 0xC2) * 0.6f;
		}
		Col.A = 1.f - Rough;
		return Col;
	}
}

void AWRSkyScenery::BuildIsland()
{
	using namespace WRSkyIslandImpl;
	const FWRSkyIsleData& D = FWRSkyIsleData::Get();
	if (!D.Islands.IsValidIndex(Index)) return;
	const FWRSkyIsleData::FIsland& I = D.Islands[Index];
	const bool bServer = GetNetMode() == NM_DedicatedServer;
	const FTransform Xf = GetActorTransform();
	const FVector C0 = I.Centre;
	const bool bNearFlue = FVector2D::Distance(FVector2D(C0), D.FlueAt) < 60000.f;
	const float R = I.R();

	FMeshOut Top, Rim;
	// ---- the top: a polar grid out to the outline (rings close in at the lip, where the roll-over is) ----
	const int32 NR = FMath::Clamp(int32(R / 500.f), 10, 44);
	const int32 NS = FMath::Clamp(int32(2.f * PI * R / 450.f), 40, 180);
	auto RhoAt = [NR](int32 k) { const float t = float(k) / NR; return 1.f - FMath::Square(1.f - t) * 0.6f - (1.f - t) * 0.4f; };
	auto LocalAt = [&](float A, float Rho) { return FVector2D(FMath::Cos(A), FMath::Sin(A)) * I.EdgeRadius(A) * Rho; };
	auto WorldOf = [&](const FVector2D& L) { const FVector2D W = I.World2D(L) - FVector2D(WRSkyIsles::DriftOf(I.Index)); return FVector(W.X, W.Y, 0.f); };
	auto TopAt = [&](const FVector2D& L) { const FVector W = WorldOf(L); return I.TopZ(W + WRSkyIsles::DriftOf(I.Index)) - WRSkyIsles::DriftOf(I.Index).Z - C0.Z; };
	{
		const FVector2D L0(0, 0);
		const FVector W = WorldOf(L0);
		Top.Add(FVector(0, 0, TopAt(L0)), FVector::UpVector, TopColour(I, W, 0.f, 0.f, bNearFlue), W + FVector(0, 0, C0.Z));
	}
	TArray<TArray<int32>> TopRings;
	for (int32 k = 1; k <= NR; ++k)
	{
		TArray<int32>& Row = TopRings.AddDefaulted_GetRef();
		const float Rho = RhoAt(k);
		for (int32 s = 0; s < NS; ++s)
		{
			const float A = 2.f * PI * s / NS;
			const FVector2D L = LocalAt(A, Rho);
			const float Z = TopAt(L);
			// Normal from the surface's slope (a cross of two small steps).
			const FVector2D Dx(60.f, 0.f), Dy(0.f, 60.f);
			const FVector Tx(120.f, 0.f, TopAt(L + Dx) - TopAt(L - Dx)), Ty(0.f, 120.f, TopAt(L + Dy) - TopAt(L - Dy));
			const FVector Nrm = FVector::CrossProduct(Tx, Ty).GetSafeNormal();
			const FVector W = WorldOf(L);
			Row.Add(Top.Add(FVector(L.X, L.Y, Z), Nrm, TopColour(I, W, Rho, 1.f - Nrm.Z, bNearFlue), W + FVector(0, 0, C0.Z + Z)));
		}
	}
	for (int32 s = 0; s < NS; ++s) Top.T.Append({ 0, TopRings[0][(s + 1) % NS], TopRings[0][s] });
	for (int32 k = 0; k + 1 < TopRings.Num(); ++k)
	{
		for (int32 s = 0; s < NS; ++s)
		{
			const int32 S1 = (s + 1) % NS;
			Top.Quad(TopRings[k][s], TopRings[k][S1], TopRings[k + 1][S1], TopRings[k + 1][s]);
		}
	}

	// ---- the rim and the root: rings of the outline going down, sheer then tapering ----
	const float RimH = FMath::Clamp(R * 0.12f, 800.f, 2500.f);
	float Depth = I.RootDepth();
	{
		// The root never reaches the ground under it (the cape's cliffs under the Lift Steps): its tip stays 40 m clear.
		float GroundZ = 0.f;
		if (WRLand::GroundUnder(FVector(C0.X, C0.Y, C0.Z - 100.f), GroundZ)) Depth = FMath::Min(Depth, C0.Z - RimH - GroundZ - 4000.f);
		Depth = FMath::Max(Depth, 1500.f);
	}
	// Caves in the rim (the Weeping Isle's three): a mouth's angle (local) and its floor (local z) to leave open.
	struct FMouth { float A = 0.f; float Z = 0.f; };
	TArray<FMouth> Mouths;
	for (const FWRSkyIsleData::FSite& S : D.Sites)
	{
		if (S.Island != I.Id || S.Kind != TEXT("weeping_cave")) continue;
		const FVector2D L = I.Local(S.At);
		Mouths.Add({ float(FMath::Atan2(L.Y, L.X)), -1300.f });
	}
	const int32 RimRows = FMath::Max(3, int32(RimH / 260.f));
	const int32 RootRows = 22;
	const FVector2D TipOff = FVector2D(N2(float(I.Seed), 1.3f, 3), N2(2.1f, float(I.Seed), 5)) * R * 0.18f;
	TArray<TArray<int32>> Wall;
	for (int32 r = 0; r <= RimRows + RootRows; ++r)
	{
		TArray<int32>& Row = Wall.AddDefaulted_GetRef();
		const bool bRim = r <= RimRows;
		const float Tr = bRim ? 0.f : float(r - RimRows) / RootRows;
		const float Z = bRim ? -RimH * float(r) / RimRows : -RimH - Depth * Tr;
		for (int32 s = 0; s < NS; ++s)
		{
			const float A = 2.f * PI * s / NS;
			const FVector2D Dir(FMath::Cos(A), FMath::Sin(A));
			float Out = I.EdgeRadius(A);
			FVector2D Off = FVector2D::ZeroVector;
			if (bRim)
			{
				// Strata: each 2.6 m course juts or steps back a little; the very top sits under the lip's roll.
				Out *= 0.995f + 0.012f * N2(A * 6.f, float(r) * 0.9f, I.Seed + 11);
			}
			else
			{
				// The root: tapering (1 - t)^0.8 with knuckled bulges, its axis drifting to an off-centre tip.
				const float Taper = FMath::Pow(1.f - Tr, 0.8f);
				Out *= Taper * (1.f + 0.14f * N2(A * 3.f, Tr * 4.f, I.Seed + 23) + 0.05f * N2(A * 9.f, Tr * 11.f, I.Seed + 29));
				Off = TipOff * FMath::Pow(Tr, 1.5f);
				Out = FMath::Max(Out, 30.f * (1.f - Tr) + 4.f);
			}
			const FVector2D L = Dir * Out + Off;
			const float Zr = r == 0 ? TopAt(Dir * I.EdgeRadius(A)) : Z;
			const FVector W = WorldOf(L) + FVector(0, 0, C0.Z + Zr);
			// Colour: the zone's rock, darker and bluer with depth (the keel shows through), spore-grey near the Flue.
			const float Fine = N2(W.X / 120.f, W.Z / 120.f, I.Seed + 3) * 0.5f + 0.5f;
			FLinearColor Col = RimColour(I.Zone, Fine);
			if (!bRim)
			{
				Col *= FMath::Lerp(1.f, 0.72f, Tr);
				if (Tr > 0.55f) Col = FMath::Lerp(Col, SRGB(0xA9, 0xC4, 0xD9) * 0.75f, FMath::Clamp((Tr - 0.55f) * 2.f, 0.f, 0.7f) * (0.5f + 0.5f * Fine));
				if (bNearFlue || I.bSpore) Col = FMath::Lerp(Col, SRGB(0x8A, 0x8A, 0x80), (I.bSpore ? 0.75f : 0.45f) * (1.f - Tr * 0.4f));
			}
			Col.A = bRim ? 0.12f : 0.1f;
			Row.Add(Rim.Add(FVector(L.X, L.Y, Zr), FVector(Dir.X, Dir.Y, bRim ? 0.f : -0.45f).GetSafeNormal(), Col, W));
		}
	}
	for (int32 r = 0; r + 1 < Wall.Num(); ++r)
	{
		for (int32 s = 0; s < NS; ++s)
		{
			const int32 S1 = (s + 1) % NS;
			// Leave a cave's mouth open (5 m wide, 4.5 m tall, floor 13 m under the rim).
			bool bHole = false;
			const float A = 2.f * PI * (s + 0.5f) / NS;
			const float Zm = -RimH * (float(r) + 0.5f) / RimRows;
			if (r < RimRows)
			{
				for (const FMouth& M : Mouths)
				{
					const float Da = FMath::Abs(FMath::FindDeltaAngleRadians(A, M.A)) * I.EdgeRadius(M.A);
					if (Da < 260.f && Zm < M.Z + 470.f && Zm > M.Z - 30.f) bHole = true;
				}
			}
			if (!bHole) Rim.Quad(Wall[r][s], Wall[r + 1][s], Wall[r + 1][S1], Wall[r][S1]);
		}
	}
	// The root's tip: a fan to the last ring's centre.
	{
		const TArray<int32>& Last = Wall.Last();
		FVector Mid = FVector::ZeroVector;
		for (int32 Ix : Last) Mid += Rim.V[Ix];
		Mid /= Last.Num();
		const int32 Tip = Rim.Add(Mid - FVector(0, 0, 300.f), -FVector::UpVector, SRGB(0xA9, 0xC4, 0xD9) * 0.7f, Xf.TransformPosition(Mid));
		for (int32 s = 0; s < NS; ++s) Rim.T.Append({ Last[s], Tip, Last[(s + 1) % NS] });
	}

	UMaterialInterface* Ground = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_LandGround.M_LandGround"));
	const float Cull = WRTune(TEXT("skyWorld.islandCull"), 0.f);
	for (FMeshOut* Out : { &Top, &Rim })
	{
		UProceduralMeshComponent* P = NewObject<UProceduralMeshComponent>(this);
		P->SetupAttachment(GetRootComponent());
		P->bUseAsyncCooking = true;
		P->SetCollisionProfileName(TEXT("BlockAll"));
		P->SetCollisionObjectType(ECC_WorldStatic);
		P->SetCanEverAffectNavigation(false);
		P->CreateMeshSection_LinearColor(0, Out->V, Out->T, Out->N, Out->UV, Out->C, Out->Tan, true);
		if (Ground && !bServer) P->SetMaterial(0, Ground);
		if (Cull > 0.f) P->SetCullDistance(Cull);
		P->RegisterComponent();
	}

	// ---- the levistone keel: crystal beards on the root's lower third (the Weeping Isle's 60 m beard) ----
	WRForestLook::FBuild B(this, 0.f);
	const int32 Crystals = FMath::Clamp(int32(R / 260.f) + int32(I.Beard / 300.f), 6, 70);
	for (int32 k = 0; k < Crystals; ++k)
	{
		const float A = 2.f * PI * B.Rand(k * 3 + 1);
		const float Tr = 0.55f + 0.42f * B.Rand(k * 3 + 2);
		const float Out = I.EdgeRadius(A) * FMath::Pow(1.f - Tr, 0.8f) * 0.92f;
		const FVector Base = FVector(FVector2D(FMath::Cos(A), FMath::Sin(A)) * Out + TipOff * FMath::Pow(Tr, 1.5f), -RimH - Depth * Tr);
		const float Len = (I.Beard > 0.f ? 600.f + I.Beard * 0.3f * B.Rand(k * 5 + 4) : 250.f + 650.f * B.Rand(k * 5 + 4)) * (0.6f + 0.4f * (1.f - Tr));
		const FVector Dir = FVector(FMath::Cos(A) * 0.35f, FMath::Sin(A) * 0.35f, -1.f).GetSafeNormal();
		const float Diam = Len * (0.16f + 0.08f * B.Rand(k * 7 + 5));
		B.K.Spike(Levistone(0.6f), Base - Dir * Diam * 0.4f, Dir, Diam, Len);
		B.N.Spike(LiftGlow(6.f), Base - Dir * Diam * 0.2f, Dir, Diam * 0.55f, Len * 1.03f);
		// Little ones round the big ones (a beard, not a spike).
		for (int32 j = 0; j < 2; ++j)
		{
			const FVector Side = FVector(FMath::Cos(A + 1.7f * (j ? 1 : -1)), FMath::Sin(A + 1.7f * (j ? 1 : -1)), 0.f) * Diam * 0.6f;
			B.K.Spike(Levistone(0.5f), Base + Side, (Dir + Side.GetSafeNormal() * 0.3f).GetSafeNormal(), Diam * 0.45f, Len * 0.45f);
		}
	}
	// A dim cyan light under the bigger islands at night, so the keels throw a glow on their own undersides.
	if (R > 7000.f) B.Lamp(FVector(TipOff, -RimH - Depth * 0.7f), SRGB(0xCF, 0xE9, 0xFF), 3500.f + R * 0.15f, R * 1.4f);
	// Spore crust's cyan glow under the islands nearest the Flue (the Deepfarmers' mycelium).
	if (bNearFlue || I.bSpore)
	{
		for (int32 k = 0; k < 10; ++k)
		{
			const float A = 2.f * PI * B.Rand(k * 11 + 3);
			const float Tr = 0.1f + 0.4f * B.Rand(k * 11 + 4);
			const float Out = I.EdgeRadius(A) * FMath::Pow(1.f - Tr, 0.8f);
			const FVector P = FVector(FVector2D(FMath::Cos(A), FMath::Sin(A)) * Out * 0.98f, -RimH - Depth * Tr);
			B.Ball(P, 90.f + 110.f * B.Rand(k), SRGB(0x8A, 0x8A, 0x80) * 0.55f, 0.95f, 0.4f);
			B.N.Add(WRSkyLook::SporeGlow(4.f), FVector(90.f, 70.f, 24.f), P + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * 40.f, FRotator(0.f, FMath::RadiansToDegrees(A), 0.f), EWRLookMesh::Cylinder);
		}
	}

	// ---- water: the lagoon's basin and the still pools (swimmable; the pearl lagoon is 12 m deep) ----
	if (I.bLagoon || I.bPool)
	{
		const float WZ = I.WaterZ() - WRSkyIsles::DriftOf(I.Index).Z - C0.Z;
		const float WR = I.bLagoon ? I.RMin() * 0.74f : I.RMin() * 0.26f;
		B.K.Disc(WRSkyLook::LagoonWater(), FVector(0, 0, WZ - 3.f), FVector::UpVector, WR * 2.f, 6.f);
		{
			// The swimmable water is every machine's (the movement it gives is predicted).
			FActorSpawnParameters SP;
			SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (AWRForestWaterVolume* V = GetWorld()->SpawnActor<AWRForestWaterVolume>(AWRForestWaterVolume::StaticClass(), FTransform(Xf.GetRotation(), Xf.GetLocation()), SP))
			{
				const float Deep = I.bLagoon ? 1300.f : 300.f;
				TArray<AWRForestWaterVolume::FBox2> Boxes;
				Boxes.Add({ FVector(0, 0, WZ - Deep * 0.5f), FVector(WR * 0.72f, WR * 0.72f, Deep * 0.5f), 0.f });
				Boxes.Add({ FVector(0, 0, WZ - Deep * 0.3f), FVector(WR * 0.92f, WR * 0.5f, Deep * 0.3f), 0.f });
				Boxes.Add({ FVector(0, 0, WZ - Deep * 0.3f), FVector(WR * 0.5f, WR * 0.92f, Deep * 0.3f), 0.f });
				V->Build(Boxes);
				V->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
			}
		}
	}

	// ---- the Weeping Isle's caves: a stair cut down the rim to each mouth (the third behind the middle fall, a 2 m
	// ledge), a tunnel 25 m in and a chamber 18 m across, inside the rock (dark, dripping, the ore and the cores) ----
	for (const FWRSkyIsleData::FSite& S : D.Sites)
	{
		if (S.Island != I.Id || S.Kind != TEXT("weeping_cave")) continue;
		const FVector2D L = I.Local(S.At);
		const float A = FMath::Atan2(L.Y, L.X);
		const FVector2D Out(FMath::Cos(A), FMath::Sin(A));
		const FVector2D Side(-Out.Y, Out.X);
		const float Edge = I.EdgeRadius(A);
		const float FloorZ = -1300.f;
		const FVector Mouth(Out * Edge, FloorZ);
		// The way down: the third cave's ledge runs along the face behind the fall; the others a cut stair.
		const bool bSecret = S.bSecret;
		const int32 Steps = 26;
		const FVector TopLip(Out * (Edge - 120.f) + Side * (bSecret ? 900.f : 700.f), TopAt(Out * (Edge - 120.f) + Side * (bSecret ? 900.f : 700.f)));
		for (int32 st = 0; st < Steps; ++st)
		{
			const float T0 = float(st) / Steps, T1 = float(st + 1) / Steps;
			const FVector P0 = FMath::Lerp(TopLip, Mouth + FVector(Side * 160.f, 0.f), T0) + FVector(Out * 90.f, 0.f);
			const FVector P1 = FMath::Lerp(TopLip, Mouth + FVector(Side * 160.f, 0.f), T1) + FVector(Out * 90.f, 0.f);
			const FVector Mid = (P0 + P1) * 0.5f;
			const float W = bSecret ? 200.f : 160.f;
			B.K.Add(RimStone(I.Zone, 0.9f), FVector(FVector::Dist2D(P0, P1) + 20.f, W, 40.f), Mid - FVector(0, 0, 20.f), FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(P1.Y - P0.Y, P1.X - P0.X)), 0.f));
			B.Box(Mid - FVector(0, 0, 20.f), FVector(FVector::Dist2D(P0, P1) * 0.5f + 10.f, W * 0.5f, 20.f), FRotator(FMath::RadiansToDegrees(FMath::Atan2(P1.Z - P0.Z, FVector::Dist2D(P0, P1))) * -1.f, FMath::RadiansToDegrees(FMath::Atan2(P1.Y - P0.Y, P1.X - P0.X)), 0.f));
			// A chain handrail on iron pins on the drop side (Kitewright work; the secret ledge has none).
			if (!bSecret && st % 4 == 0)
			{
				const FVector Pin = Mid + FVector(Out * (W * 0.5f - 10.f), 0.f);
				B.K.Rod(WRSkyLook::ChainIron(), Pin, Pin + FVector(0, 0, 100.f), 5.f);
				const float T2 = FMath::Min(1.f, float(st + 4) / Steps);
				const FVector Pin2 = FMath::Lerp(TopLip, Mouth + FVector(Side * 160.f, 0.f), T2) + FVector(Out * (90.f + W * 0.5f - 10.f), 0.f);
				B.Line(Pin + FVector(0, 0, 95.f), Pin2 + FVector(0, 0, 95.f), 2.2f, 18.f, WRSkyLook::ChainC());
			}
		}
		// The tunnel and the chamber: floor, walls, ceiling (rock blocks with collision), dripping, dark.
		const float TunnelLen = 2500.f;
		for (int32 t = 0; t < 10; ++t)
		{
			const float D0 = Edge - TunnelLen * float(t) / 10.f - 60.f;
			const FVector P(Out * D0, FloorZ);
			const FRotator Rot(0.f, FMath::RadiansToDegrees(A), 0.f);
			const float Wob = 40.f * N2(float(t), float(S.Cave), I.Seed);
			B.K.Add(RimStone(I.Zone, 0.65f), FVector(270.f, 560.f, 60.f), P - FVector(0, 0, 30.f), Rot);
			B.Box(P - FVector(0, 0, 30.f), FVector(140.f, 280.f, 30.f), Rot);
			for (int32 sd = -1; sd <= 1; sd += 2)
			{
				B.K.Add(RimStone(I.Zone, 0.55f), FVector(270.f, 80.f, 520.f), P + FVector(Side * (300.f + Wob) * sd, 250.f), Rot);
				B.Box(P + FVector(Side * (300.f + Wob) * sd, 250.f), FVector(140.f, 40.f, 260.f), Rot);
			}
			B.K.Add(RimStone(I.Zone, 0.5f), FVector(270.f, 640.f, 80.f), P + FVector(0, 0, 500.f + Wob), Rot);
			B.Box(P + FVector(0, 0, 500.f + Wob), FVector(140.f, 320.f, 40.f), Rot);
		}
		const FVector Ch(Out * (Edge - TunnelLen - 900.f), FloorZ);
		for (int32 w = 0; w < 14; ++w)
		{
			const float Wa = 2.f * PI * w / 14.f;
			const FVector2D Wd(FMath::Cos(Wa), FMath::Sin(Wa));
			// Leave the side the tunnel comes in by open.
			if (FVector2D::DotProduct(Wd, Out) > 0.93f) continue;
			const FVector P = Ch + FVector(Wd * 950.f, 300.f);
			B.K.Add(RimStone(I.Zone, 0.5f), FVector(80.f, 460.f, 700.f), P, FRotator(0.f, FMath::RadiansToDegrees(Wa), 0.f));
			B.Box(P, FVector(40.f, 230.f, 350.f), FRotator(0.f, FMath::RadiansToDegrees(Wa), 0.f));
		}
		B.K.Disc(RimStone(I.Zone, 0.6f), Ch - FVector(0, 0, 30.f), FVector::UpVector, 2000.f, 60.f);
		B.Box(Ch - FVector(0, 0, 30.f), FVector(980.f, 980.f, 30.f));
		B.K.Disc(RimStone(I.Zone, 0.45f), Ch + FVector(0, 0, 680.f), FVector::UpVector, 2000.f, 80.f);
		B.Box(Ch + FVector(0, 0, 680.f), FVector(980.f, 980.f, 40.f));
		// Its levistone cores glow from the walls; a drip loop; the third cave's Hushwing nest (SKY 2.8).
		for (int32 c = 0; c < 6; ++c)
		{
			const float Wa = A + PI + (c - 2.5f) * 0.55f;
			const FVector P = Ch + FVector(FVector2D(FMath::Cos(Wa), FMath::Sin(Wa)) * 900.f, 120.f + 260.f * B.Rand(c + 40));
			B.K.Spike(Levistone(0.8f), P, FVector(-FMath::Cos(Wa), -FMath::Sin(Wa), 0.3f).GetSafeNormal(), 60.f, 160.f);
			B.N.Spike(LiftGlow(5.f), P, FVector(-FMath::Cos(Wa), -FMath::Sin(Wa), 0.3f).GetSafeNormal(), 34.f, 168.f);
		}
		B.Lamp(Ch + FVector(0, 0, 250.f), SRGB(0xCF, 0xE9, 0xFF), 900.f, 1600.f);
		B.Loop(TEXT("sky.caveDrip"), Ch + FVector(0, 0, 600.f));
		if (bSecret)
		{
			const FVector Nest = Ch + FVector(-Out * 700.f, 0.f);
			for (int32 n = 0; n < 9; ++n) B.Limb(Nest + FVector(FMath::Cos(n * 0.7f) * 120.f, FMath::Sin(n * 0.7f) * 120.f, 10.f), Nest + FVector(FMath::Cos(n * 0.7f + 2.4f) * 120.f, FMath::Sin(n * 0.7f + 2.4f) * 120.f, 30.f), 6.f, 5.f, WRForestLook::BarkC(), 5);
		}
	}
	B.Finish();
}
