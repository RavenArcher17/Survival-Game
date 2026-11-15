// A tame's card (EWRMenu::TameCard; the tame is the controller's MenuCreature; cooks/build-taming.md 2.8, raised in
// cooks/cookpass-taming.md 3.4-3.5), opened from the tame wheel's Tend. The stone-and-leather window: its name (Rename:
// type one, or on a pad pick a suggested one) over its kind, sex, element and owner; then five tabs (click them, or
// LB / RB on a pad): Overview (level, bond bar, hunger and Feed, the six stats as real numbers with their points, the
// multiplier and the mutated share, its morphs, Soulbind), Skills (its role tree: four in a line and one of two
// capstones), Gear (saddle and barding: fit from your bag, take off), Family (its parents, mating on / off, her rest or
// the young she carries, a baby's growth and imprint, the care it asks for with an Answer button, both lines' mutation
// counts) and Packs (a hauler's packs beside your bag). Every button sends the server's tamecard commands; everything
// shown reads the creature's replicated state. It closes itself if the tame goes or you walk off.

#include "WRPlayerUI.h"
#include "WRUIStyle.h"
#include "WRUIWidgets.h"
#include "WRCreature.h"
#include "WRTaming.h"
#include "WRInventory.h"
#include "WRLifeSkills.h"
#include "WRPadInput.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRSound.h"
#include "WRTuning.h"
#include "WRAttributeSet.h"
#include "Wildreach.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
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

	/** Names to suggest for this kind of tame (Data/tame_names.json by role, then "any"). */
	TArray<FString> SuggestedNames(const FWRCreatureDef* Def)
	{
		static TSharedPtr<FJsonObject> Names = FWRTuning::LoadDataFile(TEXT("tame_names.json"));
		TArray<FString> Out;
		if (!Names.IsValid()) return Out;
		FString Role = Def ? Def->Role.ToLower() : FString();
		Role.Split(TEXT(","), &Role, nullptr);
		Role.Split(TEXT(" "), &Role, nullptr);
		for (const FString& Key : { Role.TrimStartAndEnd(), FString(TEXT("any")) })
		{
			const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
			if (Names->TryGetArrayField(Key, A)) for (const TSharedPtr<FJsonValue>& V : *A) Out.AddUnique(V->AsString());
		}
		return Out;
	}

	/** Rename: a name box over the card that takes the keyboard (UI only, as the chest's Rename does), so typing doesn't
	 *  walk you away. Enter or Save names it ("tamecard ... name <text>"); Suggest fills in the next suggested name (the
	 *  pad's way: Suggest, then Save, no typing); Esc or Cancel closes it. */
	class SWRTameNameBox : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SWRTameNameBox) {}
			SLATE_ARGUMENT(TWeakObjectPtr<AWRPlayerController>, PC)
			SLATE_ARGUMENT(TWeakObjectPtr<AWRCreature>, Tame)
		SLATE_END_ARGS()

		static TWeakPtr<SWRTameNameBox> Instance;

		void Construct(const FArguments& InArgs)
		{
			PC = InArgs._PC;
			Tame = InArgs._Tame;
			const AWRCreature* C = Tame.Get();
			Suggestions = SuggestedNames(C ? C->GetDef() : nullptr);
			if (Suggestions.Num() > 0) Next = FMath::RandHelper(Suggestions.Num());
			static const FEditableTextBoxStyle Field = []()
			{
				FEditableTextBoxStyle S = FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");
				const FSlateBrush* Normal = WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.55f), 5.f, WRUI::Brass, 1.f);
				S.SetBackgroundImageNormal(*Normal).SetBackgroundImageHovered(*Normal).SetBackgroundImageFocused(*Normal).SetBackgroundImageReadOnly(*Normal);
				S.SetForegroundColor(FSlateColor(WRUI::Ink)).SetFocusedForegroundColor(FSlateColor(WRUI::Ink)).SetBackgroundColor(FSlateColor(FLinearColor::White));
				S.SetPadding(FMargin(12.f * U, 8.f * U));
				return S;
			}();
			ChildSlot.HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0.f, 140.f * U, 0.f, 0.f)
			[
				SNew(SBox).WidthOverride(520.f * U)
				[
					SNew(SWRPanel).Kind(EWRPanel::Stone).Rivets(true).BrassTrim(true).Padding(FMargin(18.f * U, 14.f * U))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f * U)[WRUI::Text(LOCTEXT("NameTame", "Name your tame"), WRUI::Title(15), WRUI::Ink, true)]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SAssignNew(Box, SEditableTextBox).Style(&Field).Font(WRUI::Body(16, 700)).Text(FText::FromString(C ? C->TameRep.Name : FString()))
							.HintText(LOCTEXT("NameTameHint", "A name (letters, spaces, ' and -)"))
							.SelectAllTextWhenFocused(true)
							.OnTextCommitted_Lambda([this](const FText&, ETextCommit::Type How) { if (How == ETextCommit::OnEnter) Save(); else if (How == ETextCommit::OnCleared) Close(); })
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f * U, 0.f, 0.f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
							[
								SNew(SWRButton).Ghost(true).FontSize(13).MinHeight(32.f * U).Text(LOCTEXT("NameTameSuggest", "Suggest a name")).OnClicked_Lambda([this]() { Suggest(); return FReply::Handled(); })
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(8.f * U, 0.f, 0.f, 0.f)
							[
								SNew(SWRButton).Ghost(true).FontSize(13).MinHeight(32.f * U).Text(LOCTEXT("NameTameCancel", "Cancel")).OnClicked_Lambda([this]() { Close(); return FReply::Handled(); })
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(8.f * U, 0.f, 0.f, 0.f)
							[
								SNew(SWRButton).Primary(true).FontSize(13).MinHeight(32.f * U).Text(LOCTEXT("NameTameSave", "Save")).OnClicked_Lambda([this]() { Save(); return FReply::Handled(); })
							]
						]
					]
				]
			];
			// The pad: Y suggests the next, B cancels (A clicks the buttons under its cursor).
			WRInput::PushPadHandler(this, [this](FName Verb)
			{
				if (Verb == TEXT("MenuBack") || Verb == TEXT("MenuClose")) { Close(); return true; }
				if (Verb == TEXT("MenuControls")) { Suggest(); return true; }
				return Verb == TEXT("TabPrev") || Verb == TEXT("TabNext");
			});
		}

		virtual ~SWRTameNameBox() override { WRInput::PopPadHandler(this); }

		static void Open(AWRPlayerController* InPC, AWRCreature* InTame)
		{
			if (!InPC || !InTame || !GEngine || !GEngine->GameViewport || Instance.IsValid()) return;
			TSharedRef<SWRTameNameBox> Widget = SNew(SWRTameNameBox).PC(InPC).Tame(InTame);
			Instance = Widget;
			GEngine->GameViewport->AddViewportWidgetContent(Widget, 90);
			FInputModeUIOnly Mode;
			Mode.SetWidgetToFocus(Widget->Box);
			InPC->SetInputMode(Mode);
		}

		static void CloseIfOpen()
		{
			if (const TSharedPtr<SWRTameNameBox> Open = Instance.Pin()) Open->Close();
		}

		virtual void Tick(const FGeometry& Geo, const double Time, const float Delta) override
		{
			SCompoundWidget::Tick(Geo, Time, Delta);
			// Focus a frame after it opens (the Rename button's own click takes it back after its handler).
			if (Frames++ < 2 && Box.IsValid() && !WRInput::UsingPad()) FSlateApplication::Get().SetKeyboardFocus(Box, EFocusCause::SetDirectly);
			if (!Tame.IsValid() || !PC.IsValid() || PC->GetMenu() != EWRMenu::TameCard) Close();
		}

	private:
		TWeakObjectPtr<AWRPlayerController> PC;
		TWeakObjectPtr<AWRCreature> Tame;
		TSharedPtr<SEditableTextBox> Box;
		TArray<FString> Suggestions;
		int32 Next = 0;
		int32 Frames = 0;
		bool bClosed = false;

		void Suggest()
		{
			if (Suggestions.Num() == 0 || !Box.IsValid()) return;
			Box->SetText(FText::FromString(Suggestions[Next % Suggestions.Num()]));
			Next = (Next + 1) % Suggestions.Num();
			WRSound::PlayUI(PC.Get(), TEXT("ui.click"));
		}

		void Save()
		{
			const AWRCreature* C = Tame.Get();
			if (PC.IsValid() && C && Box.IsValid())
			{
				const FVector L = C->GetActorLocation();
				PC->SendCommand(FString::Printf(TEXT("tamecard %.0f %.0f %.0f name %s"), L.X, L.Y, L.Z, *Box->GetText().ToString().Replace(TEXT("|"), TEXT("")).Left(40)));
			}
			Close();
		}

		void Close()
		{
			if (bClosed) return;
			bClosed = true;
			const TSharedRef<SWRTameNameBox> KeepAlive = SharedThis(this);
			if (GEngine && GEngine->GameViewport) GEngine->GameViewport->RemoveViewportWidgetContent(KeepAlive);
			if (PC.IsValid()) { if (FWRPlayerUI* UI = PC->GetUI()) UI->ReapplyInputMode(); }
			Instance.Reset();
		}
	};
	TWeakPtr<SWRTameNameBox> SWRTameNameBox::Instance;

	enum class ETab : uint8 { Overview, Skills, Gear, Family, Packs };

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
			Tabs.Add({ ETab::Overview, LOCTEXT("TameTabOverview", "Overview") });
			Tabs.Add({ ETab::Skills, LOCTEXT("TameTabSkills", "Skills") });
			if (!Kind.IsNone()) Tabs.Add({ ETab::Gear, LOCTEXT("TameTabGear", "Gear") });
			Tabs.Add({ ETab::Family, LOCTEXT("TameTabFamily", "Family") });
			if (bHauler) Tabs.Add({ ETab::Packs, LOCTEXT("TameTabPacks", "Packs") });

			const auto Heading = [](const FText& T) { return WRUI::Text(T, WRUI::Caps(11), WRUI::Ink3); };
			const auto Pane = [this](ETab Which, const TSharedRef<SWidget>& Body)
			{
				return SNew(SBox).Visibility_Lambda([this, Which]() { return CurrentTab() == Which ? EVisibility::Visible : EVisibility::Collapsed; })[SNew(SScrollBox) + SScrollBox::Slot()[Body]];
			};

			// --- Overview: level, bond, hunger, the six stats, morphs, Soulbind.
			TSharedRef<SVerticalBox> Overview = SNew(SVerticalBox);
			TSharedRef<SVerticalBox> OvLeft = SNew(SVerticalBox), OvRight = SNew(SVerticalBox);
			OvLeft->AddSlot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(Headline()); }), WRUI::Body(14, 600), WRUI::Ink, false, true)];
			OvLeft->AddSlot().AutoHeight().Padding(0.f, 10.f * U, 0.f, 3.f * U)[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(BondLine()); }), WRUI::Body(12), WRUI::Ink2)];
			OvLeft->AddSlot().AutoHeight()[SNew(SWRBar).Height(10.f * U).Color(WRUI::Brass).Fraction_Lambda([this]() { return BondFraction(); })];
			OvLeft->AddSlot().AutoHeight().Padding(0.f, 10.f * U, 0.f, 3.f * U)[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { const AWRCreature* T = Tame.Get(); return FText::FromString(T ? FString::Printf(TEXT("Hunger %.0f%%  ·  eats %s or kibble"), T->TameRep.Hunger, *FoodName()) : FString()); }), WRUI::Body(12), WRUI::Ink2)];
			OvLeft->AddSlot().AutoHeight()[SNew(SWRBar).Height(8.f * U).Fraction_Lambda([this]() { const AWRCreature* T = Tame.Get(); return T ? T->TameRep.Hunger / 100.f : 0.f; }).Color_Lambda([this]() { const AWRCreature* T = Tame.Get(); return T && T->TameRep.Hunger < 25.f ? WRUI::Bad : WRUI::Good; })];
			OvLeft->AddSlot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 0.f).HAlign(HAlign_Left)
			[
				SNew(SWRButton).Ghost(true).MinHeight(36.f * U).FontSize(12).Text(LOCTEXT("TameFeed", "Feed it from your bag"))
				.OnClicked_Lambda([this]() { Send(TEXT("feed")); return FReply::Handled(); })
			];
			OvLeft->AddSlot().AutoHeight().Padding(0.f, 14.f * U, 0.f, 4.f * U)[Heading(LOCTEXT("TameSoul", "SOULBOND"))];
			OvLeft->AddSlot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(SoulLine()); }), WRUI::Body(12), WRUI::Ink2, false, true)];
			OvLeft->AddSlot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 0.f).HAlign(HAlign_Left)
			[
				SNew(SWRButton).Ghost(true).MinHeight(36.f * U).FontSize(12)
				.Visibility_Lambda([this]() { return IsMine() && HasSoulbond() ? EVisibility::Visible : EVisibility::Collapsed; })
				.Text_Lambda([this]() { const AWRCreature* T = Tame.Get(); return T && T->TameRep.bSoulbound ? LOCTEXT("TameUnbind", "Release its soulbond") : LOCTEXT("TameBind", "Soulbind it"); })
				.OnClicked_Lambda([this]() { Send(TEXT("soulbind")); return FReply::Handled(); })
			];
			OvRight->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f * U)[Heading(LOCTEXT("TameStats", "ITS STATS  ·  NOW  ·  POINTS  ·  MULTIPLIER"))];
			for (int32 s = 0; s < WRTameStat::Count; ++s)
			{
				OvRight->AddSlot().AutoHeight().Padding(0.f, 3.f * U)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(0.26f)[WRUI::Text(FText::FromString(WRTameStat::Label(s)), WRUI::Body(13, 600), WRUI::Ink)]
					+ SHorizontalBox::Slot().FillWidth(0.3f)[WRUI::Text(TAttribute<FText>::CreateLambda([this, s]() { return FText::FromString(StatValue(s)); }), WRUI::Body(13, 700), WRUI::Ink)]
					+ SHorizontalBox::Slot().FillWidth(0.44f)[WRUI::Text(TAttribute<FText>::CreateLambda([this, s]() { return FText::FromString(StatLine(s)); }), WRUI::Body(12), TAttribute<FSlateColor>::CreateLambda([this, s]() { return FSlateColor(StatColour(s)); }))]
				];
			}
			OvRight->AddSlot().AutoHeight().Padding(0.f, 8.f * U, 0.f, 0.f)[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(MorphLine()); }), WRUI::Body(12), WRUI::Ink2, false, true)];
			Overview->AddSlot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(0.42f).Padding(0.f, 0.f, 24.f * U, 0.f)[OvLeft]
				+ SHorizontalBox::Slot().FillWidth(0.58f)[OvRight]
			];

			// --- Skills: the role tree.
			TSharedRef<SVerticalBox> Skills = SNew(SVerticalBox);
			Skills->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f * U)[Heading(FText::FromString(Tree ? FString::Printf(TEXT("%s SKILLS  ·  ONE POINT A BOND LEVEL"), *Tree->Name.ToUpper()) : FString(TEXT("NO SKILL TREE"))))];
			Skills->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f * U)[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(FString::Printf(TEXT("%d skill point%s free"), FreePoints(), FreePoints() == 1 ? TEXT("") : TEXT("s"))); }), WRUI::Body(13, 600), WRUI::Brass)];
			for (int32 i = 0; Tree && i < Tree->Nodes.Num(); ++i)
			{
				if (i == 4) Skills->AddSlot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 2.f * U)[WRUI::Text(LOCTEXT("TameCap", "THEN ONE OF TWO"), WRUI::Caps(10), WRUI::Ink3)];
				Skills->AddSlot().AutoHeight().Padding(0.f, 3.f * U)
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
						SNew(SBox).WidthOverride(130.f * U)
						[
							SNew(SWRButton).Ghost(true).MinHeight(34.f * U).FontSize(12)
							.Visibility_Lambda([this]() { return IsMine() ? EVisibility::Visible : EVisibility::Hidden; })
							.Text_Lambda([this, i]() { return FText::FromString(NodeButton(i)); })
							.OnClicked_Lambda([this, i]() { Send(FString::Printf(TEXT("learn %d"), i)); return FReply::Handled(); })
						]
					]
				];
			}

			// --- Gear: saddle and barding, your bag to fit from.
			TSharedRef<SVerticalBox> Gear = SNew(SVerticalBox);
			if (!Kind.IsNone())
			{
				Gear->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f * U)[Heading(LOCTEXT("TameTack", "SADDLE AND BARDING"))];
				Gear->AddSlot().AutoHeight()[TackRow(false)];
				Gear->AddSlot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 0.f)[TackRow(true)];
				Gear->AddSlot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 0.f)[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(TackHelp()); }), WRUI::Body(12), WRUI::Ink3, false, true)];
				Gear->AddSlot().AutoHeight().Padding(0.f, 14.f * U, 0.f, 4.f * U)[Heading(LOCTEXT("TameBag", "YOUR BAG: CLICK SADDLES AND BARDING TO FIT THEM"))];
				Gear->AddSlot().AutoHeight()[BagGrid()];
			}

			// --- Family: parents, mating, pregnancy, growth, imprint, care.
			TSharedRef<SVerticalBox> Family = SNew(SVerticalBox);
			Family->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f * U)[Heading(LOCTEXT("TameParents", "ITS PARENTS"))];
			Family->AddSlot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(ParentsLine()); }), WRUI::Body(13), WRUI::Ink2, false, true)];
			Family->AddSlot().AutoHeight().Padding(0.f, 14.f * U, 0.f, 4.f * U)[Heading(LOCTEXT("TameFamily", "BREEDING AND YOUNG"))];
			Family->AddSlot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(FamilyLine()); }), WRUI::Body(13), WRUI::Ink2, false, true)];
			Family->AddSlot().AutoHeight().Padding(0.f, 4.f * U, 0.f, 0.f)
			[
				SNew(SBox).Visibility_Lambda([this]() { const AWRCreature* T = Tame.Get(); return T && (T->IsBaby() || T->TameRep.Pregnant > 0.f || T->TameRep.MateBar > 0.f) ? EVisibility::Visible : EVisibility::Collapsed; })
				[
					SNew(SWRBar).Height(8.f * U).Color(WRUI::Brass).Fraction_Lambda([this]() { return FamilyFraction(); })
				]
			];
			Family->AddSlot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 0.f).HAlign(HAlign_Left)
			[
				SNew(SWRButton).Ghost(true).MinHeight(36.f * U).FontSize(12)
				.Visibility_Lambda([this]() { const AWRCreature* T = Tame.Get(); return T && !T->IsBaby() && IsMine() ? EVisibility::Visible : EVisibility::Collapsed; })
				.Text_Lambda([this]() { const AWRCreature* T = Tame.Get(); return T && T->TameRep.bMating ? LOCTEXT("TameMateOff", "Mating: on (turn off)") : LOCTEXT("TameMateOn", "Mating: off (turn on)"); })
				.OnClicked_Lambda([this]() { const AWRCreature* T = Tame.Get(); Send(T && T->TameRep.bMating ? TEXT("mate off") : TEXT("mate on")); return FReply::Handled(); })
			];
			Family->AddSlot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 0.f).HAlign(HAlign_Left)
			[
				SNew(SWRButton).Primary(true).MinHeight(42.f * U).FontSize(13)
				.Visibility_Lambda([this]() { const AWRCreature* T = Tame.Get(); return T && T->TameRep.Care >= 1 && T->TameRep.Care <= 3 ? EVisibility::Visible : EVisibility::Collapsed; })
				.Text_Lambda([this]() { return FText::FromString(CareButton()); })
				.OnClicked_Lambda([this]() { Send(TEXT("care")); return FReply::Handled(); })
			];
			Family->AddSlot().AutoHeight().Padding(0.f, 10.f * U, 0.f, 0.f)[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(LinesLine()); }), WRUI::Body(12), WRUI::Ink3, false, true)];

			// --- Packs: a hauler's packs and your bag.
			TSharedRef<SVerticalBox> Packs = SNew(SVerticalBox);
			if (bHauler)
			{
				Packs->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f * U)[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { const AWRCreature* T = Tame.Get(); return FText::FromString(T ? FString::Printf(TEXT("ITS PACKS  ·  %.0f OF %.0f KG  ·  CLICK TO TAKE (SHIFT: ONE)"), T->TameRep.CargoKg, T->CargoCapacityKg()) : FString()); }), WRUI::Caps(11), WRUI::Ink3)];
				Packs->AddSlot().AutoHeight()[SNew(SWRBar).Height(6.f * U).Fraction_Lambda([this]() { const AWRCreature* T = Tame.Get(); return T && T->CargoCapacityKg() > 0.f ? FMath::Clamp(T->TameRep.CargoKg / T->CargoCapacityKg(), 0.f, 1.f) : 0.f; }).Color(WRUI::Brass)];
				TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel).SlotPadding(FMargin(3.f * U));
				for (int32 i = 0; i < 40; ++i)
				{
					Grid->AddSlot(i % 10, i / 10)
					[
						SNew(SWRSlot).Size(46.f * U).Kind(EWRSlotKind::Chest).Index(i)
						.Visibility_Lambda([this, i]() { const AWRCreature* T = Tame.Get(); return T && i < T->CargoSlots() ? EVisibility::Visible : EVisibility::Collapsed; })
						.View_Lambda([this, i]() { FWRSlotView V; const AWRCreature* T = Tame.Get(); if (T && T->Cargo.IsValidIndex(i) && !T->Cargo[i].IsEmpty()) { V.ItemId = T->Cargo[i].ItemId; V.Count = T->Cargo[i].Count; } V.bDimmed = !IsMine(); return V; })
						.OnClicked_Lambda([this, i](const FPointerEvent& E) { const AWRCreature* T = Tame.Get(); if (IsMine() && T && T->Cargo.IsValidIndex(i)) Send(FString::Printf(TEXT("take %d %d"), i, E.IsShiftDown() ? 1 : T->Cargo[i].Count)); })
					];
				}
				Packs->AddSlot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 0.f)[Grid];
				Packs->AddSlot().AutoHeight().Padding(0.f, 14.f * U, 0.f, 4.f * U)[Heading(LOCTEXT("TameBagPack", "YOUR BAG: CLICK TO PACK (SHIFT: ONE)"))];
				Packs->AddSlot().AutoHeight()[BagGrid()];
			}

			// The tab strip: click a tab, or LB / RB on a pad (their buttons at its ends).
			TSharedRef<SHorizontalBox> Strip = SNew(SHorizontalBox);
			const auto PadCap = [](const TCHAR* Verb)
			{
				const FName V(Verb);
				return SNew(SWRKeyCap).FontSize(11).Visibility_Lambda([]() { return WRInput::UsingPad() ? EVisibility::Visible : EVisibility::Collapsed; }).Text_Lambda([V]() { return WRInput::PadButton(V); });
			};
			Strip->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f * U, 0.f)[PadCap(TEXT("TabPrev"))];
			for (int32 t = 0; t < Tabs.Num(); ++t)
			{
				Strip->AddSlot().AutoWidth().Padding(0.f, 0.f, 6.f * U, 0.f)
				[
					SNew(SButton).ButtonStyle(FCoreStyle::Get(), "NoBorder").Cursor(EMouseCursor::Hand)
					.OnClicked_Lambda([this, t]() { if (TabIndex != t) { TabIndex = t; WRSound::PlayUI(PC.Get(), TEXT("ui.click")); } return FReply::Handled(); })
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(14.f * U, 8.f * U, 14.f * U, 6.f * U)
						[
							WRUI::Text(Tabs[t].Value, WRUI::Title(15), TAttribute<FSlateColor>::CreateLambda([this, t]() { return FSlateColor(TabIndex == t ? WRUI::Ink : WRUI::Ink3); }), true)
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SBox).HeightOverride(2.f)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity_Lambda([this, t]() { return TabIndex == t ? WRUI::Brass : FLinearColor::Transparent; })]
						]
					]
				];
			}
			Strip->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(4.f * U, 0.f, 0.f, 0.f)[PadCap(TEXT("TabNext"))];

			ChildSlot
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[SNew(SBackgroundBlur).BlurStrength(6.f)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.45f))]]
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(1120.f * U).HeightOverride(840.f * U)
					[
						SNew(SWRPanel).Kind(EWRPanel::Stone).Rivets(true).BrassTrim(true).Padding(FMargin(10.f * U, 10.f * U, 10.f * U, 18.f * U))
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(SBox).HeightOverride(64.f * U)
								[
									SNew(SWRPanel).Kind(EWRPanel::Leather).Padding(FMargin(16.f * U, 0.f, 14.f * U, 0.f))
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
										[
											SNew(SVerticalBox)
											+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(TitleLine()); }), WRUI::Title(19), WRUI::Ink, true)]
											+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(SubTitle()); }), WRUI::Body(12, 500), WRUI::Ink2, true)]
										]
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 14.f * U, 0.f)
										[
											SNew(SWRButton).Ghost(true).MinHeight(34.f * U).FontSize(12)
											.Visibility_Lambda([this]() { return IsMine() ? EVisibility::Visible : EVisibility::Collapsed; })
											.Text_Lambda([this]() { const AWRCreature* T = Tame.Get(); return T && !T->TameRep.Name.IsEmpty() ? LOCTEXT("TameRename", "Rename") : LOCTEXT("TameName", "Name it"); })
											.OnClicked_Lambda([this]() { SWRTameNameBox::Open(PC.Get(), Tame.Get()); return FReply::Handled(); })
										]
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 7.f * U, 0.f)[SNew(SWRKeyCap).FontSize(11).Text(LOCTEXT("TameEsc", "Esc"))]
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(LOCTEXT("TameClose", "Close"), WRUI::Body(13, 500), WRUI::Ink2, true)]
									]
								]
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(14.f * U, 8.f * U, 14.f * U, 0.f))[Strip]
							+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(14.f * U, 0.f, 14.f * U, 0.f))[WRUI::Divider()]
							+ SVerticalBox::Slot().FillHeight(1.f).Padding(FMargin(18.f * U, 14.f * U, 18.f * U, 0.f))
							[
								SNew(SOverlay)
								+ SOverlay::Slot()[Pane(ETab::Overview, Overview)]
								+ SOverlay::Slot()[Pane(ETab::Skills, Skills)]
								+ SOverlay::Slot()[Pane(ETab::Gear, Gear)]
								+ SOverlay::Slot()[Pane(ETab::Family, Family)]
								+ SOverlay::Slot()[Pane(ETab::Packs, Packs)]
							]
						]
					]
				]
			];

			// The pad: LB / RB walk the tabs (A clicks under the cursor, B closes as everywhere).
			WRInput::PushPadHandler(this, [this](FName Verb)
			{
				if (Verb != TEXT("TabPrev") && Verb != TEXT("TabNext")) return false;
				const int32 N = FMath::Max(1, Tabs.Num());
				TabIndex = ((TabIndex + (Verb == TEXT("TabNext") ? 1 : -1)) % N + N) % N;
				WRSound::PlayUI(PC.Get(), TEXT("ui.click"));
				return true;
			});
			// A baby asking for care opens on Family (that's what you came for).
			if (C && C->TameRep.Care >= 1) for (int32 t = 0; t < Tabs.Num(); ++t) if (Tabs[t].Key == ETab::Family) TabIndex = t;
		}

		virtual ~STameCard() override
		{
			WRInput::PopPadHandler(this);
			SWRTameNameBox::CloseIfOpen();
		}

		virtual void Tick(const FGeometry& Geo, const double Now, const float Dt) override
		{
			SCompoundWidget::Tick(Geo, Now, Dt);
			// It died, was released or stabled, or you walked off (or are riding it): close.
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
		TArray<TPair<ETab, FText>> Tabs;
		int32 TabIndex = 0;

		ETab CurrentTab() const { return Tabs.IsValidIndex(TabIndex) ? Tabs[TabIndex].Key : ETab::Overview; }

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

		bool HasSoulbond() const
		{
			const APlayerState* PS = PC.IsValid() ? PC->GetPlayerState<APlayerState>() : nullptr;
			return PS && UWRLifeSkillsComponent::StatFor(PS, TEXT("soulbond")) > 0.f;
		}

		void Send(const FString& Cmd)
		{
			const AWRCreature* T = Tame.Get();
			if (!PC.IsValid() || !T) return;
			const FVector L = T->GetActorLocation();
			PC->SendCommand(FString::Printf(TEXT("tamecard %.0f %.0f %.0f %s"), L.X, L.Y, L.Z, *Cmd));
			WRSound::PlayUI(PC.Get(), TEXT("ui.click"));
		}

		TSharedRef<SWidget> BagGrid()
		{
			TSharedRef<SUniformGridPanel> Bag = SNew(SUniformGridPanel).SlotPadding(FMargin(3.f * U));
			for (int32 i = 0; i < UWRInventoryComponent::NumSlots; ++i)
			{
				Bag->AddSlot(i % 10, i / 10)
				[
					SNew(SWRSlot).Size(46.f * U).Kind(EWRSlotKind::Bag).Index(i)
					.View_Lambda([this, i]()
					{
						FWRSlotView V;
						if (const UWRInventoryComponent* Inv = BagOf())
						{
							const TArray<FWRItemStack>& S = Inv->GetSlots();
							if (S.IsValidIndex(i) && !S[i].IsEmpty())
							{
								V.ItemId = S[i].ItemId;
								V.Count = S[i].Count;
								V.Quality = FMath::Clamp(int32(S[i].Quality) - 1, 0, 2);
								V.bDimmed = CurrentTab() == ETab::Gear && !FWRTaming::Tack(S[i].ItemId);
							}
						}
						return V;
					})
					.OnClicked_Lambda([this, i](const FPointerEvent& E) { BagClicked(i, E.IsShiftDown()); })
				];
			}
			return Bag;
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
			if (bHauler && CurrentTab() == ETab::Packs) Send(FString::Printf(TEXT("put %d %d"), i, bOne ? 1 : S.Count));
		}

		FString FoodName() const
		{
			const AWRCreature* T = Tame.Get();
			const FWRCreatureDef* D = T ? T->GetDef() : nullptr;
			return D && D->Food.Num() > 0 ? ItemName(D->Food[0]).ToLower() : FString(TEXT("its food"));
		}

		FString TitleLine() const
		{
			const AWRCreature* T = Tame.Get();
			const FWRCreatureDef* D = T ? T->GetDef() : nullptr;
			if (!T || !D) return TEXT("Tame");
			const FString Base = T->TameRep.Name.IsEmpty() ? D->Name : FString::Printf(TEXT("%s  ·  %s"), *T->TameRep.Name, *D->Name);
			return Base + (T->IsBaby() ? TEXT(" (baby)") : TEXT("")) + (T->TameRep.bSoulDown ? TEXT(" (down)") : TEXT(""));
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

		/** The stat as it stands now, in its own unit (cooks/cookpass-taming.md 3.5). */
		FString StatValue(int32 s) const
		{
			const AWRCreature* T = Tame.Get();
			const FWRCreatureDef* D = T ? T->GetDef() : nullptr;
			const UWRAttributeSet* A = T ? T->GetAttributes() : nullptr;
			if (!D) return FString();
			const float M = T->StatMult(s);
			switch (s)
			{
			case WRTameStat::Health: return A ? FString::Printf(TEXT("%.0f / %.0f"), A->GetHealth(), A->GetMaxHealth()) : FString();
			case WRTameStat::Stamina: return A ? FString::Printf(TEXT("%.0f"), A->GetMaxStamina()) : FString();
			case WRTameStat::Damage: return FString::Printf(TEXT("%.0f a bite"), D->Damage * M * (T->IsBaby() ? FMath::Lerp(WRTune(TEXT("tame.breed.babyStatsAtBirth"), 0.25f), 1.f, T->TameRep.Growth) : 1.f));
			case WRTameStat::Carry: return D->Carry > 0.f ? FString::Printf(TEXT("%.0f kg"), T->CargoCapacityKg()) : FString(TEXT("no packs"));
			case WRTameStat::Speed: return FString::Printf(TEXT("%.1f m/s"), (D->bMount ? D->RideSpeed : D->Speed) * M / 100.f);
			case WRTameStat::Torpor: return FString::Printf(TEXT("%.0f"), D->Health * WRTune(TEXT("taming.torporPerHealth"), 1.f) * M);
			default: return FString();
			}
		}

		FString StatLine(int32 s) const
		{
			const AWRCreature* T = Tame.Get();
			if (!T || !T->TameRep.Points.IsValidIndex(s)) return FString();
			const int32 Mut = T->TameRep.MutPoints.IsValidIndex(s) ? T->TameRep.MutPoints[s] : 0;
			return FString::Printf(TEXT("%d pt%s  ·  x%.2f%s"), T->TameRep.Points[s], T->TameRep.Points[s] == 1 ? TEXT("") : TEXT("s"), T->StatMult(s),
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
			return Line.IsEmpty() ? FString(TEXT("No marks: its own coat")) : FString::Printf(TEXT("Marked: %s"), *Line);
		}

		FString LinesLine() const
		{
			const AWRCreature* T = Tame.Get();
			return T ? FString::Printf(TEXT("Mutations carried: %d on its father's line, %d on its mother's (a line stops passing new ones at %d)"), T->TameRep.MutFather, T->TameRep.MutMother, int32(WRTune(TEXT("tame.breed.mutationCap"), 20.f))) : FString();
		}

		FString ParentsLine() const
		{
			const AWRCreature* T = Tame.Get();
			if (!T) return FString();
			if (T->TameRep.Father.IsEmpty() && T->TameRep.Mother.IsEmpty()) return TEXT("Wild-born: its parents ran free");
			return FString::Printf(TEXT("Father: %s\nMother: %s"), T->TameRep.Father.IsEmpty() ? TEXT("unknown") : *T->TameRep.Father, T->TameRep.Mother.IsEmpty() ? TEXT("unknown") : *T->TameRep.Mother);
		}

		FString SoulLine() const
		{
			const AWRCreature* T = Tame.Get();
			if (!T) return FString();
			if (T->TameRep.bSoulDown) return TEXT("Down, its soul holding it: it gets up soon");
			if (T->TameRep.bSoulbound) return TEXT("Soulbound: at the end it falls for a minute instead of dying");
			if (!HasSoulbond()) return TEXT("A tamer at Taming 100 (Soulbond) can bind one tame so it never dies for good");
			return TEXT("Not bound. Bind it and it falls for a minute at the end instead of dying (one tame at a time)");
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
			return TEXT("Riding: Sprint gallops on its stamina. Far off, whistle (hold) and Call your mount: it comes to you.");
		}
	};
}

WR_REGISTER_SCREEN("tamecard", [](AWRPlayerController* PC) -> TSharedRef<SWidget> { return SNew(WRUITameCardScreen::STameCard).PC(PC); });

#undef LOCTEXT_NAMESPACE
