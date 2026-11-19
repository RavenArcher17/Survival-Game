// The world systems' screens (cooks/build-world-systems.md 2.1-2.5): a lit shrine (travel), a path master (talk), a
// merchant (trade), cutting a message and reading one, and the journal's Quests tab. Each shows while the controller's
// menu is EWRMenu::World with its WorldScreen (or EWRMenu::Quests), reads replicated state and sends "world ..."
// commands; every control is a button or a slot, so the pad's input processor drives them as the mouse does.

#include "WRPlayerUI.h"
#include "WRFarm.h"
#include "WRUIStyle.h"
#include "WRUIWidgets.h"
#include "WRWorldSystems.h"
#include "WRCreature.h"
#include "WRInventory.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRSound.h"
#include "WRSurvival.h"
#include "WRTuning.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBackgroundBlur.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"

#define LOCTEXT_NAMESPACE "WRUI"

namespace WRUIWorldScreens
{
	using WRUI::U;

	TArray<TSharedPtr<FJsonValue>> ArrOf(const TSharedPtr<FJsonObject>& O, const FString& Key)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		return O.IsValid() && O->TryGetArrayField(Key, A) ? *A : TArray<TSharedPtr<FJsonValue>>();
	}
	FString StrIn(const TSharedPtr<FJsonObject>& O, const FString& Key) { FString S; if (O.IsValid()) O->TryGetStringField(Key, S); return S; }
	int32 IntIn(const TSharedPtr<FJsonObject>& O, const FString& Key, int32 Fallback = 0) { double V = Fallback; if (O.IsValid()) O->TryGetNumberField(Key, V); return int32(V); }

	FString Metres(float Cm)
	{
		const float M = Cm / 100.f;
		return M < 1000.f ? FString::Printf(TEXT("%d m"), FMath::RoundToInt(M / 5.f) * 5) : FString::Printf(TEXT("%.1f km"), M / 1000.f);
	}

	FString BiomeName(FName Biome) { const FString N = StrIn(WRWorld::Biome(Biome), TEXT("name")); return N.IsEmpty() ? Biome.ToString() : N; }

	FLinearColor BiomeTint(FName Biome) { return Biome == TEXT("tundra") ? WRUI::Frost : WRUI::Ember; }

	/** The standing tier's name for a value. */
	FString TierName(int32 Tier)
	{
		const TArray<TSharedPtr<FJsonValue>> Tiers = ArrOf(WRWorld::Obj(TEXT("factions")), TEXT("tiers"));
		return Tiers.IsValidIndex(Tier) ? StrIn(Tiers[Tier]->AsObject(), TEXT("name")) : FString();
	}

	/** The common frame: dimmed world, a riveted stone window, a leather title bar with the close key. */
	TSharedRef<SWidget> Frame(const TAttribute<FText>& Title, const TAttribute<FText>& Sub, TSharedRef<SWidget> Body, float Width)
	{
		return SNew(SOverlay)
			+ SOverlay::Slot()[SNew(SBackgroundBlur).BlurStrength(6.f)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.45f))]]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(Width * U)
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
										+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(Title, WRUI::Title(19), WRUI::Ink, true)]
										+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(Sub, WRUI::Body(12, 500), WRUI::Ink2, true)]
									]
									+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 7.f * U, 0.f)[SNew(SWRKeyCap).FontSize(11).Text(LOCTEXT("WEsc", "Esc"))]
									+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(LOCTEXT("WClose", "Close"), WRUI::Body(13, 500), WRUI::Ink2, true)]
								]
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(18.f * U, 16.f * U, 18.f * U, 0.f))[Body]
					]
				]
			];
	}

	TSharedRef<SWidget> Heading(const FText& T) { return WRUI::Text(T, WRUI::Caps(11), WRUI::Ink3); }

	/** A button that keeps its height in an auto-height slot (the leather tile would size it to its texture). */
	TSharedRef<SWidget> Button(const TAttribute<FText>& Text, TFunction<void()> OnClick, bool bPrimary, const TAttribute<bool>& Enabled = true, float Height = 40.f)
	{
		return SNew(SBox).HeightOverride(Height * U)
			[
				SNew(SWRButton).Text(Text).Primary(bPrimary).FontSize(13).IsEnabled(Enabled)
				.OnClicked_Lambda([OnClick]() { OnClick(); return FReply::Handled(); })
			];
	}

	/** Shared closing rule: the screen goes when its thing goes or you walk away. */
	void CloseIfAway(AWRPlayerController* P, const AActor* Target, FName Screen, float Range)
	{
		if (!P || P->GetMenu() != EWRMenu::World || P->WorldScreen != Screen) return;
		const APawn* Me = P->GetPawn();
		if (!Me || (Target == nullptr && Screen != TEXT("msgwrite")) || (Target && FVector::Dist(Target->GetActorLocation(), Me->GetActorLocation()) > Range)) P->SetMenu(EWRMenu::None);
	}

	// ------------------------------------------------------------------------------------------------ shrine

	class SShrine : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SShrine) {}
			SLATE_ARGUMENT(TWeakObjectPtr<AWRPlayerController>, PC)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			PC = InArgs._PC;
			Shrine = PC.IsValid() ? Cast<AWRShrine>(PC->WorldTarget.Get()) : nullptr;
			List = SNew(SVerticalBox);
			Rebuild();
			TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
			Body->AddSlot().AutoHeight()[WRUI::Text(LOCTEXT("ShrineAbout", "A kindled shrine pays path XP the first time and keeps its fire for four days; then the land's hunters take it back. Travel goes from one lit shrine to another you or your clan have found; tames following you within 30 m come too."), WRUI::Body(13), WRUI::Ink2, false, true)];
			Body->AddSlot().AutoHeight().Padding(0.f, 14.f * U, 0.f, 6.f * U)[Heading(LOCTEXT("ShrineTravel", "TRAVEL TO"))];
			Body->AddSlot().AutoHeight()[SNew(SBox).MaxDesiredHeight(420.f * U)[SNew(SScrollBox) + SScrollBox::Slot()[List.ToSharedRef()]]];
			ChildSlot
			[
				Frame(TAttribute<FText>::CreateLambda([this]() { const AWRShrine* S = Shrine.Get(); return FText::FromString(S ? S->DisplayName : FString()); }),
					TAttribute<FText>::CreateLambda([this]() { const AWRShrine* S = Shrine.Get(); return FText::FromString(S ? FString::Printf(TEXT("%s  ·  %s"), *BiomeName(S->Biome), S->bGreat ? TEXT("Great Shrine: always lit") : TEXT("lit")) : FString()); }),
					Body, 640.f)
			];
		}

		virtual void Tick(const FGeometry& Geo, const double Now, const float Dt) override
		{
			SCompoundWidget::Tick(Geo, Now, Dt);
			CloseIfAway(PC.Get(), Shrine.Get(), TEXT("shrine"), 900.f);
			const UWRQuestLog* Log = UWRQuestLog::For(PC.Get());
			const int32 Found = Log ? Log->FoundShrines.Num() : 0;
			if (Found != LastFound) { LastFound = Found; Rebuild(); }
		}
		virtual bool SupportsKeyboardFocus() const override { return true; }

	private:
		TWeakObjectPtr<AWRPlayerController> PC;
		TWeakObjectPtr<AWRShrine> Shrine;
		TSharedPtr<SVerticalBox> List;
		int32 LastFound = -1;

		void Rebuild()
		{
			List->ClearChildren();
			AWRPlayerController* P = PC.Get();
			const UWRQuestLog* Log = UWRQuestLog::For(P);
			if (!P || !Log) return;
			// By land, the Great Shrine first, then by distance.
			TArray<AWRShrine*> All;
			for (TActorIterator<AWRShrine> It(P->GetWorld()); It; ++It) All.Add(*It);
			const FVector Here = P->GetPawn() ? P->GetPawn()->GetActorLocation() : FVector::ZeroVector;
			All.Sort([&Here](const AWRShrine& A, const AWRShrine& B)
			{
				if (A.Biome != B.Biome) return A.Biome.LexicalLess(B.Biome);
				if (A.bGreat != B.bGreat) return A.bGreat;
				return FVector::Dist2D(A.GetActorLocation(), Here) < FVector::Dist2D(B.GetActorLocation(), Here);
			});
			FName Last;
			int32 Unfound = 0;
			for (AWRShrine* S : All)
			{
				if (!Log->HasFound(S->ShrineId)) { ++Unfound; continue; }
				if (S->Biome != Last)
				{
					Last = S->Biome;
					List->AddSlot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 3.f * U)[WRUI::Text(FText::FromString(BiomeName(S->Biome).ToUpper()), WRUI::Caps(12), BiomeTint(S->Biome))];
				}
				const TWeakObjectPtr<AWRShrine> W = S;
				List->AddSlot().AutoHeight().Padding(0.f, 2.f * U)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(FText::FromString(S->DisplayName), WRUI::Body(15, 700), WRUI::Ink)]
						+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda([this, W]() { return FText::FromString(Line(W.Get())); }), WRUI::Body(12, 500), WRUI::Ink2)]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SBox).WidthOverride(130.f * U)
						[
							Button(TAttribute<FText>::CreateLambda([this, W]() { const FString Why = UWRWorldSystems::TravelBlock(PC.Get(), W.Get()); return Why.IsEmpty() ? LOCTEXT("Travel", "Travel") : FText::FromString(Why); }),
								[this, W]() { if (PC.IsValid() && W.IsValid() && UWRWorldSystems::TravelBlock(PC.Get(), W.Get()).IsEmpty()) { PC->SendCommand(FString::Printf(TEXT("world travel %s"), *W->ShrineId.ToString())); PC->SetMenu(EWRMenu::None); } },
								true, TAttribute<bool>::CreateLambda([this, W]() { return UWRWorldSystems::TravelBlock(PC.Get(), W.Get()).IsEmpty(); }), 38.f)
						]
					]
				];
			}
			if (Unfound > 0)
			{
				List->AddSlot().AutoHeight().Padding(0.f, 10.f * U, 0.f, 0.f)[WRUI::Text(FText::FromString(FString::Printf(TEXT("%d %s not found yet: walk to them and kindle their fires."), Unfound, Unfound == 1 ? TEXT("shrine") : TEXT("shrines"))), WRUI::Body(13, 500), WRUI::Ink3, false, true)];
			}
		}

		FString Line(const AWRShrine* S) const
		{
			const APawn* Me = PC.IsValid() ? PC->GetPawn() : nullptr;
			if (!S || !Me) return FString();
			const FString State = S->bHeld ? TEXT("held by invaders") : S->bLit ? TEXT("lit") : S->bGuarded ? TEXT("guarded") : TEXT("dark");
			return FString::Printf(TEXT("%s  ·  %s"), *Metres(FVector::Dist2D(S->GetActorLocation(), Me->GetActorLocation())), *State);
		}
	};

	// ------------------------------------------------------------------------------------------------ talk

	class STalk : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(STalk) {}
			SLATE_ARGUMENT(TWeakObjectPtr<AWRPlayerController>, PC)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			PC = InArgs._PC;
			Npc = PC.IsValid() ? Cast<AWRWorldNpc>(PC->WorldTarget.Get()) : nullptr;
			TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
			Body->AddSlot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(FString::Printf(TEXT("\"%s\""), *Said())); }), WRUI::Body(17, 500), WRUI::Ink, false, true)];
			Body->AddSlot().AutoHeight().Padding(0.f, 16.f * U, 0.f, 4.f * U)[Heading(LOCTEXT("TalkTask", "THE TASK"))];
			Body->AddSlot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(Field(TEXT("title"))); }), WRUI::Title(17), WRUI::Brass)];
			Body->AddSlot().AutoHeight().Padding(0.f, 4.f * U, 0.f, 0.f)[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(Field(TEXT("task"))); }), WRUI::Body(14), WRUI::Ink2, false, true)];
			Body->AddSlot().AutoHeight().Padding(0.f, 8.f * U, 0.f, 0.f)[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(Progress()); }), WRUI::Body(13, 700), TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(IsDone() ? WRUI::Good : WRUI::Ink2); }))];
			Body->AddSlot().AutoHeight().Padding(0.f, 4.f * U, 0.f, 0.f)[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(Pays()); }), WRUI::Body(13), WRUI::Ink2, false, true)];
			Body->AddSlot().AutoHeight().Padding(0.f, 12.f * U, 0.f, 0.f)[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(StandingLine()); }), WRUI::Body(12, 500), WRUI::Ink3)];
			Body->AddSlot().AutoHeight().Padding(0.f, 14.f * U, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 8.f * U, 0.f)
				[
					Button(TAttribute<FText>::CreateLambda([this]() { const FWRQuestRep* Q = Quest(); return Q && Q->bTaken ? LOCTEXT("HandIn", "Hand it in") : LOCTEXT("TakeIt", "Take it"); }),
						[this]() { const FWRQuestRep* Q = Quest(); Send(Q && Q->bTaken ? TEXT("hand") : TEXT("take")); }, true,
						TAttribute<bool>::CreateLambda([this]() { return Row().IsValid(); }))
				]
				+ SHorizontalBox::Slot().FillWidth(1.f)[Button(LOCTEXT("Leave", "Leave"), [this]() { if (PC.IsValid()) PC->SetMenu(EWRMenu::None); }, false)]
			];
			ChildSlot
			[
				Frame(TAttribute<FText>::CreateLambda([this]() { const AWRWorldNpc* N = Npc.Get(); return FText::FromString(N ? N->NpcName : FString()); }),
					TAttribute<FText>::CreateLambda([this]() { const AWRWorldNpc* N = Npc.Get(); return FText::FromString(N ? N->GetDisplayLabel() : FString()); }),
					Body, 600.f)
			];
		}

		virtual void Tick(const FGeometry& Geo, const double Now, const float Dt) override
		{
			SCompoundWidget::Tick(Geo, Now, Dt);
			CloseIfAway(PC.Get(), Npc.Get(), TEXT("talk"), 900.f);
		}
		virtual bool SupportsKeyboardFocus() const override { return true; }

	private:
		TWeakObjectPtr<AWRPlayerController> PC;
		TWeakObjectPtr<AWRWorldNpc> Npc;

		const FWRQuestRep* Quest() const
		{
			const UWRQuestLog* Log = UWRQuestLog::For(PC.Get());
			return Log && Npc.IsValid() ? Log->QuestWith(Npc->NpcId) : nullptr;
		}
		TSharedPtr<FJsonObject> Row() const
		{
			if (!Npc.IsValid()) return nullptr;
			const TArray<TSharedPtr<FJsonValue>> List = ArrOf(WRWorld::Obj(TEXT("quests")), Npc->NpcId.ToString());
			const FWRQuestRep* Q = Quest();
			const int32 I = Q ? Q->Index : 0;
			return List.IsValidIndex(I) ? List[I]->AsObject() : nullptr;
		}
		FString Field(const TCHAR* Key) const
		{
			const TSharedPtr<FJsonObject> R = Row();
			return R.IsValid() ? StrIn(R, Key) : (FString(Key) == TEXT("title") ? FString(TEXT("Nothing left to learn")) : FString(TEXT("You've learned all this master teaches.")));
		}
		bool IsDone() const
		{
			const FWRQuestRep* Q = Quest();
			const TSharedPtr<FJsonObject> R = Row();
			return Q && Q->bTaken && R.IsValid() && Q->Progress >= IntIn(R, TEXT("count"), 1);
		}
		FString Progress() const
		{
			const FWRQuestRep* Q = Quest();
			const TSharedPtr<FJsonObject> R = Row();
			if (!R.IsValid()) return FString();
			if (!Q || !Q->bTaken) return TEXT("Not taken yet");
			const FString Type = StrIn(R, TEXT("type"));
			const int32 Need = IntIn(R, TEXT("count"), 1);
			if (Type == TEXT("bring") || Type == TEXT("show"))
			{
				const AWRPlayerState* PS = PC.IsValid() ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
				const int32 Have = PS ? PS->GetInventory()->CountOf(FName(*StrIn(R, TEXT("item")))) : 0;
				return FString::Printf(TEXT("In your bag: %d / %d %s"), FMath::Min(Have, Need), Need, *WRWorld::ItemName(FName(*StrIn(R, TEXT("item")))));
			}
			if (Type == TEXT("sneak") && Q->Stage == 1 && Q->Progress < Need) return TEXT("Close enough: now back away without waking them");
			return Q->Progress >= Need ? FString(TEXT("Done: hand it in")) : FString::Printf(TEXT("%d / %d"), Q->Progress, Need);
		}
		FString Pays() const
		{
			const TSharedPtr<FJsonObject> R = Row();
			if (!R.IsValid() || !Npc.IsValid()) return FString();
			TArray<FString> P;
			const FString Path = StrIn(WRWorld::Biome(Npc->Biome), TEXT("path"));
			if (IntIn(R, TEXT("xp")) > 0) P.Add(FString::Printf(TEXT("%d %s XP"), IntIn(R, TEXT("xp")), *Path));
			for (const auto& V : ArrOf(R, TEXT("recipes"))) P.Add(FString::Printf(TEXT("the %s recipe"), *WRWorld::ItemName(FName(*V->AsString()))));
			const TSharedPtr<FJsonObject>* Items = nullptr;
			if (R->TryGetObjectField(TEXT("items"), Items)) for (const auto& KV : (*Items)->Values) P.Add(FString::Printf(TEXT("%s x%d"), *WRWorld::ItemName(FName(*KV.Key)), int32(KV.Value->AsNumber())));
			return P.Num() ? TEXT("Pays: ") + FString::Join(P, TEXT(", ")) : FString();
		}
		FString StandingLine() const
		{
			const UWRQuestLog* Log = UWRQuestLog::For(PC.Get());
			if (!Log || !Npc.IsValid()) return FString();
			const FName Faction(*StrIn(WRWorld::Biome(Npc->Biome), TEXT("faction")));
			const FString Name = StrIn(WRWorld::Obj(TEXT("factions.") + Faction.ToString()), TEXT("name"));
			return FString::Printf(TEXT("%s: %s (%d)"), *Name, *TierName(Log->TierOf(Faction)), Log->StandingOf(Faction));
		}
		FString Said() const
		{
			const FWRQuestRep* Q = Quest();
			const TSharedPtr<FJsonObject> M = [this]() -> TSharedPtr<FJsonObject>
			{
				for (const auto& V : ArrOf(WRWorld::Data(), TEXT("masters"))) if (Npc.IsValid() && StrIn(V->AsObject(), TEXT("id")) == Npc->NpcId.ToString()) return V->AsObject();
				return nullptr;
			}();
			const TSharedPtr<FJsonObject>* Lines = nullptr;
			if (!M.IsValid() || !M->TryGetObjectField(TEXT("lines"), Lines)) return FString();
			const TCHAR* Key = !Q ? TEXT("first") : !Row().IsValid() ? TEXT("all") : !Q->bTaken ? TEXT("offer") : IsDone() ? TEXT("done") : TEXT("waiting");
			return StrIn(*Lines, Key);
		}
		void Send(const TCHAR* Verb)
		{
			if (!PC.IsValid() || !Npc.IsValid()) return;
			PC->SendCommand(FString::Printf(TEXT("world %s %s"), Verb, *Npc->NpcId.ToString()));
			WRSound::PlayUI(PC.Get(), TEXT("ui.click"));
		}
	};

	// ------------------------------------------------------------------------------------------------ trade

	class STrade : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(STrade) {}
			SLATE_ARGUMENT(TWeakObjectPtr<AWRPlayerController>, PC)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			PC = InArgs._PC;
			Npc = PC.IsValid() ? Cast<AWRWorldNpc>(PC->WorldTarget.Get()) : nullptr;
			FString Table;
			for (const auto& V : ArrOf(WRWorld::Data(), TEXT("caravans"))) if (Npc.IsValid() && StrIn(V->AsObject(), TEXT("id")) == Npc->NpcId.ToString()) Table = StrIn(V->AsObject(), TEXT("trade"));
			if (Npc.IsValid() && !Npc->TradeTable.IsNone()) Table = Npc->TradeTable.ToString();
			Offers = ArrOf(WRWorld::Obj(TEXT("trade")), Table);
			TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
			for (int32 i = 0; i < Offers.Num(); ++i)
			{
				const TSharedPtr<FJsonObject> O = Offers[i]->AsObject();
				TSharedRef<SHorizontalBox> Line = SNew(SHorizontalBox);
				const auto Slots = [this, &Line](const TSharedPtr<FJsonObject>& Items, bool bGive)
				{
					if (!Items.IsValid()) return;
					for (const auto& KV : Items->Values)
					{
						const FName Id(*KV.Key);
						const int32 Base = int32(KV.Value->AsNumber());
						Line->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 4.f * U, 0.f)
						[
							SNew(SWRSlot).Size(44.f * U).View_Lambda([this, Id, Base, bGive]() { FWRSlotView V; V.ItemId = Id; V.Count = bGive ? Cost(Base) : Base; V.bShowCount = true; V.bDimmed = bGive && Have(Id) < Cost(Base); return V; })
						];
					}
				};
				const TSharedPtr<FJsonObject>* Give = nullptr;
				const TSharedPtr<FJsonObject>* Get = nullptr;
				O->TryGetObjectField(TEXT("give"), Give);
				O->TryGetObjectField(TEXT("get"), Get);
				Slots(Give ? *Give : nullptr, true);
				Line->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(6.f * U, 0.f)[WRUI::Text(LOCTEXT("TradeFor", "for"), WRUI::Body(13, 500), WRUI::Ink3)];
				Slots(Get ? *Get : nullptr, false);
				Line->AddSlot().FillWidth(1.f).VAlign(VAlign_Center).Padding(8.f * U, 0.f)[WRUI::Text(TAttribute<FText>::CreateLambda([this, i]() { return FText::FromString(Lock(i)); }), WRUI::Body(12, 500), WRUI::Warn, false, true)];
				Line->AddSlot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(110.f * U)[Button(LOCTEXT("TradeBtn", "Trade"), [this, i]() { if (PC.IsValid() && Npc.IsValid()) { PC->SendCommand(FString::Printf(TEXT("world trade %s %d"), *Npc->NpcId.ToString(), i)); WRSound::PlayUI(PC.Get(), TEXT("ui.click")); } }, true,
						TAttribute<bool>::CreateLambda([this, i]() { return Lock(i).IsEmpty() && CanPay(i); }), 38.f)]
				];
				Rows->AddSlot().AutoHeight().Padding(0.f, 3.f * U)[Line];
			}
			TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
			Body->AddSlot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(StandingLine()); }), WRUI::Body(13, 600), WRUI::Ink2, false, true)];
			Body->AddSlot().AutoHeight().Padding(0.f, 10.f * U, 0.f, 4.f * U)[Heading(LOCTEXT("TradeOffers", "WHAT THEY TRADE  ·  YOUR GOODS ON THE LEFT"))];
			Body->AddSlot().AutoHeight()[SNew(SBox).MaxDesiredHeight(460.f * U)[SNew(SScrollBox) + SScrollBox::Slot()[Rows]]];
			ChildSlot
			[
				Frame(TAttribute<FText>::CreateLambda([this]() { const AWRWorldNpc* N = Npc.Get(); return FText::FromString(N ? N->NpcName : FString()); }),
					TAttribute<FText>::CreateLambda([this]() { const AWRWorldNpc* N = Npc.Get(); return FText::FromString(!N ? FString() : N->NpcRole == TEXT("seller") ? FString(TEXT("Market stall  ·  Hearthmoot")) : FString::Printf(TEXT("Traveling merchant  ·  %s"), *BiomeName(N->Biome))); }),
					Body, 700.f)
			];
		}

		virtual void Tick(const FGeometry& Geo, const double Now, const float Dt) override
		{
			SCompoundWidget::Tick(Geo, Now, Dt);
			AWRPlayerController* P = PC.Get();
			CloseIfAway(P, Npc.Get(), TEXT("trade"), 900.f);
			if (P && P->GetMenu() == EWRMenu::World && P->WorldScreen == TEXT("trade") && Npc.IsValid() && !Npc->bTrading) P->SetMenu(EWRMenu::None);
		}
		virtual bool SupportsKeyboardFocus() const override { return true; }

	private:
		TWeakObjectPtr<AWRPlayerController> PC;
		TWeakObjectPtr<AWRWorldNpc> Npc;
		TArray<TSharedPtr<FJsonValue>> Offers;

		FName Faction() const { return Npc.IsValid() ? FName(*StrIn(WRWorld::Biome(Npc->Biome), TEXT("faction"))) : NAME_None; }
		int32 Tier() const { const UWRQuestLog* Log = UWRQuestLog::For(PC.Get()); return Log ? Log->TierOf(Faction()) : 0; }
		float Discount() const
		{
			const TArray<TSharedPtr<FJsonValue>> Tiers = ArrOf(WRWorld::Obj(TEXT("factions")), TEXT("tiers"));
			double D = 0.0;
			if (Tiers.IsValidIndex(Tier())) Tiers[Tier()]->AsObject()->TryGetNumberField(TEXT("discount"), D);
			return float(D);
		}
		int32 Cost(int32 Base) const { return FMath::Max(1, FMath::CeilToInt(Base * (1.f - Discount()))); }
		int32 Have(FName Id) const { const AWRPlayerState* PS = PC.IsValid() ? PC->GetPlayerState<AWRPlayerState>() : nullptr; return PS ? PS->GetInventory()->CountOf(Id) : 0; }
		FString Lock(int32 i) const
		{
			const int32 Need = Offers.IsValidIndex(i) ? IntIn(Offers[i]->AsObject(), TEXT("tier")) : 0;
			return Tier() >= Need ? FString() : FString::Printf(TEXT("Needs %s"), *TierName(Need));
		}
		bool CanPay(int32 i) const
		{
			const TSharedPtr<FJsonObject>* Give = nullptr;
			if (!Offers.IsValidIndex(i) || !Offers[i]->AsObject()->TryGetObjectField(TEXT("give"), Give)) return false;
			for (const auto& KV : (*Give)->Values) if (Have(FName(*KV.Key)) < Cost(int32(KV.Value->AsNumber()))) return false;
			return true;
		}
		FString StandingLine() const
		{
			const UWRQuestLog* Log = UWRQuestLog::For(PC.Get());
			const FString Name = StrIn(WRWorld::Obj(TEXT("factions.") + Faction().ToString()), TEXT("name"));
			// Hearthmoot's market and inn belong to no faction: anyone trades there at the same price.
			if (Name.IsEmpty()) return TEXT("Hearthmoot's market: anyone may trade here, at the same price for all.");
			const float D = Discount();
			return FString::Printf(TEXT("%s count you %s (%d).%s Quests, kindled shrines, trade and defending their land raise it."), *Name, *TierName(Tier()), Log ? Log->StandingOf(Faction()) : 0,
				D > 0.f ? *FString::Printf(TEXT(" You pay %d%% less."), FMath::RoundToInt(D * 100.f)) : TEXT(""));
		}
	};

	// ------------------------------------------------------------------------------------------------ messages

	class SMessageWrite : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SMessageWrite) {}
			SLATE_ARGUMENT(TWeakObjectPtr<AWRPlayerController>, PC)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			PC = InArgs._PC;
			const TSharedPtr<FJsonObject> M = WRWorld::Obj(TEXT("messages"));
			Templates = ArrOf(M, TEXT("templates"));
			Conjunctions = ArrOf(M, TEXT("conjunctions"));
			const TSharedPtr<FJsonObject>* W = nullptr;
			if (M.IsValid() && M->TryGetObjectField(TEXT("words"), W)) { Words = *W; for (const auto& KV : Words->Values) Categories.Add(FString(KV.Key)); }
			TemplateBox = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4.f * U));
			CategoryBox = SNew(SHorizontalBox);
			WordBox = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4.f * U));
			ConjBox = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4.f * U));
			RebuildAll();
			TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
			Body->AddSlot().AutoHeight()
			[
				SNew(SWRPanel).Kind(EWRPanel::Inset).Padding(FMargin(14.f * U, 10.f * U))
				[
					WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(Preview()); }), WRUI::Title(20), WRUI::Ink, false, true)
				]
			];
			Body->AddSlot().AutoHeight().Padding(0.f, 10.f * U, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 6.f * U, 0.f)[Button(LOCTEXT("MsgPart1", "First part"), [this]() { Part = 0; RebuildAll(); }, false, true, 34.f)]
				+ SHorizontalBox::Slot().FillWidth(1.f)[Button(LOCTEXT("MsgPart2", "Second part"), [this]() { if (Pick[3] >= 0) { Part = 1; RebuildAll(); } }, false, TAttribute<bool>::CreateLambda([this]() { return Pick[3] >= 0; }), 34.f)]
			];
			Body->AddSlot().AutoHeight().Padding(0.f, 10.f * U, 0.f, 4.f * U)[Heading(LOCTEXT("MsgTemplate", "HOW IT GOES"))];
			Body->AddSlot().AutoHeight()[SNew(SBox).MaxDesiredHeight(130.f * U)[SNew(SScrollBox) + SScrollBox::Slot()[TemplateBox.ToSharedRef()]]];
			Body->AddSlot().AutoHeight().Padding(0.f, 10.f * U, 0.f, 4.f * U)[Heading(LOCTEXT("MsgWord", "THE WORD"))];
			Body->AddSlot().AutoHeight()[CategoryBox.ToSharedRef()];
			Body->AddSlot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 0.f)[SNew(SBox).MaxDesiredHeight(170.f * U)[SNew(SScrollBox) + SScrollBox::Slot()[WordBox.ToSharedRef()]]];
			Body->AddSlot().AutoHeight().Padding(0.f, 10.f * U, 0.f, 4.f * U)[Heading(LOCTEXT("MsgConj", "AND THEN? (OPTIONAL)"))];
			Body->AddSlot().AutoHeight()[ConjBox.ToSharedRef()];
			Body->AddSlot().AutoHeight().Padding(0.f, 14.f * U, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 8.f * U, 0.f)[Button(LOCTEXT("MsgCut", "Cut it in the stone"), [this]() { Cut(); }, true, TAttribute<bool>::CreateLambda([this]() { return Pick[0] >= 0 && Pick[2] >= 0 && (Pick[3] < 0 || (Pick[4] >= 0 && Pick[6] >= 0)); }))]
				+ SHorizontalBox::Slot().FillWidth(1.f)[Button(LOCTEXT("MsgCancel", "Leave it"), [this]() { if (PC.IsValid()) PC->SetMenu(EWRMenu::None); }, false)]
			];
			ChildSlot[Frame(LOCTEXT("MsgTitle", "Cut a message"), LOCTEXT("MsgSub", "Whoever comes this way reads it; a Good from them gives you back some health"), Body, 760.f)];
		}

		virtual void Tick(const FGeometry& Geo, const double Now, const float Dt) override
		{
			SCompoundWidget::Tick(Geo, Now, Dt);
			CloseIfAway(PC.Get(), nullptr, TEXT("msgwrite"), 0.f);
		}
		virtual bool SupportsKeyboardFocus() const override { return true; }

	private:
		TWeakObjectPtr<AWRPlayerController> PC;
		TArray<TSharedPtr<FJsonValue>> Templates, Conjunctions;
		TSharedPtr<FJsonObject> Words;
		TArray<FString> Categories;
		TSharedPtr<SWrapBox> TemplateBox, WordBox, ConjBox;
		TSharedPtr<SHorizontalBox> CategoryBox;
		// template, category, word, conjunction, template 2, category 2, word 2
		int32 Pick[7] = { -1, 0, -1, -1, -1, 0, -1 };
		int32 Part = 0;

		int32& T() { return Pick[Part == 0 ? 0 : 4]; }
		int32& C() { return Pick[Part == 0 ? 1 : 5]; }
		int32& W() { return Pick[Part == 0 ? 2 : 6]; }

		TArray<TSharedPtr<FJsonValue>> WordsIn(int32 Cat) const
		{
			return Categories.IsValidIndex(Cat) ? ArrOf(Words, Categories[Cat]) : TArray<TSharedPtr<FJsonValue>>();
		}

		FString Clause(int32 Ti, int32 Ci, int32 Wi) const
		{
			const FString Tpl = Templates.IsValidIndex(Ti) ? Templates[Ti]->AsString() : FString(TEXT("****"));
			const TArray<TSharedPtr<FJsonValue>> L = WordsIn(Ci);
			return Tpl.Replace(TEXT("****"), L.IsValidIndex(Wi) ? *L[Wi]->AsString() : TEXT("..."));
		}

		FString Preview() const
		{
			FString S = Clause(Pick[0], Pick[1], Pick[2]);
			if (Pick[3] >= 0 && Conjunctions.IsValidIndex(Pick[3]))
			{
				const FString Cj = Conjunctions[Pick[3]]->AsString();
				const FString Second = Clause(Pick[4], Pick[5], Pick[6]);
				S = Cj == TEXT(",") ? S + TEXT(", ") + Second : S + TEXT(" ") + Cj + TEXT(" ") + Second;
			}
			return S;
		}

		TSharedRef<SWidget> Chip(const FString& Label, TFunction<bool()> IsOn, TFunction<void()> OnPick)
		{
			return SNew(SBox).HeightOverride(32.f * U)
				[
					SNew(SWRButton).FontSize(12).Primary(false)
					.Text_Lambda([Label, IsOn]() { return FText::FromString(IsOn() ? FString::Printf(TEXT("» %s «"), *Label) : Label); })
					.OnClicked_Lambda([this, OnPick]() { OnPick(); WRSound::PlayUI(PC.Get(), TEXT("ui.click")); return FReply::Handled(); })
				];
		}

		void RebuildAll()
		{
			TemplateBox->ClearChildren();
			for (int32 i = 0; i < Templates.Num(); ++i)
			{
				TemplateBox->AddSlot()[Chip(Templates[i]->AsString(), [this, i]() { return Pick[Part == 0 ? 0 : 4] == i; }, [this, i]() { T() = i; })];
			}
			CategoryBox->ClearChildren();
			for (int32 c = 0; c < Categories.Num(); ++c)
			{
				CategoryBox->AddSlot().FillWidth(1.f).Padding(0.f, 0.f, 4.f * U, 0.f)[Chip(Categories[c], [this, c]() { return Pick[Part == 0 ? 1 : 5] == c; }, [this, c]() { C() = c; W() = -1; RebuildWords(); })];
			}
			RebuildWords();
			ConjBox->ClearChildren();
			ConjBox->AddSlot()[Chip(TEXT("(none)"), [this]() { return Pick[3] < 0; }, [this]() { Pick[3] = -1; Part = 0; RebuildAll(); })];
			for (int32 j = 0; j < Conjunctions.Num(); ++j)
			{
				const FString Cj = Conjunctions[j]->AsString();
				ConjBox->AddSlot()[Chip(Cj == TEXT(",") ? FString(TEXT("( , )")) : Cj, [this, j]() { return Pick[3] == j; }, [this, j]() { const bool bWas = Pick[3] >= 0; Pick[3] = j; if (!bWas) { Part = 1; RebuildAll(); } })];
			}
		}

		void RebuildWords()
		{
			WordBox->ClearChildren();
			const TArray<TSharedPtr<FJsonValue>> L = WordsIn(C());
			for (int32 i = 0; i < L.Num(); ++i)
			{
				WordBox->AddSlot()[Chip(L[i]->AsString(), [this, i]() { return Pick[Part == 0 ? 2 : 6] == i; }, [this, i]() { W() = i; })];
			}
		}

		void Cut()
		{
			if (!PC.IsValid()) return;
			PC->SendCommand(FString::Printf(TEXT("world msg %d %d %d %d %d %d %d"), Pick[0], Pick[1], Pick[2], Pick[3], Pick[4], Pick[5], Pick[6]));
			PC->SetMenu(EWRMenu::None);
		}
	};

	class SMessageRead : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SMessageRead) {}
			SLATE_ARGUMENT(TWeakObjectPtr<AWRPlayerController>, PC)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			PC = InArgs._PC;
			Stone = PC.IsValid() ? Cast<AWRMessageStone>(PC->WorldTarget.Get()) : nullptr;
			WRSound::PlayUI(PC.Get(), TEXT("world.carve"));
			TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
			Body->AddSlot().AutoHeight()
			[
				SNew(SWRPanel).Kind(EWRPanel::Inset).Padding(FMargin(18.f * U, 14.f * U))
				[
					WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(Stone.IsValid() ? Stone->Text : FString()); }), WRUI::Title(22), WRUI::Ink, false, true)
				]
			];
			Body->AddSlot().AutoHeight().Padding(0.f, 8.f * U, 0.f, 0.f)[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(Stone.IsValid() ? FString::Printf(TEXT("Cut by %s  ·  Good %d  ·  Poor %d"), *Stone->Author, Stone->Good, Stone->Poor) : FString()); }), WRUI::Body(13, 500), WRUI::Ink2)];
			Body->AddSlot().AutoHeight().Padding(0.f, 14.f * U, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 6.f * U, 0.f)[Button(LOCTEXT("MsgGood", "Good"), [this]() { Rate(TEXT("good")); }, true, TAttribute<bool>::CreateLambda([this]() { return CanRate(); }))]
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 6.f * U, 0.f)[Button(LOCTEXT("MsgPoor", "Poor"), [this]() { Rate(TEXT("poor")); }, false, TAttribute<bool>::CreateLambda([this]() { return CanRate(); }))]
				+ SHorizontalBox::Slot().FillWidth(1.f)[Button(LOCTEXT("MsgClose", "Close"), [this]() { if (PC.IsValid()) PC->SetMenu(EWRMenu::None); }, false)]
			];
			ChildSlot[Frame(LOCTEXT("MsgReadTitle", "A message in the stone"), FText::GetEmpty(), Body, 560.f)];
		}

		virtual void Tick(const FGeometry& Geo, const double Now, const float Dt) override
		{
			SCompoundWidget::Tick(Geo, Now, Dt);
			CloseIfAway(PC.Get(), Stone.Get(), TEXT("msgread"), 700.f);
		}
		virtual bool SupportsKeyboardFocus() const override { return true; }

	private:
		TWeakObjectPtr<AWRPlayerController> PC;
		TWeakObjectPtr<AWRMessageStone> Stone;
		bool bRated = false;

		bool CanRate() const
		{
			const APlayerState* PS = PC.IsValid() ? PC->PlayerState : nullptr;
			return !bRated && Stone.IsValid() && PS && Stone->Author != PS->GetPlayerName();
		}
		void Rate(const TCHAR* How)
		{
			if (!PC.IsValid() || !Stone.IsValid()) return;
			const FVector L = Stone->GetActorLocation();
			PC->SendCommand(FString::Printf(TEXT("world rate %.1f %.1f %.1f %s"), L.X, L.Y, L.Z, How));
			bRated = true;
		}
	};

	// ------------------------------------------------------------------------------------------------ quests tab

	class SQuestsTab : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SQuestsTab) {}
			SLATE_ARGUMENT(TWeakObjectPtr<AWRPlayerController>, PC)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			PC = InArgs._PC;
			Box = SNew(SVerticalBox);
			Rebuild();
			ChildSlot[SNew(SScrollBox) + SScrollBox::Slot().Padding(FMargin(24.f * U, 16.f * U))[Box.ToSharedRef()]];
		}

		virtual void Tick(const FGeometry& Geo, const double Now, const float Dt) override
		{
			SCompoundWidget::Tick(Geo, Now, Dt);
			// Cheap fingerprint of what's shown: rebuild only when it changes.
			const UWRQuestLog* Log = UWRQuestLog::For(PC.Get());
			int32 Key = Log ? Log->FoundShrines.Num() * 1000 + Log->LitByMe.Num() * 100 : 0;
			if (Log) for (const FWRQuestRep& Q : Log->Quests) Key += Q.Index * 31 + Q.Progress * 7 + Q.bTaken + Q.Stage * 3;
			if (Log) for (const FWRStanding& S : Log->Standing) Key += S.Value;
			if (Key != LastKey) { LastKey = Key; Rebuild(); }
		}

	private:
		TWeakObjectPtr<AWRPlayerController> PC;
		TSharedPtr<SVerticalBox> Box;
		int32 LastKey = -1;

		void Rebuild()
		{
			Box->ClearChildren();
			const UWRQuestLog* Log = UWRQuestLog::For(PC.Get());
			Box->AddSlot().AutoHeight()[WRUI::Text(LOCTEXT("QuestsMasters", "THE PATH MASTERS"), WRUI::Caps(13), WRUI::Ink3)];
			for (const auto& V : ArrOf(WRWorld::Data(), TEXT("masters")))
			{
				const TSharedPtr<FJsonObject> M = V->AsObject();
				const FName Id(*StrIn(M, TEXT("id")));
				FString Village;
				for (const auto& Vg : ArrOf(WRWorld::Data(), TEXT("villages"))) if (StrIn(Vg->AsObject(), TEXT("id")) == StrIn(M, TEXT("village"))) Village = StrIn(Vg->AsObject(), TEXT("name"));
				const TArray<TSharedPtr<FJsonValue>> List = ArrOf(WRWorld::Obj(TEXT("quests")), Id.ToString());
				const FWRQuestRep* Q = Log ? Log->QuestWith(Id) : nullptr;
				const int32 Index = Q ? Q->Index : 0;
				const TSharedPtr<FJsonObject> Row = List.IsValidIndex(Index) ? List[Index]->AsObject() : nullptr;
				FString State;
				if (!Q) State = FString::Printf(TEXT("Not met yet. Find %s at the Great Shrine in %s."), *StrIn(M, TEXT("name")), *Village);
				else if (!Row.IsValid()) State = TEXT("Everything they teach, learned.");
				else if (!Q->bTaken) State = FString::Printf(TEXT("Next: %s. Go back to %s to take it."), *StrIn(Row, TEXT("title")), *Village);
				else
				{
					const int32 Need = IntIn(Row, TEXT("count"), 1);
					State = Q->Progress >= Need ? FString::Printf(TEXT("%s: done. Hand it in at %s."), *StrIn(Row, TEXT("title")), *Village)
						: FString::Printf(TEXT("%s: %s  (%d / %d)"), *StrIn(Row, TEXT("title")), *StrIn(Row, TEXT("task")), Q->Progress, Need);
				}
				const FString Path = StrIn(M, TEXT("path"));
				Box->AddSlot().AutoHeight().Padding(0.f, 8.f * U, 0.f, 0.f)
				[
					SNew(SWRPanel).Kind(EWRPanel::Inset).Padding(FMargin(14.f * U, 10.f * U))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.f)[WRUI::Text(FText::FromString(StrIn(M, TEXT("name"))), WRUI::Title(17), Path == TEXT("frost") ? WRUI::Frost : WRUI::Ember)]
							+ SHorizontalBox::Slot().AutoWidth()[WRUI::Text(FText::FromString(FString::Printf(TEXT("%s path  ·  %s  ·  %d / %d"), *Path, *Village, FMath::Min(Index, List.Num()), List.Num())), WRUI::Body(12, 500), WRUI::Ink3)]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f * U, 0.f, 0.f)[WRUI::Text(FText::FromString(State), WRUI::Body(14), WRUI::Ink, false, true)]
					]
				];
			}
			// Standing.
			Box->AddSlot().AutoHeight().Padding(0.f, 18.f * U, 0.f, 0.f)[WRUI::Text(LOCTEXT("QuestsStanding", "STANDING"), WRUI::Caps(13), WRUI::Ink3)];
			const TSharedPtr<FJsonObject> Factions = WRWorld::Obj(TEXT("factions"));
			if (Factions.IsValid())
			{
				const TArray<TSharedPtr<FJsonValue>> Tiers = ArrOf(Factions, TEXT("tiers"));
				for (const auto& KV : Factions->Values)
				{
					const TSharedPtr<FJsonObject> F = KV.Value->AsObject();
					if (!F.IsValid() || !F->HasField(TEXT("name")) || !WRBiome::IsContentLive(FName(*StrIn(F, TEXT("biome"))))) continue;
					const FName Id(*KV.Key);
					const int32 V = Log ? Log->StandingOf(Id) : 0;
					const int32 Tier = Log ? Log->TierOf(Id) : 0;
					const int32 From = Tiers.IsValidIndex(Tier) ? IntIn(Tiers[Tier]->AsObject(), TEXT("at")) : 0;
					const int32 To = Tiers.IsValidIndex(Tier + 1) ? IntIn(Tiers[Tier + 1]->AsObject(), TEXT("at")) : From;
					const float Frac = To > From ? float(V - From) / float(To - From) : 1.f;
					Box->AddSlot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 0.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(FText::FromString(FString::Printf(TEXT("%s  ·  %s (%d)%s"), *StrIn(F, TEXT("name")), *TierName(Tier), V, To > From ? *FString::Printf(TEXT("  ·  %s at %d"), *TierName(Tier + 1), To) : TEXT(""))), WRUI::Body(14, 600), WRUI::Ink)]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f * U, 0.f, 0.f)[SNew(SWRBar).Height(8.f * U).Color(WRUI::Brass).Fraction(Frac)]
					];
				}
			}
			// Shrines.
			Box->AddSlot().AutoHeight().Padding(0.f, 18.f * U, 0.f, 0.f)[WRUI::Text(LOCTEXT("QuestsShrines", "SHRINES"), WRUI::Caps(13), WRUI::Ink3)];
			if (PC.IsValid())
			{
				for (TActorIterator<AWRShrine> It(PC->GetWorld()); It; ++It)
				{
					const bool bFound = Log && Log->HasFound(It->ShrineId);
					const bool bMine = Log && Log->LitByMe.Contains(It->ShrineId);
					const FString State = !bFound ? FString(TEXT("not found")) : It->bHeld ? FString(TEXT("held by invaders")) : It->bLit ? FString(bMine ? TEXT("lit (by you)") : TEXT("lit")) : FString(It->bGuarded ? TEXT("guarded") : TEXT("dark"));
					Box->AddSlot().AutoHeight().Padding(0.f, 3.f * U, 0.f, 0.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f)[WRUI::Text(FText::FromString(bFound ? It->DisplayName : FString(TEXT("? ? ?"))), WRUI::Body(14, 600), bFound ? WRUI::Ink : WRUI::Ink3)]
						+ SHorizontalBox::Slot().AutoWidth()[WRUI::Text(FText::FromString(FString::Printf(TEXT("%s  ·  %s"), *BiomeName(It->Biome), *State)), WRUI::Body(13, 500), It->bLit && bFound ? WRUI::Brass : WRUI::Ink2)]
					];
				}
			}
			// What's going on in the world now.
			if (const AWRWorldState* State = AWRWorldState::Get(PC.Get()))
			{
				if (State->Events.Num() > 0 || State->Invasion.bOn)
				{
					Box->AddSlot().AutoHeight().Padding(0.f, 18.f * U, 0.f, 0.f)[WRUI::Text(LOCTEXT("QuestsNow", "IN THE WORLD NOW"), WRUI::Caps(13), WRUI::Ink3)];
					for (const FWRWorldEventRep& E : State->Events)
					{
						const FString Title = StrIn(WRWorld::Obj(TEXT("events.") + E.Id.ToString()), TEXT("title"));
						Box->AddSlot().AutoHeight().Padding(0.f, 3.f * U, 0.f, 0.f)[WRUI::Text(FText::FromString(FString::Printf(TEXT("%s%s%s"), *Title, E.Biome.IsNone() ? TEXT("") : *FString::Printf(TEXT("  ·  %s"), *BiomeName(E.Biome)), E.Stage == 0 ? TEXT("  ·  coming") : TEXT(""))), WRUI::Body(14, 600), WRUI::Warn)];
					}
					if (State->Invasion.bOn)
					{
						Box->AddSlot().AutoHeight().Padding(0.f, 3.f * U, 0.f, 0.f)[WRUI::Text(FText::FromString(FString::Printf(TEXT("Invasion: %s into %s through the %s  ·  %d of %d left"), *BiomeName(State->Invasion.From), *BiomeName(State->Invasion.To), *State->Invasion.Pass.ToLower(), State->Invasion.Left, State->Invasion.Total)), WRUI::Body(14, 600), WRUI::Bad)];
					}
				}
			}
		}
	};
}

WR_REGISTER_SCREEN("shrine", [](AWRPlayerController* PC) -> TSharedRef<SWidget> { return SNew(WRUIWorldScreens::SShrine).PC(PC); });
WR_REGISTER_SCREEN("talk", [](AWRPlayerController* PC) -> TSharedRef<SWidget> { return SNew(WRUIWorldScreens::STalk).PC(PC); });
WR_REGISTER_SCREEN("trade", [](AWRPlayerController* PC) -> TSharedRef<SWidget> { return SNew(WRUIWorldScreens::STrade).PC(PC); });
WR_REGISTER_SCREEN("msgwrite", [](AWRPlayerController* PC) -> TSharedRef<SWidget> { return SNew(WRUIWorldScreens::SMessageWrite).PC(PC); });
WR_REGISTER_SCREEN("msgread", [](AWRPlayerController* PC) -> TSharedRef<SWidget> { return SNew(WRUIWorldScreens::SMessageRead).PC(PC); });
WR_REGISTER_JOURNAL_TAB("quests", [](AWRPlayerController* PC) -> TSharedRef<SWidget> { return SNew(WRUIWorldScreens::SQuestsTab).PC(PC); });

#undef LOCTEXT_NAMESPACE
