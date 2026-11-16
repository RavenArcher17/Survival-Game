#pragma once

// Every screen fits every screen (cooks/wild-hotfix-menus.md in showcase-notes).
//
// UWRDPIScalingRule: the engine's interface scale for the whole game UI (DefaultEngine.ini UserInterfaceSettings,
// UIScaleRule=Custom). The stock rule only looked at the height (height / 1080), so a narrow screen (16:10, 4:3) lost
// width and the biggest windows ran off its sides. This one keeps a canvas at least ui.canvas.minWidth wide and
// ui.canvas.height tall: scale = min(shortest side / 1080, width / 1760). 16:9 and 21:9 are unchanged.
//
// SWRFit: "scale to fit, down only" that keeps edge anchoring. Its child is laid out over the whole space it is given;
// when the child wants more than that (a big interface size, an odd window), the child is laid out on a bigger canvas
// and drawn smaller (never larger), so nothing goes off the screen and corner pieces stay in their corners. Every
// viewport root (screens, HUD, wheel, flow covers, the main menu...) sits in one.

#include "CoreMinimal.h"
#include "Engine/DPICustomScalingRule.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "WRUIFit.generated.h"

UCLASS()
class WILDREACH_API UWRDPIScalingRule : public UDPICustomScalingRule
{
	GENERATED_BODY()
public:
	virtual float GetDPIScaleBasedOnSize(FIntPoint Size) const override;
};

class WILDREACH_API SWRFit : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWRFit) : _Margin(16.f), _MinScale(0.55f) {}
		/** Logical px kept clear at each edge once it has to shrink. */
		SLATE_ARGUMENT(float, Margin)
		/** It never shrinks below this (a screen that would need less is a layout bug to fix, not to hide). */
		SLATE_ARGUMENT(float, MinScale)
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	SWRFit();
	void Construct(const FArguments& InArgs);
	/** The scale it drew at last frame (1 = fits as designed). */
	float GetFitScale() const { return Scale; }

	virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override;
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual float GetRelativeLayoutScale(int32 ChildIndex, float LayoutScaleMultiplier) const override;

private:
	float Margin = 16.f;
	float MinScale = 0.55f;
	mutable float Scale = 1.f;
};

namespace WRUI
{
	/** The usual wrap: SNew(SWRFit)[Content]. */
	WILDREACH_API TSharedRef<SWidget> Fit(TSharedRef<SWidget> Content);
}
