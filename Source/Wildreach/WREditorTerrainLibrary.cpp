#include "WREditorTerrainLibrary.h"
#include "Wildreach.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EngineUtils.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LandscapeSubsystem.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#endif

bool UWREditorTerrainLibrary::ImportLandscapeR16(const FString& HeightmapPath, int32 Size, FVector Location, FVector Scale,
	int32 QuadsPerSection, int32 SectionsPerComponent, int32 GridSize, UMaterialInterface* Material)
{
#if WITH_EDITOR
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		UE_LOG(LogWildreach, Error, TEXT("ImportLandscapeR16: no editor world"));
		return false;
	}

	const int32 QuadsPerComponent = QuadsPerSection * SectionsPerComponent;
	if (QuadsPerComponent <= 0 || (Size - 1) % QuadsPerComponent != 0)
	{
		UE_LOG(LogWildreach, Error, TEXT("ImportLandscapeR16: size %d doesn't fit %d quads per component"), Size, QuadsPerComponent);
		return false;
	}

	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *HeightmapPath) || Bytes.Num() != Size * Size * 2)
	{
		UE_LOG(LogWildreach, Error, TEXT("ImportLandscapeR16: %s missing or not %dx%d 16-bit (%d bytes)"), *HeightmapPath, Size, Size, Bytes.Num());
		return false;
	}
	TArray<uint16> Heights;
	Heights.SetNumUninitialized(Size * Size);
	FMemory::Memcpy(Heights.GetData(), Bytes.GetData(), Bytes.Num());  // little-endian on Windows

	ALandscape* Landscape = World->SpawnActor<ALandscape>(Location, FRotator::ZeroRotator);
	Landscape->SetActorRelativeScale3D(Scale);
	Landscape->LandscapeMaterial = Material;

	TMap<FGuid, TArray<uint16>> HeightData;
	HeightData.Add(FGuid(), MoveTemp(Heights));
	TMap<FGuid, TArray<FLandscapeImportLayerInfo>> Layers;
	Layers.Add(FGuid(), TArray<FLandscapeImportLayerInfo>());

	Landscape->Import(FGuid::NewGuid(), 0, 0, Size - 1, Size - 1, SectionsPerComponent, QuadsPerSection, HeightData,
		*HeightmapPath, Layers, ELandscapeImportAlphamapType::Additive, TArrayView<const FLandscapeLayer>());
	Landscape->SetActorLabel(TEXT("Landscape_Island"));

	ULandscapeInfo* Info = Landscape->GetLandscapeInfo();
	if (!Info)
	{
		UE_LOG(LogWildreach, Error, TEXT("ImportLandscapeR16: import produced no landscape info"));
		return false;
	}
	Info->UpdateLayerInfoMap(Landscape);

	if (GridSize > 0)
	{
		// World Partition: split into streaming proxies of GridSize x GridSize components.
		World->GetSubsystem<ULandscapeSubsystem>()->ChangeGridSize(Info, GridSize);
	}

	// Edit layers render the final heightmap/weightmap textures on the GPU during the editor's tick.
	// Without that the collision is right (CPU import) but the rendered surface is empty, so the build
	// script runs in the full editor and waits for AreLandscapesUpToDate() before saving.
	Landscape->ForceLayersFullUpdate();

	UE_LOG(LogWildreach, Log, TEXT("ImportLandscapeR16: %dx%d from %s, %d components per side, grid %d, layers up to date: %s"),
		Size, Size, *HeightmapPath, (Size - 1) / QuadsPerComponent, GridSize, Landscape->IsUpToDate() ? TEXT("yes") : TEXT("NO"));
	return true;
#else
	return false;
#endif
}

bool UWREditorTerrainLibrary::AreLandscapesUpToDate()
{
#if WITH_EDITOR
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return false;
	}
	for (TActorIterator<ALandscape> It(World); It; ++It)
	{
		if (!It->IsUpToDate())
		{
			return false;
		}
	}
	return true;
#else
	return true;
#endif
}
