#include "WRStatusComponent.h"
#include "EngineUtils.h"
#include "Wildreach.h"
#include "WRAttributeSet.h"
#include "WRCharacterBase.h"
#include "WRCombatLibrary.h"
#include "WRGameplayTags.h"
#include "WRPathComponent.h"
#include "WRTuning.h"
#include "WRLifeSkills.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

UWRStatusComponent::UWRStatusComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.f;
	SetIsReplicatedByDefault(false);
}

AWRCharacterBase* UWRStatusComponent::GetOwnerCharacter() const
{
	return Cast<AWRCharacterBase>(GetOwner());
}

void UWRStatusComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AWRCharacterBase* Owner = GetOwnerCharacter();
	if (!Owner || !Owner->HasAuthority() || !Owner->GetAbilitySystemComponent() || !Owner->GetAttributes() || !Owner->IsAlive())
	{
		return;
	}

	TickStatuses(DeltaTime);
	TickBuildup(DeltaTime);
	TickRegen(DeltaTime);
	if (bSurvivalNeeds)
	{
		TickSurvival(DeltaTime);
	}
}

void UWRStatusComponent::ApplyStatus(const FGameplayTag& Status, float Duration)
{
	AWRCharacterBase* Owner = GetOwnerCharacter();
	UAbilitySystemComponent* ASC = Owner ? Owner->GetAbilitySystemComponent() : nullptr;
	if (!ASC || !Owner->HasAuthority())
	{
		return;
	}

	const double End = GetWorld()->GetTimeSeconds() + Duration;
	if (double* Existing = StatusEnds.Find(Status))
	{
		*Existing = FMath::Max(*Existing, End);
		return;
	}
	StatusEnds.Add(Status, End);
	ASC->AddLooseGameplayTag(Status, 1, EGameplayTagReplicationState::TagOnly);
	UE_LOG(LogWildreach, Log, TEXT("STATUS %s +%s (%.1fs)"), *Owner->GetName(), *Status.ToString(), Duration);
}

void UWRStatusComponent::ClearStatus(const FGameplayTag& Status)
{
	AWRCharacterBase* Owner = GetOwnerCharacter();
	UAbilitySystemComponent* ASC = Owner ? Owner->GetAbilitySystemComponent() : nullptr;
	if (!ASC || !StatusEnds.Remove(Status))
	{
		return;
	}
	ASC->RemoveLooseGameplayTag(Status, 1, EGameplayTagReplicationState::TagOnly);
	if (Status == WRTags::Status_Slowed) Owner->SlowExtraPct = 0;

	if (Status == WRTags::Status_Staggered)
	{
		// Poise comes back full once a stagger ends (ER: at once after a reaction).
		ASC->SetNumericAttributeBase(UWRAttributeSet::GetPoiseAttribute(), Owner->GetAttributes()->GetMaxPoise());
	}
	if (Status == WRTags::Status_StanceBroken && Owner->IsAlive())
	{
		// The break is over (or a crit spent it): the stance is back full and the gold mark goes.
		ASC->SetNumericAttributeBase(UWRAttributeSet::GetStanceAttribute(), Owner->GetAttributes()->GetMaxStance());
		Owner->ParriesTaken = 0;
		Owner->MulticastStanceBreak(false);
	}
	if (Status == WRTags::State_BlockStun && Owner->IsAlive() && !bResetting)
	{
		// The block stun is over: Heavy is the guard counter for a moment (COMBAT-AND-LESSONS 1.2).
		ApplyStatus(WRTags::State_GuardCounterWindow, WRTune(TEXT("defense.guardCounterWindow"), 0.6f));
	}
	UE_LOG(LogWildreach, Log, TEXT("STATUS %s -%s"), *Owner->GetName(), *Status.ToString());
}

bool UWRStatusComponent::HasStatus(const FGameplayTag& Status) const
{
	return StatusEnds.Contains(Status);
}

void UWRStatusComponent::Stagger(float Duration)
{
	AWRCharacterBase* Owner = GetOwnerCharacter();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}
	ApplyStatus(WRTags::Status_Staggered, Duration);
	// A stagger interrupts whatever the character was doing.
	Owner->GetAbilitySystemComponent()->CancelAllAbilities();
}

void UWRStatusComponent::AddBuildup(const FGameplayTag& Element, float Amount, AWRCharacterBase* Source)
{
	AWRCharacterBase* Owner = GetOwnerCharacter();
	if (!Owner || Amount <= 0.f)
	{
		return;
	}
	UAbilitySystemComponent* ASC = Owner->GetAbilitySystemComponent();
	UWRAttributeSet* Attr = Owner->GetAttributes();
	const float Threshold = WRTune(TEXT("status.buildupThreshold"), 100.f);
	SinceBuildup = 0.f;

	if (Element == WRTags::Element_Fire)
	{
		const float Value = Attr->GetBurnBuildup() + Amount;
		if (Value >= Threshold)
		{
			ASC->SetNumericAttributeBase(UWRAttributeSet::GetBurnBuildupAttribute(), 0.f);
			BurnSource = Source;
			ApplyStatus(WRTags::Status_Burn, WRTune(TEXT("status.burnSeconds"), 6.f) + UWRPathComponent::Stat(Source, TEXT("burnDuration")));
			// Spreading flames: the burn jumps to the nearest enemy that isn't already burning.
			AWRCharacterBase* Self = Cast<AWRCharacterBase>(GetOwner());
			if (Source && Self && UWRPathComponent::HasNode(Source, TEXT("fire"), TEXT("spreading_flames")))
			{
				AWRCharacterBase* Next = nullptr;
				float Best = WRTune(TEXT("passives.spreadRadius"), 450.f);
				for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
				{
					const float D = FVector::Dist(It->GetActorLocation(), Self->GetActorLocation());
					if (*It != Self && *It != Source && It->IsAlive() && !It->HasTag(WRTags::Status_Burn) && Source->IsHostileTo(*It) && D < Best)
					{
						Best = D;
						Next = *It;
					}
				}
				if (Next)
				{
					Next->Status->AddBuildup(WRTags::Element_Fire, Threshold, Source);
				}
			}
		}
		else
		{
			ASC->SetNumericAttributeBase(UWRAttributeSet::GetBurnBuildupAttribute(), Value);
		}
	}
	else if (Element == WRTags::Element_Frost)
	{
		const float Value = Attr->GetFrostBuildup() + Amount;
		if (Value >= Threshold)
		{
			ASC->SetNumericAttributeBase(UWRAttributeSet::GetFrostBuildupAttribute(), 0.f);
			if (HasStatus(WRTags::Status_Slowed))
			{
				// Frostbite on an already slowed target freezes it solid.
				ApplyStatus(WRTags::Status_Frozen, WRTune(TEXT("status.frozenSeconds"), 2.5f));
				Owner->GetAbilitySystemComponent()->CancelAllAbilities();
			}
			else
			{
				const float Seconds = WRTune(TEXT("status.frostbiteSeconds"), 6.f);
				ApplyStatus(WRTags::Status_Frostbite, Seconds);
				ApplyStatus(WRTags::Status_Slowed, Seconds);
				// Biting cold: the slower's ranks make this slow deeper (tuning nodeEffects frost.biting_cold).
				Owner->SlowExtraPct = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(UWRPathComponent::Stat(Source, TEXT("slowStrength")) * 100.f), 0, 80));
			}
		}
		else
		{
			ASC->SetNumericAttributeBase(UWRAttributeSet::GetFrostBuildupAttribute(), Value);
		}
	}
}

void UWRStatusComponent::NotifyResourceGained(const FGameplayTag& Element)
{
	if (Element == WRTags::Element_Fire) SinceHeatGain = 0.f;
	else if (Element == WRTags::Element_Frost) SinceChillGain = 0.f;
}

bool UWRStatusComponent::IsDeprived() const
{
	return bSurvivalNeeds && (HungerEmptyFor > 0.f || ThirstEmptyFor > 0.f);
}

void UWRStatusComponent::ResetAll()
{
	TArray<FGameplayTag> Active;
	StatusEnds.GetKeys(Active);
	bResetting = true;
	for (const FGameplayTag& Tag : Active)
	{
		ClearStatus(Tag);
	}
	bResetting = false;
	HungerEmptyFor = ThirstEmptyFor = 0.f;
}

void UWRStatusComponent::TickStatuses(float DeltaTime)
{
	const double Now = GetWorld()->GetTimeSeconds();
	TArray<FGameplayTag> Expired;
	for (const TPair<FGameplayTag, double>& Pair : StatusEnds)
	{
		if (Pair.Value <= Now)
		{
			Expired.Add(Pair.Key);
		}
	}
	for (const FGameplayTag& Tag : Expired)
	{
		ClearStatus(Tag);
	}

	if (HasStatus(WRTags::Status_Burn))
	{
		BurnTickAccumulator += DeltaTime;
		while (BurnTickAccumulator >= 1.f)
		{
			BurnTickAccumulator -= 1.f;
			FWRHit Tick;
			Tick.Instigator = BurnSource.Get();
			Tick.Damage = WRTune(TEXT("status.burnDamagePerSecond"), 4.f) * (1.f + UWRPathComponent::Stat(BurnSource.Get(), TEXT("burnDamage")));
			Tick.Element = WRTags::Element_Fire;
			Tick.bDamageOverTime = true;
			UWRCombatLibrary::ApplyHit(GetOwnerCharacter(), Tick);
			if (!GetOwnerCharacter() || !GetOwnerCharacter()->IsAlive())
			{
				return;
			}
		}
	}
	else
	{
		BurnTickAccumulator = 0.f;
	}
}

void UWRStatusComponent::TickBuildup(float DeltaTime)
{
	AWRCharacterBase* Owner = GetOwnerCharacter();
	UAbilitySystemComponent* ASC = Owner->GetAbilitySystemComponent();
	UWRAttributeSet* Attr = Owner->GetAttributes();
	SinceBuildup += DeltaTime;
	if (SinceBuildup < WRTune(TEXT("status.buildupDecayDelay"), 3.f))
	{
		return;
	}
	const float Decay = WRTune(TEXT("status.buildupDecayPerSecond"), 8.f) * DeltaTime;
	if (Attr->GetBurnBuildup() > 0.f)
	{
		ASC->SetNumericAttributeBase(UWRAttributeSet::GetBurnBuildupAttribute(), Attr->GetBurnBuildup() - Decay);
	}
	if (Attr->GetFrostBuildup() > 0.f)
	{
		ASC->SetNumericAttributeBase(UWRAttributeSet::GetFrostBuildupAttribute(), Attr->GetFrostBuildup() - Decay);
	}
}

void UWRStatusComponent::TickRegen(float DeltaTime)
{
	AWRCharacterBase* Owner = GetOwnerCharacter();
	UAbilitySystemComponent* ASC = Owner->GetAbilitySystemComponent();
	UWRAttributeSet* Attr = Owner->GetAttributes();

	SinceStaminaUse += DeltaTime;
	SincePoiseDamage += DeltaTime;
	SinceStanceDamage += DeltaTime;
	// No regen from a swing's press until its cancel window (ER): the stamina spent on a chain stays spent.
	const bool bMidSwing = Owner->IsSwinging() && GetWorld()->GetTimeSeconds() < Owner->Swing.CancelAt;
	SinceHeatGain += DeltaTime;
	SinceChillGain += DeltaTime;

	// The fast stroke in the water costs its own stamina (Strong stroke halves it, Seaborn makes it free).
	const UCharacterMovementComponent* Move = Owner ? Owner->GetCharacterMovement() : nullptr;
	const bool bFastStroke = bSprintDraining && Move && Move->IsSwimming();
	const float DrainPerSecond = bFastStroke
		? WRTune(TEXT("water.fastSwimStamina"), 8.f) * FMath::Max(0.f, 1.f + UWRLifeSkillsComponent::StatFor(Owner, TEXT("swimStamina")))
		: WRTune(TEXT("stamina.sprintPerSecond"), 5.f);
	if (bSprintDraining && DrainPerSecond > 0.f)
	{
		ASC->SetNumericAttributeBase(UWRAttributeSet::GetStaminaAttribute(), Attr->GetStamina() - DrainPerSecond * DeltaTime);
		SinceStaminaUse = 0.f;
	}
	else if (SinceStaminaUse >= WRTune(TEXT("player.staminaRegenDelay"), 0.5f) && Attr->GetStamina() < Attr->GetMaxStamina()
		&& !ASC->HasMatchingGameplayTag(WRTags::State_Blocking) && !bMidSwing)
	{
		float Regen = WRTune(TEXT("player.staminaRegenPerSecond"), 45.f);
		if (bSurvivalNeeds && (Attr->GetHunger() <= 0.f || Attr->GetThirst() <= 0.f))
		{
			Regen *= WRTune(TEXT("survival.deprivedStaminaRegenMultiplier"), 0.5f);
		}
		if (Attr->GetMaxHeat() > 0.f && Attr->GetHeat() / Attr->GetMaxHeat() >= 0.5f)
		{
			// Bellows: faster stamina regen at high Heat.
			Regen *= 1.f + UWRPathComponent::Stat(Owner, TEXT("staminaRegenHighHeat"));
		}
		ASC->SetNumericAttributeBase(UWRAttributeSet::GetStaminaAttribute(), Attr->GetStamina() + Regen * DeltaTime);
	}

	// Poise: back to full poiseResetSeconds after the last poise hit (ER; at once after a reaction, ClearStatus).
	if (SincePoiseDamage >= WRTune(TEXT("player.poiseResetSeconds"), 30.f) && Attr->GetPoise() < Attr->GetMaxPoise()
		&& !HasStatus(WRTags::Status_Staggered))
	{
		ASC->SetNumericAttributeBase(UWRAttributeSet::GetPoiseAttribute(), Attr->GetMaxPoise());
	}
	// Stance (creatures and bosses): refills max / regenDelayDivisor s after the last stance hit, at regenPerSecond,
	// half as fast below half health (Sekiro); parries counted toward a break are forgotten once it's full.
	if (Owner->IsCreature() && Attr->GetMaxStance() > 0.f && Attr->GetStance() < Attr->GetMaxStance() && !HasStatus(WRTags::Status_StanceBroken)
		&& SinceStanceDamage >= Attr->GetMaxStance() / FMath::Max(1.f, WRTune(TEXT("stance.regenDelayDivisor"), 13.f)))
	{
		const bool bLow = Attr->GetMaxHealth() > 0.f && Attr->GetHealth() < Attr->GetMaxHealth() * 0.5f;
		const float Rate = WRTune(TEXT("stance.regenPerSecond"), 13.f) * (bLow ? WRTune(TEXT("stance.lowHealthRegen"), 0.5f) : 1.f);
		ASC->SetNumericAttributeBase(UWRAttributeSet::GetStanceAttribute(), Attr->GetStance() + Rate * DeltaTime);
		if (Attr->GetStance() >= Attr->GetMaxStance())
		{
			Owner->ParriesTaken = 0;
		}
	}

	if (Attr->GetHeat() > 0.f && SinceHeatGain >= WRTune(TEXT("resources.heatDecayDelay"), 4.f))
	{
		ASC->SetNumericAttributeBase(UWRAttributeSet::GetHeatAttribute(), Attr->GetHeat() - WRTune(TEXT("resources.heatDecayPerSecond"), 6.f) * DeltaTime);
	}
	if (Attr->GetChill() > 0.f && SinceChillGain >= WRTune(TEXT("resources.chillDecayDelay"), 8.f))
	{
		ASC->SetNumericAttributeBase(UWRAttributeSet::GetChillAttribute(), Attr->GetChill() - WRTune(TEXT("resources.chillDecayPerSecond"), 2.f) * DeltaTime);
	}
}

void UWRStatusComponent::TickSurvival(float DeltaTime)
{
	AWRCharacterBase* Owner = GetOwnerCharacter();
	UAbilitySystemComponent* ASC = Owner->GetAbilitySystemComponent();
	UWRAttributeSet* Attr = Owner->GetAttributes();

	// Meters are 0..100; full to empty in hungerMinutes / thirstMinutes of play.
	const float WorldRate = FMath::Max(0.f, WRTune(TEXT("server.hungerMultiplier"), 1.f));
	// Hearty (cooking perk): hunger drains slower for you.
	const float HungerRate = WorldRate * 100.f / (WRTune(TEXT("survival.hungerMinutes"), 60.f) * 60.f)
		* FMath::Max(0.1f, 1.f + UWRLifeSkillsComponent::StatFor(Owner, TEXT("hungerDrain")));
	const float ThirstRate = WorldRate * 100.f / (WRTune(TEXT("survival.thirstMinutes"), 40.f) * 60.f);
	ASC->SetNumericAttributeBase(UWRAttributeSet::GetHungerAttribute(), Attr->GetHunger() - HungerRate * DeltaTime);
	ASC->SetNumericAttributeBase(UWRAttributeSet::GetThirstAttribute(), Attr->GetThirst() - ThirstRate * DeltaTime);

	HungerEmptyFor = Attr->GetHunger() <= 0.f ? HungerEmptyFor + DeltaTime : 0.f;
	ThirstEmptyFor = Attr->GetThirst() <= 0.f ? ThirstEmptyFor + DeltaTime : 0.f;

	// Empty first halves stamina regen (TickRegen); after the grace period health slowly drains.
	const float Grace = WRTune(TEXT("survival.deprivedHealthGraceSeconds"), 120.f);
	const int32 Deprivations = (HungerEmptyFor > Grace ? 1 : 0) + (ThirstEmptyFor > Grace ? 1 : 0);
	if (Deprivations > 0)
	{
		ASC->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(),
			Attr->GetHealth() - WRTune(TEXT("survival.deprivedDamagePerSecond"), 0.5f) * Deprivations * DeltaTime);
		if (Attr->GetHealth() <= 0.f)
		{
			Owner->Die(nullptr);
		}
	}
}
