#include "WRGameMode.h"
#include "Wildreach.h"
#include "WRCharacter.h"
#include "WRHUD.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"

AWRGameMode::AWRGameMode()
{
	DefaultPawnClass = AWRCharacter::StaticClass();
	HUDClass = AWRHUD::StaticClass();
}

void AWRGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	UE_LOG(LogWildreach, Log, TEXT("Player joined: %s (%d connected)"),
		NewPlayer && NewPlayer->PlayerState ? *NewPlayer->PlayerState->GetPlayerName() : TEXT("?"), GetNumPlayers());
}

void AWRGameMode::Logout(AController* Exiting)
{
	UE_LOG(LogWildreach, Log, TEXT("Player left: %s"), *GetNameSafe(Exiting));
	Super::Logout(Exiting);
}

void AWRGameMode::RequestRespawn(AController* Controller)
{
	if (!Controller)
	{
		return;
	}

	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, WeakController = TWeakObjectPtr<AController>(Controller)]()
	{
		if (AController* C = WeakController.Get())
		{
			RestartPlayer(C);
		}
	}), RespawnDelay, false);
}
