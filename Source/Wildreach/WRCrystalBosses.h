#pragma once

// The Crystal caves' bosses and their arenas (cooks/biome-crystal.md 2.13, 2.14; cooks/build-biome-crystal.md Part 2
// P2.3-P2.7): the Mirrored Twins in the Archive of Wards (one real knight and the Spar Lens's double; only the real one
// has a reflection in the water; the lens turns every 12 s and they trade places; three shots on the flaring lens
// shatter it and the magazine lowers a spare), the Echo Warden in the Hall of the Door (it goes to the loudest noise;
// a struck chime sends it to hammer the chime with its back to you), the Geode Tyrant on its round of the deep halls
// (plates that throw shots back, chandeliers and selenite beams), and Lyrathis in the Prism Hall (her beam through the
// prisms on their turntables; the dome broken by her own split beam; the copies on the prisms' lines).
//
// Their moves are rows in Data/attacks.json like every boss's; this file holds:
//  - AWRCrystalBossProp: the arena machines with state, drawn on every machine from replicated numbers (the Spar Lens on
//    its gimbal, the lens magazine on its chain and counterweight, the Warden's chime stones, the Prism Hall's eight
//    turntables, a Glint lance standing in the floor).
//  - AWRPrismBeam: one sweep of Lyrathis's light through the prisms, the same rays traced on every machine.
//  - UWRCrystalBossBody: each boss's code-built parts over its rig and the state it shows.
//  - FWRCrystalBoss: the hooks AWRBoss calls for these four, beside the Graveyard's.
//  - UWRCrystalArenas: places the bosses, their machines, the run-back stones, Lyrathis's altar and summoning stone.
// Numbers are tuning bosses.<id>.*. Every blow goes through UWRCombatLibrary::ApplyHit on the server.

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRCombatTypes.h"
#include "WRCrystalBosses.generated.h"

class AWRBoss;
class AWRCreature;
class AWRCharacterBase;
class USphereComponent;
class UBoxComponent;
class UPointLightComponent;
class UProceduralMeshComponent;
class UPoseableMeshComponent;
class UMaterialInstanceDynamic;
struct FWRBossAttack;
struct FWRCrystalBossState;

/**
 * A Crystal arena machine (server-owned, replicated; every moving part a pure function of the server clock and a
 * replicated start time).
 *  lens       the Spar Lens: a 1.5 m disc of doubling spar in a bronze gimbal (a yoke on a vertical pivot under the light
 *             vent, an outer ring on a horizontal pivot) turned a quarter by a chain over a sprocket every swap (TurnAt,
 *             Clicks = quarters turned). State 0 whole, 1 shattered (its glass on the water), 2 none left.
 *  magazine   the lens magazine on the Archive's north wall: three cradles on a bronze frame, a chain over a ceiling
 *             pulley to a counterweight in a slot cut in the wall. Value = spares left; LowerAt: a spare's 20 s descent
 *             down the chain and along the ceiling track to the yoke.
 *  chime      one of the Hall of the Door's four chime stones (a 2 m crystal bell on a bronze post and arm): TurnAt the
 *             last strike (it swings, damped), Value hammerings taken, State 0 whole, 1 cracked, 2 broken.
 *  turntable  a Prism Hall prism on its bronze turntable: Clicks x 45 degrees past its home facing, TurnAt the last click
 *             (the disc eases 0.8 s; the lever swings and the pawl drops home).
 *  lance      a Glint lance standing in the Prism Hall floor (a small prism for 20 s; 3 hits shatter it).
 */
UCLASS(NotPlaceable)
class WILDREACH_API AWRCrystalBossProp : public AActor
{
	GENERATED_BODY()

public:
	AWRCrystalBossProp();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	UPROPERTY(ReplicatedUsing=OnRep_Look) FName Kind;
	UPROPERTY(ReplicatedUsing=OnRep_Look) uint8 State = 0;
	UPROPERTY(Replicated) int32 Index = 0;
	UPROPERTY(Replicated) int32 Clicks = 0;
	UPROPERTY(Replicated) float TurnAt = -100.f;
	UPROPERTY(Replicated) float LowerAt = -100.f;
	UPROPERTY(Replicated) float Value = 0.f;
	/** Size: the lens's span, the chime's height, the prism's height, the hall's floor (the magazine's ceiling height). */
	UPROPERTY(ReplicatedUsing=OnRep_Look) float Size = 100.f;

	// Server.
	TWeakObjectPtr<AWRBoss> OwnerBoss;
	float Health = 0.f;
	int32 Hits = 0;
	double ReadyAt = 0.0;
	/** Server: a blow or a shot reached it. True when it took it. */
	bool TakeHit(AWRCharacterBase* By, float Damage, bool bProjectile, const FVector& At);
	void SetState(uint8 NewState);
	float ServerNow() const;
	/** A turntable's prism facing now (degrees, world), eased over its click; the lens's yaw now. */
	float FacingNow(float Now) const;
	/** Where it can be struck (the lens's centre, the chime's bell, the turntable's hand-plate, the lance's middle). */
	FVector HitPoint() const;
	/** A turntable's prism centre (the light enters and leaves here). */
	FVector PrismCentre() const;
	static TArray<TWeakObjectPtr<AWRCrystalBossProp>>& All();

private:
	UFUNCTION() void OnRep_Look();
	void Build();
	void TickLook(float Now, float Dt);
	UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> HitShape;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Solid;
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> LookComps;
	/** Moving parts: the yoke and ring and lens (gimbal), the spare lens and counterweight (magazine), the bell (chime),
	 *  the disc + prism, the lever, the pinion (turntable). */
	UPROPERTY(Transient) TObjectPtr<USceneComponent> PartA;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> PartB;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> PartC;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> PartD;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> Glow;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> Glows;
	FName BuiltKind;
	uint8 BuiltState = 255;
	int32 ShownClicks = -1;
	float ShownTurnAt = -100.f;
};

/**
 * One sweep of Lyrathis's light (cooks/biome-crystal.md 2.14.2): from her brow gem to a prism (a pale guide thread through
 * the windup), then three rays fanned out of the prism's far face sweeping across the active time, each traced against the
 * hall (walls, pillars, prisms, the dais) up to its range. Every machine draws the same rays from the replicated prism
 * index, the start times and the sweep; the server deals 35 a contact every 0.5 s along them, and a ray on her dome
 * breaks it. Phase 3's turning beam is Kind 1 (one ray round her through every prism).
 */
UCLASS(NotPlaceable)
class WILDREACH_API AWRPrismBeam : public AActor
{
	GENERATED_BODY()

public:
	AWRPrismBeam();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(Replicated) TObjectPtr<AActor> Source;
	UPROPERTY(Replicated) TObjectPtr<AWRCrystalBossProp> Prism;
	/** 0 a fan through Prism, 1 phase 3's turning beam. */
	UPROPERTY(Replicated) uint8 Kind = 0;
	UPROPERTY(Replicated) float ThreadAt = 0.f;
	UPROPERTY(Replicated) float FireAt = 0.f;
	UPROPERTY(Replicated) float EndAt = 0.f;
	/** The fan's turn over its active time (degrees, signed: which way it sweeps). */
	UPROPERTY(Replicated) float Sweep = 40.f;
	/** The centre ray's bend toward the furthest tame (degrees; tame-punish), fixed at the windup. */
	UPROPERTY(Replicated) float Bias = 0.f;

	/** The rays now (world segments), on any machine. */
	void Rays(float Now, TArray<TPair<FVector, FVector>>& Out, TArray<int32>* OutStruckProp = nullptr) const;
	float ServerNow() const;
	// Server.
	float Damage = 35.f;
	float Mark = 30.f;
	TMap<TWeakObjectPtr<AActor>, double> HitAt;

private:
	void Draw(float Now);
	void Strike(float Now, float Dt);
	UPROPERTY(Transient) TArray<TObjectPtr<UProceduralMeshComponent>> Shafts;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> Mat;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> Light;
};

/** Each Crystal boss's look over its rig and the state every machine draws from (replicated). */
UCLASS(ClassGroup=(Creature))
class WILDREACH_API UWRCrystalBossBody : public UActorComponent
{
	GENERATED_BODY()

	struct FPart { TWeakObjectPtr<USceneComponent> Comp; int32 Tag = 0; FRotator BaseRot = FRotator::ZeroRotator; FVector BaseLoc = FVector::ZeroVector; };
	TArray<FPart> Parts;

public:
	UWRCrystalBossBody();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	static UWRCrystalBossBody* Get(const AActor* Actor);
	static void Attach(AWRCreature* Boss);

	/** A pose its parts take: the Twins 1 Ward guard (the hexafoil on the shield); the Warden 1 hammering, 2 listening;
	 *  the Tyrant 1 resting in the Wallow; Lyrathis 1 kneeling (the ward), 2 the Flare, 3 on the dais (phase 3). */
	UPROPERTY(Replicated) uint8 Pose = 0;
	/** The Tyrant: plate rows broken (bits 0-5). Lyrathis: blades broken (bit 0 left, bit 1 right). */
	UPROPERTY(Replicated) uint8 Broken = 0;
	/** The Warden: the noise its horns turn to. Lyrathis: her dome's centre (Z < -1e6 when none). */
	UPROPERTY(Replicated) FVector Point = FVector(0.f, 0.f, -1.e7f);
	/** Lyrathis's dome HP share (0..1), the brow gem's glow (0..1, the beam's windup). */
	UPROPERTY(Replicated) float Level = 0.f;
	UPROPERTY(Replicated) float Gem = 0.f;
	/** The Twins: the real knight's reflection is drawn (true on the boss; the double never has one). */
	UPROPERTY(Replicated) bool bMirrored = false;
	/** Every machine: run Fn on each built part carrying Tag (Lyrathis's gem, heart, blades, dome...). */
	void ForTagged(int32 Tag, TFunctionRef<void(USceneComponent*)> Fn) const { for (const FPart& P : Parts) if (P.Tag == Tag && P.Comp.IsValid()) Fn(P.Comp.Get()); }

private:
	void BuildLook();
	void ClearLook();
	void ApplyTint();
	USceneComponent* AddPart(FName Bone, const FTransform& Local, struct FWRToolMesh& M, bool bGlow, int32 Tag);
	FName BoneLike(const TCHAR* Hint) const;
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> LookComps;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> Glows;
	UPROPERTY(Transient) TObjectPtr<UPoseableMeshComponent> Reflection;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> Lamp;
	FName LookFor;
	int32 Tries = 0;
	float Clock = 0.f;
	float HornYaw = 0.f;
	uint8 ShownBroken = 255, ShownPose = 255;
	float RippleClock = 0.f;
};

/** The hooks AWRBoss calls for the Crystal caves' bosses (friend of AWRBoss), beside the Graveyard's. */
struct WILDREACH_API FWRCrystalBoss
{
	static bool IsCrystalBoss(const AWRBoss* Boss);
	static void Begin(AWRBoss* Boss);
	static void Reset(AWRBoss* Boss);
	static void EndPlay(AWRBoss* Boss);
	/** Not fighting: the Tyrant's round and rest, the Warden's walk of its hall. True: skip its own roam. */
	static bool TickIdle(AWRBoss* Boss, float DeltaSeconds, bool& bSkipWake);
	/** Fighting, before its attack machine: true when this ran the tick (the swap, the hammering, the dome...). */
	static bool Tick(AWRBoss* Boss, float DeltaSeconds);
	/** Who it goes for: the Warden the survivor nearest the loudest noise. */
	static AWRCharacterBase* PickTarget(AWRBoss* Boss, AWRCharacterBase* Default);
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
	/** A weapon swing or a projectile met an actor: true if it was a Crystal arena machine that took the blow. */
	static bool HitThing(AActor* Thing, UPrimitiveComponent* Comp, AWRCharacterBase* By, float Damage, bool bProjectile);
	/** The Twins' double (a hard-light knight): blows pass through it (WRCrystalCreatures' Incoming asks). */
	static bool IsDouble(const AActor* Who);
	/** Server: a creature died that a boss of these leads (the Geode Tyrant leading an invasion: nothing yet). */
	static bool ArenaOf(FName BossId, FVector& Centre, float& Radius, float& Yaw);
	/** The Geode Tyrant's round: its waypoints (world, the tubes' floors), built once. */
	static const TArray<FVector>& TyrantRoute();
	/** Server: three shots on the flaring Spar Lens shatter it (the magazine lowers a spare). */
	static void ShatterLens(AWRBoss* Boss, FWRCrystalBossState& St);
	/** Server: her own split beam on Lyrathis's dome (Amount of its HP). */
	static void DomeBurn(AWRBoss* Boss, float Amount);
	/** The prism (or Glint lance) nearest a point, for her beam and her copies' lines. */
	static AWRCrystalBossProp* NearestPrism(const UWorld* World, const FVector& At, const AWRCrystalBossProp* Except = nullptr);

private:
	static bool TickTwins(AWRBoss* Boss, FWRCrystalBossState& St, float Dt);
	static bool TickWarden(AWRBoss* Boss, FWRCrystalBossState& St, float Dt);
	static bool TickTyrant(AWRBoss* Boss, FWRCrystalBossState& St, float Dt);
	static bool TickLyrathis(AWRBoss* Boss, FWRCrystalBossState& St, float Dt);
	static bool Force(AWRBoss* Boss, FName Id, AWRCharacterBase* Target);
	static void CancelAttack(AWRBoss* Boss);
	static void StartSwap(AWRBoss* Boss, FWRCrystalBossState& St);
	static void DoubleStrike(AWRBoss* Boss, FWRCrystalBossState& St, const FWRBossAttack& Attack, float Dt);
	static void HammerChime(AWRBoss* Boss, FWRCrystalBossState& St);
	static void Lob(AWRBoss* Boss, const FVector& From, const FVector& To, float Speed, float Damage, float Splash, FName Kind);
	static void RaiseDome(AWRBoss* Boss, FWRCrystalBossState& St, bool bPermanent);
	static void BreakDome(AWRBoss* Boss, FWRCrystalBossState& St, const TCHAR* Why);
	static void FireBeam(AWRBoss* Boss, FWRCrystalBossState& St, AWRCrystalBossProp* Through, float Windup, float Active, float Sweep, float Bias);
	static void StepCopies(AWRBoss* Boss, FWRCrystalBossState& St, float Dt);
	static void Echo(AWRBoss* Boss, FWRCrystalBossState& St, FName AttackId);
	static void TickGrasp(AWRBoss* Boss, FWRCrystalBossState& St, float Dt);
	static void ReleaseGrasp(AWRBoss* Boss, FWRCrystalBossState& St, bool bThrow);
	static bool LyrathisAllowed(const AWRBoss* Boss, const FWRBossAttack& Attack, const AWRCharacterBase* Target);
	static void LyrathisStarted(AWRBoss* Boss, FWRCrystalBossState& St, FWRBossAttack& Attack, AWRCharacterBase* Target);
	static bool LyrathisStrike(AWRBoss* Boss, FWRCrystalBossState& St, FWRBossAttack& Attack);
	static float LyrathisIncoming(AWRBoss* Boss, FWRCrystalBossState& St, const FWRHit& Hit, float Damage);
	static void LyrathisLanded(AWRBoss* Boss, FWRCrystalBossState& St, AWRCharacterBase* Victim, FName AttackId, EWRHitResult Result);
};

/** Places the Crystal caves' bosses and machines (server), once the caves are built near a survivor. */
UCLASS()
class WILDREACH_API UWRCrystalArenas : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UWRCrystalArenas, STATGROUP_Tickables); }
	virtual bool IsTickable() const override;

private:
	bool Place(FName BossId);
	TSet<FName> Placed;
	float PlaceClock = 0.f;
};

// The Prism Hall's machines and Lyrathis's body (WRCrystalLyrathis.cpp), called from the look code (WRCrystalBossLook.cpp).
void WRCrystalBuildPrismProp(AWRCrystalBossProp* Prop, TArray<TObjectPtr<USceneComponent>>& Track, USceneComponent*& A, USceneComponent*& B, USceneComponent*& C, USceneComponent*& D, TArray<TObjectPtr<UMaterialInstanceDynamic>>& Glows);
void WRCrystalTickPrismProp(AWRCrystalBossProp* Prop, float Now, float Dt, USceneComponent* A, USceneComponent* B, USceneComponent* C, USceneComponent* D, const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Glows);
void WRCrystalBuildLyrathisBody(UWRCrystalBossBody* Body, AWRCreature* C, TFunctionRef<USceneComponent*(FName, const FTransform&, struct FWRToolMesh&, bool, int32)> Add, TFunctionRef<FName(const TCHAR*)> Bone);
void WRCrystalTickLyrathisBody(UWRCrystalBossBody* Body, AWRCreature* C, float Dt, float Clock);
