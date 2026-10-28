#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PhysicsVolume.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRSea.h"
#include "WRWater.generated.h"

class ACharacter;
class AWRPlayerController;

/**
 * The sea you can swim in: a water physics volume over the open sea of Data/sea_mask.json, from below the sea floor
 * (tuning water.floor) up to the surface the sea is drawn at (sea.level). Its shape is boxes merged from the mask's
 * cells, so ground below sea level that the sea doesn't reach stays dry. Every machine builds the same one from the
 * data (it isn't replicated), so the server and the predicting client agree on who swims. CharacterMovement swims
 * while the capsule's centre is inside it. It only overlaps pawns: traces, the camera and building never see it.
 */
UCLASS(NotPlaceable)
class WILDREACH_API AWRWaterVolume : public APhysicsVolume
{
	GENERATED_BODY()

public:
	AWRWaterVolume(const FObjectInitializer& ObjectInitializer);
	/** Boxes over the mask's sea cells (rows merged into rectangles) and the open sea around the grid out to Extent
	 *  cm from the middle. Returns how many boxes. */
	int32 Build(const FWRSeaMask& Mask, float SurfaceZ, float FloorZ, float Extent);
};

/** Pure water rules (tests): the mask's sea cells as rectangles, in cells (X, Y, width, height). */
struct WILDREACH_API FWRWaterRules
{
	static TArray<FIntRect> MergeCells(const FWRSeaMask& Mask);
};

/**
 * Swimming and drinking (server and every client of a map whose sea mask names it): spawns the sea's water volume,
 * turns off the map's old flat sea plane's collision, and answers "how deep is the water here", "can this survivor
 * drink" (E in the water or at its edge restores Water on the server) and "may this creature step there" (wildlife
 * stays out of water deeper than it wades). On machines that draw, it also plays the splash into the water and the
 * strokes while swimming. Numbers in tuning.json "water".
 */
UCLASS()
class WILDREACH_API UWRWaterSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return bSounds; }

	static UWRWaterSubsystem* Get(const UObject* WorldContext);
	bool HasWater() const { return bActive; }
	float GetSurfaceZ() const { return SurfaceZ; }
	/** Whether the mask has open sea over this point (outside the grid it's all sea). */
	bool IsSeaAt(const FVector& Point) const;
	/** How deep the water is over the ground at this point's X/Y: 0 on land, above the sea or where the sea isn't. */
	float DepthAt(const FVector& Point) const;
	/** Its movement is in a water volume (swimming, or wading deep enough to). */
	static bool IsInWater(const ACharacter* Character);
	/** In the water, or standing at its edge low enough to reach it. Cached per frame (the prompt asks often). */
	bool CanDrink(const ACharacter* Character) const;
	/** E at the water, on the server: a gulp of Water (validated here), the sound for everyone. Empty when this player
	 *  isn't at the water (the caller goes on to "nothing to interact with"). */
	FString Drink(AWRPlayerController* Player);
	/** A creature's step (server and a rider's client): Direction, or zero when it would take it into water deeper
	 *  than it wades. A creature already that deep may still head for shallower water. */
	FVector KeepOutOfDeepWater(const ACharacter* Creature, const FVector& Direction) const;
	/** Dev (server command "water"): "stat" logs where the player is and how it moves; "thirst <0..100>"; "sink <cm>"
	 *  drops you through the ground (the rescue's test); "creatures" logs every creature within 80 m. */
	FString DevCommand(AWRPlayerController* Player, const TArray<FString>& Args);
	/** One line for logs and tests: where it is, its movement mode, in water, the depth there, Water, can drink. */
	FString Describe(const ACharacter* Character) const;

private:
	/** In one streamed-in level, or everything loaded (null). */
	void TurnOffOldPlane(ULevel* Level);
	void OnLevelAdded(ULevel* Level, UWorld* World);
	FDelegateHandle LevelAddedHandle;

	FWRSeaMask Mask;
	TWeakObjectPtr<AWRWaterVolume> Volume;
	float SurfaceZ = -20.f;
	bool bActive = false;
	bool bSounds = false;
	int32 OldPlanes = 0;
	/** Last CanDrink answer (one character, one frame). */
	mutable uint64 CacheFrame = 0;
	mutable TWeakObjectPtr<const ACharacter> CacheWho;
	mutable bool bCacheResult = false;
	/** Server: when each player may drink again. */
	TMap<TWeakObjectPtr<AController>, double> NextDrink;
	/** Sounds: who was swimming last look, and when their next stroke plays. */
	struct FSwimmer
	{
		bool bSwimming = false;
		double NextStroke = 0.0;
	};
	TMap<TWeakObjectPtr<ACharacter>, FSwimmer> Swimmers;
	double NextWatch = 0.0;
};
