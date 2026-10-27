#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WRSea.generated.h"

class FJsonObject;
class UProceduralMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialParameterCollection;

/** Where the open sea is: Data/sea_mask.json on a grid of square cells (outside the grid is all sea). */
struct WILDREACH_API FWRSeaMask
{
	int32 Width = 0;
	int32 Height = 0;
	float Cell = 400.f;                     // cm
	FVector2D Origin = FVector2D::ZeroVector;  // cm, the grid's -X -Y corner
	TArray<uint8> Sea;                      // 1 = sea, row-major from -Y
	/** Cells to the nearest land cell (chamfer distance), filled by ComputeDistance. */
	TArray<float> Dist;

	bool IsSea(int32 X, int32 Y) const { return X < 0 || Y < 0 || X >= Width || Y >= Height || Sea[Y * Width + X] != 0; }
	/** Reads the run-length rows (Tools/terrain/sea_mask.mjs). */
	bool Parse(const FJsonObject& Json);
	void ComputeDistance();
	/** Distance (cells) to land from a cell, outside the grid too (beyond its edge, the edge's distance plus the way out). */
	float DistanceAt(int32 X, int32 Y) const;
};

/** One square of the sea mesh, in mask cells (it may lie outside the mask). */
struct FWRSeaLeaf
{
	int32 X = 0, Y = 0, Size = 1;
	bool operator==(const FWRSeaLeaf& O) const { return X == O.X && Y == O.Y && Size == O.Size; }
};
inline uint32 GetTypeHash(const FWRSeaLeaf& L) { return HashCombine(HashCombine(::GetTypeHash(L.X), ::GetTypeHash(L.Y)), ::GetTypeHash(L.Size)); }

/** Pure sea math (tests). */
struct WILDREACH_API FWRSeaRules
{
	/**
	 * The sea's squares in mask cells: a quadtree RootCells wide (a power of two) centred on the mask, finest (one cell)
	 * at the shore and growing with the distance to land (a square may be as wide as its distance over Falloff), up to
	 * MaxCells; squares that are all land are left out, and neighbours differ by at most one size (so no cracks).
	 */
	static TArray<FWRSeaLeaf> BuildLeaves(const FWRSeaMask& Mask, int32 RootCells, float Falloff, int32 MaxCells);
	/** Where the quadtree starts, in mask cells (its -X -Y corner). */
	static int32 RootStart(const FWRSeaMask& Mask, int32 RootCells) { return Mask.Width / 2 - RootCells / 2; }
};

/**
 * The sea: moving water drawn with the WaterMaterials pack's ocean material (waves that move the surface, foam where it
 * is shallow at the shore, a darker colour as it deepens offshore), on a mesh that covers only the open sea
 * (Data/sea_mask.json): fine near the shore where you see the waves, coarse far out, split into tiles so what's off
 * screen isn't drawn. It never shows inland, where the distant terrain isn't streamed in. It replaces the map's flat
 * plane (hidden here, left in the map so a missing pack still shows a sea). Colours and how it dims at night in
 * tuning.json "sea". Client-only, like the sky.
 */
UCLASS(NotPlaceable)
class WILDREACH_API AWRSea : public AActor
{
	GENERATED_BODY()

public:
	AWRSea();
	virtual void Tick(float DeltaSeconds) override;
	/** Set one of the sea material's numbers now (console wr.SeaParam, look checks). */
	void SetMaterialScalar(FName Name, float Value);

protected:
	virtual void BeginPlay() override;

private:
	/** Builds the tiles; false if there's no mask. */
	bool BuildMesh();
	void ApplyLook(float Daylight);

	UPROPERTY(Transient) TArray<TObjectPtr<UProceduralMeshComponent>> Tiles;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> Material;
	UPROPERTY(Transient) TObjectPtr<UMaterialParameterCollection> Globals;
	/** Material parameters that dim at night, with their daytime values (tuning sea.dim). */
	TMap<FName, float> DayValues;
	float LastDaylight = -1.f;
	bool bActive = false;
};
