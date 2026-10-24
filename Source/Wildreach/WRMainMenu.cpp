#include "WRMainMenu.h"
#include "Wildreach.h"
#include "WRPlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/ConfigCacheIni.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

TSharedPtr<SWidget> SWRMainMenu::Instance;
TSharedPtr<SWidget> SWRChatBox::Instance;

void SWRChatBox::Construct(const FArguments& InArgs)
{
	Owner = InArgs._Owner;
	ChildSlot.HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(FMargin(20.f, 0.f, 0.f, 240.f))
	[
		SNew(SBox).WidthOverride(520.f)
		[
			SAssignNew(Box, SEditableTextBox).HintText(FText::FromString(TEXT("Say something (Enter to send, Esc to cancel)")))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 13)).OnTextCommitted(this, &SWRChatBox::OnCommitted)
		]
	];
}

void SWRChatBox::OnCommitted(const FText& Text, ETextCommit::Type How)
{
	if (How == ETextCommit::OnEnter && !Text.IsEmpty())
	{
		if (AWRPlayerController* PC = Cast<AWRPlayerController>(Owner.Get()))
		{
			PC->ServerChat(Text.ToString().Left(200));
		}
	}
	Close();
}

void SWRChatBox::Open(APlayerController* PC)
{
	if (!PC || !GEngine || !GEngine->GameViewport || Instance.IsValid() || SWRMainMenu::IsOpen())
	{
		return;
	}
	TSharedRef<SWRChatBox> Widget = SNew(SWRChatBox).Owner(PC);
	Instance = Widget;
	GEngine->GameViewport->AddViewportWidgetContent(Widget, 90);
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(Widget->Box);
	PC->SetInputMode(Mode);
	FSlateApplication::Get().SetKeyboardFocus(Widget->Box);
}

void SWRChatBox::Close()
{
	if (Instance.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(Instance.ToSharedRef());
		if (APlayerController* PC = GEngine->GameViewport->GetWorld() ? GEngine->GameViewport->GetWorld()->GetFirstPlayerController() : nullptr)
		{
			PC->SetInputMode(FInputModeGameOnly());
		}
	}
	Instance.Reset();
}

static const TCHAR* MenuSection = TEXT("Wildreach.Menu");

void SWRMainMenu::Construct(const FArguments& InArgs)
{
	Owner = InArgs._Owner;
	FString Name = TEXT("Survivor"), Address = TEXT("127.0.0.1"), SavedPath = TEXT("fire");
	GConfig->GetString(MenuSection, TEXT("Name"), Name, GGameUserSettingsIni);
	GConfig->GetString(MenuSection, TEXT("Address"), Address, GGameUserSettingsIni);
	GConfig->GetString(MenuSection, TEXT("Path"), SavedPath, GGameUserSettingsIni);
	Path = FName(*SavedPath);

	const auto Label = [](const FString& Text, int32 Size = 14)
	{
		return SNew(STextBlock).Text(FText::FromString(Text)).Font(FCoreStyle::GetDefaultFontStyle("Regular", Size)).ColorAndOpacity(FLinearColor::White);
	};
	const auto Button = [](const FString& Text, FOnClicked Click)
	{
		return SNew(SButton).OnClicked(Click).HAlign(HAlign_Center).ContentPadding(FMargin(18.f, 8.f))
			[SNew(STextBlock).Text(FText::FromString(Text)).Font(FCoreStyle::GetDefaultFontStyle("Bold", 15))];
	};

	ChildSlot
	.HAlign(HAlign_Center).VAlign(VAlign_Center)
	[
		SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.03f, 0.88f)).Padding(32.f)
		[
			SNew(SBox).WidthOverride(520.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 18)[Label(TEXT("WILDREACH"), 34)]
				+ SVerticalBox::Slot().AutoHeight()[Label(TEXT("Your name (your character and progress are saved under it)"), 11)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 14)[SAssignNew(NameBox, SEditableTextBox).Text(FText::FromString(Name)).Font(FCoreStyle::GetDefaultFontStyle("Regular", 15))]
				+ SVerticalBox::Slot().AutoHeight()[Label(TEXT("Main path (sets where you spawn; the other is a subclass from a boss altar)"), 11)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 14)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 6, 0)
					[
						SNew(SButton).OnClicked(this, &SWRMainMenu::OnPickPath, FName(TEXT("fire"))).HAlign(HAlign_Center).ContentPadding(FMargin(10.f, 8.f))
						.ButtonColorAndOpacity_Lambda([this]() { return Path == TEXT("fire") ? FLinearColor(1.f, 0.4f, 0.1f) : FLinearColor(0.3f, 0.3f, 0.3f); })
						[SNew(STextBlock).Text(FText::FromString(TEXT("Fire - Volcanic"))).Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))]
					]
					+ SHorizontalBox::Slot().FillWidth(1).Padding(6, 0, 0, 0)
					[
						SNew(SButton).OnClicked(this, &SWRMainMenu::OnPickPath, FName(TEXT("frost"))).HAlign(HAlign_Center).ContentPadding(FMargin(10.f, 8.f))
						.ButtonColorAndOpacity_Lambda([this]() { return Path == TEXT("frost") ? FLinearColor(0.4f, 0.75f, 1.f) : FLinearColor(0.3f, 0.3f, 0.3f); })
						[SNew(STextBlock).Text(FText::FromString(TEXT("Frost - Tundra"))).Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 4)[Button(TEXT("Host a game (friends join your IP, UDP port 7777)"), FOnClicked::CreateSP(this, &SWRMainMenu::OnHost))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 14, 0, 0)[Label(TEXT("Friend's address (IP or IP:port)"), 11)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 6)[SAssignNew(AddressBox, SEditableTextBox).Text(FText::FromString(Address)).Font(FCoreStyle::GetDefaultFontStyle("Regular", 15))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 14)[Button(TEXT("Join"), FOnClicked::CreateSP(this, &SWRMainMenu::OnJoin))]
				+ SVerticalBox::Slot().AutoHeight()[Button(TEXT("Quit"), FOnClicked::CreateSP(this, &SWRMainMenu::OnQuit))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 0)[SNew(STextBlock).Text_Lambda([this]() { return Status; }).ColorAndOpacity(FLinearColor(1.f, 0.8f, 0.4f))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)[Label(TEXT("In game: E interact, I inventory, P paths, K skills, B build, F10 back to this menu"), 10)]
			]
		]
	];
}

FReply SWRMainMenu::OnPickPath(FName InPath)
{
	Path = InPath;
	return FReply::Handled();
}

FString SWRMainMenu::Options() const
{
	FString Name = NameBox->GetText().ToString().TrimStartAndEnd();
	// Keep names to what the server's file names and chat can handle.
	Name = Name.Replace(TEXT("?"), TEXT("")).Replace(TEXT("="), TEXT("")).Replace(TEXT(" "), TEXT("_")).Left(20);
	if (Name.IsEmpty()) Name = TEXT("Survivor");
	return FString::Printf(TEXT("?WRName=%s?Path=%s"), *Name, *Path.ToString());
}

void SWRMainMenu::Remember() const
{
	GConfig->SetString(MenuSection, TEXT("Name"), *NameBox->GetText().ToString(), GGameUserSettingsIni);
	GConfig->SetString(MenuSection, TEXT("Address"), *AddressBox->GetText().ToString(), GGameUserSettingsIni);
	GConfig->SetString(MenuSection, TEXT("Path"), *Path.ToString(), GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

FReply SWRMainMenu::OnHost()
{
	APlayerController* PC = Owner.Get();
	if (!PC)
	{
		return FReply::Handled();
	}
	Remember();
	const FString Opts = TEXT("listen") + Options();
	UE_LOG(LogWildreach, Log, TEXT("MENU host: L_Island?%s"), *Opts);
	Close();
	UGameplayStatics::OpenLevel(PC, TEXT("/Game/Maps/L_Island"), true, Opts);
	return FReply::Handled();
}

FReply SWRMainMenu::OnJoin()
{
	APlayerController* PC = Owner.Get();
	FString Address = AddressBox->GetText().ToString().TrimStartAndEnd();
	if (!PC || Address.IsEmpty())
	{
		Status = FText::FromString(TEXT("Type your friend's IP address first"));
		return FReply::Handled();
	}
	Remember();
	UE_LOG(LogWildreach, Log, TEXT("MENU join: %s%s"), *Address, *Options());
	Close();
	PC->ClientTravel(Address + Options(), TRAVEL_Absolute);
	return FReply::Handled();
}

FReply SWRMainMenu::OnQuit()
{
	if (APlayerController* PC = Owner.Get())
	{
		UKismetSystemLibrary::QuitGame(PC, PC, EQuitPreference::Quit, false);
	}
	return FReply::Handled();
}

void SWRMainMenu::Open(APlayerController* PC)
{
	if (!PC || !GEngine || !GEngine->GameViewport || Instance.IsValid())
	{
		return;
	}
	Instance = SNew(SWRMainMenu).Owner(PC);
	GEngine->GameViewport->AddViewportWidgetContent(Instance.ToSharedRef(), 100);
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(Instance);
	PC->SetInputMode(Mode);
	PC->bShowMouseCursor = true;
}

void SWRMainMenu::Close()
{
	if (Instance.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(Instance.ToSharedRef());
	}
	Instance.Reset();
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->GetWorld())
	{
		if (APlayerController* PC = GEngine->GameViewport->GetWorld()->GetFirstPlayerController())
		{
			PC->SetInputMode(FInputModeGameOnly());
			PC->bShowMouseCursor = false;
		}
	}
}

bool SWRMainMenu::IsOpen()
{
	return Instance.IsValid();
}
