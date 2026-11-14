// A tame's card (EWRMenu::TameCard; the tame is the controller's MenuCreature; cooks/build-taming.md 2.8), opened from
// the tame wheel's Tend. The stone-and-leather window: its name, sex, level, element and bond (the bar to its next bond
// level); its six stats (points, the multiplier they give, the share that came from mutations, its morphs); its role
// tree (four skills in a line and the two capstones, Learn on each with what it costs and what it needs); its saddle
// and barding (fit one from your bag, take it off); its family (mating on / off, her rest or the young she carries, a
// baby's growth and imprint, the care it's asking for with an Answer button, both lines' mutation counts); hunger
// (feed it by hand); and a hauler's packs beside your bag (click to move). Every button sends the server's tamecard
// commands; everything shown reads the creature's replicated state, so mouse and pad both work (the pad drives the
// buttons and slots, WRPadInput). It closes itself if the tame goes or you walk off.

#include "WRPlayerUI.h"
#include "WRUIStyle.h"
#include "WRUIWidgets.h"
#include "WRCreature.h"
#include "WRTaming.h"
#include "WRInventory.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRSound.h"
#include "WRTuning.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBackgroundBlur.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"

#define LOCTEXT_NAMESPACE "WRUI"

namespace WRUITameCardScreen
{
	using WRUI::U;

	FString ItemName(FName Id)
	{
		const FWRItemDef* D = FWRItemRegistry::Get().Find(Id);
		return D ? D->Name : Id.ToString();
	}

	FString Clock(float Seconds)
	{
		const int32 S = FMath::Max(0, FMath::RoundToInt(Seconds));
		return S >= 3600 ? FString::Printf(TEXT("%dh %02dm"), S / 3600, (S / 60) % 60) : FString::Printf(TEXT("%d:%02d"), S / 60, S % 60);
	}

	class STameCard : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(STameCard) {}
			SLATE_ARGUMENT(TWeakObjectPtr<AWRPlayerController>, PC)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			PC = InArgs._PC;
			Tame = PC.IsValid() ? PC->GetMenuCreature() : nullptr;
			const AWRCreature* C = Tame.Get();
			const FWRCreatureDef* Def = C ? C->GetDef() : nullptr;
			Tree = Def ? FWRTameTrees::Get().For(*Def) : nullptr;
			Kind = Def ? FWRTaming::MountKind(*Def) : NAME_None;
			bHauler = Def && Def->Carry > 0.f;

			TSharedRef<SVerticalBox> Left = SNew(SVerticalBox);
			TSharedRef<SVerticalBox> Right = SNew(SVerticalBox);
			const auto Heading = [](const FText& T) { return WRUI::Text(T, WRUI::Caps(11), WRUI::Ink3); };

			// --- Who it is: level, bond, element, hunger.
			Left->AddSlot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(Headline()); }), WRUI::Body(14, 600), WRUI::Ink, false, true)];
			Left->AddSlot().AutoHeight().Padding(0.f, 10.f * U, 0.f, 3.f * U)[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(BondLine()); }), WRUI::Body(12), WRUI::Ink2)];
			Left->AddSlot().AutoHeight()[SNew(SWRBar).Height(10.f * U).Color(WRUI::Brass).Fraction_Lambda([this]() { return BondFraction(); })];
			Left->AddSlot().AutoHeight().Padding(0.f, 8.f * U, 0.f, 3.f * U)[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { const AWRCreature* T = Tame.Get(); return FText::FromString(T ? FString::Printf(TEXT("Hunger %.0f%%  ·  eats %s or kibble"), T->TameRep.Hunger, *FoodName()) : FString()); }), WRUI::Body(12), WRUI::Ink2)];
			Left->AddSlot().AutoHeight()[SNew(SWRBar).Height(8.f * U).Fraction_Lambda([this]() { const AWRCreature* T = Tame.Get(); return T ? T->TameRep.Hunger / 100.f : 0.f; }).Color_Lambda([this]() { const AWRCreature* T = Tame.Get(); return T && T->TameRep.Hunger < 25.f ? WRUI::Bad : WRUI::Good; })];
			Left->AddSlot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 0.f).HAlign(HAlign_Left)
			[
				SNew(SWRButton).Ghost(true).MinHeight(36.f * U).FontSize(12).Text(LOCTEXT("TameFeed", "Feed it from your bag"))
				.OnClicked_Lambda([this]() { Send(TEXT("feed")); return FReply::Handled(); })
			];

			// --- Its six stats.
			Left->AddSlot().AutoHeight().Padding(0.f, 14.f * U, 0.f, 4.f * U)[Heading(LOCTEXT("TameStats", "ITS STATS"))];
			for (int32 s = 0; s < WRTameStat::Count; ++s)
			{
				Left->AddSlot().AutoHeight().Padding(0.f, 1.f * U)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(0.34f)[WRUI::Text(FText::FromString(WRTameStat::Label(s)), WRUI::Body(13, 600), WRUI::Ink)]
					+ SHorizontalBox::Slot().FillWidth(0.66f)[WRUI::Text(TAttribute<FText>::CreateLambda([this, s]() { return FText::FromString(StatLine(s)); }), WRUI::Body(13), TAttribute<FSlateColor>::CreateLambda([this, s]() { return FSlateColor(StatColour(s)); }))]
				];
			}
			Left->AddSlot().AutoHeight().Padding(0.f, 4.f * U, 0.f, 0.f)[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(MorphLine()); }), WRUI::Body(12), WRUI::Ink2, false, true)];

			// --- Family: mating, pregnancy, growth, imprint, care.
			Left->AddSlot().AutoHeight().Padding(0.f, 14.f * U, 0.f, 4.f * U)[Heading(LOCTEXT("TameFamily", "FAMILY"))];
			Left->AddSlot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(FamilyLine()); }), WRUI::Body(13), WRUI::Ink2, false, true)];
			Left->AddSlot().AutoHeight().Padding(0.f, 4.f * U, 0.f, 0.f)
			[
				SNew(SBox).Visibility_Lambda([this]() { const AWRCreature* T = Tame.Get(); return T && (T->IsBaby() || T->TameRep.Pregnant > 0.f || T->TameRep.MateBar > 0.f) ? EVisibility::Visible : EVisibility::Collapsed; })
				[
					SNew(SWRBar).Height(8.f * U).Color(WRUI::Brass).Fraction_Lambda([this]() { return FamilyFraction(); })
				]
			];
			Left->AddSlot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 0.f).HAlign(HAlign_Left)
			[
				SNew(SWRButton).Ghost(true).MinHeight(36.f * U).FontSize(12)
				.Visibility_Lambda([this]() { const AWRCreature* T = Tame.Get(); return T && !T->IsBaby() && IsMine() ? EVisibility::Visible : EVisibility::Collapsed; })
				.Text_Lambda([this]() { const AWRCreature* T = Tame.Get(); return T && T->TameRep.bMating ? LOCTEXT("TameMateOff", "Mating: on (turn off)") : LOCTEXT("TameMateOn", "Mating: off (turn on)"); })
				.OnClicked_Lambda([this]() { const AWRCreature* T = Tame.Get(); Send(T && T->TameRep.bMating ? TEXT("mate off") : TEXT("mate on")); return FReply::Handled(); })
			];
			Left->AddSlot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 0.f).HAlign(HAlign_Left)
			[
				SNew(SWRButton).Primary(true).MinHeight(42.f * U).FontSize(13)
				.Visibility_Lambda([this]() { const AWRCreature* T = Tame.Get(); return T && T->TameRep.Care >= 1 && T->TameRep.Care <= 3 ? EVisibility::Visible : EVisibility::Collapsed; })
				.Text_Lambda([this]() { return FText::FromString(CareButton()); })
				.OnClicked_Lambda([this]() { Send(TEXT("care")); return FReply::Handled(); })
			];

			// --- The role tree.
			Right->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f * U)[Heading(FText::FromString(Tree ? FString::Printf(TEXT("%s SKILLS  ·  ONE POINT A BOND LEVEL"), *Tree->Name.ToUpper()) : FString(TEXT("NO SKILL TREE"))))];
			Right->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f * U)[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(FString::Printf(TEXT("%d skill point%s free"), FreePoints(), FreePoints() == 1 ? TEXT("") : TEXT("s"))); }), WRUI::Body(13, 600), WRUI::Brass)];
			for (int32 i = 0; Tree && i < Tree->Nodes.Num(); ++i)
			{
				if (i == 4) Right->AddSlot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 2.f * U)[WRUI::Text(LOCTEXT("TameCap", "THEN ONE OF TWO"), WRUI::Caps(10), WRUI::Ink3)];
				Right->AddSlot().AutoHeight().Padding(0.f, 2.f * U)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f * U, 0.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(FText::FromString(FString::Printf(TEXT("%s  (%d)"), *Tree->Nodes[i].Name, Tree->Nodes[i].Cost)), WRUI::Body(13, 700), TAttribute<FSlateColor>::CreateLambda([this, i]() { const AWRCreature* T = Tame.Get(); return FSlateColor(T && T->TameRep.HasNode(i) ? WRUI::Good : WRUI::Ink); }))]
						+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(FText::FromString(Tree->Nodes[i].Text), WRUI::Body(12), WRUI::Ink2, false, true)]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SBox).WidthOverride(108.f * U)
						[
							SNew(SWRButton).Ghost(true).MinHeight(34.f * U).FontSize(12)
							.Visibility_Lambda([this]() { return IsMine() ? EVisibility::Visible : EVisibility::Hidden; })
							.Text_Lambda([this, i]() { return FText::FromString(NodeButton(i)); })
							.OnClicked_Lambda([this, i]() { Send(FString::Printf(TEXT("learn %d"), i)); return FReply::Handled(); })
						]
					]
				];
			}

			// --- Tack: saddle and barding.
			if (!Kind.IsNone())
			{
				Right->AddSlot().AutoHeight().Padding(0.f, 14.f * U, 0.f, 4.f * U)[Heading(LOCTEXT("TameTack", "SADDLE AND BARDING"))];
				Right->AddSlot().AutoHeight()[TackRow(false)];
				Right->AddSlot().AutoHeight().Padding(0.f, 4.f * U, 0.f, 0.f)[TackRow(true)];
				Right->AddSlot().AutoHeight().Padding(0.f, 4.f * U, 0.f, 0.f)[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(TackHelp()); }), WRUI::Body(12), WRUI::Ink3, false, true)];
			}

			// --- A hauler's packs.
			if (bHauler)
			{
				Right->AddSlot().AutoHeight().Padding(0.f, 14.f * U, 0.f, 4.f * U)[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { const AWRCreature* T = Tame.Get(); return FText::FromString(T ? FString::Printf(TEXT("ITS PACKS  ·  %.0f OF %.0f KG  ·  CLICK TO TAKE"), T->TameRep.CargoKg, T->CargoCapacityKg()) : FString()); }), WRUI::Caps(11), WRUI::Ink3)];
				Right->AddSlot().AutoHeight()[SNew(SWRBar).Height(6.f * U).Fraction_Lambda([this]() { const AWRCreature* T = Tame.Get(); return T && T->CargoCapacityKg() > 0.f ? FMath::Clamp(T->TameRep.CargoKg / T->CargoCapacityKg(), 0.f, 1.f) : 0.f; }).Color(WRUI::Brass)];
				TSharedRef<SUniformGridPanel> Packs = SNew(SUniformGridPanel).SlotPadding(FMargin(3.f * U));
				const int32 MaxSlots = 40;
				for (int32 i = 0; i < MaxSlots; ++i)
				{
					Packs->AddSlot(i % 10, i / 10)
					[
						SNew(SWRSlot).Size(46.f * U).Kind(EWRSlotKind::Chest).Index(i)
						.Visibility_Lambda([this, i]() { const AWRCreature* T = Tame.Get(); return T && i < T->CargoSlots() ? EVisibility::Visible : EVisibility::Collapsed; })
						.View_Lambda([this, i]() { FWRSlotView V; const AWRCreature* T = Tame.Get(); if (T && T->Cargo.IsValidIndex(i) && !T->Cargo[i].IsEmpty()) { V.ItemId = T->Cargo[i].ItemId; V.Count = T->Cargo[i].Count; } V.bDimmed = !IsMine(); return V; })
						.OnClicked_Lambda([this, i](const FPointerEvent& E) { const AWRCreature* T = Tame.Get(); if (IsMine() && T && T->Cargo.IsValidIndex(i)) Send(FString::Printf(TEXT("take %d %d"), i, E.IsShiftDown() ? 1 : T->Cargo[i].Count)); })
					];
				}
				Right->AddSlot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 0.f)[Packs];
			}
			// Your bag: tack to fit, food, and (haulers) what to pack.
			Right->AddSlot().AutoHeight().Padding(0.f, 14.f * U, 0.f, 4.f * U)[Heading(bHauler ? LOCTEXT("TameBagPack", "YOUR BAG: CLICK TO PACK, OR TO FIT TACK") : LOCTEXT("TameBag", "YOUR BAG: CLICK SADDLES AND BARDING TO FIT THEM"))];
			TSharedRef<SUniformGridPanel> Bag = SNew(SUniformGridPanel).SlotPadding(FMargin(3.f * U));
			for (int32 i = 0; i < UWRInventoryComponent::NumSlots; ++i)
			{
				Bag->AddSlot(i % 10, i / 10)
				[
					SNew(SWRSlot).Size(46.f * U).Kind(EWRSlotKind::Bag).Index(i)
					.View_Lambda([this, i]() { FWRSlotView V; if (const UWRInventoryComponent* Inv = BagOf()) { const TArray<FWRItemStack>& S = Inv->GetSlots(); if (S.IsValidIndex(i) && !S[i].IsEmpty()) { V.ItemId = S[i].ItemId; V.Count = S[i].Count; V.Quality = FMath::Clamp(int32(S[i].Quality) - 1, 0, 2); V.bDimmed = !bHauler && !FWRTaming::Tack(S[i].ItemId); } } return V; })
					.OnClicked_Lambda([this, i](const FPointerEvent& E) { BagClicked(i, E.IsShiftDown()); })
				];
			}
			Right->AddSlot().AutoHeight()[Bag];

			ChildSlot
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[SNew(SBackgroundBlur).BlurStrength(6.f)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.45f))]]
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(1120.f * U).MaxDesiredHeight(930.f * U)
					[
						SNew(SWRPanel).Kind(EWRPanel::Stone).Rivets(true).BrassTrim(true).Padding(FMargin(10.f * U, 10.f * U, 10.f * U, 18.f * U))
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(SBox).HeightOverride(60.f * U)
								[
									SNew(SWRPanel).Kind(EWRPanel::Leather).Padding(FMargin(16.f * U, 0.f, 14.f * U, 0.f))
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
										[
											SNew(SVerticalBox)
											+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { const AWRCreature* T = Tame.Get(); return FText::FromString(T ? T->GetDisplayLabel() : FString(TEXT("Tame"))); }), WRUI::Title(19), WRUI::Ink, true)]
											+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(SubTitle()); }), WRUI::Body(12, 500), WRUI::Ink2, true)]
										]
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 7.f * U, 0.f)[SNew(SWRKeyCap).FontSize(11).Text(LOCTEXT("TameEsc", "Esc"))]
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(LOCTEXT("TameClose", "Close"), WRUI::Body(13, 500), WRUI::Ink2, true)]
									]
								]
							]
							+ SVerticalBox::Slot().FillHeight(1.f).Padding(FMargin(18.f * U, 16.f * U, 18.f * U, 0.f))
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().FillWidth(0.4f).Padding(0.f, 0.f, 22.f * U, 0.f)[SNew(SScrollBox) + SScrollBox::Slot()[Left]]
								+ SHorizontalBox::Slot().FillWidth(0.6f)[SNew(SScrollBox) + SScrollBox::Slot()[Right]]
							]
						]
					]
				]
			];
		}

		virtual void Tick(const FGeometry& Geo, const double Now, const float Dt) override
		{
			SCompoundWidget::Tick(Geo, Now, Dt);
			// It died, was released, or you walked off (or are riding it): close.
			AWRPlayerController* P = PC.Get();
			const AWRCreature* T = Tame.Get();
			const APawn* Me = P ? P->GetPawn() : nullptr;
			if (P && P->GetMenu() == EWRMenu::TameCard && (!T || !T->IsAlive() || !T->IsTamed() || !Me || (Me != T && FVector::Dist(T->GetActorLocation(), Me->GetActorLocation()) > 1500.f)))
			{
				P->SetMenu(EWRMenu::None);
			}
		}

		virtual bool SupportsKeyboardFocus() const override { return true; }

	private:
		TWeakObjectPtr<AWRPlayerController> PC;
		TWeakObjectPtr<AWRCreature> Tame;
		const FWRTameTree* Tree = nullptr;
		FName Kind;
		bool bHauler = false;

		const UWRInventoryComponent* BagOf() const
		{
			const AWRPlayerState* PS = PC.IsValid() ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
			return PS ? PS->GetInventory() : nullptr;
		}

		bool IsMine() const
		{
			const AWRCreature* T = Tame.Get();
			const APlayerState* PS = PC.IsValid() ? PC->GetPlayerState<APlayerState>() : nullptr;
			return T && PS && T->TamedBy == PS->GetPlayerName();
		}

		void Send(const FString& Cmd)
		{
			const AWRCreature* T = Tame.Get();
			if (!PC.IsValid() || !T) return;
			const FVector L = T->GetActorLocation();
			PC->SendCommand(FString::Printf(TEXT("tamecard %.0f %.0f %.0f %s"), L.X, L.Y, L.Z, *Cmd));
			WRSound::PlayUI(PC.Get(), TEXT("ui.click"));
		}

		void BagClicked(int32 i, bool bOne)
		{
			const UWRInventoryComponent* Inv = BagOf();
			if (!Inv || !Inv->GetSlots().IsValidIndex(i) || Inv->GetSlots()[i].IsEmpty() || !IsMine()) return;
			const FWRItemStack& S = Inv->GetSlots()[i];
			if (const FWRTackDef* Tk = FWRTaming::Tack(S.ItemId))
			{
				Send(FString::Printf(TEXT("%s %d"), Tk->Kind == TEXT("barding") ? TEXT("barding") : TEXT("saddle"), i));
				return;
			}
			if (bHauler) Send(FString::Printf(TEXT("put %d %d"), i, bOne ? 1 : S.Count));
		}

		FString FoodName() const
		{
			const AWRCreature* T = Tame.Get();
			const FWRCreatureDef* D = T ? T->GetDef() : nullptr;
			return D && D->Food.Num() > 0 ? ItemName(D->Food[0]).ToLower() : FString(TEXT("its food"));
		}

		FString SubTitle() const
		{
			const AWRCreature* T = Tame.Get();
			const FWRCreatureDef* D = T ? T->GetDef() : nullptr;
			if (!D) return FString();
			const FString Mount = Kind == TEXT("fly") ? TEXT("flying mount") : Kind == TEXT("swim") ? TEXT("swimming mount") : Kind == TEXT("pack") ? TEXT("pack mount") : Kind.IsNone() ? D->Role : TEXT("mount");
			return FString::Printf(TEXT("%s  ·  %s  ·  %s element  ·  tamed by %s"), T->TameRep.Sex == 2 ? TEXT("Female") : TEXT("Male"), *Mount, D->Element.IsNone() ? TEXT("no") : *D->Element.ToString(), *T->TamedBy);
		}

		FString Headline() const
		{
			const AWRCreature* T = Tame.Get();
			if (!T) return FString();
			return FString::Printf(TEXT("Level %d  ·  wild %d + bond %d%s"), T->TameRep.WildLevel + T->TameRep.Bond, T->TameRep.WildLevel, T->TameRep.Bond,
				T->IsBaby() ? *FString::Printf(TEXT("  ·  a baby, %.0f%% grown"), T->TameRep.Growth * 100.f) : TEXT(""));
		}

		float BondCost(int32 Next) const { return WRTune(TEXT("tame.bondBase"), 100.f) + WRTune(TEXT("tame.bondPer"), 40.f) * Next; }

		float BondFraction() const
		{
			const AWRCreature* T = Tame.Get();
			if (!T) return 0.f;
			if (T->TameRep.Bond >= int32(WRTune(TEXT("tame.bondMax"), 20.f))) return 1.f;
			return FMath::Clamp(T->TameRep.BondXP / BondCost(T->TameRep.Bond + 1), 0.f, 1.f);
		}

		FString BondLine() const
		{
			const AWRCreature* T = Tame.Get();
			if (!T) return FString();
			if (T->TameRep.Bond >= int32(WRTune(TEXT("tame.bondMax"), 20.f))) return TEXT("Bond 20: as close as it gets");
			return FString::Printf(TEXT("Bond %d  ·  %.0f / %.0f to the next (it learns by its own work)"), T->TameRep.Bond, T->TameRep.BondXP, BondCost(T->TameRep.Bond + 1));
		}

		FString StatLine(int32 s) const
		{
			const AWRCreature* T = Tame.Get();
			if (!T || !T->TameRep.Points.IsValidIndex(s)) return FString();
			const int32 Mut = T->TameRep.MutPoints.IsValidIndex(s) ? T->TameRep.MutPoints[s] : 0;
			return FString::Printf(TEXT("%d point%s  ·  x%.2f%s"), T->TameRep.Points[s], T->TameRep.Points[s] == 1 ? TEXT("") : TEXT("s"), T->StatMult(s),
				Mut > 0 ? *FString::Printf(TEXT("  ·  +%d mutated"), Mut) : TEXT(""));
		}

		FLinearColor StatColour(int32 s) const
		{
			const AWRCreature* T = Tame.Get();
			return T && T->TameRep.MutPoints.IsValidIndex(s) && T->TameRep.MutPoints[s] > 0 ? WRUI::Brass : WRUI::Ink2;
		}

		FString MorphLine() const
		{
			const AWRCreature* T = Tame.Get();
			if (!T) return FString();
			FString Line;
			for (const uint8 K : T->TameRep.MorphKinds) Line += (Line.IsEmpty() ? TEXT("") : TEXT(", ")) + FString(WRTameMorph::Name(K));
			const FString Lines = FString::Printf(TEXT("Mutations: %d on its father's line, %d on its mother's (20 each at most)"), T->TameRep.MutFather, T->TameRep.MutMother);
			return Line.IsEmpty() ? Lines : FString::Printf(TEXT("Marked: %s.  %s"), *Line, *Lines);
		}

		FString FamilyLine() const
		{
			const AWRCreature* T = Tame.Get();
			if (!T) return FString();
			const float Mat = FWRTaming::Maturation();
			if (T->IsBaby())
			{
				return FString::Printf(TEXT("Growing: %.0f%%  ·  imprint %.0f%%%s  ·  it asks for care every eighth of its growing; each answered is +12.5%%, and its imprinter's ride or fight beside it gets up to +20%%"),
					T->TameRep.Growth * 100.f, T->TameRep.Imprint * 100.f, T->TameRep.Imprinter.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" (%s)"), *T->TameRep.Imprinter));
			}
			if (T->TameRep.Pregnant > 0.f) return FString::Printf(TEXT("Carrying young: born in %s"), *Clock(T->TameRep.Pregnant / Mat));
			if (T->TameRep.MateBar > 0.f) return FString::Printf(TEXT("Mating: %.0f%%"), T->TameRep.MateBar * 100.f);
			if (T->TameRep.Cooldown > 0.f) return FString::Printf(TEXT("Resting after mating: %s"), *Clock(T->TameRep.Cooldown / Mat));
			const FString Imp = T->TameRep.Imprint > 0.f ? FString::Printf(TEXT("Imprinted %.0f%% on %s.  "), T->TameRep.Imprint * 100.f, *T->TameRep.Imprinter) : FString();
			return Imp + (T->TameRep.bMating ? TEXT("Looking for a mate of its kind within 10 m (both fed)") : TEXT("Not mating"));
		}

		float FamilyFraction() const
		{
			const AWRCreature* T = Tame.Get();
			if (!T) return 0.f;
			if (T->IsBaby()) return T->TameRep.Growth;
			if (T->TameRep.Pregnant > 0.f && T->TameRep.PregnantTotal > 0.f) return 1.f - T->TameRep.Pregnant / T->TameRep.PregnantTotal;
			return T->TameRep.MateBar;
		}

		FString CareButton() const
		{
			const AWRCreature* T = Tame.Get();
			if (!T) return FString();
			switch (T->TameRep.Care)
			{
			case 1: return FString::Printf(TEXT("It's hungry for %s: feed it"), *ItemName(T->TameRep.CareFood).ToLower());
			case 2: return FString::Printf(TEXT("It wants a walk: %.0f of %.0f m beside you"), T->TameRep.CareWalked, WRTune(TEXT("tame.imprint.walkMetres"), 50.f));
			case 3: return TEXT("It wants a cuddle: stand beside it and cuddle (2 s)");
			default: return FString();
			}
		}

		int32 FreePoints() const
		{
			const AWRCreature* T = Tame.Get();
			if (!T || !Tree) return 0;
			int32 Spent = 0;
			for (int32 k = 0; k < Tree->Nodes.Num(); ++k) if (T->TameRep.HasNode(k)) Spent += Tree->Nodes[k].Cost;
			return FMath::Max(0, T->TameRep.Bond - Spent);
		}

		FString NodeButton(int32 i) const
		{
			const AWRCreature* T = Tame.Get();
			if (!T || !Tree) return FString();
			if (T->TameRep.HasNode(i)) return TEXT("Learned");
			if (i >= 4 && T->TameRep.HasNode(i == 4 ? 5 : 4)) return TEXT("Other chosen");
			if (i >= 1 && i <= 3 && !T->TameRep.HasNode(i - 1)) return TEXT("Needs the one above");
			if (i >= 4) for (int32 k = 0; k < 4; ++k) if (!T->TameRep.HasNode(k)) return TEXT("Needs all four");
			return FreePoints() >= Tree->Nodes[i].Cost ? TEXT("Learn") : FString::Printf(TEXT("Needs %d pts"), Tree->Nodes[i].Cost);
		}

		TSharedRef<SWidget> TackRow(bool bBarding)
		{
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f * U, 0.f)
				[
					SNew(SWRSlot).Size(52.f * U).Kind(EWRSlotKind::Other)
					.View_Lambda([this, bBarding]() { FWRSlotView V; const AWRCreature* T = Tame.Get(); if (T) { const FWRItemStack& S = bBarding ? T->TameRep.Barding : T->TameRep.Saddle; if (!S.IsEmpty()) { V.ItemId = S.ItemId; V.Quality = FMath::Clamp(int32(S.Quality) - 1, 0, 2); } } return V; })
					.OnClicked_Lambda([this, bBarding](const FPointerEvent&) { if (IsMine()) Send(bBarding ? TEXT("unbarding") : TEXT("unsaddle")); })
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					WRUI::Text(TAttribute<FText>::CreateLambda([this, bBarding]() { return FText::FromString(TackLine(bBarding)); }), WRUI::Body(13), WRUI::Ink, false, true)
				];
		}

		FString TackLine(bool bBarding) const
		{
			const AWRCreature* T = Tame.Get();
			if (!T) return FString();
			const FWRItemStack& S = bBarding ? T->TameRep.Barding : T->TameRep.Saddle;
			if (S.IsEmpty())
			{
				if (bBarding) return TEXT("No barding (armorer: iron, steel, your land's set)");
				return Kind == TEXT("pack") ? TEXT("No pack saddle: fewer packs") : TEXT("Bareback: you can't fight from its back and it takes 25% more damage");
			}
			const FWRTackDef* Tk = FWRTaming::Tack(S.ItemId);
			return FString::Printf(TEXT("%s: %.0f%% less damage%s  ·  click to take it off"), *ItemName(S.ItemId), FWRTaming::TackValue(S, PC.IsValid() ? PC->GetPlayerState<APlayerState>() : nullptr) * 100.f,
				Tk && Tk->Slots > 0 ? *FString::Printf(TEXT(", +%d packs"), Tk->Slots) : Tk && Tk->Speed < 0.f ? *FString::Printf(TEXT(", %.0f%% slower"), -Tk->Speed * 100.f) : TEXT(""));
		}

		FString TackHelp() const
		{
			const AWRCreature* T = Tame.Get();
			if (T && T->IsBaby()) return TEXT("Too young for tack: let it grow up.");
			if (Kind == TEXT("fly")) return TEXT("Riding: Jump takes off; hold Jump to climb, Dodge to dive. It glides down when its stamina runs out.");
			if (Kind == TEXT("swim")) return TEXT("Riding: it carries you through cold water, you stay dry; slow on land.");
			if (Kind == TEXT("pack")) return TEXT("Riding: a hauler walks; it slows when loaded past three quarters (Steady: never).");
			return TEXT("Riding: Sprint gallops on its stamina.");
		}
	};
}

WR_REGISTER_SCREEN("tamecard", [](AWRPlayerController* PC) -> TSharedRef<SWidget> { return SNew(WRUITameCardScreen::STameCard).PC(PC); });

#undef LOCTEXT_NAMESPACE
