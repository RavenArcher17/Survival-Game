#pragma once

#include "CoreMinimal.h"

class AWRPlayerController;
class UWorld;

/**
 * Creature raids on land claims: the world's "Creature raids" setting (server.raids: off / rare / normal / frequent),
 * numbers in tuning.json "raid".
 *  - When: every claim totem has the game hour its next raid is due (AWRClaimTotem::NextRaidHours, saved): one interval
 *    (raid.rareHours / normalHours / frequentHours, give or take raid.jitter) after it's first seen, then after each raid.
 *    A raid that's due waits until someone of the claim (its owner or their clan) is within raid.homeRange of the totem:
 *    creatures only exist near players, so nobody's base is raided while they're away.
 *  - The warning: everyone of the claim who's on gets a banner and a notice (what, how many, from which way), and
 *    raid.warnSeconds later they come, from raid.spawnDistance outside the claim's edge.
 *  - Who: the hunters of the claim's region (Data/spawn_regions.json "raiders"), raid.points + raid.pointsPerHearthLevel
 *    x the claim's Hearth level worth, each costing its tier's raid.cost.
 *  - What they do: break the claim's pieces and fight anyone near (AWRCreatureAI::TickRaid). It's over when they're all
 *    dead or gone, after raid.maxMinutes, or when nobody of the claim is on any more: the rest walk off and are taken away.
 * A raid under way isn't saved: a game that ends mid-raid ends it (the claim's next one is still due).
 */
namespace WRRaids
{
	/** Server, at the start of each game. */
	WILDREACH_API void Start(UWorld* World);
	/** Server, every raid.stepSeconds. */
	WILDREACH_API void Step(UWorld* World);
	/** Game hours between a claim's raids at the world's setting (0: raids are off). */
	WILDREACH_API double IntervalHours();
	/** What a raider costs: its tier's raid.cost (at least 1). */
	WILDREACH_API int32 CostOf(FName CreatureId);
	/**
	 * What a raid is made of: kinds rolled by weight until the points are spent (a kind that costs more than is left
	 * isn't rolled), each with how many. Pure, for tests.
	 */
	WILDREACH_API TArray<TPair<FName, int32>> Plan(const TArray<TPair<FName, float>>& Kinds, int32 Points, FRandomStream& Rand);
	/** Testing (WRCmd raid ...), on the claim nearest the player: now [warn seconds] [from bearing, compass degrees] | due
	 *  (its raid is due now, as if its time had come) | status | plan (each Hearth level's raid there) | end. What happened. */
	WILDREACH_API FString Command(AWRPlayerController* PC, const TArray<FString>& Args);
}
