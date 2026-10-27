// The settings window (mockup "Settings: controls"): Esc in game (EWRMenu::Settings, which makes it the game menu:
// Resume, the settings pages, Back to main menu, Quit) and the main menu's Settings button. Pages exist only for what
// the game really has: Controls (every FWRKeyBindings action, how abilities are cast), Audio (the volumes WRSound keeps
// in GameUserSettings.ini) and Video (the engine's UGameUserSettings). There is no mouse sensitivity setting.
// Key changes save as they're made (FWRKeyBindings saves on every change), so there is no Apply step.

#include "WRUISettings.h"
#include "WRPlayerUI.h"
#include "WRUIStyle.h"
#include "WRUIWidgets.h"
#include "WRAbilities.h"
#include "WRKeyBindings.h"
#include "WRMainMenu.h"
#include "WRPathComponent.h"
#include "WRPathTypes.h"
#include "WRPlayerController.h"
#include "WRSound.h"
#include "WRUIFlow.h"   // the Game section's "Show hints"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Rendering/DrawElements.h"
#include "TimerManager.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBackgroundBlur.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "WRUI"

// ---------------------------------------------------------------- line icons

void SWRLineIcon::Construct(const FArguments& InArgs)
{
	Icon = InArgs._Icon;
	Size = InArgs._Size;
	Color = InArgs._Color;
	Accent = InArgs._Accent;
	SetCanTick(false);
}

int32 SWRLineIcon::OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
	using V = FVector2f;
	const FVector2f Local = FVector2f(Geo.GetLocalSize());
	const float S = FMath::Min(Local.X, Local.Y) / 24.f;
	const V O = (Local - V(24.f * S, 24.f * S)) * 0.5f;
	const FPaintGeometry PG = Geo.ToPaintGeometry();
	const float Fade = Style.GetColorAndOpacityTint().A;
	const FLinearColor C = Color.Get().CopyWithNewOpacity(Color.Get().A * Fade);
	const FLinearColor A = Accent.Get().CopyWithNewOpacity(Accent.Get().A * Fade);
	const FLinearColor Dark(0.16f, 0.07f, 0.02f, Fade);

	const auto Line = [&](const TArray<V>& Pts, const FLinearColor& Col, float W)
	{
		TArray<V> P;
		for (const V& X : Pts) P.Add(O + X * S);
		FSlateDrawElement::MakeLines(Out, Layer, PG, MoveTemp(P), ESlateDrawEffect::None, Col, true, W * S);
	};
	// Angles in degrees, screen space (0 = right, 90 = down).
	const auto Arc = [&](float CX, float CY, float R, float From, float To, const FLinearColor& Col, float W)
	{
		TArray<V> P;
		const int32 N = FMath::Max(6, FMath::CeilToInt(FMath::Abs(To - From) / 12.f));
		for (int32 i = 0; i <= N; ++i)
		{
			const float Ang = FMath::DegreesToRadians(From + (To - From) * i / N);
			P.Add(V(CX + FMath::Cos(Ang) * R, CY + FMath::Sin(Ang) * R));
		}
		Line(P, Col, W);
	};
	const auto Circle = [&](float CX, float CY, float R, const FLinearColor& Col, float W) { Arc(CX, CY, R, 0.f, 360.f, Col, W); };
	// A filled disc: a ring as thick as the radius.
	const auto Dot = [&](float CX, float CY, float R, const FLinearColor& Col) { Arc(CX, CY, R * 0.5f, 0.f, 360.f, Col, R); };
	// A filled polygon, drawn as horizontal strokes one unit apart (no brush needed).
	const auto Fill = [&](const TArray<V>& Poly, const FLinearColor& Col)
	{
		float MinY = TNumericLimits<float>::Max(), MaxY = TNumericLimits<float>::Lowest();
		for (const V& X : Poly) { MinY = FMath::Min(MinY, X.Y); MaxY = FMath::Max(MaxY, X.Y); }
		const float Step = 1.f / FMath::Max(S, 0.01f);
		for (float Y = MinY + Step * 0.5f; Y < MaxY; Y += Step)
		{
			TArray<float> Xs;
			for (int32 i = 0; i < Poly.Num(); ++i)
			{
				const V& P0 = Poly[i];
				const V& P1 = Poly[(i + 1) % Poly.Num()];
				if ((P0.Y <= Y && P1.Y > Y) || (P1.Y <= Y && P0.Y > Y))
				{
					Xs.Add(P0.X + (Y - P0.Y) * (P1.X - P0.X) / (P1.Y - P0.Y));
				}
			}
			Xs.Sort();
			for (int32 i = 0; i + 1 < Xs.Num(); i += 2)
			{
				Line({ V(Xs[i], Y), V(Xs[i + 1], Y) }, Col, Step * 1.4f);
			}
		}
	};
	const auto Closed = [](TArray<V> Poly) { const V First = Poly[0]; Poly.Add(First); return Poly; };

	switch (Icon)
	{
	case EWRIcon::Gear:
		for (int32 i = 0; i < 8; ++i)
		{
			const float Ang = FMath::DegreesToRadians(i * 45.f + 22.5f);
			const V Dir(FMath::Cos(Ang), FMath::Sin(Ang));
			Line({ V(12.f, 12.f) + Dir * 7.f, V(12.f, 12.f) + Dir * 10.4f }, C, 3.2f);
		}
		Circle(12.f, 12.f, 6.6f, C, 2.2f);
		Dot(12.f, 12.f, 2.4f, C);
		break;
	case EWRIcon::Flag:
		Line({ V(5.f, 21.f), V(5.f, 3.5f) }, C, 1.8f);
		Line({ V(5.f, 4.f), V(17.f, 4.f), V(14.5f, 8.f), V(17.f, 12.f), V(5.f, 12.f) }, C, 1.8f);
		break;
	case EWRIcon::FlagFilled:
	{
		const TArray<V> Cloth = { V(5.5f, 3.5f), V(18.5f, 3.5f), V(15.5f, 8.f), V(18.5f, 12.5f), V(5.5f, 12.5f) };
		Fill(Cloth, A);
		Line(Closed(Cloth), Dark, 1.2f);
		Line({ V(5.f, 22.f), V(5.f, 2.5f) }, C, 2.f);
		break;
	}
	case EWRIcon::Keyboard:
		Line(Closed({ V(4.f, 7.f), V(20.f, 7.f), V(22.f, 9.f), V(22.f, 16.f), V(20.f, 18.f), V(4.f, 18.f), V(2.f, 16.f), V(2.f, 9.f) }), C, 1.8f);
		for (const float X : { 6.f, 10.f, 14.f }) Line({ V(X, 11.f), V(X + 1.f, 11.f) }, C, 1.8f);
		Line({ V(17.5f, 11.f), V(18.f, 11.f) }, C, 1.8f);
		Line({ V(7.f, 14.5f), V(17.f, 14.5f) }, C, 1.8f);
		break;
	case EWRIcon::Monitor:
		Line(Closed({ V(3.f, 4.f), V(21.f, 4.f), V(21.f, 16.f), V(3.f, 16.f) }), C, 1.8f);
		Line({ V(9.f, 20.f), V(15.f, 20.f) }, C, 1.8f);
		Line({ V(12.f, 16.f), V(12.f, 20.f) }, C, 1.8f);
		break;
	case EWRIcon::Play:
		Fill({ V(8.f, 5.f), V(19.f, 12.f), V(8.f, 19.f) }, C);
		break;
	case EWRIcon::Exit:
		Line({ V(11.f, 3.5f), V(5.f, 3.5f), V(5.f, 20.5f), V(11.f, 20.5f) }, C, 2.f);
		Line({ V(21.f, 12.f), V(9.f, 12.f) }, A, 2.2f);
		Line({ V(13.f, 8.f), V(9.f, 12.f), V(13.f, 16.f) }, A, 2.2f);
		break;
	case EWRIcon::Join:
		Line({ V(13.f, 3.5f), V(19.f, 3.5f), V(20.f, 4.5f), V(20.f, 19.5f), V(19.f, 20.5f), V(13.f, 20.5f) }, C, 2.f);
		Line({ V(3.f, 12.f), V(14.f, 12.f) }, A, 2.2f);
		Line({ V(10.f, 8.f), V(14.f, 12.f), V(10.f, 16.f) }, A, 2.2f);
		break;
	case EWRIcon::Power:
		Arc(12.f, 12.1f, 7.5f, -48.f, 228.f, C, 2.f);
		Line({ V(12.f, 3.f), V(12.f, 11.f) }, A, 2.2f);
		break;
	case EWRIcon::Chevron:
		Line({ V(9.f, 5.f), V(16.f, 12.f), V(9.f, 19.f) }, C, 2.6f);
		break;
	case EWRIcon::Info:
		Circle(12.f, 12.f, 9.5f, C, 1.8f);
		Line({ V(12.f, 10.5f), V(12.f, 16.5f) }, C, 2.2f);
		Dot(12.f, 7.3f, 1.3f, C);
		break;
	case EWRIcon::Clock:
		Circle(12.f, 12.f, 9.f, C, 2.f);
		Line({ V(12.f, 7.f), V(12.f, 12.f), V(15.5f, 14.f) }, C, 2.f);
		break;
	case EWRIcon::Download:
		Line({ V(12.f, 3.f), V(12.f, 15.f) }, C, 2.4f);
		Line({ V(7.f, 10.5f), V(12.f, 15.5f), V(17.f, 10.5f) }, C, 2.4f);
		Line({ V(4.5f, 20.5f), V(19.5f, 20.5f) }, A, 2.2f);
		break;
	case EWRIcon::Bulb:
		Fill({ V(12.f, 3.f), V(16.5f, 5.f), V(18.f, 9.f), V(16.6f, 12.9f), V(14.5f, 16.f), V(9.5f, 16.f), V(7.4f, 12.9f), V(6.f, 9.f), V(7.5f, 5.f) }, C.CopyWithNewOpacity(0.2f * Fade));
		Arc(12.f, 9.f, 6.f, 140.f, 400.f, C, 1.7f);
		Line({ V(16.6f, 12.86f), V(14.5f, 16.f), V(9.5f, 16.f), V(7.4f, 12.86f) }, C, 1.7f);
		Line({ V(9.f, 18.5f), V(15.f, 18.5f) }, C, 1.7f);
		Line({ V(10.f, 21.f), V(14.f, 21.f) }, C, 1.7f);
		break;
	case EWRIcon::Flame:
	{
		const TArray<V> Shape = { V(12.f, 2.f), V(14.8f, 5.6f), V(17.2f, 9.f), V(18.f, 13.f), V(17.2f, 16.2f), V(15.f, 18.3f), V(12.f, 19.2f), V(9.f, 18.3f),
			V(6.8f, 16.2f), V(6.f, 13.f), V(6.8f, 10.2f), V(9.f, 7.2f), V(9.6f, 9.3f), V(12.f, 11.f), V(11.5f, 7.f) };
		Fill(Shape, C);
		Line(Closed(Shape), Dark, 1.2f);
		break;
	}
	case EWRIcon::Snowflake:
		Line({ V(12.f, 2.f), V(12.f, 22.f) }, C, 1.8f);
		Line({ V(3.3f, 7.f), V(20.7f, 17.f) }, C, 1.8f);
		Line({ V(3.3f, 17.f), V(20.7f, 7.f) }, C, 1.8f);
		Line({ V(9.f, 3.8f), V(12.f, 6.2f), V(15.f, 3.8f) }, C, 1.8f);
		Line({ V(9.f, 20.2f), V(12.f, 17.8f), V(15.f, 20.2f) }, C, 1.8f);
		break;
	case EWRIcon::Warning:
	{
		const TArray<V> Tri = { V(12.f, 3.f), V(22.f, 21.f), V(2.f, 21.f) };
		Fill(Tri, C);
		Line(Closed(Tri), Dark, 1.2f);
		Line({ V(12.f, 10.f), V(12.f, 15.f) }, Dark, 2.2f);
		Dot(12.f, 18.f, 1.3f, Dark);
		break;
	}
	case EWRIcon::Lock:
		Arc(12.f, 7.5f, 4.5f, 180.f, 360.f, C, 2.6f);
		Line({ V(7.5f, 7.5f), V(7.5f, 10.5f) }, C, 2.6f);
		Line({ V(16.5f, 7.5f), V(16.5f, 10.5f) }, C, 2.6f);
		Fill({ V(4.5f, 10.f), V(19.5f, 10.f), V(19.5f, 21.f), V(4.5f, 21.f) }, C);
		break;
	case EWRIcon::Close:
		Line({ V(5.f, 5.f), V(19.f, 19.f) }, C, 2.4f);
		Line({ V(19.f, 5.f), V(5.f, 19.f) }, C, 2.4f);
		break;
	case EWRIcon::Speaker:
		Fill({ V(3.f, 9.f), V(7.5f, 9.f), V(13.f, 4.5f), V(13.f, 19.5f), V(7.5f, 15.f), V(3.f, 15.f) }, C);
		Arc(13.f, 12.f, 4.5f, -48.f, 48.f, C, 1.8f);
		Arc(13.f, 12.f, 8.5f, -48.f, 48.f, C, 1.8f);
		break;
	}
	return Layer;
}

// ---------------------------------------------------------------- gradient

void SWRGradientFill::Construct(const FArguments& InArgs)
{
	Colors = InArgs._Colors;
	Stops = InArgs._Stops;
	bHorizontal = InArgs._Horizontal;
	Radius = InArgs._Radius;
	SetCanTick(false);
}

int32 SWRGradientFill::OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
	if (Colors.Num() < 2) return Layer;
	const FVector2f Size = FVector2f(Geo.GetLocalSize());
	const float Fade = Style.GetColorAndOpacityTint().A;
	TArray<FSlateGradientStop> GradientStops;
	for (int32 i = 0; i < Colors.Num(); ++i)
	{
		const float T = Stops.IsValidIndex(i) ? Stops[i] : float(i) / (Colors.Num() - 1);
		const FLinearColor C = Colors[i].CopyWithNewOpacity(Colors[i].A * Fade);
		GradientStops.Add(FSlateGradientStop(bHorizontal ? FVector2f(Size.X * T, 0.f) : FVector2f(0.f, Size.Y * T), C));
	}
	// Slate's "vertical" gradient is the one whose colour changes left to right.
	FSlateDrawElement::MakeGradient(Out, Layer, Geo.ToPaintGeometry(), MoveTemp(GradientStops), bHorizontal ? Orient_Vertical : Orient_Horizontal, ESlateDrawEffect::None, FVector4f(Radius));
	return Layer;
}

namespace WRSettingsUI
{
	using WRUI::U;   // mockup px -> 1080p units

	/** A mockup colour (sRGB hex): FLinearColor literals are linear, so dark tones written by hand come out too light. */
	FLinearColor Hex(const TCHAR* H) { return FLinearColor(FColor::FromHex(H)); }

	const FName GMovement(TEXT("Movement")), GCombat(TEXT("Combat")), GItems(TEXT("Items")), GMenus(TEXT("Menus")), GAbilities(TEXT("Abilities"));

	UGameUserSettings* VideoSettings() { return GEngine ? GEngine->GetGameUserSettings() : nullptr; }

	/** The window as it is now (a -windowed start overrides the saved mode without changing it). */
	EWindowMode::Type CurrentWindowMode()
	{
		const TSharedPtr<SWindow> Window = GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWindow() : nullptr;
		if (Window.IsValid()) return Window->GetWindowMode();
		const UGameUserSettings* S = VideoSettings();
		return S ? S->GetFullscreenMode() : EWindowMode::Windowed;
	}

	FSlateFontInfo Spaced(FSlateFontInfo Font, int32 LetterSpacing)
	{
		Font.LetterSpacing = LetterSpacing;
		return Font;
	}
	/** Small capitals label (the mockups' .wr-label). */
	TSharedRef<SWidget> Label(const TAttribute<FText>& Text, int32 Size = 12, const FLinearColor& Color = WRUI::Ink2)
	{
		return WRUI::Text(Text, Spaced(WRUI::Caps(Size), 80), Color);
	}
	TSharedRef<SWidget> Heading(const FText& Text, int32 Size = 16)
	{
		return WRUI::Text(Text, Spaced(WRUI::Title(Size), 60), WRUI::Ink, true);
	}
	TSharedRef<SWidget> Muted(const TAttribute<FText>& Text, int32 Size = 11, bool bWrap = false)
	{
		return WRUI::Text(Text, WRUI::Body(Size, 500), WRUI::Ink3, false, bWrap);
	}
	TSharedRef<SWidget> Ellipsis(const TAttribute<FText>& Text, const FSlateFontInfo& Font, const FLinearColor& Color)
	{
		return SNew(STextBlock).Text(Text).Font(Font).ColorAndOpacity(Color).OverflowPolicy(ETextOverflowPolicy::Ellipsis);
	}
	/** The thin lines between rows. */
	TSharedRef<SWidget> HLine(const FLinearColor& Color, float Height = 1.f)
	{
		return SNew(SBox).HeightOverride(Height)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity(Color)];
	}
	/** The title divider: brass that fades out at both ends. */
	TSharedRef<SWidget> FadeLine()
	{
		const FLinearColor B = WRUI::Brass2;
		return SNew(SBox).HeightOverride(1.f)
		[
			SNew(SWRGradientFill).Colors({ WRUI::Alpha(B, 0.f), B, B, WRUI::Alpha(B, 0.f) }).Stops({ 0.f, 0.18f, 0.82f, 1.f })
		];
	}
	const FSlateBrush* RowBrush() { return WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.2f), 4.f, FLinearColor(0.f, 0.f, 0.f, 0.6f), 1.f); }
	const FSlateBrush* InsetBrush() { return WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.32f), 4.f, FLinearColor(0.f, 0.f, 0.f, 0.7f), 1.f); }

	const FScrollBarStyle& ScrollStyle()
	{
		static FScrollBarStyle Style = []()
		{
			FScrollBarStyle S = FCoreStyle::Get().GetWidgetStyle<FScrollBarStyle>("ScrollBar");
			const FSlateBrush None = *WRUI::Box(FLinearColor::Transparent);
			S.SetVerticalBackgroundImage(None).SetVerticalTopSlotImage(None).SetVerticalBottomSlotImage(None)
				.SetHorizontalBackgroundImage(None).SetHorizontalTopSlotImage(None).SetHorizontalBottomSlotImage(None)
				.SetNormalThumbImage(*WRUI::Box(WRUI::Alpha(WRUI::Brass2, 0.55f), 3.f))
				.SetHoveredThumbImage(*WRUI::Box(WRUI::Alpha(WRUI::Brass2, 0.85f), 3.f))
				.SetDraggedThumbImage(*WRUI::Box(WRUI::Brass, 3.f));
			return S;
		}();
		return Style;
	}

	/** "Interact (hold on your tame: orders)" -> "Interact" and "Hold on your tame: orders". */
	void SplitLabel(const FText& In, FText& Main, FText& Sub)
	{
		FString L = In.ToString(), R;
		if (L.Split(TEXT(" ("), &L, &R) && R.EndsWith(TEXT(")")))
		{
			R.LeftChopInline(1);
			if (!R.IsEmpty()) R[0] = FChar::ToUpper(R[0]);
			Main = FText::FromString(L);
			Sub = FText::FromString(R);
			return;
		}
		Main = In;
		Sub = FText::GetEmpty();
	}

	FText LabelOf(FName Action)
	{
		const FWRActionKey* A = FWRKeyBindings::Get().Find(Action);
		return A ? A->Label : FText::FromName(Action);
	}

	/** "Ability.fire.fire_bolt" -> its path and tree node (null when the data doesn't have it). */
	const FWRPathNodeDef* AbilityNode(FName Action, FName& OutPath)
	{
		FString PathStr, NodeStr;
		Action.ToString().RightChop(8).Split(TEXT("."), &PathStr, &NodeStr);
		OutPath = FName(*PathStr);
		const FWRPathDef* Def = FWRPathRegistry::Get().Find(OutPath);
		return Def ? Def->FindNode(FName(*NodeStr)) : nullptr;
	}

	// ------------------------------------------------------------ clickable pieces

	/** Left / right click with the mouse captured between press and release (a click lands where it started). */
	class SWRClickBase : public SCompoundWidget
	{
	public:
		virtual FReply OnMouseButtonDown(const FGeometry& Geo, const FPointerEvent& E) override
		{
			const FKey B = E.GetEffectingButton();
			if ((B == EKeys::LeftMouseButton && OnLeft) || (B == EKeys::RightMouseButton && OnRight))
			{
				return FReply::Handled().CaptureMouse(SharedThis(this));
			}
			return FReply::Unhandled();
		}
		virtual FReply OnMouseButtonDoubleClick(const FGeometry& Geo, const FPointerEvent& E) override { return OnMouseButtonDown(Geo, E); }
		virtual FReply OnMouseButtonUp(const FGeometry& Geo, const FPointerEvent& E) override
		{
			if (!HasMouseCapture()) return FReply::Unhandled();
			if (Geo.IsUnderLocation(E.GetScreenSpacePosition()) && IsEnabled())
			{
				WRSound::PlayUI(nullptr, TEXT("ui.click"));
				if (E.GetEffectingButton() == EKeys::LeftMouseButton && OnLeft) OnLeft();
				else if (E.GetEffectingButton() == EKeys::RightMouseButton && OnRight) OnRight();
			}
			return FReply::Handled().ReleaseMouseCapture();
		}
		virtual FCursorReply OnCursorQuery(const FGeometry& Geo, const FPointerEvent& E) const override { return FCursorReply::Cursor(EMouseCursor::Hand); }

	protected:
		TFunction<void()> OnLeft;
		TFunction<void()> OnRight;
	};

	/** A choice in a row of choices (window mode, quality...). */
	class SWRSegment : public SWRClickBase
	{
	public:
		SLATE_BEGIN_ARGS(SWRSegment) : _Height(30.f) {}
			SLATE_ARGUMENT(FText, Text)
			SLATE_ARGUMENT(float, Height)
		SLATE_END_ARGS()
		void Construct(const FArguments& InArgs, TFunction<bool()> InSelected, TFunction<void()> InClick)
		{
			Selected = MoveTemp(InSelected);
			OnLeft = MoveTemp(InClick);
			ChildSlot
			[
				SNew(SBox).HeightOverride(InArgs._Height)
				[
					SNew(SBorder)
					.BorderImage_Lambda([this]() { return Selected() ? WRUI::Box(Hex(TEXT("4D3920")), 3.f, WRUI::Brass, 1.f) : WRUI::Box(IsHovered() ? FLinearColor(1.f, 1.f, 1.f, 0.05f) : FLinearColor::Transparent, 3.f); })
					.Padding(FMargin(12.f * U, 0.f)).HAlign(HAlign_Center).VAlign(VAlign_Center)
					[
						WRUI::Text(InArgs._Text, WRUI::Body(13, 700), TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(Selected() ? WRUI::Ink : IsHovered() ? WRUI::Ink2 : WRUI::Ink3); }))
					]
				]
			];
		}
	private:
		TFunction<bool()> Selected;
	};

	TSharedRef<SWidget> Segmented(const TArray<FText>& Options, TFunction<int32()> Get, TFunction<void(int32)> Set)
	{
		TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
		for (int32 i = 0; i < Options.Num(); ++i)
		{
			Row->AddSlot().AutoWidth().Padding(i > 0 ? 4.f * U : 0.f, 0.f, 0.f, 0.f)
			[
				SNew(SWRSegment, [Get, i]() { return Get() == i; }, [Set, i]() { Set(i); }).Text(Options[i]).Height(30.f * U)
			];
		}
		return SNew(SBorder).BorderImage(WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.5f), 5.f, FLinearColor::Black, 1.f)).Padding(3.f * U)[Row];
	}

	/** A small square button with an arrow (steppers). */
	class SWRArrowButton : public SWRClickBase
	{
	public:
		SLATE_BEGIN_ARGS(SWRArrowButton) : _Left(false) {}
			SLATE_ARGUMENT(bool, Left)
		SLATE_END_ARGS()
		void Construct(const FArguments& InArgs, TFunction<void()> InClick)
		{
			OnLeft = MoveTemp(InClick);
			ChildSlot
			[
				SNew(SBox).WidthOverride(30.f * U).HeightOverride(30.f * U)
				[
					SNew(SBorder).BorderImage_Lambda([this]() { return WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.45f), 4.f, IsHovered() ? WRUI::Brass : FLinearColor::Black, 1.f); })
					.HAlign(HAlign_Center).VAlign(VAlign_Center)
					[
						SNew(SWRLineIcon).Icon(EWRIcon::Chevron).Size(14.f * U).Color(WRUI::Brass)
						.RenderTransform(InArgs._Left ? TOptional<FSlateRenderTransform>(FSlateRenderTransform(FScale2D(-1.f, 1.f))) : TOptional<FSlateRenderTransform>())
						.RenderTransformPivot(FVector2D(0.5f, 0.5f))
					]
				]
			];
		}
	};

	TSharedRef<SWidget> Stepper(TFunction<FText()> Value, TFunction<void(int32)> Step, float Width = 150.f)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[SNew(SWRArrowButton, [Step]() { Step(-1); }).Left(true)]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4.f * U, 0.f)
			[
				SNew(SBox).WidthOverride(Width * U).HeightOverride(30.f * U)
				[
					SNew(SBorder).BorderImage(WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.45f), 4.f, FLinearColor::Black, 1.f)).HAlign(HAlign_Center).VAlign(VAlign_Center)
					[
						WRUI::Text(TAttribute<FText>::CreateLambda([Value]() { return Value(); }), WRUI::Body(13, 700), WRUI::Ink)
					]
				]
			]
			+ SHorizontalBox::Slot().AutoWidth()[SNew(SWRArrowButton, [Step]() { Step(1); })];
	}

	/** An on / off switch. */
	class SWRSwitch : public SWRClickBase
	{
	public:
		SLATE_BEGIN_ARGS(SWRSwitch) {}
		SLATE_END_ARGS()
		void Construct(const FArguments& InArgs, TFunction<bool()> InOn, TFunction<void()> InFlip)
		{
			On = MoveTemp(InOn);
			OnLeft = MoveTemp(InFlip);
			ChildSlot
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f * U, 0.f)
				[
					WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return On() ? LOCTEXT("On", "On") : LOCTEXT("Off", "Off"); }), WRUI::Body(12, 700),
						TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(On() ? WRUI::Brass : WRUI::Ink3); }))
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(52.f * U).HeightOverride(28.f * U)
					[
						SNew(SOverlay)
						+ SOverlay::Slot()[SNew(SImage).Image_Lambda([this]()
						{
							return On() ? WRUI::Box(Hex(TEXT("6A3A1C")), 14.f * U, WRUI::Brass, 1.f) : WRUI::Box(Hex(TEXT("0C0D0F")), 14.f * U, IsHovered() ? WRUI::Brass2 : FLinearColor::Black, 1.f);
						})]
						+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top)
						.Padding(TAttribute<FMargin>::CreateLambda([this]() { return FMargin(On() ? 28.f * U : 4.f * U, 4.f * U, 0.f, 0.f); }))
						[
							SNew(SBox).WidthOverride(20.f * U).HeightOverride(20.f * U)
							[
								SNew(SImage).Image_Lambda([this]() { return WRUI::Box(On() ? Hex(TEXT("F0D49B")) : Hex(TEXT("474C52")), 10.f * U, FLinearColor::Black, 1.f); })
							]
						]
					]
				]
			];
		}
	private:
		TFunction<bool()> On;
	};

	/** A settings row: a name (and a line under it) on the left, a control on the right. */
	TSharedRef<SWidget> SettingRow(const FText& Name, const FText& Sub, TSharedRef<SWidget> Control, float Height = 48.f)
	{
		TSharedRef<SVerticalBox> Names = SNew(SVerticalBox) + SVerticalBox::Slot().AutoHeight()[WRUI::Text(Name, WRUI::Body(14, 700), WRUI::Ink)];
		if (!Sub.IsEmpty())
		{
			Names->AddSlot().AutoHeight()[Muted(Sub, 11)];
		}
		return SNew(SBox).HeightOverride(Height * U)
		[
			SNew(SBorder).BorderImage(RowBrush()).Padding(FMargin(14.f * U, 0.f, 8.f * U, 0.f)).VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Names]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Control]
			]
		];
	}

	// ------------------------------------------------------------ controls page

	class SWRControlsPage;

	/** The box that shows a key: click to listen for a new one, right-click to clear it. */
	class SWRKeyButton : public SWRClickBase
	{
	public:
		SLATE_BEGIN_ARGS(SWRKeyButton) : _Width(110.f), _Height(30.f), _Compact(false) {}
			SLATE_ARGUMENT(FName, Action)
			SLATE_ARGUMENT(float, Width)
			SLATE_ARGUMENT(float, Height)
			/** Just a keycap (the row of eight hotbar keys). */
			SLATE_ARGUMENT(bool, Compact)
		SLATE_END_ARGS()
		void Construct(const FArguments& InArgs, SWRControlsPage* InPage);
	private:
		SWRControlsPage* Page = nullptr;
		FName Action;
		bool bCompact = false;
		bool IsListening() const;
		bool IsLost() const;
		bool IsBound() const { return FWRKeyBindings::Get().Key(Action).IsValid(); }
		const FSlateBrush* Brush() const;
	};

	/** One of the three ways to cast abilities. */
	class SWRCastOption : public SWRClickBase
	{
	public:
		SLATE_BEGIN_ARGS(SWRCastOption) {}
			SLATE_ARGUMENT(FText, Title)
			SLATE_ATTRIBUTE(FText, Text)
		SLATE_END_ARGS()
		void Construct(const FArguments& InArgs, EWRCastMode InMode)
		{
			Mode = InMode;
			OnLeft = [this]() { FWRKeyBindings::Get().SetCastMode(Mode); };
			ChildSlot
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[SNew(SImage).Image(WRUI::Image(TEXT("leather"))).DesiredSizeOverride(FVector2D(1.f, 1.f)).Visibility_Lambda([this]() { return IsOn() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })]
				+ SOverlay::Slot()[SNew(SImage).Image_Lambda([this]()
				{
					return IsOn() ? WRUI::Box(FLinearColor::Transparent, 4.f, WRUI::Brass, 1.f) : WRUI::Box(IsHovered() ? FLinearColor(1.f, 1.f, 1.f, 0.04f) : FLinearColor::Transparent, 4.f, IsHovered() ? WRUI::Alpha(WRUI::Brass2, 0.8f) : FLinearColor::Transparent, 1.f);
				})]
				+ SOverlay::Slot().Padding(FMargin(12.f * U, 10.f * U, 12.f * U, 12.f * U))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 9.f * U, 0.f)
						[
							SNew(SBox).WidthOverride(14.f * U).HeightOverride(14.f * U)
							[
								SNew(SOverlay)
								+ SOverlay::Slot()[SNew(SImage).Image_Lambda([this]() { return WRUI::Box(Hex(TEXT("0C0D0F")), 7.f * U, IsOn() ? WRUI::Brass : WRUI::Brass2, 2.f); })]
								+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
								[
									SNew(SBox).WidthOverride(6.f * U).HeightOverride(6.f * U).Visibility_Lambda([this]() { return IsOn() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
									[
										SNew(SImage).Image(WRUI::Box(Hex(TEXT("F0D49B")), 3.f * U))
									]
								]
							]
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(InArgs._Title, Spaced(WRUI::Title(14), 60), WRUI::Ink, true)]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 5.f * U, 0.f, 0.f)
					[
						WRUI::Text(InArgs._Text, WRUI::Body(12, 500), TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(IsOn() ? WRUI::Ink2 : WRUI::Ink3); }), false, true)
					]
				]
			];
		}
	private:
		EWRCastMode Mode = EWRCastMode::Both;
		bool IsOn() const { return FWRKeyBindings::Get().GetCastMode() == Mode; }
	};

	class SWRControlsPage : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SWRControlsPage) : _InGame(true) {}
			SLATE_ARGUMENT(TWeakObjectPtr<APlayerController>, PC)
			SLATE_ARGUMENT(bool, InGame)
		SLATE_END_ARGS()

		enum class EBar : uint8 { None, Listening, Conflict, Replaced, Reset };

		void Construct(const FArguments& InArgs)
		{
			PC = InArgs._PC;
			bInGame = InArgs._InGame;
			ChildSlot
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(572.f * U)[AbilitiesColumn()]]
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(28.f * U, 0.f, 0.f, 0.f)[KeysColumn()]
			];
			RebuildAbilities();
		}

		virtual bool SupportsKeyboardFocus() const override { return true; }

		bool IsListeningTo(FName Id) const { return Listening == Id && !Id.IsNone(); }
		bool IsLost(FName Id) const { return Lost == Id && !Id.IsNone() && !FWRKeyBindings::Get().Key(Id).IsValid(); }
		/** Listening for a key or asking about a clash: Esc belongs to this page then. */
		bool IsBusy() const { return Bar == EBar::Listening || Bar == EBar::Conflict; }

		void Listen(FName Id)
		{
			if (!FWRKeyBindings::Get().Find(Id)) return;
			Listening = Id;
			Pending = FPending();
			Bar = EBar::Listening;
			// The wheel is a key too: scroll boxes mustn't eat it while we listen.
			for (const TSharedPtr<SScrollBox>& S : Scrolls) if (S.IsValid()) S->SetConsumeMouseWheel(EConsumeMouseWheel::Never);
			FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EFocusCause::SetDirectly);
		}

		void Clear(FName Id)
		{
			StopListening();
			FWRKeyBindings::Get().Unbind(Id);
			Bar = EBar::None;
		}

		/** A key or mouse button arrived while listening. */
		void Capture(const FKey& Key)
		{
			const FName Id = Listening;
			StopListening();
			FWRKeyBindings& Keys = FWRKeyBindings::Get();
			if (Id.IsNone() || Keys.Key(Id) == Key)
			{
				Bar = EBar::None;
				return;
			}
			const FName Owner = Keys.WhoUses(Key, Id);
			if (Owner.IsNone())
			{
				Keys.Bind(Id, Key);
				Bar = EBar::None;
				return;
			}
			// In use: ask first (Replace calls Bind, which takes the key from the other action).
			Pending.Action = Id;
			Pending.Key = Key;
			Pending.Owner = Owner;
			Bar = EBar::Conflict;
		}

		/** Testing: scroll the key list. */
		void ScrollKeys(float Offset) { if (KeysScroll.IsValid()) KeysScroll->SetScrollOffset(Offset); }

		void CancelAll()
		{
			StopListening();
			Pending = FPending();
			Bar = EBar::None;
		}

		void ResetAll()
		{
			CancelAll();
			FWRKeyBindings::Get().ResetAll();
			Lost = NAME_None;
			Bar = EBar::Reset;
		}

		virtual FReply OnPreviewKeyDown(const FGeometry& Geo, const FKeyEvent& E) override
		{
			const FKey Key = E.GetKey();
			if (Bar == EBar::Listening)
			{
				if (Key == EKeys::Escape) CancelAll();
				else if (Key.IsValid() && !Key.IsGamepadKey() && !Key.IsTouch()) Capture(Key);   // keyboard and mouse only
				return FReply::Handled();
			}
			if (Bar == EBar::Conflict && Key == EKeys::Escape)
			{
				CancelAll();
				return FReply::Handled();
			}
			return FReply::Unhandled();
		}

		virtual FReply OnPreviewMouseButtonDown(const FGeometry& Geo, const FPointerEvent& E) override
		{
			if (Bar != EBar::Listening) return FReply::Unhandled();
			// The bar's own Unbind / Cancel buttons still work; any other click is the new key.
			if (BarButtons.IsValid() && BarButtons->GetTickSpaceGeometry().IsUnderLocation(E.GetScreenSpacePosition())) return FReply::Unhandled();
			Capture(E.GetEffectingButton());
			return FReply::Handled();
		}

		virtual FReply OnMouseWheel(const FGeometry& Geo, const FPointerEvent& E) override
		{
			if (Bar != EBar::Listening) return FReply::Unhandled();
			Capture(E.GetWheelDelta() > 0.f ? EKeys::MouseScrollUp : EKeys::MouseScrollDown);
			return FReply::Handled();
		}

		virtual void Tick(const FGeometry& Geo, const double Time, const float Delta) override
		{
			SCompoundWidget::Tick(Geo, Time, Delta);
			// Abilities get their keys as they're learned (FWRPlayerUI ensures them): rebuild the table when that changes.
			if (Time >= NextCheck)
			{
				NextCheck = Time + 0.5;
				if (AbilitySignature() != Signature) RebuildAbilities();
			}
		}

	private:
		struct FPending { FName Action; FKey Key; FName Owner; };
		struct FLocked { FName Path; FName Node; FText Name; };

		TWeakObjectPtr<APlayerController> PC;
		bool bInGame = true;
		FName Listening;
		FPending Pending;
		FPending Replaced;
		FName Lost;
		EBar Bar = EBar::None;
		TSharedPtr<SWidget> BarButtons;
		TSharedPtr<SVerticalBox> AbilityRows;
		TArray<TSharedPtr<SScrollBox>> Scrolls;
		TSharedPtr<SScrollBox> KeysScroll;
		FString Signature;
		double NextCheck = 0.0;

		void StopListening()
		{
			Listening = NAME_None;
			for (const TSharedPtr<SScrollBox>& S : Scrolls) if (S.IsValid()) S->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
		}

		void Replace()
		{
			const FName Loser = FWRKeyBindings::Get().Bind(Pending.Action, Pending.Key);
			Replaced = Pending;
			Replaced.Owner = Loser;
			Lost = Loser;
			Pending = FPending();
			Bar = Loser.IsNone() ? EBar::None : EBar::Replaced;
		}

		FText CastText(EWRCastMode Mode) const
		{
			const FWRKeyBindings& K = FWRKeyBindings::Get();
			switch (Mode)
			{
			case EWRCastMode::Wheel: return FText::Format(LOCTEXT("CastWheel", "Hold {0} or {1}, pick with the mouse, tap to cast."), WRUI::KeyName(K.Key(TEXT("Wheel1"))), WRUI::KeyName(K.Key(TEXT("Wheel2"))));
			case EWRCastMode::Keys: return LOCTEXT("CastKeys", "Bind each ability to a key.");
			default: return LOCTEXT("CastBoth", "Wheels plus direct keys.");
			}
		}

		TArray<FLocked> LockedAbilities() const
		{
			// Abilities of your paths you haven't learned yet (in game: the main menu has no character of yours).
			TArray<FLocked> Out;
			const APawn* Pawn = bInGame && PC.IsValid() ? PC->GetPawn() : nullptr;
			const UWRPathComponent* Paths = Pawn ? UWRPathComponent::FindFor(Pawn) : nullptr;
			if (!Paths) return Out;
			for (const FName PathId : { Paths->GetMainPath(), Paths->GetSubPath() })
			{
				const FWRPathDef* Def = PathId.IsNone() ? nullptr : FWRPathRegistry::Get().Find(PathId);
				if (!Def) continue;
				for (const FWRPathNodeDef& Node : Def->Nodes)
				{
					const FName Full(*FString::Printf(TEXT("%s.%s"), *PathId.ToString(), *Node.Id.ToString()));
					if (Node.Kind == EWRNodeKind::Stat || !UWRAbilityRegistry::ClassForNode(PathId, Node.Id)) continue;
					if (FWRKeyBindings::Get().Find(FWRKeyBindings::AbilityAction(Full)) || UWRPathComponent::HasNode(Pawn, PathId, Node.Id)) continue;
					Out.Add({ PathId, Node.Id, FText::FromString(Node.Name) });
				}
			}
			return Out;
		}

		FString AbilitySignature() const
		{
			FString Sig;
			for (const FWRActionKey& A : FWRKeyBindings::Get().Actions())
			{
				if (A.Group == GAbilities) Sig += A.Id.ToString() + TEXT(",");
			}
			Sig += TEXT("|");
			for (const FLocked& L : LockedAbilities()) Sig += L.Node.ToString() + TEXT(",");
			return Sig;
		}

		// -------- left column: abilities

		TSharedRef<SWidget> AbilitiesColumn()
		{
			const auto NotWheel = [](bool bShow) { return [bShow]() { return (FWRKeyBindings::Get().GetCastMode() != EWRCastMode::Wheel) == bShow ? EVisibility::Visible : EVisibility::Collapsed; }; };
			return SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[Heading(LOCTEXT("Abilities", "Abilities"))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f * U, 0.f, 0.f)[Label(LOCTEXT("HowCast", "How you cast abilities"))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f * U, 0.f, 0.f)
				[
					SNew(SBorder).BorderImage(InsetBrush()).Padding(6.f * U)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f)[SNew(SWRCastOption, EWRCastMode::Wheel).Title(LOCTEXT("ModeWheel", "Wheel")).Text_Lambda([this]() { return CastText(EWRCastMode::Wheel); })]
						+ SHorizontalBox::Slot().FillWidth(1.f).Padding(6.f * U, 0.f)[SNew(SWRCastOption, EWRCastMode::Keys).Title(LOCTEXT("ModeKeys", "Own keys")).Text_Lambda([this]() { return CastText(EWRCastMode::Keys); })]
						+ SHorizontalBox::Slot().FillWidth(1.f)[SNew(SWRCastOption, EWRCastMode::Both).Title(LOCTEXT("ModeBoth", "Both")).Text_Lambda([this]() { return CastText(EWRCastMode::Both); })]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f * U, 0.f, 0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 5.f * U, 0.f)[WheelKeyRow(TEXT("Wheel1"))]
					+ SHorizontalBox::Slot().FillWidth(1.f).Padding(5.f * U, 0.f, 0.f, 0.f)[WheelKeyRow(TEXT("Wheel2"))]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f * U, 0.f, 0.f)
				[
					SettingRow(LOCTEXT("CastRelease", "Cast on release (skip the tap)"), LOCTEXT("CastReleaseSub", "Letting go of the wheel on an ability casts it right away."),
						SNew(SWRSwitch, []() { return FWRKeyBindings::Get().CastOnRelease(); }, []() { FWRKeyBindings::Get().SetCastOnRelease(!FWRKeyBindings::Get().CastOnRelease()); }))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f * U, 0.f, 0.f)
				[
					SNew(SHorizontalBox).Visibility_Lambda(NotWheel(true))
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Bottom)[Label(LOCTEXT("AbilityKeys", "Ability keys"))]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)[Muted(LOCTEXT("AbilityKeysSub", "Used by Own keys and Both"))]
				]
				+ SVerticalBox::Slot().FillHeight(1.f).Padding(0.f, 10.f * U, 0.f, 0.f)
				[
					SNew(SWidgetSwitcher).WidgetIndex_Lambda([]() { return FWRKeyBindings::Get().GetCastMode() == EWRCastMode::Wheel ? 1 : 0; })
					+ SWidgetSwitcher::Slot()[AbilityTable()]
					+ SWidgetSwitcher::Slot()
					[
						SNew(SBorder).BorderImage(InsetBrush()).Padding(FMargin(18.f * U, 16.f * U)).VAlign(VAlign_Top)
						[
							WRUI::Text(TAttribute<FText>::CreateLambda([]()
							{
								const FWRKeyBindings& K = FWRKeyBindings::Get();
								return FText::Format(LOCTEXT("WheelOnly", "Abilities are cast from the wheels: hold {0} or {1} and pick one. Choose Own keys or Both to give each ability a key of its own as well."),
									WRUI::KeyName(K.Key(TEXT("Wheel1"))), WRUI::KeyName(K.Key(TEXT("Wheel2"))));
							}), WRUI::Body(13, 500), WRUI::Ink2, false, true)
						]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f * U, 0.f, 0.f)[MakeBar()];
		}

		TSharedRef<SWidget> WheelKeyRow(FName Id)
		{
			return SNew(SBox).HeightOverride(44.f * U)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[SNew(SImage).Image(RowBrush())]
				+ SOverlay::Slot()[ListeningGlow(Id)]
				+ SOverlay::Slot().Padding(FMargin(14.f * U, 0.f, 6.f * U, 0.f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Ellipsis(LabelOf(Id), WRUI::Body(14, 700), WRUI::Ink)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SWRKeyButton, this).Action(Id).Width(104.f * U).Height(32.f * U)]
				]
			];
		}

		/** The ember wash on the row whose key we're listening for. */
		TSharedRef<SWidget> ListeningGlow(FName Id)
		{
			return SNew(SOverlay).Visibility_Lambda([this, Id]() { return IsListeningTo(Id) || (Bar == EBar::Conflict && Pending.Action == Id) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				+ SOverlay::Slot()[SNew(SWRGradientFill).Colors({ WRUI::Alpha(WRUI::Ember, 0.22f), WRUI::Alpha(WRUI::Ember, 0.05f) })]
				+ SOverlay::Slot()[SNew(SImage).Image(WRUI::Box(FLinearColor::Transparent, 2.f, WRUI::Alpha(WRUI::Brass, 0.55f), 1.f))]
				+ SOverlay::Slot().HAlign(HAlign_Left)[SNew(SBox).WidthOverride(3.f * U)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity(WRUI::Ember)]];
		}

		TSharedRef<SWidget> AbilityTable()
		{
			TSharedPtr<SScrollBox> Scroll;
			TSharedRef<SWidget> Table = SNew(SBorder).BorderImage(InsetBrush()).Padding(0.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SBox).HeightOverride(28.f * U)
					[
						SNew(SOverlay)
						+ SOverlay::Slot().VAlign(VAlign_Bottom)[HLine(WRUI::Alpha(WRUI::Brass2, 0.45f))]
						+ SOverlay::Slot().Padding(FMargin(14.f * U, 0.f, 12.f * U, 0.f))
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Label(LOCTEXT("ColAbility", "Ability"), 11)]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(224.f * U).HAlign(HAlign_Center)[Label(LOCTEXT("ColKey", "Key"), 11)]]
						]
					]
				]
				+ SVerticalBox::Slot().FillHeight(1.f)
				[
					SAssignNew(Scroll, SScrollBox).ScrollBarStyle(&ScrollStyle()).ScrollBarThickness(FVector2D(5.f, 5.f))
					+ SScrollBox::Slot()[SAssignNew(AbilityRows, SVerticalBox)]
				]
			];
			Scrolls.Add(Scroll);
			return Table;
		}

		void RebuildAbilities()
		{
			Signature = AbilitySignature();
			if (!AbilityRows.IsValid()) return;
			AbilityRows->ClearChildren();
			int32 N = 0;
			for (const FWRActionKey& A : FWRKeyBindings::Get().Actions())
			{
				if (A.Group != GAbilities) continue;
				AbilityRows->AddSlot().AutoHeight()[AbilityRow(A.Id)];
				++N;
			}
			const TArray<FLocked> Locked = LockedAbilities();
			for (const FLocked& L : Locked)
			{
				AbilityRows->AddSlot().AutoHeight()[LockedRow(L)];
			}
			if (N == 0)
			{
				AbilityRows->AddSlot().AutoHeight().Padding(FMargin(14.f * U, 12.f * U))
				[
					WRUI::Text(bInGame ? LOCTEXT("NoAbilities", "Learn abilities in Paths: each one you learn gets a row here for its own key.")
						: LOCTEXT("NoAbilitiesMenu", "Your abilities get a row here once you've learned them in a game."), WRUI::Body(13, 500), WRUI::Ink3, false, true)
				];
			}
		}

		TSharedRef<SWidget> Disc(const FLinearColor& Tint, TSharedRef<SWidget> Inside, TFunction<bool()> Lit)
		{
			return SNew(SBox).WidthOverride(28.f * U).HeightOverride(28.f * U)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[SNew(SImage).Image_Lambda([Tint, Lit]() { return WRUI::Box(Tint, 14.f * U, Lit() ? Hex(TEXT("F0B068")) : WRUI::Brass2, 1.f); })]
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[Inside]
			];
		}

		TSharedRef<SWidget> AbilityRow(FName Id)
		{
			FName Path;
			const FWRPathNodeDef* Node = AbilityNode(Id, Path);
			const bool bFrost = Path == TEXT("frost");
			const FLinearColor Col = bFrost ? WRUI::Frost : WRUI::Ember;
			const FText Name = LabelOf(Id);
			const FText Sub = Node ? FText::FromString(Node->Text) : FText::GetEmpty();
			const FString Glyph = Name.ToString().Left(1);
			return SNew(SBox).HeightOverride(38.f * U)
			[
				SNew(SOverlay)
				+ SOverlay::Slot().VAlign(VAlign_Bottom)[HLine(FLinearColor(1.f, 1.f, 1.f, 0.05f))]
				+ SOverlay::Slot()[ListeningGlow(Id)]
				+ SOverlay::Slot().Padding(FMargin(14.f * U, 0.f, 12.f * U, 0.f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						Disc(bFrost ? Hex(TEXT("0F1E2C")) : Hex(TEXT("301A0F")), WRUI::Text(FText::FromString(Glyph), WRUI::Title(12), Col), [this, Id]() { return IsListeningTo(Id); })
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f * U, 0.f, 0.f, 0.f)[WRUI::Text(Name, WRUI::Body(14, 700), WRUI::Ink)]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(8.f * U, 2.f * U, 10.f * U, 0.f)[Ellipsis(Sub, WRUI::Body(11, 500), WRUI::Ink3)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SWRKeyButton, this).Action(Id).Width(224.f * U).Height(30.f * U)]
				]
			];
		}

		TSharedRef<SWidget> LockedRow(const FLocked& L)
		{
			return SNew(SBox).HeightOverride(38.f * U).RenderOpacity(0.6f)
			[
				SNew(SOverlay)
				+ SOverlay::Slot().VAlign(VAlign_Bottom)[HLine(FLinearColor(1.f, 1.f, 1.f, 0.05f))]
				+ SOverlay::Slot().Padding(FMargin(14.f * U, 0.f, 12.f * U, 0.f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						Disc(Hex(TEXT("1E2023")), SNew(SWRLineIcon).Icon(EWRIcon::Lock).Size(14.f * U).Color(WRUI::Ink3), []() { return false; })
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f * U, 0.f, 0.f, 0.f)[WRUI::Text(L.Name, WRUI::Body(14, 700), WRUI::Ink2)]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(8.f * U, 2.f * U, 10.f * U, 0.f)[Ellipsis(LOCTEXT("LockedSub", "Locked · learn it in Paths"), WRUI::Body(11, 500), WRUI::Ink3)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SBox).WidthOverride(224.f * U).HeightOverride(30.f * U)
						[
							SNew(SBorder).BorderImage(WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.3f), 4.f, FLinearColor(0.f, 0.f, 0.f, 0.8f), 1.f)).HAlign(HAlign_Center).VAlign(VAlign_Center)
							[
								WRUI::Text(LOCTEXT("Locked", "Locked"), WRUI::Body(12, 500), WRUI::Ink3)
							]
						]
					]
				]
			];
		}

		// -------- the bar under the abilities: listening, a clash, what a replace did

		TSharedRef<SWidget> BarButton(const FText& Text, bool bPrimary, TFunction<void()> Click)
		{
			return SNew(SWRButton).Text(Text).Primary(bPrimary).Ghost(!bPrimary).FontSize(12).MinHeight(34.f * U)
				.OnClicked_Lambda([Click]() { Click(); return FReply::Handled(); });
		}

		int32 BarIndex() const { return Bar == EBar::Listening ? 0 : Bar == EBar::Conflict ? 1 : 2; }

		TSharedRef<SWidget> MakeBar()
		{
			return SNew(SBorder)
				.Visibility_Lambda([this]() { return Bar == EBar::None ? EVisibility::Collapsed : EVisibility::Visible; })
				.BorderImage_Lambda([this]()
				{
					return Bar == EBar::Listening ? WRUI::Box(WRUI::Alpha(Hex(TEXT("140803")), 0.9f), 4.f, WRUI::Alpha(WRUI::Ember, 0.85f), 1.f)
						: Bar == EBar::Conflict ? WRUI::Box(WRUI::Alpha(WRUI::Warn, 0.16f), 4.f, WRUI::Alpha(WRUI::Warn, 0.75f), 1.f)
						: WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.4f), 4.f, WRUI::Alpha(WRUI::Brass2, 0.9f), 1.f);
				})
				.Padding(FMargin(14.f * U, 8.f * U, 10.f * U, 8.f * U))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f * U, 0.f)
					[
						SNew(SWidgetSwitcher).WidgetIndex_Lambda([this]() { return BarIndex(); })
						+ SWidgetSwitcher::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
						[
							SNew(SBox).WidthOverride(22.f * U).HAlign(HAlign_Center)
							[
								SNew(SBox).WidthOverride(9.f * U).HeightOverride(9.f * U)[SNew(SImage).Image(WRUI::Box(WRUI::Ember, 4.5f * U))]
							]
						]
						+ SWidgetSwitcher::Slot()[SNew(SWRLineIcon).Icon(EWRIcon::Warning).Size(22.f * U).Color(WRUI::Warn)]
						+ SWidgetSwitcher::Slot()[SNew(SWRLineIcon).Icon(EWRIcon::Info).Size(22.f * U).Color(WRUI::Brass)]
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SWidgetSwitcher).WidgetIndex_Lambda([this]() { return BarIndex(); })
							+ SWidgetSwitcher::Slot()
							[
								WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::Format(LOCTEXT("PressFor", "Press a key or mouse button for {0}"), LabelOf(Listening)); }),
									WRUI::Body(14, 700), Hex(TEXT("F4A35C")), false, true)
							]
							+ SWidgetSwitcher::Slot()
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 5.f * U, 0.f)[SNew(SWRKeyCap).FontSize(11).Text_Lambda([this]() { return WRUI::KeyName(Pending.Key); })]
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(LOCTEXT("IsAlready", "is already "), WRUI::Body(14, 700), WRUI::Ink)]
								+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
								[
									Ellipsis(TAttribute<FText>::CreateLambda([this]() { FText M, S; SplitLabel(LabelOf(Pending.Owner), M, S); return M; }), WRUI::Body(14, 700), WRUI::Warn)
								]
							]
							+ SWidgetSwitcher::Slot()
							[
								WRUI::Text(TAttribute<FText>::CreateLambda([this]()
								{
									if (Bar == EBar::Reset) return LOCTEXT("ResetDone", "Every key is back to its default.");
									FText M, S;
									SplitLabel(LabelOf(Replaced.Owner), M, S);
									return FText::Format(LOCTEXT("Replaced", "{0} is now {1}. {2} has no key."), WRUI::KeyName(Replaced.Key), LabelOf(Replaced.Action), M);
								}), WRUI::Body(14, 700), WRUI::Ink, false, true)
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f * U, 0.f, 0.f)
						[
							Muted(TAttribute<FText>::CreateLambda([this]()
							{
								switch (Bar)
								{
								case EBar::Listening: return LOCTEXT("ListenSub", "Esc cancels. Mouse buttons and the wheel count too.");
								case EBar::Conflict: { FText M, S; SplitLabel(LabelOf(Pending.Owner), M, S); return FText::Format(LOCTEXT("ConflictSub", "Replace it? {0} will be left without a key."), M); }
								case EBar::Replaced: return LOCTEXT("ReplacedSub", "Click its key to give it a new one.");
								case EBar::Reset: return LOCTEXT("ResetSub", "Cast mode Both, cast on release off. Ability keys are cleared.");
								default: return FText::GetEmpty();
								}
							}), 11, true)
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f * U, 0.f, 0.f, 0.f)
					[
						SAssignNew(BarButtons, SWidgetSwitcher).WidgetIndex_Lambda([this]() { return BarIndex(); })
						+ SWidgetSwitcher::Slot()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth()[BarButton(LOCTEXT("Unbind", "Unbind"), false, [this]() { Clear(Listening); })]
							+ SHorizontalBox::Slot().AutoWidth().Padding(8.f * U, 0.f, 0.f, 0.f)[BarButton(LOCTEXT("Cancel", "Cancel"), false, [this]() { CancelAll(); })]
						]
						+ SWidgetSwitcher::Slot()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth()[BarButton(LOCTEXT("Replace", "Replace"), true, [this]() { Replace(); })]
							+ SHorizontalBox::Slot().AutoWidth().Padding(8.f * U, 0.f, 0.f, 0.f)[BarButton(LOCTEXT("Cancel", "Cancel"), false, [this]() { CancelAll(); })]
						]
						+ SWidgetSwitcher::Slot()[BarButton(LOCTEXT("Ok", "OK"), false, [this]() { Bar = EBar::None; })]
					]
				];
		}

		// -------- right column: every other key, by group

		TSharedRef<SWidget> KeysColumn()
		{
			TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
			const TPair<FName, FText> Groups[] = {
				{ GMovement, LOCTEXT("GroupMovement", "Movement") },
				{ GCombat, LOCTEXT("GroupCombat", "Combat") },
				{ GItems, LOCTEXT("GroupItems", "Hotbar and items") },
				{ GMenus, LOCTEXT("GroupMenus", "Menus") },
			};
			for (const TPair<FName, FText>& G : Groups)
			{
				List->AddSlot().AutoHeight()[GroupHeader(G.Value)];
				for (const FWRActionKey& A : FWRKeyBindings::Get().Actions())
				{
					if (A.Group != G.Key) continue;
					const FString Id = A.Id.ToString();
					// The wheel keys sit with the abilities; hotbar 2-8 and "previous" share a row with their first.
					if (Id == TEXT("Wheel1") || Id == TEXT("Wheel2") || Id == TEXT("HotbarPrev") || (Id.StartsWith(TEXT("Hotbar")) && Id != TEXT("Hotbar1") && Id != TEXT("HotbarNext"))) continue;
					if (Id == TEXT("Hotbar1")) List->AddSlot().AutoHeight()[HotbarRow()];
					else if (Id == TEXT("HotbarNext")) List->AddSlot().AutoHeight()[NextPrevRow()];
					else List->AddSlot().AutoHeight()[KeyRow(A.Id)];
				}
			}
			TSharedPtr<SScrollBox> Scroll;
			TSharedRef<SWidget> Column = SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[Heading(LOCTEXT("AllKeys", "Keys"))]
				+ SVerticalBox::Slot().FillHeight(1.f).Padding(0.f, 10.f * U, 0.f, 0.f)
				[
					SNew(SBorder).BorderImage(InsetBrush()).Padding(FMargin(0.f, 0.f, 2.f, 0.f))
					[
						SAssignNew(Scroll, SScrollBox).ScrollBarStyle(&ScrollStyle()).ScrollBarThickness(FVector2D(5.f, 5.f))
						+ SScrollBox::Slot()[List]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f * U, 0.f, 0.f)[Muted(LOCTEXT("KeysHint", "Click a key to change it. Right-click to clear it."), 12)];
			Scrolls.Add(Scroll);
			KeysScroll = Scroll;
			return Column;
		}

		TSharedRef<SWidget> GroupHeader(const FText& Name)
		{
			return SNew(SBox).HeightOverride(30.f * U)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[SNew(SImage).Image(WRUI::White()).ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.22f))]
				+ SOverlay::Slot().VAlign(VAlign_Bottom)[HLine(WRUI::Alpha(WRUI::Brass2, 0.45f))]
				+ SOverlay::Slot().Padding(FMargin(14.f * U, 0.f)).VAlign(VAlign_Center)[Label(Name, 12, WRUI::Brass)]
			];
		}

		TSharedRef<SWidget> RowFrame(TFunction<bool()> Lit, TSharedRef<SWidget> Content)
		{
			return SNew(SBox).HeightOverride(40.f * U)
			[
				SNew(SOverlay)
				+ SOverlay::Slot().VAlign(VAlign_Bottom)[HLine(FLinearColor(1.f, 1.f, 1.f, 0.05f))]
				+ SOverlay::Slot()
				[
					SNew(SOverlay).Visibility_Lambda([Lit]() { return Lit() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
					+ SOverlay::Slot()[SNew(SWRGradientFill).Colors({ WRUI::Alpha(WRUI::Ember, 0.22f), WRUI::Alpha(WRUI::Ember, 0.05f) })]
					+ SOverlay::Slot().HAlign(HAlign_Left)[SNew(SBox).WidthOverride(3.f * U)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity(WRUI::Ember)]]
				]
				+ SOverlay::Slot().Padding(FMargin(14.f * U, 0.f, 10.f * U, 0.f))[Content]
			];
		}

		bool IsBusyWith(FName Id) const { return IsListeningTo(Id) || (Bar == EBar::Conflict && Pending.Action == Id); }

		TSharedRef<SWidget> KeyRow(FName Id)
		{
			FText Main, Sub;
			SplitLabel(LabelOf(Id), Main, Sub);
			return RowFrame([this, Id]() { return IsBusyWith(Id); },
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(Main, WRUI::Body(14, 700), WRUI::Ink)]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(8.f * U, 2.f * U, 8.f * U, 0.f)[Ellipsis(Sub, WRUI::Body(11, 500), WRUI::Ink3)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SWRKeyButton, this).Action(Id).Width(110.f * U).Height(30.f * U)]);
		}

		TSharedRef<SWidget> HotbarRow()
		{
			TSharedRef<SHorizontalBox> Caps = SNew(SHorizontalBox);
			for (int32 i = 1; i <= 8; ++i)
			{
				Caps->AddSlot().AutoWidth().Padding(i > 1 ? 4.f * U : 0.f, 0.f, 0.f, 0.f)[SNew(SWRKeyButton, this).Action(FName(*FString::Printf(TEXT("Hotbar%d"), i))).Height(26.f * U).Compact(true)];
			}
			return RowFrame([this]() { for (int32 i = 1; i <= 8; ++i) if (IsBusyWith(FName(*FString::Printf(TEXT("Hotbar%d"), i)))) return true; return false; },
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[WRUI::Text(LOCTEXT("HotbarSlots", "Hotbar slots"), WRUI::Body(14, 700), WRUI::Ink)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Caps]);
		}

		TSharedRef<SWidget> NextPrevRow()
		{
			return RowFrame([this]() { return IsBusyWith(TEXT("HotbarNext")) || IsBusyWith(TEXT("HotbarPrev")); },
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(LOCTEXT("NextPrev", "Next / previous slot"), WRUI::Body(14, 700), WRUI::Ink)]
				+ SHorizontalBox::Slot().FillWidth(1.f)[SNullWidget::NullWidget]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SWRKeyButton, this).Action(TEXT("HotbarNext")).Width(92.f * U).Height(30.f * U)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.f * U, 0.f, 0.f, 0.f)[SNew(SWRKeyButton, this).Action(TEXT("HotbarPrev")).Width(92.f * U).Height(30.f * U)]);
		}
	};

	void SWRKeyButton::Construct(const FArguments& InArgs, SWRControlsPage* InPage)
	{
		Page = InPage;
		Action = InArgs._Action;
		bCompact = InArgs._Compact;
		const FName Id = Action;
		OnLeft = [this]() { if (Page) Page->Listen(Action); };
		OnRight = [this]() { if (Page) Page->Clear(Action); };
		const bool bWide = InArgs._Width >= 200.f;
		const FText ListenText = bWide ? LOCTEXT("PressKeyWide", "Press a key… (Esc to cancel)") : bCompact ? FText::GetEmpty() : LOCTEXT("PressKey", "Press a key…");

		TSharedRef<SWidget> Bound = bCompact
			? WRUI::Text(TAttribute<FText>::CreateLambda([Id]() { return WRUI::KeyName(FWRKeyBindings::Get().Key(Id)); }), WRUI::Body(11, 800), WRUI::Ink)
			: StaticCastSharedRef<SWidget>(SNew(SWRKeyCap).FontSize(12).Text_Lambda([Id]() { return WRUI::KeyName(FWRKeyBindings::Get().Key(Id)); }));
		ChildSlot
		[
			SNew(SBox).MinDesiredWidth(bCompact ? InArgs._Height : InArgs._Width).HeightOverride(InArgs._Height)
			[
				SNew(SBorder)
				.BorderImage_Lambda([this]() { return Brush(); })
				.Padding(FMargin(bCompact ? 5.f * U : 10.f * U, 0.f)).HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					SNew(SWidgetSwitcher).WidgetIndex_Lambda([this]() { return IsListening() ? 1 : IsBound() ? 0 : 2; })
					+ SWidgetSwitcher::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[Bound]
					+ SWidgetSwitcher::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(SBox).WidthOverride(7.f * U).HeightOverride(7.f * U)[SNew(SImage).Image(WRUI::Box(WRUI::Ember, 3.5f * U))]
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(ListenText.IsEmpty() ? 0.f : 8.f * U, 0.f, 0.f, 0.f)
						[
							WRUI::Text(ListenText, WRUI::Body(12, 700), Hex(TEXT("F4A35C")))
						]
					]
					+ SWidgetSwitcher::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
					[
						WRUI::Text(bCompact ? FText::FromString(TEXT("-")) : LOCTEXT("NotSet", "Not set"), WRUI::Body(12, 500),
							TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(IsLost() ? WRUI::Warn : WRUI::Ink3); }))
					]
				]
			]
		];
	}

	bool SWRKeyButton::IsListening() const { return Page && Page->IsListeningTo(Action); }
	bool SWRKeyButton::IsLost() const { return Page && Page->IsLost(Action); }

	const FSlateBrush* SWRKeyButton::Brush() const
	{
		if (IsListening()) return WRUI::Box(WRUI::Alpha(Hex(TEXT("140803")), 0.9f), 4.f, WRUI::Ember, 1.5f);
		const bool bHover = IsHovered();
		if (bCompact)
		{
			return WRUI::Box(IsBound() ? FLinearColor(0.16f, 0.175f, 0.19f) : FLinearColor(0.f, 0.f, 0.f, 0.3f), 4.f, bHover ? WRUI::Brass : FLinearColor::Black, 1.f);
		}
		if (!IsBound()) return WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.25f), 4.f, IsLost() ? WRUI::Warn : bHover ? WRUI::Brass2 : WRUI::Alpha(WRUI::Ink3, 0.55f), 1.f);
		return WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.45f), 4.f, bHover ? WRUI::Brass2 : FLinearColor::Black, 1.f);
	}

	// ------------------------------------------------------------ audio page

	/** A volume slider: click or drag along it; a soft click plays at the new level when you let go. */
	class SWRVolumeBar : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SWRVolumeBar) {}
		SLATE_END_ARGS()
		void Construct(const FArguments& InArgs, FName InCategory) { Category = InCategory; }
		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(320.f * U, 30.f * U); }
		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override
		{
			const FVector2D Size = Geo.GetLocalSize();
			const float Knob = 18.f * U, TrackH = 8.f * U;
			const float Track = FMath::Max(1.f, static_cast<float>(Size.X) - Knob);
			const float Y = (Size.Y - TrackH) * 0.5f;
			const float F = WRSound::GetVolume(Category);
			const bool bHot = IsHovered() || HasMouseCapture();
			WRUI::DrawBox(Out, Layer, Geo.ToPaintGeometry(FVector2D(Size.X, TrackH), FSlateLayoutTransform(FVector2D(0.f, Y))), WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.5f), 4.f, FLinearColor::Black, 1.f));
			if (F > 0.f)
			{
				WRUI::DrawBox(Out, Layer + 1, Geo.ToPaintGeometry(FVector2D(Knob * 0.5f + Track * F, TrackH - 2.f), FSlateLayoutTransform(FVector2D(1.f, Y + 1.f))), WRUI::Box(Hex(TEXT("6A3A1C")), 3.f, WRUI::Brass2, 1.f));
			}
			WRUI::DrawBox(Out, Layer + 2, Geo.ToPaintGeometry(FVector2D(Knob, Knob), FSlateLayoutTransform(FVector2D(Track * F, (Size.Y - Knob) * 0.5f))),
				WRUI::Box(bHot ? Hex(TEXT("F0D49B")) : WRUI::Brass, Knob * 0.5f, FLinearColor::Black, 1.f));
			return Layer + 2;
		}
		virtual FReply OnMouseButtonDown(const FGeometry& Geo, const FPointerEvent& E) override
		{
			if (E.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
			SetFrom(Geo, E, false);
			return FReply::Handled().CaptureMouse(SharedThis(this));
		}
		virtual FReply OnMouseMove(const FGeometry& Geo, const FPointerEvent& E) override
		{
			if (!HasMouseCapture()) return FReply::Unhandled();
			SetFrom(Geo, E, false);
			return FReply::Handled();
		}
		virtual FReply OnMouseButtonUp(const FGeometry& Geo, const FPointerEvent& E) override
		{
			if (!HasMouseCapture()) return FReply::Unhandled();
			SetFrom(Geo, E, true);
			WRSound::PlayUI(nullptr, TEXT("ui.click"));   // hear the new level
			return FReply::Handled().ReleaseMouseCapture();
		}
		virtual FCursorReply OnCursorQuery(const FGeometry& Geo, const FPointerEvent& E) const override { return FCursorReply::Cursor(EMouseCursor::Hand); }

	private:
		void SetFrom(const FGeometry& Geo, const FPointerEvent& E, bool bSave)
		{
			const float Knob = 18.f * U;
			const float X = Geo.AbsoluteToLocal(E.GetScreenSpacePosition()).X - Knob * 0.5f;
			const float F = FMath::Clamp(X / FMath::Max(1.f, static_cast<float>(Geo.GetLocalSize().X) - Knob), 0.f, 1.f);
			WRSound::SetVolume(Category, FMath::RoundToFloat(F * 100.f) / 100.f, bSave);
		}
		FName Category;
	};

	class SWRAudioPage : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SWRAudioPage) {}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			struct FRow { const TCHAR* Category; FText Name; FText Sub; };
			const FRow Rows[] = {
				{ TEXT("master"), LOCTEXT("MasterVolume", "Master volume"), LOCTEXT("MasterVolumeSub", "Everything at once") },
				{ TEXT("music"), LOCTEXT("MusicVolume", "Music"), LOCTEXT("MusicVolumeSub", "The menu theme, a tune now and then while you explore, fights and bosses") },
				{ TEXT("effects"), LOCTEXT("EffectsVolume", "Effects"), LOCTEXT("EffectsVolumeSub", "Footsteps, combat, creatures, spells, gathering and building") },
				{ TEXT("ambient"), LOCTEXT("AmbientVolume", "Ambience"), LOCTEXT("AmbientVolumeSub", "Wind, surf, birds and insects, caves and lava") },
				{ TEXT("ui"), LOCTEXT("UIVolume", "Interface"), LOCTEXT("UIVolumeSub", "Menus, buttons and the bag") },
			};
			TSharedRef<SVerticalBox> List = SNew(SVerticalBox) + SVerticalBox::Slot().AutoHeight()[Heading(LOCTEXT("Volume", "Volume"))];
			for (const FRow& Row : Rows)
			{
				const FName Category(Row.Category);
				List->AddSlot().AutoHeight().Padding(0.f, Category == TEXT("master") ? 10.f * U : 6.f * U, 0.f, 0.f)
				[
					SettingRow(Row.Name, Row.Sub,
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SWRVolumeBar, Category)]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(14.f * U, 0.f, 6.f * U, 0.f)
						[
							SNew(SBox).WidthOverride(46.f * U).HAlign(HAlign_Right)
							[
								WRUI::Text(TAttribute<FText>::CreateLambda([Category]() { return FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(WRSound::GetVolume(Category) * 100.f))); }),
									WRUI::Body(13, 700), WRUI::Ink)
							]
						], Category == TEXT("master") ? 56.f : 52.f)
				];
			}
			List->AddSlot().AutoHeight().Padding(0.f, 12.f * U, 0.f, 0.f)
			[
				Muted(LOCTEXT("AudioHint", "Changes apply straight away and are kept for next time. Music stays quiet most of the time while you explore."), 12, true)
			];
			ChildSlot[List];
		}
	};

	// ------------------------------------------------------------ video page

	class SWRVideoPage : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SWRVideoPage) {}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			const TArray<FText> Levels = { LOCTEXT("Low", "Low"), LOCTEXT("Medium", "Medium"), LOCTEXT("High", "High"), LOCTEXT("Epic", "Epic") };
			struct FGroup { FText Name; int32 (UGameUserSettings::*Get)() const; void (UGameUserSettings::*Set)(int32); };
			const FGroup Groups[] = {
				{ LOCTEXT("ViewDistance", "View distance"), &UGameUserSettings::GetViewDistanceQuality, &UGameUserSettings::SetViewDistanceQuality },
				{ LOCTEXT("Shadows", "Shadows"), &UGameUserSettings::GetShadowQuality, &UGameUserSettings::SetShadowQuality },
				{ LOCTEXT("GI", "Global illumination"), &UGameUserSettings::GetGlobalIlluminationQuality, &UGameUserSettings::SetGlobalIlluminationQuality },
				{ LOCTEXT("Reflections", "Reflections"), &UGameUserSettings::GetReflectionQuality, &UGameUserSettings::SetReflectionQuality },
				{ LOCTEXT("Textures", "Textures"), &UGameUserSettings::GetTextureQuality, &UGameUserSettings::SetTextureQuality },
				{ LOCTEXT("Effects", "Effects"), &UGameUserSettings::GetVisualEffectQuality, &UGameUserSettings::SetVisualEffectQuality },
				{ LOCTEXT("Foliage", "Foliage"), &UGameUserSettings::GetFoliageQuality, &UGameUserSettings::SetFoliageQuality },
				{ LOCTEXT("PostProcess", "Post processing"), &UGameUserSettings::GetPostProcessingQuality, &UGameUserSettings::SetPostProcessingQuality },
				{ LOCTEXT("AA", "Anti-aliasing"), &UGameUserSettings::GetAntiAliasingQuality, &UGameUserSettings::SetAntiAliasingQuality },
			};
			// Overall: the engine's level, or the one every group below shares (its own check counts groups not shown here).
			TArray<int32 (UGameUserSettings::*)() const> Getters;
			for (const FGroup& G : Groups) Getters.Add(G.Get);
			const auto Overall = [Getters]()
			{
				const UGameUserSettings* S = VideoSettings();
				if (!S) return -1;
				if (S->GetOverallScalabilityLevel() >= 0) return S->GetOverallScalabilityLevel();
				const int32 First = (S->*Getters[0])();
				for (const auto Get : Getters) if ((S->*Get)() != First) return -1;
				return First;
			};
			TSharedRef<SVerticalBox> Quality = SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[Heading(LOCTEXT("Quality", "Quality"))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f * U, 0.f, 0.f)
				[
					SettingRow(LOCTEXT("Overall", "Overall"), LOCTEXT("OverallSub", "Sets everything below at once"),
						Segmented(Levels, Overall,
							[](int32 V) { if (UGameUserSettings* S = VideoSettings()) { S->SetOverallScalabilityLevel(V); S->ApplySettings(false); } }), 52.f)
				];
			for (const FGroup& G : Groups)
			{
				const auto Get = G.Get;
				const auto Set = G.Set;
				Quality->AddSlot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 0.f)
				[
					SettingRow(G.Name, FText::GetEmpty(),
						Segmented(Levels, [Get]() { const UGameUserSettings* S = VideoSettings(); return S ? FMath::Min((S->*Get)(), 3) : -1; },
							[Set](int32 V) { if (UGameUserSettings* S = VideoSettings()) { (S->*Set)(V); S->ApplySettings(false); } }), 42.f)
				];
			}

			const TArray<FText> Modes = { LOCTEXT("Fullscreen", "Fullscreen"), LOCTEXT("Borderless", "Borderless"), LOCTEXT("Windowed", "Windowed") };
			TSharedRef<SVerticalBox> Display = SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[Heading(LOCTEXT("Display", "Display"))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f * U, 0.f, 0.f)
				[
					SettingRow(LOCTEXT("WindowMode", "Window mode"), FText::GetEmpty(),
						Segmented(Modes, []() { const EWindowMode::Type M = CurrentWindowMode(); return M == EWindowMode::Fullscreen ? 0 : M == EWindowMode::WindowedFullscreen ? 1 : 2; },
							[](int32 V) { if (UGameUserSettings* S = VideoSettings()) { S->SetFullscreenMode(V == 0 ? EWindowMode::Fullscreen : V == 1 ? EWindowMode::WindowedFullscreen : EWindowMode::Windowed); S->ApplySettings(false); } }), 52.f)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 0.f)
				[
					SettingRow(LOCTEXT("Resolution", "Resolution"), LOCTEXT("ResolutionSub", "Borderless always uses the desktop's"),
						Stepper([]() { const UGameUserSettings* S = VideoSettings(); const FIntPoint R = S ? S->GetScreenResolution() : FIntPoint::ZeroValue; return FText::FromString(FString::Printf(TEXT("%d × %d"), R.X, R.Y)); },
							[](int32 Dir) { StepResolution(Dir); }), 52.f)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 0.f)
				[
					SettingRow(LOCTEXT("Scale3D", "3D resolution"), LOCTEXT("Scale3DSub", "Lower is faster; the interface stays sharp"),
						Stepper([]()
							{
								float N = 0.f, V = 0.f, Lo = 0.f, Hi = 100.f;
								if (UGameUserSettings* S = VideoSettings()) S->GetResolutionScaleInformationEx(N, V, Lo, Hi);
								return V <= 0.f ? LOCTEXT("ScaleAuto", "Auto") : FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(V)));
							},
							[](int32 Dir)
							{
								// 50% to 100% in fives, then Auto (0: the engine's own choice for this machine).
								UGameUserSettings* S = VideoSettings();
								if (!S) return;
								float N = 0.f, V = 0.f, Lo = 0.f, Hi = 100.f;
								S->GetResolutionScaleInformationEx(N, V, Lo, Hi);
								const int32 Step = V <= 0.f ? 11 : FMath::Clamp(FMath::RoundToInt((V - 50.f) / 5.f), 0, 10);
								const int32 Next = FMath::Clamp(Step + Dir, 0, 11);
								S->SetResolutionScaleValueEx(Next == 11 ? 0.f : 50.f + Next * 5.f);
								S->ApplySettings(false);
							}, 110.f), 52.f)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 0.f)
				[
					SettingRow(LOCTEXT("FrameCap", "Frame rate cap"), FText::GetEmpty(),
						Stepper([]() { const UGameUserSettings* S = VideoSettings(); const float L = S ? S->GetFrameRateLimit() : 0.f; return L <= 0.f ? LOCTEXT("Unlimited", "Unlimited") : FText::AsNumber(FMath::RoundToInt(L)); },
							[](int32 Dir) { StepFrameCap(Dir); }, 110.f), 48.f)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 0.f)
				[
					SettingRow(LOCTEXT("VSync", "Vertical sync"), LOCTEXT("VSyncSub", "No tearing, a little more input delay"),
						SNew(SWRSwitch, []() { const UGameUserSettings* S = VideoSettings(); return S && S->IsVSyncEnabled(); },
							[]() { if (UGameUserSettings* S = VideoSettings()) { S->SetVSyncEnabled(!S->IsVSyncEnabled()); S->ApplySettings(false); } }), 52.f)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f * U, 0.f, 0.f)
				[
					Muted(LOCTEXT("VideoHint", "Changes apply straight away and are kept for next time. Auto-detect picks quality levels for this computer."), 12, true)
				]
				// ---- Game: first-session hints (WRUIFlow keeps which ones each player has seen). Added by the polish pass.
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 26.f * U, 0.f, 0.f)[Heading(LOCTEXT("GameHeading", "Game"))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f * U, 0.f, 0.f)
				[
					SettingRow(LOCTEXT("ShowHints", "Show hints"), LOCTEXT("ShowHintsSub", "One-time tips with your keys, the first time you meet something"),
						SNew(SWRSwitch, []() { return FWRFlow::HintsEnabled(); }, []() { FWRFlow::SetHintsEnabled(!FWRFlow::HintsEnabled()); }), 52.f)
				];

			ChildSlot
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f)[Display]
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(28.f * U, 0.f, 0.f, 0.f)[Quality]
			];
		}

		static void StepResolution(int32 Dir)
		{
			UGameUserSettings* S = VideoSettings();
			if (!S) return;
			TArray<FIntPoint> List;
			if (CurrentWindowMode() == EWindowMode::Windowed) UKismetSystemLibrary::GetConvenientWindowedResolutions(List);
			else UKismetSystemLibrary::GetSupportedFullscreenResolutions(List);
			const FIntPoint Now = S->GetScreenResolution();
			List.AddUnique(Now);
			List.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X * A.Y < B.X * B.Y || (A.X * A.Y == B.X * B.Y && A.X < B.X); });
			const int32 i = FMath::Clamp(List.IndexOfByKey(Now) + Dir, 0, List.Num() - 1);
			S->SetScreenResolution(List[i]);
			S->ApplySettings(false);
		}

		static void StepFrameCap(int32 Dir)
		{
			UGameUserSettings* S = VideoSettings();
			if (!S) return;
			static const float Caps[] = { 30.f, 60.f, 90.f, 120.f, 144.f, 165.f, 240.f, 0.f };
			const float Now = S->GetFrameRateLimit();
			int32 Best = 7;
			for (int32 i = 0; i < 7; ++i) if (Now > 0.f && FMath::Abs(Caps[i] - Now) < FMath::Abs(Caps[Best == 7 ? 0 : Best] - Now)) Best = i;
			if (Now > 0.f && Best == 7) Best = 0;
			S->SetFrameRateLimit(Caps[FMath::Clamp(Best + Dir, 0, 7)]);
			S->ApplySettings(false);
		}
	};

	// ------------------------------------------------------------ the window

	/** A row of the left-hand list: a page, or an action (Resume, Quit...). */
	class SWRNavItem : public SWRClickBase
	{
	public:
		SLATE_BEGIN_ARGS(SWRNavItem) : _Icon(EWRIcon::Gear), _Accent(WRUI::Brass) {}
			SLATE_ARGUMENT(EWRIcon, Icon)
			SLATE_ARGUMENT(FText, Text)
			SLATE_ARGUMENT(FLinearColor, Accent)
		SLATE_END_ARGS()
		void Construct(const FArguments& InArgs, TFunction<bool()> InSelected, TFunction<void()> InClick)
		{
			Selected = MoveTemp(InSelected);
			OnLeft = MoveTemp(InClick);
			const FLinearColor Accent = InArgs._Accent;
			ChildSlot
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[SNew(SWRGradientFill).Colors({ WRUI::Alpha(WRUI::Brass, 0.22f), WRUI::Alpha(WRUI::Brass, 0.04f) }).Radius(3.f).Visibility_Lambda([this]() { return IsOn() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })]
				+ SOverlay::Slot()[SNew(SImage).Image(WRUI::Box(FLinearColor(1.f, 1.f, 1.f, 0.04f), 3.f)).Visibility_Lambda([this]() { return IsHovered() && !IsOn() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })]
				+ SOverlay::Slot().HAlign(HAlign_Left)[SNew(SBox).WidthOverride(3.f * U).Visibility_Lambda([this]() { return IsOn() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })[SNew(SImage).Image(WRUI::White()).ColorAndOpacity(WRUI::Brass)]]
				+ SOverlay::Slot().Padding(FMargin(17.f * U, 12.f * U, 14.f * U, 12.f * U))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f * U, 0.f)
					[
						SNew(SWRLineIcon).Icon(InArgs._Icon).Size(18.f * U)
						.Color_Lambda([this, Accent]() { return IsOn() ? Accent : IsHovered() ? WRUI::Ink2 : WRUI::Ink3; })
						.Accent(Accent)
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[
						WRUI::Text(InArgs._Text, Spaced(WRUI::Title(14), 60), TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(IsOn() ? WRUI::Ink : IsHovered() ? WRUI::Ink2 : WRUI::Ink3); }), IsOn())
					]
				]
			];
		}
	private:
		TFunction<bool()> Selected;
		bool IsOn() const { return Selected && Selected(); }
	};

	class SWRSettings;
	TWeakPtr<SWRSettings> GLiveSettings;

	class SWRSettings : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SWRSettings) : _InGame(true) {}
			SLATE_ARGUMENT(TWeakObjectPtr<APlayerController>, PC)
			SLATE_ARGUMENT(bool, InGame)
			SLATE_EVENT(FSimpleDelegate, OnClose)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			PC = InArgs._PC;
			bInGame = InArgs._InGame;
			OnClose = InArgs._OnClose;

			TSharedRef<SVerticalBox> Nav = SNew(SVerticalBox);
			const auto Add = [&Nav](TSharedRef<SWidget> W, float Top = 4.f) { Nav->AddSlot().AutoHeight().Padding(0.f, Top * U, 0.f, 0.f)[W]; };
			if (bInGame)
			{
				Add(SNew(SWRNavItem, nullptr, [this]() { Close(); }).Icon(EWRIcon::Play).Text(LOCTEXT("Resume", "Resume")), 0.f);
				Nav->AddSlot().AutoHeight().Padding(10.f * U, 8.f * U, 10.f * U, 4.f * U)[HLine(WRUI::Alpha(WRUI::Brass2, 0.5f))];
			}
			Add(SNew(SWRNavItem, [this]() { return Page == TEXT("controls"); }, [this]() { ShowPage(TEXT("controls")); }).Icon(EWRIcon::Keyboard).Text(LOCTEXT("Controls", "Controls")), bInGame ? 4.f : 0.f);
			Add(SNew(SWRNavItem, [this]() { return Page == TEXT("audio"); }, [this]() { ShowPage(TEXT("audio")); }).Icon(EWRIcon::Speaker).Text(LOCTEXT("Audio", "Audio")));
			Add(SNew(SWRNavItem, [this]() { return Page == TEXT("video"); }, [this]() { ShowPage(TEXT("video")); }).Icon(EWRIcon::Monitor).Text(LOCTEXT("Video", "Video")));
			if (bInGame)
			{
				Nav->AddSlot().AutoHeight().Padding(10.f * U, 8.f * U, 10.f * U, 4.f * U)[HLine(WRUI::Alpha(WRUI::Brass2, 0.5f))];
				Add(SNew(SWRNavItem, nullptr, [this]() { BackToMenu(); }).Icon(EWRIcon::Exit).Text(LOCTEXT("BackToMenu", "Main menu")));
				Add(SNew(SWRNavItem, nullptr, [this]() { Quit(); }).Icon(EWRIcon::Power).Accent(WRUI::Bad).Text(LOCTEXT("Quit", "Quit")));
			}
			Nav->AddSlot().FillHeight(1.f)[SNullWidget::NullWidget];
			Nav->AddSlot().AutoHeight().Padding(14.f * U, 8.f * U)[Muted(FText::FromString(TEXT("Wildreach ") + SWRMainMenu::GameVersion()), 11)];

			ChildSlot
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[SNew(SBackgroundBlur).BlurStrength(8.f)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.5f))]]
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(1300.f * U).HeightOverride(780.f * U)
					[
						SNew(SWRPanel).Kind(EWRPanel::Stone).Rivets(true).BrassTrim(true).Padding(FMargin(0.f))
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()[TitleRow()]
							+ SVerticalBox::Slot().AutoHeight().Padding(24.f * U, 0.f)[FadeLine()]
							+ SVerticalBox::Slot().FillHeight(1.f).Padding(FMargin(24.f * U, 20.f * U, 24.f * U, 14.f * U))
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth()
								[
									SNew(SBox).WidthOverride(200.f * U)
									[
										SNew(SBorder).BorderImage(InsetBrush()).Padding(FMargin(8.f * U, 10.f * U))[Nav]
									]
								]
								+ SHorizontalBox::Slot().FillWidth(1.f).Padding(24.f * U, 0.f, 0.f, 0.f)[SAssignNew(Content, SBox)]
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(24.f * U, 0.f)[Footer()]
						]
					]
				]
			];
			ShowPage(TEXT("controls"));
		}

		virtual bool SupportsKeyboardFocus() const override { return true; }

		virtual FReply OnKeyDown(const FGeometry& Geo, const FKeyEvent& E) override
		{
			// In game Esc reaches the controller (it closes the menu); the main menu ignores game input, so close here.
			if (E.GetKey() == EKeys::Escape && !bInGame)
			{
				Close();
				return FReply::Handled();
			}
			return FReply::Unhandled();
		}

		virtual void Tick(const FGeometry& Geo, const double Time, const float Delta) override
		{
			SCompoundWidget::Tick(Geo, Time, Delta);
			// Keyboard focus once it's on screen, so Esc and key listening reach this window.
			if (!bFocused)
			{
				bFocused = true;
				FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EFocusCause::SetDirectly);
			}
		}

		void ShowPage(FName NewPage)
		{
			Page = NewPage;
			if (Page == TEXT("video"))
			{
				Controls.Reset();
				Content->SetContent(SNew(SWRVideoPage));
			}
			else if (Page == TEXT("audio"))
			{
				Controls.Reset();
				Content->SetContent(SNew(SWRAudioPage));
			}
			else
			{
				Page = TEXT("controls");
				Content->SetContent(SAssignNew(Controls, SWRControlsPage).PC(PC).InGame(bInGame));
			}
		}

		TSharedPtr<SWRControlsPage> GetControls() const { return Controls; }

	private:
		TWeakObjectPtr<APlayerController> PC;
		bool bInGame = true;
		bool bFocused = false;
		FSimpleDelegate OnClose;
		FName Page;
		TSharedPtr<SBox> Content;
		TSharedPtr<SWRControlsPage> Controls;

		void Close()
		{
			if (Controls.IsValid()) Controls->CancelAll();
			OnClose.ExecuteIfBound();
		}

		void BackToMenu()
		{
			AWRPlayerController* P = Cast<AWRPlayerController>(PC.Get());
			if (!P) return;
			// Close the menu first and leave a moment later: the UI hands input back to the game when its screen
			// closes, which would otherwise land after the main menu took it.
			P->SetMenu(EWRMenu::None);
			FTimerHandle Handle;
			P->GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(P, [P]() { P->ReturnToMenu(); }), 0.15f, false);
		}

		void Quit()
		{
			// As the main menu quits.
			if (APlayerController* P = PC.Get())
			{
				UKismetSystemLibrary::QuitGame(P, P, EQuitPreference::Quit, false);
			}
		}

		TSharedRef<SWidget> TitleRow()
		{
			return SNew(SBox).HeightOverride(72.f * U).Padding(FMargin(30.f * U, 0.f, 26.f * U, 0.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 14.f * U, 0.f)[SNew(SWRLineIcon).Icon(EWRIcon::Gear).Size(28.f * U).Color(WRUI::Brass)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(LOCTEXT("Settings", "Settings"), Spaced(WRUI::Title(23), 60), WRUI::Ink, true)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(14.f * U, 0.f)[WRUI::Text(FText::FromString(TEXT("›")), WRUI::Body(20, 500), WRUI::Ink3)]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return Page == TEXT("video") ? LOCTEXT("Video", "Video") : Page == TEXT("audio") ? LOCTEXT("Audio", "Audio") : LOCTEXT("Controls", "Controls"); }), Spaced(WRUI::Title(16), 60), WRUI::Ink2, true)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f * U, 0.f)[Muted(LOCTEXT("Close", "Close"), 12)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 14.f * U, 0.f)[SNew(SWRKeyCap).FontSize(11).Text(LOCTEXT("EscKey", "Esc"))]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SWRCloseButton, [this]() { Close(); })]
			];
		}

		class SWRCloseButton : public SWRClickBase
		{
		public:
			SLATE_BEGIN_ARGS(SWRCloseButton) {}
			SLATE_END_ARGS()
			void Construct(const FArguments& InArgs, TFunction<void()> InClick)
			{
				OnLeft = MoveTemp(InClick);
				ChildSlot
				[
					SNew(SBox).WidthOverride(36.f * U).HeightOverride(36.f * U)
					[
						SNew(SBorder).BorderImage_Lambda([this]() { return WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.35f), 4.f, IsHovered() ? WRUI::Brass : WRUI::Brass2, 1.f); })
						.HAlign(HAlign_Center).VAlign(VAlign_Center)
						[
							SNew(SWRLineIcon).Icon(EWRIcon::Close).Size(16.f * U).Color(WRUI::Ink)
						]
					]
				];
			}
		};

		TSharedRef<SWidget> Footer()
		{
			return SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[HLine(WRUI::Alpha(WRUI::Brass2, 0.45f))]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SBox).HeightOverride(76.f * U)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(SWRButton).Ghost(true).FontSize(13).MinHeight(44.f * U)
							.Text_Lambda([this]() { return Page == TEXT("video") ? LOCTEXT("AutoDetect", "Auto-detect quality") : LOCTEXT("ResetDefaults", "Reset to defaults"); })
							.OnClicked_Lambda([this]()
							{
								if (Page == TEXT("video"))
								{
									if (UGameUserSettings* S = VideoSettings())
									{
										S->RunHardwareBenchmark();
										S->ApplyHardwareBenchmarkResults();
									}
								}
								else if (Page == TEXT("audio"))
								{
									WRSound::ResetVolumes();
								}
								else if (Controls.IsValid())
								{
									Controls->ResetAll();
								}
								return FReply::Handled();
							})
						]
						+ SHorizontalBox::Slot().FillWidth(1.f)[SNullWidget::NullWidget]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 16.f * U, 0.f)[Muted(LOCTEXT("Saved", "Changes are saved as you make them"), 12)]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(SBox).MinDesiredWidth(150.f * U)
							[
								SNew(SWRButton).Primary(true).FontSize(13).MinHeight(44.f * U).Text(bInGame ? LOCTEXT("Resume", "Resume") : LOCTEXT("Done", "Done"))
								.OnClicked_Lambda([this]() { Close(); return FReply::Handled(); })
							]
						]
					]
				];
		}
	};

#if !UE_BUILD_SHIPPING
	// Testing and screenshots (the bot runs console commands): WRSettings video | controls | listen [action] | key <key> | cancel | scroll <offset>.
	static FAutoConsoleCommand GSettingsCommand(TEXT("WRSettings"),
		TEXT("Testing: drive the open settings window. WRSettings video|audio|controls, WRSettings listen [action] (default: the last ability), WRSettings key <key name>, WRSettings cancel, WRSettings scroll <offset>"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			const TSharedPtr<SWRSettings> W = GLiveSettings.Pin();
			if (!W.IsValid() || Args.Num() == 0) return;
			if (Args[0] == TEXT("video") || Args[0] == TEXT("controls") || Args[0] == TEXT("audio"))
			{
				W->ShowPage(FName(*Args[0]));
				return;
			}
			const TSharedPtr<SWRControlsPage> C = W->GetControls();
			if (!C.IsValid()) return;
			if (Args[0] == TEXT("listen"))
			{
				FName Id = Args.Num() > 1 ? FName(*Args[1]) : NAME_None;
				if (Id.IsNone())
				{
					for (const FWRActionKey& A : FWRKeyBindings::Get().Actions()) if (A.Group == GAbilities) Id = A.Id;
				}
				C->Listen(Id.IsNone() ? FName(TEXT("Jump")) : Id);
			}
			else if (Args[0] == TEXT("cancel"))
			{
				C->CancelAll();
			}
			else if (Args[0] == TEXT("scroll") && Args.Num() > 1)
			{
				C->ScrollKeys(FCString::Atof(*Args[1]));
			}
			else if (Args[0] == TEXT("key") && Args.Num() > 1)
			{
				C->Capture(FKey(FName(*Args[1])));
			}
		}));
#endif

	TSharedRef<SWidget> Make(APlayerController* PC, bool bInGame, FSimpleDelegate OnClose)
	{
		TSharedRef<SWRSettings> W = SNew(SWRSettings).PC(PC).InGame(bInGame).OnClose(OnClose);
		GLiveSettings = W;
		return W;
	}
}

TSharedRef<SWidget> WRUI::MakeSettings(APlayerController* PC, bool bInGame, FSimpleDelegate OnClose)
{
	return WRSettingsUI::Make(PC, bInGame, OnClose);
}

namespace WRSettingsUI
{
	WR_REGISTER_SCREEN("settings", [](AWRPlayerController* PC) -> TSharedRef<SWidget>
	{
		const TWeakObjectPtr<AWRPlayerController> Weak(PC);
		return WRUI::MakeSettings(PC, true, FSimpleDelegate::CreateLambda([Weak]() { if (Weak.IsValid()) Weak->SetMenu(EWRMenu::None); }));
	});
}

#undef LOCTEXT_NAMESPACE
