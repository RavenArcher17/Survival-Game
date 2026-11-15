#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

class SWidget;

/**
 * The pad keyboard (cookpass-ui 3.4): typing on a controller. A stone panel docked to the bottom third with a preview
 * line and a QWERTY grid (digits, letters, a little punctuation, Shift / Space / Back / Done). Pad: the D-pad (or the
 * cursor over a key) picks a key, A types it, X deletes, Y is a space, LT is Shift (twice quickly: caps lock), RT or
 * Menu is Done, B cancels. Mouse: click the keys. Keyboard: type, Enter is Done, Esc cancels.
 *
 * WRPadInput opens it on any text box clicked with A on a pad (chat, the report note, the menu's name and address,
 * world and tame names, door codes...), so no screen needs code for it. Done writes the text back and, for a one-line
 * box, presses Enter on it (so the chat sends, a name is taken); Cancel leaves the box as it was.
 */
namespace WRPadKeyboard
{
	struct FTarget
	{
		FText Initial;
		FText Hint;
		int32 MaxLength = 200;
		bool bMultiLine = false;
		/** Dock the keyboard at the top of the screen (the box is low down: the chat line). */
		bool bDockTop = false;
		/** Done: the new text. */
		TFunction<void(const FText&)> OnDone;
		/** Cancel (B / Esc / closed under it). */
		TFunction<void()> OnCancel;
	};

	WILDREACH_API void Open(FTarget Target);
	/** Open it on an editable text widget (SEditableText / SEditableTextBox / their multi-line kinds) found under the
	 *  pad's cursor; false when the widget isn't one. */
	WILDREACH_API bool OpenOn(const TSharedRef<SWidget>& Editable);
	WILDREACH_API bool IsOpen();
	WILDREACH_API void Close(bool bCommit);

	/** The pad processor hands it every pad button while it's open (true: used). */
	WILDREACH_API bool PadButton(const FKey& Key);
	/** The D-pad (and its repeats): move the key picked. */
	WILDREACH_API void Move(const FKey& DPadKey);

	/** True for a widget the keyboard can type into (its Slate type). */
	WILDREACH_API bool IsEditable(const SWidget& W);
}
