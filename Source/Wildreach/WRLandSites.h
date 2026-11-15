// A runtime land's places (cooks/build-biome-void.md 2.4-2.5): landmarks, camps, caves, story sites, the kit POIs the
// seeded pass places, decoration between them, and the Unmade Place. Each kind is a builder that composes the place
// from the look kit at full scale with its night light; every machine builds the same from the land file.
#pragma once

#include "CoreMinimal.h"

class AWRLand;

namespace WRLandSites
{
	/** Every place of the land (after its ground is built). */
	WILDREACH_API void Build(AWRLand* Land);
	/** A kit POI the seeded pass placed (the same on every machine): its land, kind and map point. */
	struct FKitPoi { FName Land; FName Kind; FVector2D At; };
	WILDREACH_API TArray<FKitPoi>& KitPois();
}
