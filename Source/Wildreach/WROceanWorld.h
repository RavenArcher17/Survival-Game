// The Ocean & reefs in the world (cooks/biome-ocean.md, cooks/build-biome-ocean.md 2.2-2.11): every machine builds the
// coast's places in code from Data/ocean.json once the land's ground is up (AWROceanScenery on the forest's FBuild kit);
// the server adds what you take or use (nodes, caches, spots) and the Ocean's replicated state (WRTide.h).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "Components/ActorComponent.h"
#include "WRForestWorld.h"
#include "WROceanWorld.generated.h"

class AWRPlayerController;
class AWRCharacterBase;
class AWRResourceNode;
class AWRWorldCache;
class AWROceanState;
class UExponentialHeightFogComponent;
class UPostProcessComponent;

/** One place of the Ocean, modelled in code at real scale: a landmark, a hand site, a kit POI, a cave, a Drowned Hall
 *  room, a decoration cell. Parts that move (the Tide Clock's float, the drowned bell, the Low Gate's sand, the Pound's
 *  sluice leaves) are turned from the clock and the replicated state on every machine: they glide, never snap. */
UCLASS(NotPlaceable)
class WILDREACH_API AWROceanScenery : public AWRForestScenery
{
	GENERATED_BODY()

public:
	AWROceanScenery();
	virtual void Tick(float DeltaSeconds) override;
	/** The row it was built from (a cave's, a decor cell's index). */
	int32 Index = -1;
	FName Zone;
	void BuildOcean();
	/** The Tide Clock's cedar float (rides the live level in its slot). */
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Float;
	float FloatBaseZ = 0.f;
	/** The drowned bell's headstock (swings when rung). */
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Bell;
	/** The Low Gate's sand bank (slumps in the King Tide's Low). */
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Sand;
	/** The Pound's three shut sluice gates' leaves (swing out when they burst). */
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> Leaves;
	TArray<float> LeafSign;
	/** The Beacon Stack's fire (shows when your clan lit it). */
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Fire;
	float MoveClock = 0.f;
};

/**
 * Something on the coast you hold E at (cooks/biome-ocean.md 2.4, 2.5, 2.20): a lore keepsake, a site's take, the Sluice
 * key, the drowned bell's rope, the Beacon Stack's beacon, a tsunami stone to read, the Dyke Head's vault, the Horn
 * ledge's cache, the mirror line's tripods. The server runs the hold (Seconds, stand still within reach) and pays.
 */
UCLASS(NotPlaceable)
class WILDREACH_API AWROceanSpot : public AActor
{
	GENERATED_BODY()

public:
	AWROceanSpot();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(Replicated) FName Kind;
	UPROPERTY(Replicated) FName SpotId;
	UPROPERTY(Replicated) FString Title;
	UPROPERTY(Replicated) FString Verb;
	UPROPERTY(Replicated) float Seconds = 0.f;
	UPROPERTY(Replicated) float DoneAt = 0.f;
	UPROPERTY(Replicated) FString Doer;
	float Reach = 300.f;

	// Server.
	TMap<FName, int32> Gives;
	/** A secret's window (midnight_high, noon_clear, king_tide) and what it needs (an item carried, a tame near). */
	FName When, Needs;
	FString Lore;
	float RespawnHours = 0.f;       // 0 = once per survivor
	TMap<FString, double> DoneBy;   // survivor -> game hours
	TWeakObjectPtr<AWRCharacterBase> DoerPawn;
	FVector DoerFrom = FVector::ZeroVector;

	FString Start(AWRPlayerController* PC);
	bool IsDoneBy(const FString& Player, double NowHours) const;

private:
	void Finish();
};

/** Client-only, on a tidecoral node: the coral whitens 10 s after the tide leaves it in air (it "dies into stone") and comes
 *  alive again when the sea covers it (its two looks are the node mesh's two sections). */
UCLASS()
class WILDREACH_API UWROceanCoralLife : public UActorComponent
{
	GENERATED_BODY()
public:
	UWROceanCoralLife() { PrimaryComponentTick.bCanEverTick = true; PrimaryComponentTick.TickInterval = 1.f; }
	virtual void TickComponent(float Dt, ELevelTick T, FActorComponentTickFunction* F) override;
	TWeakObjectPtr<class UProceduralMeshComponent> Mesh;
	float InAir = 0.f;
	bool bDead = false;
};

UCLASS()
class WILDREACH_API UWROceanWorld : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return bActive; }

	static UWROceanWorld* Get(const UObject* WorldContext);
	bool IsActive() const { return bActive; }
	bool IsBuilt() const { return bSceneryDone; }

	// Interact (WRWorldNpc's hooks): the prompt, the client's E, the server's "world ocean ..." command.
	static bool Prompt(const AWRPlayerController* PC, FString& OutTarget, FString& OutVerb);
	static bool ClientInteract(AWRPlayerController* PC);
	static FString Command(AWRPlayerController* PC, const TArray<FString>& Args);

	AWROceanSpot* SpotNear(const FVector& P, float Range = 0.f) const;
	/** A harpoon line held by Who and a mooring stone's ring within 3 m of them: where to tie it. */
	bool TieRing(const APawn* Who, FVector& OutRing) const;
	AWROceanSpot* FindSpot(FName Id) const;
	const TArray<TWeakObjectPtr<AWROceanScenery>>& GetScenery() const { return Scenery; }
	AWRResourceNode* SpawnNode(FName Item, const FVector& At, int32 Hits, float PerHit, float RespawnSeconds, float Yaw = -1.f);
	AWRWorldCache* SpawnCache(const FVector& At, float Yaw, const FString& Title, const TMap<FName, int32>& Items, const FString& Lore, float RefillHours);
	AWROceanSpot* SpawnSpot(FName Kind, FName Id, const FVector& At, const FString& Title, const FString& Verb, float Seconds);
	/** Server: a survivor read a tsunami stone (Hae-won's fifth asks for all nine). */
	TMap<FString, TSet<int32>> StonesRead;

private:
	void BuildScenery();
	void StepBuild();
	void SpawnServerActors();
	void StepLights(float Dt);
	void StepServer(float Dt);
	/** The local camera under water: the murk by band (40 / 25 / 12 m; x0.5 in Sea fog), the blue tint (every drawing
	 *  machine; WRUnderwater's "under"). */
	void TickLocalWater(float Dt);

	struct FPiece { FName Kind; FName Id; FName Zone; FVector At; float Yaw; int32 Index; int32 Variant; float Size; };
	TArray<FPiece> Pieces;
	int32 PiecesBuilt = 0;
	bool bActive = false, bQueued = false, bSceneryDone = false, bServerDone = false;
	float WaitClock = 0.f, LightClock = 0.f, ServerClock = 0.f;
	TArray<TWeakObjectPtr<AWROceanScenery>> Scenery;
	TArray<TWeakObjectPtr<AWROceanSpot>> Spots;
	TArray<TWeakObjectPtr<AWRWorldCache>> Caches;
	TWeakObjectPtr<AWROceanState> State;
	UPROPERTY(Transient) TObjectPtr<AActor> WaterLook;
	UPROPERTY(Transient) TObjectPtr<UExponentialHeightFogComponent> UnderFog;
	UPROPERTY(Transient) TObjectPtr<UPostProcessComponent> UnderPost;
	float UnderBlend = 0.f;
};
