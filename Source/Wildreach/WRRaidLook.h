#pragma once

#include "CoreMinimal.h"

struct FWRToolMesh;

/** The raid tools and the raid bell, modelled in code (WRRaidLook.cpp; FWRToolMesh::ApplyTo puts one on a procedural mesh). */
namespace WRRaidLook
{
	/** The bronze bell on its yoke, the pin (pivot) at the origin along Y, the bell hanging below (-Z). */
	FWRToolMesh Bell();
	/** The clapper, pivot at the origin, hanging down. */
	FWRToolMesh Clapper();
	/** The iron bracket strapped round a post (axis Z through the origin), the pin's seat at (Reach, 0, -6). */
	FWRToolMesh Bracket(float PostRadius, float Reach);
	/** The petard on its plank, the wall face at X = 0, out toward +X. */
	FWRToolMesh Petard();
	/** The slow match's path, coil to touch hole (the burning end walks along it). */
	TArray<FVector> MatchCord();
	/** A mining cartridge tamped into rock (the face at X = 0). */
	FWRToolMesh MiningCartridge();
	/** A siege ladder of this rail length leaning so its foot stands FootOut from the wall (the foot at the origin, the wall toward -X). */
	FWRToolMesh Ladder(float Length, float FootOut);
	/** The fire pot: a clay jar along +X with its rag wick. */
	FWRToolMesh FirePot();
}
