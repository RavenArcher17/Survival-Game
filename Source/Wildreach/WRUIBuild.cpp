// The build bar (mockup "Building"), shown while the controller is in EWRMenu::Build: piece categories, piece cards
// with their cost against your bag, and the picked piece's details (cost, upkeep in a claim). Clicking a card starts
// placing it the way the old B menu did (the controller's ghost); while a piece is out, the bar stays up and the keys
// to place, rotate, cancel and toggle snap sit above it, a chip over the ghost says whether it will go (and why not),
// and the claim you stand in shows top right. The screen keeps movement; the UI hides the cursor while you place.

#include "WRPlayerUI.h"
#include "WRUIStyle.h"
#include "WRUIWidgets.h"
#include "WRBuilding.h"
#include "WRInventory.h"
#include "WRKeyBindings.h"
#include "WRLifeSkills.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRSocial.h"
#include "Algo/StableSort.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"

#define LOCTEXT_NAMESPACE "WRUI"

// A named namespace, not an anonymous one: unity builds put several UI files in one translation unit.
namespace WRBuildUI
{
	using WRUI::U;   // mockup px -> 1080p units
	/** The bar starts right of the HUD's vitals panel (24 + 360 + a gap, in mockup px). */
	constexpr float BarLeft = 398.f;
	constexpr float BarHeight = 240.f;

	struct FTab { FName Id; FText Name; };
	const TArray<FTab>& Tabs()
	{
		static const TArray<FTab> List = {
			{ TEXT("foundations"), LOCTEXT("TabFoundations", "Foundations") },
			{ TEXT("walls"), LOCTEXT("TabWalls", "Walls") },
			{ TEXT("floors"), LOCTEXT("TabFloors", "Floors & roofs") },
			{ TEXT("doors"), LOCTEXT("TabDoors", "Doors & windows") },
			{ TEXT("stairs"), LOCTEXT("TabStairs", "Stairs") },
			{ TEXT("stations"), LOCTEXT("TabStations", "Stations") },
			{ TEXT("storage"), LOCTEXT("TabStorage", "Storage & beds") },
			{ TEXT("defense"), LOCTEXT("TabDefense", "Defense") },
			{ TEXT("light"), LOCTEXT("TabLight", "Light") },
			{ TEXT("decor"), LOCTEXT("TabDecor", "Decor") },
		};
		return List;
	}

	FName TabOf(const FWRPieceDef& D)
	{
		const FName S = D.Shape, K = D.Kind;
		if (D.Category == TEXT("defense")) return TEXT("defense");
		if (S == TEXT("foundation") || S == TEXT("pillar")) return TEXT("foundations");
		if (S == TEXT("wall") || S == TEXT("halfwall") || S == TEXT("upperwall")) return TEXT("walls");
		if (S == TEXT("floor") || S == TEXT("roof") || S == TEXT("gable")) return TEXT("floors");
		if (S == TEXT("door") || S == TEXT("doorway") || S == TEXT("window")) return TEXT("doors");
		if (S == TEXT("stairs")) return TEXT("stairs");
		if (K == TEXT("station") || K == TEXT("hearth") || K == TEXT("totem") || K == TEXT("crop") || K == TEXT("pen")) return TEXT("stations");
		if (K == TEXT("chest") || K == TEXT("bed")) return TEXT("storage");
		if (K == TEXT("light")) return TEXT("light");
		return TEXT("decor");
	}

	FString ItemName(FName Id)
	{
		const FWRItemDef* Def = FWRItemRegistry::Get().Find(Id);
		return Def ? Def->Name.ToLower() : Id.ToString().Replace(TEXT("_"), TEXT(" "));
	}

	/** What a piece is for, true to the building rules (one or two short sentences). */
	FString Describe(const FWRPieceDef& D)
	{
		const FName S = D.Shape, K = D.Kind;
		const FString Span = FString::Printf(TEXT(" Needs support: %s spans %d pieces from the ground."), *D.Material.ToString(), FWRBuildRules::SpanFor(D.Material));
		if (S == TEXT("foundation")) return TEXT("The base for walls, floors and stairs. Lines up with the foundations beside it.");
		if (S == TEXT("pillar")) return TEXT("Stands on a cell corner and carries what's stacked on it at no span cost.");
		if (S == TEXT("wall")) return TEXT("Stands on a cell edge: on a foundation, a floor or another wall.") + Span;
		if (S == TEXT("halfwall")) return TEXT("A waist-high wall on a cell edge.") + Span;
		if (S == TEXT("upperwall")) return TEXT("Fills the gap under a roof's high side.") + Span;
		if (S == TEXT("floor")) return TEXT("A floor, or the ceiling of the storey below.") + Span;
		if (S == TEXT("roof")) return TEXT("A sloped roof over a cell; R turns which way it rises.") + Span;
		if (S == TEXT("gable")) return TEXT("Closes a roof's open side; R flips which end is high.") + Span;
		if (S == TEXT("stairs")) return TEXT("Climbs one storey across a cell; the floor above must leave its opening.") + Span;
		if (S == TEXT("door") && D.bLockable) return TEXT("A doorway with a heavy door that takes a 4-digit code lock.");
		if (S == TEXT("door")) return TEXT("A doorway with a door: E opens and closes it.");
		if (S == TEXT("doorway")) return TEXT("A wall with an opening and no door, between rooms.");
		if (S == TEXT("window")) return TEXT("A wall with a window in it.") + Span;
		if (S == TEXT("palisade")) return TEXT("A tall log wall that stands on the ground on its own.");
		if (S == TEXT("gate")) return TEXT("A wide gate for a palisade: E opens it.");
		if (S == TEXT("tower")) return TEXT("A guard tower with a lookout platform on top.");
		if (K == TEXT("station")) return FString::Printf(TEXT("A crafting station (%s recipes). Its tier follows the Hearth in your claim."), *D.Name.ToLower());
		if (K == TEXT("hearth")) return TEXT("Goes inside your own land claim, one per claim. Feed it boss cores to raise every station's tier.");
		if (K == TEXT("totem")) return TEXT("Claims the land around it: only you and your clan build there. Stock it with wood and stone for upkeep.");
		if (K == TEXT("chest")) return TEXT("Stores what doesn't fit in your bag. Only you and your clan can open it.");
		if (K == TEXT("bed")) return TEXT("Where you wake up after you die.");
		if (K == TEXT("light")) return TEXT("Light for the night.");
		if (K == TEXT("crop")) return TEXT("Plant seeds in it (E) and harvest food when it has grown.");
		if (K == TEXT("pen")) return TEXT("Keep a tamed Pumicehog or Snowpuff inside and it produces over time.");
		return TEXT("Decoration. Places freely, no grid.");
	}

	// ---------------------------------------------------------------- piece pictures

	/** A little isometric drawing of a piece from blocks (sizes from AWRBuildPiece::ApplyLook), for the cards. */
	class SPieceArt : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SPieceArt) : _Size(FVector2D(120.f, 72.f)) {}
			SLATE_ARGUMENT(FName, PieceId)
			SLATE_ARGUMENT(FVector2D, Size)
		SLATE_END_ARGS()

		struct FBlock { FVector Size; FVector Centre; float Pitch = 0.f; FLinearColor Color; };

		void Construct(const FArguments& InArgs)
		{
			Size = InArgs._Size;
			if (const FWRPieceDef* D = FWRBuildRegistry::Get().Find(InArgs._PieceId)) Blocks = BlocksFor(*D);
		}
		virtual FVector2D ComputeDesiredSize(float) const override { return Size; }

		static TArray<FBlock> BlocksFor(const FWRPieceDef& D)
		{
			const auto C = [](uint8 R, uint8 G, uint8 B) { return FLinearColor(FColor(R, G, B)); };
			const FLinearColor Wood = C(0xA8, 0x74, 0x3F), Stone = C(0x8B, 0x8F, 0x94), Dark = C(0x6E, 0x45, 0x24), Cloth = C(0xC9, 0xB4, 0x8A);
			const FLinearColor Red = C(0x9A, 0x2E, 0x22), Ember = C(0xEC, 0x7A, 0x34), Soil = C(0x4A, 0x30, 0x20), Green = C(0x5F, 0x9E, 0x3E);
			const FLinearColor M = D.Material == TEXT("stone") ? Stone : Wood;
			const FName S = D.Shape, K = D.Kind;
			TArray<FBlock> B;
			const auto Add = [&B](FVector Sz, FVector Ce, const FLinearColor& Col, float Pitch = 0.f) { B.Add({ Sz, Ce, Pitch, Col }); };
			const auto Frame = [&](float Len, float Opening, float Height, bool bLeaf)
			{
				const float Side = (Len - Opening) * 0.5f;
				Add(FVector(Side, 20, Height), FVector(-Len * 0.5f + Side * 0.5f, 0, Height * 0.5f), M);
				Add(FVector(Side, 20, Height), FVector(Len * 0.5f - Side * 0.5f, 0, Height * 0.5f), M);
				Add(FVector(Opening, 20, Height - 240), FVector(0, 0, 240 + (Height - 240) * 0.5f), M);
				if (bLeaf) Add(FVector(Opening, 10, 240), FVector(0, 0, 120), M * 0.7f);
			};
			if (S == TEXT("foundation")) Add(FVector(400, 400, 80), FVector(0, 0, 40), M);
			else if (S == TEXT("wall")) Add(FVector(420, 20, 300), FVector(0, 0, 150), M);
			else if (S == TEXT("halfwall")) Add(FVector(420, 20, 150), FVector(0, 0, 75), M);
			else if (S == TEXT("upperwall")) Add(FVector(420, 20, 210), FVector(0, 0, 105), M);
			else if (S == TEXT("floor")) Add(FVector(400, 400, 20), FVector(0, 0, 10), M);
			else if (S == TEXT("roof"))
			{
				const float Slope = FMath::Atan2(200.f, 400.f);
				Add(FVector(450.f / FMath::Cos(Slope), 420, 15), FVector(-15, 0, 100), M, -FMath::RadiansToDegrees(Slope));
			}
			else if (S == TEXT("gable"))
			{
				// The triangle as steps.
				for (int32 i = 0; i < 6; ++i) Add(FVector(420.f - i * 70.f, 20, 35), FVector(i * 35.f, 0, 17.5f + i * 35.f), M);
			}
			else if (S == TEXT("pillar")) Add(FVector(40, 40, 300), FVector(0, 0, 150), M);
			else if (S == TEXT("stairs")) { for (int32 k = 1; k <= 8; ++k) Add(FVector(50, 200, 37.5f * k), FVector(-225 + 50 * k, 0, 18.75f * k), M); }
			else if (S == TEXT("window"))
			{
				Add(FVector(420, 20, 100), FVector(0, 0, 50), M);
				Add(FVector(420, 20, 80), FVector(0, 0, 260), M);
				Add(FVector(130, 20, 120), FVector(-145, 0, 160), M);
				Add(FVector(130, 20, 120), FVector(145, 0, 160), M);
			}
			else if (S == TEXT("door")) Frame(420, 140, 300, true);
			else if (S == TEXT("doorway")) Frame(420, 140, 300, false);
			else if (S == TEXT("gate")) Frame(440, 300, 400, true);
			else if (S == TEXT("palisade")) { for (int32 i = 0; i < 6; ++i) Add(FVector(70, 40, 380 + (i % 2) * 30), FVector(-185 + i * 74, 0, 190 + (i % 2) * 15), M); }
			else if (S == TEXT("tower"))
			{
				Add(FVector(200, 200, 500), FVector(0, 0, 250), M);
				Add(FVector(400, 400, 20), FVector(0, 0, 510), M * 0.85f);
			}
			else if (K == TEXT("chest")) { Add(FVector(100, 60, 50), FVector(0, 0, 25), Wood); Add(FVector(104, 64, 12), FVector(0, 0, 56), Dark); }
			else if (K == TEXT("bed"))
			{
				if (D.Id == TEXT("bedroll")) Add(FVector(200, 80, 15), FVector(0, 0, 8), Cloth);
				else { Add(FVector(220, 100, 35), FVector(0, 0, 18), Wood); Add(FVector(200, 90, 15), FVector(5, 0, 43), Cloth); }
			}
			else if (K == TEXT("light"))
			{
				if (D.Id == TEXT("brazier")) { Add(FVector(60, 60, 70), FVector(0, 0, 35), Stone); Add(FVector(40, 40, 25), FVector(0, 0, 82), Ember); }
				else { Add(FVector(12, 12, 150), FVector(0, 0, 75), Dark); Add(FVector(20, 20, 26), FVector(0, 0, 160), Ember); }
			}
			else if (K == TEXT("crop"))
			{
				Add(FVector(400, 400, 30), FVector(0, 0, 15), Soil);
				for (int32 i = 0; i < 4; ++i) Add(FVector(50, 50, 40), FVector(-110 + (i % 2) * 220, -110 + (i / 2) * 220, 50), Green);
			}
			else if (K == TEXT("pen"))
			{
				Add(FVector(800, 20, 120), FVector(0, -400, 60), Wood);
				Add(FVector(20, 800, 120), FVector(-400, 0, 60), Wood);
				Add(FVector(20, 800, 120), FVector(400, 0, 60), Wood);
				Add(FVector(800, 20, 120), FVector(0, 400, 60), Wood);
			}
			else if (K == TEXT("station"))
			{
				const bool bForge = D.Station == TEXT("forge");
				Add(FVector(120, 80, 80), FVector(0, 0, 40), bForge || D.Station == TEXT("armorer") ? Stone : Wood);
				Add(FVector(126, 86, 12), FVector(0, 0, 86), bForge ? Ember : Dark);
			}
			else if (K == TEXT("hearth")) { Add(FVector(150, 150, 60), FVector(0, 0, 30), Stone); Add(FVector(80, 80, 30), FVector(0, 0, 75), Ember); }
			else if (K == TEXT("totem")) { Add(FVector(40, 40, 250), FVector(0, 0, 125), Wood); Add(FVector(100, 24, 30), FVector(0, 0, 215), Dark); }
			else if (D.Id == TEXT("table"))
			{
				Add(FVector(150, 90, 10), FVector(0, 0, 75), Wood);
				for (int32 i = 0; i < 4; ++i) Add(FVector(10, 10, 70), FVector(i % 2 ? 65 : -65, i / 2 ? 35 : -35, 35), Dark);
			}
			else if (D.Id == TEXT("chair"))
			{
				Add(FVector(50, 50, 8), FVector(0, 0, 45), Wood);
				Add(FVector(8, 50, 50), FVector(-21, 0, 74), Wood);
				for (int32 i = 0; i < 4; ++i) Add(FVector(6, 6, 42), FVector(i % 2 ? 20 : -20, i / 2 ? 20 : -20, 21), Dark);
			}
			else if (D.Id == TEXT("rug")) Add(FVector(200, 150, 4), FVector(0, 0, 2), Red);
			else if (D.Id == TEXT("banner")) { Add(FVector(8, 8, 250), FVector(0, 0, 125), Dark); Add(FVector(6, 70, 150), FVector(5, 0, 160), Red); }
			else Add(FVector(80, 20, 80), FVector(0, 0, 40), Wood);
			return B;
		}

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override
		{
			if (Blocks.Num() == 0) return Layer;
			// A camera from the front-right corner, 30 degrees up: right, up and toward-the-viewer axes.
			const FVector V = FVector(0.612f, 0.612f, 0.5f).GetSafeNormal();
			const FVector R(-0.7071f, 0.7071f, 0.f);
			const FVector Up = FVector::CrossProduct(V, R).GetSafeNormal() * -1.f;
			const FVector Light = FVector(0.5f, 0.2f, 1.f).GetSafeNormal();
			struct FFace { FVector P[4]; FVector N; float Depth; FLinearColor Color; };
			TArray<FFace> Faces;
			FBox2D Bounds(ForceInit);
			static const int32 Quads[6][4] = { { 0, 1, 3, 2 }, { 4, 6, 7, 5 }, { 0, 4, 5, 1 }, { 2, 3, 7, 6 }, { 0, 2, 6, 4 }, { 1, 5, 7, 3 } };
			static const FVector Normals[6] = { FVector(-1, 0, 0), FVector(1, 0, 0), FVector(0, -1, 0), FVector(0, 1, 0), FVector(0, 0, -1), FVector(0, 0, 1) };
			for (const FBlock& B : Blocks)
			{
				const FQuat Rot = FRotator(B.Pitch, 0.f, 0.f).Quaternion();
				FVector Corner[8];
				for (int32 i = 0; i < 8; ++i)
				{
					const FVector Local((i & 4 ? 0.5f : -0.5f) * B.Size.X, (i & 2 ? 0.5f : -0.5f) * B.Size.Y, (i & 1 ? 0.5f : -0.5f) * B.Size.Z);
					Corner[i] = B.Centre + Rot.RotateVector(Local);
					Bounds += FVector2D(FVector::DotProduct(Corner[i], R), -FVector::DotProduct(Corner[i], Up));
				}
				for (int32 f = 0; f < 6; ++f)
				{
					const FVector N = Rot.RotateVector(Normals[f]);
					if (FVector::DotProduct(N, V) <= 0.01f) continue;
					FFace Face;
					FVector Mid = FVector::ZeroVector;
					for (int32 k = 0; k < 4; ++k) { Face.P[k] = Corner[Quads[f][k]]; Mid += Face.P[k] * 0.25f; }
					Face.N = N;
					Face.Depth = FVector::DotProduct(Mid, V);
					const float Shade = 0.55f + 0.45f * FMath::Max(0.f, FVector::DotProduct(N, Light));
					Face.Color = FLinearColor(B.Color.R * Shade, B.Color.G * Shade, B.Color.B * Shade, 1.f);
					Faces.Add(Face);
				}
			}
			Faces.Sort([](const FFace& A, const FFace& B) { return A.Depth < B.Depth; });
			const FVector2D S = Geo.GetLocalSize();
			const FVector2D Ext = Bounds.GetSize();
			const float Scale = FMath::Min(S.X * 0.9f / FMath::Max(1.f, Ext.X), S.Y * 0.82f / FMath::Max(1.f, Ext.Y));
			const FVector2D Origin = FVector2D(S.X * 0.5f, S.Y * 0.46f) - Bounds.GetCenter() * Scale;
			const auto Project = [&](const FVector& P) { return Origin + FVector2D(FVector::DotProduct(P, R), -FVector::DotProduct(P, Up)) * Scale; };

			// The shadow it casts, then each face back to front with a dark edge.
			const FVector2D ShadowSize(Ext.X * Scale * 0.86f, FMath::Max(6.f, S.Y * 0.16f));
			WRUI::DrawBox(Out, Layer, Geo.ToPaintGeometry(ShadowSize, FSlateLayoutTransform(FVector2D((S.X - ShadowSize.X) * 0.5f, Origin.Y + Bounds.Max.Y * Scale - ShadowSize.Y * 0.85f))),
				WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.35f), ShadowSize.Y * 0.5f));
			const FSlateResourceHandle Handle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*WRUI::White());
			const FSlateRenderTransform& Xf = Geo.GetAccumulatedRenderTransform();
			int32 L = Layer + 1;
			for (const FFace& F : Faces)
			{
				TArray<FSlateVertex> Verts;
				TArray<FVector2D> Edge;
				const FColor Col = F.Color.ToFColor(true);
				for (int32 k = 0; k < 4; ++k)
				{
					const FVector2D P = Project(F.P[k]);
					Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Xf, FVector2f(P), FVector2f(0.f, 0.f), Col));
					Edge.Add(P);
				}
				const FVector2D Closing = Edge[0];   // (not Edge[0] itself: Add may reallocate under it)
				Edge.Add(Closing);
				const TArray<SlateIndex> Index = { 0, 1, 2, 0, 2, 3 };
				FSlateDrawElement::MakeCustomVerts(Out, L, Handle, Verts, Index, nullptr, 0, 0);
				FSlateDrawElement::MakeLines(Out, L + 1, Geo.ToPaintGeometry(), Edge, ESlateDrawEffect::None, FLinearColor(0.03f, 0.02f, 0.01f, 0.75f), true, 1.f);
				L += 2;
			}
			return L;
		}

	private:
		FVector2D Size;
		TArray<FBlock> Blocks;
	};

	/** Small drawn marks: a filled disc with a check or a cross (the ghost chip), the hammer, the claim flag. */
	class SMark : public SLeafWidget
	{
	public:
		enum class EKind : uint8 { Yes, No, Hammer, HammerRed, Flag };
		SLATE_BEGIN_ARGS(SMark) : _Kind(EKind::Yes), _Size(22.f), _Color(WRUI::Good) {}
			SLATE_ATTRIBUTE(EKind, Kind)
			SLATE_ARGUMENT(float, Size)
			SLATE_ATTRIBUTE(FLinearColor, Color)
		SLATE_END_ARGS()
		void Construct(const FArguments& InArgs) { Kind = InArgs._Kind; Size = InArgs._Size; Color = InArgs._Color; }
		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(Size, Size); }
		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override
		{
			const float K = Geo.GetLocalSize().X / 24.f;
			const auto P = [K](float X, float Y) { return FVector2D(X * K, Y * K); };
			const auto Line = [&](TArray<FVector2D> Pts, const FLinearColor& C, float W, int32 L) { FSlateDrawElement::MakeLines(Out, L, Geo.ToPaintGeometry(), Pts, ESlateDrawEffect::None, C, true, W * K); };
			const auto Fill = [&](TArray<FVector2D> Pts, const FLinearColor& C, int32 L)
			{
				const FSlateResourceHandle Handle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*WRUI::White());
				TArray<FSlateVertex> Verts;
				TArray<SlateIndex> Index;
				for (const FVector2D& X : Pts) Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Geo.GetAccumulatedRenderTransform(), FVector2f(X), FVector2f(0.f, 0.f), C.ToFColor(true)));
				for (int32 i = 1; i + 1 < Pts.Num(); ++i) Index.Append({ 0, SlateIndex(i), SlateIndex(i + 1) });
				FSlateDrawElement::MakeCustomVerts(Out, L, Handle, Verts, Index, nullptr, 0, 0);
				TArray<FVector2D> Edge = Pts;
				Edge.Add(Pts[0]);
				Line(Edge, FLinearColor(0.08f, 0.05f, 0.03f), 1.2f, L + 1);
			};
			switch (Kind.Get())
			{
			case EKind::Yes:
				WRUI::DrawBox(Out, Layer, Geo.ToPaintGeometry(P(20, 20), FSlateLayoutTransform(P(2, 2))), WRUI::Box(FLinearColor(FColor(0x4F, 0x8F, 0x3A)), 10.f * K, FLinearColor(FColor(0x0F, 0x1A, 0x0A)), 1.4f * K));
				Line({ P(7.5f, 12.3f), P(10.5f, 15.3f), P(16.5f, 9) }, FLinearColor::White, 2.4f, Layer + 1);
				break;
			case EKind::No:
				WRUI::DrawBox(Out, Layer, Geo.ToPaintGeometry(P(20, 20), FSlateLayoutTransform(P(2, 2))), WRUI::Box(FLinearColor(FColor(0xC8, 0x40, 0x2C)), 10.f * K, FLinearColor(FColor(0x2A, 0x0D, 0x0A)), 1.4f * K));
				Line({ P(8.2f, 8.2f), P(15.8f, 15.8f) }, FLinearColor::White, 2.4f, Layer + 1);
				Line({ P(15.8f, 8.2f), P(8.2f, 15.8f) }, FLinearColor::White, 2.4f, Layer + 1);
				break;
			case EKind::Hammer:
			case EKind::HammerRed:
				Line({ P(4, 20), P(11.5f, 12.5f) }, FLinearColor(FColor(0xC9, 0xA2, 0x7A)), 2.6f, Layer);
				Fill({ P(7.5f, 8), P(11, 4.5f), P(19.5f, 13), P(16, 16.5f) }, Kind.Get() == EKind::Hammer ? WRUI::Brass : WRUI::Bad, Layer + 1);
				break;
			case EKind::Flag:
				Line({ P(7, 22), P(7, 3) }, WRUI::Ink, 2.f, Layer);
				Fill({ P(7.5f, 4), P(18.5f, 4), P(15.5f, 8.5f), P(18.5f, 13), P(7.5f, 13) }, Color.Get(), Layer + 1);
				break;
			}
			return Layer + 3;
		}
	private:
		TAttribute<EKind> Kind;
		float Size = 22.f;
		TAttribute<FLinearColor> Color;
	};
}

// The screen itself, in the same namespace (a using-directive would leak into the unity file).
namespace WRBuildUI
{
class SWRBuildBar : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWRBuildBar) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AWRPlayerController>, PC)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		PC = InArgs._PC;
		if (const FWRPieceDef* D = PC.IsValid() ? FWRBuildRegistry::Get().Find(PC->GetPlacingPiece()) : nullptr)
		{
			Tab = TabOf(*D);
			Picked = D->Id;
		}
		ChildSlot
		[
			SNew(SOverlay).Visibility(EVisibility::SelfHitTestInvisible)
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0.f, 20.f * U, 0.f, 0.f)[Banner()]
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(0.f, 20.f * U, 24.f * U, 0.f)[ClaimStatus()]
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top)[GhostChip()]
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 24.f * U, (24.f + BarHeight + 14.f) * U)[KeysPanel()]
			+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Bottom).Padding(BarLeft * U, 0.f, 24.f * U, 24.f * U)[Bar()]
		];
		Rebuild();
	}

	virtual void Tick(const FGeometry& Geo, const double Time, const float Delta) override
	{
		SCompoundWidget::Tick(Geo, Time, Delta);
		LocalSize = Geo.GetLocalSize();
		if (!PC.IsValid()) return;
		// While placing, the bar follows the piece in hand.
		if (const FWRPieceDef* D = FWRBuildRegistry::Get().Find(PC->GetPlacingPiece()); D && D->Id != Picked)
		{
			Picked = D->Id;
			Tab = TabOf(*D);
			bDirty = true;
		}
		const uint32 Hash = BagHash();
		if (bDirty || Hash != LastHash)
		{
			LastHash = Hash;
			bDirty = false;
			Rebuild();
		}
	}

private:
	TWeakObjectPtr<AWRPlayerController> PC;
	FName Tab = TEXT("foundations");
	FName Picked;
	FName Hovered;
	TSharedPtr<SHorizontalBox> TabStrip;
	TSharedPtr<SScrollBox> Cards;
	TSharedPtr<SBox> Details;
	TSharedPtr<SWidget> Chip;
	FVector2D LocalSize = FVector2D(1920.f, 1080.f);
	bool bDirty = false;
	uint32 LastHash = 0;

	// ---------------------------------------------------------------- state

	const UWRInventoryComponent* Inv() const
	{
		const AWRPlayerState* PS = PC.IsValid() ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
		return PS ? PS->GetInventory() : nullptr;
	}
	int32 Have(FName Item) const { const UWRInventoryComponent* I = Inv(); return I ? I->CountOf(Item) : 0; }
	FString Me() const { const AWRPlayerState* PS = PC.IsValid() ? PC->GetPlayerState<AWRPlayerState>() : nullptr; return PS ? PS->GetPlayerName() : FString(); }

	uint32 BagHash() const
	{
		uint32 H = GetTypeHash(PC.IsValid() && PC->IsPlacing());
		if (const UWRInventoryComponent* I = Inv())
		{
			for (const FWRItemStack& S : I->GetSlots()) H = HashCombine(H, HashCombine(GetTypeHash(S.ItemId), GetTypeHash(S.Count)));
		}
		return H;
	}

	/** What a piece costs you: the server's price (the Thrifty perk makes building cheaper). */
	TArray<TPair<FName, int32>> CostOf(const FWRPieceDef& D) const
	{
		const float Mult = FMath::Max(0.1f, 1.f + UWRLifeSkillsComponent::StatFor(PC.Get(), TEXT("buildCost")));
		TArray<TPair<FName, int32>> Out;
		for (const TPair<FName, int32>& C : D.Cost) Out.Add({ C.Key, FMath::Max(1, FMath::CeilToInt(C.Value * Mult)) });
		return Out;
	}
	/** The open tab's pieces in pieces.json order, kept together by shape (wood and stone foundations, then pillars). */
	TArray<const FWRPieceDef*> InTab() const
	{
		TArray<const FWRPieceDef*> Out;
		TArray<FName> Shapes;
		for (const FWRPieceDef& D : FWRBuildRegistry::Get().All())
		{
			if (TabOf(D) != Tab) continue;
			Out.Add(&D);
			Shapes.AddUnique(D.Shape.IsNone() ? D.Id : D.Shape);
		}
		Algo::StableSortBy(Out, [&Shapes](const FWRPieceDef* D) { return Shapes.IndexOfByKey(D->Shape.IsNone() ? D->Id : D->Shape); });
		return Out;
	}
	int32 Affordable(const FWRPieceDef& D) const
	{
		int32 N = 999;
		for (const TPair<FName, int32>& C : CostOf(D)) N = FMath::Min(N, Have(C.Key) / FMath::Max(1, C.Value));
		return N;
	}
	const FWRPieceDef* Shown() const
	{
		const FWRBuildRegistry& Reg = FWRBuildRegistry::Get();
		if (PC.IsValid() && PC->IsPlacing() && Reg.Find(PC->GetPlacingPiece())) return Reg.Find(PC->GetPlacingPiece());
		if (const FWRPieceDef* H = Reg.Find(Hovered)) return H;
		if (const FWRPieceDef* P = Reg.Find(Picked); P && TabOf(*P) == Tab) return P;
		const TArray<const FWRPieceDef*> Pieces = InTab();
		return Pieces.Num() > 0 ? Pieces[0] : nullptr;
	}
	FKey KeyOf(const TCHAR* Action) const { return FWRKeyBindings::Get().Key(FName(Action)); }

	/** The build ghost the controller moves around (local only). */
	const AWRBuildPiece* Ghost() const
	{
		if (!PC.IsValid() || !PC->IsPlacing()) return nullptr;
		for (TActorIterator<AWRBuildPiece> It(PC->GetWorld()); It; ++It)
		{
			if (It->bGhost) return *It;
		}
		return nullptr;
	}
	const AWRClaimTotem* ClaimHere() const
	{
		const APawn* P = PC.IsValid() ? PC->GetPawn() : nullptr;
		return P ? AWRClaimTotem::FindAt(P->GetWorld(), P->GetActorLocation()) : nullptr;
	}

	// ---------------------------------------------------------------- building

	void Rebuild()
	{
		TabStrip->ClearChildren();
		for (const FTab& T : Tabs())
		{
			if (!FWRBuildRegistry::Get().All().ContainsByPredicate([&T](const FWRPieceDef& D) { return TabOf(D) == T.Id; })) continue;
			const FName Id = T.Id;
			const bool bOn = Tab == Id;
			TabStrip->AddSlot().AutoWidth().VAlign(VAlign_Fill)
			[
				SNew(SButton).ButtonStyle(FCoreStyle::Get(), "NoBorder").ContentPadding(0.f).Cursor(EMouseCursor::Hand)
				.OnClicked_Lambda([this, Id]() { Tab = Id; Hovered = NAME_None; bDirty = true; return FReply::Handled(); })
				[
					SNew(SOverlay)
					+ SOverlay::Slot().VAlign(VAlign_Center).Padding(FMargin(8.f * U, 0.f))[WRUI::Text(T.Name, WRUI::Title(12), bOn ? WRUI::Ink : WRUI::Ink3, bOn)]
					+ SOverlay::Slot().VAlign(VAlign_Bottom).Padding(FMargin(8.f * U, 0.f, 8.f * U, 4.f * U))
					[
						SNew(SBox).HeightOverride(2.f).Visibility(bOn ? EVisibility::HitTestInvisible : EVisibility::Collapsed)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity(WRUI::Brass)]
					]
				]
			];
		}
		Cards->ClearChildren();
		for (const FWRPieceDef* D : InTab())
		{
			Cards->AddSlot().Padding(Cards->GetChildren()->Num() == 0 ? 0.f : 12.f * U, 0.f, 0.f, 0.f)[Card(*D)];
		}
		Details->SetContent(BuildDetails());
	}

	TSharedRef<SWidget> Bar()
	{
		return SNew(SBox).HeightOverride(BarHeight * U)
		[
			SNew(SWRPanel).Kind(EWRPanel::Stone).Rivets(true).BrassTrim(true).Padding(FMargin(16.f * U, 12.f * U, 16.f * U, 14.f * U))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 12.f * U)
				[
					SNew(SBox).HeightOverride(44.f * U)
					[
						SNew(SWRPanel).Kind(EWRPanel::Inset).Padding(FMargin(8.f * U, 0.f))
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.f)[SAssignNew(TabStrip, SHorizontalBox)]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.f * U, 0.f)[SNew(SBox).WidthOverride(1.f).HeightOverride(24.f * U)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity(WRUI::Brass2)]]
							+ SHorizontalBox::Slot().AutoWidth()[DemolishButton()]
						]
					]
				]
				+ SVerticalBox::Slot().FillHeight(1.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SBox).WidthOverride((3.f * 200.f + 2.f * 12.f) * U)
						[
							SAssignNew(Cards, SScrollBox).Orientation(Orient_Horizontal).ScrollBarThickness(FVector2D(4.f, 4.f))
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).Padding(12.f * U, 0.f, 0.f, 0.f)
					[
						SNew(SWRPanel).Kind(EWRPanel::Inset).Padding(FMargin(18.f * U, 11.f * U))[SAssignNew(Details, SBox)]
					]
				]
			]
		];
	}

	TSharedRef<SWidget> DemolishButton()
	{
		return SNew(SButton).ButtonStyle(FCoreStyle::Get(), "NoBorder").ContentPadding(0.f).Cursor(EMouseCursor::Hand)
			.ToolTipText(LOCTEXT("DemolishTip", "Aim at a piece you built and click: it comes down and half its cost comes back"))
			.OnClicked_Lambda([this]() { if (PC.IsValid()) PC->WRGhost(TEXT("demolish")); return FReply::Handled(); })
			[
				SNew(SBox).Padding(FMargin(12.f * U, 0.f, 8.f * U, 0.f)).VAlign(VAlign_Center)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 7.f * U, 0.f)[SNew(SMark).Kind(SMark::EKind::HammerRed).Size(16.f * U)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						WRUI::Text(LOCTEXT("Demolish", "Demolish"), WRUI::Title(12), TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(PC.IsValid() && PC->GetPlacingPiece() == TEXT("demolish") ? WRUI::Ember : WRUI::Bad); }), true)
					]
				]
			];
	}

	TSharedRef<SWidget> Card(const FWRPieceDef& D)
	{
		const FName Id = D.Id;
		const TArray<TPair<FName, int32>> Cost = CostOf(D);
		const int32 Can = Affordable(D);
		const bool bPicked = PC.IsValid() && PC->GetPlacingPiece() == Id;
		const bool bShort = Can <= 0;

		TSharedRef<SVerticalBox> Lines = SNew(SVerticalBox);
		if (Cost.Num() == 1)
		{
			const int32 H = Have(Cost[0].Key);
			Lines->AddSlot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f * U, 0.f)[SNew(SBox).WidthOverride(22.f * U).HeightOverride(22.f * U)[SNew(SImage).Image(WRUI::Icon(Cost[0].Key))]]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(FText::FromString(FString::Printf(TEXT("%d %s"), Cost[0].Value, *ItemName(Cost[0].Key))), WRUI::Body(13, 800), H >= Cost[0].Value ? WRUI::Ink : WRUI::Bad)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(5.f * U, 0.f, 0.f, 0.f)[WRUI::Text(FText::FromString(FString::Printf(TEXT("· you have %d"), H)), WRUI::Body(12, 500), H >= Cost[0].Value ? WRUI::Ink3 : WRUI::Bad)]
			];
		}
		else
		{
			TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
			for (int32 i = 0; i < Cost.Num(); ++i)
			{
				Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(i == 0 ? 0.f : 8.f * U, 0.f, 0.f, 0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 3.f * U, 0.f)[SNew(SBox).WidthOverride(20.f * U).HeightOverride(20.f * U)[SNew(SImage).Image(WRUI::Icon(Cost[i].Key))]]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(FText::AsNumber(Cost[i].Value), WRUI::Body(13, 800), Have(Cost[i].Key) >= Cost[i].Value ? WRUI::Ink : WRUI::Bad)]
				];
			}
			Lines->AddSlot().AutoHeight()[Row];
		}
		FString Enough;
		FLinearColor EnoughColor = WRUI::Good;
		if (bShort)
		{
			for (const TPair<FName, int32>& C : Cost)
			{
				if (Have(C.Key) < C.Value) { Enough = FString::Printf(TEXT("Need %d more %s"), C.Value - Have(C.Key), *ItemName(C.Key)); break; }
			}
			EnoughColor = WRUI::Bad;
		}
		else
		{
			Enough = Can >= 999 ? FString(TEXT("Free")) : FString::Printf(TEXT("Enough for %d"), Can);
			EnoughColor = Can == 1 ? WRUI::Warn : WRUI::Good;
		}
		Lines->AddSlot().AutoHeight().Padding(0.f, 2.f * U, 0.f, 0.f)[WRUI::Text(FText::FromString(Enough), WRUI::Body(11, 700), EnoughColor)];

		TSharedPtr<SButton> Button;
		SAssignNew(Button, SButton).ButtonStyle(FCoreStyle::Get(), "NoBorder").ContentPadding(0.f).Cursor(EMouseCursor::Hand)
			.OnHovered_Lambda([this, Id]() { if (Hovered != Id) { Hovered = Id; Details->SetContent(BuildDetails()); } })
			.OnUnhovered_Lambda([this, Id]() { if (Hovered == Id) { Hovered = NAME_None; Details->SetContent(BuildDetails()); } })
			.OnClicked_Lambda([this, Id]()
			{
				// Start placing it, exactly as the old menu's pick did: the controller's ghost, checked by the server on LMB.
				Picked = Id;
				if (PC.IsValid()) PC->WRGhost(Id);
				bDirty = true;
				return FReply::Handled();
			})
		[
			SNew(SBox).WidthOverride(200.f * U)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[SNew(SImage).Image(bPicked ? WRUI::Box(FLinearColor(FColor(0x33, 0x2C, 0x22)), 5.f, WRUI::Brass, 1.5f)
					: WRUI::Box(FLinearColor(FColor(0x22, 0x25, 0x29)), 5.f, bShort ? WRUI::Alpha(WRUI::Bad, 0.45f) : FLinearColor(0.02f, 0.02f, 0.025f), 1.f))]
				+ SOverlay::Slot().Padding(1.f)[SNew(SImage).Image(WRUI::Box(FLinearColor::Transparent, 4.f, bPicked ? WRUI::Alpha(WRUI::Brass, 0.6f) : FLinearColor(FColor(0x3B, 0x40, 0x47)), 1.f))]
				+ SOverlay::Slot().Padding(FMargin(12.f * U, 8.f * U, 12.f * U, 10.f * U))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[
						SNew(SBox).HeightOverride(66.f * U).RenderOpacity(bShort ? 0.75f : 1.f)[SNew(SPieceArt).PieceId(Id).Size(FVector2D(130.f * U, 66.f * U))]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f * U, 0.f, 3.f * U)[WRUI::Text(FText::FromString(D.Name), WRUI::Body(14, 800), WRUI::Ink)]
					+ SVerticalBox::Slot().AutoHeight()[Lines]
				]
			]
		];
		TWeakPtr<SButton> WeakButton = Button;
		return SNew(SOverlay)
			+ SOverlay::Slot().Padding(-3.f)[SNew(SImage).Visibility(EVisibility::HitTestInvisible).Image(bPicked ? WRUI::Box(FLinearColor::Transparent, 8.f, WRUI::Alpha(WRUI::Ember, 0.4f), 2.f) : WRUI::Box(FLinearColor::Transparent, 8.f))]
			+ SOverlay::Slot()[Button.ToSharedRef()]
			+ SOverlay::Slot()[SNew(SImage).Visibility(EVisibility::HitTestInvisible).Image_Lambda([WeakButton, bPicked]()
			{
				const TSharedPtr<SButton> B = WeakButton.Pin();
				return B && B->IsHovered() && !bPicked ? WRUI::Box(FLinearColor(1.f, 0.85f, 0.6f, 0.05f), 5.f, WRUI::Brass2, 1.f) : WRUI::Box(FLinearColor::Transparent, 5.f);
			})];
	}

	TSharedRef<SWidget> BuildDetails()
	{
		const FWRPieceDef* D = Shown();
		if (!D) return SNullWidget::NullWidget;
		const TArray<TPair<FName, int32>> Cost = CostOf(*D);
		const int32 Can = Affordable(*D);
		const bool bPlacing = PC.IsValid() && PC->GetPlacingPiece() == D->Id;
		FText ChipText = bPlacing ? LOCTEXT("Placing", "Placing") : Can > 0 ? LOCTEXT("ReadyToPlace", "Ready to place") : LOCTEXT("CantAfford", "Can't afford");
		const FLinearColor ChipColor = bPlacing ? WRUI::Brass : Can > 0 ? WRUI::Good : WRUI::Bad;

		TArray<FString> CostBits, BagBits;
		for (const TPair<FName, int32>& C : Cost)
		{
			CostBits.Add(FString::Printf(TEXT("%d %s"), C.Value, *ItemName(C.Key)));
			BagBits.Add(FString::Printf(TEXT("%d %s"), Have(C.Key), *ItemName(C.Key)));
		}
		FWRSlotView First;
		if (Cost.Num() > 0) { First.ItemId = Cost[0].Key; First.Count = Cost[0].Value; First.bShowCount = true; }

		// Upkeep: wood and stone pieces each cost their material per hour inside a claim (pieces.json).
		const float PerHour = FWRBuildRegistry::Get().UpkeepPerPiecePerHour.FindRef(D->Material);
		const bool bUpkeep = PerHour > 0.f && (D->Material == TEXT("wood") || D->Material == TEXT("stone"));
		TSharedRef<SWidget> Upkeep = bUpkeep
			? StaticCastSharedRef<SWidget>(SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 4.f * U, 0.f)[SNew(SBox).WidthOverride(18.f * U).HeightOverride(18.f * U)[SNew(SImage).Image(WRUI::Icon(D->Material))]]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(FText::AsNumber(FMath::RoundToInt(PerHour)), WRUI::Body(13, 700), WRUI::Ink)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(5.f * U, 0.f, 0.f, 0.f)[WRUI::Text(LOCTEXT("PerHour", "per hour"), WRUI::Body(12, 500), WRUI::Ink3)])
			: WRUI::Text(LOCTEXT("NoUpkeep", "None"), WRUI::Body(13, 700), WRUI::Ink);

		// What placing it here does to the claim's upkeep.
		FString Effect;
		const AWRClaimTotem* Claim = ClaimHere();
		if (!bUpkeep) Effect = FString::Printf(TEXT("%s pays no upkeep."), *D->Name);
		else if (!Claim) Effect = TEXT("Outside a land claim nothing pays upkeep, but nothing stops others building beside it.");
		else
		{
			TMap<FName, int32> Pieces;
			for (TActorIterator<AWRBuildPiece> It(Claim->GetWorld()); It; ++It)
			{
				const FWRPieceDef* O = It->GetDef();
				if (O && !It->bGhost && It->OwnerName == Claim->OwnerName && FVector::Dist2D(It->GetActorLocation(), Claim->GetActorLocation()) <= Claim->Radius && (O->Material == TEXT("wood") || O->Material == TEXT("stone")))
				{
					Pieces.FindOrAdd(O->Material)++;
				}
			}
			Pieces.FindOrAdd(D->Material)++;
			int32 Total = 0;
			TArray<FString> Bits;
			for (const TPair<FName, float>& P : FWRBuildRules::UpkeepPerHour(Pieces))
			{
				Bits.Add(FString::Printf(TEXT("%.0f %s"), P.Value, *P.Key.ToString()));
			}
			for (const TPair<FName, int32>& P : Pieces) Total += P.Value;
			Effect = FString::Printf(TEXT("Placing it makes %d pieces in %s's claim: upkeep goes to %s per hour."), Total, *Claim->OwnerName, *FString::Join(Bits, TEXT(" + ")));
		}

		const auto Stat = [](const FText& Label, const TSharedRef<SWidget>& Value)
		{
			return SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(Label, WRUI::Caps(11), WRUI::Ink2)]
				+ SVerticalBox::Slot().AutoHeight()[Value];
		};
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f * U, 0.f)[WRUI::Text(FText::FromString(D->Name), WRUI::Title(17), WRUI::Ink, true)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SWRChip).Text(ChipText).Color(ChipColor).FontSize(11)]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f * U, 0.f, 0.f)[WRUI::Text(FText::FromString(Describe(*D)), WRUI::Body(12, 500), WRUI::Ink2, false, true)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 7.f * U, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f * U, 0.f)
				[
					SNew(SBox).Visibility(Cost.Num() > 0 ? EVisibility::HitTestInvisible : EVisibility::Collapsed)[SNew(SWRSlot).Size(44.f * U).View(First)]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 22.f * U, 0.f)[Stat(LOCTEXT("Cost", "Cost"), WRUI::Text(FText::FromString(FString::Join(CostBits, TEXT(", "))), WRUI::Body(13, 700), Can > 0 ? WRUI::Ink : WRUI::Bad))]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 22.f * U, 0.f)[Stat(LOCTEXT("InYourBag", "In your bag"), WRUI::Text(FText::FromString(FString::Join(BagBits, TEXT(", "))), WRUI::Body(13, 700), WRUI::Ink))]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Stat(LOCTEXT("UpkeepInClaim", "Upkeep in a claim"), Upkeep)]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 0.f)[WRUI::Text(FText::FromString(Effect), WRUI::Body(11, 500), WRUI::Ink3, false, true)];
	}

	// ---------------------------------------------------------------- over the world

	TSharedRef<SWidget> Banner()
	{
		return SNew(SWRPanel).Kind(EWRPanel::Glass).Padding(FMargin(16.f * U, 8.f * U))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f * U, 0.f)[SNew(SMark).Kind(SMark::EKind::Hammer).Size(20.f * U)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f * U, 0.f)[WRUI::Text(LOCTEXT("BuildMode", "Build mode"), WRUI::Title(14), WRUI::Ink, true)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f * U, 0.f)[SNew(SBox).WidthOverride(1.f).HeightOverride(20.f * U)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity(WRUI::Brass2)]]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f * U, 0.f)[SNew(SWRKeyCap).FontSize(12).Text_Lambda([this]() { return WRUI::KeyName(KeyOf(TEXT("Build"))); })]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(LOCTEXT("Leave", "Leave"), WRUI::Body(12, 500), WRUI::Ink2)]
		];
	}

	TSharedRef<SWidget> ClaimStatus()
	{
		return SNew(SWRPanel).Kind(EWRPanel::Glass).Padding(FMargin(12.f * U, 9.f * U, 16.f * U, 10.f * U))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 11.f * U, 0.f)
			[
				SNew(SMark).Kind(SMark::EKind::Flag).Size(22.f * U).Color_Lambda([this]() { FString A, B; FLinearColor C; ClaimLines(A, B, C); return C; })
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { FString A, B; FLinearColor C; ClaimLines(A, B, C); return FText::FromString(A); }), WRUI::Body(14, 800),
						TAttribute<FSlateColor>::CreateLambda([this]() { FString A, B; FLinearColor C; ClaimLines(A, B, C); return FSlateColor(C); }))]
					+ SHorizontalBox::Slot().AutoWidth().Padding(5.f * U, 0.f, 0.f, 0.f)[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { const AWRClaimTotem* T = ClaimHere(); return T ? FText::FromString(FString::Printf(TEXT("· %s's claim"), *T->OwnerName)) : FText::GetEmpty(); }), WRUI::Body(14, 500), WRUI::Ink2)]
				]
				+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { FString A, B; FLinearColor C; ClaimLines(A, B, C); return FText::FromString(B); }), WRUI::Body(12, 500), WRUI::Ink2)]
			]
		];
	}

	/** The claim you stand in: first line, second line, the flag's colour. */
	void ClaimLines(FString& OutA, FString& OutB, FLinearColor& OutColor) const
	{
		const APawn* P = PC.IsValid() ? PC->GetPawn() : nullptr;
		OutColor = WRUI::Ink3;
		if (!P) return;
		FString Why;
		const AWRClaimTotem* T = ClaimHere();
		if (FWRSocialRules::IsInSafeZone(P->GetWorld(), P->GetActorLocation()))
		{
			OutA = TEXT("Safe zone");
			OutB = TEXT("No building here");
			OutColor = WRUI::Bad;
		}
		else if (!T)
		{
			OutA = TEXT("No land claim here");
			OutB = TEXT("A claim totem stops others building near you");
		}
		else if (!AWRClaimTotem::CanBuild(P->GetWorld(), P->GetActorLocation(), Me(), Why))
		{
			OutA = TEXT("Someone else's claim");
			OutB = TEXT("You can't build here");
			OutColor = WRUI::Bad;
		}
		else
		{
			OutA = T->OwnerName == Me() ? TEXT("In your claim") : TEXT("In your clan's claim");
			OutColor = WRUI::Good;
			float Hours = -1.f;
			for (const TPair<FName, float>& C : T->CurrentUpkeepPerHour())
			{
				if (C.Value <= 0.f) continue;
				const float H = FWRInventoryRules::CountOf(T->UpkeepStorage, C.Key) / C.Value;
				Hours = Hours < 0.f ? H : FMath::Min(Hours, H);
			}
			if (T->bUnpaid) { OutB = TEXT("Upkeep unpaid: your pieces are decaying"); OutColor = WRUI::Bad; }
			else if (Hours < 0.f) OutB = TEXT("No upkeep due yet");
			else OutB = FString::Printf(TEXT("Upkeep stocked for %d h %02d m"), FMath::FloorToInt(Hours), FMath::FloorToInt(Hours * 60.f) % 60);
		}
	}

	/** Over the ghost: will it go, and if not, why. Follows the ghost on screen. */
	TSharedRef<SWidget> GhostChip()
	{
		return SAssignNew(Chip, SBox)
			.Visibility_Lambda([this]() { FVector2D At; return ChipAt(At) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			.RenderTransform_Lambda([this]()
			{
				FVector2D At;
				ChipAt(At);
				const FVector2D S = Chip.IsValid() ? Chip->GetDesiredSize() : FVector2D::ZeroVector;
				const FVector2D Pos(FMath::Clamp(At.X - S.X * 0.5f, 8.f, LocalSize.X - S.X - 8.f), FMath::Clamp(At.Y - S.Y - 10.f, 8.f, LocalSize.Y - S.Y - 8.f));
				return FSlateRenderTransform(Pos);
			})
			[
				SNew(SBorder)
				.BorderImage_Lambda([this]() { return PC.IsValid() && PC->IsPlacementValid() ? WRUI::Box(FLinearColor(FColor(0x18, 0x1A, 0x1E, 0xE6)), 6.f, FLinearColor::Black, 1.f) : WRUI::Box(FLinearColor(FColor(0x0E, 0x0C, 0x0C, 0xF0)), 6.f, WRUI::Alpha(WRUI::Bad, 0.7f), 1.f); })
				.Padding(FMargin(8.f * U, 6.f * U, 14.f * U, 6.f * U))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 9.f * U, 0.f)
					[
						SNew(SMark).Size(22.f * U).Kind_Lambda([this]() { return PC.IsValid() && PC->IsPlacementValid() ? SMark::EKind::Yes : SMark::EKind::No; })
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(ChipTitle()); }), WRUI::Body(14, 800),
							TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(PC.IsValid() && PC->IsPlacementValid() ? WRUI::Ink : FLinearColor::White); }))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(9.f * U, 0.f, 0.f, 0.f)
					[
						SNew(SBox).Visibility_Lambda([this]() { return ChipCost().IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 9.f * U, 0.f)[WRUI::Text(FText::FromString(TEXT("·")), WRUI::Body(14, 500), WRUI::Ink3)]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f * U, 0.f)
							[
								SNew(SBox).WidthOverride(18.f * U).HeightOverride(18.f * U)[SNew(SImage).Image_Lambda([this]() { const FWRPieceDef* D = PC.IsValid() ? FWRBuildRegistry::Get().Find(PC->GetPlacingPiece()) : nullptr; return WRUI::Icon(D && D->Cost.Num() > 0 ? D->Cost[0].Key : FName(TEXT("wood"))); })]
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(ChipCost()); }), WRUI::Body(14, 500), WRUI::Ink2)]
						]
					]
				]
			];
	}

	/** Where the chip goes (local units): above the ghost's top, or false when there's no ghost on screen. */
	bool ChipAt(FVector2D& Out) const
	{
		const AWRBuildPiece* G = Ghost();
		if (!G) return false;
		FVector Origin, Extent;
		G->GetActorBounds(false, Origin, Extent);
		if (Extent.IsNearlyZero()) { Origin = G->GetActorLocation(); Extent = FVector(0.f, 0.f, 100.f); }
		FVector2D Screen;
		if (!PC->ProjectWorldLocationToScreen(Origin + FVector(0.f, 0.f, Extent.Z + 20.f), Screen, true)) return false;
		int32 SX = 0, SY = 0;
		PC->GetViewportSize(SX, SY);
		if (SX <= 0 || SY <= 0) return false;
		Out = FVector2D(Screen.X * LocalSize.X / SX, Screen.Y * LocalSize.Y / SY);
		return true;
	}

	FString ChipTitle() const
	{
		if (!PC.IsValid()) return FString();
		const FWRPieceDef* D = FWRBuildRegistry::Get().Find(PC->GetPlacingPiece());
		if (!PC->IsPlacementValid())
		{
			FString Why = PC->GetPlacementWhy();
			if (Why.IsEmpty()) Why = TEXT("It won't go here");
			Why[0] = FChar::ToUpper(Why[0]);
			return Why;
		}
		if (!D)
		{
			const AWRBuildPiece* G = Ghost();
			const FWRPieceDef* Target = G ? G->GetDef() : nullptr;
			return Target ? FString::Printf(TEXT("Demolish %s"), *Target->Name) : FString(TEXT("Demolish"));
		}
		return D->Name;
	}

	FString ChipCost() const
	{
		if (!PC.IsValid() || !PC->IsPlacementValid()) return FString();
		const FWRPieceDef* D = FWRBuildRegistry::Get().Find(PC->GetPlacingPiece());
		if (D)
		{
			TArray<FString> Bits;
			for (const TPair<FName, int32>& C : CostOf(*D)) Bits.Add(FString::Printf(TEXT("%d %s"), C.Value, *ItemName(C.Key)));
			return FString::Join(Bits, TEXT(", "));
		}
		// Demolish: half the piece's cost comes back.
		const AWRBuildPiece* G = Ghost();
		const FWRPieceDef* Target = G ? G->GetDef() : nullptr;
		if (!Target) return FString();
		TArray<FString> Bits;
		for (const TPair<FName, int32>& C : Target->Cost) if (C.Value / 2 > 0) Bits.Add(FString::Printf(TEXT("%d %s back"), C.Value / 2, *ItemName(C.Key)));
		return FString::Join(Bits, TEXT(", "));
	}

	/** The keys, above the bar: what they do right now (browsing, placing, demolishing). */
	TSharedRef<SWidget> KeysPanel()
	{
		TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
		Rows->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f * U)
		[
			WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return Mode() == 1 ? LOCTEXT("KPlacing", "Placing") : Mode() == 2 ? LOCTEXT("KDemolish", "Demolishing") : LOCTEXT("KBuilding", "Building"); }), WRUI::Caps(12), WRUI::Ink2)
		];
		for (int32 Row = 0; Row < 5; ++Row)
		{
			Rows->AddSlot().AutoHeight().Padding(0.f, 5.f * U, 0.f, 0.f)
			[
				SNew(SHorizontalBox).Visibility_Lambda([this, Row]() { FText K, L; bool bSnap = false; return KeyRow(Row, K, L, bSnap) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).WidthOverride(84.f * U).HAlign(HAlign_Right).Padding(0.f, 0.f, 12.f * U, 0.f)
					[
						SNew(SWRKeyCap).FontSize(12).Text_Lambda([this, Row]() { FText K, L; bool bSnap = false; KeyRow(Row, K, L, bSnap); return K; })
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					WRUI::Text(TAttribute<FText>::CreateLambda([this, Row]() { FText K, L; bool bSnap = false; KeyRow(Row, K, L, bSnap); return L; }), WRUI::Body(14, 500), WRUI::Ink)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f * U, 0.f, 0.f, 0.f)
				[
					SNew(SBox).Visibility_Lambda([this, Row]() { FText K, L; bool bSnap = false; KeyRow(Row, K, L, bSnap); return bSnap ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
					[
						SNew(SWRChip).FontSize(11)
						.Text_Lambda([this]() { return PC.IsValid() && PC->IsSnapOn() ? LOCTEXT("On", "On") : LOCTEXT("Off", "Off"); })
						.Color_Lambda([this]() { return PC.IsValid() && PC->IsSnapOn() ? WRUI::Good : WRUI::Warn; })
					]
				]
			];
		}
		return SNew(SBox).WidthOverride(252.f * U)
		[
			SNew(SWRPanel).Kind(EWRPanel::Glass).Rivets(true).Padding(FMargin(18.f * U, 14.f * U, 18.f * U, 16.f * U))[Rows]
		];
	}

	/** 0 browsing the bar, 1 placing a piece, 2 demolishing. */
	int32 Mode() const { return !PC.IsValid() || !PC->IsPlacing() ? 0 : PC->GetPlacingPiece() == TEXT("demolish") ? 2 : 1; }

	/** One line of the keys panel: its key and what it does (false: no line there in this mode). */
	bool KeyRow(int32 Row, FText& OutKey, FText& OutLabel, bool& bOutSnap) const
	{
		const int32 M = Mode();
		const FWRPieceDef* D = PC.IsValid() ? FWRBuildRegistry::Get().Find(PC->GetPlacingPiece()) : nullptr;
		const bool bGrid = D && D->Snaps() && PC->IsSnapOn();
		struct FLine { const TCHAR* Action; FText Label; bool bSnap; };
		TArray<FLine> Lines;
		if (M == 1)
		{
			Lines.Add({ TEXT("Light"), LOCTEXT("KPlace", "Place"), false });
			Lines.Add({ TEXT("Heavy"), bGrid ? LOCTEXT("KRotate90", "Rotate 90°") : LOCTEXT("KRotate45", "Rotate 45°"), false });
			Lines.Add({ TEXT("Block"), LOCTEXT("KCancel", "Cancel"), false });
			Lines.Add({ TEXT("Snap"), LOCTEXT("KSnap", "Snap"), true });
			Lines.Add({ TEXT("Build"), LOCTEXT("KLeave", "Leave build mode"), false });
		}
		else if (M == 2)
		{
			Lines.Add({ TEXT("Light"), LOCTEXT("KRemove", "Remove (half back)"), false });
			Lines.Add({ TEXT("Block"), LOCTEXT("KStop", "Stop demolishing"), false });
			Lines.Add({ TEXT("Build"), LOCTEXT("KLeave", "Leave build mode"), false });
		}
		else
		{
			Lines.Add({ TEXT("Light"), LOCTEXT("KPick", "Pick a piece below"), false });
			Lines.Add({ nullptr, LOCTEXT("KDemolishHint", "Take down a piece"), false });
			Lines.Add({ TEXT("Snap"), LOCTEXT("KSnap", "Snap"), true });
			Lines.Add({ TEXT("Build"), LOCTEXT("KLeave", "Leave build mode"), false });
		}
		if (!Lines.IsValidIndex(Row)) return false;
		OutKey = Lines[Row].Action ? WRUI::KeyName(KeyOf(Lines[Row].Action)) : LOCTEXT("KDemolishKey", "Demolish");
		OutLabel = Lines[Row].Label;
		bOutSnap = Lines[Row].bSnap;
		return true;
	}
};

	WR_REGISTER_SCREEN("build", [](AWRPlayerController* PC) -> TSharedRef<SWidget> { return SNew(SWRBuildBar).PC(PC); });
}

#undef LOCTEXT_NAMESPACE
