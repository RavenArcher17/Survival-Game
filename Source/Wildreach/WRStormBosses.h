#pragma once

// The Storm peaks bosses (cooks/biome-lightning.md 2.10-2.11, cooks/combat.md 2.17): the Bellwether (roaming, with his
// herd, the bell), Galvara the Kite Queen (her banner cloud's three thunderhead cores, the rain of strikes, the drop and
// the retaken sky), Coilback (under the Copperfall: the soil ridge, ring pulses along the copper veins, metal sense, the
// frost-slowed tunnel, the star nose) and Kharvox the Storm Roc (his Charge on his feathers, bleed and curse, the twelve
// rim rods, called strikes and rod shadow, the bone drop, the Thunderclap, the hidden eye of the storm).
//
// Their moves are rows in Data/attacks.json like every boss's; what's new is FWRStormBoss, the hooks AWRBoss calls
// (friend) for these four, the arena's props (AWRStormProp: rim rods, the nest, the ossuary slab, the bronze rod-altar,
// the Copperfall's copper veins) and UWRStormArenas, which places the arena bosses, their props and the altar at runtime
// from Data/storm_peaks.json "arenas" (once per world, when the ground there is in).

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRStormTypes.h"
#include "WRCreature.h"
#include "WRCombatTypes.h"
#include "WRStormBosses.generated.h"

class AWRBoss;
class AWRCreatureAI;
class AWRCharacterBase;
class UWRStormBody;
struct FWRBossAttack;
struct FWRStormBossState;

/** A piece of a Storm peaks arena: Kharvox's rim rods, his nest, the ossuary slab, the rod-altar, the Copperfall veins. */
UCLASS()
class WILDREACH_API AWRStormProp : public AActor
{
	GENERATED_BODY()

public:
	AWRStormProp();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

	/** rim_rod, nest, ossuary, altar_rod, veins. */
	UPROPERTY(ReplicatedUsing=OnRep_Look) FName Kind;
	/** A rim rod: 0 standing, 1 its tip lit (St Elmo's fire), 2 melted down, 3 knocked down (the stump stands). */
	UPROPERTY(ReplicatedUsing=OnRep_Look) uint8 State = 0;
	/** Size: a rod's height, the nest's radius, the veins' reach (cm). */
	UPROPERTY(ReplicatedUsing=OnRep_Look) float Size = 800.f;

	/** Server. */
	void SetState(uint8 NewState);
	bool IsStanding() const { return Kind == TEXT("rim_rod") && (State == 0 || State == 1); }
	FVector Tip() const { return GetActorLocation() + FVector(0.f, 0.f, Size); }
	/** Strikes this rod has taken this fight (it melts at bosses.kharvox.rodStrikesToMelt). */
	int32 Strikes = 0;
	/** Every machine: the veins light up round a ring pulse (Coilback). */
	UFUNCTION(NetMulticast, Unreliable) void MulticastPulse(FVector_NetQuantize Centre, float Seconds);
	/** Every machine: an arc jumps from this rod's tip to Other's (the rod chain). */
	UFUNCTION(NetMulticast, Unreliable) void MulticastArc(FVector_NetQuantize To, float Seconds);

private:
	UFUNCTION() void OnRep_Look();
	void Build();
	void SetCollision();
	int32 RodHandle = INDEX_NONE;
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> LookComps;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> GlowRoot;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> TipFx;
	UPROPERTY(VisibleAnywhere) TObjectPtr<class UCapsuleComponent> Shaft;
	UPROPERTY(Transient) TArray<TObjectPtr<class UBoxComponent>> Boxes;
	double PulseUntil = 0.0;
	double ArcUntil = 0.0;
	FVector ArcTo = FVector::ZeroVector;
	float ArcClock = 0.f;
	FName BuiltKind;
	uint8 BuiltState = 255;
};

/** The hooks AWRBoss calls for the Storm peaks bosses (friend of AWRBoss and AWRCreatureAI). */
struct WILDREACH_API FWRStormBoss
{
	static bool IsStormBoss(const AWRBoss* Boss);
	static void Begin(AWRBoss* Boss);
	static void Reset(AWRBoss* Boss);
	static void EndPlay(AWRBoss* Boss);
	/** Not fighting: the Bellwether's herd, rest at the fold and sleep by night, Galvara's cores. True: skip its roam
	 *  (bSkipWake: and its wake check: asleep, nobody close enough to wake him). */
	static bool TickIdle(AWRBoss* Boss, float DeltaSeconds, bool& bSkipWake);
	/** Fighting, before its attack machine: true when this ran the tick (a grab's climb, Galvara's drop and retake). */
	static bool Tick(AWRBoss* Boss, float DeltaSeconds);
	/** Who it goes for: Coilback the one wearing the most metal. */
	static AWRCharacterBase* PickTarget(AWRBoss* Boss, AWRCharacterBase* Nearest);
	static bool Allowed(const AWRBoss* Boss, const FWRBossAttack& Attack, const AWRCharacterBase* Target);
	static void AttackStarted(AWRBoss* Boss, FWRBossAttack& Attack, AWRCharacterBase* Target);
	/** Its wind-up (a dive's climb and fold, the eye bolt's aim lock and leader): true when this ran it. */
	static bool TickWindup(AWRBoss* Boss, FWRBossAttack& Attack, float DeltaSeconds);
	static bool BeginStrike(AWRBoss* Boss, FWRBossAttack& Attack);
	static bool TickStrike(AWRBoss* Boss, FWRBossAttack& Attack, float DeltaSeconds);
	static void EndStrike(AWRBoss* Boss, const FWRBossAttack& Attack);
	static void ModifyHit(const AWRBoss* Boss, const FWRBossAttack& Attack, FWRHit& Hit);
	static void HitLanded(AWRBoss* Boss, AWRCharacterBase* Victim, FName AttackId, EWRHitResult Result);
	static float Incoming(AWRBoss* Boss, const FWRHit& Hit, float Damage);
	/** Weak to Blood and curse (x1.5), not to the opposite element of fire / frost. */
	static bool OwnWeakness(const AWRBoss* Boss);
	static void Damaged(AWRBoss* Boss, const FWRHit& Hit, float Damage);
	static void PartBroken(AWRBoss* Boss, const FWRCreatureDef::FBodyPoint& Part, AActor* By);
	/** A charge into rock: stuck (the Bellwether: 5 s, x1.5 damage, x2 stance). */
	static void WallStun(AWRBoss* Boss, float Seconds);
	static void PhaseChanged(AWRBoss* Boss, int32 NewPhase);
	/** Its shrine-stone at the arena's data spot (storm_peaks.json arenas.<id>.runback). True: handled. */
	static bool PlaceRunback(AWRBoss* Boss);
	/** Its boss part (Kharvox: the Stormheart). */
	static FName BossPart(const AWRBoss* Boss, bool bScarred, FName Fallback);
	static void Died(AWRBoss* Boss);
	/** Any strike landed (server): Kharvox +10 Charge for his own, his rods count theirs. */
	static void StrikeLanded(UWorld* World, const FWRStrikeCall& Call);
	/** WRCombatLibrary ApplyHit's one-line hook: stance taken x (the Bellwether stuck x2), and the bone drop's head stance. */
	static float StanceTaken(AWRCharacterBase* Target, const FWRHit& Hit, float Stance);

	// --- Data (Data/storm_peaks.json) ---
	static bool SiteAt(const UWorld* World, FName SiteId, FVector& Out);
	static bool NearestKit(const UWorld* World, FName Kind, const FVector& Near, float Range, FVector& Out);

	/** A living-world beat on a creature (the Rams' Ring clash, a kite's snatch): true while it runs. */
	static bool TickBeat(AWRCreatureAI* AI, AWRCreature* Self, UWRStormBody* Body, float DeltaSeconds);

private:
	static void SpawnCores(AWRBoss* Boss, int32 Count, float Health);
	static void OrbitCores(AWRBoss* Boss);
	static void Surface(AWRBoss* Boss, const FVector& At);
	static void ReleaseCarried(AWRBoss* Boss, bool bDrop);
	static bool TickGalvara(AWRBoss* Boss, FWRStormBossState& St, UWRStormBody* Body, float DeltaSeconds);
	static bool TickCoilback(AWRBoss* Boss, FWRStormBossState& St, UWRStormBody* Body, float DeltaSeconds);
	static bool TickKharvox(AWRBoss* Boss, FWRStormBossState& St, UWRStormBody* Body, float DeltaSeconds);
	static FVector EyePoint(const AWRBoss* Boss);
	static FVector BoltEnd(const AWRBoss* Boss, const FVector& From, const FVector& Aim);
	static void Stroke(AWRBoss* Boss, const FVector& From, const FVector& To);
};

/** Server: places the Storm peaks arena bosses, their props and Kharvox's altar from Data/storm_peaks.json, once a world. */
UCLASS()
class WILDREACH_API UWRStormArenas : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

private:
	void Step();
	bool Place(FName Arena, const TSharedPtr<class FJsonObject>& Row);
	TSet<FName> Placed;
	FTimerHandle StepTimer;
};
