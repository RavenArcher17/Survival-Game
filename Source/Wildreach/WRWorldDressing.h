#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRWorldDressing.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UPointLightComponent;

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
	/** Build from the "instances" object of a dressing file. */
	void Build(const TSharedPtr<class FJsonObject>& Instances);
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
