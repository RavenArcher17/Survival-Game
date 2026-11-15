// The Desert's places contract (Data/desert.json, written by Tools/terrain/desert.mjs; cooks/build-biome-desert.md
// 2.1-2.19): every landmark, site, kit POI, decoration instance, cave, the Hourhold's rooms and ways, resource node,
// hazard (quicksand, brine, the belt, the flood wadis, the dust devils' Pan), arena, route, the tilted stelae, the dig
// spots and the portal slots over the desert's ground (Data/lands/desert.json). Parsed once; positions in game cm.
#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class AActor;
class UProceduralMeshComponent;

/** The desert ground's surface classes (the byte beside each height in desert_ground.bin; desert.mjs SURF). The
 *  movement, the hazards and the sand-swimmers all read the class under a foot. */
namespace EWRDesertSurf
{
	enum Type : uint8
	{
		Zone = 0, Sand = 1, WetSand = 2, Salt = 3, Brine = 4, Grass = 5, SoftSand = 6, Slipface = 7, ShellGrit = 8,
		Quicksand = 9, Oasis = 10, Gravel = 11, Hardpan = 12, RedRock = 13, Slot = 14, Glass = 15, Pavement = 16,
		Blown = 17, Road = 18, Nitre = 19, Plaza = 20, Sea = 21, Spine = 22, None = 255
	};
}

struct WILDREACH_API FWRDesertData
{
	struct FPlace
	{
		FName Id;
		FName Kind;
		FString Name;
		FName Zone;
		FVector At = FVector::ZeroVector;
		float Yaw = 0.f;
		int32 Variant = 0;
		float Size = 0.f;
		float Lean = 0.f;
		bool bCamp = false;
		bool bTilted = false;
		FName Ore;
		FName Route;
		FVector Ship = FVector::ZeroVector;   // a camp's newcomer ship on the beach below it
	};
	/** A decoration instance: where, yaw, scale. */
	struct FDecor { FVector At = FVector::ZeroVector; float Yaw = 0.f; float Scale = 1.f; };
	struct FCave
	{
		FName Id;
		FString Name;
		FName Zone;
		FVector At = FVector::ZeroVector;   // the mouth's ground point
		float Floor = 0.f;                  // the rooms' floor (cm, world z)
		float Roof = 600.f;                 // floor to ceiling (cm)
		TArray<FVector> Rooms;              // (dx, dy, radius) cm from At
		FVector MouthOffset = FVector::ZeroVector;
		float MouthYaw = 0.f, MouthW = 600.f, MouthH = 500.f;
		bool bSandFloor = false;
		bool bShaft = false;
		TArray<FVector4f> Flooded;          // (dx0, dy0, dx1, dy1) cm: a flooded run you swim
		FName Arena;
		TMap<FName, int32> Creatures;
	};
	/** One of the Hourhold's rooms: its floor centre (cm), size w x d x h (cm), what it holds. */
	struct FRoom
	{
		FName Id;
		FString Name;
		FVector At = FVector::ZeroVector;
		float W = 1000.f, D = 1000.f, H = 600.f;
		int32 Pillars = 0, Shafts = 0, Ushabti = 0, Mirrors = 0;
		float Water = -1.0e9f;              // a water surface (cm, world z), or none
		FVector Well = FVector::ZeroVector;
		FName Sealed;                       // gallery | long_noon
		bool bQuicksand = false, bRope = false, bSpring = false, bSandRamp = false;
	};
	/** One of the Hourhold's ways in: its mouth on the surface and the room it reaches. */
	struct FWay { FName Id; FName Kind; FVector From = FVector::ZeroVector; FName To; FVector ToAt = FVector::ZeroVector; FName Opens; };
	struct FNode { FName Item; FVector At = FVector::ZeroVector; bool bTree = false; bool bWild = false; bool bHall = false; FName Cave; float Scale = 1.f; };
	/** A quicksand pocket or a brine patch: centre (cm, its surface) and radius (cm). */
	struct FPit { FVector At = FVector::ZeroVector; float R = 400.f; };
	struct FFlood { FName Id; TArray<FVector> Pts; float Width = 1000.f; TArray<FVector> Ledges; };
	struct FTilted { FVector Stela = FVector::ZeroVector; FVector Cache = FVector::ZeroVector; bool bSeventh = false; };
	struct FSlot { FName Rank; FVector At = FVector::ZeroVector; };

	bool bLoaded = false;
	float PanZ = 2600.f;                 // the White Pan's level (cm)
	FBox2D Frame = FBox2D(ForceInit);    // cm
	TMap<FName, FString> ZoneNames;
	TArray<FVector> Coast;               // cm, z 0
	TArray<FVector2D> PanPoly;           // cm
	TArray<FVector> Wadi, Saltgate, WhaleRoad;
	FVector MotherAt = FVector::ZeroVector;
	float MotherH = 14000.f;
	TArray<float> MotherArms;
	TArray<FPlace> Landmarks, Sites, Kit;
	TArray<FDecor> Grass, Driftwood, Saltbush, Palms, Reeds, Acacias, Pears, Rocks, Pebbles, Hoodoos, Shells;
	TArray<FCave> Caves;
	FVector HourholdAt = FVector::ZeroVector;
	float HourholdSurface = 0.f;
	TArray<FRoom> Rooms;
	TArray<TPair<FName, FName>> Links;
	TArray<FWay> Ways;
	TArray<FNode> Nodes;
	TArray<FVector> Bloom;
	TArray<FPit> Quicksand, Brine;
	float BeltSouth = 0.f, BeltNorth = 0.f, BeltWest = 0.f, BeltEast = 0.f;   // cm
	TArray<FFlood> Floods;
	TArray<FVector2D> DevilPoly;
	float DevilFrom = 11.f, DevilTo = 16.f;
	TArray<FVector> Digs;
	TArray<FTilted> Tilted;
	TMap<FName, TArray<FVector>> Routes;
	TArray<FSlot> PortalSlots;
	TSharedPtr<FJsonObject> Arenas;

	static const FWRDesertData& Get();
	const FPlace* FindSite(FName Id) const;
	const FPlace* FindLandmark(FName Id) const;
	const FCave* FindCave(FName Id) const;
	const FRoom* FindRoom(FName Id) const;
	/** The west coast's x (cm) at a y (cm). */
	float CoastX(float Y) const;
	bool InPan(const FVector2D& P) const;
	/** A number from the arenas block: arenas.<Arena>.<Key>. */
	float ArenaNum(const TCHAR* Arena, const TCHAR* Key, float Fallback) const;
	/** A point from the arenas block (metres -> cm). */
	bool ArenaPoint(const TCHAR* Arena, const TCHAR* Key, FVector& Out) const;
	/** Points (metres -> cm) from the arenas block. */
	TArray<FVector> ArenaPoints(const TCHAR* Arena, const TCHAR* Key) const;
	bool Parse(const TSharedPtr<FJsonObject>& J);
};

namespace WRDesert
{
	/** The Desert is in the game (tuning world.liveBiomes has "desert") and its data loaded. */
	WILDREACH_API bool Live();
	/** Its data is loaded (the scenery can be built before the land goes live in the lists). */
	WILDREACH_API bool Loaded();
	WILDREACH_API FName Biome();
	/** Inside the Desert's frame (its ground box and the belt's north strip over the Fungal's dunes). */
	WILDREACH_API bool Contains(const FVector& World);
	/** The sub-zone (D1..D5) at a point, or None outside. */
	WILDREACH_API FName ZoneAt(const FVector& World);
	/** The ground's surface class under a point (EWRDesertSurf), None outside the land's ground. */
	WILDREACH_API uint8 SurfaceAt(const FVector& World);
	/** Soft sand underfoot (softsand, slipface, blown, the shell grit): the slow, and where sand-swimmers dive. */
	WILDREACH_API bool IsSoftSand(uint8 Surf);
	/** In the Haboob Wall's belt (D5 and the Fungal strip north of it): 25 m sight, x0.7 run, no Radiance. */
	WILDREACH_API bool InBelt(const FVector& World);
	/** On the White Pan (the dust devils, the mirages, the Long Noon's bloom). */
	WILDREACH_API bool InPan(const FVector& World);
	/** In a cave or the Hourhold (underground: no rescue, the sky light fades, it counts as night for Radiance). */
	WILDREACH_API bool InCave(const FVector& World);
	WILDREACH_API bool InHourhold(const FVector& World);
	/** The quicksand pocket under a point (its index), or INDEX_NONE. */
	WILDREACH_API int32 QuicksandAt(const FVector& World);
	/** The brine patch under a point, or INDEX_NONE. */
	WILDREACH_API int32 BrineAt(const FVector& World);
	/** The Desert's gathering nodes' looks (WRDesertNodes.cpp; WRResourceLook asks): is it one, its collider (width,
	 *  height cm), and the look modelled on the node (every drawing machine). */
	WILDREACH_API bool IsNodeItem(FName Item);
	WILDREACH_API FVector2D NodeBody(FName Item);
	WILDREACH_API void NodeLook(AActor* Node, FName Item, uint32 Seed);
	/** A thrown thing of the desert modelled in code (WRProjectile's "code:" models): the Oasis water jar, the flash pot,
	 *  the red ochre beer, a sunglass-tipped arrow, Ahrumet's glassfall glob, the kite's bone. Along +X. */
	WILDREACH_API bool BuildThrownModel(const FString& ModelPath, UProceduralMeshComponent* Comp);
}
