#include "WRUIMotion.h"
#include "Framework/Application/SlateApplication.h"

double WRUIMotion::Now()
{
	return FSlateApplication::IsInitialized() ? FSlateApplication::Get().GetCurrentTime() : FPlatformTime::Seconds();
}

void SWRAppear::Construct(const FArguments& InArgs)
{
	InSeconds = FMath::Max(0.01f, InArgs._Seconds);
	Offset = InArgs._Offset;
	StartScale = InArgs._Scale;
	Start = WRUIMotion::Now();
	SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	ChildSlot[InArgs._Content.Widget];
	Apply();
}

void SWRAppear::Tick(const FGeometry& Geo, const double Time, const float Delta)
{
	SCompoundWidget::Tick(Geo, Time, Delta);
	Apply();
}

void SWRAppear::Apply()
{
	const double Now = WRUIMotion::Now();
	float T = WRUIMotion::EaseOut(float(Now - Start) / InSeconds);
	if (OutStart >= 0.0)
	{
		T = FMath::Min(T, 1.f - WRUIMotion::EaseIn(float(Now - OutStart) / OutSeconds));
	}
	SetRenderOpacity(T);
	const FVector2D Move = Offset * (1.f - T);
	const float S = FMath::Lerp(StartScale, 1.f, T);
	if (Move.IsNearlyZero(0.01f) && FMath::IsNearlyEqual(S, 1.f))
	{
		SetRenderTransform(TOptional<FSlateRenderTransform>());
	}
	else
	{
		SetRenderTransform(FSlateRenderTransform(FScale2D(S), Move));
	}
}

void SWRAppear::PlayOut(float Seconds)
{
	if (OutStart < 0.0)
	{
		OutStart = WRUIMotion::Now();
		OutSeconds = FMath::Max(0.01f, Seconds);
		SetVisibility(EVisibility::HitTestInvisible);
	}
}

bool SWRAppear::IsGone() const
{
	return OutStart >= 0.0 && WRUIMotion::Now() - OutStart >= OutSeconds;
}
