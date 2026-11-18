#pragma once

// The Unmade Place (cooks/biome-void.md 2.5): the pocket inside the Great Rift, 60-160 m below the lip. Floating Choir
// ruins in starless black reached by the Choir's Descent (a spiral stair cut 120 m down the rift's north wall, then
// floating stones, gaps 1.5-2.5 m, each on its voidglass keel) or the Descent lift (a Choir winch on the lip: a timber
// drum on an iron axle in two bearing blocks, a ratchet and pawl, two cranks, a rope over a timber jib to a basket
// cage; repaired with rope 10 and iron 4). Its eight places (U1-U8), the drain x1.5, the last note. Geometry is built on
// every machine from Data/lands/wasteland.json "unmade"; the lift's state replicates and the cage eases to it.

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WRVoidUnmade.generated.h"

class AWRLand;
class AWRPlayerController;
class UInstancedStaticMeshComponent;

UCLASS(NotPlaceable)
class WILDREACH_API AWRUnmadePlace : public AActor
{
	GENERATED_BODY()

public:
	AWRUnmadePlace();
	virtual void Tick(float DeltaSeconds) override;
	/** Server: the place and its lift (WRVoidWorld::LandReady). */
	static void Spawn(AWRLand* Land);
	FName LandId;

protected:
	virtual void BeginPlay() override;

private:
	void Build();
	float LipZ = 0.f;
	FVector Centre = FVector::ZeroVector;
	/** The Great Rift's radius (cm): the drain reaches only who is inside it, not every land's caves. */
	float RadiusCm = 0.f;
	FVector U8 = FVector::ZeroVector;
	TSet<FString> ReachedLastNote;
	float DrainClock = 0.f;
};

UCLASS(NotPlaceable)
class WILDREACH_API AWRDescentLift : public AActor
{
	GENERATED_BODY()

public:
	AWRDescentLift();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;
	static AWRDescentLift* Find(const UWorld* World);

	/** The prompts (WRLandThings): what E does here now, and doing it (server). */
	bool PromptFor(const FVector& At, FString& OutVerb) const;
	FString Use(AWRPlayerController* By);

	UPROPERTY(Replicated) bool bRepaired = false;
	/** Metres the cage is below the lip (0 at the top, Drop at the U1 ledge). */
	UPROPERTY(Replicated) float CageDepth = 0.f;
	/** +1 lowering, -1 raising, 0 held by the pawl. */
	UPROPERTY(Replicated) int8 Moving = 0;
	float Drop = 100.f;

protected:
	virtual void BeginPlay() override;

private:
	void Build();
	bool NearCrank(const FVector& At) const;
	bool InCage(const FVector& At) const;
	FVector CageTop() const;
	UPROPERTY() TObjectPtr<USceneComponent> Drum;
	UPROPERTY() TObjectPtr<USceneComponent> Cage;
	UPROPERTY() TObjectPtr<USceneComponent> Rope;
	UPROPERTY() TObjectPtr<USceneComponent> Pawl;
	UPROPERTY() TObjectPtr<USceneComponent> SnappedRope;
	float ShownDepth = 0.f;
	float LastClick = 0.f;
	TWeakObjectPtr<APawn> Cranker;
	TWeakObjectPtr<APawn> Repairer;
	double RepairStarted = 0.0;
};
