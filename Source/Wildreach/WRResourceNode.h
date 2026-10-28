#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WRResourceNode.generated.h"

class AWRCharacterBase;
class UStaticMeshComponent;
struct FWRItemStack;

/**
 * A gathering tool from Data/items.json ("tool": {"kind": "hatchet" | "pick", "yield": 1.5, "damage": 7, "head": [r,g,b]}).
 * Tools go in the weapon hand ("equip": {"slot": "weapon", "durability": N}): held, they're what you swing.
 */
struct FWRToolDef
{
	FName ItemId;
	FName Kind;
	float Yield = 1.f;
	/** Light-attack damage as a weapon (heavy x1.6, the flat-damage rule); halved when broken. */
	float Damage = 6.f;
	/** The head's colour on the hand model (linear). */
	FLinearColor Head = FLinearColor(0.3f, 0.3f, 0.29f);
};

/**
 * Everything that gives materials when hit (resource nodes, the island's trees, rocks and shrubs) yields through here.
 * The held item decides: a hatchet in hand chops wood at its yield, a pick breaks stone and ore at its own; a sword,
 * bare hands, the wrong tool or a broken one gathers at the base rate. The held item wears with each gathering hit.
 */
struct WILDREACH_API FWRGathering
{
	static const TArray<FWRToolDef>& Tools();
	/** The tool definition of an item, or null if it isn't a tool. */
	static const FWRToolDef* FindTool(FName ItemId);
	/** The tool kind that speeds up gathering an item (hatchet for wood, pick for stone/ore/sulfur), or None. */
	static FName ToolKindFor(FName ItemId);
	/** The held tool that gives its yield for a kind: the held item if it's that kind of tool and not broken, else null
	 *  (the base rate). Pure, for tests. */
	static const FWRToolDef* HeldTool(FName Kind, FName HeldItem, float Durability);
	/** A blade in hand: a hatchet or a sword that isn't broken (a body gives it its whole share). */
	static bool IsSharp(const FWRItemStack& Held);
	/** Server: one hit's worth of ItemId into the gatherer's bag (held tool, server multiplier, life skills). Returns how many went in. */
	static int32 Gather(AWRCharacterBase* Gatherer, FName ItemId, float PerHit, bool bHeavy, const FString& Source);
	/** Server: the held tool or weapon wears for one gathering hit (gather.tools.wearPerHit for the right tool,
	 *  wearOther for anything else). Returns true if that hit broke it. */
	static bool WearHeld(AWRCharacterBase* Gatherer, bool bRightTool);
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
