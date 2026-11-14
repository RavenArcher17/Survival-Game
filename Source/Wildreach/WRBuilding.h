#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WRInventory.h"
#include "WRMachine.h"
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
	/** Comfort (survival-crafting 2.2): the group it counts in (seat, table, bed, floor, wall, fire, light, trophy...)
	 *  and how much; only the best piece of each group near a bed counts. */
	FName ComfortGroup;
	int32 Comfort = 0;
	/** Farming and food (cooks/build-farming.md): the covering Hearth's tech level it needs ("hearth"), the biome it
	 *  belongs to ("biome": placed only there, shown only while that land is live), the plot type a crop plot is
	 *  ("plot": soil, ashpit, paddy, kelp, log), glazing ("glass": daylight through it, a greenhouse), a chest's cold
	 *  ("cold": food keeps at this pace; an ice box burns "fuel" every "fuelMinutes"), a creature it needs ("needs":
	 *  the hive's Hiveback), the legendary fish a trophy shows ("trophy"). */
	int32 HearthLevel = 0;
	FName Biome;
	FName Plot;
	bool bGlass = false;
	float Cold = 0.f;
	FName Fuel;
	float FuelMinutes = 0.f;
	FName NeedsCreature;
	FName Trophy;
	/** The build card's line (pieces.json "note"). */
	FString Note;
	/** Shown only while this biome's land is live, though it builds anywhere (pieces.json "liveWith": the cold store,
	 *  the soul engine, the resonance pylon, the rift chest: cooks/build-machinery.md 2.5). */
	FName LiveWith;
	/** build-building (Data/building_sets.json, FWRBuildSets): the tier a generated piece belongs to (metal, wood,
	 *  basalt_hall...; Material is the same for its structure), a decor piece's model and variant (WRBuildLookDecor), a
	 *  freeform beam's length (cm: it runs that far along its own X from where it's placed), the blueprint that must be
	 *  learned to build it, a placing rule ("vent": lava within reach, a hot spring bath). */
	FName Tier;
	FName Arch;
	FName Variant;
	float FreeLength = 0.f;
	FName Blueprint;
	FName Needs;
	/** Freeform structural pieces (beams, posts, braces): placed at any angle, carry support from their ends. */
	bool IsBeam() const { return Kind == TEXT("beam"); }
	/** Anything that takes part in support (snapped structure and beams). */
	bool IsStructural() const { return Snaps() || IsBeam(); }
	/** Plots, pens, hives, traps, salt pans, wells and channels: run by the farm subsystem (WRFarm). */
	bool IsFarm() const { return Kind == TEXT("crop") || Kind == TEXT("pen") || Kind == TEXT("hive") || Kind == TEXT("trap") || Kind == TEXT("saltpan") || Kind == TEXT("well") || Kind == TEXT("channel"); }

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
	/** The item a material's upkeep is paid in (wood -> wood, stone -> stone, metal -> steel plate, a set -> its block). */
	TMap<FName, FName> UpkeepItem;
	FName UpkeepItemFor(FName Material) const { const FName* I = UpkeepItem.Find(Material); return I ? *I : Material; }
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
	/**
	 * The same, on a grid laid out from Origin and turned FrameYaw: a foundation's own, so what's built beside a foundation
	 * that was placed at an angle lines up with it. InOutYaw is the world's both ways. LineStep under Grid is the fine grid:
	 * edge pieces go on any line that far apart across a cell (the edges among them) and pillars on any crossing of them;
	 * LineStep = Grid is the plain grid. Cell pieces stay on whole cells either way.
	 */
	static FVector SnapInFrame(FName Shape, const FVector& Hit, float BaseZ, float Grid, float LineStep, const FVector& Origin, float FrameYaw, float& InOutYaw);
	/** A piece is where SnapInFrame with this frame could have put it: on its lines, square to it (2 cm, 1 degree of slack). */
	static bool IsSnappedIn(FName Shape, const FVector& Location, float Yaw, float Grid, float LineStep, const FVector& Origin, float FrameYaw);
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
	/** Upkeep per hour for a claim holding these pieces (by material), keyed by the item it's paid in. */
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
	/** Server, before it's spawned: its health x Scale (its builder's Building level and Fortifier perk; saved). */
	void ScaleHealth(float Scale) { MaxHealth *= FMath::Max(0.1f, Scale); Health = MaxHealth; }
	/** Server: when it last took damage (Self repair waits for a quiet minute). */
	double LastDamagedAt = -1.0e9;
	UPROPERTY(ReplicatedUsing=OnRep_Door) bool bOpen = false;
	UPROPERTY(Replicated) bool bLocked = false;
	/** Crop plots: server time planting finished growing (0 = empty). */
	UPROPERTY(Replicated) float CropReadyAt = 0.f;
	UPROPERTY(Replicated) bool bPlanted = false;
	/** Chests; a pen's, hive's, trap's or salt pan's products waiting to be collected (shown on it). */
	UPROPERTY(ReplicatedUsing=OnRep_Farm) TArray<FWRItemStack> Storage;
	/** Farming (WRFarm): a plot's crop, its growth 0..255, what's helping or stopping it (WRFarmFlag, EWRFarmStop);
	 *  a pen's trough or a hive's feed; an ice box's ice. */
	UPROPERTY(ReplicatedUsing=OnRep_Farm) FName CropId;
	UPROPERTY(ReplicatedUsing=OnRep_Farm) uint8 Growth8 = 0;
	UPROPERTY(ReplicatedUsing=OnRep_Farm) uint8 FarmFlags = 0;
	UPROPERTY(Replicated) uint8 FarmStop = 0;
	UPROPERTY(ReplicatedUsing=OnRep_Farm) int32 Feed = 0;
	/** Server: growth 0..1, who planted it and their growing speed then, the crop it last grew; a producer's clocks. */
	float Growth = 0.f;
	FString Planter;
	float PlanterSpeed = 1.f;
	bool bPlanterCompanion = false;
	bool bPlanterWaterWise = false;
	bool bPlanterGraft = false;
	FName LastCrop;
	float ProduceClockA = 0.f, ProduceClockB = 0.f, FuelClock = 0.f;
	/** Client: the plants / products / water drawn on it (WRFarmLook) and what they were drawn for. */
	UPROPERTY(Transient) TObjectPtr<class UProceduralMeshComponent> FarmMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> WaterSurface;
	uint32 FarmLookKey = 0;
	UFUNCTION() void OnRep_Farm();
	/** Machines (WRMachine, cooks/build-machinery.md): what every machine draws and the screen reads; a conveyor's
	 *  riding stacks (their stacks themselves are in Storage, in the same order); the server's own state (saved). */
	UPROPERTY(Replicated) FWRMachineRep Machine;
	/** Turrets (WRTurret): where the head points, its aim line and its last shot (every machine draws them). */
	UPROPERTY(Replicated) FWRTurretRep Turret;
	UPROPERTY(Replicated) TArray<FWRBeltItem> Belt;
	FWRMachineState Mach;
	/** A name its owner gave it (chests: "chestname"; empty = the piece's own name). Saved with the world. */
	UPROPERTY(Replicated) FString CustomName;

	/** Steps to the ground (0 = grounded); replicated so the build ghost can tell you before you place. */
	UPROPERTY(Replicated) int32 Support = 0;
	/** Code-locked doors: who has entered its code since it was set (replicated so the prompt and E know who may open it:
	 *  the keypad only comes up for someone who can't). Not saved: after a restart, the code again. */
	UPROPERTY(Replicated) TArray<FString> UnlockedFor;
	/** Server bookkeeping. */
	FString LockCode;
	/** A Hearth's piece: the game hour it was built (its first day takes an offering) and the meal left on it (saved). */
	double PlacedHours = -1.0;
	FName Offering;
	/** Build mode's stability colour laid over it (client, WRBuildIntegrity): which colour it wears now (-1 none). */
	int32 TintStep = -1;
	/** Beds: the players who've set it to wake at (WRBeds; saved with the world). */
	TArray<FString> Sleepers;
	/** Ghosts are local previews in build mode: no collision, tinted, in the piece's real shape. */
	bool bGhost = false;
	/** A ghost without the detail blocks (demolish's glowing copy of what you aim at: its glow isn't made for instanced blocks). */
	bool bPlainGhost = false;

	const FWRPieceDef* GetDef() const { return FWRBuildRegistry::Get().Find(PieceId); }
	void SetGhostValid(bool bValid);
	/** Tint a ghost. */
	void SetGhostColor(const FLinearColor& Color);
	/** Make a ghost glow, unlit, so it reads the same by day and by night (demolish mode marks what a click takes down). */
	void SetGhostGlow(const FLinearColor& Color, float Glow);
	void ApplyLook();
	/** What a ghost is showing, for tests: the prop mesh's name, or how many blocks it's made of. */
	FString GhostLook() const;

	// --- Server ---
	void InitPiece(FName InPieceId, const FString& InOwner);
	void TakeBuildDamage(float Amount, AActor* DamageSource);
	FString Interact(AWRPlayerController* Player);
	bool CanAccess(const FString& PlayerName) const;
	/** As any machine can tell (clans from the replicated player states, so an offline owner's clan isn't known): the
	 *  piece is Player's or their clan's (CanAccess's rule), and may Player open this door (no lock, theirs, or they've
	 *  entered its code)? The prompt and E use these; the server decides with CanAccess / UnlockedFor. */
	bool SeemsShared(const AController* Player) const;
	bool SeemsOpenable(const AController* Player) const;
	/** Doors: open or shut it (the right code on its keypad opens it). */
	void SetOpen(bool bNewOpen) { bOpen = bNewOpen; OnRep_Door(); }
	AActor* GetChild() const { return ChildActor.Get(); }

	TSharedRef<class FJsonObject> ToJson() const;
	void FromJson(const TSharedPtr<class FJsonObject>& Json);

	/** Server: recompute support for structure pieces around a point; what's unsupported falls (rubble, WRBuildIntegrity). */
	static void RecomputeSupport(UWorld* World, const FVector& Around, float Radius);
	/** Support a new structure piece would have at this spot (INDEX_NONE = unsupported). */
	static int32 SupportAt(UWorld* World, FName PieceId, const FVector& Location, const AWRBuildPiece* Ignore = nullptr)
	{
		return SupportAt(World, PieceId, FTransform(Location), Ignore);
	}
	/** The same with its turn and slope (a beam reaches from its location along its own X). */
	static int32 SupportAt(UWorld* World, FName PieceId, const FTransform& At, const AWRBuildPiece* Ignore = nullptr);
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
	/** Server: the game hour (AWRGameState::GetTotalHours) its next creature raid is due (WRRaids); < 0: not set yet. Saved. */
	double NextRaidHours = -1.0;

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
