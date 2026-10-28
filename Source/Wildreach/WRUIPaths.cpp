// The journal's Paths tab (mockup "Paths"): the path tree of the viewed path (main path or subclass) with learned /
// learnable / locked nodes, the points to spend, a details panel for the hovered or selected node (learning sends
// "learn <path> <node>"), and the two ability wheels: drag a learned ability from the tree onto a wheel slot, or select
// it and click a slot. Wheel slots are client settings (FWRPlayerUI); the tree is the replicated UWRPathComponent.
// Sizes are mockup px x U (1080p units). Node glyphs: Data/UI/Glyphs (Tools/ui_glyphs.mjs).

#include "WRPlayerUI.h"
#include "WRUIStyle.h"
#include "WRUIWidgets.h"
#include "WRAbilities.h"
#include "WRKeyBindings.h"
#include "WRPathComponent.h"
#include "WRPathTypes.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRTuning.h"
#include "Dom/JsonObject.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCanvas.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "WRUI"

// Named (not anonymous): unity builds put several UI files in one translation unit.
namespace WRPathsTab
{
	using WRUI::U;

	FLinearColor Hex(const TCHAR* H) { return FLinearColor(FColor::FromHex(H)); }
	const FSlateBrush* Glyph(const FString& Name) { return WRUI::Image(FName(*(TEXT("Glyphs/") + Name))); }

	// Tree layout from the mockup (px on the 1002 x 640 board): trunk circles' centre row and columns, the branch
	// spines, the bar that feeds them, the branch header plates, and the node ladder down each spine.
	constexpr float TY = 44.f, BAR = 136.f, PLATE_T = 156.f, PLATE_B = 188.f, NY0 = 200.f, STEP = 65.f, OFF = 78.f, R = 28.f;
	const float TX[4] = { 230.f, 410.f, 590.f, 770.f };
	const float SX[3] = { 167.f, 500.f, 833.f };

	enum class ENodeState : uint8 { Learned, Available, Locked };

	/** How a node looks in a state (mockup LOOK table), per path colour. */
	struct FNodeLook
	{
		FLinearColor Ring, Fill, Glow, P, Ac, Name, Meta, PipOff;
		float GlowA = 0.f;
	};

	struct FPathColours
	{
		FLinearColor Main, LitFill, LitP, LitAc, Wire, WireHi;
	};

	FPathColours ColoursFor(FName Path)
	{
		if (Path == TEXT("frost"))
		{
			return { WRUI::Frost, Hex(TEXT("0F2E46")), Hex(TEXT("7FC0E8")), Hex(TEXT("D6ECF8")), Hex(TEXT("6FB3E0")), Hex(TEXT("E2F4FF")) };
		}
		return { WRUI::Ember, Hex(TEXT("45190A")), Hex(TEXT("EC7A34")), Hex(TEXT("FFCF86")), Hex(TEXT("E5843A")), Hex(TEXT("FFD7A0")) };
	}

	FString Humanize(const FString& Id)
	{
		// "burnDamage" / "ice_blade" -> "Burn damage" / "Ice blade"
		FString Out;
		for (int32 i = 0; i < Id.Len(); ++i)
		{
			const TCHAR C = Id[i];
			if (C == TEXT('_')) { Out += TEXT(' '); continue; }
			if (i > 0 && FChar::IsUpper(C)) { Out += TEXT(' '); Out += FChar::ToLower(C); continue; }
			Out += i == 0 ? FChar::ToUpper(C) : C;
		}
		return Out;
	}

	/** "fire_wall" -> "fireWall": the tuning key of a tree spell (spells.<key>.*). */
	FString CamelKey(const FString& Id)
	{
		FString Out;
		bool bUp = false;
		for (const TCHAR C : Id)
		{
			if (C == TEXT('_')) { bUp = true; continue; }
			Out += bUp ? FChar::ToUpper(C) : C;
			bUp = false;
		}
		return Out;
	}

	/** tuning.json as authored (to know which keys exist without the tuning's missing-key warnings). */
	const TSharedPtr<FJsonObject>& TuningFile()
	{
		static TSharedPtr<FJsonObject> File = FWRTuning::LoadDataFile(TEXT("tuning.json"));
		return File;
	}

	FString Num(float V)
	{
		return FMath::IsNearlyEqual(V, FMath::RoundToFloat(V)) ? FString::Printf(TEXT("%.0f"), V) : FString::Printf(TEXT("%.1f"), V);
	}

	FString Signed(float V, bool bPercent)
	{
		return bPercent ? FString::Printf(TEXT("%s%.0f%%"), V >= 0.f ? TEXT("+") : TEXT(""), V * 100.f) : FString::Printf(TEXT("%s%s"), V >= 0.f ? TEXT("+") : TEXT(""), *Num(V));
	}

	FString Capitalised(const FString& S) { return S.IsEmpty() ? S : FString::Chr(FChar::ToUpper(S[0])) + S.Mid(1); }

	/** Absolute placement on the board, in mockup px. */
	void Place(const TSharedRef<SCanvas>& Canvas, float X, float Y, float W, float H, const TSharedRef<SWidget>& Content)
	{
		Canvas->AddSlot().Position(FVector2D(X, Y) * U).Size(FVector2D(W, H) * U)[Content];
	}

/** What the tab shows: the viewed path, read live from the replicated path component. */
struct FWRPathsModel
{
	TWeakObjectPtr<AWRPlayerController> PC;
	FName Path;

	const UWRPathComponent* Comp() const
	{
		const AWRPlayerState* PS = PC.IsValid() ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
		return PS ? PS->GetPaths() : nullptr;
	}
	const FWRPathDef* Def() const { return FWRPathRegistry::Get().Find(Path); }
	const FWRPathNodeDef* Node(FName Id) const { const FWRPathDef* D = Def(); return D ? D->FindNode(Id) : nullptr; }
	FWRPathProgress Progress() const
	{
		const UWRPathComponent* C = Comp();
		const FWRPathProgress* P = C ? C->GetProgress(Path) : nullptr;
		return P ? *P : FWRPathProgress();
	}
	int32 Rank(FName Id) const { const UWRPathComponent* C = Comp(); const FWRPathProgress* P = C ? C->GetProgress(Path) : nullptr; return P ? P->GetRank(Id) : 0; }
	int32 Earned() const { const UWRPathComponent* C = Comp(); return C ? C->GetEarned(Path) : 0; }
	int32 Points() const { const UWRPathComponent* C = Comp(); return C ? C->GetEarned(Path) - C->GetSpent(Path) : 0; }
	bool IsActive() const { const UWRPathComponent* C = Comp(); return C && C->IsActivePath(Path); }
	bool IsMain() const { const UWRPathComponent* C = Comp(); return C && C->GetMainPath() == Path; }
	int32 BranchSpent(FName Branch) const { const FWRPathDef* D = Def(); return D ? FWRPathRules::SpentInBranch(*D, Progress(), Branch) : 0; }
	bool TrunkDone() const { const FWRPathDef* D = Def(); return D && FWRPathRules::TrunkComplete(*D, Progress()); }

	ENodeState State(const FWRPathNodeDef& N) const
	{
		if (Rank(N.Id) > 0) return ENodeState::Learned;
		if (!IsActive()) return ENodeState::Locked;
		if (!N.Branch.IsNone() && (!TrunkDone() || BranchSpent(N.Branch) < N.Gate)) return ENodeState::Locked;
		return ENodeState::Available;
	}
	/** The server's own rule (empty = can buy now). */
	FString WhyNot(const FWRPathNodeDef& N) const
	{
		const UWRPathComponent* C = Comp();
		const FWRPathDef* D = Def();
		if (!C || !D) return TEXT("no path");
		if (!C->IsActivePath(Path)) return TEXT("not your main path or subclass");
		const FName Other = Path == C->GetMainPath() ? C->GetSubPath() : C->GetMainPath();
		return FWRPathRules::CanBuy(*D, Progress(), C->GetSpent(Other), N.Id);
	}
	bool Castable(const FWRPathNodeDef& N) const { return N.Kind != EWRNodeKind::Stat && UWRAbilityRegistry::ClassForNode(Path, N.Id) != nullptr; }
	/** The id wheels and quick slots use: "fire.fire_bolt". */
	FName WheelId(FName NodeId) const { return FName(*FString::Printf(TEXT("%s.%s"), *Path.ToString(), *NodeId.ToString())); }

	FNodeLook Look(const FWRPathNodeDef& N, bool bSelected) const
	{
		const FPathColours C = ColoursFor(Path);
		FNodeLook L;
		const ENodeState S = State(N);
		if (S == ENodeState::Learned)
		{
			L = { WRUI::Brass, C.LitFill, C.Main, C.LitP, C.LitAc, WRUI::Ink, WRUI::Brass, Hex(TEXT("4D5258")), 0.5f };
		}
		else if (S == ENodeState::Available)
		{
			const bool bShort = Points() < N.Cost;
			L = { Hex(TEXT("F2DEB0")), Hex(TEXT("2C2E32")), Hex(TEXT("F2DEB0")), WRUI::Ink, WRUI::Brass, WRUI::Ink, bShort ? WRUI::Warn : WRUI::Ink2, Hex(TEXT("7A6C50")), 0.3f };
		}
		else
		{
			L = { Hex(TEXT("3A3E44")), Hex(TEXT("191B1E")), FLinearColor::Transparent, Hex(TEXT("6A6E73")), Hex(TEXT("4B4F54")), Hex(TEXT("A39780")), WRUI::Ink3, Hex(TEXT("34383D")), 0.f };
			if (bSelected)
			{
				L.P = Hex(TEXT("8D9196"));
				L.Ac = Hex(TEXT("686C71"));
				L.Name = WRUI::Ink;
				L.Meta = WRUI::Ink2;
			}
		}
		if (bSelected) L.Ring = WRUI::Brass;
		return L;
	}

	/** "Learned", "2/3 · 1 pt/rank", "needs 8 · 5 pts". */
	FString Meta(const FWRPathNodeDef& N) const
	{
		const bool bRanked = N.Kind == EWRNodeKind::Stat;
		// The starter spell comes free with the path (WRPathTypes).
		const FString Cost = N.bStarter ? FString(TEXT("starter · free")) : bRanked ? FString(TEXT("1 pt/rank")) : FString::Printf(TEXT("%d pts"), N.Cost);
		const int32 Rk = Rank(N.Id);
		switch (State(N))
		{
		case ENodeState::Learned: return bRanked ? FString::Printf(TEXT("%d/%d · %s"), Rk, N.MaxRank, *Cost) : FString(N.bStarter ? TEXT("Starter") : TEXT("Learned"));
		case ENodeState::Available: return bRanked ? FString::Printf(TEXT("%d/%d · %s"), Rk, N.MaxRank, *Cost) : Cost;
		default: return N.Gate > 0 ? FString::Printf(TEXT("needs %d · %s"), N.Gate, *Cost) : Cost;
		}
	}

	FString BranchName(FName Branch) const { return Branch.IsNone() ? FString(TEXT("Trunk")) : Humanize(Branch.ToString()); }

	/** "Spell", "Passive", "Stat", "Capstone"; abilities the game doesn't have yet say so. */
	FString KindName(const FWRPathNodeDef& N) const
	{
		if (N.Kind == EWRNodeKind::Stat) return TEXT("Stat");
		const bool bBuilt = UWRAbilityRegistry::IsBuilt(Path, N.Id);
		const bool bCast = Castable(N);
		const FString K = N.Kind == EWRNodeKind::Capstone ? FString(TEXT("Capstone")) : bCast ? FString(TEXT("Ability")) : FString(TEXT("Passive"));
		return bBuilt ? K : K + TEXT(" · not built yet");
	}
};

/** A learned ability dragged from the tree or a wheel slot onto a wheel slot. */
class FWRAbilityDragDrop : public FDragDropOperation
{
public:
	DRAG_DROP_OPERATOR_TYPE(FWRAbilityDragDrop, FDragDropOperation)
	FName WheelId;           // "fire.fire_bolt"
	int32 FromWheel = -1;    // a wheel slot it came from (-1: the tree)
	int32 FromPos = -1;
	static TSharedRef<FWRAbilityDragDrop> New(FName WheelId, int32 FromWheel, int32 FromPos)
	{
		TSharedRef<FWRAbilityDragDrop> Op = MakeShared<FWRAbilityDragDrop>();
		Op->WheelId = WheelId;
		Op->FromWheel = FromWheel;
		Op->FromPos = FromPos;
		Op->Construct();
		return Op;
	}
	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override
	{
		FString PathStr, NodeStr;
		WheelId.ToString().Split(TEXT("."), &PathStr, &NodeStr);
		const FPathColours C = ColoursFor(FName(*PathStr));
		return SNew(SBox).WidthOverride(48.f * U).HeightOverride(48.f * U)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()[SNew(SImage).Image(WRUI::Box(C.LitFill, 24.f * U, WRUI::Brass, 2.f))]
			+ SOverlay::Slot().Padding(10.f * U)[SNew(SImage).Image(Glyph(TEXT("path_") + NodeStr + TEXT("_a"))).ColorAndOpacity(C.LitAc)]
			+ SOverlay::Slot().Padding(10.f * U)[SNew(SImage).Image(Glyph(TEXT("path_") + NodeStr + TEXT("_p"))).ColorAndOpacity(C.LitP)]
		];
	}
};

/** One node of the tree: ring, glyph, badges, name, cost line and rank pips. */
class SWRPathNode : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWRPathNode) {}
		SLATE_ARGUMENT(TSharedPtr<FWRPathsModel>, Model)
		SLATE_ARGUMENT(FName, NodeId)
		SLATE_ATTRIBUTE(bool, Selected)
	SLATE_END_ARGS()

	TFunction<void(FName, bool)> OnHover;
	TFunction<void(FName)> OnSelect;
	TFunction<void(FName)> OnLearn;

	void Construct(const FArguments& InArgs)
	{
		Model = InArgs._Model;
		NodeId = InArgs._NodeId;
		Selected = InArgs._Selected;
		const FWRPathNodeDef* N = Model->Node(NodeId);
		const FString G = TEXT("path_") + NodeId.ToString();
		const int32 Ranks = N && N->Kind == EWRNodeKind::Stat ? N->MaxRank : 0;

		TSharedRef<SHorizontalBox> Pips = SNew(SHorizontalBox);
		for (int32 i = 0; i < Ranks; ++i)
		{
			Pips->AddSlot().AutoWidth().Padding(i > 0 ? 3.f * U : 0.f, 0.f, 0.f, 0.f)
			[
				SNew(SBox).WidthOverride(9.f * U).HeightOverride(5.f * U)
				[
					SNew(SImage).Image(WRUI::White()).ColorAndOpacity_Lambda([this, i]()
					{
						const FWRPathNodeDef* D = Model->Node(NodeId);
						return D && i < Model->Rank(NodeId) ? ColoursFor(Model->Path).Main : Look().PipOff;
					})
				]
			];
		}

		// The 56 px circle sits in a 76 px box so its glow and halo have room.
		const float Box = 76.f * U, Ring = 56.f * U;
		TSharedRef<SCanvas> Badges = SNew(SCanvas);
		// Castable on the wheel: a diamond at the top right.
		Badges->AddSlot().Position(FVector2D(46.f, 4.f) * U).Size(FVector2D(18.f, 18.f) * U)
		[
			SNew(SOverlay).Visibility_Lambda([this]() { const FWRPathNodeDef* D = Model->Node(NodeId); return D && Model->Castable(*D) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			+ SOverlay::Slot()[SNew(SImage).Image(Glyph(TEXT("diamond"))).ColorAndOpacity_Lambda([this]() { return DiamondFill(); })]
			+ SOverlay::Slot()[SNew(SImage).Image(Glyph(TEXT("diamond_line"))).ColorAndOpacity_Lambda([this]() { return Look().Ring == WRUI::Brass ? Hex(TEXT("FFD9A0")) : Look().Ring; })]
			+ SOverlay::Slot().Padding(5.5f * U)[SNew(SImage).Image(Glyph(TEXT("diamond"))).ColorAndOpacity_Lambda([this]() { return State() == ENodeState::Locked ? Hex(TEXT("3E4247")) : State() == ENodeState::Learned ? Hex(TEXT("FFF0D0")) : WRUI::Brass; })]
		];
		// Locked: a padlock; maxed: a brass tick.
		Badges->AddSlot().Position(FVector2D(44.f, 44.f) * U).Size(FVector2D(22.f, 22.f) * U)
		[
			SNew(SOverlay).Visibility_Lambda([this]() { return State() == ENodeState::Locked ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			+ SOverlay::Slot()[SNew(SImage).Image(WRUI::Box(Hex(TEXT("0D0E10")), 11.f * U, Hex(TEXT("5A5F66")), 1.f))]
			+ SOverlay::Slot().Padding(5.f * U)[SNew(SImage).Image(Glyph(TEXT("lock"))).ColorAndOpacity(Hex(TEXT("B9AD94")))]
		];
		Badges->AddSlot().Position(FVector2D(44.f, 44.f) * U).Size(FVector2D(22.f, 22.f) * U)
		[
			SNew(SOverlay).Visibility_Lambda([this]() { const FWRPathNodeDef* D = Model->Node(NodeId); return D && Model->Rank(NodeId) >= D->MaxRank ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			+ SOverlay::Slot()[SNew(SImage).Image(WRUI::Box(Hex(TEXT("C99E5A")), 11.f * U, Hex(TEXT("2A1D12")), 1.f))]
			+ SOverlay::Slot().Padding(4.5f * U)[SNew(SImage).Image(Glyph(TEXT("check"))).ColorAndOpacity(Hex(TEXT("2A1D12")))]
		];

		ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(SBox).WidthOverride(Box).HeightOverride(Box)
				[
					SNew(SOverlay)
					// Glow, then the selected node's brass halo, then the pulsing ring of a node you can take.
					+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
					[
						SNew(SBox).WidthOverride(72.f * U).HeightOverride(72.f * U)
						[SNew(SImage).Image(WRUI::Box(FLinearColor::White, 36.f * U)).ColorAndOpacity_Lambda([this]() { const FNodeLook L = Look(); return WRUI::Alpha(L.Glow, L.GlowA * 0.22f); })]
					]
					+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
					[
						SNew(SBox).WidthOverride(64.f * U).HeightOverride(64.f * U)
						[SNew(SImage).Image(WRUI::Box(FLinearColor::White, 32.f * U)).ColorAndOpacity_Lambda([this]() { const FNodeLook L = Look(); return WRUI::Alpha(L.Glow, L.GlowA * 0.35f); })]
					]
					+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
					[
						// The node you point at gets a fainter one, so it's clear what the details panel is showing.
						SNew(SBox).WidthOverride(74.f * U).HeightOverride(74.f * U).Visibility_Lambda([this]() { return Selected.Get() || IsHovered() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
						[SNew(SImage).Image(WRUI::Box(WRUI::Alpha(WRUI::Brass, 0.18f), 37.f * U, WRUI::Brass, 3.f)).ColorAndOpacity_Lambda([this]() { return FLinearColor(1.f, 1.f, 1.f, Selected.Get() ? 1.f : 0.5f); })]
					]
					+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
					[
						SNew(SBox).WidthOverride(76.f * U).HeightOverride(76.f * U).Visibility_Lambda([this]() { return State() == ENodeState::Available && !Selected.Get() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
						[
							SNew(SImage).Image(WRUI::Box(FLinearColor::Transparent, 38.f * U, Hex(TEXT("F2DEB0")), 2.f))
							.ColorAndOpacity_Lambda([]() { const double T = FSlateApplication::Get().GetCurrentTime(); return FLinearColor(1.f, 1.f, 1.f, 0.12f + 0.5f * (0.5f + 0.5f * FMath::Sin(T * 2.0 * PI / 2.4))); })
						]
					]
					+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
					[
						SNew(SBox).WidthOverride(Ring + 4.f * U).HeightOverride(Ring + 4.f * U)
						[SNew(SImage).Image(WRUI::Box(Hex(TEXT("0A0B0C")), (Ring + 4.f * U) * 0.5f))]
					]
					+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
					[
						SNew(SBox).WidthOverride(Ring).HeightOverride(Ring)
						[SNew(SImage).Image_Lambda([this, Ring]() { const FNodeLook L = Look(); return WRUI::Box(L.Fill, Ring * 0.5f, L.Ring, 3.f * U); })]
					]
					// A soft light from above, the mockups' radial gradient.
					+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0.f, 0.f, 0.f, 10.f * U)
					[
						SNew(SBox).WidthOverride(34.f * U).HeightOverride(26.f * U)
						[SNew(SImage).Image(WRUI::Box(FLinearColor(1.f, 1.f, 1.f, 0.05f), 13.f * U))]
					]
					+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
					[
						SNew(SBox).WidthOverride(30.f * U).HeightOverride(30.f * U)
						[
							SNew(SOverlay)
							+ SOverlay::Slot()[SNew(SImage).Image(Glyph(G + TEXT("_a"))).ColorAndOpacity_Lambda([this]() { return Look().Ac; })]
							+ SOverlay::Slot()[SNew(SImage).Image(Glyph(G + TEXT("_p"))).ColorAndOpacity_Lambda([this]() { return Look().P; })]
						]
					]
					+ SOverlay::Slot()[Badges]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, -5.f * U, 0.f, 0.f)
			[
				WRUI::Text(FText::FromString(N ? N->Name : NodeId.ToString()), WRUI::Body(13, 800), TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(Look().Name); }), true)
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				WRUI::Text(TAttribute<FText>::CreateLambda([this]() { const FWRPathNodeDef* D = Model->Node(NodeId); return D ? FText::FromString(Model->Meta(*D)) : FText::GetEmpty(); }),
					WRUI::Body(11, 700), TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(Look().Meta); }))
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 4.f * U, 0.f, 0.f)
			[
				Pips
			]
		];
		SetCursor(EMouseCursor::Hand);
	}

	virtual void OnMouseEnter(const FGeometry& Geo, const FPointerEvent& Event) override
	{
		SCompoundWidget::OnMouseEnter(Geo, Event);
		if (OnHover) OnHover(NodeId, true);
	}
	virtual void OnMouseLeave(const FPointerEvent& Event) override
	{
		SCompoundWidget::OnMouseLeave(Event);
		if (OnHover) OnHover(NodeId, false);
	}
	virtual FReply OnMouseButtonDown(const FGeometry& Geo, const FPointerEvent& Event) override
	{
		if (Event.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
		const FWRPathNodeDef* D = Model->Node(NodeId);
		if (D && Model->Castable(*D) && Model->Rank(NodeId) > 0)
		{
			return FReply::Handled().DetectDrag(SharedThis(this), EKeys::LeftMouseButton);
		}
		return FReply::Handled();
	}
	virtual FReply OnMouseButtonUp(const FGeometry& Geo, const FPointerEvent& Event) override
	{
		if (Event.GetEffectingButton() == EKeys::LeftMouseButton && OnSelect) OnSelect(NodeId);
		return FReply::Handled();
	}
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& Geo, const FPointerEvent& Event) override
	{
		if (OnLearn) OnLearn(NodeId);
		return FReply::Handled();
	}
	virtual FReply OnDragDetected(const FGeometry& Geo, const FPointerEvent& Event) override
	{
		if (OnSelect) OnSelect(NodeId);
		return FReply::Handled().BeginDragDrop(FWRAbilityDragDrop::New(Model->WheelId(NodeId), -1, -1));
	}

private:
	TSharedPtr<FWRPathsModel> Model;
	FName NodeId;
	TAttribute<bool> Selected;

	ENodeState State() const { const FWRPathNodeDef* D = Model->Node(NodeId); return D ? Model->State(*D) : ENodeState::Locked; }
	FNodeLook Look() const { const FWRPathNodeDef* D = Model->Node(NodeId); return D ? Model->Look(*D, Selected.Get()) : FNodeLook(); }
	FLinearColor DiamondFill() const
	{
		const ENodeState S = State();
		return S == ENodeState::Learned ? ColoursFor(Model->Path).Main : S == ENodeState::Available ? Hex(TEXT("2A2C30")) : Hex(TEXT("1A1C1F"));
	}
};

/** The carved connectors between the nodes: dark grooves, brass where you can go next, lit where you have been. */
class SWRPathWires : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SWRPathWires) {}
		SLATE_ARGUMENT(TSharedPtr<FWRPathsModel>, Model)
	SLATE_END_ARGS()
	void Construct(const FArguments& InArgs) { Model = InArgs._Model; }
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1002.f, 640.f) * U; }

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override
	{
		const FWRPathDef* Def = Model->Def();
		if (!Def) return Layer;
		enum ECat { Dark, Ready, Lit };
		TArray<TPair<TArray<FVector2f>, ECat>> Lines;
		const auto Add = [&Lines](std::initializer_list<FVector2f> Pts, ECat C) { TArray<FVector2f> A; for (const FVector2f& P : Pts) A.Add(P * U); Lines.Add({ MoveTemp(A), C }); };
		const auto Cat = [this](const FWRPathNodeDef& N) { const ENodeState S = Model->State(N); return S == ENodeState::Learned ? Lit : S == ENodeState::Available ? Ready : Dark; };

		// Trunk: left to right, each stretch lit once the node it reaches is learned.
		TArray<const FWRPathNodeDef*> Trunk;
		TArray<FName> Branches;
		for (const FWRPathNodeDef& N : Def->Nodes)
		{
			if (N.Branch.IsNone()) Trunk.Add(&N);
			else Branches.AddUnique(N.Branch);
		}
		for (int32 i = 0; i + 1 < Trunk.Num() && i + 1 < 4; ++i)
		{
			Add({ FVector2f(TX[i], TY), FVector2f(TX[i + 1], TY) }, Cat(*Trunk[i + 1]));
		}
		const bool bTrunk = Model->TrunkDone();
		Add({ FVector2f(500.f, TY), FVector2f(500.f, BAR) }, bTrunk ? Lit : Dark);
		for (int32 b = 0; b < Branches.Num() && b < 3; ++b)
		{
			const float X = SX[b];
			const ECat BarCat = Model->BranchSpent(Branches[b]) > 0 ? Lit : bTrunk && Model->IsActive() ? Ready : Dark;
			if (!FMath::IsNearlyEqual(X, 500.f)) Add({ FVector2f(500.f, BAR), FVector2f(X, BAR) }, BarCat);
			int32 k = 0;
			float Prev = PLATE_B;
			for (const FWRPathNodeDef& N : Def->Nodes)
			{
				if (N.Branch != Branches[b]) continue;
				const float Cy = NY0 + STEP * k + R;
				const float Cx = X + (k % 2 ? OFF : -OFF);
				if (k == 0) Add({ FVector2f(X, BAR), FVector2f(X, PLATE_T) }, Cat(N));
				Add({ FVector2f(X, Prev), FVector2f(X, Cy), FVector2f(Cx, Cy) }, Cat(N));
				Prev = Cy;
				++k;
			}
		}
		// Grooves under everything, then brass for the ready ones, then the lit ones glowing in the path's colour.
		const FPathColours C = ColoursFor(Model->Path);
		const FPaintGeometry PG = Geo.ToPaintGeometry();
		for (const auto& L : Lines) FSlateDrawElement::MakeLines(Out, Layer, PG, L.Key, ESlateDrawEffect::None, Hex(TEXT("050607")), true, 10.f * U);
		for (const auto& L : Lines) FSlateDrawElement::MakeLines(Out, Layer + 1, PG, L.Key, ESlateDrawEffect::None, Hex(TEXT("22262A")), true, 4.f * U);
		for (const auto& L : Lines) if (L.Value == Ready) FSlateDrawElement::MakeLines(Out, Layer + 2, PG, L.Key, ESlateDrawEffect::None, Hex(TEXT("8F7A52")), true, 3.f * U);
		for (const auto& L : Lines) if (L.Value == Lit) FSlateDrawElement::MakeLines(Out, Layer + 2, PG, L.Key, ESlateDrawEffect::None, WRUI::Alpha(C.Main, 0.22f), true, 13.f * U);
		for (const auto& L : Lines) if (L.Value == Lit) FSlateDrawElement::MakeLines(Out, Layer + 3, PG, L.Key, ESlateDrawEffect::None, C.Wire, true, 3.6f * U);
		for (const auto& L : Lines) if (L.Value == Lit) FSlateDrawElement::MakeLines(Out, Layer + 4, PG, L.Key, ESlateDrawEffect::None, C.WireHi, true, 1.2f * U);
		return Layer + 4;
	}

private:
	TSharedPtr<FWRPathsModel> Model;
};

/** One position on an ability wheel: shows the ability placed there; takes drops from the tree or other slots. */
class SWRWheelSlot : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWRWheelSlot) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AWRPlayerController>, PC)
		SLATE_ARGUMENT(int32, Wheel)
		SLATE_ARGUMENT(int32, Pos)
	SLATE_END_ARGS()

	/** The learned ability selected in the tree, if any ("fire.fire_bolt"). */
	TFunction<FName()> GetSelected;

	void Construct(const FArguments& InArgs)
	{
		PC = InArgs._PC;
		Wheel = InArgs._Wheel;
		Pos = InArgs._Pos;
		const float S = 33.f * U;
		ChildSlot
		[
			SNew(SBox).WidthOverride(S).HeightOverride(S)
			.ToolTipText_Lambda([this]() { return Tip(); })
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[SNew(SImage).Image_Lambda([this]()
				{
					const bool bPicked = IsPicked();
					const FLinearColor Rim = bDropHover || IsHovered() ? WRUI::Brass : bPicked ? WRUI::Brass : WRUI::Rim;
					return WRUI::Box(Ability().IsNone() ? FLinearColor(0.06f, 0.065f, 0.075f, 0.9f) : Fill(), 4.f, Rim, bPicked || bDropHover ? 2.f : 1.f);
				})]
				+ SOverlay::Slot().Padding(6.f * U)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()[SNew(SImage).Image_Lambda([this]() { return GlyphFor(TEXT("_a")); }).ColorAndOpacity_Lambda([this]() { return Ability().IsNone() ? FLinearColor::Transparent : Cols().LitAc; })]
					+ SOverlay::Slot()[SNew(SImage).Image_Lambda([this]() { return GlyphFor(TEXT("_p")); }).ColorAndOpacity_Lambda([this]() { return Ability().IsNone() ? FLinearColor::Transparent : Cols().LitP; })]
				]
				+ SOverlay::Slot().Padding(11.f * U)
				[
					SNew(SImage).Image(Glyph(TEXT("plus"))).ColorAndOpacity_Lambda([this]() { return Ability().IsNone() ? WRUI::Alpha(WRUI::Ink3, IsHovered() || bDropHover ? 0.8f : 0.35f) : FLinearColor::Transparent; })
				]
				// The ability a tap of the wheel key casts.
				+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(0.f, 2.f * U, 2.f * U, 0.f)
				[
					SNew(SBox).WidthOverride(8.f * U).HeightOverride(8.f * U).Visibility_Lambda([this]() { return IsPicked() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
					[SNew(SImage).Image(Glyph(TEXT("diamond"))).ColorAndOpacity(WRUI::Brass)]
				]
			]
		];
		SetCursor(EMouseCursor::Hand);
	}

	virtual FReply OnMouseButtonDown(const FGeometry& Geo, const FPointerEvent& Event) override
	{
		if (Event.GetEffectingButton() == EKeys::LeftMouseButton && !Ability().IsNone())
		{
			return FReply::Handled().DetectDrag(SharedThis(this), EKeys::LeftMouseButton);
		}
		return FReply::Handled();
	}
	virtual FReply OnMouseButtonUp(const FGeometry& Geo, const FPointerEvent& Event) override
	{
		FWRPlayerUI* UI = PC.IsValid() ? PC->GetUI() : nullptr;
		if (!UI) return FReply::Handled();
		if (Event.GetEffectingButton() == EKeys::RightMouseButton)
		{
			UI->SetWheelAbility(Wheel, Pos, NAME_None);
			return FReply::Handled();
		}
		// A selected learned ability goes here; otherwise the click makes this the wheel's tap ability.
		const FName Sel = GetSelected ? GetSelected() : NAME_None;
		if (!Sel.IsNone() && Sel != Ability())
		{
			UI->SetWheelAbility(Wheel, Pos, Sel);
		}
		else if (!Ability().IsNone())
		{
			UI->SetWheelPicked(Wheel, Pos);
		}
		return FReply::Handled();
	}
	virtual FReply OnDragDetected(const FGeometry& Geo, const FPointerEvent& Event) override
	{
		return FReply::Handled().BeginDragDrop(FWRAbilityDragDrop::New(Ability(), Wheel, Pos));
	}
	virtual void OnDragEnter(const FGeometry& Geo, const FDragDropEvent& Event) override { bDropHover = Event.GetOperationAs<FWRAbilityDragDrop>().IsValid(); }
	virtual void OnDragLeave(const FDragDropEvent& Event) override { bDropHover = false; }
	virtual FReply OnDrop(const FGeometry& Geo, const FDragDropEvent& Event) override
	{
		bDropHover = false;
		const TSharedPtr<FWRAbilityDragDrop> Op = Event.GetOperationAs<FWRAbilityDragDrop>();
		FWRPlayerUI* UI = PC.IsValid() ? PC->GetUI() : nullptr;
		if (!Op.IsValid() || !UI) return FReply::Unhandled();
		// Slot to slot swaps the two; from the tree it just lands here (and leaves wherever it was).
		const FName Here = Ability();
		UI->SetWheelAbility(Wheel, Pos, Op->WheelId);
		if (Op->FromWheel >= 0 && !Here.IsNone() && Here != Op->WheelId) UI->SetWheelAbility(Op->FromWheel, Op->FromPos, Here);
		return FReply::Handled();
	}

private:
	TWeakObjectPtr<AWRPlayerController> PC;
	int32 Wheel = 0;
	int32 Pos = 0;
	bool bDropHover = false;

	FName Ability() const
	{
		const FWRPlayerUI* UI = PC.IsValid() ? PC->GetUI() : nullptr;
		if (!UI) return NAME_None;
		const FName Id = UI->GetWheelAbility(Wheel, Pos);
		// Only what you can cast now (a slot may remember an ability of a path you've swapped out).
		return UI->FindCastable(Id, UI->GetCastables()) ? Id : NAME_None;
	}
	bool IsPicked() const { const FWRPlayerUI* UI = PC.IsValid() ? PC->GetUI() : nullptr; return UI && UI->GetWheelPicked(Wheel) == Pos && !Ability().IsNone(); }
	FPathColours Cols() const { FString P, N; Ability().ToString().Split(TEXT("."), &P, &N); return ColoursFor(FName(*P)); }
	FLinearColor Fill() const { return Cols().LitFill; }
	const FSlateBrush* GlyphFor(const TCHAR* Layer) const
	{
		FString P, N;
		if (!Ability().ToString().Split(TEXT("."), &P, &N)) return WRUI::White();
		return Glyph(TEXT("path_") + N + Layer);
	}
	FText Tip() const
	{
		const FName Id = Ability();
		const FText Key = WRUI::KeyName(FWRKeyBindings::Get().Key(Wheel == 0 ? TEXT("Wheel1") : TEXT("Wheel2")));
		if (Id.IsNone()) return FText::Format(LOCTEXT("SlotEmptyTip", "Wheel {0}, slot {1}: empty. Drag a learned ability here."), Wheel + 1, Pos + 1);
		FString P, N;
		Id.ToString().Split(TEXT("."), &P, &N);
		const FWRPathDef* Def = FWRPathRegistry::Get().Find(FName(*P));
		const FWRPathNodeDef* Node = Def ? Def->FindNode(FName(*N)) : nullptr;
		return FText::Format(LOCTEXT("SlotTip", "{0}: wheel {1}, slot {2}.\nClick: a tap of {3} casts it. Right-click: clear. Drag: move."),
			FText::FromString(Node ? Node->Name : N), Wheel + 1, Pos + 1, Key);
	}
};

class SWRPathsTab : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWRPathsTab) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AWRPlayerController>, PC)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		PC = InArgs._PC;
		Model = MakeShared<FWRPathsModel>();
		Model->PC = PC;
		const UWRPathComponent* C = Model->Comp();
		Model->Path = C && !C->GetMainPath().IsNone() ? C->GetMainPath() : FWRPathRegistry::Get().All().Num() > 0 ? FWRPathRegistry::Get().All()[0].Id : NAME_None;

		ChildSlot
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).WidthOverride(1002.f * U)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[SNew(SBox).HeightOverride(40.f * U)[Header()]]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f * U, 0.f, 0.f)[SNew(SBox).HeightOverride(640.f * U)[SAssignNew(BoardBox, SBox)]]
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(22.f * U, 0.f, 0.f, 0.f)
			[
				SNew(SWRPanel).Kind(EWRPanel::Leather).Rivets(true).Padding(FMargin(24.f * U, 24.f * U, 24.f * U, 20.f * U))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().FillHeight(1.f)[SAssignNew(DetailsBox, SBox)]
					+ SVerticalBox::Slot().AutoHeight()[Loadout()]
				]
			]
		];
		BuildBoard();
	}

	virtual void Tick(const FGeometry& Geo, const double Time, const float Delta) override
	{
		SCompoundWidget::Tick(Geo, Time, Delta);
		const FString Sig = DetailsSignature();
		if (Sig != ShownSig)
		{
			ShownSig = Sig;
			WRUI::SetContentMeasured(*DetailsBox, Details(), Geo.Scale);
		}
	}

private:
	TWeakObjectPtr<AWRPlayerController> PC;
	TSharedPtr<FWRPathsModel> Model;
	TSharedPtr<SBox> BoardBox;
	TSharedPtr<SBox> DetailsBox;
	FName Selected;
	FName Hovered;
	FString ShownSig;

	FName Shown() const { return !Hovered.IsNone() ? Hovered : Selected; }

	FString DetailsSignature() const
	{
		const FName Id = Shown();
		return FString::Printf(TEXT("%s|%s|%d|%d|%d|%d|%d"), *Model->Path.ToString(), *Id.ToString(), Model->Rank(Id), Model->Points(), Model->TrunkDone() ? 1 : 0,
			Model->Node(Id) ? Model->BranchSpent(Model->Node(Id)->Branch) : 0, Model->IsActive() ? 1 : 0);
	}

	void Learn(FName NodeId)
	{
		const FWRPathNodeDef* N = Model->Node(NodeId);
		if (!N || !PC.IsValid()) return;
		const FString Why = Model->WhyNot(*N);
		if (!Why.IsEmpty())
		{
			PC->ShowLocalNotice(FString::Printf(TEXT("%s: %s"), *N->Name, *Why));
			return;
		}
		PC->SendCommand(FString::Printf(TEXT("learn %s %s"), *Model->Path.ToString(), *NodeId.ToString()));
	}

	/** The learned castable ability selected in the tree, for click-to-place on a wheel. */
	FName SelectedCastable() const
	{
		const FWRPathNodeDef* N = Model->Node(Selected);
		return N && Model->Castable(*N) && Model->Rank(Selected) > 0 && Model->IsActive() ? Model->WheelId(Selected) : NAME_None;
	}

	void ViewPath(FName Path)
	{
		if (Path == Model->Path || !FWRPathRegistry::Get().Find(Path)) return;
		Model->Path = Path;
		BuildBoard();
	}

	/** Pick something worth reading when the tab opens: a node you can take now, else the next one to unlock. */
	FName DefaultSelection() const
	{
		const FWRPathDef* Def = Model->Def();
		if (!Def) return NAME_None;
		for (const FWRPathNodeDef& N : Def->Nodes) if (Model->WhyNot(N).IsEmpty()) return N.Id;
		for (const FWRPathNodeDef& N : Def->Nodes) if (Model->State(N) == ENodeState::Available && Model->Rank(N.Id) < N.MaxRank) return N.Id;
		for (const FWRPathNodeDef& N : Def->Nodes) if (Model->State(N) == ENodeState::Locked) return N.Id;
		return Def->Nodes.Num() > 0 ? Def->Nodes[0].Id : NAME_None;
	}

	// ---------------------------------------------------------------- header: the path pill and the subclass chip

	TSharedRef<SWidget> PathPill(FName Path, bool bMain)
	{
		const FPathColours C = ColoursFor(Path);
		const FWRPathDef* Def = FWRPathRegistry::Get().Find(Path);
		const FString Name = Def ? Def->Name : Humanize(Path.ToString());
		const FString Icon = Path == TEXT("frost") ? TEXT("snowflake") : TEXT("flame");
		return SNew(SButton).ButtonStyle(FCoreStyle::Get(), "NoBorder").ContentPadding(0.f).Cursor(EMouseCursor::Hand)
			.OnClicked_Lambda([this, Path]() { ViewPath(Path); return FReply::Handled(); })
			[
				SNew(SBorder)
				.BorderImage_Lambda([this, Path, C]() { const bool bOn = Model->Path == Path; return WRUI::Box(bOn ? FLinearColor(C.Main.R * 0.22f, C.Main.G * 0.22f, C.Main.B * 0.22f, 0.75f) : FLinearColor(0.f, 0.f, 0.f, 0.4f), 20.f * U, WRUI::Alpha(C.Main, bOn ? 0.65f : 0.4f), 1.f); })
				.Padding(FMargin(5.f * U, 0.f, 18.f * U, 0.f))
				[
					SNew(SBox).HeightOverride(40.f * U).VAlign(VAlign_Center)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(SBox).WidthOverride(30.f * U).HeightOverride(30.f * U)
							[
								SNew(SOverlay)
								+ SOverlay::Slot()[SNew(SImage).Image(WRUI::Box(C.LitFill, 15.f * U, WRUI::Brass, 2.f))]
								+ SOverlay::Slot().Padding(6.f * U)[SNew(SImage).Image(Glyph(Icon)).ColorAndOpacity(C.Main)]
							]
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.f * U, 0.f, 0.f, 0.f)
						[
							WRUI::Text(FText::FromString(Name), WRUI::Title(17), C.Main)
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.f * U, 2.f * U, 0.f, 0.f)
						[
							WRUI::Text(bMain ? LOCTEXT("MainPath", "· main path") : LOCTEXT("SubPath", "· subclass"), WRUI::Body(14, 700), C.Main)
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.f * U, 0.f)
						[
							SNew(SBox).WidthOverride(1.f).HeightOverride(22.f * U)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity(WRUI::Alpha(C.Main, 0.45f))]
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							WRUI::Text(TAttribute<FText>::CreateLambda([this, Path]() { const UWRPathComponent* P = Model->Comp(); return FText::AsNumber(P ? P->GetEarned(Path) - P->GetSpent(Path) : 0); }), WRUI::Body(19, 800), WRUI::Ink)
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(5.f * U, 2.f * U, 0.f, 0.f)
						[
							WRUI::Text(LOCTEXT("PointsToSpend", "points to spend"), WRUI::Body(14, 500), WRUI::Ink)
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(5.f * U, 2.f * U, 0.f, 0.f)
						[
							WRUI::Text(TAttribute<FText>::CreateLambda([this, Path]() { const UWRPathComponent* P = Model->Comp(); return FText::Format(LOCTEXT("Earned", "· {0} earned"), P ? P->GetEarned(Path) : 0); }), WRUI::Body(12, 500), WRUI::Ink3)
						]
					]
				]
			];
	}

	TSharedRef<SWidget> Header()
	{
		const UWRPathComponent* C = Model->Comp();
		const FName Main = C ? C->GetMainPath() : Model->Path;
		const FName Sub = C ? C->GetSubPath() : NAME_None;
		TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
		Row->AddSlot().AutoWidth().VAlign(VAlign_Center)[PathPill(Main.IsNone() ? Model->Path : Main, true)];
		if (!Sub.IsNone())
		{
			Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(12.f * U, 0.f, 0.f, 0.f)[PathPill(Sub, false)];
		}
		else
		{
			// No subclass yet: the other paths are offered at a boss altar.
			for (const FWRPathDef& D : FWRPathRegistry::Get().All())
			{
				if (D.Id == Main) continue;
				const FPathColours PC2 = ColoursFor(D.Id);
				Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(12.f * U, 0.f, 0.f, 0.f)
				[
					SNew(SBorder).BorderImage(WRUI::Box(WRUI::Alpha(PC2.Main, 0.07f), 17.f * U, WRUI::Alpha(PC2.Main, 0.35f), 1.f)).Padding(FMargin(14.f * U, 0.f))
					.ToolTipText(LOCTEXT("SubTip", "A subclass comes from an altar: make an offering at a boss altar of the other biome."))
					[
						SNew(SBox).HeightOverride(34.f * U)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								SNew(SBox).WidthOverride(15.f * U).HeightOverride(15.f * U)[SNew(SImage).Image(Glyph(D.Id == TEXT("frost") ? TEXT("snowflake") : TEXT("flame"))).ColorAndOpacity(WRUI::Alpha(PC2.Main, 0.7f))]
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f * U, 0.f)
							[
								WRUI::Text(FText::Format(LOCTEXT("SubLocked", "{0} · subclass, locked: offer at a boss altar"), FText::FromString(D.Name)), WRUI::Body(13, 700), WRUI::Alpha(PC2.Main, 0.7f))
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								SNew(SBox).WidthOverride(13.f * U).HeightOverride(13.f * U)[SNew(SImage).Image(Glyph(TEXT("lock"))).ColorAndOpacity(WRUI::Alpha(PC2.Main, 0.7f))]
							]
						]
					]
				];
			}
		}
		return Row;
	}

	// ---------------------------------------------------------------- the board

	void BuildBoard()
	{
		Selected = DefaultSelection();
		Hovered = NAME_None;
		ShownSig.Reset();
		const FWRPathDef* Def = Model->Def();
		TSharedRef<SCanvas> Canvas = SNew(SCanvas);

		TArray<FName> Branches;
		TArray<const FWRPathNodeDef*> Trunk;
		if (Def)
		{
			for (const FWRPathNodeDef& N : Def->Nodes)
			{
				if (N.Branch.IsNone()) Trunk.Add(&N);
				else Branches.AddUnique(N.Branch);
			}
		}
		const FPathColours C = ColoursFor(Model->Path);
		const float PanelX[3] = { 6.f, 339.f, 673.f };
		for (int32 b = 0; b < Branches.Num() && b < 3; ++b)
		{
			const FName Br = Branches[b];
			Place(Canvas, PanelX[b], 148.f, 321.f, 484.f,
				SNew(SOverlay)
				+ SOverlay::Slot()[SNew(SImage).Image(WRUI::Box(FLinearColor(1.f, 1.f, 1.f, 0.025f), 6.f, FLinearColor(0.f, 0.f, 0.f, 0.6f), 1.f))]
				+ SOverlay::Slot().Padding(1.f)[Warmth(Br)]);
		}
		// The selected node's brass plate (behind the wires and the node).
		Canvas->AddSlot().Size(FVector2D(148.f, 118.f) * U)
			.Position_Lambda([this]() { return PlateAt(); })
			[
				SNew(SImage).Image(WRUI::Box(WRUI::Alpha(WRUI::Brass, 0.1f), 8.f, WRUI::Brass, 1.f))
				.Visibility_Lambda([this]() { return PlateAt().X > -1000.f ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			];
		Place(Canvas, 0.f, 0.f, 1002.f, 640.f, SNew(SWRPathWires).Model(Model).Visibility(EVisibility::HitTestInvisible));

		// Trunk label and the key.
		Place(Canvas, 18.f, 22.f, 132.f, 60.f,
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(LOCTEXT("Trunk", "Trunk"), WRUI::Title(17), WRUI::Ink)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 5.f * U, 0.f, 0.f)[WRUI::Text(LOCTEXT("TrunkSub", "Every branch\ngrows from here"), WRUI::Body(12, 500), WRUI::Ink3)]);
		Place(Canvas, 850.f, 10.f, 150.f, 136.f, Key());

		for (int32 b = 0; b < Branches.Num() && b < 3; ++b)
		{
			const FName Br = Branches[b];
			Place(Canvas, SX[b] - 96.f, PLATE_T, 192.f, 32.f,
				SNew(SOverlay)
				+ SOverlay::Slot()[SNew(SImage).Image(WRUI::Box(Hex(TEXT("24272B")), 4.f, WRUI::Brass2, 1.f))]
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						WRUI::Text(FText::FromString(Model->BranchName(Br)), WRUI::Title(14), TAttribute<FSlateColor>::CreateLambda([this, Br, C]() { return FSlateColor(Model->BranchSpent(Br) > 0 ? C.Main : WRUI::Ink); }))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f * U, 0.f, 0.f, 0.f)
					[
						SNew(SWRChip).FontSize(11)
						.Text_Lambda([this, Br]() { return FText::Format(LOCTEXT("Spent", "{0} spent"), Model->BranchSpent(Br)); })
						.Color_Lambda([this, Br, C]() { return Model->BranchSpent(Br) > 0 ? C.Main : WRUI::Ink3; })
					]
				]);
		}

		const auto AddNode = [&](const FWRPathNodeDef& N, float Cx, float Top)
		{
			TSharedRef<SWRPathNode> W = SNew(SWRPathNode).Model(Model).NodeId(N.Id).Selected_Lambda([this, Id = N.Id]() { return Selected == Id; });
			W->OnHover = [this](FName Id, bool bIn) { if (bIn) Hovered = Id; else if (Hovered == Id) Hovered = NAME_None; };
			W->OnSelect = [this](FName Id) { Selected = Id; };
			W->OnLearn = [this](FName Id) { Learn(Id); };
			Place(Canvas, Cx - 66.f, Top - 10.f, 132.f, 122.f, W);
		};
		for (int32 i = 0; i < Trunk.Num() && i < 4; ++i) AddNode(*Trunk[i], TX[i], TY - R);
		for (int32 b = 0; b < Branches.Num() && b < 3; ++b)
		{
			int32 k = 0;
			for (const FWRPathNodeDef& N : Def->Nodes)
			{
				if (N.Branch != Branches[b]) continue;
				AddNode(N, SX[b] + (k % 2 ? OFF : -OFF), NY0 + STEP * k);
				++k;
			}
		}

		BoardBox->SetContent(SNew(SWRPanel).Kind(EWRPanel::Inset).Padding(0.f)[Canvas]);
	}

	/** A branch you've put points in is warmed by its path's colour from the top: stacked bands read as a gradient. */
	TSharedRef<SWidget> Warmth(FName Branch)
	{
		const FPathColours C = ColoursFor(Model->Path);
		TSharedRef<SOverlay> O = SNew(SOverlay).Visibility(EVisibility::HitTestInvisible);
		for (int32 i = 1; i <= 8; ++i)
		{
			O->AddSlot().VAlign(VAlign_Top)
			[
				SNew(SBox).HeightOverride(i * 26.f * U)
				[SNew(SImage).Image(WRUI::Box(FLinearColor::White, 6.f)).ColorAndOpacity_Lambda([this, Branch, C]() { return WRUI::Alpha(C.Main, Model->BranchSpent(Branch) > 0 ? 0.014f : 0.f); })]
			];
		}
		return O;
	}

	/** Where the brass plate goes: behind the selected node (off the board when none). */
	FVector2D PlateAt() const
	{
		const FWRPathDef* Def = Model->Def();
		if (!Def || Selected.IsNone()) return FVector2D(-5000.f, 0.f);
		int32 TrunkIdx = 0;
		TArray<FName> Branches;
		TMap<FName, int32> InBranch;
		for (const FWRPathNodeDef& N : Def->Nodes)
		{
			float Cx = 0.f, Top = 0.f;
			if (N.Branch.IsNone())
			{
				if (TrunkIdx >= 4) continue;
				Cx = TX[TrunkIdx++];
				Top = TY - R;
			}
			else
			{
				const int32 b = Branches.AddUnique(N.Branch);
				const int32 k = InBranch.FindOrAdd(N.Branch)++;
				if (b >= 3) continue;
				Cx = SX[b] + (k % 2 ? OFF : -OFF);
				Top = NY0 + STEP * k;
			}
			if (N.Id == Selected) return FVector2D(Cx - 74.f, Top - 8.f) * U;
		}
		return FVector2D(-5000.f, 0.f);
	}

	TSharedRef<SWidget> Key()
	{
		const FPathColours C = ColoursFor(Model->Path);
		const auto Row = [](TSharedRef<SWidget> Icon, const FText& Text, const FText& Sub = FText::GetEmpty()) -> TSharedRef<SWidget>
		{
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.f, 2.f * U, 8.f * U, 0.f)[SNew(SBox).WidthOverride(14.f * U).HeightOverride(14.f * U)[Icon]]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(Text, WRUI::Body(12, 500), WRUI::Ink2)]
					+ SVerticalBox::Slot().AutoHeight()[Sub.IsEmpty() ? SNullWidget::NullWidget : WRUI::Text(Sub, WRUI::Body(11, 500), WRUI::Ink3)]
				];
		};
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 3.f * U)[WRUI::Text(LOCTEXT("KeyTitle", "Key"), WRUI::Caps(11), WRUI::Ink3)]
			+ SVerticalBox::Slot().AutoHeight()[Row(SNew(SImage).Image(WRUI::Box(C.Main, 7.f * U, WRUI::Brass, 2.f)), LOCTEXT("KeyLearned", "Learned"))]
			+ SVerticalBox::Slot().AutoHeight()[Row(SNew(SImage).Image(WRUI::Box(Hex(TEXT("2C2E32")), 7.f * U, Hex(TEXT("F2DEB0")), 2.f)), LOCTEXT("KeyAvail", "Available"))]
			+ SVerticalBox::Slot().AutoHeight()[Row(SNew(SImage).Image(WRUI::Box(Hex(TEXT("191B1E")), 7.f * U, Hex(TEXT("3A3E44")), 2.f)), LOCTEXT("KeyLocked", "Locked"))]
			+ SVerticalBox::Slot().AutoHeight()[Row(SNew(SImage).Image(Glyph(TEXT("diamond_line"))).ColorAndOpacity(Hex(TEXT("F2DEB0"))), LOCTEXT("KeyCast", "Castable"), LOCTEXT("KeyCastSub", "on your ability wheel"))]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f * U, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f * U, 0.f)[WRUI::Text(LOCTEXT("KeyPts", "4 pts"), WRUI::Body(11, 800), WRUI::Warn)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(LOCTEXT("KeyShort", "Short of points"), WRUI::Body(12, 500), WRUI::Ink2)]
			];
	}

	// ---------------------------------------------------------------- details of the hovered / selected node

	TSharedRef<SWidget> StatRow(const FString& Icon, const FLinearColor& IconColor, const FText& Label, const FText& Value, const FLinearColor& ValueColor, bool bLast)
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBox).HeightOverride(34.f * U)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f * U, 0.f)
					[
						SNew(SBox).WidthOverride(15.f * U).HeightOverride(15.f * U)[Icon.IsEmpty() ? SNullWidget::NullWidget : SNew(SImage).Image(Glyph(Icon)).ColorAndOpacity(IconColor)]
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[WRUI::Text(Label, WRUI::Body(13, 500), WRUI::Ink2)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(Value, WRUI::Body(14, 800), ValueColor)]
				]
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBox).HeightOverride(1.f).Visibility(bLast ? EVisibility::Collapsed : EVisibility::Visible)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.07f))]
			];
	}

	TSharedRef<SWidget> Details()
	{
		const FWRPathNodeDef* N = Model->Node(Shown());
		if (!N)
		{
			return SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)[WRUI::Text(LOCTEXT("PickNode", "Point at a node to read it."), WRUI::Body(14, 500), WRUI::Ink3)];
		}
		const FPathColours C = ColoursFor(Model->Path);
		const ENodeState State = Model->State(*N);
		const int32 Rank = Model->Rank(N->Id);
		const int32 Points = Model->Points();
		const bool bRanked = N->Kind == EWRNodeKind::Stat;
		const bool bMaxed = Rank >= N->MaxRank;
		const FString Why = Model->WhyNot(*N);
		const FNodeLook L = Model->Look(*N, true);
		const FString G = TEXT("path_") + N->Id.ToString();

		// Title block.
		const FText ChipText = bMaxed ? LOCTEXT("ChipLearned", "Learned") : Rank > 0 ? FText::Format(LOCTEXT("ChipRank", "Rank {0}/{1}"), Rank, N->MaxRank)
			: State == ENodeState::Locked ? LOCTEXT("ChipLocked", "Locked") : Points < N->Cost ? LOCTEXT("ChipShort", "Short of points") : LOCTEXT("ChipAvail", "Available");
		const FLinearColor ChipColor = bMaxed || Rank > 0 ? WRUI::Good : State == ENodeState::Locked ? WRUI::Ink2 : Points < N->Cost ? WRUI::Warn : WRUI::Brass;
		const int32 TitleSize = N->Name.Len() > 13 ? 19 : 24;

		TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
		Box->AddSlot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(92.f * U).HeightOverride(92.f * U)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()[SNew(SImage).Image(WRUI::Box(WRUI::Alpha(C.Main, 0.12f), 46.f * U))]
					+ SOverlay::Slot().Padding(1.f * U)[SNew(SImage).Image(WRUI::Box(FLinearColor::Transparent, 45.f * U, WRUI::Alpha(WRUI::Brass, 0.45f), 2.f * U))]
					+ SOverlay::Slot().Padding(4.f * U)[SNew(SImage).Image(WRUI::Box(Hex(TEXT("0A0B0C")), 42.f * U))]
					+ SOverlay::Slot().Padding(5.f * U)[SNew(SImage).Image(WRUI::Box(State == ENodeState::Learned ? C.LitFill : Hex(TEXT("1E1611")), 41.f * U, WRUI::Brass, 3.f * U))]
					+ SOverlay::Slot().Padding(23.f * U)
					[
						SNew(SOverlay)
						+ SOverlay::Slot()[SNew(SImage).Image(Glyph(G + TEXT("_a"))).ColorAndOpacity(State == ENodeState::Locked ? Hex(TEXT("8A6A45")) : L.Ac)]
						+ SOverlay::Slot()[SNew(SImage).Image(Glyph(G + TEXT("_p"))).ColorAndOpacity(State == ENodeState::Locked ? Hex(TEXT("C8763C")) : L.P)]
					]
					+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 6.f * U, 6.f * U)
					[
						SNew(SBox).WidthOverride(26.f * U).HeightOverride(26.f * U).Visibility(State == ENodeState::Locked || bMaxed ? EVisibility::HitTestInvisible : EVisibility::Collapsed)
						[
							SNew(SOverlay)
							+ SOverlay::Slot()[SNew(SImage).Image(bMaxed ? WRUI::Box(Hex(TEXT("C99E5A")), 13.f * U, Hex(TEXT("2A1D12")), 1.f) : WRUI::Box(Hex(TEXT("0D0E10")), 13.f * U, Hex(TEXT("5A5F66")), 1.f))]
							+ SOverlay::Slot().Padding(6.f * U)[SNew(SImage).Image(Glyph(bMaxed ? TEXT("check") : TEXT("lock"))).ColorAndOpacity(bMaxed ? Hex(TEXT("2A1D12")) : Hex(TEXT("B9AD94")))]
						]
					]
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(12.f * U, 0.f, 0.f, 0.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(N->Name)).Font(WRUI::Title(TitleSize)).ColorAndOpacity(WRUI::Ink).AutoWrapText(true)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 5.f * U, 0.f, 0.f)
				[
					WRUI::Text(FText::FromString(FString::Printf(TEXT("%s · %s"), *Model->KindName(*N), *Model->BranchName(N->Branch))), WRUI::Caps(13), C.Main)
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left).Padding(0.f, 6.f * U, 0.f, 0.f)[SNew(SWRChip).Text(ChipText).Color(ChipColor).FontSize(11)]
			]
		];

		// What it takes: cost, the branch gate and how far along you are.
		{
			const int32 Have = N->Branch.IsNone() ? 0 : Model->BranchSpent(N->Branch);
			const FString CostText = N->bStarter ? FString(TEXT("Free: the path's starter spell"))
				: bRanked ? FString::Printf(TEXT("%d point%s per rank"), N->Cost, N->Cost == 1 ? TEXT("") : TEXT("s")) : FString::Printf(TEXT("%d points"), N->Cost);
			TSharedRef<SWrapBox> Line = SNew(SWrapBox).UseAllottedSize(true);
			Line->AddSlot().Padding(0.f, 0.f, 5.f * U, 0.f)[WRUI::Text(FText::FromString(CostText), WRUI::Body(14, 800), WRUI::Ink)];
			if (!N->Branch.IsNone() && N->Gate > 0)
			{
				Line->AddSlot().Padding(0.f, 0.f, 5.f * U, 0.f)[WRUI::Text(FText::FromString(FString::Printf(TEXT("· needs %d in %s"), N->Gate, *Model->BranchName(N->Branch))), WRUI::Body(14, 500), WRUI::Ink2)];
				Line->AddSlot()[WRUI::Text(FText::FromString(FString::Printf(TEXT("(you have %d)"), Have)), WRUI::Body(14, 500), WRUI::Ink3)];
			}
			else if (N->Branch.IsNone())
			{
				Line->AddSlot()[WRUI::Text(LOCTEXT("TrunkReq", "· trunk: open to every build"), WRUI::Body(14, 500), WRUI::Ink2)];
			}
			else
			{
				Line->AddSlot()[WRUI::Text(LOCTEXT("BranchReq", "· opens once the trunk is done"), WRUI::Body(14, 500), WRUI::Ink2)];
			}
			// The bar: ranks for a stat, the branch gate for a gated node, trunk progress for the first of a branch.
			float Frac = 1.f;
			FString BarText;
			if (bRanked) { Frac = float(Rank) / FMath::Max(1, N->MaxRank); BarText = FString::Printf(TEXT("%d / %d"), Rank, N->MaxRank); }
			else if (!N->Branch.IsNone() && N->Gate > 0) { Frac = float(Have) / N->Gate; BarText = FString::Printf(TEXT("%d / %d"), FMath::Min(Have, N->Gate), N->Gate); }
			else if (Rank > 0) { BarText = TEXT("1 / 1"); }
			else { Frac = 0.f; BarText = TEXT("0 / 1"); }
			Box->AddSlot().AutoHeight().Padding(0.f, 16.f * U, 0.f, 0.f)
			[
				SNew(SWRPanel).Kind(EWRPanel::Inset).Padding(FMargin(14.f * U, 10.f * U))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[Line]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f * U, 0.f, 0.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[SNew(SWRBar).Height(8.f * U).Fraction(FMath::Clamp(Frac, 0.f, 1.f)).Color(C.Main)]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f * U, 0.f, 0.f, 0.f)[WRUI::Text(FText::FromString(BarText), WRUI::Body(12, 700), WRUI::Ink2)]
					]
				]
			];
		}

		Box->AddSlot().AutoHeight().Padding(0.f, 14.f * U, 0.f, 0.f)
		[
			SNew(STextBlock).Text(FText::FromString(N->Text.Len() > 0 ? Capitalised(N->Text) + TEXT(".") : FString())).Font(WRUI::Body(15, 500)).ColorAndOpacity(WRUI::Ink).AutoWrapText(true)
		];

		// Numbers from the game: what a cast costs and does, or what each rank adds.
		TArray<TSharedRef<SWidget>> Rows;
		const bool bFrost = Model->Path == TEXT("frost");
		if (N->Kind == EWRNodeKind::Stat)
		{
			const TSharedPtr<FJsonObject>* Effects = nullptr;
			const TSharedPtr<FJsonObject>* Stats = nullptr;
			if (TuningFile().IsValid() && TuningFile()->TryGetObjectField(TEXT("nodeEffects"), Effects) && (*Effects)->TryGetObjectField(Model->Path.ToString() + TEXT(".") + N->Id.ToString(), Stats))
			{
				for (const TPair<FString, TSharedPtr<FJsonValue>>& S : (*Stats)->Values)
				{
					const float Per = static_cast<float>(S.Value->AsNumber());
					const bool bPct = FMath::Abs(Per) < 1.f;
					Rows.Add(StatRow(TEXT("diamond"), C.Main, FText::Format(LOCTEXT("PerRank", "{0}, per rank"), FText::FromString(Humanize(S.Key))), FText::FromString(Signed(Per, bPct)), C.Main, false));
					Rows.Add(StatRow(TEXT("diamond_line"), WRUI::Ink3, FText::Format(LOCTEXT("NowRank", "Now (rank {0})"), Rank), FText::FromString(Signed(Per * Rank, bPct)), WRUI::Ink, false));
				}
			}
			else
			{
				Rows.Add(StatRow(TEXT("diamond_line"), WRUI::Ink3, LOCTEXT("StatRanks", "Ranks"), FText::Format(LOCTEXT("RanksOf", "{0} of {1}"), Rank, N->MaxRank), WRUI::Ink, false));
			}
		}
		else if (const TSubclassOf<UGameplayAbility> Class = UWRAbilityRegistry::ClassForNode(Model->Path, N->Id))
		{
			const FString Key = CamelKey(N->Id.ToString());
			const TSharedPtr<FJsonObject>* Spells = nullptr;
			const TSharedPtr<FJsonObject>* Spell = nullptr;
			const bool bHas = TuningFile().IsValid() && TuningFile()->TryGetObjectField(TEXT("spells"), Spells) && (*Spells)->TryGetObjectField(Key, Spell);
			const auto Val = [&](const TCHAR* Field, float& Out) { double V = 0.0; if (!bHas || !(*Spell)->TryGetNumberField(Field, V)) return false; Out = FWRTuning::Get().Num(FString::Printf(TEXT("spells.%s.%s"), *Key, Field), static_cast<float>(V)); return true; };
			float V = 0.f;
			if (Val(bFrost ? TEXT("chill") : TEXT("heat"), V))
			{
				Rows.Add(StatRow(bFrost ? TEXT("snowflake") : TEXT("flame"), C.Main, bFrost ? LOCTEXT("BuildsChill", "Builds Chill") : LOCTEXT("BuildsHeat", "Builds Heat"), FText::FromString(TEXT("+") + Num(V)), C.Main, false));
			}
			const UWRGameplayAbility* CDO = Cast<UWRGameplayAbility>(Class->GetDefaultObject());
			if (CDO && CDO->GetCooldownSeconds() > 0.f)
			{
				Rows.Add(StatRow(TEXT("path_long_burn_a"), WRUI::Ink2, LOCTEXT("Cooldown", "Cooldown"), FText::FromString(Num(CDO->GetCooldownSeconds()) + TEXT(" s")), WRUI::Ink, false));
			}
			float Dps = 0.f, Secs = 0.f, Mult = 0.f;
			if (Val(TEXT("damage"), V)) Rows.Add(StatRow(TEXT("diamond"), WRUI::Ink3, LOCTEXT("Damage", "Damage"), FText::FromString(Num(V)), WRUI::Ink, false));
			else if (Val(TEXT("dps"), Dps) && Val(TEXT("seconds"), Secs)) Rows.Add(StatRow(TEXT("diamond"), WRUI::Ink3, LOCTEXT("Burns", "Damage over time"), FText::FromString(FString::Printf(TEXT("%s/s for %s s"), *Num(Dps), *Num(Secs))), WRUI::Ink, false));
			else if (Val(TEXT("multiplier"), Mult)) Rows.Add(StatRow(TEXT("diamond"), WRUI::Ink3, LOCTEXT("WeaponDmg", "Weapon damage"), FText::FromString(TEXT("x") + Num(Mult)), WRUI::Ink, false));
		}
		else if (UWRAbilityRegistry::IsBuilt(Model->Path, N->Id))
		{
			Rows.Add(StatRow(TEXT("diamond_line"), WRUI::Ink3, LOCTEXT("Passive", "Passive"), LOCTEXT("PassiveVal", "always on once learned"), WRUI::Ink, false));
		}
		else
		{
			Rows.Add(StatRow(TEXT("diamond_line"), WRUI::Warn, LOCTEXT("NotBuilt", "In the game"), LOCTEXT("NotBuiltVal", "not yet: coming later"), WRUI::Warn, false));
		}
		if (Rows.Num() > 0)
		{
			TSharedRef<SVerticalBox> Table = SNew(SVerticalBox);
			for (int32 i = 0; i < Rows.Num(); ++i) Table->AddSlot().AutoHeight()[Rows[i]];
			Box->AddSlot().AutoHeight().Padding(0.f, 14.f * U, 0.f, 0.f)[SNew(SWRPanel).Kind(EWRPanel::Inset).Padding(FMargin(14.f * U, 2.f * U))[Table]];
		}

		// Learn, and why not.
		Box->AddSlot().FillHeight(1.f)[SNullWidget::NullWidget];
		const FText Button = bMaxed ? LOCTEXT("Learned", "Learned") : bRanked ? FText::Format(LOCTEXT("LearnRank", "Learn rank {0} ({1} pt)"), Rank + 1, N->Cost) : FText::Format(LOCTEXT("LearnPts", "Learn ({0} pts)"), N->Cost);
		const FName NodeId = N->Id;
		// SWRButton's leather tile would size it to 256 units in an auto-height slot: give it its height.
		Box->AddSlot().AutoHeight().Padding(0.f, 10.f * U, 0.f, 0.f)
		[
			SNew(SBox).HeightOverride(44.f * U)
			[
				SNew(SWRButton).Text(Button).Primary(Why.IsEmpty()).FontSize(15).IsEnabled(Why.IsEmpty())
				.OnClicked_Lambda([this, NodeId]() { Learn(NodeId); return FReply::Handled(); })
			]
		];
		if (!bMaxed)
		{
			TArray<FString> Reasons;
			if (!Model->IsActive()) Reasons.Add(TEXT("Not your main path or subclass"));
			else
			{
				if (!N->Branch.IsNone() && !Model->TrunkDone()) Reasons.Add(TEXT("Finish the trunk first"));
				if (!N->Branch.IsNone() && Model->BranchSpent(N->Branch) < N->Gate) Reasons.Add(FString::Printf(TEXT("Needs %d points in %s (you have %d)"), N->Gate, *Model->BranchName(N->Branch), Model->BranchSpent(N->Branch)));
				if (Points < N->Cost) Reasons.Add(FString::Printf(TEXT("Needs %d point%s to spend (you have %d)"), N->Cost, N->Cost == 1 ? TEXT("") : TEXT("s"), Points));
				if (Reasons.Num() == 0 && !Why.IsEmpty()) Reasons.Add(Capitalised(Why));
			}
			for (const FString& R2 : Reasons)
			{
				Box->AddSlot().AutoHeight().Padding(0.f, 5.f * U, 0.f, 0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 7.f * U, 0.f)
					[
						SNew(SBox).WidthOverride(11.f * U).HeightOverride(11.f * U)
						[SNew(SImage).Image(Glyph(TEXT("plus"))).ColorAndOpacity(WRUI::Bad).RenderTransform(FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(45.f)))).RenderTransformPivot(FVector2D(0.5f, 0.5f))]
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[WRUI::Text(FText::FromString(R2), WRUI::Body(12, 500), WRUI::Bad)]
				];
			}
			if (Why.IsEmpty() && Model->IsActive())
			{
				Box->AddSlot().AutoHeight().Padding(0.f, 5.f * U, 0.f, 0.f)[WRUI::Text(LOCTEXT("DoubleClick", "Or double-click the node."), WRUI::Body(12, 500), WRUI::Ink3)];
			}
		}
		return Box;
	}

	// ---------------------------------------------------------------- the two ability wheels

	TSharedRef<SWidget> WheelRow(int32 Wheel)
	{
		TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
		Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f * U, 0.f)
		[
			SNew(SBox).WidthOverride(26.f * U).HAlign(HAlign_Center)
			[
				SNew(SWRKeyCap).FontSize(11).Text_Lambda([Wheel]() { return WRUI::KeyName(FWRKeyBindings::Get().Key(Wheel == 0 ? TEXT("Wheel1") : TEXT("Wheel2"))); })
			]
		];
		for (int32 i = 0; i < 8; ++i)
		{
			TSharedRef<SWRWheelSlot> S = SNew(SWRWheelSlot).PC(PC).Wheel(Wheel).Pos(i);
			S->GetSelected = [this]() { return SelectedCastable(); };
			Row->AddSlot().AutoWidth().Padding(i > 0 ? 3.f * U : 0.f, 0.f, 0.f, 0.f)[S];
		}
		return Row;
	}

	/** "Wheel 1 · slot 3" for the shown ability, or where it would go. */
	FText WhereOnWheel() const
	{
		const FWRPathNodeDef* N = Model->Node(Shown());
		const FWRPlayerUI* UI = PC.IsValid() ? PC->GetUI() : nullptr;
		if (!N || !UI) return FText::GetEmpty();
		if (!Model->Castable(*N)) return N->Kind == EWRNodeKind::Stat ? LOCTEXT("NotCastStat", "A stat: always on") : LOCTEXT("NotCastPassive", "Passive: not on a wheel");
		if (Model->Rank(N->Id) <= 0) return LOCTEXT("CastOnceLearned", "Goes on a wheel once learned");
		const FName Id = Model->WheelId(N->Id);
		for (int32 W = 0; W < 2; ++W) for (int32 i = 0; i < 8; ++i) if (UI->GetWheelAbility(W, i) == Id) return FText::Format(LOCTEXT("WheelSlot", "Wheel {0} · slot {1}"), W + 1, i + 1);
		return LOCTEXT("NotOnWheel", "Not on a wheel: drag it to one");
	}

	TSharedRef<SWidget> Loadout()
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f * U, 0.f, 10.f * U)[WRUI::Divider()]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[WRUI::Text(LOCTEXT("Wheels", "Ability wheels"), WRUI::Caps(13), WRUI::Ink2)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return WhereOnWheel(); }), WRUI::Body(12, 700), WRUI::Brass)
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f * U, 0.f, 8.f * U)
			[
				WRUI::Text(LOCTEXT("WheelsHelp", "Drag a learned ability onto a slot, or select it and click one."), WRUI::Body(11, 500), WRUI::Ink3)
			]
			+ SVerticalBox::Slot().AutoHeight()[WheelRow(0)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 5.f * U, 0.f, 0.f)[WheelRow(1)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 9.f * U, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return TapLine(); }), WRUI::Body(11, 500), WRUI::Ink3)
				]
				// The ability's own key, if you gave it one in Settings (rebinding lives there).
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBorder).BorderImage(WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.4f), 13.f * U, WRUI::Brass2, 1.f)).Padding(FMargin(10.f * U, 2.f * U, 4.f * U, 2.f * U))
					.Visibility_Lambda([this]() { const FWRPathNodeDef* N = Model->Node(Shown()); return N && Model->Castable(*N) ? EVisibility::Visible : EVisibility::Collapsed; })
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f * U, 0.f)[WRUI::Text(LOCTEXT("OwnKey", "Own key:"), WRUI::Body(12, 700), WRUI::Ink)]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(SWRKeyCap).FontSize(10).Text_Lambda([this]() { return WRUI::KeyName(FWRKeyBindings::Get().Key(FWRKeyBindings::AbilityAction(Model->WheelId(Shown())))); })
						]
					]
				]
			];
	}

	FText TapLine() const
	{
		const FWRPlayerUI* UI = PC.IsValid() ? PC->GetUI() : nullptr;
		if (!UI) return FText::GetEmpty();
		const TArray<FWRCastable> All = UI->GetCastables();
		FString Line;
		for (int32 W = 0; W < 2; ++W)
		{
			const FWRCastable* C = UI->FindCastable(UI->GetWheelAbility(W, UI->GetWheelPicked(W)), All);
			const FString Key = WRUI::KeyName(FWRKeyBindings::Get().Key(W == 0 ? TEXT("Wheel1") : TEXT("Wheel2"))).ToString();
			if (!Line.IsEmpty()) Line += TEXT("  ·  ");
			Line += FString::Printf(TEXT("Tap %s: %s"), *Key, C ? *C->Name.ToString() : TEXT("nothing"));
		}
		return FText::FromString(Line);
	}
};

} // namespace WRPathsTab

WR_REGISTER_JOURNAL_TAB("paths", [](AWRPlayerController* PC) -> TSharedRef<SWidget> { return SNew(WRPathsTab::SWRPathsTab).PC(PC); });

#undef LOCTEXT_NAMESPACE
