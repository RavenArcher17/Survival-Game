#include "WRAbilitiesNature.h"
#include "WRGraveBosses.h"
#include "Wildreach.h"
#include "WRAttributeSet.h"
#include "WRCharacter.h"
#include "WRCharacterBase.h"
#include "WRCombatLibrary.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRNaturePath.h"
#include "WRPathComponent.h"
#include "WRPlantSummon.h"
#include "WRPlayerController.h"
#include "WRStatusComponent.h"
#include "WRTuning.h"
#include "WRWeapons.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"

// ---------------------------------------------------------------- aim helpers (the same rules as WRAbilities' spells)

namespace WRNatureAim
{
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
		FCollisionQueryParams Query(SCENE_QUERY_STAT(WRNatureGround), false, Caster);
		return Caster->GetWorld()->LineTraceSingleByChannel(Hit, At + FVector(0, 0, 500), At - FVector(0, 0, 2000), ECC_WorldStatic, Query) ? FVector(Hit.ImpactPoint) : At;
	}

	static bool ClearLine(const AWRCharacterBase* Caster, const AActor* Target)
	{
		FHitResult Hit;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(WRNatureSight), false, Caster);
		Query.AddIgnoredActor(Target);
		return !Caster->GetWorld()->LineTraceSingleByChannel(Hit, Caster->GetActorLocation() + FVector(0, 0, 50), Target->GetActorLocation(), ECC_WorldStatic, Query);
	}

	static FVector FeetOf(const AWRCharacterBase* Who)
	{
		return Who->GetActorLocation() - FVector(0.f, 0.f, Who->GetSimpleCollisionHalfHeight());
	}

	/** The Growth a cast costs (spells.<key>.growthCost). */
	static float Cost(const FString& SpellKey, float Fallback)
	{
		return FWRTuning::Get().Num(SpellKey + TEXT(".growthCost"), Fallback);
	}

	/** Can Who pay Cost in Growth? Refused on the owning machine, the gauge flashes (no dead button). */
	static bool CanPay(const FGameplayAbilityActorInfo* ActorInfo, float Cost)
	{
		const AWRCharacterBase* Who = ActorInfo ? Cast<AWRCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
		if (!Who)
		{
			return false;
		}
		if (WRNature::HasGrowth(Who, Cost))
		{
			return true;
		}
		if (Who->IsLocallyControlled())
		{
			WRNature::NoteRefused(Who);
		}
		return false;
	}

	/** Server: give Growth back for a cast that found nothing to do (no target, nowhere to plant). */
	static void Refund(AWRCharacterBase* Who, float Amount)
	{
		UAbilitySystemComponent* ASC = Who ? Who->GetAbilitySystemComponent() : nullptr;
		UWRAttributeSet* Attr = Who ? Who->GetAttributes() : nullptr;
		if (ASC && Attr && Amount > 0.f)
		{
			ASC->SetNumericAttributeBase(UWRAttributeSet::GetGrowthAttribute(), FMath::Min(Attr->GetMaxGrowth(), Attr->GetGrowth() + Amount));
		}
	}

	/** The Hornwarden: the one boss roots hold (cooks/biome-forest.md 2.13.4). */
	static bool IsHornwarden(const AWRCharacterBase* Who)
	{
		const AWRCreature* C = Cast<AWRCreature>(Who);
		const FWRCreatureDef* Def = C ? C->GetDef() : nullptr;
		return Def && Def->Id == TEXT("hornwarden");
	}
}

// ---------------------------------------------------------------- the spell base

UWRGA_NatureSpell::UWRGA_NatureSpell()
{
	StaminaKey = TEXT("stamina.spell");
	StaminaFallback = 12.f;
	PathId = TEXT("nature");
	Element = WRNature::Element_Nature;
	ActivationOwnedTags.AddTag(WRTags::State_Attacking);
	FGameplayTagContainer Blocked = ActivationBlockedTags;
	Blocked.AddTag(WRTags::State_Attacking);
	Blocked.AddTag(WRTags::State_Dodging);
	ActivationBlockedTags = Blocked;
}

float UWRGA_NatureSpell::Spell(const TCHAR* Field, float Fallback) const
{
	return FWRTuning::Get().Num(SpellKey + TEXT(".") + Field, Fallback);
}

FVector UWRGA_NatureSpell::AimedGround(AWRCharacterBase* Caster, float Range) const
{
	const FVector Feet = WRNatureAim::FeetOf(Caster);
	if (const AWRCharacterBase* Lock = WRNatureAim::Locked(Caster))
	{
		const FVector Off = Lock->GetActorLocation() - Feet;
		return WRNatureAim::Ground(Caster, FVector(Feet.X, Feet.Y, Lock->GetActorLocation().Z) + FVector(Off.X, Off.Y, 0.f).GetClampedToMaxSize(Range));
	}
	const AWRCharacter* Survivor = Cast<AWRCharacter>(Caster);
	FVector Point;
	if (Survivor && Survivor->CrosshairPoint(Range, false, Point))
	{
		// The crosshair's ground, held to the spell's range from the feet.
		const FVector Off = Point - Feet;
		return FVector(Off.X, Off.Y, 0.f).Size() <= Range ? Point : WRNatureAim::Ground(Caster, Feet + FVector(Off.X, Off.Y, 0.f).GetClampedToMaxSize(Range));
	}
	const FVector Flat = FRotator(0.f, Caster->GetBaseAimRotation().Yaw, 0.f).Vector();
	return WRNatureAim::Ground(Caster, Feet + Flat * Range * 0.7f);
}

AWRCharacterBase* UWRGA_NatureSpell::AimedEnemy(AWRCharacterBase* Caster, float Range, float ConeDegrees) const
{
	AWRCharacterBase* Lock = WRNatureAim::Locked(Caster);
	if (Lock && FVector::Dist(Lock->GetActorLocation(), Caster->GetActorLocation()) <= Range + Lock->GetSimpleCollisionRadius())
	{
		return Lock;
	}
	const float CosCone = FMath::Cos(FMath::DegreesToRadians(ConeDegrees * 0.5f));
	const FVector Eye = Caster->GetActorLocation() + FVector(0.f, 0.f, 50.f);
	const FVector Aim = Caster->GetBaseAimRotation().Vector();
	AWRCharacterBase* Best = nullptr;
	float BestDot = -1.f;
	for (TActorIterator<AWRCharacterBase> It(Caster->GetWorld()); It; ++It)
	{
		AWRCharacterBase* Who = *It;
		if (Who == Caster || !Who->IsAlive() || Who->IsHidden() || !Caster->IsHostileTo(Who))
		{
			continue;
		}
		const FVector To = Who->GetActorLocation() - Eye;
		const float Dot = FVector::DotProduct(To.GetSafeNormal(), Aim);
		if (To.Size() > Range + Who->GetSimpleCollisionRadius() || Dot < CosCone || Dot <= BestDot || !WRNatureAim::ClearLine(Caster, Who))
		{
			continue;
		}
		BestDot = Dot;
		Best = Who;
	}
	return Best;
}

bool UWRGA_NatureSpell::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags) && WRNatureAim::CanPay(ActorInfo, WRNatureAim::Cost(SpellKey, 0.f));
}

void UWRGA_NatureSpell::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
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
	// Growth is spent as the cast begins (combat.md 2.12: Nature is a spender), as stamina is.
	if (IsServer() && !WRNature::SpendGrowth(Caster, WRNatureAim::Cost(SpellKey, 0.f)))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	// The shape (combat.md 2.13, tuning spells.shapes.<shape>): its wind-up, rooted or walking at half speed, its
	// recovery. The creatures that answer a cast read State.Casting.
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
		// Green light gathers in the casting hand, a leafy rustle.
		WRFx::BroadcastOn(Caster, TEXT("nature.cast"), TEXT("hand"), 1.f, TEXT("nature.cast"), Windup + 0.1f);
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
			CastNature(C);
		}
	});
	After(Windup + Recovery, [this]() { EndSelf(); });
}

// ---------------------------------------------------------------- Thorn shot

UWRGA_ThornShot::UWRGA_ThornShot()
{
	DisplayName = TEXT("Thorn shot"); NodeId = TEXT("thorn_shot");
	SpellKey = TEXT("spells.thornShot"); CooldownKey = TEXT("spells.thornShot.cooldown"); CooldownFallback = 1.f;
	ShapeName = TEXT("bolt");
	bAllowedMounted = true;  // bolts fly from the saddle (combat.md 2.19)
}

void UWRGA_ThornShot::CastNature(AWRCharacterBase* Caster)
{
	const float Speed = Spell(TEXT("speed"), 2400.f);
	AWRCharacterBase* Lock = WRNatureAim::Locked(Caster);
	const FVector Chest = Caster->GetActorLocation() + FVector(0.f, 0.f, 40.f);
	// Aimed ahead of where a locked target is going; the thorn itself flies straight (it never homes).
	FVector Dir;
	if (Lock)
	{
		const FVector Velocity = WRTune(TEXT("spells.lockLead"), 1.f) > 0.f ? Lock->GetVelocity() : FVector::ZeroVector;
		Dir = (UWRCombatLibrary::LeadPoint(Chest, Lock->GetActorLocation(), Velocity, Speed) - Chest).GetSafeNormal();
	}
	else
	{
		Dir = WRNatureAim::ThroughCrosshair(Caster, Chest);
	}
	const FVector SpawnAt = Chest + Dir * 70.f;

	FWRHit Hit;
	Hit.Instigator = Caster;
	Hit.Damage = Spell(TEXT("damage"), 15.f);
	Hit.PoiseDamage = Spell(TEXT("poise"), 8.f);
	Hit.Element = WRNature::Element_Nature;
	Hit.Buildups.Add(TEXT("bleed"), Spell(TEXT("bleedBuildup"), 20.f));
	Hit.PathId = PathId;
	Hit.bSpell = true;
	Hit.PhysType = 3;   // a thorn pierces

	// Point blank: a body between the chest and the muzzle would be spawned inside, so it takes the thorn at once.
	FHitResult Near;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(WRThornMuzzle), false, Caster);
	if (Caster->GetWorld()->SweepSingleByChannel(Near, Chest, SpawnAt, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(10.f), Query))
	{
		AWRCharacterBase* Blocker = Cast<AWRCharacterBase>(Near.GetActor());
		if (Blocker && Caster->IsHostileTo(Blocker))
		{
			Hit.Origin = Chest;
			Hit.bProjectile = true;
			UWRCombatLibrary::ApplyHit(Blocker, Hit);
			WRFx::Broadcast(Caster, TEXT("nature.thorn_hit"), Near.ImpactPoint, (-Dir).Rotation(), 1.f, TEXT("nature.thorn"));
			return;
		}
	}
	FActorSpawnParameters Params;
	Params.Owner = Caster;
	Params.Instigator = Caster;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AWRThornShot* Thorn = Caster->GetWorld()->SpawnActor<AWRThornShot>(AWRThornShot::StaticClass(), SpawnAt, Dir.Rotation(), Params))
	{
		Thorn->Launch(Hit, Speed, nullptr);
	}
}

// ---------------------------------------------------------------- Regrowth

UWRGA_Regrowth::UWRGA_Regrowth()
{
	DisplayName = TEXT("Regrowth"); NodeId = TEXT("regrowth");
	SpellKey = TEXT("spells.regrowth"); CooldownKey = TEXT("spells.regrowth.cooldown"); CooldownFallback = 8.f;
	ShapeName = TEXT("ally");
}

void UWRGA_Regrowth::CastNature(AWRCharacterBase* Caster)
{
	// The ally nearest the crosshair inside the cone and range, in sight; yourself when there's none.
	const float Range = Spell(TEXT("range"), 1500.f);
	const float CosCone = FMath::Cos(FMath::DegreesToRadians(Spell(TEXT("cone"), 25.f) * 0.5f));
	const FVector Eye = Caster->GetActorLocation() + FVector(0.f, 0.f, 50.f);
	const FVector Aim = Caster->GetBaseAimRotation().Vector();
	AWRCharacterBase* Target = Caster;
	float BestDot = -1.f;
	for (TActorIterator<AWRCharacterBase> It(Caster->GetWorld()); It; ++It)
	{
		AWRCharacterBase* Who = *It;
		if (Who == Caster || Who->IsHidden() || !WRNature::IsAlly(Caster, Who))
		{
			continue;
		}
		const FVector To = Who->GetActorLocation() - Eye;
		const float Dot = FVector::DotProduct(To.GetSafeNormal(), Aim);
		if (To.Size() > Range + Who->GetSimpleCollisionRadius() || Dot < CosCone || Dot <= BestDot || !WRNatureAim::ClearLine(Caster, Who))
		{
			continue;
		}
		BestDot = Dot;
		Target = Who;
	}
	const float Seconds = Spell(TEXT("seconds"), 10.f);
	if (AWRNatureField* Field = AWRNatureField::Spawn(AWRNatureField::EKind::Regrowth, Caster, Target->GetActorLocation(), Target, 0.f, Seconds))
	{
		Field->PerSecond = Spell(TEXT("heal"), 30.f) / FMath::Max(1.f, Seconds);
	}
	// A thread of gold-green light from the hand to the ally, and leaves of light round them while it heals.
	if (Target != Caster)
	{
		const FVector Hand = Caster->GetActorLocation() + FVector(0.f, 0.f, 40.f);
		const FVector D = Target->GetActorLocation() - Hand;
		WRFx::Broadcast(Caster, TEXT("nature.mend"), Hand, D.Rotation(), D.Size() / 100.f, NAME_None);
	}
	WRFx::BroadcastOn(Target, TEXT("nature.regrowth"), NAME_None, 1.f, TEXT("nature.heal"), Seconds);
}

// ---------------------------------------------------------------- Sanctuary

UWRGA_Sanctuary::UWRGA_Sanctuary()
{
	DisplayName = TEXT("Sanctuary"); NodeId = TEXT("sanctuary");
	SpellKey = TEXT("spells.sanctuary"); CooldownKey = TEXT("spells.sanctuary.cooldown"); CooldownFallback = 30.f;
	ShapeName = TEXT("pool");
}

void UWRGA_Sanctuary::CastNature(AWRCharacterBase* Caster)
{
	const FVector At = AimedGround(Caster, Spell(TEXT("range"), 1500.f));
	const float Radius = Spell(TEXT("radius"), 450.f);
	const float Seconds = Spell(TEXT("seconds"), 10.f);
	if (AWRNatureField* Field = AWRNatureField::Spawn(AWRNatureField::EKind::Sanctuary, Caster, At, nullptr, Radius, Seconds))
	{
		Field->PerSecond = Spell(TEXT("healPerSecond"), 4.f);
		WRNature::AddHealZone(Field, Caster, Radius, false);
	}
	// A ring of moss and gold-green light on the ground for as long as it lasts (scale = radius in metres).
	WRFx::Broadcast(Caster, TEXT("nature.sanctuary"), At, FRotator::ZeroRotator, Radius / 100.f, TEXT("nature.heal"), Seconds);
}

// ---------------------------------------------------------------- Root snare

UWRGA_RootSnare::UWRGA_RootSnare()
{
	DisplayName = TEXT("Root snare"); NodeId = TEXT("root_snare");
	SpellKey = TEXT("spells.rootSnare"); CooldownKey = TEXT("spells.rootSnare.cooldown"); CooldownFallback = 14.f;
	ShapeName = TEXT("pool");
}

void UWRGA_RootSnare::CastNature(AWRCharacterBase* Caster)
{
	const FVector At = AimedGround(Caster, Spell(TEXT("range"), 1500.f));
	// Briar spread: +15% radius a rank.
	const float Radius = Spell(TEXT("radius"), 250.f) * WRNature::RootRadiusScale(Caster);
	const float Seconds = Spell(TEXT("seconds"), 2.5f);
	if (AWRNatureField* Field = AWRNatureField::Spawn(AWRNatureField::EKind::Snare, Caster, At, nullptr, Radius, Seconds))
	{
		Field->HoldSeconds = Spell(TEXT("hold"), 2.5f);
		Field->BossSlow = Spell(TEXT("bossSlow"), 0.7f);
		Field->BossSlowSeconds = Spell(TEXT("bossSlowSeconds"), 3.f);
	}
	// Roots heave up through the ground across the pool and grope for feet while it lasts.
	WRFx::Broadcast(Caster, TEXT("nature.snare"), At, FRotator::ZeroRotator, Radius / 100.f, TEXT("nature.entangle"), Seconds);
}

// ---------------------------------------------------------------- Strangling vines

UWRGA_StranglingVines::UWRGA_StranglingVines()
{
	DisplayName = TEXT("Strangling vines"); NodeId = TEXT("strangling_vines");
	SpellKey = TEXT("spells.stranglingVines"); CooldownKey = TEXT("spells.stranglingVines.cooldown"); CooldownFallback = 60.f;
	ShapeName = TEXT("pool");
}

void UWRGA_StranglingVines::CastNature(AWRCharacterBase* Caster)
{
	// The target: the lock, else the enemy nearest the crosshair (Briar spread widens how far off the line it finds one).
	AWRCharacterBase* Target = AimedEnemy(Caster, Spell(TEXT("range"), 1500.f), Spell(TEXT("cone"), 30.f) * WRNature::RootRadiusScale(Caster));
	if (!Target)
	{
		// Nothing to strangle: the vines wither in the hand, and the Growth comes back (the cooldown stays: it was cast).
		WRNatureAim::Refund(Caster, WRNatureAim::Cost(SpellKey, 60.f));
		WRFx::BroadcastOn(Caster, TEXT("nature.wither"), TEXT("hand"), 0.4f, NAME_None);
		return;
	}
	const float Seconds = Spell(TEXT("seconds"), 4.f);
	const bool bMajor = UWRCombatLibrary::StanceKind(Target) == TEXT("major");
	if (bMajor)
	{
		// A major boss isn't held: the vines crush it, wear its stance and drag at it.
		WRNature::SlowBody(Target, Spell(TEXT("bossSlow"), 0.6f), Seconds);
	}
	else
	{
		// Highland creatures and minis are held too (the boss rule's exception), for the whole crush; players 0.6 s.
		WRNature::EntangleEx(Target, Seconds, Caster, true);
	}
	if (AWRNatureField* Field = AWRNatureField::Spawn(AWRNatureField::EKind::Strangle, Caster, Target->GetActorLocation(), Target, 0.f, Seconds))
	{
		Field->PerSecond = Spell(TEXT("dps"), 12.f);
		Field->bBossCrush = bMajor;
		Field->StancePerSecond = bMajor ? Spell(TEXT("bossStance"), 10.f) : 0.f;
	}
	// Thick vines burst up and coil round the body, tightening (kept up for the crush).
	WRFx::BroadcastOn(Target, TEXT("nature.strangle"), NAME_None, FMath::Max(1.f, Target->GetSimpleCollisionRadius() / 50.f), TEXT("nature.entangle"), Seconds);
}

// ---------------------------------------------------------------- the plants

void UWRGA_PlantSpell::CastNature(AWRCharacterBase* Caster)
{
	const FVector At = AimedGround(Caster, Spell(TEXT("plantRange"), 800.f));
	AWRPlantSummon* Plant = PlantClass ? AWRPlantSummon::Plant(PlantClass, Caster, At, Caster->GetActorRotation().Yaw) : nullptr;
	if (!Plant)
	{
		// Nowhere it could take root (a wall, a ledge, the sea): the seed falls dead and the Growth comes back.
		WRNatureAim::Refund(Caster, WRNatureAim::Cost(SpellKey, 0.f));
		WRFx::BroadcastOn(Caster, TEXT("nature.wither"), TEXT("hand"), 0.4f, NAME_None);
	}
}

UWRGA_SproutTurret::UWRGA_SproutTurret()
{
	DisplayName = TEXT("Sprout turret"); NodeId = TEXT("sprout_turret");
	SpellKey = TEXT("spells.sproutTurret"); CooldownKey = TEXT("spells.sproutTurret.cooldown"); CooldownFallback = 24.f;
	ShapeName = TEXT("summon");
	PlantClass = AWRSproutTurret::StaticClass();
}

UWRGA_WorldTree::UWRGA_WorldTree()
{
	DisplayName = TEXT("World tree"); NodeId = TEXT("world_tree");
	SpellKey = TEXT("spells.worldTree"); CooldownKey = TEXT("spells.worldTree.cooldown"); CooldownFallback = 90.f;
	ShapeName = TEXT("summon");
	PlantClass = AWRWorldTree::StaticClass();
}

UWRGA_AncientGuardian::UWRGA_AncientGuardian()
{
	DisplayName = TEXT("Ancient guardian"); NodeId = TEXT("ancient_guardian");
	SpellKey = TEXT("spells.ancientGuardian"); CooldownKey = TEXT("spells.ancientGuardian.cooldown"); CooldownFallback = 120.f;
	ShapeName = TEXT("summon");
	PlantClass = AWRAncientGuardian::StaticClass();
}

// ---------------------------------------------------------------- Briar strike

UWRGA_BriarStrike::UWRGA_BriarStrike()
{
	DisplayName = TEXT("Briar strike"); PathId = TEXT("nature"); NodeId = TEXT("briar_strike");
	StaminaKey = TEXT("stamina.weaponArt"); StaminaFallback = 18.f;
	CooldownKey = TEXT("spells.briarStrike.cooldown"); CooldownFallback = 3.f;
	TimingKey = TEXT("weapon.lightTiming"); SwingKind = EWRSwingKind::WeaponArt;
	Element = WRNature::Element_Nature; ArtKey = TEXT("spells.briarStrike");
	StanceMultiplier = 1.5f;  // combat.md 2.12
	ActivationOwnedTags.AddTag(WRNature::Ability_BriarStrike);
}

bool UWRGA_BriarStrike::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags) && WRNatureAim::CanPay(ActorInfo, WRNatureAim::Cost(ArtKey, 15.f));
}

void UWRGA_BriarStrike::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	// Growth is a spender (combat.md 2.12): the Wildbrand pays it as the swing begins; thorns bristle along the blade.
	if (IsServer() && IsActive())
	{
		WRNature::SpendGrowth(GetCharacter(), WRNatureAim::Cost(ArtKey, 15.f));
		WRFx::BroadcastOn(GetCharacter(), TEXT("nature.weapon"), TEXT("weapon"), 1.f, TEXT("nature.thorn"), 0.5f);
	}
}

// ---------------------------------------------------------------- Vine lash

UWRGA_VineLash::UWRGA_VineLash()
{
	DisplayName = TEXT("Vine lash"); PathId = TEXT("nature"); NodeId = TEXT("vine_lash");
	StaminaKey = TEXT("stamina.weaponArt"); StaminaFallback = 18.f;
	CooldownKey = TEXT("spells.vineLash.cooldown"); CooldownFallback = 5.f;
	ActivationOwnedTags.AddTag(WRTags::State_Attacking);
	ActivationOwnedTags.AddTag(WRNature::Ability_VineLash);
	FGameplayTagContainer Blocked = ActivationBlockedTags;
	Blocked.AddTag(WRTags::State_Attacking);
	Blocked.AddTag(WRTags::State_Dodging);
	ActivationBlockedTags = Blocked;
	// A roll from the recovery cuts it, like any weapon art.
	FGameplayTagContainer Tags;
	Tags.AddTag(WRTags::Ability_Melee);
	SetAssetTags(Tags);
	bRules = true;
	RuleAct = WRCombatRules::EAct::Attack;
}

bool UWRGA_VineLash::FollowUpReady() const
{
	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	return FollowFrom >= 0.0 && Now + 0.05 >= FollowFrom && Now <= FollowUntil;
}

bool UWRGA_VineLash::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	const AWRCharacterBase* Who = ActorInfo ? Cast<AWRCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	FString Why;
	if (!Who || !UWRGA_Melee::WildbrandFits(Who, NodeId, Why))
	{
		return false;   // a melee weapon in hand (wildbrands.vine_lash.fits), not riding
	}
	// The follow-up pull is free; the lash costs its Growth.
	return FollowUpReady() || WRNatureAim::CanPay(ActorInfo, WRNatureAim::Cost(TEXT("spells.vineLash"), 20.f));
}

bool UWRGA_VineLash::CheckCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	// The follow-up press isn't held back by the lash's cooldown.
	return FollowUpReady() || Super::CheckCooldown(Handle, ActorInfo, OptionalRelevantTags);
}

void UWRGA_VineLash::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	if (!FollowUpReady())
	{
		Super::ApplyCooldown(Handle, ActorInfo, ActivationInfo);
	}
}

void UWRGA_VineLash::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	const FWRTuning& T = FWRTuning::Get();
	const FString W = TEXT("wildbrands.vine_lash");
	bThisFollowUp = FollowUpReady();
	CostOverride = T.Num(W + TEXT(".stamina"), T.Num(TEXT("stamina.weaponArt"), 18.f)) * (bThisFollowUp ? 0.5f : 1.f);
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	AWRCharacterBase* Caster = GetCharacter();
	if (!Caster || (!bThisFollowUp && IsServer() && !WRNature::SpendGrowth(Caster, WRNatureAim::Cost(TEXT("spells.vineLash"), 20.f))))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	// The Wildbrand's frames (W 0.30 / A 0.20 / R 0.45; the pull a quicker tug), timed on the weapon art clip.
	float Windup = FMath::Max(T.Num(W + TEXT(".windup"), 0.3f), T.Num(TEXT("combat.minArtWindup"), 0.2f));
	const float Active = T.Num(W + TEXT(".active"), 0.2f);
	float Recovery = T.Num(W + TEXT(".recovery"), 0.45f);
	if (bThisFollowUp)
	{
		Windup = FMath::Max(0.2f, T.Num(W + TEXT(".followUp.windup"), Windup * 0.7f));
		Recovery = T.Num(W + TEXT(".followUp.recovery"), Recovery * 0.8f);
	}
	const double Now = GetWorld()->GetTimeSeconds();
	// Face the aim (the lock, else the camera) for the lash.
	if (const AWRCharacterBase* Lock = WRNatureAim::Locked(Caster))
	{
		Caster->SetActorRotation((Lock->GetActorLocation() - Caster->GetActorLocation()).GetSafeNormal2D().Rotation());
	}
	else if (Caster->Controller)
	{
		Caster->SetActorRotation(FRotator(0.f, Caster->GetControlRotation().Yaw, 0.f));
	}
	Caster->PlaySwing(EWRSwingKind::WeaponArt, Windup + Active, 0, Windup, Active, Recovery);
	SetMoveSlow(false, true);
	// The follow-up window: from the end of the lash until followUp.window after it (the same clock on both machines).
	if (bThisFollowUp)
	{
		FollowFrom = FollowUntil = -1.0;
	}
	else
	{
		FollowFrom = Now + Windup + Active + Recovery - 0.05;
		FollowUntil = Now + Windup + Active + Recovery + T.Num(W + TEXT(".followUp.window"), 0.6f);
	}
	const bool bPull = bThisFollowUp;
	After(Windup, [this, bPull]()
	{
		AWRCharacterBase* C = GetCharacter();
		if (C && IsServer() && C->IsAlive())
		{
			if (bPull) Pull(C);
			else Lash(C);
		}
	});
	After(Windup + Active + Recovery, [this]() { EndSelf(); });
}

void UWRGA_VineLash::Lash(AWRCharacterBase* Caster)
{
	const FWRTuning& T = FWRTuning::Get();
	const FString W = TEXT("wildbrands.vine_lash");
	const float Reach = T.Num(W + TEXT(".reach"), 700.f);
	const float Width = T.Num(W + TEXT(".width"), 45.f);
	const FVector Start = Caster->GetActorLocation() + FVector(0.f, 0.f, 30.f);
	FVector Dir = Caster->GetActorForwardVector().GetSafeNormal2D();
	if (const AWRCharacterBase* Lock = WRNatureAim::Locked(Caster))
	{
		const FVector To = Lock->GetActorLocation() - Start;
		if (To.Size() <= Reach + Lock->GetSimpleCollisionRadius()) Dir = To.GetSafeNormal();
	}
	// The vine stops at a wall.
	FVector End = Start + Dir * Reach;
	{
		FHitResult Wall;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(WRVineWall), false, Caster);
		if (Caster->GetWorld()->LineTraceSingleByChannel(Wall, Start, End, ECC_WorldStatic, Query))
		{
			End = Wall.ImpactPoint;
		}
	}
	// What the whip meets along its line, nearest first.
	TArray<FHitResult> Hits;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(WRVineLash), false, Caster);
	Caster->GetWorld()->SweepMultiByChannel(Hits, Start, End, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(Width), Params);
	TArray<AWRCharacterBase*> Victims;
	for (const FHitResult& H : Hits)
	{
		AWRCharacterBase* Who = Cast<AWRCharacterBase>(H.GetActor());
		if (Who && Who->IsAlive() && Caster->IsHostileTo(Who))
		{
			Victims.AddUnique(Who);
		}
	}
	// The thorned vine cracks out from the weapon hand along the line (scale = its length in metres), a whip crack.
	const FVector Hand = Caster->GetActorLocation() + Caster->GetActorRightVector() * 25.f + FVector(0.f, 0.f, 40.f);
	WRFx::Broadcast(Caster, TEXT("nature.vine"), Hand, (End - Hand).Rotation(), FVector::Dist(Hand, End) / 100.f, TEXT("nature.vine_lash"));

	// The class's light numbers, x1.2 (combat.md 2.12), as any Wildbrand.
	const FWRWeaponClass& Class = FWRWeapons::Get().ClassFor(Caster);
	const FWRMove* Light = Class.Move(TEXT("light1"));
	const float ClassScale = Caster->Weapon.FlatDamage > 0.f ? 1.f : FWRWeapons::ClassBase(Class, false) / FMath::Max(1.f, T.Num(TEXT("weapon.copperSwordLight"), 12.f));
	Caught.Reset();
	for (AWRCharacterBase* Who : Victims)
	{
		if (!Caught.IsValid()) Caught = Who;
		FWRHit Hit;
		Hit.Instigator = Caster;
		Hit.Damage = UWRCombatLibrary::WeaponDamage(Caster->Weapon, false) * ClassScale * T.Num(W + TEXT(".dmg"), 1.2f);
		Hit.PoiseDamage = Light ? Light->Poise : 30.f;
		Hit.StanceDamage = (Light ? Light->Stance : 5.f) * T.Num(W + TEXT(".stance"), 1.f);
		Hit.StaminaDamage = Light ? Light->GuardStamina : 30.f;
		Hit.Element = WRNature::Element_Nature;
		Hit.Buildups.Add(TEXT("bleed"), T.Num(W + TEXT(".buildup.bleed"), 25.f));
		Hit.PathId = PathId;
		Hit.bMelee = true;
		Hit.bWeaponArt = true;
		Hit.PhysType = 1;
		Hit.Impact = 2;
		Hit.Origin = Caster->GetActorLocation();
		Hit.ImpactPoint = Who->GetActorLocation();
		UWRCombatLibrary::ApplyHit(Who, Hit);
	}
}

void UWRGA_VineLash::Pull(AWRCharacterBase* Caster)
{
	const FWRTuning& T = FWRTuning::Get();
	AWRCharacterBase* Target = Caught.Get();
	Caught.Reset();
	const FVector Hand = Caster->GetActorLocation() + Caster->GetActorRightVector() * 25.f + FVector(0.f, 0.f, 40.f);
	// Never a boss, a plant, or a body that's gone out of the vine's reach.
	if (!Target || !Target->IsAlive() || Target->IsA<AWRPlantSummon>() || UWRCombatLibrary::StanceKind(Target) != TEXT("normal")
		|| FVector::Dist(Target->GetActorLocation(), Caster->GetActorLocation()) > T.Num(TEXT("wildbrands.vine_lash.reach"), 700.f) + 150.f)
	{
		WRFx::Broadcast(Caster, TEXT("nature.vine"), Hand, Caster->GetActorRotation(), 1.5f, TEXT("nature.vine_lash"));
		return;
	}
	const FVector D = Target->GetActorLocation() - Hand;
	WRFx::Broadcast(Caster, TEXT("nature.vine"), Hand, D.Rotation(), D.Size() / 100.f, TEXT("nature.vine_lash"));
	// The vine yanks it 300 cm toward you: a short hop (no ground friction eating it), landing short of you.
	const FVector Flat = (Caster->GetActorLocation() - Target->GetActorLocation()) * FVector(1.f, 1.f, 0.f);
	const float Gap = Flat.Size() - Caster->GetSimpleCollisionRadius() - Target->GetSimpleCollisionRadius() - 40.f;
	const float Dist = FMath::Clamp(Gap, 0.f, T.Num(TEXT("wildbrands.vine_lash.followUp.pull"), 300.f));
	if (Dist <= 1.f)
	{
		return;
	}
	const float Up = T.Num(TEXT("wildbrands.vine_lash.followUp.hop"), 320.f);
	const float Air = 2.f * Up / 980.f;
	Target->LaunchCharacter(Flat.GetSafeNormal() * (Dist / FMath::Max(0.1f, Air)) + FVector(0.f, 0.f, Up), true, true);
	UE_LOG(LogWildreach, Log, TEXT("NATURE vine lash pulls %s %.0f cm"), *Target->GetName(), Dist);
}

void UWRGA_VineLash::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// ---------------------------------------------------------------- the fields

AWRNatureField::AWRNatureField()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

AWRNatureField* AWRNatureField::Spawn(EKind Kind, AWRCharacterBase* Caster, const FVector& At, AWRCharacterBase* On, float Radius, float Seconds)
{
	if (!Caster || !Caster->HasAuthority())
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWRNatureField* F = Caster->GetWorld()->SpawnActor<AWRNatureField>(AWRNatureField::StaticClass(), At, FRotator::ZeroRotator, Params);
	if (F)
	{
		F->Kind = Kind;
		F->Caster = Caster;
		F->On = On;
		F->Radius = Radius;
		F->Seconds = Seconds;
		F->SetLifeSpan(Seconds + 0.5f);
		// The first tick comes at once (a heal you see land, roots that catch who stood there).
		F->Clock = 100.f;
		// Roots coming up (a snare, strangling vines): an open tomb in Nhal's chapel or the Ossuary Lord's pile in reach
		// is bound (WRGraveBosses).
		if (Kind == EKind::Snare || Kind == EKind::Strangle) FWRGraveBoss::NatureLanded(Caster->GetWorld(), On ? On->GetActorLocation() : At, Radius, Caster);
	}
	return F;
}

void AWRNatureField::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AWRCharacterBase* By = Caster.Get();
	Age += DeltaSeconds;
	if (!By || Age > Seconds)
	{
		Destroy();
		return;
	}
	AWRCharacterBase* Rider = On.Get();
	if (Kind == EKind::Regrowth || Kind == EKind::Strangle)
	{
		if (!Rider || !Rider->IsAlive())
		{
			Destroy();
			return;
		}
		SetActorLocation(Rider->GetActorLocation());
	}
	Clock += DeltaSeconds;
	switch (Kind)
	{
	case EKind::Sanctuary:
	{
		// Every second: the allies inside are healed (Purify on each tick, the first one ends a poison or rot).
		if (Clock < 1.f) break;
		Clock = 0.f;
		const float Each = PerSecond;
		TArray<AWRCharacterBase*> Inside;
		for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
		{
			if (FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) < Radius + It->GetSimpleCollisionRadius()
				&& FMath::Abs(It->GetActorLocation().Z - GetActorLocation().Z) < 300.f && WRNature::IsAlly(By, *It))
			{
				Inside.Add(*It);
			}
		}
		for (AWRCharacterBase* Who : Inside)
		{
			const bool bFirst = !Done.Contains(Who);
			Done.Add(Who);
			if (WRNature::Heal(By, Who, Each, true, bFirst) > 0.f)
			{
				WRFx::BroadcastOn(Who, TEXT("nature.heal"), NAME_None, 0.7f, NAME_None);
			}
		}
		break;
	}
	case EKind::Snare:
	{
		// Roots catch any enemy in the pool once while it lasts: held 2.5 s (the tier times don't apply), a boss slowed
		// x0.7 for 3 s instead, the Hornwarden held (the one boss roots take).
		if (Clock < 0.1f) break;
		Clock = 0.f;
		TArray<AWRCharacterBase*> Caught;
		for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
		{
			if (*It != By && It->IsAlive() && !Done.Contains(*It) && By->IsHostileTo(*It) && !It->IsA<AWRPlantSummon>()
				&& FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) < Radius + It->GetSimpleCollisionRadius()
				&& FMath::Abs(It->GetActorLocation().Z - It->GetSimpleCollisionHalfHeight() - GetActorLocation().Z) < 150.f)
			{
				Caught.Add(*It);
			}
		}
		for (AWRCharacterBase* Who : Caught)
		{
			Done.Add(Who);
			WRNature::EntangleEx(Who, HoldSeconds, By, WRNatureAim::IsHornwarden(Who), BossSlow, BossSlowSeconds);
		}
		break;
	}
	case EKind::Regrowth:
	{
		if (Clock < 1.f) break;
		Clock = 0.f;
		const bool bFirst = !Done.Contains(Rider);
		Done.Add(Rider);
		WRNature::Heal(By, Rider, PerSecond, true, bFirst);
		break;
	}
	case EKind::Strangle:
	{
		// The vines crush twice a second (12 a second; a major boss's stance 10 a second).
		const float Step = 0.5f;
		if (Clock < Step) break;
		Clock = 0.f;
		WRNature::NatureHit(By, Rider, PerSecond * Step, 0.f, GetActorLocation(), true, true, bBossCrush ? StancePerSecond * Step : 0.f, 0.f);
		break;
	}
	}
}
