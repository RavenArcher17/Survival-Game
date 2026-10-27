#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRPreload.generated.h"

struct FStreamableHandle;

/**
 * Background preload of art the game loads by path the first time something appears: a creature when it comes within
 * the 150 m clients are sent creatures (its mesh, clips and materials from Data/creatures.json), a cave at 250 m, a
 * player's body or held weapon. Loading those on the spot stalls the frame (the benchmark's walk showed 70-300 ms
 * hitches exactly there); asking for all of them at map start, in the background, means they're in memory by then.
 * What's preloaded: every "/Game/..." path in the data files Data/preload.json lists, plus its own "assets" list. The
 * handle keeps them loaded for the life of the world. preload.enabled 0 in Data/tuning.json turns it off.
 */
UCLASS()
class WILDREACH_API UWRPreloadSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** Every object path the data asks to preload (only those whose package exists). */
	static TArray<FSoftObjectPath> CollectPaths();

private:
	TSharedPtr<FStreamableHandle> Handle;
};
