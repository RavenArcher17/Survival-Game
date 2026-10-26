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
	Taming,    // a knocked-out wild creature: give food, kibble, narcotics
	Skills,
	Paths,
	Map,       // the journal's map tab
	Hearth,    // the Hearth: tech level, cores, unlocks
	Settings,  // settings: controls (key bindings) and more
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
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	/** The stone-and-leather UI (local players; null on servers and with -oldui). */
	class FWRPlayerUI* GetUI() const { return UI.Get(); }
	/** Open a menu / journal tab (None closes it). */
	void SetMenu(EWRMenu NewMenu);
	/** Send a gameplay command to the server ("slot 3", "craft hunting_bow none", "store 4 20"...). */
	void SendCommand(const FString& Command) { ServerCommand(Command); }
	/** A notice shown like the server's, without a round trip. */
	void ShowLocalNotice(const FString& Message);
	/** What E does right now (the same as a tap of it). */
	void InteractNow() { WRInteract(); }
	AWRStation* GetMenuStation() const { return MenuStation.Get(); }
	AWRBuildPiece* GetMenuPiece() const { return MenuPiece.Get(); }
	class AWRCreature* GetMenuCreature() const { return MenuCreature.Get(); }
	/** Building: snap pieces to each other (F). */
	bool IsSnapOn() const { return bSnap; }
	/** F10: leave the game and go back to the main menu. */
	void ReturnToMenu();

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
	/** Every tame of yours within 30 m takes the order (follow, stay, aggressive, passive). */
	UFUNCTION(Exec) void WRTameAll(FName Order);
	UFUNCTION(Exec) void WRDismount();
	UFUNCTION(Exec) void WRTorpor(float Amount);
	UFUNCTION(Exec) void WRTameStat();
	/** Knockout taming (the Taming menu's lines): all of a food into the nearest knocked-out creature's stash / a narcotic. */
	UFUNCTION(Exec) void WRTameGive(FName Item);
	UFUNCTION(Exec) void WRNarcotic();
	/** Dev/test: send a gameplay command to the server as a screen's button does ("WRCmd hearthfeed", "WRCmd claimdeposit"). */
	UFUNCTION(Exec) void WRCmd(const FString& Command);
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
	/** Dev: what biome hazard is underfoot, and does it affect me. */
	UFUNCTION(Exec) void WRHazard();
	/** Dev/test: press the ability in quick slot 1-8 (as the number keys do). */
	UFUNCTION(Exec) void WRCast(int32 Slot);
	/** Dev/test: learn a tree node through the same server command the Paths menu sends. */
	UFUNCTION(Exec) void WRLearn(FName Path, FName Node);
	/** Dev/test: chat a message; toggle the map list. */
	UFUNCTION(Exec) void WRSay(const FString& Message);
	UFUNCTION(Exec) void WRMap();

	// --- Building ---
	bool IsPlacing() const { return !PlacingPiece.IsNone(); }
	FName GetPlacingPiece() const { return PlacingPiece; }
	bool IsPlacementValid() const { return bPlaceValid; }
	const FString& GetPlacementWhy() const { return PlaceWhy; }
	void ConfirmPlacement();
	void RotatePlacement();
	void CancelPlacement();
	/** Nearest built piece in range; bUsableOnly skips walls, floors, decor and stations (what E can't use on the piece itself). */
	AWRBuildPiece* FindNearbyPiece(float Range = 350.f, bool bUsableOnly = false) const;
	/** What E uses (and the prompt shows): the nearer of the nearest usable piece (door, chest, bed, crop, claim) and station. */
	AActor* FindUseTarget() const;
	class AWRCreature* FindNearbyCreature(bool bOwnTame, float Range = 450.f) const;

	/** Place a piece at a world spot (testing; normal play uses the build ghost). */
	UFUNCTION(Exec) void WRPlace(FName Piece, float X, float Y, float Z, float Yaw);
	UFUNCTION(Exec) void WRSetCode(const FString& Code);
	UFUNCTION(Exec) void WRCode(const FString& Code);
	/** Remove your piece standing at X Y Z (no arguments: the nearest one); half the cost back. */
	UFUNCTION(Exec) void WRDemolish(float X = 0.f, float Y = 0.f, float Z = 0.f);
	/** Bots: the build ghost as a player gets it (B menu pick): WRGhost <piece>, aim with WRLook, then WRGhostPlace logs where it snapped and places it. */
	UFUNCTION(Exec) void WRGhost(FName Piece);
	UFUNCTION(Exec) void WRGhostPlace();
	/** N sword hits on the nearest building piece (testing raids/collapse). */
	UFUNCTION(Exec) void WRHitPiece(int32 Hits);

	/** Short message from the server ("Frost is now your subclass."), shown by the HUD for a few seconds. */
	const FString& GetNotice() const { return Notice; }
	double GetNoticeTime() const { return NoticeTime; }

	UFUNCTION(Client, Reliable) void ClientNotify(const FString& Message);

	UFUNCTION(Exec) void WRInteract();
	/** Testing / screenshot aid: open a menu (inventory, build, skills, paths) on line Select, toggle the map (map),
	 *  press Enter on the selected line (confirm) or close it (none). Tools\menushots.ps1 uses it. */
	UFUNCTION(Exec) void WRMenu(FName Which, int32 Select);
	/** Testing / screenshot aid: hold a wheel open with a wedge lit (0, 1 ability wheels, 2 tame orders; -1 closes); toggle the controls help. */
	UFUNCTION(Exec) void WRWheel(int32 Which, int32 Hover);
	UFUNCTION(Exec) void WRHelp();
	UFUNCTION(Exec) void WRGive(FName Item, int32 Count);
	/** Ranged weapon (bots): out (1) / away (0), aim, fire, cycle ammo, face the nearest creature of an id. */
	UFUNCTION(Exec) void WRRanged(int32 bOn);
	UFUNCTION(Exec) void WRAim(int32 bOn);
	UFUNCTION(Exec) void WRFire();
	UFUNCTION(Exec) void WRAmmo(int32 Dir);
	UFUNCTION(Exec) void WRAimAt(FName Creature);
	/** Testing aid: root the wild creatures within 30 m for some seconds / remove them and the training dummies. */
	UFUNCTION(Exec) void WRHold(float Seconds);
	UFUNCTION(Exec) void WRClearCreatures();
	/** Set the world clock to an hour (0-24; jumps forward to the next time it's that hour). */
	UFUNCTION(Exec) void WRHour(float Hour);
	/** Log the island wildlife: sites, how many packs are out, what lives within 300 m, the nearest quiet site. */
	UFUNCTION(Exec) void WRWildlife();
	/** Testing aid: spawn a creature at world X/Y (on the ground) / set the nearest wild one of an id to a fraction of its health. */
	UFUNCTION(Exec) void WRSpawnAt(FName Creature, float X, float Y);
	UFUNCTION(Exec) void WRCreatureHP(FName Creature, float Fraction);
	/** Log what this machine's sky shows: hour, sun and moon height and light, exposure (SKY ...). */
	UFUNCTION(Exec) void WRSky();
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
	/** Bot: walk to the nearest tree / rock / shrub (the island's), node (resource nodes) or any, and gather with light attacks; off stops. */
	UFUNCTION(Exec) void WRGather(const FString& What);
	/** Art check: frame the nearest creature of this kind ("me" = your own body) from Yaw degrees off its nose and follow it ("off" = back to the pawn). */
	UFUNCTION(Exec) void WRArtCam(FName Creature, float Yaw = 60.f);
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
	void TogglePaths();
public:
	void ToggleMap();
	void OpenChat();
	bool IsMapShown() const { return bShowMap; }

	/** Chat: everyone on the server sees it. */
	UFUNCTION(Server, Reliable) void ServerChat(const FString& Message);
	UFUNCTION(Client, Reliable) void ClientChat(const FString& From, const FString& Message);
	struct FChatLine { FString Text; double Time; };
	const TArray<FChatLine>& GetChat() const { return Chat; }
private:
	void MenuUp();
	void MenuDown();
	void MenuConfirm();
	/** Enter: the old text menu's confirm, unless a stone-and-leather screen (with its own buttons) is open. */
	void MenuConfirmKey();
	void MenuClose();
	void CycleInfuse();

	/** (Re)bind every key from FWRKeyBindings (called again when a binding changes). */
	void BindKeys();
	void InteractPressed();
	void InteractReleased();
	void OpenJournal(EWRMenu Tab);
	/** Esc: close what's open (a wheel, a menu, the build ghost); with nothing open, the game menu (settings). */
	void EscapePressed();
	void ToggleSnap() { bSnap = !bSnap; }
	TSharedPtr<class FWRPlayerUI> UI;
	FDelegateHandle KeysChangedHandle;
	bool bSnap = true;

	EWRMenu Menu = EWRMenu::None;
	bool bShowMap = false;
	TArray<FChatLine> Chat;
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
	void UpdateArtCam(float DeltaTime);
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
	/** What the gathering bot goes for (WRGather): node, tree, rock, shrub or any. */
	FString BotGatherGroup = TEXT("node");
	TArray<FString> BotCmdQueue;
	float BotCmdResume = 0.f;
	bool bBotPaused = false;
	TWeakObjectPtr<AActor> ArtCamTarget;
	UPROPERTY() TObjectPtr<class ACameraActor> ArtCam;
	float ArtCamYaw = 60.f;
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
