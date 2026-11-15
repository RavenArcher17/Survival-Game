// The Necromancy path's abilities (see WRAbilitiesNecro.h; cooks/biome-graveyard.md 2.17).

#include "WRAbilitiesNecro.h"
#include "Wildreach.h"
#include "WRAttributeSet.h"
#include "WRCharacter.h"
#include "WRCharacterBase.h"
#include "WRCombatLibrary.h"
#include "WRCombos.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRGraveEvents.h"
#include "WRGraveTypes.h"
#include "WRGameplayTags.h"
#include "WRNecroPath.h"
#include "WRPathComponent.h"
#include "WRPlayerController.h"
#include "WRProjectile.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRTuning.h"
#include "WRWeapons.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace WRNecroAim
{
	static const FLinearColor BoneGreen = FLinearColor(FColor(0xA8, 0xB8, 0x9A));
	static const FLinearColor GraveGreen = FLinearColor(FColor(0x9F, 0xE3, 0xC0));
	static const FLinearColor CurseViolet = FLinearColor(FColor(0x3A, 0x1A, 0x4A));

	static AWRCharacterBase* Locked(const AWRCharacterBase* Caster)
	{
		const AWRPlayerController* PC = Caster ? Cast<AWRPlayerController>(Caster->GetController()) : nullptr;
		AWRCharacterBase* Target = PC ? PC->GetServerLock() : nullptr;
		return Target && Target->IsAlive() && !Target->IsHidden() && Caster->IsHostileTo(Target)
			&& FVector::Dist(Target->GetActorLocation(), Caster->GetActorLocation()) <= WRTune(TEXT("spells.lockRange"), 4000.f) ? Target : nullptr;
	}
	static FVector ThroughCrosshair(const AWRCharacterBase* Caster, const FVector& From)
	{
		const FVector View = Caster->GetBaseAimRotation().Vector();
		const AWRCharacter* Survivor = Cast<AWRCharacter>(Caster);
		FVector Point;
		if (!Survivor || !Survivor->GetController() || !Survivor->CrosshairPoint(20000.f, true, Point)) return View;
		const FVector Dir = (Point - From).GetSafeNormal();
		return Dir.IsNearlyZero() || FVector::DistSquared(Point, From) < FMath::Square(100.f) || FVector::DotProduct(Dir, View) < 0.3f ? View : Dir;
	}
	static FVector Ground(const AWRCharacterBase* Caster, const FVector& At)
	{
		FHitResult Hit;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(WRNecroGround), false, Caster);
		return Caster->GetWorld()->LineTraceSingleByChannel(Hit, At + FVector(0, 0, 500), At - FVector(0, 0, 2000), ECC_WorldStatic, Query) ? FVector(Hit.ImpactPoint) : At;
	}
	static bool ClearLine(const AWRCharacterBase* Caster, const AActor* Target)
	{
		FHitResult Hit;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(WRNecroSight), false, Caster);
		Query.AddIgnoredActor(Target);
		return !Caster->GetWorld()->LineTraceSingleByChannel(Hit, Caster->GetActorLocation() + FVector(0, 0, 50), Target->GetActorLocation(), ECC_WorldStatic, Query);
	}
	static FVector Feet(const AWRCharacterBase* Who) { return Who->GetActorLocation() - FVector(0.f, 0.f, Who->GetSimpleCollisionHalfHeight()); }
	/** Can the owner pay Cost in Souls? Refused on the owning machine: the Souls row flashes. */
	static bool CanPay(const FGameplayAbilityActorInfo* ActorInfo, float Cost)
	{
		const AWRCharacterBase* Who = ActorInfo ? Cast<AWRCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
		if (!Who) return false;
		if (WRNecro::HasSouls(Who, Cost)) return true;
		if (Who->IsLocallyControlled()) WRNecro::NoteRefused(Who);
		return false;
	}
	static void Tell(const AActor* Who, const FString& Text)
	{
		const APawn* P = Cast<APawn>(Who);
		if (AWRPlayerController* PC = P ? Cast<AWRPlayerController>(P->GetController()) : nullptr) PC->ClientNotify(Text);
	}
	/** A Necromancy blow from Caster: its element, path and XP. */
	static FWRHit NecroHit(AWRCharacterBase* Caster, float Damage, float Decay, bool bSpell)
	{
		FWRHit Hit;
		Hit.Instigator = Caster;
		Hit.Damage = Damage;
		Hit.Element = WRNecro::Element_Necromancy;
		Hit.PathId = TEXT("necromancy");
		Hit.bSpell = bSpell;
		if (Decay > 0.f) Hit.Buildups.Add(TEXT("decay"), Decay);
		Hit.Origin = Caster->GetActorLocation();
		return Hit;
	}
}

// ================================================================================================ the spell base

UWRGA_NecroSpell::UWRGA_NecroSpell()
{
	StaminaKey = TEXT("stamina.spell");
	StaminaFallback = 12.f;
	PathId = TEXT("necromancy");
	Element = WRNecro::Element_Necromancy;
	ActivationOwnedTags.AddTag(WRTags::State_Attacking);
	FGameplayTagContainer Blocked = ActivationBlockedTags;
	Blocked.AddTag(WRTags::State_Attacking);
	Blocked.AddTag(WRTags::State_Dodging);
	ActivationBlockedTags = Blocked;
}

float UWRGA_NecroSpell::Spell(const TCHAR* Field, float Fallback) const
{
	return FWRTuning::Get().Num(SpellKey + TEXT(".") + Field, Fallback);
}

float UWRGA_NecroSpell::SoulCost() const
{
	return Spell(TEXT("soulCost"), 0.f);
}

bool UWRGA_NecroSpell::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	return Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags) && WRNecroAim::CanPay(ActorInfo, SoulCost());
}

void UWRGA_NecroSpell::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	Super::ApplyCost(Handle, ActorInfo, ActivationInfo);
	AWRCharacterBase* Who = ActorInfo ? Cast<AWRCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (Who && HasAuthority(&ActivationInfo)) WRNecro::SpendSouls(Who, SoulCost());
}

void UWRGA_NecroSpell::Refund(AWRCharacterBase* Caster, const TCHAR* Why) const
{
	WRNecro::RefundSouls(Caster, SoulCost());
	WRNecroAim::Tell(Caster, Why);
}

void UWRGA_NecroSpell::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
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
	if (T.Num(ShapeRow + TEXT(".roots"), 0.f) > 0.f) SetMoveRoot(true);
	else SetMoveSlow(true, true);
	Caster->PlaySwing(EWRSwingKind::Cast, Windup + 0.1f);
	Caster->SetCombatTag(WRTags::State_Casting, true);
	if (IsServer())
	{
		// Grave-light gathers in the casting hand, bone-green smoke curling off the fingers.
		WRFx::BroadcastOn(Caster, TEXT("necro.cast"), TEXT("hand"), 1.f, TEXT("necro.cast"), Windup + 0.1f);
	}
	After(Windup, [this]()
	{
		AWRCharacterBase* C = GetCharacter();
		if (!C) return;
		C->SetCombatTag(WRTags::State_Casting, false);
		if (IsServer() && C->IsAlive()) CastNecro(C);
	});
	After(Windup + Recovery, [this]() { EndSelf(); });
}

FVector UWRGA_NecroSpell::AimedGround(AWRCharacterBase* Caster, float Range) const
{
	const FVector Feet = WRNecroAim::Feet(Caster);
	if (const AWRCharacterBase* Lock = WRNecroAim::Locked(Caster))
	{
		const FVector Off = Lock->GetActorLocation() - Feet;
		return WRNecroAim::Ground(Caster, FVector(Feet.X, Feet.Y, Lock->GetActorLocation().Z) + FVector(Off.X, Off.Y, 0.f).GetClampedToMaxSize(Range));
	}
	const AWRCharacter* Survivor = Cast<AWRCharacter>(Caster);
	FVector Point;
	if (Survivor && Survivor->CrosshairPoint(Range, false, Point))
	{
		const FVector Off = Point - Feet;
		return FVector(Off.X, Off.Y, 0.f).Size() <= Range ? Point : WRNecroAim::Ground(Caster, Feet + FVector(Off.X, Off.Y, 0.f).GetClampedToMaxSize(Range));
	}
	return WRNecroAim::Ground(Caster, Feet + FRotator(0.f, Caster->GetBaseAimRotation().Yaw, 0.f).Vector() * Range * 0.6f);
}

void UWRGA_NecroSpell::FireBolt(AWRCharacterBase* Caster, FWRHit Hit, const FLinearColor& Color) const
{
	const float Speed = FMath::Min(Spell(TEXT("speed"), 1800.f), WRTune(TEXT("spells.maxBoltSpeed"), 4000.f));
	const FVector Chest = Caster->GetActorLocation() + FVector(0.f, 0.f, 40.f);
	AWRCharacterBase* Lock = WRNecroAim::Locked(Caster);
	FVector Dir;
	if (Lock)
	{
		const FVector Velocity = WRTune(TEXT("spells.lockLead"), 1.f) > 0.f ? Lock->GetVelocity() : FVector::ZeroVector;
		Dir = (UWRCombatLibrary::LeadPoint(Chest, Lock->GetActorLocation(), Velocity, Speed) - Chest).GetSafeNormal();
	}
	else
	{
		Dir = WRNecroAim::ThroughCrosshair(Caster, Chest);
	}
	const FVector SpawnAt = Chest + Dir * 70.f;
	Hit.Origin = Chest;
	Hit.bProjectile = true;
	FHitResult Near;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(WRNecroMuzzle), false, Caster);
	if (Caster->GetWorld()->SweepSingleByChannel(Near, Chest, SpawnAt, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(14.f), Query))
	{
		AWRCharacterBase* Blocker = Cast<AWRCharacterBase>(Near.GetActor());
		if (Blocker && Caster->IsHostileTo(Blocker))
		{
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
		Bolt->Init(Hit, Speed, Color);
		// The fire-frost RAISE 3 rule: it curves after a locked creature, never at a player.
		if (Lock && Lock->IsCreature())
		{
			const float Homing = WRTune(TEXT("spells.lockHoming"), 6000.f);
			if (Homing > 0.f) Bolt->HomeOn(Lock, Homing);
		}
	}
}

// ================================================================================================ the spells

UWRGA_RaiseDead::UWRGA_RaiseDead()
{
	DisplayName = TEXT("Raise dead"); NodeId = TEXT("raise_dead");
	SpellKey = TEXT("spells.raiseDead"); CooldownKey = TEXT("spells.raiseDead.cooldown"); CooldownFallback = 6.f;
	ShapeName = TEXT("summon");
}

void UWRGA_RaiseDead::CastNecro(AWRCharacterBase* Caster)
{
	// The body nearest your aim within raiseRange (two with Second wave) stands up for you.
	const FVector At = AimedGround(Caster, Spell(TEXT("range"), 1500.f));
	TArray<AWRCreature*> Bodies = WRNecro::BodiesNear(Caster->GetWorld(), At, Spell(TEXT("raiseRange"), 1000.f), Caster);
	if (Bodies.Num() == 0)
	{
		Refund(Caster, TEXT("No body there fresh enough to raise"));
		return;
	}
	int32 N = WRNecro::MinionCap(Caster) > int32(WRTune(TEXT("necromancy.minionCap"), 3.f)) && UWRPathComponent::HasNode(Caster, TEXT("necromancy"), TEXT("second_wave")) ? 2 : 1;
	// The Blood Moon's Rising: raises come two at a time on the Graveyard.
	if (UWRGraveEvents::Rising(Caster) && WRGrave::Contains(Caster->GetActorLocation())) N *= int32(WRTune(TEXT("necromancy.bloodMoonRaises"), 2.f));
	// A body behind a wall (inside someone's base): raising through walls is a raider's tool (tuning
	// raiding.paths.necromancy): within its reach, at its Soul cost multiple; where the server turns it off, only bodies
	// you can see rise.
	const bool bThroughWalls = WRTune(TEXT("raiding.paths.necromancy.raiseThroughWalls"), 0.f) > 0.f;
	Bodies.RemoveAll([Caster, bThroughWalls](const AWRCreature* B)
	{
		if (WRNecroAim::ClearLine(Caster, B)) return false;
		return !bThroughWalls || FVector::Dist(B->GetActorLocation(), Caster->GetActorLocation()) > WRTune(TEXT("raiding.paths.necromancy.raiseReachCm"), 1200.f);
	});
	if (Bodies.Num() == 0)
	{
		Refund(Caster, TEXT("No body you can reach to raise"));
		return;
	}
	for (int32 i = 0; i < N && i < Bodies.Num(); ++i)
	{
		if (!WRNecroAim::ClearLine(Caster, Bodies[i]))
		{
			const float Extra = SoulCost() * (WRTune(TEXT("raiding.paths.necromancy.raiseCostMult"), 3.f) - 1.f);
			if (!WRNecro::SpendSouls(Caster, Extra))
			{
				WRNecroAim::Tell(Caster, TEXT("Raising through a wall takes more Souls than you have"));
				continue;
			}
		}
		// Ash wraith (Fire + Necromancy): a body that died burning rises as an ash wraith.
		FName Variant;
		if (Caster->HasTag(WRNecro::Combo_AshWraith) && Bodies[i]->HasTag(WRTags::Status_Burn)) Variant = TEXT("ash");
		WRNecro::Raise(Caster, Bodies[i], Spell(TEXT("seconds"), 30.f), false, -1.f, Variant);
	}
}

UWRGA_CorpseExplosion::UWRGA_CorpseExplosion()
{
	DisplayName = TEXT("Corpse explosion"); NodeId = TEXT("corpse_explosion");
	SpellKey = TEXT("spells.corpseExplosion"); CooldownKey = TEXT("spells.corpseExplosion.cooldown"); CooldownFallback = 4.f;
	ShapeName = TEXT("nova");
}

void UWRGA_CorpseExplosion::CastNecro(AWRCharacterBase* Caster)
{
	// The minion nearest your aim blows apart in a ring of green fire.
	const FVector Aim = AimedGround(Caster, Spell(TEXT("range"), 2500.f));
	AWRCreature* Best = nullptr;
	float BestD = Spell(TEXT("pick"), 1500.f);
	for (AWRCreature* M : WRNecro::MinionsOf(Caster))
	{
		const float D = float(FVector::Dist(M->GetActorLocation(), Aim));
		if (D < BestD) { BestD = D; Best = M; }
	}
	if (!Best)
	{
		Refund(Caster, TEXT("No minion of yours there to detonate"));
		return;
	}
	const FVector At = Best->GetActorLocation();
	const float Radius = Spell(TEXT("radius"), 400.f);
	const bool bPyre = Caster->HasTag(WRNecro::Combo_FuneralPyre);
	WRNecro::Crumble(Best, true);
	if (bPyre) WRFx::Broadcast(Caster, TEXT("necro.funeral_pyre"), At, FRotator::ZeroRotator, Radius / 200.f, TEXT("fire.impact"));
	for (TActorIterator<AWRCharacterBase> It(Caster->GetWorld()); It; ++It)
	{
		AWRCharacterBase* Who = *It;
		if (!Who->IsAlive() || !Caster->IsHostileTo(Who) || FVector::Dist(Who->GetActorLocation(), At) > Radius + Who->GetSimpleCollisionRadius()) continue;
		FWRHit Hit = WRNecroAim::NecroHit(Caster, Spell(TEXT("damage"), 40.f), Spell(TEXT("decay"), 50.f), true);
		Hit.Origin = At;
		Hit.PoiseDamage = Spell(TEXT("poise"), 40.f);
		Hit.Impact = 3;
		// Funeral pyre (Fire + Necromancy 50): the blast burns.
		if (bPyre) Hit.Buildups.Add(TEXT("burn"), FWRComboRegistry::Get().Num(TEXT("funeral_pyre"), TEXT("burn"), 60.f));
		UWRCombatLibrary::ApplyHit(Who, Hit);
	}
}

UWRGA_ArmyOfTheDead::UWRGA_ArmyOfTheDead()
{
	DisplayName = TEXT("Army of the dead"); NodeId = TEXT("army_of_the_dead");
	SpellKey = TEXT("spells.armyOfTheDead"); CooldownKey = TEXT("spells.armyOfTheDead.cooldown"); CooldownFallback = 120.f;
	ShapeName = TEXT("nova");
}

void UWRGA_ArmyOfTheDead::CastNecro(AWRCharacterBase* Caster)
{
	TArray<AWRCreature*> Bodies = WRNecro::BodiesNear(Caster->GetWorld(), Caster->GetActorLocation(), Spell(TEXT("radius"), 1200.f), Caster);
	if (Bodies.Num() == 0)
	{
		Refund(Caster, TEXT("No bodies here to raise"));
		return;
	}
	WRFx::Broadcast(Caster, TEXT("necro.army"), WRNecroAim::Feet(Caster), FRotator::ZeroRotator, Spell(TEXT("radius"), 1200.f) / 400.f, TEXT("necro.army"));
	for (AWRCreature* B : Bodies)
	{
		FName Variant;
		if (Caster->HasTag(WRNecro::Combo_AshWraith) && B->HasTag(WRTags::Status_Burn)) Variant = TEXT("ash");
		WRNecro::Raise(Caster, B, Spell(TEXT("seconds"), 20.f), true, -1.f, Variant);
	}
}

UWRGA_ShadowBolt::UWRGA_ShadowBolt()
{
	DisplayName = TEXT("Shadow bolt"); NodeId = TEXT("shadow_bolt");
	SpellKey = TEXT("spells.shadowBolt"); CooldownKey = TEXT("spells.shadowBolt.cooldown"); CooldownFallback = 3.f;
	ShapeName = TEXT("bolt");
	bAllowedMounted = true;
}

void UWRGA_ShadowBolt::CastNecro(AWRCharacterBase* Caster)
{
	FWRHit Hit = WRNecroAim::NecroHit(Caster, Spell(TEXT("damage"), 30.f), Spell(TEXT("decay"), 40.f), true);
	Hit.PoiseDamage = Spell(TEXT("poise"), 20.f);
	FireBolt(Caster, Hit, WRNecroAim::BoneGreen);
	// Storm lich (Lightning + Necromancy 50) chains in Lich form: a second and third bolt at half to the nearest others.
	if (Caster->HasTag(WRNecro::Combo_StormLich) && Caster->HasTag(WRNecro::Status_LichForm))
	{
		const AWRCharacterBase* Lock = WRNecroAim::Locked(Caster);
		int32 Chained = 0;
		for (TActorIterator<AWRCharacterBase> It(Caster->GetWorld()); It && Chained < int32(FWRComboRegistry::Get().Num(TEXT("storm_lich"), TEXT("targets"), 2.f)); ++It)
		{
			AWRCharacterBase* Who = *It;
			if (Who == Lock || !Who->IsAlive() || !Caster->IsHostileTo(Who) || FVector::Dist(Who->GetActorLocation(), Caster->GetActorLocation()) > 1500.f || !WRNecroAim::ClearLine(Caster, Who)) continue;
			FWRHit Arc = WRNecroAim::NecroHit(Caster, Hit.Damage * FWRComboRegistry::Get().Num(TEXT("storm_lich"), TEXT("share"), 0.5f), Spell(TEXT("decay"), 40.f) * 0.5f, true);
			Arc.Element = WRTags::Element_Lightning;
			WRFx::Broadcast(Caster, TEXT("lightning.arc"), Caster->GetActorLocation() + FVector(0.f, 0.f, 40.f), (Who->GetActorLocation() - Caster->GetActorLocation()).Rotation(),
				float(FVector::Dist(Who->GetActorLocation(), Caster->GetActorLocation())) / 100.f, TEXT("storm.arc"));
			UWRCombatLibrary::ApplyHit(Who, Arc);
			++Chained;
		}
	}
}

UWRGA_CurseOfWeakness::UWRGA_CurseOfWeakness()
{
	DisplayName = TEXT("Curse of weakness"); NodeId = TEXT("curse_of_weakness");
	SpellKey = TEXT("spells.curseOfWeakness"); CooldownKey = TEXT("spells.curseOfWeakness.cooldown"); CooldownFallback = 10.f;
	ShapeName = TEXT("bolt");
	bAllowedMounted = true;
}

void UWRGA_CurseOfWeakness::CastNecro(AWRCharacterBase* Caster)
{
	FWRHit Hit = WRNecroAim::NecroHit(Caster, 0.f, 0.f, true);
	Hit.Buildups.Add(TEXT("curse"), Spell(TEXT("curse"), 80.f));
	FireBolt(Caster, Hit, WRNecroAim::CurseViolet);
}

UWRGA_LifeDrain::UWRGA_LifeDrain()
{
	DisplayName = TEXT("Life drain"); NodeId = TEXT("life_drain");
	SpellKey = TEXT("spells.lifeDrain"); CooldownKey = TEXT("spells.lifeDrain.cooldown"); CooldownFallback = 12.f;
	ShapeName = TEXT("bolt");
}

void UWRGA_LifeDrain::CastNecro(AWRCharacterBase* Caster)
{
	// The enemy under your lock or nearest the crosshair within 12 m, in sight.
	const float Range = Spell(TEXT("range"), 1200.f);
	AWRCharacterBase* Target = WRNecroAim::Locked(Caster);
	if (!Target || FVector::Dist(Target->GetActorLocation(), Caster->GetActorLocation()) > Range + Target->GetSimpleCollisionRadius())
	{
		Target = nullptr;
		const FVector Aim = Caster->GetBaseAimRotation().Vector();
		float BestDot = FMath::Cos(FMath::DegreesToRadians(20.f));
		for (TActorIterator<AWRCharacterBase> It(Caster->GetWorld()); It; ++It)
		{
			AWRCharacterBase* Who = *It;
			if (Who == Caster || !Who->IsAlive() || !Caster->IsHostileTo(Who)) continue;
			const FVector To = Who->GetActorLocation() - Caster->GetActorLocation();
			const float Dot = FVector::DotProduct(To.GetSafeNormal(), Aim);
			if (To.Size() > Range + Who->GetSimpleCollisionRadius() || Dot < BestDot || !WRNecroAim::ClearLine(Caster, Who)) continue;
			BestDot = Dot;
			Target = Who;
		}
	}
	if (!Target)
	{
		Refund(Caster, TEXT("Nothing in reach to drain"));
		return;
	}
	Drained = Target;
	// The channel holds you still for its 2 s; 8 pulls of a quarter second each.
	SetMoveRoot(true);
	const int32 Ticks = FMath::Max(1, FMath::RoundToInt(Spell(TEXT("seconds"), 2.f) / 0.25f));
	DrainTick(Ticks);
}

void UWRGA_LifeDrain::DrainTick(int32 Left)
{
	AWRCharacterBase* C = GetCharacter();
	AWRCharacterBase* T = Drained.Get();
	if (!C || !C->IsAlive() || !T || !T->IsAlive() || Left <= 0
		|| FVector::Dist(T->GetActorLocation(), C->GetActorLocation()) > Spell(TEXT("range"), 1200.f) + 200.f || !WRNecroAim::ClearLine(C, T))
	{
		SetMoveRoot(false);
		return;
	}
	// A pale-green beam from your hand to its chest, the life pulled back along it.
	const FVector From = C->GetActorLocation() + C->GetActorRightVector() * 25.f + FVector(0.f, 0.f, 40.f), To = T->GetActorLocation();
	WRFx::Broadcast(C, TEXT("necro.drain_beam"), From, (To - From).Rotation(), float(FVector::Dist(From, To)) / 100.f, Left % 4 == 0 ? FName(TEXT("necro.drain")) : FName(NAME_None));
	FWRHit Hit = WRNecroAim::NecroHit(C, Spell(TEXT("dps"), 12.f) * 0.25f, 0.f, true);
	Hit.bDamageOverTime = true;
	const UWRAttributeSet* TA = T->GetAttributes();
	const float Before = TA ? TA->GetHealth() : 0.f;
	UWRCombatLibrary::ApplyHit(T, Hit);
	const float Took = TA ? FMath::Max(0.f, Before - TA->GetHealth()) : 0.f;
	if (UAbilitySystemComponent* ASC = C->GetAbilitySystemComponent())
	{
		const UWRAttributeSet* A = C->GetAttributes();
		if (A && Took > 0.f) ASC->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), FMath::Min(A->GetMaxHealth(), A->GetHealth() + Took * Spell(TEXT("heal"), 0.5f)));
	}
	After(0.25f, [this, Left]() { DrainTick(Left - 1); });
}

UWRGA_LichForm::UWRGA_LichForm()
{
	DisplayName = TEXT("Lich form"); NodeId = TEXT("lich_form");
	SpellKey = TEXT("spells.lichForm"); CooldownKey = TEXT("spells.lichForm.cooldown"); CooldownFallback = 120.f;
	ShapeName = TEXT("buff");
}

void UWRGA_LichForm::CastNecro(AWRCharacterBase* Caster)
{
	const float Seconds = Spell(TEXT("seconds"), 15.f);
	if (Caster->Status) Caster->Status->ApplyStatus(WRNecro::Status_LichForm, Seconds);
	// Full ash skin, green eyes, a crown of grave-light, green breath; the choir's held chord under your mix.
	WRFx::BroadcastOn(Caster, TEXT("necro.lich_crown"), TEXT("head"), 1.f, TEXT("necro.lich"), Seconds);
}

// ================================================================================================ the Wildbrands

UWRGA_GraveStrike::UWRGA_GraveStrike()
{
	DisplayName = TEXT("Grave strike"); PathId = TEXT("necromancy"); NodeId = TEXT("grave_strike");
	StaminaKey = TEXT("stamina.weaponArt"); StaminaFallback = 18.f;
	CooldownKey = TEXT("spells.graveStrike.cooldown"); CooldownFallback = 2.5f;
	TimingKey = TEXT("weapon.lightTiming"); SwingKind = EWRSwingKind::WeaponArt;
	Element = WRNecro::Element_Necromancy; ArtKey = TEXT("spells.graveStrike");
	StanceMultiplier = 1.5f;
}

bool UWRGA_GraveStrike::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags) && WRNecroAim::CanPay(ActorInfo, WRTune(TEXT("spells.graveStrike.soulCost"), 1.f));
}

void UWRGA_GraveStrike::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	// Souls are spent as the swing begins; grave-light runs down the blade.
	if (IsServer() && IsActive())
	{
		WRNecro::SpendSouls(GetCharacter(), WRTune(TEXT("spells.graveStrike.soulCost"), 1.f));
		WRFx::BroadcastOn(GetCharacter(), TEXT("necro.weapon"), TEXT("weapon"), 1.f, TEXT("necro.cast"), 0.5f);
	}
}

UWRGA_ScytheSweep::UWRGA_ScytheSweep()
{
	DisplayName = TEXT("Scythe sweep"); PathId = TEXT("necromancy"); NodeId = TEXT("scythe_sweep");
	StaminaKey = TEXT("stamina.weaponArt"); StaminaFallback = 22.f;
	CooldownKey = TEXT("spells.scytheSweep.cooldown"); CooldownFallback = 6.f;
	ActivationOwnedTags.AddTag(WRTags::State_Attacking);
	FGameplayTagContainer Blocked = ActivationBlockedTags;
	Blocked.AddTag(WRTags::State_Attacking);
	Blocked.AddTag(WRTags::State_Dodging);
	ActivationBlockedTags = Blocked;
	FGameplayTagContainer Tags;
	Tags.AddTag(WRTags::Ability_Melee);
	SetAssetTags(Tags);
	bRules = true;
	RuleAct = WRCombatRules::EAct::Attack;
}

bool UWRGA_ScytheSweep::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)) return false;
	const AWRCharacterBase* Who = ActorInfo ? Cast<AWRCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	FString Why;
	if (!Who || !UWRGA_Melee::WildbrandFits(Who, NodeId, Why)) return false;   // a scythe, greatsword or spear in hand
	return WRNecroAim::CanPay(ActorInfo, WRTune(TEXT("spells.scytheSweep.soulCost"), 2.f));
}

void UWRGA_ScytheSweep::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	const FWRTuning& T = FWRTuning::Get();
	const FString W = TEXT("wildbrands.scythe_sweep");
	CostOverride = T.Num(W + TEXT(".stamina"), T.Num(TEXT("stamina.weaponArt"), 18.f));
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	AWRCharacterBase* Caster = GetCharacter();
	if (!Caster || (IsServer() && !WRNecro::SpendSouls(Caster, T.Num(TEXT("spells.scytheSweep.soulCost"), 2.f))))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	const float Windup = FMath::Max(T.Num(W + TEXT(".windup"), 0.4f), T.Num(TEXT("combat.minArtWindup"), 0.2f));
	const float Active = T.Num(W + TEXT(".active"), 0.3f);
	const float Recovery = T.Num(W + TEXT(".recovery"), 0.55f);
	if (Caster->Controller) Caster->SetActorRotation(FRotator(0.f, Caster->GetControlRotation().Yaw, 0.f));
	Caster->PlaySwing(EWRSwingKind::WeaponArt, Windup + Active, 1, Windup, Active, Recovery);
	SetMoveSlow(false, true);
	// Hyperarmor through the wind-up and the sweep (it can't be staggered out of it).
	if (IsServer() && Caster->Status && T.Num(W + TEXT(".ha"), 1.f) > 0.f) Caster->Status->ApplyStatus(WRTags::State_Hyperarmor, Windup + Active);
	if (IsServer()) WRFx::BroadcastOn(Caster, TEXT("necro.weapon"), TEXT("weapon"), 1.2f, TEXT("necro.cast"), Windup + Active);
	After(Windup, [this]()
	{
		AWRCharacterBase* C = GetCharacter();
		if (C && IsServer() && C->IsAlive()) Reap(C);
	});
	After(Windup + Active + Recovery, [this]() { EndSelf(); });
}

void UWRGA_ScytheSweep::Reap(AWRCharacterBase* Caster)
{
	// One sweep 300 degrees round you at 3.2 m (the blade's arc), everything in it once.
	const FWRTuning& T = FWRTuning::Get();
	const FString W = TEXT("wildbrands.scythe_sweep");
	const float Reach = T.Num(W + TEXT(".reach"), 320.f);
	const float Half = T.Num(W + TEXT(".arc"), 300.f) * 0.5f;
	const FVector C = Caster->GetActorLocation();
	const FVector Fwd = Caster->GetActorForwardVector().GetSafeNormal2D();
	WRFx::Broadcast(Caster, TEXT("necro.scythe_arc"), C, Caster->GetActorRotation(), Reach / 100.f, TEXT("swing.heavy"));
	const FWRWeaponClass& Class = FWRWeapons::Get().ClassFor(Caster);
	const FWRMove* Light = Class.Move(TEXT("light1"));
	const float ClassScale = Caster->Weapon.FlatDamage > 0.f ? 1.f : FWRWeapons::ClassBase(Class, false) / FMath::Max(1.f, T.Num(TEXT("weapon.copperSwordLight"), 12.f));
	for (TActorIterator<AWRCharacterBase> It(Caster->GetWorld()); It; ++It)
	{
		AWRCharacterBase* Who = *It;
		if (Who == Caster || !Who->IsAlive() || !Caster->IsHostileTo(Who)) continue;
		const FVector To = Who->GetActorLocation() - C;
		if (To.Size2D() > Reach + Who->GetSimpleCollisionRadius() || FMath::Abs(To.Z) > 250.f) continue;
		const float Ang = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(To.GetSafeNormal2D(), Fwd), -1.f, 1.f)));
		if (Ang > Half) continue;
		FWRHit Hit;
		Hit.Instigator = Caster;
		Hit.Damage = UWRCombatLibrary::WeaponDamage(Caster->Weapon, false) * ClassScale * T.Num(W + TEXT(".dmg"), 1.5f);
		Hit.PoiseDamage = (Light ? Light->Poise : 30.f) * 1.5f;
		Hit.StanceDamage = (Light ? Light->Stance : 5.f) * T.Num(W + TEXT(".stance"), 2.f);
		Hit.StaminaDamage = Light ? Light->GuardStamina : 30.f;
		Hit.Element = WRNecro::Element_Necromancy;
		Hit.Buildups.Add(TEXT("decay"), T.Num(W + TEXT(".buildup.decay"), 50.f));
		Hit.PathId = PathId;
		Hit.bMelee = true;
		Hit.bWeaponArt = true;
		Hit.PhysType = 1;
		Hit.Impact = 3;
		Hit.Origin = C;
		Hit.ImpactPoint = Who->GetActorLocation() - To.GetSafeNormal2D() * Who->GetSimpleCollisionRadius();
		UWRCombatLibrary::ApplyHit(Who, Hit);
	}
}
