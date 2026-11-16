#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class APlayerController;
class SEditableTextBox;

/** One-line chat input (T): Enter sends to everyone, Escape cancels. */
class WILDREACH_API SWRChatBox : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWRChatBox) {}
		SLATE_ARGUMENT(TWeakObjectPtr<APlayerController>, Owner)
	SLATE_END_ARGS()
	void Construct(const FArguments& InArgs);
	static void Open(APlayerController* PC);
	static void Close();
	static bool IsOpen() { return Instance.IsValid(); }

	/** Tab in the line: the next channel (cooks/cookpass-multiplayer.md 3.3). */
	virtual FReply OnPreviewKeyDown(const FGeometry& Geo, const FKeyEvent& Key) override;

private:
	void OnCommitted(const FText& Text, ETextCommit::Type How);
	/** The channel the chip shows (WRChat::EChannel: everyone, nearby, clan), kept between lines; the next one. */
	static uint8 Channel;
	void NextChannel();
	bool InClan() const;
	/** The line as sent: the chip's channel in front, unless it starts with its own (/w name, /r, /g...). */
	FString WithChannel(const FString& Text) const;
	static FText ChannelName(uint8 InChannel);
	TWeakObjectPtr<APlayerController> Owner;
	TSharedPtr<SEditableTextBox> Box;
	static TSharedPtr<SWidget> Instance;
};

/**
 * Front end shown when the game runs on its own (not connected to a server): pick a name and a main
 * path (fire / frost sets your spawn biome), then Host (choose a world, WRUIWorlds, and your game becomes
 * its server on UDP 7777) or Join a friend by IP. Choices are remembered in GameUserSettings.ini. Settings
 * opens the same settings window as Esc in game (WRUISettings).
 */
class WILDREACH_API SWRMainMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWRMainMenu) {}
		SLATE_ARGUMENT(TWeakObjectPtr<APlayerController>, Owner)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SWRMainMenu() override;
	virtual void Tick(const FGeometry& Geo, const double Time, const float Delta) override;
	/** Esc closes the card over the menu (a disconnect, a crash). */
	virtual FReply OnKeyDown(const FGeometry& Geo, const FKeyEvent& E) override;
	/** Open() focuses the menu (UI-only input). */
	virtual bool SupportsKeyboardFocus() const override { return true; }

	/** Show the menu over the current world for this local player. */
	static void Open(APlayerController* PC);
	static void Close();
	static bool IsOpen();
	/** Open or close the settings window over the menu (when the menu is open). */
	static void ShowSettings(bool bShow);

	/** This build's version (ProjectVersion in DefaultGame.ini; releases are tagged v<version>). */
	static FString GameVersion();
	/** "v0.2.1" is newer than "0.2.0"; a tag that isn't a version number never is. */
	static bool IsNewerVersion(const FString& Tag, const FString& Than);
	/** Remember why a join or game ended badly (a different version than the host's...) to show when the menu comes back. */
	static void WatchNetworkFailures();
	/** A failure for the card: its title, the plain line, whether Rejoin can help. */
	static void NoteFailure(const FString& Title, const FString& Line, bool bRejoin);
	/** Why this game is about to end (kicked, banned, the host closing the world: cooks/cookpass-multiplayer.md 3.4): the
	 *  card that follows says it, with no Rejoin, instead of "lost the connection". */
	static void NoteLeaving(const FString& Title, const FString& Line);
	/** The look picked for the next join ([Wildreach.Menu] Look = <face>.<build>[.h], the ?Look= option): face index,
	 *  build index (0 lean, 1 athletic, 2 strong) and whether the helmet is hidden. Defaults 0, 1, shown. */
	static void GetLook(int32& Face, int32& Build, bool& bHideHelmet);
	static void SetLook(int32 Face, int32 Build, bool bHideHelmet);
	/** The carved WILDREACH title and its tagline (the menu and the loading screen show it). */
	static TSharedRef<SWidget> MakeTitle();

private:
	FReply OnHost();
	/** Host this world (Choose a world's Play). */
	void HostWorld(const FString& WorldId);
	FReply OnJoin();
	FReply OnQuit();
	FReply OnUpdate();
	/** Once per run (packaged game, or -wrupdatefeed=<url>): ask GitHub for the newest release (sets LatestVersion). */
	static void CheckForUpdate();
	static FString LatestVersion;
	static FString LastFailure;
	FReply OnPickPath(FName Path);
	/** The main path choices, one per Data/paths file. */
	TSharedRef<SWidget> MakePathPicker();
	FReply OnPickBody(FName InBody);

	/**
	 * The look picker (cookpass-ui 3.5, the hero look's ?Look=): a Look row under Body opens a side panel with the
	 * faces by name, the three builds and the helmet; while it's open the menu's camera frames your survivor (the menu's
	 * own standalone world: AWRCharacter::PreviewLook), lit by a soft key light, so every pick is seen at once. Saved to
	 * [Wildreach.Menu] Look like the settings' helmet switch.
	 */
	TSharedRef<SWidget> MakeLookPanel();
	void ApplyPreview();
	void FrameSurvivor(float DeltaTime);
	void EndFraming();
	/** The face names for the body picked (player_art.json hero.<m|f>.faces[].name). */
	TArray<FString> FaceNames() const;
	FText LookSummary() const;
	bool bLookOpen = false;
	int32 LookFace = 0, LookBuild = 1;
	bool bLookHideHelmet = false;
	TWeakObjectPtr<class UPointLightComponent> LookLight;
	FString Options() const;
	void Remember() const;
	TSharedRef<SWidget> MakePanel();
	TSharedRef<SWidget> MakeTip();
	TSharedRef<SWidget> MakeUpdate();

	/**
	 * The card over the menu (cookpass-ui 3.3): after a network failure (a plain line for what happened and, when this
	 * game was joined to a host, Rejoin: it tries the same address again up to ui.rejoin.tries times within
	 * ui.rejoin.windowSeconds, counting down between tries) or after a crash on the last run (send its report, or not now).
	 * Mouse, keys (Esc closes) and pad (the D-pad reaches its buttons, B closes).
	 */
	TSharedRef<SWidget> MakeCard();
	void CloseCard();
	void Rejoin();
	static int32 CardKind;        // 0 none, 1 a network failure, 2 a crash on the last run
	static FString CardTitle;
	static bool bCanRejoin;
	static int32 RejoinTries;     // tries in this run of Rejoin presses / countdowns
	static double RejoinFirstAt;
	static double LastRejoinAt;
	static double NextRejoinAt;   // > 0: a countdown to the next try
	static double LastFailAt;
	static bool bJoining;         // the last thing the menu did was join a host (so a travel failure can be rejoined)
	/** A kick, ban or closing world's word, and when it came (FPlatformTime). */
	static FString LeaveTitle;
	static FString LeaveLine;
	static double LeaveAt;
	/** The host asked for a password: the card has a box for it. What was typed (this run only, never saved) and the
	 *  address it's for; the join sends its wire hash (WRAccess::WireHash). */
	static bool bNeedPassword;
	static FString TypedPassword;
	static FString PasswordAddress;
	TSharedPtr<SEditableTextBox> PasswordBox;
	void JoinWithPassword();
	TSharedPtr<SWidget> CardPanel;   // the card itself: the pad's D-pad navigates inside it while it shows
	bool bCardNav = false;

	TWeakObjectPtr<APlayerController> Owner;
	TSharedPtr<SEditableTextBox> NameBox;
	TSharedPtr<SEditableTextBox> AddressBox;
	TSharedPtr<class SBox> SettingsHost;
	FName Path = TEXT("fire");
	FName Body;  // None = the default (male) body, "f" = the female body (Data/player_art.json "bodies")
	FText Status;
	int32 Tip = 0;
	double NextTip = 0.0;
	static TSharedPtr<SWidget> Instance;
	/** The backdrop: a fixed scenic camera over the island (flow.menuCamera in tuning.json) instead of the spawn. */
	static TWeakObjectPtr<class ACameraActor> Backdrop;
	static void ShowBackdrop(APlayerController* PC, bool bShow);
};

namespace WRMenuUI
{
	/** A tip with a real key in it: "Tip: hold [Q] to open your ability wheel" (the menu and the loading screen). */
	struct FTip { FText Before; const TCHAR* Action; FText After; };
	WILDREACH_API const TArray<FTip>& Tips();
}
