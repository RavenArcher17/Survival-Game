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
	/** Its own colour for steep ground ("slopeRock": the Volcanic's basalt cliffs, the glacier's blue ice faces), or the
	 *  land's slopeRock when it has none. */
	FLinearColor SlopeRock = FLinearColor::Gray;
	bool bOwnSlope = false;
	float Rough = 0.9f;
	FName Region;
	FName Bed;
	TSharedPtr<FJsonObject> Decor;
	/** A heightfield land's zone (the Ancient forest): its letter in the land's zone grid, and specks of other colours
	 *  scattered through it (wildflowers in the meadow, moss in the leaf litter) by SpeckAmount at SpeckScale m. */
	TCHAR Letter = 0;
	TArray<FLinearColor> Speck;
	float SpeckAmount = 0.f, SpeckScale = 5.f;
	/** A layered ground's mix here ("layers": litter, moss, soil, grass, stones, sand; WRLand::LayerCount), or all 0. */
	float Layers[6] = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
	bool bLayers = false;
	bool Contains(const FVector2D& M) const;
};

/** A heightfield land's surface class (written per sample beside its heights): river bed, gravel bank, sand, a road... */
struct WILDREACH_API FWRLandSurface
{
	uint8 Class = 0;
	FLinearColor Color = FLinearColor::Gray;
	float Rough = 0.9f;
	TArray<FLinearColor> Speck;
	float SpeckAmount = 0.f, SpeckScale = 4.f;
	float Layers[6] = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
	bool bLayers = false;
	/** "soft": an area (snow, moss, cinder), not a line (a road, a bed): blended over the land's surfaceBlend m round each
	 *  sample, so its edge doesn't step along the 4 m grid (round 10: the snow's edge was a staircase). */
	bool bSoft = false;
};

/** A wash of colour over a band of the map (the frost on the tundra neck's ferns): y from Y0 to Y1 m, patchy by noise. */
struct WILDREACH_API FWRLandBand
{
	float Y0 = 0.f, Y1 = 0.f, Amount = 0.f, NoiseM = 30.f;
	FLinearColor Color = FLinearColor::White;
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

/** The underground caves' shape (Tools/terrain/cave_shape.mjs CAVE: m). */
namespace WRCaveShape { inline constexpr float Join = 1.5f; }

/** One shape of an underground cave's air (Tools/terrain/cave_shape.mjs; map metres, z absolute). */
struct WILDREACH_API FWRLandCaveShape
{
	/** 0 a tube (a passage along P: x, y, floor, half-width; PH its height at each point), 1 a hall (an ellipsoid cut
	 *  flat by its floor: C its floor centre, R its half-axes along and across Yaw, H its height), 2 a shaft (a vertical
	 *  cylinder at C, radius R.X, from Z0 up to Z1). */
	uint8 Kind = 0;
	FName Role;
	TArray<FVector4f> P;
	TArray<float> PH;
	FVector C = FVector::ZeroVector;
	FVector2D R = FVector2D::ZeroVector;
	float Yaw = 0.f, H = 0.f, Z0 = 0.f, Z1 = 0.f;
	/** A skylight's debris cone (land file "talus": floor, height, radius m; 0 height for none). */
	FVector Talus = FVector::ZeroVector;
	FBox Box = FBox(ForceInit);
	/** Signed distance (m, approximate: negative inside), the floor under the point and the shape's size there. Bench:
	 *  the cave's lava bench (height above the floor, depth; 0 depth for none). */
	float Air(const FVector& Pm, float& OutFloor, float& OutWidth, const FVector2f& Bench = FVector2f::ZeroVector) const;
};

/** A formation of a cave (drip, icicle, mite, icemite, column, icecolumn, rock, rimrock, bench, crack, sulfur, icefall,
 *  pool, trapped): at At (map metres), its sizes A and B (m), turned Yaw (deg). */
struct WILDREACH_API FWRLandCaveFeat
{
	FName Kind;
	FVector At = FVector::ZeroVector;
	float A = 0.f, B = 0.f, Yaw = 0.f;
};

struct WILDREACH_API FWRLandCaveLight
{
	FVector At = FVector::ZeroVector;
	FLinearColor Color = FLinearColor::White;
	float Intensity = 0.f, Radius = 0.f;   // radius in m
	/** Daylight thrown in by a skylight: its intensity follows the day. */
	bool bDay = false;
};

/** A field of open crevasses cut into a heightfield land's glacier (land file "crevasses", round 14: the Crevasse Maze):
 *  its frame (U across, V down the slope, map units) and its slots, each a trench down V: its middle (map m), half length,
 *  half width and depth (m) and the ramp its floor climbs to the surface at each end (m). WRLandCaves draws them as the
 *  caves' rock (their ice walls, the ground's lip over them); the maze's builder lays its bridges across them. */
struct WILDREACH_API FWRLandCrevasses
{
	FName Id;
	FVector2D At = FVector2D::ZeroVector, U = FVector2D(1.f, 0.f), V = FVector2D(0.f, 1.f);
	struct FSlot { FVector2D C = FVector2D::ZeroVector; float HalfLen = 0.f, HalfW = 0.f, Depth = 0.f, Ramp = 0.f; };
	TArray<FSlot> Slots;
};

/** An underground cave of a heightfield land (land file "caves"; Tools/terrain/cave_layout.mjs lays it, round_ground.mjs
 *  sites it and cuts the ground at its mouth and skylights): a small network of shapes joined smoothly (an entrance,
 *  chambers, passages, a crawl to a pocket, a squeeze, a ledge and its ramp, a skylight), its formations and lights,
 *  and the spots its contents stand on. WRLandCaves builds its rock and formations, AWRCave (or a hidden tube) and the
 *  world systems its contents. */
struct WILDREACH_API FWRLandCave
{
	FName Id;
	FName Biome;
	FString Name;
	/** A hidden tube's room (<tube>_r<room>): its contents are the tube's. */
	bool bRoom = false;
	FVector2D Dir = FVector2D(1.f, 0.f);
	FVector Mouth = FVector::ZeroVector;
	/** A lava tube's bench (height above the floor, depth, m; land file "bench"): below it the way is narrower. */
	FVector2f Bench = FVector2f::ZeroVector;
	/** The main chamber's floor centre (a cave's big hall; a room's room). */
	FVector Hall = FVector::ZeroVector;
	TArray<FWRLandCaveShape> Shapes;
	TArray<FWRLandCaveFeat> Feats;
	/** The rock lip over the mouth (land file "brow": [x, y, z, along, across, up, yaw], map m): a solid ellipsoid the
	 *  cave's rock is drawn round (WRLandCaves), half in the hillside, overhanging the way in. */
	bool bHasBrow = false;
	FVector BrowC = FVector::ZeroVector, BrowR = FVector::OneVector;
	float BrowYaw = 0.f;
	TArray<FWRLandCaveLight> Lights;
	/** Where the contents stand (map metres): the ores in AWRCave's order, star iron, the hazard, the den, the cache. */
	TArray<FVector> OreSpots;
	FVector StarSpot = FVector::ZeroVector, HazardSpot = FVector::ZeroVector, DenSpot = FVector::ZeroVector, CacheSpot = FVector::ZeroVector;
	float CacheYaw = 0.f;
	bool bHasCache = false, bHasSpots = false;
	/** Signed distance (m) to the cave's air at a map point with an absolute height (m): negative inside. OutFloor: the
	 *  floor of the shape nearest it, OutWidth that shape's size there (m). */
	float Air(const FVector& P, float* OutFloor = nullptr, float* OutWidth = nullptr) const;
	/** The cave's air's box (map metres, z absolute), grown by Grow. */
	FBox Bounds(float Grow) const;
};

struct WILDREACH_API FWRLandDef
{
	FName Id;
	FName Biome;
	FString Name;
	/** The map it stands on (the round island's lands: "L_Wildreach"). Empty: the Old Island's map, as before. */
	FString Map;
	/** Its heightfield waits to be read until its map begins (a land of another map costs the one playing nothing). */
	bool bHeightfieldPending = false;
	/** A zone letter's own biome (the round island holds every land in one ground: "zoneBiomes"); empty: Biome. */
	TMap<TCHAR, FName> ZoneBiomes;
	/** Far detail levels (the round island, "lod"): a chunk at least LodRanges[k] m from the view uses every 2^(k+1)th
	 *  sample; ground you can stand on (collision) only within CollisionRange m of a player, a chunk round anyone else. */
	bool bLod = false;
	TArray<float> LodRanges;
	float CollisionRange = 600.f;
	/** The biome at a map point: its zone letter's (ZoneBiomes), else the land's. */
	FName BiomeAtMap(const FVector2D& M) const;
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
	/** slopeRock.ice / iceClasses: steep ground of these surfaces (glacier, ice) shows this ice instead of the zone's rock
	 *  (round 7: a glacier's knob in the Snow Flats read as grey rock with ice stuck on it). */
	FLinearColor SlopeIce = FLinearColor::Transparent;
	TArray<uint8> SlopeIceClasses;
	FBox2D MapBox = FBox2D(ForceInit);
	TSharedPtr<FJsonObject> Json;

	// ---- A heightfield land (cooks/build-biome-forest.md 2.1-2.3: the Ancient forest). Its ground is a grid of samples
	// read from a file beside the land's json ("heightfield": file, x0, y0, step, samples, unitsPerMetre, offset,
	// surface), bilinear between them, instead of the crest-and-coast function; its colour comes from a zone-letter grid
	// ("zoneGrid") and the per-sample surface classes. The wasteland's function path is untouched.
	bool bHeightfield = false;
	FVector2D HfOrigin = FVector2D::ZeroVector;   // map metres of sample (0, 0)
	float HfStep = 4.f;
	int32 HfN = 0;
	float HfUnits = 16.f, HfOffset = 32768.f;
	TArray<uint16> HfHeights;
	TArray<uint8> HfSurface;                       // per sample: 0 the zone's ground, HfHidden no ground at all
	static constexpr uint8 HfHidden = 255;
	/** Map metres: north of this the ground is another land's (the island's landscape ends at y +2,016). */
	float OwnFromY = -1.0e9f;
	FVector2D GridOrigin = FVector2D::ZeroVector;
	float GridCell = 16.f;
	int32 GridW = 0, GridH = 0;
	TArray<FString> GridRows;
	float ZoneBlend = 45.f;
	// The zone's second colour in patches (HfGround): PatchSoft 0 = the old sharp 34 m patches; over 0, soft patches of
	// three sizes (PatchScale m and smaller) blended over +-PatchSoft (the round island: hard camouflage blotches from the air).
	float PatchScale = 34.f;
	float PatchSoft = 0.f;
	/** groundPatch.farBlend: a far chunk's vertex takes its ground averaged over the squares round it (one 4 m sample
	 *  every 16-32 m aliased the ground's specks and patches into camouflage blotches seen from the air). */
	bool bFarBlend = false;
	/** wetShore: the ground darkens within Sea m over the sea and Lake m over the lake's level (near its outline), by
	 *  Dark at the water's edge: the wet band a shore shows (11-22: grass and sand ran dry into the water). */
	float WetSea = 0.f, WetLake = 0.f, WetDark = 1.f, LakeLevel = 0.f;
	/** wetShore.shown: where the sea's edge is drawn (m: its material lifts and waves its surface over its level), the wet
	 *  band measured up from there (it was from 0, under the drawn water: no band showed). */
	float WetShown = 0.f;
	/** dampMoss [reach, most]: the moss on dry ground beside a bed, bank or marsh: within reach samples, at most this much
	 *  (2 and 0.7 unless a land says: the round island's 1 and 0.25, round 9: its moss drew a pale lime band stepped along
	 *  the 4 m samples either side of the stream). */
	int32 DampReach = 2;
	float DampMost = 0.7f;
	FBox2D LakeBox = FBox2D(ForceInit);
	TArray<FWRLandSurface> Surfaces;
	/** surfaceBlend: the soft surfaces' blend, m (0: none, every surface sharp to its sample as before). */
	float SurfaceBlend = 0.f;
	/** Each surface class's entry in Surfaces (256 of them, -1 none), filled as they're read. */
	TArray<int16> SurfaceIndex;
	const FWRLandSurface* SurfaceOf(uint8 Cls) const { return SurfaceIndex.Num() == 256 && SurfaceIndex[Cls] >= 0 ? &Surfaces[SurfaceIndex[Cls]] : nullptr; }
	bool IsSoftSurface(uint8 Cls) const { const FWRLandSurface* S = SurfaceBlend > 0.f ? SurfaceOf(Cls) : nullptr; return S && S->bSoft; }
	/** The soft blend's ragged ring round sample (I, J): nine samples' classes and weights (the sample itself 2, its ring 1
	 *  each, 0.5-1 x SurfaceBlend out as the noise turns). False when none of them is a soft surface. */
	bool HfSoftRing(int32 I, int32 J, uint8 OutCls[9], float OutW[9]) const;
	TArray<FWRLandBand> Bands;
	FLinearColor LedgeColor = FLinearColor::Transparent;
	/** Where bodies are rightly under the ground (no rescue): zone-grid letters, and ways [x1, y1, x2, y2, r] (m). */
	FString UndergroundLetters;
	TArray<FVector4f> UndergroundWays;
	TArray<float> UndergroundRadii;
	TArray<FVector2D> BuildFirst;
	/** Its underground caves (heightfield lands: the round island's). */
	TArray<FWRLandCave> Caves;
	/** Its open crevasse fields (the round island's Crevasse Maze). */
	TArray<FWRLandCrevasses> Crevasses;
	/** Where two lands both have ground at a point, the higher priority's is the ground there (the Graveyard's 1 over the
	 *  forest's west face of the Spine). */
	int32 Priority = 0;
	/** Boxes (map metres) of another land's ground this land replaces while it is live (the Graveyard cuts the forest's
	 *  Rootgrave plug): that land drops its triangles inside, and its lookups answer this land there. */
	struct FCut { FName Land; FBox2D Box = FBox2D(ForceInit); };
	TArray<FCut> Cuts;
	/** A live land cuts this land's ground here (WRLand "cuts"). */
	bool CutAt(const FVector2D& M) const;

	/** Heightfield: the sample at (I, J) clamped to the grid, metres. */
	float HfSample(int32 I, int32 J) const
	{
		I = FMath::Clamp(I, 0, HfN - 1);
		J = FMath::Clamp(J, 0, HfN - 1);
		return (float(HfHeights[J * HfN + I]) - HfOffset) / HfUnits;
	}
	uint8 HfSurfaceAt(int32 I, int32 J) const
	{
		return HfSurface.Num() == HfHeights.Num() ? HfSurface[FMath::Clamp(J, 0, HfN - 1) * HfN + FMath::Clamp(I, 0, HfN - 1)] : 0;
	}
	/** Heightfield: bilinear height (m) at a map point (clamped to the grid's edge). */
	float HfHeight(const FVector2D& M) const;
	/** Heightfield: no ground of this land here (another land's own ground covers it). */
	bool HfHiddenAt(const FVector2D& M) const;
	/** The zone-grid letter at a map point (0 outside the grid). */
	TCHAR GridLetter(const FVector2D& M) const;
	/** Heightfield: the colour (linear) and roughness of sample (I, J), its normal N. */
	FLinearColor HfGround(int32 I, int32 J, const FVector& N, float& OutRough) const;
	/** How wet the shore is at a sample (0..1, eased: wetShore), or -1 where the land has no wet band. The island's ground
	 *  takes it as its first UV's X (M_IslandGround: darker and glossier, as in rain; the vertex colour alone barely
	 *  darkened the sand there). */
	float HfWet(int32 I, int32 J) const;
	/** A layered ground (Aaron 11-16, r-forest-ground; cooks/wild-answers-1115.md 2.5): its own material ("groundMaterial",
	 *  /Game/Terrain/M_ForestGround) reads each sample's mix of six textured layers from UV1-UV3; the vertex colour stays
	 *  the sub-zone's tint. Empty: the plain vertex-colour ground. */
	FString GroundMaterial;
	bool bLayered = false;
	/** Heightfield: the layer mix (litter, moss, soil, grass, stones, sand; sums to 1) of sample (I, J): its sub-zone's,
	 *  blended across zone edges as the colour is, its surface's over it, damp moss by the water and in the hollows. */
	void HfLayers(int32 I, int32 J, float Out[6]) const;
	/** A body here is rightly under the ground (a heightfield land's underground letters and ways). */
	bool IsUnderground(const FVector2D& M) const;

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
	/** The map the lands are being built for (set as a game world begins): a land with a "map" is live only there, the
	 *  Old Island's lands (no "map") only on a map named like the island. */
	WILDREACH_API void SetCurrentMap(const FString& Map);
	WILDREACH_API const FString& CurrentMap();
	/** A land whose biome is live (tuning world.liveBiomes). */
	WILDREACH_API bool IsLive(const FWRLandDef& Def);
	/** The map being played has a land of its own (the round island, L_Wildreach): the Old Island's lands and everything
	 *  placed on them (their tides, hazards, bosses, shrines, scenery, creatures) stay off it. */
	WILDREACH_API bool OwnLandMap();
	WILDREACH_API const FWRLandDef* Find(FName Id);
	/** The live land containing a game point (its coast box + 300 m), or null. */
	WILDREACH_API const FWRLandDef* At(const FVector& World);
	/** That land's biome, or None. */
	WILDREACH_API FName BiomeAt(const FVector& World);
	/** Inside one of a live land's holes (the Great Rift) and below its lip: no ground-rescue there, the black is the floor.
	 *  Also true where a heightfield land says a body is rightly under its ground (the forest's Hollow Root, its squeezes). */
	WILDREACH_API bool InHole(const FVector& World);
	/** A live land's ground under one of its map points (metres), in game cm: rows placed on a land ("land" + "at" in
	 *  its map metres: shrines, masters, dens) stand on the height function, never a trace (chunks may still be building). */
	WILDREACH_API bool PlaceOn(FName LandId, const FVector2D& MapM, FVector& OutWorld);
	/** The ground height (game cm) under a game point inside a live land (outside every land, or over a hole: Fallback). */
	WILDREACH_API float GroundZAt(const FVector& World, float Fallback);
	/** A live heightfield land's own ground height (game cm) under a point (built or not: the samples); false outside
	 *  every such land or where it builds none (the island's strip). */
	WILDREACH_API bool GroundUnder(const FVector& World, float& OutZ);
	/** A live land's underground cave by id (a hidden tube's room: <tube>_r<room>), or null. */
	WILDREACH_API const FWRLandCave* FindCave(FName Id, const FWRLandDef** OutLand = nullptr);
	/** In one of a live land's underground caves, more than Depth cm under its ground (the sky's light fades there). */
	WILDREACH_API bool InLandCave(const FVector& World, float Depth = 150.f);
	/** In a land cave near one of its skylights: 1 within 6 m of the hole's edge, easing to 0 by 20 m (0 outside caves). */
	WILDREACH_API float SkylightNear(const FVector& World);
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
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	static class AWRLand* FindLand(const UWorld* World, FName Id);
	/** The map name without its PIE or streaming prefix. */
	static FString MapOf(const UWorld& World);

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
	/** What its ground chunks draw with (the land's instance, so rain wets anything else drawn with it too). */
	UMaterialInterface* GroundLook() const;
	UMaterialInterface* LightMaterial() const;
	void AddMover(const FWRLandMover& M) { Movers.Add(M); }
	/** The server's clock as every machine reads it (movers and pulses are on it). */
	double ServerTime() const;
	/** Server, at load: a land's "camps" (PlayerStarts tagged for its path's biome, a safe zone each, the starter
	 *  stations at one of them), only what the map doesn't already have. The ground is a function, so no wait. */
	static void SpawnCamps(UWorld* World, const FWRLandDef& Def);

protected:
	virtual void BeginPlay() override;

private:
	void QueueChunks();
	bool BuildChunk(const FIntPoint& Cell);
	bool BuildHeightfieldChunk(const FIntPoint& Cell, bool bAsync = true);
	/** Far detail levels (FWRLandDef::bLod): which chunks exist, at what level, with collision or not. */
	struct FLodChunk
	{
		TObjectPtr<UProceduralMeshComponent> Mesh = nullptr;
		int8 Level = -1;
		bool bCollision = false;
	};
	TMap<FIntPoint, FLodChunk> LodChunks;
	TSet<FIntPoint> LodCells;            // every chunk with ground above the sea floor
	struct FLodJob { FIntPoint Cell; int8 Level = -1; bool bCollision = false; float Priority = 0.f; };
	TArray<FLodJob> LodJobs;             // nearest last (popped first)
	float LodCheck = 0.f;
	int32 LodPasses = 0;
	bool bLodFirstPass = true;
	void LodStart();
	void LodTick(float DeltaSeconds);
	/** One chunk of a far-detail land at Level (stride 2^Level samples), with or without collision. */
	bool BuildLodChunk(const FIntPoint& Cell, int32 Level, bool bCollision, bool bAsync);
	void BuildHoles();
	/** Rain on a heightfield land: its ground's Wet look (M_LandGround's Wet scalar) eased in and out. */
	void StepWet(float DeltaSeconds);
	TArray<FIntPoint> Queue;
	TSet<FIntPoint> BuiltCells;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> GroundMID;
	float Wet = 0.f;
	float WetCheck = 0.f;
	int32 Built = 0;
	bool bGroundDone = false;
	bool bSitesDone = false;
	UPROPERTY() TArray<TObjectPtr<UProceduralMeshComponent>> Chunks;
	UPROPERTY() TObjectPtr<UMaterialInterface> GroundMat;
	UPROPERTY() TObjectPtr<UMaterialInterface> TearMat;
	UPROPERTY() TObjectPtr<UMaterialInterface> LightMat;
	TArray<FWRLandMover> Movers;
};
