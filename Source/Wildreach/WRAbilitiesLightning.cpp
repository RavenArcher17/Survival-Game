#include "WRAbilitiesLightning.h"
#include "WRRaiding.h"
#include "Wildreach.h"
#include "WRAttributeSet.h"
#include "WRCharacter.h"
#include "WRCharacterBase.h"
#include "WRCombatLibrary.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRLightningPath.h"
#include "WRBloodPath.h"
#include "WRPathComponent.h"
#include "WRPlayerController.h"
#include "WRProjectile.h"
#include "WRStatusComponent.h"
#include "WRSwampBosses.h"
#include "WRTuning.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"

// ---------------------------------------------------------------- aim helpers (the same rules as WRAbilities' spells)

namespace WRLightningAim
{
	static const FLinearColor Violet = FLinearColor(FColor(0xB9, 0xA7, 0xFF));

	/** What the caster has locked on to, if a spell can still go for it. */
	static AWRCharacterBase* Locked(const AWRCharacterBase* Caster)
	{
		const AWRPlayerController* PC = Caster ? Cast<AWRPlayerController>(Caster->GetController()) : nullptr;
		AWRCharacterBase* Target = PC ? PC->GetServerLock() : nullptr;
		return Target && Target->IsAlive() && !Target->IsHidden() && Caster->IsHostileTo(Target)
			&& FVector::Dist(Target->GetActorLocation(), Caster->GetActorLocation()) <= WRTune(TEXT("spells.lockRange"), 4000.f) ? Target : nullptr;
	}

	/** From the caster's chest through the point under the crosshair. */
	static FVector ThroughCrosshair(const AWRCharacterBase* Caster, const FVector& From)
	{
		const FVector View = Caster->GetBaseAimRotation().Vector();
		const AWRCharacter* Survivor = Cast<AWRCharacter>(Caster);
		FVector Point;
		if (!Survivor || !Survivor->GetController() || !Survivor->CrosshairPoint(20000.f, true, Point))
		{
			return View;
		}
		const FVector Dir = (Point - From).GetSafeNormal();
		return Dir.IsNearlyZero() || FVector::DistSquared(Point, From) < FMath::Square(100.f) || FVector::DotProduct(Dir, View) < 0.3f ? View : Dir;
	}

	static FVector Ground(const AWRCharacterBase* Caster, const FVector& At)
	{
		FHitResult Hit;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(WRLightningGround), false, Caster);
		return Caster->GetWorld()->LineTraceSingleByChannel(Hit, At + FVector(0, 0, 500), At - FVector(0, 0, 2000), ECC_WorldStatic, Query) ? FVector(Hit.ImpactPoint) : At;
	}

	static bool ClearLine(const AWRCharacterBase* Caster, const AWRCharacterBase* Target)
	{
		FHitResult Hit;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(WRLightningSight), false, Caster);
		Query.AddIgnoredActor(Target);
		return !Caster->GetWorld()->LineTraceSingleByChannel(Hit, Caster->GetActorLocation() + FVector(0, 0, 50), Target->GetActorLocation(), ECC_WorldStatic, Query);
	}
}

// ---------------------------------------------------------------- the spell base

UWRGA_LightningSpell::UWRGA_LightningSpell()
{
	StaminaKey = TEXT("stamina.spell");
	StaminaFallback = 12.f;
	PathId = TEXT("lightning");
	Element = WRTags::Element_Lightning;
	ActivationOwnedTags.AddTag(WRTags::State_Attacking);
	FGameplayTagContainer Blocked = ActivationBlockedTags;
	Blocked.AddTag(WRTags::State_Attacking);
	Blocked.AddTag(WRTags::State_Dodging);
	Blocked.AddTag(WRLightning::State_BoltDash);
	ActivationBlockedTags = Blocked;
}

float UWRGA_LightningSpell::Spell(const TCHAR* Field, float Fallback) const
{
	return FWRTuning::Get().Num(SpellKey + TEXT(".") + Field, Fallback);
}

FVector UWRGA_LightningSpell::AimedGround(AWRCharacterBase* Caster, float Range) const
{
	const FVector Feet = Caster->GetActorLocation() - FVector(0.f, 0.f, Caster->GetSimpleCollisionHalfHeight());
	if (const AWRCharacterBase* Lock = WRLightningAim::Locked(Caster))
	{
		const FVector Off = Lock->GetActorLocation() - Feet;
		return WRLightningAim::Ground(Caster, FVector(Feet.X, Feet.Y, Lock->GetActorLocation().Z) + FVector(Off.X, Off.Y, 0.f).GetClampedToMaxSize(Range));
	}
	const AWRCharacter* Survivor = Cast<AWRCharacter>(Caster);
	FVector Point;
	if (Survivor && Survivor->CrosshairPoint(Range, false, Point))
	{
		return Point;
	}
	const FVector Flat = FRotator(0.f, Caster->GetBaseAimRotation().Yaw, 0.f).Vector();
	return WRLightningAim::Ground(Caster, Feet + Flat * Range * 0.7f);
}

void UWRGA_LightningSpell::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
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
	// The shape (combat.md 2.13, tuning spells.shapes.<shape>): its wind-up (a spell's own windup over it), rooted or
	// walking at half speed, its recovery. The creatures that answer a cast read State.Casting.
	const FWRTuning& T = FWRTuning::Get();
	const FString ShapeRow = TEXT("spells.shapes.") + ShapeName;
	const float Windup = Spell(TEXT("windup"), T.Num(ShapeRow + TEXT(".windup"), 0.3f));
	const float Recovery = Spell(TEXT("recovery"), T.Num(ShapeRow + TEXT(".recovery"), 0.35f));
	if (T.Num(ShapeRow + TEXT(".roots"), 0.f) > 0.f) SetMoveRoot(true);
	else SetMoveSlow(true, true);
	Caster->PlaySwing(EWRSwingKind::Cast, Windup + 0.1f);
	Caster->SetCombatTag(WRTags::State_Casting, true);
	if (IsServer())
	{
		WRFx::BroadcastOn(Caster, TEXT("cast.lightning"), TEXT("hand"), 1.f, CastSound, Windup + 0.1f);
	}
	After(Windup, [this]()
	{
		AWRCharacterBase* C = GetCharacter();
		if (!C)
		{
			return;
		}
		C->SetCombatTag(WRTags::State_Casting, false);
		if (IsServer() && C->IsAlive())
		{
			CastLightning(C);
			// Charge fills as you cast (spells.<key>.charge: Spark +8, Chain lightning +12).
			WRLightning::GainCharge(C, Spell(TEXT("charge"), 0.f));
		}
	});
	After(Windup + Recovery, [this]() { EndSelf(); });
}

// ---------------------------------------------------------------- Spark

UWRGA_Spark::UWRGA_Spark()
{
	DisplayName = TEXT("Spark"); NodeId = TEXT("spark");
	SpellKey = TEXT("spells.spark"); CooldownKey = TEXT("spells.spark.cooldown"); CooldownFallback = 1.f;
	ShapeName = TEXT("bolt"); CastSound = TEXT("storm.spark");
	bAllowedMounted = true;  // bolts fly from the saddle (combat.md 2.19)
}

void UWRGA_Spark::CastLightning(AWRCharacterBase* Caster)
{
	const float Speed = Spell(TEXT("speed"), 3200.f);
	AWRCharacterBase* Lock = WRLightningAim::Locked(Caster);
	const FVector Chest = Caster->GetActorLocation() + FVector(0.f, 0.f, 40.f);
	FVector Dir;
	if (Lock)
	{
		const FVector Velocity = WRTune(TEXT("spells.lockLead"), 1.f) > 0.f ? Lock->GetVelocity() : FVector::ZeroVector;
		Dir = (UWRCombatLibrary::LeadPoint(Chest, Lock->GetActorLocation(), Velocity, Speed) - Chest).GetSafeNormal();
	}
	else
	{
		Dir = WRLightningAim::ThroughCrosshair(Caster, Chest);
	}
	const FVector SpawnAt = Chest + Dir * 70.f;

	FWRHit Hit;
	Hit.Instigator = Caster;
	Hit.Damage = Spell(TEXT("damage"), 16.f);
	Hit.PoiseDamage = Spell(TEXT("poise"), 8.f);
	Hit.Element = WRTags::Element_Lightning;
	Hit.Buildups.Add(TEXT("shock"), Spell(TEXT("shockBuildup"), 35.f));
	Hit.PathId = PathId;
	Hit.bSpell = true;

	// Point blank: a body between the chest and the muzzle would be spawned inside, so it takes the bolt at once.
	FHitResult Near;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(WRSparkMuzzle), false, Caster);
	if (Caster->GetWorld()->SweepSingleByChannel(Near, Chest, SpawnAt, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(14.f), Query))
	{
		AWRCharacterBase* Blocker = Cast<AWRCharacterBase>(Near.GetActor());
		if (Blocker && Caster->IsHostileTo(Blocker))
		{
			Hit.Origin = Chest;
			UWRCombatLibrary::ApplyHit(Blocker, Hit);
			return;
		}
	}
	FActorSpawnParameters Params;
	Params.Owner = Caster;
	Params.Instigator = Caster;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AWRProjectile* Bolt = Caster->GetWorld()->SpawnActor<AWRProjectile>(AWRProjectile::StaticClass(), SpawnAt, Dir.Rotation(), Params))
	{
		Bolt->Init(Hit, Speed, WRLightningAim::Violet);
		if (Lock)
		{
			// The fire-frost RAISE 3 rule (spells.spark.lockHomingPlayers 0): it curves after a creature, never at a player.
			const float Homing = Lock->IsCreature() ? WRTune(TEXT("spells.lockHoming"), 6000.f) : Spell(TEXT("lockHomingPlayers"), 0.f);
			if (Homing > 0.f)
			{
				Bolt->HomeOn(Lock, Homing);
			}
		}
	}
}

// ---------------------------------------------------------------- Chain lightning

UWRGA_ChainLightning::UWRGA_ChainLightning()
{
	DisplayName = TEXT("Chain lightning"); NodeId = TEXT("chain_lightning");
	SpellKey = TEXT("spells.chainLightning"); CooldownKey = TEXT("spells.chainLightning.cooldown"); CooldownFallback = 6.f;
	ShapeName = TEXT("chain"); CastSound = TEXT("storm.spark");
	bAllowedMounted = true;
}

void UWRGA_ChainLightning::CastLightning(AWRCharacterBase* Caster)
{
	// The first target: the lock, else the enemy nearest the crosshair inside the cone and range, in sight.
	const float Range = Spell(TEXT("range"), 1500.f);
	const float CosCone = FMath::Cos(FMath::DegreesToRadians(Spell(TEXT("cone"), 30.f) * 0.5f));
	const FVector Eye = Caster->GetActorLocation() + FVector(0.f, 0.f, 50.f);
	const FVector Aim = Caster->GetBaseAimRotation().Vector();
	AWRCharacterBase* First = WRLightningAim::Locked(Caster);
	if (First && FVector::Dist(First->GetActorLocation(), Caster->GetActorLocation()) > Range + First->GetSimpleCollisionRadius())
	{
		First = nullptr;
	}
	if (!First)
	{
		float Best = -1.f;
		for (TActorIterator<AWRCharacterBase> It(Caster->GetWorld()); It; ++It)
		{
			AWRCharacterBase* Who = *It;
			if (Who == Caster || !Who->IsAlive() || Who->IsHidden() || !Caster->IsHostileTo(Who))
			{
				continue;
			}
			const FVector To = Who->GetActorLocation() - Eye;
			const float Dist = To.Size();
			const float Dot = FVector::DotProduct(To.GetSafeNormal(), Aim);
			if (Dist > Range + Who->GetSimpleCollisionRadius() || Dot < CosCone)
			{
				continue;
			}
			// Closest to the crosshair first, then the nearer.
			const float Score = Dot - Dist / (Range * 10.f);
			if (Score > Best && WRLightningAim::ClearLine(Caster, Who))
			{
				Best = Score;
				First = Who;
			}
		}
	}
	const FVector Hand = Caster->GetActorLocation() + FVector(0.f, 0.f, 40.f) + Caster->GetActorForwardVector() * 40.f;
	if (!First)
	{
		// Nothing to jump to: the bolt cracks out along the aim and dies.
		WRLightning::Ribbon(Caster, Hand, Hand + Aim * FMath::Min(Range, 900.f), true);
		return;
	}
	const float Damage = Spell(TEXT("damage"), 20.f), Shock = Spell(TEXT("shockBuildup"), 30.f);
	// Red lightning (Lightning + Blood combo): every body this cast's chain hits may bleed (WRBloodPath.cpp).
	const WRBlood::FChainScope RedLightning(Caster);
	WRLightning::Ribbon(Caster, Hand, First->GetActorLocation() + FVector(0.f, 0.f, 30.f), true);
	WRLightning::Zap(Caster, First, Damage, Shock, Caster->GetActorLocation(), true, false);
	// Then it jumps: spells.chainLightning.jumps + one a rank of Arc, to the nearest enemy it hasn't hit within jumpRadius.
	const int32 Jumps = FMath::RoundToInt(Spell(TEXT("jumps"), 2.f) + UWRPathComponent::Stat(Caster, TEXT("extraJumps")));
	TSet<AWRCharacterBase*> Already;
	WRLightning::Chain(Caster, First, Jumps, Spell(TEXT("jumpRadius"), 800.f), Damage * Spell(TEXT("jumpDamage"), 1.f), Shock, Already, false);
}

// ---------------------------------------------------------------- Thunderclap

UWRGA_Thunderclap::UWRGA_Thunderclap()
{
	DisplayName = TEXT("Thunderclap"); NodeId = TEXT("thunderclap");
	SpellKey = TEXT("spells.thunderclap"); CooldownKey = TEXT("spells.thunderclap.cooldown"); CooldownFallback = 10.f;
	ShapeName = TEXT("nova"); CastSound = TEXT("storm.charge_whine");
}

void UWRGA_Thunderclap::CastLightning(AWRCharacterBase* Caster)
{
	const float Radius = Spell(TEXT("radius"), 450.f);
	const FVector Feet = Caster->GetActorLocation() - FVector(0.f, 0.f, Caster->GetSimpleCollisionHalfHeight());
	WRFx::Broadcast(Caster, TEXT("lightning.thunderclap"), Feet, FRotator::ZeroRotator, Radius / 100.f, TEXT("storm.strike"));
	WRFx::Shake(Caster, 0.3f, Feet, Radius * 3.f);
	const float Damage = Spell(TEXT("damage"), 18.f);
	// Path raiding (cookpass-building 3.7): the clap cracks the buildings round you too (lightning x1.5 on metal).
	FWRRaiding::SplashPieces(Caster->GetWorld(), Feet, Radius, Damage, EWRRaidSource::Spell, Caster, FWRRaiding::Responsible(Caster), PathId);
	TArray<AWRCharacterBase*> Caught;
	for (TActorIterator<AWRCharacterBase> It(Caster->GetWorld()); It; ++It)
	{
		if (*It != Caster && It->IsAlive() && Caster->IsHostileTo(*It) && FVector::Dist(It->GetActorLocation(), Caster->GetActorLocation()) < Radius + It->GetSimpleCollisionRadius())
		{
			Caught.Add(*It);
		}
	}
	for (AWRCharacterBase* Who : Caught)
	{
		FWRHit Hit;
		Hit.Instigator = Caster;
		Hit.Damage = Damage;
		Hit.PoiseDamage = Spell(TEXT("poise"), 40.f);
		Hit.Element = WRTags::Element_Lightning;
		Hit.Buildups.Add(TEXT("shock"), Spell(TEXT("shockBuildup"), 60.f));
		// Stance x2 (combat.md 2.13): twice a spell's stance for its damage.
		Hit.StanceDamage = Damage * WRTune(TEXT("stance.spellPerDamage"), 0.3f) * Spell(TEXT("stance"), 2.f);
		Hit.PathId = PathId;
		Hit.bSpell = true;
		Hit.Impact = 2;
		Hit.Tier = EWRHitTier::Heavy;
		Hit.Origin = Caster->GetActorLocation();
		Hit.ImpactPoint = Who->GetActorLocation();
		UWRCombatLibrary::ApplyHit(Who, Hit);
	}
}

// ---------------------------------------------------------------- Static field

UWRGA_StaticField::UWRGA_StaticField()
{
	DisplayName = TEXT("Static field"); NodeId = TEXT("static_field");
	SpellKey = TEXT("spells.staticField"); CooldownKey = TEXT("spells.staticField.cooldown"); CooldownFallback = 14.f;
	ShapeName = TEXT("pool"); CastSound = TEXT("storm.charge_whine");
}

void UWRGA_StaticField::CastLightning(AWRCharacterBase* Caster)
{
	// Field strength: +10% radius and +1.5 s a rank (nodeEffects lightning.field_strength).
	const float Radius = Spell(TEXT("radius"), 400.f) * (1.f + UWRPathComponent::Stat(Caster, TEXT("fieldRadius")));
	const float Seconds = Spell(TEXT("seconds"), 8.f) + UWRPathComponent::Stat(Caster, TEXT("fieldSeconds"));
	const FVector At = AimedGround(Caster, Spell(TEXT("range"), 1500.f));
	AWRLightningField::SpawnField(Caster, At, Radius, Seconds, Spell(TEXT("dps"), 6.f), Spell(TEXT("shockPerSecond"), 15.f));
	WRFx::Broadcast(Caster, TEXT("lightning.field"), At, FRotator::ZeroRotator, Radius / 100.f, TEXT("storm.arc"), Seconds);
}

// ---------------------------------------------------------------- Tempest

UWRGA_Tempest::UWRGA_Tempest()
{
	DisplayName = TEXT("Tempest"); NodeId = TEXT("tempest");
	SpellKey = TEXT("spells.tempest"); CooldownKey = TEXT("spells.tempest.cooldown"); CooldownFallback = 45.f;
	ShapeName = TEXT("storm"); CastSound = TEXT("storm.charge_whine");
}

void UWRGA_Tempest::CastLightning(AWRCharacterBase* Caster)
{
	const float Radius = Spell(TEXT("radius"), 900.f), Seconds = Spell(TEXT("seconds"), 10.f);
	AWRLightningField::SpawnTempest(Caster, Radius, Seconds, Spell(TEXT("every"), 0.8f), Spell(TEXT("damage"), 30.f), Spell(TEXT("shockBuildup"), 40.f),
		Spell(TEXT("tell"), 1.f), Spell(TEXT("strikeRadius"), 200.f));
	WRFx::BroadcastOn(Caster, TEXT("lightning.tempest"), NAME_None, Radius / 100.f, TEXT("storm.strike"), Seconds);
}

// ---------------------------------------------------------------- Thunder god

UWRGA_ThunderGod::UWRGA_ThunderGod()
{
	DisplayName = TEXT("Thunder god"); NodeId = TEXT("thunder_god");
	SpellKey = TEXT("spells.thunderGod"); CooldownKey = TEXT("spells.thunderGod.cooldown"); CooldownFallback = 60.f;
	ShapeName = TEXT("buff"); CastSound = TEXT("storm.charge_whine");
}

void UWRGA_ThunderGod::CastLightning(AWRCharacterBase* Caster)
{
	const float Seconds = Spell(TEXT("seconds"), 12.f);
	Caster->Status->ApplyStatus(WRLightning::Status_ThunderGod, Seconds);
	WRFx::BroadcastOn(Caster, TEXT("lightning.thundergod"), NAME_None, 1.f, TEXT("storm.strike"), Seconds);
	UE_LOG(LogWildreach, Log, TEXT("LIGHTNING thunder god on %s for %.0f s"), *Caster->GetName(), Seconds);
}

// ---------------------------------------------------------------- Wildbrands

UWRGA_StaticStrike::UWRGA_StaticStrike()
{
	DisplayName = TEXT("Static strike"); PathId = TEXT("lightning"); NodeId = TEXT("static_strike");
	StaminaKey = TEXT("stamina.weaponArt"); StaminaFallback = 18.f;
	CooldownKey = TEXT("spells.staticStrike.cooldown"); CooldownFallback = 2.5f;
	TimingKey = TEXT("weapon.lightTiming"); SwingKind = EWRSwingKind::WeaponArt;
	Element = WRTags::Element_Lightning; ArtKey = TEXT("spells.staticStrike");
	StanceMultiplier = 1.5f;  // combat.md 2.12
	ActivationOwnedTags.AddTag(WRLightning::Ability_StaticStrike);
}

void UWRGA_StaticStrike::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	// Charge is a builder (combat.md 2.12): the Wildbrand adds to it (wildbrands.static_strike.charge).
	if (IsServer() && IsActive())
	{
		WRLightning::GainCharge(GetCharacter(), WRTune(TEXT("wildbrands.static_strike.chargeGain"), 15.f));
		WRFx::BroadcastOn(GetCharacter(), TEXT("lightning.weapon"), TEXT("weapon"), 1.f, TEXT("storm.spark"), 0.5f);
	}
}

UWRGA_ChargedWeapon::UWRGA_ChargedWeapon()
{
	DisplayName = TEXT("Charged weapon"); PathId = TEXT("lightning"); NodeId = TEXT("charged_weapon");
	StaminaKey = TEXT("stamina.weaponArt"); StaminaFallback = 18.f;
	CooldownKey = TEXT("spells.chargedWeapon.cooldown"); CooldownFallback = 8.f;
	TimingKey = TEXT("weapon.heavyTiming"); SwingKind = EWRSwingKind::WeaponArt; bHeavy = true;
	Element = WRTags::Element_Lightning; ArtKey = TEXT("spells.chargedWeapon");
	StanceMultiplier = 3.f;
	ActivationOwnedTags.AddTag(WRLightning::Ability_ChargedWeapon);
}

bool UWRGA_ChargedWeapon::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	// It spends all Charge, and needs some to spend (wildbrands.charged_weapon.minCharge).
	const AActor* Who = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	return WRLightning::ChargeValue(Who) >= WRTune(TEXT("wildbrands.charged_weapon.minCharge"), 30.f);
}

void UWRGA_ChargedWeapon::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	bReleased = false;
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive() || !IsServer())
	{
		return;
	}
	WRLightning::SetStoredCharge(GetCharacter(), 0.f);
	WRFx::BroadcastOn(GetCharacter(), TEXT("lightning.weapon"), TEXT("weapon"), 1.3f, TEXT("storm.charge_whine"), 1.2f);
	HoldTick();
}

void UWRGA_ChargedWeapon::HoldTick()
{
	AWRCharacterBase* C = GetCharacter();
	if (!C || !IsActive() || bReleased)
	{
		return;
	}
	if (C->HasTag(WRTags::State_Charging))
	{
		// Held: it stores more first (+20 Charge a second into the bar it's about to release).
		const float Step = 0.05f;
		if (UWRAttributeSet* A = C->GetAttributes())
		{
			C->GetAbilitySystemComponent()->SetNumericAttributeBase(UWRAttributeSet::GetChargeAttribute(),
				A->GetCharge() + WRTune(TEXT("wildbrands.charged_weapon.holdChargePerSecond"), 20.f) * Step);
		}
		After(Step, [this]() { HoldTick(); });
		return;
	}
	Release();
}

void UWRGA_ChargedWeapon::Release()
{
	AWRCharacterBase* C = GetCharacter();
	if (!C || bReleased)
	{
		return;
	}
	bReleased = true;
	// The blow carries the whole bar: its share of a full bar scales the damage and the shock (WRLightning::DamageScale,
	// AfterHit); the bar empties.
	const float Pct = WRLightning::ChargeFraction(C);
	WRLightning::SpendCharge(C, WRLightning::ChargeValue(C));
	WRLightning::SetStoredCharge(C, Pct);
	UE_LOG(LogWildreach, Log, TEXT("LIGHTNING charged weapon by %s releases %.0f%%"), *C->GetName(), Pct * 100.f);
}

// ---------------------------------------------------------------- Bolt dash, Lightning step

UWRGA_BoltDash::UWRGA_BoltDash()
{
	DisplayName = TEXT("Bolt dash"); PathId = TEXT("lightning"); NodeId = TEXT("bolt_dash");
	StaminaKey = TEXT("stamina.spell"); StaminaFallback = 12.f;
	CooldownKey = TEXT("spells.boltDash.cooldown"); CooldownFallback = 6.f;
	ActivationOwnedTags.AddTag(WRLightning::State_BoltDash);
	FGameplayTagContainer Blocked = ActivationBlockedTags;
	Blocked.AddTag(WRTags::State_Dodging);
	Blocked.AddTag(WRLightning::State_BoltDash);
	ActivationBlockedTags = Blocked;
	// Out of a swing's cancel window it cuts the swing, as a roll does, and drops a guard.
	CancelAbilitiesWithTag.AddTag(WRTags::Ability_Melee);
	CancelAbilitiesWithTag.AddTag(WRTags::Ability_Block);
	bRules = true;
	RuleAct = WRCombatRules::EAct::Dodge;
}

void UWRGA_BoltDash::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!GetCharacter() || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	Dash(true);
}

void UWRGA_BoltDash::Dash(bool bSpendCharge)
{
	AWRCharacterBase* Character = GetCharacter();
	const FWRTuning& T = FWRTuning::Get();
	// Toward the stick (the same on the server and the owner: the acceleration is in the saved moves), else the aim.
	FVector Dir = Character->GetCharacterMovement()->GetCurrentAcceleration().GetSafeNormal2D();
	if (Dir.IsNearlyZero())
	{
		Dir = FRotator(0.f, Character->GetControlRotation().Yaw, 0.f).Vector();
	}
	DashDir = Dir;
	const float Windup = T.Num(Key + TEXT(".windup"), T.Num(TEXT("spells.shapes.dash.windup"), 0.08f));
	const float TravelTime = FMath::Max(0.05f, T.Num(Key + TEXT(".travel"), T.Num(TEXT("spells.shapes.dash.travel"), 0.18f)));
	const float Recovery = T.Num(Key + TEXT(".recovery"), T.Num(TEXT("spells.shapes.dash.recovery"), 0.25f));
	const float Distance = T.Num(Key + TEXT(".distance"), 600.f);
	Character->SetActorRotation(Dir.Rotation());
	Character->PlaySwing(EWRSwingKind::Cast, Windup + TravelTime);
	if (IsServer())
	{
		if (bSpendCharge)
		{
			WRLightning::SpendCharge(Character, T.Num(Key + TEXT(".chargeCost"), 20.f));
		}
		WRFx::BroadcastOn(Character, TEXT("lightning.dash"), NAME_None, 1.f, TEXT("storm.arc"), Windup + TravelTime);
	}
	After(Windup, [this, Distance, TravelTime]()
	{
		AWRCharacterBase* C = GetCharacter();
		if (!C)
		{
			return;
		}
		// Through bodies, untouchable, along the dash (combat.md 2.13 dash shape: i-frames the whole travel).
		DashFrom = C->GetActorLocation();
		SetPassThrough(true);
		SetIFrames(true);
		Travel = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
			this, TEXT("BoltDash"), DashDir, Distance / TravelTime, TravelTime, false, nullptr,
			ERootMotionFinishVelocityMode::ClampVelocity, FVector::ZeroVector, C->GetCharacterMovement()->GetMaxSpeed(), true);
		if (Travel)
		{
			Travel->ReadyForActivation();
		}
	});
	After(Windup + TravelTime, [this]()
	{
		AWRCharacterBase* C = GetCharacter();
		SetIFrames(false);
		SetPassThrough(false);
		if (C && IsServer())
		{
			Strike(DashFrom, C->GetActorLocation());
		}
	});
	After(Windup + TravelTime + Recovery, [this]() { EndSelf(); });
}

void UWRGA_BoltDash::Strike(const FVector& From, const FVector& To)
{
	AWRCharacterBase* C = GetCharacter();
	if (!C)
	{
		return;
	}
	const FWRTuning& T = FWRTuning::Get();
	WRLightning::Ribbon(C, From + FVector(0.f, 0.f, 20.f), To + FVector(0.f, 0.f, 20.f), false);
	// Everyone the line passed through: 12 damage and shock 40 each.
	const float Width = C->GetSimpleCollisionRadius() + T.Num(Key + TEXT(".width"), 60.f);
	TArray<AWRCharacterBase*> Passed;
	for (TActorIterator<AWRCharacterBase> It(C->GetWorld()); It; ++It)
	{
		AWRCharacterBase* Who = *It;
		if (Who == C || !Who->IsAlive() || !C->IsHostileTo(Who))
		{
			continue;
		}
		const FVector P = FMath::ClosestPointOnSegment(Who->GetActorLocation(), From, To);
		if (FVector::Dist2D(P, Who->GetActorLocation()) <= Width + Who->GetSimpleCollisionRadius() && FMath::Abs(P.Z - Who->GetActorLocation().Z) <= Who->GetSimpleCollisionHalfHeight() + 100.f)
		{
			Passed.Add(Who);
		}
	}
	for (AWRCharacterBase* Who : Passed)
	{
		WRLightning::Zap(C, Who, T.Num(Key + TEXT(".damage"), 12.f), T.Num(Key + TEXT(".shockBuildup"), 40.f), From, true, false);
	}
}

void UWRGA_BoltDash::SetPassThrough(bool bOn)
{
	AWRCharacterBase* C = GetCharacter();
	UCapsuleComponent* Capsule = C ? C->GetCapsuleComponent() : nullptr;
	if (!Capsule || bPassing == bOn)
	{
		return;
	}
	bPassing = bOn;
	if (bOn)
	{
		WasPawnResponse = Capsule->GetCollisionResponseToChannel(ECC_Pawn);
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	}
	else
	{
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, WasPawnResponse);
	}
}

void UWRGA_BoltDash::SetIFrames(bool bOn)
{
	AWRCharacterBase* C = GetCharacter();
	if (!C || !IsServer() || bIFrames == bOn)
	{
		return;
	}
	bIFrames = bOn;
	if (bOn) C->GetAbilitySystemComponent()->AddLooseGameplayTag(WRTags::State_Invulnerable);
	else C->GetAbilitySystemComponent()->RemoveLooseGameplayTag(WRTags::State_Invulnerable);
}

void UWRGA_BoltDash::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	SetIFrames(false);
	SetPassThrough(false);
	Travel = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

UWRGA_LightningStep::UWRGA_LightningStep()
{
	DisplayName = TEXT("Lightning step"); NodeId = TEXT("lightning_step");
	CooldownKey = TEXT("spells.lightningStep.cooldown"); CooldownFallback = 12.f;
	Key = TEXT("spells.lightningStep");
}

bool UWRGA_LightningStep::InSet() const
{
	const UWorld* World = GetWorld();
	return World && StepsUsed > 0 && StepsUsed < FMath::RoundToInt(WRTune(TEXT("spells.lightningStep.dashes"), 3.f))
		&& World->GetTimeSeconds() - SetStartedAt <= WRTune(TEXT("spells.lightningStep.window"), 2.f);
}

bool UWRGA_LightningStep::CheckCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	// The second and third dash of a set aren't held back by the set's cooldown.
	return InSet() || Super::CheckCooldown(Handle, ActorInfo, OptionalRelevantTags);
}

void UWRGA_LightningStep::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	// One cooldown for the set, from its first dash.
	if (!InSet())
	{
		Super::ApplyCooldown(Handle, ActorInfo, ActivationInfo);
	}
}

void UWRGA_LightningStep::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	const bool bFollowOn = InSet();
	if (!GetCharacter() || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (!bFollowOn)
	{
		SetStartedAt = GetWorld()->GetTimeSeconds();
		StepsUsed = 0;
	}
	++StepsUsed;
	// Only the first dash of a set spends Charge.
	Dash(!bFollowOn);
}

// ---------------------------------------------------------------- the field / storm actor

AWRLightningField::AWRLightningField()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

AWRLightningField* AWRLightningField::SpawnField(AWRCharacterBase* InCaster, const FVector& At, float InRadius, float Seconds, float InDps, float InShock)
{
	if (!InCaster || !InCaster->HasAuthority())
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWRLightningField* F = InCaster->GetWorld()->SpawnActor<AWRLightningField>(AWRLightningField::StaticClass(), At, FRotator::ZeroRotator, Params);
	if (F)
	{
		F->Caster = InCaster;
		F->Radius = InRadius;
		F->Dps = InDps;
		F->ShockPerSecond = InShock;
		F->SetLifeSpan(FMath::Max(0.1f, Seconds));
	}
	return F;
}

AWRLightningField* AWRLightningField::SpawnTempest(AWRCharacterBase* InCaster, float InRadius, float Seconds, float InEvery, float InDamage, float InShock, float InTell, float InStrikeRadius)
{
	if (!InCaster || !InCaster->HasAuthority())
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWRLightningField* F = InCaster->GetWorld()->SpawnActor<AWRLightningField>(AWRLightningField::StaticClass(), InCaster->GetActorLocation(), FRotator::ZeroRotator, Params);
	if (F)
	{
		F->Caster = InCaster;
		F->bTempest = true;
		F->Radius = InRadius;
		F->Every = FMath::Max(0.1f, InEvery);
		F->StrikeDamage = InDamage;
		F->StrikeShock = InShock;
		F->Tell = InTell;
		F->StrikeRadius = InStrikeRadius;
		F->Clock = F->Every * 0.5f;   // the first strike comes soon after the cast
		F->SetLifeSpan(FMath::Max(0.1f, Seconds));
	}
	return F;
}

void AWRLightningField::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AWRCharacterBase* By = Caster.Get();
	if (!HasAuthority())
	{
		return;
	}
	if (!By || !By->IsAlive())
	{
		Destroy();
		return;
	}
	if (bTempest)
	{
		// Round you: it moves with you, and strikes a random enemy inside every Every s (each with its ground glow).
		SetActorLocation(By->GetActorLocation());
		Clock += DeltaSeconds;
		if (Clock < Every)
		{
			return;
		}
		Clock -= Every;
		TArray<AWRCharacterBase*> Inside;
		for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
		{
			if (*It != By && It->IsAlive() && !It->IsHidden() && By->IsHostileTo(*It) && FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) < Radius)
			{
				Inside.Add(*It);
			}
		}
		if (Inside.Num() > 0)
		{
			AWRCharacterBase* Pick = Inside[FMath::RandRange(0, Inside.Num() - 1)];
			WRLightning::CallBolt(By, Pick->GetActorLocation() - FVector(0.f, 0.f, Pick->GetSimpleCollisionHalfHeight()), StrikeDamage, StrikeShock, Tell, StrikeRadius);
		}
		return;
	}
	// Laid in the Mother's Throat it runs into the water, a second's worth at a time (the rods, Vharra: WRSwampVharra.cpp).
	Clock += DeltaSeconds;
	if (Clock >= 1.f)
	{
		Clock -= 1.f;
		FWRSwampBoss::LightningAt(GetWorld(), GetActorLocation(), Dps, By, false);
	}
	// Static field: what stands in it takes its damage and shock every frame (damage over time: no guard, no hit-stop).
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		AWRCharacterBase* Who = *It;
		if (Who == By || !Who->IsAlive() || !By->IsHostileTo(Who) || Who->GetAttachParentActor())
		{
			continue;
		}
		const FVector Feet = Who->GetActorLocation() - FVector(0.f, 0.f, Who->GetSimpleCollisionHalfHeight());
		if (FVector::Dist2D(Feet, GetActorLocation()) > Radius + Who->GetSimpleCollisionRadius() || FMath::Abs(Feet.Z - GetActorLocation().Z) > 300.f)
		{
			continue;
		}
		FWRHit Hit;
		Hit.Instigator = By;
		Hit.Damage = Dps * DeltaSeconds;
		Hit.Element = WRTags::Element_Lightning;
		Hit.Buildups.Add(TEXT("shock"), ShockPerSecond * DeltaSeconds);
		Hit.PathId = TEXT("lightning");
		Hit.bDamageOverTime = true;
		Hit.bSpell = true;
		Hit.Origin = GetActorLocation();
		UWRCombatLibrary::ApplyHit(Who, Hit);
	}
}
