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

protected:
	virtual void BeginPlay() override;

private:
	/** The city and the forts: their blocks (looks only) and their solids (hidden boxes you bump into). */
	void BuildCity();
	/** The lake's surface and its swim volume, the stream's ribbon of water down to the sea. */
	void BuildWater();
	/** The trees, saplings and hedge shrubs (Data/hearthmoot.json "plants"), one instanced mesh per kind and variant. */
	void BuildPlants(const TSharedPtr<class FJsonObject>& J);
	/** Machines that draw: the Volcanic's lava river (island.json "lava"), molten in the middle and crusting at its
	 *  rims, with its glow lights along it. */
	void BuildLava();
	/** The lands' wake-up camps (island.json camps.groups): a lived-in campsite at each, a fire ring with its fire,
	 *  canvas tents, bedrolls, a log seat, crates and a barrel (looks on clients; the tents and crates solid on every
	 *  machine). The camps were spawn points with nothing to see. */
	void BuildCamps();
	/** Server: the Volcanic's and the Frozen tundra's creature dens (Data/wildreach_life.json "dens": map metres, a day
	 *  pack and a night pack each) and the roaming minis' loops on their lands ("roam"). */
	void PlaceLife();
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Lava;
	/** Server: the meadow is the Commons, a safe zone (roundIsland.safeRadius). */
	void BuildSafeZone();
	/** Machines that draw: grass and wildflowers on the meadow and the city's yards, the crops in the fields' rows,
	 *  tufts of thin blades in cells round the view, made and dropped as it moves (roundIsland.cover*, grass*, flower*,
	 *  crop*). */
	void StepCover();
	UPROPERTY() TObjectPtr<UStaticMesh> BladeMesh;
	UPROPERTY() TObjectPtr<class UMaterialInterface> BladeMat;
	/** Flower heads and grain ears: the engine cylinder (a disc, a rounded ear) in the same tinted material. */
	UPROPERTY() TObjectPtr<UStaticMesh> DiscMesh;
	/** Real grass (MW Landscape Auto Material's clumps A-D, wind-swayed cards) and its material per grass: the meadow,
	 *  the steppe, the cinder grass, the thaw's sedge, the kipuka (tinted each). Empty without the pack: cube blades. */
	UPROPERTY() TArray<TObjectPtr<UStaticMesh>> GrassMeshes;
	UPROPERTY() TArray<TObjectPtr<class UMaterialInstanceDynamic>> GrassMats;
	/** The fields' crops (round 8; Tools/art/make_meshes.py crop()): wheat clumps, barley clumps, cabbages, two of each,
	 *  in the tinted material. Empty if they're missing: the old blades. */
	UPROPERTY() TArray<TObjectPtr<UStaticMesh>> CropMeshes;
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
	/** The hearth's crackle now and then while you're near it. */
	float CrackleClock = 0.f;

	/** Rooms you walk into (Data/hearthmoot.json "floors", map metres): no grass grows through their floors. */
	struct FFloor { FVector2D C = FVector2D::ZeroVector, Half = FVector2D::ZeroVector; float Yaw = 0.f; };
	TArray<FFloor> Floors;

	/** Server: Hearthmoot's people (Data/hearthmoot.json "people"): out while any player is near the city (a few each
	 *  tick, so no hitch), gone a while after the last one leaves; the walkers kept moving along their walks. */
	void LoadPeople(const TSharedPtr<class FJsonObject>& J);
	void StepPeople(float DeltaSeconds);
	struct FWalk { TArray<FVector> Pts; bool bClosed = false; };
	struct FPerson
	{
		uint8 Kind = 0;               // 0 walker, 1 seller, 2 at a post
		FName Role, Sex, Outfit, Table;
		FString Name, Id;
		int32 Face = 0, Build = 0, Walk = -1, Point = 0, Dir = 1;
		FVector At = FVector::ZeroVector;
		float Yaw = 0.f;
		TWeakObjectPtr<class AWRWorldNpc> Npc;
	};
	TArray<FWalk> Walks;
	TArray<FPerson> People;
	bool bPeopleOut = false;
	float AloneFor = 0.f;

	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Solids;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Water;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> LakeMat;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> StreamMat;
	/** The lake wears the sea's water, so it glows, shines and mirrors the sky as the sea does by day and dims at night
	 *  (tuning sea.dim, their daytime values). */
	TMap<FName, float> LakeDay;
	float LastDaylight = -1.f;
	/** The moon on the lake (M_WaterGlint over its water: the pack's water shows no light's highlight). */
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Glint;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> GlintMat;
	void StepGlint();
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
