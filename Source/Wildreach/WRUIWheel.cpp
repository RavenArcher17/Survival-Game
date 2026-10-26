// The radial wheels (mockups "Ability wheel (hold Q)" and "Tame orders (hold E)"): eight wedges of carved stone, the
// one under the mouse lit; the centre disc tells you what it does. FWRPlayerUI decides when a wheel is open and what
// a release or click picks; this widget only draws it (and turns a click into a pick for the tame wheel).

#include "WRPlayerUI.h"
#include "WRUIStyle.h"
#include "WRUIWidgets.h"
#include "WRAttributeSet.h"
#include "WRCreature.h"
#include "WRKeyBindings.h"
#include "WRPathTypes.h"
#include "WRPlayerController.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCanvas.h"
#include "Widgets/SOverlay.h"

#define LOCTEXT_NAMESPACE "WRUI"

namespace { constexpr float U = 1.2f; constexpr float WheelSize = 560.f * U; }

class SWRWheel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWRWheel) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AWRPlayerController>, PC)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		PC = InArgs._PC;
		TSharedRef<SCanvas> Labels = SNew(SCanvas);
		for (int32 i = 0; i < 8; ++i)
		{
			const float A = FMath::DegreesToRadians(i * 45.f);
			const FVector2D Centre = FVector2D(WheelSize * 0.5f) + FVector2D(FMath::Sin(A), -FMath::Cos(A)) * WheelSize * 0.355f;
			const FVector2D Box(150.f * U, 90.f * U);
			Labels->AddSlot().Position(Centre - Box * 0.5f).Size(Box)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(SBox).WidthOverride(44.f * U).HeightOverride(44.f * U)
					[
						SNew(SOverlay)
						+ SOverlay::Slot()[SNew(SImage).Image_Lambda([this, i]() { return WRUI::Box(IsLit(i) ? WRUI::Ember * 0.45f : FLinearColor(0.08f, 0.08f, 0.09f), 22.f * U, IsLit(i) ? WRUI::Brass : WRUI::Brass2, IsLit(i) ? 2.f : 1.f); })]
						+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
						[
							WRUI::Text(TAttribute<FText>::CreateLambda([this, i]() { return Glyph(i); }), WRUI::Title(17), TAttribute<FSlateColor>::CreateLambda([this, i]() { return FSlateColor(Empty(i) ? WRUI::Ink3 : IsLit(i) ? WRUI::Ink : GlyphColor(i)); }))
						]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 6.f * U, 0.f, 0.f)
				[
					WRUI::Text(TAttribute<FText>::CreateLambda([this, i]() { return Label(i); }), WRUI::Body(14, 800), TAttribute<FSlateColor>::CreateLambda([this, i]() { return FSlateColor(Empty(i) ? WRUI::Ink3 : IsDanger(i) ? WRUI::Bad : WRUI::Ink); }), true)
				]
			];
		}

		SetVisibility(TAttribute<EVisibility>::CreateLambda([this]() { const FWRPlayerUI* X = UI(); return X && Mode() == 2 && X->IsTameWheelSticky() ? EVisibility::Visible : EVisibility::HitTestInvisible; }));
		ChildSlot
		[
			SNew(SOverlay).Visibility_Lambda([this]() { return Mode() == 0 ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
			+ SOverlay::Slot()[SNew(SImage).Image(WRUI::White()).ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.35f))]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 14.f * U)[Header()]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(SBox).WidthOverride(WheelSize).HeightOverride(WheelSize)
					[
						SNew(SOverlay)
						+ SOverlay::Slot()[SNew(SImage).Image(WRUI::Image(TEXT("wheel_base")))]
						+ SOverlay::Slot()
						[
							SNew(SImage).Image(WRUI::Image(TEXT("wheel_hi")))
							.Visibility_Lambda([this]() { return Hover() >= 0 ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
							.RenderTransform_Lambda([this]() { return FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(FMath::Max(0, Hover()) * 45.f))); })
							.RenderTransformPivot(FVector2D(0.5f, 0.5f))
						]
						+ SOverlay::Slot()[Labels]
						+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[SNew(SBox).WidthOverride(WheelSize * 0.4f)[Centre()]]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 14.f * U, 0.f, 0.f)[Footer()]
			]
		];
	}

	virtual FReply OnMouseButtonDown(const FGeometry& Geo, const FPointerEvent& Event) override
	{
		// A tapped tame wheel waits for a click: the wedge under the cursor, or outside the wheel to cancel.
		FWRPlayerUI* X = UI();
		if (!X || Mode() != 2 || !X->IsTameWheelSticky()) return FReply::Unhandled();
		X->PickTameOrder(Event.GetEffectingButton() == EKeys::LeftMouseButton ? X->GetTameHover() : -1);
		return FReply::Handled();
	}

private:
	TWeakObjectPtr<AWRPlayerController> PC;

	FWRPlayerUI* UI() const { return PC.IsValid() ? PC->GetUI() : nullptr; }
	/** 0 closed, 1 an ability wheel, 2 the tame wheel. */
	int32 Mode() const { const FWRPlayerUI* X = UI(); return !X ? 0 : X->GetOpenWheel() >= 0 ? 1 : X->IsTameWheelOpen() ? 2 : 0; }
	int32 Wheel() const { const FWRPlayerUI* X = UI(); return X ? X->GetOpenWheel() : -1; }
	int32 Hover() const { const FWRPlayerUI* X = UI(); return !X ? -1 : Mode() == 1 ? X->GetWheelHover() : Mode() == 2 ? X->GetTameHover() : -1; }
	bool IsLit(int32 i) const { return Hover() == i && !Empty(i); }

	const FWRCastable* Ability(int32 i, TArray<FWRCastable>& Store) const
	{
		const FWRPlayerUI* X = UI();
		if (!X || Mode() != 1) return nullptr;
		Store = X->GetCastables();
		return X->FindCastable(X->GetWheelAbility(Wheel(), i), Store);
	}
	bool Empty(int32 i) const
	{
		if (Mode() == 2) return FWRPlayerUI::TameOrders()[i].Key.IsNone();
		TArray<FWRCastable> S;
		return Ability(i, S) == nullptr;
	}
	bool IsDanger(int32 i) const { return Mode() == 2 && FWRPlayerUI::TameOrders()[i].Key == TEXT("release"); }
	FText Label(int32 i) const
	{
		if (Mode() == 2) return FWRPlayerUI::TameOrders()[i].Value;
		TArray<FWRCastable> S;
		const FWRCastable* C = Ability(i, S);
		return C ? C->Name : LOCTEXT("EmptySlot", "Empty");
	}
	FText Glyph(int32 i) const
	{
		if (Mode() == 2)
		{
			static const TCHAR* G[] = { TEXT(">"), TEXT("||"), TEXT("^"), TEXT(""), TEXT("x"), TEXT(""), TEXT("~"), TEXT("!") };
			return FText::FromString(G[i]);
		}
		TArray<FWRCastable> S;
		const FWRCastable* C = Ability(i, S);
		return FText::FromString(C ? C->Name.ToString().Left(1) : TEXT("+"));
	}
	FLinearColor GlyphColor(int32 i) const
	{
		if (Mode() == 2) return IsDanger(i) ? WRUI::Bad : WRUI::Brass;
		TArray<FWRCastable> S;
		const FWRCastable* C = Ability(i, S);
		return C && C->Path == TEXT("frost") ? WRUI::Frost : WRUI::Ember;
	}

	TSharedRef<SWidget> Header()
	{
		return SNew(SWRPanel).Kind(EWRPanel::Stone).Rivets(true).Padding(FMargin(18.f * U, 8.f * U))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f * U, 0.f)
			[
				SNew(SWRKeyCap).FontSize(13).Text_Lambda([this]() { return WRUI::KeyName(FWRKeyBindings::Get().Key(Mode() == 2 ? TEXT("Interact") : Wheel() == 1 ? TEXT("Wheel2") : TEXT("Wheel1"))); })
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return Mode() == 2 ? LOCTEXT("TameOrders", "Tame orders") : Wheel() == 1 ? LOCTEXT("Wheel2", "Ability wheel 2") : LOCTEXT("Wheel1", "Ability wheel 1"); }), WRUI::Title(17), WRUI::Ink)
			]
		];
	}

	TSharedRef<SWidget> Centre()
	{
		return SNew(SVerticalBox)
			// The ability under the mouse (or the one picked), what it costs, what it does.
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(SBox).WidthOverride(76.f * U).HeightOverride(76.f * U).Visibility_Lambda([this]() { return Mode() == 2 ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				[
					SNew(SOverlay)
					+ SOverlay::Slot()[SNew(SImage).Image(WRUI::Box(FLinearColor(0.1f, 0.1f, 0.11f), 38.f * U, WRUI::Brass, 2.f))]
					+ SOverlay::Slot().Padding(4.f)[SNew(SImage).Image_Lambda([this]() { const AWRCreature* T = Tame(); const FSlateBrush* B = T ? WRUI::Portrait(T->CreatureId) : nullptr; return B ? B : WRUI::Icon(TEXT("kibble")); })]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 6.f * U, 0.f, 0.f)
			[
				WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return CentreTitle(); }), WRUI::Title(20), WRUI::Ink, true)
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 4.f * U, 0.f, 0.f)
			[
				WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return CentreSub(); }), WRUI::Body(13, 700), TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(Mode() == 2 ? WRUI::Good : WRUI::Ember); }))
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 8.f * U, 0.f, 0.f)
			[
				SNew(SBox).WidthOverride(WheelSize * 0.36f)
				[
					SNew(STextBlock).Text_Lambda([this]() { return CentreBody(); }).Font(WRUI::Body(13, 500)).ColorAndOpacity(WRUI::Ink2).AutoWrapText(true).Justification(ETextJustify::Center)
				]
			];
	}

	const AWRCreature* Tame() const { return PC.IsValid() ? PC->FindNearbyCreature(true, 800.f) : nullptr; }
	FText CentreTitle() const
	{
		if (Mode() == 2) { const AWRCreature* T = Tame(); return T ? FText::FromString(T->GetDisplayLabel()) : FText::GetEmpty(); }
		const FWRPlayerUI* X = UI();
		if (!X) return FText::GetEmpty();
		TArray<FWRCastable> S;
		const FWRCastable* C = Ability(Hover() >= 0 ? Hover() : X->GetWheelPicked(Wheel()), S);
		return C ? C->Name : LOCTEXT("NoAbility", "No ability here");
	}
	FText CentreSub() const
	{
		if (Mode() == 2)
		{
			const AWRCreature* T = Tame();
			if (!T) return FText::GetEmpty();
			return FText::FromString(FString::Printf(TEXT("%s · %s"), T->TameOrder == EWRTameOrder::Follow ? TEXT("Following") : TEXT("Staying"), T->bTameAggressive ? TEXT("aggressive") : TEXT("passive")));
		}
		TArray<FWRCastable> S;
		const FWRCastable* C = Ability(Hover() >= 0 ? Hover() : UI()->GetWheelPicked(Wheel()), S);
		if (!C) return FText::GetEmpty();
		return C->Cooldown > 0.f ? FText::FromString(FString::Printf(TEXT("Ready in %.1f s"), C->Cooldown)) : FText::FromString(C->Path == TEXT("frost") ? TEXT("Frost ability") : TEXT("Fire ability"));
	}
	FText CentreBody() const
	{
		if (Mode() == 2) return FText::GetEmpty();
		TArray<FWRCastable> S;
		const FWRPlayerUI* X = UI();
		const FWRCastable* C = Ability(Hover() >= 0 ? Hover() : X->GetWheelPicked(Wheel()), S);
		if (!C) return LOCTEXT("LearnMore", "Learn abilities in Paths: each one you learn joins a wheel.");
		FString PathStr, NodeStr;
		C->NodeId.ToString().Split(TEXT("."), &PathStr, &NodeStr);
		const FWRPathDef* Def = FWRPathRegistry::Get().Find(FName(*PathStr));
		const FWRPathNodeDef* Node = Def ? Def->FindNode(FName(*NodeStr)) : nullptr;
		return FText::FromString(Node ? Node->Text : FString());
	}

	TSharedRef<SWidget> Footer()
	{
		return SNew(SWRPanel).Kind(EWRPanel::Glass).Padding(FMargin(16.f * U, 7.f * U))
		[
			WRUI::Text(TAttribute<FText>::CreateLambda([this]()
			{
				if (Mode() == 2)
				{
					const FWRPlayerUI* X = UI();
					return X && X->IsTameWheelSticky() ? LOCTEXT("TameClick", "Click an order  ·  Esc to close") : LOCTEXT("TameRelease", "Release on an order");
				}
				const FText K = WRUI::KeyName(FWRKeyBindings::Get().Key(Wheel() == 1 ? TEXT("Wheel2") : TEXT("Wheel1")));
				return FWRKeyBindings::Get().CastOnRelease() ? FText::Format(LOCTEXT("CastRelease", "Release {0} to cast"), K) : FText::Format(LOCTEXT("KeepTap", "Release {0} to keep  ·  Tap {0} to cast"), K);
			}), WRUI::Body(13, 700), WRUI::Ink2)
		];
	}
};

WR_REGISTER_SCREEN("wheel", [](AWRPlayerController* PC) -> TSharedRef<SWidget> { return SNew(SWRWheel).PC(PC); });

#undef LOCTEXT_NAMESPACE
