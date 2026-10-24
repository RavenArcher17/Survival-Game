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
};

/** Decorative trees, rocks and shrubs as instanced meshes. Trunks and rocks block; foliage doesn't. */
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

private:
	UHierarchicalInstancedStaticMeshComponent* MakePart(const TCHAR* Mesh, const FLinearColor& Color, bool bCollision, float CullDistance);
	int32 Total = 0;
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
