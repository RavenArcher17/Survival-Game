#include "WRCharacter.h"
#include "WRLifeSkills.h"
#include "WRFishing.h"
#include "WRAnimInstance.h"
#include "WRHeroAnimInstance.h"
#include "WRCameraBoom.h"
#include "WRSocial.h"
#include "WRWorldDressing.h"
#include "Wildreach.h"
#include "WRAbilities.h"
#include "WRAttributeSet.h"
#include "WRCharacterMovementComponent.h"
#include "WRCreature.h"
#include "WRGameMode.h"
#include "WRGameplayTags.h"
#include "WRInventory.h"
#include "WRPathComponent.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRResourceNode.h"
#include "WRStatusComponent.h"
#include "WRTuning.h"
#include "AbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequenceBase.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "AnimationRuntime.h"
#include "EngineUtils.h"
#include "Dom/JsonObject.h"
#include "EnhancedInputComponent.h"
#include "WRPadInput.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "WRKeyBindings.h"
#include "WRPlayerUI.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Net/UnrealNetwork.h"
#include "Components/LODSyncComponent.h"
#include "Engine/SkeletalMeshSocket.h"
#include "GroomComponent.h"
#include "GroomAsset.h"
#include "GroomBindingAsset.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "Animation/AnimInstance.h"

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

	ThirdPersonBoom = CreateDefaultSubobject<UWRCameraBoom>(TEXT("ThirdPersonBoom"));  // eases back out after a wall or trunk
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
	// The pose always updates (bones only refresh when seen): footsteps of a survivor behind you, or your own in first
	// person (where your body is hidden from you), come from the animation's foot phase.
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPose;

	// The MetaHuman's head and the rest of the hero (filled from player_art.json by LoadBodyArt).
	Face = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Face"));
	Face->SetupAttachment(Body);
	Face->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Face->SetVisibility(false);
	Face->bCastHiddenShadow = true;  // first person hides the head from you; your shadow keeps it
	static const TCHAR* GroomSlots[] = { TEXT("Hair"), TEXT("Eyebrows"), TEXT("Eyelashes"), TEXT("Beard"), TEXT("Mustache") };
	for (const TCHAR* Slot : GroomSlots)
	{
		UGroomComponent* Groom = CreateDefaultSubobject<UGroomComponent>(Slot);
		Groom->SetupAttachment(Face);
		Groom->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Groom->SetVisibility(false);
		Groom->bCastHiddenShadow = true;
		Grooms.Add(Groom);
	}
	static const TCHAR* OutfitSlots[] = { TEXT("OutfitHead"), TEXT("OutfitChest"), TEXT("OutfitHands"), TEXT("OutfitLegs") };
	for (const TCHAR* Slot : OutfitSlots)
	{
		USkeletalMeshComponent* Piece = CreateDefaultSubobject<USkeletalMeshComponent>(Slot);
		Piece->SetupAttachment(Body);
		Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Piece->SetVisibility(false);
		Piece->bCastHiddenShadow = true;
		Outfit.Add(Piece);
	}
	LODSync = CreateDefaultSubobject<ULODSyncComponent>(TEXT("LODSync"));

	ShieldMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShieldMesh"));
	ShieldMesh->SetupAttachment(Body);
	ShieldMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ShieldMesh->SetVisibility(false);

	RangedMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RangedMesh"));
	RangedMesh->SetupAttachment(Body);
	RangedMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RangedMesh->SetVisibility(false);
}

void AWRCharacter::BeginPlay()
{
	Super::BeginPlay();
	Body->PrimaryComponentTick.AddPrerequisite(this, PrimaryActorTick);  // animate from this frame's movement
	LoadBodyArt();
	ApplyViewMode();
	Cast<UWRCharacterMovementComponent>(GetCharacterMovement())->SprintSpeed = WRTune(TEXT("player.sprintSpeed"), 750.f);
	GetCharacterMovement()->MaxWalkSpeed = WRTune(TEXT("player.walkSpeed"), 450.f);
	// Movement (FULL-GAME 9): 0.15 s to full speed and a quick stop, so starts and stops read; motion matching's
	// warping keeps the feet planted on the snappier capsule.
	{
		UCharacterMovementComponent* Move = GetCharacterMovement();
		Move->MaxAcceleration = WRTune(TEXT("player.maxAcceleration"), 5000.f);
		Move->BrakingDecelerationWalking = WRTune(TEXT("player.brakingDeceleration"), 4000.f);
		Move->bUseSeparateBrakingFriction = true;
		Move->BrakingFriction = WRTune(TEXT("player.brakingFriction"), 6.f);
		Move->GroundFriction = WRTune(TEXT("player.groundFriction"), 8.f);
	}
	DefaultFriction = GetCharacterMovement()->GroundFriction;
	DefaultBraking = GetCharacterMovement()->BrakingDecelerationWalking;
	DefaultAcceleration = GetCharacterMovement()->MaxAcceleration;
}

void AWRCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Blocking keeps you facing where you aim (WRCharacterBase): the movement mustn't also turn the body toward a
	// strafe or a step back, or the two fight every frame and the body wobbles (worse on a long frame). Same rule as
	// aiming and first person; set on every machine so prediction agrees.
	GetCharacterMovement()->bOrientRotationToMovement = !(bFirstPerson || bAiming || HasTag(WRTags::State_Blocking));
	if (GetNetMode() != NM_DedicatedServer)
	{
		if (IsLocallyControlled())
		{
			TickAimCamera(DeltaSeconds);
			TickFirstPersonCamera(DeltaSeconds);
		}
		BodyAimTurn = FMath::FInterpTo(BodyAimTurn, IsAimingRanged() ? RangedAimYaw : 0.f, DeltaSeconds, 10.f);
		TickBodyAnim();
	}
	if (HasAuthority())
	{
		UpdateReplicatedAccel();
		// Swimming XP (life skill): metres swum across the water, checked twice a second (a jump in distance, a
		// teleport or a respawn, doesn't count).
		SwimCheck += DeltaSeconds;
		if (SwimCheck >= 0.5f)
		{
			SwimCheck = 0.f;
			const FVector Here = GetActorLocation();
			if (GetCharacterMovement()->IsSwimming() && IsAlive() && !LastSwimAt.IsZero())
			{
				const float Step = FVector::Dist2D(Here, LastSwimAt);
				if (Step < GetCharacterMovement()->GetMaxSpeed() * 0.5f * 1.5f + 50.f) SwumCm += Step;
				if (SwumCm >= 1000.f)
				{
					UWRLifeSkillsComponent::Award(this, TEXT("swimming"), TEXT("swim"), 0.5f, SwumCm / 100.f);
					SwumCm = 0.f;
				}
			}
			LastSwimAt = GetCharacterMovement()->IsSwimming() ? Here : FVector::ZeroVector;
		}
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
	DOREPLIFETIME(AWRCharacter, BodyStyle);
	DOREPLIFETIME(AWRCharacter, RangedItem);
	DOREPLIFETIME(AWRCharacter, bRangedMode);
	DOREPLIFETIME(AWRCharacter, bAiming);
	DOREPLIFETIME(AWRCharacter, SelectedAmmo);
	DOREPLIFETIME(AWRCharacter, MeleeItem);
	DOREPLIFETIME_CONDITION(AWRCharacter, ReplicatedAccel, COND_SimulatedOnly);
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
	BodyStyle = PS->BodyStyle;  // replicates; a listen server or standalone game swaps its own copy now
	LoadBodyArt();

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

void AWRCharacter::OnRep_BodyStyle()
{
	if (HasActorBegunPlay())
	{
		LoadBodyArt();  // otherwise BeginPlay loads it
	}
}

void AWRCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	InitAbilityActorInfo();
}

void AWRCharacter::OnDeath(AActor* Killer)
{
	AWRLootBag::ApplyDeathPenalty(this, GetPlayerState<AWRPlayerState>());
	WRFlow::SendDeathNote(this, Killer);  // the death screen: killer, bag, where you'll respawn
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
	// Full-body first person (hero.md 2.7): you see your own arms, body and legs; the head and everything on it is
	// hidden from you (its shadow stays), so the eye sits inside nothing.
	Body->SetOwnerNoSee(false);
	Face->SetOwnerNoSee(bFirstPerson);
	for (UGroomComponent* Groom : Grooms)
	{
		Groom->SetOwnerNoSee(bFirstPerson);
	}
	if (Outfit.Num() > 0)
	{
		Outfit[0]->SetOwnerNoSee(bFirstPerson);  // the Head slot (hood, helm)
	}
	ShieldMesh->SetOwnerNoSee(bFirstPerson);  // it would fill the first-person view; the sword stays
	bEyeInit = false;

	// First person: body yaw follows the camera. Third person: body turns toward movement.
	bUseControllerRotationYaw = bFirstPerson;
	GetCharacterMovement()->bOrientRotationToMovement = !bFirstPerson;
}

void AWRCharacter::TickFirstPersonCamera(float DeltaSeconds)
{
	if (!bFirstPerson || !Body->IsVisible() || DeltaSeconds <= 0.f)
	{
		return;
	}
	// The eye: the head bone after animation, a little forward and up (hero.md 2.7). Across the ground it follows the head
	// one to one; up and down it rides a critically damped spring with a short half-life and a small bob limit, so a
	// stride's bounce doesn't shake the view. The view's turn is always the controller's, never the head's (no roll).
	const FVector Forward = GetActorForwardVector();
	const float EyeForward = WRTune(TEXT("fp.eyeForward"), 9.f), EyeUp = WRTune(TEXT("fp.eyeUp"), 6.f);
	const FName HeadBone(TEXT("head"));
	FVector Eye = Body->DoesSocketExist(HeadBone) ? Body->GetSocketLocation(HeadBone) + Forward * EyeForward + FVector(0.f, 0.f, EyeUp)
	                                              : GetActorLocation() + FVector(0.f, 0.f, 64.f);
	// A roll in first person: the view only drops (30 cm and back over the roll), it never tumbles with the head.
	const double Now = GetWorld()->GetTimeSeconds();
	if (HasTag(WRTags::State_Dodging))
	{
		if (DodgeStartedAt < 0.0)
		{
			DodgeStartedAt = Now;
			DodgeEye = FirstPersonCamera->GetComponentLocation() - GetActorLocation();
		}
		const float Length = FMath::Max(WRTune(TEXT("player.dodgeDuration"), 0.45f), 0.1f);
		const float T = FMath::Clamp(float(Now - DodgeStartedAt) / Length, 0.f, 1.f);
		Eye = GetActorLocation() + DodgeEye - FVector(0.f, 0.f, WRTune(TEXT("fp.rollDrop"), 30.f) * FMath::Sin(T * PI));
		const float Vignette = WRTune(TEXT("fp.rollVignette"), 0.15f) * FMath::Clamp(1.f - float(Now - DodgeStartedAt) / 0.1f, 0.f, 1.f);
		FirstPersonCamera->PostProcessSettings.bOverride_VignetteIntensity = Vignette > 0.f;
		FirstPersonCamera->PostProcessSettings.VignetteIntensity = Vignette;
	}
	else if (DodgeStartedAt >= 0.0)
	{
		DodgeStartedAt = -1.0;
		FirstPersonCamera->PostProcessSettings.bOverride_VignetteIntensity = false;
	}
	if (!bEyeInit)
	{
		bEyeInit = true;
		EyeZ = float(Eye.Z);
		EyeZSpeed = 0.f;
	}
	// Critically damped spring (half-life), clamped to the bob limit around the head.
	const float HalfLife = FMath::Max(WRTune(TEXT("fp.halfLife"), 0.05f), 0.001f);
	const float Y = (4.f * 0.69314718f) / HalfLife / 2.f;
	const float J0 = EyeZ - float(Eye.Z);
	const float J1 = EyeZSpeed + J0 * Y;
	const float E = FMath::Exp(-Y * DeltaSeconds);
	EyeZ = E * (J0 + J1 * DeltaSeconds) + float(Eye.Z);
	EyeZSpeed = E * (EyeZSpeed - J1 * Y * DeltaSeconds);
	const float Bob = WRTune(TEXT("fp.bob"), 2.5f);
	EyeZ = FMath::Clamp(EyeZ, float(Eye.Z) - Bob, float(Eye.Z) + Bob);
	FirstPersonCamera->SetWorldLocation(FVector(Eye.X, Eye.Y, EyeZ));
}

void AWRCharacter::UpdateReplicatedAccel()
{
	// Server: what other machines' copies need for motion matching (the owner and the server have the real thing).
	const UCharacterMovementComponent* Move = GetCharacterMovement();
	const FVector Accel = Move->GetCurrentAcceleration();
	const float Max = FMath::Max(Move->GetMaxAcceleration(), 1.f);
	FWRReplicatedAccel Next;
	const float Angle = FMath::Atan2(float(Accel.Y), float(Accel.X));
	Next.Angle = uint8(FMath::RoundToInt(FMath::Fmod(Angle + 2.f * PI, 2.f * PI) / (2.f * PI) * 255.f));
	Next.Magnitude = uint8(FMath::Clamp(FMath::RoundToInt(float(Accel.Size2D()) / Max * 255.f), 0, 255));
	Next.Z = int8(FMath::Clamp(FMath::RoundToInt(float(Accel.Z) / Max * 127.f), -127, 127));
	if (const UWRHeroAnimInstance* Anim = BodyAnimInstance())
	{
		Next.Bits = uint8(Anim->HeroInput.Gait) | (uint8(Anim->HeroInput.Stance) << 2) | (Anim->HeroInput.bLockedOn ? 0x20 : 0);
	}
	else
	{
		// A dedicated server has no body: the same rules from the movement itself.
		const float Speed = Move->GetMaxSpeed();
		const uint8 Gait = Speed >= WRTune(TEXT("player.sprintSpeed"), 750.f) - 1.f ? 2 : Speed <= WRTune(TEXT("anim.mm.walkBelow"), 320.f) ? 0 : 1;
		const uint8 Stance = IsRangedOut() ? (bAiming ? 2 : 1) : HasTag(WRTags::State_Blocking) ? 4 : 0;
		Next.Bits = Gait | (Stance << 2);
	}
	if (FMemory::Memcmp(&Next, &ReplicatedAccel, sizeof(FWRReplicatedAccel)) != 0)
	{
		ReplicatedAccel = Next;
	}
}

// ---------------------------------------------------------------- body art

void AWRCharacter::LoadBodyArt()
{
	if (GetNetMode() == NM_DedicatedServer || (bBodyLoaded && LoadedBodyStyle == BodyStyle))
	{
		return;  // the server never draws bodies
	}
	const TSharedPtr<FJsonObject> Art = FWRTuning::LoadDataFile(TEXT("player_art.json"));
	const TSharedPtr<FJsonObject>* Heroes = nullptr;
	if (!Art.IsValid() || !Art->TryGetObjectField(TEXT("hero"), Heroes))
	{
		return;
	}
	// The body picked in the menu: "f" the female hero, anything else the male.
	const FString Sex = BodyStyle == TEXT("f") ? TEXT("f") : TEXT("m");
	const TSharedPtr<FJsonObject>* HeroPtr = nullptr;
	if (!(*Heroes)->TryGetObjectField(Sex, HeroPtr))
	{
		return;
	}
	const TSharedPtr<FJsonObject> Hero = *HeroPtr;
	FString BodyPath;
	Hero->TryGetStringField(TEXT("body"), BodyPath);
	USkeletalMesh* BodyAsset = BodyPath.IsEmpty() ? nullptr : LoadObject<USkeletalMesh>(nullptr, *BodyPath);
	if (!BodyAsset)
	{
		UE_LOG(LogWildreach, Warning, TEXT("Player art: hero body %s not found, keeping the placeholder"), *BodyPath);
		return;
	}
	LoadedBodyStyle = BodyStyle;
	bBodyLoaded = true;
	BodyAnims.Reset();
	BodyAnimRefs.Reset();
	BodyAnim = NAME_None;
	BodyOneShotUntil = 0.0;
	const double Yaw = Art->HasField(TEXT("meshYaw")) ? Art->GetNumberField(TEXT("meshYaw")) : -90.0;
	const TSharedPtr<FJsonObject>* Sockets = nullptr;
	Art->TryGetObjectField(TEXT("sockets"), Sockets);
	const auto SocketName = [Sockets](const TCHAR* Key, const TCHAR* Fallback)
	{
		FString Name;
		return FName(Sockets && (*Sockets)->TryGetStringField(Key, Name) ? *Name : Fallback);
	};
	WeaponSocket = SocketName(TEXT("weapon"), TEXT("weapon_r"));
	ShieldSocket = SocketName(TEXT("shield"), TEXT("shield_l"));

	// Feet on the capsule's bottom, facing the actor's forward (the MetaHuman faces +Y).
	Body->SetSkeletalMesh(BodyAsset);
	const FBox Box = BodyAsset->GetBounds().GetBox();
	Body->SetRelativeScale3D(FVector::OneVector);
	BodyBaseZ = -GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - float(FMath::Min(Box.Min.Z, 0.0));
	SwimLift = 0.f;
	Body->SetRelativeLocation(FVector(0.f, 0.f, BodyBaseZ));
	BodyYaw = float(Yaw);
	BodyAimTurn = 0.f;
	Body->SetRelativeRotation(FRotator(0.f, Yaw, 0.f));
	Body->SetVisibility(true);
	BodyMesh->SetVisibility(false);
	FaceMarker->SetVisibility(false);

	// The head: the MetaHuman face mesh with its own animation blueprint (it copies the body's pose and runs the face
	// rig: blinks, the jaw), the grooms bound to it, rendered as cards in the game.
	FString FacePath, FaceAnimPath;
	Hero->TryGetStringField(TEXT("face"), FacePath);
	Art->TryGetStringField(TEXT("faceAnim"), FaceAnimPath);
	USkeletalMesh* FaceAsset = FacePath.IsEmpty() ? nullptr : LoadObject<USkeletalMesh>(nullptr, *FacePath);
	Face->SetSkeletalMesh(FaceAsset);
	Face->SetVisibility(FaceAsset != nullptr);
	if (FaceAsset)
	{
		if (UClass* FaceAnim = FaceAnimPath.IsEmpty() ? nullptr : LoadClass<UAnimInstance>(nullptr, *FaceAnimPath))
		{
			Face->SetAnimInstanceClass(FaceAnim);
		}
		else
		{
			Face->SetLeaderPoseComponent(Body);  // no face rig: the head still follows the body
		}
	}
	const TSharedPtr<FJsonObject>* GroomSet = nullptr;
	Hero->TryGetObjectField(TEXT("grooms"), GroomSet);
	for (UGroomComponent* Groom : Grooms)
	{
		const TSharedPtr<FJsonObject>* Slot = nullptr;
		FString GroomPath, BindingPath;
		if (FaceAsset && GroomSet && (*GroomSet)->TryGetObjectField(Groom->GetName(), Slot))
		{
			(*Slot)->TryGetStringField(TEXT("groom"), GroomPath);
			(*Slot)->TryGetStringField(TEXT("binding"), BindingPath);
		}
		UGroomAsset* GroomAsset = GroomPath.IsEmpty() ? nullptr : LoadObject<UGroomAsset>(nullptr, *GroomPath);
		UGroomBindingAsset* Binding = BindingPath.IsEmpty() ? nullptr : LoadObject<UGroomBindingAsset>(nullptr, *BindingPath);
		Groom->SetUseCards(true);
		Groom->SetGroomAsset(GroomAsset, Binding, false);
		Groom->SetEnableSimulation(false);
		Groom->SetVisibility(GroomAsset != nullptr);
	}
	// The outfit (hero.md 2.5: tier 0, Survivor Wraps): each piece follows the body's bones.
	const TSharedPtr<FJsonObject>* OutfitSet = nullptr;
	Hero->TryGetObjectField(TEXT("outfit"), OutfitSet);
	static const TCHAR* OutfitKeys[] = { TEXT("Head"), TEXT("Chest"), TEXT("Hands"), TEXT("Legs") };
	for (int32 i = 0; i < Outfit.Num() && i < UE_ARRAY_COUNT(OutfitKeys); ++i)
	{
		FString PiecePath;
		if (OutfitSet) (*OutfitSet)->TryGetStringField(OutfitKeys[i], PiecePath);
		USkeletalMesh* Piece = PiecePath.IsEmpty() ? nullptr : LoadObject<USkeletalMesh>(nullptr, *PiecePath);
		Outfit[i]->SetSkeletalMesh(Piece);
		Outfit[i]->SetLeaderPoseComponent(Piece ? Body.Get() : nullptr);
		Outfit[i]->SetVisibility(Piece != nullptr);
	}
	// One LOD for the whole hero, led by the face (it has the most levels): the body, outfit and hair follow.
	LODSync->ComponentsToSync.Reset();
	LODSync->CustomLODMapping.Reset();
	LODSync->NumLODs = FaceAsset ? FaceAsset->GetLODNum() : BodyAsset->GetLODNum();
	LODSync->ComponentsToSync.Add({ Face->GetFName(), FaceAsset ? ESyncOption::Drive : ESyncOption::Disabled });
	LODSync->ComponentsToSync.Add({ Body->GetFName(), FaceAsset ? ESyncOption::Passive : ESyncOption::Drive });
	const auto MapTo = [this](int32 Levels)
	{
		FLODMappingData Map;
		for (int32 L = 0; L < LODSync->NumLODs; ++L)
		{
			Map.Mapping.Add(FMath::Min(Levels - 1, L * Levels / FMath::Max(LODSync->NumLODs, 1)));
		}
		return Map;
	};
	if (FaceAsset)
	{
		LODSync->CustomLODMapping.Add(Body->GetFName(), MapTo(BodyAsset->GetLODNum()));
	}
	for (USkeletalMeshComponent* Piece : Outfit)
	{
		if (Piece->GetSkeletalMeshAsset())
		{
			LODSync->ComponentsToSync.Add({ Piece->GetFName(), ESyncOption::Passive });
			LODSync->CustomLODMapping.Add(Piece->GetFName(), MapTo(Piece->GetSkeletalMeshAsset()->GetLODNum()));
		}
	}
	for (UGroomComponent* Groom : Grooms)
	{
		if (Groom->GroomAsset)
		{
			LODSync->ComponentsToSync.Add({ Groom->GetFName(), ESyncOption::Passive });
		}
	}
	LODSync->RefreshSyncComponents();

	// The sword in the right hand's socket (the palm's frame: X out of the fist past the index finger, Z out of the
	// palm), the blade out of the fist and the crossguard across the knuckles.
	FString WeaponPath, ShieldPath;
	Art->TryGetStringField(TEXT("weaponMesh"), WeaponPath);
	Art->TryGetStringField(TEXT("shieldMesh"), ShieldPath);
	WeaponMesh->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, WeaponSocket);
	UStaticMesh* Sword = WeaponPath.IsEmpty() ? nullptr : LoadObject<UStaticMesh>(nullptr, *WeaponPath);
	const FTransform Fist = FistFrame();
	if (Sword)
	{
		// A modelled sword (Tools/art/normalize_gear.py: grip at the origin, blade along the longest axis, crossguard
		// along the next).
		const FBox SwordBox = Sword->GetBoundingBox();
		const FVector Size = SwordBox.GetSize();
		const int32 Long = Size.X >= Size.Y && Size.X >= Size.Z ? 0 : (Size.Y >= Size.Z ? 1 : 2);
		const int32 Guard = Size[(Long + 1) % 3] >= Size[(Long + 2) % 3] ? (Long + 1) % 3 : (Long + 2) % 3;
		FVector LocalBlade = FVector::ZeroVector, LocalGuard = FVector::ZeroVector;
		LocalBlade[Long] = SwordBox.Max[Long] >= -SwordBox.Min[Long] ? 1.f : -1.f;  // the blade is the long side of the grip
		LocalGuard[Guard] = 1.f;
		const FQuat Rot = Fist.GetRotation() * FQuat(FRotationMatrix::MakeFromXY(LocalBlade, LocalGuard)).Inverse();
		WeaponMesh->SetStaticMesh(Sword);
		WeaponMesh->EmptyOverrideMaterials();
		WeaponMesh->SetRelativeRotation(Rot.Rotator());
		WeaponMesh->SetRelativeLocation(Fist.GetLocation());
		WeaponMesh->SetRelativeScale3D(FVector::OneVector);
	}
	else
	{
		WeaponMesh->SetRelativeRotation(FRotationMatrix::MakeFromY(Fist.GetUnitAxis(EAxis::X)).Rotator());  // the cube's long axis is its Y
		WeaponMesh->SetRelativeLocation(Fist.GetLocation() + Fist.GetUnitAxis(EAxis::X) * 40.f);
		WeaponMesh->SetRelativeScale3D(FVector(0.05f, 0.95f, 0.1f));
	}
	bArtWeapon = true;

	// The shield on the left forearm's socket (X along the forearm toward the wrist, Z out of the back of the arm, its
	// origin already off the arm): its face out, its top toward the hand, its back resting on the socket.
	UStaticMesh* Shield = ShieldPath.IsEmpty() ? nullptr : LoadObject<UStaticMesh>(nullptr, *ShieldPath);
	bShieldShown = Shield && BodyAsset->FindSocket(ShieldSocket);
	ShieldMesh->SetVisibility(bShieldShown);
	if (bShieldShown)
	{
		const FVector Size = Shield->GetBoundingBox().GetSize();
		const int32 Thin = Size.X <= Size.Y && Size.X <= Size.Z ? 0 : (Size.Y <= Size.Z ? 1 : 2);
		FVector LocalFace = FVector::ZeroVector;
		LocalFace[Thin] = 1.f;  // normalize_gear.py faces the front along +Z (Unreal's glTF import keeps it)
		const FVector LocalUp = Thin == 2 ? FVector::YAxisVector : FVector::ZAxisVector;
		const FQuat Rot = FQuat(FRotationMatrix::MakeFromXY(FVector::ZAxisVector, FVector::XAxisVector)) * FQuat(FRotationMatrix::MakeFromXY(LocalFace, LocalUp)).Inverse();
		ShieldMesh->SetStaticMesh(Shield);
		ShieldMesh->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, ShieldSocket);
		ShieldMesh->SetRelativeRotation(Rot.Rotator());
		ShieldMesh->SetRelativeLocation(FVector(0.f, 0.f, Size[Thin] * 0.5f + WRTune(TEXT("player.shieldOffArm"), 0.f)));
	}

	// The clips (retargeted onto the hero once, Content/Wildreach/Characters/Hero/Anims).
	FString AnimDir;
	Art->TryGetStringField(TEXT("animDir"), AnimDir);
	const TSharedPtr<FJsonObject>* Anims = nullptr;
	if (Art->TryGetObjectField(TEXT("anims"), Anims))
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Set : (*Anims)->Values)
		{
			FBodyAnimSet& Out = BodyAnims.Add(FName(*Set.Key));
			for (const TSharedPtr<FJsonValue>& Name : Set.Value->AsArray())
			{
				const FString Clip = Name->AsString().StartsWith(TEXT("/")) ? Name->AsString() : AnimDir + Name->AsString();
				if (UAnimSequenceBase* Anim = LoadObject<UAnimSequenceBase>(nullptr, *FString::Printf(TEXT("%s.%s"), *Clip, *FPaths::GetBaseFilename(Clip))))
				{
					Out.Clips.Add(Anim);
					BodyAnimRefs.Add(Anim);
				}
				else
				{
					UE_LOG(LogWildreach, Warning, TEXT("Player art: clip %s not found"), *Clip);
				}
			}
		}
	}
	UE_LOG(LogWildreach, Log, TEXT("Player art: hero %s (%s) with %d clips"), *BodyAsset->GetName(), *Sex, BodyAnimRefs.Num());
	// The hero's animation instance (motion-matched locomotion under the layered one-shots), set up after the mesh.
	Body->SetAnimInstanceClass(UWRHeroAnimInstance::StaticClass());
	SetupBodyAnim(Art, Hero);
	ApplyViewMode();
	UpdateWeaponLook();  // a ranged weapon may already be out
}

FTransform AWRCharacter::FistFrame() const
{
	// In the weapon socket's own space: X out of the fist (index side), Y toward the fingers (the knuckles' way).
	const USkeletalMesh* BodyAsset = Body->GetSkeletalMeshAsset();
	const USkeletalMeshSocket* Socket = BodyAsset ? BodyAsset->FindSocket(WeaponSocket) : nullptr;
	if (!Socket)
	{
		return FTransform::Identity;
	}
	const FReferenceSkeleton& Ref = BodyAsset->GetRefSkeleton();
	const int32 Bone = Ref.FindBoneIndex(Socket->BoneName);
	const FString Side = Socket->BoneName.ToString().Right(2);  // "_r" / "_l"
	const int32 Finger = Ref.FindBoneIndex(FName(*(TEXT("middle_01") + Side)));
	if (Bone == INDEX_NONE || Finger == INDEX_NONE)
	{
		return FTransform::Identity;
	}
	const FTransform SocketCS = Socket->GetSocketLocalTransform() * FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, Bone);
	const FVector Fingers = SocketCS.InverseTransformVectorNoScale(FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, Finger).GetLocation()
		- FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, Bone).GetLocation());
	FVector Y = FVector::VectorPlaneProject(Fingers, FVector::XAxisVector).GetSafeNormal();
	if (Y.IsNearlyZero())
	{
		Y = FVector::YAxisVector;
	}
	return FTransform(FQuat(FRotationMatrix::MakeFromXY(FVector::XAxisVector, Y)), FVector::ZeroVector);
}

UWRHeroAnimInstance* AWRCharacter::BodyAnimInstance() const
{
	return Cast<UWRHeroAnimInstance>(Body->GetAnimInstance());
}

UAnimSequenceBase* AWRCharacter::BodyClip(FName Key) const
{
	const FBodyAnimSet* Set = BodyAnims.Find(Key);
	return Set && Set->Clips.Num() > 0 ? Set->Clips[0].Get() : nullptr;
}

void AWRCharacter::SetupBodyAnim(const TSharedPtr<FJsonObject>& Art, const TSharedPtr<FJsonObject>& Hero)
{
	UWRHeroAnimInstance* Anim = BodyAnimInstance();
	if (!Anim)
	{
		UE_LOG(LogWildreach, Warning, TEXT("Player art: no animation instance on the body"));
		return;
	}
	Anim->ResetSetup();
	const auto LoadClipAt = [](const FString& Path) -> UAnimSequenceBase*
	{
		return Path.IsEmpty() ? nullptr : LoadObject<UAnimSequenceBase>(nullptr, *FString::Printf(TEXT("%s.%s"), *Path, *FPaths::GetBaseFilename(Path)));
	};
	FString IdlePath, TurnLeft, TurnRight;
	Art->TryGetStringField(TEXT("locoIdle"), IdlePath);
	Art->TryGetStringField(TEXT("turnLeft"), TurnLeft);
	Art->TryGetStringField(TEXT("turnRight"), TurnRight);
	UAnimSequenceBase* Idle = LoadClipAt(IdlePath);
	if (Idle)
	{
		BodyAnimRefs.Add(Idle);
	}
	// The base layer's own set holds the idle (until the first match) and the turn-in-place steps; motion matching does
	// the rest of the locomotion.
	Anim->AddLocoSet(TEXT("hero"), Idle, {});
	Anim->SetTurnClips(TEXT("hero"), LoadClipAt(TurnLeft), LoadClipAt(TurnRight));
	Anim->SetMeshForward(FRotator(0.f, BodyYaw, 0.f).UnrotateVector(FVector::ForwardVector));

	const auto Names = [&Art](const TCHAR* Field, TArray<FName> Fallback)
	{
		const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
		if (Art->TryGetArrayField(Field, List))
		{
			Fallback.Reset();
			for (const TSharedPtr<FJsonValue>& V : *List) Fallback.Add(FName(*V->AsString()));
		}
		return Fallback;
	};
	FString Upper = TEXT("spine_01");
	Art->TryGetStringField(TEXT("upperBody"), Upper);
	Anim->SetUpperBody(FName(*Upper), Names(TEXT("spineBones"), { TEXT("spine_02"), TEXT("spine_03"), TEXT("spine_04"), TEXT("spine_05") }));
	Anim->SetLegs(Names(TEXT("legBones"), { TEXT("thigh_l"), TEXT("calf_l"), TEXT("foot_l"), TEXT("thigh_r"), TEXT("calf_r"), TEXT("foot_r") }));
	FullBodyKeys.Reset();
	for (const FName& Key : Names(TEXT("fullBody"), { TEXT("dodge"), TEXT("death"), TEXT("swim"), TEXT("tread"), TEXT("gather"), TEXT("chop"), TEXT("pickup") }))
	{
		FullBodyKeys.Add(Key);
	}

	// The chooser: state x gait x stance -> the databases to search (player_art.json "locomotion"; "fallback" for far
	// copies of other players).
	FString MMDir;
	Art->TryGetStringField(TEXT("mmDir"), MMDir);
	const auto Rows = [&](const TCHAR* Field)
	{
		TArray<FWRHeroLocoRow> Out;
		const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
		if (!Art->TryGetArrayField(Field, List))
		{
			return Out;
		}
		static const TMap<FString, EWRHeroMove> States = { { TEXT("idle"), EWRHeroMove::Idle }, { TEXT("moving"), EWRHeroMove::Moving },
			{ TEXT("air"), EWRHeroMove::Air }, { TEXT("land"), EWRHeroMove::Land }, { TEXT("swim"), EWRHeroMove::Swim } };
		static const TMap<FString, int32> Gaits = { { TEXT("walk"), 0 }, { TEXT("run"), 1 }, { TEXT("sprint"), 2 } };
		static const TMap<FString, int32> Stances = { { TEXT("melee"), 0 }, { TEXT("ranged"), 1 }, { TEXT("bow_aim"), 2 }, { TEXT("rifle_aim"), 3 }, { TEXT("block"), 4 } };
		for (const TSharedPtr<FJsonValue>& Value : *List)
		{
			const TSharedPtr<FJsonObject> Obj = Value->AsObject();
			FString State, Gait, Stance;
			if (!Obj.IsValid() || !Obj->TryGetStringField(TEXT("state"), State) || !States.Contains(State))
			{
				continue;
			}
			FWRHeroLocoRow& Row = Out.AddDefaulted_GetRef();
			Row.State = States[State];
			Row.Gait = Obj->TryGetStringField(TEXT("gait"), Gait) && Gaits.Contains(Gait) ? Gaits[Gait] : -1;
			Row.Stance = Obj->TryGetStringField(TEXT("stance"), Stance) && Stances.Contains(Stance) ? Stances[Stance] : -1;
			const TArray<TSharedPtr<FJsonValue>>* Dbs = nullptr;
			if (Obj->TryGetArrayField(TEXT("dbs"), Dbs))
			{
				for (const TSharedPtr<FJsonValue>& Db : *Dbs)
				{
					const FString Path = MMDir + Db->AsString();
					if (const UPoseSearchDatabase* Database = LoadObject<UPoseSearchDatabase>(nullptr, *FString::Printf(TEXT("%s.%s"), *Path, *FPaths::GetBaseFilename(Path))))
					{
						Row.Databases.Add(Database);
					}
					else
					{
						UE_LOG(LogWildreach, Warning, TEXT("Player art: motion matching database %s not found"), *Path);
					}
				}
			}
		}
		return Out;
	};
	Anim->SetLocomotion(Rows(TEXT("locomotion")), Rows(TEXT("fallback")));
	Anim->SetFemale(BodyStyle == TEXT("f"));
	Anim->SetupProportions(Idle);
}

float AWRCharacter::PlayBodyAnim(FName Key, bool bLoop, float PlayRate, float Duration)
{
	FBodyAnimSet* Set = BodyAnims.Find(Key);
	UWRHeroAnimInstance* Anim = BodyAnimInstance();
	if (!Set || Set->Clips.Num() == 0 || !Anim)
	{
		return 0.f;
	}
	UAnimSequenceBase* Clip = Set->Clips[Set->Next % Set->Clips.Num()];
	Set->Next++;
	const float Length = Clip->GetPlayLength();
	if (Duration > 0.f)
	{
		PlayRate = FMath::Clamp(Length / Duration, 0.5f, 3.f);
	}
	PlayRate = PlayRate > KINDA_SMALL_NUMBER ? PlayRate : 1.f;
	const bool bHold = Key == TEXT("death");  // stay down
	Anim->PlayOneShot(Clip, FullBodyKeys.Contains(Key) ? EWRAnimLayer::Full : EWRAnimLayer::Upper, PlayRate, 0.f, 0.f, bHold);
	BodyAnim = Key;
	return Length / PlayRate;
}

void AWRCharacter::TickBodyAnim()
{
	UWRHeroAnimInstance* Anim = BodyAnimInstance();
	if (!Body->IsVisible() || BodyAnims.Num() == 0 || !Anim)
	{
		return;
	}
	const UCharacterMovementComponent* Move = GetCharacterMovement();
	const double Now = GetWorld()->GetTimeSeconds();
	const float Dt = GetWorld()->GetDeltaSeconds();
	const FVector Velocity = GetVelocity();
	const float Speed = Velocity.Size2D();
	const bool bDead = HasTag(WRTags::State_Dead);
	const bool bBusy = Now < BodyOneShotUntil;
	const FName Group = IsRangedOut() ? RangedAnimGroup() : NAME_None;
	const bool bBlocking = !IsRangedOut() && HasTag(WRTags::State_Blocking);

	// The layers (UWRAnimInstance): loops over the upper body, swimming over all of it, turn in place, slopes.
	FWRAnimInput& In = Anim->Input;
	In.Speed = bDead ? 0.f : Speed;
	In.ActorYaw = GetActorRotation().Yaw;
	In.Angle = Speed > 1.f ? FRotator::NormalizeAxis(Velocity.Rotation().Yaw - In.ActorYaw) : 0.f;
	In.bGrounded = Move->IsMovingOnGround();
	In.Set = TEXT("hero");
	In.UpperLoop = nullptr;
	In.FullLoop = nullptr;
	In.bFullLoopHold = false;
	In.FullLoopStart = 0.f;
	In.ExtraTwist = BodyAimTurn;
	if (!bDead)
	{
		if (Move->IsSwimming())
		{
			In.FullLoop = BodyClip(Speed > 40.f ? FName(TEXT("swim")) : FName(TEXT("tread")));
		}
		if (bAiming && Group == TEXT("rifle"))
		{
			In.UpperLoop = BodyClip(TEXT("rifle_aim"));  // shouldered crossbow / blowgun over the walking legs
		}
		else if (bAiming && Group == TEXT("bow"))
		{
			In.UpperLoop = BodyClip(TEXT("bow_aim"));
		}
		else if (IsRangedOut() && Group == TEXT("bow"))
		{
			In.UpperLoop = BodyClip(TEXT("ranged_idle"));  // the bow held ready at the side
		}
		else if (bBlocking)
		{
			In.UpperLoop = BodyClip(TEXT("block"));  // shield up, legs walking, strafing or backing off
		}
	}
	In.bTurnInPlace = !bDead && !bBusy && In.bGrounded && !In.FullLoop;

	// Motion matching's input: where the body goes and where the stick says it's about to.
	FWRHeroAnimInput& H = Anim->HeroInput;
	const bool bOwnMove = IsLocallyControlled() || HasAuthority();
	H.Velocity = bDead ? FVector::ZeroVector : Velocity;
	if (bOwnMove)
	{
		H.Acceleration = Move->GetCurrentAcceleration();
	}
	else
	{
		const float Max = Move->GetMaxAcceleration();
		const float Angle = ReplicatedAccel.Angle / 255.f * 2.f * PI;
		const float Size = ReplicatedAccel.Magnitude / 255.f * Max;
		H.Acceleration = FVector(FMath::Cos(Angle) * Size, FMath::Sin(Angle) * Size, ReplicatedAccel.Z / 127.f * Max);
	}
	if (bDead)
	{
		H.Acceleration = FVector::ZeroVector;
	}
	H.MaxSpeed = Move->GetMaxSpeed();
	H.MaxAcceleration = Move->GetMaxAcceleration();
	H.BrakingDeceleration = Move->GetMaxBrakingDeceleration();
	H.TurnRate = Move->RotationRate.Yaw;
	H.bOrientToMovement = Move->bOrientRotationToMovement && !bUseControllerRotationYaw;
	H.DesiredYaw = IsLocallyControlled() && Controller ? float(Controller->GetControlRotation().Yaw) : float(GetActorRotation().Yaw);
	H.bLocal = IsLocallyControlled();
	if (bOwnMove)
	{
		const float Sprint = WRTune(TEXT("player.sprintSpeed"), 750.f);
		H.Gait = H.MaxSpeed >= Sprint - 1.f ? EWRHeroGait::Sprint : H.MaxSpeed <= WRTune(TEXT("anim.mm.walkBelow"), 320.f) ? EWRHeroGait::Walk : EWRHeroGait::Run;
		H.Stance = IsRangedOut() ? (bAiming ? (Group == TEXT("rifle") ? EWRHeroStance::RifleAim : EWRHeroStance::BowAim) : EWRHeroStance::Ranged)
			: bBlocking ? EWRHeroStance::Block : EWRHeroStance::Melee;
		const AWRPlayerController* PC = Cast<AWRPlayerController>(Controller);
		H.bLockedOn = PC && PC->GetLockTarget() != nullptr;
	}
	else
	{
		H.Gait = EWRHeroGait(FMath::Min(ReplicatedAccel.Bits & 0x3, 2));
		H.Stance = EWRHeroStance(FMath::Min((ReplicatedAccel.Bits >> 2) & 0x7, 4));
		H.bLockedOn = (ReplicatedAccel.Bits & 0x20) != 0;
		// Another machine's copy doesn't run the owner's sprint or aim speed: its top speed comes from the gait.
		H.MaxSpeed = H.Gait == EWRHeroGait::Sprint ? WRTune(TEXT("player.sprintSpeed"), 750.f) : FMath::Max(H.MaxSpeed, Speed);
	}
	H.Move = Move->IsSwimming() ? EWRHeroMove::Swim : Move->IsFalling() ? EWRHeroMove::Air
		: (Speed < 10.f && H.Acceleration.SizeSquared2D() < 1.f) ? EWRHeroMove::Idle : EWRHeroMove::Moving;
	if (bDead)
	{
		H.Move = EWRHeroMove::Idle;
	}
	// The spine takes the aim's pitch in first person and while aiming (the arms and the weapon point where you look).
	AimPitchBlend = FMath::FInterpTo(AimPitchBlend, !bDead && (bFirstPerson || IsAimingRanged()) ? 1.f : 0.f, Dt, 8.f);
	H.AimPitch = FRotator::NormalizeAxis(GetBaseAimRotation().Pitch);
	H.AimPitchAlpha = AimPitchBlend;

	// Swimming (WRWater): one float height for the capsule, but the stroke lies along the surface (the body rides higher
	// on the capsule) while treading water stands in it (head and shoulders out: a little lower).
	const float LiftGoal = !bDead && Move->IsSwimming() ? WRTune(Speed > 40.f ? TEXT("water.swimLift") : TEXT("water.treadLift"), Speed > 40.f ? 60.f : -5.f) : 0.f;
	if (!FMath::IsNearlyEqual(SwimLift, LiftGoal, 0.05f))
	{
		SwimLift = FMath::FInterpTo(SwimLift, LiftGoal, Dt, 4.f);
		Body->SetRelativeLocation(FVector(0.f, 0.f, BodyBaseZ + SwimLift));
	}
	// Standing on a slope: where the ground is under each foot, against the capsule's bottom (the instance sinks the
	// hips to the lower one and bends the other leg up to its own).
	const FVector Bottom = GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	for (int32 Side = 0; Side < 2; ++Side)
	{
		float Want = 0.f;
		const FName Foot = Anim->FootBone(Side);
		if (!Foot.IsNone() && !bDead && In.bGrounded && !In.FullLoop && Speed < 30.f && Body->WasRecentlyRendered(0.2f))
		{
			const FVector At = Body->GetSocketLocation(Foot);
			const float Reach = WRTune(TEXT("anim.footMaxRaise"), 45.f);
			FHitResult Ground;
			FCollisionQueryParams Query(SCENE_QUERY_STAT(WRFootPlant), false, this);
			FCollisionObjectQueryParams Floors(ECC_WorldStatic);
			Floors.AddObjectTypesToQuery(ECC_WorldDynamic);
			if (GetWorld()->LineTraceSingleByObjectType(Ground, FVector(At.X, At.Y, Bottom.Z + Reach), FVector(At.X, At.Y, Bottom.Z - Reach), Floors, Query))
			{
				Want = FMath::Clamp(float(Ground.ImpactPoint.Z - Bottom.Z), -Reach, Reach);
			}
		}
		In.FootLift[Side] = FMath::FInterpTo(In.FootLift[Side], Want, Dt, 12.f);
	}
	if (bDead || bBusy)
	{
		return;  // a one-shot is on: nothing new starts until it's done (the gameplay's timing)
	}
	BodyAnim = NAME_None;
	if (HasTag(WRTags::State_Dodging))
	{
		BodyOneShotUntil = Now + PlayBodyAnim(TEXT("dodge"), false, 1.f, WRTune(TEXT("player.dodgeVisualSeconds"), 0.7f));
		return;
	}
	if (!QueuedOneShot.IsNone())
	{
		// The second half of a shot: nock the next arrow / reload the crossbow.
		const FName Key = QueuedOneShot;
		QueuedOneShot = NAME_None;
		if (BodyAnims.Contains(Key) && !Move->IsFalling() && !Move->IsSwimming())
		{
			BodyOneShotUntil = Now + PlayBodyAnim(Key, false, 1.f, QueuedOneShotSeconds);
		}
	}
}

void AWRCharacter::OnSwingVisual(EWRSwingKind Kind, float Duration)
{
	if (!Body->IsVisible())
	{
		return;
	}
	FName Key = Kind == EWRSwingKind::Cast ? FName(TEXT("cast"))
		: (Kind == EWRSwingKind::Heavy || Kind == EWRSwingKind::WeaponArt) ? FName(TEXT("heavy")) : FName(TEXT("light"));
	// A plain swing at a resource node right in front gathers instead: an overhead chop for wood, stone and ore, a crouch
	// for plants. Purely visual (the swing's own hit does the harvesting), so every machine decides from what it sees.
	if (Kind == EWRSwingKind::Light || Kind == EWRSwingKind::Heavy)
	{
		const FVector Here = GetActorLocation(), Forward = GetActorForwardVector();
		bool bGathering = false;
		for (TActorIterator<AWRResourceNode> It(GetWorld()); It; ++It)
		{
			const FVector To = It->GetActorLocation() - Here;
			if (!It->IsDepleted() && To.Size2D() < 280.f && FVector::DotProduct(To.GetSafeNormal2D(), Forward) > 0.5f)
			{
				const FString Item = It->ItemId.ToString();
				const bool bHard = Item == TEXT("wood") || Item == TEXT("stone") || Item.EndsWith(TEXT("_ore")) || Item.StartsWith(TEXT("sulfur"));
				Key = bHard ? FName(TEXT("chop")) : FName(TEXT("gather"));
				bGathering = true;
				break;
			}
		}
		// A kill's body on the ground (WRCarcass.cpp): an overhead chop butchers it.
		for (TActorIterator<AWRCreature> It(GetWorld()); It && !bGathering; ++It)
		{
			const FVector To = It->GetActorLocation() - Here;
			if (It->IsHarvestableBody() && To.Size2D() < 200.f + It->GetSimpleCollisionRadius() && FVector::DotProduct(To.GetSafeNormal2D(), Forward) > 0.5f)
			{
				Key = TEXT("chop");
				bGathering = true;
			}
		}
		// The island's trees and rocks (chop) and shrubs (gather) are instanced decoration, not actors.
		const AWRDressing* Dressing = bGathering ? nullptr : UWRDressingSubsystem::FindDressing(GetWorld());
		FVector Ground;
		if (Dressing && Dressing->FindNearest(Here + Forward * 150.f, TEXT("shrub"), 150.f, Ground))
		{
			Key = TEXT("gather");
		}
		else if (Dressing && Dressing->FindNearest(Here + Forward * 150.f, FString(), 200.f, Ground))
		{
			Key = TEXT("chop");
		}
	}
	BodyAnim = NAME_None;  // restart even if the same set just played
	PlayBodyAnim(Key, false, 1.f, Duration);
	BodyOneShotUntil = GetWorld()->GetTimeSeconds() + Duration;
}

void AWRCharacter::MulticastBodyOneShot_Implementation(FName Key)
{
	if (GetNetMode() == NM_DedicatedServer || !Body->IsVisible() || GetWorld()->GetTimeSeconds() < BodyOneShotUntil)
	{
		return;
	}
	BodyAnim = NAME_None;
	BodyOneShotUntil = GetWorld()->GetTimeSeconds() + PlayBodyAnim(Key, false, 1.f, WRTune(TEXT("player.pickupVisualSeconds"), 1.2f));
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
	// A fishing rod in hand: the attack casts, strikes and reels (WRFishing), Block lets the line go; no swings.
	if (IsHoldingRod() && !IsRangedOut() && (Input == EWRAbilityInput::Light || Input == EWRAbilityInput::Heavy || Input == EWRAbilityInput::Block))
	{
		if (Input == EWRAbilityInput::Light)
		{
			FishChargeLocal = AWRBobber::FindFor(this) ? -1.0 : GetWorld()->GetTimeSeconds();
			ServerFishInput(true);
		}
		else if (Input == EWRAbilityInput::Block)
		{
			ServerFishCancel();
		}
		return;
	}
	// Ranged weapon out: right mouse aims, left mouse fires (heavy has nothing to do).
	if (IsRangedOut() && (Input == EWRAbilityInput::Block || Input == EWRAbilityInput::Light || Input == EWRAbilityInput::Heavy))
	{
		if (Input == EWRAbilityInput::Block) SetAiming(true);
		else if (Input == EWRAbilityInput::Light) FireRanged();
		return;
	}
	if (AbilitySystem)
	{
		AbilitySystem->AbilityLocalInputPressed(static_cast<int32>(Input));
	}
}

void AWRCharacter::ReleaseAbilityInput(EWRAbilityInput Input)
{
	if (Input == EWRAbilityInput::Light && (IsHoldingRod() || FishChargeLocal >= 0.0))
	{
		FishChargeLocal = -1.0;
		ServerFishInput(false);
		return;
	}
	if (Input == EWRAbilityInput::Heavy)
	{
		if (AWRPlayerController* PC = Cast<AWRPlayerController>(Controller); PC && PC->IsPlacing())
		{
			PC->RotatePlacementEnd();
		}
	}
	if (Input == EWRAbilityInput::Block && bAiming)
	{
		SetAiming(false);
		return;
	}
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
	PadLookAction = MakeAction(EInputActionValueType::Axis2D);
	JumpAction = MakeAction(EInputActionValueType::Boolean);
	SprintAction = MakeAction(EInputActionValueType::Boolean);
	ToggleViewAction = MakeAction(EInputActionValueType::Boolean);

	SwapWeaponAction = NewObject<UInputAction>(this);
	AmmoNextAction = NewObject<UInputAction>(this);
	AmmoPrevAction = NewObject<UInputAction>(this);
	// The path abilities (quick slots 1-8) are cast through the ability wheels or their own keys (FWRPlayerUI);
	// the number keys belong to the item hotbar.
	for (const EWRAbilityInput Input : { EWRAbilityInput::Light, EWRAbilityInput::Heavy, EWRAbilityInput::Block, EWRAbilityInput::Dodge })
	{
		AbilityActions.Add(Input, MakeAction(EInputActionValueType::Boolean));
	}
	MapKeys();
}

void AWRCharacter::MapKeys()
{
	// Keys from FWRKeyBindings (rebindable in Settings); rebuilt when they change.
	if (!MappingContext)
	{
		MappingContext = NewObject<UInputMappingContext>(this);
	}
	MappingContext->UnmapAll();
	const auto Map = [this](UInputAction* Action, FName Id) -> FEnhancedActionKeyMapping*
	{
		const FKey Key = FWRKeyBindings::Get().Key(Id);
		return Key.IsValid() ? &MappingContext->MapKey(Action, Key) : nullptr;
	};
	// Forward/back/left/right -> Axis2D (X = right, Y = forward).
	if (FEnhancedActionKeyMapping* M = Map(MoveAction, TEXT("Forward"))) M->Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(this));
	if (FEnhancedActionKeyMapping* M = Map(MoveAction, TEXT("Back"))) { M->Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(this)); M->Modifiers.Add(NewObject<UInputModifierNegate>(this)); }
	if (FEnhancedActionKeyMapping* M = Map(MoveAction, TEXT("Left"))) M->Modifiers.Add(NewObject<UInputModifierNegate>(this));
	Map(MoveAction, TEXT("Right"));
	MappingContext->MapKey(LookAction, EKeys::Mouse2D);
	Map(JumpAction, TEXT("Jump"));
	Map(SprintAction, TEXT("Sprint"));
	Map(ToggleViewAction, TEXT("View"));
	Map(SwapWeaponAction, TEXT("SwapRanged"));
	Map(AmmoNextAction, TEXT("HotbarNext"));
	Map(AmmoPrevAction, TEXT("HotbarPrev"));
	const TPair<EWRAbilityInput, const TCHAR*> AbilityKeys[] = {
		{ EWRAbilityInput::Light, TEXT("Light") }, { EWRAbilityInput::Heavy, TEXT("Heavy") },
		{ EWRAbilityInput::Block, TEXT("Block") }, { EWRAbilityInput::Dodge, TEXT("Dodge") },
	};
	for (const TPair<EWRAbilityInput, const TCHAR*>& Pair : AbilityKeys)
	{
		if (const TObjectPtr<UInputAction>* Action = AbilityActions.Find(Pair.Key)) Map(*Action, Pair.Value);
	}
	// The pad (Data/gamepad.json): the same actions, one verb per button. The move stick gets a radial dead zone.
	const FWRPadLayout& Pad = FWRPadLayout::Get();
	const auto MapPad = [this, &Pad](UInputAction* Action, const TCHAR* Verb) -> FEnhancedActionKeyMapping*
	{
		const FKey Key = Pad.Key(FName(Verb));
		return Action && Key.IsValid() ? &MappingContext->MapKey(Action, Key) : nullptr;
	};
	if (FEnhancedActionKeyMapping* M = MapPad(MoveAction, TEXT("Move")))
	{
		UInputModifierDeadZone* Dead = NewObject<UInputModifierDeadZone>(this);
		Dead->Type = EDeadZoneType::Radial;
		Dead->LowerThreshold = WRTune(TEXT("look.moveDeadZone"), 0.2f);
		M->Modifiers.Add(Dead);
	}
	MapPad(PadLookAction, TEXT("Look"));
	MapPad(JumpAction, TEXT("Jump"));
	MapPad(SprintAction, TEXT("Sprint"));
	MapPad(ToggleViewAction, TEXT("View"));
	const TPair<EWRAbilityInput, const TCHAR*> PadAbilityKeys[] = {
		{ EWRAbilityInput::Light, TEXT("Light") }, { EWRAbilityInput::Heavy, TEXT("Heavy") },
		{ EWRAbilityInput::Block, TEXT("Block") }, { EWRAbilityInput::Dodge, TEXT("Dodge") },
	};
	for (const TPair<EWRAbilityInput, const TCHAR*>& Pair : PadAbilityKeys)
	{
		if (const TObjectPtr<UInputAction>* Action = AbilityActions.Find(Pair.Key)) MapPad(*Action, Pair.Value);
	}
	if (const APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = PC->GetLocalPlayer() ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()) : nullptr)
		{
			Subsystem->RequestRebuildControlMappings();
		}
	}
}

void AWRCharacter::WheelStep(int32 Dir)
{
	// The mouse wheel: turns what you're placing; ammo while aiming a ranged weapon; otherwise the hotbar.
	if (AWRPlayerController* Placing = Cast<AWRPlayerController>(Controller); Placing && Placing->IsPlacing() && !Placing->IsDemolishing())
	{
		Placing->WheelPlacement(Dir);
		return;
	}
	if (IsAimingRanged())
	{
		CycleAmmo(Dir);
		return;
	}
	if (AWRPlayerController* PC = Cast<AWRPlayerController>(Controller))
	{
		if (FWRPlayerUI* UI = PC->GetUI())
		{
			UI->CycleHotbar(Dir);
			return;
		}
	}
	CycleAmmo(Dir);
}

void AWRCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	BuildInput();
	if (!KeysChangedHandle.IsValid())
	{
		KeysChangedHandle = FWRKeyBindings::Get().OnChanged.AddWeakLambda(this, [this]() { MapKeys(); });
	}

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
		Input->BindAction(PadLookAction, ETriggerEvent::Triggered, this, &AWRCharacter::PadLook);
		Input->BindAction(PadLookAction, ETriggerEvent::Completed, this, &AWRCharacter::PadLook);
		Input->BindAction(JumpAction, ETriggerEvent::Started, this, &AWRCharacter::InputJump);
		Input->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		Input->BindAction(SprintAction, ETriggerEvent::Started, this, &AWRCharacter::InputSprint);
		Input->BindAction(SprintAction, ETriggerEvent::Completed, this, &AWRCharacter::StopSprint);
		Input->BindAction(ToggleViewAction, ETriggerEvent::Started, this, &AWRCharacter::InputView);
		Input->BindAction(SwapWeaponAction, ETriggerEvent::Started, this, &AWRCharacter::ToggleWeaponSet);
		Input->BindAction(AmmoNextAction, ETriggerEvent::Started, this, &AWRCharacter::AmmoNext);
		Input->BindAction(AmmoPrevAction, ETriggerEvent::Started, this, &AWRCharacter::AmmoPrev);
		for (const TPair<EWRAbilityInput, TObjectPtr<UInputAction>>& Pair : AbilityActions)
		{
			Input->BindAction(Pair.Value, ETriggerEvent::Started, this, &AWRCharacter::InputPressed, Pair.Key);
			Input->BindAction(Pair.Value, ETriggerEvent::Completed, this, &AWRCharacter::InputReleased, Pair.Key);
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
	WRInput::MouseLook(this, Value.Get<FVector2D>());
}

void AWRCharacter::PadLook(const FInputActionValue& Value)
{
	const FVector2D Stick = Value.Get<FVector2D>();
	AWRPlayerController* PC = Cast<AWRPlayerController>(Controller);
	if (PC && PC->GetLockTarget())
	{
		// Locked on, the camera follows the target: a flick of the right stick moves the lock to the next one that way.
		const float Mag = Stick.Size();
		if (Mag < WRTune(TEXT("look.flickRearm"), 0.3f)) bPadFlickReady = true;
		else if (bPadFlickReady && Mag > WRTune(TEXT("look.flickThreshold"), 0.6f))
		{
			bPadFlickReady = false;
			PC->FlickLock(Stick);
		}
		return;
	}
	bPadFlickReady = true;
	WRInput::PadLook(this, Stick, GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f);
}

void AWRCharacter::InputPressed(EWRAbilityInput Input)
{
	if (!WRInput::AllowPress(Cast<APlayerController>(Controller))) return;
	HeldInputs.Add(Input);
	PressAbilityInput(Input);
}

void AWRCharacter::InputReleased(EWRAbilityInput Input)
{
	// Only a press this body took is let go (one dropped by a menu already was; one the gate stopped never began).
	if (HeldInputs.Remove(Input) == 0) return;
	ReleaseAbilityInput(Input);
}

void AWRCharacter::InputJump()
{
	if (WRInput::AllowPress(Cast<APlayerController>(Controller))) Jump();
}

void AWRCharacter::InputSprint()
{
	if (WRInput::AllowPress(Cast<APlayerController>(Controller))) StartSprint();
}

void AWRCharacter::InputView()
{
	if (WRInput::AllowPress(Cast<APlayerController>(Controller))) ToggleView();
}

void AWRCharacter::DropHeldInputs()
{
	const TArray<EWRAbilityInput> Held = HeldInputs.Array();
	HeldInputs.Reset();
	for (const EWRAbilityInput Input : Held) ReleaseAbilityInput(Input);
	StopSprint();
	StopJumping();
}

void AWRCharacter::StartSprint()
{
	CastChecked<UWRCharacterMovementComponent>(GetCharacterMovement())->bWantsToSprint = true;
}

void AWRCharacter::StopSprint()
{
	CastChecked<UWRCharacterMovementComponent>(GetCharacterMovement())->bWantsToSprint = false;
}
