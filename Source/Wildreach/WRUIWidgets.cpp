#include "WRUIWidgets.h"
#include "WRUIStyle.h"
#include "Rendering/DrawElements.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "WRUI"

// ---------------------------------------------------------------- helpers

TSharedRef<SWidget> WRUI::Text(const TAttribute<FText>& InText, const FSlateFontInfo& Font, const TAttribute<FSlateColor>& Color, bool bShadow, bool bWrap)
{
	return SNew(STextBlock)
		.Text(InText)
		.Font(Font)
		.ColorAndOpacity(Color)
		.ShadowOffset(bShadow ? FVector2D(1.f, 1.f) : FVector2D::ZeroVector)
		.ShadowColorAndOpacity(WRUI::Shadow)
		.AutoWrapText(bWrap);
}

FText WRUI::KeyName(const FKey& Key)
{
	if (!Key.IsValid()) return LOCTEXT("Unbound", "Not set");
	if (Key == EKeys::LeftMouseButton) return LOCTEXT("LMB", "LMB");
	if (Key == EKeys::RightMouseButton) return LOCTEXT("RMB", "RMB");
	if (Key == EKeys::MiddleMouseButton) return LOCTEXT("MMB", "MMB");
	if (Key == EKeys::ThumbMouseButton) return LOCTEXT("M4", "Mouse 4");
	if (Key == EKeys::ThumbMouseButton2) return LOCTEXT("M5", "Mouse 5");
	if (Key == EKeys::MouseScrollUp) return LOCTEXT("WheelUp", "Wheel up");
	if (Key == EKeys::MouseScrollDown) return LOCTEXT("WheelDown", "Wheel down");
	if (Key == EKeys::SpaceBar) return LOCTEXT("Space", "Space");
	if (Key == EKeys::LeftShift) return LOCTEXT("Shift", "Shift");
	if (Key == EKeys::LeftControl) return LOCTEXT("Ctrl", "Ctrl");
	if (Key == EKeys::LeftAlt) return LOCTEXT("Alt", "Alt");
	if (Key == EKeys::Escape) return LOCTEXT("Esc", "Esc");
	if (Key == EKeys::Tab) return LOCTEXT("Tab", "Tab");
	if (Key == EKeys::Enter) return LOCTEXT("Enter", "Enter");
	return Key.GetDisplayName(false);
}

void WRUI::DrawBox(FSlateWindowElementList& Out, int32 Layer, const FPaintGeometry& Geometry, const FSlateBrush* Brush)
{
	// Draw elements don't apply a brush's own tint (widgets do): pass it, or rounded boxes come out white.
	FSlateDrawElement::MakeBox(Out, Layer, Geometry, Brush, ESlateDrawEffect::None, Brush ? Brush->TintColor.GetSpecifiedColor() : FLinearColor::White);
}

TSharedRef<SWidget> WRUI::Divider()
{
	return SNew(SBox).HeightOverride(1.f)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity(WRUI::Alpha(WRUI::Brass2, 0.8f))];
}

// ---------------------------------------------------------------- panel

void SWRPanel::Construct(const FArguments& InArgs)
{
	const EWRPanel Kind = InArgs._Kind;
	const FSlateBrush* Material = Kind == EWRPanel::Leather ? WRUI::Image(TEXT("leather")) : Kind == EWRPanel::Parchment ? WRUI::Image(TEXT("parchment")) : WRUI::Image(TEXT("stone"));
	const FLinearColor Tint = Kind == EWRPanel::Glass ? FLinearColor(0.82f, 0.82f, 0.86f, 0.86f) : Kind == EWRPanel::Inset ? FLinearColor(0.f, 0.f, 0.f, 0.35f) : FLinearColor::White;
	const FLinearColor Outline = InArgs._BrassTrim ? WRUI::Brass2 : WRUI::Edge;
	const float OutlineW = InArgs._BrassTrim ? 2.f : 1.f;

	TSharedRef<SOverlay> Overlay = SNew(SOverlay);
	if (Kind == EWRPanel::Inset)
	{
		Overlay->AddSlot()[SNew(SImage).Image(WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.32f), 4.f, FLinearColor(0.f, 0.f, 0.f, 0.7f), 1.f))];
	}
	else
	{
		// The tile mustn't size the panel (its texture is 256 px): the content does.
		Overlay->AddSlot()[SNew(SImage).Image(Material).ColorAndOpacity(Tint).DesiredSizeOverride(FVector2D(1.f, 1.f))];
		// Depth: a soft dark band along the bottom and a faint highlight along the top.
		Overlay->AddSlot().VAlign(VAlign_Bottom)[SNew(SBox).HeightOverride(28.f)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.18f))]];
		Overlay->AddSlot().VAlign(VAlign_Top)[SNew(SBox).HeightOverride(1.f)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.08f))]];
		Overlay->AddSlot()[SNew(SImage).Image(WRUI::Box(FLinearColor::Transparent, 5.f, Outline, OutlineW))];
		if (Kind == EWRPanel::Leather)
		{
			// The stitching: a thin inner line in thread colour.
			Overlay->AddSlot().Padding(5.f)[SNew(SImage).Image(WRUI::Box(FLinearColor::Transparent, 3.f, WRUI::Alpha(FLinearColor(0.82f, 0.68f, 0.45f), 0.45f), 1.f))];
		}
	}
	if (InArgs._Rivets)
	{
		const FSlateBrush* Rivet = WRUI::Image(TEXT("rivet"));
		const auto R = [Rivet](EHorizontalAlignment H, EVerticalAlignment V) { return SNew(SBox).WidthOverride(10.f).HeightOverride(10.f).HAlign(H).VAlign(V)[SNew(SImage).Image(Rivet)]; };
		Overlay->AddSlot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(5.f)[R(HAlign_Left, VAlign_Top)];
		Overlay->AddSlot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(5.f)[R(HAlign_Right, VAlign_Top)];
		Overlay->AddSlot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(5.f)[R(HAlign_Left, VAlign_Bottom)];
		Overlay->AddSlot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(5.f)[R(HAlign_Right, VAlign_Bottom)];
	}
	Overlay->AddSlot().Padding(InArgs._Padding)[InArgs._Content.Widget];
	ChildSlot[Overlay];
}

// ---------------------------------------------------------------- slot

TSharedRef<FWRItemDragDrop> FWRItemDragDrop::New(EWRSlotKind From, int32 Index, FName ItemId, int32 Count)
{
	TSharedRef<FWRItemDragDrop> Op = MakeShared<FWRItemDragDrop>();
	Op->From = From;
	Op->Index = Index;
	Op->ItemId = ItemId;
	Op->Count = Count;
	Op->Construct();
	return Op;
}

TSharedPtr<SWidget> FWRItemDragDrop::GetDefaultDecorator() const
{
	return SNew(SBox).WidthOverride(56.f).HeightOverride(56.f)[SNew(SImage).Image(WRUI::Icon(ItemId)).ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.85f))];
}

void SWRSlot::Construct(const FArguments& InArgs)
{
	View = InArgs._View;
	Kind = InArgs._Kind;
	Index = InArgs._Index;
	OnClicked = InArgs._OnClicked;
	OnDropped = InArgs._OnDropped;
	const float Size = InArgs._Size;

	ChildSlot.HAlign(HAlign_Center).VAlign(VAlign_Center)
	[
		SNew(SBox).WidthOverride(Size).HeightOverride(Size)
		[
			SNew(SOverlay)
			// Mockup colours are sRGB (#2E3238 to #141619 with a near-black edge): FColor, not linear values, or slots wash out grey.
			+ SOverlay::Slot()[SNew(SImage).Image(WRUI::Box(FLinearColor(FColor(10, 11, 13, 235)), 4.f, FLinearColor(FColor(4, 4, 5)), 1.f))]
			+ SOverlay::Slot().Padding(1.f)[SNew(SImage).Image_Lambda([this]()
			{
				const FWRSlotView V = View.Get();
				const FLinearColor Rim = bDropHover ? WRUI::Brass : V.bSelected ? WRUI::Brass : V.Quality == 2 ? FLinearColor(0.25f, 0.43f, 0.66f) : V.Quality == 1 ? FLinearColor(0.31f, 0.5f, 0.24f) : WRUI::Rim;
				return WRUI::Box(FLinearColor(FColor(33, 36, 41, V.ItemId.IsNone() ? 165 : 215)), 3.f, Rim, V.bSelected || bDropHover ? 2.f : 1.f);
			})]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(Size * 0.76f).HeightOverride(Size * 0.76f)
				[
					SNew(SImage)
					.Image_Lambda([this]() { const FWRSlotView V = View.Get(); return V.ItemId.IsNone() ? WRUI::White() : WRUI::Icon(V.ItemId); })
					.ColorAndOpacity_Lambda([this]() { const FWRSlotView V = View.Get(); return V.ItemId.IsNone() ? FLinearColor::Transparent : V.bLocked ? FLinearColor(0.35f, 0.35f, 0.35f, 1.f) : V.bDimmed ? FLinearColor(1.f, 1.f, 1.f, 0.35f) : FLinearColor::White; })
					.RenderTransform_Lambda([this]() { const float R = View.Get().IconRotation; return R == 0.f ? TOptional<FSlateRenderTransform>() : TOptional<FSlateRenderTransform>(FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(R)))); })
					.RenderTransformPivot(FVector2D(0.5f, 0.5f))
				]
			]
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(5.f, 1.f)
			[
				WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return View.Get().Key; }), WRUI::Body(Size >= 60.f ? 12 : 10, 700), WRUI::Brass, true)
			]
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(5.f, 0.f, 5.f, 2.f)
			[
				WRUI::Text(TAttribute<FText>::CreateLambda([this]()
				{
					const FWRSlotView V = View.Get();
					return !V.ItemId.IsNone() && (V.Count > 1 || V.bShowCount) ? FText::AsNumber(V.Count) : FText::GetEmpty();
				}), WRUI::Body(Size >= 60.f ? 14 : 12, 800), WRUI::Ink, true)
			]
			+ SOverlay::Slot().VAlign(VAlign_Bottom).Padding(6.f, 0.f, 6.f, 4.f)
			[
				SNew(SBox).HeightOverride(3.f).Visibility_Lambda([this]() { const FWRSlotView V = View.Get(); return V.Durability >= 0.f && !V.ItemId.IsNone() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				[
					SNew(SWRBar).Height(3.f)
					.Fraction_Lambda([this]() { return View.Get().Durability; })
					.Color_Lambda([this]() { const float D = View.Get().Durability; return D < 0.25f ? WRUI::Bad : D < 0.5f ? WRUI::Warn : WRUI::Good; })
				]
			]
		]
	];
}

FReply SWRSlot::OnMouseButtonDown(const FGeometry& Geo, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() == EKeys::LeftMouseButton && !View.Get().ItemId.IsNone() && Kind != EWRSlotKind::Other)
	{
		return FReply::Handled().DetectDrag(SharedThis(this), EKeys::LeftMouseButton);
	}
	return FReply::Handled();
}

FReply SWRSlot::OnMouseButtonUp(const FGeometry& Geo, const FPointerEvent& Event)
{
	OnClicked.ExecuteIfBound(Event);
	return FReply::Handled();
}

FReply SWRSlot::OnDragDetected(const FGeometry& Geo, const FPointerEvent& Event)
{
	const FWRSlotView V = View.Get();
	if (V.ItemId.IsNone())
	{
		return FReply::Unhandled();
	}
	return FReply::Handled().BeginDragDrop(FWRItemDragDrop::New(Kind, Index, V.ItemId, V.Count));
}

void SWRSlot::OnDragEnter(const FGeometry& Geo, const FDragDropEvent& Event)
{
	bDropHover = OnDropped.IsBound() && Event.GetOperationAs<FWRItemDragDrop>().IsValid();
}

void SWRSlot::OnDragLeave(const FDragDropEvent& Event)
{
	bDropHover = false;
}

FReply SWRSlot::OnDrop(const FGeometry& Geo, const FDragDropEvent& Event)
{
	bDropHover = false;
	const TSharedPtr<FWRItemDragDrop> Op = Event.GetOperationAs<FWRItemDragDrop>();
	if (!Op.IsValid() || !OnDropped.IsBound())
	{
		return FReply::Unhandled();
	}
	OnDropped.Execute(Op->From, Op->Index);
	return FReply::Handled();
}

// ---------------------------------------------------------------- bar

void SWRBar::Construct(const FArguments& InArgs)
{
	Fraction = InArgs._Fraction;
	Color = InArgs._Color;
	Height = InArgs._Height;
}

int32 SWRBar::OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
	const FVector2D Size = Geo.GetLocalSize();
	const float R = FMath::Min(3.f, Size.Y * 0.5f);
	WRUI::DrawBox(Out, Layer, Geo.ToPaintGeometry(), WRUI::Box(FLinearColor(0.035f, 0.037f, 0.043f, 0.95f), R, FLinearColor(0.f, 0.f, 0.f, 1.f), 1.f));
	const float F = FMath::Clamp(Fraction.Get(), 0.f, 1.f);
	if (F > 0.f)
	{
		const FVector2D FillSize(FMath::Max(2.f, (Size.X - 2.f) * F), Size.Y - 2.f);
		const FLinearColor C = Color.Get();
		WRUI::DrawBox(Out, Layer + 1, Geo.ToPaintGeometry(FillSize, FSlateLayoutTransform(FVector2D(1.f, 1.f))), WRUI::Box(C, FMath::Max(0.f, R - 1.f)));
		// Gloss on the top half.
		WRUI::DrawBox(Out, Layer + 2, Geo.ToPaintGeometry(FVector2D(FillSize.X, FillSize.Y * 0.45f), FSlateLayoutTransform(FVector2D(1.f, 1.f))), WRUI::Box(FLinearColor(1.f, 1.f, 1.f, 0.16f), FMath::Max(0.f, R - 1.f)));
	}
	return Layer + 2;
}

// ---------------------------------------------------------------- keycap, button, chip

void SWRKeyCap::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(WRUI::Box(FLinearColor(0.16f, 0.175f, 0.19f), 4.f, FLinearColor(0.f, 0.f, 0.f), 1.f))
		.Padding(FMargin(6.f, 2.f, 6.f, 3.f))
		.HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox).MinDesiredWidth(InArgs._FontSize + 6.f).HAlign(HAlign_Center)
			[
				WRUI::Text(InArgs._Text, WRUI::Body(InArgs._FontSize, 800), WRUI::Ink)
			]
		]
	];
}

void SWRButton::Construct(const FArguments& InArgs)
{
	const bool bPrimary = InArgs._Primary;
	const bool bGhost = InArgs._Ghost;
	TSharedPtr<SButton> Button;
	ChildSlot
	[
		SAssignNew(Button, SButton)
		.ButtonStyle(FCoreStyle::Get(), "NoBorder")
		.ContentPadding(0.f)
		.OnClicked(InArgs._OnClicked)
		.Cursor(EMouseCursor::Hand)
		[
			SNew(SBox).MinDesiredHeight(InArgs._MinHeight)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[SNew(SImage).Image(bGhost ? WRUI::White() : WRUI::Image(TEXT("leather"))).ColorAndOpacity(bGhost ? FLinearColor(0.f, 0.f, 0.f, 0.32f) : FLinearColor::White).DesiredSizeOverride(FVector2D(1.f, 1.f))]
				+ SOverlay::Slot()[SNew(SImage).Image_Lambda([this, bPrimary]()
				{
					const bool bHover = IsHovered();
					return WRUI::Box(bHover ? FLinearColor(1.f, 0.85f, 0.6f, 0.08f) : FLinearColor::Transparent, 4.f, bPrimary || bHover ? WRUI::Brass : WRUI::Brass2, bPrimary ? 1.5f : 1.f);
				})]
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(FMargin(18.f, 6.f))
				[
					WRUI::Text(InArgs._Text, WRUI::Title(InArgs._FontSize), TAttribute<FSlateColor>::CreateLambda([this]() { return IsEnabled() ? WRUI::Ink : WRUI::Ink3; }), true)
				]
			]
		]
	];
}

void SWRChip::Construct(const FArguments& InArgs)
{
	TAttribute<FLinearColor> Color = InArgs._Color;
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage_Lambda([Color]() { return WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.5f), 10.f, WRUI::Alpha(Color.Get(), 0.5f), 1.f); })
		.Padding(FMargin(9.f, 1.f, 9.f, 2.f))
		[
			WRUI::Text(InArgs._Text, WRUI::Body(InArgs._FontSize, 700), TAttribute<FSlateColor>::CreateLambda([Color]() { return FSlateColor(Color.Get()); }))
		]
	];
}

#undef LOCTEXT_NAMESPACE
