// The journal's Inventory tab (mockup "Inventory"): what you wear and hold, the 40-slot bag, the selected item's
// details, and the item hotbar strip. It reads the replicated inventory and changes it only through the server's
// commands: slot (use / equip), unequip, move, split, drop. Also the pieces the storage screen shares (WRUIInventory.h).

#include "WRUIInventory.h"
#include "WRPlayerUI.h"
#include "WRUIStyle.h"
#include "WRUIWidgets.h"
#include "WRBuilding.h"
#include "WRCharacter.h"
#include "WRCombatLibrary.h"
#include "WRCombatTypes.h"
#include "WRCrafting.h"
#include "WRInventory.h"
#include "WRKeyBindings.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRTuning.h"
#include "WRWorldDressing.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "Rendering/DrawElements.h"
#include "UnrealClient.h"
#include "Widgets/SViewport.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "WRUI"

using WRUI::U;

// ---------------------------------------------------------------- shared pieces

void SWRInvLineIcon::Construct(const FArguments& InArgs)
{
	Icon = InArgs._Icon;
	Size = InArgs._Size;
	Thickness = InArgs._Thickness;
	Color = InArgs._Color;
}

int32 SWRInvLineIcon::OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
	const FVector2D Local = Geo.GetLocalSize();
	const float S = FMath::Min(Local.X, Local.Y) / 24.f;
	const FVector2f Off((Local.X - 24.f * S) * 0.5f, (Local.Y - 24.f * S) * 0.5f);
	const FLinearColor C = Color.Get();
	for (const TArray<FVector2f>& Line : Icon.Lines)
	{
		TArray<FVector2f> Pts;
		for (const FVector2f& P : Line) Pts.Add(Off + P * S);
		FSlateDrawElement::MakeLines(Out, Layer, Geo.ToPaintGeometry(), Pts, ESlateDrawEffect::None, C, true, Thickness);
	}
	for (const FVector3f& D : Icon.Dots)
	{
		const float R = D.Z * S;
		WRUI::DrawBox(Out, Layer, Geo.ToPaintGeometry(FVector2D(R * 2.f, R * 2.f), FSlateLayoutTransform(FVector2D(Off.X + D.X * S - R, Off.Y + D.Y * S - R))), WRUI::Box(C, R));
	}
	return Layer;
}

void SWRInvDropZone::Construct(const FArguments& InArgs)
{
	OnDropped = InArgs._OnDropped;
	// Hit-testable itself, so a drop on empty space inside it lands here.
	SetVisibility(EVisibility::Visible);
	ChildSlot[InArgs._Content.Widget];
}

FReply SWRInvDropZone::OnDrop(const FGeometry& Geo, const FDragDropEvent& Event)
{
	const TSharedPtr<FWRItemDragDrop> Op = Event.GetOperationAs<FWRItemDragDrop>();
	if (!Op.IsValid() || !OnDropped.IsBound())
	{
		return FReply::Unhandled();
	}
	OnDropped.Execute(Op->From, Op->Index);
	return FReply::Handled();
}

/** The picker's slider: a dark track, a brass fill and a round brass knob; drag, click or scroll the wheel. */
class SWRInvAmountSlider : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SWRInvAmountSlider) : _Max(1) {}
		SLATE_ATTRIBUTE(int32, Value)
		SLATE_ARGUMENT(int32, Max)
		SLATE_EVENT(FOnWRInvAmount, OnChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		Value = InArgs._Value;
		Max = FMath::Max(1, InArgs._Max);
		OnChanged = InArgs._OnChanged;
		SetCursor(EMouseCursor::Hand);
	}
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(200.f, 22.f * U); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override
	{
		const FVector2D Size = Geo.GetLocalSize();
		const float Knob = 20.f * U, TrackH = 8.f * U;
		const float KX = KnobX(Size.X);
		const float TY = (Size.Y - TrackH) * 0.5f;
		WRUI::DrawBox(Out, Layer, Geo.ToPaintGeometry(FVector2D(Size.X, TrackH), FSlateLayoutTransform(FVector2D(0.f, TY))), WRUI::Box(FLinearColor(0.006f, 0.007f, 0.009f), 4.f * U, FLinearColor::Black, 1.f));
		WRUI::DrawBox(Out, Layer + 1, Geo.ToPaintGeometry(FVector2D(KX, TrackH), FSlateLayoutTransform(FVector2D(0.f, TY))), WRUI::Box(WRUI::Brass, 4.f * U));
		WRUI::DrawBox(Out, Layer + 2, Geo.ToPaintGeometry(FVector2D(KX, TrackH * 0.45f), FSlateLayoutTransform(FVector2D(0.f, TY))), WRUI::Box(FLinearColor(1.f, 1.f, 1.f, 0.22f), 3.f * U));
		const FVector2D KnobPos(KX - Knob * 0.5f, (Size.Y - Knob) * 0.5f);
		WRUI::DrawBox(Out, Layer + 2, Geo.ToPaintGeometry(FVector2D(Knob + 8.f, Knob + 8.f), FSlateLayoutTransform(KnobPos - FVector2D(4.f, 4.f))), WRUI::Box(WRUI::Alpha(WRUI::Ember, 0.22f), Knob * 0.5f + 4.f));
		WRUI::DrawBox(Out, Layer + 3, Geo.ToPaintGeometry(FVector2D(Knob, Knob), FSlateLayoutTransform(KnobPos)), WRUI::Box(FLinearColor(0.32f, 0.19f, 0.05f), Knob * 0.5f, FLinearColor::Black, 1.f));
		WRUI::DrawBox(Out, Layer + 4, Geo.ToPaintGeometry(FVector2D(Knob * 0.72f, Knob * 0.72f), FSlateLayoutTransform(KnobPos + FVector2D(Knob * 0.1f, Knob * 0.08f))), WRUI::Box(FLinearColor(0.62f, 0.45f, 0.2f), Knob * 0.36f));
		WRUI::DrawBox(Out, Layer + 5, Geo.ToPaintGeometry(FVector2D(Knob * 0.36f, Knob * 0.36f), FSlateLayoutTransform(KnobPos + FVector2D(Knob * 0.22f, Knob * 0.18f))), WRUI::Box(FLinearColor(0.87f, 0.67f, 0.34f), Knob * 0.18f));
		return Layer + 5;
	}
	virtual FReply OnMouseButtonDown(const FGeometry& Geo, const FPointerEvent& Event) override
	{
		if (Event.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
		SetFrom(Geo, Event);
		return FReply::Handled().CaptureMouse(SharedThis(this));
	}
	virtual FReply OnMouseMove(const FGeometry& Geo, const FPointerEvent& Event) override
	{
		if (!HasMouseCapture()) return FReply::Unhandled();
		SetFrom(Geo, Event);
		return FReply::Handled();
	}
	virtual FReply OnMouseButtonUp(const FGeometry& Geo, const FPointerEvent& Event) override
	{
		return HasMouseCapture() ? FReply::Handled().ReleaseMouseCapture() : FReply::Unhandled();
	}
	virtual FReply OnMouseWheel(const FGeometry& Geo, const FPointerEvent& Event) override
	{
		OnChanged.ExecuteIfBound(FMath::Clamp(Value.Get() + (Event.GetWheelDelta() > 0.f ? 1 : -1), 1, Max));
		return FReply::Handled();
	}

private:
	float KnobX(float Width) const
	{
		const float Half = 10.f * U;
		const float F = Max > 1 ? float(Value.Get() - 1) / float(Max - 1) : 1.f;
		return FMath::Lerp(Half, Width - Half, FMath::Clamp(F, 0.f, 1.f));
	}
	void SetFrom(const FGeometry& Geo, const FPointerEvent& Event)
	{
		const float Half = 10.f * U;
		const float X = Geo.AbsoluteToLocal(Event.GetScreenSpacePosition()).X;
		const float F = FMath::Clamp((X - Half) / FMath::Max(1.f, Geo.GetLocalSize().X - 2.f * Half), 0.f, 1.f);
		OnChanged.ExecuteIfBound(1 + FMath::RoundToInt(F * (Max - 1)));
	}
	TAttribute<int32> Value;
	int32 Max = 1;
	FOnWRInvAmount OnChanged;
};

void SWRInvAmountPicker::Construct(const FArguments& InArgs)
{
	Max = FMath::Max(1, InArgs._Max);
	Value = FMath::Clamp(InArgs._Initial, 1, Max);
	OnConfirm = InArgs._OnConfirm;
	OnCancel = InArgs._OnCancel;
	const TFunction<FText(int32)> SubLine = InArgs._SubLine;
	const TFunction<FText(int32)> RestLine = InArgs._RestLine;
	FWRSlotView Icon;
	Icon.ItemId = InArgs._ItemId;

	ChildSlot
	[
		SNew(SBox).WidthOverride(356.f * U)
		[
			SNew(SWRPanel).Kind(EWRPanel::Stone).BrassTrim(true).Padding(FMargin(16.f * U, 14.f * U))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SWRSlot).Size(42.f * U).View(Icon)]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(12.f * U, 0.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(InArgs._Title, WRUI::Title(14), WRUI::Ink)]
						+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda([this, SubLine]() { return SubLine ? SubLine(Value) : FText::GetEmpty(); }), WRUI::Body(12, 500), WRUI::Ink3)]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::AsNumber(Value); }), WRUI::Body(20, 800), WRUI::Ink)]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(4.f, 0.f, 0.f, 2.f)[WRUI::Text(FText::FromString(FString::Printf(TEXT("/ %d"), Max)), WRUI::Body(13, 500), WRUI::Ink3)]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f * U, 0.f, 0.f)
				[
					SNew(SWRInvAmountSlider).Max(Max).Value_Lambda([this]() { return Value; }).OnChanged_Lambda([this](int32 V) { Value = V; })
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f * U, 0.f, 0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[
						WRUI::Text(TAttribute<FText>::CreateLambda([this, RestLine]() { return RestLine ? RestLine(Value) : FText::GetEmpty(); }), WRUI::Body(12, 500), WRUI::Ink3)
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						WRInv::TextButton(LOCTEXT("PickCancel", "Cancel"), false, true, 12, 36.f * U, FOnClicked::CreateLambda([this]() { OnCancel.ExecuteIfBound(); return FReply::Handled(); }))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(8.f * U, 0.f, 0.f, 0.f)
					[
						SNew(SBox).MinDesiredWidth(84.f * U)
						[
							WRInv::TextButton(InArgs._ConfirmText, true, false, 12, 36.f * U, FOnClicked::CreateLambda([this]() { Confirm(); return FReply::Handled(); }))
						]
					]
				]
			]
		]
	];
}

void SWRInvAmountPicker::Confirm()
{
	OnConfirm.ExecuteIfBound(Value);
}

FReply SWRInvAmountPicker::OnKeyDown(const FGeometry& Geo, const FKeyEvent& Event)
{
	const FKey Key = Event.GetKey();
	if (Key == EKeys::Enter || Key == EKeys::SpaceBar) { Confirm(); return FReply::Handled(); }
	if (Key == EKeys::Escape) { OnCancel.ExecuteIfBound(); return FReply::Handled(); }
	if (Key == EKeys::Left || Key == EKeys::Down) { Value = FMath::Max(1, Value - 1); return FReply::Handled(); }
	if (Key == EKeys::Right || Key == EKeys::Up) { Value = FMath::Min(Max, Value + 1); return FReply::Handled(); }
	if (Key == EKeys::Home) { Value = 1; return FReply::Handled(); }
	if (Key == EKeys::End) { Value = Max; return FReply::Handled(); }
	return FReply::Unhandled();
}

FWRSlotView WRInv::StackView(const FWRItemStack& Stack)
{
	FWRSlotView V;
	if (Stack.IsEmpty()) return V;
	V.ItemId = Stack.ItemId;
	V.Count = Stack.Count;
	const FWRItemDef* Def = FWRItemRegistry::Get().Find(Stack.ItemId);
	if (Def && Def->MaxDurability > 0.f) V.Durability = FMath::Clamp(Stack.Durability / Def->MaxDurability, 0.f, 1.f);
	// Slot rims: green for fine, blue for masterwork (crude and common have none).
	if (Def && Def->IsEquipment()) V.Quality = Stack.Quality == EWRQuality::Masterwork ? 2 : Stack.Quality == EWRQuality::Fine ? 1 : 0;
	return V;
}

FString WRInv::Kg(float Kg)
{
	FString S = Kg > 0.f && Kg < 0.095f ? FString::Printf(TEXT("%.2f"), Kg) : FString::Printf(TEXT("%.1f"), Kg);
	if (S.EndsWith(TEXT(".0"))) S.LeftChopInline(2);
	return S;
}

FString WRInv::ItemName(FName ItemId)
{
	const FWRItemDef* Def = FWRItemRegistry::Get().Find(ItemId);
	return Def ? Def->Name : ItemId.ToString();
}

FString WRInv::Plural(const FString& Name, int32 Count)
{
	return Count == 1 || Name.EndsWith(TEXT("s")) ? Name : Name + TEXT("s");
}

TSharedRef<STextBlock> WRInv::Label(const TAttribute<FText>& Text, const FSlateFontInfo& Font, const TAttribute<FSlateColor>& Color, bool bWrap, ETextJustify::Type Justify)
{
	return SNew(STextBlock).Text(Text).Font(Font).ColorAndOpacity(Color).AutoWrapText(bWrap).Justification(Justify);
}

TSharedPtr<FWRItemDragDrop> WRInv::CurrentDrag()
{
	if (!FSlateApplication::IsInitialized() || !FSlateApplication::Get().IsDragDropping()) return nullptr;
	const TSharedPtr<FDragDropOperation> Op = FSlateApplication::Get().GetDragDroppingContent();
	return Op.IsValid() && Op->IsOfType<FWRItemDragDrop>() ? StaticCastSharedPtr<FWRItemDragDrop>(Op) : nullptr;
}

TSharedRef<SWidget> WRInv::IconButton(const FText& Text, const FWRInvIcon& Icon, bool bIconAfter, bool bPrimary, bool bGhost, int32 FontSize, float MinHeight, FOnClicked OnClicked)
{
	// SWRButton's look, with room for an icon; the hover rim needs the button, which exists only after SNew.
	const TSharedRef<TWeakPtr<SButton>> Self = MakeShared<TWeakPtr<SButton>>();
	const FLinearColor IconColor = bPrimary ? FLinearColor(FColor(236, 201, 138)) : bGhost ? WRUI::Brass : WRUI::Ink;
	const bool bHasIcon = Icon.Lines.Num() > 0 || Icon.Dots.Num() > 0;
	const TSharedRef<SWidget> IconWidget = SNew(SWRInvLineIcon).Icon(Icon).Size(16.f * U).Thickness(2.4f).Color(IconColor).Visibility(bHasIcon ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
	const TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
	if (!bIconAfter) Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, bHasIcon ? 9.f * U : 0.f, 0.f)[IconWidget];
	Row->AddSlot().AutoWidth().VAlign(VAlign_Center)
	[
		WRUI::Text(Text, WRUI::Title(FontSize), TAttribute<FSlateColor>::CreateLambda([Self]() { const TSharedPtr<SButton> B = Self->Pin(); return FSlateColor(!B.IsValid() || B->IsEnabled() ? WRUI::Ink : WRUI::Ink3); }), true)
	];
	if (bIconAfter) Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(bHasIcon ? 9.f * U : 0.f, 0.f, 0.f, 0.f)[IconWidget];
	const TSharedRef<SButton> Button = SNew(SButton).ButtonStyle(FCoreStyle::Get(), "NoBorder").ContentPadding(0.f).OnClicked(OnClicked).Cursor(EMouseCursor::Hand)
	[
		SNew(SBox).MinDesiredHeight(MinHeight)
		[
			SNew(SOverlay)
			// The leather tile mustn't size the button (its texture is 256 px); the label does.
			+ SOverlay::Slot()[SNew(SImage).Image(bGhost ? WRUI::White() : WRUI::Image(TEXT("leather"))).DesiredSizeOverride(FVector2D(1.f, 1.f))
				.ColorAndOpacity(bGhost ? FLinearColor(0.f, 0.f, 0.f, 0.32f) : FLinearColor::White)]
			+ SOverlay::Slot()[SNew(SImage).Image_Lambda([Self, bPrimary]()
			{
				const TSharedPtr<SButton> B = Self->Pin();
				const bool bHover = B.IsValid() && B->IsHovered() && B->IsEnabled();
				return WRUI::Box(bHover ? FLinearColor(1.f, 0.85f, 0.6f, 0.08f) : FLinearColor::Transparent, 4.f, bPrimary || bHover ? WRUI::Brass : WRUI::Brass2, bPrimary ? 1.5f : 1.f);
			})]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(FMargin(12.f * U, 6.f))[Row]
		]
	];
	*Self = Button;
	return Button;
}

TSharedRef<SWidget> WRInv::TextButton(const FText& Text, bool bPrimary, bool bGhost, int32 FontSize, float MinHeight, FOnClicked OnClicked, bool bEnabled)
{
	const TSharedRef<SWidget> Button = IconButton(Text, FWRInvIcon(), false, bPrimary && bEnabled, bGhost, FontSize, MinHeight, OnClicked);
	Button->SetEnabled(bEnabled);
	return SNew(SBorder).BorderImage(WRUI::Box(FLinearColor::Transparent)).Padding(0.f).ColorAndOpacity(bEnabled ? FLinearColor::White : FLinearColor(0.62f, 0.6f, 0.58f, 0.85f))[Button];
}

namespace WRInv
{
	namespace
	{
		TArray<FVector2f> InvLine(std::initializer_list<FVector2f> P) { return TArray<FVector2f>(P); }
		TArray<FVector2f> InvCircle(float CX, float CY, float R, int32 N = 20)
		{
			TArray<FVector2f> Pts;
			for (int32 i = 0; i <= N; ++i) Pts.Add(FVector2f(CX + R * FMath::Cos(2.f * PI * i / N), CY + R * FMath::Sin(2.f * PI * i / N)));
			return Pts;
		}
	}
	FWRInvIcon Icons::Sort() { FWRInvIcon I; I.Lines = { InvLine({ { 4, 6 }, { 20, 6 } }), InvLine({ { 4, 12 }, { 14, 12 } }), InvLine({ { 4, 18 }, { 9, 18 } }), InvLine({ { 18, 12 }, { 18, 20 } }), InvLine({ { 15.5f, 17.5f }, { 18, 20 }, { 20.5f, 17.5f } }) }; return I; }
	FWRInvIcon Icons::ArrowRight() { FWRInvIcon I; I.Lines = { InvLine({ { 4, 12 }, { 19, 12 } }), InvLine({ { 14, 6 }, { 20, 12 }, { 14, 18 } }) }; return I; }
	FWRInvIcon Icons::ArrowLeft() { FWRInvIcon I; I.Lines = { InvLine({ { 20, 12 }, { 5, 12 } }), InvLine({ { 10, 6 }, { 4, 12 }, { 10, 18 } }) }; return I; }
	FWRInvIcon Icons::Swap() { FWRInvIcon I; I.Lines = { InvLine({ { 3, 8 }, { 18, 8 }, { 14, 4 } }), InvLine({ { 21, 16 }, { 6, 16 }, { 10, 20 } }) }; return I; }
	FWRInvIcon Icons::Drag()
	{
		FWRInvIcon I;
		I.Lines = { InvLine({ { 12, 3 }, { 12, 21 } }), InvLine({ { 3, 12 }, { 21, 12 } }), InvLine({ { 9, 6 }, { 12, 3 }, { 15, 6 } }), InvLine({ { 9, 18 }, { 12, 21 }, { 15, 18 } }),
			InvLine({ { 6, 9 }, { 3, 12 }, { 6, 15 } }), InvLine({ { 18, 9 }, { 21, 12 }, { 18, 15 } }) };
		return I;
	}
	FWRInvIcon Icons::Flag() { FWRInvIcon I; I.Lines = { InvLine({ { 6, 3 }, { 6, 21 } }), InvLine({ { 6, 4.5f }, { 18, 4.5f }, { 15, 8.5f }, { 18, 12.5f }, { 6, 12.5f } }), InvLine({ { 7, 7 }, { 15, 7 } }), InvLine({ { 7, 10 }, { 15, 10 } }) }; return I; }
	FWRInvIcon Icons::Paw() { FWRInvIcon I; I.Dots = { { 6.5f, 9.f, 2.3f }, { 12.f, 6.f, 2.3f }, { 17.5f, 9.f, 2.3f }, { 12.f, 15.5f, 4.2f }, { 9.f, 17.5f, 2.8f }, { 15.f, 17.5f, 2.8f } }; return I; }
	FWRInvIcon Icons::Info() { FWRInvIcon I; I.Lines = { InvCircle(12, 12, 9), InvLine({ { 12, 11 }, { 12, 17 } }) }; I.Dots = { { 12.f, 7.5f, 1.4f } }; return I; }

	namespace
	{
		int32 InvCategoryRank(const FWRItemDef* Def)
		{
			static const TCHAR* Order[] = { TEXT("weapon"), TEXT("ranged"), TEXT("armor"), TEXT("tool"), TEXT("ammo"), TEXT("dart"), TEXT("potion"), TEXT("food"), TEXT("drink"),
				TEXT("tame_food"), TEXT("egg"), TEXT("gem"), TEXT("material"), TEXT("core"), TEXT("trophy") };
			for (int32 i = 0; Def && i < int32(UE_ARRAY_COUNT(Order)); ++i) if (Def->Category == Order[i]) return i;
			return int32(UE_ARRAY_COUNT(Order));
		}
		bool InvSameStack(const FWRItemStack& A, const FWRItemStack& B)
		{
			return A.ItemId == B.ItemId && A.Count == B.Count && A.Quality == B.Quality && A.Upgrade == B.Upgrade && A.Durability == B.Durability && A.Roll == B.Roll && A.Element == B.Element && A.Sockets == B.Sockets;
		}
		/** The server's "move a b" (WRPlayerMenus.cpp): merge into a matching stack with room, else swap. */
		void InvSimulateMove(TArray<FWRItemStack>& Slots, int32 A, int32 B)
		{
			if (!Slots.IsValidIndex(A) || !Slots.IsValidIndex(B) || A == B || Slots[A].IsEmpty()) return;
			const FWRItemDef* Def = FWRItemRegistry::Get().Find(Slots[A].ItemId);
			if (Def && !Slots[B].IsEmpty() && Slots[B].ItemId == Slots[A].ItemId && Def->StackSize > 1 && !Def->IsEquipment())
			{
				const int32 Moved = FMath::Min(Slots[A].Count, Def->StackSize - Slots[B].Count);
				Slots[B].Count += Moved;
				Slots[A].Count -= Moved;
				if (Slots[A].Count <= 0) Slots[A] = FWRItemStack();
			}
			else
			{
				Swap(Slots[A], Slots[B]);
			}
		}
	}

	void SortBag(AWRPlayerController* PC)
	{
		const AWRPlayerState* PS = PC ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
		const UWRInventoryComponent* Inv = PS ? PS->GetInventory() : nullptr;
		if (!Inv) return;
		// Work it out on a copy with the server's own move rules, then send the moves; they run in order there.
		TArray<FWRItemStack> Sim = Inv->GetSlots();
		TArray<FString> Moves;
		const auto Move = [&Sim, &Moves](int32 A, int32 B) { InvSimulateMove(Sim, A, B); Moves.Add(FString::Printf(TEXT("move %d %d"), A, B)); };
		// Part stacks of one item merge first.
		for (int32 i = 0; i < Sim.Num(); ++i)
		{
			const FWRItemDef* Def = Sim[i].IsEmpty() ? nullptr : FWRItemRegistry::Get().Find(Sim[i].ItemId);
			if (!Def || Def->StackSize <= 1 || Def->IsEquipment()) continue;
			for (int32 j = i + 1; j < Sim.Num() && Sim[i].Count < Def->StackSize; ++j)
			{
				if (!Sim[j].IsEmpty() && Sim[j].ItemId == Sim[i].ItemId) Move(j, i);
			}
		}
		TArray<FWRItemStack> Target;
		for (const FWRItemStack& S : Sim) if (!S.IsEmpty()) Target.Add(S);
		Target.StableSort([](const FWRItemStack& A, const FWRItemStack& B)
		{
			const FWRItemDef* DA = FWRItemRegistry::Get().Find(A.ItemId);
			const FWRItemDef* DB = FWRItemRegistry::Get().Find(B.ItemId);
			const int32 RA = InvCategoryRank(DA), RB = InvCategoryRank(DB);
			if (RA != RB) return RA < RB;
			const FString NA = DA ? DA->Name : A.ItemId.ToString(), NB = DB ? DB->Name : B.ItemId.ToString();
			if (NA != NB) return NA < NB;
			if (A.Quality != B.Quality) return A.Quality > B.Quality;
			if (A.Count != B.Count) return A.Count > B.Count;
			return A.Durability > B.Durability;
		});
		// Then each place in turn takes its stack (swaps; same-item stacks trade amounts, which comes to the same).
		for (int32 i = 0; i < Target.Num(); ++i)
		{
			if (InvSameStack(Sim[i], Target[i])) continue;
			for (int32 j = i + 1; j < Sim.Num(); ++j)
			{
				if (InvSameStack(Sim[j], Target[i])) { Move(j, i); break; }
			}
		}
		for (const FString& M : Moves) PC->SendCommand(M);
		PC->ShowLocalNotice(Moves.Num() > 0 ? TEXT("Bag sorted") : TEXT("Your bag is already in order"));
	}
}

// ---------------------------------------------------------------- the Inventory tab

namespace WRInvTab
{
	const EWREquipSlot EquipSlots[3] = { EWREquipSlot::Weapon, EWREquipSlot::Ranged, EWREquipSlot::Armor };
	const TCHAR* EquipNames[3] = { TEXT("weapon"), TEXT("ranged"), TEXT("armor") };

	int32 EquipIndexOf(EWREquipSlot Slot)
	{
		return Slot == EWREquipSlot::Weapon ? 0 : Slot == EWREquipSlot::Ranged ? 1 : Slot == EWREquipSlot::Armor ? 2 : INDEX_NONE;
	}

	enum class EFilter : uint8 { All, Gear, Resources, Food, Ammo, Taming };

	bool InFilter(const FWRItemDef* D, EFilter F)
	{
		if (!D || F == EFilter::All) return true;
		const FName C = D->Category;
		switch (F)
		{
		case EFilter::Gear: return C == TEXT("weapon") || C == TEXT("armor") || C == TEXT("ranged") || C == TEXT("tool");
		case EFilter::Resources: return C == TEXT("material") || C == TEXT("core") || C == TEXT("trophy") || C == TEXT("gem");
		case EFilter::Food: return C == TEXT("food") || C == TEXT("drink") || C == TEXT("potion");
		case EFilter::Ammo: return C == TEXT("ammo") || C == TEXT("dart") || D->IsThrowable();
		case EFilter::Taming: return C == TEXT("tame_food") || C == TEXT("egg") || C == TEXT("dart") || D->Id == TEXT("narcotic") || D->AmmoTorpor > 0.f;
		default: return true;
		}
	}

	FString StationName(FName Station)
	{
		if (Station.IsNone()) return FString();
		if (Station == TEXT("hand")) return TEXT("By hand");
		FString S = Station.ToString().Replace(TEXT("_"), TEXT(" "));
		S[0] = FChar::ToUpper(S[0]);
		return S;
	}

	FLinearColor QualityColor(EWRQuality Q)
	{
		return Q == EWRQuality::Fine ? WRUI::Good : Q == EWRQuality::Masterwork ? WRUI::Frost : Q == EWRQuality::Crude ? WRUI::Ink3 : WRUI::Ink2;
	}

	float QualityMult(EWRQuality Q)
	{
		return FWRTuning::Get().Num(FString::Printf(TEXT("weapon.quality.%s"), *FString(FWRItemStack::QualityName(Q)).ToLower()), 1.f);
	}

	float GemSum(const FWRItemStack& S, FName Stat)
	{
		float Total = 0.f;
		for (const FName& Gem : S.Sockets) if (const FWRItemDef* G = FWRItemRegistry::Get().Find(Gem)) Total += G->GemStats.FindRef(Stat);
		return Total;
	}

	/** The sword table's numbers for a weapon stack, as UWRInventoryComponent::ApplyEquipment builds them. */
	FWRWeaponStats WeaponStats(const FWRItemStack* S, const FWRItemDef* D)
	{
		FWRWeaponStats Stats;
		if (!S || !D || S->IsEmpty())
		{
			Stats.Quality = TEXT("crude");   // bare hands
			Stats.Roll = 0.5f;
			return Stats;
		}
		Stats.Metal = D->Metal;
		Stats.Quality = FName(*FString(FWRItemStack::QualityName(S->Quality)).ToLower());
		Stats.Upgrade = S->Upgrade;
		Stats.Roll = S->Roll;
		Stats.GemDamage = GemSum(*S, TEXT("damage"));
		Stats.bBroken = S->Durability <= 0.f;
		return Stats;
	}

	float ArmorOf(const FWRItemStack& S, const FWRItemDef& D)
	{
		if (S.Durability <= 0.f) return 0.f;
		return D.Armor * QualityMult(S.Quality) * S.Roll * (1.f + S.Upgrade * WRTune(TEXT("weapon.upgradePerLevel"), 0.05f));
	}

	FString AmmoWord(FName Kind, bool bPlural)
	{
		const FString K = Kind.IsNone() ? FString(TEXT("ammo")) : Kind.ToString();
		return bPlural && K != TEXT("ammo") ? K + TEXT("s") : K;
	}

	FString LauncherOf(FName AmmoKind)
	{
		return AmmoKind == TEXT("arrow") ? TEXT("bows") : AmmoKind == TEXT("bolt") ? TEXT("crossbows") : AmmoKind == TEXT("dart") ? TEXT("blowguns") : TEXT("ranged weapons");
	}

	FString KindLine(const FWRItemDef& D)
	{
		const FName C = D.Category;
		if (D.IsThrowable()) return TEXT("Thrown weapon · held in the ranged slot");
		if (D.IsRanged()) return FString::Printf(TEXT("Ranged weapon · shoots %s"), *AmmoWord(D.AmmoKind, true));
		if (D.Slot == EWREquipSlot::Weapon) return D.Metal.IsNone() ? FString(TEXT("Melee weapon")) : FString::Printf(TEXT("Melee weapon · %s"), *D.Metal.ToString());
		if (D.Slot == EWREquipSlot::Armor) return TEXT("Armor · worn on the body");
		if (C == TEXT("tool")) return TEXT("Tool · works from your bag");
		if (C == TEXT("ammo")) return FString::Printf(TEXT("Ammo · for %s"), *LauncherOf(D.AmmoOf));
		if (C == TEXT("dart")) return TEXT("Dart · blowgun ammo, or use it by hand");
		if (C == TEXT("potion")) return TEXT("Potion · drink it");
		if (C == TEXT("food")) return TEXT("Food · eat it");
		if (C == TEXT("drink")) return TEXT("Drink");
		if (C == TEXT("tame_food")) return TEXT("Taming food");
		if (C == TEXT("egg")) return TEXT("Creature egg · hatch it");
		if (C == TEXT("gem")) return TEXT("Gem · goes in a weapon socket");
		if (C == TEXT("core")) return TEXT("Boss core · for your Hearth");
		if (C == TEXT("trophy")) return TEXT("Trophy");
		return TEXT("Resource · for crafting and building");
	}

	FString CategoryWord(const FWRItemDef& D)
	{
		const FName C = D.Category;
		return C == TEXT("material") ? TEXT("Resource") : C == TEXT("tool") ? TEXT("Tool") : C == TEXT("ammo") ? TEXT("Ammo") : C == TEXT("dart") ? TEXT("Dart")
			: C == TEXT("potion") ? TEXT("Potion") : C == TEXT("food") ? TEXT("Food") : C == TEXT("drink") ? TEXT("Drink") : C == TEXT("tame_food") ? TEXT("Taming")
			: C == TEXT("egg") ? TEXT("Egg") : C == TEXT("gem") ? TEXT("Gem") : C == TEXT("core") ? TEXT("Boss core") : C == TEXT("trophy") ? TEXT("Trophy") : TEXT("Item");
	}

	/** One line of advice the game's rules back up (the details panel's tinted box); bTaming picks the paw icon. */
	FString HintFor(const FWRItemDef& D, bool& bTaming)
	{
		bTaming = false;
		const FName C = D.Category;
		if (D.IsRanged() && !D.IsThrowable())
		{
			// The ammo of its kind that knocks creatures out, if there is one.
			for (const FName Id : { FName(TEXT("tranq_arrow")), FName(TEXT("tranq_bolt")), FName(TEXT("torpor_dart")) })
			{
				const FWRItemDef* A = FWRItemRegistry::Get().Find(Id);
				if (A && A->AmmoOf == D.AmmoKind && A->AmmoTorpor > 0.f)
				{
					bTaming = true;
					return FString::Printf(TEXT("%s knock creatures out for taming."), *WRInv::Plural(A->Name, 2));
				}
			}
			return FString::Printf(TEXT("Fires %s from your bag."), *AmmoWord(D.AmmoKind, true));
		}
		if (D.IsThrowable()) return TEXT("Thrown from the ranged slot; the next one in your bag takes its place.");
		if (D.AmmoTorpor > 0.f || C == TEXT("dart")) { bTaming = true; return TEXT("Knocks creatures out for taming (torpor)."); }
		if (C == TEXT("ammo")) return FString::Printf(TEXT("Carried in your bag: %s fire it."), *LauncherOf(D.AmmoOf));
		if (C == TEXT("gem")) return TEXT("Right-click to socket it into your weapon. Its quality sets the sockets.");
		if (C == TEXT("egg")) { bTaming = true; return TEXT("Right-click to hatch it: the newborn imprints on you."); }
		if (C == TEXT("tame_food")) { bTaming = true; return TEXT("Every tameable creature eats it; it tames twice as fast."); }
		if (D.Id == TEXT("narcotic")) { bTaming = true; return TEXT("Feed it to a knocked-out creature to keep it under."); }
		if (C == TEXT("tool")) return TEXT("Tools work from your bag: gather and the best one is used.");
		if (C == TEXT("potion")) return TEXT("Potions share a belt: one, then a cooldown.");
		if (C == TEXT("core")) return TEXT("Bring it to your Hearth to raise its tech level.");
		if (D.Slot == EWREquipSlot::Weapon) return TEXT("Gems socket into it; upgrade shards raise its damage.");
		if (D.Slot == EWREquipSlot::Armor) return TEXT("Wears down as you're hit; repair it before it breaks.");
		return FString();
	}

	FString GemLine(FName Stat, float Amount)
	{
		if (Stat == TEXT("damage")) return FString::Printf(TEXT("+%.0f%% damage"), Amount * 100.f);
		if (Stat == TEXT("critChance")) return FString::Printf(TEXT("+%.0f%% critical chance"), Amount * 100.f);
		if (Stat == TEXT("poise")) return FString::Printf(TEXT("+%.0f poise"), Amount);
		if (Stat == TEXT("staggerShockwave")) return TEXT("Staggering a foe sends out lava");
		if (Stat == TEXT("blockFreeze")) return TEXT("Blocks can freeze the attacker");
		return FString::Printf(TEXT("%s %+.2f"), *Stat.ToString(), Amount);
	}

	/** What recipes (by hand and at every station pieces.json has) take this item. */
	TArray<FString> UsedToMake(FName Item)
	{
		TArray<FName> Stations = { FName(TEXT("hand")) };
		for (const FWRPieceDef& P : FWRBuildRegistry::Get().All()) if (!P.Station.IsNone()) Stations.AddUnique(P.Station);
		TArray<FString> Out;
		for (const FName& St : Stations)
		{
			for (const FWRRecipe* R : FWRRecipeRegistry::Get().ForStation(St))
			{
				for (const TPair<FName, int32>& In : R->Inputs) if (In.Key == Item) Out.AddUnique(WRInv::ItemName(R->Output));
			}
		}
		return Out;
	}

	FString ShortAmmoName(const FWRItemDef& D)
	{
		FString Name = D.Name;
		const FString Kind = D.AmmoOf.ToString();
		if (Name.EndsWith(TEXT(" ") + Kind, ESearchCase::IgnoreCase)) Name.LeftChopInline(Kind.Len() + 1);
		return Name.IsEmpty() ? D.Name : Name;
	}

	/** The faint body in the equipment column (the mockup's figure). */
	class SWRInvFigure : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SWRInvFigure) {}
		SLATE_END_ARGS()
		void Construct(const FArguments&) { SetVisibility(EVisibility::HitTestInvisible); }
		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(130.f * U, 282.f * U); }
		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override
		{
			// Drawn on the mockup's 120 x 260 grid.
			const FVector2D Size = Geo.GetLocalSize();
			const float S = FMath::Min(Size.X / 120.f, Size.Y / 260.f);
			const FVector2f O((Size.X - 120.f * S) * 0.5f, 0.f);
			const auto P = [&](float X, float Y) { return O + FVector2f(X, Y) * S; };
			const FLinearColor Fill(0.93f, 0.89f, 0.8f, 0.045f);
			const auto Fillet = [&](float X, float Y, float W, float H, float R)
			{
				WRUI::DrawBox(Out, Layer, Geo.ToPaintGeometry(FVector2D(W * S, H * S), FSlateLayoutTransform(FVector2D(P(X, Y)))), WRUI::Box(Fill, R * S));
			};
			Fillet(45.f, 7.f, 30.f, 30.f, 15.f);       // head
			Fillet(28.f, 42.f, 64.f, 98.f, 12.f);      // body
			Fillet(16.f, 52.f, 16.f, 73.f, 7.f);       // arms
			Fillet(88.f, 52.f, 16.f, 73.f, 7.f);
			Fillet(34.f, 120.f, 22.f, 132.f, 7.f);     // legs
			Fillet(64.f, 120.f, 22.f, 132.f, 7.f);

			struct FSeg { bool bQuad; float CX, CY, X, Y; };
			static const FSeg Path[] = {
				{ true, 60, 38, 80, 44 }, { false, 0, 0, 92, 50 }, { true, 99, 54, 100, 64 }, { false, 0, 0, 104, 116 }, { true, 105, 124, 99, 125 },
				{ false, 0, 0, 96, 125 }, { true, 91, 124, 90, 116 }, { false, 0, 0, 85, 74 }, { false, 0, 0, 84, 124 }, { false, 0, 0, 87, 244 },
				{ true, 87, 252, 80, 252 }, { false, 0, 0, 72, 252 }, { true, 66, 252, 66, 244 }, { false, 0, 0, 61, 140 }, { false, 0, 0, 59, 140 },
				{ false, 0, 0, 54, 244 }, { true, 54, 252, 48, 252 }, { false, 0, 0, 40, 252 }, { true, 33, 252, 33, 244 }, { false, 0, 0, 36, 124 },
				{ false, 0, 0, 35, 74 }, { false, 0, 0, 30, 116 }, { true, 29, 124, 24, 125 }, { false, 0, 0, 21, 125 }, { true, 15, 124, 16, 116 },
				{ false, 0, 0, 20, 64 }, { true, 21, 54, 28, 50 }, { false, 0, 0, 40, 44 } };
			TArray<FVector2f> Outline = { P(40, 44) };
			FVector2f Last(40, 44);
			for (const FSeg& G : Path)
			{
				if (G.bQuad)
				{
					for (int32 i = 1; i <= 6; ++i)
					{
						const float T = i / 6.f;
						const FVector2f Q = Last * FMath::Square(1.f - T) + FVector2f(G.CX, G.CY) * (2.f * (1.f - T) * T) + FVector2f(G.X, G.Y) * (T * T);
						Outline.Add(P(Q.X, Q.Y));
					}
				}
				else
				{
					Outline.Add(P(G.X, G.Y));
				}
				Last = FVector2f(G.X, G.Y);
			}
			const FLinearColor Line = WRUI::Alpha(WRUI::Brass, 0.3f);
			FSlateDrawElement::MakeLines(Out, Layer + 1, Geo.ToPaintGeometry(), Outline, ESlateDrawEffect::None, Line, true, 1.1f * S);
			TArray<FVector2f> Head;
			for (int32 i = 0; i <= 28; ++i) Head.Add(P(60.f + 15.f * FMath::Cos(2.f * PI * i / 28), 22.f + 15.f * FMath::Sin(2.f * PI * i / 28)));
			FSlateDrawElement::MakeLines(Out, Layer + 1, Geo.ToPaintGeometry(), Head, ESlateDrawEffect::None, Line, true, 1.1f * S);
			TArray<FVector2f> Plate = { P(44, 60), P(52, 57.5f), P(60, 57), P(68, 57.5f), P(76, 60), P(74, 104), P(67, 106.5f), P(60, 107), P(53, 106.5f), P(46, 104), P(44, 60) };
			FSlateDrawElement::MakeLines(Out, Layer + 1, Geo.ToPaintGeometry(), Plate, ESlateDrawEffect::None, WRUI::Alpha(WRUI::Brass, 0.18f), true, 0.8f * S);
			return Layer + 1;
		}
	};

	class SWRInventoryTab : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SWRInventoryTab) {}
			SLATE_ARGUMENT(TWeakObjectPtr<AWRPlayerController>, PC)
		SLATE_END_ARGS()

		/** The open tab (the testing hooks below reach it through this). */
		static TWeakPtr<SWRInventoryTab> Current;

		void Construct(const FArguments& InArgs)
		{
			PC = InArgs._PC;
			ChildSlot
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SWRInvDropZone).OnDropped(this, &SWRInventoryTab::DroppedOnTab)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().FillHeight(1.f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(320.f * U)[SNew(SWRInvDropZone).OnDropped(this, &SWRInventoryTab::DroppedOnEquipment)[EquipmentPanel()]]]
							+ SHorizontalBox::Slot().FillWidth(1.f).Padding(22.f * U, 0.f)[BagPanel()]
							+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(340.f * U)[SNew(SWRPanel).Kind(EWRPanel::Inset).Padding(FMargin(18.f * U))[SAssignNew(DetailsBox, SBox)]]]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 16.f * U, 0.f, 0.f)[HotbarStrip()]
					]
				]
				+ SOverlay::Slot()[SAssignNew(PickerBox, SBox).Visibility(EVisibility::Collapsed)]
			];
		}

		virtual void Tick(const FGeometry& Geo, const double Time, const float Delta) override
		{
			SCompoundWidget::Tick(Geo, Time, Delta);
			const UWRInventoryComponent* I = Inv();
			if (!I) return;
			if (SelKind == EWRSlotKind::Other)
			{
				// Open on something: the first thing in the bag, else what's in your hand.
				const int32 First = I->GetSlots().IndexOfByPredicate([](const FWRItemStack& S) { return !S.IsEmpty(); });
				if (First != INDEX_NONE) Select(EWRSlotKind::Bag, First);
				else if (!I->GetEquipped(EWREquipSlot::Weapon).IsEmpty()) Select(EWRSlotKind::Equipment, 0);
			}
			const uint32 Hash = StateHash();
			if (!bDetailsBuilt || Hash != DetailsHash)
			{
				DetailsHash = Hash;
				bDetailsBuilt = true;
				DetailsBox->SetContent(BuildDetails());
				// The stack being split or dropped changed under the picker: close it.
				if (PickMode != EPick::None && (!I->GetSlots().IsValidIndex(PickSlot) || I->GetSlots()[PickSlot].ItemId != PickItem || I->GetSlots()[PickSlot].Count < PickMin)) ClosePicker();
			}
		}

		void Select(EWRSlotKind Kind, int32 Index)
		{
			SelKind = Kind;
			SelIndex = Index;
		}

		/** Testing / screenshots: open the split or drop picker on a bag slot. */
		void DebugPicker(bool bSplit, int32 Slot) { OpenPicker(bSplit ? EPick::Split : EPick::Drop, Slot); }

	private:
		enum class EPick : uint8 { None, Split, Drop };

		TWeakObjectPtr<AWRPlayerController> PC;
		EWRSlotKind SelKind = EWRSlotKind::Other;
		int32 SelIndex = INDEX_NONE;
		EFilter Filter = EFilter::All;
		TSharedPtr<SBox> DetailsBox;
		TSharedPtr<SBox> PickerBox;
		uint32 DetailsHash = 0;
		bool bDetailsBuilt = false;
		TSharedPtr<SWidget> KeyWidgets[8];
		EPick PickMode = EPick::None;
		int32 PickSlot = INDEX_NONE;
		FName PickItem;
		int32 PickMin = 1;

		// ------------------------------------------------------------ state
		const AWRPlayerState* PS() const { return PC.IsValid() ? PC->GetPlayerState<AWRPlayerState>() : nullptr; }
		UWRInventoryComponent* Inv() const { const AWRPlayerState* P = PS(); return P ? P->GetInventory() : nullptr; }
		AWRCharacter* Survivor() const { return PC.IsValid() ? Cast<AWRCharacter>(PC->GetPawn()) : nullptr; }
		FWRPlayerUI* UI() const { return PC.IsValid() ? PC->GetUI() : nullptr; }
		void Send(const FString& Command) const { if (PC.IsValid()) PC->SendCommand(Command); }
		void Notice(const FString& Text) const { if (PC.IsValid()) PC->ShowLocalNotice(Text); }

		const FWRItemStack* StackAt(EWRSlotKind Kind, int32 Index) const
		{
			const UWRInventoryComponent* I = Inv();
			if (!I) return nullptr;
			if (Kind == EWRSlotKind::Bag) return I->GetSlots().IsValidIndex(Index) ? &I->GetSlots()[Index] : nullptr;
			if (Kind == EWRSlotKind::Equipment && Index >= 0 && Index < 3) return &I->GetEquipped(EquipSlots[Index]);
			return nullptr;
		}
		static const FWRItemDef* DefOf(const FWRItemStack* S) { return S && !S->IsEmpty() ? FWRItemRegistry::Get().Find(S->ItemId) : nullptr; }

		int32 UsedSlots() const
		{
			const UWRInventoryComponent* I = Inv();
			int32 N = 0;
			if (I) for (const FWRItemStack& S : I->GetSlots()) N += !S.IsEmpty();
			return N;
		}
		int32 FirstEmpty() const
		{
			const UWRInventoryComponent* I = Inv();
			return I ? I->GetSlots().IndexOfByPredicate([](const FWRItemStack& S) { return S.IsEmpty(); }) : INDEX_NONE;
		}
		float CarryCap() const { return WRTune(TEXT("inventory.carryKg"), 80.f); }
		float Carry() const { const UWRInventoryComponent* I = Inv(); return I ? I->GetCarryWeight() : 0.f; }

		/** Everything the details panel shows depends on: rebuilt when it changes. */
		uint32 StateHash() const
		{
			const UWRInventoryComponent* I = Inv();
			uint32 H = HashCombine(GetTypeHash(static_cast<uint8>(SelKind)), GetTypeHash(SelIndex));
			const auto Mix = [&H](const FWRItemStack& S)
			{
				H = HashCombine(H, GetTypeHash(S.ItemId));
				H = HashCombine(H, GetTypeHash(S.Count));
				H = HashCombine(H, GetTypeHash(FMath::RoundToInt(S.Durability)));
				H = HashCombine(H, GetTypeHash(static_cast<uint8>(S.Quality) * 100 + S.Upgrade * 7 + S.Sockets.Num()));
				H = HashCombine(H, GetTypeHash(S.Element));
			};
			for (const FWRItemStack& S : I->GetSlots()) Mix(S);
			for (const EWREquipSlot E : EquipSlots) Mix(I->GetEquipped(E));
			if (const AWRCharacter* S = Survivor()) H = HashCombine(H, GetTypeHash(S->GetSelectedAmmo()));
			return H;
		}

		FWRSlotView BagView(int32 i) const
		{
			const FWRItemStack* S = StackAt(EWRSlotKind::Bag, i);
			if (!S) return FWRSlotView();
			FWRSlotView V = WRInv::StackView(*S);
			V.bSelected = SelKind == EWRSlotKind::Bag && SelIndex == i && !V.ItemId.IsNone();
			if (!V.ItemId.IsNone() && Filter != EFilter::All) V.bDimmed = !InFilter(DefOf(S), Filter);
			return V;
		}

		FWRSlotView EquipView(int32 e) const
		{
			const FWRItemStack* S = StackAt(EWRSlotKind::Equipment, e);
			if (!S) return FWRSlotView();
			FWRSlotView V = WRInv::StackView(*S);
			V.bSelected = SelKind == EWRSlotKind::Equipment && SelIndex == e && !V.ItemId.IsNone();
			return V;
		}

		/** What the worn bow / crossbow / blowgun fires next (the preferred ammo while it lasts). */
		FName LoadedAmmo() const
		{
			const UWRInventoryComponent* I = Inv();
			const FWRItemDef* Bow = DefOf(StackAt(EWRSlotKind::Equipment, 1));
			if (!I || !Bow || Bow->AmmoKind.IsNone()) return NAME_None;
			const AWRCharacter* S = Survivor();
			return I->FindAmmo(Bow->AmmoKind, S ? S->GetSelectedAmmo() : NAME_None);
		}

		// ------------------------------------------------------------ actions
		void UseSlot(int32 i) const
		{
			const FWRItemStack* S = StackAt(EWRSlotKind::Bag, i);
			if (S && !S->IsEmpty()) Send(FString::Printf(TEXT("slot %d"), i));
		}

		void Unequip(int32 e) const
		{
			const FWRItemStack* S = StackAt(EWRSlotKind::Equipment, e);
			if (!S || S->IsEmpty()) return;
			if (FirstEmpty() == INDEX_NONE) { Notice(TEXT("Your bag is full")); return; }
			Send(FString::Printf(TEXT("unequip %s"), EquipNames[e]));
		}

		/** A worn item dragged onto bag slot b: it lands there (what was there moves to the free slot it came off into). */
		void UnequipTo(int32 e, int32 b) const
		{
			const FWRItemStack* Worn = StackAt(EWRSlotKind::Equipment, e);
			const FWRItemStack* Target = StackAt(EWRSlotKind::Bag, b);
			if (!Worn || Worn->IsEmpty() || !Target) return;
			const FWRItemDef* TargetDef = DefOf(Target);
			if (TargetDef && TargetDef->Slot == EquipSlots[e])
			{
				Send(FString::Printf(TEXT("slot %d"), b));   // the same kind of gear: wear that one instead (a swap)
				return;
			}
			const int32 Free = FirstEmpty();
			if (Free == INDEX_NONE) { Notice(TEXT("Your bag is full")); return; }
			Send(FString::Printf(TEXT("unequip %s"), EquipNames[e]));
			if (Free != b) Send(FString::Printf(TEXT("move %d %d"), Free, b));
		}

		void EquipFromBag(int32 i, int32 IntoSlot) const
		{
			const FWRItemDef* Def = DefOf(StackAt(EWRSlotKind::Bag, i));
			if (!Def) return;
			if (!Def->IsEquipment()) { Notice(FString::Printf(TEXT("%s can't be worn or held"), *Def->Name)); return; }
			if (IntoSlot != INDEX_NONE && EquipSlots[IntoSlot] != Def->Slot)
			{
				Notice(FString::Printf(TEXT("%s goes in the %s slot"), *Def->Name, EquipNames[EquipIndexOf(Def->Slot)]));
				return;
			}
			Send(FString::Printf(TEXT("slot %d"), i));
		}

		void SetKey(int32 Key, FName Item) const { if (FWRPlayerUI* X = UI()) X->SetHotbarItem(Key, Item); }

		void DroppedOnBagSlot(EWRSlotKind From, int32 FromIndex, int32 i)
		{
			if (From == EWRSlotKind::Bag && FromIndex != i)
			{
				Send(FString::Printf(TEXT("move %d %d"), FromIndex, i));
				if (SelKind == EWRSlotKind::Bag && SelIndex == FromIndex) SelIndex = i;
				else if (SelKind == EWRSlotKind::Bag && SelIndex == i) SelIndex = FromIndex;
			}
			else if (From == EWRSlotKind::Equipment) UnequipTo(FromIndex, i);
			else if (From == EWRSlotKind::Hotbar) SetKey(FromIndex, NAME_None);
		}

		void DroppedOnEquipSlot(EWRSlotKind From, int32 FromIndex, int32 e)
		{
			if (From == EWRSlotKind::Bag) EquipFromBag(FromIndex, e);
		}

		void DroppedOnKey(EWRSlotKind From, int32 FromIndex, int32 k)
		{
			const FWRItemStack* S = From == EWRSlotKind::Bag || From == EWRSlotKind::Equipment ? StackAt(From, FromIndex) : nullptr;
			if (S && !S->IsEmpty())
			{
				SetKey(k, S->ItemId);
			}
			else if (From == EWRSlotKind::Hotbar && FromIndex != k && UI())
			{
				// Two keys trade items.
				const FName Moving = UI()->GetHotbarItem(FromIndex), There = UI()->GetHotbarItem(k);
				SetKey(k, Moving);
				if (!There.IsNone()) SetKey(FromIndex, There);
			}
		}

		void DroppedOnTab(EWRSlotKind From, int32 FromIndex)
		{
			// Off the bar (anywhere but on a key or the strip): the key is cleared.
			if (From == EWRSlotKind::Hotbar) SetKey(FromIndex, NAME_None);
		}

		void DroppedOnEquipment(EWRSlotKind From, int32 FromIndex)
		{
			if (From == EWRSlotKind::Bag) EquipFromBag(FromIndex, INDEX_NONE);
			else if (From == EWRSlotKind::Hotbar) SetKey(FromIndex, NAME_None);
		}

		void DroppedOnBag(EWRSlotKind From, int32 FromIndex)
		{
			if (From == EWRSlotKind::Equipment) Unequip(FromIndex);
			else if (From == EWRSlotKind::Hotbar) SetKey(FromIndex, NAME_None);
		}

		void ClickedBagSlot(const FPointerEvent& Event, int32 i)
		{
			const FWRItemStack* S = StackAt(EWRSlotKind::Bag, i);
			if (!S || S->IsEmpty()) return;
			if (Event.GetEffectingButton() == EKeys::RightMouseButton) UseSlot(i);
			else if (Event.GetEffectingButton() == EKeys::LeftMouseButton) Select(EWRSlotKind::Bag, i);
		}

		void ClickedEquipSlot(const FPointerEvent& Event, int32 e)
		{
			const FWRItemStack* S = StackAt(EWRSlotKind::Equipment, e);
			if (!S || S->IsEmpty()) return;
			if (Event.GetEffectingButton() == EKeys::RightMouseButton) Unequip(e);
			else if (Event.GetEffectingButton() == EKeys::LeftMouseButton) Select(EWRSlotKind::Equipment, e);
		}

		void ClickedKey(const FPointerEvent& Event, int32 k)
		{
			FWRPlayerUI* X = UI();
			const UWRInventoryComponent* I = Inv();
			if (!X || !I) return;
			if (Event.GetEffectingButton() == EKeys::RightMouseButton) { SetKey(k, NAME_None); return; }
			// A left click shows the key's item in the details (worn first, else its first stack in the bag).
			const FName Id = X->GetHotbarItem(k);
			if (Id.IsNone()) return;
			for (int32 e = 0; e < 3; ++e) if (I->GetEquipped(EquipSlots[e]).ItemId == Id && !I->GetEquipped(EquipSlots[e]).IsEmpty()) { Select(EWRSlotKind::Equipment, e); return; }
			const int32 At = I->GetSlots().IndexOfByPredicate([Id](const FWRItemStack& S) { return !S.IsEmpty() && S.ItemId == Id; });
			if (At != INDEX_NONE) Select(EWRSlotKind::Bag, At);
		}

		void OpenPicker(EPick Mode, int32 Slot)
		{
			const FWRItemStack* S = StackAt(EWRSlotKind::Bag, Slot);
			const FWRItemDef* Def = DefOf(S);
			if (!Def) return;
			const int32 Max = Mode == EPick::Split ? S->Count - 1 : S->Count;
			if (Max < 1) return;
			PickMode = Mode;
			PickSlot = Slot;
			PickItem = S->ItemId;
			PickMin = Mode == EPick::Split ? 2 : 1;
			const FString Name = Def->Name;
			const float Weight = Def->Weight;
			const int32 Count = S->Count;
			const bool bSplit = Mode == EPick::Split;
			TSharedRef<SWRInvAmountPicker> Picker = SNew(SWRInvAmountPicker)
				.ItemId(S->ItemId).Max(Max).Initial(bSplit ? FMath::Max(1, Count / 2) : Max)
				.Title(bSplit ? LOCTEXT("SplitHowMany", "Split how many?") : LOCTEXT("DropHowMany", "Drop how many?"))
				.ConfirmText(bSplit ? LOCTEXT("SplitGo", "Split") : LOCTEXT("DropGo", "Drop"))
				.SubLine([Name, Weight, bSplit](int32 N) { return FText::FromString(bSplit ? FString::Printf(TEXT("%s into a free slot · %s kg"), *Name, *WRInv::Kg(N * Weight)) : FString::Printf(TEXT("%s on the ground · %s kg"), *Name, *WRInv::Kg(N * Weight))); })
				.RestLine([Count, bSplit](int32 N) { return FText::FromString(bSplit ? FString::Printf(TEXT("%d stay in this stack"), Count - N) : FString::Printf(TEXT("%d stay in your bag"), Count - N)); })
				.OnConfirm_Lambda([this, bSplit, Slot](int32 N) { Send(bSplit ? FString::Printf(TEXT("split %d %d"), Slot, N) : FString::Printf(TEXT("drop %d %d"), Slot, N)); ClosePicker(); })
				.OnCancel_Lambda([this]() { ClosePicker(); });
			PickerBox->SetContent(
				SNew(SOverlay)
				// A click beside the picker cancels it.
				+ SOverlay::Slot()[SNew(SBorder).BorderImage(WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.18f))).OnMouseButtonDown_Lambda([this](const FGeometry&, const FPointerEvent&) { ClosePicker(); return FReply::Handled(); })]
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0.f, 0.f, 0.f, 100.f * U)[Picker]);
			PickerBox->SetVisibility(EVisibility::Visible);
			FSlateApplication::Get().SetKeyboardFocus(Picker, EFocusCause::SetDirectly);
		}

		void ClosePicker()
		{
			PickMode = EPick::None;
			PickSlot = INDEX_NONE;
			if (PickerBox.IsValid())
			{
				PickerBox->SetVisibility(EVisibility::Collapsed);
				PickerBox->SetContent(SNullWidget::NullWidget);
			}
		}

		// ------------------------------------------------------------ equipment column
		TSharedRef<SWidget> EquipmentPanel()
		{
			return SNew(SWRPanel).Kind(EWRPanel::Inset).Padding(FMargin(16.f * U, 16.f * U, 16.f * U, 18.f * U))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Bottom)[WRUI::Text(LOCTEXT("Equipment", "Equipment"), WRUI::Title(16), WRUI::Ink)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(0.f, 0.f, 0.f, 2.f)[WRUI::Text(LOCTEXT("WornHeld", "Worn and held"), WRUI::Caps(11), WRUI::Ink2)]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f * U, 0.f, 0.f)
				[
					SNew(SBox).HeightOverride(282.f * U)
					[
						SNew(SOverlay)
						+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top)[SNew(SWRInvFigure)]
						+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(0.f, 8.f * U, 0.f, 0.f)[EquipBlock(0, LOCTEXT("SlotWeapon", "Weapon"))]
						+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(0.f, 160.f * U, 0.f, 0.f)[EquipBlock(2, LOCTEXT("SlotArmor", "Armor"))]
						+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(0.f, 8.f * U, 0.f, 0.f)[EquipBlock(1, LOCTEXT("SlotRanged", "Ranged"))]
						+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(0.f, 168.f * U, 0.f, 0.f)[LoadedBlock()]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f * U)[WRUI::Divider()]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f)[StatTile(TEXT("copper_sword"), [this]() { return FText::AsNumber(FMath::RoundToInt(DamageNow())); }, LOCTEXT("TileDamage", "Damage"), []() { return WRUI::Ink; })]
						+ SHorizontalBox::Slot().FillWidth(1.f).Padding(8.f * U, 0.f, 0.f, 0.f)[StatTile(TEXT("hide_armor"), [this]() { const UWRInventoryComponent* I = Inv(); return FText::AsNumber(I ? FMath::RoundToInt(I->GetArmorValue()) : 0); }, LOCTEXT("TileArmor", "Armor"), []() { return WRUI::Ink; })]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f * U, 0.f, 0.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f)[StatTile(TEXT("ember_essence"), [this]() { return Resists(TEXT("lava")) ? LOCTEXT("LavaSafe", "Safe") : LOCTEXT("LavaBurns", "Burns"); }, LOCTEXT("TileLava", "Lava"),
							[this]() { return Resists(TEXT("lava")) ? WRUI::Good : WRUI::Ink; })]
						+ SHorizontalBox::Slot().FillWidth(1.f).Padding(8.f * U, 0.f, 0.f, 0.f)[StatTile(TEXT("frost_essence"), [this]() { return Resists(TEXT("ice")) ? LOCTEXT("IceGrip", "Grip") : LOCTEXT("IceSlips", "Slips"); }, LOCTEXT("TileIce", "Ice"),
							[this]() { return Resists(TEXT("ice")) ? WRUI::Good : WRUI::Ink; })]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f * U, 0.f, 0.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Bottom)[WRUI::Text(LOCTEXT("CarryWeight", "Carry weight"), WRUI::Caps(12), WRUI::Ink2)]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(WRInv::Kg(Carry())); }), WRUI::Body(14, 800),
							TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(Carry() > CarryCap() ? WRUI::Bad : WRUI::Ink); }))]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(4.f, 0.f, 0.f, 0.f)[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(FString::Printf(TEXT("/ %s kg"), *WRInv::Kg(CarryCap()))); }), WRUI::Body(14, 500), WRUI::Ink3)]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 0.f)
					[
						SNew(SWRBar).Height(14.f * U).Fraction_Lambda([this]() { return Carry() / FMath::Max(1.f, CarryCap()); })
						.Color_Lambda([this]() { return Carry() > CarryCap() ? WRUI::Bad : FLinearColor(FColor(181, 138, 74)); })
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 0.f)
					[
						WRUI::Text(TAttribute<FText>::CreateLambda([this]()
						{
							const float Speed = WRTune(TEXT("inventory.overweightSpeed"), 0.7f) * 100.f;
							return Carry() > CarryCap() ? FText::FromString(FString::Printf(TEXT("Overloaded: you move at %.0f%% speed"), Speed))
								: FText::FromString(FString::Printf(TEXT("Over %s kg you move slower"), *WRInv::Kg(CarryCap())));
						}), WRUI::Body(12, 500), TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(Carry() > CarryCap() ? WRUI::Bad : WRUI::Ink3); }))
					]
				]
			];
		}

		bool Resists(FName Kind) const
		{
			const AWRCharacter* S = Survivor();
			return S && FWRBiomeHazards::Resists(S, Kind);
		}

		float DamageNow() const
		{
			const FWRItemStack* W = StackAt(EWRSlotKind::Equipment, 0);
			return UWRCombatLibrary::WeaponDamage(WeaponStats(W, DefOf(W)), false);
		}

		TSharedRef<SWidget> StatTile(FName Icon, TFunction<FText()> Value, const FText& Label, TFunction<FLinearColor()> Color)
		{
			return SNew(SBorder).BorderImage(WRUI::Box(FLinearColor(1.f, 1.f, 1.f, 0.03f), 4.f, FLinearColor(0.f, 0.f, 0.f, 0.65f), 1.f)).Padding(FMargin(10.f * U, 7.f * U))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f * U, 0.f)[SNew(SBox).WidthOverride(22.f * U).HeightOverride(22.f * U)[SNew(SImage).Image(WRUI::Icon(Icon))]]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda(MoveTemp(Value)), WRUI::Body(16, 800), TAttribute<FSlateColor>::CreateLambda([Color]() { return FSlateColor(Color()); }))]
					+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(Label, WRUI::Body(11, 500), WRUI::Ink3)]
				]
			];
		}

		TSharedRef<SWidget> EquipBlock(int32 e, const FText& Title)
		{
			TSharedRef<SVerticalBox> Badges = SNew(SVerticalBox).Visibility(EVisibility::HitTestInvisible);
			for (int32 g = 0; g < 3 && e == 0; ++g)
			{
				// Socketed gems: little brass-rimmed badges down the weapon slot's right edge.
				Badges->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 2.f * U)
				[
					SNew(SBox).WidthOverride(24.f * U).HeightOverride(24.f * U).Visibility_Lambda([this, g]() { const FWRItemStack* W = StackAt(EWRSlotKind::Equipment, 0); return W && !W->IsEmpty() && W->Sockets.IsValidIndex(g) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
					[
						SNew(SOverlay)
						+ SOverlay::Slot()[SNew(SImage).Image(WRUI::Box(FLinearColor(0.03f, 0.012f, 0.01f), 12.f * U, WRUI::Brass, 1.f))]
						+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
						[
							SNew(SBox).WidthOverride(16.f * U).HeightOverride(16.f * U)
							[
								SNew(SImage).Image_Lambda([this, g]() { const FWRItemStack* W = StackAt(EWRSlotKind::Equipment, 0); return W && W->Sockets.IsValidIndex(g) ? WRUI::Icon(W->Sockets[g]) : WRUI::White(); })
							]
						]
					]
				];
			}
			return SNew(SBox).WidthOverride(100.f * U)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[WRUI::Text(Title, WRUI::Caps(11), WRUI::Ink2)]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 4.f * U, 0.f, 0.f)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SNew(SWRSlot).Size(76.f * U).Kind(EWRSlotKind::Equipment).Index(e)
						.View_Lambda([this, e]() { return EquipView(e); })
						.OnClicked_Lambda([this, e](const FPointerEvent& Ev) { ClickedEquipSlot(Ev, e); })
						.OnDropped_Lambda([this, e](EWRSlotKind From, int32 Idx) { DroppedOnEquipSlot(From, Idx, e); })
					]
					+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top)[SNew(SBox).RenderTransform(FSlateRenderTransform(FVector2D(9.f * U, -9.f * U)))[Badges]]
				]
				// Wrapped labels fill the column and centre their lines (a centred slot would wrap at the last text's width).
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f * U, 0.f, 0.f)
				[
					WRInv::Label(TAttribute<FText>::CreateLambda([this, e]() { const FWRItemDef* D = DefOf(StackAt(EWRSlotKind::Equipment, e)); return D ? FText::FromString(D->Name) : LOCTEXT("EmptySlot", "Empty"); }),
						WRUI::Body(12, 700), TAttribute<FSlateColor>::CreateLambda([this, e]() { return FSlateColor(DefOf(StackAt(EWRSlotKind::Equipment, e)) ? WRUI::Ink : WRUI::Ink3); }), true, ETextJustify::Center)
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()
					[
						WRUI::Text(TAttribute<FText>::CreateLambda([this, e]()
						{
							const FWRItemStack* S = StackAt(EWRSlotKind::Equipment, e);
							const FWRItemDef* D = DefOf(S);
							return D && D->MaxDurability > 0.f && S->Quality != EWRQuality::Common ? FText::FromString(FWRItemStack::QualityName(S->Quality)) : FText::GetEmpty();
						}), WRUI::Body(11, 700), TAttribute<FSlateColor>::CreateLambda([this, e]() { const FWRItemStack* S = StackAt(EWRSlotKind::Equipment, e); return FSlateColor(S ? QualityColor(S->Quality) : WRUI::Ink3); }))
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						WRUI::Text(TAttribute<FText>::CreateLambda([this, e]()
						{
							const FWRItemStack* S = StackAt(EWRSlotKind::Equipment, e);
							const FWRItemDef* D = DefOf(S);
							if (!D) return FText::GetEmpty();
							if (D->MaxDurability > 0.f)
							{
								const FString Dur = FString::Printf(TEXT("%.0f/%.0f"), S->Durability, D->MaxDurability);
								return FText::FromString(S->Quality != EWRQuality::Common ? TEXT(" · ") + Dur : Dur);
							}
							return S->Count > 1 ? FText::FromString(FString::Printf(TEXT("x%d"), S->Count)) : FText::GetEmpty();
						}), WRUI::Body(11, 500), WRUI::Ink3)
					]
				]
			];
		}

		TSharedRef<SWidget> LoadedBlock()
		{
			return SNew(SBox).WidthOverride(100.f * U).Visibility_Lambda([this]() { const FWRItemDef* Bow = DefOf(StackAt(EWRSlotKind::Equipment, 1)); return Bow && !Bow->AmmoKind.IsNone() ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[WRUI::Text(LOCTEXT("Loaded", "Loaded"), WRUI::Caps(11), WRUI::Ink2)]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 5.f * U, 0.f, 0.f)
				[
					SNew(SWRSlot).Size(48.f * U).View_Lambda([this]()
					{
						FWRSlotView V;
						const UWRInventoryComponent* I = Inv();
						V.ItemId = LoadedAmmo();
						V.Count = I && !V.ItemId.IsNone() ? I->CountOf(V.ItemId) : 0;
						V.bShowCount = true;
						return V;
					})
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 5.f * U, 0.f, 0.f)
				[
					WRInv::Label(TAttribute<FText>::CreateLambda([this]()
					{
						const FName Ammo = LoadedAmmo();
						const UWRInventoryComponent* I = Inv();
						const FWRItemDef* Bow = DefOf(StackAt(EWRSlotKind::Equipment, 1));
						if (Ammo.IsNone()) return FText::FromString(FString::Printf(TEXT("No %s"), Bow ? *AmmoWord(Bow->AmmoKind, true) : TEXT("ammo")));
						const int32 N = I ? I->CountOf(Ammo) : 0;
						return FText::FromString(FString::Printf(TEXT("%s x%d"), *WRInv::Plural(WRInv::ItemName(Ammo), N), N));
					}), WRUI::Body(11, 700), TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(LoadedAmmo().IsNone() ? WRUI::Bad : WRUI::Ink2); }), true, ETextJustify::Center)
				]
			];
		}

		// ------------------------------------------------------------ bag
		TSharedRef<SWidget> FilterChip(EFilter F, const FText& Name)
		{
			return SNew(SButton).ButtonStyle(FCoreStyle::Get(), "NoBorder").ContentPadding(0.f).Cursor(EMouseCursor::Hand)
				.OnClicked_Lambda([this, F]() { Filter = F; return FReply::Handled(); })
				[
					SNew(SBorder).Padding(FMargin(14.f * U, 4.f * U, 14.f * U, 5.f * U))
					.BorderImage_Lambda([this, F]() { return Filter == F ? WRUI::Box(WRUI::Alpha(WRUI::Brass, 0.18f), 15.f * U, WRUI::Brass, 1.f) : WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.45f), 15.f * U, WRUI::Rim, 1.f); })
					[
						WRUI::Text(Name, WRUI::Body(13, 700), TAttribute<FSlateColor>::CreateLambda([this, F]() { return FSlateColor(Filter == F ? WRUI::Ink : WRUI::Ink2); }))
					]
				];
		}

		TSharedRef<SWidget> BagPanel()
		{
			TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel).SlotPadding(FMargin(5.f * U));
			for (int32 i = 0; i < UWRInventoryComponent::NumSlots; ++i)
			{
				Grid->AddSlot(i % 8, i / 8)
				[
					SNew(SWRSlot).Size(64.f * U).Kind(EWRSlotKind::Bag).Index(i)
					.View_Lambda([this, i]() { return BagView(i); })
					.OnClicked_Lambda([this, i](const FPointerEvent& Ev) { ClickedBagSlot(Ev, i); })
					.OnDropped_Lambda([this, i](EWRSlotKind From, int32 Idx) { DroppedOnBagSlot(From, Idx, i); })
				];
			}
			return SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(2.f * U, 0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Bottom)[WRUI::Text(LOCTEXT("Bag", "Bag"), WRUI::Title(18), WRUI::Ink)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::AsNumber(UsedSlots()); }), WRUI::Body(15, 800), WRUI::Ink)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(4.f, 0.f, 0.f, 0.f)[WRUI::Text(FText::FromString(FString::Printf(TEXT("/ %d"), UWRInventoryComponent::NumSlots)), WRUI::Body(15, 500), WRUI::Ink3)]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f * U, 0.f, 0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[FilterChip(EFilter::All, LOCTEXT("FAll", "All"))]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f * U, 0.f, 0.f, 0.f)[FilterChip(EFilter::Gear, LOCTEXT("FGear", "Gear"))]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f * U, 0.f, 0.f, 0.f)[FilterChip(EFilter::Resources, LOCTEXT("FRes", "Resources"))]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f * U, 0.f, 0.f, 0.f)[FilterChip(EFilter::Food, LOCTEXT("FFood", "Food"))]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f * U, 0.f, 0.f, 0.f)[FilterChip(EFilter::Ammo, LOCTEXT("FAmmo", "Ammo"))]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f * U, 0.f, 0.f, 0.f)[FilterChip(EFilter::Taming, LOCTEXT("FTaming", "Taming"))]
					+ SHorizontalBox::Slot().FillWidth(1.f)[SNullWidget::NullWidget]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						WRInv::IconButton(LOCTEXT("Sort", "Sort"), WRInv::Icons::Sort(), false, false, true, 12, 32.f * U,
							FOnClicked::CreateLambda([this]() { WRInv::SortBag(PC.Get()); return FReply::Handled(); }))
					]
				]
				+ SVerticalBox::Slot().FillHeight(1.f).Padding(0.f, 12.f * U, 0.f, 0.f)
				[
					SNew(SWRInvDropZone).OnDropped(this, &SWRInventoryTab::DroppedOnBag)
					[
						SNew(SWRPanel).Kind(EWRPanel::Inset).Padding(FMargin(14.f * U))
						[
							SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)[Grid]
						]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(2.f * U, 12.f * U, 2.f * U, 0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[
						WRUI::Text(TAttribute<FText>::CreateLambda([this]()
						{
							const int32 Empty = UWRInventoryComponent::NumSlots - UsedSlots();
							return FText::FromString(Empty == 0 ? FString(TEXT("Your bag is full")) : FString::Printf(TEXT("%d empty slot%s"), Empty, Empty == 1 ? TEXT("") : TEXT("s")));
						}), WRUI::Body(12, 500), TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(UsedSlots() >= UWRInventoryComponent::NumSlots ? WRUI::Warn : WRUI::Ink3); }))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SWRKeyCap).FontSize(11).Text(LOCTEXT("RMB", "RMB"))]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f * U, 0.f, 0.f, 0.f)[WRUI::Text(LOCTEXT("UseOrEat", "Use or eat"), WRUI::Body(12, 500), WRUI::Ink3)]
				];
		}

		// ------------------------------------------------------------ details
		TSharedRef<SWidget> StatRow(const FString& Label, const FString& Value, const FLinearColor& ValueColor = WRUI::Ink) const
		{
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[WRUI::Text(FText::FromString(Label), WRUI::Body(14, 500), WRUI::Ink3)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(FText::FromString(Value), WRUI::Body(14, 700), ValueColor)];
		}

		TSharedRef<SWidget> SmallRow(const FString& Label, const FString& Value) const
		{
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.f, 0.f, 12.f * U, 0.f)[WRUI::Text(FText::FromString(Label), WRUI::Body(12, 500), WRUI::Ink3)]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Top)[WRInv::Label(FText::FromString(Value), WRUI::Body(12, 500), WRUI::Ink2, true, ETextJustify::Right)];
		}

		TSharedRef<SWidget> Button(const FText& Text, bool bPrimary, bool bGhost, bool bEnabled, TFunction<void()> OnClick) const
		{
			return WRInv::TextButton(Text, bPrimary, bGhost, 13, 44.f * U, FOnClicked::CreateLambda([OnClick]() { OnClick(); return FReply::Handled(); }), bEnabled);
		}

		TSharedRef<SWidget> BuildDetails()
		{
			const FWRItemStack* S = StackAt(SelKind, SelIndex);
			const FWRItemDef* D = DefOf(S);
			const TSharedRef<SWidget> DragHint = SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f * U, 0.f)[SNew(SWRInvLineIcon).Icon(WRInv::Icons::Drag()).Size(16.f * U).Thickness(1.6f).Color(WRUI::Ink3)]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[WRUI::Text(LOCTEXT("DragHint", "Drag onto the hotbar to bind it to a key"), WRUI::Body(12, 500), WRUI::Ink3)];
			if (!D)
			{
				return SNew(SVerticalBox)
					+ SVerticalBox::Slot().FillHeight(1.f).VAlign(VAlign_Center).HAlign(HAlign_Center)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[WRUI::Text(LOCTEXT("NothingSel", "Nothing selected"), WRUI::Title(16), WRUI::Ink3)]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 8.f * U, 0.f, 0.f)
						[
							WRInv::Label(LOCTEXT("NothingSelHelp", "Click an item to see it here.\nRight-click uses or equips it."), WRUI::Body(13, 500), WRUI::Ink3, false, ETextJustify::Center)
						]
					]
					+ SVerticalBox::Slot().AutoHeight()[DragHint];
			}
			const bool bWorn = SelKind == EWRSlotKind::Equipment;
			const UWRInventoryComponent* I = Inv();

			// Header: the framed icon, name, quality (or kind of item) and what it is.
			FWRSlotView Big = WRInv::StackView(*S);
			Big.Durability = -1.f;
			const bool bGear = D->IsEquipment() && !D->IsThrowable();
			FString QualityLine = bGear ? FString(FWRItemStack::QualityName(S->Quality)) : CategoryWord(*D);
			if (bGear && S->Upgrade > 0) QualityLine += FString::Printf(TEXT("  +%d"), S->Upgrade);
			if (bGear && !S->Element.IsNone()) QualityLine += FString::Printf(TEXT("  ·  %s"), *S->Element.ToString());
			const FLinearColor QColor = bGear ? QualityColor(S->Quality) : WRUI::Brass;

			TSharedRef<SVerticalBox> Stats = SNew(SVerticalBox);
			const auto AddRow = [&Stats](const TSharedRef<SWidget>& W, float Top = 10.f) { Stats->AddSlot().AutoHeight().Padding(0.f, Stats->NumSlots() == 0 ? 0.f : Top * U, 0.f, 0.f)[W]; };
			if (D->Slot == EWREquipSlot::Weapon)
			{
				const FWRWeaponStats WS = WeaponStats(S, D);
				AddRow(StatRow(TEXT("Damage"), FString::Printf(TEXT("%.0f"), UWRCombatLibrary::WeaponDamage(WS, false))));
				AddRow(StatRow(TEXT("Heavy attack"), FString::Printf(TEXT("%.0f"), UWRCombatLibrary::WeaponDamage(WS, true))));
				const int32 Sockets = FWRItemStack::SocketCount(S->Quality);
				if (Sockets > 0 || S->Sockets.Num() > 0)
				{
					TArray<FString> Gems;
					for (const FName& G : S->Sockets) Gems.Add(WRInv::ItemName(G));
					AddRow(StatRow(TEXT("Sockets"), FString::Printf(TEXT("%d/%d%s"), S->Sockets.Num(), Sockets, Gems.Num() ? *(TEXT("  ") + FString::Join(Gems, TEXT(", "))) : TEXT(""))));
				}
				const float Crit = GemSum(*S, TEXT("critChance"));
				if (Crit > 0.f) AddRow(StatRow(TEXT("Critical chance"), FString::Printf(TEXT("+%.0f%%"), Crit * 100.f)));
			}
			else if (D->Slot == EWREquipSlot::Armor)
			{
				AddRow(StatRow(TEXT("Armor"), FString::Printf(TEXT("%.0f"), ArmorOf(*S, *D))));
				if (S->Element == TEXT("fire")) AddRow(StatRow(TEXT("Fire-infused"), TEXT("lava can't burn you"), WRUI::Ember));
				if (S->Element == TEXT("frost")) AddRow(StatRow(TEXT("Frost-infused"), TEXT("grip on ice"), WRUI::Frost));
			}
			else if (D->IsRanged())
			{
				const bool bBroken = D->MaxDurability > 0.f && S->Durability <= 0.f;
				const float Mult = D->IsThrowable() ? 1.f : QualityMult(S->Quality) * S->Roll * (bBroken ? 0.5f : 1.f);
				AddRow(StatRow(TEXT("Damage"), D->IsThrowable() ? FString::Printf(TEXT("%.0f"), D->RangedDamage) : FString::Printf(TEXT("%.0f + %s"), D->RangedDamage * Mult, *AmmoWord(D->AmmoKind, false))));
				AddRow(StatRow(D->IsThrowable() ? TEXT("Throw time") : TEXT("Reload"), FString::Printf(TEXT("%.1f s"), D->Reload)));
				if (D->TorporMult != 1.f && !D->IsThrowable()) AddRow(StatRow(TEXT("Torpor"), FString::Printf(TEXT("x%.1f"), D->TorporMult)));
			}
			else if (!D->AmmoOf.IsNone())
			{
				AddRow(StatRow(TEXT("Damage"), FString::Printf(TEXT("+%.0f"), D->AmmoDamage)));
				if (D->AmmoTorpor > 0.f) AddRow(StatRow(TEXT("Torpor"), FString::Printf(TEXT("%.0f"), D->AmmoTorpor), WRUI::Torpor));
				AddRow(StatRow(TEXT("Fired by"), LauncherOf(D->AmmoOf)));
			}
			else if (D->IsUsable())
			{
				if (D->Heal > 0.f) AddRow(StatRow(TEXT("Heals"), FString::Printf(TEXT("%.0f"), D->Heal), WRUI::Hp));
				if (D->Hunger > 0.f) AddRow(StatRow(TEXT("Food"), FString::Printf(TEXT("+%.0f"), D->Hunger), WRUI::Hunger));
				if (D->Thirst > 0.f) AddRow(StatRow(TEXT("Water"), FString::Printf(TEXT("+%.0f"), D->Thirst), WRUI::Thirst));
				if (D->bCleanse) AddRow(StatRow(TEXT("Cures"), TEXT("burn, frostbite, slow")));
				if (D->UseCooldown > 0.f) AddRow(StatRow(TEXT("Belt cooldown"), FString::Printf(TEXT("%.0f s"), D->UseCooldown)));
			}
			else if (D->GemStats.Num() > 0)
			{
				for (const TPair<FName, float>& G : D->GemStats) AddRow(StatRow(TEXT("Socketed"), GemLine(G.Key, G.Value), WRUI::Good));
			}
			if (D->MaxDurability > 0.f)
			{
				const float F = FMath::Clamp(S->Durability / D->MaxDurability, 0.f, 1.f);
				AddRow(SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[StatRow(TEXT("Durability"), FString::Printf(TEXT("%.0f/%.0f"), S->Durability, D->MaxDurability), S->Durability <= 0.f ? WRUI::Bad : WRUI::Ink)]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 0.f)[SNew(SWRBar).Height(6.f * U).Fraction(F).Color(F < 0.25f ? WRUI::Bad : F < 0.5f ? WRUI::Warn : WRUI::Good)]);
			}
			if (D->StackSize > 1) AddRow(StatRow(TEXT("Stack"), FString::Printf(TEXT("%d / %d"), S->Count, D->StackSize)));
			AddRow(StatRow(TEXT("Weight"), S->Count > 1 ? FString::Printf(TEXT("%s kg each · %s kg"), *WRInv::Kg(D->Weight), *WRInv::Kg(D->Weight * S->Count)) : FString::Printf(TEXT("%s kg"), *WRInv::Kg(D->Weight))));

			TSharedRef<SVerticalBox> Middle = SNew(SVerticalBox);
			Middle->AddSlot().AutoHeight()[Stats];
			bool bTaming = false;
			const FString Hint = HintFor(*D, bTaming);
			if (!Hint.IsEmpty())
			{
				Middle->AddSlot().AutoHeight().Padding(0.f, 14.f * U, 0.f, 0.f)
				[
					SNew(SBorder).BorderImage(WRUI::Box(WRUI::Alpha(WRUI::Brass, 0.07f), 4.f, WRUI::Alpha(WRUI::Brass, 0.22f), 1.f)).Padding(FMargin(12.f * U, 10.f * U))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.f, 1.f, 10.f * U, 0.f)[SNew(SWRInvLineIcon).Icon(bTaming ? WRInv::Icons::Paw() : WRInv::Icons::Info()).Size(18.f * U).Thickness(1.6f).Color(WRUI::Brass)]
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[WRInv::Label(FText::FromString(Hint), WRUI::Body(13, 500), WRUI::Ink2, true)]
					]
				];
			}
			// A bow's ammo in the bag (the one it fires next is rimmed).
			if (D->IsRanged() && !D->AmmoKind.IsNone() && I)
			{
				TArray<FName> Kinds;
				for (const FWRItemStack& B : I->GetSlots())
				{
					const FWRItemDef* A = DefOf(&B);
					if (A && A->AmmoOf == D->AmmoKind) Kinds.AddUnique(B.ItemId);
				}
				TSharedRef<SHorizontalBox> Cards = SNew(SHorizontalBox);
				const FName Next = bWorn ? LoadedAmmo() : NAME_None;
				for (int32 k = 0; k < Kinds.Num() && k < 3; ++k)
				{
					const FWRItemDef* A = FWRItemRegistry::Get().Find(Kinds[k]);
					FWRSlotView AV;
					AV.ItemId = Kinds[k];
					Cards->AddSlot().FillWidth(1.f).Padding(k == 0 ? 0.f : 8.f * U, 0.f, 0.f, 0.f)
					[
						SNew(SBorder).BorderImage(WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.25f), 4.f, Kinds[k] == Next ? WRUI::Alpha(WRUI::Brass, 0.8f) : FLinearColor(0.f, 0.f, 0.f, 0.6f), 1.f)).Padding(FMargin(5.f * U, 5.f * U, 10.f * U, 5.f * U))
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SWRSlot).Size(40.f * U).View(AV)]
							+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(10.f * U, 0.f, 0.f, 0.f)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(FText::FromString(A ? ShortAmmoName(*A) : Kinds[k].ToString()), WRUI::Body(14, 700), WRUI::Ink)]
								+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(FText::FromString(FString::Printf(TEXT("x%d"), I->CountOf(Kinds[k]))), WRUI::Body(12, 500), WRUI::Ink3)]
							]
						]
					];
				}
				Middle->AddSlot().AutoHeight().Padding(0.f, 14.f * U, 0.f, 0.f)[WRUI::Text(FText::FromString(FString::Printf(TEXT("%s in your bag"), *WRInv::Plural(AmmoWord(D->AmmoKind, false).Left(1).ToUpper() + AmmoWord(D->AmmoKind, false).Mid(1), 2))), WRUI::Caps(12), WRUI::Ink2)];
				if (Kinds.Num() > 0) Middle->AddSlot().AutoHeight().Padding(0.f, 8.f * U, 0.f, 0.f)[Cards];
				else Middle->AddSlot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 0.f)[WRInv::Label(LOCTEXT("NoAmmo", "None: craft some to shoot it."), WRUI::Body(13, 500), WRUI::Bad, true)];
			}
			// Where it's mended, upgraded or made.
			TSharedRef<SVerticalBox> Where = SNew(SVerticalBox);
			TArray<TPair<FString, FString>> WhereRows;
			const FWRRecipe* Recipe = FWRRecipeRegistry::Get().ForOutput(D->Id);
			const int32 Shards = I ? I->CountOf(TEXT("upgrade_shard")) : 0;
			if (D->Slot == EWREquipSlot::Weapon || D->Slot == EWREquipSlot::Armor)
			{
				const FString Station = D->Slot == EWREquipSlot::Weapon ? TEXT("Forge") : TEXT("Armorer");
				WhereRows.Add({ TEXT("Repair at"), Station });
				const int32 Cost = FWRCraftingRules::UpgradeCost(S->Upgrade);
				WhereRows.Add({ TEXT("Upgrade with"), FString::Printf(TEXT("%d upgrade shard%s (%d in bag)"), Cost, Cost == 1 ? TEXT("") : TEXT("s"), Shards) });
			}
			else if (D->IsRanged() && D->MaxDurability > 0.f && Recipe)
			{
				WhereRows.Add({ TEXT("Repair at"), StationName(Recipe->Station) });
			}
			else if (Recipe)
			{
				WhereRows.Add({ TEXT("Made at"), StationName(Recipe->Station) });
			}
			const TArray<FString> Makes = UsedToMake(D->Id);
			if (Makes.Num() > 0)
			{
				TArray<FString> Shown(Makes.GetData(), FMath::Min(3, Makes.Num()));
				WhereRows.Add({ TEXT("Used to make"), FString::Join(Shown, TEXT(", ")) + (Makes.Num() > 3 ? FString::Printf(TEXT(" and %d more"), Makes.Num() - 3) : FString()) });
			}
			for (int32 r = 0; r < WhereRows.Num(); ++r)
			{
				Where->AddSlot().AutoHeight().Padding(0.f, r == 0 ? 0.f : 6.f * U, 0.f, 0.f)[SmallRow(WhereRows[r].Key, WhereRows[r].Value)];
			}
			if (WhereRows.Num() > 0) Middle->AddSlot().AutoHeight().Padding(0.f, 14.f * U, 0.f, 0.f)[Where];

			// The actions.
			FText UseText = LOCTEXT("Use", "Use");
			bool bCanUse = true;
			const FName C = D->Category;
			if (bWorn) UseText = LOCTEXT("Unequip", "Unequip");
			else if (D->IsEquipment()) UseText = LOCTEXT("Equip", "Equip");
			else if (C == TEXT("food")) UseText = LOCTEXT("Eat", "Eat");
			else if (C == TEXT("drink") || C == TEXT("potion")) UseText = LOCTEXT("Drink", "Drink");
			else if (C == TEXT("gem")) UseText = LOCTEXT("Socket", "Socket");
			else if (C == TEXT("egg")) UseText = LOCTEXT("Hatch", "Hatch");
			else if (C == TEXT("dart")) UseText = LOCTEXT("UseDart", "Use");
			else bCanUse = false;
			const int32 Sel = SelIndex;
			const bool bCanSplit = !bWorn && S->Count >= 2 && FirstEmpty() != INDEX_NONE;
			const bool bCanDrop = !bWorn;

			return SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SBorder).BorderImage(WRUI::Box(FLinearColor(0.07f, 0.045f, 0.022f), 6.f, WRUI::Brass2, 2.f)).Padding(5.f * U)
						[
							SNew(SWRSlot).Size(92.f * U).View(Big)
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(14.f * U, 0.f, 0.f, 0.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[WRInv::Label(FText::FromString(D->Name), WRUI::Title(19), WRUI::Ink, true)]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 5.f * U, 0.f, 0.f)[WRUI::Text(FText::FromString(QualityLine), WRUI::Caps(13), QColor)]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 5.f * U, 0.f, 0.f)[WRInv::Label(FText::FromString(KindLine(*D)), WRUI::Body(12, 500), WRUI::Ink2, true)]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f * U)[WRUI::Divider()]
				+ SVerticalBox::Slot().FillHeight(1.f)
				[
					SNew(SScrollBox).ScrollBarVisibility(EVisibility::Collapsed)
					+ SScrollBox::Slot()[Middle]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f * U, 0.f, 0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f)
					[
						Button(UseText, true, false, bCanUse, [this, bWorn, Sel, D]()
						{
							if (bWorn) { Unequip(Sel); return; }
							if (D->IsEquipment())
							{
								const int32 E = EquipIndexOf(D->Slot);
								EquipFromBag(Sel, INDEX_NONE);
								if (E != INDEX_NONE) Select(EWRSlotKind::Equipment, E);   // follow it onto your body
								return;
							}
							UseSlot(Sel);
						})
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(8.f * U, 0.f, 0.f, 0.f)[Button(LOCTEXT("Split", "Split"), false, false, bCanSplit, [this, Sel]() { OpenPicker(EPick::Split, Sel); })]
					+ SHorizontalBox::Slot().AutoWidth().Padding(8.f * U, 0.f, 0.f, 0.f)
					[
						Button(LOCTEXT("Drop", "Drop"), false, true, bCanDrop, [this, Sel]()
						{
							const FWRItemStack* T = StackAt(EWRSlotKind::Bag, Sel);
							if (T && T->Count > 1) OpenPicker(EPick::Drop, Sel);
							else if (T && !T->IsEmpty()) Send(FString::Printf(TEXT("drop %d"), Sel));
						})
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f * U, 0.f, 0.f)[DragHint];
		}

		// ------------------------------------------------------------ hotbar strip
		FText KeysLine() const
		{
			const FWRKeyBindings& K = FWRKeyBindings::Get();
			const FString First = WRUI::KeyName(K.Key(TEXT("Hotbar1"))).ToString(), Last = WRUI::KeyName(K.Key(TEXT("Hotbar8"))).ToString();
			const FKey Next = K.Key(TEXT("HotbarNext")), Prev = K.Key(TEXT("HotbarPrev"));
			const FString Cycle = (Next == EKeys::MouseScrollDown || Next == EKeys::MouseScrollUp) && (Prev == EKeys::MouseScrollDown || Prev == EKeys::MouseScrollUp)
				? FString(TEXT("the mouse wheel cycles")) : FString::Printf(TEXT("%s / %s cycle"), *WRUI::KeyName(Prev).ToString(), *WRUI::KeyName(Next).ToString());
			return FText::FromString(FString::Printf(TEXT("Keys %s-%s use an item · %s"), *First, *Last, *Cycle));
		}

		int32 HoveredKey() const
		{
			if (!FSlateApplication::IsInitialized()) return INDEX_NONE;
			const FVector2D Cursor = FSlateApplication::Get().GetCursorPos();
			for (int32 k = 0; k < 8; ++k)
			{
				if (KeyWidgets[k].IsValid() && KeyWidgets[k]->GetTickSpaceGeometry().IsUnderLocation(Cursor)) return k;
			}
			return INDEX_NONE;
		}

		/** The drag chip's words: what letting go will do. */
		FText DragLine(bool& bKey) const
		{
			bKey = false;
			const TSharedPtr<FWRItemDragDrop> Op = WRInv::CurrentDrag();
			if (!Op.IsValid()) return FText::GetEmpty();
			if (Op->From == EWRSlotKind::Hotbar) return LOCTEXT("DragOffBar", "Release off the bar to clear the key");
			const int32 Key = HoveredKey();
			bKey = Key != INDEX_NONE;
			return bKey ? FText::FromString(FString::Printf(TEXT("Release to bind %s to"), *WRInv::ItemName(Op->ItemId)))
				: FText::FromString(FString::Printf(TEXT("Drop on a key to bind %s"), *WRInv::ItemName(Op->ItemId)));
		}

		TSharedRef<SWidget> HotbarStrip()
		{
			TSharedRef<SHorizontalBox> Keys = SNew(SHorizontalBox);
			for (int32 k = 0; k < 8; ++k)
			{
				Keys->AddSlot().AutoWidth().Padding(k == 0 ? 0.f : 6.f * U, 0.f, 0.f, 0.f)
				[
					SAssignNew(KeyWidgets[k], SWRSlot).Size(64.f * U).Kind(EWRSlotKind::Hotbar).Index(k)
					.View_Lambda([this, k]() { const FWRPlayerUI* X = UI(); return X ? X->HotbarView(k) : FWRSlotView(); })
					.OnClicked_Lambda([this, k](const FPointerEvent& Ev) { ClickedKey(Ev, k); })
					.OnDropped_Lambda([this, k](EWRSlotKind From, int32 Idx) { DroppedOnKey(From, Idx, k); })
				];
			}
			return SNew(SBox).HeightOverride(92.f * U)
			[
				// The strip itself keeps a key dragged a little off it (only off the strip clears it).
				SNew(SWRInvDropZone).OnDropped_Lambda([](EWRSlotKind, int32) {})
				[
					SNew(SWRPanel).Kind(EWRPanel::Inset).Padding(FMargin(24.f * U, 0.f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(SBox).WidthOverride(320.f * U)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(LOCTEXT("HotbarTitle", "Hotbar: drag items here"), WRUI::Title(14), WRUI::Ink)]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f * U, 0.f, 0.f)[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return KeysLine(); }), WRUI::Body(12, 500), WRUI::Ink3)]
							]
						]
						+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Center).VAlign(VAlign_Center)
						[
							SNew(SWRPanel).Kind(EWRPanel::Stone).Rivets(true).Padding(FMargin(14.f * U, 10.f * U))[Keys]
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(SBox).WidthOverride(320.f * U).HAlign(HAlign_Right)
							[
								SNew(SOverlay)
								+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Center)
								[
									SNew(SBorder).Visibility_Lambda([this]() { return WRInv::CurrentDrag().IsValid() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
									.BorderImage(WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.45f), 15.f * U, WRUI::Alpha(WRUI::Warn, 0.5f), 1.f)).Padding(FMargin(12.f * U, 5.f * U))
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { bool bKey; return DragLine(bKey); }), WRUI::Body(13, 700), WRUI::Warn)]
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.f * U, 0.f, 0.f, 0.f)
										[
											SNew(SBox).Visibility_Lambda([this]() { bool bKey; DragLine(bKey); return bKey ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
											[
												SNew(SWRKeyCap).FontSize(11).Text_Lambda([this]() { const int32 K = HoveredKey(); return K == INDEX_NONE ? FText::GetEmpty() : WRUI::KeyName(FWRKeyBindings::Get().Key(FName(*FString::Printf(TEXT("Hotbar%d"), K + 1)))); })
											]
										]
									]
								]
								+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Center)
								[
									SNew(SBox).Visibility_Lambda([this]() { return WRInv::CurrentDrag().IsValid() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
									[
										WRInv::Label(LOCTEXT("KeyHelp", "Right-click a key, or drag it off the bar, to clear it"), WRUI::Body(12, 500), WRUI::Ink3, true, ETextJustify::Right)
									]
								]
							]
						]
					]
				]
			];
		}
	};

	TWeakPtr<SWRInventoryTab> SWRInventoryTab::Current;
}

WR_REGISTER_JOURNAL_TAB("inventory", [](AWRPlayerController* PC) -> TSharedRef<SWidget>
{
	TSharedRef<WRInvTab::SWRInventoryTab> Tab = SNew(WRInvTab::SWRInventoryTab).PC(PC);
	WRInvTab::SWRInventoryTab::Current = Tab;
	return Tab;
});

#if !UE_BUILD_SHIPPING
// Testing / screenshots (uishots.ps1): select a bag slot ("3"), a worn slot ("weapon", "ranged", "armor") or the first
// stack of an item ("hunting_bow") in the open Inventory tab; open its split / drop picker.
static FAutoConsoleCommandWithWorldAndArgs GWRUISelect(TEXT("WRUISelect"), TEXT("Inventory tab: select a bag slot, a worn slot or an item"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const TSharedPtr<WRInvTab::SWRInventoryTab> Tab = WRInvTab::SWRInventoryTab::Current.Pin();
		const AWRPlayerController* PC = World ? Cast<AWRPlayerController>(World->GetFirstPlayerController()) : nullptr;
		const AWRPlayerState* PS = PC ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
		if (!Tab.IsValid() || !PS || !PS->GetInventory() || Args.Num() == 0) return;
		const FString& A = Args[0];
		for (int32 e = 0; e < 3; ++e) if (A == WRInvTab::EquipNames[e]) { Tab->Select(EWRSlotKind::Equipment, e); return; }
		if (A.IsNumeric()) { Tab->Select(EWRSlotKind::Bag, FCString::Atoi(*A)); return; }
		const int32 At = PS->GetInventory()->GetSlots().IndexOfByPredicate([&A](const FWRItemStack& S) { return !S.IsEmpty() && S.ItemId == FName(*A); });
		if (At != INDEX_NONE) Tab->Select(EWRSlotKind::Bag, At);
	}));

static FAutoConsoleCommandWithWorldAndArgs GWRUIInvPicker(TEXT("WRUIInvPicker"), TEXT("Inventory tab: open the split|drop picker on a bag slot (or an item's first stack)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const TSharedPtr<WRInvTab::SWRInventoryTab> Tab = WRInvTab::SWRInventoryTab::Current.Pin();
		const AWRPlayerController* PC = World ? Cast<AWRPlayerController>(World->GetFirstPlayerController()) : nullptr;
		const AWRPlayerState* PS = PC ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
		if (!Tab.IsValid() || !PS || !PS->GetInventory() || Args.Num() < 2) return;
		const FName Item(*Args[1]);
		const int32 Slot = Args[1].IsNumeric() ? FCString::Atoi(*Args[1]) : PS->GetInventory()->GetSlots().IndexOfByPredicate([Item](const FWRItemStack& S) { return !S.IsEmpty() && S.ItemId == Item; });
		if (Slot != INDEX_NONE)
		{
			Tab->Select(EWRSlotKind::Bag, Slot);
			Tab->DebugPicker(Args[0] == TEXT("split"), Slot);
		}
	}));

// Real Slate input for tests: mouse clicks and drags at screenshot pixels of the game viewport, and key presses.
//   WRUIMouse click|rclick|shiftclick|ctrlclick X Y      WRUIMouse drag X1 Y1 X2 Y2      WRUIKey Enter
namespace WRUITestInput
{
	FVector2D ToScreen(float X, float Y)
	{
		const TSharedPtr<SViewport> Viewport = GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetGameViewportWidget() : nullptr;
		FViewport* VP = GEngine && GEngine->GameViewport ? GEngine->GameViewport->Viewport : nullptr;
		if (!Viewport.IsValid() || !VP) return FVector2D(X, Y);
		const FGeometry& Geo = Viewport->GetTickSpaceGeometry();
		const FVector2D Pixels(VP->GetSizeXY());
		return FVector2D(Geo.GetAbsolutePosition()) + FVector2D(X, Y) * (FVector2D(Geo.GetAbsoluteSize()) / FVector2D::Max(Pixels, FVector2D(1.f, 1.f)));
	}

	void Mouse(const FVector2D& At, const FVector2D& Last, const TSet<FKey>& Pressed, const FKey& Button, bool bShift, bool bCtrl, int32 Kind /* 0 move, 1 down, 2 up */)
	{
		const FModifierKeysState Mods(bShift, false, bCtrl, false, false, false, false, false, false);
		const FPointerEvent Event(FSlateApplication::Get().CursorPointerIndex, At, Last, Pressed, Button, 0.f, Mods);
		if (Kind == 1) FSlateApplication::Get().ProcessMouseButtonDownEvent(nullptr, Event);
		else if (Kind == 2) FSlateApplication::Get().ProcessMouseButtonUpEvent(Event);
		else FSlateApplication::Get().ProcessMouseMoveEvent(Event);
	}
}

static FAutoConsoleCommandWithWorldAndArgs GWRUIMouse(TEXT("WRUIMouse"), TEXT("Test input: click|rclick|shiftclick|ctrlclick X Y, or drag X1 Y1 X2 Y2 (viewport pixels)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*)
	{
		using namespace WRUITestInput;
		if (Args.Num() < 3 || !FSlateApplication::IsInitialized()) return;
		const FVector2D A = ToScreen(FCString::Atof(*Args[1]), FCString::Atof(*Args[2]));
		const FString& What = Args[0];
		{
			const FWidgetPath Path = FSlateApplication::Get().LocateWindowUnderMouse(A, FSlateApplication::Get().GetInteractiveTopLevelWindows());
			UE_LOG(LogTemp, Log, TEXT("WRUIMouse at screen %s: %d widgets, under it %s"), *A.ToString(), Path.Widgets.Num(), Path.IsValid() ? *Path.Widgets.Last().Widget->GetTypeAsString() : TEXT("nothing"));
		}
		const FKey Button = What == TEXT("rclick") ? EKeys::RightMouseButton : EKeys::LeftMouseButton;
		const bool bShift = What == TEXT("shiftclick"), bCtrl = What == TEXT("ctrlclick");
		Mouse(A, A, {}, EKeys::Invalid, bShift, bCtrl, 0);
		Mouse(A, A, { Button }, Button, bShift, bCtrl, 1);
		if (What == TEXT("drag") && Args.Num() >= 5)
		{
			const FVector2D B = ToScreen(FCString::Atof(*Args[3]), FCString::Atof(*Args[4]));
			FVector2D Last = A;
			for (int32 i = 1; i <= 12; ++i)
			{
				const FVector2D P = FMath::Lerp(A, B, i / 12.f);
				Mouse(P, Last, { Button }, EKeys::Invalid, false, false, 0);
				Last = P;
			}
			Mouse(B, B, {}, Button, false, false, 2);
			UE_LOG(LogTemp, Log, TEXT("WRUIMouse drag (%s %s) -> (%s %s)"), *Args[1], *Args[2], *Args[3], *Args[4]);
			return;
		}
		Mouse(A, A, {}, Button, bShift, bCtrl, 2);
		UE_LOG(LogTemp, Log, TEXT("WRUIMouse %s (%s %s)"), *What, *Args[1], *Args[2]);
	}));

static FAutoConsoleCommandWithWorldAndArgs GWRUIKey(TEXT("WRUIKey"), TEXT("Test input: press a key through Slate (WRUIKey Enter)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*)
	{
		if (Args.Num() < 1 || !FSlateApplication::IsInitialized()) return;
		const FKey Key(*Args[0]);
		const FKeyEvent Event(Key, FModifierKeysState(), FSlateApplication::Get().GetUserIndexForKeyboard(), false, 0, 0);
		FSlateApplication::Get().ProcessKeyDownEvent(Event);
		FSlateApplication::Get().ProcessKeyUpEvent(Event);
	}));

// Send a gameplay command as the local player, as the screens do ("store 3 20", "take 0").
static FAutoConsoleCommandWithWorldAndArgs GWRUICmd(TEXT("WRUICmd"), TEXT("Send a gameplay command to the server as the local player"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		AWRPlayerController* PC = World ? Cast<AWRPlayerController>(World->GetFirstPlayerController()) : nullptr;
		if (PC && Args.Num() > 0) PC->SendCommand(FString::Join(Args, TEXT(" ")));
	}));
#endif

#undef LOCTEXT_NAMESPACE
