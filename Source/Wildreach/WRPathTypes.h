#pragma once

#include "CoreMinimal.h"
#include "WRPathTypes.generated.h"

UENUM()
enum class EWRNodeKind : uint8
{
	Stat,
	Ability,
	Capstone,
};

/** One node of a skill tree, as written in Data/paths/<id>.json. */
struct FWRPathNodeDef
{
	FName Id;
	FString Name;
	EWRNodeKind Kind = EWRNodeKind::Ability;
	/** Points per rank (stats: 1) */
	int32 Cost = 1;
	int32 MaxRank = 1;
	/** Points already spent in this branch before the node can be bought (design review #8). */
	int32 Gate = 0;
	/** None = trunk. */
	FName Branch;
	FString Text;
};

struct FWRPathDef
{
	FName Id;
	FString Name;
	FName Biome;
	FName ResourceId;
	FString RootPassiveName;
	TArray<FWRPathNodeDef> Nodes;

	const FWRPathNodeDef* FindNode(FName NodeId) const;
	bool IsValid() const { return !Id.IsNone(); }
};

/** All path definitions loaded from Data/paths/*.json. */
class WILDREACH_API FWRPathRegistry
{
public:
	static FWRPathRegistry& Get();
	const FWRPathDef* Find(FName PathId) const;
	const TArray<FWRPathDef>& All() const { return Paths; }
	void Reload();

	/** Parse one tree from JSON text (public for tests). */
	static bool ParsePath(const FString& JsonText, FWRPathDef& Out);

private:
	FWRPathRegistry();
	TArray<FWRPathDef> Paths;
};

USTRUCT()
struct FWRNodeRank
{
	GENERATED_BODY()

	UPROPERTY() FName NodeId;
	UPROPERTY() int32 Rank = 0;
};

/** One character's saved progress in one path. Kept when the path is swapped out. */
USTRUCT()
struct FWRPathProgress
{
	GENERATED_BODY()

	UPROPERTY() FName PathId;
	UPROPERTY() int32 XP = 0;
	UPROPERTY() TArray<FWRNodeRank> Ranks;

	int32 GetRank(FName NodeId) const;
	void SetRank(FName NodeId, int32 Rank);
};

/** Pure progression rules (no world needed) so automation tests can cover them. */
struct WILDREACH_API FWRPathRules
{
	/** XP cost of point number N (1-based): base + perPoint x N. */
	static int32 PointCost(int32 N);
	/** Points earned from XP, capped at the per-path max. */
	static int32 PointsForXP(int32 XP);
	/** Total XP needed to earn this many points. */
	static int32 XPForPoints(int32 Points);

	static int32 Spent(const FWRPathDef& Def, const FWRPathProgress& Progress);
	static int32 SpentInBranch(const FWRPathDef& Def, const FWRPathProgress& Progress, FName Branch);
	static bool TrunkComplete(const FWRPathDef& Def, const FWRPathProgress& Progress);

	/** Empty string when the node can be bought; otherwise the reason it can't. */
	static FString CanBuy(const FWRPathDef& Def, const FWRPathProgress& Progress, int32 OtherPathSpent, FName NodeId);

	/** Character level = total path points spent / 3, 1..50 (design review #36). */
	static int32 CharacterLevel(int32 TotalSpent);
	/** Linear growth from the level-1 value to exactly the level-50 value (design review #35). */
	static float LevelStat(float AtLevel1, float AtLevel50, int32 Level);
};
