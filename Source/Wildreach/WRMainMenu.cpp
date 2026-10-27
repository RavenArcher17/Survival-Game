#include "WRMainMenu.h"
#include "Wildreach.h"
#include "WRPlayerController.h"
#include "WRPlayerUI.h"
#include "WRKeyBindings.h"
#include "WRUISettings.h"
#include "WRUIMotion.h"
#include "WRUIStyle.h"
#include "WRUIWidgets.h"
#include "WRTuning.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
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

TSharedPtr<SWidget> SWRMainMenu::Instance;
TWeakObjectPtr<ACameraActor> SWRMainMenu::Backdrop;
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

	/** One side of a two-way choice: Male / Female, Fire / Frost. Kind 0 plain, 1 fire, 2 frost. */
	class SWRChoice : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SWRChoice) : _Kind(0) {}
			SLATE_ARGUMENT(FText, Text)
			SLATE_ARGUMENT(int32, Kind)
			SLATE_EVENT(FOnClicked, OnClicked)
		SLATE_END_ARGS()
		void Construct(const FArguments& InArgs, TFunction<bool()> InSelected)
		{
			Selected = MoveTemp(InSelected);
			Kind = InArgs._Kind;
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
				WRUI::Text(InArgs._Text, WRUI::Body(14, 700), TAttribute<FSlateColor>::CreateLambda([this]()
				{
					if (!Selected()) return FSlateColor(IsHovered() ? WRUI::Ink2 : WRUI::Ink3);
					return FSlateColor(Kind == 1 ? Hex(TEXT("FFDCC2")) : Kind == 2 ? Hex(TEXT("D8EEFC")) : WRUI::Ink);
				}))
			];
			ChildSlot
			[
				SNew(SButton).ButtonStyle(FCoreStyle::Get(), "NoBorder").ContentPadding(0.f).Cursor(EMouseCursor::Hand).OnClicked(InArgs._OnClicked)
				[
					SNew(SBox).HeightOverride(34.f * U)
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
		TFunction<bool()> Selected;
		int32 Kind = 0;
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
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(72.f * U, 44.f * U, 0.f, 0.f)[MakeTitle()]
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(72.f * U, 226.f * U, 0.f, 0.f)[MakePanel()]
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(0.f, 44.f * U, 48.f * U, 0.f)[MakeTip()]
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 48.f * U, 40.f * U)[MakeUpdate()]
		+ SOverlay::Slot()[SAssignNew(SettingsHost, SBox)]
	];
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
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 11.f * U, 0.f, 0.f)
					[
						FieldRow(NSLOCTEXT("WRUI", "Path", "Path"), ChoicePair(
							SNew(SWRChoice, [this]() { return Path == TEXT("fire"); }).Kind(1).Text(NSLOCTEXT("WRUI", "FirePath", "Fire · Volcanic")).OnClicked(this, &SWRMainMenu::OnPickPath, FName(TEXT("fire"))),
							SNew(SWRChoice, [this]() { return Path == TEXT("frost"); }).Kind(2).Text(NSLOCTEXT("WRUI", "FrostPath", "Frost · Tundra")).OnClicked(this, &SWRMainMenu::OnPickPath, FName(TEXT("frost")))))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(60.f * U, 11.f * U, 0.f, 0.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f * U, 0.f)[SNew(SWRLineIcon).Icon(EWRIcon::Info).Size(15.f * U).Color(WRUI::Brass)]
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
						[
							WRUI::Text(NSLOCTEXT("WRUI", "PathInfo", "Your main path sets where you spawn"), WRUI::Body(12, 500), WRUI::Ink2, false, true)
						]
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f * U, 0.f, 0.f)
			[
				SNew(SWRMenuButton).Primary(true).Height(68.f).FontSize(18).Chevron(true)
				.Icon(EWRIcon::FlagFilled).IconSize(28.f).IconColor(WRUI::Brass).IconAccent(WRUI::Ember)
				.Text(NSLOCTEXT("WRUI", "Host", "Host a game")).Sub(NSLOCTEXT("WRUI", "HostSub", "Friends join your IP · UDP 7777"))
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
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f * U, 0.f, 0.f)
			[
				SNew(SWRMenuButton).Icon(EWRIcon::Gear).IconColor(WRUI::Brass).Text(NSLOCTEXT("WRUI", "SettingsButton", "Settings"))
				.OnClicked_Lambda([]() { ShowSettings(true); return FReply::Handled(); })
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f * U, 0.f, 0.f)
			[
				SNew(SWRMenuButton).Ghost(true).Icon(EWRIcon::Power).IconColor(WRUI::Ink2).IconAccent(WRUI::Bad).Text(NSLOCTEXT("WRUI", "QuitButton", "Quit")).OnClicked(this, &SWRMainMenu::OnQuit)
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
			SNew(SWRKeyCap).FontSize(12).Text_Lambda([this]() { return WRUI::KeyName(FWRKeyBindings::Get().Key(Tips()[Tip].Action)); })
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
		// A newer release on GitHub: one click installs it (the updater) and starts it again.
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
								WRUI::Text(NSLOCTEXT("WRUI", "UpdateSub", "A new version is ready to install"), WRUI::Body(12, 500), WRUI::Ink3, false, true)
							]
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.f * U, 0.f, 0.f, 0.f)
						[
							SNew(SWRButton).Primary(true).FontSize(12).MinHeight(40.f * U).Text(NSLOCTEXT("WRUI", "UpdateNow", "Update now")).OnClicked(this, &SWRMainMenu::OnUpdate)
						]
					]
				]
				// The banner's brighter edge (the mockup's brass border).
				+ SOverlay::Slot()[SNew(SImage).Visibility(EVisibility::HitTestInvisible).Image(WRUI::Box(FLinearColor::Transparent, 5.f, WRUI::Alpha(WRUI::Brass, 0.7f), 1.f))]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.f, 8.f * U, 0.f, 0.f)
		[
			WRUI::Text(FText::Format(NSLOCTEXT("WRUI", "VersionLine", "Version {0}"), FText::FromString(GameVersion())), WRUI::Body(11, 500), WRUI::Ink3, true)
		]
	];
}

void SWRMainMenu::Tick(const FGeometry& Geo, const double Time, const float Delta)
{
	SCompoundWidget::Tick(Geo, Time, Delta);
	// The body is possessed after the menu opens (and takes the view): keep the backdrop camera and the body still.
	if (APlayerController* PC = Owner.Get())
	{
		const ACharacter* Survivor = Cast<ACharacter>(PC->GetPawn());
		if (!Backdrop.IsValid() || PC->GetViewTarget() != Backdrop.Get() || (Survivor && Survivor->HasAuthority() && Survivor->GetCharacterMovement()->MovementMode != MOVE_None))
		{
			ShowBackdrop(PC, true);
		}
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

#if !UE_BUILD_SHIPPING
// Testing and screenshots (the bot runs console commands): WRMainMenu opens the front end; WRMainMenu settings also
// opens its settings window.
static FAutoConsoleCommandWithWorldAndArgs GWRMainMenuCommand(TEXT("WRMainMenu"), TEXT("Testing: open the main menu (WRMainMenu settings: with its settings open)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		SWRMainMenu::Open(World ? World->GetFirstPlayerController() : nullptr);
		SWRMainMenu::ShowSettings(Args.Num() > 0 && Args[0] == TEXT("settings"));
	}));
#endif
