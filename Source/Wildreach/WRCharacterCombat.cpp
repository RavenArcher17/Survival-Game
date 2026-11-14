// The hero in a fight beyond the core (cooks/build-combat-full.md 2.6, 2.19): two-handing, breaking out of a freeze,
// framing giants and the camera at walls (CAL 1.15).
#include "WRCharacter.h"
#include "Wildreach.h"
#include "WRCameraBoom.h"
#include "WRCreature.h"
#include "WRGameplayTags.h"
#include "WRPlayerController.h"
#include "WRStatusComponent.h"
#include "WRTuning.h"
#include "WRWeapons.h"
#include "WRSound.h"
#include "AbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"

void AWRCharacter::ServerSetTwoHanded_Implementation(bool bOn)
{
	// Only a one-handed weapon has a choice (2h and great weapons are always held in both hands); the guard drops while
	// the grip changes.
	const FWRWeaponClass& Class = FWRWeapons::Get().ClassFor(this);
	if (Class.Hands != TEXT("1h") || bTwoHanded == bOn || HasTag(WRTags::State_Attacking) || HasTag(WRTags::State_Mounted))
	{
		return;
	}
	bTwoHanded = bOn;
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		FGameplayTagContainer Guard;
		Guard.AddTag(WRTags::Ability_Block);
		ASC->CancelAbilities(&Guard);
	}
	WRSound::Broadcast(this, TEXT("equip"));
	OnRep_TwoHanded();
	UE_LOG(LogWildreach, Log, TEXT("COMBAT %s %s"), *GetName(), bOn ? TEXT("two-hands the weapon") : TEXT("takes the shield back up"));
}

void AWRCharacter::OnRep_TwoHanded()
{
	UpdateWeaponLook();
}

void AWRCharacter::ServerMashFrozen_Implementation()
{
	Status->MashFrozen();
}

void AWRCharacter::TickCombatCamera(float DeltaSeconds)
{
	// Giant framing (CAL 1.15): locked onto something more than giantRatio times your height, the arm pulls back and the
	// view tips up, so a boss fills the frame without its feet being all you see. Eased over giantSeconds.
	const AWRPlayerController* PC = Cast<AWRPlayerController>(GetController());
	const AWRCharacterBase* Lock = PC ? PC->GetLockTarget() : nullptr;
	const FWRTuning& T = FWRTuning::Get();
	const bool bGiant = Lock && !bFirstPerson && Lock->GetSimpleCollisionHalfHeight() > GetSimpleCollisionHalfHeight() * T.Num(TEXT("camera.giantRatio"), 3.f);
	const float Rate = 1.f / FMath::Max(0.05f, T.Num(TEXT("camera.giantSeconds"), 0.4f));
	GiantBlend = FMath::FInterpConstantTo(GiantBlend, bGiant ? 1.f : 0.f, DeltaSeconds, Rate);
	if (GiantBlend > 0.f)
	{
		ThirdPersonBoom->TargetArmLength += T.Num(TEXT("camera.giantBoom"), 150.f) * GiantBlend;
		ThirdPersonBoom->SocketOffset.Z += T.Num(TEXT("camera.giantBoom"), 150.f) * 0.25f * GiantBlend;
		// (The tip-up of the view is the lock-on aim's: AWRPlayerController::UpdateLockOn.)
	}
	// The camera right up against the body (a wall behind you): the hero goes from view rather than filling it.
	if (!bFirstPerson && Body)
	{
		const float Gap = FVector::Dist(ThirdPersonCamera->GetComponentLocation(), GetActorLocation() + FVector(0.f, 0.f, GetSimpleCollisionHalfHeight() * 0.6f));
		const bool bHide = Gap < T.Num(TEXT("camera.hideUnder"), 100.f);
		if (bHide != bHiddenByCamera)
		{
			bHiddenByCamera = bHide;
			Body->SetOwnerNoSee(bHide);
			for (USceneComponent* Child : Body->GetAttachChildren())
			{
				if (UPrimitiveComponent* P = Cast<UPrimitiveComponent>(Child)) P->SetOwnerNoSee(bHide);
			}
		}
	}
}
