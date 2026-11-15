#pragma once

// The Ancient forest's eleven creatures (cooks/biome-forest.md 2.12; cooks/build-biome-forest.md 1.3, 2.9): what they
// do beyond their attack rows (Data/attacks.json) and how they look on their stand-in rigs.
//
//  - UWRForestBody: a replicated component the server adds to every forest creature (creatures.json biome "forest";
//    the bosses keep theirs in WRForestBosses). It carries what every machine draws from (flags, the perch / drop /
//    lift spot, who has a hand on a Mossmaul's moss) and, on machines that draw, the creature's look: the cook's
//    palette over the rig's own texture (M_CreatureTint) and the parts that make its silhouette, modelled as real
//    meshes (FWRToolMesh sweeps on M_HeldTool) on frames riding its bones, placed on the rig's own body surface (the
//    physics asset's shapes): the Mossmaul's moss hump and hooked claws, the Thornlynx's ear spikes and sabres, the
//    Rootcoil's bark plates, the Bramblewasp's orange head and banded abdomen, the Elderelk's palms, the Burlbeast's
//    bark plates and stockings, the Truffler's snout, the Hiveback's barrel, the Fernsheep's fern-curl fleece, the
//    Dappledoe's spots, the Glimmerfinch flock.
//  - UWRForestTouch: what the forest does to any other body (a survivor, a tame, a wasp): the Bramblewasp scout's mark,
//    Hivebacks clinging in a heat ball, a Maul hug or a Constrict holding it. Replicated, so the held survivor's own
//    machine knows to mash and both machines move it alike.
//  - FWRForestAI: the hooks AWRCreatureAI calls (canopy drops, tree escapes, log rest, the grabs, the roar, the bugle,
//    scout marks, heat balls, herds, rain hunting) and the tame uses (the Elderelk's thin ridden body, the Burlbeast's
//    heartwood, the Truffler's nose, the Glimmerfinch's pollen and whistle sweep, the Mossmaul's path-magic tame).
//  - UWRForestLife: the server's living-world beats (larders, dusk ambushes, Rootcoils across fords, midday wasp
//    raids, finch signs over kills and ripe patches, the Rootgrave dusk fight once Gravehounds exist).
//  - AWRForestMark: an owner-only marker over something a tame found (a cache its Truffler smelled).

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRForestCreatures.generated.h"

class AWRCreature;
class AWRCreatureAI;
class AWRCharacterBase;
class AWRCharacter;
class AWRPlayerController;
class APlayerController;
class USkeletalMeshComponent;
class UMaterialInstanceDynamic;
struct FWRAttackRow;
struct FWRHit;
struct FWRToolMesh;
enum class EWRHitResult : uint8;

/** What a forest body shows, replicated as bits (UWRForestBody::Flags). */
namespace WRForestFlag
{
	enum : uint16
	{
		Perched  = 1 << 0,   // a Thornlynx up a trunk (over a path, or escaped up it)
		DropTell = 1 << 1,   // its 0.9 s before the drop: leaves and bark rain from where it hangs
		Downed   = 1 << 2,   // knocked off its trunk: grounded 1.5 s
		LogRest  = 1 << 3,   // a Rootcoil lying still as a log among the roots (it breathes)
		Holding  = 1 << 4,   // a Maul hug or a Constrict holding someone
		PathTame = 1 << 5,   // a survivor's hand on a Mossmaul's moss: flowers and ferns growing into it
		Clinging = 1 << 6,   // a Hiveback clinging in a heat ball
		Reared   = 1 << 7,   // a Mossmaul up on its hind legs (hammer down, hug): the belly open
		Calmed   = 1 << 8,   // a wasp or bee dazed in smoke
		Lifted   = 1 << 9,   // a Glimmerfinch flock up and circling: a sign over a kill, a ripe patch, the whistle
		Rooting  = 1 << 10,  // a Truffler nose-down, digging
		Climbing = 1 << 11,  // a Thornlynx going up a trunk
		Larder   = 1 << 12,  // a Mossmaul guarding its cached kill
		Raiding  = 1 << 13,  // a Bramblewasp in a raid swarm
	};
}

/** Every machine: the state of a forest creature; on machines that draw, its modelled look. */
UCLASS(ClassGroup=(Creature))
class WILDREACH_API UWRForestBody : public UActorComponent
{
	GENERATED_BODY()

public:
	UWRForestBody();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	static UWRForestBody* Get(const AActor* Actor);
	/** A creatures.json row of this land that isn't a boss (biome "forest"). */
	static bool IsForestKind(FName CreatureId);
	/** Server: the component on a new forest creature (AWRCreature::BeginPlay). */
	static void Attach(AWRCreature* Creature);
	/** The creature kind its rules and look are for (creatures.json look.kind; the id when it has none). */
	FName KindId() const;
	/** This machine's forest bodies (grips, clings and herds are looked up here, not in a world sweep). */
	static TArray<TWeakObjectPtr<UWRForestBody>> Live;

	UPROPERTY(Replicated) uint16 Flags = 0;
	/** The spot it's about: a perch it hangs from, the ground it drops on, the point a finch flock circles. */
	UPROPERTY(Replicated) FVector_NetQuantize Spot = FVector::ZeroVector;
	/** The survivor laying a hand on its moss (path-magic tame), by player name; empty when nobody is. */
	UPROPERTY(Replicated) FString PathTamer;

	bool Has(uint16 Flag) const { return (Flags & Flag) != 0; }
	void Set(uint16 Flag, bool bOn) { Flags = bOn ? uint16(Flags | Flag) : uint16(Flags & ~Flag); }

	/** Every machine: a moment worth an effect on the body (a snort, a bugle's breath, the moss blooming). */
	UFUNCTION(NetMulticast, Unreliable) void MulticastMoment(FName Effect, FName Bone, float Scale);

	// --- Server state ---
	double LastAttackAt = 0.0;
	// The Thornlynx's perch (0 none, 1 walking to the trunk's foot, 2 climbing, 3 up). Escape: up because it was hurt.
	int32 PerchStage = 0;
	bool bEscape = false;
	FVector PerchAt = FVector::ZeroVector, TrunkFoot = FVector::ZeroVector;
	double PerchSince = 0.0, PerchUntil = 0.0, NextPerchAt = 0.0, NextEscapeAt = 0.0, DownedUntil = 0.0;
	bool bDropping = false;
	double DropStarted = 0.0;
	// The Rootcoil's rest; any ford it lies across.
	double RestAgainAt = 0.0;
	double NextBreathAt = 0.0;
	// The grabs (Maul hug, Constrict): the catch window, the held body, when it lets go.
	double GrabUntil = 0.0;
	FName GrabRow;
	TWeakObjectPtr<AWRCharacterBase> Hold;
	double HoldStart = 0.0, HoldEnds = 0.0, HoldTickAt = 0.0;
	float FreeDamage = 0.f;
	// The Mossmaul: its roar this fight, its larder.
	bool bRoared = false;
	double LastFightAt = -1000.0;
	FVector LarderAt = FVector::ZeroVector;
	double LarderUntil = 0.0;
	double NextRainHuntAt = 0.0;
	// The Elderelk's bugle; herds alerted.
	double BugledAt = -1000.0;
	// The Hiveback's heat ball: the body it flies at, the one it clings to, its slot on it.
	TWeakObjectPtr<AWRCharacterBase> BallTarget, ClingTo;
	double BallUntil = 0.0, ClingUntil = 0.0, ClingTickAt = 0.0, WaspKillAt = 0.0;
	int32 ClingSlot = 0;
	// A raid swarm member: where it came from and the hive it's after.
	FVector RaidFrom = FVector::ZeroVector, RaidTo = FVector::ZeroVector;
	bool bRaider = false, bRaidArrived = false, bRaidDone = false;
	double RaidQuietSince = -1.0, RaidStarted = 0.0;
	// A finch flock lifted over something (a sign), and until when.
	double LiftUntil = 0.0;
	// Tame uses.
	double NextTruffleAt = 0.0, NextSeedAt = 0.0, NextCacheAt = 0.0, RootingUntil = 0.0, NextPitAt = 0.0;
	int32 LogDragDay = -1;
	double DigStarted = -1.0;
	FVector DigAt = FVector::ZeroVector;
	// Path-magic tame (the Mossmaul): when the hand went on.
	double PathTameStart = 0.0;
	// Hover (wasps, bees, finches): its current drift goal.
	FVector HoverGoal = FVector::ZeroVector;
	double NextHoverAt = 0.0;
	// The Elderelk's bugle owed (it was hit: the bugle comes before anything else), and at whom.
	double BugleWantedAt = -1000.0;
	TWeakObjectPtr<AActor> BugleAt;
	// The wasps' look round for a marked body; a Truffler's last snort; a passive's corner; the last sleep check.
	double NextWaspScanAt = 0.0, NextSnortAt = 0.0, CorneredSince = -1.0, NextSleepCheckAt = 0.0;
	bool bWasFleeing = false;
	// A beat of the living world under way: what (ForestBeat), where it's going, until when, on whom.
	int32 Beat = 0;
	FVector BeatGoal = FVector::ZeroVector;
	double BeatUntil = 0.0;
	TWeakObjectPtr<AActor> BeatOn;
	// The Mossmaul whose larder this carcass is (on the dead Burlbeast).
	TWeakObjectPtr<AWRCreature> LarderOf;
	// A tame's own clocks: the Burlbeast's log drag, the Truffler's dig spot search.
	double NextDragCheckAt = 0.0, NextDigSenseAt = 0.0;
	/** The row it goes into next, at once, when its current one ends (drop -> fang bite, log strike -> constrict). */
	FName ForceNext;
	double ForceUntil = 0.0;
	/** The trunk it's on (centre at ground level, radius cm) and the climb's start angle round it. */
	FVector TrunkAt = FVector::ZeroVector;
	float TrunkRadius = 0.f, ClimbAngle = 0.f;
	double ClimbStarted = 0.0;
	/** A survivor's body clinging in a heat ball: which of the 4 places round it this bee holds. */
	bool bLanded = true;
	/** The Rootcoil's ford (or rest place) it lies across, and the yaw it lies at. */
	FVector RestAt = FVector::ZeroVector;
	float RestYaw = 0.f;

private:
	void BuildLook();
	void ClearLook();
	void TickLook(float DeltaTime);
	void TickServer(float DeltaTime);
	/** A frame on a bone at WorldAt (keeping its world place, turned WorldRot), the mesh built in the frame's space. */
	USceneComponent* AddPart(FName Bone, const FVector& WorldAt, const FRotator& WorldRot, const FWRToolMesh& Mesh, int32 Kind);
	/** The same under another part's frame (a flock finch's wings ride its body). */
	USceneComponent* AddPartTo(USceneComponent* Parent, const FVector& WorldAt, const FRotator& WorldRot, const FWRToolMesh& Mesh, int32 Kind);
	USkeletalMeshComponent* Skel() const;
	/** The mesh's own pose offsets (the Thornlynx hanging on a trunk, the Mossmaul reared, the Rootcoil lying as a log). */
	void TickPose(float DeltaTime);
	void TickThin();
	/** The forest's own fit on top of the creature's (look.flatten cm tall, look.stretch along the spine). */
	void ApplyFit();
	/** Every part of a kind shown or hidden (the bloom while a hand is on the moss, the coils round a held body). */
	void ShowParts(int32 Kind, bool bShow);
	FVector FitScale = FVector::ZeroVector;
	double SettleUntil = 0.0;
	bool bPosing = false;
	float DownAlpha = 0.f;

	FName LookFor;
	uint8 LookSex = 0;
	// The fitted mesh transform the pose offsets start from (re-read when the fit runs again).
	FVector SkelBaseLoc = FVector::ZeroVector;
	FRotator SkelBaseRot = FRotator::ZeroRotator;
	FVector SkelBaseScale = FVector::OneVector;
	bool bSkelBase = false, bAnimPaused = false;
	float HangAlpha = 0.f, ClimbAlpha = 0.f, RearAlpha = 0.f, RestAlpha = 0.f, BloomAlpha = 0.f, SpreadAlpha = 0.f, TailAlpha = 0.f;
	float RootClock = 0.f;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> LiftFx;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> BloomFx;
	struct FPart { TWeakObjectPtr<USceneComponent> Frame; int32 Kind = 0; FRotator BaseRot = FRotator::ZeroRotator; FVector BaseLoc = FVector::ZeroVector; FVector BaseScale = FVector::OneVector; float Phase = 0.f; bool bShown = true; FVector Orbit = FVector::ZeroVector; };
	TArray<FPart> Parts;
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> LookComps;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> TintMats;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> LoopFx;
	FName LoopFxName;
	float Clock = 0.f, FxClock = 0.f, WetAlpha = -1.f;
	int32 LookTries = 0;
	float BaseRadius = 0.f;
	bool bThin = false;
};

/** What the forest does to a body that isn't a forest creature's own: the wasp mark, heat-ball bees, a grab. */
UCLASS(ClassGroup=(Creature))
class WILDREACH_API UWRForestTouch : public UActorComponent
{
	GENERATED_BODY()

public:
	UWRForestTouch();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	static UWRForestTouch* Get(const AActor* Actor);
	/** Server: the component on a body (made the first time the forest touches it). */
	static UWRForestTouch* FindOrAdd(AActor* Actor);

	/** Marked by a Bramblewasp scout (its own machine draws the pale dust). */
	UPROPERTY(ReplicatedUsing=OnRep_Mark) bool bWaspMark = false;
	/** Hivebacks clinging to it in a heat ball (each one slows it). */
	UPROPERTY(Replicated) uint8 Clinging = 0;
	/** This machine's touched bodies (the wasps look here for a mark). */
	static TArray<TWeakObjectPtr<UWRForestTouch>> Live;
	/** The creature holding it (Maul hug, Constrict; a Hiveback's grip on a wasp). */
	UPROPERTY(Replicated) TObjectPtr<AActor> HeldBy;

	UFUNCTION() void OnRep_Mark();

	// --- Server state ---
	double MarkUntil = 0.0;
	TArray<TWeakObjectPtr<AWRCreature>> Bees;
	bool bWasDodging = false;

private:
	void ShowMark(bool bOn);
	UPROPERTY(Transient) TObjectPtr<USceneComponent> MarkFx;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> HeatFx;
	bool bMarkShown = false;
};

/** The grabs and heat-ball clings as the rest of the game sees them (WRCharacter's mash, the movement speed). */
namespace WRForestGrip
{
	/** Held in a Mossmaul's hug or a Rootcoil's coils: presses mash out of it (either machine). */
	WILDREACH_API bool IsHeld(const AActor* Who);
	/** Server: a mash press from a held survivor (-mash s off the hold, never below its minimum). */
	WILDREACH_API void Mash(AActor* Who);
	/** Movement speed x: 0 while held, -10% per clinging Hiveback (both machines read the replicated state). */
	WILDREACH_API float MoveScale(const AActor* Who);
}

/** A tamed Burlbeast within tame.forest.haulRange of At (heartwood comes off a fallen giant x1.25 there). Server. */
WILDREACH_API bool WRForestBurlbeastNear(const UObject* WorldContext, const FVector& At);
/** Who is riding an Elderelk (it slips through thornbrakes with its rider). */
WILDREACH_API bool WRForestRidingElderelk(const AActor* Who);
/** A tamed Truffler has dug at At (within Radius) for its full tame.forest.digSeconds (the Hollow Root's soft spot). */
WILDREACH_API bool WRForestTrufflerDugAt(const UObject* WorldContext, const FVector& At, float Radius);

/** The hooks AWRCreatureAI calls for forest creatures (friend of AWRCreatureAI and AWRCreature). */
struct WILDREACH_API FWRForestAI
{
	/** Wild brain: true when it ran this tick itself (perches, rest, holds, raids, hovering, larders, cornered prey). */
	static bool TickWild(AWRCreatureAI* AI, AWRCreature* Self, float DeltaSeconds);
	/** Tame brain with nothing to fight: true when it ran this tick (a finch sweep, a Truffler digging). */
	static bool TickTame(AWRCreatureAI* AI, AWRCreature* Self, float DeltaSeconds);
	/** May it pick this row now (the drop only from a perch, the roar once a fight, the bees' stings near their hive)? */
	static bool RowAllowed(const AWRCreatureAI* AI, const AWRCreature* Self, const FWRAttackRow& Row, const AWRCharacterBase* Target, bool bReading);
	static void RowStarted(AWRCreatureAI* AI, AWRCreature* Self, const FWRAttackRow& Row, AWRCharacterBase* Target);
	/** A row's strike: true when this took it over (the drop, the climb, a grab, the roar, the heat ball, the mark). */
	static bool StrikeRow(AWRCreatureAI* AI, AWRCreature* Self, const FWRAttackRow& Row, AWRCharacterBase* Target);
	/** How a row's blow came out (ApplyHit's resolution): the catches (Maul hug, Constrict) and the follow-ups that come
	 *  only when the opener landed (drop -> bite, log strike -> constrict). */
	static void Resolved(AWRCreature* Self, AWRCharacterBase* Victim, FName RowId, EWRHitResult Result);
	static void ModifyHit(const AWRCreature* Self, FWRHit& Hit, const FWRAttackRow* Row);
	static void AfterRowHit(AWRCreature* Self, AWRCharacterBase* Victim, const FWRAttackRow& Row);
	/** Damage on a forest body before it lands (the Mossmaul's belly, the Burlbeast's back plates, bees on wasps). */
	static float Incoming(const AWRCreature* Self, const FWRHit& Hit, float Damage);
	/** After damage: knocked off a trunk, a hold broken, the herd turning, the bugle. */
	static void Damaged(AWRCreature* Self, const FWRHit& Hit, float Damage);
	/** At its death (server): chance drops, holds let go, a larder made, a fresh kill for the finches. */
	static void Died(AWRCreature* Self, AActor* Killer);
	/** The Mossmaul's taming is the hold-Interact path magic (TryPathMagic leaves it alone). */
	static bool OwnsPathMagic(const AWRCreature* Self);
	/** Server: Interact on a wild creature: a Mossmaul you can calm (knocked out, or below 30% and Entangled; 10+
	 *  Nature points; 40 Growth) starts the 4 s hold. True when it started (OutResult says so). */
	static bool TryPathTame(AWRCreature* Wild, AWRPlayerController* Player, FString& OutResult);
	/** Server: Interact let go. A tap (under tame.forest.pathTameTap s) feeds as usual instead. */
	static void StopPathTame(AWRPlayerController* Player);
	/** Client: is there a Mossmaul near this player one might be laying a hand on (send the release)? */
	static bool PathTameNear(const APlayerController* Player);
	/** Either machine: the path-magic tame is open to Who on Self (the prompt and the client's E read it). OutVerb is the
	 *  prompt's verb ("Hold: grow ferns into its moss (4 s, 40 Growth)"). */
	static bool PathTamePrompt(const AWRCreature* Self, const AActor* Who, FString& OutVerb);
	/** Server: a tame heard its owner's whistle (a Glimmerfinch flock sweeps the nearest ripe plant or hive). */
	static void Whistled(AWRCreature* Tame, AActor* Owner);
	/** A tamed Glimmerfinch flock pollinates crops within tame.forest.pollinateRange of At. */
	static bool FinchPollinates(const UObject* WorldContext, const FVector& At);
	/** The forest's rain over At by day (cooks/biome-forest.md 2.9): its night hunters (creatureAI.forest.rainHunters:
	 *  Mossmaul, Rootcoil) come out and hunt; a night-only spawner of one doesn't lie up (AWRCreatureSpawner::IsLyingUp). */
	static bool OutInRain(const UObject* WorldContext, FName CreatureId, const FVector& At);

	// Internal (the body and the touch call these: they need the AI's row machine).
	static void ContactRow(AWRCreature* Self, AWRCharacterBase* Victim);
	static int32 RowPhaseOf(const AWRCreature* Self, FName* OutRow = nullptr);
	static void BeginHold(AWRCreature* Self, AWRCharacterBase* Victim);
	static void ReleaseHold(AWRCreature* Self, bool bThrow);
	static void Cling(AWRCreature* Bee, AWRCharacterBase* Victim);
	static void Uncling(AWRCreature* Bee, bool bKnocked);
	static void FinishPathTame(AWRCreature* Mossmaul, AWRCharacter* Tamer);
	static void WakeFor(AWRCreature* Self, float Seconds);
	/** The body's server life each tick (UWRForestBody::TickComponent): holds, clings, perches, raids, tame uses. */
	static void TickBody(AWRCreature* Self, UWRForestBody* Body, float DeltaTime);
	/** Server: the species' signature call (cook 2.15) on every machine. */
	static void Signature(AWRCreature* Self);
	/** A wild Bramblewasp (raids, marks and heat balls are about these). */
	static bool IsWasp(const AActor* Who);
	/** A raid swarm member's brain: to the hive tree, at its Hivebacks, home again (true: it ran this tick). */
	static bool TickRaid(AWRCreatureAI* AI, AWRCreature* Self, UWRForestBody* Body, float DeltaSeconds);
	/** The herd within Radius turns on By together (the Elderelk's bugle). */
	static void Rally(AWRCreature* Self, AActor* By, float Radius);
	/** The flags every machine draws from, from the server's state and its brain's row. */
	static void SyncFlags(AWRCreature* Self, UWRForestBody* Body);
};

/** An owner-only marker over something a tame found (only its owner's machine gets it). */
UCLASS()
class WILDREACH_API AWRForestMark : public AActor
{
	GENERATED_BODY()

public:
	AWRForestMark();
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	/** Server: a mark for Owner's machine at At, Seconds long. Kind: cache / pit / dig. */
	static AWRForestMark* Spawn(APlayerController* For, const FVector& At, FName Kind, float Seconds);
	UPROPERTY(Replicated) FName Kind;

private:
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Fx;
};

/** Server: the forest's living-world beats (cooks/biome-forest.md 2.12.3). */
UCLASS()
class WILDREACH_API UWRForestLife : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** A fresh kill (a body to butcher): a sign for the Glimmerfinches for a few minutes. */
	static void NoteKill(UWorld* World, const FVector& At);

private:
	void Step();
	void BeatLarders();
	void BeatDusk(float Hour);
	void BeatFords();
	void BeatWasps(float Hour);
	void BeatFinches();
	void BeatRootgrave(float Hour);
	FTimerHandle StepTimer;
	struct FKill { FVector At = FVector::ZeroVector; double When = 0.0; };
	TArray<FKill> Kills;
	TArray<FVector> Fords;
	bool bFordsBuilt = false;
	double NextLarderAt = 0.0, NextFinchAt = 0.0, NextRootgraveAt = 0.0;
	int32 ScoutDay = -1;
	double RaidAtHours = -1.0;
	FVector RaidNest = FVector::ZeroVector, RaidHive = FVector::ZeroVector;
};
