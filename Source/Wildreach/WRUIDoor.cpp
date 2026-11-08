// A code-lock door's keypad (EWRMenu::Door; the door is the controller's MenuPiece). E on a locked door that isn't
// yours brings it up to enter the code: the right one opens the door, and it's yours to open until the code changes.
// Held E on your own (or your clan's) door brings it up to set a code and lock it, or to take the lock off.
// Digits by click or by the number keys (number row or pad), Backspace, Delete clears, Enter confirms; Esc closes.

#include "WRPlayerUI.h"
#include "WRUIStyle.h"
#include "WRUIWidgets.h"
#include "WRBuilding.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "Wildreach.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBackgroundBlur.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"

#define LOCTEXT_NAMESPACE "WRUI"

namespace WRUIDoorScreen
{
	using WRUI::U;

	class SDoorScreen : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDoorScreen) {}
			SLATE_ARGUMENT(TWeakObjectPtr<AWRPlayerController>, PC)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			PC = InArgs._PC;
			Door = PC.IsValid() ? PC->GetMenuPiece() : nullptr;
			bManage = Door.IsValid() && Door->SeemsShared(PC.Get());

			// 1 2 3 / 4 5 6 / 7 8 9 / Clear 0 Back
			TSharedRef<SUniformGridPanel> Pad = SNew(SUniformGridPanel).SlotPadding(FMargin(5.f * U)).MinDesiredSlotHeight(58.f * U);
			static const TCHAR* const Keys[] = { TEXT("1"), TEXT("2"), TEXT("3"), TEXT("4"), TEXT("5"), TEXT("6"), TEXT("7"), TEXT("8"), TEXT("9"), TEXT("Clear"), TEXT("0"), TEXT("Back") };
			for (int32 i = 0; i < UE_ARRAY_COUNT(Keys); ++i)
			{
				const FString Key = Keys[i];
				const bool bDigit = Key.Len() == 1;
				Pad->AddSlot(i % 3, i / 3)
				[
					SNew(SWRButton).Ghost(!bDigit).FontSize(bDigit ? 22 : 13).MinHeight(58.f * U).Text(FText::FromString(Key))
					.OnClicked_Lambda([this, Key]() { if (Key == TEXT("Clear")) Clear(); else if (Key == TEXT("Back")) Back(); else Digit(Key[0]); return FReply::Handled(); })
				];
			}

			TSharedRef<SHorizontalBox> Code = SNew(SHorizontalBox);
			for (int32 i = 0; i < 4; ++i)
			{
				Code->AddSlot().AutoWidth().Padding(i == 0 ? 0.f : 10.f * U, 0.f, 0.f, 0.f)
				[
					SNew(SBox).WidthOverride(62.f * U).HeightOverride(74.f * U)
					[
						SNew(SOverlay)
						+ SOverlay::Slot()[SNew(SImage).Image_Lambda([this, i]() { return WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.45f), 6.f, i == Digits.Len() && Digits.Len() < 4 ? WRUI::Brass : WRUI::Alpha(WRUI::Rim, 0.9f), i == Digits.Len() && Digits.Len() < 4 ? 2.f : 1.f); })]
						+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
						[
							WRUI::Text(TAttribute<FText>::CreateLambda([this, i]() { return i < Digits.Len() ? FText::FromString(Digits.Mid(i, 1)) : FText::GetEmpty(); }), WRUI::Title(34), WRUI::Ink, true)
						]
					]
				];
			}

			ChildSlot
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[SNew(SBackgroundBlur).BlurStrength(6.f)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.45f))]]
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(470.f * U)
					[
						SNew(SWRPanel).Kind(EWRPanel::Stone).Rivets(true).BrassTrim(true).Padding(FMargin(10.f * U, 10.f * U, 10.f * U, 20.f * U))
						[
							SNew(SVerticalBox)
							// The title band: the door, whose it is, Esc / close.
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(SBox).HeightOverride(60.f * U)
								[
									SNew(SWRPanel).Kind(EWRPanel::Leather).Padding(FMargin(16.f * U, 0.f, 14.f * U, 0.f))
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
										[
											SNew(SVerticalBox)
											+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(DoorName()); }), WRUI::Title(19), WRUI::Ink, true)]
											+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(Whose()); }), WRUI::Body(12, 500), WRUI::Ink2, true)]
										]
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 7.f * U, 0.f)[SNew(SWRKeyCap).FontSize(11).Text(LOCTEXT("DoorEsc", "Esc"))]
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(LOCTEXT("DoorClose", "Close"), WRUI::Body(13, 500), WRUI::Ink2, true)]
									]
								]
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(18.f * U, 16.f * U, 18.f * U, 0.f))
							[
								SNew(SVerticalBox)
								// What it is now, and what to do.
								+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
								[
									WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(StateLine()); }), WRUI::Caps(13),
										TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(IsLocked() ? WRUI::Warn : WRUI::Good); }))
								]
								+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 4.f * U, 0.f, 0.f)
								[
									WRUI::Text(bManage ? LOCTEXT("DoorManageHow", "Type 4 digits, then set them as the code.") : LOCTEXT("DoorEnterHow", "Enter its 4-digit code to open it."), WRUI::Body(13, 500), WRUI::Ink2)
								]
								// The four digits (they shake on a wrong code).
								+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 16.f * U, 0.f, 0.f)
								[
									SNew(SBox).RenderTransform_Lambda([this]()
									{
										const double T = FPlatformTime::Seconds() - WrongAt;
										const float X = T < 0.4 ? FMath::Sin(static_cast<float>(T) * 45.f) * 9.f * U * (1.f - static_cast<float>(T) / 0.4f) : 0.f;
										return TOptional<FSlateRenderTransform>(FSlateRenderTransform(FVector2D(X, 0.f)));
									})
									[
										Code
									]
								]
								// The last answer from the door ("Wrong code", "Code set"...).
								+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 10.f * U, 0.f, 0.f)
								[
									SNew(SBox).HeightOverride(22.f * U)
									[
										WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(Message); }), WRUI::Body(14, 700),
											TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(MessageColor); }))
									]
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f * U, 0.f, 0.f)[Pad]
								// Unlock / Set code and lock, Remove lock.
								+ SVerticalBox::Slot().AutoHeight().Padding(5.f * U, 12.f * U, 5.f * U, 0.f)
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot().FillWidth(1.f)
									[
										SNew(SWRButton).Primary(true).FontSize(15).MinHeight(50.f * U)
										.Text(bManage ? LOCTEXT("DoorSet", "Set code and lock") : LOCTEXT("DoorUnlock", "Unlock"))
										.IsEnabled_Lambda([this]() { return Digits.Len() == 4; })
										.OnClicked_Lambda([this]() { Confirm(); return FReply::Handled(); })
									]
									+ SHorizontalBox::Slot().AutoWidth().Padding(10.f * U, 0.f, 0.f, 0.f)
									[
										SNew(SBox).Visibility_Lambda([this]() { return bManage && IsLocked() ? EVisibility::Visible : EVisibility::Collapsed; })
										[
											SNew(SWRButton).Ghost(true).FontSize(13).MinHeight(50.f * U).Text(LOCTEXT("DoorRemove", "Remove lock"))
											.OnClicked_Lambda([this]() { Send(TEXT("clearcode")); return FReply::Handled(); })
										]
									]
								]
								+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 12.f * U, 0.f, 0.f)
								[
									WRUI::Text(LOCTEXT("DoorKeysHint", "Number keys work too: Backspace, Enter"), WRUI::Body(12, 500), WRUI::Ink3)
								]
							]
						]
					]
				]
			];
		}

		virtual bool SupportsKeyboardFocus() const override { return true; }

		virtual FReply OnKeyDown(const FGeometry& Geo, const FKeyEvent& Event) override
		{
			const FKey K = Event.GetKey();
			static const FKey Row[] = { EKeys::Zero, EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
			static const FKey Num[] = { EKeys::NumPadZero, EKeys::NumPadOne, EKeys::NumPadTwo, EKeys::NumPadThree, EKeys::NumPadFour, EKeys::NumPadFive, EKeys::NumPadSix, EKeys::NumPadSeven, EKeys::NumPadEight, EKeys::NumPadNine };
			for (int32 d = 0; d < 10; ++d)
			{
				if (K == Row[d] || K == Num[d]) { Digit(TCHAR('0' + d)); return FReply::Handled(); }
			}
			if (K == EKeys::BackSpace) { Back(); return FReply::Handled(); }
			if (K == EKeys::Delete) { Clear(); return FReply::Handled(); }
			if (K == EKeys::Enter)
			{
				if (Digits.Len() == 4) Confirm();
				return FReply::Handled();
			}
			return FReply::Unhandled();   // Esc and the rest: the game's keys (Esc closes)
		}

		virtual void Tick(const FGeometry& Geo, const double Time, const float Delta) override
		{
			SCompoundWidget::Tick(Geo, Time, Delta);
			AWRPlayerController* P = PC.Get();
			if (!P) return;
			// Take the keyboard as it opens (E, which opened it, is still on its way up the first frames).
			if (Frames++ < 3) FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EFocusCause::SetDirectly);
			// Gone, or walked away from: close.
			const APawn* Me = P->GetPawn();
			if (!Door.IsValid() || Door->IsActorBeingDestroyed() || !Me || FVector::Dist(Door->GetActorLocation(), Me->GetActorLocation()) > 650.f)
			{
				P->SetMenu(EWRMenu::None);
				return;
			}
			// The door's answer to what was sent comes back as a notice.
			if (SentAt > 0.0 && P->GetNoticeTime() >= SentAt)
			{
				const FString N = P->GetNotice();
				SentAt = 0.0;
				if (N == TEXT("Code accepted"))
				{
					P->SetMenu(EWRMenu::None);   // it opens; the server swung it
					return;
				}
				Message = N;
				const bool bBad = N.StartsWith(TEXT("Wrong")) || N.StartsWith(TEXT("Not")) || N.StartsWith(TEXT("no ")) || N.StartsWith(TEXT("Codes"));
				MessageColor = bBad ? WRUI::Bad : WRUI::Good;
				if (N.StartsWith(TEXT("Wrong"))) WrongAt = FPlatformTime::Seconds();
				Digits.Reset();
			}
		}

	private:
		TWeakObjectPtr<AWRPlayerController> PC;
		TWeakObjectPtr<AWRBuildPiece> Door;
		bool bManage = false;
		FString Digits;
		FString Message;
		FLinearColor MessageColor = WRUI::Ink2;
		double SentAt = 0.0;
		double WrongAt = -100.0;
		int32 Frames = 0;

		bool IsLocked() const { return Door.IsValid() && Door->bLocked; }

		FString DoorName() const
		{
			const FWRPieceDef* Def = Door.IsValid() ? Door->GetDef() : nullptr;
			FString Name = Def ? Def->Name : FString(TEXT("Door"));
			Name.RemoveFromEnd(TEXT(" (code lock)"));
			return Name;
		}

		FString Whose() const
		{
			const AWRBuildPiece* D = Door.Get();
			const FString Me = PC.IsValid() && PC->PlayerState ? PC->PlayerState->GetPlayerName() : FString();
			if (!D || D->OwnerName.IsEmpty()) return FString(TEXT("Code lock"));
			if (D->OwnerName == Me) return FString(TEXT("Your door · code lock"));
			return bManage ? FString::Printf(TEXT("%s's door · your clan"), *D->OwnerName) : FString::Printf(TEXT("%s's door"), *D->OwnerName);
		}

		FString StateLine() const
		{
			if (!IsLocked()) return FString(TEXT("No lock: anyone can open it"));
			return bManage ? FString(TEXT("Locked with a code")) : FString(TEXT("Locked"));
		}

		void Digit(TCHAR C)
		{
			if (Digits.Len() >= 4) return;
			Digits.AppendChar(C);
			if (!Message.IsEmpty() && SentAt <= 0.0) Message.Reset();
		}
		void Back() { if (Digits.Len() > 0) Digits.LeftChopInline(1); }
		void Clear() { Digits.Reset(); }

		void Send(const FString& What)
		{
			AWRPlayerController* P = PC.Get();
			const AWRBuildPiece* D = Door.Get();
			if (!P || !D) return;
			const FVector At = D->GetActorLocation();
			P->SendCommand(FString::Printf(TEXT("%s %.0f %.0f %.0f"), *What, At.X, At.Y, At.Z));
			SentAt = P->GetWorld()->GetTimeSeconds();
			UE_LOG(LogWildreach, Log, TEXT("DOOR keypad sends %s"), *What);
		}

		void Confirm()
		{
			if (Digits.Len() != 4) return;
			Send(FString::Printf(TEXT("%s %s"), bManage ? TEXT("setcode") : TEXT("code"), *Digits));
		}
	};
}

WR_REGISTER_SCREEN("door", [](AWRPlayerController* PC) -> TSharedRef<SWidget> { return SNew(WRUIDoorScreen::SDoorScreen).PC(PC); });

#undef LOCTEXT_NAMESPACE
