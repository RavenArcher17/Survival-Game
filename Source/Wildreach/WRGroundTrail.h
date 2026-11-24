// Worn ground: trails and trodden patches drawn over a heightfield land in the land's own ground material (round 13 of
// the look pass: the villages stood on bare ash and snow, no way worn between the doors and the fire). A trail is a
// ribbon lying a few centimetres over the land; its middle takes the trail's colour and layer mix, its edges the
// land's own (from the same samples the land draws), so it fades into the ground with no edge. Nothing is drawn where
// no heightfield land is live (the Old Island's landscape).
#pragma once

#include "CoreMinimal.h"

class AActor;
class USceneComponent;
class UProceduralMeshComponent;

/** How a worn way looks at its middle. */
struct FWRTrailLook
{
	/** Linear colour (the land's vertex colour: the ground material multiplies its textures by it). */
	FLinearColor Colour = FLinearColor(0.05f, 0.045f, 0.04f);
	/** The ground material's six layers: litter, moss, soil, grass, stones, sand. */
	float Layers[6] = { 0.f, 0.f, 0.6f, 0.f, 0.3f, 0.1f };
	float Rough = 0.9f;
	/** How fully the trail replaces the ground along its middle (0..1). */
	float Strength = 1.f;

	/** Trodden ash and earth (the Volcanic's villages). */
	static FWRTrailLook Ash();
	/** Packed, dirtied snow (the Tundra's camps and villages). */
	static FWRTrailLook Snow();
};

namespace WRGroundTrail
{
	/** A worn way through world points (cm, at least two; smoothed through them), HalfWidth cm either side of its middle
	 *  (varying a little along it). Attached to Parent. Null where no heightfield land is live. */
	WILDREACH_API UProceduralMeshComponent* Path(AActor* Owner, USceneComponent* Parent, const TArray<FVector2D>& WorldPts, float HalfWidth, const FWRTrailLook& Look, int32 Seed);
	/** A trodden patch round a world point: full within Inner cm, fading out by Outer (its edge ragged). */
	WILDREACH_API UProceduralMeshComponent* Patch(AActor* Owner, USceneComponent* Parent, const FVector2D& WorldCentre, float Inner, float Outer, const FWRTrailLook& Look, int32 Seed);
}
