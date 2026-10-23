#include "WRGameMode.h"
#include "Wildreach.h"
#include "WRCharacter.h"
#include "WRHUD.h"
#include "WRPathComponent.h"
#include "WRPathTypes.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRTuning.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

AWRGameMode::AWRGameMode()
{
	DefaultPawnClass = AWRCharacter::StaticClass();
	HUDClass = AWRHUD::StaticClass();
	PlayerStateClass = AWRPlayerState::StaticClass();
	PlayerControllerClass = AWRPlayerController::StaticClass();
}

FString AWRGameMode::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal)
{
	const FString Result = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);

	// Main path is picked at creation. Until there's a creation screen it comes from the
	// join URL (?Path=frost), defaulting to fire.
	FString Path = UGameplayStatics::ParseOption(Options, TEXT("Path")).ToLower();
	if (!FWRPathRegistry::Get().Find(FName(*Path)))
	{
		Path = TEXT("fire");
	}
	if (AWRPlayerState* PS = NewPlayerController ? NewPlayerController->GetPlayerState<AWRPlayerState>() : nullptr)
	{
		PS->GetPaths()->SetMainPath(FName(*Path));
	}
	return Result;
}

void AWRGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	const AWRPlayerState* PS = NewPlayer ? NewPlayer->GetPlayerState<AWRPlayerState>() : nullptr;
	UE_LOG(LogWildreach, Log, TEXT("Player joined: %s main=%s (%d connected)"),
		PS ? *PS->GetPlayerName() : TEXT("?"), PS ? *PS->GetPaths()->GetMainPath().ToString() : TEXT("?"), GetNumPlayers());
}

void AWRGameMode::Logout(AController* Exiting)
{
	UE_LOG(LogWildreach, Log, TEXT("Player left: %s"), *GetNameSafe(Exiting));
	Super::Logout(Exiting);
}

AActor* AWRGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	// Spawn in the main path's biome: player starts are tagged "volcanic" / "tundra".
	const AWRPlayerState* PS = Player ? Player->GetPlayerState<AWRPlayerState>() : nullptr;
	const FWRPathDef* Def = PS ? FWRPathRegistry::Get().Find(PS->GetPaths()->GetMainPath()) : nullptr;
	if (Def)
	{
		TArray<APlayerStart*> Matching;
		for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
		{
			if (It->PlayerStartTag == Def->Biome)
			{
				Matching.Add(*It);
			}
		}
		if (Matching.Num() > 0)
		{
			return Matching[FMath::RandRange(0, Matching.Num() - 1)];
		}
	}
	return Super::ChoosePlayerStart_Implementation(Player);
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
	}), WRTune(TEXT("player.respawnSeconds"), 10.f), false);
}
