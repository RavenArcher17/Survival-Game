// The storage chest (mockup "Storage chest"), shown while the controller's menu is EWRMenu::Chest: your bag and the
// chest side by side. Items cross with the server's store / take commands (a drag asks how many; Shift+click or
// right-click moves the whole stack, Ctrl+click one); storeall / takeall move everything. The chest's slots are
// AWRBuildPiece::Storage, which replicates to every client, so the screen reads them straight off the piece.

#include "WRUIInventory.h"
#include "WRPlayerUI.h"
#include "WRUIStyle.h"
#include "WRUIWidgets.h"
#include "WRBuilding.h"
#include "WRInventory.h"
#include "WRKeyBindings.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRTuning.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameStateBase.h"
#include "HAL/IConsoleManager.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBackgroundBlur.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "WRUI"

using WRUI::U;

namespace WRStorageUI
{
	constexpr float BagW = 610.f, MiddleW = 230.f, Gap = 20.f, Pad = 24.f, PickerW = 356.f;

	class SWRStorageScreen : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SWRStorageScreen) {}
			SLATE_ARGUMENT(TWeakObjectPtr<AWRPlayerController>, PC)
		SLATE_END_ARGS()

		/** The open screen (the testing hook below reaches it through this). */
		static TWeakPtr<SWRStorageScreen> Current;

		void Construct(const FArguments& InArgs)
		{
			PC = InArgs._PC;
			Capacity = ChestCapacity();
			ChildSlot
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[SNew(SBackgroundBlur).BlurStrength(8.f)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.5f))]]
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[SAssignNew(WindowBox, SBox)[Window()]]
			];
		}

		virtual void Tick(const FGeometry& Geo, const double Time, const float Delta) override
		{
			SCompoundWidget::Tick(Geo, Time, Delta);
			if (!PC.IsValid()) return;
			if (!Piece())
			{
				PC->SetMenu(EWRMenu::None);   // the chest is gone (demolished, broken)
				return;
			}
			if (ChestCapacity() != Capacity)
			{
				Capacity = ChestCapacity();
				ClosePicker();
				WRUI::SetContentMeasured(*WindowBox, Window(), Geo.Scale);
			}
			// The stack being moved changed under the picker: close it.
			if (PickSide != EWRSlotKind::Other)
			{
				const FWRItemStack* S = StackAt(PickSide, PickIndex);
				if (!S || S->IsEmpty() || S->ItemId != PickItem) ClosePicker();
			}
		}

		/** Testing / screenshots: the "how many?" picker for a bag slot (into the chest) or a chest slot (into the bag). */
		void DebugPicker(bool bStore, int32 Index) { AskMove(bStore ? EWRSlotKind::Bag : EWRSlotKind::Chest, Index); }

	private:
		TWeakObjectPtr<AWRPlayerController> PC;
		TSharedPtr<SBox> WindowBox;
		TSharedPtr<SBox> PickerBox;
		int32 Capacity = 20;
		EWRSlotKind SelSide = EWRSlotKind::Other;
		int32 SelIndex = INDEX_NONE;
		EWRSlotKind PickSide = EWRSlotKind::Other;
		int32 PickIndex = INDEX_NONE;
		FName PickItem;

		// ------------------------------------------------------------ state
		const AWRPlayerState* PS() const { return PC.IsValid() ? PC->GetPlayerState<AWRPlayerState>() : nullptr; }
		const UWRInventoryComponent* Inv() const { const AWRPlayerState* P = PS(); return P ? P->GetInventory() : nullptr; }
		const AWRBuildPiece* Piece() const { return PC.IsValid() ? PC->GetMenuPiece() : nullptr; }
		void Send(const FString& Command) const { if (PC.IsValid()) PC->SendCommand(Command); }

		int32 ChestCapacity() const { const AWRBuildPiece* P = Piece(); return P && P->Storage.Num() > 0 ? P->Storage.Num() : 20; }
		int32 Columns() const { return Capacity <= 20 ? 5 : Capacity <= 30 ? 6 : 8; }
		float ChestW() const { const int32 C = Columns(); return C * 64.f + (C - 1) * 8.f + 40.f; }

		const FWRItemStack* StackAt(EWRSlotKind Side, int32 i) const
		{
			if (Side == EWRSlotKind::Bag)
			{
				const UWRInventoryComponent* I = Inv();
				return I && I->GetSlots().IsValidIndex(i) ? &I->GetSlots()[i] : nullptr;
			}
			const AWRBuildPiece* P = Piece();
			return Side == EWRSlotKind::Chest && P && P->Storage.IsValidIndex(i) ? &P->Storage[i] : nullptr;
		}

		int32 Used(EWRSlotKind Side) const
		{
			int32 N = 0;
			const UWRInventoryComponent* I = Inv();
			const AWRBuildPiece* P = Piece();
			if (Side == EWRSlotKind::Bag && I) for (const FWRItemStack& S : I->GetSlots()) N += !S.IsEmpty();
			if (Side == EWRSlotKind::Chest && P) for (const FWRItemStack& S : P->Storage) N += !S.IsEmpty();
			return N;
		}

		float Carry() const { const UWRInventoryComponent* I = Inv(); return I ? I->GetCarryWeight() : 0.f; }
		float CarryCap() const { return WRTune(TEXT("inventory.carryKg"), 80.f); }

		FString ChestName() const
		{
			const AWRBuildPiece* P = Piece();
			const FWRPieceDef* Def = P ? P->GetDef() : nullptr;
			return Def ? Def->Name : FString(TEXT("Storage chest"));
		}

		/** Who the chest belongs to, as the chip under its name says it (clan rules: FWRSocialRules::CanShare). */
		void Owner(FString& OutText, FLinearColor& OutColor) const
		{
			const AWRBuildPiece* P = Piece();
			const AWRPlayerState* Me = PS();
			OutColor = WRUI::Good;
			if (!P || !Me) { OutText.Reset(); return; }
			if (P->OwnerName == Me->GetPlayerName())
			{
				OutText = Me->ClanName.IsEmpty() ? FString(TEXT("Your chest")) : FString::Printf(TEXT("Shared with clan %s"), *Me->ClanName);
				return;
			}
			const AWRPlayerState* OwnerPS = nullptr;
			if (const AGameStateBase* GS = PC->GetWorld() ? PC->GetWorld()->GetGameState() : nullptr)
			{
				for (APlayerState* X : GS->PlayerArray) if (X && X->GetPlayerName() == P->OwnerName) OwnerPS = Cast<AWRPlayerState>(X);
			}
			const bool bClan = OwnerPS && !Me->ClanName.IsEmpty() && OwnerPS->ClanName == Me->ClanName;
			OutText = bClan ? FString::Printf(TEXT("%s's chest · your clan"), *P->OwnerName) : FString::Printf(TEXT("%s's chest"), *P->OwnerName);
			OutColor = bClan ? WRUI::Good : WRUI::Warn;
		}

		FWRSlotView ViewOf(EWRSlotKind Side, int32 i) const
		{
			const FWRItemStack* S = StackAt(Side, i);
			if (!S) return FWRSlotView();
			FWRSlotView V = WRInv::StackView(*S);
			V.bSelected = SelSide == Side && SelIndex == i && !V.ItemId.IsNone();
			V.bDimmed = PickSide == Side && PickIndex == i;   // the stack the picker is moving
			return V;
		}

		// ------------------------------------------------------------ moving things
		void MoveAcross(EWRSlotKind From, int32 i, int32 N) const
		{
			const FWRItemStack* S = StackAt(From, i);
			if (!S || S->IsEmpty()) return;
			const TCHAR* Cmd = From == EWRSlotKind::Bag ? TEXT("store") : TEXT("take");
			Send(N >= S->Count ? FString::Printf(TEXT("%s %d"), Cmd, i) : FString::Printf(TEXT("%s %d %d"), Cmd, i, FMath::Max(1, N)));
		}

		void Clicked(EWRSlotKind Side, int32 i, const FPointerEvent& Event)
		{
			const FWRItemStack* S = StackAt(Side, i);
			if (!S || S->IsEmpty()) return;
			const bool bRight = Event.GetEffectingButton() == EKeys::RightMouseButton;
			if (bRight || (Event.GetEffectingButton() == EKeys::LeftMouseButton && Event.IsShiftDown())) MoveAcross(Side, i, S->Count);
			else if (Event.GetEffectingButton() == EKeys::LeftMouseButton && Event.IsControlDown()) MoveAcross(Side, i, 1);
			else if (Event.GetEffectingButton() == EKeys::LeftMouseButton) { SelSide = Side; SelIndex = i; }
		}

		/** A stack dragged across: one moves straight over, more asks how many. */
		void AskMove(EWRSlotKind From, int32 i)
		{
			const FWRItemStack* S = StackAt(From, i);
			const FWRItemDef* Def = S && !S->IsEmpty() ? FWRItemRegistry::Get().Find(S->ItemId) : nullptr;
			if (!Def) return;
			if (S->Count <= 1) { MoveAcross(From, i, 1); return; }
			ClosePicker();
			PickSide = From;
			PickIndex = i;
			PickItem = S->ItemId;
			const bool bStore = From == EWRSlotKind::Bag;
			const FString Name = Def->Name, Into = bStore ? ChestName() : FString(TEXT("your bag"));
			const float Weight = Def->Weight;
			const int32 Count = S->Count;
			const TSharedRef<SWRInvAmountPicker> Picker = SNew(SWRInvAmountPicker)
				.ItemId(S->ItemId).Max(Count).Initial(Count)
				.Title(LOCTEXT("MoveHowMany", "Move how many?")).ConfirmText(LOCTEXT("MoveGo", "Move"))
				.SubLine([Name, Into, Weight](int32 N) { return FText::FromString(FString::Printf(TEXT("%s into %s · %s kg"), *Name, *Into, *WRInv::Kg(N * Weight))); })
				.RestLine([Count, bStore](int32 N) { return FText::FromString(bStore ? FString::Printf(TEXT("%d stay in your bag"), Count - N) : FString::Printf(TEXT("%d stay in the chest"), Count - N)); })
				.OnConfirm_Lambda([this, From, i](int32 N) { MoveAcross(From, i, N); ClosePicker(); })
				.OnCancel_Lambda([this]() { ClosePicker(); });
			// Over the side it's going to, low on the grid (as the mockup has it).
			const float Side = bStore ? Pad + FMath::Max(0.f, (ChestW() - PickerW) * 0.5f) : Pad + (BagW - PickerW) * 0.5f;
			PickerBox->SetContent(
				SNew(SOverlay)
				+ SOverlay::Slot()[SNew(SBorder).BorderImage(WRUI::Box(FLinearColor::Transparent)).OnMouseButtonDown_Lambda([this](const FGeometry&, const FPointerEvent&) { ClosePicker(); return FReply::Handled(); })]
				+ SOverlay::Slot().HAlign(bStore ? HAlign_Right : HAlign_Left).VAlign(VAlign_Bottom)
					.Padding(bStore ? 0.f : Side * U, 0.f, bStore ? Side * U : 0.f, 102.f * U)[Picker]);
			PickerBox->SetVisibility(EVisibility::Visible);
			FSlateApplication::Get().SetKeyboardFocus(Picker, EFocusCause::SetDirectly);
		}

		void ClosePicker()
		{
			PickSide = EWRSlotKind::Other;
			PickIndex = INDEX_NONE;
			if (PickerBox.IsValid())
			{
				PickerBox->SetVisibility(EVisibility::Collapsed);
				PickerBox->SetContent(SNullWidget::NullWidget);
			}
		}

		void DroppedOnSlot(EWRSlotKind Side, int32 i, EWRSlotKind From, int32 FromIndex)
		{
			if (From == EWRSlotKind::Bag && Side == EWRSlotKind::Bag && FromIndex != i) Send(FString::Printf(TEXT("move %d %d"), FromIndex, i));   // tidy the bag here too
			else if (From != Side && (From == EWRSlotKind::Bag || From == EWRSlotKind::Chest)) AskMove(From, FromIndex);
		}

		void DroppedOnBagSide(EWRSlotKind From, int32 FromIndex) { if (From == EWRSlotKind::Chest) AskMove(From, FromIndex); }
		void DroppedOnChestSide(EWRSlotKind From, int32 FromIndex) { if (From == EWRSlotKind::Bag) AskMove(From, FromIndex); }

		// ------------------------------------------------------------ widgets
		TSharedRef<SWidget> Tip(EWRSlotKind Side, int32 i)
		{
			const auto Hint = [](const FText& Key, const FText& What)
			{
				return SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()[WRUI::Text(Key, WRUI::Body(12, 800), WRUI::Brass)]
					+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f, 0.f, 0.f)[WRUI::Text(What, WRUI::Body(12, 500), WRUI::Ink2)];
			};
			return SNew(SBox).WidthOverride(236.f * U)
			[
				SNew(SWRPanel).Kind(EWRPanel::Stone).BrassTrim(true).Padding(FMargin(12.f * U, 10.f * U))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f * U, 0.f)
						[
							SNew(SBox).WidthOverride(30.f * U).HeightOverride(30.f * U)[SNew(SImage).Image_Lambda([this, Side, i]() { const FWRItemStack* S = StackAt(Side, i); return S && !S->IsEmpty() ? WRUI::Icon(S->ItemId) : WRUI::White(); })]
						]
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
						[
							WRInv::Label(TAttribute<FText>::CreateLambda([this, Side, i]()
							{
								const FWRItemStack* S = StackAt(Side, i);
								const FWRItemDef* D = S && !S->IsEmpty() ? FWRItemRegistry::Get().Find(S->ItemId) : nullptr;
								if (!D) return FText::GetEmpty();
								return FText::FromString(S->Count > 1 ? FString::Printf(TEXT("%s x%d"), *D->Name, S->Count) : D->Name);
							}), WRUI::Body(14, 700), WRUI::Ink, true)
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.f * U, 0.f, 0.f, 0.f)
						[
							WRUI::Text(TAttribute<FText>::CreateLambda([this, Side, i]()
							{
								const FWRItemStack* S = StackAt(Side, i);
								const FWRItemDef* D = S && !S->IsEmpty() ? FWRItemRegistry::Get().Find(S->ItemId) : nullptr;
								return D ? FText::FromString(FString::Printf(TEXT("%s kg"), *WRInv::Kg(D->Weight * S->Count))) : FText::GetEmpty();
							}), WRUI::Body(13, 500), WRUI::Ink3)
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f * U)[WRUI::Divider()]
					+ SVerticalBox::Slot().AutoHeight()[Hint(LOCTEXT("TipShift", "Shift+click:"), LOCTEXT("TipShiftWhat", "move the stack"))]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f * U, 0.f, 0.f)[Hint(LOCTEXT("TipCtrl", "Ctrl+click:"), LOCTEXT("TipCtrlWhat", "move one"))]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f * U, 0.f, 0.f)[Hint(LOCTEXT("TipDrag", "Drag:"), LOCTEXT("TipDragWhat", "choose how many"))]
				]
			];
		}

		TSharedRef<SWidget> SlotWidget(EWRSlotKind Side, int32 i)
		{
			const TSharedRef<SToolTip> ToolTip = SNew(SToolTip).BorderImage(WRUI::Box(FLinearColor::Transparent)).TextMargin(0.f)[Tip(Side, i)];
			return SNew(SWRSlot).Size(64.f * U).Kind(Side).Index(i)
				.View_Lambda([this, Side, i]() { return ViewOf(Side, i); })
				.OnClicked_Lambda([this, Side, i](const FPointerEvent& Ev) { Clicked(Side, i, Ev); })
				.OnDropped_Lambda([this, Side, i](EWRSlotKind From, int32 Idx) { DroppedOnSlot(Side, i, From, Idx); })
				.ToolTip_Lambda([this, Side, i, ToolTip]() -> TSharedPtr<IToolTip> { const FWRItemStack* S = StackAt(Side, i); return S && !S->IsEmpty() ? ToolTip : TSharedPtr<SToolTip>(); });
		}

		TSharedRef<SWidget> Count(EWRSlotKind Side, int32 Of)
		{
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)[WRUI::Text(TAttribute<FText>::CreateLambda([this, Side]() { return FText::AsNumber(Used(Side)); }), WRUI::Body(15, 800), WRUI::Ink)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(4.f, 0.f, 0.f, 0.f)[WRUI::Text(FText::FromString(FString::Printf(TEXT("/ %d"), Of)), WRUI::Body(15, 500), WRUI::Ink3)];
		}

		TSharedRef<SWidget> BagPanel()
		{
			TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel).SlotPadding(FMargin(4.f * U));
			for (int32 i = 0; i < UWRInventoryComponent::NumSlots; ++i) Grid->AddSlot(i % 8, i / 8)[SlotWidget(EWRSlotKind::Bag, i)];
			return SNew(SWRPanel).Kind(EWRPanel::Inset).Padding(FMargin(16.f * U))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(4.f * U, 4.f * U, 4.f * U, 0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Bottom)[WRUI::Text(LOCTEXT("YourBag", "Your bag"), WRUI::Title(18), WRUI::Ink)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)[Count(EWRSlotKind::Bag, UWRInventoryComponent::NumSlots)]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(4.f * U, 12.f * U, 4.f * U, 0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(LOCTEXT("Weight", "Weight"), WRUI::Caps(11), WRUI::Ink2)]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(12.f * U, 0.f)
					[
						SNew(SWRBar).Height(14.f * U).Fraction_Lambda([this]() { return Carry() / FMath::Max(1.f, CarryCap()); })
						.Color_Lambda([this]() { return Carry() > CarryCap() ? WRUI::Bad : FLinearColor(FColor(181, 138, 74)); })
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(WRInv::Kg(Carry())); }), WRUI::Body(14, 800),
							TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(Carry() > CarryCap() ? WRUI::Bad : WRUI::Ink); }))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.f, 0.f, 0.f, 0.f)
					[
						WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(FString::Printf(TEXT("/ %s kg"), *WRInv::Kg(CarryCap()))); }), WRUI::Body(14, 500), WRUI::Ink3)
					]
				]
				+ SVerticalBox::Slot().FillHeight(1.f).VAlign(VAlign_Bottom).Padding(0.f, 8.f * U, 0.f, 0.f)[Grid]
			];
		}

		TSharedRef<SWidget> ChestPanel()
		{
			const int32 Cols = Columns();
			TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel).SlotPadding(FMargin(4.f * U));
			for (int32 i = 0; i < Capacity; ++i) Grid->AddSlot(i % Cols, i / Cols)[SlotWidget(EWRSlotKind::Chest, i)];
			return SNew(SWRPanel).Kind(EWRPanel::Inset).Padding(FMargin(16.f * U))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(4.f * U, 4.f * U, 4.f * U, 0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Bottom)[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(ChestName()); }), WRUI::Title(18), WRUI::Ink)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(10.f * U, 0.f, 0.f, 0.f)[Count(EWRSlotKind::Chest, Capacity)]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left).Padding(4.f * U, 12.f * U, 4.f * U, 0.f)
				[
					SNew(SBorder).Padding(FMargin(10.f * U, 3.f * U, 12.f * U, 4.f * U))
					.Visibility_Lambda([this]() { FString T; FLinearColor C; Owner(T, C); return T.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
					.BorderImage_Lambda([this]() { FString T; FLinearColor C; Owner(T, C); return WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.45f), 13.f * U, WRUI::Alpha(C, 0.45f), 1.f); })
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f * U, 0.f)
						[
							SNew(SWRInvLineIcon).Icon(WRInv::Icons::Flag()).Size(14.f * U).Thickness(1.8f).Color_Lambda([this]() { FString T; FLinearColor C; Owner(T, C); return C; })
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							WRUI::Text(TAttribute<FText>::CreateLambda([this]() { FString T; FLinearColor C; Owner(T, C); return FText::FromString(T); }), WRUI::Body(12, 700),
								TAttribute<FSlateColor>::CreateLambda([this]() { FString T; FLinearColor C; Owner(T, C); return FSlateColor(C); }))
						]
					]
				]
				+ SVerticalBox::Slot().FillHeight(1.f).VAlign(VAlign_Bottom).Padding(0.f, 8.f * U, 0.f, 0.f)[Grid]
			];
		}

		TSharedRef<SWidget> Middle()
		{
			return SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(SBox).WidthOverride(56.f * U).HeightOverride(56.f * U)
					[
						SNew(SOverlay)
						+ SOverlay::Slot()[SNew(SImage).Image(WRUI::Box(FLinearColor(0.06f, 0.035f, 0.016f), 28.f * U, WRUI::Brass2, 2.f))]
						+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[SNew(SWRInvLineIcon).Icon(WRInv::Icons::Swap()).Size(28.f * U).Thickness(2.6f).Color(WRUI::Brass)]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 8.f * U, 0.f, 0.f)[WRUI::Text(LOCTEXT("Transfer", "Transfer"), WRUI::Caps(12), WRUI::Ink2)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 18.f * U, 0.f, 0.f)
				[
					WRInv::IconButton(LOCTEXT("StoreAll", "Store all resources"), WRInv::Icons::ArrowRight(), true, true, false, 12, 44.f * U,
						FOnClicked::CreateLambda([this]() { Send(TEXT("storeall")); return FReply::Handled(); }))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(6.f * U, 8.f * U, 6.f * U, 0.f)
				[
					WRInv::Label(LOCTEXT("StoreAllWhat", "Wood, stone, fiber, hide, ores, ingots and your other resources"), WRUI::Body(11, 500), WRUI::Ink3, true, ETextJustify::Center)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f * U, 0.f, 0.f)
				[
					WRInv::IconButton(LOCTEXT("TakeAll", "Take all"), WRInv::Icons::ArrowLeft(), false, false, false, 13, 44.f * U,
						FOnClicked::CreateLambda([this]() { Send(TEXT("takeall")); return FReply::Handled(); }))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f * U, 0.f, 0.f)
				[
					WRInv::IconButton(LOCTEXT("SortBag", "Sort your bag"), WRInv::Icons::Sort(), false, false, true, 13, 44.f * U,
						FOnClicked::CreateLambda([this]() { WRInv::SortBag(PC.Get()); return FReply::Handled(); }))
				];
		}

		TSharedRef<SWidget> Footer()
		{
			const auto Item = [](const TSharedRef<SWidget>& Key, const FText& Lead, const FText& Rest)
			{
				return SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Key]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f * U, 0.f, 0.f, 0.f)[WRUI::Text(Lead, WRUI::Body(13, 500), WRUI::Ink2)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(5.f * U, 0.f, 0.f, 0.f)[WRUI::Text(Rest, WRUI::Body(13, 500), WRUI::Ink3)];
			};
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[Item(SNew(SWRKeyCap).FontSize(12).Text_Lambda([]() { return WRUI::KeyName(FWRKeyBindings::Get().Key(TEXT("Interact"))); }), LOCTEXT("FootClose", "Close"), FText::GetEmpty())]
				+ SHorizontalBox::Slot().AutoWidth().Padding(36.f * U, 0.f, 0.f, 0.f)[Item(SNew(SWRKeyCap).FontSize(12).Text(LOCTEXT("KShift", "Shift")), LOCTEXT("FootClick", "+ click"), LOCTEXT("FootStack", "· move the stack"))]
				+ SHorizontalBox::Slot().AutoWidth().Padding(36.f * U, 0.f, 0.f, 0.f)[Item(SNew(SWRKeyCap).FontSize(12).Text(LOCTEXT("KCtrl", "Ctrl")), LOCTEXT("FootClick2", "+ click"), LOCTEXT("FootOne", "· move one"))]
				+ SHorizontalBox::Slot().AutoWidth().Padding(36.f * U, 0.f, 0.f, 0.f)[Item(SNew(SWRInvLineIcon).Icon(WRInv::Icons::Drag()).Size(18.f * U).Thickness(1.6f).Color(WRUI::Ink2), LOCTEXT("FootDrag", "Drag"), LOCTEXT("FootHowMany", "· choose how many"))];
		}

		TSharedRef<SWidget> Window()
		{
			const float Width = Pad * 2.f + BagW + Gap + MiddleW + Gap + ChestW();
			return SNew(SBox).WidthOverride(Width * U)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SWRPanel).Kind(EWRPanel::Stone).Rivets(true).BrassTrim(true).Padding(FMargin(Pad * U))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(BagW * U)[SNew(SWRInvDropZone).OnDropped(this, &SWRStorageScreen::DroppedOnBagSide)[BagPanel()]]]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(Gap * U, 0.f)[SNew(SBox).WidthOverride(MiddleW * U)[Middle()]]
							+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(ChestW() * U)[SNew(SWRInvDropZone).OnDropped(this, &SWRStorageScreen::DroppedOnChestSide)[ChestPanel()]]]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 16.f * U)[WRUI::Divider()]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Footer()]
					]
				]
				+ SOverlay::Slot()[SAssignNew(PickerBox, SBox).Visibility(EVisibility::Collapsed)]
			];
		}
	};

	TWeakPtr<SWRStorageScreen> SWRStorageScreen::Current;
}

WR_REGISTER_SCREEN("storage", [](AWRPlayerController* PC) -> TSharedRef<SWidget>
{
	TSharedRef<WRStorageUI::SWRStorageScreen> Screen = SNew(WRStorageUI::SWRStorageScreen).PC(PC);
	WRStorageUI::SWRStorageScreen::Current = Screen;
	return Screen;
});

#if !UE_BUILD_SHIPPING
// Testing / screenshots (uishots.ps1): open the storage screen's "how many?" picker for a bag slot (store) or chest slot (take).
static FAutoConsoleCommandWithWorldAndArgs GWRUIStoragePicker(TEXT("WRUIStoragePicker"), TEXT("Storage screen: open the move picker (store|take <slot or item>)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const TSharedPtr<WRStorageUI::SWRStorageScreen> Screen = WRStorageUI::SWRStorageScreen::Current.Pin();
		const AWRPlayerController* PC = World ? Cast<AWRPlayerController>(World->GetFirstPlayerController()) : nullptr;
		const AWRPlayerState* PS = PC ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
		const AWRBuildPiece* Chest = PC ? PC->GetMenuPiece() : nullptr;
		if (!Screen.IsValid() || !PS || !PS->GetInventory() || !Chest || Args.Num() < 2) return;
		const bool bStore = Args[0] == TEXT("store");
		const FName Item(*Args[1]);
		const TArray<FWRItemStack>& From = bStore ? PS->GetInventory()->GetSlots() : Chest->Storage;
		const int32 Slot = Args[1].IsNumeric() ? FCString::Atoi(*Args[1]) : From.IndexOfByPredicate([Item](const FWRItemStack& S) { return !S.IsEmpty() && S.ItemId == Item; });
		if (Slot != INDEX_NONE) Screen->DebugPicker(bStore, Slot);
	}));
#endif

#undef LOCTEXT_NAMESPACE
