#pragma once

#include "CoreMinimal.h"

class SWidget;
struct FWRDeathNote;

/** The island map outside the journal's Map tab (WRUIMap.cpp). */
namespace WRUIMap
{
	/** What the death screen's map reads: the note's places to wake, and what a click on one does. */
	struct FWakeMapArgs
	{
		TFunction<const FWRDeathNote*()> Note;
		/** Changes with each death: the map frames the new places. */
		TFunction<int32()> Version;
		/** A bed on the list still resting (dimmed, can't be picked). */
		TFunction<bool(int32)> Resting;
		TFunction<void(int32)> Pick;
	};

	/**
	 * The map on the death screen: only the places you can wake (your beds, your clan's, your path's camps) and where
	 * you fell, framed around them each death, with the compass and zoom buttons. A click on a place picks it.
	 */
	WILDREACH_API TSharedRef<SWidget> MakeWakeMap(const FWakeMapArgs& Args);

	/** Tests: where a place named on the map drawn last sits on the screen (false: no map is up, or no such place). */
	WILDREACH_API bool FindMarkerOnScreen(const FString& Name, FVector2D& OutScreen);
}
