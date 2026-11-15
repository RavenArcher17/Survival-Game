// The Desert's look kit (cooks/biome-desert.md 2.1-2.4, cooks/build-biome-desert.md 2.2): its palette (the cook's
// hexes) and the pieces every site shares, on the forest's FBuild (WRForestSceneryKit.h): Sunwatch sandstone and basalt
// (stelae, pylons, coursed walls, pillars carved with hours, braziers, mirrors on pivots), the old sea (whale skeletons,
// ancient hulls, sun-barques), the newcomers (ships, casks, tents, crosses), caravans and Dromad bones, and the living
// things (date palms, umbrella thorns, prickly pear, saltbush, sea-grass, reeds, hoodoos). Every free function is inline
// and named Ds... (unity builds join the scenery files with the other lands' kits).
#pragma once

#include "CoreMinimal.h"
#include "WRForestSceneryKit.h"

namespace WRDesertLook
{
	using namespace WRForestLook;
	using FSurf = WRLookKit::FSurf;

	inline FLinearColor DsRGB(uint8 R, uint8 G, uint8 B) { return FLinearColor(FColor(R, G, B)); }
	// ---- the palette (cook 2.1's hexes; linear, a little under the albedo so it sits in hard sun as stone does)
	inline FSurf DsSandstone(float K = 1.f) { return FSurf::Stone(DsRGB(0xC8, 0xA0, 0x70) * 0.56f * K, 0.9f); }       // Sunwatch dressed stone
	inline FSurf DsSandstoneRed(float K = 1.f) { return FSurf::Stone(DsRGB(0xB8, 0x6A, 0x48) * 0.52f * K, 0.9f); }
	inline FSurf DsRedRock(float K = 1.f) { return FSurf::Stone(DsRGB(0xA8, 0x46, 0x2E) * 0.55f * K, 0.88f); }        // canyon red
	inline FSurf DsVarnish(float K = 1.f) { return FSurf::Stone(DsRGB(0x3A, 0x2A, 0x2E) * 0.6f * K, 0.55f); }         // desert varnish streaks
	inline FSurf DsCaprock(float K = 1.f) { return FSurf::Stone(DsRGB(0xD8, 0xC0, 0x98) * 0.55f * K, 0.9f); }         // the hard pale cap on a hoodoo
	inline FSurf DsBasalt(float K = 1.f) { return FSurf::Stone(DsRGB(0x2A, 0x2A, 0x2E) * 0.75f * K, 0.62f); }         // the gnomon's black stone
	inline FSurf DsLimestone(float K = 1.f) { return FSurf::Stone(DsRGB(0xE4, 0xDC, 0xC8) * 0.66f * K, 0.75f); }      // the plaza's white
	inline FSurf DsMudbrick(float K = 1.f) { return FSurf::Stone(DsRGB(0xA8, 0x86, 0x5E) * 0.5f * K, 0.95f); }
	inline FSurf DsBronze() { return FSurf::Steel(DsRGB(0x9C, 0x74, 0x3E) * 0.62f, 0.38f); }
	inline FSurf DsBronzeDull() { return FSurf::Steel(DsRGB(0x6E, 0x52, 0x30) * 0.55f, 0.6f); }
	inline FSurf DsVerdigris() { return FSurf::Steel(DsRGB(0x4E, 0x8A, 0x72) * 0.5f, 0.75f); }
	inline FSurf DsGold() { return FSurf::Steel(DsRGB(0xE8, 0xB2, 0x30) * 0.8f, 0.22f); }                             // Sun gold #E8B230
	inline FSurf DsCedar(float K = 1.f) { return FSurf::Wood(DsRGB(0x9A, 0x58, 0x36) * 0.5f * K); }                   // sun-barque cedar
	inline FSurf DsSunBleach(float K = 1.f) { return FSurf::Wood(DsRGB(0xB8, 0xA8, 0x90) * 0.5f * K); }               // wood bleached in the sun
	inline FSurf DsTimber(float K = 1.f) { return FSurf::Wood(DsRGB(0x6E, 0x54, 0x3A) * 0.48f * K); }                 // newcomer ship timber
	inline FSurf DsTar() { return FSurf::Wood(DsRGB(0x2C, 0x24, 0x1C) * 0.55f); }
	inline FSurf DsRope() { return FSurf::Cloth(DsRGB(0x9C, 0x84, 0x60) * 0.5f); }
	inline FSurf DsLinen(float K = 1.f) { return FSurf::Cloth(DsRGB(0xE0, 0xD4, 0xB8) * 0.55f * K); }
	inline FSurf DsIndigo() { return FSurf::Cloth(DsRGB(0x2A, 0x34, 0x6E) * 0.55f); }                                 // tagelmust indigo
	inline FSurf DsHide(float K = 1.f) { return FSurf::Cloth(DsRGB(0x8A, 0x62, 0x3E) * 0.48f * K); }
	inline FSurf DsBone(float K = 1.f) { return FSurf::Stone(DsRGB(0xE8, 0xE0, 0xCC) * 0.7f * K, 0.72f); }
	inline FSurf DsSalt() { return FSurf::Stone(DsRGB(0xF2, 0xEF, 0xE6) * 0.8f, 0.6f); }
	inline FSurf DsClay(float K = 1.f) { return FSurf::Stone(DsRGB(0xA8, 0x6A, 0x46) * 0.5f * K, 0.85f); }
	inline FSurf DsOchre() { return FSurf::Stone(DsRGB(0xB8, 0x5A, 0x2E) * 0.55f, 0.9f); }
	inline FSurf DsFaience() { return FSurf::Glaze(DsRGB(0x3A, 0x8A, 0xB0) * 0.55f, 0.3f); }                           // Ushabti blue
	inline FSurf DsGlassBlack() { return FSurf::Glass(DsRGB(0x1E, 0x2A, 0x1E) * 0.6f, 0.f); }                           // fused canyon glass
	inline FSurf DsSunglass() { return FSurf::Glass(DsRGB(0xC8, 0xC0, 0x60) * 0.6f, 0.f); }                              // Libyan desert glass
	inline FSurf DsSunstone(float Glow) { return FSurf::Lit(DsRGB(0xF2, 0xA2, 0x3A) * 0.7f, Glow); }                     // sunstone amber #F2A23A
	inline FSurf DsEmber() { return FSurf::Lit(DsRGB(0xFF, 0x9A, 0x48), 6.f); }
	inline FSurf DsWater() { return FSurf::Glaze(DsRGB(0x2E, 0x6E, 0x70) * 0.45f, 0.04f); }                              // oasis turquoise
	inline FSurf DsSandFill(float K = 1.f) { return FSurf::Stone(DsRGB(0xD9, 0xA8, 0x5B) * 0.58f * K, 0.95f); }          // dune gold #D9A85B
	inline FSurf DsPaleSand(float K = 1.f) { return FSurf::Stone(DsRGB(0xE4, 0xD3, 0xAE) * 0.6f * K, 0.95f); }
	inline FLinearColor DsPalmTrunkC() { return DsRGB(0x7A, 0x62, 0x48) * 0.5f; }
	inline FLinearColor DsFrondC() { return DsRGB(0x5E, 0x7E, 0x3A) * 0.5f; }
	inline FLinearColor DsFrondDryC() { return DsRGB(0xA8, 0x90, 0x5A) * 0.5f; }
	inline FLinearColor DsDateC() { return DsRGB(0xC0, 0x5A, 0x22) * 0.6f; }
	inline FLinearColor DsAcaciaBarkC() { return DsRGB(0x4A, 0x3E, 0x34) * 0.55f; }
	inline FLinearColor DsAcaciaLeafC() { return DsRGB(0x6E, 0x86, 0x46) * 0.48f; }
	inline FLinearColor DsPearPadC() { return DsRGB(0x6E, 0x96, 0x58) * 0.5f; }
	inline FLinearColor DsPearFruitC() { return DsRGB(0xBE, 0x1E, 0x6E) * 0.6f; }
	inline FLinearColor DsBoneC() { return DsRGB(0xE8, 0xE0, 0xCC) * 0.7f; }
	inline FLinearColor DsBoneOldC() { return DsRGB(0xD8, 0xC8, 0xA8) * 0.62f; }
	inline FLinearColor DsDriftC() { return DsRGB(0xA8, 0xA3, 0x9A) * 0.5f; }
	inline FLinearColor DsRedRockC() { return DsRGB(0xA8, 0x46, 0x2E) * 0.55f; }
	inline FLinearColor DsRockTint() { return DsRGB(0xC0, 0x6A, 0x48); }

	// ---- a moving part: a pivot of its own (every machine turns it), built from blocks in its own space
	inline USceneComponent* DsPart(FBuild& B, const FVector& At, const FRotator& Rot, TArray<FWRLookBlock>& Blocks)
	{
		if (B.bServer) return nullptr;
		USceneComponent* Pivot = NewObject<USceneComponent>(B.Act);
		Pivot->SetupAttachment(B.Act->GetRootComponent());
		Pivot->SetRelativeLocation(At);
		Pivot->SetRelativeRotation(Rot);
		Pivot->RegisterComponent();
		FWRBuildLook::Spawn(B.Act, Pivot, Blocks, FLinearColor::Black, false);
		return Pivot;
	}

	/** Coursed sandstone along a straight run (a wall, a terrace edge, a cistern): blocks Bw x Bh staggered every other
	 *  course, from A to Bt, Height tall, Thick through. Broken > 0: the top courses gone in a ragged line (a ruin).
	 *  Collision one box (to the broken height). */
	inline void DsCoursedWall(FBuild& B, const FVector& A, const FVector& Bt, float Height, float Thick, const FSurf& S, float Broken = 0.f, float Bw = 110.f, float Bh = 45.f, bool bCollide = true)
	{
		const FVector D = Bt - A;
		const float L = D.Size2D();
		if (L < 10.f) return;
		const FVector Dir = FVector(D.X, D.Y, 0.f) / L;
		const FRotator R(0.f, FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)), 0.f);
		const int32 Courses = FMath::Max(1, FMath::RoundToInt(Height / Bh));
		float MinTop = Height;
		for (int32 c = 0; c < Courses; ++c)
		{
			const float Off = (c % 2) * Bw * 0.5f;
			for (float s = -Off; s < L; s += Bw)
			{
				const float s0 = FMath::Max(0.f, s), s1 = FMath::Min(L, s + Bw - 4.f);
				if (s1 - s0 < 8.f) continue;
				const float Mid = (s0 + s1) * 0.5f;
				// A ruin's top: a ragged line from noise along the run (higher at the ends, a breach in the middle).
				if (Broken > 0.f)
				{
					const float Keep = Height * (1.f - Broken * (0.35f + 0.65f * (0.5f + 0.5f * FMath::Sin(Mid * 0.011f + B.Rand(int32(Mid)) * 2.f))));
					if ((c + 1) * Bh > Keep) { MinTop = FMath::Min(MinTop, c * Bh); continue; }
				}
				const float Z = FMath::Lerp(A.Z, Bt.Z, Mid / L);
				const float Shade = 0.86f + 0.24f * B.Rand(c * 131 + int32(s));
				B.K.Add(S.Dim(Shade), FVector(s1 - s0, Thick - 5.f * B.Rand(c + int32(s) * 7), Bh - 3.f), A + Dir * Mid + FVector(0, 0, Z - A.Z + Bh * (c + 0.5f)), R);
			}
		}
		const float Top = Broken > 0.f ? FMath::Max(Bh, MinTop) : Height;
		if (bCollide) B.SegBox(A + FVector(0, 0, Top * 0.5f), Bt + FVector(0, 0, Top * 0.5f), Thick * 0.5f, Top * 0.5f);
	}

	/** A gnomon stela (the Sunwatch waymark, world-map KEPT): a 4 m dressed slab on a two-step footing, a bronze gnomon
	 *  blade on top throwing the hour onto hour lines carved down its face, a sun disc at the head. Lean degrees off
	 *  plumb (the tilted seven lean 4). Variant 1 is older: weathered, its gnomon gone. */
	inline void DsStela(FBuild& B, const FVector& Base, float Yaw, float Lean, int32 Variant, float H = 400.f)
	{
		const FRotator Ry(0.f, Yaw, 0.f);
		const FRotator Rl(Lean, Yaw, 0.f);
		const bool bOld = Variant % 2 == 1;
		const float K = bOld ? 0.85f : 1.f;
		B.K.Add(DsSandstone(0.8f * K), FVector(200.f, 120.f, 40.f), Base + FVector(0, 0, 0.f), Ry);
		B.K.Add(DsSandstone(0.9f * K), FVector(160.f, 90.f, 30.f), Base + FVector(0, 0, 35.f), Ry);
		const FVector Foot = Base + FVector(0, 0, 50.f);
		const FVector Up = Rl.RotateVector(FVector(0, 0, 1));
		B.K.Add(DsSandstone(K), FVector(120.f, 50.f, H), Foot + Up * (H * 0.5f), Rl);
		// The hour lines (darker grooves) fanning down the face from the gnomon's root, and the sun disc.
		for (int32 l = 0; l < 7; ++l)
		{
			const float A = -60.f + l * 20.f;
			const FVector From = Foot + Up * (H - 60.f) + Rl.RotateVector(FVector(0, 26.f, 0));
			const FVector Dir = Rl.RotateVector(FRotator(0.f, 0.f, A).RotateVector(FVector(0, 0, -1)));
			B.K.Rod(DsSandstone(0.55f * K), From + Rl.RotateVector(FVector(0, 0.f, 0)), From + FVector(Dir.X, Dir.Y, Dir.Z) * (H * 0.55f), 4.f, true);
		}
		B.K.Disc(bOld ? DsSandstone(0.7f) : DsBronze(), Foot + Up * (H - 30.f) + Rl.RotateVector(FVector(0, 26.f, 0)), Rl.RotateVector(FVector(0, 1, 0)), 46.f, 6.f);
		if (!bOld)
		{
			// The gnomon: a triangular bronze fin standing out from the top edge (its shadow tells the hour).
			for (int32 k = 0; k < 5; ++k) B.K.Add(DsBronze(), FVector(8.f, 50.f - k * 9.f, 14.f), Foot + Up * (H + 7.f + k * 14.f) + Rl.RotateVector(FVector(0, 25.f - k * 4.5f, 0)), Rl);
		}
		else
		{
			// A crack across it and a chunk off the top corner.
			B.K.Add(DsSandstone(0.5f), FVector(124.f, 8.f, 6.f), Foot + Up * (H * 0.62f), Rl + FRotator(0.f, 0.f, 9.f));
		}
		B.Box(Foot + Up * (H * 0.5f), FVector(60.f, 26.f, H * 0.5f), Rl);
	}

	/** A date palm (Phoenix dactylifera, research: 15-25 m, a trunk of diamond leaf-base scars, a crown of 20-30 arching
	 *  fronds 4-6 m, orange date bunches under them). Scale 1 = 16 m. The trunk leans and curves a little with the wind.
	 *  bDates: bunches hang (a fruiting palm). */
	inline void DsPalm(FBuild& B, const FVector& Base, float Yaw, float Scale, int32 Seed, bool bDates = true, float Collide = 1.f)
	{
		const float H = 1600.f * Scale;
		const FVector Lean = FRotator(-6.f - 6.f * B.Rand(Seed), Yaw, 0.f).RotateVector(FVector(0, 0, 1));
		TArray<FVector> Spine;
		for (int32 i = 0; i <= 8; ++i)
		{
			const float T = i / 8.f;
			// A gentle S: out with the lean, back up near the crown.
			const FVector Off = (FVector(Lean.X, Lean.Y, 0.f) * H * (0.35f * T - 0.12f * T * T));
			Spine.Add(Base + FVector(0, 0, T * H) + Off - FVector(0, 0, i == 0 ? 40.f : 0.f));
		}
		B.M.Tube(Spine, 22.f * Scale + 6.f, 8, DsPalmTrunkC(), 0.92f);
		// The leaf-base scars: rings of stubs spiralling up the trunk (a diamond pattern's ridges).
		for (int32 r = 1; r < 26; ++r)
		{
			const float T = r / 26.f;
			const int32 Si = FMath::Min(7, int32(T * 8.f));
			const FVector P = FMath::Lerp(Spine[Si], Spine[Si + 1], T * 8.f - Si);
			for (int32 s = 0; s < 4; ++s)
			{
				const float A = (r * 45.f + s * 90.f) * PI / 180.f;
				const FVector Dir(FMath::Cos(A), FMath::Sin(A), 0.25f);
				B.Limb(P, P + Dir * (12.f + 4.f * Scale), 9.f * Scale + 3.f, 3.f, DsPalmTrunkC() * 1.15f, 4);
			}
		}
		const FVector Crown = Spine.Last();
		// Fronds: 22 arching rachises with leaflets as narrow blades in two ranks; the lowest few dry and hanging.
		const int32 NF = 22;
		for (int32 f = 0; f < NF; ++f)
		{
			const float Az = (f * 137.5f + Seed * 31.f) * PI / 180.f;
			const bool bDry = f < 4;
			const float Rise = bDry ? -70.f : FMath::Lerp(55.f, -10.f, float(f % 7) / 6.f);
			const float Len = (450.f + 120.f * B.Rand(Seed * 7 + f)) * Scale;
			TArray<FVector> Rach;
			for (int32 k = 0; k <= 5; ++k)
			{
				const float T = k / 5.f;
				const float El = FMath::DegreesToRadians(Rise - 85.f * T * T);
				Rach.Add(Crown + FVector(FMath::Cos(Az) * FMath::Cos(El), FMath::Sin(Az) * FMath::Cos(El), 0.f) * Len * T + FVector(0, 0, Len * T * FMath::Sin(FMath::DegreesToRadians(Rise)) * (1.f - T) - 40.f * T * T * Scale * 4.f));
			}
			const FLinearColor Fc = bDry ? DsFrondDryC() : DsFrondC() * (0.9f + 0.2f * B.Rand(f + Seed));
			B.M.Tube(Rach, 4.f * Scale + 1.f, 4, Fc, 0.8f);
			const FVector Side = FVector::CrossProduct((Rach[5] - Rach[0]).GetSafeNormal(), FVector::UpVector).GetSafeNormal();
			for (int32 k = 1; k < 5; ++k)
			{
				for (int32 sd = -1; sd <= 1; sd += 2)
				{
					const FVector P = FMath::Lerp(Rach[k], Rach[k + 1], 0.5f);
					const FVector Tip = P + (Side * sd * 0.85f + FVector(0, 0, -0.35f)).GetSafeNormal() * (80.f - k * 10.f) * Scale;
					B.Limb(P, Tip, 3.5f * Scale + 0.6f, 0.4f, Fc * 1.05f, 3);
				}
			}
		}
		if (bDates)
		{
			for (int32 d = 0; d < 4; ++d)
			{
				const float Az = (d * 90.f + 40.f + Seed * 13.f) * PI / 180.f;
				const FVector Hang = Crown + FVector(FMath::Cos(Az), FMath::Sin(Az), 0.f) * 50.f * Scale - FVector(0, 0, 60.f * Scale);
				B.Limb(Crown - FVector(0, 0, 10.f), Hang, 3.f, 2.f, DsRGB(0xD8, 0xA0, 0x40) * 0.5f, 4);
				B.Ball(Hang - FVector(0, 0, 40.f * Scale), 34.f * Scale, DsDateC(), 0.6f, 1.6f);
			}
		}
		if (Collide > 0.f) B.Box(Base + FVector(0, 0, 300.f), FVector(24.f * Scale + 8.f, 24.f * Scale + 8.f, 300.f));
	}

	/** An umbrella thorn (Vachellia tortilis, research: 4-15 m, a flat umbrella crown, twisting trunks, white thorns).
	 *  Scale 1 = 8 m. Two or three forked trunks lean out from one root, their limbs carrying a flat, layered crown. */
	inline void DsAcacia(FBuild& B, const FVector& Base, float Yaw, float Scale, int32 Seed)
	{
		const float H = 800.f * Scale;
		const int32 NT = 2 + (Seed % 2);
		TArray<FVector> Tops;
		for (int32 t = 0; t < NT; ++t)
		{
			const FRotator Out(-20.f - 14.f * B.Rand(Seed + t), Yaw + t * (360.f / NT) + 20.f * B.Rand(Seed * 3 + t), 0.f);
			const FVector Mid = Base + FVector(0, 0, H * 0.35f) + FVector(Out.Vector().X, Out.Vector().Y, 0.f) * H * 0.18f;
			B.Limb(Base - FVector(0, 0, 30.f), Mid, 20.f * Scale + 4.f, 14.f * Scale + 3.f, DsAcaciaBarkC(), 7);
			for (int32 k = 0; k < 3; ++k)
			{
				const FRotator Br(-28.f - 10.f * k, Out.Yaw + (k - 1) * 40.f, 0.f);
				const FVector Tip = Mid + Br.Vector() * H * 0.12f + FVector(0, 0, H * (0.42f + 0.06f * k)) + FVector(Br.Vector().X, Br.Vector().Y, 0.f) * H * 0.22f;
				B.Limb(Mid, Tip, 12.f * Scale + 2.f, 5.f * Scale + 1.f, DsAcaciaBarkC() * 1.08f, 6);
				Tops.Add(Tip);
			}
		}
		// The crown: flat discs of foliage (squashed balls) over the limb tips, one wide layer, a few smaller above.
		for (int32 i = 0; i < Tops.Num(); ++i)
		{
			B.Ball(Tops[i] + FVector(0, 0, 30.f * Scale), 260.f * Scale + 20.f, DsAcaciaLeafC() * (0.9f + 0.2f * B.Rand(i + Seed)), 0.95f, 0.22f);
			if (i % 2 == 0) B.Ball(Tops[i] + FVector(40.f, -30.f, 110.f * Scale), 170.f * Scale + 10.f, DsAcaciaLeafC() * 1.08f, 0.95f, 0.25f);
		}
		B.Box(Base + FVector(0, 0, H * 0.2f), FVector(26.f * Scale + 6.f, 26.f * Scale + 6.f, H * 0.2f));
	}

	/** A prickly pear (Opuntia, survival-crafting KEPT): flat oval pads growing out of each other, magenta fruit on the
	 *  top rims, yellow flowers now and then. Scale 1 = 1.6 m. */
	inline void DsPear(FBuild& B, const FVector& Base, float Yaw, float Scale, int32 Seed)
	{
		struct FPad { FVector At; float Yaw; float Tilt; int32 Gen; };
		TArray<FPad> Pads;
		Pads.Add({ Base + FVector(0, 0, 30.f * Scale), Yaw, 0.f, 0 });
		for (int32 i = 0; i < 9 && i < Pads.Num() + 6; ++i)
		{
			if (!Pads.IsValidIndex(i)) break;
			const FPad P = Pads[i];
			if (P.Gen >= 2) continue;
			for (int32 c = 0; c < 2; ++c)
			{
				const float Sd = c ? 1.f : -1.f;
				const FRotator R(P.Tilt + Sd * (20.f + 15.f * B.Rand(Seed + i * 5 + c)), P.Yaw + Sd * 30.f, 0.f);
				Pads.Add({ P.At + R.RotateVector(FVector(0, 0, 50.f * Scale)), P.Yaw + 60.f * B.Rand(i + c), float(R.Pitch) * 0.6f, P.Gen + 1 });
			}
		}
		for (int32 i = 0; i < Pads.Num(); ++i)
		{
			const FPad& P = Pads[i];
			const FRotator R(P.Tilt, P.Yaw, 0.f);
			B.M.Lathe(P.At - R.RotateVector(FVector(0, 0, 28.f * Scale)), R.RotateVector(FVector(0, 0, 1)), 56.f * Scale,
				{ FVector(0.f, 4.f, 2.f), FVector(0.3f, 22.f * Scale, 5.f * Scale), FVector(0.7f, 22.f * Scale, 5.f * Scale), FVector(1.f, 4.f, 2.f) }, 8, DsPearPadC() * (0.9f + 0.2f * B.Rand(i)), 0.7f);
			if (P.Gen == 2 && B.Rand(Seed + i) > 0.35f)
			{
				for (int32 f = 0; f < 3; ++f) B.Ball(P.At + R.RotateVector(FVector((f - 1) * 12.f * Scale, 0.f, 30.f * Scale)), 6.f * Scale + 1.f, f == 1 && B.Rand(i * 3) > 0.7f ? DsRGB(0xE8, 0xC8, 0x40) * 0.7f : DsPearFruitC(), 0.6f, 1.2f);
			}
		}
	}

	/** A tuft of sea-grass on the strand's dunes (marram-like blades leaning with the east wind). */
	inline void DsGrassTuft(FBuild& B, const FVector& P, float Scale, int32 n)
	{
		for (int32 b = 0; b < 7; ++b) B.K.Spike(FSurf::Cloth(DsRGB(0x8F, 0xA0, 0x5A) * (0.42f + 0.1f * B.Rand(n * 7 + b))), P + FVector((b - 3) * 4.f, (b % 2) * 4.f, -2.f), FVector(-0.3f + 0.1f * (b - 3), 0.08f * (b % 3), 1.f), 3.f, 50.f * Scale + 12.f * B.Rand(b + n));
	}

	/** A hoodoo (research: a spire of soft red sandstone protected by a harder pale caprock): stacked drums narrowing to a
	 *  neck under the cap, Height tall. Collision a column. */
	inline void DsHoodoo(FBuild& B, const FVector& Base, float Height, float R, int32 Seed, bool bCollide = true)
	{
		const int32 Drums = FMath::Max(3, int32(Height / 380.f));
		float Z = -60.f;
		for (int32 d = 0; d < Drums; ++d)
		{
			const float T = float(d) / Drums;
			const float Rr = R * (1.f - 0.45f * T) * (0.85f + 0.3f * B.Rand(Seed * 11 + d)) * (d == Drums - 1 ? 0.6f : 1.f);
			const float Hh = Height / Drums;
			B.K.Add(d % 3 == 1 ? DsSandstoneRed(0.95f) : DsRedRock(0.9f + 0.2f * B.Rand(d + Seed)), FVector(Rr * 2.f, Rr * 2.f * (0.85f + 0.15f * B.Rand(d)), Hh + 10.f), Base + FVector(10.f * (B.Rand(d * 3 + Seed) - 0.5f), 10.f * (B.Rand(d * 7 + Seed) - 0.5f), Z + Hh * 0.5f), FRotator(0.f, Seed * 37.f + d * 23.f, 0.f), EWRLookMesh::Cylinder);
			// A varnish streak down the drum's lee side.
			if (d % 2 == 0) B.K.Add(DsVarnish(), FVector(Rr * 0.6f, 6.f, Hh * 0.9f), Base + FVector(Rr * 0.92f, 0, Z + Hh * 0.5f), FRotator(0.f, Seed * 37.f + 180.f, 0.f));
			Z += Hh;
		}
		B.K.Add(DsCaprock(), FVector(R * 1.5f, R * 1.3f, 90.f), Base + FVector(0, 0, Z + 30.f), FRotator(4.f, Seed * 53.f, 0.f), EWRLookMesh::Cylinder);
		if (bCollide) B.Box(Base + FVector(0, 0, Height * 0.5f), FVector(R * 0.8f, R * 0.8f, Height * 0.5f + 40.f));
	}

	/** A whale skeleton on the old sea floor (research: Wadi Al-Hitan), Len long, half in the sand along Yaw: the skull
	 *  (rostrum, braincase), a line of vertebrae as discs half buried, the ribs as arcs rising from the sand, a flipper's
	 *  bones. Bury 0..1: how deep the sand has it. */
	inline void DsWhale(FBuild& B, const FVector& Base, float Yaw, float Len, int32 Seed, float Bury = 0.5f)
	{
		const FRotator R(0.f, Yaw, 0.f);
		const FVector Fw = R.Vector(), Side = FRotator(0.f, Yaw + 90.f, 0.f).Vector();
		const float Sink = 40.f + 120.f * Bury;
		// The skull: a long rostrum tapering forward, the braincase a dome behind it.
		const FVector Head = Base + Fw * Len * 0.45f;
		B.M.Lathe(Head - FVector(0, 0, Sink * 0.6f), Fw, Len * 0.2f, { FVector(0.f, Len * 0.05f, Len * 0.035f), FVector(0.5f, Len * 0.04f, Len * 0.025f), FVector(1.f, Len * 0.012f, Len * 0.01f) }, 10, DsBoneC(), 0.75f);
		B.Ball(Head - Fw * Len * 0.03f + FVector(0, 0, Len * 0.01f - Sink * 0.4f), Len * 0.06f, DsBoneC() * 0.95f, 0.75f, 0.7f);
		// The jaw bones lying apart on the sand like two curved beams.
		for (int32 sd = -1; sd <= 1; sd += 2) B.Limb(Head + Side * sd * Len * 0.05f - FVector(0, 0, Sink * 0.7f), Head + Fw * Len * 0.22f + Side * sd * Len * 0.09f - FVector(0, 0, Sink * 0.7f), Len * 0.012f, Len * 0.006f, DsBoneOldC(), 6);
		// The vertebrae: discs from the skull back to the tail, smaller toward the tail, half buried.
		const int32 NV = 34;
		for (int32 v = 0; v < NV; ++v)
		{
			const float T = float(v) / NV;
			const FVector P = Head - Fw * (Len * 0.06f + Len * 0.85f * T) + Side * FMath::Sin(T * 3.f + Seed) * Len * 0.03f;
			const float Rd = Len * (0.022f - 0.016f * T);
			B.K.Disc(DsBone(0.9f + 0.1f * B.Rand(v + Seed)), P - FVector(0, 0, Sink * 0.6f - Rd * 0.6f), Fw, Rd * 2.f, Rd * 0.9f);
			if (v % 2 == 0) B.K.Spike(DsBone(0.85f), P + FVector(0, 0, Rd * 0.6f - Sink * 0.6f), FVector(0, 0, 1), Rd * 0.4f, Rd * 1.6f);
		}
		// The ribs: arcs from the spine's sides curving up and out, then down into the sand (the long ones first).
		for (int32 r = 0; r < 14; ++r)
		{
			const float T = 0.06f + r * 0.028f;
			const FVector P = Head - Fw * (Len * 0.08f + Len * 0.85f * T) - FVector(0, 0, Sink * 0.6f);
			const float Rl = Len * (0.13f - 0.006f * r) * (0.85f + 0.3f * B.Rand(Seed + r));
			for (int32 sd = -1; sd <= 1; sd += 2)
			{
				if (B.Rand(Seed * 13 + r * 2 + sd) < 0.18f) continue;          // some taken by the sand or broken
				TArray<FVector> Arc;
				for (int32 k = 0; k <= 6; ++k)
				{
					const float A = k / 6.f * 1.9f;
					Arc.Add(P + Side * sd * FMath::Sin(A) * Rl * 0.75f + FVector(0, 0, FMath::Cos(A) * Rl * 0.55f + Rl * 0.15f) - Fw * k * 6.f);
				}
				B.M.Tube(Arc, Len * 0.005f + 4.f, 6, DsBoneC() * (0.92f + 0.1f * B.Rand(r)), 0.78f);
			}
		}
		// A flipper's bones fanned out on the sand.
		const FVector Fl = Head - Fw * Len * 0.18f + Side * Len * 0.12f - FVector(0, 0, Sink * 0.5f);
		for (int32 d = 0; d < 5; ++d) B.Limb(Fl, Fl + FRotator(0.f, Yaw + 60.f + d * 12.f, 0.f).Vector() * Len * (0.05f + 0.01f * d), Len * 0.006f, Len * 0.003f, DsBoneOldC(), 5);
		B.Box(Head + FVector(0, 0, Len * 0.02f - Sink * 0.4f), FVector(Len * 0.12f, Len * 0.06f, Len * 0.04f), R);
	}

	/** A hull. Kind 0: a newcomer's carvel-planked ship (tarred, masts and rigging); 1: a Sunwatch sun-barque (cedar, high
	 *  curved prow and stern posts ending in papyrus-bud finials, a deckhouse); 2: the old sea people's oared ship (oar
	 *  ports, a ram, a figurehead). Len x Beam, Depth deep, heeled Heel degrees, buried Bury cm below the sand line. Its
	 *  frames show where planks have gone (Broken 0..1). Collision: the hull's sides and deck. */
	inline void DsHull(FBuild& B, const FVector& Base, const FRotator& R, float Len, float Beam, float Depth, int32 Kind, float Bury, float Broken, int32 Seed)
	{
		const FLinearColor Wood = Kind == 1 ? DsRGB(0x9A, 0x58, 0x36) * 0.5f : Kind == 2 ? DsRGB(0x7A, 0x5A, 0x3E) * 0.48f : DsRGB(0x3A, 0x30, 0x26) * 0.55f;
		const FVector O = Base - FVector(0, 0, Bury);
		// The hull: lofted rings, a U section that narrows to the stem and stern and rises at both ends (sheer).
		TArray<TArray<FVector>> Rings;
		const int32 NR = 12;
		for (int32 i = 0; i <= NR; ++i)
		{
			const float T = float(i) / NR;                        // 0 stern .. 1 stem
			const float W = Beam * 0.5f * FMath::Pow(FMath::Sin(PI * FMath::Clamp(0.05f + 0.9f * T, 0.f, 1.f)), Kind == 1 ? 0.8f : 0.6f) + 6.f;
			const float Sheer = Depth * (Kind == 1 ? 0.6f : 0.25f) * (FMath::Square(2.f * T - 1.f));
			const float D = Depth * (0.55f + 0.45f * FMath::Sin(PI * T));
			TArray<FVector>& Ring = Rings.AddDefaulted_GetRef();
			for (int32 s = 0; s <= 8; ++s)
			{
				const float A = PI * s / 8.f;                        // 0 = port gunwale, pi = starboard gunwale
				const float Y = -FMath::Cos(A) * W;
				const float Z = Depth + Sheer - FMath::Sin(A) * D;
				Ring.Add(O + R.RotateVector(FVector((T - 0.5f) * Len, Y, Z)));
			}
		}
		if (Broken < 0.95f) B.M.Loft(Rings, Wood, 0.85f, false);
		// The frames (ribs) where the planks are gone, the gunwale rail, the keel.
		for (int32 i = 1; i < NR; ++i)
		{
			if (B.Rand(Seed + i) > 0.35f + Broken * 0.6f) continue;
			B.M.Tube(Rings[i], 7.f, 5, Wood * 1.2f, 0.85f);
		}
		TArray<FVector> Port, Star, Keel;
		for (const TArray<FVector>& Rg : Rings) { Port.Add(Rg[0]); Star.Add(Rg[8]); Keel.Add(Rg[4]); }
		B.M.Tube(Port, 9.f, 5, Wood * 0.9f, 0.8f);
		B.M.Tube(Star, 9.f, 5, Wood * 0.9f, 0.8f);
		B.M.Tube(Keel, 14.f, 5, Wood * 0.8f, 0.8f);
		const FVector Stem = Rings.Last()[4], Stern = Rings[0][4];
		if (Kind == 1)
		{
			// The sun-barque's posts: rising and curling inward, papyrus-bud finials, a sun disc nailed to the prow post.
			for (int32 e = 0; e < 2; ++e)
			{
				const FVector Root = e ? Stem : Stern;
				const float Sg = e ? 1.f : -1.f;
				TArray<FVector> Post;
				for (int32 k = 0; k <= 6; ++k) { const float A = k / 6.f * 1.6f; Post.Add(Root + R.RotateVector(FVector(Sg * FMath::Sin(A) * Depth * 0.9f, 0.f, (1.f - FMath::Cos(A)) * Depth * 1.6f + Depth * 0.8f))); }
				B.M.Tube(Post, 16.f, 6, Wood * 1.1f, 0.8f);
				B.Ball(Post.Last(), 30.f, DsRGB(0x9A, 0x58, 0x36) * 0.55f, 0.8f, 1.4f);
			}
			B.K.Disc(DsGold(), Stem + R.RotateVector(FVector(Depth * 0.4f, 0.f, Depth * 1.6f)), R.Vector(), 70.f, 6.f);
			B.K.Add(DsCedar(0.8f), FVector(Len * 0.18f, Beam * 0.55f, Depth * 0.7f), O + R.RotateVector(FVector(-Len * 0.12f, 0.f, Depth + Depth * 0.35f)), R);
		}
		else if (Kind == 2)
		{
			// Oar ports along both sides, a bronze ram at the waterline, the sea god's figurehead on the stem.
			for (int32 p = 0; p < 10; ++p) for (int32 sd = -1; sd <= 1; sd += 2)
			{
				const int32 i = 2 + p * (NR - 4) / 10;
				const FVector Pp = FMath::Lerp(Rings[i][sd < 0 ? 1 : 7], Rings[i][sd < 0 ? 0 : 8], 0.5f);
				B.K.Disc(DsTar(), Pp, R.RotateVector(FVector(0, float(sd), 0)), 22.f, 6.f);
			}
			B.K.Add(DsVerdigris(), FVector(Len * 0.08f, 30.f, 40.f), Stem + R.RotateVector(FVector(Len * 0.04f, 0.f, Depth * 0.25f)), R, EWRLookMesh::Cone);
			B.Ball(Stem + R.RotateVector(FVector(Depth * 0.15f, 0.f, Depth * 1.25f)), 40.f, Wood * 1.3f, 0.8f, 1.3f);
		}
		else
		{
			// A newcomer's ship: tarred planks with a pale strake, masts and their stays.
			const int32 Masts = Len > 2500.f ? 3 : 1;
			for (int32 m = 0; m < Masts; ++m)
			{
				const FVector Step = O + R.RotateVector(FVector(Len * (Masts == 1 ? 0.05f : -0.28f + m * 0.28f), 0.f, Depth * 0.3f));
				const float MH = Len * (Masts == 1 ? 0.6f : 0.75f - 0.1f * FMath::Abs(m - 1));
				const FVector Top = Step + R.RotateVector(FVector(0, 0, MH));
				if (B.Rand(Seed + m * 5) < 0.75f || m == 0)
				{
					B.Limb(Step, Top, 20.f, 11.f, DsRGB(0x6E, 0x54, 0x3A) * 0.5f, 7);
					B.K.Rod(DsTimber(0.8f), Top - R.RotateVector(FVector(0, 0, MH * 0.3f)) + R.RotateVector(FVector(0, -Beam * 0.6f, 0)), Top - R.RotateVector(FVector(0, 0, MH * 0.3f)) + R.RotateVector(FVector(0, Beam * 0.6f, 0)), 10.f);
					for (int32 sd = -1; sd <= 1; sd += 2) B.Line(Top - R.RotateVector(FVector(0, 0, 40.f)), Rings[FMath::Clamp(4 + m * 3 - 3, 0, NR)][sd < 0 ? 0 : 8], 1.5f, 30.f, DsRGB(0x9C, 0x84, 0x60) * 0.45f, 6);
				}
				else B.Limb(Step, Step + R.RotateVector(FVector(0, 0, MH * 0.3f)), 20.f, 16.f, DsRGB(0x6E, 0x54, 0x3A) * 0.5f, 7);   // snapped
			}
			B.M.Tube(TArray<FVector>{ Rings[1][1], Rings[NR / 2][1], Rings[NR - 1][1] }, 6.f, 4, DsRGB(0xB8, 0xA8, 0x90) * 0.5f, 0.8f);
		}
		// Collision: the two sides and a deck at the gunwale's height minus a little.
		for (int32 sd = -1; sd <= 1; sd += 2) B.SegBox(Rings[2][sd < 0 ? 1 : 7], Rings[NR - 2][sd < 0 ? 1 : 7], 20.f, Depth * 0.45f);
		B.Box(O + R.RotateVector(FVector(0, 0, Depth * 0.6f)), FVector(Len * 0.38f, Beam * 0.38f, 15.f), R);
	}

	/** A Dromad's bones (a camel's: long leg bones, the arched neck's vertebrae, a skull) where it fell, its pack (a
	 *  hide-wrapped bale on a frame) still on it when bPack. */
	inline void DsDromadBones(FBuild& B, const FVector& Base, float Yaw, bool bPack, int32 Seed, float Sink = 30.f)
	{
		const FRotator R(0.f, Yaw, 0.f);
		const FVector Fw = R.Vector(), Side = FRotator(0.f, Yaw + 90.f, 0.f).Vector();
		const FVector O = Base - FVector(0, 0, Sink);
		for (int32 r = 0; r < 8; ++r)
		{
			const FVector P = O - Fw * (r * 22.f - 60.f);
			for (int32 sd = -1; sd <= 1; sd += 2) { TArray<FVector> Arc; for (int32 k = 0; k <= 4; ++k) { const float A = k / 4.f * 1.6f; Arc.Add(P + Side * sd * FMath::Sin(A) * 55.f + FVector(0, 0, FMath::Cos(A) * 45.f + 20.f)); } B.M.Tube(Arc, 3.f, 4, DsBoneC(), 0.8f); }
		}
		TArray<FVector> Neck;
		for (int32 k = 0; k <= 6; ++k) Neck.Add(O + Fw * (90.f + k * 26.f) + FVector(0, 0, 15.f + 40.f * FMath::Sin(k / 6.f * 2.4f)));
		for (int32 k = 0; k <= 6; ++k) B.Ball(Neck[k], 9.f, DsBoneC(), 0.8f, 0.8f);
		B.M.Lathe(Neck.Last(), (Fw + FVector(0, 0, -0.3f)).GetSafeNormal(), 55.f, { FVector(0.f, 13.f, 15.f), FVector(0.6f, 10.f, 11.f), FVector(1.f, 4.f, 5.f) }, 8, DsBoneC(), 0.75f);
		for (int32 l = 0; l < 4; ++l) B.Limb(O + Fw * (l < 2 ? 60.f : -100.f) + Side * (l % 2 ? 30.f : -30.f), O + Fw * (l < 2 ? 140.f : -40.f) + Side * (l % 2 ? 80.f : -90.f), 5.f, 4.f, DsBoneOldC(), 5);
		if (bPack)
		{
			B.K.Add(DsHide(0.9f), FVector(120.f, 140.f, 70.f), O + FVector(0, 0, 80.f), R + FRotator(0.f, 0.f, 12.f));
			B.K.Rod(DsSunBleach(0.8f), O + Side * -80.f + FVector(0, 0, 40.f), O + Side * 80.f + FVector(0, 0, 120.f), 6.f);
			B.K.Add(DsLinen(0.8f), FVector(90.f, 60.f, 50.f), O + Side * 70.f + FVector(0, 0, 40.f), R);
		}
	}

	/** A Sunwatch brazier: a bronze bowl on a stone drum, coals in it; lit with a fire when bLit (and a night lamp). */
	inline void DsBrazier(FBuild& B, const FVector& Base, float Scale, bool bLit)
	{
		B.K.Add(DsSandstone(0.9f), FVector(110.f, 110.f, 70.f) * Scale, Base + FVector(0, 0, 35.f * Scale), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		B.K.Add(DsSandstone(1.f), FVector(80.f, 80.f, 40.f) * Scale, Base + FVector(0, 0, 90.f * Scale), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		B.K.Add(DsBronze(), FVector(150.f, 150.f, 40.f) * Scale, Base + FVector(0, 0, 125.f * Scale), FRotator(180.f, 0.f, 0.f), EWRLookMesh::Cone);
		B.K.Ring(DsBronze(), Base + FVector(0, 0, 140.f * Scale), 75.f * Scale, 8.f * Scale, 10.f * Scale, 14);
		if (bLit)
		{
			B.K.Add(DsEmber(), FVector(110.f, 110.f, 10.f) * Scale, Base + FVector(0, 0, 138.f * Scale), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			B.Loop(TEXT("brazier.fire"), Base + FVector(0, 0, 145.f * Scale), FRotator::ZeroRotator, Scale);
			B.Lamp(Base + FVector(0, 0, 220.f * Scale), DsRGB(0xFF, 0xA8, 0x58), 2400.f * Scale, 1800.f * Scale);
		}
		B.Box(Base + FVector(0, 0, 70.f * Scale), FVector(55.f, 55.f, 70.f) * Scale);
	}

	/** A pylon tower (research: Karnak): battered walls (they lean in 6 degrees), torus mouldings up the corners, a cavetto
	 *  cornice, a winged sun disc carved high on the face. W x D at the foot, H tall; Broken > 0 takes its top courses. */
	inline void DsPylon(FBuild& B, const FVector& Base, float Yaw, float W, float D, float H, float Broken, int32 Seed)
	{
		const FRotator R(0.f, Yaw, 0.f);
		const float Batter = FMath::Tan(FMath::DegreesToRadians(6.f));
		const int32 Courses = int32(H / 80.f);
		const int32 Keep = int32(Courses * (1.f - Broken));
		for (int32 c = 0; c < Keep; ++c)
		{
			const float Z = c * 80.f + 40.f;
			const float Iw = W - 2.f * Batter * Z, Id = D - 2.f * Batter * Z;
			B.K.Add(DsSandstone(0.9f + 0.12f * B.Rand(Seed + c)), FVector(Id, Iw, 76.f), Base + FVector(0, 0, Z), R);
		}
		const float TopZ = Keep * 80.f;
		const float Tw = W - 2.f * Batter * TopZ, Td = D - 2.f * Batter * TopZ;
		// The torus mouldings (round rolls up the four corners) and the cornice's cavetto (a flared lip) when it stands whole.
		for (int32 cx = -1; cx <= 1; cx += 2) for (int32 cy = -1; cy <= 1; cy += 2)
		{
			const FVector A = Base + R.RotateVector(FVector(cx * D * 0.5f, cy * W * 0.5f, 0.f));
			const FVector Bt = Base + R.RotateVector(FVector(cx * Td * 0.5f, cy * Tw * 0.5f, TopZ));
			B.K.Rod(DsSandstone(1.05f), A, Bt, 34.f);
		}
		if (Broken <= 0.05f)
		{
			B.K.Add(DsSandstone(1.05f), FVector(Td + 120.f, Tw + 120.f, 50.f), Base + FVector(0, 0, TopZ + 25.f), R);
			B.K.Add(DsSandstone(0.95f), FVector(Td + 60.f, Tw + 60.f, 40.f), Base + FVector(0, 0, TopZ - 10.f), R);
		}
		// The winged sun disc on the outer face (the -X side), high up: a disc and two long feathered wings.
		const float DiscZ = FMath::Min(TopZ - 200.f, H * 0.78f);
		if (DiscZ > 200.f)
		{
			const FVector Face = Base + R.RotateVector(FVector(-(D * 0.5f - Batter * DiscZ) - 6.f, 0.f, DiscZ));
			B.K.Disc(DsSandstone(1.15f), Face, R.RotateVector(FVector(-1, 0, 0)), 150.f, 10.f);
			for (int32 sd = -1; sd <= 1; sd += 2) for (int32 f = 0; f < 5; ++f)
				B.K.Add(DsSandstone(1.1f - 0.04f * f), FVector(8.f, 260.f - f * 30.f, 22.f), Face + R.RotateVector(FVector(0.f, sd * (190.f + f * 6.f), 30.f - f * 24.f)), R + FRotator(0.f, 0.f, sd * (8.f + f * 3.f)));
		}
		B.Box(Base + FVector(0, 0, TopZ * 0.5f), FVector(D * 0.5f - Batter * TopZ * 0.5f, W * 0.5f - Batter * TopZ * 0.5f, TopZ * 0.5f), R);
	}

	/** A colossus's head (the Fallen Hour, the Workshop of Faces): a king's face worn by sand (no nose, a mouth line), a
	 *  striped nemes cloth falling to the shoulders, Size tall. On its side (bFallen) the face looks along +Y. bBroken: the
	 *  face's left half sheared off. */
	inline void DsColossusHead(FBuild& B, const FVector& Base, float Yaw, float Size, bool bFallen, bool bBroken, int32 Seed)
	{
		const FRotator R = bFallen ? FRotator(0.f, Yaw, 90.f) : FRotator(0.f, Yaw, 0.f);
		const FVector Up = R.RotateVector(FVector(0, 0, 1)), Fw = R.RotateVector(FVector(1, 0, 0)), Sd = R.RotateVector(FVector(0, 1, 0));
		const FVector C = Base + (bFallen ? FVector(0, 0, Size * 0.32f) : Up * Size * 0.5f);
		// The skull and face: a lathe along Up (round at the back, flatter at the face).
		B.M.Lathe(C - Up * Size * 0.5f, Up, Size, { FVector(0.f, Size * 0.28f, Size * 0.3f), FVector(0.25f, Size * 0.36f, Size * 0.38f), FVector(0.6f, Size * 0.36f, Size * 0.34f), FVector(0.9f, Size * 0.26f, Size * 0.24f), FVector(1.f, Size * 0.1f, Size * 0.1f) }, 14, DsRGB(0xC8, 0xA0, 0x70) * 0.55f, 0.88f);
		// The nemes: lappets falling either side of the face, striped (alternate tones), and its brow band.
		for (int32 s = -1; s <= 1; s += 2)
		{
			for (int32 st = 0; st < 6; ++st) B.K.Add(st % 2 ? DsSandstone(1.08f) : DsSandstone(0.82f), FVector(Size * 0.12f, Size * 0.07f, Size * 0.09f), C + Sd * s * Size * 0.4f + Fw * Size * 0.12f - Up * (st * Size * 0.09f + Size * 0.05f), R);
		}
		B.K.Add(DsSandstone(1.0f), FVector(Size * 0.3f, Size * 0.74f, Size * 0.06f), C + Fw * Size * 0.12f + Up * Size * 0.18f, R);
		// The face: brow ridge, the empty eye sockets, the mouth's worn line; the nose long gone.
		B.K.Add(DsSandstone(0.95f), FVector(Size * 0.08f, Size * 0.5f, Size * 0.06f), C + Fw * Size * 0.33f + Up * Size * 0.08f, R);
		for (int32 s = -1; s <= 1; s += 2) B.K.Disc(DsSandstone(0.45f), C + Fw * Size * 0.34f + Sd * s * Size * 0.12f + Up * Size * 0.02f, Fw, Size * 0.09f, Size * 0.02f);
		B.K.Add(DsSandstone(0.6f), FVector(Size * 0.03f, Size * 0.2f, Size * 0.015f), C + Fw * Size * 0.35f - Up * Size * 0.18f, R);
		if (bBroken) B.K.Add(DsSandstone(0.7f), FVector(Size * 0.4f, Size * 0.3f, Size * 0.6f), C + Fw * Size * 0.2f - Sd * Size * 0.32f, R + FRotator(10.f, 0.f, 0.f));
		B.Box(C, FVector(Size * 0.36f, Size * 0.36f, Size * 0.48f), R);
	}

	/** A kneeling stone pilgrim (the Pilgrims' Ring): a figure on its knees on a plinth, hands on thighs, facing Yaw;
	 *  bHeadless: the head broken off and laid beside it facing away. */
	inline void DsPilgrim(FBuild& B, const FVector& Base, float Yaw, bool bHeadless, int32 Seed)
	{
		const FRotator R(0.f, Yaw, 0.f);
		const FVector Fw = R.Vector();
		B.K.Add(DsSandstone(0.8f), FVector(110.f, 90.f, 24.f), Base + FVector(0, 0, 12.f), R);
		B.K.Add(DsSandstone(0.95f), FVector(70.f, 60.f, 40.f), Base + FVector(0, 0, 44.f) - Fw * 15.f, R);                // the folded legs
		B.M.Lathe(Base + FVector(0, 0, 60.f), FVector(0, 0, 1), 80.f, { FVector(0.f, 26.f, 18.f), FVector(0.6f, 24.f, 16.f), FVector(1.f, 22.f, 14.f) }, 8, DsRGB(0xC8, 0xA0, 0x70) * 0.55f, 0.9f);
		for (int32 s = -1; s <= 1; s += 2) B.Limb(Base + FVector(0, 0, 130.f) + R.RotateVector(FVector(0, s * 24.f, 0)), Base + FVector(0, 0, 72.f) + R.RotateVector(FVector(22.f, s * 20.f, 0)), 7.f, 6.f, DsRGB(0xC8, 0xA0, 0x70) * 0.52f, 6);
		const FVector Neck = Base + FVector(0, 0, 142.f);
		if (!bHeadless) B.Ball(Neck + FVector(0, 0, 16.f), 15.f, DsRGB(0xC8, 0xA0, 0x70) * 0.55f, 0.9f, 1.15f);
		else
		{
			B.K.Add(DsSandstone(0.7f), FVector(26.f, 26.f, 8.f), Neck, R);                                                    // the break
			const FVector Lie = Base + R.RotateVector(FVector(70.f, 50.f * (Seed % 2 ? 1.f : -1.f), 15.f));
			B.Ball(Lie, 15.f, DsRGB(0xC8, 0xA0, 0x70) * 0.5f, 0.9f, 1.1f);                                                     // laid facing away
		}
		B.Box(Base + FVector(0, 0, 75.f), FVector(45.f, 40.f, 75.f), R);
	}

	/** A newcomer's cask lying in the sand, its bung out (the Tall Wreck's empties). */
	inline void DsCask(FBuild& B, const FVector& At, float Yaw, bool bStanding = false)
	{
		const FVector Axis = bStanding ? FVector(0, 0, 1) : FRotator(0.f, Yaw, 0.f).Vector();
		const FVector From = At + (bStanding ? FVector::ZeroVector : FVector(0, 0, 32.f)) - Axis * (bStanding ? 0.f : 45.f);
		B.M.Lathe(From, Axis, 90.f, { FVector(0.f, 28.f, 28.f), FVector(0.5f, 34.f, 34.f), FVector(1.f, 28.f, 28.f) }, 12, DsRGB(0x7A, 0x5E, 0x40) * 0.5f, 0.85f);
		for (int32 h = 0; h < 3; ++h) B.M.Lathe(From + Axis * (8.f + h * 37.f), Axis, 5.f, { FVector(0.f, 31.f + (h == 1 ? 3.f : 0.f), 31.f + (h == 1 ? 3.f : 0.f)), FVector(1.f, 31.f + (h == 1 ? 3.f : 0.f), 31.f + (h == 1 ? 3.f : 0.f)) }, 12, DsRGB(0x3A, 0x30, 0x28) * 0.5f, 0.6f);
	}

	/** A driftwood cross over a newcomer's grave (two silver limbs lashed with rope, a mound of sand, a ring of shells). */
	inline void DsCross(FBuild& B, const FVector& Base, float Yaw, int32 Seed)
	{
		const FRotator R(3.f * (B.Rand(Seed) - 0.5f) * 4.f, Yaw, 0.f);
		B.Limb(Base - FVector(0, 0, 30.f), Base + R.RotateVector(FVector(0, 0, 150.f)), 6.f, 5.f, DsDriftC(), 6);
		B.Limb(Base + R.RotateVector(FVector(0, -40.f, 110.f)), Base + R.RotateVector(FVector(0, 40.f, 110.f)), 5.f, 4.f, DsDriftC() * 0.95f, 6);
		B.K.Add(DsRope(), FVector(14.f, 14.f, 14.f), Base + R.RotateVector(FVector(0, 0, 110.f)), R + FRotator(0.f, 45.f, 45.f));
		B.K.Add(DsPaleSand(0.95f), FVector(180.f, 80.f, 26.f), Base + R.RotateVector(FVector(-100.f, 0, 4.f)), FRotator(0.f, Yaw, 0.f), EWRLookMesh::Cylinder);
	}

	/** A Sunwatch mirror on its bronze pivot (the Lookout, the gallery, the relay): a round bronze face 1.2 m in a fork on
	 *  a post. Built into its own part so it can turn (returned; null on a server). */
	inline USceneComponent* DsMirrorPart(FBuild& B, const FVector& Base, float Yaw, float R = 60.f)
	{
		B.K.Add(DsSandstone(0.9f), FVector(70.f, 70.f, 60.f), Base + FVector(0, 0, 30.f), FRotator(0.f, Yaw, 0.f));
		B.K.Rod(DsBronzeDull(), Base + FVector(0, 0, 60.f), Base + FVector(0, 0, 150.f), 10.f);
		B.Box(Base + FVector(0, 0, 90.f), FVector(35.f, 35.f, 90.f));
		TArray<FWRLookBlock> Bl;
		FKit K(FTransform::Identity, Bl);
		for (int32 s = -1; s <= 1; s += 2) K.Rod(DsBronzeDull(), FVector(0, s * (R + 10.f), 0), FVector(0, s * (R + 10.f), R + 20.f), 6.f);
		K.Rod(DsBronzeDull(), FVector(0, -(R + 10.f), 0), FVector(0, R + 10.f, 0), 8.f);
		K.Disc(DsBronze(), FVector(0, 0, R + 10.f), FVector(1, 0, 0.25f).GetSafeNormal(), R * 2.f, 6.f);
		K.Ring(DsBronzeDull(), FVector(0, 0, R + 10.f), R + 4.f, 6.f, 8.f, 16);
		return DsPart(B, Base + FVector(0, 0, 150.f), FRotator(0.f, Yaw, 0.f), Bl);
	}

	/** A caravan wagon half buried (its wheels to the hubs in sand, the bed tilted, a torn awning on hoops) or, Variant 1,
	 *  a pack train's loads in a line with their Dromads' bones. */
	inline void DsCaravan(FBuild& B, const FVector& Base, float Yaw, int32 Variant, int32 Seed)
	{
		const FRotator R(0.f, Yaw, 4.f + 6.f * B.Rand(Seed));
		if (Variant % 2 == 0)
		{
			const FVector O = Base - FVector(0, 0, 50.f);
			B.K.Add(DsSunBleach(0.85f), FVector(380.f, 170.f, 18.f), O + FVector(0, 0, 90.f), R);
			for (int32 sd = -1; sd <= 1; sd += 2) B.K.Add(DsSunBleach(0.75f), FVector(380.f, 8.f, 50.f), O + R.RotateVector(FVector(0, sd * 85.f, 120.f)), R);
			for (int32 w = 0; w < 4; ++w)
			{
				const FVector Hub = O + R.RotateVector(FVector(w < 2 ? 130.f : -130.f, w % 2 ? 95.f : -95.f, 60.f));
				B.K.Ring(DsSunBleach(0.7f), Hub, 58.f, 8.f, 8.f, 14, 0.f, 360.f, 1.f);
				for (int32 sp = 0; sp < 6; ++sp) B.K.Rod(DsSunBleach(0.65f), Hub, Hub + R.RotateVector(FRotator(sp * 30.f, 0.f, 0.f).RotateVector(FVector(0, 0, 54.f))), 4.f);
			}
			for (int32 h = 0; h < 4; ++h)
			{
				const float X = -150.f + h * 100.f;
				TArray<FVector> Hoop;
				for (int32 k = 0; k <= 6; ++k) { const float A = PI * k / 6.f; Hoop.Add(O + R.RotateVector(FVector(X, -FMath::Cos(A) * 85.f, 125.f + FMath::Sin(A) * 110.f))); }
				if (h != 2 || B.Rand(Seed) > 0.5f) B.M.Tube(Hoop, 3.f, 4, DsRGB(0xB8, 0xA8, 0x90) * 0.5f, 0.8f);
			}
			B.K.Add(DsLinen(0.8f), FVector(160.f, 6.f, 130.f), O + R.RotateVector(FVector(-80.f, 84.f, 170.f)), R + FRotator(0.f, 0.f, -24.f));     // the torn awning
			for (int32 j = 0; j < 3; ++j) B.K.Add(DsClay(0.9f), FVector(40.f, 40.f, 60.f), O + R.RotateVector(FVector(-60.f + j * 50.f, -30.f + j * 20.f, 130.f)), R, EWRLookMesh::Cylinder);
			B.K.Add(DsSandFill(), FVector(300.f, 260.f, 70.f), Base + R.RotateVector(FVector(-20.f, -140.f, -5.f)), FRotator(0.f, Yaw, 0.f), EWRLookMesh::Cylinder);   // drifted against it
			B.Box(O + FVector(0, 0, 100.f), FVector(190.f, 90.f, 60.f), R);
		}
		else
		{
			for (int32 k = 0; k < 3; ++k) DsDromadBones(B, Base + FRotator(0.f, Yaw, 0.f).RotateVector(FVector(k * 420.f, 0, 0)), Yaw + 6.f * (B.Rand(Seed + k) - 0.5f), true, Seed + k, 45.f);
		}
	}

	/** A Sunwatch ruin: Variant 0 a wall stub with a doorway, 1 a roofless hut (four low walls), 2 a cistern (a round
	 *  stone-lined pit with a lip and steps). Sand drifts against the windward (east) faces. */
	inline void DsRuin(FBuild& B, const FVector& Base, float Yaw, int32 Variant, int32 Seed)
	{
		const FRotator R(0.f, Yaw, 0.f);
		const auto P = [&](float X, float Y) { const FVector L = R.RotateVector(FVector(X, Y, 0.f)); return FVector(L.X, L.Y, B.G(Base.X + L.X - B.Act->GetActorLocation().X, Base.Y + L.Y - B.Act->GetActorLocation().Y, Base.Z) - 20.f); };
		if (Variant == 0)
		{
			DsCoursedWall(B, Base + R.RotateVector(FVector(0, -500.f, -20.f)), Base + R.RotateVector(FVector(0, -90.f, -20.f)), 300.f, 70.f, DsSandstone(0.95f), 0.5f);
			DsCoursedWall(B, Base + R.RotateVector(FVector(0, 90.f, -20.f)), Base + R.RotateVector(FVector(0, 600.f, -20.f)), 300.f, 70.f, DsSandstone(0.95f), 0.6f);
			B.K.Add(DsSandstone(1.05f), FVector(90.f, 260.f, 40.f), Base + R.RotateVector(FVector(0, 0, 260.f)), R);         // the lintel
			for (int32 sd = -1; sd <= 1; sd += 2) B.K.Add(DsSandstone(1.f), FVector(80.f, 30.f, 260.f), Base + R.RotateVector(FVector(0, sd * 100.f, 110.f)), R);
		}
		else if (Variant == 1)
		{
			const float H = 140.f + 60.f * B.Rand(Seed);
			const FVector C0 = Base + R.RotateVector(FVector(-260.f, -220.f, -20.f)), C1 = Base + R.RotateVector(FVector(260.f, -220.f, -20.f)), C2 = Base + R.RotateVector(FVector(260.f, 220.f, -20.f)), C3 = Base + R.RotateVector(FVector(-260.f, 220.f, -20.f));
			DsCoursedWall(B, C0, C1, H, 50.f, DsMudbrick(), 0.4f, 60.f, 22.f);
			DsCoursedWall(B, C1, C2, H, 50.f, DsMudbrick(), 0.3f, 60.f, 22.f);
			DsCoursedWall(B, C2, C3, H * 0.7f, 50.f, DsMudbrick(), 0.5f, 60.f, 22.f);
			DsCoursedWall(B, C3, C3 + (C0 - C3) * 0.4f, H, 50.f, DsMudbrick(), 0.2f, 60.f, 22.f);
			B.K.Add(DsSandFill(0.95f), FVector(560.f, 300.f, 60.f), Base + R.RotateVector(FVector(0, 80.f, -10.f)), R, EWRLookMesh::Cylinder);
			B.K.Add(DsClay(0.8f), FVector(50.f, 50.f, 70.f), Base + R.RotateVector(FVector(120.f, -120.f, 10.f)), R, EWRLookMesh::Cylinder);
		}
		else
		{
			// A cistern: a dressed ring 6 m across, its lip 50 cm proud of the ground, steps going down inside to dry mud.
			for (int32 s = 0; s < 24; ++s)
			{
				const FRotator Rr(0.f, s * 15.f, 0.f);
				B.K.Add(DsSandstone(0.95f + 0.1f * B.Rand(s)), FVector(70.f, 82.f, 50.f), Base + Rr.RotateVector(FVector(300.f, 0, 10.f)), Rr);
				B.Box(Base + Rr.RotateVector(FVector(300.f, 0, 10.f)), FVector(35.f, 41.f, 25.f), Rr);
			}
			B.K.Add(DsSandstone(0.6f), FVector(560.f, 560.f, 30.f), Base - FVector(0, 0, 260.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			for (int32 st = 0; st < 8; ++st) B.K.Add(DsSandstone(0.8f), FVector(60.f, 110.f, 30.f), Base + R.RotateVector(FVector(240.f - st * 26.f, 0.f, -30.f - st * 30.f)), R);
			B.K.Add(DsClay(0.6f), FVector(500.f, 500.f, 8.f), Base - FVector(0, 0, 244.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		}
		(void)P;
	}

	/** A sunstone outcrop on sun-facing rock (the Ore kit; the nodes stand on it): a ledge of the zone's rock with amber
	 *  crystals and copper glitter, their stored glow at night (N blocks). */
	inline void DsOutcrop(FBuild& B, const FVector& Base, float Yaw, FName Ore, int32 Seed)
	{
		const bool bSun = Ore == TEXT("sunstone"), bCopper = Ore == TEXT("copper_ore"), bSalt = Ore == TEXT("rock_salt"), bGlass = Ore == TEXT("sunglass");
		const FLinearColor Rock = bSalt ? DsRGB(0xE8, 0xE4, 0xDC) : bGlass ? DsRGB(0x6E, 0x5A, 0x48) : DsRockTint();
		for (int32 r = 0; r < 4; ++r) B.Rock(r + Seed, FVector(260.f, 220.f, 160.f) * (0.6f + 0.6f * B.Rand(Seed + r)), Base + FRotator(0.f, Yaw + r * 90.f, 0.f).RotateVector(FVector(160.f * (r > 0), 0, -20.f)), FRotator(0.f, Yaw + r * 70.f, 0.f), Rock, r == 0);
		for (int32 c = 0; c < 7; ++c)
		{
			const FVector P = Base + FRotator(0.f, Yaw + c * 51.f, 0.f).RotateVector(FVector(70.f + 60.f * B.Rand(c + Seed), 0, 60.f + 40.f * B.Rand(c * 3)));
			const FVector Dir = FRotator(50.f + 30.f * B.Rand(c), Yaw + c * 51.f, 0.f).Vector();
			if (bSun) { B.K.Spike(DsSunstone(0.f), P, Dir, 18.f, 40.f + 30.f * B.Rand(c + 9)); B.N.Spike(DsSunstone(2.2f), P + Dir * 2.f, Dir, 12.f, 30.f); }
			else if (bCopper) B.K.Add(FSurf::Stone(DsRGB(0x3A, 0x8A, 0x5A) * 0.5f, 0.7f), FVector(60.f, 30.f, 14.f), P, Dir.Rotation());       // malachite bands
			else if (bSalt) B.K.Add(DsSalt(), FVector(40.f, 40.f, 40.f), P, FRotator(10.f * c, 30.f * c, 0.f));
			else if (bGlass) B.K.Add(DsSunglass(), FVector(36.f, 28.f, 20.f), P, FRotator(20.f * c, 40.f * c, 0.f));
		}
		if (bSun) B.Lamp(Base + FVector(0, 0, 120.f), DsRGB(0xF2, 0xA2, 0x3A), 500.f, 600.f);
	}

	/** A seep: a spring at the foot of a rock, a pool 3-5 m, two or three palms and reeds round it. */
	inline void DsSeep(FBuild& B, const FVector& Base, float Yaw, int32 Variant, int32 Seed)
	{
		B.K.Add(DsWater(), FVector(440.f, 340.f, 6.f), Base + FVector(0, 0, -6.f), FRotator(0.f, Yaw, 0.f), EWRLookMesh::Cylinder);
		B.K.Add(FSurf::Stone(DsRGB(0x6E, 0x5A, 0x40) * 0.45f, 0.4f), FVector(520.f, 420.f, 8.f), Base + FVector(0, 0, -10.f), FRotator(0.f, Yaw, 0.f), EWRLookMesh::Cylinder);   // wet margin
		B.Rock(Seed, FVector(300.f, 260.f, 220.f), Base + FRotator(0.f, Yaw, 0.f).RotateVector(FVector(-300.f, 0, -40.f)), FRotator(0.f, Yaw, 0.f), DsRockTint(), true);
		for (int32 p = 0; p < 2 + Variant; ++p)
		{
			const FVector At = Base + FRotator(0.f, Yaw + 60.f + p * 110.f, 0.f).RotateVector(FVector(320.f + 60.f * B.Rand(p), 0, 0));
			DsPalm(B, FVector(At.X, At.Y, B.G(At.X, At.Y, Base.Z)), Yaw + p * 40.f, 0.6f + 0.25f * B.Rand(Seed + p), Seed + p, p == 0);
		}
		for (int32 r = 0; r < 10; ++r)
		{
			const float A = (r * 36.f + Yaw) * PI / 180.f;
			const FVector P = Base + FVector(FMath::Cos(A) * 240.f, FMath::Sin(A) * 190.f, -4.f);
			for (int32 b = 0; b < 5; ++b) B.K.Spike(FSurf::Cloth(DsRGB(0x6E, 0x86, 0x46) * 0.45f), P + FVector(b * 4.f, (b % 2) * 5.f, 0.f), FVector(0.05f * (b - 2), 0.04f * b, 1.f), 3.f, 110.f + 50.f * B.Rand(r * 5 + b));
		}
	}
}
