#include "WRSky.h"
#include "Wildreach.h"
#include "WRGameState.h"
#include "WRSea.h"
#include "WRTuning.h"
#include "Dom/JsonObject.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/LocalFogVolumeComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SkyLight.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Engine/GameViewportClient.h"
#include "UnrealClient.h"

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

float FWRSkyRules::BiomeBlend(float X, float Centre, float Width)
{
	const float Half = FMath::Max(1.f, Width * 0.5f);
	return FMath::SmoothStep(Centre - Half, Centre + Half, X);
}

float FWRSkyRules::LowSun(float SunElevation, float FadeAbove)
{
	return FMath::SmoothStep(-2.f, 1.f, SunElevation) * (1.f - FMath::SmoothStep(3.f, FMath::Max(4.f, FadeAbove), SunElevation));
}

namespace WRSkyDetail
{
	/** A colour from tuning as "r,g,b" in sRGB 0-255 (how a designer picks it), or the fallback. */
	static FLinearColor SRGB(const TCHAR* Path, const FColor& Fallback)
	{
		TArray<FString> Parts;
		FWRTuning::Get().Str(Path, FString()).ParseIntoArray(Parts, TEXT(","));
		if (Parts.Num() < 3)
		{
			return FLinearColor(Fallback);
		}
		return FLinearColor(FColor(FCString::Atoi(*Parts[0]), FCString::Atoi(*Parts[1]), FCString::Atoi(*Parts[2])));
	}

	/** Three plain numbers from tuning as "a,b,c" (multipliers, linear colours). */
	static FVector Vec(const TCHAR* Path, const FVector& Fallback)
	{
		TArray<FString> Parts;
		FWRTuning::Get().Str(Path, FString()).ParseIntoArray(Parts, TEXT(","));
		return Parts.Num() < 3 ? Fallback : FVector(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2]));
	}

	/** wr.Cam X Y Z Yaw Pitch: look from a fixed point (screenshots of places the survivor can't stand); wr.Cam off. */
	static void Cam(const TArray<FString>& Args, UWorld* World)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		if (!PC)
		{
			return;
		}
		static TWeakObjectPtr<ACameraActor> Camera;
		if (Args.Num() < 5)
		{
			if (PC->GetPawn())
			{
				PC->SetViewTarget(PC->GetPawn());
			}
			return;
		}
		if (!Camera.IsValid() || Camera->GetWorld() != World)
		{
			Camera = World->SpawnActor<ACameraActor>();
		}
		Camera->SetActorLocationAndRotation(FVector(FCString::Atof(*Args[0]), FCString::Atof(*Args[1]), FCString::Atof(*Args[2])),
			FRotator(FCString::Atof(*Args[4]), FCString::Atof(*Args[3]), 0.f));
		PC->SetViewTarget(Camera.Get());
		UE_LOG(LogWildreach, Log, TEXT("CAM at %s"), *Camera->GetActorLocation().ToCompactString());
	}

	static void FrameStats(const TArray<FString>& Args, UWorld* World)
	{
		const UWRSkySubsystem* Sub = World ? World->GetSubsystem<UWRSkySubsystem>() : nullptr;
		if (AWRSky* Sky = Sub ? Sub->GetSky() : nullptr)
		{
			Sky->StartFrameStats(Args.Num() > 0 ? FCString::Atof(*Args[0]) : 4.f);
		}
	}

	static FAutoConsoleCommandWithWorldAndArgs CamCommand(TEXT("wr.Cam"),
		TEXT("Screenshot camera: wr.Cam X Y Z Yaw Pitch looks from that point; wr.Cam off goes back to the survivor."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Cam));
	static FAutoConsoleCommandWithWorldAndArgs FrameStatsCommand(TEXT("wr.FrameStats"),
		TEXT("wr.FrameStats [seconds]: average frame / game / draw / GPU ms over that long (stat unit must be on), logged as FRAMESTATS."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&FrameStats));

	/** Look checks: scale the valley mist (0 = off) to compare a view with and without it. */
	static float MistScale = 1.f;
	static FAutoConsoleVariableRef MistScaleVar(TEXT("wr.MistScale"), MistScale, TEXT("Valley mist density multiplier (look checks; 1 = as tuned, 0 = off)."));
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
	// Exposure: auto-exposure would brighten a moonlit night back to day; at night it may only go so far. The same
	// unbound volume carries the grade (white balance, saturation, contrast, tint) that follows the biome.
	Exposure = CreateDefaultSubobject<UPostProcessComponent>(TEXT("Exposure"));
	Exposure->SetupAttachment(RootComponent);
	Exposure->bUnbound = true;
	Exposure->Priority = 10.f;
	FPostProcessSettings& S = Exposure->Settings;
	S.bOverride_AutoExposureMinBrightness = true;
	S.bOverride_WhiteTemp = true;
	S.bOverride_ColorSaturation = true;
	S.bOverride_ColorContrast = true;
	S.bOverride_ColorGain = true;
	S.bOverride_ColorOffsetShadows = true;
	S.bOverride_VignetteIntensity = true;
}

void AWRSky::BeginPlay()
{
	Super::BeginPlay();
	FindMapLights();
	BuildMist();
	Apply(true);
}

void AWRSky::BuildMist()
{
	const TSharedPtr<FJsonObject> File = FWRTuning::LoadDataFile(TEXT("valley_fog.json"));
	const TArray<TSharedPtr<FJsonValue>>* Valleys = nullptr;
	if (!File.IsValid() || !File->TryGetArrayField(TEXT("valleys"), Valleys))
	{
		return;
	}
	const FWRTuning& T = FWRTuning::Get();
	const float Lift = T.Num(TEXT("sky.mistLift"), 300.f);
	const float Scale = T.Num(TEXT("sky.mistRadiusScale"), 1.f);
	for (const TSharedPtr<FJsonValue>& V : *Valleys)
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		const FVector Floor(O->GetNumberField(TEXT("x")), O->GetNumberField(TEXT("y")), O->GetNumberField(TEXT("z")));
		ULocalFogVolumeComponent* Volume = NewObject<ULocalFogVolumeComponent>(this);
		Volume->SetupAttachment(RootComponent);
		Volume->SetUsingAbsoluteLocation(true);
		Volume->SetUsingAbsoluteRotation(true);
		Volume->SetUsingAbsoluteScale(true);
		// A sphere centred on the hollow's floor whose fog lies low in it (the height fog fades within a few metres of
		// its middle) and fades softly toward its rim; a touch warm on the ash side, cold over the snow.
		Volume->SetWorldLocationAndRotation(Floor + FVector(0.f, 0.f, Lift), FRotator::ZeroRotator);
		Volume->SetWorldScale3D(FVector(O->GetNumberField(TEXT("radius")) * Scale / ULocalFogVolumeComponent::GetBaseVolumeSize()));
		Volume->HeightFogFalloff = T.Num(TEXT("sky.mistFalloff"), 1000.f);
		Volume->HeightFogOffset = T.Num(TEXT("sky.mistHeightOffset"), 0.f);
		Volume->RadialFogExtinction = 0.f;
		Volume->HeightFogExtinction = 0.f;
		Volume->FogAlbedo = Floor.X < 0.f ? WRSkyDetail::SRGB(TEXT("sky.volcanicMistColor"), FColor(232, 220, 206)) : WRSkyDetail::SRGB(TEXT("sky.tundraMistColor"), FColor(214, 226, 240));
		Volume->RegisterComponent();
		Mist.Add(Volume);
	}
	UE_LOG(LogWildreach, Log, TEXT("SKY valley mist: %d volumes"), Mist.Num());
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
		if (UExponentialHeightFogComponent* F = Fog.Get())
		{
			FogColor = F->FogInscatteringLuminance;
			FogDensity = F->FogDensity;
			FogFalloff = F->FogHeightFalloff;
			if (WRTune(TEXT("sky.volumetricFog"), 0.f) > 0.f)
			{
				F->SetVolumetricFog(true);
			}
		}
	}
	UE_LOG(LogWildreach, Log, TEXT("SKY sun %s (%.1f lux), sky light %s, fog %s (density %.3f falloff %.2f)"), Sun.IsValid() ? TEXT("found") : TEXT("MISSING"), SunLux,
		SkyLight.IsValid() ? TEXT("found") : TEXT("missing"), Fog.IsValid() ? TEXT("found") : TEXT("missing"), FogDensity, FogFalloff);
}

void AWRSky::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Apply(false);
	if (const AWRGameState* State = AWRGameState::Get(this))
	{
		ApplyAtmosphere(DeltaSeconds, State->GetDaylight());
	}
	TickFrameStats(DeltaSeconds);
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
	// Night: the sky light (it captures the dark sky) is lifted so shapes still read, and exposure may only brighten
	// so far. (The fog's colour and density are ApplyAtmosphere's.)
	if (USkyLightComponent* Sky = SkyLight.Get())
	{
		Sky->SetIntensity(SkyIntensity * FMath::Lerp(T.Num(TEXT("sky.nightSkyLight"), 3.f), 1.f, Daylight));
	}
	Exposure->Settings.AutoExposureMinBrightness = FMath::Lerp(T.Num(TEXT("sky.nightMinEV100"), 0.5f), T.Num(TEXT("sky.dayMinEV100"), -10.f), Daylight);

	if (bForce || bJumped || FMath::FloorToInt(Hour) != FMath::FloorToInt(LastLoggedHour))
	{
		LastLoggedHour = Hour;
		UE_LOG(LogWildreach, Log, TEXT("SKY %s"), *Describe());
	}
}

bool AWRSky::GetViewLocation(FVector& Out) const
{
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC || !PC->PlayerCameraManager)
	{
		return false;
	}
	Out = PC->PlayerCameraManager->GetCameraLocation();
	return !Out.IsNearlyZero();
}

void AWRSky::ApplyAtmosphere(float DeltaSeconds, float Daylight)
{
	using namespace WRSkyDetail;
	const FWRTuning& T = FWRTuning::Get();
	FVector View;
	if (!GetViewLocation(View))
	{
		return;
	}
	const AWRGameState* State = AWRGameState::Get(this);
	const float Hour = State ? State->GetHour() : 12.f;

	// Which air we're in: the camera's side of the ridge, eased so a teleport doesn't snap the whole grade.
	const float Target = FWRSkyRules::BiomeBlend(View.X, T.Num(TEXT("sky.biomeCentre"), 0.f), T.Num(TEXT("sky.biomeWidth"), 60000.f));
	Biome = Biome < 0.f ? Target : FMath::FInterpTo(Biome, Target, DeltaSeconds, 1.5f);
	const float B = Biome;
	const float Low = FWRSkyRules::LowSun(SunElevation, T.Num(TEXT("sky.goldenHourBelow"), 18.f));

	if (UExponentialHeightFogComponent* F = Fog.Get())
	{
		// Haze: warm ash west, cold blue east, deep blue at night (readable, not black).
		const FLinearColor DayFog = FMath::Lerp(SRGB(TEXT("sky.volcanicFogColor"), FColor(176, 146, 120)), SRGB(TEXT("sky.tundraFogColor"), FColor(160, 190, 228)), B);
		const FVector Night = Vec(TEXT("sky.nightFogColor"), FVector(0.01f, 0.015f, 0.035f));
		F->SetFogInscatteringColor(FMath::Lerp(FLinearColor(Night.X, Night.Y, Night.Z), DayFog * T.Num(TEXT("sky.fogBrightness"), 1.f), Daylight));
		F->SetFogDensity(FMath::Lerp(T.Num(TEXT("sky.volcanicFogDensity"), 0.027f), T.Num(TEXT("sky.tundraFogDensity"), 0.022f), B));
		// Toward a low sun the haze glows warm: the dawn and dusk glare across a valley.
		F->SetDirectionalInscatteringExponent(T.Num(TEXT("sky.sunGlowExponent"), 6.f));
		F->SetDirectionalInscatteringColor(SRGB(TEXT("sky.sunGlowColor"), FColor(255, 150, 80)) * (Low * T.Num(TEXT("sky.sunGlow"), 0.6f)));
	}

	// Valley mist: local fog volumes sitting in the island's hollows (Data/valley_fog.json), thickest around dawn, a
	// little at dusk, thin by day and light at night so it never hides the way. Updated a few times a second.
	MistClock -= DeltaSeconds;
	if (MistClock <= 0.f && Mist.Num() > 0)
	{
		MistClock = 0.25f;
		const float Dawn = T.Num(TEXT("clock.dawnHour"), 6.f), Dusk = T.Num(TEXT("clock.duskHour"), 20.f);
		const float DawnPeak = FMath::Exp(-FMath::Square((Hour - Dawn - 0.5f) / T.Num(TEXT("sky.mistDawnHours"), 1.8f)));
		const float DuskPeak = FMath::Exp(-FMath::Square((Hour - Dusk - 0.5f) / 1.5f));
		const float Amount = FMath::Lerp(T.Num(TEXT("sky.mistNight"), 0.04f), T.Num(TEXT("sky.mistDay"), 0.12f), Daylight)
			+ DawnPeak * T.Num(TEXT("sky.mistDawn"), 1.f) + DuskPeak * T.Num(TEXT("sky.mistDusk"), 0.45f);
		const float Density = Amount * T.Num(TEXT("sky.mistDensity"), 1.2f) * WRSkyDetail::MistScale;
		if (!FMath::IsNearlyEqual(Density, MistDensity, 0.005f))
		{
			MistDensity = Density;
			for (ULocalFogVolumeComponent* V : Mist)
			{
				V->SetRadialFogExtinction(Density);
				V->SetHeightFogExtinction(Density);
			}
		}
	}

	// Light shafts: screen-space god rays around a low sun (they're what dawn and dusk look like through trees).
	if (UDirectionalLightComponent* S = Sun.Get())
	{
		const bool bShafts = Low > 0.02f;
		if (bShafts != bShaftsOn)
		{
			bShaftsOn = bShafts;
			S->SetEnableLightShaftBloom(bShafts);
			S->SetEnableLightShaftOcclusion(bShafts);
			S->SetBloomThreshold(T.Num(TEXT("sky.shaftThreshold"), 2.f));
			S->SetOcclusionMaskDarkness(T.Num(TEXT("sky.shaftDarkness"), 0.6f));
			S->SetBloomTint(SRGB(TEXT("sky.shaftTint"), FColor(255, 210, 160)).ToFColor(true));
		}
		if (bShafts)
		{
			S->SetBloomScale(Low * T.Num(TEXT("sky.shaftBloom"), 0.4f));
		}
	}

	// The grade: warm and a touch desaturated in the ash, cool on the snow; a faint shadow lift so night and caves
	// never go to flat black; vignette kept light.
	FPostProcessSettings& P = Exposure->Settings;
	P.WhiteTemp = FMath::Lerp(T.Num(TEXT("sky.volcanicWhiteTemp"), 7000.f), T.Num(TEXT("sky.tundraWhiteTemp"), 5900.f), B);
	const float Sat = FMath::Lerp(T.Num(TEXT("sky.volcanicSaturation"), 0.94f), T.Num(TEXT("sky.tundraSaturation"), 0.97f), B);
	P.ColorSaturation = FVector4(Sat, Sat, Sat, 1.f);
	const float Contrast = T.Num(TEXT("sky.contrast"), 1.04f);
	P.ColorContrast = FVector4(Contrast, Contrast, Contrast, 1.f);
	const FVector Gain = FMath::Lerp(Vec(TEXT("sky.volcanicGain"), FVector(1.03f, 1.f, 0.95f)), Vec(TEXT("sky.tundraGain"), FVector(0.96f, 0.99f, 1.04f)), B);
	P.ColorGain = FVector4(Gain.X, Gain.Y, Gain.Z, 1.f);
	const FVector Lift = Vec(TEXT("sky.shadowLift"), FVector(0.004f, 0.005f, 0.008f)) * FMath::Lerp(T.Num(TEXT("sky.nightShadowLift"), 2.f), 1.f, Daylight);
	P.ColorOffsetShadows = FVector4(Lift.X, Lift.Y, Lift.Z, 0.f);
	P.VignetteIntensity = T.Num(TEXT("sky.vignette"), 0.3f);
}

void AWRSky::StartFrameStats(float Seconds)
{
	StatsLeft = FMath::Max(0.5f, Seconds);
	StatsFrames = 0;
	StatsFrame = StatsGame = StatsDraw = StatsGPU = StatsWorstFrame = 0.0;
}

void AWRSky::TickFrameStats(float DeltaSeconds)
{
	if (StatsLeft <= 0.f)
	{
		return;
	}
	// The numbers "stat unit" shows: whole frame, game thread, render thread, GPU.
	const double Frame = DeltaSeconds * 1000.0;
	StatsFrame += Frame;
	StatsWorstFrame = FMath::Max(StatsWorstFrame, Frame);
	// (Filled in while stat unit is on, which the capture script turns on first.)
	const UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
	if (const FStatUnitData* Unit = Viewport ? Viewport->GetStatUnitData() : nullptr)
	{
		StatsGame += Unit->RawGameThreadTime;
		StatsDraw += Unit->RawRenderThreadTime;
		StatsGPU += Unit->RawGPUFrameTime[0];
	}
	++StatsFrames;
	StatsLeft -= DeltaSeconds;
	if (StatsLeft <= 0.f && StatsFrames > 0)
	{
		FVector View = FVector::ZeroVector;
		GetViewLocation(View);
		UE_LOG(LogWildreach, Log, TEXT("FRAMESTATS frame %.2f ms (worst %.1f) game %.2f draw %.2f gpu %.2f over %d frames at %s"),
			StatsFrame / StatsFrames, StatsWorstFrame, StatsGame / StatsFrames, StatsDraw / StatsFrames, StatsGPU / StatsFrames, StatsFrames, *View.ToCompactString());
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
	Sea = InWorld.SpawnActor<AWRSea>(AWRSea::StaticClass(), FTransform::Identity, Params);
}
