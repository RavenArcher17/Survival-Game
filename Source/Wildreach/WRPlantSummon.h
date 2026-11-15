#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WRCharacterBase.h"
#include "WRCombatTypes.h"
#include "WRPlantSummon.generated.h"

class UAudioComponent;
class UPointLightComponent;
class UProceduralMeshComponent;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

/**
 * The Nature path's living summons (cooks/biome-forest.md 2.17, cooks/build-biome-forest.md 2.11): the Sprout turret, the
 * World tree and the Ancient guardian. A plant is a body in the fight (it has health, takes every blow through
 * UWRCombatLibrary::ApplyHit, creatures attack it), so it is a character; but it is not a tame (no AWRCreature, never in
 * a boss's head count, no tame slot). It fights for its planter: hostile to whom the planter is, never to the planter's
 * clan or their tames. Server-spawned and replicated; every machine builds its look from the Tree kit and grows it from
 * the replicated planting time (late joiners see it at its size). Deep roots (health), Fertile (time), Overgrowth (two
 * turrets), Pollen (a slowing haze), the Plant actor tag (Lush ground for Growth), Forest fire (it bursts when it dies).
 * Numbers: tuning spells.sproutTurret / worldTree / ancientGuardian, passives.pollen*, nature.plants.
 */
UCLASS(Abstract)
class WILDREACH_API AWRPlantSummon : public AWRCharacterBase
{
	GENERATED_BODY()

public:
	AWRPlantSummon(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	/** Its planter's name: a tame's owner for the hostility rules, the boss's damage ledger and the PvP scale. */
	virtual FString GetTameOwner() const override { return OwnerName; }
	virtual bool IsHostileTo(const AWRCharacterBase* Other) const override;
	virtual FString GetDisplayLabel() const override;
	virtual float ModifyIncomingDamage(const struct FWRHit& Hit, float Damage) override;

	/** Server: plant one of Class for Planter standing on Ground, facing Yaw. The planter's limit for the kind (turrets 1, 2
	 *  with Overgrowth; one World tree, one guardian): the oldest withers to make room. Null if it couldn't be planted. */
	static AWRPlantSummon* Plant(UClass* Class, AWRCharacterBase* Planter, const FVector& Ground, float Yaw);
	/** Every standing plant of Planter's (of one kind, or all). */
	static TArray<AWRPlantSummon*> PlantsOf(const AWRCharacterBase* Planter, FName Kind = NAME_None);

	/** Alive and not withering. */
	bool IsStanding() const;
	/** Server: its time is up or its planter is gone: it withers into the ground (no kill, no loot). */
	void Wither();
	/** Server: Quickening (Lightning + Nature): it fires faster until Until (server time). */
	void Quicken(float Until) { QuickenedUntil = FMath::Max(QuickenedUntil, Until); }
	/** Server: Thorn feast: heal Share of its max health. */
	void HealShare(float Share);
	/** The server's clock every machine agrees on (the game state's), for the growth and the moves. */
	float ServerNow() const;

	/** The survivor who planted it (null once they're gone: it withers). */
	UPROPERTY(Replicated) TObjectPtr<AWRCharacterBase> OwnerPawn;
	UPROPERTY(Replicated) FString OwnerName;
	/** Its kind's id; named CreatureId because WRFx reads that property for the impact a hit throws up (Data/fx.json
	 *  "impacts": wood chips, not blood). */
	UPROPERTY(Replicated) FName CreatureId;
	/** Server time it was planted (the grow-in). */
	UPROPERTY(Replicated) float BornAt = 0.f;
	/** Pollen (Bloom node): enemies near it move slower; a pale-yellow haze shows the area. */
	UPROPERTY(ReplicatedUsing=OnRep_Pollen) bool bPollen = false;
	/** Withering (its time is up) or dying (killed): server time it began, < 0 while it stands. */
	UPROPERTY(ReplicatedUsing=OnRep_Withering) float WitherStart = -1.f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void OnDeath(AActor* Killer) override;

	/** Its tuning block (spells.<key>) and kind. */
	FString SpellKey;
	FName KindId;
	FString KindName;
	/** Health, seconds and the grow-in (seconds); the capsule. */
	float PlantHealth = 60.f;
	float PlantSeconds = 20.f;
	float GrowSeconds = 0.6f;
	float Num(const TCHAR* Field, float Fallback) const;

	/** Server: what it does each tick while standing. */
	virtual void ServerTick(float DeltaSeconds) {}
	/** Every machine with a picture: build the look (once), then move it each frame (Grow 0..1, Wither 0..1). */
	virtual void BuildLook() {}
	virtual void TickLook(float DeltaSeconds, float Grow, float WitherShare) {}
	/** Server, once when it stands complete: the arrival (the guardian's aggro draw). */
	virtual void OnGrown() {}

	/** What the look hangs from: sunk and scaled by the grow-in and the wither. */
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> LookRoot;
	/** A Tree kit species at a height (cm) under Parent: null where there's no picture (a dedicated server). */
	UStaticMeshComponent* AddTree(USceneComponent* Parent, FName Species, int32 Variant, float Height, const FVector& At, const FRotator& Rot);
	/** A code-modelled part (bark limbs, a seed pod) under Parent. */
	UProceduralMeshComponent* AddModel(USceneComponent* Parent, const struct FWRToolMesh& Part, const FVector& At, const FRotator& Rot);

	float QuickenedUntil = -1.f;

private:
	UFUNCTION() void OnRep_Pollen();
	UFUNCTION() void OnRep_Withering();
	void Explode();
	float DieAt = 0.f;
	bool bGrownDone = false;
	bool bLookBuilt = false;
	bool bExploded = false;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> PollenFx;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> PollenHum;
	double WitherFxAt = -1.0;
};

/**
 * Sprout turret (Bloom 5): a bramble mound with a seed-pod head on a stalk that turns to the nearest enemy within
 * 1,500 cm it can see and spits 3 thorns a second (6 damage, bleed 3 each) at 2,400 cm/s, never homing. 60 health, 20 s.
 */
UCLASS()
class WILDREACH_API AWRSproutTurret : public AWRPlantSummon
{
	GENERATED_BODY()
public:
	AWRSproutTurret(const FObjectInitializer& ObjectInitializer);
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	/** Every machine: what the head turns to (null: it idles), and a counter bumped by every shot (the head's recoil). */
	UPROPERTY(Replicated) TObjectPtr<AActor> AimAt;
	UPROPERTY(ReplicatedUsing=OnRep_Shot) uint8 ShotSerial = 0;
protected:
	virtual void ServerTick(float DeltaSeconds) override;
	virtual void BuildLook() override;
	virtual void TickLook(float DeltaSeconds, float Grow, float WitherShare) override;
private:
	UFUNCTION() void OnRep_Shot();
	AWRCharacterBase* PickTarget() const;
	FVector MouthWorld() const;
	void Shoot(AWRCharacterBase* Target);
	float Clock = 0.f;
	float Recoil = 0.f;
	float HeadYaw = 0.f;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Head;
};

/**
 * World tree (Grove capstone): an 8 m lime grows out of the ground in 0.8 s, its gold-green crown healing allies within
 * 600 cm 6 a second (Lifebloom, Purify) and taking 20% off the damage they take, for 15 s.
 */
UCLASS()
class WILDREACH_API AWRWorldTree : public AWRPlantSummon
{
	GENERATED_BODY()
public:
	AWRWorldTree(const FObjectInitializer& ObjectInitializer);
protected:
	virtual void ServerTick(float DeltaSeconds) override;
	virtual void BuildLook() override;
	virtual void TickLook(float DeltaSeconds, float Grow, float WitherShare) override;
	virtual void OnGrown() override;
private:
	float Clock = 0.f;
	TSet<TWeakObjectPtr<AWRCharacterBase>> Ticked;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> CrownLight;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> CrownFx;
};

/**
 * Ancient guardian (Bloom capstone): a 4 m walking tree pulls itself up out of the ground, draws the creatures within
 * 10 m to it, walks after its planter's enemies and slams them (25 damage, a 0.8 s wind-up you can read, the blow lands
 * where the arms come down). 400 health, 30 s.
 */
UCLASS()
class WILDREACH_API AWRAncientGuardian : public AWRPlantSummon
{
	GENERATED_BODY()
public:
	AWRAncientGuardian(const FObjectInitializer& ObjectInitializer);
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	/** Server time the current slam began (every machine times the arms from it); < 0: none yet. */
	UPROPERTY(Replicated) float SlamStart = -100.f;
protected:
	virtual void ServerTick(float DeltaSeconds) override;
	virtual void BuildLook() override;
	virtual void TickLook(float DeltaSeconds, float Grow, float WitherShare) override;
	virtual void OnGrown() override;
private:
	AWRCharacterBase* PickTarget() const;
	void Strike();
	TWeakObjectPtr<AWRCharacterBase> Foe;
	float NextSlamAt = 0.f;
	bool bStruck = true;
	float WalkPhase = 0.f;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Body;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> ArmL;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> ArmR;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> LegL;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> LegR;
};

/**
 * A thorn in flight: Thorn shot's and the Sprout turret's. A curved dark thorn modelled in code (a green sheath at its
 * base), flying straight at 2,400 cm/s and never homing; it bites what it strikes through ApplyHit and sticks where it
 * lands. Living conductor (Lightning + Nature) may chain a turret's thorn on.
 */
UCLASS()
class WILDREACH_API AWRThornShot : public AActor
{
	GENERATED_BODY()
public:
	AWRThornShot();
	/** Server: fly Hit along the actor's forward at Speed. From: the plant that spat it (or null for a spell). */
	void Launch(const FWRHit& InHit, float Speed, AActor* From);
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	UFUNCTION() void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& HitResult);
	UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> Collision;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UProjectileMovementComponent> Movement;
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Model;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Trail;
	FWRHit Hit;
	TWeakObjectPtr<AActor> Spitter;
	bool bSpent = false;
};
