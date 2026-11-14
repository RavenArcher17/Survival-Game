// The in-game HUD of the stone-and-leather UI (mockup "HUD: exploring"): compass, clock, level and statuses, vitals,
// the item hotbar, the ability keys, your tame, the taming panel, prompts, pickups, chat, boss bar and help (H).
// Sizes are in 1080p units (the mockups' 1600x900 px x 1.2); Slate scales them to the screen.

#include "WRPlayerUI.h"
#include "WRTaming.h"
#include "WRPadInput.h"
#include "WRUIStyle.h"
#include "WRUIWidgets.h"
#include "WRUIMap.h"
#include "WRAltar.h"
#include "WRAttributeSet.h"
#include "WRBoss.h"
#include "WRStatusComponent.h"
#include "WRBuilding.h"
#include "WRCharacter.h"
#include "WRCrafting.h"
#include "WRCreature.h"
#include "WRGameplayTags.h"
#include "WRGameState.h"
#include "WRInventory.h"
#include "WRKeyBindings.h"
#include "WRPathComponent.h"
#include "WRPathTypes.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRFarm.h"
#include "WRSurvival.h"
#include "WRWorldDressing.h"
#include "WRTuning.h"
#include "WRUIFlow.h"
#include "WRUIMotion.h"
#include "WRFx.h"
#include "WRWater.h"
#include "AbilitySystemComponent.h"
#include "EngineUtils.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "WRUI"

namespace
{
	using WRUI::U;

	FVector2D Measure(const FText& Text, const FSlateFontInfo& Font)
	{
		return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text, Font);
	}

	void DrawTextAt(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FText& Text, const FSlateFontInfo& Font, const FVector2D& Pos, const FLinearColor& Color, bool bShadow = true)
	{
		const FVector2D Size = Measure(Text, Font);
		if (bShadow)
		{
			FSlateDrawElement::MakeText(Out, Layer, Geo.ToPaintGeometry(Size, FSlateLayoutTransform(Pos + FVector2D(1.f, 1.f))), Text, Font, ESlateDrawEffect::None, WRUI::Shadow);
		}
		FSlateDrawElement::MakeText(Out, Layer + 1, Geo.ToPaintGeometry(Size, FSlateLayoutTransform(Pos)), Text, Font, ESlateDrawEffect::None, Color);
	}

	FString Clock(float Seconds) { const int32 S = FMath::Max(0, FMath::FloorToInt(Seconds)); return FString::Printf(TEXT("%d:%02d"), S / 60, S % 60); }
}

/** The compass strip: headings and the island's places within 90 degrees of where you look. */
/** The player's own status meters (combat.md 2.11): a thin bar per status that's building, in its colour, above the
 *  stamina bar; nothing when none are. */
class SWRStatusMeters : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SWRStatusMeters) {}
		SLATE_ATTRIBUTE(const AWRCharacterBase*, Who)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs) { Who = InArgs._Who; }

	virtual FVector2D ComputeDesiredSize(float) const override
	{
		return FVector2D(300.f, FMath::Max(0, Count()) * 7.f);
	}

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override
	{
		const AWRCharacterBase* C = Who.Get();
		if (!C) return Layer;
		const float W = Geo.GetLocalSize().X;
		float Y = 0.f;
		for (int32 i = 0; i < C->BuildupPct.Num() && i < WRStatus::Count; ++i)
		{
			if (C->BuildupPct[i] == 0) continue;
			FSlateDrawElement::MakeBox(Out, Layer, Geo.ToPaintGeometry(FVector2D(W, 5.f), FSlateLayoutTransform(FVector2D(0.f, Y))), WRUI::White(), ESlateDrawEffect::None, FLinearColor(0.f, 0.f, 0.f, 0.5f));
			FSlateDrawElement::MakeBox(Out, Layer + 1, Geo.ToPaintGeometry(FVector2D(W * C->BuildupPct[i] / 100.f, 5.f), FSlateLayoutTransform(FVector2D(0.f, Y))), WRUI::White(), ESlateDrawEffect::None, WRStatus::Color(i));
			Y += 7.f;
		}
		return Layer + 1;
	}

private:
	int32 Count() const
	{
		const AWRCharacterBase* C = Who.Get();
		int32 N = 0;
		if (C) for (const uint8 P : C->BuildupPct) N += P > 0 ? 1 : 0;
		return N;
	}
	TAttribute<const AWRCharacterBase*> Who;
};

class SWRCompass : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SWRCompass) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AWRPlayerController>, PC)
	SLATE_END_ARGS()
	void Construct(const FArguments& InArgs) { PC = InArgs._PC; }
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(560.f * U, 46.f * U); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override
	{
		const AWRPlayerController* P = PC.Get();
		const APawn* Pawn = P ? P->GetPawn() : nullptr;
		if (!Pawn) return Layer;
		const FVector2D Size = Geo.GetLocalSize();
		const FVector F = P->GetControlRotation().Vector();
		const float Heading = FMath::RadiansToDegrees(FMath::Atan2(F.X, -F.Y));
		const auto XFor = [&](float Deg, bool& bVisible)
		{
			const float Off = FMath::UnwindDegrees(Deg - Heading);
			bVisible = FMath::Abs(Off) <= 90.f;
			return Size.X * 0.5f + Off / 90.f * (Size.X * 0.5f - 22.f);
		};
		static const TCHAR* Names[] = { TEXT("N"), TEXT("NE"), TEXT("E"), TEXT("SE"), TEXT("S"), TEXT("SW"), TEXT("W"), TEXT("NW") };
		for (int32 i = 0; i < 16; ++i)
		{
			bool bVis = false;
			const float X = XFor(i * 22.5f, bVis);
			if (!bVis) continue;
			if (i % 2 == 0)
			{
				const FText T = FText::FromString(Names[i / 2]);
				const FSlateFontInfo Font = WRUI::Title(i == 0 ? 17 : 13);
				const FVector2D S = Measure(T, Font);
				DrawTextAt(Out, Layer + 1, Geo, T, Font, FVector2D(X - S.X * 0.5f, Size.Y * 0.5f - S.Y * 0.5f), i == 0 ? WRUI::Brass : WRUI::Ink2);
			}
			else
			{
				FSlateDrawElement::MakeBox(Out, Layer + 1, Geo.ToPaintGeometry(FVector2D(2.f, 9.f), FSlateLayoutTransform(FVector2D(X - 1.f, Size.Y * 0.5f - 4.f))), WRUI::White(), ESlateDrawEffect::None, WRUI::Alpha(WRUI::Ink3, 0.6f));
			}
		}
		// Places: a coloured marker in the strip; the camp or boss nearest the middle gets its name and distance under it.
		const FWRPoi* Nearest = nullptr;
		float NearestOff = Size.X * 0.16f;
		for (const FWRPoi& Poi : FWRPoi::All)
		{
			const FVector D = Poi.Location - Pawn->GetActorLocation();
			bool bVis = false;
			const float X = XFor(FMath::RadiansToDegrees(FMath::Atan2(D.X, -D.Y)), bVis);
			if (bVis && (Poi.Kind.Contains(TEXT("boss")) || Poi.Kind == TEXT("camp")) && FMath::Abs(X - Size.X * 0.5f) < NearestOff)
			{
				NearestOff = FMath::Abs(X - Size.X * 0.5f);
				Nearest = &Poi;
			}
		}
		for (const FWRPoi& Poi : FWRPoi::All)
		{
			const FVector D = Poi.Location - Pawn->GetActorLocation();
			bool bVis = false;
			const float X = XFor(FMath::RadiansToDegrees(FMath::Atan2(D.X, -D.Y)), bVis);
			if (!bVis) continue;
			const bool bBoss = Poi.Kind.Contains(TEXT("boss"));
			const FLinearColor C = bBoss ? WRUI::Bad : Poi.Kind == TEXT("camp") ? WRUI::Good : Poi.Kind == TEXT("cave") ? WRUI::Ink3 : WRUI::Warn;
			const float R = bBoss ? 7.f : 5.f;
			WRUI::DrawBox(Out, Layer + 3, Geo.ToPaintGeometry(FVector2D(R * 2.f, R * 2.f), FSlateLayoutTransform(FVector2D(X - R, 4.f))), WRUI::Box(C, R, FLinearColor::Black, 1.f));
			if (&Poi == Nearest)
			{
				const float Km = D.Size2D() / 100000.f;
				const FText T = FText::FromString(FString::Printf(TEXT("%s · %s"), *Poi.Name, Km < 1.f ? *FString::Printf(TEXT("%.0f m"), Km * 1000.f) : *FString::Printf(TEXT("%.1f km"), Km)));
				const FSlateFontInfo Font = WRUI::Body(12, 700);
				const FVector2D S = Measure(T, Font);
				const FVector2D Pos(X - S.X * 0.5f, Size.Y + 6.f);
				WRUI::DrawBox(Out, Layer + 2, Geo.ToPaintGeometry(S + FVector2D(16.f, 4.f), FSlateLayoutTransform(Pos - FVector2D(8.f, 2.f))), WRUI::Box(FLinearColor(0.05f, 0.04f, 0.035f, 0.72f), 9.f, WRUI::Alpha(C, 0.5f), 1.f));
				DrawTextAt(Out, Layer + 3, Geo, T, Font, Pos, C.Desaturate(0.3f) * 1.1f, false);
			}
		}
		// Your belongings (the death screen's bag) while they wait: a warm marker with its distance.
		FVector Bag = FVector::ZeroVector;
		const FWRPlayerUI* UI = P->GetUI();
		const FWRFlow* Flow = UI ? UI->GetFlow() : nullptr;
		if (Flow && Flow->GetBagMarker(Bag))
		{
			const FVector D = Bag - Pawn->GetActorLocation();
			bool bVis = false;
			const float X = XFor(FMath::RadiansToDegrees(FMath::Atan2(D.X, -D.Y)), bVis);
			if (bVis)
			{
				const float R = 6.f;
				FSlateDrawElement::MakeRotatedBox(Out, Layer + 3, Geo.ToPaintGeometry(FVector2D(R * 2.f, R * 2.f), FSlateLayoutTransform(FVector2D(X - R, 4.f))), WRUI::Box(WRUI::Warn, 1.f, FLinearColor::Black, 1.f),
					ESlateDrawEffect::None, FMath::DegreesToRadians(45.f), TOptional<FVector2f>(), FSlateDrawElement::RelativeToElement, WRUI::Warn);
				const FText T = FText::FromString(FString::Printf(TEXT("Your belongings · %s"), D.Size2D() < 100000.f ? *FString::Printf(TEXT("%.0f m"), D.Size2D() / 100.f) : *FString::Printf(TEXT("%.1f km"), D.Size2D() / 100000.f)));
				const FSlateFontInfo Font = WRUI::Body(12, 700);
				const FVector2D S = Measure(T, Font);
				const FVector2D Pos(X - S.X * 0.5f, Size.Y + 30.f);
				WRUI::DrawBox(Out, Layer + 2, Geo.ToPaintGeometry(S + FVector2D(16.f, 4.f), FSlateLayoutTransform(Pos - FVector2D(8.f, 2.f))), WRUI::Box(FLinearColor(0.05f, 0.04f, 0.035f, 0.72f), 9.f, WRUI::Alpha(WRUI::Warn, 0.5f), 1.f));
				DrawTextAt(Out, Layer + 3, Geo, T, Font, Pos, WRUI::Warn, false);
			}
		}
		// Your waypoint (right-click on the map): a brass pin and its distance; behind you it waits at the strip's nearer
		// end with an arrow, so you know which way to turn.
		FVector Way;
		if (WRUIMap::GetWaypoint(P->GetWorld(), Way))
		{
			const FVector D = Way - Pawn->GetActorLocation();
			const float Deg = FMath::RadiansToDegrees(FMath::Atan2(D.X, -D.Y));
			bool bVis = false;
			float X = XFor(Deg, bVis);
			const bool bLeft = FMath::UnwindDegrees(Deg - Heading) < 0.f;
			if (!bVis) X = bLeft ? 22.f : Size.X - 22.f;
			const float R = 7.f;
			WRUI::DrawBox(Out, Layer + 3, Geo.ToPaintGeometry(FVector2D(R * 2.f, R * 2.f), FSlateLayoutTransform(FVector2D(X - R, 3.f))), WRUI::Box(WRUI::Brass, R, FLinearColor::Black, 1.5f));
			const float M = D.Size2D() / 100.f;
			const FString Far = M < 1000.f ? FString::Printf(TEXT("%.0f m"), M) : FString::Printf(TEXT("%.1f km"), M / 1000.f);
			const FText T = FText::FromString(bVis ? FString::Printf(TEXT("Waypoint · %s"), *Far) : bLeft ? FString::Printf(TEXT("‹ Waypoint · %s"), *Far) : FString::Printf(TEXT("Waypoint · %s ›"), *Far));
			const FSlateFontInfo Font = WRUI::Body(12, 700);
			const FVector2D S = Measure(T, Font);
			const float Row = Size.Y + (Flow && Flow->GetBagMarker(Bag) ? 54.f : 30.f);
			const FVector2D Pos(FMath::Clamp(X - S.X * 0.5f, 0.f, Size.X - S.X), Row);
			WRUI::DrawBox(Out, Layer + 2, Geo.ToPaintGeometry(S + FVector2D(16.f, 4.f), FSlateLayoutTransform(Pos - FVector2D(8.f, 2.f))), WRUI::Box(FLinearColor(0.05f, 0.04f, 0.035f, 0.72f), 9.f, WRUI::Alpha(WRUI::Brass, 0.55f), 1.f));
			DrawTextAt(Out, Layer + 3, Geo, T, Font, Pos, FLinearColor(FColor(0xF0, 0xD4, 0x9B)), false);
		}
		FSlateDrawElement::MakeBox(Out, Layer + 4, Geo.ToPaintGeometry(FVector2D(2.f, 10.f), FSlateLayoutTransform(FVector2D(Size.X * 0.5f - 1.f, Size.Y - 10.f))), WRUI::White(), ESlateDrawEffect::None, WRUI::Brass);
		return Layer + 5;
	}
private:
	TWeakObjectPtr<AWRPlayerController> PC;
};

/** A ring gauge (food, water, Heat): a dark ring and the filled arc clockwise from the top. */
class SWRRing : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SWRRing) : _Diameter(34.f * U), _Thickness(4.f * U) {}
		SLATE_ATTRIBUTE(float, Fraction)
		SLATE_ATTRIBUTE(FLinearColor, Color)
		SLATE_ARGUMENT(float, Diameter)
		SLATE_ARGUMENT(float, Thickness)
	SLATE_END_ARGS()
	void Construct(const FArguments& InArgs) { Fraction = InArgs._Fraction; Color = InArgs._Color; Diameter = InArgs._Diameter; Thickness = InArgs._Thickness; }
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(Diameter, Diameter); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override
	{
		const FVector2D C = Geo.GetLocalSize() * 0.5f;
		const float R = FMath::Min(C.X, C.Y) - Thickness * 0.5f;
		const auto Arc = [&](float From, float To, const FLinearColor& Col, float Thick, int32 L)
		{
			TArray<FVector2D> Pts;
			const int32 N = FMath::Max(2, FMath::CeilToInt((To - From) / 6.f));
			for (int32 i = 0; i <= N; ++i)
			{
				const float A = FMath::DegreesToRadians(From + (To - From) * i / N);
				Pts.Add(C + FVector2D(FMath::Sin(A), -FMath::Cos(A)) * R);
			}
			FSlateDrawElement::MakeLines(Out, L, Geo.ToPaintGeometry(), Pts, ESlateDrawEffect::None, Col, true, Thick);
		};
		Arc(0.f, 360.f, FLinearColor(0.f, 0.f, 0.f, 0.85f), Thickness + 2.f, Layer);
		const float F = FMath::Clamp(Fraction.Get(), 0.f, 1.f);
		if (F > 0.005f) Arc(0.f, 360.f * F, Color.Get(), Thickness, Layer + 1);
		return Layer + 1;
	}
private:
	TAttribute<float> Fraction;
	TAttribute<FLinearColor> Color;
	float Diameter = 34.f;
	float Thickness = 4.f;
};

class SWRHud : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWRHud) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AWRPlayerController>, PC)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		PC = InArgs._PC;
		ChildSlot
		[
			SNew(SOverlay)
			+ SOverlay::Slot()[WRFx::MakeHudOverlay(PC)]   // damage numbers, hit marker, parry flash (under the panels)
			+ SOverlay::Slot()[SleepCover()]               // asleep: the view dims, a bar fills (WRSurvival)
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(24.f * U, 20.f * U)[LevelPlate()]
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(24.f * U, 104.f * U)[RaidTracker()]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0.f, 18.f * U)[CompassAndBoss()]
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(24.f * U, 20.f * U)[ClockPlate()]
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(24.f * U, 150.f * U)[TamingPanel()]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0.f, 0.f, 0.f, 0.f)[Crosshair()]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0.f, 250.f * U)[CentreNotice()]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 0.f, 330.f * U)[Prompt()]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 0.f, 175.f * U)[Pickups()]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 0.f, 150.f * U)[HazardLine()]
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(26.f * U, 0.f, 0.f, 210.f * U)[ChatLines()]
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(24.f * U)[Vitals()]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 0.f, 24.f * U)[HotbarArea()]
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 24.f * U, 24.f * U)[AbilityPlate()]
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 24.f * U, 140.f * U)[TameCard()]
		];
	}

private:
	TWeakObjectPtr<AWRPlayerController> PC;

	AWRCharacter* Survivor() const { return PC.IsValid() ? Cast<AWRCharacter>(PC->GetPawn()) : nullptr; }
	AWRCharacterBase* Body() const { return PC.IsValid() ? Cast<AWRCharacterBase>(PC->GetPawn()) : nullptr; }
	const UWRAttributeSet* Attr() const { const AWRCharacterBase* B = Body(); return B ? B->GetAttributes() : nullptr; }
	FWRPlayerUI* UI() const { return PC.IsValid() ? PC->GetUI() : nullptr; }
	EVisibility IfSurvivor() const { return Survivor() && !BuildMode() ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; }
	// The taming screen is a side panel over the game: vitals, hotbar and pickups stay up beside it (Hidden), while
	// what it would overlap or repeat (prompt, crosshair, the right-hand cards) goes for any screen (Covered).
	bool Hidden() const { const FWRPlayerUI* X = UI(); return !X || (X->IsScreenOpen() && X->GetScreen() != TEXT("taming")); }
	bool Covered() const { const FWRPlayerUI* X = UI(); return !X || X->IsScreenOpen(); }
	/** An ability or tame wheel covers the centre: text that sits behind it steps aside. */
	bool WheelOpen() const { const FWRPlayerUI* X = UI(); return X && (X->GetOpenWheel() >= 0 || X->IsTameWheelOpen()); }
	/** The build bar is up: the HUD comes down to the vitals (mockup "Building"); the bar has its own banner and claim line. */
	bool BuildMode() const { const FWRPlayerUI* X = UI(); return X && X->GetScreen() == TEXT("build"); }

	// ---------------------------------------------------------------- top left: level, path, statuses
	TSharedRef<SWidget> LevelPlate()
	{
		return SNew(SBox).WidthOverride(300.f * U).Visibility_Lambda([this]() { return IfSurvivor(); })
		[
			SNew(SWRPanel).Kind(EWRPanel::Glass).Rivets(true).Padding(FMargin(14.f * U, 10.f * U))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f * U, 0.f)
				[
					SNew(SBox).WidthOverride(46.f * U).HeightOverride(46.f * U)
					[
						SNew(SOverlay)
						+ SOverlay::Slot()[SNew(SImage).Image_Lambda([this]() { return WRUI::Box(PathColor() * 0.35f, 23.f * U, WRUI::Brass2, 2.f); })]
						+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
						[
							SNew(SBox).WidthOverride(30.f * U).HeightOverride(30.f * U)
							[
								SNew(SImage).Image_Lambda([this]() { return WRUI::Icon(MainPath() == TEXT("frost") ? TEXT("frost_essence") : TEXT("ember_essence")); })
							]
						]
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f)[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { const UWRPathComponent* P = PathComp(); return FText::Format(LOCTEXT("Level", "Level {0}"), P ? P->GetCharacterLevel() : 1); }), WRUI::Title(15), WRUI::Ink)]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(MainPath().IsNone() ? TEXT("") : MainPath().ToString() + TEXT(" path")); }), WRUI::Caps(12), TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(PathColor()); }))]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 5.f * U)[SNew(SWRBar).Height(5.f * U).Color(WRUI::Brass).Fraction_Lambda([this]() { return PathXPFraction(); })]
					+ SVerticalBox::Slot().AutoHeight()
					[
						WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(StatusLine()); }), WRUI::Body(11, 700), WRUI::Warn, true)
					]
				]
			]
		];
	}

	// ---------------------------------------------------------------- under it: a raid on your claim (WRRaids)
	struct FRaidLine { uint8 Stage = 0; int32 Left = 0, Total = 0; float Seconds = 0.f; FString Way; };
	bool RaidLine(FRaidLine& L) const { return PC.IsValid() && PC->GetRaidTrack(L.Stage, L.Left, L.Total, L.Seconds, L.Way); }
	TSharedRef<SWidget> RaidTracker()
	{
		return SNew(SBox).WidthOverride(300.f * U).Visibility_Lambda([this]()
			{
				FRaidLine L;
				return Survivor() && !BuildMode() && !Hidden() && RaidLine(L) ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
			})
		[
			SNew(SWRPanel).Kind(EWRPanel::Glass).Rivets(true).Padding(FMargin(14.f * U, 9.f * U))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[WRUI::Text(LOCTEXT("RaidHead", "Raid on your base"), WRUI::Caps(12), WRUI::Bad)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						WRUI::Text(TAttribute<FText>::CreateLambda([this]() { FRaidLine L; return RaidLine(L) ? FText::FromString(Clock(L.Seconds)) : FText::GetEmpty(); }), WRUI::Title(15), WRUI::Ink)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f * U, 0.f, 0.f)
				[
					WRUI::Text(TAttribute<FText>::CreateLambda([this]()
					{
						FRaidLine L;
						if (!RaidLine(L)) return FText::GetEmpty();
						if (L.Stage == 0) return FText::FromString(FString::Printf(TEXT("%d coming from the %s"), L.Total, *L.Way));
						return FText::FromString(FString::Printf(TEXT("%d of %d raiders left"), L.Left, L.Total));
					}), WRUI::Body(13, 700), WRUI::Ink, true)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 5.f * U, 0.f, 0.f)
				[
					SNew(SWRBar).Height(5.f * U).Color(WRUI::Bad).Fraction_Lambda([this]() { FRaidLine L; return RaidLine(L) && L.Stage == 1 && L.Total > 0 ? float(L.Left) / float(L.Total) : 1.f; })
				]
			]
		];
	}

	const UWRPathComponent* PathComp() const { const AWRCharacter* S = Survivor(); return S ? UWRPathComponent::FindFor(S) : nullptr; }
	FName MainPath() const { const UWRPathComponent* P = PathComp(); return P ? P->GetMainPath() : NAME_None; }
	FLinearColor PathColor() const { return MainPath() == TEXT("frost") ? WRUI::Frost : WRUI::Ember; }
	float PathXPFraction() const
	{
		const UWRPathComponent* P = PathComp();
		const FWRPathProgress* Prog = P ? P->GetProgress(P->GetMainPath()) : nullptr;
		if (!Prog) return 0.f;
		const int32 Pts = FWRPathRules::PointsForXP(Prog->XP);
		const int32 From = FWRPathRules::XPForPoints(Pts), To = FWRPathRules::XPForPoints(Pts + 1);
		return To > From ? float(Prog->XP - From) / float(To - From) : 1.f;
	}
	FString StatusLine() const
	{
		const AWRCharacterBase* B = Body();
		const UAbilitySystemComponent* ASC = B ? B->GetAbilitySystemComponent() : nullptr;
		FString Line;
		const TPair<FGameplayTag, const TCHAR*> Names[] = {
			{ WRTags::Status_Burn, TEXT("Burning") }, { WRTags::Status_Frostbite, TEXT("Frostbite") }, { WRTags::Status_Slowed, TEXT("Slowed") },
			{ WRTags::Status_Frozen, TEXT("Frozen") }, { WRTags::Status_Staggered, TEXT("Staggered") } };
		for (const auto& N : Names) if (ASC && ASC->HasMatchingGameplayTag(N.Key)) Line += FString(N.Value) + TEXT("   ");
		const AWRPlayerState* PS = PC.IsValid() ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
		if (PS && PS->GetInventory() && PS->GetInventory()->GetCarryWeight() > WRTune(TEXT("inventory.carryKg"), 80.f)) Line += TEXT("Overloaded   ");
		return Line;
	}

	// ---------------------------------------------------------------- top centre: compass, boss bar
	TSharedRef<SWidget> CompassAndBoss()
	{
		return SNew(SVerticalBox).Visibility_Lambda([this]() { return Survivor() && !BuildMode() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[SNew(SBox).WidthOverride(560.f * U).HeightOverride(46.f * U)[SNew(SWRPanel).Kind(EWRPanel::Glass).Padding(0.f)[SNullWidget::NullWidget]]]
				+ SOverlay::Slot()[SNew(SWRCompass).PC(PC)]
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 40.f * U, 0.f, 0.f)
			[
				SNew(SBox).WidthOverride(640.f * U).Visibility_Lambda([this]() { return EngagedBoss() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[
						WRUI::Text(TAttribute<FText>::CreateLambda([this]()
						{
							const AWRBoss* B = EngagedBoss();
							return B ? FText::FromString(FString::Printf(TEXT("%s   ·   phase %d%s"), *B->GetDisplayLabel(), B->Phase, B->Participants > 1 ? *FString::Printf(TEXT("   ·   %d fighters"), B->Participants) : TEXT(""))) : FText::GetEmpty();
						}), WRUI::Title(17), WRUI::Ink, true)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 0.f)
					[
						SNew(SWRBar).Height(16.f * U).Color(FLinearColor(0.72f, 0.12f, 0.08f)).Fraction_Lambda([this]()
						{
							const AWRBoss* B = EngagedBoss();
							const UWRAttributeSet* A = B ? B->GetAttributes() : nullptr;
							return A ? A->GetHealth() / FMath::Max(1.f, A->GetMaxHealth()) : 0.f;
						})
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f * U, 0.f, 0.f)
					[
						SNew(SWRBar).Height(5.f * U).Color(WRUI::Poise).Fraction_Lambda([this]() { const AWRBoss* B = EngagedBoss(); return B ? B->ShellFraction : 0.f; })
						.Visibility_Lambda([this]() { const AWRBoss* B = EngagedBoss(); return B && B->ShellFraction > 0.f ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
					]
				]
			];
	}

	const AWRBoss* EngagedBoss() const
	{
		const APawn* Me = PC.IsValid() ? PC->GetPawn() : nullptr;
		if (!Me) return nullptr;
		for (TActorIterator<AWRBoss> It(Me->GetWorld()); It; ++It)
		{
			if (It->bEngaged && It->IsAlive() && FVector::Dist(It->GetActorLocation(), Me->GetActorLocation()) < 4500.f) return *It;
		}
		return nullptr;
	}

	// ---------------------------------------------------------------- top right: clock, help hint
	TSharedRef<SWidget> ClockPlate()
	{
		return SNew(SVerticalBox).Visibility_Lambda([this]() { return IfSurvivor(); })
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBox).WidthOverride(190.f * U)
				[
					SNew(SWRPanel).Kind(EWRPanel::Glass).Rivets(true).Padding(FMargin(16.f * U, 9.f * U))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f * U, 0.f)
						[
							SNew(SBox).WidthOverride(26.f * U).HeightOverride(26.f * U)
							[
								SNew(SImage).Image_Lambda([this]() { const bool bNight = IsNight(); return WRUI::Box(bNight ? FLinearColor(0.75f, 0.82f, 0.95f) : WRUI::Warn, 13.f * U, bNight ? FLinearColor(0.3f, 0.35f, 0.5f) : FLinearColor(0.35f, 0.25f, 0.06f), 2.f); })
							]
						]
						+ SHorizontalBox::Slot().FillWidth(1.f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { const AWRGameState* G = AWRGameState::Get(PC.Get()); return FText::Format(LOCTEXT("Day", "Day {0}"), G ? G->GetDay() : 1); }), WRUI::Caps(12), WRUI::Ink2)]
							+ SVerticalBox::Slot().AutoHeight()
							[
								WRUI::Text(TAttribute<FText>::CreateLambda([this]()
								{
									const AWRGameState* G = AWRGameState::Get(PC.Get());
									const float H = G ? G->GetHour() : 8.f;
									return FText::FromString(FString::Printf(TEXT("%02d:%02d"), FMath::FloorToInt(H), FMath::FloorToInt(H * 60.f) % 60));
								}), WRUI::Title(19), TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(IsNight() ? WRUI::Frost : WRUI::Ink); }))
							]
						]
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.f, 8.f * U, 2.f * U, 0.f)
			[
				// (F1 on the keyboard; a pad opens the page with Y from any menu, so the hint is the keyboard's only)
				SNew(SHorizontalBox).Visibility_Lambda([]() { return WRInput::UsingPad() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
				+ SHorizontalBox::Slot().AutoWidth()[SNew(SWRKeyCap).Text_Lambda([]() { return WRInput::ActionKey(TEXT("Help")); }).FontSize(11)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(7.f, 0.f, 0.f, 0.f)[WRUI::Text(LOCTEXT("Controls", "Controls"), WRUI::Body(12, 700), WRUI::Ink2, true)]
			];
	}

	bool IsNight() const { const AWRGameState* G = AWRGameState::Get(PC.Get()); return G && G->IsNight(); }

	// ---------------------------------------------------------------- right: taming panel (a knocked-out creature near you)
	const AWRCreature* KnockedOut() const
	{
		const APawn* Me = PC.IsValid() ? PC->GetPawn() : nullptr;
		if (!Me) return nullptr;
		const AWRCreature* Best = nullptr;
		float BestDist = 1500.f;
		for (TActorIterator<AWRCreature> It(Me->GetWorld()); It; ++It)
		{
			const float D = FVector::Dist(It->GetActorLocation(), Me->GetActorLocation());
			if (It->IsAlive() && !It->IsTamed() && It->IsUnconscious() && D < BestDist) { BestDist = D; Best = *It; }
		}
		return Best;
	}

	TSharedRef<SWidget> TamingPanel()
	{
		return SNew(SBox).WidthOverride(330.f * U).Visibility_Lambda([this]() { return !Covered() && KnockedOut() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		[
			SNew(SWRPanel).Kind(EWRPanel::Glass).Rivets(true).Padding(FMargin(16.f * U, 12.f * U))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 12.f * U, 0.f)
					[
						SNew(SBox).WidthOverride(56.f * U).HeightOverride(56.f * U)
						[
							SNew(SOverlay)
							+ SOverlay::Slot()[SNew(SImage).Image(WRUI::Box(FLinearColor(0.1f, 0.1f, 0.11f), 4.f, WRUI::Brass2, 2.f))]
							+ SOverlay::Slot().Padding(2.f)[SNew(SImage).Image_Lambda([this]() { const AWRCreature* C = KnockedOut(); const FSlateBrush* B = C ? WRUI::Portrait(C->CreatureId) : nullptr; return B ? B : WRUI::Icon(TEXT("raw_meat")); })]
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { const AWRCreature* C = KnockedOut(); return C ? FText::FromString(C->GetDisplayLabel()) : FText::GetEmpty(); }), WRUI::Title(16), WRUI::Ink)]
						+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(LOCTEXT("KO", "Knocked out"), WRUI::Caps(12), WRUI::Torpor)]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f * U, 0.f, 0.f)[LabelledBar(LOCTEXT("Torpor", "Torpor"), WRUI::Torpor, [this]() { const AWRCreature* C = KnockedOut(); return C && C->MaxTorpor > 0.f ? C->Torpor / C->MaxTorpor : 0.f; },
					[this]() { const AWRCreature* C = KnockedOut(); return C ? FText::FromString(TEXT("wakes in ") + Clock(C->WakeIn)) : FText::GetEmpty(); })]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 0.f)[LabelledBar(LOCTEXT("Tamed", "Tamed"), WRUI::Good, [this]() { const AWRCreature* C = KnockedOut(); return C ? C->TameProgress : 0.f; },
					[this]() { const AWRCreature* C = KnockedOut(); return C ? FText::FromString(FString::Printf(TEXT("%.0f%%"), C->TameProgress * 100.f)) : FText::GetEmpty(); })]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f * U, 0.f, 0.f)
				[
					WRUI::Text(TAttribute<FText>::CreateLambda([this]()
					{
						const AWRCreature* C = KnockedOut();
						return C ? FText::FromString(FString::Printf(TEXT("Effectiveness %.0f%%   ·   Stash %d"), C->TameEffectiveness * 100.f, C->TameFoodCount)) : FText::GetEmpty();
					}), WRUI::Body(13, 500), WRUI::Ink2)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f * U, 0.f, 0.f)
				[
					WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(TamingAdvice()); }), WRUI::Body(13, 700), TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(TamingAdviceColor()); }), false, true)
				]
			]
		];
	}

	FString TamingAdvice() const
	{
		const AWRCreature* C = KnockedOut();
		if (!C) return FString();
		const FWRItemDef* Food = FWRItemRegistry::Get().Find(C->TameFoodItem);
		const FString FoodName = Food ? Food->Name : FString(TEXT("food"));
		const FString Key = WRInput::ActionKey(TEXT("Interact")).ToString();
		if (C->TameFoodCount == 0 && C->MealLeft <= 0.f) return FString::Printf(TEXT("Hungry: %s to feed it %s or kibble"), *Key, *FoodName);
		if (C->TameEta < 0.f) return FString::Printf(TEXT("Needs %d more %s"), C->TameFoodNeeded, *FoodName);
		if (C->TameEta > C->WakeIn) return FString::Printf(TEXT("Tamed in about %.0f s, but it wakes first: give it a narcotic"), C->TameEta);
		return FString::Printf(TEXT("Tamed in about %.0f s"), C->TameEta);
	}
	FLinearColor TamingAdviceColor() const
	{
		const AWRCreature* C = KnockedOut();
		return !C ? WRUI::Ink : (C->TameFoodCount == 0 && C->MealLeft <= 0.f) || C->TameEta > C->WakeIn ? WRUI::Ember : C->TameEta < 0.f ? WRUI::Warn : WRUI::Good;
	}

	TSharedRef<SWidget> LabelledBar(const FText& Label, const FLinearColor& Color, TFunction<float()> Fraction, TFunction<FText()> Right)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(64.f * U)[WRUI::Text(Label, WRUI::Caps(12), WRUI::Ink2)]]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[SNew(SWRBar).Height(12.f * U).Color(Color).Fraction_Lambda(MoveTemp(Fraction))]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f * U, 0.f, 0.f, 0.f)[SNew(SBox).MinDesiredWidth(76.f * U)[WRUI::Text(TAttribute<FText>::CreateLambda(MoveTemp(Right)), WRUI::Body(12, 700), WRUI::Ink2)]];
	}

	// ---------------------------------------------------------------- centre: crosshair, notice, prompt, pickups, hazards
	TSharedRef<SWidget> Crosshair()
	{
		// A dot and four short ticks at the screen's middle, where spells, shots and the build ghost go. A thin dark edge keeps
		// them readable on snow and sky alike; red over something hostile.
		TSharedRef<SOverlay> Marks = SNew(SOverlay);
		const auto Stroke = [this, &Marks](float Width, float Height, float X, float Y)
		{
			Marks->AddSlot().HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(Width + 2.f).HeightOverride(Height + 2.f)
				.RenderTransform(TOptional<FSlateRenderTransform>(FSlateRenderTransform(FVector2D(X, Y))))
				[
					SNew(SOverlay)
					+ SOverlay::Slot()[SNew(SImage).Image(WRUI::White()).ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.45f))]
					+ SOverlay::Slot().Padding(1.f)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity_Lambda([this]() { return CrosshairColor(); })]
				]
			];
		};
		Stroke(2.f, 2.f, 0.f, 0.f);
		Stroke(2.f, 6.f, 0.f, -9.f);
		Stroke(2.f, 6.f, 0.f, 9.f);
		Stroke(6.f, 2.f, -9.f, 0.f);
		Stroke(6.f, 2.f, 9.f, 0.f);
		return SNew(SBox).WidthOverride(36.f).HeightOverride(36.f).Visibility_Lambda([this]()
		{
			const AWRCharacter* S = Survivor();
			const AWRPlayerController* P = PC.Get();
			const bool bPlacing = P && P->IsPlacing();
			const bool bWanted = FWRKeyBindings::Get().ShowCrosshair() || (S && (S->IsAimingRanged() || S->IsFirstPerson())) || bPlacing;
			return S && (!Covered() || bPlacing) && !WheelOpen() && bWanted ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
		})
		[
			Marks
		];
	}

	FSlateColor CrosshairColor() const
	{
		const AWRPlayerController* P = PC.Get();
		const bool bRed = bAimOnFoe || (P && P->IsDemolishing() && P->IsPlacementValid());
		return bRed ? FLinearColor(1.f, 0.28f, 0.16f, 1.f) : FLinearColor(1.f, 1.f, 1.f, 0.9f);
	}
	bool bAimOnFoe = false;

	/** Is something hostile right under the crosshair? */
	void UpdateCrosshair()
	{
		bAimOnFoe = false;
		const AWRPlayerController* P = PC.Get();
		const AWRCharacter* S = Survivor();
		if (!P || !S || !P->PlayerCameraManager || Covered())
		{
			return;
		}
		const FVector Eye = P->PlayerCameraManager->GetCameraLocation();
		const FVector Dir = P->PlayerCameraManager->GetCameraRotation().Vector();
		const float Along = FMath::Max(0.f, static_cast<float>(FVector::DotProduct(S->GetActorLocation() - Eye, Dir)));
		FCollisionQueryParams Query(SCENE_QUERY_STAT(WRCrosshair), false, S);
		FCollisionObjectQueryParams Objects;
		Objects.AddObjectTypesToQuery(ECC_WorldStatic);
		Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
		Objects.AddObjectTypesToQuery(ECC_Pawn);
		FHitResult Hit;
		if (S->GetWorld()->LineTraceSingleByObjectType(Hit, Eye + Dir * Along, Eye + Dir * (Along + WRTune(TEXT("ui.crosshairFoeRange"), 4000.f)), Objects, Query))
		{
			const AWRCharacterBase* Who = Cast<AWRCharacterBase>(Hit.GetActor());
			bAimOnFoe = Who && Who->IsAlive() && !Who->IsHidden() && S->IsHostileTo(Who);
		}
	}

	TSharedRef<SWidget> CentreNotice()
	{
		// Notices queue in the UI (each gets its turn) and ease in and out rather than snapping.
		const auto Alpha = [this]() { float A = 0.f; if (const FWRPlayerUI* X = UI()) X->GetCentreNotice(A); return A; };
		// A banner (level up, a boss's or a raid's card) sits where the notice does: the notice steps down under it
		// meanwhile, or the one showed through the other.
		const auto Under = [this]() { float A = 0.f; const FWRPlayerUI* X = UI(); const FWRFlow* F = X ? X->GetFlow() : nullptr; return F && F->GetBanner(A) ? A * 150.f * U : 0.f; };
		TSharedRef<SBox> Box = SNew(SBox).Visibility_Lambda([Alpha]() { return Alpha() > 0.f ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			.RenderTransform_Lambda([Alpha, Under]() { return TOptional<FSlateRenderTransform>(FSlateRenderTransform(FVector2D(0.f, (1.f - Alpha()) * -8.f + Under()))); })
			[
				SNew(SWRPanel).Kind(EWRPanel::Glass).Padding(FMargin(18.f * U, 7.f * U))
				[
					WRUI::Text(TAttribute<FText>::CreateLambda([this]() { float A = 0.f; const FWRPlayerUI* X = UI(); return X ? FText::FromString(X->GetCentreNotice(A)) : FText::GetEmpty(); }), WRUI::Body(16, 700), WRUI::Ink)
				]
			];
		NoticeBox = Box;
		return Box;
	}
	TSharedPtr<SBox> NoticeBox;

	virtual void Tick(const FGeometry& Geo, const double Time, const float Delta) override
	{
		SCompoundWidget::Tick(Geo, Time, Delta);
		UpdateCrosshair();
		if (NoticeBox.IsValid())
		{
			float A = 0.f;
			if (const FWRPlayerUI* X = UI()) X->GetCentreNotice(A);
			NoticeBox->SetRenderOpacity(A);
		}
	}

	FString PromptText(FString& OutTarget) const
	{
		AWRPlayerController* P = PC.Get();
		const AWRCharacter* S = Survivor();
		if (!P || !S || P->GetMenu() != EWRMenu::None || P->IsPlacing()) return FString();
		// The critical, the first time for each kind of creature (then never again: COMBAT-AND-LESSONS L6).
		if (const AWRCharacterBase* Crit = S->CritPromptTarget()) { OutTarget = Crit->GetDisplayLabel(); return TEXT("Critical"); }
		if (const AWRBossAltar* Altar = P->FindNearbyAltar()) { OutTarget = TEXT("Boss altar"); return Altar->GetPrompt(P).Replace(TEXT("E: "), TEXT("")); }
		for (TActorIterator<AWRRunbackStone> It(P->GetWorld()); It; ++It)
		{
			// A shrine-stone by a boss arena: rest there and you wake there (cooks/build-combat-full.md 2.16).
			if (FVector::Dist(It->GetActorLocation(), S->GetActorLocation()) < 300.f) { OutTarget = TEXT("Shrine-stone"); return TEXT("Rest (wake here)"); }
		}
		for (TActorIterator<AWRLaidEgg> It(P->GetWorld()); It; ++It)
		{
			// A fertilised egg in its scrape (cooks/build-taming.md 2.6).
			if (FVector::Dist(It->GetActorLocation(), S->GetActorLocation()) < 300.f) { const FWRItemDef* E = FWRItemRegistry::Get().Find(It->Egg.ItemId); OutTarget = E ? E->Name : TEXT("Egg"); return TEXT("Take egg"); }
		}
		if (const AWRCreature* Tame = P->FindNearbyCreature(true))
		{
			// A baby asking for care says so (its card's Answer, from the wheel's Tend).
			static const TCHAR* Asks[] = { TEXT(""), TEXT("hungry"), TEXT("wants a walk"), TEXT("wants a cuddle") };
			OutTarget = Tame->TameRep.Care >= 1 && Tame->TameRep.Care <= 3 ? FString::Printf(TEXT("%s · %s"), *Tame->GetDisplayLabel(), Asks[Tame->TameRep.Care]) : Tame->GetDisplayLabel();
			return Tame->TameRep.Care >= 1 && Tame->TameRep.Care <= 3 ? TEXT("Hold: orders  ·  Tend: care") : TEXT("Hold: orders  ·  Tap: interact");
		}
		if (const AWRCreature* Wild = P->FindNearbyCreature(false, 400.f))
		{
			if (Wild->IsUnconscious()) { OutTarget = Wild->GetDisplayLabel(); return TEXT("Tame"); }
		}
		if (const AActor* Use = P->FindUseTarget())
		{
			if (const AWRStation* St = Cast<AWRStation>(Use)) { OutTarget = St->GetDisplayName(); return FWRFarm::IsCookStation(St->StationType, S) ? TEXT("Cook") : TEXT("Use"); }
			if (const AWRBuildPiece* Piece = Cast<AWRBuildPiece>(Use))
			{
				const FWRPieceDef* Def = Piece->GetDef();
				if (!Def) return FString();
				OutTarget = Def->Name;
				if (Def->Kind == TEXT("door"))
				{
					// A code-lock door: its keypad when it's locked to you; your own one also has its keypad on a hold.
					if (Def->bLockable && !Piece->SeemsOpenable(P)) return TEXT("Enter code");
					const TCHAR* Act = Piece->bOpen ? TEXT("Close") : TEXT("Open");
					return Def->bLockable && Piece->SeemsShared(P) ? FString::Printf(TEXT("%s  ·  Hold: code lock"), Act) : FString(Act);
				}
				if (Def->Kind == TEXT("chest") && Def->Cold > 0.f)
				{
					// An ice box: whether it's cold (ice burning, or the tundra's ground) or needs ice.
					OutTarget = Piece->Feed > 0 ? FString::Printf(TEXT("%s · cold: food keeps 5x as long"), *Def->Name) : FString::Printf(TEXT("%s · no ice: put ice blocks in"), *Def->Name);
					return TEXT("Open");
				}
				if (Def->Kind == TEXT("chest")) return TEXT("Open");
				if (Def->Kind == TEXT("totem")) return TEXT("Land claim");
				if (Def->Kind == TEXT("incubator"))
				{
					// An incubator: what's in it and how it's doing on the second line.
					const FWRItemDef* E = Piece->Storage.Num() > 0 && !Piece->Storage[0].IsEmpty() ? FWRItemRegistry::Get().Find(Piece->Storage[0].ItemId) : nullptr;
					OutTarget = !E ? FString::Printf(TEXT("%s · empty"), *Def->Name)
						: Piece->FarmStop == 1 ? FString::Printf(TEXT("%s · %s, cold: needs fuel"), *Def->Name, *E->Name)
						: Piece->FarmStop == 2 ? FString::Printf(TEXT("%s · ready: make room under your tame limit"), *Def->Name)
						: FString::Printf(TEXT("%s · %s %.0f%%"), *Def->Name, *E->Name, Piece->Growth8 / 2.55f);
					return TEXT("Incubator");
				}
				if (FWRMachines::IsMachine(*Def))
				{
					// A machine: what it's doing on the second line (cooks/build-machinery.md 2.7).
					OutTarget = FString::Printf(TEXT("%s · %s"), *Def->Name, *FWRMachines::StateText(Piece));
					return TEXT("Machine");
				}
				if (Def->IsFarm())
				{
					// The farm piece's state on the second line (WRFarm::Describe), what E does on the first.
					OutTarget = FWRFarm::Describe(Piece, P);
					const AWRPlayerState* MyPS = P->GetPlayerState<AWRPlayerState>();
					const UWRInventoryComponent* Inv = MyPS ? MyPS->GetInventory() : nullptr;
					if (Def->Kind == TEXT("crop"))
					{
						if (Piece->CropId.IsNone()) return TEXT("Plant");
						if (Piece->Growth8 >= 255) return TEXT("Harvest");
						if (!(Piece->FarmFlags & WRFarmFlag::Fertilised) && Inv && (Inv->CountOf(TEXT("compost")) > 0 || Inv->CountOf(TEXT("bone_meal")) > 0 || Inv->CountOf(TEXT("stone_dust")) > 0)) return TEXT("Fertilise");
						return TEXT("Growing");
					}
					if (Def->Kind == TEXT("pen")) return TEXT("Feed · collect");
					if (Def->Kind == TEXT("well") || Def->Kind == TEXT("channel")) return TEXT("Water");
					return TEXT("Collect");
				}
				if (Def->Kind == TEXT("bed")) return TEXT("Sleep · set respawn");
				return TEXT("Use");
			}
		}
		if (const AWRHearth* Hearth = P->FindNearbyHearth()) { OutTarget = FString::Printf(TEXT("Hearth · tech level %d"), Hearth->GetTechLevel()); return TEXT("Hearth"); }
		// In the water or at its edge (WRWater), as the server decides it: E drinks.
		if (const UWRWaterSubsystem* Water = UWRWaterSubsystem::Get(P); Water && Water->CanDrink(S)) { OutTarget = TEXT("Sea water"); return TEXT("Drink"); }
		return FString();
	}

	TSharedRef<SWidget> Prompt()
	{
		return SNew(SBox).Visibility_Lambda([this]() { FString T; return !Covered() && !WheelOpen() && !PromptText(T).IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		[
			SNew(SWRPanel).Kind(EWRPanel::Glass).Padding(FMargin(12.f * U, 8.f * U))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f * U, 0.f)[SNew(SWRKeyCap).FontSize(14).Text_Lambda([this]()
				{
					// The crit is Light; everything else in this slot is Interact. The device in use names the button.
					const AWRCharacter* S = Survivor();
					return WRInput::ActionKey(S && S->CritPromptTarget() ? TEXT("Light") : TEXT("Interact"));
				})]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { FString T; return FText::FromString(PromptText(T)); }), WRUI::Body(15, 700), WRUI::Ink)]
					+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { FString T; PromptText(T); return FText::FromString(T); }), WRUI::Body(12, 500), WRUI::Ink3)]
				]
			]
		];
	}

	TSharedRef<SWidget> Pickups()
	{
		// The newest line slides up into the bottom row and the stack eases up to make room; a line that stacks
		// more of its item ("+3 Wood" -> "+5 Wood") gives a little pulse; old lines fade.
		const float RowH = 23.f * U;
		const auto Entry = [this](int32 i) -> const FWRPlayerUI::FPickup*
		{
			const FWRPlayerUI* X = UI();
			if (!X) return nullptr;
			const TArray<FWRPlayerUI::FPickup>& P = X->GetPickups();
			const int32 Idx = P.Num() - 5 + i;
			return P.IsValidIndex(Idx) ? &P[Idx] : nullptr;
		};
		TSharedRef<SVerticalBox> Box = SNew(SVerticalBox).Visibility_Lambda([this]() { return Hidden() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
			.RenderTransform_Lambda([this, RowH]()
			{
				const FWRPlayerUI* X = UI();
				const float T = X ? WRUIMotion::EaseOut(float(WRUIMotion::Now() - X->GetPickupAddedAt()) / 0.18f) : 1.f;
				return TOptional<FSlateRenderTransform>(FSlateRenderTransform(FVector2D(0.f, (1.f - T) * RowH)));
			});
		for (int32 i = 0; i < 5; ++i)
		{
			Box->AddSlot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(SBox).HeightOverride(RowH).VAlign(VAlign_Center)
				.RenderTransformPivot(FVector2D(0.5f, 0.5f))
				.RenderTransform_Lambda([Entry, i]()
				{
					const FWRPlayerUI::FPickup* P = Entry(i);
					const float Pulse = P ? 1.f - WRUIMotion::EaseOut(float(WRUIMotion::Now() - P->Bumped) / 0.22f) : 0.f;
					return TOptional<FSlateRenderTransform>(FSlateRenderTransform(FScale2D(1.f + 0.14f * Pulse)));
				})
				[
					// A soft dark pill behind each line keeps it readable on snow and sand; line and pill fade together.
					SNew(SBorder).BorderImage(WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.42f), 10.f * U)).Padding(FMargin(12.f * U, 1.f * U))
					.Visibility_Lambda([Entry, i]() { return Entry(i) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
					.ColorAndOpacity_Lambda([Entry, i]()
					{
						const FWRPlayerUI::FPickup* P = Entry(i);
						const float Age = P ? float(WRUIMotion::Now() - P->Time) : 99.f;
						const float In = i == 4 ? WRUIMotion::EaseOut(Age / 0.18f) : 1.f;
						return FLinearColor(1.f, 1.f, 1.f, FMath::Min(In, FMath::Clamp(1.f - (Age - 2.f) / 1.5f, 0.f, 1.f)));
					})
					[
						WRUI::Text(TAttribute<FText>::CreateLambda([Entry, i]()
						{
							const FWRPlayerUI::FPickup* P = Entry(i);
							return P ? FText::FromString(P->Text) : FText::GetEmpty();
						}), WRUI::Body(15, 800), WRUI::Ink, true)
					]
				]
			];
		}
		return Box;
	}

	TSharedRef<SWidget> HazardLine()
	{
		return WRUI::Text(TAttribute<FText>::CreateLambda([this]()
		{
			const AWRCharacter* S = Survivor();
			if (!S) return FText::GetEmpty();
			const FName Kind = FWRBiomeHazards::KindAt(S->GetActorLocation() - FVector(0.f, 0.f, S->GetSimpleCollisionHalfHeight()));
			if (Kind.IsNone() || FWRBiomeHazards::Resists(S, Kind)) return FText::GetEmpty();
			return Kind == TEXT("lava") ? LOCTEXT("Lava", "Lava! It burns: the fire path, fire-infused armor or a mount keeps you safe")
				: LOCTEXT("Ice", "Slick ice: the frost path or frost-infused armor gives you grip");
		}), WRUI::Body(16, 800), WRUI::Ember, true);
	}

	TSharedRef<SWidget> ChatLines()
	{
		TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
		for (int32 i = 0; i < 6; ++i)
		{
			Box->AddSlot().AutoHeight()
			[
				WRUI::Text(TAttribute<FText>::CreateLambda([this, i]()
				{
					if (!PC.IsValid()) return FText::GetEmpty();
					const TArray<AWRPlayerController::FChatLine>& C = PC->GetChat();
					const int32 Idx = C.Num() - 6 + i;
					return C.IsValidIndex(Idx) && PC->GetWorld()->GetTimeSeconds() - C[Idx].Time < 25.0 ? FText::FromString(C[Idx].Text) : FText::GetEmpty();
				}), WRUI::Body(14, 500), FLinearColor(0.85f, 0.92f, 0.98f), true)
			];
		}
		return Box;
	}

	// ---------------------------------------------------------------- bottom left: vitals
	/** 0..1: how much of the "no stamina" flash is left (a refused press, combat.md 2.3: no dead button). */
	float StaminaRefusal() const
	{
		const AWRCharacter* S = Survivor();
		const float Len = WRTune(TEXT("stamina.refuseFlashSeconds"), 0.35f);
		const float Age = S && S->GetWorld() ? float(S->GetWorld()->GetTimeSeconds() - S->StaminaRefusedAt) : 100.f;
		return Age >= 0.f && Age < Len ? 1.f - Age / Len : 0.f;
	}

	TSharedRef<SWidget> VitalRow(const FText& Label, const FLinearColor& Color, float Height, TFunction<float()> Value, TFunction<float()> Max, TFunction<float()> Flash = nullptr)
	{
		return SNew(SHorizontalBox)
			// A refused press shakes the row a few px and flashes the bar white (never gold: that's block / parry).
			.RenderTransform_Lambda([this, Flash]()
			{
				const float F = Flash ? Flash() : 0.f;
				const float Px = F > 0.f ? WRTune(TEXT("stamina.refuseShake"), 3.f) * U * F * FMath::Sin(float(WRUIMotion::Now()) * 2.f * PI * 22.f) : 0.f;
				return TOptional<FSlateRenderTransform>(FSlateRenderTransform(FVector2D(Px, 0.f)));
			})
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(62.f * U)[WRUI::Text(Label, WRUI::Caps(12), WRUI::Ink2)]]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[SNew(SWRBar).Height(Height)
				.Color_Lambda([Color, Flash]() { return Flash ? FMath::Lerp(Color, FLinearColor(FColor(245, 240, 230)), Flash()) : Color; })
				.Fraction_Lambda([Value, Max]() { return FMath::Max(0.f, Value()) / FMath::Max(1.f, Max()); })]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f * U, 0.f, 0.f, 0.f)
			[
				SNew(SBox).WidthOverride(64.f * U).HAlign(HAlign_Right)
				[
					WRUI::Text(TAttribute<FText>::CreateLambda([Value, Max]() { return FText::FromString(FString::Printf(TEXT("%.0f/%.0f"), FMath::Max(0.f, Value()), Max())); }), WRUI::Body(13, 700), WRUI::Ink)
				]
			];
	}

	TSharedRef<SWidget> Gauge(const FText& Label, const FLinearColor& Color, FName Icon, TFunction<float()> Value, TFunction<bool()> Visible = nullptr)
	{
		return SNew(SHorizontalBox).Visibility_Lambda([Visible]() { return !Visible || Visible() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[SNew(SWRRing).Color(Color).Fraction_Lambda([Value]() { return Value() / 100.f; })]
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[SNew(SBox).WidthOverride(18.f * U).HeightOverride(18.f * U)[SNew(SImage).Image(WRUI::Icon(Icon))]]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(7.f * U, 0.f, 0.f, 0.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda([Value]() { return FText::AsNumber(FMath::RoundToInt(Value())); }), WRUI::Body(13, 800), WRUI::Ink)]
				+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(Label, WRUI::Body(11, 500), WRUI::Ink3)]
			];
	}

	TSharedRef<SWidget> Vitals()
	{
		const auto A = [this]() { return Attr(); };
		return SNew(SBox).WidthOverride(360.f * U).Visibility_Lambda([this]() { return Body() && (!Hidden() || BuildMode()) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		[
			SNew(SWRPanel).Kind(EWRPanel::Leather).Rivets(true).Padding(FMargin(18.f * U, 14.f * U))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f * U)
				[
					WRUI::Text(TAttribute<FText>::CreateLambda([this]() { const AWRCreature* M = PC.IsValid() ? Cast<AWRCreature>(PC->GetPawn()) : nullptr; return M ? FText::Format(LOCTEXT("Riding", "Riding {0}  ·  {1} to dismount"), FText::FromString(M->GetDisplayLabel()), WRInput::ActionKey(TEXT("Interact"))) : FText::GetEmpty(); }), WRUI::Body(13, 700), WRUI::Brass)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f * U)[VitalRow(LOCTEXT("Health", "Health"), WRUI::Hp, 16.f * U, [A]() { const UWRAttributeSet* X = A(); return X ? X->GetHealth() : 0.f; }, [A]() { const UWRAttributeSet* X = A(); return X ? X->GetMaxHealth() : 1.f; })]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f * U)[SNew(SWRStatusMeters).Who_Lambda([this]() { return Cast<AWRCharacterBase>(Body()); })]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f * U)[VitalRow(LOCTEXT("Stamina", "Stamina"), WRUI::Stamina, 14.f * U, [A]() { const UWRAttributeSet* X = A(); return X ? X->GetStamina() : 0.f; }, [A]() { const UWRAttributeSet* X = A(); return X ? X->GetMaxStamina() : 1.f; }, [this]() { return StaminaRefusal(); })]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f * U)[VitalRow(LOCTEXT("Poise", "Poise"), WRUI::Poise, 6.f * U, [A]() { const UWRAttributeSet* X = A(); return X ? X->GetPoise() : 0.f; }, [A]() { const UWRAttributeSet* X = A(); return X ? X->GetMaxPoise() : 1.f; })]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f * U, 0.f, 8.f * U)[WRUI::Divider()]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox).Visibility_Lambda([this]() { return Survivor() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
					+ SHorizontalBox::Slot().FillWidth(1.f)[Gauge(LOCTEXT("Food", "Food"), WRUI::Hunger, TEXT("cooked_meat"), [A]() { const UWRAttributeSet* X = A(); return X ? X->GetHunger() : 0.f; })]
					+ SHorizontalBox::Slot().FillWidth(1.f)[Gauge(LOCTEXT("Water", "Water"), WRUI::Thirst, TEXT("waterskin"), [A]() { const UWRAttributeSet* X = A(); return X ? X->GetThirst() : 0.f; })]
					+ SHorizontalBox::Slot().FillWidth(1.f)[Gauge(LOCTEXT("Heat", "Heat"), WRUI::Ember, TEXT("ember_essence"), [A]() { const UWRAttributeSet* X = A(); return X ? 100.f * X->GetHeat() / FMath::Max(1.f, X->GetMaxHeat()) : 0.f; },
						[this]() { const UWRPathComponent* P = PathComp(); return P && P->IsActivePath(TEXT("fire")); })]
					+ SHorizontalBox::Slot().FillWidth(1.f)[Gauge(LOCTEXT("Chill", "Chill"), WRUI::Frost, TEXT("frost_essence"), [A]() { const UWRAttributeSet* X = A(); return X ? 100.f * X->GetChill() / FMath::Max(1.f, X->GetMaxChill()) : 0.f; },
						[this]() { const UWRPathComponent* P = PathComp(); return P && P->IsActivePath(TEXT("frost")); })]
				]
				+ SVerticalBox::Slot().AutoHeight()[Conditions()]
			]
		];
	}

	// ---------------------------------------------------------------- survival (WRSurvival): rested, weather, dark, corruption
	const UWRSurvivalComponent* Surv() const
	{
		const AWRPlayerState* P = PC.IsValid() ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
		return P ? P->GetSurvival() : nullptr;
	}

	const UWRFoodComponent* Food() const
	{
		const AWRPlayerState* P = PC.IsValid() ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
		return P ? P->GetFood() : nullptr;
	}

	static FString Clock(float Seconds)
	{
		const int32 S = FMath::Max(0, FMath::CeilToInt(Seconds));
		return S >= 60 ? FString::Printf(TEXT("%d min"), FMath::CeilToInt(S / 60.f)) : FString::Printf(TEXT("%d s"), S);
	}

	/** One condition under the gauges: a tinted glyph, a line, and a muted second line. */
	TSharedRef<SWidget> Condition(TFunction<bool()> Show, TFunction<FName()> Glyph, TFunction<FLinearColor()> Tint, TFunction<FText()> Line, TFunction<FText()> Sub)
	{
		return SNew(SBox).Padding(FMargin(0.f, 6.f * U, 0.f, 0.f)).Visibility_Lambda([Show]() { return Show() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(26.f * U).HeightOverride(26.f * U)
				[
					SNew(SImage).Image_Lambda([Glyph]() { return WRUI::Image(FName(*(TEXT("Glyphs/") + Glyph().ToString()))); })
					.ColorAndOpacity_Lambda([Tint]() { return FSlateColor(Tint()); })
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(9.f * U, 0.f, 0.f, 0.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda(MoveTemp(Line)), WRUI::Body(13, 800), TAttribute<FSlateColor>::CreateLambda([Tint]() { return FSlateColor(FMath::Lerp(WRUI::Ink, Tint(), 0.35f)); }))]
				+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font(WRUI::Body(11, 500)).ColorAndOpacity(WRUI::Ink3).AutoWrapText(true).Text_Lambda(MoveTemp(Sub))]
			]
		];
	}

	/** The weather of the biome you're in, when one is coming or on (and its row). */
	const FWRWeatherNow* WeatherHere(const FWRWeatherKind*& OutKind) const
	{
		OutKind = nullptr;
		const APawn* P = PC.IsValid() ? PC->GetPawn() : nullptr;
		if (!P) return nullptr;
		const FName Biome = WRBiome::At(P, P->GetActorLocation());
		const FWRWeatherNow* W = UWRWeatherSubsystem::For(P, Biome);
		const float Now = UWRWeatherSubsystem::Now(P);
		if (!W || W->Kind.IsNone() || Now >= W->EndAt || Now < W->WarnAt) return nullptr;
		OutKind = FWRWeatherDefs::Get().Kind(Biome, W->Kind);
		return W;
	}

	TSharedRef<SWidget> Conditions()
	{
		const FLinearColor RestTint(FColor(196, 176, 255)), DarkTint(FColor(150, 150, 170)), CorruptTint(FColor(150, 70, 190));
		return SNew(SVerticalBox).Visibility_Lambda([this]() { return Survivor() ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
			// Rested, or the comfort of the bed you're standing by (survival-crafting 2.2); it pulses in its last minute.
			+ SVerticalBox::Slot().AutoHeight()
			[
				Condition([this]() { const UWRSurvivalComponent* S = Surv(); return S && (S->IsRested() || S->GetComfortHere() >= 0); },
					[]() { return FName(TEXT("rested")); },
					[this, RestTint]()
					{
						const UWRSurvivalComponent* S = Surv();
						if (!S || !S->IsRested()) return WRUI::Alpha(RestTint, 0.45f);
						return S->RestedSecondsLeft() > 60.f ? RestTint : WRUI::Alpha(RestTint, 0.55f + 0.45f * float(FMath::Abs(FMath::Sin(FPlatformTime::Seconds() * 3.0))));
					},
					[this]()
					{
						const UWRSurvivalComponent* S = Surv();
						if (S && S->IsRested()) return FText::FromString(FString::Printf(TEXT("Rested · %s"), *Clock(S->RestedSecondsLeft())));
						return FText::FromString(FString::Printf(TEXT("Comfort %d"), S ? FMath::Max(0, S->GetComfortHere()) : 0));
					},
					[this]()
					{
						const UWRSurvivalComponent* S = Surv();
						const int32 C = S ? FMath::Max(0, S->GetComfortHere()) : 0;
						if (S && S->IsRested())
						{
							return FText::FromString(S->GetComfortHere() >= 0 ? FString::Printf(TEXT("Stamina +20%%, slower hunger and thirst, skills learn faster · comfort %d here"), C)
								: FString(TEXT("Stamina +20%, slower hunger and thirst, skills learn faster")));
						}
						return FText::FromString(FString::Printf(TEXT("Sleep in your bed (%s) for %d min of Rested"), *WRInput::ActionKey(TEXT("Interact")).ToString(),
							FMath::RoundToInt(WRTune(TEXT("rested.bedMinutes"), 20.f) + C * WRTune(TEXT("rested.perComfort"), 1.f))));
					})
			]
			// Sated after a good meal (cooks/build-farming.md 2.8): hunger holds still; it pulses in its last minute.
			+ SVerticalBox::Slot().AutoHeight()
			[
				Condition([this]() { const UWRFoodComponent* F = Food(); return F && F->SatedSecondsLeft() > 0.f; },
					[]() { return FName(TEXT("sated")); },
					[this]()
					{
						const FLinearColor SatedTint(FColor(236, 190, 110));
						const UWRFoodComponent* F = Food();
						return F && F->SatedSecondsLeft() > 60.f ? SatedTint : WRUI::Alpha(SatedTint, 0.55f + 0.45f * float(FMath::Abs(FMath::Sin(FPlatformTime::Seconds() * 3.0))));
					},
					[this]() { const UWRFoodComponent* F = Food(); return FText::FromString(FString::Printf(TEXT("Sated · %s"), *Clock(F ? F->SatedSecondsLeft() : 0.f))); },
					[]() { return FText::FromString(TEXT("A good meal: hunger holds still")); })
			]
			// The weather where you are: coming (with the count), or on, and whether you're beating it.
			+ SVerticalBox::Slot().AutoHeight()
			[
				Condition([this]() { const FWRWeatherKind* K = nullptr; return WeatherHere(K) && K; },
					[this]() { const FWRWeatherKind* K = nullptr; WeatherHere(K); return K && !K->Glyph.IsNone() ? K->Glyph : FName(TEXT("weather_storm")); },
					[this]()
					{
						const UWRSurvivalComponent* S = Surv();
						return S && (S->IsWeatherCountered() || S->IsSheltered()) ? WRUI::Good : WRUI::Warn;
					},
					[this]()
					{
						const FWRWeatherKind* K = nullptr;
						const FWRWeatherNow* W = WeatherHere(K);
						if (!W || !K) return FText::GetEmpty();
						const float Now = UWRWeatherSubsystem::Now(PC.Get());
						return FText::FromString(W->IsComing(Now) ? FString::Printf(TEXT("%s coming · %s"), *K->Name, *Clock(W->StartAt - Now))
							: FString::Printf(TEXT("%s · %s"), *K->Name, *Clock(W->EndAt - Now)));
					},
					[this]()
					{
						const FWRWeatherKind* K = nullptr;
						WeatherHere(K);
						const UWRSurvivalComponent* S = Surv();
						if (!K || !S) return FText::GetEmpty();
						if (S->IsSheltered()) return LOCTEXT("Sheltered", "Sheltered: it can't reach you here");
						if (S->IsWeatherCountered()) return LOCTEXT("Countered", "Your gear holds it off");
						return FText::FromString(K->Does);
					})
			]
			// Unlit: dark-hunters smell you from twice as far.
			+ SVerticalBox::Slot().AutoHeight()
			[
				Condition([this]() { const UWRSurvivalComponent* S = Surv(); return S && S->IsUnlit(); },
					[]() { return FName(TEXT("dark")); },
					[DarkTint]() { return DarkTint; },
					[]() { return LOCTEXT("Unlit", "Unlit"); },
					[]()
					{
						// A pad has no light button: the torch or lantern is lit from the belt.
						const FString Key = WRInput::ActionKey(TEXT("LightToggle")).ToString();
						return FText::FromString(Key.IsEmpty() ? FString(TEXT("Dark-hunters smell you from twice as far · light a torch or lantern from your belt"))
							: FString::Printf(TEXT("Dark-hunters smell you from twice as far · %s: torch or lantern"), *Key));
					})
			]
			// Corruption (never "Void": that's the Void path's own resource).
			+ SVerticalBox::Slot().AutoHeight()
			[
				Condition([this]() { const UWRSurvivalComponent* S = Surv(); return S && S->GetCorruption() >= 0.5f; },
					[]() { return FName(TEXT("corruption")); },
					[CorruptTint]() { return CorruptTint; },
					[this]() { const UWRSurvivalComponent* S = Surv(); return FText::FromString(FString::Printf(TEXT("Corruption %d"), S ? FMath::RoundToInt(S->GetCorruption()) : 0)); },
					[this]()
					{
						const UWRSurvivalComponent* S = Surv();
						if (!S) return FText::GetEmpty();
						if (S->MatchesCorruption()) return FText::FromString(FString::Printf(TEXT("Your path feeds on it: path damage +%.0f%%"), (S->PathDamageMult() - 1.f) * 100.f));
						static const TCHAR* Steps[] = { TEXT("A fire or the Hearth stops it; sleep halves it"), TEXT("Healing -10%"), TEXT("Healing -10%, stamina regen -15%, the dark finds you"),
							TEXT("Healing -10%, stamina regen -15%, max stamina -20"), TEXT("Marked: everything here hunts you, path power fills slower") };
						return FText::FromString(Steps[FMath::Clamp(S->CorruptionStep(), 0, 4)]);
					})
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(35.f * U, 3.f * U, 0.f, 0.f)
			[
				SNew(SBox).Visibility_Lambda([this]() { const UWRSurvivalComponent* S = Surv(); return S && S->GetCorruption() >= 0.5f ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				[
					SNew(SWRBar).Height(5.f * U).Color(CorruptTint).Fraction_Lambda([this]() { const UWRSurvivalComponent* S = Surv(); return S ? S->GetCorruption() / 100.f : 0.f; })
				]
			];
	}

	/** Asleep: the view dims to rest.dimTo over rest.dimSeconds; the bar fills; any move gets you up. */
	TSharedRef<SWidget> SleepCover()
	{
		return SNew(SOverlay).Visibility_Lambda([this]() { const UWRSurvivalComponent* S = Surv(); return S && S->IsSleeping() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			+ SOverlay::Slot()
			[
				SNew(SImage).Image(WRUI::White()).ColorAndOpacity_Lambda([this]()
				{
					const UWRSurvivalComponent* S = Surv();
					const float Since = S ? UWRWeatherSubsystem::Now(PC.Get()) - S->GetSleepStartedAt() : 0.f;
					const float Dim = FMath::Clamp(Since / FMath::Max(0.1f, WRTune(TEXT("rest.dimSeconds"), 2.f)), 0.f, 1.f) * (1.f - WRTune(TEXT("rest.dimTo"), 0.3f));
					return FSlateColor(FLinearColor(0.f, 0.f, 0.01f, Dim));
				})
			]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0.f, 120.f * U, 0.f, 0.f)
			[
				SNew(SBox).WidthOverride(320.f * U)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[WRUI::Text(LOCTEXT("Sleeping", "Sleeping"), WRUI::Title(20), WRUI::Ink)]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f * U)
					[
						SNew(SWRBar).Height(8.f * U).Color(FLinearColor(FColor(196, 176, 255))).Fraction_Lambda([this]()
						{
							const UWRSurvivalComponent* S = Surv();
							if (!S) return 0.f;
							const float Now = UWRWeatherSubsystem::Now(PC.Get());
							return FMath::Clamp((Now - S->GetSleepStartedAt()) / FMath::Max(0.1f, S->GetSleepEndsAt() - S->GetSleepStartedAt()), 0.f, 1.f);
						})
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[WRUI::Text(LOCTEXT("SleepMove", "Move to get up (no rest)"), WRUI::Body(12, 500), WRUI::Ink3)]
				]
			];
	}

	// ---------------------------------------------------------------- bottom centre: hotbar
	TSharedRef<SWidget> HotbarArea()
	{
		TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
		for (int32 i = 0; i < 8; ++i)
		{
			Row->AddSlot().AutoWidth().Padding(i == 0 ? 0.f : 6.f * U, 0.f, 0.f, 0.f)
			[
				SNew(SWRSlot).Size(64.f * U).Kind(EWRSlotKind::Hotbar).Index(i)
				.View_Lambda([this, i]() { const FWRPlayerUI* X = UI(); return X ? X->HotbarView(i) : FWRSlotView(); })
				.OnClicked_Lambda([this, i](const FPointerEvent&) { if (FWRPlayerUI* X = UI()) X->UseHotbar(i); })
			];
		}
		return SNew(SVerticalBox).Visibility_Lambda([this]() { return Survivor() && !Hidden() ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 8.f * U)
			[
				SNew(SBox).Visibility_Lambda([this]() { return SelectedItemName().IsEmpty() || WheelOpen() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
				[
					SNew(SWRPanel).Kind(EWRPanel::Glass).Padding(FMargin(14.f * U, 3.f * U))
					[
						WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(SelectedItemName()); }), WRUI::Body(15, 800), WRUI::Ink)
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SWRPanel).Kind(EWRPanel::Stone).Rivets(true).Padding(FMargin(14.f * U, 10.f * U))[Row]
			];
	}

	FString SelectedItemName() const
	{
		const FWRPlayerUI* X = UI();
		const AWRCharacter* S = Survivor();
		const AWRPlayerState* PS = PC.IsValid() ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
		const UWRInventoryComponent* Inv = PS ? PS->GetInventory() : nullptr;
		if (!X || !S || !Inv) return FString();
		const FWRItemStack& Held = Inv->GetEquipped(S->IsRangedOut() ? EWREquipSlot::Ranged : EWREquipSlot::Weapon);
		const FWRItemDef* Def = Held.IsEmpty() ? nullptr : FWRItemRegistry::Get().Find(Held.ItemId);
		if (!Def) return FString();
		FString Line = FString::Printf(TEXT("%s%s"), Held.Quality != EWRQuality::Common ? *(FString(FWRItemStack::QualityName(Held.Quality)) + TEXT(" ")) : TEXT(""), *Def->Name);
		if (Def->MaxDurability > 0.f) Line += FString::Printf(TEXT("   %.0f / %.0f"), Held.Durability, Def->MaxDurability);
		if (S->IsRangedOut())
		{
			const FWRItemDef* Ammo = FWRItemRegistry::Get().Find(S->GetSelectedAmmo());
			if (Ammo) Line += FString::Printf(TEXT("   ·   %s x%d"), *Ammo->Name, Inv->CountOf(Ammo->Id));
		}
		return Line;
	}

	// ---------------------------------------------------------------- bottom right: abilities, tame
	TSharedRef<SWidget> AbilityMedallion(int32 Wheel, float Size)
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(SBox).WidthOverride(Size).HeightOverride(Size)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()[SNew(SImage).Image_Lambda([this, Wheel, Size]() { return WRUI::Box(AbilityName(Wheel).IsEmpty() ? FLinearColor(0.1f, 0.1f, 0.11f) : PathColor() * 0.3f, Size * 0.5f, Wheel == 0 ? WRUI::Brass : WRUI::Brass2, 2.f); })]
					+ SOverlay::Slot()[SNew(SWRRing).Diameter(Size).Thickness(4.f * U).Color(FLinearColor(0.f, 0.f, 0.f, 0.75f)).Fraction_Lambda([this, Wheel]() { return CooldownFraction(Wheel); })]
					+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
					[
						WRUI::Text(TAttribute<FText>::CreateLambda([this, Wheel]() { const float C = Cooldown(Wheel); return C > 0.f ? FText::FromString(FString::Printf(TEXT("%.1f"), C)) : FText::FromString(AbilityName(Wheel).Left(1)); }), WRUI::Title(Wheel == 0 ? 20 : 16), TAttribute<FSlateColor>::CreateLambda([this, Wheel]() { return FSlateColor(Cooldown(Wheel) > 0.f ? WRUI::Ink : PathColor()); }), true)
					]
					+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(-6.f, 0.f, 0.f, -4.f)
					[
						SNew(SWRKeyCap).FontSize(11).Text_Lambda([Wheel]() { return WRInput::ActionKey(Wheel == 0 ? TEXT("Wheel1") : TEXT("Wheel2")); })
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 5.f * U, 0.f, 0.f)
			[
				WRUI::Text(TAttribute<FText>::CreateLambda([this, Wheel]() { const FString N = AbilityName(Wheel); return FText::FromString(N.IsEmpty() ? TEXT("Empty") : N); }), WRUI::Body(12, 700), TAttribute<FSlateColor>::CreateLambda([this, Wheel]() { return FSlateColor(AbilityName(Wheel).IsEmpty() ? WRUI::Ink3 : WRUI::Ink); }))
			];
	}

	FString AbilityName(int32 Wheel) const
	{
		const FWRPlayerUI* X = UI();
		if (!X) return FString();
		const TArray<FWRCastable> All = X->GetCastables();
		const FWRCastable* C = X->FindCastable(X->GetWheelAbility(Wheel, X->GetWheelPicked(Wheel)), All);
		return !C ? FString() : C->Blocked.IsEmpty() ? C->Name.ToString() : FString::Printf(TEXT("%s  (%s)"), *C->Name.ToString(), *C->Blocked);
	}
	float Cooldown(int32 Wheel) const
	{
		const FWRPlayerUI* X = UI();
		if (!X) return 0.f;
		const TArray<FWRCastable> All = X->GetCastables();
		const FWRCastable* C = X->FindCastable(X->GetWheelAbility(Wheel, X->GetWheelPicked(Wheel)), All);
		return C ? C->Cooldown : 0.f;
	}
	float CooldownFraction(int32 Wheel) const { const float C = Cooldown(Wheel); return C > 0.f ? FMath::Clamp(C / 10.f, 0.05f, 1.f) : 0.f; }

	TSharedRef<SWidget> AbilityPlate()
	{
		return SNew(SBox).Visibility_Lambda([this]() { return Survivor() && !Covered() && FWRKeyBindings::Get().GetCastMode() != EWRCastMode::Keys ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		[
			SNew(SWRPanel).Kind(EWRPanel::Stone).Rivets(true).Padding(FMargin(16.f * U, 12.f * U))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 16.f * U, 0.f)[AbilityMedallion(0, 62.f * U)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(0.f, 0.f, 16.f * U, 0.f)[AbilityMedallion(1, 52.f * U)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(104.f * U)
					[
						WRUI::Text(TAttribute<FText>::CreateLambda([]()
						{
							const FText K = WRInput::ActionKey(TEXT("Wheel1"));
							return FText::Format(LOCTEXT("WheelHint", "Hold {0} for the wheel\nTap {0} to cast"), K);
						}), WRUI::Body(12, 500), WRUI::Ink3, false, true)
					]
				]
			]
		];
	}

	const AWRCreature* NearTame() const
	{
		const APawn* Me = PC.IsValid() ? PC->GetPawn() : nullptr;
		const AWRPlayerState* PS = PC.IsValid() ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
		if (!Me || !PS) return nullptr;
		const AWRCreature* Best = nullptr;
		float BestDist = 3000.f;
		for (TActorIterator<AWRCreature> It(Me->GetWorld()); It; ++It)
		{
			const float D = FVector::Dist(It->GetActorLocation(), Me->GetActorLocation());
			// The one you ride first, then a baby asking for care (cooks/build-taming.md 2.6), then the nearest following.
			const bool bRidden = *It == Me;
			const bool bAsks = It->TameRep.Care >= 1 && It->TameRep.Care <= 3;
			const float Score = bRidden ? -2.f : bAsks ? D - 3000.f : D;
			if (It->IsAlive() && It->IsTamed() && It->TamedBy == PS->GetPlayerName() && (It->TameOrder == EWRTameOrder::Follow || bAsks || bRidden) && D < 3000.f && Score < BestDist) { BestDist = Score; Best = *It; }
		}
		return Best;
	}

	/** The tame card's last line: what it needs or is doing (care, hunger, mating, young, its follow and stance). */
	FText TameStatus() const
	{
		const AWRCreature* T = NearTame();
		if (!T) return FText::GetEmpty();
		const FWRTameRep& R = T->TameRep;
		if (R.Care == 1) { const FWRItemDef* Food = FWRItemRegistry::Get().Find(R.CareFood); return FText::FromString(FString::Printf(TEXT("Hungry for %s: Tend"), Food ? *Food->Name.ToLower() : *R.CareFood.ToString())); }
		if (R.Care == 2) return FText::FromString(FString::Printf(TEXT("Wants a walk: %.0f / 50 m"), R.CareWalked));
		if (R.Care == 3) return FText::FromString(TEXT("Wants a cuddle: Tend"));
		if (R.Hunger <= 0.f) return FText::FromString(TEXT("Starving: feed it"));
		if (R.Hunger < 25.f) return FText::FromString(TEXT("Hungry"));
		if (R.MateBar > 0.f) return FText::FromString(FString::Printf(TEXT("Mating %.0f%%"), R.MateBar * 100.f));
		if (R.Pregnant > 0.f) return FText::FromString(TEXT("Carrying young"));
		if (T->IsBaby()) return FText::FromString(FString::Printf(TEXT("Growing %.0f%% · imprint %.0f%%"), R.Growth * 100.f, R.Imprint * 100.f));
		return T->bTameAggressive ? LOCTEXT("FollowAggro", "Following · aggressive") : LOCTEXT("Follow", "Following · passive");
	}

	FLinearColor TameStatusColour() const
	{
		const AWRCreature* T = NearTame();
		if (!T) return WRUI::Good;
		const FWRTameRep& R = T->TameRep;
		return (R.Care >= 1 && R.Care <= 3) || R.Hunger < 25.f ? WRUI::Warn : WRUI::Good;
	}

	/** Riding a flyer: its wings' stamina (cooks/build-taming.md 2.4). */
	const AWRCreature* RiddenFlyer() const
	{
		const AWRCreature* Mount = PC.IsValid() ? Cast<AWRCreature>(PC->GetPawn()) : nullptr;
		return Mount && Mount->GetMountKind() == TEXT("fly") ? Mount : nullptr;
	}

	TSharedRef<SWidget> TameCard()
	{
		return SNew(SBox).WidthOverride(240.f * U).Visibility_Lambda([this]() { return NearTame() && !Covered() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		[
			SNew(SWRPanel).Kind(EWRPanel::Glass).Padding(FMargin(12.f * U, 9.f * U))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f * U, 0.f)
				[
					SNew(SBox).WidthOverride(46.f * U).HeightOverride(46.f * U)
					[
						SNew(SOverlay)
						+ SOverlay::Slot()[SNew(SImage).Image(WRUI::Box(FLinearColor(0.1f, 0.1f, 0.11f), 23.f * U, WRUI::Brass2, 2.f))]
						+ SOverlay::Slot().Padding(3.f)[SNew(SImage).Image_Lambda([this]() { const AWRCreature* T = NearTame(); const FSlateBrush* B = T ? WRUI::Portrait(T->CreatureId) : nullptr; return B ? B : WRUI::Icon(TEXT("kibble")); })]
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { const AWRCreature* T = NearTame(); return T ? FText::FromString(T->GetDisplayLabel()) : FText::GetEmpty(); }), WRUI::Body(14, 800), WRUI::Ink)]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f * U)[SNew(SWRBar).Height(5.f * U).Color(WRUI::Hp).Fraction_Lambda([this]() { const AWRCreature* T = NearTame(); const UWRAttributeSet* A = T ? T->GetAttributes() : nullptr; return A ? A->GetHealth() / FMath::Max(1.f, A->GetMaxHealth()) : 0.f; })]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SBox).Visibility_Lambda([this]() { return RiddenFlyer() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
						[
							SNew(SWRBar).Height(5.f * U).Color(WRUI::Frost).Fraction_Lambda([this]() { const AWRCreature* F = RiddenFlyer(); return F ? FMath::Clamp(F->TameRep.FlyStamina / FMath::Max(1.f, WRTune(TEXT("tame.fly.stamina"), 300.f) * F->StatMult(WRTameStat::Stamina)), 0.f, 1.f) : 0.f; })
						]
					]
					+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return TameStatus(); }), WRUI::Body(12, 700), TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(TameStatusColour()); }))]
				]
			]
		];
	}
};

WR_REGISTER_SCREEN("hud", [](AWRPlayerController* PC) -> TSharedRef<SWidget> { return SNew(SWRHud).PC(PC); });

#undef LOCTEXT_NAMESPACE
