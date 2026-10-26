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

private:
	void OnCommitted(const FText& Text, ETextCommit::Type How);
	TWeakObjectPtr<APlayerController> Owner;
	TSharedPtr<SEditableTextBox> Box;
	static TSharedPtr<SWidget> Instance;
};

/**
 * Front end shown when the game runs on its own (not connected to a server): pick a name and a main
 * path (fire / frost sets your spawn biome), then Host (your game becomes the server on UDP 7777)
 * or Join a friend by IP. Choices are remembered in GameUserSettings.ini.
 */
class WILDREACH_API SWRMainMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWRMainMenu) {}
		SLATE_ARGUMENT(TWeakObjectPtr<APlayerController>, Owner)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Show the menu over the current world for this local player. */
	static void Open(APlayerController* PC);
	static void Close();
	static bool IsOpen();

	/** This build's version (ProjectVersion in DefaultGame.ini; releases are tagged v<version>). */
	static FString GameVersion();
	/** "v0.2.1" is newer than "0.2.0"; a tag that isn't a version number never is. */
	static bool IsNewerVersion(const FString& Tag, const FString& Than);
	/** Remember why a join or game ended badly (a different version than the host's...) to show when the menu comes back. */
	static void WatchNetworkFailures();

private:
	FReply OnHost();
	FReply OnJoin();
	FReply OnQuit();
	FReply OnUpdate();
	/** Once per run (packaged game, or -wrupdatefeed=<url>): ask GitHub for the newest release (sets LatestVersion). */
	static void CheckForUpdate();
	static FString LatestVersion;
	static FString LastFailure;
	FReply OnPickPath(FName Path);
	FReply OnPickBody(FName InBody);
	FString Options() const;
	void Remember() const;

	TWeakObjectPtr<APlayerController> Owner;
	TSharedPtr<SEditableTextBox> NameBox;
	TSharedPtr<SEditableTextBox> AddressBox;
	FName Path = TEXT("fire");
	FName Body;  // None = the default (male) body, "f" = the female body (Data/player_art.json "bodies")
	FText Status;
	static TSharedPtr<SWidget> Instance;
};
