// The journal's Clan tab (O): until now clans only went through the console (WRClan create / invite / join...).
// Left: your clan (its name, leader, members online or not, the invites it has out, Leave) or, with none, Found a clan
// and the invites you've had (Accept / Turn down). Right: the players in this game, each with Invite for a leader,
// Invite by name for someone offline, and what a clan shares. It reads your player state's clan fields (only your
// own client gets them; FWRClans::RefreshPlayerStates fills them) and sends "clan <action> ...".

#include "WRPlayerUI.h"
#include "WRUIStyle.h"
#include "WRUIWidgets.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRGameState.h"
#include "WRAccess.h"
#include "WRTuning.h"
#include "Wildreach.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameStateBase.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"

#define LOCTEXT_NAMESPACE "WRUI"

// Named (not anonymous): unity builds put several UI files in one translation unit.
namespace WRClanTab
{
	using WRUI::U;

	/** A name to type (a new clan's, or someone to invite): a box over the screen that takes the keyboard (UI only, as
	 *  the chest's Rename does), so the letters don't open menus or walk you off. Enter sends it, Esc cancels. */
	class SWRClanNameBox : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SWRClanNameBox) {}
			SLATE_ARGUMENT(TWeakObjectPtr<AWRPlayerController>, PC)
			SLATE_ARGUMENT(FText, Title)
			SLATE_ARGUMENT(FText, Hint)
			SLATE_ARGUMENT(FText, Note)
			SLATE_ARGUMENT(FText, Action)
			SLATE_ARGUMENT(FString, Command)
			SLATE_ARGUMENT(bool, bOneWord)
		SLATE_END_ARGS()

		static TWeakPtr<SWRClanNameBox> Instance;

		void Construct(const FArguments& InArgs)
		{
			PC = InArgs._PC;
			Command = InArgs._Command;
			bOneWord = InArgs._bOneWord;
			static const FEditableTextBoxStyle Field = []()
			{
				FEditableTextBoxStyle S = FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");
				const FSlateBrush* Normal = WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.55f), 5.f, WRUI::Brass, 1.f);
				S.SetBackgroundImageNormal(*Normal).SetBackgroundImageHovered(*Normal).SetBackgroundImageFocused(*Normal).SetBackgroundImageReadOnly(*Normal);
				S.SetForegroundColor(FSlateColor(WRUI::Ink)).SetFocusedForegroundColor(FSlateColor(WRUI::Ink)).SetBackgroundColor(FSlateColor(FLinearColor::White));
				S.SetPadding(FMargin(12.f * U, 8.f * U));
				return S;
			}();
			ChildSlot.HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0.f, 160.f * U, 0.f, 0.f)
			[
				SNew(SBox).WidthOverride(480.f * U)
				[
					SNew(SWRPanel).Kind(EWRPanel::Stone).Rivets(true).BrassTrim(true).Padding(FMargin(18.f * U, 14.f * U))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f * U)[WRUI::Text(InArgs._Title, WRUI::Title(15), WRUI::Ink, true)]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SAssignNew(Box, SEditableTextBox).Style(&Field).Font(WRUI::Body(16, 700)).HintText(InArgs._Hint)
							.OnTextCommitted_Lambda([this](const FText&, ETextCommit::Type How)
							{
								if (How == ETextCommit::OnEnter) Save();
								else if (How == ETextCommit::OnCleared) Close();
							})
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f * U, 0.f, 0.f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[WRUI::Text(InArgs._Note, WRUI::Body(12, 500), WRUI::Ink3)]
							+ SHorizontalBox::Slot().AutoWidth().Padding(8.f * U, 0.f, 0.f, 0.f)
							[
								SNew(SWRButton).Ghost(true).FontSize(13).MinHeight(32.f * U).Text(LOCTEXT("ClanBoxCancel", "Cancel")).OnClicked_Lambda([this]() { Close(); return FReply::Handled(); })
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(8.f * U, 0.f, 0.f, 0.f)
							[
								SNew(SWRButton).Primary(true).FontSize(13).MinHeight(32.f * U).Text(InArgs._Action).OnClicked_Lambda([this]() { Save(); return FReply::Handled(); })
							]
						]
					]
				]
			];
		}

		static void Open(AWRPlayerController* InPC, const FText& Title, const FText& Hint, const FText& Note, const FText& Action, const FString& Command, bool bOneWord)
		{
			if (!InPC || !GEngine || !GEngine->GameViewport || Instance.IsValid()) return;
			TSharedRef<SWRClanNameBox> Widget = SNew(SWRClanNameBox).PC(InPC).Title(Title).Hint(Hint).Note(Note).Action(Action).Command(Command).bOneWord(bOneWord);
			Instance = Widget;
			GEngine->GameViewport->AddViewportWidgetContent(Widget, 90);
			FInputModeUIOnly Mode;
			Mode.SetWidgetToFocus(Widget->Box);
			InPC->SetInputMode(Mode);
			UE_LOG(LogWildreach, Log, TEXT("CLAN name box open (%s)"), *Command);
		}

		/** Focus goes to the box a frame after it opens: the button that opened it takes focus back after its click. */
		virtual void Tick(const FGeometry& Geo, const double Time, const float Delta) override
		{
			SCompoundWidget::Tick(Geo, Time, Delta);
			if (Frames++ < 2 && Box.IsValid()) FSlateApplication::Get().SetKeyboardFocus(Box, EFocusCause::SetDirectly);
		}

	private:
		void Save()
		{
			FString Name = Box.IsValid() ? Box->GetText().ToString().Replace(TEXT("|"), TEXT("")).TrimStartAndEnd() : FString();
			if (bOneWord)
			{
				int32 Space = INDEX_NONE;
				if (Name.FindChar(TEXT(' '), Space)) Name = Name.Left(Space);   // a player's name is one word
			}
			if (!Name.IsEmpty() && PC.IsValid()) PC->SendCommand(FString::Printf(TEXT("%s %s"), *Command, *Name.Left(24)));
			Close();
		}

		void Close()
		{
			if (bClosed) return;
			bClosed = true;
			const TSharedRef<SWRClanNameBox> KeepAlive = SharedThis(this);   // the viewport held the last other reference
			if (GEngine && GEngine->GameViewport) GEngine->GameViewport->RemoveViewportWidgetContent(KeepAlive);
			if (PC.IsValid()) { if (FWRPlayerUI* UI = PC->GetUI()) UI->ReapplyInputMode(); }   // the journal's cursor and input back
			Instance.Reset();
		}

		TWeakObjectPtr<AWRPlayerController> PC;
		TSharedPtr<SEditableTextBox> Box;
		FString Command;
		bool bOneWord = false;
		bool bClosed = false;
		int32 Frames = 0;
	};
	TWeakPtr<SWRClanNameBox> SWRClanNameBox::Instance;

	class SWRClanTab : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SWRClanTab) {}
			SLATE_ARGUMENT(TWeakObjectPtr<AWRPlayerController>, PC)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			PC = InArgs._PC;
			// The players list's search (cooks/build-wild-100players.md 3.7): one box for the tab's life, so typing
			// isn't lost when someone joins and the list redraws. A name or a clan; the pad's keyboard opens on it.
			SAssignNew(SearchBox, SEditableTextBox).Font(WRUI::Body(14, 600))
				.HintText(LOCTEXT("ClanSearchHint", "Find a player or clan"))
				.OnTextChanged_Lambda([this](const FText& Text) { Search = Text.ToString().TrimStartAndEnd(); });
			ChildSlot[SAssignNew(Content, SBox)];
			Rebuild();
		}

		virtual void Tick(const FGeometry& Geo, const double Time, const float Delta) override
		{
			SCompoundWidget::Tick(Geo, Time, Delta);
			const uint32 Hash = StateHash();
			if (Hash != LastHash)
			{
				Rebuild();
				WRUI::MeasureNow(*Content, Geo.Scale);   // (this is inside Slate's paint: see MeasureNow)
			}
			else if (ListHashNow() != ListHash && ListBox.IsValid())
			{
				// Someone came, went, changed clan or role, or the search changed: only the list redraws (its scroll
				// position and the search box stay).
				RebuildList();
				WRUI::MeasureNow(*ListBox, Geo.Scale);
			}
		}

	private:
		TWeakObjectPtr<AWRPlayerController> PC;
		TSharedPtr<SBox> Content;
		uint32 LastHash = 0;
		/** The players list (cooks/build-wild-100players.md 3.7): its own box, redrawn alone when people come and go. */
		TSharedPtr<SBox> ListBox;
		uint32 ListHash = 0;
		TSharedPtr<SEditableTextBox> SearchBox;
		FString Search;
		/** 0 everyone, 1 my clan, 2 staff (host and admins). */
		int32 Filter = 0;
		bool bConfirmLeave = false;
		/** The player whose Ban asks how long (24 h / for good / cancel), "" none. */
		FString BanAsk;

		const AWRPlayerState* PS() const { return PC.IsValid() ? PC->GetPlayerState<AWRPlayerState>() : nullptr; }
		FString Me() const { const AWRPlayerState* S = PS(); return S ? S->GetPlayerName() : FString(); }
		bool IsLeader() const { const AWRPlayerState* S = PS(); return S && !S->ClanName.IsEmpty() && S->ClanLeader == S->GetPlayerName(); }

		/** Everyone in this game but you, with their clan (from the replicated player states). */
		TArray<TPair<FString, FString>> Others() const
		{
			TArray<TPair<FString, FString>> Out;
			const AGameStateBase* GS = PC.IsValid() && PC->GetWorld() ? PC->GetWorld()->GetGameState() : nullptr;
			const FString Mine = Me();
			for (const APlayerState* P : GS ? GS->PlayerArray : TArray<TObjectPtr<APlayerState>>())
			{
				const AWRPlayerState* W = Cast<AWRPlayerState>(P);
				if (W && W->GetPlayerName() != Mine && !W->GetPlayerName().IsEmpty()) Out.Add({ W->GetPlayerName(), W->ClanName });
			}
			Out.Sort([](const TPair<FString, FString>& A, const TPair<FString, FString>& B) { return A.Key < B.Key; });
			return Out;
		}
		/** Everyone in this game but you, as their player states (their role and ping for the players list). */
		TArray<const AWRPlayerState*> OtherStates() const
		{
			TArray<const AWRPlayerState*> Out;
			const AGameStateBase* GS = PC.IsValid() && PC->GetWorld() ? PC->GetWorld()->GetGameState() : nullptr;
			const FString Mine = Me();
			for (const APlayerState* P : GS ? GS->PlayerArray : TArray<TObjectPtr<APlayerState>>())
			{
				const AWRPlayerState* W = Cast<AWRPlayerState>(P);
				if (W && W->GetPlayerName() != Mine && !W->GetPlayerName().IsEmpty()) Out.Add(W);
			}
			return Out;
		}
		const AWRPlayerState* StateOf(const FString& Name) const
		{
			for (const AWRPlayerState* W : OtherStates()) if (W->GetPlayerName() == Name) return W;
			return nullptr;
		}
		bool IsOnline(const FString& Name) const
		{
			if (Name == Me()) return true;
			return Others().ContainsByPredicate([&Name](const TPair<FString, FString>& O) { return O.Key == Name; });
		}

		uint32 StateHash() const
		{
			// (The filter's buttons show which is on, so a change redraws the tab; Ban's question is the list's.)
			uint32 H = HashCombine(GetTypeHash(bConfirmLeave), GetTypeHash(Filter) + 17);
			if (const AWRPlayerState* S = PS())
			{
				H = HashCombine(H, GetTypeHash(S->AccessRole));
				for (const FString& X : S->AdminBans) H = HashCombine(H, GetTypeHash(X) + 3);
				H = HashCombine(H, HashCombine(GetTypeHash(S->ClanName), GetTypeHash(S->ClanLeader)));
				for (const FString& X : S->ClanMembers) H = HashCombine(H, GetTypeHash(X));
				for (const FString& X : S->ClanInvited) H = HashCombine(H, GetTypeHash(X) + 1);
				for (const FString& X : S->ClanInvitesToMe) H = HashCombine(H, GetTypeHash(X) + 2);
			}
			// (Who else is here goes in ListHashNow: their comings and goings redraw only the list.) Your clan's
			// members online or not show on the left.
			if (const AWRPlayerState* S = PS())
			{
				TSet<FString> Here;
				for (const AWRPlayerState* W : OtherStates()) Here.Add(W->GetPlayerName());
				const FString Mine = Me();
				for (const FString& X : S->ClanMembers) H = HashCombine(H, GetTypeHash(X == Mine || Here.Contains(X)) + 11);
			}
			return H;
		}

		/** What the players list shows: everyone else (name, clan, role), Ban's question, the search and the filter. */
		uint32 ListHashNow() const
		{
			uint32 H = HashCombine(GetTypeHash(BanAsk), HashCombine(GetTypeHash(Search), GetTypeHash(Filter)));
			for (const AWRPlayerState* W : OtherStates())
			{
				H = HashCombine(H, HashCombine(GetTypeHash(W->GetPlayerName()), HashCombine(GetTypeHash(W->ClanName), GetTypeHash(W->AccessRole) + 7)));
			}
			if (const AWRPlayerState* S = PS())
			{
				for (const FString& X : S->ClanInvited) H = HashCombine(H, GetTypeHash(X) + 13);
			}
			return H;
		}

		void Send(const FString& Command) { if (PC.IsValid()) PC->SendCommand(Command); }

		void Rebuild()
		{
			LastHash = StateHash();
			Content->SetContent(
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 18.f * U, 0.f)
				[
					SNew(SWRPanel).Kind(EWRPanel::Inset).Padding(FMargin(22.f * U, 18.f * U))
					[
						SNew(SScrollBox) + SScrollBox::Slot()[Left()]
					]
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).WidthOverride(560.f * U)
					[
						SNew(SWRPanel).Kind(EWRPanel::Inset).Padding(FMargin(22.f * U, 18.f * U))
						[
							Right()
						]
					]
				]);
			RebuildList();
		}

		// ---------------------------------------------------------------- pieces

		static TSharedRef<SWidget> Heading(const FText& Text)
		{
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f * U, 0.f)[WRUI::Text(Text, WRUI::Caps(12), WRUI::Ink2)]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[SNew(SBox).HeightOverride(1.f)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity(WRUI::Alpha(WRUI::Brass2, 0.5f))]];
		}

		static TSharedRef<SWidget> Dot(bool bOn)
		{
			return SNew(SBox).WidthOverride(10.f * U).HeightOverride(10.f * U)[SNew(SImage).Image(WRUI::Box(bOn ? WRUI::Good : WRUI::Alpha(WRUI::Ink3, 0.6f), 5.f * U))];
		}

		/** A row: a dot (a player: on or not; none for a clan), the name, chips, then whatever buttons it has at the right. */
		TSharedRef<SWidget> Row(const FString& Name, bool bOnline, const TArray<TPair<FText, FLinearColor>>& Chips, TSharedRef<SWidget> Buttons, bool bDot = true) const
		{
			TSharedRef<SHorizontalBox> Line = SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, bDot ? 10.f * U : 0.f, 0.f)[bDot ? Dot(bOnline) : SNullWidget::NullWidget]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f * U, 0.f)[WRUI::Text(FText::FromString(Name), WRUI::Body(15, 700), bOnline ? WRUI::Ink : WRUI::Ink2, true)];
			for (const TPair<FText, FLinearColor>& C : Chips)
			{
				Line->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f * U, 0.f)[SNew(SWRChip).FontSize(11).Text(C.Key).Color(C.Value)];
			}
			Line->AddSlot().FillWidth(1.f)[SNullWidget::NullWidget];
			Line->AddSlot().AutoWidth().VAlign(VAlign_Center)[Buttons];
			return SNew(SBox).MinDesiredHeight(46.f * U).Padding(FMargin(12.f * U, 4.f * U))
				[
					SNew(SOverlay)
					+ SOverlay::Slot()[SNew(SImage).Image(WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.28f), 5.f, FLinearColor(0.f, 0.f, 0.f, 0.75f), 1.f))]
					+ SOverlay::Slot().Padding(FMargin(12.f * U, 4.f * U)).VAlign(VAlign_Center)[Line]
				];
		}

		static TSharedRef<SWidget> Button(const FText& Text, bool bPrimary, TFunction<void()> OnClick)
		{
			return SNew(SWRButton).Primary(bPrimary).Ghost(!bPrimary).FontSize(12).MinHeight(32.f * U).Text(Text).OnClicked_Lambda([OnClick]() { OnClick(); return FReply::Handled(); });
		}

		// ---------------------------------------------------------------- left: your clan, or found one / your invites

		TSharedRef<SWidget> Left()
		{
			TSharedRef<SVerticalBox> Col = SNew(SVerticalBox);
			const AWRPlayerState* S = PS();
			if (!S) return Col;
			const FString Mine = S->GetPlayerName();
			if (S->ClanName.IsEmpty())
			{
				Col->AddSlot().AutoHeight()[WRUI::Text(LOCTEXT("ClanNone", "No clan yet"), WRUI::Title(28), WRUI::Ink, true)];
				Col->AddSlot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 0.f)
				[
					WRUI::Text(LOCTEXT("ClanNoneWhy", "Found one and invite your friends, or accept an invite below. Clanmates share their bases and can't hurt each other."), WRUI::Body(14, 500), WRUI::Ink2, false, true)
				];
				Col->AddSlot().AutoHeight().HAlign(HAlign_Left).Padding(0.f, 16.f * U, 0.f, 0.f)
				[
					SNew(SWRButton).Primary(true).FontSize(14).MinHeight(44.f * U).Text(LOCTEXT("ClanFound", "Found a clan"))
					.OnClicked_Lambda([this]()
					{
						SWRClanNameBox::Open(PC.Get(), LOCTEXT("ClanFoundTitle", "Found a clan"), LOCTEXT("ClanFoundHint", "Its name"),
							LOCTEXT("ClanFoundNote", "1 to 24 letters; you lead it"), LOCTEXT("ClanFoundGo", "Found it"), TEXT("clan create"), false);
						return FReply::Handled();
					})
				];
				Col->AddSlot().AutoHeight().Padding(0.f, 28.f * U, 0.f, 10.f * U)[Heading(LOCTEXT("ClanInvitesHead", "Invites"))];
				if (S->ClanInvitesToMe.Num() == 0)
				{
					Col->AddSlot().AutoHeight()[WRUI::Text(LOCTEXT("ClanNoInvites", "No invites. A clan's leader can invite you from their Clan tab."), WRUI::Body(13, 500), WRUI::Ink3, false, true)];
				}
				for (int32 i = 0; i < S->ClanInvitesToMe.Num(); ++i)
				{
					const FString Clan = S->ClanInvitesToMe[i];
					const FString From = S->ClanInviteFrom.IsValidIndex(i) ? S->ClanInviteFrom[i] : FString();
					TSharedRef<SHorizontalBox> Buttons = SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 8.f * U, 0.f)[Button(LOCTEXT("ClanAccept", "Accept"), true, [this, Clan]() { Send(TEXT("clan join ") + Clan); })]
						+ SHorizontalBox::Slot().AutoWidth()[Button(LOCTEXT("ClanDecline", "Turn down"), false, [this, Clan]() { Send(TEXT("clan decline ") + Clan); })];
					Col->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f * U)
					[
						Row(Clan, true, { { FText::FromString(From.IsEmpty() ? FString(TEXT("an invite")) : FString::Printf(TEXT("from %s"), *From)), WRUI::Brass } }, Buttons, false)
					];
				}
			}
			else
			{
				const bool bLead = IsLeader();
				const int32 Limit = FMath::RoundToInt(WRTune(TEXT("server.clanSizeLimit"), 0.f));
				Col->AddSlot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 14.f * U, 0.f)[WRUI::Text(FText::FromString(S->ClanName), WRUI::Title(28), WRUI::Ink, true)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SWRChip).FontSize(12).Color(WRUI::Brass).Text(bLead ? LOCTEXT("ClanYouLead", "You lead it") : FText::FromString(FString::Printf(TEXT("Leader: %s"), *S->ClanLeader)))
					]
				];
				Col->AddSlot().AutoHeight().Padding(0.f, 4.f * U, 0.f, 0.f)
				[
					WRUI::Text(FText::FromString(Limit > 0 ? FString::Printf(TEXT("%d of %d members"), S->ClanMembers.Num(), Limit)
						: FString::Printf(TEXT("%d member%s"), S->ClanMembers.Num(), S->ClanMembers.Num() == 1 ? TEXT("") : TEXT("s"))), WRUI::Body(14, 500), WRUI::Ink2)
				];
				Col->AddSlot().AutoHeight().Padding(0.f, 20.f * U, 0.f, 10.f * U)[Heading(LOCTEXT("ClanMembersHead", "Members"))];
				for (const FString& M : S->ClanMembers)
				{
					TArray<TPair<FText, FLinearColor>> Chips;
					if (M == S->ClanLeader) Chips.Add({ LOCTEXT("ClanLeaderChip", "Leader"), WRUI::Brass });
					if (M == Mine) Chips.Add({ LOCTEXT("ClanYouChip", "You"), WRUI::Good });
					if (!IsOnline(M)) Chips.Add({ LOCTEXT("ClanOffline", "away"), WRUI::Ink3 });
					TSharedRef<SWidget> Buttons = bLead && M != Mine ? Button(LOCTEXT("ClanKick", "Remove"), false, [this, M]() { Send(TEXT("clan kick ") + M); }) : SNullWidget::NullWidget;
					Col->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f * U)[Row(M, IsOnline(M), Chips, Buttons)];
				}
				if (bLead && S->ClanInvited.Num() > 0)
				{
					Col->AddSlot().AutoHeight().Padding(0.f, 16.f * U, 0.f, 10.f * U)[Heading(LOCTEXT("ClanOutHead", "Invited, not joined yet"))];
					for (const FString& M : S->ClanInvited)
					{
						Col->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f * U)
						[
							Row(M, IsOnline(M), { { LOCTEXT("ClanInvitedChip", "invited"), WRUI::Warn } }, Button(LOCTEXT("ClanUninvite", "Take back"), false, [this, M]() { Send(TEXT("clan uninvite ") + M); }))
						];
					}
				}
				// Leave: one click asks, the second leaves.
				TSharedRef<SWidget> Leave = !bConfirmLeave
					? StaticCastSharedRef<SWidget>(SNew(SBox).HAlign(HAlign_Left)[Button(LOCTEXT("ClanLeave", "Leave clan"), false, [this]() { bConfirmLeave = true; })])
					: StaticCastSharedRef<SWidget>(SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f * U, 0.f)
						[
							WRUI::Text(FText::FromString(bLead && S->ClanMembers.Num() > 1
								? FString::Printf(TEXT("Leave %s? %s leads it after you."), *S->ClanName, *S->ClanMembers[S->ClanMembers[0] == Mine ? 1 : 0])
								: S->ClanMembers.Num() <= 1 ? FString::Printf(TEXT("Leave %s? It ends with you."), *S->ClanName) : FString::Printf(TEXT("Leave %s?"), *S->ClanName)), WRUI::Body(14, 700), WRUI::Warn)
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 8.f * U, 0.f)[Button(LOCTEXT("ClanLeaveYes", "Yes, leave"), true, [this]() { bConfirmLeave = false; Send(TEXT("clan leave")); })]
						+ SHorizontalBox::Slot().AutoWidth()[Button(LOCTEXT("ClanLeaveNo", "Stay"), false, [this]() { bConfirmLeave = false; })]);
				Col->AddSlot().AutoHeight().Padding(0.f, 22.f * U, 0.f, 0.f)[Leave];
			}
			// What it's for (here since build-wild-100players: the right column is the players list's).
			Col->AddSlot().AutoHeight().Padding(0.f, 28.f * U, 0.f, 10.f * U)[Heading(LOCTEXT("ClanSharesHead", "Clanmates share"))];
			static const TCHAR* const Lines[] = {
				TEXT("Bases: build in each other's claims; open each other's chests and doors, code locks too; use each other's stations; repair and feed the Hearth"),
				TEXT("Beds: wake at a clanmate's bed after you die"),
				TEXT("No fighting: clanmates can't hurt each other, and your tames leave them be"),
				TEXT("Raids: a raid on your claim warns all of you, and comes while any of you is home"),
			};
			for (const TCHAR* L : Lines)
			{
				Col->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f * U)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.f, 6.f * U, 10.f * U, 0.f)[SNew(SBox).WidthOverride(6.f * U).HeightOverride(6.f * U)[SNew(SImage).Image(WRUI::Box(WRUI::Brass, 3.f * U))]]
					+ SHorizontalBox::Slot().FillWidth(1.f)[WRUI::Text(FText::FromString(L), WRUI::Body(13, 500), WRUI::Ink2, false, true)]
				];
			}
			return Col;
		}

		// ---------------------------------------------------------------- right: who's here, invites, what a clan shares

		TSharedRef<SWidget> Right()
		{
			TSharedRef<SVerticalBox> Col = SNew(SVerticalBox);
			const AWRPlayerState* S = PS();
			if (!S) return Col;
			const bool bLead = IsLeader();
			const uint8 MyRole = S->AccessRole;
			const TWeakObjectPtr<AWRPlayerController> WeakPC = PC;
			// Header: how many are here of how many may be, who's waiting (the host and admins see it), and the host's
			// cap stepper on a dedicated world (cooks/build-wild-100players.md 3.7). Live: no redraw for a number.
			auto GameState = [WeakPC]() -> const AWRGameState* { return WeakPC.IsValid() && WeakPC->GetWorld() ? WeakPC->GetWorld()->GetGameState<AWRGameState>() : nullptr; };
			TSharedRef<SHorizontalBox> Head = SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Heading(LOCTEXT("ClanHereHead", "Players in this game"))]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f * U, 0.f, 0.f, 0.f)
				[
					WRUI::Text(TAttribute<FText>::CreateLambda([GameState, MyRole]()
					{
						const AWRGameState* GS = GameState();
						if (!GS) return FText::GetEmpty();
						const int32 Here = GS->PlayerArray.Num();
						FString Line = GS->PlayerCap > 0 ? FString::Printf(TEXT("%d / %d"), Here, GS->PlayerCap) : FString::FromInt(Here);
						if (MyRole > 0 && GS->Waiting > 0) Line += FString::Printf(TEXT("  \u00B7  %d waiting"), GS->Waiting);
						return FText::FromString(Line);
					}), WRUI::Title(15), WRUI::Ink)
				];
			if (MyRole == 2)
			{
				// The host's cap, on a dedicated world (a listen world's is set on its Hosting page before it opens).
				auto Step = [this, GameState](int32 By)
				{
					const AWRGameState* GS = GameState();
					if (!GS || GS->PlayerCap <= 0) return;
					const int32 Want = FMath::Clamp(GS->PlayerCap + By, WRAccess::MinPlayers(), WRAccess::MaxPlayersLimit(GS->bDedicatedWorld));
					if (Want != GS->PlayerCap) Send(FString::Printf(TEXT("admin cap %d"), Want));
				};
				Head->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(10.f * U, 0.f, 0.f, 0.f)
				[
					SNew(SBox).Visibility_Lambda([GameState]() { const AWRGameState* GS = GameState(); return GS && GS->bDedicatedWorld && GS->PlayerCap > 0 ? EVisibility::Visible : EVisibility::Collapsed; })
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f * U, 0.f)[WRUI::Text(LOCTEXT("ClanCap", "Cap"), WRUI::Caps(11), WRUI::Ink3)]
						+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(40.f * U)[Button(FText::FromString(TEXT("\u2212")), false, [Step]() { Step(-1); })]]
						+ SHorizontalBox::Slot().AutoWidth().Padding(4.f * U, 0.f, 0.f, 0.f)[SNew(SBox).WidthOverride(40.f * U)[Button(FText::FromString(TEXT("+")), false, [Step]() { Step(1); })]]
					]
				];
			}
			Col->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f * U)[Head];
			// Who runs the world (cooks/cookpass-multiplayer.md 3.4): the host and admins kick and ban from these rows.
			if (MyRole > 0)
			{
				Col->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f * U)
				[
					WRUI::Text(MyRole == 2 ? LOCTEXT("ClanYouHost", "You host this world: kick or ban from a player's row, and make admins who can too.")
						: LOCTEXT("ClanYouAdmin", "You're an admin of this world: kick or ban from a player's row."), WRUI::Body(13, 500), WRUI::Brass, false, true)
				];
			}
			// Find: the search box and Everyone / My clan / Staff.
			auto FilterButton = [this](const FText& Text, int32 Which)
			{
				return SNew(SBox).Padding(FMargin(6.f * U, 0.f, 0.f, 0.f))
					[
						SNew(SWRButton).FontSize(11).MinHeight(32.f * U).Text(Text).Primary(Filter == Which).Ghost(Filter != Which)
						.OnClicked_Lambda([this, Which]() { Filter = Which; return FReply::Handled(); })
					];
			};
			Col->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f * U)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[SearchBox.ToSharedRef()]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[FilterButton(LOCTEXT("ClanFilterAll", "Everyone"), 0)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[FilterButton(LOCTEXT("ClanFilterClan", "My clan"), 1)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[FilterButton(LOCTEXT("ClanFilterStaff", "Staff"), 2)]
			];
			// The list (and, for admins, who's banned), in its own scroll filling the column.
			Col->AddSlot().FillHeight(1.f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()[SAssignNew(ListBox, SBox)]
			];
			if (bLead)
			{
				Col->AddSlot().AutoHeight().HAlign(HAlign_Left).Padding(0.f, 10.f * U, 0.f, 0.f)
				[
					SNew(SWRButton).Ghost(true).FontSize(13).MinHeight(36.f * U).Text(LOCTEXT("ClanInviteByName", "Invite by name"))
					.OnClicked_Lambda([this]()
					{
						SWRClanNameBox::Open(PC.Get(), LOCTEXT("ClanInviteTitle", "Invite a player"), LOCTEXT("ClanInviteHint", "Their name, as they play"),
							LOCTEXT("ClanInviteNote", "They can accept it later, when they're on"), LOCTEXT("ClanInviteGo", "Invite"), TEXT("clan invite"), true);
						return FReply::Handled();
					})
				];
			}
			else if (!S->ClanName.IsEmpty())
			{
				Col->AddSlot().AutoHeight().Padding(0.f, 10.f * U, 0.f, 0.f)
				[
					WRUI::Text(FText::FromString(FString::Printf(TEXT("%s invites new members."), *S->ClanLeader)), WRUI::Body(13, 500), WRUI::Ink3, false, true)
				];
			}
			return Col;
		}

		/** The players list's rows: the filter and search applied; your clanmates first, then the host and admins,
		 *  then everyone by name. */
		void RebuildList()
		{
			ListHash = ListHashNow();
			if (!ListBox.IsValid()) return;
			TSharedRef<SVerticalBox> Col = SNew(SVerticalBox);
			const AWRPlayerState* S = PS();
			if (!S)
			{
				ListBox->SetContent(Col);
				return;
			}
			const bool bLead = IsLeader();
			const uint8 MyRole = S->AccessRole;
			TArray<const AWRPlayerState*> All = OtherStates();
			const int32 Total = All.Num();
			const FString Needle = Search.ToLower();
			All.RemoveAll([&](const AWRPlayerState* W)
			{
				if (Filter == 1 && (S->ClanName.IsEmpty() || W->ClanName != S->ClanName)) return true;
				if (Filter == 2 && W->AccessRole == 0) return true;
				return !Needle.IsEmpty() && !W->GetPlayerName().ToLower().Contains(Needle) && !W->ClanName.ToLower().Contains(Needle);
			});
			const FString MyClan = S->ClanName;
			All.Sort([&MyClan](const AWRPlayerState& A, const AWRPlayerState& B)
			{
				const int32 RankA = (!MyClan.IsEmpty() && A.ClanName == MyClan) ? 0 : A.AccessRole > 0 ? 1 : 2;
				const int32 RankB = (!MyClan.IsEmpty() && B.ClanName == MyClan) ? 0 : B.AccessRole > 0 ? 1 : 2;
				if (RankA != RankB) return RankA < RankB;
				return A.GetPlayerName() < B.GetPlayerName();
			});
			if (Total == 0)
			{
				Col->AddSlot().AutoHeight()[WRUI::Text(LOCTEXT("ClanNobody", "Nobody else is here. Friends join with your IP from the main menu (and the world's password, if it has one)."), WRUI::Body(13, 500), WRUI::Ink3, false, true)];
			}
			else if (All.Num() != Total)
			{
				Col->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f * U)
				[
					WRUI::Text(All.Num() == 0 ? LOCTEXT("ClanNoMatch", "Nobody here matches.") : FText::Format(LOCTEXT("ClanShowing", "Showing {0} of {1}"), All.Num(), Total), WRUI::Body(12, 600), WRUI::Ink3)
				];
			}
			for (const AWRPlayerState* Them : All)
			{
				const FString Name = Them->GetPlayerName();
				const FString TheirClan = Them->ClanName;
				const uint8 TheirRole = Them->AccessRole;
				TArray<TPair<FText, FLinearColor>> Chips;
				if (TheirRole == 2) Chips.Add({ LOCTEXT("ClanHostChip", "Host"), WRUI::Brass });
				else if (TheirRole == 1) Chips.Add({ LOCTEXT("ClanAdminChip", "Admin"), WRUI::Brass });
				if (!TheirClan.IsEmpty()) Chips.Add({ FText::FromString(TheirClan == S->ClanName ? FString(TEXT("your clan")) : TheirClan), TheirClan == S->ClanName ? WRUI::Good : WRUI::Ink2 });
				TSharedRef<SHorizontalBox> Buttons = SNew(SHorizontalBox);
				// Its ping, live: green under 80 ms, amber under 160, red past that.
				const TWeakObjectPtr<const AWRPlayerState> WeakThem(Them);
				Buttons->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f * U, 0.f)
				[
					SNew(SWRChip).FontSize(11)
					.Text_Lambda([WeakThem]() { return FText::FromString(WeakThem.IsValid() ? FString::Printf(TEXT("%d ms"), FMath::RoundToInt(WeakThem->GetPingInMilliseconds())) : FString(TEXT("-"))); })
					.Color_Lambda([WeakThem]()
					{
						const float Ms = WeakThem.IsValid() ? WeakThem->GetPingInMilliseconds() : 999.f;
						return Ms < WRTune(TEXT("access.pingGoodMs"), 80.f) ? WRUI::Good : Ms < WRTune(TEXT("access.pingFairMs"), 160.f) ? WRUI::Warn : WRUI::Bad;
					})
				];
				if (BanAsk == Name)
				{
					// How long: 24 h, for good, or not after all.
					Buttons->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f * U, 0.f)[WRUI::Text(LOCTEXT("ClanBanHow", "Ban for"), WRUI::Body(13, 700), WRUI::Warn)];
					Buttons->AddSlot().AutoWidth().Padding(0.f, 0.f, 6.f * U, 0.f)[Button(LOCTEXT("ClanBan24", "24 h"), true, [this, Name]() { BanAsk.Reset(); Send(FString::Printf(TEXT("admin ban %s 24"), *Name)); })];
					Buttons->AddSlot().AutoWidth().Padding(0.f, 0.f, 6.f * U, 0.f)[Button(LOCTEXT("ClanBanGood", "For good"), true, [this, Name]() { BanAsk.Reset(); Send(FString::Printf(TEXT("admin ban %s 0"), *Name)); })];
					Buttons->AddSlot().AutoWidth()[Button(LOCTEXT("ClanBanCancel", "Cancel"), false, [this]() { BanAsk.Reset(); })];
				}
				else
				{
					if (bLead && TheirClan.IsEmpty())
					{
						Buttons->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f * U, 0.f)
						[
							S->ClanInvited.Contains(Name) ? StaticCastSharedRef<SWidget>(WRUI::Text(LOCTEXT("ClanInvitedNote", "Invited"), WRUI::Body(13, 700), WRUI::Warn))
								: Button(LOCTEXT("ClanInvite", "Invite"), true, [this, Name]() { Send(TEXT("clan invite ") + Name); })
						];
					}
					// The host over anyone; an admin over players.
					if (MyRole > TheirRole)
					{
						Buttons->AddSlot().AutoWidth().Padding(0.f, 0.f, 6.f * U, 0.f)[Button(LOCTEXT("ClanKickPlayer", "Kick"), false, [this, Name]() { Send(TEXT("admin kick ") + Name); })];
						Buttons->AddSlot().AutoWidth().Padding(0.f, 0.f, 6.f * U, 0.f)[Button(LOCTEXT("ClanBanPlayer", "Ban"), false, [this, Name]() { BanAsk = Name; })];
					}
					if (MyRole == 2)
					{
						Buttons->AddSlot().AutoWidth()[Button(TheirRole == 1 ? LOCTEXT("ClanDeop", "Remove admin") : LOCTEXT("ClanOp", "Make admin"), false,
							[this, Name, TheirRole]() { Send(FString::Printf(TEXT("admin %s %s"), TheirRole == 1 ? TEXT("deop") : TEXT("op"), *Name)); })];
					}
				}
				Col->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f * U)[Row(Name, true, Chips, Buttons)];
			}
			// Admins: who's banned, and Unban.
			if (MyRole > 0 && S->AdminBans.Num() > 0)
			{
				Col->AddSlot().AutoHeight().Padding(0.f, 16.f * U, 0.f, 10.f * U)[Heading(LOCTEXT("ClanBannedHead", "Banned"))];
				for (const FString& Line : S->AdminBans)
				{
					FString Name, Left;
					if (!Line.Split(TEXT("\t"), &Name, &Left)) Name = Line;
					Col->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f * U)
					[
						Row(Name, false, { { FText::FromString(Left), WRUI::Bad } }, Button(LOCTEXT("ClanUnban", "Unban"), false, [this, Name]() { Send(TEXT("admin unban ") + Name); }))
					];
				}
			}
			ListBox->SetContent(Col);
		}
	};
}

WR_REGISTER_JOURNAL_TAB("clan", [](AWRPlayerController* PC) -> TSharedRef<SWidget> { return SNew(WRClanTab::SWRClanTab).PC(PC); });

#undef LOCTEXT_NAMESPACE
