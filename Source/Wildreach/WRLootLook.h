#pragma once

#include "CoreMinimal.h"
#include "WRToolMesh.h"

/**
 * What a dropped bag looks like (cooks/cookpass-multiplayer.md 3.5; the sphere it replaces was a primitive). Each is one
 * runtime mesh from the tool kit (FWRToolMesh lathe, loft and tube; M_HeldTool reads colour and roughness from the
 * vertex colour), in cm, standing on its origin (Z up, the ground at 0), the long side along X.
 *   Satchel   a death's belongings: a rolled fur bedroll (the roll's layers spiral at its ends) with two leather straps
 *             and bronze buckles, one strap fallen loose across the ground; a hide satchel slumped against it, its flap
 *             over the top and front, shut by a bone toggle; a cord-tied pouch and a stoppered waterskin spilled beside.
 *   Crate     a broken chest's contents: a nailed board crate, its front stove in (two boards fallen out, one leaning),
 *             its lid knocked off beside it, a burlap sack slumped against it and a few lumps rolled out in front.
 *   Sack      anything else dropped (loot, what didn't fit): a hide sack tied at the neck with cord, its top flopped
 *             over, and a small rolled fur bundle beside it.
 */
namespace WRLootLook
{
	WILDREACH_API FWRToolMesh Satchel();
	WILDREACH_API FWRToolMesh Crate();
	WILDREACH_API FWRToolMesh Sack();
	/** The owner's column of light over their satchel: a soft tapering shaft BeaconHeight tall (its colour comes from the
	 *  glow material), open at the top. */
	WILDREACH_API FWRToolMesh Beacon(float Height);
}
