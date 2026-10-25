#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WRResourceNode.generated.h"

class AWRCharacterBase;
class UStaticMeshComponent;

/** A gathering tool from Data/items.json ("tool": {"kind": "hatchet" | "pick", "yield": 1.5}). */
struct FWRToolDef
{
	FName ItemId;
	FName Kind;
	float Yield = 1.f;
};

/**
 * Everything that gives materials when hit (resource nodes, the island's trees, rocks and shrubs) yields through here.
 * Tools work from the bag: the best hatchet you carry chops wood, the best pick breaks stone and ore.
 */
struct WILDREACH_API FWRGathering
{
	static const TArray<FWRToolDef>& Tools();
	/** The tool kind that speeds up gathering an item (hatchet for wood, pick for stone/ore/sulfur), or None. */
	static FName ToolKindFor(FName ItemId);
	/** Best tool of a kind among carried item ids (null = bare hands). Pure, for tests. */
	static const FWRToolDef* BestTool(FName Kind, const TArray<FName>& Carried);
	/** Server: one hit's worth of ItemId into the gatherer's bag (tool, server multiplier, life skills). Returns how many went in. */
	static int32 Gather(AWRCharacterBase* Gatherer, FName ItemId, float PerHit, bool bHeavy, const FString& Source);
};

/**
 * Gatherable node (tree, rock, ore vein, crystal, lichen patch). Melee hits give its item to the
 * hitter's inventory; heavy hits give more. Depletes after a number of hits, then respawns.
 * Ore only appears in caves and mines (Crafting tab); placement enforces that, not this class.
 */
UCLASS()
class WILDREACH_API AWRResourceNode : public AActor
{
	GENERATED_BODY()

public:
	AWRResourceNode();

	/** Item id from Data/items.json (wood, stone, copper_ore, sulfur_crystal, lichen). */
	UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_Look, Category="Resource") FName ItemId = TEXT("wood");
	UPROPERTY(EditAnywhere, Category="Resource") int32 Hits = 6;
	UPROPERTY(EditAnywhere, Category="Resource") int32 PerHit = 2;
	UPROPERTY(EditAnywhere, Category="Resource") float RespawnSeconds = 60.f;
	/** Chance per ore hit of a gem (caves are richer). */
	UPROPERTY(EditAnywhere, Category="Resource") float GemChance = 0.04f;

	/** Server: a melee swing landed on this node. */
	void Harvest(AWRCharacterBase* Gatherer, bool bHeavy);

	bool IsDepleted() const { return bDepleted; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION() void OnRep_Look();
	UFUNCTION() void OnRep_Depleted();

	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(ReplicatedUsing=OnRep_Depleted) bool bDepleted = false;
	int32 HitsLeft = 0;
	FTimerHandle RespawnTimer;
};
