#include "WRCharacter.h"
#include "WRSocial.h"
#include "WRWorldDressing.h"
#include "Wildreach.h"
#include "WRAbilities.h"
#include "WRAttributeSet.h"
#include "WRCharacterMovementComponent.h"
#include "WRGameMode.h"
#include "WRGameplayTags.h"
#include "WRInventory.h"
#include "WRPathComponent.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRStatusComponent.h"
#include "WRTuning.h"
#include "AbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequenceBase.h"
#include "Engine/SkeletalMesh.h"
#include "AnimationRuntime.h"
#include "Dom/JsonObject.h"
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

	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(RootComponent);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetVisibility(false);
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
}

void AWRCharacter::BeginPlay()
{
	Super::BeginPlay();
	LoadBodyArt();
	ApplyViewMode();
	Cast<UWRCharacterMovementComponent>(GetCharacterMovement())->SprintSpeed = WRTune(TEXT("player.sprintSpeed"), 750.f);
	GetCharacterMovement()->MaxWalkSpeed = WRTune(TEXT("player.walkSpeed"), 450.f);
	DefaultFriction = GetCharacterMovement()->GroundFriction;
	DefaultBraking = GetCharacterMovement()->BrakingDecelerationWalking;
	DefaultAcceleration = GetCharacterMovement()->MaxAcceleration;
}

void AWRCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (GetNetMode() != NM_DedicatedServer)
	{
		TickBodyAnim();
	}
	// Slick ice: the same check runs on the server and the owning client, so movement prediction agrees.
	if (HasAuthority() || IsLocallyControlled())
	{
		const FVector Feet = GetActorLocation() - FVector(0.f, 0.f, GetSimpleCollisionHalfHeight());
		const bool bIce = FWRBiomeHazards::KindAt(Feet) == TEXT("ice") && !FWRBiomeHazards::Resists(this, TEXT("ice"));
		if (bIce != bOnIce)
		{
			bOnIce = bIce;
			UCharacterMovementComponent* Move = GetCharacterMovement();
			Move->GroundFriction = bIce ? WRTune(TEXT("hazards.iceFriction"), 0.3f) : DefaultFriction;
			Move->BrakingDecelerationWalking = bIce ? WRTune(TEXT("hazards.iceBraking"), 100.f) : DefaultBraking;
			Move->MaxAcceleration = bIce ? WRTune(TEXT("hazards.iceAcceleration"), 600.f) : DefaultAcceleration;
		}
	}
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
	PS->GetInventory()->GiveStartingKit();
	PS->GetInventory()->ApplyEquipment();
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
	AWRLootBag::ApplyDeathPenalty(this, GetPlayerState<AWRPlayerState>());
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
	Body->SetOwnerNoSee(bFirstPerson);

	// First person: body yaw follows the camera. Third person: body turns toward movement.
	bUseControllerRotationYaw = bFirstPerson;
	GetCharacterMovement()->bOrientRotationToMovement = !bFirstPerson;
}

// ---------------------------------------------------------------- body art

void AWRCharacter::LoadBodyArt()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;  // the server never draws bodies
	}
	const TSharedPtr<FJsonObject> Art = FWRTuning::LoadDataFile(TEXT("player_art.json"));
	FString MeshPath, Prefix, Socket;
	if (!Art.IsValid() || !Art->TryGetStringField(TEXT("mesh"), MeshPath))
	{
		return;
	}
	USkeletalMesh* BodyAsset = LoadObject<USkeletalMesh>(nullptr, *MeshPath);
	if (!BodyAsset)
	{
		UE_LOG(LogWildreach, Warning, TEXT("Player art: mesh %s not found, keeping the placeholder"), *MeshPath);
		return;
	}
	Art->TryGetStringField(TEXT("animPrefix"), Prefix);
	Art->TryGetStringField(TEXT("weaponSocket"), Socket);
	const double Yaw = Art->HasField(TEXT("meshYaw")) ? Art->GetNumberField(TEXT("meshYaw")) : 0.0;
	const double Scale = Art->HasField(TEXT("meshScale")) ? Art->GetNumberField(TEXT("meshScale")) : 1.0;
	const TSharedPtr<FJsonObject>* Speeds = nullptr;
	if (Art->TryGetObjectField(TEXT("speeds"), Speeds))
	{
		(*Speeds)->TryGetNumberField(TEXT("walk"), WalkClipSpeed);
		(*Speeds)->TryGetNumberField(TEXT("run"), RunClipSpeed);
	}

	// Feet on the capsule's bottom, facing the actor's forward.
	Body->SetSkeletalMesh(BodyAsset);
	const FBox Box = BodyAsset->GetBounds().GetBox();
	Body->SetRelativeScale3D(FVector(Scale));
	Body->SetRelativeLocation(FVector(0.f, 0.f, -GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - Box.Min.Z * Scale));
	Body->SetRelativeRotation(FRotator(0.f, Yaw, 0.f));
	Body->SetVisibility(true);
	BodyMesh->SetVisibility(false);
	FaceMarker->SetVisibility(false);

	// The placeholder blade becomes a sword held in the right hand. Mixamo's rest pose is a T-pose with the palms down,
	// so a held blade points along the body's forward there: work that out in the hand bone's space from the reference
	// skeleton, gripped in the palm (partway from the wrist to the index finger), ~95 cm long.
	const FReferenceSkeleton& Ref = BodyAsset->GetRefSkeleton();
	const int32 HandBone = Socket.IsEmpty() ? INDEX_NONE : Ref.FindBoneIndex(FName(*Socket));
	if (HandBone != INDEX_NONE)
	{
		const FTransform HandCS = FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, HandBone);
		const int32 FingerBone = Ref.FindBoneIndex(FName(*(Socket + TEXT("Index1"))));
		const FVector FingerCS = FingerBone != INDEX_NONE ? FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, FingerBone).GetLocation()
		                                                  : HandCS.GetLocation();
		const FVector BodyForward = FRotator(0.f, Yaw, 0.f).UnrotateVector(FVector::ForwardVector);  // in mesh space
		const FVector Blade = HandCS.InverseTransformVectorNoScale(BodyForward).GetSafeNormal();
		const FVector Grip = HandCS.InverseTransformPositionNoScale(FMath::Lerp(HandCS.GetLocation(), FingerCS, 0.6f));
		WeaponMesh->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName(*Socket));
		WeaponMesh->SetRelativeRotation(FRotationMatrix::MakeFromY(Blade).Rotator());  // the cube's long axis is its Y
		WeaponMesh->SetRelativeLocation(Grip + Blade * 40.f);                               // centre: blade ahead, short grip behind
		WeaponMesh->SetRelativeScale3D(FVector(0.05f, 0.95f, 0.1f));
		bArtWeapon = true;
	}

	const TSharedPtr<FJsonObject>* Anims = nullptr;
	if (Art->TryGetObjectField(TEXT("anims"), Anims))
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Set : (*Anims)->Values)
		{
			FBodyAnimSet& Out = BodyAnims.Add(FName(*Set.Key));
			for (const TSharedPtr<FJsonValue>& Name : Set.Value->AsArray())
			{
				const FString Clip = Prefix + Name->AsString();
				if (UAnimSequenceBase* Anim = LoadObject<UAnimSequenceBase>(nullptr, *FString::Printf(TEXT("%s.%s"), *Clip, *FPaths::GetBaseFilename(Clip))))
				{
					Out.Clips.Add(Anim);
					BodyAnimRefs.Add(Anim);
				}
			}
		}
	}
	UE_LOG(LogWildreach, Log, TEXT("Player art: %s with %d clips"), *BodyAsset->GetName(), BodyAnimRefs.Num());
}

float AWRCharacter::PlayBodyAnim(FName Key, bool bLoop, float PlayRate, float Duration)
{
	FBodyAnimSet* Set = BodyAnims.Find(Key);
	if (!Set || Set->Clips.Num() == 0)
	{
		return 0.f;
	}
	if (bLoop && BodyAnim == Key)
	{
		Body->SetPlayRate(PlayRate);  // already playing: just keep its pace matched to the speed
		return 0.f;
	}
	UAnimSequenceBase* Clip = Set->Clips[Set->Next % Set->Clips.Num()];
	Set->Next++;
	const float Length = Clip->GetPlayLength();
	if (Duration > 0.f)
	{
		PlayRate = FMath::Clamp(Length / Duration, 0.5f, 3.f);
	}
	Body->PlayAnimation(Clip, bLoop);
	Body->SetPlayRate(PlayRate);
	BodyAnim = Key;
	return Length / PlayRate;
}

void AWRCharacter::TickBodyAnim()
{
	if (!Body->IsVisible() || BodyAnims.Num() == 0 || HasTag(WRTags::State_Dead) || GetWorld()->GetTimeSeconds() < BodyOneShotUntil)
	{
		return;
	}
	const UCharacterMovementComponent* Move = GetCharacterMovement();
	const float Speed = GetVelocity().Size2D();
	if (HasTag(WRTags::State_Dodging) && BodyAnim != TEXT("dodge"))
	{
		BodyOneShotUntil = GetWorld()->GetTimeSeconds() + PlayBodyAnim(TEXT("dodge"), false, 1.f, WRTune(TEXT("player.dodgeVisualSeconds"), 0.7f));
		return;
	}
	if (Move->IsSwimming())
	{
		PlayBodyAnim(Speed > 40.f ? FName(TEXT("swim")) : FName(TEXT("tread")), true);
	}
	else if (Move->IsFalling())
	{
		if (BodyAnim != TEXT("fall")) PlayBodyAnim(TEXT("fall"), false);  // plays once and holds the airborne pose
	}
	else if (HasTag(WRTags::State_Blocking))
	{
		PlayBodyAnim(TEXT("block"), true);
	}
	else if (Speed > 280.f)
	{
		PlayBodyAnim(TEXT("run"), true, FMath::Clamp(Speed / RunClipSpeed, 0.7f, 1.8f));
	}
	else if (Speed > 25.f)
	{
		PlayBodyAnim(TEXT("walk"), true, FMath::Clamp(Speed / WalkClipSpeed, 0.6f, 1.8f));
	}
	else
	{
		PlayBodyAnim(TEXT("idle"), true);
	}
}

void AWRCharacter::OnSwingVisual(EWRSwingKind Kind, float Duration)
{
	if (!Body->IsVisible())
	{
		return;
	}
	const FName Key = Kind == EWRSwingKind::Cast ? FName(TEXT("cast"))
		: (Kind == EWRSwingKind::Heavy || Kind == EWRSwingKind::WeaponArt) ? FName(TEXT("heavy")) : FName(TEXT("light"));
	BodyAnim = NAME_None;  // restart even if the same set just played
	PlayBodyAnim(Key, false, 1.f, Duration);
	BodyOneShotUntil = GetWorld()->GetTimeSeconds() + Duration;
}

void AWRCharacter::OnHitVisual(EWRHitResult Result)
{
	if (!Body->IsVisible() || Result != EWRHitResult::Hit || GetWorld()->GetTimeSeconds() < BodyOneShotUntil)
	{
		return;  // a swing in progress isn't interrupted by the flinch
	}
	BodyAnim = NAME_None;
	BodyOneShotUntil = GetWorld()->GetTimeSeconds() + PlayBodyAnim(TEXT("hit"), false, 1.f, 0.45f);
}

void AWRCharacter::OnDeathVisual()
{
	if (Body->IsVisible())
	{
		BodyAnim = NAME_None;
		PlayBodyAnim(TEXT("death"), false);
		BodyOneShotUntil = 1.0e12;  // stay down
	}
}

void AWRCharacter::ToggleView()
{
	SetFirstPerson(!bFirstPerson);
}

// ---------------------------------------------------------------- input

void AWRCharacter::PressAbilityInput(EWRAbilityInput Input)
{
	// In build mode the attack buttons place / rotate / cancel instead.
	if (AWRPlayerController* PC = Cast<AWRPlayerController>(Controller); PC && PC->IsPlacing())
	{
		if (Input == EWRAbilityInput::Light) PC->ConfirmPlacement();
		else if (Input == EWRAbilityInput::Heavy) PC->RotatePlacement();
		else if (Input == EWRAbilityInput::Block) PC->CancelPlacement();
		return;
	}
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
