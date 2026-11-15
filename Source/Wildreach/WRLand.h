// Lands built at runtime (cooks/build-biome-void.md 2.1-2.3): a land the island's map doesn't hold (the Corrupted
// wasteland first) is a data file in Data/lands/, its ground a pure function of the world-map's metres, built as
// chunks of procedural mesh on every machine (no replication, no map rebuild). Its POIs, rifts and hazards come from
// the same file (WRLandSites.cpp, WRRift.cpp).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRLand.generated.h"

class FJsonObject;
class UProceduralMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/** A sub-zone (V1..V5): polygons or a road corridor, its ground colours and what lives and sounds there. */
struct WILDREACH_API FWRLandZone
{
	FName Id;
	FString Name;
	TArray<TArray<FVector2D>> Polys;
	TArray<FVector2D> Road;
	float RoadWidth = 0.f;
	FLinearColor Ground = FLinearColor::Gray;
	FLinearColor GroundAlt = FLinearColor::Gray;
	float Rough = 0.9f;
	FName Region;
	FName Bed;
	TSharedPtr<FJsonObject> Decor;
	bool Contains(const FVector2D& M) const;
};

/** One shaping term of the ground (map metres): hole, gorge, ravine, flatten, road. */
struct WILDREACH_API FWRLandFeature
{
	FName Kind;
	FName Id;
	FVector2D A = FVector2D::ZeroVector;
	FVector2D B = FVector2D::ZeroVector;
	TArray<FVector2D> Points;
	TArray<float> Levels;      // road: the grade at each point; gorge: floor at A and B; flatten: [level]
	float R = 0.f, Width = 0.f, Depth = 0.f, Walls = 0.f, Blend = 0.f, LipDrop = 0.f;
};

struct WILDREACH_API FWRLandDef
{
	FName Id;
	FName Biome;
	FString Name;
	FVector Origin = FVector::ZeroVector;      // game cm the map centre sits on
	FVector2D MapCentre = FVector2D::ZeroVector;
	int32 Seed = 1;
	float Grid = 5.f, Chunk = 200.f, DrawDistance = 900000.f;
	int32 ChunksPerFrame = 3;
	TArray<FVector2D> Crest;
	TArray<FVector2D> HeightCurve;
	float NoiseScale = 180.f, NoiseAmp = 4.f, FineScale = 55.f, FineAmp = 1.f;
	TArray<FVector2D> Coast;
	float CoastBeach = 90.f, CoastSea = 140.f, SeaFloor = -12.f;
	TArray<FWRLandFeature> Features;
	TArray<FWRLandZone> Zones;
	FLinearColor SlopeRock = FLinearColor::Gray;
	float SlopeFrom = 30.f, SlopeTo = 44.f;
	FBox2D MapBox = FBox2D(ForceInit);
	TSharedPtr<FJsonObject> Json;

	FVector ToWorld(const FVector2D& M, float Zm) const
	{
		return Origin + FVector((M.X - MapCentre.X) * 100.f, (M.Y - MapCentre.Y) * 100.f, Zm * 100.f);
	}
	FVector2D ToMap(const FVector& W) const
	{
		return FVector2D((W.X - Origin.X) / 100.f + MapCentre.X, (W.Y - Origin.Y) / 100.f + MapCentre.Y);
	}
	/** Within the land's coast box grown by MarginM. */
	bool Contains(const FVector& W, float MarginM = 300.f) const;
	/** The ground's height (m) at a map point, holes ignored. */
	float Height(const FVector2D& M) const;
	/** Before the features: crest base + noise + coast (the features read their levels from it). */
	float BaseHeight(const FVector2D& M) const;
	/** Signed distance (m) to the coast: positive inside the land. */
	float CoastDistance(const FVector2D& M) const;
	/** Inside a hole feature (grown by Grow m). */
	const FWRLandFeature* HoleAt(const FVector2D& M, float Grow = 0.f) const;
	const FWRLandFeature* Feature(FName Id) const;
	const FWRLandZone* ZoneAt(const FVector2D& M) const;
	/** The ground's colour (linear) and roughness at a point whose surface normal is N. */
	FLinearColor Ground(const FVector2D& M, const FVector& N, float& OutRough) const;
	/** The ground point in game cm. */
	FVector GroundPoint(const FVector2D& M) const { return ToWorld(M, Height(M)); }
	/** A child object of the land file ("rifts", "crossing"...). */
	TSharedPtr<FJsonObject> Section(const TCHAR* Key) const;
	/** Value noise in [-1, 1] (seeded by the land). */
	float Noise(const FVector2D& P) const;
	bool Parse(const TSharedPtr<FJsonObject>& J);
};

namespace WRLand
{
	/** Every land file in Data/lands. */
	WILDREACH_API const TArray<FWRLandDef>& All();
	/** A land whose biome is live (tuning world.liveBiomes). */
	WILDREACH_API bool IsLive(const FWRLandDef& Def);
	WILDREACH_API const FWRLandDef* Find(FName Id);
	/** The live land containing a game point (its coast box + 300 m), or null. */
	WILDREACH_API const FWRLandDef* At(const FVector& World);
	/** That land's biome, or None. */
	WILDREACH_API FName BiomeAt(const FVector& World);
	/** Inside one of a live land's holes (the Great Rift) and below its lip: no ground-rescue there, the black is the floor. */
	WILDREACH_API bool InHole(const FVector& World);
	/** A live land's ground under one of its map points (metres), in game cm: rows placed on a land ("land" + "at" in
	 *  its map metres: shrines, masters, dens) stand on the height function, never a trace (chunks may still be building). */
	WILDREACH_API bool PlaceOn(FName LandId, const FVector2D& MapM, FVector& OutWorld);
	/** The ground height (game cm) under a game point inside a live land (outside every land, or over a hole: Fallback). */
	WILDREACH_API float GroundZAt(const FVector& World, float Fallback);
	/** The sub-zone at a game point (None outside every land). */
	WILDREACH_API FName ZoneAt(const FVector& World);
	/** sRGB array [r,g,b] -> linear. */
	WILDREACH_API FLinearColor Colour(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, const FColor& Fallback);
	WILDREACH_API FVector2D Vec2(const TSharedPtr<class FJsonValue>& V);
	WILDREACH_API float DistToSegment(const FVector2D& P, const FVector2D& A, const FVector2D& B, float* OutT = nullptr);
	/** Distance to a polyline and the point's place along it (segment index + t). */
	WILDREACH_API float DistToPolyline(const FVector2D& P, const TArray<FVector2D>& Line, int32* OutSeg = nullptr, float* OutT = nullptr);
}

/** Builds and keeps the live lands of this world (every machine builds its own copy, like the island's dressing). */
UCLASS()
class WILDREACH_API UWRLandSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	static class AWRLand* FindLand(const UWorld* World, FName Id);

private:
	TArray<TWeakObjectPtr<class AWRLand>> Lands;
};

/** Something of the land that floats or drifts (the floating rule): its place is a pure function of the server's clock,
 *  so every machine shows it in the same place and it never snaps. Base is relative to its parent. */
struct FWRLandMover
{
	TWeakObjectPtr<USceneComponent> Comp;
	FVector Base = FVector::ZeroVector;
	FRotator Rot = FRotator::ZeroRotator;
	float Bob = 0.f;            // cm up and down
	float BobPeriod = 10.f;     // s
	float Phase = 0.f;          // 0..1 of a period
	float Spin = 0.f;           // degrees a second about its own Z
	FVector Sway = FVector::ZeroVector;  // cm, drifting out and back
	float SwayPeriod = 120.f;
};

/** One land: its ground chunks (procedural mesh with collision), the hole walls and the black under them, then its sites. */
UCLASS(NotPlaceable)
class WILDREACH_API AWRLand : public AActor
{
	GENERATED_BODY()

public:
	AWRLand();
	virtual void Tick(float DeltaSeconds) override;

	FName LandId;
	const FWRLandDef* Def() const { return WRLand::Find(LandId); }
	/** True once every ground chunk exists (sites, rifts and spawns wait for it). */
	bool IsGroundReady() const { return bGroundDone; }
	/** Height of the ground (game cm) under a game point: the function, never a trace. */
	float GroundZ(const FVector& World) const;
	UMaterialInterface* GroundMaterial() const;
	UMaterialInterface* TearMaterial() const;
	UMaterialInterface* LightMaterial() const;
	void AddMover(const FWRLandMover& M) { Movers.Add(M); }
	/** The server's clock as every machine reads it (movers and pulses are on it). */
	double ServerTime() const;

protected:
	virtual void BeginPlay() override;

private:
	void QueueChunks();
	bool BuildChunk(const FIntPoint& Cell);
	void BuildHoles();
	TArray<FIntPoint> Queue;
	int32 Built = 0;
	bool bGroundDone = false;
	bool bSitesDone = false;
	UPROPERTY() TArray<TObjectPtr<UProceduralMeshComponent>> Chunks;
	UPROPERTY() TObjectPtr<UMaterialInterface> GroundMat;
	UPROPERTY() TObjectPtr<UMaterialInterface> TearMat;
	UPROPERTY() TObjectPtr<UMaterialInterface> LightMat;
	TArray<FWRLandMover> Movers;
};
