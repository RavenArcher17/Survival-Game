#include "WRCharacter.h"
#include "WRCharacterMovementComponent.h"
#include "WRGameMode.h"
#include "WRSurvivalComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

AWRCharacter::AWRCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UWRCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.f, 540.f, 0.f);
	Move->MaxWalkSpeed = 450.f;
	Move->JumpZVelocity = 500.f;
	Move->AirControl = 0.35f;

	ThirdPersonBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("ThirdPersonBoom"));
	ThirdPersonBoom->SetupAttachment(RootComponent);
	ThirdPersonBoom->TargetArmLength = 350.f;
	ThirdPersonBoom->SocketOffset = FVector(0.f, 50.f, 60.f);
	ThirdPersonBoom->bUsePawnControlRotation = true;

	ThirdPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ThirdPersonCamera"));
	ThirdPersonCamera->SetupAttachment(ThirdPersonBoom, USpringArmComponent::SocketName);
	ThirdPersonCamera->bUsePawnControlRotation = false;

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(RootComponent);
	FirstPersonCamera->SetRelativeLocation(FVector(10.f, 0.f, 64.f));
	FirstPersonCamera->bUsePawnControlRotation = true;
	FirstPersonCamera->bAutoActivate = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(RootComponent);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetRelativeScale3D(FVector(0.84f, 0.84f, 1.92f));
	if (CylinderMesh.Succeeded())
	{
		BodyMesh->SetStaticMesh(CylinderMesh.Object);
	}

	// Shows which way a placeholder survivor is facing.
	FaceMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FaceMarker"));
	FaceMarker->SetupAttachment(RootComponent);
	FaceMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FaceMarker->SetRelativeLocation(FVector(40.f, 0.f, 60.f));
	FaceMarker->SetRelativeScale3D(FVector(0.2f, 0.4f, 0.2f));
	if (CubeMesh.Succeeded())
	{
		FaceMarker->SetStaticMesh(CubeMesh.Object);
	}

	Survival = CreateDefaultSubobject<UWRSurvivalComponent>(TEXT("Survival"));
}

void AWRCharacter::BeginPlay()
{
	Super::BeginPlay();

	ApplyViewMode();
	if (HasAuthority())
	{
		Survival->OnDied.AddDynamic(this, &AWRCharacter::HandleDied);
	}
}

void AWRCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRCharacter, bFirstPerson);
}

void AWRCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority())
	{
		const UWRCharacterMovementComponent* Move = CastChecked<UWRCharacterMovementComponent>(GetCharacterMovement());
		Survival->SetStaminaDraining(Move->IsSprinting() && GetVelocity().SizeSquared2D() > FMath::Square(10.f));
	}
}

bool AWRCharacter::CanSprint() const
{
	return Survival && Survival->IsAlive() && Survival->HasStamina();
}

void AWRCharacter::SetFirstPerson(bool bNewFirstPerson)
{
	bFirstPerson = bNewFirstPerson;
	ApplyViewMode();
	if (!HasAuthority())
	{
		ServerSetFirstPerson(bNewFirstPerson);
	}
}

void AWRCharacter::ServerSetFirstPerson_Implementation(bool bNewFirstPerson)
{
	bFirstPerson = bNewFirstPerson;
	ApplyViewMode();
}

void AWRCharacter::OnRep_FirstPerson()
{
	ApplyViewMode();
}

void AWRCharacter::ApplyViewMode()
{
	FirstPersonCamera->SetActive(bFirstPerson);
	ThirdPersonCamera->SetActive(!bFirstPerson);
	BodyMesh->SetOwnerNoSee(bFirstPerson);
	FaceMarker->SetOwnerNoSee(bFirstPerson);

	// First person: body yaw follows the camera. Third person: body turns toward movement.
	bUseControllerRotationYaw = bFirstPerson;
	GetCharacterMovement()->bOrientRotationToMovement = !bFirstPerson;
}

void AWRCharacter::HandleDied()
{
	UE_LOG(LogTemp, Log, TEXT("%s died"), *GetName());

	GetCharacterMovement()->DisableMovement();
	if (AWRGameMode* GameMode = GetWorld()->GetAuthGameMode<AWRGameMode>())
	{
		GameMode->RequestRespawn(Controller);
	}
	DetachFromControllerPendingDestroy();
	SetLifeSpan(5.f);
}

void AWRCharacter::BuildInput()
{
	if (MappingContext)
	{
		return;
	}

	auto MakeAction = [this](EInputActionValueType ValueType)
	{
		UInputAction* Action = NewObject<UInputAction>(this);
		Action->ValueType = ValueType;
		return Action;
	};

	MoveAction = MakeAction(EInputActionValueType::Axis2D);
	LookAction = MakeAction(EInputActionValueType::Axis2D);
	JumpAction = MakeAction(EInputActionValueType::Boolean);
	SprintAction = MakeAction(EInputActionValueType::Boolean);
	ToggleViewAction = MakeAction(EInputActionValueType::Boolean);

	MappingContext = NewObject<UInputMappingContext>(this);

	// WASD -> Axis2D (X = right, Y = forward).
	MappingContext->MapKey(MoveAction, EKeys::W).Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(this));
	FEnhancedActionKeyMapping& Back = MappingContext->MapKey(MoveAction, EKeys::S);
	Back.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(this));
	Back.Modifiers.Add(NewObject<UInputModifierNegate>(this));
	MappingContext->MapKey(MoveAction, EKeys::A).Modifiers.Add(NewObject<UInputModifierNegate>(this));
	MappingContext->MapKey(MoveAction, EKeys::D);

	MappingContext->MapKey(LookAction, EKeys::Mouse2D);
	MappingContext->MapKey(JumpAction, EKeys::SpaceBar);
	MappingContext->MapKey(SprintAction, EKeys::LeftShift);
	MappingContext->MapKey(ToggleViewAction, EKeys::V);
}

void AWRCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	BuildInput();

	if (const APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->ClearAllMappings();
			Subsystem->AddMappingContext(MappingContext, 0);
		}
	}

	if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AWRCharacter::Move);
		Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &AWRCharacter::Look);
		Input->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		Input->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		Input->BindAction(SprintAction, ETriggerEvent::Started, this, &AWRCharacter::StartSprint);
		Input->BindAction(SprintAction, ETriggerEvent::Completed, this, &AWRCharacter::StopSprint);
		Input->BindAction(ToggleViewAction, ETriggerEvent::Started, this, &AWRCharacter::ToggleView);
	}
}

void AWRCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (!Controller)
	{
		return;
	}

	const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
	const FRotationMatrix YawMatrix(YawRotation);
	AddMovementInput(YawMatrix.GetUnitAxis(EAxis::X), Axis.Y);
	AddMovementInput(YawMatrix.GetUnitAxis(EAxis::Y), Axis.X);
}

void AWRCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(-Axis.Y);
}

void AWRCharacter::StartSprint()
{
	CastChecked<UWRCharacterMovementComponent>(GetCharacterMovement())->bWantsToSprint = true;
}

void AWRCharacter::StopSprint()
{
	CastChecked<UWRCharacterMovementComponent>(GetCharacterMovement())->bWantsToSprint = false;
}

void AWRCharacter::ToggleView()
{
	SetFirstPerson(!bFirstPerson);
}
