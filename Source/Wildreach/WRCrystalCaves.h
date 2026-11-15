#pragma once

// The Crystal caves' rock (cooks/build-biome-crystal.md 2.0.5, 2.3, 2.6): the boundary of the caves' air
// (FWRCrystalData::Air: every hall, tube, passage and shaft, unioned) meshed by surface nets in 31 m cubes, built on
// every machine with no network cost (the server needs the collision, clients the look), a few cubes a frame. Where a
// cave opens to the sky (a descent, a vent, the Severance Gate, the Sinking Geode's ramp) the open air above the ground
// is part of the union, so the rock rolls over the lip into the ground with no crack; the part of that skin that lies on
// the ground the land already draws is dropped. The rock's colour is in its vertex colours (agate bands, quartz,
// amethyst, lava glaze, guano, hewn basalt courses, crystal dust), its gloss in their alpha (M_HeldTool).
//
// Each hall's crystal lining (amethyst points in clusters off the walls and ceiling, selenite in the Crystal Forests, the
// Prism Hall's violet) is one mesh in M_CrystalGlow with its own material instance: the hall's glow (rest, lit, the four
// rings of noise) is set on it by the crystal world (WRCrystalWorld).

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WRCrystalCaves.generated.h"

class UProceduralMeshComponent;
class UMaterialInstanceDynamic;

UCLASS(NotPlaceable)
class WILDREACH_API AWRCrystalCaves : public AActor
{
	GENERATED_BODY()

public:
	AWRCrystalCaves();
	virtual void Tick(float DeltaSeconds) override;

	/** Every rock cube is built (collision may still be cooking for a frame or two). */
	bool IsBuilt() const { return bQueued && Next >= Queue.Num() && NextHall >= HallOrder.Num(); }
	/** The rock cube at this point exists. */
	bool IsBuiltAt(const FVector& World) const;
	/** A hall's crystal lining's material (its glow is driven through it), or null (not built, or no lining). */
	UMaterialInstanceDynamic* HallGlow(int32 Hall) const;
	/** Where a hall's crystals are densest (its lights stand there): up to three points. */
	const TArray<FVector>* HallClusters(int32 Hall) const { return Clusters.Find(Hall); }
	/** The cube edge (cm). */
	static float CubeSize();

protected:
	virtual void BeginPlay() override;

private:
	void QueueCubes();
	bool BuildCube(const FIntVector& Key);
	void BuildLining(int32 Hall);
	TArray<FIntVector> Queue;
	TSet<FIntVector> Done;
	int32 Next = 0;
	TArray<int32> HallOrder;
	int32 NextHall = 0;
	bool bQueued = false;
	float Cell = 125.f;
	int32 Cells = 25;
	UPROPERTY() TArray<TObjectPtr<UProceduralMeshComponent>> Rock;
	UPROPERTY() TArray<TObjectPtr<UProceduralMeshComponent>> Linings;
	UPROPERTY() TMap<int32, TObjectPtr<UMaterialInstanceDynamic>> Glows;
	TMap<int32, TArray<FVector>> Clusters;
};
