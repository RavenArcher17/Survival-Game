// The Void path's abilities (see WRAbilitiesVoid.h; cooks/biome-void.md 2.17).
#include "WRAbilitiesVoid.h"
#include "WRRaiding.h"
#include "Wildreach.h"
#include "WRAttributeSet.h"
#include "WRBoss.h"
#include "WRBuildLook.h"
#include "WRBuildLookKit.h"
#include "WRCharacter.h"
#include "WRCharacterBase.h"
#include "WRCombatLibrary.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRLand.h"
#include "WRPathComponent.h"
#include "WRPlayerController.h"
#include "WRProjectile.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRTuning.h"
#include "WRVoid.h"
#include "WRVoidPath.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"

namespace WRVoidAim
{
	static const FLinearColor VaViolet = FLinearColor(FColor(0x7B, 0x4F, 0xD6));
	static const FLinearColor VaDeep = FLinearColor(FColor(0x2B, 0x18, 0x40));

	AWRCharacterBase* VaLocked(const AWRCharacterBase* Caster)
	{
		const AWRPlayerController* PC = Caster ? Cast<AWRPlayerController>(Caster->GetController()) : nullptr;
		AWRCharacterBase* Target = PC ? (Caster->HasAuthority() ? PC->GetServerLock() : PC->GetLockTarget()) : nullptr;
		return Target && Target->IsAlive() && !Target->IsHidden() && Caster->IsHostileTo(Target)
			&& FVector::Dist(Target->GetActorLocation(), Caster->GetActorLocation()) <= WRTune(TEXT("spells.lockRange"), 4000.f) ? Target : nullptr;
	}
	FVector VaAimDir(const AWRCharacterBase* Caster, const FVector& From, float Speed, AWRCharacterBase*& OutLock)
	{
		OutLock = VaLocked(Caster);
		if (OutLock)
		{
			const FVector Velocity = WRTune(TEXT("spells.lockLead"), 1.f) > 0.f && Speed > 0.f ? OutLock->GetVelocity() : FVector::ZeroVector;
			return (UWRCombatLibrary::LeadPoint(From, OutLock->GetActorLocation(), Velocity, FMath::Max(1.f, Speed)) - From).GetSafeNormal();
		}
		const FVector View = Caster->GetBaseAimRotation().Vector();
		const AWRCharacter* S = Cast<AWRCharacter>(Caster);
		FVector Point;
		if (!S || !S->GetController() || !S->CrosshairPoint(20000.f, true, Point)) return View;
		const FVector Dir = (Point - From).GetSafeNormal();
		return Dir.IsNearlyZero() || FVector::DotProduct(Dir, View) < 0.3f ? View : Dir;
	}
	double VaServerNow(const UObject* W)
	{
		const UWorld* World = W ? W->GetWorld() : nullptr;
		const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
		return GS ? GS->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);
	}
	FVector VaGround(const UWorld* World, const FVector& P, const AActor* Ignore)
	{
		const float Z = WRLand::GroundZAt(P, TNumericLimits<float>::Lowest());
		if (Z > TNumericLimits<float>::Lowest()) return FVector(P.X, P.Y, Z);
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRVoidAimGround), false, Ignore);
		return World->LineTraceSingleByChannel(Hit, P + FVector(0, 0, 800.f), P - FVector(0, 0, 3000.f), ECC_WorldStatic, Q) ? FVector(Hit.ImpactPoint) : P;
	}
	bool VaIsBoss(const AActor* A) { return Cast<AWRBoss>(A) != nullptr; }
	FName VaTierOf(const AActor* A)
	{
		const AWRCreature* C = Cast<AWRCreature>(A);
		return C && C->GetDef() ? C->GetDef()->Tier : FName(TEXT("highland"));
	}
}
using namespace WRVoidAim;

// ================================================================================================ the spell base

UWRGA_VoidSpell::UWRGA_VoidSpell()
{
	StaminaKey = TEXT("stamina.spell");
	StaminaFallback = 12.f;
	PathId = TEXT("void");
	ActivationOwnedTags.AddTag(WRTags::State_Attacking);
	FGameplayTagContainer Blocked = ActivationBlockedTags;
	Blocked.AddTag(WRTags::State_Attacking);
	Blocked.AddTag(WRTags::State_Dodging);
	ActivationBlockedTags = Blocked;
}

float UWRGA_VoidSpell::Spell(const TCHAR* Field, float Fallback) const
{
	return WRVoidPath::SpellNum(NodeId, Field, Fallback);
}

bool UWRGA_VoidSpell::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags)) return false;
	const AWRCharacterBase* Who = ActorInfo ? Cast<AWRCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	FString Why;
	if (WRVoidPath::CanUse(Who, NodeId, bAlwaysCost, Why)) return true;
	// The self-cost limit: the dry crack and the slot's refusal flash (like stamina).
	if (Who && Who->IsLocallyControlled())
	{
		WRSound::PlayUI(Who, TEXT("void.refused"));
		if (AWRPlayerController* PC = Cast<AWRPlayerController>(Who->GetController())) PC->ShowLocalNotice(Why);
	}
	return false;
}

void UWRGA_VoidSpell::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	Super::ApplyCost(Handle, ActorInfo, ActivationInfo);
	AWRCharacterBase* Who = ActorInfo ? Cast<AWRCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (Who && HasAuthority(&ActivationInfo)) WRVoidPath::Pay(Who, NodeId, bAlwaysCost);
}

void UWRGA_VoidSpell::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
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
	const float Windup = Spell(TEXT("windup"), T.Num(ShapeRow + TEXT(".windup"), 0.3f));
	const float Recovery = Spell(TEXT("recovery"), T.Num(ShapeRow + TEXT(".recovery"), 0.35f));
	if (T.Num(ShapeRow + TEXT(".roots"), 0.f) > 0.f) SetMoveRoot(true);
	else SetMoveSlow(true, true);
	Caster->PlaySwing(EWRSwingKind::Cast, Windup + 0.1f);
	Caster->SetCombatTag(WRTags::State_Casting, true);
	// The coil: the hand closing on a point of black (biome-void 2.17.4).
	if (IsServer()) WRFx::BroadcastOn(Caster, TEXT("void.cast"), TEXT("hand"), 1.f, CastSound, Windup + 0.1f);
	After(Windup, [this]()
	{
		AWRCharacterBase* C = GetCharacter();
		if (!C) return;
		C->SetCombatTag(WRTags::State_Casting, false);
		if (IsServer() && C->IsAlive()) CastVoid(C);
	});
	After(Windup + Recovery, [this]() { EndSelf(); });
}

FVector UWRGA_VoidSpell::AimGround(const AWRCharacterBase* Caster, float Range) const
{
	if (AWRCharacterBase* Lock = VaLocked(Caster)) if (FVector::Dist(Lock->GetActorLocation(), Caster->GetActorLocation()) <= Range) return VaGround(Caster->GetWorld(), Lock->GetActorLocation(), Caster);
	FVector Point;
	const AWRCharacter* S = Cast<AWRCharacter>(Caster);
	if (S && S->CrosshairPoint(Range, false, Point) && FVector::Dist(Point, Caster->GetActorLocation()) <= Range) return VaGround(Caster->GetWorld(), Point, Caster);
	const FVector Ahead = Caster->GetActorLocation() + FRotator(0.f, Caster->GetControlRotation().Yaw, 0.f).Vector() * FMath::Min(Range, 600.f);
	return VaGround(Caster->GetWorld(), Ahead, Caster);
}

// ---------------------------------------------------------------- Void bolt

UWRGA_VoidBolt::UWRGA_VoidBolt()
{
	DisplayName = TEXT("Void bolt"); NodeId = TEXT("void_bolt");
	CooldownKey = TEXT("spells.voidBolt.cooldown"); CooldownFallback = 1.2f;
	ShapeName = TEXT("bolt");
	bAllowedMounted = true;
}

void UWRGA_VoidBolt::CastVoid(AWRCharacterBase* Caster)
{
	const float Speed = Spell(TEXT("speed"), 2800.f);
	const FVector Chest = Caster->GetActorLocation() + FVector(0.f, 0.f, 40.f);
	AWRCharacterBase* Lock = nullptr;
	const FVector Dir = VaAimDir(Caster, Chest, Speed, Lock);
	FWRHit Hit;
	Hit.Instigator = Caster;
	Hit.Damage = Spell(TEXT("damage"), 24.f);
	Hit.PoiseDamage = Spell(TEXT("poise"), 8.f);
	Hit.Element = WRTags::Element_Void;
	Hit.PathId = PathId;
	Hit.bSpell = true;
	Hit.Origin = Chest;
	FActorSpawnParameters Params;
	Params.Owner = Caster;
	Params.Instigator = Caster;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AWRProjectile* Bolt = Caster->GetWorld()->SpawnActor<AWRProjectile>(AWRProjectile::StaticClass(), Chest + Dir * 70.f, Dir.Rotation(), Params))
	{
		Bolt->Init(Hit, Speed, VaDeep);
		if (Lock)
		{
			const float Homing = Lock->IsCreature() ? WRTune(TEXT("spells.lockHoming"), 6000.f) : Spell(TEXT("lockHomingPlayers"), 0.f);
			if (Homing > 0.f) Bolt->HomeOn(Lock, Homing);
		}
	}
}

// ---------------------------------------------------------------- Void rift, Black hole

namespace WRVoidAim
{
	AWRVoidField* VaSpawnField(AWRCharacterBase* Caster, const FVector& At, uint8 Kind, float Radius, float Seconds)
	{
		const FTransform T(At);
		AWRVoidField* F = Caster->GetWorld()->SpawnActorDeferred<AWRVoidField>(AWRVoidField::StaticClass(), T, Caster, Caster, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!F) return nullptr;
		F->Kind = Kind;
		F->Radius = Radius;
		F->Seconds = Seconds;
		F->Born = VaServerNow(Caster);
		F->Caster = Caster;
		F->bSwallow = UWRPathComponent::HasNode(Caster, TEXT("void"), TEXT("event_horizon"));
		F->bSpark = Kind == 1 && Caster->HasTag(WRVoidPath::Combo_SingularitySpark);
		F->SetLifeSpan(Seconds + 1.f);
		return F;
	}
}

UWRGA_VoidRift::UWRGA_VoidRift()
{
	DisplayName = TEXT("Void rift"); NodeId = TEXT("void_rift");
	CooldownKey = TEXT("spells.voidRift.cooldown"); CooldownFallback = 12.f;
	ShapeName = TEXT("pool");
}

void UWRGA_VoidRift::CastVoid(AWRCharacterBase* Caster)
{
	const FVector At = AimGround(Caster, Spell(TEXT("range"), 1500.f));
	if (AWRVoidField* F = VaSpawnField(Caster, At, 0, Spell(TEXT("radius"), 300.f), Spell(TEXT("seconds"), 6.f)))
	{
		F->Dps = Spell(TEXT("dps"), 14.f);
		F->FinishSpawning(FTransform(At));
	}
}

UWRGA_BlackHole::UWRGA_BlackHole()
{
	DisplayName = TEXT("Black hole"); NodeId = TEXT("black_hole");
	CooldownKey = TEXT("spells.blackHole.cooldown"); CooldownFallback = 60.f;
	ShapeName = TEXT("strike");
}

void UWRGA_BlackHole::CastVoid(AWRCharacterBase* Caster)
{
	const FVector At = AimGround(Caster, Spell(TEXT("range"), 2000.f));
	if (AWRVoidField* F = VaSpawnField(Caster, At, 1, Spell(TEXT("radius"), 600.f), Spell(TEXT("seconds"), 4.f)))
	{
		F->Pull = Spell(TEXT("pull"), 600.f);
		F->Crush = Spell(TEXT("damage"), 80.f);
		F->BossSlow = Spell(TEXT("bossSlow"), 0.3f);
		F->FinishSpawning(FTransform(At));
	}
}

// ---------------------------------------------------------------- Collapse

UWRGA_Collapse::UWRGA_Collapse()
{
	DisplayName = TEXT("Collapse"); NodeId = TEXT("collapse");
	CooldownKey = TEXT("spells.collapse.cooldown"); CooldownFallback = 16.f;
	ShapeName = TEXT("nova");
}

void UWRGA_Collapse::CastVoid(AWRCharacterBase* Caster)
{
	// An implosion round you: everything within 5 m is dragged in to your feet (lowland and midland all the way, highland
	// half, bosses not at all), then crushed for 30.
	const FVector Centre = Caster->GetActorLocation();
	const float Radius = Spell(TEXT("radius"), 500.f);
	const float PullT = Spell(TEXT("pullSeconds"), 0.6f);
	WRFx::Broadcast(Caster, TEXT("void.collapse"), Centre, FRotator::ZeroRotator, Radius / 500.f, TEXT("void.collapse"));
	TArray<TWeakObjectPtr<AWRCharacterBase>> Pulled;
	for (TActorIterator<AWRCharacterBase> It(Caster->GetWorld()); It; ++It)
	{
		if (*It == Caster || !It->IsAlive() || !Caster->IsHostileTo(*It) || FVector::Dist(It->GetActorLocation(), Centre) > Radius + It->GetSimpleCollisionRadius()) continue;
		Pulled.Add(*It);
		if (VaIsBoss(*It)) continue;
		const float Share = VaTierOf(*It) == TEXT("highland") ? Spell(TEXT("highlandShare"), 0.5f) : 1.f;
		const FVector To = Centre - It->GetActorLocation();
		const float Dist = FMath::Max(0.f, To.Size2D() - Caster->GetSimpleCollisionRadius() - It->GetSimpleCollisionRadius() - 30.f) * Share;
		It->LaunchCharacter(To.GetSafeNormal2D() * Dist / FMath::Max(0.1f, PullT) + FVector(0, 0, 120.f), true, true);
	}
	TWeakObjectPtr<AWRCharacterBase> WeakCaster(Caster);
	const float Damage = Spell(TEXT("damage"), 30.f);
	FTimerHandle H;
	Caster->GetWorldTimerManager().SetTimer(H, [WeakCaster, Pulled, Damage, Radius, this]()
	{
		AWRCharacterBase* C = WeakCaster.Get();
		if (!C) return;
		// Path raiding (cookpass-building 3.7): the implosion crushes the buildings in it too (void x1.25 on all).
		FWRRaiding::SplashPieces(C->GetWorld(), C->GetActorLocation(), Radius, Damage, EWRRaidSource::Spell, C, FWRRaiding::Responsible(C), TEXT("void"));
		for (const TWeakObjectPtr<AWRCharacterBase>& W : Pulled)
		{
			AWRCharacterBase* V = W.Get();
			if (!V || !V->IsAlive()) continue;
			FWRHit Hit;
			Hit.Instigator = C;
			Hit.Damage = Damage;
			Hit.Element = WRTags::Element_Void;
			Hit.PathId = TEXT("void");
			Hit.bSpell = true;
			Hit.Impact = 3;
			Hit.Origin = C->GetActorLocation();
			UWRCombatLibrary::ApplyHit(V, Hit);
		}
		WRFx::Broadcast(C, TEXT("hit.void"), C->GetActorLocation(), FRotator::ZeroRotator, 2.f, TEXT("void.crush"));
	}, PullT, false);
}

// ---------------------------------------------------------------- Maw of the void

UWRGA_MawOfTheVoid::UWRGA_MawOfTheVoid()
{
	DisplayName = TEXT("Maw of the void"); NodeId = TEXT("maw_of_the_void");
	CooldownKey = TEXT("spells.mawOfTheVoid.cooldown"); CooldownFallback = 90.f;
	ShapeName = TEXT("wall");
}

void UWRGA_MawOfTheVoid::CastVoid(AWRCharacterBase* Caster)
{
	// A four-petal maw of black glass opens out of the ground in front of you and closes (0.8 s): the cone 90 deg x 8 m.
	const float Range = Spell(TEXT("range"), 800.f), Arc = Spell(TEXT("arc"), 90.f), Close = Spell(TEXT("close"), 0.8f);
	const FRotator Face(0.f, Caster->GetControlRotation().Yaw, 0.f);
	const FVector Feet = Caster->GetActorLocation() - FVector(0, 0, Caster->GetSimpleCollisionHalfHeight());
	const FTransform T(Face, Feet + Face.Vector() * Range * 0.5f);
	if (AWRVoidMaw* M = Caster->GetWorld()->SpawnActorDeferred<AWRVoidMaw>(AWRVoidMaw::StaticClass(), T, Caster, Caster, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		M->Born = VaServerNow(Caster);
		M->Range = Range;
		M->Close = Close;
		M->SetLifeSpan(Close + 1.2f);
		M->FinishSpawning(T);
	}
	TWeakObjectPtr<AWRCharacterBase> WeakCaster(Caster);
	const float Damage = Spell(TEXT("damage"), 90.f), EatBelow = Spell(TEXT("eatBelow"), 0.3f), EatRestore = Spell(TEXT("eatRestore"), 5.f);
	FTimerHandle H;
	Caster->GetWorldTimerManager().SetTimer(H, [WeakCaster, Face, Range, Arc, Damage, EatBelow, EatRestore]()
	{
		AWRCharacterBase* C = WeakCaster.Get();
		if (!C) return;
		const FVector From = C->GetActorLocation();
		for (TActorIterator<AWRCharacterBase> It(C->GetWorld()); It; ++It)
		{
			if (*It == C || !It->IsAlive() || !C->IsHostileTo(*It)) continue;
			const FVector To = It->GetActorLocation() - From;
			if (To.Size2D() > Range + It->GetSimpleCollisionRadius() || FVector::DotProduct(Face.Vector(), To.GetSafeNormal2D()) < FMath::Cos(FMath::DegreesToRadians(Arc * 0.5f))) continue;
			// Lowland and midland foes under 30% are eaten outright (each wins back 5 max HP).
			const AWRCreature* Cr = Cast<AWRCreature>(*It);
			const bool bEat = Cr && !VaIsBoss(Cr) && (VaTierOf(Cr) == TEXT("lowland") || VaTierOf(Cr) == TEXT("midland")) && Cr->GetHealthFraction() < EatBelow;
			FWRHit Hit;
			Hit.Instigator = C;
			Hit.Damage = bEat ? FMath::Max(Damage, It->GetAttributes() ? It->GetAttributes()->GetHealth() + 1.f : Damage) : Damage;
			Hit.Element = WRTags::Element_Void;
			Hit.PathId = TEXT("void");
			Hit.bSpell = true;
			Hit.bUnblockable = bEat;
			Hit.Impact = 4;
			Hit.Origin = From;
			UWRCombatLibrary::ApplyHit(*It, Hit);
			if (bEat) WRVoid::RestoreMaxHealth(C, EatRestore);
		}
		WRSound::PlayAt(C, TEXT("void.maw.close"), From + Face.Vector() * Range * 0.5f);
		// The petals close on whatever stands in the cone, buildings too (its middle and half its reach).
		FWRRaiding::SplashPieces(C->GetWorld(), From + Face.Vector() * Range * 0.5f, Range * 0.5f, Damage, EWRRaidSource::Spell, C, FWRRaiding::Responsible(C), TEXT("void"));
	}, Close, false);
}

// ---------------------------------------------------------------- self buffs

UWRGA_Phase::UWRGA_Phase()
{
	DisplayName = TEXT("Phase"); NodeId = TEXT("phase");
	CooldownKey = TEXT("spells.phase.cooldown"); CooldownFallback = 10.f;
	ShapeName = TEXT("buff");
}

void UWRGA_Phase::CastVoid(AWRCharacterBase* Caster)
{
	// You turn grey-translucent: blows and grabs pass through you for 0.6 s (+Fade).
	const float S = Spell(TEXT("seconds"), 0.6f) + WRVoidPath::FadeSeconds(Caster);
	if (Caster->Status) Caster->Status->ApplyStatus(WRVoidPath::Status_Phased, S);
	WRFx::BroadcastOn(Caster, TEXT("void.phase"), NAME_None, 1.f, TEXT("void.phase"), S);
}

UWRGA_Nowhere::UWRGA_Nowhere()
{
	DisplayName = TEXT("Nowhere"); NodeId = TEXT("nowhere");
	CooldownKey = TEXT("spells.nowhere.cooldown"); CooldownFallback = 60.f;
	ShapeName = TEXT("buff");
}

void UWRGA_Nowhere::CastVoid(AWRCharacterBase* Caster)
{
	// Your body goes to a shimmer of bent light: untargetable 3 s (everything after you loses you), the next hit crits.
	const float S = Spell(TEXT("seconds"), 3.f);
	if (Caster->Status) Caster->Status->ApplyStatus(WRVoidPath::Status_Nowhere, S);
	for (TActorIterator<AWRCreature> It(Caster->GetWorld()); It; ++It)
	{
		if (It->GetAggroTarget() == Caster) It->LoseSightOf(Caster, S);
	}
	WRVoidPath::Unseen(Caster);
	WRFx::BroadcastOn(Caster, TEXT("void.nowhere"), NAME_None, 1.f, TEXT("void.nowhere"), S);
	// The crit waits on the path's clock (WRVoidPath::DamageScale uses it up on your next blow).
	WRVoidPath::ArmCrit(Caster);
}

UWRGA_Sacrifice::UWRGA_Sacrifice()
{
	DisplayName = TEXT("Sacrifice"); NodeId = TEXT("sacrifice");
	CooldownKey = TEXT("spells.sacrifice.cooldown"); CooldownFallback = 30.f;
	ShapeName = TEXT("buff");
	bAlwaysCost = true;
}

void UWRGA_Sacrifice::CastVoid(AWRCharacterBase* Caster)
{
	const float S = Spell(TEXT("seconds"), 10.f);
	if (Caster->Status) Caster->Status->ApplyStatus(WRVoidPath::Status_Sacrifice, S);
	WRFx::BroadcastOn(Caster, TEXT("void.sacrifice"), TEXT("hand"), 1.f, TEXT("void.sacrifice"), S);
}

// ---------------------------------------------------------------- Blink, Shadow step

UWRGA_VoidBlink::UWRGA_VoidBlink()
{
	DisplayName = TEXT("Blink"); PathId = TEXT("void"); NodeId = TEXT("blink");
	StaminaKey = TEXT("stamina.spell"); StaminaFallback = 10.f;
	CooldownKey = TEXT("spells.blink.cooldown"); CooldownFallback = 3.f;
	FGameplayTagContainer Blocked = ActivationBlockedTags;
	Blocked.AddTag(WRTags::State_Dodging);
	ActivationBlockedTags = Blocked;
	CancelAbilitiesWithTag.AddTag(WRTags::Ability_Melee);
	CancelAbilitiesWithTag.AddTag(WRTags::Ability_Block);
	bRules = true;
	RuleAct = WRCombatRules::EAct::Dodge;
}

UWRGA_ShadowStep::UWRGA_ShadowStep()
{
	DisplayName = TEXT("Shadow step"); NodeId = TEXT("shadow_step");
	CooldownKey = TEXT("spells.shadowStep.cooldown"); CooldownFallback = 8.f;
	Key = TEXT("spells.shadowStep");
	bBehindLock = true;
}

bool UWRGA_VoidBlink::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)) return false;
	// Shadow step needs someone locked to step behind.
	const AWRCharacterBase* Who = ActorInfo ? Cast<AWRCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	return !bBehindLock || VaLocked(Who) != nullptr;
}

bool UWRGA_VoidBlink::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags)) return false;
	const AWRCharacterBase* Who = ActorInfo ? Cast<AWRCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	FString Why;
	if (WRVoidPath::CanUse(Who, NodeId, false, Why)) return true;
	if (Who && Who->IsLocallyControlled()) WRSound::PlayUI(Who, TEXT("void.refused"));
	return false;
}

void UWRGA_VoidBlink::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	Super::ApplyCost(Handle, ActorInfo, ActivationInfo);
	AWRCharacterBase* Who = ActorInfo ? Cast<AWRCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (Who && HasAuthority(&ActivationInfo)) WRVoidPath::Pay(Who, NodeId, false);
}

void UWRGA_VoidBlink::SetIFrames(bool bOn)
{
	AWRCharacterBase* C = GetCharacter();
	if (!C || !IsServer() || bIFrames == bOn) return;
	bIFrames = bOn;
	if (bOn) C->GetAbilitySystemComponent()->AddLooseGameplayTag(WRTags::State_Invulnerable);
	else C->GetAbilitySystemComponent()->RemoveLooseGameplayTag(WRTags::State_Invulnerable);
}

void UWRGA_VoidBlink::SetPassThrough(bool bOn)
{
	AWRCharacterBase* C = GetCharacter();
	UCapsuleComponent* Capsule = C ? C->GetCapsuleComponent() : nullptr;
	if (!Capsule || bPassing == bOn) return;
	bPassing = bOn;
	if (bOn) { WasPawn = Capsule->GetCollisionResponseToChannel(ECC_Pawn); Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap); }
	else Capsule->SetCollisionResponseToChannel(ECC_Pawn, WasPawn);
}

void UWRGA_VoidBlink::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AWRCharacterBase* C = GetCharacter();
	if (!C || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	const FWRTuning& T = FWRTuning::Get();
	const float Windup = T.Num(Key + TEXT(".windup"), T.Num(TEXT("spells.shapes.dash.windup"), 0.06f));
	const float TravelTime = FMath::Max(0.05f, T.Num(Key + TEXT(".travel"), 0.18f));
	const float Recovery = T.Num(Key + TEXT(".recovery"), 0.2f);
	const FVector From = C->GetActorLocation();
	// Where: toward the stick (else the aim) for Blink; 1.2 m behind the lock, facing its back, for Shadow step.
	FVector Dir = C->GetCharacterMovement()->GetCurrentAcceleration().GetSafeNormal2D();
	if (Dir.IsNearlyZero()) Dir = FRotator(0.f, C->GetControlRotation().Yaw, 0.f).Vector();
	float Distance = T.Num(Key + TEXT(".distance"), 700.f);
	AWRCharacterBase* Lock = bBehindLock ? VaLocked(C) : nullptr;
	if (Lock)
	{
		const FVector Behind = Lock->GetActorLocation() - Lock->GetActorForwardVector().GetSafeNormal2D() * (T.Num(Key + TEXT(".behind"), 120.f) + Lock->GetSimpleCollisionRadius() + C->GetSimpleCollisionRadius());
		const FVector To = Behind - From;
		Dir = To.GetSafeNormal2D();
		Distance = To.Size2D();
	}
	C->SetActorRotation(Dir.Rotation());
	if (IsServer())
	{
		WRFx::Broadcast(C, TEXT("void.blink.smear"), From, Dir.Rotation(), 1.f, TEXT("void.blink.snap"));
		// Frozen blink (Frost + Void 25): the spot you left freezes.
		if (C->HasTag(WRVoidPath::Combo_FrozenBlink))
		{
			const float R = WRTune(TEXT("voidPath.combos.frozenBlinkRadius"), 250.f);
			AWRHazard::Spawn(C->GetWorld(), C, From - FVector(0, 0, C->GetSimpleCollisionHalfHeight()), R, WRTune(TEXT("voidPath.combos.frozenBlinkSeconds"), 4.f), 0.f, TEXT("frost"), false);
			WRFx::Broadcast(C, TEXT("frostnova"), From - FVector(0, 0, C->GetSimpleCollisionHalfHeight()), FRotator::ZeroRotator, R / 300.f, NAME_None);
		}
	}
	TWeakObjectPtr<AWRCharacterBase> WeakLock(Lock);
	After(Windup, [this, Dir, Distance, TravelTime]()
	{
		AWRCharacterBase* Ch = GetCharacter();
		if (!Ch) return;
		SetPassThrough(true);
		SetIFrames(true);
		Ch->SetCombatTag(WRTags::State_IFrames, true);
		Travel = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(this, TEXT("VoidBlink"), Dir, Distance / TravelTime, TravelTime, false, nullptr,
			ERootMotionFinishVelocityMode::ClampVelocity, FVector::ZeroVector, Ch->GetCharacterMovement()->GetMaxSpeed(), true);
		if (Travel) Travel->ReadyForActivation();
	});
	// The i-frames hold the whole travel, then Fade's extra.
	After(Windup + TravelTime + WRVoidPath::FadeSeconds(C), [this]()
	{
		SetIFrames(false);
		if (AWRCharacterBase* Ch = GetCharacter()) Ch->SetCombatTag(WRTags::State_IFrames, false);
	});
	After(Windup + TravelTime, [this, WeakLock]()
	{
		SetPassThrough(false);
		AWRCharacterBase* Ch = GetCharacter();
		if (!Ch || !IsServer()) return;
		WRFx::Broadcast(Ch, TEXT("void.blink.smear"), Ch->GetActorLocation(), Ch->GetActorRotation(), 1.f, NAME_None);
		if (const AWRCharacterBase* L = WeakLock.Get())
		{
			// Arrived behind it, facing its back: the backstab arc holds.
			Ch->SetActorRotation(FRotator(0.f, (L->GetActorLocation() - Ch->GetActorLocation()).Rotation().Yaw, 0.f));
		}
		WRVoidPath::Unseen(Ch);
	});
	After(Windup + TravelTime + Recovery, [this]() { EndSelf(); });
}

void UWRGA_VoidBlink::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	SetIFrames(false);
	SetPassThrough(false);
	if (AWRCharacterBase* Ch = GetCharacter()) Ch->SetCombatTag(WRTags::State_IFrames, false);
	Travel = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// ---------------------------------------------------------------- Null strike

UWRGA_NullStrike::UWRGA_NullStrike()
{
	DisplayName = TEXT("Null strike"); PathId = TEXT("void"); NodeId = TEXT("null_strike");
	StaminaKey = TEXT("stamina.weaponArt"); StaminaFallback = 18.f;
	TimingKey = TEXT("weapon.lightTiming"); SwingKind = EWRSwingKind::WeaponArt;
	Element = WRTags::Element_Void;
	StanceMultiplier = 1.5f;
	CooldownKey = TEXT("spells.nullStrike.cooldown"); CooldownFallback = 3.f;
	ArtKey = TEXT("spells.nullStrike");
	ActivationOwnedTags.AddTag(WRVoidPath::Ability_NullStrike);
}

bool UWRGA_NullStrike::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags)) return false;
	const AWRCharacterBase* Who = ActorInfo ? Cast<AWRCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	FString Why;
	if (WRVoidPath::CanUse(Who, NodeId, false, Why)) return true;
	if (Who && Who->IsLocallyControlled()) WRSound::PlayUI(Who, TEXT("void.refused"));
	return false;
}

void UWRGA_NullStrike::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	Super::ApplyCost(Handle, ActorInfo, ActivationInfo);
	AWRCharacterBase* Who = ActorInfo ? Cast<AWRCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (Who && HasAuthority(&ActivationInfo)) WRVoidPath::Pay(Who, NodeId, false);
}

void UWRGA_NullStrike::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	// The blade runs black with a violet edge for the swing.
	if (IsServer() && IsActive()) WRFx::BroadcastOn(GetCharacter(), TEXT("void.weapon"), TEXT("weapon"), 1.f, TEXT("void.null_strike"), 0.6f);
}

// ---------------------------------------------------------------- Consume

UWRGA_Consume::UWRGA_Consume()
{
	DisplayName = TEXT("Consume"); PathId = TEXT("void"); NodeId = TEXT("consume");
	StaminaKey = TEXT("stamina.weaponArt"); StaminaFallback = 18.f;
	CooldownKey = TEXT("spells.consume.cooldown"); CooldownFallback = 15.f;
	ActivationOwnedTags.AddTag(WRTags::State_Attacking);
	FGameplayTagContainer Blocked = ActivationBlockedTags;
	Blocked.AddTag(WRTags::State_Attacking);
	Blocked.AddTag(WRTags::State_Dodging);
	ActivationBlockedTags = Blocked;
}

AWRCharacterBase* UWRGA_Consume::Prey(const AWRCharacterBase* Who)
{
	if (!Who) return nullptr;
	const float Reach = WRVoidPath::SpellNum(TEXT("consume"), TEXT("reach"), 260.f);
	float Threshold = WRVoidPath::SpellNum(TEXT("consume"), TEXT("threshold"), 0.25f) + UWRPathComponent::Stat(Who, TEXT("consumeThreshold"));
	Threshold = FMath::Min(Threshold, 0.35f);
	const bool bIceMaw = Who->HasTag(WRVoidPath::Combo_IceMaw);
	AWRCharacterBase* Best = nullptr;
	float BestD = Reach;
	for (TActorIterator<AWRCharacterBase> It(Who->GetWorld()); It; ++It)
	{
		if (*It == Who || !It->IsAlive() || !Who->IsHostileTo(*It)) continue;
		const FVector To = It->GetActorLocation() - Who->GetActorLocation();
		const float D = To.Size2D() - It->GetSimpleCollisionRadius() - Who->GetSimpleCollisionRadius();
		if (D > BestD || FVector::DotProduct(Who->GetActorForwardVector().GetSafeNormal2D(), To.GetSafeNormal2D()) < 0.5f) continue;
		const UWRAttributeSet* A = It->GetAttributes();
		const float Frac = A ? A->GetHealth() / FMath::Max(1.f, A->GetMaxHealth()) : 1.f;
		// Bosses only while their stance is broken; Ice maw: any frozen non-boss; Insatiable 3: Riftborn and highland
		// creatures at full.
		bool bOk = VaIsBoss(*It) ? It->HasTag(WRTags::Status_StanceBroken) : Frac < Threshold || (bIceMaw && It->HasTag(WRTags::Status_Frozen));
		if (!bOk && UWRPathComponent::Stat(Who, TEXT("consumeThreshold")) >= 0.09f && !VaIsBoss(*It) && VaTierOf(*It) == TEXT("highland")) bOk = true;
		if (bOk) { BestD = D; Best = *It; }
	}
	return Best;
}

bool UWRGA_Consume::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)) return false;
	const AWRCharacterBase* Who = ActorInfo ? Cast<AWRCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (Prey(Who)) return true;
	if (Who && Who->IsLocallyControlled())
	{
		WRSound::PlayUI(Who, TEXT("void.refused"));
		if (AWRPlayerController* PC = Cast<AWRPlayerController>(Who->GetController())) PC->ShowLocalNotice(TEXT("Nothing weak enough in front of you to consume"));
	}
	return false;
}

void UWRGA_Consume::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	Super::ApplyCost(Handle, ActorInfo, ActivationInfo);
	AWRCharacterBase* Who = ActorInfo ? Cast<AWRCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (Who && HasAuthority(&ActivationInfo)) WRVoidPath::Pay(Who, NodeId, false);   // gain -20: it takes Void out
}

void UWRGA_Consume::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AWRCharacterBase* C = GetCharacter();
	AWRCharacterBase* V = Prey(C);
	if (!C || !V || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	Victim = V;
	const float Windup = WRVoidPath::SpellNum(NodeId, TEXT("windup"), 0.35f), Paired = WRVoidPath::SpellNum(NodeId, TEXT("paired"), 1.4f);
	SetMoveRoot(true);
	C->SetActorRotation(FRotator(0.f, (V->GetActorLocation() - C->GetActorLocation()).Rotation().Yaw, 0.f));
	C->PlaySwing(EWRSwingKind::WeaponArt, Windup + Paired * 0.5f);
	if (IsServer())
	{
		// Paired: it's held while the black maw in your hand takes it (1.4 s), then it's gone.
		if (V->Status) V->Status->Stagger(Windup + Paired);
		WRFx::BroadcastOn(V, TEXT("void.consume"), TEXT("spine"), 1.2f, TEXT("void.consume"), Windup + Paired);
	}
	After(Windup + Paired, [this]()
	{
		AWRCharacterBase* Ch = GetCharacter();
		AWRCharacterBase* Vi = Victim.Get();
		if (!Ch || !Vi || !IsServer() || !Vi->IsAlive()) return;
		FWRHit Hit;
		Hit.Instigator = Ch;
		Hit.Element = WRTags::Element_Void;
		Hit.PathId = TEXT("void");
		Hit.bUnblockable = true;
		Hit.bCritical = true;
		Hit.Impact = 5;
		Hit.Origin = Ch->GetActorLocation();
		// A boss (stance-broken) takes the riposte x1.2 instead of being eaten.
		Hit.Damage = VaIsBoss(Vi) ? UWRCombatLibrary::WeaponDamage(Ch->Weapon, false) * WRTune(TEXT("voidPath.critMult"), 3.f) * WRVoidPath::SpellNum(TEXT("consume"), TEXT("bossRiposte"), 1.2f)
			: (Vi->GetAttributes() ? Vi->GetAttributes()->GetHealth() + 1.f : 9999.f);
		UWRCombatLibrary::ApplyHit(Vi, Hit);
		// It gives back 10% of what the Void took (Abyssal hunger heals a quarter of your max HP too).
		if (const UWRAttributeSet* A = Ch->GetAttributes())
		{
			WRVoid::RestoreMaxHealth(Ch, A->GetMaxHealthLost() * WRVoidPath::SpellNum(TEXT("consume"), TEXT("restore"), 0.1f));
			if (Ch->HasTag(WRVoidPath::Combo_AbyssalHunger))
			{
				const float Heal = A->GetMaxHealth() * WRTune(TEXT("voidPath.combos.abyssalHungerHeal"), 0.25f);
				Ch->GetAbilitySystemComponent()->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), FMath::Min(A->GetEffectiveMaxHealth(), A->GetHealth() + Heal));
			}
		}
	});
	After(Windup + Paired + 0.3f, [this]() { EndSelf(); });
}

// ================================================================================================ the void field

AWRVoidField::AWRVoidField()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(false);
	Tear = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Tear"));
	Tear->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Tear->SetCastShadow(false);
	RootComponent = Tear;
	SetNetCullDistanceSquared(FMath::Square(15000.f));
}

void AWRVoidField::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRVoidField, Kind);
	DOREPLIFETIME(AWRVoidField, Radius);
	DOREPLIFETIME(AWRVoidField, Seconds);
	DOREPLIFETIME(AWRVoidField, Born);
}

double AWRVoidField::Now() const { return VaServerNow(this); }

void AWRVoidField::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() != NM_DedicatedServer) BuildLook();
}

void AWRVoidField::BuildLook()
{
	// Your own small tear (the rift's look in the caster's violet): two crossed lens-shaped faces of M_VoidTear standing on
	// the ground, motes drifting in from the area's edge; a Black hole's tear is a round dark lens hanging 3 m up.
	UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_VoidTear.M_VoidTear"));
	const bool bHole = Kind == 1;
	const float H = bHole ? 360.f : 220.f, W = bHole ? 360.f : 90.f, Z0 = bHole ? 140.f : 0.f;
	TArray<FVector> V; TArray<int32> T; TArray<FVector> N; TArray<FVector2D> UV;
	for (int32 Face = 0; Face < 2; ++Face)
	{
		const FVector Across = Face == 0 ? FVector(0, 1, 0) : FVector(1, 0, 0);
		const int32 Base = V.Num();
		const int32 Segs = 12;
		for (int32 i = 0; i <= Segs; ++i)
		{
			const float S = float(i) / Segs;
			const float Half = W * 0.5f * FMath::Sin(PI * S);
			V.Add(FVector(0, 0, Z0 + H * S) - Across * Half);
			V.Add(FVector(0, 0, Z0 + H * S) + Across * Half);
			N.Add(FVector::CrossProduct(Across, FVector::UpVector)); N.Add(FVector::CrossProduct(Across, FVector::UpVector));
			UV.Add(FVector2D(0.f, S)); UV.Add(FVector2D(1.f, S));
			if (i > 0)
			{
				const int32 a = Base + (i - 1) * 2, b = a + 1, c = a + 2, d = a + 3;
				T.Append({ a, c, b, b, c, d, a, b, c, b, d, c });
			}
		}
	}
	Tear->CreateMeshSection(0, V, T, N, UV, TArray<FColor>(), TArray<FProcMeshTangent>(), false);
	if (M) Tear->SetMaterial(0, UMaterialInstanceDynamic::Create(M, this));
	if (USceneComponent* Fx = WRFx::Attach(Tear, bHole ? TEXT("rift.motes.large") : TEXT("rift.motes"), NAME_None, Radius / (bHole ? 900.f : 600.f))) Fx->SetRelativeLocation(FVector(0, 0, 10.f));
	if (USceneComponent* Fx = WRFx::Attach(Tear, TEXT("rift.edge"), NAME_None, bHole ? 2.f : 0.8f)) Fx->SetRelativeLocation(FVector(0, 0, Z0 + H * 0.5f));
	WRSound::PlayAt(this, bHole ? FName(TEXT("void.blackhole")) : FName(TEXT("void.rift.open")), GetActorLocation());
}

void AWRVoidField::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Age = float(Now() - Born);
	// It opens over 0.3 s and closes over its last 0.4 s (scaled, never popping).
	const float Open = FMath::Clamp(Age / 0.3f, 0.f, 1.f) * FMath::Clamp((Seconds - Age) / 0.4f, 0.f, 1.f);
	if (Tear) Tear->SetRelativeScale3D(FVector(FMath::Max(0.02f, Open)));
	if (!HasAuthority()) return;
	AWRCharacterBase* C = Caster.Get();
	const FVector Centre = GetActorLocation();
	// Event horizon: enemy projectiles that enter it are swallowed (a small inward flash, a gulp).
	if (bSwallow)
	{
		for (TActorIterator<AWRProjectile> It(GetWorld()); It; ++It)
		{
			const AWRCharacterBase* By = Cast<AWRCharacterBase>(It->GetInstigator());
			if (!By || !C || !C->IsHostileTo(By) || FVector::Dist(It->GetActorLocation(), Centre) > Radius) continue;
			WRFx::Broadcast(this, TEXT("void.swallow"), It->GetActorLocation(), FRotator::ZeroRotator, 1.f, TEXT("void.swallow"));
			It->Destroy();
		}
	}
	if (Kind == 0)
	{
		// The rift: 14 a second to every foe inside, in half-second bites (no drain: that's a player's hazard).
		TickAcc += DeltaSeconds;
		if (TickAcc < 0.5f || Age > Seconds) return;
		TickAcc -= 0.5f;
		for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
		{
			if (!It->IsAlive() || !C || !C->IsHostileTo(*It) || FVector::Dist2D(It->GetActorLocation(), Centre) > Radius + It->GetSimpleCollisionRadius() || FMath::Abs(It->GetActorLocation().Z - Centre.Z) > 400.f) continue;
			FWRHit Hit;
			Hit.Instigator = C;
			Hit.Damage = Dps * 0.5f;
			Hit.Element = WRTags::Element_Void;
			Hit.PathId = TEXT("void");
			Hit.bSpell = true;
			Hit.bDamageOverTime = true;
			Hit.Origin = Centre;
			UWRCombatLibrary::ApplyHit(*It, Hit);
		}
		return;
	}
	// The black hole: everything inside dragged to its heart at 600 cm/s (bosses resist: slowed 30% instead), then crushed.
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		if (!It->IsAlive() || !C || !C->IsHostileTo(*It) || Age > Seconds) continue;
		const FVector To = Centre + FVector(0, 0, It->GetSimpleCollisionHalfHeight()) - It->GetActorLocation();
		if (To.Size2D() > Radius + It->GetSimpleCollisionRadius() || FMath::Abs(To.Z) > 600.f) continue;
		if (Cast<AWRBoss>(*It)) { if (It->Status) It->Status->ApplyStatus(WRTags::Status_Slowed, 0.3f); continue; }
		if (To.Size2D() > 60.f) It->LaunchCharacter(To.GetSafeNormal2D() * FMath::Min(Pull, To.Size2D() / FMath::Max(0.05f, DeltaSeconds)), true, false);
	}
	// Singularity spark (Lightning + Void 50): a bolt to a random foe inside every half second.
	if (bSpark && C && Age < Seconds)
	{
		SparkAcc += DeltaSeconds;
		if (SparkAcc >= WRTune(TEXT("voidPath.combos.singularityEvery"), 0.5f))
		{
			SparkAcc = 0.f;
			TArray<AWRCharacterBase*> Inside;
			for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It) if (It->IsAlive() && C->IsHostileTo(*It) && FVector::Dist2D(It->GetActorLocation(), Centre) < Radius) Inside.Add(*It);
			if (Inside.Num())
			{
				AWRCharacterBase* V = Inside[FMath::RandHelper(Inside.Num())];
				FWRHit Hit;
				Hit.Instigator = C;
				Hit.Damage = WRTune(TEXT("voidPath.combos.singularityBolt"), 20.f);
				Hit.Element = WRTags::Element_Lightning;
				Hit.bSpell = true;
				Hit.Buildups.Add(TEXT("shock"), WRTune(TEXT("voidPath.combos.singularityShock"), 25.f));
				Hit.Origin = Centre + FVector(0, 0, 300.f);
				UWRCombatLibrary::ApplyHit(V, Hit);
				WRFx::Broadcast(this, TEXT("lightning.bolt"), Centre + FVector(0, 0, 300.f), (V->GetActorLocation() - Centre).Rotation(), FVector::Dist(V->GetActorLocation(), Centre) / 100.f, TEXT("storm.arc"));
			}
		}
	}
	if (!bCrushed && Age >= Seconds)
	{
		bCrushed = true;
		WRFx::Broadcast(this, TEXT("void.crush"), Centre + FVector(0, 0, 200.f), FRotator::ZeroRotator, Radius / 300.f, TEXT("void.crush"));
		for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
		{
			if (!It->IsAlive() || !C || !C->IsHostileTo(*It) || FVector::Dist2D(It->GetActorLocation(), Centre) > Radius + It->GetSimpleCollisionRadius()) continue;
			FWRHit Hit;
			Hit.Instigator = C;
			Hit.Damage = Crush;
			Hit.Element = WRTags::Element_Void;
			Hit.PathId = TEXT("void");
			Hit.bSpell = true;
			Hit.Impact = 4;
			Hit.Origin = Centre;
			UWRCombatLibrary::ApplyHit(*It, Hit);
		}
	}
}

// ================================================================================================ the maw

AWRVoidMaw::AWRVoidMaw()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(false);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetNetCullDistanceSquared(FMath::Square(15000.f));
}

void AWRVoidMaw::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRVoidMaw, Born);
	DOREPLIFETIME(AWRVoidMaw, Range);
	DOREPLIFETIME(AWRVoidMaw, Close);
}

double AWRVoidMaw::Now() const { return VaServerNow(this); }

void AWRVoidMaw::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer) return;
	using namespace WRLookKit;
	// Four petals of black glass, each a long curved plate edged with hooked teeth, hinged at the ground round the cone's
	// middle; they rise (open) and fold over (close). Each petal is its own frame so it turns about its hinge.
	const FSurf Glass = FSurf::Glaze(FLinearColor(0.004f, 0.0035f, 0.006f), 0.1f);
	const FSurf Edge = FSurf::Lit(VaViolet, 2.5f);
	const float R = Range * 0.45f;
	for (int32 p = 0; p < 4; ++p)
	{
		const float A = 90.f * p + 45.f;
		USceneComponent* Hinge = NewObject<USceneComponent>(this);
		Hinge->SetupAttachment(RootComponent);
		Hinge->RegisterComponent();
		Hinge->SetRelativeLocation(FRotator(0.f, A, 0.f).Vector() * R);
		Hinge->SetRelativeRotation(FRotator(0.f, A + 180.f, 0.f));
		TArray<FWRLookBlock> Blocks;
		FKit K(FTransform::Identity, Blocks);
		for (int32 s = 0; s < 6; ++s)
		{
			const float T0 = s / 6.f;
			K.Add(Glass, FVector(R * 0.2f, R * (0.9f - 0.12f * s), 18.f), FVector(R * 0.18f * s, 0.f, 0.f), FRotator(0.f, 0.f, 0.f));
			K.Spike(Glass, FVector(R * 0.18f * s, R * (0.42f - 0.06f * s), 12.f), FVector(0.3f, 0.f, 1.f).GetSafeNormal(), 10.f, 50.f + 20.f * T0);
			K.Spike(Glass, FVector(R * 0.18f * s, -R * (0.42f - 0.06f * s), 12.f), FVector(0.3f, 0.f, 1.f).GetSafeNormal(), 10.f, 50.f + 20.f * T0);
			K.Add(Edge, FVector(R * 0.2f, 3.f, 3.f), FVector(R * 0.18f * s, R * (0.45f - 0.06f * s), 10.f));
		}
		FWRBuildLook::Spawn(this, Hinge, Blocks, FLinearColor::Black, false);
		Petals.Add(Hinge);
	}
	WRFx::SpawnAt(this, TEXT("void.rift.burst"), GetActorLocation(), GetActorRotation(), Range / 400.f);
	WRSound::PlayAt(this, TEXT("void.maw.open"), GetActorLocation());
}

void AWRVoidMaw::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (GetNetMode() == NM_DedicatedServer) return;
	// Rise out of the ground (0..0.35 of the close), stand open, then fold over and shut at Close; then sink.
	const float T = float(Now() - Born);
	const float Rise = FMath::Clamp(T / (Close * 0.35f), 0.f, 1.f);
	const float Shut = FMath::Clamp((T - Close * 0.5f) / (Close * 0.5f), 0.f, 1.f);
	const float Sink = FMath::Clamp((T - Close - 0.5f) / 0.6f, 0.f, 1.f);
	const float Pitch = FMath::Lerp(-90.f, -20.f, FMath::InterpEaseOut(0.f, 1.f, Rise, 2.f)) + 85.f * FMath::InterpEaseIn(0.f, 1.f, Shut, 3.f);
	for (const TWeakObjectPtr<USceneComponent>& P : Petals)
	{
		if (USceneComponent* C = P.Get())
		{
			const FRotator R = C->GetRelativeRotation();
			C->SetRelativeRotation(FRotator(-Pitch, R.Yaw, 0.f));
			C->SetRelativeScale3D(FVector(FMath::Max(0.01f, 1.f - Sink)));
		}
	}
}
