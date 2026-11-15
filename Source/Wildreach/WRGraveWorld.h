#pragma once

// The Graveyard on the map (cooks/biome-graveyard.md 2.1-2.7, 2.11, 2.19, 2.20; cooks/build-biome-graveyard.md 2.3-2.9):
// everything the game builds on the Graveyard's land at runtime from Data/graveyard.json (Tools/terrain/graveyard.mjs,
// parsed by FWRGraveData in WRGraveTypes.cpp). Static scenery (the landmarks, the hand sites, the kit POIs, the grave
// fields, the lantern roads, the trees, the stream, the Lantern Stair, the catacombs' rooms and galleries) is built on
// every machine with no network cost, like the forest's (WRGraveScenery*.cpp, WRGraveCatacombs.cpp). Things with state
// are the server's and replicate: the gathering (soulstone, bone, yew, limestone, mortsafes for grave iron, silver,
// wax, star iron, black rye, asphodel), the caches, the spots you use (keepsakes, digs, the Stair's unlit lantern, the
// seventh name, the drowned bell, the Row of the Named's tōrō), and the Graveyard's state (the Stair's gold, the chapel
// crypt's stair).

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRForestWorld.h"
#include "WRGraveWorld.generated.h"

class AWRPlayerController;
class AWRPlayerState;
class AWRCharacterBase;
class AWRResourceNode;
class AWRWorldCache;
class UPointLightComponent;

/** One place of the Graveyard, modelled in code at real scale on the forest's FBuild (the scenery kit is shared:
 *  WRGraveLook.h). Kind: a site's or kit row's kind, or field / road / trees / stream / stair / room / link. */
UCLASS(NotPlaceable)
class WILDREACH_API AWRGraveScenery : public AWRForestScenery
{
	GENERATED_BODY()

public:
	/** The row it was built from (a field, a road, a tree cell, a room or a link index). */
	int32 Index = -1;
	void BuildGrave();
	/** Lamps the world lights only near the viewer (the lantern roads' tōrō: a light budget every machine keeps). */
	UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> NearLamps;
	TArray<float> NearPower;
	/** A second night look (the Lantern Stair's gold, S3): shown instead of NightRoot while the state says so. */
	UPROPERTY(Transient) TObjectPtr<USceneComponent> AltNightRoot;
	/** A part that changes with the Graveyard's state (StateFlag: seventh / crypt): StateOn shows (and StateBox stops
	 *  blocking) once the flag is set, StateOff shows before. Every machine, collision included. */
	FName StateFlag;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> StateOn;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> StateOff;
	UPROPERTY(Transient) TObjectPtr<class UBoxComponent> StateBox;
	bool bStateShown = false;
	void ApplyState(bool bOn);
};

/** The Graveyard's world state every machine draws (late joiners get it): the Lantern Stair gone gold (secret S3), the
 *  chapel crypt's stair open (after Nhal's first kill), the seventh crypt open (S4). */
UCLASS(NotPlaceable)
class WILDREACH_API AWRGraveState : public AActor
{
	GENERATED_BODY()

public:
	AWRGraveState();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	static AWRGraveState* Get(const UObject* WorldContext);

	/** Game hours (AWRGameState::GetTotalHours) until which the Stair's tōrō burn gold and corruption doesn't build on it. */
	UPROPERTY(ReplicatedUsing=OnRep_State) float StairGoldUntilHours = -1.f;
	UPROPERTY(ReplicatedUsing=OnRep_State) bool bChapelCryptOpen = false;
	UPROPERTY(ReplicatedUsing=OnRep_State) bool bSeventhOpen = false;
	/** Every machine: the Stair burns gold now. */
	bool StairGold() const;

	UFUNCTION() void OnRep_State();
};

/** A thing of the Graveyard you use with E (held for Seconds, standing within Reach). Kinds:
 *   keepsake  a lore item set down at its story site: each survivor takes one (GY 2.20)
 *   dig       a secret grave or a cenotaph: a pick in hand, dig Seconds; each survivor finds its loot once (S1, S2, H2)
 *   lantern   a tōrō to light with a flame in hand (S3 at the Stair's break; Morwen's first quest on the Row of the Named)
 *   carve     the seventh name: the Mourners' chisel in the bag, carve Seconds; opens the seventh crypt (S4)
 *   chamber   the drowned bell's air pocket: what lies in it, once each (S5) */
UCLASS(NotPlaceable)
class WILDREACH_API AWRGraveSpot : public AActor
{
	GENERATED_BODY()

public:
	AWRGraveSpot();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(Replicated) FName Kind;
	UPROPERTY(Replicated) FName SpotId;
	UPROPERTY(Replicated) FString Title;
	UPROPERTY(Replicated) FString Verb;
	UPROPERTY(Replicated) float Seconds = 0.f;
	/** A held action under way: the server time it finishes (0: none) and who holds it. */
	UPROPERTY(Replicated) float DoneAt = 0.f;
	UPROPERTY(Replicated) FString Doer;
	/** Lit (a lantern spot), for every machine's look. */
	UPROPERTY(ReplicatedUsing=OnRep_Lit) bool bLit = false;
	float Reach = 260.f;

	// Server.
	TMap<FName, int32> Gives;
	FName Needs;          // an item that must be in the bag (carve: the chisel)
	FName Tool;           // pick: a pick in hand
	FName Quest;          // a quest event this fires (event kind, this id)
	int32 Xp = 0;         // path XP, once each (S3)
	bool bNightOnly = false;
	bool bFlame = false;  // a lit torch or lantern held
	TSet<FString> DoneBy;
	TWeakObjectPtr<AWRCharacterBase> DoerPawn;
	FVector DoerFrom = FVector::ZeroVector;

	/** Server: E pressed here. What the player is told. */
	FString Start(AWRPlayerController* PC);
	/** Whether this survivor has had what it gives (each finds it once). */
	bool IsDoneBy(const FString& Player) const { return DoneBy.Contains(Player); }

private:
	void Finish();
	UFUNCTION() void OnRep_Lit();
	void BuildLook();
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> Light;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Glint;
	bool bLookBuilt = false;
};

/** Runs the Graveyard's world: builds it, refills its caches, keeps its night lights within budget, answers its spots. */
UCLASS()
class WILDREACH_API UWRGraveWorld : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UWRGraveWorld* Get(const UObject* WorldContext);
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UWRGraveWorld, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return bActive; }
	bool bActive = false;

	/** The interact prompt for a Graveyard spot in reach (WRWorldNpc). */
	static bool Prompt(const AWRPlayerController* PC, FString& OutTarget, FString& OutVerb);
	/** Client: E on a Graveyard spot sends its command. */
	static bool ClientInteract(AWRPlayerController* PC);
	/** Server: "world grave <verb> ..." (WRWorldSystems). */
	static FString Command(AWRPlayerController* PC, const TArray<FString>& Args);
	/** Server: using a Graveyard item from the bag. Empty when it isn't one of the Graveyard's. */
	static FString UseItem(AWRPlayerState* PS, FName ItemId);

	/** The scenery is built in this world. */
	bool IsBuilt() const { return bActive && bSceneryDone; }
	/** The spot nearest At within its reach (any machine: spots replicate). */
	AWRGraveSpot* SpotNear(const FVector& At) const;
	/** Server: Nhal fell for the first time on this server: the chapel crypt's stair opens (GY 2.5 C12). */
	void OpenChapelCrypt();

private:
	struct FPiece
	{
		FName Kind, Id;
		FVector At = FVector::ZeroVector;
		float Yaw = 0.f;
		int32 Index = -1, Variant = 0;
	};
	void BuildScenery();
	void StepBuild();
	void SpawnServerActors();
	AWRResourceNode* SpawnNode(FName Item, const FVector& At, int32 Hits, float PerHit, float RespawnSeconds, float Yaw = -1.f);
	AWRWorldCache* SpawnCache(const FVector& At, float Yaw, const FString& Title, const TMap<FName, int32>& Items, const FString& Lore, float RefillHours);
	AWRGraveSpot* SpawnSpot(FName Kind, FName Id, const FVector& At, const FString& Title, const FString& Verb, float Seconds);
	void StepLights(float Dt);
	void StepServer(float Dt);
	void StepState();
	void StepMorwen(float Dt);
	TArray<FPiece> Pieces;
	int32 PiecesBuilt = 0;
	bool bQueued = false;
	bool bSceneryDone = false;
	bool bServerDone = false;
	float WaitClock = 0.f;
	TArray<TWeakObjectPtr<AWRGraveScenery>> Scenery;
	TArray<TWeakObjectPtr<AWRWorldCache>> Caches;
	TArray<TWeakObjectPtr<AWRGraveSpot>> Spots;
	UPROPERTY(Transient) TObjectPtr<AWRGraveState> State;
	/** Every near lamp of every place (positions and comps), for the light budget. */
	TArray<TWeakObjectPtr<UPointLightComponent>> AllNear;
	TArray<float> AllNearPower;
	float LightClock = 0.f;
	float ServerClock = 0.f;
	float MorwenClock = 0.f;
	int32 MorwenPhase = 0;
	FVector MorwenHome = FVector::ZeroVector;
	FRotator MorwenHomeFace = FRotator::ZeroRotator;
};
