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
		if (IsLocallyControlled())
		{
			TickAimCamera(DeltaSeconds);
		}
		const float Turn = FMath::FInterpTo(BodyAimTurn, IsAimingRanged() ? RangedAimYaw : 0.f, DeltaSeconds, 10.f);
		if (bBodyLoaded && !FMath::IsNearlyEqual(Turn, BodyAimTurn, 0.01f))
		{
			Body->SetRelativeRotation(FRotator(0.f, BodyYaw + Turn, 0.f));
		}
		BodyAimTurn = Turn;
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
	ShieldMesh->SetOwnerNoSee(bFirstPerson);  // it would fill the first-person view; the sword stays

	// First person: body yaw follows the camera. Third person: body turns toward movement.
	bUseControllerRotationYaw = bFirstPerson;
	GetCharacterMovement()->bOrientRotationToMovement = !bFirstPerson;
}

// ---------------------------------------------------------------- body art

void AWRCharacter::LoadBodyArt()
{
	if (GetNetMode() == NM_DedicatedServer || (bBodyLoaded && LoadedBodyStyle == BodyStyle))
	{
		return;  // the server never draws bodies
	}
	const TSharedPtr<FJsonObject> Art = FWRTuning::LoadDataFile(TEXT("player_art.json"));
	if (!Art.IsValid())
	{
		return;
	}
	// A body variant ("bodies": {"f": {...}}) overrides any top-level field it has.
	TSharedPtr<FJsonObject> Variant;
	const TSharedPtr<FJsonObject>* Bodies = nullptr;
	const TSharedPtr<FJsonObject>* Found = nullptr;
	if (!BodyStyle.IsNone() && Art->TryGetObjectField(TEXT("bodies"), Bodies) && (*Bodies)->TryGetObjectField(BodyStyle.ToString(), Found))
	{
		Variant = *Found;
	}
	const auto Pick = [&Art, &Variant](const TCHAR* Field) { return Variant.IsValid() && Variant->HasField(Field) ? Variant : Art; };
	FString MeshPath, Prefix, Socket, WeaponPath, ShieldPath, ShieldSocket;
	if (!Pick(TEXT("mesh"))->TryGetStringField(TEXT("mesh"), MeshPath))
	{
		return;
	}
	USkeletalMesh* BodyAsset = LoadObject<USkeletalMesh>(nullptr, *MeshPath);
	if (!BodyAsset)
	{
		UE_LOG(LogWildreach, Warning, TEXT("Player art: mesh %s not found, keeping the placeholder"), *MeshPath);
		return;
	}
	LoadedBodyStyle = BodyStyle;
	bBodyLoaded = true;
	BodyAnims.Reset();
	BodyAnimRefs.Reset();
	BodyAnim = NAME_None;
	BodyOneShotUntil = 0.0;
	Pick(TEXT("animPrefix"))->TryGetStringField(TEXT("animPrefix"), Prefix);
	Pick(TEXT("weaponSocket"))->TryGetStringField(TEXT("weaponSocket"), Socket);
	Pick(TEXT("weaponMesh"))->TryGetStringField(TEXT("weaponMesh"), WeaponPath);
	Pick(TEXT("shieldMesh"))->TryGetStringField(TEXT("shieldMesh"), ShieldPath);
	Pick(TEXT("shieldSocket"))->TryGetStringField(TEXT("shieldSocket"), ShieldSocket);
	const double Yaw = Pick(TEXT("meshYaw"))->HasField(TEXT("meshYaw")) ? Pick(TEXT("meshYaw"))->GetNumberField(TEXT("meshYaw")) : 0.0;
	const double Scale = Pick(TEXT("meshScale"))->HasField(TEXT("meshScale")) ? Pick(TEXT("meshScale"))->GetNumberField(TEXT("meshScale")) : 1.0;
	const TSharedPtr<FJsonObject>* Speeds = nullptr;
	if (Pick(TEXT("speeds"))->TryGetObjectField(TEXT("speeds"), Speeds))
	{
		(*Speeds)->TryGetNumberField(TEXT("walk"), WalkClipSpeed);
		(*Speeds)->TryGetNumberField(TEXT("run"), RunClipSpeed);
	}

	// Feet on the capsule's bottom, facing the actor's forward.
	Body->SetSkeletalMesh(BodyAsset);
	const FBox Box = BodyAsset->GetBounds().GetBox();
	Body->SetRelativeScale3D(FVector(Scale));
	Body->SetRelativeLocation(FVector(0.f, 0.f, -GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - Box.Min.Z * Scale));
	BodyYaw = Yaw;
	BodyAimTurn = 0.f;
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
		UStaticMesh* Sword = WeaponPath.IsEmpty() ? nullptr : LoadObject<UStaticMesh>(nullptr, *WeaponPath);
		if (Sword)
		{
			// A modelled sword (Tools/art/normalize_gear.py: grip at the origin, blade along the longest axis, crossguard
			// along the next). A palm-down fist holds it flat: blade forward, crossguard along the fingers.
			const FBox SwordBox = Sword->GetBoundingBox();
			const FVector Size = SwordBox.GetSize();
			const int32 Long = Size.X >= Size.Y && Size.X >= Size.Z ? 0 : (Size.Y >= Size.Z ? 1 : 2);
			const int32 Guard = Size[(Long + 1) % 3] >= Size[(Long + 2) % 3] ? (Long + 1) % 3 : (Long + 2) % 3;
			FVector LocalBlade = FVector::ZeroVector, LocalGuard = FVector::ZeroVector;
			LocalBlade[Long] = SwordBox.Max[Long] >= -SwordBox.Min[Long] ? 1.f : -1.f;  // the blade is the long side of the grip
			LocalGuard[Guard] = 1.f;
			const FVector Fingers = HandCS.InverseTransformPositionNoScale(FingerCS);
			const FQuat Rot = FQuat(FRotationMatrix::MakeFromXY(Blade, Fingers)) * FQuat(FRotationMatrix::MakeFromXY(LocalBlade, LocalGuard)).Inverse();
			WeaponMesh->SetStaticMesh(Sword);
			WeaponMesh->EmptyOverrideMaterials();
			WeaponMesh->SetRelativeRotation(Rot.Rotator());
			WeaponMesh->SetRelativeLocation(Grip);
			WeaponMesh->SetRelativeScale3D(FVector::OneVector);
		}
		else
		{
			WeaponMesh->SetRelativeRotation(FRotationMatrix::MakeFromY(Blade).Rotator());  // the cube's long axis is its Y
			WeaponMesh->SetRelativeLocation(Grip + Blade * 40.f);                               // centre: blade ahead, short grip behind
			WeaponMesh->SetRelativeScale3D(FVector(0.05f, 0.95f, 0.1f));
		}
		bArtWeapon = true;
	}

	// Shield on the outside of the forearm (the back of the arm), centred a little past its middle, its back resting on
	// the arm. The skeleton's reference pose isn't always the T-pose (Unreal can take a clip's first frame), so the face
	// is worked out from the arm itself: away from the arm, between up and out from the body's centre line. In a T-pose
	// (palms down) that's straight up; with the arms hanging (palms in) it's out to the side. Both are the back of the arm.
	UStaticMesh* Shield = ShieldPath.IsEmpty() ? nullptr : LoadObject<UStaticMesh>(nullptr, *ShieldPath);
	const int32 ArmBone = Shield ? Ref.FindBoneIndex(FName(*ShieldSocket)) : INDEX_NONE;
	bShieldShown = ArmBone != INDEX_NONE;
	ShieldMesh->SetVisibility(bShieldShown);
	if (ArmBone != INDEX_NONE)
	{
		const FTransform ArmCS = FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, ArmBone);
		FVector WristCS = ArmCS.GetLocation();
		for (int32 Bone = 0; Bone < Ref.GetNum(); ++Bone)
		{
			if (Ref.GetParentIndex(Bone) == ArmBone)
			{
				WristCS = FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, Bone).GetLocation();
				break;
			}
		}
		const FVector ArmDir = (WristCS - ArmCS.GetLocation()).GetSafeNormal();
		const FVector Side = FVector(ArmCS.GetLocation().X, ArmCS.GetLocation().Y, 0.f).GetSafeNormal();  // the mesh origin is on the centre line
		const FVector Away = FVector::VectorPlaneProject(FVector::UpVector + Side, ArmDir).GetSafeNormal();
		const FVector Out = ArmCS.InverseTransformVectorNoScale(Away);
		const FVector Along = ArmCS.InverseTransformVectorNoScale(WristCS - ArmCS.GetLocation());
		const FVector Size = Shield->GetBoundingBox().GetSize();
		const int32 Thin = Size.X <= Size.Y && Size.X <= Size.Z ? 0 : (Size.Y <= Size.Z ? 1 : 2);
		FVector LocalFace = FVector::ZeroVector;
		LocalFace[Thin] = 1.f;  // normalize_gear.py faces the front along +Z (Unreal's glTF import keeps it)
		const FVector LocalUp = Thin == 2 ? FVector::YAxisVector : FVector::ZAxisVector;
		const FQuat Rot = FQuat(FRotationMatrix::MakeFromXY(Out, Along)) * FQuat(FRotationMatrix::MakeFromXY(LocalFace, LocalUp)).Inverse();
		const FVector Centre = ArmCS.InverseTransformPositionNoScale(FMath::Lerp(ArmCS.GetLocation(), WristCS, WRTune(TEXT("player.shieldAlongArm"), 0.55f)));
		ShieldMesh->SetStaticMesh(Shield);
		ShieldMesh->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName(*ShieldSocket));
		ShieldMesh->SetRelativeRotation(Rot.Rotator());
		ShieldMesh->SetRelativeLocation(Centre + Out.GetSafeNormal() * (Size[Thin] * 0.5f + WRTune(TEXT("player.shieldOffArm"), 4.f)));
	}

	const TSharedPtr<FJsonObject>* Anims = nullptr;
	if (Pick(TEXT("anims"))->TryGetObjectField(TEXT("anims"), Anims))
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
	UpdateWeaponLook();  // a ranged weapon may already be out
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
	if (!QueuedOneShot.IsNone())
	{
		// The second half of a shot: nock the next arrow / reload the crossbow.
		const FName Key = QueuedOneShot;
		QueuedOneShot = NAME_None;
		if (BodyAnims.Contains(Key) && !Move->IsFalling() && !Move->IsSwimming())
		{
			BodyOneShotUntil = GetWorld()->GetTimeSeconds() + PlayBodyAnim(Key, false, 1.f, QueuedOneShotSeconds);
			return;
		}
	}
	const auto Pick = [this](const TCHAR* Want, const TCHAR* Fallback) { return BodyAnims.Contains(Want) ? FName(Want) : FName(Fallback); };
	if (Move->IsSwimming())
	{
		PlayBodyAnim(Speed > 40.f ? FName(TEXT("swim")) : FName(TEXT("tread")), true);
	}
	else if (Move->IsFalling())
	{
		if (BodyAnim != TEXT("fall"))
		{
			const float Length = PlayBodyAnim(TEXT("fall"), false);  // plays once and holds the airborne pose
			Body->SetPosition(Length * WRTune(TEXT("player.fallClipStart"), 0.35f));  // skip the crouch and take-off
		}
	}
	else if (IsRangedOut())
	{
		// Ranged weapon out: its own stance, and the aim pose while aiming (bow: drawn; crossbow / blowgun: shouldered).
		const FName Group = RangedAnimGroup();
		if (bAiming && Group == TEXT("bow"))
		{
			PlayBodyAnim(Speed > 25.f ? Pick(TEXT("bow_aimwalk"), TEXT("bow_aim")) : FName(TEXT("bow_aim")), true);
		}
		else if (bAiming && Group == TEXT("rifle"))
		{
			PlayBodyAnim(TEXT("rifle_aim"), true);
		}
		else if (Speed > 280.f)
		{
			PlayBodyAnim(Pick(TEXT("ranged_run"), TEXT("run")), true, FMath::Clamp(Speed / RunClipSpeed, 0.7f, 1.8f));
		}
		else if (Speed > 25.f)
		{
			PlayBodyAnim(Pick(TEXT("ranged_walk"), TEXT("walk")), true, FMath::Clamp(Speed / WalkClipSpeed, 0.6f, 1.8f));
		}
		else
		{
			PlayBodyAnim(Pick(TEXT("ranged_idle"), TEXT("idle")), true);
		}
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
	FName Key = Kind == EWRSwingKind::Cast ? FName(TEXT("cast"))
		: (Kind == EWRSwingKind::Heavy || Kind == EWRSwingKind::WeaponArt) ? FName(TEXT("heavy")) : FName(TEXT("light"));
	// A plain swing at a resource node right in front gathers instead: an overhead chop for wood, stone and ore, a crouch
	// for plants. Purely visual (the swing's own hit does the harvesting), so every machine decides from what it sees.
	if (Kind == EWRSwingKind::Light || Kind == EWRSwingKind::Heavy)
	{
		const FVector Here = GetActorLocation(), Forward = GetActorForwardVector();
		for (TActorIterator<AWRResourceNode> It(GetWorld()); It; ++It)
		{
			const FVector To = It->GetActorLocation() - Here;
			if (!It->IsDepleted() && To.Size2D() < 280.f && FVector::DotProduct(To.GetSafeNormal2D(), Forward) > 0.5f)
			{
				const FString Item = It->ItemId.ToString();
				const bool bHard = Item == TEXT("wood") || Item == TEXT("stone") || Item.EndsWith(TEXT("_ore")) || Item.StartsWith(TEXT("sulfur"));
				Key = bHard ? FName(TEXT("chop")) : FName(TEXT("gather"));
				break;
			}
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
	SwapWeaponAction = NewObject<UInputAction>(this);
	MappingContext->MapKey(SwapWeaponAction, EKeys::G);
	AmmoNextAction = NewObject<UInputAction>(this);
	MappingContext->MapKey(AmmoNextAction, EKeys::MouseScrollUp);
	AmmoPrevAction = NewObject<UInputAction>(this);
	MappingContext->MapKey(AmmoPrevAction, EKeys::MouseScrollDown);

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
		Input->BindAction(SwapWeaponAction, ETriggerEvent::Started, this, &AWRCharacter::ToggleWeaponSet);
		Input->BindAction(AmmoNextAction, ETriggerEvent::Started, this, &AWRCharacter::AmmoNext);
		Input->BindAction(AmmoPrevAction, ETriggerEvent::Started, this, &AWRCharacter::AmmoPrev);
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
