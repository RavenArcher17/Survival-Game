// The journal's Skills tab (mockup "Life skills"): every life skill as a card by group (level, XP to the next level,
// the every-level bonus), and a side panel for the selected skill: its perk tiers at 25 / 50 / 75 / 100, the two
// options of a tier you've reached ("perk <skill> <tier> <option>", permanent), and why later tiers are locked.
// Skills with no way to level yet (no system behind them) show as coming soon. Sizes are mockup px x U.

#include "WRPlayerUI.h"
#include "WRUIStyle.h"
#include "WRUIWidgets.h"
#include "WRLifeSkills.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "WRUI"

// Named (not anonymous): unity builds put several UI files in one translation unit.
namespace WRSkillsTab
{
	using WRUI::U;
	constexpr float CardW = 240.f, ColGap = 14.f;

	FLinearColor Hex(const TCHAR* H) { return FLinearColor(FColor::FromHex(H)); }
	const FSlateBrush* Glyph(const FString& Name) { return WRUI::Image(FName(*(TEXT("Glyphs/") + Name))); }
	FString Lowered(const FString& S) { return S.IsEmpty() ? S : FString::Chr(FChar::ToLower(S[0])) + S.Mid(1); }

	/** Reads the replicated life skills of the local player. */
	struct FSkillsModel
	{
		TWeakObjectPtr<AWRPlayerController> PC;

		const UWRLifeSkillsComponent* Comp() const
		{
			const AWRPlayerState* PS = PC.IsValid() ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
			return PS ? PS->GetLifeSkills() : nullptr;
		}
		const FWRSkillProgress* Progress(FName Id) const { const UWRLifeSkillsComponent* C = Comp(); return C ? C->FindProgress(Id) : nullptr; }
		int32 Level(FName Id) const { const FWRSkillProgress* P = Progress(Id); return P ? P->Level : 1; }
		int32 XP(FName Id) const { const FWRSkillProgress* P = Progress(Id); return P ? P->XP : 0; }
		int8 Chosen(FName Id, int32 Tier) const { const FWRSkillProgress* P = Progress(Id); return P && P->Perks.IsValidIndex(Tier) ? P->Perks[Tier] : -1; }
		float XPFrac(FName Id) const
		{
			const int32 L = Level(Id);
			return L >= FWRLifeSkillRules::MaxLevel ? 1.f : FMath::Clamp(float(XP(Id)) / FMath::Max(1, FWRLifeSkillRules::XPToNext(L)), 0.f, 1.f);
		}

		/** No system feeds it XP yet: nothing in the data is wired to a stat (fishing, engineering, swimming). */
		bool IsSoon(const FWRLifeSkillDef& D) const
		{
			if (Level(D.Id) > 1 || XP(D.Id) > 0) return false;
			if (!D.EveryLevelStat.IsNone()) return false;
			for (const TArray<FWRPerkDef>& Tier : D.Tiers) for (const FWRPerkDef& P : Tier) if (!P.Stat.IsNone()) return false;
			return true;
		}
		/** First tier you've reached and not chosen yet (-1: none). */
		int32 PendingTier(const FWRLifeSkillDef& D) const
		{
			for (int32 T = 0; T < 4 && T < D.Tiers.Num(); ++T)
			{
				if (Level(D.Id) >= FWRLifeSkillRegistry::TierLevels[T] && Chosen(D.Id, T) < 0 && D.Tiers[T].Num() > 0) return T;
			}
			return -1;
		}
		int32 PendingCount() const
		{
			int32 N = 0;
			for (const FWRLifeSkillDef& D : FWRLifeSkillRegistry::Get().All()) if (PendingTier(D) >= 0) ++N;
			return N;
		}
		/** "+14%" from the every-level bonus (empty when it isn't wired to a stat). */
		FString Bonus(const FWRLifeSkillDef& D) const
		{
			if (D.EveryLevelStat.IsNone()) return FString();
			const float V = D.PerLevel * (Level(D.Id) - 1);
			return FMath::Abs(D.PerLevel) < 0.1f ? FString::Printf(TEXT("+%d%%"), FMath::RoundToInt(FMath::Abs(V) * 100.f)) : FString::Printf(TEXT("+%d"), FMath::RoundToInt(FMath::Abs(V)));
		}
		FString XPLine(const FWRLifeSkillDef& D) const
		{
			const int32 L = Level(D.Id);
			return L >= FWRLifeSkillRules::MaxLevel ? FString(TEXT("max level")) : FString::Printf(TEXT("%d%% to %d"), FMath::FloorToInt(XPFrac(D.Id) * 100.f), L + 1);
		}
	};

	/** The glyph colours of a skill medallion: picked, normal, or not in the game yet. */
	struct FMedal { FLinearColor Ring, Fill, P, Ac; };
	FMedal MedalFor(bool bSelected, bool bSoon)
	{
		if (bSoon) return { Hex(TEXT("34383D")), Hex(TEXT("1B1D20")), Hex(TEXT("5D6166")), Hex(TEXT("45494E")) };
		if (bSelected) return { WRUI::Brass, Hex(TEXT("3A2612")), Hex(TEXT("F6DCA4")), WRUI::Brass };
		return { WRUI::Brass2, Hex(TEXT("2A2017")), Hex(TEXT("E6C88E")), Hex(TEXT("A77F45")) };
	}

	TSharedRef<SWidget> Medal(FName SkillId, float Size, float GlyphSize, TFunction<FMedal()> Look, float RingW = 2.f)
	{
		const FString G = TEXT("skill_") + SkillId.ToString();
		return SNew(SBox).WidthOverride(Size).HeightOverride(Size)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()[SNew(SImage).Image(WRUI::Box(Hex(TEXT("0A0B0C")), Size * 0.5f))]
			+ SOverlay::Slot().Padding(2.f)[SNew(SImage).Image_Lambda([Look, Size, RingW]() { const FMedal M = Look(); return WRUI::Box(M.Fill, Size * 0.5f - 2.f, M.Ring, RingW); })]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(GlyphSize).HeightOverride(GlyphSize)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()[SNew(SImage).Image(Glyph(G + TEXT("_a"))).ColorAndOpacity_Lambda([Look]() { return Look().Ac; })]
					+ SOverlay::Slot()[SNew(SImage).Image(Glyph(G + TEXT("_p"))).ColorAndOpacity_Lambda([Look]() { return Look().P; })]
				]
			]
		];
	}

	/** A small pill with a glyph and text, on an opaque ground (it may sit across a card's edge). */
	TSharedRef<SWidget> IconChip(const FString& Icon, const TAttribute<FText>& Text, const FLinearColor& Color, const FLinearColor& Ground, int32 FontSize = 11)
	{
		return SNew(SBorder).BorderImage(WRUI::Box(Ground, 11.f * U, WRUI::Alpha(Color, 0.55f), 1.f)).Padding(FMargin(9.f * U, 1.f * U, 10.f * U, 2.f * U))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f * U, 0.f)
				[
					SNew(SBox).WidthOverride(10.f * U).HeightOverride(10.f * U)[SNew(SImage).Image(Glyph(Icon)).ColorAndOpacity(Color)]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(Text, WRUI::Body(FontSize, 700), Color)]
			];
	}

	TSharedRef<SWidget> GroupHeader(const FString& Name, const FLinearColor& Color)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(0.f, 0.f, 12.f * U, 0.f)[WRUI::Text(FText::FromString(Name), WRUI::Caps(13), Color)]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(0.f, 6.f * U, 0.f, 0.f)
			[
				SNew(SBox).HeightOverride(1.f)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity(WRUI::Alpha(Color, 0.45f))]
			];
	}

/** One skill card: medallion, name and level, XP bar, the every-level bonus; a click selects it. */
class SWRSkillCard : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWRSkillCard) {}
		SLATE_ARGUMENT(TSharedPtr<FSkillsModel>, Model)
		SLATE_ARGUMENT(FName, SkillId)
		SLATE_ATTRIBUTE(bool, Selected)
	SLATE_END_ARGS()

	TFunction<void(FName)> OnSelect;

	void Construct(const FArguments& InArgs)
	{
		Model = InArgs._Model;
		SkillId = InArgs._SkillId;
		Selected = InArgs._Selected;
		const FWRLifeSkillDef* D = FWRLifeSkillRegistry::Get().Find(SkillId);
		const FString Name = D ? D->Name : SkillId.ToString();
		const FString Every = D ? D->EveryLevelText : FString();

		ChildSlot
		[
			SNew(SOverlay)
			// Selected: a brass rim with a warm glow around it.
			+ SOverlay::Slot().Padding(-3.f * U)
			[
				SNew(SImage).Image(WRUI::Box(WRUI::Alpha(WRUI::Brass, 0.2f), 8.f)).Visibility_Lambda([this]() { return Selected.Get() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			]
			+ SOverlay::Slot()[SNew(SImage).Image_Lambda([this]()
			{
				const bool bSoon = Soon();
				return Selected.Get() ? WRUI::Box(Hex(TEXT("2B2823")), 5.f, WRUI::Brass, 1.f) : WRUI::Box(bSoon ? Hex(TEXT("181A1D")) : Hex(TEXT("222529")), 5.f, IsHovered() ? WRUI::Brass2 : Hex(TEXT("050506")), 1.f);
			})]
			+ SOverlay::Slot().VAlign(VAlign_Top).Padding(1.f)[SNew(SBox).HeightOverride(1.f)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.07f))]]
			+ SOverlay::Slot().Padding(FMargin(14.f * U, 12.f * U))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().FillHeight(1.f).VAlign(VAlign_Center)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f * U, 0.f)
					[
						Medal(SkillId, 52.f * U, 30.f * U, [this]() { return MedalFor(Selected.Get(), Soon()); })
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Bottom)
							[
								WRUI::Text(FText::FromString(Name), WRUI::Body(15, 800), TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(Soon() ? Hex(TEXT("8A8070")) : WRUI::Ink); }))
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(0.f, 0.f, 4.f * U, 2.f * U)
							[
								SNew(SBox).Visibility_Lambda([this]() { return Soon() ? EVisibility::Collapsed : EVisibility::Visible; })[WRUI::Text(LOCTEXT("Lv", "Lv"), WRUI::Caps(11), WRUI::Ink3)]
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
							[
								SNew(SBox).Visibility_Lambda([this]() { return Soon() ? EVisibility::Collapsed : EVisibility::Visible; })
								[
									WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::AsNumber(Model->Level(SkillId)); }), WRUI::Title(21), WRUI::Ink)
								]
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 7.f * U, 0.f, 0.f)
						[
							SNew(SOverlay)
							+ SOverlay::Slot()
							[
								SNew(SHorizontalBox).Visibility_Lambda([this]() { return Soon() ? EVisibility::Collapsed : EVisibility::Visible; })
								+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
								[
									SNew(SWRBar).Height(8.f * U).Color(WRUI::Brass).Fraction_Lambda([this]() { return Model->XPFrac(SkillId); })
								]
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f * U, 0.f, 0.f, 0.f)
								[
									WRUI::Text(TAttribute<FText>::CreateLambda([this]() { const FWRLifeSkillDef* D2 = FWRLifeSkillRegistry::Get().Find(SkillId); return D2 ? FText::FromString(Model->XPLine(*D2)) : FText::GetEmpty(); }), WRUI::Body(11, 700), WRUI::Ink3)
								]
							]
							+ SOverlay::Slot().HAlign(HAlign_Left)
							[
								SNew(SBox).Visibility_Lambda([this]() { return Soon() ? EVisibility::Visible : EVisibility::Collapsed; })
								[
									SNew(SWRChip).Text(LOCTEXT("ComingSoon", "Coming soon")).Color(WRUI::Ink2).FontSize(11)
								]
							]
						]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f * U, 0.f, 0.f)
				[
					SNew(SBox).MinDesiredHeight(20.f * U).VAlign(VAlign_Center)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							WRUI::Text(TAttribute<FText>::CreateLambda([this, Every]() { return Soon() ? LOCTEXT("LaterUpdate", "Arrives in a later update") : FText::FromString(Every); }), WRUI::Body(13, 500),
								TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(Soon() ? WRUI::Ink3 : WRUI::Ink2); }))
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(5.f * U, 0.f, 0.f, 0.f)
						[
							WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return BonusText(); }), WRUI::Body(13, 800),
								TAttribute<FSlateColor>::CreateLambda([this]() { const FWRLifeSkillDef* D2 = FWRLifeSkillRegistry::Get().Find(SkillId); return FSlateColor(D2 && !D2->EveryLevelStat.IsNone() && Model->Level(SkillId) > 1 ? WRUI::Good : WRUI::Ink3); }))
						]
					]
				]
			]
			// A perk waits: a chip across the top edge.
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(0.f, 0.f, 10.f * U, 0.f)
			[
				SNew(SBox).RenderTransform(FSlateRenderTransform(FVector2D(0.f, -11.f * U))).Visibility_Lambda([this]() { const FWRLifeSkillDef* D2 = FWRLifeSkillRegistry::Get().Find(SkillId); return D2 && Model->PendingTier(*D2) >= 0 ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				[
					IconChip(TEXT("diamond"), LOCTEXT("PerkReady", "Perk ready"), WRUI::Warn, Hex(TEXT("231C10")))
				]
			]
		];
		SetCursor(EMouseCursor::Hand);
	}

	virtual FReply OnMouseButtonDown(const FGeometry& Geo, const FPointerEvent& Event) override { return FReply::Handled(); }
	virtual FReply OnMouseButtonUp(const FGeometry& Geo, const FPointerEvent& Event) override
	{
		if (Event.GetEffectingButton() == EKeys::LeftMouseButton && OnSelect) OnSelect(SkillId);
		return FReply::Handled();
	}

private:
	TSharedPtr<FSkillsModel> Model;
	FName SkillId;
	TAttribute<bool> Selected;

	bool Soon() const { const FWRLifeSkillDef* D = FWRLifeSkillRegistry::Get().Find(SkillId); return D && Model->IsSoon(*D); }
	FText BonusText() const
	{
		const FWRLifeSkillDef* D = FWRLifeSkillRegistry::Get().Find(SkillId);
		if (!D || Soon()) return FText::GetEmpty();
		// Every-level bonuses not wired to a system yet say so rather than show a number that does nothing.
		return D->EveryLevelStat.IsNone() ? LOCTEXT("BonusSoon", "· coming") : FText::FromString(Model->Bonus(*D));
	}
};

class SWRSkillsTab : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWRSkillsTab) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AWRPlayerController>, PC)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		PC = InArgs._PC;
		Model = MakeShared<FSkillsModel>();
		Model->PC = PC;
		Selected = DefaultSelection();

		ChildSlot
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).WidthOverride(1002.f * U)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[SNew(SBox).HeightOverride(40.f * U)[Header()]]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f * U, 0.f, 0.f)[SNew(SBox).HeightOverride(640.f * U)[Grid()]]
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(22.f * U, 0.f, 0.f, 0.f)
			[
				SNew(SWRPanel).Kind(EWRPanel::Leather).Rivets(true).Padding(FMargin(22.f * U, 24.f * U, 22.f * U, 22.f * U))
				[
					SAssignNew(SideBox, SBox)
				]
			]
		];
	}

	virtual void Tick(const FGeometry& Geo, const double Time, const float Delta) override
	{
		SCompoundWidget::Tick(Geo, Time, Delta);
		FString Sig = Selected.ToString();
		if (const FWRSkillProgress* P = Model->Progress(Selected))
		{
			Sig += FString::Printf(TEXT("|%d|%d"), P->Level, P->XP);
			for (const int8 C : P->Perks) Sig += FString::Printf(TEXT("|%d"), C);
		}
		if (Sig != SideSig)
		{
			SideSig = Sig;
			SideBox->SetContent(Side());
		}
	}

private:
	TWeakObjectPtr<AWRPlayerController> PC;
	TSharedPtr<FSkillsModel> Model;
	TSharedPtr<SBox> SideBox;
	FName Selected;
	FString SideSig;

	/** A skill with a perk waiting, else the one you've raised most. */
	FName DefaultSelection() const
	{
		const TArray<FWRLifeSkillDef>& All = FWRLifeSkillRegistry::Get().All();
		for (const FWRLifeSkillDef& D : All) if (Model->PendingTier(D) >= 0) return D.Id;
		FName Best = All.Num() > 0 ? All[0].Id : NAME_None;
		int32 BestLv = 0;
		for (const FWRLifeSkillDef& D : All)
		{
			const int32 L = Model->Level(D.Id) * 1000 + Model->XP(D.Id);
			if (L > BestLv) { BestLv = L; Best = D.Id; }
		}
		return Best;
	}

	TSharedRef<SWidget> Header()
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(LOCTEXT("LifeSkills", "Life skills"), WRUI::Title(21), WRUI::Ink)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(16.f * U, 3.f * U, 0.f, 0.f)
			[
				WRUI::Text(LOCTEXT("LifeSkillsSub", "Level 1 to 100 by doing the work · a perk choice every 25 levels"), WRUI::Body(14, 500), WRUI::Ink2)
			]
			+ SHorizontalBox::Slot().FillWidth(1.f)[SNullWidget::NullWidget]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).HeightOverride(30.f * U).VAlign(VAlign_Center).Visibility_Lambda([this]() { return Model->PendingCount() > 0 ? EVisibility::Visible : EVisibility::Collapsed; })
				[
					IconChip(TEXT("diamond"), TAttribute<FText>::CreateLambda([this]() { const int32 N = Model->PendingCount(); return FText::Format(LOCTEXT("PerksToChoose", "{0} {0}|plural(one=perk,other=perks) to choose"), N); }),
						WRUI::Warn, FLinearColor(0.f, 0.f, 0.f, 0.5f), 12)
				]
			];
	}

	// ---------------------------------------------------------------- the grid of cards

	TSharedRef<SWidget> Card(FName Id)
	{
		TSharedRef<SWRSkillCard> C = SNew(SWRSkillCard).Model(Model).SkillId(Id).Selected_Lambda([this, Id]() { return Selected == Id; });
		C->OnSelect = [this](FName S) { Selected = S; };
		return C;
	}

	/** The four tier diamonds of the selected skill (the mockup's "Perk choices" box). */
	TSharedRef<SWidget> PerkKey()
	{
		TSharedRef<SHorizontalBox> Marks = SNew(SHorizontalBox);
		for (int32 T = 0; T < 4; ++T)
		{
			if (T > 0) Marks->AddSlot().FillWidth(1.f).VAlign(VAlign_Top).Padding(0.f, 8.f * U, 0.f, 0.f)[SNew(SBox).HeightOverride(2.f * U)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity(Hex(TEXT("3A3E44")))]];
			Marks->AddSlot().AutoWidth()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(SBox).WidthOverride(18.f * U).HeightOverride(18.f * U)
					[
						SNew(SOverlay)
						+ SOverlay::Slot()[SNew(SImage).Image(Glyph(TEXT("diamond"))).ColorAndOpacity_Lambda([this, T]() { return TierColour(T, true); })]
						+ SOverlay::Slot()[SNew(SImage).Image(Glyph(TEXT("diamond_line"))).ColorAndOpacity_Lambda([this, T]() { return TierColour(T, false); })]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 3.f * U, 0.f, 0.f)
				[
					WRUI::Text(FText::AsNumber(FWRLifeSkillRegistry::TierLevels[T]), WRUI::Body(11, 800), TAttribute<FSlateColor>::CreateLambda([this, T]() { return FSlateColor(TierState(T) == 0 ? WRUI::Ink3 : TierState(T) == 1 ? WRUI::Warn : WRUI::Brass); }))
				]
			];
		}
		return SNew(SBorder).BorderImage(WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.22f), 5.f, WRUI::Alpha(WRUI::Brass, 0.4f), 1.f)).Padding(FMargin(16.f * U, 10.f * U))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().FillHeight(1.f).VAlign(VAlign_Center)[WRUI::Text(LOCTEXT("OneChoice", "One choice every 25 levels"), WRUI::Body(13, 500), WRUI::Ink2)]
				+ SVerticalBox::Slot().AutoHeight().Padding(2.f * U, 6.f * U)[Marks]
				+ SVerticalBox::Slot().FillHeight(1.f).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Font(WRUI::Body(12, 500)).ColorAndOpacity(WRUI::Ink2).AutoWrapText(true).Text_Lambda([this]() { return KeyLine(); })
				]
			];
	}

	/** 0 locked, 1 ready to choose, 2 chosen. */
	int32 TierState(int32 T) const
	{
		if (Model->Chosen(Selected, T) >= 0) return 2;
		return Model->Level(Selected) >= FWRLifeSkillRegistry::TierLevels[T] ? 1 : 0;
	}
	FLinearColor TierColour(int32 T, bool bFill) const
	{
		const int32 S = TierState(T);
		if (bFill) return S == 2 ? WRUI::Brass : S == 1 ? WRUI::Warn : Hex(TEXT("1D1F22"));
		return S == 2 ? Hex(TEXT("F6DCA4")) : S == 1 ? Hex(TEXT("2A1D12")) : Hex(TEXT("6A6E73"));
	}
	FText KeyLine() const
	{
		const FWRLifeSkillDef* D = FWRLifeSkillRegistry::Get().Find(Selected);
		if (!D) return FText::GetEmpty();
		const int32 P = Model->PendingTier(*D);
		if (P >= 0) return FText::Format(LOCTEXT("KeyReady", "{0} is ready to choose"), FText::FromString(D->Name));
		for (int32 T = 0; T < 4; ++T)
		{
			if (TierState(T) == 0) return FText::Format(LOCTEXT("KeyNext", "{0}: next choice at level {1}"), FText::FromString(D->Name), FWRLifeSkillRegistry::TierLevels[T]);
		}
		return FText::Format(LOCTEXT("KeyAll", "{0}: every perk chosen"), FText::FromString(D->Name));
	}

	TSharedRef<SWidget> Grid()
	{
		// Groups in data order; a group with four skills takes a row, smaller ones share the last row, and the perk key
		// fills a free column of the last row (the mockup's layout, from the data).
		struct FGroup { FString Name; TArray<FName> Skills; };
		TArray<FGroup> Groups;
		for (const FWRLifeSkillDef& D : FWRLifeSkillRegistry::Get().All())
		{
			if (Groups.Num() == 0 || Groups.Last().Name != D.Group) Groups.Add({ D.Group, {} });
			Groups.Last().Skills.Add(D.Id);
		}
		struct FPlaced { FString Name; TArray<FName> Skills; bool bKey = false; };
		TArray<TArray<FPlaced>> Rows;
		int32 Used = 4;
		for (const FGroup& G : Groups)
		{
			for (int32 i = 0; i < G.Skills.Num(); i += 4)
			{
				const int32 N = FMath::Min(4, G.Skills.Num() - i);
				if (Used + N > 4) { Rows.AddDefaulted(); Used = 0; }
				FPlaced& P = Rows.Last().AddDefaulted_GetRef();
				P.Name = G.Name;
				for (int32 k = 0; k < N; ++k) P.Skills.Add(G.Skills[i + k]);
				Used += N;
			}
		}
		if (Rows.Num() > 0 && Used < 4) { FPlaced& K = Rows.Last().AddDefaulted_GetRef(); K.Name = TEXT("Perk choices"); K.bKey = true; }

		TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
		for (int32 r = 0; r < Rows.Num(); ++r)
		{
			TSharedRef<SHorizontalBox> Heads = SNew(SHorizontalBox);
			TSharedRef<SHorizontalBox> Cards = SNew(SHorizontalBox);
			int32 Col = 0;
			for (const FPlaced& P : Rows[r])
			{
				const int32 Span = P.bKey ? 1 : P.Skills.Num();
				const float W = Span * CardW + (Span - 1) * ColGap;
				const float Gap = Col > 0 ? ColGap * U : 0.f;
				Heads->AddSlot().AutoWidth().Padding(Gap, 0.f, 0.f, 0.f)[SNew(SBox).WidthOverride(W * U).VAlign(VAlign_Bottom)[GroupHeader(P.Name, P.bKey ? WRUI::Ink3 : WRUI::Brass)]];
				if (P.bKey)
				{
					Cards->AddSlot().AutoWidth().Padding(Gap, 0.f, 0.f, 0.f)[SNew(SBox).WidthOverride(CardW * U)[PerkKey()]];
				}
				else
				{
					for (int32 k = 0; k < P.Skills.Num(); ++k)
					{
						Cards->AddSlot().AutoWidth().Padding(k == 0 ? Gap : ColGap * U, 0.f, 0.f, 0.f)[SNew(SBox).WidthOverride(CardW * U)[Card(P.Skills[k])]];
					}
				}
				Col += Span;
			}
			Box->AddSlot().AutoHeight().Padding(0.f, r > 0 ? 8.f * U : 0.f, 0.f, 0.f)[SNew(SBox).HeightOverride(28.f * U).Padding(FMargin(0.f, 0.f, 0.f, 2.f * U))[Heads]];
			Box->AddSlot().AutoHeight().Padding(0.f, 8.f * U, 0.f, 0.f)[SNew(SBox).HeightOverride(118.f * U)[Cards]];
		}
		return Box;
	}

	// ---------------------------------------------------------------- the side panel: the selected skill's perks

	TSharedRef<SWidget> PerkOption(const FWRLifeSkillDef& D, int32 Tier, int32 Option)
	{
		const FWRPerkDef& Perk = D.Tiers[Tier][Option];
		const bool bLive = !Perk.Stat.IsNone();
		const FName SkillId = D.Id;
		const int32 TierLevel = FWRLifeSkillRegistry::TierLevels[Tier];
		TSharedRef<SVerticalBox> Words = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(FText::FromString(Perk.Name), WRUI::Title(17), WRUI::Ink)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f * U, 0.f, 0.f)[SNew(STextBlock).Text(FText::FromString(Perk.Text + TEXT("."))).Font(WRUI::Body(14, 500)).ColorAndOpacity(WRUI::Ink2).AutoWrapText(true)];
		if (!bLive)
		{
			Words->AddSlot().AutoHeight().Padding(0.f, 3.f * U, 0.f, 0.f)[WRUI::Text(LOCTEXT("PerkSoon", "Takes effect once its system is in"), WRUI::Body(12, 500), WRUI::Ink3)];
		}
		return SNew(SWRPanel).Kind(EWRPanel::Stone).Rivets(true).Padding(FMargin(16.f * U, 14.f * U, 16.f * U, 12.f * U))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 14.f * U, 0.f)
					[
						SNew(SBox).WidthOverride(60.f * U).HeightOverride(60.f * U)
						[
							SNew(SOverlay)
							+ SOverlay::Slot()[Medal(SkillId, 58.f * U, 34.f * U, []() { return MedalFor(true, false); })]
							+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom)
							[
								SNew(SBox).WidthOverride(22.f * U).HeightOverride(22.f * U)
								[
									SNew(SOverlay)
									+ SOverlay::Slot()[SNew(SImage).Image(bLive ? WRUI::Box(Hex(TEXT("1B2A14")), 11.f * U, WRUI::Good, 1.f) : WRUI::Box(Hex(TEXT("1A1C1F")), 11.f * U, Hex(TEXT("5A5F66")), 1.f))]
									+ SOverlay::Slot().Padding(5.f * U)[SNew(SImage).Image(Glyph(bLive ? TEXT("plus") : TEXT("diamond_line"))).ColorAndOpacity(bLive ? WRUI::Good : WRUI::Ink3)]
								]
							]
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Words]
				]
				// SWRButton's leather tile would size it to 256 units in an auto-height slot: give it its height.
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f * U, 0.f, 0.f)
				[
					SNew(SBox).HeightOverride(38.f * U)
					[
						SNew(SWRButton).Text(LOCTEXT("Choose", "Choose")).Primary(true).FontSize(14).MinHeight(38.f * U)
						.OnClicked_Lambda([this, SkillId, TierLevel, Option]()
						{
							if (PC.IsValid()) PC->SendCommand(FString::Printf(TEXT("perk %s %d %d"), *SkillId.ToString(), TierLevel, Option));
							return FReply::Handled();
						})
					]
				]
			];
	}

	/** A reached or locked tier as a few lines: its options, the one you took, or why it's shut. */
	TSharedRef<SWidget> TierLines(const FWRLifeSkillDef& D, int32 T, bool bCompact)
	{
		const int32 TierLevel = FWRLifeSkillRegistry::TierLevels[T];
		const int8 Pick = Model->Chosen(D.Id, T);
		const int32 Lv = Model->Level(D.Id);
		const bool bOpen = Lv >= TierLevel;
		const FLinearColor Mark = Pick >= 0 ? WRUI::Brass : bOpen ? WRUI::Warn : Hex(TEXT("6A6E73"));
		FText Status = Pick >= 0 ? FText::Format(LOCTEXT("TierChosen", "Chosen: {0}"), FText::FromString(D.Tiers[T].IsValidIndex(Pick) ? D.Tiers[T][Pick].Name : FString()))
			: bOpen ? LOCTEXT("TierReady", "Ready to choose")
			: FText::Format(LOCTEXT("TierLocked", "Locked: reach level {0} (you are {1})"), TierLevel, Lv);
		TSharedRef<SVerticalBox> Box = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f * U, 0.f)
				[
					SNew(SBox).WidthOverride(14.f * U).HeightOverride(14.f * U)[SNew(SImage).Image(Glyph(Pick >= 0 ? TEXT("diamond") : TEXT("diamond_line"))).ColorAndOpacity(Mark)]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f * U, 0.f)
				[
					WRUI::Text(FText::Format(LOCTEXT("TierLevel", "Level {0}"), TierLevel), WRUI::Title(14), Pick >= 0 || bOpen ? WRUI::Ink : WRUI::Ink2)
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(Status).Font(WRUI::Body(12, 700)).ColorAndOpacity(Pick >= 0 ? WRUI::Good : bOpen ? WRUI::Warn : WRUI::Ink3).AutoWrapText(true)
				]
			];
		if (bCompact) return Box;
		for (int32 O = 0; O < D.Tiers[T].Num(); ++O)
		{
			const FWRPerkDef& P = D.Tiers[T][O];
			const bool bTaken = Pick == O;
			const bool bPassed = Pick >= 0 && !bTaken;
			Box->AddSlot().AutoHeight().Padding(22.f * U, 4.f * U, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.f, 3.f * U, 6.f * U, 0.f)
				[
					SNew(SBox).WidthOverride(11.f * U).HeightOverride(11.f * U)
					[
						SNew(SImage).Image(Glyph(bTaken ? TEXT("check") : TEXT("diamond_line"))).ColorAndOpacity(bTaken ? WRUI::Good : WRUI::Alpha(WRUI::Ink3, 0.7f))
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(STextBlock).AutoWrapText(true).Font(WRUI::Body(13, bTaken ? 800 : 500))
					.ColorAndOpacity(bTaken ? WRUI::Ink : bPassed ? WRUI::Alpha(WRUI::Ink3, 0.7f) : WRUI::Ink2)
					.Text(FText::FromString(FString::Printf(TEXT("%s: %s"), *P.Name, *Lowered(P.Text))))
				]
			];
		}
		return Box;
	}

	TSharedRef<SWidget> Side()
	{
		const FWRLifeSkillDef* D = FWRLifeSkillRegistry::Get().Find(Selected);
		if (!D) return SNullWidget::NullWidget;
		const bool bSoon = Model->IsSoon(*D);
		const int32 Lv = Model->Level(D->Id);
		const int32 Pending = Model->PendingTier(*D);
		const FString Bonus = Model->Bonus(*D);

		TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
		const FText Over = bSoon ? LOCTEXT("SideSoon", "Coming soon")
			: Pending >= 0 ? FText::Format(LOCTEXT("SideReached", "Level {0} reached"), FWRLifeSkillRegistry::TierLevels[Pending])
			: FText::Format(LOCTEXT("SideLevel", "Level {0} · {1}"), Lv, FText::FromString(Model->XPLine(*D)));
		Box->AddSlot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 14.f * U, 0.f)
			[
				Medal(D->Id, 64.f * U, 36.f * U, [bSoon]() { return MedalFor(!bSoon, bSoon); }, 3.f)
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(Over, WRUI::Caps(12), Pending >= 0 ? WRUI::Warn : WRUI::Ink3)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f * U, 0.f, 0.f)[WRUI::Text(FText::FromString(D->Name), WRUI::Title(D->Name.Len() > 11 ? 20 : 23), WRUI::Ink)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f * U, 0.f, 0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()[WRUI::Text(FText::FromString(D->EveryLevelText), WRUI::Body(13, 500), WRUI::Ink2)]
					+ SHorizontalBox::Slot().AutoWidth().Padding(5.f * U, 0.f, 0.f, 0.f)
					[
						WRUI::Text(Bonus.IsEmpty() ? LOCTEXT("SideBonusSoon", "· coming") : FText::FromString(Bonus), WRUI::Body(13, 800), Bonus.IsEmpty() ? WRUI::Ink3 : WRUI::Good)
					]
				]
			]
		];
		Box->AddSlot().AutoHeight().Padding(0.f, 14.f * U)[WRUI::Divider()];

		if (Pending >= 0)
		{
			Box->AddSlot().AutoHeight()[WRUI::Text(LOCTEXT("ChoosePerk", "Choose a perk"), WRUI::Title(18), WRUI::Ink)];
			Box->AddSlot().AutoHeight().Padding(0.f, 3.f * U, 0.f, 12.f * U)[WRUI::Text(LOCTEXT("PickOne", "Pick one of the two. The choice is for good."), WRUI::Body(13, 500), WRUI::Ink2)];
			for (int32 O = 0; O < D->Tiers[Pending].Num(); ++O)
			{
				if (O > 0)
				{
					Box->AddSlot().AutoHeight().Padding(0.f, 6.f * U)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[SNew(SBox).HeightOverride(1.f)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity(WRUI::Alpha(Hex(TEXT("D2AD72")), 0.35f))]]
						+ SHorizontalBox::Slot().AutoWidth().Padding(10.f * U, 0.f)[WRUI::Text(LOCTEXT("Or", "or"), WRUI::Caps(11), WRUI::Ink2)]
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[SNew(SBox).HeightOverride(1.f)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity(WRUI::Alpha(Hex(TEXT("D2AD72")), 0.35f))]]
					];
				}
				Box->AddSlot().AutoHeight()[PerkOption(*D, Pending, O)];
			}
			// The other tiers in a line each.
			for (int32 T = 0; T < 4 && T < D->Tiers.Num(); ++T)
			{
				if (T == Pending) continue;
				if (Model->Chosen(D->Id, T) < 0 && T > Pending + 1) continue;
				Box->AddSlot().AutoHeight().Padding(0.f, 12.f * U, 0.f, 0.f)[TierLines(*D, T, true)];
			}
		}
		else
		{
			Box->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f * U)[WRUI::Text(LOCTEXT("Perks", "Perks"), WRUI::Title(18), WRUI::Ink)];
			for (int32 T = 0; T < 4 && T < D->Tiers.Num(); ++T)
			{
				Box->AddSlot().AutoHeight().Padding(0.f, T > 0 ? 10.f * U : 0.f, 0.f, 0.f)[TierLines(*D, T, false)];
			}
		}

		Box->AddSlot().FillHeight(1.f)[SNullWidget::NullWidget];
		Box->AddSlot().AutoHeight().Padding(0.f, 12.f * U, 0.f, 0.f)
		[
			SNew(SWRPanel).Kind(EWRPanel::Parchment).Padding(FMargin(16.f * U, 11.f * U, 16.f * U, 12.f * U))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(LOCTEXT("HowYouLevel", "How you level"), WRUI::Caps(12), Hex(TEXT("4A3420")))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f * U, 0.f, 0.f)
				[
					SNew(STextBlock).AutoWrapText(true).Font(WRUI::Body(14, 500)).ColorAndOpacity(WRUI::InkDark)
					.Text(FText::Format(LOCTEXT("LevelsBy", "{0} levels by {1}."), FText::FromString(D->Name), FText::FromString(Lowered(D->LevelsBy))))
				]
			]
		];
		return Box;
	}
};

} // namespace WRSkillsTab

WR_REGISTER_JOURNAL_TAB("skills", [](AWRPlayerController* PC) -> TSharedRef<SWidget> { return SNew(WRSkillsTab::SWRSkillsTab).PC(PC); });

#undef LOCTEXT_NAMESPACE
