#pragma once

// The Ancient forest's hazards and world state (cooks/biome-forest.md 2.8-2.10, cooks/build-biome-forest.md 2.7, 2.13):
// thornbrakes, widowmakers, leaf pits, Bramblewasp nests, the rivers and their spate, smoke, and the Bloom (its warning,
// its Elder Falls, its noon raid and its night hunt). Everything is placed from Data/forest.json (FWRForestData, track A's
// generator) and every number is tuning hazards.* (Data/tuning.json): this file decides how a thing behaves and looks,
// never where it goes. The shared calls (WRForest::Bloom, PassesThornbrake, AddSmoke...) are declared in WRForestTypes.h.
// The Hollow Root and the secrets are in WRForestHollow.h/.cpp.

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PhysicsVolume.h"
#include "NativeGameplayTags.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRForestTypes.h"
#include "WRForestHazards.generated.h"

class UAudioComponent;
class UBoxComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UProceduralMeshComponent;
class USphereComponent;
class UStaticMeshComponent;
class AWRCharacterBase;
class AWRCreature;
class AWRPlayerController;
class AWRResourceNode;
class AWRWorldCache;

namespace WRForestTags
{
	/** On a pawn pushing through a thornbrake without a pass (server sets it, every machine reads it): run x0.5, the
	 *  arms-up shield layer. A replicated loose tag, like Raven's Status.Slowed. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Hazard_Thornbrake);
}

namespace WRForestHazards
{
	/** The thornbrake's run multiplier for this pawn (1 when it isn't in one): AWRCharacterBase::GetSpeedMultiplier. */
	WILDREACH_API float MoveScale(const AActor* Who);
	/** It is pushing through brambles right now (the arms-up shield layer: AWRCharacter's upper-body loop). */
	WILDREACH_API bool Shielding(const AActor* Who);
	/** The Four Hearts' sap runs bright: the Bloom's warning and the Bloom itself (the boss track's heart-trees read it). */
	WILDREACH_API bool HeartsBright(const UObject* WorldContext);
	/** The Alder and its streams are in spate (the Rain block and the one after it). */
	WILDREACH_API bool Spate(const UObject* WorldContext);

	/** Server: something took a blow: a projectile (bFire: a fire shot) or a melee swing. Returns true if it was one of
	 *  the forest's things (a widowmaker limb, a wasp nest) and took the blow. Fire anywhere also burns thornbrake. */
	WILDREACH_API bool HitThing(AActor* Thing, AWRCharacterBase* By, float Damage, bool bFire, const FVector& At);

	/** The interact prompt for a forest thing in reach (a wasp nest to rob, the bell, the Thrumming Root, a door...). */
	WILDREACH_API bool Prompt(const AWRPlayerController* PC, FString& OutTarget, FString& OutVerb);
	/** E on one: sends "fhaz use x y z" (or acts locally first: the Thrumming Root's map flash). */
	WILDREACH_API bool ClientInteract(AWRPlayerController* PC);
	/** Server: "world fhaz use x y z" (Args[2] the verb), "world fhaz secret x y z" (WRForestHollow) and the dev
	 *  commands ("world fhaz bloom", "... spate", "... elderfall"). */
	WILDREACH_API FString Command(AWRPlayerController* PC, const TArray<FString>& Args);

	/** Crop growth x for an outdoor plot here (WRFarm GrowthRate): Rain +25% (cooks/biome-forest.md 2.9), the Bloom x2
	 *  (2.10). 1 outside the forest. bRainOnPlot: the rain reaches the plot (no roof). */
	WILDREACH_API float CropScale(const UObject* WorldContext, const FVector& At, bool bRainOnPlot);
	/** A gather's x from the forest's weather (WRWorld::GatherScale): resin +50% in the morning mist (the plenty after
	 *  rain, cooks/biome-forest.md 2.9). 1 otherwise. */
	WILDREACH_API float GatherScale(const UObject* WorldContext, FName ItemId, const FVector& At);
	/** The forest's morning mist is on over this point. */
	WILDREACH_API bool Misty(const UObject* WorldContext, const FVector& Pos);
	/** Server, the tree-harvest path (AWRDressing::HarvestInFront): felling refused here (within 60 m of a giant's or a
	 *  heart-tree's root crown): the axe bounces, "The roots hold it.". True: the blow is spent on the bounce. */
	WILDREACH_API bool RefuseFelling(AActor* Who, const FVector& Tree);
	/** A thrown forest jar's model ("code:blightjar", "code:smokepot") for the projectile (WRProjectile). */
	WILDREACH_API bool BuildThrownModel(const FString& ModelPath, UProceduralMeshComponent* Mesh);
	/** Server, WRProjectile: a thrown forest jar broke at At (ballistic, never homing): blightjar -> UWRForestWorld::
	 *  BlightLanded, smokepot -> UWRForestWorld::SmokePotLanded. True if Kind was one of them. */
	WILDREACH_API bool ThrownLanded(UWorld* World, FName Kind, const FVector& At, AActor* Thrower);
	/** Is this one of the forest's hittable things (a widowmaker limb, a wasp nest)? Melee sweeps ask first. */
	WILDREACH_API bool IsHazardThing(const AActor* Thing);
	/** Server: fire landed here (a fire bolt or fire pot's impact, a fire blow's touch): thornbrake within reach burns a
	 *  lane open, the barred door's living bark catches (WRForestHollow::FireAt). Cheap when nothing is near. */
	WILDREACH_API void FireLanded(UWorld* World, const FVector& At);
}

/**
 * For the creature track's Truffler (cooks/biome-forest.md 2.8: "a Truffler snorts at pits within 10 m"): the nearest
 * leaf pit that hasn't collapsed within Range of At (OutPit: its centre), and its rim shows to everyone for 5 s.
 * Server. False: none near.
 */
WILDREACH_API bool WRForestHazardsLeafPitNear(const FVector& At, float Range, FVector& OutPit);

/** A burnt (or the generator's natural) gap in a thornbrake: a lane across its depth, centred on At. */
USTRUCT()
struct FWRBrakeGap
{
	GENERATED_BODY()
	UPROPERTY() FVector_NetQuantize At;
	/** Half its width across the brake's run (cm). */
	UPROPERTY() float HalfWidth = 200.f;
	/** Server time the canes start growing back (they take hazards.thornbrake.regrowSeconds). */
	UPROPERTY() float OpenUntil = 0.f;
};

/** A live smoke sphere (a smoke pot, a torch held at a nest): wasps and bees inside go passive. */
USTRUCT()
struct FWRForestSmoke
{
	GENERATED_BODY()
	UPROPERTY() FVector_NetQuantize At;
	UPROPERTY() float Radius = 400.f;
	UPROPERTY() float Until = 0.f;
};

/**
 * The Grovekin pillar of the Twelve on the Crown's Ring (cooks/world-map.md 2.12: each land's pillar burns in its colour
 * while its event comes): an oak-grey granite shaft wound with a carved living root, banded in green glass that burns
 * through the Bloom's warning and the Bloom. Built locally on every machine where the Ring stands (Storm peaks live).
 */
UCLASS(NotPlaceable)
class WILDREACH_API AWRForestRingPillar : public AActor
{
	GENERATED_BODY()

public:
	AWRForestRingPillar();
	virtual void Tick(float DeltaSeconds) override;
	void Build(const FVector& Ground);

private:
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> Light;
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Bands;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> GlowMats;
	float Burn = 0.f;
	float FlameClock = 0.f;
};

/** The one replicated actor for the forest's shared state: the spate and the live smoke. */
UCLASS(NotPlaceable)
class WILDREACH_API AWRForestHazardState : public AActor
{
	GENERATED_BODY()

public:
	AWRForestHazardState();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	static AWRForestHazardState* Get(const UObject* WorldContext);

	UPROPERTY(Replicated) bool bSpate = false;
	UPROPERTY(Replicated) TArray<FWRForestSmoke> Smoke;
};

/**
 * A thornbrake (cooks/biome-forest.md 2.8): a ring of bramble round a giant's root crown or a wall across a way, 2-4 m
 * tall, 3-12 m deep. A player pushing through without a pass bleeds (6 buildup a second from 0.6 s after contact) and
 * runs at half speed with their arms up; any Nature points, briarproof leathers or Heartguard 4 walk through untouched
 * and the canes bend away from them. A torch held to it for 2 s or any fire burns a 4 m lane through it, open 5 game
 * hours, then the canes grow back over 30 s. Creatures ignore it. Server-spawned, its gaps replicated.
 */
UCLASS(NotPlaceable)
class WILDREACH_API AWRThornbrake : public AActor
{
	GENERATED_BODY()

public:
	AWRThornbrake();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** The shape (FWRForestData::FBrake): ring (Radius inner, Depth thick) or wall (Length along the actor's yaw, Depth
	 *  across). Height 2-4 m. Guards: what it hides (a cache's id), if anything. */
	UPROPERTY(Replicated) bool bRing = true;
	UPROPERTY(Replicated) float Radius = 0.f;
	UPROPERTY(Replicated) float Depth = 600.f;
	UPROPERTY(Replicated) float Length = 0.f;
	UPROPERTY(Replicated) float Height = 300.f;
	UPROPERTY(Replicated) FName Guards;
	/** The natural way in (plan 2.6: "a 2-3 m gap on one side most of the time") and every burnt lane. */
	UPROPERTY(ReplicatedUsing=OnRep_Gaps) TArray<FWRBrakeGap> Gaps;

	/** Server, set before it spawns: its natural gap came from the data (BeginPlay doesn't roll one). */
	bool bGapDecided = false;

	/** Is this point (a pawn's feet) in the brambles: inside the shape, not in an open gap (Pad widens it). */
	bool Contains(const FVector& Feet, float Pad) const;
	/** The point on the brake's middle line nearest At, and how far At is from the brambles (0 inside). */
	FVector NearestMid(const FVector& At, float& OutDistance) const;
	/** Server: burn a lane through it at At (Seconds open). */
	void Burn(const FVector& At, float HalfWidth, float Seconds);
	/** 0 closed .. 1 a fully open lane at At (regrowing gaps count part way). */
	float OpenAt(const FVector& At) const;

	UFUNCTION(NetMulticast, Unreliable) void MulticastScrape(AActor* Who);

private:
	UFUNCTION() void OnRep_Gaps();
	void BuildLook();
	void StepLook(float Dt);
	/** Where the canes stand (local frame), built once from the shape (same on every machine: seeded by position). */
	struct FCane { FTransform Base; int32 Part = 0; int32 Index = 0; };
	TArray<FCane> Canes;
	UPROPERTY(Transient) TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Parts;
	UPROPERTY(Transient) TArray<TObjectPtr<UProceduralMeshComponent>> Char;
	TMap<int32, float> Bent;      // cane -> how far it's bent away (0..1), eased
	TArray<float> LastScale;      // cane -> the burnt/regrow scale it was last drawn at
	bool bLookBuilt = false;
	float LookClock = 0.f;
	float GapClock = 0.f;
};

/**
 * A widowmaker (cooks/biome-forest.md 2.8): a dead grey limb wedged in a giant's crown over a path. Someone running or
 * fighting under it, a Mossmaul's roar within 6 m, or 30 damage shot into it: a loud crack, bark showering down and a
 * shadow growing on the ground for 1.0 s, then it falls (every machine moves it down the same gravity path from the
 * replicated start) and crushes everything within 2 m: 70 damage and a knockdown. It lies there a game day, then a
 * new one hangs in its place.
 */
UCLASS(NotPlaceable)
class WILDREACH_API AWRWidowmaker : public AActor
{
	GENERATED_BODY()

public:
	AWRWidowmaker();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** The ground under the limb (its fall ends there). */
	UPROPERTY(Replicated) float GroundZ = 0.f;
	UPROPERTY(Replicated) float FallRadius = 200.f;
	/** Server time the crack came (< 0: hanging, armed). The fall starts tell seconds later. */
	UPROPERTY(ReplicatedUsing=OnRep_Tell) float TellAt = -1.f;

	bool IsArmed() const { return TellAt < 0.f; }
	/** Server: it goes now (a trigger, a roar, a shot). */
	void Drop(AActor* By);
	/** Server: a blow on the limb (30 brings it down). */
	void TakeShot(float Damage, AActor* By);

private:
	UFUNCTION() void OnRep_Tell();
	void BuildLook();
	FTransform LimbAt(float SinceFall) const;
	void Land();
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Limb;
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> LimbMesh;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> Hitbox;
	float ShotDamage = 0.f;
	bool bLanded = false;
	bool bToldHere = false;
	bool bImpactShown = false;
	double ReArmAt = -1.0;
	float CheckClock = 0.f;
	TWeakObjectPtr<AActor> DroppedBy;
};

/**
 * A leaf pit (cooks/biome-forest.md 2.8): a root hole 4-9 m deep (carved into the ground by the generator) under a mat
 * of leaf litter. The litter sags in a ring; the edge step sounds hollow; stepping on it, the mat gives way and drops
 * into the hole. It re-forms after a game hour. A Truffler snorting at it shows its rim for 5 s.
 */
UCLASS(NotPlaceable)
class WILDREACH_API AWRLeafPit : public AActor
{
	GENERATED_BODY()

public:
	AWRLeafPit();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(Replicated) float Radius = 250.f;
	UPROPERTY(Replicated) float Depth = 600.f;
	/** Server time the mat gave way (< 0: whole). */
	UPROPERTY(ReplicatedUsing=OnRep_Collapsed) float CollapsedAt = -1.f;

	bool IsWhole() const { return CollapsedAt < 0.f; }
	/** Server: its rim shows for Seconds (the Truffler's snort). */
	void Reveal(float Seconds);
	UFUNCTION(NetMulticast, Unreliable) void MulticastDrum(FVector_NetQuantize At);

private:
	UFUNCTION() void OnRep_Collapsed();
	void BuildLook();
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Floor;
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Mat;
	TMap<TWeakObjectPtr<AActor>, double> DrumAt;
	double ReformAt = -1.0;
	double StepOnAt = -1.0;
	float CheckClock = 0.f;
};

/**
 * A Bramblewasp nest (cooks/biome-forest.md 2.8): a grey paper nest on a branch or an overhang. It hums within 15 m;
 * hit it (any blow, even an arrow) or stand within 4 m of it for 5 s and a scout marks you and 6-10 wasps pour out. Smoke
 * calms it for 60 s (a torch held at it 3 s, a smoke pot); at night it sleeps (a hit wakes it). While it's calm, E
 * steals a larva: the Bramblewasp's tame egg.
 */
UCLASS(NotPlaceable)
class WILDREACH_API AWRWaspNest : public AActor
{
	GENERATED_BODY()

public:
	AWRWaspNest();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Hung under an overhang (a ravine's) rather than from a branch. */
	UPROPERTY(Replicated) bool bOverhang = false;
	/** Where its bough is rooted (inside the trunk it grows from, or in the ground for a dead snag); zero: the overhang. */
	UPROPERTY(Replicated) FVector_NetQuantize BranchFrom;
	/** Server time until which smoke keeps it calm. */
	UPROPERTY(Replicated) float CalmUntil = 0.f;
	/** Server time it last swarmed (the hum rises for a while after). */
	UPROPERTY(Replicated) float SwarmedAt = -1000.f;

	bool IsCalm() const;
	bool IsAsleep() const;
	/** Server: it swarms Who (a blow or a loiterer). */
	void Swarm(AActor* Who);
	/** Server: E while it's calm. */
	FString StealLarva(AWRPlayerController* PC);

private:
	void BuildLook();
	UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> Hitbox;
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Body;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Hum;
	TMap<TWeakObjectPtr<AActor>, float> Near;   // pawn -> seconds within 4 m
	TMap<TWeakObjectPtr<AActor>, float> Torch;  // pawn -> seconds holding a torch at it
	TArray<TWeakObjectPtr<AWRCreature>> Wasps;
	TMap<FString, int32> RobbedDay;              // player -> game day they took a larva
	float CheckClock = 0.f;
};

/** A river's swimmable water: boxes along its surface (one per segment), from the surface down to below its bed. Every
 *  machine builds the same one from the data (not replicated), like the sea's (AWRWaterVolume). Pawns only. */
UCLASS(NotPlaceable)
class WILDREACH_API AWRForestWaterVolume : public APhysicsVolume
{
	GENERATED_BODY()

public:
	AWRForestWaterVolume(const FObjectInitializer& ObjectInitializer);
	struct FBox2 { FVector Centre; FVector Half; float Yaw = 0.f; };
	int32 Build(const TArray<FBox2>& Boxes);
};

/**
 * A river, stream or race of the forest (FWRForestData::Rivers): its surface drawn as a ribbon along the polyline at
 * its widths (the WaterMaterials river, flowing downstream), its water swimmable, and its spate: after Rain the Alder
 * runs brown and loud with logs riding it, and at the fords and the Great Falls' lip the current shoves anyone in it
 * downstream at 900 cm/s. Built locally on every machine (the spate is AWRForestHazardState's).
 */
UCLASS(NotPlaceable)
class WILDREACH_API AWRForestWater : public AActor
{
	GENERATED_BODY()

public:
	AWRForestWater();
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	FName RiverId, Kind;
	TArray<FVector> Points;
	TArray<float> Widths;
	/** Points (cm) where the current shoves in spate: fords where roads cross, the falls' lips. */
	struct FPush { FVector At; float Radius = 800.f; FVector Dir; bool bLip = false; };
	TArray<FPush> Pushes;

	void Build();
	/** The surface over a point if it's on this river (within its width), else false. OutDown: downstream. */
	bool SurfaceAt(const FVector& At, float& OutZ, FVector& OutDown) const;

private:
	void StepLogs(float Dt, bool bSpate);
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Surface;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> Mat;
	UPROPERTY(Transient) TObjectPtr<AWRForestWaterVolume> Volume;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Roar;
	UPROPERTY(Transient) TArray<TObjectPtr<UProceduralMeshComponent>> Logs;
	TArray<float> LogAlong;
	TArray<float> Along;      // cumulative cm along the polyline at each point
	float SpateLook = 0.f;    // 0 clear .. 1 full spate, eased
};

/**
 * An Elder Fall in the Bloom (cooks/biome-forest.md 2.10): a dead giant in F4/F5 cracks (heard from 1 km), birds lift,
 * it leans for 5 s, then falls along a 90 m line, crushing everything under it, and lies as a fresh fallen giant with
 * 12 heartwood and 6 resin nodes for the day. Every machine moves it from the replicated start.
 */
UCLASS(NotPlaceable)
class WILDREACH_API AWRElderFall : public AActor
{
	GENERATED_BODY()

public:
	AWRElderFall();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	/** The fall's bearing (degrees) and its length (cm). */
	UPROPERTY(Replicated) float FallYaw = 0.f;
	UPROPERTY(Replicated) float FallLength = 9000.f;
	/** Server time the crack starts (< 0: standing quietly, waiting its turn). */
	UPROPERTY(ReplicatedUsing=OnRep_Start) float StartAt = -1.f;

	/** Server: crack now. */
	void Begin();
	bool HasBegun() const { return StartAt >= 0.f; }
	bool HasLanded() const { return bLanded; }

private:
	UFUNCTION() void OnRep_Start();
	void BuildLook();
	float Tilt(float Since) const;
	void Land();
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Pivot;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Trunk;
	UPROPERTY(Transient) TObjectPtr<UBoxComponent> LogBlock;
	TArray<TWeakObjectPtr<AWRResourceNode>> Nodes;
	bool bLanded = false;
	bool bToldHere = false;
	bool bFellHere = false;
	/** Where it comes to rest (degrees from upright: its crown meets the ground there; the server works it out from the
	 *  ground along its line when it cracks) and its look's variant. */
	UPROPERTY(Replicated) float EndTilt = 88.f;
	int32 Variant = 0;
	float TrunkRadius = 250.f;
	bool bShapeKnown = false;
	/** The fall, worked out once from the lean (a rigid trunk pivoting on its broken foot): tilt at 1/120 s steps. */
	TArray<float> FallTable;
	bool bCrushed = false;
	/** Server: gone (with its nodes) a game day after it fell. */
	double GoneAtHours = -1.0;
	void ResolveShape();
	void LieDown();
	void SpawnNodes();
};

/**
 * Runs the forest's hazards: spawns them from the data, steps the thornbrakes, nests, widowmakers, pits, water and
 * smoke, and the Bloom. Server ticks the rules; every machine builds the rivers (local) and ticks their look.
 */
UCLASS()
class WILDREACH_API UWRForestHazards : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UWRForestHazards* Get(const UObject* WorldContext);
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UWRForestHazards, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return bActive; }

	bool bActive = false;
	/** Server: every brake, nest, limb, pit (spawned here). */
	TArray<TWeakObjectPtr<AWRThornbrake>> Brakes;
	TArray<TWeakObjectPtr<AWRWaspNest>> Nests;
	TArray<TWeakObjectPtr<AWRWidowmaker>> Limbs;
	TArray<TWeakObjectPtr<AWRLeafPit>> Pits;
	/** Every machine: the rivers. */
	TArray<TWeakObjectPtr<AWRForestWater>> Waters;

	/** Server: the brakes near a point (the grid). */
	void BrakesNear(const FVector& At, float Range, TArray<AWRThornbrake*>& Out) const;
	/** Is this pawn in a river's water (wading with its feet under the surface, or swimming)? */
	bool InRiver(const AActor* Who) const;
	/** Server: force the Bloom's steps for a test (dev command). */
	void DevElderFall();
	/** Server: the Alder spates for a block from now (dev command). */
	void DevSpate();
	/** Server: the cache behind a thornbrake wall (its Guards) and the brake. Opening it is Rowan's "Through the
	 *  briars" (quest event forest_briar_cache). */
	void AddGuardedCache(AWRWorldCache* Cache, FName Guards);
	/** Server: is this pawn (its feet) inside some brake's brambles right now (not in a gap)? */
	AWRThornbrake* BrakeAt(const FVector& Feet) const;
	/** Server: spawn a thornbrake (rings and walls from the data, the guards the secrets need). GapWidth (cm): a ring's
	 *  natural way in, centred on B.Yaw (the generator points it at the nearest way); 0 a closed ring; < 0 the old
	 *  seeded roll (hazards.thornbrake.ringGapShare). A wall's gap (> 0) sits at its middle. */
	AWRThornbrake* SpawnBrake(const FWRForestData::FBrake& B, float GapWidth = -1.f);

	// The Hollow Root and the secrets' server state (WRForestHollow.cpp keeps it here, one per world).
	/** The Lost Path's true chain (start, four bends, the gap) and each walker's progress along it. */
	TArray<FVector> LostChain;
	struct FLostWalker { int32 Stage = 0; double LastTold = -100.0; bool bThrough = false; };
	TMap<FString, FLostWalker> LostWalkers;
	TWeakObjectPtr<AWRThornbrake> LostBrake;
	/** A creature that lives in one place (the Hollow's Rootcoils, the sleeping Mossmaul): back a while after it dies. */
	struct FSleeper { FName Creature; FVector At = FVector::ZeroVector; float Yaw = 0.f; bool bAsleep = false; TWeakObjectPtr<AWRCreature> Body; double BackAtHours = -1.0; };
	TArray<FSleeper> Sleepers;
	TArray<TWeakObjectPtr<class AWRForestSecret>> Secrets;

private:
	void SpawnServerActors();
	void BuildLocal();
	void StepPawns(float Dt);
	void StepSmoke(float Dt);
	void StepSpate(float Dt);
	void StepBloom(float Dt);
	void StepCaches(float Dt);
	void StepWarningLook(float Dt);
	bool PickElderSpot(FVector& OutBase, float& OutYaw) const;
	bool FarFromSafe(const FVector& At, float Clear) const;
	void SpawnElderFalls();

	TMap<FIntPoint, TArray<int32>> BrakeGrid;
	struct FPawnState { float Contact = -1.f; float Outside = 0.f; float Torch = 0.f; float CrossedCm = 0.f; double LastIn = -1.0; FVector Last = FVector::ZeroVector; float ScrapeClock = 0.f; bool bTagged = false; };
	TMap<TWeakObjectPtr<AWRCharacterBase>, FPawnState> Pawns;
	struct FGuarded { TWeakObjectPtr<AWRWorldCache> Cache; FName Guards; bool bWasTaken = false; };
	TArray<FGuarded> Guarded;
	UPROPERTY(Transient) TObjectPtr<AWRForestHazardState> State;
	float PawnClock = 0.f;
	float SmokeClock = 0.f;
	float SpateClock = 0.f;
	float BloomClock = 0.f;
	float WarnLookClock = 0.f;
	float CacheClock = 0.f;
	/** The spate: raining now, or rain ended less than a block ago (server). */
	double RainEndedAt = -1e9;
	double RainStartedAt = -1e9;
	bool bWasRaining = false;
	/** The Bloom's Wild Hunt night hours and its noon raid, by game day (so each happens once a day). */
	int32 RaidDay = -1;
	// The Bloom's run (server).
	bool bBloomOn = false;
	bool bBloomWarned = false;
	bool bRaidDone = false;
	bool bHuntOn = false;
	float BloomStartAt = 0.f, BloomEndAt = 0.f;
	TArray<TWeakObjectPtr<AWRElderFall>> Elders;
	TArray<float> ElderTimes;
	/** Wild nodes' respawn halved for the Bloom (their own seconds, put back after). */
	TMap<TWeakObjectPtr<AWRResourceNode>, float> BloomRespawn;
	/** Client: the rain's look (open-ground rain at once, heavy drips under the canopy 20 s late). */
	void StepRainLook(float Dt);
	float RainLookClock = 0.f;
	float RainSeenAt = -1.f;
	float RainGoneAt = -1.f;
	bool bWasRainingLook = false;
	float PillarClock = 0.f;
	bool bWarnLookOn = false;
	UPROPERTY(Transient) TObjectPtr<AWRForestRingPillar> Pillar;
};
