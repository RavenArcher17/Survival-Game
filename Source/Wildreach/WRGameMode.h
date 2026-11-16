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

	/** Picks the world this game is (WRWorlds), then loads its settings and clans. */
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	/** Refuses a join whose name belongs to someone else on this world (WRIdentity: the name's key or platform account). */
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal) override;
	/** A joiner whose held slot lapsed while they loaded, into a world that has filled meanwhile, waits in the queue (WRAccess). */
	virtual APlayerController* Login(UPlayer* NewPlayer, ENetRole InRemoteRole, const FString& Portal, const FString& Options, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
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
	 * Persistence: one row per character in the world store (WRStore: Saved/Worlds/<world>/world.db, SQLite; the
	 * characters table), keyed by player name, which WRIdentity ties to a PC's key. Autosaves every 10 s, and on logout
	 * and server shutdown (-wrstore=json: one JSON file per character in players/, as before 0.8).
	 */
	void SavePlayer(class AWRPlayerState* PS);
	void LoadPlayer(class AWRPlayerState* PS);
	void SaveAllPlayers(bool bOnlyDirty);
	/** The autosave's turn (every second): the players whose turn it is, round robin, so each is saved every
	 *  net.save.playerEverySeconds and never all in one frame (cooks/build-wild-100players.md 3.8). */
	void AutosaveStep();
	/** "" when this game runs no world (the menu's island). */
	static FString PlayerSavePath(const FString& PlayerName);

	/** Everything players have built in this world, in Saved/Worlds/<world>/world.json (also counts its play time).
	 *  Now, whole (shutdown, the admin's save, a sliced save in flight is dropped for this one). */
	void SaveWorld();
	/** The periodic save, sliced: net.save.piecesPerFrame pieces a frame into JSON, then the rest and the write. */
	void BeginSlicedWorldSave();
	void LoadWorld();
	FString WorldSavePath() const;

	/** The host changed this world's settings: saved, in force at once and sent to everyone. The clock keeps its time. */
	void ChangeWorldSettings(const struct FWRWorldSettings& Settings);
	/** Tells the other players (when the host is done, not at each change). */
	void AnnounceWorldSettings();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	/** Puts anyone who fell through the island back on top of it (checked once a second). */
	void RescueUnderground();

	FTimerHandle AutosaveTimer;
	FTimerHandle WorldSaveTimer;
	FTimerHandle UndergroundTimer;
	FTimerHandle WildlifeTimer;
	FTimerHandle RaidTimer;
	FTimerHandle RaidingTimer;
	FTimerHandle RoamingBossTimer;
	FTimerHandle AccessTimer;
	/** The autosave's place in the player list. */
	int32 AutosaveCursor = 0;
	/** The sliced world save in flight. */
	bool bSliceSaving = false;
	int32 SliceCursor = 0;
	TArray<TWeakObjectPtr<class AWRBuildPiece>> SliceList;
	TArray<TSharedPtr<class FJsonValue>> SlicePieces;
	void StepSlicedWorldSave();
	/** Tames, eggs, the clock and the other documents with these pieces, then the write. */
	void FinishWorldSave(TArray<TSharedPtr<class FJsonValue>>&& Pieces);
	static bool ShouldSavePiece(const class AWRBuildPiece* Piece);
	/** The roaming minis: when each may come out again, and which have been out this session. */
	void StepRoamingBosses();
	TMap<FName, double> RoamingBossBack;
	TSet<FName> RoamingBossSeen;
	/** Adds to the world's play time (WRWorlds::Touch). Leaving: a player on their way out, who doesn't count from now on. */
	void CountPlayTime(const AController* Leaving = nullptr);
	/** When the world's play time was last counted (FPlatformTime), and whether anyone has been here since. */
	double PlayCountedAt = 0.0;
	bool bPlayersHere = false;
};
