#pragma once

// The underground caves of a heightfield land (the round island's: land file "caves", FWRLandCave): the boundary of each
// cave's air unioned with the open air over the ground, meshed by surface nets in 20 m cubes on worker threads (as the
// Fungal caverns' rock, WRCaveBuild.h), built on every machine (the server needs the collision, clients the look). Where
// a cave's passage breaks the surface (its mouth: round_ground.mjs cut the land's ground there) the rock rolls over the
// lip into the ground with no crack; the part of that skin lying on ground the land draws itself is dropped. The walls
// are the land's rock (dark basalt in the Volcanic, ice in the Tundra); the lip is the land's own ground colour.

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WRCaveBuild.h"
#include "WRLandCaves.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;
struct FWRLandDef;

UCLASS(NotPlaceable)
class WILDREACH_API AWRLandCaves : public AActor
{
	GENERATED_BODY()

public:
	AWRLandCaves();
	virtual void Tick(float DeltaSeconds) override;
	bool IsBuilt() const { return bQueued && Next >= Queue.Num() && InFlight.Num() == 0; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	/** A cube's rock, worked out on a worker thread from the land's static data alone. Tris: the walls; Lip: the skin
	 *  over a mouth's lip (the land's ground colour). */
	struct FCube
	{
		FWRCaveMesh Mesh;
		TArray<int32> Lip;
		/** The land's own vertex data (WRLand BuildChunk: colour with gloss in alpha, the layered ground's mixes in UV1-3),
		 *  bilinear from its samples, so the lip draws as the land does. */
		TArray<FLinearColor> Colours;
		TArray<FVector2D> UV1, UV2, UV3;
		/** The cave's own rock (M_CaveRock, the Volcanic's basalt; M_CaveIce, the Tundra's ice): its colour (RGB) and
		 *  roughness (A). Basalt: UV1 = (the height above the passage's floor, a puddle's depth under its waterline + 0.3 m),
		 *  UV2 = (heat, the signed distance across a seam still glowing, m). Ice: UV1 = (ice 0..1, thin cover 0..1), UV2 = (how deep: the ice darkens,
		 *  clear ice). Toward the ground's surface the colour fades to the ground's. */
		TArray<FColor> RockColours;
		TArray<FVector2D> RockUV1, RockUV2;
		/** The lip's tangents (U's direction, as its UVs were projected). */
		TArray<FVector> LipTangentX;
		FName Biome;
	};
	using FCubePtr = TSharedPtr<FCube, ESPMode::ThreadSafe>;
	struct FJob { UE::Tasks::FTask Task; FCubePtr Cube; };
	static void ComputeCube(const FWRLandDef* Land, const FIntVector& Key, float Cell, int32 Cells, const FVector& ActorLoc, float GroundSkin, FCube& Out);
	void FinishCube(const FCube& C);
	/** The cave mesh's rock: M_CaveRock (the Volcanic's basalt) or M_CaveIce (the Tundra's ice). */
	UMaterialInterface* WallMaterial(FName Biome);
	/** The formations' rock meshes (rocks, benches): M_Rock tinted. */
	UMaterialInterface* FormationMaterial(FName Biome);
	/** The caves' formations (the look kit's spikes, rods and slabs; the rock set's rocks), their collision (every machine)
	 *  and their lights (clients). */
	void BuildFormations();

	const FWRLandDef* Land = nullptr;
	TArray<FIntVector> Queue;
	TArray<FJob> InFlight;
	int32 Next = 0;
	bool bQueued = false;
	float Cell = 100.f;
	int32 Cells = 20;
	int32 Meshes = 0;
	/** Vertices no longer held twice by a mesh with both a rock and a lip section (FinishCube), for the build log. */
	int64 SplitSaved = 0;
	UPROPERTY() TArray<TObjectPtr<UProceduralMeshComponent>> Rock;
	UPROPERTY() TMap<FName, TObjectPtr<UMaterialInterface>> Walls;
	UPROPERTY() TMap<FName, TObjectPtr<UMaterialInterface>> FormationMats;
	/** The ice's material instance: its thin places glow with the day ("Glow", set each second). */
	UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> IceLook;
	/** The skylights' daylight (bounce off the sunlit floor) and its full intensity: scaled by the day each second. */
	UPROPERTY() TArray<TObjectPtr<class UPointLightComponent>> DayLights;
	TArray<float> DayLightFull;
	bool bBuiltLogged = false;
	UPROPERTY() TObjectPtr<UMaterialInterface> GroundLook;
};
