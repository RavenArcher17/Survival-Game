#include "WRGameState.h"
#include "Wildreach.h"
#include "WRTuning.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

void AWRGameState::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		Preset = FWRTuning::Get().GetPreset();
		ServerSettings = FWRTuning::Get().ExportServerSettings();
		if (!bClockSet)
		{
			SetTotalHours(WRTune(TEXT("clock.startHour"), 8.f));
		}
	}
}

void AWRGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRGameState, Preset);
	DOREPLIFETIME(AWRGameState, ServerSettings);
	DOREPLIFETIME(AWRGameState, ClockAnchorHours);
	DOREPLIFETIME(AWRGameState, ClockAnchorTime);
}

void AWRGameState::OnRep_ServerSettings()
{
	FWRTuning::Get().ImportServerSettings(Preset, ServerSettings);
	UE_LOG(LogWildreach, Log, TEXT("Server settings from host: %s (%d values)"), *Preset, ServerSettings.Num());
}

// ---------------------------------------------------------------- world clock

double AWRGameState::GetTotalHours() const
{
	const double DaySeconds = FMath::Max(1.0, static_cast<double>(WRTune(TEXT("clock.dayMinutes"), 60.f)) * 60.0);
	return ClockAnchorHours + (GetServerWorldTimeSeconds() - ClockAnchorTime) * 24.0 / DaySeconds;
}

float AWRGameState::GetHour() const
{
	return static_cast<float>(FMath::Fmod(FMath::Max(0.0, GetTotalHours()), 24.0));
}

int32 AWRGameState::GetDay() const
{
	return 1 + FMath::FloorToInt32(FMath::Max(0.0, GetTotalHours()) / 24.0);
}

bool AWRGameState::IsNight() const
{
	return IsNightAt(GetHour());
}

bool AWRGameState::IsNightAt(float Hour)
{
	return Hour >= WRTune(TEXT("clock.duskHour"), 20.f) || Hour < WRTune(TEXT("clock.dawnHour"), 6.f);
}

float AWRGameState::GetDaylight() const
{
	return DaylightAt(GetHour());
}

float AWRGameState::DaylightAt(float Hour)
{
	// Full day an hour (clock.twilightHours) after dawn until that long before dusk, dark from dusk to dawn.
	const float Dawn = WRTune(TEXT("clock.dawnHour"), 6.f);
	const float Dusk = WRTune(TEXT("clock.duskHour"), 20.f);
	const float Ramp = FMath::Max(0.1f, WRTune(TEXT("clock.twilightHours"), 1.f));
	if (Hour < Dawn || Hour >= Dusk)
	{
		return 0.f;
	}
	const float Up = FMath::Clamp((Hour - Dawn) / Ramp, 0.f, 1.f);
	const float Down = FMath::Clamp((Dusk - Hour) / Ramp, 0.f, 1.f);
	return FMath::SmoothStep(0.f, 1.f, FMath::Min(Up, Down));
}

void AWRGameState::SetHour(float Hour)
{
	const double Now = GetTotalHours();
	double Target = FMath::FloorToDouble(Now / 24.0) * 24.0 + FMath::Fmod(FMath::Max(0.f, Hour), 24.f);
	if (Target < Now)
	{
		Target += 24.0;  // already past that hour today: tomorrow's
	}
	SetTotalHours(Target);
}

void AWRGameState::SetTotalHours(double Hours)
{
	if (!HasAuthority())
	{
		return;
	}
	ClockAnchorHours = FMath::Max(0.0, Hours);
	ClockAnchorTime = GetServerWorldTimeSeconds();
	bClockSet = true;
	UE_LOG(LogWildreach, Log, TEXT("CLOCK day %d %05.2f"), GetDay(), GetHour());
}

const AWRGameState* AWRGameState::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetGameState<AWRGameState>() : nullptr;
}

float AWRGameState::HourIn(const UObject* WorldContext)
{
	const AWRGameState* State = Get(WorldContext);
	return State ? State->GetHour() : 12.f;
}

bool AWRGameState::IsNightIn(const UObject* WorldContext)
{
	const AWRGameState* State = Get(WorldContext);
	return State && State->IsNight();
}
