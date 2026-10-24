#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "WRGameState.generated.h"

/** Replicates the server's settings (preset + every server.* value) so client-side rules match: lock-on, PvP, HUD. */
UCLASS()
class WILDREACH_API AWRGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(ReplicatedUsing=OnRep_ServerSettings) FString Preset;
	UPROPERTY(ReplicatedUsing=OnRep_ServerSettings) TArray<FString> ServerSettings;

private:
	UFUNCTION() void OnRep_ServerSettings();
};
