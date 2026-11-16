// Ranged weapons on the survivor: bow, crossbow, blowgun, throwing spears and bolas (Data/items.json "ranged").
// G swaps between sword-and-shield and the ranged weapon; with it out, right mouse aims (over-the-shoulder zoom,
// slower, facing the crosshair) and left mouse fires along the crosshair. The server spends the ammo, keeps the
// weapon's reload, and launches a real projectile (WRProjectile) with drop.
#include "WRCharacter.h"
#include "WRKeyBindings.h"
#include "WRAbilitiesTide.h"
#include "WROceanBosses.h"
#include "WRFx.h"
#include "WRWorldSystems.h"
#include "WRPlayerState.h"
#include "WRCreature.h"
#include "WRUnderwater.h"
#include "WRPlayerController.h"
#include "WRWindPath.h"
#include "WRVoidWorld.h"
#include "WRSwampSystems.h"
#include "WRForestHazards.h"
#include "WRSurvival.h"
#include "WRWeapons.h"
#include "WRSound.h"
#include "Wildreach.h"
#include "WRCameraBoom.h"
#include "WRCharacterMovementComponent.h"
#include "WRCombatLibrary.h"
#include "WRGameplayTags.h"
#include "WRInventory.h"
#include "WRProjectile.h"
#include "WRDesertHazards.h"
#include "WRResourceNode.h"
#include "WRStatusComponent.h"
#include "WRTuning.h"
#include "AnimationRuntime.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "GameFramework/SpringArmComponent.h"
#include "TimerManager.h"

namespace
{
	const FWRItemDef* ItemDef(FName Id) { return Id.IsNone() ? nullptr : FWRItemRegistry::Get().Find(Id); }

	/** "+X" / "-Y" / ... -> unit vector; anything else -> Fallback. */
	FVector ParseAxis(const FString& Axis, const FVector& Fallback)
	{
		if (Axis.Len() != 2) return Fallback;
		const float Sign = Axis[0] == TEXT('-') ? -1.f : 1.f;
		switch (FChar::ToUpper(Axis[1]))
		{
		case TEXT('X'): return FVector(Sign, 0.f, 0.f);
		case TEXT('Y'): return FVector(0.f, Sign, 0.f);
		case TEXT('Z'): return FVector(0.f, 0.f, Sign);
		default: return Fallback;
		}
	}

	/** A mesh's long axis, pointing to whichever side of its origin reaches further (the tip / the blade). */
	FVector LongAxis(const FBox& Box)
	{
		const FVector Size = Box.GetSize();
		const int32 A = Size.X >= Size.Y && Size.X >= Size.Z ? 0 : (Size.Y >= Size.Z ? 1 : 2);
		FVector V = FVector::ZeroVector;
		V[A] = Box.Max[A] >= -Box.Min[A] ? 1.f : -1.f;
		return V;
	}

	FVector ThinAxis(const FBox& Box)
	{
		const FVector Size = Box.GetSize();
		FVector V = FVector::ZeroVector;
		V[Size.X <= Size.Y && Size.X <= Size.Z ? 0 : (Size.Y <= Size.Z ? 1 : 2)] = 1.f;
		return V;
	}
}

FName AWRCharacter::RangedAnimGroup() const
{
	const FWRItemDef* Def = ItemDef(RangedItem);
	if (!Def) return NAME_None;
	if (Def->IsThrowable()) return TEXT("throw");
	return Def->RangedKind == TEXT("bow") ? FName(TEXT("bow")) : FName(TEXT("rifle"));
}

// ---------------------------------------------------------------- equipment

void AWRCharacter::SetRangedItem(FName ItemId)
{
	RangedItem = ItemId;
	if (RangedItem.IsNone())
	{
		bAiming = false;
	}
	const FWRItemDef* Def = ItemDef(RangedItem);
	const FWRItemDef* Ammo = ItemDef(SelectedAmmo);
	if (!Def || !Ammo || Ammo->AmmoOf != Def->AmmoKind)
	{
		const UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(this);
		SelectedAmmo = Def && Inv && !Def->AmmoKind.IsNone() ? Inv->FindAmmo(Def->AmmoKind, NAME_None) : NAME_None;
	}
	OnRep_RangedLook();
}

void AWRCharacter::ToggleWeaponSet()
{
	// A toggled guard / aim belongs to the weapon it was raised with: swapping lets it go.
	if (bBlockToggled && HeldInputs.Contains(EWRAbilityInput::Block))
	{
		bBlockToggled = false;
		HeldInputs.Remove(EWRAbilityInput::Block);
		ReleaseAbilityInput(EWRAbilityInput::Block);
	}
	SetRangedMode(!bRangedMode);
}

void AWRCharacter::SetRangedMode(bool bOn)
{
	if (bOn == bRangedMode)
	{
		return;
	}
	bRangedMode = bOn;
	if (!bOn)
	{
		SetAiming(false);
	}
	OnRep_RangedLook();
	if (!HasAuthority())
	{
		ServerSetRangedMode(bOn);
	}
	else if (bOn && !RangedItem.IsNone())
	{
		MulticastBodyOneShot(TEXT("ranged_equip"));
	}
}

void AWRCharacter::ServerSetRangedMode_Implementation(bool bOn)
{
	SetRangedMode(bOn);
}

void AWRCharacter::OnRep_RangedLook()
{
	if (!IsAimingRanged() && bAiming && HasAuthority())
	{
		bAiming = false;
	}
	UpdateWeaponLook();
}

void AWRCharacter::UpdateWeaponLook()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const bool bRanged = IsRangedOut();
	const FWRItemDef* Def = bRanged ? ItemDef(RangedItem) : nullptr;
	UStaticMesh* Model = Def && !Def->HeldMesh.IsEmpty() ? LoadObject<UStaticMesh>(nullptr, *Def->HeldMesh) : nullptr;
	// The weapon hand shows what it holds: the sword, a tool built from shapes (WRHeldTool.cpp), or nothing.
	const bool bTool = !bRanged && Body->IsVisible() && (FWRGathering::FindTool(MeleeItem) != nullptr || IsClassWeapon(MeleeItem));
	// A fire pot in the throwing hand is modelled in code too (WRRaidLook), drawn where a tool would be.
	const bool bPot = bRanged && Def && Def->RangedKind == TEXT("firepot") && Body->IsVisible();
	WeaponMesh->SetVisibility(!bRanged && !bTool && !MeleeItem.IsNone());
	UpdateToolLook(bTool || bPot);
	// The shield comes off the arm when two-handing (or with a great weapon: combat.md 2.4.0).
	// ...and while a torch is lit in that hand (WRSurvival); the torch or lantern itself (WRHeldLight.cpp).
	ShieldMesh->SetVisibility(!bRanged && bShieldShown && !FWRWeapons::IsTwoHanding(this) && !UWRSurvivalComponent::HoldsTorch(this));
	UpdateHeldLightLook();
	RangedMesh->SetVisibility(Model != nullptr && Body->IsVisible());
	if (Model && Body->IsVisible())
	{
		AttachHeld(RangedMesh, Model, *Def);
	}
	// A worn weapon glow follows the weapon into the hand (WREndgameLook).
	ApplyDeedsLook();
}

void AWRCharacter::AttachHeld(UStaticMeshComponent* Comp, UStaticMesh* Model, const FWRItemDef& Def)
{
	const USkeletalMesh* BodyAsset = Body->GetSkeletalMeshAsset();
	if (!BodyAsset)
	{
		return;
	}
	// Held in a hand socket (weapon_r, bow_l: the palm's frame, its origin in the palm). The item's directions ("along",
	// "face") are read from the body: the skeleton's reference pose, or a clip's pose ("pose") for gear gripped the same
	// way in every clip (a crossbow runs from the trigger hand to the other hand in the rifle-aim pose, which the
	// reference pose can't tell), and turned into the socket's own frame.
	const FReferenceSkeleton& Ref = BodyAsset->GetRefSkeleton();
	const FName SocketName = Def.HeldSocket.IsNone() ? WeaponSocket : Def.HeldSocket;
	const USkeletalMeshSocket* Socket = BodyAsset->FindSocket(SocketName);
	const int32 Bone = Ref.FindBoneIndex(Socket ? Socket->BoneName : SocketName);
	if (Bone == INDEX_NONE)
	{
		return;
	}
	const FTransform SocketLocal = Socket ? Socket->GetSocketLocalTransform() : FTransform::Identity;
	const FBodyAnimSet* PoseSet = Def.HeldPose.IsNone() ? nullptr : BodyAnims.Find(Def.HeldPose);
	const UAnimSequence* PoseClip = PoseSet && PoseSet->Clips.Num() > 0 ? Cast<UAnimSequence>(PoseSet->Clips[0]) : nullptr;
	const USkeleton* PoseSkeleton = PoseClip ? PoseClip->GetSkeleton() : nullptr;
	const auto BoneCS = [&](int32 MeshBone) -> FTransform
	{
		const int32 First = PoseSkeleton ? PoseSkeleton->GetReferenceSkeleton().FindBoneIndex(Ref.GetBoneName(MeshBone)) : INDEX_NONE;
		if (First == INDEX_NONE)
		{
			return FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, MeshBone);
		}
		const FAnimExtractContext At(PoseClip->GetPlayLength() * 0.5);
		FTransform Out = FTransform::Identity;
		for (int32 B = First; B != INDEX_NONE; B = PoseSkeleton->GetReferenceSkeleton().GetParentIndex(B))
		{
			FTransform Local = PoseSkeleton->GetReferenceSkeleton().GetRefBonePose()[B];
			PoseClip->GetBoneTransform(Local, FSkeletonPoseBoneIndex(B), At, false);
			Out = Out * Local;
		}
		return Out;
	};
	const FTransform SocketCS = SocketLocal * BoneCS(Bone);
	const FVector Hand = SocketCS.GetLocation();
	// The fingers run out of the hand toward the middle finger; the other hand is the same bone on the other side.
	const FString BoneName = Ref.GetBoneName(Bone).ToString();
	const FString Side = BoneName.Right(2);
	const int32 Finger = Ref.FindBoneIndex(FName(*(TEXT("middle_01") + Side)));
	const FVector FingerDir = Finger != INDEX_NONE ? (BoneCS(Finger).GetLocation() - BoneCS(Bone).GetLocation()).GetSafeNormal()
	                                               : SocketCS.GetUnitAxis(EAxis::Y);
	const FString OtherName = BoneName.LeftChop(2) + (Side == TEXT("_l") ? TEXT("_r") : TEXT("_l"));
	const int32 OtherHand = Ref.FindBoneIndex(FName(*OtherName));
	const FVector Forward = FRotator(0.f, BodyYaw, 0.f).UnrotateVector(FVector::ForwardVector);
	const FVector SideDir = FVector(Hand.X, Hand.Y, 0.f).GetSafeNormal();  // the mesh origin is on the body's centre line
	const auto Dir = [&](FName Name) -> FVector
	{
		if (Name == TEXT("fingers")) return FingerDir;
		if (Name == TEXT("grip")) return SocketCS.GetUnitAxis(EAxis::X);       // through the fist, out past the index finger
		if (Name == TEXT("palm")) return SocketCS.GetUnitAxis(EAxis::Z);       // out of the palm
		if (Name == TEXT("backhand")) return -SocketCS.GetUnitAxis(EAxis::Z);  // out of the back of the hand
		if (Name == TEXT("arm"))                                               // down the forearm, elbow to wrist
		{
			const int32 Elbow = Ref.GetParentIndex(Bone);
			return Elbow != INDEX_NONE ? (BoneCS(Bone).GetLocation() - BoneCS(Elbow).GetLocation()).GetSafeNormal() : FingerDir;
		}
		if (Name == TEXT("hands") && OtherHand != INDEX_NONE)
		{
			const FVector ToOther = BoneCS(OtherHand).GetLocation() - Hand;
			return FVector(ToOther.X, ToOther.Y, 0.f).GetSafeNormal();  // level: the other hand holds it from below
		}
		if (Name == TEXT("up")) return FVector::UpVector;
		if (Name == TEXT("out")) return SideDir;
		if (Name == TEXT("back")) return -Forward;
		return Forward;
	};
	const FVector Along = Dir(Def.HeldAlong);
	if (Comp == RangedMesh)
	{
		// A gun held across the body in the aim pose: turn the body that much while aiming so it points ahead.
		const FVector AlongActor = FRotator(0.f, BodyYaw, 0.f).RotateVector(Along);
		RangedAimYaw = PoseClip && Def.HeldAlong == TEXT("hands") ? -FMath::RadiansToDegrees(FMath::Atan2(AlongActor.Y, AlongActor.X)) : 0.f;
	}
	FVector FaceDir = FVector::VectorPlaneProject(Dir(Def.HeldFace.IsNone() ? FName(TEXT("out")) : Def.HeldFace), Along).GetSafeNormal();
	if (FaceDir.IsNearlyZero())
	{
		FaceDir = FVector::VectorPlaneProject(FVector::UpVector, Along).GetSafeNormal();
	}
	const FBox Box = Model->GetBoundingBox();
	const FVector LocalAlong = ParseAxis(Def.MeshAlong, LongAxis(Box));
	FVector LocalFace = FVector::VectorPlaneProject(ParseAxis(Def.MeshFace, ThinAxis(Box)), LocalAlong).GetSafeNormal();
	if (LocalFace.IsNearlyZero())
	{
		LocalFace = FVector::VectorPlaneProject(FVector(0.31f, 0.53f, 0.79f), LocalAlong).GetSafeNormal();
	}
	const FQuat Want = FQuat(FRotationMatrix::MakeFromXY(SocketCS.InverseTransformVectorNoScale(Along), SocketCS.InverseTransformVectorNoScale(FaceDir)));
	const FQuat Have = FQuat(FRotationMatrix::MakeFromXY(LocalAlong, LocalFace));
	const FQuat Rot = FQuat(Def.HeldRot) * Want * Have.Inverse();
	Comp->SetStaticMesh(Model);
	Comp->EmptyOverrideMaterials();
	Comp->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket ? SocketName : Ref.GetBoneName(Bone));
	Comp->SetRelativeRotation(Rot.Rotator());
	Comp->SetRelativeLocation(Def.HeldOffset);  // the socket's origin is already in the palm
	Comp->SetRelativeScale3D(FVector::OneVector);
}

// ---------------------------------------------------------------- aiming

void AWRCharacter::SetAiming(bool bOn)
{
	bOn = bOn && IsRangedOut();
	if (bOn == bAiming)
	{
		return;
	}
	bAiming = bOn;
	ApplyAimRotation();
	if (bOn && RangedAnimGroup() == TEXT("bow"))
	{
		WRSound::PlayOn(this, TEXT("bow.draw"));
	}
	if (!HasAuthority())
	{
		ServerSetAiming(bOn);
	}
}

void AWRCharacter::ServerSetAiming_Implementation(bool bOn)
{
	bAiming = bOn && IsRangedOut();
	ApplyAimRotation();
}

void AWRCharacter::ApplyAimRotation()
{
	if (bAiming)
	{
		// Face the crosshair while aiming, in either view.
		bUseControllerRotationYaw = true;
		GetCharacterMovement()->bOrientRotationToMovement = false;
	}
	else
	{
		bUseControllerRotationYaw = bFirstPerson;
		GetCharacterMovement()->bOrientRotationToMovement = !bFirstPerson;
	}
}

void AWRCharacter::TickAimCamera(float DeltaSeconds)
{
	AimBlend = FMath::FInterpTo(AimBlend, IsAimingRanged() ? 1.f : 0.f, DeltaSeconds, 10.f);
	// Under water the camera comes in close (cooks/biome-ocean.md 2.6: 250 cm), eased, so murk and kelp don't fill it.
	UnderwaterBlend = FMath::FInterpTo(UnderwaterBlend, WRUnderwater::IsUnder(this) ? 1.f : 0.f, DeltaSeconds, 3.f);
	ThirdPersonBoom->TargetArmLength = FMath::Lerp(FMath::Lerp(420.f, WRTune(TEXT("underwater.cameraArm"), 250.f), UnderwaterBlend), WRTune(TEXT("ranged.aimArmLength"), 170.f), AimBlend);
	// The shoulder the camera sits over (Settings > Camera): right, left or centred behind the head; eased, so a change
	// in the menu slides the view across rather than cutting.
	const FWRKeyBindings& Prefs = FWRKeyBindings::Get();
	const int32 SideChoice = Prefs.PrefInt(TEXT("CameraSide"));
	const float SideTarget = SideChoice == 1 ? -1.f : SideChoice == 2 ? 0.f : 1.f;
	CameraSideBlend = FMath::FInterpTo(CameraSideBlend, SideTarget, DeltaSeconds, 6.f);
	ThirdPersonBoom->SocketOffset = FMath::Lerp(FVector(0.f, 75.f * CameraSideBlend, 85.f), FVector(0.f, 60.f * CameraSideBlend, 70.f), AimBlend);
	// Field of view per view (Settings > Camera; the combat camera was tuned at 90): aiming zooms by the same ratio
	// whatever the base, so a wide-FOV player's bow still zooms 60 / 90 of their view.
	const float AimRatio = WRTune(TEXT("ranged.aimFov"), 60.f) / 90.f;
	const float BaseThird = Prefs.Pref(TEXT("FovThird")), BaseFirst = Prefs.Pref(TEXT("FovFirst"));
	float Widen = 0.f;
	// Sprinting widens the view a little (eased in and out; not while aiming).
	if (UWRCameraBoom* Boom = Cast<UWRCameraBoom>(ThirdPersonBoom.Get()))
	{
		const UWRCharacterMovementComponent* Move = Cast<UWRCharacterMovementComponent>(GetCharacterMovement());
		const bool bSprinting = Move && Move->IsSprinting() && GetVelocity().Size2D() > Move->MaxWalkSpeed * 1.1f;
		Widen = Boom->TickSprintWiden(bSprinting, DeltaSeconds) * (1.f - AimBlend);
	}
	ThirdPersonCamera->SetFieldOfView(FMath::Lerp(BaseThird, BaseThird * AimRatio, AimBlend) + Widen);
	FirstPersonCamera->SetFieldOfView(FMath::Lerp(BaseFirst, BaseFirst * AimRatio, AimBlend) + Widen);
	TickSprintToggle(DeltaSeconds);
}

void AWRCharacter::CycleAmmo(int32 Dir)
{
	if (IsRangedOut())
	{
		ServerCycleAmmo(Dir);
	}
}

void AWRCharacter::ServerCycleAmmo_Implementation(int32 Dir)
{
	const FWRItemDef* Def = ItemDef(RangedItem);
	const UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(this);
	if (Def && Inv && !Def->AmmoKind.IsNone())
	{
		SelectedAmmo = Inv->NextAmmo(Def->AmmoKind, SelectedAmmo, Dir >= 0 ? 1 : -1);
		UE_LOG(LogWildreach, Log, TEXT("RANGED %s loads %s"), *GetName(), *SelectedAmmo.ToString());
	}
}

// ---------------------------------------------------------------- firing

void AWRCharacter::GetAimView(FVector& OutLoc, FRotator& OutRot) const
{
	// The survivor's own camera, not the player's view point: a spectator or art camera must not move the crosshair.
	const UCameraComponent* Cam = bFirstPerson ? FirstPersonCamera.Get() : ThirdPersonCamera.Get();
	OutLoc = Cam ? Cam->GetComponentLocation() : GetPawnViewLocation();
	OutRot = GetControlRotation();
}

FRotator AWRCharacter::AimRotationAt(const FVector& Target) const
{
	// The shoulder camera swings with the rotation and sits off to the side, so a few rounds settle a rotation whose
	// crosshair ray passes through the target (same placement as the spring arm, without its collision pull-in).
	FVector Loc;
	FRotator Rot;
	GetAimView(Loc, Rot);
	for (int32 Round = 0; Round < 4; ++Round)
	{
		if (!bFirstPerson)
		{
			Loc = ThirdPersonBoom->GetComponentLocation() + ThirdPersonBoom->TargetOffset - Rot.Vector() * ThirdPersonBoom->TargetArmLength
				+ FRotationMatrix(Rot).TransformVector(ThirdPersonBoom->SocketOffset);
		}
		Rot = (Target - Loc).Rotation();
	}
	return Rot;
}

bool AWRCharacter::CrosshairPoint(float Range, bool bPawns, FVector& OutPoint) const
{
	const FRotator Rot = GetControlRotation();
	const FVector Dir = Rot.Vector();
	FVector Cam;
	if (bFirstPerson || !ThirdPersonBoom)
	{
		Cam = FirstPersonCamera ? FirstPersonCamera->GetComponentLocation() : GetPawnViewLocation();
	}
	else
	{
		// As AimRotationAt places it: the server has no view of its own to ask, and pull-in only moves it along the ray.
		Cam = ThirdPersonBoom->GetComponentLocation() + ThirdPersonBoom->TargetOffset - Dir * ThirdPersonBoom->TargetArmLength
			+ FRotationMatrix(Rot).TransformVector(ThirdPersonBoom->SocketOffset);
	}
	const float Along = FMath::Max(60.f, static_cast<float>(FVector::DotProduct(GetActorLocation() - Cam, Dir)));
	const FVector Start = Cam + Dir * Along;
	const FVector End = Start + Dir * Range;
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(WRCrosshairPoint), false, this);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	if (bPawns)
	{
		Objects.AddObjectTypesToQuery(ECC_Pawn);
		Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
	}
	const bool bHit = GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, Objects, Query);
	OutPoint = bHit ? FVector(Hit.ImpactPoint) : End;
	return bHit;
}

void AWRCharacter::FireRanged()
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (!IsRangedOut() || !IsAlive() || Now < LocalNextShotAt)
	{
		return;
	}
	// Aim at whatever is under the crosshair; the server launches from the weapon toward that point.
	FVector ViewLoc;
	FRotator ViewRot;
	GetAimView(ViewLoc, ViewRot);
	const FVector ViewDir = ViewRot.Vector();
	const FVector End = ViewLoc + ViewDir * 20000.f;
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(WRRangedAim), false, this);
	// Creatures count too (their capsules don't block the visibility channel): a crosshair on a low creature must not
	// aim at the ground behind it.
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
	const FVector Point = GetWorld()->LineTraceSingleByObjectType(Hit, ViewLoc + ViewDir * 60.f, End, Objects, Query) ? Hit.ImpactPoint : End;
	LocalNextShotAt = Now + 0.15;
	ServerFireRanged(Point, ViewDir);
}

void AWRCharacter::ServerFireRanged_Implementation(FVector_NetQuantize AimPoint, FVector_NetQuantizeNormal ViewDir)
{
	const double Now = GetWorld()->GetTimeSeconds();
	UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(this);
	const FWRItemDef* Gun = ItemDef(RangedItem);
	if (!Inv || !Gun || !bRangedMode || !IsAlive() || Now < NextShotAt
		|| HasTag(WRTags::Status_Staggered) || HasTag(WRTags::Status_Frozen) || HasTag(WRTags::Status_Unconscious))
	{
		return;
	}
	const bool bThrow = Gun->IsThrowable();
	// Under water (cooks/biome-ocean.md 2.6): a bow won't draw; a crossbow's bolt carries half as far; thrown spears and
	// harpoons fly as ever.
	if (WRUnderwater::IsUnder(this) && Gun->RangedKind == TEXT("bow"))
	{
		if (AWRPlayerController* PC = Cast<AWRPlayerController>(GetController())) PC->ClientNotify(TEXT("Your bow won't draw under water"));
		NextShotAt = Now + 0.6;
		return;
	}
	FName Ammo;
	if (!bThrow)
	{
		Ammo = Inv->FindAmmo(Gun->AmmoKind, SelectedAmmo);
		if (Ammo.IsNone())
		{
			UE_LOG(LogWildreach, Log, TEXT("RANGED %s has no %s ammo for %s"), *GetName(), *Gun->AmmoKind.ToString(), *Gun->Id.ToString());
			return;
		}
		SelectedAmmo = Ammo;
		Inv->RemoveItem(Ammo, 1);
	}
	// Toward the crosshair's point; a point behind or right on top of us falls back to the view direction. The view
	// direction comes with the shot: the server's copy of the control rotation can be a frame behind a quick flick.
	const FVector Muzzle = GetActorLocation() + FVector(0.f, 0.f, 55.f) + GetActorForwardVector() * 35.f;
	const FVector View = FVector(ViewDir).IsNearlyZero() ? GetControlRotation().Vector() : FVector(ViewDir).GetSafeNormal();
	FVector Dir = (FVector(AimPoint) - Muzzle).GetSafeNormal();
	if (Dir.IsNearlyZero() || FVector::DistSquared(AimPoint, Muzzle) < FMath::Square(100.f) || FVector::DotProduct(Dir, View) < 0.3f)
	{
		Dir = View;
	}
	if (!bAiming)
	{
		Dir = FMath::VRandCone(Dir, FMath::DegreesToRadians(WRTune(TEXT("ranged.hipSpreadDegrees"), 4.f)));  // hip fire wanders
	}
	// Swift draw (the Wind path): the draw and the throw's wind-up go faster.
	const float Draw = WRWind::DrawTimeScale(this);
	const float Delay = bThrow ? WRTune(TEXT("ranged.throwDelay"), 0.45f) * Draw : 0.f;
	NextShotAt = Now + Gun->Reload * Draw + Delay;
	const FName Group = RangedAnimGroup();
	MulticastRangedShot(Group == TEXT("throw") ? FName(TEXT("throw")) : FName(*(Group.ToString() + TEXT("_fire"))),
		Group == TEXT("throw") ? NAME_None : FName(*(Group.ToString() + TEXT("_reload"))), Gun->Reload);
	UE_LOG(LogWildreach, Log, TEXT("RANGED %s fires %s (%s)%s"), *GetName(), *Gun->Id.ToString(), bThrow ? TEXT("thrown") : *Ammo.ToString(), bAiming ? TEXT(" aimed") : TEXT(""));
	WRSound::Broadcast(this, bThrow ? FName(TEXT("throw")) : Gun->RangedKind == TEXT("bow") ? FName(TEXT("bow.release"))
		: Gun->RangedKind == TEXT("blowgun") ? FName(TEXT("blowgun.fire")) : FName(TEXT("crossbow.fire")));
	if (bThrow)
	{
		const FName Thrown = Gun->Id;
		Inv->SpendThrowable();  // may unequip the last one
		GetWorldTimerManager().SetTimer(ThrowTimer, FTimerDelegate::CreateWeakLambda(this, [this, Thrown, Dir]()
		{
			if (const FWRItemDef* Def = ItemDef(Thrown))
			{
				LaunchProjectile(*Def, NAME_None, Dir);
			}
		}), Delay, false);
	}
	else
	{
		// A crafted launcher's quality scales its shots; each shot wears it a little.
		LaunchProjectile(*Gun, Ammo, Dir, Inv->GetRangedMultiplier());
		Inv->WearEquipped(EWREquipSlot::Ranged, WRTune(TEXT("ranged.wearPerShot"), 1.f));
		// Tailwind shot (the Wind path): an aimed (fully drawn) shot pushes you back unless Sprint is held.
		WRWind::OnShotFired(this, nullptr, bAiming);
	}
}

void AWRCharacter::LaunchProjectile(const FWRItemDef& Launcher, FName AmmoId, const FVector& Dir, float LauncherMult)
{
	if (!IsAlive())
	{
		return;
	}
	const FWRItemDef* Ammo = ItemDef(AmmoId);
	FWRHit Hit;
	Hit.Instigator = this;
	Hit.Damage = (Launcher.RangedDamage * LauncherMult + (Ammo ? Ammo->AmmoDamage : 0.f)) * WRWind::ProjectileDamageScale(this);   // Crosswind
	Hit.PoiseDamage = Hit.Damage * WRTune(TEXT("ranged.poisePerDamage"), 0.4f);
	Hit.Element = WRTags::Element_Physical;
	// A drawn and aimed shot lands with a heavy blow's weight (hit-stop, flash, shake: cookpass-combat 3.6); a hip shot light.
	if (bAiming) Hit.Tier = EWRHitTier::Heavy;
	Hit.Torpor = (Ammo ? Ammo->AmmoTorpor : 0.f) * Launcher.TorporMult * LauncherMult;
	// An arrow's own status (items.json ammo: the Murkjaw-tooth arrow's bleed 25): WRSwampSystems reads it.
	if (Ammo) WRSwamp::AddAmmoBuildups(Ammo->Id, Hit);
	if (Ammo && Ammo->Id == TEXT("shark_tooth_arrow")) Hit.SwimTargetMult = WRTune(TEXT("oceanWorld.sharkToothSwimmingMult"), 1.1f);
	// Hush arrows: silent, sneak hits x1.15 (cooks/biome-sky.md 2.17).
	if (Ammo) { Hit.UnawareMult = Ammo->AmmoSneakMult; Hit.bQuiet = Ammo->bAmmoSilent; }
	// Arrows, bolts, darts and thrown spears fly straight enough to be thrown back by a reflector (crystal 2.12); bolas
	// and pots are lobs.
	{
		const FString K = Launcher.RangedKind.ToString();
		Hit.bReflectable = K.Contains(TEXT("arrow")) || K.Contains(TEXT("bolt")) || K.Contains(TEXT("dart")) || K.Contains(TEXT("javelin")) || K.Contains(TEXT("spear"));
	}
	const FVector Chest = GetActorLocation() + FVector(0.f, 0.f, 55.f);
	const FVector Muzzle = Chest + Dir * 45.f;

	// Point blank: a target between us and the muzzle would start inside the projectile and never get hit.
	FHitResult Near;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(WRRangedMuzzle), false, this);
	if (GetWorld()->SweepSingleByChannel(Near, Chest, Muzzle, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(8.f), Query))
	{
		AWRCharacterBase* Blocker = Cast<AWRCharacterBase>(Near.GetActor());
		if (Blocker && IsHostileTo(Blocker))
		{
			Hit.Origin = Chest;
			UWRCombatLibrary::ApplyHit(Blocker, Hit);
			if (Launcher.RangedKind == TEXT("bola"))
			{
				AWRProjectile::ApplyBola(Blocker);
			}
			return;
		}
	}

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AWRProjectile* Projectile = GetWorld()->SpawnActor<AWRProjectile>(AWRProjectile::StaticClass(), Muzzle, Dir.Rotation(), Params))
	{
		// (The fire pot is modelled in code: the projectile builds it from "code:firepot".)
		// (So are the Swamp's salt pouch and stormglass jar: "code:saltpouch", "code:stormjar", WRSwampSystems.)
		// (And the Wasteland's throwing stone and anchor salt bag: "code:riftstone", "code:anchorsalt", WRVoidWorld.)
		// (And the Ancient forest's Bootlace blight jar and smoke pot: "code:blightjar", "code:smokepot", WRForestHazards.)
		const bool bSwampThrown = Launcher.RangedKind == TEXT("saltpouch") || Launcher.RangedKind == TEXT("stormjar")
			|| Launcher.RangedKind == TEXT("blightjar") || Launcher.RangedKind == TEXT("smokepot") || Launcher.RangedKind == TEXT("limepot") || Launcher.RangedKind == TEXT("sporepot") || Launcher.RangedKind == TEXT("galeflask") || WRVoidWorld::IsThrownKind(Launcher.RangedKind) || WRDesertHz::IsThrownKind(Launcher.RangedKind);
		const FString Model = Launcher.RangedKind == TEXT("firepot") ? FString(TEXT("code:firepot"))
			: bSwampThrown ? FString(TEXT("code:")) + Launcher.RangedKind.ToString()
			: Ammo && !Ammo->ProjectileMesh.IsEmpty() ? Ammo->ProjectileMesh : Launcher.HeldMesh;
		const FString MeshAlong = Ammo && !Ammo->ProjectileMesh.IsEmpty() ? Ammo->MeshAlong : Launcher.MeshAlong;
		const float WaterMult = WRUnderwater::IsUnder(this) && Launcher.RangedKind == TEXT("crossbow") ? WRTune(TEXT("underwater.crossbowRange"), 0.5f) : 1.f;
		Projectile->InitPhysical(Hit, Dir * Launcher.ProjectileSpeed * WaterMult * WRWind::ProjectileSpeedScale(this) * (Ammo ? Ammo->AmmoSpeedMult : 1.f), Launcher.ProjectileGravity, Model, MeshAlong, Launcher.RangedKind,
			Launcher.IsThrowable() && Launcher.RangedKind == TEXT("spear") ? Launcher.Id : NAME_None);
		// Through bodies: the ammo's own pierce (items.json) and Piercing gale's one (the Wind path).
		Projectile->PierceLeft = WRWind::ShotPierce(this, AmmoId);
		// Gale arrows and Galespine throw what they strike back along the shot (cooks/biome-sky.md 2.17, 2.11): the
		// knockback hit property through the Wind path's one knock (WRSkyAir::Push: brace, anchor boots, edge credit).
		const float Knock = FMath::Max(Launcher.Knockback, Ammo ? Ammo->Knockback : 0.f);
		if (Knock > 0.f)
		{
			TWeakObjectPtr<AWRCharacter> WeakSelf(this);
			const FVector KnockDir = Dir.GetSafeNormal2D();
			Projectile->OnStruck = [WeakSelf, KnockDir, Knock](AWRCharacterBase* Struck, EWRHitResult Result)
			{
				if (!Struck || !Struck->IsAlive() || (Result != EWRHitResult::Hit && Result != EWRHitResult::GuardBroken)) return;
				WRWind::Knock(WeakSelf.Get(), Struck, KnockDir, Knock, WRTune(TEXT("ranged.knockSeconds"), 0.35f), TEXT("arrow"));
			};
		}
		// A thrown spear or harpoon (cooks/biome-ocean.md 2.11, 2.19): Hae-won's third quest counts it under the water; the
		// Whaler's harpoon puts its 18 m line on what it strikes (hauling the small in; E at a mooring stone ties it off).
		if (Launcher.RangedKind == TEXT("spear"))
		{
			TWeakObjectPtr<AWRCharacter> WeakSelf(this);
			const bool bLine = Launcher.Id == TEXT("whalers_harpoon");
			TFunction<void(AWRCharacterBase*, EWRHitResult)> Before = Projectile->OnStruck;
			Projectile->OnStruck = [WeakSelf, bLine, Before](AWRCharacterBase* Struck, EWRHitResult Result)
			{
				if (Before) Before(Struck, Result);
				AWRCharacter* Me = WeakSelf.Get();
				if (!Me || !Struck || (Result != EWRHitResult::Hit && Result != EWRHitResult::GuardBroken)) return;
				if (Struck->IsA<AWRCreature>() && (WRUnderwater::IsUnder(Struck) || WRUnderwater::IsUnder(Me)))
				{
					AWRPlayerState* PS = Me->GetPlayerState<AWRPlayerState>();
					if (UWRWorldSystems* WS = PS ? Me->GetWorld()->GetSubsystem<UWRWorldSystems>() : nullptr) WS->QuestEvent(PS, TEXT("event"), TEXT("harpoon_underwater"), 1);
				}
				if (!bLine) return;
				AWRHarpoonLine* L = AWRHarpoonLine::Make(Me, Struck, Struck->GetActorLocation(), false);
				if (!L) return;
				// It waits on the line (to be tied off) as long as it lasts; the lowland and midland are hauled in.
				L->ReelUntil = L->BornAt + L->Lifetime;
				const AWRCreature* C = Cast<AWRCreature>(Struck);
				const FName Tier = C && C->GetDef() ? C->GetDef()->Tier : NAME_None;
				if (Tier == TEXT("lowland") || Tier == TEXT("midland"))
				{
					const FVector Toward = (Me->GetActorLocation() - Struck->GetActorLocation()).GetSafeNormal();
					Struck->LaunchCharacter(Toward * WRTune(TEXT("oceanWorld.whalerReel"), 1600.f) + FVector(0, 0, 150.f), true, true);
					L->bTaut = true;
				}
				WRSound::PlayAt(Me, TEXT("tide.harpoon_hit"), Struck->GetActorLocation());
			};
		}
		// The sunstone flare: a 2 m burst of daylight where it comes down or strikes (blind 25 to all but the thrower).
		if (Launcher.RangedKind == TEXT("sunflare"))
		{
			TWeakObjectPtr<AWRCharacter> WeakSelf(this);
			auto Burst = [WeakSelf](const FVector& At)
			{
				AWRCharacter* Me = WeakSelf.Get();
				if (!Me) return;
				WRFx::Broadcast(Me, TEXT("ocean.sunflare"), At, FRotator::ZeroRotator, 1.f, TEXT("ocean.sunflare"));
				// On the Tide Leviathan: a second of the sun's beam on the part it burst at.
				FWROceanBoss::SunBurst(Me->GetWorld(), At, WRTune(TEXT("oceanWorld.sunflareBeam"), 1.f));
				for (TActorIterator<AWRCharacterBase> It(Me->GetWorld()); It; ++It)
				{
					if (*It == Me || !It->IsAlive() || FVector::Dist(It->GetActorLocation(), At) > WRTune(TEXT("oceanWorld.sunflareRadius"), 200.f) + It->GetSimpleCollisionRadius()) continue;
					FWRHit Flash;
					Flash.Instigator = Me;
					Flash.Damage = 0.f;
					Flash.bUnblockable = true;
					Flash.Origin = At;
					Flash.Element = WRTags::Element_Physical;
					Flash.Buildups.Add(TEXT("blind"), WRTune(TEXT("oceanWorld.sunflareBlind"), 25.f));
					UWRCombatLibrary::ApplyHit(*It, Flash);
				}
			};
			Projectile->OnLanded = Burst;
			TFunction<void(AWRCharacterBase*, EWRHitResult)> Before = Projectile->OnStruck;
			Projectile->OnStruck = [Burst, Before](AWRCharacterBase* Struck, EWRHitResult Result) { if (Before) Before(Struck, Result); if (Struck) Burst(Struck->GetActorLocation()); };
		}
	}
}

void AWRCharacter::MulticastRangedShot_Implementation(FName FireKey, FName ReloadKey, float Reload)
{
	if (GetNetMode() == NM_DedicatedServer || !Body->IsVisible())
	{
		return;
	}
	BodyAnim = NAME_None;
	const float Length = PlayBodyAnim(FireKey, false);
	const double Now = GetWorld()->GetTimeSeconds();
	BodyOneShotUntil = Now + Length;
	// Then nock / reload for the rest of the weapon's reload.
	QueuedOneShot = ReloadKey;
	QueuedOneShotSeconds = FMath::Max(0.3f, Reload - Length);
}
