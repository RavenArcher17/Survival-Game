#pragma once

#include "CoreMinimal.h"

/**
 * How a path shows in the UI, from its Data/paths/<id>.json ("color", "icon", "glyph", "resource"), so a new path (the
 * Lightning path, cooks/biome-lightning.md 2.18) shows in the HUD, the Paths screen, the wheel and the main menu without a
 * code change. Fire and Frost keep their mockup colours and pictures when their files don't say.
 */
namespace WRPathLook
{
	/** The path's colour (linear; sRGB in the data). */
	WILDREACH_API FLinearColor Color(FName Path);
	/** Its item icon (Data/UI/Icons): ember_essence, frost_essence, stormglass. */
	WILDREACH_API FName Icon(FName Path);
	/** Its line glyph (Data/UI/Glyphs): flame, snowflake, weather_storm. */
	WILDREACH_API FString Glyph(FName Path);
	/** "Fire", "Lightning". */
	WILDREACH_API FString Name(FName Path);
	/** Its resource's name ("Heat", "Chill", "Charge") and id ("heat", "chill", "charge"). */
	WILDREACH_API FString ResourceName(FName Path);
	WILDREACH_API FName ResourceId(FName Path);
	/** The biome's name in the main menu's path picker ("Volcanic", "Tundra", "Storm peaks"). */
	WILDREACH_API FString BiomeName(FName Path);
	/** The path's colour, darkened for a filled background (the picker, path pills). */
	WILDREACH_API FLinearColor Dark(FName Path, float Share = 0.22f);
}
