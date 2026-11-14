#pragma once
// Raids for 6 to 10 (DD; biome-void 2.23.2; cooks/endgame.md 2.9; cooks/build-endgame.md 2.10): a war-hall under each
// pass, two single-element champions who are each other's enemies as well as yours. The gate stands at the pass (the
// pass fort's undercroft door, when the forts exist); the hall is built at runtime in hall space (Data/raids.json
// hallSpace), like the portal bays. One group at a time: the first raider's party may follow until the first of them
// crosses the inner threshold; then an iron grate drops behind them and only that group comes back in (after a death,
// from the run-back stone by the gate). The grate rises when both champions are dead (lootSeconds to gather) or when
// no raider has been inside for emptySeconds (the champions reset; bags left inside go to the gate). Champions rise
// again raidRespawnHours after a win (server setting; saved in the world document "warhalls").
//
// The Thaw Hall (Fire / Frost, under the North pass): 112 x 72 m of 4 m basalt flags over a lava sump that shows in two
// gutters; the Fallen Smith's slams turn flags to lava, the Burnt Wyrm's sweeps and breath to ice, and a flag turned
// from one to the other within thawWindow bursts in steam (the Thaw) and hurts everything on it, champions too. Lava
// burns; ice is slick. The Smith's slams melt the Wyrm's footing (it slips: stance taken x1.5); the Wyrm's breath
// hardens the Smith's seams (he slows). A champion that drives its rival under triumphAt gains Triumph.
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "WRWarHall.generated.h"

class AWRPlayerController;
class AWRBoss;
class AWRCharacterBase;
class AWRRunbackStone;
class UProceduralMeshComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
struct FWRHit;

UENUM()
enum class EWRHallState : uint8
{
	Ready,      // champions waiting, nobody inside
	Open,       // a group is coming in (not sealed yet)
	Sealed,     // the grate is down: the fight
	Won,        // both champions dead: loot time, then they rest
	Resting,    // until they rise again
};

/** The war-hall's door at the pass: its screen, its state, the group. */
UCLASS(NotPlaceable)
class WILDREACH_API AWRWarHallGate : public AActor
{
	GENERATED_BODY()
public:
	AWRWarHallGate();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	UPROPERTY(ReplicatedUsing=OnRep_Look) FName HallId;
	UPROPERTY(ReplicatedUsing=OnRep_State) EWRHallState State = EWRHallState::Ready;
	/** Server time the state began (the gate card's "6:40"), the rest's end in unix seconds (Resting). */
	UPROPERTY(Replicated) float StateSince = 0.f;
	UPROPERTY(Replicated) double RestUntil = 0.0;
	/** The group inside (sealed or coming in) and whose party it is. */
	UPROPERTY(Replicated) TArray<FString> Group;
	UPROPERTY(Replicated) FString Leader;
	/** How many survivors are inside now. */
	UPROPERTY(Replicated) int32 Inside = 0;
	FVector FrontPoint() const;
private:
	UFUNCTION() void OnRep_Look();
	UFUNCTION() void OnRep_State();
	void BuildLook();
	bool bBuilt = false;
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> Leaves;
	float LeafOpen = 0.f;
};

/** A war-hall itself: its floor of flags (their state replicates), walls, vault, the grate, the vestibule. */
UCLASS(NotPlaceable)
class WILDREACH_API AWRWarHall : public AActor
{
	GENERATED_BODY()
public:
	AWRWarHall();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	UPROPERTY(ReplicatedUsing=OnRep_Look) FName HallId;
	/** One byte per flag: 0 basalt, 1 lava, 2 ice. */
	UPROPERTY(ReplicatedUsing=OnRep_Tiles) TArray<uint8> Tiles;
	/** The grate: server time it started down (sealed) or up, and which way (every machine moves it from these). */
	UPROPERTY(Replicated) float GrateAt = -100.f;
	UPROPERTY(Replicated) bool bGrateDown = false;

	int32 NX = 28, NY = 18;
	float Tile = 400.f;
	float HalfX = 5600.f, HalfY = 3600.f;
	/** Local (hall space, origin = floor centre) to world. */
	FVector Local(float X, float Y, float Z = 0.f) const { return GetActorLocation() + FVector(X, Y, Z); }
	bool Contains(const FVector& World, float Grow = 0.f) const;
	bool InVestibule(const FVector& World) const;
	int32 TileAt(const FVector& World) const;
	FVector TileCentre(int32 Index) const;
	FVector EntryPoint() const { return Local(0.f, -HalfY - 700.f, 0.f); }
	FVector ReentryPoint() const { return Local(0.f, -HalfY + 500.f, 0.f); }
	FVector ExitArch() const { return Local(0.f, -HalfY - 1000.f, 0.f); }

	// --- Server ---
	/** Turn every flag whose centre is inside the shape (a circle; or a cone from Origin along Dir, half-angle deg). */
	void Flip(const FVector& Centre, float Radius, uint8 NewState, AActor* By, float ConeHalfAngle = 0.f, const FVector& Dir = FVector::ZeroVector);
	void FlipTile(int32 Index, uint8 NewState, AActor* By);
	void Thaw(int32 Index);
	TArray<double> FlippedAt;
	/** Who one blow's Thaws have hurt already (each body once a blow). */
	TSet<TWeakObjectPtr<AActor>> ThawHit;
	bool bInFlip = false;
	TArray<double> RevertAt;
	TWeakObjectPtr<AWRWarHallGate> Gate;
	TWeakObjectPtr<AWRRunbackStone> Runback;
	TArray<TWeakObjectPtr<AWRBoss>> Champions;
	double EmptySince = -1.0;
	double LootUntil = 0.0;
	double FightStart = 0.0;
	bool bTriumphSeen = false;
	float HazardClock = 0.f;
	void SpawnChampions();
	void SetState(EWRHallState S);
	void ResetHall();
	void Won();
	void Clear(bool bToGate);
	/** Redraw the flags here (a listen server's own screen; clients redraw on replication). */
	void RedrawTiles() { if (GetNetMode() != NM_DedicatedServer) OnRep_Tiles(); }
	/** The fight's damage by survivor and its group size, kept as each champion falls (the raid board's entry). */
	TMap<FString, float> RaidLedger;
	int32 RaidSize = 0;

private:
	UFUNCTION() void OnRep_Look();
	UFUNCTION() void OnRep_Tiles();
	void BuildLook();
	void DrawTiles();
	void TickGrate();
	bool bBuilt = false;
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> LavaMesh;
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> IceMesh;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> GrateRoot;
	TArray<uint8> Drawn;
};

/** A raid champion's place in its hall (server, on the boss): its rival, Triumph, slips and hardened seams. */
UCLASS()
class WILDREACH_API UWRChampionBrain : public UActorComponent
{
	GENERATED_BODY()
public:
	UWRChampionBrain();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	TWeakObjectPtr<AWRWarHall> Hall;
	TWeakObjectPtr<AWRBoss> Rival;
	bool bTriumph = false;
	double SlipUntil = 0.0;
	double HardenUntil = 0.0;
	double NextRivalAt = 0.0;
	float BaseSpeed = 0.f;
	void Reset();
};

namespace WRWarHall
{
	WILDREACH_API void Start(UWorld* World);
	WILDREACH_API bool IsLive(FName Raid);
	WILDREACH_API int32 LiveCount();
	WILDREACH_API FString Name(FName Raid);
	WILDREACH_API TSharedPtr<class FJsonObject> Row(FName Raid);
	/** Inside a war-hall (any machine). */
	WILDREACH_API AWRWarHall* HallAt(const UObject* WorldContext, const FVector& Point);
	/** Its land: the west half the first land's, the east half the second's (None: not in a hall). */
	WILDREACH_API FName BiomeAt(const UObject* WorldContext, const FVector& Point);
	/** An ice flag under these feet (the slick rule, server and owning client alike). */
	WILDREACH_API bool IceAt(const UObject* WorldContext, const FVector& Feet);

	// --- The boss's hooks ---
	WILDREACH_API bool IsRival(const AWRBoss* Boss, const AWRCharacterBase* Other);
	WILDREACH_API bool MayTargetRival(const AWRBoss* Boss, float Dist);
	WILDREACH_API void OnAttackStart(AWRBoss* Boss, const AWRCharacterBase* Target);
	WILDREACH_API float Incoming(AWRBoss* Boss, const FWRHit& Hit, float Damage);
	WILDREACH_API float Outgoing(const AWRBoss* Boss);
	/** Triumph's speed (walk and wings). */
	WILDREACH_API float SpeedScale(const AWRBoss* Boss);
	/** Hardened seams: the pause between a champion's strings stretches (its attack speed x hardenSpeed). */
	WILDREACH_API float RecoveryScale(const AWRBoss* Boss);
	/** A flier's hover point kept inside the hall's walls and under its vault (unchanged outside a hall). */
	WILDREACH_API FVector ClampInside(const AWRBoss* Boss, const FVector& Want);
	WILDREACH_API float StanceScale(const AWRCharacterBase* Target);
	/** A champion's blow landed: its flags turn. True: the hall did what the blow leaves (skip the arena hazards). */
	WILDREACH_API bool Aftermath(AWRBoss* Boss, FName AttackId, const FVector& At, float Size, const FRotator& Facing);
	WILDREACH_API void OnChampionDied(AWRBoss* Boss);

	// --- Players ---
	WILDREACH_API FString EnterBlock(const AWRPlayerController* PC, const AWRWarHallGate* Gate);
	WILDREACH_API FString StateLine(const AWRWarHallGate* Gate);
	WILDREACH_API FString Command(AWRPlayerController* PC, const TArray<FString>& Args);
	WILDREACH_API bool Prompt(const AWRPlayerController* PC, FString& OutTarget, FString& OutVerb);
	WILDREACH_API bool ClientInteract(AWRPlayerController* PC);
}
