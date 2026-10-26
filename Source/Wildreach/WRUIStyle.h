#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"

/**
 * The game UI's look: dark stone and leather (see the mockups on the design canvas). Fonts (Cinzel, Alegreya Sans,
 * Alegreya Sans SC) and images (stone / leather / parchment tiles, item icons, creature portraits, wheel art, the
 * island map) load at runtime from Data/UI, so they ship with the game's data files and need no editor import.
 */
namespace WRUI
{
	/** Mockup px -> 1080p Slate units (the mockups are 1600x900). One definition, so unity builds can join UI files. */
	inline constexpr float U = 1.2f;

	// Colours (linear), named as in the mockups' tokens.
	extern const FLinearColor Ink;        // main text
	extern const FLinearColor Ink2;       // secondary text
	extern const FLinearColor Ink3;       // muted text
	extern const FLinearColor InkDark;    // text on parchment
	extern const FLinearColor Brass;      // selection, focus, trim
	extern const FLinearColor Brass2;     // darker trim
	extern const FLinearColor Ember;      // fire
	extern const FLinearColor Frost;      // frost
	extern const FLinearColor Good;
	extern const FLinearColor Bad;
	extern const FLinearColor Warn;
	extern const FLinearColor Hp;
	extern const FLinearColor Stamina;
	extern const FLinearColor Poise;
	extern const FLinearColor Hunger;
	extern const FLinearColor Thirst;
	extern const FLinearColor Torpor;
	extern const FLinearColor Edge;       // near-black panel edge
	extern const FLinearColor Rim;        // slot inner rim
	extern const FLinearColor Shadow;     // text shadow

	FLinearColor Alpha(const FLinearColor& C, float A);

	/** Cinzel (carved capitals) for titles: Weight 600 or 700. */
	FSlateFontInfo Title(int32 Size, int32 Weight = 700);
	/** Alegreya Sans for text: Weight 400, 500, 700 or 800. */
	FSlateFontInfo Body(int32 Size, int32 Weight = 500);
	/** Alegreya Sans SC (small capitals) for labels: Weight 500 or 700. */
	FSlateFontInfo Caps(int32 Size, int32 Weight = 700);

	/** A named UI image from Data/UI/<Name>.png: stone, leather, parchment (tiled), rivet, wheel_base, wheel_hi, map. */
	const FSlateBrush* Image(FName Name);
	/** A solid rounded box (fill, optional outline), cached by its values. */
	const FSlateBrush* Box(const FLinearColor& Fill, float Radius = 4.f, const FLinearColor& Outline = FLinearColor::Transparent, float OutlineWidth = 0.f);
	/** Plain white, for tinted rectangles and bars. */
	const FSlateBrush* White();
	/** An item's icon (Data/UI/Icons/<id>.png), or the "unknown" icon. */
	const FSlateBrush* Icon(FName ItemId);
	/** A creature's portrait (Data/UI/Portraits/<id>.png), or nullptr when it has none. */
	const FSlateBrush* Portrait(FName CreatureId);

	/** Data/UI on disk. */
	FString Dir();
}
