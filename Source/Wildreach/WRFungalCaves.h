#pragma once

// The Fungal caverns' rock (cooks/build-biome-fungal.md 2.0.6, 2.2-2.4): the boundary of the caverns' air
// (FWRFungalData::Air: every hall, doline, chasm, grotto, tube, crawl and shaft, unioned) meshed by surface nets in 30 m
// cubes, built on every machine with no network cost (the server needs the collision, clients the look), a few cubes a
// frame, nearest the descents first. Where a cavern opens to the sky (a doline, a descent, the Sieves, the Sea Cave) the
// open air above the ground is part of the union, so the rock rolls over the lip into the ground with no crack; the part
// of that skin that lies on ground the land already draws is dropped. A cube whose centre is further from the boundary
// than its half-diagonal holds no surface and is skipped without sampling it.
//
// The caverns light themselves (fungal 2.1 read rules): the rock is drawn in M_FungalGlow, its colour in the vertex rgb
// (limestone, flowstone, rimstone, rhizomorph black, mycelium white) and how much of it glows in the vertex alpha (the
// Halls' moss carpets cyan, the ceilings' starthread white, the Webs' amber fissures, the Grottos' teal waterline, the
// Heart Grotto's warm membrane). Its one material instance is driven by UWRFungalWorld (the circadian curve, the beat).

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WRCaveBuild.h"
#include "WRFungalCaves.generated.h"

class UProceduralMeshComponent;
class UMaterialInstanceDynamic;

UCLASS(NotPlaceable)
class WILDREACH_API AWRFungalCaves : public AActor
{
	GENERATED_BODY()

public:
	AWRFungalCaves();
	virtual void Tick(float DeltaSeconds) override;

	/** Every rock cube is built (collision may still be cooking for a frame or two). */
	bool IsBuilt() const { return bQueued && Next >= Queue.Num() && InFlight.Num() == 0; }
	bool IsBuiltAt(const FVector& World) const;
	/** The rock's material instance (null on a dedicated server). */
	UMaterialInstanceDynamic* RockGlow() const { return RockMID; }
	static float CubeSize();

protected:
	virtual void BeginPlay() override;

private:
	void QueueCubes();
	/** A cube's rock, worked out on a worker thread from the fungal data alone (WRCaveBuild.h). */
	static void ComputeCube(const FIntVector& Key, float Cell, int32 Cells, const FVector& ActorLoc, float GroundSkin, FWRCaveMesh& Out);
	/** On the game thread: a computed cube becomes a mesh component (false: nothing to draw). */
	bool FinishCube(const FWRCaveMesh& M);
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	TArray<FWRCaveJob> InFlight;
	TArray<FIntVector> Queue;
	TSet<FIntVector> Done;
	int32 Next = 0;
	bool bQueued = false;
	float Cell = 150.f;
	int32 Cells = 20;
	int32 Skipped = 0;
	UPROPERTY() TArray<TObjectPtr<UProceduralMeshComponent>> Rock;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> RockMID;
};
