#include "WRTidePath.h"
#include "Wildreach.h"
#include "WRArcanePath.h"
#include "WRAttributeSet.h"
#include "WRCharacterBase.h"
#include "WRCharacterMovementComponent.h"
#include "WRCombatLibrary.h"
#include "WRCombatTypes.h"
#include "WRCombos.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRNecroPath.h"
#include "WRPathComponent.h"
#include "WRPlantSummon.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRTuning.h"
#include "WRUnderwater.h"
#include "WRWindPath.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace WRTidePath
{
	UE_DEFINE_GAMEPLAY_TAG(Passive_Gills, "Passive.Gills");
	UE_DEFINE_GAMEPLAY_TAG(Status_Current, "Status.Tide.Current");
	UE_DEFINE_GAMEPLAY_TAG(Status_Current2, "Status.Tide.Current2");
	UE_DEFINE_GAMEPLAY_TAG(Status_Current3, "Status.Tide.Current3");
	UE_DEFINE_GAMEPLAY_TAG(Status_Anchored, "Status.Tide.Anchored");
	UE_DEFINE_GAMEPLAY_TAG(Combo_HotSpring, "Combo.HotSpring");
	UE_DEFINE_GAMEPLAY_TAG(Combo_BoilingSea, "Combo.BoilingSea");
	UE_DEFINE_GAMEPLAY_TAG(Combo_LiveWater, "Combo.LiveWater");
	UE_DEFINE_GAMEPLAY_TAG(Combo_ThunderTide, "Combo.ThunderTide");
	UE_DEFINE_GAMEPLAY_TAG(Combo_Rain, "Combo.Rain");
	UE_DEFINE_GAMEPLAY_TAG(Combo_SwampGuardian, "Combo.SwampGuardian");
	UE_DEFINE_GAMEPLAY_TAG(Combo_FlashFreeze, "Combo.FlashFreeze");
	UE_DEFINE_GAMEPLAY_TAG(Combo_FrozenMaelstrom, "Combo.FrozenMaelstrom");
	UE_DEFINE_GAMEPLAY_TAG(Combo_LifeTide, "Combo.LifeTide");
	UE_DEFINE_GAMEPLAY_TAG(Combo_SharkHarpoon, "Combo.SharkHarpoon");
	UE_DEFINE_GAMEPLAY_TAG(Combo_TidalDead, "Combo.TidalDead");
	UE_DEFINE_GAMEPLAY_TAG(Combo_Depths, "Combo.Depths");
}

namespace WRTidePathImpl
{
	static const FName TpPathId(TEXT("tide"));
	float TpT(const TCHAR* Key, float Fallback) { return FWRTuning::Get().Num(FString(TEXT("tide.path.")) + Key, Fallback); }
	float TpF(const TCHAR* Key, float Fallback) { return FWRTuning::Get().Num(FString(TEXT("tide.flow.")) + Key, Fallback); }
	float TpCombo(const TCHAR* Id, const TCHAR* Key, float Fallback) { return FWRComboRegistry::Get().Num(FName(Id), FName(Key), Fallback); }
	bool TpTagged(const AActor* Who, const FGameplayTag& Tag)
	{
		const AWRCharacterBase* C = Cast<AWRCharacterBase>(Who);
		return C && C->HasTag(Tag);
	}
	void TpSetAttr(AWRCharacterBase* Who, const FGameplayAttribute& Attr, float V)
	{
		if (UAbilitySystemComponent* ASC = Who ? Who->GetAbilitySystemComponent() : nullptr) ASC->SetNumericAttributeBase(Attr, V);
	}
	double TpNow(const UObject* Ctx)
	{
		const UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
		const AGameStateBase* GS = W ? W->GetGameState() : nullptr;
		return GS ? GS->GetServerWorldTimeSeconds() : (W ? W->GetTimeSeconds() : 0.0);
	}
	TMap<TWeakObjectPtr<const AActor>, double>& TpRefusals() { static TMap<TWeakObjectPtr<const AActor>, double> M; return M; }
	TMap<TWeakObjectPtr<const AActor>, double>& TpFilling() { static TMap<TWeakObjectPtr<const AActor>, double> M; return M; }
	/** Server: per survivor, how long they've been still and how long the drain waits. */
	struct FTpOwner { float Still = 0.f; double HoldUntil = 0.0; float Carry = 0.f; };
	TMap<TWeakObjectPtr<AWRCharacterBase>, FTpOwner>& TpOwners() { static TMap<TWeakObjectPtr<AWRCharacterBase>, FTpOwner> M; return M; }
	/** Server: who soaked whom last (Drowning and Heavy water are the soaker's), and the tidal shields' owners. */
	TMap<TWeakObjectPtr<const AActor>, TWeakObjectPtr<AWRCharacterBase>>& TpSoakedBy() { static TMap<TWeakObjectPtr<const AActor>, TWeakObjectPtr<AWRCharacterBase>> M; return M; }
	struct FTpShield { TWeakObjectPtr<AWRCharacterBase> Caster; double Until = 0.0; };
	TMap<TWeakObjectPtr<AWRCharacterBase>, FTpShield>& TpShields() { static TMap<TWeakObjectPtr<AWRCharacterBase>, FTpShield> M; return M; }
	/** Server: Flash freeze's per-target wait. */
	TMap<TWeakObjectPtr<const AActor>, double>& TpFroze() { static TMap<TWeakObjectPtr<const AActor>, double> M; return M; }
	float TpStrength(const AActor* Who)
	{
		const UWRPathComponent* P = UWRPathComponent::FindFor(Who);
		if (!P || !P->IsActivePath(TpPathId)) return 0.f;
		const float Max = FMath::Max(1.f, TpT(TEXT("lookMaxPoints"), 60.f));
		const float S = FMath::SmoothStep(0.08f, 1.f, FMath::Clamp(float(P->GetSpent(TpPathId)) / Max, 0.f, 1.f));
		return P->GetMainPath() == TpPathId ? S : S * WRTune(TEXT("pathMarks.subScale"), 0.6f);
	}
	bool TpSoaked(const AActor* Who) { return TpTagged(Who, WRTags::Status_Soak); }
}
using namespace WRTidePathImpl;

// ================================================================================================ the path

bool WRTidePath::Walks(const AActor* Who)
{
	const UWRPathComponent* P = UWRPathComponent::FindFor(Who);
	return P && P->IsActivePath(TpPathId);
}

int32 WRTidePath::Rank(const AActor* Who, FName Node)
{
	const UWRPathComponent* P = UWRPathComponent::FindFor(Who);
	const FWRPathProgress* G = P && P->IsActivePath(TpPathId) ? P->GetProgress(TpPathId) : nullptr;
	return G ? G->GetRank(Node) : 0;
}

int32 WRTidePath::PointsIn(const AActor* Who)
{
	const UWRPathComponent* P = UWRPathComponent::FindFor(Who);
	return P && P->IsActivePath(TpPathId) ? P->GetSpent(TpPathId) : 0;
}

float WRTidePath::Num(const TCHAR* Key, float Fallback) { return TpT(Key, Fallback); }

// ================================================================================================ Flow

float WRTidePath::FlowOf(const AActor* Who)
{
	const AWRCharacterBase* C = Cast<AWRCharacterBase>(Who);
	const UWRAttributeSet* A = C ? C->GetAttributes() : nullptr;
	return A ? A->GetFlow() : 0.f;
}

float WRTidePath::MaxFlowOf(const AActor* Who)
{
	const AWRCharacterBase* C = Cast<AWRCharacterBase>(Who);
	const UWRAttributeSet* A = C ? C->GetAttributes() : nullptr;
	return A ? A->GetMaxFlow() : 100.f;
}

bool WRTidePath::HasFlow(const AActor* Who, float Amount)
{
	return Amount <= 0.f || FlowOf(Who) + 0.01f >= Amount;
}

bool WRTidePath::SpendFlow(AWRCharacterBase* Who, float Amount)
{
	if (!Who || !Who->HasAuthority()) return false;
	if (Amount <= 0.f) return true;
	if (!HasFlow(Who, Amount)) return false;
	TpSetAttr(Who, UWRAttributeSet::GetFlowAttribute(), FlowOf(Who) - Amount);
	return true;
}

void WRTidePath::GainFlow(AWRCharacterBase* Who, float Amount)
{
	const UWRAttributeSet* A = Who ? Who->GetAttributes() : nullptr;
	if (!A || !Who->HasAuthority() || Amount <= 0.f || !Walks(Who)) return;
	TpSetAttr(Who, UWRAttributeSet::GetFlowAttribute(), FMath::Min(A->GetMaxFlow(), A->GetFlow() + Amount));
	TpFilling().Add(Who, TpNow(Who));
}

void WRTidePath::NoteRefused(const AActor* Who)
{
	if (Who) TpRefusals().Add(Who, TpNow(Who));
}

float WRTidePath::RefusedFlash(const AActor* Who)
{
	const double* At = Who ? TpRefusals().Find(Who) : nullptr;
	if (!At) return 0.f;
	const float Len = TpF(TEXT("refuseFlash"), 0.35f);
	const float Age = float(TpNow(Who) - *At);
	return Age >= 0.f && Age < Len ? 1.f - Age / Len : 0.f;
}

float WRTidePath::FillingGlow(const AActor* Who)
{
	// The owner's bar: rising Flow makes it ripple. On the owner the attribute climbs by replication: watch it here.
	static TMap<TWeakObjectPtr<const AActor>, float> Last;
	static TMap<TWeakObjectPtr<const AActor>, double> RoseAt;
	const float Now = FlowOf(Who);
	float& Prev = Last.FindOrAdd(Who);
	if (Now > Prev + 0.05f) RoseAt.Add(Who, TpNow(Who));
	Prev = Now;
	const double* At = RoseAt.Find(Who);
	return At ? FMath::Clamp(1.f - float(TpNow(Who) - *At) / 0.8f, 0.f, 1.f) : 0.f;
}

void WRTidePath::HoldDrain(AWRCharacterBase* Who, float Seconds)
{
	if (!Who) return;
	FTpOwner& O = TpOwners().FindOrAdd(Who);
	O.HoldUntil = FMath::Max(O.HoldUntil, TpNow(Who) + Seconds);
}

void WRTidePath::OnDodge(AWRCharacterBase* Who, bool bPerfect)
{
	if (!Who || !Who->HasAuthority() || !Walks(Who)) return;
	const bool bWater = Who->GetCharacterMovement() && Who->GetCharacterMovement()->IsSwimming();
	const float Mult = bWater && Who->HasTag(Passive_Gills) ? TpF(TEXT("gillsWaterMult"), 1.5f) : 1.f;
	GainFlow(Who, (bPerfect ? TpF(TEXT("perfectDodgeGain"), 15.f) : TpF(TEXT("dodgeGain"), 10.f)) * Mult);
	if (bPerfect) Proc(Who, TEXT("tide.flow_ribbon"), TEXT("tide.flow_rush"));
}

void WRTidePath::Proc(AWRCharacterBase* Who, FName Fx, FName Sound)
{
	if (!Who || !Who->HasAuthority()) return;
	WRFx::BroadcastOn(Who, Fx, TEXT("spine"), 1.f, Sound, 0.6f);
}

void WRTidePath::TickOwner(AWRCharacterBase* Who, float DeltaTime)
{
	if (!Who || !Who->HasAuthority() || !Who->GetPlayerState()) return;
	const UWRAttributeSet* A = Who->GetAttributes();
	if (!A) return;
	// Tidal shields' bursts (any caster's: the shield's owner list is server-wide, checked on the carrier's tick).
	if (FTpShield* Sh = TpShields().Find(Who))
	{
		const double Now = TpNow(Who);
		if (WRArcane::ShieldOf(Who) <= 0.f && Now < Sh->Until)
		{
			// It broke: a water burst soaks every attacker within 2 m.
			const float R = WRTune(TEXT("spells.tidalShield.burstRadius"), 200.f);
			WRFx::Broadcast(Who, TEXT("tide.shield_burst"), Who->GetActorLocation(), FRotator::ZeroRotator, R / 100.f, TEXT("tide.splash"));
			for (TActorIterator<AWRCharacterBase> It(Who->GetWorld()); It; ++It)
			{
				if (!It->IsAlive() || !Who->IsHostileTo(*It) || FVector::DistSquared(It->GetActorLocation(), Who->GetActorLocation()) > FMath::Square(R + It->GetSimpleCollisionRadius())) continue;
				FWRHit Hit;
				Hit.Instigator = Sh->Caster.Get();
				Hit.Element = WRTags::Element_Water;
				Hit.PathId = TpPathId;
				Hit.bSpell = true;
				Hit.Buildups.Add(TEXT("soak"), WRTune(TEXT("spells.tidalShield.burstSoak"), 30.f));
				Hit.Origin = Who->GetActorLocation();
				UWRCombatLibrary::ApplyHit(*It, Hit);
			}
			TpShields().Remove(Who);
		}
		else if (Now >= Sh->Until) TpShields().Remove(Who);
	}
	// Drowning (the soaker's node): a soaked survivor loses 6 stamina a second.
	if (TpSoaked(Who))
	{
		const TWeakObjectPtr<AWRCharacterBase>* By = TpSoakedBy().Find(Who);
		if (By && By->IsValid() && Rank(By->Get(), TEXT("drowning")) > 0 && A->GetStamina() > 0.f)
		{
			TpSetAttr(Who, UWRAttributeSet::GetStaminaAttribute(), FMath::Max(0.f, A->GetStamina() - TpT(TEXT("drowningStamina"), 6.f) * DeltaTime));
		}
	}
	if (!Walks(Who))
	{
		if (A->GetFlow() > 0.f) TpSetAttr(Who, UWRAttributeSet::GetFlowAttribute(), 0.f);
		return;
	}
	// Capacity 100, +15 a Flow capacity rank (nodeEffects tide.flow_capacity maxFlow).
	const float Max = TpF(TEXT("max"), 100.f) + FMath::Max(0.f, UWRPathComponent::Stat(Who, TEXT("maxFlow")));
	if (!FMath::IsNearlyEqual(A->GetMaxFlow(), Max))
	{
		TpSetAttr(Who, UWRAttributeSet::GetMaxFlowAttribute(), Max);
		if (A->GetFlow() > Max) TpSetAttr(Who, UWRAttributeSet::GetFlowAttribute(), Max);
	}
	if (!Who->IsAlive()) return;
	FTpOwner& O = TpOwners().FindOrAdd(Who);
	const UCharacterMovementComponent* Move = Who->GetCharacterMovement();
	const float Speed = Who->GetVelocity().Size();
	const bool bSwim = Move && Move->IsSwimming();
	const AWRCreature* Mount = Cast<AWRCreature>(Who->GetAttachParentActor());
	const bool bSwimMount = Mount && Mount->GetCharacterMovement() && Mount->GetCharacterMovement()->IsSwimming();
	const bool bFast = (Move && Move->IsMovingOnGround() && Cast<UWRCharacterMovementComponent>(Move) && Cast<UWRCharacterMovementComponent>(Move)->IsSprinting()) || bSwim || bSwimMount;
	const float Moving = bSwimMount ? Mount->GetVelocity().Size() : Speed;
	float Gain = 0.f;
	if (bFast && Moving > TpF(TEXT("stillSpeed"), 150.f)) Gain = TpF(TEXT("sprintGain"), 8.f);
	else if (Moving > TpF(TEXT("moveThreshold"), 300.f)) Gain = TpF(TEXT("moveGain"), 5.f);
	if ((bSwim || bSwimMount) && Who->HasTag(Passive_Gills)) Gain *= TpF(TEXT("gillsWaterMult"), 1.5f);
	if (Gain > 0.f) GainFlow(Who, Gain * DeltaTime);
	// Standing still empties you: after 1.5 s under 150 cm/s, -8 a second (a rooted cast holds it).
	O.Still = Moving < TpF(TEXT("stillSpeed"), 150.f) ? O.Still + DeltaTime : 0.f;
	if (O.Still >= TpF(TEXT("stillAfter"), 1.5f) && TpNow(Who) >= O.HoldUntil && A->GetFlow() > 0.f)
	{
		TpSetAttr(Who, UWRAttributeSet::GetFlowAttribute(), FMath::Max(0.f, A->GetFlow() - TpF(TEXT("drain"), 8.f) * DeltaTime));
	}
	// Swamp guardian (Nature + Tide 50): your Ancient guardian draws enemies in like a whirlpool.
	if (Who->HasTag(Combo_SwampGuardian))
	{
		O.Carry += DeltaTime;
		if (O.Carry >= 0.25f)
		{
			O.Carry = 0.f;
			const float R = TpCombo(TEXT("swamp_guardian"), TEXT("radius"), 900.f);
			for (AWRPlantSummon* P : AWRPlantSummon::PlantsOf(Who))
			{
				if (!P || !P->IsA(AWRAncientGuardian::StaticClass())) continue;
				for (TActorIterator<AWRCharacterBase> It(Who->GetWorld()); It; ++It)
				{
					if (!It->IsAlive() || !Who->IsHostileTo(*It) || It->IsA<AWRPlantSummon>()) continue;
					const FVector To = P->GetActorLocation() - It->GetActorLocation();
					if (To.SizeSquared2D() > R * R || To.SizeSquared2D() < 150.f * 150.f) continue;
					WRWind::Knock(Who, *It, To.GetSafeNormal2D(), TpCombo(TEXT("swamp_guardian"), TEXT("pull"), 120.f), 0.25f, TEXT("swamp_guardian"));
				}
			}
		}
	}
}

// ================================================================================================ combat

void WRTidePath::ScaleBuildups(const AWRCharacterBase* Attacker, FWRHit& Hit)
{
	if (!Attacker || !Walks(Attacker)) return;
	const int32 Soak = Rank(Attacker, TEXT("soak_chance"));
	if (Soak <= 0) return;
	// Soak chance: +10% to the soak on every hit, and +6 soak on every weapon hit a rank (so any weapon soaks).
	if (float* S = Hit.Buildups.Find(TEXT("soak"))) *S *= 1.f + UWRPathComponent::Stat(Attacker, TEXT("soakBuildup"));
	if (Hit.bMelee && !Hit.bDamageOverTime) Hit.Buildups.FindOrAdd(TEXT("soak")) += UWRPathComponent::Stat(Attacker, TEXT("weaponSoak"));
}

void WRTidePath::AfterHit(AWRCharacterBase* Target, const FWRHit& Hit, float Damage)
{
	AWRCharacterBase* Attacker = Cast<AWRCharacterBase>(Hit.Instigator);
	if (!Target || !Attacker) return;
	// Who soaked whom (Drowning, Heavy water are the soaker's nodes).
	if (Hit.Buildups.Contains(TEXT("soak")) && Walks(Attacker)) TpSoakedBy().Add(Target, Attacker);
	// Flow for a weapon hit landed while moving (+4).
	if (Hit.bMelee && Walks(Attacker) && Attacker->GetVelocity().Size() > TpF(TEXT("hitMoveSpeed"), 150.f)) GainFlow(Attacker, TpF(TEXT("hitGain"), 4.f));
	// Live water (Lightning + Tide 25): lightning on a soaked enemy chains to up to two more soaked enemies within 6 m.
	if (Hit.Element == WRTags::Element_Lightning && !Hit.bReaction && Attacker->HasTag(Combo_LiveWater) && TpSoaked(Target))
	{
		int32 Left = int32(TpCombo(TEXT("live_water"), TEXT("chains"), 2.f));
		const float R = TpCombo(TEXT("live_water"), TEXT("radius"), 600.f);
		for (TActorIterator<AWRCharacterBase> It(Target->GetWorld()); It && Left > 0; ++It)
		{
			if (*It == Target || !It->IsAlive() || !Attacker->IsHostileTo(*It) || !TpSoaked(*It) || FVector::DistSquared(It->GetActorLocation(), Target->GetActorLocation()) > R * R) continue;
			FWRHit Chain = Hit;
			Chain.Damage = Damage * TpCombo(TEXT("live_water"), TEXT("share"), 0.5f);
			Chain.bReaction = true;
			Chain.Origin = Target->GetActorLocation();
			WRFx::Broadcast(Target, TEXT("tide.live_water"), Target->GetActorLocation(), (It->GetActorLocation() - Target->GetActorLocation()).Rotation(), 1.f, TEXT("lightning.arc"));
			UWRCombatLibrary::ApplyHit(*It, Chain);
			--Left;
		}
	}
	// Flash freeze (Frost + Tide 25): frost on a soaked enemy freezes it at once (once every 10 s for that body).
	if (Hit.Element == WRTags::Element_Frost && Attacker->HasTag(Combo_FlashFreeze) && TpSoaked(Target) && Target->IsAlive() && Target->Status)
	{
		double& Last = TpFroze().FindOrAdd(Target);
		if (TpNow(Target) - Last > TpCombo(TEXT("flash_freeze"), TEXT("cooldown"), 10.f))
		{
			Last = TpNow(Target);
			Target->Status->ApplyStatus(WRTags::Status_Frozen, TpCombo(TEXT("flash_freeze"), TEXT("seconds"), 2.f));
			WRFx::Broadcast(Target, TEXT("tide.flash_freeze"), Target->GetActorLocation(), FRotator::ZeroRotator, 1.f, TEXT("frost.freeze"));
		}
	}
	// Shark harpoon (Blood + Tide 50): a harpoon's blow heals you a share of what it did.
	if (Hit.PathId == TpPathId && Hit.bWeaponArt && Attacker->HasTag(Combo_SharkHarpoon) && Damage > 0.f)
	{
		const UWRAttributeSet* AA = Attacker->GetAttributes();
		if (AA) TpSetAttr(Attacker, UWRAttributeSet::GetHealthAttribute(), FMath::Min(AA->GetMaxHealth(), AA->GetHealth() + Damage * TpCombo(TEXT("shark_harpoon"), TEXT("heal"), 0.2f)));
	}
	// Depths (Tide + Necromancy 50): your minions' bites on a swimmer drag it 2 m down.
	if (WRNecro::IsRaised(Attacker))
	{
		AWRCharacterBase* Master = WRNecro::MasterOf(Attacker);
		if (Master && Master->HasTag(Combo_Depths) && Target->GetCharacterMovement() && Target->GetCharacterMovement()->IsSwimming())
		{
			Target->LaunchCharacter(FVector(0, 0, -TpCombo(TEXT("depths"), TEXT("drag"), 400.f)), false, false);
		}
	}
}

float WRTidePath::SoakSlow(const AActor* Who)
{
	float Slow = WRTune(TEXT("status.defs.soak.slow"), 0.8f);
	const TWeakObjectPtr<AWRCharacterBase>* By = TpSoakedBy().Find(Who);
	if (By && By->IsValid()) Slow -= UWRPathComponent::Stat(By->Get(), TEXT("soakSlow"));
	return FMath::Max(TpT(TEXT("slowFloor"), 0.4f), Slow);
}

float WRTidePath::MoveSpeedScale(const AActor* Who)
{
	const AWRCharacterBase* C = Cast<AWRCharacterBase>(Who);
	if (!C) return 1.f;
	float S = 1.f;
	const float Per = TpT(TEXT("currentSpeed"), 0.06f);
	if (C->HasTag(Status_Current3)) S *= 1.f + Per * 3.f;
	else if (C->HasTag(Status_Current2)) S *= 1.f + Per * 2.f;
	else if (C->HasTag(Status_Current)) S *= 1.f + Per;
	// Heavy water: a creature soaked by a Heavy-water walker slows further (the base soak slow is the shared one).
	if (C->IsCreature() && C->HasTag(WRTags::Status_Soak))
	{
		const float Base = WRTune(TEXT("status.defs.soak.slow"), 0.8f);
		S *= SoakSlow(Who) / FMath::Max(0.1f, Base);
	}
	return S;
}

float WRTidePath::SoakExtraSeconds(const AActor* Who)
{
	return Who ? FMath::Max(0.f, UWRPathComponent::Stat(Who, TEXT("soakDuration"))) : 0.f;
}

float WRTidePath::StanceRegenScale(const AActor* Who)
{
	if (!TpSoaked(Who)) return 1.f;
	const TWeakObjectPtr<AWRCharacterBase>* By = TpSoakedBy().Find(Who);
	return By && By->IsValid() && Rank(By->Get(), TEXT("drowning")) > 0 ? 0.f : 1.f;
}

float WRTidePath::StanceTakenScale(const AActor* Who)
{
	if (!TpSoaked(Who)) return 1.f;
	const TWeakObjectPtr<AWRCharacterBase>* By = TpSoakedBy().Find(Who);
	return By && By->IsValid() && Rank(By->Get(), TEXT("drowning")) > 0 ? 1.f + TpT(TEXT("drowningStance"), 0.15f) : 1.f;
}

float WRTidePath::MendScale(const AActor* Who)
{
	return 1.f + FMath::Max(0.f, UWRPathComponent::Stat(Who, TEXT("tideHeal")));
}

void WRTidePath::GiveTidalShield(AWRCharacterBase* Caster, AWRCharacterBase* Target, float Amount, float Seconds)
{
	if (!Target || !Target->HasAuthority()) return;
	WRArcane::GiveShield(Target, Amount, Seconds, Caster);
	FTpShield& S = TpShields().FindOrAdd(Target);
	S.Caster = Caster;
	S.Until = TpNow(Target) + Seconds;
}

// ================================================================================================ the body

uint32 WRTidePath::LookSig(const AActor* Hero)
{
	const float F = MaxFlowOf(Hero) > 0.f ? FlowOf(Hero) / MaxFlowOf(Hero) : 0.f;
	return (uint32(FMath::RoundToInt(F * 8.f)) << 1) | (WRUnderwater::IsUnder(Hero) ? 1u : 0u);
}

void WRTidePath::ApplyLook(const AActor* Hero, const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Skin, FName Main, float MainS, FName Sub, float SubS)
{
	// High Flow brightens the sheen (cook 2.18); at full strength the sheen glows faintly under water in the dark, like
	// the night surf (the gills' opening is the skin material's own Underwater switch).
	if (Main != TpPathId && Sub != TpPathId) return;
	const FString Prefix = Main == TpPathId ? TEXT("Main") : TEXT("Sub");
	const float S = Main == TpPathId ? MainS : SubS;
	const float F = MaxFlowOf(Hero) > 0.f ? FlowOf(Hero) / MaxFlowOf(Hero) : 0.f;
	const bool bUnder = WRUnderwater::IsUnder(Hero);
	for (UMaterialInstanceDynamic* M : Skin)
	{
		if (!M) continue;
		M->SetScalarParameterValue(FName(*(Prefix + TEXT("Strength"))), FMath::Min(1.f, S * (TpT(TEXT("lookFlowBase"), 0.8f) + TpT(TEXT("lookFlowRange"), 0.4f) * F)));
		M->SetScalarParameterValue(FName(*(Prefix + TEXT("EmissiveNits"))), bUnder && S >= 0.99f ? TpT(TEXT("lookUnderGlow"), 0.6f) * (0.5f + 0.5f * F) : 0.f);
		M->SetScalarParameterValue(FName(*(Prefix + TEXT("PulseHz"))), bUnder && S >= 0.75f ? TpT(TEXT("gillFlutterHz"), 1.2f) + F : 0.f);
		M->SetScalarParameterValue(FName(*(Prefix + TEXT("PulseAmp"))), bUnder && S >= 0.75f ? TpT(TEXT("gillFlutterAmp"), 0.3f) : 0.f);
	}
}
