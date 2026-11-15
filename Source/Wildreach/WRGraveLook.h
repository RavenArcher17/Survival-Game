#pragma once

// The Graveyard's scenery kit (shared by WRGraveScenery*.cpp and WRGraveCatacombs.cpp): the Graveyard palette (GY 2.1:
// bruise violet, ash, chalk, bone, yew black-green, soul-green grave-light, soulstone blue-green) and the pieces every
// place is built from, on the forest's FBuild (WRForestSceneryKit.h: one site's day / night blocks, code mesh, collision
// boxes, tree kit, night lamps). Real measures (build-biome-graveyard.md 1): headstones 0.6-1.2 m tall and 8-15 cm thick
// at the head (west) end of a 2 m grave, facing east; old ones lean back as the grave sinks; tōrō are six parts (hōju,
// kasa, hibukuro, chūdai, sao, kiso); a mortsafe is an iron cage 1.9 x 0.8 x 1.1 m, bars 2.5 cm at 10 cm centres.
// Every free function is inline and lives in WRGraveLook (unity builds join the files).

#include "WRForestSceneryKit.h"
#include "WRLand.h"
#include "WRTuning.h"

namespace WRGraveLook
{
	using WRForestLook::FBuild;
	using FSurf = WRLookKit::FSurf;
	using WRForestLook::Rgb;

	// ---- the palette (linear, dimmed as the look kit's tinted blocks want it) ----
	inline FSurf Chalk(float K = 1.f) { return FSurf::Stone(Rgb(230, 225, 211) * 0.62f * K, 0.94f); }
	inline FSurf Limestone(float K = 1.f) { return FSurf::Stone(Rgb(180, 175, 164) * 0.58f * K, 0.9f); }
	inline FSurf GreyStone(float K = 1.f) { return FSurf::Stone(Rgb(138, 133, 128) * 0.55f * K, 0.9f); }
	inline FSurf DarkStone(float K = 1.f) { return FSurf::Stone(Rgb(90, 86, 82) * 0.55f * K, 0.9f); }
	inline FSurf Slate(float K = 1.f) { return FSurf::Stone(Rgb(72, 76, 82) * 0.6f * K, 0.7f); }
	inline FSurf Bone(float K = 1.f) { return FSurf::Stone(Rgb(217, 207, 184) * 0.66f * K, 0.78f); }
	inline FSurf Moss(float K = 1.f) { return FSurf::Cloth(Rgb(79, 91, 58) * 0.48f * K); }
	inline FSurf Lichen(float K = 1.f) { return FSurf::Cloth(Rgb(150, 156, 120) * 0.5f * K); }
	inline FSurf Verdigris(float K = 1.f) { return FSurf::Steel(Rgb(78, 140, 122) * 0.5f * K, 0.62f); }
	inline FSurf Bronze(float K = 1.f) { return FSurf::Steel(Rgb(122, 90, 52) * 0.55f * K, 0.45f); }
	inline FSurf Iron(float K = 1.f) { return FSurf::Steel(FLinearColor(0.03f, 0.028f, 0.027f) * K, 0.7f); }
	inline FSurf Rust(float K = 1.f) { return FSurf::Steel(Rgb(98, 52, 30) * 0.45f * K, 0.85f); }
	inline FSurf Timber(float K = 1.f) { return FSurf::Wood(Rgb(84, 68, 54) * 0.45f * K); }
	inline FSurf OldWood(float K = 1.f) { return FSurf::Wood(Rgb(110, 102, 92) * 0.45f * K); }
	inline FSurf YewWood(float K = 1.f) { return FSurf::Wood(Rgb(122, 74, 52) * 0.45f * K); }
	inline FSurf Black(float K = 1.f) { return FSurf::Wood(Rgb(26, 24, 24) * 0.6f * K); }
	inline FSurf Soil(float K = 1.f) { return FSurf::Cloth(Rgb(59, 53, 48) * 0.5f * K); }
	inline FSurf Turf(float K = 1.f) { return FSurf::Cloth(Rgb(110, 122, 94) * 0.45f * K); }
	inline FSurf Linen(float K = 1.f) { return FSurf::Cloth(Rgb(214, 208, 192) * 0.5f * K); }
	inline FSurf Sackcloth(float K = 1.f) { return FSurf::Cloth(Rgb(150, 132, 104) * 0.45f * K); }
	inline FSurf Wax(float K = 1.f) { return FSurf::Glaze(Rgb(232, 221, 176) * 0.62f * K, 0.55f); }
	inline FSurf Lime(float K = 1.f) { return FSurf::Stone(Rgb(237, 235, 224) * 0.72f * K, 0.96f); }
	inline FSurf Slag(float K = 1.f) { return FSurf::Glaze(Rgb(40, 38, 40) * 0.6f * K, 0.35f); }
	inline FSurf Paper(float K = 1.f) { return FSurf::Cloth(Rgb(232, 222, 190) * 0.55f * K); }
	/** Grave-light: the pale soul-green that never goes out (#9FE3C0). */
	inline FSurf GraveLight(float Glow) { return FSurf::Lit(Rgb(159, 227, 192) * 0.6f, Glow); }
	/** Candle flame / a lantern's warm light (#FFC870). */
	inline FSurf Candle(float Glow) { return FSurf::Lit(Rgb(255, 200, 112) * 0.6f, Glow); }
	/** Soulstone: clear with a pale-green inner glow fresh on bone (#2F7F86 once in air). */
	inline FSurf Soulstone(float Glow) { FSurf S = FSurf::Glass(Rgb(120, 200, 170) * 0.45f, Glow); return S; }
	inline FSurf SoulstoneBlue() { return FSurf::Glass(Rgb(47, 127, 134) * 0.5f, 0.f); }
	inline FSurf Water() { return FSurf::Glaze(Rgb(46, 60, 64) * 0.45f, 0.04f); }
	inline FLinearColor GraveLightC() { return FLinearColor(FColor(159, 227, 192)); }
	inline FLinearColor CandleC() { return FLinearColor(FColor(255, 200, 112)); }
	inline FLinearColor BoneC() { return Rgb(217, 207, 184) * 0.66f; }
	inline FLinearColor RootC() { return Rgb(92, 76, 58) * 0.5f; }
	inline FLinearColor YewC() { return Rgb(122, 74, 52) * 0.45f; }
	inline FLinearColor RopeC() { return Rgb(156, 132, 96) * 0.5f; }
	inline FLinearColor IronC() { return FLinearColor(0.03f, 0.028f, 0.027f); }
	inline FLinearColor BlackC() { return WRForestLook::BlackC(); }

	/** The ground (local Z) under a local XY from the land's own samples (no trace: thousands of stones). */
	inline float GZ(const FBuild& B, float X, float Y)
	{
		const FVector W = B.T.TransformPosition(FVector(X, Y, 0.f));
		float Z = 0.f;
		if (!WRLand::GroundUnder(W, Z)) return B.G(X, Y);
		return float(B.T.InverseTransformPosition(FVector(W.X, W.Y, Z)).Z);
	}
	inline FVector GAt(const FBuild& B, float X, float Y, float Lift = 0.f) { return FVector(X, Y, GZ(B, X, Y) + Lift); }

	/** A water surface (a procedural mesh with the pack's river or lake material). */
	inline UProceduralMeshComponent* WaterMesh(AActor* Act, bool bRiver)
	{
		if (Act->GetNetMode() == NM_DedicatedServer) return nullptr;
		UProceduralMeshComponent* P = NewObject<UProceduralMeshComponent>(Act);
		P->SetupAttachment(Act->GetRootComponent());
		P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		P->SetCanEverAffectNavigation(false);
		P->RegisterComponent();
		UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, *FWRTuning::Get().Str(bRiver ? TEXT("hazards.water.material") : TEXT("graveyardWorld.pondMaterial"), bRiver ? TEXT("/Game/WaterMaterials/Materials/M_River.M_River") : TEXT("/Game/WaterMaterials/Materials/M_Lake.M_Lake")));
		if (!M) M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/WaterMaterials/Materials/M_Lake.M_Lake"));
		if (M) P->SetMaterial(0, M);
		return P;
	}
	/** A ribbon of water through local points (per-point widths), U along the flow. */
	inline void WaterRibbon(AActor* Act, const TArray<FVector>& Pts, const TArray<float>& Widths)
	{
		UProceduralMeshComponent* P = WaterMesh(Act, true);
		if (!P || Pts.Num() < 2) return;
		TArray<FVector> V; TArray<int32> Tr; TArray<FVector> N; TArray<FVector2D> UV;
		float Along = 0.f;
		for (int32 i = 0; i < Pts.Num(); ++i)
		{
			const FVector D = (Pts[FMath::Min(i + 1, Pts.Num() - 1)] - Pts[FMath::Max(i - 1, 0)]).GetSafeNormal2D();
			const FVector Side(-D.Y, D.X, 0.f);
			if (i > 0) Along += float(FVector::Dist(Pts[i], Pts[i - 1]));
			V.Add(Pts[i] - Side * Widths[i] * 0.5f);
			V.Add(Pts[i] + Side * Widths[i] * 0.5f);
			N.Add(FVector::UpVector); N.Add(FVector::UpVector);
			UV.Add(FVector2D(Along / 400.f, 0.f)); UV.Add(FVector2D(Along / 400.f, 1.f));
			if (i > 0) { const int32 b = (i - 1) * 2; Tr.Append({ b, b + 2, b + 1, b + 1, b + 2, b + 3 }); }
		}
		P->CreateMeshSection(0, V, Tr, N, UV, {}, {}, false);
	}
	/** An elliptical water surface (the Lantern Pond, the cisterns, a flooded stair) at Centre's height. */
	inline void WaterPond(AActor* Act, const FVector& Centre, float Rx, float Ry, float Yaw = 0.f, bool bRect = false)
	{
		UProceduralMeshComponent* P = WaterMesh(Act, false);
		if (!P) return;
		TArray<FVector> V; TArray<int32> Tr; TArray<FVector> N; TArray<FVector2D> UV;
		const FRotator R(0.f, Yaw, 0.f);
		if (bRect)
		{
			for (int32 c = 0; c < 4; ++c) { V.Add(Centre + R.RotateVector(FVector(c % 2 ? Rx : -Rx, c < 2 ? -Ry : Ry, 0.f))); N.Add(FVector::UpVector); UV.Add(FVector2D(c % 2, c < 2 ? 0 : 1) * (Rx / 200.f)); }
			Tr.Append({ 0, 2, 1, 1, 2, 3 });
		}
		else
		{
			V.Add(Centre); N.Add(FVector::UpVector); UV.Add(FVector2D(0.5f, 0.5f));
			const int32 S = 40;
			for (int32 i = 0; i <= S; ++i)
			{
				const float A = 2.f * PI * i / S;
				V.Add(Centre + R.RotateVector(FVector(FMath::Cos(A) * Rx, FMath::Sin(A) * Ry, 0.f)));
				N.Add(FVector::UpVector);
				UV.Add(FVector2D(0.5f + 0.5f * FMath::Cos(A), 0.5f + 0.5f * FMath::Sin(A)) * (Rx / 400.f));
				if (i > 0) Tr.Append({ 0, i + 1, i });
			}
		}
		P->CreateMeshSection(0, V, Tr, N, UV, {}, {}, false);
	}


	/** A tōrō (kasuga-dōrō) of the Lantern Mourners: kiso (hex base), sao (pillar), chūdai (platform), hibukuro (the
	 *  fire box with a window each side), kasa (the roof with its four corners turned up), hōju (the jewel finial). Its
	 *  grave-light burns pale green at night (a night block in the fire box) and, with bLamp, a real light (lit by the
	 *  world's light step near the viewer). Scale 1 = 1.9 m. Tilt / Split for the old and the broken ones. */
	inline void Toro(FBuild& B, const FVector& Base, float Yaw, float Scale = 1.f, bool bLamp = false, float Tilt = 0.f, bool bUnlit = false, bool bGold = false)
	{
		const FRotator R(Tilt, Yaw, Tilt * 0.5f);
		const auto P = [&](float X, float Y, float Z) { return Base + R.RotateVector(FVector(X, Y, Z) * Scale); };
		const FSurf S = GreyStone(0.95f);
		const FSurf L = Lichen(0.9f);
		const float Sc = Scale;
		B.K.Hex(S, Base.X, Base.Y, Base.Z - 12.f * Sc, Base.Z + 16.f * Sc, 40.f * Sc, Yaw);                 // kiso
		B.K.Add(L, FVector(64.f, 64.f, 4.f) * Sc, P(0, 0, 17.f), R);
		B.K.Add(S, FVector(24.f, 24.f, 74.f) * Sc, P(0, 0, 54.f), R, EWRLookMesh::Cylinder);               // sao
		B.K.Add(S, FVector(30.f, 30.f, 6.f) * Sc, P(0, 0, 90.f), R, EWRLookMesh::Cylinder);
		B.K.Hex(S, P(0, 0, 0).X, P(0, 0, 0).Y, P(0, 0, 94.f).Z, P(0, 0, 108.f).Z, 30.f * Sc, Yaw + 30.f);    // chūdai
		// The hibukuro: four corner posts, a solid back and front frame, the window openings showing the light.
		for (int32 c = 0; c < 4; ++c) B.K.Add(S, FVector(9.f, 9.f, 40.f) * Sc, P(c % 2 ? 19.f : -19.f, c < 2 ? -19.f : 19.f, 128.f), R);
		B.K.Add(S, FVector(46.f, 46.f, 5.f) * Sc, P(0, 0, 110.f), R);
		B.K.Add(S, FVector(46.f, 46.f, 5.f) * Sc, P(0, 0, 146.f), R);
		const FSurf Box = bUnlit ? DarkStone(0.4f) : bGold ? FSurf::Lit(Rgb(255, 196, 96) * 0.6f, 6.f) : GraveLight(5.f);
		if (bUnlit) B.K.Add(Box, FVector(28.f, 28.f, 30.f) * Sc, P(0, 0, 128.f), R);
		else
		{
			// Day: a dim paper screen in the window; night: the light itself (the readable night: dotted green roads).
			B.K.Add(Paper(0.8f), FVector(30.f, 30.f, 30.f) * Sc, P(0, 0, 128.f), R);
			B.N.Add(Box, FVector(31.f, 31.f, 31.f) * Sc, P(0, 0, 128.f), R);
		}
		// The kasa: a broad slab, a second step, the four corners turned up (warabite).
		B.K.Add(S, FVector(92.f, 92.f, 12.f) * Sc, P(0, 0, 155.f), R);
		B.K.Add(S, FVector(66.f, 66.f, 14.f) * Sc, P(0, 0, 166.f), R);
		B.K.Add(L, FVector(70.f, 70.f, 3.f) * Sc, P(0, 0, 174.f), R);
		for (int32 c = 0; c < 4; ++c) B.K.Add(S, FVector(16.f, 16.f, 10.f) * Sc, P(c % 2 ? 44.f : -44.f, c < 2 ? -44.f : 44.f, 163.f), FRotator(R.Pitch + 10.f, R.Yaw + 45.f + 90.f * c, R.Roll));
		// The hōju: a jewel on a lotus collar.
		B.K.Add(S, FVector(20.f, 20.f, 6.f) * Sc, P(0, 0, 177.f), R, EWRLookMesh::Cylinder);
		B.K.Spike(S, P(0, 0, 179.f), R.RotateVector(FVector::UpVector), 16.f * Sc, 18.f * Sc);
		if (bLamp && !bUnlit) B.Lamp(P(0, 0, 128.f), bGold ? CandleC() : GraveLightC(), bGold ? 1600.f : 1100.f, 900.f * Sc);
	}

	/** A headstone at the head (west) end of a grave, facing east (+X of Yaw): Style 0 slab (round-shouldered), 1 cross in
	 *  a ring (a wheel-head), 2 a tōrō-topped stone (the Mourners' own), 3 a wooden marker gone grey. H the stone's
	 *  height (cm). Lean: degrees back (west) as the grave sinks; bFallen: lying face up on its grave. bKerb: the plot's
	 *  low stone kerb. bCollide: a box the size of the stone. Salt varies the details. */
	inline void Headstone(FBuild& B, const FVector& Base, float Yaw, int32 Style, float H, float Lean, bool bFallen, bool bKerb, bool bCollide, int32 Salt, bool bSmith = false)
	{
		const float W = Style == 3 ? 0.3f * H : FMath::Clamp(0.62f * H, 40.f, 76.f);
		const float T = Style == 3 ? 7.f : 9.f + 4.f * B.Rand(Salt);
		const FSurf S = bSmith ? Slag(0.9f + 0.2f * B.Rand(Salt + 4)) : B.Rand(Salt + 1) < 0.55f ? GreyStone(0.85f + 0.3f * B.Rand(Salt + 2)) : Limestone(0.85f + 0.25f * B.Rand(Salt + 3));
		// Lean back (toward -X: the head end sinks), or fallen face up across the grave's head.
		const FRotator R = bFallen ? FRotator(-88.f, Yaw, 4.f * (B.Rand(Salt + 5) - 0.5f)) : FRotator(-Lean, Yaw, 3.f * (B.Rand(Salt + 6) - 0.5f));
		const FVector Pivot = bFallen ? Base + FRotator(0.f, Yaw, 0.f).RotateVector(FVector(T * 0.5f, 0.f, 4.f)) : Base;
		const auto P = [&](float X, float Y, float Z) { return Pivot + R.RotateVector(FVector(X, Y, Z)); };
		if (bKerb)
		{
			// The kerbed plot: a 2.0 x 0.9 m stone frame east of the stone, gravel inside.
			const FRotator Rk(0.f, Yaw, 0.f);
			const auto Q = [&](float X, float Y, float Z) { return Base + Rk.RotateVector(FVector(X, Y, Z)); };
			B.K.Add(GreyStone(0.9f), FVector(200.f, 12.f, 14.f), Q(105.f, -44.f, 3.f), Rk);
			B.K.Add(GreyStone(0.9f), FVector(200.f, 12.f, 14.f), Q(105.f, 44.f, 3.f), Rk);
			B.K.Add(GreyStone(0.9f), FVector(12.f, 100.f, 14.f), Q(204.f, 0.f, 3.f), Rk);
			B.K.Add(Chalk(0.8f), FVector(188.f, 76.f, 6.f), Q(105.f, 0.f, 1.f), Rk);
		}
		switch (Style)
		{
		case 0:
		{
			// A slab sunk 20 cm, its shoulders rounded (a half-cylinder cap), lichen on its north edge.
			B.K.Add(S, FVector(T, W, H * 0.82f), P(0.f, 0.f, H * 0.41f - 20.f), R);
			// The rounded head: a disc through the stone's thickness (its axis along the stone's facing).
			B.K.Add(S, FVector(W, W, T), P(0.f, 0.f, H * 0.82f - 20.f), (R.Quaternion() * FRotator(90.f, 0.f, 0.f).Quaternion()).Rotator(), EWRLookMesh::Cylinder);
			B.K.Add(Lichen(0.9f), FVector(T + 1.f, W * 0.3f, H * 0.3f), P(0.f, -W * 0.36f, H * 0.62f - 20.f), R);
			// The carved panel: a darker inset where a name was.
			B.K.Add(DarkStone(0.7f), FVector(2.f, W * 0.62f, H * 0.3f), P(T * 0.5f, 0.f, H * 0.5f - 20.f), R);
			break;
		}
		case 1:
		{
			// A wheel-head cross: shaft, arms, the ring joining them, a stepped base.
			B.K.Add(S, FVector(T + 14.f, W + 14.f, 24.f), P(0.f, 0.f, -8.f), R);
			B.K.Add(S, FVector(T, W * 0.32f, H), P(0.f, 0.f, H * 0.5f), R);
			B.K.Add(S, FVector(T, W * 1.05f, W * 0.3f), P(0.f, 0.f, H * 0.72f), R);
			// The ring: eight stones round the crossing in the stone's face plane, each laid along the ring's tangent.
			const FVector Fw = R.RotateVector(FVector::ForwardVector), Rt = R.RotateVector(FVector::RightVector), Up = R.RotateVector(FVector::UpVector);
			for (int32 k = 0; k < 8; ++k)
			{
				const float A = FMath::DegreesToRadians(22.5f + 45.f * k);
				const float Rad = W * 0.36f;
				const FVector Tan = -FMath::Sin(A) * Rt + FMath::Cos(A) * Up;
				B.K.Add(S, FVector(Rad * 0.82f, 7.f, T * 0.9f), P(0.f, FMath::Cos(A) * Rad, H * 0.72f + FMath::Sin(A) * Rad), FRotationMatrix::MakeFromXZ(Tan, Fw).Rotator());
			}
			B.K.Add(Lichen(0.85f), FVector(T + 1.f, W * 0.2f, H * 0.4f), P(0.f, -W * 0.1f, H * 0.3f), R);
			break;
		}
		case 2:
		{
			// The Mourners' own: a short square pillar carrying a small lantern house and a roof cap.
			B.K.Add(S, FVector(T + 18.f, W * 0.7f, H * 0.6f), P(0.f, 0.f, H * 0.3f - 15.f), R);
			B.K.Add(S, FVector(T + 26.f, W * 0.8f, 8.f), P(0.f, 0.f, H * 0.6f - 11.f), R);
			B.K.Add(Paper(0.7f), FVector(T + 6.f, W * 0.45f, H * 0.22f), P(0.f, 0.f, H * 0.6f + H * 0.11f - 7.f), R);
			B.N.Add(GraveLight(3.5f), FVector(T + 8.f, W * 0.47f, H * 0.23f), P(0.f, 0.f, H * 0.6f + H * 0.11f - 7.f), R);
			for (int32 c = 0; c < 4; ++c) B.K.Add(S, FVector(5.f, 5.f, H * 0.22f), P((c % 2 ? 0.5f : -0.5f) * (T + 4.f), (c < 2 ? -0.5f : 0.5f) * W * 0.45f, H * 0.6f + H * 0.11f - 7.f), R);
			B.K.Add(S, FVector(T + 34.f, W * 0.95f, 9.f), P(0.f, 0.f, H * 0.83f - 5.f), R);
			B.K.Spike(S, P(0.f, 0.f, H * 0.83f), R.RotateVector(FVector::UpVector), 14.f, 14.f);
			break;
		}
		default:
		{
			// A wooden marker: a weathered grey post with a cross-board, split along its grain.
			B.K.Add(OldWood(0.9f), FVector(8.f, 10.f, H), P(0.f, 0.f, H * 0.5f - 15.f), R);
			B.K.Add(OldWood(0.85f), FVector(6.f, W * 1.6f, 9.f), P(1.f, 0.f, H * 0.7f), R);
			break;
		}
		}
		if (bCollide && !bFallen)
		{
			const FRotator Rb(0.f, Yaw, 0.f);
			B.Box(Base + Rb.RotateVector(FVector(0.f, 0.f, H * 0.4f)), FVector(T * 0.5f + 4.f, W * 0.5f, H * 0.42f), Rb);
		}
	}

	/** An iron mortsafe over a grave: a cage 1.9 x 0.8 x 1.1 m of bars at 10 cm on a frame, crossed straps over its lid,
	 *  rusted at the feet. bPrised: one bar bent out and the lid strap broken (someone got in anyway). Decoration only
	 *  (the mortsafes you can break for grave iron are resource nodes: WRResourceLook draws theirs the same way). */
	inline void Mortsafe(FBuild& B, const FVector& Base, float Yaw, bool bPrised, int32 Salt)
	{
		const FRotator R(0.f, Yaw, 0.f);
		const auto P = [&](float X, float Y, float Z) { return Base + R.RotateVector(FVector(X, Y, Z)); };
		const FSurf I = B.Rand(Salt) < 0.5f ? Iron() : Rust(1.1f);
		const float Lx = 190.f, Ly = 80.f, Hz = 110.f;
		for (const float Z : { 4.f, Hz })
		{
			B.K.Add(I, FVector(Lx, 5.f, 5.f), P(0.f, -Ly * 0.5f, Z), R);
			B.K.Add(I, FVector(Lx, 5.f, 5.f), P(0.f, Ly * 0.5f, Z), R);
			B.K.Add(I, FVector(5.f, Ly, 5.f), P(-Lx * 0.5f, 0.f, Z), R);
			B.K.Add(I, FVector(5.f, Ly, 5.f), P(Lx * 0.5f, 0.f, Z), R);
		}
		for (float X = -Lx * 0.5f; X <= Lx * 0.5f + 1.f; X += 10.f)
		{
			for (const float Y : { -Ly * 0.5f, Ly * 0.5f })
			{
				const bool bBent = bPrised && Y > 0.f && FMath::Abs(X - 20.f) < 6.f;
				B.K.Rod(I, P(X, Y, 4.f), P(X, Y + (bBent ? 26.f : 0.f), bBent ? Hz * 0.6f : Hz), 2.5f);
			}
		}
		for (float Y = -Ly * 0.5f + 10.f; Y < Ly * 0.5f; Y += 10.f) for (const float X : { -Lx * 0.5f, Lx * 0.5f }) B.K.Rod(I, P(X, Y, 4.f), P(X, Y, Hz), 2.5f);
		// The lid: two straps crossed and a padlock hasp; broken on a prised one.
		B.K.Add(I, FVector(Lx * 1.02f, 7.f, 3.f), P(0.f, 0.f, Hz + 3.f), FRotator(0.f, Yaw + 23.f, 0.f));
		if (!bPrised) B.K.Add(I, FVector(Lx * 1.02f, 7.f, 3.f), P(0.f, 0.f, Hz + 3.f), FRotator(0.f, Yaw - 23.f, 0.f));
		B.K.Add(Rust(), FVector(8.f, 6.f, 10.f), P(Lx * 0.5f + 4.f, 0.f, Hz - 6.f), R);
		B.K.Add(Soil(0.9f), FVector(Lx - 10.f, Ly - 10.f, 8.f), P(0.f, 0.f, 0.f), R);
		B.Box(P(0.f, 0.f, Hz * 0.5f), FVector(Lx * 0.5f, Ly * 0.5f, Hz * 0.5f + 4.f), R);
	}

	/** A coffin (a tapered six-sided box, the old shape): lid on or off, a lantern lying in it (the cenotaphs). */
	inline void Coffin(FBuild& B, const FVector& Centre, const FRotator& Rot, bool bOpen, bool bLantern, const FSurf& Wood)
	{
		const auto P = [&](float X, float Y, float Z) { return Centre + Rot.RotateVector(FVector(X, Y, Z)); };
		// Sides: the shoulders widest at 30% from the head.
		B.K.Add(Wood, FVector(140.f, 4.f, 42.f), P(-20.f, -27.f, 0.f), Rot + FRotator(0.f, -3.f, 0.f));
		B.K.Add(Wood, FVector(140.f, 4.f, 42.f), P(-20.f, 27.f, 0.f), Rot + FRotator(0.f, 3.f, 0.f));
		B.K.Add(Wood, FVector(62.f, 4.f, 42.f), P(78.f, -21.f, 0.f), Rot + FRotator(0.f, 20.f, 0.f));
		B.K.Add(Wood, FVector(62.f, 4.f, 42.f), P(78.f, 21.f, 0.f), Rot + FRotator(0.f, -20.f, 0.f));
		B.K.Add(Wood, FVector(4.f, 40.f, 42.f), P(-90.f, 0.f, 0.f), Rot);
		B.K.Add(Wood, FVector(4.f, 30.f, 42.f), P(108.f, 0.f, 0.f), Rot);
		B.K.Add(Wood, FVector(196.f, 50.f, 3.f), P(8.f, 0.f, -20.f), Rot);
		if (!bOpen) B.K.Add(Wood, FVector(200.f, 56.f, 4.f), P(8.f, 0.f, 23.f), Rot);
		else B.K.Add(Wood, FVector(200.f, 56.f, 4.f), P(8.f, 52.f, -14.f), Rot + FRotator(0.f, 6.f, 70.f));
		if (bLantern)
		{
			// The Mourners' answer to every grave: a paper lantern with a name tag tied to its handle.
			B.K.Add(Paper(0.9f), FVector(22.f, 22.f, 26.f), P(20.f, 0.f, -4.f), Rot, EWRLookMesh::Cylinder);
			B.K.Add(Black(), FVector(24.f, 24.f, 3.f), P(20.f, 0.f, 10.f), Rot, EWRLookMesh::Cylinder);
			B.K.Add(Paper(1.1f), FVector(7.f, 1.f, 4.f), P(32.f, 4.f, 6.f), Rot);
		}
	}

	/** A human skull (code mesh): the cranium, the cheekbones, the dark sockets, the jaw. Faces +X of Yaw. */
	inline void Skull(FBuild& B, const FVector& C, float S, float Yaw, const FLinearColor& Col)
	{
		const FRotator R(0.f, Yaw, 0.f);
		B.Ball(C, 8.f * S, Col, 0.75f, 0.92f);
		B.Ball(C + R.RotateVector(FVector(6.f, 0.f, -5.f) * S), 5.2f * S, Col, 0.75f, 0.8f);
		for (const float Y : { -2.6f, 2.6f }) B.Ball(C + R.RotateVector(FVector(9.6f, Y, -2.f) * S), 1.9f * S, BlackC(), 0.95f);
		B.Ball(C + R.RotateVector(FVector(10.f, 0.f, -5.5f) * S), 1.1f * S, BlackC(), 0.95f);
		B.Limb(C + R.RotateVector(FVector(3.f, -4.f, -9.f) * S), C + R.RotateVector(FVector(9.f, 0.f, -11.f) * S), 1.3f * S, 1.1f * S, Col, 5);
		B.Limb(C + R.RotateVector(FVector(3.f, 4.f, -9.f) * S), C + R.RotateVector(FVector(9.f, 0.f, -11.f) * S), 1.3f * S, 1.1f * S, Col, 5);
	}
	/** A long bone (femur-like): a shaft with knobbed ends. */
	inline void LongBone(FBuild& B, const FVector& A, const FVector& E, float R, const FLinearColor& Col)
	{
		B.Limb(A, E, R, R * 0.85f, Col, 6);
		B.Ball(A, R * 1.7f, Col, 0.75f, 0.8f);
		B.Ball(E, R * 1.5f, Col, 0.75f, 0.8f);
	}

	/** A heap of bones (long bones and skulls) in a rough cone of radius Rad: what the pits hold, what the Risen fall to. */
	inline void BonePile(FBuild& B, const FVector& Centre, float Rad, int32 Salt, int32 Skulls = 3)
	{
		const FLinearColor Col = BoneC();
		const int32 N = FMath::Clamp(int32(Rad / 6.f), 6, 40);
		for (int32 i = 0; i < N; ++i)
		{
			const float A = B.Rand(Salt + i * 3) * 2.f * PI, D = FMath::Sqrt(B.Rand(Salt + i * 3 + 1)) * Rad;
			const float Hh = (1.f - D / Rad) * Rad * 0.45f;
			const FVector Mid = Centre + FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, Hh);
			const FVector Dir = FRotator(B.Rand(Salt + i * 7) * 50.f - 25.f, B.Rand(Salt + i * 5) * 360.f, 0.f).Vector();
			LongBone(B, Mid - Dir * 22.f, Mid + Dir * 22.f, 2.2f, Col);
		}
		for (int32 s = 0; s < Skulls; ++s)
		{
			const float A = B.Rand(Salt + 90 + s) * 2.f * PI, D = B.Rand(Salt + 95 + s) * Rad * 0.6f;
			Skull(B, Centre + FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, (1.f - D / Rad) * Rad * 0.45f + 8.f), 1.f, B.Rand(Salt + 99 + s) * 360.f, Col);
		}
	}

	/** A dry-stone field wall from A to B (local ground points): two faces of coursed rubble on the ground, through-stones,
	 *  a cope of upright stones on top; 0.9 m tall, 0.6 m wide at the foot. Collision along it. */
	inline void DryWall(FBuild& B, const FVector2D& A, const FVector2D& E, float Height = 90.f, int32 Salt = 0)
	{
		const FVector2D D = E - A;
		const float Len = D.Size();
		if (Len < 50.f) return;
		const FVector2D U = D / Len;
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(U.Y, U.X));
		const FRotator R(0.f, Yaw, 0.f);
		const int32 N = FMath::Max(1, FMath::CeilToInt(Len / 110.f));
		for (int32 i = 0; i < N; ++i)
		{
			const float T0 = float(i) / N, T1 = float(i + 1) / N;
			const FVector2D Pa = A + D * T0, Pb = A + D * T1, Pm = (Pa + Pb) * 0.5f;
			const float Z = B.G(Pm.X, Pm.Y);
			const float Seg = Len / N;
			// Courses: three rows of stones of varied length, each a shade apart.
			for (int32 c = 0; c < 3; ++c)
			{
				float X = -Seg * 0.5f;
				int32 k = 0;
				while (X < Seg * 0.5f - 5.f)
				{
					const float L = FMath::Min(Seg * 0.5f - X, 26.f + 30.f * B.Rand(Salt + i * 31 + c * 7 + k));
					const float Wd = 58.f - 10.f * c;
					const FVector Cn = FVector(Pm.X, Pm.Y, Z) + R.RotateVector(FVector(X + L * 0.5f, 0.f, 12.f + 24.f * c));
					B.K.Add(GreyStone(0.75f + 0.35f * B.Rand(Salt + i * 13 + c * 5 + k)), FVector(L - 2.f, Wd, 22.f), Cn, R);
					X += L;
					++k;
				}
			}
			// The cope: upright stones edge-on along the top.
			for (float X = -Seg * 0.5f + 6.f; X < Seg * 0.5f; X += 13.f)
			{
				const FVector Cn = FVector(Pm.X, Pm.Y, Z) + R.RotateVector(FVector(X, 0.f, Height - 8.f));
				B.K.Add(GreyStone(0.7f + 0.3f * B.Rand(Salt + int32(X) + i)), FVector(9.f, 34.f, 24.f), Cn, R + FRotator(0.f, 0.f, 6.f * (B.Rand(Salt + int32(X)) - 0.5f)));
			}
			if (B.Rand(Salt + i) < 0.35f) B.K.Add(Lichen(0.9f), FVector(Seg * 0.6f, 60.f, 3.f), FVector(Pm.X, Pm.Y, Z + Height - 22.f), R);
			B.Box(FVector(Pm.X, Pm.Y, Z + Height * 0.5f), FVector(Seg * 0.5f, 30.f, Height * 0.5f), R);
		}
	}

	/** Iron railings round a family plot: spear-headed bars at 12 cm on two rails, on a stone kerb. */
	inline void Railing(FBuild& B, const FVector2D& A, const FVector2D& E)
	{
		const FVector2D D = E - A;
		const float Len = D.Size();
		if (Len < 20.f) return;
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X));
		const FRotator R(0.f, Yaw, 0.f);
		const FVector2D Mid = (A + E) * 0.5f;
		const float Z = B.G(Mid.X, Mid.Y);
		B.K.Add(GreyStone(0.9f), FVector(Len, 24.f, 20.f), FVector(Mid.X, Mid.Y, Z + 4.f), R);
		B.K.Add(Iron(), FVector(Len, 3.f, 3.f), FVector(Mid.X, Mid.Y, Z + 30.f), R);
		B.K.Add(Iron(), FVector(Len, 3.f, 3.f), FVector(Mid.X, Mid.Y, Z + 96.f), R);
		for (float X = -Len * 0.5f + 6.f; X < Len * 0.5f; X += 12.f)
		{
			const FVector P = FVector(Mid.X, Mid.Y, Z) + R.RotateVector(FVector(X, 0.f, 0.f));
			B.K.Rod(Iron(), P + FVector(0, 0, 14.f), P + FVector(0, 0, 108.f), 2.f);
			B.K.Spike(Iron(), P + FVector(0, 0, 108.f), FVector::UpVector, 4.f, 9.f);
		}
		B.Box(FVector(Mid.X, Mid.Y, Z + 60.f), FVector(Len * 0.5f, 8.f, 60.f), R);
	}

	/** A pointed (Gothic) arch in the plane at X: two arcs meeting at the apex, voussoirs of Depth, the opening Span wide
	 *  and springing at Spring; the arch face across Y. */
	inline void PointedArch(FBuild& B, const FVector& Centre, float Yaw, float Span, float Spring, float Depth, float Thick, const FSurf& S, int32 Stones = 7)
	{
		// An equilateral pointed arch: each side an arc of radius Span centred on the far springing point, 60 degrees
		// from its springing to the apex. Voussoirs laid along the arc's tangent, their faces in the wall's plane.
		const FRotator R(0.f, Yaw, 0.f);
		const FVector Fw = R.RotateVector(FVector::ForwardVector), Rt = R.RotateVector(FVector::RightVector), Up = R.RotateVector(FVector::UpVector);
		const float Rad = Span;
		for (const float Side : { -1.f, 1.f })
		{
			const float CtrY = -Side * Span * 0.5f;
			for (int32 i = 0; i < Stones; ++i)
			{
				const float A0 = FMath::DegreesToRadians(60.f * i / Stones), A1 = FMath::DegreesToRadians(60.f * (i + 1) / Stones);
				const float Am = (A0 + A1) * 0.5f;
				const float Rm = Rad + Depth * 0.5f;
				const float Y = CtrY + Side * FMath::Cos(Am) * Rm, Z = Spring + FMath::Sin(Am) * Rm;
				const FVector Tan = (-Side * FMath::Sin(Am)) * Rt + FMath::Cos(Am) * Up;
				const float Chord = 2.f * Rm * FMath::Sin((A1 - A0) * 0.5f) - 1.5f;
				B.K.Add(S, FVector(Chord, Depth, Thick), Centre + Rt * Y + Up * Z, FRotationMatrix::MakeFromXZ(Tan, Fw).Rotator());
			}
		}
	}

	/** A paper lantern on a pole (the Mourners' procession lanterns): pole, crook, lantern; lit green at night. */
	inline void PoleLantern(FBuild& B, const FVector& Foot, const FVector& Top, bool bLit, bool bLamp = false)
	{
		B.K.Rod(OldWood(0.8f), Foot, Top, 5.f);
		const FVector Dir = (Top - Foot).GetSafeNormal();
		const FVector Hook = Top + FVector(Dir.X, Dir.Y, 0.f).GetSafeNormal() * 30.f;
		B.K.Rod(OldWood(0.8f), Top, Hook, 3.f);
		const FVector L = Hook - FVector(0, 0, 28.f);
		B.Line(Hook, L + FVector(0, 0, 14.f), 0.6f, 0.f, RopeC(), 2);
		B.K.Add(Paper(0.9f), FVector(24.f, 24.f, 30.f), L, FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		B.K.Add(Black(), FVector(26.f, 26.f, 3.f), L + FVector(0, 0, 16.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		if (bLit) B.N.Add(GraveLight(4.f), FVector(25.f, 25.f, 30.f), L, FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		if (bLit && bLamp) B.Lamp(L, GraveLightC(), 700.f, 700.f);
	}

	/** A family mausoleum: a stone cell on a plinth with a pedimented front, pilasters, a door (bronze gone green, or iron
	 *  grille, or broken in), a stone roof; Var 0 small, 1 bronze-door, 2 collapsed (roof down, a Gravehound den), 3 with
	 *  a Wickbeetle nest (wax crusting the door). Front faces +X. W x D x H cm. */
	inline void Mausoleum(FBuild& B, const FVector& Base, float Yaw, float W, float D, float H, int32 Var, int32 Salt)
	{
		const FRotator R(0.f, Yaw, 0.f);
		const auto P = [&](float X, float Y, float Z) { return Base + R.RotateVector(FVector(X, Y, Z)); };
		const FSurf S = B.Rand(Salt) < 0.5f ? GreyStone(0.9f) : Limestone(0.85f);
		const FSurf Sd = DarkStone(0.8f);
		B.K.Add(S, FVector(D + 60.f, W + 60.f, 30.f), P(0.f, 0.f, 0.f), R);
		B.K.Add(S, FVector(30.f, W * 0.6f, 15.f), P(D * 0.5f + 40.f, 0.f, -2.f), R);   // the step
		const bool bCollapsed = Var == 2;
		// Walls in courses (rusticated base course), the back and two sides; the front with its door.
		for (int32 c = 0; c < 6; ++c)
		{
			const float Z = 15.f + c * (H / 6.f) + H / 12.f;
			if (bCollapsed && c >= 4) break;
			const float K = 0.85f + 0.2f * B.Rand(Salt + c);
			B.K.Add(GreyStone(K), FVector(D, 22.f, H / 6.f - 2.f), P(0.f, -W * 0.5f + 11.f, Z), R);
			B.K.Add(GreyStone(K), FVector(D, 22.f, H / 6.f - 2.f), P(0.f, W * 0.5f - 11.f, Z), R);
			B.K.Add(GreyStone(K), FVector(22.f, W - 44.f, H / 6.f - 2.f), P(-D * 0.5f + 11.f, 0.f, Z), R);
		}
		// The front: pilasters either side of the door, a lintel, the pediment.
		for (const float Y : { -W * 0.5f + 18.f, W * 0.5f - 18.f }) B.K.Add(S, FVector(30.f, 36.f, H), P(D * 0.5f + 4.f, Y, 15.f + H * 0.5f), R);
		B.K.Add(S, FVector(34.f, W - 64.f, H * 0.28f), P(D * 0.5f + 2.f, 0.f, 15.f + H * 0.86f), R);
		const float DoorW = FMath::Min(110.f, W * 0.42f), DoorH = H * 0.7f;
		if (Var == 1)
		{
			// Bronze doors gone verdigris, two leaves with panels and ring handles.
			for (const float Y : { -DoorW * 0.25f, DoorW * 0.25f })
			{
				B.K.Add(Verdigris(), FVector(8.f, DoorW * 0.5f - 2.f, DoorH), P(D * 0.5f, Y, 15.f + DoorH * 0.5f), R);
				B.K.Add(Bronze(0.9f), FVector(9.f, DoorW * 0.36f, DoorH * 0.3f), P(D * 0.5f + 1.f, Y, 15.f + DoorH * 0.7f), R);
				B.K.Disc(Bronze(1.1f), P(D * 0.5f + 6.f, Y * 0.3f, 15.f + DoorH * 0.48f), R.Vector(), 12.f, 2.f);
			}
		}
		else if (Var == 0 || Var == 3)
		{
			// An iron grille door: the dark tomb behind it.
			B.K.Add(Sd, FVector(6.f, DoorW, DoorH), P(D * 0.5f - 10.f, 0.f, 15.f + DoorH * 0.5f), R);
			for (float Y = -DoorW * 0.5f + 8.f; Y < DoorW * 0.5f; Y += 12.f) B.K.Rod(Iron(), P(D * 0.5f, Y, 15.f), P(D * 0.5f, Y, 15.f + DoorH), 2.5f);
			for (const float Z : { 0.3f, 0.7f }) B.K.Add(Iron(), FVector(3.f, DoorW, 4.f), P(D * 0.5f, 0.f, 15.f + DoorH * Z), R);
			if (Var == 3)
			{
				// Wax: a Wickbeetle nest crusting the grille and spilling down the step in drips.
				for (int32 k = 0; k < 9; ++k) B.Ball(P(D * 0.5f + 6.f, (B.Rand(Salt + k) - 0.5f) * DoorW, 15.f + B.Rand(Salt + k + 20) * DoorH), 9.f + 7.f * B.Rand(Salt + k + 40), Rgb(232, 221, 176) * 0.62f, 0.5f);
				B.N.Add(Candle(2.f), FVector(10.f, DoorW * 0.5f, 20.f), P(D * 0.5f - 20.f, 0.f, 30.f), R);
			}
		}
		else
		{
			// Collapsed: the door gone, a dark hole, the roof slabs fallen in, soil pushed out of the doorway.
			B.K.Add(Black(0.5f), FVector(6.f, DoorW, DoorH * 0.8f), P(D * 0.5f - 12.f, 0.f, 15.f + DoorH * 0.4f), R);
			B.K.Add(Soil(), FVector(90.f, DoorW + 20.f, 24.f), P(D * 0.5f + 50.f, 0.f, 4.f), R + FRotator(-10.f, 0.f, 0.f));
			for (int32 k = 0; k < 3; ++k) B.K.Add(S, FVector(D * 0.5f, W * 0.45f, 18.f), P(-D * 0.2f + k * 40.f, (k - 1) * W * 0.25f, 30.f + k * 20.f), R + FRotator(25.f - k * 15.f, 10.f * k, 15.f));
		}
		if (!bCollapsed)
		{
			// The roof: a stone slab with a low pitched cap and a cross or an urn finial.
			B.K.Add(S, FVector(D + 40.f, W + 40.f, 22.f), P(0.f, 0.f, 15.f + H + 11.f), R);
			for (const float Sd2 : { -1.f, 1.f }) B.K.Add(S, FVector(D + 36.f, W * 0.56f, 14.f), P(0.f, Sd2 * W * 0.24f, 15.f + H + 34.f), R + FRotator(0.f, 0.f, Sd2 * 14.f));
			B.K.Add(S, FVector(20.f, W * 0.7f, 30.f), P(D * 0.5f + 10.f, 0.f, 15.f + H + 30.f), R);
			if (Var == 1) B.K.Add(Bronze(0.9f), FVector(18.f, 18.f, 40.f), P(0.f, 0.f, 15.f + H + 64.f), R, EWRLookMesh::Cylinder);
			else { B.K.Add(S, FVector(10.f, 10.f, 54.f), P(D * 0.5f + 10.f, 0.f, 15.f + H + 70.f), R); B.K.Add(S, FVector(10.f, 34.f, 10.f), P(D * 0.5f + 10.f, 0.f, 15.f + H + 82.f), R); }
		}
		B.K.Add(Moss(0.9f), FVector(D * 0.6f, W * 0.4f, 4.f), P(-D * 0.1f, -W * 0.2f, 15.f + (bCollapsed ? H * 0.6f : H + 26.f)), R);
		B.Box(P(0.f, 0.f, (H + 40.f) * 0.5f), FVector(D * 0.5f + 20.f, W * 0.5f + 10.f, (H + 40.f) * 0.5f), R);
	}

	/** A castaway wreck on a shingle beach: the broken-backed hull of a newcomers' ship lying on its side, ribs showing
	 *  through planking torn away, a snapped mast, rope. Along +X (bow), Len cm. */
	inline void Wreck(FBuild& B, const FVector& Base, float Yaw, float Len, int32 Salt)
	{
		const FRotator R(0.f, Yaw, 18.f);
		const auto P = [&](float X, float Y, float Z) { return Base + R.RotateVector(FVector(X, Y, Z)); };
		const FSurf W = OldWood(0.75f);
		// The keel and stem, then ribs (frames) in pairs, planking on one side, torn on the other.
		B.K.Add(Timber(0.8f), FVector(Len, 26.f, 30.f), P(0.f, 0.f, 15.f), R);
		B.K.Rod(Timber(0.8f), P(Len * 0.5f, 0.f, 15.f), P(Len * 0.5f + 120.f, 0.f, 330.f), 24.f, true);
		const int32 N = FMath::Max(6, int32(Len / 70.f));
		for (int32 i = 0; i < N; ++i)
		{
			const float X = -Len * 0.5f + (i + 0.5f) * Len / N;
			const float Beam = 260.f * FMath::Sin(PI * (i + 0.5f) / N) + 60.f;
			for (const float S : { -1.f, 1.f })
			{
				TArray<FVector> Rib;
				for (int32 k = 0; k <= 4; ++k) { const float A = k / 4.f; Rib.Add(P(X, S * Beam * FMath::Sin(A * PI * 0.5f), 30.f + 240.f * A - 40.f * A * A)); }
				B.Root(Rib, 9.f, 7.f, Rgb(110, 102, 92) * 0.45f, 6);
			}
			if (B.Rand(Salt + i) < 0.75f)
			{
				for (int32 s = 0; s < 4; ++s) B.K.Add(W, FVector(Len / N + 6.f, 4.f, 34.f), P(X, -Beam * FMath::Sin((s + 0.5f) / 4.f * PI * 0.5f) - 8.f, 50.f + s * 58.f), R + FRotator(0.f, 0.f, -15.f * s));
			}
		}
		B.K.Rod(Timber(0.7f), P(-Len * 0.1f, 0.f, 40.f), P(-Len * 0.1f, 140.f, 520.f), 22.f);
		B.Line(P(-Len * 0.1f, 140.f, 500.f), P(Len * 0.4f, -60.f, 20.f), 2.f, 60.f, RopeC(), 8);
		B.Box(P(0.f, 0.f, 120.f), FVector(Len * 0.5f, 170.f, 120.f), R);
	}
}
