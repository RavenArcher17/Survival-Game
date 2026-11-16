#pragma once
// Sound captions on the HUD (cookpass-audio 4.9; TLOU2's captions with direction, Fortnite's sound visualisation): the
// lines WRSoundCaptions collects, bottom centre over the pickups, each with an arrow toward where it came from.
#include "CoreMinimal.h"

class SWidget;
class AWRPlayerController;

namespace WRUICaptions
{
	/** The caption stack (placed by the HUD, WRUIHud.cpp). Collapsed while Settings > Audio > Sound captions is Off. */
	TSharedRef<SWidget> Make(TWeakObjectPtr<AWRPlayerController> PC);
}
