#include "WRAbilitiesSun.h"
#include "WRRaiding.h"
#include "Wildreach.h"
#include "WRAttributeSet.h"
#include "WRBloodPath.h"
#include "WRBoss.h"
#include "WRCharacter.h"
#include "WRCharacterBase.h"
#include "WRCombatLibrary.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRGearPerks.h"
#include "WRPathComponent.h"
#include "WRPlayerController.h"
#include "WRProjectile.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRSunPath.h"
#include "WRCombos.h"
#include "WRTuning.h"
#include "WRWindPath.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "AbilitySystemComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

namespace WRAbilitiesSunImpl
{
	static const FLinearColor SnGold(FColor(0xE8, 0xB2, 0x30));
	float SnNow(const UObject* Ctx)
	{
		const UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
		const AGameStateBase* GS = W ? W->GetGameState() : nullptr;
		return GS ? float(GS->GetServerWorldTimeSeconds()) : (W ? W->GetTimeSeconds() : 0.f);
	}
	AWRCharacterBase* SnLocked(const AWRCharacterBase* Caster)
	{
		const AWRPlayerController* PC = Caster ? Cast<AWRPlayerController>(Caster->GetController()) : nullptr;
		AWRCharacterBase* T = PC ? PC->GetServerLock() : nullptr;
		return T && T->IsAlive() && !T->IsHidden() && Caster->IsHostileTo(T) && FVector::Dist(T->GetActorLocation(), Caster->GetActorLocation()) <= WRTune(TEXT("spells.lockRange"), 4000.f) ? T : nullptr;
	}
	FVector SnCrosshair(const AWRCharacterBase* Caster, const FVector& From)
	{
		const FVector View = Caster->GetBaseAimRotation().Vector();
		const AWRCharacter* S = Cast<AWRCharacter>(Caster);
		FVector Point;
		if (!S || !S->GetController() || !S->CrosshairPoint(20000.f, true, Point)) return View;
		const FVector Dir = (Point - From).GetSafeNormal();
		return Dir.IsNearlyZero() || FVector::DistSquared(Point, From) < FMath::Square(100.f) || FVector::DotProduct(Dir, View) < 0.3f ? View : Dir;
	}
	bool SnCanPay(const FGameplayAbilityActorInfo* ActorInfo, float Cost)
	{
		const AWRCharacterBase* Who = ActorInfo ? Cast<AWRCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
		if (!Who) return false;
		if (WRSun::HasRadiance(Who, Cost)) return true;
		if (Who->IsLocallyControlled()) WRSun::NoteRefused(Who);
		return false;
	}
	/** lowland / midland / highland, or boss (a major) / mini. */
	FName SnTier(const AWRCharacterBase* Who)
	{
		if (const AWRBoss* B = Cast<AWRBoss>(Who)) return B->IsMajor() ? FName(TEXT("boss")) : FName(TEXT("mini"));
		const AWRCreature* C = Cast<AWRCreature>(Who);
		const FWRCreatureDef* D = C ? C->GetDef() : nullptr;
		return D && !D->Tier.IsNone() ? D->Tier : FName(TEXT("lowland"));
	}
	bool SnSmall(FName Tier) { return Tier == TEXT("lowland") || Tier == TEXT("midland"); }
	/** Undead and void creatures (the Beacon burns them): the Graveyard's and the Wasteland's, a raised minion. */
	bool SnDark(const AWRCharacterBase* Who)
	{
		const AWRCreature* C = Cast<AWRCreature>(Who);
		const FWRCreatureDef* D = C ? C->GetDef() : nullptr;
		return D && (D->Element == TEXT("void") || D->Element == TEXT("necromancy"));
	}
	TArray<AWRCharacterBase*> SnHostilesNear(AWRCharacterBase* Caster, const FVector& At, float Radius)
	{
		TArray<AWRCharacterBase*> Out;
		for (TActorIterator<AWRCharacterBase> It(Caster->GetWorld()); It; ++It)
		{
			if (!It->IsAlive() || !Caster->IsHostileTo(*It)) continue;
			if (FVector::DistSquared(It->GetActorLocation(), At) <= FMath::Square(Radius + It->GetSimpleCollisionRadius())) Out.Add(*It);
		}
		return Out;
	}
	FVector SnGround(const AActor* Ctx, const FVector& At)
	{
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRSunGround), false, Ctx);
		return Ctx->GetWorld()->LineTraceSingleByChannel(Hit, At + FVector(0, 0, 500), At - FVector(0, 0, 3000), ECC_WorldStatic, Q) ? FVector(Hit.ImpactPoint) : At;
	}
	FWRHit SnBase(AActor* Caster, float Damage, float Blind)
	{
		FWRHit Hit;
		Hit.Instigator = Caster;
		Hit.Damage = Damage;
		Hit.PathId = TEXT("sun");
		Hit.bSpell = true;
		if (Blind > 0.f) Hit.Buildups.Add(TEXT("blind"), Blind);
		Hit.Origin = Caster ? Caster->GetActorLocation() : FVector::ZeroVector;
		return Hit;
	}
	float SnShift(const AActor* Caster) { return 1.f + FMath::Max(0.f, UWRPathComponent::Stat(Caster, TEXT("stormRadius"))); }
}
using namespace WRAbilitiesSunImpl;

// ================================================================================================ the spell base

UWRGA_SunSpell::UWRGA_SunSpell()
{
	StaminaKey = TEXT("stamina.spell");
	StaminaFallback = 12.f;
	PathId = TEXT("sun");
	ActivationOwnedTags.AddTag(WRTags::State_Attacking);
	FGameplayTagContainer Blocked = ActivationBlockedTags;
	Blocked.AddTag(WRTags::State_Attacking);
	Blocked.AddTag(WRTags::State_Dodging);
	ActivationBlockedTags = Blocked;
	bAllowedMounted = true;
}

float UWRGA_SunSpell::Spell(const TCHAR* Field, float Fallback) const { return FWRTuning::Get().Num(SpellKey + TEXT(".") + Field, Fallback); }
// Wardcloth's four: spells cost less (spellCost, WRGearPerks).
float UWRGA_SunSpell::RadianceCost() const { return Spell(TEXT("radianceCost"), 0.f) * WRGearPerks::SpellCostScale(CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr); }

bool UWRGA_SunSpell::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	return Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags) && SnCanPay(ActorInfo, RadianceCost());
}

void UWRGA_SunSpell::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	Super::ApplyCost(Handle, ActorInfo, ActivationInfo);
	AWRCharacterBase* Who = ActorInfo ? Cast<AWRCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (Who && HasAuthority(&ActivationInfo)) WRSun::SpendRadiance(Who, RadianceCost());
}

FWRHit UWRGA_SunSpell::SunHit(AWRCharacterBase* Caster, float Damage, float Blind) const { return SnBase(Caster, Damage, Blind); }

// ---------------------------------------------------------------- Sacrifice to the sun (Sun + Blood 50)

namespace WRSunSacrificeImpl
{
	float Num(const TCHAR* Key, float Fallback) { return FWRComboRegistry::Get().Num(TEXT("sacrifice_to_the_sun"), FName(Key), Fallback); }
}

UWRGA_SunSacrifice::UWRGA_SunSacrifice()
{
	DisplayName = TEXT("Sacrifice to the sun"); PathId = TEXT("sun"); NodeId = TEXT("sacrifice_to_the_sun");
	CooldownFallback = 20.f;   // combos.json's cooldown, read again as it goes off
}

bool UWRGA_SunSacrifice::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	// Yours while the pair earns it (its slot goes with the tag); nothing to fill when the bar is already full.
	const AWRCharacterBase* Who = ActorInfo ? Cast<AWRCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	return Who && Who->HasTag(WRSun::Combo_SunSacrifice) && WRSun::RadianceOf(Who) < WRSun::MaxRadianceOf(Who) - 0.5f
		&& Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

bool UWRGA_SunSacrifice::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	// The Blood rule: HP above the price pays (it may leave you at 1); short, the slot shakes.
	const AWRCharacterBase* Who = ActorInfo ? Cast<AWRCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (UWRBloodPath::CanSpend(Who, WRSunSacrificeImpl::Num(TEXT("health"), 20.f))) return Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags);
	if (Who && Who->IsLocallyControlled()) WRBlood::NoteRefused(Who);
	return false;
}

void UWRGA_SunSacrifice::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	Super::ApplyCost(Handle, ActorInfo, ActivationInfo);
	AWRCharacterBase* Who = ActorInfo ? Cast<AWRCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (Who && HasAuthority(&ActivationInfo)) UWRBloodPath::SpendBlood(Who, WRSunSacrificeImpl::Num(TEXT("health"), 20.f));   // into Spilled
}

void UWRGA_SunSacrifice::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	CooldownFallback = WRSunSacrificeImpl::Num(TEXT("cooldown"), 20.f);
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	AWRCharacterBase* Who = GetCharacter();
	if (Who && IsServer())
	{
		// The blood burns off the skin in a gold flash and the bar fills at once.
		const float Radiance = WRSunSacrificeImpl::Num(TEXT("radiance"), 40.f);
		WRSun::GainRadiance(Who, Radiance);
		const FVector Chest = Who->GetActorLocation() + FVector(0.f, 0.f, Who->GetSimpleCollisionHalfHeight() * 0.4f);
		WRFx::Broadcast(Who, TEXT("blood.cast"), Chest, Who->GetActorRotation(), 1.f, TEXT("blood.cast"));
		WRFx::Broadcast(Who, TEXT("sun.flare"), Chest, FRotator::ZeroRotator, 0.4f, TEXT("sun.cast"));
		UE_LOG(LogWildreach, Log, TEXT("SUN %s sacrifices %.0f HP to the sun: +%.0f Radiance (%.0f now)"), *Who->GetName(), WRSunSacrificeImpl::Num(TEXT("health"), 20.f), Radiance, WRSun::RadianceOf(Who));
	}
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UWRGA_SunSpell::TakeAim(AWRCharacterBase* Caster)
{
	const FVector Chest = Caster->GetActorLocation() + FVector(0.f, 0.f, 40.f);
	AWRCharacterBase* Lock = SnLocked(Caster);
	AimLock = Lock;
	const float Speed = Spell(TEXT("speed"), 3400.f);
	AimDir = Lock ? (UWRCombatLibrary::LeadPoint(Chest, Lock->GetActorLocation(), Lock->GetVelocity(), Speed) - Chest).GetSafeNormal() : SnCrosshair(Caster, Chest);
}

FVector UWRGA_SunSpell::AimedGround(AWRCharacterBase* Caster, float Range, float Ahead) const
{
	FVector At = Caster->GetActorLocation() + AimDir.GetSafeNormal2D() * Ahead;
	if (const AWRCharacterBase* Lock = AimLock.Get()) At = Lock->GetActorLocation();
	const AWRCharacter* S = Cast<AWRCharacter>(Caster);
	FVector Point;
	if (!AimLock.IsValid() && S && S->CrosshairPoint(Range, false, Point)) At = Point;
	return SnGround(Caster, At);
}

void UWRGA_SunSpell::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	AWRCharacterBase* Caster = GetCharacter();
	if (!Caster)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	const FWRTuning& T = FWRTuning::Get();
	const FString ShapeRow = TEXT("spells.shapes.") + ShapeName;
	const float Windup = Spell(TEXT("windup"), T.Num(ShapeRow + TEXT(".windup"), 0.3f));
	const float Recovery = Spell(TEXT("recovery"), T.Num(ShapeRow + TEXT(".recovery"), 0.35f));
	const bool bRoots = Spell(TEXT("roots"), T.Num(ShapeRow + TEXT(".roots"), 0.f)) > 0.f;
	if (bRoots) SetMoveRoot(true);
	else SetMoveSlow(true, true);
	Caster->PlaySwing(EWRSwingKind::Cast, Windup + 0.1f);
	Caster->SetCombatTag(WRTags::State_Casting, true);
	// The cast at the hand: the light gathering, a rising glass note (the cook's proc rule: you see it coming).
	if (IsServer()) WRFx::BroadcastOn(Caster, TEXT("sun.cast"), TEXT("hand"), 1.f, TEXT("sun.cast"), Windup + 0.1f);
	After(Windup, [this]()
	{
		AWRCharacterBase* C = GetCharacter();
		if (!C) return;
		C->SetCombatTag(WRTags::State_Casting, false);
		if (!IsServer() || !C->IsAlive()) return;
		TakeAim(C);
		CastSun(C);
	});
	After(Windup + Recovery, [this]() { EndSelf(); });
}

void UWRGA_SunSpell::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (AWRCharacterBase* C = GetCharacter()) C->SetCombatTag(WRTags::State_Casting, false);
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// ================================================================================================ Sunbeam

UWRGA_Sunbeam::UWRGA_Sunbeam()
{
	DisplayName = TEXT("Sunbeam"); NodeId = TEXT("sunbeam");
	SpellKey = TEXT("spells.sunbeam"); CooldownKey = TEXT("spells.sunbeam.cooldown"); CooldownFallback = 1.f;
	ShapeName = TEXT("bolt");
}

void UWRGA_Sunbeam::CastSun(AWRCharacterBase* Caster)
{
	// A bolt of hard light (3,400 cm/s, a gold streak); it homes only on creatures (fire-frost RAISE 3), never players.
	const float Speed = FMath::Min(Spell(TEXT("speed"), 3400.f), WRTune(TEXT("spells.maxBoltSpeed"), 4000.f));
	const FVector Chest = Caster->GetActorLocation() + FVector(0.f, 0.f, 40.f);
	FWRHit Hit = SunHit(Caster, Spell(TEXT("damage"), 16.f), Spell(TEXT("blind"), 30.f));
	Hit.bProjectile = true;
	Hit.bReflectable = true;
	Hit.PoiseDamage = Spell(TEXT("poise"), 8.f);
	Hit.Origin = Chest;
	FHitResult Near;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(WRSunMuzzle), false, Caster);
	if (Caster->GetWorld()->SweepSingleByChannel(Near, Chest, Chest + AimDir * 70.f, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(14.f), Query))
	{
		AWRCharacterBase* Blocker = Cast<AWRCharacterBase>(Near.GetActor());
		if (Blocker && Caster->IsHostileTo(Blocker)) { UWRCombatLibrary::ApplyHit(Blocker, Hit); return; }
	}
	FActorSpawnParameters Params;
	Params.Owner = Caster;
	Params.Instigator = Caster;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AWRProjectile* Bolt = Caster->GetWorld()->SpawnActor<AWRProjectile>(AWRProjectile::StaticClass(), Chest + AimDir * 70.f, AimDir.Rotation(), Params))
	{
		Bolt->Init(Hit, Speed, SnGold);
		Bolt->SetTrail(TEXT("sun.bolt"));
		AWRCharacterBase* Lock = AimLock.Get();
		if (Lock && Lock->IsAlive() && Lock->IsCreature())
		{
			const float Homing = WRTune(TEXT("spells.lockHoming"), 6000.f);
			if (Homing > 0.f) Bolt->HomeOn(Lock, Homing);
		}
	}
	WRSound::PlayAt(Caster, TEXT("sun.bolt"), Chest);
}

// ================================================================================================ Solar flare

UWRGA_SolarFlare::UWRGA_SolarFlare()
{
	DisplayName = TEXT("Solar flare"); NodeId = TEXT("solar_flare");
	SpellKey = TEXT("spells.solarFlare"); CooldownKey = TEXT("spells.solarFlare.cooldown"); CooldownFallback = 14.f;
	ShapeName = TEXT("nova");
}

void UWRGA_SolarFlare::CastSun(AWRCharacterBase* Caster)
{
	// A flash of white-gold light round you: everything within 6 m blinded (80), 10 damage.
	const float R = Spell(TEXT("radius"), 600.f);
	const FVector At = Caster->GetActorLocation();
	WRFx::Broadcast(Caster, TEXT("sun.flare"), At, FRotator::ZeroRotator, R / 600.f, TEXT("sun.flare"));
	for (AWRCharacterBase* Who : SnHostilesNear(Caster, At, R))
	{
		FWRHit Hit = SunHit(Caster, Spell(TEXT("damage"), 10.f), Spell(TEXT("blind"), 80.f));
		Hit.ImpactPoint = Who->GetActorLocation();
		UWRCombatLibrary::ApplyHit(Who, Hit);
	}
}

// ================================================================================================ the fields

static AWRSunField* SnSpawnField(AWRCharacterBase* Caster, const FVector& At, FName Kind, float Radius, float Seconds)
{
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWRSunField* F = Caster->GetWorld()->SpawnActorDeferred<AWRSunField>(AWRSunField::StaticClass(), FTransform(At), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!F) return nullptr;
	F->Caster = Caster;
	F->Kind = Kind;
	F->Radius = Radius;
	F->EndAt = SnNow(Caster) + Seconds;
	F->FinishSpawning(FTransform(At));
	return F;
}

UWRGA_SunBeacon::UWRGA_SunBeacon()
{
	DisplayName = TEXT("Beacon"); NodeId = TEXT("beacon");
	SpellKey = TEXT("spells.beacon"); CooldownKey = TEXT("spells.beacon.cooldown"); CooldownFallback = 20.f;
	ShapeName = TEXT("pool");
}

void UWRGA_SunBeacon::CastSun(AWRCharacterBase* Caster)
{
	const FVector At = AimedGround(Caster, Spell(TEXT("range"), 1500.f), 600.f);
	SnSpawnField(Caster, At, TEXT("beacon"), Spell(TEXT("radius"), 500.f), Spell(TEXT("seconds"), 10.f));
	WRSound::PlayAt(Caster, TEXT("sun.beacon"), At);
}

UWRGA_Sandstorm::UWRGA_Sandstorm()
{
	DisplayName = TEXT("Sandstorm"); NodeId = TEXT("sandstorm");
	SpellKey = TEXT("spells.sandstorm"); CooldownKey = TEXT("spells.sandstorm.cooldown"); CooldownFallback = 20.f;
	ShapeName = TEXT("buff");
}

void UWRGA_Sandstorm::CastSun(AWRCharacterBase* Caster)
{
	if (AWRSunField* F = SnSpawnField(Caster, Caster->GetActorLocation(), TEXT("sandstorm"), Spell(TEXT("radius"), 700.f) * SnShift(Caster), Spell(TEXT("seconds"), 8.f)))
	{
		F->AttachToActor(Caster, FAttachmentTransformRules::KeepWorldTransform);
	}
	WRSound::Broadcast(Caster, TEXT("sun.storm"));
}

UWRGA_DesertsWrath::UWRGA_DesertsWrath()
{
	DisplayName = TEXT("Desert's wrath"); NodeId = TEXT("deserts_wrath");
	SpellKey = TEXT("spells.desertsWrath"); CooldownKey = TEXT("spells.desertsWrath.cooldown"); CooldownFallback = 60.f;
	ShapeName = TEXT("storm");
}

void UWRGA_DesertsWrath::CastSun(AWRCharacterBase* Caster)
{
	if (AWRSunField* F = SnSpawnField(Caster, Caster->GetActorLocation(), TEXT("wrath"), Spell(TEXT("radius"), 1000.f) * SnShift(Caster), Spell(TEXT("seconds"), 10.f)))
	{
		F->AttachToActor(Caster, FAttachmentTransformRules::KeepWorldTransform);
	}
	WRSound::Broadcast(Caster, TEXT("sun.wrath"));
}

UWRGA_Quicksand::UWRGA_Quicksand()
{
	DisplayName = TEXT("Quicksand"); NodeId = TEXT("quicksand");
	SpellKey = TEXT("spells.quicksand"); CooldownKey = TEXT("spells.quicksand.cooldown"); CooldownFallback = 16.f;
	ShapeName = TEXT("pool");
}

void UWRGA_Quicksand::CastSun(AWRCharacterBase* Caster)
{
	const FVector At = AimedGround(Caster, Spell(TEXT("range"), 1500.f), 700.f);
	SnSpawnField(Caster, At, TEXT("quicksand"), Spell(TEXT("radius"), 300.f), Spell(TEXT("sandSeconds"), 6.f));
	WRSound::PlayAt(Caster, TEXT("sun.quicksand"), At);
}

// ================================================================================================ Sunburst

UWRGA_Sunburst::UWRGA_Sunburst()
{
	DisplayName = TEXT("Sunburst"); NodeId = TEXT("sunburst");
	SpellKey = TEXT("spells.sunburst"); CooldownKey = TEXT("spells.sunburst.cooldown"); CooldownFallback = 50.f;
	ShapeName = TEXT("strike");
}

void UWRGA_Sunburst::CastSun(AWRCharacterBase* Caster)
{
	const FVector At = AimedGround(Caster, Spell(TEXT("range"), 2500.f), 1000.f);
	AWRSunColumn::Drop(Caster, At, Spell(TEXT("radius"), 600.f), Spell(TEXT("delay"), 1.f), Spell(TEXT("damage"), 70.f), Spell(TEXT("blind"), 100.f), Spell(TEXT("stance"), 30.f), NodeId);
}

// ================================================================================================ Burrow

UWRGA_Burrow::UWRGA_Burrow()
{
	DisplayName = TEXT("Burrow"); PathId = TEXT("sun"); NodeId = TEXT("burrow");
	StaminaKey = TEXT("stamina.dodge"); StaminaFallback = 10.f;
	CooldownKey = TEXT("spells.burrow.cooldown"); CooldownFallback = 4.f;
	FGameplayTagContainer Blocked = ActivationBlockedTags;
	Blocked.AddTag(WRTags::State_Attacking);
	ActivationBlockedTags = Blocked;
	ActivationOwnedTags.AddTag(WRTags::State_Dodging);
}

bool UWRGA_Burrow::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)) return false;
	const AWRCharacterBase* Who = ActorInfo ? Cast<AWRCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!Who || !Who->GetCharacterMovement() || !Who->GetCharacterMovement()->IsMovingOnGround()) return false;
	// Sand only (any land's sand: the cook says it greys out "needs sand" elsewhere: the slot reads this same rule).
	if (!WRSun::SandUnder(Who))
	{
		if (Who->IsLocallyControlled())
		{
			if (AWRPlayerController* PC = Cast<AWRPlayerController>(Who->GetController())) PC->ShowLocalNotice(TEXT("Burrow needs sand under you"));
		}
		return false;
	}
	return SnCanPay(ActorInfo, WRTune(TEXT("spells.burrow.radianceCost"), 10.f));
}

void UWRGA_Burrow::SetUnder(bool bOn)
{
	AWRCharacterBase* C = GetCharacter();
	if (!C || !IsServer() || bUnder == bOn) return;
	bUnder = bOn;
	// Under the sand: untouchable and unseen (the body's hidden on every machine; the fin and the hiss show where you go).
	if (bOn) C->GetAbilitySystemComponent()->AddLooseGameplayTag(WRTags::State_Invulnerable);
	else C->GetAbilitySystemComponent()->RemoveLooseGameplayTag(WRTags::State_Invulnerable);
	C->SetActorHiddenInGame(bOn);
}

void UWRGA_Burrow::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	AWRCharacterBase* C = GetCharacter();
	if (!C)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	const FWRTuning& T = FWRTuning::Get();
	FVector Dir = C->GetCharacterMovement()->GetCurrentAcceleration().GetSafeNormal2D();
	if (Dir.IsNearlyZero()) Dir = FRotator(0.f, C->GetControlRotation().Yaw, 0.f).Vector();
	const float Windup = T.Num(TEXT("spells.burrow.windup"), 0.12f);
	const float TravelTime = FMath::Max(0.05f, T.Num(TEXT("spells.burrow.travel"), 0.45f));
	const float Distance = T.Num(TEXT("spells.burrow.distance"), 800.f);
	const float Recovery = T.Num(TEXT("spells.burrow.recovery"), 0.2f);
	C->SetActorRotation(Dir.Rotation());
	C->PlaySwing(EWRSwingKind::Cast, Windup);
	if (IsServer())
	{
		WRSun::SpendRadiance(C, T.Num(TEXT("spells.burrow.radianceCost"), 10.f));
		const FVector Feet = C->GetActorLocation() - FVector(0, 0, C->GetSimpleCollisionHalfHeight());
		WRFx::Broadcast(C, TEXT("sun.burrow_dive"), Feet, Dir.Rotation(), 1.f, TEXT("sun.burrow"));
	}
	After(Windup, [this, Dir, Distance, TravelTime]()
	{
		AWRCharacterBase* Ch = GetCharacter();
		if (!Ch) return;
		SetUnder(true);
		if (IsServer()) WRFx::BroadcastOn(Ch, TEXT("sun.burrow_fin"), NAME_None, 1.f, NAME_None, TravelTime);
		Travel = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(this, TEXT("Burrow"), Dir, Distance / TravelTime, TravelTime, false, nullptr,
			ERootMotionFinishVelocityMode::ClampVelocity, FVector::ZeroVector, Ch->GetCharacterMovement()->GetMaxSpeed(), true);
		if (Travel) Travel->ReadyForActivation();
	});
	After(Windup + TravelTime, [this]()
	{
		AWRCharacterBase* Ch = GetCharacter();
		SetUnder(false);
		if (Ch && IsServer())
		{
			const FVector Feet = Ch->GetActorLocation() - FVector(0, 0, Ch->GetSimpleCollisionHalfHeight());
			WRFx::Broadcast(Ch, TEXT("sun.burrow_pop"), Feet, FRotator::ZeroRotator, 1.f, TEXT("sun.pop"));
		}
	});
	After(Windup + TravelTime + Recovery, [this]() { EndSelf(); });
}

void UWRGA_Burrow::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	SetUnder(false);
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// ================================================================================================ the Wildbrands

UWRGA_SunStrike::UWRGA_SunStrike()
{
	DisplayName = TEXT("Sun strike"); PathId = TEXT("sun"); NodeId = TEXT("sun_strike");
	StaminaKey = TEXT("stamina.weaponArt"); StaminaFallback = 18.f;
	CooldownKey = TEXT("spells.sunStrike.cooldown"); CooldownFallback = 3.f;
	TimingKey = TEXT("weapon.lightTiming"); SwingKind = EWRSwingKind::WeaponArt;
	ArtKey = TEXT("spells.sunStrike");
	StanceMultiplier = 2.f;
	bAllowedMounted = true;
	RadianceCost = 15.f;
	Fx = TEXT("sun.strike");
}

bool UWRGA_SunStrike::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags) && SnCanPay(ActorInfo, WRTune(*(ArtKey + TEXT(".radianceCost")), RadianceCost));
}

void UWRGA_SunStrike::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (IsServer() && IsActive())
	{
		WRSun::SpendRadiance(GetCharacter(), WRTune(*(ArtKey + TEXT(".radianceCost")), RadianceCost));
		WRFx::BroadcastOn(GetCharacter(), Fx, TEXT("weapon"), 1.f, Fx, 0.6f);
	}
}

UWRGA_BlazingArc::UWRGA_BlazingArc()
{
	DisplayName = TEXT("Blazing arc"); NodeId = TEXT("blazing_arc");
	CooldownKey = TEXT("spells.blazingArc.cooldown"); CooldownFallback = 6.f;
	ArtKey = TEXT("spells.blazingArc");
	StanceMultiplier = 2.5f;
	RadianceCost = 25.f;
	Fx = TEXT("sun.arc");
}

UWRGA_NoonStrike::UWRGA_NoonStrike()
{
	DisplayName = TEXT("Noon strike"); NodeId = TEXT("noon_strike");
	CooldownKey = TEXT("spells.noonStrike.cooldown"); CooldownFallback = 30.f;
	TimingKey = TEXT("weapon.heavyTiming");
	ArtKey = TEXT("spells.noonStrike");
	StanceMultiplier = 6.f;
	RadianceCost = 60.f;
	Fx = TEXT("sun.noon");
}

void UWRGA_NoonStrike::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsServer() || !IsActive()) return;
	// The column falls on the struck point 0.9 s after the blow lands (the swing's wind-up and active, then the delay):
	// where the blade comes down, in front of you (or on your lock).
	AWRCharacterBase* C = GetCharacter();
	const FWRTuning& T = FWRTuning::Get();
	const float Land = T.Num(FString(TEXT("wildbrands.noon_strike.windup")), 0.8f) + T.Num(FString(TEXT("wildbrands.noon_strike.active")), 0.3f) * 0.5f;
	FVector At = C->GetActorLocation() + C->GetActorForwardVector() * T.Num(TEXT("spells.noonStrike.reach"), 300.f);
	if (const AWRCharacterBase* Lock = SnLocked(C)) At = Lock->GetActorLocation();
	At = SnGround(C, At);
	TWeakObjectPtr<AWRCharacterBase> W(C);
	FTimerHandle H;
	C->GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([W, At]()
	{
		if (AWRCharacterBase* Ch = W.Get())
		{
			const FWRTuning& Tt = FWRTuning::Get();
			AWRSunColumn::Drop(Ch, At, Tt.Num(TEXT("spells.noonStrike.columnRadius"), 300.f), Tt.Num(TEXT("spells.noonStrike.columnDelay"), 0.9f), Tt.Num(TEXT("spells.noonStrike.column"), 50.f), Tt.Num(TEXT("spells.noonStrike.columnBlind"), 80.f), Tt.Num(TEXT("spells.noonStrike.columnStance"), 20.f), TEXT("noon_strike"));
		}
	}), FMath::Max(0.05f, Land), false);
}

// ================================================================================================ AWRSunField

AWRSunField::AWRSunField()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	SetNetUpdateFrequency(10.f);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRSunField::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRSunField, Kind);
	DOREPLIFETIME(AWRSunField, Radius);
	DOREPLIFETIME(AWRSunField, EndAt);
	DOREPLIFETIME(AWRSunField, Caster);
}

TArray<AWRSunField*> AWRSunField::Of(const AActor* InCaster, FName InKind)
{
	TArray<AWRSunField*> Out;
	if (!InCaster) return Out;
	for (TActorIterator<AWRSunField> It(InCaster->GetWorld()); It; ++It) if (It->Caster == InCaster && (InKind.IsNone() || It->Kind == InKind)) Out.Add(*It);
	return Out;
}

void AWRSunField::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer) return;
	// Its look, by kind: the beacon's pool of gold light and its 10 m lamp, the storms' walls of sand, the quicksand's
	// churning skin.
	const FName Fx = Kind == TEXT("beacon") ? FName(TEXT("sun.beacon")) : Kind == TEXT("sandstorm") ? FName(TEXT("sun.sandstorm")) : Kind == TEXT("wrath") ? FName(TEXT("sun.wrath")) : FName(TEXT("sun.quicksand"));
	Look = WRFx::Attach(RootComponent, Fx, NAME_None, Radius / 500.f);
	if (Kind == TEXT("beacon"))
	{
		Light = NewObject<UPointLightComponent>(this);
		Light->SetupAttachment(RootComponent);
		Light->SetRelativeLocation(FVector(0, 0, 250.f));
		Light->SetLightColor(FLinearColor(FColor(0xFF, 0xD8, 0x80)));
		Light->SetIntensity(WRTune(TEXT("spells.beacon.lightIntensity"), 6000.f));
		Light->SetAttenuationRadius(WRTune(TEXT("spells.beacon.light"), 1000.f));
		Light->SetCastShadows(false);
		Light->RegisterComponent();
	}
}

void AWRSunField::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Caster && Kind == TEXT("beacon")) Caster->Tags.Remove(TEXT("SunBeaconLit"));
	if (Look) Look->DestroyComponent();
	Super::EndPlay(Reason);
}

void AWRSunField::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority()) return;
	if (SnNow(this) >= EndAt || !Caster)
	{
		for (const TWeakObjectPtr<AActor>& R : Rooted) if (AWRCharacterBase* B = Cast<AWRCharacterBase>(R.Get())) B->GetAbilitySystemComponent()->RemoveLooseGameplayTag(WRTags::Status_Rooted);
		Destroy();
		return;
	}
	PulseClock -= DeltaSeconds;
	if (PulseClock <= 0.f) { PulseClock = 0.25f; Pulse(); }
	// Solar storm (Lightning + Sun 50): a strike every 1.0 s on a random enemy inside your Sandstorm.
	if (Kind == TEXT("sandstorm") && Caster->HasTag(WRSun::Combo_SolarStorm))
	{
		StrikeClock -= DeltaSeconds;
		if (StrikeClock <= 0.f)
		{
			StrikeClock = FWRComboRegistry::Get().Num(TEXT("solar_storm"), TEXT("every"), 1.f);
			TArray<AWRCharacterBase*> In = SnHostilesNear(Caster, GetActorLocation(), Radius);
			if (In.Num() > 0)
			{
				AWRCharacterBase* T = In[FMath::RandRange(0, In.Num() - 1)];
				FWRHit Hit = SnBase(Caster, FWRComboRegistry::Get().Num(TEXT("solar_storm"), TEXT("damage"), 25.f), 0.f);
				Hit.Element = WRTags::Element_Lightning;
				Hit.Buildups.Add(TEXT("shock"), FWRComboRegistry::Get().Num(TEXT("solar_storm"), TEXT("shock"), 30.f));
				Hit.ImpactPoint = T->GetActorLocation();
				WRFx::Broadcast(this, TEXT("storm.bolt"), T->GetActorLocation(), FRotator::ZeroRotator, 1.f, TEXT("storm.strike"));
				UWRCombatLibrary::ApplyHit(T, Hit);
			}
		}
	}
}

void AWRSunField::Pulse()
{
	const float Dt = 0.25f;
	const FVector At = GetActorLocation();
	const FWRTuning& T = FWRTuning::Get();
	if (Kind == TEXT("beacon"))
	{
		// The light counts as a sunstone lamp for its caster while they stand in it at night.
		if (FVector::DistSquared(Caster->GetActorLocation(), At) < Radius * Radius) Caster->Tags.AddUnique(TEXT("SunBeaconLit"));
		else Caster->Tags.Remove(TEXT("SunBeaconLit"));
		for (AWRCharacterBase* Who : SnHostilesNear(Caster, At, Radius))
		{
			if (!SnDark(Who)) continue;
			FWRHit Hit = SnBase(Caster, T.Num(TEXT("spells.beacon.dps"), 10.f) * Dt, 0.f);
			Hit.bDamageOverTime = true;
			Hit.ImpactPoint = Who->GetActorLocation();
			UWRCombatLibrary::ApplyHit(Who, Hit);
		}
		return;
	}
	if (Kind == TEXT("sandstorm"))
	{
		// Grit in the eyes (blind 10 a second), the slow, Scour's 4 a second a rank.
		const float Scour = FMath::Max(0.f, UWRPathComponent::Stat(Caster, TEXT("stormDps")));
		for (AWRCharacterBase* Who : SnHostilesNear(Caster, At, Radius))
		{
			FWRHit Hit = SnBase(Caster, Scour * Dt, T.Num(TEXT("spells.sandstorm.blindPerSecond"), 10.f) * Dt);
			Hit.bDamageOverTime = true;
			Hit.ImpactPoint = Who->GetActorLocation();
			UWRCombatLibrary::ApplyHit(Who, Hit);
			if (Who->Status) Who->Status->ApplyStatus(WRTags::Status_Slowed, 0.5f);
			if (Scour > 0.f) WRFx::Broadcast(this, TEXT("sun.scour"), Who->GetActorLocation(), FRotator::ZeroRotator, 0.6f);
		}
		return;
	}
	if (Kind == TEXT("wrath"))
	{
		// The vortex: everything pulled in at 2 m/s (bosses never), 14 a second, grit in the eyes.
		const float Pull = T.Num(TEXT("spells.desertsWrath.pull"), 200.f) * Dt;
		const float Scour = FMath::Max(0.f, UWRPathComponent::Stat(Caster, TEXT("stormDps")));
		for (AWRCharacterBase* Who : SnHostilesNear(Caster, At, Radius))
		{
			FWRHit Hit = SnBase(Caster, (T.Num(TEXT("spells.desertsWrath.dps"), 14.f) + Scour) * Dt, T.Num(TEXT("spells.desertsWrath.blindPerSecond"), 10.f) * Dt);
			Hit.bDamageOverTime = true;
			Hit.ImpactPoint = Who->GetActorLocation();
			UWRCombatLibrary::ApplyHit(Who, Hit);
			const FName Tier = SnTier(Who);
			if (Tier == TEXT("boss") || Tier == TEXT("mini") || !Who->IsAlive()) continue;
			const FVector To = At - Who->GetActorLocation();
			if (To.Size2D() > 150.f) WRWind::Knock(Caster, Who, To.GetSafeNormal2D(), FMath::Min(Pull, To.Size2D() - 100.f), Dt, TEXT("deserts_wrath"));
		}
		return;
	}
	// Quicksand: the small rooted (2 s from when they stepped in), everything slowed x0.5; Frozen sand's frost.
	for (AWRCharacterBase* Who : SnHostilesNear(Caster, At, Radius))
	{
		if (!Who->Status) continue;
		Who->Status->ApplyStatus(WRTags::Status_Slowed, 0.5f);
		const FName Tier = SnTier(Who);
		if (SnSmall(Tier) && !Rooted.Contains(Who))
		{
			Rooted.Add(Who);
			Who->Status->ApplyStatus(WRTags::Status_Rooted, T.Num(TEXT("spells.quicksand.root"), 2.f));
		}
		if (Caster->HasTag(WRSun::Combo_FrozenSand) && Who->HasTag(WRTags::Status_Rooted))
		{
			FWRHit Hit = SnBase(Caster, 0.f, 0.f);
			Hit.Element = WRTags::Element_Frost;
			Hit.FrostBuildup = FWRComboRegistry::Get().Num(TEXT("frozen_sand"), TEXT("frost"), 30.f) * Dt;
			Hit.bDamageOverTime = true;
			UWRCombatLibrary::ApplyHit(Who, Hit);
		}
	}
}

// ================================================================================================ AWRSunColumn

AWRSunColumn::AWRSunColumn()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	SetNetUpdateFrequency(10.f);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	InitialLifeSpan = 6.f;
}

void AWRSunColumn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRSunColumn, Radius);
	DOREPLIFETIME(AWRSunColumn, StrikeAt);
}

AWRSunColumn* AWRSunColumn::Drop(AWRCharacterBase* InCaster, const FVector& At, float InRadius, float InDelay, float InDamage, float InBlind, float InStance, FName InNode)
{
	if (!InCaster || !InCaster->HasAuthority()) return nullptr;
	AWRSunColumn* C = InCaster->GetWorld()->SpawnActorDeferred<AWRSunColumn>(AWRSunColumn::StaticClass(), FTransform(At), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!C) return nullptr;
	C->Caster = InCaster;
	C->Radius = InRadius;
	C->StrikeAt = SnNow(InCaster) + InDelay;
	C->Damage = InDamage;
	C->Blind = InBlind;
	C->Stance = InStance;
	C->Node = InNode;
	C->FinishSpawning(FTransform(At));
	WRSound::PlayAt(InCaster, TEXT("sun.column_tell"), At);
	return C;
}

void AWRSunColumn::BeginPlay()
{
	Super::BeginPlay();
	// The tell: a gold column standing over the spot the whole time it's charging (your own ability: the party sees it).
	if (GetNetMode() != NM_DedicatedServer) WRFx::SpawnAt(this, TEXT("sun.column_tell"), GetActorLocation(), FRotator::ZeroRotator, Radius / 600.f);
}

void AWRSunColumn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || bStruck || SnNow(this) < StrikeAt) return;
	bStruck = true;
	AWRCharacterBase* C = Caster.Get();
	WRFx::Broadcast(this, TEXT("sun.column"), GetActorLocation(), FRotator::ZeroRotator, Radius / 600.f, TEXT("sun.column"));
	if (!C) return;
	// Path raiding (cookpass-building 3.7): the column of sun scorches the buildings it falls on.
	FWRRaiding::SplashPieces(GetWorld(), GetActorLocation(), Radius, Damage, EWRRaidSource::Spell, C, FWRRaiding::Responsible(C), TEXT("sun"));
	for (AWRCharacterBase* Who : SnHostilesNear(C, GetActorLocation(), Radius))
	{
		FWRHit Hit = SnBase(C, Damage, Blind);
		Hit.StanceDamage = Stance;
		Hit.ImpactPoint = Who->GetActorLocation();
		Hit.Origin = GetActorLocation();
		UWRCombatLibrary::ApplyHit(Who, Hit);
	}
}
