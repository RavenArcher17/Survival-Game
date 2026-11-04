#pragma once

#include "CoreMinimal.h"
#include "WRBeds.generated.h"

class AActor;
class AController;
class AWRBuildPiece;
class AWRPlayerState;
struct FWRDeathNote;

/** A bed or bedroll a player has set, as their client sees it (the map, the death screen). */
USTRUCT()
struct FWRBedSpot
{
	GENERATED_BODY()

	/** "Your bed", "Your bedroll", "Mira's bed". */
	UPROPERTY() FString Name;
	UPROPERTY() FVector At = FVector::ZeroVector;
	UPROPERTY() bool bBedroll = false;
	/** Set by a clanmate, not by you. */
	UPROPERTY() bool bClan = false;
};

/**
 * Beds and where you wake (server). A bed or bedroll remembers who has set it (its sleepers, saved with the world),
 * so it survives a restart and leaves everyone's list when it's demolished or smashed. Placing one sets it; E on it
 * sets it again (and makes it the one you wake at by default). When you die, the death screen's map offers every bed
 * you've set, your clan's beds and your path's camps; a bed you just woke at rests for beds.restSeconds, and a
 * bedroll is used up when you wake on it.
 */
namespace WRBeds
{
	bool IsBed(const AWRBuildPiece* Piece);
	bool IsBedroll(const AWRBuildPiece* Piece);
	/** "Your bed" as it reads mid-sentence ("You'll wake at your bed"); other names as they are. */
	WILDREACH_API FString InSentence(const FString& Name);

	/** Set this bed for the player and make it the one they wake at by default. */
	WILDREACH_API void Set(AWRPlayerState* PS, AWRBuildPiece* Bed);
	/** A bed is going away (demolished, smashed, collapsed): off every sleeper's list. */
	void OnGone(AWRBuildPiece* Bed);

	/** The beds this player has set, then their clan's (each once). */
	TArray<AWRBuildPiece*> BedsFor(const AWRPlayerState* PS);
	/** Rebuild the player's replicated bed list (their map) from the world's beds. */
	WILDREACH_API void Refresh(AWRPlayerState* PS);
	WILDREACH_API void RefreshAll(UWorld* World);

	/** A spot standing on top of the bed (its origin is on the floor: a body there sinks into it), alive for Seconds. */
	AActor* SpotAbove(AActor* Bed, float HalfHeight, float Seconds);

	/**
	 * As a player dies: list where they can wake (their default first) into the note and the player state, and pin the
	 * default as the controller's start spot.
	 */
	void FillWakeChoices(AController* Controller, AWRPlayerState* PS, FWRDeathNote& Note, float HalfHeight);
	/** The player picked another place on the death screen; false if it's not one they were offered or it's resting. */
	bool Choose(AController* Controller, AWRPlayerState* PS, int32 Index);
	/** They're back: rest the bed they woke at, or use up the bedroll. */
	void OnWoke(AWRPlayerState* PS);

	/** The save keeps where your default bed stands; on joining it's found again among the world's beds. */
	TSharedPtr<class FJsonObject> SaveDefault(const AWRPlayerState* PS);
	void LoadDefault(AWRPlayerState* PS, const TSharedPtr<class FJsonObject>& Json);
}
