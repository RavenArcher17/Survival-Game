// Driving the UI the way a player does, for tests. WRClick presses the mouse on a piece of on-screen text (a button's
// label, a tab, a recipe) and WRKey presses a key, both handed to Slate as the real mouse and keyboard are, so they go
// through hit-testing, mouse capture and keyboard focus. WRMenu, WRSettings and WRCraft call the menus' code directly
// and skip all of that, which is how menus that wouldn't take clicks went unnoticed. Tools\menutest.ps1 uses these.

#include "Wildreach.h"
#include "WRPlayerController.h"
#include "WRUIFlow.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/IConsoleManager.h"
#include "Layout/WidgetPath.h"
#include "Widgets/SWindow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

namespace WRUIDrive
{
	struct FFound
	{
		TSharedPtr<SWidget> Widget;
		FString Text;
		FVector2D Centre = FVector2D::ZeroVector;
	};

	TSharedPtr<SWindow> GameWindow()
	{
		return GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWindow() : nullptr;
	}

	void CollectText(const TSharedRef<SWidget>& W, TArray<TSharedRef<SWidget>>& Out)
	{
		const EVisibility V = W->GetVisibility();
		if (V == EVisibility::Collapsed || V == EVisibility::Hidden) return;
		if (W->GetType() == TEXT("STextBlock")) Out.Add(W);
		FChildren* Children = W->GetChildren();
		for (int32 i = 0; Children && i < Children->Num(); ++i) CollectText(Children->GetChildAt(i), Out);
	}

	/** Every piece of text drawn in the game window, top to bottom then left to right, with its centre on screen. */
	TArray<FFound> VisibleText()
	{
		TArray<FFound> Out;
		const TSharedPtr<SWindow> Window = GameWindow();
		if (!Window.IsValid() || !FSlateApplication::IsInitialized()) return Out;
		TArray<TSharedRef<SWidget>> Texts;
		CollectText(Window.ToSharedRef(), Texts);
		const FSlateRect Client = Window->GetClientRectInScreen();
		for (const TSharedRef<SWidget>& W : Texts)
		{
			FWidgetPath Path;
			if (!FSlateApplication::Get().FindPathToWidget(W, Path) || !Path.IsValid()) continue;
			const FString Text = StaticCastSharedRef<STextBlock>(W)->GetText().ToString().TrimStartAndEnd();
			if (Text.IsEmpty()) continue;
			const FVector2D Centre(Path.Widgets.Last().Geometry.GetAbsolutePositionAtCoordinates(FVector2D(0.5, 0.5)));
			if (!Client.ContainsPoint(Centre)) continue;
			Out.Add({ W, Text, Centre });
		}
		Out.Sort([](const FFound& A, const FFound& B) { return FMath::Abs(A.Centre.Y - B.Centre.Y) > 4.0 ? A.Centre.Y < B.Centre.Y : A.Centre.X < B.Centre.X; });
		return Out;
	}

	/** Text equal to Label (or, failing that, containing it). "On/Off" (a slash and no spaces) matches either. */
	TArray<FFound> Matching(const TArray<FFound>& All, const FString& Label)
	{
		TArray<FString> Alts;
		if (!Label.Contains(TEXT(" ")) && Label.Contains(TEXT("/"))) Label.ParseIntoArray(Alts, TEXT("/"));
		else Alts.Add(Label);
		const auto Is = [&Alts](const FFound& F, bool bExact)
		{
			return Alts.ContainsByPredicate([&F, bExact](const FString& A) { return bExact ? F.Text.Equals(A, ESearchCase::IgnoreCase) : F.Text.Contains(A, ESearchCase::IgnoreCase); });
		};
		TArray<FFound> Out = All.FilterByPredicate([&Is](const FFound& F) { return Is(F, true); });
		if (Out.Num() == 0) Out = All.FilterByPredicate([&Is](const FFound& F) { return Is(F, false); });
		return Out;
	}

	/** True when a click at Pos would reach Widget's own control (it, or one of the few widgets holding it), not
	 *  something drawn on top. OutUnder: the innermost widgets the click lands on. */
	bool Reaches(const FVector2D& Pos, const TSharedPtr<SWidget>& Widget, FString& OutUnder)
	{
		const TSharedPtr<SWindow> Window = GameWindow();
		if (!Window.IsValid()) return false;
		const FWidgetPath Hit = FSlateApplication::Get().LocateWindowUnderMouse(Pos, { Window.ToSharedRef() });
		TArray<FString> Tail;
		for (int32 i = Hit.Widgets.Num() - 1; i >= 0 && Tail.Num() < 5; --i) Tail.Add(Hit.Widgets[i].Widget->GetTypeAsString());
		OutUnder = Tail.Num() > 0 ? FString::Join(Tail, TEXT("<")) : TEXT("nothing");
		int32 Depth = 0;
		for (TSharedPtr<SWidget> W = Widget; W.IsValid() && Depth < 6; W = W->GetParentWidget(), ++Depth)
		{
			if (Hit.ContainsWidget(W.Get())) return true;
		}
		return false;
	}

	/** The button holding Widget, if it's in one. */
	TSharedPtr<SButton> ButtonOf(const TSharedPtr<SWidget>& Widget)
	{
		for (TSharedPtr<SWidget> W = Widget; W.IsValid(); W = W->GetParentWidget())
		{
			if (W->GetType() == TEXT("SButton")) return StaticCastSharedPtr<SButton>(W);
		}
		return nullptr;
	}

	/** A mouse move to Pos, a press and a release. OutButton: how the button under it took it (hovered / pressed). */
	void Click(const FVector2D& Pos, const FKey& Button, const TSharedPtr<SButton>& Target, FString& OutButton)
	{
		FSlateApplication& App = FSlateApplication::Get();
		const uint32 User = FSlateApplication::CursorUserIndex, Pointer = FSlateApplication::CursorPointerIndex;
		App.ProcessMouseMoveEvent(FPointerEvent(User, Pointer, Pos, App.GetCursorPos(), TSet<FKey>(), EKeys::Invalid, 0.f, FModifierKeysState()));
		const bool bHovered = Target.IsValid() && Target->IsHovered();
		// Slate only lets a widget capture the mouse (which a button needs between press and release) while the game is
		// the active app. A test's window behind another (the host's, under the friend's) isn't, so every click there
		// fell through; a player's real click makes the game active first. And no native window: that would have
		// Windows capture the mouse for a window that may not be in front.
		const bool bWasAllowed = App.GetHandleDeviceInputWhenApplicationNotActive();
		App.SetHandleDeviceInputWhenApplicationNotActive(true);
		App.ProcessMouseButtonDownEvent(nullptr, FPointerEvent(User, Pointer, Pos, Pos, TSet<FKey>({ Button }), Button, 0.f, FModifierKeysState()));
		const bool bPressed = Target.IsValid() && Target->IsPressed();
		const bool bHeld = Target.IsValid() && Target->HasMouseCapture();
		App.ProcessMouseButtonUpEvent(FPointerEvent(User, Pointer, Pos, Pos, TSet<FKey>(), Button, 0.f, FModifierKeysState()));
		App.SetHandleDeviceInputWhenApplicationNotActive(bWasAllowed);
		OutButton = Target.IsValid() ? FString::Printf(TEXT("button hovered=%d pressed=%d captured=%d enabled=%d"), bHovered, bPressed, bHeld, Target->IsEnabled()) : TEXT("no button");
	}

	void Press(const FKey& Key)
	{
		FSlateApplication& App = FSlateApplication::Get();
		const uint32* KeyCodePtr = nullptr;
		const uint32* CharCodePtr = nullptr;
		FInputKeyManager::Get().GetCodesFromKey(Key, KeyCodePtr, CharCodePtr);
		const uint32 KeyCode = KeyCodePtr ? *KeyCodePtr : 0, CharCode = CharCodePtr ? *CharCodePtr : 0;
		const uint32 User = FSlateApplication::CursorUserIndex;
		App.ProcessKeyDownEvent(FKeyEvent(Key, FModifierKeysState(), User, false, CharCode, KeyCode));
		if (CharCode >= 32) App.ProcessKeyCharEvent(FCharacterEvent(TCHAR(CharCode), FModifierKeysState(), User, false));
		App.ProcessKeyUpEvent(FKeyEvent(Key, FModifierKeysState(), User, false, CharCode, KeyCode));
	}

	const TCHAR* MenuName(EWRMenu M)
	{
		static const TCHAR* Names[] = { TEXT("none"), TEXT("crafting"), TEXT("inventory"), TEXT("build"), TEXT("chest"), TEXT("claim"), TEXT("tame"),
			TEXT("taming"), TEXT("skills"), TEXT("paths"), TEXT("map"), TEXT("hearth"), TEXT("settings") };
		const int32 i = static_cast<int32>(M);
		return i >= 0 && i < UE_ARRAY_COUNT(Names) ? Names[i] : TEXT("?");
	}

	// WRClick <text> [#n] [@ <text on the same row>]: click the nth match (from 1, top to bottom), or the match nearest
	// the row of another label ("WRClick Off @ Vertical sync"). Exact text wins over text that only contains it.
	FAutoConsoleCommandWithWorldAndArgs ClickCommand(TEXT("WRClick"),
		TEXT("Testing: click on-screen text as the mouse would. WRClick <text> [#n] [@ <text on the same row>] [rmb]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*)
		{
			FString Line = FString::Join(Args, TEXT(" "));
			FKey Button = EKeys::LeftMouseButton;
			if (Line.EndsWith(TEXT(" rmb"))) { Button = EKeys::RightMouseButton; Line.LeftChopInline(4); }
			FString Label = Line, Anchor;
			Line.Split(TEXT("@"), &Label, &Anchor);
			int32 Nth = 1;
			FString Base, Num;
			if (Label.Split(TEXT("#"), &Base, &Num, ESearchCase::IgnoreCase, ESearchDir::FromEnd) && Num.TrimStartAndEnd().IsNumeric())
			{
				Label = Base;
				Nth = FMath::Max(1, FCString::Atoi(*Num));
			}
			Label.TrimStartAndEndInline();
			Anchor.TrimStartAndEndInline();
			const TArray<FFound> All = VisibleText();
			TArray<FFound> Hits = Matching(All, Label);
			if (!Anchor.IsEmpty())
			{
				const TArray<FFound> Anchors = Matching(All, Anchor);
				if (Anchors.Num() == 0) Hits.Reset();
				else
				{
					const FVector2D At = Anchors[0].Centre;
					Hits.Sort([At](const FFound& A, const FFound& B) { return FMath::Abs(A.Centre.Y - At.Y) + 0.01 * FMath::Abs(A.Centre.X - At.X) < FMath::Abs(B.Centre.Y - At.Y) + 0.01 * FMath::Abs(B.Centre.X - At.X); });
				}
			}
			if (Hits.Num() < Nth)
			{
				TArray<FString> Seen;
				for (const FFound& F : All) Seen.AddUnique(F.Text.Left(30));
				UE_LOG(LogWildreach, Warning, TEXT("CLICK \"%s\" not on screen (%d matches); on screen: %s"), *Line, Hits.Num(), *FString::Join(Seen, TEXT(" | ")).Left(1500));
				return;
			}
			const FFound& F = Hits[Nth - 1];
			FString Under, Took;
			const bool bReaches = Reaches(F.Centre, F.Widget, Under);
			Click(F.Centre, Button, ButtonOf(F.Widget), Took);
			UE_LOG(LogWildreach, Log, TEXT("CLICK \"%s\" at %.0f,%.0f reaches=%d (%s; under %s)"), *F.Text, F.Centre.X, F.Centre.Y, bReaches ? 1 : 0, *Took, *Under);
		}));

	FAutoConsoleCommandWithWorldAndArgs KeyCommand(TEXT("WRKey"),
		TEXT("Testing: press and release a key as the keyboard would (WRKey I, WRKey Escape, WRKey Enter)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*)
		{
			const FKey Key(Args.Num() > 0 ? FName(*Args[0]) : NAME_None);
			if (!Key.IsValid())
			{
				UE_LOG(LogWildreach, Warning, TEXT("KEY %s is not a key"), Args.Num() > 0 ? *Args[0] : TEXT("(none)"));
				return;
			}
			Press(Key);
			UE_LOG(LogWildreach, Log, TEXT("KEY %s"), *Key.ToString());
		}));

	// WRType <text>: type characters into whatever has keyboard focus (a text box after WRClick on it).
	FAutoConsoleCommandWithWorldAndArgs TypeCommand(TEXT("WRType"),
		TEXT("Testing: type text into the focused text box as the keyboard would"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*)
		{
			const FString Text = FString::Join(Args, TEXT(" "));
			for (const TCHAR C : Text) FSlateApplication::Get().ProcessKeyCharEvent(FCharacterEvent(C, FModifierKeysState(), FSlateApplication::CursorUserIndex, false));
			UE_LOG(LogWildreach, Log, TEXT("TYPE \"%s\""), *Text);
		}));

	FAutoConsoleCommandWithWorld TextCommand(TEXT("WRUIText"),
		TEXT("Testing: log every piece of text on screen with its position"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld*)
		{
			for (const FFound& F : VisibleText()) UE_LOG(LogWildreach, Log, TEXT("UITEXT %4.0f,%4.0f %s"), F.Centre.X, F.Centre.Y, *F.Text.Left(60));
		}));

	// WRUIState: what a player would notice. The open menu, the cursor, whether game input and the mouse are free, where
	// keyboard focus sits, and the settings the menu test changes.
	FAutoConsoleCommandWithWorld StateCommand(TEXT("WRUIState"),
		TEXT("Testing: log the open menu, cursor, input mode, keyboard focus and a few settings"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			const AWRPlayerController* PC = World ? Cast<AWRPlayerController>(World->GetFirstPlayerController()) : nullptr;
			UGameViewportClient* VC = GEngine ? GEngine->GameViewport : nullptr;
			FSlateApplication& App = FSlateApplication::Get();
			const TSharedPtr<SWidget> Focus = App.GetKeyboardFocusedWidget();
			const UGameUserSettings* S = GEngine ? GEngine->GetGameUserSettings() : nullptr;
			UE_LOG(LogWildreach, Log, TEXT("UISTATE menu=%s cursor=%d ignoreinput=%d capturemode=%d mousecaptor=%d focus=%s ignoremove=%d ignorelook=%d hints=%d vsync=%d"),
				PC ? MenuName(PC->GetMenu()) : TEXT("?"), PC ? PC->bShowMouseCursor : -1, VC ? VC->IgnoreInput() : -1,
				VC ? static_cast<int32>(VC->GetMouseCaptureMode()) : -1, App.HasAnyMouseCaptor() ? 1 : 0,
				Focus.IsValid() ? *Focus->GetTypeAsString() : TEXT("none"), PC ? PC->IsMoveInputIgnored() : -1, PC ? PC->IsLookInputIgnored() : -1,
				FWRFlow::HintsEnabled() ? 1 : 0, S && S->IsVSyncEnabled() ? 1 : 0);
		}));
}
