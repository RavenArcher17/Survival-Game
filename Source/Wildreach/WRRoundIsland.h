// The round island's own things that aren't ground (plan phase 3, "One big world"): Hearthmoot and the forts on its
// main roads (Data/hearthmoot.json: the building look's blocks, Tools/terrain/hearthmoot.mjs), and the meadow's lake and
// its stream (Data/lands/island.json "lake": water you see, and the lake's you swim in). Built on every machine from the
// data, like the ground (nothing replicated, nothing saved).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRRoundIsland.generated.h"

class UInstancedStaticMeshComponent;
class UProceduralMeshComponent;
class UMaterialInstanceDynamic;

UCLASS(NotPlaceable)
class WILDREACH_API AWRRoundIsland : public AActor
{
	GENERATED_BODY()

public:
	AWRRoundIsland();
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	/** The city and the forts: their blocks (looks only) and their solids (hidden boxes you bump into). */
	void BuildCity();
	/** The lake's surface and its swim volume, the stream's ribbon of water down to the sea. */
	void BuildWater();

	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Solids;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Water;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> LakeMat;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> StreamMat;
	/** The lake wears the sea's water, so it glows, shines and mirrors the sky as the sea does by day and dims at night
	 *  (tuning sea.dim, their daytime values). */
	TMap<FName, float> LakeDay;
	float LastDaylight = -1.f;
};

/** Spawns the round island's things on a map that has a land of its own (FWRLandDef::Map), on every machine. */
UCLASS()
class WILDREACH_API UWRRoundIslandSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
};
