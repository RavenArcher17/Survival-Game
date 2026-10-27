// The HUD's combat layer (stone-and-leather UI): floating damage numbers styled by what kind of hit it was, the
// crosshair hit marker and a brief flash when you parry. It sits under the HUD's panels (WRUIHud adds it first) and
// reads what WRFx::ShowHit recorded on this machine.

#include "WRFx.h"
#include "WRFxSystem.h"
#include "WRPlayerController.h"
#include "WRPlayerUI.h"
#include "WRTuning.h"
#include "WRUIStyle.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Widgets/SLeafWidget.h"

#define LOCTEXT_NAMESPACE "WRFx"

namespace WRFxHudDetail
{
	/** One damage number's look. */
	struct FStyle
	{
		FText Text;
		FSlateFontInfo Font;
		FLinearColor Color = FLinearColor::White;
		float Life = 1.1f;
		float Pop = 0.f;       // extra scale at the start (crits punch in)
		float Rise = 70.f;     // cm it climbs over its life
		float Alpha = 1.f;
		FText Tag;             // a small word under it ("weak", "riposte")
	};

	FStyle StyleFor(const UWRFxSubsystem::FHudHit& H)
	{
		using namespace WRFx;
		FStyle S;
		const bool bInvolved = H.bMine || H.bOnMe;
		S.Alpha = bInvolved ? 1.f : 0.55f;   // other people's fights stay readable but quiet
		switch (H.Result)
		{
		case EWRHitResult::Parried: S.Text = LOCTEXT("FxParried", "Parried"); S.Font = WRUI::Caps(20); S.Color = WRUI::Brass; S.Pop = 0.4f; return S;
		case EWRHitResult::Blocked: S.Text = LOCTEXT("FxBlocked", "Blocked"); S.Font = WRUI::Caps(16); S.Color = FLinearColor(0.72f, 0.8f, 0.9f); return S;
		case EWRHitResult::Dodged: S.Text = LOCTEXT("FxDodged", "Dodged"); S.Font = WRUI::Caps(15); S.Color = FLinearColor(0.75f, 0.85f, 0.95f); return S;
		default: break;
		}
		S.Text = FText::AsNumber(FMath::Max(1, FMath::RoundToInt(H.Damage)));
		if (H.Flags & Hit_Dot)
		{
			S.Font = WRUI::Body(15, 700);
			S.Color = (H.Flags & Hit_Frost) ? WRUI::Frost : (H.Flags & Hit_Fire) ? WRUI::Ember : FLinearColor(0.8f, 0.8f, 0.75f);
			S.Life = 0.9f;
			S.Rise = 45.f;
			return S;
		}
		if (H.bOnMe)
		{
			S.Font = WRUI::Body(22, 800);
			S.Color = WRUI::Bad;
			return S;
		}
		const bool bCrit = (H.Flags & (Hit_Crit | Hit_Riposte | Hit_Shatter)) != 0;
		const bool bWeak = (H.Flags & Hit_Weak) != 0;
		if (bCrit)
		{
			S.Font = WRUI::Title(32);
			S.Color = FLinearColor(1.f, 0.78f, 0.25f);
			S.Pop = 0.6f;
			S.Life = 1.4f;
			S.Rise = 95.f;
			S.Tag = (H.Flags & Hit_Shatter) ? LOCTEXT("FxShatter", "shatter") : (H.Flags & Hit_Riposte) ? LOCTEXT("FxRiposte", "riposte") : LOCTEXT("FxCrit", "critical");
		}
		else if (bWeak)
		{
			S.Font = WRUI::Title(28);
			S.Color = (H.Flags & Hit_Frost) ? FLinearColor(0.55f, 0.85f, 1.f) : FLinearColor(1.f, 0.55f, 0.2f);
			S.Pop = 0.45f;
			S.Life = 1.3f;
			S.Tag = LOCTEXT("FxWeak", "weak");
		}
		else if (H.Flags & Hit_Heavy)
		{
			S.Font = WRUI::Body(26, 800);
			S.Color = FLinearColor(1.f, 0.93f, 0.75f);
			S.Pop = 0.3f;
		}
		else
		{
			S.Font = WRUI::Body(21, 800);
			S.Color = (H.Flags & Hit_Fire) ? FLinearColor(1.f, 0.8f, 0.55f) : (H.Flags & Hit_Frost) ? FLinearColor(0.8f, 0.92f, 1.f) : FLinearColor(1.f, 0.96f, 0.86f);
		}
		return S;
	}
}

class SWRFxOverlay : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SWRFxOverlay) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AWRPlayerController>, PC)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		PC = InArgs._PC;
		SetVisibility(EVisibility::HitTestInvisible);
	}

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override
	{
		using namespace WRFxHudDetail;
		AWRPlayerController* P = PC.Get();
		UWRFxSubsystem* Fx = P ? UWRFxSubsystem::Get(P) : nullptr;
		if (!Fx || !Fx->IsActive())
		{
			return Layer;
		}
		const FWRPlayerUI* UI = P->GetUI();
		if (UI && UI->IsScreenOpen() && UI->GetScreen() != TEXT("taming"))
		{
			return Layer;
		}
		const double Now = P->GetWorld()->GetTimeSeconds();
		const FVector2D Size = Geo.GetLocalSize();
		int32 VX = 0, VY = 0;
		P->GetViewportSize(VX, VY);
		if (VX <= 0 || VY <= 0)
		{
			return Layer;
		}
		const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

		// Parry: a warm flash at the screen's edges, gone in a blink.
		const float ParryAge = static_cast<float>(Now - Fx->ParryFlashTime);
		const float ParryLen = WRTune(TEXT("fx.hud.parryFlashSeconds"), 0.18f);
		if (ParryAge >= 0.f && ParryAge < ParryLen)
		{
			const float A = WRTune(TEXT("fx.hud.parryFlashAlpha"), 0.22f) * (1.f - ParryAge / ParryLen);
			FSlateDrawElement::MakeBox(Out, Layer, Geo.ToPaintGeometry(), WRUI::White(), ESlateDrawEffect::None, FLinearColor(1.f, 0.9f, 0.65f, A));
		}

		// Damage numbers, oldest first so new ones draw on top.
		for (const UWRFxSubsystem::FHudHit& H : Fx->HudHits)
		{
			const FStyle S = StyleFor(H);
			const float Age = static_cast<float>(Now - H.Time);
			if (Age < 0.f || Age > S.Life) continue;
			const float T = Age / S.Life;
			const FVector World = H.World + FVector(0.f, 0.f, 25.f + S.Rise * (1.f - FMath::Square(1.f - T)));
			FVector2D Screen;
			if (!P->ProjectWorldLocationToScreen(World, Screen, true)) continue;
			FVector2D Pos(Screen.X * Size.X / VX, Screen.Y * Size.Y / VY);
			Pos.X += H.Drift * 18.f * T;
			// Pop in, settle; scale in steps so the font cache doesn't grow a size per frame.
			const float Pop = 1.f + S.Pop * FMath::Max(0.f, 1.f - Age / 0.14f);
			const float Scale = FMath::RoundToFloat(Pop * 10.f) / 10.f;
			const float Alpha = S.Alpha * (T < 0.6f ? 1.f : 1.f - (T - 0.6f) / 0.4f);
			FSlateFontInfo Font = S.Font;
			Font.Size = FMath::RoundToInt(Font.Size * Scale);
			Font.OutlineSettings.OutlineSize = Font.Size >= 26 ? 2 : 1;
			Font.OutlineSettings.OutlineColor = FLinearColor(0.02f, 0.015f, 0.01f, 0.9f * Alpha);
			const FVector2D TextSize = Measure->Measure(S.Text, Font);
			const FVector2D At = Pos - TextSize * 0.5f;
			FSlateDrawElement::MakeText(Out, Layer + 1, Geo.ToPaintGeometry(TextSize, FSlateLayoutTransform(At)), S.Text, Font, ESlateDrawEffect::None, WRUI::Alpha(S.Color, Alpha));
			if (!S.Tag.IsEmpty())
			{
				FSlateFontInfo TagFont = WRUI::Caps(12);
				TagFont.OutlineSettings.OutlineSize = 1;
				TagFont.OutlineSettings.OutlineColor = FLinearColor(0.02f, 0.015f, 0.01f, 0.9f * Alpha);
				const FVector2D TagSize = Measure->Measure(S.Tag, TagFont);
				FSlateDrawElement::MakeText(Out, Layer + 1, Geo.ToPaintGeometry(TagSize, FSlateLayoutTransform(FVector2D(Pos.X - TagSize.X * 0.5f, At.Y + TextSize.Y - 4.f))),
					S.Tag, TagFont, ESlateDrawEffect::None, WRUI::Alpha(S.Color, Alpha * 0.9f));
			}
		}

		// Hit marker: four short strokes around the screen's centre when your blow lands.
		const float MarkAge = static_cast<float>(Now - Fx->MarkerTime);
		const float MarkLen = WRTune(TEXT("fx.hud.markerSeconds"), 0.28f);
		if (MarkAge >= 0.f && MarkAge < MarkLen)
		{
			const uint16 F = Fx->MarkerFlags;
			const bool bBig = (F & (WRFx::Hit_Crit | WRFx::Hit_Riposte | WRFx::Hit_Weak | WRFx::Hit_Shatter)) != 0;
			const bool bHeavy = bBig || (F & WRFx::Hit_Heavy) || Fx->MarkerResult == EWRHitResult::GuardBroken;
			const bool bBlocked = Fx->MarkerResult == EWRHitResult::Blocked;
			const FLinearColor Color = bBlocked ? FLinearColor(0.6f, 0.65f, 0.7f) : bBig ? FLinearColor(1.f, 0.78f, 0.25f) : FLinearColor(1.f, 0.97f, 0.9f);
			const float T = MarkAge / MarkLen;
			const float Pop = 1.f + (bHeavy ? 0.5f : 0.3f) * FMath::Max(0.f, 1.f - MarkAge / 0.08f);
			const float Gap = (bHeavy ? 9.f : 7.f) * Pop;
			const float Len = (bHeavy ? 11.f : 8.f) * Pop;
			const float Thick = bHeavy ? 2.5f : 2.f;
			const FVector2D C = Size * 0.5f;
			const FLinearColor Shade(0.f, 0.f, 0.f, 0.55f * (1.f - T));
			const FLinearColor Tint = WRUI::Alpha(Color, 1.f - T * T);
			for (int32 k = 0; k < 4; ++k)
			{
				const FVector2D D(k == 0 || k == 3 ? -0.7071f : 0.7071f, k < 2 ? -0.7071f : 0.7071f);
				const TArray<FVector2D> Line = { C + D * Gap, C + D * (Gap + Len) };
				const TArray<FVector2D> Under = { Line[0] + FVector2D(1.f, 1.f), Line[1] + FVector2D(1.f, 1.f) };
				FSlateDrawElement::MakeLines(Out, Layer + 2, Geo.ToPaintGeometry(), Under, ESlateDrawEffect::None, Shade, true, Thick + 1.f);
				FSlateDrawElement::MakeLines(Out, Layer + 3, Geo.ToPaintGeometry(), Line, ESlateDrawEffect::None, Tint, true, Thick);
			}
		}
		return Layer + 3;
	}

private:
	TWeakObjectPtr<AWRPlayerController> PC;
};

TSharedRef<SWidget> WRFx::MakeHudOverlay(TWeakObjectPtr<AWRPlayerController> PC)
{
	return SNew(SWRFxOverlay).PC(PC);
}

#undef LOCTEXT_NAMESPACE
