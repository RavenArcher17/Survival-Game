#include "WRSky.h"
#include "Wildreach.h"
#include "WRGameState.h"
#include "WRTuning.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SkyLight.h"
#include "EngineUtils.h"

// ---------------------------------------------------------------- rules

FVector FWRSkyRules::SunDirection(float Hour, float Dawn, float Dusk, float NoonElevation, float MidnightDepth)
{
	// Azimuth 0 = east (+X), a quarter turn = south (+Y): the day is the first half turn, the night the second.
	const float DayLength = FMath::Clamp(Dusk - Dawn, 1.f, 23.f);
	float Azimuth, Elevation;
	if (Hour >= Dawn && Hour < Dusk)
	{
		const float T = (Hour - Dawn) / DayLength;
		Azimuth = T * PI;
		Elevation = FMath::Sin(T * PI) * NoonElevation;
	}
	else
	{
		const float T = FMath::Fmod(Hour - Dusk + 24.f, 24.f) / (24.f - DayLength);
		Azimuth = PI + T * PI;
		Elevation = -FMath::Sin(T * PI) * MidnightDepth;
	}
	const float E = FMath::DegreesToRadians(Elevation);
	return FVector(FMath::Cos(E) * FMath::Cos(Azimuth), FMath::Cos(E) * FMath::Sin(Azimuth), FMath::Sin(E));
}

// ---------------------------------------------------------------- sky driver

AWRSky::AWRSky()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	// The moon: a second atmosphere light, so the sky draws its disc and it lights the night sky, clouds and fog.
	Moon = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Moon"));
	RootComponent = Moon;
	Moon->SetMobility(EComponentMobility::Movable);
	Moon->bAtmosphereSunLight = true;
	Moon->AtmosphereSunLightIndex = 1;
	Moon->LightSourceAngle = 0.6f;
	Moon->Intensity = 0.f;
	Moon->LightColor = FColor(158, 184, 255);
	Moon->SetVisibility(false);
	// Exposure: auto-exposure would brighten a moonlit night back to day; at night it may only go so far.
	Exposure = CreateDefaultSubobject<UPostProcessComponent>(TEXT("Exposure"));
	Exposure->SetupAttachment(RootComponent);
	Exposure->bUnbound = true;
	Exposure->Priority = 10.f;
	Exposure->Settings.bOverride_AutoExposureMinBrightness = true;
}

void AWRSky::BeginPlay()
{
	Super::BeginPlay();
	FindMapLights();
	Apply(true);
}

void AWRSky::FindMapLights()
{
	for (TActorIterator<ADirectionalLight> It(GetWorld()); It && !Sun.IsValid(); ++It)
	{
		if (UDirectionalLightComponent* C = Cast<UDirectionalLightComponent>(It->GetLightComponent()))
		{
			C->SetMobility(EComponentMobility::Movable);
			Sun = C;
			SunLux = C->Intensity;
		}
	}
	for (TActorIterator<ASkyLight> It(GetWorld()); It && !SkyLight.IsValid(); ++It)
	{
		SkyLight = It->GetLightComponent();
		SkyIntensity = SkyLight.IsValid() ? SkyLight->Intensity : 1.f;
	}
	for (TActorIterator<AExponentialHeightFog> It(GetWorld()); It && !Fog.IsValid(); ++It)
	{
		Fog = It->GetComponent();
		FogColor = Fog.IsValid() ? Fog->FogInscatteringLuminance : FogColor;
	}
	UE_LOG(LogWildreach, Log, TEXT("SKY sun %s (%.1f lux), sky light %s, fog %s"), Sun.IsValid() ? TEXT("found") : TEXT("MISSING"), SunLux,
		SkyLight.IsValid() ? TEXT("found") : TEXT("missing"), Fog.IsValid() ? TEXT("found") : TEXT("missing"));
}

void AWRSky::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Apply(false);
}

void AWRSky::Apply(bool bForce)
{
	const AWRGameState* State = AWRGameState::Get(this);
	if (!State)
	{
		return;
	}
	const FWRTuning& T = FWRTuning::Get();
	const float Hour = State->GetHour();
	const float Daylight = State->GetDaylight();
	const FVector SunDir = FWRSkyRules::SunDirection(Hour, T.Num(TEXT("clock.dawnHour"), 6.f), T.Num(TEXT("clock.duskHour"), 20.f),
		T.Num(TEXT("sky.noonElevation"), 65.f), T.Num(TEXT("sky.midnightDepth"), 50.f));
	// The moon rides opposite the sun, lifted a little so it's up all night.
	const FVector MoonDir = (-SunDir + FVector(0.f, 0.f, T.Num(TEXT("sky.moonLift"), 0.3f))).GetSafeNormal();
	SunElevation = FWRSkyRules::Elevation(SunDir);
	MoonElevation = FWRSkyRules::Elevation(MoonDir);

	// Lights turn only once the sun has moved a little (each turn redraws their shadow maps); a clock jump turns them now.
	const bool bJumped = !LastSunDir.IsZero() && FVector::DotProduct(SunDir, LastSunDir) < FMath::Cos(FMath::DegreesToRadians(3.f));
	if (bForce || bJumped || FVector::DotProduct(SunDir, LastSunDir) < FMath::Cos(FMath::DegreesToRadians(0.05f)))
	{
		LastSunDir = SunDir;
		if (UDirectionalLightComponent* S = Sun.Get())
		{
			S->SetWorldRotation((-SunDir).Rotation());
		}
		Moon->SetWorldRotation((-MoonDir).Rotation());
	}
	// The sun fades as it sets (the atmosphere reddens it on the way down), the moon comes up as it goes.
	const float SunUp = FMath::SmoothStep(-4.f, 3.f, SunElevation);
	if (UDirectionalLightComponent* S = Sun.Get())
	{
		S->SetIntensity(SunLux * SunUp);
		S->SetVisibility(SunUp > 0.f);
	}
	const float MoonLux = T.Num(TEXT("sky.moonLux"), 0.5f) * FMath::SmoothStep(-2.f, 10.f, MoonElevation) * (1.f - SunUp);
	Moon->SetIntensity(MoonLux);
	Moon->SetVisibility(MoonLux > 0.f);
	// Night: the sky light (it captures the dark sky) is lifted so shapes still read, the fog goes deep blue, and
	// exposure may only brighten so far.
	if (USkyLightComponent* Sky = SkyLight.Get())
	{
		Sky->SetIntensity(SkyIntensity * FMath::Lerp(T.Num(TEXT("sky.nightSkyLight"), 3.f), 1.f, Daylight));
	}
	if (UExponentialHeightFogComponent* F = Fog.Get())
	{
		F->SetFogInscatteringColor(FMath::Lerp(FLinearColor(0.01f, 0.015f, 0.035f), FogColor, Daylight));
	}
	Exposure->Settings.AutoExposureMinBrightness = FMath::Lerp(T.Num(TEXT("sky.nightMinEV100"), 0.5f), T.Num(TEXT("sky.dayMinEV100"), -10.f), Daylight);

	if (bForce || bJumped || FMath::FloorToInt(Hour) != FMath::FloorToInt(LastLoggedHour))
	{
		LastLoggedHour = Hour;
		UE_LOG(LogWildreach, Log, TEXT("SKY %s"), *Describe());
	}
}

FString AWRSky::Describe() const
{
	const AWRGameState* State = AWRGameState::Get(this);
	const UDirectionalLightComponent* S = Sun.Get();
	return FString::Printf(TEXT("day %d hour %.2f daylight %.2f sun %.1f deg %.2f lux, moon %.1f deg %.2f lux, min EV100 %.1f"),
		State ? State->GetDay() : 0, State ? State->GetHour() : -1.f, State ? State->GetDaylight() : -1.f,
		SunElevation, S && S->IsVisible() ? S->Intensity : 0.f, MoonElevation, Moon->IsVisible() ? Moon->Intensity : 0.f,
		Exposure->Settings.AutoExposureMinBrightness);
}

// ---------------------------------------------------------------- subsystem

bool UWRSkySubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UWRSkySubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (InWorld.GetNetMode() == NM_DedicatedServer)
	{
		return;  // nothing to look at
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Sky = InWorld.SpawnActor<AWRSky>(AWRSky::StaticClass(), FTransform::Identity, Params);
}
