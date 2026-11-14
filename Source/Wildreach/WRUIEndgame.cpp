// The endgame's screens (cooks/build-endgame.md 2.11): the boss altar's pages (Path, Return, Forge soul), the Heron's
// draught, the war-hall's gate card, a record tablet's board, a monument's plaque, the journal's Deeds tab
// (Achievements, Records, Titles, Cosmetics, Marks) and the achievement toast. Every control is a button or a hold
// button (both answer the mouse and the pad: the pad's A is a synthesised left click, held while A is held), B / Esc
// backs out, F1 shows the controls, and no page is ever blank. Screens only send "deeds ..." / "warhall ..." commands;
// the server checks and acts (WRDeeds, WRWarHall, WRRecords).

#include "WRUIEndgame.h"
#include "WRPlayerUI.h"
#include "WRAltar.h"
#include "WRArtifacts.h"
#include "WRBuilding.h"
#include "WRCreature.h"
#include "WRDeeds.h"
#include "WRDeedsInternal.h"
#include "WRInventory.h"
#include "WRPathComponent.h"
#include "WRPathTypes.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRPortals.h"
#include "WRRecords.h"
#include "WRSound.h"
#include "WRTuning.h"
#include "WRUIStyle.h"
#include "WRUIWidgets.h"
#include "WRWarHall.h"
#include "Misc/DateTime.h"
#include "GameFramework/GameStateBase.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBackgroundBlur.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"

#define LOCTEXT_NAMESPACE "WRUI"

namespace WRUIEndgameScreens
{
	using WRUI::U;
	using namespace WRDeedsImpl;

	// ------------------------------------------------------------------------------------------------ the kit

	FLinearColor PathColour(FName Path)
	{
		return Path == TEXT("fire") ? WRUI::Ember : Path == TEXT("frost") ? WRUI::Frost : WRUI::Brass;
	}
	FString PathName(FName Path)
	{
		const FWRPathDef* D = FWRPathRegistry::Get().Find(Path);
		return D ? D->Name : Path.ToString();
	}
	FString CreatureName(FName Id, bool bShort = true)
	{
		const FWRCreatureDef* D = FWRCreatureRegistry::Get().Find(Id);
		FString N = D ? D->Name : Id.ToString();
		if (bShort) N.Split(TEXT(","), &N, nullptr);
		return N;
	}
	FString ItemName(FName Id)
	{
		const FWRItemDef* D = FWRItemRegistry::Get().Find(Id);
		return D ? D->Name : Id.ToString();
	}
	FString DateOf(double Unix)
	{
		return Unix > 0.0 ? FDateTime::FromUnixTimestamp(int64(Unix)).ToString(TEXT("%d %b %Y")) : FString();
	}
	TSharedRef<SWidget> Heading(const TAttribute<FText>& T) { return WRUI::Text(T, WRUI::Caps(11), WRUI::Ink3); }
	TSharedRef<SWidget> Line(const TAttribute<FText>& T, const FLinearColor& C = WRUI::Ink2, int32 Size = 13, int32 Weight = 500) { return WRUI::Text(T, WRUI::Body(Size, Weight), C, false, true); }

	TSharedRef<SWidget> Button(const TAttribute<FText>& Text, TFunction<void()> OnClick, bool bPrimary, const TAttribute<bool>& Enabled = true, float Height = 40.f)
	{
		return SNew(SBox).HeightOverride(Height * U)
			[
				SNew(SWRButton).Text(Text).Primary(bPrimary).FontSize(13).IsEnabled(Enabled)
				.OnClicked_Lambda([OnClick]() { OnClick(); return FReply::Handled(); })
			];
	}

	/** A window: a dimmed, blurred view behind, a carved stone frame, a leather title strap with an accent, Esc / Back. */
	TSharedRef<SWidget> Frame(const TAttribute<FText>& Title, const TAttribute<FText>& Sub, TSharedRef<SWidget> Body, float Width, const TAttribute<FLinearColor>& Accent)
	{
		return SNew(SOverlay)
			+ SOverlay::Slot()[SNew(SBackgroundBlur).BlurStrength(6.f)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.45f))]]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(Width * U).MaxDesiredHeight(860.f * U)
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
										SNew(SBox).WidthOverride(14.f * U).HeightOverride(40.f * U)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity_Lambda([Accent]() { return Accent.Get(); })]
									]
									+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
									[
										SNew(SVerticalBox)
										+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(Title, WRUI::Title(19), WRUI::Ink, true)]
										+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(Sub, WRUI::Body(12, 500), WRUI::Ink2, true)]
									]
									+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 7.f * U, 0.f)[SNew(SWRKeyCap).FontSize(11).Text(LOCTEXT("EndEsc", "Esc"))]
									+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(LOCTEXT("EndBackCap", "Back"), WRUI::Body(13, 500), WRUI::Ink2, true)]
								]
							]
						]
						+ SVerticalBox::Slot().FillHeight(1.f).Padding(FMargin(18.f * U, 16.f * U, 18.f * U, 0.f))
						[
							SNew(SScrollBox) + SScrollBox::Slot()[Body]
						]
					]
				]
			];
	}

	/** A pickable card: an inset panel that warms its rim when chosen (mouse click or the pad's A). */
	TSharedRef<SWidget> Card(TSharedRef<SWidget> Content, TFunction<void()> OnPick, TFunction<bool()> IsPicked, const TAttribute<bool>& Enabled = true, const FLinearColor& Accent = WRUI::Brass)
	{
		return SNew(SButton).ButtonStyle(FCoreStyle::Get(), "NoBorder").Cursor(EMouseCursor::Hand).IsEnabled(Enabled).ContentPadding(0.f)
			.OnClicked_Lambda([OnPick]() { OnPick(); return FReply::Handled(); })
			[
				SNew(SBorder).Padding(FMargin(14.f * U, 12.f * U))
				.BorderImage_Lambda([IsPicked, Accent]() { const bool b = IsPicked(); return WRUI::Box(b ? FLinearColor(0.11f, 0.075f, 0.04f) : FLinearColor(0.055f, 0.05f, 0.048f), 6.f, b ? Accent : WRUI::Rim, b ? 2.f : 1.f); })
				[Content]
			];
	}

	/** A boss's sigil: its 9 rows of '#' woven as squares (the tablets, banners and the gate's lintel draw the same grid). */
	class SSigil : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SSigil) : _Size(54.f), _Colour(FLinearColor::White) {}
			SLATE_ARGUMENT(FName, Id)
			SLATE_ARGUMENT(float, Size)
			SLATE_ARGUMENT(FLinearColor, Colour)
		SLATE_END_ARGS()
		void Construct(const FArguments& A) { Rows = WRDeeds::Sigil(A._Id); Size = A._Size * U; Colour = A._Colour; }
		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(Size, Size); }
		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override
		{
			FSlateDrawElement::MakeBox(Out, Layer, Geo.ToPaintGeometry(), WRUI::Box(FLinearColor(0.035f, 0.03f, 0.028f), 4.f, WRUI::Brass2, 1.f));
			int32 Cols = 0;
			for (const FString& R : Rows) Cols = FMath::Max(Cols, R.Len());
			if (Rows.Num() == 0 || Cols == 0) return Layer;
			const float Pad = Size * 0.1f;
			const float Cell = (Size - 2.f * Pad) / FMath::Max(Rows.Num(), Cols);
			const FVector2D Off((Size - Cell * Cols) * 0.5f, (Size - Cell * Rows.Num()) * 0.5f);
			for (int32 y = 0; y < Rows.Num(); ++y)
			{
				for (int32 x = 0; x < Rows[y].Len(); ++x)
				{
					if (Rows[y][x] != TEXT('#')) continue;
					FSlateDrawElement::MakeBox(Out, Layer + 1, Geo.ToPaintGeometry(FVector2f(Cell * 0.86f, Cell * 0.86f), FSlateLayoutTransform(FVector2f(Off.X + x * Cell, Off.Y + y * Cell))), WRUI::White(), ESlateDrawEffect::None, Colour);
				}
			}
			return Layer + 1;
		}
	private:
		TArray<FString> Rows;
		float Size = 54.f;
		FLinearColor Colour;
	};

	/** Hold to act: a ring fills while the left button (or the pad's A) is held; letting go early cancels. */
	class SHoldButton : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SHoldButton) : _Seconds(1.5f), _Accent(FLinearColor::White) {}
			SLATE_ATTRIBUTE(FText, Text)
			SLATE_ATTRIBUTE(bool, Ready)
			SLATE_ARGUMENT(float, Seconds)
			SLATE_ARGUMENT(FLinearColor, Accent)
			SLATE_EVENT(FSimpleDelegate, OnDone)
		SLATE_END_ARGS()

		void Construct(const FArguments& A)
		{
			Ready = A._Ready;
			Seconds = FMath::Max(0.2f, A._Seconds);
			Accent = A._Accent;
			OnDone = A._OnDone;
			ChildSlot
			[
				SNew(SBox).HeightOverride(54.f * U)
				[
					SNew(SBorder).Padding(FMargin(14.f * U, 0.f))
					.BorderImage_Lambda([this]() { const bool bOn = Ready.Get(); return WRUI::Box(bOn ? (bHeld ? FLinearColor(0.16f, 0.1f, 0.05f) : (IsHovered() ? FLinearColor(0.12f, 0.08f, 0.045f) : FLinearColor(0.09f, 0.065f, 0.04f))) : FLinearColor(0.05f, 0.048f, 0.046f), 6.f, bOn ? WRUI::Brass : WRUI::Rim, bOn ? 2.f : 1.f); })
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f * U, 0.f)
						[
							SNew(SBox).WidthOverride(34.f * U).HeightOverride(34.f * U)
						]
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
						[
							WRUI::Text(A._Text, WRUI::Title(15), TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(Ready.Get() ? WRUI::Ink : WRUI::Ink3); }), false, true)
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return Ready.Get() ? FText::Format(LOCTEXT("EndHoldS", "hold {0} s"), FText::AsNumber(Seconds)) : FText::GetEmpty(); }), WRUI::Caps(11), WRUI::Ink3)
						]
					]
				]
			];
		}
		virtual FReply OnMouseButtonDown(const FGeometry& Geo, const FPointerEvent& E) override
		{
			if (E.GetEffectingButton() != EKeys::LeftMouseButton || !Ready.Get()) return FReply::Unhandled();
			bHeld = true;
			bFired = false;
			HeldAt = FPlatformTime::Seconds();
			return FReply::Handled().CaptureMouse(SharedThis(this));
		}
		virtual FReply OnMouseButtonUp(const FGeometry& Geo, const FPointerEvent& E) override
		{
			if (E.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
			bHeld = false;
			return FReply::Handled().ReleaseMouseCapture();
		}
		virtual void OnMouseCaptureLost(const FCaptureLostEvent& E) override { bHeld = false; }
		virtual void Tick(const FGeometry& Geo, const double Now, const float Dt) override
		{
			SCompoundWidget::Tick(Geo, Now, Dt);
			if (bHeld && !Ready.Get()) bHeld = false;
			if (bHeld && !bFired && Fraction() >= 1.f)
			{
				bFired = true;
				bHeld = false;
				OnDone.ExecuteIfBound();
			}
		}
		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override
		{
			int32 L = SCompoundWidget::OnPaint(Args, Geo, Cull, Out, Layer, Style, bParentEnabled);
			// The ring at the left: dark track, the held part in the accent, from twelve o'clock clockwise.
			const float R = 15.f * U, T = 4.f * U;
			const FVector2D C(14.f * U + 17.f * U, Geo.GetLocalSize().Y * 0.5f);
			const auto Arc = [&](float To, const FLinearColor& Col, float Thick, int32 At)
			{
				TArray<FVector2D> Pts;
				const int32 N = FMath::Max(2, FMath::CeilToInt(To / 6.f));
				for (int32 i = 0; i <= N; ++i)
				{
					const float Ang = FMath::DegreesToRadians(To * i / N);
					Pts.Add(C + FVector2D(FMath::Sin(Ang), -FMath::Cos(Ang)) * R);
				}
				FSlateDrawElement::MakeLines(Out, At, Geo.ToPaintGeometry(), Pts, ESlateDrawEffect::None, Col, true, Thick);
			};
			Arc(360.f, FLinearColor(0.f, 0.f, 0.f, 0.8f), T + 2.f, L + 1);
			const float F = bHeld ? Fraction() : 0.f;
			if (F > 0.005f) Arc(360.f * F, Accent, T, L + 2);
			return L + 2;
		}
	private:
		TAttribute<bool> Ready;
		float Seconds = 1.5f;
		FLinearColor Accent;
		FSimpleDelegate OnDone;
		bool bHeld = false;
		bool bFired = false;
		double HeldAt = 0.0;
		float Fraction() const { return FMath::Clamp(float(FPlatformTime::Seconds() - HeldAt) / Seconds, 0.f, 1.f); }
	};

	/** Three knots, gold for each mark a path carries (the prestige marks). */
	TSharedRef<SWidget> Knots(TFunction<int32()> Count)
	{
		TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
		for (int32 i = 0; i < 3; ++i)
		{
			Row->AddSlot().AutoWidth().Padding(0.f, 0.f, 6.f * U, 0.f)
			[
				SNew(SBox).WidthOverride(16.f * U).HeightOverride(16.f * U)
				[
					SNew(SImage).Image_Lambda([Count, i]() { const bool b = Count() > i; return WRUI::Box(b ? FLinearColor(0.85f, 0.62f, 0.18f) : FLinearColor(0.06f, 0.055f, 0.05f), 8.f * U, b ? FLinearColor(1.f, 0.85f, 0.45f) : WRUI::Brass2, 1.5f); })
				]
			];
		}
		return Row;
	}

	/** The local player's state and deeds. */
	AWRPlayerState* StateOf(const TWeakObjectPtr<AWRPlayerController>& PC) { return PC.IsValid() ? PC->GetPlayerState<AWRPlayerState>() : nullptr; }
	UWRDeedsComponent* DeedsOf(const TWeakObjectPtr<AWRPlayerController>& PC) { return UWRDeedsComponent::For(StateOf(PC)); }
	void Send(const TWeakObjectPtr<AWRPlayerController>& PC, const FString& C) { if (AWRPlayerController* P = PC.Get()) P->SendCommand(C); }
	/** A world screen closes when you walk away from its thing (or it's gone). */
	void CloseIfFar(const TWeakObjectPtr<AWRPlayerController>& PC, FName Screen, const AActor* Thing, float Reach)
	{
		AWRPlayerController* P = PC.Get();
		if (!P || P->GetMenu() != EWRMenu::World || P->WorldScreen != Screen) return;
		const APawn* Me = P->GetPawn();
		if (!Thing || !Me || FVector::Dist(Thing->GetActorLocation(), Me->GetActorLocation()) > Reach) P->SetMenu(EWRMenu::None);
	}

	// ------------------------------------------------------------------------------------------------ a kill-time board

	/** One board in full (a boss or a raid): band picker, the artifact column, seasons, the ten best. */
	class SBoardView : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SBoardView) {}
			SLATE_ARGUMENT(TWeakObjectPtr<AWRPlayerController>, PC)
			SLATE_ARGUMENT(FName, Board)
		SLATE_END_ARGS()

		void Construct(const FArguments& A)
		{
			PC = A._PC;
			Board = A._Board;
			Box = SNew(SVerticalBox);
			ChildSlot[Box.ToSharedRef()];
			Rebuild();
		}
		virtual void Tick(const FGeometry& Geo, const double Now, const float Dt) override
		{
			SCompoundWidget::Tick(Geo, Now, Dt);
			const TSharedPtr<FJsonObject> Data = WRRecords::ClientBoards(Board);
			if (Data != Shown) Rebuild();
		}
	private:
		TWeakObjectPtr<AWRPlayerController> PC;
		FName Board;
		TSharedPtr<SVerticalBox> Box;
		TSharedPtr<FJsonObject> Shown;
		FString Band;          // empty: every band
		bool bArtifacts = false;

		void Ask(int32 Season) { Send(PC, FString::Printf(TEXT("deeds records %s %d"), *Board.ToString(), Season)); }

		void Rebuild()
		{
			Box->ClearChildren();
			Shown = WRRecords::ClientBoards(Board);
			if (!Shown.IsValid())
			{
				Box->AddSlot().AutoHeight()[Line(LOCTEXT("EndReading", "Reading the stone..."), WRUI::Ink3, 14)];
				return;
			}
			const int32 Season = FMath::RoundToInt(DNum(Shown, TEXT("season"), 1.f));
			const int32 Current = FMath::RoundToInt(DNum(Shown, TEXT("current"), 1.f));
			const TSharedPtr<FJsonObject> B = DChild(Shown, TEXT("board"));
			// The season (older ones stay readable, never written again).
			if (Current > 1)
			{
				Box->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f * U)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 8.f * U, 0.f)[SNew(SBox).WidthOverride(46.f * U)[Button(FText::FromString(TEXT("<")), [this, Season]() { Ask(Season - 1); }, false, Season > 1, 34.f)]]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(FText::FromString(FString::Printf(TEXT("Season %d%s"), Season, Season == Current ? TEXT("  (this one)") : TEXT("  (closed: read only)"))), WRUI::Body(14, 700), WRUI::Ink)]
					+ SHorizontalBox::Slot().AutoWidth().Padding(8.f * U, 0.f, 0.f, 0.f)[SNew(SBox).WidthOverride(46.f * U)[Button(FText::FromString(TEXT(">")), [this, Season]() { Ask(Season + 1); }, false, Season < Current, 34.f)]]
				];
			}
			if (const TSharedPtr<FJsonObject> First = DChild(B, TEXT("first")))
			{
				TArray<FString> N;
				if (const TArray<TSharedPtr<FJsonValue>>* L = DArr(First, TEXT("names"))) for (const auto& V : *L) N.Add(V->AsString());
				Box->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f * U)[Line(FText::FromString(FString::Printf(TEXT("First on this world: %s, %s  ·  %d kills"), *FString::Join(N, TEXT(", ")), *DateOf(DNum(First, TEXT("at"), 0.f)), FMath::RoundToInt(DNum(B, TEXT("kills"), 0.f)))), WRUI::Brass, 13, 600)];
			}
			// The bands (tames count as members, as they do for the boss's health) and the artifact column.
			TSharedRef<SWrapBox> Picker = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(6.f * U, 6.f * U));
			TArray<FString> Keys = { FString() };
			Keys.Append(WRRecords::Bands());
			for (const FString& K : Keys)
			{
				Picker->AddSlot()[SNew(SBox).MinDesiredWidth(84.f * U)[Button(FText::FromString(K.IsEmpty() ? FString(TEXT("Every size")) : WRRecords::BandName(K)), [this, K]() { Band = K; Rebuild(); }, Band == K, true, 34.f)]];
			}
			Picker->AddSlot()[SNew(SBox).MinDesiredWidth(140.f * U)[Button(bArtifacts ? LOCTEXT("EndArtOn", "Artifact runs") : LOCTEXT("EndArtOff", "Without artifacts"), [this]() { bArtifacts = !bArtifacts; Rebuild(); }, bArtifacts, true, 34.f)]];
			Box->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 12.f * U)[Picker];
			// The best, fastest first.
			TArray<TSharedPtr<FJsonObject>> Entries;
			if (const TSharedPtr<FJsonObject> Bands = DChild(B, TEXT("bands")))
			{
				for (const auto& P : Bands->Values)
				{
					const FString PKey(*P.Key);
					const bool bAl = PKey.EndsWith(TEXT(".al"));
					const FString K = bAl ? PKey.LeftChop(3) : PKey;
					if (bAl != bArtifacts || (!Band.IsEmpty() && K != Band)) continue;
					const TArray<TSharedPtr<FJsonValue>>* L = nullptr;
					if (P.Value->TryGetArray(L)) for (const auto& V : *L) Entries.Add(V->AsObject());
				}
			}
			Entries.Sort([](const TSharedPtr<FJsonObject>& X, const TSharedPtr<FJsonObject>& Y) { return DNum(X, TEXT("secs"), 1e9f) < DNum(Y, TEXT("secs"), 1e9f); });
			if (Entries.Num() == 0)
			{
				Box->AddSlot().AutoHeight()[Line(B.IsValid() ? LOCTEXT("EndNoBand", "No kills in this column yet.") : LOCTEXT("EndNoKills", "No kills yet. The first kill here is cut into the stone."), WRUI::Ink2, 14)];
				return;
			}
			const FString Me = PC.IsValid() && PC->PlayerState ? PC->PlayerState->GetPlayerName() : FString();
			const int32 Keep = FMath::RoundToInt(WRTune(TEXT("records.keep"), 10.f));
			for (int32 i = 0; i < Entries.Num() && i < Keep; ++i)
			{
				const TSharedPtr<FJsonObject>& E = Entries[i];
				TArray<FString> N;
				if (const TArray<TSharedPtr<FJsonValue>>* L = DArr(E, TEXT("names"))) for (const auto& V : *L) N.Add(V->AsString());
				const bool bMine = N.Contains(Me);
				const int32 Size = FMath::RoundToInt(DNum(E, TEXT("size"), 1.f));
				const int32 AL = FMath::RoundToInt(DNum(E, TEXT("al"), 0.f));
				Box->AddSlot().AutoHeight().Padding(0.f, 2.f * U)
				[
					SNew(SBorder).Padding(FMargin(12.f * U, 7.f * U)).BorderImage(WRUI::Box(bMine ? FLinearColor(0.11f, 0.08f, 0.045f) : FLinearColor(0.05f, 0.046f, 0.044f), 4.f, bMine ? WRUI::Brass2 : FLinearColor::Transparent, 1.f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(34.f * U)[WRUI::Text(FText::AsNumber(i + 1), WRUI::Title(16), i == 0 ? WRUI::Brass : WRUI::Ink2)]]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(92.f * U)[WRUI::Text(FText::FromString(WRDeeds::Clock(DNum(E, TEXT("secs"), 0.f))), WRUI::Body(16, 800), WRUI::Ink)]]
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Line(FText::FromString(FString::Join(N, TEXT(", "))), WRUI::Ink, 14, 600)]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f * U, 0.f, 0.f, 0.f)
						[
							WRUI::Text(FText::FromString(FString::Printf(TEXT("%s%s  ·  %s"), *WRRecords::BandName(WRRecords::BandOf(Size)), AL > 0 ? *FString::Printf(TEXT("  ·  AL %d"), AL) : TEXT(""), *DateOf(DNum(E, TEXT("at"), 0.f)))), WRUI::Body(12), WRUI::Ink3)
						]
					]
				];
			}
		}
	};

	// ------------------------------------------------------------------------------------------------ the boss altar

	class SAltarScreen : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SAltarScreen) {}
			SLATE_ARGUMENT(TWeakObjectPtr<AWRPlayerController>, PC)
		SLATE_END_ARGS()

		void Construct(const FArguments& A)
		{
			PC = A._PC;
			Altar = PC.IsValid() ? Cast<AWRBossAltar>(PC->WorldTarget.Get()) : nullptr;
			Path = Altar.IsValid() ? Altar->PathId : NAME_None;
			Boss = WRDeeds::AltarBoss(Path);
			Body = SNew(SVerticalBox);
			TSharedRef<SHorizontalBox> Tabs = SNew(SHorizontalBox);
			const FText Names[] = { LOCTEXT("EndTabPath", "Path"), LOCTEXT("EndTabReturn", "Return"), LOCTEXT("EndTabForge", "Forge soul") };
			for (int32 i = 0; i < 3; ++i)
			{
				Tabs->AddSlot().FillWidth(1.f).Padding(0.f, 0.f, i < 2 ? 6.f * U : 0.f, 0.f)[Button(Names[i], [this, i]() { Page = i; Rebuild(); }, false, true, 38.f)];
			}
			TSharedRef<SVerticalBox> All = SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[Tabs]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f * U, 0.f, 14.f * U)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f)[SNew(SBox).HeightOverride(3.f)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity_Lambda([this]() { return Page == 0 ? PathColour(Path) : FLinearColor::Transparent; })]]
					+ SHorizontalBox::Slot().FillWidth(1.f).Padding(6.f * U, 0.f)[SNew(SBox).HeightOverride(3.f)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity_Lambda([this]() { return Page == 1 ? PathColour(Path) : FLinearColor::Transparent; })]]
					+ SHorizontalBox::Slot().FillWidth(1.f)[SNew(SBox).HeightOverride(3.f)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity_Lambda([this]() { return Page == 2 ? PathColour(Path) : FLinearColor::Transparent; })]]
				]
				+ SVerticalBox::Slot().AutoHeight()[Body.ToSharedRef()];
			Rebuild();
			ChildSlot
			[
				Frame(FText::FromString(FString::Printf(TEXT("Altar of %s"), *PathName(Path))),
					FText::FromString(Boss.IsNone() ? FString(TEXT("A boss altar")) : FString::Printf(TEXT("Behind %s's arena  ·  everything about this path, in one place"), *CreatureName(Boss))),
					All, 720.f, PathColour(Path))
			];
		}

		virtual void Tick(const FGeometry& Geo, const double Now, const float Dt) override
		{
			SCompoundWidget::Tick(Geo, Now, Dt);
			CloseIfFar(PC, TEXT("altar"), Altar.Get(), AWRBossAltar::InteractRange + 400.f);
			// What the pages read changed on the server (a mark, a forge, the bag, the path): redraw.
			const AWRPlayerState* PS = StateOf(PC);
			const UWRDeedsComponent* D = DeedsOf(PC);
			uint32 Key = 0;
			if (PS && D)
			{
				const UWRPathComponent* P = PS->GetPaths();
				Key = HashCombine(GetTypeHash(P->GetMainPath()), GetTypeHash(P->GetSubPath()));
				Key = HashCombine(Key, P->GetEarned(Path) * 7 + P->GetSpent(Path));
				if (const FWRPrestigeRep* R = D->MarksOn(Path)) Key = HashCombine(Key, R->Keener + R->Deeper * 4 + R->Rooted * 16);
				for (const FWRSoulForged& F : D->Forged) Key = HashCombine(Key, GetTypeHash(F.Boss) + F.bWeapon + 2 * F.bTalisman);
				for (const FWRPathLayout& L : D->Layouts) Key = HashCombine(Key, GetTypeHash(L.Path) + L.Ranks.Num());
				if (const TSharedPtr<FJsonObject> Row = WRDeeds::SoulRow(Boss)) Key = HashCombine(Key, PS->GetInventory()->CountOf(FName(*DStr(Row, TEXT("soul")))));
			}
			if (Key != LastKey) { LastKey = Key; Rebuild(); }
		}
		virtual bool SupportsKeyboardFocus() const override { return true; }

	private:
		TWeakObjectPtr<AWRPlayerController> PC;
		TWeakObjectPtr<AWRBossAltar> Altar;
		FName Path, Boss;
		TSharedPtr<SVerticalBox> Body;
		int32 Page = 0;
		FName Perk;
		uint32 LastKey = 0;

		void Rebuild()
		{
			Body->ClearChildren();
			if (Page == 0) PathPage();
			else if (Page == 1) ReturnPage();
			else ForgePage();
			Body->AddSlot().AutoHeight().Padding(0.f, 18.f * U, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f)
				+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(150.f * U)[Button(LOCTEXT("EndBack", "Back"), [this]() { if (PC.IsValid()) PC->SetMenu(EWRMenu::None); }, false, true, 44.f)]]
			];
		}

		void PathPage()
		{
			const AWRPlayerState* PS = StateOf(PC);
			const UWRPathComponent* P = PS ? PS->GetPaths() : nullptr;
			if (!P) return;
			const FString Name = PathName(Path);
			const bool bMain = P->GetMainPath() == Path, bSub = P->GetSubPath() == Path;
			Body->AddSlot().AutoHeight()[WRUI::Text(FText::FromString(bMain ? FString::Printf(TEXT("%s is your main path."), *Name) : bSub ? FString::Printf(TEXT("%s is your subclass."), *Name) : FString::Printf(TEXT("Take %s as your subclass."), *Name)), WRUI::Title(17), WRUI::Ink)];
			Body->AddSlot().AutoHeight().Padding(0.f, 8.f * U, 0.f, 0.f)
			[
				Line(FText::FromString(FString::Printf(TEXT("Points earned in %s: %d  ·  spent: %d. A path you set aside keeps its ranks: take it back here and they return."), *Name, P->GetEarned(Path), P->GetSpent(Path))))
			];
			if (!bMain && !bSub)
			{
				Body->AddSlot().AutoHeight().Padding(0.f, 10.f * U, 0.f, 0.f)
				[
					Line(FText::FromString(P->GetSubPath().IsNone() ? FString(TEXT("You have no subclass now.")) : FString::Printf(TEXT("%s, your subclass now, is set aside with its ranks."), *PathName(P->GetSubPath()))), WRUI::Ink3, 12)
				];
				Body->AddSlot().AutoHeight().Padding(0.f, 14.f * U, 0.f, 0.f)[Button(FText::FromString(FString::Printf(TEXT("Take %s as your subclass"), *Name)), [this]() { Send(PC, FString::Printf(TEXT("deeds subclass %s"), *Path.ToString())); }, true, true, 46.f)];
			}
		}

		void ReturnPage()
		{
			const AWRPlayerState* PS = StateOf(PC);
			const UWRPathComponent* P = PS ? PS->GetPaths() : nullptr;
			const UWRDeedsComponent* D = DeedsOf(PC);
			if (!P || !D) return;
			const FString Name = PathName(Path);
			const int32 Need = FMath::RoundToInt(WRTune(TEXT("prestige.atPoints"), 100.f));
			const int32 MaxMarks = FMath::RoundToInt(WRTune(TEXT("prestige.maxMarks"), 3.f));
			Body->AddSlot().AutoHeight()[Line(FText::FromString(FString::Printf(TEXT("Return %s at %d points: it starts again from nothing, and you carry a mark of it for good (up to %d). Your layout is kept: Restore layout buys it back as points come."), *Name, Need, MaxMarks)))];
			Body->AddSlot().AutoHeight().Padding(0.f, 12.f * U, 0.f, 4.f * U)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[WRUI::Text(TAttribute<FText>::CreateLambda([this, Need]() { const AWRPlayerState* S = StateOf(PC); return FText::FromString(FString::Printf(TEXT("%s: %d / %d points"), *PathName(Path), S ? S->GetPaths()->GetEarned(Path) : 0, Need)); }), WRUI::Body(15, 700), WRUI::Ink)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f * U, 0.f)[Heading(LOCTEXT("EndMarks", "MARKS"))]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Knots([this]() { const UWRDeedsComponent* X = DeedsOf(PC); const FWRPrestigeRep* R = X ? X->MarksOn(Path) : nullptr; return R ? R->Total() : 0; })]
			];
			Body->AddSlot().AutoHeight()[SNew(SWRBar).Height(8.f * U).Color(PathColour(Path)).Fraction_Lambda([this, Need]() { const AWRPlayerState* S = StateOf(PC); return S ? FMath::Clamp(float(S->GetPaths()->GetEarned(Path)) / FMath::Max(1, Need), 0.f, 1.f) : 0.f; })];
			// The three marks to choose from.
			const FWRPrestigeRep* R = D->MarksOn(Path);
			const FWRPathDef* Def = FWRPathRegistry::Get().Find(Path);
			struct FPerk { FName Id; FString Title; FString Text; bool bOk; };
			const int32 Dmg = FMath::RoundToInt(WRTune(TEXT("prestige.damage"), 0.05f) * 100.f), Res = FMath::RoundToInt(WRTune(TEXT("prestige.resource"), 0.1f) * 100.f);
			const FPerk Perks[] = {
				{ TEXT("keener"), TEXT("Keener"), FString::Printf(TEXT("%s's blows and arts land %d%% harder while you walk it."), *Name, Dmg), true },
				{ TEXT("deeper"), TEXT("Deeper"), FString::Printf(TEXT("%s's resource holds and fills %d%% more."), *Name, Res), true },
				{ TEXT("rooted"), TEXT("Rooted"), FString::Printf(TEXT("%s's root passive, %s, works at half strength while it's your subclass. Once a path."), *Name, Def ? *Def->RootPassiveName : TEXT("")), !(R && R->Rooted > 0) },
			};
			Body->AddSlot().AutoHeight().Padding(0.f, 16.f * U, 0.f, 6.f * U)[Heading(LOCTEXT("EndPickMark", "PICK THE MARK YOU CARRY OUT"))];
			TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
			for (int32 i = 0; i < 3; ++i)
			{
				const FPerk K = Perks[i];
				const int32 Have = !R ? 0 : i == 0 ? R->Keener : i == 1 ? R->Deeper : R->Rooted;
				Row->AddSlot().FillWidth(1.f).Padding(0.f, 0.f, i < 2 ? 8.f * U : 0.f, 0.f)
				[
					Card(SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(FText::FromString(Have > 0 ? FString::Printf(TEXT("%s  ×%d"), *K.Title, Have) : K.Title), WRUI::Title(16), K.bOk ? WRUI::Ink : WRUI::Ink3)]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 0.f)[Line(FText::FromString(K.bOk ? K.Text : FString(TEXT("Taken: Rooted is once a path."))), K.bOk ? WRUI::Ink2 : WRUI::Ink3, 12)],
						[this, K]() { if (K.bOk) Perk = K.Id; }, [this, K]() { return Perk == K.Id; }, K.bOk, PathColour(Path))
				];
			}
			Body->AddSlot().AutoHeight()[Row];
			// Why not (never blank), then the hold.
			Body->AddSlot().AutoHeight().Padding(0.f, 14.f * U, 0.f, 6.f * U)
			[
				WRUI::Text(TAttribute<FText>::CreateLambda([this]() { const FString Why = Block(); return FText::FromString(Why.IsEmpty() ? FString::Printf(TEXT("Ready. Hold to return %s."), *PathName(Path)) : Why); }), WRUI::Body(13, 700),
					TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(Block().IsEmpty() ? WRUI::Good : WRUI::Warn); }), false, true)
			];
			Body->AddSlot().AutoHeight()
			[
				SNew(SHoldButton).Text(FText::FromString(FString::Printf(TEXT("Return %s"), *Name))).Seconds(WRTune(TEXT("prestige.holdSeconds"), 1.5f)).Accent(FLinearColor(1.f, 0.8f, 0.35f))
				.Ready_Lambda([this]() { return Block().IsEmpty(); })
				.OnDone_Lambda([this]() { Send(PC, FString::Printf(TEXT("deeds return %s %s"), *Path.ToString(), *Perk.ToString())); Perk = NAME_None; })
			];
			// The saved layout from the last return.
			const FWRPathLayout* Lay = D->Layouts.FindByPredicate([this](const FWRPathLayout& L) { return L.Path == Path; });
			if (Lay && Lay->Ranks.Num() > 0)
			{
				int32 Ranks = 0;
				for (const FWRNodeRank& N : Lay->Ranks) Ranks += N.Rank;
				Body->AddSlot().AutoHeight().Padding(0.f, 16.f * U, 0.f, 0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Line(FText::FromString(FString::Printf(TEXT("Saved layout: %d ranks. Restore buys them back in order, as far as your points go."), Ranks)))]
					+ SHorizontalBox::Slot().AutoWidth().Padding(10.f * U, 0.f, 0.f, 0.f)[SNew(SBox).WidthOverride(180.f * U)[Button(LOCTEXT("EndRestore", "Restore layout"), [this]() { Send(PC, FString::Printf(TEXT("deeds restore %s"), *Path.ToString())); }, false, P->IsActivePath(Path))]]
				];
			}
		}

		FString Block() const
		{
			const FString Why = WRDeeds::PrestigeBlock(PC.Get(), Path);
			if (!Why.IsEmpty()) return Why;
			return Perk.IsNone() ? FString(TEXT("Pick the mark you carry out")) : FString();
		}

		void ForgePage()
		{
			const AWRPlayerState* PS = StateOf(PC);
			const UWRDeedsComponent* D = DeedsOf(PC);
			const TSharedPtr<FJsonObject> Row = WRDeeds::SoulRow(Boss);
			if (!PS || !D) return;
			if (!Row.IsValid())
			{
				Body->AddSlot().AutoHeight()[Line(LOCTEXT("EndNoSoul", "No soul answers at this altar yet."), WRUI::Ink2, 14)];
				return;
			}
			const FString BossName = CreatureName(Boss);
			const FName Soul(*DStr(Row, TEXT("soul")));
			const int32 Souls = PS->GetInventory()->CountOf(Soul);
			const FLinearColor Col = DLin(DColour(Row, TEXT("colour"), FColor(220, 180, 110)));
			Body->AddSlot().AutoHeight()[Line(FText::FromString(FString::Printf(TEXT("%s's soul drops to everyone who fought it: always the first time, then now and then (more often at a higher Artifact Level, and twice as often in its hidden third phase). One soul makes one of its two gifts; a second makes the other."), *BossName)))];
			Body->AddSlot().AutoHeight().Padding(0.f, 10.f * U, 0.f, 12.f * U)[WRUI::Text(FText::FromString(FString::Printf(TEXT("%s in your bag: %d"), *ItemName(Soul), Souls)), WRUI::Body(15, 700), Souls > 0 ? WRUI::Ink : WRUI::Ink3)];
			const TSharedPtr<FJsonObject> Art = DChild(Row, TEXT("art"));
			const TSharedPtr<FJsonObject> Tal = DChild(Row, TEXT("talisman"));
			const FName Weapon(*DStr(Row, TEXT("weapon")));
			const auto Gift = [this, &D, Col](bool bWeapon, const FString& Title, const FString& Sub, const FString& Text)
			{
				const bool bOwned = D->HasForged(Boss, bWeapon);
				return SNew(SBorder).Padding(FMargin(14.f * U, 12.f * U)).BorderImage(WRUI::Box(FLinearColor(0.055f, 0.05f, 0.048f), 6.f, bOwned ? Col : WRUI::Rim, bOwned ? 2.f : 1.f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(FText::FromString(Title), WRUI::Title(16), WRUI::Ink)]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f * U, 0.f, 0.f)[WRUI::Text(FText::FromString(Sub), WRUI::Body(13, 700), Col)]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f * U, 0.f, 0.f)[Line(FText::FromString(Text), WRUI::Ink2, 12)]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 0.f)
							[
								WRUI::Text(TAttribute<FText>::CreateLambda([this, bWeapon]() { const FString W = WRDeeds::ForgeBlock(PC.Get(), Boss, bWeapon); return FText::FromString(W == TEXT("Owned") ? FString(TEXT("Yours.")) : W.IsEmpty() ? FString(TEXT("Ready to forge.")) : W); }), WRUI::Body(12, 700),
									TAttribute<FSlateColor>::CreateLambda([this, bWeapon]() { const FString W = WRDeeds::ForgeBlock(PC.Get(), Boss, bWeapon); return FSlateColor(W == TEXT("Owned") || W.IsEmpty() ? WRUI::Good : WRUI::Warn); }), false, true)
							]
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.f * U, 0.f, 0.f, 0.f)
						[
							SNew(SBox).WidthOverride(130.f * U)
							[
								Button(LOCTEXT("EndForge", "Forge"), [this, bWeapon]() { Send(PC, FString::Printf(TEXT("deeds forge %s %s"), *Boss.ToString(), bWeapon ? TEXT("weapon") : TEXT("talisman"))); }, true,
									TAttribute<bool>::CreateLambda([this, bWeapon]() { return WRDeeds::ForgeBlock(PC.Get(), Boss, bWeapon).IsEmpty(); }), 44.f)
							]
						]
					];
			};
			Body->AddSlot().AutoHeight()[Gift(true, FString::Printf(TEXT("A  ·  Awaken the %s"), *ItemName(Weapon)), FString::Printf(TEXT("Soul art: %s"), *DStr(Art, TEXT("name"))),
				FString::Printf(TEXT("%s The weapon keeps its quality, sockets and upgrades, strikes as starsteel, and carries %s for good. The art sits in a quick slot while you hold it (25 stamina, 30 s)."), *DStr(Art, TEXT("line")), *DStr(Row, TEXT("element"))))];
			Body->AddSlot().AutoHeight().Padding(0.f, 8.f * U, 0.f, 0.f)[Gift(false, FString::Printf(TEXT("B  ·  %s"), *DStr(Tal, TEXT("name"))), TEXT("A soul talisman (ring slot)"), DStr(Tal, TEXT("line")))];
			if (D->HasForged(Boss, true) && D->HasForged(Boss, false))
			{
				const TSharedPtr<FJsonObject> Sal = DChild(WRDeeds::Souls(), TEXT("salvage"));
				const FString Gives = FString::Printf(TEXT("%d %s"), FMath::RoundToInt(DNum(Sal, TEXT("count"), 5.f)), *ItemName(FName(*DStr(Sal, TEXT("item"), TEXT("core_shard")))));
				Body->AddSlot().AutoHeight().Padding(0.f, 12.f * U, 0.f, 0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Line(FText::FromString(FString::Printf(TEXT("Both gifts are yours. A spare soul gives up %s here."), *Gives)))]
					+ SHorizontalBox::Slot().AutoWidth().Padding(10.f * U, 0.f, 0.f, 0.f)[SNew(SBox).WidthOverride(170.f * U)[Button(LOCTEXT("EndSalvage", "Give a soul"), [this]() { Send(PC, FString::Printf(TEXT("deeds salvage %s"), *Boss.ToString())); }, false, Souls > 0)]]
				];
			}
		}
	};

	// ------------------------------------------------------------------------------------------------ the Heron's draught

	class SRespecScreen : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SRespecScreen) {}
			SLATE_ARGUMENT(TWeakObjectPtr<AWRPlayerController>, PC)
		SLATE_END_ARGS()

		void Construct(const FArguments& A)
		{
			PC = A._PC;
			Body = SNew(SVerticalBox);
			Rebuild();
			ChildSlot
			[
				Frame(LOCTEXT("EndHeron", "The Heron's draught"), LOCTEXT("EndHeronSub", "\"The heron of forgetting hovers over the ale and steals men's wits.\""), Body.ToSharedRef(), 640.f, FLinearColor(0.55f, 0.6f, 0.66f))
			];
		}
		virtual void Tick(const FGeometry& Geo, const double Now, const float Dt) override
		{
			SCompoundWidget::Tick(Geo, Now, Dt);
			const AWRPlayerState* PS = StateOf(PC);
			uint32 Key = 0;
			if (PS)
			{
				const UWRPathComponent* P = PS->GetPaths();
				Key = HashCombine(GetTypeHash(P->GetMainPath()), GetTypeHash(P->GetSubPath()));
				Key = HashCombine(Key, P->GetSpent(P->GetMainPath()) * 131 + P->GetSpent(P->GetSubPath()));
				Key = HashCombine(Key, PS->GetInventory()->CountOf(TEXT("heron_draught")));
			}
			if (Key != LastKey) { LastKey = Key; Rebuild(); }
		}
		virtual bool SupportsKeyboardFocus() const override { return true; }
	private:
		TWeakObjectPtr<AWRPlayerController> PC;
		TSharedPtr<SVerticalBox> Body;
		FName Pick;
		uint32 LastKey = 0;

		void Rebuild()
		{
			Body->ClearChildren();
			const AWRPlayerState* PS = StateOf(PC);
			const UWRPathComponent* P = PS ? PS->GetPaths() : nullptr;
			if (!P) return;
			const int32 Have = PS->GetInventory()->CountOf(TEXT("heron_draught"));
			Body->AddSlot().AutoHeight()[Line(LOCTEXT("EndHeronAbout", "Drink it and one path's learning is gone: its ranks go (the starter stays), its points come back to spend at once. Its XP stays. Only a path you walk now can be forgotten; one set aside at an altar keeps its ranks."))];
			Body->AddSlot().AutoHeight().Padding(0.f, 8.f * U, 0.f, 12.f * U)[WRUI::Text(FText::FromString(FString::Printf(TEXT("Heron's draughts in your bag: %d"), Have)), WRUI::Body(14, 700), Have > 0 ? WRUI::Ink : WRUI::Warn)];
			TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
			const FName Paths[] = { P->GetMainPath(), P->GetSubPath() };
			const FText Roles[] = { LOCTEXT("EndMain", "Main path"), LOCTEXT("EndSub", "Subclass") };
			for (int32 i = 0; i < 2; ++i)
			{
				const FName Id = Paths[i];
				TSharedRef<SVerticalBox> C = SNew(SVerticalBox);
				C->AddSlot().AutoHeight()[Heading(Roles[i])];
				if (Id.IsNone())
				{
					C->AddSlot().AutoHeight().Padding(0.f, 4.f * U, 0.f, 0.f)[WRUI::Text(LOCTEXT("EndNone", "None"), WRUI::Title(16), WRUI::Ink3)];
				}
				else
				{
					C->AddSlot().AutoHeight().Padding(0.f, 4.f * U, 0.f, 0.f)[WRUI::Text(FText::FromString(PathName(Id)), WRUI::Title(17), PathColour(Id))];
					C->AddSlot().AutoHeight().Padding(0.f, 4.f * U, 0.f, 0.f)[Line(FText::FromString(FString::Printf(TEXT("%d points spent  ·  %d earned"), P->GetSpent(Id), P->GetEarned(Id))), WRUI::Ink2, 12)];
				}
				Row->AddSlot().FillWidth(1.f).Padding(0.f, 0.f, i == 0 ? 8.f * U : 0.f, 0.f)[Card(C, [this, Id]() { if (!Id.IsNone()) Pick = Id; }, [this, Id]() { return !Id.IsNone() && Pick == Id; }, !Id.IsNone(), PathColour(Id))];
			}
			Body->AddSlot().AutoHeight()[Row];
			Body->AddSlot().AutoHeight().Padding(0.f, 14.f * U, 0.f, 6.f * U)
			[
				WRUI::Text(TAttribute<FText>::CreateLambda([this]() { const FString W = Block(); return FText::FromString(W.IsEmpty() ? FString::Printf(TEXT("Ready. Hold to forget %s."), *PathName(Pick)) : W); }), WRUI::Body(13, 700),
					TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(Block().IsEmpty() ? WRUI::Good : WRUI::Warn); }), false, true)
			];
			Body->AddSlot().AutoHeight()
			[
				SNew(SHoldButton).Text(LOCTEXT("EndForget", "Drink and forget")).Seconds(WRTune(TEXT("deeds.respecHoldSeconds"), 1.5f)).Accent(FLinearColor(0.7f, 0.78f, 0.86f))
				.Ready_Lambda([this]() { return Block().IsEmpty(); })
				.OnDone_Lambda([this]() { Send(PC, FString::Printf(TEXT("deeds forget %s"), *Pick.ToString())); if (PC.IsValid()) PC->SetMenu(EWRMenu::None); })
			];
			Body->AddSlot().AutoHeight().Padding(0.f, 14.f * U, 0.f, 0.f)
			[
				SNew(SHorizontalBox) + SHorizontalBox::Slot().FillWidth(1.f)
				+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(150.f * U)[Button(LOCTEXT("EndKeep", "Keep it"), [this]() { if (PC.IsValid()) PC->SetMenu(EWRMenu::None); }, false, true, 44.f)]]
			];
		}
		FString Block() const
		{
			if (Pick.IsNone()) return TEXT("Pick the path the heron takes");
			return WRDeeds::RespecBlock(PC.Get(), Pick);
		}
	};

	// ------------------------------------------------------------------------------------------------ the war-hall's gate

	class SWarHallCard : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SWarHallCard) {}
			SLATE_ARGUMENT(TWeakObjectPtr<AWRPlayerController>, PC)
		SLATE_END_ARGS()

		void Construct(const FArguments& A)
		{
			PC = A._PC;
			Gate = PC.IsValid() ? Cast<AWRWarHallGate>(PC->WorldTarget.Get()) : nullptr;
			const FName Hall = Gate.IsValid() ? Gate->HallId : NAME_None;
			const TSharedPtr<FJsonObject> Row = WRWarHall::Row(Hall);
			TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
			Body->AddSlot().AutoHeight()[Line(FText::FromString(DStr(Row, TEXT("line"))), WRUI::Ink2, 13)];
			// The champions: their sigils (from their own lands' great bosses), names, what they are.
			Body->AddSlot().AutoHeight().Padding(0.f, 14.f * U, 0.f, 6.f * U)[Heading(LOCTEXT("EndChamps", "THE CHAMPIONS  ·  RIVALS: THEY HATE EACH OTHER AS MUCH AS YOU"))];
			TSharedRef<SHorizontalBox> Champs = SNew(SHorizontalBox);
			if (const TArray<TSharedPtr<FJsonValue>>* L = DArr(Row, TEXT("champions")))
			{
				for (int32 i = 0; i < L->Num(); ++i)
				{
					const FName Id(*(*L)[i]->AsString());
					const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(Id);
					FName Sigil;
					for (const FWRCreatureDef& C : FWRCreatureRegistry::Get().All()) if (Def && C.Biome == Def->Biome && C.Role.Contains(TEXT("major"))) Sigil = C.Id;
					const FLinearColor Col = Def && Def->Biome == TEXT("volcanic") ? WRUI::Ember : WRUI::Frost;
					const float Base = WRTune(*FString::Printf(TEXT("bosses.%s.health"), *Id.ToString()), 12000.f);
					Champs->AddSlot().FillWidth(1.f).Padding(0.f, 0.f, i == 0 ? 10.f * U : 0.f, 0.f)
					[
						SNew(SBorder).Padding(FMargin(12.f * U, 10.f * U)).BorderImage(WRUI::Box(FLinearColor(0.055f, 0.05f, 0.048f), 6.f, WRUI::Rim, 1.f))
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f * U, 0.f)[SNew(SSigil).Id(Sigil).Size(56.f).Colour(Col)]
							+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(FText::FromString(CreatureName(Id)), WRUI::Title(16), WRUI::Ink)]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f * U, 0.f, 0.f)[Line(FText::FromString(FString::Printf(TEXT("%s  ·  %s alone, more with every fighter"), Def ? *WRPortals::BiomeName(Def->Biome) : TEXT(""), *FText::AsNumber(FMath::RoundToInt(Base)).ToString())), WRUI::Ink3, 12)]
							]
						]
					];
				}
			}
			Body->AddSlot().AutoHeight()[Champs];
			Body->AddSlot().AutoHeight().Padding(0.f, 10.f * U, 0.f, 0.f)[Line(LOCTEXT("EndHallHow", "The floor is the fight: the Smith's slams turn flags to lava, the Wyrm's sweeps and breath to ice, and a flag turned from one to the other within moments bursts in steam that hurts everything on it. Keep them even: one that drives its rival low gains Triumph."), WRUI::Ink2, 12)];
			// Now.
			Body->AddSlot().AutoHeight().Padding(0.f, 16.f * U, 0.f, 4.f * U)[Heading(LOCTEXT("EndHallNow", "THE DOOR"))];
			Body->AddSlot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(WRWarHall::StateLine(Gate.Get())); }), WRUI::Body(15, 700), WRUI::Ink, false, true)];
			Body->AddSlot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 0.f)[Line(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(GroupLine()); }), WRUI::Ink2, 13)];
			Body->AddSlot().AutoHeight().Padding(0.f, 4.f * U, 0.f, 0.f)
			[
				Line(TAttribute<FText>::CreateLambda([]()
				{
					const int32 Tuned = FMath::RoundToInt(WRTune(TEXT("server.raidGroupTuned"), 8.f));
					const int32 Cap = FMath::RoundToInt(WRTune(TEXT("server.raidGroupMax"), 0.f));
					return FText::FromString(FString::Printf(TEXT("Made for %d  ·  %s  ·  tames count as fighters"), Tuned, Cap > 0 ? *FString::Printf(TEXT("at most %d inside"), Cap) : TEXT("no cap on this world")));
				}), WRUI::Ink3, 12)
			];
			Body->AddSlot().AutoHeight().Padding(0.f, 18.f * U, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 8.f * U, 0.f)
				[
					Button(TAttribute<FText>::CreateLambda([this]() { const FString W = WRWarHall::EnterBlock(PC.Get(), Gate.Get()); return W.IsEmpty() ? LOCTEXT("EndEnter", "Go down into the hall") : FText::FromString(W); }),
						[this]()
						{
							AWRPlayerController* P = PC.Get();
							if (!P || !Gate.IsValid() || !WRWarHall::EnterBlock(P, Gate.Get()).IsEmpty()) return;
							P->SendCommand(FString::Printf(TEXT("warhall enter %s"), *Gate->HallId.ToString()));
							P->SetMenu(EWRMenu::None);
						}, true, TAttribute<bool>::CreateLambda([this]() { return WRWarHall::EnterBlock(PC.Get(), Gate.Get()).IsEmpty(); }), 46.f)
				]
				+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(150.f * U)[Button(LOCTEXT("EndBack2", "Back"), [this]() { if (PC.IsValid()) PC->SetMenu(EWRMenu::None); }, false, true, 46.f)]]
			];
			ChildSlot
			[
				Frame(FText::FromString(WRWarHall::Name(Hall)), FText::FromString(FString::Printf(TEXT("A war-hall  ·  raids for %d to %d"), 6, 10)), Body, 760.f, WRUI::Brass)
			];
		}
		virtual void Tick(const FGeometry& Geo, const double Now, const float Dt) override
		{
			SCompoundWidget::Tick(Geo, Now, Dt);
			CloseIfFar(PC, TEXT("warhall"), Gate.Get(), 1200.f);
		}
		virtual bool SupportsKeyboardFocus() const override { return true; }
	private:
		TWeakObjectPtr<AWRPlayerController> PC;
		TWeakObjectPtr<AWRWarHallGate> Gate;

		FString GroupLine() const
		{
			const AWRPlayerController* P = PC.Get();
			const APawn* Me = P ? P->GetPawn() : nullptr;
			const AWRWarHallGate* G = Gate.Get();
			if (!Me || !G || !P->GetWorld()->GetGameState()) return FString();
			TArray<FString> Near;
			for (APlayerState* S : P->GetWorld()->GetGameState()->PlayerArray)
			{
				const APawn* O = S ? S->GetPawn() : nullptr;
				if (O && FVector::Dist(O->GetActorLocation(), G->GetActorLocation()) < 1000.f) Near.Add(S->GetPlayerName());
			}
			FString Out = FString::Printf(TEXT("At the door: %s"), Near.Num() ? *FString::Join(Near, TEXT(", ")) : TEXT("you"));
			if (G->Inside > 0) Out += FString::Printf(TEXT("  ·  inside: %d"), G->Inside);
			if (G->State == EWRHallState::Sealed && G->Group.Num() > 0) Out += FString::Printf(TEXT("  ·  the group: %s"), *FString::Join(G->Group, TEXT(", ")));
			return Out;
		}
	};

	// ------------------------------------------------------------------------------------------------ a record tablet

	class SRecordsScreen : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SRecordsScreen) {}
			SLATE_ARGUMENT(TWeakObjectPtr<AWRPlayerController>, PC)
		SLATE_END_ARGS()
		void Construct(const FArguments& A)
		{
			PC = A._PC;
			Tablet = PC.IsValid() ? Cast<AWRRecordTablet>(PC->WorldTarget.Get()) : nullptr;
			const FName Board = Tablet.IsValid() ? Tablet->BoardId : NAME_None;
			TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
			Body->AddSlot().AutoHeight()[Line(LOCTEXT("EndBoardAbout", "Timed from the first blow to the death. Bands by the group's size (tames count); kills made wearing artifacts go in their own column. Kept for good on this world."), WRUI::Ink2, 12)];
			Body->AddSlot().AutoHeight().Padding(0.f, 12.f * U, 0.f, 0.f)[SNew(SBoardView).PC(PC).Board(Board)];
			Body->AddSlot().AutoHeight().Padding(0.f, 16.f * U, 0.f, 0.f)
			[
				SNew(SHorizontalBox) + SHorizontalBox::Slot().FillWidth(1.f)
				+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(150.f * U)[Button(LOCTEXT("EndBack3", "Back"), [this]() { if (PC.IsValid()) PC->SetMenu(EWRMenu::None); }, false, true, 44.f)]]
			];
			ChildSlot[Frame(FText::FromString(WRRecords::BoardName(Board)), LOCTEXT("EndBoardSub", "Kill times, cut in the stone"), Body, 820.f, FLinearColor(0.5f, 0.5f, 0.52f))];
		}
		virtual void Tick(const FGeometry& Geo, const double Now, const float Dt) override
		{
			SCompoundWidget::Tick(Geo, Now, Dt);
			CloseIfFar(PC, TEXT("records"), Tablet.Get(), 900.f);
		}
		virtual bool SupportsKeyboardFocus() const override { return true; }
	private:
		TWeakObjectPtr<AWRPlayerController> PC;
		TWeakObjectPtr<AWRRecordTablet> Tablet;
	};

	// ------------------------------------------------------------------------------------------------ a monument's plaque

	class SPlaqueScreen : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SPlaqueScreen) {}
			SLATE_ARGUMENT(TWeakObjectPtr<AWRPlayerController>, PC)
		SLATE_END_ARGS()
		void Construct(const FArguments& A)
		{
			PC = A._PC;
			Piece = PC.IsValid() ? Cast<AWRBuildPiece>(PC->WorldTarget.Get()) : nullptr;
			const FWRPieceDef* Def = Piece.IsValid() ? Piece->GetDef() : nullptr;
			const FName Boss = Def ? Def->Monument : NAME_None;
			TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
			Body->AddSlot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.f, 0.f, 14.f * U, 0.f)[SNew(SSigil).Id(Boss).Size(64.f).Colour(FLinearColor(0.78f, 0.55f, 0.28f))]
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(Main()); }), WRUI::Title(17), WRUI::Ink, false, true)]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 0.f)[Line(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(Names()); }), WRUI::Ink2, 13)]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 0.f)[Line(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(Al()); }), WRUI::Brass, 13, 700)]
				]
			];
			Body->AddSlot().AutoHeight().Padding(0.f, 14.f * U, 0.f, 0.f)[Line(LOCTEXT("EndPlaqueAbout", "The plaque keeps its owners' best kill of this boss from the kill-time boards, and the heaviest Artifact Level they've beaten it at."), WRUI::Ink3, 12)];
			Body->AddSlot().AutoHeight().Padding(0.f, 16.f * U, 0.f, 0.f)
			[
				SNew(SHorizontalBox) + SHorizontalBox::Slot().FillWidth(1.f)
				+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(150.f * U)[Button(LOCTEXT("EndBack4", "Back"), [this]() { if (PC.IsValid()) PC->SetMenu(EWRMenu::None); }, false, true, 44.f)]]
			];
			ChildSlot[Frame(FText::FromString(Def ? Def->Name : FString(TEXT("Monument"))), FText::FromString(Piece.IsValid() ? FString::Printf(TEXT("Raised by %s"), *Piece->OwnerName) : FString()), Body, 620.f, FLinearColor(0.78f, 0.55f, 0.28f))];
		}
		virtual void Tick(const FGeometry& Geo, const double Now, const float Dt) override
		{
			SCompoundWidget::Tick(Geo, Now, Dt);
			CloseIfFar(PC, TEXT("plaque"), Piece.Get(), 900.f);
		}
		virtual bool SupportsKeyboardFocus() const override { return true; }
	private:
		TWeakObjectPtr<AWRPlayerController> PC;
		TWeakObjectPtr<AWRBuildPiece> Piece;
		TArray<FString> Parts() const { TArray<FString> P; if (Piece.IsValid()) Piece->CustomName.ParseIntoArray(P, TEXT("|"), false); return P; }
		FString Main() const
		{
			const TArray<FString> P = Parts();
			const FWRPieceDef* Def = Piece.IsValid() ? Piece->GetDef() : nullptr;
			const FString Boss = Def ? CreatureName(Def->Monument) : FString();
			return P.Num() >= 1 && !P[0].IsEmpty() ? FString::Printf(TEXT("%s, killed in %s"), *Boss, *P[0]) : FString::Printf(TEXT("%s: no kill of yours on the board yet"), *Boss);
		}
		FString Names() const { const TArray<FString> P = Parts(); return P.Num() >= 2 && !P[1].IsEmpty() ? FString::Printf(TEXT("By %s"), *P[1]) : FString(TEXT("Kill it, and the bronze takes your names and your time.")); }
		FString Al() const { const TArray<FString> P = Parts(); const int32 AL = P.Num() >= 3 ? FCString::Atoi(*P[2]) : 0; return AL > 0 ? FString::Printf(TEXT("Highest Artifact Level beaten: %d%s"), AL, AL >= 15 ? TEXT("  ·  the knot border") : TEXT("")) : FString(); }
	};

	// ------------------------------------------------------------------------------------------------ the Deeds tab

	class SDeedsTab : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDeedsTab) {}
			SLATE_ARGUMENT(TWeakObjectPtr<AWRPlayerController>, PC)
		SLATE_END_ARGS()

		void Construct(const FArguments& A)
		{
			PC = A._PC;
			Box = SNew(SVerticalBox);
			TSharedRef<SHorizontalBox> Subs = SNew(SHorizontalBox);
			const FText Names[] = { LOCTEXT("EndSubAch", "Achievements"), LOCTEXT("EndSubRec", "Records"), LOCTEXT("EndSubTit", "Titles"), LOCTEXT("EndSubCos", "Cosmetics"), LOCTEXT("EndSubMarks", "Marks") };
			for (int32 i = 0; i < 5; ++i)
			{
				Subs->AddSlot().FillWidth(1.f).Padding(0.f, 0.f, i < 4 ? 6.f * U : 0.f, 0.f)
				[
					SNew(SBox).HeightOverride(38.f * U)
					[
						SNew(SWRButton).Text(Names[i]).FontSize(13).Primary(false)
						.OnClicked_Lambda([this, i]() { Sub = i; Board = NAME_None; if (Sub == 1) Send(PC, TEXT("deeds records all 0")); Rebuild(); return FReply::Handled(); })
					]
				];
			}
			ChildSlot
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(24.f * U, 14.f * U, 24.f * U, 0.f))[Subs]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(24.f * U, 4.f * U, 24.f * U, 0.f))
				[
					SNew(SBox).HeightOverride(3.f)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity(WRUI::Brass2)]
				]
				+ SVerticalBox::Slot().FillHeight(1.f)[SNew(SScrollBox) + SScrollBox::Slot().Padding(FMargin(24.f * U, 14.f * U))[Box.ToSharedRef()]]
			];
			Rebuild();
		}

		virtual void Tick(const FGeometry& Geo, const double Now, const float Dt) override
		{
			SCompoundWidget::Tick(Geo, Now, Dt);
			const UWRDeedsComponent* D = DeedsOf(PC);
			uint32 Key = HashCombine(uint32(Sub), GetTypeHash(Cat));
			if (D)
			{
				Key = HashCombine(Key, D->Achieved.Num() * 31 + D->Titles.Num() * 7 + D->Cosmetics.Num());
				for (const FIntPoint& P : D->Progress) Key = HashCombine(Key, P.X);
				Key = HashCombine(Key, GetTypeHash(D->ActiveTitle));
				Key = HashCombine(Key, HashCombine(GetTypeHash(D->WornGlow), HashCombine(GetTypeHash(D->WornDye), HashCombine(GetTypeHash(D->WornMark), GetTypeHash(D->BannerSigil)))));
				for (const FWRPrestigeRep& R : D->Marks) Key = HashCombine(Key, R.Total() + GetTypeHash(R.Path));
				Key = HashCombine(Key, D->Bosses.Num());
			}
			if (const UWRArtifactComponent* Art = UWRArtifactComponent::For(StateOf(PC))) Key = HashCombine(Key, Art->Titles.Num());
			if (Sub == 1 && Board.IsNone()) Key = HashCombine(Key, PointerHash(WRRecords::ClientBoards(NAME_None).Get()));
			if (Key != LastKey) { LastKey = Key; Rebuild(); }
		}

	private:
		TWeakObjectPtr<AWRPlayerController> PC;
		TSharedPtr<SVerticalBox> Box;
		int32 Sub = 0;
		FString Cat;
		FName Board;
		uint32 LastKey = 0;

		void Rebuild()
		{
			Box->ClearChildren();
			if (!DeedsOf(PC)) return;
			switch (Sub)
			{
			case 0: Achievements(); break;
			case 1: Records(); break;
			case 2: Titles(); break;
			case 3: Cosmetics(); break;
			default: Marks(); break;
			}
		}

		// --- Achievements

		void Achievements()
		{
			const UWRDeedsComponent* D = DeedsOf(PC);
			const TSharedPtr<FJsonObject> Data = WRDeeds::Achievements();
			const TSharedPtr<FJsonObject> Cats = DChild(Data, TEXT("categories"));
			const TArray<TSharedPtr<FJsonValue>>* Rows = DArr(Data, TEXT("achievements"));
			if (!Rows) return;
			int32 Live = 0, Got = 0;
			TMap<FString, FIntPoint> PerCat;
			for (const auto& V : *Rows)
			{
				const TSharedPtr<FJsonObject> R = V->AsObject();
				if (!WRDeeds::IsLive(R)) continue;
				const bool bGot = D->HasDeed(FName(*DStr(R, TEXT("id"))));
				++Live; Got += bGot;
				FIntPoint& C = PerCat.FindOrAdd(DStr(R, TEXT("cat")));
				++C.Y; C.X += bGot;
			}
			Box->AddSlot().AutoHeight()[WRUI::Text(FText::FromString(FString::Printf(TEXT("%d of %d deeds"), Got, Live)), WRUI::Title(18), WRUI::Ink)];
			Box->AddSlot().AutoHeight().Padding(0.f, 2.f * U, 0.f, 10.f * U)[SNew(SWRBar).Height(6.f * U).Color(WRUI::Brass).Fraction(Live > 0 ? float(Got) / Live : 0.f)];
			TSharedRef<SWrapBox> Picker = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(6.f * U, 6.f * U));
			Picker->AddSlot()[SNew(SBox).MinDesiredWidth(80.f * U)[Button(LOCTEXT("EndAll", "All"), [this]() { Cat.Reset(); Rebuild(); }, Cat.IsEmpty(), true, 34.f)]];
			if (Cats.IsValid())
			{
				for (const auto& P : Cats->Values)
				{
					const FString K(*P.Key);
					const FIntPoint C = PerCat.FindRef(K);
					if (C.Y == 0) continue;   // a category with nothing live in this build
					Picker->AddSlot()[SNew(SBox).MinDesiredWidth(100.f * U)[Button(FText::FromString(FString::Printf(TEXT("%s  %d/%d"), *P.Value->AsString(), C.X, C.Y)), [this, K]() { Cat = K; Rebuild(); }, Cat == K, true, 34.f)]];
				}
			}
			Box->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 12.f * U)[Picker];
			int32 HiddenLeft = 0;
			for (const auto& V : *Rows)
			{
				const TSharedPtr<FJsonObject> R = V->AsObject();
				if (!WRDeeds::IsLive(R) || (!Cat.IsEmpty() && DStr(R, TEXT("cat")) != Cat)) continue;
				const FName Id(*DStr(R, TEXT("id")));
				const bool bGot = D->HasDeed(Id);
				if (DBool(R, TEXT("hidden")) && !bGot) { ++HiddenLeft; continue; }
				const int32 Pi = D->ProgressIds.IndexOfByKey(Id);
				const FIntPoint Prog = Pi != INDEX_NONE && D->Progress.IsValidIndex(Pi) ? D->Progress[Pi] : FIntPoint::ZeroValue;
				TArray<FString> Gives;
				if (!DStr(R, TEXT("title")).IsEmpty()) Gives.Add(FString::Printf(TEXT("title: %s"), *DStr(R, TEXT("title"))));
				if (!DStr(R, TEXT("cosmetic")).IsEmpty()) Gives.Add(FString::Printf(TEXT("%s"), *DStr(WRDeeds::Cosmetic(FName(*DStr(R, TEXT("cosmetic")))), TEXT("name"))));
				Box->AddSlot().AutoHeight().Padding(0.f, 2.f * U)
				[
					SNew(SBorder).Padding(FMargin(12.f * U, 9.f * U)).BorderImage(WRUI::Box(bGot ? FLinearColor(0.1f, 0.075f, 0.04f) : FLinearColor(0.05f, 0.046f, 0.044f), 5.f, bGot ? WRUI::Brass2 : FLinearColor::Transparent, 1.f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f * U, 0.f)
						[
							SNew(SBox).WidthOverride(18.f * U).HeightOverride(18.f * U)[SNew(SImage).Image(WRUI::Box(bGot ? WRUI::Brass : FLinearColor(0.07f, 0.065f, 0.06f), 9.f * U, WRUI::Brass2, 1.5f))]
						]
						+ SHorizontalBox::Slot().FillWidth(1.f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(FText::FromString(DStr(R, TEXT("name"))), WRUI::Body(15, 700), bGot ? WRUI::Ink : WRUI::Ink2)]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f * U, 0.f, 0.f)[Line(FText::FromString(DStr(R, TEXT("text"))), WRUI::Ink3, 12)]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f * U, 0.f, 0.f)[Line(FText::FromString(Gives.Num() ? FString::Printf(TEXT("Gives %s"), *FString::Join(Gives, TEXT(", "))) : FString()), WRUI::Brass, 12, 600)]
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f * U, 0.f, 0.f, 0.f)
						[
							WRUI::Text(FText::FromString(bGot ? FString(TEXT("Done")) : Prog.Y > 0 ? FString::Printf(TEXT("%d / %d"), Prog.X, Prog.Y) : FString()), WRUI::Body(13, 700), bGot ? WRUI::Good : WRUI::Ink2)
						]
					]
				];
			}
			Box->AddSlot().AutoHeight().Padding(0.f, 10.f * U, 0.f, 0.f)[Line(FText::FromString(HiddenLeft > 0 ? FString::Printf(TEXT("And %d hidden deed%s, still to be found."), HiddenLeft, HiddenLeft == 1 ? TEXT("") : TEXT("s")) : FString()), WRUI::Ink3, 12)];
		}

		// --- Records

		void Records()
		{
			if (!Board.IsNone())
			{
				Box->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f * U)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(170.f * U)[Button(LOCTEXT("EndAllBoards", "Every board"), [this]() { Board = NAME_None; Send(PC, TEXT("deeds records all 0")); Rebuild(); }, false, true, 36.f)]]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(12.f * U, 0.f, 0.f, 0.f)[WRUI::Text(FText::FromString(WRRecords::BoardName(Board)), WRUI::Title(18), WRUI::Ink)]
				];
				Box->AddSlot().AutoHeight()[SNew(SBoardView).PC(PC).Board(Board)];
				return;
			}
			const TSharedPtr<FJsonObject> Sum = WRRecords::ClientBoards(NAME_None);
			if (!Sum.IsValid()) { Box->AddSlot().AutoHeight()[Line(LOCTEXT("EndReading2", "Reading the boards..."), WRUI::Ink3, 14)]; return; }
			const TArray<TSharedPtr<FJsonValue>>* L = DArr(Sum, TEXT("boards"));
			if (!L || L->Num() == 0) { Box->AddSlot().AutoHeight()[Line(LOCTEXT("EndNoBoards", "No kills on this world's boards yet. Every great boss, lesser boss, portal guardian and war-hall is timed from the first blow."), WRUI::Ink2, 14)]; return; }
			Box->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f * U)[Line(LOCTEXT("EndBoardsAbout", "Every board on this world, its fastest kill and yours. A stone tablet by each boss shows its top three."), WRUI::Ink3, 12)];
			const FString Me = PC.IsValid() && PC->PlayerState ? PC->PlayerState->GetPlayerName() : FString();
			for (const auto& V : *L)
			{
				const TSharedPtr<FJsonObject> R = V->AsObject();
				const FName Id(*DStr(R, TEXT("id")));
				const TSharedPtr<FJsonObject> Best = DChild(R, TEXT("best")), Mine = DChild(R, TEXT("mine"));
				const auto Who = [](const TSharedPtr<FJsonObject>& E) { TArray<FString> N; if (const TArray<TSharedPtr<FJsonValue>>* A = DArr(E, TEXT("names"))) for (const auto& X : *A) N.Add(X->AsString()); return FString::Join(N, TEXT(", ")); };
				Box->AddSlot().AutoHeight().Padding(0.f, 3.f * U)
				[
					SNew(SBorder).Padding(FMargin(12.f * U, 9.f * U)).BorderImage(WRUI::Box(FLinearColor(0.05f, 0.046f, 0.044f), 5.f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f * U, 0.f)[SNew(SSigil).Id(Id).Size(40.f).Colour(FLinearColor(0.78f, 0.6f, 0.36f))]
						+ SHorizontalBox::Slot().FillWidth(1.f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(FText::FromString(FString::Printf(TEXT("%s  ·  %d kills"), *DStr(R, TEXT("name")), FMath::RoundToInt(DNum(R, TEXT("kills"), 0.f)))), WRUI::Body(15, 700), WRUI::Ink)]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f * U, 0.f, 0.f)[Line(FText::FromString(Best.IsValid() ? FString::Printf(TEXT("Fastest: %s by %s"), *WRDeeds::Clock(DNum(Best, TEXT("secs"), 0.f)), *Who(Best)) : FString()), WRUI::Ink2, 12)]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f * U, 0.f, 0.f)[Line(FText::FromString(Mine.IsValid() ? FString::Printf(TEXT("Yours: %s"), *WRDeeds::Clock(DNum(Mine, TEXT("secs"), 0.f))) : FString(TEXT("Not on this board yet"))), Mine.IsValid() ? WRUI::Brass : WRUI::Ink3, 12, 600)]
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(130.f * U)[Button(LOCTEXT("EndOpenBoard", "The board"), [this, Id]() { Board = Id; Send(PC, FString::Printf(TEXT("deeds records %s 0"), *Id.ToString())); Rebuild(); }, false, true, 36.f)]]
					]
				];
			}
		}

		// --- Titles

		void Titles()
		{
			const UWRDeedsComponent* D = DeedsOf(PC);
			TArray<FString> All = D->Titles;
			if (const UWRArtifactComponent* Art = UWRArtifactComponent::For(StateOf(PC))) for (const FString& T : Art->Titles) All.AddUnique(T);
			Box->AddSlot().AutoHeight()[WRUI::Text(FText::FromString(D->ActiveTitle.IsEmpty() ? FString(TEXT("You wear no title.")) : FString::Printf(TEXT("You wear: %s"), *D->ActiveTitle)), WRUI::Title(18), WRUI::Ink)];
			Box->AddSlot().AutoHeight().Padding(0.f, 4.f * U, 0.f, 12.f * U)[Line(LOCTEXT("EndTitleAbout", "A worn title shows under your name and before it in chat. Titles come from deeds, prestige marks, raids, the Banes of your geis records and the path masters' last tasks."), WRUI::Ink3, 12)];
			if (All.Num() == 0) { Box->AddSlot().AutoHeight()[Line(LOCTEXT("EndNoTitles", "No titles yet."), WRUI::Ink2, 14)]; return; }
			for (const FString& T : All)
			{
				const bool bOn = D->ActiveTitle == T;
				Box->AddSlot().AutoHeight().Padding(0.f, 2.f * U)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[WRUI::Text(FText::FromString(T), WRUI::Body(15, bOn ? 800 : 600), bOn ? WRUI::Brass : WRUI::Ink)]
					+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(130.f * U)[Button(bOn ? LOCTEXT("EndWorn", "Worn") : LOCTEXT("EndWear", "Wear"), [this, T]() { Send(PC, FString::Printf(TEXT("deeds title %s"), *T)); }, !bOn, !bOn, 36.f)]]
				];
			}
			if (!D->ActiveTitle.IsEmpty()) Box->AddSlot().AutoHeight().Padding(0.f, 10.f * U, 0.f, 0.f)[SNew(SBox).WidthOverride(170.f * U).HAlign(HAlign_Left)[Button(LOCTEXT("EndWearNone", "Wear none"), [this]() { Send(PC, TEXT("deeds title")); }, false, true, 36.f)]];
		}

		// --- Cosmetics

		void Cosmetics()
		{
			const UWRDeedsComponent* D = DeedsOf(PC);
			Box->AddSlot().AutoHeight()[Line(LOCTEXT("EndCosAbout", "Only hard things give these: deathless kills of the great bosses, the war-halls' champions, prestige. Nothing here is sold."), WRUI::Ink3, 12)];
			struct FList { const TCHAR* Field; const TCHAR* Kind; FText Head; FName Worn; };
			const FList Lists[] = {
				{ TEXT("glows"), TEXT("glow"), LOCTEXT("EndGlows", "WEAPON GLOWS  ·  AN ELEMENT EDGE ON WHAT YOU HOLD"), D->WornGlow },
				{ TEXT("dyes"), TEXT("dye"), LOCTEXT("EndDyes", "ARMOUR DYES  ·  YOUR OUTFIT'S TRIM"), D->WornDye },
				{ TEXT("marks"), TEXT("mark"), LOCTEXT("EndRaidMarks", "RAID MARKS  ·  IN YOUR ARMOUR'S SEAMS"), D->WornMark },
			};
			for (const FList& L : Lists)
			{
				Box->AddSlot().AutoHeight().Padding(0.f, 14.f * U, 0.f, 4.f * U)[Heading(L.Head)];
				const TArray<TSharedPtr<FJsonValue>>* Rows = DArr(WRDeeds::Cosmetics(), L.Field);
				int32 Shown = 0;
				if (Rows)
				{
					for (const auto& V : *Rows)
					{
						const TSharedPtr<FJsonObject> R = V->AsObject();
						if (!DBool(R, TEXT("live"))) continue;
						++Shown;
						const FName Id(*DStr(R, TEXT("id")));
						const bool bOwn = D->Cosmetics.Contains(Id), bOn = L.Worn == Id;
						const TSharedPtr<FJsonObject> Seam = DChild(R, TEXT("seam"));
						const TSharedPtr<FJsonObject> Inlay = DChild(R, TEXT("inlay"));
						const FLinearColor Swatch = DLin(Seam.IsValid() ? DColour(Seam, TEXT("colour"), FColor::White) : Inlay.IsValid() ? DColour(Inlay, TEXT("colour"), FColor::White) : DColour(R, TEXT("colour"), FColor::White));
						FString From;
						if (!DStr(R, TEXT("boss")).IsEmpty()) From = FString::Printf(TEXT("From a deathless kill of %s"), *CreatureName(FName(*DStr(R, TEXT("boss")))));
						else if (!DStr(R, TEXT("champion")).IsEmpty()) From = FString::Printf(TEXT("Now and then from %s in %s"), *CreatureName(FName(*DStr(R, TEXT("champion")))), *WRWarHall::Name(FName(*DStr(R, TEXT("raid")))));
						else if (!DStr(R, TEXT("raid")).IsEmpty()) From = FString::Printf(TEXT("Now and then from the champions of %s"), *WRWarHall::Name(FName(*DStr(R, TEXT("raid")))));
						const FString Kind = L.Kind;
						Box->AddSlot().AutoHeight().Padding(0.f, 2.f * U)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f * U, 0.f)[SNew(SBox).WidthOverride(26.f * U).HeightOverride(26.f * U)[SNew(SImage).Image(WRUI::Box(bOwn ? Swatch : Swatch * 0.25f, 5.f, WRUI::Brass2, 1.f))]]
							+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(FText::FromString(DStr(R, TEXT("name"))), WRUI::Body(14, 700), bOwn ? WRUI::Ink : WRUI::Ink3)]
								+ SVerticalBox::Slot().AutoHeight()[Line(FText::FromString(bOwn ? DStr(R, TEXT("line"), From) : From), WRUI::Ink3, 12)]
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								SNew(SBox).WidthOverride(130.f * U)
								[
									Button(!bOwn ? LOCTEXT("EndLocked", "Not yet") : bOn ? LOCTEXT("EndTakeOff", "Take off") : LOCTEXT("EndWear2", "Wear"),
										[this, Kind, Id, bOn]() { Send(PC, FString::Printf(TEXT("deeds wear %s %s"), *Kind, bOn ? TEXT("none") : *Id.ToString())); }, bOwn && !bOn, bOwn, 34.f)
								]
							]
						];
					}
				}
				if (Shown == 0) Box->AddSlot().AutoHeight()[Line(LOCTEXT("EndNoneYet", "None in this world yet."), WRUI::Ink3, 12)];
			}
			// Banner sigils: any boss you've beaten.
			Box->AddSlot().AutoHeight().Padding(0.f, 14.f * U, 0.f, 4.f * U)[Heading(LOCTEXT("EndSigils", "BANNER SIGIL  ·  WOVEN ON YOUR CLAIM BANNERS AND DRAWN ON YOUR CLAIM'S MAP MARK"))];
			TSharedRef<SWrapBox> Sigils = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8.f * U, 8.f * U));
			int32 N = 0;
			for (const FName& B : D->Bosses)
			{
				if (WRDeeds::Sigil(B).Num() == 0) continue;
				++N;
				const bool bOn = D->BannerSigil == B;
				Sigils->AddSlot()
				[
					Card(SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[SNew(SSigil).Id(B).Size(54.f).Colour(bOn ? WRUI::Brass : WRUI::Ink2)]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 4.f * U, 0.f, 0.f)[WRUI::Text(FText::FromString(CreatureName(B)), WRUI::Body(12, 700), bOn ? WRUI::Brass : WRUI::Ink2)],
						[this, B, bOn]() { Send(PC, FString::Printf(TEXT("deeds wear sigil %s"), bOn ? TEXT("none") : *B.ToString())); }, [bOn]() { return bOn; })
				];
			}
			if (N == 0) Box->AddSlot().AutoHeight()[Line(LOCTEXT("EndNoSigils", "Beat a boss and its sigil is yours to weave."), WRUI::Ink3, 12)];
			else Box->AddSlot().AutoHeight()[Sigils];
		}

		// --- Marks

		void Marks()
		{
			const UWRDeedsComponent* D = DeedsOf(PC);
			const AWRPlayerState* PS = StateOf(PC);
			Box->AddSlot().AutoHeight()[Line(LOCTEXT("EndMarksAbout", "Bring a path to 100 points and return it at its altar: it starts again, and you carry one mark of it for good (three at most). Marks work while that path is your main path or subclass, and show as gold threads in its marks on your skin."), WRUI::Ink3, 12)];
			int32 Shown = 0;
			for (const FWRPathDef& P : FWRPathRegistry::Get().All())
			{
				const FWRPrestigeRep* R = D->MarksOn(P.Id);
				const bool bActive = PS && PS->GetPaths()->IsActivePath(P.Id);
				if (!R && !bActive) continue;
				++Shown;
				const FName Id = P.Id;
				TArray<FString> Perks;
				if (R && R->Keener) Perks.Add(FString::Printf(TEXT("Keener ×%d (+%d%% damage)"), R->Keener, FMath::RoundToInt(R->Keener * WRTune(TEXT("prestige.damage"), 0.05f) * 100.f)));
				if (R && R->Deeper) Perks.Add(FString::Printf(TEXT("Deeper ×%d (+%d%% resource)"), R->Deeper, FMath::RoundToInt(R->Deeper * WRTune(TEXT("prestige.resource"), 0.1f) * 100.f)));
				if (R && R->Rooted) Perks.Add(FString::Printf(TEXT("Rooted (%s at half as your subclass)"), *P.RootPassiveName));
				Box->AddSlot().AutoHeight().Padding(0.f, 8.f * U, 0.f, 0.f)
				[
					SNew(SBorder).Padding(FMargin(12.f * U, 10.f * U)).BorderImage(WRUI::Box(FLinearColor(0.05f, 0.046f, 0.044f), 5.f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(FText::FromString(FString::Printf(TEXT("%s%s"), *P.Name, R && R->Total() >= 3 ? *FString::Printf(TEXT("  ·  %s"), *WRDeeds::SwornTitle(Id)) : TEXT(""))), WRUI::Title(16), PathColour(Id))]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f * U, 0.f, 0.f)[Line(FText::FromString(Perks.Num() ? FString::Join(Perks, TEXT("  ·  ")) : FString::Printf(TEXT("No marks yet  ·  %d / %d points"), PS ? PS->GetPaths()->GetEarned(Id) : 0, FMath::RoundToInt(WRTune(TEXT("prestige.atPoints"), 100.f)))), WRUI::Ink2, 12)]
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f * U, 0.f)[Knots([R]() { return R ? R->Total() : 0; })]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(SBox).WidthOverride(170.f * U)
							[
								Button(LOCTEXT("EndRestore2", "Restore layout"), [this, Id]() { Send(PC, FString::Printf(TEXT("deeds restore %s"), *Id.ToString())); }, false,
									bActive && D->Layouts.ContainsByPredicate([Id](const FWRPathLayout& L) { return L.Path == Id && L.Ranks.Num() > 0; }), 36.f)
							]
						]
					]
				];
			}
			if (Shown == 0) Box->AddSlot().AutoHeight().Padding(0.f, 10.f * U, 0.f, 0.f)[Line(LOCTEXT("EndNoMarks", "No marks yet."), WRUI::Ink2, 14)];
		}
	};

	// ------------------------------------------------------------------------------------------------ the toast

	class SDeedToast : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDeedToast) {}
			SLATE_ARGUMENT(TWeakObjectPtr<AWRPlayerController>, PC)
		SLATE_END_ARGS()
		void Construct(const FArguments& A)
		{
			PC = A._PC;
			ChildSlot
			[
				SNew(SBox).WidthOverride(460.f * U).Visibility_Lambda([this]() { return Row.IsValid() ? EVisibility::Visible : EVisibility::Collapsed; })
				[
					SNew(SButton).ButtonStyle(FCoreStyle::Get(), "NoBorder").ContentPadding(0.f).OnClicked_Lambda([this]() { Row.Reset(); return FReply::Handled(); })
					[
						SNew(SBorder).Padding(FMargin(14.f * U, 10.f * U)).BorderImage(WRUI::Box(FLinearColor(0.07f, 0.055f, 0.04f, 0.94f), 7.f, WRUI::Brass, 2.f))
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f * U, 0.f)
							[
								SNew(SBox).WidthOverride(46.f * U).HeightOverride(46.f * U)
								[
									SNew(SOverlay)
									+ SOverlay::Slot()[SNew(SImage).Image(WRUI::Box(FLinearColor(0.16f, 0.11f, 0.05f), 23.f * U, WRUI::Brass, 2.f))]
									+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(Glyph()); }), WRUI::Title(18), WRUI::Brass)]
								]
							]
							+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(FString::Printf(TEXT("DEED  ·  %s"), *CatName().ToUpper())); }), WRUI::Caps(10), WRUI::Brass)]
								+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(DStr(Row, TEXT("name"))); }), WRUI::Title(17), WRUI::Ink, true)]
								+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(DStr(Row, TEXT("text"))); }), WRUI::Body(12), WRUI::Ink2, true, true)]
							]
						]
					]
				]
			];
		}
		virtual void Tick(const FGeometry& Geo, const double Now, const float Dt) override
		{
			SCompoundWidget::Tick(Geo, Now, Dt);
			const float Show = WRTune(TEXT("deeds.toastSeconds"), 3.f);
			if (Row.IsValid() && FPlatformTime::Seconds() - Since > Show) Row.Reset();
			AWRPlayerController* P = PC.Get();
			if (!Row.IsValid() && P && P->DeedToasts.Num() > 0)
			{
				Row = WRDeeds::Deed(P->DeedToasts[0]);
				P->DeedToasts.RemoveAt(0);
				Since = FPlatformTime::Seconds();
			}
			SetRenderOpacity(Row.IsValid() ? Opacity() : 0.f);   // eases in, holds, eases out
		}
	private:
		TWeakObjectPtr<AWRPlayerController> PC;
		TSharedPtr<FJsonObject> Row;
		double Since = 0.0;
		float Opacity() const
		{
			const float T = float(FPlatformTime::Seconds() - Since), Show = WRTune(TEXT("deeds.toastSeconds"), 3.f);
			return FMath::Clamp(FMath::Min(T / 0.2f, (Show - T) / 0.35f), 0.f, 1.f);
		}
		FString CatName() const { return DStr(DChild(WRDeeds::Achievements(), TEXT("categories")), DStr(Row, TEXT("cat"))); }
		FString Glyph() const { const FString C = CatName(); return C.IsEmpty() ? FString(TEXT("*")) : C.Left(1); }
	};
}

TSharedRef<SWidget> WRUIEndgame::MakeToast(TWeakObjectPtr<AWRPlayerController> PC)
{
	return SNew(WRUIEndgameScreens::SDeedToast).PC(PC);
}

WR_REGISTER_SCREEN("altar", [](AWRPlayerController* PC) -> TSharedRef<SWidget> { return SNew(WRUIEndgameScreens::SAltarScreen).PC(PC); });
WR_REGISTER_SCREEN("respec", [](AWRPlayerController* PC) -> TSharedRef<SWidget> { return SNew(WRUIEndgameScreens::SRespecScreen).PC(PC); });
WR_REGISTER_SCREEN("warhall", [](AWRPlayerController* PC) -> TSharedRef<SWidget> { return SNew(WRUIEndgameScreens::SWarHallCard).PC(PC); });
WR_REGISTER_SCREEN("records", [](AWRPlayerController* PC) -> TSharedRef<SWidget> { return SNew(WRUIEndgameScreens::SRecordsScreen).PC(PC); });
WR_REGISTER_SCREEN("plaque", [](AWRPlayerController* PC) -> TSharedRef<SWidget> { return SNew(WRUIEndgameScreens::SPlaqueScreen).PC(PC); });
WR_REGISTER_JOURNAL_TAB("deeds", [](AWRPlayerController* PC) -> TSharedRef<SWidget> { return SNew(WRUIEndgameScreens::SDeedsTab).PC(PC); });

#undef LOCTEXT_NAMESPACE
