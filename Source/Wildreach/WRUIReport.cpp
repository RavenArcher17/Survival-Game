// The Report a problem panel (F8 / View on a pad): the frozen frame, the 8 kinds, a note, Send / Cancel. The rest of the
// report (context, sending, saving, the host) is WRReport.cpp.

#include "WRReport.h"
#include "WRPadInput.h"
#include "WRSound.h"
#include "WRUIStyle.h"
#include "WRUIWidgets.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBackgroundBlur.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"

#define LOCTEXT_NAMESPACE "WRReport"

using WRUI::U;

namespace WRReportUI
{
	/** One kind (Bug, Stuck...): a chip that lights when picked. */
	class SWRReportKind : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SWRReportKind) {}
			SLATE_ARGUMENT(FText, Text)
		SLATE_END_ARGS()
		void Construct(const FArguments& InArgs, TFunction<bool()> InOn, TFunction<void()> InPick)
		{
			On = MoveTemp(InOn);
			Pick = MoveTemp(InPick);
			ChildSlot
			[
				SNew(SBox).HeightOverride(36.f * U)
				[
					SNew(SBorder).Padding(FMargin(10.f * U, 0.f)).HAlign(HAlign_Center).VAlign(VAlign_Center)
					.BorderImage_Lambda([this]() { return On() ? WRUI::Box(FColor(0x4D, 0x39, 0x20), 4.f, WRUI::Brass, 1.f) : WRUI::Box(IsHovered() ? FLinearColor(1.f, 1.f, 1.f, 0.07f) : FLinearColor(0.f, 0.f, 0.f, 0.35f), 4.f, WRUI::Brass2, 1.f); })
					[
						WRUI::Text(InArgs._Text, WRUI::Body(14, 700), TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(On() ? WRUI::Ink : IsHovered() ? WRUI::Ink2 : WRUI::Ink3); }))
					]
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

	class SWRReportPanel : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SWRReportPanel) {}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			Draft = WRReport::GetDraft();
			static const FEditableTextBoxStyle Field = []()
			{
				FEditableTextBoxStyle S = FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");
				const FSlateBrush* Normal = WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.55f), 5.f, WRUI::Brass2, 1.f);
				const FSlateBrush* Focused = WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.6f), 5.f, WRUI::Brass, 1.f);
				S.SetBackgroundImageNormal(*Normal).SetBackgroundImageHovered(*Normal).SetBackgroundImageFocused(*Focused).SetBackgroundImageReadOnly(*Normal);
				S.SetForegroundColor(FSlateColor(WRUI::Ink)).SetFocusedForegroundColor(FSlateColor(WRUI::Ink)).SetBackgroundColor(FSlateColor(FLinearColor::White));
				S.SetPadding(FMargin(12.f * U, 10.f * U));
				return S;
			}();

			const TArray<FString>& Kinds = WRReport::Kinds();
			TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel).SlotPadding(FMargin(3.f * U));
			for (int32 i = 0; i < Kinds.Num(); ++i)
			{
				Grid->AddSlot(i % 4, i / 4)
				[
					SNew(SWRReportKind, [this, i]() { return Kind == i; }, [this, i]() { Kind = i; }).Text(FText::FromString(Kinds[i]))
				];
			}

			const FSlateBrush* Shot = Draft.IsValid() ? Draft->Brush.Get() : nullptr;
			TSharedRef<SWidget> Picture = Shot
				? StaticCastSharedRef<SWidget>(SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SImage).Image(Shot)])
				: StaticCastSharedRef<SWidget>(SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)[WRUI::Text(LOCTEXT("NoShot", "No picture this time"), WRUI::Body(14, 500), WRUI::Ink3)]);

			ChildSlot
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[SNew(SBackgroundBlur).BlurStrength(6.f)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.55f))]]
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(1120.f * U)
					[
						SNew(SWRPanel).Kind(EWRPanel::Stone).Rivets(true).BrassTrim(true).Padding(FMargin(26.f * U, 20.f * U))
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[WRUI::Text(LOCTEXT("Title", "Report a problem"), WRUI::Title(22), WRUI::Ink, true)]
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(FText::FromString(TEXT("Wildreach ") + (Draft.IsValid() && Draft->Context.IsValid() ? Draft->Context->GetStringField(TEXT("build")) : FString())), WRUI::Body(12, 500), WRUI::Ink3)]
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f * U, 0.f, 14.f * U)
							[
								WRUI::Text(LOCTEXT("Sub", "What happened? It goes to the developers with this picture, where you are and the last minute of play."), WRUI::Body(14, 500), WRUI::Ink2, false, true)
							]
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(SHorizontalBox)
								// The frozen frame.
								+ SHorizontalBox::Slot().AutoWidth()
								[
									SNew(SBox).WidthOverride(520.f * U).HeightOverride(292.f * U)
									[
										SNew(SBorder).BorderImage(WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.6f), 4.f, WRUI::Brass2, 1.f)).Padding(3.f)[Picture]
									]
								]
								// The kind and the note.
								+ SHorizontalBox::Slot().FillWidth(1.f).Padding(20.f * U, 0.f, 0.f, 0.f)
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f * U)[WRUI::Text(LOCTEXT("Kind", "What kind of problem"), WRUI::Caps(13), WRUI::Ink2)]
									+ SVerticalBox::Slot().AutoHeight()[Grid]
									+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f * U, 0.f, 6.f * U)[WRUI::Text(LOCTEXT("Note", "Your note"), WRUI::Caps(13), WRUI::Ink2)]
									+ SVerticalBox::Slot().FillHeight(1.f)
									[
										SNew(SBox).MinDesiredHeight(130.f * U)
										[
											SAssignNew(Box, SMultiLineEditableTextBox).Style(&Field).Font(WRUI::Body(15, 500)).AutoWrapText(true)
											.HintText(LOCTEXT("Hint", "What happened, and what did you expect?"))
										]
									]
								]
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 16.f * U, 0.f, 0.f)
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Hints()]
								+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 10.f * U, 0.f)
								[
									SNew(SBox).WidthOverride(150.f * U)[SNew(SWRButton).Ghost(true).Text(LOCTEXT("Cancel", "Cancel")).OnClicked_Lambda([]() { WRReport::Cancel(); return FReply::Handled(); })]
								]
								+ SHorizontalBox::Slot().AutoWidth()
								[
									SNew(SBox).WidthOverride(190.f * U)[SNew(SWRButton).Primary(true).Text(LOCTEXT("Send", "Send report")).OnClicked_Lambda([this]() { SendNow(); return FReply::Handled(); })]
								]
							]
						]
					]
				]
			];

			// The pad: LB / RB or the D-pad's left / right pick the kind, Y sends, B (or Menu) cancels; A clicks.
			WRInput::PushPadHandler(this, [this](FName Verb)
			{
				if (Verb == TEXT("MenuBack") || Verb == TEXT("MenuClose")) { WRReport::Cancel(); return true; }
				if (Verb == TEXT("MenuControls")) { SendNow(); return true; }
				if (Verb == TEXT("TabPrev") || Verb == TEXT("DLeft")) { Step(-1); return true; }
				if (Verb == TEXT("TabNext") || Verb == TEXT("DRight")) { Step(1); return true; }
				return false;
			});
		}

		virtual ~SWRReportPanel() override { WRInput::PopPadHandler(this); }

		TSharedPtr<SWidget> GetFocusTarget() const { return Box; }

		virtual bool SupportsKeyboardFocus() const override { return true; }

		virtual FReply OnPreviewKeyDown(const FGeometry&, const FKeyEvent& E) override
		{
			// Esc cancels, Ctrl+Enter sends, even while typing (the box would take them otherwise).
			if (E.GetKey() == EKeys::Escape) { WRReport::Cancel(); return FReply::Handled(); }
			if (E.GetKey() == EKeys::Enter && E.IsControlDown()) { SendNow(); return FReply::Handled(); }
			return FReply::Unhandled();
		}

	private:
		TSharedPtr<WRReport::FDraft> Draft;
		TSharedPtr<SMultiLineEditableTextBox> Box;
		int32 Kind = 0;

		void Step(int32 Dir)
		{
			const int32 N = WRReport::Kinds().Num();
			Kind = ((Kind + Dir) % N + N) % N;
			WRSound::PlayUI(nullptr, TEXT("ui.click"));
		}

		void SendNow()
		{
			const FString Note = Box.IsValid() ? Box->GetText().ToString() : FString();
			WRReport::Send(Kind, Note);
		}

		TSharedRef<SWidget> Hints()
		{
			// The keys for the device in use only.
			const auto Pair = [](TAttribute<FText> Key, const FText& What)
			{
				return SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SWRKeyCap).FontSize(11).Text(Key)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.f * U, 0.f, 16.f * U, 0.f)[WRUI::Text(What, WRUI::Body(12, 600), WRUI::Ink3)];
			};
			const auto Pad = [](const TCHAR* A, const TCHAR* B)
			{
				return TAttribute<FText>::CreateLambda([A, B]()
				{
					const FText First = WRInput::PadButton(A);
					return B ? FText::Format(LOCTEXT("PadPair", "{0} {1}"), First, WRInput::PadButton(B)) : First;
				});
			};
			return SNew(SWidgetSwitcher).WidgetIndex_Lambda([]() { return WRInput::UsingPad() ? 1 : 0; })
				+ SWidgetSwitcher::Slot()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()[Pair(FText::FromString(TEXT("Ctrl+Enter")), LOCTEXT("KSend", "Send"))]
					+ SHorizontalBox::Slot().AutoWidth()[Pair(FText::FromString(TEXT("Esc")), LOCTEXT("KCancel", "Cancel"))]
				]
				+ SWidgetSwitcher::Slot()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()[Pair(Pad(TEXT("TabPrev"), TEXT("TabNext")), LOCTEXT("PKind", "Kind"))]
					+ SHorizontalBox::Slot().AutoWidth()[Pair(Pad(TEXT("MenuControls"), nullptr), LOCTEXT("PSend", "Send"))]
					+ SHorizontalBox::Slot().AutoWidth()[Pair(Pad(TEXT("MenuBack"), nullptr), LOCTEXT("PCancel", "Cancel"))]
				];
		}
	};

	TSharedRef<SWidget> Make(TSharedPtr<SWidget>& OutFocus)
	{
		TSharedRef<SWRReportPanel> Panel = SNew(SWRReportPanel);
		OutFocus = Panel->GetFocusTarget();
		return Panel;
	}
}

#undef LOCTEXT_NAMESPACE
