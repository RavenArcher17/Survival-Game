#pragma once

// Hands-on crafting (cooks/cookpass-crafting.md 3.1, 3.3): crafting by hand is a timed, committed work you see the hero
// do (knapping a flint, twisting fibre), and at a forge, armorer, workbench or alchemy table the hero walks to its work
// spot and works the order with their own hands (hammer and tongs at the anvil, a plane along the board, a pestle round
// the mortar), which runs the station x craft.work.handsOn while they're at it. The server decides (FWRCraftWork); every
// machine draws the body and the held tools from the replicated FWRWorkRep (WRCraftWorkLook.cpp).

#include "CoreMinimal.h"
#include "WRCraftWork.generated.h"

class AWRCharacter;
class AWRPlayerController;
class AWRStation;
struct FWRRecipe;

UENUM()
enum class EWRWork : uint8
{
	None,
	Knap,     // by hand, stone in the inputs: a hammerstone on a flint core
	Twist,    // by hand, the rest: fibre twisted between the hands
	Forge,    // the forge's anvil: hand hammer and tongs
	Armor,    // the armorer's block: a lighter, quicker planishing hammer
	Plane,    // the workbench: a plane pushed along the board
	Pestle,   // the alchemy table: a pestle ground round the mortar
};

/** What the hero is working (replicated to every machine). */
USTRUCT()
struct FWRWorkRep
{
	GENERATED_BODY()

	UPROPERTY() EWRWork Kind = EWRWork::None;
	/** What's being made: by hand its output, at a station the first order's output (the piece the station shows). */
	UPROPERTY() FName Item;
	/** The station worked (none by hand). */
	UPROPERTY() TObjectPtr<AWRStation> At = nullptr;
	/** At a station: where the hero stands and which way they face to work it (world). */
	UPROPERTY() FVector_NetQuantize Spot = FVector::ZeroVector;
	UPROPERTY() float Yaw = 0.f;
	/** By hand: one item's seconds, and n of N. */
	UPROPERTY() float Seconds = 0.f;
	UPROPERTY() uint8 Made = 0;
	UPROPERTY() uint8 Of = 0;
	/** Bumped for each new item or start, so a client restarts its cycle. */
	UPROPERTY() uint8 Serial = 0;

	bool IsStation() const { return Kind >= EWRWork::Forge; }
	bool IsHand() const { return Kind == EWRWork::Knap || Kind == EWRWork::Twist; }
};

struct WILDREACH_API FWRCraftWork
{
	/** Server: craft Times of a hand recipe, one after another, each a timed work (its inputs taken as it starts, back if
	 *  it's interrupted). Returns the line for the player. */
	static FString StartHand(AWRPlayerController* Player, FName RecipeId, int32 Times);
	/** Server: the hand batch stops after the one in the hands. */
	static FString StopHand(AWRPlayerController* Player);
	/** Server: start working Station with your own hands if its first order is yours (after ordering, or opening its
	 *  screen). Quiet when it can't (bQuiet), else the reason. */
	static FString StartStation(AWRPlayerController* Player, AWRStation* Station, bool bQuiet);
	/** Server, from the station's tick: keep its worker working, or end it (moved off, hit, busy, not their order). */
	static void TickStation(AWRStation* Station);
	/** Server: is Who at Station's spot working it now (the x handsOn speed)? */
	static bool IsWorking(const AWRStation* Station);
	/** Server: end the work (any kind) with an optional line. */
	static void End(AWRCharacter* Who, const FString& Why = FString());
	/** The hands-on kind a station type has (None: no hands-on work there). */
	static EWRWork KindFor(FName StationType);
	/** A hand recipe's kind: stone among its inputs knaps, the rest twists. */
	static EWRWork HandKind(const FWRRecipe& Recipe);
	/** Station frame (cm, the build piece's frame: the station's actor less ChildLift): where its worker stands, and the
	 *  yaw (degrees, piece frame) they face. False: no hands-on spot. */
	static bool SpotFor(FName StationType, FVector& OutLocal, float& OutYaw);
	/** The beat's rate for a kind (craft.work.<kind>.hz). */
	static float Hz(EWRWork Kind);
	/** Where in the cycle the blow lands (0..1): the beat's sound and sparks play there. */
	static float StrikeAt(EWRWork Kind);
};
