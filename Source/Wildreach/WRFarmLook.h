#pragma once

#include "CoreMinimal.h"

class AWRBuildPiece;
struct FWRCropDef;
struct FWRToolMesh;

/**
 * How the farm looks (cooks/build-farming.md 2.2-2.7), modelled in code like the held tools (FWRToolMesh: lathed
 * stems and fruit, lofted leaf blades, vertex colour into M_HeldTool), drawn on every machine but a dedicated server:
 *  - a plot's nine plants in their crop's form (bush, vine on a stake, root rosette, grain tufts, flowering herb,
 *    cactus pads, young palm, orchard sapling, low berry, melon vine, forced stalks, mushrooms, kelp ribbons), four
 *    stages (sprout, leaf, flower, ripe: fruit colouring in from green), dark wet soil when watered, a mulch ring
 *    when fertilised;
 *  - what waits on a pen's shelf (eggs in the nest box, bundles of wool / feathers, a crock of fat), a hive's combs,
 *    a trap's catch, a salt pan's crust;
 *  - the water in a well and a running irrigation channel;
 *  - a legendary fish on its trophy board.
 */
struct WILDREACH_API FWRFarmLook
{
	/** Redraw what the piece's replicated farm state shows (cheap when nothing it shows changed). */
	static void Refresh(AWRBuildPiece* Piece);
	/** One plant of a crop at this growth (0..1), standing at Base turned Yaw, into Mesh. Seed varies it. */
	static void Plant(FWRToolMesh& Mesh, const FWRCropDef& Crop, float Growth, const FVector& Base, float Yaw, float Seed);
	/** A fish on its side along +X (nose at +X), Length cm, in its colours (back, belly, fins). */
	static void Fish(FWRToolMesh& Mesh, const FVector& Centre, float Length, const FLinearColor& Back, const FLinearColor& Belly, const FLinearColor& Fins);
};
