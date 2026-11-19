#pragma once

// Cave rock built off the game thread (the Crystal caves, the Fungal caverns). A cube's field and surface net are
// computed on a worker (FWRCaveMesh, from static land data only), and the game thread just turns finished ones into
// mesh components. Built in place, on the game thread, each cube's field blocked the frame: 11 cubes a frame came to
// about 61 ms on the server for its first ten minutes (11-18 trace).

#include "CoreMinimal.h"
#include "Tasks/Task.h"

struct FWRCaveMesh
{
	FIntVector Key = FIntVector::ZeroValue;
	TArray<FVector> Verts;
	TArray<int32> Tris;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FColor> Colours;
	/** Skipped without building (a cube far from every surface): counted, nothing to draw. */
	bool bSkipped = false;
};

using FWRCaveMeshPtr = TSharedPtr<FWRCaveMesh, ESPMode::ThreadSafe>;

struct FWRCaveJob
{
	UE::Tasks::FTask Task;
	FWRCaveMeshPtr Mesh;
};
