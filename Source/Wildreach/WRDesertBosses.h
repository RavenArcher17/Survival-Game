#pragma once

// The Desert's bosses and their arenas (cooks/biome-desert.md 2.13-2.14; cooks/build-biome-desert.md 2.13-2.14):
//  - Ssarrok, the Dune Leviathan, on its route through the Erg (down to the strand's wet sand at dusk; resting at its
//    lair by day in a heat wave): fin wake, breach, the swallow (4 s inside, struggle, 400 to the throat frees you, else
//    spat out: 40 and blind 80), tail breach slam, geyser roar. Counter: firm ground. A burst or breach that ends on
//    anything but soft dry sand (hardpan, rock, wet sand: an Oasis water jar's splash, the strand's tide line) beaches
//    it 6 s, belly up (x1.3 taken, stance open).
//  - The Mirage Matriarch in the Sevenfold Wells (asleep in the deepest pool at night: no fight): three mirages that
//    cast no shadow (half her damage, one hit pops one, re-form 12 s; half as long under overcast); blind on the real
//    one breaks them all and stops new ones 15 s; the crossfire charge sends all four.
//  - The Glass Empress in the Glass Canyon: six fused-glass plates (pincers, two back plates, tail, bulb), 300 each,
//    x0.3 through a plate, strike blows x2 on a plate, frost then fire within 4 s shatters one (shards 15 in 4 m, her
//    too); the bulb gone ends the Prism beam (daylight only, rock blocks it) and the sting's blind.
//  - Ahrumet, the Sun Colossus, on his throne at the Sundial Plaza: a step onto the meridian strip wakes him (4 s to
//    stand). The eye beam sweep (the gnomon and the hour-stelae block it; three beams break a stela), stomps, the palm
//    sweep, the grab (300 to the held wrist frees you), the sand wave, the Gaze on tames; sun-hot stone after each beam
//    and pulse (6 s): a water hit (an Oasis jar, Tide; frost at half) cracks that part (+60 stance, x1.5 for the rest of
//    the fight). At 50% the Uprooting of the Gnomon (the dial's centre becomes a quicksand funnel): the club combo, the
//    Noon Pulse (80 and blind 100 to all not in shadow), the held beam, Glassfall. Hidden phase 3 with artifacts: the
//    Noon Lock (a beam turning round the plaza, a pulse every 20 s: only under him is shade).
// Moves are rows in Data/attacks.json (bosses.<id>); numbers in tuning bosses.<id>.

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRCombatTypes.h"
#include "WRDesertBosses.generated.h"

class AWRBoss;
class AWRCharacterBase;
class UStaticMeshComponent;
struct FWRBossAttack;

/** One of the Sundial Plaza's twelve hour-stelae: cover from the eye and the Noon Pulse; three beams break it. */
UCLASS(NotPlaceable)
class WILDREACH_API AWRDesertStela : public AActor
{
	GENERATED_BODY()

public:
	AWRDesertStela();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	UPROPERTY(ReplicatedUsing=OnRep_State) uint8 Broken = 0;
	int32 BeamHits = 0;
	void SetBroken(bool bBroken);
	static TArray<TWeakObjectPtr<AWRDesertStela>>& All();

private:
	UFUNCTION() void OnRep_State();
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Stone;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Rubble;
};

/** The hooks AWRBoss calls for the Desert's four (friend of AWRBoss), beside the Ocean's. */
struct WILDREACH_API FWRDesertBoss
{
	static bool IsDesertBoss(const AWRBoss* Boss);
	static void Begin(AWRBoss* Boss);
	static void Reset(AWRBoss* Boss);
	static void EndPlay(AWRBoss* Boss);
	static bool TickIdle(AWRBoss* Boss, float DeltaSeconds, bool& bSkipWake);
	static bool Tick(AWRBoss* Boss, float DeltaSeconds);
	static bool Allowed(const AWRBoss* Boss, const FWRBossAttack& Attack, const AWRCharacterBase* Target);
	static void AttackStarted(AWRBoss* Boss, FWRBossAttack& Attack, AWRCharacterBase* Target);
	static bool BeginStrike(AWRBoss* Boss, FWRBossAttack& Attack);
	static bool TickStrike(AWRBoss* Boss, FWRBossAttack& Attack, float DeltaSeconds);
	static void EndStrike(AWRBoss* Boss, const FWRBossAttack& Attack);
	static void HitLanded(AWRBoss* Boss, AWRCharacterBase* Victim, FName AttackId, EWRHitResult Result);
	static float Incoming(AWRBoss* Boss, const FWRHit& Hit, float Damage);
	static bool OwnWeakness(const AWRBoss* Boss);
	static void Damaged(AWRBoss* Boss, const FWRHit& Hit, float Damage);
	static void PhaseChanged(AWRBoss* Boss, int32 NewPhase);
	static bool PlaceRunback(AWRBoss* Boss);
	static FName BossPart(const AWRBoss* Boss, bool bScarred, FName Fallback);
	static void Died(AWRBoss* Boss);
	/** The Empress's plates left (0..6) and the bulb (for the HUD's boss line). */
	static int32 PlatesLeft(const AWRBoss* Boss);
};

/** Places the Desert's bosses and the plaza's stelae (server), once someone is in the Desert. */
UCLASS()
class WILDREACH_API UWRDesertArenas : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;
	static UWRDesertArenas* Get(const UObject* WorldContext);
	/** Server: the plaza back to its start (the stelae stand, the gnomon in the dial, no funnel). */
	void ResetPlaza();

private:
	bool Place(FName BossId);
	TSet<FName> Placed;
	float PlaceClock = 0.f;
	bool bStelae = false;
};
