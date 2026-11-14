#pragma once
// The endgame's screens (WRUIEndgame.cpp; cooks/build-endgame.md 2.11): the HUD's achievement toast is made here and
// placed by the HUD (WRUIHud.cpp); the rest register themselves as screens and a journal tab.
#include "CoreMinimal.h"

class SWidget;
class AWRPlayerController;

namespace WRUIEndgame
{
	/** The achievement toast: top-centre, one at a time from the controller's queue, 3 s each (deeds.toastSeconds). */
	TSharedRef<SWidget> MakeToast(TWeakObjectPtr<AWRPlayerController> PC);
}
