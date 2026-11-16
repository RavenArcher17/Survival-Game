// The main menu's Host a game window, in the settings window's look: the worlds on this PC, last played first, with
// New world (a name, a preset and its rules: WRUI::MakeWorldRules), Play, Rename and Delete (which asks first). WRWorlds
// keeps the folders; the menu hosts the world Play picks (L_Island?listen?World=<id>).

#include "WRUIWorlds.h"
#include "Wildreach.h"
#include "WRSound.h"
#include "WRUISettings.h"
#include "WRUIStyle.h"
#include "WRUIWidgets.h"
#include "WRWorlds.h"
#include "WRAccess.h"
#include "WRStore.h"
#include "WRTuning.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBackgroundBlur.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "WRUI"

namespace WRWorldsUI
{
	using WRUI::U;

	FLinearColor Hex(const TCHAR* H) { return FLinearColor(FColor::FromHex(H)); }

	FSlateFontInfo Spaced(FSlateFontInfo Font, int32 LetterSpacing)
	{
		Font.LetterSpacing = LetterSpacing;
		return Font;
	}

	TSharedRef<SWidget> Label(const FText& Text)
	{
		return WRUI::Text(Text, Spaced(WRUI::Caps(12), 80), WRUI::Ink2);
	}

	TSharedRef<SWidget> Muted(const TAttribute<FText>& Text, int32 Size = 12, bool bWrap = false)
	{
		return WRUI::Text(Text, WRUI::Body(Size, 500), WRUI::Ink3, false, bWrap);
	}

	TSharedRef<SWidget> HLine(const FLinearColor& Color)
	{
		return SNew(SBox).HeightOverride(1.f)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity(Color)];
	}

	/** The title divider: brass that fades out at both ends. */
	TSharedRef<SWidget> FadeLine()
	{
		const FLinearColor B = WRUI::Brass2;
		return SNew(SBox).HeightOverride(1.f)[SNew(SWRGradientFill).Colors({ WRUI::Alpha(B, 0.f), B, B, WRUI::Alpha(B, 0.f) }).Stops({ 0.f, 0.18f, 0.82f, 1.f })];
	}

	const FSlateBrush* InsetBrush() { return WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.32f), 4.f, FLinearColor(0.f, 0.f, 0.f, 0.7f), 1.f); }

	/** A dark recessed text field (as the main menu's name box). */
	const FEditableTextBoxStyle& FieldStyle()
	{
		static const FEditableTextBoxStyle Style = []()
		{
			FEditableTextBoxStyle S = FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");
			const FSlateBrush* Normal = WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.5f), 5.f, FLinearColor::Black, 1.f);
			const FSlateBrush* Hover = WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.5f), 5.f, WRUI::Brass2, 1.f);
			const FSlateBrush* Focus = WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.55f), 5.f, WRUI::Brass, 1.f);
			S.SetBackgroundImageNormal(*Normal).SetBackgroundImageHovered(*Hover).SetBackgroundImageFocused(*Focus).SetBackgroundImageReadOnly(*Normal);
			S.SetForegroundColor(FSlateColor(WRUI::Ink)).SetFocusedForegroundColor(FSlateColor(WRUI::Ink)).SetBackgroundColor(FSlateColor(FLinearColor::White));
			S.SetPadding(FMargin(12.f * U, 8.f * U));
			return S;
		}();
		return Style;
	}

	FLinearColor PresetColor(const FString& Preset)
	{
		if (Preset == TEXT("pvp_standard")) return WRUI::Bad;
		if (Preset == TEXT("casual_coop")) return WRUI::Frost;
		if (Preset == TEXT("playtest")) return WRUI::Warn;
		if (Preset == TEXT("custom")) return WRUI::Brass;
		return WRUI::Good;
	}

	FText PresetBlurb(const FString& Preset)
	{
		if (Preset == TEXT("pvp_standard")) return LOCTEXT("BlurbPvP", "Players can fight and raid each other. Bases are safer while their owners are away.");
		if (Preset == TEXT("casual_coop")) return LOCTEXT("BlurbCasual", "No fighting other players, you keep your things when you die, and twice the XP and gathering.");
		if (Preset == TEXT("playtest")) return LOCTEXT("BlurbPlaytest", "Casual co-op with ten times the XP and taming, for trying things out.");
		return LOCTEXT("BlurbPvE", "No fighting other players. You leave a bag behind when you die, and go back for it.");
	}

	/** "today at 17:32", "yesterday at 9:05", "3 days ago", "12 Oct". Times are kept in UTC. */
	FText When(const FDateTime& Utc)
	{
		const FDateTime Local = Utc + (FDateTime::Now() - FDateTime::UtcNow());
		const int32 Days = (FDateTime::Now().GetDate() - Local.GetDate()).GetDays();
		const FText Clock = FText::FromString(Local.ToString(TEXT("%H:%M")));
		if (Days <= 0) return FText::Format(LOCTEXT("WhenToday", "today at {0}"), Clock);
		if (Days == 1) return FText::Format(LOCTEXT("WhenYesterday", "yesterday at {0}"), Clock);
		if (Days < 7) return FText::Format(LOCTEXT("WhenDays", "{0} days ago"), Days);
		static const TCHAR* Months[] = { TEXT("Jan"), TEXT("Feb"), TEXT("Mar"), TEXT("Apr"), TEXT("May"), TEXT("Jun"), TEXT("Jul"), TEXT("Aug"), TEXT("Sep"), TEXT("Oct"), TEXT("Nov"), TEXT("Dec") };
		const FString Date = FString::Printf(TEXT("%d %s"), Local.GetDay(), Months[FMath::Clamp(Local.GetMonth(), 1, 12) - 1]);
		return FText::FromString(Local.GetYear() == FDateTime::Now().GetYear() ? Date : FString::Printf(TEXT("%s %d"), *Date, Local.GetYear()));
	}

	FText PlayTime(double Seconds)
	{
		const int32 Minutes = FMath::FloorToInt32(Seconds / 60.0);
		if (Minutes < 1) return LOCTEXT("PlayedUnderMinute", "under a minute played");
		if (Minutes < 60) return FText::Format(LOCTEXT("PlayedMinutes", "{0} min played"), Minutes);
		return FText::Format(LOCTEXT("PlayedHours", "{0} h {1} min played"), Minutes / 60, Minutes % 60);
	}

	FText Summary(const FWRWorldInfo& W)
	{
		if (W.PlaySeconds >= 1.0) return FText::Format(LOCTEXT("WorldPlayed", "Played {0} · {1}"), When(W.LastPlayed), PlayTime(W.PlaySeconds));
		// Played, but not counted: the old save's Island, from before play time was kept.
		if (W.LastPlayed > W.Created) return FText::Format(LOCTEXT("WorldPlayedOld", "Played {0}"), When(W.LastPlayed));
		return FText::Format(LOCTEXT("WorldNew", "Not played yet · made {0}"), When(W.Created));
	}

	/** A click that lands where it started (the mouse is held between press and release), and a double click. */
	class SWRWorldClick : public SCompoundWidget
	{
	public:
		virtual FReply OnMouseButtonDown(const FGeometry& Geo, const FPointerEvent& E) override
		{
			return E.GetEffectingButton() == EKeys::LeftMouseButton && OnClick ? FReply::Handled().CaptureMouse(SharedThis(this)) : FReply::Unhandled();
		}
		virtual FReply OnMouseButtonUp(const FGeometry& Geo, const FPointerEvent& E) override
		{
			if (!HasMouseCapture()) return FReply::Unhandled();
			if (Geo.IsUnderLocation(E.GetScreenSpacePosition()) && IsEnabled())
			{
				WRSound::PlayUI(nullptr, TEXT("ui.click"));
				OnClick();
			}
			return FReply::Handled().ReleaseMouseCapture();
		}
		virtual FReply OnMouseButtonDoubleClick(const FGeometry& Geo, const FPointerEvent& E) override
		{
			if (E.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
			if (OnDouble) OnDouble();
			return FReply::Handled();
		}
		virtual FCursorReply OnCursorQuery(const FGeometry& Geo, const FPointerEvent& E) const override { return FCursorReply::Cursor(EMouseCursor::Hand); }

	protected:
		TFunction<void()> OnClick;
		TFunction<void()> OnDouble;
	};

	/** One world on the list: its flag, name (or the rename box), when it was played, and its preset. */
	class SWRWorldRow : public SWRWorldClick
	{
	public:
		SLATE_BEGIN_ARGS(SWRWorldRow) {}
			SLATE_ARGUMENT(FWRWorldInfo, Info)
			SLATE_ARGUMENT(TSharedPtr<SWidget>, NameEditor)
		SLATE_END_ARGS()
		void Construct(const FArguments& InArgs, TFunction<bool()> InSelected, TFunction<void()> InClick, TFunction<void()> InDouble)
		{
			Selected = MoveTemp(InSelected);
			OnClick = MoveTemp(InClick);
			OnDouble = MoveTemp(InDouble);
			const FWRWorldInfo& W = InArgs._Info;
			const TSharedRef<SWidget> Name = InArgs._NameEditor.IsValid() ? InArgs._NameEditor.ToSharedRef()
				: WRUI::Text(FText::FromString(W.Name), Spaced(WRUI::Title(17), 40), WRUI::Ink);
			ChildSlot
			[
				SNew(SBox).HeightOverride(76.f * U)
				[
					SNew(SBorder).Padding(FMargin(18.f * U, 0.f, 16.f * U, 0.f)).VAlign(VAlign_Center)
					.BorderImage_Lambda([this]()
					{
						if (Selected()) return WRUI::Box(Hex(TEXT("3A2B17")), 4.f, WRUI::Brass, 1.5f);
						return IsHovered() ? WRUI::Box(FLinearColor(1.f, 0.9f, 0.7f, 0.05f), 4.f, WRUI::Brass2, 1.f) : WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.22f), 4.f, FLinearColor(0.f, 0.f, 0.f, 0.6f), 1.f);
					})
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 16.f * U, 0.f)
						[
							SNew(SWRLineIcon).Icon(EWRIcon::FlagFilled).Size(26.f * U)
							.Color_Lambda([this]() { return Selected() ? WRUI::Brass : WRUI::Ink3; })
							.Accent_Lambda([this]() { return Selected() ? WRUI::Ember : WRUI::Alpha(WRUI::Ink3, 0.6f); })
						]
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()[Name]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f * U, 0.f, 0.f)[WRUI::Text(Summary(W), WRUI::Body(13, 500), WRUI::Ink2)]
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(14.f * U, 0.f, 0.f, 0.f)
						[
							SNew(SWRChip).Text(WRWorlds::PresetName(W.Preset)).Color(PresetColor(W.Preset)).FontSize(12)
						]
					]
				]
			];
		}

	private:
		TFunction<bool()> Selected;
	};

	/** A preset to start a new world from: its name and what it means. */
	class SWRPresetTile : public SWRWorldClick
	{
	public:
		SLATE_BEGIN_ARGS(SWRPresetTile) {}
			SLATE_ARGUMENT(FString, Preset)
		SLATE_END_ARGS()
		void Construct(const FArguments& InArgs, TFunction<bool()> InSelected, TFunction<void()> InClick)
		{
			Selected = MoveTemp(InSelected);
			OnClick = MoveTemp(InClick);
			const FString Preset = InArgs._Preset;
			ChildSlot
			[
				SNew(SBox).HeightOverride(92.f * U)
				[
					SNew(SBorder).Padding(FMargin(14.f * U, 10.f * U)).VAlign(VAlign_Top)
					.BorderImage_Lambda([this]()
					{
						if (Selected()) return WRUI::Box(Hex(TEXT("3A2B17")), 5.f, WRUI::Brass, 1.5f);
						return IsHovered() ? WRUI::Box(FLinearColor(1.f, 0.9f, 0.7f, 0.05f), 5.f, WRUI::Brass2, 1.f) : WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.25f), 5.f, FLinearColor(0.f, 0.f, 0.f, 0.6f), 1.f);
					})
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 9.f * U, 0.f)
							[
								SNew(SBox).WidthOverride(10.f * U).HeightOverride(10.f * U)[SNew(SImage).Image(WRUI::Box(PresetColor(Preset), 5.f * U))]
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								WRUI::Text(WRWorlds::PresetName(Preset), Spaced(WRUI::Title(15), 40), TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(Selected() ? WRUI::Ink : WRUI::Ink2); }))
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 0.f)[WRUI::Text(PresetBlurb(Preset), WRUI::Body(13, 500), WRUI::Ink2, false, true)]
					]
				]
			];
		}

	private:
		TFunction<bool()> Selected;
	};

	/** The close box in the title row. */
	class SWRCloseBox : public SWRWorldClick
	{
	public:
		SLATE_BEGIN_ARGS(SWRCloseBox) {}
		SLATE_END_ARGS()
		void Construct(const FArguments& InArgs, TFunction<void()> InClick)
		{
			OnClick = MoveTemp(InClick);
			ChildSlot
			[
				SNew(SBox).WidthOverride(36.f * U).HeightOverride(36.f * U)
				[
					SNew(SBorder).HAlign(HAlign_Center).VAlign(VAlign_Center)
					.BorderImage_Lambda([this]() { return WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.35f), 4.f, IsHovered() ? WRUI::Brass : WRUI::Brass2, 1.f); })
					[
						SNew(SWRLineIcon).Icon(EWRIcon::Close).Size(16.f * U).Color(WRUI::Ink)
					]
				]
			];
		}
	};

	class SWRWorldChooser : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SWRWorldChooser) {}
			SLATE_ARGUMENT(FString, Selected)
			SLATE_EVENT(FSimpleDelegate, OnClose)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, TFunction<void(const FString&)> InPlay)
		{
			OnPlay = MoveTemp(InPlay);
			OnClose = InArgs._OnClose;
			SelectedId = InArgs._Selected;
			Refresh();
			ChildSlot
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[SNew(SBackgroundBlur).BlurStrength(8.f)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.5f))]]
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(1120.f * U).HeightOverride(720.f * U)
					[
						SNew(SWRPanel).Kind(EWRPanel::Stone).Rivets(true).BrassTrim(true).Padding(FMargin(0.f))
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()[TitleRow()]
							+ SVerticalBox::Slot().AutoHeight().Padding(24.f * U, 0.f)[FadeLine()]
							+ SVerticalBox::Slot().FillHeight(1.f).Padding(FMargin(28.f * U, 20.f * U, 28.f * U, 14.f * U))[SAssignNew(Body, SBox)]
							+ SVerticalBox::Slot().AutoHeight().Padding(24.f * U, 0.f)[HLine(WRUI::Alpha(WRUI::Brass2, 0.45f))]
							+ SVerticalBox::Slot().AutoHeight().Padding(24.f * U, 0.f)[SNew(SBox).HeightOverride(76.f * U)[SAssignNew(Footer, SBox).VAlign(VAlign_Center)]]
						]
					]
				]
			];
			ShowList();
		}

		virtual bool SupportsKeyboardFocus() const override { return true; }

		virtual FReply OnKeyDown(const FGeometry& Geo, const FKeyEvent& E) override
		{
			if (E.GetKey() != EKeys::Escape) return FReply::Unhandled();
			if (Mode == EMode::List) Close();
			else ShowList();
			return FReply::Handled();
		}

		virtual void Tick(const FGeometry& Geo, const double Time, const float Delta) override
		{
			SCompoundWidget::Tick(Geo, Time, Delta);
			// Keyboard focus once things are on screen: a text box being typed in, else this window (for Esc).
			if (FocusNext.IsValid())
			{
				FSlateApplication::Get().SetKeyboardFocus(FocusNext, EFocusCause::SetDirectly);
				FocusNext.Reset();
			}
			else if (!bFocused)
			{
				FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EFocusCause::SetDirectly);
			}
			bFocused = true;
		}

	private:
		enum class EMode : uint8 { List, New, Rename, ConfirmDelete, Hosting, Backups };
		EMode Mode = EMode::List;
		TFunction<void(const FString&)> OnPlay;
		FSimpleDelegate OnClose;
		TArray<FWRWorldInfo> Worlds;
		FString SelectedId;
		TSharedPtr<FWRWorldSettings> NewValues;
		bool bNewChanged = false;
		FText Status;
		bool bFocused = false;
		TSharedPtr<SWidget> FocusNext;
		TSharedPtr<SBox> Body;
		TSharedPtr<SBox> Footer;
		TSharedPtr<SEditableTextBox> NameBox;
		/** The Hosting page's copy of the world's access.json (cooks/cookpass-multiplayer.md 3.4) and its password box. */
		TSharedPtr<FWRAccess> HostAccess;
		TSharedPtr<SEditableTextBox> PassBox;
		FText HostNote;

		/** One Esc in a text box goes back (left alone, the first only clears its selection). */
		FReply OnTextBoxKey(const FGeometry& Geo, const FKeyEvent& E)
		{
			if (E.GetKey() != EKeys::Escape) return FReply::Unhandled();
			ShowList();
			return FReply::Handled();
		}

		void Refresh()
		{
			Worlds = WRWorlds::List();
			if (!Worlds.ContainsByPredicate([this](const FWRWorldInfo& W) { return W.Id == SelectedId; }))
			{
				SelectedId = Worlds.Num() > 0 ? Worlds[0].Id : FString();
			}
		}

		const FWRWorldInfo* SelectedWorld() const
		{
			return Worlds.FindByPredicate([this](const FWRWorldInfo& W) { return W.Id == SelectedId; });
		}

		void Close()
		{
			OnClose.ExecuteIfBound();
		}

		void Play(const FString& Id)
		{
			if (Id.IsEmpty()) return;
			UE_LOG(LogWildreach, Log, TEXT("WORLDS play '%s'"), *Id);
			OnPlay(Id);
		}

		TSharedRef<SWidget> TitleRow()
		{
			return SNew(SBox).HeightOverride(72.f * U).Padding(FMargin(30.f * U, 0.f, 26.f * U, 0.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 14.f * U, 0.f)[SNew(SWRLineIcon).Icon(EWRIcon::FlagFilled).Size(28.f * U).Color(WRUI::Brass).Accent(WRUI::Ember)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(LOCTEXT("ChooseWorld", "Choose a world"), Spaced(WRUI::Title(23), 60), WRUI::Ink, true)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(14.f * U, 0.f)
				[
					SNew(SBox).Visibility_Lambda([this]() { return Mode == EMode::New ? EVisibility::Visible : EVisibility::Collapsed; })
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 14.f * U, 0.f)[WRUI::Text(FText::FromString(TEXT("›")), WRUI::Body(20, 500), WRUI::Ink3)]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(LOCTEXT("NewWorldTitle", "A new world"), Spaced(WRUI::Title(16), 60), WRUI::Ink2, true)]
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f)[SNullWidget::NullWidget]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f * U, 0.f)[Muted(LOCTEXT("CloseHint", "Close"), 12)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 14.f * U, 0.f)[SNew(SWRKeyCap).FontSize(11).Text(LOCTEXT("EscKey", "Esc"))]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SWRCloseBox, [this]() { if (Mode == EMode::List) Close(); else ShowList(); })]
			];
		}

		/** The footer: what's on the left (Back, a question), then the buttons on the right. */
		void SetFooter(TSharedRef<SWidget> Left, TSharedRef<SHorizontalBox> Right)
		{
			Footer->SetContent(
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Left]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Right]);
		}

		static TSharedRef<SWidget> Button(const FText& Text, bool bPrimary, TFunction<void()> Click, TFunction<bool()> Enabled = nullptr, float Width = 0.f)
		{
			TSharedRef<SWRButton> B = SNew(SWRButton).Primary(bPrimary).Ghost(!bPrimary).FontSize(13).MinHeight(44.f * U).Text(Text)
				.OnClicked_Lambda([Click]() { Click(); return FReply::Handled(); });
			if (Enabled) B->SetEnabled(TAttribute<bool>::CreateLambda([Enabled]() { return Enabled(); }));
			return SNew(SBox).MinDesiredWidth(Width > 0.f ? Width * U : 0.f).Padding(FMargin(10.f * U, 0.f, 0.f, 0.f))[B];
		}

		TSharedRef<SWidget> StatusLine()
		{
			return SNew(SHorizontalBox).Visibility_Lambda([this]() { return Status.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(16.f * U, 0.f, 8.f * U, 0.f)[SNew(SWRLineIcon).Icon(EWRIcon::Warning).Size(15.f * U).Color(WRUI::Warn)]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return Status; }), WRUI::Body(13, 700), WRUI::Warn, false, true)];
		}

		// ------------------------------------------------------------ the list

		void ShowList()
		{
			Mode = EMode::List;
			NameBox.Reset();
			FocusNext.Reset();
			bFocused = false;  // Esc closes again
			Refresh();
			RebuildList(FString());
			const auto HasPick = [this]() { return SelectedWorld() != nullptr; };
			SetFooter(
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).MinDesiredWidth(110.f * U)[SNew(SWRButton).Ghost(true).FontSize(13).MinHeight(44.f * U).Text(LOCTEXT("Back", "Back")).OnClicked_Lambda([this]() { Close(); return FReply::Handled(); })]]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[StatusLine()],
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[Button(LOCTEXT("Delete", "Delete"), false, [this]() { AskDelete(); }, HasPick)]
				+ SHorizontalBox::Slot().AutoWidth()[Button(LOCTEXT("Rename", "Rename"), false, [this]() { StartRename(); }, HasPick)]
				+ SHorizontalBox::Slot().AutoWidth()[Button(LOCTEXT("Hosting", "Hosting"), false, [this]() { ShowHosting(); }, HasPick)]
				+ SHorizontalBox::Slot().AutoWidth()[Button(LOCTEXT("Backups", "Backups"), false, [this]() { ShowBackups(); }, HasPick)]
				+ SHorizontalBox::Slot().AutoWidth()[Button(LOCTEXT("NewWorld", "New world"), false, [this]() { ShowNew(); })]
				+ SHorizontalBox::Slot().AutoWidth()[Button(LOCTEXT("Play", "Play"), true, [this]() { Play(SelectedId); }, HasPick, 150.f)]);
		}

		/** The list; EditingId's row shows a box to type its new name in. */
		void RebuildList(const FString& EditingId)
		{
			if (Worlds.Num() == 0)
			{
				Body->SetContent(
					SNew(SBorder).BorderImage(InsetBrush()).HAlign(HAlign_Center).VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[SNew(SWRLineIcon).Icon(EWRIcon::Flag).Size(44.f * U).Color(WRUI::Brass2).Accent(WRUI::Ink3)]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 14.f * U, 0.f, 0.f)[WRUI::Text(LOCTEXT("NoWorlds", "No worlds yet"), Spaced(WRUI::Title(18), 60), WRUI::Ink, true)]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 8.f * U, 0.f, 0.f)[Muted(LOCTEXT("NoWorldsSub", "Make one with New world: each world keeps its own buildings, tames, clans and characters."), 14)]
					]);
				return;
			}
			TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
			for (const FWRWorldInfo& W : Worlds)
			{
				const FString Id = W.Id;
				TSharedPtr<SWidget> Editor;
				if (Id == EditingId)
				{
					Editor = SAssignNew(NameBox, SEditableTextBox).Style(&FieldStyle()).Font(WRUI::Body(16, 700)).Text(FText::FromString(W.Name))
						.SelectAllTextWhenFocused(true).OnKeyDownHandler(this, &SWRWorldChooser::OnTextBoxKey)
						.OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type How) { if (How == ETextCommit::OnEnter) FinishRename(); });
					FocusNext = NameBox;
				}
				Rows->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f * U)
				[
					SNew(SWRWorldRow, [this, Id]() { return SelectedId == Id; }, [this, Id]() { if (Mode == EMode::List) SelectedId = Id; }, [this, Id]() { if (Mode == EMode::List) Play(Id); })
					.Info(W).NameEditor(Editor)
				];
			}
			Body->SetContent(
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(2.f * U, 0.f, 0.f, 10.f * U)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Label(LOCTEXT("WorldsHere", "Worlds on this PC"))]
					+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Right).VAlign(VAlign_Center)[Muted(LOCTEXT("WorldsHint", "Friends join the world you host by your IP · UDP 7777"), 12)]
				]
				+ SVerticalBox::Slot().FillHeight(1.f)
				[
					SNew(SBorder).BorderImage(InsetBrush()).Padding(FMargin(10.f * U, 10.f * U, 6.f * U, 2.f * U))
					[
						SNew(SScrollBox).ScrollBarThickness(FVector2D(5.f, 5.f))
						+ SScrollBox::Slot().Padding(FMargin(0.f, 0.f, 6.f * U, 0.f))[Rows]
					]
				]);
		}

		// ------------------------------------------------------------ hosting: players, password, admins, bans

		/** One row of the Hosting page: what it is and what it means on the left, its controls on the right. */
		static TSharedRef<SWidget> HostRow(const FText& Title, const TAttribute<FText>& Sub, TSharedRef<SWidget> Controls)
		{
			return SNew(SBorder).BorderImage(WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.22f), 4.f, FLinearColor(0.f, 0.f, 0.f, 0.6f), 1.f)).Padding(FMargin(18.f * U, 14.f * U))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(0.f, 0.f, 16.f * U, 0.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(Title, Spaced(WRUI::Title(15), 40), WRUI::Ink)]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f * U, 0.f, 0.f)[WRUI::Text(Sub, WRUI::Body(13, 500), WRUI::Ink2, false, true)]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Controls]
				];
		}

		void ShowHosting()
		{
			if (!SelectedWorld()) return;
			Mode = EMode::Hosting;
			Status = FText::GetEmpty();
			HostNote = FText::GetEmpty();
			HostAccess = MakeShared<FWRAccess>(WRAccess::Read(SelectedId));
			HostAccess->MaxPlayers = FMath::Clamp(HostAccess->MaxPlayers, WRAccess::MinPlayers(), WRAccess::MaxPlayersLimit(false));
			RebuildHosting();
			SetFooter(
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).MinDesiredWidth(110.f * U)[SNew(SWRButton).Ghost(true).FontSize(13).MinHeight(44.f * U).Text(LOCTEXT("HostBack", "Back")).OnClicked_Lambda([this]() { ShowList(); return FReply::Handled(); })]]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(16.f * U, 0.f, 0.f, 0.f)[Muted(TAttribute<FText>::CreateLambda([this]() { return HostNote.IsEmpty() ? LOCTEXT("HostUnsaved", "Back leaves it as it was; Save keeps your changes.") : HostNote; }), 13, true)],
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[Button(LOCTEXT("HostSave", "Save"), true, [this]() { SaveHosting(); }, nullptr, 150.f)]);
		}

		void SaveHosting()
		{
			if (!HostAccess.IsValid()) return;
			// A password typed but not set yet goes in too: Save means what the page shows.
			if (PassBox.IsValid() && !PassBox->GetText().IsEmpty()) WRAccess::SetPassword(*HostAccess, PassBox->GetText().ToString());
			if (!WRAccess::Write(SelectedId, *HostAccess))
			{
				HostNote = LOCTEXT("HostSaveFailed", "Couldn't save: is the world's folder read-only?");
				return;
			}
			UE_LOG(LogWildreach, Log, TEXT("WORLDS hosting '%s': %d players, %s, %d admins, %d bans"), *SelectedId, HostAccess->MaxPlayers,
				HostAccess->HasPassword() ? TEXT("password") : TEXT("open"), HostAccess->Admins.Num(), HostAccess->Bans.Num());
			ShowList();
			Status = LOCTEXT("HostSaved", "Hosting saved: it applies the next time you host this world.");
		}

		void RebuildHosting()
		{
			const FWRWorldInfo* W = SelectedWorld();
			if (!W || !HostAccess.IsValid()) return;
			const TSharedPtr<FWRAccess> A = HostAccess;
			TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
			// Players: the most at once, you included.
			Rows->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f * U)
			[
				HostRow(LOCTEXT("HostPlayers", "Players"), LOCTEXT("HostPlayersSub", "The most who can be in this world at once, you included. Tuned for 4 to 8."),
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(48.f * U)[SNew(SWRButton).Ghost(true).FontSize(16).MinHeight(40.f * U).Text(FText::FromString(TEXT("\u2212")))
						.OnClicked_Lambda([A]() { A->MaxPlayers = FMath::Max(WRAccess::MinPlayers(), A->MaxPlayers - 1); return FReply::Handled(); })]]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(64.f * U).HAlign(HAlign_Center)
						[WRUI::Text(TAttribute<FText>::CreateLambda([A]() { return FText::AsNumber(A->MaxPlayers); }), WRUI::Title(20), WRUI::Ink)]]
					+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(48.f * U)[SNew(SWRButton).Ghost(true).FontSize(16).MinHeight(40.f * U).Text(FText::FromString(TEXT("+")))
						.OnClicked_Lambda([A]() { A->MaxPlayers = FMath::Min(WRAccess::MaxPlayersLimit(false), A->MaxPlayers + 1); return FReply::Handled(); })]])
			];
			// Password: set, change or remove (only its salted hash is kept).
			Rows->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f * U)
			[
				HostRow(LOCTEXT("HostPassword", "Password"), TAttribute<FText>::CreateLambda([A]()
					{
						return A->HasPassword() ? LOCTEXT("HostPassOn", "Set: friends type it when they join. Only a scrambled copy is kept, so a new one replaces it.")
							: LOCTEXT("HostPassOff", "None: anyone with your address can join.");
					}),
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f * U, 0.f)
					[
						SNew(SBox).WidthOverride(220.f * U)
						[
							SAssignNew(PassBox, SEditableTextBox).Style(&FieldStyle()).Font(WRUI::Body(15, 700)).IsPassword(true)
							.HintText_Lambda([A]() { return A->HasPassword() ? LOCTEXT("HostPassNewHint", "A new password") : LOCTEXT("HostPassHint", "A password"); })
							.OnKeyDownHandler(this, &SWRWorldChooser::OnTextBoxKey)
						]
					]
					+ SHorizontalBox::Slot().AutoWidth()[Button(LOCTEXT("HostPassSet", "Set"), true, [this, A]()
						{
							if (!PassBox.IsValid() || PassBox->GetText().IsEmpty()) return;
							WRAccess::SetPassword(*A, PassBox->GetText().ToString());
							PassBox->SetText(FText::GetEmpty());
							HostNote = LOCTEXT("HostPassSetNote", "Password set. Save to keep it.");
						}, [this]() { return PassBox.IsValid() && !PassBox->GetText().IsEmpty(); })]
					+ SHorizontalBox::Slot().AutoWidth()[Button(LOCTEXT("HostPassClear", "Remove"), false, [this, A]()
						{
							WRAccess::SetPassword(*A, FString());
							HostNote = LOCTEXT("HostPassClearNote", "Password removed. Save to keep it that way.");
						}, [A]() { return A->HasPassword(); })])
			];
			// Admins: kick and ban too (made from the Clan tab while you play); removed here.
			TSharedRef<SVerticalBox> AdminList = SNew(SVerticalBox);
			for (const FString& Name : A->Admins)
			{
				AdminList->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f * U)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f * U, 0.f)[SNew(SBox).MinDesiredWidth(160.f * U)[WRUI::Text(FText::FromString(Name), WRUI::Body(15, 700), WRUI::Ink)]]
					+ SHorizontalBox::Slot().AutoWidth()[Button(LOCTEXT("HostAdminRemove", "Remove"), false, [this, A, Name]()
						{
							A->Admins.Remove(Name);
							HostNote = FText::Format(LOCTEXT("HostAdminRemoved", "{0} won't be an admin once you save."), FText::FromString(Name));
							RebuildHosting();
						})]
				];
			}
			Rows->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f * U)
			[
				HostRow(LOCTEXT("HostAdmins", "Admins"), A->Admins.Num() == 0 ? LOCTEXT("HostAdminsNone", "Only you. While you play, Make admin on a player's row in the Clan tab (O) lets them kick and ban too.")
					: LOCTEXT("HostAdminsSome", "They can kick and ban players (not you)."), AdminList)
			];
			// Bans: who's kept out, and for how long.
			TSharedRef<SVerticalBox> BanList = SNew(SVerticalBox);
			for (const FWRBan& Ban : A->Bans)
			{
				const FString Name = Ban.Name;
				BanList->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f * U)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f * U, 0.f)[SNew(SBox).MinDesiredWidth(140.f * U)[WRUI::Text(FText::FromString(Name), WRUI::Body(15, 700), WRUI::Ink)]]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f * U, 0.f)[SNew(SWRChip).FontSize(11).Color(WRUI::Bad).Text(FText::FromString(WRAccess::BanLeft(Ban)))]
					+ SHorizontalBox::Slot().AutoWidth()[Button(LOCTEXT("HostUnban", "Unban"), false, [this, A, Name]()
						{
							A->Bans.RemoveAll([&Name](const FWRBan& B) { return B.Name == Name; });
							HostNote = FText::Format(LOCTEXT("HostUnbanned", "{0} may come back once you save."), FText::FromString(Name));
							RebuildHosting();
						})]
				];
			}
			Rows->AddSlot().AutoHeight()
			[
				HostRow(LOCTEXT("HostBans", "Bans"), A->Bans.Num() == 0 ? LOCTEXT("HostBansNone", "Nobody is banned. Ban from a player's row in the Clan tab (O).")
					: LOCTEXT("HostBansSome", "Kept out by their name and their address."), BanList)
			];
			Body->SetContent(
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(2.f * U, 0.f, 0.f, 10.f * U)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Label(FText::Format(LOCTEXT("HostingWorld", "Hosting {0}"), FText::FromString(W->Name)))]
					+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Right).VAlign(VAlign_Center)[Muted(LOCTEXT("HostingHint", "Friends join by your IP · UDP 7777"), 12)]
				]
				+ SVerticalBox::Slot().FillHeight(1.f)
				[
					SNew(SBorder).BorderImage(InsetBrush()).Padding(FMargin(10.f * U, 10.f * U, 6.f * U, 2.f * U))
					[
						SNew(SScrollBox).ScrollBarThickness(FVector2D(5.f, 5.f))
						+ SScrollBox::Slot().Padding(FMargin(0.f, 0.f, 6.f * U, 0.f))[Rows]
					]
				]);
		}

		// ------------------------------------------------------------ backups: put the world back as it was

		static FText BackupKind(const FString& Kind)
		{
			if (Kind == TEXT("hourly")) return LOCTEXT("BackupHourly", "Hourly");
			if (Kind == TEXT("daily")) return LOCTEXT("BackupDaily", "Daily");
			if (Kind == TEXT("shutdown")) return LOCTEXT("BackupShutdown", "When it closed");
			if (Kind == TEXT("before-restore")) return LOCTEXT("BackupBeforeRestore", "Before a restore");
			return FText::FromString(Kind.Replace(TEXT("-"), TEXT(" ")));
		}

		void ShowBackups()
		{
			const FWRWorldInfo* W = SelectedWorld();
			if (!W) return;
			Mode = EMode::Backups;
			Status = FText::GetEmpty();
			const TArray<WRStore::FBackupInfo> All = WRStore::ListBackups(SelectedId);
			const int32 Shown = FMath::Min(All.Num(), FMath::Max(1, FMath::RoundToInt(WRTune(TEXT("store.backupsShown"), 12.f))));
			TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
			if (All.Num() == 0)
			{
				Rows->AddSlot().AutoHeight().Padding(8.f * U)[Muted(LOCTEXT("NoBackups", "No backups yet: the world keeps one every hour you play, one a day and one each time it closes."), 14, true)];
			}
			for (int32 i = 0; i < Shown; ++i)
			{
				const WRStore::FBackupInfo B = All[i];
				const FString Name = W->Name;
				Rows->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f * U)
				[
					SNew(SBorder).BorderImage(WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.22f), 4.f, FLinearColor(0.f, 0.f, 0.f, 0.6f), 1.f)).Padding(FMargin(18.f * U, 10.f * U))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(When(B.MadeUtc), Spaced(WRUI::Title(15), 40), WRUI::Ink)]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f * U, 0.f, 0.f)
							[
								WRUI::Text(FText::Format(LOCTEXT("BackupLine", "{0} · {1} KB"), BackupKind(B.Kind), FText::AsNumber(FMath::Max<int64>(1, B.Bytes / 1024))), WRUI::Body(13, 500), WRUI::Ink2)
							]
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Button(LOCTEXT("BackupRestore", "Restore"), false, [this, B, Name]() { AskRestore(B, Name); })]
					]
				];
			}
			Body->SetContent(
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(2.f * U, 0.f, 0.f, 10.f * U)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Label(FText::Format(LOCTEXT("BackupsOf", "Backups of {0}"), FText::FromString(W->Name)))]
					+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Right).VAlign(VAlign_Center)[Muted(LOCTEXT("BackupsHint", "Newest first · a restore keeps the world as it is now too"), 12)]
				]
				+ SVerticalBox::Slot().FillHeight(1.f)
				[
					SNew(SBorder).BorderImage(InsetBrush()).Padding(FMargin(10.f * U, 10.f * U, 6.f * U, 2.f * U))
					[
						SNew(SScrollBox).ScrollBarThickness(FVector2D(5.f, 5.f))
						+ SScrollBox::Slot().Padding(FMargin(0.f, 0.f, 6.f * U, 0.f))[Rows]
					]
				]);
			SetFooter(
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).MinDesiredWidth(110.f * U)[SNew(SWRButton).Ghost(true).FontSize(13).MinHeight(44.f * U).Text(LOCTEXT("BackupsBack", "Back")).OnClicked_Lambda([this]() { ShowList(); return FReply::Handled(); })]]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[StatusLine()],
				SNew(SHorizontalBox));
		}

		void AskRestore(const WRStore::FBackupInfo& B, const FString& WorldName)
		{
			SetFooter(
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f * U, 0.f)[SNew(SWRLineIcon).Icon(EWRIcon::Warning).Size(20.f * U).Color(WRUI::Warn)]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					WRUI::Text(FText::Format(LOCTEXT("RestoreAsk", "Put \"{0}\" back as it was {1}? The world as it is now is kept as a backup first."), FText::FromString(WorldName), When(B.MadeUtc)), WRUI::Body(14, 700), WRUI::Ink, false, true)
				],
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[Button(LOCTEXT("RestoreNo", "Keep it as it is"), false, [this]() { ShowBackups(); })]
				+ SHorizontalBox::Slot().AutoWidth()[Button(LOCTEXT("RestoreYes", "Restore"), true, [this, B]()
				{
					FString Why;
					if (WRStore::RestoreBackup(SelectedId, B.File, Why))
					{
						ShowList();
						Status = FText::Format(LOCTEXT("Restored", "Restored the backup from {0}."), When(B.MadeUtc));
					}
					else
					{
						ShowBackups();
						Status = FText::FromString(Why);
					}
				}, nullptr, 150.f)]);
		}

		// ------------------------------------------------------------ rename and delete

		void StartRename()
		{
			if (!SelectedWorld()) return;
			Mode = EMode::Rename;
			Status = FText::GetEmpty();
			RebuildList(SelectedId);
			SetFooter(Muted(LOCTEXT("RenameHint", "Type the world's new name, then Enter"), 13),
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[Button(LOCTEXT("Cancel", "Cancel"), false, [this]() { ShowList(); })]
				+ SHorizontalBox::Slot().AutoWidth()[Button(LOCTEXT("SaveName", "Save name"), true, [this]() { FinishRename(); }, nullptr, 150.f)]);
		}

		void FinishRename()
		{
			if (Mode != EMode::Rename || !NameBox.IsValid()) return;
			const FString Name = NameBox->GetText().ToString().TrimStartAndEnd();
			if (!Name.IsEmpty() && !WRWorlds::Rename(SelectedId, Name))
			{
				Status = LOCTEXT("RenameFailed", "Couldn't rename the world (is its folder read-only?)");
			}
			ShowList();
		}

		void AskDelete()
		{
			const FWRWorldInfo* W = SelectedWorld();
			if (!W) return;
			Mode = EMode::ConfirmDelete;
			SetFooter(
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f * U, 0.f)[SNew(SWRLineIcon).Icon(EWRIcon::Warning).Size(20.f * U).Color(WRUI::Bad)]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					WRUI::Text(FText::Format(LOCTEXT("DeleteAsk", "Delete \"{0}\"? Everything built there and every character in it is gone for good."), FText::FromString(W->Name)), WRUI::Body(14, 700), WRUI::Ink, false, true)
				],
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[Button(LOCTEXT("KeepIt", "Keep it"), false, [this]() { ShowList(); })]
				+ SHorizontalBox::Slot().AutoWidth()[Button(LOCTEXT("YesDelete", "Yes, delete it"), true, [this]()
				{
					if (!WRWorlds::Delete(SelectedId)) Status = LOCTEXT("DeleteFailed", "Couldn't delete the world: a file in it may be open");
					else Status = FText::GetEmpty();
					ShowList();
				}, nullptr, 170.f)]);
		}

		// ------------------------------------------------------------ a new world

		FString DefaultName() const
		{
			for (int32 N = Worlds.Num() + 1; ; ++N)
			{
				const FString Name = FString::Printf(TEXT("World %d"), N);
				if (!Worlds.ContainsByPredicate([&Name](const FWRWorldInfo& W) { return W.Name.Equals(Name, ESearchCase::IgnoreCase); })) return Name;
			}
		}

		void ShowNew()
		{
			Mode = EMode::New;
			Status = FText::GetEmpty();
			NewValues = MakeShared<FWRWorldSettings>(WRWorlds::PresetSettings(TEXT("pve")));
			bNewChanged = false;
			SAssignNew(NameBox, SEditableTextBox).Style(&FieldStyle()).Font(WRUI::Body(17, 700)).Text(FText::FromString(DefaultName()))
				.SelectAllTextWhenFocused(true).HintText(LOCTEXT("WorldNameHint", "The world's name")).OnKeyDownHandler(this, &SWRWorldChooser::OnTextBoxKey)
				.OnTextCommitted_Lambda([this](const FText&, ETextCommit::Type How) { if (How == ETextCommit::OnEnter) CreateAndPlay(); });
			FocusNext = NameBox;
			TSharedRef<SVerticalBox> Tiles = SNew(SVerticalBox);
			for (const FString& P : WRWorlds::Presets())
			{
				Tiles->AddSlot().AutoHeight().Padding(0.f, Tiles->NumSlots() > 0 ? 8.f * U : 0.f, 0.f, 0.f)
				[
					SNew(SWRPresetTile, [this, P]() { return NewValues->Base == P; }, [this, P]() { *NewValues = WRWorlds::PresetSettings(P); bNewChanged = false; }).Preset(P)
				];
			}
			Body->SetContent(
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).WidthOverride(400.f * U)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[Label(LOCTEXT("WorldName", "Name"))]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f * U, 0.f, 0.f)[SNew(SBox).HeightOverride(46.f * U)[NameBox.ToSharedRef()]]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 18.f * U, 0.f, 0.f)[Label(LOCTEXT("WorldKind", "Kind of world"))]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f * U, 0.f, 0.f)[Tiles]
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(22.f * U, 0.f, 0.f, 0.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Label(LOCTEXT("WorldRules", "Its rules"))]
						+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Right).VAlign(VAlign_Center)
						[
							Muted(TAttribute<FText>::CreateLambda([this]() { return bNewChanged ? LOCTEXT("WorldChangedNew", "Changed from the preset") : LOCTEXT("WorldRulesLater", "The host can change these later"); }), 12)
						]
					]
					+ SVerticalBox::Slot().FillHeight(1.f).Padding(0.f, 8.f * U, 0.f, 0.f)
					[
						SNew(SBorder).BorderImage(InsetBrush()).Padding(FMargin(12.f * U, 12.f * U, 6.f * U, 6.f * U))
						[
							SNew(SScrollBox).ScrollBarThickness(FVector2D(5.f, 5.f))
							+ SScrollBox::Slot().Padding(FMargin(0.f, 0.f, 8.f * U, 0.f))
							[
								WRUI::MakeWorldRules(NewValues.ToSharedRef(), [this]() { bNewChanged = WRWorlds::IsCustom(*NewValues); })
							]
						]
					]
				]);
			SetFooter(
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).MinDesiredWidth(110.f * U)[SNew(SWRButton).Ghost(true).FontSize(13).MinHeight(44.f * U).Text(LOCTEXT("Back", "Back")).OnClicked_Lambda([this]() { ShowList(); return FReply::Handled(); })]]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(16.f * U, 0.f, 0.f, 0.f)[Muted(LOCTEXT("NewWorldInfo", "Everyone starts fresh in a new world"), 13)]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[StatusLine()],
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[Button(LOCTEXT("CreatePlay", "Create and play"), true, [this]() { CreateAndPlay(); }, nullptr, 190.f)]);
		}

		void CreateAndPlay()
		{
			if (Mode != EMode::New || !NameBox.IsValid()) return;
			FString Name = NameBox->GetText().ToString().TrimStartAndEnd();
			if (Name.IsEmpty()) Name = DefaultName();
			const FString Id = WRWorlds::Create(Name, *NewValues);
			if (Id.IsEmpty())
			{
				Status = LOCTEXT("CreateFailed", "Couldn't make the world's folder in Saved/Worlds");
				return;
			}
			Play(Id);
		}
	};
}

TSharedRef<SWidget> WRUI::MakeWorldChooser(const FString& Selected, TFunction<void(const FString&)> OnPlay, FSimpleDelegate OnClose)
{
	return SNew(WRWorldsUI::SWRWorldChooser, MoveTemp(OnPlay)).Selected(Selected).OnClose(OnClose);
}

#undef LOCTEXT_NAMESPACE
