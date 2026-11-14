#include "WRAbilities.h"
#include "Wildreach.h"
#include "WRAttributeSet.h"
#include "WRCharacterBase.h"
#include "WRCharacterMovementComponent.h"
#include "WRCombatLibrary.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRPathComponent.h"
#include "WRProjectile.h"
#include "WRResourceNode.h"
#include "WRWorldDressing.h"
#include "WRBuilding.h"
#include "WRBoss.h"
#include "WRCharacter.h"
#include "WRPlayerController.h"
#include "GameFramework/PlayerState.h"
#include "WRStatusComponent.h"
#include "WRTuning.h"
#include "WRLifeSkills.h"
#include "WRCreature.h"
#include "WRWeapons.h"
#include "WRLag.h"
#include "WRCombatRules.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionMoveToForce.h"
#include "Components/CapsuleComponent.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "EngineUtils.h"

// ---------------------------------------------------------------- where a spell goes
namespace WRSpellAim
{
	/** What the caster has locked on to, if a spell can still go for it (the client tells the server when it locks on). */
	static AWRCharacterBase* Locked(const AWRCharacterBase* Caster)
	{
		const AWRPlayerController* PC = Caster ? Cast<AWRPlayerController>(Caster->GetController()) : nullptr;
		AWRCharacterBase* Target = PC ? PC->GetServerLock() : nullptr;
		return Target && Target->IsAlive() && !Target->IsHidden() && Caster->IsHostileTo(Target)
			&& FVector::Dist(Target->GetActorLocation(), Caster->GetActorLocation()) <= WRTune(TEXT("spells.lockRange"), 4000.f) ? Target : nullptr;
	}

	/** From the caster's chest toward where the target will be when a bolt of this speed gets there. */
	static FVector Leading(const FVector& From, const AWRCharacterBase* Target, float Speed)
	{
		const FVector Velocity = WRTune(TEXT("spells.lockLead"), 1.f) > 0.f ? Target->GetVelocity() : FVector::ZeroVector;
		return (UWRCombatLibrary::LeadPoint(From, Target->GetActorLocation(), Velocity, Speed) - From).GetSafeNormal();
	}

	/** From the caster's chest through the point under the crosshair (it isn't the chest's line: the camera sits behind the shoulder). */
	static FVector ThroughCrosshair(const AWRCharacterBase* Caster, const FVector& From)
	{
		const FVector View = Caster->GetBaseAimRotation().Vector();
		const AWRCharacter* Survivor = Cast<AWRCharacter>(Caster);
		FVector Point;
		if (!Survivor || !Survivor->GetController())
		{
			return View;
		}
		Survivor->CrosshairPoint(20000.f, true, Point);
		const FVector Dir = (Point - From).GetSafeNormal();
		// Something right in your face, or a point off behind you: straight down your look instead.
		return Dir.IsNearlyZero() || FVector::DistSquared(Point, From) < FMath::Square(100.f) || FVector::DotProduct(Dir, View) < 0.3f ? View : Dir;
	}
}

// ---------------------------------------------------------------- base

UWRGameplayAbility::UWRGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer Blocked;
	Blocked.AddTag(WRTags::State_Dead);
	Blocked.AddTag(WRTags::Status_Frozen);
	Blocked.AddTag(WRTags::Status_Staggered);
	Blocked.AddTag(WRTags::Status_Unconscious);
	ActivationBlockedTags = Blocked;
}

AWRCharacterBase* UWRGameplayAbility::GetCharacter() const
{
	return Cast<AWRCharacterBase>(GetAvatarActorFromActorInfo());
}

bool UWRGameplayAbility::IsServer() const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	return Avatar && Avatar->HasAuthority();
}

bool UWRGameplayAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	if (!bRules)
	{
		return true;
	}
	const AWRCharacterBase* Who = ActorInfo ? Cast<AWRCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!Who)
	{
		return false;
	}
	// A cast in progress (no swing windows) holds every other attack until it's out.
	if (RuleAct == WRCombatRules::EAct::Attack && Who->HasTag(WRTags::State_Attacking) && !Who->IsSwinging())
	{
		return false;
	}
	return WRCombatRules::Allows(Who, RuleAct, WRCombatRules::ToleranceFor(Who));
}

bool UWRGameplayAbility::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (StaminaKey.IsEmpty() && CostOverride < 0.f)
	{
		return true;
	}
	AWRCharacterBase* Character = ActorInfo ? Cast<AWRCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	// Souls-style: any stamina left lets you act, and the cost may take you below zero (stamina.graceFloor, ER's last
	// swing); nothing starts while it's at or under zero.
	const bool bOk = Character && Character->GetAttributes() && Character->GetAttributes()->GetStamina() > 0.f;
	if (!bOk && Character && Character->IsLocallyControlled() && Character->GetWorld())
	{
		Character->StaminaRefusedAt = Character->GetWorld()->GetTimeSeconds();  // the HUD flashes the bar: no dead button
	}
	return bOk;
}

void UWRGameplayAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	if (StaminaKey.IsEmpty() || !HasAuthority(&ActivationInfo))
	{
		return;
	}
	AWRCharacterBase* Character = ActorInfo ? Cast<AWRCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (Character && Character->GetAttributes())
	{
		const float Cost = CostOverride >= 0.f ? CostOverride : FWRTuning::Get().Num(StaminaKey, StaminaFallback);
		Character->GetAbilitySystemComponent()->SetNumericAttributeBase(UWRAttributeSet::GetStaminaAttribute(),
			Character->GetAttributes()->GetStamina() - Cost);
		Character->Status->NotifyStaminaUsed();
	}
}

float UWRGameplayAbility::GetCooldownSeconds() const
{
	return CooldownKey.IsEmpty() ? CooldownFallback : FWRTuning::Get().Num(CooldownKey, CooldownFallback);
}

float UWRGameplayAbility::GetCooldownRemaining() const
{
	const UWorld* World = GetWorld();
	return World ? FMath::Max(0.f, static_cast<float>(LastCommitTime + GetCooldownSeconds() - World->GetTimeSeconds())) : 0.f;
}

bool UWRGameplayAbility::CheckCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	return GetCooldownRemaining() <= 0.f;
}

void UWRGameplayAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	if (const UWorld* World = GetWorld())
	{
		LastCommitTime = World->GetTimeSeconds();
	}
}

bool UWRGameplayAbility::CommitAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, FGameplayTagContainer* OptionalRelevantTags)
{
	if (!Super::CommitAbility(Handle, ActorInfo, ActivationInfo, OptionalRelevantTags))
	{
		return false;
	}
	if (IsServer())
	{
		UE_LOG(LogWildreach, Verbose, TEXT("ABILITY %s used %s"), *GetNameSafe(GetAvatarActorFromActorInfo()), *GetClass()->GetName());
	}
	if (IsServer() && !PathId.IsNone())
	{
		// Path XP comes from using that path's abilities.
		if (UWRPathComponent* Paths = UWRPathComponent::FindFor(GetAvatarActorFromActorInfo()))
		{
			Paths->AwardXP(PathId, FMath::RoundToInt(WRTune(TEXT("paths.xpPerAbilityUse"), 6.f)));
		}
		// The path marks flare on every machine (hero.md 2.6: the combat pulse; Lightning's scar flicker and crackle).
		if (AWRCharacter* Hero = Cast<AWRCharacter>(GetAvatarActorFromActorInfo()))
		{
			Hero->OnPathAbilityUsed(PathId);
		}
	}
	return true;
}

void UWRGameplayAbility::After(float Delay, TFunction<void()> Fn)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (Delay <= 0.f)
	{
		Fn();
		return;
	}
	// A step can set more timers (a swing's re-sweeps) or start things that do (a hit, a riposte), and a new timer can
	// grow the timer manager's storage, moving the delegate that's running and the captures in it: the server crashed
	// reading `this` from a moved closure. So the step lives on the heap and runs from a local reference.
	const TSharedRef<TFunction<void()>> Step = MakeShared<TFunction<void()>>(MoveTemp(Fn));
	FTimerHandle& Handle = Timers.AddDefaulted_GetRef();
	World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [Step]()
	{
		const TSharedRef<TFunction<void()>> Run = Step;
		(*Run)();
	}), Delay, false);
}

void UWRGameplayAbility::EndSelf(bool bCancelled)
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bCancelled);
	}
}

void UWRGameplayAbility::SetMoveSlow(bool bBlock, bool bOn)
{
	AWRCharacterBase* Character = GetCharacter();
	UWRCharacterMovementComponent* Move = Character ? Cast<UWRCharacterMovementComponent>(Character->GetCharacterMovement()) : nullptr;
	if (!Move)
	{
		return;
	}
	// Only the machine that predicts movement sets the flag; for remote players the server
	// receives it inside their saved moves.
	const bool bPredicts = Character->IsLocallyControlled() || (Character->HasAuthority() && Character->GetRemoteRole() != ROLE_AutonomousProxy);
	if (!bPredicts)
	{
		return;
	}
	if (bBlock)
	{
		Move->bBlockSlow = bOn;
		bAppliedBlockSlow = bOn;
	}
	else
	{
		Move->bAttackSlow = bOn;
		bAppliedAttackSlow = bOn;
	}
}

void UWRGameplayAbility::SetMoveRoot(bool bOn)
{
	AWRCharacterBase* Character = GetCharacter();
	UWRCharacterMovementComponent* Move = Character ? Cast<UWRCharacterMovementComponent>(Character->GetCharacterMovement()) : nullptr;
	if (!Move)
	{
		return;
	}
	const bool bPredicts = Character->IsLocallyControlled() || (Character->HasAuthority() && Character->GetRemoteRole() != ROLE_AutonomousProxy);
	if (bPredicts)
	{
		Move->bAttackRoot = bOn;
		bAppliedRoot = bOn;
	}
}

void UWRGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bAppliedAttackSlow) SetMoveSlow(false, false);
	if (bAppliedBlockSlow) SetMoveSlow(true, false);
	if (bAppliedRoot) SetMoveRoot(false);
	CostOverride = -1.f;
	if (UWorld* World = GetWorld())
	{
		for (FTimerHandle& Timer : Timers)
		{
			World->GetTimerManager().ClearTimer(Timer);
		}
	}
	Timers.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

/// ---------------------------------------------------------------- melee

UWRGA_Melee::UWRGA_Melee()
{
	ActivationOwnedTags.AddTag(WRTags::State_Attacking);
	FGameplayTagContainer Blocked = ActivationBlockedTags;
	Blocked.AddTag(WRTags::State_Dodging);
	// Not State_Attacking: the next swing of a chain starts from the last one's combo window (WRCombatRules), and the
	// same ability starting again ends the swing it's in (bRetriggerInstancedAbility). Not State_Blocking: a swing comes
	// straight out of a held guard; the guard drops while you swing (a block only counts when you aren't attacking,
	// WRCombatLibrary) and is back up once the swing ends if Guard is still held.
	ActivationBlockedTags = Blocked;
	// A new swing (or a roll / guard from the cancel window) cuts the one in progress.
	CancelAbilitiesWithTag.AddTag(WRTags::Ability_Melee);
	bRetriggerInstancedAbility = true;
	Element = WRTags::Element_Physical;
	// Survivors' swings and weapon arts wait for the combo window (WRCombatRules); creature bites set bRules false.
	bRules = true;
	RuleAct = WRCombatRules::EAct::Attack;
	FGameplayTagContainer Tags;
	Tags.AddTag(WRTags::Ability_Melee);
	SetAssetTags(Tags);
}

FName UWRGA_Melee::PickMove(const AWRCharacterBase* Character) const
{
	const FWRWeaponClass& Class = FWRWeapons::Get().ClassFor(Character);
	if (bHeavy)
	{
		// Heavy inside the window after a blocked blow is the guard counter (COMBAT-AND-LESSONS 1.2).
		return Character->HasTag(WRTags::State_GuardCounterWindow) && Class.Move(TEXT("guardCounter")) ? FName(TEXT("guardCounter")) : FName(TEXT("heavy"));
	}
	// The light chain: the next light of a swing that got to its combo window (and ended no longer ago than chainGrace).
	const double Now = Character->GetWorld()->GetTimeSeconds();
	if (Character->bLastSwingChains && Now - Character->LastSwingEnd <= WRTune(TEXT("combat.chainGrace"), 0.35f))
	{
		const FWRMove* Last = Class.Move(Character->LastSwingMove);
		if (Last && !Last->Next.IsNone() && Class.Move(Last->Next))
		{
			return Last->Next;
		}
	}
	return TEXT("light1");
}

AWRCharacterBase* UWRGA_Melee::StepTarget(AWRCharacterBase* Character, float Reach) const
{
	// The lock-on target (this machine's: the owner's own lock, the server's copy of it), if it's near enough.
	const float Range = WRTune(TEXT("combat.stepIn.range"), 300.f);
	AWRCharacterBase* Lock = nullptr;
	if (const AWRPlayerController* PC = Cast<AWRPlayerController>(Character->GetController()))
	{
		Lock = Character->HasAuthority() && !Character->IsLocallyControlled() ? PC->GetServerLock() : PC->GetLockTarget();
	}
	const auto Gap = [Character](const AWRCharacterBase* T)
	{
		return FVector::Dist2D(T->GetActorLocation(), Character->GetActorLocation()) - T->GetSimpleCollisionRadius() - Character->GetSimpleCollisionRadius();
	};
	if (Lock && Lock->IsAlive() && Character->IsHostileTo(Lock) && Gap(Lock) <= Range)
	{
		return Lock;
	}
	// Else the nearest enemy inside the swing's cone ahead of the aim.
	const FVector Aim = FRotator(0.f, Character->GetControlRotation().Yaw, 0.f).Vector();
	const float CosCone = FMath::Cos(FMath::DegreesToRadians(WRTune(TEXT("combat.stepIn.cone"), 60.f) * 0.5f));
	AWRCharacterBase* Best = nullptr;
	float BestGap = Range;
	for (TActorIterator<AWRCharacterBase> It(Character->GetWorld()); It; ++It)
	{
		AWRCharacterBase* T = *It;
		if (T == Character || !T->IsAlive() || T->IsHidden() || !Character->IsHostileTo(T))
		{
			continue;
		}
		const FVector To = (T->GetActorLocation() - Character->GetActorLocation()).GetSafeNormal2D();
		const float G = Gap(T);
		if (G < BestGap && FVector::DotProduct(To, Aim) >= CosCone)
		{
			BestGap = G;
			Best = T;
		}
	}
	return Best;
}

void UWRGA_Melee::SetWindows(AWRCharacterBase* Character, double Now, float Windup, float Active, float Recovery, bool bHyperarmor)
{
	// The swing's windows (COMBAT-AND-LESSONS 1.16): the same numbers on the server and the predicting owner. The combo
	// window is the last comboFraction of the recovery; the cancel window opens cancelFraction into it.
	const FWRTuning& T = FWRTuning::Get();
	const float Cancel = bHeavy ? T.Num(TEXT("combat.cancelFraction.heavy"), 0.65f) : T.Num(TEXT("combat.cancelFraction.light"), 0.5f);
	AWRCharacterBase::FSwingWindows& W = Character->Swing;
	W.Start = Now;
	W.ActiveAt = Now + Windup;
	W.ActiveEnd = W.ActiveAt + Active;
	W.End = W.ActiveEnd + Recovery;
	W.ComboAt = W.ActiveEnd + Recovery * (1.f - T.Num(TEXT("combat.comboFraction"), 0.4f));
	W.CancelAt = W.ActiveEnd + Recovery * Cancel;
	W.Move = bUsesMoves ? MoveId : FName(*GetClass()->GetName());
	W.bLight = !bHeavy;
	bWindowsSet = true;

	TWeakObjectPtr<AWRCharacterBase> Weak(Character);
	Character->SetCombatTag(WRTags::State_Windup, true);
	After(Windup, [Weak]() { if (AWRCharacterBase* C = Weak.Get()) C->SetCombatTag(WRTags::State_Windup, false); });
	if (bHyperarmor)
	{
		// Hyperarmor from the press to the end of the strike (COMBAT-AND-LESSONS 1.5): the punish window after it is real.
		Character->SetCombatTag(WRTags::State_Hyperarmor, true);
		After(Windup + Active, [Weak]() { if (AWRCharacterBase* C = Weak.Get()) C->SetCombatTag(WRTags::State_Hyperarmor, false); });
	}
	After(float(W.ComboAt - Now), [Weak]() { if (AWRCharacterBase* C = Weak.Get()) C->SetCombatTag(WRTags::State_ComboWindow, true); });
	After(float(W.CancelAt - Now), [Weak]() { if (AWRCharacterBase* C = Weak.Get()) C->SetCombatTag(WRTags::State_CancelWindow, true); });
}

void UWRGA_Melee::ClearWindows(AWRCharacterBase* Character, bool bEnded)
{
	if (!bWindowsSet || !Character)
	{
		return;
	}
	bWindowsSet = false;
	const double Now = Character->GetWorld()->GetTimeSeconds();
	// What the next light chains from: this swing, if it got to its combo window (a cancel by a stagger doesn't count).
	Character->LastSwingMove = Character->Swing.Move;
	Character->LastSwingEnd = Now;
	Character->bLastSwingChains = bEnded && bUsesMoves && !bHeavy && Now + WRCombatRules::ToleranceFor(Character) >= Character->Swing.ComboAt;
	Character->Swing.Start = -1.0;
	for (const FGameplayTag& Tag : TArray<FGameplayTag>{ WRTags::State_Windup, WRTags::State_Hyperarmor, WRTags::State_ComboWindow, WRTags::State_CancelWindow })
	{
		Character->SetCombatTag(Tag, false);
	}
}

void UWRGA_Melee::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AWRCharacterBase* Character = GetCharacter();
	if (!Character)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	const FWRTuning& T = FWRTuning::Get();
	const FWRWeaponClass& Class = FWRWeapons::Get().ClassFor(Character);

	// The move: a row of the held weapon's class (light chain, heavy, guard counter), or the ability's own timing
	// (weapon arts, creature bites).
	float Windup = 0.f, Active = 0.f, Recovery = 0.f;
	bool bHyperarmor = bHeavy;
	uint8 Variant = 0;
	const FWRMove* Move = nullptr;
	if (bUsesMoves)
	{
		MoveId = PickMove(Character);
		Move = Class.Move(MoveId);
	}
	if (Move)
	{
		Windup = Move->Windup;
		Active = Move->Active;
		Recovery = Move->Recovery;
		bHyperarmor = Move->bHyperarmor;
		Variant = uint8(FMath::Clamp(Move->Clip, 0, 7));
		CostOverride = Move->Stamina;
	}
	else
	{
		Windup = T.Num(TimingKey + TEXT(".windup"), 0.25f);
		Active = T.Num(TimingKey + TEXT(".active"), 0.18f);
		Recovery = T.Num(TimingKey + TEXT(".recovery"), 0.3f);
		bHyperarmor = bHeavy && !Character->IsCreature();
		CostOverride = -1.f;
	}
	MoveActive = Active;

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();

	// Face the target the swing steps in on, else the aim; a survivor's swing steps in toward it, motion-warped and
	// clamped (COMBAT-AND-LESSONS 1.17): the swing meets the body instead of the air in front of it.
	AWRCharacterBase* Step = Character->IsCreature() ? nullptr : StepTarget(Character, Class.Reach);
	if (Step)
	{
		const FVector To = (Step->GetActorLocation() - Character->GetActorLocation()).GetSafeNormal2D();
		Character->SetActorRotation(To.Rotation());
		const float Gap = FVector::Dist2D(Step->GetActorLocation(), Character->GetActorLocation()) - Step->GetSimpleCollisionRadius() - Character->GetSimpleCollisionRadius();
		const float Want = Gap - T.Num(TEXT("combat.stepIn.ideal"), 0.55f) * Class.Reach;
		const float StepCm = FMath::Min(Want, T.Num(TEXT("combat.stepIn.max"), 130.f));
		if (StepCm >= T.Num(TEXT("combat.stepIn.min"), 45.f) && Character->GetCharacterMovement()->IsMovingOnGround())
		{
			const float Seconds = FMath::Max(0.06f, Windup * T.Num(TEXT("combat.stepIn.stepFraction"), 0.8f));
			if (UAbilityTask_ApplyRootMotionMoveToForce* Warp = UAbilityTask_ApplyRootMotionMoveToForce::ApplyRootMotionMoveToForce(
				this, TEXT("StepIn"), Character->GetActorLocation() + To * StepCm, Seconds, false, MOVE_Walking, false, nullptr,
				ERootMotionFinishVelocityMode::SetVelocity, FVector::ZeroVector, 0.f))
			{
				Warp->ReadyForActivation();
			}
		}
	}
	else if (Character->Controller)
	{
		Character->SetActorRotation(FRotator(0.f, Character->GetControlRotation().Yaw, 0.f));
	}

	Character->PlaySwing(SwingKind, Windup + Active, Variant, Windup, Active, Recovery);
	// The waist split: lights keep the legs walking, heavies root (COMBAT-AND-LESSONS C2).
	if (bHeavy && !Character->IsCreature())
	{
		SetMoveRoot(true);
	}
	else
	{
		SetMoveSlow(false, true);
	}
	SetWindows(Character, Now, Windup, Active, Recovery, bHyperarmor);

	// Swinging out of a held block (survivors): log the guard dropping and coming back, for the bot tests.
	bFromGuard = IsServer() && !Character->IsCreature() && Character->HasTag(WRTags::State_Blocking);
	if (bFromGuard)
	{
		UE_LOG(LogWildreach, Log, TEXT("BLOCK %s swings from the guard (%s): the guard is down for %.2f s"), *Character->GetName(), *GetClass()->GetName(), Windup + Active + Recovery);
	}
	if (IsServer() && bUsesMoves)
	{
		UE_LOG(LogWildreach, Verbose, TEXT("SWING %s %s %.2f/%.2f/%.2f%s"), *Character->GetName(), *MoveId.ToString(), Windup, Active, Recovery, Step ? TEXT(" (stepping in)") : TEXT(""));
	}

	if (IsServer() && !ArtKey.IsEmpty())
	{
		UWRCombatLibrary::GainResource(Character, WRTags::Element_Fire, T.Num(ArtKey + TEXT(".heat"), 0.f));
		UWRCombatLibrary::GainResource(Character, WRTags::Element_Frost, T.Num(ArtKey + TEXT(".chill"), 0.f));
	}

	After(Windup, [this, Active]()
	{
		if (!IsServer())
		{
			return;
		}
		HitThisSwing.Reset();
		ActiveEnd = GetWorld()->GetTimeSeconds() + Active;
		Sweep();
		// Re-sweep through the active window so fast targets still get caught.
		const int32 Steps = FMath::Max(1, FMath::CeilToInt(Active / 0.03f));
		for (int32 i = 1; i <= Steps; ++i)
		{
			After(i * Active / Steps, [this]() { Sweep(); });
		}
	});
	After(Windup + Active + Recovery, [this]() { EndSelf(); });
}

void UWRGA_Melee::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// (Here, not in the swing's last timer: on a server the owning client's end of the swing usually arrives first.)
	AWRCharacterBase* Self = GetCharacter();
	if (bFromGuard && Self && IsActive())
	{
		UE_LOG(LogWildreach, Log, TEXT("BLOCK %s swing done: %s"), *Self->GetName(),
			Self->HasTag(WRTags::State_Blocking) ? TEXT("block still held, guard back up") : TEXT("block let go"));
	}
	bFromGuard = false;
	// A swing cut by the next chain attack still counts as reaching its combo window (it was only allowed there); one
	// cut by a stagger, a freeze or death doesn't.
	const bool bEndedWell = Self && Self->IsAlive() && !Self->HasTag(WRTags::Status_Staggered) && !Self->HasTag(WRTags::Status_Frozen);
	ClearWindows(Self, bEndedWell);
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UWRGA_Melee::Sweep()
{
	AWRCharacterBase* Character = GetCharacter();
	if (!Character || !IsServer())
	{
		return;
	}
	const FWRTuning& T = FWRTuning::Get();
	const FWRWeaponClass& Class = FWRWeapons::Get().ClassFor(Character);
	float Reach = Character->IsCreature() ? T.Num(TEXT("weapon.reach"), 220.f) : Class.Reach;
	// A creature bites from as far as it starts biting (its attackRange): a big one's jaws are well past a weapon's reach.
	if (const AWRCreature* Creature = Cast<AWRCreature>(Character); Creature && Creature->GetDef())
	{
		Reach = FMath::Max(Reach, Creature->GetDef()->AttackRange);
	}
	const float Radius = Character->IsCreature() ? T.Num(TEXT("weapon.sweepRadius"), 45.f) : Class.SweepRadius;
	const FVector Start = Character->GetActorLocation();
	const FVector End = Start + Character->GetActorForwardVector() * Reach;

	TArray<FHitResult> Hits;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(WRMelee), false, Character);
	Character->GetWorld()->SweepMultiByChannel(Hits, Start, End, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(Radius), Params);
	AWRDressing* Dressing = UWRDressingSubsystem::FindDressing(Character->GetWorld());

	// A survivor's swing meets bodies where its player saw them (server rewind, COMBAT-AND-LESSONS 1.17); the world (a
	// building, a node, a carcass) and creatures' bites use the present.
	const bool bRewind = Character->IsPlayerControlled() && !Character->IsLocallyControlled();
	UWRLagSubsystem* Lag = bRewind ? UWRLagSubsystem::Get(Character) : nullptr;
	const double SeenAt = GetWorld()->GetTimeSeconds() - UWRLagSubsystem::SeenDelay(Character);
	TArray<AWRCharacterBase*> Targets;
	if (Lag)
	{
		for (TActorIterator<AWRCharacterBase> It(Character->GetWorld()); It; ++It)
		{
			AWRCharacterBase* Who = *It;
			if (Who == Character || !Who->IsAlive() || !Character->IsHostileTo(Who)
				|| FVector::DistSquared(Who->GetActorLocation(), Start) > FMath::Square(Reach + Radius + 600.f))
			{
				continue;
			}
			FVector Centre;
			float R = 40.f, HH = 90.f;
			if (!Lag->BodyAt(Who, SeenAt, Centre, R, HH))
			{
				Centre = Who->GetActorLocation();
				Who->GetCapsuleComponent()->GetScaledCapsuleSize(R, HH);
			}
			// The swing's sphere along Start-End against the capsule's axis.
			FVector OnSwing, OnBody;
			FMath::SegmentDistToSegmentSafe(Start, End, Centre - FVector(0.f, 0.f, FMath::Max(0.f, HH - R)), Centre + FVector(0.f, 0.f, FMath::Max(0.f, HH - R)), OnSwing, OnBody);
			if (FVector::Dist(OnSwing, OnBody) <= Radius + R)
			{
				Targets.Add(Who);
			}
		}
	}

	for (const FHitResult& HitResult : Hits)
	{
		// Raiding: weapons damage other people's buildings (stations/Hearths/totems belong to their piece).
		AWRBuildPiece* Piece = Cast<AWRBuildPiece>(HitResult.GetActor());
		if (!Piece && HitResult.GetActor())
		{
			Piece = Cast<AWRBuildPiece>(HitResult.GetActor()->GetOwner());
		}
		if (Piece)
		{
			const APlayerState* PS = Character->GetPlayerState();
			if (!HitThisSwing.Contains(Piece) && PS && !Piece->CanAccess(PS->GetPlayerName()))
			{
				HitThisSwing.Add(Piece);
				{
					// Splinters off a wooden wall, grit off a stone one.
					const FWRPieceDef* PieceDef = Piece->GetDef();
					const bool bWood = PieceDef && PieceDef->Material == TEXT("wood");
					const FVector At = HitResult.bBlockingHit ? FVector(HitResult.ImpactPoint) : Piece->GetActorLocation();
					WRFx::Broadcast(Character, bWood ? TEXT("gather.wood") : TEXT("hit.dust"), At, (Start - At).GetSafeNormal().Rotation(), bHeavy ? 1.3f : 1.f, bWood ? TEXT("gather.wood") : TEXT("hit.block"));
				}
				Piece->TakeBuildDamage(UWRCombatLibrary::WeaponDamage(Character->Weapon, bHeavy) * (bHeavy ? 1.5f : 1.f)
					* T.Num(TEXT("building.raidDamageMultiplier"), 1.f), Character);
			}
			continue;
		}
		// Weapon swings also gather from resource nodes (once per swing).
		if (AWRResourceNode* Node = Cast<AWRResourceNode>(HitResult.GetActor()))
		{
			if (!HitThisSwing.Contains(Node))
			{
				HitThisSwing.Add(Node);
				if (Dressing) HitThisSwing.Add(Dressing);  // one gather per swing
				Node->Harvest(Character, bHeavy);
			}
			continue;
		}
		// ...and butcher a kill's body (WRCarcass.cpp), once per swing like a node.
		AWRCreature* Body = Cast<AWRCreature>(HitResult.GetActor());
		if (Body && Body->IsHarvestableBody())
		{
			if (!HitThisSwing.Contains(Body))
			{
				HitThisSwing.Add(Body);
				if (Dressing) HitThisSwing.Add(Dressing);
				Body->HarvestBody(Character, bHeavy);
			}
			continue;
		}
		AWRCharacterBase* Target = Cast<AWRCharacterBase>(HitResult.GetActor());
		if (!Lag && Target && Character->IsHostileTo(Target))
		{
			Targets.AddUnique(Target);
		}
	}

	const FWRMove* Move = bUsesMoves ? Class.Move(MoveId) : nullptr;
	const FWRMove* Light = Class.Move(TEXT("light1"));
	const FWRMove* Heavy = Class.Move(TEXT("heavy"));
	for (AWRCharacterBase* Target : Targets)
	{
		if (!Target || HitThisSwing.Contains(Target) || !Character->IsHostileTo(Target) || !Target->IsAlive())
		{
			continue;
		}
		HitThisSwing.Add(Target);

		float Mult = DamageMultiplier;
		float Burn = BurnBuildup;
		float Frost = FrostBuildup;
		float StanceMult = StanceMultiplier;
		if (!ArtKey.IsEmpty())
		{
			Mult = T.Num(ArtKey + TEXT(".multiplier"), Mult);
			Burn = T.Num(ArtKey + TEXT(".burnBuildup"), Burn);
			Frost = T.Num(ArtKey + TEXT(".frostBuildup"), Frost);
			StanceMult = T.Num(ArtKey + TEXT(".stance"), StanceMult);
		}

		FWRHit Hit;
		Hit.Instigator = Character;
		Hit.Element = Element;
		Hit.BurnBuildup = Burn;
		Hit.FrostBuildup = Frost;
		if (Move)
		{
			// A move row: its damage, stance, poise, guard cost and how it lands (Data/weapons.json).
			Hit.Damage = UWRCombatLibrary::WeaponDamage(Character->Weapon, Move->bHeavy) * Move->Damage;
			Hit.PoiseDamage = Move->Poise;
			Hit.StanceDamage = Move->Stance;
			Hit.StaminaDamage = Move->GuardStamina;
			Hit.Impact = Move->Impact;
			Hit.Tier = Move->Tier;
			Hit.bParryable = true;
		}
		else if (Character->IsCreature())
		{
			// A creature's bite (tuning creatureAI.bite): its flat damage; a blocked bite costs creatureStaminaDamage x it.
			Hit.Damage = UWRCombatLibrary::WeaponDamage(Character->Weapon, bHeavy) * Mult;
			Hit.PoiseDamage = T.Num(TEXT("creatureAI.bite.poise"), 20.f);
			Hit.Impact = uint8(FMath::Clamp(FMath::RoundToInt(T.Num(TEXT("creatureAI.bite.impact"), 2.f)), 1, 5));
			Hit.bParryable = T.Num(TEXT("creatureAI.bite.parryable"), 1.f) > 0.f;
			Hit.bLow = T.Num(TEXT("creatureAI.bite.low"), 0.f) > 0.f;
			Hit.Tier = EWRHitTier::Light;
		}
		else
		{
			// A weapon art: the class's light / heavy numbers, its own multiplier and stance (combat.md 2.12).
			const FWRMove* Base = bHeavy ? Heavy : Light;
			Hit.Damage = UWRCombatLibrary::WeaponDamage(Character->Weapon, bHeavy) * Mult;
			Hit.PoiseDamage = Base ? Base->Poise : 30.f;
			Hit.StanceDamage = (Light ? Light->Stance : 5.f) * StanceMult;
			Hit.StaminaDamage = Base ? Base->GuardStamina : 30.f;
			Hit.Impact = bHeavy ? 3 : 2;
			Hit.Tier = bHeavy ? EWRHitTier::Heavy : EWRHitTier::Light;
			Hit.bParryable = true;
		}
		// An infused weapon gives plain attacks its element (the path infuses the weapon).
		if (Element == WRTags::Element_Physical && !Character->Weapon.Element.IsNone())
		{
			const float Infused = T.Num(TEXT("weapon.infusedBuildup"), 15.f);
			if (Character->Weapon.Element == TEXT("fire")) { Hit.Element = WRTags::Element_Fire; Hit.BurnBuildup += Infused; }
			else if (Character->Weapon.Element == TEXT("frost")) { Hit.Element = WRTags::Element_Frost; Hit.FrostBuildup += Infused; }
		}
		Hit.PathId = PathId;
		Hit.bMelee = true;
		Hit.bHeavy = bHeavy;
		Hit.bWeaponArt = !ArtKey.IsEmpty();
		Hit.Origin = Character->GetActorLocation();
		UWRCombatLibrary::ApplyHit(Target, Hit);
	}
	// The island's trees, rocks and shrubs are instanced decoration, not actors: the swing gathers from the nearest one in reach.
	if (Dressing && !HitThisSwing.Contains(Dressing) && Dressing->HarvestInFront(Character, Reach + Radius, bHeavy))
	{
		HitThisSwing.Add(Dressing);
	}
}

UWRGA_LightAttack::UWRGA_LightAttack()
{
	DisplayName = TEXT("Light attack");
	StaminaKey = TEXT("stamina.lightAttack");
	StaminaFallback = 10.f;
	TimingKey = TEXT("weapon.lightTiming");
	SwingKind = EWRSwingKind::Light;
	bUsesMoves = true;  // light1-2-3 of the held weapon's class (Data/weapons.json)
	FGameplayTagContainer Tags;
	Tags.AddTag(WRTags::Ability_Light);
	Tags.AddTag(WRTags::Ability_Melee);
	SetAssetTags(Tags);
}

UWRGA_HeavyAttack::UWRGA_HeavyAttack()
{
	DisplayName = TEXT("Heavy attack");
	StaminaKey = TEXT("stamina.heavyAttack");
	StaminaFallback = 20.f;
	TimingKey = TEXT("weapon.heavyTiming");
	SwingKind = EWRSwingKind::Heavy;
	bHeavy = true;
	bUsesMoves = true;  // heavy, or the guard counter inside its window (Data/weapons.json)
	FGameplayTagContainer Tags;
	Tags.AddTag(WRTags::Ability_Heavy);
	Tags.AddTag(WRTags::Ability_Melee);
	SetAssetTags(Tags);
}

UWRGA_MoltenEdge::UWRGA_MoltenEdge()
{
	DisplayName = TEXT("Molten edge"); PathId = TEXT("fire"); NodeId = TEXT("molten_edge");
	StaminaKey = TEXT("stamina.weaponArt"); StaminaFallback = 14.f;
	CooldownKey = TEXT("spells.moltenEdge.cooldown"); CooldownFallback = 2.f;
	TimingKey = TEXT("weapon.lightTiming"); SwingKind = EWRSwingKind::WeaponArt;
	Element = WRTags::Element_Fire; ArtKey = TEXT("spells.moltenEdge");
	StanceMultiplier = 1.5f;  // combat.md 2.12
}

UWRGA_Flurry::UWRGA_Flurry()
{
	DisplayName = TEXT("Flurry"); PathId = TEXT("frost"); NodeId = TEXT("flurry");
	StaminaKey = TEXT("stamina.weaponArt"); StaminaFallback = 12.f;
	CooldownKey = TEXT("spells.flurry.cooldown"); CooldownFallback = 1.5f;
	TimingKey = TEXT("weapon.lightTiming"); SwingKind = EWRSwingKind::WeaponArt;
	Element = WRTags::Element_Frost; ArtKey = TEXT("spells.flurry");
	StanceMultiplier = 0.5f;
}

UWRGA_MoltenCleave::UWRGA_MoltenCleave()
{
	DisplayName = TEXT("Molten cleave"); PathId = TEXT("fire"); NodeId = TEXT("molten_cleave");
	StaminaKey = TEXT("stamina.weaponArt"); StaminaFallback = 25.f;
	CooldownKey = TEXT("spells.moltenCleave.cooldown"); CooldownFallback = 12.f;
	TimingKey = TEXT("weapon.heavyTiming"); SwingKind = EWRSwingKind::WeaponArt; bHeavy = true;
	Element = WRTags::Element_Fire; ArtKey = TEXT("spells.moltenCleave");
	StanceMultiplier = 6.f;
}

void UWRGA_MoltenCleave::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	AWRCharacterBase* Caster = GetCharacter();
	if (!Caster || !IsServer())
	{
		return;
	}
	// The cleave leaves a strip of lava where it lands (world timer: outlives the ability).
	TWeakObjectPtr<AWRCharacterBase> Weak = Caster;
	FTimerHandle T;
	Caster->GetWorldTimerManager().SetTimer(T, FTimerDelegate::CreateLambda([Weak]()
	{
		if (AWRCharacterBase* C = Weak.Get())
		{
			const FVector At = C->GetActorLocation() + C->GetActorForwardVector() * 250.f - FVector(0, 0, C->GetSimpleCollisionHalfHeight());
			AWRHazard::Spawn(C->GetWorld(), C, At, WRTune(TEXT("spells.moltenCleave.lavaRadius"), 220.f), WRTune(TEXT("spells.moltenCleave.lavaSeconds"), 6.f),
				WRTune(TEXT("spells.moltenCleave.lavaDps"), 10.f), TEXT("lava"), false);
			WRFx::Broadcast(C, TEXT("lavapool"), At, FRotator::ZeroRotator, WRTune(TEXT("spells.moltenCleave.lavaRadius"), 220.f) / 100.f, TEXT("lavapool"), WRTune(TEXT("spells.moltenCleave.lavaSeconds"), 6.f));
		}
	}), WRTune(TEXT("weapon.heavyTiming.windup"), 0.5f) + 0.05f, false);
}

UWRGA_FlameSlash::UWRGA_FlameSlash()
{
	DisplayName = TEXT("Flame slash");
	PathId = TEXT("fire");
	NodeId = TEXT("flame_slash");
	StaminaKey = TEXT("stamina.weaponArt");
	StaminaFallback = 18.f;
	CooldownKey = TEXT("spells.flameSlash.cooldown");
	CooldownFallback = 3.f;
	TimingKey = TEXT("weapon.heavyTiming");
	SwingKind = EWRSwingKind::WeaponArt;
	bHeavy = true;
	Element = WRTags::Element_Fire;
	ArtKey = TEXT("spells.flameSlash");
	StanceMultiplier = 2.f;
}

UWRGA_IceSlash::UWRGA_IceSlash()
{
	DisplayName = TEXT("Ice slash");
	PathId = TEXT("frost");
	NodeId = TEXT("ice_slash");
	StaminaKey = TEXT("stamina.weaponArt");
	StaminaFallback = 18.f;
	CooldownKey = TEXT("spells.iceSlash.cooldown");
	CooldownFallback = 3.f;
	TimingKey = TEXT("weapon.heavyTiming");
	SwingKind = EWRSwingKind::WeaponArt;
	bHeavy = true;
	Element = WRTags::Element_Frost;
	ArtKey = TEXT("spells.iceSlash");
	StanceMultiplier = 2.f;
}

UWRGA_Bite::UWRGA_Bite()
{
	DisplayName = TEXT("Bite");
	TimingKey = TEXT("creature.biteTiming");
	SwingKind = EWRSwingKind::Bite;
	CooldownFallback = 1.2f;
	bRules = false;  // the creature's AI times its bites
}

/// ---------------------------------------------------------------- dodge

UWRGA_Dodge::UWRGA_Dodge()
{
	DisplayName = TEXT("Dodge");
	StaminaKey = TEXT("stamina.dodge");
	StaminaFallback = 12.f;
	ActivationOwnedTags.AddTag(WRTags::State_Dodging);
	FGameplayTagContainer Blocked = ActivationBlockedTags;
	Blocked.AddTag(WRTags::State_Dodging);
	ActivationBlockedTags = Blocked;
	// A roll from a swing's cancel window cuts it (before then it waits in the buffer: WRCombatRules), and drops a guard.
	CancelAbilitiesWithTag.AddTag(WRTags::Ability_Block);
	CancelAbilitiesWithTag.AddTag(WRTags::Ability_Melee);
	bRules = true;
	RuleAct = WRCombatRules::EAct::Dodge;
	FGameplayTagContainer Tags;
	Tags.AddTag(WRTags::Ability_Dodge);
	SetAssetTags(Tags);
}

FName UWRGA_Dodge::LoadTier(float Ratio)
{
	if (Ratio < WRTune(TEXT("defense.load.lightBelow"), 0.3f)) return TEXT("light");
	if (Ratio < WRTune(TEXT("defense.load.mediumBelow"), 0.7f)) return TEXT("medium");
	if (Ratio < 1.f) return TEXT("heavy");
	return TEXT("overloaded");
}

void UWRGA_Dodge::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AWRCharacterBase* Character = GetCharacter();
	if (!Character || Character->GetCharacterMovement()->IsFalling())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	const FWRTuning& T = FWRTuning::Get();
	// The stick at rest is a backstep; otherwise a roll toward it, by the equip load tier (COMBAT-AND-LESSONS 1.7).
	const FVector Input = Character->GetCharacterMovement()->GetCurrentAcceleration().GetSafeNormal2D();
	const bool bBackstep = Input.IsNearlyZero();
	const FName Tier = bBackstep ? FName(TEXT("backstep")) : LoadTier(Character->GetEquipLoadRatio());
	const FString Key = bBackstep ? FString(TEXT("defense.backstep.")) : FString::Printf(TEXT("defense.load.%s."), *Tier.ToString());
	const bool bOverloaded = Tier == TEXT("overloaded");
	// Light step (frost node): the same roll, quicker (its frames shrink with it).
	const float Quick = 1.f + UWRPathComponent::Stat(Character, TEXT("dodgeSpeed"));
	const float Distance = T.Num(Key + TEXT("distance"), bBackstep ? 250.f : 500.f);
	const float TravelTime = FMath::Max(0.05f, T.Num(Key + TEXT("travel"), bBackstep ? 0.25f : 0.46f) / Quick);
	const float IStart = T.Num(Key + TEXT("iStart"), 0.03f) / Quick;
	const float IEnd = T.Num(Key + TEXT("iEnd"), bBackstep ? 0.13f : 0.46f) / Quick;
	const float Recovery = T.Num(Key + TEXT("recovery"), bBackstep ? 0.22f : 0.27f) / Quick;
	CostOverride = bBackstep ? T.Num(TEXT("stamina.backstep"), 8.f) : T.Num(TEXT("stamina.dodge"), 12.f);
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FVector Dir = bBackstep ? -Character->GetActorForwardVector().GetSafeNormal2D() : Input;
	if (IsServer())
	{
		// Back-dated to the press for a remote player: the perfect dodge is judged from when it really began.
		Character->DodgeStartedAt = GetWorld()->GetTimeSeconds() - UWRLagSubsystem::OneWay(Character);
		const FVector Feet = Character->GetActorLocation() - FVector(0.f, 0.f, Character->GetSimpleCollisionHalfHeight());
		WRFx::Broadcast(Character, TEXT("dodge.dust"), Feet, (-Dir).Rotation(), bBackstep ? 0.7f : 1.f);  // (the dodge sound is the audio director's)
		// Vanish (sneaking perk): the roll breaks the sight of the creatures on you, for a moment.
		const float Vanish = UWRLifeSkillsComponent::StatFor(Character, TEXT("vanish"));
		if (Vanish > 0.f && !bOverloaded)
		{
			for (TActorIterator<AWRCreature> It(Character->GetWorld()); It; ++It)
			{
				if (It->IsAlive() && !It->IsTamed() && It->GetAggroTarget() == Character
					&& FVector::Dist(It->GetActorLocation(), Character->GetActorLocation()) < T.Num(TEXT("lifeSkills.vanishRange"), 1500.f))
				{
					It->LoseSightOf(Character, Vanish);
				}
			}
		}
		UE_LOG(LogWildreach, Verbose, TEXT("DODGE %s %s %.0f cm over %.2f s, i-frames %.2f-%.2f"), *Character->GetName(), *Tier.ToString(), Distance, TravelTime, IStart, IEnd);
	}

	// Ledges: a roll stops at a drop of more than ledgeDrop unless the stick still holds toward it when it gets there
	// (no accidental death rolls; you can still roll off on purpose). Same check on the server and the owner.
	float Edge = -1.f;
	{
		const float HalfHeight = Character->GetSimpleCollisionHalfHeight();
		const float Drop = T.Num(TEXT("defense.ledgeDrop"), 200.f);
		FCollisionQueryParams Query(SCENE_QUERY_STAT(WRDodgeLedge), false, Character);
		for (float D = 40.f; D <= Distance; D += 40.f)
		{
			const FVector At = Character->GetActorLocation() + Dir * D;
			FHitResult Ground;
			if (!GetWorld()->LineTraceSingleByChannel(Ground, At + FVector(0.f, 0.f, 40.f), At - FVector(0.f, 0.f, HalfHeight + Drop), ECC_Visibility, Query))
			{
				Edge = FMath::Max(0.f, D - 40.f + Character->GetSimpleCollisionRadius() * 0.5f);
				break;
			}
		}
	}

	Travel = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
		this, TEXT("Dodge"), Dir, Distance / TravelTime, TravelTime, false, nullptr,
		ERootMotionFinishVelocityMode::ClampVelocity, FVector::ZeroVector, Character->GetCharacterMovement()->GetMaxSpeed(), true);
	if (Travel)
	{
		Travel->ReadyForActivation();
	}
	if (Edge >= 0.f && Edge < Distance)
	{
		const float AtEdge = TravelTime * Edge / Distance;
		TWeakObjectPtr<AWRCharacterBase> Weak(Character);
		After(AtEdge, [this, Weak, Dir]()
		{
			AWRCharacterBase* C = Weak.Get();
			if (!C || !Travel)
			{
				return;
			}
			const FVector Stick = C->GetCharacterMovement()->GetCurrentAcceleration().GetSafeNormal2D();
			if (FVector::DotProduct(Stick, Dir) < WRTune(TEXT("defense.ledgeStickDot"), 0.7f))
			{
				Travel->EndTask();
				Travel = nullptr;
				C->GetCharacterMovement()->StopMovementImmediately();
			}
		});
	}

	if (IEnd > IStart && !bOverloaded)
	{
		After(IStart, [this]() { SetInvulnerable(true); });
		After(IEnd, [this]() { SetInvulnerable(false); });
	}
	After(FMath::Max(TravelTime, IEnd) + Recovery, [this]() { EndSelf(); });
}

void UWRGA_Dodge::SetInvulnerable(bool bOn)
{
	AWRCharacterBase* Character = GetCharacter();
	if (!Character || !IsServer() || bInvulnerable == bOn)
	{
		return;
	}
	bInvulnerable = bOn;
	UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
	if (bOn)
	{
		ASC->AddLooseGameplayTag(WRTags::State_Invulnerable);
	}
	else
	{
		ASC->RemoveLooseGameplayTag(WRTags::State_Invulnerable);
	}
}

void UWRGA_Dodge::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	SetInvulnerable(false);
	Travel = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// ---------------------------------------------------------------- guard / parry

UWRGA_Block::UWRGA_Block()
{
	DisplayName = TEXT("Guard");
	// The guard's tag is set here, not owned: a parry tap drops the guard at release while its recovery plays on.
	FGameplayTagContainer Blocked = ActivationBlockedTags;
	Blocked.AddTag(WRTags::State_Dodging);
	ActivationBlockedTags = Blocked;
	// From a swing's cancel window, the guard cuts it.
	CancelAbilitiesWithTag.AddTag(WRTags::Ability_Melee);
	bRules = true;
	RuleAct = WRCombatRules::EAct::Guard;
	FGameplayTagContainer Tags;
	Tags.AddTag(WRTags::Ability_Block);
	SetAssetTags(Tags);
}

void UWRGA_Block::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	AWRCharacterBase* Character = GetCharacter();
	const double Now = GetWorld()->GetTimeSeconds();
	// Guard is up from the press (a guard pressed early still blocks: COMBAT-AND-LESSONS L4).
	Character->SetCombatTag(WRTags::State_Blocking, true);
	SetMoveSlow(true, true);
	bRecovering = false;
	PressedAt = Now;

	// The parry window: the carried off-hand's (or the weapon's) startup and active; a press soon after the last one
	// shrinks it (Sekiro's spam rule).
	const FWRGuard Guard = FWRWeapons::Get().GuardFor(Character);
	const bool bSpam = Now - LastPressAt < WRTune(TEXT("defense.parrySpamWindow"), 0.5f);
	LastPressAt = Now;
	if (Guard.bCanParry && Guard.ParryActive > 0.f)
	{
		const float Active = Guard.ParryActive * (bSpam ? 1.f - WRTune(TEXT("defense.parrySpamShrink"), 0.3f) : 1.f);
		After(Guard.ParryStartup, [this]() { SetParrying(true); });
		After(Guard.ParryStartup + Active, [this]() { SetParrying(false); });
	}
	if (IsServer())
	{
		UE_LOG(LogWildreach, Verbose, TEXT("BLOCK %s raises the guard (%s)%s%s"), *GetNameSafe(Character), *Guard.Id.ToString(), bSpam ? TEXT(", parry shrunk") : TEXT(""),
			Character->HasTag(WRTags::State_Attacking) ? TEXT(" (mid-swing: up when it ends)") : TEXT(""));
	}

	if (UAbilityTask_WaitInputRelease* Wait = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true))
	{
		Wait->OnRelease.AddDynamic(this, &UWRGA_Block::OnReleased);
		Wait->ReadyForActivation();
	}
}

void UWRGA_Block::OnReleased(float TimeHeld)
{
	AWRCharacterBase* Character = GetCharacter();
	const FWRGuard Guard = FWRWeapons::Get().GuardFor(Character);
	const float Held = Character ? float(GetWorld()->GetTimeSeconds() - PressedAt) : TimeHeld;
	if (!Character || !Guard.bCanParry || Held >= WRTune(TEXT("defense.parryTapSeconds"), 0.3f))
	{
		EndSelf();  // a held guard: let go, nothing to pay
		return;
	}
	// A tap was a committed parry: the guard drops, the parry plays out and its recovery holds you (no guard, roll or
	// swing) until it's done. Spamming it is punished by the recovery and the shrink.
	bRecovering = true;
	Character->SetCombatTag(WRTags::State_Blocking, false);
	Character->SetCombatTag(WRTags::State_ParryRecovery, true);
	SetMoveSlow(true, false);
	const float Done = Guard.ParryStartup + Guard.ParryActive + Guard.ParryRecovery;
	After(FMath::Max(0.f, Done - Held), [this]() { EndSelf(); });
}

void UWRGA_Block::SetParrying(bool bOn)
{
	AWRCharacterBase* Character = GetCharacter();
	if (!Character || !IsServer() || bParrying == bOn)
	{
		return;
	}
	bParrying = bOn;
	if (bOn)
	{
		Character->GetAbilitySystemComponent()->AddLooseGameplayTag(WRTags::State_Parrying);
	}
	else
	{
		Character->GetAbilitySystemComponent()->RemoveLooseGameplayTag(WRTags::State_Parrying);
	}
}

void UWRGA_Block::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	AWRCharacterBase* Character = GetCharacter();
	if (IsServer() && IsActive())
	{
		UE_LOG(LogWildreach, Verbose, TEXT("BLOCK %s lowers the guard%s"), *GetNameSafe(Character), bRecovering ? TEXT(" (parry recovered)") : TEXT(""));
	}
	SetParrying(false);
	if (Character)
	{
		Character->SetCombatTag(WRTags::State_Blocking, false);
		Character->SetCombatTag(WRTags::State_ParryRecovery, false);
	}
	bRecovering = false;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// ---------------------------------------------------------------- critical attacks

UWRGA_Critical::UWRGA_Critical()
{
	DisplayName = TEXT("Critical");
	StaminaKey = TEXT("stamina.lightAttack");
	StaminaFallback = 10.f;
	ActivationOwnedTags.AddTag(WRTags::State_Critting);
	ActivationOwnedTags.AddTag(WRTags::State_Attacking);
	CancelAbilitiesWithTag.AddTag(WRTags::Ability_Melee);
	CancelAbilitiesWithTag.AddTag(WRTags::Ability_Block);
	bRules = true;
	RuleAct = WRCombatRules::EAct::Critical;
	FGameplayTagContainer Tags;
	Tags.AddTag(WRTags::Ability_Critical);
	SetAssetTags(Tags);
}

AWRCharacterBase* UWRGA_Critical::FindTarget(const AWRCharacterBase* Attacker, EKind& OutKind, bool bRewound)
{
	OutKind = EKind::None;
	if (!Attacker || !Attacker->IsAlive())
	{
		return nullptr;
	}
	const FWRTuning& T = FWRTuning::Get();
	UWRLagSubsystem* Lag = bRewound ? UWRLagSubsystem::Get(Attacker) : nullptr;
	const double SeenAt = Attacker->GetWorld()->GetTimeSeconds() - (Lag ? UWRLagSubsystem::SeenDelay(Attacker) : 0.f);
	const FVector Me = Attacker->GetActorLocation();
	const float MyRadius = Attacker->GetSimpleCollisionRadius();
	// Toward what the attacker faces or aims: no crit on something behind you.
	const FVector Aim = FRotator(0.f, Attacker->Controller ? Attacker->GetControlRotation().Yaw : Attacker->GetActorRotation().Yaw, 0.f).Vector();

	AWRCharacterBase* Best = nullptr;
	float BestGap = TNumericLimits<float>::Max();
	for (TActorIterator<AWRCharacterBase> It(Attacker->GetWorld()); It; ++It)
	{
		AWRCharacterBase* Who = *It;
		if (Who == Attacker || !Who->IsAlive() || !Attacker->IsHostileTo(Who) || Who->HasTag(WRTags::State_BeingCrit)
			|| Who->HasTag(WRTags::Status_Unconscious) || Who->HasTag(WRTags::State_Invulnerable)
			|| FVector::DistSquared2D(Who->GetActorLocation(), Me) > FMath::Square(900.f))
		{
			continue;
		}
		FVector Centre = Who->GetActorLocation();
		float Radius = Who->GetSimpleCollisionRadius(), HalfHeight = Who->GetSimpleCollisionHalfHeight();
		FRotator Facing = Who->GetActorRotation();
		if (Lag)
		{
			Lag->BodyAt(Who, SeenAt, Centre, Radius, HalfHeight, &Facing);
		}
		const FVector ToMe = (Me - Centre).GetSafeNormal2D();
		const float Gap = FVector::Dist2D(Me, Centre) - Radius - MyRadius;
		if (FVector::DotProduct(-ToMe, Aim) < FMath::Cos(FMath::DegreesToRadians(70.f)))
		{
			continue;
		}
		const float FromFront = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Facing.Vector().GetSafeNormal2D(), ToMe), -1.f, 1.f)));
		const bool bOpen = Who->HasTag(WRTags::Status_StanceBroken) || Who->HasTag(WRTags::Status_GuardBroken);
		const bool bBig = Radius > T.Num(TEXT("crit.bigRadius"), 150.f);
		// Riposte: an open target from the side its weak point faces.
		if (bOpen && FromFront <= T.Num(TEXT("crit.frontArc"), 60.f)
			&& Gap <= (bBig ? T.Num(TEXT("crit.stanceCritRange.big"), 150.f) : T.Num(TEXT("crit.stanceCritRange.normal"), 100.f)) && Gap < BestGap)
		{
			Best = Who;
			BestGap = Gap;
			OutKind = EKind::Riposte;
			continue;
		}
		// Backstab: from behind, a body no bigger than a person (anything, once broken), not winding up a swing.
		const bool bPlayer = Who->GetPlayerState() != nullptr;
		const float Arc = bPlayer ? T.Num(TEXT("crit.pvpBackstabArc"), 75.f) : T.Num(TEXT("crit.backstabArc"), 90.f);
		const bool bSmall = Radius <= T.Num(TEXT("crit.backstabMaxRadius"), 70.f) && HalfHeight <= T.Num(TEXT("crit.backstabMaxHalfHeight"), 110.f);
		const bool bBoss = Cast<AWRCreature>(Who) && Cast<AWRCreature>(Who)->GetDef() && Cast<AWRCreature>(Who)->GetDef()->Archetype == TEXT("boss");
		if (180.f - FromFront <= Arc * 0.5f && Gap <= T.Num(TEXT("crit.backstabRange"), 120.f) && ((bSmall && !bBoss) || bOpen)
			&& !Who->HasTag(WRTags::State_Windup) && Gap < BestGap)
		{
			Best = Who;
			BestGap = Gap;
			OutKind = EKind::Backstab;
		}
	}
	return Best;
}

void UWRGA_Critical::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AWRCharacterBase* Character = GetCharacter();
	// The server finds the target again on its own view of where the attacker saw it; no crit if there isn't one.
	EKind Found = EKind::None;
	AWRCharacterBase* Target = Character ? FindTarget(Character, Found, IsServer()) : nullptr;
	const FWRWeaponClass& Class = FWRWeapons::Get().ClassFor(Character);
	if (const FWRMove* Light = Class.Move(TEXT("light1")))
	{
		CostOverride = Light->Stamina;
	}
	if (!Target || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	Victim = Target;
	Kind = Found;
	const FWRTuning& T = FWRTuning::Get();
	const FWRMove* Row = Class.Move(Kind == EKind::Backstab ? FName(TEXT("backstab")) : FName(TEXT("riposte")));
	const float Total = Row && Row->Total > 0.f ? Row->Total : (Kind == EKind::Backstab ? T.Num(TEXT("crit.backstabPaired.total"), 1.4f) : T.Num(TEXT("crit.ripostePaired.total"), 1.6f));
	const float DamageAt = Row && Row->DamageAt > 0.f ? Row->DamageAt : (Kind == EKind::Backstab ? T.Num(TEXT("crit.backstabPaired.damageAt"), 0.75f) : T.Num(TEXT("crit.ripostePaired.damageAt"), 0.9f));

	// The sync point (COMBAT-AND-LESSONS 1.17): in front of its weak point for a riposte, behind it for a backstab, far
	// enough that the two bodies never overlap; the attacker warps there over warpSeconds and faces it.
	const FVector Facing = Target->GetActorForwardVector().GetSafeNormal2D();
	const float Apart = FMath::Max(T.Num(TEXT("crit.syncDistance"), 110.f), Target->GetSimpleCollisionRadius() + Character->GetSimpleCollisionRadius() + 20.f);
	const FVector Side = Kind == EKind::Backstab ? -Facing : Facing;
	FVector Spot = Target->GetActorLocation() + Side * Apart;
	Spot.Z = Character->GetActorLocation().Z;
	if (UAbilityTask_ApplyRootMotionMoveToForce* Warp = UAbilityTask_ApplyRootMotionMoveToForce::ApplyRootMotionMoveToForce(
		this, TEXT("CritWarp"), Spot, FMath::Max(0.05f, T.Num(TEXT("crit.warpSeconds"), 0.12f)), false, MOVE_Walking, false, nullptr,
		ERootMotionFinishVelocityMode::SetVelocity, FVector::ZeroVector, 0.f))
	{
		Warp->ReadyForActivation();
	}
	Character->SetActorRotation((-Side).Rotation());
	SetMoveRoot(true);
	// The stab: the heavy thrust timed so its blade arrives on the damage frame (weapons.json riposte / backstab clip).
	const float Strike = FMath::Min(0.2f, DamageAt * 0.25f);
	Character->PlaySwing(EWRSwingKind::Heavy, DamageAt + Strike, uint8(Row ? FMath::Clamp(Row->Clip, 0, 7) : 0), DamageAt, Strike, FMath::Max(0.05f, Total - DamageAt - Strike));

	if (IsServer())
	{
		SetInvulnerable(true);
		Hold(true, DamageAt + T.Num(TEXT("crit.holdAfter"), 0.3f));
		// The paired victim faces its killer for a riposte, away for a backstab.
		if (!Target->GetPlayerState() || Kind == EKind::Riposte)
		{
			Target->SetActorRotation((Kind == EKind::Backstab ? -Side : Side).Rotation());
		}
		UE_LOG(LogWildreach, Log, TEXT("CRIT %s %s on %s"), *Character->GetName(), Kind == EKind::Backstab ? TEXT("backstab") : TEXT("riposte"), *Target->GetName());
	}
	if (Character->IsLocallyControlled())
	{
		// The prompt has done its job for this species (COMBAT-AND-LESSONS L6): it won't show again.
		if (AWRCharacter* Hero = Cast<AWRCharacter>(Character))
		{
			Hero->NoteCritPromptSeen(Target);
		}
	}

	const EKind CritKind = Kind;
	After(DamageAt, [this, CritKind]()
	{
		AWRCharacterBase* Me = GetCharacter();
		AWRCharacterBase* Who = Victim.Get();
		SetInvulnerable(false);
		if (!IsServer() || !Me || !Who || !Who->IsAlive())
		{
			return;
		}
		const FWRTuning& Tune = FWRTuning::Get();
		const FWRWeaponClass& C = FWRWeapons::Get().ClassFor(Me);
		FWRHit Hit;
		Hit.Instigator = Me;
		Hit.Damage = UWRCombatLibrary::WeaponDamage(Me->Weapon, false) * (CritKind == EKind::Backstab ? C.CritBackstab : C.CritRiposte)
			* (Who->GetPlayerState() ? Tune.Num(TEXT("crit.pvpDamage"), 0.7f) : 1.f);
		Hit.Element = WRTags::Element_Physical;
		if (Me->Weapon.Element == TEXT("fire")) { Hit.Element = WRTags::Element_Fire; Hit.BurnBuildup = Tune.Num(TEXT("weapon.infusedBuildup"), 15.f); }
		else if (Me->Weapon.Element == TEXT("frost")) { Hit.Element = WRTags::Element_Frost; Hit.FrostBuildup = Tune.Num(TEXT("weapon.infusedBuildup"), 15.f); }
		Hit.StanceDamage = 0.f;
		Hit.PoiseDamage = 0.f;
		Hit.Impact = 3;
		Hit.Tier = EWRHitTier::Crit;
		Hit.bMelee = true;
		Hit.bHeavy = true;
		Hit.bCritical = true;
		Hit.bUnblockable = true;
		Hit.Origin = Me->GetActorLocation();
		UWRCombatLibrary::ApplyHit(Who, Hit);
		// The crit spends the opening; the stance comes back when the break ends (UWRStatusComponent).
		Who->Status->ClearStatus(WRTags::Status_GuardBroken);
		Who->Status->ClearStatus(WRTags::Status_StanceBroken);
		// Cold riposte (frost node): ripostes freeze.
		if (CritKind == EKind::Riposte && Who->IsAlive() && UWRPathComponent::HasNode(Me, TEXT("frost"), TEXT("cold_riposte")))
		{
			Who->Status->ApplyStatus(WRTags::Status_Frozen, Tune.Num(TEXT("status.frozenSeconds"), 2.5f));
		}
		// A player who survives it gets up untouchable for a moment (COMBAT-AND-LESSONS 1.18).
		if (Who->IsAlive() && Who->GetPlayerState())
		{
			Who->Status->ApplyStatus(WRTags::State_Invulnerable, Tune.Num(TEXT("crit.holdAfter"), 0.3f) + Tune.Num(TEXT("crit.riseIFrames"), 0.3f));
		}
	});
	After(DamageAt + T.Num(TEXT("crit.holdAfter"), 0.3f), [this]() { Hold(false, 0.f); });
	After(Total, [this]() { EndSelf(); });
}

void UWRGA_Critical::SetInvulnerable(bool bOn)
{
	AWRCharacterBase* Character = GetCharacter();
	if (!Character || !IsServer() || bInvulnerable == bOn)
	{
		return;
	}
	bInvulnerable = bOn;
	if (bOn) Character->GetAbilitySystemComponent()->AddLooseGameplayTag(WRTags::State_Invulnerable);
	else Character->GetAbilitySystemComponent()->RemoveLooseGameplayTag(WRTags::State_Invulnerable);
}

void UWRGA_Critical::Hold(bool bOn, float Seconds)
{
	AWRCharacterBase* Who = Victim.Get();
	if (!Who || !IsServer() || bHolding == bOn)
	{
		return;
	}
	bHolding = bOn;
	if (bOn)
	{
		// Held through the stab: no swing, no step, its AI waits (Staggered stops creatures; BeingCrit stops everything).
		Who->SetCombatTag(WRTags::State_BeingCrit, true);
		Who->Status->Stagger(FMath::Max(0.1f, Seconds));
	}
	else
	{
		Who->SetCombatTag(WRTags::State_BeingCrit, false);
	}
}

void UWRGA_Critical::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	SetInvulnerable(false);
	Hold(false, 0.f);
	Victim = nullptr;
	Kind = EKind::None;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// ---------------------------------------------------------------- spells

UWRGA_Spell::UWRGA_Spell()
{
	StaminaKey = TEXT("stamina.spell");
	StaminaFallback = 12.f;
	ActivationOwnedTags.AddTag(WRTags::State_Attacking);
	FGameplayTagContainer Blocked = ActivationBlockedTags;
	Blocked.AddTag(WRTags::State_Attacking);
	Blocked.AddTag(WRTags::State_Dodging);
	ActivationBlockedTags = Blocked;
}

void UWRGA_Spell::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	AWRCharacterBase* Character = GetCharacter();
	Character->PlaySwing(EWRSwingKind::Cast, 0.25f);
	SetMoveSlow(false, true);
	if (IsServer())
	{
		const bool bFire = Element == WRTags::Element_Fire;
		WRFx::BroadcastOn(Character, bFire ? TEXT("cast.fire") : TEXT("cast.frost"), TEXT("hand"), 1.f, bFire ? TEXT("cast.fire") : TEXT("cast.frost"), 0.4f);
	}

	After(0.15f, [this]()
	{
		AWRCharacterBase* Caster = GetCharacter();
		if (!Caster || !IsServer())
		{
			return;
		}
		const FWRTuning& T = FWRTuning::Get();
		const float Speed = T.Num(SpellKey + TEXT(".speed"), 2600.f);
		// At what you've locked on to, ahead of where it's going; otherwise through the crosshair.
		AWRCharacterBase* Lock = WRSpellAim::Locked(Caster);
		const FVector Chest = Caster->GetActorLocation() + FVector(0.f, 0.f, 40.f);
		const FVector Dir = Lock ? WRSpellAim::Leading(Chest, Lock, Speed) : WRSpellAim::ThroughCrosshair(Caster, Chest);
		const FRotator Aim = Dir.Rotation();
		const FVector SpawnAt = Chest + Dir * 70.f;

		FWRHit Hit;
		Hit.Instigator = Caster;
		Hit.Damage = T.Num(SpellKey + TEXT(".damage"), 15.f);
		Hit.PoiseDamage = 8.f;
		Hit.Element = Element;
		Hit.BurnBuildup = T.Num(SpellKey + TEXT(".burnBuildup"), 0.f);
		Hit.FrostBuildup = T.Num(SpellKey + TEXT(".frostBuildup"), 0.f);
		Hit.PathId = PathId;

		// Point blank: a target between the caster and the spawn point would be spawned inside
		// and never get a hit event, so resolve it directly.
		FHitResult Near;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(WRSpellMuzzle), false, Caster);
		const FVector From = Caster->GetActorLocation() + FVector(0.f, 0.f, 40.f);
		if (Caster->GetWorld()->SweepSingleByChannel(Near, From, SpawnAt, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(14.f), Query))
		{
			AWRCharacterBase* Blocker = Cast<AWRCharacterBase>(Near.GetActor());
			if (Blocker && Caster->IsHostileTo(Blocker))
			{
				Hit.Origin = From;
				UWRCombatLibrary::ApplyHit(Blocker, Hit);
				UWRCombatLibrary::GainResource(Caster, WRTags::Element_Fire, T.Num(SpellKey + TEXT(".heat"), 0.f));
				UWRCombatLibrary::GainResource(Caster, WRTags::Element_Frost, T.Num(SpellKey + TEXT(".chill"), 0.f));
				return;
			}
		}

		FActorSpawnParameters Params;
		Params.Owner = Caster;
		Params.Instigator = Caster;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AWRProjectile* Projectile = Caster->GetWorld()->SpawnActor<AWRProjectile>(AWRProjectile::StaticClass(), SpawnAt, Aim, Params))
		{
			Projectile->Init(Hit, Speed, Color);
			if (Lock)
			{
				// A gentle curve for what the lead can't know: it turned. Other players get less of it, so they can still dodge.
				Projectile->HomeOn(Lock, Lock->IsCreature() ? T.Num(TEXT("spells.lockHoming"), 6000.f) : T.Num(TEXT("spells.lockHomingPlayers"), 2000.f));
			}
		}
		UWRCombatLibrary::GainResource(Caster, WRTags::Element_Fire, T.Num(SpellKey + TEXT(".heat"), 0.f));
		UWRCombatLibrary::GainResource(Caster, WRTags::Element_Frost, T.Num(SpellKey + TEXT(".chill"), 0.f));
	});
	After(0.35f, [this]() { EndSelf(); });
}

UWRGA_FireBolt::UWRGA_FireBolt()
{
	DisplayName = TEXT("Fire bolt");
	PathId = TEXT("fire");
	NodeId = TEXT("fire_bolt");
	SpellKey = TEXT("spells.fireBolt");
	CooldownKey = TEXT("spells.fireBolt.cooldown");
	CooldownFallback = 1.2f;
	Element = WRTags::Element_Fire;
	Color = FLinearColor(1.f, 0.35f, 0.05f);
}

UWRGA_FrostShard::UWRGA_FrostShard()
{
	DisplayName = TEXT("Frost shard");
	PathId = TEXT("frost");
	NodeId = TEXT("frost_shard");
	SpellKey = TEXT("spells.frostShard");
	CooldownKey = TEXT("spells.frostShard.cooldown");
	CooldownFallback = 1.f;
	Element = WRTags::Element_Frost;
	Color = FLinearColor(0.5f, 0.85f, 1.f);
}

// ---------------------------------------------------------------- area spells

void UWRGA_AreaSpell::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	GetCharacter()->PlaySwing(EWRSwingKind::Cast, 0.35f);
	SetMoveSlow(false, true);
	if (IsServer())
	{
		const bool bFire = Element == WRTags::Element_Fire;
		WRFx::BroadcastOn(GetCharacter(), bFire ? TEXT("cast.fire") : TEXT("cast.frost"), TEXT("hand"), 1.4f, bFire ? TEXT("cast.fire") : TEXT("cast.frost"), 0.5f);
	}
	After(0.25f, [this]() { if (IsServer()) Execute(); });
	After(0.45f, [this]() { EndSelf(); });
}

void UWRGA_AreaSpell::Execute()
{
	AWRCharacterBase* Caster = GetCharacter();
	if (!Caster)
	{
		return;
	}
	UWorld* World = Caster->GetWorld();
	const FWRTuning& T = FWRTuning::Get();
	const bool bFire = Element == WRTags::Element_Fire;
	// Shockwave (fire, "bigger blast radius") and Wide storm (frost, "larger area effects"): every radius, width and
	// spacing of an area spell grows by the node's stat (tuning nodeEffects).
	const float AreaScale = 1.f + UWRPathComponent::Stat(Caster, bFire ? TEXT("blastRadius") : TEXT("areaRadius"));
	const auto Num = [&](const TCHAR* Key, float Fallback)
	{
		const float V = T.Num(SpellKey + TEXT(".") + Key, Fallback);
		const FString K(Key);
		return K == TEXT("radius") || K == TEXT("width") || K == TEXT("spacing") ? V * AreaScale : V;
	};
	const FRotator Aim = Caster->GetBaseAimRotation();
	const FVector Flat = FRotator(0.f, Aim.Yaw, 0.f).Vector();
	const FVector Feet = Caster->GetActorLocation() - FVector(0.f, 0.f, Caster->GetSimpleCollisionHalfHeight());
	const auto Ground = [World, Caster](const FVector& At)
	{
		FHitResult Hit;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(WRSpellGround), false, Caster);
		return World->LineTraceSingleByChannel(Hit, At + FVector(0, 0, 500), At - FVector(0, 0, 2000), ECC_WorldStatic, Query) ? Hit.ImpactPoint : At;
	};
	// Where a spell that lands on the ground goes: under what you've locked on to (Lead seconds ahead of it, for one that
	// takes that long to land), else where the crosshair meets the ground, else out in front.
	const auto Aimed = [&](float Range, float Lead, float MissShare) -> FVector
	{
		if (const AWRCharacterBase* Lock = WRSpellAim::Locked(Caster))
		{
			const FVector Next = Lock->GetActorLocation() + (WRTune(TEXT("spells.lockLead"), 1.f) > 0.f ? Lock->GetVelocity() * Lead : FVector::ZeroVector);
			const FVector Off = Next - Feet;
			return Ground(FVector(Feet.X, Feet.Y, Next.Z) + FVector(Off.X, Off.Y, 0.f).GetClampedToMaxSize(Range));
		}
		const AWRCharacter* Survivor = Cast<AWRCharacter>(Caster);
		FVector Point;
		return Survivor && Survivor->CrosshairPoint(Range, false, Point) ? Point : Ground(Feet + Flat * Range * MissShare);
	};
	const auto Strike = [&](AWRCharacterBase* Target, const FVector& Origin)
	{
		FWRHit Hit;
		Hit.Instigator = Caster;
		Hit.Damage = Num(TEXT("damage"), 15.f);
		Hit.PoiseDamage = Num(TEXT("poise"), 12.f);
		Hit.Element = Element;
		(bFire ? Hit.BurnBuildup : Hit.FrostBuildup) = Num(bFire ? TEXT("burnBuildup") : TEXT("frostBuildup"), 40.f);
		Hit.PathId = PathId;
		Hit.Origin = Origin;
		UWRCombatLibrary::ApplyHit(Target, Hit);
	};

	switch (Shape)
	{
	case EWRAreaShape::Wall:
	{
		// A line of fire across your aim; enemies crossing it burn.
		const FVector Center = Feet + Flat * Num(TEXT("distance"), 600.f);
		const FVector Across = FVector::CrossProduct(Flat, FVector::UpVector);
		const int32 Patches = FMath::RoundToInt(Num(TEXT("patches"), 5.f));
		const float Width = Num(TEXT("width"), 800.f);
		for (int32 i = 0; i < Patches; ++i)
		{
			const FVector At = Center + Across * (Width * (i / FMath::Max(1.f, Patches - 1.f) - 0.5f));
			AWRHazard::Spawn(World, Caster, Ground(At), Width / Patches * 0.7f, Num(TEXT("seconds"), 6.f), Num(TEXT("dps"), 10.f), TEXT("fire"), false);
			WRFx::Broadcast(Caster, TEXT("firewall"), Ground(At), FRotator(0.f, Aim.Yaw, 0.f), Width / Patches * 0.7f / 100.f, i == Patches / 2 ? FName(TEXT("firewall")) : NAME_None, Num(TEXT("seconds"), 6.f));
		}
		break;
	}
	case EWRAreaShape::Pool:
	{
		// Lava where you aim (up to range), on the ground.
		const FVector At = Aimed(Num(TEXT("range"), 1500.f), 0.f, 0.7f);
		AWRHazard::Spawn(World, Caster, At, Num(TEXT("radius"), 300.f), Num(TEXT("seconds"), 8.f), Num(TEXT("dps"), 12.f), TEXT("lava"), false);
		WRFx::Broadcast(Caster, TEXT("lavapool"), At, FRotator::ZeroRotator, Num(TEXT("radius"), 300.f) / 100.f, TEXT("lavapool"), Num(TEXT("seconds"), 8.f));
		break;
	}
	case EWRAreaShape::Nova:
	{
		const float Radius = Num(TEXT("radius"), 500.f);
		// (The frost nova effect shows the ring it reaches; no flat marker disc.)
		WRFx::Broadcast(Caster, TEXT("frostnova"), Feet, FRotator::ZeroRotator, Radius / 100.f, TEXT("frostnova"));
		for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
		{
			if (*It != Caster && It->IsAlive() && Caster->IsHostileTo(*It) && FVector::Dist(It->GetActorLocation(), Caster->GetActorLocation()) < Radius + It->GetSimpleCollisionRadius())
			{
				Strike(*It, Caster->GetActorLocation());
			}
		}
		break;
	}
	case EWRAreaShape::Meteor:
	{
		// A telegraphed strike where you aim: lands after a moment, leaves fire behind.
		const float Radius = Num(TEXT("radius"), 450.f), Delay = Num(TEXT("delay"), 1.2f);
		const FVector At = Aimed(Num(TEXT("range"), 2500.f), Delay, 0.6f);
		AWRGroundMarker::Spawn(World, At + FVector(0, 0, 3), FRotator::ZeroRotator, FVector2D(Radius, Radius), false, FLinearColor(1.f, 0.15f, 0.05f), Delay);
		TWeakObjectPtr<AWRCharacterBase> Weak = Caster;
		const float Damage = Num(TEXT("damage"), 55.f), Burn = Num(TEXT("burnBuildup"), 70.f);
		const FName Path = PathId;
		FTimerHandle Timer;
		World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateLambda([Weak, At, Radius, Damage, Burn, Path]()
		{
			AWRCharacterBase* C = Weak.Get();
			if (!C) return;
			for (TActorIterator<AWRCharacterBase> It(C->GetWorld()); It; ++It)
			{
				if (*It != C && It->IsAlive() && C->IsHostileTo(*It) && FVector::Dist2D(It->GetActorLocation(), At) < Radius + It->GetSimpleCollisionRadius())
				{
					FWRHit H;
					H.Instigator = C; H.Damage = Damage; H.PoiseDamage = 40.f; H.Element = WRTags::Element_Fire; H.BurnBuildup = Burn; H.PathId = Path; H.bHeavy = true; H.Origin = At;
					UWRCombatLibrary::ApplyHit(*It, H);
				}
			}
			AWRHazard::Spawn(C->GetWorld(), C, At, Radius * 0.6f, 5.f, 8.f, TEXT("fire"), false);
			WRFx::Broadcast(C, TEXT("fire.impact"), At, FRotator(90.f, 0.f, 0.f), 3.f, TEXT("fire.impact"));
			WRFx::Broadcast(C, TEXT("firewall"), At, FRotator::ZeroRotator, Radius * 0.6f / 100.f, NAME_None, 5.f);
		}), Delay, false);
		break;
	}
	case EWRAreaShape::IceWall:
	{
		// Pillars of ice across your aim: they block movement, attacks and breath, then melt.
		const FVector Center = Feet + Flat * Num(TEXT("distance"), 500.f);
		const FVector Across = FVector::CrossProduct(Flat, FVector::UpVector);
		const int32 Count = FMath::RoundToInt(Num(TEXT("pillars"), 5.f));
		const float Spacing = Num(TEXT("spacing"), 200.f);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		for (int32 i = 0; i < Count; ++i)
		{
			const FVector At = Ground(Center + Across * Spacing * (i - (Count - 1) * 0.5f)) + FVector(0, 0, 200.f);
			if (AWRBossProp* Pillar = World->SpawnActor<AWRBossProp>(AWRBossProp::StaticClass(), At, FRotator::ZeroRotator, Params))
			{
				Pillar->Setup(TEXT("pillar"));
				Pillar->SetActorScale3D(FVector(0.8f, 0.8f, 0.5f));
				Pillar->SetLifeSpan(Num(TEXT("seconds"), 8.f));
			}
			WRFx::Broadcast(Caster, TEXT("frost.impact"), At - FVector(0, 0, 180.f), FRotator(90.f, 0.f, 0.f), 1.5f, i == 0 ? FName(TEXT("frost.impact")) : NAME_None);
		}
		break;
	}
	case EWRAreaShape::Storm:
		// A lasting storm around where you cast: frost buildup and slow on enemies inside.
		AWRHazard::Spawn(World, Caster, Feet, Num(TEXT("radius"), 650.f), Num(TEXT("seconds"), 8.f), Num(TEXT("dps"), 6.f), TEXT("frost"), false);
		WRFx::Broadcast(Caster, TEXT("blizzard"), Feet, FRotator::ZeroRotator, Num(TEXT("radius"), 650.f) / 100.f, TEXT("frostnova"), Num(TEXT("seconds"), 8.f));
		break;
	case EWRAreaShape::Lance:
	{
		// Pierces everything along the aim.
		const float Range = Num(TEXT("range"), 2000.f);
		const FVector From = Caster->GetActorLocation() + FVector(0, 0, 40);
		TArray<FHitResult> Hits;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(WRIceLance), false, Caster);
		World->SweepMultiByChannel(Hits, From, From + Aim.Vector() * Range, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(45.f), Query);
		TSet<AActor*> Done;
		for (const FHitResult& H : Hits)
		{
			AWRCharacterBase* Target = Cast<AWRCharacterBase>(H.GetActor());
			if (Target && !Done.Contains(Target) && Target->IsAlive() && Caster->IsHostileTo(Target))
			{
				Done.Add(Target);
				Strike(Target, From);
			}
		}
		WRFx::Broadcast(Caster, TEXT("icelance"), From, Aim, Range / 100.f, TEXT("icelance"));
		break;
	}
	}
	UWRCombatLibrary::GainResource(Caster, WRTags::Element_Fire, Num(TEXT("heat"), 0.f));
	UWRCombatLibrary::GainResource(Caster, WRTags::Element_Frost, Num(TEXT("chill"), 0.f));
	if (UWRPathComponent* Paths = UWRPathComponent::FindFor(Caster))
	{
		Paths->AwardXP(PathId, FMath::RoundToInt(WRTune(TEXT("paths.xpPerAbilityUse"), 6.f)));
	}
}

UWRGA_FireWall::UWRGA_FireWall()
{
	DisplayName = TEXT("Fire wall"); PathId = TEXT("fire"); NodeId = TEXT("fire_wall");
	SpellKey = TEXT("spells.fireWall"); CooldownKey = TEXT("spells.fireWall.cooldown"); CooldownFallback = 10.f;
	Shape = EWRAreaShape::Wall; Element = WRTags::Element_Fire;
}

UWRGA_LavaPool::UWRGA_LavaPool()
{
	DisplayName = TEXT("Lava pool"); PathId = TEXT("fire"); NodeId = TEXT("lava_pool");
	SpellKey = TEXT("spells.lavaPool"); CooldownKey = TEXT("spells.lavaPool.cooldown"); CooldownFallback = 12.f;
	Shape = EWRAreaShape::Pool; Element = WRTags::Element_Fire;
}

UWRGA_FrostNova::UWRGA_FrostNova()
{
	DisplayName = TEXT("Frost nova"); PathId = TEXT("frost"); NodeId = TEXT("frost_nova");
	SpellKey = TEXT("spells.frostNova"); CooldownKey = TEXT("spells.frostNova.cooldown"); CooldownFallback = 8.f;
	Shape = EWRAreaShape::Nova; Element = WRTags::Element_Frost;
}

UWRGA_IceLance::UWRGA_IceLance()
{
	DisplayName = TEXT("Ice lance"); PathId = TEXT("frost"); NodeId = TEXT("ice_lance");
	SpellKey = TEXT("spells.iceLance"); CooldownKey = TEXT("spells.iceLance.cooldown"); CooldownFallback = 6.f;
	Shape = EWRAreaShape::Lance; Element = WRTags::Element_Frost;
}

UWRGA_Meteor::UWRGA_Meteor()
{
	DisplayName = TEXT("Meteor"); PathId = TEXT("fire"); NodeId = TEXT("meteor");
	SpellKey = TEXT("spells.meteor"); CooldownKey = TEXT("spells.meteor.cooldown"); CooldownFallback = 30.f;
	Shape = EWRAreaShape::Meteor; Element = WRTags::Element_Fire;
}

UWRGA_IceWall::UWRGA_IceWall()
{
	DisplayName = TEXT("Ice wall"); PathId = TEXT("frost"); NodeId = TEXT("ice_wall");
	SpellKey = TEXT("spells.iceWall"); CooldownKey = TEXT("spells.iceWall.cooldown"); CooldownFallback = 14.f;
	Shape = EWRAreaShape::IceWall; Element = WRTags::Element_Frost;
}

UWRGA_Blizzard::UWRGA_Blizzard()
{
	DisplayName = TEXT("Blizzard"); PathId = TEXT("frost"); NodeId = TEXT("blizzard");
	SpellKey = TEXT("spells.blizzard"); CooldownKey = TEXT("spells.blizzard.cooldown"); CooldownFallback = 30.f;
	Shape = EWRAreaShape::Storm; Element = WRTags::Element_Frost;
}

// ---------------------------------------------------------------- registry

bool UWRAbilityRegistry::IsBuilt(FName PathId, FName NodeId)
{
	static const TSet<FName> Passives = {
		TEXT("spreading_flames"), TEXT("fuel_the_fire"), TEXT("wildfire"), TEXT("explosive_bolt"), TEXT("searing_parry"),
		TEXT("brittle"), TEXT("cold_return"), TEXT("chain_shatter"), TEXT("cold_riposte"), TEXT("frozen_moment"), TEXT("overheat"), TEXT("whiteout") };
	return ClassForNode(PathId, NodeId) != nullptr || Passives.Contains(NodeId);
}

TSubclassOf<UGameplayAbility> UWRAbilityRegistry::ClassForNode(FName PathId, FName NodeId)
{
	if (PathId == TEXT("fire"))
	{
		if (NodeId == TEXT("fire_bolt")) return UWRGA_FireBolt::StaticClass();
		if (NodeId == TEXT("flame_slash")) return UWRGA_FlameSlash::StaticClass();
		if (NodeId == TEXT("fire_wall")) return UWRGA_FireWall::StaticClass();
		if (NodeId == TEXT("lava_pool")) return UWRGA_LavaPool::StaticClass();
		if (NodeId == TEXT("molten_edge")) return UWRGA_MoltenEdge::StaticClass();
		if (NodeId == TEXT("molten_cleave")) return UWRGA_MoltenCleave::StaticClass();
		if (NodeId == TEXT("meteor")) return UWRGA_Meteor::StaticClass();
	}
	else if (PathId == TEXT("frost"))
	{
		if (NodeId == TEXT("frost_shard")) return UWRGA_FrostShard::StaticClass();
		if (NodeId == TEXT("ice_slash")) return UWRGA_IceSlash::StaticClass();
		if (NodeId == TEXT("frost_nova")) return UWRGA_FrostNova::StaticClass();
		if (NodeId == TEXT("ice_lance")) return UWRGA_IceLance::StaticClass();
		if (NodeId == TEXT("flurry")) return UWRGA_Flurry::StaticClass();
		if (NodeId == TEXT("ice_wall")) return UWRGA_IceWall::StaticClass();
		if (NodeId == TEXT("blizzard")) return UWRGA_Blizzard::StaticClass();
	}
	return nullptr;
}
