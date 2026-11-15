// The Void path's rules (see WRVoidPath.h; cooks/biome-void.md 2.17-2.18).
#include "WRVoidPath.h"
#include "Wildreach.h"
#include "WRAttributeSet.h"
#include "WRBoss.h"
#include "WRCharacter.h"
#include "WRCharacterBase.h"
#include "WRCombatLibrary.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRInventory.h"
#include "WRPathComponent.h"
#include "WRPlayerController.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRTuning.h"
#include "WRVoid.h"
#include "AbilitySystemComponent.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace WRVoidPath
{
	UE_DEFINE_GAMEPLAY_TAG(Status_Unravelled, "Status.Unravelled");
	UE_DEFINE_GAMEPLAY_TAG(Status_Phased, "Status.Phased");
	UE_DEFINE_GAMEPLAY_TAG(Status_Nowhere, "Status.Nowhere");
	UE_DEFINE_GAMEPLAY_TAG(Status_Sacrifice, "Status.Sacrifice");
	UE_DEFINE_GAMEPLAY_TAG(Ability_NullStrike, "Ability.NullStrike");
	UE_DEFINE_GAMEPLAY_TAG(Combo_BlackFlame, "Combo.BlackFlame");
	UE_DEFINE_GAMEPLAY_TAG(Combo_VoidFurnace, "Combo.VoidFurnace");
	UE_DEFINE_GAMEPLAY_TAG(Combo_DarkLightning, "Combo.DarkLightning");
	UE_DEFINE_GAMEPLAY_TAG(Combo_SingularitySpark, "Combo.SingularitySpark");
	UE_DEFINE_GAMEPLAY_TAG(Combo_FrozenBlink, "Combo.FrozenBlink");
	UE_DEFINE_GAMEPLAY_TAG(Combo_IceMaw, "Combo.IceMaw");
	UE_DEFINE_GAMEPLAY_TAG(Combo_VoidLeech, "Combo.VoidLeech");
	UE_DEFINE_GAMEPLAY_TAG(Combo_AbyssalHunger, "Combo.AbyssalHunger");
}

namespace WRVoidPathImpl
{
	const FName PathId(TEXT("void"));
	float VP(const TCHAR* Key, float Fallback) { return WRTune(*FString::Printf(TEXT("voidPath.%s"), Key), Fallback); }
	const UWRPathComponent* PathsOf(const AActor* Who) { return UWRPathComponent::FindFor(Who); }
	bool HasTagOn(const AActor* Who, const FGameplayTag& Tag)
	{
		const AWRCharacterBase* C = Cast<AWRCharacterBase>(Who);
		return C && C->HasTag(Tag);
	}
	/** void_bolt -> voidBolt (the spells.<key> naming). */
	FString CamelKey(FName NodeId)
	{
		TArray<FString> Parts;
		NodeId.ToString().ParseIntoArray(Parts, TEXT("_"));
		FString Out;
		for (int32 i = 0; i < Parts.Num(); ++i) Out += i == 0 ? Parts[i] : Parts[i].Left(1).ToUpper() + Parts[i].Mid(1);
		return Out;
	}
	/** Per-player clocks the server keeps: the last void ability, Nowhere's pending crit, Unseen's last blink. */
	struct FClock { double LastUse = -100.0; bool bNowhereCrit = false; double BlinkedAt = -100.0; };
	TMap<TWeakObjectPtr<const AActor>, FClock>& Clocks()
	{
		static TMap<TWeakObjectPtr<const AActor>, FClock> M;
		return M;
	}
	double Now(const UObject* W) { return W && W->GetWorld() ? W->GetWorld()->GetTimeSeconds() : 0.0; }
	bool IsVoidHit(const FWRHit& Hit) { return Hit.Element == WRTags::Element_Void || Hit.PathId == PathId; }
}
using namespace WRVoidPathImpl;

bool WRVoidPath::Walks(const AActor* Who) { const UWRPathComponent* P = PathsOf(Who); return P && P->IsActivePath(PathId); }
bool WRVoidPath::IsMain(const AActor* Who) { const UWRPathComponent* P = PathsOf(Who); return P && P->GetMainPath() == PathId; }
int32 WRVoidPath::Points(const AActor* Who) { const UWRPathComponent* P = PathsOf(Who); return P && P->IsActivePath(PathId) ? P->GetSpent(PathId) : 0; }

float WRVoidPath::SpellNum(FName NodeId, const TCHAR* Field, float Fallback)
{
	return WRTune(*FString::Printf(TEXT("spells.%s.%s"), *CamelKey(NodeId), Field), Fallback);
}

float WRVoidPath::Tolerance(const AActor* Who)
{
	return VP(TEXT("tolerance"), 20.f) + UWRPathComponent::Stat(Who, TEXT("voidTolerance"));
}

float WRVoidPath::CostNow(const AActor* Who, FName NodeId, bool bAlways)
{
	const AWRCharacterBase* C = Cast<AWRCharacterBase>(Who);
	const UWRAttributeSet* A = C ? C->GetAttributes() : nullptr;
	if (!A) return 0.f;
	float Gain = SpellNum(NodeId, TEXT("gain"), 0.f);
	if (IsMain(Who) && Gain > 0.f) Gain *= VP(TEXT("emptyVessel.gain"), 0.7f);
	// Under your tolerance (after this ability's Void), it costs nothing; at or past it, its listed max HP.
	if (!bAlways && A->GetVoid() + FMath::Max(0.f, Gain) < Tolerance(Who)) return 0.f;
	float Cost = SpellNum(NodeId, TEXT("maxHp"), 0.f);
	if (IsMain(Who) && !bAlways) Cost *= VP(TEXT("emptyVessel.cost"), 0.75f);
	// Hollowplate (4 pieces) and the Unravelled thread take some off; Void furnace halves it at full Heat.
	Cost *= FMath::Clamp(1.f + UWRInventoryComponent::GearStatFor(Who, TEXT("voidSelfDrain")), 0.f, 1.f);
	if (HasTagOn(Who, Combo_VoidFurnace) && A->GetMaxHeat() > 0.f && A->GetHeat() >= A->GetMaxHeat() - 0.5f) Cost *= VP(TEXT("combos.voidFurnaceCostMult"), 0.5f);
	return Cost;
}

bool WRVoidPath::CanUse(const AActor* Who, FName NodeId, bool bAlways, FString& OutWhy)
{
	const AWRCharacterBase* C = Cast<AWRCharacterBase>(Who);
	const UWRAttributeSet* A = C ? C->GetAttributes() : nullptr;
	if (!A) return false;
	// The self-cost limit: your own abilities can't take the band past 75% of max HP (a resource, like stamina; rifts,
	// Gnawers and every enemy can still take you all the way).
	const float Cost = CostNow(Who, NodeId, bAlways);
	if (Cost > 0.f && A->GetMaxHealthLost() + Cost > A->GetMaxHealth() * VP(TEXT("selfLimit"), 0.75f))
	{
		OutWhy = TEXT("The Void has taken all it can: rest");
		return false;
	}
	return true;
}

void WRVoidPath::Pay(AWRCharacterBase* Who, FName NodeId, bool bAlways)
{
	if (!Who || !Who->HasAuthority()) return;
	UWRAttributeSet* A = Who->GetAttributes();
	UAbilitySystemComponent* ASC = Who->GetAbilitySystemComponent();
	if (!A || !ASC) return;
	const float Cost = CostNow(Who, NodeId, bAlways);
	if (Cost > 0.f) WRVoid::LoseMaxHealth(Who, Cost, Who);
	float Gain = SpellNum(NodeId, TEXT("gain"), 0.f);
	if (IsMain(Who) && Gain > 0.f) Gain *= VP(TEXT("emptyVessel.gain"), 0.7f);
	const float Max = FMath::Max(1.f, A->GetMaxVoid() > 0.f ? A->GetMaxVoid() : VP(TEXT("max"), 100.f));
	ASC->SetNumericAttributeBase(UWRAttributeSet::GetVoidAttribute(), FMath::Clamp(A->GetVoid() + Gain, 0.f, Max));
	if (Gain > 0.f && HasTagOn(Who, Combo_VoidFurnace))
	{
		// Void furnace: what the Void gives you also stokes Heat.
		UWRCombatLibrary::GainResource(Who, WRTags::Element_Fire, Gain * VP(TEXT("combos.voidFurnaceHeatPerPoint"), 0.5f));
	}
	Clocks().FindOrAdd(Who).LastUse = Now(Who);
	if (NodeId == TEXT("blink") || NodeId == TEXT("shadow_step")) Clocks().FindOrAdd(Who).BlinkedAt = Now(Who);
	UE_LOG(LogWildreach, Log, TEXT("VOIDPATH %s uses %s: +%.0f Void (now %.0f), %.1f max HP"), *Who->GetName(), *NodeId.ToString(), Gain, A->GetVoid(), Cost);
}

void WRVoidPath::ArmCrit(AWRCharacterBase* Who)
{
	if (Who && Who->HasAuthority()) Clocks().FindOrAdd(Who).bNowhereCrit = true;
}

void WRVoidPath::TickOwner(AWRCharacterBase* Who, float DeltaTime)
{
	UWRAttributeSet* A = Who ? Who->GetAttributes() : nullptr;
	UAbilitySystemComponent* ASC = Who ? Who->GetAbilitySystemComponent() : nullptr;
	if (!A || !ASC || !Who->HasAuthority()) return;
	if (A->GetMaxVoid() <= 0.f) ASC->SetNumericAttributeBase(UWRAttributeSet::GetMaxVoidAttribute(), VP(TEXT("max"), 100.f));
	if (A->GetVoid() <= 0.f) return;
	// It drains 5 a second once 4 s pass without a void ability.
	const FClock* Ck = Clocks().Find(Who);
	if (!Ck || Now(Who) - Ck->LastUse > VP(TEXT("decayDelay"), 4.f))
	{
		ASC->SetNumericAttributeBase(UWRAttributeSet::GetVoidAttribute(), FMath::Max(0.f, A->GetVoid() - VP(TEXT("decay"), 5.f) * DeltaTime));
	}
}

float WRVoidPath::DamageScale(AWRCharacterBase* Attacker, const AWRCharacterBase* Target, const FWRHit& Hit)
{
	if (!Attacker) return 1.f;
	float K = 1.f;
	const UWRAttributeSet* A = Attacker->GetAttributes();
	if (A && IsVoidHit(Hit) && Walks(Attacker))
	{
		// Huge power: +0.3% a point of the bar; Abyssal power.
		K *= 1.f + VP(TEXT("damagePerPoint"), 0.003f) * A->GetVoid() + UWRPathComponent::Stat(Attacker, TEXT("voidDamage"));
	}
	// Gluttony: +0.1% a rank per point of max health the Void has taken (to +50%).
	if (A && A->GetMaxHealthLost() > 0.f)
	{
		K *= 1.f + FMath::Min(VP(TEXT("gluttonyCap"), 0.5f), UWRPathComponent::Stat(Attacker, TEXT("gluttony")) * A->GetMaxHealthLost());
	}
	if (Attacker->HasTag(Status_Sacrifice)) K *= 1.f + SpellNum(TEXT("sacrifice"), TEXT("share"), 0.3f);
	// Dark matter: void blows on armour (a player's armour, a shelled or plated creature).
	if (Target && IsVoidHit(Hit))
	{
		const AWRCreature* Cr = Cast<AWRCreature>(Target);
		const bool bArmoured = Target->ArmorValue > 0.f || (Cr && Cr->GetDef() && Cr->GetDef()->PhysResist[1] > 1.f);
		if (bArmoured) K *= 1.f + UWRPathComponent::Stat(Attacker, TEXT("vsArmour"));
	}
	// Black flame: burns bite harder on the armoured.
	if (Target && Hit.bDamageOverTime && Hit.Element == WRTags::Element_Fire && Attacker->HasTag(Combo_BlackFlame) && Target->ArmorValue > 0.f)
	{
		K *= 1.f + VP(TEXT("combos.blackFlameVsArmour"), 0.1f);
	}
	// Nowhere: the first blow after it is critical (the riposte multiplier), and it's used up.
	if (!Hit.bDamageOverTime && Attacker->HasAuthority())
	{
		if (FClock* Ck = Clocks().Find(Attacker); Ck && Ck->bNowhereCrit)
		{
			Ck->bNowhereCrit = false;
			K *= VP(TEXT("critMult"), 3.f);
			if (UAbilitySystemComponent* ASC = Attacker->GetAbilitySystemComponent()) ASC->RemoveLooseGameplayTag(Status_Nowhere);
			WRFx::Broadcast(Attacker, TEXT("void.nowhere.crit"), Target ? Target->GetActorLocation() : Attacker->GetActorLocation(), FRotator::ZeroRotator, 1.f, TEXT("void.nowhere.crit"));
		}
	}
	return K;
}

float WRVoidPath::ArmourPen(const AWRCharacterBase* Attacker, const AWRCharacterBase* Target, const FWRHit& Hit)
{
	float Pen = 0.f;
	if (Attacker && Attacker->HasTag(Ability_NullStrike) && Hit.bMelee) Pen += SpellNum(TEXT("null_strike"), TEXT("armorIgnore"), 0.3f);
	if (Attacker && Attacker->HasTag(Combo_DarkLightning) && Hit.Element == WRTags::Element_Lightning) Pen += VP(TEXT("combos.darkLightningPen"), 0.3f);
	if (Target && Target->HasTag(Status_Unravelled)) Pen += VP(TEXT("unravelled.armourCut"), 0.2f);
	return FMath::Clamp(Pen, 0.f, 1.f);
}

float WRVoidPath::StanceScale(const AWRCharacterBase* Target)
{
	return Target && Target->HasTag(Status_Unravelled) ? 1.f + VP(TEXT("unravelled.stanceTaken"), 0.15f) : 1.f;
}

bool WRVoidPath::PassesThrough(const AWRCharacterBase* Target, const FWRHit& Hit)
{
	// Phase: every blow (grabs too) passes through you; Nowhere: you can't be found.
	return Target && (Target->HasTag(Status_Phased) || Target->HasTag(Status_Nowhere)) && !Hit.bDamageOverTime;
}

void WRVoidPath::AfterHit(AWRCharacterBase* Target, const FWRHit& Hit, float Damage)
{
	AWRCharacterBase* Attacker = Cast<AWRCharacterBase>(Hit.Instigator);
	if (!Target || !Attacker || !IsVoidHit(Hit) || !Walks(Attacker)) return;
	// Unravel chance: 8% a rank to fray its defences (armour -20%, plating counts as bare, stance +15%).
	const float Chance = UWRPathComponent::Stat(Attacker, TEXT("unravelChance"));
	if (Chance > 0.f && Target->IsAlive() && FMath::FRand() < Chance && Target->Status)
	{
		const bool bBoss = Cast<AWRBoss>(Target) != nullptr;
		Target->Status->ApplyStatus(Status_Unravelled, bBoss ? VP(TEXT("unravelled.bossSeconds"), 3.f) : VP(TEXT("unravelled.seconds"), 6.f));
		WRFx::Broadcast(Target, TEXT("void.unravel"), Target->GetActorLocation(), FRotator::ZeroRotator, FMath::Max(1.f, Target->GetSimpleCollisionRadius() / 60.f), TEXT("void.unravel"));
	}
}

float WRVoidPath::FeedFor(const AWRCharacterBase* Killed)
{
	const AWRCreature* C = Cast<AWRCreature>(Killed);
	const FWRCreatureDef* D = C ? C->GetDef() : nullptr;
	if (!D) return Killed && Killed->GetPlayerState() ? VP(TEXT("feed.highland"), 8.f) : 0.f;
	const FString Role = D->Role;
	if (Cast<AWRBoss>(C)) return Role.Contains(TEXT("major")) ? VP(TEXT("feed.major"), 60.f) : VP(TEXT("feed.mini"), 25.f);
	return D->Tier == TEXT("highland") ? VP(TEXT("feed.highland"), 8.f) : D->Tier == TEXT("midland") ? VP(TEXT("feed.midland"), 5.f) : VP(TEXT("feed.lowland"), 3.f);
}

void WRVoidPath::OnKill(AWRCharacterBase* Target, const FWRHit& Hit)
{
	AWRCharacterBase* Killer = Cast<AWRCharacterBase>(Hit.Instigator);
	if (!Killer || Killer == Target || !UWRPathComponent::HasNode(Killer, PathId, TEXT("feed"))) return;
	const UWRAttributeSet* A = Killer->GetAttributes();
	if (!A || A->GetMaxHealthLost() <= 0.f) return;
	// Feed: a kill wins back some of what the Void took.
	WRVoid::RestoreMaxHealth(Killer, FeedFor(Target));
	WRFx::BroadcastOn(Killer, TEXT("void.feed"), TEXT("spine"), 1.f, TEXT("void.feed"));
}

float WRVoidPath::LeechHeal(AWRCharacterBase* Who, float Amount)
{
	const UWRAttributeSet* A = Who ? Who->GetAttributes() : nullptr;
	if (!A || Amount <= 0.f || !Who->HasTag(Combo_VoidLeech) || A->GetMaxHealthLost() <= 0.f) return Amount;
	const float Back = FMath::Min(A->GetMaxHealthLost(), Amount * VP(TEXT("combos.voidLeechShare"), 0.3f));
	WRVoid::RestoreMaxHealth(Who, Back);
	return Amount - Back;
}

float WRVoidPath::FadeSeconds(const AActor* Who)
{
	return UWRPathComponent::Stat(Who, TEXT("fadeIFrames"));
}

void WRVoidPath::Unseen(AWRCharacterBase* Who)
{
	const float Per = UWRPathComponent::Stat(Who, TEXT("unseenSeconds"));
	if (!Who || Per <= 0.f || !Who->HasAuthority()) return;
	// Creatures within 15 m lose you for 0.6 s a rank (bosses half); they look for you where you were.
	for (TActorIterator<AWRCreature> It(Who->GetWorld()); It; ++It)
	{
		if (It->GetAggroTarget() != Who || FVector::Dist(It->GetActorLocation(), Who->GetActorLocation()) > WRTune(TEXT("voidPath.unseenRange"), 1500.f)) continue;
		It->LoseSightOf(Who, Per * (Cast<AWRBoss>(*It) ? VP(TEXT("unseenBossShare"), 0.5f) : 1.f));
	}
}

// ---------------------------------------------------------------- the body

namespace WRVoidPathImpl
{
	/** 2.18's live driver: max(Void / 100, band / max HP x 2), 0..1. */
	float Live(const AActor* Hero)
	{
		const AWRCharacterBase* C = Cast<AWRCharacterBase>(Hero);
		const UWRAttributeSet* A = C ? C->GetAttributes() : nullptr;
		if (!A) return 0.f;
		const float Bar = A->GetVoid() / FMath::Max(1.f, A->GetMaxVoid() > 0.f ? A->GetMaxVoid() : 100.f);
		const float Band = A->GetMaxHealthLost() / FMath::Max(1.f, A->GetMaxHealth()) * 2.f;
		return FMath::Clamp(FMath::Max(Bar, Band), 0.f, 1.f);
	}
}

uint32 WRVoidPath::LookSig(const AActor* Hero)
{
	return Walks(Hero) ? uint32(FMath::RoundToInt(Live(Hero) * 20.f)) + 7u : 0u;
}

void WRVoidPath::ApplyLook(const AActor* Hero, const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Skin, FName Main, float MainS, FName Sub, float SubS)
{
	if (Main != PathId && Sub != PathId) return;
	// The stage is the cap; the bar and the band move the look within it, never below the stage's floor (0.3 of it):
	// greying from the fingertips, the black fingertips, the veins (the skin material's Corruption).
	const float Cap = Main == PathId ? MainS : SubS * 0.6f;
	const float Look = Cap * FMath::Lerp(0.3f, 1.f, Live(Hero));
	for (UMaterialInstanceDynamic* MID : Skin)
	{
		if (MID) MID->SetScalarParameterValue(TEXT("Corruption"), Look);
	}
}
