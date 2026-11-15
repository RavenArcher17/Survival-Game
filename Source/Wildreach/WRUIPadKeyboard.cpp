// The pad keyboard (cookpass-ui 3.4): see WRUIPadKeyboard.h.

#include "WRUIPadKeyboard.h"
#include "Wildreach.h"
#include "WRPadInput.h"
#include "WRSound.h"
#include "WRTuning.h"
#include "WRUIStyle.h"
#include "WRUIWidgets.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "WRPadKeyboard"

namespace WRPadKb
{
	using namespace WRUI;

	/** A key: what it types (lower / upper), or what it does; its width in grid units. */
	enum class EKbKind : uint8 { Char, Shift, Space, Back, Done };
	struct FKbKey { FString Lower, Upper; EKbKind Kind = EKbKind::Char; float Width = 1.f; };

	const TArray<TArray<FKbKey>>& Rows()
	{
		static const TArray<TArray<FKbKey>> R = []()
		{
			TArray<TArray<FKbKey>> Out;
			const auto Row = [&Out](const TCHAR* Lower, const TCHAR* Upper)
			{
				TArray<FKbKey>& Keys = Out.AddDefaulted_GetRef();
				const FString L(Lower), Up(Upper);
				for (int32 i = 0; i < L.Len(); ++i) Keys.Add({ L.Mid(i, 1), Up.Mid(i, 1), EKbKind::Char, 1.f });
			};
			Row(TEXT("1234567890"), TEXT("!@#$%&*()="));
			Row(TEXT("qwertyuiop"), TEXT("QWERTYUIOP"));
			Row(TEXT("asdfghjkl'"), TEXT("ASDFGHJKL\""));
			Row(TEXT("zxcvbnm-.?"), TEXT("ZXCVBNM_,:"));
			TArray<FKbKey>& Last = Out.AddDefaulted_GetRef();
			Last.Add({ TEXT(""), TEXT(""), EKbKind::Shift, 2.f });
			Last.Add({ TEXT(" "), TEXT(" "), EKbKind::Space, 4.f });
			Last.Add({ TEXT(""), TEXT(""), EKbKind::Back, 2.f });
			Last.Add({ TEXT(""), TEXT(""), EKbKind::Done, 2.f });
			return Out;
		}();
		return R;
	}

	/** The middle of a key across its row, 0..10 (to move up / down to the key under it). */
	float KeyCentre(int32 Row, int32 Col)
	{
		float X = 0.f;
		const TArray<FKbKey>& Keys = Rows()[Row];
		for (int32 i = 0; i < Col; ++i) X += Keys[i].Width;
		return X + Keys[Col].Width * 0.5f;
	}

	class SWRPadKbPanel;
	TSharedPtr<SWRPadKbPanel> GPanel;
	TSharedPtr<SWidget> GHost;

	/** One key: a stone cap, brass when picked; the cursor over it picks it, a click presses it. */
	class SWRPadKbKey : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SWRPadKbKey) {}
		SLATE_END_ARGS()
		void Construct(const FArguments& InArgs, TFunction<bool()> InPicked, TFunction<void()> InHover, TFunction<void()> InPress, TAttribute<FText> Label)
		{
			Picked = MoveTemp(InPicked);
			Hover = MoveTemp(InHover);
			Press = MoveTemp(InPress);
			ChildSlot
			[
				SNew(SBox).HeightOverride(50.f * U)
				[
					SNew(SBorder).HAlign(HAlign_Center).VAlign(VAlign_Center)
					.BorderImage_Lambda([this]()
					{
						return Picked() ? WRUI::Box(FLinearColor(FColor(0x4D, 0x39, 0x20)), 5.f, WRUI::Brass, 2.f)
							: WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.45f), 5.f, WRUI::Alpha(WRUI::Brass2, 0.7f), 1.f);
					})
					[
						SNew(STextBlock).Text(Label).Font(WRUI::Body(18, 700))
						.ColorAndOpacity_Lambda([this]() { return FSlateColor(Picked() ? WRUI::Ink : WRUI::Ink2); })
					]
				]
			];
		}
		virtual void OnMouseEnter(const FGeometry& Geo, const FPointerEvent& E) override { SCompoundWidget::OnMouseEnter(Geo, E); Hover(); }
		virtual FReply OnMouseButtonDown(const FGeometry&, const FPointerEvent& E) override
		{
			if (E.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
			Hover();
			Press();
			return FReply::Handled();
		}
		virtual FCursorReply OnCursorQuery(const FGeometry&, const FPointerEvent&) const override { return FCursorReply::Cursor(EMouseCursor::Hand); }
		virtual bool SupportsKeyboardFocus() const override { return false; }   // the box being typed into keeps the focus
	private:
		TFunction<bool()> Picked;
		TFunction<void()> Hover;
		TFunction<void()> Press;
	};

	class SWRPadKbPanel : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SWRPadKbPanel) {}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, WRPadKeyboard::FTarget InTarget)
		{
			Target = MoveTemp(InTarget);
			Text = Target.Initial.ToString().Left(Target.MaxLength);
			TSharedRef<SVerticalBox> Grid = SNew(SVerticalBox);
			for (int32 R = 0; R < Rows().Num(); ++R)
			{
				TSharedRef<SHorizontalBox> Line = SNew(SHorizontalBox);
				for (int32 C = 0; C < Rows()[R].Num(); ++C)
				{
					Line->AddSlot().FillWidth(Rows()[R][C].Width).Padding(3.f * U)
					[
						SNew(SWRPadKbKey, [this, R, C]() { return Row == R && Col == C; }, [this, R, C]() { Row = R; Col = C; }, [this, R, C]() { Row = R; Col = C; PressPicked(); },
							TAttribute<FText>::CreateLambda([this, R, C]() { return KeyLabel(R, C); }))
					];
				}
				Grid->AddSlot().AutoHeight()[Line];
			}
			ChildSlot
			[
				SNew(SBox).WidthOverride(980.f * U)
				[
					SNew(SWRPanel).Kind(EWRPanel::Stone).Rivets(true).BrassTrim(true).Padding(FMargin(20.f * U, 16.f * U))
					[
						SNew(SVerticalBox)
						// The line as it will be, with the caret.
						+ SVerticalBox::Slot().AutoHeight().Padding(3.f * U, 0.f, 3.f * U, 10.f * U)
						[
							SNew(SBorder).BorderImage(WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.55f), 5.f, WRUI::Brass2, 1.f)).Padding(FMargin(14.f * U, 10.f * U))
							[
								SNew(STextBlock).Font(WRUI::Body(18, 500)).AutoWrapText(true)
								.Text_Lambda([this]() { return PreviewText(); })
								.ColorAndOpacity_Lambda([this]() { return FSlateColor(Text.IsEmpty() ? WRUI::Ink3 : WRUI::Ink); })
							]
						]
						+ SVerticalBox::Slot().AutoHeight()[Grid]
						// What the buttons do, in the pad's own names.
						+ SVerticalBox::Slot().AutoHeight().Padding(3.f * U, 10.f * U, 3.f * U, 0.f)
						[
							SNew(STextBlock).Font(WRUI::Body(13, 700)).ColorAndOpacity(WRUI::Ink3).Justification(ETextJustify::Center)
							.Text_Lambda([]()
							{
								const FWRPadLayout& L = FWRPadLayout::Get();
								return FText::Format(LOCTEXT("KbHelp", "{0} type   ·   {1} delete   ·   {2} space   ·   {3} shift   ·   {4} done   ·   {5} cancel"),
									L.Label(EKeys::Gamepad_FaceButton_Bottom), L.Label(EKeys::Gamepad_FaceButton_Left), L.Label(EKeys::Gamepad_FaceButton_Top),
									L.Label(EKeys::Gamepad_LeftTrigger), L.Label(EKeys::Gamepad_RightTrigger), L.Label(EKeys::Gamepad_FaceButton_Right));
							})
						]
					]
				]
			];
		}

		virtual bool SupportsKeyboardFocus() const override { return false; }

		WRPadKeyboard::FTarget Target;
		FString Text;
		int32 Row = 1, Col = 0;   // starts on Q
		int32 Shift = 0;          // 0 off, 1 the next letter, 2 locked
		double LastShiftAt = -10.0;

		FText KeyLabel(int32 R, int32 C) const
		{
			const FKbKey& K = Rows()[R][C];
			switch (K.Kind)
			{
			case EKbKind::Shift: return Shift == 2 ? LOCTEXT("ShiftLock", "SHIFT LOCK") : Shift == 1 ? LOCTEXT("ShiftOn", "SHIFT") : LOCTEXT("Shift", "Shift");
			case EKbKind::Space: return LOCTEXT("Space", "Space");
			case EKbKind::Back: return LOCTEXT("Back", "Delete");
			case EKbKind::Done: return LOCTEXT("Done", "Done");
			default: return FText::FromString(Shift > 0 ? K.Upper : K.Lower);
			}
		}

		FText PreviewText() const
		{
			if (Text.IsEmpty() && !Target.Hint.IsEmpty()) return Target.Hint;
			// A caret that blinks at the end (twice a second).
			const bool bCaret = FMath::Fmod(FPlatformTime::Seconds(), 1.0) < 0.5;
			return FText::FromString(Text + (bCaret ? TEXT("|") : TEXT(" ")));
		}

		void Type(const FString& S)
		{
			if (Text.Len() + S.Len() > Target.MaxLength) return;
			Text += S;
			if (Shift == 1) Shift = 0;
			WRSound::PlayUI(nullptr, TEXT("ui.click"));
		}
		void Delete()
		{
			if (Text.IsEmpty()) return;
			Text.LeftChopInline(1);
			WRSound::PlayUI(nullptr, TEXT("ui.click"));
		}
		void ToggleShift()
		{
			const double Now = FPlatformTime::Seconds();
			// Twice quickly locks it; once more lets go.
			Shift = Shift == 2 ? 0 : (Shift == 1 && Now - LastShiftAt < WRTune(TEXT("ui.padKeyboard.lockSeconds"), 0.4f)) ? 2 : Shift == 1 ? 0 : 1;
			LastShiftAt = Now;
			WRSound::PlayUI(nullptr, TEXT("ui.click"));
		}
		void PressPicked()
		{
			const FKbKey& K = Rows()[Row][Col];
			switch (K.Kind)
			{
			case EKbKind::Shift: ToggleShift(); break;
			case EKbKind::Space: Type(TEXT(" ")); break;
			case EKbKind::Back: Delete(); break;
			case EKbKind::Done: WRPadKeyboard::Close(true); break;
			default: Type(Shift > 0 ? K.Upper : K.Lower); break;
			}
		}
		void MoveBy(int32 DX, int32 DY)
		{
			if (DX != 0)
			{
				const int32 N = Rows()[Row].Num();
				Col = ((Col + DX) % N + N) % N;
			}
			if (DY != 0)
			{
				// To the key under the middle of this one in the next row (the wide keys too); rows wrap.
				const float X = KeyCentre(Row, Col);
				Row = ((Row + DY) % Rows().Num() + Rows().Num()) % Rows().Num();
				int32 Best = 0;
				float BestD = TNumericLimits<float>::Max();
				for (int32 C = 0; C < Rows()[Row].Num(); ++C)
				{
					const float D = FMath::Abs(KeyCentre(Row, C) - X);
					if (D < BestD) { BestD = D; Best = C; }
				}
				Col = Best;
			}
		}
	};
}

bool WRPadKeyboard::IsOpen() { return WRPadKb::GPanel.IsValid(); }

void WRPadKeyboard::Open(FTarget Target)
{
	using namespace WRPadKb;
	if (IsOpen() || !GEngine || !GEngine->GameViewport) return;
	if (Target.MaxLength <= 0) Target.MaxLength = 200;
	// Docked at the bottom, or the top for a box low on the screen; the preview line repeats what's being typed.
	const bool bTop = Target.bDockTop;
	TSharedRef<SWRPadKbPanel> Panel = SNew(SWRPadKbPanel, MoveTemp(Target));
	GPanel = Panel;
	GHost = SNew(SOverlay)
		+ SOverlay::Slot()[SNew(SImage).Image(WRUI::White()).ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.35f))]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(bTop ? VAlign_Top : VAlign_Bottom).Padding(0.f, 36.f * WRUI::U)[Panel];
	GEngine->GameViewport->AddViewportWidgetContent(GHost.ToSharedRef(), 3000);
	WRInput::AddNavRoot(Panel, 3000);
	UE_LOG(LogWildreach, Log, TEXT("PADKB open"));
}

void WRPadKeyboard::Close(bool bCommit)
{
	using namespace WRPadKb;
	if (!GPanel.IsValid()) return;
	// Taken out first, so a Done that opens another box (or closes the screen under it) finds it closed.
	const TSharedPtr<SWRPadKbPanel> Panel = GPanel;
	GPanel.Reset();
	WRInput::RemoveNavRoot(Panel);
	if (GHost.IsValid() && GEngine && GEngine->GameViewport) GEngine->GameViewport->RemoveViewportWidgetContent(GHost.ToSharedRef());
	GHost.Reset();
	UE_LOG(LogWildreach, Log, TEXT("PADKB %s"), bCommit ? TEXT("done") : TEXT("cancelled"));
	if (bCommit)
	{
		if (Panel->Target.OnDone) Panel->Target.OnDone(FText::FromString(Panel->Text));
	}
	else if (Panel->Target.OnCancel)
	{
		Panel->Target.OnCancel();
	}
}

bool WRPadKeyboard::PadButton(const FKey& Key)
{
	using namespace WRPadKb;
	if (!GPanel.IsValid()) return false;
	if (Key == EKeys::Gamepad_FaceButton_Bottom) GPanel->PressPicked();
	else if (Key == EKeys::Gamepad_FaceButton_Left) GPanel->Delete();
	else if (Key == EKeys::Gamepad_FaceButton_Top) GPanel->Type(TEXT(" "));
	else if (Key == EKeys::Gamepad_LeftTrigger) GPanel->ToggleShift();
	else if (Key == EKeys::Gamepad_RightTrigger || Key == EKeys::Gamepad_Special_Right) Close(true);
	else if (Key == EKeys::Gamepad_FaceButton_Right) Close(false);
	else if (Key == EKeys::Gamepad_DPad_Left || Key == EKeys::Gamepad_DPad_Right || Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_DPad_Down) Move(Key);
	return true;   // every pad button is the keyboard's while it's up
}

void WRPadKeyboard::Move(const FKey& K)
{
	using namespace WRPadKb;
	if (!GPanel.IsValid()) return;
	GPanel->MoveBy(K == EKeys::Gamepad_DPad_Left ? -1 : K == EKeys::Gamepad_DPad_Right ? 1 : 0, K == EKeys::Gamepad_DPad_Up ? -1 : K == EKeys::Gamepad_DPad_Down ? 1 : 0);
}

bool WRPadKeyboard::IsEditable(const SWidget& W)
{
	const FName T = W.GetType();
	return T == TEXT("SEditableText") || T == TEXT("SMultiLineEditableText") || T == TEXT("SEditableTextBox") || T == TEXT("SMultiLineEditableTextBox");
}

bool WRPadKeyboard::OpenOn(const TSharedRef<SWidget>& Editable)
{
	const FName T = Editable->GetType();
	FTarget Target;
	const TWeakPtr<SWidget> Weak = Editable;
	// After Done: the text goes in; a one-line box gets Enter (its owner's commit: the chat sends, a name is taken).
	const auto Commit = [Weak](bool bEnter)
	{
		const TSharedPtr<SWidget> W = Weak.Pin();
		if (!W.IsValid() || !FSlateApplication::IsInitialized()) return;
		FSlateApplication& App = FSlateApplication::Get();
		App.SetKeyboardFocus(W, EFocusCause::SetDirectly);
		if (bEnter)
		{
			const FKeyEvent Ev(EKeys::Enter, App.GetModifierKeys(), App.GetUserIndexForKeyboard(), false, 0, 0);
			App.ProcessKeyDownEvent(Ev);
			App.ProcessKeyUpEvent(Ev);
		}
	};
	if (T == TEXT("SEditableText"))
	{
		TSharedRef<SEditableText> E = StaticCastSharedRef<SEditableText>(Editable);
		if (E->IsTextReadOnly()) return false;
		Target.Initial = E->GetText();
		Target.Hint = E->GetHintText();
		Target.OnDone = [Weak, Commit](const FText& Text) { if (const TSharedPtr<SWidget> W = Weak.Pin()) { StaticCastSharedPtr<SEditableText>(W)->SetText(Text); Commit(true); } };
	}
	else if (T == TEXT("SEditableTextBox"))
	{
		TSharedRef<SEditableTextBox> E = StaticCastSharedRef<SEditableTextBox>(Editable);
		Target.Initial = E->GetText();
		Target.OnDone = [Weak, Commit](const FText& Text) { if (const TSharedPtr<SWidget> W = Weak.Pin()) { StaticCastSharedPtr<SEditableTextBox>(W)->SetText(Text); Commit(true); } };
	}
	else if (T == TEXT("SMultiLineEditableText"))
	{
		TSharedRef<SMultiLineEditableText> E = StaticCastSharedRef<SMultiLineEditableText>(Editable);
		if (E->IsTextReadOnly()) return false;
		Target.Initial = E->GetText();
		Target.Hint = E->GetHintText();
		Target.bMultiLine = true;
		Target.MaxLength = 1000;
		Target.OnDone = [Weak, Commit](const FText& Text) { if (const TSharedPtr<SWidget> W = Weak.Pin()) { StaticCastSharedPtr<SMultiLineEditableText>(W)->SetText(Text); Commit(false); } };
	}
	else if (T == TEXT("SMultiLineEditableTextBox"))
	{
		TSharedRef<SMultiLineEditableTextBox> E = StaticCastSharedRef<SMultiLineEditableTextBox>(Editable);
		Target.Initial = E->GetText();
		Target.bMultiLine = true;
		Target.MaxLength = 1000;
		Target.OnDone = [Weak, Commit](const FText& Text) { if (const TSharedPtr<SWidget> W = Weak.Pin()) { StaticCastSharedPtr<SMultiLineEditableTextBox>(W)->SetText(Text); Commit(false); } };
	}
	else
	{
		return false;
	}
	// Docked clear of the box: a box in the lower half of the window gets the keyboard at the top.
	if (FSlateApplication::IsInitialized())
	{
		const TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(Editable);
		if (Window.IsValid())
		{
			const float BoxY = Editable->GetTickSpaceGeometry().GetAbsolutePositionAtCoordinates(FVector2f(0.5f, 0.5f)).Y;
			const float Top = Window->GetPositionInScreen().Y, Height = Window->GetSizeInScreen().Y;
			Target.bDockTop = Height > 0.f && (BoxY - Top) / Height > 0.5f;
		}
	}
	Open(MoveTemp(Target));
	return true;
}

#undef LOCTEXT_NAMESPACE
