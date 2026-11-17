#include "WRMainMenu.h"
#include "WRUIFit.h"
#include "Engine/NetDriver.h"
#include "WRReport.h"
#include "Components/PointLightComponent.h"
#include "WRCharacter.h"
#include "WRUIPadKeyboard.h"
#include "WRIdentity.h"
#include "WRAccess.h"
#include "WRPadInput.h"
#include "Wildreach.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRPlayerUI.h"
#include "WRKeyBindings.h"
#include "WRUISettings.h"
#include "WRUIWorlds.h"
#include "WRUIMotion.h"
#include "WRUIStyle.h"
#include "WRUIWidgets.h"
#include "WRTuning.h"
#include "WRPathLook.h"
#include "WRPathTypes.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/CharacterMovementComponent.h"
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
#include "Widgets/Images/SImage.h"
#include "Widgets/SOverlay.h"
#include "Fonts/FontMeasure.h"
#include "Containers/Ticker.h"
#include "WRWorlds.h"
#include "WRBootMark.h"

// The Freedom35 site's PC boot check (tools/bootcheck-pc.mjs) launches the packaged game with -F35Boot and reads the
// log for "F35READY menu" (the main menu is up) and "F35READY play" (FWRFlow, the loading cover lifts in a hosted or
// joined world). With the switch the menu plays on by itself after a moment: the last played world (Continue) when
// this PC has one, else a new one (New Game). Without it nothing changes.
namespace WRMenuBootCheck
{
	static double FirstTick = -1.0;
	static bool bDone = false;
	static bool Wanted() { return FParse::Param(FCommandLine::Get(), TEXT("F35Boot")); }
}
#include "Rendering/SlateRenderer.h"
#include "HAL/IConsoleManager.h"
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
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"

// Update now (wild-hotfix-menus, Aaron 11-16: "clicking to install the update should close the client, or restart it so
// it can apply the new version"; 0.8.0 opened the GitHub page when no updater sat beside the game). It never opens a web
// page: Update now -> "Updating..." on the banner -> the game closes -> the updater (or the release's own Setup) installs
// over this folder, saves kept -> the new version starts by itself.
namespace WRMenuUpdate
{
	static bool bBusy = false;
	static FText Line;
	static TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request;
	static TSharedPtr<FArchive> File;

	/** This game's folder (where Wildreach.exe and its launcher live), no trailing slash, Windows slashes. */
	static FString GameFolder()
	{
		FString Dir = FPaths::ConvertRelativePathToFull(FPaths::RootDir());
		FPaths::NormalizeDirectoryName(Dir);
		FPaths::MakePlatformFilename(Dir);
		return Dir;
	}

	/**
	 * The updater for this copy: beside the game (the shortcut's own; packaged builds carry one since 0.8.1), then each
	 * folder up from the running exe (Wildreach\Binaries\Win64), then the installer's folder (an installed copy on this PC:
	 * a build run from anywhere else still updates and starts the copy players use).
	 */
	static FString FindUpdater()
	{
		TArray<FString> Tries;
		Tries.Add(FPaths::Combine(FPaths::RootDir(), TEXT("WildreachUpdater.exe")));
		FString Up = FPaths::ConvertRelativePathToFull(FPlatformProcess::BaseDir());
		for (int32 i = 0; i < 4 && !Up.IsEmpty(); ++i)
		{
			FPaths::NormalizeDirectoryName(Up);
			Tries.Add(FPaths::Combine(Up, TEXT("WildreachUpdater.exe")));
			Up = FPaths::GetPath(Up);
		}
		const FString Local = FPlatformMisc::GetEnvironmentVariable(TEXT("LOCALAPPDATA"));
		if (!Local.IsEmpty()) Tries.Add(FPaths::Combine(Local, TEXT("Programs"), TEXT("Wildreach"), TEXT("WildreachUpdater.exe")));
		for (const FString& Try : Tries)
		{
			const FString Full = FPaths::ConvertRelativePathToFull(Try);
			if (FPaths::FileExists(Full)) return Full;
		}
		return FString();
	}

	// A FREEDOM35 INSTALL (wild-f35, Aaron 11-16: "on a Freedom35 install, updates come ONLY through Freedom35"): the
	// site's installer puts F35Update.exe and f35-update.json beside the game. Then the newest version is the site's
	// (/api/games/wildreach/latest, public), Update now hands over to F35Update (it fetches only the files that changed
	// and starts the game again), and the game never asks anywhere else.
	static FString F35File(const TCHAR* Name) { return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::RootDir(), Name)); }

	static TSharedPtr<FJsonObject> ReadJson(const FString& Path)
	{
		FString Text;
		TSharedPtr<FJsonObject> Json;
		if (FFileHelper::LoadFileToString(Text, *Path)) FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Json);
		return Json;
	}

	static bool IsFreedom35() { return FPaths::FileExists(F35File(TEXT("f35-update.json"))); }

	/** The version Freedom35 installed here: its file list's (rewritten by every update), else its config's; empty if neither. */
	static FString Freedom35Version()
	{
		FString Version;
		for (const TCHAR* Name : { TEXT("f35-files.json"), TEXT("f35-update.json") })
		{
			if (const TSharedPtr<FJsonObject> Json = ReadJson(F35File(Name)))
			{
				if (Json->TryGetStringField(TEXT("version"), Version) && !Version.IsEmpty()) return Version;
			}
		}
		return FString();
	}

	/** Where to ask: the site f35-update.json names (the one this copy came from), else tuning's. */
	static FString Freedom35Latest()
	{
		FString Site, Slug;
		if (const TSharedPtr<FJsonObject> Json = ReadJson(F35File(TEXT("f35-update.json"))))
		{
			Json->TryGetStringField(TEXT("site"), Site);
			Json->TryGetStringField(TEXT("slug"), Slug);
		}
		if (Site.IsEmpty()) Site = FWRTuning::Get().Str(TEXT("god.site"), TEXT("https://freedom35studios.pages.dev"));
		if (Slug.IsEmpty()) Slug = TEXT("wildreach");
		Site.RemoveFromEnd(TEXT("/"));
		return FString::Printf(TEXT("%s/api/games/%s/latest?platform=windows"), *Site, *Slug);
	}

	/** A moment for "Updating..." to show, then the game closes so the update can replace it. */
	static void QuitSoon(TWeakObjectPtr<APlayerController> PC)
	{
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([PC](float)
		{
			UE_LOG(LogWildreach, Log, TEXT("MENU update: closing so the update can install"));
			if (APlayerController* P = PC.Get()) UKismetSystemLibrary::QuitGame(P, P, EQuitPreference::Quit, false);
			else FPlatformMisc::RequestExit(false, TEXT("WRMenuUpdate"));
			return false;
		}), WRTune(TEXT("ui.updateCloseSeconds"), 1.2f));
	}

	static void Fail(const FText& Why)
	{
		UE_LOG(LogWildreach, Warning, TEXT("MENU update failed: %s"), *Why.ToString());
		bBusy = false;
		Line = Why;
		Request.Reset();
		if (File.IsValid()) { File->Close(); File.Reset(); }
	}

	static FText UpdatingLine(const FString& Version)
	{
		return FText::Format(NSLOCTEXT("WRUI", "Updating", "Updating to {0}: Wildreach closes, installs it and starts again (your saves are kept)"), FText::FromString(Version));
	}

	/**
	 * No updater anywhere: fetch the release's Wildreach-Setup.exe (the small download program when the release is in
	 * parts, which brings the rest), start it on this folder with this game's id to wait for, and close.
	 */
	static void DownloadSetup(const FString& Version, TWeakObjectPtr<APlayerController> PC)
	{
		const FString Url = FString::Printf(TEXT("https://github.com/RavenArcher17/Survival-Game/releases/download/v%s/Wildreach-Setup.exe"), *Version);
		const FString Dir = FPaths::Combine(FPlatformProcess::UserTempDir(), TEXT("Wildreach-Update"));
		IFileManager::Get().MakeDirectory(*Dir, true);
		const FString Setup = FPaths::ConvertRelativePathToFull(FPaths::Combine(Dir, FString::Printf(TEXT("Wildreach-Setup-v%s-game.exe"), *Version)));
		File = MakeShareable(IFileManager::Get().CreateFileWriter(*Setup));
		if (!File.IsValid())
		{
			Fail(NSLOCTEXT("WRUI", "UpdateNoTemp", "Couldn't write the update to the temp folder. Start Wildreach from its shortcut to update."));
			return;
		}
		Line = FText::Format(NSLOCTEXT("WRUI", "UpdateFetching", "Downloading Wildreach {0}..."), FText::FromString(Version));
		Request = FHttpModule::Get().CreateRequest();
		Request->SetURL(Url);
		Request->SetVerb(TEXT("GET"));
		Request->SetHeader(TEXT("User-Agent"), TEXT("Wildreach"));
		Request->SetTimeout(300.f);
		Request->SetResponseBodyReceiveStream(File.ToSharedRef());
		Request->OnRequestProgress64().BindLambda([Version](FHttpRequestPtr Req, uint64, uint64 Got)
		{
			const FHttpResponsePtr R = Req.IsValid() ? Req->GetResponse() : nullptr;
			const int64 Total = R.IsValid() ? int64(R->GetContentLength()) : 0;
			Line = Total > 0
				? FText::Format(NSLOCTEXT("WRUI", "UpdateFetchingPct", "Downloading Wildreach {0}... {1}%"), FText::FromString(Version), FText::AsNumber(FMath::Clamp<int64>(int64(Got) * 100 / Total, 0, 100)))
				: FText::Format(NSLOCTEXT("WRUI", "UpdateFetching", "Downloading Wildreach {0}..."), FText::FromString(Version));
		});
		Request->OnProcessRequestComplete().BindLambda([Setup, Version, PC](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
		{
			if (File.IsValid()) { File->Close(); File.Reset(); }
			Request.Reset();
			const int32 Code = Response.IsValid() ? Response->GetResponseCode() : 0;
			const int64 Size = IFileManager::Get().FileSize(*Setup);
			UE_LOG(LogWildreach, Log, TEXT("MENU update: downloaded the release's Setup (%d, %lld bytes) to %s"), Code, Size, *Setup);
			if (!bConnected || Code != 200 || Size < 1024)
			{
				Fail(NSLOCTEXT("WRUI", "UpdateNoDownload", "Couldn't download the update (offline?). Try again, or start Wildreach from its shortcut."));
				return;
			}
			const FString Args = FString::Printf(TEXT("--update --wait %u --dir \"%s\""), FPlatformProcess::GetCurrentProcessId(), *GameFolder());
			FProcHandle Proc = FPlatformProcess::CreateProc(*Setup, *Args, true, false, false, nullptr, 0, *FPaths::GetPath(Setup), nullptr);
			UE_LOG(LogWildreach, Log, TEXT("MENU update: %s %s (%s)"), *Setup, *Args, Proc.IsValid() ? TEXT("started") : TEXT("couldn't start"));
			if (!Proc.IsValid())
			{
				Fail(NSLOCTEXT("WRUI", "UpdateNoStart", "Couldn't start the update. Start Wildreach from its shortcut to update."));
				return;
			}
			FPlatformProcess::CloseProc(Proc);
			Line = UpdatingLine(Version);
			QuitSoon(PC);
		});
		if (!Request->ProcessRequest()) Fail(NSLOCTEXT("WRUI", "UpdateNoDownload", "Couldn't download the update (offline?). Try again, or start Wildreach from its shortcut."));
	}
}

TSharedPtr<SWidget> SWRMainMenu::Instance;
TWeakObjectPtr<ACameraActor> SWRMainMenu::Backdrop;
TSharedPtr<SWidget> SWRChatBox::Instance;
FString SWRMainMenu::LatestVersion;
FString SWRMainMenu::LastFailure;
int32 SWRMainMenu::CardKind = 0;
FString SWRMainMenu::CardTitle;
bool SWRMainMenu::bCanRejoin = false;
int32 SWRMainMenu::RejoinTries = 0;
bool SWRMainMenu::bQueued = false;
double SWRMainMenu::RejoinFirstAt = 0.0;
double SWRMainMenu::LastRejoinAt = -1000.0;
double SWRMainMenu::NextRejoinAt = 0.0;
double SWRMainMenu::LastFailAt = -1000.0;
bool SWRMainMenu::bJoining = false;
FString SWRMainMenu::LeaveTitle;
FString SWRMainMenu::LeaveLine;
double SWRMainMenu::LeaveAt = -1000.0;
bool SWRMainMenu::bNeedPassword = false;
FString SWRMainMenu::TypedPassword;
FString SWRMainMenu::PasswordAddress;

// From the start (a join straight from the command line fails before any menu opens), in a game, not a server.
static FDelayedAutoRegisterHelper GWatchNetworkFailures(EDelayedRegisterRunPhase::EndOfEngineInit, []()
{
	if (!GIsEditor && !IsRunningDedicatedServer() && !IsRunningCommandlet())
	{
		SWRMainMenu::WatchNetworkFailures();
	}
});

uint8 SWRChatBox::Channel = 0;

FText SWRChatBox::ChannelName(uint8 InChannel)
{
	switch (InChannel)
	{
	case WRChat::Local: return NSLOCTEXT("WRUI", "ChatNearby", "Nearby");
	case WRChat::Clan: return NSLOCTEXT("WRUI", "ChatClan", "Clan");
	default: return NSLOCTEXT("WRUI", "ChatEveryone", "Everyone");
	}
}

bool SWRChatBox::InClan() const
{
	const APlayerController* PC = Owner.Get();
	const AWRPlayerState* PS = PC ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
	return PS && !PS->ClanName.IsEmpty();
}

void SWRChatBox::NextChannel()
{
	// Everyone, nearby, your clan (only while you have one), round again.
	Channel = Channel == WRChat::Global ? WRChat::Local : Channel == WRChat::Local && InClan() ? WRChat::Clan : WRChat::Global;
}

FString SWRChatBox::WithChannel(const FString& Text) const
{
	const FString Line = Text.Left(200);
	if (Line.StartsWith(TEXT("/")))
	{
		return Line;   // its own channel: /w name, /r, /g, /l, /c
	}
	const uint8 Use = Channel == WRChat::Clan && !InClan() ? WRChat::Global : Channel;
	return Use == WRChat::Local ? TEXT("/l ") + Line : Use == WRChat::Clan ? TEXT("/c ") + Line : Line;
}

void SWRChatBox::Construct(const FArguments& InArgs)
{
	Owner = InArgs._Owner;
	if (Channel == WRChat::Clan && !InClan()) Channel = WRChat::Global;
	ChildSlot.HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(FMargin(20.f, 0.f, 0.f, 240.f))
	[
		SNew(SBox).WidthOverride(620.f)
		[
			SNew(SHorizontalBox)
			// The channel chip: click it (or Tab) for the next; its colour is the channel's lines' colour.
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
			[
				SNew(SBox).WidthOverride(96.f)
				[
					SNew(SWRButton).Ghost(true).FontSize(12).MinHeight(30.f)
					.Text_Lambda([]() { return ChannelName(Channel); })
					.OnClicked_Lambda([this]() { NextChannel(); if (Box.IsValid()) FSlateApplication::Get().SetKeyboardFocus(Box, EFocusCause::SetDirectly); return FReply::Handled(); })
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SAssignNew(Box, SEditableTextBox)
				.HintText_Lambda([]()
				{
					return FText::Format(NSLOCTEXT("WRUI", "ChatHint", "To {0}: Enter sends, Tab changes, /w name whispers, Esc cancels"), ChannelName(Channel));
				})
				.ForegroundColor_Lambda([]() { return FSlateColor(AWRPlayerController::ChatColor(Channel)); })
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 13)).OnTextCommitted(this, &SWRChatBox::OnCommitted)
			]
		]
	];
}

FReply SWRChatBox::OnPreviewKeyDown(const FGeometry& Geo, const FKeyEvent& Key)
{
	if (Key.GetKey() == EKeys::Tab)
	{
		NextChannel();
		return FReply::Handled();
	}
	return SCompoundWidget::OnPreviewKeyDown(Geo, Key);
}

void SWRChatBox::OnCommitted(const FText& Text, ETextCommit::Type How)
{
	// The pad keyboard over the line: its Done presses Enter here; anything else waits for it.
	if (How != ETextCommit::OnEnter && WRPadKeyboard::IsOpen()) return;
	if (How == ETextCommit::OnEnter && !Text.IsEmptyOrWhitespace())
	{
		if (AWRPlayerController* PC = Cast<AWRPlayerController>(Owner.Get()))
		{
			PC->ServerChat(WithChannel(Text.ToString()));
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
	// Opened on a pad (Settings > Chat): the keyboard comes up on the line at once; cancelling it closes the line. The
	// channel is the one last used; typed /c, /l, /g, /w name pick another.
	if (WRInput::UsingPad())
	{
		WRPadKeyboard::FTarget Target;
		Target.Hint = FText::Format(NSLOCTEXT("WRUI", "ChatPadHint2", "To {0} (/g everyone, /l nearby, /c clan, /w name)"), ChannelName(Channel));
		Target.MaxLength = 200;
		Target.bDockTop = true;   // the chat line sits low on the left
		const TWeakPtr<SWRChatBox> WeakBox = Widget;
		Target.OnDone = [WeakBox](const FText& Text)
		{
			const TSharedPtr<SWRChatBox> B = WeakBox.Pin();
			AWRPlayerController* W = B.IsValid() ? Cast<AWRPlayerController>(B->Owner.Get()) : nullptr;
			if (W && !Text.IsEmptyOrWhitespace()) W->ServerChat(B->WithChannel(Text.ToString()));
			Close();
		};
		Target.OnCancel = []() { Close(); };
		WRPadKeyboard::Open(MoveTemp(Target));
	}
}

void SWRChatBox::Close()
{
	if (Instance.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(Instance.ToSharedRef());
		if (APlayerController* PC = GEngine->GameViewport->GetWorld() ? GEngine->GameViewport->GetWorld()->GetFirstPlayerController() : nullptr)
		{
			PC->SetInputMode(FInputModeGameOnly());
			// A menu that was open under the chat box gets its cursor back.
			if (const AWRPlayerController* WPC = Cast<AWRPlayerController>(PC)) { if (FWRPlayerUI* UI = WPC->GetUI()) UI->ReapplyInputMode(); }
		}
	}
	Instance.Reset();
}

static const TCHAR* MenuSection = TEXT("Wildreach.Menu");

// The front end's look (mockup "Main menu"): the carved title over the island, a stone panel with your character and
// the ways in, a tip plaque, and the update banner when GitHub has a newer release.
namespace WRMenuUI
{
	using WRUI::U;   // mockup px -> 1080p units

	FLinearColor Hex(const TCHAR* H) { return FLinearColor(FColor::FromHex(H)); }

	FSlateFontInfo Spaced(FSlateFontInfo Font, int32 LetterSpacing)
	{
		Font.LetterSpacing = LetterSpacing;
		return Font;
	}

	const FEditableTextBoxStyle& FieldStyle(bool bBare)
	{
		// The name and address boxes: dark recessed fields (bare: no box, it sits inside one already).
		const auto Make = [](bool bNoBox)
		{
			FEditableTextBoxStyle S = FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");
			const FSlateBrush* Normal = bNoBox ? WRUI::Box(FLinearColor::Transparent) : WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.5f), 5.f, FLinearColor::Black, 1.f);
			const FSlateBrush* Hover = bNoBox ? Normal : WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.5f), 5.f, WRUI::Brass2, 1.f);
			const FSlateBrush* Focus = bNoBox ? Normal : WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.55f), 5.f, WRUI::Brass, 1.f);
			S.SetBackgroundImageNormal(*Normal).SetBackgroundImageHovered(*Hover).SetBackgroundImageFocused(*Focus).SetBackgroundImageReadOnly(*Normal);
			S.SetForegroundColor(FSlateColor(WRUI::Ink)).SetFocusedForegroundColor(FSlateColor(WRUI::Ink)).SetBackgroundColor(FSlateColor(FLinearColor::White));
			S.SetPadding(bNoBox ? FMargin(0.f, 4.f) : FMargin(12.f * U, 9.f * U));
			return S;
		};
		static const FEditableTextBoxStyle Boxed = Make(false), Bare = Make(true);
		return bBare ? Bare : Boxed;
	}

	/** A leather menu button: an icon, a carved name (and a line under it), maybe a chevron. */
	class SWRMenuButton : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SWRMenuButton) : _Icon(EWRIcon::Gear), _IconColor(WRUI::Ink2), _IconAccent(WRUI::Brass), _IconSize(22.f), _Primary(false), _Ghost(false), _Chevron(false), _Height(52.f), _FontSize(16) {}
			SLATE_ARGUMENT(EWRIcon, Icon)
			SLATE_ARGUMENT(FLinearColor, IconColor)
			SLATE_ARGUMENT(FLinearColor, IconAccent)
			SLATE_ARGUMENT(float, IconSize)
			SLATE_ARGUMENT(FText, Text)
			SLATE_ARGUMENT(FText, Sub)
			SLATE_ARGUMENT(bool, Primary)
			SLATE_ARGUMENT(bool, Ghost)
			SLATE_ARGUMENT(bool, Chevron)
			SLATE_ARGUMENT(float, Height)
			SLATE_ARGUMENT(int32, FontSize)
			SLATE_EVENT(FOnClicked, OnClicked)
		SLATE_END_ARGS()
		void Construct(const FArguments& InArgs)
		{
			const bool bPrimary = InArgs._Primary;
			TSharedRef<SVerticalBox> Names = SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(InArgs._Text, Spaced(WRUI::Title(InArgs._FontSize), 80), WRUI::Ink, true)];
			if (!InArgs._Sub.IsEmpty())
			{
				Names->AddSlot().AutoHeight().Padding(0.f, 3.f * U, 0.f, 0.f)[WRUI::Text(InArgs._Sub, WRUI::Body(13, 500), WRUI::Ink2, true)];
			}
			TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 16.f * U, 0.f)
				[
					SNew(SWRLineIcon).Icon(InArgs._Icon).Size(InArgs._IconSize * U).Color(InArgs._IconColor).Accent(InArgs._IconAccent)
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Names];
			if (InArgs._Chevron)
			{
				Row->AddSlot().AutoWidth().VAlign(VAlign_Center)[SNew(SWRLineIcon).Icon(EWRIcon::Chevron).Size(16.f * U).Color(WRUI::Brass)];
			}
			ChildSlot
			[
				SAssignNew(Button, SButton).ButtonStyle(FCoreStyle::Get(), "NoBorder").ContentPadding(0.f).Cursor(EMouseCursor::Hand).OnClicked(InArgs._OnClicked)
				[
					SNew(SBox).HeightOverride(InArgs._Height * U)
					// Pressed: the leather dips a little (UI motion).
					.RenderTransformPivot(FVector2D(0.5f, 0.5f))
					.RenderTransform_Lambda([this]() { return Button.IsValid() && Button->IsPressed() ? TOptional<FSlateRenderTransform>(FSlateRenderTransform(FScale2D(0.98f))) : TOptional<FSlateRenderTransform>(); })
					[
						SNew(SOverlay)
						+ SOverlay::Slot()[SNew(SImage).Image(InArgs._Ghost ? WRUI::White() : WRUI::Image(TEXT("leather"))).DesiredSizeOverride(FVector2D(1.f, 1.f))
							.ColorAndOpacity(InArgs._Ghost ? FLinearColor(0.f, 0.f, 0.f, 0.3f) : FLinearColor::White)]
						+ SOverlay::Slot().VAlign(VAlign_Top)[SNew(SBox).HeightOverride(1.f)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, InArgs._Ghost ? 0.05f : 0.12f))]]
						+ SOverlay::Slot()[SNew(SImage).Image_Lambda([this, bPrimary]()
						{
							const bool bHover = IsHovered();
							return WRUI::Box(bHover ? FLinearColor(1.f, 0.85f, 0.6f, 0.07f) : FLinearColor::Transparent, 4.f, bPrimary || bHover ? WRUI::Brass : WRUI::Brass2, bPrimary ? 1.5f : 1.f);
						})]
						+ SOverlay::Slot().Padding(FMargin(18.f * U, 0.f))[Row]
					]
				]
			];
		}
	private:
		TSharedPtr<SButton> Button;
	};

	/** One side of a choice: Male / Female, or a path (Fire / Frost / Lightning, coloured by its Data/paths file). Kind 0
	 *  plain, 1 fire, 2 frost; Path: any path by id (its colour and glyph, WRPathLook). */
	class SWRChoice : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SWRChoice) : _Kind(0), _Height(34.f), _FontSize(14) {}
			SLATE_ATTRIBUTE(FText, Text)
			SLATE_ARGUMENT(int32, Kind)
			SLATE_ARGUMENT(FName, Path)
			/** Mockup px (x U); the 12-path grid uses a shorter row. */
			SLATE_ARGUMENT(float, Height)
			SLATE_ARGUMENT(int32, FontSize)
			SLATE_EVENT(FOnClicked, OnClicked)
		SLATE_END_ARGS()
		void Construct(const FArguments& InArgs, TFunction<bool()> InSelected)
		{
			Selected = MoveTemp(InSelected);
			Kind = InArgs._Kind;
			Path = InArgs._Path;
			Height = InArgs._Height;
			FontSize = InArgs._FontSize;
			if (!Path.IsNone())
			{
				ConstructPath(InArgs);
				return;
			}
			TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
			if (Kind > 0)
			{
				Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 7.f * U, 0.f)
				[
					SNew(SWRLineIcon).Icon(Kind == 1 ? EWRIcon::Flame : EWRIcon::Snowflake).Size(15.f * U)
					.Color_Lambda([this]() { const FLinearColor C = Kind == 1 ? WRUI::Ember : WRUI::Frost; return Selected() ? C : WRUI::Alpha(C, 0.65f); })
				];
			}
			Row->AddSlot().AutoWidth().VAlign(VAlign_Center)
			[
				WRUI::Text(InArgs._Text, WRUI::Body(FontSize, 700), TAttribute<FSlateColor>::CreateLambda([this]()
				{
					if (!Selected()) return FSlateColor(IsHovered() ? WRUI::Ink2 : WRUI::Ink3);
					return FSlateColor(Kind == 1 ? Hex(TEXT("FFDCC2")) : Kind == 2 ? Hex(TEXT("D8EEFC")) : WRUI::Ink);
				}))
			];
			ChildSlot
			[
				SNew(SButton).ButtonStyle(FCoreStyle::Get(), "NoBorder").ContentPadding(0.f).Cursor(EMouseCursor::Hand).OnClicked(InArgs._OnClicked)
				[
					SNew(SBox).HeightOverride(Height * U)
					[
						SNew(SBorder).HAlign(HAlign_Center).VAlign(VAlign_Center)
						.BorderImage_Lambda([this]()
						{
							if (!Selected()) return WRUI::Box(IsHovered() ? FLinearColor(1.f, 1.f, 1.f, 0.04f) : FLinearColor::Transparent, 3.f);
							return Kind == 1 ? WRUI::Box(Hex(TEXT("54220D")), 3.f, WRUI::Ember, 1.f) : Kind == 2 ? WRUI::Box(Hex(TEXT("12324A")), 3.f, WRUI::Frost, 1.f) : WRUI::Box(Hex(TEXT("4D3920")), 3.f, WRUI::Brass, 1.f);
						})
						[Row]
					]
				]
			];
		}
	private:
		/** A path's choice: its glyph and name in its colour, a dark fill of it when picked. */
		void ConstructPath(const FArguments& InArgs)
		{
			const FLinearColor C = WRPathLook::Color(Path);
			const FLinearColor Light = FMath::Lerp(C, FLinearColor::White, 0.7f);
			const FLinearColor Fill = WRPathLook::Dark(Path, 0.28f);
			TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
			Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 7.f * U, 0.f)
			[
				SNew(SBox).WidthOverride(15.f * U).HeightOverride(15.f * U)
				[
					SNew(SImage).Image(WRUI::Image(FName(*(TEXT("Glyphs/") + WRPathLook::Glyph(Path)))))
					.ColorAndOpacity_Lambda([this, C]() { return FSlateColor(Selected() ? C : WRUI::Alpha(C, 0.65f)); })
				]
			];
			Row->AddSlot().AutoWidth().VAlign(VAlign_Center)
			[
				WRUI::Text(InArgs._Text, WRUI::Body(FontSize, 700), TAttribute<FSlateColor>::CreateLambda([this, Light]()
				{
					if (!Selected()) return FSlateColor(IsHovered() ? WRUI::Ink2 : WRUI::Ink3);
					return FSlateColor(Light);
				}))
			];
			ChildSlot
			[
				SNew(SButton).ButtonStyle(FCoreStyle::Get(), "NoBorder").ContentPadding(0.f).Cursor(EMouseCursor::Hand).OnClicked(InArgs._OnClicked)
				[
					SNew(SBox).HeightOverride(Height * U)
					[
						SNew(SBorder).HAlign(HAlign_Center).VAlign(VAlign_Center)
						.BorderImage_Lambda([this, C, Fill]()
						{
							if (!Selected()) return WRUI::Box(IsHovered() ? FLinearColor(1.f, 1.f, 1.f, 0.04f) : FLinearColor::Transparent, 3.f);
							return WRUI::Box(Fill, 3.f, C, 1.f);
						})
						[Row]
					]
				]
			];
		}
		TFunction<bool()> Selected;
		int32 Kind = 0;
		FName Path;
		float Height = 34.f;
		int32 FontSize = 14;
	};

	TSharedRef<SWidget> ChoicePair(TSharedRef<SWidget> A, TSharedRef<SWidget> B)
	{
		return SNew(SBorder).BorderImage(WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.5f), 5.f, FLinearColor::Black, 1.f)).Padding(3.f * U)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 2.f * U, 0.f)[A]
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(2.f * U, 0.f, 0.f, 0.f)[B]
		];
	}

	TSharedRef<SWidget> FieldLabel(const FText& Text)
	{
		return SNew(SBox).WidthOverride(48.f * U)[WRUI::Text(Text, Spaced(WRUI::Caps(12), 80), WRUI::Ink2)];
	}

	const TArray<FTip>& Tips()
	{
		static const TArray<FTip> List = {
			{ NSLOCTEXT("WRUI", "Tip1a", "hold"), TEXT("Wheel1"), NSLOCTEXT("WRUI", "Tip1b", "to open your ability wheel") },
			{ NSLOCTEXT("WRUI", "Tip2a", "hold"), TEXT("Interact"), NSLOCTEXT("WRUI", "Tip2b", "on your tame to give it orders") },
			{ NSLOCTEXT("WRUI", "Tip3a", "press"), TEXT("Build"), NSLOCTEXT("WRUI", "Tip3b", "to build, and Esc for settings and keys") },
			{ NSLOCTEXT("WRUI", "Tip4a", "press"), TEXT("Help"), NSLOCTEXT("WRUI", "Tip4b", "in game to see every control") },
			{ NSLOCTEXT("WRUI", "Tip5a", "press"), TEXT("BackToMenu"), NSLOCTEXT("WRUI", "Tip5b", "in game to come back to this menu") },
		};
		return List;
	}
}

void SWRMainMenu::Construct(const FArguments& InArgs)
{
	using namespace WRMenuUI;
	Owner = InArgs._Owner;
	FString Name = TEXT("Survivor"), Address = TEXT("127.0.0.1"), SavedPath = TEXT("fire");
	GConfig->GetString(MenuSection, TEXT("Name"), Name, GGameUserSettingsIni);
	GConfig->GetString(MenuSection, TEXT("Address"), Address, GGameUserSettingsIni);
	GConfig->GetString(MenuSection, TEXT("Path"), SavedPath, GGameUserSettingsIni);
	Path = FWRPathRegistry::Get().Find(FName(*SavedPath)) ? FName(*SavedPath) : FName(TEXT("fire"));
	FString SavedBody;
	GConfig->GetString(MenuSection, TEXT("Body"), SavedBody, GGameUserSettingsIni);
	Body = SavedBody.IsEmpty() ? NAME_None : FName(*SavedBody);
	// A failure is shown on the card; with none, a crash on the last run asks once to be sent.
	if (CardKind == 0 && !WRReport::PendingCrash().IsEmpty())
	{
		CardKind = 2;
		CardTitle = TEXT("Wildreach closed unexpectedly");
	}
	if (CardKind != 0)
	{
		UE_LOG(LogWildreach, Log, TEXT("MENU card: %s: %s"), *CardTitle, *LastFailure);
	}
	// On a pad, B closes the card (the D-pad reaches its buttons through the pad's navigation).
	WRInput::PushPadHandler(this, [this](FName Verb)
	{
		if (CardKind != 0 && Verb == TEXT("MenuBack"))
		{
			CloseCard();
			return true;
		}
		return false;
	});
	SAssignNew(NameBox, SEditableTextBox).Style(&FieldStyle(false)).Text(FText::FromString(Name)).Font(WRUI::Body(15, 700))
		.HintText(NSLOCTEXT("WRUI", "NameHint", "Your character is saved under this name"));
	SAssignNew(AddressBox, SEditableTextBox).Style(&FieldStyle(true)).Text(FText::FromString(Address)).Font(Spaced(WRUI::Body(16, 700), 30))
		.HintText(NSLOCTEXT("WRUI", "AddressHint", "Friend's IP or IP:port"));

	// The island shows through: darker on the left behind the menu and towards the edges.
	const FLinearColor Night = Hex(TEXT("060709"));
	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()[SNew(SWRGradientFill).Colors({ WRUI::Alpha(Night, 0.92f), WRUI::Alpha(Night, 0.72f), WRUI::Alpha(Night, 0.12f), WRUI::Alpha(Night, 0.1f), WRUI::Alpha(Night, 0.6f) }).Stops({ 0.f, 0.3f, 0.52f, 0.72f, 1.f })]
		+ SOverlay::Slot()[SNew(SWRGradientFill).Horizontal(false).Colors({ WRUI::Alpha(Night, 0.55f), WRUI::Alpha(Night, 0.f), WRUI::Alpha(Night, 0.f), WRUI::Alpha(Night, 0.65f) }).Stops({ 0.f, 0.3f, 0.62f, 1.f })]
		// Laid out by flow, not by fixed offsets (cooks/wild-hotfix-menus.md 3.3): the title row (the tip at its right),
		// then the character panel, the look picker beside it while open, and the stage: the free space the survivor is
		// framed into. All in a fit (WRUIFit.h), so a big interface size or a small window shrinks it rather than
		// pushing Host / Join / Settings / Quit off the screen.
		+ SOverlay::Slot()
		[
			WRUI::Fit(SNew(SOverlay)
			+ SOverlay::Slot().Padding(72.f * U, 40.f * U, 48.f * U, 32.f * U)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)[SAssignNew(TitleBox, SBox)[MakeTitle()]]
					+ SHorizontalBox::Slot().FillWidth(1.f)[SNullWidget::NullWidget]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.f, 4.f * U, 0.f, 0.f)
					[
						SAssignNew(TipBox, SBox).Visibility_Lambda([this]() { return bTipFits && !bLookOpen ? EVisibility::SelfHitTestInvisible : EVisibility::Hidden; })[MakeTip()]
					]
				]
				+ SVerticalBox::Slot().FillHeight(1.f).Padding(0.f, 22.f * U, 0.f, 0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)[MakePanel()]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)[MakeLookPanel()]
					+ SHorizontalBox::Slot().FillWidth(1.f)[SAssignNew(Stage, SBox)]
				]
			]
			// While the look picker is open the stage is the survivor's: the update banner waits until Done.
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 48.f * U, 32.f * U)
			[
				SAssignNew(UpdateBox, SBox).Visibility_Lambda([this]() { return bLookOpen ? EVisibility::Collapsed : EVisibility::SelfHitTestInvisible; })[MakeUpdate()]
			])
		]
		+ SOverlay::Slot()[WRUI::Fit(MakeCard())]
		+ SOverlay::Slot()[WRUI::Fit(SAssignNew(SettingsHost, SBox))]
	];
}

SWRMainMenu::~SWRMainMenu()
{
	WRInput::PopPadHandler(this);
	if (bCardNav) WRInput::RemoveNavRoot(CardPanel);
}

FReply SWRMainMenu::OnKeyDown(const FGeometry& Geo, const FKeyEvent& E)
{
	if (CardKind != 0 && E.GetKey() == EKeys::Escape)
	{
		CloseCard();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

TSharedRef<SWidget> SWRMainMenu::MakeCard()
{
	using namespace WRMenuUI;
	const auto Max = []() { return FMath::Max(1, static_cast<int32>(WRTune(TEXT("ui.rejoin.tries"), 6.f))); };
	return SNew(SOverlay).Visibility_Lambda([]() { return CardKind != 0 ? EVisibility::Visible : EVisibility::Collapsed; })
		// The menu behind dims and can't be clicked while the card asks.
		+ SOverlay::Slot()[SNew(SImage).Image(WRUI::White()).ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.55f))]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SAssignNew(CardPanel, SBox).WidthOverride(600.f * U)
			[
				SNew(SWRPanel).Kind(EWRPanel::Stone).Rivets(true).BrassTrim(true).Padding(FMargin(30.f * U, 24.f * U))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f * U, 0.f)
						[
							SNew(SWRLineIcon).Icon(EWRIcon::Warning).Size(26.f * U).Color(WRUI::Warn).Accent(WRUI::Warn)
						]
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
						[
							WRUI::Text(TAttribute<FText>::CreateLambda([]() { return FText::FromString(CardTitle); }), Spaced(WRUI::Title(20), 40), WRUI::Ink, true)
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f * U, 0.f, 0.f)
					[
						WRUI::Text(TAttribute<FText>::CreateLambda([]()
						{
							return CardKind == 2 ? NSLOCTEXT("WRUI", "CrashLine", "It crashed last time. Sending the crash report helps us fix it: it holds what went wrong and the last lines of the game's log, nothing else.")
								: FText::FromString(LastFailure);
						}), WRUI::Body(15, 500), WRUI::Ink2, false, true)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f * U, 0.f, 0.f)
					[
						WRUI::Text(TAttribute<FText>::CreateLambda([Max]()
						{
							if (CardKind != 1 || NextRejoinAt <= 0.0) return FText::GetEmpty();
							const int32 Left = FMath::Max(0, FMath::CeilToInt(NextRejoinAt - FPlatformTime::Seconds()));
							if (bQueued) return FText::Format(NSLOCTEXT("WRUI", "QueueCount", "Asking again in {0} s."), Left);
							return FText::Format(NSLOCTEXT("WRUI", "RejoinCount", "Try {0} of {1} didn't get in. Trying again in {2} s."), RejoinTries, Max(), Left);
						}), WRUI::Body(13, 700), WRUI::Brass, false, true)
					]
					// The host's world asked for its password (cooks/cookpass-multiplayer.md 3.4): type it here; Enter or Join.
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f * U, 0.f, 0.f)
					[
						SNew(SBox).Visibility_Lambda([]() { return CardKind == 1 && bNeedPassword ? EVisibility::Visible : EVisibility::Collapsed; })
						[
							SAssignNew(PasswordBox, SEditableTextBox).IsPassword(true).Font(WRUI::Body(16, 700))
							.HintText(NSLOCTEXT("WRUI", "PasswordHint", "The world's password"))
							.OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type How) { if (How == ETextCommit::OnEnter && !Text.IsEmpty()) JoinWithPassword(); })
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 20.f * U, 0.f, 0.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f)[SNullWidget::NullWidget]
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 10.f * U, 0.f)
						[
							SNew(SBox).MinDesiredWidth(150.f * U)
							[
								SNew(SWRButton).Ghost(true).FontSize(13).MinHeight(44.f * U)
								.Text_Lambda([]() { return CardKind == 2 ? NSLOCTEXT("WRUI", "NotNow", "Not now") : bQueued ? NSLOCTEXT("WRUI", "LeaveQueue", "Leave the queue") : NextRejoinAt > 0.0 ? NSLOCTEXT("WRUI", "StopTrying", "Stop trying") : NSLOCTEXT("WRUI", "CloseCard", "Close"); })
								.OnClicked_Lambda([this]() { CloseCard(); return FReply::Handled(); })
							]
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SBox).MinDesiredWidth(190.f * U).Visibility_Lambda([]() { return CardKind == 2 || (CardKind == 1 && (bCanRejoin || bNeedPassword)) ? EVisibility::Visible : EVisibility::Collapsed; })
							[
								SNew(SWRButton).Primary(true).FontSize(13).MinHeight(44.f * U)
								.Text_Lambda([]() { return CardKind == 2 ? NSLOCTEXT("WRUI", "SendCrash", "Send the crash report") : bNeedPassword ? NSLOCTEXT("WRUI", "JoinPassword", "Join") : NextRejoinAt > 0.0 ? NSLOCTEXT("WRUI", "RejoinNow", "Try now") : NSLOCTEXT("WRUI", "Rejoin", "Rejoin"); })
								.OnClicked_Lambda([this]()
								{
									if (CardKind == 1 && bNeedPassword)
									{
										JoinWithPassword();
									}
									else if (CardKind == 2)
									{
										WRReport::SendCrash();
										CardKind = 0;
										Status = NSLOCTEXT("WRUI", "CrashSent", "Crash report sent. Thank you.");
									}
									else
									{
										Rejoin();
									}
									return FReply::Handled();
								})
							]
						]
					]
				]
			]
		];
}

void SWRMainMenu::CloseCard()
{
	if (CardKind == 2) WRReport::DismissCrash();
	CardKind = 0;
	NextRejoinAt = 0.0;
	bCanRejoin = false;
	bNeedPassword = false;
	bQueued = false;
	LastFailure.Empty();
}

void SWRMainMenu::Rejoin()
{
	APlayerController* PC = Owner.Get();
	FString Address;
	GConfig->GetString(MenuSection, TEXT("Address"), Address, GGameUserSettingsIni);
	if (!PC || Address.IsEmpty())
	{
		CloseCard();
		return;
	}
	const double Now = FPlatformTime::Seconds();
	if (RejoinTries == 0) RejoinFirstAt = Now;
	++RejoinTries;
	LastRejoinAt = Now;
	NextRejoinAt = 0.0;
	CardKind = 0;
	LastFailure.Empty();
	bJoining = true;
	const FString Url = Address + Options();
	UE_LOG(LogWildreach, Log, TEXT("MENU rejoin %d: %s"), RejoinTries, *Url);
	Close();
	PC->ClientTravel(Url, TRAVEL_Absolute);
}

TSharedRef<SWidget> SWRMainMenu::MakeTitle()
{
	using namespace WRMenuUI;
	// Carved gold: a dark copy below for depth, then bands of lighter gold clipped to the top of the letters.
	const FText Word = FText::FromString(TEXT("WILDREACH"));
	const FSlateFontInfo Font = Spaced(WRUI::Title(74), 70);
	const float H = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Word, Font).Y;
	const auto Layer = [&Word, &Font](const FLinearColor& Color) { return SNew(STextBlock).Text(Word).Font(Font).ColorAndOpacity(Color); };
	const auto Band = [&Layer, H](float Fraction, const FLinearColor& Color)
	{
		return SNew(SBox).HeightOverride(H * Fraction).VAlign(VAlign_Top).Clipping(EWidgetClipping::ClipToBounds)[Layer(Color)];
	};
	const FText Line1 = NSLOCTEXT("WRUI", "Tagline1", "A SURVIVAL ISLAND OF "), Fire = NSLOCTEXT("WRUI", "TaglineFire", "FIRE"), And = NSLOCTEXT("WRUI", "TaglineAnd", " AND "), Frost = NSLOCTEXT("WRUI", "TaglineFrost", "FROST");
	const FSlateFontInfo TagFont = Spaced(WRUI::Body(14, 400), 520);
	const FLinearColor B2 = WRUI::Brass2;
	return SNew(SBox).WidthOverride(590.f * U)
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
		[
			SNew(SOverlay)
			+ SOverlay::Slot().Padding(0.f, 5.f * U, 0.f, 0.f)
			[
				SNew(STextBlock).Text(Word).Font(Font).ColorAndOpacity(Hex(TEXT("1A1006"))).ShadowOffset(FVector2D(0.f, 6.f * U)).ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.85f))
			]
			+ SOverlay::Slot()[Layer(Hex(TEXT("B8894A")))]
			+ SOverlay::Slot().VAlign(VAlign_Top)[Band(0.5f, Hex(TEXT("E9C27A")))]
			+ SOverlay::Slot().VAlign(VAlign_Top)[Band(0.37f, Hex(TEXT("FBE7B4")))]
		]
		// (The line box of the big letters has room for descenders they don't have: tuck the divider up under them.)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, -20.f * U, 0.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[SNew(SBox).HeightOverride(1.f)[SNew(SWRGradientFill).Colors({ WRUI::Alpha(B2, 0.f), B2 })]]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.f * U, 0.f)
			[
				SNew(SBox).WidthOverride(9.f * U).HeightOverride(9.f * U)
				[
					SNew(SImage).Image(WRUI::Box(WRUI::Brass)).RenderTransform(FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(45.f)))).RenderTransformPivot(FVector2D(0.5f, 0.5f))
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[SNew(SBox).HeightOverride(1.f)[SNew(SWRGradientFill).Colors({ B2, WRUI::Alpha(B2, 0.f) })]]
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 10.f * U, 0.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[WRUI::Text(Line1, TagFont, WRUI::Ink2, true)]
			+ SHorizontalBox::Slot().AutoWidth()[WRUI::Text(Fire, TagFont, WRUI::Ember, true)]
			+ SHorizontalBox::Slot().AutoWidth()[WRUI::Text(And, TagFont, WRUI::Ink2, true)]
			+ SHorizontalBox::Slot().AutoWidth()[WRUI::Text(Frost, TagFont, WRUI::Frost, true)]
		]
	];
}

TSharedRef<SWidget> SWRMainMenu::MakePanel()
{
	using namespace WRMenuUI;
	const auto FieldRow = [](const FText& Label, TSharedRef<SWidget> Field)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f * U, 0.f)[FieldLabel(Label)]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Field];
	};
	return SNew(SBox).WidthOverride(480.f * U)
	[
		SNew(SWRPanel).Kind(EWRPanel::Stone).Rivets(true).BrassTrim(true).Padding(FMargin(22.f * U, 20.f * U, 22.f * U, 22.f * U))
		[
			SNew(SVerticalBox)
			// Your character: name, body, main path.
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SWRPanel).Kind(EWRPanel::Leather).Padding(FMargin(20.f * U, 16.f * U))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(NSLOCTEXT("WRUI", "YourCharacter", "Your character"), Spaced(WRUI::Title(14), 60), WRUI::Ink, true)]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 11.f * U, 0.f, 0.f)[FieldRow(NSLOCTEXT("WRUI", "Name", "Name"), SNew(SBox).HeightOverride(40.f * U)[NameBox.ToSharedRef()])]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 11.f * U, 0.f, 0.f)
					[
						FieldRow(NSLOCTEXT("WRUI", "Body", "Body"), ChoicePair(
							SNew(SWRChoice, [this]() { return Body.IsNone(); }).Text(NSLOCTEXT("WRUI", "Male", "Male")).OnClicked(this, &SWRMainMenu::OnPickBody, FName()),
							SNew(SWRChoice, [this]() { return Body == TEXT("f"); }).Text(NSLOCTEXT("WRUI", "Female", "Female")).OnClicked(this, &SWRMainMenu::OnPickBody, FName(TEXT("f")))))
					]
					// Look: the face and build on one line; Change opens the picker beside the panel.
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 11.f * U, 0.f, 0.f)
					[
						FieldRow(NSLOCTEXT("WRUI", "LookRow", "Look"),
							SNew(SBorder).BorderImage(WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.5f), 5.f, FLinearColor::Black, 1.f)).Padding(3.f * U)
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(10.f * U, 0.f)
								[
									WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return LookSummary(); }), WRUI::Body(14, 700), WRUI::Ink)
								]
								+ SHorizontalBox::Slot().AutoWidth()
								[
									SNew(SBox).WidthOverride(110.f * U)
									[
										SNew(SWRChoice, [this]() { return bLookOpen; }).Text(NSLOCTEXT("WRUI", "LookChange", "Change"))
										.OnClicked_Lambda([this]() { bLookOpen = !bLookOpen; if (bLookOpen) ApplyPreview(); else EndFraming(); return FReply::Handled(); })
									]
								]
							])
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 11.f * U, 0.f, 0.f)
					[
						FieldRow(NSLOCTEXT("WRUI", "Path", "Path"), MakePathPicker())
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(60.f * U, 11.f * U, 0.f, 0.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f * U, 0.f)[SNew(SWRLineIcon).Icon(EWRIcon::Info).Size(15.f * U).Color(WRUI::Brass)]
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
						[
							WRUI::Text(TAttribute<FText>::CreateLambda([this]()
							{
								const FString Land = WRPathLook::BiomeName(Path);
								return Land.IsEmpty() ? NSLOCTEXT("WRUI", "PathInfo", "Your main path sets where you spawn")
									: FText::Format(NSLOCTEXT("WRUI", "PathWake", "Your main path sets where you wake: {0}"), FText::FromString(Land));
							}), WRUI::Body(12, 500), WRUI::Ink2, false, true)
						]
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f * U, 0.f, 0.f)
			[
				SNew(SWRMenuButton).Primary(true).Height(68.f).FontSize(18).Chevron(true)
				.Icon(EWRIcon::FlagFilled).IconSize(28.f).IconColor(WRUI::Brass).IconAccent(WRUI::Ember)
				.Text(NSLOCTEXT("WRUI", "Host", "Host a game")).Sub(NSLOCTEXT("WRUI", "HostSub", "Choose a world · friends join your IP"))
				.OnClicked(this, &SWRMainMenu::OnHost)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f * U, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).WidthOverride(150.f * U)
					[
						SNew(SWRMenuButton).Icon(EWRIcon::Join).IconColor(WRUI::Ink2).IconAccent(WRUI::Brass).Text(NSLOCTEXT("WRUI", "Join", "Join")).OnClicked(this, &SWRMainMenu::OnJoin)
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(8.f * U, 0.f, 0.f, 0.f)
				[
					SNew(SBox).HeightOverride(52.f * U)
					[
						SNew(SBorder).BorderImage(WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.5f), 4.f, FLinearColor::Black, 1.f)).Padding(FMargin(14.f * U, 0.f)).VAlign(VAlign_Center)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f * U, 0.f)[WRUI::Text(NSLOCTEXT("WRUI", "Address", "Address"), Spaced(WRUI::Caps(11), 80), WRUI::Ink2)]
							+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[AddressBox.ToSharedRef()]
						]
					]
				]
			]
			// Why the last join or game ended (a different version...), and what Update now did.
			+ SVerticalBox::Slot().AutoHeight().Padding(4.f * U, 7.f * U, 0.f, 0.f)
			[
				SNew(SHorizontalBox).Visibility_Lambda([this]() { return Status.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.f, 1.f * U, 7.f * U, 0.f)[SNew(SWRLineIcon).Icon(EWRIcon::Warning).Size(14.f * U).Color(WRUI::Warn)]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return Status; }), WRUI::Body(13, 700), WRUI::Warn, false, true)]
			]
			// Settings and Quit share a row (12 paths made the panel too tall for a 1080 canvas: wild-hotfix-menus 3.3).
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f * U, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(SWRMenuButton).Icon(EWRIcon::Gear).IconColor(WRUI::Brass).Text(NSLOCTEXT("WRUI", "SettingsButton", "Settings"))
					.OnClicked_Lambda([]() { ShowSettings(true); return FReply::Handled(); })
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(8.f * U, 0.f, 0.f, 0.f)
				[
					SNew(SBox).WidthOverride(150.f * U)
					[
						SNew(SWRMenuButton).Ghost(true).Icon(EWRIcon::Power).IconColor(WRUI::Ink2).IconAccent(WRUI::Bad).Text(NSLOCTEXT("WRUI", "QuitButton", "Quit")).OnClicked(this, &SWRMainMenu::OnQuit)
					]
				]
			]
		]
	];
}

TSharedRef<SWidget> SWRMainMenu::MakeTip()
{
	using namespace WRMenuUI;
	return SNew(SWRPanel).Kind(EWRPanel::Glass).Rivets(true).Padding(FMargin(16.f * U, 12.f * U, 20.f * U, 12.f * U))
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f * U, 0.f)[SNew(SWRLineIcon).Icon(EWRIcon::Bulb).Size(22.f * U).Color(WRUI::Warn)]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 5.f * U, 0.f)[WRUI::Text(NSLOCTEXT("WRUI", "Tip", "Tip:"), WRUI::Body(14, 700), WRUI::Brass)]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f * U, 0.f)[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return Tips()[Tip].Before; }), WRUI::Body(14, 500), WRUI::Ink2)]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f * U, 0.f)
		[
			SNew(SWRKeyCap).FontSize(12).Text_Lambda([this]() { return WRInput::ActionKey(Tips()[Tip].Action); })
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return Tips()[Tip].After; }), WRUI::Body(14, 500), WRUI::Ink2)]
	];
}

TSharedRef<SWidget> SWRMainMenu::MakeUpdate()
{
	using namespace WRMenuUI;
	return SNew(SBox).WidthOverride(460.f * U)
	[
		SNew(SVerticalBox)
		// A newer version (Freedom35's live one on a Freedom35 install): one click installs it (the updater) and starts it again.
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBox).Visibility_Lambda([]() { return LatestVersion.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SWRPanel).Kind(EWRPanel::Stone).Rivets(true).BrassTrim(true).Padding(FMargin(20.f * U, 16.f * U, 18.f * U, 16.f * U))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 14.f * U, 0.f)
						[
							SNew(SBox).WidthOverride(44.f * U).HeightOverride(44.f * U)
							[
								SNew(SOverlay)
								+ SOverlay::Slot()[SNew(SImage).Image(WRUI::Box(Hex(TEXT("2E2010")), 22.f * U, WRUI::Brass, 2.f))]
								+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[SNew(SWRLineIcon).Icon(EWRIcon::Download).Size(22.f * U).Color(Hex(TEXT("F0D49B"))).Accent(WRUI::Brass)]
							]
						]
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()
							[
								WRUI::Text(TAttribute<FText>::CreateLambda([]() { return FText::Format(NSLOCTEXT("WRUI", "UpdateOut", "Wildreach {0} is out"), FText::FromString(LatestVersion)); }), Spaced(WRUI::Title(14), 60), WRUI::Ink, true)
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f * U, 0.f, 0.f)
							[
								// While it updates, what it's doing (Updating..., the download's progress, or why it couldn't).
								WRUI::Text(TAttribute<FText>::CreateLambda([]() { return WRMenuUpdate::Line.IsEmpty() ? NSLOCTEXT("WRUI", "UpdateSub", "A new version is ready to install") : WRMenuUpdate::Line; }),
									WRUI::Body(12, 500), TAttribute<FSlateColor>::CreateLambda([]() { return FSlateColor(WRMenuUpdate::Line.IsEmpty() ? WRUI::Ink3 : WRUI::Ink2); }), false, true)
							]
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.f * U, 0.f, 0.f, 0.f)
						[
							SNew(SWRButton).Primary(true).FontSize(12).MinHeight(40.f * U).IsEnabled_Lambda([]() { return !WRMenuUpdate::bBusy; })
							.Text_Lambda([]() { return WRMenuUpdate::bBusy ? NSLOCTEXT("WRUI", "UpdatingButton", "Updating...") : NSLOCTEXT("WRUI", "UpdateNow", "Update now"); }).OnClicked(this, &SWRMainMenu::OnUpdate)
						]
					]
				]
				// The banner's brighter edge (the mockup's brass border).
				+ SOverlay::Slot()[SNew(SImage).Visibility(EVisibility::HitTestInvisible).Image(WRUI::Box(FLinearColor::Transparent, 5.f, WRUI::Alpha(WRUI::Brass, 0.7f), 1.f))]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.f, 8.f * U, 0.f, 0.f)
		[
			WRUI::Text(FText::Format(NSLOCTEXT("WRUI", "VersionLine", "Version {0}"), FText::FromString(WRMenuUpdate::IsFreedom35() && !WRMenuUpdate::Freedom35Version().IsEmpty() ? WRMenuUpdate::Freedom35Version() : GameVersion())), WRUI::Body(11, 500), WRUI::Ink3, true)
		]
	];
}

void SWRMainMenu::Tick(const FGeometry& Geo, const double Time, const float Delta)
{
	SCompoundWidget::Tick(Geo, Time, Delta);
	if (!WRMenuBootCheck::bDone && WRMenuBootCheck::Wanted())
	{
		if (WRMenuBootCheck::FirstTick < 0.0) WRMenuBootCheck::FirstTick = Time;
		if (Time - WRMenuBootCheck::FirstTick > WRTune(TEXT("flow.bootCheckMenuSeconds"), 4.f))
		{
			WRMenuBootCheck::bDone = true;
			// Hosting closes this widget: do it on the next engine tick, not inside its own Tick.
			const TWeakPtr<SWRMainMenu> Weak = SharedThis(this);
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak](float)
			{
				const TSharedPtr<SWRMainMenu> Menu = Weak.Pin();
				if (!Menu) return false;
				const TArray<FWRWorldInfo> Worlds = WRWorlds::List();
				FString Id = Worlds.Num() ? Worlds[0].Id : FString();
				const bool bContinue = !Id.IsEmpty();
				if (!bContinue) Id = WRWorlds::Create(TEXT("Boot check"), WRWorlds::PresetSettings(TEXT("pve")));
				WRBootMark::Line(FString::Printf(TEXT("F35BOOT %s world '%s'"), bContinue ? TEXT("continue") : TEXT("new"), *Id));
				if (!Id.IsEmpty()) Menu->HostWorld(Id);
				return false;
			}));
		}
	}
	// The body is possessed after the menu opens (and takes the view): keep the backdrop camera and the body still.
	if (APlayerController* PC = Owner.Get())
	{
		const ACharacter* Survivor = Cast<ACharacter>(PC->GetPawn());
		if (!Backdrop.IsValid() || PC->GetViewTarget() != Backdrop.Get() || (Survivor && Survivor->HasAuthority() && Survivor->GetCharacterMovement()->MovementMode != MOVE_None))
		{
			ShowBackdrop(PC, true);
		}
	}
	// While the card shows, the pad's D-pad moves between its buttons only (not the menu dimmed behind it).
	if ((CardKind != 0) != bCardNav && CardPanel.IsValid())
	{
		bCardNav = CardKind != 0;
		if (bCardNav) WRInput::AddNavRoot(CardPanel.ToSharedRef(), 100);
		else WRInput::RemoveNavRoot(CardPanel);
	}
	// The tip shows only where it fits beside the title (a 4:3 or small window has no room for both on one line).
	if (TitleBox.IsValid() && TipBox.IsValid())
	{
		const float Have = Geo.GetLocalSize().X;
		const float Need = (72.f + 48.f + 24.f) * WRUI::U + TitleBox->GetDesiredSize().X + TipBox->GetDesiredSize().X;
		bTipFits = Need <= Have;
	}
	// The look picker frames the survivor while it's open, into the stage (the space the panels leave).
	if (bLookOpen)
	{
		FrameSurvivor(Delta);
	}
	// The card's countdown to the next Rejoin try.
	if (CardKind == 1 && NextRejoinAt > 0.0 && FPlatformTime::Seconds() >= NextRejoinAt)
	{
		Rejoin();
		return;
	}
	// A new tip now and then.
	if (NextTip == 0.0)
	{
		NextTip = Time + 9.0;
	}
	else if (Time >= NextTip)
	{
		NextTip = Time + 9.0;
		Tip = (Tip + 1) % WRMenuUI::Tips().Num();
	}
}

void SWRMainMenu::ShowSettings(bool bShow)
{
	const TSharedPtr<SWRMainMenu> Menu = StaticCastSharedPtr<SWRMainMenu>(Instance);
	if (!Menu.IsValid() || !Menu->SettingsHost.IsValid())
	{
		return;
	}
	if (bShow)
	{
		Menu->SettingsHost->SetContent(SNew(SWRAppear).Seconds(WRTune(TEXT("flow.screenInSeconds"), 0.15f)).Offset(FVector2D(0.f, WRTune(TEXT("flow.screenRise"), 14.f)))
		[
			WRUI::MakeSettings(Menu->Owner.Get(), false, FSimpleDelegate::CreateLambda([]() { ShowSettings(false); }))
		]);
	}
	else
	{
		Menu->SettingsHost->SetContent(SNullWidget::NullWidget);
	}
}

TSharedRef<SWidget> SWRMainMenu::MakePathPicker()
{
	using namespace WRMenuUI;
	// Every path in Data/paths (the path's biome is where you spawn: its PlayerStarts carry the biome's tag), built from
	// the registry so a new path shows the day its file lands: two side by side, more in a column.
	const TArray<FWRPathDef>& Defs = FWRPathRegistry::Get().All();
	if (Defs.Num() == 2)
	{
		TArray<TSharedRef<SWidget>> Pair;
		for (const FWRPathDef& D : Defs)
		{
			const FName Id = D.Id;
			const FText Label = FText::FromString(FString::Printf(TEXT("%s · %s"), *WRPathLook::Name(Id), *WRPathLook::BiomeName(Id)));
			Pair.Add(SNew(SWRChoice, [this, Id]() { return Path == Id; }).Path(Id).Text(Label).OnClicked(this, &SWRMainMenu::OnPickPath, Id));
		}
		return ChoicePair(Pair[0], Pair[1]);
	}
	// More: a grid of three across (glyph + the path's name; the line under it says the land), so twelve paths take
	// four short rows instead of a column taller than the screen (wild-hotfix-menus 3.3). The pad's D-pad moves through
	// it by position like the rest of the menu.
	const int32 Cols = FMath::Max(1, static_cast<int32>(WRTune(TEXT("ui.menuPathColumns"), 3.f)));
	TSharedRef<SVerticalBox> Grid = SNew(SVerticalBox);
	TSharedPtr<SHorizontalBox> Row;
	for (int32 i = 0; i < Defs.Num(); ++i)
	{
		if (i % Cols == 0)
		{
			Row = SNew(SHorizontalBox);
			Grid->AddSlot().AutoHeight().Padding(0.f, i > 0 ? 3.f * U : 0.f, 0.f, 0.f)[Row.ToSharedRef()];
		}
		const FName Id = Defs[i].Id;
		Row->AddSlot().FillWidth(1.f).Padding(i % Cols > 0 ? 3.f * U : 0.f, 0.f, 0.f, 0.f)
		[
			SNew(SWRChoice, [this, Id]() { return Path == Id; }).Path(Id).Height(30.f).FontSize(12)
			.Text(FText::FromString(WRPathLook::Name(Id))).OnClicked(this, &SWRMainMenu::OnPickPath, Id)
		];
	}
	// A short last row keeps its cells the grid's width.
	for (int32 i = Defs.Num(); Row.IsValid() && i % Cols != 0; ++i)
	{
		Row->AddSlot().FillWidth(1.f).Padding(3.f * U, 0.f, 0.f, 0.f)[SNullWidget::NullWidget];
	}
	return SNew(SBorder).BorderImage(WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.5f), 5.f, FLinearColor::Black, 1.f)).Padding(3.f * U)[Grid];
}

FReply SWRMainMenu::OnPickPath(FName InPath)
{
	Path = InPath;
	return FReply::Handled();
}

void SWRMainMenu::GetLook(int32& Face, int32& Build, bool& bHideHelmet)
{
	Face = 0;
	Build = 1;
	bHideHelmet = false;
	FString Look;
	GConfig->GetString(MenuSection, TEXT("Look"), Look, GGameUserSettingsIni);
	TArray<FString> Parts;
	Look.ParseIntoArray(Parts, TEXT("."));
	if (Parts.Num() >= 1) Face = FMath::Clamp(FCString::Atoi(*Parts[0]), 0, 31);
	if (Parts.Num() >= 2) Build = FMath::Clamp(FCString::Atoi(*Parts[1]), 0, 7);
	bHideHelmet = Parts.Num() >= 3 && Parts[2].Contains(TEXT("h"));
}

void SWRMainMenu::SetLook(int32 Face, int32 Build, bool bHideHelmet)
{
	const FString Look = FString::Printf(TEXT("%d.%d%s"), FMath::Clamp(Face, 0, 31), FMath::Clamp(Build, 0, 7), bHideHelmet ? TEXT(".h") : TEXT(""));
	GConfig->SetString(MenuSection, TEXT("Look"), *Look, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

FReply SWRMainMenu::OnPickBody(FName InBody)
{
	Body = InBody;
	// The other body has its own faces: keep the index inside its list, and show it on the survivor if looking.
	LookFace = FMath::Clamp(LookFace, 0, FMath::Max(0, FaceNames().Num() - 1));
	SetLook(LookFace, LookBuild, bLookHideHelmet);
	if (bLookOpen) ApplyPreview();
	return FReply::Handled();
}

TArray<FString> SWRMainMenu::FaceNames() const
{
	TArray<FString> Out;
	const TSharedPtr<FJsonObject> Art = FWRTuning::LoadDataFile(TEXT("player_art.json"));
	const TSharedPtr<FJsonObject>* Hero = nullptr;
	const TSharedPtr<FJsonObject>* Sex = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Faces = nullptr;
	if (Art.IsValid() && Art->TryGetObjectField(TEXT("hero"), Hero) && (*Hero)->TryGetObjectField(Body == TEXT("f") ? TEXT("f") : TEXT("m"), Sex) && (*Sex)->TryGetArrayField(TEXT("faces"), Faces))
	{
		for (const TSharedPtr<FJsonValue>& F : *Faces)
		{
			FString Name;
			const TSharedPtr<FJsonObject>* O = nullptr;
			if (F.IsValid() && F->TryGetObject(O)) (*O)->TryGetStringField(TEXT("name"), Name);
			Out.Add(Name.IsEmpty() ? FString::Printf(TEXT("Face %d"), Out.Num() + 1) : Name);
		}
	}
	if (Out.Num() == 0) Out.Add(TEXT("Face 1"));
	return Out;
}

FText SWRMainMenu::LookSummary() const
{
	const TArray<FString> Faces = FaceNames();
	static const TCHAR* BuildsM[] = { TEXT("Lean"), TEXT("Athletic"), TEXT("Strong") };
	static const TCHAR* BuildsF[] = { TEXT("Slim"), TEXT("Athletic"), TEXT("Curvy") };
	const int32 B = FMath::Clamp(LookBuild, 0, 2);
	return FText::FromString(FString::Printf(TEXT("%s  ·  %s"), *Faces[FMath::Clamp(LookFace, 0, Faces.Num() - 1)], Body == TEXT("f") ? BuildsF[B] : BuildsM[B]));
}

TSharedRef<SWidget> SWRMainMenu::MakeLookPanel()
{
	using namespace WRMenuUI;
	GetLook(LookFace, LookBuild, bLookHideHelmet);
	const auto Section = [](const FText& T) { return WRUI::Text(T, Spaced(WRUI::Caps(12), 80), WRUI::Ink2); };
	// The faces: one choice each, by name (up to the art's count for either body: player_art.json has six each).
	TSharedRef<SVerticalBox> FaceList = SNew(SVerticalBox);
	for (int32 i = 0; i < 12; ++i)
	{
		FaceList->AddSlot().AutoHeight().Padding(0.f, i > 0 ? 4.f * U : 0.f, 0.f, 0.f)
		[
			SNew(SBox).Visibility_Lambda([this, i]() { return i < FaceNames().Num() ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				SNew(SWRChoice, [this, i]() { return LookFace == i; })
				.Text_Lambda([this, i]() { const TArray<FString> N = FaceNames(); return FText::FromString(N.IsValidIndex(i) ? N[i] : FString()); })
				.OnClicked_Lambda([this, i]() { LookFace = i; SetLook(LookFace, LookBuild, bLookHideHelmet); ApplyPreview(); return FReply::Handled(); })
			]
		];
	}
	const auto BuildChoice = [this](int32 B, const FText& M, const FText& F)
	{
		return SNew(SWRChoice, [this, B]() { return LookBuild == B; })
			.Text_Lambda([this, M, F]() { return Body == TEXT("f") ? F : M; })
			.OnClicked_Lambda([this, B]() { LookBuild = B; SetLook(LookFace, LookBuild, bLookHideHelmet); ApplyPreview(); return FReply::Handled(); });
	};
	return SNew(SBox).Padding(FMargin(20.f * U, 0.f, 0.f, 0.f)).WidthOverride(350.f * U).Visibility_Lambda([this]() { return bLookOpen ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
	[
		SNew(SWRPanel).Kind(EWRPanel::Leather).Rivets(true).Padding(FMargin(20.f * U, 16.f * U))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(NSLOCTEXT("WRUI", "LookTitle", "Your look"), Spaced(WRUI::Title(14), 60), WRUI::Ink, true)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f * U, 0.f, 0.f)
			[
				WRUI::Text(NSLOCTEXT("WRUI", "LookSub", "Shown on your survivor behind the menu. Change it any time here."), WRUI::Body(12, 500), WRUI::Ink3, false, true)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f * U, 0.f, 6.f * U)[Section(NSLOCTEXT("WRUI", "LookFace", "Face"))]
			+ SVerticalBox::Slot().AutoHeight()[FaceList]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f * U, 0.f, 6.f * U)[Section(NSLOCTEXT("WRUI", "LookBuild", "Build"))]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 2.f * U, 0.f)[BuildChoice(0, NSLOCTEXT("WRUI", "BuildLean", "Lean"), NSLOCTEXT("WRUI", "BuildSlim", "Slim"))]
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(2.f * U, 0.f)[BuildChoice(1, NSLOCTEXT("WRUI", "BuildAthletic", "Athletic"), NSLOCTEXT("WRUI", "BuildAthletic", "Athletic"))]
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(2.f * U, 0.f, 0.f, 0.f)[BuildChoice(2, NSLOCTEXT("WRUI", "BuildStrong", "Strong"), NSLOCTEXT("WRUI", "BuildCurvy", "Curvy"))]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f * U, 0.f, 6.f * U)[Section(NSLOCTEXT("WRUI", "LookHelmet", "Helmet"))]
			+ SVerticalBox::Slot().AutoHeight()
			[
				ChoicePair(
					SNew(SWRChoice, [this]() { return !bLookHideHelmet; }).Text(NSLOCTEXT("WRUI", "HelmetShown", "Shown"))
					.OnClicked_Lambda([this]() { bLookHideHelmet = false; SetLook(LookFace, LookBuild, false); ApplyPreview(); return FReply::Handled(); }),
					SNew(SWRChoice, [this]() { return bLookHideHelmet; }).Text(NSLOCTEXT("WRUI", "HelmetHidden", "Hidden"))
					.OnClicked_Lambda([this]() { bLookHideHelmet = true; SetLook(LookFace, LookBuild, true); ApplyPreview(); return FReply::Handled(); }))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 16.f * U, 0.f, 0.f)
			[
				SNew(SWRButton).Primary(true).FontSize(13).MinHeight(42.f * U).Text(NSLOCTEXT("WRUI", "LookDone", "Done"))
				.OnClicked_Lambda([this]() { bLookOpen = false; EndFraming(); return FReply::Handled(); })
			]
		]
	];
}

void SWRMainMenu::ApplyPreview()
{
	APlayerController* PC = Owner.Get();
	if (AWRCharacter* Me = PC ? Cast<AWRCharacter>(PC->GetPawn()) : nullptr)
	{
		Me->PreviewLook(Body, uint8(FMath::Clamp(LookFace, 0, 255)), uint8(FMath::Clamp(LookBuild, 0, 255)), bLookHideHelmet);
	}
}

bool SWRMainMenu::StageRect(FVector2D& OutMin, FVector2D& OutMax) const
{
	if (!Stage.IsValid()) return false;
	const FGeometry& Root = GetTickSpaceGeometry();
	const FVector2D RootPos = Root.GetAbsolutePosition(), RootSize = Root.GetAbsoluteSize();
	const FGeometry& G = Stage->GetTickSpaceGeometry();
	if (RootSize.X < 1.f || RootSize.Y < 1.f || G.GetAbsoluteSize().X < 1.f || G.GetAbsoluteSize().Y < 1.f) return false;
	FVector2D Min = G.GetAbsolutePosition(), Max = Min + G.GetAbsoluteSize();
	// The update banner sits over the stage's bottom right when a new version is out: keep the survivor above it.
	if (UpdateBox.IsValid() && UpdateBox->GetVisibility().IsVisible() && !LatestVersion.IsEmpty())
	{
		const FGeometry& UG = UpdateBox->GetTickSpaceGeometry();
		const FVector2D UMin = UG.GetAbsolutePosition();
		if (UMin.X < Max.X && UMin.Y > Min.Y + 0.4f * (Max.Y - Min.Y)) Max.Y = FMath::Min(Max.Y, UMin.Y);
	}
	OutMin = (Min - RootPos) / RootSize;
	OutMax = (Max - RootPos) / RootSize;
	// Breathing room inside the stage.
	const FVector2D Inset = (OutMax - OutMin) * WRTune(TEXT("flow.lookCamera.stageInset"), 0.04f);
	OutMin += Inset;
	OutMax -= Inset;
	return OutMax.X - OutMin.X > 0.05f && OutMax.Y - OutMin.Y > 0.2f;
}

void SWRMainMenu::FrameSurvivor(float DeltaTime)
{
	APlayerController* PC = Owner.Get();
	ACameraActor* Cam = Backdrop.Get();
	const ACharacter* Me = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	if (!Cam || !Me) return;
	// A full-length portrait framed into the stage, the free space right of the panels (wild-hotfix-menus 3.4; how
	// Baldur's Gate 3 and Monster Hunter frame their creators): never behind a panel, whatever the screen's shape.
	const float Fov = WRTune(TEXT("flow.lookCamera.fov"), 40.f);
	const FVector2D VpSize = GetTickSpaceGeometry().GetAbsoluteSize();
	const float Aspect = VpSize.Y > 1.f ? FMath::Max(0.3f, static_cast<float>(VpSize.X / VpSize.Y)) : 16.f / 9.f;
	// The lens: which axis keeps the FOV is the local player's choice (the game keeps the vertical one, measured on the
	// camera's own 16:9, so a 4:3 screen sees less to the sides rather than more above and below).
	const UCameraComponent* Lens = Cam->GetCameraComponent();
	const float TanF = FMath::Tan(FMath::DegreesToRadians(Fov * 0.5f));
	EAspectRatioAxisConstraint Axis = AspectRatio_MaintainXFOV;
	if (const ULocalPlayer* LP = PC->GetLocalPlayer()) Axis = LP->AspectRatioAxisConstraint;
	if (Lens && Lens->bOverrideAspectRatioAxisConstraint) Axis = Lens->AspectRatioAxisConstraint;
	const bool bKeepY = Axis == AspectRatio_MaintainYFOV || (Axis == AspectRatio_MajorAxisFOV && Aspect < 1.f);
	const float LensAspect = Lens && Lens->AspectRatio > 0.1f ? Lens->AspectRatio : 16.f / 9.f;
	const float TanV = bKeepY ? TanF / LensAspect : TanF / Aspect;
	const float TanH = TanV * Aspect;
	FVector2D SMin(0.62f, 0.12f), SMax(0.96f, 0.94f);   // until the stage has been measured
	StageRect(SMin, SMax);
	const FVector2D Centre = (SMin + SMax) * 0.5f, Span = SMax - SMin;
	// Feet to crown with head and foot room, centred on the stage.
	const float BodyH = WRTune(TEXT("flow.lookCamera.bodyHeight"), 185.f);
	const float HeadRoom = WRTune(TEXT("flow.lookCamera.headRoom"), 0.1f), FootRoom = WRTune(TEXT("flow.lookCamera.footRoom"), 0.06f);
	// The feet are where the mesh stands (its root), not the capsule's bottom; a scaled body is taller.
	// (The survivor's body stands on the capsule's bottom: AWRCharacter sets it there.)
	const float Feet = Me->GetActorLocation().Z - (Me->GetCapsuleComponent() ? Me->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f);
	const float Height = BodyH;
	const float FrameH = Height * (1.f + HeadRoom + FootRoom);
	const FVector Target(Me->GetActorLocation().X, Me->GetActorLocation().Y, Feet - FootRoom * Height + FrameH * 0.5f);
	const float ByHeight = FrameH / (2.f * FMath::Max(0.05f, static_cast<float>(Span.Y)) * TanV);
	const float ByWidth = Height * 0.45f / (2.f * FMath::Max(0.05f, static_cast<float>(Span.X)) * TanH);
	const float Dist = FMath::Clamp(FMath::Max(ByHeight, ByWidth), WRTune(TEXT("flow.lookCamera.minDistance"), 180.f), WRTune(TEXT("flow.lookCamera.maxDistance"), 900.f));
	// In front of the survivor a little off the face's line; the camera kept out of rock by testing a few angles.
	const float Yaw0 = Me->GetActorRotation().Yaw + WRTune(TEXT("flow.lookCamera.yaw"), 20.f);
	FVector Best = Target + FRotator(-4.f, Yaw0, 0.f).Vector() * Dist;   // (a touch below the middle: a hero's eye line)
	for (const float Turn : { 0.f, -40.f, 40.f, -80.f, 80.f, 180.f })
	{
		const FVector Try = Target + FRotator(-4.f, Yaw0 + Turn, 0.f).Vector() * Dist;
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRLookCamera), false, Me);
		if (!Me->GetWorld()->LineTraceSingleByChannel(Hit, Target, Try, ECC_Camera, Q))
		{
			Best = Try;
			break;
		}
	}
	// Turned so the survivor lands on the stage's centre: a point at angle a off the view axis shows at tan(a) / tan(fov/2).
	const float Nx = 2.f * static_cast<float>(Centre.X) - 1.f, Ny = 1.f - 2.f * static_cast<float>(Centre.Y);
	FRotator Look = (Target - Best).Rotation();
	Look.Yaw -= FMath::RadiansToDegrees(FMath::Atan(Nx * TanH));
	Look.Pitch -= FMath::RadiansToDegrees(FMath::Atan(Ny * TanV));
	// Eased toward the framing so opening the picker glides rather than cuts.
	const float Alpha = FMath::Clamp(DeltaTime * 4.f, 0.f, 1.f);
	Cam->SetActorLocationAndRotation(FMath::Lerp(Cam->GetActorLocation(), Best, Alpha), FMath::RInterpTo(Cam->GetActorRotation(), Look, DeltaTime, 4.f));
	Cam->GetCameraComponent()->SetFieldOfView(Fov);
	// (Verbose: where the head and feet land on the screen, to check the framing against the stage.)
	if (UE_LOG_ACTIVE(LogWildreach, Verbose) && FPlatformTime::Seconds() >= NextFrameLog)
	{
		NextFrameLog = FPlatformTime::Seconds() + 2.0;
		FVector2D HeadPx, FeetPx;
		const bool bH = PC->ProjectWorldLocationToScreen(FVector(Target.X, Target.Y, Feet + Height), HeadPx);
		const bool bF = PC->ProjectWorldLocationToScreen(FVector(Target.X, Target.Y, Feet), FeetPx);
		int32 VX = 0, VY = 0;
		PC->GetViewportSize(VX, VY);
		UE_LOG(LogWildreach, Verbose, TEXT("MENU look framing: stage %.2f,%.2f-%.2f,%.2f aspect %.2f dist %.0f feet %.0f height %.0f | cam fov %.1f at %s | head %d %.0f,%.0f feet %d %.0f,%.0f of %dx%d"),
			SMin.X, SMin.Y, SMax.X, SMax.Y, Aspect, Dist, Feet, Height, PC->PlayerCameraManager ? PC->PlayerCameraManager->GetFOVAngle() : -1.f,
			*Cam->GetActorLocation().ToCompactString(), bH, HeadPx.X, HeadPx.Y, bF, FeetPx.X, FeetPx.Y, VX, VY);
	}
	// A soft warm key light beside the camera, so the face reads at night too.
	if (!LookLight.IsValid())
	{
		UPointLightComponent* L = NewObject<UPointLightComponent>(Cam);
		L->SetupAttachment(Cam->GetRootComponent());
		L->SetRelativeLocation(FVector(0.f, -120.f, 80.f));
		L->SetIntensityUnits(ELightUnits::Candelas);
		L->SetIntensity(WRTune(TEXT("flow.lookCamera.light"), 120.f));
		L->SetAttenuationRadius(WRTune(TEXT("flow.lookCamera.lightRadius"), 900.f));
		L->SetLightColor(FLinearColor(1.f, 0.88f, 0.74f));
		L->SetCastShadows(false);
		L->RegisterComponent();
		LookLight = L;
	}
}

void SWRMainMenu::EndFraming()
{
	if (UPointLightComponent* L = LookLight.Get()) L->DestroyComponent();
	LookLight.Reset();
	if (APlayerController* PC = Owner.Get())
	{
		ShowBackdrop(PC, true);   // back to the island view
	}
}

FString SWRMainMenu::Options() const
{
	FString Name = NameBox->GetText().ToString().TrimStartAndEnd();
	// Keep names to what the server's file names and chat can handle.
	Name = Name.Replace(TEXT("?"), TEXT("")).Replace(TEXT("="), TEXT("")).Replace(TEXT(" "), TEXT("_")).Left(20);
	if (Name.IsEmpty()) Name = TEXT("Survivor");
	// The face and build (cook hero-look-2): [Wildreach.Menu] Look=<face>.<build>[.h], written by the creator.
	FString Look;
	GConfig->GetString(MenuSection, TEXT("Look"), Look, GGameUserSettingsIni);
	Look = Look.Replace(TEXT("?"), TEXT("")).Replace(TEXT("="), TEXT("")).Left(12);
	// This PC's identity token (WRIdentity): the server keeps the name for whoever claimed it first.
	const FString Key = WRIdentity::Token();
	// A password typed on the card for this address (its hash: the typed text never leaves this PC).
	FString Address;
	GConfig->GetString(MenuSection, TEXT("Address"), Address, GGameUserSettingsIni);
	const FString Pass = !TypedPassword.IsEmpty() && PasswordAddress == Address ? WRAccess::WireHash(TypedPassword) : FString();
	return FString::Printf(TEXT("?WRName=%s?Path=%s%s%s%s%s"), *Name, *Path.ToString(), Body.IsNone() ? TEXT("") : *(TEXT("?Body=") + Body.ToString()),
		Look.IsEmpty() ? TEXT("") : *(TEXT("?Look=") + Look), Key.IsEmpty() ? TEXT("") : *(TEXT("?WRKey=") + Key), Pass.IsEmpty() ? TEXT("") : *(TEXT("?WRPass=") + Pass));
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
	// Choose a world first; it covers the menu as the settings window does (they share the slot).
	FString LastWorld;
	GConfig->GetString(MenuSection, TEXT("World"), LastWorld, GGameUserSettingsIni);
	const TWeakPtr<SWRMainMenu> Weak = SharedThis(this);
	SettingsHost->SetContent(SNew(SWRAppear).Seconds(WRTune(TEXT("flow.screenInSeconds"), 0.15f)).Offset(FVector2D(0.f, WRTune(TEXT("flow.screenRise"), 14.f)))
	[
		WRUI::MakeWorldChooser(LastWorld,
			[Weak](const FString& Id) { if (const TSharedPtr<SWRMainMenu> Menu = Weak.Pin()) Menu->HostWorld(Id); },
			FSimpleDelegate::CreateLambda([Weak]() { if (const TSharedPtr<SWRMainMenu> Menu = Weak.Pin()) Menu->SettingsHost->SetContent(SNullWidget::NullWidget); }))
	]);
	return FReply::Handled();
}

void SWRMainMenu::HostWorld(const FString& WorldId)
{
	bJoining = false;   // hosting: a failure from here on isn't a join to retry
	APlayerController* PC = Owner.Get();
	if (!PC)
	{
		return;
	}
	Remember();
	GConfig->SetString(MenuSection, TEXT("World"), *WorldId, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
	const FString Opts = FString::Printf(TEXT("listen%s?World=%s"), *Options(), *WorldId);
	UE_LOG(LogWildreach, Log, TEXT("MENU host: L_Island?%s"), *Opts);
	Close();
	UGameplayStatics::OpenLevel(PC, TEXT("/Game/Maps/L_Island"), true, Opts);
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
	bJoining = true;
	RejoinTries = 0;
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
	if (WRMenuUpdate::bBusy || LatestVersion.IsEmpty()) return FReply::Handled();
	WRMenuUpdate::bBusy = true;
	const TWeakObjectPtr<APlayerController> PC = Owner;
	if (WRMenuUpdate::IsFreedom35())
	{
		// Freedom35's updater beside the game: started now, it finds the new version, waits for this game to close,
		// fetches the files that changed and starts Wildreach again. Nothing else is ever downloaded from here.
		const FString F35 = WRMenuUpdate::F35File(TEXT("F35Update.exe"));
		FProcHandle Proc = FPaths::FileExists(F35)
			? FPlatformProcess::CreateProc(*F35, TEXT(""), true, false, false, nullptr, 0, *FPaths::GetPath(F35), nullptr) : FProcHandle();
		UE_LOG(LogWildreach, Log, TEXT("MENU update: %s (%s)"), *F35, Proc.IsValid() ? TEXT("started") : TEXT("couldn't start"));
		if (!Proc.IsValid())
		{
			WRMenuUpdate::Fail(NSLOCTEXT("WRUI", "UpdateF35NoStart", "Couldn't start the update. Close Wildreach and start it from its shortcut: it updates before it opens."));
			return FReply::Handled();
		}
		FPlatformProcess::CloseProc(Proc);
		WRMenuUpdate::Line = WRMenuUpdate::UpdatingLine(LatestVersion);
		WRMenuUpdate::QuitSoon(PC);
		return FReply::Handled();
	}
	// The updater waits for this game to close, installs the new release over its own folder and starts it.
	const FString Updater = WRMenuUpdate::FindUpdater();
	if (Updater.IsEmpty())
	{
		UE_LOG(LogWildreach, Log, TEXT("MENU update: no WildreachUpdater.exe for this copy (%s): fetching the release's Setup"), *WRMenuUpdate::GameFolder());
		WRMenuUpdate::DownloadSetup(LatestVersion, PC);
		return FReply::Handled();
	}
	const FString Args = FString::Printf(TEXT("--update --wait %u"), FPlatformProcess::GetCurrentProcessId());
	FProcHandle Proc = FPlatformProcess::CreateProc(*Updater, *Args, true, false, false, nullptr, 0, *FPaths::GetPath(Updater), nullptr);
	UE_LOG(LogWildreach, Log, TEXT("MENU update: %s %s (%s)"), *Updater, *Args, Proc.IsValid() ? TEXT("started") : TEXT("couldn't start"));
	if (!Proc.IsValid())
	{
		// The updater is there but won't start (a virus scanner holding it, say): the release's Setup does the same job.
		WRMenuUpdate::DownloadSetup(LatestVersion, PC);
		return FReply::Handled();
	}
	FPlatformProcess::CloseProc(Proc);
	WRMenuUpdate::Line = WRMenuUpdate::UpdatingLine(LatestVersion);
	WRMenuUpdate::QuitSoon(PC);
	return FReply::Handled();
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
	if (!bTestFeed && WRMenuUpdate::IsFreedom35())
	{
		// A Freedom35 install asks only Freedom35 (public, no account): its live version against the one installed here.
		const FString Installed = WRMenuUpdate::Freedom35Version();
		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Ask = FHttpModule::Get().CreateRequest();
		Ask->SetURL(WRMenuUpdate::Freedom35Latest());
		Ask->SetVerb(TEXT("GET"));
		Ask->SetHeader(TEXT("User-Agent"), TEXT("Wildreach"));
		Ask->SetTimeout(10.f);
		Ask->OnProcessRequestComplete().BindLambda([Installed](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
		{
			TSharedPtr<FJsonObject> Json;
			FString Live;
			if (bConnected && Response.IsValid() && Response->GetResponseCode() == 200
				&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Response->GetContentAsString()), Json) && Json.IsValid())
			{
				Json->TryGetStringField(TEXT("version"), Live);
			}
			Live = Live.TrimStartAndEnd();
			if (!Live.IsEmpty() && IsNewerVersion(Live, Installed.IsEmpty() ? GameVersion() : Installed)) LatestVersion = Live;
			UE_LOG(LogWildreach, Log, TEXT("MENU update check (Freedom35): installed %s, live %s%s"), Installed.IsEmpty() ? TEXT("?") : *Installed,
				Live.IsEmpty() ? TEXT("unknown (offline?)") : *Live, LatestVersion.IsEmpty() ? TEXT("") : TEXT(": offering the update"));
		});
		Ask->ProcessRequest();
		return;
	}
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

void SWRMainMenu::NoteFailure(const FString& Title, const FString& Line, bool bRejoin)
{
	const double Now = FPlatformTime::Seconds();
	// A new trouble long after the last Rejoin press starts its own count.
	if (Now - LastRejoinAt > WRTune(TEXT("ui.rejoin.windowSeconds"), 60.f) + 30.f) RejoinTries = 0;
	LastFailAt = Now;
	CardKind = 1;
	CardTitle = Title;
	LastFailure = Line;
	bCanRejoin = bRejoin;
	NextRejoinAt = 0.0;
	if (RejoinTries > 0 && bRejoin)
	{
		// A Rejoin didn't get in: count down to the next try, until the tries or the window run out.
		const int32 Max = static_cast<int32>(WRTune(TEXT("ui.rejoin.tries"), 6.f));
		if (RejoinTries < Max && Now - RejoinFirstAt < WRTune(TEXT("ui.rejoin.windowSeconds"), 60.f))
		{
			NextRejoinAt = Now + WRTune(TEXT("ui.rejoin.everySeconds"), 10.f);
		}
		else
		{
			LastFailure = TEXT("The host isn't answering. Try again later, or ask them to check their game is still hosting.");
			bCanRejoin = false;
			RejoinTries = 0;
		}
	}
}

void SWRMainMenu::NoteLeaving(const FString& Title, const FString& Line)
{
	LeaveTitle = Title;
	LeaveLine = Line;
	LeaveAt = FPlatformTime::Seconds();
	UE_LOG(LogWildreach, Log, TEXT("MENU leaving: %s: %s"), *Title, *Line);
}

void SWRMainMenu::JoinWithPassword()
{
	if (!PasswordBox.IsValid()) return;
	TypedPassword = PasswordBox->GetText().ToString();
	GConfig->GetString(MenuSection, TEXT("Address"), PasswordAddress, GGameUserSettingsIni);
	PasswordBox->SetText(FText::GetEmpty());
	bNeedPassword = false;
	RejoinTries = 0;   // (a password try isn't a retry of a host that isn't answering)
	Rejoin();
}

void SWRMainMenu::WatchNetworkFailures()
{
	static bool bWatching = false;
	if (bWatching || !GEngine)
	{
		return;
	}
	bWatching = true;
	GEngine->OnNetworkFailure().AddLambda([](UWorld*, UNetDriver* Driver, ENetworkFailure::Type Type, const FString& Error)
	{
		UE_LOG(LogWildreach, Log, TEXT("MENU network failure %s: %s"), ENetworkFailure::ToString(Type), *Error);
		// One failure often comes as several calls (the driver, then the travel): the first plain line stands.
		const double Now = FPlatformTime::Seconds();
		const bool bVersion = Type == ENetworkFailure::OutdatedClient || Type == ENetworkFailure::OutdatedServer;
		if (CardKind == 1 && Now - LastFailAt < 2.0 && !bVersion) return;
		const bool bClient = Driver && Driver->ServerConnection != nullptr;
		// Kicked, banned, or the host closed the world: the word that came first says it (no Rejoin).
		if (!LeaveLine.IsEmpty() && Now - LeaveAt < 20.0)
		{
			const FString Title = LeaveTitle, Said = LeaveLine;
			LeaveLine.Reset();
			bNeedPassword = false;
			NoteFailure(Title, Said, false);
			return;
		}
		FString Line;
		bool bRejoin = bClient;
		bNeedPassword = false;
		// A full world's queue (WRAccess::Refuse, cooks/build-wild-100players.md 3.6): "QUEUE|pos|waiting|playing|cap".
		// The card keeps the place: it tries again every net.queue.retrySeconds, with no limit, until it gets in or
		// the player leaves the queue.
		if ((Type == ENetworkFailure::FailureReceived || Type == ENetworkFailure::PendingConnectionFailure) && Error.StartsWith(TEXT("QUEUE|")))
		{
			TArray<FString> Parts;
			Error.ParseIntoArray(Parts, TEXT("|"), false);
			const int32 Pos = Parts.IsValidIndex(1) ? FCString::Atoi(*Parts[1]) : 0;
			const int32 Count = Parts.IsValidIndex(2) ? FCString::Atoi(*Parts[2]) : 0;
			const int32 Playing = Parts.IsValidIndex(3) ? FCString::Atoi(*Parts[3]) : 0;
			const int32 Cap = Parts.IsValidIndex(4) ? FCString::Atoi(*Parts[4]) : 0;
			const int32 Tens = Pos % 100;
			const TCHAR* Suffix = (Tens >= 11 && Tens <= 13) ? TEXT("th") : Pos % 10 == 1 ? TEXT("st") : Pos % 10 == 2 ? TEXT("nd") : Pos % 10 == 3 ? TEXT("rd") : TEXT("th");
			const FString Where = Pos <= 1 ? FString(TEXT("You're next in the queue")) : FString::Printf(TEXT("You're %d%s in the queue"), Pos, Suffix);
			RejoinTries = 0;   // (a queue isn't a host that's down: no limit on tries)
			NoteFailure(TEXT("World full"), FString::Printf(TEXT("%s (%d waiting; %d of %d playing). Your place is kept while this card is open."), *Where, FMath::Max(Count, Pos), Playing, Cap), true);
			bQueued = true;
			bNeedPassword = false;
			NextRejoinAt = FPlatformTime::Seconds() + FMath::Max(3.f, WRTune(TEXT("net.queue.retrySeconds"), 8.f));
			return;
		}
		bQueued = false;
		// The world's own refusals (WRAccess::Refuse): "PASSWORD|..." asks for the password, "WR|..." reads as it is.
		if ((Type == ENetworkFailure::FailureReceived || Type == ENetworkFailure::PendingConnectionFailure) && (Error.StartsWith(TEXT("PASSWORD|")) || Error.StartsWith(TEXT("WR|"))))
		{
			const bool bPassword = Error.StartsWith(TEXT("PASSWORD|"));
			FString Said;
			Error.Split(TEXT("|"), nullptr, &Said);
			bNeedPassword = bPassword;
			NoteFailure(bPassword ? TEXT("Password needed") : TEXT("Couldn't join"), Said.Left(200), !bPassword && !Said.StartsWith(TEXT("Banned")));
			return;
		}
		switch (Type)
		{
		case ENetworkFailure::OutdatedClient:
		case ENetworkFailure::OutdatedServer:
			Line = FString::Printf(TEXT("The host has a different version of Wildreach (you have %s). Everyone needs the same one: start Wildreach from its shortcut to update."), *GameVersion());
			bRejoin = false;
			break;
		case ENetworkFailure::ConnectionLost:
			Line = TEXT("Lost the connection to the host. Their game may have closed, or the network dropped.");
			break;
		case ENetworkFailure::ConnectionTimeout:
			Line = TEXT("The host stopped answering.");
			break;
		case ENetworkFailure::FailureReceived:
		case ENetworkFailure::PendingConnectionFailure:
			Line = Error.IsEmpty() ? FString(TEXT("The host didn't let us in.")) : FString::Printf(TEXT("The host didn't let us in: %s"), *Error.Left(160));
			break;
		case ENetworkFailure::NetDriverAlreadyExists:
		case ENetworkFailure::NetDriverCreateFailure:
		case ENetworkFailure::NetDriverListenFailure:
			Line = TEXT("Couldn't start the network connection. Close other copies of Wildreach and try again.");
			bRejoin = false;
			break;
		case ENetworkFailure::NetGuidMismatch:
		case ENetworkFailure::NetChecksumMismatch:
			Line = TEXT("Your game's files don't match the host's. Update Wildreach from its shortcut, both of you.");
			bRejoin = false;
			break;
		default:
			Line = Error.IsEmpty() ? FString(TEXT("Disconnected from the host.")) : FString::Printf(TEXT("Disconnected: %s"), *Error.Left(160));
			break;
		}
		NoteFailure(bClient ? TEXT("Disconnected") : TEXT("Couldn't connect"), Line, bRejoin);
	});
	GEngine->OnTravelFailure().AddLambda([](UWorld*, ETravelFailure::Type Type, const FString& Error)
	{
		UE_LOG(LogWildreach, Log, TEXT("MENU travel failure %s: %s"), ETravelFailure::ToString(Type), *Error);
		if (CardKind == 1 && FPlatformTime::Seconds() - LastFailAt < 2.0) return;
		NoteFailure(TEXT("Couldn't connect"), Error.IsEmpty() ? FString(TEXT("Couldn't reach that host. Check the address, and that their game is hosting.")) : FString::Printf(TEXT("Couldn't reach that host: %s"), *Error.Left(160)), bJoining);
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
	WRBootMark::Line(TEXT("F35READY menu"));
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(Instance);
	PC->SetInputMode(Mode);
	PC->bShowMouseCursor = true;
	ShowBackdrop(PC, true);
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
			ShowBackdrop(PC, false);
			if (const AWRPlayerController* WPC = Cast<AWRPlayerController>(PC)) { if (FWRPlayerUI* UI = WPC->GetUI()) UI->ReapplyInputMode(); }
		}
	}
}

void SWRMainMenu::ShowBackdrop(APlayerController* PC, bool bShow)
{
	UWorld* World = PC ? PC->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}
	ACharacter* Survivor = Cast<ACharacter>(PC->GetPawn());
	if (!bShow)
	{
		if (ACameraActor* Cam = Backdrop.Get())
		{
			if (Survivor)
			{
				PC->SetViewTarget(Survivor);
				Survivor->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			}
			Cam->Destroy();
		}
		Backdrop.Reset();
		return;
	}
	// A fixed view of the island for the menu (the tundra forest by the sea at Tundra camp 2) rather than wherever you
	// spawned. World Partition streams around the camera, so the body waits without moving (the ground under it may unload).
	const FVector At(WRTune(TEXT("flow.menuCamera.x"), 114000.f), WRTune(TEXT("flow.menuCamera.y"), -125000.f), WRTune(TEXT("flow.menuCamera.z"), 12500.f));
	const FRotator Look(WRTune(TEXT("flow.menuCamera.pitch"), -6.f), WRTune(TEXT("flow.menuCamera.yaw"), 205.f), 0.f);
	ACameraActor* Cam = Backdrop.Get();
	if (!Cam)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		Cam = World->SpawnActor<ACameraActor>(At, Look, Params);
		Backdrop = Cam;
	}
	if (!Cam)
	{
		return;
	}
	Cam->SetActorLocationAndRotation(At, Look);
	Cam->GetCameraComponent()->SetConstraintAspectRatio(false);
	Cam->GetCameraComponent()->SetFieldOfView(WRTune(TEXT("flow.menuCamera.fov"), 75.f));
	PC->SetViewTarget(Cam);
	if (Survivor && Survivor->HasAuthority())
	{
		Survivor->GetCharacterMovement()->DisableMovement();
	}
	UE_LOG(LogWildreach, Log, TEXT("MENU backdrop camera at %s looking %s"), *At.ToCompactString(), *Look.ToCompactString());
}

bool SWRMainMenu::IsOpen()
{
	return Instance.IsValid();
}

void SWRMainMenu::ShowWorlds()
{
	if (Instance.IsValid()) static_cast<SWRMainMenu&>(*Instance).OnHost();
}

void SWRMainMenu::ShowLook(bool bShow)
{
	if (!Instance.IsValid()) return;
	SWRMainMenu& M = static_cast<SWRMainMenu&>(*Instance);
	if (M.bLookOpen == bShow) return;
	M.bLookOpen = bShow;
	if (bShow) M.ApplyPreview();
	else M.EndFraming();
}

#if !UE_BUILD_SHIPPING
// Testing and screenshots (the bot runs console commands): WRMainMenu opens the front end; WRMainMenu settings also
// opens its settings window.
static FAutoConsoleCommandWithWorldAndArgs GWRMainMenuCommand(TEXT("WRMainMenu"), TEXT("Testing: open the main menu (WRMainMenu settings: with its settings open; WRMainMenu look: the look picker; WRMainMenu worlds: Host a game)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		SWRMainMenu::Open(World ? World->GetFirstPlayerController() : nullptr);
		SWRMainMenu::ShowSettings(Args.Num() > 0 && Args[0] == TEXT("settings"));
		SWRMainMenu::ShowLook(Args.Num() > 0 && Args[0] == TEXT("look"));
		if (Args.Num() > 0 && Args[0] == TEXT("worlds")) SWRMainMenu::ShowWorlds();
	}));
#endif
