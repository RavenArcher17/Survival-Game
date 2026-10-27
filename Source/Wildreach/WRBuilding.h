#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WRInventory.h"
#include "WRBuilding.generated.h"

class AWRPlayerController;
class UPointLightComponent;
class UStaticMeshComponent;

/** One buildable from Data/pieces.json. */
struct FWRPieceDef
{
	FName Id;
	FString Name;
	FName Category;  // structure / defense / utility / decor
	FName Kind;      // structure, door, chest, bed, light, crop, pen, decor, station, hearth, totem
	FName Shape;     // foundation, wall, halfwall, floor, roof, stairs, pillar, door, window, palisade, gate, tower
	FName Material;  // wood / stone / none
	FName Station;
	TArray<TPair<FName, int32>> Cost;
	float HP = 300.f;
	bool bLockable = false;
	/** A prop mesh that replaces the plain shape (stations, the Hearth, the totem, the chest): path, turn, fit box (cm). */
	FString Mesh;
	float MeshYaw = 0.f;
	FVector MeshFit = FVector::ZeroVector;

	/** Structure pieces snap to the grid and need support; everything else places freely. */
	bool Snaps() const { return Kind == TEXT("structure") || Kind == TEXT("door"); }
};

class WILDREACH_API FWRBuildRegistry
{
public:
	static FWRBuildRegistry& Get();
	const FWRPieceDef* Find(FName Id) const;
	const TArray<FWRPieceDef>& All() const { return Pieces; }

	float GridSize = 400.f;
	TMap<FName, int32> Span;
	TMap<FName, float> UpkeepPerPiecePerHour;
	float ClaimRadius = 3000.f;
	float ClaimRadiusPerUpgrade = 1000.f;
	float ClaimMaxRadius = 6000.f;
	TArray<TPair<FName, int32>> ClaimUpgradeCost;
	float DecayPerHourWhenUnpaid = 0.1f;

private:
	FWRBuildRegistry();
	TArray<FWRPieceDef> Pieces;
};

/** Pure building math (tests). */
struct WILDREACH_API FWRBuildRules
{
	/** Storey height (cm): walls are this tall, floors / roofs sit this far above the storey below. */
	static constexpr float Storey = 300.f;

	/**
	 * Snap a world point to the build grid for a shape; returns the piece location and yaw. Edge pieces get 0/90 from
	 * the nearest cell edge (a gable keeps a 180 flip from InOutYaw: which end is high); stairs and roofs keep InOutYaw
	 * rounded to 90 (which way they rise); everything else 0.
	 */
	static FVector SnapToGrid(FName Shape, const FVector& Hit, float BaseZ, float Grid, float& InOutYaw);
	/** Which slot a shape fills, so two pieces can't share one: "edge" (walls, doorways, windows, gables...), "cell" (foundations, floors, roofs), "corner" (pillars), "stairs". */
	static FName SlotOf(FName Shape);
	/** Pieces that carry what's stacked straight on top of them at no span cost (walls, pillars, foundations...). */
	static bool IsBearing(FName Shape);
	/** Span a piece at To costs when resting on one at From: INDEX_NONE = not touching, 0 = straight on top of a bearing piece, 1 = beside it. */
	static int32 SupportCost(const FVector& From, FName FromShape, const FVector& To, float Grid);
	/** Storey to build in when aiming at PointZ, from the foundation floor at Top: what you look into (a floor's surface counts as its storey). Floors, roofs, gables and upper walls go on top of it. */
	static int32 LevelFor(FName Shape, float PointZ, float Top);
	/** Max support distance (pieces) for a material: wood 4, stone 6. */
	static int32 SpanFor(FName Material);
	/** Upkeep per hour for a claim holding these pieces. */
	static TMap<FName, float> UpkeepPerHour(const TMap<FName, int32>& PiecesByMaterial);
	/**
	 * One minute of a claim's upkeep: adds each rate's sixtieth to Owed and takes whole items from Storage as they fall
	 * due, so 6 wood an hour costs 6 an hour. False when the claim can't pay (its pieces decay): a share it couldn't
	 * cover (dropped, not run up as back pay), or a material it needs run out.
	 */
	static bool PayUpkeepMinute(TArray<FWRItemStack>& Storage, TMap<FName, float>& Owed, const TMap<FName, float>& PerHour);
};

/**
 * Everything a player builds. Structure pieces carry a support value (0 = grounded); a piece
 * further than its material's span from the ground collapses. Doors open/close and can carry a
 * code lock; chests store items; beds set your respawn; crop plots grow food. Stations, the
 * Hearth and claim totems ride along as child actors so one save path covers them all.
 */
UCLASS()
class WILDREACH_API AWRBuildPiece : public AActor
{
	GENERATED_BODY()

public:
	AWRBuildPiece();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(ReplicatedUsing=OnRep_Piece) FName PieceId;
	UPROPERTY(Replicated) FString OwnerName;
	UPROPERTY(Replicated) float Health = 300.f;
	UPROPERTY(Replicated) float MaxHealth = 300.f;
	UPROPERTY(ReplicatedUsing=OnRep_Door) bool bOpen = false;
	UPROPERTY(Replicated) bool bLocked = false;
	/** Crop plots: server time planting finished growing (0 = empty). */
	UPROPERTY(Replicated) float CropReadyAt = 0.f;
	UPROPERTY(Replicated) bool bPlanted = false;
	/** Chests. */
	UPROPERTY(Replicated) TArray<FWRItemStack> Storage;

	/** Steps to the ground (0 = grounded); replicated so the build ghost can tell you before you place. */
	UPROPERTY(Replicated) int32 Support = 0;
	/** Server bookkeeping. */
	FString LockCode;
	TSet<FString> UnlockedFor;
	/** Ghosts are local previews in build mode: no collision, tinted. */
	bool bGhost = false;

	const FWRPieceDef* GetDef() const { return FWRBuildRegistry::Get().Find(PieceId); }
	void SetGhostValid(bool bValid);
	void ApplyLook();

	// --- Server ---
	void InitPiece(FName InPieceId, const FString& InOwner);
	void TakeBuildDamage(float Amount, AActor* DamageSource);
	FString Interact(AWRPlayerController* Player);
	bool CanAccess(const FString& PlayerName) const;
	AActor* GetChild() const { return ChildActor.Get(); }

	TSharedRef<class FJsonObject> ToJson() const;
	void FromJson(const TSharedPtr<class FJsonObject>& Json);

	/** Server: recompute support for structure pieces around a point; collapse what's unsupported. */
	static void RecomputeSupport(UWorld* World, const FVector& Around, float Radius);
	/** Support a new structure piece would have at this spot (INDEX_NONE = unsupported). */
	static int32 SupportAt(UWorld* World, FName PieceId, const FVector& Location, const AWRBuildPiece* Ignore = nullptr);
	/** Why a piece can't go here because of what's already built (its slot is taken, or stairs need the opening above them); empty = free. */
	static FString Blocked(UWorld* World, FName PieceId, const FVector& Location, const AWRBuildPiece* Ignore = nullptr);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UFUNCTION() void OnRep_Piece();
	UFUNCTION() void OnRep_Door();
	void SpawnChild();
	/** The real shape over the plain blocks (WRBuildLook), which stay as the collision. */
	void ApplyDetail(const FWRPieceDef& Def, const FVector& LeafSize);

	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Main;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Leaf;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> Light;
	TWeakObjectPtr<AActor> ChildActor;
	bool bLookApplied = false;
};

/** Land claim: only the owner (and their clan) builds inside; pays upkeep from its storage or decays. */
UCLASS()
class WILDREACH_API AWRClaimTotem : public AActor
{
	GENERATED_BODY()

public:
	AWRClaimTotem();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(Replicated) FString OwnerName;
	UPROPERTY(Replicated) float Radius = 3000.f;
	UPROPERTY(Replicated) int32 UpgradeLevel = 0;
	UPROPERTY(Replicated) bool bUnpaid = false;
	UPROPERTY(Replicated) TArray<FWRItemStack> UpkeepStorage;

	static AWRClaimTotem* FindAt(const UWorld* World, const FVector& Location);
	/** Can this player build here? (No claim = yes; someone else's claim = no.) */
	static bool CanBuild(const UWorld* World, const FVector& Location, const FString& PlayerName, FString& OutWhy);

	FString Deposit(AWRPlayerController* Player);
	FString Upgrade(AWRPlayerController* Player);
	FString Describe() const;
	TMap<FName, float> CurrentUpkeepPerHour() const;

protected:
	/** Where it's drawn: the carved post (pieces.json "mesh") over the plain cylinder, which stays as the collision. */
	virtual void BeginPlay() override;

private:
	float MinuteClock = 0.f;
	/** Upkeep due but not yet a whole item, per material (paid as it adds up to one). */
	TMap<FName, float> UpkeepOwed;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
};
