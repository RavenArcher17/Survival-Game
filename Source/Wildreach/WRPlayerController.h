#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "WRPlayerController.generated.h"

class AWRCharacterBase;
class AWRCharacter;
class AWRStation;
class AWRHearth;

enum class EWRMenu : uint8
{
	None,
	Craft,
	Inventory,
	Build,
	Chest,
	Claim,
	Tame,
	Skills,
};

class AWRBuildPiece;

/** One line of a menu and the server command it runs (or empty for a header line). */
struct FWRMenuEntry
{
	FString Label;
	FString Command;
	bool bAvailable = true;
};

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
	AWRStation* FindNearbyStation() const;
	AWRHearth* FindNearbyHearth() const;

	// --- Menus (client-side state; actions go to the server as validated commands) ---
	EWRMenu GetMenu() const { return Menu; }
	int32 GetMenuIndex() const { return MenuIndex; }
	FName GetMenuElement() const { return MenuElement; }
	FString GetMenuTitle() const;
	TArray<FWRMenuEntry> BuildMenuEntries() const;

	UFUNCTION(Exec) void WRCraft(FName Recipe, FName Element);
	UFUNCTION(Exec) void WRUseSlot(int32 Slot);
	UFUNCTION(Exec) void WRRepair(FName Slot);
	UFUNCTION(Exec) void WRUpgrade(FName Slot);
	UFUNCTION(Exec) void WRUnequip(FName Slot);
	/** Use the first inventory slot holding this item (testing). */
	UFUNCTION(Exec) void WREquip(FName Item);
	/** Teleport next to the nearest station of a type, or "hearth" (testing). */
	UFUNCTION(Exec) void WRGoTo(FName What, FName Extra);
	UFUNCTION(Exec) void WRTame(FName Order);
	UFUNCTION(Exec) void WRDismount();
	UFUNCTION(Exec) void WRTorpor(float Amount);
	UFUNCTION(Exec) void WRTameStat();
	/** Dev: deal damage (optionally fire/frost) to the nearest boss as this player. */
	UFUNCTION(Exec) void WRBossDmg(float Amount, FName Element);
	UFUNCTION(Exec) void WRArtifacts(int32 bOn);
	UFUNCTION(Exec) void WRBossDmgPct(float Percent, FName Element);
	/** Clans: WRClan create|invite|join|leave|kick|info [name]. */
	UFUNCTION(Exec) void WRClan(const FString& Action, const FString& Name);
	UFUNCTION(Exec) void WRPerk(FName Skill, int32 TierLevel, int32 Option);
	/** Dev: set a life skill level / award XP. */
	UFUNCTION(Exec) void WRSkill(FName Skill, int32 Level);
	UFUNCTION(Exec) void WRSkillXP(FName Skill, int32 XP);
	UFUNCTION(Exec) void WRSkillStat(FName Stat);
	UFUNCTION(Exec) void WRHitPlayer(const FString& Name, float Damage);

	// --- Building ---
	bool IsPlacing() const { return !PlacingPiece.IsNone(); }
	FName GetPlacingPiece() const { return PlacingPiece; }
	bool IsPlacementValid() const { return bPlaceValid; }
	const FString& GetPlacementWhy() const { return PlaceWhy; }
	void ConfirmPlacement();
	void RotatePlacement();
	void CancelPlacement();
	AWRBuildPiece* FindNearbyPiece(float Range = 350.f) const;
	class AWRCreature* FindNearbyCreature(bool bOwnTame, float Range = 450.f) const;

	/** Place a piece at a world spot (testing; normal play uses the build ghost). */
	UFUNCTION(Exec) void WRPlace(FName Piece, float X, float Y, float Z, float Yaw);
	UFUNCTION(Exec) void WRSetCode(const FString& Code);
	UFUNCTION(Exec) void WRCode(const FString& Code);
	UFUNCTION(Exec) void WRDemolish();
	/** N sword hits on the nearest building piece (testing raids/collapse). */
	UFUNCTION(Exec) void WRHitPiece(int32 Hits);

	/** Short message from the server ("Frost is now your subclass."), shown by the HUD for a few seconds. */
	const FString& GetNotice() const { return Notice; }
	double GetNoticeTime() const { return NoticeTime; }

	UFUNCTION(Client, Reliable) void ClientNotify(const FString& Message);

	UFUNCTION(Exec) void WRInteract();
	UFUNCTION(Exec) void WRGive(FName Item, int32 Count);
	/** Teleport to world X/Y (cm), dropped onto the ground. */
	UFUNCTION(Exec) void WRTeleport(float X, float Y);
	/** Set the camera direction (local). */
	UFUNCTION(Exec) void WRLook(float Yaw, float Pitch);
	/** Bot walks straight toward world X/Y (cm) and logs the time it takes, or where it got stuck. */
	UFUNCTION(Exec) void WRWalkTo(float X, float Y);
	void UsePotion();
	void UseFood();
	void UseDrink();
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
	/** Handles gameplay commands (crafting, inventory, interaction); false if Cmd isn't one. */
	bool RunGameplayCommand(const TArray<FString>& Args, FString& OutResult);

	void ToggleInventory();
	void ToggleSkills();
	void MenuUp();
	void MenuDown();
	void MenuConfirm();
	void MenuClose();
	void CycleInfuse();

	EWRMenu Menu = EWRMenu::None;
	int32 MenuIndex = 0;
	FName MenuElement;
	TWeakObjectPtr<AWRStation> MenuStation;
	TWeakObjectPtr<AWRBuildPiece> MenuPiece;
	TWeakObjectPtr<class AWRCreature> MenuCreature;

	void ToggleBuildMenu();
	void StartPlacing(FName Piece);
	void UpdatePlacement();
	TArray<FWRMenuEntry> BuildPieceMenu() const;
	bool RunBuildCommand(const TArray<FString>& Args, FString& OutResult);

	FName PlacingPiece;
	TWeakObjectPtr<AWRBuildPiece> Ghost;
	float PlaceYaw = 0.f;
	FVector PlaceLoc = FVector::ZeroVector;
	bool bPlaceValid = false;
	FString PlaceWhy;

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
	/** -wrbotgather: harvest resource nodes instead of fighting. */
	bool bBotGather = false;
	TArray<FString> BotCmdQueue;
	float BotCmdResume = 0.f;
	bool bBotPaused = false;
	bool bWalking = false;
	FVector2D WalkTarget = FVector2D::ZeroVector;
	float WalkStart = 0.f;
	float WalkLastProgress = 0.f;
	float WalkBestDist = 0.f;
	float WalkNextLog = 0.f;
	float BotClock = 0.f;
	float BotNextAttack = 0.f;
	float BotNextDodge = 3.f;
	float BotNextSpell = 2.f;
	float BotNextBlock = 4.f;
	float BotBlockAt = 0.f;
	float BotNextPotion = 0.f;
	float BotNextReport = 5.f;
	int32 BotSpellSlot = 0;
	TArray<TPair<int32, float>> BotHeld;
};
