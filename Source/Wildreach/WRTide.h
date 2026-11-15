// The Ocean's tide (cooks/biome-ocean.md 2.8, 2.10; cooks/build-biome-ocean.md 2.6): two highs and two lows a game
// day (highs 00:00 / 12:00, lows 06:00 / 18:00, +-1.2 m on a cosine), local to the B2 coast (a box with a ramp out to
// the still sea), plus the King Tide's drop. Every machine works the level out from the replicated game clock and the
// King Tide's replicated timeline, so nothing per-frame goes over the wire. The swimmable water is a set of water and
// air volumes over the box that follow the level; the drawn sea's tiles there are re-placed (AWRSea::SetRegionOffset).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PhysicsVolume.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRTide.generated.h"

/** The King Tide's phases (cooks/biome-ocean.md 2.10). */
UENUM()
enum class EWRKingTide : uint8 { None, Warning, Low, Return };

/**
 * The Ocean's replicated state (one per world, spawned by the server while the Ocean is live): the King Tide's phase and
 * when it began (server time), the beacons lit (clan names), the drowned bell, the Pound's sluices. Always relevant.
 */
UCLASS(NotPlaceable)
class WILDREACH_API AWROceanState : public AActor
{
	GENERATED_BODY()

public:
	AWROceanState();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	static AWROceanState* Get(const UObject* WorldContext);

	UPROPERTY(Replicated) EWRKingTide KingTide = EWRKingTide::None;
	/** Server time the King Tide's warning began (its timeline runs from here). */
	UPROPERTY(Replicated) double KingTideStart = -1.0;
	/** Server time the bore was let loose (the Return's last three minutes), or < 0. */
	UPROPERTY(Replicated) double BoreStart = -1.0;
	/** The King Tide's night: the reef spawns (glow clouds, regrowth, Glowray rings). */
	UPROPERTY(Replicated) bool bSpawningNight = false;
	/** Clans (or a lone survivor's name) whose Beacon Stack fire burns. */
	UPROPERTY(Replicated) TArray<FString> BeaconLitBy;
	/** Server time the drowned bell was last rung (the bell swings 12 s from then). */
	UPROPERTY(Replicated) double BellRungAt = -100.0;
	/** The Pound's three shut sluices burst (Thalassor's transition); reset with the fight. */
	UPROPERTY(Replicated) bool bSluicesBurst = false;
	UPROPERTY(Replicated) double SluicesBurstAt = -100.0;

	/** The King Tide's extra level (cm, <= 0) at server time Now: eased down over the warning, held through the Low,
	 *  back to 0 under the bore's front (the bore itself is drawn and swept by the ocean world). */
	float KingTideDrop(double Now) const;
};

/** One box of tide water (or of air over it: a non-water volume of the same priority so the still sea beneath never makes
 *  you swim above a low tide). Every machine spawns the same ones; they move every two seconds. */
UCLASS(NotPlaceable)
class WILDREACH_API AWRTideWater : public APhysicsVolume
{
	GENERATED_BODY()

public:
	AWRTideWater(const FObjectInitializer& ObjectInitializer);
	/** Shape it: XY box (cm), from FloorZ to TopZ. */
	void Shape(const FBox2D& Box, float FloorZ, float TopZ, bool bWater, int32 InPriority);
	/** Move its top (water) or its bottom (air) to Z, keeping the other end. */
	void SetLevel(float Z);
	FBox2D XY;
	float Floor = 0.f, Top = 0.f;
	float Weight = 1.f;
	bool bIsWater = true;
private:
	void Rebuild();
};

namespace WRTide
{
	/** The Ocean is live and the tide box is set (tuning tide.box). */
	WILDREACH_API bool Active();
	/** The tide box (XY cm) and its ramp width (cm), from tuning tide.box / tide.ramp (map metres). */
	WILDREACH_API bool Box(FBox2D& OutBox, float& OutRamp);
	/** 1 inside the tide box, falling to 0 across its ramp, 0 outside (XY cm). */
	WILDREACH_API float Weight(const FVector2D& XY);
	/** The tide's level at full weight (cm from mean): the daily curve plus the King Tide. */
	WILDREACH_API float Level(const UObject* WorldContext);
	/** The daily curve alone at a game hour (cm from mean). */
	WILDREACH_API float CurveAt(double TotalHours);
	/** The sea's mean drawn surface (cm): sea.level + water.surfaceOffset. */
	WILDREACH_API float MeanZ();
	/** The water's surface over a point (cm), tide included (the still sea's surface outside the box). */
	WILDREACH_API float SurfaceAt(const UObject* WorldContext, const FVector& Point);
	/** 0 low .. 1 high (the music's tide stem, the Tide Clock's read). */
	WILDREACH_API float Fullness(const UObject* WorldContext);
	/** Rising (true) or falling: rips run hardest on the falling tide, blowholes live on the rising. */
	WILDREACH_API bool Rising(const UObject* WorldContext);
}

/**
 * Spawns and moves the tide water (every machine of the island map while the Ocean is live) and offsets the drawn sea's
 * tiles over the box. Ticks twice a second; moves things every tide.moveSeconds.
 */
UCLASS()
class WILDREACH_API UWRTideSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return bOn; }

private:
	void Build();
	void Move(bool bForce);
	bool bOn = false;
	bool bBuilt = false;
	float Clock = 0.f;
	float LastLevel = 1.0e9f;
	TArray<TWeakObjectPtr<AWRTideWater>> Volumes;
};
