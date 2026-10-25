#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRSky.generated.h"

class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UPostProcessComponent;
class USkyLightComponent;

/** Pure sky math (tests). */
struct WILDREACH_API FWRSkyRules
{
	/**
	 * Unit vector toward the sun at an hour: it rises in the east (+X) at dawn, crosses the south (+Y) at NoonElevation
	 * degrees and sets in the west at dusk, then passes under the north, MidnightDepth degrees down, until dawn.
	 */
	static FVector SunDirection(float Hour, float Dawn, float Dusk, float NoonElevation, float MidnightDepth);
	/** Degrees above the horizon of a direction. */
	static float Elevation(const FVector& Dir) { return FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Dir.Z, -1.f, 1.f))); }
};

/**
 * The look of the world clock (AWRGameState) on one machine: turns the map's sun, raises a moon opposite it (a second
 * atmosphere light, so it has a disc and lights the night sky and clouds), dims the sky light and fog toward night,
 * and keeps auto-exposure from turning a moonlit night back into day. Numbers in tuning.json "sky". Nothing here
 * replicates (every machine works the same hour out); dedicated servers don't spawn it.
 */
UCLASS(NotPlaceable)
class WILDREACH_API AWRSky : public AActor
{
	GENERATED_BODY()

public:
	AWRSky();
	virtual void Tick(float DeltaSeconds) override;

	/** What the sky is showing (logs, tests). */
	float GetSunElevation() const { return SunElevation; }
	float GetMoonElevation() const { return MoonElevation; }
	FString Describe() const;

protected:
	virtual void BeginPlay() override;

private:
	void FindMapLights();
	void Apply(bool bForce);

	UPROPERTY(VisibleAnywhere) TObjectPtr<UDirectionalLightComponent> Moon;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPostProcessComponent> Exposure;
	TWeakObjectPtr<UDirectionalLightComponent> Sun;
	TWeakObjectPtr<USkyLightComponent> SkyLight;
	TWeakObjectPtr<UExponentialHeightFogComponent> Fog;
	float SunLux = 10.f;
	float SkyIntensity = 1.f;
	FLinearColor FogColor = FLinearColor(0.45f, 0.55f, 0.7f);
	float SunElevation = 0.f;
	float MoonElevation = 0.f;
	FVector LastSunDir = FVector::ZeroVector;
	float LastLoggedHour = -1.f;
};

/** Spawns the sky driver in every game world that renders (not on a dedicated server). */
UCLASS()
class WILDREACH_API UWRSkySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	AWRSky* GetSky() const { return Sky.Get(); }

private:
	TWeakObjectPtr<AWRSky> Sky;
};
