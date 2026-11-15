#pragma once

// The Corrupted wasteland's bosses (cooks/biome-void.md 2.13-2.14; cooks/build-biome-void.md 2.14-2.15): the four
// Gauntlet stations and the Echo, and Vorhollow on the Choir's last stage. Their moves are attacks.json boss rows; what
// they do beyond the boss machine (AWRBoss) is here, through the same hook points the Storm peaks and the Swamp use:
//  - the Unravelled: body hits x0.3; it unravels into a whirling column, travels, and its core hangs alone (x3) before
//    the pieces snap shut (Reform slam);
//  - the Mother of Rifts: brood rifts in her six pens (Gnawers, Hollowfangs); closing them all stuns her;
//  - the Starving One: every blow that lands eats a buff and it grows; food dropped near it is a bait it can't refuse;
//  - the Null Warden: a null field (spells fizzle, projectiles drop), the bell's toll; his key holds the Null Gate;
//  - the Echo: a 2.4 km loop by night, rests on the Glass Sea by day; mimics your last path ability, fails if you vary;
//  - Vorhollow: Inhale, the Rift beam, the lamprey lunge's swallow, the Hollow (its shell; voice pillars crack it), the
//    Gnashing that snaps the chains and splits the island (AWRVorhollowStage: seven pieces on the server clock), Rift
//    dive, the twin beam; hidden phase 3 (artifacts): the Gullet.
// Server authority throughout; the stage's pieces, beams, chains and the gate move by replicated clocks.

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WRVoidBosses.generated.h"

class AWRBoss;
class AWRCharacterBase;
class AWRCreature;
class AWRLand;
class AWRPlayerController;
class UBoxComponent;
class UStaticMeshComponent;
struct FWRBossAttack;
struct FWRHit;
enum class EWRHitResult : uint8;

struct WILDREACH_API FWRVoidBoss
{
	static bool IsVoidBoss(const AWRBoss* Boss);
	static void Begin(AWRBoss* Boss);
	static void EndPlay(AWRBoss* Boss);
	/** Out of a fight: the Echo walks its loop (true: it moved itself this tick). */
	static bool TickIdle(AWRBoss* Boss, float DeltaSeconds);
	/** In a fight: the unravel, the brood, the bait, the field, the shell, the Gullet (true: it ran this tick itself). */
	static bool Tick(AWRBoss* Boss, float DeltaSeconds);
	static bool Allowed(const AWRBoss* Boss, const FWRBossAttack& Attack, const AWRCharacterBase* Target);
	static void AttackStarted(AWRBoss* Boss, FWRBossAttack& Attack, AWRCharacterBase* Target);
	static bool BeginStrike(AWRBoss* Boss, FWRBossAttack& Attack);
	static bool TickStrike(AWRBoss* Boss, FWRBossAttack& Attack, float DeltaSeconds);
	static void EndStrike(AWRBoss* Boss, const FWRBossAttack& Attack);
	static void ModifyHit(const AWRBoss* Boss, const FWRBossAttack& Attack, FWRHit& Hit);
	static void HitLanded(AWRBoss* Boss, AWRCharacterBase* Victim, FName AttackId, EWRHitResult Result);
	static float Incoming(AWRBoss* Boss, const FWRHit& Hit, float Damage);
	/** A void boss has its own weakness (Arcane, not built: none of fire / frost). */
	static bool OwnWeakness(const AWRBoss* Boss);
	static void Damaged(AWRBoss* Boss, const FWRHit& Hit, float Damage);
	static void PhaseChanged(AWRBoss* Boss, int32 NewPhase);
	static void Died(AWRBoss* Boss);
	/** Stance x on a void boss (the Unravelled's open core, the Mother's strain, the Starving One eating, the Echo's fail). */
	static float StanceTaken(AWRCharacterBase* Target, const FWRHit& Hit, float Stance);
	/** A helper body died (a brood rift closed). */
	static void HelperDied(AWRCreature* Helper);

	/** Server, once the land's places exist: the arenas, the Null Gate, the stage, the bosses' spawners. */
	static void PlaceArenas(AWRLand* Land);
	/** The Null Warden's field covers this point (spells fizzle there). */
	static bool InNullField(const UWorld* World, const FVector& At);
	/** The Echo hears every path ability that hits it (the mimic's memory). */
	static void NoteAbility(AWRBoss* Boss, const FWRHit& Hit);
	/** One of the boss's own rows (to force it: the reform slam, the Hollow). */
	static FWRBossAttack* FindAttack(AWRBoss* Boss, FName Id);
	/** A player swinging something heavy right now (a heavy, a charged blow or a Wildbrand): a voice pillar rings to it. */
	static bool HeavySwing(const AWRCharacterBase* Who);
};

/**
 * The Null Gate (biome-void 2.13.4): an iron portcullis 8 m wide hung on two chains that run up over iron pulleys to two
 * counterweight stones in stone shafts either side; the chains pinned by a lock bar through the Warden's key socket.
 * When he dies the key drops into the socket, the bar slides, the counterweights fall 4 m over 6 s (seen through slit
 * windows, the chains running over the pulleys) and the portcullis rises; when he comes back he re-pins it and it lowers.
 * From the bridge side a crank winch (hold E 8 s) raises it. Everything moves on the replicated clock (never snapping).
 */
UCLASS(NotPlaceable)
class WILDREACH_API AWRNullGate : public AActor
{
	GENERATED_BODY()

public:
	AWRNullGate();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;
	/** Server: raise (true) or lower over the gate's seconds from where it is now. */
	void SetOpen(bool bOpen);
	/** 0 shut .. 1 open, now (every machine). */
	float Openness() const;
	static AWRNullGate* Find(const UWorld* World);
	/** The crank's E (the bridge side). */
	bool CrankNear(const FVector& At) const;
	void Crank(AWRPlayerController* By);

	UPROPERTY(Replicated) bool bOpen = false;
	UPROPERTY(Replicated) double ChangedAt = -100.0;
	UPROPERTY(Replicated) float FromOpenness = 0.f;

protected:
	virtual void BeginPlay() override;

private:
	void Build();
	double Now() const;
	UPROPERTY() TObjectPtr<USceneComponent> Portcullis;
	UPROPERTY() TObjectPtr<UBoxComponent> Blocker;
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> Weights;
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> Pulleys;
	TArray<TPair<FVector, FVector>> ChainEnds;   // (pulley top, weight top) per side, for the running links
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> Links;
	TMap<TWeakObjectPtr<AActor>, double> CrankStarted;
};

/** Vorhollow's arena (biome-void 2.14.1): the island on its keel, the four chains to the anchor stones, the chain bridge,
 *  the six voice pillars, the beams and the Gullet. Split clock, pillar rings, wards, beam and Gullet state replicate. */
UCLASS(NotPlaceable)
class WILDREACH_API AWRVorhollowStage : public AActor
{
	GENERATED_BODY()

public:
	AWRVorhollowStage();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;
	static AWRVorhollowStage* Find(const UWorld* World);

	/** Server: the Gnashing landed: the chains snap and the island splits (0 = whole again). */
	void Split(bool bSplit);
	bool IsSplit() const { return SplitAt > 0.0; }
	/** Server: a heavy, charged or Wildbrand blow on a pillar: it rings for its seconds. */
	bool StrikePillar(const FVector& At, float Radius);
	/** Where pillar i stands now (world), and whether it rings. */
	FVector PillarAt(int32 i) const;
	bool PillarRinging(int32 i) const;
	/** A pillar stands between From and To (cover from the Inhale and the beam). */
	bool PillarBetween(const FVector& From, const FVector& To) const;
	/** Server: a beam from Origin sweeping Sweep degrees from Yaw0 at Rate deg/s (StopAt: the twin beam's pause). */
	void StartBeam(const FVector& Origin, float Yaw0, float Sweep, float Rate, float StopAt, float StopFor, float Length);
	void EndBeam();
	/** The beam's yaw now and whether it burns. */
	bool BeamNow(float& OutYaw) const;
	/** Server: ward a piece (anchor salt, 10 s). */
	void WardAt(const FVector& At, float Seconds);
	/** The Gullet (phase 3): open / where it is / its heart. */
	void OpenGullet(bool bOpen);
	FVector GulletCentre() const;
	FVector StageTop() const;

	UPROPERTY(Replicated) double SplitAt = 0.0;
	UPROPERTY(Replicated) TArray<double> RingUntil;
	UPROPERTY(Replicated) TArray<double> WardUntil;
	UPROPERTY(Replicated) double BeamStart = 0.0;
	UPROPERTY(Replicated) FVector_NetQuantize BeamOrigin;
	UPROPERTY(Replicated) float BeamYaw0 = 0.f;
	UPROPERTY(Replicated) float BeamSweep = 100.f;
	UPROPERTY(Replicated) float BeamRate = 50.f;
	UPROPERTY(Replicated) float BeamStopAt = -1.f;
	UPROPERTY(Replicated) float BeamStopFor = 0.f;
	UPROPERTY(Replicated) float BeamLength = 4000.f;
	UPROPERTY(Replicated) bool bGullet = false;
	float LipZ = 0.f;

protected:
	virtual void BeginPlay() override;

private:
	void Build();
	/** Piece i's place now (relative to the stage), from the split clock: whole, then easing apart onto its orbit. */
	FTransform PieceNow(int32 i, double T) const;
	double Now() const;
	struct FPiece { TWeakObjectPtr<USceneComponent> Root; float Angle = 0.f, Radius = 0.f, Gap = 200.f; bool bSmall = false; };
	TArray<FPiece> Pieces;
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> Chains;
	UPROPERTY() TObjectPtr<USceneComponent> Beam;
	UPROPERTY() TObjectPtr<USceneComponent> Gullet;
	TArray<int32> PillarPiece;
	TArray<FVector> PillarLocal;
	float GulletClock = 0.f, CoughClock = 0.f, BeamTick = 0.f, VoidClock = 0.f;
	TSet<TWeakObjectPtr<AActor>> InGullet;
	bool bChainsShown = true;
};
