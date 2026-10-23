#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WREditorTerrainLibrary.generated.h"

class UMaterialInterface;

/**
 * Editor-only helpers for building maps from code (called by Tools/terrain/import_island.py).
 * The engine has no Python API for creating a landscape from a heightmap, so this does what the
 * editor's New Landscape tool does: spawn, Import, then split into World Partition streaming proxies.
 */
UCLASS()
class WILDREACH_API UWREditorTerrainLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Create a landscape in the current editor world from a raw 16-bit little-endian heightmap.
	 * @param HeightmapPath  .r16 file, Size x Size samples
	 * @param Size           samples per side (e.g. 4033)
	 * @param Location       world position of sample (0,0)
	 * @param Scale          actor scale (X/Y = cm per sample, Z = height scale; 100 = +-256 m range)
	 * @param QuadsPerSection 63 or 127
	 * @param SectionsPerComponent 1 or 2
	 * @param GridSize       World Partition grid size in components (0 = leave as one actor)
	 */
	UFUNCTION(BlueprintCallable, Category="Wildreach|Editor")
	static bool ImportLandscapeR16(const FString& HeightmapPath, int32 Size, FVector Location, FVector Scale,
		int32 QuadsPerSection, int32 SectionsPerComponent, int32 GridSize, UMaterialInterface* Material);

	/** True once every landscape in the editor world has finished its edit-layer GPU update. */
	UFUNCTION(BlueprintCallable, Category="Wildreach|Editor")
	static bool AreLandscapesUpToDate();
};
