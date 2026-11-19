#include "WRGearPerks.h"
#include "Wildreach.h"
#include "WRArcanePath.h"
#include "WRArtifacts.h"
#include "WRAttributeSet.h"
#include "WRBoss.h"
#include "WRCharacterBase.h"
#include "WRCombatLibrary.h"
#include "WRCombatTypes.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRInventory.h"
#include "WRNecroPath.h"
#include "WRStatusComponent.h"
#include "WRTuning.h"
#include "WRWindPath.h"
#include "AbilitySystemComponent.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "GameFramework/PlayerState.h"

// This file's own helpers (not in the header).
namespace WRGearPerks
{
	float Stat(const AActor* Who, const TCHAR* Name) { return Who ? UWRInventoryComponent::GearStatFor(Who, FName(Name)) : 0.f; }
	float Tn(const TCHAR* Key, float Fallback) { return FWRTuning::Get().Num(FString(TEXT("gear.")) + Key, Fallback); }
	double Now(const AActor* Who) { const UWorld* W = Who ? Who->GetWorld() : nullptr; return W ? W->GetTimeSeconds() : 0.0; }
	/** A spell blow (the combat library's own test: a spell, or a path blow that isn't a swing). */
	bool IsSpell(const FWRHit& Hit) { return Hit.bSpell || (!Hit.bMelee && !Hit.PathId.IsNone()); }

	/** What the perks remember about one survivor's body. */
	struct FState
	{
		double DodgedAt = -1000.0;    // afterDodge: the last dodge (cleared by the blow that uses it)
		double DecoyAt = -1000.0;     // dodgeDecoy: the last mirage
		double FullCastAt = -1000.0;  // the last path spell, when it was cast from a full bar
		double KillHealAt = -1000.0;  // killHeal's cooldown
		TArray<double> KillStacks;    // the Mourner's lantern: when each stack runs out
		float StillFor = 0.f;         // stillRegen
		float ResourceMax = 0.f;      // pathResourceMax already in the builder bars
	};
	TMap<TWeakObjectPtr<const AActor>, FState>& States() { static TMap<TWeakObjectPtr<const AActor>, FState> M; return M; }
	FState& For(const AActor* Who) { return States().FindOrAdd(Who); }

	/** A landed blow again at Share: buildups and poise with it, never parried, reflected, doubled or split again. */
	FWRHit Copy(const FWRHit& Hit, float Share)
	{
		FWRHit C = Hit;
		C.Damage = Hit.Damage * Share;
		C.PoiseDamage = Hit.PoiseDamage * Share;
		if (C.StanceDamage > 0.f) C.StanceDamage *= Share;
		C.BurnBuildup *= Share;
		C.FrostBuildup *= Share;
		for (TPair<FName, float>& B : C.Buildups) B.Value *= Share;
		C.Torpor *= Share;
		C.bReaction = true;
		C.bParryable = false;
		C.bReflectable = false;
		C.JudgeTime = 0.0;
		C.ImpactPoint = FVector::ZeroVector;
		return C;
	}
}

void WRGearPerks::OnDodge(AWRCharacterBase* Who)
{
	if (!Who || !Who->HasAuthority() || !Who->GetPlayerState()) return;
	const double T = Now(Who);
	// The Tooth of Ssarrok, the Echo stone: the next blow inside gear.afterDodgeSeconds lands harder (DealtBonus).
	if (Stat(Who, TEXT("afterDodge")) > 0.f) For(Who).DodgedAt = T;
	// The Mirage charm, the Mirage antler: a shadowless copy where the roll began that the creatures on you strike at
	// instead (they lose you for its seconds). Bosses see through it. Once every gear.decoyCooldown s.
	const float Decoy = Stat(Who, TEXT("dodgeDecoy"));
	if (Decoy <= 0.f) return;
	FState& S = For(Who);
	if (T - S.DecoyAt < Tn(TEXT("decoyCooldown"), 5.f)) return;
	S.DecoyAt = T;
	const FVector At = Who->GetActorLocation();
	const float Range = Tn(TEXT("decoyRange"), 1500.f);
	int32 Drawn = 0;
	for (TActorIterator<AWRCreature> It(Who->GetWorld()); It; ++It)
	{
		if (!It->IsAlive() || It->IsTamed() || It->GetAggroTarget() != Who || Cast<AWRBoss>(*It)) continue;
		if (const FWRCreatureDef* Def = It->GetDef(); Def && Def->Archetype == TEXT("boss")) continue;
		if (FVector::Dist(It->GetActorLocation(), At) > Range) continue;
		It->LoseSightOf(Who, Decoy);
		++Drawn;
	}
	WRFx::Broadcast(Who, TEXT("desert.mirage_shimmer"), At, Who->GetActorRotation(), 1.f, NAME_None, Decoy);
	UE_LOG(LogWildreach, Log, TEXT("GEAR %s's mirage draws %d creature(s) for %.1f s"), *Who->GetName(), Drawn, Decoy);
}

void WRGearPerks::OnAirDodge(AWRCharacterBase* Who)
{
	if (!Who || !Who->HasAuthority()) return;
	// The Gale plume: everything hostile within the radius thrown back airDodgePush cm (WRWind::Knock: never bosses,
	// creatures by their tier).
	const float Push = Stat(Who, TEXT("airDodgePush"));
	if (Push <= 0.f) return;
	const FVector At = Who->GetActorLocation();
	const float Radius = Tn(TEXT("airDodgePushRadius"), 400.f);
	TArray<AWRCharacterBase*> Near;
	for (TActorIterator<AWRCharacterBase> It(Who->GetWorld()); It; ++It)
	{
		if (*It == Who || !It->IsAlive() || !Who->IsHostileTo(*It)) continue;
		if (FVector::Dist(It->GetActorLocation(), At) <= Radius + It->GetSimpleCollisionRadius()) Near.Add(*It);
	}
	for (AWRCharacterBase* V : Near)
	{
		WRWind::Knock(Who, V, V->GetActorLocation() - At, Push, Tn(TEXT("airDodgePushSeconds"), 0.35f), TEXT("gale_plume"));
	}
	if (Near.Num() > 0) UE_LOG(LogWildreach, Log, TEXT("GEAR %s's gale plume throws %d back"), *Who->GetName(), Near.Num());
}

void WRGearPerks::OnParry(AWRCharacterBase* Who)
{
	// Rune of Warding: the parry puts a ward on you (the shared shield: the bigger one wins, never stacked).
	const float Ward = Stat(Who, TEXT("perfectGuardShield"));
	if (!Who || !Who->HasAuthority() || Ward <= 0.f) return;
	WRArcane::GiveShield(Who, Ward, Tn(TEXT("guardShieldSeconds"), 3.f), nullptr);
}

bool WRGearPerks::ResourceFull(const AWRCharacterBase* Who, FName PathId)
{
	const UWRAttributeSet* A = Who ? Who->GetAttributes() : nullptr;
	if (!A || PathId.IsNone()) return false;
	float Cur = 0.f, Max = 0.f;
	const FString P = PathId.ToString();
	if (P == TEXT("fire")) { Cur = A->GetHeat(); Max = A->GetMaxHeat(); }
	else if (P == TEXT("frost")) { Cur = A->GetChill(); Max = A->GetMaxChill(); }
	else if (P == TEXT("lightning")) { Cur = A->GetCharge(); Max = A->GetMaxCharge(); }
	else if (P == TEXT("nature")) { Cur = A->GetGrowth(); Max = A->GetMaxGrowth(); }
	else if (P == TEXT("wind")) { Cur = A->GetMomentum(); Max = A->GetMaxMomentum(); }
	else if (P == TEXT("necromancy")) { Cur = A->GetSouls(); Max = A->GetMaxSouls(); }
	else if (P == TEXT("arcane")) { Cur = A->GetMana(); Max = A->GetMaxMana(); }
	else if (P == TEXT("tide")) { Cur = A->GetFlow(); Max = A->GetMaxFlow(); }
	else if (P == TEXT("sun")) { Cur = A->GetRadiance(); Max = A->GetMaxRadiance(); }
	else if (P == TEXT("spore")) { Cur = A->GetSpores(); Max = A->GetMaxSpores(); }
	else if (P == TEXT("void")) { Cur = A->GetVoid(); Max = A->GetMaxVoid(); }
	else if (P == TEXT("blood")) { Cur = A->GetHealth(); Max = A->GetMaxHealth(); }   // Blood's resource is your health
	return Max > 0.f && Cur >= Max - 0.5f;
}

void WRGearPerks::NoteCast(AWRCharacterBase* Who, bool bFull)
{
	if (!Who || !Who->HasAuthority() || !Who->GetPlayerState()) return;
	if (!bFull && !States().Contains(Who)) return;
	For(Who).FullCastAt = bFull ? Now(Who) : -1000.0;
}

bool WRGearPerks::FullResourceCrit(const AWRCharacterBase* Attacker, const FWRHit& Hit, float& OutMult)
{
	// The Prism heart: a spell cast with its path's bar full always crits, at the heart's own multiplier (two hearts
	// never past gear.spellCritMultMax).
	if (!Attacker || !IsSpell(Hit)) return false;
	const FState* S = States().Find(Attacker);
	if (!S || Now(Attacker) - S->FullCastAt > Tn(TEXT("fullCastSeconds"), 4.f)) return false;
	if (Stat(Attacker, TEXT("spellCritFullResource")) <= 0.f) return false;
	const float Mult = Stat(Attacker, TEXT("spellCritMult"));
	OutMult = FMath::Clamp(Mult > 0.f ? Mult : 1.5f, 1.f, Tn(TEXT("spellCritMultMax"), 2.f));
	return true;
}

float WRGearPerks::DealtBonus(AWRCharacterBase* Attacker, const AWRCharacterBase* Target, const FWRHit& Hit)
{
	if (!Attacker || !Target || !Attacker->HasAuthority()) return 0.f;
	float Gear = 0.f;
	const double T = Now(Attacker);
	FState* S = States().Find(Attacker);
	// The Tooth of Ssarrok, the Echo stone: the first blow inside gear.afterDodgeSeconds of a dodge.
	if (S && !Hit.bReaction && T - S->DodgedAt <= Tn(TEXT("afterDodgeSeconds"), 2.f))
	{
		S->DodgedAt = -1000.0;
		Gear += Stat(Attacker, TEXT("afterDodge"));
	}
	// The Kite crown: a ranged blow loosed from gear.fromAboveCm or more above the target.
	if (Hit.bProjectile && !Hit.bMelee && Attacker->GetActorLocation().Z - Target->GetActorLocation().Z >= Tn(TEXT("fromAboveCm"), 150.f))
	{
		Gear += Stat(Attacker, TEXT("rangedFromAbove"));
	}
	// The Mother's fang: swarms (creatures.json role or archetype "swarm").
	if (const AWRCreature* Body = Cast<AWRCreature>(Target); Body && Body->GetDef()
		&& (Body->GetDef()->Archetype == TEXT("swarm") || Body->GetDef()->Role.Contains(TEXT("swarm"))))
	{
		Gear += Stat(Attacker, TEXT("vsSwarm"));
	}
	// The Mourning ring: Necromancy's blows (its element, or its spells and Wildbrands).
	if (Hit.Element == WRNecro::Element_Necromancy || Hit.PathId == TEXT("necromancy"))
	{
		Gear += Stat(Attacker, TEXT("necroDamage"));
	}
	// The Mourner's lantern: a share for each kill stack still burning.
	if (S && S->KillStacks.Num() > 0)
	{
		S->KillStacks.RemoveAll([T](double Until) { return Until <= T; });
		Gear += Stat(Attacker, TEXT("killStackDamage")) * S->KillStacks.Num();
	}
	return Gear;
}

void WRGearPerks::AfterHit(AWRCharacterBase* Target, const FWRHit& Hit)
{
	AWRCharacterBase* By = Cast<AWRCharacterBase>(Hit.Instigator.Get());
	if (!Target || !By || By == Target || !By->HasAuthority() || !By->GetPlayerState() || Hit.bReaction || Hit.bDamageOverTime) return;
	// Rune of Doubling: a doubleHit chance the blow lands again at gear.doubleHitShare, a breath later.
	if (Target->IsAlive() && FMath::FRand() < Stat(By, TEXT("doubleHit")))
	{
		const FWRHit Again = Copy(Hit, Tn(TEXT("doubleHitShare"), 0.5f));
		TWeakObjectPtr<AWRCharacterBase> WeakT(Target);
		FTimerHandle H;
		By->GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([WeakT, Again]()
		{
			if (AWRCharacterBase* V = WeakT.Get(); V && V->IsAlive())
			{
				UWRCombatLibrary::ApplyHit(V, Again);
				WRFx::Broadcast(V, TEXT("crystal.double_pass"), V->GetActorLocation(), FRotator::ZeroRotator, 0.6f);
			}
		}), FMath::Max(0.01f, Tn(TEXT("doubleHitDelay"), 0.12f)), false);
	}
	// Rune of the Prism: a spell blow splits a spellSplit copy onto the nearest other enemy within gear.spellSplitRange.
	const float Split = IsSpell(Hit) ? Stat(By, TEXT("spellSplit")) : 0.f;
	if (Split <= 0.f) return;
	AWRCharacterBase* Second = nullptr;
	float Best = Tn(TEXT("spellSplitRange"), 600.f);
	for (TActorIterator<AWRCharacterBase> It(Target->GetWorld()); It; ++It)
	{
		if (*It == Target || *It == By || !It->IsAlive() || !By->IsHostileTo(*It)) continue;
		const float D = FVector::Dist(It->GetActorLocation(), Target->GetActorLocation());
		if (D < Best) { Best = D; Second = *It; }
	}
	if (!Second) return;
	FWRHit Shard = Copy(Hit, Split);
	Shard.Origin = Target->GetActorLocation();
	Shard.bProjectile = false;
	UWRCombatLibrary::ApplyHit(Second, Shard);
}

void WRGearPerks::OnKill(AWRCharacterBase* Target, const FWRHit& Hit)
{
	AWRCharacterBase* By = Cast<AWRCharacterBase>(Hit.Instigator.Get());
	if (!Target || !By || By == Target || !By->HasAuthority() || !By->IsAlive() || !By->GetPlayerState()) return;
	const double T = Now(By);
	// The Mire heart: a kill on a bleeding body (its bleed building, or just burst) heals killHeal of your max health,
	// once every gear.killHealCooldown s.
	const float Heal = Stat(By, TEXT("killHeal"));
	const bool bBleeding = Target->HasTag(WRTags::Status_Bleeding)
		|| (Target->BuildupPct.IsValidIndex(WRStatus::Bleed) && Target->BuildupPct[WRStatus::Bleed] > 0);
	if (Heal > 0.f && bBleeding && T - For(By).KillHealAt >= Tn(TEXT("killHealCooldown"), 2.f))
	{
		For(By).KillHealAt = T;
		UWRAttributeSet* A = By->GetAttributes();
		UAbilitySystemComponent* ASC = By->GetAbilitySystemComponent();
		if (A && ASC && By->Status)
		{
			const float Amount = WRArtifacts::HealFilter(By, A->GetMaxHealth() * Heal * By->Status->HealingTakenScale(), false);
			ASC->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), FMath::Min(A->GetMaxHealth(), A->GetHealth() + Amount));
			UE_LOG(LogWildreach, Log, TEXT("GEAR %s's mire heart heals %.0f from %s"), *By->GetName(), Amount, *Target->GetName());
		}
	}
	// The Mourner's lantern: each kill adds a stack for killStackSeconds, killStackMax at most (the oldest goes first).
	if (Stat(By, TEXT("killStackDamage")) > 0.f)
	{
		FState& S = For(By);
		const int32 Max = FMath::Max(1, FMath::RoundToInt(Stat(By, TEXT("killStackMax"))));
		S.KillStacks.RemoveAll([T](double Until) { return Until <= T; });
		S.KillStacks.Add(T + FMath::Max(1.f, Stat(By, TEXT("killStackSeconds"))));
		S.KillStacks.Sort();
		while (S.KillStacks.Num() > Max) S.KillStacks.RemoveAt(0);
	}
}

float WRGearPerks::SpellCostScale(const AActor* Who)
{
	return Who ? FMath::Max(0.f, 1.f + Stat(Who, TEXT("spellCost"))) : 1.f;
}

float WRGearPerks::ResourceMaxBonus(const AActor* Who)
{
	return Who ? FMath::Max(0.f, Stat(Who, TEXT("pathResourceMax"))) : 0.f;
}

void WRGearPerks::NoteResourceMax(const AWRCharacterBase* Who, float Bonus)
{
	if (Who) For(Who).ResourceMax = Bonus;
}

void WRGearPerks::TickPlayer(AWRCharacterBase* Who, float DeltaTime, float HealScale)
{
	if (!Who || !Who->HasAuthority() || !Who->GetPlayerState() || !Who->IsAlive()) return;
	UAbilitySystemComponent* ASC = Who->GetAbilitySystemComponent();
	UWRAttributeSet* A = Who->GetAttributes();
	if (!ASC || !A) return;
	// Bodies gone (a respawn, a world closed) leave their rows: swept now and then.
	static double LastSweep = 0.0;
	const double T = Now(Who);
	if (T < LastSweep || T - LastSweep > 30.0)
	{
		LastSweep = T;
		for (auto It = States().CreateIterator(); It; ++It) if (!It.Key().IsValid()) It.RemoveCurrent();
	}
	FState& S = For(Who);
	// An amethyst socketed or taken out: the builder bars (set in UWRPathComponent::ApplyToCharacter) grow or shrink by
	// the difference. The spender bars (Mana, Flow, Radiance, Spores, Souls) add it as they work theirs out each tick.
	const float Bonus = ResourceMaxBonus(Who);
	if (!FMath::IsNearlyEqual(Bonus, S.ResourceMax))
	{
		const float Delta = Bonus - S.ResourceMax;
		S.ResourceMax = Bonus;
		const auto Grow = [ASC, Delta](const FGameplayAttribute& MaxAttr, const FGameplayAttribute& CurAttr, float MaxNow, float CurNow)
		{
			const float NewMax = FMath::Max(1.f, MaxNow + Delta);
			ASC->SetNumericAttributeBase(MaxAttr, NewMax);
			if (CurNow > NewMax) ASC->SetNumericAttributeBase(CurAttr, NewMax);
		};
		Grow(UWRAttributeSet::GetMaxHeatAttribute(), UWRAttributeSet::GetHeatAttribute(), A->GetMaxHeat(), A->GetHeat());
		Grow(UWRAttributeSet::GetMaxChillAttribute(), UWRAttributeSet::GetChillAttribute(), A->GetMaxChill(), A->GetChill());
		Grow(UWRAttributeSet::GetMaxChargeAttribute(), UWRAttributeSet::GetChargeAttribute(), A->GetMaxCharge(), A->GetCharge());
		Grow(UWRAttributeSet::GetMaxGrowthAttribute(), UWRAttributeSet::GetGrowthAttribute(), A->GetMaxGrowth(), A->GetGrowth());
		Grow(UWRAttributeSet::GetMaxMomentumAttribute(), UWRAttributeSet::GetMomentumAttribute(), A->GetMaxMomentum(), A->GetMomentum());
	}
	// The Heartguard's four: stood still gear.stillSeconds, stillRegen HP a second.
	const float Still = Stat(Who, TEXT("stillRegen"));
	if (Still <= 0.f)
	{
		S.StillFor = 0.f;
		return;
	}
	S.StillFor = Who->GetVelocity().SizeSquared() < FMath::Square(Tn(TEXT("stillSpeed"), 20.f)) ? S.StillFor + DeltaTime : 0.f;
	if (S.StillFor >= Tn(TEXT("stillSeconds"), 2.f) && A->GetHealth() < A->GetMaxHealth())
	{
		const float Amount = WRArtifacts::HealFilter(Who, Still * DeltaTime * HealScale, false);
		ASC->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), FMath::Min(A->GetMaxHealth(), A->GetHealth() + Amount));
	}
}
