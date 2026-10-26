// The Taming panel (mockup "Taming"): E on a knocked-out wild creature. A stone side panel over the game (the HUD's
// vitals and hotbar stay up beside it): the creature's portrait, torpor and when it wakes, taming progress and
// effectiveness, what's in its food stash and what it eats. The food in your bag is the feeding buttons
// ("tamegive <item>" gives it all of one food, as the server does); narcotics go in with "tamenarcotic".
// The HUD's small taming card (WRUIHud.cpp) shows the same numbers and the same advice while this is closed.

#include "WRPlayerUI.h"
#include "WRUIStyle.h"
#include "WRUIWidgets.h"
#include "WRCreature.h"
#include "WRInventory.h"
#include "WRKeyBindings.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRTuning.h"
#include "Rendering/DrawElements.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"

#define LOCTEXT_NAMESPACE "WRUI"

// Everything in a named namespace: the screens' helpers mustn't collide if files are ever merged into one unit.
namespace WRUITamingScreen
{
	using WRUI::U;   // mockup px -> 1080p units
	constexpr float PanelW = 420.f * U;
	constexpr float PortraitH = 150.f * U;

	FString Clock(float Seconds) { const int32 S = FMath::Max(0, FMath::FloorToInt(Seconds)); return FString::Printf(TEXT("%d:%02d"), S / 60, S % 60); }

	FString Capitalised(const FString& S) { return S.IsEmpty() ? S : S.Left(1).ToUpper() + S.Mid(1); }

	FString ItemName(FName Id) { const FWRItemDef* Def = FWRItemRegistry::Get().Find(Id); return Def ? Def->Name : Id.ToString(); }

	/** A small lit diamond in a row's colour (the mockup's row glyphs). */
	TSharedRef<SWidget> Diamond(const FLinearColor& Color, float Size = 10.f * U)
	{
		return SNew(SBox).WidthOverride(Size + 4.f).HeightOverride(Size + 4.f).HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(Size).HeightOverride(Size)
			[
				SNew(SImage).Image(WRUI::Box(Color, 1.5f, FLinearColor(0.f, 0.f, 0.f, 0.8f), 1.f))
				.RenderTransform(FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(45.f))))
				.RenderTransformPivot(FVector2D(0.5f, 0.5f))
			]
		];
	}

	/** The dark fade over the bottom of the portrait, so the name reads over any picture. */
	class SShade : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SShade) {}
		SLATE_END_ARGS()
		void Construct(const FArguments& InArgs) {}
		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(8.f, 8.f); }
		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override
		{
			const FVector2D S = Geo.GetLocalSize();
			TArray<FSlateGradientStop> Stops;
			Stops.Add(FSlateGradientStop(FVector2D(0.f, S.Y * 0.38f), FLinearColor(0.f, 0.f, 0.f, 0.f)));
			Stops.Add(FSlateGradientStop(FVector2D(0.f, S.Y), FLinearColor(0.004f, 0.002f, 0.001f, 0.93f)));
			FSlateDrawElement::MakeGradient(Out, Layer, Geo.ToPaintGeometry(), Stops, Orient_Horizontal);
			// A soft dark rim all round (the mockup's inset shadow).
			for (int32 i = 0; i < 6; ++i)
			{
				const float In = i * 2.f;
				WRUI::DrawBox(Out, Layer + 1, Geo.ToPaintGeometry(S - FVector2D(In * 2.f, In * 2.f), FSlateLayoutTransform(FVector2D(In, In))),
					WRUI::Box(FLinearColor::Transparent, 2.f, FLinearColor(0.f, 0.f, 0.f, 0.22f - i * 0.035f), 2.f));
			}
			return Layer + 1;
		}
	};

	class STamingScreen : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(STamingScreen) {}
			SLATE_ARGUMENT(TWeakObjectPtr<AWRPlayerController>, PC)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			PC = InArgs._PC;
			SetVisibility(EVisibility::SelfHitTestInvisible);
			ChildSlot
			[
				SNew(SOverlay)
				+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(0.f, 120.f * U, 24.f * U, 0.f)
				[
					SNew(SBox).WidthOverride(PanelW).HeightOverride(680.f * U)
					[
						SNew(SWRPanel).Kind(EWRPanel::Stone).Rivets(true).BrassTrim(true).Padding(FMargin(20.f * U, 18.f * U, 20.f * U, 20.f * U))
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()[Portrait()]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f * U, 0.f, 0.f)
							[
								SNew(SBox).Visibility_Lambda([this]() { return IsTaming() ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot().AutoHeight()[TorporBlock()]
									+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f * U)[WRUI::Divider()]
									+ SVerticalBox::Slot().AutoHeight()[TamingBlock()]
									+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f * U, 0.f, 0.f)[EffectivenessBlock()]
									+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f * U)[WRUI::Divider()]
									+ SVerticalBox::Slot().AutoHeight()[FoodBlock()]
								]
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 18.f * U, 0.f, 0.f)
							[
								// Tamed, woke up or gone: say so instead of the numbers.
								SNew(SBox).Visibility_Lambda([this]() { return IsTaming() ? EVisibility::Collapsed : EVisibility::SelfHitTestInvisible; })
								[
									WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return EndText(); }), WRUI::Body(16, 700),
										TAttribute<FSlateColor>::CreateLambda([this]() { const AWRCreature* C = Creature(); return FSlateColor(C && C->IsTamed() ? WRUI::Good : WRUI::Warn); }), false, true)
								]
							]
							+ SVerticalBox::Slot().FillHeight(1.f)[SNew(SSpacer)]
							+ SVerticalBox::Slot().AutoHeight()[Buttons()]
						]
					]
				]
			];
		}

	private:
		TWeakObjectPtr<AWRPlayerController> PC;
		FSlateBrush PortraitBrush;
		FName PortraitOf;
		bool bHasPortrait = false;

		AWRCreature* Creature() const { return PC.IsValid() ? PC->GetMenuCreature() : nullptr; }
		const FWRCreatureDef* Def() const { const AWRCreature* C = Creature(); return C ? C->GetDef() : nullptr; }
		bool IsTaming() const { const AWRCreature* C = Creature(); return C && C->IsAlive() && !C->IsTamed() && C->IsUnconscious(); }
		const UWRInventoryComponent* Bag() const { const AWRPlayerState* PS = PC.IsValid() ? PC->GetPlayerState<AWRPlayerState>() : nullptr; return PS ? PS->GetInventory() : nullptr; }
		int32 Have(FName Item) const { const UWRInventoryComponent* B = Bag(); return B ? B->CountOf(Item) : 0; }
		void Send(const FString& Command) const { if (PC.IsValid()) PC->SendCommand(Command); }

		/** What it eats, best first: kibble (twice the taming, keeps effectiveness), then its own food (the server's order). */
		TArray<FName> Foods() const
		{
			TArray<FName> Out = { FName(TEXT("kibble")) };
			if (const FWRCreatureDef* D = Def())
			{
				for (const FName& F : D->Food) Out.AddUnique(F);
			}
			return Out;
		}
		/** The food "Give food" hands over: the best one you carry. */
		FName BestFood() const
		{
			for (const FName& F : Foods())
			{
				if (Have(F) > 0) return F;
			}
			return NAME_None;
		}

		FText EndText() const
		{
			const AWRCreature* C = Creature();
			if (!C || !C->IsAlive()) return LOCTEXT("TameGone", "It's gone.");
			if (C->IsTamed()) return FText::Format(LOCTEXT("TameDone", "Tamed! {0} is yours: hold {1} on it for orders."), FText::FromString(Def() ? Def()->Name : C->GetDisplayLabel()), WRUI::KeyName(FWRKeyBindings::Get().Key(TEXT("Interact"))));
			return LOCTEXT("TameAwake", "It woke up. Knock it out again to go on taming: its progress is lost.");
		}

		// ---------------------------------------------------------------- portrait

		const FSlateBrush* PortraitImage()
		{
			const AWRCreature* C = Creature();
			const FName Id = C ? C->CreatureId : NAME_None;
			if (Id != PortraitOf)
			{
				PortraitOf = Id;
				const FSlateBrush* Src = Id.IsNone() ? nullptr : WRUI::Portrait(Id);
				bHasPortrait = Src != nullptr;
				if (Src)
				{
					// The square portrait cropped to the banner, a little above centre (the mockup's object-position 50% 24%).
					PortraitBrush = *Src;
					const float Aspect = (PortraitH - 4.f) / (PanelW - 40.f * U - 4.f);
					const float Top = 0.24f * (1.f - Aspect);
					PortraitBrush.SetUVRegion(FBox2f(FVector2f(0.f, Top), FVector2f(1.f, Top + Aspect)));
				}
			}
			return bHasPortrait ? &PortraitBrush : WRUI::White();
		}

		FText Subtitle() const
		{
			const FWRCreatureDef* D = Def();
			if (!D) return FText::GetEmpty();
			TArray<FString> Parts;
			if (!D->Tier.IsNone()) Parts.Add(Capitalised(D->Tier.ToString()));
			if (!D->Diet.IsNone()) Parts.Add(D->Diet.ToString());
			if (!D->Difficulty.IsNone()) Parts.Add(FString::Printf(TEXT("%s to tame"), *D->Difficulty.ToString()));
			return FText::FromString(FString::Join(Parts, TEXT(" · ")));
		}

		TSharedRef<SWidget> Portrait()
		{
			return SNew(SBox).HeightOverride(PortraitH)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[SNew(SImage).Image(WRUI::Box(FLinearColor(0.05f, 0.05f, 0.055f), 4.f, FLinearColor(0.f, 0.f, 0.f), 1.f))]
				+ SOverlay::Slot().Padding(2.f)
				[
					SNew(SImage).Image_Lambda([this]() { return PortraitImage(); })
					.ColorAndOpacity_Lambda([this]()
					{
						// No picture: the creature's own colour, darkened, under its name.
						const FWRCreatureDef* D = Def();
						return bHasPortrait ? FLinearColor(0.92f, 0.92f, 0.92f) : (D ? D->Color * 0.35f : FLinearColor(0.1f, 0.1f, 0.1f));
					})
				]
				+ SOverlay::Slot().Padding(2.f)[SNew(SShade)]
				+ SOverlay::Slot()[SNew(SImage).Image(WRUI::Box(FLinearColor::Transparent, 4.f, WRUI::Brass2, 2.f))]
				+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(10.f * U)
				[
					SNew(SWRChip).FontSize(11)
					.Text_Lambda([this]() { const FWRCreatureDef* D = Def(); return !D ? FText::GetEmpty() : D->Element == TEXT("frost") ? LOCTEXT("ElFrost", "Tundra") : D->Element == TEXT("fire") ? LOCTEXT("ElFire", "Volcanic") : FText::FromString(Capitalised(D->Element.ToString())); })
					.Color_Lambda([this]() { const FWRCreatureDef* D = Def(); return D && D->Element == TEXT("frost") ? WRUI::Frost : WRUI::Ember; })
					.Visibility_Lambda([this]() { const FWRCreatureDef* D = Def(); return D && !D->Element.IsNone() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				]
				+ SOverlay::Slot().VAlign(VAlign_Bottom).Padding(14.f * U, 0.f, 14.f * U, 9.f * U)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
						[
							WRUI::Text(TAttribute<FText>::CreateLambda([this]() { const FWRCreatureDef* D = Def(); return D ? FText::FromString(D->Name) : FText::GetEmpty(); }), WRUI::Title(22), WRUI::Ink, true)
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(8.f * U, 0.f, 0.f, 3.f * U)
						[
							WRUI::Text(TAttribute<FText>::CreateLambda([this]()
							{
								const AWRCreature* C = Creature();
								return !C ? FText::GetEmpty() : C->IsTamed() ? LOCTEXT("PortTamed", "· tamed") : C->IsUnconscious() ? LOCTEXT("PortKO", "· knocked out") : LOCTEXT("PortAwake", "· awake");
							}), WRUI::Body(15, 700), TAttribute<FSlateColor>::CreateLambda([this]() { const AWRCreature* C = Creature(); return FSlateColor(C && C->IsTamed() ? WRUI::Good : FLinearColor(FColor::FromHex(TEXT("C4ABF0")))); }), true)
						]
					]
					+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return Subtitle(); }), WRUI::Caps(12), WRUI::Ink2, true)]
				]
			];
		}

		// ---------------------------------------------------------------- torpor, taming, effectiveness

		/** A row heading: glyph, small-caps label, and a figure on the right. */
		TSharedRef<SWidget> Heading(const FLinearColor& Glyph, const FText& Label, TAttribute<FText> Value, TAttribute<FText> Of, TAttribute<FSlateColor> ValueColor)
		{
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f * U, 0.f)[Diamond(Glyph)]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[WRUI::Text(Label, WRUI::Caps(13), WRUI::Ink)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)[WRUI::Text(Value, WRUI::Body(14, 800), ValueColor)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)[WRUI::Text(Of, WRUI::Body(14, 500), WRUI::Ink3)];
		}

		TSharedRef<SWidget> TorporBlock()
		{
			return SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					Heading(WRUI::Torpor, LOCTEXT("Torpor", "Torpor"),
						TAttribute<FText>::CreateLambda([this]() { const AWRCreature* C = Creature(); return C ? FText::AsNumber(FMath::RoundToInt(C->Torpor)) : FText::GetEmpty(); }),
						TAttribute<FText>::CreateLambda([this]() { const AWRCreature* C = Creature(); return C ? FText::FromString(FString::Printf(TEXT(" / %d"), FMath::RoundToInt(C->MaxTorpor))) : FText::GetEmpty(); }),
						WRUI::Ink)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 0.f)
				[
					SNew(SWRBar).Height(16.f * U).Color(WRUI::Torpor).Fraction_Lambda([this]() { const AWRCreature* C = Creature(); return C && C->MaxTorpor > 0.f ? C->Torpor / C->MaxTorpor : 0.f; })
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f * U, 0.f, 0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f * U, 0.f)[Diamond(WRUI::Warn, 8.f * U)]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[
						WRUI::Text(TAttribute<FText>::CreateLambda([this]() { const AWRCreature* C = Creature(); return C ? FText::Format(LOCTEXT("WakesIn", "Wakes in {0}"), FText::FromString(Clock(C->WakeIn))) : FText::GetEmpty(); }),
							WRUI::Body(14, 700), TAttribute<FSlateColor>::CreateLambda([this]() { const AWRCreature* C = Creature(); return FSlateColor(C && C->WakeIn < 60.f ? WRUI::Bad : WRUI::Warn); }))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[NarcoticButton()]
				];
		}

		TSharedRef<SWidget> NarcoticButton()
		{
			const int32 Pct = FMath::RoundToInt(WRTune(TEXT("taming.narcoticTorpor"), 0.3f) * 100.f);
			return SNew(SButton).ButtonStyle(FCoreStyle::Get(), "NoBorder").ContentPadding(0.f).Cursor(EMouseCursor::Hand)
				.IsEnabled_Lambda([this]() { return Have(TEXT("narcotic")) > 0; })
				.ToolTipText(LOCTEXT("NarcoticTip", "Keeps it under: torpor goes up by a share of its most. Made at the alchemy table."))
				.OnClicked_Lambda([this]() { Send(TEXT("tamenarcotic")); return FReply::Handled(); })
				[
					SNew(SBox).MinDesiredHeight(36.f * U)
					[
						SNew(SOverlay)
						+ SOverlay::Slot()[SNew(SImage).Image(WRUI::Image(TEXT("leather"))).DesiredSizeOverride(FVector2D(1.f, 1.f)).ColorAndOpacity_Lambda([this]() { return Have(TEXT("narcotic")) > 0 ? FLinearColor::White : FLinearColor(0.5f, 0.5f, 0.5f); })]
						+ SOverlay::Slot()[SNew(SImage).Image(WRUI::Box(FLinearColor::Transparent, 4.f, WRUI::Brass2, 1.f))]
						+ SOverlay::Slot().VAlign(VAlign_Center).Padding(FMargin(10.f * U, 4.f * U, 12.f * U, 4.f * U))
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f * U, 0.f)
							[
								SNew(SBox).WidthOverride(20.f * U).HeightOverride(20.f * U)[SNew(SImage).Image(WRUI::Icon(TEXT("narcotic")))]
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								WRUI::Text(FText::Format(LOCTEXT("GiveNarcotic", "Give narcotic (+{0}%)"), Pct), WRUI::Body(14, 700),
									TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(Have(TEXT("narcotic")) > 0 ? WRUI::Ink : WRUI::Ink3); }), true)
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(5.f * U, 0.f, 0.f, 0.f)
							[
								WRUI::Text(TAttribute<FText>::CreateLambda([this]() { const int32 N = Have(TEXT("narcotic")); return N > 0 ? FText::Format(LOCTEXT("NarcLeft", "· {0} left"), N) : LOCTEXT("NarcNone", "· none"); }), WRUI::Body(14, 500), WRUI::Ink2, true)
							]
						]
					]
				];
		}

		/** The same advice as the HUD's taming card. */
		FString Advice() const
		{
			const AWRCreature* C = Creature();
			if (!C) return FString();
			const FString FoodName = ItemName(C->TameFoodItem);
			if (C->TameFoodCount == 0 && C->MealLeft <= 0.f) return FString::Printf(TEXT("Hungry: give it %s or kibble from your bag"), *FoodName);
			if (C->TameEta < 0.f) return FString::Printf(TEXT("Needs %d more %s to finish"), C->TameFoodNeeded, *FoodName);
			if (C->TameEta > C->WakeIn) return FString::Printf(TEXT("About %s left, but it wakes first: give it a narcotic"), *Clock(C->TameEta));
			return FString::Printf(TEXT("About %s left at this rate"), *Clock(C->TameEta));
		}
		FLinearColor AdviceColor() const
		{
			const AWRCreature* C = Creature();
			return !C ? WRUI::Ink3 : (C->TameFoodCount == 0 && C->MealLeft <= 0.f) || C->TameEta > C->WakeIn ? WRUI::Ember : C->TameEta < 0.f ? WRUI::Warn : WRUI::Ink3;
		}

		TSharedRef<SWidget> TamingBlock()
		{
			return SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					Heading(WRUI::Brass, LOCTEXT("Taming", "Taming"),
						TAttribute<FText>::CreateLambda([this]() { const AWRCreature* C = Creature(); return C ? FText::FromString(FString::Printf(TEXT("%.0f%%"), C->TameProgress * 100.f)) : FText::GetEmpty(); }),
						FText::GetEmpty(), WRUI::Ink)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 0.f)
				[
					SNew(SWRBar).Height(16.f * U).Color(WRUI::Brass).Fraction_Lambda([this]() { const AWRCreature* C = Creature(); return C ? C->TameProgress : 0.f; })
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 0.f)
				[
					WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(Advice()); }), WRUI::Body(13, 500), TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(AdviceColor()); }), false, true)
				];
		}

		TSharedRef<SWidget> EffectivenessBlock()
		{
			const int32 PerHit = FMath::RoundToInt(WRTune(TEXT("taming.effectivenessPerHit"), 0.06f) * 100.f);
			const int32 PerMeal = FMath::RoundToInt(WRTune(TEXT("taming.effectivenessPerPlainMeal"), 0.01f) * 100.f);
			const float AtFull = WRTune(TEXT("taming.bonusAtFull"), 0.5f);
			return SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					Heading(WRUI::Good, LOCTEXT("Effectiveness", "Effectiveness"),
						TAttribute<FText>::CreateLambda([this]() { const AWRCreature* C = Creature(); return C ? FText::FromString(FString::Printf(TEXT("%.0f%%"), C->TameEffectiveness * 100.f)) : FText::GetEmpty(); }),
						FText::GetEmpty(), WRUI::Good)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 0.f)
				[
					SNew(SWRBar).Height(8.f * U).Color(WRUI::Good).Fraction_Lambda([this]() { const AWRCreature* C = Creature(); return C ? C->TameEffectiveness : 0.f; })
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 0.f)
				[
					// What costs it (the server's rules): red once it has lost some.
					WRUI::Text(FText::Format(LOCTEXT("EffRule", "-{0}% for every hit while it's out · -{1}% a meal of plain food"), PerHit, PerMeal), WRUI::Body(13, 700),
						TAttribute<FSlateColor>::CreateLambda([this]() { const AWRCreature* C = Creature(); return FSlateColor(C && C->TameEffectiveness < 0.995f ? WRUI::Bad : WRUI::Ink3); }), false, true)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f * U, 0.f, 0.f)
				[
					SNew(SWRPanel).Kind(EWRPanel::Inset).Padding(FMargin(12.f * U, 7.f * U))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(LOCTEXT("TameBonus", "Tame bonus: "), WRUI::Body(15, 700), WRUI::Ink)]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							WRUI::Text(TAttribute<FText>::CreateLambda([this, AtFull]() { const AWRCreature* C = Creature(); return FText::FromString(FString::Printf(TEXT("+%d%%"), FMath::FloorToInt((C ? C->TameEffectiveness : 0.f) * AtFull * 100.f))); }), WRUI::Body(17, 800), WRUI::Good)
						]
						+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Right).VAlign(VAlign_Center)
						[
							WRUI::Text(FText::Format(LOCTEXT("BonusFull", "health and bite, up to +{0}%"), FMath::RoundToInt(AtFull * 100.f)), WRUI::Body(13, 500), WRUI::Ink3)
						]
					]
				];
		}

		// ---------------------------------------------------------------- food

		TSharedRef<SWidget> FoodBlock()
		{
			const int32 StashMax = FMath::RoundToInt(WRTune(TEXT("taming.stashMax"), 50.f));
			TSharedRef<SHorizontalBox> Slots = SNew(SHorizontalBox);
			TSharedRef<SVerticalBox> Names = SNew(SVerticalBox);
			for (const FName& Food : Foods())
			{
				Slots->AddSlot().AutoWidth().Padding(0.f, 0.f, 8.f * U, 0.f)
				[
					SNew(SBox).ToolTipText(FText::Format(LOCTEXT("GiveAllTip", "Give it all your {0}"), FText::FromString(ItemName(Food))))
					[
						SNew(SWRSlot).Size(64.f * U)
						.View_Lambda([this, Food]()
						{
							FWRSlotView V;
							V.ItemId = Food;
							V.Count = Have(Food);
							V.bShowCount = true;
							V.bDimmed = V.Count == 0;
							V.bSelected = Food == BestFood();
							return V;
						})
						.OnClicked_Lambda([this, Food](const FPointerEvent&) { if (Have(Food) > 0) Send(FString::Printf(TEXT("tamegive %s"), *Food.ToString())); })
					]
				];
				Names->AddSlot().AutoHeight().Padding(0.f, 1.f * U)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()[WRUI::Text(FText::FromString(ItemName(Food)), WRUI::Body(13, 800), WRUI::Ink)]
					+ SHorizontalBox::Slot().AutoWidth().Padding(4.f * U, 0.f, 0.f, 0.f)
					[
						WRUI::Text(TAttribute<FText>::CreateLambda([this, Food]() { return FText::FromString(FString::Printf(TEXT("x%d"), Have(Food))); }), WRUI::Body(13, 500), WRUI::Ink3)
					]
				];
			}
			Names->AddSlot().AutoHeight().Padding(0.f, 3.f * U, 0.f, 0.f)[WRUI::Text(LOCTEXT("ClickFood", "in your bag: click to give it all"), WRUI::Body(12, 500), WRUI::Ink3)];

			return SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f * U, 0.f)[Diamond(WRUI::Hunger)]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[WRUI::Text(LOCTEXT("FoodStash", "Food stash"), WRUI::Caps(13), WRUI::Ink)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						WRUI::Text(TAttribute<FText>::CreateLambda([this, StashMax]()
						{
							const AWRCreature* C = Creature();
							if (!C) return FText::GetEmpty();
							return C->TameFoodCount > 0 ? FText::Format(LOCTEXT("StashCount", "{0} of {1} · {2} next"), C->TameFoodCount, StashMax, FText::FromString(ItemName(C->TameFoodItem)))
								: FText::Format(LOCTEXT("StashEmpty", "empty · holds {0}"), StashMax);
						}), WRUI::Body(13, 500), WRUI::Ink3)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f * U, 0.f, 0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()[Slots]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(6.f * U, 0.f, 0.f, 0.f)[Names]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f * U, 0.f, 0.f)
				[
					WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return EatsLine(); }), WRUI::Body(13, 500), WRUI::Ink2, false, true)
				];
		}

		FText EatsLine() const
		{
			const FWRCreatureDef* D = Def();
			if (!D) return FText::GetEmpty();
			const int32 Kibble = FMath::RoundToInt(WRTune(TEXT("taming.kibbleValue"), 2.f));
			FString Line = FString::Printf(TEXT("Eats: Kibble (%dx faster, keeps effectiveness)"), Kibble);
			for (const FName& F : D->Food) Line += FString::Printf(TEXT(" › %s"), *ItemName(F));
			return FText::FromString(Line);
		}

		// ---------------------------------------------------------------- buttons

		TSharedRef<SWidget> Buttons()
		{
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.25f).Padding(0.f, 0.f, 10.f * U, 0.f)
				[
					SNew(SWRButton).Primary(true).FontSize(15).MinHeight(52.f * U)
					.Text_Lambda([this]() { const FName F = BestFood(); return F.IsNone() ? LOCTEXT("NoFood", "No food in your bag") : FText::Format(LOCTEXT("GiveFood", "Give {0}"), FText::FromString(ItemName(F).ToLower())); })
					.IsEnabled_Lambda([this]() { return IsTaming() && !BestFood().IsNone(); })
					.OnClicked_Lambda([this]() { const FName F = BestFood(); if (!F.IsNone()) Send(FString::Printf(TEXT("tamegive %s"), *F.ToString())); return FReply::Handled(); })
				]
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(SButton).ButtonStyle(FCoreStyle::Get(), "NoBorder").ContentPadding(0.f).Cursor(EMouseCursor::Hand)
					.OnClicked_Lambda([this]() { if (PC.IsValid()) PC->SetMenu(EWRMenu::None); return FReply::Handled(); })
					[
						SNew(SBox).MinDesiredHeight(52.f * U)
						[
							SNew(SOverlay)
							+ SOverlay::Slot()[SNew(SImage).Image(WRUI::Image(TEXT("leather"))).DesiredSizeOverride(FVector2D(1.f, 1.f))]
							+ SOverlay::Slot()[SNew(SImage).Image(WRUI::Box(FLinearColor::Transparent, 4.f, WRUI::Brass2, 1.f))]
							+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
							[
								// E closes this (as it does every menu), so the key goes on Close.
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f * U, 0.f)
								[
									SNew(SWRKeyCap).FontSize(12).Text_Lambda([]() { return WRUI::KeyName(FWRKeyBindings::Get().Key(TEXT("Interact"))); })
								]
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(LOCTEXT("TameClose", "Close"), WRUI::Title(15), WRUI::Ink, true)]
							]
						]
					]
				];
		}
	};
}

WR_REGISTER_SCREEN("taming", [](AWRPlayerController* PC) -> TSharedRef<SWidget> { return SNew(WRUITamingScreen::STamingScreen).PC(PC); });

#undef LOCTEXT_NAMESPACE
