#pragma once

// The Ancient forest's bosses and their arenas (cooks/biome-forest.md 2.10, 2.13, 2.14; cooks/build-biome-forest.md
// 2.10, 2.15, 2.16; cooks/combat.md 2.17): Old Gnarl in the Clear-cut (strip the bark, burn the pale wood), the
// Bramblequeen in the Paper Hollow (the brood combs, smoke, the Hivebacks), the Grey Matron in the Bower (cut her
// walkways), the Hornwarden on his loop with his herd (never hurt the herd, snare him) and the Wild Hunt, and Sylvaroth
// the World-Root in the Clearing of Four Hearts (the root lines that heal it, the Uprooting, the walking root, the
// heart-trees that wake).
//
// Their moves are rows in Data/attacks.json like every boss's; what's new here is FWRForestBoss (the hooks AWRBoss calls
// for these five, beside the Storm peaks' FWRStormBoss), UWRForestBossBody (each one's code-built look: Gnarl's bark plates,
// the queen's wings, the Matron's sabres, the Hornwarden's crown, Sylvaroth's root lattice), the arena pieces with state
// (root lines and knots, walkways and their rope anchors, brood combs, the waking heart-trees, the larder's hide curtain,
// the root hand), the thrown things (AWRForestLob: the stump boulder, seed pods, falling branches), the four arena
// landmarks' static scenery (AWRForestArenaScenery: L1 the Four Hearts, L5 the Clear-cut, L6 the Paper Hollow, L7 the
// Bower, built on every machine from Data/forest.json) and UWRForestArenas, which builds them, places the bosses, the
// run-back stones and the Nature altar, and keeps the Wild Hunt's flag. Numbers are tuning bosses.<id>.*.

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRCreature.h"
#include "WRCombatTypes.h"
#include "WRForestBosses.generated.h"

class AWRBoss;
class AWRCharacterBase;
class UBoxComponent;
class UCapsuleComponent;
class USphereComponent;
class UPointLightComponent;
class UInstancedStaticMeshComponent;
class UProceduralMeshComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
struct FWRBossAttack;
struct FWRForestBossState;

/** The forest bosses' world flags every machine reads (the Wild Hunt for the music layer). One per world, always relevant. */
UCLASS(NotPlaceable)
class WILDREACH_API AWRForestArenaState : public AActor
{
	GENERATED_BODY()

public:
	AWRForestArenaState();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	static AWRForestArenaState* Get(const UObject* WorldContext);
	/** The Bloom's night: the Hornwarden leads the Wild Hunt. */
	UPROPERTY(Replicated) bool bWildHunt = false;
};

/**
 * A piece of a forest arena with state (server-owned, replicated, its look rebuilt from its state on every machine that
 * draws): a brood comb in the Paper Hollow (300 HP), a waking heart-tree's limbs and amber heart (phase 3, 1,500 HP), the
 * root hand that rises under a grabbed survivor (its wrist 600 HP), the larder's hide curtain in the Bower's tallest oak.
 */
UCLASS(NotPlaceable)
class WILDREACH_API AWRForestProp : public AActor
{
	GENERATED_BODY()

public:
	AWRForestProp();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

	/** comb, heart_tree, root_hand, larder. */
	UPROPERTY(ReplicatedUsing=OnRep_Look) FName Kind;
	/** comb: 0 whole, 1 broken. heart_tree: 0 asleep, 1 awake, 2 its heart killed (dark until its line regrows).
	 *  root_hand: 0 rising, 1 holding, 2 torn (wrist broken), 3 sinking. larder: 0 curtain up, 1 fallen. */
	UPROPERTY(ReplicatedUsing=OnRep_Look) uint8 State = 0;
	/** Size: a comb's height, a heart-tree's trunk radius, the root hand's ring radius (cm). */
	UPROPERTY(ReplicatedUsing=OnRep_Look) float Size = 100.f;
	/** Which one: the comb's floor (0-3), the heart-tree's quarter (0 N, 1 E, 2 S, 3 W). */
	UPROPERTY(Replicated) int32 Index = 0;
	/** Server time of the last limb sweep (a heart-tree): every machine swings its lower limbs from it. */
	UPROPERTY(Replicated) float SweepAt = -100.f;
	/** A brood comb's stalk: how far above it the paper it hangs from is (cm). */
	UPROPERTY(ReplicatedUsing=OnRep_Look) float Hang = 0.f;

	/** Server: its own hit points (comb 300, amber heart 1,500, wrist 600). */
	float Health = 0.f;
	float MaxHealth = 0.f;
	/** Server: a blow (a weapon swing, an arrow, a bolt) reached it. Returns true when it took it. */
	bool TakeHit(AWRCharacterBase* By, float Damage, bool bFire, bool bAxe);
	/** Server. */
	void SetState(uint8 NewState);
	/** Where its hittable heart is (the comb's middle, the amber heart 3 m up the trunk, the wrist). */
	FVector HitPoint() const;
	/** Who the root hand holds (server). */
	TWeakObjectPtr<AWRCharacterBase> Held;
	TWeakObjectPtr<AWRBoss> OwnerBoss;
	/** Server: the root hand's slam when its hold runs out (a torn wrist drops them instead). */
	float SlamDamage = 40.f;

private:
	UFUNCTION() void OnRep_Look();
	void Build();
	UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> HitShape;
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> LookComps;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> LimbRoot;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> HandRoot;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> Glow;
	FName BuiltKind;
	uint8 BuiltState = 255;
	float BornAt = 0.f;
	/** This machine's time the state last changed (the root hand sinks from it). */
	float StateAt = 0.f;
};

/**
 * One of Sylvaroth's four root lines (DC 2.14): a raised root as thick as a person from a heart-tree's foot to the
 * clearing's middle, amber-green light pulsing along it toward the World-Root, and its Root Knot where it crosses the 25 m
 * ring (900 HP; axes x1.3, fire x1.5). Severed it goes dark and regrows after 45 s with a 3 s crawl of light back up the
 * root; a Bootlace blight jar on the knot kills it 90 s; the Uprooting darkens all four for 10 s.
 */
UCLASS(NotPlaceable)
class WILDREACH_API AWRForestRootLine : public AActor
{
	GENERATED_BODY()

public:
	AWRForestRootLine();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** The heart-tree's foot (actor location) and the clearing's middle (world cm). */
	UPROPERTY(ReplicatedUsing=OnRep_Look) FVector_NetQuantize To = FVector::ZeroVector;
	/** Where the knot bulges (world cm). */
	UPROPERTY(ReplicatedUsing=OnRep_Look) FVector_NetQuantize KnotAt = FVector::ZeroVector;
	/** 0 N, 1 E, 2 S, 3 W. */
	UPROPERTY(Replicated) int32 Index = 0;
	/** Server time the line is dark until (its light crawls back over the last crawlSeconds before it); 0 = live. */
	UPROPERTY(Replicated) float DarkUntil = 0.f;
	/** Why it's dark: 0 none, 1 severed (regrows with a crawl), 2 blighted, 3 the Uprooting's blackout. */
	UPROPERTY(Replicated) uint8 Why = 0;

	/** It heals Sylvaroth now (server time). */
	bool IsLive(float Now) const { return Now >= DarkUntil; }
	/** Server. */
	void Darken(float Seconds, uint8 Reason);
	/** Server: a blow on the knot. */
	bool TakeHit(AWRCharacterBase* By, float Damage, bool bFire, bool bAxe);
	float KnotHealth = 900.f;
	/** Phase 3: only the amber heart cuts it (the knot shrugs blows off). */
	bool bHeartOnly = false;
	float ServerNow() const;

private:
	UFUNCTION() void OnRep_Look();
	void Build();
	UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> Knot;
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> RootMesh;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> Strip;
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> LookComps;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> KnotLight;
	bool bBuilt = false;
	float LastGlow = -1.f;
};

/**
 * A sagging Grovekin walkway between two of the Bower's oaks (DC 2.13.3), 15-25 m up: plank deck on a catenary, rope rails,
 * a rope anchor at each tree (100 HP each: cut either and it falls). The Grey Matron's roads; arrows in it while she's on it
 * make her leap to another. It falls for real (a fall for anyone on it); it regrows 4 game hours after she dies.
 */
UCLASS(NotPlaceable)
class WILDREACH_API AWRForestWalkway : public AActor
{
	GENERATED_BODY()

public:
	AWRForestWalkway();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void NotifyHit(UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit) override;

	/** The two ends at the trunks (deck height, world cm) and the oaks they're on (0-5). */
	UPROPERTY(ReplicatedUsing=OnRep_Look) FVector_NetQuantize EndA = FVector::ZeroVector;
	UPROPERTY(ReplicatedUsing=OnRep_Look) FVector_NetQuantize EndB = FVector::ZeroVector;
	UPROPERTY(Replicated) int32 TreeA = 0;
	UPROPERTY(Replicated) int32 TreeB = 1;
	/** Down: both anchors' ropes hang, the deck lies broken on the moss below. */
	UPROPERTY(ReplicatedUsing=OnRep_Look) bool bDown = false;
	/** Server time it fell (every machine swings it down from there). */
	UPROPERTY(Replicated) float FellAt = -100.f;

	float AnchorHealth[2] = { 100.f, 100.f };
	/** Server: a blow on an anchor (0 at EndA, 1 at EndB). */
	bool TakeHit(int32 Anchor, AWRCharacterBase* By, float Damage);
	/** Server: it drops (bByPlayer: a survivor cut it). */
	void Fall(bool bByPlayer, AActor* By);
	/** Server: it grows back. */
	void Regrow();
	/** The deck point at t 0..1 (the sag), world cm. */
	FVector DeckAt(float T) const;
	/** Which anchor a component is (0/1), or -1. */
	int32 AnchorOf(const UPrimitiveComponent* Comp) const;
	/** Server: someone cut it this fight. */
	bool bCutByPlayer = false;
	double ArrowAt = -100.0;

private:
	UFUNCTION() void OnRep_Look();
	void Build();
	void SetDeckCollision(bool bOn);
	UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> AnchorA;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> AnchorB;
	UPROPERTY(Transient) TArray<TObjectPtr<UBoxComponent>> Deck;
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> LookComps;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> DeckLook;
	bool bBuiltDown = false;
	bool bBuilt = false;
};

/**
 * A thrown or falling thing on a gravity arc (never homing): Old Gnarl's stump with its root ball, Sylvaroth's seed pods,
 * the canopy shake's dead branches. Every machine draws it from the replicated launch (from, to, start, flight) so nothing
 * snaps; the server sweeps the arc against the world (a stump between you and it stops it) and bursts it where it lands.
 * Its shadow grows on the ground under the landing spot from the throw (the world showing where it'll come down).
 */
UCLASS(NotPlaceable)
class WILDREACH_API AWRForestLob : public AActor
{
	GENERATED_BODY()

public:
	AWRForestLob();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** stump, pod, branch. */
	UPROPERTY(ReplicatedUsing=OnRep_Launch) FName Kind;
	UPROPERTY(ReplicatedUsing=OnRep_Launch) FVector_NetQuantize From = FVector::ZeroVector;
	UPROPERTY(ReplicatedUsing=OnRep_Launch) FVector_NetQuantize To = FVector::ZeroVector;
	/** Server time it leaves the hand (or breaks off the canopy) and its seconds in the air. */
	UPROPERTY(ReplicatedUsing=OnRep_Launch) float LaunchAt = 0.f;
	UPROPERTY(ReplicatedUsing=OnRep_Launch) float Flight = 1.2f;
	/** Apex over the straight line (cm): 0 = a straight fall (branches). */
	UPROPERTY(ReplicatedUsing=OnRep_Launch) float Arc = 400.f;

	/** Server: what it does where it lands. */
	FWRHit Hit;
	float Radius = 300.f;
	float EntangleSeconds = 0.f;
	TWeakObjectPtr<AWRBoss> Thrower;
	/** Server: set up and go. */
	void Launch(FName InKind, const FVector& InFrom, const FVector& InTo, float InFlight, float InArc, float Delay);
	FVector PointAt(float T) const;

private:
	UFUNCTION() void OnRep_Launch();
	void BuildLook();
	void Land(const FVector& At);
	float ServerNow() const;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Look;
	TWeakObjectPtr<class AWRGroundMarker> Shadow;
	bool bLanded = false;
	bool bBuilt = false;
	FVector LastPos = FVector::ZeroVector;
};

/**
 * The four arena landmarks' static scenery (built on every machine, no network): L1 the Four Hearts (four 140 m heart-trees
 * with amber sap veins, the four Grovekin standing stones, the Nature altar's stone ring against the west cliff, the
 * foxfire haze at night), L5 the Clear-cut (340 stumps 0.5-1 m with collision that stops a thrown stump, 40 with axes
 * still in them), L6 the Paper Hollow (a dead oak 50 m wrapped in a ribbed paper hive 20 m high: three walkable gallery
 * floors and chewed-paper ramps, amber grub glow, a water butt on each floor), L7 the Bower (six hollow oaks 70 m on a
 * 30 m ring, their platforms, deer and elk hides hung like flags, green eyeshine at night, the larder hollow).
 */
UCLASS(NotPlaceable)
class WILDREACH_API AWRForestArenaScenery : public AActor
{
	GENERATED_BODY()

public:
	AWRForestArenaScenery();
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	/** four_hearts, clear_cut, paper_hollow, bower. */
	FName Kind;
	void Build();
	/** The Paper Hollow's water butts (world cm, one a floor): standing at one washes a wasp mark off. */
	TArray<FVector> Butts;
	/** The hive's floor heights over its base (cm) and its gallery ring (inner, outer radius). */
	TArray<float> FloorZ;
	float GalleryIn = 650.f, GalleryOut = 1050.f;

private:
	void BuildFourHearts();
	void BuildClearCut();
	void BuildPaperHollow();
	void BuildBower();
	USceneComponent* AddBlocks(const TArray<struct FWRLookBlock>& Blocks, bool bNight);
	UStaticMeshComponent* AddTree(FName Species, int32 Variant, const FVector& Base, float Height, float Yaw);
	void AddCapsule(const FVector& Base, float Radius, float Height);
	void AddBox(const FVector& Centre, const FVector& Half, const FRotator& Rot);
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> LookComps;
	UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> NightLights;
	TArray<float> NightPower;
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> NightOnly;
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> Loops;
	/** The Four Hearts' four trunks (their sap strips glow by the hour and the Bloom). */
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> HeartTrees;
	float LightClock = 0.f;
	int32 NightShown = -1;
	int32 HeartGlowShown = -1;
};

/** Each forest boss's look and the state its look shows, on every machine (server-owned, replicated). */
UCLASS(ClassGroup=(Creature))
class WILDREACH_API UWRForestBossBody : public UActorComponent
{
	GENERATED_BODY()

public:
	UWRForestBossBody();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	static UWRForestBossBody* Get(const AActor* Actor);
	/** Server: on a forest boss. */
	static void Attach(AWRBoss* Boss);

	/** Old Gnarl's bark plates (bit 0 front, 1 back, 2 left arm, 3 right arm): stripped to pale wood, and burning. */
	UPROPERTY(Replicated) uint8 Stripped = 0;
	UPROPERTY(Replicated) uint8 Burning = 0;
	/** The pose its own parts take (0 none; Gnarl 1 regrow; queen 1 sting curl, 2 fanning, 3 bowing over a comb, 4 wings
	 *  stopped; Matron 1 crouched to leap; Hornwarden 1 bellow; Sylvaroth 1 canopy shake, 2 kneeling to re-root). */
	UPROPERTY(Replicated) uint8 Pose = 0;
	/** Sylvaroth's lower body in the ground (phase 1, the re-root); the Bramblequeen's stinger stuck in the paper. */
	UPROPERTY(Replicated) bool bRooted = false;
	UPROPERTY(Replicated) bool bStuck = false;
	/** The Hornwarden enraged (his mane up) / leading the Wild Hunt; the Matron down for good. */
	UPROPERTY(Replicated) bool bEnraged = false;
	UPROPERTY(Replicated) bool bGrounded = false;
	/** Sylvaroth's vine sectors (phase 2): the clearing split in 6 round its middle. 0 quiet, 1 creeping, 2 live, 3 burnt
	 *  clean; until (server time). */
	UPROPERTY(Replicated) TArray<uint8> Sectors;
	UPROPERTY(Replicated) TArray<float> SectorUntil;
	UPROPERTY(Replicated) FVector_NetQuantize SectorCentre = FVector::ZeroVector;
	UPROPERTY(Replicated) float SectorRadius = 6000.f;
	/** How far Sylvaroth's body sits in the ground (cm; 0 standing). */
	UPROPERTY(Replicated) float Sink = 0.f;

private:
	void BuildLook();
	void ClearLook();
	void TickLook(float DeltaTime);
	USceneComponent* AddGroup(FName Bone, const FVector& WorldAt, const FRotator& WorldRot, const TArray<struct FWRLookBlock>& Blocks, int32 Kind);
	UProceduralMeshComponent* AddMesh(FName Bone, const FVector& WorldAt, const FRotator& WorldRot, const struct FWRToolMesh& Mesh, int32 Kind);
	/** A tree-kit mesh (a crown of oak, saplings and ferns on Sylvaroth's back) riding a bone, Height tall. */
	UStaticMeshComponent* AddKitMesh(FName Bone, const FVector& WorldAt, const FRotator& WorldRot, FName Species, int32 Variant, float Height, int32 Kind);
	/** The look's tint over the rig (creatures.json look.tint on M_CreatureTint, as the island's bodies). */
	void ApplyTint();
	void SetKindVisible(int32 Kind, bool bOn);
	void TickSectors();
	struct FGroup { TWeakObjectPtr<USceneComponent> Frame; int32 Kind = 0; FRotator BaseRot = FRotator::ZeroRotator; FVector BaseLoc = FVector::ZeroVector; bool bShown = true; };
	TArray<FGroup> Groups;
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> LookComps;
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> SectorFx;
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> PlateFx;
	UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> Lights;
	/** Sylvaroth's vine sectors drawn: one code-built mesh of creeping vines per sector (absolute: they stay on the floor). */
	UPROPERTY(Transient) TArray<TObjectPtr<UProceduralMeshComponent>> SectorMesh;
	TArray<float> SectorChanged;
	TArray<uint8> SectorShown;
	uint8 ShownStripped = 255, ShownBurning = 255;
	bool bBuilt = false;
	float Age = 0.f;
	float Clock = 0.f;
	float SinkShown = 0.f;
	bool bAntlerShown[2] = { true, true };
};

/** The hooks AWRBoss calls for the forest bosses (friend of AWRBoss), beside FWRStormBoss's. */
struct WILDREACH_API FWRForestBoss
{
	static bool IsForestBoss(const AWRBoss* Boss);
	static void Begin(AWRBoss* Boss);
	static void Reset(AWRBoss* Boss);
	static void EndPlay(AWRBoss* Boss);
	/** Not fighting: the Hornwarden's loop, rests and the Wild Hunt, the Matron in her canopy or out to the bell. True: skip
	 *  its own roam (bSkipWake: and the wake check). */
	static bool TickIdle(AWRBoss* Boss, float DeltaSeconds, bool& bSkipWake);
	/** Fighting, before its attack machine: true when this ran the tick (the Matron moving through her canopy, the stinger
	 *  stuck, a grab held, the stagger from two burning spots). */
	static bool Tick(AWRBoss* Boss, float DeltaSeconds);
	static AWRCharacterBase* PickTarget(AWRBoss* Boss, AWRCharacterBase* Nearest);
	static bool Allowed(const AWRBoss* Boss, const FWRBossAttack& Attack, const AWRCharacterBase* Target);
	static void AttackStarted(AWRBoss* Boss, FWRBossAttack& Attack, AWRCharacterBase* Target);
	static bool TickWindup(AWRBoss* Boss, FWRBossAttack& Attack, float DeltaSeconds);
	static bool BeginStrike(AWRBoss* Boss, FWRBossAttack& Attack);
	static bool TickStrike(AWRBoss* Boss, FWRBossAttack& Attack, float DeltaSeconds);
	static void EndStrike(AWRBoss* Boss, const FWRBossAttack& Attack);
	static void ModifyHit(const AWRBoss* Boss, const FWRBossAttack& Attack, FWRHit& Hit);
	static void HitLanded(AWRBoss* Boss, AWRCharacterBase* Victim, FName AttackId, EWRHitResult Result);
	static float Incoming(AWRBoss* Boss, const FWRHit& Hit, float Damage);
	/** Their weaknesses are their own (Sylvaroth: Necromancy), never the fire/frost rule's. */
	static bool OwnWeakness(const AWRBoss* Boss);
	static void Damaged(AWRBoss* Boss, const FWRHit& Hit, float Damage);
	static void PartBroken(AWRBoss* Boss, const FWRCreatureDef::FBodyPoint& Part, AActor* By);
	static void WallStun(AWRBoss* Boss, float Seconds);
	static void PhaseChanged(AWRBoss* Boss, int32 NewPhase);
	static bool PlaceRunback(AWRBoss* Boss);
	/** Sylvaroth's boss part: the Verdant core. */
	static FName BossPart(const AWRBoss* Boss, bool bScarred, FName Fallback);
	static void Died(AWRBoss* Boss);
	/** WRCombatLibrary's stance hook: the Hornwarden held by roots takes x2. */
	static float StanceTaken(AWRCharacterBase* Target, const FWRHit& Hit, float Stance);
	/** WRCombatLibrary, any blow that hurt anything (server): a blow on the Hornwarden's herd by a survivor or a tame. */
	static void AnyDamage(AWRCharacterBase* Target, const FWRHit& Hit, float Damage);
	/** A weapon swing (WRAbilities) or a projectile (WRProjectile) met an actor: true if it was a forest arena piece that
	 *  took the blow (a rope anchor, a Root Knot, a brood comb, an amber heart, the root hand's wrist). */
	static bool HitThing(AActor* Thing, UPrimitiveComponent* Comp, AWRCharacterBase* By, float Damage, bool bFire);
	/** WRProjectile's bola on the Hornwarden: roots him like a snare (true: handled). */
	static bool BolaHold(AWRCharacterBase* Target);
	/** A Bootlace blight jar broke (UWRForestWorld::OnBlightLanded): a Root Knot dies 90 s; Old Gnarl's stripped spot can't
	 *  regrow 30 s. */
	static void BlightLanded(UWorld* World, const FVector& At, AActor* Thrower);
	/** AWRForestLob came down (server): a volley's count, Old Gnarl's stump left lying where it landed. */
	static void LobLanded(AWRBoss* Boss, FName Kind);
	/** A Hornwarden's herd member (the never-hurt-the-herd rule, the Wild Hunt). */
	static bool IsHerd(const AActor* Who);
	/** The Wild Hunt, and the Matron to the bell (WRForestTypes.h contract). */
	static void SetWildHunt(UWorld* World, bool bOn);
	static void MatronToBell(UWorld* World, const FVector& Bell);

	// --- The arenas' layout (Data/forest.json arenas, with the cook's numbers where a row leaves one out); the same on
	// every machine, so the scenery, the server's pieces and the bosses agree (WRForestArenas.cpp). World cm. ---
	/** An arena's centre (ground), radius and fixed points; false when forest.json has no such arena. */
	static bool Arena(FName Id, FVector& Centre, float& Radius, TArray<FVector>& Points);
	/** The Clearing of Four Hearts: its middle, the four heart-trees (N, E, S, W) and the four standing stones. */
	static bool HeartsLayout(FVector& Centre, TArray<FVector>& Hearts, TArray<FVector>& Stones);
	/** The Bower: its six oaks' feet, and the six walkways' deck ends (at the trunks' platforms) and their oaks. */
	static bool BowerLayout(TArray<FVector>& Oaks, TArray<FVector>& WalkA, TArray<FVector>& WalkB, TArray<FIntPoint>& WalkTrees);
	/** The Paper Hollow: the dead oak's foot, the floors' heights over it (cm) and the four brood combs (world cm). */
	static bool HiveLayout(FVector& Base, TArray<float>& FloorZ, TArray<FVector>& Combs);

private:
	static bool TickGnarl(AWRBoss* Boss, FWRForestBossState& St, float Dt);
	static bool TickQueen(AWRBoss* Boss, FWRForestBossState& St, float Dt);
	static bool TickMatron(AWRBoss* Boss, FWRForestBossState& St, float Dt);
	static bool TickWarden(AWRBoss* Boss, FWRForestBossState& St, float Dt);
	static bool TickSylvaroth(AWRBoss* Boss, FWRForestBossState& St, float Dt);
	static void TickWaves(AWRBoss* Boss, FWRForestBossState& St);
	static void TickHolds(AWRBoss* Boss, FWRForestBossState& St, float Dt);
	static bool TickWardenRoute(AWRBoss* Boss, FWRForestBossState& St, float Dt, bool bFighting);
	static void MatronCanopyMove(AWRBoss* Boss, FWRForestBossState& St, float Dt);
	static void MatronDrop(AWRBoss* Boss, FWRForestBossState& St, bool bForGood);
	static void SpawnLob(AWRBoss* Boss, FName Kind, const FVector& From, const FVector& To, float Flight, float Arc, float Delay, float Damage, float Radius, float Entangle, uint8 Impact);
	static void Grab(AWRBoss* Boss, FWRForestBossState& St, AWRCharacterBase* Victim, FName How);
	static void ReleaseGrab(AWRBoss* Boss, FWRForestBossState& St, bool bThrow);
	static void Heal(AWRBoss* Boss, FWRForestBossState& St, float Dt);
	static void StartSectors(AWRBoss* Boss, FWRForestBossState& St);
	static void TickSectorsServer(AWRBoss* Boss, FWRForestBossState& St, float Dt);
	static void TickHearts(AWRBoss* Boss, FWRForestBossState& St);
	static void MatronClimb(AWRBoss* Boss, FWRForestBossState& St, int32 TreeIndex);
	static bool TickMatronClimb(AWRBoss* Boss, FWRForestBossState& St);
	/** Starts one of its rows now (a move its own rule calls: the regrow, the swarm, the screams, the enraged bellow). */
	static bool Force(AWRBoss* Boss, FName Id, AWRCharacterBase* Target);
	/** Drops the move it's in (a stagger from its counter mechanic). */
	static void CancelAttack(AWRBoss* Boss);
};

/**
 * Builds the four arena landmarks on every machine and, on the server, places the arena bosses (spawners), their stateful
 * pieces, the run-back stones and the Nature altar from Data/forest.json "arenas"; listens for the blight jar; keeps the
 * Wild Hunt flag and the Paper Hollow's water butts.
 */
UCLASS()
class WILDREACH_API UWRForestArenas : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UWRForestArenas, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return bActive; }
	static UWRForestArenas* Get(const UObject* WorldContext);

	/** Server: the Wild Hunt runs (also kept on AWRForestArenaState for every machine). */
	bool bWildHunt = false;
	/** Server: the bell rung at night (F-S5): where, and until when the Matron hunts it (world seconds). */
	FVector BellAt = FVector::ZeroVector;
	double BellUntil = 0.0;
	/** The Bower's walkways and the Matron's death (game hours) for their regrowth; the last walkway to fall (a fight
	 *  given up: they grow back 4 hours after it, so the next party finds the Bower whole). */
	double MatronDiedHours = -1.0;
	double LastFallHours = -1.0;
	/** The Clearing's resin weeping after Sylvaroth's kill: until (game hours). */
	double ResinUntilHours = -1.0;
	TArray<TWeakObjectPtr<AActor>> ResinNodes;
	/** The scenery built here (water butts, floors). */
	TArray<TWeakObjectPtr<AWRForestArenaScenery>> Scenery;

private:
	void BuildScenery();
	void SpawnServerPieces();
	void StepPlace();
	bool Place(FName Arena);
	void OnBlight(UWorld* World, const FVector& At, AActor* Thrower);
	void StepButts(float Dt);
	bool bActive = false;
	bool bScenery = false;
	bool bPieces = false;
	TSet<FName> Placed;
	/** The landmarks built on this machine (or with no arena to build). */
	TSet<FName> Built;
	float PlaceClock = 0.f;
	float ButtClock = 0.f;
	TMap<TWeakObjectPtr<AActor>, float> AtButt;
	FDelegateHandle BlightHandle;
	UPROPERTY(Transient) TObjectPtr<AWRForestArenaState> State;
};
