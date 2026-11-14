// An incubator's screen (EWRMenu::Incubator; the piece is the controller's MenuPiece; cooks/build-taming.md 2.6). The
// egg in it (its kind, sex, element and what its line carries, how far along and how long to go), its fuel box (what
// the egg needs, how much is in, how long the heat lasts), and your bag: click an egg to set it in, its fuel to stoke
// the firebox. Take the egg or the fuel back out. Reads the piece's replicated state, sends "incubator ..." commands;
// mouse and pad both work. Closes if the incubator goes or you walk off.

#include "WRPlayerUI.h"
#include "WRUIStyle.h"
#include "WRUIWidgets.h"
#include "WRBuilding.h"
#include "WRCreature.h"
#include "WRFarm.h"
#include "WRTaming.h"
#include "WRInventory.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRSound.h"
#include "WRTuning.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBackgroundBlur.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"

#define LOCTEXT_NAMESPACE "WRUI"

namespace WRUIIncubatorScreen
{
	using WRUI::U;

	FString ItemName(FName Id)
	{
		const FWRItemDef* D = FWRItemRegistry::Get().Find(Id);
		return D ? D->Name : Id.ToString();
	}

	FName SpeciesOf(const FWRItemStack& Egg)
	{
		if (Egg.IsEmpty()) return NAME_None;
		FName Species;
		FWRTameGenes::FromSockets(Egg.Sockets, &Species);
		if (!Species.IsNone()) return Species;
		const FString Id = Egg.ItemId.ToString();
		return Id.StartsWith(TEXT("fertile_")) ? FName(*Id.RightChop(8)) : Id.StartsWith(TEXT("egg_")) ? FName(*Id.RightChop(4)) : NAME_None;
	}

	class SIncubator : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SIncubator) {}
			SLATE_ARGUMENT(TWeakObjectPtr<AWRPlayerController>, PC)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			PC = InArgs._PC;
			Piece = PC.IsValid() ? PC->GetMenuPiece() : nullptr;
			TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
			const auto Heading = [](const FText& T) { return WRUI::Text(T, WRUI::Caps(11), WRUI::Ink3); };

			Body->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f * U)[Heading(LOCTEXT("IncEgg", "THE EGG"))];
			Body->AddSlot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f * U, 0.f)
				[
					SNew(SWRSlot).Size(64.f * U).Kind(EWRSlotKind::Chest).Index(0)
					.View_Lambda([this]() { FWRSlotView V; const FWRItemStack* E = Stack(0); if (E) V.ItemId = E->ItemId; return V; })
					.OnClicked_Lambda([this](const FPointerEvent&) { if (Stack(0)) Send(TEXT("out")); })
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(EggLine()); }), WRUI::Body(14, 600), WRUI::Ink, false, true)]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f * U, 0.f, 0.f)[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(StateLine()); }), WRUI::Body(13), TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(StateColour()); }), false, true)]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 0.f)[SNew(SWRBar).Height(10.f * U).Color(WRUI::Brass).Fraction_Lambda([this]() { const AWRBuildPiece* P = Piece.Get(); return P ? P->Growth8 / 255.f : 0.f; })]
				]
			];
			Body->AddSlot().AutoHeight().Padding(0.f, 16.f * U, 0.f, 4.f * U)[Heading(LOCTEXT("IncFuel", "THE FIREBOX"))];
			Body->AddSlot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f * U, 0.f)
				[
					SNew(SWRSlot).Size(64.f * U).Kind(EWRSlotKind::Chest).Index(1)
					.View_Lambda([this]() { FWRSlotView V; const FWRItemStack* F = Stack(1); if (F) { V.ItemId = F->ItemId; V.Count = F->Count; V.bShowCount = true; } return V; })
					.OnClicked_Lambda([this](const FPointerEvent&) { if (Stack(1)) Send(TEXT("fuelout")); })
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(FuelLine()); }), WRUI::Body(13), WRUI::Ink2, false, true)
				]
			];
			Body->AddSlot().AutoHeight().Padding(0.f, 16.f * U, 0.f, 4.f * U)[Heading(LOCTEXT("IncBag", "YOUR BAG: CLICK AN EGG TO SET IT IN, ITS FUEL TO STOKE THE FIRE"))];
			TSharedRef<SUniformGridPanel> Bag = SNew(SUniformGridPanel).SlotPadding(FMargin(3.f * U));
			for (int32 i = 0; i < UWRInventoryComponent::NumSlots; ++i)
			{
				Bag->AddSlot(i % 10, i / 10)
				[
					SNew(SWRSlot).Size(46.f * U).Kind(EWRSlotKind::Bag).Index(i)
					.View_Lambda([this, i]() { FWRSlotView V; if (const UWRInventoryComponent* Inv = BagOf()) { const TArray<FWRItemStack>& S = Inv->GetSlots(); if (S.IsValidIndex(i) && !S[i].IsEmpty()) { V.ItemId = S[i].ItemId; V.Count = S[i].Count; V.bDimmed = !Takes(S[i]); } } return V; })
					.OnClicked_Lambda([this, i](const FPointerEvent&) { BagClicked(i); })
				];
			}
			Body->AddSlot().AutoHeight()[Bag];

			ChildSlot
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[SNew(SBackgroundBlur).BlurStrength(6.f)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.45f))]]
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(660.f * U)
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
											+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(LOCTEXT("IncTitle", "Incubator"), WRUI::Title(19), WRUI::Ink, true)]
											+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { const AWRBuildPiece* P = Piece.Get(); return FText::FromString(P ? FString::Printf(TEXT("Built by %s  ·  the hatchling is whoever set the egg in's"), *P->OwnerName) : FString()); }), WRUI::Body(12, 500), WRUI::Ink2, true)]
										]
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 7.f * U, 0.f)[SNew(SWRKeyCap).FontSize(11).Text(LOCTEXT("IncEsc", "Esc"))]
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(LOCTEXT("IncClose", "Close"), WRUI::Body(13, 500), WRUI::Ink2, true)]
									]
								]
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(18.f * U, 16.f * U, 18.f * U, 0.f))[Body]
						]
					]
				]
			];
		}

		virtual void Tick(const FGeometry& Geo, const double Now, const float Dt) override
		{
			SCompoundWidget::Tick(Geo, Now, Dt);
			AWRPlayerController* P = PC.Get();
			const AWRBuildPiece* Q = Piece.Get();
			const APawn* Me = P ? P->GetPawn() : nullptr;
			if (P && P->GetMenu() == EWRMenu::Incubator && (!Q || !Me || FVector::Dist(Q->GetActorLocation(), Me->GetActorLocation()) > 800.f))
			{
				P->SetMenu(EWRMenu::None);
			}
		}

		virtual bool SupportsKeyboardFocus() const override { return true; }

	private:
		TWeakObjectPtr<AWRPlayerController> PC;
		TWeakObjectPtr<AWRBuildPiece> Piece;

		const UWRInventoryComponent* BagOf() const
		{
			const AWRPlayerState* PS = PC.IsValid() ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
			return PS ? PS->GetInventory() : nullptr;
		}

		const FWRItemStack* Stack(int32 i) const
		{
			const AWRBuildPiece* P = Piece.Get();
			return P && P->Storage.IsValidIndex(i) && !P->Storage[i].IsEmpty() ? &P->Storage[i] : nullptr;
		}

		const FWRCreatureDef* EggDef() const
		{
			const FWRItemStack* E = Stack(0);
			return E ? FWRCreatureRegistry::Get().Find(SpeciesOf(*E)) : nullptr;
		}

		bool IsFuel(FName Id) const
		{
			const FWRCreatureDef* D = EggDef();
			if (D) return FWRTaming::FuelFor(D->Element).Contains(Id);
			return Id == TEXT("charcoal") || Id == TEXT("sulfur_crystal") || Id == TEXT("ice_block") || Id == TEXT("waterskin") || Id == TEXT("wood");
		}

		bool Takes(const FWRItemStack& S) const
		{
			const FName Sp = SpeciesOf(S);
			return (!Stack(0) && !Sp.IsNone() && FWRCreatureRegistry::Get().Find(Sp)) || IsFuel(S.ItemId);
		}

		void Send(const FString& Cmd)
		{
			const AWRBuildPiece* P = Piece.Get();
			if (!PC.IsValid() || !P) return;
			const FVector L = P->GetActorLocation();
			PC->SendCommand(FString::Printf(TEXT("incubator %.0f %.0f %.0f %s"), L.X, L.Y, L.Z, *Cmd));
			WRSound::PlayUI(PC.Get(), TEXT("ui.click"));
		}

		void BagClicked(int32 i)
		{
			const UWRInventoryComponent* Inv = BagOf();
			if (!Inv || !Inv->GetSlots().IsValidIndex(i) || Inv->GetSlots()[i].IsEmpty()) return;
			const FWRItemStack& S = Inv->GetSlots()[i];
			if (!SpeciesOf(S).IsNone() && !Stack(0)) Send(FString::Printf(TEXT("in %d"), i));
			else Send(FString::Printf(TEXT("fuel %d"), i));
		}

		FString EggLine() const
		{
			const FWRItemStack* E = Stack(0);
			const FWRCreatureDef* D = EggDef();
			if (!E || !D) return TEXT("Empty: set an egg in from your bag");
			const FWRTameGenes G = FWRTameGenes::FromSockets(E->Sockets);
			if (!G.bValid) return FString::Printf(TEXT("%s egg  ·  %s element  ·  a wild clutch: its blood is the wild's"), *D->Name, D->Element.IsNone() ? TEXT("no") : *D->Element.ToString());
			return FString::Printf(TEXT("%s egg  ·  %s  ·  %s element  ·  level %d%s"), *D->Name, G.Sex == 2 ? TEXT("female") : TEXT("male"), D->Element.IsNone() ? TEXT("no") : *D->Element.ToString(), G.Level(),
				G.Morphs.Num() > 0 ? TEXT("  ·  something's different about this one") : TEXT(""));
		}

		FString StateLine() const
		{
			const AWRBuildPiece* P = Piece.Get();
			const FWRCreatureDef* D = EggDef();
			if (!P || !D) return FString();
			if (P->FarmStop == 2) return TEXT("Ready to hatch: make room under your tame limit");
			if (P->FarmStop == 1) return TEXT("Cold: the egg waits (it won't die). Stoke the firebox");
			const float Total = FWRTaming::ByDifficulty(*D, TEXT("breed.pregnancy"), 1800.f, 7200.f, 21600.f) / FWRTaming::Maturation();
			const int32 Left = FMath::RoundToInt(Total * (1.f - P->Growth8 / 255.f));
			return FString::Printf(TEXT("Warm: %.0f%%  ·  hatches in about %s"), P->Growth8 / 2.55f, *(Left >= 3600 ? FString::Printf(TEXT("%dh %02dm"), Left / 3600, (Left / 60) % 60) : FString::Printf(TEXT("%d:%02d"), Left / 60, Left % 60)));
		}

		FLinearColor StateColour() const
		{
			const AWRBuildPiece* P = Piece.Get();
			return !P ? WRUI::Ink2 : P->FarmStop == 1 ? WRUI::Warn : P->FarmStop == 2 ? WRUI::Brass : WRUI::Good;
		}

		FString FuelLine() const
		{
			const FWRCreatureDef* D = EggDef();
			const FString Needs = D ? FString::JoinBy(FWRTaming::FuelFor(D->Element), TEXT(" or "), [](const FName& F) { return ItemName(F).ToLower(); }) : FString(TEXT("the egg's fuel"));
			const FWRItemStack* F = Stack(1);
			const float Each = WRTune(TEXT("tame.incubator.fuelSeconds"), 900.f) / FWRTaming::Maturation();
			return FString::Printf(TEXT("It burns %s: each keeps it warm about %.0f min.%s  Click it to take it out."), *Needs, Each / 60.f,
				F ? *FString::Printf(TEXT("  %d in: about %.0f min of heat."), F->Count, F->Count * Each / 60.f) : TEXT("  Empty."));
		}
	};
}

WR_REGISTER_SCREEN("incubator", [](AWRPlayerController* PC) -> TSharedRef<SWidget> { return SNew(WRUIIncubatorScreen::SIncubator).PC(PC); });

#undef LOCTEXT_NAMESPACE
