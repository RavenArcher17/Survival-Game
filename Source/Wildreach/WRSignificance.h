#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRSignificance.generated.h"

class AWRCreature;
class USpringArmComponent;

/** Pure rules (tests). */
struct WILDREACH_API FWRSignificanceRules
{
	/**
	 * Whether a creature counts as far from every player: beyond FarRadius it goes far, and it only comes back inside
	 * NearRadius (smaller), so one walking along the edge doesn't flip at every check.
	 */
	static bool IsFar(float NearestPlayerDist, bool bWasFar, float FarRadius, float NearRadius);
};

/**
 * Wildlife cost by distance (significance). The island can have all 169 wild creatures out on a busy server, and each
 * one thinking, moving and ticking every frame is most of the server's work. A creature far from every player (beyond
 * significance.farRadius, more than the 150 m within which clients are sent creatures at all, so no player can see or
 * reach it) ticks its body, its movement and its brain significance.farTickInterval apart instead of every frame, with
 * the same behaviour in coarser steps; near any player nothing changes. Bosses and ridden creatures always run at full
 * rate; far ones are also looked at by replication a few times a second instead of every frame (nobody they could be
 * sent to is near). The authority (dedicated or listen server, or a game on its own) does this; on every machine a creature's ride
 * camera arm only ticks while a player rides it (an idle arm still sweeps for collision every frame).
 * Numbers in Data/tuning.json "significance"; -wrtune=significance.enabled:0 turns it off for comparisons.
 */
UCLASS()
class WILDREACH_API UWRSignificanceSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Creatures seen at the last check, and how many of them were far from every player. */
	int32 NumTracked() const { return Tracked.Num(); }
	int32 NumFar() const { return Far; }
	static UWRSignificanceSubsystem* Find(const UWorld* World);

private:
	struct FTracked
	{
		TWeakObjectPtr<AWRCreature> Creature;
		TWeakObjectPtr<USpringArmComponent> Arm;
		bool bFar = false;
	};
	void Update();
	/** Tick rates for one creature: Interval 0 = every frame (as spawned). */
	static void ApplyRate(AWRCreature* Creature, bool bFar, float Interval, float FarStep, float NetRateFar, bool bDedicated);

	TArray<FTracked> Tracked;
	int32 Far = 0;
	int32 Slice = 0;
	double NextLogAt = 0.0;
};
