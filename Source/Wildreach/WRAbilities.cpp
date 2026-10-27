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
#include "GameFramework/PlayerState.h"
#include "WRStatusComponent.h"
#include "WRTuning.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "EngineUtils.h"

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
	// Not State_Blocking: a swing comes straight out of a held block. The guard drops while you swing (a block only
	// counts when you aren't attacking, WRCombatLibrary) and is back up once the swing ends if block is still held.
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

	// Swinging out of a held block (survivors): log the guard dropping and coming back, for the bot tests.
	bFromGuard = IsServer() && !Character->IsCreature() && Character->HasTag(WRTags::State_Blocking);
	if (bFromGuard)
	{
		UE_LOG(LogWildreach, Log, TEXT("BLOCK %s swings from the guard (%s): the guard is down for %.2f s"), *Character->GetName(), *GetClass()->GetName(), Windup + Active + Recovery);
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
	After(Windup + Active + Recovery, [this]()
	{
		AWRCharacterBase* Self = GetCharacter();
		if (bFromGuard && Self)
		{
			UE_LOG(LogWildreach, Log, TEXT("BLOCK %s swing done: %s"), *Self->GetName(),
				Self->HasTag(WRTags::State_Blocking) ? TEXT("block still held, guard back up") : TEXT("block let go"));
		}
		bFromGuard = false;
		EndSelf();
	});
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
	AWRDressing* Dressing = UWRDressingSubsystem::FindDressing(Character->GetWorld());

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

UWRGA_MoltenEdge::UWRGA_MoltenEdge()
{
	DisplayName = TEXT("Molten edge"); PathId = TEXT("fire"); NodeId = TEXT("molten_edge");
	StaminaKey = TEXT("stamina.weaponArt"); StaminaFallback = 14.f;
	CooldownKey = TEXT("spells.moltenEdge.cooldown"); CooldownFallback = 2.f;
	TimingKey = TEXT("weapon.lightTiming"); SwingKind = EWRSwingKind::WeaponArt;
	Element = WRTags::Element_Fire; ArtKey = TEXT("spells.moltenEdge");
}

UWRGA_Flurry::UWRGA_Flurry()
{
	DisplayName = TEXT("Flurry"); PathId = TEXT("frost"); NodeId = TEXT("flurry");
	StaminaKey = TEXT("stamina.weaponArt"); StaminaFallback = 12.f;
	CooldownKey = TEXT("spells.flurry.cooldown"); CooldownFallback = 1.5f;
	TimingKey = TEXT("weapon.lightTiming"); SwingKind = EWRSwingKind::WeaponArt;
	Element = WRTags::Element_Frost; ArtKey = TEXT("spells.flurry");
}

UWRGA_MoltenCleave::UWRGA_MoltenCleave()
{
	DisplayName = TEXT("Molten cleave"); PathId = TEXT("fire"); NodeId = TEXT("molten_cleave");
	StaminaKey = TEXT("stamina.weaponArt"); StaminaFallback = 25.f;
	CooldownKey = TEXT("spells.moltenCleave.cooldown"); CooldownFallback = 12.f;
	TimingKey = TEXT("weapon.heavyTiming"); SwingKind = EWRSwingKind::WeaponArt; bHeavy = true;
	Element = WRTags::Element_Fire; ArtKey = TEXT("spells.moltenCleave");
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

	if (IsServer())
	{
		const FVector Feet = Character->GetActorLocation() - FVector(0.f, 0.f, Character->GetSimpleCollisionHalfHeight());
		WRFx::Broadcast(Character, TEXT("dodge.dust"), Feet, (-Dir).Rotation(), 1.f);  // (the dodge sound is the audio director's)
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
	// Not State_Attacking: block pressed during a swing is held ready and the guard goes up when the swing ends (it
	// doesn't count while attacking). Its parry window starts at the press, so it mostly runs out during the swing.
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
	if (IsServer())
	{
		UE_LOG(LogWildreach, Verbose, TEXT("BLOCK %s raises the guard%s"), *GetNameSafe(GetAvatarActorFromActorInfo()),
			GetCharacter() && GetCharacter()->HasTag(WRTags::State_Attacking) ? TEXT(" (mid-swing: up when it ends)") : TEXT(""));
	}

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
	if (IsServer() && IsActive())
	{
		UE_LOG(LogWildreach, Verbose, TEXT("BLOCK %s lowers the guard"), *GetNameSafe(GetAvatarActorFromActorInfo()));
	}
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
	const auto Num = [&](const TCHAR* Key, float Fallback) { return T.Num(SpellKey + TEXT(".") + Key, Fallback); };
	const FRotator Aim = Caster->GetBaseAimRotation();
	const FVector Flat = FRotator(0.f, Aim.Yaw, 0.f).Vector();
	const FVector Feet = Caster->GetActorLocation() - FVector(0.f, 0.f, Caster->GetSimpleCollisionHalfHeight());
	const bool bFire = Element == WRTags::Element_Fire;
	const auto Ground = [World, Caster](const FVector& At)
	{
		FHitResult Hit;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(WRSpellGround), false, Caster);
		return World->LineTraceSingleByChannel(Hit, At + FVector(0, 0, 500), At - FVector(0, 0, 2000), ECC_WorldStatic, Query) ? Hit.ImpactPoint : At;
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
		FHitResult Hit;
		const FVector Eye = Caster->GetActorLocation() + FVector(0, 0, 60);
		FCollisionQueryParams Query(SCENE_QUERY_STAT(WRSpellAim), false, Caster);
		const float Range = Num(TEXT("range"), 1500.f);
		const FVector At = World->LineTraceSingleByChannel(Hit, Eye, Eye + Aim.Vector() * Range, ECC_WorldStatic, Query) ? Hit.ImpactPoint : Ground(Feet + Flat * Range * 0.7f);
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
		FHitResult Hit;
		const FVector Eye = Caster->GetActorLocation() + FVector(0, 0, 60);
		FCollisionQueryParams Query(SCENE_QUERY_STAT(WRMeteorAim), false, Caster);
		const float Range = Num(TEXT("range"), 2500.f);
		const FVector At = World->LineTraceSingleByChannel(Hit, Eye, Eye + Aim.Vector() * Range, ECC_WorldStatic, Query) ? Hit.ImpactPoint : Ground(Feet + Flat * Range * 0.6f);
		const float Radius = Num(TEXT("radius"), 450.f), Delay = Num(TEXT("delay"), 1.2f);
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
