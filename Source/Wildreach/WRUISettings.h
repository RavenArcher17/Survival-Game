#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class APlayerController;

/** Small line icons drawn in code on the mockups' 24-unit grid (their SVGs), so they need no image files. */
enum class EWRIcon : uint8
{
	Gear, Flag, FlagFilled, Keyboard, Monitor, Play, Exit, Power, Join, Chevron, Info, Clock, Download, Bulb,
	Flame, Snowflake, Warning, Lock, Close, Speaker,
};

class WILDREACH_API SWRLineIcon : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SWRLineIcon) : _Icon(EWRIcon::Gear), _Size(24.f), _Color(FLinearColor::White), _Accent(FLinearColor::White) {}
		SLATE_ARGUMENT(EWRIcon, Icon)
		SLATE_ARGUMENT(float, Size)
		/** The main stroke; Accent is the second colour some icons have (the flag's cloth, the quit icon's line...). */
		SLATE_ATTRIBUTE(FLinearColor, Color)
		SLATE_ATTRIBUTE(FLinearColor, Accent)
	SLATE_END_ARGS()
	void Construct(const FArguments& InArgs);
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(Size, Size); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;

private:
	EWRIcon Icon = EWRIcon::Gear;
	float Size = 24.f;
	TAttribute<FLinearColor> Color;
	TAttribute<FLinearColor> Accent;
};

/** A linear gradient (the mockups' fades: a selected row, the front end's darkening over the island). */
class WILDREACH_API SWRGradientFill : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SWRGradientFill) : _Horizontal(true), _Radius(0.f) {}
		SLATE_ARGUMENT(TArray<FLinearColor>, Colors)
		/** Where each colour sits, 0..1 (evenly spread when empty). */
		SLATE_ARGUMENT(TArray<float>, Stops)
		SLATE_ARGUMENT(bool, Horizontal)
		SLATE_ARGUMENT(float, Radius)
	SLATE_END_ARGS()
	void Construct(const FArguments& InArgs);
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1.f, 1.f); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;

private:
	TArray<FLinearColor> Colors;
	TArray<float> Stops;
	bool bHorizontal = true;
	float Radius = 0.f;
};

namespace WRUI
{
	/**
	 * The settings window (mockup "Settings: controls"): Controls (every rebindable key, how abilities are cast),
	 * Audio (volumes, WRSound) and Video (the engine's game-user settings). In game (Esc) it is also the game menu: Resume, Back to main menu, Quit.
	 * The main menu shows the same window from its Settings button. OnClose runs on Close, Esc and Done / Resume.
	 */
	WILDREACH_API TSharedRef<SWidget> MakeSettings(APlayerController* PC, bool bInGame, FSimpleDelegate OnClose);
}
