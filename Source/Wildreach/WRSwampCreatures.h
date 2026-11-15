#pragma once

// The creatures of the Swamp (cooks/biome-swamp.md 2.12; cooks/build-biome-swamp.md 2.6): what they do beyond their
// attack rows (Data/attacks.json) and how they look on the stand-in rigs. The Storm peaks pattern (WRStormCreatures).
//
//  - UWRSwampBody: a replicated component the server adds to every swamp creature (creatures.json biome "swamp"). It
//    carries the state every machine draws from (flags, the body it holds) and, on machines that draw, the creature's
//    own look: the cook's palette as a tinted material over the rig's texture (M_CreatureTint) and the silhouette parts
//    modelled in code with the build-look kit on bone-attached frames: the Murkjaw's splayed teeth bar, moss-tufted
//    osteoderms and red eyeshine; the Gorgeleech's ring bands, ten-eye crescent, striped flanks and three-jaw disc; the
//    Hexcrow's bone-white rook face and ragged hackles; the Bogstalker's horned brows, lip line and pumping throat; the
//    Mirestrider's shoe bill, S-neck and splayed toes; the Mudhulk's box head and tusks; the Bogsnout's curled tusks; the
//    Lanterntoad's glowing throat sac (a real light); the Reedfowl's brow shield; the Slimeshell's coiled banded shell.
//  - FWRSwampAI: the hooks AWRCreatureAI calls (friend): the Murkjaw's duckweed ambush, burst and death roll, the
//    Gorgeleech latch, the Hexcrow's curse cry, dive and bring-down, call of the murder, the Bogstalker's tongue, gulp,
//    belly-flop and mud spit, the Mirestrider clatter, the Mudhulk's yawn, charge and wallow surge, the Lanterntoad's
//    flash, the Slimeshell's shell, the Reedfowl's cornered peck; tame uses; the Murkjaw blood rite.
//  - WRSwampGrip: the grabs as the rest of the game sees them (held, latched, mash, move speed).
//  - UWRSwampLife: the server's world subsystem for the land's living-world beats (leeches on the Mudhulks, the murder
//    over the dying and the dead, Murkjaws at the water's edge at dawn, the Salt Lick and the wallows).
//  - AWRSwampMark: an owner-only marker over what a tame Bogsnout found.

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRSwampCreatures.generated.h"

class AWRCreature;
class AWRCreatureAI;
class AWRCharacterBase;
class AWRCharacter;
class AWRPlayerController;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class USkeletalMeshComponent;
struct FWRAttackRow;
struct FWRHit;
struct FWRLookBlock;
enum class EWRHitResult : uint8;

/** What a swamp body shows, replicated as bits (UWRSwampBody::Flags). */
namespace WRSwampFlag
{
	enum : uint16
	{
		Ambush     = 1 << 0,   // a Murkjaw lying in wait under the duckweed (eyes and ridge only)
		Gape       = 1 << 1,   // jaws wide (death roll tell, Mudhulk yawn), eyeshine blazing
		Rolling    = 1 << 2,   // a Murkjaw in its death roll (belly up: x1.3)
		Latched    = 1 << 3,   // a leech on a body (attached to its limb)
		Grounded   = 1 << 4,   // a Hexcrow brought down
		Withdrawn  = 1 << 5,   // a Slimeshell in its shell
		Lit        = 1 << 6,   // a Lanterntoad's sac brightening for its flash
		Pumping    = 1 << 7,   // a Bogstalker's throat pumping (the tongue tell)
		Mouthful   = 1 << 8,   // a Bogstalker with a survivor in its mouth
		Hexed      = 1 << 9,   // a hexed beast (the Hex-Warden's aura, WRSwampBosses): garnet eyes
		Pouched    = 1 << 10,  // a tame Lanterntoad in its owner's shoulder pouch
		Wallow     = 1 << 11,  // a Mudhulk sunk for its surge / wallowing
		Stab       = 1 << 12,  // a Mirestrider standing to fish
		Disc       = 1 << 13,  // a Gorgeleech's three jaws open (the latch tell, and while it's on)
		Crying     = 1 << 14,  // a Hexcrow's head back, hackles flared (the curse cry)
		Tongue     = 1 << 15,  // a Bogstalker's tongue out
	};
}

/** Every machine: the state of a swamp creature; on machines that draw, its modelled look. */
UCLASS(ClassGroup=(Creature))
class WILDREACH_API UWRSwampBody : public UActorComponent
{
	GENERATED_BODY()

public:
	UWRSwampBody();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	static UWRSwampBody* Get(const AActor* Actor);
	/** A creatures.json row of this land (biome "swamp"). */
	static bool IsSwampKind(FName CreatureId);
	/** Server: the component on a new swamp creature (AWRCreature::BeginPlay). */
	static void Attach(AWRCreature* Creature);

	UPROPERTY(Replicated) uint16 Flags = 0;
	/** The body in its jaws, mouth or on its sucker (death roll, gulp, latch): every machine draws it held. */
	UPROPERTY(Replicated) TObjectPtr<AActor> Holding;
	/** The Murkjaw blood rite on this body (every machine reads it for the E prompt): 0 open, 1 under way, 2 done this
	 *  knockout (E feeds it as usual then). */
	UPROPERTY(Replicated) uint8 RiteState = 0;

	bool Has(uint16 Flag) const { return (Flags & Flag) != 0; }
	void Set(uint16 Flag, bool bOn) { Flags = bOn ? uint16(Flags | Flag) : uint16(Flags & ~Flag); }

	// --- For the other swamp tracks ---
	/** A hexed beast (the Hex-Warden's aura, the Red Tide's night walk): the garnet eyes. The Bosses track sets it. */
	bool IsHexed() const { return Has(WRSwampFlag::Hexed); }
	void SetHexed(bool bOn) { Set(WRSwampFlag::Hexed, bOn); }
	/** Server: knocks the leeches off a body (a salt pouch: bAll; one otherwise, the newest). Returns how many came off. */
	static int32 DetachLatches(AActor* Victim, bool bAll);
	/** How many leeches are on this body now (server). */
	static int32 LatchCount(const AActor* Victim);
	/** A survivor riding a swamp mount that keeps them out of this hazard (bleedwater / quag / marsh_gas: riderPasses). */
	static bool MountPasses(const AActor* Rider, FName Hazard);
	/** Gathering near a tame that boosts it (a Bogsnout: bloodmoss x2 within 15 m of it): the multiplier (1 = none). */
	static float GatherMult(const AActor* Gatherer, FName Item);
	/** A creature that ignores this swamp hazard (creatures.json resists: bleedwater, quag, marsh_gas). */
	static bool IgnoresHazard(const AActor* Who, FName Hazard);
	/** This machine's swamp bodies (grips and leeches are looked up here, not in a world sweep). */
	static TArray<TWeakObjectPtr<UWRSwampBody>> Live;

	/** Every machine: a moment worth a burst on the body (a duckweed burst, a flash): an effect at a bone. */
	UFUNCTION(NetMulticast, Unreliable) void MulticastMoment(FName Effect, FName Bone, float Scale);
	/** Every machine: a tell that runs along the ground or water toward a point over Seconds (the duckweed parting, a
	 *  V-wake): the effect every 60 cm from From to To. */
	UFUNCTION(NetMulticast, Unreliable) void MulticastLine(FName Effect, FVector_NetQuantize From, FVector_NetQuantize To, float Seconds);

	// --- Server state ---
	double LastAttackAt = 0.0;
	FName ForceNext;                 // the row it goes into next, at once (burst -> roll, tongue -> gulp)
	double ForceUntil = 0.0;
	// A hold (death roll, gulp): who, until when, the damage clock, ally hits on the head / throat, mash.
	TWeakObjectPtr<AWRCharacterBase> Held;
	double HoldUntil = 0.0;
	double HoldStarted = 0.0;
	float HoldTick = 0.f;
	float FreeDamage = 0.f;
	bool bDragUnder = false;
	FName HoldKind;                  // death_roll / gulp
	// A latch: the body, the bone, the drain clock, the dodge it already rolled for.
	TWeakObjectPtr<AWRCharacterBase> LatchedOn;
	FName LatchBone;
	float LatchTick = 0.f;
	bool bLatchRollSeen = false;
	double LatchedAt = 0.0;
	// A Hexcrow: down until, the murder called this fight, circling over.
	double GroundedUntil = 0.0;
	double GroundedSince = 0.0;
	bool bMurderCalled = false;
	TWeakObjectPtr<AActor> CircleOver;
	double LandUntil = 0.0;
	// A strike's own moments: the flop's landing, the dive's landing, the flurry's pecks, the tongue's pull.
	bool bFlopLanded = false;
	bool bDiveLanded = false;
	int32 PeckIndex = -1;
	TWeakObjectPtr<AWRCharacterBase> Pulled;
	double PullCheckAt = 0.0;
	// Moments and beats.
	double GapeUntil = 0.0;
	double LastClatterAt = -100.0;
	double NextYawnAt = 0.0;
	double WithdrawUntil = 0.0;
	double AmbushSince = -1.0;
	double StabUntil = 0.0, NextStabAt = 0.0;
	double FleeUntil = 0.0;
	double BeatUntil = 0.0;
	TWeakObjectPtr<AActor> BeatOn;
	FVector BeatAt = FVector::ZeroVector;
	int32 BeatStep = 0;
	bool bChannelPlaced = false;
	// Tame uses.
	double NextFindAt = 0.0;
	double PouchOutSince = -1.0;
	// The Murkjaw blood rite (on the Murkjaw): who's bleeding into its mouth, since when, the HP clock.
	TWeakObjectPtr<AWRCharacter> Ritualist;
	double RitualStarted = 0.0;
	float RitualTick = 0.f;
	FVector RitualFrom = FVector::ZeroVector;
	double RitualFxAt = 0.0;
	bool bRiteDone = false;          // once per knockout
	// A hexed beast (the Hex-Warden's aura) until when; a leech's limb slot; a held body's drag; misc clocks.
	double HexUntil = 0.0;
	int32 LatchSlot = -1;
	double NextHoldTickAt = 0.0;
	double NextBeatAt = 0.0;
	double CircleSince = 0.0;
	double NextPeckAt = 0.0;
	bool bTideSpawned = false;

private:
	// --- The look (machines that draw) ---
	void BuildLook();
	void ClearLook();
	void TickLook(float DeltaTime);
	USceneComponent* AddGroup(FName Bone, const FVector& WorldAt, const FRotator& WorldRot, const TArray<FWRLookBlock>& Blocks, int32 Kind);
	void SetGroupGlow(int32 Kind, float Glow);
	void SetGroupVisible(int32 Kind, bool bVisible);
	USkeletalMeshComponent* Skel() const;
	void ApplyFlatten();

	FName LookFor;
	struct FGroup { TWeakObjectPtr<USceneComponent> Frame; int32 Kind = 0; FRotator BaseRot = FRotator::ZeroRotator; FVector BaseLoc = FVector::ZeroVector; FVector BaseScale = FVector::OneVector; float Glow = -1.f; bool bVisible = true; };
	TArray<FGroup> Groups;
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> LookComps;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> TintMats;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> SacLight;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> LoopFx;
	FName LoopFxName;
	float Flatten = 0.f;          // the body's height share after the fit (the Murkjaw's low wedge), 0 = none
	float FlattenZ = 0.f;         // the mesh's Z scale once flattened (re-applied if the fit runs again)
	// The death roll's spin (the mesh turned about its long axis), the tongue's reach, the sac's glow.
	FRotator RollBaseRot = FRotator::ZeroRotator;
	FVector RollBaseLoc = FVector::ZeroVector;
	bool bRollSaved = false;
	float RollAngle = 0.f, TongueLen = 0.f, SacGlow = 0.f, EyeGlow = -1.f;
	bool bPawnIgnored = false;
	bool bSkelHidden = false;
	/** A tell running along the water toward a point (MulticastLine). */
	struct FLineFx { FName Effect; FVector From = FVector::ZeroVector, To = FVector::ZeroVector; float Seconds = 1.f, Age = 0.f; int32 Done = 0; };
	TArray<FLineFx> Lines;
	float Clock = 0.f, FxClock = 0.f, PumpClock = 0.f, GapeAlpha = 0.f, CrestAlpha = 0.f, StepClock = 0.f;
	int32 LookTries = 0;
};

/** The hooks AWRCreatureAI calls for swamp creatures (friend of AWRCreatureAI). */
struct WILDREACH_API FWRSwampAI
{
	/** Wild brain: true when it ran this tick itself (an ambush, a hold, a latch, a beat, a forced next row, cornered). */
	static bool TickWild(AWRCreatureAI* AI, AWRCreature* Self, float DeltaSeconds);
	/** Tame brain with nothing to fight: true when it ran this tick (a Lanterntoad in the pouch). */
	static bool TickTame(AWRCreatureAI* AI, AWRCreature* Self, float DeltaSeconds);
	/** May it pick this row now (only from water, only flanking, only grounded, only once a fight, only as a read)? */
	static bool RowAllowed(const AWRCreatureAI* AI, const AWRCreature* Self, const FWRAttackRow& Row, const AWRCharacterBase* Target, bool bReading);
	static void RowStarted(AWRCreatureAI* AI, AWRCreature* Self, const FWRAttackRow& Row, AWRCharacterBase* Target);
	/** A row's strike: true when this took it over (a dive, a flop, a spit, a call, a clatter, a puff, a surge). */
	static bool StrikeRow(AWRCreatureAI* AI, AWRCreature* Self, const FWRAttackRow& Row, AWRCharacterBase* Target);
	/** Each tick of a row's strike: the moves the generic shapes don't cover (the cry's cone, the tongue, the flurry,
	 *  the flop's landing ring, the surge's rise). */
	static void TickStrike(AWRCreatureAI* AI, AWRCreature* Self, const FWRAttackRow& Row, float DeltaSeconds);
	/** How a row's blow came out (from ApplyHit's resolution): the catches (latch, death roll, tongue, gulp). */
	static void Resolved(AWRCreature* Self, AWRCharacterBase* Victim, FName RowId, EWRHitResult Result);
	/** A Hexcrow down on the ground (no climbing back up until it's up). */
	static bool Grounded(const AWRCreature* Self);
	/** Damage on a swamp body before it lands (the Murkjaw's belly when it rolls or rears, the Slimeshell's shell). */
	static float Incoming(const AWRCreature* Self, const FWRHit& Hit, float Damage);
	/** After damage: freeing a held survivor, knocking a leech off, the clatter, the shell, the cry cancelled. */
	static void Damaged(AWRCreature* Self, const FWRHit& Hit, float Damage);
	/** At its death (server): lets go of whatever it held; the Red Tide's extra sacs. */
	static void Died(AWRCreature* Self, AActor* Killer);
	/** Its row blows (RowHit): a hexed beast's curse. */
	static void ModifyHit(const AWRCreature* Self, FWRHit& Hit, const FWRAttackRow* Row);
	/** The Hex-Warden's aura (WRSwampBosses calls it): Creature is hexed for Seconds (garnet eyes, curse on its blows,
	 *  aggro range x1.3); Seconds <= 0 lifts it. Server. */
	static void SetHexed(AActor* Creature, float Seconds);
	/** The Red Tide's leech boil (WRSwampSystems calls it): Count latch leeches come up out of the water at Onto's feet
	 *  and go for it. Server. Returns how many came. */
	static int32 SpawnLeechSwarm(AWRCharacterBase* Onto, int32 Count);
	/** E on a wild swamp creature: the Murkjaw blood rite. True when it took the press (Message for the player). */
	static bool Feed(AWRCreature* Self, AWRPlayerController* Player, FString& Message);
	/** A wading body's step (Mudhulk 2.5 m, Mirestrider 1.4 m): true when this decided it (Out: the step to take). */
	static bool Wade(const AWRCreature* Self, const FVector& Direction, FVector& Out);
	/** A wader standing in water no deeper than its own depth (it walks the bottom there: no swimming for the shore). */
	static bool WithinWade(const AWRCreature* Self);
	/** Either machine: the blood rite is open to Who on Self now (E; the Swamp prompt and the taming E read it). OutVerb
	 *  is the prompt's verb ("Blood rite: bleed into its jaws (5 HP/s, 4 s)"). */
	static bool RitePrompt(const AWRCreature* Self, const AActor* Who, FString& OutVerb);
	/** Server: the species' signature call (cook 2.15: the bellow before the burst, the Y-click, the curse caw...). */
	static void Signature(AWRCreature* Self);

	// --- The body's server life (UWRSwampBody::TickComponent) ---
	static void TickBody(AWRCreature* Self, UWRSwampBody* Body, float DeltaTime);
	/** The flags a row shows while it plays (the throat pumping, the hackles, the gape, the sac), from the brain's row. */
	static void SyncFlags(AWRCreature* Self, UWRSwampBody* Body);
	/** Ends its current row's strike or punish After seconds from now (a catch took over). */
	static void CutRow(AWRCreature* Self, float After);
	/** The holds: a death roll / gulp on Victim; letting go (bThrow: spat out / tossed and knocked down). */
	static void StartHold(AWRCreature* Self, UWRSwampBody* Body, AWRCharacterBase* Victim, FName Kind);
	static void Release(AWRCreature* Self, UWRSwampBody* Body, bool bThrow);
	/** A leech onto a limb of Victim (false: no free limb); off again (bKnocked: thrown clear and stunned a moment). */
	static bool StartLatch(AWRCreature* Self, UWRSwampBody* Body, AWRCharacterBase* Victim);
	static void Unlatch(AWRCreature* Self, UWRSwampBody* Body, bool bKnocked);
	/** The Mirestrider herd turns on By together (the clatter). */
	static void Rally(AWRCreature* Self, AActor* By);
};

/** The grabs and latches as the rest of the game sees them (WRCharacter's mash, the movement speed). */
namespace WRSwampGrip
{
	/** Held in a swamp creature's jaws or mouth (death roll, gulp): presses mash out of it. */
	WILDREACH_API bool IsHeld(const AActor* Who);
	/** Server: a mash press from a held survivor (-0.15 s, never below the minimum). */
	WILDREACH_API void Mash(AActor* Who);
	/** Movement speed x for a body with leeches on it (both machines read the replicated tag). */
	WILDREACH_API float MoveScale(const AActor* Who);
}

/** An owner-only marker over something a tame Bogsnout found (only its owner's machine gets it). */
UCLASS()
class WILDREACH_API AWRSwampMark : public AActor
{
	GENERATED_BODY()

public:
	AWRSwampMark();
	virtual void BeginPlay() override;
	static AWRSwampMark* Spawn(APlayerController* For, const FVector& At, float Seconds);

private:
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Fx;
};

/** Server: the land's living-world beats (cooks/biome-swamp.md 2.12.3). */
UCLASS()
class WILDREACH_API UWRSwampLife : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** A mud glob in flight (its landing slows whoever it struck). */
	void TrackSpit(class AWRProjectile* Glob, AWRCreature* By);

private:
	void Step();
	void BeatLeeches(float Hour);
	void BeatMurder();
	void BeatEdge(float Hour);
	void BeatHerds(float Hour);
	void BeatTide();
	void OnSalted(AWRCharacterBase* Target, AActor* Instigator, const FVector& At);
	UFUNCTION() void OnSpitGone(AActor* Glob);
	FTimerHandle StepTimer;
	FDelegateHandle SaltHandle;
	double NextTideAt = 0.0;
	TArray<TWeakObjectPtr<AWRCreature>> TideLeeches;
	double NextLeechAt = 0.0;
	double NextEdgeAt = 0.0;
	TMap<TWeakObjectPtr<AActor>, TWeakObjectPtr<AWRCreature>> Spits;
};
