// The Arcane path's rules (see WRArcanePath.h; cooks/biome-crystal.md 2.17-2.18, build-biome-crystal.md P2.9).

#include "WRArcanePath.h"
#include "Wildreach.h"
#include "WRAttributeSet.h"
#include "WRCharacter.h"
#include "WRCharacterBase.h"
#include "WRCombatLibrary.h"
#include "WRCombos.h"
#include "WRCreature.h"
#include "WRCrystalTypes.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRInventory.h"
#include "WRGearPerks.h"
#include "WRLightningPath.h"
#include "WRNaturePath.h"
#include "WRNecroPath.h"
#include "WRPathComponent.h"
#include "WRPathTypes.h"
#include "WRPlayerController.h"
#include "WRProjectile.h"
#include "WRSkyAir.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRTuning.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace WRArcane
{
	UE_DEFINE_GAMEPLAY_TAG(Passive_DeepWell, "Passive.DeepWell");
	UE_DEFINE_GAMEPLAY_TAG(Status_Shielded, "Status.Arcane.Shielded");
	UE_DEFINE_GAMEPLAY_TAG(Status_Inscribed, "Status.Arcane.Inscribed");
	UE_DEFINE_GAMEPLAY_TAG(Status_InCircle, "Status.Arcane.InCircle");
	UE_DEFINE_GAMEPLAY_TAG(Combo_ManaFlame, "Combo.ManaFlame");
	UE_DEFINE_GAMEPLAY_TAG(Combo_MeteorStorm, "Combo.MeteorStorm");
	UE_DEFINE_GAMEPLAY_TAG(Combo_QuickMind, "Combo.QuickMind");
	UE_DEFINE_GAMEPLAY_TAG(Combo_ManaWings, "Combo.ManaWings");
	UE_DEFINE_GAMEPLAY_TAG(Combo_RuneVolley, "Combo.RuneVolley");
	UE_DEFINE_GAMEPLAY_TAG(Combo_FrostMissile, "Combo.FrostMissile");
	UE_DEFINE_GAMEPLAY_TAG(Combo_FrozenRune, "Combo.FrozenRune");
	UE_DEFINE_GAMEPLAY_TAG(Combo_BloodMagic, "Combo.BloodMagic");
	UE_DEFINE_GAMEPLAY_TAG(Combo_BloodStar, "Combo.BloodStar");
	UE_DEFINE_GAMEPLAY_TAG(Combo_ManaSap, "Combo.ManaSap");
	UE_DEFINE_GAMEPLAY_TAG(Combo_FaerieRing, "Combo.FaerieRing");
	UE_DEFINE_GAMEPLAY_TAG(Combo_BoundDead, "Combo.BoundDead");
}

namespace WRArcaneImpl
{
	const FName ArPathId(TEXT("arcane"));
	float ArT(const TCHAR* Key, float Fallback) { return FWRTuning::Get().Num(FString::Printf(TEXT("arcane.%s"), Key), Fallback); }
	double ArNow(const UObject* W) { return W && W->GetWorld() ? W->GetWorld()->GetTimeSeconds() : 0.0; }
	bool ArTagged(const AActor* Who, const FGameplayTag& Tag)
	{
		const AWRCharacterBase* C = Cast<AWRCharacterBase>(Who);
		return C && C->HasTag(Tag);
	}
	float ArCombo(FName Combo, FName Key, float Fallback) { return FWRComboRegistry::Get().Num(Combo, Key, Fallback); }
	TMap<TWeakObjectPtr<const AActor>, double>& ArRefusals() { static TMap<TWeakObjectPtr<const AActor>, double> M; return M; }
	FVector ArChest(const AActor* A) { return A->GetActorLocation() + FVector(0.f, 0.f, 30.f); }

	/** A shield's server state: until when, whose (their Reflect and Absorb ride with it). */
	struct FArShield
	{
		double Until = 0.0;
		TWeakObjectPtr<AWRCharacterBase> Source;
		bool bReflect = false;
		int32 Absorb = 0;
	};
	TMap<TWeakObjectPtr<AWRCharacterBase>, FArShield>& ArShields() { static TMap<TWeakObjectPtr<AWRCharacterBase>, FArShield> M; return M; }
	/** The Twin shard talisman: when each wearer last dodged (a blow landed within the window echoes once). */
	TMap<TWeakObjectPtr<const AActor>, double>& ArDodgedAt() { static TMap<TWeakObjectPtr<const AActor>, double> M; return M; }

	void ArSetAttr(AWRCharacterBase* Who, const FGameplayAttribute& Attr, float V)
	{
		if (UAbilitySystemComponent* ASC = Who ? Who->GetAbilitySystemComponent() : nullptr) ASC->SetNumericAttributeBase(Attr, V);
	}
	/** hero.md 2.6's strength (smoothstep 0.08..1 of points / max points), from the server's path record. */
	float ArStrength(const AActor* Who)
	{
		const UWRPathComponent* P = UWRPathComponent::FindFor(Who);
		const FWRPathProgress* G = P && P->IsActivePath(ArPathId) ? P->GetProgress(ArPathId) : nullptr;
		if (!G) return 0.f;
		const float Max = FMath::Max(1.f, WRTune(TEXT("paths.maxPointsPerPath"), 100.f));
		const float S = FMath::SmoothStep(0.08f, 1.f, FMath::Clamp(FWRPathRules::PointsForXP(G->XP) / Max, 0.f, 1.f));
		return P->GetMainPath() == ArPathId ? S : S * WRTune(TEXT("pathMarks.subScale"), 0.6f);
	}
}
using namespace WRArcaneImpl;

// ================================================================================================ the path

bool WRArcane::Walks(const AActor* Who)
{
	const UWRPathComponent* P = UWRPathComponent::FindFor(Who);
	return P && P->IsActivePath(ArPathId);
}

int32 WRArcane::Rank(const AActor* Who, FName Node)
{
	const UWRPathComponent* P = UWRPathComponent::FindFor(Who);
	const FWRPathProgress* G = P && P->IsActivePath(ArPathId) ? P->GetProgress(ArPathId) : nullptr;
	return G ? G->GetRank(Node) : 0;
}

int32 WRArcane::PointsIn(const AActor* Who)
{
	const UWRPathComponent* P = UWRPathComponent::FindFor(Who);
	return P && P->IsActivePath(ArPathId) ? P->GetSpent(ArPathId) : 0;
}

// ================================================================================================ Mana

float WRArcane::ManaOf(const AActor* Who)
{
	const AWRCharacterBase* C = Cast<AWRCharacterBase>(Who);
	const UWRAttributeSet* A = C ? C->GetAttributes() : nullptr;
	return A ? A->GetMana() : 0.f;
}

float WRArcane::MaxManaOf(const AActor* Who)
{
	const AWRCharacterBase* C = Cast<AWRCharacterBase>(Who);
	const UWRAttributeSet* A = C ? C->GetAttributes() : nullptr;
	return A ? A->GetMaxMana() : 100.f;
}

bool WRArcane::HasMana(const AActor* Who, float Amount)
{
	if (Amount <= 0.f) return true;
	const float Have = ManaOf(Who);
	if (Have + 0.01f >= Amount) return true;
	// Blood magic (Arcane + Blood 25): what's missing comes out of HP, 1 a 2 Mana, never the last of it.
	if (ArTagged(Who, Combo_BloodMagic))
	{
		const AWRCharacterBase* C = Cast<AWRCharacterBase>(Who);
		const UWRAttributeSet* A = C ? C->GetAttributes() : nullptr;
		const float Hp = (Amount - Have) / FMath::Max(0.1f, ArCombo(TEXT("blood_magic"), TEXT("manaPerHp"), 2.f));
		return A && A->GetHealth() > Hp + 1.f;
	}
	return false;
}

bool WRArcane::SpendMana(AWRCharacterBase* Who, float Amount)
{
	if (!Who || !Who->HasAuthority()) return false;
	if (Amount <= 0.f) return true;
	if (!HasMana(Who, Amount)) return false;
	const UWRAttributeSet* A = Who->GetAttributes();
	const float Have = A->GetMana();
	if (Have + 0.01f < Amount)
	{
		// Blood magic pays the rest in HP (a thin garnet thread from the chest).
		const float Hp = (Amount - Have) / FMath::Max(0.1f, ArCombo(TEXT("blood_magic"), TEXT("manaPerHp"), 2.f));
		ArSetAttr(Who, UWRAttributeSet::GetHealthAttribute(), FMath::Max(1.f, A->GetHealth() - Hp));
		ArSetAttr(Who, UWRAttributeSet::GetManaAttribute(), 0.f);
		WRFx::BroadcastOn(Who, TEXT("arcane.blood_magic"), TEXT("spine"), 1.f, TEXT("blood.cast"), 0.4f);
		return true;
	}
	ArSetAttr(Who, UWRAttributeSet::GetManaAttribute(), Have - Amount);
	return true;
}

void WRArcane::GainMana(AWRCharacterBase* Who, float Amount)
{
	const UWRAttributeSet* A = Who ? Who->GetAttributes() : nullptr;
	if (!A || !Who->HasAuthority() || Amount <= 0.f || !Walks(Who)) return;
	ArSetAttr(Who, UWRAttributeSet::GetManaAttribute(), FMath::Min(A->GetMaxMana(), A->GetMana() + Amount));
}

void WRArcane::NoteRefused(const AActor* Who)
{
	if (Who) ArRefusals().Add(Who, ArNow(Who));
}

float WRArcane::RefusedFlash(const AActor* Who)
{
	const double* At = Who ? ArRefusals().Find(Who) : nullptr;
	if (!At) return 0.f;
	const float Len = ArT(TEXT("refuseFlash"), 0.35f);
	const float Age = float(ArNow(Who) - *At);
	return Age >= 0.f && Age < Len ? 1.f - Age / Len : 0.f;
}

void WRArcane::TickOwner(AWRCharacterBase* Who, float DeltaTime)
{
	if (!Who || !Who->HasAuthority() || !Who->GetPlayerState()) return;
	const UWRAttributeSet* A = Who->GetAttributes();
	if (!A) return;
	if (!Walks(Who))
	{
		if (A->GetMana() > 0.f) ArSetAttr(Who, UWRAttributeSet::GetManaAttribute(), 0.f);
		return;
	}
	// Capacity 100, +15 a Mana capacity rank (2.17); an amethyst's room (pathResourceMax, WRGearPerks).
	const float Max = ArT(TEXT("manaMax"), 100.f) + ArT(TEXT("manaPerRank"), 15.f) * Rank(Who, TEXT("mana_capacity")) + WRGearPerks::ResourceMaxBonus(Who);
	if (!FMath::IsNearlyEqual(A->GetMaxMana(), Max))
	{
		ArSetAttr(Who, UWRAttributeSet::GetMaxManaAttribute(), Max);
		if (A->GetMana() > Max) ArSetAttr(Who, UWRAttributeSet::GetManaAttribute(), Max);
	}
	if (!Who->IsAlive()) return;
	// Steady: 6 a second at all times; Deep well (root, main path) x1.5.
	float Regen = ArT(TEXT("manaRegen"), 6.f);
	if (Who->HasTag(Passive_DeepWell)) Regen *= ArT(TEXT("deepWell"), 1.5f);
	// Mana sap (Nature + Arcane 25): +25% while Growth is over 50 (and Growth the other way, WRNaturePath).
	if (Who->HasTag(Combo_ManaSap) && WRNature::GrowthValue(Who) > ArCombo(TEXT("mana_sap"), TEXT("above"), 50.f)) Regen *= 1.f + ArCombo(TEXT("mana_sap"), TEXT("bonus"), 0.25f);
	// Mana wings (Wind + Arcane 25): +6 a second while gliding.
	if (Who->HasTag(Combo_ManaWings) && WRSkyAir::IsGliding(Who)) Regen += ArCombo(TEXT("mana_wings"), TEXT("perSecond"), 6.f);
	// Venn's ring (gear stat manaRegen: +15%).
	Regen *= 1.f + UWRInventoryComponent::GearStatFor(Who, TEXT("manaRegen"));
	// The Silent Vault keeps path resources still (crystal 2.5).
	if (WRCrystal::InVault(Who->GetActorLocation())) Regen = 0.f;
	if (A->GetMana() < Max && Regen > 0.f) ArSetAttr(Who, UWRAttributeSet::GetManaAttribute(), FMath::Min(Max, A->GetMana() + Regen * DeltaTime));
}

// ================================================================================================ shields

float WRArcane::ShieldOf(const AActor* Who)
{
	const AWRCharacterBase* C = Cast<AWRCharacterBase>(Who);
	const UWRAttributeSet* A = C ? C->GetAttributes() : nullptr;
	return A ? A->GetShield() : 0.f;
}

float WRArcane::ShieldScale(const AActor* Who)
{
	float S = 1.f + ArT(TEXT("hardenedPerRank"), 0.2f) * Rank(Who, TEXT("hardened_ward"));
	// The Tyrant plate talisman (crystal 2.13.3): Barrier and Aegis +20%.
	S *= 1.f + UWRInventoryComponent::GearStatFor(Who, TEXT("shieldBonus"));
	return S;
}

void WRArcane::GiveShield(AWRCharacterBase* Target, float Amount, float Seconds, AWRCharacterBase* Source)
{
	if (!Target || !Target->HasAuthority() || !Target->IsAlive() || Amount <= 0.f) return;
	const UWRAttributeSet* A = Target->GetAttributes();
	if (!A) return;
	// The bigger one wins; never stacked (2.17).
	FArShield& S = ArShields().FindOrAdd(Target);
	if (A->GetShield() >= Amount && ArNow(Target) < S.Until)
	{
		S.Until = FMath::Max(S.Until, ArNow(Target) + Seconds);
		return;
	}
	S.Until = ArNow(Target) + Seconds;
	S.Source = Source;
	S.bReflect = Source && Rank(Source, TEXT("reflect")) > 0;
	S.Absorb = Source ? Rank(Source, TEXT("absorb")) : 0;
	ArSetAttr(Target, UWRAttributeSet::GetShieldAttribute(), Amount);
	ArSetAttr(Target, UWRAttributeSet::GetMaxShieldAttribute(), Amount);
	if (UAbilitySystemComponent* ASC = Target->GetAbilitySystemComponent(); ASC && !ASC->HasMatchingGameplayTag(Status_Shielded))
	{
		ASC->AddLooseGameplayTag(Status_Shielded, 1, EGameplayTagReplicationState::TagOnly);
	}
	WRSound::PlayOn(Target, TEXT("arcane.shield_up"));
}

void WRArcane::DropShield(AWRCharacterBase* Target, bool bBroken)
{
	if (!Target || !Target->HasAuthority()) return;
	ArShields().Remove(Target);
	ArSetAttr(Target, UWRAttributeSet::GetShieldAttribute(), 0.f);
	ArSetAttr(Target, UWRAttributeSet::GetMaxShieldAttribute(), 0.f);
	if (UAbilitySystemComponent* ASC = Target->GetAbilitySystemComponent(); ASC && ASC->HasMatchingGameplayTag(Status_Shielded))
	{
		ASC->RemoveLooseGameplayTag(Status_Shielded, 1, EGameplayTagReplicationState::TagOnly);
	}
	if (bBroken) WRFx::Broadcast(Target, TEXT("arcane.shield_break"), ArChest(Target), FRotator::ZeroRotator, 1.f, TEXT("arcane.shield_break"));
}

void WRArcane::TickShields(UWorld* World, float Dt)
{
	// The Twin shard talisman: note who is dodging (its echo waits on their next blow).
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const AWRCharacterBase* C = It->Get() ? Cast<AWRCharacterBase>(It->Get()->GetPawn()) : nullptr;
		if (C && C->HasTag(WRTags::State_Dodging) && UWRInventoryComponent::GearStatFor(C, TEXT("dodgeEcho")) > 0.f) ArDodgedAt().Add(C, World->GetTimeSeconds());
	}
	// Every shield's clock (survivors and tames alike), and Mana flame's burn: a shield on someone burning near a
	// survivor with Fire + Arcane 25 loses 15 a second.
	const double T = World ? World->GetTimeSeconds() : 0.0;
	TArray<TWeakObjectPtr<AWRCharacterBase>> Gone;
	for (TPair<TWeakObjectPtr<AWRCharacterBase>, FArShield>& P : ArShields())
	{
		AWRCharacterBase* C = P.Key.Get();
		if (!C || C->GetWorld() != World) { if (!C) Gone.Add(P.Key); continue; }
		if (!C->IsAlive() || T >= P.Value.Until || WRArcane::ShieldOf(C) <= 0.f) { Gone.Add(P.Key); continue; }
		if (C->HasTag(WRTags::Status_Burn))
		{
			for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
			{
				if (!It->HasTag(WRArcane::Combo_ManaFlame) || !It->IsHostileTo(C) || FVector::Dist(It->GetActorLocation(), C->GetActorLocation()) > ArCombo(TEXT("mana_flame"), TEXT("reach"), 3000.f)) continue;
				ArSetAttr(C, UWRAttributeSet::GetShieldAttribute(), FMath::Max(0.f, WRArcane::ShieldOf(C) - ArCombo(TEXT("mana_flame"), TEXT("perSecond"), 15.f) * Dt));
				break;
			}
		}
	}
	for (const TWeakObjectPtr<AWRCharacterBase>& W : Gone)
	{
		if (AWRCharacterBase* C = W.Get()) WRArcane::DropShield(C, WRArcane::ShieldOf(C) <= 0.f && C->IsAlive());
		else ArShields().Remove(W);
	}
}

bool WRArcane::IsReflectable(const FWRHit& Hit)
{
	return Hit.bProjectile && Hit.bReflectable && !Hit.bReaction && !Hit.bDamageOverTime && !Hit.bMelee;
}

bool WRArcane::Reflect(AWRCharacterBase* Reflector, const FWRHit& Incoming, float Damage, float Scale, const FVector& At)
{
	if (!Reflector || !Reflector->HasAuthority() || !IsReflectable(Incoming)) return false;
	UWorld* World = Reflector->GetWorld();
	AActor* Shooter = Incoming.Instigator.Get();
	// Back along its incoming line: from where it struck toward where it was loosed (its origin), never turning.
	const FVector Back = !Incoming.Origin.IsZero() ? Incoming.Origin : (Shooter ? ArChest(Shooter) : At - Reflector->GetActorForwardVector() * 1000.f);
	const FVector Dir = (Back - At).GetSafeNormal();
	if (Dir.IsNearlyZero()) return false;
	FWRHit Hit = Incoming;
	Hit.Instigator = Reflector;
	Hit.Damage = Damage * Scale;
	Hit.Origin = At;
	Hit.bReaction = true;          // a thrown-back shot never bounces again
	Hit.bReflectable = false;
	Hit.ImpactPoint = FVector::ZeroVector;
	FActorSpawnParameters Params;
	Params.Owner = Reflector;
	Params.Instigator = Reflector;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector From = At + Dir * 60.f;
	if (AWRProjectile* Shot = World->SpawnActor<AWRProjectile>(AWRProjectile::StaticClass(), From, Dir.Rotation(), Params))
	{
		Shot->Init(Hit, ArT(TEXT("reflectSpeed"), 2600.f), FLinearColor(0.75f, 0.6f, 1.f));
		Shot->SetTrail(TEXT("arcane.missile"));
	}
	WRFx::Broadcast(Reflector, TEXT("crystal.reflect"), At, Dir.Rotation(), 1.f, TEXT("crystal.reflect"));
	return true;
}

// ================================================================================================ the combat hooks

bool WRArcane::BurnsShields(const AActor* Who, const FWRHit& Hit)
{
	return (Hit.bSpell || Hit.PathId == ArPathId) && Rank(Who, TEXT("mana_burn")) > 0;
}

float WRArcane::DamageScale(const AWRCharacterBase* Attacker, const AWRCharacterBase* Target, const FWRHit& Hit)
{
	if (!Attacker) return 1.f;
	float S = 1.f;
	if (Hit.PathId == ArPathId && Walks(Attacker))
	{
		// Focus: +5% Arcane spell damage a rank. Mark chance: +10% a rank from your spells on the marked.
		if (Hit.bSpell) S *= 1.f + ArT(TEXT("focusPerRank"), 0.05f) * Rank(Attacker, TEXT("focus"));
		if (Hit.bSpell && Target && Target->HasTag(WRTags::Status_Mark)) S *= 1.f + ArT(TEXT("markSpellPerRank"), 0.1f) * Rank(Attacker, TEXT("mark_chance"));
	}
	// An ally's Rune circle: +15% damage while you stand in it.
	if (Attacker->HasTag(Status_InCircle)) S *= 1.f + ArT(TEXT("circleAllyDamage"), 0.15f);
	return S;
}

float WRArcane::Incoming(AWRCharacterBase* Target, const FWRHit& Hit, float Damage)
{
	if (!Target || !Target->HasAuthority() || Damage <= 0.f) return Damage;
	const float Shield = ShieldOf(Target);
	if (Shield <= 0.f) return Damage;
	FArShield* S = ArShields().Find(Target);
	const FVector At = !Hit.ImpactPoint.IsZero() ? Hit.ImpactPoint : ArChest(Target);
	// Reflect: a reflectable shot bounces straight back at x0.8; the shield takes nothing from it.
	if (S && S->bReflect && IsReflectable(Hit) && Reflect(Target, Hit, Damage, ArT(TEXT("reflectScale"), 0.8f), At))
	{
		return 0.f;
	}
	// Mana burn: spells eat shields twice as fast.
	const float Eat = BurnsShields(Hit.Instigator.Get(), Hit) ? ArT(TEXT("manaBurnShield"), 2.f) : 1.f;
	const float Taken = FMath::Min(Shield, Damage * Eat);
	ArSetAttr(Target, UWRAttributeSet::GetShieldAttribute(), Shield - Taken);
	// Absorb: 15% of what the shield took comes back to its caster as Mana, a rank.
	if (S && S->Absorb > 0)
	{
		if (AWRCharacterBase* Src = S->Source.Get()) GainMana(Src, Taken * ArT(TEXT("absorbPerRank"), 0.15f) * S->Absorb);
	}
	WRFx::Broadcast(Target, TEXT("arcane.shield_hit"), At, FRotator::ZeroRotator, 1.f, TEXT("arcane.shield_hit"));
	if (Shield - Taken <= 0.01f) DropShield(Target, true);
	return FMath::Max(0.f, Damage - Taken / Eat);
}

bool WRArcane::BlocksBuildup(const AWRCharacterBase* Target)
{
	return ShieldOf(Target) > 0.f;
}

void WRArcane::ScaleBuildups(const AWRCharacterBase* Attacker, FWRHit& Hit)
{
	if (!Attacker) return;
	// Mark chance: +20% mark buildup from your hits a rank.
	if (float* Mark = Hit.Buildups.Find(TEXT("mark")); Mark && Walks(Attacker))
	{
		*Mark *= 1.f + ArT(TEXT("markBuildupPerRank"), 0.2f) * Rank(Attacker, TEXT("mark_chance"));
	}
	// Frost missile (Arcane + Frost 25): Arcane missiles chill.
	if (Attacker->HasTag(Combo_FrostMissile) && Hit.PathId == ArPathId && Hit.bSpell && Hit.bProjectile)
	{
		Hit.Buildups.FindOrAdd(TEXT("frost")) += ArCombo(TEXT("frost_missile"), TEXT("frost"), 15.f);
	}
}

void WRArcane::AfterHit(AWRCharacterBase* Target, const FWRHit& Hit, float Damage)
{
	AWRCharacterBase* By = Cast<AWRCharacterBase>(Hit.Instigator.Get());
	if (!Target || !By || !Target->IsAlive() || Hit.bReaction || Hit.bDamageOverTime) return;
	// Inscribe: the rune on your blade adds arcane to each weapon blow (+10% a Glyph power rank).
	if (Hit.bMelee && By->HasTag(Status_Inscribed))
	{
		FWRHit Rune;
		Rune.Instigator = By;
		Rune.Damage = WRTune(TEXT("spells.inscribe.perHit"), 8.f) * (1.f + ArT(TEXT("glyphPerRank"), 0.1f) * Rank(By, TEXT("glyph_power")));
		Rune.Element = WRTags::Element_Arcane;
		Rune.PathId = ArPathId;
		Rune.bReaction = true;
		Rune.Origin = !Hit.ImpactPoint.IsZero() ? Hit.ImpactPoint : By->GetActorLocation();
		Rune.ImpactPoint = Hit.ImpactPoint;
		Rune.Impact = 0;
		UWRCombatLibrary::ApplyHit(Target, Rune);
	}
	// The Twin shard talisman (the Mirrored Twins' lens): the first blow you land after a dodge lands twice, the second
	// at 40%, half a breath later.
	if (const double* At = ArDodgedAt().Find(By); At && By->GetWorld()->GetTimeSeconds() - *At <= WRTune(TEXT("crystalLife.twinShardWindow"), 1.5f))
	{
		ArDodgedAt().Remove(By);
		const float Share = UWRInventoryComponent::GearStatFor(By, TEXT("dodgeEcho"));
		FWRHit Echo = Hit;
		Echo.Damage = Damage * Share;
		Echo.bReaction = true;
		Echo.Buildups.Reset();
		TWeakObjectPtr<AWRCharacterBase> WeakT(Target);
		FTimerHandle H;
		By->GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([WeakT, Echo]()
		{
			if (AWRCharacterBase* T = WeakT.Get(); T && T->IsAlive()) { UWRCombatLibrary::ApplyHit(T, Echo); WRFx::Broadcast(T, TEXT("crystal.double_pass"), T->GetActorLocation(), FRotator::ZeroRotator, 1.f, TEXT("crystal.boss.double_pass")); }
		}), 0.12f, false);
	}
	// Mana burn on a survivor: each spell hit drains 8 of their path resource.
	if (Hit.bSpell && Target->GetPlayerState() && Rank(By, TEXT("mana_burn")) > 0)
	{
		const float Drain = ArT(TEXT("manaBurnDrain"), 8.f);
		if (Walks(Target))
		{
			ArSetAttr(Target, UWRAttributeSet::GetManaAttribute(), FMath::Max(0.f, ManaOf(Target) - Drain));
		}
		WRLightning::SpendCharge(Target, Drain);
		if (WRNecro::Walks(Target)) WRNecro::SpendSouls(Target, FMath::Min(WRNecro::SoulsOf(Target), Drain * ArT(TEXT("manaBurnSoulShare"), 0.25f)));
	}
}

// ================================================================================================ casting

float WRArcane::WindupScale(const AActor* Caster)
{
	float S = 1.f - ArT(TEXT("quickCastPerRank"), 0.08f) * Rank(Caster, TEXT("quick_cast"));
	// Quick mind (Lightning + Arcane 25): -15% while Charge is over 60.
	if (ArTagged(Caster, Combo_QuickMind) && WRLightning::ChargeValue(Caster) > ArCombo(TEXT("quick_mind"), TEXT("above"), 60.f))
	{
		S *= 1.f - ArCombo(TEXT("quick_mind"), TEXT("windup"), 0.15f);
	}
	return FMath::Max(0.1f, S);
}

void WRArcane::OnCast(AWRCharacterBase* Caster)
{
	if (!Caster || !Caster->HasAuthority()) return;
	// The body at full strength (2.18): a soft crystal chime on every cast and the fingertips' violet trace in the air.
	if (ArStrength(Caster) >= ArT(TEXT("fullLookAt"), 0.95f))
	{
		WRFx::BroadcastOn(Caster, TEXT("arcane.finger_trace"), TEXT("hand_r"), 1.f, TEXT("arcane.cast_chime"), ArT(TEXT("traceSeconds"), 0.4f));
	}
}

// ================================================================================================ the Prismglass spear

void WRArcane::PrismLines(AWRCharacterBase* Wielder, float BlowDamage)
{
	if (!Wielder || !Wielder->HasAuthority()) return;
	UWorld* World = Wielder->GetWorld();
	const float Reach = WRTune(TEXT("arcane.spear.reach"), 520.f);
	const float Share = WRTune(TEXT("arcane.spear.share"), 0.4f);
	const FVector From = Wielder->GetActorLocation() + FVector(0.f, 0.f, 40.f);
	const float Yaw = Wielder->GetActorRotation().Yaw;
	for (const float Off : { -1.f, 1.f })
	{
		const FVector Dir = FRotator(0.f, Yaw + Off * WRTune(TEXT("arcane.spear.angle"), 15.f), 0.f).Vector();
		const FVector To = From + Dir * Reach;
		WRFx::Broadcast(Wielder, TEXT("arcane.prism_line"), From, Dir.Rotation(), Reach / 100.f, Off < 0.f ? FName(TEXT("arcane.prism_line")) : FName(NAME_None));
		// The first body in the line takes it (a line of light, not a projectile: walls stop it).
		FHitResult Wall;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRPrismLine), false, Wielder);
		const float Clear = World->LineTraceSingleByChannel(Wall, From, To, ECC_WorldStatic, Q) ? Wall.Distance : Reach;
		AWRCharacterBase* Best = nullptr;
		float BestT = Clear;
		for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
		{
			AWRCharacterBase* C = *It;
			if (C == Wielder || !C->IsAlive() || !Wielder->IsHostileTo(C)) continue;
			const FVector P = C->GetActorLocation() - From;
			const float T = float(FVector::DotProduct(P, Dir));
			if (T < 0.f || T > BestT) continue;
			if ((P - Dir * T).Size2D() > C->GetSimpleCollisionRadius() + 30.f || FMath::Abs(P.Z) > C->GetSimpleCollisionHalfHeight() + 60.f) continue;
			Best = C;
			BestT = T;
		}
		if (!Best) continue;
		FWRHit Hit;
		Hit.Instigator = Wielder;
		Hit.Damage = BlowDamage * Share;
		Hit.Element = WRTags::Element_Arcane;
		Hit.Origin = From;
		Hit.Impact = 1;
		Hit.bReaction = true;
		Hit.Buildups.Add(TEXT("mark"), WRTune(TEXT("arcane.spear.mark"), 25.f));
		UWRCombatLibrary::ApplyHit(Best, Hit);
	}
}

// ================================================================================================ the body

uint32 WRArcane::LookSig(const AActor* Hero)
{
	return ArTagged(Hero, Status_Shielded) ? 1u : 0u;
}

void WRArcane::ApplyLook(const AActor* Hero, const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Skin, FName Main, float MainS, FName Sub, float SubS)
{
	// A shield's facets catch the rune lines: while one holds, the Arcane marks glow at least at the 0.6 resting level.
	if ((Main != ArPathId && Sub != ArPathId) || !ArTagged(Hero, Status_Shielded)) return;
	const FString Prefix = Main == ArPathId ? TEXT("Main") : TEXT("Sub");
	const float S = FMath::Max(Main == ArPathId ? MainS : SubS, ArT(TEXT("shieldLookFloor"), 0.6f));
	for (UMaterialInstanceDynamic* M : Skin) if (M) M->SetScalarParameterValue(FName(*(Prefix + TEXT("Strength"))), S);
}
