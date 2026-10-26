// The journal's Map tab (mockup "Map"): the parchment island map (Data/UI/map.png) with you and your heading, the
// island's places (camps, caves, bosses, passes from the dressing file, as the HUD compass uses), your Hearth, your
// tames, land claims and, when shown, other players this client knows about. A legend filters them; the wheel zooms
// about the cursor, a drag pans, a click picks a marker for the details panel. Sizes are mockup px x U.
//
// World to map: map.png is Art/Terrain/island_height.r16 (4033 x 4033 samples, 1 m apart) sampled every 4 m into
// 1009 px, pixel (px, py) = sample (4 px, 4 py) (mock/gen_assets.mjs). The landscape is centred on the origin with
// 1 m per sample (Tools/terrain/import_island.py: world = sample x 100 cm - 201600), north (-Y) up, east (+X) right.
// So pixel centre px + 0.5 sits at world X = 400 px - 201600 cm: the picture spans X, Y in [-201800, 201800] cm.

#include "WRPlayerUI.h"
#include "WRUIStyle.h"
#include "WRUIWidgets.h"
#include "WRBuilding.h"
#include "WRCharacter.h"
#include "WRCrafting.h"
#include "WRCreature.h"
#include "WRInventory.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRWorldDressing.h"
#include "EngineUtils.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerState.h"
#include "Rendering/DrawElements.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "WRUI"

// Named (not anonymous): unity builds put several UI files in one translation unit.
namespace WRMapTab
{
	using WRUI::U;
	constexpr float MapMin = -201800.f;   // world cm at the picture's left / top edge
	constexpr float MapSize = 403600.f;   // world cm across the picture
	constexpr float MaxZoom = 8.f;

	FLinearColor Hex(const TCHAR* H) { return FLinearColor(FColor::FromHex(H)); }
	const FSlateBrush* Glyph(const FString& Name) { return WRUI::Image(FName(*(TEXT("Glyphs/") + Name))); }

	FVector2D Measure(const FText& Text, const FSlateFontInfo& Font)
	{
		return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text, Font);
	}

	FString Distance(float Cm)
	{
		const float M = Cm / 100.f;
		return M < 1000.f ? FString::Printf(TEXT("%d m"), FMath::RoundToInt(M / 5.f) * 5) : FString::Printf(TEXT("%.1f km"), M / 1000.f);
	}

	/** Legend categories (the order of the legend). */
	enum class ECat : uint8 { Camp, Cave, Boss, Pass, Base, Tame, Claim, Player, Count };

	struct FMarker
	{
		FString Key;
		ECat Cat = ECat::Camp;
		FString Name;
		FString Kind;            // poi kind: boss, mini-boss, camp, cave, pass
		FVector World = FVector::ZeroVector;
		FName CreatureId;        // bosses and tames: portrait / drops
		float Radius = 0.f;      // claims
		bool bMine = false;
		TWeakObjectPtr<AActor> Actor;
	};

	/** View and filters, kept while the game runs (reopening the map shows it as you left it). */
	struct FMapState
	{
		float Zoom = 1.f;
		FVector2D Centre = FVector2D::ZeroVector;   // world cm at the middle of the view
		bool bShow[static_cast<int32>(ECat::Count)] = { true, true, true, true, true, true, true, false };
		FString Selected;
		bool bPicked = false;
		/** Your Hearths and claims as last seen: they stop replicating once you walk out of range, but they're still there. */
		TMap<FString, FMarker> Known;
	};
	FMapState& State() { static FMapState S; return S; }

	struct FCatLook { FLinearColor Ring, Fill, Icon; const TCHAR* Glyph; float Size; bool bSquare; };
	FCatLook LookFor(const FMarker& M)
	{
		switch (M.Cat)
		{
		case ECat::Camp: return { Hex(TEXT("9CCB74")), FLinearColor(0.012f, 0.02f, 0.008f, 0.92f), Hex(TEXT("9CCB74")), TEXT("camp"), 24.f, false };
		case ECat::Cave: return { Hex(TEXT("A3AAB2")), FLinearColor(0.012f, 0.013f, 0.015f, 0.92f), Hex(TEXT("BCC3CA")), TEXT("cave"), 20.f, true };
		case ECat::Boss: return M.Kind == TEXT("boss") ? FCatLook{ Hex(TEXT("EC6A48")), Hex(TEXT("3A1208")), Hex(TEXT("EC6A48")), TEXT("skull"), 34.f, false }
			: FCatLook{ Hex(TEXT("EAB95A")), FLinearColor(0.025f, 0.012f, 0.002f, 0.94f), Hex(TEXT("EAB95A")), TEXT("skull"), 24.f, false };
		case ECat::Pass: return { WRUI::Ink3, FLinearColor(0.01f, 0.01f, 0.012f, 0.9f), WRUI::Ink2, TEXT("mountain"), 22.f, false };
		case ECat::Base: return { WRUI::Brass, FLinearColor(0.02f, 0.012f, 0.004f, 0.94f), WRUI::Brass, TEXT("house"), 24.f, false };
		case ECat::Tame: return { WRUI::Brass, FLinearColor(0.02f, 0.012f, 0.004f, 0.94f), WRUI::Brass, TEXT("paw"), 22.f, false };
		case ECat::Claim: return { M.bMine ? WRUI::Brass : WRUI::Bad, FLinearColor(0.02f, 0.012f, 0.004f, 0.94f), M.bMine ? WRUI::Brass : WRUI::Bad, TEXT("flag"), 20.f, false };
		default: return { WRUI::Frost, Hex(TEXT("0C141B")), WRUI::Frost, TEXT("person"), 22.f, false };
		}
	}

	/** Everything this client knows to put on the map, freshly each frame (actors come and go with relevancy). */
	TArray<FMarker> Gather(const AWRPlayerController* PC)
	{
		TArray<FMarker> Out;
		for (const FWRPoi& P : FWRPoi::All)
		{
			FMarker& M = Out.AddDefaulted_GetRef();
			M.Key = TEXT("poi:") + P.Name;
			M.Name = P.Name;
			M.Kind = P.Kind;
			M.World = P.Location;
			M.Cat = P.Kind == TEXT("camp") ? ECat::Camp : P.Kind == TEXT("cave") ? ECat::Cave : P.Kind.Contains(TEXT("boss")) ? ECat::Boss : ECat::Pass;
			if (M.Cat == ECat::Boss) M.CreatureId = FName(*P.Name.ToLower().Replace(TEXT(" "), TEXT("_")));
		}
		UWorld* World = PC ? PC->GetWorld() : nullptr;
		const APlayerState* PS = PC ? PC->PlayerState : nullptr;
		if (!World || !PS) return Out;
		const FString Me = PS->GetPlayerName();
		for (TActorIterator<AWRHearth> It(World); It; ++It)
		{
			// A Hearth belongs to whoever placed the piece it stands on (its owner actor, which replicates).
			const AWRBuildPiece* Piece = Cast<AWRBuildPiece>(It->GetOwner());
			if (!Piece || Piece->OwnerName != Me) continue;
			FMarker& M = Out.AddDefaulted_GetRef();
			M.Key = FString::Printf(TEXT("hearth:%d,%d"), FMath::RoundToInt(It->GetActorLocation().X / 100.f), FMath::RoundToInt(It->GetActorLocation().Y / 100.f));
			M.Cat = ECat::Base;
			M.Name = TEXT("Your Hearth");
			M.World = It->GetActorLocation();
			M.bMine = true;
			M.Actor = *It;
		}
		for (TActorIterator<AWRCreature> It(World); It; ++It)
		{
			if (!It->IsAlive() || !It->IsTamed() || It->GetTameOwner() != Me) continue;
			FMarker& M = Out.AddDefaulted_GetRef();
			M.Key = TEXT("tame:") + It->GetName();
			M.Cat = ECat::Tame;
			M.Name = It->GetDisplayLabel();
			M.World = It->GetActorLocation();
			M.CreatureId = It->CreatureId;
			M.bMine = true;
			M.Actor = *It;
		}
		for (TActorIterator<AWRClaimTotem> It(World); It; ++It)
		{
			FMarker& M = Out.AddDefaulted_GetRef();
			M.Key = FString::Printf(TEXT("claim:%d,%d"), FMath::RoundToInt(It->GetActorLocation().X / 100.f), FMath::RoundToInt(It->GetActorLocation().Y / 100.f));
			M.Cat = ECat::Claim;
			M.bMine = It->OwnerName == Me;
			M.Name = M.bMine ? TEXT("Your land claim") : FString::Printf(TEXT("%s's claim"), *It->OwnerName);
			M.World = It->GetActorLocation();
			M.Radius = It->Radius;
			M.Actor = *It;
		}
		for (TActorIterator<AWRCharacter> It(World); It; ++It)
		{
			if (*It == PC->GetPawn() || !It->IsAlive() || !It->GetPlayerState()) continue;
			FMarker& M = Out.AddDefaulted_GetRef();
			M.Key = TEXT("player:") + It->GetPlayerState()->GetPlayerName();
			M.Cat = ECat::Player;
			M.Name = It->GetPlayerState()->GetPlayerName();
			M.World = It->GetActorLocation();
			M.Actor = *It;
		}
		// Remember what's yours; keep showing it out of range, and forget it only when you're close enough that it
		// would replicate and it doesn't (it's gone).
		TMap<FString, FMarker>& Known = State().Known;
		TSet<FString> Seen;
		for (const FMarker& M : Out)
		{
			if (M.bMine && (M.Cat == ECat::Base || M.Cat == ECat::Claim)) { Known.Add(M.Key, M); Seen.Add(M.Key); }
		}
		const APawn* Pawn = PC->GetPawn();
		for (auto It = Known.CreateIterator(); It; ++It)
		{
			if (Seen.Contains(It.Key())) continue;
			if (Pawn && FVector::Dist2D(Pawn->GetActorLocation(), It.Value().World) < 12000.f) { It.RemoveCurrent(); continue; }
			FMarker& M = Out.Add_GetRef(It.Value());
			M.Actor = nullptr;
		}
		return Out;
	}

	FString CatName(ECat C)
	{
		static const TCHAR* Names[] = { TEXT("Camps"), TEXT("Caves"), TEXT("Bosses"), TEXT("Passes"), TEXT("Your base"), TEXT("Tames"), TEXT("Land claims"), TEXT("Other players") };
		return Names[static_cast<int32>(C)];
	}

	/** The picture and everything on it; wheel zooms about the cursor, drag pans, click picks a marker. */
	class SWRMapView : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SWRMapView) {}
			SLATE_ARGUMENT(TWeakObjectPtr<AWRPlayerController>, PC)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			PC = InArgs._PC;
			SetClipping(EWidgetClipping::ClipToBounds);
		}
		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(862.f, 678.f) * U; }

		// --- view maths
		static float Frac(float W) { return (W - MapMin) / MapSize; }
		FVector2D ToLocal(const FVector& W, const FVector2D& Size) const
		{
			const FMapState& S = State();
			const float Img = Size.Y * S.Zoom;
			return FVector2D((Frac(W.X) - Frac(S.Centre.X)) * Img, (Frac(W.Y) - Frac(S.Centre.Y)) * Img) + Size * 0.5f;
		}
		FVector2D ToWorld(const FVector2D& L, const FVector2D& Size) const
		{
			const FMapState& S = State();
			const float Img = Size.Y * S.Zoom;
			const FVector2D F = (L - Size * 0.5f) / Img + FVector2D(Frac(S.Centre.X), Frac(S.Centre.Y));
			return F * MapSize + FVector2D(MapMin, MapMin);
		}
		static void Clamp()
		{
			FMapState& S = State();
			S.Zoom = FMath::Clamp(S.Zoom, 1.f, MaxZoom);
			S.Centre.X = FMath::Clamp(S.Centre.X, MapMin, MapMin + MapSize);
			S.Centre.Y = FMath::Clamp(S.Centre.Y, MapMin, MapMin + MapSize);
		}
		static void ZoomBy(float Factor) { State().Zoom *= Factor; Clamp(); }

		virtual FReply OnMouseWheel(const FGeometry& Geo, const FPointerEvent& Event) override
		{
			// Keep the point under the cursor where it is.
			const FVector2D Size = Geo.GetLocalSize();
			const FVector2D L = Geo.AbsoluteToLocal(Event.GetScreenSpacePosition());
			const FVector2D Before = ToWorld(L, Size);
			State().Zoom *= Event.GetWheelDelta() > 0.f ? 1.25f : 0.8f;
			Clamp();
			const FVector2D After = ToWorld(L, Size);
			State().Centre += Before - After;
			Clamp();
			return FReply::Handled();
		}
		virtual FReply OnMouseButtonDown(const FGeometry& Geo, const FPointerEvent& Event) override
		{
			if (Event.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
			bDown = true;
			bDragged = false;
			DownAt = LastAt = Event.GetScreenSpacePosition();
			return FReply::Handled().CaptureMouse(SharedThis(this));
		}
		virtual FReply OnMouseMove(const FGeometry& Geo, const FPointerEvent& Event) override
		{
			const FVector2D Size = Geo.GetLocalSize();
			MouseLocal = Geo.AbsoluteToLocal(Event.GetScreenSpacePosition());
			if (bDown && HasMouseCapture())
			{
				if (FVector2D::Distance(Event.GetScreenSpacePosition(), DownAt) > 4.f) bDragged = true;
				if (bDragged)
				{
					const FVector2D DL = Geo.AbsoluteToLocal(Event.GetScreenSpacePosition()) - Geo.AbsoluteToLocal(LastAt);
					State().Centre -= DL / (Size.Y * State().Zoom) * MapSize;
					Clamp();
				}
				LastAt = Event.GetScreenSpacePosition();
				return FReply::Handled();
			}
			return FReply::Unhandled();
		}
		virtual FReply OnMouseButtonUp(const FGeometry& Geo, const FPointerEvent& Event) override
		{
			if (!bDown) return FReply::Unhandled();
			bDown = false;
			if (!bDragged)
			{
				const FString Hit = MarkerAt(Geo.AbsoluteToLocal(Event.GetScreenSpacePosition()), Geo.GetLocalSize());
				if (!Hit.IsEmpty()) { State().Selected = Hit; State().bPicked = true; }
			}
			return FReply::Handled().ReleaseMouseCapture();
		}
		virtual void OnMouseLeave(const FPointerEvent& Event) override { MouseLocal = FVector2D(-1e6f, -1e6f); }
		virtual FCursorReply OnCursorQuery(const FGeometry& Geo, const FPointerEvent& Event) const override
		{
			return FCursorReply::Cursor(bDragged && bDown ? EMouseCursor::GrabHandClosed : !MarkerAt(MouseLocal, Geo.GetLocalSize()).IsEmpty() ? EMouseCursor::Hand : EMouseCursor::GrabHand);
		}

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override
		{
			const FVector2D Size = Geo.GetLocalSize();
			const FMapState& S = State();
			const auto Box = [&](int32 L, const FVector2D& Pos, const FVector2D& Sz, const FSlateBrush* B, const FLinearColor& Tint = FLinearColor::White)
			{
				FSlateDrawElement::MakeBox(Out, L, Geo.ToPaintGeometry(Sz, FSlateLayoutTransform(Pos)), B, ESlateDrawEffect::None, B && Tint == FLinearColor::White ? B->TintColor.GetSpecifiedColor() : Tint);
			};
			const auto Text = [&](int32 L, const FText& T, const FSlateFontInfo& F, const FVector2D& Pos, const FLinearColor& C, bool bShadow)
			{
				const FVector2D TS = Measure(T, F);
				if (bShadow) FSlateDrawElement::MakeText(Out, L, Geo.ToPaintGeometry(TS, FSlateLayoutTransform(Pos + FVector2D(1.f, 1.f))), T, F, ESlateDrawEffect::None, WRUI::Shadow);
				FSlateDrawElement::MakeText(Out, L + 1, Geo.ToPaintGeometry(TS, FSlateLayoutTransform(Pos)), T, F, ESlateDrawEffect::None, C);
			};
			// Sea, then the island picture.
			Box(Layer, FVector2D::ZeroVector, Size, WRUI::White(), Hex(TEXT("18242E")));
			const float Img = Size.Y * S.Zoom;
			const FVector2D ImgPos = ToLocal(FVector(MapMin, MapMin, 0.f), Size);
			if (const FSlateBrush* Map = WRUI::Image(TEXT("map")))
			{
				FSlateDrawElement::MakeBox(Out, Layer + 1, Geo.ToPaintGeometry(FVector2D(Img, Img), FSlateLayoutTransform(ImgPos)), Map);
			}
			// A 500 m grid over everything.
			const float Step = 50000.f;
			for (float W = 0.f; W <= MapSize * 0.5f; W += Step)
			{
				for (const float Sign : { -1.f, 1.f })
				{
					if (W == 0.f && Sign < 0.f) continue;
					const FVector2D A = ToLocal(FVector(Sign * W, Sign * W, 0.f), Size);
					FSlateDrawElement::MakeBox(Out, Layer + 2, Geo.ToPaintGeometry(FVector2D(1.f, Size.Y), FSlateLayoutTransform(FVector2D(A.X, 0.f))), WRUI::White(), ESlateDrawEffect::None, WRUI::Alpha(WRUI::Ink, 0.07f));
					FSlateDrawElement::MakeBox(Out, Layer + 2, Geo.ToPaintGeometry(FVector2D(Size.X, 1.f), FSlateLayoutTransform(FVector2D(0.f, A.Y))), WRUI::White(), ESlateDrawEffect::None, WRUI::Alpha(WRUI::Ink, 0.07f));
					FSlateDrawElement::MakeBox(Out, Layer + 2, Geo.ToPaintGeometry(FVector2D(1.f, Img), FSlateLayoutTransform(FVector2D(A.X, ImgPos.Y))), WRUI::White(), ESlateDrawEffect::None, FLinearColor(0.008f, 0.005f, 0.002f, 0.22f));
					FSlateDrawElement::MakeBox(Out, Layer + 2, Geo.ToPaintGeometry(FVector2D(Img, 1.f), FSlateLayoutTransform(FVector2D(ImgPos.X, A.Y))), WRUI::White(), ESlateDrawEffect::None, FLinearColor(0.008f, 0.005f, 0.002f, 0.22f));
				}
			}
			// Region names, carved into the picture.
			const FSlateFontInfo RegionFont = WRUI::Title(FMath::RoundToInt(14.f + 2.f * (S.Zoom - 1.f)));
			const auto Region = [&](const TCHAR* Name, const FVector& At, const FLinearColor& C)
			{
				const FText T = FText::FromString(Name);
				const FVector2D TS = Measure(T, RegionFont);
				Text(Layer + 3, T, RegionFont, ToLocal(At, Size) - TS * 0.5f, C, false);
			};
			Region(TEXT("V  O  L  C  A  N  I  C"), FVector(-56600.f, 113400.f, 0.f), FLinearColor(1.f, 0.82f, 0.6f, 0.55f));
			Region(TEXT("T  U  N  D  R  A"), FVector(78400.f, -101600.f, 0.f), FLinearColor(0.02f, 0.05f, 0.1f, 0.55f));

			const TArray<FMarker> Markers = Gather(PC.Get());
			const FString Hover = MarkerAt(MouseLocal, Size);
			int32 L = Layer + 6;
			// Claims: their reach as a ring.
			for (const FMarker& M : Markers)
			{
				if (M.Cat != ECat::Claim || !S.bShow[static_cast<int32>(ECat::Claim)]) continue;
				const FVector2D C = ToLocal(M.World, Size);
				const float R = M.Radius / MapSize * Img;
				TArray<FVector2f> Ring;
				for (int32 i = 0; i <= 64; ++i) { const float A = 2.f * PI * i / 64.f; Ring.Add(FVector2f(C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R)); }
				FSlateDrawElement::MakeLines(Out, L, Geo.ToPaintGeometry(), Ring, ESlateDrawEffect::None, WRUI::Alpha(M.bMine ? WRUI::Brass : WRUI::Bad, 0.75f), true, 2.f);
			}
			// Markers, small things first so bosses sit on top.
			const ECat Order[] = { ECat::Cave, ECat::Pass, ECat::Claim, ECat::Camp, ECat::Boss, ECat::Base, ECat::Tame, ECat::Player };
			for (const ECat Cat : Order)
			{
				if (!S.bShow[static_cast<int32>(Cat)]) continue;
				for (const FMarker& M : Markers)
				{
					if (M.Cat != Cat) continue;
					const FCatLook Lk = LookFor(M);
					const FVector2D C = ToLocal(M.World, Size);
					const float D = Lk.Size * U;
					const bool bMajor = M.Kind == TEXT("boss");
					const bool bHot = M.Key == Hover || M.Key == S.Selected;
					if (bMajor) Box(L, C - FVector2D(D * 0.75f), FVector2D(D * 1.5f), WRUI::Box(WRUI::Alpha(Lk.Ring, 0.22f), D * 0.75f));
					if (bHot) Box(L, C - FVector2D(D * 0.5f + 5.f), FVector2D(D + 10.f), WRUI::Box(FLinearColor::Transparent, Lk.bSquare ? 8.f : D * 0.5f + 5.f, WRUI::Brass, 2.f));
					Box(L + 1, C - FVector2D(D * 0.5f + 1.f), FVector2D(D + 2.f), WRUI::Box(FLinearColor::Black, Lk.bSquare ? 6.f : D * 0.5f + 1.f));
					Box(L + 2, C - FVector2D(D * 0.5f), FVector2D(D), WRUI::Box(Lk.Fill, Lk.bSquare ? 5.f : D * 0.5f, Lk.Ring, bMajor ? 2.f : 1.5f));
					const float G = D * (bMajor ? 0.64f : 0.6f);
					Box(L + 3, C - FVector2D(G * 0.5f), FVector2D(G), Glyph(Lk.Glyph), Lk.Icon);
					// Names: camps and bosses always, the rest when you point at them.
					const bool bLabel = Cat == ECat::Camp || Cat == ECat::Boss || Cat == ECat::Tame || Cat == ECat::Player || bHot;
					if (!bLabel) continue;
					const FSlateFontInfo F = bMajor ? WRUI::Title(13) : WRUI::Body(11, 800);
					const FText T = FText::FromString(bMajor ? M.Name.ToUpper() : M.Name);
					const FVector2D TS = Measure(T, F);
					const bool bSide = Cat == ECat::Tame || Cat == ECat::Player;
					// Side names go seaward (left in the west half, right in the east), away from the bosses on the ridge.
					const float SideX = M.World.X < 0.f ? -D * 0.5f - 5.f - TS.X : D * 0.5f + 5.f;
					const FVector2D Pos = bSide ? C + FVector2D(SideX, -TS.Y * 0.5f) : C + FVector2D(-TS.X * 0.5f, D * 0.5f + 3.f);
					const FLinearColor TextC = bMajor ? Hex(TEXT("FFB7A0")) : Cat == ECat::Camp ? Hex(TEXT("E2F2D2")) : M.Kind == TEXT("mini-boss") ? Hex(TEXT("F4D99C")) : Cat == ECat::Tame || Cat == ECat::Base ? Hex(TEXT("F0D49B")) : WRUI::Ink;
					if (!bSide)
					{
						Box(L + 4, Pos - FVector2D(6.f, 1.f), TS + FVector2D(12.f, 2.f), bMajor ? WRUI::Box(FLinearColor(0.09f, 0.025f, 0.01f, 0.8f), 3.f, WRUI::Alpha(Hex(TEXT("EC6A48")), 0.45f), 1.f) : WRUI::Box(FLinearColor(0.03f, 0.035f, 0.03f, 0.65f), 3.f));
					}
					Text(L + 5, T, F, Pos, TextC, true);
				}
			}
			// You: a soft halo and the arrow turned to where you look.
			if (const APawn* Pawn = PC.IsValid() ? PC->GetPawn() : nullptr)
			{
				const FVector2D C = ToLocal(Pawn->GetActorLocation(), Size);
				const float H = 38.f * U, A = 28.f * U;
				Box(L + 8, C - FVector2D(H * 0.5f), FVector2D(H), WRUI::Box(WRUI::Alpha(WRUI::Brass, 0.18f), H * 0.5f, FLinearColor(0.94f, 0.83f, 0.61f, 0.55f), 1.5f));
				// Map up is north (-Y), right is east (+X): a yaw of 0 (facing +X) points the arrow right.
				const float Yaw = PC->GetControlRotation().Yaw;
				FSlateDrawElement::MakeRotatedBox(Out, L + 9, Geo.ToPaintGeometry(FVector2D(A, A), FSlateLayoutTransform(C - FVector2D(A * 0.5f))), Glyph(TEXT("map_player")), ESlateDrawEffect::None,
					FMath::DegreesToRadians(Yaw + 90.f));
			}
			// The frame's inner shadow.
			Box(L + 10, FVector2D::ZeroVector, Size, WRUI::Box(FLinearColor::Transparent, 3.f, FLinearColor(0.f, 0.f, 0.f, 0.22f), 30.f));
			Box(L + 10, FVector2D::ZeroVector, Size, WRUI::Box(FLinearColor::Transparent, 3.f, FLinearColor(0.f, 0.f, 0.f, 0.3f), 12.f));
			Box(L + 10, FVector2D::ZeroVector, Size, WRUI::Box(FLinearColor::Transparent, 3.f, FLinearColor::Black, 1.f));
			// Scale bar: 0 · half · whole, the longest of 1 km / 500 / 200 / 100 / 50 m that fits a short bar at this zoom.
			{
				float Metres = 1000.f;
				for (const float Try : { 1000.f, 500.f, 200.f, 100.f, 50.f })
				{
					Metres = Try;
					if (Try * 100.f / MapSize * Img <= 200.f * U) break;
				}
				const float Km = Metres * 100.f / MapSize * Img;
				const FVector2D Base(18.f * U, Size.Y - 16.f * U - 8.f * U);
				const FSlateFontInfo F = WRUI::Body(11, 700);
				Text(L + 11, FText::FromString(TEXT("0")), F, Base + FVector2D(0.f, -18.f * U), WRUI::Ink2, true);
				const auto Label = [](float M) { return FText::FromString(M >= 1000.f ? FString::Printf(TEXT("%.0f km"), M / 1000.f) : FString::Printf(TEXT("%.0f m"), M)); };
				const FText Half = Label(Metres * 0.5f), One = Label(Metres);
				Text(L + 11, Half, F, Base + FVector2D(Km * 0.5f - Measure(Half, F).X * 0.5f, -18.f * U), WRUI::Ink2, true);
				Text(L + 11, One, F, Base + FVector2D(Km - Measure(One, F).X, -18.f * U), WRUI::Ink, true);
				Box(L + 11, Base - FVector2D(1.f), FVector2D(Km + 2.f, 8.f * U + 2.f), WRUI::White(), FLinearColor::Black);
				for (int32 i = 0; i < 4; ++i)
				{
					Box(L + 12, Base + FVector2D(Km * i / 4.f, 0.f), FVector2D(Km / 4.f, 8.f * U), WRUI::White(), i % 2 ? Hex(TEXT("16181B")) : WRUI::Brass);
				}
			}
			return L + 13;
		}

		/** The marker under a local point (within its disc, a little forgiving), or empty. */
		FString MarkerAt(const FVector2D& Local, const FVector2D& Size) const
		{
			const FMapState& S = State();
			FString Best;
			float BestD = 16.f * U;
			for (const FMarker& M : Gather(PC.Get()))
			{
				if (!S.bShow[static_cast<int32>(M.Cat)]) continue;
				const float D = FVector2D::Distance(ToLocal(M.World, Size), Local) - (M.Kind == TEXT("boss") ? 6.f * U : 0.f);
				if (D < BestD) { BestD = D; Best = M.Key; }
			}
			return Best;
		}

	private:
		TWeakObjectPtr<AWRPlayerController> PC;
		bool bDown = false;
		bool bDragged = false;
		FVector2D DownAt = FVector2D::ZeroVector;
		FVector2D LastAt = FVector2D::ZeroVector;
		FVector2D MouseLocal = FVector2D(-1e6f, -1e6f);
	};

	class SWRMapTabWidget : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SWRMapTabWidget) {}
			SLATE_ARGUMENT(TWeakObjectPtr<AWRPlayerController>, PC)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			PC = InArgs._PC;
			// Until you pick a marker, the panel shows the major boss nearest you.
			if (!State().bPicked) State().Selected = NearestBoss();

			ChildSlot
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).WidthOverride(880.f * U)
					[
						SNew(SWRPanel).Kind(EWRPanel::Leather).Padding(FMargin(9.f * U))
						[
							SNew(SOverlay)
							+ SOverlay::Slot()[SAssignNew(View, SWRMapView).PC(PC)]
							+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(14.f * U)
							[
								SNew(SBox).WidthOverride(92.f * U).HeightOverride(92.f * U).Visibility(EVisibility::HitTestInvisible)
								[
									SNew(SOverlay)
									+ SOverlay::Slot()[SNew(SImage).Image(Glyph(TEXT("map_compass")))]
									+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0.f, -2.f * U, 0.f, 0.f)[WRUI::Text(FText::FromString(TEXT("N")), WRUI::Title(12), Hex(TEXT("F0C878")), true)]
									+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 0.f, -4.f * U)[WRUI::Text(FText::FromString(TEXT("S")), WRUI::Title(9), WRUI::Ink2, true)]
									+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Center)[WRUI::Text(FText::FromString(TEXT("E")), WRUI::Title(9), WRUI::Ink2, true)]
									+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center)[WRUI::Text(FText::FromString(TEXT("W")), WRUI::Title(9), WRUI::Ink2, true)]
								]
							]
							+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(14.f * U)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()[MapButton(TEXT("plus"), LOCTEXT("ZoomIn", "Zoom in"), []() { SWRMapView::ZoomBy(1.4f); })]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f * U, 0.f, 0.f)[MapButton(TEXT("minus"), LOCTEXT("ZoomOut", "Zoom out"), []() { SWRMapView::ZoomBy(1.f / 1.4f); })]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f * U, 0.f, 0.f)[MapButton(TEXT("target"), LOCTEXT("CentreYou", "Centre on you"), [this]() { CentreOnYou(); })]
							]
						]
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(22.f * U, 0.f, 0.f, 0.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Bottom)[WRUI::Text(LOCTEXT("TheIsland", "The island"), WRUI::Title(20), WRUI::Ink)]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(0.f, 0.f, 0.f, 3.f * U)
						[
							WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return WhereLine(); }), WRUI::Body(12, 500), WRUI::Ink3)
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f * U, 0.f, 0.f)[Legend()]
					+ SVerticalBox::Slot().FillHeight(1.f).Padding(0.f, 14.f * U, 0.f, 0.f)
					[
						SNew(SBorder).BorderImage(WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.34f), 6.f, WRUI::Brass2, 1.f)).Padding(FMargin(18.f * U, 14.f * U, 18.f * U, 16.f * U))
						[
							SAssignNew(PickedBox, SBox)
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f * U, 0.f, 0.f)[Controls()]
				]
			];
		}

		virtual void Tick(const FGeometry& Geo, const double Time, const float Delta) override
		{
			SCompoundWidget::Tick(Geo, Time, Delta);
			// Rebuild the details when the pick changes, and now and then for distances and moving tames.
			if (State().Selected != ShownKey || Time > NextRefresh)
			{
				ShownKey = State().Selected;
				NextRefresh = Time + 0.5;
				PickedBox->SetContent(Picked());
			}
		}

	private:
		TWeakObjectPtr<AWRPlayerController> PC;
		TSharedPtr<SWRMapView> View;
		TSharedPtr<SBox> PickedBox;
		FString ShownKey = TEXT("-");
		double NextRefresh = 0.0;

		FVector Here() const { const APawn* P = PC.IsValid() ? PC->GetPawn() : nullptr; return P ? P->GetActorLocation() : FVector::ZeroVector; }

		FString NearestBoss() const
		{
			FString Best;
			float BestD = TNumericLimits<float>::Max();
			for (const FWRPoi& P : FWRPoi::All)
			{
				if (P.Kind != TEXT("boss")) continue;
				const float D = FVector::Dist2D(P.Location, Here());
				if (D < BestD) { BestD = D; Best = TEXT("poi:") + P.Name; }
			}
			return Best;
		}

		void CentreOnYou()
		{
			const FVector H = Here();
			State().Centre = FVector2D(H.X, H.Y);
			State().Zoom = FMath::Max(State().Zoom, 2.5f);
			SWRMapView::Clamp();
		}

		FText WhereLine() const
		{
			// The ridge runs down the middle (x = 0): volcanic to the west, tundra to the east.
			return FText::Format(LOCTEXT("Where", "4 × 4 km · you are on the {0} side"), Here().X < 0.f ? LOCTEXT("Volcanic", "volcanic") : LOCTEXT("Tundra", "tundra"));
		}

		TSharedRef<SWidget> MapButton(const FString& Icon, const FText& Tip, TFunction<void()> OnClick)
		{
			return SNew(SButton).ButtonStyle(FCoreStyle::Get(), "NoBorder").ContentPadding(0.f).Cursor(EMouseCursor::Hand).ToolTipText(Tip)
				.OnClicked_Lambda([OnClick]() { OnClick(); return FReply::Handled(); })
				[
					SNew(SBox).WidthOverride(34.f * U).HeightOverride(34.f * U)
					[
						SNew(SOverlay)
						+ SOverlay::Slot()[SNew(SImage).Image(WRUI::Box(FLinearColor(0.055f, 0.06f, 0.067f, 0.88f), 4.f, WRUI::Brass2, 1.f))]
						+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
						[
							SNew(SBox).WidthOverride(16.f * U).HeightOverride(Icon == TEXT("minus") ? 3.f * U : 16.f * U)
							[
								Icon == TEXT("minus") ? SNew(SImage).Image(WRUI::Box(WRUI::Ink, 1.5f * U)) : SNew(SImage).Image(Glyph(Icon)).ColorAndOpacity(Icon == TEXT("target") ? WRUI::Brass : WRUI::Ink)
							]
						]
					]
				];
		}

		// ---------------------------------------------------------------- legend

		int32 CountOf(ECat C, int32* OutMajor = nullptr) const
		{
			int32 N = 0, Major = 0;
			for (const FMarker& M : Gather(PC.Get()))
			{
				if (M.Cat != C) continue;
				++N;
				Major += M.Kind == TEXT("boss");
			}
			if (OutMajor) *OutMajor = Major;
			return N;
		}

		FText CatSub(ECat C) const
		{
			switch (C)
			{
			case ECat::Camp: return LOCTEXT("SubCamp", "· where survivors wake");
			case ECat::Boss: { int32 Major = 0; const int32 N = CountOf(C, &Major); return FText::Format(LOCTEXT("SubBoss", "· {0} major, {1} mini"), Major, N - Major); }
			case ECat::Pass: return LOCTEXT("SubPass", "· over the ridge");
			case ECat::Player: return State().bShow[static_cast<int32>(ECat::Player)] ? LOCTEXT("SubPlayersOn", "· those near you") : LOCTEXT("SubPlayersOff", "· hidden");
			default: return FText::GetEmpty();
			}
		}

		TSharedRef<SWidget> LegendIcon(ECat C)
		{
			FMarker M;
			M.Cat = C;
			M.bMine = true;
			const FCatLook Lk = LookFor(M);
			return SNew(SBox).WidthOverride(24.f * U).HeightOverride(24.f * U)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()[SNew(SImage).Image(WRUI::Box(Lk.Fill, Lk.bSquare ? 5.f : 12.f * U, Lk.Ring, 1.5f))]
					+ SOverlay::Slot().Padding(5.f * U)[SNew(SImage).Image(Glyph(Lk.Glyph)).ColorAndOpacity(Lk.Icon)]
				];
		}

		TSharedRef<SWidget> Legend()
		{
			const FLinearColor Ink = WRUI::InkDark, Soft = Hex(TEXT("5E4630"));
			TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
			for (int32 i = 0; i < static_cast<int32>(ECat::Count); ++i)
			{
				const ECat C = static_cast<ECat>(i);
				Rows->AddSlot().AutoHeight().Padding(0.f, i > 0 ? 4.f * U : 0.f, 0.f, 0.f)
				[
					SNew(SButton).ButtonStyle(FCoreStyle::Get(), "NoBorder").ContentPadding(0.f).Cursor(EMouseCursor::Hand)
					.OnClicked_Lambda([i]() { State().bShow[i] = !State().bShow[i]; return FReply::Handled(); })
					[
						SNew(SBox).HeightOverride(26.f * U)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f * U, 0.f)
							[
								SNew(SBox).WidthOverride(20.f * U).HeightOverride(20.f * U)
								[
									SNew(SOverlay)
									+ SOverlay::Slot()[SNew(SImage).Image_Lambda([i]() { return State().bShow[i] ? WRUI::Box(Hex(TEXT("4A3520")), 3.f, Hex(TEXT("2A1D12")), 1.5f) : WRUI::Box(FLinearColor(1.f, 0.97f, 0.9f, 0.55f), 3.f, Hex(TEXT("2A1D12")), 1.5f); })]
									+ SOverlay::Slot().Padding(3.f * U)[SNew(SImage).Image(Glyph(TEXT("check"))).ColorAndOpacity_Lambda([i]() { return State().bShow[i] ? Hex(TEXT("F0D49B")) : FLinearColor::Transparent; })]
								]
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f * U, 0.f)
							[
								SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("NoBorder")).Padding(0.f).ColorAndOpacity_Lambda([i]() { return FLinearColor(1.f, 1.f, 1.f, State().bShow[i] ? 1.f : 0.5f); })[LegendIcon(C)]
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								WRUI::Text(FText::FromString(CatName(C)), WRUI::Body(14, 700), TAttribute<FSlateColor>::CreateLambda([i, Ink]() { return FSlateColor(State().bShow[i] ? Ink : Hex(TEXT("6B5640"))); }))
							]
							+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(5.f * U, 0.f, 0.f, 0.f)
							[
								WRUI::Text(TAttribute<FText>::CreateLambda([this, C]() { return CatSub(C); }), WRUI::Body(14, 500), Soft)
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								WRUI::Text(TAttribute<FText>::CreateLambda([this, C]() { return FText::AsNumber(CountOf(C)); }), WRUI::Body(13, 500), Soft)
							]
						]
					]
				];
			}
			return SNew(SWRPanel).Kind(EWRPanel::Parchment).Padding(FMargin(18.f * U, 12.f * U, 18.f * U, 16.f * U))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f * U)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Bottom)[WRUI::Text(LOCTEXT("Legend", "Legend"), WRUI::Title(15), Ink)]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)[WRUI::Text(LOCTEXT("ShowOnMap", "Show on map"), WRUI::Body(12, 700), Soft)]
					]
					+ SVerticalBox::Slot().AutoHeight()[Rows]
				];
		}

		// ---------------------------------------------------------------- the picked marker

		TSharedRef<SWidget> Picked()
		{
			const TArray<FMarker> All = Gather(PC.Get());
			const FMarker* M = All.FindByPredicate([](const FMarker& X) { return X.Key == State().Selected; });
			if (!M)
			{
				return SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)[WRUI::Text(LOCTEXT("PickMarker", "Click a marker on the map to read about it."), WRUI::Body(14, 500), WRUI::Ink3)];
			}
			const FCatLook Lk = LookFor(*M);
			const float Dist = FVector::Dist2D(M->World, Here());
			const bool bWest = M->World.X < 0.f;
			const FWRCreatureDef* Creature = M->CreatureId.IsNone() ? nullptr : FWRCreatureRegistry::Get().Find(M->CreatureId);
			const FSlateBrush* Portrait = M->CreatureId.IsNone() ? nullptr : WRUI::Portrait(M->CreatureId);

			// The kind chip and what the game says about it.
			FText Chip, About;
			FLinearColor ChipColor = Lk.Ring;
			switch (M->Cat)
			{
			case ECat::Boss:
				Chip = FText::Format(LOCTEXT("ChipBoss", "{0} · {1}"), M->Kind == TEXT("boss") ? LOCTEXT("MajorBoss", "Major boss") : LOCTEXT("MiniBoss", "Mini-boss"),
					FText::FromString(Creature && !Creature->Element.IsNone() ? Creature->Element.ToString() : FString(bWest ? TEXT("fire") : TEXT("frost"))));
				ChipColor = M->Kind == TEXT("boss") ? WRUI::Ember : WRUI::Warn;
				break;
			case ECat::Camp: Chip = LOCTEXT("ChipCamp", "Camp"); About = LOCTEXT("AboutCamp", "Where new survivors of this side wake up. Wild creatures don't nest close by."); break;
			case ECat::Cave: Chip = LOCTEXT("ChipCave", "Cave"); About = bWest ? LOCTEXT("AboutCaveV", "A dark chamber of rich ore (better gem odds), sulfur gas and a burrower colony.") : LOCTEXT("AboutCaveT", "A dark chamber of rich ore (better gem odds), ice underfoot and a burrower colony."); break;
			case ECat::Pass: Chip = LOCTEXT("ChipPass", "Pass"); About = LOCTEXT("AboutPass", "A way over the ridge between the volcanic west and the tundra east."); break;
			case ECat::Base:
			{
				const AWRHearth* H = Cast<AWRHearth>(M->Actor.Get());
				Chip = LOCTEXT("ChipBase", "Your base");
				About = H ? FText::Format(LOCTEXT("AboutHearth", "Tech level {0}, {1} {1}|plural(one=core,other=cores) fed{2}."), H->GetTechLevel(), H->GetCores().Num(), H->IsBroken() ? LOCTEXT("Broken", ", broken: repair it to craft") : FText::GetEmpty()) : FText::GetEmpty();
				break;
			}
			case ECat::Tame:
			{
				const AWRCreature* T = Cast<AWRCreature>(M->Actor.Get());
				Chip = LOCTEXT("ChipTame", "Your tame");
				About = T ? FText::Format(LOCTEXT("AboutTame", "{0}, {1}."), T->TameOrder == EWRTameOrder::Follow ? LOCTEXT("Following", "Following you") : LOCTEXT("Staying", "Staying put"), T->bTameAggressive ? LOCTEXT("Aggressive", "aggressive") : LOCTEXT("Passive", "passive")) : FText::GetEmpty();
				break;
			}
			case ECat::Claim: Chip = M->bMine ? LOCTEXT("ChipClaimMine", "Your claim") : LOCTEXT("ChipClaim", "Land claim");
				About = FText::Format(LOCTEXT("AboutClaim", "Only its owner and their clan build within {0}."), FText::FromString(Distance(M->Radius))); break;
			default: Chip = LOCTEXT("ChipPlayer", "Survivor"); About = LOCTEXT("AboutPlayer", "Another survivor near enough for you to know of."); break;
			}

			TSharedRef<SVerticalBox> Box = SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[WRUI::Text(LOCTEXT("SelectedMarker", "Selected marker"), WRUI::Caps(13), WRUI::Ink2)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(FText::Format(LOCTEXT("FromYou", "{0} from you"), FText::FromString(Distance(Dist))), WRUI::Body(12, 500), WRUI::Ink2)]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f * U, 0.f, 0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 16.f * U, 0.f)
					[
						SNew(SBox).WidthOverride(96.f * U).HeightOverride(96.f * U)
						[
							SNew(SOverlay)
							+ SOverlay::Slot()[SNew(SImage).Image(WRUI::Box(FLinearColor(0.05f, 0.05f, 0.055f, 1.f), 4.f, WRUI::Brass2, 2.f))]
							+ SOverlay::Slot().Padding(2.f)[Portrait ? StaticCastSharedRef<SWidget>(SNew(SImage).Image(Portrait)) : StaticCastSharedRef<SWidget>(SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)[SNew(SBox).WidthOverride(46.f * U).HeightOverride(46.f * U)[SNew(SImage).Image(Glyph(Lk.Glyph)).ColorAndOpacity(Lk.Icon)]])]
							+ SOverlay::Slot().Padding(2.f).VAlign(VAlign_Bottom)[SNew(SBox).HeightOverride(36.f * U).Visibility(Portrait ? EVisibility::HitTestInvisible : EVisibility::Collapsed)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity(FLinearColor(0.2f, 0.05f, 0.01f, 0.35f))]]
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(M->Name)).Font(WRUI::Title(M->Name.Len() > 13 ? 18 : 24)).ColorAndOpacity(WRUI::Ink).AutoWrapText(true)]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left).Padding(0.f, 7.f * U, 0.f, 0.f)[SNew(SWRChip).Text(Chip).Color(ChipColor).FontSize(12)]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 7.f * U, 0.f, 0.f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f * U, 0.f)[SNew(SBox).WidthOverride(14.f * U).HeightOverride(14.f * U)[SNew(SImage).Image(Glyph(TEXT("mountain"))).ColorAndOpacity(WRUI::Ink3)]]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								WRUI::Text(FText::Format(LOCTEXT("Side", "{0} side · {1} m up"), bWest ? LOCTEXT("SideWest", "Volcanic, west") : LOCTEXT("SideEast", "Tundra, east"), FMath::Max(0, FMath::RoundToInt(M->World.Z / 100.f))), WRUI::Body(13, 500), WRUI::Ink2)
							]
						]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f * U)[WRUI::Divider()];

			// Bosses: what they drop (creature loot); everything else: what the game says it is.
			if (Creature && M->Cat == ECat::Boss && Creature->Loot.Num() > 0)
			{
				TSharedRef<SHorizontalBox> Slots = SNew(SHorizontalBox);
				FString Names;
				for (const TPair<FName, int32>& Drop : Creature->Loot)
				{
					const FName Item = Drop.Key;
					const int32 Count = Drop.Value;
					Slots->AddSlot().AutoWidth().Padding(0.f, 0.f, 6.f * U, 0.f)[SNew(SWRSlot).Size(48.f * U).View_Lambda([Item, Count]() { FWRSlotView V; V.ItemId = Item; V.Count = Count; return V; })];
					const FWRItemDef* Def = FWRItemRegistry::Get().Find(Item);
					Names += (Names.IsEmpty() ? TEXT("") : TEXT(", ")) + (Def ? Def->Name : Item.ToString());
				}
				Box->AddSlot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f * U, 0.f)[Slots]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::Format(LOCTEXT("Drops", "Drops: {0}"), FText::FromString(Names))).Font(WRUI::Body(13, 700)).ColorAndOpacity(WRUI::Ink).AutoWrapText(true)]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock).Font(WRUI::Body(12, 500)).ColorAndOpacity(WRUI::Ink2).AutoWrapText(true)
							.Text(M->Kind == TEXT("boss") ? LOCTEXT("CoreNote", "(its core raises your Hearth to the next tech level)") : LOCTEXT("ShardNote", "(shards upgrade your gear)"))
						]
					]
				];
			}
			else
			{
				Box->AddSlot().AutoHeight()[SNew(STextBlock).Text(About).Font(WRUI::Body(14, 500)).ColorAndOpacity(WRUI::Ink2).AutoWrapText(true)];
			}

			const FVector At = M->World;
			Box->AddSlot().FillHeight(1.f)[SNullWidget::NullWidget];
			// SWRButton's leather tile would size it to 256 units in an auto-height slot: give it its height.
			Box->AddSlot().AutoHeight().Padding(0.f, 10.f * U, 0.f, 0.f)
			[
				SNew(SBox).HeightOverride(44.f * U)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.5f)
					[
						SNew(SWRButton).Text(LOCTEXT("ShowOnMapBtn", "Show on map")).Primary(true).FontSize(14)
						.OnClicked_Lambda([At]() { State().Centre = FVector2D(At.X, At.Y); State().Zoom = FMath::Max(State().Zoom, 3.f); SWRMapView::Clamp(); return FReply::Handled(); })
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).Padding(10.f * U, 0.f, 0.f, 0.f)
					[
						SNew(SWRButton).Text(LOCTEXT("WholeIsland", "Whole island")).Ghost(true).FontSize(14)
						.OnClicked_Lambda([]() { State().Centre = FVector2D::ZeroVector; State().Zoom = 1.f; return FReply::Handled(); })
					]
				]
			];
			return Box;
		}

		TSharedRef<SWidget> Controls()
		{
			const auto Hint = [](const TCHAR* Icon, const FText& Text) -> TSharedRef<SWidget>
			{
				return SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 7.f * U, 0.f)[SNew(SBox).WidthOverride(18.f * U).HeightOverride(18.f * U)[SNew(SImage).Image(Glyph(Icon)).ColorAndOpacity(WRUI::Ink2)]]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(Text, WRUI::Body(12, 500), WRUI::Ink2)];
			};
			return SNew(SWRPanel).Kind(EWRPanel::Inset).Padding(FMargin(14.f * U, 10.f * U))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Left)[Hint(TEXT("mouse_left"), LOCTEXT("DragPan", "Drag to pan"))]
					+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Center)[Hint(TEXT("mouse_wheel"), LOCTEXT("WheelZoom", "Wheel to zoom"))]
					+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Right)[Hint(TEXT("mouse_left"), LOCTEXT("ClickMarker", "Click a marker: details"))]
				];
		}
	};
} // namespace WRMapTab

WR_REGISTER_JOURNAL_TAB("map", [](AWRPlayerController* PC) -> TSharedRef<SWidget> { return SNew(WRMapTab::SWRMapTabWidget).PC(PC); });

#undef LOCTEXT_NAMESPACE
