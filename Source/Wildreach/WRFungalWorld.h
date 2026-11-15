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
	/** Server: a survivor was reeled on a lure line (Ilmari's Cold light walk is spoiled for this crossing). */
	void GalleryWalkSpoiled(const FString& Player);
	/** E at a door or a secret (the interaction chain in WRWorldNpc, the command in WRWorldSystems: world fungal spot). */
	static bool Prompt(const class AWRPlayerController* PC, FString& OutTarget, FString& OutVerb);
	static bool ClientInteract(class AWRPlayerController* PC);
	static FString Command(class AWRPlayerController* PC, const TArray<FString>& Args);

private:
	/** WRFungalThings.cpp: the server's nodes, caches and keepsakes, once the scenery is built. */
	void SpawnServerActors();
	bool bServerDone = false;
	TArray<TWeakObjectPtr<class AWRResourceNode>> Nodes;
	TArray<TWeakObjectPtr<class AWRWorldCache>> Caches;
	// WRFungalEvents.cpp: the hazard machines, foul air, glow paste, the bloom's beat, Spore drift, the Sporefall, the
	// invasions' marks, Ilmari's day and night, the Lure Gallery walk, the bloomed beasts' caps.
	void SpawnHazards();
	void StepHazards(float Dt);
	void StepSporefall(float Dt);
	void StepFlush(float Dt);
	void StepIlmari(float Dt);
	void StepInvasionMarks(float Dt);
	void StepClientLooks(float Dt);
	bool bHazardsDone = false;
	TWeakObjectPtr<class AWRFungalState> State;
	float HazardClock = 0.f, DriftClock = 0.f, FlushClock = 0.f, IlmariClock = 0.f, InvClock = 0.f, LookClock = 0.f;
	TMap<TWeakObjectPtr<class AWRCharacterBase>, float> FoulTime;
	TMap<FString, int32> GalleryFrom;
	TSet<FString> GallerySpoiled;
	TArray<TWeakObjectPtr<class AWRSporeCloud>> Flush;
	TSet<FIntPoint> FlushCells;
	TArray<TWeakObjectPtr<class AWRResourceNode>> FlushNodes;
	TMap<TWeakObjectPtr<class AWRCreature>, float> BloomTime;
	TMap<TWeakObjectPtr<class AWRCreature>, FVector> StandAt;
	TMap<TWeakObjectPtr<class AWRCreature>, float> StandStill;
	int32 IlmariPhase = 0;
	FVector IlmariHome = FVector::ZeroVector;
	FRotator IlmariFace = FRotator::ZeroRotator;
	TWeakObjectPtr<USceneComponent> GoldBreath;
	TMap<TWeakObjectPtr<class AWRCharacterBase>, TWeakObjectPtr<class UPointLightComponent>> PasteLights;
	TMap<TWeakObjectPtr<class AWRCharacterBase>, TWeakObjectPtr<class UProceduralMeshComponent>> BloomCaps;
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
