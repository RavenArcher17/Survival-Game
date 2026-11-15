// The Desert in the world (cooks/biome-desert.md, cooks/build-biome-desert.md 2.2-2.7): every machine builds the
// desert's places in code from Data/desert.json once the land's ground is up (AWRDesertScenery on the forest's FBuild
// kit); the server adds what you take or use (nodes, caches, spots) and the Desert's replicated state.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "Components/ActorComponent.h"
#include "WRForestWorld.h"
#include "WRDesertWorld.generated.h"

class AWRPlayerController;
class AWRCharacterBase;
class AWRResourceNode;
class AWRWorldCache;

/**
 * The Desert's replicated state (one per world, server-spawned): the Lee Door's sand (open for 3 game hours after every
 * Sandstorm block), the Dig cleared at the Lost Expedition, the Mirror Gallery's four mirrors and the Sun-Vault's lock,
 * the Lookout's signal. Everything that moves on clients is a function of these and the server clock (it never snaps).
 */
UCLASS(NotPlaceable)
class WILDREACH_API AWRDesertState : public AActor
{
	GENERATED_BODY()

public:
	AWRDesertState();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	static AWRDesertState* Get(const UObject* WorldContext);

	/** Game hours (total) until which the Lee Door stands clear of sand; 0 = buried. */
	UPROPERTY(Replicated) float LeeOpenUntil = 0.f;
	/** The server time the Lee Door's sand last started to slump away or pour back (the sand's ease). */
	UPROPERTY(Replicated) float LeeChangedAt = -100.f;
	/** H-E3: the pit cleared (once for the server, then it stays open). */
	UPROPERTY(Replicated) bool bDigDone = false;
	UPROPERTY(Replicated) float DigDoneAt = -100.f;
	/** The Mirror Gallery: each mirror's step (x 45 degrees), the step it turned from and when (server time). */
	UPROPERTY(Replicated) TArray<uint8> MirrorStep;
	UPROPERTY(Replicated) TArray<uint8> MirrorFrom;
	UPROPERTY(Replicated) TArray<float> MirrorAt;
	/** The Sun-Vault's door slid open (the gallery's beam reached its lock). */
	UPROPERTY(Replicated) bool bVaultOpen = false;
	UPROPERTY(Replicated) float VaultOpenAt = -100.f;
	/** The Lookout's bronze signal mirror flashed Saltgate (secret X3) today: the server time of the flash. */
	UPROPERTY(Replicated) float SignalAt = -100.f;

	/** Server: the gallery's beam reaches the vault's lock (every mirror on its right step). */
	bool GalleryAligned() const;
	/** A mirror's angle (degrees) now, eased over 0.6 s from its last turn. */
	float MirrorAngle(int32 Index, float Now) const;
};

/** One place of the Desert, modelled in code at real scale: a landmark, a hand site, a kit POI, a cave, an Hourhold
 *  room, a decoration cell. Parts that move (the qanat tower's bucket wheel, the Lookout's signal mirror, the Wall's
 *  scrolling dust, the gallery's mirrors, the Lee Door's sand bank, the Dig's sand, the Hall of Hours' light shafts) are
 *  turned from the clock and the replicated state on every machine: they glide, never snap. */
UCLASS(NotPlaceable)
class WILDREACH_API AWRDesertScenery : public AWRForestScenery
{
	GENERATED_BODY()

public:
	AWRDesertScenery();
	virtual void Tick(float DeltaSeconds) override;
	int32 Index = -1;
	FName Zone;
	void BuildDesert();
	/** The qanat tower's bucket wheel (6 rpm about its axle, the pots' chain going round with it). */
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Wheel;
	/** The Lookout's signal mirror on its pivot (tracks the sun's bearing across the day). */
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Signal;
	/** The Haboob Wall's rows of dust (each slides along the wall at its own speed and wraps). */
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> Dust;
	TArray<float> DustSpeed;
	float DustSpan = 0.f;
	/** The Mirror Gallery's mirrors (yaw on their bronze pivots). */
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> Mirrors;
	/** The Sun-Vault's sliding stone door; the gallery's beam (a lit bar from mirror to mirror). */
	UPROPERTY(Transient) TObjectPtr<USceneComponent> VaultDoor;
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> Beam;
	/** The Lee Door's sand bank, the Dig pit's sand (they sink away when opened). */
	UPROPERTY(Transient) TObjectPtr<USceneComponent> LeeSand;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> DigSand;
	/** The Hall of Hours' shafts of light from its roof holes (aimed down the real sun's direction). */
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> Shafts;
	/** A dust devil's column (spins about its own axis: the hazards move the whole actor). */
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Spin;
	float MoveClock = 0.f;
};

/**
 * Something in the desert you hold E at (cooks/biome-desert.md 2.4, 2.5, 2.11): a lore keepsake, a site's take, the
 * lens holders, the Hands Wall's rubbing, the Mirror Gallery's mirrors, the Dig, a run-back brazier, a Wellfrog to squeeze,
 * a tilted stela's cache (the Long Noon's), a dig mound a Scarab digger found. The server runs the hold (Seconds, stand
 * still within reach) and pays.
 */
UCLASS(NotPlaceable)
class WILDREACH_API AWRDesertSpot : public AActor
{
	GENERATED_BODY()

public:
	AWRDesertSpot();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(Replicated) FName Kind;
	UPROPERTY(Replicated) FName SpotId;
	UPROPERTY(Replicated) FString Title;
	UPROPERTY(Replicated) FString Verb;
	UPROPERTY(Replicated) float Seconds = 0.f;
	UPROPERTY(Replicated) float DoneAt = 0.f;
	UPROPERTY(Replicated) FString Doer;
	/** Hidden (a tilted stela's cache outside the Long Noon, a dig mound nobody has found): no prompt. */
	UPROPERTY(Replicated) bool bConcealed = false;
	float Reach = 300.f;

	// Server.
	TMap<FName, int32> Gives;
	FString Lore;
	float RespawnHours = 0.f;       // 0 = once per survivor
	TMap<FString, double> DoneBy;   // survivor -> game hours
	int32 Mirror = INDEX_NONE;      // the gallery's mirror index
	TWeakObjectPtr<AWRCharacterBase> DoerPawn;
	FVector DoerFrom = FVector::ZeroVector;

	FString Start(AWRPlayerController* PC);
	bool IsDoneBy(const FString& Player, double NowHours) const;

private:
	void Finish();
};

/** Client-only, on a sunstone node or outcrop: its stored light at night (KEPT: brightness = hours of sun that day / 14),
 *  a small amber light and the crystals' glow, off by day and underground unlit. */
UCLASS()
class WILDREACH_API UWRDesertNodeGlow : public UActorComponent
{
	GENERATED_BODY()
public:
	UWRDesertNodeGlow() { PrimaryComponentTick.bCanEverTick = true; PrimaryComponentTick.TickInterval = 2.f; }
	virtual void TickComponent(float Dt, ELevelTick T, FActorComponentTickFunction* F) override;
	TWeakObjectPtr<class UPointLightComponent> Light;
	TWeakObjectPtr<class UProceduralMeshComponent> Glow;
	float Power = 260.f;
};

UCLASS()
class WILDREACH_API UWRDesertWorld : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return bActive; }

	static UWRDesertWorld* Get(const UObject* WorldContext);
	bool IsActive() const { return bActive; }
	bool IsBuilt() const { return bSceneryDone; }

	// Interact (WRWorldNpc's hooks): the prompt, the client's E, the server's "world desert ..." command.
	static bool Prompt(const AWRPlayerController* PC, FString& OutTarget, FString& OutVerb);
	static bool ClientInteract(AWRPlayerController* PC);
	static FString Command(AWRPlayerController* PC, const TArray<FString>& Args);

	AWRDesertSpot* SpotNear(const FVector& P, float Range = 0.f) const;
	AWRDesertSpot* FindSpot(FName Id) const;
	const TArray<TWeakObjectPtr<AWRDesertScenery>>& GetScenery() const { return Scenery; }
	AWRResourceNode* SpawnNode(FName Item, const FVector& At, int32 Hits, float PerHit, float RespawnSeconds, float Yaw = -1.f);
	AWRWorldCache* SpawnCache(const FVector& At, float Yaw, const FString& Title, const TMap<FName, int32>& Items, const FString& Lore, float RefillHours);
	AWRDesertSpot* SpawnSpot(FName Kind, FName Id, const FVector& At, const FString& Title, const FString& Verb, float Seconds);
	/** Server: a sunstone node's yield multiplier now (+50% the block after a Sandstorm, x3 in the Long Noon). */
	static float SunstoneYield(const UObject* WorldContext);
	/** Every machine: hours of direct sun so far today in the desert (0..14), what a sunstone's night glow stores. */
	static float SunHoursToday(const UObject* WorldContext);

private:
	void BuildScenery();
	void StepBuild();
	void SpawnServerActors();
	void StepLights(float Dt);
	void StepServer(float Dt);

	struct FPiece { FName Kind; FName Id; FName Zone; FVector At; float Yaw; int32 Index; int32 Variant; float Size; };
	TArray<FPiece> Pieces;
	int32 PiecesBuilt = 0;
	bool bActive = false, bQueued = false, bSceneryDone = false, bServerDone = false;
	float WaitClock = 0.f, LightClock = 0.f, ServerClock = 0.f;
	TArray<TWeakObjectPtr<AWRDesertScenery>> Scenery;
	TArray<TWeakObjectPtr<AWRDesertSpot>> Spots;
	TArray<TWeakObjectPtr<AWRWorldCache>> Caches;
	TWeakObjectPtr<AWRDesertState> State;
	/** Server: the weather's last kind in the desert (to open the Lee Door when a Sandstorm block ends). */
	FName LastWeather;
};
