#include "WRCharacter.h"
#include "Wildreach.h"
#include "WRAbilities.h"
#include "WRAttributeSet.h"
#include "WRCharacterMovementComponent.h"
#include "WRGameMode.h"
#include "WRGameplayTags.h"
#include "WRPathComponent.h"
#include "WRPlayerState.h"
#include "WRStatusComponent.h"
#include "WRTuning.h"
#include "AbilitySystemComponent.h"
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

AWRCharacter::AWRCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
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
	// Over-the-shoulder so your own body doesn't hide what you're aiming at.
	ThirdPersonBoom->TargetArmLength = 420.f;
	ThirdPersonBoom->SocketOffset = FVector(0.f, 75.f, 85.f);
	ThirdPersonBoom->bUsePawnControlRotation = true;

	ThirdPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ThirdPersonCamera"));
	ThirdPersonCamera->SetupAttachment(ThirdPersonBoom, USpringArmComponent::SocketName);
	ThirdPersonCamera->bUsePawnControlRotation = false;

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(RootComponent);
	FirstPersonCamera->SetRelativeLocation(FVector(10.f, 0.f, 64.f));
	FirstPersonCamera->bUsePawnControlRotation = true;
	FirstPersonCamera->bAutoActivate = false;

	BaseBodyColor = FLinearColor(0.55f, 0.6f, 0.7f);
	Status->bSurvivalNeeds = true;
}

void AWRCharacter::BeginPlay()
{
	Super::BeginPlay();
	ApplyViewMode();
	Cast<UWRCharacterMovementComponent>(GetCharacterMovement())->SprintSpeed = WRTune(TEXT("player.sprintSpeed"), 750.f);
	GetCharacterMovement()->MaxWalkSpeed = WRTune(TEXT("player.walkSpeed"), 450.f);
}

void AWRCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRCharacter, bFirstPerson);
}

void AWRCharacter::InitAbilityActorInfo()
{
	AWRPlayerState* PS = GetPlayerState<AWRPlayerState>();
	if (!PS)
	{
		return;
	}
	AbilitySystem = PS->GetAbilitySystemComponent();
	Attributes = PS->GetAttributes();
	AbilitySystem->InitAbilityActorInfo(PS, this);
}

void AWRCharacter::ClearStaleTags()
{
	// The ASC lives on the PlayerState, so tags from the previous body would otherwise survive death.
	static const FGameplayTag Stale[] = {
		WRTags::State_Dead, WRTags::State_Invulnerable, WRTags::State_Parrying,
		WRTags::Status_Burn, WRTags::Status_Frostbite, WRTags::Status_Slowed, WRTags::Status_Frozen, WRTags::Status_Staggered };
	for (const FGameplayTag& Tag : Stale)
	{
		AbilitySystem->SetLooseGameplayTagCount(Tag, 0, EGameplayTagReplicationState::TagOnly);
	}
}

void AWRCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	InitAbilityActorInfo();

	AWRPlayerState* PS = GetPlayerState<AWRPlayerState>();
	if (!PS || !AbilitySystem)
	{
		return;
	}

	ClearStaleTags();
	if (!PS->bDefaultAbilitiesGranted)
	{
		PS->bDefaultAbilitiesGranted = true;
		GrantAbility(UWRGA_LightAttack::StaticClass(), EWRAbilityInput::Light);
		GrantAbility(UWRGA_HeavyAttack::StaticClass(), EWRAbilityInput::Heavy);
		GrantAbility(UWRGA_Dodge::StaticClass(), EWRAbilityInput::Dodge);
		GrantAbility(UWRGA_Block::StaticClass(), EWRAbilityInput::Block);
	}

	InitVitals(WRTune(TEXT("player.health"), 100.f), WRTune(TEXT("player.stamina"), 100.f), WRTune(TEXT("player.poise"), 40.f));
	AbilitySystem->SetNumericAttributeBase(UWRAttributeSet::GetHungerAttribute(), 100.f);
	AbilitySystem->SetNumericAttributeBase(UWRAttributeSet::GetThirstAttribute(), 100.f);
	PS->GetPaths()->ApplyToCharacter(this);
	// Path level can raise max health; respawn at full.
	AbilitySystem->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), Attributes->GetMaxHealth());
	AbilitySystem->SetNumericAttributeBase(UWRAttributeSet::GetStaminaAttribute(), Attributes->GetMaxStamina());
}

void AWRCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	InitAbilityActorInfo();
}

void AWRCharacter::OnDeath(AActor* Killer)
{
	if (AWRGameMode* GameMode = GetWorld()->GetAuthGameMode<AWRGameMode>())
	{
		GameMode->RequestRespawn(Controller);
	}
	DetachFromControllerPendingDestroy();
	SetLifeSpan(5.f);
}

// ---------------------------------------------------------------- view

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

void AWRCharacter::ToggleView()
{
	SetFirstPerson(!bFirstPerson);
}

// ---------------------------------------------------------------- input

void AWRCharacter::PressAbilityInput(EWRAbilityInput Input)
{
	if (AbilitySystem)
	{
		AbilitySystem->AbilityLocalInputPressed(static_cast<int32>(Input));
	}
}

void AWRCharacter::ReleaseAbilityInput(EWRAbilityInput Input)
{
	if (AbilitySystem)
	{
		AbilitySystem->AbilityLocalInputReleased(static_cast<int32>(Input));
	}
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

	const TPair<EWRAbilityInput, FKey> AbilityKeys[] = {
		{ EWRAbilityInput::Light, EKeys::LeftMouseButton },
		{ EWRAbilityInput::Heavy, EKeys::R },
		{ EWRAbilityInput::Block, EKeys::RightMouseButton },
		{ EWRAbilityInput::Dodge, EKeys::C },
		{ EWRAbilityInput::Slot1, EKeys::One }, { EWRAbilityInput::Slot2, EKeys::Two },
		{ EWRAbilityInput::Slot3, EKeys::Three }, { EWRAbilityInput::Slot4, EKeys::Four },
		{ EWRAbilityInput::Slot5, EKeys::Five }, { EWRAbilityInput::Slot6, EKeys::Six },
		{ EWRAbilityInput::Slot7, EKeys::Seven }, { EWRAbilityInput::Slot8, EKeys::Eight },
	};
	for (const TPair<EWRAbilityInput, FKey>& Pair : AbilityKeys)
	{
		UInputAction* Action = MakeAction(EInputActionValueType::Boolean);
		MappingContext->MapKey(Action, Pair.Value);
		AbilityActions.Add(Pair.Key, Action);
	}
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
		for (const TPair<EWRAbilityInput, TObjectPtr<UInputAction>>& Pair : AbilityActions)
		{
			Input->BindAction(Pair.Value, ETriggerEvent::Started, this, &AWRCharacter::PressAbilityInput, Pair.Key);
			Input->BindAction(Pair.Value, ETriggerEvent::Completed, this, &AWRCharacter::ReleaseAbilityInput, Pair.Key);
		}
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
