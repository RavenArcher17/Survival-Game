// The survival state on the hero's body (WRSurvival, cooks/build-survival.md 2.2 and 2.4):
//  - the lit torch in the off hand (the shield comes off that arm) or the horn lantern hung at the left hip, each a
//    modelled mesh (WRToolMesh) with its own warm point light, radius = the light's gameplay reach (tuning lights.*),
//    so what you see lit is what the creatures treat as lit. Both stay upright as the body moves: a torch is carried
//    flame-up, a lantern hangs plumb from its bail. The flame, sparks and flicker are the ambient fire effects
//    (WRFxAmbient tracks the light, as it does for placed torches).
//  - asleep: the body lies on the mattress (player_art "sleep" clip, else the end of the death clip held), and the
//    owner's move / jump gets them up.
#include "WRCharacter.h"
#include "WRSurvival.h"
#include "WRToolMesh.h"
#include "WRTuning.h"
#include "WRHeroAnimInstance.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ProceduralMeshComponent.h"

namespace WRHeldLightDetail
{
	FName KindOf(FName Item)
	{
		const FString K = FWRTuning::Get().Str(FString::Printf(TEXT("lights.%s.kind"), *Item.ToString()), FString());
		return K.IsEmpty() ? NAME_None : FName(*K);
	}

	/** The socket if the body has it, else the bone, else none. */
	FName Usable(const USkeletalMeshComponent* Body, FName Socket, FName Bone)
	{
		const USkeletalMesh* Asset = Body ? Body->GetSkeletalMeshAsset() : nullptr;
		if (!Asset) return NAME_None;
		if (!Socket.IsNone() && (Asset->FindSocket(Socket) || Asset->GetRefSkeleton().FindBoneIndex(Socket) != INDEX_NONE)) return Socket;
		return Asset->GetRefSkeleton().FindBoneIndex(Bone) != INDEX_NONE ? Bone : NAME_None;
	}
}

void AWRCharacter::UpdateHeldLightLook()
{
	using namespace WRHeldLightDetail;
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const UWRSurvivalComponent* Survival = UWRSurvivalComponent::For(this);
	const FName Item = Survival && Survival->IsLightOn() && IsAlive() ? Survival->GetLightItem() : NAME_None;
	const FName Kind = Item.IsNone() ? NAME_None : KindOf(Item);
	const bool bTorch = Kind == TEXT("torch");
	const FName Attach = Kind.IsNone() ? NAME_None : bTorch ? Usable(Body, TorchSocket, TEXT("hand_l")) : Usable(Body, HipSocket, TEXT("thigh_l"));
	if (Attach.IsNone() || !Body->IsVisible())
	{
		if (LightMesh) LightMesh->SetVisibility(false);
		if (HeldLight) HeldLight->SetVisibility(false);
		return;
	}
	if (!LightMesh)
	{
		LightMesh = NewObject<UProceduralMeshComponent>(this, TEXT("HeldLightMesh"));
		LightMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		LightMesh->SetGenerateOverlapEvents(false);
		LightMesh->SetupAttachment(Body, Attach);
		LightMesh->RegisterComponent();
	}
	if (!HeldLight)
	{
		HeldLight = NewObject<UPointLightComponent>(this, TEXT("HeldLight"));
		HeldLight->SetupAttachment(LightMesh);
		HeldLight->SetCastShadows(true);
		HeldLight->RegisterComponent();
	}
	if (LightMeshFor != Item)
	{
		(bTorch ? FWRToolMesh::Torch() : FWRToolMesh::Lantern()).ApplyTo(LightMesh);
		LightMeshFor = Item;
		// The light: where the flame is, as far as the light reaches in play, the colour of its fire.
		const FWRTuning& T = FWRTuning::Get();
		const FString Key = FString::Printf(TEXT("lights.%s."), *Item.ToString());
		HeldLight->SetRelativeLocation(bTorch ? FWRToolMesh::TorchTip() + FVector(6.f, 0.f, 0.f) : FWRToolMesh::LanternFlame() + FVector(0.f, 0.f, 1.5f));
		HeldLight->SetAttenuationRadius(T.Num(Key + TEXT("radius"), bTorch ? 600.f : 900.f));
		HeldLight->SetIntensity(T.Num(Key + TEXT("intensity"), bTorch ? 5200.f : 3600.f));
		HeldLight->SetLightColor(bTorch ? FLinearColor(1.f, 0.52f, 0.22f) : FLinearColor(1.f, 0.68f, 0.38f));
		HeldLight->SetSourceRadius(bTorch ? 6.f : 3.f);
	}
	if (LightMesh->GetAttachSocketName() != Attach)
	{
		LightMesh->AttachToComponent(Body, FAttachmentTransformRules::KeepRelativeTransform, Attach);
	}
	LightMesh->SetOwnerNoSee(bFirstPerson && !bTorch);   // the lantern at the hip would sit in the first-person view
	LightMesh->SetVisibility(true);
	HeldLight->SetVisibility(true);
}

void AWRCharacter::TickSurvivalLook(float DeltaSeconds)
{
	using namespace WRHeldLightDetail;
	UWRSurvivalComponent* Survival = UWRSurvivalComponent::For(this);
	const bool bAsleep = Survival && Survival->IsSleeping() && IsAlive();

	if (GetNetMode() != NM_DedicatedServer)
	{
		// A body that came after the light did (a respawn, joining late) catches up here.
		const bool bWantLight = Survival && Survival->IsLightOn() && IsAlive() && Body->IsVisible();
		if (bWantLight != (LightMesh && LightMesh->IsVisible()) && GetWorld()->GetTimeSeconds() - LightLookCheckedAt > 0.5)
		{
			LightLookCheckedAt = GetWorld()->GetTimeSeconds();
			UpdateWeaponLook();
		}
		// Keep the light upright whatever the hand or hip does: the torch flame-up, tipped a little forward, the
		// lantern hanging plumb below its bail (a hand's width out from the thigh, so it clears the leg).
		if (LightMesh && LightMesh->IsVisible())
		{
			const bool bTorch = KindOf(LightMeshFor) == TEXT("torch");
			const FVector Anchor = Body->GetSocketLocation(LightMesh->GetAttachSocketName());
			const FVector Fwd = GetActorForwardVector().GetSafeNormal2D();
			if (bTorch)
			{
				const FVector Up = (FVector::UpVector * 0.94f + Fwd * WRTune(TEXT("lights.torchTilt"), 0.34f)).GetSafeNormal();
				const FRotator Rot = FRotationMatrix::MakeFromXZ(Up, -Fwd).Rotator();
				// The fist closes about the haft a hand above the butt (the mesh's 0).
				LightMesh->SetWorldLocationAndRotation(Anchor, Rot);
			}
			else
			{
				const FVector Out = -GetActorRightVector() * WRTune(TEXT("lights.lanternOut"), 9.f);
				LightMesh->SetWorldLocationAndRotation(Anchor + Out + FVector(0.f, 0.f, WRTune(TEXT("lights.lanternDrop"), 2.f)), FRotator(0.f, GetActorRotation().Yaw, 0.f));
			}
		}

		// Asleep: lie on the bed. The hero has no sleep clip yet: the death clip's last pose (on the back) is held,
		// eased in from rest.poseStart of the clip at rest.poseRate, so the body settles down rather than falls.
		if (bAsleep != bSleepPose)
		{
			bSleepPose = bAsleep;
			UWRHeroAnimInstance* Anim = BodyAnimInstance();
			if (Anim && Body->IsVisible())
			{
				if (bAsleep)
				{
					const FName Key = BodyAnims.Contains(TEXT("sleep")) ? FName(TEXT("sleep")) : FName(TEXT("death"));
					if (FBodyAnimSet* Set = BodyAnims.Find(Key); Set && Set->Clips.Num() > 0)
					{
						UAnimSequenceBase* Clip = Set->Clips[0];
						const bool bOwnClip = Key == TEXT("sleep");
						const float Start = bOwnClip ? 0.f : Clip->GetPlayLength() * WRTune(TEXT("rest.poseStart"), 0.55f);
						Anim->PlayOneShot(Clip, EWRAnimLayer::Full, bOwnClip ? 1.f : WRTune(TEXT("rest.poseRate"), 0.6f), Start, 0.f, true);
						BodyAnim = Key;
						BodyOneShotUntil = 1.0e12;
					}
				}
				else if (IsAlive())
				{
					// Up again (a body that died in its sleep keeps the death pose its own death set).
					Anim->StopOneShot(EWRAnimLayer::Full);
					BodyAnim = NAME_None;
					BodyOneShotUntil = 0.0;
				}
			}
		}
	}

	// The owner gets up by moving or jumping (the server also wakes anyone moved off the bed).
	if (bAsleep && IsLocallyControlled())
	{
		const double Now = GetWorld()->GetTimeSeconds();
		if ((GetLastMovementInputVector().SizeSquared() > 0.04f || GetPendingMovementInputVector().SizeSquared() > 0.04f || bPressedJump) && Now - WakeAskedAt > 0.5)
		{
			WakeAskedAt = Now;
			ServerWakeUp();
		}
	}
}

void AWRCharacter::ServerWakeUp_Implementation()
{
	if (UWRSurvivalComponent* Survival = UWRSurvivalComponent::For(this))
	{
		Survival->Wake(FString());
	}
}
