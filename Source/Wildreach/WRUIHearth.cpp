// The Hearth (mockup "Hearth"): E on your Hearth. What the Hearth really does: boss cores fed to it set its tech level,
// which is the tier of every crafting station in its claim (and the cap on gear upgrades); raiders can break it, and a
// broken Hearth stops those stations. Left: the Hearth itself (its fire, the level, the two major bosses' cores, its
// health, "Feed a core"); right: what each level unlocks at your stations (from the recipes) and the stations it runs.

#include "WRPlayerUI.h"
#include "WRUIStyle.h"
#include "WRUIWidgets.h"
#include "WRUIWindowKit.h"
#include "WRBuilding.h"
#include "WRCrafting.h"
#include "WRCreature.h"
#include "WRInventory.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRWorldDressing.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "WRUI"

namespace WRUIHearthScreen
{
	// The kit's names, declared here (not a using-directive) so they win over helpers a unity build brings in from other files.
	using WRUIWindowKit::Window; using WRUIWindowKit::SBadge; using WRUIWindowKit::Circle; using WRUIWindowKit::DrawCircle; using WRUIWindowKit::FillShape;
	using WRUIWindowKit::Cubic; using WRUIWindowKit::Flame; using WRUIWindowKit::CardBrush; using WRUIWindowKit::Roman; using WRUIWindowKit::Num;
	using WRUIWindowKit::CompassWord; using WRUIWindowKit::CompassName; using WRUIWindowKit::Distance;
	using WRUI::U;

	FLinearColor Hex(const TCHAR* H) { return FLinearColor(FColor::FromHex(H)); }

	/** A major boss and the core it drops (the cores the Hearth takes, one of each). */
	struct FCoreInfo
	{
		FName Core;
		FName Boss;
		FString BossName;   // "Vulkarn"
		FName Element;
	};

	/** The major bosses whose cores exist: those are the levels a Hearth can reach. */
	TArray<FCoreInfo> MajorCores()
	{
		TArray<FCoreInfo> Out;
		for (const FWRCreatureDef& D : FWRCreatureRegistry::Get().All())
		{
			const FName Core(*(TEXT("core_") + D.Id.ToString()));
			if (D.Role == TEXT("major boss") && FWRItemRegistry::Get().Find(Core))
			{
				FString Short = D.Name;
				Short.Split(TEXT(","), &Short, nullptr);
				Out.Add({ Core, D.Id, Short, D.Element });
			}
		}
		return Out;
	}

	/** Metal colours of the level medallions (copper, bronze, iron...). */
	FLinearColor MetalColor(const FString& Metal)
	{
		const FString M = Metal.ToLower();
		return M == TEXT("copper") ? Hex(TEXT("C97A48")) : M == TEXT("bronze") ? Hex(TEXT("C99A4A")) : M == TEXT("iron") ? Hex(TEXT("8F979E")) : WRUI::Brass;
	}

	/**
	 * The Hearth's fire in its stone ring (the mockup's emblem): an ember disc with a flame, a ring of carved ticks and a
	 * diamond per level it can reach, lit up to its level. Grey and cold when it's broken.
	 */
	class SEmblem : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SEmblem) : _Size(254.f * U) {}
			SLATE_ARGUMENT(float, Size)
			SLATE_ATTRIBUTE(int32, Level)
			SLATE_ATTRIBUTE(int32, MaxLevel)
			SLATE_ATTRIBUTE(bool, Broken)
		SLATE_END_ARGS()
		void Construct(const FArguments& InArgs) { Size = InArgs._Size; Level = InArgs._Level; MaxLevel = InArgs._MaxLevel; Broken = InArgs._Broken; }
		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(Size, Size); }
		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override
		{
			// Drawn in the mockup's 240-unit space, scaled to the widget.
			const float K = Size / 240.f;
			const FVector2D C(Size * 0.5f, Size * 0.5f);
			const bool bCold = Broken.Get();
			const auto Ring = [&](float R, float W, const FLinearColor& Col, int32 L) { DrawCircle(Out, L, Geo, C, (R + W * 0.5f) * K, FLinearColor::Transparent, Col, W * K); };

			// Its glow on the stone around it, and a shadow under it.
			for (int32 i = 10; i >= 1; --i)
			{
				DrawCircle(Out, Layer, Geo, C + FVector2D(0.f, 4.f * K), (117.f + i * 2.6f) * K, bCold ? FLinearColor(0.f, 0.f, 0.f, 0.03f) : WRUI::Alpha(WRUI::Ember, 0.022f));
			}
			DrawCircle(Out, Layer + 1, Geo, C, 117.f * K, Hex(TEXT("1B1E21")), Hex(TEXT("060708")), 2.f * K);
			DrawCircle(Out, Layer + 1, Geo, C - FVector2D(0.f, 16.f * K), 96.f * K, WRUI::Alpha(Hex(TEXT("3B3F45")), 0.35f));
			Ring(108.f, 4.f, Hex(TEXT("8E6B38")), Layer + 2);
			Ring(105.f, 1.2f, FLinearColor::Black, Layer + 2);
			Ring(111.f, 1.2f, FLinearColor::Black, Layer + 2);
			DrawCircle(Out, Layer + 2, Geo, C, 95.f * K, Hex(TEXT("202327")), Hex(TEXT("0A0B0C")), 2.f * K);
			// The carved ticks.
			for (int32 i = 0; i < 36; ++i)
			{
				const float A = 2.f * PI * i / 36.f;
				const FVector2D Dir(FMath::Sin(A), -FMath::Cos(A));
				FSlateDrawElement::MakeLines(Out, Layer + 3, Geo.ToPaintGeometry(), TArray<FVector2D>{ C + Dir * (85.5f * K), C + Dir * (92.5f * K) }, ESlateDrawEffect::None, Hex(TEXT("4A5057")), true, 4.f * K);
			}
			DrawCircle(Out, Layer + 3, Geo, C, 83.f * K, Hex(TEXT("1A1C1F")), Hex(TEXT("08090A")), 2.f * K);
			Ring(78.f, 1.2f, Hex(TEXT("3B4047")), Layer + 4);

			// A diamond per level it can reach, round the ring from the top, lit up to its level.
			const int32 Max = FMath::Max(1, MaxLevel.Get());
			for (int32 i = 0; i < Max; ++i)
			{
				const float A = 2.f * PI * i / Max;
				const FVector2D P = C + FVector2D(FMath::Sin(A), -FMath::Cos(A)) * (69.f * K);
				const bool bLit = !bCold && i < Level.Get();
				if (bLit) DrawCircle(Out, Layer + 4, Geo, P, 9.f * K, WRUI::Alpha(WRUI::Ember, 0.3f));
				const float D = 9.5f * K;
				const FGeometry Gem = Geo.MakeChild(FVector2D(D, D), FSlateLayoutTransform(P - FVector2D(D * 0.5f, D * 0.5f)), FSlateRenderTransform(FQuat2D(PI * 0.25f)), FVector2D(0.5f, 0.5f));
				WRUI::DrawBox(Out, Layer + 5, Gem.ToPaintGeometry(), WRUI::Box(bLit ? WRUI::Ember : Hex(TEXT("2B2F34")), 1.f, bLit ? Hex(TEXT("2A1006")) : Hex(TEXT("4A5057")), 1.f));
			}

			// The fire: an ember disc, brightest a little below the middle (the mockup's radial gradient, in rings).
			DrawCircle(Out, Layer + 5, Geo, C, 59.f * K, bCold ? Hex(TEXT("121314")) : Hex(TEXT("1A0A05")), Hex(TEXT("08090A")), 2.f * K);
			struct FStop { float T; FLinearColor Col; };
			const FStop Stops[] = { { 0.f, Hex(TEXT("FFE2A0")) }, { 0.28f, Hex(TEXT("FFA04A")) }, { 0.58f, Hex(TEXT("B8401A")) }, { 0.85f, Hex(TEXT("3A1308")) }, { 1.f, Hex(TEXT("1A0A05")) } };
			const auto GradientAt = [&](float T)
			{
				for (int32 s = 1; s < UE_ARRAY_COUNT(Stops); ++s)
				{
					if (T <= Stops[s].T) return FMath::Lerp(Stops[s - 1].Col, Stops[s].Col, (T - Stops[s - 1].T) / (Stops[s].T - Stops[s - 1].T));
				}
				return Stops[UE_ARRAY_COUNT(Stops) - 1].Col;
			};
			const FVector2D Hot = C + FVector2D(0.f, 5.8f * K);
			const int32 Steps = 26;
			for (int32 s = 0; s < Steps; ++s)
			{
				const float T = 0.8f * (1.f - static_cast<float>(s) / Steps);
				FLinearColor Col = GradientAt(T);
				if (bCold) Col = FLinearColor(Col.GetLuminance() * 0.25f, Col.GetLuminance() * 0.25f, Col.GetLuminance() * 0.27f);
				DrawCircle(Out, Layer + 6, Geo, Hot, 63.8f * T * K, Col);
			}
			// Coals along the bottom.
			TArray<FVector2D> Grate = { C + FVector2D(-36.f, 36.f) * K };
			Cubic(Grate, C + FVector2D(-36.f, 36.f) * K, C + FVector2D(-28.f, 42.f) * K, C + FVector2D(28.f, 42.f) * K, C + FVector2D(36.f, 36.f) * K, 10);
			FSlateDrawElement::MakeLines(Out, Layer + 7, Geo.ToPaintGeometry(), Grate, ESlateDrawEffect::None, Hex(TEXT("2A1006")), true, 3.f * K);
			const auto Coal = [&](float X, float Y, float RX, float RY, const TCHAR* Col)
			{
				WRUI::DrawBox(Out, Layer + 7, Geo.ToPaintGeometry(FVector2D(RX * 2.f, RY * 2.f) * K, FSlateLayoutTransform(C + FVector2D(X - 120.f - RX, Y - 120.f - RY) * K)), Circle(bCold ? Hex(TEXT("2A2B2D")) : Hex(Col)));
			};
			Coal(104.f, 150.f, 9.f, 4.f, TEXT("7A2A0E"));
			Coal(124.f, 153.f, 10.f, 4.f, TEXT("9A3412"));
			Coal(141.f, 149.f, 8.f, 3.5f, TEXT("7A2A0E"));
			if (!bCold)
			{
				FVector2D Mid;
				const TArray<FVector2D> Outer = Flame(C + FVector2D(84.f - 120.f, 76.f - 120.f) * K, 3.f * K, Mid);
				FillShape(Out, Layer + 8, Geo, Outer, Mid, WRUI::Alpha(Hex(TEXT("FF9A3C")), 0.92f));
				const TArray<FVector2D> Inner = Flame(C + FVector2D(100.8f - 120.f, 101.f - 120.f) * K, 1.6f * K, Mid);
				FillShape(Out, Layer + 9, Geo, Inner, Mid, Hex(TEXT("FFE6A8")));
			}
			return Layer + 9;
		}
	private:
		float Size = 254.f;
		TAttribute<int32> Level;
		TAttribute<int32> MaxLevel;
		TAttribute<bool> Broken;
	};

	class SHearthScreen : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SHearthScreen) {}
			SLATE_ARGUMENT(TWeakObjectPtr<AWRPlayerController>, PC)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			PC = InArgs._PC;
			Cores = MajorCores();
			MaxLevel = FWRCraftingRules::TechLevelForCores(Cores.Num());
			Hearth = FindHearth();

			ChildSlot
			[
				Window(PC, SNew(SBadge), LOCTEXT("HearthTitle", "Hearth"),
					TAttribute<FText>::CreateLambda([this]() { return Subtitle(); }),
					TAttribute<FText>::CreateLambda([this]() { return HeaderChip(); }),
					TAttribute<FLinearColor>::CreateLambda([this]() { const AWRHearth* H = Hearth.Get(); return H && H->IsBroken() ? WRUI::Bad : WRUI::Ember; }),
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SNew(SVerticalBox).Visibility_Lambda([this]() { return Hearth.IsValid() ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
						+ SVerticalBox::Slot().FillHeight(1.f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(520.f * U)[LeftColumn()]]
							+ SHorizontalBox::Slot().FillWidth(1.f).Padding(26.f * U, 0.f, 0.f, 0.f)[RightColumn()]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 16.f * U, 0.f, 0.f)[Footer()]
					]
					+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
					[
						SNew(SBox).WidthOverride(640.f * U).Visibility_Lambda([this]() { return Hearth.IsValid() ? EVisibility::Collapsed : EVisibility::SelfHitTestInvisible; })
						[
							WRUI::Text(LOCTEXT("NoHearth", "There's no Hearth here. Build one inside your own land claim (Build menu, Claim), then feed it the cores of the major bosses to raise the tier of every station in the claim."),
								WRUI::Body(17, 500), WRUI::Ink2, false, true)
						]
					])
			];
			RebuildStations();
		}

		virtual void Tick(const FGeometry& Geo, const double Time, const float Delta) override
		{
			SCompoundWidget::Tick(Geo, Time, Delta);
			Hearth = FindHearth();
			RebuildStations();
		}

	private:
		TWeakObjectPtr<AWRPlayerController> PC;
		TWeakObjectPtr<AWRHearth> Hearth;
		TArray<FCoreInfo> Cores;
		int32 MaxLevel = 3;
		TSharedPtr<SWrapBox> StationChips;
		FString StationsShown = TEXT("-");

		// ---------------------------------------------------------------- state

		/** The Hearth you stand at; else the one whose claim you're in; else your own (the screen can open from afar). */
		AWRHearth* FindHearth() const
		{
			AWRPlayerController* P = PC.Get();
			if (!P) return nullptr;
			if (AWRHearth* Near = P->FindNearbyHearth()) return Near;
			const APawn* Me = P->GetPawn();
			if (Me)
			{
				if (AWRHearth* Covering = AWRHearth::FindCovering(P->GetWorld(), Me->GetActorLocation())) return Covering;
			}
			const FString Who = P->PlayerState ? P->PlayerState->GetPlayerName() : FString();
			for (TActorIterator<AWRHearth> It(P->GetWorld()); It; ++It)
			{
				if (!Who.IsEmpty() && OwnerOf(*It) == Who) return *It;
			}
			return nullptr;
		}

		/** Hearths ride along on their build piece (it's their owner actor), which knows who built it. */
		static FString OwnerOf(const AWRHearth* H)
		{
			const AWRBuildPiece* Piece = H ? Cast<AWRBuildPiece>(H->GetOwner()) : nullptr;
			return Piece ? Piece->OwnerName : FString();
		}

		int32 Level() const { const AWRHearth* H = Hearth.Get(); return H ? H->GetTechLevel() : 1; }
		bool IsBroken() const { const AWRHearth* H = Hearth.Get(); return H && H->IsBroken(); }
		bool HasCore(FName Core) const { const AWRHearth* H = Hearth.Get(); return H && H->GetCores().Contains(Core); }
		int32 Carried(FName Item) const
		{
			const AWRPlayerState* PS = PC.IsValid() ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
			return PS && PS->GetInventory() ? PS->GetInventory()->CountOf(Item) : 0;
		}
		/** At the Hearth (the server feeds the nearest one in reach) with a core it doesn't have yet. */
		bool CanFeed() const
		{
			if (!PC.IsValid() || !Hearth.IsValid() || PC->FindNearbyHearth() != Hearth.Get()) return false;
			for (const FCoreInfo& C : Cores)
			{
				if (!HasCore(C.Core) && Carried(C.Core) > 0) return true;
			}
			return false;
		}

		/** "Copper" for a level: the metal of the ingot its forge recipes unlock. */
		static FString MetalOf(int32 Lvl)
		{
			for (const FWRRecipe* R : FWRRecipeRegistry::Get().ForStation(TEXT("forge")))
			{
				if (R->Level == Lvl && R->Output.ToString().EndsWith(TEXT("_ingot")))
				{
					const FWRItemDef* Def = FWRItemRegistry::Get().Find(R->Output);
					FString Name = Def ? Def->Name : R->Output.ToString();
					Name.Split(TEXT(" "), &Name, nullptr);
					return Name;
				}
			}
			return FString::Printf(TEXT("Tier %d"), Lvl);
		}

		FText Subtitle() const
		{
			const AWRHearth* H = Hearth.Get();
			if (!H) return LOCTEXT("HearthNone", "No Hearth nearby");
			const FString Owner = OwnerOf(H);
			// The owner's clan, from their player state while they're on (clans live on the server).
			FString Clan;
			if (const AGameStateBase* GS = H->GetWorld() ? H->GetWorld()->GetGameState() : nullptr)
			{
				for (const APlayerState* P : GS->PlayerArray)
				{
					const AWRPlayerState* W = Cast<AWRPlayerState>(P);
					if (W && W->GetPlayerName() == Owner) Clan = W->ClanName;
				}
			}
			FString Line = Owner.IsEmpty() ? FString(TEXT("The heart of this base")) : FString::Printf(TEXT("The heart of %s's base"), *Owner);
			if (!Clan.IsEmpty()) Line += FString::Printf(TEXT(" · Clan %s"), *Clan);
			return FText::FromString(Line);
		}

		FText HeaderChip() const
		{
			if (!Hearth.IsValid()) return LOCTEXT("ChipNone", "No Hearth");
			if (IsBroken()) return LOCTEXT("ChipBroken", "Broken · stations stopped");
			return FText::FromString(FString::Printf(TEXT("Tech level %d · %s"), Level(), *MetalOf(Level())));
		}

		// ---------------------------------------------------------------- left: the Hearth

		TSharedRef<SWidget> LeftColumn()
		{
			TSharedRef<SHorizontalBox> Diamonds = SNew(SHorizontalBox);
			for (int32 i = 0; i < MaxLevel; ++i)
			{
				Diamonds->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(i > 0 ? 9.f * U : 0.f, 0.f, 0.f, 0.f)
				[
					SNew(SBox).WidthOverride(14.f * U).HeightOverride(14.f * U)
					[
						SNew(SImage)
						.Image_Lambda([this, i]() { const bool bLit = !IsBroken() && i < Level(); return WRUI::Box(bLit ? WRUI::Ember : Hex(TEXT("2B2F34")), 1.f, bLit ? Hex(TEXT("2A1006")) : Hex(TEXT("4A5057")), 1.f); })
						.RenderTransform(FSlateRenderTransform(FQuat2D(PI * 0.25f)))
						.RenderTransformPivot(FVector2D(0.5f, 0.5f))
					]
				];
			}
			TSharedRef<SHorizontalBox> CoreCards = SNew(SHorizontalBox);
			for (int32 i = 0; i < Cores.Num(); ++i)
			{
				CoreCards->AddSlot().FillWidth(1.f).Padding(i > 0 ? 12.f * U : 0.f, 0.f, 0.f, 0.f)[CoreCard(Cores[i])];
			}

			return SNew(SWRPanel).Kind(EWRPanel::Inset).Padding(FMargin(22.f * U, 18.f * U))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(SEmblem).Level_Lambda([this]() { return Level(); }).MaxLevel(MaxLevel).Broken_Lambda([this]() { return IsBroken(); })
				]
				+ SVerticalBox::Slot().FillHeight(1.f)[SNew(SSpacer)]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 14.f * U, 0.f)
					[
						WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::Format(LOCTEXT("TechOf", "Tech level {0} of {1}"), Level(), MaxLevel); }), WRUI::Title(19), WRUI::Ink, true)
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Diamonds]
				]
				+ SVerticalBox::Slot().FillHeight(1.f)[SNew(SSpacer)]
				+ SVerticalBox::Slot().AutoHeight()[CoreCards]
				+ SVerticalBox::Slot().FillHeight(1.f)[SNew(SSpacer)]
				+ SVerticalBox::Slot().AutoHeight()[HealthBlock()]
				+ SVerticalBox::Slot().FillHeight(1.f)[SNew(SSpacer)]
				+ SVerticalBox::Slot().AutoHeight()[FeedBlock()]
			];
		}

		TSharedRef<SWidget> CoreCard(const FCoreInfo& Info)
		{
			const FName Core = Info.Core;
			const FLinearColor Tint = Info.Element == TEXT("frost") ? WRUI::Frost : WRUI::Ember;
			const FWRItemDef* Def = FWRItemRegistry::Get().Find(Core);
			const FText Name = FText::FromString(Def ? Def->Name : Core.ToString());
			const FText BossChip = FText::FromString(Info.Element == TEXT("frost") ? TEXT("Frost boss") : TEXT("Fire boss"));
			const FCoreInfo Copy = Info;
			return SNew(SBorder)
				.BorderImage_Lambda([this, Core, Tint]() { return CardBrush(WRUI::Alpha(Tint, HasCore(Core) ? 0.45f : 0.3f)); })
				.Padding(FMargin(12.f * U, 10.f * U))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f * U, 0.f)
					[
						SNew(SBox).WidthOverride(64.f * U).HeightOverride(64.f * U)
						[
							SNew(SOverlay)
							+ SOverlay::Slot()
							[
								SNew(SImage).Image_Lambda([this, Core, Tint]()
								{
									return HasCore(Core) ? Circle(Tint * 0.22f + FLinearColor(0.f, 0.f, 0.f, 0.78f), WRUI::Brass, 2.f) : Circle(FLinearColor(0.02f, 0.025f, 0.03f), WRUI::Alpha(Tint, 0.75f), 2.f);
								})
							]
							+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
							[
								SNew(SBox).WidthOverride(44.f * U).HeightOverride(44.f * U)
								[
									SNew(SImage).Image(WRUI::Icon(Core)).ColorAndOpacity_Lambda([this, Core]() { return HasCore(Core) ? FLinearColor::White : FLinearColor(1.f, 1.f, 1.f, 0.35f); })
								]
							]
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							WRUI::Text(Name, WRUI::Body(15, 800), TAttribute<FSlateColor>::CreateLambda([this, Core, Tint]() { return FSlateColor(HasCore(Core) ? WRUI::Ink : Tint); }))
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f * U, 0.f, 0.f)
						[
							WRUI::Text(TAttribute<FText>::CreateLambda([this, Copy]() { return CoreLine(Copy); }), WRUI::Body(13, 500),
								TAttribute<FSlateColor>::CreateLambda([this, Core]() { return FSlateColor(!HasCore(Core) && Carried(Core) > 0 ? WRUI::Good : WRUI::Ink2); }), false, true)
						]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left).Padding(0.f, 4.f * U, 0.f, 0.f)
						[
							SNew(SBox).Visibility_Lambda([this, Core]() { return HasCore(Core) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
							[
								SNew(SWRChip).FontSize(11).Text(BossChip).Color(Tint)
							]
						]
					]
				];
		}

		FText CoreLine(const FCoreInfo& Info) const
		{
			if (HasCore(Info.Core)) return LOCTEXT("CoreFed", "Fed to this Hearth");
			if (Carried(Info.Core) > 0) return LOCTEXT("CoreCarried", "In your bag: feed it");
			// Where the boss is, from the Hearth (the island's places).
			const AWRHearth* H = Hearth.Get();
			for (const FWRPoi& Poi : FWRPoi::All)
			{
				if (H && Poi.Kind == TEXT("boss") && Poi.Name == Info.BossName)
				{
					const FVector D = Poi.Location - H->GetActorLocation();
					return FText::FromString(FString::Printf(TEXT("Defeat %s, %s %s of here"), *Info.BossName, *Distance(D.Size2D()), CompassName(D)));
				}
			}
			return FText::FromString(FString::Printf(TEXT("Defeat %s for its core"), *Info.BossName));
		}

		TSharedRef<SWidget> HealthBlock()
		{
			return SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Bottom)
					[
						WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return IsBroken() ? LOCTEXT("HealthBroken", "Hearth health · broken") : LOCTEXT("Health", "Hearth health"); }), WRUI::Caps(12),
							TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(IsBroken() ? WRUI::Bad : WRUI::Ink2); }))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
					[
						WRUI::Text(TAttribute<FText>::CreateLambda([this]() { const AWRHearth* H = Hearth.Get(); return H ? Num(H->GetHealth()) : FText::GetEmpty(); }), WRUI::Body(15, 800), WRUI::Ink)
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
					[
						WRUI::Text(TAttribute<FText>::CreateLambda([this]() { const AWRHearth* H = Hearth.Get(); return H ? FText::Format(LOCTEXT("HealthOf", " / {0}"), Num(H->MaxHealth)) : FText::GetEmpty(); }), WRUI::Body(15, 500), WRUI::Ink3)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 0.f)
				[
					SNew(SWRBar).Height(16.f * U).Color(WRUI::Hp).Fraction_Lambda([this]() { const AWRHearth* H = Hearth.Get(); return H && H->MaxHealth > 0.f ? H->GetHealth() / H->MaxHealth : 0.f; })
				];
		}

		TSharedRef<SWidget> FeedBlock()
		{
			return SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SWRButton).Primary(true).FontSize(15).MinHeight(44.f * U).Text(LOCTEXT("FeedCore", "Feed a core"))
					.IsEnabled_Lambda([this]() { return CanFeed(); })
					.OnClicked_Lambda([this]() { if (PC.IsValid()) PC->SendCommand(TEXT("hearthfeed")); return FReply::Handled(); })
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 5.f * U, 0.f, 0.f)
				[
					WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FeedNote(); }), WRUI::Body(12, 500), WRUI::Ink3)
				];
		}

		FText FeedNote() const
		{
			if (!PC.IsValid() || !Hearth.IsValid() || PC->FindNearbyHearth() != Hearth.Get()) return LOCTEXT("FeedGoThere", "Stand at the Hearth to feed it");
			TArray<FString> Carry;
			bool bAnyNew = false;
			for (const FCoreInfo& C : Cores)
			{
				if (Carried(C.Core) > 0)
				{
					const FWRItemDef* Def = FWRItemRegistry::Get().Find(C.Core);
					Carry.Add(Def ? Def->Name : C.Core.ToString());
					bAnyNew |= !HasCore(C.Core);
				}
			}
			if (Carry.Num() == 0) return LOCTEXT("FeedNone", "You carry no boss cores");
			if (!bAnyNew) return LOCTEXT("FeedHas", "It already has the cores you carry: each boss counts once");
			return FText::FromString(FString::Printf(TEXT("You carry %s"), *FString::Join(Carry, TEXT(" and "))));
		}

		// ---------------------------------------------------------------- right: what each level unlocks

		TSharedRef<SWidget> RightColumn()
		{
			TSharedRef<SVerticalBox> Col = SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Bottom)[WRUI::Text(LOCTEXT("Unlocks", "What each level unlocks"), WRUI::Title(17), WRUI::Ink, true)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)[WRUI::Text(LOCTEXT("UnlocksSub", "Recipes at your stations"), WRUI::Body(13, 500), WRUI::Ink3)]
				];
			for (int32 L = 1; L <= MaxLevel; ++L)
			{
				Col->AddSlot().AutoHeight().Padding(0.f, 12.f * U, 0.f, 0.f)[LevelCard(L)];
			}
			Col->AddSlot().FillHeight(1.f)[SNew(SSpacer)];
			Col->AddSlot().AutoHeight().Padding(0.f, 12.f * U, 0.f, 0.f)
			[
				SNew(SBorder).BorderImage(CardBrush()).Padding(FMargin(16.f * U, 10.f * U, 16.f * U, 12.f * U))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return StationsLabel(); }), WRUI::Caps(12), WRUI::Ink2)]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f * U, 0.f, 0.f)[SAssignNew(StationChips, SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8.f * U, 6.f * U))]
				]
			];
			return Col;
		}

		TSharedRef<SWidget> LevelCard(int32 L)
		{
			const FString Metal = MetalOf(L);
			const FLinearColor MetalCol = MetalColor(Metal);
			TArray<const FWRRecipe*> Recipes;
			for (const FName Station : { FName(TEXT("forge")), FName(TEXT("armorer")), FName(TEXT("alchemy_table")), FName(TEXT("workbench")) })
			{
				for (const FWRRecipe* R : FWRRecipeRegistry::Get().ForStation(Station))
				{
					if (R->Level == L) Recipes.Add(R);
				}
			}
			// Six tiles fit a row; past that, five and a "+N more" tile that lists the rest.
			const int32 Shown = Recipes.Num() > 6 ? 5 : Recipes.Num();
			TSharedRef<SHorizontalBox> Tiles = SNew(SHorizontalBox);
			for (int32 i = 0; i < Shown; ++i)
			{
				const FWRItemDef* Out = FWRItemRegistry::Get().Find(Recipes[i]->Output);
				const FName OutId = Recipes[i]->Output;
				const FString Name = Out ? Out->Name : OutId.ToString();
				Tiles->AddSlot().AutoWidth().Padding(0.f, 0.f, 8.f * U, 0.f)
				[
					SNew(SBox).WidthOverride(88.f * U)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
						[
							SNew(SWRSlot).Size(46.f * U).View_Lambda([this, OutId, L]() { FWRSlotView V; V.ItemId = OutId; V.bLocked = L > Level() || (IsBroken() && L > 1); return V; })
						]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 5.f * U, 0.f, 0.f)
						[
							SNew(STextBlock).Text(FText::FromString(Name)).Font(WRUI::Body(12, 500)).Justification(ETextJustify::Center).AutoWrapText(true)
							.ColorAndOpacity_Lambda([this, L]() { return FSlateColor(L > Level() ? WRUI::Ink3 : WRUI::Ink2); })
						]
					]
				];
			}
			if (Recipes.Num() > Shown)
			{
				TArray<FString> Rest;
				for (int32 i = Shown; i < Recipes.Num(); ++i)
				{
					const FWRItemDef* Out = FWRItemRegistry::Get().Find(Recipes[i]->Output);
					Rest.Add(Out ? Out->Name : Recipes[i]->Output.ToString());
				}
				Tiles->AddSlot().AutoWidth()
				[
					SNew(SBox).WidthOverride(88.f * U).ToolTipText(FText::FromString(FString::Join(Rest, TEXT(", "))))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
						[
							SNew(SBox).WidthOverride(46.f * U).HeightOverride(46.f * U)
							[
								SNew(SBorder).BorderImage(WRUI::Box(FLinearColor(0.13f, 0.14f, 0.16f, 0.6f), 3.f, WRUI::Rim, 1.f)).HAlign(HAlign_Center).VAlign(VAlign_Center)
								[
									WRUI::Text(FText::FromString(FString::Printf(TEXT("+%d"), Rest.Num())), WRUI::Body(16, 800), WRUI::Ink2)
								]
							]
						]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 5.f * U, 0.f, 0.f)
						[
							WRUI::Text(LOCTEXT("MoreRecipes", "more recipes"), WRUI::Body(12, 500), WRUI::Ink3)
						]
					]
				];
			}

			return SNew(SBorder)
				.BorderImage_Lambda([this, L]()
				{
					// This level glows brass; ones it has passed are plain; ones ahead are dimmer.
					return L == Level() ? WRUI::Box(FLinearColor(0.021f, 0.009f, 0.0037f, 0.42f), 5.f, WRUI::Brass, 1.f)
						: L < Level() ? CardBrush() : CardBrush(WRUI::Alpha(WRUI::Ink3, 0.4f), FLinearColor(0.f, 0.f, 0.f, 0.38f));
				})
				.Padding(FMargin(16.f * U, 12.f * U, 16.f * U, 14.f * U))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f * U, 0.f)
						[
							SNew(SBox).WidthOverride(40.f * U).HeightOverride(40.f * U)
							[
								SNew(SOverlay)
								+ SOverlay::Slot()[SNew(SImage).Image_Lambda([this, L, MetalCol]() { return Circle(L > Level() ? MetalCol.Desaturate(0.5f) * 0.7f : MetalCol, Hex(TEXT("2A1A0E")), 2.f); })]
								+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[WRUI::Text(FText::FromString(Roman(L)), WRUI::Title(15), Hex(TEXT("1E1008")))]
							]
						]
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth()
								[
									WRUI::Text(FText::Format(LOCTEXT("LevelN", "Level {0} · "), L), WRUI::Body(16, 800), TAttribute<FSlateColor>::CreateLambda([this, L]() { return FSlateColor(L > Level() ? WRUI::Ink2 : WRUI::Ink); }))
								]
								+ SHorizontalBox::Slot().AutoWidth()[WRUI::Text(FText::FromString(Metal), WRUI::Body(16, 800), (MetalCol * 1.15f).GetClamped())]
							]
							+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda([this, L]() { return LevelLine(L); }), WRUI::Body(13, 500), WRUI::Ink3)]
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(SWRChip).FontSize(12)
							.Text_Lambda([this, L]() { return L < Level() ? LOCTEXT("Unlocked", "Unlocked") : L == Level() ? LOCTEXT("Current", "Current level") : LOCTEXT("Locked", "Locked"); })
							.Color_Lambda([this, L]() { return L < Level() ? WRUI::Good : L == Level() ? WRUI::Brass : WRUI::Ink3; })
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f * U, 0.f, 0.f)[Tiles]
				];
		}

		/** How a level is reached: the cores of different major bosses (TechLevelForCores). */
		FText LevelLine(int32 L) const
		{
			if (L <= 1) return LOCTEXT("Level1Line", "Ready as soon as the Hearth is built");
			int32 Need = 0;
			while (Need < 20 && FWRCraftingRules::TechLevelForCores(Need) < L) ++Need;
			TArray<FString> Fed, Missing;
			for (const FCoreInfo& C : Cores) (HasCore(C.Core) ? Fed : Missing).Add(C.BossName);
			if (L <= Level())
			{
				return FText::FromString(Fed.Num() > 0 && Need == 1 ? FString::Printf(TEXT("Unlocked by feeding it %s's core"), *Fed[0])
					: FString::Printf(TEXT("Unlocked by cores from %s"), *FString::Join(Fed, TEXT(" and "))));
			}
			const int32 More = Need - Fed.Num();
			if (More == 1 && Missing.Num() > 0)
			{
				return FText::FromString(Fed.Num() > 0 ? FString::Printf(TEXT("Feed a core from the other major boss: %s"), *FString::Join(Missing, TEXT(" or ")))
					: FString::Printf(TEXT("Feed it the core of a major boss: %s"), *FString::Join(Missing, TEXT(" or "))));
			}
			return FText::FromString(FString::Printf(TEXT("Feed it cores from %d different major bosses"), Need));
		}

		// ---------------------------------------------------------------- stations, footer

		/** The stations whose tier this Hearth sets: those inside its claim. */
		TArray<const AWRStation*> Stations() const
		{
			TArray<const AWRStation*> Out;
			const AWRHearth* H = Hearth.Get();
			if (!H) return Out;
			for (TActorIterator<AWRStation> It(H->GetWorld()); It; ++It)
			{
				if (AWRHearth::FindCovering(H->GetWorld(), It->GetActorLocation()) == H) Out.Add(*It);
			}
			// A steady order (by kind), not the world's.
			Out.Sort([](const AWRStation& A, const AWRStation& B) { return A.StationType.LexicalLess(B.StationType); });
			return Out;
		}

		FText StationsLabel() const
		{
			const TArray<const AWRStation*> S = Stations();
			if (S.Num() == 0) return LOCTEXT("StationsNone", "Stations in your claim · none yet");
			const int32 Tier = S[0]->GetTier();
			return FText::FromString(FString::Printf(TEXT("Stations in your claim · all at tier %d"), Tier));
		}

		void RebuildStations()
		{
			if (!StationChips.IsValid()) return;
			FString Key;
			const TArray<const AWRStation*> S = Stations();
			for (const AWRStation* St : S) Key += FString::Printf(TEXT("%s%d,"), *St->StationType.ToString(), St->GetTier());
			if (Key == StationsShown) return;
			StationsShown = Key;
			StationChips->ClearChildren();
			if (S.Num() == 0)
			{
				StationChips->AddSlot()[WRUI::Text(LOCTEXT("StationsHint", "Build a workbench, forge, armorer or alchemy table inside the claim: they work at the Hearth's level."), WRUI::Body(13, 500), WRUI::Ink3, false, true)];
				return;
			}
			for (const AWRStation* St : S)
			{
				FString Name = St->StationType.ToString().Replace(TEXT("_"), TEXT(" "));
				Name[0] = FChar::ToUpper(Name[0]);
				StationChips->AddSlot()
				[
					SNew(SBorder).BorderImage(WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.5f), 10.f, WRUI::Alpha(WRUI::Ink2, 0.45f), 1.f)).Padding(FMargin(10.f * U, 2.f * U, 10.f * U, 3.f * U))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(FText::FromString(Name), WRUI::Body(13, 700), WRUI::Ink)]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(7.f * U, 0.f, 0.f, 0.f)[WRUI::Text(FText::FromString(Roman(St->GetTier())), WRUI::Title(12), WRUI::Brass)]
					]
				];
			}
		}

		TSharedRef<SWidget> Footer()
		{
			return SNew(SBox).HeightOverride(46.f * U)
			[
				SNew(SWRPanel).Kind(EWRPanel::Inset).Padding(FMargin(18.f * U, 0.f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f * U, 0.f)
					[
						SNew(SBox).WidthOverride(20.f * U).HeightOverride(20.f * U)
						[
							SNew(SOverlay)
							+ SOverlay::Slot()[SNew(SImage).Image(Circle(FLinearColor::Transparent, WRUI::Brass, 1.8f))]
							+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[WRUI::Text(FText::FromString(TEXT("i")), WRUI::Body(13, 800), WRUI::Brass)]
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						WRUI::Text(TAttribute<FText>::CreateLambda([this]()
						{
							return FText::Format(LOCTEXT("FooterText", "Every crafting station in your claim works at the Hearth's tech level, and gear upgrades stop at +{0}. "),
								FWRCraftingRules::UpgradeCap(IsBroken() ? 1 : Level()));
						}), WRUI::Body(15, 500), WRUI::Ink)
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(LOCTEXT("FooterWarn", "A broken Hearth stops them all."), WRUI::Body(15, 800), WRUI::Warn)]
				]
			];
		}
	};
}

WR_REGISTER_SCREEN("hearth", [](AWRPlayerController* PC) -> TSharedRef<SWidget> { return SNew(WRUIHearthScreen::SHearthScreen).PC(PC); });

#undef LOCTEXT_NAMESPACE
