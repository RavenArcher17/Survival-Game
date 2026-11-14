#include "WRCombatLibrary.h"
#include "WRSurvival.h"
#include "WRBoss.h"
#include "WRLifeSkills.h"
#include "Wildreach.h"
#include "WRAttributeSet.h"
#include "WRCharacterBase.h"
#include "WRGameplayTags.h"
#include "WRStatusComponent.h"
#include "WRTuning.h"
#include "WRPathComponent.h"
#include "WRInventory.h"
#include "WRFx.h"
#include "WRCreature.h"
#include "WRWater.h"
#include "WRWeapons.h"
#include "WRLag.h"
#include "WRStatusComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemComponent.h"
#include "EngineUtils.h"

const TCHAR* UWRCombatLibrary::ResultName(EWRHitResult Result)
{
	switch (Result)
	{
	case EWRHitResult::Hit: return TEXT("hit");
	case EWRHitResult::Blocked: return TEXT("blocked");
	case EWRHitResult::GuardBroken: return TEXT("guardbroken");
	case EWRHitResult::Parried: return TEXT("parried");
	case EWRHitResult::Dodged: return TEXT("dodged");
	default: return TEXT("ignored");
	}
}

FVector UWRCombatLibrary::LeadPoint(const FVector& From, const FVector& Target, const FVector& Velocity, float Speed)
{
	// |Target + Velocity t - From| = Speed t: a quadratic in t, and the first time from now on is when they meet.
	const FVector D = Target - From;
	const double A = FVector::DotProduct(Velocity, Velocity) - double(Speed) * Speed;
	const double B = 2.0 * FVector::DotProduct(D, Velocity);
	const double C = FVector::DotProduct(D, D);
	double T = Speed > 0.f ? D.Size() / Speed : 0.0;   // no meeting: where it'll be when a shot reaches where it is now
	if (FMath::Abs(A) < 1.0)
	{
		// Exactly as fast as the shot: the equation loses its square.
		if (B < 0.0) T = -C / B;
	}
	else
	{
		const double Disc = B * B - 4.0 * A * C;
		if (Disc >= 0.0)
		{
			const double Root = FMath::Sqrt(Disc);
			const double T1 = (-B - Root) / (2.0 * A), T2 = (-B + Root) / (2.0 * A);
			const double Soonest = T1 >= 0.0 && (T1 < T2 || T2 < 0.0) ? T1 : T2;
			if (Soonest >= 0.0) T = Soonest;
		}
	}
	return Target + Velocity * T;
}

float UWRCombatLibrary::WeaponDamage(const FWRWeaponStats& Weapon, bool bHeavy)
{
	if (Weapon.FlatDamage > 0.f)
	{
		return Weapon.FlatDamage * (bHeavy ? 1.6f : 1.f);
	}
	const float Base = bHeavy ? WRTune(TEXT("weapon.copperSwordHeavy"), 22.f) : WRTune(TEXT("weapon.copperSwordLight"), 12.f);
	const float Metal = FWRTuning::Get().Num(FString::Printf(TEXT("weapon.metal.%s"), *Weapon.Metal.ToString()), 1.f);
	const float Quality = FWRTuning::Get().Num(FString::Printf(TEXT("weapon.quality.%s"), *Weapon.Quality.ToString()), 1.f);
	const float Upgrade = 1.f + Weapon.Upgrade * WRTune(TEXT("weapon.upgradePerLevel"), 0.05f);
	return Base * Metal * Quality * Upgrade * Weapon.Roll * (1.f + Weapon.GemDamage) * (Weapon.bBroken ? 0.5f : 1.f);
}

float UWRCombatLibrary::HeatBonus(float Heat, float MaxHeat)
{
	if (MaxHeat <= 0.f)
	{
		return 0.f;
	}
	return WRTune(TEXT("resources.heatDamageBonusAtFull"), 0.3f) * FMath::Clamp(Heat / MaxHeat, 0.f, 1.f);
}

bool UWRCombatLibrary::IsInFront(const AActor* Defender, const FVector& AttackOrigin, float HalfAngleDegrees)
{
	FVector ToAttacker = AttackOrigin - Defender->GetActorLocation();
	ToAttacker.Z = 0.f;
	if (!ToAttacker.Normalize())
	{
		return true;
	}
	const float CosAngle = FVector::DotProduct(Defender->GetActorForwardVector().GetSafeNormal2D(), ToAttacker);
	return CosAngle >= FMath::Cos(FMath::DegreesToRadians(HalfAngleDegrees));
}

void UWRCombatLibrary::GainResource(AWRCharacterBase* Who, const FGameplayTag& Element, float Amount)
{
	if (!Who || !Who->HasAuthority() || Amount <= 0.f)
	{
		return;
	}
	UAbilitySystemComponent* ASC = Who->GetAbilitySystemComponent();
	UWRAttributeSet* Attr = Who->GetAttributes();
	if (!ASC || !Attr)
	{
		return;
	}
	// Marked by corruption: path resources fill slower (WRSurvival).
	if (const UWRSurvivalComponent* Survival = UWRSurvivalComponent::For(Who))
	{
		Amount *= Survival->ResourceGainMult();
	}

	const bool bBalance = ASC->HasMatchingGameplayTag(WRTags::Combo_Balance);
	const float Share = WRTune(TEXT("resources.balanceShare"), 0.5f);
	if (Element == WRTags::Element_Fire)
	{
		// Heat builds as you attack and simply stops at full (design review #2: no self-damage).
		ASC->SetNumericAttributeBase(UWRAttributeSet::GetHeatAttribute(), Attr->GetHeat() + Amount);
		// Overheat: reaching full Heat releases an explosion around you (and empties the bar).
		if (Attr->GetHeat() >= Attr->GetMaxHeat() && Attr->GetMaxHeat() > 0.f && UWRPathComponent::HasNode(Who, TEXT("fire"), TEXT("overheat")))
		{
			ASC->SetNumericAttributeBase(UWRAttributeSet::GetHeatAttribute(), 0.f);
			const float Radius = WRTune(TEXT("passives.overheatRadius"), 350.f) * (1.f + UWRPathComponent::Stat(Who, TEXT("blastRadius")));  // Shockwave
			WRFx::Broadcast(Who, TEXT("fire.impact"), Who->GetActorLocation() - FVector(0, 0, Who->GetSimpleCollisionHalfHeight() - 20.f), FRotator(90.f, 0.f, 0.f), Radius / 120.f, TEXT("fire.impact"));
			for (TActorIterator<AWRCharacterBase> It(Who->GetWorld()); It; ++It)
			{
				if (*It != Who && It->IsAlive() && Who->IsHostileTo(*It) && FVector::Dist(It->GetActorLocation(), Who->GetActorLocation()) < Radius)
				{
					FWRHit Blast;
					Blast.Instigator = Who;
					Blast.Damage = WRTune(TEXT("passives.overheatDamage"), 35.f);
					Blast.PoiseDamage = 25.f;
					Blast.Element = WRTags::Element_Fire;
					Blast.BurnBuildup = 60.f;
					Blast.Origin = Who->GetActorLocation();
					ApplyHit(*It, Blast);
				}
			}
			UE_LOG(LogWildreach, Log, TEXT("COMBAT overheat by %s"), *Who->GetName());
		}
		if (bBalance)
		{
			ASC->SetNumericAttributeBase(UWRAttributeSet::GetChillAttribute(), Attr->GetChill() + Amount * Share);
		}
		Who->Status->NotifyResourceGained(WRTags::Element_Fire);
	}
	else if (Element == WRTags::Element_Frost)
	{
		ASC->SetNumericAttributeBase(UWRAttributeSet::GetChillAttribute(), Attr->GetChill() + Amount);
		Who->Status->NotifyResourceGained(WRTags::Element_Frost);
	}
}

float UWRCombatLibrary::SpendAllChill(AWRCharacterBase* Who)
{
	if (!Who || !Who->HasAuthority())
	{
		return 0.f;
	}
	UAbilitySystemComponent* ASC = Who->GetAbilitySystemComponent();
	UWRAttributeSet* Attr = Who->GetAttributes();
	if (!ASC || !Attr || Attr->GetMaxChill() <= 0.f || Attr->GetChill() <= 0.f)
	{
		return 0.f;
	}

	const float Spent = Attr->GetChill();
	const float Fraction = Spent / Attr->GetMaxChill();
	ASC->SetNumericAttributeBase(UWRAttributeSet::GetChillAttribute(), 0.f);
	if (ASC->HasMatchingGameplayTag(WRTags::Combo_Balance))
	{
		// Balance: Heat and Chill fill each other as you use them.
		ASC->SetNumericAttributeBase(UWRAttributeSet::GetHeatAttribute(), Attr->GetHeat() + Spent * WRTune(TEXT("resources.balanceShare"), 0.5f));
	}
	return Fraction;
}

namespace WRCombatPerks
{
	/** A creature that hasn't noticed its attacker: no target, not fleeing, awake or asleep. */
	static bool IsUnaware(const AWRCharacterBase* Target, const AWRCharacterBase* Attacker)
	{
		const AWRCreature* Creature = Cast<AWRCreature>(Target);
		return Creature && !Creature->IsTamed() && Creature->GetAggroTarget() != Attacker && Creature->GetAggroRep() != Attacker
			&& Creature->CombatTarget.Get() != Attacker;
	}

	/** Damage scale from the attacker's life-skill perks (1 = none); sets bOutCrit for Assassin's sure critical. */
	static float DamageScale(const AWRCharacterBase* Attacker, const AWRCharacterBase* Target, bool bUnaware, bool& bOutCrit)
	{
		if (!Attacker || !Target || Attacker == Target)
		{
			return 1.f;
		}
		float Scale = 1.f;
		if (Attacker->GetPlayerState())
		{
			// Backstab: from behind on a creature that hasn't noticed you.
			if (bUnaware && !UWRCombatLibrary::IsInFront(Target, Attacker->GetActorLocation(), WRTune(TEXT("lifeSkills.backstabFrontDegrees"), 100.f)))
			{
				Scale *= 1.f + UWRLifeSkillsComponent::StatFor(Attacker, TEXT("backstab"));
			}
			// Assassin: the first hit on an unaware creature is critical.
			if (bUnaware && UWRLifeSkillsComponent::StatFor(Attacker, TEXT("assassin")) > 0.f)
			{
				bOutCrit = true;
			}
			// Water fighter: your blows from the water (swimming or wading).
			if (UWRWaterSubsystem::IsInWater(Attacker))
			{
				Scale *= 1.f + UWRLifeSkillsComponent::StatFor(Attacker, TEXT("waterDamage"));
			}
		}
		else if (!Attacker->GetTameOwner().IsEmpty())
		{
			// Pack leader: a tame fighting near its owner.
			if (const AGameStateBase* GS = Attacker->GetWorld()->GetGameState())
			{
				for (const APlayerState* PS : GS->PlayerArray)
				{
					const APawn* Owner = PS ? PS->GetPawn() : nullptr;
					if (Owner && PS->GetPlayerName() == Attacker->GetTameOwner()
						&& FVector::Dist(Owner->GetActorLocation(), Attacker->GetActorLocation()) < WRTune(TEXT("lifeSkills.packLeaderRange"), 2000.f))
					{
						Scale *= 1.f + UWRLifeSkillsComponent::StatFor(PS, TEXT("packLeader"));
						break;
					}
				}
			}
		}
		return Scale;
	}
}

void UWRCombatLibrary::RefreshPoise(AWRCharacterBase* Who)
{
	UAbilitySystemComponent* ASC = Who ? Who->GetAbilitySystemComponent() : nullptr;
	UWRAttributeSet* Attr = Who ? Who->GetAttributes() : nullptr;
	if (!ASC || !Attr || !Who->HasAuthority() || !Who->GetPlayerState())
	{
		return;
	}
	const UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Who);
	const float Max = FMath::Max(0.f, WRTune(TEXT("player.poise"), 0.f) + UWRPathComponent::Stat(Who, TEXT("maxPoise"))
		+ UWRLifeSkillsComponent::StatFor(Who, TEXT("armorPoise")) + (Inv ? Inv->GetWornPoise() : 0.f));
	const bool bWasFull = Attr->GetPoise() >= Attr->GetMaxPoise() - KINDA_SMALL_NUMBER;
	ASC->SetNumericAttributeBase(UWRAttributeSet::GetMaxPoiseAttribute(), Max);
	ASC->SetNumericAttributeBase(UWRAttributeSet::GetPoiseAttribute(), bWasFull ? Max : FMath::Min(Attr->GetPoise(), Max));
}

FName UWRCombatLibrary::StanceKind(const AWRCharacterBase* Who)
{
	const AWRCreature* Creature = Cast<AWRCreature>(Who);
	const FWRCreatureDef* Def = Creature ? Creature->GetDef() : nullptr;
	if (Def && Def->Archetype == TEXT("boss"))
	{
		return Def->Role.Contains(TEXT("major")) ? FName(TEXT("major")) : FName(TEXT("mini"));
	}
	return TEXT("normal");
}

float UWRCombatLibrary::MaxStanceFor(const AWRCharacterBase* Who)
{
	const AWRCreature* Creature = Cast<AWRCreature>(Who);
	const FWRCreatureDef* Def = Creature ? Creature->GetDef() : nullptr;
	if (!Def)
	{
		return WRTune(TEXT("stance.tiers.none"), 60.f);
	}
	if (Def->Stance > 0.f)
	{
		return Def->Stance;
	}
	const FName Kind = StanceKind(Who);
	const FString Tier = Kind != TEXT("normal") ? Kind.ToString() : Def->Tier.IsNone() ? FString(TEXT("none")) : Def->Tier.ToString();
	return FWRTuning::Get().Num(FString::Printf(TEXT("stance.tiers.%s"), *Tier), WRTune(TEXT("stance.tiers.none"), 60.f));
}

float UWRCombatLibrary::StanceBreakSeconds(const AWRCharacterBase* Who)
{
	const AWRCreature* Creature = Cast<AWRCreature>(Who);
	const FWRCreatureDef* Def = Creature ? Creature->GetDef() : nullptr;
	const FName Kind = StanceKind(Who);
	const FString Tier = Kind != TEXT("normal") ? Kind.ToString() : (Def && !Def->Tier.IsNone()) ? Def->Tier.ToString() : FString(TEXT("none"));
	return FWRTuning::Get().Num(FString::Printf(TEXT("stance.breakSeconds.%s"), *Tier), 2.5f);
}

void UWRCombatLibrary::BreakStance(AWRCharacterBase* Who, float Seconds, AActor* By)
{
	if (!Who || !Who->HasAuthority() || !Who->IsAlive() || Seconds <= 0.f)
	{
		return;
	}
	UAbilitySystemComponent* ASC = Who->GetAbilitySystemComponent();
	if (ASC && Who->GetAttributes())
	{
		ASC->SetNumericAttributeBase(UWRAttributeSet::GetStanceAttribute(), 0.f);
	}
	Who->ParriesTaken = 0;
	// Stunned (its abilities stop, its AI waits) and open to a critical; the stance refills when the break ends.
	Who->Status->Stagger(Seconds);
	Who->Status->ApplyStatus(WRTags::Status_StanceBroken, Seconds);
	Who->MulticastStanceBreak(true);
	UE_LOG(LogWildreach, Log, TEXT("STANCE %s broken by %s for %.1f s"), *Who->GetName(), *GetNameSafe(By), Seconds);
}

void UWRCombatLibrary::SteamBurst(AWRCharacterBase* Target, AWRCharacterBase* By, float Strength)
{
	// Fire then frost (FULL-GAME 4.2): the frost hits the hot body and flashes to steam. A scalding white jet, a hiss;
	// everyone close (the target included) is scalded, soaked and half-blinded by it; the burn is put out.
	if (!Target || !Target->HasAuthority())
	{
		return;
	}
	const FWRTuning& T = FWRTuning::Get();
	const FVector At = Target->GetActorLocation();
	const float Radius = T.Num(TEXT("reactions.steam.radius"), 300.f);
	WRFx::Broadcast(Target, TEXT("reaction.steam"), At, FRotator(90.f, 0.f, 0.f), Radius / 150.f, TEXT("reaction.steam"));
	Target->Status->ClearStatus(WRTags::Status_Burn);
	UE_LOG(LogWildreach, Log, TEXT("COMBAT steam burst on %s (x%.2f)"), *Target->GetName(), Strength);
	TArray<AWRCharacterBase*> Caught;
	for (TActorIterator<AWRCharacterBase> It(Target->GetWorld()); It; ++It)
	{
		if (It->IsAlive() && (*It == Target || (By && By->IsHostileTo(*It) && *It != By)) && FVector::Dist(It->GetActorLocation(), At) < Radius + It->GetSimpleCollisionRadius())
		{
			Caught.Add(*It);
		}
	}
	for (AWRCharacterBase* Who : Caught)
	{
		FWRHit Steam;
		Steam.Instigator = By;
		Steam.Damage = T.Num(TEXT("reactions.steam.damage"), 30.f) * Strength;
		Steam.PoiseDamage = T.Num(TEXT("reactions.steam.poise"), 20.f) * Strength;
		Steam.Element = WRTags::Element_Water;
		Steam.bReaction = true;
		Steam.Impact = 1;
		Steam.Origin = At;
		Steam.Buildups.Add(TEXT("soak"), T.Num(TEXT("reactions.steam.soak"), 30.f) * Strength);
		Steam.Buildups.Add(TEXT("blind"), T.Num(TEXT("reactions.steam.blind"), 40.f) * Strength);
		ApplyHit(Who, Steam);
	}
}

float UWRCombatLibrary::GuardStaminaCost(float StaminaDamage, float GuardBoost)
{
	return FMath::Max(0.f, StaminaDamage) * FMath::Clamp(1.f - GuardBoost / 100.f, 0.f, 1.f);
}

EWRHitResult UWRCombatLibrary::ApplyHit(AWRCharacterBase* Target, const FWRHit& Hit, TFunction<void(EWRHitResult)> OnResolved)
{
	const auto Done = [&OnResolved](EWRHitResult R)
	{
		if (OnResolved) OnResolved(R);
		return R;
	};
	if (!Target || !Target->HasAuthority() || !Target->IsAlive())
	{
		return Done(EWRHitResult::Ignored);
	}
	UAbilitySystemComponent* TargetASC = Target->GetAbilitySystemComponent();
	UWRAttributeSet* TargetAttr = Target->GetAttributes();
	if (!TargetASC || !TargetAttr)
	{
		return Done(EWRHitResult::Ignored);
	}

	AWRCharacterBase* Attacker = Cast<AWRCharacterBase>(Hit.Instigator);
	UAbilitySystemComponent* AttackerASC = Attacker ? Attacker->GetAbilitySystemComponent() : nullptr;
	if (Attacker && Attacker != Target && Attacker->GetPlayerState() && !Attacker->IsHostileTo(Target))
	{
		return Done(EWRHitResult::Ignored);  // PvP off, clanmate, safe zone, or your own tame
	}

	// A creature's or boss's blow on a remote player waits for the defence it may already have pressed on its screen
	// (server rewind, COMBAT-AND-LESSONS 1.17): it resolves a moment later, judged at the time that player saw it.
	UWRLagSubsystem* Lag = UWRLagSubsystem::Get(Target);
	if (Lag && Lag->ShouldHold(Target, Hit))
	{
		Lag->Hold(Target, Hit, MoveTemp(OnResolved));
		return EWRHitResult::Ignored;
	}
	const bool bJudged = Lag && Hit.JudgeTime > 0.0;
	// A defence the target had up when the blow is judged (now, or when a remote player saw it land).
	const auto Defended = [&](const FGameplayTag& Tag)
	{
		return bJudged ? Lag->WasDefended(Target, Tag, Hit.JudgeTime) : TargetASC->HasMatchingGameplayTag(Tag);
	};
	const double Now = Target->GetWorld()->GetTimeSeconds();
	const double JudgeAt = bJudged ? Hit.JudgeTime : Now;

	// Before this hit wakes it up: had the creature noticed its attacker? (Backstab, Assassin.)
	const bool bWasUnaware = Attacker && !Hit.bDamageOverTime && WRCombatPerks::IsUnaware(Target, Attacker);
	if (Attacker && !Hit.bDamageOverTime)
	{
		// Path magic: a same-element path hit calms a tamable creature instead of hurting it.
		if (Target->TryPathMagic(Hit))
		{
			return Done(EWRHitResult::Ignored);
		}
		Target->NotifyAttackedBy(Attacker);
		// Remember who's fighting whom (tames help their owner).
		Attacker->CombatTarget = Target;
		Target->CombatTarget = Attacker;
	}

	// --- I-frames, and blows that pass under a jump ---
	if (!Hit.bDamageOverTime && Defended(WRTags::State_Invulnerable))
	{
		// A perfect dodge: the roll's i-frames took it right as the roll began (combat.md 2.6). Nodes listen for it.
		const bool bPerfect = Attacker && Now - Target->DodgeStartedAt < 5.0
			&& JudgeAt - Target->DodgeStartedAt <= WRTune(TEXT("defense.perfectDodgeSeconds"), 0.2f) && JudgeAt >= Target->DodgeStartedAt;
		if (bPerfect)
		{
			FGameplayEventData Event;
			Event.Instigator = Attacker;
			Event.Target = Target;
			TargetASC->HandleGameplayEvent(WRTags::Event_PerfectDodge, &Event);
			UE_LOG(LogWildreach, Log, TEXT("COMBAT %s perfect dodge of %s"), *Target->GetName(), *GetNameSafe(Attacker));
			// Frozen moment: a perfect dodge freezes everything close to you (once every few seconds).
			static TMap<TWeakObjectPtr<AActor>, double> LastFrozenMoment;
			if (UWRPathComponent::HasNode(Target, TEXT("frost"), TEXT("frozen_moment")) && Now - LastFrozenMoment.FindRef(Target) > WRTune(TEXT("passives.frozenMomentCooldown"), 6.f))
			{
				LastFrozenMoment.Add(Target, Now);
				for (TActorIterator<AWRCharacterBase> It(Target->GetWorld()); It; ++It)
				{
					if (*It != Target && It->IsAlive() && Target->IsHostileTo(*It) && FVector::Dist(It->GetActorLocation(), Target->GetActorLocation()) < WRTune(TEXT("passives.frozenMomentRadius"), 400.f))
					{
						It->Status->ApplyStatus(WRTags::Status_Frozen, WRTune(TEXT("status.frozenSeconds"), 2.5f));
					}
				}
				UE_LOG(LogWildreach, Log, TEXT("COMBAT frozen moment by %s"), *Target->GetName());
			}
		}
		Target->ReportHit(0.f, EWRHitResult::Dodged, WRFx::HitFlagsFor(Hit, false, false, false, false, false), Hit.Instigator, Hit.Origin);
		return Done(EWRHitResult::Dodged);
	}
	if (Hit.bLow && Defended(WRTags::State_JumpLegsInvuln))
	{
		UE_LOG(LogWildreach, Log, TEXT("COMBAT %s jumps over %s's low blow"), *Target->GetName(), *GetNameSafe(Attacker));
		Target->ReportHit(0.f, EWRHitResult::Dodged, WRFx::HitFlagsFor(Hit, false, false, false, false, false), Hit.Instigator, Hit.Origin);
		return Done(EWRHitResult::Dodged);
	}

	// Whiteout: enemies inside a frost storm whose caster has the node miss more often.
	if (Attacker && !Hit.bDamageOverTime)
	{
		for (TActorIterator<AWRHazard> It(Target->GetWorld()); It; ++It)
		{
			const AWRCharacterBase* Caster = Cast<AWRCharacterBase>(It->Source.Get());
			if (It->Kind == TEXT("frost") && Caster && Caster->IsHostileTo(Attacker) && FVector::Dist2D(It->GetActorLocation(), Attacker->GetActorLocation()) < It->Radius
				&& UWRPathComponent::HasNode(Caster, TEXT("frost"), TEXT("whiteout")) && FMath::FRand() < WRTune(TEXT("passives.whiteoutMissChance"), 0.3f))
			{
				Target->ReportHit(0.f, EWRHitResult::Dodged, WRFx::HitFlagsFor(Hit, false, false, false, false, false), Hit.Instigator, Hit.Origin);
				return Done(EWRHitResult::Dodged);
			}
		}
	}

	float Damage = Hit.Damage;
	float PoiseDamage = Hit.PoiseDamage;
	EWRHitResult Result = EWRHitResult::Hit;
	// Deep delver (mining perk): the world's own hazards (lava, cave gas and ice: damage over time with no one behind
	// it) hurt less.
	if (!Attacker && Hit.bDamageOverTime)
	{
		Damage *= FMath::Max(0.f, 1.f + UWRLifeSkillsComponent::StatFor(Target, TEXT("hazardDamage")));
	}
	// What kind of hit it was, for the effects and damage numbers on every machine.
	bool bCrit = false, bRiposte = Hit.bCritical, bShatter = false, bStanceBroke = false;
	int32 Reaction = 0;  // the body's reaction for every machine's clip: 0 a shudder, 1-3 by the blow's impact
	const float BlockAngle = WRTune(TEXT("defense.blockAngleDegrees"), 70.f);
	const bool bFacing = IsInFront(Target, Hit.Origin, BlockAngle);
	// Swinging drops the guard, except a spear's poke from behind a raised shield (it keeps the guard up).
	const bool bSwinging = TargetASC->HasMatchingGameplayTag(WRTags::State_Attacking) && !(Target->IsSwinging() && Target->Swing.bKeepsGuard);
	// Player against player (or a player's tame against one): the pvp block (combat.md 2.20) scales it below.
	const bool bTameAttacker = Attacker && !Attacker->GetTameOwner().IsEmpty();
	const bool bPvP = Attacker && Attacker != Target && Target->GetPlayerState() && (Attacker->GetPlayerState() || bTameAttacker);
	// Counter-hit: a blow that lands on a body winding up its own (COMBAT-AND-LESSONS 1.1).
	const bool bCounterHit = Attacker && !Hit.bDamageOverTime && !Hit.bCritical && TargetASC->HasMatchingGameplayTag(WRTags::State_Windup);

	// --- Parry (COMBAT-AND-LESSONS 1.3): only blows flagged parryable, in the parry window, from the front ---
	if (Hit.bMelee && Hit.bParryable && !Hit.bCritical && !Hit.bProjectile && !Hit.bDamageOverTime && Attacker && bFacing && !bSwinging
		&& Defended(WRTags::State_Parrying))
	{
		// Searing parry: parries set the attacker burning.
		if (UWRPathComponent::HasNode(Target, TEXT("fire"), TEXT("searing_parry")))
		{
			Attacker->Status->AddBuildup(WRTags::Element_Fire, WRTune(TEXT("passives.searingParryBuildup"), 100.f), Target);
		}
		// Counted toward a break: a creature 1, a mini-boss 2, a major boss 3 (a player: 1). A parry that doesn't break
		// still interrupts and takes a quarter of its stance.
		const FName Kind = Attacker->GetPlayerState() ? FName(TEXT("normal")) : StanceKind(Attacker);
		const int32 Need = FMath::Max(1, FMath::RoundToInt(FWRTuning::Get().Num(FString::Printf(TEXT("stance.parriesToBreak.%s"), *Kind.ToString()), 1.f)));
		++Attacker->ParriesTaken;
		if (Attacker->ParriesTaken >= Need)
		{
			BreakStance(Attacker, WRTune(TEXT("crit.parryOpenSeconds"), 1.6f), Target);
			bStanceBroke = true;
		}
		else
		{
			Attacker->Status->Stagger(WRTune(TEXT("crit.parryInterruptSeconds"), 0.6f));
			if (UWRAttributeSet* AA = Attacker->GetAttributes(); AA && AttackerASC && Attacker->IsCreature())
			{
				AttackerASC->SetNumericAttributeBase(UWRAttributeSet::GetStanceAttribute(), AA->GetStance() - AA->GetMaxStance() * WRTune(TEXT("stance.nonBreakParryStance"), 0.25f));
				Attacker->Status->NotifyStanceDamaged();
			}
		}
		// Valheim's group reward: a parried creature takes more from everyone for a moment.
		Attacker->Status->ApplyStatus(WRTags::Status_Exposed, WRTune(TEXT("crit.exposedSeconds"), 2.f));
		UE_LOG(LogWildreach, Log, TEXT("COMBAT %s parried %s (%d/%d)"), *Target->GetName(), *Attacker->GetName(), Attacker->ParriesTaken, Need);
		if (bStanceBroke) Attacker->ParriesTaken = 0;
		uint16 Flags = WRFx::HitFlagsFor(Hit, false, false, false, false, false);
		if (bStanceBroke) Flags |= WRFx::Hit_StanceBreak;
		Target->ReportHit(0.f, EWRHitResult::Parried, Flags, Hit.Instigator, Hit.Origin);
		return Done(EWRHitResult::Parried);
	}

	// --- Guard (COMBAT-AND-LESSONS 1.2): from the front, not while swinging, not a grab or an unblockable blow ---
	if (!Hit.bDamageOverTime && !Hit.bUnblockable && !Hit.bGrab && !Hit.bCritical && !bSwinging && bFacing && Defended(WRTags::State_Blocking))
	{
		const FWRGuard Guard = FWRWeapons::Get().GuardFor(Target);
		// Wyrm scale: a block can freeze the attacker.
		if (Attacker && Hit.bMelee && Target->Weapon.bBlockFreeze && FMath::FRand() < WRTune(TEXT("bossParts.wyrmScaleFreezeChance"), 0.25f))
		{
			Attacker->Status->ApplyStatus(WRTags::Status_Frozen, WRTune(TEXT("status.frozenSeconds"), 2.5f));
			Attacker->GetAbilitySystemComponent()->CancelAllAbilities();
		}
		// What blocking it costs: the blow's stamina damage (a move row's; a creature's or boss's x its damage), less
		// the guard boost; Bastion (armorcraft perk) takes some off.
		const bool bBossBlow = Attacker && StanceKind(Attacker) != TEXT("normal");
		const float StaminaDamage = Hit.StaminaDamage >= 0.f ? Hit.StaminaDamage
			: Damage * (bBossBlow ? WRTune(TEXT("defense.bossStaminaDamage"), 1.6f) : WRTune(TEXT("defense.creatureStaminaDamage"), 1.2f));
		const float Cost = GuardStaminaCost(StaminaDamage, Guard.Boost) * FMath::Max(0.f, 1.f + UWRLifeSkillsComponent::StatFor(Target, TEXT("blockStamina")));
		// Chip: what the guard doesn't stop of this kind of damage gets through.
		const bool bElemental = Hit.Element.IsValid() && Hit.Element != WRTags::Element_Physical;
		Damage *= 1.f - FMath::Clamp(bElemental ? Guard.Element : Guard.Phys, 0.f, 1.f);
		PoiseDamage = 0.f;
		Target->Status->NotifyStaminaUsed();
		if (TargetAttr->GetStamina() - Cost < 0.f)
		{
			// The guard breaks: stamina gone, staggered, and a player is open to a riposte for a moment.
			TargetASC->SetNumericAttributeBase(UWRAttributeSet::GetStaminaAttribute(), 0.f);
			Target->Status->Stagger(WRTune(TEXT("defense.guardBreakStaggerSeconds"), 1.2f));
			if (Target->GetPlayerState())
			{
				Target->Status->ApplyStatus(WRTags::Status_GuardBroken, WRTune(TEXT("crit.guardBrokenRiposte"), 1.f));
			}
			Result = EWRHitResult::GuardBroken;
			UE_LOG(LogWildreach, Log, TEXT("COMBAT %s's guard broken by %s (cost %.1f)"), *Target->GetName(), *GetNameSafe(Attacker), Cost);
		}
		else
		{
			TargetASC->SetNumericAttributeBase(UWRAttributeSet::GetStaminaAttribute(), TargetAttr->GetStamina() - Cost);
			// Block stun, then the guard counter window (it opens as the stun ends: UWRStatusComponent).
			const float Stun = (Hit.Impact >= 3 ? WRTune(TEXT("defense.blockStun.heavy"), 0.35f) : WRTune(TEXT("defense.blockStun.light"), 0.2f)) * Guard.BlockStunScale;
			Target->Status->ApplyStatus(WRTags::State_BlockStun, Stun);
			Result = EWRHitResult::Blocked;
		}
	}

	// --- Multipliers ---
	if (Result == EWRHitResult::Hit && !Hit.bDamageOverTime)
	{
		if (Hit.Element == WRTags::Element_Fire && Attacker && Attacker->GetAttributes())
		{
			Damage *= 1.f + HeatBonus(Attacker->GetAttributes()->GetHeat(), Attacker->GetAttributes()->GetMaxHeat());
		}
		if (TargetASC->HasMatchingGameplayTag(WRTags::Status_Frostbite))
		{
			Damage *= 1.f + WRTune(TEXT("status.frostbiteDamageTakenBonus"), 0.15f);
		}
		if (AttackerASC && AttackerASC->HasMatchingGameplayTag(WRTags::Passive_Permafrost) && TargetASC->HasMatchingGameplayTag(WRTags::Status_Slowed))
		{
			Damage *= 1.f + WRTune(TEXT("status.permafrostBonusVsSlowed"), 0.15f);
		}
		// Just parried: everyone's blows land harder (crit.exposedDamage).
		if (TargetASC->HasMatchingGameplayTag(WRTags::Status_Exposed))
		{
			Damage *= WRTune(TEXT("crit.exposedDamage"), 1.5f);
		}
		// Corruption on the matching path: path damage (spells, Wildbrands) grows with it (WRSurvival).
		if (Hit.bWeaponArt || Hit.bSpell || !Hit.PathId.IsNone())
		{
			if (const UWRSurvivalComponent* Survival = Attacker ? UWRSurvivalComponent::For(Attacker) : nullptr) Damage *= Survival->PathDamageMult();
		}
		// Stat nodes (Data/tuning.json nodeEffects).
		if (Hit.bWeaponArt)
		{
			Damage *= 1.f + UWRPathComponent::Stat(Attacker, TEXT("weaponArtDamage"));
		}
		if (TargetASC->HasMatchingGameplayTag(WRTags::Status_Staggered))
		{
			Damage *= 1.f + UWRPathComponent::Stat(Attacker, TEXT("vsStaggered"));
		}
		// Hyperarmor (a heavy's wind-up and strike): it shrugs off a little, a great weapon's more (COMBAT-AND-LESSONS 1.5).
		if (TargetASC->HasMatchingGameplayTag(WRTags::State_Hyperarmor) && !Hit.bIgnoreHyperarmor && !Hit.bGrab)
		{
			Damage *= 1.f - (Target->IsSwinging() && Target->Swing.bGreat ? WRTune(TEXT("poise.hyperarmorDamageTakenGreat"), 0.175f) : WRTune(TEXT("poise.hyperarmorDamageTaken"), 0.075f));
		}
		// Counter-hit: +15%, a thrust +30% (combat.md 2.4.0).
		if (bCounterHit)
		{
			Damage *= 1.f + (Hit.PhysType == 3 ? WRTune(TEXT("combat.counterHit.pierceDamage"), 0.3f) : WRTune(TEXT("combat.counterHit.damage"), 0.15f));
		}
		// Slash / strike / pierce against what the body is made of (creatures.json physResist: shells take blunt blows,
		// fur turns blunt ones and parts for blades).
		if (const AWRCreature* Body = Cast<AWRCreature>(Target); Body && Body->GetDef() && Hit.PhysType >= 1 && Hit.PhysType <= 3)
		{
			Damage *= Body->GetDef()->PhysResist[Hit.PhysType];
		}
		// Marked: everyone's blows land harder. Cursed attackers hit softer.
		if (TargetASC->HasMatchingGameplayTag(WRTags::Status_Mark))
		{
			Damage *= 1.f + WRTune(TEXT("status.defs.mark.damageTaken"), 0.15f);
		}
		if (AttackerASC && AttackerASC->HasMatchingGameplayTag(WRTags::Status_Curse))
		{
			Damage *= 1.f - WRTune(TEXT("status.defs.curse.damageDealt"), 0.15f);
		}
		// Lightning on a soaked body: +25% and a chance of a free shock (the reactions table).
		if (Hit.Element == WRTags::Element_Lightning && TargetASC->HasMatchingGameplayTag(WRTags::Status_Soak))
		{
			Damage *= 1.f + WRTune(TEXT("reactions.lightningOnSoak.damage"), 0.25f);
			if (FMath::FRand() < WRTune(TEXT("reactions.lightningOnSoak.shockChance"), 0.3f))
			{
				Target->Status->AddStatusBuildup(WRStatus::Shock, Target->Status->Threshold(WRStatus::Shock), Attacker, Damage);
			}
		}
		// Brittle: frozen enemies take more critical hits. Fuel the fire: hitting burning enemies builds Heat.
		const float BrittleCrit = TargetASC->HasMatchingGameplayTag(WRTags::Status_Frozen) && UWRPathComponent::HasNode(Attacker, TEXT("frost"), TEXT("brittle"))
			? WRTune(TEXT("passives.brittleCrit"), 0.25f) : 0.f;
		if (TargetASC->HasMatchingGameplayTag(WRTags::Status_Burn) && UWRPathComponent::HasNode(Attacker, TEXT("fire"), TEXT("fuel_the_fire")))
		{
			GainResource(Attacker, WRTags::Element_Fire, WRTune(TEXT("passives.fuelTheFireHeat"), 4.f));
		}
		// Life-skill perks on the attacker (WRCombatPerks below): backstab, water fighter, pack leader; assassin crits.
		bool bPerkCrit = false;
		Damage *= WRCombatPerks::DamageScale(Attacker, Target, bWasUnaware, bPerkCrit);
		if (!Hit.bCritical && ((Hit.bMelee && FMath::FRand() < UWRPathComponent::Stat(Attacker, TEXT("critChance")) + (Attacker ? Attacker->Weapon.GemCrit : 0.f) + BrittleCrit) || bPerkCrit))
		{
			Damage *= 1.5f;
			bCrit = true;
			UE_LOG(LogWildreach, Log, TEXT("COMBAT crit on %s%s"), *Target->GetName(), bPerkCrit ? TEXT(" (unaware: assassin)") : TEXT(""));
		}

		// Frozen targets: fire thaws them (Thaw combo), heavy hits shatter them.
		if (TargetASC->HasMatchingGameplayTag(WRTags::Status_Frozen))
		{
			// Thaw: the owners of the Fire + Frost combo always; anyone else when someone else's frost froze it (the
			// reactions rule: two players' elements meeting), at half strength (FULL-GAME 4.2).
			const bool bOwnsThaw = AttackerASC && AttackerASC->HasMatchingGameplayTag(WRTags::Combo_Thaw);
			const bool bGroupThaw = !bOwnsThaw && Target->AuraElement == WRTags::Element_Frost && Target->AuraBy.Get() != Attacker
				&& Now - Target->AuraAt <= WRTune(TEXT("reactions.window"), 4.f);
			if (Hit.Element == WRTags::Element_Fire && (bOwnsThaw || bGroupThaw))
			{
				Damage += WRTune(TEXT("status.thawDamage"), 40.f) * (bOwnsThaw ? 1.f : WRTune(TEXT("reactions.nonOwnerScale"), 0.5f));
				Target->Status->ClearStatus(WRTags::Status_Frozen);
				UE_LOG(LogWildreach, Log, TEXT("COMBAT thaw on %s"), *Target->GetName());
			}
			else if (Hit.bHeavy)
			{
				// Chill: spending it all at once triggers bigger shatters.
				const float ChillFraction = SpendAllChill(Attacker);
				bShatter = true;
				Damage += WRTune(TEXT("status.shatterDamage"), 30.f) * (1.f + ChillFraction * WRTune(TEXT("resources.chillShatterBonusAtFull"), 1.f))
					* (1.f + UWRPathComponent::Stat(Attacker, TEXT("shatterDamage")));
				Target->Status->ClearStatus(WRTags::Status_Frozen);
				UE_LOG(LogWildreach, Log, TEXT("COMBAT shatter on %s (chill spent %.0f%%)"), *Target->GetName(), ChillFraction * 100.f);
				// Cold return: shatters refund some Chill.
				if (UWRPathComponent::HasNode(Attacker, TEXT("frost"), TEXT("cold_return")))
				{
					GainResource(Attacker, WRTags::Element_Frost, WRTune(TEXT("passives.coldReturnChill"), 15.f));
				}
				// Chain shatter: one shatter sets off nearby frozen enemies.
				if (Attacker && UWRPathComponent::HasNode(Attacker, TEXT("frost"), TEXT("chain_shatter")))
				{
					for (TActorIterator<AWRCharacterBase> It(Target->GetWorld()); It; ++It)
					{
						if (*It != Target && *It != Attacker && It->IsAlive() && It->HasTag(WRTags::Status_Frozen) && Attacker->IsHostileTo(*It)
							&& FVector::Dist(It->GetActorLocation(), Target->GetActorLocation()) < WRTune(TEXT("passives.chainShatterRadius"), 450.f))
						{
							It->Status->ClearStatus(WRTags::Status_Frozen);
							FWRHit Chain;
							Chain.Instigator = Attacker;
							Chain.Damage = WRTune(TEXT("status.shatterDamage"), 30.f);
							Chain.Element = WRTags::Element_Frost;
							Chain.Origin = Target->GetActorLocation();
							ApplyHit(*It, Chain);
						}
					}
				}
			}
		}
	}

	// Armor from equipped gear (players).
	if (!Hit.bDamageOverTime && Target->ArmorValue > 0.f)
	{
		Damage *= 100.f / (100.f + Target->ArmorValue);
	}
	// Player against player: its own multipliers (combat.md 2.20, ER patch 1.07): global, the weapon class, Wildbrands
	// and spells; a tame's bite on a player counts half.
	if (bPvP && !Hit.bDamageOverTime)
	{
		const FWRTuning& Tn = FWRTuning::Get();
		float Scale = Tn.Num(TEXT("pvp.damage"), 0.6f);
		if (bTameAttacker)
		{
			Scale *= Tn.Num(TEXT("pvp.tameDamage"), 0.5f);
		}
		else if (Hit.bWeaponArt)
		{
			Scale *= Tn.Num(TEXT("pvp.wildbrand"), 0.85f);
		}
		else if (Hit.bSpell || (!Hit.bMelee && !Hit.PathId.IsNone()))
		{
			Scale *= Tn.Num(TEXT("pvp.spell"), 0.85f);
		}
		else if (Hit.bMelee)
		{
			Scale *= FWRWeapons::Get().ClassFor(Attacker).PvpScale;
		}
		Damage *= Scale;
	}

	// Gear wears with use.
	if (Hit.bMelee && Result != EWRHitResult::Dodged)
	{
		if (UWRInventoryComponent* AttackerInv = UWRInventoryComponent::FindFor(Attacker))
		{
			AttackerInv->WearEquipped(EWREquipSlot::Weapon, 1.f);
		}
	}
	if (!Hit.bDamageOverTime && Damage > 0.f)
	{
		if (UWRInventoryComponent* TargetInv = UWRInventoryComponent::FindFor(Target))
		{
			TargetInv->WearEquipped(EWREquipSlot::Armor, 1.f);
		}
	}

	const float BeforeModify = Damage;
	Damage = Target->ModifyIncomingDamage(Hit, Damage);
	// A target that takes more than it was dealt is weak to it (bosses and the opposite element).
	const bool bWeak = BeforeModify > 0.f && Damage > BeforeModify * 1.05f;

	// --- Apply (a blow that takes it to zero kills it: nothing holds health at 1, COMBAT-AND-LESSONS L5) ---
	const float NewHealth = TargetAttr->GetHealth() - Damage;
	TargetASC->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), NewHealth);
	if (Damage > 0.f)
	{
		Target->OnDamageTaken(Hit, Damage);
		Target->Status->NotifyHurt();
	}
	// Fire ends frostbite (ER; the thaw): the cold's grip and its slow go.
	if (Hit.Element == WRTags::Element_Fire && TargetASC->HasMatchingGameplayTag(WRTags::Status_Frostbite))
	{
		Target->Status->ClearStatus(WRTags::Status_Frostbite);
	}
	// Combat tag (combat.md 2.20): hitting or being hit by a player (or their tame) holds both in the fight a while.
	if (bPvP)
	{
		const float Tag = WRTune(TEXT("pvp.combatTagSeconds"), 15.f);
		Target->Status->ApplyStatus(WRTags::State_CombatTagged, Tag);
		if (Attacker->GetPlayerState()) Attacker->Status->ApplyStatus(WRTags::State_CombatTagged, Tag);
	}

	// --- Poise (COMBAT-AND-LESSONS 1.5): a blow that breaks it makes the body react ---
	if (Result == EWRHitResult::Hit && PoiseDamage > 0.f && !Hit.bDamageOverTime && TargetAttr->GetHealth() > 0.f
		&& !TargetASC->HasMatchingGameplayTag(WRTags::Status_Staggered) && !TargetASC->HasMatchingGameplayTag(WRTags::Status_StanceBroken))
	{
		const bool bHyper = !Hit.bIgnoreHyperarmor && !Hit.bGrab && TargetASC->HasMatchingGameplayTag(WRTags::State_Hyperarmor);
		// The move's own hyperarmor (heavy 1.5, charged 2, great weapons 2.5: weapons.json haMult).
		const float MoveArmour = Target->IsSwinging() && Target->Swing.HyperarmorMult > 0.f ? Target->Swing.HyperarmorMult : WRTune(TEXT("poise.hyperarmor.heavy"), 1.5f);
		const float Armour = bHyper ? MoveArmour : 1.f;
		const float Left = TargetAttr->GetPoise() * Armour - PoiseDamage;
		Target->Status->NotifyPoiseDamaged();
		// A creature with hyperarmor on its own attacks (big bodies, bosses) keeps swinging through it.
		const AWRCreature* Creature = Cast<AWRCreature>(Target);
		const bool bCreatureArmour = Creature && (TargetASC->HasMatchingGameplayTag(WRTags::State_Attacking) || TargetASC->HasMatchingGameplayTag(WRTags::State_Hyperarmor))
			&& (Creature->GetDef() && (Creature->GetDef()->bHyperarmor || Creature->GetDef()->Archetype == TEXT("boss")));
		if (Left > 0.f || bCreatureArmour)
		{
			TargetASC->SetNumericAttributeBase(UWRAttributeSet::GetPoiseAttribute(), FMath::Max(0.f, Left / Armour));
		}
		else
		{
			// Broken: a player reacts by the blow's impact, a creature flinches; poise comes back full after it.
			const float Lock = Target->GetPlayerState()
				? FWRTuning::Get().Num(FString::Printf(TEXT("poise.reactions.%d"), FMath::Clamp<int32>(Hit.Impact, 1, 5)), 0.55f)
				: WRTune(TEXT("poise.creatureFlinchSeconds"), 0.45f);
			Target->Status->Stagger(Lock);
			Reaction = FMath::Clamp<int32>(Hit.Impact, 1, 3);
			// Giant's ember: staggering an enemy sends out a small lava shockwave.
			if (Attacker && Attacker->Weapon.bStaggerShockwave)
			{
				for (TActorIterator<AWRCharacterBase> It(Target->GetWorld()); It; ++It)
				{
					if (*It != Attacker && *It != Target && Attacker->IsHostileTo(*It) && It->IsAlive()
						&& FVector::Dist(It->GetActorLocation(), Target->GetActorLocation()) < WRTune(TEXT("bossParts.emberShockwaveRadius"), 350.f))
					{
						FWRHit Wave;
						Wave.Instigator = Attacker;
						Wave.Damage = WRTune(TEXT("bossParts.emberShockwaveDamage"), 20.f);
						Wave.Element = WRTags::Element_Fire;
						Wave.BurnBuildup = 30.f;
						Wave.bDamageOverTime = true;
						ApplyHit(*It, Wave);
					}
				}
			}
		}
	}

	// --- Stance (creatures, bosses, tames; COMBAT-AND-LESSONS 1.4) ---
	if ((Result == EWRHitResult::Hit || Result == EWRHitResult::GuardBroken) && Target->IsCreature() && !Hit.bDamageOverTime && !Hit.bCritical
		&& TargetAttr->GetHealth() > 0.f && !TargetASC->HasMatchingGameplayTag(WRTags::Status_StanceBroken))
	{
		float Stance = Hit.StanceDamage >= 0.f ? Hit.StanceDamage
			: Damage * (Attacker && Attacker->GetPlayerState() ? WRTune(TEXT("stance.spellPerDamage"), 0.3f) : WRTune(TEXT("stance.creaturePerDamage"), 0.3f));
		if (bShatter)
		{
			Stance *= WRTune(TEXT("stance.shatterMult"), 2.f);
		}
		// Counter-hits, blows it never saw coming (stealth), a blunt blow to the head and a rimed body all take more
		// stance (combat.md 2.4.0, 2.10, 2.11).
		if (bCounterHit) Stance *= 1.f + WRTune(TEXT("combat.counterHit.stance"), 0.3f);
		if (bWasUnaware) Stance *= 1.f + WRTune(TEXT("combat.stealth.stance"), 0.2f);
		if (TargetASC->HasMatchingGameplayTag(WRTags::Status_Rimed)) Stance *= WRTune(TEXT("status.defs.frost.rimedStance"), 1.5f);
		if (const AWRBoss* Open = Cast<AWRBoss>(Target); Open && Now < Open->StanceOpenUntil) Stance *= WRTune(TEXT("bosses.old_tusker.wallowStance"), 1.5f);
		if (AWRCreature* Body = Cast<AWRCreature>(Target))
		{
			if (Hit.PhysType == 2 && Body->IsNearLockPoint(Hit.ImpactPoint, TEXT("head"), WRTune(TEXT("stance.headRadius"), 70.f)))
			{
				Stance *= WRTune(TEXT("stance.headStrikeMult"), 1.5f);
			}
			// A breakable part near where it landed takes the blow's stance too (combat.md 2.10).
			Body->DamagePart(Hit, Stance, Attacker);
		}
		// A boss fought by many takes less stance from each (ER): x group[participants - 1], tames counting.
		if (const AWRBoss* Boss = Cast<AWRBoss>(Target))
		{
			const int32 N = FMath::Clamp(Boss->Participants, 1, 4);
			Stance *= FWRTuning::Get().Num(FString::Printf(TEXT("stance.group.%d"), N), N == 1 ? 1.f : N == 2 ? 0.6f : N == 3 ? 0.45f : 0.35f);
		}
		if (Stance > 0.f)
		{
			const float NewStance = TargetAttr->GetStance() - Stance;
			TargetASC->SetNumericAttributeBase(UWRAttributeSet::GetStanceAttribute(), NewStance);
			Target->Status->NotifyStanceDamaged();
			if (NewStance <= 0.f)
			{
				BreakStance(Target, StanceBreakSeconds(Target), Attacker);
				bStanceBroke = true;
			}
		}
	}

	// Torpor (knockout taming): heavy hits and frost wear creatures down.
	if (Result == EWRHitResult::Hit && !Hit.bDamageOverTime && Target->IsCreature())
	{
		float Torpor = (Hit.bHeavy ? WRTune(TEXT("taming.torporPerHeavyHit"), 15.f) : WRTune(TEXT("taming.torporPerHit"), 3.f)) + Hit.Torpor;
		if (Hit.Element == WRTags::Element_Frost)
		{
			Torpor += Damage * WRTune(TEXT("taming.torporFromFrost"), 0.25f);
		}
		Target->AddTorpor(Torpor * (1.f + UWRLifeSkillsComponent::StatFor(Attacker, TEXT("torporDealt"))));
	}

	// Element reactions (FULL-GAME 4.2, cooks/build-combat-full.md 2.10): a different element landing while the last one
	// still clings (reactions.window s) reacts, and uses it up. Fire then frost = a steam burst.
	if (Result == EWRHitResult::Hit && Attacker && !Hit.bDamageOverTime && !Hit.bReaction && Hit.Element.IsValid() && Hit.Element != WRTags::Element_Physical
		&& TargetAttr->GetHealth() > 0.f)
	{
		const bool bFresh = Now - Target->AuraAt <= WRTune(TEXT("reactions.window"), 4.f) && Target->AuraElement.IsValid();
		if (bFresh && Target->AuraElement == WRTags::Element_Fire && Hit.Element == WRTags::Element_Frost)
		{
			// Two players' elements meeting (or one who owns the Fire + Frost pair) react in full; one player alone at half.
			const bool bFull = Target->AuraBy.Get() != Attacker || (AttackerASC && AttackerASC->HasMatchingGameplayTag(WRTags::Combo_Thaw));
			Target->AuraElement = FGameplayTag();
			SteamBurst(Target, Attacker, bFull ? 1.f : WRTune(TEXT("reactions.nonOwnerScale"), 0.5f));
		}
		else
		{
			Target->AuraElement = Hit.Element;
			Target->AuraBy = Attacker;
			Target->AuraAt = Now;
		}
	}

	if (Result == EWRHitResult::Hit && Target->IsAlive())
	{
		// Buildup scales: a blow it never saw coming builds 20% more (stealth); against players 30% less (pvp.buildup).
		const float BuildScale = (bWasUnaware ? 1.f + WRTune(TEXT("combat.stealth.buildup"), 0.2f) : 1.f) * (bPvP ? WRTune(TEXT("pvp.buildup"), 0.7f) : 1.f);
		for (const TPair<FName, float>& B : Hit.Buildups)
		{
			const int32 Id = WRStatus::FromName(B.Key);
			if (Id == WRStatus::Burn || Id == WRStatus::Frost || Id >= WRStatus::Count)
			{
				continue;  // burn / frost go through the element path below
			}
			Target->Status->AddStatusBuildup(Id, B.Value * BuildScale, Attacker, Damage);
			if (!Target->IsAlive()) break;
		}
		float Burn = (Hit.BurnBuildup + Hit.Buildups.FindRef(TEXT("burn"))) * BuildScale;
		// Ignite chance: any fire hit can set the target burning outright.
		if (Hit.Element == WRTags::Element_Fire && !Hit.bDamageOverTime && FMath::FRand() < UWRPathComponent::Stat(Attacker, TEXT("igniteChance")))
		{
			Burn += WRTune(TEXT("status.buildupThreshold"), 100.f);
		}
		float Frost = (Hit.FrostBuildup + Hit.Buildups.FindRef(TEXT("frost"))) * BuildScale * (1.f + UWRPathComponent::Stat(Attacker, TEXT("frostBuildup")));
		if (TargetASC->HasMatchingGameplayTag(WRTags::Status_Slowed))
		{
			Frost *= 1.f + UWRPathComponent::Stat(Attacker, TEXT("freezeBuildup"));
		}
		if (Target->IsAlive()) Target->Status->AddBuildup(WRTags::Element_Fire, Burn, Attacker);
		if (Target->IsAlive()) Target->Status->AddBuildup(WRTags::Element_Frost, Frost, Attacker);
	}

	// Unhorsed (combat.md 2.19): a big blow on a ridden mount (impact 4+), or one that staggers it, throws the rider.
	if (AWRCreature* Mount = Cast<AWRCreature>(Target); Mount && Mount->GetRider() && Target->IsAlive()
		&& (Result == EWRHitResult::Hit || Result == EWRHitResult::GuardBroken) && !Hit.bDamageOverTime
		&& (Hit.Impact >= uint8(WRTune(TEXT("mounted.unhorseImpact"), 4.f)) || Reaction > 0))
	{
		Mount->Unhorse(Attacker);
	}

	if (!Hit.bDamageOverTime && Attacker)
	{
		if (Hit.Element == WRTags::Element_Fire)
		{
			GainResource(Attacker, Hit.Element, WRTune(TEXT("resources.heatPerHit"), 8.f));
		}
		else if (Hit.Element == WRTags::Element_Frost)
		{
			GainResource(Attacker, Hit.Element, WRTune(TEXT("resources.chillPerHit"), 4.f));
		}
		if (Hit.bMelee)
		{
			// Chilling touch: every melee hit builds Chill.
			GainResource(Attacker, WRTags::Element_Frost, UWRPathComponent::Stat(Attacker, TEXT("meleeChill")));
		}
		if (!Hit.PathId.IsNone())
		{
			if (UWRPathComponent* Paths = UWRPathComponent::FindFor(Attacker))
			{
				Paths->AwardXP(Hit.PathId, FMath::RoundToInt(WRTune(TEXT("paths.xpPerAbilityHit"), 4.f)));
			}
		}
	}

	UE_LOG(LogWildreach, Log, TEXT("COMBAT %s -> %s dmg=%.1f poise=%.1f elem=%s result=%s hp=%.1f%s%s"),
		*GetNameSafe(Hit.Instigator), *Target->GetName(), Damage, PoiseDamage,
		Hit.Element.IsValid() ? *Hit.Element.ToString() : TEXT("none"), ResultName(Result), TargetAttr->GetHealth(),
		bJudged ? TEXT(" (judged at the player's view)") : TEXT(""), bStanceBroke ? TEXT(" STANCE BROKEN") : TEXT(""));

	uint16 Flags = WRFx::HitFlagsFor(Hit, bCrit, bRiposte, bWeak, bShatter, Target->ArmorValue > 0.f);
	if (bStanceBroke) Flags |= WRFx::Hit_StanceBreak;
	if (Hit.bCritical) Reaction = 3;  // a crit's victim always recoils
	Flags = WRFx::WithReaction(Flags, Reaction);
	const bool bDies = TargetAttr->GetHealth() <= 0.f;
	if (bDies && StanceKind(Target) == TEXT("major") && !Hit.bDamageOverTime)
	{
		Flags = WRFx::WithTier(Flags, EWRHitTier::BossKill);  // the last blow on a major boss (fx.hitStop.bossKill)
	}
	Target->ReportHit(Damage, Result, Flags, Hit.Instigator, Hit.Origin);
	if (bDies)
	{
		// Wildfire: burning enemies erupt into flames when they die.
		if (Attacker && TargetASC->HasMatchingGameplayTag(WRTags::Status_Burn) && UWRPathComponent::HasNode(Attacker, TEXT("fire"), TEXT("wildfire")))
		{
			for (TActorIterator<AWRCharacterBase> It(Target->GetWorld()); It; ++It)
			{
				if (*It != Target && *It != Attacker && It->IsAlive() && Attacker->IsHostileTo(*It)
					&& FVector::Dist(It->GetActorLocation(), Target->GetActorLocation()) < WRTune(TEXT("passives.wildfireRadius"), 350.f))
				{
					It->Status->AddBuildup(WRTags::Element_Fire, WRTune(TEXT("passives.wildfireBuildup"), 80.f), Attacker);
				}
			}
			UE_LOG(LogWildreach, Log, TEXT("COMBAT wildfire from %s"), *Target->GetName());
		}
		Target->Die(Hit.Instigator);
	}
	return Done(Result);
}
