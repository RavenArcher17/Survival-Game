#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class APlayerController;
class SEditableTextBox;

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

private:
	FReply OnHost();
	FReply OnJoin();
	FReply OnQuit();
	FReply OnPickPath(FName Path);
	FString Options() const;
	void Remember() const;

	TWeakObjectPtr<APlayerController> Owner;
	TSharedPtr<SEditableTextBox> NameBox;
	TSharedPtr<SEditableTextBox> AddressBox;
	FName Path = TEXT("fire");
	FText Status;
	static TSharedPtr<SWidget> Instance;
};
