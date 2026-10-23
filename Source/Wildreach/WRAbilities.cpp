#include "WRAbilities.h"
#include "Wildreach.h"
#include "WRAttributeSet.h"
#include "WRCharacterBase.h"
#include "WRCharacterMovementComponent.h"
#include "WRCombatLibrary.h"
#include "WRGameplayTags.h"
#include "WRPathComponent.h"
#include "WRProjectile.h"
#include "WRStatusComponent.h"
#include "WRTuning.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

// ---------------------------------------------------------------- base

UWRGameplayAbility::UWRGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer Blocked;
	Blocked.AddTag(WRTags::State_Dead);
	Blocked.AddTag(WRTags::Status_Frozen);
	Blocked.AddTag(WRTags::Status_Staggered);
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

bool UWRGameplayAbility::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (StaminaKey.IsEmpty())
	{
		return true;
	}
	const AWRCharacterBase* Character = ActorInfo ? Cast<AWRCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	// Souls-style: any stamina left lets you act; the cost can take you to zero.
	return Character && Character->GetAttributes() && Character->GetAttributes()->GetStamina() >= 1.f;
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
		const float Cost = FWRTuning::Get().Num(StaminaKey, StaminaFallback);
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
	FTimerHandle& Handle = Timers.AddDefaulted_GetRef();
	World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, MoveTemp(Fn)), Delay, false);
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

void UWRGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bAppliedAttackSlow) SetMoveSlow(false, false);
	if (bAppliedBlockSlow) SetMoveSlow(true, false);
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

// ---------------------------------------------------------------- melee

UWRGA_Melee::UWRGA_Melee()
{
	ActivationOwnedTags.AddTag(WRTags::State_Attacking);
	FGameplayTagContainer Blocked = ActivationBlockedTags;
	Blocked.AddTag(WRTags::State_Attacking);
	Blocked.AddTag(WRTags::State_Dodging);
	Blocked.AddTag(WRTags::State_Blocking);
	ActivationBlockedTags = Blocked;
	Element = WRTags::Element_Physical;
}

void UWRGA_Melee::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	AWRCharacterBase* Character = GetCharacter();
	if (!Character)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Commit to the aim direction at the start of the swing.
	if (Character->Controller)
	{
		Character->SetActorRotation(FRotator(0.f, Character->GetControlRotation().Yaw, 0.f));
	}

	const FWRTuning& T = FWRTuning::Get();
	const float Windup = T.Num(TimingKey + TEXT(".windup"), 0.25f);
	const float Active = T.Num(TimingKey + TEXT(".active"), 0.18f);
	const float Recovery = T.Num(TimingKey + TEXT(".recovery"), 0.3f);

	Character->PlaySwing(SwingKind, Windup + Active);
	SetMoveSlow(false, true);

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

void UWRGA_Melee::Sweep()
{
	AWRCharacterBase* Character = GetCharacter();
	if (!Character || !IsServer())
	{
		return;
	}
	const FWRTuning& T = FWRTuning::Get();
	const float Reach = T.Num(TEXT("weapon.reach"), 220.f);
	const float Radius = T.Num(TEXT("weapon.sweepRadius"), 45.f);
	const FVector Start = Character->GetActorLocation();
	const FVector End = Start + Character->GetActorForwardVector() * Reach;

	TArray<FHitResult> Hits;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(WRMelee), false, Character);
	Character->GetWorld()->SweepMultiByChannel(Hits, Start, End, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(Radius), Params);

	for (const FHitResult& HitResult : Hits)
	{
		AWRCharacterBase* Target = Cast<AWRCharacterBase>(HitResult.GetActor());
		if (!Target || HitThisSwing.Contains(Target) || !Character->IsHostileTo(Target))
		{
			continue;
		}
		HitThisSwing.Add(Target);

		float Mult = DamageMultiplier;
		float Burn = BurnBuildup;
		float Frost = FrostBuildup;
		if (!ArtKey.IsEmpty())
		{
			Mult = T.Num(ArtKey + TEXT(".multiplier"), Mult);
			Burn = T.Num(ArtKey + TEXT(".burnBuildup"), Burn);
			Frost = T.Num(ArtKey + TEXT(".frostBuildup"), Frost);
		}

		FWRHit Hit;
		Hit.Instigator = Character;
		Hit.Damage = UWRCombatLibrary::WeaponDamage(Character->Weapon, bHeavy) * Mult;
		Hit.PoiseDamage = bHeavy ? T.Num(TEXT("weapon.heavyPoiseDamage"), 25.f) : T.Num(TEXT("weapon.lightPoiseDamage"), 10.f);
		Hit.Element = Element;
		Hit.BurnBuildup = Burn;
		Hit.FrostBuildup = Frost;
		Hit.PathId = PathId;
		Hit.bMelee = true;
		Hit.bHeavy = bHeavy;
		Hit.bWeaponArt = !ArtKey.IsEmpty();
		Hit.Origin = Character->GetActorLocation();
		UWRCombatLibrary::ApplyHit(Target, Hit);
	}
}

UWRGA_LightAttack::UWRGA_LightAttack()
{
	DisplayName = TEXT("Light attack");
	StaminaKey = TEXT("stamina.lightAttack");
	StaminaFallback = 10.f;
	TimingKey = TEXT("weapon.lightTiming");
	SwingKind = EWRSwingKind::Light;
	FGameplayTagContainer Tags;
	Tags.AddTag(WRTags::Ability_Light);
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
	FGameplayTagContainer Tags;
	Tags.AddTag(WRTags::Ability_Heavy);
	SetAssetTags(Tags);
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
}

UWRGA_Bite::UWRGA_Bite()
{
	DisplayName = TEXT("Bite");
	TimingKey = TEXT("creature.biteTiming");
	SwingKind = EWRSwingKind::Bite;
	CooldownFallback = 1.2f;
}

// ---------------------------------------------------------------- dodge

UWRGA_Dodge::UWRGA_Dodge()
{
	DisplayName = TEXT("Dodge");
	StaminaKey = TEXT("stamina.dodge");
	StaminaFallback = 15.f;
	ActivationOwnedTags.AddTag(WRTags::State_Dodging);
	FGameplayTagContainer Blocked = ActivationBlockedTags;
	Blocked.AddTag(WRTags::State_Dodging);
	ActivationBlockedTags = Blocked;
	// A dodge interrupts a block (but not an attack already swinging).
	CancelAbilitiesWithTag.AddTag(WRTags::Ability_Block);
	FGameplayTagContainer Tags;
	Tags.AddTag(WRTags::Ability_Dodge);
	SetAssetTags(Tags);
}

void UWRGA_Dodge::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AWRCharacterBase* Character = GetCharacter();
	if (!Character || Character->GetCharacterMovement()->IsFalling() || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FWRTuning& T = FWRTuning::Get();
	const float Distance = T.Num(TEXT("defense.dodgeDistance"), 450.f);
	// Light step: same distance, quicker roll. Progress replicates to the owner, so client and server agree.
	const float Duration = T.Num(TEXT("defense.dodgeDuration"), 0.45f) / (1.f + UWRPathComponent::Stat(Character, TEXT("dodgeSpeed")));
	const float IStart = T.Num(TEXT("defense.dodgeIFrameStart"), 0.05f);
	const float IFrames = T.Num(TEXT("defense.dodgeIFrames"), 0.3f);

	// Roll toward the input direction; with no input, step back.
	FVector Dir = Character->GetCharacterMovement()->GetCurrentAcceleration().GetSafeNormal2D();
	if (Dir.IsNearlyZero())
	{
		Dir = -Character->GetActorForwardVector().GetSafeNormal2D();
	}

	UAbilityTask_ApplyRootMotionConstantForce* Task = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
		this, TEXT("Dodge"), Dir, Distance / Duration, Duration, false, nullptr,
		ERootMotionFinishVelocityMode::ClampVelocity, FVector::ZeroVector, Character->GetCharacterMovement()->GetMaxSpeed(), true);
	if (Task)
	{
		Task->ReadyForActivation();
	}

	After(IStart, [this]() { SetInvulnerable(true); });
	After(IStart + IFrames, [this]() { SetInvulnerable(false); });
	After(Duration, [this]() { EndSelf(); });
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
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// ---------------------------------------------------------------- block / parry

UWRGA_Block::UWRGA_Block()
{
	DisplayName = TEXT("Block");
	CooldownFallback = 0.35f;
	ActivationOwnedTags.AddTag(WRTags::State_Blocking);
	FGameplayTagContainer Blocked = ActivationBlockedTags;
	Blocked.AddTag(WRTags::State_Attacking);
	Blocked.AddTag(WRTags::State_Dodging);
	ActivationBlockedTags = Blocked;
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
	SetParrying(true);
	SetMoveSlow(true, true);
	After(WRTune(TEXT("defense.parryWindow"), 0.2f), [this]() { SetParrying(false); });

	if (UAbilityTask_WaitInputRelease* Wait = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true))
	{
		Wait->OnRelease.AddDynamic(this, &UWRGA_Block::OnReleased);
		Wait->ReadyForActivation();
	}
}

void UWRGA_Block::OnReleased(float TimeHeld)
{
	EndSelf();
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
	SetParrying(false);
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

	After(0.15f, [this]()
	{
		AWRCharacterBase* Caster = GetCharacter();
		if (!Caster || !IsServer())
		{
			return;
		}
		const FWRTuning& T = FWRTuning::Get();
		const FRotator Aim = Caster->GetBaseAimRotation();
		const FVector Dir = Aim.Vector();
		const FVector SpawnAt = Caster->GetActorLocation() + FVector(0.f, 0.f, 40.f) + Dir * 70.f;

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
			Projectile->Init(Hit, T.Num(SpellKey + TEXT(".speed"), 2600.f), Color);
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

// ---------------------------------------------------------------- registry

TSubclassOf<UGameplayAbility> UWRAbilityRegistry::ClassForNode(FName PathId, FName NodeId)
{
	if (PathId == TEXT("fire"))
	{
		if (NodeId == TEXT("fire_bolt")) return UWRGA_FireBolt::StaticClass();
		if (NodeId == TEXT("flame_slash")) return UWRGA_FlameSlash::StaticClass();
	}
	else if (PathId == TEXT("frost"))
	{
		if (NodeId == TEXT("frost_shard")) return UWRGA_FrostShard::StaticClass();
		if (NodeId == TEXT("ice_slash")) return UWRGA_IceSlash::StaticClass();
	}
	return nullptr;
}
