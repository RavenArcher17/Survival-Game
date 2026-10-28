// Ranged weapons on the survivor: bow, crossbow, blowgun, throwing spears and bolas (Data/items.json "ranged").
// G swaps between sword-and-shield and the ranged weapon; with it out, right mouse aims (over-the-shoulder zoom,
// slower, facing the crosshair) and left mouse fires along the crosshair. The server spends the ammo, keeps the
// weapon's reload, and launches a real projectile (WRProjectile) with drop.
#include "WRCharacter.h"
#include "WRSound.h"
#include "Wildreach.h"
#include "WRCameraBoom.h"
#include "WRCharacterMovementComponent.h"
#include "WRCombatLibrary.h"
#include "WRGameplayTags.h"
#include "WRInventory.h"
#include "WRProjectile.h"
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
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
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
	const bool bTool = !bRanged && Body->IsVisible() && FWRGathering::FindTool(MeleeItem) != nullptr;
	WeaponMesh->SetVisibility(!bRanged && !bTool && !MeleeItem.IsNone());
	UpdateToolLook(bTool);
	ShieldMesh->SetVisibility(!bRanged && bShieldShown);
	RangedMesh->SetVisibility(Model != nullptr && Body->IsVisible());
	if (Model && Body->IsVisible())
	{
		AttachHeld(RangedMesh, Model, *Def);
	}
}

void AWRCharacter::AttachHeld(UStaticMeshComponent* Comp, UStaticMesh* Model, const FWRItemDef& Def)
{
	const USkeletalMesh* BodyAsset = Body->GetSkeletalMeshAsset();
	if (!BodyAsset)
	{
		return;
	}
	// Directions come from the skeleton's reference pose (not always a T-pose: arms may hang), in mesh space, or from
	// a clip's pose ("pose") for gear gripped the same way in every clip: a crossbow runs from the trigger hand to the
	// other hand in the rifle-aim pose, which the reference pose can't tell.
	const FReferenceSkeleton& Ref = BodyAsset->GetRefSkeleton();
	const FName Socket = Def.HeldSocket.IsNone() ? FName(TEXT("RightHand")) : Def.HeldSocket;
	const int32 Bone = Ref.FindBoneIndex(Socket);
	if (Bone == INDEX_NONE)
	{
		return;
	}
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
			FTransform Local;
			PoseClip->GetBoneTransform(Local, FSkeletonPoseBoneIndex(B), At, false);
			Out = Out * Local;
		}
		return Out;
	};
	const FTransform HandCS = BoneCS(Bone);
	const FVector Hand = HandCS.GetLocation();
	const int32 Parent = Ref.GetParentIndex(Bone);
	const FVector Elbow = Parent != INDEX_NONE ? BoneCS(Parent).GetLocation() : Hand + FVector::UpVector;
	const int32 Finger = Ref.FindBoneIndex(FName(*(Socket.ToString() + TEXT("Index1"))));
	const FVector FingerDir = Finger != INDEX_NONE ? (BoneCS(Finger).GetLocation() - Hand).GetSafeNormal()
	                                               : (Hand - Elbow).GetSafeNormal();  // no finger bones: the hand continues the forearm
	const FString OtherName = Socket.ToString().StartsWith(TEXT("Left")) ? Socket.ToString().Replace(TEXT("Left"), TEXT("Right"))
	                                                                     : Socket.ToString().Replace(TEXT("Right"), TEXT("Left"));
	const int32 OtherHand = Ref.FindBoneIndex(FName(*OtherName));
	const FVector Forward = FRotator(0.f, BodyYaw, 0.f).UnrotateVector(FVector::ForwardVector);
	const FVector Side = FVector(Hand.X, Hand.Y, 0.f).GetSafeNormal();  // the mesh origin is on the body's centre line
	const auto Dir = [&](FName Name) -> FVector
	{
		if (Name == TEXT("fingers")) return FingerDir;
		if (Name == TEXT("hands") && OtherHand != INDEX_NONE)
		{
			const FVector ToOther = BoneCS(OtherHand).GetLocation() - Hand;
			return FVector(ToOther.X, ToOther.Y, 0.f).GetSafeNormal();  // level: the other hand holds it from below
		}
		if (Name == TEXT("up")) return FVector::UpVector;
		if (Name == TEXT("out")) return Side;
		if (Name == TEXT("back")) return -Forward;
		return Forward;
	};
	const FVector Along = Dir(Def.HeldAlong);
	if (Comp == RangedMesh)
	{
		// Mixamo's rifle aim holds the gun a little left of where the body faces: turn the body that much while aiming.
		const FVector AlongActor = FRotator(0.f, BodyYaw, 0.f).RotateVector(Along);
		RangedAimYaw = PoseClip && Def.HeldAlong == TEXT("hands") ? -FMath::RadiansToDegrees(FMath::Atan2(AlongActor.Y, AlongActor.X)) : 0.f;
	}
	FVector Face = FVector::VectorPlaneProject(Dir(Def.HeldFace.IsNone() ? FName(TEXT("out")) : Def.HeldFace), Along).GetSafeNormal();
	if (Face.IsNearlyZero())
	{
		Face = FVector::VectorPlaneProject(FVector::UpVector, Along).GetSafeNormal();
	}
	const FBox Box = Model->GetBoundingBox();
	const FVector LocalAlong = ParseAxis(Def.MeshAlong, LongAxis(Box));
	FVector LocalFace = FVector::VectorPlaneProject(ParseAxis(Def.MeshFace, ThinAxis(Box)), LocalAlong).GetSafeNormal();
	if (LocalFace.IsNearlyZero())
	{
		LocalFace = FVector::VectorPlaneProject(FVector(0.31f, 0.53f, 0.79f), LocalAlong).GetSafeNormal();
	}
	const FQuat Want = FQuat(FRotationMatrix::MakeFromXY(HandCS.InverseTransformVectorNoScale(Along), HandCS.InverseTransformVectorNoScale(Face)));
	const FQuat Have = FQuat(FRotationMatrix::MakeFromXY(LocalAlong, LocalFace));
	const FQuat Rot = FQuat(Def.HeldRot) * Want * Have.Inverse();
	const FVector Grip = HandCS.InverseTransformPositionNoScale(Hand + FingerDir * WRTune(TEXT("ranged.gripFromWrist"), 5.f)) + Def.HeldOffset;
	Comp->SetStaticMesh(Model);
	Comp->EmptyOverrideMaterials();
	Comp->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
	Comp->SetRelativeRotation(Rot.Rotator());
	Comp->SetRelativeLocation(Grip);
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
	ThirdPersonBoom->TargetArmLength = FMath::Lerp(420.f, WRTune(TEXT("ranged.aimArmLength"), 170.f), AimBlend);
	ThirdPersonBoom->SocketOffset = FMath::Lerp(FVector(0.f, 75.f, 85.f), FVector(0.f, 60.f, 70.f), AimBlend);
	float Fov = FMath::Lerp(90.f, WRTune(TEXT("ranged.aimFov"), 60.f), AimBlend);
	// Sprinting widens the view a little (eased in and out; not while aiming).
	if (UWRCameraBoom* Boom = Cast<UWRCameraBoom>(ThirdPersonBoom.Get()))
	{
		const UWRCharacterMovementComponent* Move = Cast<UWRCharacterMovementComponent>(GetCharacterMovement());
		const bool bSprinting = Move && Move->IsSprinting() && GetVelocity().Size2D() > Move->MaxWalkSpeed * 1.1f;
		Fov += Boom->TickSprintWiden(bSprinting, DeltaSeconds) * (1.f - AimBlend);
	}
	ThirdPersonCamera->SetFieldOfView(Fov);
	FirstPersonCamera->SetFieldOfView(Fov);
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
	const float Delay = bThrow ? WRTune(TEXT("ranged.throwDelay"), 0.45f) : 0.f;
	NextShotAt = Now + Gun->Reload + Delay;
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
	Hit.Damage = Launcher.RangedDamage * LauncherMult + (Ammo ? Ammo->AmmoDamage : 0.f);
	Hit.PoiseDamage = Hit.Damage * WRTune(TEXT("ranged.poisePerDamage"), 0.4f);
	Hit.Element = WRTags::Element_Physical;
	Hit.Torpor = (Ammo ? Ammo->AmmoTorpor : 0.f) * Launcher.TorporMult * LauncherMult;
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
		const FString Model = Ammo && !Ammo->ProjectileMesh.IsEmpty() ? Ammo->ProjectileMesh : Launcher.HeldMesh;
		const FString MeshAlong = Ammo && !Ammo->ProjectileMesh.IsEmpty() ? Ammo->MeshAlong : Launcher.MeshAlong;
		Projectile->InitPhysical(Hit, Dir * Launcher.ProjectileSpeed, Launcher.ProjectileGravity, Model, MeshAlong, Launcher.RangedKind,
			Launcher.IsThrowable() && Launcher.RangedKind == TEXT("spear") ? Launcher.Id : NAME_None);
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
