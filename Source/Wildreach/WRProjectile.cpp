#include "WRProjectile.h"
#include "WRCrystalBosses.h"
#include "WRBoss.h"
#include "WRCrystalWorld.h"
#include "WRCrystalHazards.h"
#include "WRWindPath.h"
#include "WRVoidHazards.h"
#include "WRVoidWorld.h"
#include "WRSwampSystems.h"
#include "WRSwampBosses.h"
#include "WRForestBosses.h"
#include "WRGraveBosses.h"
#include "WRGraveHazards.h"
#include "WRForestHazards.h"
#include "WRRaiding.h"
#include "WRRaidLook.h"
#include "WRToolMesh.h"
#include "ProceduralMeshComponent.h"
#include "WRPortals.h"
#include "WRBuilding.h"
#include "WRTurret.h"
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
	DOREPLIFETIME(AWRProjectile, HomingOn);
	DOREPLIFETIME(AWRProjectile, HomingAcceleration);
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
	if (Trail || TrailFx.IsNone() || GetNetMode() == NM_DedicatedServer || IsActorBeingDestroyed() || !(HasActorBegunPlay() || IsActorBeginningPlay()))
	{
		return;
	}
	Trail = WRFx::Attach(Collision, TrailFx);
	if (Trail && ModelPath.IsEmpty())
	{
		Mesh->SetVisibility(false);  // the glowing core and its trail are the bolt now (a modelled shot keeps its model)
	}
}

void AWRProjectile::Init(const FWRHit& InHit, float Speed, const FLinearColor& Color)
{
	Hit = InHit;
	// A bolt, a spit or a shard flying straight: a reflector can send it back (cooks/biome-crystal.md 2.12). A shot that
	// was itself thrown back never bounces again.
	Hit.bReflectable = !InHit.bReaction;
	TintColor = Color;
	Movement->Velocity = GetActorForwardVector() * Speed;
	TrailFx = InHit.Element == WRTags::Element_Fire ? FName(TEXT("fire.bolt")) : InHit.Element == WRTags::Element_Frost ? FName(TEXT("frost.shard"))
		: InHit.Element == WRTags::Element_Lightning ? FName(TEXT("lightning.bolt")) : NAME_None;   // Spark (the Lightning path)
	OnRep_Color();
	OnRep_Fx();
}

void AWRProjectile::HomeOn(AActor* Target, float Acceleration)
{
	// Enemy projectiles never home (COMBAT-AND-LESSONS 1.9, the hard rule): only a player's (or its tame's) may curve.
	const AWRCharacterBase* By = Cast<AWRCharacterBase>(Hit.Instigator);
	if (!By || (!By->GetPlayerState() && By->GetTameOwner().IsEmpty()))
	{
		return;
	}
	HomingOn = Target;
	HomingAcceleration = Acceleration;
	OnRep_Homing();
}

void AWRProjectile::OnRep_Homing()
{
	USceneComponent* At = HomingOn ? HomingOn->GetRootComponent() : nullptr;
	Movement->HomingTargetComponent = At;
	Movement->HomingAccelerationMagnitude = HomingAcceleration;
	Movement->bIsHomingProjectile = At != nullptr && HomingAcceleration > 0.f;
}

FVector AWRProjectile::LobVelocity(const FVector& From, const FVector& To, float HorizontalSpeed, float Gravity)
{
	const FVector Flat = (To - From) * FVector(1.f, 1.f, 0.f);
	const float T = FMath::Max(0.25f, Flat.Size() / FMath::Max(100.f, HorizontalSpeed));
	const float G = 980.f * Gravity;
	// z(t) = vz t - g t^2 / 2 = dz  ->  vz = dz / t + g t / 2.
	return Flat / T + FVector(0.f, 0.f, (To.Z - From.Z) / T + 0.5f * G * T);
}

void AWRProjectile::InitLob(const FWRHit& InHit, const FVector& Velocity, float Gravity, float InSplashRadius, const FString& MeshPath)
{
	Hit = InHit;
	SplashRadius = InSplashRadius;
	Movement->ProjectileGravityScale = Gravity;
	Movement->MaxSpeed = FMath::Max(4000.f, Velocity.Size());
	Movement->Velocity = Velocity;
	SetLifeSpan(8.f);
	if (!MeshPath.IsEmpty())
	{
		ModelPath = MeshPath;
		OnRep_Model();
	}
	else
	{
		TintColor = InHit.Element == WRTags::Element_Frost ? FLinearColor(0.5f, 0.85f, 1.f) : FLinearColor(1.f, 0.45f, 0.05f);
		TrailFx = InHit.Element == WRTags::Element_Fire ? FName(TEXT("fire.bolt")) : InHit.Element == WRTags::Element_Frost ? FName(TEXT("frost.shard")) : NAME_None;
		OnRep_Color();
		OnRep_Fx();
	}
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
	if (ModelPath == TEXT("code:firepot"))
	{
		// The clay fire pot, modelled in code (WRRaidLook), its wick end trailing sparks.
		if (GetNetMode() == NM_DedicatedServer || CodeModel) return;
		CodeModel = NewObject<UProceduralMeshComponent>(this, TEXT("FirePot"));
		CodeModel->SetupAttachment(GetRootComponent());
		CodeModel->RegisterComponent();
		WRRaidLook::FirePot().ApplyTo(CodeModel);
		CodeModel->SetRelativeLocation(FVector(-6.f, 0.f, 0.f));
		Mesh->SetVisibility(false);
		TrailFx = TEXT("raid.fuse");
		OnRep_Fx();
		return;
	}
	if (ModelPath.StartsWith(TEXT("code:")))
	{
		// The Swamp's thrown things (WRSwampSystems): the salt pouch, the stormglass jar, modelled in code.
		if (GetNetMode() == NM_DedicatedServer || CodeModel) return;
		CodeModel = NewObject<UProceduralMeshComponent>(this, TEXT("ThrownModel"));
		CodeModel->SetupAttachment(GetRootComponent());
		CodeModel->RegisterComponent();
		// (And the Ancient forest's: the Bootlace blight jar, the smoke pot: WRForestHazards.)
		if (WRSwamp::BuildThrownModel(ModelPath, CodeModel) || WRVoidWorld::BuildThrownModel(ModelPath, CodeModel)
			|| WRForestHazards::BuildThrownModel(ModelPath, CodeModel) || UWRGraveHazards::BuildThrownModel(ModelPath, CodeModel))
		{
			Mesh->SetVisibility(false);
		}
		return;
	}
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
	// A lightning bolt landing in the Mother's Throat: on a Rodwarden rod, into the water, at someone wading it (Vharra
	// herself counts it as her blow lands: WRSwampVharra.cpp). Nothing happens anywhere else.
	if (Hit.Element == WRTags::Element_Lightning && !(Cast<AWRBoss>(Target) && FWRSwampBoss::IsVharra(Target)))
	{
		const FVector Landed = Target ? Target->GetActorLocation() - FVector(0.f, 0.f, Target->GetSimpleCollisionHalfHeight())
			: HitResult.bBlockingHit ? FVector(HitResult.ImpactPoint) : GetActorLocation();
		FWRSwampBoss::LightningAt(GetWorld(), Landed, Hit.Damage, Hit.Instigator, false);
	}
	// A fold's things: an arrow or a bolt drops an icicle, cuts a chute's rope, cracks a frost bolt (fire melts too).
	{
		const bool bFireShot = Hit.Element == WRTags::Element_Fire;
		AWRCharacterBase* By = const_cast<AWRCharacterBase*>(Caster);
		if (OtherActor) WRPortals::HitThing(OtherActor, By, Hit.Damage, bFireShot);
		// The forest's boss arenas: an arrow cuts a Bower rope anchor, splits a brood comb, bites a Root Knot or an amber heart.
		if (OtherActor && OtherComp) FWRForestBoss::HitThing(OtherActor, OtherComp, By, Hit.Damage, bFireShot);
		// The Graveyard's: an arrow cuts the Belfry rope, bursts an egg sac, parts the chandelier's chain.
		if (OtherActor && OtherComp) FWRGraveBoss::HitThing(OtherActor, OtherComp, By, Hit.Damage, bFireShot);
		// The Crystal caves': an arrow or a bolt rings a chime stone, turns a prism's hand-plate, cracks the flaring lens.
		if (OtherActor && OtherComp && Hit.Instigator != nullptr && !Cast<AWRBoss>(Hit.Instigator)) FWRCrystalBoss::HitThing(OtherActor, OtherComp, By, Hit.Damage, true);
		if (SplashRadius > 0.f || bFireShot)
		{
			const FVector At = HitResult.bBlockingHit ? FVector(HitResult.ImpactPoint) : GetActorLocation();
			for (TActorIterator<AWRFoldThing> It(GetWorld()); It; ++It)
			{
				if (*It != OtherActor && FVector::Dist(It->GetActorLocation(), At) < FMath::Max(SplashRadius, 150.f) + 100.f) WRPortals::HitThing(*It, By, Hit.Damage, bFireShot);
			}
		}
	}
	// The Crystal caves (WRCrystalThings): an arrow, a bolt or a thrown stone landing is a noise (a Glassleg bow's arrows
	// half one), and one that lands on a hall's keystone chime strikes it. A spell bolt rings like a cast.
	{
		const FVector At = HitResult.bBlockingHit ? FVector(HitResult.ImpactPoint) : GetActorLocation();
		AWRCharacterBase* By = const_cast<AWRCharacterBase*>(Caster);
		bool bQuiet = false;
		if (const UWRInventoryComponent* Inv = By ? UWRInventoryComponent::FindFor(By) : nullptr)
		{
			const FName Bow = Inv->GetEquipped(EWREquipSlot::Ranged).ItemId;
			bQuiet = !Bow.IsNone() && FWRTuning::Get().Str(TEXT("crystalWorld.quietBows"), TEXT("glassleg_bow")).Contains(Bow.ToString());
		}
		UWRCrystalWorld::ProjectileLanded(this, At, By, Kind, bQuiet);
		// A ward's lead keystone plate struck by it drops the ward (crystal 2.8): a hit on the plate, or close enough.
		if (AWRWardKeystone* Key = Cast<AWRWardKeystone>(OtherActor)) Key->Struck(By);
		else for (TActorIterator<AWRWardKeystone> KIt(GetWorld()); KIt; ++KIt) if (FVector::Dist(KIt->GetActorLocation(), At) < WRTune(TEXT("hazards.crystal.ward.keystoneReach"), 90.f)) { KIt->Struck(By); break; }
	}
	// The Ancient forest's things: an arrow brings a widowmaker limb down or stirs a wasp nest; fire landing (a fire
	// bolt, a fire pot) burns a lane through thornbrake and the barred door's bark (WRForestHazards).
	{
		const FVector At = HitResult.bBlockingHit ? FVector(HitResult.ImpactPoint) : GetActorLocation();
		AWRCharacterBase* By = const_cast<AWRCharacterBase*>(Caster);
		if (OtherActor && WRForestHazards::IsHazardThing(OtherActor)) WRForestHazards::HitThing(OtherActor, By, Hit.Damage, Hit.Element == WRTags::Element_Fire, At);
		if (Hit.Element == WRTags::Element_Fire || Kind == TEXT("firepot")) WRForestHazards::FireLanded(GetWorld(), At);
		// The forest's thrown jars break where they land, ballistic (never homing): blight, smoke.
		if (WRForestHazards::ThrownLanded(GetWorld(), Kind, At, By))
		{
			Destroy();
			return;
		}
	}
	// The Graveyard's lime pot breaks where it lands: a quicklime splash (WRGraveHazards; on the Ossuary Lord's pile it
	// stops the grave-light knitting the bones).
	if (Kind == TEXT("limepot"))
	{
		const FVector At = HitResult.bBlockingHit ? FVector(HitResult.ImpactPoint) : GetActorLocation();
		UWRGraveHazards::LimePotLanded(GetWorld(), At, const_cast<AWRCharacterBase*>(Caster), Hit.Damage);
		Destroy();
		return;
	}
	// The Swamp's thrown things break where they land: salt bursts, a stormglass jar shocks the water (WRSwampSystems).
	if (Kind == TEXT("saltpouch") || Kind == TEXT("stormjar"))
	{
		const FVector At = HitResult.bBlockingHit ? FVector(HitResult.ImpactPoint) : GetActorLocation();
		WRSwamp::ThrownLanded(this, Kind, At, OtherActor, const_cast<AWRCharacterBase*>(Caster));
		Destroy();
		return;
	}
	// The Wasteland's thrown things: a stone floats in a rift zone, salt bursts in a cloud (WRVoidWorld).
	if (WRVoidWorld::IsThrownKind(Kind))
	{
		const FVector At = HitResult.bBlockingHit ? FVector(HitResult.ImpactPoint) : GetActorLocation();
		WRVoidWorld::ThrownLanded(this, Kind, At, OtherActor, const_cast<AWRCharacterBase*>(Caster));
		Destroy();
		return;
	}
	// Fire landing near marsh gas lights it (a fire bolt's impact, a fire pot's burst): WRSwampSystems.
	if (Hit.Element == WRTags::Element_Fire || Kind == TEXT("firepot"))
	{
		WRSwamp::FireAt(this, HitResult.bBlockingHit ? FVector(HitResult.ImpactPoint) : GetActorLocation(), const_cast<AWRCharacterBase*>(Caster));
		AWRVoidState::FireAt(this, HitResult.bBlockingHit ? FVector(HitResult.ImpactPoint) : GetActorLocation());
	}
	// A thrown fire pot breaks wherever it lands: shards, fire, burning ground (WRRaiding).
	if (Kind == TEXT("firepot"))
	{
		const FVector At = HitResult.bBlockingHit ? FVector(HitResult.ImpactPoint) : GetActorLocation();
		FWRRaiding::FirePotBurst(this, At, const_cast<AWRCharacterBase*>(Caster));
		Destroy();
		return;
	}
	// A survivor's spell or arrow on someone else's building: the raid rule's spell / weapon share x its path.
	if (AWRBuildPiece* Piece = FWRRaiding::PieceOf(OtherActor); Piece && Caster && !Cast<AWRBuildPiece>(Hit.Instigator))
	{
		FWRRaiding::DamagePiece(Piece, Hit.Damage, ModelPath.IsEmpty() ? EWRRaidSource::Spell : EWRRaidSource::Weapon, this, FWRRaiding::Responsible(Caster),
			ModelPath.IsEmpty() ? Hit.PathId : NAME_None);
	}
	if (SplashRadius > 0.f)
	{
		// A lobbed thing bursts where it comes down: everyone hostile in the splash takes it (once), the ground shows it.
		const FVector At = HitResult.bBlockingHit ? FVector(HitResult.ImpactPoint) : GetActorLocation();
		for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
		{
			if (It->IsAlive() && (!Caster || (Caster->IsHostileTo(*It) && *It != Caster)) && FVector::Dist(It->GetActorLocation(), At) < SplashRadius + It->GetSimpleCollisionRadius() + It->GetSimpleCollisionHalfHeight() * 0.5f)
			{
				FWRHit Splash = Hit;
				Splash.Origin = At;
				Splash.bProjectile = true;
				UWRCombatLibrary::ApplyHit(*It, Splash);
			}
		}
		const bool bFire = Hit.Element == WRTags::Element_Fire, bFrost = Hit.Element == WRTags::Element_Frost;
		WRFx::Broadcast(this, bFire ? TEXT("fire.impact") : bFrost ? TEXT("frost.impact") : TEXT("stomp"), At, FRotator(90.f, 0.f, 0.f), FMath::Max(1.f, SplashRadius / 120.f),
			bFire ? TEXT("fire.impact") : bFrost ? TEXT("frost.impact") : TEXT("stomp"));
		if (OnLanded) OnLanded(At);
		Destroy();
		return;
	}
	// A turret's shot (build-building) hurts only what that turret may shoot at: never its builder, their clan or tames.
	const AWRBuildPiece* FromTurret = Cast<AWRBuildPiece>(Hit.Instigator);
	if (Target && (FromTurret ? UWRTurretSubsystem::MayHit(FromTurret, Target) : (!Caster || Caster->IsHostileTo(Target))))
	{
		Hit.Origin = GetActorLocation() - GetVelocity().GetSafeNormal() * 100.f;
		Hit.bProjectile = true;  // never parried (COMBAT-AND-LESSONS 1.3)
		TFunction<void(EWRHitResult)> Struck;
		if (OnStruck)
		{
			const TFunction<void(AWRCharacterBase*, EWRHitResult)> Cb = OnStruck;
			const TWeakObjectPtr<AWRCharacterBase> WeakT(Target);
			Struck = [Cb, WeakT](EWRHitResult Result) { if (WeakT.IsValid()) Cb(WeakT.Get(), Result); };
		}
		UWRCombatLibrary::ApplyHit(Target, Hit, Struck);
		if (Kind == TEXT("bola") && !FWRForestBoss::BolaHold(Target))   // the Hornwarden: a bola holds him like a snare
		{
			ApplyBola(Target);
		}
		// A shot that goes through bodies (Wind arrow, Piercing gale, an ammo's own pierce) flies on past this one.
		if (PierceLeft > 0 && !ModelPath.IsEmpty() && Kind != TEXT("bola"))
		{
			ContinuePast(Target);
			Destroy();
			return;
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
		// Windborne seeds (Nature + Wind 50): an arrow of yours coming down may plant a Sprout turret (WRWindPath.cpp).
		if ((Kind == TEXT("arrow") || Kind == TEXT("bolt")) && Caster)
		{
			WRWind::OnArrowLanded(const_cast<AWRCharacterBase*>(Caster), HitResult.ImpactPoint);
		}
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
		const float Radius = WRTune(TEXT("passives.explosiveBoltRadius"), 300.f) * (1.f + UWRPathComponent::Stat(Caster, TEXT("blastRadius")));  // Shockwave
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

void AWRProjectile::ContinuePast(AActor* Through)
{
	const FVector V = Movement && !Movement->Velocity.IsNearlyZero() ? Movement->Velocity : GetVelocity();
	if (!HasAuthority() || V.IsNearlyZero())
	{
		return;
	}
	const float Clear = Through ? Through->GetSimpleCollisionRadius() * 2.f + 30.f : 40.f;
	FActorSpawnParameters Params;
	Params.Owner = GetOwner();
	Params.Instigator = GetInstigator();
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWRProjectile* Next = GetWorld()->SpawnActor<AWRProjectile>(AWRProjectile::StaticClass(), GetActorLocation() + V.GetSafeNormal() * Clear, V.Rotation(), Params);
	if (!Next)
	{
		return;
	}
	Next->PierceLeft = PierceLeft - 1;
	Next->OnStruck = OnStruck;
	if (Through)
	{
		Next->Collision->IgnoreActorWhenMoving(Through, true);
	}
	Next->InitPhysical(Hit, V, Movement ? Movement->ProjectileGravityScale : 1.f, ModelPath, ModelAlong, Kind, PickupItem);
	if (!TrailFx.IsNone())
	{
		Next->SetTrail(TrailFx);
	}
}
