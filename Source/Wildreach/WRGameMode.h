#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "WRGameMode.generated.h"

UCLASS()
class WILDREACH_API AWRGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AWRGameMode();

	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	/** Restart the given controller's pawn after RespawnDelay seconds. */
	void RequestRespawn(AController* Controller);

	UPROPERTY(EditDefaultsOnly, Category="Respawn")
	float RespawnDelay = 5.f;
};
