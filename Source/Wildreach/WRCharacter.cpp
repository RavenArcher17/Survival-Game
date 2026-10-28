#include "WRCharacter.h"
#include "WRAnimInstance.h"
#include "WRCameraBoom.h"
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
#include "WRKeyBindings.h"
#include "WRPlayerUI.h"
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
		}
		BodyAimTurn = FMath::FInterpTo(BodyAimTurn, IsAimingRanged() ? RangedAimYaw : 0.f, DeltaSeconds, 10.f);
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

	// Feet on the capsule's bottom, facing the actor's forward.
	Body->SetSkeletalMesh(BodyAsset);
	const FBox Box = BodyAsset->GetBounds().GetBox();
	Body->SetRelativeScale3D(FVector(Scale));
	BodyBaseZ = -GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - Box.Min.Z * Scale;
	SwimLift = 0.f;
	Body->SetRelativeLocation(FVector(0.f, 0.f, BodyBaseZ));
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
	// The layered animation instance (no animation blueprint), set up after the mesh so it measures at its scale.
	Body->SetAnimInstanceClass(UWRAnimInstance::StaticClass());
	SetupBodyAnim(Art, Variant);
	UpdateWeaponLook();  // a ranged weapon may already be out
}

UWRAnimInstance* AWRCharacter::BodyAnimInstance() const
{
	return Cast<UWRAnimInstance>(Body->GetAnimInstance());
}

UAnimSequenceBase* AWRCharacter::BodyClip(FName Key) const
{
	const FBodyAnimSet* Set = BodyAnims.Find(Key);
	return Set && Set->Clips.Num() > 0 ? Set->Clips[0].Get() : nullptr;
}

void AWRCharacter::SetupBodyAnim(const TSharedPtr<FJsonObject>& Art, const TSharedPtr<FJsonObject>& Variant)
{
	UWRAnimInstance* Anim = BodyAnimInstance();
	if (!Anim)
	{
		UE_LOG(LogWildreach, Warning, TEXT("Player art: no animation instance on the body"));
		return;
	}
	Anim->ResetSetup();
	const auto Pick = [&Art, &Variant](const TCHAR* Field) { return Variant.IsValid() && Variant->HasField(Field) ? Variant : Art; };
	// Paces from "speeds" (cm/s at play rate 1) where given; the rest are measured from the clips' feet.
	const TSharedPtr<FJsonObject>* Speeds = nullptr;
	Pick(TEXT("speeds"))->TryGetObjectField(TEXT("speeds"), Speeds);
	const auto Pace = [Speeds](const FString& Key)
	{
		double Value = 0.0;
		if (Speeds) (*Speeds)->TryGetNumberField(Key, Value);
		return float(Value);
	};
	// A set: an idle and walk / run clips forward, back, left and right (<walk>, <walk>_back, <walk>_left, ...).
	const auto AddSet = [&](FName Name, const FString& Idle, const FString& Walk, const FString& Run)
	{
		TArray<FWRLocoClipDesc> Clips;
		const TPair<const TCHAR*, float> Dirs[] = { { TEXT(""), 0.f }, { TEXT("_back"), 180.f }, { TEXT("_left"), -90.f }, { TEXT("_right"), 90.f } };
		for (int32 Gait = 0; Gait < 2; ++Gait)
		{
			const FString& Base = Gait == 0 ? Walk : Run;
			for (const TPair<const TCHAR*, float>& Dir : Dirs)
			{
				const FString Key = Base + Dir.Key;
				if (UAnimSequenceBase* Clip = Base.IsEmpty() ? nullptr : BodyClip(FName(*Key)))
				{
					Clips.Add({ Clip, Dir.Value, Gait, Pace(Key) });
				}
			}
		}
		Anim->AddLocoSet(Name, BodyClip(FName(*Idle)), Clips);
	};
	const auto Have = [this](const TCHAR* Key, const TCHAR* Fallback) { return FString(BodyAnims.Contains(Key) ? Key : Fallback); };
	AddSet(TEXT("melee"), TEXT("idle"), TEXT("walk"), TEXT("run"));
	AddSet(TEXT("ranged"), Have(TEXT("ranged_idle"), TEXT("idle")), Have(TEXT("ranged_walk"), TEXT("walk")), Have(TEXT("ranged_run"), TEXT("run")));
	if (BodyAnims.Contains(TEXT("bow_aimwalk")))
	{
		AddSet(TEXT("bow_aim"), TEXT("bow_aim"), TEXT("bow_aimwalk"), FString());  // aiming walks only (half speed)
	}
	Anim->SetTurnClips(TEXT("melee"), BodyClip(TEXT("turn_left")), BodyClip(TEXT("turn_right")));
	const FName RangedTurn[] = { BodyAnims.Contains(TEXT("ranged_turn_left")) ? FName(TEXT("ranged_turn_left")) : FName(TEXT("turn_left")),
	                             BodyAnims.Contains(TEXT("ranged_turn_right")) ? FName(TEXT("ranged_turn_right")) : FName(TEXT("turn_right")) };
	Anim->SetTurnClips(TEXT("ranged"), BodyClip(RangedTurn[0]), BodyClip(RangedTurn[1]));
	Anim->SetTurnClips(TEXT("bow_aim"), BodyClip(RangedTurn[0]), BodyClip(RangedTurn[1]));

	// The upper body (block, swings, casts, shots) from this bone up; the spine bones share the twist that keeps the
	// chest facing the camera while the legs stand.
	FString Upper = TEXT("Spine");
	Pick(TEXT("upperBody"))->TryGetStringField(TEXT("upperBody"), Upper);
	TArray<FName> Spine;
	const TArray<TSharedPtr<FJsonValue>>* SpineList = nullptr;
	if (Pick(TEXT("spineBones"))->TryGetArrayField(TEXT("spineBones"), SpineList))
	{
		for (const TSharedPtr<FJsonValue>& Bone : *SpineList) Spine.Add(FName(*Bone->AsString()));
	}
	else
	{
		Spine = { TEXT("Spine"), TEXT("Spine1"), TEXT("Spine2") };
	}
	Anim->SetUpperBody(FName(*Upper), Spine);
	TArray<FName> Legs;
	const TArray<TSharedPtr<FJsonValue>>* LegList = nullptr;
	if (Pick(TEXT("legBones"))->TryGetArrayField(TEXT("legBones"), LegList))
	{
		for (const TSharedPtr<FJsonValue>& Bone : *LegList) Legs.Add(FName(*Bone->AsString()));
	}
	Anim->SetLegs(Legs);  // feet planted on slopes while standing
	FullBodyKeys.Reset();
	const TArray<TSharedPtr<FJsonValue>>* Full = nullptr;
	if (Pick(TEXT("fullBody"))->TryGetArrayField(TEXT("fullBody"), Full))
	{
		for (const TSharedPtr<FJsonValue>& Key : *Full) FullBodyKeys.Add(FName(*Key->AsString()));
	}
	else
	{
		FullBodyKeys = { TEXT("dodge"), TEXT("death"), TEXT("fall"), TEXT("swim"), TEXT("tread"), TEXT("gather"), TEXT("chop"), TEXT("pickup") };
	}
	UE_LOG(LogWildreach, Log, TEXT("Player anim: walk %.0f back %.0f left %.0f right %.0f | run %.0f back %.0f left %.0f right %.0f cm/s"),
		Anim->LocoSpeed(TEXT("melee"), 0, 0.f), Anim->LocoSpeed(TEXT("melee"), 0, 180.f), Anim->LocoSpeed(TEXT("melee"), 0, -90.f), Anim->LocoSpeed(TEXT("melee"), 0, 90.f),
		Anim->LocoSpeed(TEXT("melee"), 1, 0.f), Anim->LocoSpeed(TEXT("melee"), 1, 180.f), Anim->LocoSpeed(TEXT("melee"), 1, -90.f), Anim->LocoSpeed(TEXT("melee"), 1, 90.f));
}

float AWRCharacter::PlayBodyAnim(FName Key, bool bLoop, float PlayRate, float Duration)
{
	FBodyAnimSet* Set = BodyAnims.Find(Key);
	UWRAnimInstance* Anim = BodyAnimInstance();
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
	UWRAnimInstance* Anim = BodyAnimInstance();
	if (!Body->IsVisible() || BodyAnims.Num() == 0 || !Anim)
	{
		return;
	}
	const UCharacterMovementComponent* Move = GetCharacterMovement();
	const double Now = GetWorld()->GetTimeSeconds();
	const FVector Velocity = GetVelocity();
	const float Speed = Velocity.Size2D();
	const bool bDead = HasTag(WRTags::State_Dead);
	const bool bBusy = Now < BodyOneShotUntil;
	const FName Group = IsRangedOut() ? RangedAnimGroup() : NAME_None;

	// What the body is doing this frame (the instance blends between states).
	FWRAnimInput& In = Anim->Input;
	In.Speed = bDead ? 0.f : Speed;
	In.ActorYaw = GetActorRotation().Yaw;
	In.Angle = Speed > 1.f ? FRotator::NormalizeAxis(Velocity.Rotation().Yaw - In.ActorYaw) : 0.f;  // strafes, walking back
	In.bGrounded = Move->IsMovingOnGround();
	In.Set = !IsRangedOut() ? FName(TEXT("melee")) : bAiming && Group == TEXT("bow") && BodyAnims.Contains(TEXT("bow_aimwalk")) ? FName(TEXT("bow_aim")) : FName(TEXT("ranged"));
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
		else if (Move->IsFalling())
		{
			// Plays once and holds the airborne pose, skipping the crouch and take-off.
			In.FullLoop = BodyClip(TEXT("fall"));
			In.bFullLoopHold = true;
			In.FullLoopStart = In.FullLoop ? In.FullLoop->GetPlayLength() * WRTune(TEXT("player.fallClipStart"), 0.35f) : 0.f;
		}
		if (bAiming && Group == TEXT("rifle"))
		{
			In.UpperLoop = BodyClip(TEXT("rifle_aim"));  // shouldered crossbow / blowgun over the walking legs
		}
		else if (bAiming && Group == TEXT("bow") && !BodyAnims.Contains(TEXT("bow_aimwalk")))
		{
			In.UpperLoop = BodyClip(TEXT("bow_aim"));
		}
		else if (!IsRangedOut() && HasTag(WRTags::State_Blocking))
		{
			In.UpperLoop = BodyClip(TEXT("block"));  // shield up, legs walking, strafing or backing off
		}
	}
	In.bTurnInPlace = !bDead && !bBusy && In.bGrounded && !In.FullLoop;
	// Standing on a slope: where the ground is under each foot, against the capsule's bottom (the instance sinks the
	// hips to the lower one and bends the other leg up to its own).
	const float Dt = GetWorld()->GetDeltaSeconds();
	// Swimming (WRWater): one float height for the capsule, but the stroke lies along the surface (the body rides higher
	// on the capsule) while treading water stands in it (head and shoulders out: a little lower).
	const float LiftGoal = !bDead && Move->IsSwimming() ? WRTune(Speed > 40.f ? TEXT("water.swimLift") : TEXT("water.treadLift"), Speed > 40.f ? 70.f : -5.f) : 0.f;
	if (!FMath::IsNearlyEqual(SwimLift, LiftGoal, 0.05f))
	{
		SwimLift = FMath::FInterpTo(SwimLift, LiftGoal, Dt, 4.f);
		Body->SetRelativeLocation(FVector(0.f, 0.f, BodyBaseZ + SwimLift));
	}
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
	// The mouse wheel: ammo while aiming a ranged weapon, otherwise the hotbar.
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
