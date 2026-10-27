#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRSky.generated.h"

class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UPostProcessComponent;
class USkyLightComponent;
class AWRSea;

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
	/** 0 on the volcanic (west, -X) side, 1 on the tundra (east) side, a smooth blend Width cm wide around Centre X. */
	static float BiomeBlend(float X, float Centre, float Width);
	/** How much "golden hour" there is: 1 with the sun just over the horizon, fading out by FadeAbove degrees and below -2. */
	static float LowSun(float SunElevation, float FadeAbove);
};

/**
 * The look of the world clock (AWRGameState) on one machine: turns the map's sun, raises a moon opposite it (a second
 * atmosphere light, so it has a disc and lights the night sky and clouds), dims the sky light and fog toward night,
 * and keeps auto-exposure from turning a moonlit night back into day. On top of that, where the camera is picks the
 * air: a warm ash haze and warm grade on the volcanic side, a cold blue haze and cool grade on the tundra, blended
 * across the ridge; light shafts and a warm glow in the fog toward a low sun at dawn and dusk; and mist lying in the
 * island's hollows (local fog volumes, thickest at dawn).
 * Numbers in tuning.json "sky". Nothing here replicates (every machine works the same hour out); dedicated servers
 * don't spawn it.
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
	/** Unit vector toward the sun (the sea's highlights follow it). */
	FVector GetSunDirection() const { return LastSunDir; }
	FString Describe() const;

	/** Average frame, game, draw and GPU time over the next Seconds, then log it (FRAMESTATS ...; console wr.FrameStats). */
	void StartFrameStats(float Seconds);

protected:
	virtual void BeginPlay() override;

private:
	void FindMapLights();
	void Apply(bool bForce);
	/** Biome haze, grade, light shafts, valley mist (each frame, cheap). */
	void ApplyAtmosphere(float DeltaSeconds, float Daylight);
	/** The valley mist volumes from Data/valley_fog.json. */
	void BuildMist();
	/** The camera's location on this machine (the first local player's view), or false (no view yet). */
	bool GetViewLocation(FVector& Out) const;
	void TickFrameStats(float DeltaSeconds);
	/** Auto-exposure's floor: how far the camera may brighten, by daylight and by whether it's under cover. */
	void UpdateExposure(float DeltaSeconds, float Daylight);

	UPROPERTY(VisibleAnywhere) TObjectPtr<UDirectionalLightComponent> Moon;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPostProcessComponent> Exposure;
	TWeakObjectPtr<UDirectionalLightComponent> Sun;
	TWeakObjectPtr<USkyLightComponent> SkyLight;
	TWeakObjectPtr<UExponentialHeightFogComponent> Fog;
	float SunLux = 10.f;
	float SkyIntensity = 1.f;
	FLinearColor FogColor = FLinearColor(0.45f, 0.55f, 0.7f);
	float FogDensity = 0.02f;
	float FogFalloff = 0.2f;
	float SunElevation = 0.f;
	float MoonElevation = 0.f;
	FVector LastSunDir = FVector::ZeroVector;
	float LastLoggedHour = -1.f;
	/** Smoothed biome blend at the camera (0 volcanic .. 1 tundra). */
	float Biome = -1.f;
	/** The valley mist (BuildMist): one local fog volume per hollow, all at the same density. */
	UPROPERTY(Transient) TArray<TObjectPtr<class ULocalFogVolumeComponent>> Mist;
	float MistDensity = -1.f;
	float MistClock = 0.f;
	/** Under something (a cave, a roof, a canopy) as of the last check; checked a few times a second. */
	bool bSheltered = false;
	float ShelterClock = 0.f;
	/** The exposure floor in EV100, eased toward its target (unset below -50). */
	float FloorEV = -100.f;
	bool bShaftsOn = false;
	/** wr.FrameStats accumulators. */
	float StatsLeft = 0.f;
	int32 StatsFrames = 0;
	double StatsFrame = 0.0, StatsGame = 0.0, StatsDraw = 0.0, StatsGPU = 0.0, StatsWorstFrame = 0.0;
};

/** Spawns the sky driver and the sea in every game world that renders (not on a dedicated server). */
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
	TWeakObjectPtr<AWRSea> Sea;
};
