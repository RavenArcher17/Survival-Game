#pragma once

#include "CoreMinimal.h"

class AWRCreatureSpawner;
class UWorld;
enum class EWRPackDoing : uint8;

/**
 * The island's wildlife keeps living when nobody is near. Each pack is a roaming AWRCreatureSpawner: where it is, what
 * it's doing, how many are left and how hurt they are. A few times a minute (wildlife.stepSeconds) this moves them over a
 * coarse map of where animals can walk (Data/island_walk.json, Tools/terrain/walkmap.mjs), inside the elevation band and
 * the side of the island their kind belongs to, and clear of the camps:
 *   grazers drift between feeding grounds and travel while awake; predators keep a territory, go after prey when hungry
 *   (off screen a hunt is settled by odds) and rest when they've eaten; a few (creatures.json wanderChance) travel far;
 *   burrowers stay home.
 * A player coming within wildlife.activationRadius brings the pack out where it has got to, doing what it was doing; when
 * everyone has gone it's a record again, with its numbers and wounds.
 * Where they come from (Data/spawn_regions.json): the island is cut into regions, each with a weighted list of creatures
 * and a population target (times the world's creature numbers). A new world fills every region; after that, every
 * wildlife.spawnCheckSeconds a region short of its target gets a few new packs (wildlife.spawnPacksPerCheck), each kind
 * rolled by weight, somewhere it can live that's wildlife.spawnAwayFromPlayers from every player and
 * wildlife.spawnAwayFromBuildings from anything built, outside the land claims. A pack keeps counting for its region
 * wherever it roams; a pack that's wiped out is gone.
 * Saved in the world's folder (wildlife.json). Tames aren't part of any of this.
 */
namespace WRWildlife
{
	/** Server, at the start of a game: the world's packs from its wildlife.json, or every region filled for a new world. */
	WILDREACH_API int32 Start(UWorld* World);
	/** Server: moves every pack on by this many seconds (times wildlife.timeScale, for tests). */
	WILDREACH_API void Step(UWorld* World, float Seconds);
	/** Server: writes wildlife.json in the world's folder. */
	WILDREACH_API void Save(UWorld* World);

	/** Can this kind stand here? (Its side of the island, its band, off the sea and steep ground, clear of the camps.) */
	WILDREACH_API bool CanStand(FName CreatureId, const FVector2D& At);
	/** A way from one spot to another that only uses ground this kind can stand on (false: there is none). */
	WILDREACH_API bool FindPath(FName CreatureId, const FVector2D& From, const FVector2D& To, TArray<FVector2D>& OutPath);
	/** How many of the kind the island should have just now. */
	WILDREACH_API int32 TargetOf(FName CreatureId);
	/**
	 * Raids (WRRaids): the region a spot is in, or failing that the one on its side of the island whose band is nearest
	 * (a beach below the lowland), and that region's raiders with their weights (spawn_regions.json "raiders"; without
	 * them, its aggressive kinds). Read from the file, so it works whether or not the wildlife is running. None: no region.
	 */
	WILDREACH_API FName RaidersAt(const FVector2D& At, TArray<TPair<FName, float>>& OutRaiders);

	WILDREACH_API const TCHAR* DoingName(EWRPackDoing Doing);
}
