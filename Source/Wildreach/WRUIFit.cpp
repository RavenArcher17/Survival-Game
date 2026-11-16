#include "WRUIFit.h"
#include "WRTuning.h"
#include "Layout/ArrangedChildren.h"

float UWRDPIScalingRule::GetDPIScaleBasedOnSize(FIntPoint Size) const
{
	if (Size.X <= 0 || Size.Y <= 0)
	{
		return 1.f;
	}
	// Read once: the engine asks on every viewport size change.
	static const float Height = FMath::Max(1.f, WRTune(TEXT("ui.canvas.height"), 1080.f));
	static const float MinWidth = FMath::Max(1.f, WRTune(TEXT("ui.canvas.minWidth"), 1760.f));
	const float ByHeight = static_cast<float>(FMath::Min(Size.X, Size.Y)) / Height;
	const float ByWidth = static_cast<float>(Size.X) / MinWidth;
	return FMath::Max(0.05f, FMath::Min(ByHeight, ByWidth));
}

SWRFit::SWRFit()
{
	SetCanTick(false);
	bCanSupportFocus = false;
	bHasRelativeLayoutScale = true;
}

void SWRFit::Construct(const FArguments& InArgs)
{
	Margin = FMath::Max(0.f, InArgs._Margin);
	MinScale = FMath::Clamp(InArgs._MinScale, 0.1f, 1.f);
	// A full-screen layer: only what's in it takes clicks (an empty layer must never eat them: HANDOFF "Traps").
	SetVisibility(EVisibility::SelfHitTestInvisible);
	ChildSlot[InArgs._Content.Widget];
}

void SWRFit::OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const
{
	const TSharedRef<SWidget>& Child = ChildSlot.GetWidget();
	if (!ArrangedChildren.Accepts(Child->GetVisibility()))
	{
		return;
	}
	const FVector2D Avail = AllottedGeometry.GetLocalSize();
	const FVector2D Want = Child->GetDesiredSize();
	// The child's desired size is in its own units (independent of the scale drawn at), so this doesn't feed back.
	float S = 1.f;
	if (Want.X > Avail.X && Want.X > 1.f) S = FMath::Min(S, FMath::Max(1.f, Avail.X - 2.f * Margin) / Want.X);
	if (Want.Y > Avail.Y && Want.Y > 1.f) S = FMath::Min(S, FMath::Max(1.f, Avail.Y - 2.f * Margin) / Want.Y);
	S = FMath::Clamp(S, MinScale, 1.f);
	Scale = S;
	ArrangedChildren.AddWidget(AllottedGeometry.MakeChild(Child, FVector2D::ZeroVector, Avail / S, S));
}

FVector2D SWRFit::ComputeDesiredSize(float) const
{
	return ChildSlot.GetWidget()->GetDesiredSize() * Scale;
}

float SWRFit::GetRelativeLayoutScale(int32 ChildIndex, float LayoutScaleMultiplier) const
{
	return Scale;
}

TSharedRef<SWidget> WRUI::Fit(TSharedRef<SWidget> Content)
{
	return SNew(SWRFit)[Content];
}
