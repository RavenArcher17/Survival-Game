#pragma once

// The Graveyard's bosses and their arenas (cooks/biome-graveyard.md 2.13, 2.14; cooks/build-biome-graveyard.md 2.15,
// 2.16; cooks/combat.md 2.17): the Bell-Ringer in the Belfry yard (cut the rope and his Toll stops raising the dead), the
// Crypt Mother in the Brood Gallery (burn the egg sacs, burn the young off her back), the Ossuary Lord in the Chandelier
// Hall (it re-forms from its pile unless roots, quicklime or its three keystones stop it; the chandelier comes down on
// it once), the Gravedigger on his night round (dig each other out, smash his lantern), and Nhal, the Last Mourner, in
// the roofless Chapel of Nhal (seal the twelve tombs with Nature or heartwood stakes, the Opening, every grave open, the
// hidden Procession).
//
// Their moves are rows in Data/attacks.json like every boss's; this file holds:
//  - AWRGraveBossProp: the arena pieces with state, one machine each, built on every machine that draws from replicated
//    numbers (the Belfry's bell, wheel and rope; an egg sac; the chandelier on its chain; a floor tomb; a thrown
//    headstone left as cover; a grave the Gravedigger dug; the Procession's ring of shades).
//  - UWRGraveBossBody: each boss's code-built look over its stand-in rig (bell helm and hand-bell, brood cloak, bone
//    giant and keystones, horse skull and mortsafe cage, moth-veil and lantern crown) and the state it shows.
//  - FWRGraveBoss: the hooks AWRBoss calls for these five, beside the forest's.
//  - UWRGraveArenas: places the bosses, their pieces, the run-back stones and the Necromancy altar; runs the held actions
//    (dig a friend out, drive a stake, pull a tame up) and the Gravedigger's round.
// Numbers are tuning bosses.<id>.*.

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRCreature.h"
#include "WRCombatTypes.h"
#include "WRGraveBosses.generated.h"

class AWRBoss;
class AWRCharacterBase;
class AWRPlayerController;
class USphereComponent;
class UBoxComponent;
class UPointLightComponent;
class USkeletalMeshComponent;
class UMaterialInstanceDynamic;
struct FWRBossAttack;
struct FWRLookBlock;
struct FWRGraveBossState;

/**
 * A Graveyard arena piece with state (server-owned, replicated, drawn on every machine from what replicates).
 *  belfry      the great bell, its wheel and headstock on the frame, and the rope down the shaft to the arch: the
 *              frayed section 4 m up is the Bell-Ringer's counter (400 HP). State 0 whole, 1 cut (the bell silent).
 *              It swings from TollAt (a toll's swing dying away) and always stirs a little in the wind.
 *  sac         a Crypt Mother's egg sac on a pillar or wall: 150 HP, fire x3, a torch held to it 2 s. 0 whole, 1 burnt.
 *  chandelier  the bone chandelier on its iron chain from the vault (the chain 300 HP). 0 hanging (it sways on the
 *              chain), 1 falling (from FellAt), 2 down.
 *  tomb        one of the chapel's twelve floor tombs: 0 closed, 1 open (slab cracked aside, grave-light), 2 sealed
 *              (roots through the cracks, white blossom).
 *  headstone   a stone the Bell-Ringer threw, standing where it landed (cover).
 *  dug         a grave the Gravedigger opened on his round (fresh earth, an open hole): gone at dawn.
 *  procession  Nhal's hidden third phase: forty shades with lantern poles walking a ring round the nave (gaps after
 *              every sixth), the ring closing and opening, the bell's toll turning them back.
 */
UCLASS(NotPlaceable)
class WILDREACH_API AWRGraveBossProp : public AActor
{
	GENERATED_BODY()

public:
	AWRGraveBossProp();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(ReplicatedUsing=OnRep_Look) FName Kind;
	UPROPERTY(ReplicatedUsing=OnRep_Look) uint8 State = 0;
	/** Which one (a tomb 0-11, a sac 0-7); a size (a sac's height, the chandelier's drop below the vault, cm). */
	UPROPERTY(Replicated) int32 Index = 0;
	UPROPERTY(ReplicatedUsing=OnRep_Look) float Size = 100.f;
	/** Server time: the bell's last toll; the chandelier's fall; the Procession's start; a shade ring's last turn. */
	UPROPERTY(Replicated) float TollAt = -100.f;
	UPROPERTY(Replicated) float FellAt = -100.f;
	UPROPERTY(Replicated) float StartAt = 0.f;
	UPROPERTY(Replicated) float TurnAt = 0.f;
	/** The Procession: its angle at the last turn (radians) and its way round (+1 / -1). */
	UPROPERTY(Replicated) float TurnAngle = 0.f;
	UPROPERTY(Replicated) float Dir = 1.f;

	/** Server. */
	float Health = 0.f;
	float MaxHealth = 0.f;
	TWeakObjectPtr<AWRBoss> OwnerBoss;
	/** A torch held to it (a sac): since when, by whom. */
	double TorchSince = 0.0;
	/** Server: a blow reached it. Returns true when it took it. */
	bool TakeHit(AWRCharacterBase* By, float Damage, const FWRHit* Hit, const FVector& At);
	void SetState(uint8 NewState);
	/** Where its hittable part is (the frayed rope, the sac's middle, the chain's middle). */
	FVector HitPoint() const;
	/** The Procession (every machine): the shade ring's radius and angle now, and shade i's place. */
	float RingRadius(float Now) const;
	float RingAngle(float Now) const;
	FVector ShadeAt(int32 i, float Now) const;
	static int32 ShadeCount() { return 40; }
	float ServerNow() const;

private:
	UFUNCTION() void OnRep_Look();
	void Build();
	void TickLook(float Now);
	UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> HitShape;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Solid;
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> LookComps;
	/** Moving parts: the bell + headstock (Swing), the wheel (Wheel), the rope's lower run (RopeLow), the chandelier
	 *  (Hang), the shades (Shades). */
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Swing;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Wheel;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> RopeLow;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Hang;
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> Shades;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> Glow;
	FName BuiltKind;
	uint8 BuiltState = 255;
	float Clock = 0.f;
};

/** Each Graveyard boss's look over its stand-in rig, and the state every machine draws from (replicated). */
UCLASS(ClassGroup=(Creature))
class WILDREACH_API UWRGraveBossBody : public UActorComponent
{
	GENERATED_BODY()

public:
	UWRGraveBossBody();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	static UWRGraveBossBody* Get(const AActor* Actor);
	static void Attach(AWRBoss* Boss);

	/** A pose its parts take: 0 none; Bell-Ringer 1 rope wrapped (the Toll), 2 hand-bell up; Crypt Mother 1 on the
	 *  ceiling, 2 shuddering (Brood shed); Ossuary Lord 1 collapsed (a pile), 2 the surge (a wave of bone), 3 ribs open;
	 *  Gravedigger 1 asleep, 2 digging, 3 cage open; Nhal 1 veil spread (Shroud), 2 kneeling (the Procession). */
	UPROPERTY(Replicated) uint8 Pose = 0;
	/** Bell-Ringer: the rope cut (the hand-bell off his belt and in his hand). Gravedigger: his lantern smashed. */
	UPROPERTY(Replicated) bool bBroken = false;
	/** Crypt Mother: the young still riding her back (about 20). Ossuary Lord: keystones smashed (bits). */
	UPROPERTY(Replicated) uint8 Count = 0;
	/** Nhal: 0 veiled, 1 the veil lifted (on the roof ribs, her face bare), 2 the jaw unhinged (phase 2), 3 the shade. */
	UPROPERTY(Replicated) uint8 Veil = 0;
	/** Gravedigger: who is in his cage (every machine draws them there). */
	UPROPERTY(Replicated) TObjectPtr<AActor> Caged;

private:
	void BuildLook();
	void ClearLook();
	void ApplyTint();
	USceneComponent* AddGroup(FName Bone, const FVector& WorldAt, const FRotator& WorldRot, const TArray<FWRLookBlock>& Blocks, int32 Kind);
	USkeletalMeshComponent* Skel() const;
	struct FGroup { TWeakObjectPtr<USceneComponent> Frame; int32 Kind = 0; FRotator BaseRot = FRotator::ZeroRotator; FVector BaseLoc = FVector::ZeroVector; float Phase = 0.f; };
	TArray<FGroup> Groups;
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> LookComps;
	UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> Lights;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> TintMats;
	FName LookFor;
	int32 LookTries = 0;
	float Clock = 0.f;
	uint8 ShownCount = 255, ShownVeil = 255, ShownPose = 255;
	bool bShownBroken = false;
	bool bMeshHidden = false;
	/** Nhal kneeling (cm the body has sunk); the Gravedigger's cage door (degrees open). */
	float Kneel = 0.f, KneelShown = 0.f, DoorAngle = 0.f;
};

/** The hooks AWRBoss calls for the Graveyard's bosses (friend of AWRBoss), beside the forest's. */
struct WILDREACH_API FWRGraveBoss
{
	static bool IsGraveBoss(const AWRBoss* Boss);
	static void Begin(AWRBoss* Boss);
	static void Reset(AWRBoss* Boss);
	static void EndPlay(AWRBoss* Boss);
	/** Not fighting: the Gravedigger's sleep by day and his round by night. True: skip its own roam. */
	static bool TickIdle(AWRBoss* Boss, float DeltaSeconds, bool& bSkipWake);
	/** Fighting, before its attack machine: true when this ran the tick (a hold, the pile, the ceiling, the surge). */
	static bool Tick(AWRBoss* Boss, float DeltaSeconds);
	static bool Allowed(const AWRBoss* Boss, const FWRBossAttack& Attack, const AWRCharacterBase* Target);
	static void AttackStarted(AWRBoss* Boss, FWRBossAttack& Attack, AWRCharacterBase* Target);
	static bool BeginStrike(AWRBoss* Boss, FWRBossAttack& Attack);
	static bool TickStrike(AWRBoss* Boss, FWRBossAttack& Attack, float DeltaSeconds);
	static void EndStrike(AWRBoss* Boss, const FWRBossAttack& Attack);
	static void ModifyHit(const AWRBoss* Boss, const FWRBossAttack& Attack, FWRHit& Hit);
	static void HitLanded(AWRBoss* Boss, AWRCharacterBase* Victim, FName AttackId, EWRHitResult Result);
	static float Incoming(AWRBoss* Boss, const FWRHit& Hit, float Damage);
	static bool OwnWeakness(const AWRBoss* Boss);
	static void Damaged(AWRBoss* Boss, const FWRHit& Hit, float Damage);
	static void PhaseChanged(AWRBoss* Boss, int32 NewPhase);
	static bool PlaceRunback(AWRBoss* Boss);
	static FName BossPart(const AWRBoss* Boss, bool bScarred, FName Fallback);
	static void Died(AWRBoss* Boss);
	/** A weapon swing or a projectile met an actor: true if it was a Graveyard arena piece that took the blow. */
	static bool HitThing(AActor* Thing, UPrimitiveComponent* Comp, AWRCharacterBase* By, float Damage, bool bFire);
	/** Server: the chandelier came down on the hall's floor: the Ossuary Lord under it takes the blow and a stance break. */
	static void ChandelierLanded(AWRGraveBossProp* Chandelier, const FVector& Floor);
	/** A lime pot broke (WRProjectile's limepot): on the Ossuary Lord's pile it counts toward stopping it re-forming. */
	static void LimeLanded(UWorld* World, const FVector& At, AActor* Thrower);
	/** A held survivor pressed a button (mash, server): a boss's hold or the Gravedigger's earth ends sooner. True when
	 *  they were held by one of these. */
	static bool Mash(AWRCharacterBase* Victim);
	/** Every machine: held by a Graveyard boss (attached to it) or buried under one of the Gravedigger's mounds. */
	static bool IsHeld(const AActor* Who);
	/** Server: Nature roots came up here (a Nature path field: snare, vines, sanctuary): open chapel tombs within the
	 *  seal reach close, and the Ossuary Lord's pile is bound. */
	static void NatureLanded(UWorld* World, const FVector& At, float Radius, AActor* By);
	/** Someone held by a Graveyard boss now (every machine: the hold's own pose). */
	static AWRBoss* HolderOf(const AActor* Victim);
	/** Server: a survivor buried by the Gravedigger, or a tame half dragged into Nhal's floor: who may be helped out. */
	static AWRCharacterBase* HelpableNear(const UWorld* World, const FVector& At, float Range, FName& OutHow);
	/** Server: they're out (dug out, pulled up). */
	static void Freed(AWRCharacterBase* Who);
	/** Server: an open chapel tomb within Range of At (a stake's target). */
	static AWRGraveBossProp* OpenTombNear(const UWorld* World, const FVector& At, float Range);
	/** Server: seal it (a Nature blow on it, a stake driven in). */
	static void SealTomb(AWRGraveBossProp* Tomb, AActor* By);
	/** The arenas' layout in world cm from Data/graveyard.json (the same on every machine). */
	static bool ArenaOf(FName BossId, FVector& Centre, float& Radius, float& Yaw);
	static TArray<TWeakObjectPtr<AWRGraveBossProp>>& Props();

private:
	static bool TickRinger(AWRBoss* Boss, FWRGraveBossState& St, float Dt);
	static bool TickMother(AWRBoss* Boss, FWRGraveBossState& St, float Dt);
	static bool TickLord(AWRBoss* Boss, FWRGraveBossState& St, float Dt);
	static bool TickDigger(AWRBoss* Boss, FWRGraveBossState& St, float Dt);
	static bool TickNhal(AWRBoss* Boss, FWRGraveBossState& St, float Dt);
	static void TickHold(AWRBoss* Boss, FWRGraveBossState& St, float Dt);
	static void TickBuried(AWRBoss* Boss, FWRGraveBossState& St, float Dt);
	static void Grab(AWRBoss* Boss, FWRGraveBossState& St, AWRCharacterBase* Victim, FName How, float Seconds, float Dps);
	static void ReleaseGrab(AWRBoss* Boss, FWRGraveBossState& St, bool bThrow);
	static bool Force(AWRBoss* Boss, FName Id, AWRCharacterBase* Target);
	static void CancelAttack(AWRBoss* Boss);
	static void Raise(AWRBoss* Boss, FWRGraveBossState& St, FName Id, const FVector& At);
	static void Lob(AWRBoss* Boss, const FVector& From, const FVector& To, float Speed, float Damage, float Splash, FName Kind, FName Buildup = NAME_None, float Amount = 0.f);
	static void Collapse(AWRBoss* Boss, FWRGraveBossState& St);
	static void Reform(AWRBoss* Boss, FWRGraveBossState& St);
	static void FinishLord(AWRBoss* Boss, FWRGraveBossState& St, const TCHAR* Why);
	static void StartProcession(AWRBoss* Boss, FWRGraveBossState& St);
};

/** Places the Graveyard's bosses and pieces (server), runs the held actions and the Gravedigger's dug graves. */
UCLASS()
class WILDREACH_API UWRGraveArenas : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UWRGraveArenas, STATGROUP_Tickables); }
	virtual bool IsTickable() const override;
	static UWRGraveArenas* Get(const UObject* WorldContext);

	/** Client: the E prompt for a held action in reach (dig out, pull up, drive a stake). */
	static bool Prompt(const AWRPlayerController* PC, FString& OutTarget, FString& OutVerb);
	/** Client: E pressed: true when it sent a command. */
	static bool ClientInteract(AWRPlayerController* PC);
	/** Server: "world grave boss <verb>". */
	FString Command(AWRPlayerController* PC, const TArray<FString>& Args);

	/** A held action under way (server). */
	struct FHeld { TWeakObjectPtr<AWRCharacterBase> Who; TWeakObjectPtr<AActor> Target; FName How; double Until = 0.0; FVector From = FVector::ZeroVector; };
	TArray<FHeld> Held;

private:
	bool Place(FName BossId);
	void StepHeld();
	TSet<FName> Placed;
	float PlaceClock = 0.f;
};
