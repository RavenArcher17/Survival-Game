#pragma once

// The Sky islands' nine creatures (cooks/biome-sky.md 2.9; cooks/build-biome-sky.md 2.11): what they do beyond their attack
// rows (Data/attacks.json) and how they look.
//
//  - UWRSkyBody: a replicated component the server adds to every Sky creature (creatures.json biome "sky_islands", not a
//    boss). It carries what every machine draws from (flags: soaring, stooping, grounded, holding, parachuting, the
//    Undertow's pull, a breach, the horn-lobes unrolled, the crest flared, deflated...) and, on machines that draw, the
//    look: the cook's palette over the rig's own texture (M_CreatureTint) and the parts that make each silhouette,
//    modelled on frames riding the rig's bones (the Galeharrier's double crest and face ring, the Windmane's four feathered
//    wings and mane, the Puffwing's soft wings and red shield, the Gallowshrike's bandit mask, hooked beak and dagger
//    claws, the Hushwing's frogmouth gape and facial disc, the Mistlark's fanned tail). The Skyray, the Cloudback and the
//    Sailbell have no rig anywhere: their bodies are built and moved in code (UWRSkyBeast, WRSkyBeast.h).
//  - FWRSkyAI: the hooks AWRCreatureAI calls (the flyer rule: soar a ring over a home node, stoop on a locked line, sit
//    grounded after; ground creatures held to their island; the Undertow's cone pull; the impale and smother grabs; meat
//    theft; the Puffwing's parachute; the Sailbell's curtain; the Cloudback's spout; the Gallowshrike's hop-glide).
//  - UWRSkyLife: the server's populations per sub-zone and island (the generic wildlife can't stand things on floating
//    islands) and the living world's beats (Mistlarks in the columns, the Skyrays' dusk breach in a Gale, the
//    Galeharriers' robbery, the larder kills, the blooms' drift, the Windmanes' dawn grazing).

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRSkyCreatures.generated.h"

class AWRCreature;
class AWRCreatureAI;
class AWRCharacterBase;
class USkeletalMeshComponent;
class UMaterialInstanceDynamic;
class UWRSkyBeast;
struct FWRAttackRow;
struct FWRHit;
struct FWRLookBlock;
enum class EWRHitResult : uint8;

namespace WRSkyFlag
{
	enum : uint32
	{
		Soar      = 1 << 0,   // a flyer riding its ring over its home node (wings set, a slow bank)
		Stoop     = 1 << 1,   // folded, on its locked line
		Grounded  = 1 << 2,   // down after a stoop or a wing broken (the bring-down window)
		Holding   = 1 << 3,   // a grab holding someone (impale, smother, snatch)
		Parachute = 1 << 4,   // wings spread, falling slow (a Puffwing off an edge)
		Undertow  = 1 << 5,   // a Skyray's pull (horn-lobes unrolled, mouth open)
		Breach    = 1 << 6,   // a Skyray leaping out of the cloud
		Lobes     = 1 << 7,   // the horn-lobes unrolled (the Undertow's tell)
		Crest     = 1 << 8,   // the crest flared (the stoop's tell)
		Flying    = 1 << 9,   // a Windmane / Cloudback / Skyray off its feet (wings beating)
		Deflated  = 1 << 10,  // a Sailbell dead (not by fire): its curtain lies stinging 60 s
		Lash      = 1 << 11,  // a Sailbell's curtain sweeping out
		Gape      = 1 << 12,  // a Hushwing's head split open
		Mantle    = 1 << 13,  // a Hushwing's wings up and wide like a cloak (the smother's tell)
		Spout     = 1 << 14,  // a Cloudback's blowhole whistling
		Flap      = 1 << 15,  // hovering in place, wings beating hard
		Hood      = 1 << 16,  // a Windmane's hind-wings snapped open
		Asleep    = 1 << 17,  // roosting (a Hushwing by day, Mistlarks in the cedars at night)
		LeftHand  = 1 << 18,  // a Sailbell drifting left of the wind (right-handed drift right)
		Chased    = 1 << 19,  // fleeing (a Mistlark flock bursting, a Puffwing running for the edge)
	};
}

UCLASS(ClassGroup=(Creature))
class WILDREACH_API UWRSkyBody : public UActorComponent
{
	GENERATED_BODY()

public:
	UWRSkyBody();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	static UWRSkyBody* Get(const AActor* Actor);
	/** A creatures.json row of the Sky that isn't a boss. */
	static bool IsSkyKind(FName CreatureId);
	/** Server: the component on a new Sky creature (AWRCreature::BeginPlay). */
	static void Attach(AWRCreature* Creature);
	static TArray<TWeakObjectPtr<UWRSkyBody>> Live;

	UPROPERTY(ReplicatedUsing=OnRep_Flags) uint32 Flags = 0;
	UPROPERTY(Replicated) TObjectPtr<AActor> Holding;
	/** The body's own seed (the Skyray's belly blotches, a bloom's handedness): every machine draws the same. */
	UPROPERTY(Replicated) int32 Seed = 0;

	bool Has(uint32 Flag) const { return (Flags & Flag) != 0; }
	void Set(uint32 Flag, bool bOn);

	/** Every machine: a moment (an effect at a bone, a sound). */
	UFUNCTION(NetMulticast, Unreliable) void MulticastMoment(FName Effect, FName Bone, float Scale, FName Sound);

	// --- Server state ---
	/** The home it flies its ring over / grazes on (an island's id; the node's point). */
	FName HomeIsland;
	FVector HomeNode = FVector::ZeroVector;
	float RingRadius = 3000.f, RingHeight = 2500.f, RingPhase = 0.f, RingDir = 1.f;
	/** A roosting flyer (the Hushwing's shelf): its ring rides this far over its roost when it's out hunting. */
	float RingLift = 0.f;
	double GroundedUntil = 0.0;
	TWeakObjectPtr<AWRCharacterBase> HeldBody;
	double HoldUntil = 0.0, HoldTickAt = 0.0;
	FName HoldKind;
	float FreeStance = 0.f;
	FVector PinAt = FVector::ZeroVector;
	double ParachuteSince = 0.0;
	double DeflatedUntil = 0.0;
	double NextStingAt = 0.0;
	TMap<TWeakObjectPtr<AActor>, double> StungAt;
	FVector StoopFrom = FVector::ZeroVector, StoopTo = FVector::ZeroVector;
	double StoopStart = 0.0;
	double BreachStart = 0.0;
	FVector BreachFrom = FVector::ZeroVector;
	double NextVoiceAt = 0.0;
	double LastStolenAt = -100.0;
	double AirborneUntil = 0.0;   // a Windmane's takeoff gust: 4 s up, then back down with a charge
	bool bRattled = false;
	bool bScreeched = false;
	bool bSwallowed = false;

private:
	UFUNCTION() void OnRep_Flags();
	void BuildLook();
	void ClearLook();
	void TickLook(float DeltaTime);
	USceneComponent* AddGroup(FName Bone, const FVector& WorldAt, const FRotator& WorldRot, const TArray<FWRLookBlock>& Blocks, int32 Kind);
	USkeletalMeshComponent* Skel() const;

	FName LookFor;
	struct FGroup { TWeakObjectPtr<USceneComponent> Frame; int32 Kind = 0; FRotator BaseRot = FRotator::ZeroRotator; FVector BaseScale = FVector(1.f); float Phase = 0.f; float Side = 1.f; };
	TArray<FGroup> Groups;
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> LookComps;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> TintMats;
	UPROPERTY(Transient) TObjectPtr<UWRSkyBeast> Beast;
	float Clock = 0.f;
	float Beat = 0.f;   // eased wingbeat amount (0 still .. 1 hard flight)
	int32 LookTries = 0;
};

/** The hooks AWRCreatureAI calls for Sky creatures (friend of AWRCreatureAI). */
struct WILDREACH_API FWRSkyAI
{
	/** Wild brain: true when it ran this tick itself (soaring, grounded, holding, parachuting, drifting). */
	static bool TickWild(AWRCreatureAI* AI, AWRCreature* Self, float DeltaSeconds);
	static bool TickTame(AWRCreatureAI* AI, AWRCreature* Self, float DeltaSeconds);
	static bool RowAllowed(const AWRCreatureAI* AI, const AWRCreature* Self, const FWRAttackRow& Row, const AWRCharacterBase* Target, bool bReading);
	static void RowStarted(AWRCreatureAI* AI, AWRCreature* Self, const FWRAttackRow& Row, AWRCharacterBase* Target);
	/** A row's strike it takes over (the stoop's line, the Undertow, the belly-flop, the lash, the spout): true when it did. */
	static bool StrikeRow(AWRCreatureAI* AI, AWRCreature* Self, const FWRAttackRow& Row, AWRCharacterBase* Target);
	/** Every tick of a strike's active window (the pull, the line, the curtain). */
	static void TickStrike(AWRCreatureAI* AI, AWRCreature* Self, const FWRAttackRow& Row, float DeltaSeconds);
	static void Resolved(AWRCreature* Self, AWRCharacterBase* Victim, FName RowId, EWRHitResult Result);
	static float Incoming(const AWRCreature* Self, const FWRHit& Hit, float Damage);
	/** After a blow landed (server): a held body struck free, the Undertow cancelled by a blow to the mouth, a rattle. */
	static void Damaged(AWRCreature* Self, const FWRHit& Hit, float Damage);
	static void Died(AWRCreature* Self, AActor* Killer);
	static void TickBody(AWRCreature* Self, UWRSkyBody* Body, float DeltaTime);
	static void StartHold(AWRCreature* Self, UWRSkyBody* Body, AWRCharacterBase* Victim, FName Kind, float Seconds);
	static void Release(AWRCreature* Self, UWRSkyBody* Body, bool bThrow);
	/** A player's button press while held (mash): the hold ends sooner. */
	static void Mash(AWRCharacterBase* Victim);
	/** The body holding this one, if any (every machine). */
	static AWRCreature* HolderOf(const AActor* Victim);
	/** A Sky flyer (its row is a flyer, or a Windmane / Cloudback / Skyray / Sailbell that floats). */
	static bool Floats(const AWRCreature* Self);
};

/** The Sky's living world (server): populations on the islands and the beats. */
UCLASS()
class WILDREACH_API UWRSkyLife : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UWRSkyLife* Get(const UObject* WorldContext);
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UWRSkyLife, STATGROUP_Tickables); }
	virtual bool IsTickable() const override;
	/** Server: a Sky creature died (the larder, the crows of the sky: Galeharriers come to it). */
	void NoteDeath(AWRCreature* Dead, AActor* Killer);
	/** Server: spawn one of Kind on an island of Zone (or at At): its home set by the flyer rule. */
	AWRCreature* SpawnOn(FName Kind, FName Island, const FVector& At);
	/** Server: a Windmane egg taken from a ledge nest: the herd there turns on the thief. */
	void EggTaken(const FVector& Nest, AActor* Thief);

private:
	float Clock = 0.f;
	float BeatClock = 0.f;
	bool bSeeded = false;
	/** Per population row (tuning skyLife.populations): the creatures it keeps. */
	TMap<FName, TArray<TWeakObjectPtr<AWRCreature>>> Kept;
	TMap<FName, double> NextRefillAt;
	double LastBreachAt = -1000.0;
	double LastRobAt = -1000.0;
	double LastGrazeDay = -1.0;
	void StepPopulations();
	void BeatMistlarks();
	void BeatBreach();
	void BeatRobbery();
	void BeatGraze();
	void BeatBlooms();
};
