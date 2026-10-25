#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRWorldDressing.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UPointLightComponent;

/**
 * Biome gear-check hazards (M3 plan, no temperature meter): lava burns unless you resist lava (fire
 * main path, fire-infused armor, or riding a mount); slick ice drops your grip unless you resist ice
 * (frost main path or frost-infused armor). Cells come from the dressing file and are known to every
 * machine, so the ice friction is identical on the server and the owning client (no corrections).
 */
struct WILDREACH_API FWRBiomeHazards
{
	static void Load(const TSharedPtr<class FJsonObject>& Hazards);
	/** "lava", "ice" or none, for feet at this location. */
	static FName KindAt(const FVector& Feet);
	static bool Resists(const class AWRCharacterBase* Who, FName Kind);
	static const TMap<FIntPoint, float>& Cells(FName Kind);
	static float CellSize() { return Cell; }

private:
	static TMap<FIntPoint, float> Lava;
	static TMap<FIntPoint, float> Ice;
	static float Cell;
};

/** Places of interest for the map list (bosses, mini-bosses, passes, camps, caves). */
struct WILDREACH_API FWRPoi
{
	FString Name;
	FString Kind;
	FVector Location = FVector::ZeroVector;
	static TArray<FWRPoi> All;
};

/**
 * Loads Data/<map>_dressing.json (made by Tools/terrain/dressing.mjs) when a game world starts:
 * every machine builds the decoration locally (no replication, no map rebuild to change it);
 * the server also spawns the caves, which replicate.
 */
UCLASS()
class WILDREACH_API UWRDressingSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	class AWRDressing* GetDressing() const { return Dressing.Get(); }
	class AWRHarvestLedger* GetLedger() const { return Ledger.Get(); }
	void SetLedger(class AWRHarvestLedger* InLedger) { Ledger = InLedger; }
	static class AWRDressing* FindDressing(const UWorld* World);

private:
	TWeakObjectPtr<class AWRDressing> Dressing;
	TWeakObjectPtr<class AWRHarvestLedger> Ledger;
};

/**
 * Decorative trees, rocks and shrubs as instanced meshes. Trunks and rocks block; foliage doesn't.
 * They are also the island's main gathering: a melee swing chops the tree, breaks the rock or picks the shrub in
 * front of you (wood / stone / fiber, numbers in tuning.json "gathering"). Used-up ones vanish on every machine
 * through the replicated AWRHarvestLedger and grow back later.
 */
UCLASS(NotPlaceable)
class WILDREACH_API AWRDressing : public AActor
{
	GENERATED_BODY()

public:
	AWRDressing();
	virtual void Tick(float DeltaSeconds) override;
	/** Build from the "instances" object of a dressing file. */
	void Build(const TSharedPtr<class FJsonObject>& Instances);
	/** Lava and ice surfaces (clients only: the server just needs the cells). */
	void BuildHazardVisuals();
	int32 GetInstanceCount() const { return Total; }

	/** Server: a swing from Who reached this far (cm from its centre). Gathers from the nearest harvestable decoration in front. */
	bool HarvestInFront(class AWRCharacterBase* Who, float Reach, bool bHeavy);
	/** Nearest standing harvestable of a group ("tree", "rock", "shrub"; empty = any) within Range: its ground point, or false. */
	bool FindNearest(const FVector& From, const FString& Group, float Range, FVector& OutGround) const;
	/** Every machine: hide or bring back the decorations whose keys are (or no longer are) in the ledger. */
	void SyncGone(const TArray<int32>& Gone);
	/** Server: one grew back (forget its hits). */
	void GrewBack(int32 Key) { HitsTaken.Remove(Key); }

private:
	UHierarchicalInstancedStaticMeshComponent* MakePart(const TCHAR* Mesh, const FLinearColor& Color, bool bCollision, float CullDistance);
	/** An instanced part with the mesh's own materials. */
	UHierarchicalInstancedStaticMeshComponent* MakeMeshPart(class UStaticMesh* Mesh, bool bCollision, float CullDistance);
	void SetGone(int32 Key, bool bGone);
	int32 Total = 0;

	/** Instanced parts that draw / collide one kind: decoration i is instance (i - Phase) / Stride of a part when i % Stride == Phase. */
	struct FPartRef { TWeakObjectPtr<UHierarchicalInstancedStaticMeshComponent> Part; int32 Stride = 1; int32 Phase = 0; };
	struct FHarvestKind
	{
		FString Kind;
		FString Group;
		FName Item;
		float Radius = 50.f;
		TArray<FVector> Ground;
		TArray<float> Scale;
		TMap<FIntPoint, TArray<int32>> Cells;
		TArray<FPartRef> Parts;
	};
	TArray<FHarvestKind> Harvestables;
	/** Gone decorations: original transforms of their instances (to put back when they grow back). */
	TMap<int32, TArray<FTransform>> Hidden;
	/** Server: hits taken by standing decorations. */
	TMap<int32, int32> HitsTaken;
	static constexpr float HarvestCell = 1000.f;
	static constexpr int32 KeyShift = 20;
};

/**
 * Server-owned list of the decorations that were gathered away (felled trees, broken rocks, picked shrubs),
 * replicated so every machine hides them. Each grows back after its group's respawn time.
 */
UCLASS(NotPlaceable)
class WILDREACH_API AWRHarvestLedger : public AActor
{
	GENERATED_BODY()

public:
	AWRHarvestLedger();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	/** Server: this decoration is used up; it grows back in Seconds. */
	void Fell(int32 Key, float Seconds);
	const TArray<int32>& GetGone() const { return Gone; }

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION() void OnRep_Gone();
	UPROPERTY(ReplicatedUsing=OnRep_Gone) TArray<int32> Gone;
	TMap<int32, double> GrowBackAt;
};

/**
 * A rock cave built from blocks on the surface: entrance tunnel and a dark chamber with rich ore
 * (better gem odds), a hazard (sulfur gas in the volcanic biome, ice underfoot in the tundra) and a
 * burrower colony. Geometry builds on every machine from the replicated biome; contents are server-spawned.
 */
UCLASS()
class WILDREACH_API AWRCave : public AActor
{
	GENERATED_BODY()

public:
	AWRCave();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated) FName Biome = TEXT("volcanic");

protected:
	virtual void BeginPlay() override;

private:
	void BuildShell();
	void SpawnContents();
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Rock;
	UPROPERTY() TObjectPtr<UPointLightComponent> Glow;
};
