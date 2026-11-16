#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "WRRaiding.generated.h"

class AWRBuildPiece;
class AWRCharacterBase;
class AWRClaimTotem;
class AWRPlayerController;
class AWRPlayerState;
class AWRProjectile;
class AWRResourceNode;
class UProceduralMeshComponent;
class UPointLightComponent;
class UBoxComponent;

/** What hurt a piece: each has its own share per material (tuning raiding.material.<source>). */
enum class EWRRaidSource : uint8 { Weapon, Blast, FirePot, Spell, Tame, Fire, Ram, Siege };   // Ram / Siege: siege engines (WRSiege)

/**
 * Player raiding (FULL-GAME 7; cooks/build-server-raids.md 2.3 to 2.7): one rule for every player-side source that hurts
 * a building piece (weapons, charges, fire pots, path spells, burning ground, tames), the raid tools (the petard, the
 * mining charge, the siege ladder, the code breaker), the claim's raid bell and its alerts, and the raid record that
 * tells owners who were away what happened. Creature raids stay in WRRaids (they bite through TakeBuildDamage).
 * Everything is the server's; numbers are tuning.json "raiding".
 */
struct WILDREACH_API FWRRaiding
{
	/** The piece an actor is or belongs to (a station, the Hearth and the totem are a piece's child actors). */
	static AWRBuildPiece* PieceOf(AActor* Actor);
	/** The player answerable for Actor: a survivor's name, a tame's owner, "" for anything else. */
	static FString Responsible(const AActor* Actor);
	/** May Raider's side hurt this piece? Not their own or their clan's, and only on a PvP world. OutWhy says why not. */
	static bool MayRaid(const AWRBuildPiece* Piece, const FString& Raider, FString* OutWhy = nullptr);
	/** wood / stone / metal / biome / hearth / other: the row of the share tables a piece reads. */
	static FName Category(const AWRBuildPiece* Piece);
	/** The share of a hit that lands: the source's share for the piece's material x the path's multiplier for it. */
	static float Scale(const AWRBuildPiece* Piece, EWRRaidSource Source, FName PathId);
	/**
	 * Server: Amount (before shares) to Piece from Raider's side; rings the claim's bell, alerts its owners and keeps the
	 * raid record. Offline protection still applies (TakeBuildDamage). Returns what it was hit for (0: not allowed).
	 */
	static float DamagePiece(AWRBuildPiece* Piece, float Amount, EWRRaidSource Source, AActor* Causer, const FString& Raider, FName PathId);
	/** Server: every piece Raider may hurt whose bounds come within Radius of At, once each. Returns how many. */
	static int32 SplashPieces(UWorld* World, const FVector& At, float Radius, float Amount, EWRRaidSource Source, AActor* Causer, const FString& Raider, FName PathId, const AWRBuildPiece* Skip = nullptr);

	/** Server: strike a claim's raid bell (at most every raiding.bell.minGapSeconds): its swing on every machine, its
	 *  sound a quarter swing later. Creature raids ring it at their warning and arrival (WRRaids). */
	static void RingBell(AWRClaimTotem* Totem);
	/** Server: a raid item used from the bag or belt (items.json category "raid"): place a charge, a ladder, start the
	 *  code breaker. Returns the line the player sees. */
	static FString UseItem(AWRPlayerState* PS, FName ItemId);
	/** Server: a fire pot came down (AWRProjectile, kind firepot): shards, a fire burst, burning ground. */
	static void FirePotBurst(AWRProjectile* Pot, const FVector& At, AWRCharacterBase* Thrower);

	/** Server: a joining player hears about raids on their claims while they were away. */
	static void PlayerJoined(AWRPlayerController* PC);
	/** The raid record in the world store (the "raid" document). */
	static void Save();
	static void Load(UWorld* World);

	/** Server, every second: code breakers in progress. */
	static void Step(UWorld* World);

	/** Full-release server settings (server_presets.json): the most survivors one war-hall raid lets in (0 = no cap) and
	 *  the group its champions are balanced for. For the endgame raids (cooks/endgame.md 2.9). */
	static int32 RaidGroupMax();
	static int32 RaidGroupTuned();
	/** Server: tames of Owner in earshot go for the piece under Owner's crosshair ("Break that"). Returns the line. */
	static FString OrderTamesToBreak(AWRPlayerController* PC);
	/** This machine, the local survivor: with a fire pot out and aimed, the arc it will fly is drawn as glowing dots with
	 *  a ring where it comes down (projectiles never home: you aim the arc yourself). */
	static void TickThrowArc(class AWRCharacter* Who, float DeltaSeconds);
	/** The piece a survivor's crosshair rests on within Range, or null. */
	static AWRBuildPiece* PieceInSights(const AWRCharacterBase* Who, float Range, FVector* OutPoint = nullptr);
};

/** A placed powder charge (the petard) or mining charge: lit when placed, it goes off when its fuse has burnt. */
UCLASS()
class WILDREACH_API AWRRaidCharge : public AActor
{
	GENERATED_BODY()

public:
	AWRRaidCharge();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	/** powder_charge or mining_charge (items.json). */
	UPROPERTY(ReplicatedUsing=OnRep_Kind) FName Kind;
	/** Server time it was lit, and how long its match burns. */
	UPROPERTY(Replicated) float LitAt = 0.f;
	UPROPERTY(Replicated) float FuseSeconds = 5.f;

	// --- Server
	TWeakObjectPtr<AWRBuildPiece> OnPiece;
	TWeakObjectPtr<AWRResourceNode> OnNode;
	FString Placer;
	TWeakObjectPtr<AWRCharacterBase> PlacerBody;
	bool bFalling = false;
	FVector FallVelocity = FVector::ZeroVector;

	/** Where the match's burning end is now (the cord runs from its coil to the touch hole as the fuse burns). */
	FVector MatchPoint() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UFUNCTION() void OnRep_Kind();
	void Explode();
	float FuseLeft() const;

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Body;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> Glow;
	UPROPERTY(Transient) TObjectPtr<class UAudioComponent> Hiss;
	float SparkClock = 0.f;
	bool bBuilt = false;
};

/** A siege ladder against a wall: its feet on the ground, its hooks over the wall's top. Climbed by walking into it. */
UCLASS()
class WILDREACH_API AWRSiegeLadder : public AActor
{
	GENERATED_BODY()

public:
	AWRSiegeLadder();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	/** Rail length (cm) and how far its foot stands out from the wall: the lean. Replicated: every machine builds the same ladder. */
	UPROPERTY(ReplicatedUsing=OnRep_Shape) float Length = 600.f;
	UPROPERTY(ReplicatedUsing=OnRep_Shape) float FootOut = 160.f;
	UPROPERTY(Replicated) float Health = 300.f;

	/** The ladder's frame: X out from the wall (toward the climber), Z up the rails, from the foot. */
	FVector Up() const;
	FVector Out() const;
	FVector Foot() const { return GetActorLocation(); }
	FVector Top() const { return Foot() + Up() * Length; }
	/** Is this point (a capsule's centre) on the ladder's climbing lane, and how far up it (0..1)? */
	bool OnLane(const FVector& At, float Radius, float& OutAlong) const;

	// --- Server
	TWeakObjectPtr<AWRBuildPiece> Against;
	FString Placer;
	float DieAt = 0.f;
	/** Server: a blow on it (wood: weapons hurt it fully). */
	void TakeHit(float Amount, AActor* By);

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION() void OnRep_Shape();
	void Build();

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Body;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Blocker;
};

/**
 * The raid bell on a claim totem (cooks/build-server-raids.md 2.5): a bronze bell on a yoke and pin in an iron bracket
 * strapped round the post, its clapper hanging inside. Struck (the totem's replicated BellRungAt), it swings as a damped
 * pendulum and the clapper lags it, the same on every machine. Visual only; the server rings it (FWRRaiding).
 */
UCLASS()
class WILDREACH_API UWRRaidBellComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UWRRaidBellComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	/** Hangs it on the post: Height and Radius of the post where the strap goes (cm, in the totem's space). */
	void Hang(float Height, float PostRadius);

private:
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Bracket;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Pivot;
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Bell;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> ClapperPivot;
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Clapper;
};
