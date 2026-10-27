#pragma once

#include "CoreMinimal.h"

class UObject;
class USceneComponent;

/**
 * Visual effects by name, like WRSound: gameplay names the moment, the effects pass decides what it looks like. Unknown
 * effects draw nothing and are logged once. Nothing spawns on a dedicated server. Effect names:
 *   combat   hit.spark (metal, block), hit.blood (flesh), parry.flash, dodge.dust, land.dust, death.poof
 *   spells   fire.bolt, frost.shard (attached trails), fire.impact, frost.impact, firewall, lavapool, frostnova, icelance,
 *            cast.fire, cast.frost (hand glow)
 *   world    gather.wood, gather.stone, gather.fiber, build.place, build.break, hearth.fire, lava.embers, torch.fire
 *   weather  snow, ash (attached to the local camera)
 *   moments  levelup, tame.success, discover
 */
namespace WRFx
{
	/** A one-shot effect at a place. */
	WILDREACH_API void SpawnAt(const UObject* WorldContext, FName Effect, const FVector& Location, const FRotator& Rotation = FRotator::ZeroRotator, float Scale = 1.f);
	/** A looping effect attached to a component (a bolt's trail, Hearth flames); keep it to stop it later. May be null. */
	WILDREACH_API USceneComponent* Attach(USceneComponent* Parent, FName Effect, FName Socket = NAME_None, float Scale = 1.f);
	/** Stops (and removes) an effect from Attach. */
	WILDREACH_API void Stop(USceneComponent* Fx);
}
