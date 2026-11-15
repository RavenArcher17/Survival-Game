#pragma once

// The Crystal caves on the map (cooks/biome-crystal.md 2.1-2.5, 2.11, 2.20; cooks/build-biome-crystal.md 2.0-2.7): what
// the game builds on the Crystal land and under it at runtime from Data/crystal.json (Tools/terrain/crystal.mjs, parsed
// by FWRCrystalData in WRCrystalTypes.cpp). The rock of the caves is AWRCrystalCaves (WRCrystalCaves.cpp); static
// scenery (the camps and their geos, the landmarks, the descents and their stairs, the vents, the domes, the hand sites,
// the kit POIs, the chime roads, the plateau's columns, the caves' furniture) is built on every machine with no network
// cost, like the forest's (WRCrystalScenery*.cpp, on the forest's FBuild).

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRForestWorld.h"
#include "WRCrystalWorld.generated.h"

class AWRCrystalCaves;
class UPointLightComponent;

/** One place of the Crystal caves, modelled in code at real scale on the forest's FBuild (the kit: WRCrystalLook.h).
 *  Kind: a site's, a kit row's or a descent's kind, or cell (the plateau's columns and grass in a 200 m cell), road,
 *  vent, dome, chime, hall (a hall's furniture), tube (a tube's furniture). */
UCLASS(NotPlaceable)
class WILDREACH_API AWRCrystalScenery : public AWRForestScenery
{
	GENERATED_BODY()

public:
	/** The row it was built from (a hall, a tube, a dome, a cell index...). */
	int32 Index = -1;
	/** Underground: its lamps burn day and night. */
	bool bUnder = false;
	void BuildCrystal();
};

/** Runs the Crystal caves' world: builds the rock and the scenery, keeps their night lights. */
UCLASS()
class WILDREACH_API UWRCrystalWorld : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UWRCrystalWorld* Get(const UObject* WorldContext);
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UWRCrystalWorld, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return bActive; }
	bool bActive = false;

	/** The scenery is built in this world. */
	bool IsBuilt() const { return bActive && bSceneryDone; }
	AWRCrystalCaves* Caves() const { return CavesActor.Get(); }

private:
	struct FPiece
	{
		FName Kind, Id;
		FVector At = FVector::ZeroVector;
		float Yaw = 0.f;
		int32 Index = -1;
		bool bUnder = false;
	};
	void BuildScenery();
	void StepBuild();
	void StepLights(float Dt);
	TArray<FPiece> Pieces;
	int32 PiecesBuilt = 0;
	bool bQueued = false;
	bool bSceneryDone = false;
	float WaitClock = 0.f;
	float LightClock = 0.f;
	TArray<TWeakObjectPtr<AWRCrystalScenery>> Scenery;
	TWeakObjectPtr<AWRCrystalCaves> CavesActor;
};
