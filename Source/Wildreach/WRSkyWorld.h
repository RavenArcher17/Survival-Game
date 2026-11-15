#pragma once

// The Sky islands on the map (cooks/biome-sky.md 2.2-2.8, 2.13-2.15; cooks/build-biome-sky.md 2.2-2.10, 2.15-2.16):
// everything the game builds over the Hollow Cape at runtime from Data/sky.json (Tools/terrain/sky.mjs, parsed by
// FWRSkyIsleData in WRSkyIsleTypes.cpp). Static scenery (the 44 islands, the landmarks, the hand sites, the kit POIs,
// the bridges and chain walks, the wind-poles, the waterfalls, the Flue's shaft, the Great Kite, the Undercloud's bank)
// is built on every machine with no network cost, on the forest's scenery kit (WRSkyScenery*.cpp, WRSkyIsland.cpp).
// Things with state are the server's and replicate: the gathering, the caches, the spots you use, the Kitewright
// lifts, the sinking stones, the floating shards, the Last Kite's line, the cloud bridges, and the Sky's state (the
// Turning Wind, the Toll Shrine's lanterns, the Bonebell, the Wind Rose).

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRForestWorld.h"
#include "WRSkyWorld.generated.h"

class AWRPlayerController;
class AWRPlayerState;
class AWRCharacter;
class AWRCharacterBase;
class AWRResourceNode;
class AWRWorldCache;
class UPointLightComponent;
class UBoxComponent;
class UProceduralMeshComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FWRSkyWho, AWRCharacterBase*);

/** One place of the Sky, modelled in code at real scale on the forest's FBuild. Kind: an island, a site's or kit row's
 *  kind, or bridge / poles / falls / flue / bank / road / decor. Pieces on an island that drifts ride its actor. */
UCLASS(NotPlaceable)
class WILDREACH_API AWRSkyScenery : public AWRForestScenery
{
	GENERATED_BODY()

public:
	AWRSkyScenery();
	virtual void Tick(float DeltaSeconds) override;
	/** The row it was built from (an island, a bridge, a fall: their index). */
	int32 Index = -1;
	FName Island;
	void BuildSky();
	/** Parts that move on their own (the Fallen Span's pendulum, the Great Kite's yaw, the Wind Wheel's hub, wind-pole
	 *  cloth): every machine turns them from the server time, so they glide and never snap. */
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Swing;
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> Cloths;
	TArray<float> ClothPhase;
	/** The Toll Shrine's lanterns on the hanging deck (shown once it is lit). */
	UPROPERTY(Transient) TObjectPtr<USceneComponent> TollLamps;
	float SwingPeriod = 22.f, SwingDegrees = 0.6f;
	/** The Sentinel's deck (SKY 2.10): its slabs one by one (the weak ones flicker; a thrown slab leaves a hole). */
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Slabs;
	TArray<bool> SlabWeak;
	/** An island's own look: its night keel glow kept so the sky's light rules can dim it. */
	bool bIslandDrifts = false;
	FVector Home = FVector::ZeroVector;
	float ClothClock = 0.f;

	// Builders (WRSkyIsland.cpp, WRSkyScenery.cpp, WRSkySites.cpp).
	void BuildIsland();
};

/** The Sky's world state every machine draws (late joiners get it). */
UCLASS(NotPlaceable)
class WILDREACH_API AWRSkyState : public AActor
{
	GENERATED_BODY()

public:
	AWRSkyState();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;
	static AWRSkyState* Get(const UObject* WorldContext);

	/** The Turning Wind (SKY 2.15): server times of its sign (-5 min), start and end (0 = none). */
	UPROPERTY(Replicated) float TurnSignAt = 0.f;
	UPROPERTY(Replicated) float TurnStartAt = 0.f;
	UPROPERTY(Replicated) float TurnEndAt = 0.f;
	/** The Toll Shrine lit (its lanterns on the Fallen Span). */
	UPROPERTY(Replicated) bool bTollLit = false;
	/** The Bonebell's last stroke (server time): every kite in the bank shakes for 20 s. */
	UPROPERTY(Replicated) float BonebellAt = -1000.f;
	/** The Wind Rose's floor plate turned (server time; it turns back after an hour). */
	UPROPERTY(Replicated) float RoseTurnedAt = -1000.f;
	/** The Flue's spore plume: which vent and when it last breathed (WRSkyBosses; every machine draws the column). */
	UPROPERTY(Replicated) float PlumeAt = -1000.f;
	UPROPERTY(Replicated) int32 PlumeVent = 0;

	/** 0..1: how far the wind has swung (eased over 120 s each way); every machine. */
	float TurnAmount(float Now) const;
	/** 0..1: how far a drifting island has gone along its spline (1 m/s, then holds; home at the end). */
	float DriftAmount(float Now, float SplineLength) const;
	/** The cloud bridges' state: 0 gone, 1 whole; Fray > 0 while their ends fray (the last minute's fade). */
	float BridgeAmount(float Now) const;
	bool TurningNow(float Now) const { return TurnStartAt > 0.f && Now >= TurnStartAt && Now < TurnEndAt; }
	bool SignNow(float Now) const { return TurnSignAt > 0.f && Now >= TurnSignAt && Now < TurnEndAt + 120.f; }
};

/**
 * A thing of the Sky you use with E (held for Seconds, standing within Reach). Kinds:
 *   keepsake  a lore item at its story site: each survivor takes one (SKY 2.14)
 *   gossamer  a gossamer drift: wind Skysilk onto a spindle, 4 s (x2 in rain), back a game day per survivor
 *   vista     a high place that pays path XP and reveals the map round it once each (the Kite Field, the anchor drum,
 *             the First Kite, the vista stones)
 *   bonebell  the Bonebell: strike it (every kite in the bank shakes; the roost wakes; in the Turning Wind, the echo)
 *   rose      the Wind Rose's eight winds: stand on the one the Great Kite points away from at noon and use it
 *   drum      the Great Kite's anchor drum (a vista)
 *   bowls     the Toll House's offering bowls (copper, and the Sentinel wakes)
 *   station   the Lift Station's winch house (the Updraft lift blueprint)
 *   skiff     the pearl lagoon's sunken skiff (12 m down)
 */
UCLASS(NotPlaceable)
class WILDREACH_API AWRSkySpot : public AActor
{
	GENERATED_BODY()

public:
	AWRSkySpot();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(Replicated) FName Kind;
	UPROPERTY(Replicated) FName SpotId;
	UPROPERTY(Replicated) FString Title;
	UPROPERTY(Replicated) FString Verb;
	UPROPERTY(Replicated) float Seconds = 0.f;
	UPROPERTY(Replicated) float DoneAt = 0.f;
	UPROPERTY(Replicated) FString Doer;
	float Reach = 300.f;

	// Server.
	TMap<FName, int32> Gives;
	FName Quest;
	int32 Xp = 0;
	float RevealRadius = 0.f;
	float RespawnHours = 0.f;     // per survivor (gossamer); 0 = once each
	TMap<FString, double> DoneBy; // survivor -> game hours it was done
	TWeakObjectPtr<AWRCharacterBase> DoerPawn;
	FVector DoerFrom = FVector::ZeroVector;

	FString Start(AWRPlayerController* PC);
	bool IsDoneBy(const FString& Player, double NowHours) const;

private:
	void Finish();
};

/**
 * A Kitewright lift (SKY L6, world-map 2.5): a timber headframe on the island's lip, a 3.2 m iron sheave on its cross-
 * head, the cage (2.4 x 2.4 x 2.6 m, iron frame, plank floor, waist rails, a gate) on twin chains, a stone counterweight
 * in an iron cradle on the far fall, a winch (drum, crank, pawl) beside the frame. E at either stage or in the cage
 * starts the crank; it climbs the whole rise in 40 s at a steady pace (eased 1.5 s at each end) while the cranker stays
 * at the winch or in the cage; leave and the pawl holds it; E mid-way reverses it. Every machine places the cage and the
 * counterweight from the replicated trip (start time, from, direction), the sheave turning by the chain's travel: no
 * snaps. The cage floor and rails are moving collision, so riders are carried alike everywhere.
 */
UCLASS(NotPlaceable)
class WILDREACH_API AWRSkyLift : public AActor
{
	GENERATED_BODY()

public:
	AWRSkyLift();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** The cage floor's rise (cm, foot stage to the top stage). The actor stands at the foot stage's cage spot. */
	UPROPERTY(Replicated) float Rise = 30000.f;
	UPROPERTY(Replicated) float FaceYaw = 0.f;
	UPROPERTY(ReplicatedUsing=OnRep_Trip) bool bUp = true;
	UPROPERTY(ReplicatedUsing=OnRep_Trip) bool bMoving = false;
	UPROPERTY(ReplicatedUsing=OnRep_Trip) float TripStart = -1000.f;
	UPROPERTY(ReplicatedUsing=OnRep_Trip) float TripFrom = 0.f;
	TWeakObjectPtr<AWRCharacterBase> Cranker;
	FName LiftId;

	/** 0 = cage at the foot stage, 1 = at the top, at a server time. */
	float PositionAt(float ServerSeconds) const;
	float CurrentPosition() const;
	/** Within reach of a winch, a stage or in the cage. */
	bool Near(const FVector& At) const;
	/** Server: E pressed by Who. */
	FString Use(AWRCharacterBase* Who);

private:
	UFUNCTION() void OnRep_Trip();
	void BuildLook();
	void Place(float Pos);
	float Seconds() const;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Cage;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Weight;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Sheave;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Crank;
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> CageChains;
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> WeightChains;
	UPROPERTY(Transient) TObjectPtr<class UAudioComponent> Rattle;
	float LastPos = -1.f;
	bool bBuilt = false;
};

/** A levistone shard knocked loose (a struck shard node, a Turning Wind shard fall): it floats up 1 m and hangs glowing
 *  for 3 s; grab it (walk into it or E) or it drifts up into the sky and is gone. Server-timed, replicated. */
UCLASS(NotPlaceable)
class WILDREACH_API AWRSkyShard : public AActor
{
	GENERATED_BODY()

public:
	AWRSkyShard();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	UPROPERTY(Replicated) float BornAt = 0.f;
	UPROPERTY(Replicated) FVector_NetQuantize Origin = FVector::ZeroVector;
	UPROPERTY(Replicated) int32 Count = 1;
	UPROPERTY(Replicated) FName ItemId;
	/** Server: who may take it first (the striker) for the first second; anyone after. */
	TWeakObjectPtr<AActor> Striker;
	FVector WhereAt(float Now) const;
	bool TryTake(AWRCharacterBase* Who);

private:
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Look;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> Glow;
};

/** The Last Kite's line (SKY 2.7): a silk-and-chain line from the Last Kite up through the cloud to the Kitefield's lip.
 *  E within reach of it grabs it; you climb at 3 m/s, no stamina (W/S or the stick up and down), Jump lets go. The server
 *  holds the climbers on it (their position along it replicates as any movement does). */
UCLASS(NotPlaceable)
class WILDREACH_API AWRSkyLine : public AActor
{
	GENERATED_BODY()

public:
	AWRSkyLine();
	virtual void Tick(float DeltaSeconds) override;
	FVector A = FVector::ZeroVector, B = FVector::ZeroVector;
	float Sag = 1500.f;
	FVector PointAt(float T) const;
	/** The line's nearest T to a point and how far off it is. */
	float Nearest(const FVector& P, float& OutDist) const;
	/** Server: Who grabs it (or lets go). */
	FString Grab(AWRCharacterBase* Who);
	void Release(AWRCharacterBase* Who, bool bJump);
	bool Holds(const AActor* Who) const;
	struct FClimber { TWeakObjectPtr<AWRCharacterBase> Who; float T = 0.f; };
	TArray<FClimber> Climbers;
	void BuildLook();
};

/** The sinking stones (W2, SKY 2.5): flat levistone stones in lines between garden islands. Land on one and its keel
 *  glow flickers, dust trickles and it groans (1.0 s), then it dips 1 m, sinks at 4 m/s for 6 s, holds, and rises back
 *  over 20 s. A stone's height is a pure function of the server time it was set off (replicated), so every machine
 *  places it the same and its collision carries riders. One actor holds a whole line. */
UCLASS(NotPlaceable)
class WILDREACH_API AWRSkyStones : public AActor
{
	GENERATED_BODY()

public:
	AWRSkyStones();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	UPROPERTY(Replicated) int32 Line = 0;
	/** Per stone: the server time it was set off (-1000: at rest). */
	UPROPERTY(Replicated) TArray<float> SetOffAt;
	/** How far down a stone is (cm) a given seconds after it was set off. */
	static float DropAt(float Since);
	/** Seconds from a set-off until the stone is home again (it can be set off again then). */
	static float DropTotal();

private:
	TArray<int32> Stones;    // indexes into FWRSkyIsleData::Stones
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> Parts;
	UPROPERTY(Transient) TArray<TObjectPtr<UBoxComponent>> Boxes;
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> Glows;
	TArray<bool> Told;
	void BuildLook();
};

/** The Turning Wind's cloud bridges and drifting islands (SKY 2.15), on every machine from AWRSkyState's times: six
 *  bridges of dense cloud (4 m wide, walkable, lit from inside) along the event's splines, fraying grey 1.0 s before a
 *  section fades from the ends in; small islands drifting 40-120 m at 1 m/s and holding, carrying what stands on them. */
UCLASS(NotPlaceable)
class WILDREACH_API AWRSkyEventLook : public AActor
{
	GENERATED_BODY()

public:
	AWRSkyEventLook();
	virtual void Tick(float DeltaSeconds) override;
	void BuildLook();

private:
	struct FSeg { int32 Bridge = 0; float T0 = 0.f, T1 = 0.f; };
	TArray<FSeg> Segs;
	UPROPERTY(Transient) TArray<TObjectPtr<UBoxComponent>> SegBoxes;
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> SegLooks;
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> SegFray;
	bool bBuilt = false;
};

/** Runs the Sky's world: builds it, refills its caches, keeps its night lights, answers its spots and machines. */
UCLASS()
class WILDREACH_API UWRSkyWorld : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UWRSkyWorld* Get(const UObject* WorldContext);
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UWRSkyWorld, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return bActive; }
	bool bActive = false;

	/** The interact prompt for a Sky thing in reach (WRWorldNpc). */
	static bool Prompt(const AWRPlayerController* PC, FString& OutTarget, FString& OutVerb);
	/** Client: E on a Sky thing sends its command. */
	static bool ClientInteract(AWRPlayerController* PC);
	/** Server: "world sky <verb> ..." (WRWorldSystems). */
	static FString Command(AWRPlayerController* PC, const TArray<FString>& Args);

	bool IsBuilt() const { return bActive && bSceneryDone; }
	AWRSkySpot* SpotNear(const FVector& At) const;
	AWRSkyLift* LiftNear(const FVector& At) const;
	AWRSkyLine* LineNear(const FVector& At, bool bHeld = false, const AActor* Who = nullptr) const;
	AWRSkyShard* ShardNear(const FVector& At, float Reach) const;
	/** Server: a levistone shard knocked loose at At (a node struck, a shard fall). */
	AWRSkyShard* SpawnShard(const FVector& At, FName Item, int32 Count, AActor* Striker);
	/** The scenery piece built for an island (its actor: drifting islands move it). */
	AWRSkyScenery* IslandActor(FName Island) const;
	/** Server: the Toll Shrine was kindled (its lanterns on the Fallen Span). */
	void SetTollLit();
	/** Server: the Bonebell was struck by Who. */
	void StrikeBonebell(AWRCharacterBase* Who);

	/** Server: the Toll House's copper was taken (the Sentinel wakes); the Bonebell was struck (the roost wakes). */
	FWRSkyWho OnTollTaken;
	FWRSkyWho OnBonebell;
	TArray<TWeakObjectPtr<AWRSkyScenery>> Scenery;
	TArray<TWeakObjectPtr<AWRSkyLift>> Lifts;
	TArray<TWeakObjectPtr<AWRSkyStones>> StoneLines;
	TWeakObjectPtr<AWRSkyLine> KiteLine;

private:
	struct FPiece
	{
		FName Kind, Id, Island;
		FVector At = FVector::ZeroVector;
		float Yaw = 0.f;
		int32 Index = -1, Variant = 0;
	};
	void BuildScenery();
	void StepBuild();
	void SpawnServerActors();
	AWRResourceNode* SpawnNode(FName Item, const FVector& At, int32 Hits, float PerHit, float RespawnSeconds, float Yaw = -1.f);
	AWRWorldCache* SpawnCache(const FVector& At, float Yaw, const FString& Title, const TMap<FName, int32>& Items, const FString& Lore, float RefillHours);
	AWRSkySpot* SpawnSpot(FName Kind, FName Id, const FVector& At, const FString& Title, const FString& Verb, float Seconds);
	void StepLights(float Dt);
	void StepServer(float Dt);
	void StepDrift();
	TArray<FPiece> Pieces;
	int32 PiecesBuilt = 0;
	bool bQueued = false;
	bool bSceneryDone = false;
	bool bServerDone = false;
	float WaitClock = 0.f;
	TArray<TWeakObjectPtr<AWRWorldCache>> Caches;
	TArray<TWeakObjectPtr<AWRSkySpot>> Spots;
	TArray<TWeakObjectPtr<AWRSkyShard>> Shards;
	TMap<FName, TWeakObjectPtr<AWRSkyScenery>> IslandActors;
	UPROPERTY(Transient) TObjectPtr<AWRSkyState> State;
	UPROPERTY(Transient) TObjectPtr<AWRSkyEventLook> EventLook;
	float LightClock = 0.f;
	float ServerClock = 0.f;
	float NodeClock = 0.f;
	int32 ShardFallsDone = 0;
	/** Who has had the Bonebell's echo (the Kitewright's last kite: once each). */
	TSet<FString> BonebellEcho;
};
