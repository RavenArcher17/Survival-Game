// A runtime land's live things (cooks/build-biome-void.md 2.1, 2.5, 2.6): the Grey Crossing's two landings, and the
// commands, prompts and interactions every land thing answers through. Server authority: the server spawns them, they
// replicate their state; every machine draws them from that state.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WRLandThings.generated.h"

class AWRPlayerController;

/** One end of a crossing by boat: a longboat at a landing, a mooring post, a lantern. Rowing takes you to the other end. */
UCLASS(NotPlaceable)
class WILDREACH_API AWRCrossing : public AActor
{
	GENERATED_BODY()

public:
	AWRCrossing();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	/** The land this crossing reaches, and which end this is (true: the land's; false: the island's). */
	UPROPERTY(Replicated) FName LandId;
	UPROPERTY(Replicated) bool bLandEnd = false;
	/** Where a rower steps off at this end (on the ground, facing inland), set once the end is settled. */
	UPROPERTY(ReplicatedUsing = OnRep_Ready) FVector StepOff = FVector::ZeroVector;
	UPROPERTY(Replicated) float FaceYaw = 0.f;
	UPROPERTY(ReplicatedUsing = OnRep_Ready) bool bReady = false;
	TWeakObjectPtr<AWRCrossing> Other;

	FString PromptVerb() const;
	FString PlaceName() const;

protected:
	virtual void BeginPlay() override;
	UFUNCTION() void OnRep_Ready();

private:
	/** Server: find the shore (the island end traces; the land end reads the land's ground function). */
	bool Settle();
	void BuildLook();
	bool bBuilt = false;
	float RetryAt = 0.f;
};

namespace WRLandThings
{
	/** Server: spawn the land's crossings, rifts and machines once its ground is up (UWRLandSubsystem calls it). */
	WILDREACH_API void Start(UWorld* World);
	/** The interact prompt (HUD) for a land thing in reach. */
	WILDREACH_API bool Prompt(const AWRPlayerController* PC, FString& OutTarget, FString& OutVerb);
	/** E on a land thing: act on it (sends the server a "land ..." command). */
	WILDREACH_API bool ClientInteract(AWRPlayerController* PC);
	/** Server: "land cross" and friends; returns a line for the player or empty. */
	WILDREACH_API FString Command(AWRPlayerController* PC, const TArray<FString>& Args);
}
