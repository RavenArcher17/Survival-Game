#pragma once

// The Hollow Root and the Ancient forest's secrets (cooks/biome-forest.md 2.5, 2.22, 2.4 #14/#16/#26; cooks/build-biome-
// forest.md 2.8, 2.15). The Hollow Root is the chasm network Tools/terrain/forest.mjs carves 10-60 m under the old growth
// from the Eldest to the west heart-tree (Data/forest.json "hollow" rooms, in route order): the landscape is its floor and
// this file roofs every room and passage with woven roots (open only at its four ways in), dresses it (root pillars and
// arches, foxfire on every surface with a dim light every 15 m, glowworm curtains) and fills its fourteen rooms. The
// secrets are the hazards track's: F-S1 the Lost Path, F-S2 the Alder Grotto's back room, F-S3 the Sinkroot squeeze,
// F-S4 the barred door, F-S5 the Bell-Tree's bell (F-S6, the Matron's larder, is the boss track's), the eel trap at Otter
// Steps and the thornbrake-wall caches (their brakes are WRForestHazards').
//
// What it reads from Data/forest.json (FWRForestData, track A's generator):
//   hollow rooms by id: root_hall, cradle_vault, grove_of_bones, weeping_roots, mossmaul_den, bootlace_gallery,
//     thrumming_root, drowned_root, seed_store, wasp_grave, glowworm_chasm, lattice_chamber, iron_seam, heart_root
//     (any missing room is left out; with no rooms at all the Hollow Root isn't built);
//   spots: hollow_door (the Eldest's Door's foot, Yaw = facing out), hollow_dig (the Truffler's soft spot), hollow_heart
//     (where the Heart Root comes up in the Four Hearts' clearing), hollow_squeeze (the Sinkroot's bottom, the crawl's
//     start), grotto_squeeze + grotto_room (F-S2), barred_door (F-S4: the door's foot, Yaw = facing out), eel_trap,
//     glowworm (extra glowworm curtains anywhere);
//   sites/kit by kind: eldest, sinkroot, bell_tree (and giants kind bell_tree), bread_stone_lane, otter_steps, lost_path,
//     eel_trap, thornbrake_cache; routes kind lost_path (start, the four trail-tree bends, the end gap);
//   arenas: sylvaroth (Points[3] = the west heart-tree, the Heart Root's fallback exit).
// Numbers: tuning hazards.hollow.* / hazards.secrets.*.

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WRForestTypes.h"
#include "WRForestHollow.generated.h"

class UAudioComponent;
class UBoxComponent;
class UPointLightComponent;
class UProceduralMeshComponent;
class UPrimitiveComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UStaticMeshComponent;
class AWRPlayerController;
class AWRCharacterBase;

/** Is this point inside the Hollow Root (in a room or a passage, under its roof)? Data only: any machine, any time, no
 *  world needed. WRForest::ZoneAt (WRForestTypes.cpp) answers 'h' when this is true. */
WILDREACH_API bool WRForestHollowContains(const FVector& Pos);

namespace WRForestHollow
{
	/** Server, once the forest is live (UWRForestHazards::SpawnServerActors): every stateful thing of the Hollow Root and
	 *  the secrets (doors, the plug, the bell, the jars, the trap, caches, nodes, the sleepers). */
	WILDREACH_API void SpawnServer(UWorld* World);
	/** Every machine: the Hollow Root's roofs, roots, foxfire, glowworms and lights; the Lost Path's foxfire loop and bent
	 *  trees; the grotto's back room; the bell-tree's root holds (the server too: their collision). */
	WILDREACH_API void BuildLocal(UWorld* World);
	/** Server, from UWRForestHazards::Tick: the Lost Path's walkers, the Hollow's sleepers coming back. */
	WILDREACH_API void StepServer(UWorld* World, float Dt);
	/** The interact prompt for a secret in reach (the Thrumming Root, the jars, the doors, the plug, the bell, the trap). */
	WILDREACH_API bool Prompt(const AWRPlayerController* PC, FString& OutTarget, FString& OutVerb);
	/** E: the Thrumming Root's vision plays here at once; every secret sends "world fhaz secret x y z". */
	WILDREACH_API bool ClientInteract(AWRPlayerController* PC);
	/** Server: "world fhaz secret x y z". */
	WILDREACH_API FString Command(AWRPlayerController* PC, const TArray<FString>& Args);
	/** One rare seed of any biome's rare crop (farming.json wildSeeds: the rare row of each biome), or None. */
	WILDREACH_API FName RareSeed();
	/** Server: a fire source at At (a torch held at it, a fire spell) burns the barred door's living bark. */
	WILDREACH_API void FireAt(UWorld* World, const FVector& At, float Seconds);

	// ---- The Truffler's dig (for the creature track, WRForestCreatures.cpp) ----
	/** Where the Hollow Root's soft spot is (the root plug in the Cradle Vault's roof, nearest the cook's (-220, +3,080))
	 *  and how close a digger must stand. False: no plug in this world, or it's already dug through. Either machine. */
	WILDREACH_API bool DigSpot(const UObject* WorldContext, FVector& OutAt, float& OutRadius);
	/** Server, every tick a tamed Truffler is digging (its owner commanded it to the spot): adds DeltaSeconds of digging
	 *  if it stands within the spot's radius. After hazards.hollow.digSeconds (20 s) in all the plug collapses into the
	 *  Cradle Vault and stays open. Returns the dig's progress 0..1, or -1 if the Truffler isn't at the spot (or isn't a
	 *  tame, or there's nothing left to dig). The plug's soil sprays and its sound play while it rises. */
	WILDREACH_API float TrufflerDigging(AActor* Truffler, float DeltaSeconds);
	/** The plug has been dug through (the way in from above is open). */
	WILDREACH_API bool IsDugOpen(const UObject* WorldContext);
}

/**
 * The Hollow Root's look and its walking surfaces, built the same on every machine from the data (not replicated):
 * root-mat roofs over every room and passage (each tile with a hidden slab that you walk on and that keeps the sky out),
 * buttress-root pillars down the walls and root arches over the passages, foxfire streaks on the roots with a dim green
 * light every 15 m, glowworm curtains under the roof, each room's dressing (the cradles, the bones, the basins, the
 * laces, the jars, the dead nest, the lattice, the seam), the Glowworm Chasm's spiral root stair, the stairs down from the
 * Eldest's Door and up to the Heart Root's door, the drowned room's water and the Sinkroot squeeze.
 */
UCLASS(NotPlaceable)
class WILDREACH_API AWRHollowRoot : public AActor
{
	GENERATED_BODY()

public:
	AWRHollowRoot();
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	void Build();
	/** The roof's top over a room (by index) once built; the floor + 8 m before. */
	float RoofZ(int32 Room) const { return Roofs.IsValidIndex(Room) ? Roofs[Room] : 0.f; }

private:
	void BuildRoofs(bool bDraw);
	void BuildRoom(int32 Index, bool bDraw);
	void BuildPassages(bool bDraw);
	void BuildWaysIn(bool bDraw);
	UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> Lights;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> PulseLight;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> PulseRoot;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Heartbeat;
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> Curtains;
	UPROPERTY(Transient) TObjectPtr<AActor> WaterVolume;
	TArray<float> Roofs;
	TArray<FVector> LightAt;
	float LightClock = 0.f;
	bool bBuilt = false;
	/** Tries left waiting for the land under the rooms (its collision is built at runtime by WRLand). */
	int32 BuildTries = 120;
	float BuildClock = 0.f;
	float SwayClock = 0.f;
	/** Hidden walk slabs over the roofs, with their gaps (the Eldest's Door, the Truffler's hole, the Heart Root). */
	UPROPERTY(Transient) TArray<TObjectPtr<class UBoxComponent>> Slabs;
	UPROPERTY(Transient) TArray<TObjectPtr<UProceduralMeshComponent>> Meshes;
	TArray<float> CurtainPhase;
	FVector PulseAt = FVector::ZeroVector;
	void AddLight(const FVector& At, float Scale);
	void BuildGlows(bool bDraw);
};

/** A secret's state, replicated: what it is and where it stands in its little story. */
UCLASS(NotPlaceable)
class WILDREACH_API AWRForestSecret : public AActor
{
	GENERATED_BODY()

public:
	AWRForestSecret();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** heart_door (the Heart Root's one-way door), root_plug (the Truffler's soft spot), thrumming_root, seed_store,
	 *  barred_door (F-S4), bell (F-S5), eel_trap. */
	UPROPERTY(Replicated) FName Kind;
	/** heart_door 0 shut / 1 open; root_plug 0 whole / 1 dug through; barred_door 0 shut / 1 burnt open; bell: rings
	 *  count; eel_trap 0 empty / 1 a catch in it. */
	UPROPERTY(ReplicatedUsing=OnRep_State) uint8 State = 0;
	/** Server time the state last changed (the door swings, the plug falls, the bell swings from it). */
	UPROPERTY(Replicated) float StateAt = -1000.f;
	/** A shape number the look reads (the door's width, the bell's size). */
	UPROPERTY(Replicated) float Size = 100.f;
	/** Its burn progress (barred door: 0..1 of the 6 s of torch). */
	UPROPERTY(Replicated) float Burnt = 0.f;

	/** Server: E on it. */
	FString Use(AWRPlayerController* PC);
	/** Who may use it from where: within its reach (and the heart door only from inside). */
	bool InReach(const FVector& At, float Slack = 0.f) const;
	FString PromptVerb(const AActor* Who, FString& OutTarget) const;

	// Server bookkeeping.
	TMap<FString, int32> DayBy;
	TSet<FString> Told;
	double NextAtHours = -1.0;
	float TorchClock = 0.f;

private:
	UFUNCTION() void OnRep_State();
	void BuildLook();
	void StepLook(float Dt);
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Moving;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Clapper;
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Mesh;
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> MovingMesh;
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Catch;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Block;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> Glow;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Loop;
	/** Collision built with the look (on every machine, the server too): the bell's root holds, the back room's walls,
	 *  the dug plug's soil ramp, the family's table. */
	UPROPERTY(Transient) TArray<TObjectPtr<UPrimitiveComponent>> Solids;
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Char;
	bool bLookBuilt = false;
	bool bToldHere = false;
	float LookClock = 0.f;
	/** The barred door: found the lane's real doorway (the scenery's bar) and turned its bar off once burnt open. */
	bool bDoorBarOff = false;
	/** Where the look was drawn (the barred door moves once onto the lane's real doorway: the look follows). */
	FVector LookBuiltAt = FVector::ZeroVector;
	void SetDoorBar(bool bOn);
	void BuildSolids();
	void StepServer(float Dt);
	float ServerClock = 0.f;

public:
	/** Server: a dig's progress (root_plug) and when it last moved (the soil spray). */
	float DigSeconds = 0.f;
	float LastDigAt = -100.f;
	/** Where the plug's ramp lands on the Cradle Vault's floor (root_plug; from the data on every machine). */
	FVector RampFoot = FVector::ZeroVector;
};

/** The Lost Path's foxfire loop and the bent trail trees (local look; the walking rules are the server's). */
UCLASS(NotPlaceable)
class WILDREACH_API AWRLostPathLook : public AActor
{
	GENERATED_BODY()

public:
	AWRLostPathLook();
	virtual void Tick(float DeltaSeconds) override;
	void Build(const TArray<FVector>& Chain);

private:
	UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> Wisps;
	TArray<FVector> Loop;
	float Clock = 0.f;
};
