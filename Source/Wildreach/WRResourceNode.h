#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WRResourceNode.generated.h"

class AWRCharacterBase;
class UStaticMeshComponent;

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
