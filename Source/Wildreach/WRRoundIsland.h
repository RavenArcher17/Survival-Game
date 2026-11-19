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
class UHierarchicalInstancedStaticMeshComponent;
class UStaticMesh;
class UProceduralMeshComponent;
class UMaterialInstanceDynamic;

UCLASS(NotPlaceable)
class WILDREACH_API AWRRoundIsland : public AActor
{
	GENERATED_BODY()

public:
	AWRRoundIsland();
	virtual void Tick(float DeltaSeconds) override;
	/** Look checks (wr.Lake): param Name Value | colour Name R G B (sRGB) | material Path (the lake's base material). */
	void DebugLake(const TArray<FString>& Args);

protected:
	virtual void BeginPlay() override;

private:
	/** The city and the forts: their blocks (looks only) and their solids (hidden boxes you bump into). */
	void BuildCity();
	/** The lake's surface and its swim volume, the stream's ribbon of water down to the sea. */
	void BuildWater();
	/** The trees, saplings and hedge shrubs (Data/hearthmoot.json "plants"), one instanced mesh per kind and variant. */
	void BuildPlants(const TSharedPtr<class FJsonObject>& J);
	/** Server: the meadow is the Commons, a safe zone (roundIsland.safeRadius). */
	void BuildSafeZone();
	/** Machines that draw: grass and wildflowers on the meadow and the city's yards, the crops in the fields' rows,
	 *  tufts of thin blades in cells round the view, made and dropped as it moves (roundIsland.cover*, grass*, flower*,
	 *  crop*). */
	void StepCover();
	UPROPERTY() TObjectPtr<UStaticMesh> BladeMesh;
	UPROPERTY() TObjectPtr<class UMaterialInterface> BladeMat;
	TMap<FIntPoint, TArray<TObjectPtr<UInstancedStaticMeshComponent>>> CoverCells;

	/** Machines that draw: the city's lights with the clock. The night blocks (lit windows, lantern glass) show after
	 *  dusk; the hearth burns always; braziers and torches light at dusk; each fire's flame is drawn only near the view. */
	void StepLights(float Daylight);
	UPROPERTY() TObjectPtr<USceneComponent> NightRoot;
	struct FFire
	{
		uint8 Kind = 0;               // 0 hearth, 1 brazier, 2 lantern (a light only), 3 torch (a flame only)
		FVector At = FVector::ZeroVector;
		float Scale = 1.f;
		float Phase = 0.f;
		TWeakObjectPtr<class UPointLightComponent> Light;
		TWeakObjectPtr<USceneComponent> Fx;
	};
	TArray<FFire> Fires;
	bool bNightShown = false;

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
