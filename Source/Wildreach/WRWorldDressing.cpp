#include "WRWorldDressing.h"
#include "Wildreach.h"
#include "WRBoss.h"
#include "WRCreature.h"
#include "WRResourceNode.h"
#include "WRTuning.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"

static UMaterialInterface* ShapeMaterial()
{
	return LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
}

// ---------------------------------------------------------------- subsystem

bool UWRDressingSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UWRDressingSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	FString Map = InWorld.GetMapName();
	Map.RemoveFromStart(InWorld.StreamingLevelsPrefix);
	Map.RemoveFromStart(TEXT("L_"));
	const TSharedPtr<FJsonObject> File = FWRTuning::LoadDataFile(FString::Printf(TEXT("%s_dressing.json"), *Map.ToLower()));
	if (!File.IsValid())
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const TSharedPtr<FJsonObject>* Instances = nullptr;
	if (File->TryGetObjectField(TEXT("instances"), Instances))
	{
		if (AWRDressing* Dressing = InWorld.SpawnActor<AWRDressing>(AWRDressing::StaticClass(), FTransform::Identity, Params))
		{
			Dressing->Build(*Instances);
		}
	}
	// Caves replicate: only the server spawns them.
	const TArray<TSharedPtr<FJsonValue>>* Caves = nullptr;
	if (InWorld.GetNetMode() != NM_Client && File->TryGetArrayField(TEXT("caves"), Caves))
	{
		for (const TSharedPtr<FJsonValue>& V : *Caves)
		{
			const TSharedPtr<FJsonObject> C = V->AsObject();
			const FVector At(C->GetNumberField(TEXT("x")), C->GetNumberField(TEXT("y")), C->GetNumberField(TEXT("z")));
			const FTransform T(FRotator(0.f, C->GetNumberField(TEXT("yaw")), 0.f), At);
			if (AWRCave* Cave = InWorld.SpawnActorDeferred<AWRCave>(AWRCave::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
			{
				Cave->Biome = FName(*C->GetStringField(TEXT("biome")));
				Cave->FinishSpawning(T);
			}
		}
		UE_LOG(LogWildreach, Log, TEXT("DRESSING spawned %d caves"), Caves->Num());
	}
}

// ---------------------------------------------------------------- decoration

AWRDressing::AWRDressing()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	bReplicates = false;
	RootComponent->SetMobility(EComponentMobility::Static);
}

UHierarchicalInstancedStaticMeshComponent* AWRDressing::MakePart(const TCHAR* MeshPath, const FLinearColor& Color, bool bCollision, float CullDistance)
{
	UHierarchicalInstancedStaticMeshComponent* Part = NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
	Part->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, MeshPath));
	Part->SetupAttachment(RootComponent);
	Part->SetMobility(EComponentMobility::Static);
	Part->SetCollisionEnabled(bCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	if (bCollision) Part->SetCollisionProfileName(TEXT("BlockAll"));
	Part->SetCullDistances(0, CullDistance);
	Part->bAffectDistanceFieldLighting = false;
	Part->SetCastShadow(bCollision || CullDistance > 20000.f);
	Part->RegisterComponent();
	if (UMaterialInterface* Mat = ShapeMaterial())
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Mat, this);
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		Part->SetMaterial(0, MID);
	}
	return Part;
}

void AWRDressing::Build(const TSharedPtr<FJsonObject>& Instances)
{
	const bool bServer = GetWorld()->GetNetMode() == NM_DedicatedServer;
	static const TCHAR* Cyl = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	static const TCHAR* Cone = TEXT("/Engine/BasicShapes/Cone.Cone");
	static const TCHAR* Sphere = TEXT("/Engine/BasicShapes/Sphere.Sphere");
	static const TCHAR* Cube = TEXT("/Engine/BasicShapes/Cube.Cube");

	// One part of a decoration: mesh, colour, size (engine shapes are 100 units), offset above the ground.
	struct FPart { const TCHAR* Mesh; FLinearColor Color; FVector Size; float Lift; bool bCollision; float Cull; FVector Tilt; };
	const TMap<FString, TArray<FPart>> Kinds = {
		{ TEXT("char_tree"), { { Cyl, FLinearColor(0.02f, 0.015f, 0.012f), FVector(40, 40, 520), 240, true, 24000, FVector::ZeroVector },
		                       { Cone, FLinearColor(0.03f, 0.025f, 0.02f), FVector(180, 180, 260), 520, false, 24000, FVector::ZeroVector } } },
		{ TEXT("pine"), { { Cyl, FLinearColor(0.12f, 0.06f, 0.03f), FVector(45, 45, 300), 140, true, 24000, FVector::ZeroVector },
		                  { Cone, FLinearColor(0.02f, 0.12f, 0.05f), FVector(300, 300, 620), 520, false, 24000, FVector::ZeroVector },
		                  { Cone, FLinearColor(0.92f, 0.95f, 0.98f), FVector(150, 150, 220), 760, false, 24000, FVector::ZeroVector } } },
		{ TEXT("basalt_rock"), { { Sphere, FLinearColor(0.025f, 0.022f, 0.022f), FVector(260, 220, 150), 30, true, 24000, FVector(8, 0, 0) } } },
		{ TEXT("ice_rock"), { { Cube, FLinearColor(0.45f, 0.65f, 0.85f), FVector(200, 170, 160), 50, true, 24000, FVector(12, 0, 18) } } },
		{ TEXT("ember_shrub"), { { Sphere, FLinearColor(0.6f, 0.12f, 0.02f), FVector(110, 110, 60), 15, false, 12000, FVector::ZeroVector } } },
		{ TEXT("snow_shrub"), { { Sphere, FLinearColor(0.75f, 0.82f, 0.8f), FVector(120, 120, 70), 15, false, 12000, FVector::ZeroVector } } },
	};
	for (const TPair<FString, TArray<FPart>>& Kind : Kinds)
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (!Instances->TryGetArrayField(Kind.Key, Arr) || Arr->Num() < 5)
		{
			continue;
		}
		const int32 Count = Arr->Num() / 5;
		for (const FPart& Part : Kind.Value)
		{
			if (bServer && !Part.bCollision)
			{
				continue;  // the server only needs what blocks movement
			}
			TArray<FTransform> Transforms;
			Transforms.Reserve(Count);
			for (int32 i = 0; i < Count; ++i)
			{
				const float S = (*Arr)[i * 5 + 4]->AsNumber();
				const float Yaw = (*Arr)[i * 5 + 3]->AsNumber();
				const FVector Ground((*Arr)[i * 5]->AsNumber(), (*Arr)[i * 5 + 1]->AsNumber(), (*Arr)[i * 5 + 2]->AsNumber());
				// Small per-instance tilt for rocks, from the yaw so every machine agrees.
				const FRotator Rot(Part.Tilt.X * FMath::Sin(Yaw), Yaw, Part.Tilt.Z * FMath::Cos(Yaw * 1.7f));
				Transforms.Emplace(Rot, Ground + FVector(0.f, 0.f, Part.Lift * S - 20.f), Part.Size / 100.f * S);
			}
			MakePart(Part.Mesh, Part.Color, Part.bCollision, Part.Cull)->AddInstances(Transforms, false, true);
		}
		Total += Count;
	}
	UE_LOG(LogWildreach, Log, TEXT("DRESSING built %d decorations (%s)"), Total, bServer ? TEXT("collision only") : TEXT("full"));
}

// ---------------------------------------------------------------- caves

AWRCave::AWRCave()
{
	bReplicates = true;
	SetNetCullDistanceSquared(FMath::Square(25000.f));  // matches the decoration / terrain streaming range
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Rock = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Rock"));
	Rock->SetupAttachment(RootComponent);
	Rock->SetCollisionProfileName(TEXT("BlockAll"));
	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(RootComponent);
	Glow->SetRelativeLocation(FVector(0.f, 0.f, 550.f));
	Glow->SetIntensity(1200.f);
	Glow->SetAttenuationRadius(1600.f);
}

void AWRCave::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRCave, Biome);
}

void AWRCave::BeginPlay()
{
	Super::BeginPlay();
	BuildShell();
	if (HasAuthority())
	{
		SpawnContents();
	}
}

void AWRCave::BuildShell()
{
	// Local space: chamber interior x/y -900..900, 0..700 high; the entrance tunnel runs out along +X.
	// Walls reach 4 m underground so uneven ground never leaves gaps.
	Rock->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	const bool bFrost = Biome == TEXT("tundra");
	if (UMaterialInterface* Mat = ShapeMaterial())
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Mat, this);
		MID->SetVectorParameterValue(TEXT("Color"), bFrost ? FLinearColor(0.35f, 0.45f, 0.58f) : FLinearColor(0.04f, 0.035f, 0.03f));
		Rock->SetMaterial(0, MID);
	}
	const auto Block = [this](const FVector& Center, const FVector& Size) { Rock->AddInstance(FTransform(FRotator::ZeroRotator, Center, Size / 100.f)); };
	Block(FVector(-1050, 0, 300), FVector(300, 2400, 1400));       // back wall
	Block(FVector(0, -1050, 300), FVector(2400, 300, 1400));       // side walls
	Block(FVector(0, 1050, 300), FVector(2400, 300, 1400));
	Block(FVector(1050, -725, 300), FVector(300, 950, 1400));      // front wall either side of the opening
	Block(FVector(1050, 725, 300), FVector(300, 950, 1400));
	Block(FVector(1050, 0, 725), FVector(300, 500, 550));          // over the opening
	Block(FVector(0, 0, 1150), FVector(2400, 2400, 300));          // roof
	Block(FVector(1600, -400, 175), FVector(800, 300, 1150));      // tunnel
	Block(FVector(1600, 400, 175), FVector(800, 300, 1150));
	Block(FVector(1600, 0, 600), FVector(800, 800, 300));
	Glow->SetLightColor(bFrost ? FLinearColor(0.5f, 0.7f, 1.f) : FLinearColor(1.f, 0.45f, 0.15f));
}

void AWRCave::SpawnContents()
{
	const bool bFrost = Biome == TEXT("tundra");
	const auto Floor = [this](const FVector& Local)
	{
		const FVector Top = GetActorTransform().TransformPosition(FVector(Local.X, Local.Y, 650.f));
		FHitResult Hit;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(WRCave), false, this);
		return GetWorld()->LineTraceSingleByChannel(Hit, Top, Top - FVector(0, 0, 2000), ECC_WorldStatic, Query) ? Hit.ImpactPoint : Top - FVector(0, 0, 650);
	};
	// Rich ore: the better metals live in caves, with three times the usual gem odds.
	const TArray<TPair<FName, FVector>> Ores = {
		{ TEXT("iron_ore"), FVector(-550, -550, 0) }, { TEXT("iron_ore"), FVector(-550, 550, 0) }, { TEXT("tin_ore"), FVector(0, -650, 0) },
		{ TEXT("copper_ore"), FVector(300, 620, 0) }, { bFrost ? FName(TEXT("lichen")) : FName(TEXT("sulfur_crystal")), FVector(-700, 0, 0) },
	};
	for (const TPair<FName, FVector>& Ore : Ores)
	{
		const FTransform T(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), Floor(Ore.Value));
		if (AWRResourceNode* Node = GetWorld()->SpawnActorDeferred<AWRResourceNode>(AWRResourceNode::StaticClass(), T, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			Node->ItemId = Ore.Key;
			Node->GemChance = WRTune(TEXT("caves.gemChance"), 0.12f);
			Node->FinishSpawning(T);
		}
	}
	// Cave hazard: burning sulfur gas (volcanic) or slick ice (tundra). Permanent.
	AWRHazard::Spawn(GetWorld(), nullptr, Floor(FVector(250, 0, 0)), bFrost ? 450.f : 260.f, 0.f,
		WRTune(TEXT("caves.gasDps"), 6.f), bFrost ? FName(TEXT("ice")) : FName(TEXT("gas")));
	// A burrower colony (Kilnmites nest here and mine ore once tamed; Glacimaws lurk under the ice).
	const FTransform SpawnAt(FRotator::ZeroRotator, Floor(FVector(-300, 0, 0)));
	if (AWRCreatureSpawner* Spawner = GetWorld()->SpawnActorDeferred<AWRCreatureSpawner>(AWRCreatureSpawner::StaticClass(), SpawnAt, this))
	{
		Spawner->CreatureId = bFrost ? FName(TEXT("glacimaw")) : FName(TEXT("kilnmite"));
		Spawner->Count = bFrost ? 1 : 2;
		Spawner->Radius = 300.f;
		Spawner->RespawnSeconds = 300.f;
		Spawner->FinishSpawning(SpawnAt);
	}
	UE_LOG(LogWildreach, Log, TEXT("CAVE %s (%s) at %s: %d ore nodes"), *GetName(), *Biome.ToString(), *GetActorLocation().ToCompactString(), Ores.Num());
}
