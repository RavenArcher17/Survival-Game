#pragma once

// The Graveyard's micro-hazards (cooks/biome-graveyard.md 2.8; build-biome-graveyard.md 2.10): each with a tell of at
// least 0.9 s (a sound and a small visual cue), each able to kill.
//  - Grave subsidence (G1): turf lids over the pits the generator cut (3 m deep). Stand on one: 1.0 s of the turf
//    cracking in a ring, chalk dust and a dull crunch from below, then it gives way and you drop into the pit; half of
//    them hold a sleeping Gravehound den (2-3 wake). A lid grows back once nobody's in its pit. Dark, fresh-turned grave
//    soil marks the risky ground; a tamed Gloamcrow screams before you step on one.
//  - Lime pits (G3): quicklime slurry: 0.9 s of the crust crackling and hissing (a puff of steam), then 8 a second and
//    armour wear while you stand in it; rain makes it boil (x2). Graveyard creatures don't go in.
//  - Crypt draughts (the catacombs' draught rooms): every 8-15 s a 1.2 s rising whistle as every flame leans one way,
//    then a gust puts out held torches inside (lanterns and grave-light burn on).

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRGraveHazards.generated.h"

class AWRCharacterBase;
class UBoxComponent;

/** A turf lid over a subsidence pit (server-owned, replicated): intact (it holds), cracking (the tell), broken (open). */
UCLASS(NotPlaceable)
class WILDREACH_API AWRGraveLid : public AActor
{
	GENERATED_BODY()

public:
	AWRGraveLid();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** 0 intact, 1 cracking (the 1 s tell), 2 broken open. */
	UPROPERTY(ReplicatedUsing=OnRep_Stage) uint8 Stage = 0;
	/** The pit's floor (world Z) and whether a den sleeps in it. */
	float FloorZ = 0.f;
	bool bDen = false;

private:
	UFUNCTION() void OnRep_Stage();
	void BuildLook();
	void ApplyStage();
	UPROPERTY(Transient) TObjectPtr<UBoxComponent> Plate;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Turf;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Broken;
	double StageAt = 0.0;
	double NextScreamAt = 0.0;
	bool bLook = false;
};

/** The Graveyard's hazards (server): spawns the lids, runs the lime pits and the draughts. */
UCLASS()
class WILDREACH_API UWRGraveHazards : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UWRGraveHazards, STATGROUP_Tickables); }
	virtual bool IsTickable() const override;
	/** In a lime pit's slurry here (any machine: the data). */
	static bool InLime(const FVector& At, float* OutSurface = nullptr);

private:
	void SpawnLids();
	void StepLime(float Dt);
	void StepDraughts(float Dt);
	bool bLids = false;
	float Clock = 0.f;
	TMap<TWeakObjectPtr<AWRCharacterBase>, double> LimeSince;
	TMap<int32, double> NextGust;
	TMap<int32, bool> Whistling;
};
