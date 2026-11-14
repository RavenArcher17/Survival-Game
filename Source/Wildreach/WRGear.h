#pragma once

// Gear beyond the four stations' recipes (cooks/build-crafting.md, survival-crafting 2.13 / 2.14): the biome armour
// sets and their bonuses, rings, salvage, and blueprints. Numbers live in Data/gear.json.

#include "CoreMinimal.h"

class AActor;
class AWRPlayerState;
class UWRInventoryComponent;
struct FWRItemDef;
struct FWRItemStack;

/** A set bonus: from this many intact pieces worn, these stats (UWRInventoryComponent::GearStat) and this line. */
struct FWRSetBonus
{
	int32 Pieces = 2;
	TMap<FName, float> Stats;
	FString Text;
};

/** A biome armour set (gear.json sets). */
struct FWRSetDef
{
	FName Id;
	FString Name;
	FName Biome;
	FName Load;
	TArray<FWRSetBonus> Bonuses;   // fewest pieces first
};

/** A blueprint (gear.json blueprints.list): the item that teaches, its recipes, the boss that drops it. */
struct FWRBlueprintDef
{
	FName Id;
	FName Boss;
	FString Kind;
	TArray<FName> Recipes;
};

class WILDREACH_API FWRGear
{
public:
	static const FWRGear& Get();

	const FWRSetDef* FindSet(FName Id) const { return Sets.FindByPredicate([Id](const FWRSetDef& S) { return S.Id == Id; }); }
	const TArray<FWRSetDef>& AllSets() const { return Sets; }
	const FWRBlueprintDef* FindBlueprint(FName Id) const { return Blueprints.FindByPredicate([Id](const FWRBlueprintDef& B) { return B.Id == Id; }); }
	const TArray<FWRBlueprintDef>& AllBlueprints() const { return Blueprints; }

	// rings
	float RingShare = 0.6f;
	float RingPerTier = 0.1f;
	TMap<FName, int32> MetalTier;
	// salvage
	float SalvageBase = 0.4f;
	float SalvagePerQuality = 0.1f;
	float SalvageShards = 0.25f;
	int32 CoreShards = 10;
	int32 CoreLevel = 5;
	float SalvageXP = 5.f;
	// blueprints
	float RepeatChance = 0.2f;
	TArray<float> PortalOdds;
	int32 LegendaryFromRank = 4;

	/**
	 * What breaking this piece of gear down gives back: of its recipe's metal (ingots, plates, star iron) and its main
	 * input, base + perQuality x quality step (rounded down, at least 1); a quarter of the upgrade shards spent. OutLost
	 * gets what's destroyed: its infusion and every extra in its sockets.
	 */
	static TArray<TPair<FName, int32>> SalvageReturns(const FWRItemStack& Item, TArray<FString>* OutLost = nullptr);

private:
	FWRGear();
	TArray<FWRSetDef> Sets;
	TArray<FWRBlueprintDef> Blueprints;
};

/** Reading blueprints and where they drop (server). */
struct WILDREACH_API FWRBlueprints
{
	/** Has this survivor (a pawn, controller or player state) read it? */
	static bool Knows(const AActor* Who, FName Blueprint);
	/** RMB on a blueprint in the bag: learns it for good and uses it up (already known: kept, to hand to a clan mate). */
	static FString Learn(UWRInventoryComponent* Inv, int32 SlotIndex);
	/** A major boss's drop for this looter: its first kill always, later kills blueprints.repeatChance. A blueprint
	 *  they don't know or carry yet if there is one. None: no drop this time. */
	static FName BossDrop(const AWRPlayerState* PS, FName BossId, bool bFirstKill);
	/** A portal boss of this rank (1..5) and this land: blueprints.portalOdds; legendary ones from legendaryFromRank.
	 *  For the portals when they come. */
	static FName PortalDrop(const AWRPlayerState* PS, int32 Rank);
};
