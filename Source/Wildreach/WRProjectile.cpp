#include "WRProjectile.h"
#include "Wildreach.h"
#include "GameFramework/PlayerState.h"
#include "WRBoss.h"
#include "WRPathComponent.h"
#include "WRTuning.h"
#include "EngineUtils.h"
#include "WRCharacterBase.h"
#include "WRCombatLibrary.h"
#include "WRGameplayTags.h"
#include "WRInventory.h"
#include "WRFx.h"
#include "WRSocial.h"
#include "WRStatusComponent.h"
#include "Engine/StaticMesh.h"
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
	DOREPLIFETIME(AWRProjectile, ModelPath);
	DOREPLIFETIME(AWRProjectile, ModelAlong);
	DOREPLIFETIME(AWRProjectile, TrailFx);
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
	OnRep_Fx();
}

void AWRProjectile::EndPlay(const EEndPlayReason::Type Reason)
{
	// The trail stops; what it left behind fades on its own (the impact came from the server, see OnHit).
	if (Trail)
	{
		WRFx::Stop(Trail);
		Trail = nullptr;
	}
	Super::EndPlay(Reason);
}

void AWRProjectile::OnRep_Fx()
{
	// Before BeginPlay (initial replication) BeginPlay calls this again; during it, IsActorBeginningPlay.
	if (Trail || TrailFx.IsNone() || !ModelPath.IsEmpty() || GetNetMode() == NM_DedicatedServer || IsActorBeingDestroyed() || !(HasActorBegunPlay() || IsActorBeginningPlay()))
	{
		return;
	}
	Trail = WRFx::Attach(Collision, TrailFx);
	if (Trail)
	{
		Mesh->SetVisibility(false);  // the glowing core and its trail are the bolt now
	}
}

void AWRProjectile::Init(const FWRHit& InHit, float Speed, const FLinearColor& Color)
{
	Hit = InHit;
	TintColor = Color;
	Movement->Velocity = GetActorForwardVector() * Speed;
	TrailFx = InHit.Element == WRTags::Element_Fire ? FName(TEXT("fire.bolt")) : InHit.Element == WRTags::Element_Frost ? FName(TEXT("frost.shard")) : NAME_None;
	OnRep_Color();
	OnRep_Fx();
}

void AWRProjectile::InitPhysical(const FWRHit& InHit, const FVector& Velocity, float Gravity, const FString& MeshPath, const FString& MeshAlong, FName InKind, FName InPickupItem)
{
	Hit = InHit;
	Kind = InKind;
	PickupItem = InPickupItem;
	Collision->SetSphereRadius(WRTune(TEXT("ranged.projectileRadius"), 6.f));
	Movement->ProjectileGravityScale = Gravity;
	Movement->MaxSpeed = FMath::Max(4000.f, Velocity.Size());
	Movement->Velocity = Velocity;
	SetLifeSpan(WRTune(TEXT("ranged.flightSeconds"), 8.f));
	ModelAlong = MeshAlong;
	ModelPath = MeshPath;
	OnRep_Model();
}

void AWRProjectile::OnRep_Model()
{
	UStaticMesh* Model = ModelPath.IsEmpty() ? nullptr : LoadObject<UStaticMesh>(nullptr, *ModelPath);
	if (!Model)
	{
		return;
	}
	// The model's tip points along the flight (the actor's X, which follows the velocity).
	const FBox Box = Model->GetBoundingBox();
	const FVector Size = Box.GetSize();
	const int32 A = Size.X >= Size.Y && Size.X >= Size.Z ? 0 : (Size.Y >= Size.Z ? 1 : 2);
	FVector Tip = FVector::ZeroVector;
	Tip[A] = Box.Max[A] >= -Box.Min[A] ? 1.f : -1.f;
	if (ModelAlong.Len() == 2)
	{
		const float Sign = ModelAlong[0] == TEXT('-') ? -1.f : 1.f;
		const TCHAR C = FChar::ToUpper(ModelAlong[1]);
		Tip = C == TEXT('X') ? FVector(Sign, 0.f, 0.f) : C == TEXT('Y') ? FVector(0.f, Sign, 0.f) : FVector(0.f, 0.f, Sign);
	}
	// The tip sits on the actor's origin (where the collision sphere is), so hits land at the tip and a stuck arrow
	// goes in point first instead of from its grip.
	const int32 TipAxis = FMath::Abs(Tip.X) > 0.5f ? 0 : (FMath::Abs(Tip.Y) > 0.5f ? 1 : 2);
	const float TipLength = Tip[TipAxis] > 0.f ? Box.Max[TipAxis] : -Box.Min[TipAxis];
	Mesh->SetStaticMesh(Model);
	Mesh->EmptyOverrideMaterials();
	Mesh->SetRelativeScale3D(FVector::OneVector);
	Mesh->SetRelativeRotation(FQuat(FRotationMatrix::MakeFromX(Tip)).Inverse().Rotator());
	Mesh->SetRelativeLocation(FVector(-TipLength, 0.f, 0.f));
}

void AWRProjectile::ApplyBola(AWRCharacterBase* Target)
{
	if (!Target || !Target->IsAlive() || !Target->Status)
	{
		return;
	}
	const bool bBig = Target->GetSimpleCollisionRadius() > WRTune(TEXT("ranged.bolaRootMaxRadius"), 90.f);
	Target->Status->ApplyStatus(bBig ? WRTags::Status_Slowed : WRTags::Status_Rooted,
		bBig ? WRTune(TEXT("ranged.bolaSlowSeconds"), 4.f) : WRTune(TEXT("ranged.bolaRootSeconds"), 4.f));
	UE_LOG(LogWildreach, Log, TEXT("RANGED bola %s %s"), bBig ? TEXT("slows") : TEXT("roots"), *Target->GetName());
}

void AWRProjectile::OnRep_Color()
{
	if (!ModelPath.IsEmpty())
	{
		return;  // a model keeps its own materials
	}
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
		if (Kind == TEXT("bola"))
		{
			ApplyBola(Target);
		}
	}
	if (!ModelPath.IsEmpty())
	{
		UE_LOG(LogWildreach, Log, TEXT("RANGED %s hit %s"), *Kind.ToString(), *GetNameSafe(OtherActor));
		if (!Target)
		{
			// An arrow into the ground or a wall kicks up a little dust (a body shows its own hit).
			WRFx::Broadcast(this, TEXT("hit.dust"), HitResult.ImpactPoint, HitResult.ImpactNormal.Rotation(), 0.45f, TEXT("arrow.hit"));
		}
	}
	else
	{
		// A spell bursts where it struck; an explosive bolt bursts bigger.
		const bool bFire = Hit.Element == WRTags::Element_Fire;
		const bool bExplosive = Caster && Hit.PathId == TEXT("fire") && UWRPathComponent::HasNode(Caster, TEXT("fire"), TEXT("explosive_bolt"));
		const FVector At = HitResult.bBlockingHit ? FVector(HitResult.ImpactPoint) : GetActorLocation();
		const FVector Normal = HitResult.bBlockingHit ? FVector(HitResult.ImpactNormal) : -GetVelocity().GetSafeNormal();
		WRFx::Broadcast(this, bFire ? TEXT("fire.impact") : TEXT("frost.impact"), At, Normal.Rotation(), bExplosive ? 2.f : 1.f, bFire ? TEXT("fire.impact") : TEXT("frost.impact"));
	}
	if (!ModelPath.IsEmpty() && Kind != TEXT("bola"))
	{
		// Arrows, bolts, darts and spears stick where they land, tip first.
		const FVector Velocity = GetVelocity();
		Movement->StopMovementImmediately();
		Movement->Deactivate();
		Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SetActorLocation(HitResult.ImpactPoint + Velocity.GetSafeNormal() * WRTune(TEXT("ranged.stickDepth"), 6.f));
		if (OtherComp && OtherComp->Mobility == EComponentMobility::Movable)
		{
			AttachToComponent(OtherComp, FAttachmentTransformRules::KeepWorldTransform);
		}
		SetLifeSpan(WRTune(TEXT("ranged.stuckSeconds"), 12.f));
		if (!PickupItem.IsNone())
		{
			// A thrown spear can be picked up again where it fell.
			FHitResult Ground;
			const FVector From = HitResult.ImpactPoint + FVector(0.f, 0.f, 50.f);
			FCollisionQueryParams GroundQuery(SCENE_QUERY_STAT(WRSpearDrop), false, this);
			GroundQuery.AddIgnoredActor(OtherActor);
			const FVector At = GetWorld()->LineTraceSingleByChannel(Ground, From, From - FVector(0.f, 0.f, 3000.f), ECC_WorldStatic, GroundQuery) ? Ground.ImpactPoint : HitResult.ImpactPoint;
			if (AWRLootBag* Bag = GetWorld()->SpawnActor<AWRLootBag>(AWRLootBag::StaticClass(), At, FRotator::ZeroRotator))
			{
				Bag->bOwnerOnly = false;
				Bag->OwnerName = Caster && Caster->GetPlayerState() ? Caster->GetPlayerState()->GetPlayerName() : FString();
				Bag->Items.Add(FWRInventoryRules::MakeInstance(PickupItem));
				UE_LOG(LogWildreach, Log, TEXT("RANGED %s landed; left to pick up"), *PickupItem.ToString());
			}
		}
		return;
	}
	// Explosive bolt: fire bolts burst on impact.
	if (Caster && Hit.PathId == TEXT("fire") && UWRPathComponent::HasNode(Caster, TEXT("fire"), TEXT("explosive_bolt")))
	{
		const float Radius = WRTune(TEXT("passives.explosiveBoltRadius"), 300.f);
		// (The burst is the impact effect at twice the size, sent above; no flat marker disc.)
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
