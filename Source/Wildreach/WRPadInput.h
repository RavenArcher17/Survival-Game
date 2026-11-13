#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

class APawn;
class APlayerController;
class SWidget;
class UWorld;

/**
 * Controller support, the device in use, and held-input isolation.
 *
 * The pad's buttons come from Data/gamepad.json: one verb per button in play (COMBAT-AND-LESSONS L4: RB light, RT heavy,
 * LB block, LT sprint, A jump, B roll, X use, Y the belt item, D-pad quick slots and belt, R3 lock-on, L3 view, Menu the
 * journal, View the bug report) and one per button in menus (A click, X the second click, B back, LB / RB the journal's
 * pages, Y the controls page, Menu close). Gameplay buttons go through the character's input mapping and the controller's
 * key bindings like the keyboard's; menus are driven by one Slate input processor (FWRPadProcessor in the .cpp): while the
 * mouse cursor is up, the left stick moves it (slower over something clickable), the D-pad jumps it to the nearest
 * clickable thing that way, A and X click and drag, the right stick scrolls, so every screen works by pad without each
 * one knowing about it. It also catches F8 / F1 anywhere (the main menu included) and notes the device last used, so
 * prompts show that device's buttons only.
 */
enum class EWRDevice : uint8 { KeyboardMouse, Gamepad };

/** Data/gamepad.json. */
class WILDREACH_API FWRPadLayout
{
public:
	static const FWRPadLayout& Get();
	/** The pad button of a verb ("Light", "QuickFire", "MenuBack"...); invalid when none. */
	FKey Key(FName Verb) const;
	/** The verb a rebindable action (FWRKeyBindings id) has on the pad, for prompts; None when the pad has none. */
	FName PromptVerb(FName Action) const;
	/** A button's name in the chosen set (Settings: Xbox or PlayStation): "RB" / "R1". */
	FText Label(const FKey& Key) const;

	/** The F1 page: groups of rows, each a verb (its button) or a fixed button text, and what it does. */
	struct FRow { FName Verb; FString Button; FText Does; };
	struct FGroup { FText Title; TArray<FRow> Rows; };
	TArray<FGroup> Groups;

private:
	FWRPadLayout();
	TMap<FName, FKey> Verbs;
	TMap<FName, FName> Prompts;
	TMap<FString, FString> Names[2];
};

namespace WRInput
{
	/** The device the player last pressed something on (keys, mouse, pad buttons or sticks). */
	WILDREACH_API EWRDevice Device();
	WILDREACH_API bool UsingPad();
	WILDREACH_API void NoteDevice(EWRDevice NewDevice);

	/** What to show for a rebindable action on the device in use: its key, or its pad button (empty when the pad has
	 *  none, so no keyboard key shows while playing on a pad). */
	WILDREACH_API FText ActionKey(FName Action);
	/** A pad verb's button name ("RB"), empty when unmapped. */
	WILDREACH_API FText PadButton(FName Verb);

	/** Look input with the player's sensitivity and invert (Settings > Look & controller). Mouse: the raw delta. Pad: the
	 *  right stick (dead zone, curve and turn rates from tuning "look"), for this frame's Dt. */
	WILDREACH_API void MouseLook(APawn* Pawn, FVector2D Delta);
	WILDREACH_API void PadLook(APawn* Pawn, FVector2D Stick, float Dt);

	/** A short rumble on the local player's pad (Settings can turn it off); Strength 0..1. */
	WILDREACH_API void Rumble(UWorld* World, float Strength, float Seconds);

	/**
	 * Held-input isolation (COMBAT-AND-LESSONS L8 / 1.16). Called when the player moves between play and a mode that
	 * takes the buttons (a menu or wheel, build placing, a mount, the death screen, the report or controls page): held
	 * gameplay inputs are let go (block, charge, sprint, jump) and, for tuning look.isolationSeconds (0.14 s), no new
	 * gameplay press is taken, so the press that closed a menu never also swings.
	 */
	WILDREACH_API void Transition(APlayerController* PC, const TCHAR* Why);
	/** May a gameplay button press act now? False inside a menu / wheel / overlay and just after leaving one. */
	WILDREACH_API bool AllowPress(const APlayerController* PC);

	/** Install the input processor once (game clients; idempotent). */
	WILDREACH_API void Install();

	/** A screen the pad's D-pad navigates (the topmost by Priority wins); removed when it closes. With none, the whole
	 *  game window is searched. */
	WILDREACH_API void AddNavRoot(const TSharedRef<SWidget>& Root, int32 Priority);
	WILDREACH_API void RemoveNavRoot(const TSharedPtr<SWidget>& Root);

	/** A page over everything (the report, the controls page) takes the keys and the cursor (UI only, Focus focused);
	 *  EndOverlay gives them back to what was under it (the main menu, a screen, play). Counted, so pages can stack. */
	WILDREACH_API void BeginOverlay(const TSharedPtr<SWidget>& Focus);
	WILDREACH_API void EndOverlay();

	/** An open page that wants some menu buttons itself (verbs: MenuBack, MenuClose, MenuControls, TabPrev, TabNext,
	 *  DLeft, DRight, DUp, DDown): the newest handler is asked first; true = it used the button. */
	WILDREACH_API void PushPadHandler(const void* Owner, TFunction<bool(FName)> Handler);
	WILDREACH_API void PopPadHandler(const void* Owner);

	/** The settings window is listening for a new key: F1 / F8 go to it, not to their pages. */
	WILDREACH_API void SetKeyCapture(bool bCapturing);

	/** Which ability wheel the pad's quick slot is on (0, 1): its prompt shows the D-pad. */
	WILDREACH_API int32 PadQuickWheel();
	WILDREACH_API void SetPadQuickWheel(int32 Wheel);
}

/** The controls page (F1, Y in menus on a pad): every action and its key or button for the device in use (WRUIControls.cpp). */
namespace WRControlsPage
{
	WILDREACH_API void Toggle();
	WILDREACH_API void Close();
	WILDREACH_API bool IsOpen();
}
