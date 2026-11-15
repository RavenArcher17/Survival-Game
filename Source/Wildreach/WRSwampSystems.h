#pragma once

// The Swamp's hazards, events and hidden places (cooks/biome-swamp.md 2.5, 2.7-2.11, 2.20, 2.21;
// cooks/build-biome-swamp.md 2.5, 2.10). Everything here reads Data/swamp.json (FWRSwampData) and does nothing when it
// is missing or the swamp isn't live. Numbers: tuning swampHazards.* and redTide.*.
//  - Bleedwater (4 Hz on the server): feet in a bleedwater cell build bleed 10/s through the status component (the
//    proc's x1.3 growth is the status component's); never slows. Passes: sealed waders (worn passBleedwater), Mirehide 4,
//    a mount that resists bleedwater; leech tar x0.3. The 0.9 s ripple tell and the wet suck play on the entering
//    player's own machine, and the first tick waits for the tell. Leeches gathered by hand while wading.
//  - Quag: run x0.5 (a replicated tag both machines read), standing still 3 s sinks you to the knee: no roll (a
//    backstep and a jump still work) until you're out. Mirestrider riders are exempt.
//  - Marsh gas (AWRMarshGas, one per field): bubbles, a blue flicker at night; fire within 3 m, a 0.9 s rising whoomp,
//    then a 4 m flash vent to vent every 0.15 s (40 + burn 40 through ApplyHit), 60 s to recharge, never into a claim.
//  - The Red Tide (world.json events.red_tide, run by the world systems' event step): drives AWRSwampState; the leech
//    boil (15/s + latch leeches) in its bleedwater; the Reliquary; the Hex-Warden's night walk flag.
//  - Weather reads (fog ambush, miasma curse cries and candles by day, rain-spread pools).
//  - The Sunken Coven (AWRSwampCoven): built in its own space (like the war-halls and the portal folds: the landscape is
//    a heightfield and the sea's water fills every hollow under it, so nothing can stand under it), straight above where
//    it lies by swampHazards.coven.lift, so x/y stay the Mire's (biome, sub-zone, weather, music). Entered by the dive,
//    the cellar ladder, the Bogsnout's chimney; left by the same ways or the one-way Weeping Gate into the Throat.
//  - Secrets M-S1..M-S5, the five shrines' places, Ruslana's day and night, her quests' events, wild bog rice and
//    bloodroot, invaders' bleedwater puddles, the storm cell over Storm invaders, the salt pouch and the stormglass jar.

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PhysicsVolume.h"
#include "Subsystems/WorldSubsystem.h"
#include "NativeGameplayTags.h"
#include "WRSwampTypes.h"
#include "WRSwampSystems.generated.h"

class AWRCharacter;
class AWRCharacterBase;
class AWRCreature;
class AWRPlayerController;
class AWRPlayerState;
class AWRWorldCache;
class UPointLightComponent;
class UProceduralMeshComponent;
class UBoxComponent;
class APhysicsVolume;
class FJsonObject;
struct FWRHit;
struct FWRLookBlock;
namespace WRLookKit { struct FKit; }

namespace WRSwampTags
{
	/** In quag mud: movement x swampHazards.quag.runMultiplier (both machines read it). */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Quag);
	/** Sunk to the knee in quag: no roll (a backstep and a jump still work). */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sunk);
}

namespace WRSwamp
{
	/** The water as it hurts a player here: WaterAt (the Red Tide) plus rain-spread pools (cells within
	 *  swampHazards.bleedwater.rainSpread of bleedwater count as bleedwater while it rains), the Deep Pool's top 2 m, an
	 *  invader's puddle, the Sunken Coven's Birthing Pools. */
	WILDREACH_API EWRSwampWater HazardWaterAt(const UObject* WorldContext, const FVector& Pos);
	/** The Red Tide is on (its day, not the warning). */
	WILDREACH_API bool IsTide(const UObject* WorldContext);
	/** The swamp's weather now (clear, overcast, rain, fog, miasma) at strength. */
	WILDREACH_API FName WeatherNow(const UObject* WorldContext);
	WILDREACH_API bool IsRaining(const UObject* WorldContext);
	/** Creatures track: ambush trigger ranges x this (fog 2; the fog's aggro x2 for Murkjaw/Bogstalker is already in
	 *  weather.json aggroMult, so use this for ambush/burst distances only). */
	WILDREACH_API float AmbushRangeMult(const UObject* WorldContext);
	/** Creatures track: Hexcrow curse-cry range x this (miasma 1.5). */
	WILDREACH_API float CurseCryMult(const UObject* WorldContext);
	/** World track: corpse candles burn by day (miasma). */
	WILDREACH_API bool CandlesByDay(const UObject* WorldContext);
	/** Creatures track: spawn weight / count multiplier for a swamp creature now (Red Tide: gorgeleech and latch_leech
	 *  x redTide.leechSpawn). 1 otherwise. */
	WILDREACH_API float SpawnMultiplier(const UObject* WorldContext, FName CreatureId);
	/** Creatures track: Gorgeleech sac drops x this (Red Tide 2). */
	WILDREACH_API float SacDropMult(const UObject* WorldContext);
	/** Bosses track: the Bloat gorges x this (Red Tide 2). */
	WILDREACH_API float BloatGorgeMult(const UObject* WorldContext);

	/** Fire touched this point (a fire shot's impact, a fire pot, a fire blow): marsh gas within reach lights. Server. */
	WILDREACH_API void FireAt(const UObject* WorldContext, const FVector& At, AActor* By);
	/** Server: shock the water within Radius of Center for Seconds: everyone standing or swimming in it takes the jar's
	 *  shock (swampHazards.stormJar) once; OnWaterShocked tells the bosses (Vharra's non-Lightning counter). */
	WILDREACH_API void ShockWater(UWorld* World, const FVector& Center, float Radius, float Seconds, AActor* Instigator);
	DECLARE_MULTICAST_DELEGATE_FiveParams(FOnWaterShocked, UWorld* /*World*/, const FVector& /*Center*/, float /*Radius*/, float /*Seconds*/, AActor* /*Instigator*/);
	WILDREACH_API FOnWaterShocked& OnWaterShocked();
	/** Server: salt landed on Target (a thrown salt pouch: within swampHazards.salt.radius of where it broke). The
	 *  Creatures track detaches latches / leeches, the Bosses track makes the Bloat vomit. */
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnSalted, AWRCharacterBase* /*Target*/, AActor* /*Instigator*/, const FVector& /*At*/);
	WILDREACH_API FOnSalted& OnSalted();

	/** A thrown thing of this file landed (WRProjectile: kinds saltpouch and stormjar). Server. */
	WILDREACH_API void ThrownLanded(AActor* Projectile, FName Kind, const FVector& At, AActor* HitActor, AWRCharacterBase* By);
	/** Builds a thrown thing's model ("code:saltpouch", "code:stormjar") on a procedural mesh; false if not ours. */
	WILDREACH_API bool BuildThrownModel(const FString& ModelPath, UProceduralMeshComponent* Mesh);
	/** An arrow's / bolt's status buildups from items.json ammo (bleed, shock, ...), added to the hit. */
	WILDREACH_API void AddAmmoBuildups(FName AmmoId, FWRHit& Hit);
	/** Extra bleed a blow carries now (Thinblood oil on the attacker's blade). */
	WILDREACH_API float ExtraBleed(const AActor* Attacker, const FWRHit& Hit);
	/** Server, ApplyHit before the buildups land: Thinblood oil's bleed onto a melee blow, worn curse buildup (the Coven
	 *  ring's +5%) onto any curse the blow carries. One call covers both. */
	WILDREACH_API void ScaleBuildups(const AActor* Attacker, FWRHit& Hit);
	/** The bloomery's look (pieces.json bloomery): a clay shaft furnace with its hide bellows, in the piece's space
	 *  (origin at its foot, +X its tapping arch). For FWRBuildLook::Blocks. */
	WILDREACH_API void BloomeryBlocks(const FTransform& PieceToWorld, TArray<FWRLookBlock>& Out);
	/** The stilt foundation's look (pieces.json stilt_foundation, cook 2.19): a cypress deck on posts sunk into the mud
	 *  under up to 2 m of water, cross-braced, in the piece's space (deck top at +92 like every foundation). */
	WILDREACH_API void StiltFoundationBlocks(const FTransform& PieceToWorld, TArray<FWRLookBlock>& Out);
	/** Building: where a piece's base sits over water. A stilt foundation over water stands its deck clear of the surface
	 *  (its posts go down to the bottom); every other piece keeps BaseZ. Both machines. */
	WILDREACH_API float StiltBaseZ(const UObject* WorldContext, FName PieceId, const FVector& At, float BaseZ);
	/** Building: why a stilt foundation can't stand here (empty: it can): its posts reach 2 m of water and a metre of mud. */
	WILDREACH_API FString StiltCheck(const UObject* WorldContext, FName PieceId, const FVector& At);
	/** Building: how far under a ground piece its own footing reaches (the grounded check's trace): the stilts' length. */
	WILDREACH_API float GroundReach(FName PieceId, float Default);
	/** How high the Sunken Coven's own space floats over the swamp it lies under (cm, swampHazards.coven.lift): the rooms
	 *  keep their x/y, so the biome, the sub-zone and the weather under them are still the Mire's. */
	WILDREACH_API float CovenLift();

	/** Server hooks for Ruslana's quests. */
	WILDREACH_API void OnKilled(AWRCharacterBase* Victim, AActor* Killer, AWRPlayerState* KillerPS);
	WILDREACH_API void OnBleedCured(AWRCharacterBase* Who);

	/** World systems: a shrine row with "swamp": true takes its place from swamp.json shrines. */
	WILDREACH_API bool ShrinePlace(FName Id, FVector2D& OutAt, float& OutYaw);
	/** World systems: a master row with swampDay / swampNight (site ids) stands at the day place now. */
	WILDREACH_API bool MasterPlace(const TSharedPtr<FJsonObject>& Row, FVector& OutAt, float& OutYaw);

	/** Interact: the swamp's things (caches, the cage, the trapdoor, ladders, the bell, eggs, wild plants, leeches). */
	WILDREACH_API bool Prompt(const AWRPlayerController* PC, FString& OutTarget, FString& OutVerb);
	WILDREACH_API bool ClientInteract(AWRPlayerController* PC);
	/** Server: "world swamp ..." commands. */
	WILDREACH_API FString Command(AWRPlayerController* PC, const TArray<FString>& Args);

	/** The Sunken Coven's space (audio, darkness, biome): inside its built rooms and lanes. */
	WILDREACH_API bool InCoven(const FVector& Pos);
	/** The Coven chapel's pool (the Old Gar): its surface and depth under At; false elsewhere. */
	WILDREACH_API bool CovenPool(const FVector& At, float& OutSurfaceZ, float& OutDepth);
}

/**
 * A marsh-gas field (swamp.json gas): its vents bubble (and flicker blue at night); fire within igniteRadius lights it.
 * The server decides when; every machine plays the tell and the chain from the replicated times.
 */
UCLASS(NotPlaceable)
class WILDREACH_API AWRMarshGas : public AActor
{
	GENERATED_BODY()

public:
	AWRMarshGas();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** The vents (world), in the order the flash runs from the last ignition (index 0 lit first). */
	UPROPERTY(ReplicatedUsing=OnRep_Vents) TArray<FVector_NetQuantize> Vents;
	/** Server time the whoomp's tell began (0: never), the flash starts tell seconds later. */
	UPROPERTY(ReplicatedUsing=OnRep_Lit) float LitAt = 0.f;
	/** Vents that sit in a claim: they never flash. One bit each. */
	UPROPERTY(Replicated) int32 Claimed = 0;

	/** Server: fire at At (by By): lights the field if a vent is within reach and it's charged. */
	bool TryIgnite(const FVector& At, AActor* By);
	bool IsCharged() const;
	float Reach() const;

private:
	UFUNCTION() void OnRep_Vents();
	UFUNCTION() void OnRep_Lit();
	void BuildLook();
	void StepServer(float Now);
	TWeakObjectPtr<AActor> Igniter;
	int32 NextVent = 0;
	float ClientShown = -1.f;
	int32 ClientNext = 0;
	bool bTellShown = false;
	UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> Flicker;
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> Bubbles;
	float FlickerClock = 0.f;
};

/**
 * A swamp thing you can use (Interact): a cache (secret, Coven room), the Hanging Cypress's key cage, the Walking House
 * trapdoor, the Coven's ladders / dive hole / chimney / gate, the bell, an egg case, the library's recipe, a wild plant,
 * the fourth bog body. Server state; the look is drawn on every machine from Kind.
 */
UCLASS(NotPlaceable)
class WILDREACH_API AWRSwampThing : public AActor
{
	GENERATED_BODY()

public:
	AWRSwampThing();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	UPROPERTY(Replicated) FName Kind;
	UPROPERTY(Replicated) FString Title;
	UPROPERTY(Replicated) FString Verb;
	UPROPERTY(Replicated) float Reach = 300.f;
	/** Used up (a cache taken, a plant picked, an egg cut loose); refills at RefillAtHours. */
	UPROPERTY(ReplicatedUsing=OnRep_State) bool bSpent = false;
	/** Shut (the Reliquary outside the tide, the chimney not dug, the trapdoor locked to this machine's eyes). */
	UPROPERTY(ReplicatedUsing=OnRep_State) bool bShut = false;
	/** Hidden until found (the fourth bog body). */
	UPROPERTY(ReplicatedUsing=OnRep_State) bool bConcealed = false;
	/** For wild plants: which crop. */
	UPROPERTY(Replicated) FName Crop;
	/** Where it sends you (ladders, the dive hole, the gate): world. */
	UPROPERTY(Replicated) FVector_NetQuantize Dest = FVector::ZeroVector;
	UPROPERTY(Replicated) float DestYaw = 0.f;

	/** Server. */
	TMap<FName, int32> Items;
	FString Lore;
	float RefillHours = 0.f;
	double RefillAtHours = -1.0;
	/** Per-player once (the key cage gives each survivor their own key). */
	TSet<FString> TakenBy;
	FString Use(AWRPlayerController* PC);
	void Refill();
	bool CanUse() const { return !bSpent && !bShut && !bConcealed; }
	/** Server: change its state (the tide opens the Reliquary, a snout uncovers the body, a dig opens the chimney): the
	 *  look and the door's block follow on every machine. */
	void SetState(bool bInSpent, bool bInShut, bool bInConcealed);

private:
	UFUNCTION() void OnRep_State();
	void BuildLook();
	UPROPERTY(Transient) TObjectPtr<USceneComponent> LookRoot;
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Plant;
	UPROPERTY(Transient) TObjectPtr<UBoxComponent> DoorBlock;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> Glow;
	bool bLookBuilt = false;
};

/** A bleedwater puddle an invader of the Mire left where it stood (2 game hours): it builds bleed like a pool. */
UCLASS(NotPlaceable)
class WILDREACH_API AWRSwampPuddle : public AActor
{
	GENERATED_BODY()

public:
	AWRSwampPuddle();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	UPROPERTY(Replicated) float Radius = 300.f;
	double GoneAtHours = 0.0;
};

/** The corpse candle of M-S1: a cold flame drifting the hidden line over Corpse-Candle Mere at night. */
UCLASS(NotPlaceable)
class WILDREACH_API AWRSwampCandle : public AActor
{
	GENERATED_BODY()

public:
	AWRSwampCandle();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	UPROPERTY(ReplicatedUsing=OnRep_Out) bool bOut = true;
	/** How far along the hidden line it has drifted (a point index, fractional); every machine eases it there. */
	UPROPERTY(Replicated) float Along = 0.f;
	/** Where it floats for an Along (world). */
	static FVector PointAt(float Along);

private:
	friend class UWRSwampSystems;
	UFUNCTION() void OnRep_Out();
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> Light;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Flame;
	float Shown = -1.f;
};

/** The Mire's two secret ways: "candle", the Coven's hidden boardwalk 30 cm under the Corpse-Candle Mere's duckweed
 *  (M-S1); "ledge", the cut steps behind the Weeping Wall's third fall up to the Gullet's mouth (M-S4). Both are drawn
 *  and made solid by the World track's sceneries (candle_path, fall_ledge: the steps are traced to the cliff), so this
 *  builds nothing and isn't spawned; LedgeTop is where the smith's cache stands (Swamp-finish NOTES, agreed). */
UCLASS(NotPlaceable)
class WILDREACH_API AWRSwampWays : public AActor
{
	GENERATED_BODY()

public:
	AWRSwampWays();
	FName Kind;
	void Build();
	/** The ledge's top landing beside the Gullet's mouth (world): the smith's cache stands on it. */
	static FVector LedgeTop();

private:
	void BuildCandleWalk();
	void BuildLedge();
	UPROPERTY(Transient) TObjectPtr<USceneComponent> LookRoot;
};

/** Swimmable water of the Sunken Coven (the Old Gar's chapel pool): a box volume every machine builds, as the sea's. */
UCLASS(NotPlaceable)
class WILDREACH_API AWRSwampWater : public APhysicsVolume
{
	GENERATED_BODY()

public:
	AWRSwampWater(const FObjectInitializer& ObjectInitializer);
	/** World boxes (centre, half size). */
	void Build(const TArray<FBox>& Boxes);
};

/**
 * The Sunken Coven (DC 2.5): the drowned village under the peat mat, built in its own space from swamp.json coven
 * (rooms keep their layout round the Deep Pool). Every machine builds the same stone, water and dressing; the server
 * places the things you use and the creatures.
 */
UCLASS(NotPlaceable)
class WILDREACH_API AWRSwampCoven : public AActor
{
	GENERATED_BODY()

public:
	AWRSwampCoven();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Server time the Ringing Room's chimes last rang (every machine plays them). */
	UPROPERTY(ReplicatedUsing=OnRep_Chimes) float ChimedAt = 0.f;
	/** Server time the bell was last rung. */
	UPROPERTY(ReplicatedUsing=OnRep_Bell) float BellAt = 0.f;

	/** Where a swamp.json point (world cm) sits in the Coven's space. */
	static FVector ToCoven(const FVector& SwampPoint);
	static FVector Origin();
	/** A room's box in Coven space (centre, half size) by id. */
	static bool RoomBox(FName Id, FVector& OutCentre, FVector& OutHalf);
	/** The chapel pool (the Old Gar's): centre, half size, surface Z. */
	static bool ChapelPool(FVector& OutCentre, FVector& OutHalf, float& OutSurface);
	/** Bleedwater in the Coven (the Birthing Pools, the drowned lane in the tide): feet at Pos. */
	static bool IsBleedPool(const FVector& Pos);

	/** Server. */
	void RingChimes(AWRCharacter* By);
	FString RingBell(AWRPlayerController* PC);
	double BellReadyAtHours = -1.0;
	TArray<TWeakObjectPtr<AWRCreature>> Brood;      // the Birthing Pools' adults (woken by the chimes)
	TArray<TWeakObjectPtr<AWRCreature>> Dwellers;   // every Gorgeleech in the Coven (the bell calls them)
	/** A room's walk floor (world Z in Coven space; the chapel's is its fishing ledge). */
	static float WalkFloor(FName Room);
	/** Where a lane meets a room (the gap's middle at floor height) and the lane's heading into the room. */
	static bool LaneGap(FName Room, FName Other, FVector& OutAt, float& OutYaw);
	/** A room or landing's centre at its walk floor (Coven space); false if not in the layout. */
	static bool RoomFloorAt(FName Room, FVector& OutAt);

private:
	UFUNCTION() void OnRep_Chimes();
	UFUNCTION() void OnRep_Bell();
	void Build();
	void BuildRoom(int32 Index, WRLookKit::FKit& K);
	void SpawnServerThings();
	void StepCreatures();
	bool bBuilt = false;
	bool bServerSpawned = false;
	float CreatureClock = 0.f;
	float ChimeShown = 0.f;
	float BellShown = 0.f;
	float ChimeCheck = 0.f;
	/** Every machine that draws: the Ringing Room's chime strings (swing when rung) and how hard they swing now. */
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> ChimeGroups;
	float ChimeSwing = 0.f;
	float ChimeTime = 0.f;
	/** Server: who lives where (respawned while the Coven is empty of survivors). */
	struct FSlot { FName Id; FVector At = FVector::ZeroVector; TWeakObjectPtr<AWRCreature> Body; double DeadAt = -1.0; bool bBrood = false; bool bDweller = false; };
	TArray<FSlot> Slots;
	UPROPERTY(Transient) TArray<TObjectPtr<APhysicsVolume>> Water;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> LookRoot;
};

/**
 * Runs the Swamp's systems on the server (hazards, the Red Tide, gas, secrets, the Coven, Ruslana, invaders' puddles,
 * the storm cell) and the player's own tells on clients.
 */
UCLASS()
class WILDREACH_API UWRSwampSystems : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UWRSwampSystems* Get(const UObject* WorldContext);
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UWRSwampSystems, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return bActive; }
	virtual bool IsTickableInEditor() const override { return false; }

	bool bActive = false;
	bool bServer = false;

	/** Server: per survivor. */
	struct FWader
	{
		double InBleedSince = -1.0;     // entered bleedwater (the tell runs first)
		float WadeSeconds = 0.f;        // spent wading in the tide's boil (latches every latchEvery s)
		double StillSince = -1.0;       // standing still in quag
		FVector LastPos = FVector::ZeroVector;
		bool bQuag = false;
		bool bSunk = false;
		bool bGathering = false;
		FVector GatherFrom = FVector::ZeroVector;
		double NextLeechAt = 0.0;
		bool bBledInWater = false;      // a bleed proc in bleedwater (Ruslana's 2nd quest), until cured
		bool bWasBleeding = false;
		double HexWardReadyAt = 0.0;
		double NextChimeAt = 0.0;
		double DigStartedAt = -1.0;     // a tamed Bogsnout digging the chimney
		float StandSeconds = 0.f;
		int32 CandleIndex = -1;         // M-S1: how far along the line they've walked
		double NextDiveAt = 0.0;        // the Deep Pool's dive (one transfer, then a breather)
	};
	TMap<TWeakObjectPtr<AWRCharacter>, FWader> Waders;
	/** Server: the Bloat's fights: who salted it. */
	TMap<TWeakObjectPtr<AWRCharacterBase>, TSet<FString>> SaltedBloat;
	/** Server: water shock fields. */
	struct FShock { FVector Center; float Radius = 600.f; double Until = 0.0; TWeakObjectPtr<AActor> By; TSet<TWeakObjectPtr<AWRCharacterBase>> Hit; };
	TArray<FShock> Shocks;
	TArray<TWeakObjectPtr<AWRMarshGas>> GasFields;
	TArray<TWeakObjectPtr<AWRSwampPuddle>> Puddles;
	TArray<TWeakObjectPtr<AWRSwampThing>> Things;
	TWeakObjectPtr<AWRSwampCoven> Coven;
	TWeakObjectPtr<AWRSwampThing> Reliquary;
	TWeakObjectPtr<AWRSwampThing> Chimney;
	/** The chimney's foot inside the Coven (shut and open with the surface one). */
	TWeakObjectPtr<AWRSwampThing> ChimneyIn;
	/** Bloodmoss nodes' own yield before the Red Tide tripled it (restored when it ebbs). */
	TMap<TWeakObjectPtr<AActor>, int32> MossBase;
	TWeakObjectPtr<AWRSwampThing> FourthBody;
	TWeakObjectPtr<AWRSwampCandle> Candle;
	TWeakObjectPtr<AWRSwampThing> CandleCache;
	TArray<TWeakObjectPtr<AWRSwampThing>> ReliquaryParts;   // the Reliquary's door and its cache (the tide opens both)
	/** Server: the chimney's dig so far (s) by a tamed Bogsnout, and when it falls in again (game hours). */
	float ChimneyDug = 0.f;
	double ChimneyFallsAtHours = -1.0;
	/** Server: M-S1, the night's walk: the candle out until (s), and the last time a follower was near it. */
	double CandleOutUntil = 0.0;
	double CandleLastFollowed = 0.0;
	/** Server: the storm cell over a Storm invasion (the strike that's coming, so it isn't doubled). */
	double NextCellCloud = 0.0;

	AWRSwampThing* SpawnThing(FName Kind, const FVector& At, float Yaw, const FString& Title, const FString& Verb, float Reach);
	/** The same, with Setup run before it finishes spawning (its state, items, where it sends you): the first look and the
	 *  first replication already carry them. */
	AWRSwampThing* SpawnThing(FName Kind, const FVector& At, float Yaw, const FString& Title, const FString& Verb, float Reach, TFunctionRef<void(AWRSwampThing&)> Setup);

private:
	void BuildLocal();
	void SpawnServer();
	void StepWaders(float Dt);
	void StepTide();
	void StepGas();
	void StepShocks();
	void StepPuddles(float Dt);
	void StepStormCell();
	void StepRuslana();
	void StepSecrets(float Dt);
	void StepRefills();
	void StepClientTells();
	float ServerClock = 0.f;
	float SlowClock = 0.f;
	float RefillClock = 0.f;
	float TellClock = 0.f;
	bool bSpawned = false;
	EWRRedTide LastTide = EWRRedTide::None;
	double NextCellStrike = 0.0;
	TMap<TWeakObjectPtr<AWRCreature>, float> InvaderStill;
	bool bWasTide = false;
	bool bRuslanaNight = false;
	/** Client: the local survivor's last water kind (for the tells). */
	EWRSwampWater LastLocalKind = EWRSwampWater::Dry;
	bool bLocalWasWet = false;
	bool bLocalSunk = false;
	double NextCellCloudAt = 0.0;
	double NextLocalBoil = 0.0;
	FDelegateHandle StrikeHandle;
	/** Every machine: the secret ways built here. */
	TArray<TWeakObjectPtr<AWRSwampWays>> Ways;
	void StrikeLanded(UWorld* World, const struct FWRStrikeCall& Call);
	void SpawnLatchers(AWRCharacter* Wader, int32 Count);
};
