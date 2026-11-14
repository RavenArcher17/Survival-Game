#pragma once

#include "CoreMinimal.h"

class AWRPlayerController;
class FSlateWindowElementList;
struct FGeometry;

/**
 * What some life-skill perks show on your own screen, drawn in the HUD's combat layer (WRFxHud):
 *   Prospector (mining): ore veins within reach marked through rock, in their ore's colour, the distance when near.
 *   Predator sense (tracking): "Hunted" and a pip at the edge of a ring around the crosshair toward each predator
 *   that has picked you as its target.
 *   Beast lore (tracking): under what you've locked on to, what it is weak to, what it shrugs off and how it's tamed.
 * Client only: it reads replicated state (resource nodes, creatures' AggroRep, the lock-on) and the owner's life skills.
 */
namespace WRPerkHud
{
	/** Paints over Layer; returns the last layer used. Size: the overlay's size, VX/VY the viewport's (pixels). */
	int32 Paint(AWRPlayerController* PC, const FGeometry& Geo, FSlateWindowElementList& Out, int32 Layer, const FVector2D& Size, int32 VX, int32 VY);
}
