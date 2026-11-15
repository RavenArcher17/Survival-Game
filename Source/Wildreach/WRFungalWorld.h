#pragma once

// The Fungal caverns on the map (cooks/biome-fungal.md 2.1-2.6, 2.11, 2.20; cooks/build-biome-fungal.md 2.0-2.7): what
// the game builds on the Fungal land and under it at runtime from Data/fungal.json (Tools/terrain/fungal.mjs, parsed by
// FWRFungalData in WRFungalTypes.cpp). The rock of the caverns is AWRFungalCaves (WRFungalCaves.cpp); the static scenery
// (the camps, the landmarks, the descents and their ways down, the hand sites, the kit POIs, the halls' furniture, the
// Heart Grotto's city, the plateau's juniper and boulders) is built on every machine with no network cost, like the
// forest's (WRFungalScenery*.cpp, on the forest's FBuild and the fungal look kit WRFungalLook.h). The cavern glow follows
// the surface clock (fungal 2.1 read rules): one value drives every glowing thing through the shared material instances.

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRForestWorld.h"
#include "WRFungalWorld.generated.h"

class AWRFungalCaves;
class AWRForestWaterVolume;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UMaterialInterface;

/** One place of the Fungal caverns, modelled in code at real scale (the kit: WRFungalLook.h). Kind: a site's, a kit
 *  row's, a descent's, or cell (the plateau's juniper, boulders and tufts in a 200 m cell), hall (a hall's furniture),
 *  tube (a tube's furniture), shrine. */
UCLASS(NotPlaceable)
class WILDREACH_API AWRFungalScenery : public AWRForestScenery
{
	GENERATED_BODY()

public:
	/** The row it was built from (a hall, a tube, a kit row, a cell index...). */
	int32 Index = -1;
	/** Underground: its lamps burn day and night. */
	bool bUnder = false;
	void BuildFungal();
};

/** Runs the Fungal caverns' world: builds the rock and the scenery, keeps their lights and the cavern glow. */
UCLASS()
class WILDREACH_API UWRFungalWorld : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UWRFungalWorld* Get(const UObject* WorldContext);
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UWRFungalWorld, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return bActive; }
	bool bActive = false;

	bool IsBuilt() const { return bActive && bSceneryDone; }
	AWRFungalCaves* Caves() const { return CavesActor.Get(); }
	/** The shared glowing / see-through material instances (null on a dedicated server). */
	UMaterialInstanceDynamic* GlowMID() const { return Glow; }
	UMaterialInstanceDynamic* VeilMID() const { return Veil; }
	/** The cavern glow's level now (WRFungal::GlowNow). */
	float GlowLevel() const { return GlowCurrent; }
	/** A sync beat over the glow (the Spore bloom's tell at 1 Hz, the Sporefall's 35 a minute): its amount 0..1 and its
	 *  rate (beats a second). Set by the weather and the event. */
	float BeatAmount = 0.f;
	float BeatRate = 1.f;

	/** The signature resource's yield multiplier now (the Sporefall's x3 on deepcap). */
	float SignatureScale() const;
	/** The Sporefall is on (WRFungalEvents sets it). */
	bool bSporefall = false;
	const TArray<TWeakObjectPtr<class AWRResourceNode>>& GetNodes() const { return Nodes; }

private:
	/** WRFungalThings.cpp: the server's nodes, caches and keepsakes, once the scenery is built. */
	void SpawnServerActors();
	bool bServerDone = false;
	TArray<TWeakObjectPtr<class AWRResourceNode>> Nodes;
	TArray<TWeakObjectPtr<class AWRWorldCache>> Caches;
	void BuildScenery();
	void StepBuild();
	void StepLights(float Dt);
	void StepGlow(float Dt);
	void SpawnWater();

	struct FPiece
	{
		FName Kind, Id;
		FVector At = FVector::ZeroVector;
		float Yaw = 0.f;
		int32 Index = -1;
		bool bUnder = false;
	};
	TArray<FPiece> Pieces;
	int32 PiecesBuilt = 0;
	bool bQueued = false;
	bool bSceneryDone = false;
	float WaitClock = 0.f;
	float LightClock = 0.f;
	float GlowClock = 0.f;
	float GlowCurrent = 0.6f;
	TArray<TWeakObjectPtr<AWRFungalScenery>> Scenery;
	TWeakObjectPtr<AWRFungalCaves> CavesActor;
	TArray<TWeakObjectPtr<AWRForestWaterVolume>> WaterVolumes;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> Glow;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> Veil;
};
