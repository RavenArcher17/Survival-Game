// The controls page (F1 anywhere, Y in any menu on a pad): every action with its key or button, opening on the page of
// the device in use (keyboard and mouse, or controller) and switching with it; LB / RB or the tabs show the other.
// The keyboard page reads FWRKeyBindings (rebound keys show as rebound); the controller page is Data/gamepad.json's
// "page" (button names in the set chosen in Settings: Xbox or PlayStation).

#include "WRPadInput.h"
#include "Wildreach.h"
#include "WRKeyBindings.h"
#include "WRSound.h"
#include "WRUIStyle.h"
#include "WRUIWidgets.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBackgroundBlur.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"

#define LOCTEXT_NAMESPACE "WRControls"

using WRUI::U;

namespace WRControlsUI
{
	/** A tab at the top ("Keyboard and mouse" / "Controller"). */
	class SWRControlsTab : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SWRControlsTab) {}
			SLATE_ARGUMENT(FText, Text)
		SLATE_END_ARGS()
		void Construct(const FArguments& InArgs, TFunction<bool()> InOn, TFunction<void()> InPick)
		{
			On = MoveTemp(InOn);
			Pick = MoveTemp(InPick);
			ChildSlot
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(14.f * U, 6.f * U)
				[
					WRUI::Text(InArgs._Text, WRUI::Title(15), TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(On() ? WRUI::Ink : IsHovered() ? WRUI::Ink2 : WRUI::Ink3); }), true)
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SBox).HeightOverride(2.f)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity_Lambda([this]() { return On() ? WRUI::Brass : FLinearColor::Transparent; })]
				]
			];
		}
		virtual FReply OnMouseButtonDown(const FGeometry&, const FPointerEvent& E) override
		{
			if (E.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
			WRSound::PlayUI(nullptr, TEXT("ui.click"));
			Pick();
			return FReply::Handled();
		}
		virtual FCursorReply OnCursorQuery(const FGeometry&, const FPointerEvent&) const override { return FCursorReply::Cursor(EMouseCursor::Hand); }
	private:
		TFunction<bool()> On;
		TFunction<void()> Pick;
	};

	class SWRControlsPanel : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SWRControlsPanel) {}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			bPad = bLastDevice = WRInput::UsingPad();
			ChildSlot
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[SNew(SBackgroundBlur).BlurStrength(6.f)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.5f))]]
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(1180.f * U).MaxDesiredHeight(780.f * U)
					[
						SNew(SWRPanel).Kind(EWRPanel::Stone).Rivets(true).BrassTrim(true).Padding(FMargin(26.f * U, 18.f * U))
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 24.f * U, 0.f)[WRUI::Text(LOCTEXT("Title", "Controls"), WRUI::Title(22), WRUI::Ink, true)]
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)[SNew(SWRControlsTab, [this]() { return !bPad; }, [this]() { bPad = false; }).Text(LOCTEXT("KBM", "Keyboard and mouse"))]
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)[SNew(SWRControlsTab, [this]() { return bPad; }, [this]() { bPad = true; }).Text(LOCTEXT("Pad", "Controller"))]
								+ SHorizontalBox::Slot().FillWidth(1.f)
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
								[
									WRUI::Text(TAttribute<FText>::CreateLambda([this]()
									{
										return bPad ? LOCTEXT("PadNote", "Button names: Settings > Look and controller") : LOCTEXT("KeyNote", "Change any key in Settings > Controls");
									}), WRUI::Body(12, 500), WRUI::Ink3)
								]
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f * U, 0.f, 12.f * U)[WRUI::Divider()]
							+ SVerticalBox::Slot().FillHeight(1.f)
							[
								SNew(SWidgetSwitcher).WidgetIndex_Lambda([this]() { return bPad ? 1 : 0; })
								+ SWidgetSwitcher::Slot()[SNew(SScrollBox) + SScrollBox::Slot()[KeyboardPage()]]
								+ SWidgetSwitcher::Slot()[SNew(SScrollBox) + SScrollBox::Slot()[PadPage()]]
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f * U, 0.f, 0.f)[Footer()]
						]
					]
				]
			];
			WRInput::PushPadHandler(this, [this](FName Verb)
			{
				if (Verb == TEXT("MenuBack") || Verb == TEXT("MenuClose") || Verb == TEXT("MenuControls")) { WRControlsPage::Close(); return true; }
				if (Verb == TEXT("TabPrev") || Verb == TEXT("TabNext")) { bPad = !bPad; WRSound::PlayUI(nullptr, TEXT("ui.click")); return true; }
				return false;
			});
		}

		virtual ~SWRControlsPanel() override { WRInput::PopPadHandler(this); }

		virtual bool SupportsKeyboardFocus() const override { return true; }

		virtual FReply OnKeyDown(const FGeometry&, const FKeyEvent& E) override
		{
			if (E.GetKey() == EKeys::Escape) { WRControlsPage::Close(); return FReply::Handled(); }
			return FReply::Unhandled();
		}

		virtual void Tick(const FGeometry& Geo, const double Time, const float Delta) override
		{
			SCompoundWidget::Tick(Geo, Time, Delta);
			// Follow the device: pick up the pad and the page turns to it (and back).
			const bool bNow = WRInput::UsingPad();
			if (bNow != bLastDevice) { bLastDevice = bNow; bPad = bNow; }
		}

	private:
		bool bPad = false;
		bool bLastDevice = false;

		static TSharedRef<SWidget> Row(const TAttribute<FText>& Key, const FText& Does)
		{
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(120.f * U).HAlign(HAlign_Left)[SNew(SWRKeyCap).FontSize(12).Text(Key)]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[WRUI::Text(Does, WRUI::Body(14, 500), WRUI::Ink, false, true)];
		}

		static TSharedRef<SVerticalBox> Group(const FText& Title)
		{
			return SNew(SVerticalBox) + SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f * U)[WRUI::Text(Title, WRUI::Caps(14), WRUI::Brass)];
		}

		/** Groups laid into two columns, each group kept whole, the shorter column taking the next. */
		static TSharedRef<SWidget> Columns(const TArray<TPair<TSharedRef<SVerticalBox>, int32>>& Groups)
		{
			TSharedRef<SVerticalBox> Cols[2] = { SNew(SVerticalBox), SNew(SVerticalBox) };
			int32 Lines[2] = { 0, 0 };
			for (const TPair<TSharedRef<SVerticalBox>, int32>& G : Groups)
			{
				const int32 c = Lines[0] <= Lines[1] ? 0 : 1;
				Cols[c]->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 16.f * U)[G.Key];
				Lines[c] += G.Value + 2;
			}
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 28.f * U, 0.f)[Cols[0]]
				+ SHorizontalBox::Slot().FillWidth(1.f)[Cols[1]];
		}

		static TSharedRef<SWidget> KeyboardPage()
		{
			struct FG { FName Id; FText Title; };
			const FG Order[] = {
				{ TEXT("Movement"), LOCTEXT("GMove", "Moving") }, { TEXT("Combat"), LOCTEXT("GCombat", "Fighting") },
				{ TEXT("Items"), LOCTEXT("GItems", "Items") }, { TEXT("Menus"), LOCTEXT("GMenus", "Menus and building") },
			};
			TArray<TPair<TSharedRef<SVerticalBox>, int32>> Groups;
			for (const FG& G : Order)
			{
				TSharedRef<SVerticalBox> Box = Group(G.Title);
				int32 N = 0;
				if (G.Id == TEXT("Movement"))
				{
					Box->AddSlot().AutoHeight().Padding(0.f, 3.f * U)[Row(LOCTEXT("Mouse", "Mouse"), LOCTEXT("MouseLook", "Look around"))];
					++N;
				}
				for (const FWRActionKey& A : FWRKeyBindings::Get().Actions())
				{
					if (A.Group != G.Id) continue;
					const FString Id = A.Id.ToString();
					if (Id.StartsWith(TEXT("Hotbar")) && Id != TEXT("Hotbar1") && Id != TEXT("HotbarNext") && Id != TEXT("HotbarPrev")) continue;
					const FName ActionId = A.Id;
					const TAttribute<FText> Key = Id == TEXT("Hotbar1")
						? TAttribute<FText>::CreateLambda([]() { const FWRKeyBindings& K = FWRKeyBindings::Get(); return FText::Format(LOCTEXT("Range", "{0} - {1}"), WRUI::KeyName(K.Key(TEXT("Hotbar1"))), WRUI::KeyName(K.Key(TEXT("Hotbar8")))); })
						: TAttribute<FText>::CreateLambda([ActionId]() { return WRUI::KeyName(FWRKeyBindings::Get().Key(ActionId)); });
					Box->AddSlot().AutoHeight().Padding(0.f, 3.f * U)[Row(Key, Id == TEXT("Hotbar1") ? LOCTEXT("HotbarAll", "Hotbar slots") : A.Label)];
					++N;
				}
				if (G.Id == TEXT("Menus"))
				{
					Box->AddSlot().AutoHeight().Padding(0.f, 3.f * U)[Row(FText::FromString(TEXT("Esc")), LOCTEXT("EscDoes", "Close what's open; the game menu"))];
					++N;
				}
				Groups.Add({ Box, N });
			}
			return Columns(Groups);
		}

		static TSharedRef<SWidget> PadPage()
		{
			TArray<TPair<TSharedRef<SVerticalBox>, int32>> Groups;
			for (const FWRPadLayout::FGroup& G : FWRPadLayout::Get().Groups)
			{
				TSharedRef<SVerticalBox> Box = Group(G.Title);
				for (const FWRPadLayout::FRow& R : G.Rows)
				{
					const FName Verb = R.Verb;
					const FString Fixed = R.Button;
					const TAttribute<FText> Key = Verb.IsNone() ? TAttribute<FText>(FText::FromString(Fixed))
						: TAttribute<FText>::CreateLambda([Verb]() { return WRInput::PadButton(Verb); });
					Box->AddSlot().AutoHeight().Padding(0.f, 3.f * U)[Row(Key, R.Does)];
				}
				Groups.Add({ Box, G.Rows.Num() });
			}
			if (Groups.Num() == 0)
			{
				return WRUI::Text(LOCTEXT("NoPad", "The controller layout (Data/gamepad.json) is missing."), WRUI::Body(14, 500), WRUI::Bad);
			}
			return Columns(Groups);
		}

		TSharedRef<SWidget> Footer()
		{
			const auto Hint = [](TAttribute<FText> Key, const FText& What)
			{
				return SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SWRKeyCap).FontSize(11).Text(Key)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.f * U, 0.f, 18.f * U, 0.f)[WRUI::Text(What, WRUI::Body(12, 600), WRUI::Ink3)];
			};
			return SNew(SWidgetSwitcher).WidgetIndex_Lambda([]() { return WRInput::UsingPad() ? 1 : 0; })
				+ SWidgetSwitcher::Slot()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()[Hint(TAttribute<FText>::CreateLambda([]() { return WRUI::KeyName(FWRKeyBindings::Get().Key(TEXT("Help"))); }), LOCTEXT("FClose", "Close"))]
					+ SHorizontalBox::Slot().AutoWidth()[Hint(FText::FromString(TEXT("Esc")), LOCTEXT("FClose2", "Close"))]
				]
				+ SWidgetSwitcher::Slot()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()[Hint(TAttribute<FText>::CreateLambda([]() { return FText::Format(LOCTEXT("TabPair", "{0} {1}"), WRInput::PadButton(TEXT("TabPrev")), WRInput::PadButton(TEXT("TabNext"))); }), LOCTEXT("FSwitch", "Keyboard / controller"))]
					+ SHorizontalBox::Slot().AutoWidth()[Hint(TAttribute<FText>::CreateLambda([]() { return WRInput::PadButton(TEXT("MenuBack")); }), LOCTEXT("FBack", "Close"))]
				];
		}
	};

	TSharedPtr<SWidget> Open;
}

bool WRControlsPage::IsOpen() { return WRControlsUI::Open.IsValid(); }

void WRControlsPage::Toggle()
{
	if (IsOpen())
	{
		Close();
		return;
	}
	if (!GEngine || !GEngine->GameViewport) return;
	TSharedRef<WRControlsUI::SWRControlsPanel> Panel = SNew(WRControlsUI::SWRControlsPanel);
	WRControlsUI::Open = Panel;
	GEngine->GameViewport->AddViewportWidgetContent(Panel, 900);
	WRInput::AddNavRoot(Panel, 900);
	WRInput::BeginOverlay(Panel);
	WRSound::PlayUI(nullptr, TEXT("ui.open"));
	UE_LOG(LogWildreach, Log, TEXT("UI controls page (%s)"), WRInput::UsingPad() ? TEXT("controller") : TEXT("keyboard and mouse"));
}

void WRControlsPage::Close()
{
	if (!WRControlsUI::Open.IsValid()) return;
	const TSharedPtr<SWidget> Panel = WRControlsUI::Open;
	WRControlsUI::Open.Reset();
	WRInput::RemoveNavRoot(Panel);
	if (GEngine && GEngine->GameViewport) GEngine->GameViewport->RemoveViewportWidgetContent(Panel.ToSharedRef());
	WRInput::EndOverlay();
	WRSound::PlayUI(nullptr, TEXT("ui.close"));
}

#undef LOCTEXT_NAMESPACE
