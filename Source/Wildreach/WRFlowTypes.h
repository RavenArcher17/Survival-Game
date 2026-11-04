#pragma once

#include "CoreMinimal.h"
#include "WRFlowTypes.generated.h"

class AActor;
class AWRCharacter;

/** A place the death screen offers to wake at (WRBeds): one of your beds, a clanmate's, or a camp. */
USTRUCT()
struct FWRWakeOption
{
	GENERATED_BODY()

	/** "Your bed", "Mira's bedroll", "Volcanic camp 3". */
	UPROPERTY() FString Name;
	/** bed, bedroll or camp. */
	UPROPERTY() FName Kind;
	UPROPERTY() FVector At = FVector::ZeroVector;
	/** Seconds this bed still rests after you last woke at it (0: ready). */
	UPROPERTY() float RestSeconds = 0.f;
	UPROPERTY() bool bClan = false;
};

/**
 * What the server tells a player who just died, for the death screen (WRUIFlow): who or what killed them, where their
 * belongings wait (the server's death penalty), and where and when they come back.
 */
USTRUCT()
struct FWRDeathNote
{
	GENERATED_BODY()

	/** "Cinderfang", "Vulkarn, the Forge Giant", a player's name, a cause ("Hunger", "Lava"), or empty when unknown. */
	UPROPERTY() FString Killer;
	/** The killer's creature id (its portrait), or None. */
	UPROPERTY() FName KillerCreature;
	UPROPERTY() bool bKillerIsPlayer = false;
	UPROPERTY() FVector DeathAt = FVector::ZeroVector;
	/** The server's death penalty: gentle (you keep everything), souls (a bag only you can open), drop, full. */
	UPROPERTY() FString Penalty;
	UPROPERTY() bool bBag = false;
	UPROPERTY() FVector BagAt = FVector::ZeroVector;
	UPROPERTY() int32 BagStacks = 0;
	UPROPERTY() float BagMinutes = 0.f;
	/** "your Bed", "Volcanic camp 3". */
	UPROPERTY() FString RespawnName;
	UPROPERTY() bool bRespawnBed = false;
	UPROPERTY() FVector RespawnAt = FVector::ZeroVector;
	/** Until the server brings you back (player.respawnSeconds). */
	UPROPERTY() float Seconds = 10.f;
	/** Where you can wake, the default first (WRBeds); the screen lets you pick another before the countdown ends. */
	UPROPERTY() TArray<FWRWakeOption> Wake;
	UPROPERTY() int32 WakePick = 0;
};

namespace WRFlow
{
	/**
	 * Server, as a survivor dies (after the death penalty ran): pin where they'll come back as the controller's start
	 * spot (their bed when one is set, else the camp they started at), and send them the death note.
	 */
	WILDREACH_API void SendDeathNote(AWRCharacter* Died, AActor* Killer);
}
