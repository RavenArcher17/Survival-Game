// The in-game HUD of the stone-and-leather UI (mockup "HUD: exploring"): compass, clock, level and statuses, vitals,
// the item hotbar, the ability keys, your tame, the taming panel, prompts, pickups, chat, boss bar and help (H).
// Sizes are in 1080p units (the mockups' 1600x900 px x 1.2); Slate scales them to the screen.

#include "WRPlayerUI.h"
#include "WRUIStyle.h"
#include "WRUIWidgets.h"
#include "WRAltar.h"
#include "WRAttributeSet.h"
#include "WRBoss.h"
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
#include "WRWorldDressing.h"
#include "WRTuning.h"
#include "AbilitySystemComponent.h"
#include "EngineUtils.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Widgets/Images/SImage.h"
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
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(24.f * U, 20.f * U)[LevelPlate()]
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
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[HelpOverlay()]
		];
	}

private:
	TWeakObjectPtr<AWRPlayerController> PC;

	AWRCharacter* Survivor() const { return PC.IsValid() ? Cast<AWRCharacter>(PC->GetPawn()) : nullptr; }
	AWRCharacterBase* Body() const { return PC.IsValid() ? Cast<AWRCharacterBase>(PC->GetPawn()) : nullptr; }
	const UWRAttributeSet* Attr() const { const AWRCharacterBase* B = Body(); return B ? B->GetAttributes() : nullptr; }
	FWRPlayerUI* UI() const { return PC.IsValid() ? PC->GetUI() : nullptr; }
	EVisibility IfSurvivor() const { return Survivor() ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; }
	bool Hidden() const { const FWRPlayerUI* X = UI(); return !X || X->IsScreenOpen(); }

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
		return SNew(SVerticalBox).Visibility_Lambda([this]() { return Survivor() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
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
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[SNew(SWRKeyCap).Text_Lambda([]() { return WRUI::KeyName(FWRKeyBindings::Get().Key(TEXT("Help"))); }).FontSize(11)]
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
		return SNew(SBox).WidthOverride(330.f * U).Visibility_Lambda([this]() { return !Hidden() && KnockedOut() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
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
		const FString Key = WRUI::KeyName(FWRKeyBindings::Get().Key(TEXT("Interact"))).ToString();
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
		return SNew(SBox).WidthOverride(18.f).HeightOverride(18.f).Visibility_Lambda([this]()
		{
			const AWRCharacter* S = Survivor();
			return S && !Hidden() && (S->IsAimingRanged() || S->IsFirstPerson()) ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
		})
		[
			SNew(SOverlay)
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[SNew(SBox).WidthOverride(16.f).HeightOverride(2.f)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity_Lambda([this]() { const AWRCharacter* S = Survivor(); return S && S->IsAimingRanged() ? FLinearColor(1, 1, 1, 0.9f) : FLinearColor::Transparent; })]]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[SNew(SBox).WidthOverride(2.f).HeightOverride(16.f)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity_Lambda([this]() { const AWRCharacter* S = Survivor(); return S && S->IsAimingRanged() ? FLinearColor(1, 1, 1, 0.9f) : FLinearColor::Transparent; })]]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[SNew(SBox).WidthOverride(4.f).HeightOverride(4.f)[SNew(SImage).Image(WRUI::Box(FLinearColor::White, 2.f, FLinearColor::Black, 1.f))]]
		];
	}

	TSharedRef<SWidget> CentreNotice()
	{
		return SNew(SBox).Visibility_Lambda([this]()
		{
			const AWRPlayerController* P = PC.Get();
			return P && !P->GetNotice().IsEmpty() && !P->GetNotice().StartsWith(TEXT("+")) && P->GetWorld()->GetTimeSeconds() - P->GetNoticeTime() < 4.0 ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
		})
		[
			SNew(SWRPanel).Kind(EWRPanel::Glass).Padding(FMargin(18.f * U, 7.f * U))
			[
				WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return PC.IsValid() ? FText::FromString(PC->GetNotice()) : FText::GetEmpty(); }), WRUI::Body(16, 700), WRUI::Ink)
			]
		];
	}

	FString PromptText(FString& OutTarget) const
	{
		AWRPlayerController* P = PC.Get();
		const AWRCharacter* S = Survivor();
		if (!P || !S || P->GetMenu() != EWRMenu::None || P->IsPlacing()) return FString();
		if (const AWRBossAltar* Altar = P->FindNearbyAltar()) { OutTarget = TEXT("Boss altar"); return Altar->GetPrompt(P).Replace(TEXT("E: "), TEXT("")); }
		if (const AWRCreature* Tame = P->FindNearbyCreature(true)) { OutTarget = Tame->GetDisplayLabel(); return TEXT("Hold: orders  ·  Tap: interact"); }
		if (const AWRCreature* Wild = P->FindNearbyCreature(false, 400.f))
		{
			if (Wild->IsUnconscious()) { OutTarget = Wild->GetDisplayLabel(); return TEXT("Tame"); }
		}
		if (const AActor* Use = P->FindUseTarget())
		{
			if (const AWRStation* St = Cast<AWRStation>(Use)) { OutTarget = St->GetDisplayName(); return TEXT("Use"); }
			if (const AWRBuildPiece* Piece = Cast<AWRBuildPiece>(Use))
			{
				const FWRPieceDef* Def = Piece->GetDef();
				if (!Def) return FString();
				OutTarget = Def->Name;
				if (Def->Kind == TEXT("door")) return Piece->bOpen ? TEXT("Close") : Piece->bLocked ? TEXT("Locked") : TEXT("Open");
				if (Def->Kind == TEXT("chest")) return TEXT("Open");
				if (Def->Kind == TEXT("totem")) return TEXT("Land claim");
				if (Def->Kind == TEXT("crop")) return Piece->bPlanted ? TEXT("Growing") : TEXT("Plant");
				if (Def->Kind == TEXT("bed")) return TEXT("Set respawn");
				return TEXT("Use");
			}
		}
		if (const AWRHearth* Hearth = P->FindNearbyHearth()) { OutTarget = FString::Printf(TEXT("Hearth · tech level %d"), Hearth->GetTechLevel()); return TEXT("Hearth"); }
		return FString();
	}

	TSharedRef<SWidget> Prompt()
	{
		return SNew(SBox).Visibility_Lambda([this]() { FString T; return !Hidden() && !PromptText(T).IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		[
			SNew(SWRPanel).Kind(EWRPanel::Glass).Padding(FMargin(12.f * U, 8.f * U))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f * U, 0.f)[SNew(SWRKeyCap).FontSize(14).Text_Lambda([]() { return WRUI::KeyName(FWRKeyBindings::Get().Key(TEXT("Interact"))); })]
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
		TSharedRef<SVerticalBox> Box = SNew(SVerticalBox).Visibility_Lambda([this]() { return Hidden() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; });
		for (int32 i = 0; i < 5; ++i)
		{
			Box->AddSlot().AutoHeight().HAlign(HAlign_Center)
			[
				WRUI::Text(TAttribute<FText>::CreateLambda([this, i]()
				{
					const FWRPlayerUI* X = UI();
					if (!X) return FText::GetEmpty();
					const TArray<FWRPlayerUI::FPickup>& P = X->GetPickups();
					const int32 Idx = P.Num() - 5 + i;
					return P.IsValidIndex(Idx) ? FText::FromString(P[Idx].Text) : FText::GetEmpty();
				}), WRUI::Body(15, 800), TAttribute<FSlateColor>::CreateLambda([this, i]()
				{
					const FWRPlayerUI* X = UI();
					if (!X) return FSlateColor(WRUI::Ink);
					const TArray<FWRPlayerUI::FPickup>& P = X->GetPickups();
					const int32 Idx = P.Num() - 5 + i;
					const float Age = P.IsValidIndex(Idx) ? float(FPlatformTime::Seconds() - P[Idx].Time) : 99.f;
					return FSlateColor(WRUI::Alpha(WRUI::Ink, FMath::Clamp(1.f - (Age - 2.f) / 1.5f, 0.f, 1.f)));
				}), true)
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
	TSharedRef<SWidget> VitalRow(const FText& Label, const FLinearColor& Color, float Height, TFunction<float()> Value, TFunction<float()> Max)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(62.f * U)[WRUI::Text(Label, WRUI::Caps(12), WRUI::Ink2)]]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[SNew(SWRBar).Height(Height).Color(Color).Fraction_Lambda([Value, Max]() { return Value() / FMath::Max(1.f, Max()); })]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f * U, 0.f, 0.f, 0.f)
			[
				SNew(SBox).WidthOverride(64.f * U).HAlign(HAlign_Right)
				[
					WRUI::Text(TAttribute<FText>::CreateLambda([Value, Max]() { return FText::FromString(FString::Printf(TEXT("%.0f/%.0f"), Value(), Max())); }), WRUI::Body(13, 700), WRUI::Ink)
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
		return SNew(SBox).WidthOverride(360.f * U).Visibility_Lambda([this]() { return Body() && !Hidden() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		[
			SNew(SWRPanel).Kind(EWRPanel::Leather).Rivets(true).Padding(FMargin(18.f * U, 14.f * U))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f * U)
				[
					WRUI::Text(TAttribute<FText>::CreateLambda([this]() { const AWRCreature* M = PC.IsValid() ? Cast<AWRCreature>(PC->GetPawn()) : nullptr; return M ? FText::Format(LOCTEXT("Riding", "Riding {0}  ·  {1} to dismount"), FText::FromString(M->GetDisplayLabel()), WRUI::KeyName(FWRKeyBindings::Get().Key(TEXT("Interact")))) : FText::GetEmpty(); }), WRUI::Body(13, 700), WRUI::Brass)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f * U)[VitalRow(LOCTEXT("Health", "Health"), WRUI::Hp, 16.f * U, [A]() { const UWRAttributeSet* X = A(); return X ? X->GetHealth() : 0.f; }, [A]() { const UWRAttributeSet* X = A(); return X ? X->GetMaxHealth() : 1.f; })]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f * U)[VitalRow(LOCTEXT("Stamina", "Stamina"), WRUI::Stamina, 14.f * U, [A]() { const UWRAttributeSet* X = A(); return X ? X->GetStamina() : 0.f; }, [A]() { const UWRAttributeSet* X = A(); return X ? X->GetMaxStamina() : 1.f; })]
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
				SNew(SBox).Visibility_Lambda([this]() { return SelectedItemName().IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
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
						SNew(SWRKeyCap).FontSize(11).Text_Lambda([Wheel]() { return WRUI::KeyName(FWRKeyBindings::Get().Key(Wheel == 0 ? TEXT("Wheel1") : TEXT("Wheel2"))); })
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
		return C ? C->Name.ToString() : FString();
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
		return SNew(SBox).Visibility_Lambda([this]() { return Survivor() && !Hidden() && FWRKeyBindings::Get().GetCastMode() != EWRCastMode::Keys ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
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
							const FText K = WRUI::KeyName(FWRKeyBindings::Get().Key(TEXT("Wheel1")));
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
			if (It->IsAlive() && It->IsTamed() && It->TamedBy == PS->GetPlayerName() && It->TameOrder == EWRTameOrder::Follow && D < BestDist) { BestDist = D; Best = *It; }
		}
		return Best;
	}

	TSharedRef<SWidget> TameCard()
	{
		return SNew(SBox).WidthOverride(240.f * U).Visibility_Lambda([this]() { return NearTame() && !Hidden() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
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
					+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { const AWRCreature* T = NearTame(); return T && T->bTameAggressive ? LOCTEXT("FollowAggro", "Following · aggressive") : LOCTEXT("Follow", "Following · passive"); }), WRUI::Body(12, 700), WRUI::Good)]
				]
			]
		];
	}

	// ---------------------------------------------------------------- help (H)
	TSharedRef<SWidget> HelpOverlay()
	{
		TSharedRef<SVerticalBox> Col1 = SNew(SVerticalBox), Col2 = SNew(SVerticalBox);
		int32 n = 0;
		for (const FWRActionKey& A : FWRKeyBindings::Get().Actions())
		{
			if (A.Group == TEXT("Abilities") || (A.Id.ToString().StartsWith(TEXT("Hotbar")) && A.Id != TEXT("Hotbar1"))) continue;
			const FName Id = A.Id;
			TSharedRef<SVerticalBox> Col = (n++ % 2 == 0) ? Col1 : Col2;
			Col->AddSlot().AutoHeight().Padding(0.f, 3.f * U)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(96.f * U)[SNew(SWRKeyCap).FontSize(11).Text_Lambda([Id]() { return Id == TEXT("Hotbar1") ? FText::FromString(TEXT("1 - 8")) : WRUI::KeyName(FWRKeyBindings::Get().Key(Id)); })]]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[WRUI::Text(Id == TEXT("Hotbar1") ? LOCTEXT("HotbarAll", "Hotbar slots") : A.Label, WRUI::Body(13, 500), WRUI::Ink)]
			];
		}
		return SNew(SBox).WidthOverride(760.f * U).Visibility_Lambda([this]() { const FWRPlayerUI* X = UI(); return X && X->IsHelpShown() && !X->IsScreenOpen() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		[
			SNew(SWRPanel).Kind(EWRPanel::Stone).Rivets(true).BrassTrim(true).Padding(FMargin(26.f * U, 20.f * U))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 12.f * U)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f)[WRUI::Text(LOCTEXT("ControlsTitle", "Controls"), WRUI::Title(20), WRUI::Ink)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(LOCTEXT("ControlsHint", "Change any key in Settings > Controls"), WRUI::Body(12, 500), WRUI::Ink3)]
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 20.f * U, 0.f)[Col1]
					+ SHorizontalBox::Slot().FillWidth(1.f)[Col2]
				]
			]
		];
	}
};

WR_REGISTER_SCREEN("hud", [](AWRPlayerController* PC) -> TSharedRef<SWidget> { return SNew(SWRHud).PC(PC); });

#undef LOCTEXT_NAMESPACE
