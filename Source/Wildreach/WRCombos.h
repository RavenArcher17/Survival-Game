#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class AActor;
class UWRPathComponent;

/** One level of a path pair's combo (Data/combos.json at25 / at50). */
struct FWRComboLevel
{
	FName Id;
	FString Name;
	FString Text;
	int32 At = 25;
	TArray<FGameplayTag> Grants;
	/** "path.node" the owner must have learned too (empty: none). */
	FName RequiresPath;
	FName RequiresNode;
	TMap<FName, float> Numbers;
};

struct FWRComboPair
{
	FName A, B;
	TArray<FWRComboLevel> Levels;
};

/**
 * Path combos (Docs/plan/DATA_SCHEMAS.md "Combo", cooks/biome-lightning.md 2.18.4): a main path and subclass with enough
 * points spent in both own the pair's combos. UWRPathComponent grants their tags; the combat code reads the tags and the
 * numbers here (Fire + Frost: Thaw, Balance; Fire + Lightning: Plasma, Meltdown; Frost + Lightning: Brittle shock,
 * Blizzard storm).
 */
class WILDREACH_API FWRComboRegistry
{
public:
	static FWRComboRegistry& Get();
	void Reload();

	/** The pair for these two paths (either order), or null. */
	const FWRComboPair* Find(FName PathA, FName PathB) const;
	/** The combo level by id ("plasma"), or null. */
	const FWRComboLevel* Level(FName ComboId) const;
	/** A combo's number (combos.json numbers.<Key>), else Fallback. */
	float Num(FName ComboId, FName Key, float Fallback) const;

	/** The tags a survivor's main path and subclass have earned (points in both, learned requirements). */
	TArray<FGameplayTag> TagsFor(const UWRPathComponent& Paths) const;

	const TArray<FWRComboPair>& All() const { return Pairs; }

private:
	FWRComboRegistry();
	TArray<FWRComboPair> Pairs;
};
