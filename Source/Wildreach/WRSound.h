#pragma once

#include "CoreMinimal.h"

class AActor;
class UObject;

/**
 * Sound events by name. Gameplay calls these where something happens; which files play (if any), how loud and how far
 * they carry is the sound table's business (Data/Audio/sounds.json, files beside it), so code never names a file.
 * Unknown events are silent and logged once. Nothing plays on a dedicated server. Event names (area.thing, lower case):
 *   player   footstep (surface picked from the ground: footstep.sand / snow / stone / wood / grass / gravel / water),
 *            swing.light, swing.heavy, hit.flesh, hit.armor, hit.block, parry, dodge, jump, land, hurt, death, eat,
 *            drink, potion, pickup, equip (voices: hurt / death / jump take a ".female" variant for the female body)
 *   ranged   bow.draw, bow.release, crossbow.fire, blowgun.fire, throw, arrow.hit
 *   spells   cast.fire, cast.frost, fire.impact, frost.impact, firewall, lavapool, frostnova, icelance
 *   world    gather.wood, gather.stone, gather.fiber, build.place, build.demolish, build.invalid, door.open, door.close,
 *            craft.done, hearth.feed
 *   moments  levelup, recipe.unlock, discover, boss.intro, tame.success, respawn
 *   creature creature.<id>.idle | aggro | attack | hurt | death; falls back to creature.<size>.<family>.<kind>, then
 *            creature.<family>.<kind>, then creature.<size>.<kind> (size small / medium / large from the capsule,
 *            family from the archetype: canine, beast, flyer, bug, herd, critter, boss, wisp)
 *   ui       ui.hover, ui.click, ui.open, ui.close, ui.error, ui.drag, ui.drop
 * Ambient beds (by biome, time of day, coast, caves) and music (menu, exploring, combat, boss) run on their own from
 * the same table (UWRAudioDirector). Dev: `wr.SoundLog 1` (or -wrsoundlog) logs every play with its file and volume,
 * `WRSoundPlay <event>` plays one, `WRSoundStats` lists what played and what the minimum gap held back.
 */
namespace WRSound
{
	/** A one-shot in the world at a location. */
	WILDREACH_API void PlayAt(const UObject* WorldContext, FName Event, const FVector& Location, float VolumeScale = 1.f);
	/** A one-shot that follows an actor (creature calls, a swing). */
	WILDREACH_API void PlayOn(AActor* Actor, FName Event, float VolumeScale = 1.f);
	/** A footstep for this actor, the surface picked from what it stands on. */
	WILDREACH_API void Footstep(AActor* Actor, float VolumeScale = 1.f);
	/** 2D, for the local player only (UI, moments). A null context uses the game viewport's world (Slate widgets). */
	WILDREACH_API void PlayUI(const UObject* WorldContext, FName Event);

	/** A character's own voice: Kind = idle, aggro, attack, hurt, death (creatures, resolved as above) or hurt, death,
	 *  jump (survivors). Silent when the table has nothing for it. */
	WILDREACH_API void Voice(AActor* Actor, FName Kind, float VolumeScale = 1.f);
	/** Server code: play on this actor on every machine that has it (a character: through its multicast). Plays
	 *  locally on a listen server or standalone too. */
	WILDREACH_API void Broadcast(AActor* Actor, FName Event);

	/** Whether the table has this event. */
	WILDREACH_API bool Has(FName Event);
	/** The surface under an actor's feet (sand, snow, stone, wood, grass, gravel, water). */
	WILDREACH_API FName SurfaceUnder(const AActor* Actor);

	/** Player volume settings, 0..1 (Settings > Audio; saved in GameUserSettings.ini). Category: master, music,
	 *  effects, ambient, ui. */
	WILDREACH_API float GetVolume(FName Category);
	/** bSave = false while a slider is being dragged (saved when it's let go). */
	WILDREACH_API void SetVolume(FName Category, float Value, bool bSave = true);
	WILDREACH_API void ResetVolumes();
	/** Final gain for a category: player setting x master x the table's mix trim. */
	WILDREACH_API float CategoryGain(FName Category);
}
