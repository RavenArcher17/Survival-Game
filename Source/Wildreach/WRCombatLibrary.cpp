#include "WRCombatLibrary.h"
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

float UWRCombatLibrary::BlockStaminaCost(float Damage)
{
	// The doc gives 10-25 "depending on the hit"; map 0..40 damage onto that range.
	const float Min = WRTune(TEXT("stamina.blockMin"), 10.f);
	const float Max = WRTune(TEXT("stamina.blockMax"), 25.f);
	return FMath::Lerp(Min, Max, FMath::Clamp(Damage / 40.f, 0.f, 1.f));
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
			const float Radius = WRTune(TEXT("passives.overheatRadius"), 350.f);
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

EWRHitResult UWRCombatLibrary::ApplyHit(AWRCharacterBase* Target, const FWRHit& Hit)
{
	if (!Target || !Target->HasAuthority() || !Target->IsAlive())
	{
		return EWRHitResult::Ignored;
	}
	UAbilitySystemComponent* TargetASC = Target->GetAbilitySystemComponent();
	UWRAttributeSet* TargetAttr = Target->GetAttributes();
	if (!TargetASC || !TargetAttr)
	{
		return EWRHitResult::Ignored;
	}

	AWRCharacterBase* Attacker = Cast<AWRCharacterBase>(Hit.Instigator);
	UAbilitySystemComponent* AttackerASC = Attacker ? Attacker->GetAbilitySystemComponent() : nullptr;
	if (Attacker && Attacker != Target && Attacker->GetPlayerState() && !Attacker->IsHostileTo(Target))
	{
		return EWRHitResult::Ignored;  // PvP off, clanmate, safe zone, or your own tame
	}
	if (Attacker && !Hit.bDamageOverTime)
	{
		// Path magic: a same-element path hit calms a tamable creature instead of hurting it.
		if (Target->TryPathMagic(Hit))
		{
			return EWRHitResult::Ignored;
		}
		Target->NotifyAttackedBy(Attacker);
		// Remember who's fighting whom (tames help their owner).
		Attacker->CombatTarget = Target;
		Target->CombatTarget = Attacker;
	}

	if (!Hit.bDamageOverTime && TargetASC->HasMatchingGameplayTag(WRTags::State_Invulnerable))
	{
		// Frozen moment: a perfect dodge freezes everything close to you (once every few seconds).
		static TMap<TWeakObjectPtr<AActor>, double> LastFrozenMoment;
		const double Now = Target->GetWorld()->GetTimeSeconds();
		if (Attacker && UWRPathComponent::HasNode(Target, TEXT("frost"), TEXT("frozen_moment")) && Now - LastFrozenMoment.FindRef(Target) > WRTune(TEXT("passives.frozenMomentCooldown"), 6.f))
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
		Target->ReportHit(0.f, EWRHitResult::Dodged, WRFx::HitFlagsFor(Hit, false, false, false, false, false), Hit.Instigator, Hit.Origin);
		return EWRHitResult::Dodged;
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
				return EWRHitResult::Dodged;
			}
		}
	}

	float Damage = Hit.Damage;
	float PoiseDamage = Hit.PoiseDamage;
	EWRHitResult Result = EWRHitResult::Hit;
	// What kind of hit it was, for the effects and damage numbers on every machine.
	bool bCrit = false, bRiposte = false, bShatter = false;

	// --- Block and parry (melee only, from the front; not while the blocker is swinging or casting out of the guard) ---
	if (Hit.bMelee && !Hit.bDamageOverTime && TargetASC->HasMatchingGameplayTag(WRTags::State_Blocking)
		&& !TargetASC->HasMatchingGameplayTag(WRTags::State_Attacking)
		&& IsInFront(Target, Hit.Origin, WRTune(TEXT("defense.blockAngleDegrees"), 70.f)))
	{
		// Wyrm scale: a block can freeze the attacker.
		if (Attacker && Target->Weapon.bBlockFreeze && FMath::FRand() < WRTune(TEXT("bossParts.wyrmScaleFreezeChance"), 0.25f))
		{
			Attacker->Status->ApplyStatus(WRTags::Status_Frozen, WRTune(TEXT("status.frozenSeconds"), 2.5f));
			Attacker->GetAbilitySystemComponent()->CancelAllAbilities();
		}
		if (TargetASC->HasMatchingGameplayTag(WRTags::State_Parrying) && Attacker)
		{
			Attacker->Status->Stagger(WRTune(TEXT("defense.staggerSeconds"), 1.5f));
			// Searing parry: parries set the attacker burning.
			if (UWRPathComponent::HasNode(Target, TEXT("fire"), TEXT("searing_parry")))
			{
				Attacker->Status->AddBuildup(WRTags::Element_Fire, WRTune(TEXT("passives.searingParryBuildup"), 100.f), Target);
			}
			UE_LOG(LogWildreach, Log, TEXT("COMBAT %s parried %s"), *Target->GetName(), *Attacker->GetName());
			Target->ReportHit(0.f, EWRHitResult::Parried, WRFx::HitFlagsFor(Hit, false, false, false, false, false), Hit.Instigator, Hit.Origin);
			return EWRHitResult::Parried;
		}

		const float Cost = BlockStaminaCost(Damage);
		if (TargetAttr->GetStamina() >= Cost)
		{
			TargetASC->SetNumericAttributeBase(UWRAttributeSet::GetStaminaAttribute(), TargetAttr->GetStamina() - Cost);
			Target->Status->NotifyStaminaUsed();
			Damage *= WRTune(TEXT("defense.blockDamageTaken"), 0.f);
			PoiseDamage *= 0.25f;
			Result = EWRHitResult::Blocked;
		}
		else
		{
			// Out of stamina: the guard breaks and the hit lands at half strength.
			TargetASC->SetNumericAttributeBase(UWRAttributeSet::GetStaminaAttribute(), 0.f);
			Target->Status->NotifyStaminaUsed();
			Target->Status->Stagger(WRTune(TEXT("defense.guardBreakStaggerSeconds"), 1.2f));
			Damage *= 0.5f;
			Result = EWRHitResult::GuardBroken;
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
		// Stat nodes (Data/tuning.json nodeEffects).
		if (Hit.bWeaponArt)
		{
			Damage *= 1.f + UWRPathComponent::Stat(Attacker, TEXT("weaponArtDamage"));
		}
		if (TargetASC->HasMatchingGameplayTag(WRTags::Status_Staggered))
		{
			Damage *= 1.f + UWRPathComponent::Stat(Attacker, TEXT("vsStaggered"));
		}
		// Brittle: frozen enemies take more critical hits. Fuel the fire: hitting burning enemies builds Heat.
		const float BrittleCrit = TargetASC->HasMatchingGameplayTag(WRTags::Status_Frozen) && UWRPathComponent::HasNode(Attacker, TEXT("frost"), TEXT("brittle"))
			? WRTune(TEXT("passives.brittleCrit"), 0.25f) : 0.f;
		if (TargetASC->HasMatchingGameplayTag(WRTags::Status_Burn) && UWRPathComponent::HasNode(Attacker, TEXT("fire"), TEXT("fuel_the_fire")))
		{
			GainResource(Attacker, WRTags::Element_Fire, WRTune(TEXT("passives.fuelTheFireHeat"), 4.f));
		}
		if (Hit.bMelee && FMath::FRand() < UWRPathComponent::Stat(Attacker, TEXT("critChance")) + (Attacker ? Attacker->Weapon.GemCrit : 0.f) + BrittleCrit)
		{
			Damage *= 1.5f;
			bCrit = true;
			UE_LOG(LogWildreach, Log, TEXT("COMBAT crit on %s"), *Target->GetName());
		}
		if (Hit.bMelee && TargetASC->HasMatchingGameplayTag(WRTags::Status_Staggered))
		{
			// Riposte / critical on a staggered target.
			Damage *= WRTune(TEXT("weapon.riposteMultiplier"), 3.f);
			bRiposte = true;
			Target->Status->ClearStatus(WRTags::Status_Staggered);
			UE_LOG(LogWildreach, Log, TEXT("COMBAT riposte on %s"), *Target->GetName());
			// Cold riposte: ripostes freeze the target.
			if (UWRPathComponent::HasNode(Attacker, TEXT("frost"), TEXT("cold_riposte")))
			{
				Target->Status->ApplyStatus(WRTags::Status_Frozen, WRTune(TEXT("status.frozenSeconds"), 2.5f));
			}
		}

		// Frozen targets: fire thaws them (Thaw combo), heavy hits shatter them.
		if (TargetASC->HasMatchingGameplayTag(WRTags::Status_Frozen))
		{
			if (Hit.Element == WRTags::Element_Fire && AttackerASC && AttackerASC->HasMatchingGameplayTag(WRTags::Combo_Thaw))
			{
				Damage += WRTune(TEXT("status.thawDamage"), 40.f);
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

	// --- Apply ---
	const float NewHealth = TargetAttr->GetHealth() - Damage;
	TargetASC->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), NewHealth);
	if (Damage > 0.f)
	{
		Target->OnDamageTaken(Hit, Damage);
	}

	if (PoiseDamage > 0.f && !TargetASC->HasMatchingGameplayTag(WRTags::Status_Staggered))
	{
		const float NewPoise = TargetAttr->GetPoise() - PoiseDamage;
		TargetASC->SetNumericAttributeBase(UWRAttributeSet::GetPoiseAttribute(), NewPoise);
		Target->Status->NotifyPoiseDamaged();
		if (NewPoise <= 0.f)
		{
			Target->Status->Stagger(WRTune(TEXT("defense.staggerSeconds"), 1.5f));
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

	if (Result == EWRHitResult::Hit)
	{
		float Burn = Hit.BurnBuildup;
		// Ignite chance: any fire hit can set the target burning outright.
		if (Hit.Element == WRTags::Element_Fire && !Hit.bDamageOverTime && FMath::FRand() < UWRPathComponent::Stat(Attacker, TEXT("igniteChance")))
		{
			Burn += WRTune(TEXT("status.buildupThreshold"), 100.f);
		}
		float Frost = Hit.FrostBuildup * (1.f + UWRPathComponent::Stat(Attacker, TEXT("frostBuildup")));
		if (TargetASC->HasMatchingGameplayTag(WRTags::Status_Slowed))
		{
			Frost *= 1.f + UWRPathComponent::Stat(Attacker, TEXT("freezeBuildup"));
		}
		Target->Status->AddBuildup(WRTags::Element_Fire, Burn, Attacker);
		Target->Status->AddBuildup(WRTags::Element_Frost, Frost, Attacker);
	}

	if (!Hit.bDamageOverTime && Attacker && Result != EWRHitResult::Parried)
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

	UE_LOG(LogWildreach, Log, TEXT("COMBAT %s -> %s dmg=%.1f poise=%.1f elem=%s result=%s hp=%.1f"),
		*GetNameSafe(Hit.Instigator), *Target->GetName(), Damage, PoiseDamage,
		Hit.Element.IsValid() ? *Hit.Element.ToString() : TEXT("none"), ResultName(Result), TargetAttr->GetHealth());

	Target->ReportHit(Damage, Result, WRFx::HitFlagsFor(Hit, bCrit, bRiposte, bWeak, bShatter, Target->ArmorValue > 0.f), Hit.Instigator, Hit.Origin);
	if (TargetAttr->GetHealth() <= 0.f)
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
	return Result;
}
