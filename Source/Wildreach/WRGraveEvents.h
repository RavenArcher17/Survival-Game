#pragma once

// The Graveyard's biome event, the Blood Moon (cooks/biome-graveyard.md 2.10; build-biome-graveyard.md 2.12): a
// world.json events row (bloodmoon: about every 3 real days at dusk, one game day, the Biome events setting), scheduled
// and announced by the world systems like the Bloom. This file runs what it does on the Graveyard:
//  - the sign (its 5 minute warning): the sky goes rust-red and the moon the Graveyard never has rises red over the
//    chapel (WRGraveSky::BloodMoon, eased on every machine); the Belfry's bell and the chapel tower's toll once;
//  - the Rising (its night, 20:00-05:00): corpse fog forced, corruption builds twice as fast on the land, processions of
//    4-8 Risen behind a lantern-bearer walk the procession roads toward the nearest light within 600 m (one every 150 m
//    of road where a survivor is near), standing vigil at a camp's safe-zone edge; one destroyed re-forms 6 minutes
//    later; the Bell-Ringer tolls every 15 s, the Gravedigger digs two more graves, Necromancers gain Souls twice over
//    and raise two bodies at a time;
//  - dawn: every Risen of the processions falls into a bone pile (bone, a chance of soulstone); the Mourning lasts to the
//    day's end (the fog hangs).
// Numbers: tuning graveyardEvents.bloodMoon.*.

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRGraveEvents.generated.h"

class AWRCreature;

UCLASS()
class WILDREACH_API UWRGraveEvents : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UWRGraveEvents, STATGROUP_Tickables); }
	virtual bool IsTickable() const override;

	/** Every machine: the Blood Moon's sign (its warning) is up. */
	static bool Sign(const UObject* WorldContext);
	/** Every machine: the Blood Moon is on and it's night (the Rising). */
	static bool Rising(const UObject* WorldContext);
	/** Every machine: the Blood Moon is on (sign excluded): the Rising or the Mourning after it. */
	static bool On(const UObject* WorldContext);

private:
	struct FProcession
	{
		int32 Road = 0;
		float S = 0.f;          // cm along the road
		float Dir = 1.f;        // walking up or down it
		TArray<TWeakObjectPtr<AWRCreature>> Members;   // [0] the lantern-bearer
		double ReformAt = 0.0;  // destroyed: when it forms again
		bool bSpawned = false;
	};
	struct FRoadLine { TArray<FVector> P; TArray<float> Acc; float Len = 0.f; };
	void BuildRoads();
	FVector RoadAt(const FRoadLine& R, float S) const;
	void StepServer(float Dt);
	void SpawnProcession(FProcession& P);
	void StepProcession(FProcession& P, float Dt);
	void Dawn();
	TArray<FRoadLine> Roads;
	TArray<FProcession> Processions;
	bool bRoads = false, bWasSign = false, bWasRising = false;
	float Clock = 0.f;
};
