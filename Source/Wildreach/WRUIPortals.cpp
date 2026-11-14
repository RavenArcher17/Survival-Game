// The portals' and artifacts' screens (cooks/build-portals-artifacts.md 2.15): the portal's card (rank, land, state,
// modifiers, owner, who comes; Enter / Back), the party's 5 s accept card, and the journal's Artifacts tab (the belt's
// three hooks, the pouch with hook / fuse / salvage, the geis records and titles, the band). Every control is a button,
// so the pad's input processor drives them as the mouse does; each sends "portal ..." / "art ..." / "band ..." commands.

#include "WRPlayerUI.h"
#include "WRArtifacts.h"
#include "WRPortals.h"
#include "WRPortalsInternal.h"
#include "WRUIStyle.h"
#include "WRUIWidgets.h"
#include "WRPlayerController.h"
#include "WRCreature.h"
#include "WRPlayerState.h"
#include "WRSocial.h"
#include "WRTuning.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBackgroundBlur.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"

#define LOCTEXT_NAMESPACE "WRUI"

namespace WRUIPortalScreens
{
	using WRUI::U;

	TSharedRef<SWidget> PFrame(const TAttribute<FText>& Title, const TAttribute<FText>& Sub, TSharedRef<SWidget> Body, float Width, const TAttribute<FSlateColor>& Accent)
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
							SNew(SBox).HeightOverride(64.f * U)
							[
								SNew(SWRPanel).Kind(EWRPanel::Leather).Padding(FMargin(16.f * U, 0.f, 14.f * U, 0.f))
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f * U, 0.f)
									[
										SNew(SBox).WidthOverride(14.f * U).HeightOverride(40.f * U)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity_Lambda([Accent]() { return Accent.Get().GetSpecifiedColor(); })]
									]
									+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
									[
										SNew(SVerticalBox)
										+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(Title, WRUI::Title(19), WRUI::Ink, true)]
										+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(Sub, WRUI::Body(12, 500), WRUI::Ink2, true)]
									]
									+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 7.f * U, 0.f)[SNew(SWRKeyCap).FontSize(11).Text(LOCTEXT("PEsc", "Esc"))]
									+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(LOCTEXT("PClose", "Back"), WRUI::Body(13, 500), WRUI::Ink2, true)]
								]
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(18.f * U, 16.f * U, 18.f * U, 0.f))[Body]
					]
				]
			];
	}

	TSharedRef<SWidget> PButton(const TAttribute<FText>& Text, TFunction<void()> OnClick, bool bPrimary, const TAttribute<bool>& Enabled = true, float Height = 40.f)
	{
		return SNew(SBox).HeightOverride(Height * U)
			[
				SNew(SWRButton).Text(Text).Primary(bPrimary).FontSize(13).IsEnabled(Enabled)
				.OnClicked_Lambda([OnClick]() { OnClick(); return FReply::Handled(); })
			];
	}

	TSharedRef<SWidget> Heading(const FText& T) { return WRUI::Text(T, WRUI::Caps(11), WRUI::Ink3); }

	FLinearColor Lin(const FColor& C) { return FLinearColor(C); }

	// ------------------------------------------------------------------------------------------------ the portal's card

	class SPortalCard : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SPortalCard) {}
			SLATE_ARGUMENT(TWeakObjectPtr<AWRPlayerController>, PC)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			PC = InArgs._PC;
			Portal = PC.IsValid() ? Cast<AWRPortal>(PC->WorldTarget.Get()) : nullptr;
			const AWRPortal* P = Portal.Get();
			TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
			Body->AddSlot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { const AWRPortal* Pt = Portal.Get(); return FText::FromString(Pt ? Pt->StateLine() : FString()); }), WRUI::Body(14, 600), WRUI::Ink, false, true)];
			Body->AddSlot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 0.f)
			[
				WRUI::Text(LOCTEXT("PortalAbout", "A fold into a sealed place of this land's old people: handmade rooms, their traps and trials, and the guardian at the end. Death inside follows this world's rules; while it stays open you can walk back in and run from the entry stone."), WRUI::Body(12), WRUI::Ink2, false, true)
			];
			// The modifiers, before you go in (no surprises): each adds a quarter to the loot.
			Body->AddSlot().AutoHeight().Padding(0.f, 14.f * U, 0.f, 4.f * U)[Heading(LOCTEXT("PortalMods", "ITS MODIFIERS  ·  EACH ADDS 25% TO THE LOOT"))];
			if (!P || P->Modifiers.Num() == 0)
			{
				Body->AddSlot().AutoHeight()[WRUI::Text(LOCTEXT("PortalNoMods", "None: the fold as its makers left it."), WRUI::Body(13), WRUI::Ink2)];
			}
			else
			{
				for (const FName& M : P->Modifiers)
				{
					Body->AddSlot().AutoHeight().Padding(0.f, 2.f * U)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 10.f * U, 0.f)[WRUI::Text(FText::FromString(WRPortals::ModifierName(M)), WRUI::Body(14, 700), WRUI::Ink)]
						+ SHorizontalBox::Slot().FillWidth(1.f)[WRUI::Text(FText::FromString(WRPortals::ModifierLine(M)), WRUI::Body(13), WRUI::Ink2, false, true)]
					];
				}
			}
			// Who comes: everyone within 10 m is asked as you step in.
			Body->AddSlot().AutoHeight().Padding(0.f, 14.f * U, 0.f, 4.f * U)[Heading(LOCTEXT("PortalParty", "WHO COMES WITH YOU"))];
			Body->AddSlot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(PartyLine()); }), WRUI::Body(13), WRUI::Ink2, false, true)];
			Body->AddSlot().AutoHeight().Padding(0.f, 18.f * U, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 8.f * U, 0.f)
				[
					PButton(TAttribute<FText>::CreateLambda([this]() { const FString Why = WRPortals::EnterBlock(PC.Get(), Portal.Get()); return Why.IsEmpty() ? LOCTEXT("PortalEnter", "Enter the fold") : FText::FromString(Why); }),
						[this]()
						{
							AWRPlayerController* P2 = PC.Get();
							const AWRPortal* Pt = Portal.Get();
							if (!P2 || !Pt || !WRPortals::EnterBlock(P2, Pt).IsEmpty()) return;
							P2->SendCommand(FString::Printf(TEXT("portal enter %d"), Pt->PortalId));
							P2->SetMenu(EWRMenu::None);
						}, true, TAttribute<bool>::CreateLambda([this]() { return WRPortals::EnterBlock(PC.Get(), Portal.Get()).IsEmpty(); }), 46.f)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).WidthOverride(150.f * U)[PButton(LOCTEXT("PortalBack", "Back"), [this]() { if (PC.IsValid()) PC->SetMenu(EWRMenu::None); }, false, true, 46.f)]
				]
			];
			ChildSlot
			[
				PFrame(TAttribute<FText>::CreateLambda([this]() { const AWRPortal* Pt = Portal.Get(); return FText::FromString(Pt ? FString::Printf(TEXT("%s portal"), *WRPortals::RankName(Pt->Rank)) : FString()); }),
					TAttribute<FText>::CreateLambda([this]() { const AWRPortal* Pt = Portal.Get(); return FText::FromString(Pt ? FString::Printf(TEXT("%s%s"), *WRPortals::BiomeName(Pt->Biome), Pt->bKey ? *FString::Printf(TEXT("  ·  Owner: %s"), *Pt->OwnerName) : TEXT("  ·  wild: first come")) : FString()); }),
					Body, 680.f, TAttribute<FSlateColor>::CreateLambda([this]() { const AWRPortal* Pt = Portal.Get(); return FSlateColor(Pt ? Lin(WRPortals::RankColour(Pt->Rank)) : WRUI::Ink3); }))
			];
		}

		virtual void Tick(const FGeometry& Geo, const double Now, const float Dt) override
		{
			SCompoundWidget::Tick(Geo, Now, Dt);
			AWRPlayerController* P = PC.Get();
			if (!P || P->GetMenu() != EWRMenu::World || P->WorldScreen != TEXT("portal")) return;
			const APawn* Me = P->GetPawn();
			if (!Portal.IsValid() || !Me || FVector::Dist(Portal->GetActorLocation(), Me->GetActorLocation()) > 1200.f) P->SetMenu(EWRMenu::None);
		}
		virtual bool SupportsKeyboardFocus() const override { return true; }

	private:
		TWeakObjectPtr<AWRPlayerController> PC;
		TWeakObjectPtr<AWRPortal> Portal;

		FString PartyLine() const
		{
			const AWRPlayerController* P = PC.Get();
			const APawn* Me = P ? P->GetPawn() : nullptr;
			if (!Me || !P->GetWorld() || !P->GetWorld()->GetGameState()) return FString();
			TArray<FString> Near;
			for (APlayerState* S : P->GetWorld()->GetGameState()->PlayerArray)
			{
				const APawn* O = S ? S->GetPawn() : nullptr;
				if (O && O != Me && FVector::Dist(O->GetActorLocation(), Me->GetActorLocation()) < WRPortals::Num(TEXT("entry.partyRadius"), 1000.f)) Near.Add(S->GetPlayerName());
			}
			return Near.Num() == 0 ? FString(TEXT("You alone. Others may follow you in until the boss door seals."))
				: FString::Printf(TEXT("%s will be asked to come (5 s to answer)."), *FString::Join(Near, TEXT(", ")));
		}
	};

	// ------------------------------------------------------------------------------------------------ the accept card

	class SPortalInvite : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SPortalInvite) {}
			SLATE_ARGUMENT(TWeakObjectPtr<AWRPlayerController>, PC)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			PC = InArgs._PC;
			Portal = PC.IsValid() ? Cast<AWRPortal>(PC->WorldTarget.Get()) : nullptr;
			Opened = FPlatformTime::Seconds();
			Seconds = WRPortals::Num(TEXT("entry.acceptSeconds"), 5.f);
			TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
			Body->AddSlot().AutoHeight()[WRUI::Text(LOCTEXT("InviteAsk", "Your party steps into the fold. Come with them?"), WRUI::Body(14, 600), WRUI::Ink, false, true)];
			Body->AddSlot().AutoHeight().Padding(0.f, 10.f * U, 0.f, 0.f)
			[
				SNew(SWRBar).Height(8.f * U).Color(WRUI::Brass).Fraction_Lambda([this]() { return FMath::Clamp(1.f - float(FPlatformTime::Seconds() - Opened) / Seconds, 0.f, 1.f); })
			];
			Body->AddSlot().AutoHeight().Padding(0.f, 14.f * U, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 8.f * U, 0.f)[PButton(LOCTEXT("InviteCome", "Come"), [this]() { Answer(true); }, true, true, 44.f)]
				+ SHorizontalBox::Slot().FillWidth(1.f)[PButton(LOCTEXT("InviteStay", "Stay"), [this]() { Answer(false); }, false, true, 44.f)]
			];
			ChildSlot
			[
				PFrame(TAttribute<FText>::CreateLambda([this]() { const AWRPortal* Pt = Portal.Get(); return FText::FromString(Pt ? FString::Printf(TEXT("The %s fold"), *WRPortals::RankName(Pt->Rank)) : FString()); }),
					TAttribute<FText>::CreateLambda([this]() { const AWRPortal* Pt = Portal.Get(); return FText::FromString(Pt ? WRPortals::BiomeName(Pt->Biome) : FString()); }),
					Body, 520.f, TAttribute<FSlateColor>::CreateLambda([this]() { const AWRPortal* Pt = Portal.Get(); return FSlateColor(Pt ? Lin(WRPortals::RankColour(Pt->Rank)) : WRUI::Ink3); }))
			];
		}

		virtual void Tick(const FGeometry& Geo, const double Now, const float Dt) override
		{
			SCompoundWidget::Tick(Geo, Now, Dt);
			if (FPlatformTime::Seconds() - Opened > Seconds) Answer(false);
		}
		virtual bool SupportsKeyboardFocus() const override { return true; }

	private:
		TWeakObjectPtr<AWRPlayerController> PC;
		TWeakObjectPtr<AWRPortal> Portal;
		double Opened = 0.0;
		float Seconds = 5.f;
		bool bAnswered = false;

		void Answer(bool bCome)
		{
			AWRPlayerController* P = PC.Get();
			if (bAnswered || !P) return;
			bAnswered = true;
			if (const AWRPortal* Pt = Portal.Get()) P->SendCommand(FString::Printf(TEXT("portal accept %d %d"), Pt->PortalId, bCome ? 1 : 0));
			if (P->GetMenu() == EWRMenu::World && P->WorldScreen == TEXT("portalinvite")) P->SetMenu(EWRMenu::None);
		}
	};

	// ------------------------------------------------------------------------------------------------ the artifacts tab

	class SArtifactsTab : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SArtifactsTab) {}
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
			const UWRArtifactComponent* A = Art();
			uint32 Key = 0;
			if (A)
			{
				for (const FWRArtifactRep& R : A->Hooks) Key = HashCombine(Key, HashCombine(GetTypeHash(R.Id), R.Weight));
				for (const FWRArtifactRep& R : A->Pouch) Key = HashCombine(Key, HashCombine(GetTypeHash(R.Id), R.Weight + 7));
				for (const FWRGeisRecord& R : A->Records) Key = HashCombine(Key, HashCombine(GetTypeHash(R.Boss), R.Best));
				for (const FString& S : A->Band) Key = HashCombine(Key, GetTypeHash(S));
				for (const FString& S : A->BandInvites) Key = HashCombine(Key, GetTypeHash(S) + 3);
				Key = HashCombine(Key, A->Titles.Num());
			}
			const UWorld* W = PC.IsValid() ? PC->GetWorld() : nullptr;
			if (W && W->GetGameState()) Key = HashCombine(Key, W->GetGameState()->PlayerArray.Num());
			Key = HashCombine(Key, WRArtifacts::AtAlchemyTable(PC.IsValid() ? PC->GetPawn() : nullptr) ? 1u : 2u);
			if (Key != LastKey) { LastKey = Key; Rebuild(); }
		}

	private:
		TWeakObjectPtr<AWRPlayerController> PC;
		TSharedPtr<SVerticalBox> Box;
		uint32 LastKey = 0;
		int32 FusePick = -1;

		UWRArtifactComponent* Art() const { return PC.IsValid() ? UWRArtifactComponent::For(PC->PlayerState) : nullptr; }
		void Send(const FString& C) const { if (AWRPlayerController* P = PC.Get()) P->SendCommand(C); }

		TSharedRef<SWidget> Charm(const FWRArtifactRep& R, TSharedRef<SWidget> Buttons) const
		{
			const TSharedPtr<FJsonObject> Def = WRArtifacts::Def(R.Id);
			FString Geis, Look;
			if (Def.IsValid()) { Def->TryGetStringField(TEXT("geis"), Geis); Def->TryGetStringField(TEXT("look"), Look); }
			static const TCHAR* Roman[] = { TEXT("I"), TEXT("II"), TEXT("III"), TEXT("IV"), TEXT("V") };
			return SNew(SWRPanel).Kind(EWRPanel::Inset).Padding(FMargin(12.f * U, 10.f * U))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 12.f * U, 0.f)
					[
						SNew(SBox).WidthOverride(10.f * U)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity(Lin(WRArtifacts::MetalColour(R.Weight)))]
					]
					+ SHorizontalBox::Slot().FillWidth(1.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(FText::FromString(FString::Printf(TEXT("%s  ·  %s, Weight %s"), *WRArtifacts::Name(R.Id), *WRArtifacts::MetalName(R.Weight), Roman[FMath::Clamp<int32>(R.Weight, 1, 5) - 1])), WRUI::Body(15, 700), WRUI::Ink)]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f * U, 0.f, 0.f)[WRUI::Text(FText::FromString(Geis), WRUI::Body(13, 600), WRUI::Brass, false, true)]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f * U, 0.f, 0.f)[WRUI::Text(FText::FromString(Look), WRUI::Body(12), WRUI::Ink3, false, true)]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f * U, 0.f, 0.f, 0.f)[Buttons]
				];
		}

		void Rebuild()
		{
			Box->ClearChildren();
			UWRArtifactComponent* A = Art();
			AWRPlayerController* P = PC.Get();
			if (!A || !P) return;
			const int32 AL = A->RawAL();
			const int32 Party = WRArtifacts::PartyAL(P->PlayerState);
			const float PerDmg = WRArtifacts::Num(TEXT("perAL.damageTaken"), 0.07f), PerLoot = WRArtifacts::Num(TEXT("perAL.loot"), 0.05f), PerXP = WRArtifacts::Num(TEXT("perAL.pathXP"), 0.04f);
			// What your charms do now.
			Box->AddSlot().AutoHeight()[Heading(LOCTEXT("ArtLevel", "ARTIFACT LEVEL"))];
			Box->AddSlot().AutoHeight().Padding(0.f, 4.f * U, 0.f, 0.f)
			[
				WRUI::Text(FText::FromString(FString::Printf(TEXT("Yours: %d   ·   your party's (the highest counts): %d"), AL, Party)), WRUI::Title(17), WRUI::Ink)
			];
			Box->AddSlot().AutoHeight().Padding(0.f, 4.f * U, 0.f, 0.f)
			[
				WRUI::Text(TAttribute<FText>::CreateLambda([this, Party, PerDmg, PerLoot, PerXP]()
				{
					const UWRArtifactComponent* X = Art();
					FString S = FString::Printf(TEXT("You take %d%% more from creatures, bosses and the folds' traps; loot +%d%%, path XP +%d%%, boss parts +%d points."),
						FMath::RoundToInt(PerDmg * 100.f * Party), FMath::RoundToInt(PerLoot * 100.f * Party), FMath::RoundToInt(PerXP * 100.f * Party), Party);
					if (Party >= 5) S += TEXT(" Parts may come Scarred.");
					if (Party >= 1) S += TEXT(" Bosses' hidden third phases open for your party.");
					if (X && X->IsPaused()) S += TEXT("  Paused: player blows or a safe zone (they come back on their own).");
					return FText::FromString(S);
				}), WRUI::Body(13), WRUI::Ink2, false, true)
			];
			// The belt: three hooks.
			Box->AddSlot().AutoHeight().Padding(0.f, 18.f * U, 0.f, 4.f * U)[Heading(LOCTEXT("ArtBelt", "YOUR BELT  ·  TYING ONE ON OR OFF TAKES 2 S, OUT OF A FIGHT, NEVER INSIDE A FOLD"))];
			Box->AddSlot().AutoHeight()
			[
				SNew(SWRBar).Height(6.f * U).Color(WRUI::Brass).Fraction_Lambda([this]() { const UWRArtifactComponent* X = Art(); const UWorld* W = PC.IsValid() ? PC->GetWorld() : nullptr; return X && W && X->HookingUntil > 0.f ? FMath::Clamp(1.f - (X->HookingUntil - WRPortalsImpl::PServerNow(W)) / 2.f, 0.f, 1.f) : 0.f; })
				.Visibility_Lambda([this]() { const UWRArtifactComponent* X = Art(); return X && X->HookingUntil > 0.f ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			];
			const int32 Slots = FMath::Clamp(FMath::RoundToInt(WRTune(TEXT("server.artifactSlots"), 3.f)), 0, 3);
			for (int32 h = 0; h < 3; ++h)
			{
				const FWRArtifactRep R = A->Hooks.IsValidIndex(h) ? A->Hooks[h] : FWRArtifactRep();
				if (h >= Slots)
				{
					Box->AddSlot().AutoHeight().Padding(0.f, 3.f * U)[WRUI::Text(FText::Format(LOCTEXT("ArtClosed", "Hook {0}: closed on this world"), h + 1), WRUI::Body(13), WRUI::Ink3)];
					continue;
				}
				if (R.IsEmpty())
				{
					Box->AddSlot().AutoHeight().Padding(0.f, 3.f * U)[SNew(SWRPanel).Kind(EWRPanel::Inset).Padding(FMargin(12.f * U, 10.f * U))[WRUI::Text(FText::Format(LOCTEXT("ArtFree", "Hook {0}: free"), h + 1), WRUI::Body(14, 600), WRUI::Ink3)]];
					continue;
				}
				Box->AddSlot().AutoHeight().Padding(0.f, 3.f * U)
				[
					Charm(R, SNew(SBox).WidthOverride(130.f * U)[PButton(LOCTEXT("ArtUntie", "Untie"), [this, h]() { Send(FString::Printf(TEXT("art unhook %d"), h)); }, false)])
				];
			}
			// The pouch.
			const bool bTable = WRArtifacts::AtAlchemyTable(P->GetPawn());
			Box->AddSlot().AutoHeight().Padding(0.f, 18.f * U, 0.f, 4.f * U)[Heading(bTable ? LOCTEXT("ArtPouchTable", "THE POUCH  ·  AT THE ALCHEMY TABLE: FUSE TWO OF A KIND, OR SALVAGE ONE FOR ESSENCE")
				: LOCTEXT("ArtPouch", "THE POUCH  ·  FUSE AND SALVAGE AT AN ALCHEMY TABLE"))];
			if (A->Pouch.Num() == 0)
			{
				Box->AddSlot().AutoHeight()[WRUI::Text(LOCTEXT("ArtNone", "Nothing yet. Artifacts drop from the great bosses (more often the heavier your charms) and from Rare, Epic and Legendary folds; your fourth great kill always gives one."), WRUI::Body(13), WRUI::Ink2, false, true)];
			}
			for (int32 i = 0; i < A->Pouch.Num(); ++i)
			{
				const FWRArtifactRep R = A->Pouch[i];
				TSharedRef<SHorizontalBox> Buttons = SNew(SHorizontalBox);
				for (int32 h = 0; h < Slots; ++h)
				{
					Buttons->AddSlot().AutoWidth().Padding(0.f, 0.f, 4.f * U, 0.f)[SNew(SBox).WidthOverride(84.f * U)[PButton(FText::Format(LOCTEXT("ArtHookN", "Hook {0}"), h + 1), [this, i, h]() { Send(FString::Printf(TEXT("art hook %d %d"), i, h)); }, h == 0)]];
				}
				if (bTable)
				{
					Buttons->AddSlot().AutoWidth().Padding(6.f * U, 0.f, 4.f * U, 0.f)
					[
						SNew(SBox).WidthOverride(96.f * U)
						[
							PButton(TAttribute<FText>::CreateLambda([this, i]() { return FusePick == i ? LOCTEXT("ArtPicked", "Picked") : FusePick >= 0 ? LOCTEXT("ArtFuseWith", "Fuse with") : LOCTEXT("ArtFuse", "Fuse"); }),
								[this, i]()
								{
									if (FusePick == i) { FusePick = -1; return; }
									if (FusePick >= 0) { Send(FString::Printf(TEXT("art fuse %d %d"), FusePick, i)); FusePick = -1; return; }
									FusePick = i;
								}, false)
						]
					];
					Buttons->AddSlot().AutoWidth()[SNew(SBox).WidthOverride(96.f * U)[PButton(LOCTEXT("ArtSalvage", "Salvage"), [this, i]() { Send(FString::Printf(TEXT("art salvage %d"), i)); }, false)]];
				}
				Box->AddSlot().AutoHeight().Padding(0.f, 3.f * U)[Charm(R, Buttons)];
			}
			// Geis records and titles.
			Box->AddSlot().AutoHeight().Padding(0.f, 18.f * U, 0.f, 4.f * U)[Heading(LOCTEXT("ArtRecords", "GEIS RECORDS  ·  A NEW HIGHEST LEVEL ON A GREAT BOSS PAYS ONCE"))];
			if (A->Records.Num() == 0) Box->AddSlot().AutoHeight()[WRUI::Text(LOCTEXT("ArtNoRecords", "None yet: kill a great boss wearing your charms."), WRUI::Body(13), WRUI::Ink2)];
			for (const FWRGeisRecord& R : A->Records)
			{
				const FWRCreatureDef* D = FWRCreatureRegistry::Get().Find(R.Boss);
				Box->AddSlot().AutoHeight().Padding(0.f, 2.f * U)[WRUI::Text(FText::FromString(FString::Printf(TEXT("%s  ·  highest: AL %d"), D ? *D->Name : *R.Boss.ToString(), int32(R.Best))), WRUI::Body(14, 600), WRUI::Ink)];
			}
			if (A->Titles.Num() > 0) Box->AddSlot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 0.f)[WRUI::Text(FText::FromString(FString::Printf(TEXT("Titles: %s"), *FString::Join(A->Titles, TEXT(", ")))), WRUI::Body(14, 700), WRUI::Brass)];
			// The band: a party beyond the clan, for sharing what your charms give and take.
			Box->AddSlot().AutoHeight().Padding(0.f, 18.f * U, 0.f, 4.f * U)[Heading(LOCTEXT("ArtBand", "YOUR BAND  ·  ITS HIGHEST ARTIFACT LEVEL COUNTS FOR ALL OF YOU NEAR EACH OTHER, LIKE YOUR CLAN'S"))];
			Box->AddSlot().AutoHeight()[WRUI::Text(FText::FromString(A->Band.Num() ? FString::Printf(TEXT("With you: %s"), *FString::Join(A->Band, TEXT(", "))) : FString(TEXT("You're in no band."))), WRUI::Body(14, 600), WRUI::Ink)];
			if (A->Band.Num()) Box->AddSlot().AutoHeight().Padding(0.f, 4.f * U, 0.f, 0.f)[SNew(SBox).WidthOverride(200.f * U).HAlign(HAlign_Left)[PButton(LOCTEXT("ArtLeave", "Leave the band"), [this]() { Send(TEXT("band leave")); }, false)]];
			for (const FString& From : A->BandInvites)
			{
				Box->AddSlot().AutoHeight().Padding(0.f, 4.f * U)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[WRUI::Text(FText::FromString(FString::Printf(TEXT("%s asks you into their band"), *From)), WRUI::Body(14, 600), WRUI::Ink)]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 6.f * U, 0.f)[SNew(SBox).WidthOverride(110.f * U)[PButton(LOCTEXT("ArtAccept", "Join"), [this, From]() { Send(FString::Printf(TEXT("band accept %s"), *From)); }, true)]]
					+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(110.f * U)[PButton(LOCTEXT("ArtDecline", "Decline"), [this, From]() { Send(FString::Printf(TEXT("band decline %s"), *From)); }, false)]]
				];
			}
			if (const UWorld* W = P->GetWorld(); W && W->GetGameState())
			{
				for (APlayerState* S : W->GetGameState()->PlayerArray)
				{
					if (!S || S == P->PlayerState || A->Band.Contains(S->GetPlayerName())) continue;
					const FString Name = S->GetPlayerName();
					Box->AddSlot().AutoHeight().Padding(0.f, 3.f * U)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[WRUI::Text(FText::FromString(Name), WRUI::Body(14), WRUI::Ink2)]
						+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(150.f * U)[PButton(LOCTEXT("ArtInvite", "Ask into band"), [this, Name]() { Send(FString::Printf(TEXT("band invite %s"), *Name)); }, false)]]
					];
				}
			}
		}
	};
}

WR_REGISTER_SCREEN("portal", [](AWRPlayerController* PC) -> TSharedRef<SWidget> { return SNew(WRUIPortalScreens::SPortalCard).PC(PC); });
WR_REGISTER_SCREEN("portalinvite", [](AWRPlayerController* PC) -> TSharedRef<SWidget> { return SNew(WRUIPortalScreens::SPortalInvite).PC(PC); });
WR_REGISTER_JOURNAL_TAB("artifacts", [](AWRPlayerController* PC) -> TSharedRef<SWidget> { return SNew(WRUIPortalScreens::SArtifactsTab).PC(PC); });

#undef LOCTEXT_NAMESPACE
