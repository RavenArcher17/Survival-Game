#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRMachineTypes.h"
#include "WRMachine.generated.h"

class AWRBuildPiece;
class AWRPlayerController;
class AWRStation;
class FJsonObject;
class UWRInventoryComponent;
struct FWRPieceDef;
struct FWRItemStack;

/**
 * Machinery and power (cooks/build-machinery.md; survival-crafting 2.15). Power P is rotation: every source ends in a
 * turning output stub (a port), every machine takes one, and pieces whose ports meet share a network. A network runs at
 * min(1, supply / load); load counts machines switched on with work. Everything runs on the server, only while a
 * player is near; every machine draws it from the piece's replicated FWRMachineRep (WRMachineLook).
 */

/** A machine's row in Data/machines.json. */
struct FWRMachineDef
{
	FName Piece;
	FName Role;         // source, shaft, gearbox, processor, press, pump, conveyor, sorter, lamp, growlight, drill, coldstore, lift, pylon, still, vat, riftchest
	FName Flow;         // a source's power: water, wind, lava, soul, storm, sun, tide
	float Supply = 0.f;
	float SupplyHigh = 0.f;
	float Load = 0.f;
	float Rpm = 30.f;
	FName Station;      // processors: the recipes they make (recipes.json station)
	int32 In = 0, Out = 0;
	float Length = 200.f;   // conveyors
	FName NeedsStat;    // a life-skill stat its builder needs (Long lines)
	FName FreeIn;       // a biome where it needs no power (the cold store in the tundra)
	FName Input, Output;
	TArray<FName> Outputs;

	bool IsSource() const { return Role == TEXT("source"); }
	bool Carries() const { return Role == TEXT("shaft") || Role == TEXT("gearbox") || Role == TEXT("conveyor"); }
	bool IsProcessor() const { return Role == TEXT("processor") || Role == TEXT("press"); }
	/** Takes part in a power network (has a port). */
	bool IsPowered() const { return Role != TEXT("lift") && Role != TEXT("riftchest") && Role != TEXT("still") && Role != TEXT("vat") && Role != TEXT("press") && Role != TEXT("pylon"); }
};

class WILDREACH_API FWRMachineDefs
{
public:
	static FWRMachineDefs& Get();
	const FWRMachineDef* Find(FName Piece) const { return Defs.Find(Piece); }
	/** machines.json rules.<Path> (e.g. "wind.storm"). */
	float Rule(const TCHAR* Path, float Fallback) const;
	FString RuleStr(const TCHAR* Path, const FString& Fallback) const;

private:
	FWRMachineDefs();
	TMap<FName, FWRMachineDef> Defs;
	TSharedPtr<FJsonObject> Rules;
};

/** A machine's server-only state (progress, batteries, fuel, clocks): saved with its piece. */
struct FWRMachineState
{
	bool bOn = true;
	bool bOverclock = false;
	float Progress = 0.f;       // seconds into the current item / ore / brew
	FName Recipe;
	float Charge = 0.f;         // battery, P-hours
	float FuelLeft = 0.f;       // seconds left of the burning soulstone
	float RunSeconds = 0.f;     // running time toward its owner's Engineering XP
	float BeltClock = 0.f;      // conveyors: seconds to the next pull; sorting chests: to the next send
	float LastSpeed = 0.f;      // stations: the power boost their queue was last timed at
	int32 VatCount = 0;         // mycelium vat: grown since its last compost
	bool bVatLions = false;
	FVector LinkTo = FVector::ZeroVector;   // an updraft lift's partner (zero: none)
	float BlockedCheck = 0.f;   // windmill clearance: seconds to the next look round
	bool bBlocked = false;
	bool bOwnerEfficient = false, bOwnerSelfRepair = false, bOwnerChief = false;
	float LiveP = 0.f;          // a storm coil / sun array: what the sky gives it this step (the battery banks the rest)
	float OwnerSpeed = 0.f;     // the owner's machineSpeed when last seen online
};

/** One port: where a stub ends, in world space, and the axis it turns about. */
struct FWRPort
{
	FVector At;
	FVector Axis;
};

/** Machine rules every machine (and the server) shares: ports, placement, what E does, what it says. */
struct WILDREACH_API FWRMachines
{
	static bool IsMachine(const FWRPieceDef& Def);
	static const FWRMachineDef* DefOf(const AWRBuildPiece* Piece);
	/** Its ports in world space (a water wheel's hangs at its axle's height over the water). */
	static TArray<FWRPort> Ports(const AWRBuildPiece* Piece);
	/** The same for a piece that isn't placed yet (the ghost), from its def and transform. */
	static TArray<FWRPort> PortsAt(const UObject* WorldContext, const FWRPieceDef& Def, const FTransform& T);
	/** The water wheel / tide mill: its axle's height over the piece (water surface - piece + radius - dip). */
	static float HubZ(const UObject* WorldContext, const FWRPieceDef& Def, const FTransform& T);
	/** Why it can't go here (empty: it can): its level (a Hearth of yours anywhere), its biome, water, wind, lava, ore. */
	static FString PlaceCheck(const UObject* WorldContext, const FWRPieceDef& Def, const FVector& Location, float Yaw, const FString& Builder, const AActor* BuilderActor);
	/** Building ghost: a shaft or belt's end snaps onto a free port near it (location, height and line). */
	static bool SnapPlacement(const UObject* WorldContext, const FWRPieceDef& Def, FVector& InOutLoc, float& InOutYaw);
	/** Highest tech level of a Hearth this player (or their clan) owns anywhere. */
	static int32 OwnedHearthLevel(const UObject* WorldContext, const FString& Player);
	/** Building costs: planks pay for wood one for one. How many of Item the bag can pay with; take it (planks first). */
	static int32 CanPay(const UWRInventoryComponent* Inv, FName Item);
	static void Pay(UWRInventoryComponent* Inv, FName Item, int32 Count);
	/** The prompt's second line and the screen's state line. */
	static FString StateText(const AWRBuildPiece* Piece);
	static FString StateWord(EWRMachineState State);
	/** Server: E on a machine (opens its screen on the client; here: a lift calls / sends its platform). */
	static FString Interact(AWRBuildPiece* Piece, AWRPlayerController* Player);
	/** Server: the machine screen's commands (mon, moverclock, mput, mtake, mtakeall, mrepair, mlink, mcall). */
	static bool RunCommand(AWRPlayerController* Player, const TArray<FString>& Args, FString& OutResult);
	/** Save / load its state (into its piece's JSON). */
	static void ToJson(const AWRBuildPiece* Piece, TSharedRef<FJsonObject> O);
	static void FromJson(AWRBuildPiece* Piece, const TSharedPtr<FJsonObject>& O);
	/** Repair: what it costs now (half its build cost scaled by what's missing). */
	static TArray<TPair<FName, int32>> RepairCost(const AWRBuildPiece* Piece);
	/** An updraft lift's platform at a server time: where it is between its pad (0) and its partner (1), eased. */
	static float LiftAlpha(const FWRMachineRep& Rep, float ServerTime);
	/** Wind (every machine, one shared clock): its strength 0..1.2-ish factor and the way it blows (yaw degrees). */
	static float WindAt(const UObject* WorldContext, const FVector& At, float& OutYaw);
	/** The water a wheel stands in moves (the surf line, or a river): 0 = still or none. */
	static float FlowAt(const UObject* WorldContext, const FVector& WheelAt, bool bTide);
	/** Lava within Reach cm of a point (the volcanic flow). */
	static bool LavaNear(const FVector& At, float Reach);
};

/**
 * Server: every machine piece, their networks, and the step that runs them (sources, processors, belts, sorting,
 * drills, pumps, lamps, biome machines, wear, Engineering XP). Networks are rebuilt when a piece comes or goes.
 */
UCLASS()
class WILDREACH_API UWRMachineSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UWRMachineSubsystem* Get(const UObject* WorldContext);
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return !IsTemplate(); }

	void Register(AWRBuildPiece* Piece);
	void Unregister(AWRBuildPiece* Piece);
	/** A station's piece joined a network through a shaft (WRCrafting): how much faster its orders run now (1: unpowered). */
	float StationBoost(const AWRBuildPiece* StationPiece) const;
	/** Is a running pump's trough here (a channel source, WRFarm)? Points of every running pump's trough. */
	TArray<FVector> PumpOutlets() const;
	/** Greenhouse lights running within Reach of a point (WRFarm). */
	bool GrowLightNear(const FVector& At, float Reach) const;
	/** A running cold store whose room holds this container (WRFarm's spoilage). */
	bool ColdStoreFor(const AWRBuildPiece* Container) const;
	/** Rift chests: the shared slots of this piece's group changed; copy them to every chest of its group. */
	void SyncRift(AWRBuildPiece* Changed);
	/** Another rift chest of this one's clan (or owner) still stands (breaking one then spills nothing). */
	bool HasOtherRift(const AWRBuildPiece* Piece) const;
	/** A machine was changed by a command (on/off, items in): look again now. */
	void Touch() { bDirty = true; }
	/** Updraft lifts: Link (the first marks the lift, the second links the pair) and Send (its platform goes over). */
	FString LinkLift(AWRBuildPiece* Piece, AWRPlayerController* Player);
	FString SendLift(AWRBuildPiece* Piece);

private:
	struct FNet
	{
		TArray<TWeakObjectPtr<AWRBuildPiece>> Pieces;
		TArray<TWeakObjectPtr<AWRBuildPiece>> Stations;
		float Supply = 0.f, Load = 0.f, Speed = 0.f;
		int32 Sources = 0, Machines = 0;
		bool bAwake = false;
		bool bWasRunning = false;
		FString Owner;   // the network's first source's owner (chain limits)
	};
	TArray<TWeakObjectPtr<AWRBuildPiece>> Pieces;
	TArray<FNet> Nets;
	TMap<TWeakObjectPtr<const AWRBuildPiece>, int32> NetOf;
	/** Stations a shaft runs into: their piece -> network. */
	TMap<TWeakObjectPtr<const AWRBuildPiece>, int32> StationNet;
	/** What each belt's head gives to and its tail takes from; the vein under each drill; pumps with water. */
	TMap<TWeakObjectPtr<AWRBuildPiece>, TWeakObjectPtr<AWRBuildPiece>> Heads;
	TMap<TWeakObjectPtr<AWRBuildPiece>, TWeakObjectPtr<AWRBuildPiece>> Tails;
	TMap<TWeakObjectPtr<AWRBuildPiece>, TWeakObjectPtr<class AWRResourceNode>> Veins;
	TMap<TWeakObjectPtr<AWRBuildPiece>, bool> PumpWater;
	/** Stations on a line this step: how much faster they run. */
	TMap<TWeakObjectPtr<const AWRBuildPiece>, float> StationBoosts;
	/** A player's lift waiting for its partner (Link pressed on one). */
	TMap<FString, TWeakObjectPtr<AWRBuildPiece>> PendingLink;
	/** Engineering XP: machines counted per owner in the current 5-minute window. */
	struct FRunWindow { double Window = -1.0; int32 Count = 0; };
	TMap<FString, FRunWindow> RunWindows;
	bool bDirty = true;
	float StepClock = 0.f;
	float RebuildClock = 0.f;

	void Rebuild();
	void Step(float Dt);
	void StepSource(AWRBuildPiece* P, const FWRMachineDef& D, float Dt, float& OutSupply, EWRMachineState& OutState, float& OutFactor);
	/** Load it asks for this step (0: idle) and the state it would be in with power. */
	float Demand(AWRBuildPiece* P, const FWRMachineDef& D, EWRMachineState& OutState);
	void StepLoad(AWRBuildPiece* P, const FWRMachineDef& D, float Speed, float Dt);
	void StepProcessor(AWRBuildPiece* P, const FWRMachineDef& D, float Rate, float Dt);
	void StepConveyor(AWRBuildPiece* P, const FWRMachineDef& D, float Rate, float Dt);
	void StepSorter(AWRBuildPiece* P, const FWRMachineDef& D, float Rate, float Dt);
	void StepDrill(AWRBuildPiece* P, const FWRMachineDef& D, float Rate, float Dt);
	void StepUnpowered(AWRBuildPiece* P, const FWRMachineDef& D, float Dt);
	void StepLift(AWRBuildPiece* P, float Dt);
	/** A processor's next recipe from its input slots (and whether there's room for what it makes). */
	const struct FWRRecipe* PickRecipe(AWRBuildPiece* P, const FWRMachineDef& D, bool& bOutFull) const;
	bool IsAwake(const FVector& At) const;
	/** Pieces whose bounds (or a conveyor's tail) are within Reach of a world point (conveyor hand-off). */
	AWRBuildPiece* ContainerAt(const FVector& At, float Reach, const AWRBuildPiece* Ignore) const;
	/** The machines counted along the belts that feed P (chains). */
	int32 ChainDepth(const AWRBuildPiece* P, int32 Guard = 0) const;
	void AwardRun(AWRBuildPiece* P, float Dt);
	void RefreshOwnerPerks(AWRBuildPiece* P) const;
};
