#pragma once

#include "CoreMinimal.h"

class SWidget;

namespace WRUI
{
	/**
	 * Choose a world (the main menu's Host a game): the worlds on this PC (WRWorlds), last played first, with New world
	 * (a name and a preset), Play, Rename and Delete (which asks first). Selected: the world picked to begin with (else
	 * the last played). OnPlay gets the id of the world to host; OnClose runs on Back and Esc.
	 */
	WILDREACH_API TSharedRef<SWidget> MakeWorldChooser(const FString& Selected, TFunction<void(const FString&)> OnPlay, FSimpleDelegate OnClose);
}
