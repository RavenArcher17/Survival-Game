#include "WRBloodPath.h"
#include "WRVoidPath.h"
#include "Wildreach.h"
#include "WRAttributeSet.h"
#include "WRCharacter.h"
#include "WRCharacterBase.h"
#include "WRCombatLibrary.h"
#include "WRCombos.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRInventory.h"
#include "WRLightningPath.h"
#include "WRPathComponent.h"
#include "WRPathTypes.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRTuning.h"
#include "WRUIStyle.h"
#include "WRUIWidgets.h"
#include "AbilitySystemComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Rendering/DrawElements.h"
#include "TimerManager.h"
#include "Widgets/SLeafWidget.h"

namespace WRBlood
{
	UE_DEFINE_GAMEPLAY_TAG(Element_Blood, "Element.Blood");
	UE_DEFINE_GAMEPLAY_TAG(Ability_LeechStrike, "Ability.Blood.LeechStrike");
	UE_DEFINE_GAMEPLAY_TAG(Ability_CrimsonSlash, "Ability.Blood.CrimsonSlash");
	UE_DEFINE_GAMEPLAY_TAG(Status_SanguineLord, "Status.SanguineLord");
	UE_DEFINE_GAMEPLAY_TAG(Status_IceVeins, "Status.IceVeins");
	UE_DEFINE_GAMEPLAY_TAG(Combo_HexFlame, "Combo.HexFlame");
	UE_DEFINE_GAMEPLAY_TAG(Combo_BurningLifesteal, "Combo.BurningLifesteal");
	UE_DEFINE_GAMEPLAY_TAG(Combo_ColdBlooded, "Combo.ColdBlooded");
	UE_DEFINE_GAMEPLAY_TAG(Combo_IceVeins, "Combo.IceVeins");
	UE_DEFINE_GAMEPLAY_TAG(Combo_Adrenaline, "Combo.Adrenaline");
	UE_DEFINE_GAMEPLAY_TAG(Combo_RedLightning, "Combo.RedLightning");

	const FName DoomMarker(TEXT("blood_doom"));
}

namespace WRBloodImpl
{
	static const FName PathId(TEXT("blood"));

	/** The garnet of the path (#7A0A14, never crimson: crimson means unblockable). */
	static FLinearColor Garnet() { return FLinearColor(FColor(0x7A, 0x0A, 0x14)); }

	static float Num(const TCHAR* Key, float Fallback) { return FWRTuning::Get().Num(Key, Fallback); }
	static float Passive(const TCHAR* Field, float Fallback) { return FWRTuning::Get().Num(FString(TEXT("blood.passives.")) + Field, Fallback); }
	static float Look(const TCHAR* Field, float Fallback) { return FWRTuning::Get().Num(FString(TEXT("blood.look.")) + Field, Fallback); }

	/** The server's world time on either machine (the window and fight clocks are written in it). */
	static double ServerNow(const UObject* Ctx)
	{
		const UWorld* World = Ctx ? Ctx->GetWorld() : nullptr;
		if (!World) return 0.0;
		if (const AGameStateBase* GS = World->GetGameState()) return GS->GetServerWorldTimeSeconds();
		return World->GetTimeSeconds();
	}

	static UWRPathComponent* PathsOf(const AActor* Who) { return UWRPathComponent::FindFor(Who); }

	static const UWRAttributeSet* AttrOf(const AActor* Who)
	{
		const AWRCharacterBase* C = Cast<AWRCharacterBase>(Who);
		return C ? C->GetAttributes() : nullptr;
	}

	static float HealthFraction(const AActor* Who)
	{
		const UWRAttributeSet* A = AttrOf(Who);
		return A && A->GetMaxHealth() > 0.f ? FMath::Clamp(A->GetHealth() / A->GetMaxHealth(), 0.f, 1.f) : 1.f;
	}

	static bool IsBoss(const AWRCharacterBase* Who) { return Who && UWRCombatLibrary::StanceKind(Who) != TEXT("normal"); }

	static FVector Chest(const AActor* Who) { return Who->GetActorLocation() + FVector(0.f, 0.f, 30.f); }
	static FVector Hand(const AActor* Who) { return Who->GetActorLocation() + FVector(0.f, 0.f, 40.f) + Who->GetActorForwardVector() * 40.f; }

	/** A thread of garnet (or hex black-purple) laid from A to B: the recipe lays along X over 100 cm x scale. */
	static void Thread(const UObject* Ctx, FName Effect, const FVector& A, const FVector& B, FName Sound)
	{
		const FVector D = B - A;
		if (!D.IsNearlyZero())
		{
			WRFx::Broadcast(Ctx, Effect, A, D.Rotation(), D.Size() / 100.f, Sound);
		}
	}

	// ---- server bookkeeping
	static TMap<TWeakObjectPtr<const AActor>, TWeakObjectPtr<AWRCharacterBase>>& Cursers()
	{
		static TMap<TWeakObjectPtr<const AActor>, TWeakObjectPtr<AWRCharacterBase>> Map;
		return Map;
	}

	struct FDoom
	{
		TWeakObjectPtr<AWRCharacterBase> By;
		float Taken = 0.f;
		int32 Serial = 0;
	};
	static TMap<TWeakObjectPtr<AWRCharacterBase>, FDoom>& Dooms()
	{
		static TMap<TWeakObjectPtr<AWRCharacterBase>, FDoom> Map;
		return Map;
	}

	static TArray<const AActor*>& ChainStack()
	{
		static TArray<const AActor*> Stack;
		return Stack;
	}

	/** Once every Seconds per actor (server-side clocks for the procs' own cooldowns and throttles). */
	static bool Every(TMap<TWeakObjectPtr<const AActor>, double>& Clock, const AActor* Who, double Now, float Seconds)
	{
		if (const double* Last = Clock.Find(Who); Last && Now - *Last < Seconds)
		{
			return false;
		}
		Clock.Add(Who, Now);
		return true;
	}

	static void SetHealth(AWRCharacterBase* Who, float Value)
	{
		if (UAbilitySystemComponent* ASC = Who ? Who->GetAbilitySystemComponent() : nullptr)
		{
			ASC->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), Value);
		}
	}

	/** Plain healing (Transfusion, Feast): x healing taken, up to max health. */
	static void PlainHeal(AWRCharacterBase* Who, float Amount)
	{
		const UWRAttributeSet* A = Who ? Who->GetAttributes() : nullptr;
		if (!A || !Who->IsAlive() || Amount <= 0.f) return;
		SetHealth(Who, FMath::Min(A->GetMaxHealth(), A->GetHealth() + Amount * Who->Status->HealingTakenScale()));
	}

	/** "blood_bolt" -> "bloodBolt" (the spells.<key> rows). */
	static FString Camel(const FString& Id)
	{
		FString Out;
		bool bUpper = false;
		for (const TCHAR C : Id)
		{
			if (C == TEXT('_')) { bUpper = true; continue; }
			Out += bUpper ? FChar::ToUpper(C) : C;
			bUpper = false;
		}
		return Out;
	}

	/** The rally window's length: 8 s, 12 s with Clotting. */
	static float WindowSeconds(const AActor* Who)
	{
		return UWRPathComponent::HasNode(Who, PathId, TEXT("clotting")) ? Num(TEXT("blood.resources.clottingWindow"), 12.f) : Num(TEXT("blood.resources.spilledWindow"), 8.f);
	}

	static void MarkFight(const AActor* Who)
	{
		UWRPathComponent* P = PathsOf(Who);
		if (P && UWRBloodPath::Walks(Who))
		{
			const float Until = float(ServerNow(Who)) + Passive(TEXT("fightSeconds"), 8.f);
			if (Until - P->BloodFightUntil > 1.f) P->BloodFightUntil = Until;   // only a real change replicates
		}
	}
}

// ================================================================================================ spend and heal

bool UWRBloodPath::Walks(const AActor* Who)
{
	const UWRPathComponent* P = WRBloodImpl::PathsOf(Who);
	return P && P->IsActivePath(WRBloodImpl::PathId);
}

bool UWRBloodPath::IsMain(const AActor* Who)
{
	const UWRPathComponent* P = WRBloodImpl::PathsOf(Who);
	return P && P->GetMainPath() == WRBloodImpl::PathId;
}

int32 UWRBloodPath::BloodPoints(const AActor* Who)
{
	const UWRPathComponent* P = WRBloodImpl::PathsOf(Who);
	return P && P->IsActivePath(WRBloodImpl::PathId) ? P->GetSpent(WRBloodImpl::PathId) : 0;
}

int32 UWRBloodPath::BloodNodes(const AActor* Who)
{
	const UWRPathComponent* P = WRBloodImpl::PathsOf(Who);
	const FWRPathProgress* Progress = P && P->IsActivePath(WRBloodImpl::PathId) ? P->GetProgress(WRBloodImpl::PathId) : nullptr;
	int32 Count = 0;
	if (Progress)
	{
		for (const FWRNodeRank& R : Progress->Ranks)
		{
			Count += R.Rank > 0 ? 1 : 0;
		}
	}
	return Count;
}

float UWRBloodPath::HpCost(FName NodeId)
{
	return FMath::Max(0.f, FWRTuning::Get().Num(FString::Printf(TEXT("spells.%s.hpCost"), *WRBloodImpl::Camel(NodeId.ToString())), 0.f));
}

bool UWRBloodPath::CanSpend(const AActor* Who, float Cost)
{
	if (Cost <= 0.f) return true;
	const UWRAttributeSet* A = WRBloodImpl::AttrOf(Who);
	// No floor (2.17): any HP above the cost pays, and the cast may leave you at 1.
	return A && A->GetHealth() > Cost;
}

bool UWRBloodPath::SpendBlood(AWRCharacterBase* Who, float Cost, bool bSpilled)
{
	if (!Who || !Who->HasAuthority() || !Who->IsAlive())
	{
		return false;
	}
	if (Cost <= 0.f)
	{
		return true;
	}
	const UWRAttributeSet* A = Who->GetAttributes();
	if (!A || A->GetHealth() <= Cost)
	{
		return false;
	}
	const float NewHealth = A->GetHealth() - Cost;
	WRBloodImpl::SetHealth(Who, NewHealth);
	if (UWRPathComponent* P = bSpilled ? WRBloodImpl::PathsOf(Who) : nullptr)
	{
		P->BloodSpilled = FMath::Min(P->BloodSpilled + Cost, FMath::Max(0.f, A->GetMaxHealth() - NewHealth));
		P->BloodWindowEnd = float(WRBloodImpl::ServerNow(Who)) + WRBloodImpl::WindowSeconds(Who);
	}
	// Adrenaline (Lightning + Blood 25): spending HP fills Charge. Cold blooded (Frost + Blood 25): spending HP fills Chill.
	if (Who->HasTag(WRBlood::Combo_Adrenaline))
	{
		WRLightning::GainCharge(Who, Cost * FWRComboRegistry::Get().Num(TEXT("adrenaline"), TEXT("chargePerHp"), 2.f));
	}
	if (Who->HasTag(WRBlood::Combo_ColdBlooded))
	{
		UWRCombatLibrary::GainResource(Who, WRTags::Element_Frost, Cost * FWRComboRegistry::Get().Num(TEXT("cold_blooded"), TEXT("chillPerHp"), 1.5f));
	}
	UE_LOG(LogWildreach, Verbose, TEXT("BLOOD %s spends %.0f HP (hp %.0f, spilled %.0f)"), *Who->GetName(), Cost, NewHealth, Spilled(Who));
	return true;
}

void UWRBloodPath::Heal(AWRCharacterBase* Who, float Amount, bool bLifesteal, const AActor* From)
{
	const UWRAttributeSet* A = Who ? Who->GetAttributes() : nullptr;
	if (!A || !Who->HasAuthority() || !Who->IsAlive() || Amount <= 0.f)
	{
		return;
	}
	// Void leech (Blood + Void 25): a share of lifesteal wins back max HP the Void took first.
	if (bLifesteal)
	{
		Amount = WRVoidPath::LeechHeal(Who, Amount);
		if (Amount <= 0.f) return;
	}
	Amount *= Who->Status->HealingTakenScale();
	float Gain = 0.f;
	UWRPathComponent* P = WRBloodImpl::PathsOf(Who);
	const double Now = WRBloodImpl::ServerNow(Who);
	if (bLifesteal && P && P->BloodSpilled > 0.f && Now < P->BloodWindowEnd)
	{
		// Inside the rally window lifesteal wins the Spilled HP back at x2; once it's all back, it heals at x1.
		const float Rally = FMath::Max(1.f, WRBloodImpl::Num(TEXT("blood.resources.rallyMultiplier"), 2.f));
		const float Refill = FMath::Min(P->BloodSpilled, Amount * Rally);
		P->BloodSpilled -= Refill;
		Amount -= Refill / Rally;
		Gain += Refill;
	}
	Gain += Amount;
	WRBloodImpl::SetHealth(Who, FMath::Min(A->GetMaxHealth(), A->GetHealth() + Gain));
	if (!bLifesteal)
	{
		return;
	}
	if (P)
	{
		++P->BloodStealSerial;   // the body's flush on every machine
	}
	// The threads: thin garnet lines drawn from the target into your hand for 0.2 s, and a soft wet draw (not every hit
	// of a fast weapon: threadEvery).
	static TMap<TWeakObjectPtr<const AActor>, double> LastThread;
	if (From && WRBloodImpl::Every(LastThread, Who, Now, WRBloodImpl::Passive(TEXT("threadEvery"), 0.25f)))
	{
		WRBloodImpl::Thread(Who, TEXT("blood.thread"), WRBloodImpl::Chest(From), WRBloodImpl::Hand(Who), TEXT("blood.draw"));
	}
	// The heal itself, heard on the body (throttled: a fast weapon's every hit would be a drone).
	static TMap<TWeakObjectPtr<const AActor>, double> LastStealSound;
	if (WRBloodImpl::Every(LastStealSound, Who, Now, WRBloodImpl::Passive(TEXT("lifestealSoundEvery"), 0.6f)))
	{
		WRSound::Broadcast(Who, TEXT("blood.lifesteal"));
	}
	// Ice veins (Frost + Blood 50): lifesteal grants ice armour for a few seconds.
	static TMap<TWeakObjectPtr<const AActor>, double> LastIce;
	if (Who->HasTag(WRBlood::Combo_IceVeins) && WRBloodImpl::Every(LastIce, Who, Now, FWRComboRegistry::Get().Num(TEXT("ice_veins"), TEXT("cooldown"), 6.f)))
	{
		const float Seconds = FWRComboRegistry::Get().Num(TEXT("ice_veins"), TEXT("seconds"), 4.f);
		Who->Status->ApplyStatus(WRBlood::Status_IceVeins, Seconds);
		WRFx::BroadcastOn(Who, TEXT("blood.iceveins"), NAME_None, 1.f, TEXT("blood.iceveins"), Seconds);
	}
}

float UWRBloodPath::Spilled(const AActor* Who)
{
	const UWRPathComponent* P = WRBloodImpl::PathsOf(Who);
	return P && WRBloodImpl::ServerNow(Who) < P->BloodWindowEnd ? FMath::Max(0.f, P->BloodSpilled) : 0.f;
}

float UWRBloodPath::WindowLeft(const AActor* Who)
{
	const UWRPathComponent* P = WRBloodImpl::PathsOf(Who);
	return P && P->BloodSpilled > 0.f ? FMath::Max(0.f, float(P->BloodWindowEnd - WRBloodImpl::ServerNow(Who))) : 0.f;
}

float UWRBloodPath::WindowFraction(const AActor* Who)
{
	return FMath::Clamp(WindowLeft(Who) / FMath::Max(0.1f, WRBloodImpl::WindowSeconds(Who)), 0.f, 1.f);
}

float UWRBloodPath::BleedBurst(const AWRCharacterBase* Target, const AActor* Source)
{
	const UWRAttributeSet* A = Target ? Target->GetAttributes() : nullptr;
	if (!A) return 0.f;
	const FWRTuning& T = FWRTuning::Get();
	return (A->GetMaxHealth() * T.Num(TEXT("status.defs.bleed.burstPct"), 0.12f) + T.Num(TEXT("status.defs.bleed.burstFlat"), 15.f))
		* (WRBloodImpl::IsBoss(Target) ? T.Num(TEXT("status.defs.bleed.bossScale"), 0.7f) : 1.f) * WRBlood::BleedBurstScale(Source);
}

// ================================================================================================ Doom

void UWRBloodPath::MarkDoom(AWRCharacterBase* By, AWRCharacterBase* Target)
{
	if (!By || !Target || !Target->HasAuthority() || !Target->IsAlive())
	{
		return;
	}
	static int32 Serials = 0;
	WRBloodImpl::FDoom& D = WRBloodImpl::Dooms().FindOrAdd(Target);
	D.By = By;
	D.Taken = 0.f;
	D.Serial = ++Serials;
	const int32 Serial = D.Serial;
	const float Delay = WRBloodImpl::Num(TEXT("spells.doom.delay"), 4.f);
	// A black-purple sigil over it counting down three pulses, a low toll on each (2.17 proc feedback).
	WRFx::BroadcastOn(Target, TEXT("blood.doom"), NAME_None, 1.f, TEXT("blood.doom_toll"), Delay);
	UE_LOG(LogWildreach, Log, TEXT("BLOOD doom on %s by %s (%.1f s)"), *Target->GetName(), *By->GetName(), Delay);
	TWeakObjectPtr<AWRCharacterBase> WeakTarget(Target);
	FTimerManager& Timers = Target->GetWorld()->GetTimerManager();
	for (int32 Pulse = 1; Pulse <= 2; ++Pulse)
	{
		FTimerHandle H;
		Timers.SetTimer(H, FTimerDelegate::CreateLambda([WeakTarget, Serial]()
		{
			AWRCharacterBase* T = WeakTarget.Get();
			const WRBloodImpl::FDoom* Now = T ? WRBloodImpl::Dooms().Find(T) : nullptr;
			if (T && T->IsAlive() && Now && Now->Serial == Serial) WRSound::Broadcast(T, TEXT("blood.doom_toll"));
		}), Delay * Pulse / 3.f, false);
	}
	FTimerHandle Burst;
	Timers.SetTimer(Burst, FTimerDelegate::CreateLambda([WeakTarget, Serial]()
	{
		AWRCharacterBase* T = WeakTarget.Get();
		WRBloodImpl::FDoom* Found = T ? WRBloodImpl::Dooms().Find(T) : nullptr;
		if (!Found || Found->Serial != Serial)
		{
			return;
		}
		const WRBloodImpl::FDoom Mark = *Found;
		WRBloodImpl::Dooms().Remove(T);
		AWRCharacterBase* Caster = Mark.By.Get();
		if (!T->IsAlive())
		{
			return;
		}
		// 60 + 25% of what it took in those seconds; bosses x0.6; x1.3 on a cursed body. A burst of damage, never a
		// "dies outright" (CAL: no instant kills).
		const FWRTuning& Tn = FWRTuning::Get();
		float Damage = Tn.Num(TEXT("spells.doom.base"), 60.f) + Tn.Num(TEXT("spells.doom.takenShare"), 0.25f) * Mark.Taken;
		if (WRBloodImpl::IsBoss(T)) Damage *= Tn.Num(TEXT("spells.doom.bossScale"), 0.6f);
		if (T->HasTag(WRTags::Status_Curse)) Damage *= Tn.Num(TEXT("spells.doom.cursedScale"), 1.3f);
		WRFx::BroadcastOn(T, TEXT("blood.doom_crack"), NAME_None, 1.f, TEXT("blood.doom_crack"));
		FWRHit Hit;
		Hit.Instigator = Caster;
		Hit.Damage = Damage;
		Hit.PoiseDamage = 30.f;
		Hit.Element = WRBlood::Element_Blood;
		Hit.PathId = WRBloodImpl::PathId;
		Hit.bSpell = true;
		Hit.bReaction = true;
		Hit.bUnblockable = true;
		Hit.Impact = 2;
		Hit.Tier = EWRHitTier::Heavy;
		Hit.Origin = T->GetActorLocation();
		Hit.ImpactPoint = T->GetActorLocation();
		UE_LOG(LogWildreach, Log, TEXT("BLOOD doom bursts on %s for %.0f (took %.0f)"), *T->GetName(), Damage, Mark.Taken);
		UWRCombatLibrary::ApplyHit(T, Hit);
	}), Delay, false);
}

// ================================================================================================ hooks

float WRBlood::DamageScale(const AWRCharacterBase* Attacker, const AWRCharacterBase* Target, const FWRHit& Hit)
{
	float Scale = 1.f;
	// Sanguine power: Blood spell damage (+6% a rank).
	if (Attacker && Hit.PathId == WRBloodImpl::PathId && (Hit.bSpell || !Hit.bMelee))
	{
		Scale *= 1.f + UWRPathComponent::Stat(Attacker, TEXT("bloodSpellDamage"));
	}
	if (Target)
	{
		// Ruin: a body cursed by someone with Ruin takes more from everyone (bosses less).
		if (Target->HasTag(WRTags::Status_Curse))
		{
			const AWRCharacterBase* Curser = CursedBy(Target);
			if (Curser && UWRPathComponent::HasNode(Curser, WRBloodImpl::PathId, TEXT("ruin")))
			{
				Scale *= 1.f + (WRBloodImpl::IsBoss(Target) ? WRBloodImpl::Passive(TEXT("ruinBossBonus"), 0.15f) : WRBloodImpl::Passive(TEXT("ruinBonus"), 0.25f));
			}
		}
		// Ice veins: the ice armour takes some off.
		if (Target->HasTag(Status_IceVeins))
		{
			Scale *= FMath::Clamp(FWRComboRegistry::Get().Num(TEXT("ice_veins"), TEXT("damageTaken"), 0.85f), 0.f, 1.f);
		}
	}
	return Scale;
}

bool WRBlood::Whiffs(AWRCharacterBase* Target, const FWRHit& Hit)
{
	AWRCharacterBase* Attacker = Cast<AWRCharacterBase>(Hit.Instigator);
	if (!Target || !Attacker || Attacker == Target || Hit.bDamageOverTime || Hit.bReaction || !Attacker->HasTag(WRTags::Status_Curse))
	{
		return false;
	}
	const AWRCharacterBase* Curser = CursedBy(Attacker);
	if (!Curser || !UWRPathComponent::HasNode(Curser, WRBloodImpl::PathId, TEXT("misfortune")))
	{
		return false;
	}
	const bool bPlayer = Attacker->GetPlayerState() != nullptr;
	if (!bPlayer && !Attacker->IsCreature())
	{
		return false;
	}
	const float Chance = bPlayer ? WRBloodImpl::Passive(TEXT("misfortunePvp"), 0.1f) : WRBloodImpl::Passive(TEXT("misfortuneChance"), 0.2f);
	if (FMath::FRand() >= Chance)
	{
		return false;
	}
	// The whiff is seen: the blow goes wide on its contact frame, the body stumbles (not a boss), a black-purple hex spark
	// where it should have landed, a hollow whiff.
	const FVector At = !Hit.ImpactPoint.IsZero() ? Hit.ImpactPoint : (Target->GetActorLocation() + Attacker->GetActorLocation()) * 0.5f;
	WRFx::Broadcast(Target, TEXT("blood.misfortune"), At, (Target->GetActorLocation() - Attacker->GetActorLocation()).Rotation(), 1.f, TEXT("blood.misfortune_whiff"));
	if (!bPlayer && !WRBloodImpl::IsBoss(Attacker))
	{
		Attacker->Status->Stagger(WRBloodImpl::Passive(TEXT("misfortuneStumble"), 0.5f));
	}
	UE_LOG(LogWildreach, Log, TEXT("BLOOD misfortune: %s's blow on %s whiffs"), *Attacker->GetName(), *Target->GetName());
	return true;
}

void WRBlood::AfterDamage(AWRCharacterBase* Target, const FWRHit& Hit, float Damage, bool bLanded)
{
	if (!Target || Damage <= 0.f)
	{
		return;
	}
	AWRCharacterBase* Attacker = Cast<AWRCharacterBase>(Hit.Instigator);
	// Doom's tally: everything the marked body takes while the sigil turns.
	if (WRBloodImpl::FDoom* D = WRBloodImpl::Dooms().Find(Target))
	{
		D->Taken += Damage;
	}
	// The heartbeat's fight clock (both sides of a blow).
	WRBloodImpl::MarkFight(Target);
	if (Attacker && Attacker != Target)
	{
		WRBloodImpl::MarkFight(Attacker);
	}
	if (!Attacker || Attacker == Target || !Attacker->IsAlive() || !Attacker->GetPlayerState())
	{
		return;
	}
	// One lifesteal pipeline (2.17): every source adds together, then it goes through Spilled first. Base lifesteal is 0.
	float Share = 0.f;
	if (UWRBloodPath::IsMain(Attacker))
	{
		// Bloodthirst: 0% at >= 90% HP, rising in a straight line to 15% at <= 20%.
		const float Full = WRBloodImpl::Passive(TEXT("bloodthirstFullAt"), 0.9f), Low = WRBloodImpl::Passive(TEXT("bloodthirstMaxAt"), 0.2f);
		const float T = FMath::Clamp((Full - WRBloodImpl::HealthFraction(Attacker)) / FMath::Max(0.01f, Full - Low), 0.f, 1.f);
		Share += WRBloodImpl::Passive(TEXT("bloodthirstMax"), 0.15f) * T;
	}
	if (Hit.bMelee && bLanded)
	{
		Share += UWRPathComponent::Stat(Attacker, TEXT("lifesteal"));   // Lifesteal: weapon and Wildbrand hits
	}
	if (Hit.bWeaponArt && bLanded && Attacker->HasTag(Ability_LeechStrike))
	{
		Share += WRBloodImpl::Num(TEXT("spells.leechStrike.heal"), 0.5f);
	}
	if (Attacker->HasTag(Status_SanguineLord))
	{
		Share += WRBloodImpl::Num(TEXT("spells.sanguineLord.share"), 0.3f);   // all damage, spells and bleed included
	}
	if (bLanded && !Hit.bDamageOverTime && !Hit.bReaction)
	{
		// Worn lifesteal (Rune of Thirst, Mirehide's four pieces, the Mire-witch's ring: gear / item stat "lifesteal").
		Share += UWRInventoryComponent::GearStatFor(Attacker, TEXT("lifesteal"));
	}
	if (Hit.bDamageOverTime && Hit.Element == WRTags::Element_Fire && Attacker->HasTag(Combo_BurningLifesteal))
	{
		Share += FWRComboRegistry::Get().Num(TEXT("burning_lifesteal"), TEXT("share"), 0.1f);
	}
	if (Share <= 0.f)
	{
		return;
	}
	if (Target->GetPlayerState())
	{
		Share *= WRBloodImpl::Passive(TEXT("pvpLifesteal"), 0.5f);   // the PvP block
	}
	UWRBloodPath::Heal(Attacker, Damage * Share, true, Target);
}

void WRBlood::AfterHit(AWRCharacterBase* Target, const FWRHit& Hit, float Damage)
{
	AWRCharacterBase* Attacker = Cast<AWRCharacterBase>(Hit.Instigator);
	if (!Target || !Attacker || Attacker == Target || Hit.bDamageOverTime || !Target->HasAuthority())
	{
		return;
	}
	// A Doom bolt landed: its mark.
	if (Hit.Buildups.Contains(DoomMarker))
	{
		UWRBloodPath::MarkDoom(Attacker, Target);
	}
	// A Blood spell's or Wildbrand's garnet splash.
	if (Hit.PathId == WRBloodImpl::PathId && !Hit.bReaction && Damage > 0.f)
	{
		WRFx::BroadcastOn(Target, TEXT("blood.hit"), NAME_None, Hit.bMelee ? 0.8f : 1.f, Hit.bMelee ? NAME_None : FName(TEXT("blood.hit")));
	}
	if (!Target->IsAlive())
	{
		return;
	}
	float Bleed = 0.f;
	// Bleed chance (trunk): +10% on the bleed your hits build, and every weapon hit builds a little (any weapon can bleed).
	if (Attacker->GetPlayerState() && !Hit.bReaction)
	{
		Bleed += Hit.Buildups.FindRef(TEXT("bleed")) * UWRPathComponent::Stat(Attacker, TEXT("bleedBuildup"));
		if (Hit.bMelee) Bleed += UWRPathComponent::Stat(Attacker, TEXT("weaponBleed"));
	}
	// Red lightning (Lightning + Blood 50): Chain lightning bleeds every body it hits.
	if (Hit.Element == WRTags::Element_Lightning && Attacker->HasTag(Combo_RedLightning) && WRBloodImpl::ChainStack().Contains(Attacker))
	{
		Bleed += FWRComboRegistry::Get().Num(TEXT("red_lightning"), TEXT("bleed"), 35.f);
		WRFx::BroadcastOn(Target, TEXT("blood.hit"), NAME_None, 0.7f);
	}
	if (Bleed > 0.f)
	{
		if (Target->GetPlayerState()) Bleed *= WRTune(TEXT("pvp.buildup"), 0.7f);
		Target->Status->AddStatusBuildup(WRStatus::Bleed, Bleed, Attacker, Damage);
	}
}

void WRBlood::OnKill(AWRCharacterBase* Target, const FWRHit& Hit)
{
	if (!Target || !Target->HasAuthority())
	{
		return;
	}
	WRBloodImpl::Dooms().Remove(Target);
	AWRCharacterBase* Attacker = Cast<AWRCharacterBase>(Hit.Instigator);
	UWorld* World = Target->GetWorld();
	const double Now = World->GetTimeSeconds();

	// Feast: a kill by melee, Wildbrand or riposte heals 15% max HP + 10 (a mini-boss 30%), once every 3 s.
	static TMap<TWeakObjectPtr<const AActor>, double> LastFeast;
	if (Attacker && Attacker != Target && Attacker->IsAlive() && (Hit.bMelee || Hit.bWeaponArt || Hit.bCritical) && !Hit.bDamageOverTime
		&& UWRPathComponent::HasNode(Attacker, WRBloodImpl::PathId, TEXT("feast"))
		&& WRBloodImpl::Every(LastFeast, Attacker, Now, WRBloodImpl::Passive(TEXT("feastCooldown"), 3.f)))
	{
		const UWRAttributeSet* A = Attacker->GetAttributes();
		const bool bMini = UWRCombatLibrary::StanceKind(Target) == TEXT("mini");
		const float Heal = A ? A->GetMaxHealth() * (bMini ? WRBloodImpl::Passive(TEXT("feastMini"), 0.3f) : WRBloodImpl::Passive(TEXT("feastHeal"), 0.15f)) + WRBloodImpl::Passive(TEXT("feastFlat"), 10.f) : 0.f;
		WRBloodImpl::PlainHeal(Attacker, Heal);
		// A garnet burst from the kill into you, a heartbeat thump.
		WRBloodImpl::Thread(Attacker, TEXT("blood.thread"), WRBloodImpl::Chest(Target), WRBloodImpl::Chest(Attacker), NAME_None);
		WRFx::BroadcastOn(Attacker, TEXT("blood.feast"), NAME_None, 1.f, TEXT("blood.feast_heartbeat"));
		UE_LOG(LogWildreach, Log, TEXT("BLOOD feast: %s heals %.0f from %s"), *Attacker->GetName(), Heal, *Target->GetName());
		// The short drinking flourish, only when the fight is over: a moment after the kill, no living enemy near and not
		// mid-swing (never costs you a beat in a fight).
		TWeakObjectPtr<AWRCharacterBase> WeakFeaster(Attacker);
		FTimerHandle Flourish;
		World->GetTimerManager().SetTimer(Flourish, FTimerDelegate::CreateLambda([WeakFeaster]()
		{
			AWRCharacter* Hero = Cast<AWRCharacter>(WeakFeaster.Get());
			if (!Hero || !Hero->IsAlive() || Hero->IsSwinging() || Hero->HasTag(WRTags::State_Attacking) || Hero->HasTag(WRTags::State_Dodging))
			{
				return;
			}
			const float Calm = WRBloodImpl::Passive(TEXT("feastCalmRadius"), 1500.f);
			for (TActorIterator<AWRCharacterBase> It(Hero->GetWorld()); It; ++It)
			{
				if (*It != Hero && It->IsAlive() && Hero->IsHostileTo(*It) && FVector::DistSquared(It->GetActorLocation(), Hero->GetActorLocation()) < FMath::Square(Calm))
				{
					return;
				}
			}
			Hero->MulticastBodyOneShot(TEXT("drink"));
		}), FMath::Max(0.05f, WRBloodImpl::Passive(TEXT("feastFlourishDelay"), 0.6f)), false);
	}

	// Contagion: a cursed body dying passes its curse, at full duration, to up to 2 enemies within 8 m (a thread jumps).
	AWRCharacterBase* Curser = Target->HasTag(WRTags::Status_Curse) ? CursedBy(Target) : nullptr;
	if (Curser && UWRPathComponent::HasNode(Curser, WRBloodImpl::PathId, TEXT("contagion")))
	{
		const FVector From = WRBloodImpl::Chest(Target);
		const float Radius = WRBloodImpl::Passive(TEXT("contagionRadius"), 800.f);
		TArray<TPair<float, AWRCharacterBase*>> Near;
		for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
		{
			const float D = FVector::Dist(It->GetActorLocation(), Target->GetActorLocation());
			if (*It != Target && *It != Curser && It->IsAlive() && !It->HasTag(WRTags::Status_Curse) && Curser->IsHostileTo(*It) && D < Radius)
			{
				Near.Add({ D, *It });
			}
		}
		Near.Sort([](const TPair<float, AWRCharacterBase*>& A, const TPair<float, AWRCharacterBase*>& B) { return A.Key < B.Key; });
		const int32 Count = FMath::Min(Near.Num(), FMath::RoundToInt(WRBloodImpl::Passive(TEXT("contagionTargets"), 2.f)));
		for (int32 i = 0; i < Count; ++i)
		{
			AWRCharacterBase* Next = Near[i].Value;
			WRBloodImpl::Thread(Target, TEXT("blood.contagion"), From, WRBloodImpl::Chest(Next), i == 0 ? FName(TEXT("blood.contagion_hiss")) : NAME_None);
			TWeakObjectPtr<AWRCharacterBase> WeakNext(Next), WeakCurser(Curser);
			FTimerHandle H;
			World->GetTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([WeakNext, WeakCurser]()
			{
				AWRCharacterBase* N = WeakNext.Get();
				if (N && N->IsAlive())
				{
					// A full meter: the curse procs at its full duration (Lingering hex counted from its owner).
					N->Status->AddStatusBuildup(WRStatus::Curse, N->Status->Threshold(WRStatus::Curse), WeakCurser.Get());
				}
			}), FMath::Max(0.01f, WRBloodImpl::Passive(TEXT("contagionDelay"), 0.15f)), false);
		}
		if (Count > 0)
		{
			UE_LOG(LogWildreach, Log, TEXT("BLOOD contagion from %s to %d"), *Target->GetName(), Count);
		}
	}
	WRBloodImpl::Cursers().Remove(Target);
}

void WRBlood::TickOwner(AWRCharacterBase* Who, float DeltaTime)
{
	UWRPathComponent* P = Who && Who->HasAuthority() ? WRBloodImpl::PathsOf(Who) : nullptr;
	const UWRAttributeSet* A = Who ? Who->GetAttributes() : nullptr;
	if (P && A)
	{
		// Blood frenzy: crossing down into a faster step (below half, below a quarter) is heard once each time.
		static TMap<TWeakObjectPtr<const AActor>, int32> FrenzyStep;
		const float FrenzyScale = Who->IsAlive() ? SwingScale(Who) : 1.f;
		const int32 FrenzyNow = FrenzyScale >= 1.f ? 0 : FrenzyScale > WRBloodImpl::Passive(TEXT("frenzyQuarter"), 0.82f) + 0.001f ? 1 : 2;
		int32& Was = FrenzyStep.FindOrAdd(Who);
		if (FrenzyNow > Was)
		{
			WRSound::Broadcast(Who, TEXT("blood.frenzy"));
		}
		Was = FrenzyNow;
	}
	if (!P || !A || P->BloodSpilled <= 0.f)
	{
		return;
	}
	const double Now = WRBloodImpl::ServerNow(Who);
	if (!Who->IsAlive() || Now >= P->BloodWindowEnd || !P->IsActivePath(WRBloodImpl::PathId))
	{
		// The window closed: what lifesteal didn't win back is simply lost.
		P->BloodSpilled = 0.f;
		return;
	}
	// Clotting: the Spilled HP regrows by itself (2 HP a second) while the window lasts.
	if (UWRPathComponent::HasNode(Who, WRBloodImpl::PathId, TEXT("clotting")))
	{
		const float Grow = FMath::Min(P->BloodSpilled, WRBloodImpl::Num(TEXT("blood.resources.clottingRegen"), 2.f) * DeltaTime);
		P->BloodSpilled -= Grow;
		WRBloodImpl::SetHealth(Who, FMath::Min(A->GetMaxHealth(), A->GetHealth() + Grow));
	}
	// Healed by something else (a potion, a meal): the segment never runs past the bar.
	const float Room = FMath::Max(0.f, A->GetMaxHealth() - A->GetHealth());
	if (P->BloodSpilled > Room)
	{
		P->BloodSpilled = Room;
	}
}

float WRBlood::CurseSeconds(AWRCharacterBase* Owner, AWRCharacterBase* Source, float BaseSeconds)
{
	if (!Owner)
	{
		return BaseSeconds;
	}
	WRBloodImpl::Cursers().Add(Owner, Source);
	// A curse of the path's own taking hold (2.17 proc feedback): its cue on the body.
	if (Source && Owner->HasAuthority() && UWRBloodPath::Walks(Source))
	{
		WRSound::Broadcast(Owner, TEXT("blood.proc"));
	}
	// Lingering hex: +25% a rank (15 -> 26 s at 3).
	const float Seconds = BaseSeconds * (1.f + FMath::Max(0.f, UWRPathComponent::Stat(Source, TEXT("curseDuration"))));
	// Ruin: a slow-turning black-purple sigil over the cursed body for the whole party, a low hum near it.
	if (Source && UWRPathComponent::HasNode(Source, WRBloodImpl::PathId, TEXT("ruin")))
	{
		WRFx::BroadcastOn(Owner, TEXT("blood.ruin"), NAME_None, 1.f, TEXT("blood.ruin_hum"), Seconds);
	}
	return Seconds;
}

float WRBlood::BleedBurstScale(const AActor* Source, AActor* ProcOn)
{
	// A bleed of the path's own bursting (2.17 proc feedback): its cue on the body.
	if (ProcOn && ProcOn->HasAuthority() && Source && UWRBloodPath::Walks(Source))
	{
		WRSound::Broadcast(ProcOn, TEXT("blood.proc"));
	}
	return 1.f + FMath::Max(0.f, UWRPathComponent::Stat(Source, TEXT("bleedBurst")));   // Hemorrhage: +15% a rank
}

float WRBlood::BurnSecondsBonus(const AWRCharacterBase* Owner, const AWRCharacterBase* Source)
{
	return Owner && Source && Owner->HasTag(WRTags::Status_Curse) && Source->HasTag(Combo_HexFlame)
		? FWRComboRegistry::Get().Num(TEXT("hex_flame"), TEXT("burnSeconds"), 1.5f) : 0.f;
}

AWRCharacterBase* WRBlood::CursedBy(const AActor* Body)
{
	const TWeakObjectPtr<AWRCharacterBase>* Found = Body ? WRBloodImpl::Cursers().Find(Body) : nullptr;
	return Found ? Found->Get() : nullptr;
}

float WRBlood::SwingScale(const AActor* Who)
{
	if (!Who || !UWRPathComponent::HasNode(Who, WRBloodImpl::PathId, TEXT("blood_frenzy")))
	{
		return 1.f;
	}
	// Blood frenzy: below half health windups and recoveries x0.9, below a quarter x0.82. Hit-stop is untouched.
	const float H = WRBloodImpl::HealthFraction(Who);
	return H < 0.25f ? WRBloodImpl::Passive(TEXT("frenzyQuarter"), 0.82f) : H < 0.5f ? WRBloodImpl::Passive(TEXT("frenzyHalf"), 0.9f) : 1.f;
}

WRBlood::FChainScope::FChainScope(const AActor* By)
{
	WRBloodImpl::ChainStack().Add(By);
}

WRBlood::FChainScope::~FChainScope()
{
	if (WRBloodImpl::ChainStack().Num() > 0) WRBloodImpl::ChainStack().Pop();
}

// ================================================================================================ the HUD

namespace WRBloodUI
{
	static TMap<TWeakObjectPtr<const AActor>, double>& Refused()
	{
		static TMap<TWeakObjectPtr<const AActor>, double> Map;
		return Map;
	}

	/** The Spilled segment: drawn over the HP bar's own fill (SWRBar's insets), from where health ends for as much as was
	 *  spent; its right edge is the rally window, bright while it's fresh and fading as it runs out. */
	class SWRBloodSpilled : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SWRBloodSpilled) {}
		SLATE_END_ARGS()

		void Construct(const FArguments&, TFunction<const AActor*()> InWho) { Who = MoveTemp(InWho); }

		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1.f, 1.f); }

		virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
		{
			SLeafWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
			// The rally window opening (a spend with none running): its cue, for you alone (blood.rally is 2D).
			const AActor* Body = Who ? Who() : nullptr;
			const float Left = Body ? UWRBloodPath::WindowLeft(Body) : 0.f;
			if (Body && Left > 0.f && PrevLeft <= 0.f && UWRBloodPath::Spilled(Body) > 0.f)
			{
				WRSound::PlayUI(Body, TEXT("blood.rally"));
			}
			PrevLeft = Left;
		}

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override
		{
			const AActor* Body = Who ? Who() : nullptr;
			const UWRAttributeSet* A = WRBloodImpl::AttrOf(Body);
			const float Spilled = Body ? UWRBloodPath::Spilled(Body) : 0.f;
			if (!A || Spilled <= 0.f || A->GetMaxHealth() <= 0.f)
			{
				return Layer;
			}
			const FVector2D Size = Geo.GetLocalSize();
			const float Inner = Size.X - 2.f;
			const float Max = A->GetMaxHealth();
			const float Hp = FMath::Clamp(A->GetHealth(), 0.f, Max);
			const float Seg = FMath::Min(Spilled, Max - Hp);
			if (Seg <= 0.f || Inner <= 2.f)
			{
				return Layer;
			}
			const float X0 = 1.f + Inner * Hp / Max;
			const float W = FMath::Max(2.f, Inner * Seg / Max);
			const float Left = UWRBloodPath::WindowFraction(Body);
			const float R = FMath::Max(0.f, FMath::Min(3.f, Size.Y * 0.5f) - 1.f);
			const FLinearColor Garnet = WRBloodImpl::Garnet();
			WRUI::DrawBox(Out, Layer + 1, Geo.ToPaintGeometry(FVector2D(W, Size.Y - 2.f), FSlateLayoutTransform(FVector2D(X0, 1.f))), WRUI::Box(WRUI::Alpha(Garnet, 0.6f + 0.35f * Left), R));
			WRUI::DrawBox(Out, Layer + 2, Geo.ToPaintGeometry(FVector2D(W, (Size.Y - 2.f) * 0.45f), FSlateLayoutTransform(FVector2D(X0, 1.f))), WRUI::Box(FLinearColor(1.f, 1.f, 1.f, 0.10f), R));
			// The rally window: the segment's far edge, a lighter garnet that fades as the window runs out.
			const float Edge = FMath::Min(3.f, W);
			const FLinearColor Lit = FMath::Lerp(Garnet, FLinearColor(FColor(0xD0, 0x4A, 0x5A)), 0.85f);
			WRUI::DrawBox(Out, Layer + 3, Geo.ToPaintGeometry(FVector2D(Edge, Size.Y - 2.f), FSlateLayoutTransform(FVector2D(X0 + W - Edge, 1.f))), WRUI::Box(WRUI::Alpha(Lit, Left), 0.f));
			return Layer + 3;
		}

	private:
		TFunction<const AActor*()> Who;
		float PrevLeft = 0.f;
	};
}

TSharedRef<SWidget> WRBlood::MakeSpilledOverlay(TFunction<const AActor*()> Who)
{
	return SNew(WRBloodUI::SWRBloodSpilled, MoveTemp(Who)).Visibility(EVisibility::HitTestInvisible);
}

bool WRBlood::SlotShort(const AActor* Who, FName Path, FName Node, FString& OutWhy)
{
	if (Path != WRBloodImpl::PathId || !Who)
	{
		return false;
	}
	const float Cost = UWRBloodPath::HpCost(Node);
	if (UWRBloodPath::CanSpend(Who, Cost))
	{
		return false;
	}
	OutWhy = FString::Printf(TEXT("needs %d HP"), FMath::CeilToInt(Cost) + 1);
	return true;
}

float WRBlood::RefusedShake(const AActor* Who)
{
	const double* At = Who ? WRBloodUI::Refused().Find(Who) : nullptr;
	if (!At) return 0.f;
	const float Len = WRTune(TEXT("stamina.refuseFlashSeconds"), 0.35f);
	const float Age = float(FPlatformTime::Seconds() - *At);
	return Age >= 0.f && Age < Len ? 1.f - Age / Len : 0.f;
}

void WRBlood::NoteRefused(const AActor* Who)
{
	if (Who)
	{
		WRBloodUI::Refused().Add(Who, FPlatformTime::Seconds());
	}
}

// ================================================================================================ the body

namespace WRBloodLook
{
	struct FState
	{
		uint8 Serial = 0;
		double FlushAt = -100.0;
		bool bSeen = false;
	};
	static TMap<TWeakObjectPtr<const AActor>, FState>& States()
	{
		static TMap<TWeakObjectPtr<const AActor>, FState> Map;
		return Map;
	}

	/** Path strength (hero.md 2.6: smoothstep(0.08, 1, points / 100)) at a number of points. */
	static float StrengthAt(float Points)
	{
		const float Max = FMath::Max(1.f, WRTune(TEXT("paths.maxPointsPerPath"), 100.f));
		return FMath::SmoothStep(0.08f, 1.f, FMath::Clamp(Points / Max, 0.f, 1.f));
	}

	/** 0..1: how far into the 2.18 stage that starts at From (share of 100 points) the strength S is (5 points to come in). */
	static float Stage(float S, float From)
	{
		const float Max = FMath::Max(1.f, WRTune(TEXT("paths.maxPointsPerPath"), 100.f));
		const float Hi = StrengthAt(From * Max), Lo = StrengthAt(From * Max - 5.f);
		return Hi <= Lo ? (S >= Hi ? 1.f : 0.f) : FMath::Clamp((S - Lo) / (Hi - Lo), 0.f, 1.f);
	}

	/** The blood row's own skin numbers in Data/player_art.json (what the stages build on). */
	static float RowSkin(const TCHAR* Field, float Fallback)
	{
		static TSharedPtr<FJsonObject> Skin = []()
		{
			const TSharedPtr<FJsonObject> Art = FWRTuning::LoadDataFile(TEXT("player_art.json"));
			const TSharedPtr<FJsonObject>* Paths = nullptr;
			const TSharedPtr<FJsonObject>* Row = nullptr;
			const TSharedPtr<FJsonObject>* Out = nullptr;
			return Art.IsValid() && Art->TryGetObjectField(TEXT("paths"), Paths) && (*Paths)->TryGetObjectField(TEXT("blood"), Row) && (*Row)->TryGetObjectField(TEXT("skin"), Out) ? *Out : TSharedPtr<FJsonObject>();
		}();
		double V = Fallback;
		if (Skin.IsValid()) Skin->TryGetNumberField(Field, V);
		return float(V);
	}

	static FLinearColor LookColor(const TCHAR* Field, const FLinearColor& Fallback)
	{
		const TSharedPtr<FJsonObject> Row = FWRTuning::Get().Object(TEXT("blood.look"));
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (!Row.IsValid() || !Row->TryGetArrayField(Field, Arr) || Arr->Num() < 3) return Fallback;
		return FLinearColor(float((*Arr)[0]->AsNumber()), float((*Arr)[1]->AsNumber()), float((*Arr)[2]->AsNumber()), 1.f);
	}

	/** A material's own value of a parameter the rows never name (read once, before anything here writes it). */
	static float OwnScalar(UMaterialInstanceDynamic* MID, FName Param, bool& bHas)
	{
		static TMap<TPair<TWeakObjectPtr<UMaterialInstanceDynamic>, FName>, float> Cache;
		const TPair<TWeakObjectPtr<UMaterialInstanceDynamic>, FName> Key(MID, Param);
		if (const float* V = Cache.Find(Key)) { bHas = true; return *V; }
		float V = 0.f;
		bHas = MID->GetScalarParameterValue(FMaterialParameterInfo(Param), V);
		if (bHas) Cache.Add(Key, V);
		return V;
	}

	struct FLive
	{
		float SpilledShare = 0.f;  // Spilled as a share of a quarter of max health (veins at their darkest from there)
		float Flush = 0.f;         // 0..1 left of the lifesteal flush
		bool bLord = false;        // Sanguine lord
		bool bLowFight = false;    // below a quarter of health and fighting
		bool bFight = false;       // fighting (the throat and lips flush)
	};

	/** The blood row's own "Channels" (which path-mask channels its tracery reads: R veins by default). */
	static FLinearColor RowChannels()
	{
		static const FLinearColor Ch = []()
		{
			const TSharedPtr<FJsonObject> Art = FWRTuning::LoadDataFile(TEXT("player_art.json"));
			const TSharedPtr<FJsonObject>* Paths = nullptr;
			const TSharedPtr<FJsonObject>* Row = nullptr;
			const TSharedPtr<FJsonObject>* Skin = nullptr;
			const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
			if (Art.IsValid() && Art->TryGetObjectField(TEXT("paths"), Paths) && (*Paths)->TryGetObjectField(TEXT("blood"), Row) && (*Row)->TryGetObjectField(TEXT("skin"), Skin)
				&& (*Skin)->TryGetArrayField(TEXT("Channels"), Arr) && Arr->Num() >= 4)
			{
				return FLinearColor(float((*Arr)[0]->AsNumber()), float((*Arr)[1]->AsNumber()), float((*Arr)[2]->AsNumber()), float((*Arr)[3]->AsNumber()));
			}
			return FLinearColor(1.f, 0.f, 0.f, 0.f);
		}();
		return Ch;
	}

	/** Another path row's skin number (the mixed looks tone the other side down under the garnet). */
	static float OtherSkin(FName Path, const TCHAR* Field, float Fallback)
	{
		const TSharedPtr<FJsonObject> Art = FWRTuning::LoadDataFile(TEXT("player_art.json"));
		const TSharedPtr<FJsonObject>* Paths = nullptr;
		const TSharedPtr<FJsonObject>* Row = nullptr;
		const TSharedPtr<FJsonObject>* Skin = nullptr;
		double V = Fallback;
		if (Art.IsValid() && Art->TryGetObjectField(TEXT("paths"), Paths) && (*Paths)->TryGetObjectField(Path.ToString(), Row) && (*Row)->TryGetObjectField(TEXT("skin"), Skin))
		{
			(*Skin)->TryGetNumberField(Field, V);
		}
		return float(V);
	}

	static FLinearColor OtherChannels(FName Path)
	{
		const TSharedPtr<FJsonObject> Art = FWRTuning::LoadDataFile(TEXT("player_art.json"));
		const TSharedPtr<FJsonObject>* Paths = nullptr;
		const TSharedPtr<FJsonObject>* Row = nullptr;
		const TSharedPtr<FJsonObject>* Skin = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (Art.IsValid() && Art->TryGetObjectField(TEXT("paths"), Paths) && (*Paths)->TryGetObjectField(Path.ToString(), Row) && (*Row)->TryGetObjectField(TEXT("skin"), Skin)
			&& (*Skin)->TryGetArrayField(TEXT("Channels"), Arr) && Arr->Num() >= 4)
		{
			return FLinearColor(float((*Arr)[0]->AsNumber()), float((*Arr)[1]->AsNumber()), float((*Arr)[2]->AsNumber()), float((*Arr)[3]->AsNumber()));
		}
		return FLinearColor(0.f, 0.f, 0.f, 0.f);
	}

	static FLive LiveOf(const AActor* Hero)
	{
		FLive L;
		const UWRPathComponent* P = WRBloodImpl::PathsOf(Hero);
		const AWRCharacterBase* C = Cast<AWRCharacterBase>(Hero);
		const UWRAttributeSet* A = C ? C->GetAttributes() : nullptr;
		if (!P || !A)
		{
			return L;
		}
		L.SpilledShare = FMath::Clamp(UWRBloodPath::Spilled(Hero) / FMath::Max(1.f, A->GetMaxHealth() * 0.25f), 0.f, 1.f);
		L.bLord = C->HasTag(WRBlood::Status_SanguineLord);
		L.bFight = WRBloodImpl::ServerNow(Hero) < P->BloodFightUntil;
		L.bLowFight = WRBloodImpl::HealthFraction(Hero) < WRBloodImpl::Look(TEXT("lowHealth"), 0.25f) && L.bFight;
		FState& S = States().FindOrAdd(Hero);
		const double Now = Hero->GetWorld() ? Hero->GetWorld()->GetTimeSeconds() : 0.0;
		if (!S.bSeen)
		{
			S.bSeen = true;
			S.Serial = P->BloodStealSerial;
		}
		else if (S.Serial != P->BloodStealSerial)
		{
			S.Serial = P->BloodStealSerial;
			S.FlushAt = Now;
		}
		L.Flush = FMath::Clamp(1.f - float(Now - S.FlushAt) / FMath::Max(0.05f, WRBloodImpl::Look(TEXT("flushSeconds"), 0.3f)), 0.f, 1.f);
		return L;
	}
}

uint32 WRBlood::LookSig(const AActor* Hero)
{
	const UWRPathComponent* P = Hero ? WRBloodImpl::PathsOf(Hero) : nullptr;
	if (!P || !P->IsActivePath(WRBloodImpl::PathId))
	{
		return 0u;
	}
	const WRBloodLook::FLive L = WRBloodLook::LiveOf(Hero);
	uint32 Sig = uint32(FMath::RoundToInt(L.SpilledShare * 8.f));
	Sig = HashCombine(Sig, uint32(FMath::RoundToInt(L.Flush * 10.f)));
	Sig = HashCombine(Sig, (L.bLord ? 1u : 0u) | (L.bLowFight ? 2u : 0u) | (L.bFight ? 4u : 0u));
	return Sig;
}

void WRBlood::ApplyLook(const AActor* Hero, const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Skin, const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Eyes,
	const TMap<FName, FLinearColor>& EyeDefaultColors, FName Main, float MainS, FName Sub, float SubS)
{
	using namespace WRBloodLook;
	const FName Blood = WRBloodImpl::PathId;
	if (!Hero || (Main != Blood && Sub != Blood))
	{
		// Not (or no longer) on the path: a narrowed pupil goes back to the material's own.
		for (UMaterialInstanceDynamic* MID : Eyes)
		{
			bool bHas = false;
			const float Own = MID ? OwnScalar(MID, TEXT("Pupil Scale"), bHas) : 0.f;
			if (bHas) MID->SetScalarParameterValue(TEXT("Pupil Scale"), Own);
		}
		return;
	}
	const FLive L = LiveOf(Hero);
	const FWRTuning& T = FWRTuning::Get();
	const auto LookNum = [&T](const TCHAR* Field, float Fallback) { return T.Num(FString(TEXT("blood.look.")) + Field, Fallback); };

	// Skin (MF_PathMarks, Main* / Sub*): the row drew the tracery and its tint by strength; the stages and the live parts
	// go on top. The hero's path mask is graded per channel (Tools: gen_masks.py): R veins hands 1.0 > forearms .85 >
	// upper arms .65 > neck .5 > chest .35, head neck .5 / temples .3; A extremities fingertips 1.0, head lips .75, eyelids
	// .45, ear tips .45. A texel shows once its value >= 1 - Strength, so the vein reach (wrists, then the neck, the chest
	// up to the collarbones, the temples) is the strength's own reveal: the material has no separate per-region masks to
	// step it by stage. What it does allow: the lips through the A channel at 10 (weighted so the eyelids and ear tips stay
	// clear), the flush spreading from the hands outward along R's grading, and the mixed looks on the other side's row.
	const FName OtherPath = Main == Blood ? Sub : Main;
	for (const TPair<FString, float> Side : { TPair<FString, float>(TEXT("Main"), Main == Blood ? MainS : 0.f), TPair<FString, float>(TEXT("Sub"), Sub == Blood ? SubS : 0.f) })
	{
		const float S = Side.Value;
		if (S <= 0.f)
		{
			continue;
		}
		const FString& Prefix = Side.Key;
		const FString OtherPrefix = Prefix == TEXT("Main") ? FString(TEXT("Sub")) : FString(TEXT("Main"));
		const float Lips = Stage(S, LookNum(TEXT("lipsFrom"), 0.1f));
		const float Veins = Stage(S, LookNum(TEXT("veinsFrom"), 0.3f));
		const float Porcelain = Stage(S, LookNum(TEXT("porcelainFrom"), 0.75f));
		const float Full = Stage(S, LookNum(TEXT("irisFrom"), 1.f));
		// The lifesteal flush runs out from the hand that struck: for its 0.3 s the reveal sweeps up the mask's grading
		// (hands, forearms, upper arms, neck, chest, temples) and back, a warm wave across the skin.
		const float FlushPhase = L.Flush > 0.f ? 1.f - L.Flush : 0.f;
		const float Sweep = L.Flush > 0.f ? FMath::Sin(PI * FlushPhase) * LookNum(TEXT("flushReach"), 0.85f) : 0.f;
		// From 10 the reveal can't be nothing (the material hides everything at 0), however little the strength.
		const float Reveal = FMath::Clamp(FMath::Max(S, LookNum(TEXT("lipsReveal"), 0.02f) * Lips) + (1.f - S) * Sweep, 0.f, 1.f);
		// The lips (A channel .75 on the head) by the berry-garnet stage, weighted so they're fully in at this reveal while
		// the eyelids and ear tips (.45) stay under it; in a fight a little more (the flush at the lips and throat).
		const float FightLips = L.bFight ? LookNum(TEXT("fightLips"), 0.12f) : 0.f;
		const float LipW = Lips * FMath::Clamp((1.1f - Reveal) / 0.75f + FightLips, LookNum(TEXT("lipsMinWeight"), 0.15f), LookNum(TEXT("lipsMaxWeight"), 1.5f));
		const FLinearColor RowCh = RowChannels();
		const FLinearColor BloodCh(RowCh.R, RowCh.G, RowCh.B, FMath::Max(RowCh.A, LipW));
		// The veins darken while Spilled is high; under Frost they give a little, so its fern frosts over them.
		float Albedo = RowSkin(TEXT("AlbedoMix"), 0.65f) * (1.f + LookNum(TEXT("spilledDarken"), 0.6f) * L.SpilledShare * Veins);
		if (OtherPath == TEXT("frost")) Albedo *= LookNum(TEXT("mixFrostBloodAlbedo"), 0.75f);
		// Sanguine lord lights the veins garnet for its 10 s; every lifesteal heal sends a 0.3 s warm flush through them;
		// a faint flush in a fight from 10 (lips and throat); with Fire the garnet glows as the cores of the ember lines.
		float Nits = FMath::Max(RowSkin(TEXT("EmissiveNits"), 0.f), L.bLord ? LookNum(TEXT("sanguineNits"), 1.6f) : 0.f) + LookNum(TEXT("flushNits"), 0.9f) * L.Flush;
		if (L.bFight) Nits += LookNum(TEXT("fightFlushNits"), 0.2f) * Lips;
		if (OtherPath == TEXT("fire")) Nits = FMath::Max(Nits, LookNum(TEXT("mixFireCoreNits"), 0.7f) * Veins);
		// The heartbeat: Sanguine lord's calm 70 bpm, and at 100 in a fight below a quarter of health 110 bpm.
		float Hz = RowSkin(TEXT("PulseHz"), 0.f), Amp = RowSkin(TEXT("PulseAmp"), 0.f);
		if (L.bLowFight && Full > 0.f && Prefix == TEXT("Main"))
		{
			Hz = LookNum(TEXT("heartbeatFight"), 110.f) / 60.f;
			Amp = LookNum(TEXT("pulseAmp"), 0.45f) * Full;
		}
		else if (L.bLord)
		{
			Hz = LookNum(TEXT("heartbeatCalm"), 70.f) / 60.f;
			Amp = LookNum(TEXT("pulseAmp"), 0.45f);
		}
		// At 75: a cool porcelain clarity (a touch less colour, a touch more light) so the garnet reads striking.
		// The material scales these by Strength, which is now the reveal: divided back so the skin's tone follows S.
		const float ToneScale = Reveal > KINDA_SMALL_NUMBER ? S / Reveal : 0.f;
		const float Desat = (RowSkin(TEXT("SkinDesat"), 0.f) + LookNum(TEXT("porcelainDesat"), 0.12f) * Porcelain) * ToneScale;
		const float Light = (RowSkin(TEXT("SkinLight"), 0.f) + LookNum(TEXT("porcelainLight"), 0.06f) * Porcelain) * ToneScale;
		// Mixed looks (cook 2.18): Blood with Fire, the ember lines run over garnet cores (the fire row's own albedo gives
		// way to the garnet under its glow); Blood with Frost, the frost-fern reaches the vein lines too and frosts over them.
		const float OtherMix = OtherPath == TEXT("fire") ? OtherSkin(OtherPath, TEXT("AlbedoMix"), 0.55f) * LookNum(TEXT("mixFireEmberAlbedo"), 0.5f) : -1.f;
		FLinearColor OtherCh = FLinearColor::Transparent;
		const bool bFrostOver = OtherPath == TEXT("frost");
		if (bFrostOver)
		{
			OtherCh = OtherChannels(OtherPath);
			OtherCh.R = FMath::Max(OtherCh.R, LookNum(TEXT("mixFrostVeins"), 0.8f) * Veins);
		}
		for (UMaterialInstanceDynamic* MID : Skin)
		{
			if (!MID) continue;
			MID->SetScalarParameterValue(FName(*(Prefix + TEXT("Strength"))), Reveal);
			MID->SetVectorParameterValue(FName(*(Prefix + TEXT("Channels"))), BloodCh);
			MID->SetScalarParameterValue(FName(*(Prefix + TEXT("AlbedoMix"))), Albedo);
			MID->SetScalarParameterValue(FName(*(Prefix + TEXT("EmissiveNits"))), Nits);
			MID->SetScalarParameterValue(FName(*(Prefix + TEXT("PulseHz"))), Hz);
			MID->SetScalarParameterValue(FName(*(Prefix + TEXT("PulseAmp"))), Amp);
			MID->SetScalarParameterValue(FName(*(Prefix + TEXT("SkinDesat"))), Desat);
			MID->SetScalarParameterValue(FName(*(Prefix + TEXT("SkinLight"))), Light);
			if (OtherMix >= 0.f) MID->SetScalarParameterValue(FName(*(OtherPrefix + TEXT("AlbedoMix"))), OtherMix);
			if (bFrostOver) MID->SetVectorParameterValue(FName(*(OtherPrefix + TEXT("Channels"))), OtherCh);
		}
	}

	// Eyes (main path only): a thin garnet ring at the iris edge from 50, the whole iris garnet at 100; at 100 in a fight
	// below a quarter of health the pupils narrow.
	const float S = Main == Blood ? MainS : 0.f;
	const float Ring = Stage(S, LookNum(TEXT("ringFrom"), 0.5f));
	const float Iris = Stage(S, LookNum(TEXT("irisFrom"), 1.f));
	const FLinearColor RingColor = LookColor(TEXT("ring"), FLinearColor(0.48f, 0.04f, 0.08f));
	const FLinearColor IrisColor = LookColor(TEXT("iris"), FLinearColor(0.62f, 0.06f, 0.1f));
	const FLinearColor* RingOwn = EyeDefaultColors.Find(TEXT("eye:Melanosis Ring Color"));
	const FLinearColor* IrisOwn = EyeDefaultColors.Find(TEXT("eye:Iris Color Multiply"));
	for (UMaterialInstanceDynamic* MID : Eyes)
	{
		if (!MID) continue;
		if (Ring > 0.f && RingOwn) MID->SetVectorParameterValue(TEXT("Melanosis Ring Color"), FMath::Lerp(*RingOwn, RingColor, Ring));
		if (Iris > 0.f && IrisOwn) MID->SetVectorParameterValue(TEXT("Iris Color Multiply"), FMath::Lerp(*IrisOwn, IrisColor, Iris));
		bool bHas = false;
		const float Own = OwnScalar(MID, TEXT("Pupil Scale"), bHas);
		if (bHas)
		{
			MID->SetScalarParameterValue(TEXT("Pupil Scale"), L.bLowFight && Iris > 0.f ? Own * FMath::Lerp(1.f, LookNum(TEXT("pupilNarrow"), 0.75f), Iris) : Own);
		}
	}
}
