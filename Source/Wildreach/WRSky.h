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
	/** Unit vector toward the moon, and how much of its light reaches the world (0..1: 0 by day, while it's down, under
	 *  cloud, in the moonless Graveyard): the lake's glints (WRRoundIsland). */
	FVector GetMoonDirection() const { return MoonDirNow; }
	float GetMoonGlint() const { return MoonGlint; }
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
	/** The round island's caves' skylights: a volume of dust in each skylit chamber for the sun's beam to light (shown,
	 *  with the volumetric fog on, only near one by day). */
	void BuildSkylightDust();
	/** The round island's night sky: a dome of stars round the camera (M_NightSky), shown as the sun goes down and the
	 *  clouds and weather allow, turning with the hours about the pole. */
	void BuildStars();
	void TickStars();
	/** The camera's location on this machine (the first local player's view), or false (no view yet). */
	bool GetViewLocation(FVector& Out) const;
	void TickFrameStats(float DeltaSeconds);
	/** Auto-exposure's floor: how far the camera may brighten, by daylight and by whether it's under cover. */
	void UpdateExposure(float DeltaSeconds, float Daylight);
	/** How near a cave's skylight the view is (eased): the eye opens up less there by day. */
	float SkylitCave = 0.f;

	UPROPERTY(VisibleAnywhere) TObjectPtr<UDirectionalLightComponent> Moon;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPostProcessComponent> Exposure;
	TWeakObjectPtr<UDirectionalLightComponent> Sun;
	TWeakObjectPtr<USkyLightComponent> SkyLight;
	TWeakObjectPtr<UExponentialHeightFogComponent> Fog;
	/** The map's sky atmosphere: thick weather tints its light toward the weather's haze (WeatherSky). */
	TWeakObjectPtr<class USkyAtmosphereComponent> Atmosphere;
	FLinearColor SkyFactorBase = FLinearColor::White;
	FLinearColor FogAmbientBase = FLinearColor::White;
	/** The map's sun colour (a low sun warms it: sky.goldenSunColor) and its exposure bias (sky.*ExposureBias add to it);
	 *  the sky's brightness factor and the sun's colour as last set (set only when they move). */
	FLinearColor SunColorBase = FLinearColor::White;
	float ExposureBiasBase = 0.f;
	FLinearColor SkyFactorSet = FLinearColor(-1.f, -1.f, -1.f, -1.f);
	FLinearColor SunColorSet = FLinearColor(-1.f, -1.f, -1.f, -1.f);
	/** How far into golden hour the sun is (0..1, FWRSkyRules::LowSun with sky.goldenHourBelow) as of the last Apply. */
	float Golden = 0.f;
	/** How far thick weather has closed the sky (0..1) as last set, and the round island's weather layer's ground (the
	 *  second fog stands on the ground under the camera there, eased so a cliff doesn't jolt it). */
	float WeatherSkySet = -1.f;
	/** The round island's weather layer density (sky.roundWeather.<kind>'s, or the look's x sky.roundWeatherDensity). */
	float WeatherLayerDensity = 0.f;
	float WeatherGround = -1.f;
	bool bWeatherLayerSet = false;
	float SunLux = 10.f;
	float SkyIntensity = 1.f;
	FLinearColor FogColor = FLinearColor(0.45f, 0.55f, 0.7f);
	float FogDensity = 0.02f;
	float FogFalloff = 0.2f;
	float SunElevation = 0.f;
	float MoonElevation = 0.f;
	FVector MoonDirNow = FVector::UpVector;
	float MoonGlint = 0.f;
	FVector LastSunDir = FVector::ZeroVector;
	float LastLoggedHour = -1.f;
	/** Smoothed biome blend at the camera (0 volcanic .. 1 tundra). */
	float Biome = -1.f;
	/** 0..1: how far into Storm peaks' air the camera is (north of the island). */
	float StormAir = -1.f;
	/** 0..1: how far into the Ancient forest's air the camera is (south of the island), and the canopy over it (0 coast ..
	 *  0.9 Elder Canopy, WRForest::Canopy), both eased; the sky light last set for it (set only when it changes). */
	float ForestAir = -1.f;
	float ForestCanopy = 0.f;
	float ForestSkySet = -1.f;
	/** 0..1: the view is in the Crystal caves (the sky light fades out under their rock). */
	float CrystalCave = 0.f;
	/** 0..1: how far into the Graveyard's air the camera is (eased), its mist layer's height (knee for Ground mist,
	 *  chest for Corpse fog) and whether the second fog layer was last set by it (so leaving it clears the layer). */
	float GraveAir = -1.f;
	/** The Desert's air and its belt's sand fog (eased), and how far you see in it (cm). */
	float DesertAir = -1.f, BeltAir = 0.f, BeltVision = 2500.f;
	/** The round island's map (not the Old Island's): its air is the land under the camera's (sky.roundAir), eased; the
	 *  Old Island's ridge, mist and the old lands' airs (placed by that map's coordinates) stay off. */
	bool bRoundIsland = false;
	FLinearColor RoundFog = FLinearColor::Black;
	float RoundDensity = -1.f;
	float GraveMistTop = 0.f;
	float GraveMist = 0.f;
	bool bGraveFogSet = false;
	/** World seconds of the next whisper while the local survivor's Corruption reads 75+ on the Graveyard. */
	double NextWhisperAt = 0.0;
	/** The forest's own weather looks at the camera (cooks/biome-forest.md 2.9), eased 0..1: Clear's green light shafts
	 *  under the old growth and the Elder Canopy by day (volumetric scattering on the sun, shaft bloom, drifting motes
	 *  and beams round the view: ShaftFx), and Overcast closing the Elder Canopy from dusk to near-dark. */
	float ForestClear = 0.f;
	float ForestDark = 0.f;
	bool bForestVolumetric = false;
	float SunScatterBase = 1.f;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> ShaftFx;
	/** The weather at the camera (WRSurvival): how far in it is (eased; the 60 s approach brings the far fog in first)
	 *  and its look (weather.json look: fog density and colour, how much it dims the sun). */
	float WeatherAmount = 0.f;
	float WeatherFogDensity = 0.f;
	FLinearColor WeatherFogColor = FLinearColor::Gray;
	float WeatherSkyDim = 0.f;
	/** The valley mist (BuildMist): one local fog volume per hollow, all at the same density. */
	UPROPERTY(Transient) TArray<TObjectPtr<class ULocalFogVolumeComponent>> Mist;
	UPROPERTY(Transient) TArray<TObjectPtr<class ULocalFogVolumeComponent>> SkylightDust;
	bool bSkylightBeams = false;
	UPROPERTY(Transient) TObjectPtr<class UStaticMeshComponent> Stars;
	UPROPERTY(Transient) TObjectPtr<class UMaterialInstanceDynamic> StarsMID;
	float MistDensity = -1.f;
	float MistClock = 0.f;
	/** Under something (a cave, a roof, a canopy) as of the last check; checked a few times a second. */
	bool bSheltered = false;
	float ShelterClock = 0.f;
	/** The exposure floor in EV100, eased toward its target (unset below -50). */
	float FloorEV = -100.f;
	/** How much the ground under the camera is black lava (by day) or snow (by night), eased: their own exposure floors. */
	float LavaGround = 0.f, SnowGround = 0.f;
	FName GroundBiome;
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
