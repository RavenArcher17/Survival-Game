#include "WRAltar.h"
#include "Wildreach.h"
#include "WRPathComponent.h"
#include "WRPathTypes.h"
#include "WRPathLook.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "WRBuildLook.h"
#include "WRFx.h"
#include "Components/PointLightComponent.h"

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
	if (GetNetMode() != NM_DedicatedServer)
	{
		// A stepped altar of dressed stone with an iron fire bowl burning in its path's colour (11-22: it was the engine's
		// plain cylinder with a ball on it). The cylinder stays as its collision, unseen.
		Pedestal->SetHiddenInGame(true);
		Flame->SetHiddenInGame(true);
		const FLinearColor PathCol = PathId == TEXT("fire") ? FLinearColor(1.f, 0.35f, 0.05f) : PathId == TEXT("frost") ? FLinearColor(0.35f, 0.7f, 1.f) : WRPathLook::Color(PathId);
		FRandomStream R(GetTypeHash(GetActorLocation().GridSnap(10.f)));
		TArray<FWRLookBlock> B;
		const float G = -50.f;   // the ground (the actor stands at the collision cylinder's middle)
		const auto Stone = [&](const FVector& Size, const FVector& At, float Yaw)
		{
			FWRLookBlock Bl;
			Bl.Size = Size;
			Bl.Centre = At;
			Bl.Rot = FRotator(R.FRandRange(-0.6f, 0.6f), Yaw + R.FRandRange(-1.5f, 1.5f), R.FRandRange(-0.6f, 0.6f));
			Bl.Mat = EWRLookMat::Stone;
			Bl.Tone = R.FRand();
			B.Add(Bl);
		};
		// Two tiers of four slabs each, a finger's gap between them, the lower sunk a little; the altar block and its
		// overhanging top slab; four short corner stones on the lower tier.
		for (int32 q = 0; q < 4; ++q)
		{
			const FVector2D S((q & 1) ? 1.f : -1.f, (q & 2) ? 1.f : -1.f);
			Stone(FVector(112.f, 112.f, 32.f), FVector(S.X * 57.f, S.Y * 57.f, G + 12.f), 0.f);
			Stone(FVector(77.f, 77.f, 34.f), FVector(S.X * 39.5f, S.Y * 39.5f, G + 28.f + 17.f), 0.f);
			Stone(FVector(24.f, 24.f, 30.f), FVector(S.X * 98.f, S.Y * 98.f, G + 28.f + 15.f), R.FRandRange(0.f, 20.f));
		}
		Stone(FVector(94.f, 94.f, 54.f), FVector(0.f, 0.f, G + 62.f + 27.f), 0.f);
		Stone(FVector(120.f, 120.f, 14.f), FVector(0.f, 0.f, G + 116.f + 7.f), 0.f);
		// The bowl: hammered iron, the coals glowing in the path's colour.
		FWRLookBlock Bowl;
		Bowl.Size = FVector(76.f, 76.f, 16.f);
		Bowl.Centre = FVector(0.f, 0.f, G + 130.f + 8.f);
		Bowl.Mat = EWRLookMat::Tinted;
		Bowl.Mesh = EWRLookMesh::Cylinder;
		Bowl.Color = FLinearColor(0.07f, 0.065f, 0.06f);
		Bowl.Metal = 0.85f;
		Bowl.Rough = 0.5f;
		Bowl.Pattern = 2.f;
		B.Add(Bowl);
		FWRLookBlock Coals = Bowl;
		Coals.Size = FVector(64.f, 64.f, 4.f);
		Coals.Centre.Z = G + 130.f + 15.f;
		Coals.Color = FMath::Lerp(FLinearColor(0.05f, 0.04f, 0.035f), PathCol, 0.35f);
		Coals.Metal = 0.f;
		Coals.Rough = 0.9f;
		Coals.Glow = 0.6f;
		Coals.Pattern = 0.f;
		B.Add(Coals);
		FWRBuildLook::Spawn(this, RootComponent, B, FLinearColor::Gray, true);
		USceneComponent* Fire = NewObject<USceneComponent>(this);
		Fire->SetupAttachment(RootComponent);
		Fire->SetUsingAbsoluteScale(true);
		Fire->SetRelativeLocation(FVector(0.f, 0.f, G + 150.f));
		Fire->RegisterComponent();
		WRFx::Attach(Fire, PathId == TEXT("frost") ? FName(TEXT("ff.blue_fire")) : FName(TEXT("brazier.fire")), NAME_None, PathId == TEXT("frost") ? 0.6f : 1.f);
		UPointLightComponent* L = NewObject<UPointLightComponent>(this);
		L->SetupAttachment(Fire);
		L->SetRelativeLocation(FVector(0.f, 0.f, 40.f));
		L->SetLightColor(PathCol);
		L->SetIntensity(6000.f);
		L->SetAttenuationRadius(900.f);
		L->SetCastShadows(false);
		L->RegisterComponent();
	}
	if (UMaterialInstanceDynamic* Mat = Flame->CreateAndSetMaterialInstanceDynamic(0))
	{
		// Fire and Frost keep their flames; any other path's altar (Lightning, Blood at Vharra's) burns in its own colour.
		Mat->SetVectorParameterValue(TEXT("Color"), PathId == TEXT("fire") ? FLinearColor(1.f, 0.35f, 0.05f) : PathId == TEXT("frost") ? FLinearColor(0.5f, 0.85f, 1.f) : WRPathLook::Color(PathId));
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
