// The land claim totem (mockup "Land claim"): E on a claim totem. Left: the claim from above (the circle it holds now,
// the circle after an upgrade, your pieces where they really stand, the totem and Hearth). Right: who owns it and who
// can build, the upkeep (what your wood and stone pieces cost an hour, the totem's stock and how long it lasts,
// "claimdeposit" to stock everything you carry), the radius upgrade ("claimupgrade", costs as have / need), and the
// server's rules (PvP, decay). Every number comes from the totem and the pieces the server replicates.

#include "WRPlayerUI.h"
#include "WRUIStyle.h"
#include "WRUIWidgets.h"
#include "WRUIWindowKit.h"
#include "WRBuilding.h"
#include "WRCrafting.h"
#include "WRInventory.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRTuning.h"
#include "WRWorldDressing.h"
#include "EngineUtils.h"
#include "Fonts/FontMeasure.h"
#include "GameFramework/GameStateBase.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SWrapBox.h"

#define LOCTEXT_NAMESPACE "WRUI"

namespace WRUIClaimScreen
{
	// The kit's names, declared here (not a using-directive) so they win over helpers a unity build brings in from other files.
	using WRUIWindowKit::Window; using WRUIWindowKit::SBadge; using WRUIWindowKit::Circle; using WRUIWindowKit::DrawCircle; using WRUIWindowKit::FillShape;
	using WRUIWindowKit::Cubic; using WRUIWindowKit::Flame; using WRUIWindowKit::CardBrush; using WRUIWindowKit::Roman; using WRUIWindowKit::Num;
	using WRUIWindowKit::CompassWord; using WRUIWindowKit::CompassName; using WRUIWindowKit::Distance;
	using WRUI::U;

	FLinearColor Hex(const TCHAR* H) { return FLinearColor(FColor::FromHex(H)); }

	/** "7 h 30 m" / "45 m". */
	FString Duration(float Hours)
	{
		const int32 M = FMath::Max(0, FMath::FloorToInt(Hours * 60.f));
		return M >= 60 ? FString::Printf(TEXT("%d h %d m"), M / 60, M % 60) : FString::Printf(TEXT("%d m"), M);
	}

	FString ItemName(FName Id) { const FWRItemDef* Def = FWRItemRegistry::Get().Find(Id); return Def ? Def->Name.ToLower() : Id.ToString(); }

	/** The claim's numbers as the server works them out (AWRClaimTotem::CurrentUpkeepPerHour and its minute tick). */
	struct FUpkeep
	{
		FName Mat;
		int32 Pieces = 0;     // your pieces of this material inside the circle
		float PerPiece = 0.f; // pieces.json
		float PerHour = 0.f;  // after the server's multiplier and perks
		int32 Stock = 0;
		bool Pays() const { return PerHour > 0.f; }
		float Hours() const { return Pays() ? Stock / PerHour : TNumericLimits<float>::Max(); }
	};

	void DrawTextAt(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FText& Text, const FSlateFontInfo& Font, const FVector2D& Pos, const FLinearColor& Color)
	{
		const FVector2D Size = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text, Font);
		FSlateDrawElement::MakeText(Out, Layer, Geo.ToPaintGeometry(Size, FSlateLayoutTransform(Pos)), Text, Font, ESlateDrawEffect::None, Color);
	}

	FVector2D MeasureText(const FText& Text, const FSlateFontInfo& Font) { return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text, Font); }

	/** A tag on the map: dark box, text in it. Pos is the box's centre. */
	void DrawTag(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FText& Text, const FSlateFontInfo& Font, const FVector2D& Centre, const FLinearColor& Color, const FLinearColor& Edge = FLinearColor::Transparent)
	{
		const FVector2D S = MeasureText(Text, Font);
		const FVector2D Box = S + FVector2D(14.f, 4.f);
		WRUI::DrawBox(Out, Layer, Geo.ToPaintGeometry(Box, FSlateLayoutTransform(Centre - Box * 0.5f)), WRUI::Box(FLinearColor(0.04f, 0.035f, 0.025f, 0.9f), 3.f, Edge, Edge.A > 0.f ? 1.f : 0.f));
		DrawTextAt(Out, Layer + 1, Geo, Text, Font, Centre - S * 0.5f, Color);
	}

	/** A dashed circle (the radius after an upgrade). */
	void DashedCircle(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2D& C, float R, const FLinearColor& Color, float Thick, float Dash, float Gap)
	{
		const float Circ = 2.f * PI * R;
		const int32 N = FMath::Max(8, FMath::FloorToInt(Circ / (Dash + Gap)));
		const float Step = 2.f * PI / N, DashA = Step * Dash / (Dash + Gap);
		for (int32 i = 0; i < N; ++i)
		{
			TArray<FVector2D> Pts;
			for (int32 k = 0; k <= 3; ++k)
			{
				const float A = i * Step + DashA * k / 3.f;
				Pts.Add(C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R);
			}
			FSlateDrawElement::MakeLines(Out, Layer, Geo.ToPaintGeometry(), Pts, ESlateDrawEffect::None, Color, true, Thick);
		}
	}

	/**
	 * The claim from above: grid lines every 10 m, the claimed circle (green) and the one after an upgrade (dashed
	 * brass), your pieces at their real places and turns, the totem and the Hearth. North is up, as on the map.
	 */
	class SClaimMap : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SClaimMap) {}
			SLATE_ARGUMENT(TFunction<const AWRClaimTotem*()>, Totem)
		SLATE_END_ARGS()
		void Construct(const FArguments& InArgs) { Totem = InArgs._Totem; }
		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(506.f * U, 550.f * U); }

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override
		{
			const FVector2D Size = Geo.GetLocalSize();
			// The ground: dark stone, lighter in the middle.
			WRUI::DrawBox(Out, Layer, Geo.ToPaintGeometry(), WRUI::Box(Hex(TEXT("1E2023")), 4.f, FLinearColor::Black, 1.f));
			FSlateDrawElement::MakeBox(Out, Layer, Geo.ToPaintGeometry(), WRUI::Image(TEXT("stone")), ESlateDrawEffect::None, FLinearColor(0.55f, 0.55f, 0.58f, 0.55f));
			const FVector2D Mid = Size * 0.5f;
			for (int32 i = 6; i >= 1; --i) DrawCircle(Out, Layer, Geo, Mid, Size.GetMax() * 0.12f * i, FLinearColor(0.f, 0.f, 0.f, 0.06f));

			const AWRClaimTotem* T = Totem ? Totem() : nullptr;
			if (!T) return Layer + 1;
			const FWRBuildRegistry& Reg = FWRBuildRegistry::Get();
			const float Next = T->Radius < Reg.ClaimMaxRadius ? FMath::Min(Reg.ClaimMaxRadius, T->Radius + Reg.ClaimRadiusPerUpgrade) : 0.f;
			const float Reach = FMath::Max(T->Radius, Next);
			const float Scale = (FMath::Min(Size.X, Size.Y) * 0.5f - 20.f * U) / Reach;   // local units per cm
			const FVector2D C = Mid + FVector2D(0.f, 8.f * U);
			const FVector Origin = T->GetActorLocation();
			const auto ToMap = [&](const FVector& W) { return C + FVector2D(W.X - Origin.X, W.Y - Origin.Y) * Scale; };
			const int32 L = Layer + 1;

			// Grid every 10 m, through the totem.
			const float Cell = 1000.f * Scale;
			for (float X = FMath::Fmod(C.X, Cell); X < Size.X; X += Cell)
			{
				FSlateDrawElement::MakeLines(Out, L, Geo.ToPaintGeometry(), TArray<FVector2D>{ FVector2D(X, 0.f), FVector2D(X, Size.Y) }, ESlateDrawEffect::None, FLinearColor(1.f, 1.f, 1.f, 0.06f), true, 1.f);
			}
			for (float Y = FMath::Fmod(C.Y, Cell); Y < Size.Y; Y += Cell)
			{
				FSlateDrawElement::MakeLines(Out, L, Geo.ToPaintGeometry(), TArray<FVector2D>{ FVector2D(0.f, Y), FVector2D(Size.X, Y) }, ESlateDrawEffect::None, FLinearColor(1.f, 1.f, 1.f, 0.06f), true, 1.f);
			}

			// The circles.
			const float R = T->Radius * Scale;
			if (Next > 0.f) DashedCircle(Out, L + 1, Geo, C, Next * Scale, WRUI::Alpha(WRUI::Brass, 0.85f), 2.f, 10.f, 8.f);
			DrawCircle(Out, L + 1, Geo, C, R, WRUI::Alpha(T->bUnpaid ? WRUI::Bad : WRUI::Good, 0.11f), T->bUnpaid ? WRUI::Bad : WRUI::Good, 2.5f);

			// Your pieces: floors and roofs as cells, walls as edges, the rest as small blocks.
			TArray<const AWRBuildPiece*> Cells, Edges, Things;
			for (TActorIterator<AWRBuildPiece> It(T->GetWorld()); It; ++It)
			{
				const FWRPieceDef* Def = It->GetDef();
				if (!Def || It->bGhost || It->OwnerName != T->OwnerName || Def->Kind == TEXT("totem") || FVector::Dist2D(It->GetActorLocation(), Origin) > Reach + Reg.GridSize) continue;
				const FName S = Def->Shape;
				if (S == TEXT("foundation") || S == TEXT("floor") || S == TEXT("roof") || S == TEXT("stairs") || S == TEXT("tower")) Cells.Add(*It);
				else if (Def->Snaps() && S != TEXT("pillar")) Edges.Add(*It);
				else Things.Add(*It);
			}
			const auto Block = [&](const AWRBuildPiece* P, const FVector2D& Extent, const FLinearColor& Fill, const FLinearColor& Edge, int32 Lay)
			{
				const FVector2D Px = FVector2D::Max(Extent * Scale, FVector2D(3.f, 3.f));
				const FGeometry G = Geo.MakeChild(Px, FSlateLayoutTransform(ToMap(P->GetActorLocation()) - Px * 0.5f), FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(P->GetActorRotation().Yaw))), FVector2D(0.5f, 0.5f));
				WRUI::DrawBox(Out, Lay, G.ToPaintGeometry(), WRUI::Box(Fill, 1.5f, Edge, 1.2f));
			};
			const FLinearColor PieceFill = Hex(TEXT("121315")), PieceEdge = Hex(TEXT("5D646B"));
			for (const AWRBuildPiece* P : Cells) Block(P, FVector2D(Reg.GridSize, Reg.GridSize) * 0.97f, PieceFill, PieceEdge, L + 2);
			for (const AWRBuildPiece* P : Edges) Block(P, FVector2D(Reg.GridSize, 45.f), Hex(TEXT("3A3F45")), Hex(TEXT("0A0B0C")), L + 3);
			const AWRHearth* Hearth = nullptr;
			for (const AWRBuildPiece* P : Things)
			{
				const FWRPieceDef* Def = P->GetDef();
				if (Def->Kind == TEXT("hearth")) { Hearth = Cast<AWRHearth>(P->GetChild()); continue; }
				const bool bStation = Def->Kind == TEXT("station");
				Block(P, FVector2D(Def->Shape == TEXT("pillar") ? 50.f : 130.f, Def->Shape == TEXT("pillar") ? 50.f : 130.f), bStation ? Hex(TEXT("2A1D12")) : PieceFill, bStation ? WRUI::Brass2 : PieceEdge, L + 4);
			}
			// The Hearth (its piece's child only exists on the server: find it by place).
			for (const AWRBuildPiece* P : Things)
			{
				if (P->GetDef()->Kind != TEXT("hearth")) continue;
				const FVector2D H = ToMap(P->GetActorLocation());
				DrawCircle(Out, L + 5, Geo, H, 11.f * U, WRUI::Alpha(WRUI::Ember, 0.22f));
				DrawCircle(Out, L + 5, Geo, H, 7.f * U, Hex(TEXT("5A2412")), WRUI::Ember, 2.f);
				// Its name to the right, clear of the pieces around it.
				const FText HT = NSLOCTEXT("WRUI", "MapHearth", "Hearth");
				DrawTag(Out, L + 6, Geo, HT, WRUI::Body(12, 700), H + FVector2D(MeasureText(HT, WRUI::Body(12, 700)).X * 0.5f + 20.f * U, 0.f), WRUI::Ember);
			}

			// Radius line and its length, east of the totem.
			FSlateDrawElement::MakeLines(Out, L + 6, Geo.ToPaintGeometry(), TArray<FVector2D>{ C + FVector2D(14.f * U, 0.f), C + FVector2D(R, 0.f) }, ESlateDrawEffect::None, WRUI::Alpha(WRUI::Good, 0.8f), true, 1.5f);
			DrawTag(Out, L + 7, Geo, FText::FromString(FString::Printf(TEXT("%.0f m"), T->Radius / 100.f)), WRUI::Body(14, 700), C + FVector2D(R * 0.62f, -12.f * U), WRUI::Good);

			// The totem.
			DrawCircle(Out, L + 8, Geo, C, 16.f * U, WRUI::Alpha(WRUI::Brass, 0.18f));
			DrawCircle(Out, L + 8, Geo, C, 11.f * U, Hex(TEXT("2A1D12")), WRUI::Brass, 2.f);
			{
				const float S = 0.75f * U;
				const auto At = [&](float X, float Y) { return C + FVector2D(X - 12.f, Y - 12.f) * S; };
				FSlateDrawElement::MakeLines(Out, L + 9, Geo.ToPaintGeometry(), TArray<FVector2D>{ At(8.f, 20.f), At(8.f, 4.f) }, ESlateDrawEffect::None, WRUI::Ink, true, 1.6f);
				FillShape(Out, L + 9, Geo, { At(8.5f, 4.5f), At(18.f, 4.5f), At(15.5f, 8.f), At(18.f, 11.5f), At(8.5f, 11.5f) }, At(11.f, 8.f), WRUI::Good);
			}
			DrawTag(Out, L + 9, Geo, NSLOCTEXT("WRUI", "MapTotem", "Totem"), WRUI::Body(13, 700), C + FVector2D(-6.f * U, 26.f * U), WRUI::Ink);

			// The circle after an upgrade gets its label on top.
			if (Next > 0.f)
			{
				const float Y = FMath::Max(16.f * U, C.Y - Next * Scale);
				DrawTag(Out, L + 9, Geo, FText::FromString(FString::Printf(TEXT("%.0f m after upgrade"), Next / 100.f)), WRUI::Body(14, 700), FVector2D(C.X, Y), WRUI::Brass, WRUI::Brass2);
			}

			// North and the scale.
			{
				const FVector2D N(26.f * U, 22.f * U);
				FillShape(Out, L + 9, Geo, { N + FVector2D(0.f, -10.f) * U, N + FVector2D(7.f, 8.f) * U, N + FVector2D(0.f, 3.f) * U, N + FVector2D(-7.f, 8.f) * U }, N + FVector2D(0.f, 2.f) * U, WRUI::Ink);
				const FText NT = FText::FromString(TEXT("N"));
				const FSlateFontInfo NF = WRUI::Title(13);
				DrawTextAt(Out, L + 9, Geo, NT, NF, N + FVector2D(-MeasureText(NT, NF).X * 0.5f, 12.f * U), WRUI::Ink);

				const float Bar = 1000.f * Scale;
				const FVector2D B0(14.f * U, Size.Y - 18.f * U), B1 = B0 + FVector2D(Bar, 0.f);
				FSlateDrawElement::MakeLines(Out, L + 9, Geo.ToPaintGeometry(), TArray<FVector2D>{ B0, B1 }, ESlateDrawEffect::None, WRUI::Ink, true, 1.6f);
				FSlateDrawElement::MakeLines(Out, L + 9, Geo.ToPaintGeometry(), TArray<FVector2D>{ B0 - FVector2D(0.f, 6.f), B0 + FVector2D(0.f, 6.f) }, ESlateDrawEffect::None, WRUI::Ink, true, 1.6f);
				FSlateDrawElement::MakeLines(Out, L + 9, Geo.ToPaintGeometry(), TArray<FVector2D>{ B1 - FVector2D(0.f, 6.f), B1 + FVector2D(0.f, 6.f) }, ESlateDrawEffect::None, WRUI::Ink, true, 1.6f);
				const FText BT = FText::FromString(TEXT("10 m"));
				const FSlateFontInfo BF = WRUI::Body(13, 700);
				DrawTextAt(Out, L + 9, Geo, BT, BF, FVector2D((B0.X + B1.X) * 0.5f - MeasureText(BT, BF).X * 0.5f, B0.Y - 26.f * U), WRUI::Ink);
			}
			return L + 10;
		}

	private:
		TFunction<const AWRClaimTotem*()> Totem;
	};

	class SClaimScreen : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SClaimScreen) {}
			SLATE_ARGUMENT(TWeakObjectPtr<AWRPlayerController>, PC)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			PC = InArgs._PC;
			Totem = FindTotem();
			ChildSlot
			[
				Window(PC, SNew(SBadge).bFlag(true), LOCTEXT("ClaimTitle", "Land claim"),
					TAttribute<FText>::CreateLambda([this]() { return Subtitle(); }),
					TAttribute<FText>::CreateLambda([this]() { return HeaderChip(); }),
					TAttribute<FLinearColor>::CreateLambda([this]() { const AWRClaimTotem* T = Totem.Get(); return T && T->bUnpaid ? WRUI::Bad : WRUI::Good; }),
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(540.f * U)[LeftColumn()]]
					+ SHorizontalBox::Slot().FillWidth(1.f).Padding(24.f * U, 0.f, 0.f, 0.f)[RightColumn()])
			];
			RebuildMembers();
		}

		virtual void Tick(const FGeometry& Geo, const double Time, const float Delta) override
		{
			SCompoundWidget::Tick(Geo, Time, Delta);
			Totem = FindTotem();
			Upkeep = ComputeUpkeep();
			RebuildMembers();
		}

	private:
		TWeakObjectPtr<AWRPlayerController> PC;
		TWeakObjectPtr<AWRClaimTotem> Totem;
		TArray<FUpkeep> Upkeep;
		TSharedPtr<SWrapBox> MemberChips;
		FString MembersShown = TEXT("-");

		// ---------------------------------------------------------------- state

		/** The totem of the piece E was used on (totems ride on their piece, which is their owner actor); else the claim you stand in. */
		AWRClaimTotem* FindTotem() const
		{
			AWRPlayerController* P = PC.Get();
			if (!P) return nullptr;
			const AWRBuildPiece* Piece = P->GetMenuPiece();
			for (TActorIterator<AWRClaimTotem> It(P->GetWorld()); Piece && It; ++It)
			{
				if (It->GetOwner() == Piece || FVector::Dist2D(It->GetActorLocation(), Piece->GetActorLocation()) < 120.f) return *It;
			}
			return P->GetPawn() ? AWRClaimTotem::FindAt(P->GetWorld(), P->GetPawn()->GetActorLocation()) : nullptr;
		}

		FString Me() const { return PC.IsValid() && PC->PlayerState ? PC->PlayerState->GetPlayerName() : FString(); }
		const UWRInventoryComponent* Bag() const { const AWRPlayerState* PS = PC.IsValid() ? PC->GetPlayerState<AWRPlayerState>() : nullptr; return PS ? PS->GetInventory() : nullptr; }
		int32 Have(FName Item) const { const UWRInventoryComponent* B = Bag(); return B ? B->CountOf(Item) : 0; }

		/** A player's clan from their replicated player state (empty while they're offline or clanless). */
		FString ClanOf(const FString& Player) const
		{
			const AGameStateBase* GS = PC.IsValid() && PC->GetWorld() ? PC->GetWorld()->GetGameState() : nullptr;
			for (const APlayerState* P : GS ? GS->PlayerArray : TArray<TObjectPtr<APlayerState>>())
			{
				const AWRPlayerState* W = Cast<AWRPlayerState>(P);
				if (W && W->GetPlayerName() == Player) return W->ClanName;
			}
			return FString();
		}
		bool IsOwner() const { const AWRClaimTotem* T = Totem.Get(); return T && T->OwnerName == Me(); }
		/** The owner or their clan may upgrade (the server's CanShare); only the owner stocks it. */
		bool CanShare() const
		{
			const AWRClaimTotem* T = Totem.Get();
			if (!T) return false;
			const FString Clan = ClanOf(T->OwnerName);
			return T->OwnerName == Me() || (!Clan.IsEmpty() && Clan == ClanOf(Me()));
		}

		TArray<FUpkeep> ComputeUpkeep() const
		{
			TArray<FUpkeep> Out;
			const AWRClaimTotem* T = Totem.Get();
			if (!T) return Out;
			const TMap<FName, float> PerHour = T->CurrentUpkeepPerHour();
			for (const FName Mat : { FName(TEXT("wood")), FName(TEXT("stone")) })
			{
				FUpkeep U2;
				U2.Mat = Mat;
				U2.PerPiece = FWRBuildRegistry::Get().UpkeepPerPiecePerHour.FindRef(Mat);
				U2.PerHour = PerHour.FindRef(Mat);
				U2.Stock = FWRInventoryRules::CountOf(T->UpkeepStorage, Mat);
				for (TActorIterator<AWRBuildPiece> It(T->GetWorld()); It; ++It)
				{
					const FWRPieceDef* Def = It->GetDef();
					if (Def && !It->bGhost && It->OwnerName == T->OwnerName && Def->Material == Mat && FVector::Dist2D(It->GetActorLocation(), T->GetActorLocation()) <= T->Radius) ++U2.Pieces;
				}
				Out.Add(U2);
			}
			return Out;
		}

		/** Hours until the first material runs out (max float: nothing to pay). */
		float HoursLeft(const TArray<FUpkeep>& Of, FName* OutFirst = nullptr) const
		{
			float Best = TNumericLimits<float>::Max();
			for (const FUpkeep& X : Of)
			{
				if (X.Pays() && X.Hours() < Best) { Best = X.Hours(); if (OutFirst) *OutFirst = X.Mat; }
			}
			return Best;
		}

		int32 PieceCount() const
		{
			const AWRClaimTotem* T = Totem.Get();
			int32 N = 0;
			for (TActorIterator<AWRBuildPiece> It(T ? T->GetWorld() : nullptr); T && It; ++It)
			{
				const FWRPieceDef* Def = It->GetDef();
				if (Def && !It->bGhost && It->OwnerName == T->OwnerName && Def->Kind != TEXT("totem") && FVector::Dist2D(It->GetActorLocation(), T->GetActorLocation()) <= T->Radius) ++N;
			}
			return N;
		}

		FText Subtitle() const
		{
			const AWRClaimTotem* T = Totem.Get();
			if (!T) return LOCTEXT("ClaimNone", "You're not in a land claim");
			// The nearest of the island's named places, for where it is.
			const FWRPoi* Near = nullptr;
			float Best = TNumericLimits<float>::Max();
			for (const FWRPoi& Poi : FWRPoi::All)
			{
				const float D = FVector::Dist2D(Poi.Location, T->GetActorLocation());
				if (D < Best) { Best = D; Near = &Poi; }
			}
			FString Line = FString::Printf(TEXT("Claim totem at %s's base"), *T->OwnerName);
			if (Near) Line += FString::Printf(TEXT(" · %s %s of %s"), *Distance(Best), CompassWord(T->GetActorLocation() - Near->Location), *Near->Name);
			return FText::FromString(Line);
		}

		FText HeaderChip() const
		{
			const AWRClaimTotem* T = Totem.Get();
			if (!T) return FText::GetEmpty();
			if (T->bUnpaid) return LOCTEXT("ChipUnpaid", "Unpaid · decaying");
			const int32 N = PieceCount();
			return FText::FromString(FString::Printf(TEXT("Radius %.0f m · %d piece%s"), T->Radius / 100.f, N, N == 1 ? TEXT("") : TEXT("s")));
		}

		// ---------------------------------------------------------------- left: the map

		TSharedRef<SWidget> Swatch(TSharedRef<SWidget> Mark, const TAttribute<FText>& Label)
		{
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 7.f * U, 0.f)[Mark]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(Label, WRUI::Body(12, 500), WRUI::Ink2)];
		}

		TSharedRef<SWidget> LeftColumn()
		{
			const auto Line = [](const FLinearColor& Col, float Thick, bool bDashed)
			{
				TSharedRef<SHorizontalBox> Box = SNew(SHorizontalBox);
				for (int32 i = 0; i < (bDashed ? 3 : 1); ++i)
				{
					Box->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(i > 0 ? 3.f : 0.f, 0.f, 0.f, 0.f)
					[
						SNew(SBox).WidthOverride(bDashed ? 6.f * U : 22.f * U).HeightOverride(Thick)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity(Col)]
					];
				}
				return Box;
			};
			return SNew(SWRPanel).Kind(EWRPanel::Inset).Padding(FMargin(16.f * U, 14.f * U))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Bottom)[WRUI::Text(LOCTEXT("FromAbove", "Your claim from above"), WRUI::Caps(12), WRUI::Ink2)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)[WRUI::Text(LOCTEXT("GridEvery", "Grid lines every 10 m"), WRUI::Body(13, 500), WRUI::Ink3)]
				]
				+ SVerticalBox::Slot().FillHeight(1.f).Padding(0.f, 10.f * U)
				[
					SNew(SClaimMap).Totem([this]() -> const AWRClaimTotem* { return Totem.Get(); })
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(14.f * U, 8.f * U))
					+ SWrapBox::Slot()
					[
						Swatch(SNew(SBox).WidthOverride(14.f * U).HeightOverride(14.f * U)[SNew(SImage).Image(WRUI::Box(Hex(TEXT("121315")), 2.f, Hex(TEXT("5D646B")), 1.f))],
							TAttribute<FText>::CreateLambda([this]() { return FText::Format(LOCTEXT("LegendPieces", "Your pieces ({0})"), PieceCount()); }))
					]
					+ SWrapBox::Slot()
					[
						Swatch(SNew(SBox).WidthOverride(14.f * U).HeightOverride(14.f * U)[SNew(SImage).Image(Circle(Hex(TEXT("2A1D12")), WRUI::Brass, 2.f))], LOCTEXT("LegendTotem", "Claim totem"))
					]
					+ SWrapBox::Slot()
					[
						Swatch(Line(WRUI::Good, 2.5f, false), TAttribute<FText>::CreateLambda([this]() { const AWRClaimTotem* T = Totem.Get(); return FText::FromString(FString::Printf(TEXT("Claimed now: %.0f m"), T ? T->Radius / 100.f : 0.f)); }))
					]
					+ SWrapBox::Slot()
					[
						SNew(SBox).Visibility_Lambda([this]() { return NextRadius() > 0.f ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
						[
							Swatch(Line(WRUI::Brass, 2.f, true), TAttribute<FText>::CreateLambda([this]() { return FText::FromString(FString::Printf(TEXT("After upgrade: %.0f m"), NextRadius() / 100.f)); }))
						]
					]
				]
			];
		}

		float NextRadius() const
		{
			const AWRClaimTotem* T = Totem.Get();
			const FWRBuildRegistry& Reg = FWRBuildRegistry::Get();
			return T && T->Radius < Reg.ClaimMaxRadius ? FMath::Min(Reg.ClaimMaxRadius, T->Radius + Reg.ClaimRadiusPerUpgrade) : 0.f;
		}

		// ---------------------------------------------------------------- right: owner, upkeep, upgrade, rules

		TSharedRef<SWidget> Card(TSharedRef<SWidget> Content, const FMargin& Pad = FMargin(16.f * U, 12.f * U, 16.f * U, 14.f * U))
		{
			return SNew(SWRPanel).Kind(EWRPanel::Inset).Padding(Pad)[Content];
		}

		TSharedRef<SWidget> RightColumn()
		{
			return SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[OwnerCard()]
				+ SVerticalBox::Slot().FillHeight(1.f)[SNew(SSpacer)]
				+ SVerticalBox::Slot().AutoHeight()[UpkeepCard()]
				+ SVerticalBox::Slot().FillHeight(1.f)[SNew(SSpacer)]
				+ SVerticalBox::Slot().AutoHeight()[UpgradeCard()]
				+ SVerticalBox::Slot().FillHeight(1.f)[SNew(SSpacer)]
				+ SVerticalBox::Slot().AutoHeight()[RulesBlock()];
		}

		TSharedRef<SWidget> OwnerCard()
		{
			return Card(SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Bottom)[WRUI::Text(LOCTEXT("Owner", "Owner"), WRUI::Caps(12), WRUI::Ink2)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
					[
						WRUI::Text(TAttribute<FText>::CreateLambda([this]()
						{
							const AWRClaimTotem* T = Totem.Get();
							if (!T) return FText::GetEmpty();
							const FString Clan = ClanOf(T->OwnerName);
							return Clan.IsEmpty() ? FText::Format(LOCTEXT("OnlyOwner", "Only {0} can build here"), FText::FromString(T->OwnerName)) : FText::Format(LOCTEXT("ClanBuilds", "Clan {0} can build here"), FText::FromString(Clan));
						}), WRUI::Body(13, 500), WRUI::Ink2)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f * U, 0.f, 0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
					[
						WRUI::Text(TAttribute<FText>::CreateLambda([this]() { const AWRClaimTotem* T = Totem.Get(); return T ? FText::FromString(T->OwnerName) : FText::GetEmpty(); }), WRUI::Title(20), WRUI::Ink, true)
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(10.f * U, 0.f, 0.f, 2.f * U)
					[
						WRUI::Text(TAttribute<FText>::CreateLambda([this]()
						{
							const AWRClaimTotem* T = Totem.Get();
							const FString Clan = T ? ClanOf(T->OwnerName) : FString();
							return !T ? FText::GetEmpty() : Clan.IsEmpty() ? (IsOwner() ? LOCTEXT("YouNoClan", "· you, no clan") : LOCTEXT("NoClan", "· no clan")) : FText::FromString(TEXT("· Clan ") + Clan);
						}), WRUI::Body(15, 500), WRUI::Ink2)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f * U, 0.f, 0.f)
				[
					SAssignNew(MemberChips, SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8.f * U, 6.f * U))
				]);
		}

		/** The owner and their clanmates who are on (clans live on the server; the rest aren't known here). */
		void RebuildMembers()
		{
			const AWRClaimTotem* T = Totem.Get();
			if (!MemberChips.IsValid() || !T) return;
			TArray<FString> Names = { T->OwnerName };
			const FString Clan = ClanOf(T->OwnerName);
			if (const AGameStateBase* GS = T->GetWorld() ? T->GetWorld()->GetGameState() : nullptr)
			{
				for (const APlayerState* P : GS->PlayerArray)
				{
					const AWRPlayerState* W = Cast<AWRPlayerState>(P);
					if (W && !Clan.IsEmpty() && W->ClanName == Clan) Names.AddUnique(W->GetPlayerName());
				}
			}
			const FString Key = FString::Join(Names, TEXT(","));
			if (Key == MembersShown) return;
			MembersShown = Key;
			MemberChips->ClearChildren();
			static const TCHAR* Colours[] = { TEXT("E9A06A"), TEXT("9FD0F0"), TEXT("B7D68F"), TEXT("D6C49E"), TEXT("D8A6E0"), TEXT("E6D27A") };
			for (int32 i = 0; i < Names.Num(); ++i)
			{
				const bool bOwner = i == 0;
				const FString Initial = Names[i].Left(1).ToUpper();
				TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f * U, 0.f)
					[
						SNew(SBox).WidthOverride(24.f * U).HeightOverride(24.f * U)
						[
							SNew(SOverlay)
							+ SOverlay::Slot()[SNew(SImage).Image(Circle(Hex(Colours[i % UE_ARRAY_COUNT(Colours)])))]
							+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[WRUI::Text(FText::FromString(Initial), WRUI::Body(13, 800), Hex(TEXT("140D08")))]
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(FText::FromString(Names[i]), WRUI::Body(14, 700), WRUI::Ink)];
				if (bOwner)
				{
					Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(8.f * U, 0.f, 0.f, 0.f)[WRUI::Text(LOCTEXT("OwnerTag", "Owner"), WRUI::Caps(12), WRUI::Brass)];
				}
				MemberChips->AddSlot()
				[
					SNew(SBorder).BorderImage(WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.5f), 14.f * U, WRUI::Alpha(bOwner ? WRUI::Warn : WRUI::Ink2, 0.5f), 1.f)).Padding(FMargin(4.f * U, 3.f * U, 12.f * U, 3.f * U))
					[
						Row
					]
				];
			}
		}

		TSharedRef<SWidget> UpkeepRow(int32 Index)
		{
			// One material: "9 wood pieces x 2 wood = 18 wood an hour" (and the server's multiplier when it isn't 1).
			return SNew(SHorizontalBox).Visibility_Lambda([this, Index]() { return Upkeep.IsValidIndex(Index) && Upkeep[Index].Pieces > 0 ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f * U, 0.f)
				[
					SNew(SBox).WidthOverride(22.f * U).HeightOverride(22.f * U)[SNew(SImage).Image_Lambda([this, Index]() { return WRUI::Icon(Upkeep.IsValidIndex(Index) ? Upkeep[Index].Mat : NAME_None); })]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					WRUI::Text(TAttribute<FText>::CreateLambda([this, Index]()
					{
						if (!Upkeep.IsValidIndex(Index)) return FText::GetEmpty();
						const FUpkeep& X = Upkeep[Index];
						return FText::FromString(FString::Printf(TEXT("%d %s piece%s"), X.Pieces, *X.Mat.ToString(), X.Pieces == 1 ? TEXT("") : TEXT("s")));
					}), WRUI::Body(15, 800), WRUI::Ink)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(7.f * U, 0.f)
				[
					WRUI::Text(TAttribute<FText>::CreateLambda([this, Index]()
					{
						if (!Upkeep.IsValidIndex(Index)) return FText::GetEmpty();
						const FUpkeep& X = Upkeep[Index];
						const float Base = X.Pieces * X.PerPiece;
						const float Mult = Base > 0.f ? X.PerHour / Base : 1.f;
						FString S = FString::Printf(TEXT("x %g %s"), X.PerPiece, *X.Mat.ToString());
						if (!FMath::IsNearlyEqual(Mult, 1.f, 0.01f)) S += FString::Printf(TEXT(" x %g (server)"), FMath::RoundToFloat(Mult * 100.f) / 100.f);
						return FText::FromString(S + TEXT(" ="));
					}), WRUI::Body(15, 500), WRUI::Ink3)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					WRUI::Text(TAttribute<FText>::CreateLambda([this, Index]()
					{
						if (!Upkeep.IsValidIndex(Index)) return FText::GetEmpty();
						return FText::FromString(FString::Printf(TEXT("%g %s"), FMath::RoundToFloat(Upkeep[Index].PerHour * 10.f) / 10.f, *Upkeep[Index].Mat.ToString()));
					}), WRUI::Body(17, 800), WRUI::Ink)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.f * U, 0.f, 0.f, 0.f)[WRUI::Text(LOCTEXT("PerHour", "an hour"), WRUI::Body(15, 500), WRUI::Ink3)];
		}

		TSharedRef<SWidget> StockCell(int32 Index)
		{
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f * U, 0.f)
				[
					SNew(SWRSlot).Size(46.f * U).View_Lambda([this, Index]()
					{
						FWRSlotView V;
						if (Upkeep.IsValidIndex(Index)) { V.ItemId = Upkeep[Index].Mat; V.Count = Upkeep[Index].Stock; V.bShowCount = true; V.bDimmed = Upkeep[Index].Stock == 0; V.bSelected = IsFirstOut(Index); }
						return V;
					})
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						WRUI::Text(TAttribute<FText>::CreateLambda([this, Index]() { return Upkeep.IsValidIndex(Index) ? FText::FromString(FString::Printf(TEXT("%d %s stocked"), Upkeep[Index].Stock, *Upkeep[Index].Mat.ToString())) : FText::GetEmpty(); }), WRUI::Body(15, 700), WRUI::Ink)
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						WRUI::Text(TAttribute<FText>::CreateLambda([this, Index]()
						{
							if (!Upkeep.IsValidIndex(Index)) return FText::GetEmpty();
							const FUpkeep& X = Upkeep[Index];
							if (!X.Pays()) return FText::FromString(FString::Printf(TEXT("Not needed: no %s pieces"), *X.Mat.ToString()));
							return X.Stock <= 0 ? LOCTEXT("StockOut", "Run out") : FText::FromString(TEXT("Lasts ") + Duration(X.Hours()));
						}), WRUI::Body(13, 500), TAttribute<FSlateColor>::CreateLambda([this, Index]()
						{
							const bool bOut = Upkeep.IsValidIndex(Index) && Upkeep[Index].Pays() && Upkeep[Index].Stock <= 0;
							return FSlateColor(bOut ? WRUI::Bad : IsFirstOut(Index) ? WRUI::Warn : WRUI::Ink2);
						}))
					]
				];
		}

		bool IsFirstOut(int32 Index) const
		{
			FName First;
			const float H = HoursLeft(Upkeep, &First);
			int32 Paying = 0;
			for (const FUpkeep& X : Upkeep) Paying += X.Pays() ? 1 : 0;
			return Paying > 1 && H < TNumericLimits<float>::Max() && Upkeep.IsValidIndex(Index) && Upkeep[Index].Mat == First;
		}

		FLinearColor LastsColor() const
		{
			const AWRClaimTotem* T = Totem.Get();
			const float H = HoursLeft(Upkeep);
			return (T && T->bUnpaid) || H < 1.f / 60.f ? WRUI::Bad : H >= 12.f ? WRUI::Good : WRUI::Warn;
		}

		/** What stocking everything you carry would do (the server adds to the totem's stacks until they're full). */
		FText StockNote() const
		{
			const AWRClaimTotem* T = Totem.Get();
			if (!T) return FText::GetEmpty();
			if (!IsOwner()) return FText::Format(LOCTEXT("StockNotOwner", "Only {0} can stock this totem."), FText::FromString(T->OwnerName));
			const int32 Wood = Have(TEXT("wood")), Stone = Have(TEXT("stone"));
			if (Wood + Stone == 0) return LOCTEXT("StockNothing", "You carry no wood or stone to stock.");
			TArray<FWRItemStack> After = T->UpkeepStorage;
			const int32 WoodLeft = FWRInventoryRules::Add(After, TEXT("wood"), Wood);
			const int32 StoneLeft = FWRInventoryRules::Add(After, TEXT("stone"), Stone);
			TArray<FUpkeep> Then = Upkeep;
			for (FUpkeep& X : Then) X.Stock = FWRInventoryRules::CountOf(After, X.Mat);
			const float H = HoursLeft(Then);
			FString S = FString::Printf(TEXT("You carry %d wood and %d stone."), Wood, Stone);
			if (WoodLeft + StoneLeft >= Wood + Stone) return FText::FromString(S + TEXT(" The totem's stock is full."));
			if (H < TNumericLimits<float>::Max()) S += FString::Printf(TEXT(" Stocking it all makes it last %s."), *Duration(H));
			if (WoodLeft + StoneLeft > 0) S += FString::Printf(TEXT(" %d won't fit."), WoodLeft + StoneLeft);
			return FText::FromString(S);
		}

		TSharedRef<SWidget> UpkeepCard()
		{
			return Card(SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Bottom)[WRUI::Text(LOCTEXT("Upkeep", "Upkeep"), WRUI::Caps(12), WRUI::Ink2)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
					[
						WRUI::Text(TAttribute<FText>::CreateLambda([this]()
						{
							// The totem takes whole items as each hour's share comes due (a wood every 10 minutes at 6 an hour).
							bool bAny = false;
							for (const FUpkeep& X : Upkeep) bAny |= X.Pays();
							return bAny ? LOCTEXT("UpkeepDue", "Taken from the totem's stock a little at a time, as it comes due") : LOCTEXT("UpkeepFree", "None of your wood or stone pieces are inside");
						}), WRUI::Body(13, 500), WRUI::Ink3)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f * U, 0.f, 0.f)[UpkeepRow(0)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f * U, 0.f, 0.f)[UpkeepRow(1)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f * U, 0.f, 0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f)[StockCell(0)]
					+ SHorizontalBox::Slot().FillWidth(1.f).Padding(12.f * U, 0.f, 0.f, 0.f)[StockCell(1)]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f * U, 0.f, 0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Bottom)
					[
						WRUI::Text(TAttribute<FText>::CreateLambda([this]()
						{
							const AWRClaimTotem* T = Totem.Get();
							const float H = HoursLeft(Upkeep);
							if (T && T->bUnpaid) return LOCTEXT("LastsUnpaid", "Unpaid: your pieces are decaying");
							return H >= TNumericLimits<float>::Max() ? LOCTEXT("LastsFree", "Nothing to pay") : FText::FromString(TEXT("Lasts ") + Duration(H));
						}), WRUI::Body(17, 800), TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(LastsColor()); }))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
					[
						WRUI::Text(TAttribute<FText>::CreateLambda([this]()
						{
							FName First;
							HoursLeft(Upkeep, &First);
							int32 Paying = 0;
							for (const FUpkeep& X : Upkeep) Paying += X.Pays() ? 1 : 0;
							return Paying > 1 && !First.IsNone() ? FText::FromString(FString::Printf(TEXT("%s runs out first"), *(First.ToString().Left(1).ToUpper() + First.ToString().Mid(1)))) : FText::GetEmpty();
						}), WRUI::Body(13, 500), WRUI::Ink2)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 5.f * U, 0.f, 0.f)
				[
					SNew(SWRBar).Height(16.f * U).Color_Lambda([this]() { return LastsColor(); }).Fraction_Lambda([this]() { const float H = HoursLeft(Upkeep); return H >= TNumericLimits<float>::Max() ? 1.f : FMath::Clamp(H / 24.f, 0.f, 1.f); })
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f * U, 0.f, 0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f)[WRUI::Text(FText::FromString(TEXT("0 h")), WRUI::Body(12, 500), WRUI::Ink3)]
					+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Center)[WRUI::Text(FText::FromString(TEXT("12 h")), WRUI::Body(12, 500), WRUI::Ink3)]
					+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Right)[WRUI::Text(FText::FromString(TEXT("24 h")), WRUI::Body(12, 500), WRUI::Ink3)]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f * U, 0.f, 0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SWRButton).Primary(true).FontSize(14).MinHeight(44.f * U).Text(LOCTEXT("StockAll", "Stock all your wood and stone"))
						.IsEnabled_Lambda([this]() { return IsOwner() && Have(TEXT("wood")) + Have(TEXT("stone")) > 0; })
						.OnClicked_Lambda([this]() { if (PC.IsValid()) PC->SendCommand(TEXT("claimdeposit")); return FReply::Handled(); })
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f * U, 0.f, 0.f)
				[
					WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return StockNote(); }), WRUI::Body(13, 500), WRUI::Ink3, false, true)
				]);
		}

		TSharedRef<SWidget> UpgradeCard()
		{
			TSharedRef<SHorizontalBox> Costs = SNew(SHorizontalBox);
			for (const TPair<FName, int32>& Cost : FWRBuildRegistry::Get().ClaimUpgradeCost)
			{
				const FName Item = Cost.Key;
				const int32 Need = Cost.Value;
				Costs->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 14.f * U, 0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 5.f * U, 0.f)[SNew(SBox).WidthOverride(20.f * U).HeightOverride(20.f * U)[SNew(SImage).Image(WRUI::Icon(Item))]]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						WRUI::Text(TAttribute<FText>::CreateLambda([this, Item, Need]() { return FText::FromString(FString::Printf(TEXT("%d / %d %s"), Have(Item), Need, *ItemName(Item))); }), WRUI::Body(14, 800),
							TAttribute<FSlateColor>::CreateLambda([this, Item, Need]() { return FSlateColor(Have(Item) >= Need ? WRUI::Good : WRUI::Bad); }))
					]
				];
			}
			Costs->AddSlot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(LOCTEXT("FromBag", "have / need, from your bag"), WRUI::Body(14, 500), WRUI::Ink3)];

			return Card(SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						WRUI::Text(TAttribute<FText>::CreateLambda([this]()
						{
							const AWRClaimTotem* T = Totem.Get();
							return NextRadius() > 0.f ? FText::FromString(FString::Printf(TEXT("Upgrade radius to %.0f m"), NextRadius() / 100.f))
								: FText::FromString(FString::Printf(TEXT("Radius %.0f m: as large as a claim gets"), T ? T->Radius / 100.f : 0.f));
						}), WRUI::Body(16, 800), WRUI::Ink)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 5.f * U, 0.f, 0.f)
					[
						SNew(SBox).Visibility_Lambda([this]() { return NextRadius() > 0.f ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })[Costs]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 5.f * U, 0.f, 0.f)
					[
						WRUI::Text(TAttribute<FText>::CreateLambda([this]()
						{
							return !CanShare() && Totem.IsValid() ? LOCTEXT("UpgradeNotYours", "Only the owner and their clan can upgrade it.")
								: LOCTEXT("UpgradeNote", "Upkeep is per piece: a bigger circle only costs more if it takes in more of your pieces.");
						}), WRUI::Body(13, 500), WRUI::Ink3, false, true)
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(16.f * U, 0.f, 0.f, 0.f)
				[
					SNew(SWRButton).FontSize(15).MinHeight(44.f * U).Text(LOCTEXT("Upgrade", "Upgrade"))
					.IsEnabled_Lambda([this]()
					{
						if (!CanShare() || NextRadius() <= 0.f) return false;
						for (const TPair<FName, int32>& Cost : FWRBuildRegistry::Get().ClaimUpgradeCost) if (Have(Cost.Key) < Cost.Value) return false;
						return true;
					})
					.OnClicked_Lambda([this]() { if (PC.IsValid()) PC->SendCommand(TEXT("claimupgrade")); return FReply::Handled(); })
				], FMargin(16.f * U, 12.f * U));
		}

		TSharedRef<SWidget> RulesBlock()
		{
			const bool bPvp = WRTune(TEXT("server.pvp"), 1.f) > 0.f;
			const float Protect = WRTune(TEXT("server.offlineProtection"), 0.f);
			const int32 Decay = FMath::RoundToInt(FWRBuildRegistry::Get().DecayPerHourWhenUnpaid * 100.f);
			FString Rule = bPvp ? TEXT("PvP is on on this server.") : TEXT("PvP is off on this server.");
			Rule += Protect > 0.f ? FString::Printf(TEXT(" While you're all offline your pieces take %.0f%% less damage."), Protect * 100.f) : FString(TEXT(" Outsiders can still break your pieces."));
			return SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(4.f * U, 0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f * U, 0.f)
					[
						SNew(SBox).WidthOverride(16.f * U).HeightOverride(16.f * U)[SNew(SImage).Image(Circle(FLinearColor::Transparent, bPvp ? WRUI::Warn : WRUI::Good, 1.8f))]
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[WRUI::Text(FText::FromString(Rule), WRUI::Body(14, 500), WRUI::Ink2, false, true)]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f * U, 0.f, 0.f)
				[
					SNew(SBorder)
					.BorderImage_Lambda([this]() { const AWRClaimTotem* T = Totem.Get(); const FLinearColor C = T && T->bUnpaid ? WRUI::Bad : WRUI::Warn; return WRUI::Box(WRUI::Alpha(C, 0.08f), 4.f, WRUI::Alpha(C, 0.5f), 1.f); })
					.Padding(FMargin(12.f * U, 8.f * U))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f * U, 0.f)
						[
							WRUI::Text(FText::FromString(TEXT("!")), WRUI::Body(16, 800), TAttribute<FSlateColor>::CreateLambda([this]() { const AWRClaimTotem* T = Totem.Get(); return FSlateColor(T && T->bUnpaid ? WRUI::Bad : WRUI::Warn); }))
						]
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
						[
							WRUI::Text(FText::Format(LOCTEXT("DecayRule", "If upkeep runs out, your pieces lose {0}% of their health every hour until it's stocked again."), Decay), WRUI::Body(14, 500),
								TAttribute<FSlateColor>::CreateLambda([this]() { const AWRClaimTotem* T = Totem.Get(); return FSlateColor(T && T->bUnpaid ? WRUI::Bad : WRUI::Warn); }), false, true)
						]
					]
				];
		}
	};
}

WR_REGISTER_SCREEN("claim", [](AWRPlayerController* PC) -> TSharedRef<SWidget> { return SNew(WRUIClaimScreen::SClaimScreen).PC(PC); });

#undef LOCTEXT_NAMESPACE
