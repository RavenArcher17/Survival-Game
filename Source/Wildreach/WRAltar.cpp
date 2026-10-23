#include "WRAltar.h"
#include "Wildreach.h"
#include "WRPathComponent.h"
#include "WRPathTypes.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

AWRBossAltar::AWRBossAltar()
{
	bReplicates = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	Pedestal = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pedestal"));
	RootComponent = Pedestal;
	Pedestal->SetRelativeScale3D(FVector(1.2f, 1.2f, 1.f));
	if (Cylinder.Succeeded()) Pedestal->SetStaticMesh(Cylinder.Object);

	Flame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Flame"));
	Flame->SetupAttachment(RootComponent);
	Flame->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Flame->SetRelativeLocation(FVector(0.f, 0.f, 80.f));
	Flame->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.5f));
	if (Sphere.Succeeded()) Flame->SetStaticMesh(Sphere.Object);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (ShapeMaterial.Succeeded())
	{
		Flame->SetMaterial(0, ShapeMaterial.Object);
	}
}

void AWRBossAltar::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRBossAltar, PathId);
}

void AWRBossAltar::BeginPlay()
{
	Super::BeginPlay();
	if (UMaterialInstanceDynamic* Mat = Flame->CreateAndSetMaterialInstanceDynamic(0))
	{
		Mat->SetVectorParameterValue(TEXT("Color"), PathId == TEXT("fire") ? FLinearColor(1.f, 0.35f, 0.05f) : FLinearColor(0.5f, 0.85f, 1.f));
	}
}

FString AWRBossAltar::GetPrompt(const AWRPlayerController* Player) const
{
	const AWRPlayerState* PS = Player ? Player->GetPlayerState<AWRPlayerState>() : nullptr;
	const FWRPathDef* Def = FWRPathRegistry::Get().Find(PathId);
	if (!PS || !Def)
	{
		return FString();
	}
	const UWRPathComponent* Paths = PS->GetPaths();
	if (Paths->GetMainPath() == PathId)
	{
		return FString::Printf(TEXT("Altar of %s (your main path)"), *Def->Name);
	}
	if (Paths->GetSubPath() == PathId)
	{
		return FString::Printf(TEXT("Altar of %s (your subclass)"), *Def->Name);
	}
	return FString::Printf(TEXT("E: take %s as your subclass"), *Def->Name);
}

FString AWRBossAltar::Interact(AWRPlayerController* Player)
{
	AWRPlayerState* PS = Player ? Player->GetPlayerState<AWRPlayerState>() : nullptr;
	if (!PS || !HasAuthority())
	{
		return FString();
	}
	UWRPathComponent* Paths = PS->GetPaths();
	if (Paths->GetMainPath() == PathId || Paths->GetSubPath() == PathId)
	{
		return TEXT("You already walk this path.");
	}
	const FString Why = Paths->SetSubPath(PathId);
	UE_LOG(LogWildreach, Log, TEXT("ALTAR %s: %s takes %s as subclass -> %s"), *GetName(), *PS->GetPlayerName(), *PathId.ToString(), Why.IsEmpty() ? TEXT("ok") : *Why);
	return Why.IsEmpty() ? FString::Printf(TEXT("%s is now your subclass."), *PathId.ToString()) : Why;
}
