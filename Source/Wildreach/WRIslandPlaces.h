// The Fire and Frost places on the Old Island (cooks/cookpass-world.md 3.2-3.6): the ten sub-zones, the landmarks,
// story sites, the slag-cairn and inuksuk roads, the kit POIs and the micro-hazards, all from Data/island_places.json
// (Tools/terrain/island_places.mjs). Every machine builds every place itself on the ground traced under it, once that
// ground has streamed in (no replication, L_Island untouched); the server also runs the hazards (WRIslandHazards.cpp)
// and replicates the two that react to people (thin ice, snow bridges) through AWRIsleState.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRIslandPlaces.generated.h"

class FJsonObject;
class UPointLightComponent;
class UStaticMeshComponent;
class UBoxComponent;
class AWRCharacterBase;

namespace WRIsle
{
	/** The places are built in this world (the Old Island with its places file). */
	WILDREACH_API bool Live(const UObject* WorldContext);
	/** The Fire/Frost sub-zone at a world point ("V1".."V5", "T1".."T5"), or none off the island. */
	WILDREACH_API FName ZoneAt(const FVector& At);
	/** A sub-zone's name ("The Ash Plains"), its biome and its colour (sRGB from the file). */
	WILDREACH_API FString ZoneName(FName Zone);
	WILDREACH_API FName ZoneBiome(FName Zone);
	WILDREACH_API FLinearColor ZoneColour(FName Zone);
	/** A landmark's line for its discovery banner (empty: not one of ours). */
	WILDREACH_API FString LandmarkLine(const FString& Name);
	/** The server's clock as every machine reads it (the clocked hazards and the landmarks' motion run on it). */
	WILDREACH_API double Now(const UObject* WorldContext);
}

/** A hazard that reacts to people: its state as the server last set it (State 0 whole, 1 cracking, 2 broken). */
USTRUCT()
struct FWRIsleBreak
{
	GENERATED_BODY()
	UPROPERTY() int32 Key = 0;
	UPROPERTY() uint8 State = 0;
	/** Server time the state began. */
	UPROPERTY() float At = 0.f;
};

/** The server's record of the reacting hazards, replicated to every client (a handful of rows). */
UCLASS(NotPlaceable)
class WILDREACH_API AWRIsleState : public AActor
{
	GENERATED_BODY()
public:
	AWRIsleState();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UPROPERTY(ReplicatedUsing = OnRep_Breaks) TArray<FWRIsleBreak> Breaks;
	UFUNCTION() void OnRep_Breaks();
	/** Server: set a hazard's state (and tell the local machine too). */
	void Set(int32 Key, uint8 State, float At);
	const FWRIsleBreak* Find(int32 Key) const { return Breaks.FindByPredicate([Key](const FWRIsleBreak& B) { return B.Key == Key; }); }
};

/** What the places own on this machine (components only; not replicated). */
UCLASS(NotPlaceable)
class WILDREACH_API AWRIslandPlacesActor : public AActor
{
	GENERATED_BODY()
public:
	AWRIslandPlacesActor();
};

/** One hazard a place registered (the server ticks it; every machine draws its tells). Positions in world cm. */
struct FWRIsleHazard
{
	enum class EKind : uint8 { Fumarole, Geyser, Serac, Acid, ThinIce, SnowBridge };
	EKind Kind = EKind::Fumarole;
	int32 Key = 0;
	FVector At = FVector::ZeroVector;
	FVector Half = FVector(100.f);   // a bridge's box half size; a pool's / patch's radius in X
	FRotator Rot = FRotator::ZeroRotator;
	float Radius = 200.f;
	double Phase = 0.0;
	float Period = 0.f;          // a clocked hazard's own period (s), set the first time it's stepped
	/** Looks that change with the state (whole / broken), and the collision that goes with "whole". */
	TArray<TWeakObjectPtr<USceneComponent>> Whole, Broken;
	TArray<TWeakObjectPtr<UBoxComponent>> Solid, RimSolid;
	/** Server: who was hit in this burst, and the state machine. */
	int64 LastCycle = -1;
	TSet<TWeakObjectPtr<AActor>> HitThisCycle;
	uint8 State = 0;
	double StateAt = 0.0;
	double WeightSince = -1.0;
	/** Client: the last tell / burst drawn, and the crack sounds already played. */
	int64 DrawnTell = -1, DrawnBurst = -1;
	int32 CracksPlayed = 0;
	uint8 ShownState = 0;
};

/** One thing on a place that moves or lights with the clock (client looks). */
struct FWRIsleAnim
{
	enum class EKind : uint8 { NightLight, Breath, PassCup, ShrineLight, NightFx, Glowmoth };
	EKind Kind = EKind::NightLight;
	TWeakObjectPtr<UPointLightComponent> Light;
	TWeakObjectPtr<USceneComponent> Fx;
	FVector At = FVector::ZeroVector;
	float Base = 1000.f;
	float Period = 6.f, Phase = 0.f, Length = 2.4f;
	FName Name;                 // a shrine id (ShrineLight), an fx row (NightFx / Breath)
	double LitUntil = -1.0;     // a cup's local burn (PassCup)
	int64 LastPuff = -1;
};

UCLASS()
class WILDREACH_API UWRIslandPlaces : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return bLive; }
	static UWRIslandPlaces* Get(const UObject* WorldContext);

	bool IsLive() const { return bLive; }
	AWRIslandPlacesActor* GetOwnerActor() const { return Owner.Get(); }
	AWRIsleState* GetState() const { return State.Get(); }
	void SetState(AWRIsleState* InState) { State = InState; }
	/** A hazard's replicated state changed (the server's own set, or a client's OnRep). */
	void OnBreak(const FWRIsleBreak& B);

	TArray<FWRIsleHazard> Hazards;
	TArray<FWRIsleAnim> Anims;
	/** How many places were built and how many still wait for their ground. */
	int32 Built = 0;
	int32 PendingCount() const { return Pending.Num(); }

private:
	struct FPending
	{
		FString Kind;
		int32 Index = 0;            // the stable key base (row order in the file)
		FVector2D At;               // cm
		float Yaw = 0.f;
		bool bLandmark = false;
		TSharedPtr<FJsonObject> Row;
		int32 Variant = 0;
	};
	void Load();
	bool BuildOne(const FPending& P);
	void TickLooks(float Dt);
	void StepServer(float Dt);
	void StepClient(float Dt);
	void ApplyShown(FWRIsleHazard& H, uint8 NewState);

	bool bLive = false;
	bool bServer = false, bDraw = false;
	bool bPoisAdded = false;
	TArray<FPending> Pending;
	TWeakObjectPtr<AWRIslandPlacesActor> Owner;
	TWeakObjectPtr<AWRIsleState> State;
	float ServerAccum = 0.f, LookAccum = 0.f;
	/** Places still waiting for their ground are tried again after this (s), not every frame. */
	float RetryIn = 0.f;
	/** The katabatic ring round the glacier bowl (cm) and the last gust each side handled / drew. */
	FVector2D GustCentre = FVector2D::ZeroVector;
	float GustInner = 0.f, GustOuter = 0.f;
	int64 GustDone = -1, GustDrawn = -1;
	/** Client: the thin-ice patch the local survivor last stood on (its creak plays once per step on). */
	int32 IceUnderMe = INDEX_NONE;
};
