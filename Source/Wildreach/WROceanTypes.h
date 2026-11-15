// The Ocean & reefs' places contract (Data/ocean.json, written by Tools/terrain/ocean.mjs; cooks/build-biome-ocean.md
// 2.1-2.20): every landmark, site, kit POI, decoration instance, cave, the Drowned Hall, resource node, hazard, arena,
// route, secret and portal slot over the coast's ground (Data/lands/ocean.json). Parsed once; positions in game cm.
#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class AActor;
class UProceduralMeshComponent;

struct WILDREACH_API FWROceanData
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
		bool bSea = false;
		bool bCamp = false;
		float Stack = 0.f;          // the Kelp Stack shrine: the sea stack's height (m)
	};
	/** A decoration instance: where, yaw, scale and (coral) its variant. */
	struct FDecor { FVector At = FVector::ZeroVector; float Yaw = 0.f; float Scale = 1.f; int32 Variant = 0; };
	struct FWall { FVector A = FVector::ZeroVector, B = FVector::ZeroVector; };
	struct FCave
	{
		FName Id;
		FString Name;
		FName Zone;
		FVector At = FVector::ZeroVector;   // the first room's floor centre
		float Roof = 6.f;                   // m from floor to ceiling
		bool bDry = false;
		TArray<FVector> Rooms;              // (dx, dy, radius) m from At
		FVector MouthOffset = FVector::ZeroVector;
		float MouthYaw = 0.f, MouthW = 6.f, MouthH = 5.f;
		FName Arena;
		FName Legendary;
		FVector Side = FVector::ZeroVector;
		TArray<FVector> Islands;            // Siren's Hollow: (dx, dy, radius) m
		TMap<FName, int32> Creatures;
	};
	struct FHallRoom { FName Id; FString Name; FVector At = FVector::ZeroVector; float R = 8.f; bool bAir = false; bool bDome = false; float AirTop = 0.f, WaterTop = 0.f; };
	struct FNode { FName Item; FVector At = FVector::ZeroVector; bool bUnder = false; bool bWild = false; FName Cave; float Size = 1.f; };
	struct FRip { FVector At = FVector::ZeroVector; float Length = 120.f, Width = 8.f; };
	struct FBlowhole { FVector At = FVector::ZeroVector; bool bHorn = false; FVector Ledge = FVector::ZeroVector; };
	struct FSurge { FVector At = FVector::ZeroVector; float Length = 60.f, Yaw = 180.f; };
	struct FFleet { FVector At = FVector::ZeroVector; int32 Count = 10; };
	struct FPocket { FName Cave, Hall; FVector At = FVector::ZeroVector; float R = 10.f; float Top = 0.f; };
	struct FSecret { FName Id; FName Kind; FVector At = FVector::ZeroVector; FName When; FName Needs; TMap<FName, int32> Pays; };
	struct FSlot { FName Rank; FVector At = FVector::ZeroVector; };

	bool bLoaded = false;
	float SeaM = 0.6f;
	FBox2D Frame = FBox2D(ForceInit);   // cm
	TMap<FName, FString> ZoneNames;
	TArray<FVector> Coast;              // cm, z 0
	TArray<FPlace> Landmarks, Sites, Kit;
	TArray<FDecor> Grass, Driftwood, Tamarisk, Kelp, Coral, Clams, Rocks, Barnacles, Urchins, Mangroves;
	TArray<FWall> Walls;
	TArray<FCave> Caves;
	FVector HallAt = FVector::ZeroVector;
	TArray<FHallRoom> HallRooms;
	FVector LowGateAt = FVector::ZeroVector, SluiceDoorTo = FVector::ZeroVector, BellDiveFrom = FVector::ZeroVector;
	TArray<FNode> Nodes;
	float TsunamiContour = 600.f;       // cm
	TArray<FVector> TsunamiStones;      // cm; W = yaw (in a parallel array)
	TArray<float> TsunamiYaw;
	TArray<FVector> DykeTowers;
	float DykeWallTop = -100.f;         // cm
	TArray<FVector> Sluiceway, Inlet, Road, Saltgate;
	TArray<FRip> Rips;
	TArray<FBlowhole> Blowholes;
	TArray<FSurge> Surges;
	TArray<FFleet> Fleets;
	TArray<FPocket> Pockets;
	TArray<FVector> MooringStones;
	TMap<FName, TArray<FVector>> Routes;
	TArray<FSecret> Secrets;
	TArray<FSlot> PortalSlots;
	TSharedPtr<FJsonObject> Arenas;

	static const FWROceanData& Get();
	const FPlace* FindSite(FName Id) const;
	const FPlace* FindLandmark(FName Id) const;
	const FCave* FindCave(FName Id) const;
	const FHallRoom* FindHallRoom(FName Id) const;
	/** The Pound's centre (cm, the flat's level) and its wall radius (cm). */
	FVector PoundAt() const;
	float PoundRadius() const;
	/** A number from the arenas block: arenas.<Arena>.<Key>. */
	float ArenaNum(const TCHAR* Arena, const TCHAR* Key, float Fallback) const;
	/** A point from the arenas block (metres -> cm). */
	bool ArenaPoint(const TCHAR* Arena, const TCHAR* Key, FVector& Out) const;
	/** Points (metres -> cm) from the arenas block. */
	TArray<FVector> ArenaPoints(const TCHAR* Arena, const TCHAR* Key) const;
	/** The coast's x (cm) at a y (cm): the east shore. */
	float CoastX(float Y) const;
	bool Parse(const TSharedPtr<FJsonObject>& J);
};

namespace WROcean
{
	/** The Ocean is in the game (tuning world.liveBiomes has "ocean") and its data loaded. */
	WILDREACH_API bool Live();
	WILDREACH_API FName Biome();
	/** Inside the Ocean's frame (its ground box, the sea out to the reef's drop and beyond). */
	WILDREACH_API bool Contains(const FVector& World);
	/** The sub-zone (O1..O5) at a point, or None outside. */
	WILDREACH_API FName ZoneAt(const FVector& World);
	/** In the sea (east of the coast, or under the sea's surface anywhere in the frame). */
	WILDREACH_API bool InSea(const FVector& World);
	/** In one of the sea caves or the Drowned Hall (underground: the rescue leaves you alone, the sky light fades). */
	WILDREACH_API bool InCave(const FVector& World);
	/** In the Drowned Hall (its music, its rules: no claims). */
	WILDREACH_API bool InHall(const FVector& World);
	/** Below the Tsunami Line (the King Tide's bore reaches here). */
	WILDREACH_API bool BelowTsunamiLine(const UObject* WorldContext, const FVector& World);
	/** The Ocean's gathering nodes' looks (WROceanNodes.cpp; WRResourceLook asks): is it one, its collider (width,
	 *  height cm), and the look modelled on the node (every drawing machine). */
	WILDREACH_API bool IsNodeItem(FName Item);
	WILDREACH_API FVector2D NodeBody(FName Item);
	WILDREACH_API void NodeLook(AActor* Node, FName Item, uint32 Seed);
	/** A thrown thing of the coast modelled in code (WRProjectile's "code:" models): the harpoon (ash shaft, bronze toggle
	 *  head, a coil of line at the butt), the chain-hook harpoon (Leviathan's hook), Thalassor's spout bomb, Old Barnacle's
	 *  barrel and anchor, the Sunstone flare. Along +X. */
	WILDREACH_API bool BuildThrownModel(const FString& ModelPath, UProceduralMeshComponent* Comp);
}
