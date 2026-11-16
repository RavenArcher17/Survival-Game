// The testers' god mode panel (EWRMenu::God, screen "god") and the GOD badge (cooks/build-godmode.md 2.4).
//
// Only a verified tester ever sees either (WRGod::IsLocalTester): the screen maps to nothing for anyone else and the
// badge never shows. Every button sends a "god ..." server command (WRGod::Run), which checks the tester flag again.
// The window is the stone window kit; a rail of seven pages on the left (Self, Travel, Creatures, Items, Progress,
// World, Quests), the page in a scroll box on the right. Every clickable piece is a button with the hand cursor, so the
// pad's cursor drives the whole panel (WRPadInput); the search boxes open the pad keyboard.

#include "WRGod.h"
#include "WRPlayerUI.h"
#include "WRUIStyle.h"
#include "WRUIWidgets.h"
#include "WRUIWindowKit.h"
#include "WRUIMap.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRCharacterBase.h"
#include "WRCreature.h"
#include "WRInventory.h"
#include "WRCrafting.h"
#include "WRPathComponent.h"
#include "WRPathTypes.h"
#include "WRLifeSkills.h"
#include "WRSurvival.h"
#include "WRFarm.h"
#include "WRGameState.h"
#include "WRWorldSystems.h"
#include "WRTuning.h"
#include "WRSound.h"
#include "WRPadInput.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SLeafWidget.h"

#define LOCTEXT_NAMESPACE "WRUIGod"

namespace WRUIGodPanel
{
	using WRUI::U;

	FLinearColor Srgb(uint8 R, uint8 G, uint8 B, float A = 1.f) { FLinearColor C(FColor(R, G, B)); C.A = A; return C; }
	const FLinearColor& Oxblood() { static const FLinearColor C = Srgb(0x5A, 0x12, 0x10); return C; }
	const FLinearColor& Gold() { static const FLinearColor C = Srgb(0xF0, 0xC8, 0x6A); return C; }
	const FLinearColor& GodRed() { static const FLinearColor C = Srgb(0xFF, 0x6B, 0x2E); return C; }

	/** The panel's sigil: an oxblood disc, a brass rim, gold rays and an open eye (the badge in the title band). */
	class SGodSigil : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SGodSigil) : _Size(40.f * U) {}
			SLATE_ARGUMENT(float, Size)
		SLATE_END_ARGS()
		void Construct(const FArguments& InArgs) { Size = InArgs._Size; }
		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(Size, Size); }
		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override
		{
			using namespace WRUIWindowKit;
			const float R = Size * 0.5f;
			const FVector2D C(R, R);
			for (int32 i = 3; i >= 1; --i) DrawCircle(Out, Layer, Geo, C, R + i * 2.5f, WRUI::Alpha(GodRed(), 0.07f));
			DrawCircle(Out, Layer + 1, Geo, C, R, Oxblood(), WRUI::Brass2, 2.f);
			// Twelve rays round the eye.
			for (int32 i = 0; i < 12; ++i)
			{
				const float A = 2.f * PI * i / 12.f;
				const FVector2D Dir(FMath::Cos(A), FMath::Sin(A));
				FSlateDrawElement::MakeLines(Out, Layer + 2, Geo.ToPaintGeometry(), TArray<FVector2D>{ C + Dir * R * 0.52f, C + Dir * R * (i % 2 ? 0.7f : 0.8f) }, ESlateDrawEffect::None, Gold(), true, 1.5f);
			}
			// The eye: an almond of gold with a dark iris and a gold pupil-glint.
			TArray<FVector2D> Almond;
			for (int32 i = 0; i <= 12; ++i) { const float T = float(i) / 12.f; Almond.Add(C + FVector2D(FMath::Lerp(-0.42f, 0.42f, T) * R, -FMath::Sin(T * PI) * 0.24f * R)); }
			for (int32 i = 11; i >= 1; --i) { const float T = float(i) / 12.f; Almond.Add(C + FVector2D(FMath::Lerp(-0.42f, 0.42f, T) * R, FMath::Sin(T * PI) * 0.24f * R)); }
			FillShape(Out, Layer + 3, Geo, Almond, C, Gold());
			DrawCircle(Out, Layer + 4, Geo, C, R * 0.17f, Oxblood());
			DrawCircle(Out, Layer + 5, Geo, C - FVector2D(R * 0.05f, R * 0.05f), R * 0.06f, Gold());
			return Layer + 5;
		}
	private:
		float Size = 40.f;
	};

	/** A plain clickable (SButton, no border: the pad's cursor treats it as a button) around any content. */
	TSharedRef<SWidget> Clickable(TSharedRef<SWidget> Content, TFunction<void()> Click, const FText& Tip = FText::GetEmpty())
	{
		return SNew(SButton).ButtonStyle(FCoreStyle::Get(), "NoBorder").ContentPadding(0.f).Cursor(EMouseCursor::Hand).ToolTipText(Tip)
			.OnClicked_Lambda([Click]() { Click(); return FReply::Handled(); })[Content];
	}

	/** A small leather action button. */
	TSharedRef<SWidget> Action(const FText& Label, TFunction<void()> Click, bool bPrimary = false)
	{
		return SNew(SWRButton).Ghost(!bPrimary).Primary(bPrimary).MinHeight(32.f * U).FontSize(12).Text(Label)
			.OnClicked_Lambda([Click]() { Click(); return FReply::Handled(); });
	}

	/** A filter / amount chip: lit when picked. */
	TSharedRef<SWidget> Chip(const FText& Label, TFunction<bool()> Picked, TFunction<void()> Click)
	{
		return Clickable(
			SNew(SBorder).Padding(FMargin(10.f * U, 4.f * U))
			.BorderImage_Lambda([Picked]() { return Picked() ? WRUI::Box(Srgb(0x6A, 0x3A, 0x1C), 11.f * U, WRUI::Brass, 1.f) : WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.4f), 11.f * U, FLinearColor::Black, 1.f); })
			[
				WRUI::Text(Label, WRUI::Body(12, 700), TAttribute<FSlateColor>::CreateLambda([Picked]() { return FSlateColor(Picked() ? WRUI::Ink : WRUI::Ink2); }))
			], Click);
	}

	/** The on / off pill. */
	TSharedRef<SWidget> Pill(TFunction<bool()> On)
	{
		return SNew(SBox).WidthOverride(52.f * U).HeightOverride(28.f * U)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[SNew(SImage).Image_Lambda([On]() { return On() ? WRUI::Box(Srgb(0x6A, 0x1C, 0x14), 14.f * U, GodRed(), 1.f) : WRUI::Box(Srgb(0x0C, 0x0D, 0x0F), 14.f * U, FLinearColor::Black, 1.f); })]
				+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(TAttribute<FMargin>::CreateLambda([On]() { return FMargin(On() ? 28.f * U : 4.f * U, 4.f * U, 0.f, 0.f); }))
				[
					SNew(SBox).WidthOverride(20.f * U).HeightOverride(20.f * U)
					[
						SNew(SImage).Image_Lambda([On]() { return WRUI::Box(On() ? Gold() : Srgb(0x47, 0x4C, 0x52), 10.f * U, FLinearColor::Black, 1.f); })
					]
				]
			];
	}

	/** A card row: a name and a line under it on the left, controls on the right. */
	TSharedRef<SWidget> Row(const TAttribute<FText>& Name, const TAttribute<FText>& Line, TSharedRef<SWidget> Right, const FLinearColor& NameColor = WRUI::Ink)
	{
		return SNew(SBox).Padding(FMargin(0.f, 3.f * U))
			[
				SNew(SBorder).BorderImage(WRUIWindowKit::CardBrush()).Padding(FMargin(12.f * U, 7.f * U))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f * U, 0.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(Name, WRUI::Body(14, 700), NameColor)]
						+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(Line, WRUI::Body(12), WRUI::Ink3, false, true)]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Right]
				]
			];
	}

	TSharedRef<SWidget> Heading(const FText& T)
	{
		return SNew(SBox).Padding(FMargin(0.f, 12.f * U, 0.f, 4.f * U))[WRUI::Text(T, WRUI::Caps(12), WRUI::Brass)];
	}

	TSharedRef<SWidget> Muted(const FText& T)
	{
		return SNew(SBox).Padding(FMargin(0.f, 4.f * U))[WRUI::Text(T, WRUI::Body(12), WRUI::Ink3, false, true)];
	}

	FText KindWord(FName Kind)
	{
		static const TMap<FName, FText> Words = {
			{ TEXT("land"), LOCTEXT("KLand", "Land") }, { TEXT("boss"), LOCTEXT("KBoss", "Boss arena") }, { TEXT("shrine"), LOCTEXT("KShrine", "Shrine") },
			{ TEXT("village"), LOCTEXT("KVillage", "Village") }, { TEXT("camp"), LOCTEXT("KCamp", "Camp") }, { TEXT("portal"), LOCTEXT("KPortal", "Portal") },
			{ TEXT("cave"), LOCTEXT("KCave", "Cave") }, { TEXT("pass"), LOCTEXT("KPass", "Pass") }, { TEXT("landmark"), LOCTEXT("KLandmark", "Landmark") },
			{ TEXT("place"), LOCTEXT("KPlace", "Place") }, { TEXT("player"), LOCTEXT("KPlayer", "Player") } };
		const FText* W = Words.Find(Kind);
		return W ? *W : FText::FromName(Kind);
	}

	FString LandName(FName Land)
	{
		return Land.IsNone() ? FString(TEXT("between lands")) : WRBiome::Name(Land);
	}

	enum class EPage : uint8 { Self, Travel, Creatures, Items, Progress, World, Quests };
	enum class EItems : uint8 { Items, Sets, Recipes, Unlock };

	class SGodPanel : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SGodPanel) {}
			SLATE_ARGUMENT(TWeakObjectPtr<AWRPlayerController>, PC)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			PC = InArgs._PC;
			SAssignNew(Body, SVerticalBox);
			SearchStyle = FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");
			const FSlateBrush* Field = WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.55f), 5.f, WRUI::Brass2, 1.f);
			SearchStyle.SetBackgroundImageNormal(*Field).SetBackgroundImageHovered(*Field).SetBackgroundImageFocused(*Field).SetBackgroundImageReadOnly(*Field);
			SearchStyle.SetForegroundColor(FSlateColor(WRUI::Ink)).SetFocusedForegroundColor(FSlateColor(WRUI::Ink)).SetBackgroundColor(FSlateColor(FLinearColor::White));
			SearchStyle.SetPadding(FMargin(10.f * U, 6.f * U));

			TSharedRef<SVerticalBox> Rail = SNew(SVerticalBox);
			const TPair<EPage, FText> Pages[] = {
				{ EPage::Self, LOCTEXT("PSelf", "Self") }, { EPage::Travel, LOCTEXT("PTravel", "Travel") }, { EPage::Creatures, LOCTEXT("PCreatures", "Creatures") },
				{ EPage::Items, LOCTEXT("PItems", "Items") }, { EPage::Progress, LOCTEXT("PProgress", "Progress") }, { EPage::World, LOCTEXT("PWorld", "World") },
				{ EPage::Quests, LOCTEXT("PQuests", "Quests") } };
			for (const TPair<EPage, FText>& P : Pages)
			{
				const EPage Id = P.Key;
				Rail->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f * U)
				[
					Clickable(
						SNew(SBox).HeightOverride(44.f * U)
						[
							SNew(SBorder).Padding(FMargin(14.f * U, 0.f)).VAlign(VAlign_Center)
							.BorderImage_Lambda([this, Id]() { return Page == Id ? WRUI::Box(Srgb(0x4A, 0x16, 0x10), 5.f, Gold(), 1.f) : WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.35f), 5.f, FLinearColor::Black, 1.f); })
							[
								WRUI::Text(P.Value, WRUI::Title(14), TAttribute<FSlateColor>::CreateLambda([this, Id]() { return FSlateColor(Page == Id ? Gold() : WRUI::Ink2); }))
							]
						], [this, Id]() { SetPage(Id); })
				];
			}
			Rail->AddSlot().FillHeight(1.f)[SNullWidget::NullWidget];
			Rail->AddSlot().AutoHeight()[WRUI::Text(LOCTEXT("RailFoot", "Every action is checked by the server against your Freedom35 account."), WRUI::Body(11), WRUI::Ink3, false, true)];

			const TSharedRef<SWidget> Content = SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f * U)
				[
					SNew(SBorder).Visibility_Lambda([this]() { const UWRGodComponent* G = God(); return G && (!G->bCharacterTested || G->bPublicWorld) ? EVisibility::Visible : EVisibility::Collapsed; })
					.BorderImage(WRUI::Box(Srgb(0x3A, 0x10, 0x0C, 0.85f), 5.f, GodRed(), 1.f)).Padding(FMargin(12.f * U, 8.f * U))
					[
						WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return BannerText(); }), WRUI::Body(13, 700), WRUI::Ink, false, true)
					]
				]
				+ SVerticalBox::Slot().FillHeight(1.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 16.f * U, 0.f)[SNew(SBox).WidthOverride(190.f * U)[Rail]]
					+ SHorizontalBox::Slot().FillWidth(1.f)[SNew(SScrollBox) + SScrollBox::Slot()[Body.ToSharedRef()]]
				];

			ChildSlot
			[
				WRUIWindowKit::Window(PC, SNew(SGodSigil), LOCTEXT("Title", "God mode"),
					TAttribute<FText>::CreateLambda([this]() { const UWRGodComponent* G = God(); return FText::Format(LOCTEXT("Sub", "Tester tools  ·  {0}  ·  only Freedom35's developers have this"), FText::FromString(G ? G->Label : FString())); }),
					TAttribute<FText>::CreateLambda([this]() { return StateChip(); }),
					TAttribute<FLinearColor>::CreateLambda([this]() { const UWRGodComponent* G = God(); return !G ? WRUI::Ink3 : G->bPublicWorld ? WRUI::Bad : G->bWorldTested ? WRUI::Warn : WRUI::Good; }),
					Content)
			];
			Rebuild();
		}

		virtual void Tick(const FGeometry& Geo, const double Now, const float Dt) override
		{
			SCompoundWidget::Tick(Geo, Now, Dt);
			AWRPlayerController* P = PC.Get();
			if (P && P->GetMenu() == EWRMenu::God && !WRGod::IsLocalTester(P))
			{
				P->SetMenu(EWRMenu::None);   // no longer a tester (a new connection): the panel goes
				return;
			}
			if (Now >= NextCheck)
			{
				NextCheck = Now + 0.25;
				const uint32 K = Key();
				if (K != LastKey) { Rebuild(); WRUI::MeasureNow(*Body, Geo.Scale); }
			}
		}

		virtual bool SupportsKeyboardFocus() const override { return true; }

	private:
		TWeakObjectPtr<AWRPlayerController> PC;
		TSharedPtr<SVerticalBox> Body;
		FEditableTextBoxStyle SearchStyle;
		EPage Page = EPage::Self;
		EItems ItemsPage = EItems::Items;
		FString Search;
		FName Filter;      // Travel: a kind; Creatures: a land; Items: a category
		int32 Amount = 1;
		uint32 LastKey = 0;
		double NextCheck = 0.0;
		bool bAskedPlaces = false;

		const UWRGodComponent* God() const { return UWRGodComponent::For(PC.Get()); }
		AWRPlayerState* State() const { return PC.IsValid() ? PC->GetPlayerState<AWRPlayerState>() : nullptr; }

		void Send(const FString& Cmd)
		{
			if (AWRPlayerController* P = PC.Get())
			{
				P->SendCommand(TEXT("god ") + Cmd);
				WRSound::PlayUI(P, TEXT("ui.click"));
			}
		}

		void SetPage(EPage P)
		{
			if (Page == P) return;
			Page = P;
			Search.Reset();
			Filter = NAME_None;
			Amount = 1;
			if (P == EPage::Travel) { Send(TEXT("places")); bAskedPlaces = true; }
			Rebuild();
		}

		FText StateChip() const
		{
			const UWRGodComponent* G = God();
			if (!G) return FText::GetEmpty();
			if (G->bPublicWorld) return LOCTEXT("ChipPublic", "Public world: off");
			if (G->bWorldTested) return LOCTEXT("ChipTested", "World marked tested");
			return LOCTEXT("ChipTesting", "Testing world");
		}

		FText BannerText() const
		{
			const UWRGodComponent* G = God();
			const AWRPlayerState* PS = State();
			if (G && G->bPublicWorld) return LOCTEXT("BannerPublic", "This is a public server: god mode is off here so its world stays clean. It works in single player, on a hosted world and on playtest servers.");
			return FText::Format(LOCTEXT("Banner", "Using anything here marks {0} as a test character for good: no achievements or kill-time records from it. World changes (time, weather, events, spawns, freezing, kills) mark this world too."),
				FText::FromString(PS ? PS->GetPlayerName() : FString(TEXT("this character"))));
		}

		/** What the page shows depends on: rebuilt when it changes. */
		uint32 Key() const
		{
			uint32 H = HashCombine(GetTypeHash(uint8(Page)), GetTypeHash(uint8(ItemsPage)));
			H = HashCombine(H, HashCombine(GetTypeHash(Search), HashCombine(GetTypeHash(Filter), GetTypeHash(Amount))));
			const UWRGodComponent* G = God();
			const AWRPlayerState* PS = State();
			if (Page == EPage::Travel && G) H = HashCombine(H, GetTypeHash(G->PlacesSerial));
			if (Page == EPage::Progress && PS)
			{
				const UWRPathComponent* Paths = PS->GetPaths();
				H = HashCombine(H, HashCombine(GetTypeHash(Paths->GetMainPath()), GetTypeHash(Paths->GetSubPath())));
				for (const FWRPathDef& D : FWRPathRegistry::Get().All()) H = HashCombine(H, HashCombine(GetTypeHash(Paths->GetEarned(D.Id)), GetTypeHash(Paths->GetSpent(D.Id))));
				if (const UWRLifeSkillsComponent* S = PS->GetLifeSkills()) for (const FWRSkillProgress& P : S->GetSkills()) H = HashCombine(H, GetTypeHash(P.Level));
			}
			if (Page == EPage::Quests && PS)
			{
				if (const UWRQuestLog* Log = UWRQuestLog::For(PS)) for (const FWRQuestRep& Q : Log->Quests) H = HashCombine(H, HashCombine(GetTypeHash(Q.Master), GetTypeHash(Q.Index)));
			}
			return H;
		}

		TSharedRef<SWidget> SearchBox(const FText& Hint)
		{
			return SNew(SBox).WidthOverride(320.f * U)
				[
					SNew(SEditableTextBox).Style(&SearchStyle).Font(WRUI::Body(14, 500)).HintText(Hint).Text(FText::FromString(Search))
					.OnTextChanged_Lambda([this](const FText& T) { Search = T.ToString().TrimStartAndEnd(); })
				];
		}

		bool Matches(const FString& A, const FString& B = FString()) const
		{
			return Search.IsEmpty() || A.Contains(Search) || (!B.IsEmpty() && B.Contains(Search));
		}

		TSharedRef<SWidget> Amounts(const TArray<int32>& Choices, const FText& StackWord = FText::GetEmpty())
		{
			TSharedRef<SHorizontalBox> Box = SNew(SHorizontalBox) + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f * U, 0.f)[WRUI::Text(LOCTEXT("Amount", "How many"), WRUI::Caps(11), WRUI::Ink3)];
			for (int32 N : Choices)
			{
				const FText Label = N < 0 ? StackWord : FText::AsNumber(N);
				Box->AddSlot().AutoWidth().Padding(0.f, 0.f, 6.f * U, 0.f)[Chip(Label, [this, N]() { return Amount == N; }, [this, N]() { Amount = N; })];
			}
			return Box;
		}

		void Rebuild()
		{
			LastKey = Key();
			Body->ClearChildren();
			switch (Page)
			{
			case EPage::Self: BuildSelf(); break;
			case EPage::Travel: BuildTravel(); break;
			case EPage::Creatures: BuildCreatures(); break;
			case EPage::Items: BuildItems(); break;
			case EPage::Progress: BuildProgress(); break;
			case EPage::World: BuildWorld(); break;
			case EPage::Quests: BuildQuests(); break;
			}
		}

		// ---------------------------------------------------------------- Self

		void Power(uint16 Flag, const TCHAR* Word, const FText& Name, const FText& What)
		{
			const auto On = [this, Flag]() { const UWRGodComponent* G = God(); return G && (G->Flags & Flag) != 0; };
			Body->AddSlot().AutoHeight()
			[
				Row(Name, What, Clickable(Pill(On), [this, On, Word]() { Send(FString::Printf(TEXT("set %s %d"), Word, On() ? 0 : 1)); }))
			];
		}

		void BuildSelf()
		{
			Body->AddSlot().AutoHeight()[Heading(LOCTEXT("HPowers", "POWERS"))];
			Power(EWRGodFlag::Invulnerable, TEXT("invuln"), LOCTEXT("Invuln", "Invulnerable"), LOCTEXT("InvulnWhat", "Blows still land (guard, stagger, sparks, sounds) but take no health; nothing kills you."));
			Power(EWRGodFlag::Vitals, TEXT("vitals"), LOCTEXT("Vitals", "Endless vitals"), LOCTEXT("VitalsWhat", "Stamina, every path's bar (Heat, Chill, Mana...), hunger and thirst stay full; corruption and status build-ups stay empty. Off to test running dry."));
			Power(EWRGodFlag::OneHit, TEXT("onehit"), LOCTEXT("OneHit", "One-hit kill"), LOCTEXT("OneHitWhat", "A blow of yours that lands takes the whole of the target's health (bosses and their phases too)."));
			Power(EWRGodFlag::Fly, TEXT("fly"), LOCTEXT("Fly", "Fly"), FText::Format(LOCTEXT("FlyWhat", "Forward goes where you look; {0} rises, {1} or the Dodge button sinks; Sprint doubles the speed."),
				WRInput::ActionKey(TEXT("Jump")), WRInput::ActionKey(TEXT("Dive"))));
			Power(EWRGodFlag::Noclip, TEXT("noclip"), LOCTEXT("Noclip", "Pass through walls"), LOCTEXT("NoclipWhat", "Flying with no collision: through rock, walls and the ground. Off drops you where you are."));
			Power(EWRGodFlag::Unseen, TEXT("unseen"), LOCTEXT("Unseen", "Unseen"), LOCTEXT("UnseenWhat", "Creatures and bosses never pick you as a target, and anything after you lets you go."));
			Body->AddSlot().AutoHeight()
			[
				Row(LOCTEXT("FlySpeed", "Fly speed"), TAttribute<FText>::CreateLambda([this]()
				{
					const UWRGodComponent* G = God();
					const float X = G ? G->FlySpeed : 1.f;
					return FText::Format(LOCTEXT("FlySpeedLine", "x{0}  ({1} m/s)"), FText::AsNumber(X), FText::AsNumber(FMath::RoundToInt(WRTune(TEXT("god.flyBase"), 600.f) * X / 100.f)));
				}),
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 6.f * U, 0.f)[Action(LOCTEXT("Slower", "Slower"), [this]() { Step(-1); })]
					+ SHorizontalBox::Slot().AutoWidth()[Action(LOCTEXT("Faster", "Faster"), [this]() { Step(1); })])
			];
			{
				const auto On = [this]() { const UWRGodComponent* G = God(); return G && G->bShowHitboxes; };
				Body->AddSlot().AutoHeight()
				[
					Row(LOCTEXT("Hitboxes", "Hitboxes and aggro"), LOCTEXT("HitboxesWhat", "On your screen only, within 30 m: every body's capsule (you and players green, tames blue, wild amber, red when it's after you) and a line to whoever each creature is after."),
						Clickable(Pill(On), [this]() { if (UWRGodComponent* G = UWRGodComponent::For(PC.Get())) G->bShowHitboxes = !G->bShowHitboxes; }))
				];
			}
			Body->AddSlot().AutoHeight()[Heading(LOCTEXT("HNow", "NOW"))];
			Body->AddSlot().AutoHeight()
			[
				SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8.f * U, 8.f * U))
				+ SWrapBox::Slot()[Action(LOCTEXT("Heal", "Heal fully"), [this]() { Send(TEXT("heal")); }, true)]
				+ SWrapBox::Slot()[Action(LOCTEXT("ClearStatus", "Clear statuses"), [this]() { Send(TEXT("clearstatus")); })]
				+ SWrapBox::Slot()[Action(LOCTEXT("Revive", "Revive where I fell"), [this]() { Send(TEXT("revive")); })]
				+ SWrapBox::Slot()[Action(FText::Format(LOCTEXT("KillAll", "Kill everything within {0} m"), FText::AsNumber(FMath::RoundToInt(WRTune(TEXT("god.killRadius"), 4000.f) / 100.f))), [this]() { Send(TEXT("killall")); })]
			];
		}

		void Step(int32 Dir)
		{
			static const float Speeds[] = { 0.5f, 1.f, 2.f, 3.f, 5.f, 8.f, 10.f };
			const UWRGodComponent* G = God();
			const float Now = G ? G->FlySpeed : 1.f;
			int32 At = 0;
			for (int32 i = 0; i < UE_ARRAY_COUNT(Speeds); ++i) if (FMath::Abs(Speeds[i] - Now) < FMath::Abs(Speeds[At] - Now)) At = i;
			At = FMath::Clamp(At + Dir, 0, int32(UE_ARRAY_COUNT(Speeds)) - 1);
			Send(FString::Printf(TEXT("flyspeed %.2f"), Speeds[At]));
		}

		// ---------------------------------------------------------------- Travel

		void BuildTravel()
		{
			const UWRGodComponent* G = God();
			if (G && G->Places.Num() == 0 && !bAskedPlaces) { Send(TEXT("places")); bAskedPlaces = true; }
			Body->AddSlot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 8.f * U, 0.f)[SearchBox(LOCTEXT("TravelHint", "Search places..."))]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 8.f * U, 0.f)[Action(LOCTEXT("Refresh", "Refresh the list"), [this]() { Send(TEXT("places")); })]
				+ SHorizontalBox::Slot().AutoWidth()[Action(LOCTEXT("Waypoint", "Go to my map waypoint"), [this]()
				{
					FVector Where;
					if (PC.IsValid() && WRUIMap::GetWaypoint(PC->GetWorld(), Where)) Send(FString::Printf(TEXT("goxy %.0f %.0f"), Where.X, Where.Y));
					else if (PC.IsValid()) PC->ShowLocalNotice(TEXT("No waypoint: set one on the map first"));
				})]
			];
			TSharedRef<SWrapBox> Kinds = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(6.f * U, 6.f * U));
			Kinds->AddSlot()[Chip(LOCTEXT("All", "All"), [this]() { return Filter.IsNone(); }, [this]() { Filter = NAME_None; })];
			for (const TCHAR* K : { TEXT("land"), TEXT("boss"), TEXT("shrine"), TEXT("village"), TEXT("camp"), TEXT("portal"), TEXT("cave"), TEXT("pass"), TEXT("landmark"), TEXT("place"), TEXT("player") })
			{
				const FName Kind(K);
				if (G && !G->Places.ContainsByPredicate([Kind](const FWRGodPlace& P) { return P.Kind == Kind; })) continue;
				Kinds->AddSlot()[Chip(KindWord(Kind), [this, Kind]() { return Filter == Kind; }, [this, Kind]() { Filter = Kind; })];
			}
			Body->AddSlot().AutoHeight().Padding(0.f, 8.f * U)[Kinds];
			if (!G || G->Places.Num() == 0)
			{
				Body->AddSlot().AutoHeight()[Muted(LOCTEXT("PlacesWait", "Asking the server for the list..."))];
				return;
			}
			const APawn* Me = PC.IsValid() ? PC->GetPawn() : nullptr;
			const int32 Max = int32(WRTune(TEXT("god.listMax"), 60.f));
			int32 Shown = 0, More = 0;
			for (int32 i = 0; i < G->Places.Num(); ++i)
			{
				const FWRGodPlace& P = G->Places[i];
				if ((!Filter.IsNone() && P.Kind != Filter) || !Matches(P.Name, LandName(P.Land))) continue;
				if (Shown >= Max) { ++More; continue; }
				++Shown;
				const FString Dist = Me ? WRUIWindowKit::Distance(FVector::Dist2D(Me->GetActorLocation(), P.Where)) : FString();
				const FText Line = FText::Format(LOCTEXT("PlaceLine", "{0}  ·  {1}  ·  {2} away"), KindWord(P.Kind), FText::FromString(LandName(P.Land)), FText::FromString(Dist));
				Body->AddSlot().AutoHeight()[Row(FText::FromString(P.Name), Line, Action(LOCTEXT("Go", "Go"), [this, i]() { Send(FString::Printf(TEXT("go %d"), i)); }, true))];
			}
			if (More > 0) Body->AddSlot().AutoHeight()[Muted(FText::Format(LOCTEXT("MorePlaces", "{0} more: type to narrow the list."), FText::AsNumber(More)))];
			if (Shown == 0) Body->AddSlot().AutoHeight()[Muted(LOCTEXT("NoPlaces", "Nothing matches."))];
		}

		// ---------------------------------------------------------------- Creatures

		void BuildCreatures()
		{
			Body->AddSlot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 14.f * U, 0.f)[SearchBox(LOCTEXT("CreatureHint", "Search creatures..."))]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Amounts({ 1, 3, 5, 10 })]
			];
			TSet<FName> Lands;
			for (const FWRCreatureDef& D : FWRCreatureRegistry::Get().All()) Lands.Add(D.Biome);
			TArray<FName> Sorted = Lands.Array();
			Sorted.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
			TSharedRef<SWrapBox> Chips = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(6.f * U, 6.f * U));
			Chips->AddSlot()[Chip(LOCTEXT("All", "All"), [this]() { return Filter.IsNone(); }, [this]() { Filter = NAME_None; })];
			for (const FName& L : Sorted)
			{
				if (L.IsNone()) continue;
				Chips->AddSlot()[Chip(FText::FromString(LandName(L)), [this, L]() { return Filter == L; }, [this, L]() { Filter = L; })];
			}
			Body->AddSlot().AutoHeight().Padding(0.f, 8.f * U)[Chips];
			TArray<const FWRCreatureDef*> Defs;
			for (const FWRCreatureDef& D : FWRCreatureRegistry::Get().All())
			{
				if ((!Filter.IsNone() && D.Biome != Filter) || !Matches(D.Name, D.Id.ToString())) continue;
				Defs.Add(&D);
			}
			// Bosses first (the majors, minis, portal bosses), then the rest by name.
			Defs.Sort([](const FWRCreatureDef& A, const FWRCreatureDef& B)
			{
				const bool AB = A.Archetype == TEXT("boss"), BB = B.Archetype == TEXT("boss");
				return AB != BB ? AB : A.Name < B.Name;
			});
			const int32 Max = int32(WRTune(TEXT("god.listMax"), 60.f));
			for (int32 i = 0; i < Defs.Num() && i < Max; ++i)
			{
				const FWRCreatureDef& D = *Defs[i];
				const FName Id = D.Id;
				const bool bBoss = D.Archetype == TEXT("boss");
				const FText Line = FText::Format(LOCTEXT("CreatureLine", "{0}  ·  {1}  ·  {2}"), FText::FromName(D.Tier), FText::FromString(D.Role.IsEmpty() ? D.Archetype.ToString() : D.Role), FText::FromString(LandName(D.Biome)));
				TSharedRef<SHorizontalBox> Buttons = SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()[Action(LOCTEXT("Spawn", "Spawn"), [this, Id]() { Send(FString::Printf(TEXT("spawn %s %d"), *Id.ToString(), Amount)); }, true)];
				if (!bBoss) Buttons->AddSlot().AutoWidth().Padding(6.f * U, 0.f, 0.f, 0.f)[Action(LOCTEXT("SpawnTamed", "Spawn tamed"), [this, Id]() { Send(FString::Printf(TEXT("spawn %s %d tamed"), *Id.ToString(), Amount)); })];
				Body->AddSlot().AutoHeight()[Row(FText::FromString(D.Name), Line, Buttons, bBoss ? GodRed() : WRUI::Ink)];
			}
			if (Defs.Num() > Max) Body->AddSlot().AutoHeight()[Muted(FText::Format(LOCTEXT("MoreCreatures", "{0} more: type to narrow the list."), FText::AsNumber(Defs.Num() - Max)))];
			if (Defs.Num() == 0) Body->AddSlot().AutoHeight()[Muted(LOCTEXT("NoCreatures", "Nothing matches."))];
		}

		// ---------------------------------------------------------------- Items

		void BuildItems()
		{
			TSharedRef<SHorizontalBox> Tabs = SNew(SHorizontalBox);
			const TPair<EItems, FText> Subs[] = { { EItems::Items, LOCTEXT("SItems", "Items") }, { EItems::Sets, LOCTEXT("SSets", "Armour sets") }, { EItems::Recipes, LOCTEXT("SRecipes", "Recipes") }, { EItems::Unlock, LOCTEXT("SUnlock", "Unlock all") } };
			for (const TPair<EItems, FText>& S : Subs)
			{
				const EItems Id = S.Key;
				Tabs->AddSlot().AutoWidth().Padding(0.f, 0.f, 6.f * U, 0.f)[Chip(S.Value, [this, Id]() { return ItemsPage == Id; }, [this, Id]() { if (ItemsPage != Id) { ItemsPage = Id; Search.Reset(); Filter = NAME_None; Amount = 1; } })];
			}
			Body->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f * U)[Tabs];
			switch (ItemsPage)
			{
			case EItems::Items: BuildItemList(); break;
			case EItems::Sets: BuildSets(); break;
			case EItems::Recipes: BuildRecipes(); break;
			case EItems::Unlock: BuildUnlock(); break;
			}
		}

		void BuildItemList()
		{
			Body->AddSlot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 14.f * U, 0.f)[SearchBox(LOCTEXT("ItemHint", "Search items..."))]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Amounts({ 1, 10, 50, -1 }, LOCTEXT("Stack", "A stack"))]
			];
			TMap<FName, int32> Counts;
			for (const FWRItemDef& D : FWRItemRegistry::Get().All()) if (!D.Category.IsNone()) Counts.FindOrAdd(D.Category)++;
			Counts.ValueSort([](int32 A, int32 B) { return A > B; });
			TSharedRef<SWrapBox> Chips = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(6.f * U, 6.f * U));
			Chips->AddSlot()[Chip(LOCTEXT("All", "All"), [this]() { return Filter.IsNone(); }, [this]() { Filter = NAME_None; })];
			for (const TPair<FName, int32>& C : Counts)
			{
				const FName Cat = C.Key;
				Chips->AddSlot()[Chip(FText::FromString(Cat.ToString().Replace(TEXT("_"), TEXT(" "))), [this, Cat]() { return Filter == Cat; }, [this, Cat]() { Filter = Cat; })];
			}
			Body->AddSlot().AutoHeight().Padding(0.f, 8.f * U)[Chips];
			const int32 Max = int32(WRTune(TEXT("god.listMax"), 60.f));
			int32 Shown = 0, More = 0;
			for (const FWRItemDef& D : FWRItemRegistry::Get().All())
			{
				if (D.Id.IsNone() || (!Filter.IsNone() && D.Category != Filter) || !Matches(D.Name, D.Id.ToString())) continue;
				if (Shown >= Max) { ++More; continue; }
				++Shown;
				const FName Id = D.Id;
				const int32 Stack = FMath::Max(1, D.StackSize);
				Body->AddSlot().AutoHeight()
				[
					Row(FText::FromString(D.Name), FText::Format(LOCTEXT("ItemLine", "{0}  ·  stacks to {1}"), FText::FromName(D.Category), FText::AsNumber(Stack)),
						Action(LOCTEXT("Give", "Give"), [this, Id, Stack]() { Send(FString::Printf(TEXT("give %s %d"), *Id.ToString(), Amount < 0 ? Stack : Amount)); }, true))
				];
			}
			if (More > 0) Body->AddSlot().AutoHeight()[Muted(FText::Format(LOCTEXT("MoreItems", "{0} more: type to narrow the list."), FText::AsNumber(More)))];
			if (Shown == 0) Body->AddSlot().AutoHeight()[Muted(LOCTEXT("NoItems", "Nothing matches."))];
		}

		void BuildSets()
		{
			const TSharedPtr<FJsonObject> Gear = FWRTuning::LoadDataFile(TEXT("gear.json"));
			const TArray<TSharedPtr<FJsonValue>>* Sets = nullptr;
			if (!Gear.IsValid() || !Gear->TryGetArrayField(TEXT("sets"), Sets)) return;
			Body->AddSlot().AutoHeight()[Muted(LOCTEXT("SetsWhat", "All four pieces (head, armour, hands, legs) into your bag; Ascendant is the set's upgraded version where it has one."))];
			for (const TSharedPtr<FJsonValue>& V : *Sets)
			{
				const TSharedPtr<FJsonObject> S = V->AsObject();
				FString Id, Name, Biome, Load;
				if (!S.IsValid() || !S->TryGetStringField(TEXT("id"), Id)) continue;
				S->TryGetStringField(TEXT("name"), Name);
				S->TryGetStringField(TEXT("biome"), Biome);
				S->TryGetStringField(TEXT("load"), Load);
				const bool bAscendant = FWRItemRegistry::Get().Find(FName(*(Id + TEXT("_ascendant_armor")))) != nullptr;
				TSharedRef<SHorizontalBox> Buttons = SNew(SHorizontalBox) + SHorizontalBox::Slot().AutoWidth()[Action(LOCTEXT("GiveSet", "Give set"), [this, Id]() { Send(TEXT("giveset ") + Id); }, true)];
				if (bAscendant) Buttons->AddSlot().AutoWidth().Padding(6.f * U, 0.f, 0.f, 0.f)[Action(LOCTEXT("GiveAsc", "Give Ascendant"), [this, Id]() { Send(TEXT("giveset ") + Id + TEXT(" asc")); })];
				Body->AddSlot().AutoHeight()[Row(FText::FromString(Name.IsEmpty() ? Id : Name), FText::Format(LOCTEXT("SetLine", "{0}  ·  {1} load"), FText::FromString(LandName(FName(*Biome))), FText::FromString(Load)), Buttons)];
			}
		}

		void BuildRecipes()
		{
			Body->AddSlot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 14.f * U, 0.f)[SearchBox(LOCTEXT("RecipeHint", "Search recipes..."))]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Amounts({ 1, 5 })]
			];
			const int32 Max = int32(WRTune(TEXT("god.listMax"), 60.f));
			int32 Shown = 0, More = 0;
			for (const FWRRecipe& R : FWRRecipeRegistry::Get().All())
			{
				const FString Out = WRWorld::ItemName(R.Output);
				if (!Matches(Out, R.Id.ToString())) continue;
				if (Shown >= Max) { ++More; continue; }
				++Shown;
				const FName Id = R.Id;
				TArray<FString> Ins;
				for (const TPair<FName, int32>& In : R.Inputs) Ins.Add(FString::Printf(TEXT("%d %s"), In.Value, *WRWorld::ItemName(In.Key)));
				const FText Line = FText::Format(LOCTEXT("RecipeLine", "{0}  ·  {1}"), FText::FromName(R.Station), FText::FromString(FString::Join(Ins, TEXT(", "))));
				Body->AddSlot().AutoHeight()
				[
					Row(FText::FromString(R.Count > 1 ? FString::Printf(TEXT("%s x%d"), *Out, R.Count) : Out), Line,
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth()[Action(LOCTEXT("GiveResult", "Give it"), [this, Id]() { Send(FString::Printf(TEXT("recipe %s result %d"), *Id.ToString(), Amount)); }, true)]
						+ SHorizontalBox::Slot().AutoWidth().Padding(6.f * U, 0.f, 0.f, 0.f)[Action(LOCTEXT("GiveMats", "Materials"), [this, Id]() { Send(FString::Printf(TEXT("recipe %s materials %d"), *Id.ToString(), Amount)); })])
				];
			}
			if (More > 0) Body->AddSlot().AutoHeight()[Muted(FText::Format(LOCTEXT("MoreRecipes", "{0} more: type to narrow the list."), FText::AsNumber(More)))];
		}

		void BuildUnlock()
		{
			Body->AddSlot().AutoHeight()[Row(LOCTEXT("Blueprints", "Learn every blueprint"), LOCTEXT("BlueprintsWhat", "Every recipe a blueprint teaches is open to you for good."), Action(LOCTEXT("Learn", "Learn all"), [this]() { Send(TEXT("blueprints")); }, true))];
			Body->AddSlot().AutoHeight()[Row(LOCTEXT("BuildKit", "Builder's kit"), FText::Format(LOCTEXT("BuildKitWhat", "{0} of every material any building piece costs, into your bag."), FText::AsNumber(int32(WRTune(TEXT("god.buildKit"), 500.f)))),
				Action(LOCTEXT("GiveKit", "Give"), [this]() { Send(TEXT("buildkit")); }, true))];
			TSharedRef<SHorizontalBox> Levels = SNew(SHorizontalBox);
			for (int32 L = 1; L <= 5; ++L) Levels->AddSlot().AutoWidth().Padding(0.f, 0.f, 6.f * U, 0.f)[Action(FText::FromString(WRUIWindowKit::Roman(L)), [this, L]() { Send(FString::Printf(TEXT("hearth %d"), L)); })];
			Body->AddSlot().AutoHeight()[Row(LOCTEXT("Hearth", "Hearth tech level"), LOCTEXT("HearthWhat", "The nearest Hearth's tech level (stand by one): every station, piece and recipe of that level opens."), Levels)];
		}

		// ---------------------------------------------------------------- Progress

		void BuildProgress()
		{
			AWRPlayerState* PS = State();
			if (!PS) return;
			const UWRPathComponent* Paths = PS->GetPaths();
			Body->AddSlot().AutoHeight()[Heading(FText::Format(LOCTEXT("HPaths", "PATHS  ·  CHARACTER LEVEL {0}"), FText::AsNumber(Paths->GetCharacterLevel())))];
			Body->AddSlot().AutoHeight()[Muted(LOCTEXT("PathsWhat", "A main path and a subclass are active at a time (the game's rule). Max gives the path its 100 points and learns every ability it can, in tree order."))];
			for (const FWRPathDef& D : FWRPathRegistry::Get().All())
			{
				const FName Id = D.Id;
				const bool bMain = Paths->GetMainPath() == Id, bSub = Paths->GetSubPath() == Id;
				const FText Line = FText::Format(LOCTEXT("PathLine", "{0}{1} points earned, {2} spent"), bMain ? LOCTEXT("IsMain", "Main path  ·  ") : bSub ? LOCTEXT("IsSub", "Subclass  ·  ") : FText::GetEmpty(),
					FText::AsNumber(Paths->GetEarned(Id)), FText::AsNumber(Paths->GetSpent(Id)));
				TSharedRef<SHorizontalBox> B = SNew(SHorizontalBox);
				if (!bMain) B->AddSlot().AutoWidth().Padding(0.f, 0.f, 5.f * U, 0.f)[Action(LOCTEXT("Main", "Main"), [this, Id]() { Send(TEXT("path main ") + Id.ToString()); })];
				if (!bMain && !bSub) B->AddSlot().AutoWidth().Padding(0.f, 0.f, 5.f * U, 0.f)[Action(LOCTEXT("Sub", "Subclass"), [this, Id]() { Send(TEXT("path sub ") + Id.ToString()); })];
				if (bMain || bSub)
				{
					B->AddSlot().AutoWidth().Padding(0.f, 0.f, 5.f * U, 0.f)[Action(LOCTEXT("Plus10", "+10 points"), [this, Id]() { Send(TEXT("path points ") + Id.ToString() + TEXT(" 10")); })];
					B->AddSlot().AutoWidth().Padding(0.f, 0.f, 5.f * U, 0.f)[Action(LOCTEXT("Max", "Max"), [this, Id]() { Send(TEXT("path max ") + Id.ToString()); }, true)];
					B->AddSlot().AutoWidth()[Action(LOCTEXT("Unlearn", "Unlearn"), [this, Id]() { Send(TEXT("path unlearn ") + Id.ToString()); })];
				}
				Body->AddSlot().AutoHeight()[Row(FText::FromString(D.Name), Line, B, D.Color.A > 0.f ? D.Color : WRUI::Ink)];
			}
			Body->AddSlot().AutoHeight()[Heading(LOCTEXT("HSkills", "LIFE SKILLS"))];
			const UWRLifeSkillsComponent* Skills = PS->GetLifeSkills();
			for (const FWRLifeSkillDef& S : FWRLifeSkillRegistry::Get().All())
			{
				const FName Id = S.Id;
				const int32 Level = Skills ? Skills->GetLevel(Id) : 1;
				Body->AddSlot().AutoHeight()
				[
					Row(FText::FromString(S.Name), FText::Format(LOCTEXT("SkillLine", "Level {0} of 100  ·  {1}"), FText::AsNumber(Level), FText::FromString(S.Group)),
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 5.f * U, 0.f)[Action(LOCTEXT("Minus10", "-10"), [this, Id, Level]() { Send(FString::Printf(TEXT("skill %s %d"), *Id.ToString(), FMath::Max(1, Level - 10))); })]
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 5.f * U, 0.f)[Action(LOCTEXT("Plus10L", "+10"), [this, Id, Level]() { Send(FString::Printf(TEXT("skill %s %d"), *Id.ToString(), FMath::Min(100, Level + 10))); })]
						+ SHorizontalBox::Slot().AutoWidth()[Action(LOCTEXT("MaxSkill", "100"), [this, Id]() { Send(FString::Printf(TEXT("skill %s 100"), *Id.ToString())); }, true)])
				];
			}
		}

		// ---------------------------------------------------------------- World

		void BuildWorld()
		{
			Body->AddSlot().AutoHeight()[Heading(LOCTEXT("HTime", "TIME OF DAY"))];
			const auto Hour = [this]() { const AWRGameState* GS = PC.IsValid() ? PC->GetWorld()->GetGameState<AWRGameState>() : nullptr; return GS ? GS->GetHour() : 12.f; };
			TSharedRef<SWrapBox> Times = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(6.f * U, 6.f * U));
			const TPair<FText, float> Marks[] = { { LOCTEXT("Dawn", "Dawn"), 6.f }, { LOCTEXT("Noon", "Noon"), 12.f }, { LOCTEXT("Dusk", "Dusk"), 18.f }, { LOCTEXT("Midnight", "Midnight"), 0.f } };
			for (const TPair<FText, float>& M : Marks) { const float H = M.Value; Times->AddSlot()[Action(M.Key, [this, H]() { Send(FString::Printf(TEXT("hour %.2f"), H)); })]; }
			Times->AddSlot()[Action(LOCTEXT("MinusHour", "-1 hour"), [this, Hour]() { Send(FString::Printf(TEXT("hour %.2f"), FMath::Fmod(Hour() + 23.f, 24.f))); })];
			Times->AddSlot()[Action(LOCTEXT("PlusHour", "+1 hour"), [this, Hour]() { Send(FString::Printf(TEXT("hour %.2f"), FMath::Fmod(Hour() + 1.f, 24.f))); })];
			Body->AddSlot().AutoHeight()[Row(TAttribute<FText>::CreateLambda([Hour]() { const float H = Hour(); return FText::FromString(FString::Printf(TEXT("%02d:%02d"), int32(H), int32(FMath::Frac(H) * 60.f))); }),
				LOCTEXT("TimeWhat", "The world's clock (it jumps forward to the next time it's that hour)."), Times)];

			const auto Frozen = [this]() { const UWRGodComponent* G = God(); return G && G->bAIFrozen; };
			Body->AddSlot().AutoHeight()[Row(LOCTEXT("Freeze", "Freeze every creature"), LOCTEXT("FreezeWhat", "Every creature's and boss's mind stops where it stands, for everyone on this world (no moving, no new attacks) until it's off."),
				Clickable(Pill(Frozen), [this, Frozen]() { Send(Frozen() ? TEXT("freeze 0") : TEXT("freeze 1")); }))];

			Body->AddSlot().AutoHeight()[Heading(LOCTEXT("HWeather", "WEATHER"))];
			for (const FWRBiomeSurvival& B : FWRWeatherDefs::Get().All())
			{
				if (B.Kinds.Num() == 0 || !WRBiome::IsLive(B.Biome)) continue;
				const FName Land = B.Biome;
				TSharedRef<SWrapBox> Kinds = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(5.f * U, 5.f * U));
				for (const FWRWeatherKind& K : B.Kinds)
				{
					const FName Kind = K.Id;
					Kinds->AddSlot()[Action(FText::FromString(K.Name), [this, Land, Kind]() { Send(FString::Printf(TEXT("weather %s %s 10"), *Land.ToString(), *Kind.ToString())); })];
				}
				Kinds->AddSlot()[Action(LOCTEXT("Clear", "Clear"), [this, Land]() { Send(FString::Printf(TEXT("weather %s clear 1"), *Land.ToString())); })];
				Body->AddSlot().AutoHeight()[Row(FText::FromString(B.Name.IsEmpty() ? LandName(Land) : B.Name), LOCTEXT("WeatherWhat", "Starts now for 10 minutes."), SNew(SBox).MaxDesiredWidth(560.f * U)[Kinds])];
			}

			Body->AddSlot().AutoHeight()[Heading(LOCTEXT("HEvents", "EVENTS"))];
			if (const TSharedPtr<FJsonObject> Events = WRWorld::Obj(TEXT("events")))
			{
				for (const TPair<FString, TSharedPtr<FJsonValue>>& KV : Events->Values)
				{
					const TSharedPtr<FJsonObject> E = KV.Value.IsValid() && KV.Value->Type == EJson::Object ? KV.Value->AsObject() : nullptr;
					if (!E.IsValid() || KV.Key.StartsWith(TEXT("_"))) continue;
					FString Name, Biome;
					E->TryGetStringField(TEXT("name"), Name);
					E->TryGetStringField(TEXT("biome"), Biome);
					const FString Id = KV.Key;
					if (!Biome.IsEmpty() && !WRBiome::IsLive(FName(*Biome))) continue;
					Body->AddSlot().AutoHeight()[Row(FText::FromString(Name.IsEmpty() ? Id.Replace(TEXT("_"), TEXT(" ")) : Name), FText::FromString(Biome.IsEmpty() ? FString(TEXT("Every land")) : LandName(FName(*Biome))),
						Action(LOCTEXT("Start", "Start"), [this, Id]() { Send(TEXT("event ") + Id); }, true))];
				}
			}

			Body->AddSlot().AutoHeight()[Heading(LOCTEXT("HInvasions", "INVASIONS"))];
			if (const TSharedPtr<FJsonObject> Inv = WRWorld::Obj(TEXT("invasions")))
			{
				const TArray<TSharedPtr<FJsonValue>>* Pairs = nullptr;
				if (Inv->TryGetArrayField(TEXT("pairs"), Pairs))
				{
					for (int32 i = 0; i < Pairs->Num(); ++i)
					{
						const TSharedPtr<FJsonObject> P = (*Pairs)[i]->AsObject();
						FString From, To, Pass;
						if (!P.IsValid()) continue;
						P->TryGetStringField(TEXT("from"), From);
						P->TryGetStringField(TEXT("to"), To);
						P->TryGetStringField(TEXT("passName"), Pass);
						Body->AddSlot().AutoHeight()[Row(FText::Format(LOCTEXT("InvTitle", "{0} into {1}"), FText::FromString(LandName(FName(*From))), FText::FromString(LandName(FName(*To)))),
							FText::FromString(Pass.IsEmpty() ? FString() : TEXT("Through ") + Pass), Action(LOCTEXT("StartInv", "Start"), [this, i]() { Send(FString::Printf(TEXT("invasion %d"), i)); }, true))];
					}
				}
			}
		}

		// ---------------------------------------------------------------- Quests

		void BuildQuests()
		{
			Body->AddSlot().AutoHeight()[Row(LOCTEXT("Shrines", "Find every shrine"), LOCTEXT("ShrinesWhat", "Every shrine on this world is found by you (fast travel between them, the map)."), Action(LOCTEXT("Find", "Find all"), [this]() { Send(TEXT("shrines")); }, true))];
			const AWRPlayerState* PS = State();
			const UWRQuestLog* Log = PS ? UWRQuestLog::For(PS) : nullptr;
			const TSharedPtr<FJsonObject> Root = WRWorld::Obj(TEXT(""));
			const TSharedPtr<FJsonObject> Quests = WRWorld::Obj(TEXT("quests"));
			const TArray<TSharedPtr<FJsonValue>>* Masters = nullptr;
			if (!Root.IsValid() || !Quests.IsValid() || !Root->TryGetArrayField(TEXT("masters"), Masters)) return;
			for (const TSharedPtr<FJsonValue>& V : *Masters)
			{
				const TSharedPtr<FJsonObject> M = V->AsObject();
				FString Id, Name, Path;
				if (!M.IsValid() || !M->TryGetStringField(TEXT("id"), Id)) continue;
				M->TryGetStringField(TEXT("name"), Name);
				M->TryGetStringField(TEXT("path"), Path);
				const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
				if (!Quests->TryGetArrayField(Id, Rows) || Rows->Num() == 0) continue;
				const FWRQuestRep* Q = Log ? Log->QuestWith(FName(*Id)) : nullptr;
				const int32 At = Q ? Q->Index : 0, N = Rows->Num();
				FString StepName;
				if (At < N) { const TSharedPtr<FJsonObject> R = (*Rows)[At]->AsObject(); if (R.IsValid()) R->TryGetStringField(TEXT("name"), StepName); }
				const FText Line = At >= N ? FText::Format(LOCTEXT("QuestsDone", "All {0} done"), FText::AsNumber(N))
					: FText::Format(LOCTEXT("QuestStep", "Quest {0} of {1}: {2}"), FText::AsNumber(At + 1), FText::AsNumber(N), FText::FromString(StepName));
				Body->AddSlot().AutoHeight()
				[
					Row(FText::Format(LOCTEXT("MasterTitle", "{0}  ·  {1}"), FText::FromString(Name), FText::FromString(Path)), Line,
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 5.f * U, 0.f)[Action(LOCTEXT("Back", "Back one"), [this, Id, At]() { Send(FString::Printf(TEXT("quest %s %d"), *Id, FMath::Max(0, At - 1))); })]
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 5.f * U, 0.f)[Action(LOCTEXT("Complete", "Complete this one"), [this, Id, At, N]() { Send(FString::Printf(TEXT("quest %s %d"), *Id, FMath::Min(N, At + 1))); }, true)]
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 5.f * U, 0.f)[Action(LOCTEXT("Reset", "Reset"), [this, Id]() { Send(FString::Printf(TEXT("quest %s 0"), *Id)); })]
						+ SHorizontalBox::Slot().AutoWidth()[Action(LOCTEXT("FinishAll", "Finish all"), [this, Id, N]() { Send(FString::Printf(TEXT("quest %s %d"), *Id, N)); })])
				];
			}
		}
	};

	// ---------------------------------------------------------------- the badge

	/** The GOD tab on the left edge of the screen, over everything, while a tester has anything on. */
	class SGodBadge : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SGodBadge) {}
			SLATE_ARGUMENT(TWeakObjectPtr<APlayerController>, PC)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			PC = InArgs._PC;
			SetVisibility(TAttribute<EVisibility>::CreateLambda([this]() { const UWRGodComponent* G = UWRGodComponent::For(PC.Get()); return G && G->AnyOn() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; }));
			ChildSlot.HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(FMargin(-8.f * U, 0.f, 0.f, 0.f))
			[
				SNew(SBorder).BorderImage(WRUI::Box(Srgb(0x3A, 0x0E, 0x0B, 0.92f), 6.f * U, WRUI::Brass2, 1.5f)).Padding(FMargin(18.f * U, 8.f * U, 12.f * U, 9.f * U))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[WRUI::Text(LOCTEXT("God", "GOD"), WRUI::Title(19), Gold(), true)]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 2.f * U, 0.f, 0.f)
					[
						WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(FString::Join(WRGod::ActiveWords(UWRGodComponent::For(PC.Get())), TEXT("\n"))); }), WRUI::Caps(10), WRUI::Ink, true)
					]
				]
			];
		}

	private:
		TWeakObjectPtr<APlayerController> PC;
	};

	TMap<TWeakObjectPtr<APlayerController>, TSharedPtr<SWidget>>& Badges()
	{
		static TMap<TWeakObjectPtr<APlayerController>, TSharedPtr<SWidget>> Map;
		return Map;
	}
}

void WRGod::TickBadge(APlayerController* PC)
{
	using namespace WRUIGodPanel;
	if (!PC || !PC->IsLocalController() || !GEngine || !GEngine->GameViewport) return;
	TMap<TWeakObjectPtr<APlayerController>, TSharedPtr<SWidget>>& Map = Badges();
	for (auto It = Map.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid())
		{
			if (It->Value.IsValid()) GEngine->GameViewport->RemoveViewportWidgetContent(It->Value.ToSharedRef());
			It.RemoveCurrent();
		}
	}
	// Made once a tester is verified (never for anyone else); it shows itself while anything is on.
	if (!Map.Contains(PC) && WRGod::IsLocalTester(PC))
	{
		const TSharedRef<SWidget> Badge = SNew(SGodBadge).PC(PC);
		GEngine->GameViewport->AddViewportWidgetContent(Badge, 60);
		Map.Add(PC, Badge);
	}
}

WR_REGISTER_SCREEN("god", [](AWRPlayerController* PC) -> TSharedRef<SWidget> { return SNew(WRUIGodPanel::SGodPanel).PC(PC); });

#undef LOCTEXT_NAMESPACE
