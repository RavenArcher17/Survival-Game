#pragma once

// The Swamp on the map (cooks/biome-swamp.md 2.1-2.7, 2.20; cooks/build-biome-swamp.md 2.4): everything the game builds on
// the Swamp landscape at runtime from Data/swamp.json (Tools/terrain/swamp.mjs, parsed by WRSwampTypes). The landmarks,
// the 24 hand-made sites, the kit POIs, the camps, the route edges and the water mats are static scenery, built on every
// machine from the same data (no network cost). The things with state the server spawns and replicates: the Coven's
// boardwalks and their planks, the site caches, the resource nodes, the caves and the Peat Cellar's wall.
// AWRSwampState (WRSwampTypes.h) carries the Red Tide: the falls run oxblood, the mist column doubles and the M3-M5 water
// mats turn to bleedwater on every machine when it changes.
// Numbers: tuning.json "swampWorld".

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PhysicsVolume.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRSwampTypes.h"
#include "WRSwampWorld.generated.h"

class UBoxComponent;
class UPointLightComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UInstancedStaticMeshComponent;
class AWRPlayerController;
class AWRResourceNode;
class AWRWorldCache;
class AWRCave;
class FJsonObject;

/** Static scenery: a landmark, a hand-made site, a kit POI, a camp or a route's edge dressing, modelled in code from the
 *  build kit at real scale in the swamp palette, with collision fitted to what's drawn and its night lights. Spawned
 *  locally on every machine. */
UCLASS(NotPlaceable)
class WILDREACH_API AWRSwampScenery : public AActor
{
	GENERATED_BODY()

public:
	AWRSwampScenery();
	FName Kind;
	FName Id;
	int32 Variant = 0;
	/** The site's whole row (kind-specific fields), or null. */
	TSharedPtr<FJsonObject> Row;
	/** A route's points for route kinds (world cm). */
	TArray<FVector> Points;
	float Width = 200.f;
	void Build();

	/** Lights that only burn at night (camp fires, the Walking House window, bone lanterns, toad rings), each with its
	 *  full intensity in NightBase. */
	UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> NightLights;
	TArray<float> NightBase;
	/** Lights that pulse at night (Lanterntoad throat sacs), full intensity in PulseBase. */
	UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> PulseLights;
	TArray<float> PulseBase;
	/** Things hung on chains and cords (cages, bottle charms, bone chimes) that sway about their hook: each turns a little
	 *  round its pivot (client, near the camera). */
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> Swingers;
	/** The kit cache spot for kinds that hold one (a fallen cage, a punt's locker), local; zero: none. */
	FVector CacheSpot = FVector::ZeroVector;
	/** FX that only show at night (toad glows, corpse candles on the Mere's shore). */
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> NightFx;
	/** The Weeping Wall's falls: the rust-red set and the Red Tide's oxblood set (one shows). */
	UPROPERTY(Transient) TObjectPtr<USceneComponent> FallsNormal;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> FallsTide;
	/** The Throat's mist column: the second, doubled layer that rises in the Red Tide, and the glow under it. */
	UPROPERTY(Transient) TObjectPtr<USceneComponent> MistTide;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> MistGlow;
	/** Is this the Red Tide look. */
	bool bTideLook = false;
	void SetTideLook(bool bTide);
	/** The third fall's ribbons (its rust and its oxblood set, each pivoted on the fall's centre line): they narrow at
	 *  sunset so the ledge behind shows (M-S4). */
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> FallThin;
	/** Lights that flash now and then at night (the lightning in Stormgut behind the Bled Roc), full intensity in FlickerBase. */
	UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> FlickerLights;
	TArray<float> FlickerBase;
	/** Underground (the Sunken Coven): its lights burn day and night. */
	bool bUnder = false;
	/** A Sunken Coven place by its swamp.json name, in the Coven's space (AWRSwampCoven::ToCoven / RoomBox): a room's
	 *  floor centre and half size, or an entry's point (mouth, chimney, cellar) with a zero half size. */
	static bool CovenPoint(FName Name, FVector& OutFloor, FVector& OutHalf);
	/** The Coven's lanes as swamp.json coven.lanes lists them (pairs of room / entry names). */
	static void CovenLanes(TArray<TPair<FName, FName>>& Out);
	/** Where the lane between From and To leaves From (Coven space, at From's walk floor): the edge of From's box (an
	 *  entry's landing is a 6 x 6 m chamber) on the line toward To. */
	static FVector CovenLaneEnd(FName From, FName To);
	/** A room's walk floor (Coven space Z): its floor, the Old Gar's chapel its fishing ledge; an entry's is the floor of
	 *  the room its lane leads to. */
	static float CovenWalkFloor(FName Name);

private:
	void BuildCovenRoom(struct FWRSwampBuild& B);
	void BuildCovenLane(struct FWRSwampBuild& B);
	void BuildCandlePath(struct FWRSwampBuild& B);
	void BuildFallLedge(struct FWRSwampBuild& B);
	void BuildLandmark(struct FWRSwampBuild& B);
	void BuildSite(struct FWRSwampBuild& B);
	void BuildKit(struct FWRSwampBuild& B);
	void BuildRouteEdge(struct FWRSwampBuild& B);
	void BuildFalls(struct FWRSwampBuild& B);
};

/** A Coven boardwalk (cooks/biome-swamp.md 2.8, build plan 2.4): deck boards 2 m wide on paired cypress posts every 2.4 m,
 *  sunk a metre into the mud and cross-braced. Each 2.4 m bay is one plank unit with its own state: sound (grey-green),
 *  rotten (black boards, shelf fungus, a 6 cm sag: it creaks, then goes 0.9 s later under a run, a roll, a heavy landing
 *  or two players), gone, a gap from the start (the data's broken places), or mended (fresh light wood: holds 4 game days
 *  then the swamp has it rotten again). A gap left alone mends itself after 4 game days. Anyone can lay a plank (2 wood). */
UCLASS(NotPlaceable)
class WILDREACH_API AWRSwampBoardwalk : public AActor
{
	GENERATED_BODY()

public:
	AWRSwampBoardwalk();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	enum EBay : uint8 { Sound = 0, Rotten = 1, Gone = 2, Gap = 3, Mended = 4, MendedGap = 5 };

	/** The route in Data/swamp.json (every machine reads its points from there). */
	UPROPERTY(Replicated) FName RouteId;
	/** One byte per 2.4 m bay (EBay). */
	UPROPERTY(ReplicatedUsing=OnRep_Bays) TArray<uint8> Bays;
	/** Per bay, the game hour a mended plank lets go again or a gone one grows back (0: never), for the prompt. */
	UPROPERTY(Replicated) TArray<float> BackAt;
	/** Every machine plays a bay's moment where it is: 0 the creak (the 0.9 s tell), 2 a plank laid. */
	UFUNCTION(NetMulticast, Unreliable) void MulticastPlank(int32 InBay, uint8 What);
	/** Can a plank be laid / replaced at this bay (rotten, gone, or a gap from the start). */
	bool IsRepairable(int32 InBay) const { return Bays.IsValidIndex(InBay) && (Bays[InBay] == Rotten || Bays[InBay] == Gone || Bays[InBay] == Gap); }
	/** Is anyone standing on this bay (the plank can't be laid under them). */
	bool IsOccupied(int32 InBay) const;

	int32 BayCount() const { return Bays.Num(); }
	/** The deck's centre over bay i (world). */
	FVector BayCentre(int32 i) const;
	/** The nearest gap within Reach (INDEX_NONE if none): what a player standing there could mend. */
	int32 GapNear(const FVector& At, float Reach) const;
	/** The bay under a point (INDEX_NONE off the deck). */
	int32 BayAt(const FVector& At) const;
	/** Server: lay a plank in a gap (2 wood, not while anyone stands over it). */
	FString Mend(AWRPlayerController* PC, int32 InBay);
	/** Server: a bay goes (a boss's slam, a rotten plank's end). Returns false if there was nothing there. */
	bool BreakBay(int32 InBay);

private:
	UFUNCTION() void OnRep_Bays();
	void ReadRoute();
	void BuildFrame();
	void BuildDeck();
	FVector DeckAt(float Dist) const;
	FVector DirAt(float Dist) const;
	TArray<FVector> Line;      // deck line (world, deck height)
	TArray<float> LineDist;    // distance along it at each point
	float Length = 0.f;
	float Bay = 240.f;
	float HalfW = 100.f;
	TArray<uint8> Base;        // what each bay goes back to (rotten or gap)
	TArray<float> DeckZ;       // deck height at each line point (world)
	UPROPERTY(Transient) TArray<TObjectPtr<UBoxComponent>> BayBoxes;
	/** The deck boards in chunks of bays (a chunk is rebuilt when one of its bays changes; the frame stays). */
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> DeckChunks;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> DeckRoot;
	TArray<uint8> Shown;       // what each bay was last drawn as
	FBox2D Bounds = FBox2D(ForceInit);
	TArray<double> BreakAt;
	TMap<TWeakObjectPtr<AActor>, float> LastFallSpeed;
	TSet<int32> Creaked;
	bool bBuilt = false;
	float CheckClock = 0.f;
	float MendClock = 0.f;
};

/** The Eel Hole's tide (cook 2.6): each 12 game hours the sea comes in for one game hour and fills the cave. A water
 *  physics volume (swimming, like the sea's) and the surface you see, both from the clock, so every machine agrees. */
UCLASS(NotPlaceable)
class WILDREACH_API AWRSwampCaveTide : public APhysicsVolume
{
	GENERATED_BODY()

public:
	AWRSwampCaveTide(const FObjectInitializer& ObjectInitializer);
	virtual void Tick(float DeltaSeconds) override;
	/** The cave's local box (cm) the water fills, from FloorZ (world) up to FloorZ + Depth at the flood's height. */
	void Setup(const FTransform& Cave, const FVector& Half, float FloorZ, float Depth);
	/** 0..1 how full it is now (rising over the first tenth of the hour, falling over the last). */
	static float Fullness(const UObject* WorldContext);

private:
	UPROPERTY(Transient) TObjectPtr<class UStaticMeshComponent> Surface;
	FVector BoxHalf = FVector(900.f, 900.f, 300.f);
	float Floor = 0.f;
	float MaxDepth = 250.f;
	float Shown = -1.f;
};

/** The Peat Cellar's diggable wall (cook 2.6): a face of cut peat closing a side gallery. Four pick hits on its peat node
 *  (the server's AWRResourceNode in front of it) and the wall falls in; it packs back when the peat grows back. */
UCLASS(NotPlaceable)
class WILDREACH_API AWRSwampPeatWall : public AActor
{
	GENERATED_BODY()

public:
	AWRSwampPeatWall();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	UPROPERTY(ReplicatedUsing=OnRep_Open) bool bOpen = false;
	/** The wall's half size (cm, local): replicated so every machine builds the same face. */
	UPROPERTY(Replicated) FVector_NetQuantize WallHalf = FVector(30.f, 250.f, 300.f);
	void SetOpen(bool bInOpen);

private:
	UFUNCTION() void OnRep_Open();
	void BuildLook();
	UPROPERTY(Transient) TObjectPtr<UBoxComponent> Blocker;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Face;
	/** The face fallen in: the slumped heap of turves and roots it leaves. */
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Heap;
	bool bBuilt = false;
};

/** The water mats (cook 2.1 read rule 3, build plan 2.4): duckweed, bleedwater, quag, brackish and murk drawn over the
 *  water-kind grid just above the sea's surface, as merged rectangles in chunks. Every machine that draws builds them; a
 *  Red Tide change redraws them (M3-M5 murk and duckweed run as bleedwater while it lasts). */
UCLASS(NotPlaceable)
class WILDREACH_API AWRSwampWaterMats : public AActor
{
	GENERATED_BODY()

public:
	AWRSwampWaterMats();
	void Rebuild(bool bTide);
	bool bBuiltTide = false;
	bool bBuilt = false;
	/** Where bleedwater (and quag) lies near a point, for the ripple and bubble sprites (cell centres). */
	void NearCells(const FVector& At, float Radius, TArray<FVector>& OutBleed, TArray<FVector>& OutQuag) const;

private:
	UPROPERTY(Transient) TArray<TObjectPtr<UActorComponent>> Parts;
	TArray<uint8> Kinds;   // per cell, the kind drawn (EWRSwampWater)
};

/** Runs the Swamp's world: builds it, spawns its stateful actors, switches the Red Tide look, lights the nights, floods
 *  the Eel Hole, keeps the Peat Cellar's wall, refills the caches, and drifts the corpse candles over the Mere. */
UCLASS()
class WILDREACH_API UWRSwampWorld : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UWRSwampWorld* Get(const UObject* WorldContext);
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UWRSwampWorld, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return bActive; }

	/** The boardwalk whose gap a player could mend, for the interact prompt (and the server's command). */
	AWRSwampBoardwalk* BoardwalkGapNear(const FVector& At, int32& OutBay) const;
	/** The boardwalk bay under a point, for anything that breaks planks (Mother Maw's slams). */
	AWRSwampBoardwalk* BoardwalkAt(const FVector& At, int32& OutBay) const;
	/** Interact wiring (WRSwamp::Prompt / ClientInteract / Command call these): a gap in a boardwalk at your feet.
	 *  PlankPrompt fills the prompt; PlankCommand is the server's "world swamp plank". */
	static bool PlankPrompt(const AWRPlayerController* PC, FString& OutTarget, FString& OutVerb);
	static FString PlankCommand(AWRPlayerController* PC);
	bool bActive = false;

private:
	void BuildScenery();
	void SpawnServerActors();
	void SpawnCaves();
	AWRResourceNode* SpawnNode(FName Item, const FVector& At, int32 Hits, int32 PerHit, float RespawnDays);
	AWRWorldCache* SpawnCache(const FVector& At, float Yaw, const FString& Title, const TMap<FName, int32>& Items, const FString& Lore);
	void StepTide(float Dt);
	void StepLights(float Dt);
	void StepWaterLife(float Dt);
	void StepServer(float Dt);
	void StepNodeLooks(float Dt);
	void SpawnCaveTides();
	void SpawnStarts();
	void SpawnNodes();
	void SpawnCaches();
	AWRSwampScenery* SpawnScenery(FName Kind, FName Id, int32 Variant, const FTransform& T, const TSharedPtr<FJsonObject>& Row, const TArray<FVector>* Points = nullptr, float Width = 200.f);
	/** Waits for the swamp's ground to stream in before building on it (a trace each second, a minute at most). */
	bool GroundReady();
	bool bBuilt = false;
	int32 WaitTries = 0;
	float WaitClock = 0.f;
	float NodeClock = 0.f;
	float SwayTime = 0.f;
	TMap<FName, TWeakObjectPtr<AWRSwampScenery>> SiteScenery;
	TArray<TWeakObjectPtr<AWRSwampScenery>> KitScenery;
	TSet<TWeakObjectPtr<AWRResourceNode>> Dressed;
	TArray<TWeakObjectPtr<AWRSwampScenery>> Scenery;
	TArray<TWeakObjectPtr<AWRWorldCache>> Caches;
	TWeakObjectPtr<AWRSwampWaterMats> Mats;
	struct FPeat { TWeakObjectPtr<AWRResourceNode> Node; TWeakObjectPtr<AWRSwampPeatWall> Wall; float Dug = 0.f; };
	/** Scenery waiting to be built: the server builds it all once the ground is in, a client each piece when its viewer
	 *  comes within Range of Probe (the Sunken Coven's when they're in it). */
	struct FPendingScenery
	{
		FName Kind, Id;
		int32 Variant = 0;
		FTransform T;
		TSharedPtr<FJsonObject> Row;
		TArray<FVector> Points;
		float Width = 200.f;
		FVector Probe = FVector::ZeroVector;
		float Range = 0.f;
	};
	TArray<FPendingScenery> PendingScenery;
	void QueueScenery();
	void StepPending(float Dt);
	float PendingClock = 0.f;
	/** Every machine that draws: the Eel Hole's tide water. */
	TArray<TWeakObjectPtr<AWRSwampCaveTide>> Tides;
	TArray<TWeakObjectPtr<AWRSwampBoardwalk>> Boardwalks;
	bool bStartsChecked = false;
	bool bTideLookShown = false;
	float FallThinShown = 1.f;
	TArray<FPeat> PeatWalls;
	/** Corpse candles over the Mere (client): each drifts low along a loop round the Mere, only at night. */
	struct FCandle { TWeakObjectPtr<USceneComponent> Fx; FVector Centre; float Radius = 0.f; float Phase = 0.f; float Speed = 0.f; };
	TArray<FCandle> Candles;
	UPROPERTY(Transient) TObjectPtr<AActor> CandleHost;
	float LightClock = 0.f;
	float LifeClock = 0.f;
	float ServerClock = 0.f;
	int32 LastTide = -1;
	bool bNightShown = false;
	bool bCandlesShown = false;
	bool bLightsInit = false;
};
