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
#include "WRCrystalThings.h"
#include "WRResourceNode.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/AudioComponent.h"
#include "WRCrystalWorld.generated.h"

class AWRCrystalCaves;
class AWRCrystalState;
class AWRCrystalSpot;
class AWRCharacterBase;
class AWRPlayerController;
class AWRResourceNode;
class UMaterialInstanceDynamic;
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

	// ---- the noise model and the light (WRCrystalThings.cpp; crystal 2.1) ----
	/** Server: every noise heard in the Crystal caves (where, its radius cm, who made it, its kind: step, land, hit,
	 *  cast, roll, chime, crystal, pick, land_shot, bellow...). Echobats, chandeliers, the Vault door listen. */
	TMulticastDelegate<void(const FVector&, float, AActor*, FName)> OnNoise;
	/** Every machine: light the crystals round a noise (Key: the same key moves its own ring: a walker's steps). */
	void Light(const FVector& At, float Radius, float Strength, const void* Key);
	/** A crystal outside the halls' linings that lights with them (a node's cluster, an outcrop). */
	void RegisterGlow(UMaterialInstanceDynamic* MID, const FVector& At);
	/** Server: a hall's keystone chime struck (lit 20 game minutes, the chime's noise, Saga's quest count). */
	void StrikeKeystone(int32 Hall, AWRCharacterBase* By, AWRCrystalSpot* Spot);
	/** Server: an arrow, bolt or stone came down here (its noise; a keystone chime it lands on rings). */
	static void ProjectileLanded(const UObject* WorldContext, const FVector& At, AActor* By, FName Kind, bool bQuiet);
	/** The signature resource's yield multiplier now (the plenty after rain, the Resonance). */
	float SignatureScale() const;
	/** The Resonance's multiplier on resonant crystal and its glow on every lining (WRCrystalEvents sets them). */
	float ResonanceYield = 1.f;
	float ResonanceGlow = 0.f;
	/** Server: each of the Resonance's peals (its number from 0, where it rolls out from, its speed cm/s): the
	 *  Echobats go deaf, the nest geodes crack (the creatures listen). */
	TMulticastDelegate<void(int32, const FVector&, float)> OnPeal;
	AWRCrystalState* GetState() const { return State.Get(); }
	const TArray<TWeakObjectPtr<AWRResourceNode>>& GetNodes() const { return Nodes; }
	AWRCrystalSpot* SpotNear(const FVector& At) const;
	/** Server: an Echobat came for this survivor (Saga's "quiet road" across the Organ is spoiled for this crossing). */
	void QuietRoadSpoiled(AActor* Who);
	/** Every machine: a spot came into this world (its BeginPlay), so the E prompt can find it. */
	void AddSpot(AWRCrystalSpot* Spot) { Spots.AddUnique(Spot); }
	/** E at a crystal spot (the interaction chain in WRWorldNpc, the command in WRWorldSystems: world crystal spot). */
	static bool Prompt(const AWRPlayerController* PC, FString& OutTarget, FString& OutVerb);
	static bool ClientInteract(AWRPlayerController* PC);
	static FString Command(AWRPlayerController* PC, const TArray<FString>& Args);

private:
	struct FRing
	{
		FVector At = FVector::ZeroVector;
		float Radius = 0.f;
		float Strength = 0.f;
		float Age = 0.f;        // 0..1 over the fade
		const void* Key = nullptr;
		float Now() const { return Strength * FMath::Max(0.f, 1.f - Age); }
	};
	struct FGlow { TWeakObjectPtr<UMaterialInstanceDynamic> MID; FVector At; int32 Hall = -1; };
	void SpawnServerActors();
	/** WRCrystalHazards.cpp: ward fields and keystones, chandeliers, thin floors, the Vault door; scalding pools; the
	 *  Resonance (every machine: its glow, swell and peals; the server: the peals' work). */
	void SpawnHazards();
	void StepPools(float Dt);
	/** WRCrystalCreatures.cpp: the dens by sub-zone with their day and night tables (crystal 2.12.3), near survivors. */
	void StepDens(float Dt);
	void StepQuietRoad();
	void StepResonance(float Dt);
	void Peal(int32 Index, const FVector& Source, float Speed);
	void StepLight(float Dt);
	void StepServerNoise(float Dt);
	void StepVenn(float Dt);
	/** Per hall (FWRCrystalData::Halls index), then one loose set for the surface: up to four rings each. */
	TArray<TArray<FRing>> HallRings;
	TArray<float> HallLit;
	TArray<FGlow> ExtraGlows;
	/** The halls' lights (owned by the caves actor), made the first time the view comes near. */
	TMap<int32, TArray<TWeakObjectPtr<UPointLightComponent>>> HallLights;
	TWeakObjectPtr<AWRCrystalState> State;
	TArray<TWeakObjectPtr<AWRCrystalSpot>> Spots;
	TArray<TWeakObjectPtr<AWRResourceNode>> Nodes;
	TMap<TWeakObjectPtr<AActor>, double> FallStart;
	bool bServerDone = false;
	float StepClock = 0.f;
	float ServerStepClock = 0.f;
	float VennClock = 0.f;
	int32 VennPhase = 0;
	FVector VennHome = FVector::ZeroVector;
	FRotator VennFace = FRotator::ZeroRotator;
	mutable double RainSeenAt = -1.0;
	float PoolClock = 0.f;
	struct FDen { FVector At = FVector::ZeroVector; FName Zone; bool bCave = false; float Spread = 600.f; TArray<TWeakObjectPtr<class AWRCreature>> Alive; double NextAt = 0.0; bool bNight = false; };
	TArray<FDen> Dens;
	float DenClock = 0.f;
	/** Saga's quiet road: who is crossing the Organ, from which end (0 / 1), and whether a bat has come for them. */
	TMap<FString, int32> QuietFrom;
	TSet<FString> QuietSpoiled;
	float ChimeClock = 0.f;
	int32 PealsDone = 0;
	int32 PealHeard = -1;
	TWeakObjectPtr<UAudioComponent> Swell;
	TArray<TWeakObjectPtr<AWRCrystalSpot>> SingingGeodes;

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
