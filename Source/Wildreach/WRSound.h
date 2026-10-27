#pragma once

#include "CoreMinimal.h"

class AActor;
class UObject;

/**
 * Sound events by name. Gameplay calls these where something happens; which files play (if any), how loud and how far
 * they carry is the sound table's business (Data/Audio), so code never names a file. Unknown events are silent and
 * logged once. Nothing plays on a dedicated server. Event names (area.thing, lower case):
 *   player   footstep (surface picked from the ground), swing.light, swing.heavy, hit.flesh, hit.armor, hit.block, parry,
 *            dodge, jump, land, hurt, death, eat, drink, potion, pickup, equip
 *   ranged   bow.draw, bow.release, crossbow.fire, blowgun.fire, throw, arrow.hit
 *   spells   cast.fire, cast.frost, fire.impact, frost.impact, firewall, lavapool, frostnova, icelance
 *   world    gather.wood, gather.stone, gather.fiber, build.place, build.demolish, build.invalid, door.open, door.close,
 *            craft.done, hearth.feed
 *   moments  levelup, recipe.unlock, discover, boss.intro, tame.success, respawn
 *   creature creature.<id>.idle | aggro | attack | hurt | death (falls back to creature.<size>.<kind>), stomp (a slam on the ground)
 *   ui       ui.hover, ui.click, ui.open, ui.close, ui.error, ui.drag, ui.drop
 */
namespace WRSound
{
	/** A one-shot in the world at a location. */
	WILDREACH_API void PlayAt(const UObject* WorldContext, FName Event, const FVector& Location, float VolumeScale = 1.f);
	/** A one-shot that follows an actor (creature calls, a swing). */
	WILDREACH_API void PlayOn(AActor* Actor, FName Event, float VolumeScale = 1.f);
	/** A footstep for this actor, the surface picked from what it stands on. */
	WILDREACH_API void Footstep(AActor* Actor, float VolumeScale = 1.f);
	/** 2D, for the local player only (UI, moments). */
	WILDREACH_API void PlayUI(const UObject* WorldContext, FName Event);
}
