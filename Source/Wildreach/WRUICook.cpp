// The Cook screen (EWRMenu::Cook; the controller's MenuStation is the cooking pot, oven, or a campfire with Camp
// chef): discovery cooking (cooks/build-farming.md 2.8, after BotW's pot). Left, the four places in the pot and Cook;
// middle, the food in your bag (click / A puts one in, click a place in the pot takes it out); right, the dishes you
// know (one click cooks one from your bag) and the ones a recipe scrap hinted at, and what's on the fire now with the
// tray to take from. A dish found for the first time slides a parchment card in. Every action is a server command
// ("cook <slots...>", "cookknown <dish>", "crafttake"); the screen only reads replicated state. Mouse and pad: the
// slots and buttons are the pad's targets (WRPadInput); Enter cooks, Backspace takes the last one out, Esc closes.

#include "WRPlayerUI.h"
#include "WRUIStyle.h"
#include "WRUIWidgets.h"
#include "WRCrafting.h"
#include "WRFarm.h"
#include "WRInventory.h"
#include "WRLifeSkills.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRSound.h"
#include "Wildreach.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBackgroundBlur.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"

#define LOCTEXT_NAMESPACE "WRUI"

namespace WRUICookScreen
{
	using WRUI::U;

	class SCookScreen : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SCookScreen) {}
			SLATE_ARGUMENT(TWeakObjectPtr<AWRPlayerController>, PC)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			PC = InArgs._PC;
			Station = PC.IsValid() ? PC->GetMenuStation() : nullptr;
			const UWRFoodComponent* F = Food();
			SeenDiscovery = F ? F->DiscoveredAt : -100.f;

			TSharedRef<SHorizontalBox> Pot = SNew(SHorizontalBox);
			for (int32 i = 0; i < 4; ++i)
			{
				Pot->AddSlot().AutoWidth().Padding(i == 0 ? 0.f : 10.f * U, 0.f, 0.f, 0.f)
				[
					SNew(SWRSlot).Size(76.f * U).Kind(EWRSlotKind::Other).Index(i)
					.View_Lambda([this, i]() { return PotView(i); })
					.OnClicked_Lambda([this, i](const FPointerEvent&) { TakeOut(i); })
				];
			}

			ChildSlot
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[SNew(SBackgroundBlur).BlurStrength(6.f)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.45f))]]
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(1180.f * U).HeightOverride(680.f * U)
					[
						SNew(SWRPanel).Kind(EWRPanel::Stone).Rivets(true).BrassTrim(true).Padding(FMargin(10.f * U, 10.f * U, 10.f * U, 14.f * U))
						[
							SNew(SVerticalBox)
							// Title band: which station, how it works, Esc.
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
											+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(StationTitle()); }), WRUI::Title(19), WRUI::Ink, true)]
											+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(LOCTEXT("CookHow", "Put 1 to 4 ingredients in and cook: what they make, you'll know from then on."), WRUI::Body(12, 500), WRUI::Ink2, true)]
										]
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 7.f * U, 0.f)[SNew(SWRKeyCap).FontSize(11).Text(LOCTEXT("CookEsc", "Esc"))]
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(LOCTEXT("CookClose", "Close"), WRUI::Body(13, 500), WRUI::Ink2, true)]
									]
								]
							]
							+ SVerticalBox::Slot().FillHeight(1.f).Padding(FMargin(12.f * U, 14.f * U, 12.f * U, 0.f))
							[
								SNew(SHorizontalBox)
								// Left: the pot, what it'll make, Cook.
								+ SHorizontalBox::Slot().AutoWidth()
								[
									SNew(SBox).WidthOverride(350.f * U)
									[
										SNew(SVerticalBox)
										+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(LOCTEXT("InPot", "In the pot"), WRUI::Caps(12), WRUI::Ink2)]
										+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f * U, 0.f, 0.f)[Pot]
										+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f * U, 0.f, 0.f)
										[
											SNew(SBox).HeightOverride(64.f * U)
											[
												WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(PreviewLine()); }), WRUI::Body(14, 700),
													TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(PreviewColor()); }), false, true)
											]
										]
										+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 0.f)
										[
											SNew(SHorizontalBox)
											+ SHorizontalBox::Slot().FillWidth(1.f)
											[
												SNew(SWRButton).Primary(true).FontSize(16).MinHeight(52.f * U).Text(LOCTEXT("CookBtn", "Cook (Enter)"))
												.IsEnabled_Lambda([this]() { return InPot.Num() > 0; })
												.OnClicked_Lambda([this]() { CookNow(); return FReply::Handled(); })
											]
											+ SHorizontalBox::Slot().AutoWidth().Padding(10.f * U, 0.f, 0.f, 0.f)
											[
												SNew(SWRButton).Ghost(true).FontSize(13).MinHeight(52.f * U).Text(LOCTEXT("CookClear", "Empty it"))
												.IsEnabled_Lambda([this]() { return InPot.Num() > 0; })
												.OnClicked_Lambda([this]() { InPot.Reset(); return FReply::Handled(); })
											]
										]
										// What's cooking now, and the tray.
										+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 18.f * U, 0.f, 0.f)[WRUI::Text(LOCTEXT("OnFire", "On the fire"), WRUI::Caps(12), WRUI::Ink2)]
										+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 0.f)
										[
											WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(CookingLine()); }), WRUI::Body(13, 600), WRUI::Ink, false, true)
										]
										+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 0.f)
										[
											SNew(SWRBar).Height(8.f * U).Color(WRUI::Ember)
											.Fraction_Lambda([this]() { return Station.IsValid() && Station->Queue.Num() > 0 ? Station->JobProgress() : 0.f; })
										]
										+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f * U, 0.f, 0.f)
										[
											SNew(SWRButton).FontSize(13).MinHeight(44.f * U)
											.Text_Lambda([this]() { return FText::Format(LOCTEXT("TakeTray", "Take from the tray ({0})"), TrayCount()); })
											.IsEnabled_Lambda([this]() { return TrayCount() > 0; })
											.OnClicked_Lambda([this]() { Send(TEXT("crafttake")); return FReply::Handled(); })
										]
										+ SVerticalBox::Slot().FillHeight(1.f)[SNew(SSpacer)]
										+ SVerticalBox::Slot().AutoHeight()
										[
											WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(Message); }), WRUI::Body(13, 700),
												TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(MessageColor); }), false, true)
										]
									]
								]
								// Middle: the food in your bag.
								+ SHorizontalBox::Slot().FillWidth(1.f).Padding(18.f * U, 0.f, 0.f, 0.f)
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(LOCTEXT("YourFood", "Food in your bag"), WRUI::Caps(12), WRUI::Ink2)]
									+ SVerticalBox::Slot().FillHeight(1.f).Padding(0.f, 8.f * U, 0.f, 0.f)
									[
										SNew(SWRPanel).Kind(EWRPanel::Inset).Padding(FMargin(8.f * U))
										[
											SNew(SScrollBox) + SScrollBox::Slot()[SAssignNew(BagBox, SBox)]
										]
									]
								]
								// Right: known dishes, hints.
								+ SHorizontalBox::Slot().AutoWidth().Padding(18.f * U, 0.f, 0.f, 0.f)
								[
									SNew(SBox).WidthOverride(330.f * U)
									[
										SNew(SVerticalBox)
										+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(KnownTitle()); }), WRUI::Caps(12), WRUI::Ink2)]
										+ SVerticalBox::Slot().FillHeight(1.f).Padding(0.f, 8.f * U, 0.f, 0.f)
										[
											SNew(SWRPanel).Kind(EWRPanel::Inset).Padding(FMargin(8.f * U))
											[
												SNew(SScrollBox) + SScrollBox::Slot()[SAssignNew(KnownBox, SBox)]
											]
										]
									]
								]
							]
						]
					]
				]
				// The discovery card: parchment, sliding in from the right, for a few seconds.
				+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(FMargin(0.f, 120.f * U, 60.f * U, 0.f))
				[
					SNew(SBox).WidthOverride(420.f * U)
					.Visibility_Lambda([this]() { return CardAge() < 4.0 ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
					.RenderTransform_Lambda([this]()
					{
						const double T = CardAge();
						const float In = FMath::Clamp(float(T / 0.35), 0.f, 1.f), Out = FMath::Clamp(float((T - 3.5) / 0.5), 0.f, 1.f);
						return TOptional<FSlateRenderTransform>(FSlateRenderTransform(FVector2D((1.f - FMath::InterpEaseOut(0.f, 1.f, In, 3.f) + Out) * 480.f * U, 0.f)));
					})
					[
						SNew(SWRPanel).Kind(EWRPanel::Parchment).BrassTrim(true).Padding(FMargin(16.f * U, 12.f * U, 16.f * U, 14.f * U))
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								SNew(SBox).WidthOverride(64.f * U).HeightOverride(64.f * U)[SNew(SImage).Image_Lambda([this]() { const UWRFoodComponent* F = Food(); return WRUI::Icon(F ? F->LastDiscovery : NAME_None); })]
							]
							+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(14.f * U, 0.f, 0.f, 0.f)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(LOCTEXT("NewRecipe", "NEW RECIPE"), WRUI::Caps(12), WRUI::InkDark)]
								+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(CardName()); }), WRUI::Title(22), WRUI::InkDark)]
								+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(CardPattern()); }), WRUI::Body(12, 600), WRUI::InkDark, false, true)]
							]
						]
					]
				]
			];
			RebuildLists();
		}

		virtual bool SupportsKeyboardFocus() const override { return true; }

		virtual FReply OnKeyDown(const FGeometry& Geo, const FKeyEvent& Event) override
		{
			const FKey K = Event.GetKey();
			if (K == EKeys::Enter) { CookNow(); return FReply::Handled(); }
			if (K == EKeys::BackSpace) { if (InPot.Num() > 0) InPot.Pop(); return FReply::Handled(); }
			if (K == EKeys::Delete) { InPot.Reset(); return FReply::Handled(); }
			return FReply::Unhandled();   // Esc and the rest: the game's keys
		}

		virtual void Tick(const FGeometry& Geo, const double Time, const float Delta) override
		{
			SCompoundWidget::Tick(Geo, Time, Delta);
			AWRPlayerController* P = PC.Get();
			if (!P) return;
			if (Frames++ < 3) FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EFocusCause::SetDirectly);
			const APawn* Me = P->GetPawn();
			if (!Station.IsValid() || Station->IsActorBeingDestroyed() || !Me || FVector::Dist(Station->GetActorLocation(), Me->GetActorLocation()) > 650.f)
			{
				P->SetMenu(EWRMenu::None);
				return;
			}
			// A slot in the pot whose stack ran out (eaten, moved) comes out.
			const UWRInventoryComponent* Inv = Bag();
			if (Inv)
			{
				TMap<int32, int32> Used;
				for (int32 i = InPot.Num() - 1; i >= 0; --i)
				{
					const TArray<FWRItemStack>& S = Inv->GetSlots();
					if (!S.IsValidIndex(InPot[i]) || S[InPot[i]].IsEmpty() || ++Used.FindOrAdd(InPot[i]) > S[InPot[i]].Count) InPot.RemoveAt(i);
				}
			}
			// The lists follow the bag and what you know.
			const uint32 Key = ListKey();
			if (Key != LastListKey) RebuildLists();
			// The server's answer to what was sent.
			if (SentAt > 0.0 && P->GetNoticeTime() >= SentAt)
			{
				SentAt = 0.0;
				Message = P->GetNotice();
				const bool bBad = Message.StartsWith(TEXT("Not")) || Message.StartsWith(TEXT("no ")) || Message.StartsWith(TEXT("that")) || Message.StartsWith(TEXT("you ")) || Message.Contains(TEXT("full"));
				MessageColor = bBad ? WRUI::Bad : WRUI::Good;
			}
			// A new dish: the card and its chime.
			const UWRFoodComponent* F = Food();
			if (F && F->DiscoveredAt > SeenDiscovery + 0.01f)
			{
				SeenDiscovery = F->DiscoveredAt;
				CardShownAt = FPlatformTime::Seconds();
				WRSound::PlayUI(P, TEXT("cook.discover"));
			}
		}

	private:
		TWeakObjectPtr<AWRPlayerController> PC;
		TWeakObjectPtr<AWRStation> Station;
		/** Bag slot indices in the pot (a slot twice = two of it). */
		TArray<int32> InPot;
		TSharedPtr<SBox> BagBox, KnownBox;
		uint32 LastListKey = 0;
		FString Message;
		FLinearColor MessageColor = WRUI::Ink2;
		double SentAt = 0.0;
		float SeenDiscovery = -100.f;
		double CardShownAt = -100.0;
		int32 Frames = 0;

		const UWRInventoryComponent* Bag() const
		{
			const AWRPlayerState* PS = PC.IsValid() ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
			return PS ? PS->GetInventory() : nullptr;
		}
		const UWRFoodComponent* Food() const
		{
			const AWRPlayerState* PS = PC.IsValid() ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
			return PS ? PS->GetFood() : nullptr;
		}
		FName CookAt() const
		{
			const FName T = Station.IsValid() ? Station->StationType : NAME_None;
			return T == TEXT("campfire") ? FName(TEXT("cooking_pot")) : T;
		}

		FString StationTitle() const
		{
			const FName T = Station.IsValid() ? Station->StationType : NAME_None;
			return T == TEXT("oven") ? TEXT("Oven") : T == TEXT("campfire") ? TEXT("Campfire (Camp chef)") : TEXT("Cooking pot");
		}

		static FString ItemName(FName Id)
		{
			const FWRItemDef* D = FWRItemRegistry::Get().Find(Id);
			return D ? D->Name : Id.ToString();
		}

		FWRSlotView PotView(int32 i) const
		{
			const UWRInventoryComponent* Inv = Bag();
			if (!Inv || !InPot.IsValidIndex(i) || !Inv->GetSlots().IsValidIndex(InPot[i])) return FWRSlotView();
			FWRSlotView V;
			const FWRItemStack& S = Inv->GetSlots()[InPot[i]];
			V.ItemId = S.ItemId;
			V.Count = 1;
			if (FWRSpoil::Perishes(S.ItemId)) V.Durability = FWRSpoil::FreshOf(S, FWRSpoil::DisplayNow);
			return V;
		}

		void PutIn(int32 BagSlot)
		{
			const UWRInventoryComponent* Inv = Bag();
			if (!Inv || !Inv->GetSlots().IsValidIndex(BagSlot) || InPot.Num() >= 4) return;
			int32 Already = 0;
			for (const int32 S : InPot) Already += S == BagSlot ? 1 : 0;
			if (Already >= Inv->GetSlots()[BagSlot].Count) return;
			InPot.Add(BagSlot);
			WRSound::PlayUI(PC.Get(), TEXT("ui.drop"));
		}

		void TakeOut(int32 i)
		{
			if (InPot.IsValidIndex(i)) { InPot.RemoveAt(i); WRSound::PlayUI(PC.Get(), TEXT("ui.drag")); }
		}

		TArray<FName> PotItems() const
		{
			TArray<FName> Out;
			const UWRInventoryComponent* Inv = Bag();
			for (const int32 S : InPot) if (Inv && Inv->GetSlots().IsValidIndex(S)) Out.Add(Inv->GetSlots()[S].ItemId);
			return Out;
		}

		FString PreviewLine() const
		{
			if (InPot.Num() == 0) return TEXT("Pick ingredients from your bag.");
			const UWRFoodComponent* F = Food();
			const FWRDishDef* D = FWRFarmDefs::Get().Match(CookAt(), PotItems());
			if (!D && Station.IsValid() && Station->StationType == TEXT("campfire")) D = FWRFarmDefs::Get().Match(TEXT("oven"), PotItems());
			// Only what you know is told: anything else, you find out by cooking it.
			if (D && F && F->Known.Contains(D->Id)) return FString::Printf(TEXT("Makes %s"), *ItemName(D->Id));
			return TEXT("Something new? Cook it and see (no dish: Grey mush).");
		}
		FLinearColor PreviewColor() const
		{
			const UWRFoodComponent* F = Food();
			const FWRDishDef* D = InPot.Num() > 0 ? FWRFarmDefs::Get().Match(CookAt(), PotItems()) : nullptr;
			return D && F && F->Known.Contains(D->Id) ? WRUI::Good : WRUI::Ink2;
		}

		FString CookingLine() const
		{
			const AWRStation* S = Station.Get();
			if (!S || S->Queue.Num() == 0) return TEXT("Nothing cooking.");
			const FWRRecipe* R = FWRRecipeRegistry::Get().Find(S->Queue[0].Recipe);
			return FString::Printf(TEXT("%s%s"), R ? *ItemName(R->Output) : TEXT("Something"), S->Queue.Num() > 1 ? *FString::Printf(TEXT(" (+%d after)"), S->Queue.Num() - 1) : TEXT(""));
		}
		int32 TrayCount() const
		{
			int32 N = 0;
			if (const AWRStation* S = Station.Get()) for (const FWRItemStack& I : S->Output) N += I.IsEmpty() ? 0 : I.Count;
			return N;
		}

		FString KnownTitle() const
		{
			const UWRFoodComponent* F = Food();
			int32 Here = 0, All = 0;
			for (const FWRDishDef& D : FWRFarmDefs::Get().Dishes())
			{
				if (!WRBiome::IsLive(D.Biome)) continue;
				++All;
				Here += F && F->Known.Contains(D.Id) ? 1 : 0;
			}
			return FString::Printf(TEXT("Dishes you know · %d of %d"), Here, All);
		}

		double CardAge() const { return FPlatformTime::Seconds() - CardShownAt; }
		FString CardName() const { const UWRFoodComponent* F = Food(); return F ? ItemName(F->LastDiscovery) : FString(); }
		FString CardPattern() const
		{
			const UWRFoodComponent* F = Food();
			const FWRDishDef* D = F ? FWRFarmDefs::Get().Dish(F->LastDiscovery) : nullptr;
			return D ? FWRFarmDefs::Get().PatternText(*D, false) : FString();
		}

		void Send(const FString& Command)
		{
			if (AWRPlayerController* P = PC.Get())
			{
				P->SendCommand(Command);
				SentAt = P->GetWorld()->GetTimeSeconds();
			}
		}

		void CookNow()
		{
			if (InPot.Num() == 0) return;
			FString Cmd = TEXT("cook");
			for (const int32 S : InPot) Cmd += FString::Printf(TEXT(" %d"), S);
			Send(Cmd);
			InPot.Reset();
			UE_LOG(LogWildreach, Log, TEXT("COOK screen sends %s"), *Cmd);
		}

		uint32 ListKey() const
		{
			uint32 Key = 0;
			if (const UWRInventoryComponent* Inv = Bag())
			{
				for (const FWRItemStack& S : Inv->GetSlots()) Key = HashCombine(Key, S.IsEmpty() ? 0u : HashCombine(GetTypeHash(S.ItemId), uint32(S.Count)));
			}
			if (const UWRFoodComponent* F = Food())
			{
				Key = HashCombine(Key, uint32(F->Known.Num()) * 131u + uint32(F->Hinted.Num()));
			}
			return Key;
		}

		void RebuildLists()
		{
			LastListKey = ListKey();
			const UWRInventoryComponent* Inv = Bag();
			const UWRFoodComponent* F = Food();
			// The bag's food: anything with a food group (and golden crops), 6 to a row.
			TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel).SlotPadding(FMargin(4.f * U));
			int32 n = 0;
			if (Inv)
			{
				const TArray<FWRItemStack>& Slots = Inv->GetSlots();
				for (int32 s = 0; s < Slots.Num(); ++s)
				{
					if (Slots[s].IsEmpty()) continue;
					if (FWRFarmDefs::Get().GroupOf(Slots[s].ItemId).IsNone() && Slots[s].ItemId != TEXT("golden_crop")) continue;
					Grid->AddSlot(n % 6, n / 6)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
						[
							SNew(SWRSlot).Size(62.f * U).Kind(EWRSlotKind::Bag).Index(s)
							.View_Lambda([this, s]()
							{
								const UWRInventoryComponent* I = Bag();
								if (!I || !I->GetSlots().IsValidIndex(s)) return FWRSlotView();
								FWRSlotView V;
								const FWRItemStack& St = I->GetSlots()[s];
								V.ItemId = St.ItemId;
								int32 Used = 0;
								for (const int32 P : InPot) Used += P == s ? 1 : 0;
								V.Count = St.Count - Used;
								V.bShowCount = true;
								V.bDimmed = V.Count <= 0;
								if (FWRSpoil::Perishes(St.ItemId)) V.Durability = FWRSpoil::FreshOf(St, FWRSpoil::DisplayNow);
								return V;
							})
							.OnClicked_Lambda([this, s](const FPointerEvent&) { PutIn(s); })
						]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 2.f * U, 0.f, 0.f)
						[
							WRUI::Text(FText::FromName(Slots[s].ItemId == TEXT("golden_crop") ? FName(TEXT("Golden")) : FWRFarmDefs::Get().GroupOf(Slots[s].ItemId)), WRUI::Body(10, 600), WRUI::Ink3)
						]
					];
					++n;
				}
			}
			if (n == 0)
			{
				BagBox->SetContent(WRUI::Text(LOCTEXT("NoFood", "No ingredients in your bag: meat, fish, crops, eggs, milk, honey, a waterskin..."), WRUI::Body(13, 500), WRUI::Ink3, false, true));
			}
			else
			{
				BagBox->SetContent(Grid);
			}

			// Known dishes (one click cooks one), then hints, then how many are still out there.
			TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
			int32 Unknown = 0;
			int32 MinN = 4, MaxN = 1;
			for (const FWRDishDef& D : FWRFarmDefs::Get().Dishes())
			{
				if (!WRBiome::IsLive(D.Biome)) continue;
				const bool bKnown = F && F->Known.Contains(D.Id);
				const bool bHint = F && F->Hinted.Contains(D.Id);
				if (!bKnown && !bHint) { ++Unknown; MinN = FMath::Min(MinN, D.Pattern.Num()); MaxN = FMath::Max(MaxN, D.Pattern.Num()); continue; }
				const bool bHere = D.Station == CookAt() || (Station.IsValid() && Station->StationType == TEXT("campfire"));
				const FName Id = D.Id;
				List->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f * U)
				[
					SNew(SWRButton).Ghost(!bKnown).FontSize(13).MinHeight(48.f * U)
					.Text(FText::FromString(FString::Printf(TEXT("%s%s · %s"), bKnown ? TEXT("") : TEXT("Hint: "), *ItemName(D.Id), *FWRFarmDefs::Get().PatternText(D, false))))
					.IsEnabled(bKnown && bHere)
					.OnClicked_Lambda([this, Id]() { Send(FString::Printf(TEXT("cookknown %s"), *Id.ToString())); return FReply::Handled(); })
				];
				if (bKnown && !bHere)
				{
					List->AddSlot().AutoHeight().Padding(8.f * U, 0.f, 0.f, 6.f * U)[WRUI::Text(FText::FromString(FString::Printf(TEXT("(made at the %s)"), D.Station == TEXT("oven") ? TEXT("oven") : TEXT("cooking pot"))), WRUI::Body(11, 500), WRUI::Ink3)];
				}
			}
			if (Unknown > 0)
			{
				// Recipe sense (cooking perk): how many ingredients the undiscovered ones take.
				const bool bSense = UWRLifeSkillsComponent::StatFor(PC.Get(), TEXT("recipeSense")) > 0.f;
				const FString Line = bSense ? FString::Printf(TEXT("%d still to find, each from %d to %d ingredients (Recipe sense)."), Unknown, MinN, MaxN)
					: FString::Printf(TEXT("%d still to find. Recipe scraps in old kitchens and on the line hint at them."), Unknown);
				List->AddSlot().AutoHeight().Padding(0.f, 8.f * U, 0.f, 0.f)[WRUI::Text(FText::FromString(Line), WRUI::Body(12, 500), WRUI::Ink3, false, true)];
			}
			KnownBox->SetContent(List);
		}
	};
}

WR_REGISTER_SCREEN("cook", [](AWRPlayerController* PC) -> TSharedRef<SWidget> { return SNew(WRUICookScreen::SCookScreen).PC(PC); });

#undef LOCTEXT_NAMESPACE
