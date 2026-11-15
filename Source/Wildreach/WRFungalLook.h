#pragma once

// The Fungal caverns' scenery kit (shared by WRFungalScenery*.cpp, the hazards and the bosses' props): the palette
// (fungal 2.2, exact hexes: limestone #A7A498, salt-grass #7E8C5A, sea slate #4E5F66, daylight gold #E8C878, cap ivory
// #E6DCC6, moss #5E7A3A, gill pink #C88A8A, glow-moss cyan #39E6C2, flowstone #D8CBB0, cave black #121416, starthread
// #F2F6F0, rhizomorph black #1A1714, mycelium white #E8E4DA, Heart amber #E8A040, silk grey #B8B8B0, water teal #1E8C86,
// rimstone #E0D4BC, toad pink #E8C8C0) and the pieces every place is built from, on the forest's FBuild
// (WRForestSceneryKit.h) with two more code meshes: a glowing one (M_FungalGlow: vertex colour the light's colour,
// vertex alpha how much glows) and a see-through one (M_FungalVeil), both on the fungal world's shared instances so the
// circadian curve and the bloom's beat reach every glowing thing at once.
//
// Real measures (fungal 1.2): a parasol mushroom's cap is a flat disc with a raised umbo and a ring on the stem; an ink
// cap is a tall bell; an oyster grows as a fan-shaped shelf straight out of wood or rock; a honey cap is convex, in
// clusters from one root; gills radiate from the stem to the rim, about 1.5 per cm of rim; rhizomorphs are black
// bootlaces with a fibrous twist; a cordyceps stalk is a thin white club from the back of the skull, its tip swollen.
// Every free function is inline and lives in WRFungalLook (unity builds join the files).

#include "WRForestSceneryKit.h"
#include "WRFungalTypes.h"
#include "WRLand.h"
#include "WRTuning.h"

class UMaterialInterface;

namespace WRFungalLook
{
	using WRForestLook::FBuild;
	using FSurf = WRLookKit::FSurf;
	using WRForestLook::Rgb;

	// ---- the palette (linear, dimmed as the look kit's tinted blocks want it) ----
	inline FSurf Limestone(float K = 1.f) { return FSurf::Stone(Rgb(167, 164, 152) * 0.6f * K, 0.88f); }
	inline FSurf Flowstone(float K = 1.f) { return FSurf::Glaze(Rgb(216, 203, 176) * 0.6f * K, 0.45f); }
	inline FSurf Rimstone(float K = 1.f) { return FSurf::Glaze(Rgb(224, 212, 188) * 0.6f * K, 0.4f); }
	inline FSurf Carved(float K = 1.f) { return FSurf::Stone(Rgb(150, 146, 134) * 0.6f * K, 0.8f); }
	inline FSurf Loam(float K = 1.f) { return FSurf::Cloth(Rgb(70, 56, 40) * 0.5f * K); }
	inline FSurf Moss(float K = 1.f) { return FSurf::Cloth(Rgb(94, 122, 58) * 0.48f * K); }
	inline FSurf Fern(float K = 1.f) { return FSurf::Cloth(Rgb(84, 118, 52) * 0.5f * K); }
	inline FSurf SaltGrass(float K = 1.f) { return FSurf::Cloth(Rgb(126, 140, 90) * 0.48f * K); }
	inline FSurf Timber(float K = 1.f) { return FSurf::Wood(Rgb(96, 78, 58) * 0.45f * K); }
	inline FSurf OldWood(float K = 1.f) { return FSurf::Wood(Rgb(120, 108, 92) * 0.45f * K); }
	inline FSurf Drift(float K = 1.f) { return FSurf::Wood(Rgb(150, 142, 128) * 0.45f * K); }
	inline FSurf Tarred(float K = 1.f) { return FSurf::Wood(Rgb(36, 30, 26) * 0.6f * K); }
	inline FSurf Bamboo(float K = 1.f) { return FSurf::Wood(Rgb(196, 170, 112) * 0.5f * K); }
	inline FSurf Clay(float K = 1.f) { return FSurf::Stone(Rgb(168, 104, 66) * 0.55f * K, 0.9f); }
	inline FSurf Bronze(float K = 1.f) { return FSurf::Steel(Rgb(140, 106, 58) * 0.6f * K, 0.45f); }
	inline FSurf Verdigris(float K = 1.f) { return FSurf::Steel(Rgb(84, 140, 122) * 0.5f * K, 0.62f); }
	inline FSurf Rope(float K = 1.f) { return FSurf::Cloth(Rgb(156, 132, 96) * 0.5f * K); }
	inline FSurf Linen(float K = 1.f) { return FSurf::Cloth(Rgb(206, 196, 170) * 0.5f * K); }
	inline FSurf Sailcloth(float K = 1.f) { return FSurf::Cloth(Rgb(196, 186, 160) * 0.48f * K); }
	inline FSurf PrayerCloth(int32 i) { static const FColor C[] = { FColor(40, 90, 170), FColor(230, 230, 220), FColor(190, 40, 30), FColor(40, 130, 60), FColor(230, 190, 40) }; return FSurf::Cloth(FLinearColor(C[((i % 5) + 5) % 5]) * 0.5f); }
	inline FSurf Bone(float K = 1.f) { return FSurf::Stone(Rgb(217, 207, 184) * 0.66f * K, 0.78f); }
	inline FSurf Furred(float K = 1.f) { return FSurf::Cloth(Rgb(232, 228, 218) * 0.6f * K); }
	inline FSurf Rhizo(float K = 1.f) { return FSurf::Glaze(Rgb(26, 23, 20) * 0.7f * K, 0.3f); }
	inline FSurf Mycel(float K = 1.f) { return FSurf::Cloth(Rgb(232, 228, 218) * 0.62f * K); }
	inline FSurf Silk(float K = 1.f) { return FSurf::Cloth(Rgb(184, 184, 176) * 0.6f * K); }
	inline FSurf Sand(float K = 1.f) { return FSurf::Stone(Rgb(216, 192, 138) * 0.6f * K, 0.95f); }
	inline FSurf Water() { return FSurf::Glaze(Rgb(30, 140, 134) * 0.4f, 0.04f); }
	inline FSurf Sap() { return FSurf::Glaze(Rgb(232, 160, 64) * 0.5f, 0.1f); }
	inline FSurf Juniper(float K = 1.f) { return FSurf::Cloth(Rgb(58, 82, 60) * 0.5f * K); }
	inline FSurf Grain(float K = 1.f) { return FSurf::Cloth(Rgb(30, 28, 26) * 0.6f * K); }
	// Light (night blocks and the glow mesh's colours).
	inline FSurf CyanGlow(float Glow) { return FSurf::Lit(Rgb(57, 230, 194) * 0.6f, Glow); }
	inline FSurf AmberGlow(float Glow) { return FSurf::Lit(Rgb(232, 160, 64) * 0.6f, Glow); }
	inline FSurf FoxfireGlow(float Glow) { return FSurf::Lit(Rgb(127, 224, 122) * 0.55f, Glow); }
	inline FSurf HearthGlow(float Glow) { return FSurf::Lit(FLinearColor(1.f, 0.6f, 0.28f), Glow); }
	inline FLinearColor CyanC() { return FLinearColor(FColor(57, 230, 194)); }
	inline FLinearColor AmberC() { return FLinearColor(FColor(232, 160, 64)); }
	inline FLinearColor TealC() { return FLinearColor(FColor(30, 140, 134)); }
	inline FLinearColor StarC() { return FLinearColor(FColor(242, 246, 240)); }
	inline FLinearColor GoldC() { return FLinearColor(FColor(232, 200, 120)); }
	inline FLinearColor GrubC() { return FLinearColor(FColor(127, 232, 208)); }
	inline FLinearColor FoxC() { return FLinearColor(FColor(127, 224, 122)); }
	inline FLinearColor FireLight() { return FLinearColor(FColor(255, 170, 90)); }
	// Code-mesh colours (vertex colour on M_HeldTool).
	inline FLinearColor IvoryC() { return Rgb(230, 220, 198) * 0.62f; }
	inline FLinearColor TanC() { return Rgb(196, 170, 130) * 0.55f; }
	inline FLinearColor RustCapC() { return Rgb(168, 74, 48) * 0.6f; }
	inline FLinearColor HoneyC() { return Rgb(200, 150, 70) * 0.6f; }
	inline FLinearColor UmberC() { return Rgb(58, 42, 30) * 0.6f; }
	inline FLinearColor OysterC() { return Rgb(154, 164, 170) * 0.58f; }
	inline FLinearColor GillC() { return Rgb(200, 138, 138) * 0.55f; }
	inline FLinearColor StipeC() { return Rgb(222, 214, 196) * 0.6f; }
	inline FLinearColor RhizoC() { return Rgb(26, 23, 20) * 0.7f; }
	inline FLinearColor MycelC() { return Rgb(232, 228, 218) * 0.62f; }
	inline FLinearColor BoneC() { return Rgb(217, 207, 184) * 0.66f; }
	inline FLinearColor SkinC() { return Rgb(150, 112, 86) * 0.55f; }
	inline FLinearColor ClayC() { return Rgb(168, 104, 66) * 0.55f; }
	inline FLinearColor StoneC() { return Rgb(150, 146, 134) * 0.6f; }
	inline FLinearColor TimberC() { return Rgb(96, 78, 58) * 0.45f; }
	inline FLinearColor RopeC() { return Rgb(156, 132, 96) * 0.5f; }
	inline FLinearColor JuniperBarkC() { return Rgb(110, 86, 64) * 0.5f; }

	inline float FgH(int32 Seed, int32 Salt) { return FMath::Frac(FMath::Sin(float(Seed * 131 + Salt * 977) * 0.731f) * 43758.5453f); }

	/** The materials the glow and veil meshes share (UWRFungalWorld's instances; the base material if no world). */
	WILDREACH_API UMaterialInterface* GlowMaterial(const UObject* WorldContext);
	WILDREACH_API UMaterialInterface* VeilMaterial(const UObject* WorldContext);

	/** One fungal place's build: the forest's FBuild (blocks, the code mesh, collision, lamps, loops) plus a glowing code
	 *  mesh and a see-through one. */
	struct FFg
	{
		FBuild& B;
		FWRToolMesh Glow;
		FWRToolMesh Veil;
		explicit FFg(FBuild& InB) : B(InB) {}
		float Rand(int32 Salt) const { return B.Rand(Salt); }
		void Finish()
		{
			B.Finish(FLinearColor(0.3f, 0.28f, 0.26f));
			if (B.bServer) return;
			const auto Make = [this](FWRToolMesh& M, UMaterialInterface* Mat)
			{
				if (M.Verts.Num() == 0) return;
				UProceduralMeshComponent* P = NewObject<UProceduralMeshComponent>(B.Act);
				P->SetupAttachment(B.Act->GetRootComponent());
				P->RegisterComponent();
				M.ApplyTo(P);
				if (Mat) P->SetMaterial(0, Mat);
				P->SetCastShadow(false);
				P->SetCullDistance(B.Cull);
			};
			Make(Glow, GlowMaterial(B.Act));
			Make(Veil, VeilMaterial(B.Act));
		}
	};

	// ---------------------------------------------------------------- mushrooms
	enum class ECap : uint8 { Parasol, Ink, Honey, Umbrella, Deep, Spring, Glowcap, Puff };

	/**
	 * A mushroom standing at Base along Up: a stipe H tall (a bulb at its foot, a ring at 70% on a parasol), a cap CapR
	 * across its half (the species' profile), gills underneath in the glow mesh (Glow 0..1: how bright the gills burn).
	 * Collide: a box round the stipe (a giant's stem is a pillar you walk round), and the cap's top as a box when Walk.
	 */
	inline void Mushroom(FFg& F, const FVector& Base, const FVector& Up, float H, float CapR, ECap Kind, float Glow, int32 Seed, bool bCollide = true, bool bWalk = false, const FLinearColor* CapTint = nullptr)
	{
		const FVector U = Up.GetSafeNormal();
		const float StemR = Kind == ECap::Ink ? CapR * 0.16f : Kind == ECap::Honey ? CapR * 0.18f : Kind == ECap::Spring ? CapR * 0.34f : Kind == ECap::Puff ? CapR * 0.5f : CapR * 0.13f;
		const float Bend = (FgH(Seed, 1) - 0.5f) * (Kind == ECap::Honey ? 0.35f : 0.12f);
		const FLinearColor Stipe = Kind == ECap::Deep ? Rgb(236, 244, 248) * 0.6f : StipeC();
		// The stipe: a bulb at its foot, a long gentle taper, a little flare under the cap.
		F.B.M.Lathe(Base - U * 10.f, U, H + 10.f, { FVector(0.f, StemR * 1.55f, StemR * 1.5f), FVector(0.08f, StemR * 1.2f, StemR * 1.2f), FVector(0.5f, StemR, StemR), FVector(0.9f, StemR * 0.92f, StemR * 0.92f), FVector(1.f, StemR * 1.18f, StemR * 1.18f) }, 14, Stipe, 0.8f, Bend, float(Seed % 37 + 1));
		const FVector Top = Base + U * H + FVector(Bend * H * 0.25f, 0.f, 0.f);
		FLinearColor CapC = Kind == ECap::Parasol ? TanC() : Kind == ECap::Ink ? IvoryC() : Kind == ECap::Honey ? HoneyC() : Kind == ECap::Umbrella ? UmberC() : Kind == ECap::Spring ? Rgb(196, 184, 160) * 0.62f : Kind == ECap::Puff ? Rgb(237, 230, 214) * 0.62f : IvoryC();
		if (CapTint) CapC = *CapTint;
		TArray<FVector> Prof;
		float CapH = CapR * 0.35f;
		switch (Kind)
		{
		case ECap::Parasol:   // flat with a raised umbo, scales
			CapH = CapR * 0.28f;
			Prof = { FVector(0.f, CapR, CapR), FVector(0.25f, CapR * 0.96f, CapR * 0.96f), FVector(0.55f, CapR * 0.62f, CapR * 0.62f), FVector(0.75f, CapR * 0.28f, CapR * 0.28f), FVector(0.92f, CapR * 0.12f, CapR * 0.12f), FVector(1.f, 1.f, 1.f) };
			break;
		case ECap::Ink:       // a tall bell, its rim curling and blackening
			CapH = CapR * 1.6f;
			Prof = { FVector(0.f, CapR, CapR), FVector(0.2f, CapR * 0.95f, CapR * 0.95f), FVector(0.6f, CapR * 0.78f, CapR * 0.78f), FVector(0.88f, CapR * 0.48f, CapR * 0.48f), FVector(1.f, 1.f, 1.f) };
			break;
		case ECap::Spring:    // a bulging, taut dome (pressure)
			CapH = CapR * 0.7f;
			Prof = { FVector(0.f, CapR * 0.92f, CapR * 0.92f), FVector(0.25f, CapR, CapR), FVector(0.6f, CapR * 0.86f, CapR * 0.86f), FVector(0.88f, CapR * 0.5f, CapR * 0.5f), FVector(1.f, 1.f, 1.f) };
			break;
		case ECap::Puff:      // a ball
			CapH = CapR * 1.7f;
			Prof = { FVector(0.f, CapR * 0.5f, CapR * 0.5f), FVector(0.25f, CapR * 0.92f, CapR * 0.92f), FVector(0.55f, CapR, CapR), FVector(0.85f, CapR * 0.75f, CapR * 0.75f), FVector(1.f, 1.f, 1.f) };
			break;
		default:              // convex
			Prof = { FVector(0.f, CapR, CapR), FVector(0.3f, CapR * 0.9f, CapR * 0.9f), FVector(0.65f, CapR * 0.62f, CapR * 0.62f), FVector(0.9f, CapR * 0.3f, CapR * 0.3f), FVector(1.f, 1.f, 1.f) };
			break;
		}
		const bool bTranslucent = Kind == ECap::Deep || Kind == ECap::Glowcap;
		const FVector CapBase = Top - U * (Kind == ECap::Ink ? CapH * 0.6f : CapH * 0.12f);
		if (bTranslucent)
		{
			// A deepcap's / glow-cap's cap is translucent and lit from within: drawn in the glow mesh, its colour the light.
			F.Glow.Lathe(CapBase, U, CapH, Prof, 16, Kind == ECap::Deep ? Rgb(200, 236, 255) : CyanC(), FMath::Clamp(0.45f + Glow * 0.5f, 0.f, 1.f), 0.f, float(Seed % 41 + 2));
		}
		else
		{
			F.B.M.Lathe(CapBase, U, CapH, Prof, 20, CapC, 0.72f, 0.f, float(Seed % 41 + 2));
		}
		// The gills: a shallow cone under the cap from the stem to just inside the rim, glowing; the ink cap's are black.
		if (Kind != ECap::Puff)
		{
			const float GillDrop = FMath::Max(4.f, CapR * 0.1f);
			const FVector GillTop = CapBase + U * 2.f;
			if (Glow > 0.f) F.Glow.Lathe(GillTop - U * GillDrop, U, GillDrop, { FVector(0.f, StemR * 1.05f, StemR * 1.05f), FVector(0.5f, CapR * 0.6f, CapR * 0.6f), FVector(1.f, CapR * 0.94f, CapR * 0.94f) }, 20, Kind == ECap::Honey ? AmberC() : CyanC(), FMath::Clamp(Glow, 0.f, 1.f));
			else F.B.M.Lathe(GillTop - U * GillDrop, U, GillDrop, { FVector(0.f, StemR * 1.05f, StemR * 1.05f), FVector(0.5f, CapR * 0.6f, CapR * 0.6f), FVector(1.f, CapR * 0.94f, CapR * 0.94f) }, 20, Kind == ECap::Ink ? Rgb(30, 28, 30) * 0.6f : GillC(), 0.9f);
		}
		// A parasol's ring on the stem; scales on its cap (small dark flecks: blocks).
		if (Kind == ECap::Parasol)
		{
			F.B.M.Lathe(Base + U * (H * 0.7f), U, StemR * 0.4f, { FVector(0.f, StemR * 1.02f, StemR * 1.02f), FVector(0.5f, StemR * 1.9f, StemR * 1.9f), FVector(1.f, StemR * 1.05f, StemR * 1.05f) }, 12, IvoryC(), 0.9f);
			if (CapR > 150.f)
			{
				for (int32 s = 0; s < 10; ++s)
				{
					const float A = 2.f * PI * FgH(Seed, s + 10), Rr = CapR * (0.3f + 0.55f * FgH(Seed, s + 20));
					const FVector P = CapBase + U * (CapH * (1.f - Rr / CapR) * 0.9f) + FVector(FMath::Cos(A) * Rr, FMath::Sin(A) * Rr, 0.f);
					F.B.K.Add(FSurf::Cloth(Rgb(110, 84, 60) * 0.5f), FVector(CapR * 0.08f, CapR * 0.06f, 3.f), P, FRotator(0.f, FMath::RadiansToDegrees(A), 0.f));
				}
			}
		}
		if (bCollide && H > 120.f)
		{
			const float R = FMath::Max(15.f, StemR * 0.9f);
			F.B.SegBox(Base, Base + U * H, R, R);
		}
		if (bWalk && CapR > 80.f)
		{
			// A cap you can stand on: its top as a flat box (an octagon of two boxes) at the cap's crown.
			const FVector C = CapBase + U * (CapH * 0.55f);
			F.B.Box(C, FVector(CapR * 0.7f, CapR * 0.7f, 12.f));
			F.B.Box(C, FVector(CapR * 0.7f, CapR * 0.7f, 12.f), FRotator(0.f, 45.f, 0.f));
		}
	}

	/** A bracket fungus out of a wall at At (its face along Out): a half-disc shelf R deep. Step: box collision on top,
	 *  so a run of them is a stair up the wall (fungal 2.2: shelf-fungus stairs). Oyster: the fan-shaped grey-blue kind. */
	inline void Shelf(FFg& F, const FVector& At, const FVector& Out, float R, bool bStep, bool bOyster, int32 Seed)
	{
		const FVector O = FVector(Out.X, Out.Y, 0.f).GetSafeNormal();
		const FVector Side = FVector::CrossProduct(FVector::UpVector, O);
		const float Th = FMath::Max(10.f, R * (bOyster ? 0.12f : 0.22f));
		TArray<TArray<FVector>> Rings;
		const int32 NS = 9;
		for (int32 r = 0; r <= 4; ++r)
		{
			const float T = r / 4.f;
			TArray<FVector> Ring;
			for (int32 s = 0; s <= NS; ++s)
			{
				const float A = PI * (float(s) / NS - 0.5f);
				const float Rr = R * T * (1.f + 0.08f * FMath::Sin(A * 5.f + Seed));
				const float Z = Th * (1.f - T * T) * (bOyster ? 0.6f : 1.f) - (bOyster ? R * 0.15f * T * T : 0.f);
				Ring.Add(At + O * FMath::Cos(A) * Rr + Side * FMath::Sin(A) * Rr * 1.15f + FVector(0.f, 0.f, Z));
			}
			Rings.Add(Ring);
		}
		F.B.M.Loft(Rings, bOyster ? OysterC() : Rgb(170, 140, 100) * 0.55f, 0.8f, true);
		// Growth bands on the top (a hoof fungus's zones), the pore face underneath faintly lit.
		F.Glow.Lathe(At - FVector(0, 0, Th * 0.4f), FVector::UpVector, 4.f, { FVector(0.f, R * 0.3f, R * 0.3f), FVector(1.f, R * 0.85f, R * 0.85f) }, 8, bOyster ? CyanC() * 0.6f : AmberC() * 0.5f, 0.18f);
		if (bStep) F.B.Box(At + O * (R * 0.45f) + FVector(0, 0, Th * 0.5f), FVector(R * 0.42f, R * 0.85f, Th * 0.5f + 4.f), O.Rotation());
	}

	/** A rhizomorph: a black bootlace cable along Pts, R thick, with fibrous twist ridges; Walk: box collision along its
	 *  top (a cable bridge). Silk: grey Webspinner silk wrapped round it in places. */
	inline void Cable(FFg& F, const TArray<FVector>& Pts, float R, bool bWalk, bool bSilk, int32 Seed)
	{
		if (Pts.Num() < 2) return;
		F.B.M.Tube(Pts, R, 10, RhizoC(), 0.3f);
		// The twist: three thin strands spiralling round it.
		if (R > 20.f)
		{
			for (int32 s = 0; s < 3; ++s)
			{
				TArray<FVector> Sp;
				for (int32 i = 0; i < Pts.Num(); ++i)
				{
					const FVector D = (Pts[FMath::Min(i + 1, Pts.Num() - 1)] - Pts[FMath::Max(i - 1, 0)]).GetSafeNormal();
					FVector A, Bv;
					D.FindBestAxisVectors(A, Bv);
					const float Ang = i * 0.9f + s * 2.094f;
					Sp.Add(Pts[i] + (A * FMath::Cos(Ang) + Bv * FMath::Sin(Ang)) * R * 0.95f);
				}
				F.B.M.Tube(Sp, R * 0.18f, 5, Rgb(40, 34, 28) * 0.7f, 0.4f);
			}
		}
		if (bSilk)
		{
			for (int32 i = 1; i + 1 < Pts.Num(); i += 3)
			{
				if (FgH(Seed, i) < 0.5f) continue;
				const FVector D = (Pts[i + 1] - Pts[i - 1]).GetSafeNormal();
				F.Veil.Lathe(Pts[i] - D * R * 2.f, D, R * 4.f, { FVector(0.f, R * 1.02f, R * 1.02f), FVector(0.5f, R * 1.45f, R * 1.3f), FVector(1.f, R * 1.02f, R * 1.02f) }, 10, Rgb(232, 232, 224), 0.15f);
			}
		}
		if (bWalk)
		{
			for (int32 i = 0; i + 1 < Pts.Num(); ++i)
			{
				const FVector A = Pts[i] + FVector(0, 0, R * 0.55f), Bp = Pts[i + 1] + FVector(0, 0, R * 0.55f);
				F.B.SegBox(A, Bp, FMath::Max(R * 0.75f, 70.f), R * 0.45f);
			}
		}
	}

	/** A cordyceps stalk: a thin white club from Base along Dir, Len long, its tip swollen and cyan (fungal 1.2 summited). */
	inline void Stalk(FFg& F, const FVector& Base, const FVector& Dir, float Len, int32 Seed)
	{
		const FVector D = Dir.GetSafeNormal();
		F.B.M.Lathe(Base, D, Len * 0.85f, { FVector(0.f, 2.2f, 2.2f), FVector(0.5f, 1.6f, 1.6f), FVector(1.f, 1.4f, 1.4f) }, 6, Rgb(232, 226, 208) * 0.62f, 0.8f, (FgH(Seed, 3) - 0.5f) * 0.4f);
		F.Glow.Lathe(Base + D * Len * 0.82f, D, Len * 0.18f, { FVector(0.f, 1.6f, 1.6f), FVector(0.5f, 3.6f, 3.6f), FVector(1.f, 0.6f, 0.6f) }, 6, CyanC(), 0.7f);
	}

	enum class EPose : uint8 { Beam, Rack, CableTip, Roof, Lying, Bound, Elder };

	/**
	 * One of the summited (fungal 1.2, 2.20): a Deepfarmer who climbed high and bit down, dead and furred pale with
	 * mycelium, a white stalk out of the back of the skull, the face toward the Heart (Face: a world direction). Built
	 * from limbs. Beam / Rack / CableTip / Roof: the four places they clamp; Lying: on a grave; Bound: the one in the
	 * Last House who refused (no stalk, mouth bound with cord, wrists bound); Elder: sitting in the tower's ring.
	 */
	inline void Body(FFg& F, const FVector& Jaw, const FVector& Face, EPose Pose, int32 Seed)
	{
		const FVector Fw = FVector(Face.X, Face.Y, 0.f).GetSafeNormal();
		const FVector Rt = FVector::CrossProduct(FVector::UpVector, Fw);
		const FLinearColor Skin = FMath::Lerp(SkinC(), MycelC(), Pose == EPose::Bound ? 0.25f : 0.6f);
		const FLinearColor Cloth = Rgb(110, 96, 74) * 0.45f;
		const auto L = [&](const FVector& A, const FVector& Bp, float R0, float R1, const FLinearColor& C) { F.B.Limb(A, Bp, R0, R1, C, 7, 0.9f); };
		FVector Head, Neck, Hip, Chest;
		if (Pose == EPose::Lying || Pose == EPose::Bound)
		{
			// On its back, along Face.
			Head = Jaw + FVector(0, 0, 14.f);
			Neck = Head - Fw * 18.f;
			Chest = Neck - Fw * 30.f;
			Hip = Chest - Fw * 34.f;
			L(Neck, Hip, 17.f, 15.f, Cloth);
			L(Hip, Hip - Fw * 46.f + Rt * 10.f, 8.f, 6.f, Cloth);
			L(Hip - Fw * 46.f + Rt * 10.f, Hip - Fw * 90.f + Rt * 12.f, 6.f, 4.5f, Skin);
			L(Hip, Hip - Fw * 46.f - Rt * 10.f, 8.f, 6.f, Cloth);
			L(Hip - Fw * 46.f - Rt * 10.f, Hip - Fw * 90.f - Rt * 12.f, 6.f, 4.5f, Skin);
			if (Pose == EPose::Bound)
			{
				// Wrists crossed on the chest and bound (the knots pulled tight with the teeth), a cord round the mouth.
				L(Chest + Rt * 18.f, Chest + Fw * 6.f + FVector(0, 0, 12.f), 5.f, 4.f, Skin);
				L(Chest - Rt * 18.f, Chest + Fw * 6.f + FVector(0, 0, 12.f), 5.f, 4.f, Skin);
				F.B.M.Lathe(Chest + Fw * 6.f + FVector(0, 0, 8.f) - Rt * 6.f, Rt, 12.f, { FVector(0.f, 5.f, 5.f), FVector(1.f, 5.f, 5.f) }, 6, Rgb(156, 132, 96) * 0.5f, 0.95f);
				F.B.M.Lathe(Head - Rt * 9.f + Fw * 3.f, Rt, 18.f, { FVector(0.f, 1.2f, 1.2f), FVector(1.f, 1.2f, 1.2f) }, 5, Rgb(156, 132, 96) * 0.5f, 0.95f);
			}
			else
			{
				L(Chest + Rt * 18.f, Chest + Rt * 30.f - Fw * 30.f, 5.f, 4.f, Skin);
				L(Chest - Rt * 18.f, Chest - Rt * 30.f - Fw * 30.f, 5.f, 4.f, Skin);
			}
			F.B.Ball(Head, 11.f, Skin, 0.9f, 1.05f);
			if (Pose == EPose::Lying) Stalk(F, Head - FVector(0, 0, 6.f), FVector::UpVector + Fw * 0.3f, 40.f + 25.f * FgH(Seed, 1), Seed);
			return;
		}
		// Clamped high: the jaw locked on the root or beam at Jaw, the body hanging below and behind it.
		Head = Jaw - Fw * 8.f + FVector(0, 0, 4.f);
		Neck = Head - Fw * 6.f - FVector(0, 0, 14.f);
		const float Hang = Pose == EPose::CableTip ? 1.f : Pose == EPose::Rack ? 0.75f : Pose == EPose::Elder ? 0.f : 0.35f;
		Chest = Neck - FVector(0, 0, 28.f * (0.3f + Hang)) - Fw * 22.f * (1.f - Hang);
		Hip = Chest - FVector(0, 0, 36.f * (0.4f + Hang)) - Fw * 28.f * (1.f - Hang);
		L(Neck, Chest, 7.f, 16.f, Skin);
		L(Chest, Hip, 17.f, 15.f, Cloth);
		// Arms: wrapped round the root (rack, cable tip) or braced on the roof / beam; legs folded or hanging.
		for (const float S : { -1.f, 1.f })
		{
			const FVector Sh = Chest + Rt * S * 18.f + FVector(0, 0, 8.f);
			const FVector Hand = Pose == EPose::Elder ? Chest + Fw * 22.f + Rt * S * 10.f - FVector(0, 0, 26.f) : Jaw - Fw * 4.f + Rt * S * 12.f - FVector(0, 0, 6.f);
			const FVector Elb = (Sh + Hand) * 0.5f + Rt * S * 14.f - FVector(0, 0, 8.f);
			L(Sh, Elb, 5.5f, 4.5f, Skin);
			L(Elb, Hand, 4.5f, 3.5f, Skin);
			const FVector Kn = Pose == EPose::Elder ? Hip + Fw * 40.f + Rt * S * 14.f + FVector(0, 0, 14.f) : Hip - FVector(0, 0, 42.f * Hang) + Fw * 26.f * (1.f - Hang) + Rt * S * 10.f;
			const FVector Ft = Pose == EPose::Elder ? Kn - FVector(0, 0, 40.f) + Fw * 4.f : Kn - FVector(0, 0, 40.f * Hang + 10.f) - Fw * 30.f * (1.f - Hang);
			L(Hip + Rt * S * 9.f, Kn, 8.f, 6.f, Cloth);
			L(Kn, Ft, 6.f, 4.5f, Skin);
		}
		F.B.Ball(Head, 11.f, Skin, 0.9f, 1.05f);
		// Mycelium fur over the shoulders (pale tufts), and the stalk from the back of the skull.
		F.B.Ball(Chest + FVector(0, 0, 12.f) - Fw * 4.f, 12.f, MycelC(), 1.f, 0.6f);
		Stalk(F, Head - Fw * 9.f + FVector(0, 0, 4.f), (FVector::UpVector * 0.8f - Fw * 0.6f), 45.f + 30.f * FgH(Seed, 2), Seed);
	}

	/** A clay knocker idol (Cornish lore, fungal 1.2): a 50 cm figure, a round head with two stick eyes, arms folded,
	 *  set on a stone facing Face. */
	inline void Knocker(FFg& F, const FVector& At, const FVector& Face, float Scale, int32 Seed)
	{
		const FVector Fw = FVector(Face.X, Face.Y, 0.f).GetSafeNormal();
		const FVector Rt = FVector::CrossProduct(FVector::UpVector, Fw);
		F.B.K.Add(Carved(0.9f), FVector(26.f, 26.f, 12.f) * Scale, At + FVector(0, 0, 6.f * Scale));
		F.B.M.Lathe(At + FVector(0, 0, 12.f * Scale), FVector::UpVector, 30.f * Scale, { FVector(0.f, 11.f * Scale, 11.f * Scale), FVector(0.6f, 9.f * Scale, 9.f * Scale), FVector(1.f, 6.f * Scale, 6.f * Scale) }, 10, ClayC(), 0.92f, 0.f, float(Seed % 9 + 1));
		const FVector H = At + FVector(0, 0, 48.f * Scale);
		F.B.Ball(H, 9.f * Scale, ClayC(), 0.92f);
		for (const float S : { -1.f, 1.f })
		{
			F.B.K.Rod(Timber(0.7f), H + Fw * 6.f * Scale + Rt * S * 3.5f * Scale + FVector(0, 0, 2.f * Scale), H + Fw * 12.f * Scale + Rt * S * 4.f * Scale + FVector(0, 0, 2.f * Scale), 1.6f * Scale);
			F.B.Limb(At + FVector(0, 0, 36.f * Scale) + Rt * S * 9.f * Scale, At + FVector(0, 0, 28.f * Scale) + Fw * 8.f * Scale - Rt * S * 3.f * Scale, 3.f * Scale, 2.6f * Scale, ClayC(), 6);
		}
	}

	/** A glow-cap ring (the Deepfarmer waymark, world-map SPEC): nine caps in a 3 m ring round a carved stone. */
	inline void GlowRing(FFg& F, const FVector& At, int32 Seed, TArray<FVector>* OutCaps = nullptr)
	{
		F.B.K.Add(Carved(), FVector(46.f, 46.f, 70.f), At + FVector(0, 0, 32.f), FRotator(0.f, FgH(Seed, 1) * 90.f, 0.f));
		F.B.K.Add(Carved(0.8f), FVector(30.f, 30.f, 16.f), At + FVector(0, 0, 74.f), FRotator(0.f, FgH(Seed, 1) * 90.f + 45.f, 0.f));
		F.B.Box(At + FVector(0, 0, 35.f), FVector(24.f, 24.f, 35.f));
		for (int32 c = 0; c < 9; ++c)
		{
			const float A = 2.f * PI * c / 9.f;
			const FVector P = At + FVector(FMath::Cos(A) * 150.f, FMath::Sin(A) * 150.f, -4.f);
			Mushroom(F, P, FVector::UpVector + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * 0.08f, 22.f + 10.f * FgH(Seed, c + 3), 11.f + 4.f * FgH(Seed, c + 9), ECap::Glowcap, 0.8f, Seed + c, false);
			if (OutCaps) OutCaps->Add(P);
		}
	}

	/** Bones of a creature that fell (fungal 2.4 host clusters, the Fall Field): skull, spine, ribs, long bones, furred
	 *  white where the mycelium took them; Scale 1 is a deer. Returns where deepcaps grow on it. */
	inline TArray<FVector> Bones(FFg& F, const FVector& At, float Yaw, float Scale, int32 Seed)
	{
		TArray<FVector> Caps;
		const FRotator R(0.f, Yaw, 0.f);
		const auto P = [&](float X, float Y, float Z) { return At + R.RotateVector(FVector(X, Y, Z) * Scale); };
		const FLinearColor C = FMath::Lerp(BoneC(), MycelC(), 0.5f);
		// The spine: a gentle curve of vertebrae; the ribs arching up from it like a broken basket.
		TArray<FVector> Sp;
		for (int32 i = 0; i <= 8; ++i) Sp.Add(P(-80.f + i * 20.f, 6.f * FMath::Sin(i * 0.7f), 12.f + 4.f * FMath::Sin(i * 0.5f)));
		F.B.M.Tube(Sp, 5.f * Scale, 6, C, 0.85f);
		for (int32 r = 0; r < 7; ++r)
		{
			const float X = -40.f + r * 12.f;
			if (FgH(Seed, r) < 0.15f) continue;
			for (const float S : { -1.f, 1.f })
			{
				TArray<FVector> Rb;
				for (int32 k = 0; k <= 5; ++k) { const float T = k / 5.f; Rb.Add(P(X + T * 6.f, S * (8.f + 34.f * FMath::Sin(T * 1.9f)), 14.f + 36.f * FMath::Sin(T * PI * 0.85f) * (1.f - 0.4f * T))); }
				F.B.M.Tube(Rb, 2.4f * Scale, 5, C, 0.85f);
			}
		}
		F.B.Ball(P(100.f, 0.f, 18.f), 16.f * Scale, C, 0.85f, 0.8f);
		F.B.Limb(P(100.f, 0.f, 12.f), P(140.f, 4.f, 6.f), 9.f * Scale, 5.f * Scale, C);
		for (int32 l = 0; l < 3; ++l) F.B.Limb(P(-60.f + l * 50.f, 60.f - l * 30.f, 4.f), P(-20.f + l * 50.f, 80.f - l * 20.f, 4.f), 4.f * Scale, 3.f * Scale, C);
		// Mycelium fur over the ribs (pale cloth mounds), where the deepcaps rise.
		F.B.Ball(P(-10.f, 0.f, 8.f), 34.f * Scale, MycelC() * 0.9f, 1.f, 0.35f);
		Caps.Add(P(-10.f, 14.f, 14.f));
		Caps.Add(P(30.f, -10.f, 16.f));
		Caps.Add(P(100.f, 0.f, 30.f));
		return Caps;
	}

	/** A cap-house of the Deepfarmers (fungal 2.5): a hollow giant cap: the stipe its round wall (a door and gill
	 *  windows), the cap its roof; Lit: the windows glow amber (the city). Collision: the wall as a ring of boxes with
	 *  the door's gap, the roof's top. Returns the roof's ridge point (where the summited lie). */
	inline FVector CapHouse(FFg& F, const FVector& At, float Yaw, float R, float H, bool bLit, int32 Seed)
	{
		const FRotator Rot(0.f, Yaw, 0.f);
		const FVector Fw = Rot.Vector(), Rt = FVector::CrossProduct(FVector::UpVector, Fw);
		F.B.M.Lathe(At - FVector(0, 0, 20.f), FVector::UpVector, H + 20.f, { FVector(0.f, R * 1.12f, R * 1.12f), FVector(0.15f, R, R), FVector(0.85f, R * 0.94f, R * 0.94f), FVector(1.f, R * 1.08f, R * 1.08f) }, 18, StipeC() * 0.95f, 0.85f, 0.f, float(Seed % 23 + 1));
		const float CapR = R * 1.9f, CapH = R * 0.9f;
		F.B.M.Lathe(At + FVector(0, 0, H - CapH * 0.15f), FVector::UpVector, CapH, { FVector(0.f, CapR, CapR), FVector(0.3f, CapR * 0.9f, CapR * 0.9f), FVector(0.7f, CapR * 0.55f, CapR * 0.55f), FVector(1.f, 1.f, 1.f) }, 22, FMath::Lerp(HoneyC(), UmberC(), FgH(Seed, 2) * 0.6f), 0.75f);
		F.Glow.Lathe(At + FVector(0, 0, H - CapH * 0.2f), FVector::UpVector, 6.f, { FVector(0.f, R * 1.05f, R * 1.05f), FVector(1.f, CapR * 0.92f, CapR * 0.92f) }, 22, bLit ? AmberC() : CyanC(), bLit ? 0.5f : 0.3f);
		// The door: a dark arched opening in the wall facing Fw (a frame of carved wood); windows: gill-slots glowing.
		const FVector Door = At + Fw * (R * 0.98f);
		F.B.K.Add(FSurf::Stone(Rgb(18, 16, 14) * 0.6f, 0.95f), FVector(8.f, 90.f, 190.f), Door + FVector(0, 0, 95.f), Rot);
		F.B.K.Rod(Timber(), Door - Rt * 50.f, Door - Rt * 50.f + FVector(0, 0, 200.f), 12.f, true);
		F.B.K.Rod(Timber(), Door + Rt * 50.f, Door + Rt * 50.f + FVector(0, 0, 200.f), 12.f, true);
		F.B.K.Rod(Timber(), Door - Rt * 56.f + FVector(0, 0, 204.f), Door + Rt * 56.f + FVector(0, 0, 204.f), 14.f, true);
		for (int32 w = 0; w < 3; ++w)
		{
			const float A = FMath::DegreesToRadians(Yaw + 70.f + w * 110.f);
			const FVector Dir(FMath::Cos(A), FMath::Sin(A), 0.f);
			F.Glow.Lathe(At + Dir * (R * 0.97f) + FVector(0, 0, H * 0.55f), Dir, 6.f, { FVector(0.f, 14.f, 40.f), FVector(1.f, 12.f, 36.f) }, 8, bLit ? AmberC() : Rgb(40, 60, 56), bLit ? 0.9f : 0.05f);
		}
		if (bLit) F.B.Lamp(At + Fw * (R * 1.3f) + FVector(0, 0, H * 0.5f), AmberC(), 1800.f, R * 5.f);
		// Collision: the wall ring (14 boxes, the door's gap left open), the roof.
		for (int32 s = 0; s < 14; ++s)
		{
			const float A = 2.f * PI * (s + 0.5f) / 14.f;
			const FVector Dir(FMath::Cos(A), FMath::Sin(A), 0.f);
			if (FVector::DotProduct(Dir, Fw) > 0.93f) continue;
			F.B.Box(At + Dir * R * 0.95f + FVector(0, 0, H * 0.5f), FVector(18.f, R * 0.24f, H * 0.5f), Dir.Rotation());
		}
		F.B.Box(At + FVector(0, 0, H + CapH * 0.25f), FVector(CapR * 0.6f, CapR * 0.6f, CapH * 0.25f));
		return At + FVector(0, 0, H + CapH * 0.8f);
	}

	/** A Deepfarmer log bed (survival-crafting KEPT): oak logs laid on a stone kerb, oyster caps fanning from their sides. */
	inline void LogBed(FFg& F, const FVector& At, float Yaw, int32 Seed)
	{
		const FRotator R(0.f, Yaw, 0.f);
		const FVector Fw = R.Vector(), Rt = FVector::CrossProduct(FVector::UpVector, Fw);
		F.B.K.Add(Carved(0.85f), FVector(260.f, 120.f, 22.f), At + FVector(0, 0, 11.f), R);
		for (int32 l = 0; l < 3; ++l)
		{
			const FVector C = At + Rt * ((l - 1) * 36.f) + FVector(0, 0, 38.f);
			F.B.Limb(C - Fw * 120.f, C + Fw * 120.f, 17.f, 16.f, TimberC() * 0.9f, 10);
			for (int32 c = 0; c < 4; ++c)
			{
				if (FgH(Seed, l * 7 + c) < 0.3f) continue;
				const FVector Side = (l == 0 ? -Rt : l == 2 ? Rt : FVector::UpVector);
				Shelf(F, C + Fw * (-90.f + c * 60.f) + Side * 16.f, Side.Z > 0.5f ? Fw : Side, 14.f + 6.f * FgH(Seed, c + 30), false, true, Seed + l * 5 + c);
			}
		}
		F.B.Box(At + FVector(0, 0, 40.f), FVector(130.f, 60.f, 40.f), R);
	}

	/** A mushroom drying rack 5 m tall (fungal 2.4 S3): four posts, crossbars every 70 cm, caps dried to grey paper. */
	inline FVector DryingRack(FFg& F, const FVector& At, float Yaw, float H, int32 Seed)
	{
		const FRotator R(0.f, Yaw, 0.f);
		const FVector Fw = R.Vector(), Rt = FVector::CrossProduct(FVector::UpVector, Fw);
		for (const float X : { -110.f, 110.f }) for (const float Y : { -40.f, 40.f }) F.B.K.Rod(OldWood(), At + Fw * X + Rt * Y, At + Fw * X + Rt * Y + FVector(0, 0, H), 12.f, true);
		for (float Z = 70.f; Z < H; Z += 70.f)
		{
			F.B.K.Rod(OldWood(0.9f), At - Fw * 116.f + FVector(0, 0, Z), At + Fw * 116.f + FVector(0, 0, Z), 7.f);
			for (int32 c = 0; c < 6; ++c)
			{
				if (FgH(Seed, int32(Z) + c) < 0.25f) continue;
				const FVector P = At - Fw * 90.f + Fw * (c * 36.f) + FVector(0, 0, Z - 6.f);
				F.B.K.Add(FSurf::Cloth(Rgb(150, 146, 138) * 0.5f), FVector(26.f, 26.f, 3.f), P, FRotator(FgH(Seed, c) * 20.f, FgH(Seed, c + 5) * 90.f, 0.f), EWRLookMesh::Cylinder);
			}
		}
		F.B.SegBox(At - Fw * 110.f, At - Fw * 110.f + FVector(0, 0, H), 10.f, 10.f);
		F.B.SegBox(At + Fw * 110.f, At + Fw * 110.f + FVector(0, 0, H), 10.f, 10.f);
		return At + FVector(0, 0, H);
	}

	/** A round Deepfarmer granary on stone feet (fungal 2.4 S4): wattle drum, conical thatch, its ladder laid down. */
	inline void Granary(FFg& F, const FVector& At, float R, float H, float Yaw, bool bLadderDown, int32 Seed)
	{
		for (int32 s = 0; s < 6; ++s)
		{
			const float A = 2.f * PI * s / 6.f;
			F.B.K.Add(Carved(0.85f), FVector(40.f, 40.f, 90.f), At + FVector(FMath::Cos(A) * R * 0.75f, FMath::Sin(A) * R * 0.75f, 45.f));
			F.B.K.Disc(Carved(0.75f), At + FVector(FMath::Cos(A) * R * 0.75f, FMath::Sin(A) * R * 0.75f, 96.f), FVector::UpVector, 70.f, 12.f);   // the rat stone
		}
		F.B.M.Lathe(At + FVector(0, 0, 100.f), FVector::UpVector, H, { FVector(0.f, R, R), FVector(0.5f, R * 1.04f, R * 1.04f), FVector(1.f, R, R) }, 16, Rgb(150, 120, 84) * 0.5f, 0.92f);
		F.B.M.Lathe(At + FVector(0, 0, 100.f + H - 10.f), FVector::UpVector, R * 1.2f, { FVector(0.f, R * 1.25f, R * 1.25f), FVector(0.6f, R * 0.5f, R * 0.5f), FVector(1.f, 2.f, 2.f) }, 16, Rgb(176, 146, 80) * 0.45f, 0.95f);
		F.B.Box(At + FVector(0, 0, 100.f + H * 0.5f), FVector(R * 0.85f, R * 0.85f, H * 0.5f));
		const FVector Fw = FRotator(0.f, Yaw, 0.f).Vector();
		if (bLadderDown)
		{
			const FVector L0 = At + Fw * (R + 60.f), L1 = L0 + FVector::CrossProduct(FVector::UpVector, Fw) * 340.f;
			for (const float S : { -22.f, 22.f }) F.B.K.Rod(OldWood(), L0 + Fw * S + FVector(0, 0, 5.f), L1 + Fw * S + FVector(0, 0, 5.f), 7.f);
			for (int32 r = 1; r < 9; ++r) F.B.K.Rod(OldWood(0.9f), FMath::Lerp(L0, L1, r / 9.f) - Fw * 22.f + FVector(0, 0, 5.f), FMath::Lerp(L0, L1, r / 9.f) + Fw * 22.f + FVector(0, 0, 5.f), 4.f);
		}
	}

	/** A sealed clay jar with a spore print pressed on its lid (fungal 2.4 C8): white, cream, pink, rust or purple-black. */
	inline void Jar(FFg& F, const FVector& At, float H, int32 Seed)
	{
		static const FColor Prints[] = { FColor(240, 238, 230), FColor(226, 210, 170), FColor(214, 150, 150), FColor(150, 76, 44), FColor(52, 34, 48) };
		const float R = H * 0.34f;
		F.B.M.Lathe(At, FVector::UpVector, H, { FVector(0.f, R * 0.6f, R * 0.6f), FVector(0.4f, R, R), FVector(0.8f, R * 0.7f, R * 0.7f), FVector(0.9f, R * 0.45f, R * 0.45f), FVector(1.f, R * 0.5f, R * 0.5f) }, 12, ClayC(), 0.9f);
		F.B.K.Disc(FSurf::Cloth(FLinearColor(Prints[int32(FgH(Seed, 1) * 5.f) % 5]) * 0.55f), At + FVector(0, 0, H + 1.f), FVector::UpVector, R * 0.9f, 2.f);
	}

	/** A carved standing stone (fungal 2.3 L3: spiralling mycelium cut into it). */
	inline void Standing(FFg& F, const FVector& At, float H, float Yaw, int32 Seed)
	{
		const FRotator R(0.f, Yaw, 0.f);
		F.B.K.Add(Carved(0.95f + 0.1f * FgH(Seed, 1)), FVector(70.f, 44.f, H), At + FVector(0, 0, H * 0.5f - 20.f), R + FRotator(FgH(Seed, 2) * 4.f - 2.f, 0.f, 0.f));
		for (int32 g = 0; g < 7; ++g)
		{
			const float T = g / 7.f;
			F.B.K.Add(Carved(0.6f), FVector(4.f, 30.f - g * 2.f, 4.f), At + R.RotateVector(FVector(23.f, 8.f * FMath::Sin(T * 9.f), H * (0.25f + 0.6f * T))), R + FRotator(0.f, 0.f, T * 160.f));
		}
		F.B.Box(At + FVector(0, 0, H * 0.5f - 20.f), FVector(35.f, 22.f, H * 0.5f), R);
	}

	/** A cold-light brazier: a stone bowl of glow-moss oil burning cyan (the fungal shrines' fire, fungal 2.11). */
	inline void ColdBrazier(FFg& F, const FVector& At, float Scale)
	{
		F.B.K.Add(Carved(), FVector(50.f, 50.f, 70.f) * Scale, At + FVector(0, 0, 35.f * Scale));
		F.B.M.Lathe(At + FVector(0, 0, 70.f * Scale), FVector::UpVector, 26.f * Scale, { FVector(0.f, 20.f * Scale, 20.f * Scale), FVector(1.f, 44.f * Scale, 44.f * Scale) }, 14, StoneC(), 0.85f);
		F.Glow.Lathe(At + FVector(0, 0, 90.f * Scale), FVector::UpVector, 6.f * Scale, { FVector(0.f, 38.f * Scale, 38.f * Scale), FVector(1.f, 34.f * Scale, 34.f * Scale) }, 14, CyanC(), 1.f);
		F.B.Box(At + FVector(0, 0, 45.f * Scale), FVector(30.f, 30.f, 45.f) * Scale);
	}

	/** A Deepfarmer bronze sickle (their one blade: fungal 2.3 L6, 2.4 S6). */
	inline void Sickle(FFg& F, const FVector& At, const FVector& Dir, float Scale)
	{
		const FVector D = Dir.GetSafeNormal();
		FVector A, Bv;
		D.FindBestAxisVectors(A, Bv);
		F.B.K.Rod(Timber(), At - D * 30.f * Scale, At, 5.f * Scale);
		TArray<FVector> Blade;
		for (int32 i = 0; i <= 8; ++i) { const float T = i / 8.f; Blade.Add(At + D * (40.f * FMath::Sin(T * 2.4f)) * Scale + A * (40.f * (1.f - FMath::Cos(T * 2.4f))) * Scale); }
		F.B.M.Tube(Blade, 3.f * Scale, 5, Rgb(140, 106, 58) * 0.6f, 0.45f);
	}

	/** A Kitewright war-kite (fungal 2.4 S6): a bamboo frame, sailcloth wings, prayer cloth on its tail. Crashed: nose
	 *  down, one wing broken back. */
	inline void Kite(FFg& F, const FVector& At, float Yaw, float Span, bool bCrashed, int32 Seed)
	{
		const FRotator R(bCrashed ? -32.f : 0.f, Yaw, bCrashed ? 14.f : 0.f);
		const auto P = [&](float X, float Y, float Z) { return At + R.RotateVector(FVector(X, Y, Z)); };
		F.B.K.Rod(Bamboo(), P(-Span * 0.45f, 0.f, 0.f), P(Span * 0.35f, 0.f, 0.f), 9.f);
		F.B.K.Rod(Bamboo(), P(0.f, -Span * 0.5f, 0.f), P(0.f, Span * 0.5f * (bCrashed ? 0.55f : 1.f), bCrashed ? 60.f : 0.f), 8.f);
		for (const float S : { -1.f, 1.f })
		{
			if (bCrashed && S > 0.f)
			{
				F.B.K.Rod(Bamboo(0.9f), P(0.f, Span * 0.27f, 30.f), P(-Span * 0.3f, Span * 0.42f, 80.f), 6.f);   // the broken wing, folded back
				F.B.K.Add(Sailcloth(0.85f), FVector(Span * 0.3f, Span * 0.18f, 2.f), P(-Span * 0.12f, Span * 0.3f, 50.f), R + FRotator(0.f, 30.f, 40.f));
				continue;
			}
			F.B.K.Add(Sailcloth(), FVector(Span * 0.42f, Span * 0.45f, 2.f), P(-Span * 0.08f, S * Span * 0.24f, 2.f), R + FRotator(0.f, S * 8.f, 0.f));
		}
		for (int32 p = 0; p < 5; ++p) F.B.K.Add(PrayerCloth(p + Seed), FVector(40.f, 28.f, 1.5f), P(-Span * 0.45f - 30.f - p * 42.f, 6.f * FMath::Sin(p * 1.3f), -6.f * p), R + FRotator(0.f, 0.f, p * 9.f));
	}
}
