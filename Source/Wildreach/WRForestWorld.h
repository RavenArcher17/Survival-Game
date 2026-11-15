#pragma once

// The Ancient forest on the map (cooks/biome-forest.md 2.1-2.7, 2.11, 2.19, 2.20; cooks/build-biome-forest.md 2.5,
// 2.12): everything the game builds on the forest landscape at runtime from Data/forest.json (Tools/terrain/forest.mjs,
// parsed by FWRForestData in WRForestTypes.cpp). Static scenery (the landmarks L2-L4 and L8-L12, every non-arena hand
// site, the kit POIs, the canopy walkways, the waterfalls) is built on every machine with no network cost, like the
// island's dressing (WRForestScenery.cpp). Things with state are the server's and replicate: the site caches, the caves,
// the resource nodes (heartwood, resin, honey, rhizomorph, foxfire wood, thorns, antler, wild oats and Idunn apples), the
// Canopy Lift, the offering stones, snare stakes, the forest's glow marks (cut rhizomorph).
//
// Site kinds this file understands (forest.json "sites" / "kit" rows, `kind`): the landmarks eldest, great_falls,
// rotted_treant, hermits_oak, strangler_arch, antler_barrow, bell_tree, ringfort; the hand sites wreck_meadow, farmstead,
// mill_race, truffle_glade, woodcutters_camp, hanging_oak, hernes_ford, falls_pool, root_bridge, otter_steps,
// frosted_ferns, nurse_log, bears_larder, wasp_ravine, coil_hollow, offering_stones, long_hall, cradle_wood,
// herb_terraces, bread_stone_lane, root_mine, sunken_orchard, crystal_roots, rootgrave_fort, lantern_line, fern_stair,
// canopy_lift, sky_larder, sinkroot, camp, village, cave_mouth (and the Root-Mine head as its cave); the kit trail_tree, fallen_giant, hedgerow_gap, beehive_tree,
// house_tree, kill_scene, ford, foxfire_bank, field_ruin, canopy_platform (wasp_nest and thornbrake_cache are the
// hazards track's: WRForestHazards.cpp). Arena landmarks (four_hearts, clear_cut, paper_hollow, bower) are the boss
// track's (WRForestBosses.cpp) and are skipped here.

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRForestTypes.h"
#include "WRForestWorld.generated.h"

class UBoxComponent;
class UPointLightComponent;
class UAudioComponent;
class AWRCharacterBase;
class AWRLand;
class AWRPlayerController;
class AWRPlayerState;
class AWRResourceNode;
class AWRWorldCache;

/** A spot that glows for a while (a cut bootlace rhizomorph's green ends: DC 2.7 "it glows for 1 game day after cutting"). */
USTRUCT()
struct FWRForestGlow
{
	GENERATED_BODY()
	UPROPERTY() FVector_NetQuantize At = FVector::ZeroVector;
	/** Game hours (AWRGameState::GetTotalHours) until which it glows. */
	UPROPERTY() float UntilHours = 0.f;
};

/** The one replicated actor for the forest's world state every machine draws: the glow marks. Late joiners get the list. */
UCLASS(NotPlaceable)
class WILDREACH_API AWRForestState : public AActor
{
	GENERATED_BODY()

public:
	AWRForestState();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	static AWRForestState* Get(const UObject* WorldContext);

	UPROPERTY(ReplicatedUsing=OnRep_Glows) TArray<FWRForestGlow> Glows;
	/** Server: a spot glows green until UntilHours. */
	void AddGlow(const FVector& At, float UntilHours);
	/** Server: drop the glows that are over. */
	void PruneGlows(double NowHours);

private:
	UFUNCTION() void OnRep_Glows();
	UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> GlowLights;
};

/** Static scenery: a landmark, a hand-made site, a kit POI, a walkway or a waterfall, modelled in code at real scale from
 *  the tree kit (WRTreeKit), the build look kit and code meshes (FWRToolMesh), in the forest's palette, with its
 *  collision fitted to what's drawn and its night light. Spawned locally on every machine (WRForestScenery.cpp). */
UCLASS(NotPlaceable)
class WILDREACH_API AWRForestScenery : public AActor
{
	GENERATED_BODY()

public:
	AWRForestScenery();
	FName Kind;
	FName SiteId;
	/** A cave's theme (badger_sett, alder_grotto, root_mine, foxfire_cellar, sinkroot): its mouth's dressing. */
	FName Theme;
	int32 Variant = 0;
	/** A second and third point some kinds need (a walkway's ends, a waterfall's lip and plunge point; world cm). */
	FVector Extra = FVector::ZeroVector, Extra2 = FVector::ZeroVector;
	/** A width or size some kinds need (a waterfall's width, cm). */
	float Size = 0.f;
	void Build();
	/** Every machine: lights that burn at night only (lit windows, lanterns, foxfire, glowworms), with their power. */
	UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> NightLights;
	TArray<float> NightPower;
	/** Glowing parts that only show at night (foxfire streaks, glowworm threads, the crystal roots' violet). */
	UPROPERTY(Transient) TObjectPtr<USceneComponent> NightRoot;
	/** The Great Falls' noon rainbow (11:00-13:00 on a clear day): where it stands, and the effect while it shows. */
	bool bRainbow = false;
	FTransform RainbowAt;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> RainbowFx;
	/** Effects that run all the time near the place (spray, chimney smoke), kept so EndPlay can stop them. */
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> Loops;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
};

/**
 * The Canopy Lift (DC 2.4 #33, plan 2.5): a Grovekin counterweight lift. A 30 m rope runs over a pulley wheel hung on an
 * oak beam in a giant's fork; the basket hangs on one fall of the rope and a stone counterweight in a rope cradle on the
 * other. Interact at either landing (or in the basket) pulls the stone: the basket rises (or sinks) at 150 cm/s while the
 * stone goes the other way the same distance, the wheel turning by the rope's travel. Every machine places both from the
 * replicated start time and direction, so nothing snaps; the basket's floor and rails are moving collision, so riders are
 * carried on every machine alike. One trip at a time; stepping off mid-way is a fall (the rails are at hip height).
 */
UCLASS(NotPlaceable)
class WILDREACH_API AWRCanopyLift : public AActor
{
	GENERATED_BODY()

public:
	AWRCanopyLift();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	/** Lift height (cm, basket floor at the bottom landing to the top landing) and the trunk it hangs beside. */
	UPROPERTY(Replicated) float Rise = 3000.f;
	UPROPERTY(Replicated) FVector_NetQuantize TrunkAt = FVector::ZeroVector;
	UPROPERTY(Replicated) float TrunkRadius = 300.f;
	/** Where the basket is going (true: up) and the server time the trip began; the position it started from (0..1). */
	UPROPERTY(ReplicatedUsing=OnRep_Trip) bool bGoingUp = false;
	UPROPERTY(ReplicatedUsing=OnRep_Trip) float TripStart = -1000.f;
	UPROPERTY(ReplicatedUsing=OnRep_Trip) float TripFrom = 0.f;

	/** 0 = basket at the bottom landing, 1 = at the top, at a server time. */
	float PositionAt(float ServerSeconds) const;
	/** Where the basket is now (0..1), on any machine. */
	float CurrentPosition() const { return PositionAt(ServerNow()); }
	bool IsMoving() const;
	/** Server: pull the stone (or let it down): start a trip toward the far end. Returns what the player is told. */
	FString Pull(AWRPlayerController* PC);
	/** Within reach to use it: in the basket or at either landing. */
	bool InReach(const FVector& At) const;
	/** World points: the basket's floor centre at position T, the bottom and top landings. */
	FVector BasketAt(float T) const;

private:
	UFUNCTION() void OnRep_Trip();
	void BuildLook();
	void Place(float T);
	float ServerNow() const;
	float WheelRadius() const;
	FVector WheelCentre() const;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Basket;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Stone;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Wheel;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> RopeBasket;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> RopeStone;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Creak;
	bool bLookBuilt = false;
	float LastT = -1.f;
};

/** A placed snare stake (DC 2.19): the first creature up to mount size that steps on it is Entangled 3 s (the Hornwarden
 *  too); then it's sprung and spent. Not inside claims. It rots after a game hour. Server-ticked, replicated look. */
UCLASS(NotPlaceable)
class WILDREACH_API AWRForestSnare : public AActor
{
	GENERATED_BODY()

public:
	AWRForestSnare();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	UPROPERTY(ReplicatedUsing=OnRep_Sprung) bool bSprung = false;
	FString Placer;
	TWeakObjectPtr<AActor> PlacerBody;
	double GoneAtHours = 0.0;

private:
	UFUNCTION() void OnRep_Sprung();
	void BuildLook();
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Noose;
};

DECLARE_MULTICAST_DELEGATE_ThreeParams(FWROnBlightLanded, UWorld* /*World*/, const FVector& /*At*/, AActor* /*Thrower*/);

/** Shapes the scenery (every machine) and the server agree on, so a thing the server places sits on what's drawn. */
namespace WRForestShape
{
	/** The standing tree of forest.json "giants" nearest At within Range (of Kind if given), or null. */
	WILDREACH_API const FWRForestData::FTree* GiantNear(const FVector& At, float Range, FName Kind = NAME_None);
	/** A tree kit species' trunk radius at breast height when grown to Height (cm). */
	WILDREACH_API float TrunkRadius(FName Species, float Height);
	/** The Hermit's Oak: its height and trunk radius (the "giants" row if there is one, else tuning). */
	WILDREACH_API void HermitOak(const FWRForestData::FSite& Oak, float& OutHeight, float& OutRadius);
	/** The Hermit's Oak's house floor (world cm, the floor's top in front of the hearth): Rowan's night spot. */
	WILDREACH_API FVector HermitHouseFloor(const FWRForestData::FSite& Oak);
	/** The deck height of a canopy platform at a kit row (cm): forest.json's own "deck" for the row, else the nearest
	 *  non-arena walkway end within 25 m (the walkways end on the deck), else 20-40 m by variant. */
	WILDREACH_API float CanopyDeckZ(const FWRForestData::FSite& Kit);
	/** A forest.json row's "deck" (a shrine's, a canopy platform's: cm above its ground point; 0 when it has none). */
	WILDREACH_API float RowDeck(FName RowId);
	/** The Canopy Lift's top (world Z): the height the canopy walkways that start at its giant end at (within the trunk
	 *  radius + 8 m), else its foot + forestWorld.lift.rise. The machine's rise and the gallery round the trunk use it. */
	WILDREACH_API float LiftTopZ(const FVector& Trunk, float TrunkR, float FootZ);

	/** The runtime land the forest's ground is (WRLand: Data/lands, biome forest), or null (no such land file: the ground
	 *  is whatever static world is there, a landscape). */
	WILDREACH_API AWRLand* Land(const UWorld* World);
	/** Every ground chunk of the forest's land exists (or there is no runtime land to wait for). Scenery, nodes and
	 *  caves wait for this so their traces find the ground on every machine alike. */
	WILDREACH_API bool GroundReady(const UWorld* World);
	/** The ground's height (world cm) under At: a trace that only counts the land's chunks (or the static world when there
	 *  is no runtime land) and, with bOnScenery, the forest's own built scenery (a platform, a log, a deck); anything else
	 *  (dressing trees, replicated actors a client may not have yet) is looked through so every machine gets the same
	 *  answer. No hit: the land's height function, else Fallback. */
	WILDREACH_API float GroundZ(const UWorld* World, const FVector& At, float Up, float Down, float Fallback, bool bOnScenery, const AActor* Ignore = nullptr);
	/** Where a shrine of the forest stands (world cm, the top of what it stands on) and which way it faces: the Falls
	 *  Shrine on the fishing platform in the Great Falls' pool, the Canopy Shrine on its deck 30 m up, the rest on the
	 *  ground at their forest.json row. Both the AWRShrine (WRWorldSystems) and its footing (the scenery) use this. */
	WILDREACH_API bool ShrineSpot(const UWorld* World, FName ShrineId, FVector& OutAt, float& OutYaw);
}

/** Runs the Ancient forest's world: builds it, refills its caches, keeps its glows and nodes, moves Rowan between the
 *  Grove Circle (day) and the Hermit's Oak (night), and answers the forest's interactions (the Canopy Lift, the offering
 *  stones) and items (the snare stake; the smoke pot and the Bootlace blight jar when they land). */
UCLASS()
class WILDREACH_API UWRForestWorld : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UWRForestWorld* Get(const UObject* WorldContext);
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UWRForestWorld, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return bActive; }
	virtual void Deinitialize() override;
	bool bActive = false;

	/** Server: a thrown Bootlace blight jar broke here (WRProjectile). The boss track subscribes (Root Knots, Old Gnarl's
	 *  stripped wood, tree-bodied creatures) to OnBlightLanded(). */
	static void BlightLanded(UWorld* World, const FVector& At, AActor* Thrower);
	static FWROnBlightLanded& OnBlightLanded();
	/** Server: a thrown smoke pot broke here (WRProjectile): 6 m of smoke for 10 s (WRForest::AddSmoke). */
	static void SmokePotLanded(UWorld* World, const FVector& At, AActor* Thrower);

	/** The interact prompt for something of the forest's in reach (WRWorld::Prompt). */
	static bool Prompt(const AWRPlayerController* PC, FString& OutTarget, FString& OutVerb);
	/** Client: E on something of the forest's sends its command (WRWorld::ClientInteract). */
	static bool ClientInteract(AWRPlayerController* PC);
	/** Server: "world forest <verb> ..." (WRWorld::Command). */
	static FString Command(AWRPlayerController* PC, const TArray<FString>& Args);
	/** Server: using a forest item from the bag (the snare stake). Empty when it isn't one of the forest's. */
	static FString UseItem(AWRPlayerState* PS, FName ItemId);

	/** Whether the Ancient forest's scenery is built in this world (it waits for the land's ground, then builds a few
	 *  places a frame). */
	bool IsBuilt() const { return bActive && bSceneryDone; }

private:
	struct FOffering { FVector At = FVector::ZeroVector; TMap<FString, int32> DayBy; };
	struct FNodeRef { TWeakObjectPtr<AWRResourceNode> Node; FName Kind; };
	/** One place waiting to be built (BuildScenery queues them all, Tick builds forestWorld.buildPerFrame a frame). */
	struct FPiece
	{
		FName Kind, Id, Theme;
		FVector At = FVector::ZeroVector, E1 = FVector::ZeroVector, E2 = FVector::ZeroVector;
		float Yaw = 0.f, Size = 0.f;
		int32 Variant = 0;
	};
	void BuildScenery();
	void StepBuild();
	TArray<FPiece> Pieces;
	int32 PiecesBuilt = 0;
	bool bQueued = false;
	bool bSceneryDone = false;
	bool bServerDone = false;
	float WaitClock = 0.f;
	void SpawnServerActors();
	void SpawnNode(FName Item, const FVector& At, FName Kind, int32 Hits, float PerHit, float RespawnSeconds);
	void SpawnCache(const FVector& At, float Yaw, const FString& Title, const TMap<FName, int32>& Items, const FString& Lore, float RefillHours);
	void StepLights(float Dt);
	void StepServer(float Dt);
	void StepRowan(float Dt);
	FString Offer(AWRPlayerController* PC);
	const FOffering* OfferingNear(const FVector& At, float Reach) const;
	AWRCanopyLift* LiftNear(const FVector& At) const;
	TArray<TWeakObjectPtr<AWRForestScenery>> Scenery;
	TArray<TWeakObjectPtr<AWRWorldCache>> Caches;
	TArray<FNodeRef> Nodes;
	TArray<FOffering> Offerings;
	TArray<TWeakObjectPtr<AWRCanopyLift>> Lifts;
	UPROPERTY(Transient) TObjectPtr<AWRForestState> State;
	float LightClock = 0.f;
	float ServerClock = 0.f;
	float RowanClock = 0.f;
	int32 RowanNight = -1;
};
