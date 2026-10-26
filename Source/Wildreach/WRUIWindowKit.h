#pragma once

// Shared by the Hearth and land-claim screens (WRUIHearth.cpp, WRUIClaim.cpp): the centred stone window with its
// leather title band (mockups "Hearth", "Land claim"), round badges, and a few painting helpers (circles, filled
// shapes) the custom-painted parts use. Header-only; everything lives in WRUIWindowKit so nothing collides.

#include "CoreMinimal.h"
#include "WRUIStyle.h"
#include "WRUIWidgets.h"
#include "WRPlayerController.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBackgroundBlur.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"

namespace WRUIWindowKit
{
	using WRUI::U;   // mockup px -> 1080p units

	/** A circle (or a ring with a transparent fill) of the geometry it's drawn in. */
	inline const FSlateBrush* Circle(const FLinearColor& Fill, const FLinearColor& Outline = FLinearColor::Transparent, float OutlineWidth = 0.f)
	{
		static TMap<TTuple<uint32, uint32, int32>, TUniquePtr<FSlateBrush>> Brushes;
		const TTuple<uint32, uint32, int32> Key(Fill.ToFColor(false).DWColor(), Outline.ToFColor(false).DWColor(), FMath::RoundToInt(OutlineWidth * 8.f));
		TUniquePtr<FSlateBrush>& Brush = Brushes.FindOrAdd(Key);
		if (!Brush.IsValid())
		{
			// This constructor rounds by half the height: a square comes out round.
			Brush = MakeUnique<FSlateRoundedBoxBrush>(Fill, Outline, OutlineWidth);
		}
		return Brush.Get();
	}

	/** A circle centred at C (local units), radius R; the outline sits inside R. */
	inline void DrawCircle(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2D& C, float R, const FLinearColor& Fill, const FLinearColor& Outline = FLinearColor::Transparent, float OutlineWidth = 0.f)
	{
		WRUI::DrawBox(Out, Layer, Geo.ToPaintGeometry(FVector2D(R * 2.f, R * 2.f), FSlateLayoutTransform(C - FVector2D(R, R))), Circle(Fill, Outline, OutlineWidth));
	}

	/** A 4x4 white texture: filled shapes are drawn as triangles over it (a colour brush has no texture to sample). */
	inline const FSlateBrush* SolidTexture()
	{
		static TSharedPtr<FSlateDynamicImageBrush> Brush;
		if (!Brush.IsValid())
		{
			TArray<uint8> Pixels;
			Pixels.Init(255, 4 * 4 * 4);
			Brush = FSlateDynamicImageBrush::CreateWithImageData(TEXT("WRUIWindowKitSolid"), FVector2D(4.f, 4.f), Pixels);
		}
		return Brush.Get();
	}

	/** A filled shape: a fan from Centre, so the outline must be visible from it (a flame, a pennant). */
	inline void FillShape(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const TArray<FVector2D>& Points, const FVector2D& Centre, const FLinearColor& Color)
	{
		if (Points.Num() < 3 || !FSlateApplication::IsInitialized()) return;
		const FSlateResourceHandle Handle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*SolidTexture());
		const FSlateRenderTransform& T = Geo.GetAccumulatedRenderTransform();
		const FColor C = Color.ToFColor(true);
		TArray<FSlateVertex> Verts;
		TArray<SlateIndex> Indexes;
		Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(T, FVector2f(Centre), FVector2f(0.5f, 0.5f), C));
		for (const FVector2D& P : Points)
		{
			Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(T, FVector2f(P), FVector2f(0.5f, 0.5f), C));
		}
		for (int32 i = 0; i < Points.Num(); ++i)
		{
			Indexes.Add(0);
			Indexes.Add(1 + i);
			Indexes.Add(1 + (i + 1) % Points.Num());
		}
		FSlateDrawElement::MakeCustomVerts(Out, Layer, Handle, Verts, Indexes, nullptr, 0, 0);
	}

	inline void Cubic(TArray<FVector2D>& Out, const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& D, int32 Steps = 8)
	{
		for (int32 i = 1; i <= Steps; ++i)
		{
			const float T = static_cast<float>(i) / Steps, S = 1.f - T;
			Out.Add(A * (S * S * S) + B * (3.f * S * S * T) + C * (3.f * S * T * T) + D * (T * T * T));
		}
	}

	/** The mockups' flame (a 24-unit icon) at Origin, Scale units per icon unit; Centre gets the fan's middle. */
	inline TArray<FVector2D> Flame(const FVector2D& Origin, float Scale, FVector2D& OutCentre)
	{
		TArray<FVector2D> P;
		const auto At = [&](float X, float Y) { return Origin + FVector2D(X, Y) * Scale; };
		P.Add(At(12.f, 2.f));
		Cubic(P, At(12.f, 2.f), At(15.f, 6.f), At(18.f, 8.f), At(18.f, 13.f));
		for (int32 i = 1; i <= 12; ++i)
		{
			const float A = PI * i / 12.f;
			P.Add(At(12.f + 6.f * FMath::Cos(A), 13.f + 6.f * FMath::Sin(A)));
		}
		Cubic(P, At(6.f, 13.f), At(6.f, 10.f), At(8.f, 8.f), At(9.f, 7.f));
		Cubic(P, At(9.f, 7.f), At(10.f, 5.f), At(11.5f, 3.5f), At(12.f, 2.f), 4);
		OutCentre = At(12.f, 13.f);
		return P;
	}

	/** A round badge: a dark disc with a brass rim and a flame (the Hearth) or a pennant (a land claim). */
	class SBadge : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SBadge) : _Size(40.f * U), _bFlag(false) {}
			SLATE_ARGUMENT(float, Size)
			SLATE_ARGUMENT(bool, bFlag)
		SLATE_END_ARGS()
		void Construct(const FArguments& InArgs) { Size = InArgs._Size; bFlag = InArgs._bFlag; }
		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(Size, Size); }
		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override
		{
			const float R = Size * 0.5f;
			const FVector2D C(R, R);
			const FLinearColor Glow = bFlag ? WRUI::Good : WRUI::Ember;
			for (int32 i = 3; i >= 1; --i) DrawCircle(Out, Layer, Geo, C, R + i * 2.5f, WRUI::Alpha(Glow, 0.07f));
			DrawCircle(Out, Layer + 1, Geo, C, R, bFlag ? FLinearColor(0.03f, 0.045f, 0.02f) : FLinearColor(0.06f, 0.018f, 0.008f), WRUI::Brass2, 2.f);
			DrawCircle(Out, Layer + 1, Geo, C - FVector2D(0.f, R * 0.12f), R * 0.62f, bFlag ? FLinearColor(0.04f, 0.07f, 0.025f, 0.8f) : FLinearColor(0.11f, 0.03f, 0.01f, 0.8f));
			const float S = Size * 0.55f / 24.f;   // the 24-unit icon at 55% of the badge
			const FVector2D O = C - FVector2D(12.f, 12.f) * S;
			if (bFlag)
			{
				const auto At = [&](float X, float Y) { return O + FVector2D(X, Y) * S; };
				FSlateDrawElement::MakeLines(Out, Layer + 2, Geo.ToPaintGeometry(), TArray<FVector2D>{ At(7.f, 22.f), At(7.f, 3.f) }, ESlateDrawEffect::None, WRUI::Ink, true, FMath::Max(1.5f, 2.f * S));
				FillShape(Out, Layer + 2, Geo, { At(7.5f, 4.f), At(18.5f, 4.f), At(15.5f, 8.5f), At(18.5f, 13.f), At(7.5f, 13.f) }, At(10.f, 8.5f), WRUI::Good);
			}
			else
			{
				FVector2D Mid;
				const TArray<FVector2D> F = Flame(O, S, Mid);
				FillShape(Out, Layer + 2, Geo, F, Mid, WRUI::Ember);
			}
			return Layer + 2;
		}
	private:
		float Size = 40.f;
		bool bFlag = false;
	};

	/** The close button's cross. */
	class SCross : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SCross) {}
		SLATE_END_ARGS()
		void Construct(const FArguments& InArgs) {}
		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(16.f * U, 16.f * U); }
		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override
		{
			const FVector2D S = Geo.GetLocalSize();
			const float In = S.X * 0.2f;
			FSlateDrawElement::MakeLines(Out, Layer, Geo.ToPaintGeometry(), TArray<FVector2D>{ FVector2D(In, In), S - FVector2D(In, In) }, ESlateDrawEffect::None, WRUI::Ink, true, 2.4f);
			FSlateDrawElement::MakeLines(Out, Layer, Geo.ToPaintGeometry(), TArray<FVector2D>{ FVector2D(S.X - In, In), FVector2D(In, S.Y - In) }, ESlateDrawEffect::None, WRUI::Ink, true, 2.4f);
			return Layer;
		}
	};

	/**
	 * The window: blurred game behind, a 1240x760 (mockup px) stone window with brass trim and rivets, the leather title
	 * band (badge, title and line under it, a chip, "Esc Close" and the X), then Content in the body.
	 */
	inline TSharedRef<SWidget> Window(TWeakObjectPtr<AWRPlayerController> PC, TSharedRef<SWidget> Badge, const TAttribute<FText>& Title, const TAttribute<FText>& Subtitle,
		const TAttribute<FText>& Chip, const TAttribute<FLinearColor>& ChipColor, TSharedRef<SWidget> Content)
	{
		return SNew(SOverlay)
			+ SOverlay::Slot()[SNew(SBackgroundBlur).BlurStrength(8.f)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.5f))]]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(1240.f * U).HeightOverride(760.f * U)
				[
					SNew(SWRPanel).Kind(EWRPanel::Stone).Rivets(true).BrassTrim(true).Padding(FMargin(10.f * U, 10.f * U, 10.f * U, 0.f))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SBox).HeightOverride(60.f * U)
							[
								SNew(SWRPanel).Kind(EWRPanel::Leather).Padding(FMargin(16.f * U, 0.f, 18.f * U, 0.f))
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f * U, 0.f)[Badge]
									+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
									[
										SNew(SVerticalBox)
										+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(Title, WRUI::Title(20), WRUI::Ink, true)]
										+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(Subtitle, WRUI::Body(12, 500), WRUI::Ink2, true)]
									]
									+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 14.f * U, 0.f)
									[
										SNew(SWRChip).FontSize(12).Text(Chip).Color(ChipColor)
									]
									+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 7.f * U, 0.f)[SNew(SWRKeyCap).FontSize(11).Text(NSLOCTEXT("WRUI", "KitEsc", "Esc"))]
									+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 14.f * U, 0.f)[WRUI::Text(NSLOCTEXT("WRUI", "KitClose", "Close"), WRUI::Body(13, 500), WRUI::Ink2, true)]
									+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
									[
										SNew(SButton).ButtonStyle(FCoreStyle::Get(), "NoBorder").ContentPadding(0.f).Cursor(EMouseCursor::Hand).ToolTipText(NSLOCTEXT("WRUI", "KitCloseTip", "Close (Esc)"))
										.OnClicked_Lambda([PC]() { if (PC.IsValid()) PC->SetMenu(EWRMenu::None); return FReply::Handled(); })
										[
											SNew(SBox).WidthOverride(36.f * U).HeightOverride(36.f * U)
											[
												SNew(SOverlay)
												+ SOverlay::Slot()[SNew(SImage).Image(WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.35f), 4.f, WRUI::Brass2, 1.f))]
												+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[SNew(SCross)]
											]
										]
									]
								]
							]
						]
						+ SVerticalBox::Slot().FillHeight(1.f).Padding(FMargin(16.f * U, 20.f * U, 16.f * U, 22.f * U))[Content]
					]
				]
			];
	}

	/** A dark recessed card (the mockups' rgba(0,0,0,0.28) boxes with a black hairline). */
	inline const FSlateBrush* CardBrush(const FLinearColor& Edge = FLinearColor(0.f, 0.f, 0.f, 0.75f), const FLinearColor& Fill = FLinearColor(0.f, 0.f, 0.f, 0.28f))
	{
		return WRUI::Box(Fill, 5.f, Edge, 1.f);
	}

	/** Roman numerals for tiers (I..X). */
	inline FString Roman(int32 N)
	{
		static const TCHAR* R[] = { TEXT("0"), TEXT("I"), TEXT("II"), TEXT("III"), TEXT("IV"), TEXT("V"), TEXT("VI"), TEXT("VII"), TEXT("VIII"), TEXT("IX"), TEXT("X") };
		return N >= 0 && N <= 10 ? R[N] : FString::FromInt(N);
	}

	/** "1,200". */
	inline FText Num(float V) { return FText::AsNumber(FMath::RoundToInt(V)); }

	/** Compass word for a world offset (north is -Y, east +X, as on the compass strip and the map). */
	inline const TCHAR* CompassWord(const FVector& D)
	{
		static const TCHAR* Dirs[] = { TEXT("N"), TEXT("NE"), TEXT("E"), TEXT("SE"), TEXT("S"), TEXT("SW"), TEXT("W"), TEXT("NW") };
		const float Deg = FMath::RadiansToDegrees(FMath::Atan2(D.X, -D.Y));
		return Dirs[((FMath::RoundToInt(Deg / 45.f) % 8) + 8) % 8];
	}

	/** The same in words ("north-east"), for sentences. */
	inline const TCHAR* CompassName(const FVector& D)
	{
		static const TCHAR* Names[] = { TEXT("north"), TEXT("north-east"), TEXT("east"), TEXT("south-east"), TEXT("south"), TEXT("south-west"), TEXT("west"), TEXT("north-west") };
		const float Deg = FMath::RadiansToDegrees(FMath::Atan2(D.X, -D.Y));
		return Names[((FMath::RoundToInt(Deg / 45.f) % 8) + 8) % 8];
	}

	/** "1.2 km" / "340 m". */
	inline FString Distance(float Cm) { return Cm >= 100000.f ? FString::Printf(TEXT("%.1f km"), Cm / 100000.f) : FString::Printf(TEXT("%.0f m"), Cm / 100.f); }
}
