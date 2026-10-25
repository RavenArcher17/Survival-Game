#include "WRCharacterBase.h"
#include "WRSocial.h"
#include "Wildreach.h"
#include "WRAttributeSet.h"
#include "WRCharacterMovementComponent.h"
#include "WRGameplayTags.h"
#include "WRStatusComponent.h"
#include "WRTuning.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerState.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

FWROnHitShown AWRCharacterBase::OnHitShown;

AWRCharacterBase::AWRCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UWRCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	// The shapes default to a grid material with no colour input; this one has a "Color" parameter.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(RootComponent);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (CylinderMesh.Succeeded()) BodyMesh->SetStaticMesh(CylinderMesh.Object);

	FaceMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FaceMarker"));
	FaceMarker->SetupAttachment(RootComponent);
	FaceMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (CubeMesh.Succeeded()) FaceMarker->SetStaticMesh(CubeMesh.Object);

	// Placeholder blade on the right side; its pivot swings for attacks.
	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(RootComponent);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (CubeMesh.Succeeded()) WeaponMesh->SetStaticMesh(CubeMesh.Object);

	if (ShapeMaterial.Succeeded())
	{
		BodyMesh->SetMaterial(0, ShapeMaterial.Object);
		FaceMarker->SetMaterial(0, ShapeMaterial.Object);
		WeaponMesh->SetMaterial(0, ShapeMaterial.Object);
	}

	Status = CreateDefaultSubobject<UWRStatusComponent>(TEXT("Status"));
}

void AWRCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	// Fit the placeholder body to whatever capsule the subclass chose.
	const float Radius = GetCapsuleComponent()->GetUnscaledCapsuleRadius();
	const float HalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	BodyMesh->SetRelativeScale3D(FVector(Radius / 50.f, Radius / 50.f, HalfHeight / 50.f));
	FaceMarker->SetRelativeLocation(FVector(Radius, 0.f, HalfHeight * 0.6f));
	FaceMarker->SetRelativeScale3D(FVector(0.2f, 0.4f, 0.2f));
	WeaponMesh->SetRelativeLocation(FVector(30.f, Radius + 15.f, 10.f));
	WeaponMesh->SetRelativeScale3D(FVector(1.1f, 0.06f, 0.12f));

	BodyMaterial = BodyMesh->CreateAndSetMaterialInstanceDynamic(0);
	SetBodyColor(BaseBodyColor);
}

void AWRCharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRCharacterBase, Weapon);
	DOREPLIFETIME_CONDITION(AWRCharacterBase, SwingSeq, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AWRCharacterBase, SwingKindRep, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AWRCharacterBase, SwingDurationRep, COND_SkipOwner);
}

UAbilitySystemComponent* AWRCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

UWRAttributeSet* AWRCharacterBase::GetAttributes() const
{
	return Attributes;
}

bool AWRCharacterBase::IsAlive() const
{
	return Attributes && Attributes->GetHealth() > 0.f && !HasTag(WRTags::State_Dead);
}

bool AWRCharacterBase::HasTag(const FGameplayTag& Tag) const
{
	return AbilitySystem && AbilitySystem->HasMatchingGameplayTag(Tag);
}

float AWRCharacterBase::GetSpeedMultiplier() const
{
	if (!AbilitySystem || HasTag(WRTags::State_Dead) || HasTag(WRTags::Status_Frozen) || HasTag(WRTags::Status_Staggered) || HasTag(WRTags::Status_Unconscious))
	{
		return 0.f;
	}
	// Attack/block slowdowns are predicted flags on the movement component, not tags.
	return HasTag(WRTags::Status_Slowed) ? WRTune(TEXT("status.slowMultiplier"), 0.6f) : 1.f;
}

void AWRCharacterBase::InitVitals(float MaxHealth, float MaxStamina, float MaxPoise)
{
	if (!HasAuthority() || !AbilitySystem)
	{
		return;
	}
	AbilitySystem->SetNumericAttributeBase(UWRAttributeSet::GetMaxHealthAttribute(), MaxHealth);
	AbilitySystem->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), MaxHealth);
	AbilitySystem->SetNumericAttributeBase(UWRAttributeSet::GetMaxStaminaAttribute(), MaxStamina);
	AbilitySystem->SetNumericAttributeBase(UWRAttributeSet::GetStaminaAttribute(), MaxStamina);
	AbilitySystem->SetNumericAttributeBase(UWRAttributeSet::GetMaxPoiseAttribute(), MaxPoise);
	AbilitySystem->SetNumericAttributeBase(UWRAttributeSet::GetPoiseAttribute(), MaxPoise);
	AbilitySystem->SetNumericAttributeBase(UWRAttributeSet::GetBurnBuildupAttribute(), 0.f);
	AbilitySystem->SetNumericAttributeBase(UWRAttributeSet::GetFrostBuildupAttribute(), 0.f);
	AbilitySystem->SetNumericAttributeBase(UWRAttributeSet::GetHeatAttribute(), 0.f);
	AbilitySystem->SetNumericAttributeBase(UWRAttributeSet::GetChillAttribute(), 0.f);
}

FGameplayAbilitySpecHandle AWRCharacterBase::GrantAbility(TSubclassOf<UGameplayAbility> AbilityClass, EWRAbilityInput Input, int32 Level)
{
	if (!HasAuthority() || !AbilitySystem || !AbilityClass)
	{
		return FGameplayAbilitySpecHandle();
	}
	FGameplayAbilitySpec Spec(AbilityClass, Level, static_cast<int32>(Input), this);
	return AbilitySystem->GiveAbility(Spec);
}

void AWRCharacterBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateVisuals(DeltaSeconds);

	// Blocking keeps you facing where you aim, so the guard covers what you're looking at.
	if (Controller && HasTag(WRTags::State_Blocking))
	{
		SetActorRotation(FRotator(0.f, GetControlRotation().Yaw, 0.f));
	}

	if (HasAuthority() && Status)
	{
		const UWRCharacterMovementComponent* Move = Cast<UWRCharacterMovementComponent>(GetCharacterMovement());
		Status->SetSprintDraining(Move && Move->IsSprinting() && GetVelocity().SizeSquared2D() > FMath::Square(10.f));
	}
}

void AWRCharacterBase::Die(AActor* Killer)
{
	if (!HasAuthority() || HasTag(WRTags::State_Dead) || !AbilitySystem)
	{
		return;
	}
	AbilitySystem->CancelAllAbilities();
	AbilitySystem->AddLooseGameplayTag(WRTags::State_Dead, 1, EGameplayTagReplicationState::TagOnly);
	GetCharacterMovement()->DisableMovement();
	UE_LOG(LogWildreach, Log, TEXT("DEATH %s killed by %s"), *GetName(), *GetNameSafe(Killer));
	MulticastDied();
	OnDeath(Killer);
}

bool AWRCharacterBase::IsHostileTo(const AWRCharacterBase* Other) const
{
	if (!Other || Other == this)
	{
		return false;
	}
	// A survivor never fights their own (or a clanmate's) tames; survivors fight each other only where PvP allows.
	const APlayerState* PS = GetPlayerState();
	const FString OtherOwner = Other->GetTameOwner();
	if (PS && !OtherOwner.IsEmpty() && FWRSocialRules::CanShare(OtherOwner, PS->GetPlayerName()))
	{
		return false;
	}
	if (PS && Other->GetPlayerState())
	{
		return FWRSocialRules::PlayersCanFight(this, Other);
	}
	return true;
}

FString AWRCharacterBase::GetDisplayLabel() const
{
	const APlayerState* PS = GetPlayerState();
	return PS ? PS->GetPlayerName() : GetName();
}

void AWRCharacterBase::ReportHit(float Damage, EWRHitResult Result)
{
	if (HasAuthority())
	{
		MulticastHit(Damage, Result);
	}
}

void AWRCharacterBase::MulticastHit_Implementation(float Damage, EWRHitResult Result)
{
	HitFlash = Result == EWRHitResult::Hit ? 0.15f : 0.f;
	OnHitVisual(Result);
	OnHitShown.Broadcast(this, Damage, Result);
}

void AWRCharacterBase::MulticastDied_Implementation()
{
	// Placeholder death: tip the body over and grey it out.
	BodyMesh->SetRelativeRotation(FRotator(0.f, 0.f, 90.f));
	SetBodyColor(FLinearColor(0.15f, 0.15f, 0.15f));
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	OnDeathVisual();
}

void AWRCharacterBase::PlaySwing(EWRSwingKind Kind, float Duration)
{
	if (IsLocallyControlled() || GetNetMode() != NM_DedicatedServer)
	{
		StartSwingVisual(Kind, Duration);
	}
	if (HasAuthority())
	{
		SwingKindRep = Kind;
		SwingDurationRep = Duration;
		++SwingSeq;
	}
}

void AWRCharacterBase::OnRep_Swing()
{
	StartSwingVisual(SwingKindRep, SwingDurationRep);
}

void AWRCharacterBase::StartSwingVisual(EWRSwingKind Kind, float Duration)
{
	VisualSwing = Kind;
	VisualSwingTime = 0.f;
	VisualSwingDuration = FMath::Max(Duration, 0.05f);
	OnSwingVisual(Kind, Duration);
}

void AWRCharacterBase::SetBodyColor(const FLinearColor& Color)
{
	if (BodyMaterial)
	{
		BodyMaterial->SetVectorParameterValue(TEXT("Color"), Color);
	}
}

void AWRCharacterBase::UpdateVisuals(float DeltaSeconds)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// Blade swing: sweep from right-back to left-front over the swing's duration.
	float Yaw = 60.f;
	float Pitch = 0.f;
	if (VisualSwing != EWRSwingKind::None)
	{
		VisualSwingTime += DeltaSeconds;
		const float T = FMath::Clamp(VisualSwingTime / VisualSwingDuration, 0.f, 1.f);
		const float Ease = FMath::InterpEaseInOut(0.f, 1.f, T, 2.f);
		if (VisualSwing == EWRSwingKind::Heavy || VisualSwing == EWRSwingKind::WeaponArt)
		{
			Pitch = FMath::Lerp(70.f, -20.f, Ease);
			Yaw = 0.f;
		}
		else if (VisualSwing == EWRSwingKind::Cast)
		{
			Yaw = 0.f;
			Pitch = 20.f;
		}
		else
		{
			Yaw = FMath::Lerp(80.f, -70.f, Ease);
		}
		if (T >= 1.f)
		{
			VisualSwing = EWRSwingKind::None;
		}
	}
	if (!bArtWeapon)
	{
		WeaponMesh->SetRelativeRotation(FRotator(Pitch, Yaw, 0.f));
	}

	// Status tint so placeholder bodies read at a glance.
	FLinearColor Color = BaseBodyColor;
	if (HasTag(WRTags::State_Dead)) Color = FLinearColor(0.15f, 0.15f, 0.15f);
	else if (HasTag(WRTags::Status_Frozen)) Color = FLinearColor(0.6f, 0.9f, 1.f);
	else if (HasTag(WRTags::Status_Staggered)) Color = FLinearColor(1.f, 1.f, 0.3f);
	else if (HasTag(WRTags::Status_Burn)) Color = FLinearColor::LerpUsingHSV(BaseBodyColor, FLinearColor(1.f, 0.35f, 0.f), 0.6f);
	else if (HasTag(WRTags::Status_Slowed)) Color = FLinearColor::LerpUsingHSV(BaseBodyColor, FLinearColor(0.4f, 0.6f, 1.f), 0.5f);
	if (HitFlash > 0.f)
	{
		HitFlash -= DeltaSeconds;
		Color = FLinearColor::White;
	}
	SetBodyColor(Color);
}
