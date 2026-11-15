// A stable's screen (EWRMenu::Stable; the piece is the controller's MenuPiece; cooks/cookpass-taming.md 3.9). Two
// lists: your tames in its stalls (name, kind, level, sex, health; "Call out", 5 s while it's led out, then it walks out
// of the gate) and your tames standing near it (within 20 m: "Put in a stall", it walks in through the half door).
// Clan-mates' stalls aren't shown. Reads the piece's replicated roster and the tames on this machine, sends
// "stable <x> <y> <z> ..." commands; mouse and pad both work (the pad's cursor drives the buttons). Closes if the stable
// goes or you walk off.

#include "WRPlayerUI.h"
#include "WRUIStyle.h"
#include "WRUIWidgets.h"
#include "WRBuilding.h"
#include "WRCreature.h"
#include "WRTaming.h"
#include "WRPlayerController.h"
#include "WRSound.h"
#include "WRTuning.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBackgroundBlur.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"

#define LOCTEXT_NAMESPACE "WRUI"

namespace WRUIStableScreen
{
	using WRUI::U;

	/** One stall line from the roster ("owner\tname\tspecies\tlevel\tsex\thealth"). */
	struct FStall { FString Owner, Name, Species; int32 Level = 0, Sex = 0; float Health = 1.f; };

	FStall Parse(const FString& Line)
	{
		TArray<FString> P;
		Line.ParseIntoArray(P, TEXT("\t"), false);
		FStall S;
		if (P.Num() >= 6)
		{
			S.Owner = P[0];
			S.Name = P[1];
			S.Species = P[2];
			S.Level = FCString::Atoi(*P[3]);
			S.Sex = FCString::Atoi(*P[4]);
			S.Health = FCString::Atof(*P[5]);
		}
		return S;
	}

	FString KindName(const FString& Species)
	{
		const FWRCreatureDef* D = FWRCreatureRegistry::Get().Find(FName(*Species));
		return D ? D->Name : Species;
	}

	class SStable : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SStable) {}
			SLATE_ARGUMENT(TWeakObjectPtr<AWRPlayerController>, PC)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			PC = InArgs._PC;
			Piece = PC.IsValid() ? PC->GetMenuPiece() : nullptr;
			StallList = SNew(SVerticalBox);
			NearList = SNew(SVerticalBox);
			const auto Heading = [](const TAttribute<FText>& T) { return WRUI::Text(T, WRUI::Caps(11), WRUI::Ink3); };

			ChildSlot
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[SNew(SBackgroundBlur).BlurStrength(6.f)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.45f))]]
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(980.f * U).MaxDesiredHeight(760.f * U)
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
											+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(LOCTEXT("StableTitle", "Stable"), WRUI::Title(19), WRUI::Ink, true)]
											+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(SubTitle()); }), WRUI::Body(12, 500), WRUI::Ink2, true)]
										]
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 7.f * U, 0.f)[SNew(SWRKeyCap).FontSize(11).Text(LOCTEXT("StableEsc", "Esc"))]
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(LOCTEXT("StableClose", "Close"), WRUI::Body(13, 500), WRUI::Ink2, true)]
									]
								]
							]
							+ SVerticalBox::Slot().FillHeight(1.f).Padding(FMargin(18.f * U, 16.f * U, 18.f * U, 0.f))
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().FillWidth(0.5f).Padding(0.f, 0.f, 22.f * U, 0.f)
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f * U)[Heading(LOCTEXT("StableIn", "IN ITS STALLS"))]
									+ SVerticalBox::Slot().FillHeight(1.f)[SNew(SScrollBox) + SScrollBox::Slot()[StallList.ToSharedRef()]]
								]
								+ SHorizontalBox::Slot().FillWidth(0.5f)
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f * U)[Heading(LOCTEXT("StableNear", "YOUR TAMES NEAR IT (20 M)"))]
									+ SVerticalBox::Slot().FillHeight(1.f)[SNew(SScrollBox) + SScrollBox::Slot()[NearList.ToSharedRef()]]
								]
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(18.f * U, 10.f * U, 18.f * U, 0.f))
							[
								WRUI::Text(LOCTEXT("StableHelp", "Stabled tames don't eat, age or count on your tame limit. Calling one out takes a moment; it walks out of the gate. Torn down, a stable lets every tame in it out."),
									WRUI::Body(12), WRUI::Ink3, false, true)
							]
						]
					]
				]
			];
			Rebuild();
		}

		virtual void Tick(const FGeometry& Geo, const double Now, const float Dt) override
		{
			SCompoundWidget::Tick(Geo, Now, Dt);
			AWRPlayerController* P = PC.Get();
			const AWRBuildPiece* Q = Piece.Get();
			const APawn* Me = P ? P->GetPawn() : nullptr;
			if (P && P->GetMenu() == EWRMenu::Stable && (!Q || !Me || FVector::Dist(Q->GetActorLocation(), Me->GetActorLocation()) > 1200.f))
			{
				P->SetMenu(EWRMenu::None);
				return;
			}
			// The lists change as tames go in and out and walk about: rebuilt when what they'd show changes.
			const uint32 Key = ListKey();
			if (Key != LastKey) Rebuild();
		}

		virtual bool SupportsKeyboardFocus() const override { return true; }

	private:
		TWeakObjectPtr<AWRPlayerController> PC;
		TWeakObjectPtr<AWRBuildPiece> Piece;
		TSharedPtr<SVerticalBox> StallList, NearList;
		uint32 LastKey = 0;

		FString Me() const
		{
			const APlayerState* PS = PC.IsValid() ? PC->PlayerState : nullptr;
			return PS ? PS->GetPlayerName() : FString();
		}

		TArray<AWRCreature*> Near() const
		{
			TArray<AWRCreature*> Out;
			const AWRBuildPiece* Q = Piece.Get();
			if (!Q || !PC.IsValid()) return Out;
			const FString Name = Me();
			for (TActorIterator<AWRCreature> It(Q->GetWorld()); It; ++It)
			{
				if (It->IsAlive() && It->TamedBy == Name && FVector::Dist(It->GetActorLocation(), Q->GetActorLocation()) < WRTune(TEXT("tame.stable.storeRange"), 2000.f)) Out.Add(*It);
			}
			Out.Sort([Q](const AWRCreature& A, const AWRCreature& B) { return FVector::DistSquared(A.GetActorLocation(), Q->GetActorLocation()) < FVector::DistSquared(B.GetActorLocation(), Q->GetActorLocation()); });
			return Out;
		}

		uint32 ListKey() const
		{
			const AWRBuildPiece* Q = Piece.Get();
			uint32 H = Q ? GetTypeHash(Q->Roster.Num()) : 0;
			if (Q) for (const FString& L : Q->Roster) H = HashCombine(H, GetTypeHash(L));
			for (const AWRCreature* C : Near()) H = HashCombine(H, HashCombine(GetTypeHash(C), GetTypeHash(C->GetRider() != nullptr)));
			return H;
		}

		int32 Mine() const
		{
			const AWRBuildPiece* Q = Piece.Get();
			return Q ? FWRTaming::StabledBy(Q, Me()) : 0;
		}

		FString SubTitle() const
		{
			const AWRBuildPiece* Q = Piece.Get();
			if (!Q) return FString();
			return FString::Printf(TEXT("%d of your %d stalls here in use  ·  %s's"), Mine(), int32(WRTune(TEXT("tame.stable.perPlayer"), 10.f)), *Q->OwnerName);
		}

		void Send(const FString& Cmd)
		{
			const AWRBuildPiece* Q = Piece.Get();
			if (!PC.IsValid() || !Q) return;
			const FVector L = Q->GetActorLocation();
			PC->SendCommand(FString::Printf(TEXT("stable %.0f %.0f %.0f %s"), L.X, L.Y, L.Z, *Cmd));
			WRSound::PlayUI(PC.Get(), TEXT("ui.click"));
		}

		TSharedRef<SWidget> Row(const FString& Title, const FString& Line, float Health, const TSharedRef<SWidget>& Right)
		{
			return SNew(SBox).Padding(FMargin(0.f, 3.f * U))
			[
				SNew(SWRPanel).Kind(EWRPanel::Leather).Padding(FMargin(12.f * U, 8.f * U))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f * U, 0.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(FText::FromString(Title), WRUI::Body(14, 700), WRUI::Ink)]
						+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(FText::FromString(Line), WRUI::Body(12), WRUI::Ink2, false, true)]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f * U, 0.f, 0.f)[SNew(SWRBar).Height(5.f * U).Fraction(FMath::Clamp(Health, 0.f, 1.f)).Color(Health < 0.3f ? WRUI::Bad : WRUI::Good)]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(170.f * U)[Right]]
				]
			];
		}

		void Rebuild()
		{
			LastKey = ListKey();
			StallList->ClearChildren();
			NearList->ClearChildren();
			const AWRBuildPiece* Q = Piece.Get();
			if (!Q) return;
			const FString Name = Me();
			int32 Shown = 0;
			for (int32 i = 0; i < Q->Roster.Num(); ++i)
			{
				const FStall S = Parse(Q->Roster[i]);
				if (S.Owner != Name) continue;
				++Shown;
				const FString Kind = KindName(S.Species);
				const FString Title = S.Name.IsEmpty() ? Kind : FString::Printf(TEXT("%s  ·  %s"), *S.Name, *Kind);
				const FString Line = FString::Printf(TEXT("Level %d  ·  %s  ·  %.0f%% health"), S.Level, S.Sex == 2 ? TEXT("female") : TEXT("male"), S.Health * 100.f);
				TSharedRef<SWidget> Button = SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SWRButton).Ghost(true).MinHeight(38.f * U).FontSize(12)
						.Text_Lambda([this, i]() { const AWRBuildPiece* P = Piece.Get(); return P && P->CallingOut == i ? LOCTEXT("StableComing", "Coming out...") : LOCTEXT("StableOut", "Call out"); })
						.OnClicked_Lambda([this, i]() { Send(FString::Printf(TEXT("out %d"), i)); return FReply::Handled(); })
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f * U, 0.f, 0.f)
					[
						SNew(SBox).Visibility_Lambda([this, i]() { const AWRBuildPiece* P = Piece.Get(); return P && P->CallingOut == i ? EVisibility::Visible : EVisibility::Collapsed; })
						[
							SNew(SWRBar).Height(5.f * U).Color(WRUI::Brass).Fraction_Lambda([this]()
							{
								const AWRBuildPiece* P = Piece.Get();
								const float Total = WRTune(TEXT("tame.stable.callSeconds"), 5.f);
								const float Now = P && P->GetWorld() ? P->GetWorld()->GetTimeSeconds() : 0.f;
								return P ? FMath::Clamp(1.f - (P->CallingOutAt - Now) / FMath::Max(0.1f, Total), 0.f, 1.f) : 0.f;
							})
						]
					];
				StallList->AddSlot().AutoHeight()[Row(Title, Line, S.Health, Button)];
			}
			if (Shown == 0) StallList->AddSlot().AutoHeight().Padding(0.f, 6.f * U)[WRUI::Text(LOCTEXT("StableEmpty", "None of yours. Bring a tame within 20 m and put it in a stall."), WRUI::Body(13), WRUI::Ink3, false, true)];

			const TArray<AWRCreature*> Tames = Near();
			for (const AWRCreature* C : Tames)
			{
				const FWRCreatureDef* D = C->GetDef();
				const FString Kind = D ? D->Name : FString();
				const FString Title = C->TameRep.Name.IsEmpty() ? Kind : FString::Printf(TEXT("%s  ·  %s"), *C->TameRep.Name, *Kind);
				const FString Why = C->GetRider() ? TEXT("being ridden") : C->TameRep.Pregnant > 0.f ? TEXT("carrying young: not now") : C->TameRep.bSoulDown ? TEXT("down: let it get up") : FString();
				const FString Line = FString::Printf(TEXT("Level %d  ·  %s  ·  %.0f m away%s"), C->TameRep.WildLevel + C->TameRep.Bond, C->TameRep.Sex == 2 ? TEXT("female") : TEXT("male"),
					FVector::Dist(C->GetActorLocation(), Q->GetActorLocation()) / 100.f, Why.IsEmpty() ? TEXT("") : *(TEXT("  ·  ") + Why));
				const TWeakObjectPtr<const AWRCreature> Weak(C);
				TSharedRef<SWidget> Button = SNew(SWRButton).Ghost(true).MinHeight(38.f * U).FontSize(12).Text(LOCTEXT("StablePut", "Put in a stall"))
					.OnClicked_Lambda([this, Weak]() { if (const AWRCreature* T = Weak.Get()) { const FVector L = T->GetActorLocation(); Send(FString::Printf(TEXT("store %.0f %.0f %.0f"), L.X, L.Y, L.Z)); } return FReply::Handled(); });
				NearList->AddSlot().AutoHeight()[Row(Title, Line, C->GetHealthFraction(), Button)];
			}
			if (Tames.Num() == 0) NearList->AddSlot().AutoHeight().Padding(0.f, 6.f * U)[WRUI::Text(LOCTEXT("StableNone", "No tame of yours within 20 m."), WRUI::Body(13), WRUI::Ink3, false, true)];
		}
	};
}

WR_REGISTER_SCREEN("stable", [](AWRPlayerController* PC) -> TSharedRef<SWidget> { return SNew(WRUIStableScreen::SStable).PC(PC); });

#undef LOCTEXT_NAMESPACE
