#include "WRSky.h"
#include "WRDesertHazards.h"
#include "WRDesertTypes.h"
#include "WRUnderwater.h"
#include "WRSurvival.h"
#include "WRInventory.h"
#include "Wildreach.h"
#include "WRGameState.h"
#include "WRSea.h"
#include "WRSwampTypes.h"
#include "WRForestTypes.h"
#include "WRGraveTypes.h"
#include "WRCrystalTypes.h"
#include "WRFungalTypes.h"
#include "WRFx.h"
#include "WRSound.h"
#include "WRTuning.h"
#include "WRLand.h"
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

// The Swamp's air (cooks/biome-swamp.md 2.1 read rules; build-biome-swamp.md 1.1 Blighttown lesson: the fog is a sky
// parameter, never volumes): flat, grey-green, low light, and knee fog held at 0-2.5 m over the water on calm days (the
// height fog's second layer), raised only by the Fog and Miasma weathers. Blended by how much of a 60 m ring round the
// camera is swamp. Numbers: tuning swampWorld.sky.
namespace WRSwampSky
{
	float Air = -1.f;
	float Raise = 0.f;
	float Miasma = 0.f;

	/** 0..1: the share of the camera's spot and a ring round it inside the swamp. */
	float Share(const FVector& View, float Ring)
	{
		if (!WRSwamp::IsLive()) return 0.f;
		int32 In = WRSwamp::Contains(View) ? 2 : 0;
		for (int32 i = 0; i < 8; ++i)
		{
			const float A = i * PI * 0.25f;
			if (WRSwamp::Contains(View + FVector(FMath::Cos(A) * Ring, FMath::Sin(A) * Ring, 0.f))) ++In;
		}
		return In / 10.f;
	}
}

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
		const FVector At = Camera->GetActorLocation();
		const FWRLandDef* Land = WRLand::At(At);
		UE_LOG(LogWildreach, Log, TEXT("CAM at %s: biome %s, land %s, zone %c"), *At.ToCompactString(), *WRBiome::At(World, At).ToString(),
			Land ? *Land->Id.ToString() : TEXT("-"), Land && Land->GridLetter(Land->ToMap(At)) ? Land->GridLetter(Land->ToMap(At)) : TEXT('-'));
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
	// The round island's map (a map that has its own lands, Data/lands "map"): its own air, none of the Old Island's.
	for (const FWRLandDef& L : WRLand::All()) bRoundIsland |= !L.Map.IsEmpty() && L.Map == WRLand::CurrentMap();
	BuildMist();
	Apply(true);
}

void AWRSky::BuildMist()
{
	if (bRoundIsland) return;   // the valleys in valley_fog.json are the Old Island's
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
			SunScatterBase = C->VolumetricScatteringIntensity;
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
		// The Long Noon holds the Desert's light at noon (WRDesertHazards); everywhere else the clock's own.
		float Daylight = State->GetDaylight();
		WRDesertHz::SkyHour(this, State->GetHour(), Daylight);
		ApplyAtmosphere(DeltaSeconds, Daylight);
		UpdateExposure(DeltaSeconds, Daylight);
	}
	TickFrameStats(DeltaSeconds);
}

void AWRSky::UpdateExposure(float DeltaSeconds, float Daylight)
{
	// With no floor by day, auto-exposure brightened dark ground (Vulkarn's basalt arena) until everything lighter on it
	// blew out to white. By day it may now only brighten to about an ordinary scene's level; under cover (a cave, a roof,
	// a canopy) it may go further, so interiors still read; at night the floor that keeps a moonlit night dark.
	const FWRTuning& T = FWRTuning::Get();
	ShelterClock -= DeltaSeconds;
	if (ShelterClock <= 0.f)
	{
		ShelterClock = 0.25f;
		FVector View;
		bSheltered = false;
		if (GetViewLocation(View))
		{
			FCollisionQueryParams Query(SCENE_QUERY_STAT(WRSkyShelter), false);
			if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
			{
				Query.AddIgnoredActor(PC->GetPawn());
			}
			FHitResult Hit;
			bSheltered = GetWorld()->LineTraceSingleByChannel(Hit, View, View + FVector(0.f, 0.f, T.Num(TEXT("sky.shelterHeight"), 2500.f)), ECC_Visibility, Query);
		}
	}
	// Overcast over the Elder Canopy: the eye may not open up as far as under a sunlit canopy, so the deep wood reads
	// near-dark by day and the foxfire shows (cooks/biome-forest.md 2.9).
	// The ground's own brightness: the Volcanic's black rock is not opened up to grey by day, nor is moonlit snow made
	// daylight by night (the biome under the camera).
	{
		FVector Here;
		const FName B = GetViewLocation(Here) ? WRBiome::At(this, Here) : NAME_None;
		static const FName Volcanic(TEXT("volcanic")), Tundra(TEXT("tundra"));
		if (B != GroundBiome)
		{
			GroundBiome = B;
			UE_LOG(LogWildreach, Log, TEXT("SKY ground biome %s at %s"), *B.ToString(), *Here.ToString());
		}
		LavaGround = FMath::FInterpTo(LavaGround, B == Volcanic ? 1.f : 0.f, DeltaSeconds, 0.6f);
		SnowGround = FMath::FInterpTo(SnowGround, B == Tundra ? 1.f : 0.f, DeltaSeconds, 0.6f);
	}
	const float OpenDayEV = FMath::Lerp(T.Num(TEXT("sky.dayMinEV100"), -2.f), T.Num(TEXT("sky.volcanicDayMinEV100"), -2.f), LavaGround);
	const float DayEV = FMath::Lerp(bSheltered ? T.Num(TEXT("sky.shelteredMinEV100"), -5.5f) : OpenDayEV,
		T.Num(TEXT("sky.forestOvercastMinEV100"), 0.5f), ForestDark);
	const float NightEV = FMath::Lerp(T.Num(TEXT("sky.nightMinEV100"), 2.f), T.Num(TEXT("sky.tundraNightMinEV100"), 2.f), SnowGround);
	const float Target = FMath::Lerp(NightEV, DayEV, Daylight);
	// Eased, so stepping under a branch or through a doorway doesn't pump the exposure.
	FloorEV = FloorEV < -50.f || DeltaSeconds <= 0.f ? Target : FMath::FInterpTo(FloorEV, Target, DeltaSeconds, 1.5f);
	// The setting is EV100 only with the extended luminance range on; this project leaves it off, where the same
	// setting is a luminance (cd/m2, EV100 = log2(L * 8)), and a negative "EV" there meant no floor at all.
	static const IConsoleVariable* Extended = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DefaultFeature.AutoExposure.ExtendDefaultLuminanceRange"));
	const bool bEV = Extended && Extended->GetInt() != 0;
	Exposure->Settings.AutoExposureMinBrightness = bEV ? FloorEV : 0.125f * FMath::Pow(2.f, FloorEV);
}

void AWRSky::Apply(bool bForce)
{
	const AWRGameState* State = AWRGameState::Get(this);
	if (!State)
	{
		return;
	}
	const FWRTuning& T = FWRTuning::Get();
	// The Long Noon: in the Desert the sun stands at noon for the day (and eases back through a short dusk after).
	float Daylight = State->GetDaylight();
	const float Hour = WRDesertHz::SkyHour(this, State->GetHour(), Daylight);
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
		// (Overcast over the Elder Canopy takes most of what's left under the leaves: ForestDark, cooks/biome-forest.md 2.9.)
		S->SetIntensity(SunLux * SunUp * (1.f - WeatherSkyDim * WeatherAmount) * (1.f - ForestDark * T.Num(TEXT("sky.forestOvercastSunDim"), 0.6f)));
		S->SetVisibility(SunUp > 0.f);
	}
	// The Graveyard has no moon (cooks/biome-graveyard.md 2.7: moonless nights, only the lanterns), unless the Blood Moon
	// is up: then it comes back red and brighter than any island night.
	const float GraveMoonless = FMath::Max(GraveAir, 0.f) * (1.f - WRGraveSky::BloodMoon);
	const float MoonLux = T.Num(TEXT("sky.moonLux"), 0.5f) * FMath::SmoothStep(-2.f, 10.f, MoonElevation) * (1.f - SunUp)
		* (1.f - GraveMoonless) * (1.f + WRGraveSky::BloodMoon * T.Num(TEXT("graveyardWorld.sky.bloodMoonLux"), 0.6f));
	Moon->SetIntensity(MoonLux);
	Moon->SetVisibility(MoonLux > 0.f);
	Moon->SetLightColor(FMath::Lerp(FLinearColor::White, WRSkyDetail::SRGB(TEXT("graveyardWorld.sky.bloodMoonColor"), FColor(200, 40, 30)), WRGraveSky::BloodMoon));
	// Night: the sky light (it captures the dark sky) is lifted so shapes still read, and exposure may only brighten
	// so far. (The fog's colour and density are ApplyAtmosphere's.)
	if (USkyLightComponent* Sky = SkyLight.Get())
	{
		Sky->SetIntensity(SkyIntensity * FMath::Lerp(T.Num(TEXT("sky.nightSkyLight"), 3.f), 1.f, Daylight));
	}
	if (bForce)
	{
		UpdateExposure(0.f, Daylight);
	}

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
	// Storm peaks' air north of the island (cooks/biome-lightning.md 2.1): iron-dark haze, a cool slate grade, blended in
	// across the Ash Sound and the neck.
	if (bRoundIsland)
	{
		// The round island's air: the haze of the land under the camera (sky.roundAir.<biome> "r,g,b,density", sRGB; .default
		// where a biome has none), eased so a border or a teleport fades rather than snaps.
		const FName Here = WRBiome::At(this, View);
		FString Row = T.Str(*FString::Printf(TEXT("sky.roundAir.%s"), *Here.ToString()), TEXT(""));
		if (Row.IsEmpty()) Row = T.Str(TEXT("sky.roundAir.default"), TEXT("196,212,230,0.0035"));
		TArray<FString> P;
		Row.ParseIntoArray(P, TEXT(","));
		const FLinearColor Want = P.Num() >= 3 ? FLinearColor(FColor(uint8(FCString::Atoi(*P[0])), uint8(FCString::Atoi(*P[1])), uint8(FCString::Atoi(*P[2])))) : FLinearColor(0.55f, 0.65f, 0.78f);
		const float WantDensity = P.Num() >= 4 ? FCString::Atof(*P[3]) : 0.0035f;
		if (RoundDensity < 0.f) { RoundFog = Want; RoundDensity = WantDensity; }
		else
		{
			RoundFog = FMath::CInterpTo(RoundFog, Want, DeltaSeconds, 0.8f);
			RoundDensity = FMath::FInterpTo(RoundDensity, WantDensity, DeltaSeconds, 0.8f);
		}
	}
	const float StormTarget = bRoundIsland ? 0.f : FMath::SmoothStep(0.f, 1.f, float((T.Num(TEXT("world.islandNorthEdge"), -201600.f) + T.Num(TEXT("sky.stormBlendFrom"), 15000.f) - View.Y) / T.Num(TEXT("sky.stormBlendWidth"), 40000.f)));
	StormAir = StormAir < 0.f ? StormTarget : FMath::FInterpTo(StormAir, StormTarget, DeltaSeconds, 1.5f);
	const float SA = StormAir;
	// The Swamp blends over Storm peaks' air where it lies (its frame is inside the north lands).
	const float SwampTarget = bRoundIsland ? 0.f : WRSwampSky::Share(View, T.Num(TEXT("swampWorld.sky.blendRing"), 6000.f));
	WRSwampSky::Air = WRSwampSky::Air < 0.f ? SwampTarget : FMath::FInterpTo(WRSwampSky::Air, SwampTarget, DeltaSeconds, 1.2f);
	const float SW = WRSwampSky::Air;
	// The Ancient forest's air south of the island (cooks/biome-forest.md 2.1; build plan 2.4): green-gold light and a
	// soft haze on its own ground, and under the trees the canopy closes the light down (0 at the coast, 0.4 in the
	// streams and the villages, 0.75 in the old growth, 0.9 in the Elder Canopy: green dusk at noon), eased so a step
	// into a clearing opens the light over a second or two instead of snapping.
	const float ForestTarget = !bRoundIsland && WRForest::Contains(View) ? 1.f : 0.f;
	ForestAir = ForestAir < 0.f ? ForestTarget : FMath::FInterpTo(ForestAir, ForestTarget, DeltaSeconds, T.Num(TEXT("sky.forestBlendSpeed"), 0.7f));
	ForestCanopy = FMath::FInterpTo(ForestCanopy, ForestTarget > 0.f ? WRForest::Canopy(View) : 0.f, DeltaSeconds, T.Num(TEXT("sky.forestCanopySpeed"), 1.2f));
	const float FA = ForestAir, CN = ForestCanopy * ForestAir;
	// Under the Crystal caves' rock the open sky is gone (crystal 2.8 full dark): the sky light fades out within a second
	// or two of going in, so only carried light, tames and crystals woken by sound light the caves (the caves' rock is
	// procedural mesh, which the sky light's occlusion can't see).
	CrystalCave = FMath::FInterpTo(CrystalCave, !bRoundIsland && (WRCrystal::InCave(View) || WRFungal::UnderRock(View)) ? 1.f : 0.f, DeltaSeconds, T.Num(TEXT("crystalWorld.sky.caveFadeSpeed"), 1.5f));
	// The Graveyard's air west of the Spine (cooks/biome-graveyard.md 2.7): a bruise-violet haze by day that never quite
	// clears, a sickly yellow-green rim on the dusk instead of gold, and its two mists: Ground mist pooled knee-deep over
	// the graves by day, Corpse fog standing chest-high at night (the second fog layer, its floor at your feet).
	const float GraveTarget = !bRoundIsland && WRGrave::Contains(View) ? 1.f : 0.f;
	GraveAir = GraveAir < 0.f ? GraveTarget : FMath::FInterpTo(GraveAir, GraveTarget, DeltaSeconds, T.Num(TEXT("graveyardWorld.sky.blendSpeed"), 0.7f));
	const float GA = GraveAir;
	{
		float MistTarget = 0.f, TopTarget = GraveMistTop;
		if (GA > 0.f)
		{
			if (const FWRWeatherNow* W = UWRWeatherSubsystem::For(this, WRGrave::Biome()))
			{
				const float Now = UWRWeatherSubsystem::Now(this);
				const float Str = W->IsComing(Now) ? W->Approach(Now) * 0.45f : W->Strength(Now);
				if (W->Kind == TEXT("groundmist")) { MistTarget = Str; TopTarget = T.Num(TEXT("graveyardWorld.sky.kneeTop"), 60.f); }
				if (W->Kind == TEXT("corpsefog")) { MistTarget = Str; TopTarget = T.Num(TEXT("graveyardWorld.sky.chestTop"), 150.f); }
			}
		}
		GraveMist = FMath::FInterpTo(GraveMist, MistTarget * GA, DeltaSeconds, 0.5f);
		GraveMistTop = MistTarget > 0.f ? TopTarget : GraveMistTop;
	}
	{
		// The forest's weather looks (cooks/biome-forest.md 2.9). Clear (no weather): green light shafts through the gaps
		// in the old growth (canopy 0.75) and the Elder Canopy (0.9) while the sun stands high enough to reach the floor.
		// Overcast: the Elder Canopy goes from its green dusk to near-dark by day (the foxfire shows: UWRForestWorld).
		float ClearTarget = 0.f, DarkTarget = 0.f;
		if (FA > 0.f)
		{
			const FWRWeatherNow* FW = UWRWeatherSubsystem::For(this, FName(TEXT("forest")));
			const float WNow = UWRWeatherSubsystem::Now(this);
			const bool bWeather = FW && (FW->IsOn(WNow) || FW->IsComing(WNow));
			const float Str = !bWeather ? 0.f : FW->IsComing(WNow) ? FW->Approach(WNow) * 0.45f : FW->Strength(WNow);
			const float Old = FMath::GetMappedRangeValueClamped(FVector2f(T.Num(TEXT("sky.forestShaftCanopyFrom"), 0.5f), 0.75f), FVector2f(0.f, 1.f), ForestCanopy);
			const float Elder = FMath::GetMappedRangeValueClamped(FVector2f(T.Num(TEXT("sky.forestDarkCanopyFrom"), 0.78f), 0.9f), FVector2f(0.f, 1.f), ForestCanopy);
			const float SunHigh = FMath::SmoothStep(T.Num(TEXT("sky.forestShaftSunFrom"), 8.f), T.Num(TEXT("sky.forestShaftSunFull"), 25.f), SunElevation);
			ClearTarget = FA * Old * SunHigh * (1.f - Str);
			if (bWeather && FW->Kind == TEXT("overcast")) DarkTarget = FA * Elder * Str * Daylight;
		}
		ForestClear = FMath::FInterpTo(ForestClear, ClearTarget, DeltaSeconds, 0.5f);
		ForestDark = FMath::FInterpTo(ForestDark, DarkTarget, DeltaSeconds, 0.35f);
	}
	{
		// Fog and Miasma lift the knee fog into a wall (cook 2.9); Miasma turns it oxblood.
		float RaiseTarget = 0.f, MiasmaTarget = 0.f;
		if (SW > 0.f)
		{
			if (const FWRWeatherNow* W = UWRWeatherSubsystem::For(this, FName(TEXT("swamp"))))
			{
				const float Now = UWRWeatherSubsystem::Now(this);
				const float Str = W->IsComing(Now) ? W->Approach(Now) * 0.45f : W->Strength(Now);
				if (W->Kind == TEXT("fog") || W->Kind == TEXT("miasma")) RaiseTarget = Str;
				if (W->Kind == TEXT("miasma")) MiasmaTarget = Str;
			}
		}
		WRSwampSky::Raise = FMath::FInterpTo(WRSwampSky::Raise, RaiseTarget, DeltaSeconds, 0.5f);
		WRSwampSky::Miasma = FMath::FInterpTo(WRSwampSky::Miasma, MiasmaTarget, DeltaSeconds, 0.5f);
	}
	const float Low = FWRSkyRules::LowSun(SunElevation, T.Num(TEXT("sky.goldenHourBelow"), 18.f));

	// Weather (WRSurvival, Data/weather.json): the biome's weather at the camera thickens and tints the haze. Over the
	// 60 s warning the fog comes in from the far distance first (the storm wall on the horizon), then it closes to the
	// weather's vision; it lifts over the last 30 s.
	{
		float WTarget = 0.f;
		const FName WBiome = WRBiome::At(this, View);
		if (const FWRWeatherNow* W = UWRWeatherSubsystem::For(this, WBiome))
		{
			const float Now = UWRWeatherSubsystem::Now(this);
			if (const FWRWeatherKind* K = FWRWeatherDefs::Get().Kind(WBiome, W->Kind))
			{
				WTarget = W->IsComing(Now) ? W->Approach(Now) * 0.45f : W->Strength(Now);
				// Stalker fang talisman (the Pale Hunter's eye for snow): a blizzard doesn't close in on its wearer.
				const APlayerController* LocalPC = GetWorld()->GetFirstPlayerController();
				if (W->Kind == TEXT("blizzard") && LocalPC && LocalPC->GetPawn() && UWRInventoryComponent::GearStatFor(LocalPC->GetPawn(), TEXT("blizzardSight")) > 0.f)
				{
					WTarget *= T.Num(TEXT("gear.blizzardSightFog"), 0.2f);
				}
				if (WTarget > 0.f)
				{
					WeatherFogDensity = K->FogDensity;
					WeatherFogColor = K->FogColor;
					WeatherSkyDim = K->SkyDim;
				}
			}
		}
		WeatherAmount = FMath::FInterpTo(WeatherAmount, WTarget, DeltaSeconds, 0.6f);
	}
	if (UExponentialHeightFogComponent* F = Fog.Get())
	{
		// Haze: warm ash west, cold blue east, deep blue at night (readable, not black).
		const FLinearColor DayFog = bRoundIsland ? RoundFog : FMath::Lerp(FMath::Lerp(SRGB(TEXT("sky.volcanicFogColor"), FColor(176, 146, 120)), SRGB(TEXT("sky.tundraFogColor"), FColor(160, 190, 228)), B),
			SRGB(TEXT("sky.stormFogColor"), FColor(132, 140, 156)), SA);
		const FVector Night = Vec(TEXT("sky.nightFogColor"), FVector(0.01f, 0.015f, 0.035f));
		const FLinearColor Haze = FMath::Lerp(FLinearColor(Night.X, Night.Y, Night.Z), DayFog * T.Num(TEXT("sky.fogBrightness"), 1.f), Daylight);
		const FLinearColor Storm = WeatherFogColor * FMath::Lerp(T.Num(TEXT("sky.weatherNightFog"), 0.06f), T.Num(TEXT("sky.fogBrightness"), 1.f), Daylight);
		F->SetFogInscatteringColor(FMath::Lerp(Haze, Storm, WeatherAmount));
		const float BaseDensity = bRoundIsland ? RoundDensity : FMath::Lerp(FMath::Lerp(T.Num(TEXT("sky.volcanicFogDensity"), 0.027f), T.Num(TEXT("sky.tundraFogDensity"), 0.022f), B), T.Num(TEXT("sky.stormFogDensity"), 0.018f), SA);
		F->SetFogDensity(FMath::Lerp(BaseDensity, FMath::Max(BaseDensity, WeatherFogDensity), WeatherAmount));
		// Toward a low sun the haze glows warm: the dawn and dusk glare across a valley.
		F->SetDirectionalInscatteringExponent(T.Num(TEXT("sky.sunGlowExponent"), 6.f));
		F->SetDirectionalInscatteringColor(SRGB(TEXT("sky.sunGlowColor"), FColor(255, 150, 80)) * (Low * T.Num(TEXT("sky.sunGlow"), 0.6f)));
		if (WRSwamp::IsLive())
		{
			// The swamp's haze over whatever the land's was: grey-green by day (moss and still water), denser; the
			// weather's own colour still wins while it blows (Fog's grey-green, Miasma's oxblood: weather.json).
			const FLinearColor SwampDay = FMath::Lerp(SRGB(TEXT("swampWorld.sky.fogColor"), FColor(138, 144, 126)), SRGB(TEXT("swampWorld.sky.miasmaColor"), FColor(74, 10, 15)), WRSwampSky::Miasma);
			const FLinearColor SwampHaze = FMath::Lerp(FLinearColor(Night.X, Night.Y, Night.Z), SwampDay * T.Num(TEXT("sky.fogBrightness"), 1.f), Daylight);
			const FLinearColor Weather = FMath::Lerp(SwampHaze, WeatherFogColor * FMath::Lerp(T.Num(TEXT("sky.weatherNightFog"), 0.06f), T.Num(TEXT("sky.fogBrightness"), 1.f), Daylight), WeatherAmount * (1.f - WRSwampSky::Miasma));
			F->SetFogInscatteringColor(FMath::Lerp(FMath::Lerp(Haze, Storm, WeatherAmount), Weather, SW));
			const float SwampDensity = T.Num(TEXT("swampWorld.sky.fogDensity"), 0.03f);
			F->SetFogDensity(FMath::Lerp(FMath::Lerp(BaseDensity, FMath::Max(BaseDensity, WeatherFogDensity), WeatherAmount), FMath::Max(SwampDensity, WeatherFogDensity * WeatherAmount), SW));
			// Knee fog: the second layer, its floor at the water, thick under kneeTop and gone by a man's chest; Fog and
			// Miasma lift its top to raisedTop. Off outside the swamp (density 0).
			const float Water = FWRSwampData::Get().SeaZ;
			const float Top = FMath::Lerp(T.Num(TEXT("swampWorld.sky.kneeTop"), 250.f), T.Num(TEXT("swampWorld.sky.raisedTop"), 2500.f), WRSwampSky::Raise);
			F->SetSecondFogHeightOffset(Water + Top * 0.35f - F->GetComponentLocation().Z);
			F->SetSecondFogHeightFalloff(T.Num(TEXT("swampWorld.sky.kneeFalloff"), 2.6f) * 250.f / FMath::Max(50.f, Top));
			const float Calm = FMath::Lerp(T.Num(TEXT("swampWorld.sky.kneeNight"), 0.6f), 1.f, Daylight);
			F->SetSecondFogDensity(SW * Calm * T.Num(TEXT("swampWorld.sky.kneeDensity"), 0.35f) * (1.f + WRSwampSky::Raise * T.Num(TEXT("swampWorld.sky.raisedMult"), 1.5f)));
		}
		if (FA > 0.001f)
		{
			// The forest's haze: a soft green-gold over the meadows, deepening to moss-green under a closed canopy and
			// thicker there (the far trunks fade into it); the weather's own colour still wins while it blows.
			const FLinearColor ForestDay = FMath::Lerp(SRGB(TEXT("sky.forestFogColor"), FColor(176, 186, 140)), SRGB(TEXT("sky.forestCanopyFogColor"), FColor(66, 86, 54)), CN)
				* (1.f - ForestDark * T.Num(TEXT("sky.forestOvercastFogDim"), 0.6f));
			const FLinearColor ForestHaze = FMath::Lerp(FLinearColor(Night.X, Night.Y, Night.Z), ForestDay * T.Num(TEXT("sky.fogBrightness"), 1.f), Daylight);
			F->SetFogInscatteringColor(FMath::Lerp(FMath::Lerp(Haze, Storm, WeatherAmount), FMath::Lerp(ForestHaze, Storm, WeatherAmount), FA));
			const float ForestDensity = T.Num(TEXT("sky.forestFogDensity"), 0.02f) + CN * T.Num(TEXT("sky.forestCanopyFog"), 0.04f);
			F->SetFogDensity(FMath::Lerp(FMath::Lerp(BaseDensity, FMath::Max(BaseDensity, WeatherFogDensity), WeatherAmount), FMath::Max(ForestDensity, WeatherFogDensity * WeatherAmount), FA));
		}
		if (GA > 0.001f)
		{
			// The Graveyard's haze: bruise-violet grey by day, a touch thicker than the island's; the weather's own colour
			// still wins while it blows. Toward a low sun the glow is yellow-green, not gold.
			const FLinearColor GraveHaze = FMath::Lerp(FLinearColor(Night.X, Night.Y, Night.Z), SRGB(TEXT("graveyardWorld.sky.fogColor"), FColor(124, 112, 132)) * T.Num(TEXT("sky.fogBrightness"), 1.f), Daylight);
			F->SetFogInscatteringColor(FMath::Lerp(F->FogInscatteringLuminance, FMath::Lerp(GraveHaze, Storm, WeatherAmount), GA));
			const float GraveDensity = T.Num(TEXT("graveyardWorld.sky.fogDensity"), 0.032f);
			F->SetFogDensity(FMath::Lerp(F->FogDensity, FMath::Max(GraveDensity, WeatherFogDensity * WeatherAmount), GA));
			F->SetDirectionalInscatteringColor(FMath::Lerp(SRGB(TEXT("sky.sunGlowColor"), FColor(255, 150, 80)), SRGB(TEXT("graveyardWorld.sky.duskColor"), FColor(196, 204, 92)), GA)
				* (Low * T.Num(TEXT("sky.sunGlow"), 0.6f)));
		}
		if (GraveMist > 0.001f && !WRSwamp::IsLive())
		{
			// The mist layer: its floor at the survivor's feet, thick under its top and gone a little above it.
			const APlayerController* LocalPC = GetWorld()->GetFirstPlayerController();
			const APawn* Me = LocalPC ? LocalPC->GetPawn() : nullptr;
			const float Feet = Me ? Me->GetActorLocation().Z - Me->GetSimpleCollisionHalfHeight() : View.Z - 170.f;
			const float Top = FMath::Max(40.f, GraveMistTop);
			F->SetSecondFogHeightOffset(Feet + Top * 0.35f - F->GetComponentLocation().Z);
			F->SetSecondFogHeightFalloff(T.Num(TEXT("graveyardWorld.sky.mistFalloff"), 2.6f) * 250.f / Top);
			F->SetSecondFogDensity(GraveMist * T.Num(TEXT("graveyardWorld.sky.mistDensity"), 0.4f));
			bGraveFogSet = true;
		}
		else if (bGraveFogSet && !WRSwamp::IsLive())
		{
			F->SetSecondFogDensity(0.f);
			bGraveFogSet = false;
		}
	}
	// The Desert's air (cooks/biome-desert.md 2.1, 2.8): a pale warm haze over the sand by day; in the belt or a
	// sandstorm a wall of ochre dust you see 25 m into (60 m in a sand veil or Dunewrap 4: WRDesertHz::BeltAmount).
	if (UExponentialHeightFogComponent* F = Fog.Get(); F && WRDesert::Live())
	{
		const APlayerController* LocalPC = GetWorld()->GetFirstPlayerController();
		const APawn* Me = LocalPC ? LocalPC->GetPawn() : nullptr;
		float Vision = 0.f;
		const float BeltTarget = bRoundIsland ? 0.f : WRDesertHz::BeltAmount(Me, Vision);
		const float DesertTarget = !bRoundIsland && WRDesert::Contains(View) && !WRDesert::InCave(View) && !WRDesert::InHourhold(View) ? 1.f : 0.f;
		DesertAir = DesertAir < 0.f ? DesertTarget : FMath::FInterpTo(DesertAir, DesertTarget, DeltaSeconds, 0.7f);
		BeltAir = FMath::FInterpTo(BeltAir, BeltTarget, DeltaSeconds, 0.8f);
		if (Vision > 0.f) BeltVision = Vision;
		if (DesertAir > 0.001f)
		{
			const FLinearColor DesertDay = SRGB(TEXT("hazards.desert.sky.hazeColor"), FColor(236, 214, 170)) * T.Num(TEXT("sky.fogBrightness"), 1.f);
			const FLinearColor DesertHaze = FMath::Lerp(FLinearColor(0.05f, 0.06f, 0.1f), DesertDay, Daylight);
			F->SetFogInscatteringColor(FMath::Lerp(F->FogInscatteringLuminance, DesertHaze, DesertAir * (1.f - WeatherAmount * 0.5f)));
			F->SetFogDensity(FMath::Lerp(F->FogDensity, FMath::Max(T.Num(TEXT("hazards.desert.sky.hazeDensity"), 0.012f), F->FogDensity * 0.6f), DesertAir));
		}
		if (BeltAir > 0.001f)
		{
			// Exponential fog: density ~ 3 / (the distance you see, in metres x 1000) gets ~95% at that distance.
			const float Dense = FMath::Clamp(3.f / FMath::Max(1.f, BeltVision / 100.f), 0.f, 0.5f) * T.Num(TEXT("hazards.desert.sky.beltDensityScale"), 8.f);
			const FLinearColor Dust = SRGB(TEXT("hazards.desert.sky.dustColor"), FColor(176, 128, 74)) * FMath::Lerp(0.08f, 1.f, Daylight);
			F->SetFogInscatteringColor(FMath::Lerp(F->FogInscatteringLuminance, Dust, BeltAir));
			F->SetFogDensity(FMath::Lerp(F->FogDensity, Dense, BeltAir));
		}
	}
	// Under water (cooks/biome-ocean.md 2.1, 2.6): the murk closes in round the local head, blue-green, by depth band.
	if (UExponentialHeightFogComponent* F = Fog.Get()) WRUnderwater::ApplyFog(F, GetWorld(), DeltaSeconds);
	// Under the canopy the open sky's light is mostly leaves away: the sky light dims with the cover (the sun's own light
	// is the trees' shadows' job). Only touched when the value moves, so the rest of the world never pays for it.
	if (USkyLightComponent* SkyL = SkyLight.Get(); SkyL && (FA > 0.f || CrystalCave > 0.001f || ForestSkySet >= 0.f))
	{
		const float Want = SkyIntensity * FMath::Lerp(T.Num(TEXT("sky.nightSkyLight"), 3.f), 1.f, Daylight) * (1.f - CN * T.Num(TEXT("sky.forestCanopyDim"), 0.6f))
			* (1.f - ForestDark * T.Num(TEXT("sky.forestOvercastDim"), 0.7f)) * (1.f - CrystalCave * T.Num(TEXT("crystalWorld.sky.caveSkyDim"), 0.97f));
		if (FMath::Abs(Want - SkyL->Intensity) > 0.005f * FMath::Max(SkyIntensity, 0.01f))
		{
			SkyL->SetIntensity(Want);
			ForestSkySet = FA > 0.f || CrystalCave > 0.001f ? Want : -1.f;
		}
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
	// Under a clear sky in the forest's old growth the same rays stand all day, green-gold (cooks/biome-forest.md 2.9).
	if (UDirectionalLightComponent* S = Sun.Get())
	{
		const float Green = ForestClear > 0.02f ? ForestClear : 0.f;
		const bool bShafts = Low > 0.02f || Green > 0.f;
		if (bShafts != bShaftsOn)
		{
			bShaftsOn = bShafts;
			S->SetEnableLightShaftBloom(bShafts);
			S->SetEnableLightShaftOcclusion(bShafts);
			S->SetBloomThreshold(T.Num(TEXT("sky.shaftThreshold"), 2.f));
			S->SetOcclusionMaskDarkness(T.Num(TEXT("sky.shaftDarkness"), 0.6f));
		}
		if (bShafts)
		{
			S->SetBloomScale(FMath::Max(Low * T.Num(TEXT("sky.shaftBloom"), 0.4f), Green * T.Num(TEXT("sky.forestShaftBloom"), 0.5f)));
			const FLinearColor Tint = FMath::Lerp(SRGB(TEXT("sky.shaftTint"), FColor(255, 210, 160)), SRGB(TEXT("sky.forestShaftTint"), FColor(200, 236, 150)), Green);
			S->SetBloomTint(Tint.ToFColor(true));
		}

		// The shafts themselves: volumetric fog scattering the sun, so every gap in the canopy throws a beam through the
		// haze (the trees' shadows cut it). Volumetric fog is costly, so it's on only while the shafts are (unless the map
		// keeps it on anyway: sky.volumetricFog).
		const bool bVolumetric = Green > 0.05f && T.Num(TEXT("sky.forestShaftVolumetric"), 1.f) > 0.f;
		UExponentialHeightFogComponent* F = Fog.Get();
		if (F && bVolumetric != bForestVolumetric)
		{
			bForestVolumetric = bVolumetric;
			F->SetVolumetricFog(bVolumetric || T.Num(TEXT("sky.volumetricFog"), 0.f) > 0.f);
			if (!bVolumetric) S->SetVolumetricScatteringIntensity(SunScatterBase);
		}
		if (F && bVolumetric)
		{
			F->SetVolumetricFogScatteringDistribution(T.Num(TEXT("sky.forestShaftForward"), 0.75f));
			F->SetVolumetricFogAlbedo(SRGB(TEXT("sky.forestShaftAlbedo"), FColor(214, 240, 170)).ToFColor(true));
			F->SetVolumetricFogExtinctionScale(T.Num(TEXT("sky.forestShaftExtinction"), 1.5f));
			F->SetVolumetricFogDistance(T.Num(TEXT("sky.forestShaftDistance"), 9000.f));
			S->SetVolumetricScatteringIntensity(SunScatterBase * (1.f + Green * T.Num(TEXT("sky.forestShaftScatter"), 3.f)));
		}

		// And round the viewer: soft beams slanting down the sun's line and motes drifting in them (Data/fx.json
		// forest.light_shafts), laid in the world where they spawn so you walk through them. Hysteresis so a cloud
		// edge doesn't start and stop it.
		const bool bWantFx = ShaftFx ? Green > 0.35f : Green > 0.5f;
		if (bWantFx && !ShaftFx)
		{
			ShaftFx = WRFx::Attach(GetRootComponent(), TEXT("forest.light_shafts"));
		}
		else if (!bWantFx && ShaftFx)
		{
			WRFx::Stop(ShaftFx);
			ShaftFx = nullptr;
		}
		if (ShaftFx)
		{
			// X down the light's line (from the sun toward the ground), centred a little ahead of the camera.
			const FVector LightDir = LastSunDir.IsNearlyZero() ? FVector(0.f, 0.f, -1.f) : -LastSunDir;
			FVector Ahead = FVector::ZeroVector;
			if (const APlayerController* PC = GetWorld()->GetFirstPlayerController(); PC && PC->PlayerCameraManager)
			{
				Ahead = PC->PlayerCameraManager->GetCameraRotation().Vector().GetSafeNormal2D() * T.Num(TEXT("sky.forestShaftAhead"), 800.f);
			}
			ShaftFx->SetWorldLocationAndRotation(View + Ahead, LightDir.Rotation());
		}
	}

	// The grade: warm and a touch desaturated in the ash, cool on the snow; a faint shadow lift so night and caves
	// never go to flat black; vignette kept light.
	FPostProcessSettings& P = Exposure->Settings;
	P.WhiteTemp = FMath::Lerp(FMath::Lerp(FMath::Lerp(T.Num(TEXT("sky.volcanicWhiteTemp"), 7000.f), T.Num(TEXT("sky.tundraWhiteTemp"), 5900.f), B), T.Num(TEXT("sky.stormWhiteTemp"), 6200.f), SA),
		T.Num(TEXT("swampWorld.sky.whiteTemp"), 6000.f), SW);
	// The swamp's light is flat: less saturation and contrast, a grey-green gain (cook 2.1 "flat light").
	const float Sat = FMath::Lerp(FMath::Lerp(FMath::Lerp(T.Num(TEXT("sky.volcanicSaturation"), 0.94f), T.Num(TEXT("sky.tundraSaturation"), 0.97f), B), T.Num(TEXT("sky.stormSaturation"), 0.9f), SA),
		T.Num(TEXT("swampWorld.sky.saturation"), 0.86f), SW);
	P.ColorSaturation = FVector4(Sat, Sat, Sat, 1.f);
	const float Contrast = FMath::Lerp(T.Num(TEXT("sky.contrast"), 1.04f), T.Num(TEXT("swampWorld.sky.contrast"), 0.96f), SW);
	P.ColorContrast = FVector4(Contrast, Contrast, Contrast, 1.f);
	const FVector Gain = FMath::Lerp(FMath::Lerp(Vec(TEXT("sky.volcanicGain"), FVector(1.03f, 1.f, 0.95f)), Vec(TEXT("sky.tundraGain"), FVector(0.96f, 0.99f, 1.04f)), B),
		Vec(TEXT("sky.stormGain"), FVector(0.97f, 0.98f, 1.03f)), SA);
	const FVector SwampGain = FMath::Lerp(Gain, Vec(TEXT("swampWorld.sky.gain"), FVector(0.97f, 1.0f, 0.95f)), SW);
	P.ColorGain = FVector4(SwampGain.X, SwampGain.Y, SwampGain.Z, 1.f);
	if (FA > 0.f)
	{
		// The forest's grade: warm green-gold light, full colour (its meadows are the brightest ground in the world).
		P.WhiteTemp = FMath::Lerp(P.WhiteTemp, T.Num(TEXT("sky.forestWhiteTemp"), 5800.f), FA);
		const float FSat = FMath::Lerp(Sat, T.Num(TEXT("sky.forestSaturation"), 1.02f), FA);
		P.ColorSaturation = FVector4(FSat, FSat, FSat, 1.f);
		const FVector FGain = FMath::Lerp(SwampGain, Vec(TEXT("sky.forestGain"), FVector(0.98f, 1.02f, 0.94f)), FA);
		P.ColorGain = FVector4(FGain.X, FGain.Y, FGain.Z, 1.f);
	}
	const FVector Lift = Vec(TEXT("sky.shadowLift"), FVector(0.004f, 0.005f, 0.008f)) * FMath::Lerp(T.Num(TEXT("sky.nightShadowLift"), 2.f), 1.f, Daylight);
	P.ColorOffsetShadows = FVector4(Lift.X, Lift.Y, Lift.Z, 0.f);
	P.VignetteIntensity = T.Num(TEXT("sky.vignette"), 0.3f);
	{
		// The Graveyard's grade (cooks/biome-graveyard.md 2.7): cool, washed-out colour, a faint violet in the gain.
		// Its Corruption reads in the picture (2.15): from 50 the colour drains out (grey by 100) and the edges close in;
		// from 75 something whispers just behind you now and then; at 100 the tōrō flicker (UWRGraveWorld::StepLights).
		float Corr = 0.f;
		if (GA > 0.001f)
		{
			P.WhiteTemp = FMath::Lerp(P.WhiteTemp, T.Num(TEXT("graveyardWorld.sky.whiteTemp"), 6700.f), GA);
			const float GSat = FMath::Lerp(P.ColorSaturation.X, T.Num(TEXT("graveyardWorld.sky.saturation"), 0.82f), GA);
			P.ColorSaturation = FVector4(GSat, GSat, GSat, 1.f);
			const FVector GG = FMath::Lerp(FVector(P.ColorGain.X, P.ColorGain.Y, P.ColorGain.Z), Vec(TEXT("graveyardWorld.sky.gain"), FVector(0.98f, 0.96f, 1.02f)), GA);
			P.ColorGain = FVector4(GG.X, GG.Y, GG.Z, 1.f);
			APlayerController* LocalPC = GetWorld()->GetFirstPlayerController();
			if (const UWRSurvivalComponent* S = LocalPC ? UWRSurvivalComponent::For(LocalPC->GetPawn()) : nullptr) Corr = S->GetCorruption() * GA;
			const float Drain = FMath::GetMappedRangeValueClamped(FVector2f(50.f, 100.f), FVector2f(0.f, 1.f), Corr);
			const float DSat = P.ColorSaturation.X * (1.f - Drain * T.Num(TEXT("graveyardWorld.corruption.drain"), 0.7f));
			P.ColorSaturation = FVector4(DSat, DSat, DSat, 1.f);
			P.VignetteIntensity += Drain * T.Num(TEXT("graveyardWorld.corruption.vignette"), 0.35f);
			const double Now = GetWorld()->GetTimeSeconds();
			if (Corr >= 75.f && Now >= NextWhisperAt)
			{
				if (NextWhisperAt > 0.0)
				{
					FVector Eye; FRotator EyeRot(0.f);
					LocalPC->GetPlayerViewPoint(Eye, EyeRot);
					const FVector Behind = View - FRotator(0.f, EyeRot.Yaw, 0.f).Vector() * 250.f + FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f).Vector() * 120.f;
					WRSound::PlayAt(this, TEXT("grave.whisper"), Behind, FMath::GetMappedRangeValueClamped(FVector2f(75.f, 100.f), FVector2f(0.6f, 1.f), Corr));
				}
				NextWhisperAt = Now + FMath::FRandRange(T.Num(TEXT("graveyardWorld.corruption.whisperMin"), 14.f), T.Num(TEXT("graveyardWorld.corruption.whisperMax"), 30.f));
			}
		}
		WRGraveSky::Corruption = Corr;
	}
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
	return FString::Printf(TEXT("day %d hour %.2f daylight %.2f sun %.1f deg %.2f lux, moon %.1f deg %.2f lux, exposure floor EV100 %.1f%s, ground lava %.2f snow %.2f (%s)"),
		State ? State->GetDay() : 0, State ? State->GetHour() : -1.f, State ? State->GetDaylight() : -1.f,
		SunElevation, S && S->IsVisible() ? S->Intensity : 0.f, MoonElevation, Moon->IsVisible() ? Moon->Intensity : 0.f,
		FloorEV, bSheltered ? TEXT(" (sheltered)") : TEXT(""), LavaGround, SnowGround, *GroundBiome.ToString());
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
