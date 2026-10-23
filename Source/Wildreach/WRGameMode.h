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

	virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	/** Restart the given controller's pawn after the respawn delay (tuning: player.respawnSeconds). */
	void RequestRespawn(AController* Controller);

	/**
	 * Persistence v1: one JSON file per player under Saved/ServerDB/players/, keyed by player name.
	 * Autosaves dirty players every 10 s, and on logout and server shutdown. SQLite + real
	 * account ids replace this in M7.
	 */
	void SavePlayer(class AWRPlayerState* PS);
	void LoadPlayer(class AWRPlayerState* PS);
	void SaveAllPlayers(bool bOnlyDirty);
	static FString PlayerSavePath(const FString& PlayerName);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	FTimerHandle AutosaveTimer;
};
