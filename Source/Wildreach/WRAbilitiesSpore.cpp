// The Spore path's abilities (see WRAbilitiesSpore.h; cooks/biome-fungal.md 2.17).

#include "WRAbilitiesSpore.h"
#include "WRSporePath.h"
#include "WRFungalHazards.h"
#include "Wildreach.h"
#include "WRCharacter.h"
#include "WRCharacterBase.h"
#include "WRCombatLibrary.h"
#include "WRCombos.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRPlayerController.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRTuning.h"
#include "AbilitySystemComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace WRAbilitiesSporeImpl
{
	AWRCharacterBase* SaLocked(const AWRCharacterBase* Caster)
	{
		const AWRPlayerController* PC = Caster ? Cast<AWRPlayerController>(Caster->GetController()) : nullptr;
		AWRCharacterBase* T = PC ? PC->GetServerLock() : nullptr;
		return T && T->IsAlive() && !T->IsHidden() && Caster->IsHostileTo(T) && FVector::Dist(T->GetActorLocation(), Caster->GetActorLocation()) <= WRTune(TEXT("spells.lockRange"), 4000.f) ? T : nullptr;
	}
	bool SaCanPay(const FGameplayAbilityActorInfo* ActorInfo, float Cost)
	{
		const AWRCharacterBase* Who = ActorInfo ? Cast<AWRCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
		if (!Who) return false;
		if (WRSporePath::HasSpores(Who, Cost)) return true;
		if (Who->IsLocallyControlled()) WRSporePath::NoteRefused(Who);
		return false;
	}
	void SaTell(const AActor* Who, const FString& Text)
	{
		const APawn* P = Cast<APawn>(Who);
		if (AWRPlayerController* PC = P ? Cast<AWRPlayerController>(P->GetController()) : nullptr) PC->ClientNotify(Text);
	}
	FVector SaGround(const AActor* Ctx, const FVector& At)
	{
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRSporeGround), false, Ctx);
		return Ctx->GetWorld()->LineTraceSingleByChannel(Hit, At + FVector(0, 0, 500), At - FVector(0, 0, 3000), ECC_WorldStatic, Q) ? FVector(Hit.ImpactPoint) : At;
	}
}
using namespace WRAbilitiesSporeImpl;

// ================================================================================================ the spell base

UWRGA_SporeSpell::UWRGA_SporeSpell()
{
	StaminaKey = TEXT("stamina.spell");
	StaminaFallback = 12.f;
	PathId = TEXT("spore");
	ActivationOwnedTags.AddTag(WRTags::State_Attacking);
	FGameplayTagContainer Blocked = ActivationBlockedTags;
	Blocked.AddTag(WRTags::State_Attacking);
	Blocked.AddTag(WRTags::State_Dodging);
	ActivationBlockedTags = Blocked;
	bAllowedMounted = true;
}

float UWRGA_SporeSpell::Spell(const TCHAR* Field, float Fallback) const { return FWRTuning::Get().Num(SpellKey + TEXT(".") + Field, Fallback); }

bool UWRGA_SporeSpell::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	return Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags) && SaCanPay(ActorInfo, SporeCost());
}

void UWRGA_SporeSpell::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	Super::ApplyCost(Handle, ActorInfo, ActivationInfo);
	AWRCharacterBase* Who = ActorInfo ? Cast<AWRCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (Who && HasAuthority(&ActivationInfo)) WRSporePath::SpendSpores(Who, SporeCost());
}

FVector UWRGA_SporeSpell::AimedGround(AWRCharacterBase* Caster, float Range) const
{
	const FVector Feet = Caster->GetActorLocation() - FVector(0, 0, Caster->GetSimpleCollisionHalfHeight());
	FVector At = Feet + Caster->GetBaseAimRotation().Vector().GetSafeNormal2D() * FMath::Min(Range, 800.f);
	if (const AWRCharacterBase* Lock = SaLocked(Caster)) return SaGround(Caster, Lock->GetActorLocation());
	FVector Point;
	if (const AWRCharacter* S = Cast<AWRCharacter>(Caster); S && S->CrosshairPoint(Range, false, Point)) At = Point;
	return SaGround(Caster, At);
}

void UWRGA_SporeSpell::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
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
	const float Windup = Spell(TEXT("windup"), T.Num(ShapeRow + TEXT(".windup"), 0.4f));
	const float Recovery = Spell(TEXT("recovery"), T.Num(ShapeRow + TEXT(".recovery"), 0.35f));
	const bool bRoots = Spell(TEXT("roots"), T.Num(ShapeRow + TEXT(".roots"), 0.f)) > 0.f;
	if (bRoots) SetMoveRoot(true);
	else SetMoveSlow(true, true);
	Caster->PlaySwing(EWRSwingKind::Cast, Windup + 0.1f);
	Caster->SetCombatTag(WRTags::State_Casting, true);
	if (IsServer()) WRFx::BroadcastOn(Caster, TEXT("spore.cast"), TEXT("hand"), 1.f, TEXT("spore.cast"), Windup + 0.1f);
	After(Windup, [this]()
	{
		AWRCharacterBase* C = GetCharacter();
		if (!C) return;
		C->SetCombatTag(WRTags::State_Casting, false);
		if (!IsServer() || !C->IsAlive()) return;
		CastSpore(C);
	});
	After(Windup + Recovery, [this]() { EndSelf(); });
}

void UWRGA_SporeSpell::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (AWRCharacterBase* C = GetCharacter()) C->SetCombatTag(WRTags::State_Casting, false);
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// ================================================================================================ Spore cloud

UWRGA_SporeCloud::UWRGA_SporeCloud()
{
	DisplayName = TEXT("Spore cloud"); NodeId = TEXT("spore_cloud");
	SpellKey = TEXT("spells.sporeCloud"); CooldownKey = TEXT("spells.sporeCloud.cooldown"); CooldownFallback = 6.f;
	ShapeName = TEXT("pool");
}

void UWRGA_SporeCloud::CastSpore(AWRCharacterBase* Caster)
{
	// A 3 m cloud of cyan spores at the aimed ground: the cloud's own poison on enemies in it, and a Spore planted on
	// each every second (Spore drift: wider; Static cloud: it shocks).
	const float Wide = Caster->HasTag(WRSporePath::Combo_SporeDrift) ? FWRComboRegistry::Get().Num(TEXT("spore_drift"), TEXT("reach"), 1.5f) : 1.f;
	const float R = Spell(TEXT("radius"), 300.f) * Wide;
	const FVector At = AimedGround(Caster, Spell(TEXT("range"), 1500.f));
	const float Seconds = Spell(TEXT("seconds"), 6.f);
	AWRSporeCloud* Cloud = AWRSporeCloud::Spawn(Caster->GetWorld(), At + FVector(0.f, 0.f, 120.f), R, TEXT("path"), Seconds, Caster);
	WRSound::PlayAt(Caster, TEXT("spore.cloud"), At);
	TWeakObjectPtr<AWRCharacterBase> WeakC(Caster);
	TWeakObjectPtr<AWRSporeCloud> WeakCloud(Cloud);
	const float Every = FMath::Max(0.25f, Spell(TEXT("plantEvery"), 1.f));
	const bool bStatic = Caster->HasTag(WRSporePath::Combo_StaticCloud);
	for (float t = Every; t <= Seconds + 0.01f; t += Every)
	{
		FTimerHandle H;
		Caster->GetWorldTimerManager().SetTimer(H, [WeakC, WeakCloud, R, bStatic]()
		{
			AWRCharacterBase* C = WeakC.Get();
			AWRSporeCloud* Cl = WeakCloud.Get();
			if (!C || !Cl || !Cl->IsLive()) return;
			for (TActorIterator<AWRCharacterBase> It(C->GetWorld()); It; ++It)
			{
				if (!It->IsAlive() || !C->IsHostileTo(*It) || !Cl->Contains(It->GetActorLocation(), It->GetSimpleCollisionRadius())) continue;
				WRSporePath::Plant(C, *It, 1);
				if (bStatic && It->Status) It->Status->AddStatusBuildup(WRStatus::Shock, FWRComboRegistry::Get().Num(TEXT("static_cloud"), TEXT("shock"), 10.f), C);
			}
		}, t, false);
	}
}

// ================================================================================================ Detonate

UWRGA_Detonate::UWRGA_Detonate()
{
	DisplayName = TEXT("Detonate"); NodeId = TEXT("detonate");
	SpellKey = TEXT("spells.detonate"); CooldownKey = TEXT("spells.detonate.cooldown"); CooldownFallback = 1.f;
	ShapeName = TEXT("bolt");
	StaminaFallback = 0.f;
}

void UWRGA_Detonate::CastSpore(AWRCharacterBase* Caster)
{
	const int32 N = WRSporePath::Detonate(Caster, Spell(TEXT("range"), 2000.f));
	if (N <= 0) SaTell(Caster, TEXT("No Spores of yours within 20 m"));
}

// ================================================================================================ Blight

UWRGA_Blight::UWRGA_Blight()
{
	DisplayName = TEXT("Blight"); NodeId = TEXT("blight");
	SpellKey = TEXT("spells.blight"); CooldownKey = TEXT("spells.blight.cooldown"); CooldownFallback = 18.f;
	ShapeName = TEXT("pool");
}

void UWRGA_Blight::CastSpore(AWRCharacterBase* Caster)
{
	// A rust-cyan cloud that spreads 50 cm/s from 4 m to 7 m: rot 15 a second on enemies in it. A spore cloud (wind
	// pushes it, fire flashes it).
	const FVector At = AimedGround(Caster, Spell(TEXT("range"), 1500.f));
	const float R0 = Spell(TEXT("radius"), 400.f), R1 = Spell(TEXT("maxRadius"), 700.f), Grow = Spell(TEXT("spread"), 50.f), Seconds = Spell(TEXT("seconds"), 10.f), Rot = Spell(TEXT("rotPerSecond"), 15.f);
	AWRSporeCloud* Cloud = AWRSporeCloud::Spawn(Caster->GetWorld(), At + FVector(0.f, 0.f, 120.f), R0, TEXT("path"), Seconds, Caster);
	WRFx::Broadcast(Caster, TEXT("spore.blight"), At, FRotator::ZeroRotator, R0 / 200.f, TEXT("spore.blight"));
	TWeakObjectPtr<AWRCharacterBase> WeakC(Caster);
	TWeakObjectPtr<AWRSporeCloud> WeakCloud(Cloud);
	for (float t = 0.25f; t <= Seconds + 0.01f; t += 0.25f)
	{
		const bool bTick = FMath::Fmod(t, 1.f) < 0.01f;
		const float Radius = FMath::Min(R1, R0 + Grow * t);
		FTimerHandle H;
		Caster->GetWorldTimerManager().SetTimer(H, [WeakC, WeakCloud, Radius, bTick, Rot]()
		{
			AWRCharacterBase* C = WeakC.Get();
			AWRSporeCloud* Cl = WeakCloud.Get();
			if (!C || !Cl || !Cl->IsLive()) return;
			Cl->Radius = Radius;
			if (!bTick) return;
			for (TActorIterator<AWRCharacterBase> It(C->GetWorld()); It; ++It)
			{
				if (It->IsAlive() && It->Status && C->IsHostileTo(*It) && Cl->Contains(It->GetActorLocation(), It->GetSimpleCollisionRadius())) It->Status->AddStatusBuildup(WRStatus::Rot, Rot, C);
			}
		}, t, false);
	}
}

// ================================================================================================ the summons

UWRGA_Sporeling::UWRGA_Sporeling()
{
	DisplayName = TEXT("Sporeling"); NodeId = TEXT("sporeling");
	SpellKey = TEXT("spells.sporeling"); CooldownKey = TEXT("spells.sporeling.cooldown"); CooldownFallback = 10.f;
	ShapeName = TEXT("summon");
}

void UWRGA_Sporeling::CastSpore(AWRCharacterBase* Caster)
{
	WRSporePath::Summon(Caster, TEXT("sporeling"), Spell(TEXT("seconds"), 25.f), Caster->GetActorLocation() + Caster->GetActorForwardVector() * 200.f);
}

UWRGA_FungalBehemoth::UWRGA_FungalBehemoth()
{
	DisplayName = TEXT("Fungal behemoth"); NodeId = TEXT("fungal_behemoth");
	SpellKey = TEXT("spells.fungalBehemoth"); CooldownKey = TEXT("spells.fungalBehemoth.cooldown"); CooldownFallback = 120.f;
	ShapeName = TEXT("summon");
}

void UWRGA_FungalBehemoth::CastSpore(AWRCharacterBase* Caster)
{
	WRSporePath::Summon(Caster, TEXT("fungal_behemoth"), Spell(TEXT("seconds"), 40.f), Caster->GetActorLocation() + Caster->GetActorForwardVector() * 450.f);
}

// ================================================================================================ the Symbiote

UWRGA_SporeArmor::UWRGA_SporeArmor()
{
	DisplayName = TEXT("Spore armor"); NodeId = TEXT("spore_armor");
	SpellKey = TEXT("spells.sporeArmor"); CooldownKey = TEXT("spells.sporeArmor.cooldown"); CooldownFallback = 30.f;
	ShapeName = TEXT("buff");
}

void UWRGA_SporeArmor::CastSpore(AWRCharacterBase* Caster)
{
	// A coat of soft cyan-tipped fuzz over the body: -15% damage taken for 20 s.
	const float Seconds = Spell(TEXT("seconds"), 20.f);
	if (Caster->Status) Caster->Status->ApplyStatus(WRSporePath::Status_SporeArmor, Seconds);
	WRFx::BroadcastOn(Caster, TEXT("spore.armor"), NAME_None, 1.f, TEXT("spore.armor"), Seconds);
}

UWRGA_LivingColony::UWRGA_LivingColony()
{
	DisplayName = TEXT("Living colony"); NodeId = TEXT("living_colony");
	SpellKey = TEXT("spells.livingColony"); CooldownKey = TEXT("spells.livingColony.cooldown"); CooldownFallback = 120.f;
	ShapeName = TEXT("buff");
}

void UWRGA_LivingColony::CastSpore(AWRCharacterBase* Caster)
{
	// 15 s a walking spore cloud: haloed in drifting cyan motes, your breath many breaths (WRSporePath::TickOwner).
	const float Seconds = Spell(TEXT("seconds"), 15.f);
	if (Caster->Status) Caster->Status->ApplyStatus(WRSporePath::Status_LivingColony, Seconds);
	WRFx::BroadcastOn(Caster, TEXT("spore.colony"), NAME_None, 1.f, TEXT("spore.colony"), Seconds);
}

// ================================================================================================ Infect strike

UWRGA_InfectStrike::UWRGA_InfectStrike()
{
	DisplayName = TEXT("Infect strike"); PathId = TEXT("spore"); NodeId = TEXT("infect_strike");
	StaminaKey = TEXT("stamina.weaponArt"); StaminaFallback = 18.f;
	CooldownKey = TEXT("spells.infectStrike.cooldown"); CooldownFallback = 2.5f;
	TimingKey = TEXT("weapon.lightTiming"); SwingKind = EWRSwingKind::WeaponArt;
	ArtKey = TEXT("spells.infectStrike");
	StanceMultiplier = 1.5f;
	bAllowedMounted = true;
}

bool UWRGA_InfectStrike::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags) && SaCanPay(ActorInfo, WRTune(TEXT("spells.infectStrike.sporeCost"), 10.f));
}

void UWRGA_InfectStrike::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (IsServer() && IsActive())
	{
		WRSporePath::SpendSpores(GetCharacter(), WRTune(TEXT("spells.infectStrike.sporeCost"), 10.f));
		WRSporePath::MarkInfect(GetCharacter(), 1.0f);
		WRFx::BroadcastOn(GetCharacter(), TEXT("spore.infect"), TEXT("weapon"), 1.f, TEXT("spore.infect"), 0.5f);
	}
}
