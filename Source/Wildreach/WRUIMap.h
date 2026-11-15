#pragma once

#include "CoreMinimal.h"

class SWidget;
class UWorld;
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

	/**
	 * Your waypoint: one spot you set on the journal map (right-click; right-click it again to clear it), shown there and
	 * on the HUD compass with its distance. This client's own, for the world it was set in (another world drops it).
	 */
	WILDREACH_API bool GetWaypoint(const UWorld* World, FVector& OutWhere);
	WILDREACH_API void SetWaypoint(const UWorld* World, const FVector& Where);
	WILDREACH_API void ClearWaypoint();

	/**
	 * Your map pins (cookpass-ui 3.6): up to ui.pins.max marks with a glyph (0 camp, 1 ore, 2 danger, 3 creature, 4 loot,
	 * 5 flag) and a label, placed with a right-click (X on a pad) on the journal map; this PC's own, kept per world in
	 * Saved/Wildreach/Pins/<world>.json (the host's world id, or the address joined). The compass shows those near.
	 */
	struct FPin { FVector2D Where = FVector2D::ZeroVector; uint8 Glyph = 5; FString Label; };
	WILDREACH_API const TArray<FPin>& GetPins(const UWorld* World);
	/** A pin glyph drawn in code (line strokes) centred at Centre, Size across. */
	WILDREACH_API void DrawPinGlyph(class FSlateWindowElementList& Out, int32 Layer, const struct FGeometry& Geo, const FVector2D& Centre, float Size, uint8 Glyph, const FLinearColor& Color);
	/** "Camp", "Ore"... (a pin with no label shows its glyph's name). */
	WILDREACH_API FString PinGlyphName(uint8 Glyph);

	/**
	 * A vision on the map (the Hollow Root's Thrumming Root: the four heart-trees, cooks/biome-forest.md 2.5): these
	 * places flash on this client's journal map for Seconds, pulsing green, and the map centres on them. Client only.
	 */
	WILDREACH_API void Flash(const TArray<FVector>& Where, const FString& Name, float Seconds);
}
