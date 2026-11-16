#pragma once

#include "CoreMinimal.h"

struct FWRToolMesh;

/**
 * What a food, drink or potion looks like (cooks/cookpass-survival.md 3.1, 3.4; Data/food_look.json): modelled in code
 * like the held tools (FWRToolMesh: lathe-turned and lofted parts, vertex colour into M_HeldTool). Used for the food in
 * the hand while it's eaten or drunk (AWRCharacter::TickConsumeLook), the meat or fish turning on a campfire's spit and
 * what waits on a kitchen's hearth stone (AWRStation's kitchen show).
 */
struct WILDREACH_API FWRFoodLook
{
	FName Shape;                          // meat, fish, bowl, loaf, berries, fruit, skewer, egg, cake, strip, mushroom, waterskin, jar, bottle
	FLinearColor Colour = FLinearColor(0.6f, 0.35f, 0.15f);
	bool bLong = false;                   // fruit: a root or pod rather than a round fruit
	bool bFlat = false;                   // loaf: a flatbread / omelette
	bool bPie = false;                    // loaf: a lidded pie
	bool bFishColours = false;            // fish: its fish.json back / belly / fins
	FLinearColor Back, Belly, Fins;

	/** The look of an item (exact row, then a word in its id, then its category). */
	static const FWRFoodLook& For(FName Item);
	/** Raw meat or fish colour (it browns to its cooked colour on the spit). */
	static FLinearColor Raw(FName Shape);
	/** Shapes that lie along X (held across, hung on a spit); the rest stand on Z. */
	static bool IsAlongX(FName Shape);
	/** Builds it into M centred on At (X along, Z up), Cooked 0 (raw) .. 1 (as it's eaten), Scale x its real size. */
	static void Build(FWRToolMesh& M, const FWRFoodLook& Look, const FVector& At, float Cooked = 1.f, float Scale = 1.f);
};
