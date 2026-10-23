#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "WRCharacterBase.h"
#include "WRCreature.generated.h"

class AWRCreature;

/** One creature definition from Data/creatures.json. */
struct FWRCreatureDef
{
	FName Id;
	FString Name;
	FName Tier;
	FName Behavior;   // aggressive / neutral / passive
	FName Archetype;  // pack_hunter, ground_predator, dummy, ...
	float Health = 70.f;
	float Damage = 12.f;
	float Speed = 450.f;
	float Poise = 30.f;
	float Radius = 40.f;
	float HalfHeight = 60.f;
	float AggroRadius = 1500.f;
	float AttackRange = 190.f;
	FLinearColor Color = FLinearColor::Gray;
	FName Element;
	TArray<FName> Resists;
	/** Given to whoever lands the killing blow (default: 2 raw meat, 1 hide). */
	TArray<TPair<FName, int32>> Loot;
};

class WILDREACH_API FWRCreatureRegistry
{
public:
	static FWRCreatureRegistry& Get();
	const FWRCreatureDef* Find(FName Id) const;
	void Reload();

private:
	FWRCreatureRegistry();
	TArray<FWRCreatureDef> Defs;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FWROnCreatureDied, AWRCreature*);

/** Creature body: owns its ability system, takes its stats from a creature definition. */
UCLASS()
class WILDREACH_API AWRCreature : public AWRCharacterBase
{
	GENERATED_BODY()

public:
	AWRCreature(const FObjectInitializer& ObjectInitializer);

	/** Spawn a creature by id (picks the dummy class for the training dummy). Server only. */
	static AWRCreature* SpawnCreature(UWorld* World, FName CreatureId, const FVector& Location, const FRotator& Rotation);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool IsCreature() const override { return true; }
	virtual void NotifyAttackedBy(AActor* Attacker) override;
	virtual FString GetDisplayLabel() const override;

	const FWRCreatureDef* GetDef() const { return FWRCreatureRegistry::Get().Find(CreatureId); }
	AActor* GetAggroTarget() const { return AggroTarget.Get(); }
	void SetAggroTarget(AActor* Target) { AggroTarget = Target; }
	FVector GetHome() const { return Home; }

	UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_CreatureId, Category="Creature") FName CreatureId = TEXT("cinderfang");

	FWROnCreatureDied OnCreatureDied;

protected:
	virtual void BeginPlay() override;
	virtual void OnDeath(AActor* Killer) override;
	void ApplyDef();

private:
	UFUNCTION() void OnRep_CreatureId();

	TWeakObjectPtr<AActor> AggroTarget;
	FVector Home = FVector::ZeroVector;
};

/** Stationary target for testing combat. Never dies; resets to full health a few seconds after the last hit. */
UCLASS()
class WILDREACH_API AWRTrainingDummy : public AWRCreature
{
	GENERATED_BODY()

public:
	AWRTrainingDummy(const FObjectInitializer& ObjectInitializer);
	virtual void Tick(float DeltaSeconds) override;
	virtual void Die(AActor* Killer) override;
	virtual void NotifyAttackedBy(AActor* Attacker) override;

private:
	double LastHitTime = -1000.0;
};

/** Simple server-side creature brain: idle -> chase (packs flank) -> bite -> give up and go home. */
UCLASS()
class WILDREACH_API AWRCreatureAI : public AAIController
{
	GENERATED_BODY()

public:
	AWRCreatureAI();
	virtual void Tick(float DeltaSeconds) override;

private:
	AActor* PickTarget(AWRCreature* Self) const;
	float ThinkTimer = 0.f;
	float FlankAngle = 0.f;
};

/** Keeps a number of creatures alive around itself. */
UCLASS()
class WILDREACH_API AWRCreatureSpawner : public AActor
{
	GENERATED_BODY()

public:
	AWRCreatureSpawner();
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category="Spawner") FName CreatureId = TEXT("cinderfang");
	UPROPERTY(EditAnywhere, Category="Spawner") int32 Count = 3;
	UPROPERTY(EditAnywhere, Category="Spawner") float Radius = 400.f;
	UPROPERTY(EditAnywhere, Category="Spawner") float RespawnSeconds = 20.f;

protected:
	virtual void BeginPlay() override;

private:
	void SpawnOne();
	TArray<TWeakObjectPtr<AWRCreature>> Alive;
	TArray<double> PendingRespawns;
};
