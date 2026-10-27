#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

/**
 * UI motion: screens and tabs ease in, toasts slide, buttons answer the mouse. Everything is short (about 150 ms) and
 * never holds input: a leaving widget stops taking clicks the moment it starts to go. Times are real seconds, so a
 * paused or slowed game still animates its menus.
 */
namespace WRUIMotion
{
	/** Real time (Slate's clock). */
	WILDREACH_API double Now();
	/** 0..1, fast start and a soft landing. */
	inline float EaseOut(float T) { T = FMath::Clamp(T, 0.f, 1.f); const float S = 1.f - T; return 1.f - S * S * S; }
	/** 0..1, slow start. */
	inline float EaseIn(float T) { T = FMath::Clamp(T, 0.f, 1.f); return T * T; }
	/** In over InSeconds from Start, hold, out over OutSeconds ending at Start + Total: 0..1. */
	inline float InHoldOut(double Age, float InSeconds, float Total, float OutSeconds)
	{
		if (Age < 0.0 || Age > Total) return 0.f;
		const float In = InSeconds > 0.f ? EaseOut(float(Age) / InSeconds) : 1.f;
		const float Out = OutSeconds > 0.f ? 1.f - EaseIn(float(Age - (Total - OutSeconds)) / OutSeconds) : 1.f;
		return FMath::Min(In, Age > Total - OutSeconds ? Out : 1.f);
	}
}

/** A hover that eases in and out (about 100 ms) instead of snapping: Set on mouse enter / leave, Get 0..1 to draw. */
struct FWRHoverFade
{
	void Set(bool bHover)
	{
		if (bHover != bOn)
		{
			From = Get();
			bOn = bHover;
			ChangedAt = WRUIMotion::Now();
		}
	}
	float Get() const { return FMath::Lerp(From, bOn ? 1.f : 0.f, WRUIMotion::EaseOut(float(WRUIMotion::Now() - ChangedAt) / 0.1f)); }
private:
	double ChangedAt = -10.0;
	float From = 0.f;
	bool bOn = false;
};

/** Fades and slides its content into place as it appears; PlayOut fades it away again. */
class WILDREACH_API SWRAppear : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWRAppear) : _Seconds(0.15f), _Offset(FVector2D(0.f, 14.f)), _Scale(1.f) {}
		SLATE_ARGUMENT(float, Seconds)
		/** Where the content starts, relative to its place (slate units). */
		SLATE_ARGUMENT(FVector2D, Offset)
		/** Starting scale (1 = none), about the middle. */
		SLATE_ARGUMENT(float, Scale)
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& Geo, const double Time, const float Delta) override;

	/** Fade out (and drift back toward Offset) over Seconds; from now on clicks pass through. */
	void PlayOut(float Seconds);
	/** Out and finished (safe to remove). */
	bool IsGone() const;

private:
	void Apply();
	double Start = 0.0;
	double OutStart = -1.0;
	float InSeconds = 0.15f;
	float OutSeconds = 0.12f;
	FVector2D Offset = FVector2D::ZeroVector;
	float StartScale = 1.f;
};
