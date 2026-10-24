#include "WRProjectile.h"
#include "WRBoss.h"
#include "WRPathComponent.h"
#include "WRTuning.h"
#include "EngineUtils.h"
#include "WRCharacterBase.h"
#include "WRCombatLibrary.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

AWRProjectile::AWRProjectile()
{
	bReplicates = true;
	SetReplicateMovement(true);
	InitialLifeSpan = 3.f;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(14.f);
	Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Collision->SetNotifyRigidBodyCollision(true);
	RootComponent = Collision;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetRelativeScale3D(FVector(0.28f));
	if (SphereMesh.Succeeded())
	{
		Mesh->SetStaticMesh(SphereMesh.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (ShapeMaterial.Succeeded())
	{
		Mesh->SetMaterial(0, ShapeMaterial.Object);
	}

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->ProjectileGravityScale = 0.f;
	Movement->bRotationFollowsVelocity = true;
	Movement->InitialSpeed = 2600.f;
	Movement->MaxSpeed = 4000.f;
}

void AWRProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRProjectile, TintColor);
}

void AWRProjectile::BeginPlay()
{
	Super::BeginPlay();
	if (GetInstigator())
	{
		Collision->IgnoreActorWhenMoving(GetInstigator(), true);
	}
	Collision->OnComponentHit.AddDynamic(this, &AWRProjectile::OnHit);
	OnRep_Color();
}

void AWRProjectile::Init(const FWRHit& InHit, float Speed, const FLinearColor& Color)
{
	Hit = InHit;
	TintColor = Color;
	Movement->Velocity = GetActorForwardVector() * Speed;
	OnRep_Color();
}

void AWRProjectile::OnRep_Color()
{
	if (UMaterialInstanceDynamic* Mat = Mesh->CreateAndSetMaterialInstanceDynamic(0))
	{
		Mat->SetVectorParameterValue(TEXT("Color"), TintColor);
	}
}

void AWRProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& HitResult)
{
	if (!HasAuthority() || bSpent)
	{
		return;
	}
	bSpent = true;

	AWRCharacterBase* Target = Cast<AWRCharacterBase>(OtherActor);
	const AWRCharacterBase* Caster = Cast<AWRCharacterBase>(Hit.Instigator);
	if (Target && (!Caster || Caster->IsHostileTo(Target)))
	{
		Hit.Origin = GetActorLocation() - GetVelocity().GetSafeNormal() * 100.f;
		UWRCombatLibrary::ApplyHit(Target, Hit);
	}
	// Explosive bolt: fire bolts burst on impact.
	if (Caster && Hit.PathId == TEXT("fire") && UWRPathComponent::HasNode(Caster, TEXT("fire"), TEXT("explosive_bolt")))
	{
		const float Radius = WRTune(TEXT("passives.explosiveBoltRadius"), 300.f);
		AWRGroundMarker::Spawn(GetWorld(), GetActorLocation(), FRotator::ZeroRotator, FVector2D(Radius, Radius), false, FLinearColor(1.f, 0.4f, 0.05f), 0.35f);
		for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
		{
			if (*It != Target && *It != Caster && It->IsAlive() && Caster->IsHostileTo(*It) && FVector::Dist(It->GetActorLocation(), GetActorLocation()) < Radius)
			{
				FWRHit Splash = Hit;
				Splash.Damage *= WRTune(TEXT("passives.explosiveBoltDamage"), 0.5f);
				Splash.BurnBuildup *= 0.5f;
				Splash.Origin = GetActorLocation();
				UWRCombatLibrary::ApplyHit(*It, Splash);
			}
		}
	}
	Destroy();
}
