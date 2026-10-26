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
#include "Dom/JsonObject.h"
#include "HAL/PlatformProcess.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/DelayedAutoRegister.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

TSharedPtr<SWidget> SWRMainMenu::Instance;
TSharedPtr<SWidget> SWRChatBox::Instance;
FString SWRMainMenu::LatestVersion;
FString SWRMainMenu::LastFailure;

static const TCHAR* ReleasesPage = TEXT("https://github.com/RavenArcher17/Survival-Game/releases/latest");

// From the start (a join straight from the command line fails before any menu opens), in a game, not a server.
static FDelayedAutoRegisterHelper GWatchNetworkFailures(EDelayedRegisterRunPhase::EndOfEngineInit, []()
{
	if (!GIsEditor && !IsRunningDedicatedServer() && !IsRunningCommandlet())
	{
		SWRMainMenu::WatchNetworkFailures();
	}
});

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
	FString SavedBody;
	GConfig->GetString(MenuSection, TEXT("Body"), SavedBody, GGameUserSettingsIni);
	Body = SavedBody.IsEmpty() ? NAME_None : FName(*SavedBody);
	Status = FText::FromString(LastFailure);
	if (!LastFailure.IsEmpty())
	{
		UE_LOG(LogWildreach, Log, TEXT("MENU shows: %s"), *LastFailure);
		LastFailure.Empty();
	}

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
				// A newer release on GitHub: one click installs it (the updater) and starts it again.
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 16)
				[
					SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.12f, 0.32f, 0.1f, 0.95f)).Padding(10.f)
					.Visibility_Lambda([]() { return LatestVersion.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
						[
							SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 12)).ColorAndOpacity(FLinearColor::White).AutoWrapText(true)
							.Text_Lambda([]() { return FText::FromString(FString::Printf(TEXT("Wildreach %s is out (you have %s)"), *LatestVersion, *GameVersion())); })
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(10, 0, 0, 0)[Button(TEXT("Update now"), FOnClicked::CreateSP(this, &SWRMainMenu::OnUpdate))]
					]
				]
				+ SVerticalBox::Slot().AutoHeight()[Label(TEXT("Your name (your character and progress are saved under it)"), 11)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 14)[SAssignNew(NameBox, SEditableTextBox).Text(FText::FromString(Name)).Font(FCoreStyle::GetDefaultFontStyle("Regular", 15))]
				+ SVerticalBox::Slot().AutoHeight()[Label(TEXT("Body"), 11)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 14)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 6, 0)
					[
						SNew(SButton).OnClicked(this, &SWRMainMenu::OnPickBody, FName()).HAlign(HAlign_Center).ContentPadding(FMargin(10.f, 8.f))
						.ButtonColorAndOpacity_Lambda([this]() { return Body.IsNone() ? FLinearColor(0.85f, 0.7f, 0.45f) : FLinearColor(0.3f, 0.3f, 0.3f); })
						[SNew(STextBlock).Text(FText::FromString(TEXT("Male"))).Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))]
					]
					+ SHorizontalBox::Slot().FillWidth(1).Padding(6, 0, 0, 0)
					[
						SNew(SButton).OnClicked(this, &SWRMainMenu::OnPickBody, FName(TEXT("f"))).HAlign(HAlign_Center).ContentPadding(FMargin(10.f, 8.f))
						.ButtonColorAndOpacity_Lambda([this]() { return Body == TEXT("f") ? FLinearColor(0.85f, 0.7f, 0.45f) : FLinearColor(0.3f, 0.3f, 0.3f); })
						[SNew(STextBlock).Text(FText::FromString(TEXT("Female"))).Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))]
					]
				]
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
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)[SNew(STextBlock).Text(FText::FromString(TEXT("Version ") + GameVersion())).Font(FCoreStyle::GetDefaultFontStyle("Regular", 9)).ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))]
			]
		]
	];
}

FReply SWRMainMenu::OnPickPath(FName InPath)
{
	Path = InPath;
	return FReply::Handled();
}

FReply SWRMainMenu::OnPickBody(FName InBody)
{
	Body = InBody;
	return FReply::Handled();
}

FString SWRMainMenu::Options() const
{
	FString Name = NameBox->GetText().ToString().TrimStartAndEnd();
	// Keep names to what the server's file names and chat can handle.
	Name = Name.Replace(TEXT("?"), TEXT("")).Replace(TEXT("="), TEXT("")).Replace(TEXT(" "), TEXT("_")).Left(20);
	if (Name.IsEmpty()) Name = TEXT("Survivor");
	return FString::Printf(TEXT("?WRName=%s?Path=%s%s"), *Name, *Path.ToString(), Body.IsNone() ? TEXT("") : *(TEXT("?Body=") + Body.ToString()));
}

void SWRMainMenu::Remember() const
{
	GConfig->SetString(MenuSection, TEXT("Name"), *NameBox->GetText().ToString(), GGameUserSettingsIni);
	GConfig->SetString(MenuSection, TEXT("Address"), *AddressBox->GetText().ToString(), GGameUserSettingsIni);
	GConfig->SetString(MenuSection, TEXT("Path"), *Path.ToString(), GGameUserSettingsIni);
	GConfig->SetString(MenuSection, TEXT("Body"), Body.IsNone() ? TEXT("") : *Body.ToString(), GGameUserSettingsIni);
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

FReply SWRMainMenu::OnUpdate()
{
	// The updater beside Wildreach.exe waits for this game to close, installs the new release and starts it.
	const FString Updater = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::RootDir(), TEXT("WildreachUpdater.exe")));
	if (!FPaths::FileExists(Updater))
	{
		FPlatformProcess::LaunchURL(ReleasesPage, nullptr, nullptr);
		Status = FText::FromString(TEXT("Opened the download page: run the new Wildreach-Setup.exe (your saves are kept)"));
		return FReply::Handled();
	}
	const FString Args = FString::Printf(TEXT("--update --wait %u"), FPlatformProcess::GetCurrentProcessId());
	FProcHandle Proc = FPlatformProcess::CreateProc(*Updater, *Args, true, false, false, nullptr, 0, *FPaths::GetPath(Updater), nullptr);
	UE_LOG(LogWildreach, Log, TEXT("MENU update: %s %s (%s)"), *Updater, *Args, Proc.IsValid() ? TEXT("started") : TEXT("couldn't start"));
	if (!Proc.IsValid())
	{
		Status = FText::FromString(TEXT("Couldn't start the updater: download the new version from GitHub"));
		return FReply::Handled();
	}
	FPlatformProcess::CloseProc(Proc);
	return OnQuit();
}

FString SWRMainMenu::GameVersion()
{
	FString Version;
	GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("ProjectVersion"), Version, GGameIni);
	return Version.IsEmpty() ? FString(TEXT("0")) : Version;
}

bool SWRMainMenu::IsNewerVersion(const FString& Tag, const FString& Than)
{
	const auto Parse = [](FString Text, TArray<int32>& Out)
	{
		Text = Text.TrimStartAndEnd();
		Text.RemoveFromStart(TEXT("v"));
		TArray<FString> Parts;
		Text.ParseIntoArray(Parts, TEXT("."), false);
		for (const FString& Part : Parts)
		{
			if (Part.IsEmpty() || !Part.IsNumeric() || Part.Contains(TEXT("-")) || Part.Contains(TEXT(".")))
			{
				return false;
			}
			Out.Add(FCString::Atoi(*Part));
		}
		return Out.Num() > 0;
	};
	TArray<int32> A, B;
	if (!Parse(Tag, A))
	{
		return false;
	}
	if (!Parse(Than, B))
	{
		return true;
	}
	for (int32 i = 0; i < FMath::Max(A.Num(), B.Num()); ++i)
	{
		const int32 X = A.IsValidIndex(i) ? A[i] : 0, Y = B.IsValidIndex(i) ? B[i] : 0;
		if (X != Y)
		{
			return X > Y;
		}
	}
	return false;
}

void SWRMainMenu::CheckForUpdate()
{
	static bool bAsked = false;
	FString Feed;
	const bool bTestFeed = FParse::Value(FCommandLine::Get(), TEXT("-wrupdatefeed="), Feed);
	// The editor build isn't an installed copy: only check there for a test feed.
	if (bAsked || FParse::Param(FCommandLine::Get(), TEXT("noupdatecheck")) || (WITH_EDITOR && !bTestFeed))
	{
		return;
	}
	bAsked = true;
	if (!bTestFeed)
	{
		Feed = TEXT("https://api.github.com/repos/RavenArcher17/Survival-Game/releases/latest");
	}
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Feed);
	Request->SetVerb(TEXT("GET"));
	Request->SetHeader(TEXT("User-Agent"), TEXT("Wildreach"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/vnd.github+json"));
	Request->SetTimeout(10.f);
	Request->OnProcessRequestComplete().BindLambda([](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
	{
		TSharedPtr<FJsonObject> Json;
		FString Tag;
		if (bConnected && Response.IsValid() && Response->GetResponseCode() == 200
			&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Response->GetContentAsString()), Json) && Json.IsValid())
		{
			Json->TryGetStringField(TEXT("tag_name"), Tag);
		}
		if (IsNewerVersion(Tag, GameVersion()))
		{
			LatestVersion = Tag.TrimStartAndEnd();
			LatestVersion.RemoveFromStart(TEXT("v"));
		}
		UE_LOG(LogWildreach, Log, TEXT("MENU update check: this is %s, the latest release is %s%s"), *GameVersion(), Tag.IsEmpty() ? TEXT("unknown (offline?)") : *Tag,
			LatestVersion.IsEmpty() ? TEXT("") : TEXT(": offering the update"));
	});
	Request->ProcessRequest();
}

void SWRMainMenu::WatchNetworkFailures()
{
	static bool bWatching = false;
	if (bWatching || !GEngine)
	{
		return;
	}
	bWatching = true;
	GEngine->OnNetworkFailure().AddLambda([](UWorld*, UNetDriver*, ENetworkFailure::Type Type, const FString& Error)
	{
		if (Type == ENetworkFailure::OutdatedClient || Type == ENetworkFailure::OutdatedServer)
		{
			LastFailure = FString::Printf(TEXT("Couldn't join: the host has a different version of Wildreach (you have %s). Everyone needs the same one: start Wildreach from its shortcut to update."), *GameVersion());
		}
		else if (LastFailure.IsEmpty())
		{
			LastFailure = FString::Printf(TEXT("Disconnected: %s"), Error.IsEmpty() ? ENetworkFailure::ToString(Type) : *Error);
		}
		UE_LOG(LogWildreach, Log, TEXT("MENU network failure %s: %s"), ENetworkFailure::ToString(Type), *Error);
	});
	GEngine->OnTravelFailure().AddLambda([](UWorld*, ETravelFailure::Type Type, const FString& Error)
	{
		LastFailure = FString::Printf(TEXT("Couldn't connect: %s"), Error.IsEmpty() ? ETravelFailure::ToString(Type) : *Error);
		UE_LOG(LogWildreach, Log, TEXT("MENU travel failure %s: %s"), ETravelFailure::ToString(Type), *Error);
	});
}

void SWRMainMenu::Open(APlayerController* PC)
{
	WatchNetworkFailures();
	CheckForUpdate();
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
