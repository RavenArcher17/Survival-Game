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

	/**
	 * A player died: they wake when they press Wake on the death screen's map, once player.respawnSeconds have passed
	 * (player.autoWakeSeconds > 0 wakes them at their pick after that long anyway).
	 */
	void RequestRespawn(AController* Controller);
	/** Bring a dead player back at the place they picked; false if they aren't waiting to wake or it's too soon. */
	bool Wake(AController* Controller);

	/**
	 * Persistence v1: one JSON file per player under Saved/ServerDB/players/, keyed by player name.
	 * Autosaves dirty players every 10 s, and on logout and server shutdown. SQLite + real
	 * account ids replace this in M7.
	 */
	void SavePlayer(class AWRPlayerState* PS);
	void LoadPlayer(class AWRPlayerState* PS);
	void SaveAllPlayers(bool bOnlyDirty);
	static FString PlayerSavePath(const FString& PlayerName);

	/** Everything players have built on this map, in Saved/ServerDB/world_<map>.json. */
	void SaveWorld();
	void LoadWorld();
	FString WorldSavePath() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	/** Puts anyone who fell through the island back on top of it (checked once a second). */
	void RescueUnderground();

	FTimerHandle AutosaveTimer;
	FTimerHandle WorldSaveTimer;
	FTimerHandle UndergroundTimer;
};
