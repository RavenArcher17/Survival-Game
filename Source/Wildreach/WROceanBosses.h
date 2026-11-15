#pragma once

// The Ocean's bosses and their arenas (cooks/biome-ocean.md 2.13-2.14; cooks/build-biome-ocean.md 2.13-2.14):
//  - Old Barnacle in Barnacle Cove: a giant hermit crab in a newcomer longboat's hull (1,200 of its own, blunt x2, a
//    plank torn off each 300; fire on it 3 s and he bolts out naked). Naked 12 s (x2 on him, stance x1.5, only the pinch
//    and the charge), then he runs for a spare shell on the beach (the bronze bell: heavy and slow; the Driftback
//    carapace: light and fast): break it first or stand on it and he stays naked.
//  - The Siren Eel in Siren's Hollow: her song from a rock island (8 s): tames within 30 m leave the fight and come to
//    her, players within 20 m who can hear are drawn to her; 150 to her crest in the 1.2 s windup breaks it (beeswax
//    plugs: no lure; the Siren scale: your tames don't turn). Her second jaw drags you under.
//  - The Reef Tyrant on its 3.2 km loop (resting at the Mooring Stones 13:00-15:00): harpoon it and tie the line off on
//    a mooring stone (it can't go 10 m past it); two tethers in under 2 m of water beach it 8 s.
//  - Thalassor in the Pound: the pool and the flat (phase 1), beached 5 s off a lunge or into a tower, the three sun
//    mirrors on the wall (2 s of beam dries a part 20 s: x1.5, stance x2; daylight, or a loaded sunstone), the blowhole's
//    breath, the Sluice Breaks at 50% (the Pound floods 4 m over 10 s: phase 2, the towers the perches, breaches break
//    them), the hidden Maelstrom (3 s of beam breaks it 8 s).
// Moves are rows in Data/attacks.json (bosses.<id>); numbers in tuning bosses.<id>.

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRCombatTypes.h"
#include "WROceanBosses.generated.h"

class AWRBoss;
class AWRCharacterBase;
class AWRPlayerController;
class AWRTideWater;
class USphereComponent;
class UBoxComponent;
class UProceduralMeshComponent;
class UMaterialInstanceDynamic;
struct FWRBossAttack;

/**
 * An Ocean arena thing (server-owned, replicated):
 *  bell / carapace   Old Barnacle's spare shells on the cove's beach: Health; State 1 broken, 2 taken (he wears it).
 *  tower             a Pound stilt-tower stump (5 m, 4 m across its top, a stair): State 1 broken by a breach.
 */
UCLASS(NotPlaceable)
class WILDREACH_API AWROceanBossProp : public AActor
{
	GENERATED_BODY()

public:
	AWROceanBossProp();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	UPROPERTY(ReplicatedUsing=OnRep_Look) FName Kind;
	UPROPERTY(ReplicatedUsing=OnRep_Look) uint8 State = 0;
	UPROPERTY(Replicated) int32 Index = 0;
	float Health = 0.f, MaxHealth = 0.f;
	/** Server: a blow on it (a spare shell). True when it took it. */
	bool TakeHit(AActor* By, float Damage, bool bFire, bool bBlunt);
	void SetState(uint8 NewState);
	bool IsLive() const { return State == 0; }
	/** Someone standing on it (a spare shell he won't take). */
	bool Occupied() const;
	static TArray<TWeakObjectPtr<AWROceanBossProp>>& All();

private:
	UFUNCTION() void OnRep_Look();
	void Build();
	UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> HitShape;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Solid;
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Mesh;
	FName BuiltKind;
	uint8 BuiltState = 255;
};

/**
 * A Sunwatch mirror on the Pound's wall: a bronze dish 3 m across on a stone pivot. Hold E to take it, aim with the
 * camera; in daylight (06:00-20:00; Overcast half, Sea fog and Rain none) or with a sunstone loaded (30 s each) it throws
 * a gold beam. Every machine draws the dish's aim and the beam from the replicated state.
 */
UCLASS(NotPlaceable)
class WILDREACH_API AWRSunMirror : public AActor
{
	GENERATED_BODY()

public:
	AWRSunMirror();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(Replicated) FString Operator;
	UPROPERTY(Replicated) FRotator Aim = FRotator::ZeroRotator;
	UPROPERTY(Replicated) float Strength = 0.f;           // 0 no beam, 0.5 overcast, 1 full
	UPROPERTY(Replicated) FVector_NetQuantize BeamEnd = FVector::ZeroVector;
	UPROPERTY(Replicated) float SunstoneLeft = 0.f;       // s of stored daylight
	TWeakObjectPtr<APawn> OperatorPawn;
	FVector DishCentre() const;
	static TArray<TWeakObjectPtr<AWRSunMirror>>& All();

private:
	void BuildLook();
	void ServerTick(float Dt);
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Pivot;
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Dish;
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Beam;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> BeamMat;
	bool bLook = false;
};

/** The hooks AWRBoss calls for the Ocean's four (friend of AWRBoss), beside the Fungal caverns'. */
struct WILDREACH_API FWROceanBoss
{
	static bool IsOceanBoss(const AWRBoss* Boss);
	static void Begin(AWRBoss* Boss);
	static void Reset(AWRBoss* Boss);
	static void EndPlay(AWRBoss* Boss);
	/** Not fighting: the Tyrant's loop and rest, Barnacle in his cove, the Siren in her lake, Thalassor in the pool. */
	static bool TickIdle(AWRBoss* Boss, float DeltaSeconds, bool& bSkipWake);
	/** Fighting, before its attack machine: true when this ran the tick (naked and running, beached, tethered...). */
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
	/** A melee blow on an arena thing (a spare shell). */
	static bool HitThing(AActor* Thing, class UPrimitiveComponent* Comp, AWRCharacterBase* By, float Damage, bool bFire);
	/** Server: a burst of daylight at At (a sunstone flare): Seconds of beam on Thalassor's part there. */
	static void SunBurst(UWorld* World, const FVector& At, float Seconds);
	/** Thalassor's part nearest a point: 0 head, 1 blowhole, 2 flank, 3 fluke (-1 not near). */
	static int32 PartAt(const AWRBoss* Boss, const FVector& P, float Reach);
	/** Server: beam time on Thalassor's part (the mirrors, a flare, a Sun path hit). */
	static void AddBeam(AWRBoss* Boss, int32 Part, float Seconds);
};

/** Places the Ocean's bosses, the Pound's towers, mirrors, flood and run-back stone, the cove's spare shells (server);
 *  the Pound's flood water on every machine; the mirrors' E prompt. */
UCLASS()
class WILDREACH_API UWROceanArenas : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;
	static UWROceanArenas* Get(const UObject* WorldContext);

	/** Client: "Take the mirror" / "Let go" / "Load a sunstone" by a Sunwatch mirror. */
	static bool Prompt(const AWRPlayerController* PC, FString& OutTarget, FString& OutVerb);
	static bool ClientInteract(AWRPlayerController* PC);
	/** Server: "world ocean mirror". */
	static FString Command(AWRPlayerController* PC);
	/** The Pound's flood level (cm) at server time Now (-1e9 when it hasn't burst). */
	static float FloodLevel(const UObject* WorldContext);
	/** Server: the Pound's arena back to its start (towers stand, the sluices shut, the flood drains). */
	void ResetPound();
	void SpawnSpares(AWRBoss* Barnacle);

private:
	bool Place(FName BossId);
	void StepFlood();
	TSet<FName> Placed;
	float PlaceClock = 0.f;
	UPROPERTY(Transient) TObjectPtr<AWRTideWater> Flood;
	UPROPERTY(Transient) TObjectPtr<AActor> FloodLook;
	bool bPoundBuilt = false;
};
