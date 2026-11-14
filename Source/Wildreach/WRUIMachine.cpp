// A machine's screen (EWRMenu::Machine; the machine is the controller's MenuPiece; cooks/build-machinery.md 2.8). The
// stone-and-leather window: the machine's name and owner, its state in a word and why; its power line (made, drawn,
// the line's speed as a bar, how many sources and machines share it, this piece's own share and the factor behind it);
// its work (what it's making and how far along, a battery's charge, the soulstone burning); its switches (on / off,
// overclock with the perk, repair with its cost, link and send for lifts); what's in it (input and output slots: click
// to take) and your bag (click to put in what it takes). Everything reads the piece's replicated state and sends the
// server's machine commands, so mouse and pad both work (the pad drives buttons and slots, WRPadInput). It closes
// itself if the machine goes or you walk off.

#include "WRPlayerUI.h"
#include "WRUIStyle.h"
#include "WRUIWidgets.h"
#include "WRBuilding.h"
#include "WRCrafting.h"
#include "WRInventory.h"
#include "WRLifeSkills.h"
#include "WRMachine.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRSound.h"
#include "Wildreach.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBackgroundBlur.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"

#define LOCTEXT_NAMESPACE "WRUI"

namespace WRUIMachineScreen
{
	using WRUI::U;

	FString ItemName(FName Id)
	{
		const FWRItemDef* D = FWRItemRegistry::Get().Find(Id);
		return D ? D->Name : Id.ToString();
	}

	class SMachineScreen : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SMachineScreen) {}
			SLATE_ARGUMENT(TWeakObjectPtr<AWRPlayerController>, PC)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			PC = InArgs._PC;
			Piece = PC.IsValid() ? PC->GetMenuPiece() : nullptr;
			const AWRBuildPiece* P = Piece.Get();
			const FWRMachineDef* M = FWRMachines::DefOf(P);
			bManage = P && P->SeemsShared(PC.Get());
			const FName Role = M ? M->Role : NAME_None;
			In = M ? (Role == TEXT("riftchest") ? P->Storage.Num() : M->In) : 0;
			Out = M ? (Role == TEXT("drill") ? M->Out : Role == TEXT("riftchest") ? 0 : M->Out) : 0;
			if (Role == TEXT("drill")) { In = 0; }
			bTakesByHand = In > 0;
			bLift = Role == TEXT("lift");
			bProgress = M && (M->IsProcessor() || Role == TEXT("drill") || Role == TEXT("still") || Role == TEXT("vat"));
			bBattery = M && (M->Flow == TEXT("storm") || M->Flow == TEXT("sun") || M->Flow == TEXT("soul"));
			bPowered = M && M->IsPowered();

			TSharedRef<SVerticalBox> Left = SNew(SVerticalBox);
			TSharedRef<SVerticalBox> Right = SNew(SVerticalBox);

			// The state, in a word and a reason.
			Left->AddSlot().AutoHeight()
			[
				WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(StateLine()); }), WRUI::Caps(13),
					TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(StateColour()); }), false, true)
			];
			if (bPowered)
			{
				// The line: made vs drawn, its speed.
				Left->AddSlot().AutoHeight().Padding(0.f, 14.f * U, 0.f, 4.f * U)[WRUI::Text(LOCTEXT("MachLine", "THE POWER LINE"), WRUI::Caps(11), WRUI::Ink3)];
				Left->AddSlot().AutoHeight()
				[
					SNew(SWRBar).Height(14.f * U)
					.Fraction_Lambda([this]() { const AWRBuildPiece* Q = Piece.Get(); if (!Q) return 0.f; const float L = Q->Machine.Load10, S = Q->Machine.Supply10; return L <= 0.f ? (S > 0.f ? 1.f : 0.f) : FMath::Clamp(S / L, 0.f, 1.f); })
					.Color_Lambda([this]() { const AWRBuildPiece* Q = Piece.Get(); const float L = Q ? Q->Machine.Load10 : 0.f, S = Q ? Q->Machine.Supply10 : 0.f; const float F = L <= 0.f ? 1.f : S / L; return F >= 0.999f ? WRUI::Good : F >= 0.5f ? WRUI::Warn : WRUI::Bad; })
				];
				Left->AddSlot().AutoHeight().Padding(0.f, 5.f * U, 0.f, 0.f)
				[
					WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(LineText()); }), WRUI::Body(13), WRUI::Ink2, false, true)
				];
				Left->AddSlot().AutoHeight().Padding(0.f, 3.f * U, 0.f, 0.f)
				[
					WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(OwnText()); }), WRUI::Body(13), WRUI::Ink, false, true)
				];
			}
			if (bProgress)
			{
				Left->AddSlot().AutoHeight().Padding(0.f, 14.f * U, 0.f, 4.f * U)[WRUI::Text(LOCTEXT("MachWork", "AT WORK ON"), WRUI::Caps(11), WRUI::Ink3)];
				Left->AddSlot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f * U, 0.f)
					[
						SNew(SBox).WidthOverride(34.f * U).HeightOverride(34.f * U)
						[SNew(SImage).Image_Lambda([this]() { const AWRBuildPiece* Q = Piece.Get(); return Q && !Q->Machine.Making.IsNone() ? WRUI::Icon(Q->Machine.Making) : WRUI::Box(FLinearColor::Transparent); })]
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { const AWRBuildPiece* Q = Piece.Get(); return FText::FromString(Q && !Q->Machine.Making.IsNone() ? ItemName(Q->Machine.Making) : FString(TEXT("Nothing yet"))); }), WRUI::Body(14, 600), WRUI::Ink)]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f * U, 0.f, 0.f)
						[
							SNew(SWRBar).Height(8.f * U).Color(WRUI::Brass)
							.Fraction_Lambda([this]() { const AWRBuildPiece* Q = Piece.Get(); return Q ? Q->Machine.Progress / 255.f : 0.f; })
						]
					]
				];
			}
			if (bBattery)
			{
				Left->AddSlot().AutoHeight().Padding(0.f, 14.f * U, 0.f, 4.f * U)
				[
					WRUI::Text(M && M->Flow == TEXT("soul") ? LOCTEXT("MachFuel", "THE SOULSTONE BURNING") : LOCTEXT("MachBattery", "ITS BATTERY"), WRUI::Caps(11), WRUI::Ink3)
				];
				Left->AddSlot().AutoHeight()
				[
					SNew(SWRBar).Height(10.f * U).Color(M && M->Flow == TEXT("storm") ? WRUI::Frost : M && M->Flow == TEXT("soul") ? WRUI::Good : WRUI::Ember)
					.Fraction_Lambda([this]() { const AWRBuildPiece* Q = Piece.Get(); return Q ? Q->Machine.Charge / 255.f : 0.f; })
				];
			}

			// Switches.
			if (bManage)
			{
				TSharedRef<SVerticalBox> Buttons = SNew(SVerticalBox);
				const auto Add = [&Buttons](TSharedRef<SWidget> W) { Buttons->AddSlot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 0.f)[W]; };
				if (!bLift)
				{
					Add(SNew(SWRButton).Primary(true).MinHeight(44.f * U).FontSize(14)
						.Text_Lambda([this]() { const AWRBuildPiece* Q = Piece.Get(); return Q && (Q->Machine.Flags & WRMachineFlag::On) ? LOCTEXT("MachOff", "Switch off") : LOCTEXT("MachOn", "Switch on"); })
						.OnClicked_Lambda([this]() { Send(TEXT("mon")); return FReply::Handled(); }));
				}
				if (bPowered && M && !M->IsSource() && !M->Carries())
				{
					Add(SNew(SWRButton).Ghost(true).MinHeight(40.f * U).FontSize(13)
						.Text_Lambda([this]() { return FText::FromString(OverclockText()); })
						.OnClicked_Lambda([this]() { Send(TEXT("moverclock")); return FReply::Handled(); }));
				}
				if (bLift)
				{
					Add(SNew(SWRButton).Primary(true).MinHeight(44.f * U).FontSize(14).Text(LOCTEXT("MachSend", "Send / call the platform"))
						.OnClicked_Lambda([this]() { Send(TEXT("msend")); return FReply::Handled(); }));
					Add(SNew(SWRButton).Ghost(true).MinHeight(40.f * U).FontSize(13).Text(LOCTEXT("MachLink", "Link to another lift"))
						.OnClicked_Lambda([this]() { Send(TEXT("mlink")); return FReply::Handled(); }));
				}
				Add(SNew(SWRButton).Ghost(true).MinHeight(40.f * U).FontSize(13)
					.Text_Lambda([this]() { return FText::FromString(RepairText()); })
					.OnClicked_Lambda([this]() { Send(TEXT("mrepair")); return FReply::Handled(); }));
				Left->AddSlot().AutoHeight().Padding(0.f, 16.f * U, 0.f, 0.f)[Buttons];
			}
			else
			{
				Left->AddSlot().AutoHeight().Padding(0.f, 16.f * U, 0.f, 0.f)
				[
					WRUI::Text(TAttribute<FText>::CreateLambda([this]() { const AWRBuildPiece* Q = Piece.Get(); return FText::FromString(FString::Printf(TEXT("%s's machine: only they and their clan work it."), Q ? *Q->OwnerName : TEXT("Someone"))); }),
						WRUI::Body(13), WRUI::Ink3, false, true)
				];
			}

			// What it makes (processors): its recipes, plainly.
			if (M && M->IsProcessor())
			{
				Right->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f * U)[WRUI::Text(LOCTEXT("MachMakes", "WHAT IT MAKES"), WRUI::Caps(11), WRUI::Ink3)];
				for (const FWRRecipe* R : FWRRecipeRegistry::Get().ForStation(M->Station))
				{
					FString Line;
					for (const TPair<FName, int32>& I : R->Inputs) Line += FString::Printf(TEXT("%s%d %s"), Line.IsEmpty() ? TEXT("") : TEXT(" + "), I.Value, *ItemName(I.Key).ToLower());
					Line += FString::Printf(TEXT("  ->  %d %s  (%.0f s)"), R->Count, *ItemName(R->Output).ToLower(), R->Seconds);
					Right->AddSlot().AutoHeight().Padding(0.f, 1.f * U)[WRUI::Text(FText::FromString(Line), WRUI::Body(12), WRUI::Ink2)];
				}
			}
			const auto Grid = [this](int32 From, int32 Num, int32 Cols) -> TSharedRef<SWidget>
			{
				TSharedRef<SUniformGridPanel> G = SNew(SUniformGridPanel).SlotPadding(FMargin(3.f * U));
				for (int32 i = 0; i < Num; ++i)
				{
					const int32 Index = From + i;
					G->AddSlot(i % Cols, i / Cols)
					[
						SNew(SWRSlot).Size(56.f * U).Kind(EWRSlotKind::Chest).Index(Index)
						.View_Lambda([this, Index]() { FWRSlotView V; const AWRBuildPiece* Q = Piece.Get(); if (Q && Q->Storage.IsValidIndex(Index) && !Q->Storage[Index].IsEmpty()) { V.ItemId = Q->Storage[Index].ItemId; V.Count = Q->Storage[Index].Count; } V.bDimmed = !bManage; return V; })
						.OnClicked_Lambda([this, Index](const FPointerEvent&) { if (bManage) Send(TEXT("mtake"), FString::FromInt(Index)); })
					];
				}
				return G;
			};
			if (In > 0)
			{
				Right->AddSlot().AutoHeight().Padding(0.f, 12.f * U, 0.f, 4.f * U)
				[
					WRUI::Text(M && M->Role == TEXT("source") ? LOCTEXT("MachFuelBox", "FUEL BOX (SOULSTONE)") : M && (M->Role == TEXT("sorter") || M->Role == TEXT("riftchest")) ? LOCTEXT("MachHolds", "WHAT IT HOLDS") : LOCTEXT("MachIn", "IN"), WRUI::Caps(11), WRUI::Ink3)
				];
				Right->AddSlot().AutoHeight()[Grid(0, In, FMath::Min(10, In))];
			}
			if (Out > 0)
			{
				Right->AddSlot().AutoHeight().Padding(0.f, 12.f * U, 0.f, 4.f * U)[WRUI::Text(M && M->Role == TEXT("drill") ? LOCTEXT("MachHopper", "HOPPER") : LOCTEXT("MachOut", "MADE"), WRUI::Caps(11), WRUI::Ink3)];
				Right->AddSlot().AutoHeight()[Grid(M && M->Role == TEXT("drill") ? 0 : In, Out, FMath::Min(10, Out))];
			}
			if (bManage && (Out > 0 || In > 0))
			{
				Right->AddSlot().AutoHeight().Padding(0.f, 8.f * U, 0.f, 0.f).HAlign(HAlign_Left)
				[
					SNew(SWRButton).Ghost(true).MinHeight(36.f * U).FontSize(12).Text(LOCTEXT("MachTakeAll", "Take all"))
					.OnClicked_Lambda([this]() { Send(TEXT("mtakeall")); return FReply::Handled(); })
				];
			}
			if (bManage && bTakesByHand)
			{
				// Your bag: click a stack to put it in (the server says if it doesn't take it).
				Right->AddSlot().AutoHeight().Padding(0.f, 14.f * U, 0.f, 4.f * U)[WRUI::Text(LOCTEXT("MachBag", "YOUR BAG: CLICK TO PUT IN"), WRUI::Caps(11), WRUI::Ink3)];
				TSharedRef<SUniformGridPanel> Bag = SNew(SUniformGridPanel).SlotPadding(FMargin(3.f * U));
				for (int32 i = 0; i < UWRInventoryComponent::NumSlots; ++i)
				{
					Bag->AddSlot(i % 10, i / 10)
					[
						SNew(SWRSlot).Size(46.f * U).Kind(EWRSlotKind::Bag).Index(i)
						.View_Lambda([this, i]() { FWRSlotView V; if (const UWRInventoryComponent* Inv = BagOf()) { const TArray<FWRItemStack>& S = Inv->GetSlots(); if (S.IsValidIndex(i) && !S[i].IsEmpty()) { V.ItemId = S[i].ItemId; V.Count = S[i].Count; } } return V; })
						.OnClicked_Lambda([this, i](const FPointerEvent&) { Send(TEXT("mput "), FString::FromInt(i)); })
					];
				}
				Right->AddSlot().AutoHeight()[Bag];
			}

			ChildSlot
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[SNew(SBackgroundBlur).BlurStrength(6.f)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.45f))]]
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(1040.f * U).MaxDesiredHeight(900.f * U)
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
											+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { const AWRBuildPiece* Q = Piece.Get(); return FText::FromString(Q && Q->GetDef() ? Q->GetDef()->Name : FString(TEXT("Machine"))); }), WRUI::Title(19), WRUI::Ink, true)]
											+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { const AWRBuildPiece* Q = Piece.Get(); return FText::FromString(Q ? FString::Printf(TEXT("Built by %s  ·  health %.0f / %.0f"), Q->OwnerName.IsEmpty() ? TEXT("no one") : *Q->OwnerName, Q->Health, Q->MaxHealth) : FString()); }), WRUI::Body(12, 500), WRUI::Ink2, true)]
										]
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 7.f * U, 0.f)[SNew(SWRKeyCap).FontSize(11).Text(LOCTEXT("MachEsc", "Esc"))]
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(LOCTEXT("MachClose", "Close"), WRUI::Body(13, 500), WRUI::Ink2, true)]
									]
								]
							]
							+ SVerticalBox::Slot().FillHeight(1.f).Padding(FMargin(18.f * U, 16.f * U, 18.f * U, 0.f))
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().FillWidth(0.42f).Padding(0.f, 0.f, 22.f * U, 0.f)[Left]
								+ SHorizontalBox::Slot().FillWidth(0.58f)[SNew(SScrollBox) + SScrollBox::Slot()[Right]]
							]
						]
					]
				]
			];
		}

		virtual void Tick(const FGeometry& Geo, const double Now, const float Dt) override
		{
			SCompoundWidget::Tick(Geo, Now, Dt);
			// The machine went (broken, demolished) or you walked off: close.
			AWRPlayerController* P = PC.Get();
			const AWRBuildPiece* Q = Piece.Get();
			const APawn* Me = P ? P->GetPawn() : nullptr;
			if (P && P->GetMenu() == EWRMenu::Machine && (!Q || !Me || FVector::Dist(Q->GetActorLocation(), Me->GetActorLocation()) > 800.f))
			{
				P->SetMenu(EWRMenu::None);
			}
		}

		virtual bool SupportsKeyboardFocus() const override { return true; }

	private:
		TWeakObjectPtr<AWRPlayerController> PC;
		TWeakObjectPtr<AWRBuildPiece> Piece;
		bool bManage = false, bTakesByHand = false, bLift = false, bProgress = false, bBattery = false, bPowered = false;
		int32 In = 0, Out = 0;

		const UWRInventoryComponent* BagOf() const
		{
			const AWRPlayerState* PS = PC.IsValid() ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
			return PS ? PS->GetInventory() : nullptr;
		}

		void Send(const FString& Cmd, const FString& Extra = FString())
		{
			const AWRBuildPiece* Q = Piece.Get();
			if (!PC.IsValid() || !Q) return;
			const FVector L = Q->GetActorLocation();
			const FString Verb = Cmd.TrimEnd();
			PC->SendCommand(FString::Printf(TEXT("%s %.0f %.0f %.0f%s%s"), *Verb, L.X, L.Y, L.Z, Extra.IsEmpty() ? TEXT("") : TEXT(" "), *Extra));
			WRSound::PlayUI(PC.Get(), TEXT("ui.click"));
		}

		FString StateLine() const
		{
			const AWRBuildPiece* Q = Piece.Get();
			return Q ? FWRMachines::StateText(Q) : FString();
		}

		FLinearColor StateColour() const
		{
			const AWRBuildPiece* Q = Piece.Get();
			if (!Q) return WRUI::Ink2;
			switch (Q->Machine.State)
			{
			case EWRMachineState::Running: return WRUI::Good;
			case EWRMachineState::Idle: case EWRMachineState::Asleep: case EWRMachineState::Off: return WRUI::Ink2;
			case EWRMachineState::Slow: case EWRMachineState::Worn: case EWRMachineState::OutputFull: return WRUI::Warn;
			default: return WRUI::Bad;
			}
		}

		FString LineText() const
		{
			const AWRBuildPiece* Q = Piece.Get();
			if (!Q) return FString();
			const FWRMachineRep& R = Q->Machine;
			const float S = R.Supply10 / 10.f, L = R.Load10 / 10.f;
			const int32 Pct = L > 0.f ? FMath::RoundToInt(FMath::Min(1.f, S / L) * 100.f) : (S > 0.f ? 100 : 0);
			return FString::Printf(TEXT("%.1f P made, %.1f P drawn: running at %d%%.  %d source%s and %d machine%s on this line."), S, L, Pct,
				R.NetSources, R.NetSources == 1 ? TEXT("") : TEXT("s"), R.NetMachines, R.NetMachines == 1 ? TEXT("") : TEXT("s"));
		}

		FString OwnText() const
		{
			const AWRBuildPiece* Q = Piece.Get();
			const FWRMachineDef* M = FWRMachines::DefOf(Q);
			if (!Q || !M) return FString();
			const FWRMachineRep& R = Q->Machine;
			if (M->IsSource())
			{
				const float F = R.Factor100 / 100.f;
				const FString Why = M->Flow == TEXT("wind") ? FString::Printf(TEXT("the wind at x%.2f"), F)
					: M->Flow == TEXT("water") ? FString::Printf(TEXT("the water's run at x%.2f"), F)
					: M->Flow == TEXT("tide") ? FString::Printf(TEXT("the tide at x%.2f"), F)
					: M->Flow == TEXT("sun") ? FString::Printf(TEXT("the sun at x%.2f"), F)
					: M->Flow == TEXT("storm") ? (F > 0.5f ? FString(TEXT("a storm overhead")) : FString(TEXT("its battery")))
					: M->Flow == TEXT("soul") ? FString(TEXT("a soulstone burning")) : FString(TEXT("the lava's heat"));
				return FString::Printf(TEXT("This one gives %.1f P of its %.0f (%s)."), R.Own10 / 10.f, M->Supply, *Why);
			}
			if (M->Carries() && M->Role != TEXT("conveyor")) return TEXT("Carries the line's turning to whatever its ends meet.");
			const float Draw = R.Own10 / 10.f;
			return Draw > 0.f ? FString::Printf(TEXT("This one draws %.2f P while it works (%.2f P set down)%s."), Draw, M->Load, (R.Flags & WRMachineFlag::Overclock) ? TEXT(", overclocked") : TEXT(""))
				: FString::Printf(TEXT("This one draws %.2f P while it works."), M->Load);
		}

		FString OverclockText() const
		{
			const AWRBuildPiece* Q = Piece.Get();
			const bool bOn = Q && (Q->Machine.Flags & WRMachineFlag::Overclock);
			if (bOn) return TEXT("Overclocked: set back to normal");
			return UWRLifeSkillsComponent::StatFor(PC.Get(), TEXT("overclock")) > 0.f ? TEXT("Overclock: half again as fast, twice the power") : TEXT("Overclock (needs the Overclock perk)");
		}

		FString RepairText() const
		{
			const AWRBuildPiece* Q = Piece.Get();
			const TArray<TPair<FName, int32>> Cost = FWRMachines::RepairCost(Q);
			if (Cost.Num() == 0) return TEXT("In good repair");
			FString Line;
			for (const TPair<FName, int32>& C : Cost) Line += FString::Printf(TEXT("%s%d %s"), Line.IsEmpty() ? TEXT("") : TEXT(", "), C.Value, *ItemName(C.Key).ToLower());
			return FString::Printf(TEXT("Repair: %s"), *Line);
		}
	};
}

WR_REGISTER_SCREEN("machine", [](AWRPlayerController* PC) -> TSharedRef<SWidget> { return SNew(WRUIMachineScreen::SMachineScreen).PC(PC); });

#undef LOCTEXT_NAMESPACE
