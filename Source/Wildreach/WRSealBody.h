#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "WRSealBody.generated.h"

class AWRCreature;

/**
 * The Floeback's body (cooks/build-taming.md 2.5), built and moved in code: a harp-seal build 3.2 m long (blunt round head,
 * big dark eyes, a whiskered muzzle, a barrel body tapering to the hips, clawed fore flippers, fanned hind flippers) in
 * a silver-grey dappled coat with a dark harp mark over the shoulders and a pale belly, and a crust of sea ice grown on its
 * back (seven thick glassy slabs, rimed at the edges) where a rider sits.
 *
 * Every vertex is stored against the spine (where along it, and where round it) and re-placed each frame on a spine
 * that moves the way a phocid seal does: in water a side-to-side wave from the chest to the hind flippers (they sweep
 * across: the stroke), fore flippers tucked, head steady; on land a gallumph, a wave up and down the body as the chest
 * lifts and the fore flippers plant; still, it breathes and looks about; hit, it jerks; dead, it rolls onto its side.
 */
UCLASS()
class WILDREACH_API UWRSealBody : public UProceduralMeshComponent
{
	GENERATED_BODY()

public:
	UWRSealBody(const FObjectInitializer& ObjectInitializer);
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void Init(AWRCreature* InOwner);
	/** Tack it wears follows the seat on its back (the saddle on the ice crust). */
	void CarrySaddle(USceneComponent* Tack);
	void Flinch();
	void Die();

	/** Where along the spine the seat is (0 the hind flippers, 1 the nose). */
	static constexpr float SeatT = 0.56f;

private:
	struct FVert
	{
		float T = 0.f;            // along the spine
		FVector Off;              // in the spine's frame at T: X along, Y right, Z up (from the spine's centre)
		FVector Normal;           // in the same frame
		uint8 Part = 0;           // 0 body, 1-2 fore flippers (L/R), 3-4 hind flippers (L/R), 5 head-rigid
		FVector Pivot;            // a flipper's root in that frame
	};
	void Build();
	void Pose(float Dt);
	/** The spine at T now: its centre and its frame (X along, Y right, Z up), local to the component. */
	void SpineAt(float T, FVector& OutCentre, FQuat& OutFrame) const;
	float Radius(float T) const;

	TWeakObjectPtr<AWRCreature> Creature;
	TArray<FVert> Rest;
	TArray<FVector> Verts, Normals;
	TArray<FVector2D> UVs;
	TArray<FColor> Colors;
	TArray<int32> Tris;
	TArray<TWeakObjectPtr<USceneComponent>> Carried;

	float Clock = 0.f;          // the motion's phase clock
	float SwimBlend = 0.f;      // 0 on land .. 1 in the water
	float Pace = 0.f;           // 0 still .. 1 at full speed
	float LookYaw = 0.f, LookTarget = 0.f, LookClock = 0.f;
	float FlinchLeft = 0.f;
	float DeadBlend = 0.f;
	bool bDead = false;
	bool bBuilt = false;
};
