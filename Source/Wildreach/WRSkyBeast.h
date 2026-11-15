#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "WRSkyBeast.generated.h"

class AWRCreature;
class UMaterialInstanceDynamic;

/**
 * The Sky's bodies no rig anywhere has (cooks/biome-sky.md 2.9, build-biome-sky.md 2.11), built and moved in code on every
 * machine that draws, the UWRSealBody way (every vertex stored at rest and re-placed each frame by how the real animal
 * moves):
 *  - "ray": the Skyray, a 9 m manta diamond (charcoal back flecked pale, a white belly with black blotches unique to each
 *    one from its seed, a dark mouth slot and gill slits underneath, a violet rim glowing along the wing edge at night),
 *    two horn-lobes rolled up at the front that unroll before the Undertow, a whip tail with a barb. Its wings move in a
 *    travelling wave from root to tip (the manta's stroke: slow and shallow drifting, deep and fast hovering or breaching),
 *    it banks into turns, pitches with its climb, and dies belly-up.
 *  - "whale": the Cloudback (7 m) and Old Gulper (22 m, scarred): a small whale (grey-blue, white throat grooves, stubby
 *    pectoral fins, a fluke) floating on the levistone barnacles crusting its hump (they glow cyan); the tail beats up and
 *    down the body, the lower jaw drops for the Inhale (the throat's dark lining shows), it rolls on its axis.
 *  - "bell": the Sailbell, a clear-blue gas float with a violet crest-sail and a curtain of fourteen fine tentacles hanging
 *    12 m with violet beads (they glow at night); the float pulses, the strands hang and sway down their length like
 *    chains (each joint a beat behind the one above), sweep out in the Lash, and lie slack when it deflates.
 * Glowing parts are section 1 (M_Glow, brighter at night); the rest is section 0 (M_HeldTool, colour + roughness in the
 * vertex colour). Far from every camera it re-poses only twice a second.
 */
UCLASS()
class WILDREACH_API UWRSkyBeast : public UProceduralMeshComponent
{
	GENERATED_BODY()

public:
	UWRSkyBeast(const FObjectInitializer& ObjectInitializer);
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Kind ray / whale / bell; Scale x the kind's own size (Old Gulper's 22 m is a Cloudback x 3.14); Seed its marks. */
	void Init(AWRCreature* InOwner, FName InKind, float InScale, int32 InSeed, bool bScarred);
	FName GetKind() const { return Kind; }
	/** Where its mouth is (world): the Undertow's and the Inhale's cone start there. */
	FVector MouthWorld() const;

private:
	struct FV
	{
		FVector Rest;      // at rest, component space (X forward, Z up), cm at scale 1
		uint8 Part = 0;    // per kind (see WRSkyBeast.cpp)
		float A = 0.f;     // per kind: along (0 tail .. 1 nose), strand index...
		float B = 0.f;     // per kind: across (-1 .. 1), joint index...
	};
	struct FSection
	{
		TArray<FV> Rest;
		TArray<int32> Tris;
		TArray<FColor> Colors;
		TArray<FVector2D> UVs;
		TArray<FVector> Verts, Normals;
	};
	void BuildRay();
	void BuildWhale();
	void BuildBell();
	void BuildSerpent();
	/** Serpent: the spine's point and frame Dist cm behind the head along the path the head has flown (world). */
	void SpineAt(float Dist, FVector& OutAt, FQuat& OutFrame) const;
	void RecordTrail();
	FVector PlaceSerpent(const FV& V) const;
	void Pose(float Dt);
	FVector Place(const FV& V) const;
	/** Tentacle joint positions for one strand (bell), recomputed each pose. */
	void StrandJoints(int32 Strand, TArray<FVector>& Out) const;
	void Commit(bool bCreate);

	int32 Quad(FSection& S, int32 A, int32 B, int32 C, int32 D);
	int32 Add(FSection& S, const FVector& P, uint8 Part, float A, float B, const FColor& Col);
	void Tube(FSection& S, const TArray<FVector>& Points, const TArray<float>& Radii, int32 Sides, uint8 Part, const FColor& Col, float AStart, float AStep);

	TWeakObjectPtr<AWRCreature> Creature;
	FName Kind;
	float Scale = 1.f;
	int32 Seed = 0;
	bool bScar = false;
	FSection Body, Glow;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> GlowMat;

	// Pose state (eased from the creature's flags and motion).
	float Clock = 0.f;    // the stroke's phase (cycles)
	float Time = 0.f;     // seconds (sways, fins, tails, the curtain)
	float WaveAmp = 0.f, WaveFreq = 0.2f;
	float Lobes = 0.f, Jaw = 0.f, Lash = 0.f, Pulse = 0.f, Deflate = 0.f, Dead = 0.f;
	float Bank = 0.f, Pitch = 0.f, Roll = 0.f;
	float GlowNow = 0.f;
	float LastYaw = 0.f;
	float PoseClock = 0.f;
	FVector LastVel = FVector::ZeroVector;
	/** Bell: per strand where it hangs from, its sway phase; the curtain trails behind the drift. */
	TArray<FVector> StrandRoot;
	TArray<float> StrandPhase;
	FVector Trail = FVector::ZeroVector;
	/** Serpent: where the head has been (world, newest first) and the head's own frame this pose. */
	TArray<FVector> Path;
	FTransform HeadXf;
	float Sac = 0.f, Hood = 0.f, Grey = 0.f, Roll360 = 0.f, LastGrey = 0.f;
	TArray<FColor> BaseColors;
	static constexpr float SerpentLength = 6000.f;
	static constexpr int32 SerpentSegs = 60;
	static constexpr int32 StrandJointsN = 24;
	bool bBuilt = false;
};
