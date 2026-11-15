// The Necromancy path's rules (see WRNecroPath.h; cooks/biome-graveyard.md 2.17-2.18, build-biome-graveyard.md 2.18).

#include "WRNecroPath.h"
#include "Wildreach.h"
#include "WRAttributeSet.h"
#include "WRBloodPath.h"
#include "WRCharacter.h"
#include "WRCharacterBase.h"
#include "WRCombatLibrary.h"
#include "WRCombos.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRGraveCreatures.h"
#include "WRGraveTypes.h"
#include "WRPathComponent.h"
#include "WRPlayerController.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRSurvival.h"
#include "WRTuning.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"

namespace WRNecro
{
	UE_DEFINE_GAMEPLAY_TAG(Element_Necromancy, "Element.Necromancy");
	UE_DEFINE_GAMEPLAY_TAG(Passive_Deathless, "Passive.Deathless");
	UE_DEFINE_GAMEPLAY_TAG(Status_DeathlessSpent, "Status.Necro.DeathlessSpent");
	UE_DEFINE_GAMEPLAY_TAG(Status_DeathMark, "Status.Necro.DeathMark");
	UE_DEFINE_GAMEPLAY_TAG(Status_LichForm, "Status.Necro.LichForm");
	UE_DEFINE_GAMEPLAY_TAG(Status_Raised, "Status.Necro.Raised");
	UE_DEFINE_GAMEPLAY_TAG(Combo_AshWraith, "Combo.AshWraith");
	UE_DEFINE_GAMEPLAY_TAG(Combo_FuneralPyre, "Combo.FuneralPyre");
	UE_DEFINE_GAMEPLAY_TAG(Combo_Preserved, "Combo.Preserved");
	UE_DEFINE_GAMEPLAY_TAG(Combo_FrozenLegion, "Combo.FrozenLegion");
	UE_DEFINE_GAMEPLAY_TAG(Combo_JoltOfLife, "Combo.JoltOfLife");
	UE_DEFINE_GAMEPLAY_TAG(Combo_StormLich, "Combo.StormLich");
	UE_DEFINE_GAMEPLAY_TAG(Combo_BloodBond, "Combo.BloodBond");
	UE_DEFINE_GAMEPLAY_TAG(Combo_DeathCurse, "Combo.DeathCurse");
	UE_DEFINE_GAMEPLAY_TAG(Combo_CycleOfLife, "Combo.CycleOfLife");
	UE_DEFINE_GAMEPLAY_TAG(Combo_RotAndBloom, "Combo.RotAndBloom");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(Status_RaiseFlare, "Status.Necro.RaiseFlare");
}

namespace WRNecroImpl
{
	const FName NcPathId(TEXT("necromancy"));
	float NcT(const TCHAR* Key, float Fallback) { return FWRTuning::Get().Num(FString::Printf(TEXT("necromancy.%s"), Key), Fallback); }
	double NcNow(const UObject* W) { return W && W->GetWorld() ? W->GetWorld()->GetTimeSeconds() : 0.0; }
	int32 NcRank(const AActor* Who, FName Node)
	{
		const UWRPathComponent* P = UWRPathComponent::FindFor(Who);
		const FWRPathProgress* G = P && P->IsActivePath(NcPathId) ? P->GetProgress(NcPathId) : nullptr;
		return G ? G->GetRank(Node) : 0;
	}
	bool NcHas(const AActor* Who, FName Node) { return NcRank(Who, Node) > 0; }
	bool NcTagged(const AActor* Who, const FGameplayTag& Tag)
	{
		const AWRCharacterBase* C = Cast<AWRCharacterBase>(Who);
		return C && C->HasTag(Tag);
	}
	float NcComboNum(FName Combo, FName Key, float Fallback) { return FWRComboRegistry::Get().Num(Combo, Key, Fallback); }
	TMap<TWeakObjectPtr<const AActor>, double>& NcRefusals() { static TMap<TWeakObjectPtr<const AActor>, double> M; return M; }

	/** A raised minion: whose, until when, its blows' scale, what it rose as. */
	struct FNcMinion
	{
		TWeakObjectPtr<AWRCharacterBase> Owner;
		double Until = 0.0, NextLook = 0.0;
		float DamageScale = 0.6f;
		bool bCapped = true;
		FName Variant;
	};
	TMap<TWeakObjectPtr<AWRCreature>, FNcMinion>& NcMinions() { static TMap<TWeakObjectPtr<AWRCreature>, FNcMinion> M; return M; }

	/** A body on the ground: when it fell, how (frozen, burning), how many butchering hits it had, used up yet. */
	struct FNcBody
	{
		double DiedAt = 0.0;
		bool bFrozen = false, bBurning = false, bUsed = false;
		uint8 HitsAtDeath = 0;
		/** A free raise waiting on it (Frozen legion, Death curse): for whom, how long, as what. */
		TWeakObjectPtr<AWRCharacterBase> FreeFor;
		float FreeSeconds = 0.f;
		FName FreeVariant;
	};
	TMap<TWeakObjectPtr<AWRCreature>, FNcBody>& NcBodies() { static TMap<TWeakObjectPtr<AWRCreature>, FNcBody> M; return M; }

	/** Who had a hand in a body's death (a Necromancer, or their minion for them): when they last hit it. */
	TMap<TWeakObjectPtr<AActor>, TMap<TWeakObjectPtr<AWRCharacterBase>, double>>& NcCredit()
	{
		static TMap<TWeakObjectPtr<AActor>, TMap<TWeakObjectPtr<AWRCharacterBase>, double>> M;
		return M;
	}
	TMap<TWeakObjectPtr<AWRCharacterBase>, float>& NcCarry() { static TMap<TWeakObjectPtr<AWRCharacterBase>, float> M; return M; }
	TMap<TWeakObjectPtr<AWRCharacterBase>, double>& NcSiphonAt() { static TMap<TWeakObjectPtr<AWRCharacterBase>, double> M; return M; }

	FVector NcChest(const AActor* A) { return A->GetActorLocation() + FVector(0.f, 0.f, 30.f); }
	void NcTell(const AActor* Who, const FString& Text)
	{
		const APawn* P = Cast<APawn>(Who);
		if (AWRPlayerController* PC = P ? Cast<AWRPlayerController>(P->GetController()) : nullptr) PC->ClientNotify(Text);
	}
	/** Server: heal Who by Amount (clamped to max). */
	void NcHeal(AWRCharacterBase* Who, float Amount)
	{
		UAbilitySystemComponent* ASC = Who ? Who->GetAbilitySystemComponent() : nullptr;
		const UWRAttributeSet* A = Who ? Who->GetAttributes() : nullptr;
		if (!ASC || !A || Amount <= 0.f || !Who->IsAlive()) return;
		ASC->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), FMath::Min(A->GetMaxHealth(), A->GetHealth() + Amount));
	}
	/** The minion's master, or the actor itself if it's a survivor. */
	AWRCharacterBase* NcCreditFor(AActor* By)
	{
		AWRCharacterBase* C = Cast<AWRCharacterBase>(By);
		if (!C) return nullptr;
		if (AWRCharacterBase* M = WRNecro::MasterOf(C)) return M;
		return C;
	}
	/** Souls a death is worth (2.17: lowland 1, midland 2, highland 3, Risen 1, mini 10, major 25, a player 3). */
	float NcWorth(const AWRCharacterBase* Target)
	{
		if (Target->GetPlayerState()) return NcT(TEXT("souls.player"), 3.f);
		const AWRCreature* Cr = Cast<AWRCreature>(Target);
		const FWRCreatureDef* Def = Cr ? Cr->GetDef() : nullptr;
		if (!Def) return 1.f;
		if (Def->Archetype == TEXT("boss")) return Def->Role.Contains(TEXT("major")) ? NcT(TEXT("souls.major"), 25.f) : NcT(TEXT("souls.mini"), 10.f);
		if (UWRGraveBody::IsRisen(Def->Id)) return NcT(TEXT("souls.risen"), 1.f);
		if (Def->Tier == TEXT("highland")) return NcT(TEXT("souls.highland"), 3.f);
		if (Def->Tier == TEXT("midland")) return NcT(TEXT("souls.midland"), 2.f);
		return NcT(TEXT("souls.lowland"), 1.f);
	}
	/** The Graveyard's corruption buff to a Necromancer's path damage (2.17: +3/6/10/15% at 25/50/75/100). */
	float NcCorruptionBuff(const AWRCharacterBase* Who)
	{
		if (!WRGrave::Live() || !WRGrave::Contains(Who->GetActorLocation())) return 1.f;
		const UWRSurvivalComponent* S = UWRSurvivalComponent::For(Who);
		const float C = S ? S->GetCorruption() : 0.f;
		return 1.f + (C >= 100.f ? NcT(TEXT("corruption.at100"), 0.15f) : C >= 75.f ? NcT(TEXT("corruption.at75"), 0.1f) : C >= 50.f ? NcT(TEXT("corruption.at50"), 0.06f) : C >= 25.f ? NcT(TEXT("corruption.at25"), 0.03f) : 0.f);
	}
}
using namespace WRNecroImpl;

// ================================================================================================ Souls

bool WRNecro::Walks(const AActor* Who)
{
	const UWRPathComponent* P = UWRPathComponent::FindFor(Who);
	return P && P->IsActivePath(NcPathId);
}

float WRNecro::SoulsOf(const AActor* Who)
{
	const AWRCharacterBase* C = Cast<AWRCharacterBase>(Who);
	const UWRAttributeSet* A = C ? C->GetAttributes() : nullptr;
	return A ? A->GetSouls() : 0.f;
}

float WRNecro::MaxSoulsOf(const AActor* Who)
{
	const AWRCharacterBase* C = Cast<AWRCharacterBase>(Who);
	const UWRAttributeSet* A = C ? C->GetAttributes() : nullptr;
	return A ? A->GetMaxSouls() : 20.f;
}

bool WRNecro::HasSouls(const AActor* Who, float Amount)
{
	return Amount <= 0.f || NcTagged(Who, Status_LichForm) || SoulsOf(Who) + 0.01f >= Amount;
}

bool WRNecro::SpendSouls(AWRCharacterBase* Who, float Amount)
{
	UAbilitySystemComponent* ASC = Who && Who->HasAuthority() ? Who->GetAbilitySystemComponent() : nullptr;
	const UWRAttributeSet* A = Who ? Who->GetAttributes() : nullptr;
	if (!ASC || !A) return false;
	if (Amount <= 0.f || Who->HasTag(Status_LichForm)) return true;   // Lich form: spells cost no Souls
	if (A->GetSouls() + 0.01f < Amount) return false;
	ASC->SetNumericAttributeBase(UWRAttributeSet::GetSoulsAttribute(), FMath::Max(0.f, A->GetSouls() - Amount));
	return true;
}

void WRNecro::RefundSouls(AWRCharacterBase* Who, float Amount)
{
	UAbilitySystemComponent* ASC = Who ? Who->GetAbilitySystemComponent() : nullptr;
	const UWRAttributeSet* A = Who ? Who->GetAttributes() : nullptr;
	if (!ASC || !A || Amount <= 0.f || Who->HasTag(Status_LichForm)) return;
	ASC->SetNumericAttributeBase(UWRAttributeSet::GetSoulsAttribute(), FMath::Min(A->GetMaxSouls(), A->GetSouls() + Amount));
}

void WRNecro::GainSouls(AWRCharacterBase* Who, float Amount, const FVector& From, bool bBright)
{
	UAbilitySystemComponent* ASC = Who && Who->HasAuthority() ? Who->GetAbilitySystemComponent() : nullptr;
	const UWRAttributeSet* A = Who ? Who->GetAttributes() : nullptr;
	if (!ASC || !A || Amount <= 0.f || !Who->IsAlive() || !Walks(Who)) return;
	// Fractions carry over (Harvest's +20% a rank).
	float& C = NcCarry().FindOrAdd(Who);
	C += Amount;
	const int32 Whole = FMath::FloorToInt(C + 0.001f);
	if (Whole <= 0) return;
	C -= Whole;
	ASC->SetNumericAttributeBase(UWRAttributeSet::GetSoulsAttribute(), FMath::Min(A->GetMaxSouls(), A->GetSouls() + Whole));
	// A pale-green wisp arcs from the body to the chest (from up to 15 m), a soft breath in.
	const FVector To = NcChest(Who);
	const FVector D = To - From;
	if (D.Size() < NcT(TEXT("wispReach"), 1500.f) * 3.f)
	{
		WRFx::Broadcast(Who, bBright ? FName(TEXT("necro.soul_wisp_bright")) : FName(TEXT("necro.soul_wisp")), From, D.Rotation(), FMath::Max(0.5f, D.Size() / 100.f), NAME_None);
	}
	WRSound::Broadcast(Who, TEXT("necro.soul_gain"));
}

void WRNecro::NoteRefused(const AActor* Who)
{
	if (Who) NcRefusals().Add(Who, NcNow(Who));
}

float WRNecro::RefusedFlash(const AActor* Who)
{
	const double* At = Who ? NcRefusals().Find(Who) : nullptr;
	if (!At) return 0.f;
	const float Len = NcT(TEXT("refuseFlash"), 0.35f);
	const float Age = float(NcNow(Who) - *At);
	return Age >= 0.f && Age < Len ? 1.f - Age / Len : 0.f;
}

void WRNecro::TickOwner(AWRCharacterBase* Who, float DeltaTime)
{
	if (!Who || !Who->HasAuthority() || !Who->GetPlayerState()) return;
	UAbilitySystemComponent* ASC = Who->GetAbilitySystemComponent();
	const UWRAttributeSet* A = Who->GetAttributes();
	if (!ASC || !A) return;
	if (!Walks(Who))
	{
		if (A->GetSouls() > 0.f) ASC->SetNumericAttributeBase(UWRAttributeSet::GetSoulsAttribute(), 0.f);
		return;
	}
	// Max Souls 20, +5 a Soul capacity rank.
	const float Max = NcT(TEXT("soulsMax"), 20.f) + NcT(TEXT("soulCapacityPerRank"), 5.f) * NcRank(Who, TEXT("soul_capacity"));
	if (!FMath::IsNearlyEqual(A->GetMaxSouls(), Max))
	{
		ASC->SetNumericAttributeBase(UWRAttributeSet::GetMaxSoulsAttribute(), Max);
		if (A->GetSouls() > Max) ASC->SetNumericAttributeBase(UWRAttributeSet::GetSoulsAttribute(), Max);
	}
	// The minions: expire, and keep them on a foe (the master's fight, its lock, else the nearest hostile).
	const double T = NcNow(Who);
	for (auto It = NcMinions().CreateIterator(); It; ++It)
	{
		AWRCreature* M = It->Key.Get();
		FNcMinion& Mi = It->Value;
		if (!M || !M->IsAlive()) { It.RemoveCurrent(); continue; }
		if (Mi.Owner.Get() != Who) continue;
		if (T >= Mi.Until || !Who->IsAlive())
		{
			AWRCreature* Gone = M;
			It.RemoveCurrent();
			Crumble(Gone, false);
			continue;
		}
		if (T < Mi.NextLook) continue;
		Mi.NextLook = T + 0.5;
		AWRCharacterBase* Foe = Cast<AWRCharacterBase>(M->GetAggroTarget());
		if (!Foe || !Foe->IsAlive() || !M->IsHostileTo(Foe))
		{
			Foe = nullptr;
			if (const AWRPlayerController* PC = Cast<AWRPlayerController>(Who->GetController()); PC && PC->GetServerLock() && PC->GetServerLock()->IsAlive() && M->IsHostileTo(PC->GetServerLock())) Foe = PC->GetServerLock();
			if (!Foe && Who->CombatTarget.IsValid() && Who->CombatTarget->IsAlive() && M->IsHostileTo(Who->CombatTarget.Get())) Foe = Who->CombatTarget.Get();
			if (!Foe)
			{
				float Best = NcT(TEXT("minionSeek"), 1500.f);
				for (TActorIterator<AWRCharacterBase> C(Who->GetWorld()); C; ++C)
				{
					if (*C == M || !C->IsAlive() || !M->IsHostileTo(*C) || C->IsHidden()) continue;
					const float D = float(FVector::Dist(C->GetActorLocation(), M->GetActorLocation()));
					if (D < Best) { Best = D; Foe = *C; }
				}
			}
			if (Foe) M->SetAggroTarget(Foe);
		}
	}
	(void)DeltaTime;
}

void WRNecro::OwnerDied(AWRCharacterBase* Who)
{
	if (!Who || !Who->HasAuthority()) return;
	if (UAbilitySystemComponent* ASC = Who->GetAbilitySystemComponent()) ASC->SetNumericAttributeBase(UWRAttributeSet::GetSoulsAttribute(), 0.f);
	NcCarry().Remove(Who);
	for (AWRCreature* M : MinionsOf(Who)) Crumble(M, false);
}

void WRNecro::Rested(AWRCharacterBase* Who)
{
	UAbilitySystemComponent* ASC = Who ? Who->GetAbilitySystemComponent() : nullptr;
	if (!ASC || !Who->HasAuthority()) return;
	if (ASC->HasMatchingGameplayTag(Status_DeathlessSpent))
	{
		ASC->SetLooseGameplayTagCount(Status_DeathlessSpent, 0, EGameplayTagReplicationState::TagOnly);
		NcTell(Who, TEXT("Deathless is ready again"));
	}
}

// ================================================================================================ raising

bool WRNecro::IsRaised(const AActor* Who)
{
	const AWRCreature* C = Cast<AWRCreature>(Who);
	if (!C) return false;
	if (C->HasAuthority() && NcMinions().Contains(const_cast<AWRCreature*>(C))) return true;
	return C->HasTag(Status_Raised);
}

AWRCharacterBase* WRNecro::MasterOf(const AActor* Minion)
{
	const AWRCreature* C = Cast<AWRCreature>(Minion);
	const FNcMinion* M = C ? NcMinions().Find(const_cast<AWRCreature*>(C)) : nullptr;
	return M ? M->Owner.Get() : nullptr;
}

bool WRNecro::CanRaise(const AWRCreature* Body, const AWRCharacterBase* By)
{
	if (!Body || Body->IsAlive() || Body->IsTamed() || IsRaised(Body)) return false;
	const FWRCreatureDef* Def = Body->GetDef();
	if (!Def || Def->Archetype == TEXT("boss") || Def->Archetype == TEXT("dummy") || UWRGraveBody::IsRisen(Def->Id)) return false;
	const FNcBody* B = NcBodies().Find(const_cast<AWRCreature*>(Body));
	if (!B || B->bUsed) return false;
	// Not butchered: a body nobody has taken from yet.
	if (Body->GetBodyHitsLeft() <= 0 || Body->GetBodyHitsLeft() < B->HitsAtDeath) return false;
	// Fresh: 3 minutes (Preserved: a frozen death keeps 10).
	const bool bPreserved = B->bFrozen && By && By->HasTag(Combo_Preserved);
	const float Window = bPreserved ? NcComboNum(TEXT("preserved"), TEXT("seconds"), 600.f) : NcT(TEXT("raiseWindow"), 180.f);
	return NcNow(Body) - B->DiedAt <= Window;
}

TArray<AWRCreature*> WRNecro::BodiesNear(const UWorld* World, const FVector& At, float Radius, const AWRCharacterBase* By)
{
	TArray<AWRCreature*> Out;
	for (const TPair<TWeakObjectPtr<AWRCreature>, FNcBody>& P : NcBodies())
	{
		AWRCreature* C = P.Key.Get();
		if (!C || C->GetWorld() != World || FVector::Dist(C->GetActorLocation(), At) > Radius || !CanRaise(C, By)) continue;
		Out.Add(C);
	}
	Out.Sort([At](const AWRCreature& A, const AWRCreature& B) { return FVector::DistSquared(A.GetActorLocation(), At) < FVector::DistSquared(B.GetActorLocation(), At); });
	return Out;
}

int32 WRNecro::MinionCap(const AActor* Owner)
{
	return int32(NcHas(Owner, TEXT("second_wave")) ? NcT(TEXT("minionCapSecondWave"), 5.f) : NcT(TEXT("minionCap"), 3.f));
}

TArray<AWRCreature*> WRNecro::MinionsOf(const AWRCharacterBase* Owner)
{
	TArray<AWRCreature*> Out;
	for (const TPair<TWeakObjectPtr<AWRCreature>, FNcMinion>& P : NcMinions())
	{
		if (P.Value.Owner.Get() == Owner && P.Key.IsValid() && P.Key->IsAlive()) Out.Add(P.Key.Get());
	}
	return Out;
}

AWRCreature* WRNecro::Raise(AWRCharacterBase* Owner, AWRCreature* Body, float Seconds, bool bIgnoreCap, float StatScale, FName Variant)
{
	if (!Owner || !Owner->HasAuthority() || !Body || !Owner->GetPlayerState()) return nullptr;
	const FWRCreatureDef* Def = Body->GetDef();
	if (!Def) return nullptr;
	// At the cap the oldest capped minion goes to bones first (the newest raise always stands).
	if (!bIgnoreCap)
	{
		TArray<TPair<double, AWRCreature*>> Capped;
		for (const TPair<TWeakObjectPtr<AWRCreature>, FNcMinion>& P : NcMinions())
		{
			if (P.Value.Owner.Get() == Owner && P.Value.bCapped && P.Key.IsValid() && P.Key->IsAlive()) Capped.Add({ P.Value.Until, P.Key.Get() });
		}
		Capped.Sort([](const TPair<double, AWRCreature*>& A, const TPair<double, AWRCreature*>& B) { return A.Key < B.Key; });
		for (int32 i = 0; i <= Capped.Num() - MinionCap(Owner); ++i) Crumble(Capped[i].Value, false);
	}
	const FVector At = Body->GetActorLocation() + FVector(0.f, 0.f, 40.f);
	const FRotator Face(0.f, Body->GetActorRotation().Yaw, 0.f);
	const FName Id = Def->Id;
	if (FNcBody* B = NcBodies().Find(Body)) B->bUsed = true;
	// Grave-light threads pull the body upright; the eyes light; a dry crack of joints.
	WRFx::Broadcast(Owner, TEXT("necro.raise"), At - FVector(0.f, 0.f, Body->GetSimpleCollisionHalfHeight()), Face, FMath::Max(0.6f, Body->GetSimpleCollisionRadius() / 60.f), TEXT("necro.raise"));
	Body->Destroy();
	AWRCreature* M = AWRCreature::SpawnCreature(Owner->GetWorld(), Id, At, Face);
	if (!M) return nullptr;
	// Its master's: it follows them and fights what they fight (the tame orders work), but it is never a tame (no
	// limit, no save, no body when it falls).
	M->TamedBy = Owner->GetPlayerState()->GetPlayerName();
	M->TameOrder = EWRTameOrder::Follow;
	M->bTameAggressive = true;
	if (UAbilitySystemComponent* ASC = M->GetAbilitySystemComponent())
	{
		ASC->AddLooseGameplayTag(Status_Raised, 1, EGameplayTagReplicationState::TagOnly);
		const UWRAttributeSet* A = M->GetAttributes();
		const float Scale = (StatScale > 0.f ? StatScale : NcT(TEXT("raisedStats"), 0.6f)) * (1.f + NcT(TEXT("restlessPerRank"), 0.15f) * NcRank(Owner, TEXT("restless_dead")));
		if (A)
		{
			ASC->SetNumericAttributeBase(UWRAttributeSet::GetMaxHealthAttribute(), FMath::Max(1.f, A->GetMaxHealth() * Scale));
			ASC->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), FMath::Max(1.f, A->GetMaxHealth()));
		}
	}
	if (UCharacterMovementComponent* Move = M->GetCharacterMovement())
	{
		Move->MaxWalkSpeed *= 1.f + NcT(TEXT("deadMarchSpeedPerRank"), 0.1f) * NcRank(Owner, TEXT("dead_march"));
	}
	UWRGraveBody::AttachRaised(M, Variant);
	FNcMinion Mi;
	Mi.Owner = Owner;
	Mi.Until = NcNow(Owner) + Seconds;
	Mi.DamageScale = (StatScale > 0.f ? StatScale : NcT(TEXT("raisedStats"), 0.6f)) * (1.f + NcT(TEXT("deadMarchDamagePerRank"), 0.08f) * NcRank(Owner, TEXT("dead_march")));
	Mi.bCapped = !bIgnoreCap;
	Mi.Variant = Variant;
	NcMinions().Add(M, Mi);
	M->SetLifeSpan(Seconds + 10.f);   // a safety: the tick crumbles it on time
	NoteRaiseFlare(Owner);
	UE_LOG(LogWildreach, Log, TEXT("NECRO %s raises %s (%.0f s%s%s)"), *Owner->GetName(), *Id.ToString(), Seconds, bIgnoreCap ? TEXT(", beyond the cap") : TEXT(""),
		Variant.IsNone() ? TEXT("") : *FString::Printf(TEXT(", %s"), *Variant.ToString()));
	return M;
}

void WRNecro::Crumble(AWRCreature* Minion, bool bExplode)
{
	if (!Minion || !Minion->HasAuthority()) return;
	NcMinions().Remove(Minion);
	const FVector Feet = Minion->GetActorLocation() - FVector(0.f, 0.f, Minion->GetSimpleCollisionHalfHeight());
	WRFx::Broadcast(Minion, bExplode ? FName(TEXT("necro.corpse_explosion")) : FName(TEXT("grave.bone_collapse")), Feet, FRotator::ZeroRotator,
		bExplode ? 2.f : FMath::Max(0.6f, Minion->GetSimpleCollisionRadius() / 80.f), bExplode ? FName(TEXT("necro.corpse_explosion")) : FName(TEXT("creature.risen.collapse")));
	Minion->Destroy();
}

// ================================================================================================ the combat code's questions

float WRNecro::DamageScale(const AWRCharacterBase* Attacker, const AWRCharacterBase* Target, const FWRHit& Hit)
{
	if (!Attacker) return 1.f;
	float S = 1.f;
	// A minion hits at its raise's scale (60%, Dead march).
	if (const FNcMinion* M = NcMinions().Find(Cast<AWRCreature>(const_cast<AWRCharacterBase*>(Attacker)))) S *= M->DamageScale;
	if (Hit.PathId == NcPathId && Walks(Attacker))
	{
		S *= NcCorruptionBuff(Attacker);
		if (Hit.bSpell || (!Hit.bMelee && Hit.bProjectile)) S *= 1.f + NcT(TEXT("necroticPowerPerRank"), 0.06f) * NcRank(Attacker, TEXT("necrotic_power"));
	}
	// Final harvest on what it can't execute (bosses, players): +30% under 20%.
	if (Target && Hit.bMelee && NcHas(Attacker, TEXT("final_harvest")) && (Target->GetPlayerState() || UWRCombatLibrary::StanceKind(Target) != TEXT("normal")))
	{
		const UWRAttributeSet* A = Target->GetAttributes();
		if (A && A->GetHealth() < A->GetMaxHealth() * NcT(TEXT("finalHarvestAt"), 0.2f)) S *= 1.f + NcT(TEXT("finalHarvestBonus"), 0.3f);
	}
	return S;
}

float WRNecro::Incoming(AWRCharacterBase* Target, const FWRHit& Hit, float Damage)
{
	if (!Target || !Target->HasAuthority() || Damage <= 0.f || !Target->GetPlayerState()) return Damage;
	UAbilitySystemComponent* ASC = Target->GetAbilitySystemComponent();
	const UWRAttributeSet* A = Target->GetAttributes();
	if (!ASC || !A) return Damage;
	const FVector At = !Hit.ImpactPoint.IsZero() ? Hit.ImpactPoint : NcChest(Target);
	// Soul shield: with more than 5 Souls, each Soul spent absorbs 3 a rank, up to 3 a hit.
	if (const int32 R = NcRank(Target, TEXT("soul_shield")); R > 0 && A->GetSouls() > NcT(TEXT("soulShieldFloor"), 5.f))
	{
		const float Per = NcT(TEXT("soulShieldPerRank"), 3.f) * R;
		const int32 Can = FMath::Min3(int32(NcT(TEXT("soulShieldMax"), 3.f)), FMath::FloorToInt(A->GetSouls() - NcT(TEXT("soulShieldFloor"), 5.f)), FMath::CeilToInt(Damage / FMath::Max(1.f, Per)));
		if (Can > 0)
		{
			ASC->SetNumericAttributeBase(UWRAttributeSet::GetSoulsAttribute(), A->GetSouls() - Can);
			Damage = FMath::Max(0.f, Damage - Per * Can);
			WRFx::Broadcast(Target, TEXT("necro.soul_shield"), At, FRotator::ZeroRotator, 1.f, TEXT("necro.soul_shield"));
		}
	}
	// Shared pain: 20% of it split across the minions within 15 m.
	if (Damage > 0.f && !Hit.bReaction && NcHas(Target, TEXT("shared_pain")))
	{
		TArray<AWRCreature*> Near;
		for (AWRCreature* M : MinionsOf(Target)) if (FVector::Dist(M->GetActorLocation(), Target->GetActorLocation()) <= NcT(TEXT("sharedPainReach"), 1500.f)) Near.Add(M);
		if (Near.Num() > 0)
		{
			const float Share = Damage * NcT(TEXT("sharedPainShare"), 0.2f);
			Damage -= Share;
			for (AWRCreature* M : Near)
			{
				FWRHit Pain;
				Pain.Damage = Share / Near.Num();
				Pain.bDamageOverTime = true;
				Pain.bReaction = true;
				Pain.bUnblockable = true;
				Pain.Origin = M->GetActorLocation();
				UWRCombatLibrary::ApplyHit(M, Pain);
			}
		}
	}
	// Deathless (main path, once per Rest): a blow that would kill leaves you at exactly 1 HP. Nothing else.
	if (Damage >= A->GetHealth() && ASC->HasMatchingGameplayTag(Passive_Deathless) && !ASC->HasMatchingGameplayTag(Status_DeathlessSpent))
	{
		Damage = FMath::Max(0.f, A->GetHealth() - 1.f);
		ASC->AddLooseGameplayTag(Status_DeathlessSpent, 1, EGameplayTagReplicationState::TagOnly);
		WRFx::BroadcastOn(Target, TEXT("necro.deathless"), TEXT("spine"), 1.f, TEXT("necro.deathless"), 0.4f);
		NcTell(Target, TEXT("Deathless: you cheat death (it returns after a Rest)"));
		UE_LOG(LogWildreach, Log, TEXT("NECRO %s is Deathless (a killing blow leaves 1 HP)"), *Target->GetName());
	}
	return Damage;
}

void WRNecro::ScaleBuildups(const AWRCharacterBase* Attacker, FWRHit& Hit)
{
	if (!Attacker) return;
	// Wither chance: +10% decay on everything you build, and +6 decay on every weapon hit, a rank.
	if (const int32 R = NcRank(Attacker, TEXT("wither_chance")); R > 0)
	{
		if (float* D = Hit.Buildups.Find(TEXT("decay"))) *D *= 1.f + NcT(TEXT("witherPerRank"), 0.1f) * R;
		if (Hit.bMelee && !Hit.bDamageOverTime) Hit.Buildups.FindOrAdd(TEXT("decay")) += NcT(TEXT("witherWeaponPerRank"), 6.f) * R;
	}
	// A risen ash wraith's blows burn; a frozen legion's freeze.
	if (const FNcMinion* M = NcMinions().Find(Cast<AWRCreature>(const_cast<AWRCharacterBase*>(Attacker))))
	{
		if (M->Variant == TEXT("ash")) Hit.Buildups.FindOrAdd(TEXT("burn")) += NcComboNum(TEXT("ash_wraith"), TEXT("burn"), 15.f);
		if (M->Variant == TEXT("ice")) Hit.Buildups.FindOrAdd(TEXT("frost")) += NcComboNum(TEXT("frozen_legion"), TEXT("frost"), 15.f);
	}
}

void WRNecro::AfterHit(AWRCharacterBase* Target, const FWRHit& Hit, float Damage)
{
	if (!Target || !Target->HasAuthority()) return;
	AWRCharacterBase* Attacker = Cast<AWRCharacterBase>(Hit.Instigator.Get());
	AWRCharacterBase* Owner = NcCreditFor(Attacker);
	if (!Owner || Owner == Target || !Walks(Owner)) return;
	const double T = NcNow(Target);
	NcCredit().FindOrAdd(Target).Add(Owner, T);
	if (Attacker != Owner) return;   // the rest is the Necromancer's own blows
	// Death mark: Grave strike, Scythe sweep and Shadow bolt mark it for 10 s (a turning grey sigil).
	if (NcHas(Owner, TEXT("death_mark")) && Hit.PathId == NcPathId && Damage > 0.f && !Hit.bDamageOverTime && (Hit.bWeaponArt || Hit.bProjectile) && Target->Status)
	{
		const bool bNew = !Target->HasTag(Status_DeathMark);
		Target->Status->ApplyStatus(Status_DeathMark, NcT(TEXT("deathMarkSeconds"), 10.f));
		if (bNew) WRFx::BroadcastOn(Target, TEXT("necro.death_mark"), TEXT("head"), 1.f, TEXT("necro.death_mark"), NcT(TEXT("deathMarkSeconds"), 10.f));
	}
	// Final harvest: a weapon hit on a non-boss creature under 20% kills it on contact (a reaping finish).
	if (NcHas(Owner, TEXT("final_harvest")) && Hit.bMelee && !Hit.bReaction && Target->IsAlive() && !Target->GetPlayerState()
		&& Cast<AWRCreature>(Target) && UWRCombatLibrary::StanceKind(Target) == TEXT("normal") && !IsRaised(Target))
	{
		const UWRAttributeSet* A = Target->GetAttributes();
		if (A && A->GetHealth() > 0.f && A->GetHealth() < A->GetMaxHealth() * NcT(TEXT("finalHarvestAt"), 0.2f))
		{
			WRFx::Broadcast(Owner, TEXT("necro.final_harvest"), NcChest(Target), FRotator::ZeroRotator, 1.f, TEXT("necro.final_harvest"));
			FWRHit Reap;
			Reap.Instigator = Owner;
			Reap.Damage = A->GetHealth() + 1.f;
			Reap.bReaction = true;
			Reap.bUnblockable = true;
			Reap.bMelee = true;
			Reap.PathId = NcPathId;
			Reap.Element = Element_Necromancy;
			Reap.Origin = Owner->GetActorLocation();
			UWRCombatLibrary::ApplyHit(Target, Reap);
		}
	}
}

void WRNecro::OnKill(AWRCharacterBase* Target, const FWRHit& Hit)
{
	if (!Target || !Target->HasAuthority()) return;
	UWorld* World = Target->GetWorld();
	const double T = NcNow(Target);
	// A Necromancer's own death: their Souls are gone, their minions fall.
	if (Target->GetPlayerState() && Walks(Target)) OwnerDied(Target);
	// A body's state as it falls (for raising: frozen, burning).
	if (AWRCreature* Cr = Cast<AWRCreature>(Target); Cr && !IsRaised(Cr))
	{
		FNcBody& B = NcBodies().FindOrAdd(Cr);
		B.DiedAt = T;
		B.bFrozen = Cr->HasTag(WRTags::Status_Frozen) || Cr->HasTag(WRTags::Status_Frostbite);
		B.bBurning = Cr->HasTag(WRTags::Status_Burn);
	}
	// A minion fell: Cycle of life heals its master's allies within 8 m.
	if (const FNcMinion* M = NcMinions().Find(Cast<AWRCreature>(Target)))
	{
		AWRCharacterBase* Owner = M->Owner.Get();
		if (Owner && Owner->HasTag(Combo_CycleOfLife))
		{
			const float R = NcComboNum(TEXT("cycle_of_life"), TEXT("radius"), 800.f), H = NcComboNum(TEXT("cycle_of_life"), TEXT("heal"), 20.f);
			for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
			{
				if (!It->IsAlive() || It->IsHostileTo(Owner) || IsRaised(*It) || FVector::Dist(It->GetActorLocation(), Target->GetActorLocation()) > R) continue;
				if (!It->GetPlayerState() && It->GetTameOwner().IsEmpty()) continue;
				NcHeal(*It, H);
			}
			WRFx::Broadcast(Owner, TEXT("necro.cycle_of_life"), Target->GetActorLocation(), FRotator::ZeroRotator, R / 400.f, NAME_None);
		}
		return;   // no Souls for your own minion
	}
	// Souls to every Necromancer within 30 m who (or whose minion) hit it in its last 10 s.
	AWRCharacterBase* Killer = NcCreditFor(Hit.Instigator.Get());
	const TMap<TWeakObjectPtr<AWRCharacterBase>, double>* Hands = NcCredit().Find(Target);
	const float Base = NcWorth(Target);
	const bool bMarked = Target->HasTag(Status_DeathMark);
	const bool bReaped = Hit.bReaction && Hit.PathId == NcPathId && Hit.bMelee;
	for (TActorIterator<AWRCharacter> It(World); It; ++It)
	{
		AWRCharacter* N = *It;
		if (!N->IsAlive() || !Walks(N) || N == Target) continue;
		if (FVector::Dist(N->GetActorLocation(), Target->GetActorLocation()) > NcT(TEXT("soulReach"), 3000.f)) continue;
		const double* When = Hands ? Hands->Find(N) : nullptr;
		const bool bHand = N == Killer || (When && T - *When <= NcT(TEXT("creditSeconds"), 10.f));
		if (!bHand) continue;
		float Amount = Base * (1.f + NcT(TEXT("harvestPerRank"), 0.2f) * NcRank(N, TEXT("harvest")));
		if (bMarked) Amount += NcRank(N, TEXT("death_mark"));
		GainSouls(N, Amount, NcChest(Target), bReaped && N == Killer);
	}
	NcCredit().Remove(Target);
	// The killer's own kill rewards: Soul siphon (5% max HP + 4, once a second), Reaping stride (25 stamina).
	if (Killer && Killer->GetPlayerState() && Walks(Killer) && Killer->IsAlive())
	{
		if (NcHas(Killer, TEXT("soul_siphon")))
		{
			double& Last = NcSiphonAt().FindOrAdd(Killer);
			if (T - Last >= 1.0)
			{
				Last = T;
				const UWRAttributeSet* A = Killer->GetAttributes();
				NcHeal(Killer, (A ? A->GetMaxHealth() : 100.f) * NcT(TEXT("siphonPct"), 0.05f) + NcT(TEXT("siphonFlat"), 4.f));
			}
		}
		if (NcHas(Killer, TEXT("reaping_stride")))
		{
			UAbilitySystemComponent* ASC = Killer->GetAbilitySystemComponent();
			const UWRAttributeSet* A = Killer->GetAttributes();
			if (ASC && A) ASC->SetNumericAttributeBase(UWRAttributeSet::GetStaminaAttribute(), FMath::Min(A->GetMaxStamina(), A->GetStamina() + NcT(TEXT("strideStamina"), 25.f)));
		}
		// Free raises from the combos: Frozen legion (killed while Frozen), Death curse (killed by Doom).
		if (AWRCreature* Cr = Cast<AWRCreature>(Target); Cr && Cr->GetDef() && Cr->GetDef()->Archetype != TEXT("boss"))
		{
			FNcBody& B = NcBodies().FindOrAdd(Cr);
			if (Killer->HasTag(Combo_FrozenLegion) && Cr->HasTag(WRTags::Status_Frozen))
			{
				int32 Legion = 0;
				for (const TPair<TWeakObjectPtr<AWRCreature>, FNcMinion>& P : NcMinions()) Legion += P.Value.Owner.Get() == Killer && P.Value.Variant == TEXT("ice") ? 1 : 0;
				if (Legion < int32(NcComboNum(TEXT("frozen_legion"), TEXT("beyondCap"), 2.f)))
				{
					B.FreeFor = Killer;
					B.FreeSeconds = NcComboNum(TEXT("frozen_legion"), TEXT("seconds"), 20.f);
					B.FreeVariant = TEXT("ice");
				}
			}
			else if (Killer->HasTag(Combo_DeathCurse) && Hit.Buildups.Contains(WRBlood::DoomMarker))
			{
				B.FreeFor = Killer;
				B.FreeSeconds = NcComboNum(TEXT("death_curse"), TEXT("seconds"), 30.f);
				B.FreeVariant = TEXT("blood");
			}
		}
	}
}

void WRNecro::CreatureDied(AWRCreature* Creature, AActor* Killer)
{
	if (!Creature || !Creature->HasAuthority()) return;
	if (NcMinions().Contains(Creature) || Creature->HasTag(Status_Raised))
	{
		// A minion that falls goes to bones where it fell: no body, no loot.
		NcMinions().Remove(Creature);
		WRFx::Broadcast(Creature, TEXT("grave.bone_collapse"), Creature->GetActorLocation() - FVector(0.f, 0.f, Creature->GetSimpleCollisionHalfHeight()), FRotator::ZeroRotator, 1.f, TEXT("creature.risen.collapse"));
		Creature->SetActorHiddenInGame(true);
		return;
	}
	FNcBody& B = NcBodies().FindOrAdd(Creature);
	if (B.DiedAt <= 0.0) B.DiedAt = NcNow(Creature);
	B.HitsAtDeath = uint8(Creature->GetBodyHitsLeft());
	// Ash wraith: a body that died burning rises burning (the raise reads it).
	// Free raises waiting on it (Frozen legion, Death curse): it stands up a moment after it falls.
	if (AWRCharacterBase* For = B.FreeFor.Get(); For && B.HitsAtDeath > 0)
	{
		TWeakObjectPtr<AWRCreature> WeakC(Creature);
		TWeakObjectPtr<AWRCharacterBase> WeakF(For);
		const float Seconds = B.FreeSeconds;
		const FName Variant = B.FreeVariant;
		FTimerHandle H;
		Creature->GetWorldTimerManager().SetTimer(H, [WeakC, WeakF, Seconds, Variant]()
		{
			if (WeakC.IsValid() && WeakF.IsValid() && WeakF->IsAlive()) WRNecro::Raise(WeakF.Get(), WeakC.Get(), Seconds, true, -1.f, Variant);
		}, NcT(TEXT("freeRaiseDelay"), 0.8f), false);
	}
	B.FreeFor = nullptr;
	// Forget bodies long gone.
	for (auto It = NcBodies().CreateIterator(); It; ++It) if (!It->Key.IsValid()) It.RemoveCurrent();
	for (auto It = NcCredit().CreateIterator(); It; ++It) if (!It->Key.IsValid()) It.RemoveCurrent();
	(void)Killer;
}

// ================================================================================================ the body

void WRNecro::NoteRaiseFlare(AWRCharacterBase* Who)
{
	if (Who && Who->Status) Who->Status->ApplyStatus(Status_RaiseFlare, NcT(TEXT("raiseFlareSeconds"), 1.5f));
}

float WRNecro::RaiseFlareAge(const AActor* Hero)
{
	return NcTagged(Hero, Status_RaiseFlare) ? 1.f : 0.f;
}

uint32 WRNecro::LookSig(const AActor* Hero)
{
	return (NcTagged(Hero, Status_LichForm) ? 1u : 0u) | (NcTagged(Hero, Status_RaiseFlare) ? 2u : 0u);
}

void WRNecro::ApplyLook(const AActor* Hero, const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Skin, const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Eyes,
	const TMap<FName, FLinearColor>& EyeDefaultColors, FName Main, float MainS, FName Sub, float SubS)
{
	if (Main != NcPathId && Sub != NcPathId) return;
	const bool bLich = NcTagged(Hero, Status_LichForm);
	const bool bFlare = NcTagged(Hero, Status_RaiseFlare);
	// Lich form: the 100 look (full ash skin) for its 15 s at any point count (2.18).
	if (bLich)
	{
		const FString Prefix = Main == NcPathId ? TEXT("Main") : TEXT("Sub");
		for (UMaterialInstanceDynamic* M : Skin) if (M) M->SetScalarParameterValue(FName(*(Prefix + TEXT("Strength"))), 1.f);
	}
	// The pupil ring: brighter in Lich form, x1.6 for 1.5 s after each raise.
	if (bLich || bFlare)
	{
		const FLinearColor Ring(FWRTuning::Get().Num(TEXT("necromancy.ringColor.r"), 1.3f), FWRTuning::Get().Num(TEXT("necromancy.ringColor.g"), 2.3f), FWRTuning::Get().Num(TEXT("necromancy.ringColor.b"), 1.2f));
		const float K = (bFlare ? NcT(TEXT("raiseFlareMult"), 1.6f) : 1.f) * (bLich ? NcT(TEXT("lichRingMult"), 1.4f) : 1.f);
		for (UMaterialInstanceDynamic* M : Eyes) if (M) M->SetVectorParameterValue(TEXT("Melanosis Ring Color"), Ring * K);
	}
	(void)EyeDefaultColors; (void)MainS; (void)SubS;
}
