// Combat feel: what a landed hit looks and sounds like on every machine (impact by what was hit, hit-stop, camera
// shake for the people in it, sounds), plus the camera shake modifier and the hit-stop bookkeeping. Numbers are in
// tuning.json "fx" so they can be tuned without a build; all of it is client-side presentation.

#include "WRFx.h"
#include "WRFxSystem.h"
#include "WRCombatTypes.h"
#include "WRGameplayTags.h"
#include "WRSound.h"
#include "WRTuning.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

uint16 WRFx::HitFlagsFor(const FWRHit& Hit, bool bCrit, bool bRiposte, bool bWeak, bool bShatter, bool bArmored)
{
	uint16 F = 0;
	if (Hit.bHeavy || Hit.bWeaponArt) F |= Hit_Heavy;
	if (bCrit) F |= Hit_Crit;
	if (bRiposte) F |= Hit_Riposte;
	if (bWeak) F |= Hit_Weak;
	if (bShatter) F |= Hit_Shatter;
	if (Hit.bMelee) F |= Hit_Melee;
	if (Hit.bDamageOverTime) F |= Hit_Dot;
	if (Hit.Element == WRTags::Element_Fire) F |= Hit_Fire;
	if (Hit.Element == WRTags::Element_Frost) F |= Hit_Frost;
	if (bArmored) F |= Hit_Armored;
	return F;
}

namespace WRFxCombatDetail
{
	APawn* LocalPawn(const UWorld* World)
	{
		const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		return PC && PC->IsLocalController() ? PC->GetPawn() : nullptr;
	}

	/** Where on the target the blow lands: its body's surface toward where the hit came from, at a believable height. */
	FVector HitPoint(const AActor* Target, const FVector& From, bool bDot)
	{
		const FVector Centre = Target->GetActorLocation();
		float Radius = 40.f, HalfHeight = 90.f;
		Target->GetSimpleCollisionCylinder(Radius, HalfHeight);
		if (bDot)
		{
			return Centre + FVector(FMath::FRandRange(-0.5f, 0.5f) * Radius, FMath::FRandRange(-0.5f, 0.5f) * Radius, HalfHeight * FMath::FRandRange(-0.2f, 0.5f));
		}
		FVector Dir = (From - Centre).GetSafeNormal2D();
		if (Dir.IsNearlyZero()) Dir = Target->GetActorForwardVector();
		FVector P = Centre + Dir * Radius * 0.85f;
		P.Z = Centre.Z + FMath::Clamp(From.Z - Centre.Z + 15.f, -HalfHeight * 0.4f, HalfHeight * 0.6f);
		return P;
	}
}

void WRFx::ShowHit(AActor* Target, AActor* Source, float Damage, EWRHitResult Result, uint16 Flags, const FVector& Origin)
{
	UWRFxSubsystem* Fx = Target ? UWRFxSubsystem::Get(Target) : nullptr;
	if (!Fx || !Fx->IsActive() || Result == EWRHitResult::Ignored)
	{
		return;
	}
	using namespace WRFxCombatDetail;
	const FWRTuning& T = FWRTuning::Get();
	const APawn* Me = LocalPawn(Target->GetWorld());
	const bool bMine = Me && Source == Me;
	const bool bOnMe = Me && Target == Me;
	const bool bDot = (Flags & Hit_Dot) != 0;
	const bool bHeavy = (Flags & (Hit_Heavy | Hit_Crit | Hit_Riposte | Hit_Shatter)) != 0;
	const FVector From = !Origin.IsNearlyZero() ? Origin : Source ? Source->GetActorLocation() : Target->GetActorLocation() + Target->GetActorForwardVector() * 100.f;
	const FVector Point = HitPoint(Target, From, bDot);
	const FVector Toward = (From - Target->GetActorLocation()).GetSafeNormal2D();
	// Impacts spray out of the struck side, back toward the blow (where the camera usually is).
	const FRotator Back = (Toward + FVector(0.f, 0.f, 0.25f)).Rotation();
	const float StopSeconds = T.Num(TEXT("fx.hitStop.seconds"), 0.06f);

	switch (Result)
	{
	case EWRHitResult::Parried:
		WRFx::SpawnAt(Target, TEXT("parry.flash"), Point, Back, 1.f);
		WRSound::PlayAt(Target, TEXT("parry"), Point);
		WRFx::HitStop(Target, Source, T.Num(TEXT("fx.hitStop.parrySeconds"), 0.09f));
		if (bMine || bOnMe)
		{
			Fx->ShakeNamed(TEXT("parry"), FVector::ZeroVector);
			if (bOnMe) Fx->ParryFlashTime = Target->GetWorld()->GetTimeSeconds();
		}
		break;
	case EWRHitResult::Blocked:
	case EWRHitResult::GuardBroken:
	{
		const bool bBroke = Result == EWRHitResult::GuardBroken;
		WRFx::SpawnAt(Target, TEXT("hit.spark"), Point, Back, bBroke ? 1.6f : (bHeavy ? 1.3f : 1.f));
		WRSound::PlayAt(Target, TEXT("hit.block"), Point, bBroke ? 1.3f : 1.f);
		if (bBroke || bHeavy) WRFx::HitStop(Target, Source, StopSeconds);
		if (bOnMe) Fx->ShakeNamed(bBroke ? TEXT("guardBreak") : TEXT("block"), FVector::ZeroVector);
		else if (bMine && bBroke) Fx->ShakeNamed(TEXT("heavyDealt"), FVector::ZeroVector);
		break;
	}
	case EWRHitResult::Hit:
	{
		if (bDot)
		{
			break;  // burn ticks and hazards: the status's own flames show it, the number says how much
		}
		const float Scale = bHeavy ? T.Num(TEXT("fx.impact.heavyScale"), 1.6f) : 1.f;
		// Out of the wound, back toward the blow (where the camera usually is) and to the sides.
		WRFx::SpawnAt(Target, Fx->ImpactFor(Target), Point, Back, Scale);
		if (Flags & Hit_Shatter)
		{
			WRFx::SpawnAt(Target, TEXT("frost.impact"), Point, Back, 1.4f);
		}
		else if ((Flags & Hit_Melee) && (Flags & (Hit_Fire | Hit_Frost)))
		{
			// A flame slash or a frost-infused blade leaves a little of its element on the wound.
			WRFx::SpawnAt(Target, (Flags & Hit_Fire) ? TEXT("fire.impact") : TEXT("frost.impact"), Point, Back, T.Num(TEXT("fx.impact.elementScale"), 0.45f));
		}
		if (Flags & (Hit_Crit | Hit_Riposte))
		{
			WRFx::SpawnAt(Target, TEXT("hit.crit"), Point, Back, 1.f);
		}
		WRSound::PlayAt(Target, (Flags & Hit_Armored) ? TEXT("hit.armor") : TEXT("hit.flesh"), Point, bHeavy ? 1.25f : 1.f);
		if (bHeavy)
		{
			WRFx::HitStop(Target, Source, StopSeconds);
			if (bMine) Fx->ShakeNamed(TEXT("heavyDealt"), FVector::ZeroVector);
			if (bOnMe) Fx->ShakeNamed(TEXT("heavyTaken"), FVector::ZeroVector);
		}
		else if (bOnMe)
		{
			Fx->ShakeNamed(TEXT("lightTaken"), FVector::ZeroVector);
		}
		break;
	}
	default:
		break;
	}
	Fx->AddHudHit(Target, Point, Damage, Result, Flags, bMine, bOnMe);
}

void UWRFxSubsystem::AddHudHit(AActor* Target, const FVector& Point, float Damage, EWRHitResult Result, uint16 Flags, bool bMine, bool bOnMe)
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (bMine && (Result == EWRHitResult::Hit || Result == EWRHitResult::GuardBroken || Result == EWRHitResult::Blocked) && !(Flags & WRFx::Hit_Dot))
	{
		MarkerTime = Now;
		MarkerFlags = Flags;
		MarkerResult = Result;
	}
	// Burn ticks and hazards add up into one number per half second instead of a stream of little ones.
	if ((Flags & WRFx::Hit_Dot) && Result == EWRHitResult::Hit)
	{
		for (FHudHit& H : HudHits)
		{
			if (H.Target.Get() == Target && (H.Flags & WRFx::Hit_Dot) && Now - H.Time < 0.5)
			{
				H.Damage += Damage;
				return;
			}
		}
		if (Damage < 0.05f) return;
	}
	FHudHit& H = HudHits.AddDefaulted_GetRef();
	H.Target = Target;
	H.World = Point;
	H.Damage = Damage;
	H.Result = Result;
	H.Flags = Flags;
	H.Time = Now;
	H.bMine = bMine;
	H.bOnMe = bOnMe;
	H.Drift = FMath::FRandRange(-1.f, 1.f);
	if (HudHits.Num() > 48) HudHits.RemoveAt(0);
}

// ---------------------------------------------------------------- hit-stop

void WRFx::HitStop(AActor* A, AActor* B, float Seconds)
{
	UWRFxSubsystem* Fx = UWRFxSubsystem::Get(A ? A : B);
	if (!Fx || !Fx->IsActive() || Seconds <= 0.f)
	{
		return;
	}
	// The pause is on the animation only (bPauseAnims): movement, prediction and the server carry on, so a listen
	// server's creatures don't really stop and a client never corrects. Never global time dilation.
	const double Until = Fx->GetWorld()->GetTimeSeconds() + Seconds;
	for (AActor* Actor : { A, B })
	{
		if (!Actor) continue;
		TArray<USkeletalMeshComponent*> Meshes;
		Actor->GetComponents(Meshes);
		for (USkeletalMeshComponent* Mesh : Meshes)
		{
			if (!Mesh->IsVisible()) continue;
			UWRFxSubsystem::FHitStop* Found = Fx->HitStops.FindByPredicate([Mesh](const UWRFxSubsystem::FHitStop& H) { return H.Mesh.Get() == Mesh; });
			if (Found)
			{
				Found->Until = FMath::Max(Found->Until, Until);
			}
			else if (!Mesh->bPauseAnims)
			{
				Mesh->bPauseAnims = true;
				Fx->HitStops.Add({ Mesh, Until });
			}
		}
	}
}

void UWRFxSubsystem::TickHitStops(float DeltaTime)
{
	const double Now = GetWorld()->GetTimeSeconds();
	for (int32 i = HitStops.Num() - 1; i >= 0; --i)
	{
		USkeletalMeshComponent* Mesh = HitStops[i].Mesh.Get();
		if (!Mesh || Now >= HitStops[i].Until)
		{
			if (Mesh) Mesh->bPauseAnims = false;
			HitStops.RemoveAtSwap(i);
		}
	}
}

// ---------------------------------------------------------------- camera shake

void WRFx::Shake(const UObject* WorldContext, float Strength, const FVector& Epicentre, float Radius)
{
	UWRFxSubsystem* Fx = UWRFxSubsystem::Get(WorldContext);
	if (!Fx || !Fx->IsActive() || Strength <= 0.f)
	{
		return;
	}
	const UWorld* World = Fx->GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC || !PC->IsLocalController() || !PC->PlayerCameraManager)
	{
		return;
	}
	float K = Strength * WRTune(TEXT("fx.shake.scale"), 1.f);
	if (Radius > 0.f)
	{
		const float Dist = FVector::Dist(PC->PlayerCameraManager->GetCameraLocation(), Epicentre);
		K *= FMath::Clamp(1.f - Dist / Radius, 0.f, 1.f);
	}
	if (K <= 0.001f)
	{
		return;
	}
	UWRFxCameraModifier* Mod = Cast<UWRFxCameraModifier>(PC->PlayerCameraManager->FindCameraModifierByClass(UWRFxCameraModifier::StaticClass()));
	if (!Mod)
	{
		Mod = Cast<UWRFxCameraModifier>(PC->PlayerCameraManager->AddNewCameraModifier(UWRFxCameraModifier::StaticClass()));
	}
	if (Mod)
	{
		Mod->AddTrauma(K);
	}
}

void UWRFxCameraModifier::AddTrauma(float Amount)
{
	Trauma = FMath::Min(1.f, Trauma + Amount);
}

bool UWRFxCameraModifier::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	Super::ModifyCamera(DeltaTime, InOutPOV);
	if (Trauma <= 0.f)
	{
		return false;
	}
	const FWRTuning& T = FWRTuning::Get();
	Clock += DeltaTime * T.Num(TEXT("fx.shake.frequency"), 22.f);
	// Trauma squared: small knocks barely move the view, big ones really jolt it.
	const float K = Trauma * Trauma;
	const float MaxAngle = T.Num(TEXT("fx.shake.maxAngle"), 2.5f);
	const float MaxOffset = T.Num(TEXT("fx.shake.maxOffset"), 6.f);
	InOutPOV.Rotation.Pitch += MaxAngle * K * FMath::PerlinNoise1D(Clock + 11.f);
	InOutPOV.Rotation.Yaw += MaxAngle * K * FMath::PerlinNoise1D(Clock + 47.f);
	InOutPOV.Rotation.Roll += MaxAngle * 0.6f * K * FMath::PerlinNoise1D(Clock + 83.f);
	const FRotationMatrix Axes(InOutPOV.Rotation);
	InOutPOV.Location += (Axes.GetScaledAxis(EAxis::Y) * FMath::PerlinNoise1D(Clock + 131.f) + Axes.GetScaledAxis(EAxis::Z) * FMath::PerlinNoise1D(Clock + 177.f)) * MaxOffset * K;
	Trauma = FMath::Max(0.f, Trauma - DeltaTime * T.Num(TEXT("fx.shake.decay"), 1.6f));
	return false;
}
