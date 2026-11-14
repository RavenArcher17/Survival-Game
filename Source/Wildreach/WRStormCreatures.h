#pragma once

// The creatures of the Storm peaks (cooks/biome-lightning.md 2.9, cooks/build-biome-lightning.md 1.3, 2.6): what they do
// beyond their attack rows (Data/attacks.json) and how they look on the stand-in rigs.
//
//  - UWRStormBody: a replicated component the server adds to every Storm peaks creature and boss (creatures.json biome
//    "storm_peaks"). It carries the state every machine draws from (Charge, flags, Kharvox's eye) and, on machines that
//    draw, the creature's own look: its palette as a tinted material over the rig's texture (M_CreatureTint) and the
//    silhouette parts modelled in code with the build-look kit (WRLookKit blocks on bone-attached frames): horns, bell,
//    crest, sail, star nose, plates, forked tail, Lichtenberg ferns, beard, feather tips.
//  - FWRStormAI: the hooks AWRCreatureAI calls (friend): Voltfang charge-up and crackle-howl, flank lunges, Thunderhorn
//    reads, Stormkite dives / darts / grounding, the Raiju's strike arrival, rebound leap, discharge and storm return,
//    the Galeback ring, Rodcrest / Puffram cornered, tame uses (Coilburrow ore sense and digging, Stormkite scout marks,
//    the Raiju's storm bonus, Voltfang shock bites).
//  - UWRStormLife: the server's world subsystem for the land's living-world beats (Voltfangs hunt Puffram at dusk, the
//    Rams' Ring clash, kites snatching Rodcrests, the Cragstrider alarm) and the Raiju's arrivals in weather strikes.
//  - AWRStormMark: an owner-only marker (a tame's find: ore its Coilburrow senses, creatures its Stormkite spotted).

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRStormTypes.h"
#include "WRStormCreatures.generated.h"

class AWRCreature;
class AWRCreatureAI;
class AWRCharacterBase;
class AWRCharacter;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class USkeletalMeshComponent;
struct FWRAttackRow;
struct FWRHit;

/** What a Storm peaks body shows, replicated as bits (UWRStormBody::Flags). */
namespace WRStormFlag
{
	enum : uint16
	{
		Charged     = 1 << 0,   // Voltfang fur lit (a static shake ready); Stormkite tail fork lit before a dive; Puffram wool charged
		Grounded    = 1 << 1,   // a Stormkite brought down (6 s), Kharvox crashed (curse / wing break)
		CrestUp     = 1 << 2,   // a Rodcrest's sail raised (storm, alarm)
		Underground = 1 << 3,   // Coilback tunnelling: the soil ridge moves over it
		Dimmed      = 1 << 4,   // Kharvox bleeding: the feathers go grey (no charge gain)
		Arriving    = 1 << 5,   // a Raiju not yet out of its strike
		Cloud       = 1 << 6,   // Galvara's banner cloud is round her
		Rimed       = 1 << 7,   // Coilback's frost-slowed tunnelling leaves a rime line
		Stuck       = 1 << 8,   // the Bellwether's horns in the rock
		Eye         = 1 << 9,   // Kharvox's eye of the storm is open (EyeAt)
		Blind       = 1 << 10,  // Coilback's star nose broken: no metal sense
		Curled      = 1 << 11,  // a Raiju curled for its discharge (halo blazing)
		Sensing     = 1 << 12,  // a star nose sparking (metal near)
		Clash       = 1 << 13,  // a Thunderhorn in a Rams' Ring clash
		Lit         = 1 << 14,  // a Rodcrest glowing after a strike near it / a storm-charged body (Raiju ferns full)
	};
}

/** Every machine: the state of a Storm peaks creature or boss; on machines that draw, its modelled look. */
UCLASS(ClassGroup=(Creature))
class WILDREACH_API UWRStormBody : public UActorComponent
{
	GENERATED_BODY()

public:
	UWRStormBody();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	static UWRStormBody* Get(const AActor* Actor);
	/** A creatures.json row of this land (biome "storm_peaks"). */
	static bool IsStormKind(FName CreatureId);
	/** Server: the component on a new Storm peaks creature (AWRCreature::BeginPlay). */
	static void Attach(AWRCreature* Creature);

	/** Kharvox's Charge (0..100, shown only on his feathers). */
	UPROPERTY(Replicated) uint8 Charge = 0;
	UPROPERTY(Replicated) uint16 Flags = 0;
	/** Kharvox's eye of the storm (phase 3), on the ground. */
	UPROPERTY(Replicated) FVector_NetQuantize EyeAt = FVector::ZeroVector;

	bool Has(uint16 Flag) const { return (Flags & Flag) != 0; }
	void Set(uint16 Flag, bool bOn) { Flags = bOn ? uint16(Flags | Flag) : uint16(Flags & ~Flag); }

	/** Every machine: a moment worth a burst (a ring pulse's veins, a bell toll, a crackle): an effect on the body. */
	UFUNCTION(NetMulticast, Unreliable) void MulticastMoment(FName Effect, FName Bone, float Scale);

	// --- Server state (creatures; bosses keep theirs in WRStormBosses) ---
	double LastAttackAt = 0.0;     // its last row (a Voltfang charges up after a lull)
	double RollUntil = 0.0;        // a Voltfang rolling in grit
	double CombatSince = -1.0;     // a Raiju's fight started (storm return after 60 s)
	double GroundedUntil = 0.0;    // a Stormkite down after its wings were hit
	int32 WingHits = 0;
	double HowledAt = -1000.0;     // a Voltfang pack's crackle-howl
	double AlarmAt = -1000.0;      // a Cragstrider's whistle
	double CorneredSince = -1.0;   // a Rodcrest / Puffram with nowhere to go
	double LitUntil = 0.0;
	FVector ArriveSpot = FVector::ZeroVector;
	double ArriveCalledAt = -1.0;
	bool bReturning = false;       // a Raiju leaving into its strike
	double ReturnCalledAt = -1.0;
	TWeakObjectPtr<AActor> SnatchedBy;  // a Rodcrest in a kite's talons
	// Tame uses.
	double NextSenseAt = 0.0, NextDigAt = 0.0, NextScoutAt = 0.0, ScoutUntil = 0.0;
	FVector ScoutAt = FVector::ZeroVector;
	// Living world: a clash partner, a snatch victim.
	TWeakObjectPtr<AWRCreature> Partner;
	double BeatUntil = 0.0;
	int32 BeatStep = 0;

private:
	// --- The look (machines that draw) ---
	void BuildLook();
	void ClearLook();
	void TickLook(float DeltaTime);
	void TickServer(float DeltaTime);
	/** A frame attached to a bone (keeping its world place), blocks spawned under it. Rot: the frame's world rotation. */
	USceneComponent* AddGroup(FName Bone, const FVector& WorldAt, const FRotator& WorldRot, const TArray<struct FWRLookBlock>& Blocks, int32 Kind);
	void SetGroupGlow(int32 Kind, float Glow);
	USkeletalMeshComponent* Skel() const;

	FName LookFor;                  // the creature id the look was built for
	struct FGroup { TWeakObjectPtr<USceneComponent> Frame; int32 Kind = 0; FRotator BaseRot = FRotator::ZeroRotator; FVector BaseLoc = FVector::ZeroVector; FVector BaseScale = FVector::OneVector; float Glow = -1.f; };
	TArray<FGroup> Groups;
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> LookComps;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> TintMats;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> LoopFx;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> EyeFx;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> EyeFrame;
	FName LoopFxName;
	// Bell spring (the Bellwether's bell swings with how he moves).
	FVector LastVelocity = FVector::ZeroVector;
	FVector2D BellAngle = FVector2D::ZeroVector, BellSpeed = FVector2D::ZeroVector;
	float StepClock = 0.f, FxClock = 0.f, CrestAlpha = 0.f, Clock = 0.f;
	float RimeClock = 0.f;
	int32 LookTries = 0;
};

/** The hooks AWRCreatureAI calls for Storm peaks creatures (friend of AWRCreatureAI and AWRCreature). */
struct WILDREACH_API FWRStormAI
{
	/** Wild brain: true when it ran this tick itself (arrival, return, charge-up roll, the herd ring, cornered, beats). */
	static bool TickWild(AWRCreatureAI* AI, AWRCreature* Self, float DeltaSeconds);
	/** Tame brain with nothing to fight: true when it ran this tick (a Stormkite out scouting). */
	static bool TickTame(AWRCreatureAI* AI, AWRCreature* Self, float DeltaSeconds);
	/** May it pick this row now (static shake only charged, grounded snap only grounded, a rebound only by a wall, a
	 *  flank lunge only from the flank, reads only in reach)? */
	static bool RowAllowed(const AWRCreatureAI* AI, const AWRCreature* Self, const FWRAttackRow& Row, const AWRCharacterBase* Target, bool bReading);
	static void RowStarted(AWRCreatureAI* AI, AWRCreature* Self, const FWRAttackRow& Row, AWRCharacterBase* Target);
	/** A row's strike: true when this took it over (discharge, darts, a kite's dive, a rebound off the wall). */
	static bool StrikeRow(AWRCreatureAI* AI, AWRCreature* Self, const FWRAttackRow& Row, AWRCharacterBase* Target);
	/** Its blows: lightning element on shock rows, a tame Raiju's storm bonus, a tame's shock bites. */
	static void ModifyHit(const AWRCreature* Self, FWRHit& Hit, const FWRAttackRow* Row);
	/** After a row's blow went in (the thunderclap's clap and flash). */
	static void AfterRowHit(AWRCreature* Self, AWRCharacterBase* Victim, const FWRAttackRow& Row);
	/** A charge that commits on its row's own tracking share (the Thunderhorn's ridge charge tracks half its wind-up). */
	static bool TracksCharge(const AWRCreature* Self);
	/** A brought-down Stormkite stays on the ground (no climbing back to the air until it's up). */
	static bool Grounded(const AWRCreature* Self);
	/** Damage on a Storm peaks body before it lands (Galvara's cores: arrows and spells x1.5). */
	static float Incoming(const AWRCreature* Self, const FWRHit& Hit, float Damage);
	/** After damage: wing hits, the crackle-howl, the alarm, a Raiju's fight clock. */
	static void Damaged(AWRCreature* Self, const FWRHit& Hit, float Damage);
	/** At its death (server): the chance drops (storm essence) into the killer's bag. */
	static void Died(AWRCreature* Self, AActor* Killer);
	/** The Raiju: called its arrival strike (hidden until it lands), or leaving into one. */
	static void BeginArrival(AWRCreature* Raiju, const FVector& Near);
	static void StrikeLanded(UWorld* World, const FWRStrikeCall& Call);
};

/** An owner-only marker over something a tame found (only its owner's machine gets it). */
UCLASS()
class WILDREACH_API AWRStormMark : public AActor
{
	GENERATED_BODY()

public:
	AWRStormMark();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	/** Server: a mark for Owner's machine at At (Follow: stays over it), Seconds long. Kind: ore / creature. */
	static AWRStormMark* Spawn(APlayerController* For, const FVector& At, AActor* Follow, FName Kind, float Seconds);
	UPROPERTY(Replicated) FName Kind;
	UPROPERTY(Replicated) TObjectPtr<AActor> Follow;
	/** This machine's live marks (a compass can list them). */
	static TArray<TWeakObjectPtr<AWRStormMark>> Live;

private:
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Fx;
};

/** Server: the land's living-world beats and the storm's Raiju (cooks/biome-lightning.md 2.9). */
UCLASS()
class WILDREACH_API UWRStormLife : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

private:
	void Step();
	void OnStrike(UWorld* World, const FWRStrikeCall& Call);
	void BeatHunt(float Hour);
	void BeatClash(float Hour);
	void BeatSnatch();
	FDelegateHandle StrikeHandle;
	FTimerHandle StepTimer;
	double NextSnatchAt = 0.0;
	double NextClashAt = 0.0;
};
