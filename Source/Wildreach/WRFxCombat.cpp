// Combat feel: what a landed hit looks and sounds like on every machine (impact by what was hit, hit-stop, camera
// shake for the people in it, sounds), plus the camera shake modifier and the hit-stop bookkeeping. Numbers are in
// tuning.json "fx" so they can be tuned without a build; all of it is client-side presentation.

#include "WRFx.h"
#include "WRPadInput.h"
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
#include "Materials/MaterialInstanceDynamic.h"
#include "WRCharacterBase.h"
#include "WRPlayerController.h"
#include "WRPlayerUI.h"
#include "WRKeyBindings.h"

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
	// How it lands: the move's tier (crits always crit-tier; old callers' heavy blows count as heavy).
	EWRHitTier Tier = Hit.Tier;
	if (Hit.bCritical || bRiposte) Tier = EWRHitTier::Crit;
	else if (Tier == EWRHitTier::Light && (Hit.bHeavy || Hit.bWeaponArt || bShatter)) Tier = EWRHitTier::Heavy;
	return WithTier(F, Tier);
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
	// How hard it lands (COMBAT-AND-LESSONS L1): the tier picks the hit-stop, the flash, the weight of the shake and rumble.
	const EWRHitTier Tier = TierOf(Flags);
	const TCHAR* TierKey = Tier == EWRHitTier::BossKill ? TEXT("bossKill") : Tier == EWRHitTier::Crit ? TEXT("crit")
		: Tier == EWRHitTier::Counter ? TEXT("counter") : Tier == EWRHitTier::Heavy ? TEXT("heavy") : TEXT("light");
	const bool bHeavy = Tier != EWRHitTier::Light;
	const FVector From = !Origin.IsNearlyZero() ? Origin : Source ? Source->GetActorLocation() : Target->GetActorLocation() + Target->GetActorForwardVector() * 100.f;
	const FVector Point = HitPoint(Target, From, bDot);
	const FVector Toward = (From - Target->GetActorLocation()).GetSafeNormal2D();
	// Impacts spray out of the struck side, back toward the blow (where the camera usually is).
	const FRotator Back = (Toward + FVector(0.f, 0.f, 0.25f)).Rotation();
	const float Weight = T.Num(FString::Printf(TEXT("fx.weight.%s"), Result == EWRHitResult::Parried ? TEXT("parry") : TierKey), 0.3f);
	const float Stop = T.Num(FString::Printf(TEXT("fx.hitStop.%s"), TierKey), 0.06f);
	// The freeze waits for the blade's deepest point, a moment after the contact the server saw (fx.hitStop.deep*).
	const float Deep = FMath::Min(T.Num(TEXT("fx.hitStop.deepMax"), 0.04f), Stop * T.Num(TEXT("fx.hitStop.deepFraction"), 0.3f));
	const float ShakeCm = FMath::Lerp(T.Num(TEXT("fx.victimShake.min"), 3.f), T.Num(TEXT("fx.victimShake.max"), 6.f), FMath::Clamp((Weight - 0.3f) / 0.7f, 0.f, 1.f));

	// The people in it feel it: camera trauma 0.04 + 0.22 w^2, the FOV punch 0.2 + 1.3 w degrees, the pad 35 + 85 w ms
	// (the heavy motors only for heavier blows). Never for burn ticks.
	const auto Feel = [&](float W)
	{
		if (!(bMine || bOnMe) || bDot)
		{
			return;
		}
		WRFx::Shake(Target, T.Num(TEXT("fx.trauma.base"), 0.04f) + T.Num(TEXT("fx.trauma.scale"), 0.22f) * W * W);
		if (W >= 0.5f)
		{
			WRFx::FovPunch(Target, T.Num(TEXT("fx.fovPunch.base"), 0.2f) + T.Num(TEXT("fx.fovPunch.scale"), 1.3f) * W, T.Num(TEXT("fx.fovPunch.seconds"), 0.18f));
		}
		const float Ms = T.Num(TEXT("fx.rumble.base"), 35.f) + T.Num(TEXT("fx.rumble.scale"), 85.f) * W;
		const float Strength = FMath::Clamp(T.Num(TEXT("fx.rumble.strength"), 0.9f) * W + (bOnMe ? 0.15f : 0.f), 0.f, 1.f);
		if (W < 0.5f && !bOnMe) WRInput::RumbleLight(Target->GetWorld(), Strength, Ms / 1000.f);
		else WRInput::Rumble(Target->GetWorld(), Strength, Ms / 1000.f);
	};

	switch (Result)
	{
	case EWRHitResult::Parried:
		// Gold: block and parry (colour law). The parrier's gold rim, the sparks, the clang, both frozen a beat.
		WRFx::SpawnAt(Target, TEXT("parry.flash"), Point, Back, 1.f);
		WRSound::PlayAt(Target, TEXT("parry"), Point);
		WRFx::HitStopEx(Source, Target, T.Num(TEXT("fx.hitStop.parry"), 0.09f), 0.f, T.Num(TEXT("fx.victimShake.min"), 3.f), -Toward);
		// The parried attacker takes the gold rim (the victim's flash, in the parry's colour).
		Fx->Flash(Source ? Source : Target, T.Num(TEXT("fx.flash.parry"), 0.18f), true);
		Feel(Weight);
		if (bOnMe) Fx->ParryFlashTime = Target->GetWorld()->GetTimeSeconds();
		break;
	case EWRHitResult::Blocked:
	case EWRHitResult::GuardBroken:
	{
		const bool bBroke = Result == EWRHitResult::GuardBroken;
		WRFx::SpawnAt(Target, bBroke ? TEXT("guard.break") : TEXT("hit.spark"), Point, Back, bBroke ? 1.2f : (bHeavy ? 1.3f : 1.f));
		WRSound::PlayAt(Target, TEXT("hit.block"), Point, bBroke ? 1.3f : 1.f);
		if (bBroke) WRSound::PlayAt(Target, TEXT("guard.break"), Point);
		WRFx::HitStopEx(Target, Source, bBroke ? T.Num(TEXT("fx.hitStop.heavy"), 0.1f) : Stop * 0.6f, Deep, bBroke ? ShakeCm : 0.f, -Toward);
		if (bBroke) Fx->Flash(Target, T.Num(TEXT("fx.flash.heavy"), 0.14f), false);
		Feel(bBroke ? FMath::Max(Weight, 0.6f) : Weight * 0.6f);
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
		const FName Impact = Fx->ImpactFor(Target);
		WRFx::SpawnAt(Target, Impact, Point, Back, Scale);
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
		// Four layers (COMBAT-AND-LESSONS L1): the edge's transient and the weight's thud by what was hit, the material,
		// and the creature's own hurt call (UWRAudioDirector::OnHitShown).
		const bool bArmour = (Flags & Hit_Armored) != 0 || Impact == TEXT("hit.spark");
		const bool bStone = Impact == TEXT("hit.dust") || Impact == TEXT("hit.ice");
		WRSound::PlayAt(Target, bArmour ? TEXT("hit.transient.armor") : bStone ? TEXT("hit.transient.stone") : TEXT("hit.transient.flesh"), Point);
		WRSound::PlayAt(Target, Tier == EWRHitTier::Crit ? TEXT("crit.stab") : Tier == EWRHitTier::Counter ? TEXT("hit.counter")
			: bHeavy ? TEXT("hit.body.heavy") : TEXT("hit.body.light"), Point);
		if (Tier == EWRHitTier::BossKill) WRSound::PlayAt(Target, TEXT("boss.kill"), Point);
		WRSound::PlayAt(Target, bArmour ? TEXT("hit.armor") : Impact == TEXT("hit.ice") ? TEXT("frost.impact") : Impact == TEXT("hit.dust") ? TEXT("hit.dust") : TEXT("hit.flesh"),
			Point, bHeavy ? 1.25f : 1.f);
		// Every connected blow freezes both bodies a beat by its tier, the victim shaking along it, flashing white.
		WRFx::HitStopEx(Target, Source, Stop, Deep, ShakeCm, -Toward);
		Fx->Flash(Target, T.Num(FString::Printf(TEXT("fx.flash.%s"), Tier == EWRHitTier::BossKill ? TEXT("crit") : TierKey), 0.07f), false);
		Feel(Weight);
		break;
	}
	default:
		break;
	}
	// State words only (COMBAT-AND-LESSONS L6): GUARD BROKEN on the one whose guard broke and who broke it, STANCE BROKEN
	// for who broke it (a parry's breaker is the one parrying). Never PERFECT or BLOCKED.
	if (AWRPlayerController* PC = Me ? Cast<AWRPlayerController>(Me->GetController()) : nullptr)
	{
		if (FWRPlayerUI* UI = PC->GetUI())
		{
			if (Result == EWRHitResult::GuardBroken && (bMine || bOnMe))
			{
				UI->NoteNotice(TEXT("GUARD BROKEN"));
			}
			const bool bBreaker = Result == EWRHitResult::Parried ? bOnMe : bMine;
			if ((Flags & Hit_StanceBreak) && bBreaker)
			{
				UI->NoteNotice(TEXT("STANCE BROKEN"));
			}
		}
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
	// Damage numbers only when they're turned on (Settings, off by default: combat.md 2.14), and no number for a parry,
	// a dodge or a guard that took it all (no hit words, COMBAT-AND-LESSONS L6).
	if (!FWRKeyBindings::Get().DamageNumbers() || Result == EWRHitResult::Parried || Result == EWRHitResult::Dodged
		|| ((Result == EWRHitResult::Blocked || Result == EWRHitResult::GuardBroken) && Damage < 0.5f))
	{
		return;
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

/// ---------------------------------------------------------------- hit-stop

void WRFx::HitStop(AActor* A, AActor* B, float Seconds)
{
	HitStopEx(A, B, Seconds, 0.f, 0.f, FVector::ZeroVector);
}

void WRFx::HitStopEx(AActor* Victim, AActor* Attacker, float Seconds, float Delay, float ShakeCm, const FVector& Dir)
{
	UWRFxSubsystem* Fx = UWRFxSubsystem::Get(Victim ? Victim : Attacker);
	if (!Fx || !Fx->IsActive() || Seconds <= 0.f)
	{
		return;
	}
	// Animation only (the mesh's GlobalAnimRateScale): movement, prediction and the server carry on, so a listen server's
	// creatures don't really stop and a client never corrects. Never global time dilation (COMBAT-AND-LESSONS 1.17).
	Seconds = FMath::Min(Seconds, 0.3f);  // nothing over 0.3 s (L1)
	const double Now = Fx->GetWorld()->GetTimeSeconds();
	const double Start = Now + FMath::Max(0.f, Delay);
	const double Until = Start + Seconds;
	const float CatchUp = FMath::Max(0.02f, WRTune(TEXT("fx.hitStop.catchUp"), 0.1f));
	for (AActor* Actor : { Victim, Attacker })
	{
		if (!Actor) continue;
		const bool bVictim = Actor == Victim;
		TArray<USkeletalMeshComponent*> Meshes;
		Actor->GetComponents(Meshes);
		for (USkeletalMeshComponent* Mesh : Meshes)
		{
			if (!Mesh->IsVisible()) continue;
			UWRFxSubsystem::FHitStop* Found = Fx->HitStops.FindByPredicate([Mesh](const UWRFxSubsystem::FHitStop& H) { return H.Mesh.Get() == Mesh; });
			if (!Found)
			{
				Found = &Fx->HitStops.AddDefaulted_GetRef();
				Found->Mesh = Mesh;
				Found->Start = Start;
			}
			Found->Until = FMath::Max(Found->Until, Until);
			Found->Seconds = FMath::Max(0.f, float(Found->Until - Found->Start));
			Found->CatchUpUntil = Found->Until + CatchUp;
			// Only the body actually struck shakes, and only its root mesh (the pieces attached to it go with it); not a
			// body posed by its own code right now (a creature rearing into a strike, or slumped in a stance break).
			const AWRCharacterBase* Char = Cast<AWRCharacterBase>(Actor);
			const bool bPosed = Char && (Char->HasTag(WRTags::Status_StanceBroken) || (Char->IsCreature() && Char->HasTag(WRTags::State_Windup)));
			if (bVictim && ShakeCm > 0.f && !bPosed && !(Mesh->GetAttachParent() && Mesh->GetAttachParent()->IsA<USkeletalMeshComponent>()))
			{
				Found->ShakeCm = FMath::Max(Found->ShakeCm, ShakeCm);
				Found->ShakeDir = Dir.IsNearlyZero() ? Actor->GetActorForwardVector() : Dir.GetSafeNormal();
			}
		}
	}
}

void UWRFxSubsystem::TickHitStops(float DeltaTime)
{
	const double Now = GetWorld()->GetTimeSeconds();
	const float Creep = FMath::Clamp(WRTune(TEXT("fx.hitStop.creep"), 0.02f), 0.f, 1.f);
	const float Hz = WRTune(TEXT("fx.victimShake.hz"), 30.f);
	const float CatchUp = FMath::Max(0.02f, WRTune(TEXT("fx.hitStop.catchUp"), 0.1f));
	for (int32 i = HitStops.Num() - 1; i >= 0; --i)
	{
		FHitStop& H = HitStops[i];
		USkeletalMeshComponent* Mesh = H.Mesh.Get();
		if (!Mesh || Now >= H.CatchUpUntil)
		{
			if (Mesh)
			{
				Mesh->GlobalAnimRateScale = 1.f;
				if (!H.Applied.IsZero()) Mesh->AddWorldOffset(-H.Applied);
			}
			HitStops.RemoveAtSwap(i);
			continue;
		}
		if (Now < H.Start)
		{
			continue;  // waiting for the blade's deepest point
		}
		FVector Offset = FVector::ZeroVector;
		if (Now < H.Until)
		{
			// Frozen: creeping at a sliver of speed (it reads as weight, not a hitch); the struck body shakes along the
			// blow at Hz, fading out as the freeze ends.
			Mesh->GlobalAnimRateScale = Creep;
			if (H.ShakeCm > 0.f)
			{
				const float Age = float(Now - H.Start);
				const float Fade = 1.f - Age / FMath::Max(0.01f, H.Seconds);
				Offset = H.ShakeDir * H.ShakeCm * Fade * FMath::Sin(2.f * PI * Hz * Age);
			}
		}
		else
		{
			// Catch up: the time the freeze held back is played back over catchUp s, so the swing lands on the server's
			// timing again with no snap.
			Mesh->GlobalAnimRateScale = 1.f + (1.f - Creep) * H.Seconds / CatchUp;
		}
		if (!(Offset - H.Applied).IsNearlyZero())
		{
			Mesh->AddWorldOffset(Offset - H.Applied);
			H.Applied = Offset;
		}
	}
	TickFlashes();
}

void WRFx::FovPunch(const UObject* WorldContext, float Degrees, float Seconds)
{
	UWRFxSubsystem* Fx = UWRFxSubsystem::Get(WorldContext);
	const UWorld* World = Fx && Fx->IsActive() ? Fx->GetWorld() : nullptr;
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC || !PC->IsLocalController() || !PC->PlayerCameraManager || Degrees <= 0.f || WRTune(TEXT("fx.shake.scale"), 1.f) <= 0.f)
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
		Mod->AddFovPunch(Degrees, Seconds);
	}
}

void UWRFxCameraModifier::AddFovPunch(float Degrees, float Seconds)
{
	// The bigger punch wins; it restarts from full.
	if (Degrees >= FovPunch * FMath::Square(1.f - FMath::Clamp(FovAge / FMath::Max(0.01f, FovSeconds), 0.f, 1.f)))
	{
		FovPunch = Degrees;
		FovSeconds = FMath::Max(0.05f, Seconds);
		FovAge = 0.f;
	}
}

// ---------------------------------------------------------------- the victim's flash

void UWRFxSubsystem::Flash(AActor* Target, float Seconds, bool bGold)
{
	if (!Target || !bActive || Seconds <= 0.f)
	{
		return;
	}
	TObjectPtr<UMaterialInstanceDynamic>& Mat = bGold ? FlashGold : FlashWhite;
	if (!Mat)
	{
		if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/FX/M_WindupGlow.M_WindupGlow")))
		{
			Mat = UMaterialInstanceDynamic::Create(Base, this);
			// White for a blow, gold for a parry (colour law); sRGB.
			Mat->SetVectorParameterValue(TEXT("Color"), bGold ? FLinearColor(FColor(255, 205, 110)) : FLinearColor(FColor(255, 255, 255)));
			Mat->SetScalarParameterValue(TEXT("Glow"), WRTune(TEXT("fx.flash.glow"), 2.f));
			Mat->SetScalarParameterValue(TEXT("Opacity"), WRTune(TEXT("fx.flash.opacity"), 0.55f));
			Mat->SetScalarParameterValue(TEXT("Wash"), bGold ? 0.15f : 0.55f);
		}
	}
	if (!Mat)
	{
		return;
	}
	const double Until = GetWorld()->GetTimeSeconds() + Seconds;
	TArray<UMeshComponent*> Meshes;
	Target->GetComponents(Meshes);
	for (UMeshComponent* Mesh : Meshes)
	{
		// The body and what it wears (the struck creature's art, the hero's body, face and outfit), not held props.
		if (!Mesh->IsVisible() || !Mesh->IsA<USkeletalMeshComponent>())
		{
			continue;
		}
		FFlash* Found = Flashes.FindByPredicate([Mesh](const FFlash& F) { return F.Mesh.Get() == Mesh; });
		if (!Found)
		{
			Found = &Flashes.AddDefaulted_GetRef();
			Found->Mesh = Mesh;
			Found->Previous = Mesh->GetOverlayMaterial();
		}
		Found->Until = FMath::Max(Found->Until, Until);
		Mesh->SetOverlayMaterial(Mat);
	}
}

void UWRFxSubsystem::TickFlashes()
{
	const double Now = GetWorld()->GetTimeSeconds();
	for (int32 i = Flashes.Num() - 1; i >= 0; --i)
	{
		UMeshComponent* Mesh = Flashes[i].Mesh.Get();
		if (!Mesh || Now >= Flashes[i].Until)
		{
			// Put back what it wore (a telegraph glow) unless something else changed it meanwhile.
			if (Mesh && (Mesh->GetOverlayMaterial() == FlashWhite || Mesh->GetOverlayMaterial() == FlashGold))
			{
				Mesh->SetOverlayMaterial(Flashes[i].Previous.Get());
			}
			Flashes.RemoveAtSwap(i);
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
	ModifyFov(DeltaTime, InOutPOV);
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

bool UWRFxCameraModifier::ModifyFov(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	// The FOV punch: in at once, eased back out (squared), so a heavy blow reads as a jolt of the eyes, not a zoom.
	if (FovPunch > 0.f && FovSeconds > 0.f)
	{
		FovAge += DeltaTime;
		const float Left = 1.f - FMath::Clamp(FovAge / FovSeconds, 0.f, 1.f);
		InOutPOV.FOV -= FovPunch * Left * Left;
		if (Left <= 0.f)
		{
			FovPunch = 0.f;
		}
	}
	return false;
}
