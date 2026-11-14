// The farm's modelled look (WRFarmLook.h). Plants are built from three shapes: lathed parts (stems, roots, fruit,
// caps), lofted blades (leaves, petals, fronds, pads) and tubes (vines, tendrils), all in vertex colour on M_HeldTool,
// rebuilt only when what they show changes (a stage, the fruit colouring in, water, a product collected).

#include "WRFarmLook.h"
#include "WRFarm.h"
#include "WRBuilding.h"
#include "WRToolMesh.h"
#include "WRTuning.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"

namespace WRFarmLookDetail
{
	/** 0..1 from a seed and a salt (the same on every machine). */
	float H(float Seed, int32 Salt)
	{
		const float X = FMath::Sin(Seed * 12.9898f + Salt * 78.233f) * 43758.5453f;
		return X - FMath::FloorToFloat(X);
	}

	FLinearColor Mix(const FLinearColor& A, const FLinearColor& B, float T) { return FMath::Lerp(A, B, FMath::Clamp(T, 0.f, 1.f)); }

	/**
	 * A leaf / petal / frond blade from Base along Dir, curving down by Droop (0..1) toward its tip, Width wide at its
	 * widest (a third of the way), a mid-rib ridge so it catches light. Up is the blade's face normal at its base.
	 */
	void Blade(FWRToolMesh& M, const FVector& Base, const FVector& Dir, const FVector& Up, float Length, float Width, float Droop, const FLinearColor& Colour, float Rough = 0.6f, int32 Segs = 5)
	{
		if (Length < 0.5f) return;
		const FVector D = Dir.GetSafeNormal();
		FVector Side = FVector::CrossProduct(Up, D).GetSafeNormal();
		if (Side.IsNearlyZero()) Side = FVector::CrossProduct(FVector::UpVector, D).GetSafeNormal();
		if (Side.IsNearlyZero()) Side = FVector::RightVector;
		const FVector N = FVector::CrossProduct(D, Side).GetSafeNormal();
		TArray<TArray<FVector>> Rings;
		for (int32 i = 0; i <= Segs; ++i)
		{
			const float T = i / float(Segs);
			// Bow down toward the tip (gravity), widest a third along, pointed at the tip.
			const FVector C = Base + D * (Length * T) - FVector(0.f, 0.f, Length * Droop * T * T);
			const float W = Width * FMath::Sin(PI * FMath::Pow(FMath::Max(T, 0.02f), 0.75f)) * (i == Segs ? 0.08f : 1.f);
			const float Rib = FMath::Max(0.25f, Width * 0.06f);
			Rings.Add({ C + Side * W, C + N * Rib, C - Side * W, C - N * Rib * 0.4f });
		}
		M.Loft(Rings, Colour, Rough, false);
	}

	/** A round-ish part (fruit, berry, root, cap): an egg along Axis, R across, Len long. */
	void Ball(FWRToolMesh& M, const FVector& Centre, const FVector& Axis, float Len, float R, const FLinearColor& Colour, float Rough, int32 Sides = 8)
	{
		const FVector A = Axis.GetSafeNormal();
		TArray<FVector> Prof;
		for (int32 i = 0; i <= 6; ++i)
		{
			const float T = i / 6.f;
			const float S = FMath::Sin(PI * T);
			Prof.Add(FVector(T, R * FMath::Max(0.08f, S), R * FMath::Max(0.08f, S)));
		}
		M.Lathe(Centre - A * Len * 0.5f, A, Len, Prof, Sides, Colour, Rough);
	}

	/** A stem from Base along Dir, tapering R0 to R1, with a gentle bow. */
	void Stem(FWRToolMesh& M, const FVector& Base, const FVector& Dir, float Len, float R0, float R1, const FLinearColor& Colour, float Bend = 0.f)
	{
		if (Len < 0.5f) return;
		M.Lathe(Base, Dir.GetSafeNormal(), Len, { FVector(0.f, R0, R0), FVector(0.5f, (R0 + R1) * 0.5f, (R0 + R1) * 0.5f), FVector(1.f, R1, R1) }, 5, Colour, 0.7f, Bend);
	}

	FVector Around(float Angle, float Up) { return FVector(FMath::Cos(Angle), FMath::Sin(Angle), Up).GetSafeNormal(); }
}
using namespace WRFarmLookDetail;

void FWRFarmLook::Plant(FWRToolMesh& M, const FWRCropDef& C, float G, const FVector& Base, float Yaw, float Seed)
{
	// Size: a sprout at a seventh of its height, full grown at 90%. Flowers between 60% and 90%; fruit from 75%,
	// green at first and its own colour when ripe.
	const float Size = FMath::Lerp(0.14f, 1.f, FMath::Clamp(G / 0.9f, 0.f, 1.f));
	const float Ht = C.Height * Size;
	const bool bSprout = G < 0.25f;
	const float FlowerT = G >= 0.6f && G < 0.92f ? FMath::Clamp((G - 0.6f) / 0.1f, 0.f, 1.f) : 0.f;
	const float FruitT = G >= 0.75f ? FMath::Clamp((G - 0.75f) / 0.25f, 0.f, 1.f) : -1.f;
	const FLinearColor Fruit = Mix(C.Leaf * 1.1f, C.Fruit, FruitT);
	const float FruitRough = G >= 1.f ? 0.25f : 0.5f;   // a ripe one is glossy: it catches the light
	const float Y0 = FMath::DegreesToRadians(Yaw);
	const FName F = C.Form;

	if (bSprout && F != TEXT("mushroom") && F != TEXT("kelp"))
	{
		// Two seed leaves on a short stem.
		Stem(M, Base, FVector::UpVector, Ht * 0.6f, 0.5f, 0.35f, C.Stem);
		for (int32 k = 0; k < 2; ++k)
		{
			const FVector D = Around(Y0 + k * PI, 0.6f);
			Blade(M, Base + FVector(0, 0, Ht * 0.6f), D, FVector::UpVector, FMath::Max(3.f, Ht * 0.7f), FMath::Max(1.2f, Ht * 0.3f), 0.1f, C.Leaf);
		}
		return;
	}

	if (F == TEXT("grain"))
	{
		// A tuft of stalks, each with its ear; green to its ripe colour, bowing as the ears fill.
		const int32 N = 9;
		for (int32 k = 0; k < N; ++k)
		{
			const float A = Y0 + k * (2.f * PI / N) + H(Seed, k) * 0.6f;
			const FVector Lean = Around(A, 6.f + H(Seed, k + 20) * 4.f);
			const float L = Ht * (0.8f + 0.25f * H(Seed, k + 40));
			const FLinearColor StalkCol = Mix(C.Stem, C.Fruit * 0.85f, FruitT);
			Stem(M, Base + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * 3.f, Lean, L, 0.45f, 0.3f, StalkCol, FruitT > 0.f ? -L * 0.12f * FruitT : 0.f);
			Blade(M, Base + Lean * L * 0.3f, Around(A + 0.4f, 0.5f), FVector::UpVector, L * 0.45f, 1.1f, 0.35f, C.Leaf);
			if (G >= 0.5f)
			{
				const FVector Tip = Base + Lean * L;
				Ball(M, Tip + Lean * 4.f, Lean + FVector(0, 0, -0.3f * FMath::Max(0.f, FruitT)), 9.f * Size, 1.3f, FruitT >= 0.f ? Fruit : C.Leaf, FruitRough, 6);
			}
		}
		return;
	}
	if (F == TEXT("root"))
	{
		// A rosette of broad leaves, the root's shoulder showing at the soil when it's ready.
		const int32 N = 6;
		for (int32 k = 0; k < N; ++k)
		{
			const float A = Y0 + k * (2.f * PI / N) + H(Seed, k) * 0.4f;
			Blade(M, Base + FVector(0, 0, 3.f * Size), Around(A, 1.4f), FVector::UpVector, Ht * (0.9f + 0.2f * H(Seed, k + 9)), Ht * 0.28f, 0.45f, C.Leaf);
			Stem(M, Base, Around(A, 1.6f), Ht * 0.3f, 0.6f, 0.4f, C.Stem);
		}
		if (G >= 0.5f)
		{
			const float R = 5.f * FMath::Clamp((G - 0.5f) / 0.5f, 0.2f, 1.f) * (C.Height / 35.f);
			Ball(M, Base + FVector(0, 0, R * 0.2f), FVector::UpVector, R * 1.7f, R, Mix(C.Stem, C.Fruit, FMath::Max(0.f, FruitT)), FruitRough, 9);
		}
		return;
	}
	if (F == TEXT("vine"))
	{
		// A chestnut stake with the vine twining up it, leaves along it, bunches hanging from the arms.
		const float Stake = C.Height * 1.05f;
		M.Lathe(Base, FVector::UpVector, Stake, { FVector(0, 2.2f, 2.2f), FVector(0.97f, 1.8f, 1.8f), FVector(1, 0.6f, 0.6f) }, 5, FLinearColor(0.16f, 0.1f, 0.05f), 0.85f, 0.f, Seed + 1.f);
		TArray<FVector> Twine;
		for (int32 i = 0; i <= 14; ++i)
		{
			const float T = i / 14.f;
			const float A = Y0 + T * 4.f * PI;
			Twine.Add(Base + FVector(FMath::Cos(A) * 3.2f, FMath::Sin(A) * 3.2f, Ht * T));
		}
		M.Tube(Twine, 0.9f, 4, C.Stem, 0.8f);
		for (int32 k = 0; k < 7; ++k)
		{
			const float T = 0.25f + 0.75f * k / 7.f;
			const float A = Y0 + k * 2.4f;
			Blade(M, Base + FVector(FMath::Cos(A) * 3.f, FMath::Sin(A) * 3.f, Ht * T), Around(A, 0.3f), FVector::UpVector, 13.f * Size, 7.f * Size, 0.35f, C.Leaf);
		}
		if (FruitT >= 0.f)
		{
			for (int32 b = 0; b < 3; ++b)
			{
				const float A = Y0 + b * 2.1f + 0.5f;
				const FVector Top = Base + FVector(FMath::Cos(A) * 7.f, FMath::Sin(A) * 7.f, Ht * (0.55f + 0.12f * b));
				for (int32 g = 0; g < 9; ++g)
				{
					const float Row = g < 4 ? 0.f : g < 7 ? 1.f : 2.f;
					const float GA = A + g * 1.7f;
					Ball(M, Top + FVector(FMath::Cos(GA) * (2.6f - Row * 0.7f), FMath::Sin(GA) * (2.6f - Row * 0.7f), -2.8f * Row), FVector::UpVector, 2.6f, 1.25f, Fruit, FruitRough, 6);
				}
			}
		}
		if (FlowerT > 0.f && FruitT < 0.f)
		{
			for (int32 b = 0; b < 3; ++b) Ball(M, Base + FVector(0, 0, Ht * (0.6f + 0.1f * b)) + Around(Y0 + b * 2.1f, 0.f) * 6.f, FVector::UpVector, 4.f, 1.2f, C.Flower, 0.6f, 5);
		}
		return;
	}
	if (F == TEXT("cactus"))
	{
		// Paddles on paddles (an opuntia), fruits along the top edges.
		const auto Pad = [&M, &C](const FVector& At, const FVector& Up, const FVector& Face, float L, float W)
		{
			TArray<TArray<FVector>> Rings;
			const FVector Side = FVector::CrossProduct(Face, Up).GetSafeNormal();
			for (int32 i = 0; i <= 6; ++i)
			{
				const float T = i / 6.f;
				const float S = FMath::Sin(PI * T);
				const FVector P = At + Up * L * T;
				Rings.Add({ P + Side * W * S, P + Face * 1.6f * S, P - Side * W * S, P - Face * 1.6f * S });
			}
			M.Loft(Rings, C.Leaf, 0.55f, false);
		};
		const FVector Face = Around(Y0, 0.f);
		Pad(Base, FVector(0.1f, 0, 1).GetSafeNormal(), Face, Ht * 0.55f, Ht * 0.22f);
		if (G >= 0.4f)
		{
			Pad(Base + FVector(0, 0, Ht * 0.45f), FVector(0.6f, 0.1f, 1).GetSafeNormal(), Face, Ht * 0.45f, Ht * 0.18f);
			Pad(Base + FVector(0, 0, Ht * 0.45f), FVector(-0.6f, -0.1f, 1).GetSafeNormal(), Face, Ht * 0.42f, Ht * 0.17f);
		}
		if (FruitT >= 0.f)
		{
			for (int32 k = 0; k < 5; ++k) Ball(M, Base + FVector((k - 2) * 4.f, 0.f, Ht * (0.85f + 0.08f * (k % 2))) + Face * 1.5f, FVector::UpVector, 5.f, 2.3f, Fruit, FruitRough, 7);
		}
		else if (FlowerT > 0.f)
		{
			for (int32 k = 0; k < 4; ++k) Ball(M, Base + FVector((k - 1.5f) * 5.f, 0.f, Ht * 0.92f), FVector::UpVector, 3.f, 2.f, C.Flower, 0.6f, 6);
		}
		return;
	}
	if (F == TEXT("palm") || F == TEXT("tree"))
	{
		// A young tree: a trunk, a crown (fronds for the palm, a cluster of leaves for the orchard tree), fruit.
		const bool bPalm = F == TEXT("palm");
		const float Trunk = Ht * (bPalm ? 0.62f : 0.55f);
		M.Lathe(Base, FVector::UpVector, Trunk, { FVector(0, 3.2f * Size, 3.2f * Size), FVector(0.5f, 2.3f * Size, 2.3f * Size), FVector(1, 1.6f * Size, 1.6f * Size) }, 6, C.Stem, 0.85f, 1.2f, Seed + 3.f);
		const FVector Top = Base + FVector(0, 0, Trunk);
		if (bPalm)
		{
			for (int32 k = 0; k < 9; ++k) Blade(M, Top, Around(Y0 + k * 0.7f, 0.5f), FVector::UpVector, Ht * 0.55f, 5.f * Size, 0.55f, C.Leaf, 0.6f, 6);
		}
		else
		{
			for (int32 k = 0; k < 5; ++k)
			{
				const FVector Branch = Around(Y0 + k * 1.26f, 1.1f);
				Stem(M, Top - FVector(0, 0, 4.f), Branch, Ht * 0.3f, 1.f * Size, 0.4f, C.Stem);
				for (int32 l = 0; l < 5; ++l)
				{
					const FVector At = Top + Branch * Ht * (0.12f + 0.05f * l);
					Blade(M, At, Around(Y0 + k * 1.26f + l * 1.3f, 0.2f), FVector::UpVector, 6.f * Size + 3.f, 3.f * Size + 1.f, 0.2f, C.Leaf);
				}
			}
			if (FlowerT > 0.f) for (int32 k = 0; k < 6; ++k) Ball(M, Top + Around(Y0 + k, 0.8f) * Ht * 0.25f, FVector::UpVector, 2.5f, 1.5f, C.Flower, 0.6f, 5);
		}
		if (FruitT >= 0.f)
		{
			for (int32 k = 0; k < (bPalm ? 12 : 6); ++k)
			{
				const FVector Dir = Around(Y0 + k * (bPalm ? 0.5f : 1.05f), bPalm ? -0.6f : 0.3f);
				const FVector At = Top + Dir * (bPalm ? 5.f + (k % 3) * 2.f : Ht * 0.22f) - FVector(0, 0, bPalm ? 3.f + (k % 4) * 2.f : 2.f);
				Ball(M, At, FVector::UpVector, bPalm ? 3.4f : 4.4f, bPalm ? 1.5f : 2.4f, Fruit, FruitRough, 7);
			}
		}
		return;
	}
	if (F == TEXT("melon"))
	{
		// A runner over the soil with broad leaves; the melon swells (and glows, the glowmelon's colour) as it ripens.
		TArray<FVector> Run;
		for (int32 i = 0; i <= 8; ++i) Run.Add(Base + FVector(FMath::Cos(Y0) * i * 4.f, FMath::Sin(Y0) * i * 4.f, 1.f + FMath::Sin(i * 1.3f) * 0.6f));
		M.Tube(Run, 0.8f, 4, C.Stem, 0.8f);
		for (int32 k = 0; k < 5; ++k) Blade(M, Run[1 + k], Around(Y0 + (k % 2 ? 1.2f : -1.2f), 0.6f), FVector::UpVector, 12.f * Size, 8.f * Size, 0.3f, C.Leaf);
		if (FruitT >= 0.f || G >= 0.65f)
		{
			const float R = 9.f * FMath::Clamp((G - 0.65f) / 0.35f, 0.25f, 1.f);
			Ball(M, Run[5] + FVector(0, 0, R * 0.8f), Around(Y0, 0.f), R * 2.3f, R, Mix(C.Leaf, C.Fruit, FMath::Max(0.f, FruitT)), FruitRough, 10);
		}
		return;
	}
	if (F == TEXT("stalk"))
	{
		// Forced stalks: tall, pale, close together, with a small crumpled leaf at each top.
		for (int32 k = 0; k < 6; ++k)
		{
			const FVector D = Around(Y0 + k * 1.05f, 8.f);
			Stem(M, Base + Around(Y0 + k * 1.05f, 0.f) * 2.5f, D, Ht * (0.85f + 0.2f * H(Seed, k)), 1.4f, 1.1f, Mix(C.Stem, C.Fruit, FMath::Max(0.f, FruitT)));
			Blade(M, Base + D * Ht * 0.95f, Around(Y0 + k * 1.05f, 0.3f), FVector::UpVector, 10.f * Size, 7.f * Size, 0.4f, C.Leaf);
		}
		return;
	}
	if (F == TEXT("mushroom"))
	{
		// Caps on short stems (a shelf of oysters, or a shaggy white mane): they swell from pins to full caps.
		for (int32 k = 0; k < 5; ++k)
		{
			const FVector At = Base + Around(Y0 + k * 1.3f, 0.f) * (3.f + 4.f * H(Seed, k));
			const float S = Size * (0.7f + 0.4f * H(Seed, k + 5));
			Stem(M, At, FVector::UpVector, 5.f * S, 1.2f * S + 0.3f, 1.f * S + 0.3f, C.Stem);
			M.Lathe(At + FVector(0, 0, 4.f * S), FVector::UpVector, 4.f * S + 0.5f, { FVector(0, 1.f * S, 1.f * S), FVector(0.3f, 6.f * S, 5.f * S), FVector(0.75f, 5.f * S, 4.f * S), FVector(1, 0.5f, 0.5f) }, 9,
				Mix(C.Leaf, C.Fruit, FMath::Max(0.f, FruitT)), 0.7f);
		}
		return;
	}
	if (F == TEXT("kelp"))
	{
		// Ribbons rising from their holdfast on the line, swaying in the swell (frozen mid-sway), bladders along them.
		for (int32 k = 0; k < 4; ++k)
		{
			const float A = Y0 + k * 1.6f;
			Blade(M, Base, FVector(FMath::Cos(A) * 0.3f, FMath::Sin(A) * 0.3f, 1.f), Around(A, 0.f), C.Height * Size * (0.8f + 0.3f * H(Seed, k)), 4.f, -0.15f, C.Leaf, 0.4f, 7);
		}
		if (FruitT >= 0.f) for (int32 k = 0; k < 6; ++k) Ball(M, Base + FVector(0, 0, Ht * (0.2f + 0.12f * k)) + Around(Y0 + k, 0.f) * 2.f, FVector::UpVector, 2.f, 1.f, Fruit, 0.4f, 5);
		return;
	}
	if (F == TEXT("berry"))
	{
		// Low, wrinkled five-lobed leaves on short stems; amber berries held up on stalks.
		for (int32 k = 0; k < 7; ++k)
		{
			const float A = Y0 + k * 0.9f;
			Stem(M, Base, Around(A, 2.f), Ht * 0.7f, 0.4f, 0.3f, C.Stem);
			Blade(M, Base + Around(A, 2.f) * Ht * 0.7f, Around(A, 0.1f), FVector::UpVector, 7.f * Size + 1.f, 5.f * Size + 1.f, 0.1f, C.Leaf);
		}
		if (FlowerT > 0.f) for (int32 k = 0; k < 3; ++k) Ball(M, Base + FVector(0, 0, Ht) + Around(Y0 + k * 2.f, 0.f) * 3.f, FVector::UpVector, 2.f, 1.8f, C.Flower, 0.6f, 6);
		if (FruitT >= 0.f) for (int32 k = 0; k < 4; ++k) Ball(M, Base + FVector(0, 0, Ht * 1.05f) + Around(Y0 + k * 1.6f, 0.f) * 3.5f, FVector::UpVector, 2.6f, 1.5f, Fruit, FruitRough, 7);
		return;
	}
	if (F == TEXT("flower"))
	{
		// Upright stems with narrow leaves; the bloom opens at the top (gentian, asphodel, flax, bloodroot), and the
		// root it's grown for fattens below as it ripens.
		const int32 N = 5;
		for (int32 k = 0; k < N; ++k)
		{
			const float A = Y0 + k * (2.f * PI / N) + H(Seed, k) * 0.5f;
			const FVector D = Around(A, 7.f);
			const float L = Ht * (0.8f + 0.25f * H(Seed, k + 3));
			Stem(M, Base, D, L, 0.5f, 0.35f, C.Stem);
			for (int32 l = 0; l < 3; ++l) Blade(M, Base + D * L * (0.2f + 0.2f * l), Around(A + l * 2.1f, 0.6f), FVector::UpVector, 7.f * Size + 2.f, 1.5f, 0.3f, C.Leaf);
			if (FlowerT > 0.f || FruitT >= 0.f)
			{
				const FVector Top = Base + D * L;
				const FLinearColor Bloom = FruitT >= 0.f ? Mix(C.Flower, C.Flower * 0.6f, FruitT) : C.Flower;
				for (int32 p = 0; p < 5; ++p) Blade(M, Top, Around(A + p * 1.2566f, 0.9f), FVector::UpVector, 3.5f * FMath::Max(FlowerT, 0.6f), 1.6f, -0.1f, Bloom, 0.5f, 3);
			}
		}
		if (FruitT >= 0.f) Ball(M, Base + FVector(0, 0, 1.f), FVector::UpVector, 5.f, 3.f * FruitT + 1.f, C.Fruit, 0.6f, 7);
		return;
	}
	// "bush" (the default): a branching shrub, white flowers, then pods hanging under the leaves (a chilli plant).
	const int32 N = 5;
	for (int32 k = 0; k < N; ++k)
	{
		const float A = Y0 + k * (2.f * PI / N) + H(Seed, k) * 0.5f;
		const FVector D = Around(A, 2.2f);
		Stem(M, Base, D, Ht * 0.75f, 0.9f, 0.4f, C.Stem);
		for (int32 l = 0; l < 4; ++l)
		{
			const FVector At = Base + D * Ht * (0.3f + 0.15f * l);
			Blade(M, At, Around(A + (l % 2 ? 0.8f : -0.8f), 0.4f), FVector::UpVector, 8.f * Size + 2.f, 3.5f * Size + 1.f, 0.3f, C.Leaf);
		}
		if (FlowerT > 0.f && FruitT < 0.f) Ball(M, Base + D * Ht * 0.7f + FVector(0, 0, 1.f), FVector::UpVector, 2.2f, 1.4f, C.Flower, 0.6f, 5);
		if (FruitT >= 0.f)
		{
			// A pod hanging down from the branch, green to its colour.
			const FVector At = Base + D * Ht * (0.55f + 0.1f * H(Seed, k));
			M.Lathe(At, FVector(0.15f, 0.f, -1.f).GetSafeNormal(), 7.f * FMath::Max(0.4f, FruitT), { FVector(0, 1.6f, 1.6f), FVector(0.3f, 1.8f, 1.8f), FVector(1, 0.2f, 0.2f) }, 6, Fruit, FruitRough);
		}
	}
}

void FWRFarmLook::Fish(FWRToolMesh& M, const FVector& Centre, float Length, const FLinearColor& Back, const FLinearColor& Belly, const FLinearColor& Fins)
{
	// A body lofted from oval sections (deepest a third back from the nose), a forked tail, dorsal and pectoral fins,
	// an eye. On its side, nose +X: how a trophy fish is mounted.
	const float L = Length;
	TArray<TArray<FVector>> Rings;
	for (int32 i = 0; i <= 10; ++i)
	{
		const float T = i / 10.f;   // 0 tail .. 1 nose
		const float Depth = L * 0.13f * FMath::Max(0.12f, FMath::Pow(FMath::Sin(PI * FMath::Pow(T, 0.8f)), 0.8f));
		const float Thick = Depth * 0.45f;
		const FVector C = Centre + FVector(L * (T - 0.5f), 0.f, 0.f);
		TArray<FVector> Ring;
		for (int32 s = 0; s < 8; ++s)
		{
			const float A = s * PI / 4.f;
			Ring.Add(C + FVector(0.f, FMath::Sin(A) * Thick, FMath::Cos(A) * Depth));
		}
		Rings.Add(Ring);
	}
	// Back on top, belly below: two lofts split along the side line.
	M.Loft(Rings, Back, 0.35f, true);
	TArray<TArray<FVector>> BellyRings;
	for (const TArray<FVector>& R : Rings) BellyRings.Add({ R[2] * 1.f + FVector(0, 0.2f, 0), R[3] + FVector(0, 0.2f, 0), R[4] + FVector(0, 0.2f, 0), R[5] + FVector(0, -0.2f, 0), R[6] + FVector(0, -0.2f, 0) });
	M.Loft(BellyRings, Belly, 0.3f, false);
	const FVector Tail = Centre - FVector(L * 0.5f, 0.f, 0.f);
	Blade(M, Tail, FVector(-1.f, 0.f, 0.7f), FVector::RightVector, L * 0.2f, L * 0.06f, 0.f, Fins, 0.5f, 3);
	Blade(M, Tail, FVector(-1.f, 0.f, -0.7f), FVector::RightVector, L * 0.2f, L * 0.06f, 0.f, Fins, 0.5f, 3);
	Blade(M, Centre + FVector(0.f, 0.f, L * 0.12f), FVector(-0.7f, 0.f, 1.f), FVector::RightVector, L * 0.14f, L * 0.08f, 0.f, Fins, 0.5f, 3);
	Blade(M, Centre + FVector(L * 0.22f, L * 0.03f, -L * 0.04f), FVector(-0.6f, 0.4f, -0.6f), FVector::RightVector, L * 0.1f, L * 0.035f, 0.f, Fins, 0.5f, 3);
	Ball(M, Centre + FVector(L * 0.4f, L * 0.035f, L * 0.03f), FVector::RightVector, L * 0.02f, L * 0.016f, FLinearColor(0.02f, 0.02f, 0.02f), 0.1f, 6);
}

void FWRFarmLook::Refresh(AWRBuildPiece* P)
{
	const FWRPieceDef* Def = P ? P->GetDef() : nullptr;
	if (!Def) return;
	const FName K = Def->Kind;
	if (Def->bGlass)
	{
		// Glazing: one sheet of glass behind the mullions (a wall's between knee wall and top rail, a roof's along its
		// slope between the rafters), in the clear icy material.
		if (!P->WaterSurface)
		{
			P->WaterSurface = NewObject<UStaticMeshComponent>(P);
			P->WaterSurface->SetupAttachment(P->GetRootComponent());
			P->WaterSurface->RegisterComponent();
			P->WaterSurface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			P->WaterSurface->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
			if (UMaterialInterface* Glass = LoadObject<UMaterialInterface>(nullptr, *FWRTuning::Get().Str(TEXT("farm.glassMaterial"), TEXT("/Game/Terrain/M_Ice.M_Ice")))) P->WaterSurface->SetMaterial(0, Glass);
			P->WaterSurface->SetCastShadow(false);
			if (Def->Shape == TEXT("roof"))
			{
				const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(200.f, 400.f));
				P->WaterSurface->SetRelativeLocationAndRotation(FVector(-15.f, 0.f, 112.f), FRotator(Pitch, 0.f, 0.f));
				P->WaterSurface->SetRelativeScale3D(FVector(4.9f, 4.14f, 0.015f));
			}
			else
			{
				P->WaterSurface->SetRelativeLocation(FVector(0.f, 0.f, 180.f));
				P->WaterSurface->SetRelativeScale3D(FVector(4.14f, 0.015f, 2.3f));
			}
		}
		return;
	}
	const bool bFarmish = Def->IsFarm() || !Def->Trophy.IsNone();
	if (!bFarmish) return;

	// What it shows now, hashed: nothing to do when that's unchanged.
	uint32 Key = GetTypeHash(P->CropId) ^ (uint32(P->Growth8 / 8) << 8) ^ (uint32(P->FarmFlags) << 20) ^ (uint32(P->Growth8 >= 255) << 30);
	for (const FWRItemStack& S : P->Storage) Key = HashCombine(Key, S.IsEmpty() ? 0u : HashCombine(GetTypeHash(S.ItemId), uint32(FMath::Min(S.Count, 12))));
	Key = HashCombine(Key, uint32(FMath::Min(P->Feed / 10, 6)));
	if (Key == P->FarmLookKey && P->FarmMesh) return;
	P->FarmLookKey = Key;

	if (!P->FarmMesh)
	{
		P->FarmMesh = NewObject<UProceduralMeshComponent>(P);
		P->FarmMesh->SetupAttachment(P->GetRootComponent());
		P->FarmMesh->RegisterComponent();
		P->FarmMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	FWRToolMesh M;
	const float Seed = FMath::Frac(P->GetActorLocation().X * 0.0137f + P->GetActorLocation().Y * 0.0091f) * 97.f;
	const FLinearColor Straw(0.42f, 0.32f, 0.12f), Wood(0.2f, 0.12f, 0.06f), Clay(0.32f, 0.16f, 0.08f);

	if (K == TEXT("crop"))
	{
		const bool bAsh = Def->Plot == TEXT("ashpit");
		const float SoilZ = bAsh ? 4.f : 29.f;   // the bed's furrow tops / the pit's ash floor
		// Wet soil darkens the bed (watered), a mulch ring under each plant (fertilised).
		if (P->FarmFlags & WRFarmFlag::Watered)
		{
			for (int32 r = 0; r < 6; ++r)
			{
				const float Y = -150.f + r * 60.f;
				M.Lathe(FVector(-180.f, Y, SoilZ), FVector::ForwardVector, 360.f, { FVector(0, 15.5f, 0.6f), FVector(1, 15.5f, 0.6f) }, 4, bAsh ? FLinearColor(0.03f, 0.03f, 0.03f) : FLinearColor(0.025f, 0.016f, 0.01f), 0.25f);
			}
		}
		const FWRCropDef* Crop = FWRFarmDefs::Get().Crop(P->CropId);
		const float G = P->Growth8 / 255.f;
		for (int32 i = 0; i < 9; ++i)
		{
			const int32 X = i % 3 - 1, Y = i / 3 - 1;
			const FVector Base(X * (bAsh ? 95.f : 120.f) + (H(Seed, i) - 0.5f) * 30.f, Y * (bAsh ? 95.f : 120.f) + (H(Seed, i + 50) - 0.5f) * 30.f, SoilZ);
			if (P->FarmFlags & WRFarmFlag::Fertilised)
			{
				M.Lathe(Base - FVector(0, 0, 0.5f), FVector::UpVector, 1.6f, { FVector(0, 18.f, 18.f), FVector(1, 14.f, 14.f) }, 10, FLinearColor(0.05f, 0.03f, 0.015f), 0.95f);
			}
			if (Crop) Plant(M, *Crop, G, Base, H(Seed, i + 100) * 360.f, Seed + i);
		}
	}
	else if (K == TEXT("pen"))
	{
		// The shelf by the gate (+X side, inside the fence): a plank on two posts, what waits on it; the trough on the
		// -X side shows its feed as a heap of fodder.
		const FVector Shelf(330.f, 260.f, 80.f);
		int32 Eggs = 0, Wool = 0, Fat = 0, Other = 0;
		for (const FWRItemStack& S : P->Storage)
		{
			if (S.IsEmpty()) continue;
			const FString Id = S.ItemId.ToString();
			if (Id.Contains(TEXT("egg"))) Eggs += S.Count;
			else if (Id.Contains(TEXT("wool")) || Id.Contains(TEXT("feather")) || Id.Contains(TEXT("fur"))) Wool += S.Count;
			else if (Id.Contains(TEXT("fat")) || Id.Contains(TEXT("milk")) || Id.Contains(TEXT("slime"))) Fat += S.Count;
			else Other += S.Count;
		}
		// The nest box: a straw-lined crate with the eggs in it.
		M.Lathe(Shelf + FVector(-25.f, 0.f, 0.f), FVector::UpVector, 12.f, { FVector(0, 18.f, 18.f), FVector(1, 18.f, 18.f) }, 4, Wood, 0.85f);
		M.Lathe(Shelf + FVector(-25.f, 0.f, 10.f), FVector::UpVector, 3.f, { FVector(0, 15.f, 15.f), FVector(1, 13.f, 13.f) }, 10, Straw, 0.95f);
		for (int32 e = 0; e < FMath::Min(Eggs, 8); ++e) Ball(M, Shelf + FVector(-25.f + (e % 3 - 1) * 7.f, (e / 3 - 1) * 7.f, 15.f), FVector(0.1f, 0.f, 1.f), 6.f, 2.2f, FLinearColor(0.75f, 0.68f, 0.55f), 0.35f, 8);
		for (int32 w = 0; w < FMath::Min(Wool, 6); ++w) Ball(M, Shelf + FVector(10.f + (w % 3) * 13.f, (w / 3) * 14.f - 7.f, 6.f), FVector::ForwardVector, 14.f, 6.f, FLinearColor(0.7f, 0.66f, 0.58f), 0.95f, 8);
		if (Fat > 0) M.Lathe(Shelf + FVector(45.f, -20.f, 0.f), FVector::UpVector, 18.f, { FVector(0, 6.f, 6.f), FVector(0.6f, 8.f, 8.f), FVector(0.85f, 5.f, 5.f), FVector(1, 5.5f, 5.5f) }, 10, Clay, 0.6f);
		for (int32 o = 0; o < FMath::Min(Other, 4); ++o) Ball(M, Shelf + FVector(-55.f + o * 9.f, 20.f, 5.f), FVector::UpVector, 7.f, 4.f, FLinearColor(0.4f, 0.4f, 0.42f), 0.7f, 6);
		const float FeedT = FMath::Clamp(P->Feed / FMath::Max(1.f, WRTune(TEXT("farm.pen.trough"), 50.f)), 0.f, 1.f);
		if (FeedT > 0.f) M.Lathe(FVector(-335.f, -150.f, 62.f), FVector::ForwardVector, 150.f, { FVector(0, 12.f, 3.f + 7.f * FeedT), FVector(0.5f, 14.f, 4.f + 9.f * FeedT), FVector(1, 12.f, 3.f + 7.f * FeedT) }, 8, FLinearColor(0.32f, 0.3f, 0.12f), 0.95f);
	}
	else if (K == TEXT("hive"))
	{
		// Honeycomb slabs leaning on the stand while honey waits; wax as pale cakes.
		int32 Honey = 0, Wax = 0;
		for (const FWRItemStack& S : P->Storage) { if (S.ItemId == TEXT("honey")) Honey += S.Count; else if (!S.IsEmpty()) Wax += S.Count; }
		for (int32 h = 0; h < FMath::Min(Honey, 5); ++h) M.Lathe(FVector(-30.f + h * 8.f, 38.f, 62.f), FVector(0.f, -0.25f, 1.f).GetSafeNormal(), 26.f, { FVector(0, 1.5f, 12.f), FVector(1, 1.5f, 10.f) }, 4, FLinearColor(0.6f, 0.32f, 0.03f), 0.2f);
		for (int32 w = 0; w < FMath::Min(Wax, 4); ++w) M.Lathe(FVector(18.f + w * 9.f, 38.f, 62.f), FVector::UpVector, 4.f, { FVector(0, 4.f, 4.f), FVector(1, 4.f, 4.f) }, 8, FLinearColor(0.75f, 0.62f, 0.32f), 0.6f);
	}
	else if (K == TEXT("trap"))
	{
		// The catch silvering inside the willow basket.
		int32 N = 0;
		for (const FWRItemStack& S : P->Storage) N += S.IsEmpty() ? 0 : S.Count;
		for (int32 f = 0; f < FMath::Min(N, 6); ++f)
		{
			Fish(M, FVector(-10.f + (f % 3) * 8.f, (f / 3) * 10.f - 5.f, 18.f + f * 3.f), 22.f, FLinearColor(0.2f, 0.24f, 0.22f), FLinearColor(0.6f, 0.62f, 0.58f), FLinearColor(0.3f, 0.3f, 0.26f));
		}
	}
	else if (K == TEXT("saltpan"))
	{
		// The crust whitening the bed as salt waits, raked into a heap at the end.
		int32 N = 0;
		for (const FWRItemStack& S : P->Storage) N += S.IsEmpty() ? 0 : S.Count;
		const float T = FMath::Clamp(N / 10.f, 0.f, 1.f);
		M.Lathe(FVector(-130.f, 0.f, 9.f), FVector::ForwardVector, 260.f, { FVector(0, 85.f, 0.6f), FVector(1, 85.f, 0.6f) }, 4, Mix(FLinearColor(0.22f, 0.26f, 0.28f), FLinearColor(0.85f, 0.85f, 0.82f), 0.3f + 0.7f * T), 0.4f);
		if (N > 0) Ball(M, FVector(110.f, 0.f, 12.f), FVector::UpVector, 6.f + 10.f * T, 8.f + 16.f * T, FLinearColor(0.9f, 0.9f, 0.88f), 0.5f, 10);
	}
	else if (!Def->Trophy.IsNone())
	{
		// The legend on its board, colours from fish.json.
		static TMap<FName, TArray<FLinearColor>> Colours;
		if (Colours.Num() == 0)
		{
			if (const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("fish.json")))
			{
				const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
				if (Root->TryGetArrayField(TEXT("fish"), List))
				{
					for (const TSharedPtr<FJsonValue>& V : *List)
					{
						const TSharedPtr<FJsonObject> O = V->AsObject();
						const TSharedPtr<FJsonObject>* Look = nullptr;
						if (!O.IsValid() || !O->TryGetObjectField(TEXT("look"), Look)) continue;
						TArray<FLinearColor>& C = Colours.Add(FName(*O->GetStringField(TEXT("id"))));
						for (const TCHAR* Part : { TEXT("back"), TEXT("belly"), TEXT("fins") })
						{
							const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
							C.Add((*Look)->TryGetArrayField(Part, A) && A->Num() >= 3 ? FLinearColor(FColor(uint8((*A)[0]->AsNumber()), uint8((*A)[1]->AsNumber()), uint8((*A)[2]->AsNumber()))) : FLinearColor(0.3f, 0.3f, 0.3f));
						}
					}
				}
			}
		}
		const TArray<FLinearColor>* C = Colours.Find(Def->Trophy);
		Fish(M, FVector(0.f, 9.f, 150.f), 110.f, C ? (*C)[0] : FLinearColor(0.2f, 0.2f, 0.2f), C ? (*C)[1] : FLinearColor(0.6f, 0.6f, 0.6f), C ? (*C)[2] : FLinearColor(0.3f, 0.3f, 0.3f));
	}
	M.ApplyTo(P->FarmMesh);
	P->FarmMesh->SetCastShadow(K == TEXT("crop") || !Def->Trophy.IsNone());

	// Water you can see: in the well's shaft, along a running channel (the lake material; ice if it's missing).
	if (K == TEXT("well") || K == TEXT("channel"))
	{
		if (!P->WaterSurface)
		{
			P->WaterSurface = NewObject<UStaticMeshComponent>(P);
			P->WaterSurface->SetupAttachment(P->GetRootComponent());
			P->WaterSurface->RegisterComponent();
			P->WaterSurface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			P->WaterSurface->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")));
			UMaterialInterface* Water = LoadObject<UMaterialInterface>(nullptr, *FWRTuning::Get().Str(TEXT("farm.waterMaterial"), TEXT("/Game/WaterMaterials/Materials/M_Lake.M_Lake")));
			if (!Water) Water = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_Ice.M_Ice"));
			if (Water) P->WaterSurface->SetMaterial(0, Water);
			P->WaterSurface->SetCastShadow(false);
		}
		if (K == TEXT("well"))
		{
			P->WaterSurface->SetRelativeLocation(FVector(0.f, 0.f, 22.f));
			P->WaterSurface->SetRelativeScale3D(FVector(0.62f, 0.62f, 1.f));
		}
		else
		{
			P->WaterSurface->SetRelativeLocation(FVector(0.f, 0.f, 20.f));
			P->WaterSurface->SetRelativeScale3D(FVector(3.96f, 0.44f, 1.f));
		}
		P->WaterSurface->SetVisibility((P->FarmFlags & WRFarmFlag::Watered) != 0);
	}
}
