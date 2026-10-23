#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "WRPlayerController.generated.h"

class AWRCharacterBase;
class AWRCharacter;

/**
 * Lock-on targeting, developer commands (WR*), and a scripted test bot (-wrbot) used
 * by headless multiplayer tests.
 */
UCLASS()
class WILDREACH_API AWRPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AWRPlayerController();

	virtual void PlayerTick(float DeltaTime) override;
	virtual void SetupInputComponent() override;

	AWRCharacterBase* GetLockTarget() const { return LockTarget.Get(); }

	/** Nearest altar in interact range (client-side, for the HUD prompt). */
	class AWRBossAltar* FindNearbyAltar() const;

	/** Short message from the server ("Frost is now your subclass."), shown by the HUD for a few seconds. */
	const FString& GetNotice() const { return Notice; }
	double GetNoticeTime() const { return NoticeTime; }

	UFUNCTION(Client, Reliable) void ClientNotify(const FString& Message);

	UFUNCTION(Exec) void WRInteract();
	/** First or third person (same as V). */
	UFUNCTION(Exec) void WRView(FName Mode);
	/** Stop/start the test bot's own actions (it keeps running queued commands). */
	UFUNCTION(Exec) void WRBotPause(int32 bPause);
	/** Teleport next to the altar of a path (testing). */
	UFUNCTION(Exec) void WRGoToAltar(FName Path);

	// --- Developer commands (console, or -ExecCmds) ---
	UFUNCTION(Exec) void WRSetMain(FName Path);
	UFUNCTION(Exec) void WRSetSub(FName Path);
	UFUNCTION(Exec) void WRGivePoints(FName Path, int32 Points);
	UFUNCTION(Exec) void WRAutoSpend(FName Path, int32 Points);
	UFUNCTION(Exec) void WRBuy(FName Path, FName Node);
	UFUNCTION(Exec) void WRWeapon(FName Metal, FName Quality, int32 Upgrade);
	UFUNCTION(Exec) void WRSpawn(FName Creature, int32 Count);
	UFUNCTION(Exec) void WRStatus(FName Status);
	UFUNCTION(Exec) void WRHeal();
	UFUNCTION(Exec) void WRKillMe();
	UFUNCTION(Exec) void WRStarve();

private:
	UFUNCTION(Server, Reliable) void ServerCommand(const FString& Command);
	void RunServerCommand(const FString& Command);

	void ToggleLockOn();
	void UpdateLockOn(float DeltaTime);
	AWRCharacterBase* FindTarget(float MaxDistance, float MaxAngleDegrees) const;

	void TickBot(float DeltaTime);
	void BotPress(int32 Input, float HoldSeconds);

	TWeakObjectPtr<AWRCharacterBase> LockTarget;
	FString Notice;
	double NoticeTime = -100.0;

	bool bBot = false;
	bool bBotPvP = false;
	/** -wrbothunt: ignore training dummies, go after creature packs. */
	bool bBotHunt = false;
	bool bBotSetupDone = false;
	TArray<FString> BotCmdQueue;
	float BotCmdResume = 0.f;
	bool bBotPaused = false;
	float BotClock = 0.f;
	float BotNextAttack = 0.f;
	float BotNextDodge = 3.f;
	float BotNextSpell = 2.f;
	float BotNextBlock = 4.f;
	float BotBlockAt = 0.f;
	float BotNextReport = 5.f;
	int32 BotSpellSlot = 0;
	TArray<TPair<int32, float>> BotHeld;
};
