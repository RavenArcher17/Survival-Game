#include "WRUICaptions.h"
#include "WRPlayerController.h"
#include "WRSound.h"
#include "WRTuning.h"
#include "WRUIStyle.h"
#include "WRUIWidgets.h"
#include "WRMainMenu.h"
#include "Camera/PlayerCameraManager.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SLeafWidget.h"

namespace WRCapDetail
{
	using WRUI::U;

	/**
	 * Up to four lines, newest at the bottom, each on a dark leather-tone plate so it reads on snow and on night alike:
	 * an arrow pointing where the sound came from (straight up = ahead of the camera), the text, "x2" when it repeats.
	 * Danger lines (wind-ups, tells, explosions) are ember, the rest ink. A line fades over its last moments and is a
	 * little dimmer the farther its sound was.
	 */
	class SWRCaptions : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SWRCaptions) {}
			SLATE_ARGUMENT(TWeakObjectPtr<AWRPlayerController>, PC)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			PC = InArgs._PC;
			SetVisibility(TAttribute<EVisibility>::CreateLambda([]()
			{
				return WRSoundCaptions::Mode() > 0 && !SWRMainMenu::IsOpen() && WRSoundCaptions::Lines().Num() > 0 ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
			}));
		}

		virtual FVector2D ComputeDesiredSize(float) const override
		{
			return FVector2D(560.f * U, 4.f * LineHeight());
		}

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override
		{
			const TArray<WRSoundCaptions::FLine>& Lines = WRSoundCaptions::Lines();
			if (Lines.Num() == 0)
			{
				return Layer;
			}
			// The camera: where "ahead" is for the arrows.
			FVector CamAt = FVector::ZeroVector;
			FRotator CamRot = FRotator::ZeroRotator;
			if (const AWRPlayerController* P = PC.Get(); P && P->PlayerCameraManager)
			{
				CamAt = P->PlayerCameraManager->GetCameraLocation();
				CamRot = P->PlayerCameraManager->GetCameraRotation();
			}
			const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
			const FSlateFontInfo Font = WRUI::Body(15, 700);
			const double Now = FPlatformTime::Seconds();
			const float Hold = WRTune(TEXT("ui.captions.seconds"), 3.f);
			const float Fade = WRTune(TEXT("ui.captions.fade"), 0.4f);
			const float H = LineHeight();
			const FVector2D Size = Geo.GetLocalSize();
			const float Arrow = 22.f * U, Pad = 12.f * U;
			for (int32 i = 0; i < Lines.Num(); ++i)
			{
				const WRSoundCaptions::FLine& L = Lines[i];
				const int32 Row = Lines.Num() - 1 - i;   // newest at the bottom
				const float Y = Size.Y - (Row + 1) * H;
				const float Age = static_cast<float>(Now - L.Time);
				float A = FMath::Clamp((Hold - Age) / FMath::Max(Fade, 0.05f), 0.f, 1.f);
				FVector From = L.Where;
				if (const AActor* Src = L.Source.Get()) From = Src->GetActorLocation();
				const float Dist = FVector::Dist(From, CamAt);
				A *= FMath::Lerp(1.f, 0.6f, FMath::Clamp(Dist / FMath::Max(L.Radius, 1.f), 0.f, 1.f));
				const FText Text = FText::FromString(L.Count > 1 ? FString::Printf(TEXT("%s  ×%d"), *L.Text, L.Count) : L.Text);
				const FVector2D TextSize = Measure->Measure(Text, Font);
				const float W = Pad + Arrow + 8.f * U + static_cast<float>(TextSize.X) + Pad;
				const float X = (static_cast<float>(Size.X) - W) * 0.5f;
				const FLinearColor Ink = L.bDanger ? WRUI::Ember : WRUI::Ink;
				// The plate.
				WRUI::DrawBox(Out, Layer, Geo.ToPaintGeometry(FVector2D(W, H - 4.f * U), FSlateLayoutTransform(FVector2D(X, Y))),
					WRUI::Box(FLinearColor(0.035f, 0.025f, 0.02f, 0.72f * A), 6.f, WRUI::Alpha(L.bDanger ? WRUI::Ember : WRUI::Brass2, 0.55f * A), 1.f));
				// The arrow: from the camera's facing to the sound, turned on the screen (up = ahead, right = to your right).
				FVector To = From - CamAt;
				To.Z = 0.f;
				const float Yaw = To.IsNearlyZero() ? 0.f : FMath::FindDeltaAngleDegrees(CamRot.Yaw, To.Rotation().Yaw);
				const float Rad = FMath::DegreesToRadians(Yaw);
				const FVector2D Dir(FMath::Sin(Rad), -FMath::Cos(Rad));
				const FVector2D Perp(-Dir.Y, Dir.X);
				const FVector2D C(X + Pad + Arrow * 0.5f, Y + (H - 4.f * U) * 0.5f);
				const float R = Arrow * 0.42f;
				const FVector2D Tip = C + Dir * R;
				TArray<FVector2D> Shaft = { C - Dir * R, Tip };
				TArray<FVector2D> Head = { Tip - Dir * (R * 0.75f) + Perp * (R * 0.6f), Tip, Tip - Dir * (R * 0.75f) - Perp * (R * 0.6f) };
				FSlateDrawElement::MakeLines(Out, Layer + 1, Geo.ToPaintGeometry(), Shaft, ESlateDrawEffect::None, WRUI::Alpha(Ink, A), true, 2.f * U);
				FSlateDrawElement::MakeLines(Out, Layer + 1, Geo.ToPaintGeometry(), Head, ESlateDrawEffect::None, WRUI::Alpha(Ink, A), true, 2.f * U);
				// The words, with a soft shadow.
				const FVector2D TextAt(X + Pad + Arrow + 8.f * U, Y + ((H - 4.f * U) - static_cast<float>(TextSize.Y)) * 0.5f);
				FSlateDrawElement::MakeText(Out, Layer + 1, Geo.ToPaintGeometry(TextSize, FSlateLayoutTransform(TextAt + FVector2D(1.f, 1.f))), Text, Font, ESlateDrawEffect::None, WRUI::Alpha(WRUI::Shadow, 0.8f * A));
				FSlateDrawElement::MakeText(Out, Layer + 2, Geo.ToPaintGeometry(TextSize, FSlateLayoutTransform(TextAt)), Text, Font, ESlateDrawEffect::None, WRUI::Alpha(Ink, A));
			}
			return Layer + 2;
		}

	private:
		static float LineHeight() { return 34.f * U; }
		TWeakObjectPtr<AWRPlayerController> PC;
	};
}

TSharedRef<SWidget> WRUICaptions::Make(TWeakObjectPtr<AWRPlayerController> PC)
{
	return SNew(WRCapDetail::SWRCaptions).PC(PC);
}
