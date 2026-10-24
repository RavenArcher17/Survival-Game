#include "WRGameState.h"
#include "Wildreach.h"
#include "WRTuning.h"
#include "Net/UnrealNetwork.h"

void AWRGameState::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		Preset = FWRTuning::Get().GetPreset();
		ServerSettings = FWRTuning::Get().ExportServerSettings();
	}
}

void AWRGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRGameState, Preset);
	DOREPLIFETIME(AWRGameState, ServerSettings);
}

void AWRGameState::OnRep_ServerSettings()
{
	FWRTuning::Get().ImportServerSettings(Preset, ServerSettings);
	UE_LOG(LogWildreach, Log, TEXT("Server settings from host: %s (%d values)"), *Preset, ServerSettings.Num());
}
