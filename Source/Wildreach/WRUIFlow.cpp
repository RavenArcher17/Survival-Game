// The moments between play (WRUIFlow.h): the death screen and the fades around death and respawn, the "Loading the
// island" cover, the loading screen during map loads, and the moments layer (banners, toasts, one-time hints).
// Sizes are 1080p units (the mockups' px x 1.2), as in the rest of the stone-and-leather UI.

#include "WRUIFlow.h"
#include "Wildreach.h"
#include "WRAttributeSet.h"
#include "WRBoss.h"
#include "WRCharacter.h"
#include "WRCrafting.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRGameState.h"
#include "WRInventory.h"
#include "WRKeyBindings.h"
#include "WRMainMenu.h"
#include "WRPathComponent.h"
#include "WRPathTypes.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRPlayerUI.h"
#include "WRResourceNode.h"
#include "WRSocial.h"
#include "WRSound.h"
#include "WRTuning.h"
#include "WRUIMotion.h"
#include "WRUISettings.h"
#include "WRUIStyle.h"
#include "WRUIWidgets.h"
#include "WRUIWindowKit.h"
#include "WRWorldDressing.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/DelayedAutoRegister.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "MoviePlayer.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/StyleDefaults.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionRuntimeCell.h"
#include "WorldPartition/WorldPartitionSubsystem.h"

#define LOCTEXT_NAMESPACE "WRUI"

// ---------------------------------------------------------------- look helpers

namespace WRFlowUI
{
	using WRUI::U;

	const TCHAR* MemorySection = TEXT("Wildreach.Flow");
	/** Tests: hints on or off for this run without touching the saved setting. */
	TOptional<bool> HintsOverride;

	FSlateFontInfo Spaced(FSlateFontInfo Font, int32 Spacing)
	{
		Font.LetterSpacing = Spacing;
		return Font;
	}

	const FSlateBrush* Glyph(FName Name) { return WRUI::Image(FName(*(TEXT("Glyphs/") + Name.ToString()))); }

	FVector2D Measure(const FText& Text, const FSlateFontInfo& Font)
	{
		return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text, Font);
	}

	FText KeyText(FName Action) { return WRUI::KeyName(FWRKeyBindings::Get().Key(Action)); }

	/** Content that fades (and drifts in from Drift) with an alpha; collapsed at zero, never takes clicks. */
	TSharedRef<SWidget> Fade(TFunction<float()> Alpha, TSharedRef<SWidget> Content, FVector2D Drift = FVector2D::ZeroVector)
	{
		return SNew(SBorder).BorderImage(FStyleDefaults::GetNoBrush()).Padding(0.f)
			.Visibility_Lambda([Alpha]() { return Alpha() > 0.002f ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			.ColorAndOpacity_Lambda([Alpha]() { return FLinearColor(1.f, 1.f, 1.f, FMath::Clamp(Alpha(), 0.f, 1.f)); })
			.RenderTransform_Lambda([Alpha, Drift]() { return TOptional<FSlateRenderTransform>(FSlateRenderTransform(Drift * (1.f - FMath::Clamp(Alpha(), 0.f, 1.f)))); })
			[
				Content
			];
	}

	/** Carved lettering as on the main menu's title: a dark copy below, then lighter bands over the top of the letters. */
	TSharedRef<SWidget> Carved(const TAttribute<FText>& Text, const FSlateFontInfo& Font, const FLinearColor& Base, const FLinearColor& Mid, const FLinearColor& Top)
	{
		const float H = Measure(FText::FromString(TEXT("WILDREACH")), Font).Y;
		const auto Layer = [Text, Font](const FLinearColor& Color) { return SNew(STextBlock).Text(Text).Font(Font).ColorAndOpacity(Color); };
		return SNew(SOverlay)
			+ SOverlay::Slot().Padding(0.f, 4.f * U, 0.f, 0.f)
			[
				SNew(STextBlock).Text(Text).Font(Font).ColorAndOpacity(FLinearColor(FColor(26, 16, 6))).ShadowOffset(FVector2D(0.f, 5.f * U)).ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.85f))
			]
			+ SOverlay::Slot()[Layer(Base)]
			+ SOverlay::Slot().VAlign(VAlign_Top)[SNew(SBox).HeightOverride(H * 0.5f).VAlign(VAlign_Top).Clipping(EWidgetClipping::ClipToBounds)[Layer(Mid)]]
			+ SOverlay::Slot().VAlign(VAlign_Top)[SNew(SBox).HeightOverride(H * 0.37f).VAlign(VAlign_Top).Clipping(EWidgetClipping::ClipToBounds)[Layer(Top)]];
	}

	TSharedRef<SWidget> Gold(const TAttribute<FText>& Text, int32 Size, int32 Spacing)
	{
		return Carved(Text, Spaced(WRUI::Title(Size), Spacing), FLinearColor(FColor::FromHex(TEXT("B8894A"))), FLinearColor(FColor::FromHex(TEXT("E9C27A"))), FLinearColor(FColor::FromHex(TEXT("FBE7B4"))));
	}

	/** A brass rule fading out to each side with a small diamond in the middle. */
	TSharedRef<SWidget> Rule(float HalfWidth, const TAttribute<FLinearColor>& Color)
	{
		const FLinearColor B2 = WRUI::Brass2;
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(HalfWidth).HeightOverride(1.f)[SNew(SWRGradientFill).Colors({ WRUI::Alpha(B2, 0.f), B2 })]]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f * U, 0.f)
			[
				SNew(SBox).WidthOverride(8.f * U).HeightOverride(8.f * U)
				[
					SNew(SImage).Image(WRUI::White()).ColorAndOpacity_Lambda([Color]() { return FSlateColor(Color.Get()); })
					.RenderTransform(FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(45.f)))).RenderTransformPivot(FVector2D(0.5f, 0.5f))
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(HalfWidth).HeightOverride(1.f)[SNew(SWRGradientFill).Colors({ B2, WRUI::Alpha(B2, 0.f) })]];
	}

	/** A turning brass arc (loading). Reads only the clock, so the movie player's thread can draw it. */
	class SWRSpinner : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SWRSpinner) : _Size(20.f) {}
			SLATE_ARGUMENT(float, Size)
		SLATE_END_ARGS()
		void Construct(const FArguments& InArgs) { Size = InArgs._Size; }
		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(Size, Size); }
		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override
		{
			const FVector2D C = Geo.GetLocalSize() * 0.5f;
			const float R = FMath::Min(C.X, C.Y) - 2.f;
			const float Spin = float(FMath::Fmod(FPlatformTime::Seconds() * 320.0, 360.0));
			const float Fade = Style.GetColorAndOpacityTint().A;
			TArray<FVector2D> Ring, Arc;
			for (int32 i = 0; i <= 24; ++i)
			{
				const float A = FMath::DegreesToRadians(i * 15.f);
				Ring.Add(C + FVector2D(FMath::Sin(A), -FMath::Cos(A)) * R);
			}
			for (int32 i = 0; i <= 16; ++i)
			{
				const float A = FMath::DegreesToRadians(Spin + i * 6.f);
				Arc.Add(C + FVector2D(FMath::Sin(A), -FMath::Cos(A)) * R);
			}
			FSlateDrawElement::MakeLines(Out, Layer, Geo.ToPaintGeometry(), Ring, ESlateDrawEffect::None, WRUI::Alpha(WRUI::Brass2, 0.45f * Fade), true, 2.f);
			FSlateDrawElement::MakeLines(Out, Layer + 1, Geo.ToPaintGeometry(), Arc, ESlateDrawEffect::None, WRUI::Alpha(WRUI::Brass, Fade), true, 2.6f);
			return Layer + 1;
		}
	private:
		float Size = 20.f;
	};

	/** Data/UI/loading.jpg (a view of the island) for the loading screens, decoded once on the game thread. */
	const FSlateBrush* LoadingBackdrop()
	{
		static TSharedPtr<FSlateDynamicImageBrush> Brush;
		static bool bTried = false;
		if (!bTried)
		{
			bTried = true;
			const FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(WRUI::Dir(), TEXT("loading.jpg")));
			TArray<uint8> File;
			if (FFileHelper::LoadFileToArray(File, *Path, FILEREAD_Silent))
			{
				IImageWrapperModule& Wrappers = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
				TSharedPtr<IImageWrapper> Jpg = Wrappers.CreateImageWrapper(EImageFormat::JPEG);
				TArray<uint8> Raw;
				if (Jpg.IsValid() && Jpg->SetCompressed(File.GetData(), File.Num()) && Jpg->GetRaw(ERGBFormat::BGRA, 8, Raw))
				{
					Brush = FSlateDynamicImageBrush::CreateWithImageData(TEXT("WRLoadingBackdrop"), FVector2D(Jpg->GetWidth(), Jpg->GetHeight()), Raw);
				}
			}
			if (!Brush.IsValid())
			{
				UE_LOG(LogWildreach, Warning, TEXT("FLOW loading backdrop missing or unreadable: %s"), *Path);
			}
		}
		return Brush.Get();
	}

	/** A spinner and a line ("Loading the island..."). */
	TSharedRef<SWidget> StatusLine(const TAttribute<FText>& Status, int32 FontSize)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f * U, 0.f)[SNew(SWRSpinner).Size(FontSize * 1.4f * U)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(Status, Spaced(WRUI::Title(FontSize, 600), 60), WRUI::Ink, true)];
	}
}

TSharedRef<SWidget> WRUIFlow::MakeLoadingScreen(const TAttribute<FText>& Status, bool bBackdrop)
{
	using namespace WRFlowUI;
	// The tip is picked now and its key read now: the movie player draws this on its own thread.
	const TArray<WRMenuUI::FTip>& Tips = WRMenuUI::Tips();
	const WRMenuUI::FTip& Tip = Tips[FMath::RandRange(0, Tips.Num() - 1)];
	const FText Key = KeyText(Tip.Action);
	const FSlateBrush* Back = bBackdrop ? LoadingBackdrop() : nullptr;
	const FLinearColor Night(FColor(6, 7, 9));
	TSharedRef<SOverlay> Screen = SNew(SOverlay)
		+ SOverlay::Slot()[SNew(SImage).Image(WRUI::White()).ColorAndOpacity(Night)];
	if (Back)
	{
		Screen->AddSlot()[SNew(SScaleBox).Stretch(EStretch::ScaleToFill)[SNew(SImage).Image(Back)]];
	}
	Screen->AddSlot()[SNew(SWRGradientFill).Horizontal(false).Colors({ WRUI::Alpha(Night, 0.72f), WRUI::Alpha(Night, 0.12f), WRUI::Alpha(Night, 0.25f), WRUI::Alpha(Night, 0.78f), WRUI::Alpha(Night, 0.94f) }).Stops({ 0.f, 0.35f, 0.6f, 0.85f, 1.f })];
	Screen->AddSlot()[SNew(SWRGradientFill).Colors({ WRUI::Alpha(Night, 0.5f), WRUI::Alpha(Night, 0.f), WRUI::Alpha(Night, 0.f), WRUI::Alpha(Night, 0.5f) }).Stops({ 0.f, 0.2f, 0.8f, 1.f })];
	Screen->AddSlot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0.f, 110.f * U, 0.f, 0.f)[SWRMainMenu::MakeTitle()];
	Screen->AddSlot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 0.f, 56.f * U)
	[
		SNew(SWRPanel).Kind(EWRPanel::Glass).Rivets(true).Padding(FMargin(18.f * U, 12.f * U, 22.f * U, 12.f * U))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f * U, 0.f)[SNew(SWRLineIcon).Icon(EWRIcon::Bulb).Size(22.f * U).Color(WRUI::Warn)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f * U, 0.f)[WRUI::Text(LOCTEXT("FlowTip", "Tip:"), WRUI::Body(15, 700), WRUI::Brass)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f * U, 0.f)[WRUI::Text(Tip.Before, WRUI::Body(15, 500), WRUI::Ink2)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f * U, 0.f)[SNew(SWRKeyCap).FontSize(13).Text(Key)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(Tip.After, WRUI::Body(15, 500), WRUI::Ink2)]
		]
	];
	Screen->AddSlot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 56.f * U, 66.f * U)[StatusLine(Status, 17)];
	return Screen;
}

// ---------------------------------------------------------------- the cover: black, loading, death screen

class SWRFlowCover : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWRFlowCover) {}
		SLATE_ARGUMENT(TWeakPtr<FWRFlow>, Flow)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		using namespace WRFlowUI;
		Flow = InArgs._Flow;
		const TWeakPtr<FWRFlow> W = Flow;
		const auto Black = [W]() { const TSharedPtr<FWRFlow> F = W.Pin(); return F ? F->GetBlack() : 0.f; };
		const auto ArtAlpha = [W]() { const TSharedPtr<FWRFlow> F = W.Pin(); return F && F->ShowsLoadingArt() ? F->GetBlack() : 0.f; };
		const auto LineAlpha = [W]() { const TSharedPtr<FWRFlow> F = W.Pin(); return F && F->GetCover() == FWRFlow::ECover::Loading && !F->ShowsLoadingArt() && F->IsWaitingForIsland() ? F->GetBlack() : 0.f; };
		const auto CardAlpha = [W]() { const TSharedPtr<FWRFlow> F = W.Pin(); return F ? F->GetDeathCardAlpha() : 0.f; };
		// The cover fills the screen over the menus (layer 60 to their 20): left Visible, the empty cover took every
		// click meant for a menu, so none could be clicked. Its parts say for themselves what they let through.
		SetVisibility(EVisibility::SelfHitTestInvisible);
		ChildSlot
		[
			SNew(SOverlay).Visibility_Lambda([Black]() { return Black() > 0.002f ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			+ SOverlay::Slot()[SNew(SImage).Image(WRUI::White()).ColorAndOpacity_Lambda([Black]() { return FSlateColor(FLinearColor(0.f, 0.f, 0.f, Black())); })]
			+ SOverlay::Slot()[Fade(ArtAlpha, WRUIFlow::MakeLoadingScreen(LOCTEXT("LoadingIsland", "Loading the island…"), true))]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[Fade(LineAlpha, StatusLine(LOCTEXT("LoadingIsland", "Loading the island…"), 18))]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[Fade(CardAlpha, DeathCard(), FVector2D(0.f, 18.f * U))]
		];
	}

private:
	TWeakPtr<FWRFlow> Flow;

	const FWRDeathNote* Note() const { const TSharedPtr<FWRFlow> F = Flow.Pin(); return F ? &F->GetDeathNote() : nullptr; }

	FText KilledBy() const
	{
		const FWRDeathNote* N = Note();
		if (!N || N->Killer.IsEmpty()) return LOCTEXT("DiedUnknown", "Your journey ends here, for now");
		if (N->KillerCreature.IsNone() && !N->bKillerIsPlayer)
		{
			// A cause rather than a killer.
			if (N->Killer == TEXT("Hunger")) return LOCTEXT("DiedHunger", "You starved");
			if (N->Killer == TEXT("Thirst")) return LOCTEXT("DiedThirst", "You died of thirst");
			if (N->Killer == TEXT("Lava")) return LOCTEXT("DiedLava", "The lava took you");
			if (N->Killer == TEXT("Burns")) return LOCTEXT("DiedBurns", "You burned to death");
			if (N->Killer == TEXT("Frostbite")) return LOCTEXT("DiedFrost", "You froze to death");
		}
		return FText::Format(LOCTEXT("KilledBy", "Killed by {0}"), FText::FromString(N->Killer));
	}

	FText BagTitle() const
	{
		const FWRDeathNote* N = Note();
		if (!N) return FText::GetEmpty();
		if (N->Penalty == TEXT("gentle")) return LOCTEXT("BagKeep", "You keep your belongings");
		if (!N->bBag) return LOCTEXT("BagNone", "You carried nothing to lose");
		return N->Penalty == TEXT("souls") ? LOCTEXT("BagSouls", "Your belongings wait where you fell") : LOCTEXT("BagDrop", "Your belongings lie where you fell");
	}

	FText BagLine() const
	{
		const FWRDeathNote* N = Note();
		if (!N) return FText::GetEmpty();
		if (N->Penalty == TEXT("gentle")) return LOCTEXT("BagKeepSub", "This server's death penalty is gentle: nothing was dropped.");
		if (!N->bBag) return LOCTEXT("BagNoneSub", "Your bag was empty, so nothing was dropped.");
		const FVector D = N->BagAt - N->RespawnAt;
		const FString Where = N->RespawnName.IsEmpty() || N->RespawnAt.IsZero()
			? FString(TEXT("where you fell"))
			: FString::Printf(TEXT("%s %s of %s"), *WRUIWindowKit::Distance(D.Size2D()), WRUIWindowKit::CompassName(D), *N->RespawnName);
		const FString Stacks = FString::Printf(TEXT("%d %s"), N->BagStacks, N->BagStacks == 1 ? TEXT("stack") : TEXT("stacks"));
		const int32 Minutes = FMath::RoundToInt(N->BagMinutes);
		if (N->Penalty == TEXT("souls"))
		{
			return FText::FromString(FString::Printf(TEXT("%s, %s. Only you can pick them up, for %d minutes; die again first and they're lost."), *Stacks, *Where, Minutes));
		}
		return FText::FromString(FString::Printf(TEXT("%s, %s. Anyone can take them; they last %d minutes."), *Stacks, *Where, Minutes));
	}

	FText WakeTitle() const
	{
		const FWRDeathNote* N = Note();
		if (!N || N->RespawnName.IsEmpty()) return LOCTEXT("WakeCamp", "You'll wake at your camp");
		return FText::Format(LOCTEXT("WakeAt", "You'll wake at {0}"), FText::FromString(N->RespawnName));
	}

	FText WakeLine() const
	{
		const FWRDeathNote* N = Note();
		if (!N || N->RespawnAt.IsZero()) return FText::GetEmpty();
		const FVector D = N->RespawnAt - N->DeathAt;
		if (D.Size2D() < 3000.f) return LOCTEXT("WakeNear", "Close to where you fell.");
		return FText::FromString(FString::Printf(TEXT("%s %s of where you fell."), *WRUIWindowKit::Distance(D.Size2D()), WRUIWindowKit::CompassName(D)));
	}

	FText Countdown() const
	{
		const TSharedPtr<FWRFlow> F = Flow.Pin();
		if (!F) return FText::GetEmpty();
		if (F->IsBack()) return LOCTEXT("WakeLoading", "Loading the island…");
		const float Left = F->GetRespawnLeft();
		return Left > 0.05f ? FText::Format(LOCTEXT("WakeIn", "Waking in {0}"), FText::AsNumber(FMath::CeilToInt(Left))) : LOCTEXT("Waking", "Waking…");
	}

	TSharedRef<SWidget> Row(FName Glyph, TAttribute<FLinearColor> GlyphColor, TAttribute<FText> Title, TAttribute<FText> Line)
	{
		using WRUI::U;
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.f, 2.f * U, 16.f * U, 0.f)
			[
				SNew(SBox).WidthOverride(40.f * U).HeightOverride(40.f * U)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()[SNew(SImage).Image(WRUIWindowKit::Circle(FLinearColor(0.f, 0.f, 0.f, 0.45f), WRUI::Brass2, 1.5f))]
					+ SOverlay::Slot().Padding(9.f * U)[SNew(SImage).Image(WRFlowUI::Glyph(Glyph)).ColorAndOpacity_Lambda([GlyphColor]() { return FSlateColor(GlyphColor.Get()); })]
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(Title, WRUI::Body(18, 700), WRUI::Ink)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f * U, 0.f, 0.f)[WRUI::Text(Line, WRUI::Body(14, 500), WRUI::Ink2, false, true)]
			];
	}

	TSharedRef<SWidget> DeathCard()
	{
		using namespace WRFlowUI;
		const FLinearColor Blood(FColor(176, 44, 34));
		const float BarW = 360.f * U;
		return SNew(SBox).WidthOverride(700.f * U)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				Carved(LOCTEXT("YouDied", "YOU DIED"), Spaced(WRUI::Title(64), 160), Blood, FLinearColor(FColor(205, 70, 52)), FLinearColor(FColor(232, 120, 92)))
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 2.f * U, 0.f, 0.f)[Rule(150.f * U, WRUI::Brass)]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 16.f * U, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f * U, 0.f)
				[
					SNew(SBox).WidthOverride(50.f * U).HeightOverride(50.f * U).Visibility_Lambda([this]() { const FWRDeathNote* N = Note(); return N && WRUI::Portrait(N->KillerCreature) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
					[
						SNew(SOverlay)
						+ SOverlay::Slot()[SNew(SImage).Image(WRUI::Box(FLinearColor(FColor(20, 10, 8)), 5.f, WRUI::Brass2, 2.f))]
						+ SOverlay::Slot().Padding(3.f * U)[SNew(SImage).Image_Lambda([this]() { const FWRDeathNote* N = Note(); const FSlateBrush* B = N ? WRUI::Portrait(N->KillerCreature) : nullptr; return B ? B : WRUI::White(); })]
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return KilledBy(); }), WRUI::Title(22, 600), WRUI::Ink, true)]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 22.f * U, 0.f, 0.f)
			[
				SNew(SWRPanel).Kind(EWRPanel::Stone).Rivets(true).BrassTrim(true).Padding(FMargin(26.f * U, 20.f * U))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						Row(TEXT("map_pin"), TAttribute<FLinearColor>::CreateLambda([this]() { const FWRDeathNote* N = Note(); return N && N->bBag ? WRUI::Warn : WRUI::Good; }),
							TAttribute<FText>::CreateLambda([this]() { return BagTitle(); }), TAttribute<FText>::CreateLambda([this]() { return BagLine(); }))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(56.f * U, 14.f * U, 0.f, 14.f * U)[WRUI::Divider()]
					+ SVerticalBox::Slot().AutoHeight()
					[
						Row(TEXT("camp"), WRUI::Brass, TAttribute<FText>::CreateLambda([this]() { return WakeTitle(); }), TAttribute<FText>::CreateLambda([this]() { return WakeLine(); }))
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 26.f * U, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f * U, 0.f)
				[
					SNew(SBox).Visibility_Lambda([this]() { const TSharedPtr<FWRFlow> F = Flow.Pin(); return F && F->IsBack() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })[SNew(SWRSpinner).Size(24.f * U)]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return Countdown(); }), Spaced(WRUI::Title(22, 600), 80), WRUI::Ink, true)]
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 10.f * U, 0.f, 0.f)
			[
				SNew(SBox).WidthOverride(BarW).HeightOverride(6.f * U)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()[SNew(SImage).Image(WRUI::Box(WRUI::Alpha(WRUI::Brass2, 0.3f), 3.f))]
					+ SOverlay::Slot().HAlign(HAlign_Left).Padding(1.f)
					[
						SNew(SBox).WidthOverride_Lambda([this, BarW]() { const TSharedPtr<FWRFlow> F = Flow.Pin(); return FOptionalSize(FMath::Max(2.f, (BarW - 2.f) * (F ? F->GetRespawnFraction() : 0.f))); })
						[
							SNew(SImage).Image(WRUI::Box(WRUI::Brass, 2.f))
						]
					]
				]
			]
		];
	}
};

// ---------------------------------------------------------------- the moments layer: banners, toasts, hints

class SWRFlowMoments : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWRFlowMoments) {}
		SLATE_ARGUMENT(TWeakPtr<FWRFlow>, Flow)
		SLATE_ARGUMENT(TWeakObjectPtr<AWRPlayerController>, PC)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		using namespace WRFlowUI;
		Flow = InArgs._Flow;
		PC = InArgs._PC;
		TSharedRef<SVerticalBox> ToastColumn = SNew(SVerticalBox);
		for (int32 i = 0; i < 3; ++i)
		{
			ToastColumn->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f * U)[ToastSlot(i)];
		}
		// Over the menus too (layer 30): never in the way of a click (see SWRFlowCover).
		SetVisibility(EVisibility::SelfHitTestInvisible);
		ChildSlot
		[
			SNew(SOverlay).Visibility_Lambda([this]()
			{
				const TSharedPtr<FWRFlow> F = Flow.Pin();
				return !F || SWRMainMenu::IsOpen() || F->GetBlack() > 0.5f ? EVisibility::Collapsed : EVisibility::HitTestInvisible;
			})
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0.f, 128.f * U, 0.f, 0.f)[Banner(false)]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0.f, 186.f * U, 0.f, 0.f)[Banner(true)]
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(24.f * U, 128.f * U, 0.f, 0.f)[ToastColumn]
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(24.f * U, 0.f, 0.f, 40.f * U)[HintCard()]
		];
	}

private:
	TWeakPtr<FWRFlow> Flow;
	TWeakObjectPtr<AWRPlayerController> PC;

	const FWRFlow::FBanner* CurrentBanner(float& A) const { A = 0.f; const TSharedPtr<FWRFlow> F = Flow.Pin(); return F ? F->GetBanner(A) : nullptr; }

	/** The centre banner (level up, discovered, tamed) or, bBoss, the boss's title card. */
	TSharedRef<SWidget> Banner(bool bBoss)
	{
		using namespace WRFlowUI;
		const auto Mine = [this, bBoss](float& A) -> const FWRFlow::FBanner*
		{
			const FWRFlow::FBanner* B = CurrentBanner(A);
			return B && (B->Kind == FWRFlow::FBanner::EKind::Boss) == bBoss ? B : nullptr;
		};
		const auto Alpha = [Mine]() { float A = 0.f; return Mine(A) ? A : 0.f; };
		const auto Over = [Mine]() { float A; const FWRFlow::FBanner* B = Mine(A); return B ? B->Over : FText::GetEmpty(); };
		const auto Title = [Mine]() { float A; const FWRFlow::FBanner* B = Mine(A); return B ? B->Title : FText::GetEmpty(); };
		const auto Sub = [Mine]() { float A; const FWRFlow::FBanner* B = Mine(A); return B ? B->Sub : FText::GetEmpty(); };
		const auto Accent = [Mine]() { float A; const FWRFlow::FBanner* B = Mine(A); return B ? B->Accent : WRUI::Brass; };
		const auto Portrait = [Mine]() -> const FSlateBrush* { float A; const FWRFlow::FBanner* B = Mine(A); return B && !B->Portrait.IsNone() ? WRUI::Portrait(B->Portrait) : nullptr; };
		const auto GlyphOf = [Mine]() -> const FSlateBrush* { float A; const FWRFlow::FBanner* B = Mine(A); return B && !B->Glyph.IsNone() ? Glyph(B->Glyph) : nullptr; };
		const FLinearColor Night(FColor(8, 7, 6));
		const float Width = (bBoss ? 1000.f : 760.f) * U;
		const int32 TitleSize = bBoss ? 60 : 42;
		return Fade(Alpha, SNew(SBox).WidthOverride(Width)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()[SNew(SWRGradientFill).Colors({ WRUI::Alpha(Night, 0.f), WRUI::Alpha(Night, bBoss ? 0.86f : 0.74f), WRUI::Alpha(Night, bBoss ? 0.86f : 0.74f), WRUI::Alpha(Night, 0.f) }).Stops({ 0.f, 0.26f, 0.74f, 1.f })]
			+ SOverlay::Slot().VAlign(VAlign_Top)[SNew(SBox).HeightOverride(1.f)[SNew(SWRGradientFill).Colors({ WRUI::Alpha(WRUI::Brass2, 0.f), WRUI::Brass2, WRUI::Brass2, WRUI::Alpha(WRUI::Brass2, 0.f) }).Stops({ 0.f, 0.3f, 0.7f, 1.f })]]
			+ SOverlay::Slot().VAlign(VAlign_Bottom)[SNew(SBox).HeightOverride(1.f)[SNew(SWRGradientFill).Colors({ WRUI::Alpha(WRUI::Brass2, 0.f), WRUI::Brass2, WRUI::Brass2, WRUI::Alpha(WRUI::Brass2, 0.f) }).Stops({ 0.f, 0.3f, 0.7f, 1.f })]]
			+ SOverlay::Slot().HAlign(HAlign_Center).Padding(0.f, (bBoss ? 20.f : 14.f) * U, 0.f, (bBoss ? 22.f : 16.f) * U)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(90.f * U).HeightOverride(1.f)[SNew(SWRGradientFill).Colors({ WRUI::Alpha(WRUI::Brass2, 0.f), WRUI::Brass2 })]]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(14.f * U, 0.f)
					[
						WRUI::Text(TAttribute<FText>::CreateLambda(Over), Spaced(WRUI::Caps(bBoss ? 17 : 15), 320), TAttribute<FSlateColor>::CreateLambda([Accent]() { return FSlateColor(Accent()); }), true)
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(90.f * U).HeightOverride(1.f)[SNew(SWRGradientFill).Colors({ WRUI::Brass2, WRUI::Alpha(WRUI::Brass2, 0.f) })]]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 2.f * U, 0.f, 0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 16.f * U, 0.f)
					[
						SNew(SBox).WidthOverride((bBoss ? 76.f : 60.f) * U).HeightOverride((bBoss ? 76.f : 60.f) * U)
						.Visibility_Lambda([Portrait, GlyphOf]() { return Portrait() || GlyphOf() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
						[
							SNew(SOverlay)
							+ SOverlay::Slot()[SNew(SImage).Image(WRUI::Box(FLinearColor(FColor(16, 13, 10)), 6.f, WRUI::Brass2, 2.f))]
							+ SOverlay::Slot().Padding(3.f * U)[SNew(SImage).Visibility_Lambda([Portrait]() { return Portrait() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; }).Image_Lambda([Portrait]() { const FSlateBrush* B = Portrait(); return B ? B : WRUI::White(); })]
							+ SOverlay::Slot().Padding(15.f * U)[SNew(SImage).Visibility_Lambda([GlyphOf]() { return GlyphOf() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; }).Image_Lambda([GlyphOf]() { const FSlateBrush* B = GlyphOf(); return B ? B : WRUI::White(); })
								.ColorAndOpacity_Lambda([Accent]() { return FSlateColor(Accent()); })]
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Gold(TAttribute<FText>::CreateLambda(Title), TitleSize, bBoss ? 140 : 70)]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 2.f * U, 0.f, 0.f)
				[
					WRUI::Text(TAttribute<FText>::CreateLambda(Sub), bBoss ? Spaced(WRUI::Title(20, 600), 60) : WRUI::Body(17, 500), bBoss ? WRUI::Ink : WRUI::Ink2, true)
				]
			]
		], FVector2D(0.f, -14.f * U));
	}

	TSharedRef<SWidget> ToastSlot(int32 Slot)
	{
		using namespace WRFlowUI;
		const auto Get = [this, Slot](float& A) -> const FWRFlow::FToast* { A = 0.f; const TSharedPtr<FWRFlow> F = Flow.Pin(); return F ? F->GetToast(Slot, A) : nullptr; };
		const auto Alpha = [Get]() { float A; return Get(A) ? A : 0.f; };
		const auto Accent = [Get]() { float A; const FWRFlow::FToast* T = Get(A); return T ? T->Accent : WRUI::Brass; };
		return Fade(Alpha, SNew(SBox).WidthOverride(330.f * U)
		[
			SNew(SWRPanel).Kind(EWRPanel::Glass).Rivets(true).Padding(FMargin(14.f * U, 11.f * U, 16.f * U, 12.f * U))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f * U, 0.f)
				[
					SNew(SBox).WidthOverride(46.f * U).HeightOverride(46.f * U)
					[
						SNew(SOverlay)
						+ SOverlay::Slot()[SNew(SImage).Image_Lambda([Accent]() { return WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.45f), 4.f, WRUI::Alpha(Accent(), 0.8f), 1.5f); })]
						+ SOverlay::Slot().Padding(5.f * U)[SNew(SImage).Image_Lambda([Get]() { float A; const FWRFlow::FToast* T = Get(A); return T && !T->Icon.IsNone() ? WRUI::Icon(T->Icon) : WRUI::Icon(TEXT("_unknown")); })]
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda([Get]() { float A; const FWRFlow::FToast* T = Get(A); return T ? T->Over : FText::GetEmpty(); }), Spaced(WRUI::Caps(12), 160),
						TAttribute<FSlateColor>::CreateLambda([Accent]() { return FSlateColor(Accent()); }))]
					+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(TAttribute<FText>::CreateLambda([Get]() { float A; const FWRFlow::FToast* T = Get(A); return T ? T->Title : FText::GetEmpty(); }), WRUI::Title(17), WRUI::Ink, true)]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f * U, 0.f, 0.f)[WRUI::Text(TAttribute<FText>::CreateLambda([Get]() { float A; const FWRFlow::FToast* T = Get(A); return T ? T->Sub : FText::GetEmpty(); }), WRUI::Body(13, 500), WRUI::Ink2, false, true)]
				]
			]
		], FVector2D(-28.f * U, 0.f));
	}

	TSharedRef<SWidget> HintCard()
	{
		using namespace WRFlowUI;
		const auto Get = [this](float& A) -> const FWRFlow::FHint* { A = 0.f; const TSharedPtr<FWRFlow> F = Flow.Pin(); return F ? F->GetHint(A) : nullptr; };
		const auto Alpha = [this, Get]()
		{
			// Out of the way while a screen is open (it waits; its time doesn't run out unseen).
			const FWRPlayerUI* X = PC.IsValid() ? PC->GetUI() : nullptr;
			float A;
			return Get(A) && !(X && X->IsScreenOpen()) ? A : 0.f;
		};
		return Fade(Alpha, SNew(SBox).WidthOverride(400.f * U)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SWRPanel).Kind(EWRPanel::Glass).Rivets(true).Padding(FMargin(18.f * U, 12.f * U, 18.f * U, 14.f * U))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f * U, 0.f)[SNew(SWRLineIcon).Icon(EWRIcon::Bulb).Size(16.f * U).Color(WRUI::Warn)]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(LOCTEXT("HintOver", "Hint"), Spaced(WRUI::Caps(13), 200), WRUI::Brass)]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f * U, 0.f, 0.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.f, 1.f * U, 12.f * U, 0.f)
						[
							SNew(SBox).Visibility_Lambda([Get]() { float A; const FWRFlow::FHint* H = Get(A); return H && !H->Action.IsNone() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
							[
								SNew(SWRKeyCap).FontSize(14).Text_Lambda([Get]() { float A; const FWRFlow::FHint* H = Get(A); return H ? KeyText(H->Action) : FText::GetEmpty(); })
							]
						]
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
						[
							WRUI::Text(TAttribute<FText>::CreateLambda([Get]() { float A; const FWRFlow::FHint* H = Get(A); return H ? H->Text : FText::GetEmpty(); }), WRUI::Body(16, 500), WRUI::Ink, false, true)
						]
					]
				]
			]
			// A brass edge down the left side marks it as a hint.
			+ SOverlay::Slot().HAlign(HAlign_Left).Padding(0.f, 8.f * U)[SNew(SBox).WidthOverride(3.f * U)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity(WRUI::Brass)]]
		], FVector2D(-28.f * U, 0.f));
	}
};

// ---------------------------------------------------------------- FWRFlow

namespace WRFlowUI
{
	float Tune(const TCHAR* Key, float Fallback) { return WRTune(*(FString(TEXT("flow.")) + Key), Fallback); }
}

FWRFlow::FWRFlow(AWRPlayerController* InPC) : PC(InPC)
{
	// A game world starts black: the island has to stream in around you (or the menu's view) before it shows.
	Cover = ECover::Loading;
	// Joining or hosting carries on from the movie player's loading screen with the same picture; the menu's own
	// island (standalone) just waits in black behind the menu.
	bLoadingArt = InPC && InPC->GetNetMode() != NM_Standalone;
	Black = 1.f;
	BlackTarget = 1.f;
	CoverStart = WRUIMotion::Now();
}

FWRFlow::~FWRFlow()
{
	if (GEngine && GEngine->GameViewport)
	{
		for (const TSharedPtr<SWidget>& W : { MomentsWidget, CoverWidget })
		{
			if (W.IsValid()) GEngine->GameViewport->RemoveViewportWidgetContent(W.ToSharedRef());
		}
	}
	if (ACameraActor* Cam = TestCam.Get())
	{
		Cam->Destroy();
	}
}

bool FWRFlow::HintsEnabled()
{
	if (WRFlowUI::HintsOverride.IsSet()) return WRFlowUI::HintsOverride.GetValue();
	bool bOn = true;
	GConfig->GetBool(WRFlowUI::MemorySection, TEXT("ShowHints"), bOn, GGameUserSettingsIni);
	return bOn;
}

void FWRFlow::SetHintsEnabled(bool bOn)
{
	WRFlowUI::HintsOverride.Reset();
	GConfig->SetBool(WRFlowUI::MemorySection, TEXT("ShowHints"), bOn, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void FWRFlow::OnDeathNote(const FWRDeathNote& Note)
{
	AWRPlayerController* P = GetPC();
	const double Now = WRUIMotion::Now();
	Death = Note;
	DeathAt = Now;
	Cover = ECover::Dying;
	bBack = false;
	bStreaming = false;
	bReleasing = false;
	CardShownAt = -1.0;
	CoverStart = Now;
	DeadBody = LastBody;
	if (Note.bBag)
	{
		bBagMarker = true;
		BagUntil = Now + Note.BagMinutes * 60.0;
	}
	if (P && P->GetMenu() != EWRMenu::None)
	{
		P->SetMenu(EWRMenu::None);
	}
	Hints.RemoveAll([](const FHint& H) { return H.Start >= 0.0; });
	UE_LOG(LogWildreach, Log, TEXT("FLOW death screen: '%s' (%s), penalty %s, bag %s, waking at '%s' in %.0f s"), *Note.Killer, *Note.KillerCreature.ToString(), *Note.Penalty,
		Note.bBag ? *FString::Printf(TEXT("%d stacks"), Note.BagStacks) : TEXT("none"), *Note.RespawnName, Note.Seconds);
	if (ShotOnDeath >= 0.f)
	{
		ShotAt = Now + ShotOnDeath;
		ShotOnDeath = -1.f;
	}
}

float FWRFlow::GetDeathCardAlpha() const
{
	if (Cover != ECover::Dying || CardShownAt < 0.0) return 0.f;
	const float In = WRUIMotion::EaseOut(float(WRUIMotion::Now() - CardShownAt) / 0.45f);
	return bReleasing ? FMath::Min(In, Black) : In;
}

float FWRFlow::GetRespawnLeft() const
{
	return Death.Seconds - float(WRUIMotion::Now() - DeathAt);
}

float FWRFlow::GetRespawnFraction() const
{
	return bBack ? 1.f : FMath::Clamp(1.f - GetRespawnLeft() / FMath::Max(0.1f, Death.Seconds), 0.f, 1.f);
}

bool FWRFlow::GetBagMarker(FVector& OutLocation) const
{
	if (!bBagMarker || WRUIMotion::Now() > BagUntil) return false;
	OutLocation = Death.BagAt;
	return true;
}

FVector FWRFlow::FocusLocation() const
{
	const AWRPlayerController* P = GetPC();
	if (!P) return FVector::ZeroVector;
	if (!SWRMainMenu::IsOpen() && P->GetPawn())
	{
		return P->GetPawn()->GetActorLocation();
	}
	return P->PlayerCameraManager ? P->PlayerCameraManager->GetCameraLocation() : P->GetFocalLocation();
}

bool FWRFlow::IsIslandStreamed(const FVector& At) const
{
	const AWRPlayerController* P = GetPC();
	UWorld* World = P ? P->GetWorld() : nullptr;
	const UWorldPartitionSubsystem* WP = World ? World->GetSubsystem<UWorldPartitionSubsystem>() : nullptr;
	if (!WP || !World->GetWorldPartition())
	{
		return true;   // not a streamed world: nothing to wait for
	}
	// The cells close around you (the ground you stand on and what's in front of you), not the whole loading range.
	FWorldPartitionStreamingQuerySource Query(At);
	Query.bUseGridLoadingRange = false;
	Query.Radius = WRFlowUI::Tune(TEXT("streamRadius"), 6000.f);
	return WP->IsStreamingCompleted(EWorldPartitionRuntimeCellState::Activated, { Query }, false);
}

void FWRFlow::StartLoading(bool bArt, float FadeSeconds)
{
	if (Cover == ECover::Dying) return;
	if (Cover != ECover::Loading)
	{
		CoverStart = WRUIMotion::Now();
	}
	Cover = ECover::Loading;
	bLoadingArt = bArt;
	bReleasing = false;
	BlackTarget = 1.f;
	BlackRate = FadeSeconds > 0.f ? 1.f / FadeSeconds : 1000.f;
	if (FadeSeconds <= 0.f) Black = 1.f;
}

void FWRFlow::StartIslandWait(const FVector& At)
{
	bStreaming = true;
	StreamStart = WRUIMotion::Now();
	NextStreamCheck = 0.0;
	UE_LOG(LogWildreach, Log, TEXT("FLOW waiting for the island at %s"), *At.ToCompactString());
}

void FWRFlow::TakeShot(const TCHAR* Why)
{
	if (AWRPlayerController* P = GetPC())
	{
		UE_LOG(LogWildreach, Log, TEXT("FLOW screenshot (%s)"), Why);
		P->ConsoleCommand(TEXT("shot showui"));
	}
}

void FWRFlow::Tick(float DeltaTime)
{
	AWRPlayerController* P = GetPC();
	if (!P || !GEngine || !GEngine->GameViewport)
	{
		return;
	}
	if (!CoverWidget.IsValid())
	{
		// Made here rather than in the constructor: the widgets hold a weak pointer to this.
		CoverWidget = SNew(SWRFlowCover).Flow(AsShared());
		MomentsWidget = SNew(SWRFlowMoments).Flow(AsShared()).PC(P);
		GEngine->GameViewport->AddViewportWidgetContent(MomentsWidget.ToSharedRef(), 30);
		GEngine->GameViewport->AddViewportWidgetContent(CoverWidget.ToSharedRef(), 60);
	}
	const double Now = WRUIMotion::Now();
	if (ShotAt >= 0.0 && Now >= ShotAt)
	{
		ShotAt = -1.0;
		TakeShot(TEXT("scheduled"));
	}

	// Who we are now: back from the dead, a first body (joined), a mount, or a long jump.
	APawn* Pawn = P->GetPawn();
	const AWRCharacterBase* Body = Cast<AWRCharacterBase>(Pawn);
	const bool bAlive = Body && Body->IsAlive();
	const float JumpCm = WRFlowUI::Tune(TEXT("teleportCm"), 6000.f);
	// Back from the dead: a new living body. Checked every frame, not only when the pawn changes: the new body's
	// attributes (on the player state) can land a frame or two after the pawn does.
	if (Cover == ECover::Dying && !bBack && !bPreviewHold && Pawn && bAlive && Pawn != DeadBody.Get())
	{
		bBack = true;
		StartIslandWait(Pawn->GetActorLocation());
		UE_LOG(LogWildreach, Log, TEXT("FLOW back from the dead at %s (%.1f s after dying)"), *Pawn->GetActorLocation().ToCompactString(), Now - DeathAt);
		if (ShotOnBack >= 0.f)
		{
			ShotAt = Now + ShotOnBack;
			ShotOnBack = -1.f;
		}
	}
	if (Pawn != LastPawn.Get())
	{
		if (Pawn && LastPawn.IsValid() && Cover == ECover::None && FVector::Dist(Pawn->GetActorLocation(), LastPawnAt) > JumpCm && !IsIslandStreamed(Pawn->GetActorLocation()))
		{
			StartLoading(false, WRFlowUI::Tune(TEXT("jumpFadeSeconds"), 0.15f));
			StartIslandWait(Pawn->GetActorLocation());
		}
		LastPawn = Pawn;
	}
	else if (Pawn && Cover == ECover::None && FVector::Dist(Pawn->GetActorLocation(), LastPawnAt) > JumpCm && !IsIslandStreamed(Pawn->GetActorLocation()))
	{
		// A teleport: don't show the unloaded island (a flat sea where the ground should be) while it streams in.
		StartLoading(false, WRFlowUI::Tune(TEXT("jumpFadeSeconds"), 0.15f));
		StartIslandWait(Pawn->GetActorLocation());
	}
	if (Pawn)
	{
		LastPawnAt = Pawn->GetActorLocation();
	}
	if (bAlive && Cast<AWRCharacter>(Pawn) && Cover != ECover::Dying)
	{
		LastBody = Pawn;
	}
	// How long we've been alive in this body (moments wait a moment after arriving; see TickMoments).
	if (!bAlive || (Cover == ECover::Dying && !bBack))
	{
		AliveSince = -1.0;
	}
	else if (AliveSince < 0.0)
	{
		AliveSince = Now;
	}

	// Joining (or the menu): wait for the island once there is something to look at.
	if (Cover == ECover::Loading && !bStreaming && !bReleasing && !bPreviewHold)
	{
		const bool bMenu = SWRMainMenu::IsOpen();
		if (bMenu) bLoadingArt = false;
		if ((bMenu && Cast<ACameraActor>(P->GetViewTarget())) || (!bMenu && Pawn))
		{
			StartIslandWait(FocusLocation());
		}
		else if (Now - CoverStart > WRFlowUI::Tune(TEXT("streamMaxSeconds"), 15.f) * 2.f)
		{
			bReleasing = true;   // nothing to wait for (a spectator, a server without a pawn for us)
		}
	}
	if (bStreaming && Now >= NextStreamCheck)
	{
		NextStreamCheck = Now + 0.1;
		const float Waited = float(Now - StreamStart);
		const bool bDone = IsIslandStreamed(FocusLocation());
		const bool bTimedOut = Waited > WRFlowUI::Tune(TEXT("streamMaxSeconds"), 15.f);
		if (bDone || bTimedOut)
		{
			bStreaming = false;
			UE_LOG(LogWildreach, Log, TEXT("FLOW island %s after %.1f s at %s"), bDone ? TEXT("streamed in") : TEXT("still streaming (gave up waiting)"), Waited, *FocusLocation().ToCompactString());
			if (Cover == ECover::Loading || (Cover == ECover::Dying && bBack))
			{
				bReleasing = true;
			}
		}
	}
	if (bPreviewHold && PreviewUntil >= 0.0 && Now >= PreviewUntil)
	{
		bPreviewHold = false;
		PreviewUntil = -1.0;
		bStreaming = false;
		bReleasing = true;
	}
	TickCover(float(FMath::Min(Now - LastTickAt, 0.1)));
	LastTickAt = Now;
	TickMoments();
}

void FWRFlow::TickCover(float Dt)
{
	AWRPlayerController* P = GetPC();
	const double Now = WRUIMotion::Now();
	if (Cover == ECover::None)
	{
		Black = 0.f;
		return;
	}
	if (Cover == ECover::Dying)
	{
		// A moment to see yourself fall, then black, then the card.
		if (!bReleasing && Now - DeathAt > WRFlowUI::Tune(TEXT("deathHoldSeconds"), 0.8f))
		{
			BlackTarget = 1.f;
			BlackRate = 1.f / FMath::Max(0.05f, WRFlowUI::Tune(TEXT("deathFadeSeconds"), 1.f));
		}
		if (CardShownAt < 0.0 && Black > 0.85f)
		{
			CardShownAt = Now;
		}
		// Keep the camera on the falling body until it's dark (the server points the view at the bare controller).
		APawn* Dead = DeadBody.Get();
		if (P && Dead && !bBack && Black < 0.98f && P->GetViewTarget() != Dead)
		{
			P->SetViewTarget(Dead);
		}
		// The card stays until it has been read a moment, even when the island is quick.
		if (bReleasing && (Black < 0.999f && BlackTarget > 0.5f))
		{
			// Not fully dark yet: finish going dark first.
		}
		else if (bReleasing && Now - CardShownAt < WRFlowUI::Tune(TEXT("deathCardMinSeconds"), 2.f))
		{
			BlackTarget = 1.f;
		}
		else if (bReleasing)
		{
			if (BlackTarget > 0.f)
			{
				WRSound::PlayUI(P, TEXT("respawn"));
			}
			BlackTarget = 0.f;
			BlackRate = 1.f / FMath::Max(0.05f, WRFlowUI::Tune(TEXT("fadeInSeconds"), 0.7f));
		}
	}
	else if (bReleasing && BlackTarget > 0.f)
	{
		BlackTarget = 0.f;
		BlackRate = 1.f / FMath::Max(0.05f, WRFlowUI::Tune(TEXT("fadeInSeconds"), 0.7f));
	}
	Black = FMath::FInterpConstantTo(Black, BlackTarget, Dt, BlackRate);
	if (bReleasing && BlackTarget <= 0.f && Black <= 0.001f)
	{
		Black = 0.f;
		Cover = ECover::None;
		bReleasing = false;
		bBack = false;
		bLoadingArt = false;
		CardShownAt = -1.0;
		DeadBody.Reset();
	}
}

// ---------------------------------------------------------------- moments

const FWRFlow::FBanner* FWRFlow::GetBanner(float& OutAlpha) const
{
	OutAlpha = 0.f;
	if (Banners.Num() == 0 || Banners[0].Start < 0.0) return nullptr;
	const FBanner& B = Banners[0];
	OutAlpha = WRUIMotion::InHoldOut(WRUIMotion::Now() - B.Start, 0.3f, B.Seconds, 0.5f);
	return &B;
}

const FWRFlow::FToast* FWRFlow::GetToast(int32 Slot, float& OutAlpha) const
{
	OutAlpha = 0.f;
	int32 Shown = 0;
	for (const FToast& T : Toasts)
	{
		if (T.Start < 0.0) continue;
		if (Shown++ == Slot)
		{
			OutAlpha = WRUIMotion::InHoldOut(WRUIMotion::Now() - T.Start, 0.25f, T.Seconds, 0.4f);
			return &T;
		}
	}
	return nullptr;
}

const FWRFlow::FHint* FWRFlow::GetHint(float& OutAlpha) const
{
	OutAlpha = 0.f;
	if (Hints.Num() == 0 || Hints[0].Start < 0.0) return nullptr;
	OutAlpha = WRUIMotion::InHoldOut(WRUIMotion::Now() - Hints[0].Start, 0.3f, Hints[0].Seconds, 0.5f);
	return &Hints[0];
}

void FWRFlow::ShowBanner(const FBanner& Banner, bool bFirst)
{
	if (bFirst)
	{
		// A boss's title card cuts in; what was showing comes back after it.
		if (Banners.Num() > 0) Banners[0].Start = -1.0;
		Banners.Insert(Banner, 0);
	}
	else
	{
		Banners.Add(Banner);
	}
	Banners[bFirst ? 0 : Banners.Num() - 1].Start = -1.0;
	UE_LOG(LogWildreach, Log, TEXT("FLOW banner: %s | %s | %s"), *Banner.Over.ToString(), *Banner.Title.ToString(), *Banner.Sub.ToString());
}

void FWRFlow::ShowToast(const FToast& Toast)
{
	Toasts.Add(Toast);
	Toasts.Last().Start = -1.0;
	UE_LOG(LogWildreach, Log, TEXT("FLOW toast: %s | %s | %s"), *Toast.Over.ToString(), *Toast.Title.ToString(), *Toast.Sub.ToString());
}

void FWRFlow::ShowHint(FName Id, FName Action, const FText& Text)
{
	FHint H;
	H.Id = Id;
	H.Action = Action;
	H.Text = Text;
	H.Seconds = WRFlowUI::Tune(TEXT("hintSeconds"), 9.f);
	Hints.Add(H);
	Seen.Add(Id);
	SaveMemory();
	UE_LOG(LogWildreach, Log, TEXT("FLOW hint %s: [%s] %s"), *Id.ToString(), *WRFlowUI::KeyText(Action).ToString(), *Text.ToString());
}

bool FWRFlow::WasSeen(FName HintId) const
{
	return Seen.Contains(HintId) || Hints.ContainsByPredicate([HintId](const FHint& H) { return H.Id == HintId; });
}

FString FWRFlow::MemoryKey(const TCHAR* What) const
{
	return FString::Printf(TEXT("%s_%s"), What, *MemoryFor);
}

void FWRFlow::LoadMemory()
{
	const AWRPlayerController* P = GetPC();
	const APlayerState* PS = P ? P->PlayerState.Get() : nullptr;
	const FString Who = PS ? PS->GetPlayerName() : FString();
	if (Who.IsEmpty() || Who == MemoryFor) return;
	MemoryFor = Who;
	Found.Reset();
	Seen.Reset();
	FString Saved;
	if (GConfig->GetString(WRFlowUI::MemorySection, *MemoryKey(TEXT("Found")), Saved, GGameUserSettingsIni))
	{
		TArray<FString> Parts;
		Saved.ParseIntoArray(Parts, TEXT("|"), true);
		Found.Append(Parts);
	}
	if (GConfig->GetString(WRFlowUI::MemorySection, *MemoryKey(TEXT("Hints")), Saved, GGameUserSettingsIni))
	{
		TArray<FString> Parts;
		Saved.ParseIntoArray(Parts, TEXT(","), true);
		for (const FString& S : Parts) Seen.Add(FName(*S));
	}
}

void FWRFlow::SaveMemory() const
{
	if (MemoryFor.IsEmpty()) return;
	GConfig->SetString(WRFlowUI::MemorySection, *MemoryKey(TEXT("Found")), *FString::Join(Found.Array(), TEXT("|")), GGameUserSettingsIni);
	TArray<FString> Ids;
	for (const FName& N : Seen) Ids.Add(N.ToString());
	GConfig->SetString(WRFlowUI::MemorySection, *MemoryKey(TEXT("Hints")), *FString::Join(Ids, TEXT(",")), GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void FWRFlow::TickMoments()
{
	AWRPlayerController* P = GetPC();
	const double Now = WRUIMotion::Now();
	// Queues move on: the current banner and hint, up to three toasts at once.
	if (Banners.Num() > 0 && Banners[0].Start >= 0.0 && Now - Banners[0].Start > Banners[0].Seconds) Banners.RemoveAt(0);
	if (Banners.Num() > 0 && Banners[0].Start < 0.0 && Cover == ECover::None) Banners[0].Start = Now;
	Toasts.RemoveAll([Now](const FToast& T) { return T.Start >= 0.0 && Now - T.Start > T.Seconds; });
	int32 Live = 0;
	for (FToast& T : Toasts)
	{
		if (T.Start >= 0.0) ++Live;
		else if (Live < 3 && Cover == ECover::None) { T.Start = Now; ++Live; }
	}
	const FWRPlayerUI* X = P ? P->GetUI() : nullptr;
	const bool bScreen = X && X->IsScreenOpen();
	if (Hints.Num() > 0 && Hints[0].Start >= 0.0 && Now - Hints[0].Start > Hints[0].Seconds) Hints.RemoveAt(0);
	if (Hints.Num() > 0 && Hints[0].Start < 0.0 && Cover == ECover::None && !bScreen) Hints[0].Start = Now;
	if (Hints.Num() > 0 && Hints[0].Start >= 0.0 && bScreen) Hints[0].Start = Now - 0.3;   // wait behind the screen

	if (!P || Now < NextMomentCheck || bPreviewHold) return;
	NextMomentCheck = Now + 0.25;
	AWRCharacter* Survivor = Cast<AWRCharacter>(P->GetPawn());
	const APawn* Pawn = P->GetPawn();
	if (!Pawn || SWRMainMenu::IsOpen() || (Cover != ECover::None && Cover != ECover::Loading)) return;
	LoadMemory();
	const FVector At = Pawn->GetActorLocation();
	// Just arrived: replicated progress is still landing, so take it as the starting point rather than news.
	const bool bSettled = AliveSince >= 0.0 && Now - AliveSince > WRFlowUI::Tune(TEXT("settleSeconds"), 3.f) && Cover == ECover::None;
	const UWRPathComponent* Paths = Survivor ? UWRPathComponent::FindFor(Survivor) : nullptr;
	if (!bSettled || bQuiet)
	{
		if (Paths)
		{
			LastLevel = Paths->GetCharacterLevel();
			LastUnspent = Paths->GetEarned(Paths->GetMainPath()) - Paths->GetSpent(Paths->GetMainPath());
		}
		if (bQuiet)
		{
			// Keep up with the world so nothing fires late when moments come back on.
			for (TActorIterator<AWRHearth> It(P->GetWorld()); It; ++It) HearthLevels.Add(*It, It->GetTechLevel());
			WildSeen.Reset();
			for (TActorIterator<AWRBoss> It(P->GetWorld()); It; ++It) if (It->bEngaged) CardedBosses.Add(*It);
		}
		return;
	}
	CheckLevel(Paths);
	CheckBosses(At);
	CheckTames(At);
	if (Now >= NextSlowCheck)
	{
		NextSlowCheck = Now + 0.5;
		CheckDiscoveries(At);
		CheckHearths(At);
		if (Survivor && Survivor->IsAlive()) CheckHints(At);
		// The bag's marker goes once you're there and the bag isn't (picked up, or taken).
		if (bBagMarker && FVector::Dist(At, Death.BagAt) < 1500.f)
		{
			bool bThere = false;
			for (TActorIterator<AWRLootBag> It(P->GetWorld()); It && !bThere; ++It)
			{
				bThere = FVector::Dist(It->GetActorLocation(), Death.BagAt) < 300.f;
			}
			bBagMarker = bThere;
		}
	}
}

void FWRFlow::CheckLevel(const UWRPathComponent* Paths)
{
	if (!Paths) return;
	AWRPlayerController* P = GetPC();
	const int32 Level = Paths->GetCharacterLevel();
	if (LastLevel > 0 && Level > LastLevel)
	{
		FBanner B;
		B.Kind = FBanner::EKind::Level;
		B.Over = LOCTEXT("LevelUp", "Level up");
		B.Title = FText::Format(LOCTEXT("LevelN", "Level {0}"), Level);
		const int32 Hp = FMath::RoundToInt(FWRPathRules::LevelStat(WRTune(TEXT("player.health"), 100.f), WRTune(TEXT("player.healthAtLevel50"), 250.f), Level));
		const int32 Sp = FMath::RoundToInt(FWRPathRules::LevelStat(WRTune(TEXT("player.stamina"), 100.f), WRTune(TEXT("player.staminaAtLevel50"), 150.f), Level));
		B.Sub = FText::Format(LOCTEXT("LevelSub", "Max health {0}  ·  max stamina {1}"), Hp, Sp);
		B.Accent = WRUI::Brass;
		B.Seconds = WRFlowUI::Tune(TEXT("bannerSeconds"), 4.f);
		ShowBanner(B);
		WRSound::PlayUI(P, TEXT("levelup"));
		if (const APawn* Pawn = P ? P->GetPawn() : nullptr) WRFx::SpawnAt(P, TEXT("levelup"), Pawn->GetActorLocation() - FVector(0.f, 0.f, Pawn->GetSimpleCollisionHalfHeight()));
	}
	LastLevel = Level;
	// A new point to spend on the main path.
	const FName Main = Paths->GetMainPath();
	const int32 Unspent = Paths->GetEarned(Main) - Paths->GetSpent(Main);
	if (LastUnspent >= 0 && Unspent > LastUnspent)
	{
		FToast T;
		T.Over = LOCTEXT("PathPoint", "Path point");
		T.Title = Unspent == 1 ? LOCTEXT("OnePoint", "1 point to spend") : FText::Format(LOCTEXT("NPoints", "{0} points to spend"), Unspent);
		T.Sub = FText::Format(LOCTEXT("PointSub", "Open Paths ({0}) to learn abilities and passives."), WRFlowUI::KeyText(TEXT("Paths")));
		T.Icon = Main == TEXT("frost") ? FName(TEXT("frost_essence")) : FName(TEXT("ember_essence"));
		T.Accent = Main == TEXT("frost") ? WRUI::Frost : WRUI::Ember;
		ShowToast(T);
	}
	LastUnspent = Unspent;
}

void FWRFlow::CheckDiscoveries(const FVector& At)
{
	if (MemoryFor.IsEmpty()) return;
	AWRPlayerController* P = GetPC();
	for (const FWRPoi& Poi : FWRPoi::All)
	{
		if (Found.Contains(Poi.Name)) continue;
		const bool bBoss = Poi.Kind == TEXT("boss");
		const bool bMini = Poi.Kind == TEXT("mini-boss");
		const float Radius = WRFlowUI::Tune(*(TEXT("discoverRadius.") + Poi.Kind.Replace(TEXT("-"), TEXT(""))), Poi.Kind == TEXT("cave") ? 2500.f : bMini ? 3500.f : bBoss ? 4500.f : 4000.f);
		if (FVector::Dist2D(Poi.Location, At) > Radius) continue;
		Found.Add(Poi.Name);
		SaveMemory();
		FBanner B;
		B.Kind = FBanner::EKind::Discover;
		B.Title = FText::FromString(Poi.Name);
		B.Accent = WRUI::Brass;
		B.Seconds = WRFlowUI::Tune(TEXT("discoverSeconds"), 5.f);
		if (Poi.Kind == TEXT("camp"))
		{
			B.Over = LOCTEXT("FoundCamp", "Discovered  ·  camp");
			B.Sub = LOCTEXT("FoundCampSub", "Safe ground: no fighting between players, and wild creatures won't follow you in.");
			B.Glyph = TEXT("camp");
			B.Accent = WRUI::Good;
		}
		else if (Poi.Kind == TEXT("cave"))
		{
			B.Over = LOCTEXT("FoundCave", "Discovered  ·  cave");
			B.Sub = LOCTEXT("FoundCaveSub", "Rich ore lies deep inside, and something lives in the dark.");
			B.Glyph = TEXT("cave");
		}
		else if (Poi.Kind == TEXT("pass"))
		{
			B.Over = LOCTEXT("FoundPass", "Discovered  ·  pass");
			B.Sub = LOCTEXT("FoundPassSub", "A way over the ridge between the volcanic west and the frozen east.");
			B.Glyph = TEXT("mountain");
		}
		else if (bBoss || bMini)
		{
			B.Over = bBoss ? LOCTEXT("FoundBoss", "Discovered  ·  boss lair") : LOCTEXT("FoundMini", "Discovered  ·  mini-boss lair");
			B.Sub = bBoss ? LOCTEXT("FoundBossSub", "A major boss lives here. Its core raises a Hearth's tech level.") : LOCTEXT("FoundMiniSub", "A mini-boss guards this place. It drops upgrade shards.");
			B.Glyph = TEXT("skull");
			B.Accent = WRUI::Bad;
		}
		else
		{
			B.Over = LOCTEXT("Found", "Discovered");
		}
		ShowBanner(B);
		WRSound::PlayUI(P, TEXT("discover"));
		WRFx::SpawnAt(P, TEXT("discover"), At);
		return;   // one at a time
	}
}

void FWRFlow::CheckBosses(const FVector& At)
{
	AWRPlayerController* P = GetPC();
	const float Range = WRFlowUI::Tune(TEXT("bossCardRange"), 6000.f);
	for (TActorIterator<AWRBoss> It(P->GetWorld()); It; ++It)
	{
		AWRBoss* Boss = *It;
		if (!Boss->bEngaged || !Boss->IsAlive())
		{
			CardedBosses.Remove(Boss);
			continue;
		}
		if (CardedBosses.Contains(Boss) || FVector::Dist(Boss->GetActorLocation(), At) > Range) continue;
		CardedBosses.Add(Boss);
		const FWRCreatureDef* Def = Boss->GetDef();
		FString Name = Def ? Def->Name : Boss->GetDisplayLabel(), Title;
		Name.Split(TEXT(", "), &Name, &Title);
		FBanner B;
		B.Kind = FBanner::EKind::Boss;
		B.Over = FText::FromString(Def && Def->Role.Contains(TEXT("major")) ? TEXT("Major boss") : TEXT("Mini-boss"));
		B.Title = FText::FromString(Name.ToUpper());
		B.Sub = FText::FromString(Title);
		B.Portrait = Boss->CreatureId;
		B.Accent = Def && Def->Element == TEXT("frost") ? WRUI::Frost : WRUI::Ember;
		B.Seconds = WRFlowUI::Tune(TEXT("bossCardSeconds"), 3.6f);
		ShowBanner(B, true);
		WRSound::PlayUI(P, TEXT("boss.intro"));
	}
}

void FWRFlow::CheckTames(const FVector& At)
{
	AWRPlayerController* P = GetPC();
	const APlayerState* PS = P->PlayerState;
	if (!PS) return;
	// A creature you saw wild nearby that is now tamed by you: the flourish (knockout taming or a fed passive one).
	for (auto It = WildSeen.CreateIterator(); It; ++It)
	{
		const AWRCreature* C = It->Get();
		if (!C || !C->IsAlive() || FVector::Dist(C->GetActorLocation(), At) > 6000.f) { It.RemoveCurrent(); continue; }
		if (!C->IsTamed()) continue;
		if (C->TamedBy == PS->GetPlayerName())
		{
			const FWRCreatureDef* Def = C->GetDef();
			FBanner B;
			B.Kind = FBanner::EKind::Tame;
			B.Over = LOCTEXT("Tamed", "Tamed");
			B.Title = FText::FromString(Def ? Def->Name : C->GetDisplayLabel());
			B.Sub = FText::Format(LOCTEXT("TamedSub", "{0}% effectiveness. It follows you: hold {1} on it for orders."), FMath::RoundToInt(C->TameEffectiveness * 100.f), WRFlowUI::KeyText(TEXT("Interact")));
			B.Portrait = C->CreatureId;
			B.Accent = WRUI::Good;
			B.Seconds = WRFlowUI::Tune(TEXT("bannerSeconds"), 4.f) + 0.5f;
			ShowBanner(B);
			WRSound::PlayUI(P, TEXT("tame.success"));
			WRFx::SpawnAt(P, TEXT("tame.success"), C->GetActorLocation());
		}
		It.RemoveCurrent();
	}
	const float Range = 3000.f;
	for (TActorIterator<AWRCreature> It(P->GetWorld()); It; ++It)
	{
		if (!It->IsTamed() && It->IsAlive() && !It->IsA<AWRBoss>() && FVector::Dist(It->GetActorLocation(), At) < Range)
		{
			WildSeen.Add(*It);
		}
	}
}

void FWRFlow::CheckHearths(const FVector& At)
{
	AWRPlayerController* P = GetPC();
	for (TActorIterator<AWRHearth> It(P->GetWorld()); It; ++It)
	{
		const int32 Level = It->GetTechLevel();
		int32* Known = HearthLevels.Find(*It);
		if (!Known)
		{
			HearthLevels.Add(*It, Level);
			continue;
		}
		const bool bNear = FVector::Dist2D(It->GetActorLocation(), At) < It->CoverRadius + 1000.f;
		if (Level > *Known && bNear)
		{
			// What the stations in this claim can make now: recipes of the new tier.
			TArray<FString> Names;
			FName FirstIcon;
			for (const TCHAR* Station : { TEXT("workbench"), TEXT("forge"), TEXT("armorer"), TEXT("alchemy_table") })
			{
				for (const FWRRecipe* R : FWRRecipeRegistry::Get().ForStation(Station))
				{
					if (R->Level != Level) continue;
					const FWRItemDef* Item = FWRItemRegistry::Get().Find(R->Output);
					Names.AddUnique(Item ? Item->Name : R->Output.ToString());
					if (FirstIcon.IsNone()) FirstIcon = R->Output;
				}
			}
			FToast T;
			T.Over = LOCTEXT("NewRecipes", "New recipes");
			T.Title = FText::Format(LOCTEXT("TechLevel", "Tech level {0}"), Level);
			if (Names.Num() == 0)
			{
				T.Sub = FText::Format(LOCTEXT("TierSub", "Stations in this claim now work at tier {0}."), Level);
			}
			else
			{
				const int32 Shown = FMath::Min(3, Names.Num());
				FString List = FString::Join(TArray<FString>(Names.GetData(), Shown), TEXT(", "));
				if (Names.Num() > Shown) List += FString::Printf(TEXT(" and %d more"), Names.Num() - Shown);
				T.Sub = FText::FromString(List);
			}
			T.Icon = FirstIcon.IsNone() ? FName(TEXT("core_vulkarn")) : FirstIcon;
			T.Accent = WRUI::Brass;
			T.Seconds = WRFlowUI::Tune(TEXT("toastSeconds"), 6.f);
			ShowToast(T);
			WRSound::PlayUI(P, TEXT("recipe.unlock"));
		}
		*Known = Level;
	}
}

void FWRFlow::CheckHints(const FVector& At)
{
	if (!HintsEnabled() || MemoryFor.IsEmpty() || Hints.Num() > 0) return;
	AWRPlayerController* P = GetPC();
	const FWRPlayerUI* X = P->GetUI();
	if (X && X->IsScreenOpen()) return;
	// In order of what a new player meets first; one at a time, each once.
	if (!WasSeen(TEXT("ability")) && X)
	{
		const TArray<FWRCastable> All = X->GetCastables();
		if (All.Num() > 0)
		{
			if (FWRKeyBindings::Get().GetCastMode() == EWRCastMode::Keys)
			{
				ShowHint(TEXT("ability"), FWRKeyBindings::AbilityAction(All[0].NodeId), FText::Format(LOCTEXT("HintAbilityKey", "Press to cast {0}. Every ability you learn can have its own key."), All[0].Name));
			}
			else
			{
				ShowHint(TEXT("ability"), TEXT("Wheel1"), FText::Format(LOCTEXT("HintAbility", "Hold for your ability wheel; tap to cast {0}."), All[0].Name));
			}
			return;
		}
	}
	if (!WasSeen(TEXT("gather")))
	{
		const float Reach = WRFlowUI::Tune(TEXT("hintGatherRange"), 400.f);
		FVector Ground = FVector::ZeroVector;
		const AWRDressing* Dressing = UWRDressingSubsystem::FindDressing(P->GetWorld());
		bool bNear = Dressing && (Dressing->FindNearest(At, TEXT("tree"), Reach, Ground) || Dressing->FindNearest(At, TEXT("rock"), Reach, Ground));
		for (TActorIterator<AWRResourceNode> It(P->GetWorld()); It && !bNear; ++It)
		{
			bNear = !It->IsDepleted() && FVector::Dist2D(It->GetActorLocation(), At) < Reach;
		}
		if (bNear)
		{
			ShowHint(TEXT("gather"), TEXT("Light"), LOCTEXT("HintGather", "Swing at a tree or rock to gather wood and stone. Hold a hatchet or a pick to gather more."));
			return;
		}
	}
	if (!WasSeen(TEXT("tame")))
	{
		for (TActorIterator<AWRCreature> It(P->GetWorld()); It; ++It)
		{
			if (!It->IsTamed() && It->IsAlive() && It->IsUnconscious() && FVector::Dist(It->GetActorLocation(), At) < WRFlowUI::Tune(TEXT("hintTameRange"), 700.f))
			{
				ShowHint(TEXT("tame"), TEXT("Interact"), FText::Format(LOCTEXT("HintTame", "This {0} is knocked out. Feed it to tame it before it wakes."), FText::FromString(It->GetDef() ? It->GetDef()->Name : It->GetDisplayLabel())));
				return;
			}
		}
	}
	if (!WasSeen(TEXT("body")))
	{
		for (TActorIterator<AWRCreature> It(P->GetWorld()); It; ++It)
		{
			if (It->IsHarvestableBody() && FVector::Dist(It->GetActorLocation(), At) < WRTune(TEXT("gather.body.hintRange"), 700.f))
			{
				ShowHint(TEXT("body"), TEXT("Light"), LOCTEXT("HintBody", "Hit the body to harvest it: meat, hide and the rest. A blade in hand (hatchet or sword) gets more than bare hands."));
				return;
			}
		}
	}
	if (!WasSeen(TEXT("tool")) && X)
	{
		// Carrying the right tool for a tree or rock in reach but not holding it: tools only work in the hand.
		const AWRPlayerState* ToolPS = P->GetPlayerState<AWRPlayerState>();
		const UWRInventoryComponent* ToolInv = ToolPS ? ToolPS->GetInventory() : nullptr;
		const float Reach = WRFlowUI::Tune(TEXT("hintGatherRange"), 400.f);
		const AWRDressing* Dressing = UWRDressingSubsystem::FindDressing(P->GetWorld());
		FVector Ground = FVector::ZeroVector;
		const FName Kind = !Dressing ? NAME_None : Dressing->FindNearest(At, TEXT("tree"), Reach, Ground) ? FName(TEXT("hatchet"))
			: Dressing->FindNearest(At, TEXT("rock"), Reach, Ground) ? FName(TEXT("pick")) : NAME_None;
		const FWRToolDef* InHand = ToolInv ? FWRGathering::FindTool(ToolInv->GetEquipped(EWREquipSlot::Weapon).ItemId) : nullptr;
		for (int32 i = 0; i < 8 && ToolInv && !Kind.IsNone() && !(InHand && InHand->Kind == Kind); ++i)
		{
			const FWRToolDef* Tool = FWRGathering::FindTool(X->GetHotbarItem(i));
			if (Tool && Tool->Kind == Kind && ToolInv->CountOf(Tool->ItemId) > 0)
			{
				ShowHint(TEXT("tool"), FName(*FString::Printf(TEXT("Hotbar%d"), i + 1)), Kind == TEXT("hatchet")
					? LOCTEXT("HintToolHatchet", "Tools work in your hand: take out your hatchet to chop more wood. Your sword's key puts it back.")
					: LOCTEXT("HintToolPick", "Tools work in your hand: take out your pick to break more stone and ore. Your sword's key puts it back."));
				return;
			}
		}
	}
	if (!WasSeen(TEXT("station")) && P->FindNearbyStation())
	{
		ShowHint(TEXT("station"), TEXT("Crafting"), LOCTEXT("HintStation", "Open crafting here to make this station's recipes."));
		return;
	}
	const AWRPlayerState* PS = P->GetPlayerState<AWRPlayerState>();
	const UWRInventoryComponent* Inv = PS ? PS->GetInventory() : nullptr;
	const float Limit = WRTune(TEXT("inventory.carryKg"), 80.f);
	if (!WasSeen(TEXT("carry")) && Inv && Inv->GetCarryWeight() > Limit)
	{
		ShowHint(TEXT("carry"), TEXT("Inventory"), FText::Format(LOCTEXT("HintCarry", "You're carrying more than {0} kg, so you move slower. Drop or store something."), FMath::RoundToInt(Limit)));
		return;
	}
	const AWRGameState* GS = AWRGameState::Get(P);
	if (!WasSeen(TEXT("night")) && GS && GS->IsNight())
	{
		ShowHint(TEXT("night"), NAME_None, LOCTEXT("HintNight", "Night has fallen. Creatures that live by day sleep now, and the night hunters wake."));
		return;
	}
}

// ---------------------------------------------------------------- tests and screenshots

void FWRFlow::Command(const TArray<FString>& Args)
{
	const FString What = Args.Num() > 0 ? Args[0].ToLower() : FString();
	const auto Arg = [&Args](int32 i) { return Args.IsValidIndex(i) ? Args[i] : FString(); };
	AWRPlayerController* P = GetPC();
	if (What == TEXT("preview")) { Preview(TArray<FString>(Args.GetData() + 1, Args.Num() - 1)); }
	else if (What == TEXT("forget"))
	{
		LoadMemory();
		Found.Reset();
		Seen.Reset();
		Hints.Reset();
		SaveMemory();
		UE_LOG(LogWildreach, Log, TEXT("FLOW forgot discoveries and hints for %s"), *MemoryFor);
	}
	else if (What == TEXT("hints")) { WRFlowUI::HintsOverride = Arg(1) != TEXT("off") && Arg(1) != TEXT("0"); }
	else if (What == TEXT("quiet")) { bQuiet = Arg(1) != TEXT("off") && Arg(1) != TEXT("0"); UE_LOG(LogWildreach, Log, TEXT("FLOW moments %s"), bQuiet ? TEXT("off (quiet)") : TEXT("on")); }
	else if (What == TEXT("shotondeath")) { ShotOnDeath = FCString::Atof(*Arg(1)); }
	else if (What == TEXT("shotonback")) { ShotOnBack = FCString::Atof(*Arg(1)); }
	else if (What == TEXT("shot")) { ShotAt = WRUIMotion::Now() + FCString::Atof(*Arg(1)); }
	else if (What == TEXT("near") && P && P->GetPawn())
	{
		// Stand next to the nearest tree / rock of the island (a gathering hint, pickups).
		FVector Ground = FVector::ZeroVector;
		const AWRDressing* Dressing = UWRDressingSubsystem::FindDressing(P->GetWorld());
		if (Dressing && Dressing->FindNearest(P->GetPawn()->GetActorLocation(), Arg(1).IsEmpty() ? TEXT("tree") : Arg(1), 20000.f, Ground))
		{
			P->WRTeleport(Ground.X + 160.f, Ground.Y);
			P->WRLook(180.f, -10.f);
			UE_LOG(LogWildreach, Log, TEXT("FLOW standing by the %s at %s"), Arg(1).IsEmpty() ? TEXT("tree") : *Arg(1), *Ground.ToCompactString());
		}
	}
	else if (What == TEXT("cam") && P)
	{
		// Try a view (picking the menu's backdrop): WRFlow cam X Y Z Yaw Pitch [Fov] | WRFlow cam off.
		if (Arg(1) == TEXT("off"))
		{
			if (ACameraActor* Cam = TestCam.Get()) Cam->Destroy();
			if (P->GetPawn()) P->SetViewTarget(P->GetPawn());
			return;
		}
		const FVector Loc(FCString::Atof(*Arg(1)), FCString::Atof(*Arg(2)), FCString::Atof(*Arg(3)));
		const FRotator Rot(FCString::Atof(*Arg(5)), FCString::Atof(*Arg(4)), 0.f);
		ACameraActor* Cam = TestCam.Get();
		if (!Cam)
		{
			Cam = P->GetWorld()->SpawnActor<ACameraActor>(Loc, Rot);
			TestCam = Cam;
		}
		if (Cam)
		{
			Cam->SetActorLocationAndRotation(Loc, Rot);
			Cam->GetCameraComponent()->SetConstraintAspectRatio(false);
			Cam->GetCameraComponent()->SetFieldOfView(Arg(6).IsEmpty() ? 75.f : FCString::Atof(*Arg(6)));
			P->SetViewTarget(Cam);
			// World Partition follows the view: the body stays put rather than fall through ground that unloads.
			if (ACharacter* Body = Cast<ACharacter>(P->GetPawn()); Body && Body->HasAuthority()) Body->GetCharacterMovement()->DisableMovement();
		}
	}
	else
	{
		UE_LOG(LogWildreach, Log, TEXT("FLOW commands: preview death|loading|island|levelup|point|recipe|discover <place>|boss <id>|tame <id>|hint <id> [seconds], forget, hints on|off, shotondeath <s>, shotonback <s>, shot <s>, near tree|rock, cam X Y Z Yaw Pitch [Fov] | off"));
	}
}

void FWRFlow::Preview(const TArray<FString>& Args)
{
	AWRPlayerController* P = GetPC();
	const FString What = Args.Num() > 0 ? Args[0].ToLower() : FString();
	const FString Arg1 = Args.Num() > 1 ? Args[1] : FString();
	const double Now = WRUIMotion::Now();
	const FVector At = P && P->GetPawn() ? P->GetPawn()->GetActorLocation() : FVector::ZeroVector;
	if (What == TEXT("death"))
	{
		// The real screen with a made-up note: killed by a Cinderfang, a souls bag 40 m off, back at the nearest camp.
		FWRDeathNote N;
		N.Killer = TEXT("Cinderfang");
		N.KillerCreature = TEXT("cinderfang");
		N.DeathAt = At;
		N.Penalty = Arg1.IsEmpty() ? TEXT("souls") : Arg1;
		N.bBag = N.Penalty != TEXT("gentle");
		N.BagAt = At;
		N.BagStacks = 7;
		N.BagMinutes = WRTune(TEXT("death.bagMinutes"), 30.f);
		const FWRPoi* Camp = nullptr;
		for (const FWRPoi& Poi : FWRPoi::All) if (Poi.Kind == TEXT("camp") && (!Camp || FVector::Dist2D(Poi.Location, At) < FVector::Dist2D(Camp->Location, At))) Camp = &Poi;
		N.RespawnName = Camp ? Camp->Name : TEXT("Volcanic camp 3");
		N.RespawnAt = Camp ? Camp->Location : At + FVector(80000.f, 0.f, 0.f);
		N.Seconds = 30.f;
		OnDeathNote(N);
		bPreviewHold = true;
		PreviewUntil = Now + 30.0;
		return;
	}
	if (What == TEXT("loading") || What == TEXT("island"))
	{
		Cover = ECover::None;
		StartLoading(What == TEXT("loading"), 0.15f);
		bStreaming = true;
		StreamStart = Now;
		NextStreamCheck = Now + 3600.0;   // held by the preview, not the island
		bPreviewHold = true;
		PreviewUntil = Now + (Arg1.IsEmpty() ? 6.0 : FCString::Atof(*Arg1));
		return;
	}
	if (What == TEXT("levelup"))
	{
		const UWRPathComponent* Paths = P && P->GetPawn() ? UWRPathComponent::FindFor(P->GetPawn()) : nullptr;
		const int32 Level = Paths ? Paths->GetCharacterLevel() : 4;
		LastLevel = Level - 1;
		LastUnspent = -1;
		CheckLevel(Paths);
		return;
	}
	if (What == TEXT("point"))
	{
		const UWRPathComponent* Paths = P && P->GetPawn() ? UWRPathComponent::FindFor(P->GetPawn()) : nullptr;
		if (Paths)
		{
			LastUnspent = Paths->GetEarned(Paths->GetMainPath()) - Paths->GetSpent(Paths->GetMainPath()) - 1;
			CheckLevel(Paths);
		}
		return;
	}
	if (What == TEXT("discover"))
	{
		const FString Name = Args.Num() > 1 ? FString::Join(TArray<FString>(Args.GetData() + 1, Args.Num() - 1), TEXT(" ")) : TEXT("North pass");
		for (const FWRPoi& Poi : FWRPoi::All)
		{
			if (Poi.Name.Equals(Name, ESearchCase::IgnoreCase))
			{
				LoadMemory();
				Found.Remove(Poi.Name);
				CheckDiscoveries(Poi.Location);
				return;
			}
		}
		return;
	}
	if (What == TEXT("boss"))
	{
		const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(FName(*(Arg1.IsEmpty() ? TEXT("vulkarn") : Arg1)));
		FString Name = Def ? Def->Name : TEXT("Vulkarn, the Forge Giant"), Title;
		Name.Split(TEXT(", "), &Name, &Title);
		FBanner B;
		B.Kind = FBanner::EKind::Boss;
		B.Over = FText::FromString(Def && !Def->Role.Contains(TEXT("major")) ? TEXT("Mini-boss") : TEXT("Major boss"));
		B.Title = FText::FromString(Name.ToUpper());
		B.Sub = FText::FromString(Title);
		B.Portrait = Def ? Def->Id : FName(TEXT("vulkarn"));
		B.Accent = Def && Def->Element == TEXT("frost") ? WRUI::Frost : WRUI::Ember;
		B.Seconds = 6.f;
		ShowBanner(B, true);
		return;
	}
	if (What == TEXT("tame"))
	{
		const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(FName(*(Arg1.IsEmpty() ? TEXT("ashstrider") : Arg1)));
		FBanner B;
		B.Kind = FBanner::EKind::Tame;
		B.Over = LOCTEXT("Tamed", "Tamed");
		B.Title = FText::FromString(Def ? Def->Name : TEXT("Ashstrider"));
		B.Sub = FText::Format(LOCTEXT("TamedSub", "{0}% effectiveness. It follows you: hold {1} on it for orders."), 92, WRFlowUI::KeyText(TEXT("Interact")));
		B.Portrait = Def ? Def->Id : FName(TEXT("ashstrider"));
		B.Accent = WRUI::Good;
		B.Seconds = 6.f;
		ShowBanner(B);
		return;
	}
	if (What == TEXT("recipe"))
	{
		for (TActorIterator<AWRHearth> It(P->GetWorld()); It; ++It)
		{
			HearthLevels.Add(*It, It->GetTechLevel() - 1);
		}
		CheckHearths(At);
		return;
	}
	if (What == TEXT("hint"))
	{
		const FName Id(*(Arg1.IsEmpty() ? TEXT("gather") : Arg1));
		Seen.Remove(Id);
		Hints.Reset();
		if (Id == TEXT("gather")) ShowHint(Id, TEXT("Light"), LOCTEXT("HintGather", "Swing at a tree or rock to gather wood and stone. Hold a hatchet or a pick to gather more."));
		else if (Id == TEXT("body")) ShowHint(Id, TEXT("Light"), LOCTEXT("HintBody", "Hit the body to harvest it: meat, hide and the rest. A blade in hand (hatchet or sword) gets more than bare hands."));
		else if (Id == TEXT("tool")) ShowHint(Id, TEXT("Hotbar2"), LOCTEXT("HintToolHatchet", "Tools work in your hand: take out your hatchet to chop more wood. Your sword's key puts it back."));
		else if (Id == TEXT("night")) ShowHint(Id, NAME_None, LOCTEXT("HintNight", "Night has fallen. Creatures that live by day sleep now, and the night hunters wake."));
		else if (Id == TEXT("station")) ShowHint(Id, TEXT("Crafting"), LOCTEXT("HintStation", "Open crafting here to make this station's recipes."));
		else ShowHint(Id, TEXT("Wheel1"), FText::Format(LOCTEXT("HintAbility", "Hold for your ability wheel; tap to cast {0}."), LOCTEXT("FireBolt", "Fire bolt")));
		return;
	}
}

// ---------------------------------------------------------------- the controller's half of the death note

void AWRPlayerController::ClientDeathNote_Implementation(const FWRDeathNote& Note)
{
	UE_LOG(LogWildreach, Log, TEXT("FLOW you died: '%s', back at '%s' in %.0f s"), *Note.Killer, *Note.RespawnName, Note.Seconds);
	if (FWRPlayerUI* X = GetUI())
	{
		if (FWRFlow* F = X->GetFlow())
		{
			F->OnDeathNote(Note);
		}
	}
}

// ---------------------------------------------------------------- the loading screen during map loads

namespace WRFlowLoading
{
	/** Keep the movie player armed with our layout: it shows it at the next map load (host, join, travel, menu). */
	bool Arm(float)
	{
		IGameMoviePlayer* Movie = GetMoviePlayer();
		if (!Movie || !Movie->IsInitialized() || Movie->IsMovieCurrentlyPlaying() || Movie->LoadingScreenIsPrepared())
		{
			return true;
		}
		FLoadingScreenAttributes Screen;
		Screen.bAutoCompleteWhenLoadingCompletes = true;
		Screen.bWaitForManualStop = false;
		Screen.bMoviesAreSkippable = false;
		Screen.MinimumLoadingScreenDisplayTime = WRTune(TEXT("flow.loadingMinSeconds"), 0.5f);
		Screen.WidgetLoadingScreen = WRUIFlow::MakeLoadingScreen(LOCTEXT("Loading", "Loading the island…"), true);
		Movie->SetupLoadingScreen(Screen);
		return true;
	}

	FDelayedAutoRegisterHelper GRegister(EDelayedRegisterRunPhase::EndOfEngineInit, []()
	{
		if (GIsEditor || IsRunningDedicatedServer() || IsRunningCommandlet() || !IsMoviePlayerEnabled())
		{
			return;
		}
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Arm), 0.25f);
		FCoreUObjectDelegates::PreLoadMap.AddLambda([](const FString& Map)
		{
			const IGameMoviePlayer* Movie = GetMoviePlayer();
			UE_LOG(LogWildreach, Log, TEXT("FLOW loading %s (loading screen %s)"), *Map, Movie && Movie->LoadingScreenIsPrepared() ? TEXT("ready") : TEXT("not ready"));
		});
		FCoreUObjectDelegates::PostLoadMapWithWorld.AddLambda([](UWorld* World)
		{
			UE_LOG(LogWildreach, Log, TEXT("FLOW loaded %s"), *GetNameSafe(World));
		});
	});
}

// ---------------------------------------------------------------- console (tests, screenshots)

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorldAndArgs GWRFlowCommand(TEXT("WRFlow"),
	TEXT("Testing: the death and loading covers, moments and hints. WRFlow preview death|loading|island|levelup|point|recipe|discover <place>|boss <id>|tame <id>|hint <id>, forget, hints on|off, shotondeath <s>, shotonback <s>, shot <s>, near tree|rock, cam X Y Z Yaw Pitch [Fov]|off"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		AWRPlayerController* PC = World ? Cast<AWRPlayerController>(World->GetFirstPlayerController()) : nullptr;
		FWRPlayerUI* X = PC ? PC->GetUI() : nullptr;
		if (FWRFlow* F = X ? X->GetFlow() : nullptr)
		{
			F->Command(Args);
		}
	}));
#endif

#undef LOCTEXT_NAMESPACE
