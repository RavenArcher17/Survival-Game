#include "WRLightningPath.h"
#include "Wildreach.h"
#include "WRAttributeSet.h"
#include "WRBoss.h"
#include "WRCharacterBase.h"
#include "WRCombatLibrary.h"
#include "WRCombos.h"
#include "WRDeeds.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRInventory.h"
#include "WRPathComponent.h"
#include "WRStatusComponent.h"
#include "WRStormTypes.h"
#include "WRSurvival.h"
#include "WRSwampBosses.h"
#include "WRTuning.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"

namespace WRLightning
{
	UE_DEFINE_GAMEPLAY_TAG(Ability_StaticStrike, "Ability.Lightning.StaticStrike");
	UE_DEFINE_GAMEPLAY_TAG(Ability_ChargedWeapon, "Ability.Lightning.ChargedWeapon");
	UE_DEFINE_GAMEPLAY_TAG(Status_ThunderGod, "Status.ThunderGod");
	UE_DEFINE_GAMEPLAY_TAG(State_BoltDash, "State.BoltDash");
	UE_DEFINE_GAMEPLAY_TAG(Combo_Plasma, "Combo.Plasma");
	UE_DEFINE_GAMEPLAY_TAG(Combo_Meltdown, "Combo.Meltdown");
	UE_DEFINE_GAMEPLAY_TAG(Combo_BrittleShock, "Combo.BrittleShock");
	UE_DEFINE_GAMEPLAY_TAG(Combo_BlizzardStorm, "Combo.BlizzardStorm");
}

namespace WRLightningImpl
{
	static const FName PathId(TEXT("lightning"));

	/** A survivor walking the Lightning path (main or subclass): only they build and use Charge. */
	static bool Walks(const AActor* Who)
	{
		const UWRPathComponent* Paths = UWRPathComponent::FindFor(Who);
		return Paths && Paths->IsActivePath(PathId);
	}

	static const UWRAttributeSet* AttrOf(const AActor* Who)
	{
		const AWRCharacterBase* C = Cast<AWRCharacterBase>(Who);
		return C ? C->GetAttributes() : nullptr;
	}

	static FVector FeetOf(const AWRCharacterBase* Who)
	{
		return Who->GetActorLocation() - FVector(0.f, 0.f, Who->GetSimpleCollisionHalfHeight());
	}

	static TMap<TWeakObjectPtr<const AActor>, float>& Stored()
	{
		static TMap<TWeakObjectPtr<const AActor>, float> Map;
		return Map;
	}
}

// ---------------------------------------------------------------- Grounded

bool WRLightning::IsGrounded(const AActor* Who)
{
	const AWRCharacterBase* C = Cast<AWRCharacterBase>(Who);
	return C && C->HasTag(WRTags::Passive_Grounded);
}

float WRLightning::StrikeDamageMult(const AActor* Who)
{
	return IsGrounded(Who) ? WRTune(TEXT("passives.groundedStrikeMult"), 0.5f) : 1.f;
}

// ---------------------------------------------------------------- Charge

float WRLightning::ChargeValue(const AActor* Who)
{
	const UWRAttributeSet* A = WRLightningImpl::AttrOf(Who);
	return A ? A->GetCharge() : 0.f;
}

float WRLightning::ChargeFraction(const AActor* Who)
{
	const UWRAttributeSet* A = WRLightningImpl::AttrOf(Who);
	return A && A->GetMaxCharge() > 0.f ? FMath::Clamp(A->GetCharge() / A->GetMaxCharge(), 0.f, 1.f) : 0.f;
}

bool WRLightning::HighCharge(const AActor* Who)
{
	return ChargeValue(Who) >= WRTune(TEXT("resources.chargeHighAt"), 60.f) && WRLightningImpl::Walks(Who);
}

void WRLightning::GainCharge(AWRCharacterBase* Who, float Amount)
{
	if (!Who || !Who->HasAuthority() || Amount <= 0.f || !WRLightningImpl::Walks(Who))
	{
		return;
	}
	UAbilitySystemComponent* ASC = Who->GetAbilitySystemComponent();
	UWRAttributeSet* Attr = Who->GetAttributes();
	if (!ASC || !Attr)
	{
		return;
	}
	// Recharge (nodeEffects lightning.recharge chargeGain); corruption's Marked, worn chargeGain gear, prestige (Deeper
	// marks) and talismans, as Heat and Chill have.
	if (const UWRSurvivalComponent* Survival = UWRSurvivalComponent::For(Who))
	{
		Amount *= Survival->ResourceGainMult();
	}
	Amount *= FMath::Max(0.f, 1.f + UWRPathComponent::Stat(Who, TEXT("chargeGain")) + UWRInventoryComponent::GearStatFor(Who, TEXT("chargeGain")));
	Amount *= WRDeeds::PrestigeResource(Who, WRLightningImpl::PathId) * WRDeeds::ResourceGainScale(Who);
	ASC->SetNumericAttributeBase(UWRAttributeSet::GetChargeAttribute(), Attr->GetCharge() + Amount);
	Who->Status->NotifyResourceGained(WRTags::Element_Lightning);
}

float WRLightning::SpendCharge(AWRCharacterBase* Who, float Amount)
{
	UAbilitySystemComponent* ASC = Who && Who->HasAuthority() ? Who->GetAbilitySystemComponent() : nullptr;
	UWRAttributeSet* Attr = Who ? Who->GetAttributes() : nullptr;
	if (!ASC || !Attr || Amount <= 0.f)
	{
		return 0.f;
	}
	const float Taken = FMath::Min(Amount, Attr->GetCharge());
	ASC->SetNumericAttributeBase(UWRAttributeSet::GetChargeAttribute(), Attr->GetCharge() - Taken);
	return Taken;
}

void WRLightning::TickCharge(AWRCharacterBase* Who, float SinceGain, float DeltaTime)
{
	UWRAttributeSet* Attr = Who ? Who->GetAttributes() : nullptr;
	UAbilitySystemComponent* ASC = Who ? Who->GetAbilitySystemComponent() : nullptr;
	if (!Attr || !ASC || Attr->GetCharge() <= 0.f)
	{
		return;
	}
	// Fills fast, empties fast (cooks/biome-lightning.md 2.18.2): after chargeDecayDelay s without a gain it drains.
	if (SinceGain >= WRTune(TEXT("resources.chargeDecayDelay"), 1.5f))
	{
		ASC->SetNumericAttributeBase(UWRAttributeSet::GetChargeAttribute(), Attr->GetCharge() - WRTune(TEXT("resources.chargeDecayPerSecond"), 25.f) * DeltaTime);
	}
}

// ---------------------------------------------------------------- passives

float WRLightning::DamageScale(const AWRCharacterBase* Attacker, const AWRCharacterBase* Target, const FWRHit& Hit)
{
	if (!Attacker || !WRLightningImpl::Walks(Attacker))
	{
		return 1.f;
	}
	float Scale = 1.f;
	const float Charge = ChargeValue(Attacker);
	// High Charge: +20% lightning damage at 60 and over; Overcharge: +8% a rank more at 95 and over.
	if (Charge >= WRTune(TEXT("resources.chargeHighAt"), 60.f))
	{
		Scale += WRTune(TEXT("resources.chargeDamageBonusAtHigh"), 0.2f);
	}
	if (Charge >= WRTune(TEXT("passives.overchargeAt"), 95.f))
	{
		Scale += UWRPathComponent::Stat(Attacker, TEXT("fullChargeDamage"));
	}
	// Conductive: soaked enemies take more lightning damage (on top of the reactions table's own share).
	if (Target && Target->HasTag(WRTags::Status_Soak) && UWRPathComponent::HasNode(Attacker, WRLightningImpl::PathId, TEXT("conductive")))
	{
		Scale += WRTune(TEXT("passives.conductiveBonus"), 0.25f);
	}
	// Charged weapon: the blow carries the Charge it released (x 1 + 1.5 x the share of a full bar).
	if (Hit.bWeaponArt && Attacker->HasTag(Ability_ChargedWeapon))
	{
		Scale *= 1.f + WRTune(TEXT("wildbrands.charged_weapon.damagePerCharge"), 1.5f) * StoredCharge(Attacker);
	}
	return Scale;
}

float WRLightning::MoveSpeedScale(const AWRCharacterBase* Who)
{
	if (!Who || Who->IsCreature() || !WRLightningImpl::Walks(Who))
	{
		return 1.f;
	}
	// Swift (+3% a rank) and high Charge (+5%), all sources together capped at +25%.
	float Bonus = UWRPathComponent::Stat(Who, TEXT("moveSpeed"));
	if (ChargeValue(Who) >= WRTune(TEXT("resources.chargeHighAt"), 60.f))
	{
		Bonus += WRTune(TEXT("resources.chargeMoveBonusAtHigh"), 0.05f);
	}
	return 1.f + FMath::Clamp(Bonus, 0.f, WRTune(TEXT("passives.moveSpeedCap"), 0.25f));
}

float WRLightning::SwingRate(const AActor* Who)
{
	if (Who && ChargeValue(Who) >= WRTune(TEXT("passives.rapidFireAt"), 70.f) && UWRPathComponent::HasNode(Who, WRLightningImpl::PathId, TEXT("rapid_fire")))
	{
		return FMath::Max(0.1f, WRTune(TEXT("passives.rapidFireAnimRate"), 1.12f));
	}
	return 1.f;
}

float WRLightning::SpellCooldownScale(const AActor* Who)
{
	if (Who && ChargeValue(Who) >= WRTune(TEXT("passives.rapidFireAt"), 70.f) && UWRPathComponent::HasNode(Who, WRLightningImpl::PathId, TEXT("rapid_fire")))
	{
		return FMath::Max(0.05f, WRTune(TEXT("passives.rapidFireCooldownMult"), 0.85f));
	}
	return 1.f;
}

void WRLightning::SetStoredCharge(const AActor* By, float Fraction)
{
	if (By)
	{
		WRLightningImpl::Stored().Add(By, FMath::Clamp(Fraction, 0.f, 1.f));
	}
}

float WRLightning::StoredCharge(const AActor* By)
{
	return By ? WRLightningImpl::Stored().FindRef(By) : 0.f;
}

// ---------------------------------------------------------------- shared hits

void WRLightning::Zap(AWRCharacterBase* By, AWRCharacterBase* Target, float Damage, float Shock, const FVector& Origin, bool bSpell, bool bReaction, float StanceDamage, float Poise)
{
	if (!Target || !Target->IsAlive() || !Target->HasAuthority())
	{
		return;
	}
	FWRHit Hit;
	Hit.Instigator = By;
	Hit.Damage = Damage;
	Hit.PoiseDamage = Poise;
	Hit.Element = WRTags::Element_Lightning;
	if (Shock > 0.f) Hit.Buildups.Add(TEXT("shock"), Shock);
	Hit.PathId = WRLightningImpl::PathId;
	Hit.bSpell = bSpell;
	Hit.bReaction = bReaction;
	Hit.StanceDamage = StanceDamage;
	Hit.Impact = 1;
	Hit.Origin = Origin;
	Hit.ImpactPoint = Target->GetActorLocation();
	const FVector Feet = Target->GetActorLocation() - FVector(0.f, 0.f, Target->GetSimpleCollisionHalfHeight());
	UWorld* ZapWorld = Target->GetWorld();
	UWRCombatLibrary::ApplyHit(Target, Hit);
	// The Mother's Throat: lightning on a body standing in her water is lightning in the water (the rods, Vharra's
	// convulse: WRSwampVharra.cpp). Nothing happens anywhere else.
	FWRSwampBoss::LightningAt(ZapWorld, Feet, Damage, By, false);
}

void WRLightning::Ribbon(const UObject* WorldContext, const FVector& A, const FVector& B, bool bSound)
{
	const FVector D = B - A;
	if (D.IsNearlyZero())
	{
		return;
	}
	// The ribbon recipe lays its sparks along X over 100 cm x scale: scale = the gap in metres.
	WRFx::Broadcast(WorldContext, TEXT("lightning.arc"), A, D.Rotation(), D.Size() / 100.f, bSound ? FName(TEXT("storm.arc")) : NAME_None);
}

void WRLightning::Chain(AWRCharacterBase* By, AWRCharacterBase* From, int32 Jumps, float Radius, float Damage, float Shock, TSet<AWRCharacterBase*>& Already, bool bReaction)
{
	if (!By || !From)
	{
		return;
	}
	AWRCharacterBase* Cur = From;
	Already.Add(From);
	for (int32 i = 0; i < Jumps; ++i)
	{
		AWRCharacterBase* Next = nullptr;
		float Best = Radius;
		for (TActorIterator<AWRCharacterBase> It(Cur->GetWorld()); It; ++It)
		{
			AWRCharacterBase* Who = *It;
			if (Who == By || Already.Contains(Who) || !Who->IsAlive() || Who->IsHidden() || !By->IsHostileTo(Who))
			{
				continue;
			}
			const float D = FVector::Dist(Who->GetActorLocation(), Cur->GetActorLocation()) - Who->GetSimpleCollisionRadius();
			if (D < Best)
			{
				Best = D;
				Next = Who;
			}
		}
		if (!Next)
		{
			break;
		}
		Ribbon(By, Cur->GetActorLocation() + FVector(0.f, 0.f, 30.f), Next->GetActorLocation() + FVector(0.f, 0.f, 30.f), i == 0);
		Already.Add(Next);
		Zap(By, Next, Damage, Shock, Cur->GetActorLocation(), true, bReaction);
		Cur = Next;
	}
}

bool WRLightning::CallBolt(AWRCharacterBase* By, const FVector& Spot, float Damage, float Shock, float Tell, float Radius)
{
	if (!By || !By->HasAuthority())
	{
		return false;
	}
	// Down to the ground under the spot, so the glow sits on it.
	FVector Ground = Spot;
	FHitResult Floor;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(WRLightningBolt), false);
	if (By->GetWorld()->LineTraceSingleByChannel(Floor, Spot + FVector(0.f, 0.f, 150.f), Spot - FVector(0.f, 0.f, 600.f), ECC_WorldStatic, Query))
	{
		Ground = Floor.ImpactPoint;
	}
	// Thunder god's quick bolts: their own short violet ground glow under the target, so other players can read them.
	if (Tell > 0.f && Tell < WRTune(TEXT("passives.quickBoltGlowBelow"), 0.5f))
	{
		WRFx::Broadcast(By, TEXT("lightning.groundglow"), Ground, FRotator::ZeroRotator, Radius / 100.f, NAME_None, Tell);
	}
	FWRStrikeCall Call;
	Call.Spot = Ground;
	Call.PreTell = 0.f;
	Call.Tell = Tell;
	Call.Damage = Damage;
	Call.Shock = Shock;
	Call.Stun = 0.f;
	Call.Radius = Radius;
	Call.Source = EWRStrikeSource::Player;
	Call.Instigator = By;
	Call.bRodsTake = false;
	Call.bGroundEffects = false;
	return WRStorm::CallStrike(By, Call);
}

// ---------------------------------------------------------------- procs on a landed blow

void WRLightning::AfterHit(AWRCharacterBase* Target, const FWRHit& Hit, float Damage)
{
	AWRCharacterBase* Attacker = Cast<AWRCharacterBase>(Hit.Instigator);
	if (!Target || !Attacker || Attacker == Target || Hit.bDamageOverTime || !Target->HasAuthority())
	{
		return;
	}
	const FWRTuning& T = FWRTuning::Get();
	const bool bLightning = Hit.Element == WRTags::Element_Lightning;
	if (bLightning)
	{
		// Every lightning hit: a 0.08 s violet-white flash on the body and a crack over the weapon's own sound.
		WRFx::BroadcastOn(Target, TEXT("lightning.hit"), NAME_None, 1.f, Hit.bReaction ? NAME_None : FName(TEXT("storm.spark")));
		// Conductive: steam crackles off a soaked body.
		if (Target->HasTag(WRTags::Status_Soak) && UWRPathComponent::HasNode(Attacker, WRLightningImpl::PathId, TEXT("conductive")))
		{
			WRFx::BroadcastOn(Target, TEXT("lightning.steam"), NAME_None, 1.f);
		}
	}
	// Stormheart (Kharvox's boss part, socketed): every fifth weapon hit chains to a second enemy within 6 m for 40% of
	// the hit (the Scarred one 50%), drawn as an arc.
	if (Hit.bMelee && !Hit.bReaction && Attacker->GetPlayerState())
	{
		const float Share = UWRInventoryComponent::GearStatFor(Attacker, TEXT("chainEveryFifth"));
		if (Share > 0.f)
		{
			static TMap<TWeakObjectPtr<AActor>, int32> Count;
			int32& N = Count.FindOrAdd(Attacker);
			if (++N >= (int32)T.Num(TEXT("gear.stormheartEvery"), 5.f))
			{
				N = 0;
				AWRCharacterBase* Best = nullptr;
				float BestD = T.Num(TEXT("gear.stormheartRange"), 600.f);
				for (TActorIterator<AWRCharacterBase> It(Target->GetWorld()); It; ++It)
				{
					if (*It == Target || *It == Attacker || !It->IsAlive() || It->GetPlayerState()) continue;
					const float D = FVector::Dist(It->GetActorLocation(), Target->GetActorLocation());
					if (D < BestD) { BestD = D; Best = *It; }
				}
				if (Best)
				{
					FWRHit Chain;
					Chain.Instigator = Attacker;
					Chain.Damage = Damage * Share;
					Chain.Element = WRTags::Element_Lightning;
					Chain.Origin = Target->GetActorLocation();
					Chain.bReaction = true;
					Chain.Impact = 1;
					WRFx::Broadcast(Target, TEXT("lightning.arc"), Target->GetActorLocation(), (Best->GetActorLocation() - Target->GetActorLocation()).Rotation(), BestD / 300.f, TEXT("storm.arc"));
					UWRCombatLibrary::ApplyHit(Best, Chain);
				}
			}
		}
	}
	if (!Target->IsAlive())
	{
		return;
	}

	// Shock chance (trunk): each weapon hit may add a free burst of shock buildup.
	if (Hit.bMelee && !Hit.bReaction && Attacker->GetPlayerState())
	{
		const float Chance = UWRPathComponent::Stat(Attacker, TEXT("shockChance"));
		if (Chance > 0.f && FMath::FRand() < Chance)
		{
			Target->Status->AddStatusBuildup(WRStatus::Shock, T.Num(TEXT("passives.shockChanceBuildup"), 25.f), Attacker, Damage);
			if (!Target->IsAlive()) return;
		}
	}

	// Static strike: the thrust arcs to one more enemy within 4 m at 40% (+1 a rank of Arc).
	if (Hit.bWeaponArt && Hit.bMelee && !Hit.bReaction && Attacker->HasTag(Ability_StaticStrike))
	{
		const int32 Jumps = FMath::RoundToInt(T.Num(TEXT("wildbrands.static_strike.arc.targets"), 1.f) + UWRPathComponent::Stat(Attacker, TEXT("extraJumps")));
		const float Share = T.Num(TEXT("wildbrands.static_strike.arc.share"), 0.4f);
		TSet<AWRCharacterBase*> Already;
		Chain(Attacker, Target, Jumps, T.Num(TEXT("wildbrands.static_strike.arc.radius"), 400.f), Damage * Share,
			T.Num(TEXT("wildbrands.static_strike.buildup.shock"), 45.f) * Share, Already, true);
		if (!Target->IsAlive()) return;
	}

	// Charged weapon: the released Charge's shock (30 + 50 x the share of a full bar).
	if (Hit.bWeaponArt && Hit.bMelee && Attacker->HasTag(Ability_ChargedWeapon))
	{
		const float Pct = StoredCharge(Attacker);
		Target->Status->AddStatusBuildup(WRStatus::Shock, T.Num(TEXT("wildbrands.charged_weapon.shockBase"), 30.f) + T.Num(TEXT("wildbrands.charged_weapon.shockPerCharge"), 50.f) * Pct, Attacker, Damage);
		WRFx::BroadcastOn(Target, TEXT("lightning.discharge"), NAME_None, 0.6f + Pct, TEXT("storm.strike"));
		if (!Target->IsAlive()) return;
	}

	// Thunder god: every weapon hit calls a bolt down on the target (its 0.15 s ground glow first, so others read it).
	if (Hit.bMelee && !Hit.bReaction && Attacker->HasTag(Status_ThunderGod))
	{
		CallBolt(Attacker, WRLightningImpl::FeetOf(Target), T.Num(TEXT("spells.thunderGod.damage"), 20.f), T.Num(TEXT("spells.thunderGod.shock"), 20.f),
			T.Num(TEXT("spells.thunderGod.tell"), 0.15f), T.Num(TEXT("spells.thunderGod.radius"), 150.f));
	}

	// Plasma (Fire + Lightning 25/25): fire on a shocked body arcs to one enemy within 5 m as burning lightning.
	if (Hit.Element == WRTags::Element_Fire && !Hit.bReaction && Attacker->HasTag(Combo_Plasma)
		&& (Target->HasTag(WRTags::Status_Shock) || (Target->BuildupPct.IsValidIndex(WRStatus::Shock) && Target->BuildupPct[WRStatus::Shock] > 0)))
	{
		const FWRComboRegistry& C = FWRComboRegistry::Get();
		int32 Left = FMath::RoundToInt(C.Num(TEXT("plasma"), TEXT("targets"), 1.f));
		const float Radius = C.Num(TEXT("plasma"), TEXT("radius"), 500.f);
		TArray<TPair<float, AWRCharacterBase*>> Near;
		for (TActorIterator<AWRCharacterBase> It(Target->GetWorld()); It; ++It)
		{
			const float D = FVector::Dist(It->GetActorLocation(), Target->GetActorLocation());
			if (*It != Target && *It != Attacker && It->IsAlive() && Attacker->IsHostileTo(*It) && D < Radius)
			{
				Near.Add({ D, *It });
			}
		}
		Near.Sort([](const TPair<float, AWRCharacterBase*>& A, const TPair<float, AWRCharacterBase*>& B) { return A.Key < B.Key; });
		for (const TPair<float, AWRCharacterBase*>& N : Near)
		{
			if (Left-- <= 0) break;
			WRFx::Broadcast(Target, TEXT("plasma.arc"), Target->GetActorLocation(), (N.Value->GetActorLocation() - Target->GetActorLocation()).Rotation(),
				N.Key / 100.f, TEXT("storm.arc"));
			FWRHit Arc;
			Arc.Instigator = Attacker;
			Arc.Damage = Damage * C.Num(TEXT("plasma"), TEXT("damageShare"), 0.5f);
			Arc.PoiseDamage = 8.f;
			Arc.Element = WRTags::Element_Fire;
			Arc.BurnBuildup = C.Num(TEXT("plasma"), TEXT("burn"), 30.f);
			Arc.bReaction = true;
			Arc.bSpell = true;
			Arc.Impact = 1;
			Arc.Origin = Target->GetActorLocation();
			UWRCombatLibrary::ApplyHit(N.Value, Arc);
		}
	}
}

void WRLightning::OnGuard(AWRCharacterBase* Target, AWRCharacterBase* Attacker, bool bParry)
{
	// Discharge: a blocked blow releases a shock around you (3 m); a parry doubles it. Once every 2 s.
	if (!Target || !Target->HasAuthority() || !UWRPathComponent::HasNode(Target, WRLightningImpl::PathId, TEXT("discharge")))
	{
		return;
	}
	static TMap<TWeakObjectPtr<AActor>, double> Last;
	const double Now = Target->GetWorld()->GetTimeSeconds();
	if (Now - Last.FindRef(Target) < WRTune(TEXT("passives.dischargeCooldown"), 2.f) && Last.Contains(Target))
	{
		return;
	}
	Last.Add(Target, Now);
	const float Mult = bParry ? WRTune(TEXT("passives.dischargeParryMult"), 2.f) : 1.f;
	const float Radius = WRTune(TEXT("passives.dischargeRadius"), 300.f);
	// The block spark turns violet.
	WRFx::BroadcastOn(Target, TEXT("lightning.blockspark"), TEXT("weapon"), bParry ? 1.4f : 1.f, TEXT("storm.arc"));
	TArray<AWRCharacterBase*> Caught;
	for (TActorIterator<AWRCharacterBase> It(Target->GetWorld()); It; ++It)
	{
		if (*It != Target && It->IsAlive() && Target->IsHostileTo(*It) && FVector::Dist(It->GetActorLocation(), Target->GetActorLocation()) < Radius + It->GetSimpleCollisionRadius())
		{
			Caught.Add(*It);
		}
	}
	for (AWRCharacterBase* Who : Caught)
	{
		Zap(Target, Who, WRTune(TEXT("passives.dischargeDamage"), 8.f) * Mult, WRTune(TEXT("passives.dischargeShock"), 30.f) * Mult, Target->GetActorLocation(), false, true);
	}
	UE_LOG(LogWildreach, Log, TEXT("LIGHTNING discharge by %s (%s, %d caught)"), *Target->GetName(), bParry ? TEXT("parry") : TEXT("block"), Caught.Num());
}

void WRLightning::OnOverheatHit(AWRCharacterBase* Caster, AWRCharacterBase* Victim)
{
	// Meltdown (Fire + Lightning 50/50): each body the Overheat blast hits takes a chain that jumps once more.
	if (!Caster || !Victim || !Caster->HasTag(Combo_Meltdown) || !Victim->IsAlive())
	{
		return;
	}
	const FWRComboRegistry& C = FWRComboRegistry::Get();
	const float Damage = C.Num(TEXT("meltdown"), TEXT("damage"), 20.f), Shock = C.Num(TEXT("meltdown"), TEXT("shock"), 40.f);
	Ribbon(Caster, Caster->GetActorLocation(), Victim->GetActorLocation(), true);
	Zap(Caster, Victim, Damage, Shock, Caster->GetActorLocation(), true, true);
	TSet<AWRCharacterBase*> Already;
	Already.Add(Victim);
	Chain(Caster, Victim, FMath::RoundToInt(C.Num(TEXT("meltdown"), TEXT("jumps"), 1.f)), C.Num(TEXT("meltdown"), TEXT("jumpRadius"), 500.f), Damage, Shock, Already, true);
}

void WRLightning::OnShockProc(AWRCharacterBase* Owner, AWRCharacterBase* Source)
{
	// Brittle shock (Frost + Lightning 25/25): a shock on a frozen body shatters it (30 burst, stance x2). Bosses are
	// rimed, never frozen, so this only finds creatures and players.
	if (!Owner || !Source || !Source->HasTag(Combo_BrittleShock) || !Owner->HasTag(WRTags::Status_Frozen) || !Owner->IsAlive())
	{
		return;
	}
	const FWRComboRegistry& C = FWRComboRegistry::Get();
	Owner->Status->ClearStatus(WRTags::Status_Frozen);
	const float Damage = C.Num(TEXT("brittle_shock"), TEXT("damage"), WRTune(TEXT("status.shatterDamage"), 30.f));
	FWRHit Shatter;
	Shatter.Instigator = Source;
	Shatter.Damage = Damage;
	Shatter.PoiseDamage = 30.f;
	Shatter.Element = WRTags::Element_Frost;
	Shatter.StanceDamage = Damage * WRTune(TEXT("stance.spellPerDamage"), 0.3f) * C.Num(TEXT("brittle_shock"), TEXT("stanceMult"), 2.f);
	Shatter.bReaction = true;
	Shatter.bHeavy = true;
	Shatter.Impact = 2;
	Shatter.Tier = EWRHitTier::Heavy;
	Shatter.Origin = Owner->GetActorLocation();
	WRFx::Broadcast(Owner, TEXT("frost.impact"), Owner->GetActorLocation(), FRotator(90.f, 0.f, 0.f), 1.6f, TEXT("frost.impact"));
	UE_LOG(LogWildreach, Log, TEXT("LIGHTNING brittle shock shatters %s"), *Owner->GetName());
	UWRCombatLibrary::ApplyHit(Owner, Shatter);
}

void WRLightning::TickOwner(AWRCharacterBase* Who, float DeltaTime)
{
	if (!Who || !Who->HasAuthority() || !Who->GetPlayerState() || !Who->IsAlive())
	{
		return;
	}
	UWorld* World = Who->GetWorld();
	const double Now = World->GetTimeSeconds();

	// Afterimage: a dodge leaves a spark where it began; afterimageDelay s later it bursts (at most one a second).
	if (UWRPathComponent::HasNode(Who, WRLightningImpl::PathId, TEXT("afterimage")))
	{
		static TMap<TWeakObjectPtr<AWRCharacterBase>, double> SeenDodge, LastSpark;
		const double Seen = SeenDodge.FindRef(Who);
		if (Who->DodgeStartedAt > Seen + 0.01 && Now - Who->DodgeStartedAt < 0.5)
		{
			SeenDodge.Add(Who, Who->DodgeStartedAt);
			if (!LastSpark.Contains(Who) || Now - LastSpark.FindRef(Who) >= WRTune(TEXT("passives.afterimageCooldown"), 1.f))
			{
				LastSpark.Add(Who, Now);
				const FVector At = Who->GetActorLocation();
				const float Delay = WRTune(TEXT("passives.afterimageDelay"), 0.6f);
				WRFx::Broadcast(Who, TEXT("lightning.afterimage"), At, Who->GetActorRotation(), 1.f, TEXT("storm.charge_whine"), Delay);
				TWeakObjectPtr<AWRCharacterBase> Weak(Who);
				FTimerHandle Timer;
				World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateLambda([Weak, At]()
				{
					AWRCharacterBase* C = Weak.Get();
					if (!C || !C->IsAlive()) return;
					const float Radius = WRTune(TEXT("passives.afterimageRadius"), 200.f);
					WRFx::Broadcast(C, TEXT("lightning.burst"), At, FRotator::ZeroRotator, Radius / 100.f, TEXT("storm.strike"));
					TArray<AWRCharacterBase*> Caught;
					for (TActorIterator<AWRCharacterBase> It(C->GetWorld()); It; ++It)
					{
						if (*It != C && It->IsAlive() && C->IsHostileTo(*It) && FVector::Dist(It->GetActorLocation(), At) < Radius + It->GetSimpleCollisionRadius())
						{
							Caught.Add(*It);
						}
					}
					for (AWRCharacterBase* Victim : Caught)
					{
						WRLightning::Zap(C, Victim, WRTune(TEXT("passives.afterimageDamage"), 10.f), WRTune(TEXT("passives.afterimageShock"), 30.f), At, false, true);
					}
				}), Delay, false);
			}
		}
	}

	// Blizzard storm (Frost + Lightning 50/50): while your Blizzard runs, a strike every 1.5 s on a random enemy in it.
	if (Who->HasTag(Combo_BlizzardStorm))
	{
		static TMap<TWeakObjectPtr<AWRHazard>, float> Clocks;
		const FWRComboRegistry& C = FWRComboRegistry::Get();
		const float Every = FMath::Max(0.2f, C.Num(TEXT("blizzard_storm"), TEXT("every"), 1.5f));
		for (TActorIterator<AWRHazard> It(World); It; ++It)
		{
			AWRHazard* Storm = *It;
			if (Storm->Source.Get() != Who || Storm->Kind != TEXT("frost"))
			{
				continue;
			}
			float& Clock = Clocks.FindOrAdd(Storm);
			Clock += DeltaTime;
			if (Clock < Every)
			{
				continue;
			}
			Clock = 0.f;
			TArray<AWRCharacterBase*> Inside;
			for (TActorIterator<AWRCharacterBase> W(World); W; ++W)
			{
				if (*W != Who && W->IsAlive() && Who->IsHostileTo(*W) && FVector::Dist2D(W->GetActorLocation(), Storm->GetActorLocation()) < Storm->Radius)
				{
					Inside.Add(*W);
				}
			}
			if (Inside.Num() > 0)
			{
				AWRCharacterBase* Pick = Inside[FMath::RandRange(0, Inside.Num() - 1)];
				CallBolt(Who, WRLightningImpl::FeetOf(Pick), C.Num(TEXT("blizzard_storm"), TEXT("damage"), 25.f), C.Num(TEXT("blizzard_storm"), TEXT("shock"), 40.f),
					C.Num(TEXT("blizzard_storm"), TEXT("tell"), 1.f), C.Num(TEXT("blizzard_storm"), TEXT("radius"), 250.f));
			}
		}
		for (auto ClockIt = Clocks.CreateIterator(); ClockIt; ++ClockIt)
		{
			if (!ClockIt->Key.IsValid()) ClockIt.RemoveCurrent();
		}
	}
}
