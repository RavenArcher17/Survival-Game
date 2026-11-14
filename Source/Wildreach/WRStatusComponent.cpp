#include "WRStatusComponent.h"
#include "WRSurvival.h"
#include "EngineUtils.h"
#include "Wildreach.h"
#include "WRAttributeSet.h"
#include "WRCharacterBase.h"
#include "WRCombatLibrary.h"
#include "WRGameplayTags.h"
#include "WRPathComponent.h"
#include "WRTuning.h"
#include "WRLifeSkills.h"
#include "WRInventory.h"
#include "WRCreature.h"
#include "WRBoss.h"
#include "WRCrafting.h"
#include "WRFx.h"
#include "WRSound.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"

// ---------------------------------------------------------------- the status table

namespace WRStatus
{
	static const TCHAR* const GNames[Count] = { TEXT("burn"), TEXT("frost"), TEXT("bleed"), TEXT("poison"), TEXT("rot"), TEXT("shock"),
		TEXT("soak"), TEXT("blind"), TEXT("decay"), TEXT("mark"), TEXT("curse") };

	FName Name(int32 Id)
	{
		return Id >= 0 && Id < Count ? FName(GNames[Id]) : NAME_None;
	}

	int32 FromName(FName InName)
	{
		if (InName == TEXT("frostbite")) return Frost;
		for (int32 i = 0; i < Count; ++i)
		{
			if (InName == GNames[i]) return i;
		}
		return Count;
	}

	FLinearColor Color(int32 Id)
	{
		// Status colours (combat.md 2.11): bleed is a dark red, never the crimson that means "unblockable".
		static const TCHAR* const Fallback[Count] = { TEXT("FF7A1E"), TEXT("A8DCFF"), TEXT("7A0A14"), TEXT("5FB830"), TEXT("A0522D"),
			TEXT("C9B8FF"), TEXT("2FA8A0"), TEXT("EEF2F6"), TEXT("7E8C6A"), TEXT("9A5CE6"), TEXT("3A1A4A") };
		const FString Hex = FWRTuning::Get().Str(FString::Printf(TEXT("status.defs.%s.color"), GNames[FMath::Clamp(Id, 0, Count - 1)]), Fallback[FMath::Clamp(Id, 0, Count - 1)]);
		return FLinearColor(FColor::FromHex(Hex));
	}

	int32 ForElement(const FGameplayTag& Element)
	{
		if (Element == WRTags::Element_Fire) return Burn;
		if (Element == WRTags::Element_Frost) return Frost;
		if (Element == WRTags::Element_Lightning) return Shock;
		if (Element == WRTags::Element_Water) return Soak;
		return Count;
	}

	/** The tags a status puts on its target (cures and resets clear them). */
	static TArray<FGameplayTag> TagsOf(int32 Id)
	{
		switch (Id)
		{
		case Burn: return { WRTags::Status_Burn };
		case Frost: return { WRTags::Status_Frostbite, WRTags::Status_Slowed, WRTags::Status_Frozen, WRTags::Status_Rimed };
		case Bleed: return { WRTags::Status_Bleeding };
		case Poison: return { WRTags::Status_Poison };
		case Rot: return { WRTags::Status_Rot };
		case Shock: return { WRTags::Status_Shock };
		case Soak: return { WRTags::Status_Soak };
		case Blind: return { WRTags::Status_Blind };
		case Decay: return { WRTags::Status_Decay };
		case Mark: return { WRTags::Status_Mark };
		case Curse: return { WRTags::Status_Curse };
		default: return {};
		}
	}

	static float Def(int32 Id, const TCHAR* Field, float Fallback)
	{
		return FWRTuning::Get().Num(FString::Printf(TEXT("status.defs.%s.%s"), GNames[FMath::Clamp(Id, 0, Count - 1)], Field), Fallback);
	}
}

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

	SinceHurt += DeltaTime;
	TickStatuses(DeltaTime);
	if (!Owner->IsAlive())
	{
		return;  // a status tick killed it
	}
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
	if (Status == WRTags::Status_Frostbite && HasStatus(WRTags::Status_Slowed) && !bResetting)
	{
		ClearStatus(WRTags::Status_Slowed);  // the frostbite's slow goes with it (fire ending it, a cure)
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
	// A stagger interrupts whatever the character was doing (a drink in progress is lost with it).
	Owner->GetAbilitySystemComponent()->CancelAllAbilities();
	ClearStatus(WRTags::State_Drinking);
}

// ---------------------------------------------------------------- buildup

float UWRStatusComponent::GetMeter(int32 Id) const
{
	const AWRCharacterBase* Owner = GetOwnerCharacter();
	const UWRAttributeSet* Attr = Owner ? Owner->GetAttributes() : nullptr;
	if (Id == WRStatus::Burn) return Attr ? Attr->GetBurnBuildup() : 0.f;
	if (Id == WRStatus::Frost) return Attr ? Attr->GetFrostBuildup() : 0.f;
	return Id >= 0 && Id < WRStatus::Count ? Meters[Id] : 0.f;
}

void UWRStatusComponent::SetMeter(int32 Id, float Value)
{
	AWRCharacterBase* Owner = GetOwnerCharacter();
	UAbilitySystemComponent* ASC = Owner ? Owner->GetAbilitySystemComponent() : nullptr;
	Value = FMath::Max(0.f, Value);
	if (Id == WRStatus::Burn) { if (ASC) ASC->SetNumericAttributeBase(UWRAttributeSet::GetBurnBuildupAttribute(), Value); }
	else if (Id == WRStatus::Frost) { if (ASC) ASC->SetNumericAttributeBase(UWRAttributeSet::GetFrostBuildupAttribute(), Value); }
	else if (Id >= 0 && Id < WRStatus::Count) Meters[Id] = Value;
}

float UWRStatusComponent::Threshold(int32 Id) const
{
	const AWRCharacterBase* Owner = GetOwnerCharacter();
	const FWRTuning& T = FWRTuning::Get();
	float Base = T.Num(TEXT("status.buildupThreshold"), 100.f);
	if (Owner && Owner->GetPlayerState())
	{
		// Players: 100 + what their armour resists (each biome set +50 to its own status).
		Base = T.Num(TEXT("status.thresholds.player"), 100.f);
		if (const UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Owner))
		{
			Base += Inv->GetWornResist(WRStatus::Name(Id));
		}
	}
	else if (const AWRCreature* Creature = Cast<AWRCreature>(Owner))
	{
		const FName Kind = UWRCombatLibrary::StanceKind(Owner);
		const FWRCreatureDef* Def = Creature->GetDef();
		const FString Tier = Kind != TEXT("normal") ? Kind.ToString() : Def && !Def->Tier.IsNone() ? Def->Tier.ToString() : FString(TEXT("lowland"));
		Base = T.Num(FString::Printf(TEXT("status.thresholds.%s"), *Tier), Base);
		// A boss fought by many resists more (x1.25 per extra participant, CAL 1.14).
		if (const AWRBoss* Boss = Cast<AWRBoss>(Owner))
		{
			Base *= FMath::Pow(T.Num(TEXT("status.bossParticipantResist"), 1.25f), FMath::Max(0, Boss->Participants - 1));
		}
	}
	// Each proc raises it (ER: Malenia's bleed 421, 519, 809...), until the fight's been over a while.
	return Base * FMath::Pow(T.Num(TEXT("status.procGrowth"), 1.3f), Id >= 0 && Id < WRStatus::Count ? Procs[Id] : 0);
}

void UWRStatusComponent::AddBuildup(const FGameplayTag& Element, float Amount, AWRCharacterBase* Source)
{
	const int32 Id = WRStatus::ForElement(Element);
	if (Id < WRStatus::Count)
	{
		AddStatusBuildup(Id, Amount, Source);
	}
}

void UWRStatusComponent::AddStatusBuildup(int32 Id, float Amount, AWRCharacterBase* Source, float HitDamage)
{
	AWRCharacterBase* Owner = GetOwnerCharacter();
	if (!Owner || !Owner->HasAuthority() || Amount <= 0.f || Id < 0 || Id >= WRStatus::Count || !Owner->IsAlive())
	{
		return;
	}
	// Soaked: fire takes half, lightning half again (the reactions table, status.defs.soak).
	if (HasStatus(WRTags::Status_Soak))
	{
		if (Id == WRStatus::Burn) Amount *= WRStatus::Def(WRStatus::Soak, TEXT("fireBuildup"), 0.5f);
		if (Id == WRStatus::Shock) Amount *= WRStatus::Def(WRStatus::Soak, TEXT("shockBuildup"), 1.5f);
		if (Id == WRStatus::Frost) Amount *= WRStatus::Def(WRStatus::Soak, TEXT("frostBuildup"), 1.5f);
	}
	SinceBuildup = 0.f;
	const float Value = GetMeter(Id) + Amount;
	const float Need = Threshold(Id);
	if (Value < Need)
	{
		SetMeter(Id, Value);
		return;
	}
	SetMeter(Id, 0.f);
	++Procs[Id];
	Proc(Id, Source, HitDamage);
}

void UWRStatusComponent::StatusHit(AWRCharacterBase* Source, float Damage, const FGameplayTag& Element)
{
	AWRCharacterBase* Owner = GetOwnerCharacter();
	if (!Owner || Damage <= 0.f)
	{
		return;
	}
	FWRHit Tick;
	Tick.Instigator = Source;
	Tick.Damage = Damage;
	Tick.Element = Element.IsValid() ? Element : WRTags::Element_Physical;
	Tick.bDamageOverTime = true;
	Tick.Origin = Owner->GetActorLocation();
	UWRCombatLibrary::ApplyHit(Owner, Tick);
}

void UWRStatusComponent::Proc(int32 Id, AWRCharacterBase* Source, float HitDamage)
{
	using namespace WRStatus;
	AWRCharacterBase* Owner = GetOwnerCharacter();
	UAbilitySystemComponent* ASC = Owner->GetAbilitySystemComponent();
	UWRAttributeSet* Attr = Owner->GetAttributes();
	const FName Kind = UWRCombatLibrary::StanceKind(Owner);
	const bool bBoss = Kind != TEXT("normal");
	const bool bPlayer = Owner->GetPlayerState() != nullptr;
	const float MaxHP = Attr->GetMaxHealth();
	UE_LOG(LogWildreach, Log, TEXT("STATUS %s procs %s (proc %d, by %s)"), *Owner->GetName(), *Name(Id).ToString(), Procs[Id], *GetNameSafe(Source));
	// The proc shows on every machine: a burst in the status's colour and its sound (fx.json / sounds.json rows).
	const FName Fx(*FString::Printf(TEXT("status.%s"), GNames[Id]));
	WRFx::BroadcastOn(Owner, Fx, NAME_None, 1.f, Fx, 0.6f);

	// A proc breaks a creature's hyperarmor (CAL 1.5): the attack it's in stops. Bosses don't flinch from it.
	if (!bPlayer && !bBoss && ASC->HasMatchingGameplayTag(WRTags::State_Hyperarmor) && Id != Burn)
	{
		Stagger(WRTune(TEXT("poise.creatureFlinchSeconds"), 0.45f));
	}

	switch (Id)
	{
	case WRStatus::Burn:
	{
		BurnSource = Source;
		ApplyStatus(WRTags::Status_Burn, Def(Burn, TEXT("seconds"), WRTune(TEXT("status.burnSeconds"), 6.f)) + UWRPathComponent::Stat(Source, TEXT("burnDuration")));
		// Spreading flames: the burn jumps to the nearest enemy that isn't already burning.
		if (Source && UWRPathComponent::HasNode(Source, TEXT("fire"), TEXT("spreading_flames")))
		{
			AWRCharacterBase* Next = nullptr;
			float Best = WRTune(TEXT("passives.spreadRadius"), 450.f);
			for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
			{
				const float D = FVector::Dist(It->GetActorLocation(), Owner->GetActorLocation());
				if (*It != Owner && *It != Source && It->IsAlive() && !It->HasTag(WRTags::Status_Burn) && Source->IsHostileTo(*It) && D < Best)
				{
					Best = D;
					Next = *It;
				}
			}
			if (Next)
			{
				Next->Status->AddStatusBuildup(Burn, Next->Status->Threshold(Burn), Source);
			}
		}
		break;
	}
	case WRStatus::Frost:
	{
		if (HasStatus(WRTags::Status_Frostbite) || HasStatus(WRTags::Status_Slowed))
		{
			// Frost on a frostbitten (or slowed) body freezes it solid (Raven's chain). Minis and majors can't be
			// frozen: they're Rimed instead, and take more stance (combat.md 2.11).
			if (bBoss)
			{
				ApplyStatus(WRTags::Status_Rimed, Def(Frost, TEXT("rimedSeconds"), 4.f));
			}
			else
			{
				ApplyStatus(WRTags::Status_Frozen, WRTune(TEXT("status.frozenSeconds"), 2.5f));
				ASC->CancelAllAbilities();
				ClearStatus(WRTags::State_Drinking);
				ClearStatus(WRTags::Status_Frostbite);
			}
			break;
		}
		// Frostbite: a burst of cold (8% of max health + 20, bosses 4%), +damage taken, slowed, and (players) slower
		// stamina; it can't proc again while it lasts (the next fill freezes instead).
		const float Pct = bBoss ? Def(Frost, TEXT("bossBurstPct"), 0.04f) : Def(Frost, TEXT("burstPct"), 0.08f);
		StatusHit(Source, MaxHP * Pct + Def(Frost, TEXT("burstFlat"), 20.f), WRTags::Element_Frost);
		if (!Owner->IsAlive())
		{
			break;
		}
		const float Seconds = Def(Frost, TEXT("seconds"), WRTune(TEXT("status.frostbiteSeconds"), 12.f));
		ApplyStatus(WRTags::Status_Frostbite, Seconds);
		ApplyStatus(WRTags::Status_Slowed, Seconds);
		// Biting cold: the slower's ranks make this slow deeper (tuning nodeEffects frost.biting_cold).
		Owner->SlowExtraPct = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(UWRPathComponent::Stat(Source, TEXT("slowStrength")) * 100.f), 0, 80));
		break;
	}
	case WRStatus::Bleed:
	{
		// Hemorrhage: 12% of max health + 15 at once (bosses x0.7).
		const float Burst = (MaxHP * Def(Bleed, TEXT("burstPct"), 0.12f) + Def(Bleed, TEXT("burstFlat"), 15.f)) * (bBoss ? Def(Bleed, TEXT("bossScale"), 0.7f) : 1.f);
		ApplyStatus(WRTags::Status_Bleeding, Def(Bleed, TEXT("markSeconds"), 6.f));
		StatusHit(Source, Burst, WRTags::Element_Physical);
		break;
	}
	case WRStatus::Poison:
		PoisonSource = Source;
		PoisonTick = 0.f;
		ApplyStatus(WRTags::Status_Poison, Def(Poison, TEXT("seconds"), 30.f));
		break;
	case WRStatus::Rot:
		RotSource = Source;
		RotTick = 0.f;
		ApplyStatus(WRTags::Status_Rot, Def(Rot, TEXT("seconds"), 20.f));
		break;
	case WRStatus::Shock:
	{
		ApplyStatus(WRTags::Status_Shock, Def(Shock, TEXT("stagger"), 0.4f));
		if (bBoss && ASC->HasMatchingGameplayTag(WRTags::State_Hyperarmor))
		{
			// A boss mid-swing isn't stopped: the shock goes into its stance instead.
			ASC->SetNumericAttributeBase(UWRAttributeSet::GetStanceAttribute(), Attr->GetStance() - Def(Shock, TEXT("bossStance"), 15.f));
			NotifyStanceDamaged();
		}
		else if (!(bPlayer && ASC->HasMatchingGameplayTag(WRTags::Passive_Grounded)))
		{
			Stagger(Def(Shock, TEXT("stagger"), 0.4f));
		}
		if (bPlayer)
		{
			ASC->SetNumericAttributeBase(UWRAttributeSet::GetStaminaAttribute(), Attr->GetStamina() - Def(Shock, TEXT("playerStamina"), 20.f));
			NotifyStaminaUsed();
		}
		// The arc: a share of the blow jumps to up to two others close by.
		const float Chain = HitDamage * Def(Shock, TEXT("chainShare"), 0.25f);
		int32 Left = FMath::RoundToInt(Def(Shock, TEXT("chainTargets"), 2.f));
		for (TActorIterator<AWRCharacterBase> It(GetWorld()); It && Left > 0 && Chain > 0.f; ++It)
		{
			if (*It != Owner && *It != Source && It->IsAlive() && (!Source || Source->IsHostileTo(*It))
				&& FVector::Dist(It->GetActorLocation(), Owner->GetActorLocation()) < Def(Shock, TEXT("chainRadius"), 400.f))
			{
				--Left;
				It->Status->StatusHit(Source, Chain, WRTags::Element_Lightning);
				WRFx::Broadcast(Owner, TEXT("status.shock"), It->GetActorLocation(), (It->GetActorLocation() - Owner->GetActorLocation()).Rotation(), 0.6f);
			}
		}
		break;
	}
	case WRStatus::Soak:
		SoakDryFor = 0.f;
		ApplyStatus(WRTags::Status_Soak, Def(Soak, TEXT("seconds"), 10.f));
		break;
	case WRStatus::Blind:
	{
		const float Seconds = bBoss ? Def(Blind, TEXT("bossSeconds"), 1.5f) : Def(Blind, TEXT("seconds"), 3.f);
		ApplyStatus(WRTags::Status_Blind, Seconds);
		// A blinded creature loses whoever it was on and goes for the spot it last saw them (AWRCreatureAI).
		if (AWRCreature* Creature = Cast<AWRCreature>(Owner))
		{
			if (AActor* Was = Creature->GetAggroTarget())
			{
				Creature->LoseSightOf(Was, Seconds);
			}
		}
		break;
	}
	case WRStatus::Decay:
	{
		// Max health down 10% until the fight ends (players: until they rest or respawn), healing halved a while.
		if (DecayLost <= 0.f)
		{
			DecayLost = MaxHP * Def(Decay, TEXT("maxHealthPct"), 0.1f);
			ASC->SetNumericAttributeBase(UWRAttributeSet::GetMaxHealthAttribute(), MaxHP - DecayLost);
			ASC->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), FMath::Min(Attr->GetHealth(), MaxHP - DecayLost));
			Owner->DecayLost = DecayLost;
		}
		ApplyStatus(WRTags::Status_Decay, Def(Decay, TEXT("seconds"), 20.f));
		break;
	}
	case WRStatus::Mark:
		ApplyStatus(WRTags::Status_Mark, Def(WRStatus::Mark, TEXT("seconds"), 10.f));
		break;
	case WRStatus::Curse:
	{
		// The madness part: a quarter of its path resource is gone, and it deals less damage for a while.
		const float Loss = Def(Curse, TEXT("resourceLoss"), 0.25f);
		ASC->SetNumericAttributeBase(UWRAttributeSet::GetHeatAttribute(), Attr->GetHeat() - Attr->GetMaxHeat() * Loss);
		ASC->SetNumericAttributeBase(UWRAttributeSet::GetChillAttribute(), Attr->GetChill() - Attr->GetMaxChill() * Loss);
		ApplyStatus(WRTags::Status_Curse, Def(Curse, TEXT("seconds"), 15.f));
		break;
	}
	default:
		break;
	}
	PublishMeters();
}

void UWRStatusComponent::CureStatus(int32 Id)
{
	if (Id < 0 || Id >= WRStatus::Count)
	{
		return;
	}
	for (const FGameplayTag& Tag : WRStatus::TagsOf(Id))
	{
		ClearStatus(Tag);
	}
	SetMeter(Id, 0.f);
	PublishMeters();
}

void UWRStatusComponent::MashFrozen()
{
	// Players break out of a freeze sooner by pressing buttons (combat.md 2.11): each press takes mashSeconds off,
	// never below minSeconds from when it started.
	double* End = StatusEnds.Find(WRTags::Status_Frozen);
	AWRCharacterBase* Owner = GetOwnerCharacter();
	if (!End || !Owner || !Owner->GetPlayerState())
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	const float Full = WRTune(TEXT("status.frozenSeconds"), 2.5f);
	const double Earliest = *End - Full + WRStatus::Def(WRStatus::Frost, TEXT("mashMinSeconds"), 1.2f);
	*End = FMath::Max(FMath::Max(Earliest, Now), *End - WRStatus::Def(WRStatus::Frost, TEXT("mashSeconds"), 0.15f));
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

float UWRStatusComponent::HealingTakenScale() const
{
	float Scale = 1.f;
	if (HasStatus(WRTags::Status_Rot)) Scale *= WRStatus::Def(WRStatus::Rot, TEXT("healingTaken"), 0.5f);
	if (HasStatus(WRTags::Status_Decay)) Scale *= WRStatus::Def(WRStatus::Decay, TEXT("healingTaken"), 0.5f);
	if (const UWRSurvivalComponent* Survival = UWRSurvivalComponent::For(GetOwner())) Scale *= Survival->HealingTakenMult();  // corruption 25
	return Scale;
}

void UWRStatusComponent::RestoreDecay()
{
	AWRCharacterBase* Owner = GetOwnerCharacter();
	UAbilitySystemComponent* ASC = Owner ? Owner->GetAbilitySystemComponent() : nullptr;
	if (!ASC || DecayLost <= 0.f || !Owner->GetAttributes())
	{
		return;
	}
	ASC->SetNumericAttributeBase(UWRAttributeSet::GetMaxHealthAttribute(), Owner->GetAttributes()->GetMaxHealth() + DecayLost);
	UE_LOG(LogWildreach, Log, TEXT("STATUS %s gets back %.0f max health lost to decay"), *Owner->GetName(), DecayLost);
	DecayLost = 0.f;
	Owner->DecayLost = 0.f;
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
	RestoreDecay();
	for (int32 i = 0; i < WRStatus::Count; ++i)
	{
		SetMeter(i, 0.f);
		Procs[i] = 0;
	}
	PublishMeters();
	HungerEmptyFor = ThirstEmptyFor = 0.f;
}

void UWRStatusComponent::PublishMeters()
{
	// Every machine's bars read these (AWRCharacterBase::BuildupPct): only a change replicates.
	AWRCharacterBase* Owner = GetOwnerCharacter();
	if (!Owner)
	{
		return;
	}
	if (Owner->BuildupPct.Num() != WRStatus::Count)
	{
		Owner->BuildupPct.Init(0, WRStatus::Count);
	}
	for (int32 i = 0; i < WRStatus::Count; ++i)
	{
		const uint8 Pct = uint8(FMath::Clamp(FMath::RoundToInt(100.f * GetMeter(i) / FMath::Max(1.f, Threshold(i))), 0, 100));
		if (Owner->BuildupPct[i] != Pct)
		{
			Owner->BuildupPct[i] = Pct;
		}
	}
}

// ---------------------------------------------------------------- ticks

void UWRStatusComponent::TickStatuses(float DeltaTime)
{
	using namespace WRStatus;
	AWRCharacterBase* Owner = GetOwnerCharacter();
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

	const float MaxHP = Owner->GetAttributes()->GetMaxHealth();
	if (HasStatus(WRTags::Status_Burn))
	{
		BurnTickAccumulator += DeltaTime;
		while (BurnTickAccumulator >= 1.f)
		{
			BurnTickAccumulator -= 1.f;
			StatusHit(BurnSource.Get(), WRTune(TEXT("status.burnDamagePerSecond"), 4.f) * (1.f + UWRPathComponent::Stat(BurnSource.Get(), TEXT("burnDamage"))), WRTags::Element_Fire);
			if (!Owner->IsAlive())
			{
				return;
			}
		}
	}
	else
	{
		BurnTickAccumulator = 0.f;
	}
	// Poison: small and long; rot: heavy and shorter (both % of max health + a flat part, every second).
	if (HasStatus(WRTags::Status_Poison))
	{
		PoisonTick += DeltaTime;
		while (PoisonTick >= 1.f && Owner->IsAlive())
		{
			PoisonTick -= 1.f;
			StatusHit(PoisonSource.Get(), MaxHP * Def(Poison, TEXT("pctPerSecond"), 0.004f) + Def(Poison, TEXT("flatPerSecond"), 2.f), WRTags::Element_Physical);
		}
	}
	if (HasStatus(WRTags::Status_Rot) && Owner->IsAlive())
	{
		RotTick += DeltaTime;
		while (RotTick >= 1.f && Owner->IsAlive())
		{
			RotTick -= 1.f;
			StatusHit(RotSource.Get(), MaxHP * Def(Rot, TEXT("pctPerSecond"), 0.008f) + Def(Rot, TEXT("flatPerSecond"), 4.f), WRTags::Element_Physical);
		}
	}
	if (!Owner->IsAlive())
	{
		return;
	}
	// Soaked: standing by a fire (a burning patch, a lava edge, a Hearth) dries it off after a few seconds; a burn does too.
	if (HasStatus(WRTags::Status_Soak))
	{
		bool bByFire = HasStatus(WRTags::Status_Burn);
		const float Near = Def(Soak, TEXT("fireRadius"), 400.f);
		for (TActorIterator<AWRHazard> It(GetWorld()); It && !bByFire; ++It)
		{
			bByFire = (It->Kind == TEXT("fire") || It->Kind == TEXT("lava")) && FVector::Dist2D(It->GetActorLocation(), Owner->GetActorLocation()) < It->Radius + Near;
		}
		for (TActorIterator<AWRHearth> It(GetWorld()); It && !bByFire; ++It)
		{
			bByFire = FVector::Dist(It->GetActorLocation(), Owner->GetActorLocation()) < Near;
		}
		SoakDryFor = bByFire ? SoakDryFor + DeltaTime : 0.f;
		if (SoakDryFor >= Def(Soak, TEXT("drySeconds"), 3.f))
		{
			CureStatus(Soak);
		}
	}
	// The fight is over a while: procs no longer raise the bar, and a creature gets back what Decay took.
	if (SinceHurt > WRTune(TEXT("status.resetAfterCombat"), 30.f))
	{
		for (int32 i = 0; i < Count; ++i) Procs[i] = 0;
		if (!Owner->GetPlayerState() || SinceHurt > Def(Decay, TEXT("playerRestSeconds"), 600.f))
		{
			RestoreDecay();
		}
	}
}

void UWRStatusComponent::TickBuildup(float DeltaTime)
{
	SinceBuildup += DeltaTime;
	if (SinceBuildup >= WRTune(TEXT("status.buildupDecayDelay"), 3.f))
	{
		const float Decay = WRTune(TEXT("status.buildupDecayPerSecond"), 8.f) * DeltaTime;
		for (int32 i = 0; i < WRStatus::Count; ++i)
		{
			const float M = GetMeter(i);
			if (M > 0.f) SetMeter(i, M - Decay);
		}
	}
	PublishClock -= DeltaTime;
	if (PublishClock <= 0.f)
	{
		PublishClock = 0.1f;
		PublishMeters();
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
	// No regen from a swing's press until its cancel window (ER): the stamina spent on a chain stays spent. Nor while a
	// charge is held, or a drink is going down (combat.md 2.3).
	const bool bMidSwing = (Owner->IsSwinging() && GetWorld()->GetTimeSeconds() < Owner->Swing.CancelAt)
		|| ASC->HasMatchingGameplayTag(WRTags::State_Charging) || ASC->HasMatchingGameplayTag(WRTags::State_Drinking);
	SinceHeatGain += DeltaTime;
	SinceChillGain += DeltaTime;

	// The fast stroke in the water costs its own stamina (Strong stroke halves it, Seaborn makes it free).
	const UCharacterMovementComponent* Move = Owner ? Owner->GetCharacterMovement() : nullptr;
	const bool bFastStroke = bSprintDraining && Move && Move->IsSwimming();
	const UWRSurvivalComponent* Survival = UWRSurvivalComponent::For(Owner);
	const float DrainPerSecond = bFastStroke
		? WRTune(TEXT("water.fastSwimStamina"), 8.f) * FMath::Max(0.f, 1.f + UWRLifeSkillsComponent::StatFor(Owner, TEXT("swimStamina"))) * (Survival ? Survival->SwimStaminaMult() : 1.f)
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
		if (HasStatus(WRTags::Status_Frostbite) && Owner->GetPlayerState())
		{
			// Frostbitten players recover stamina slower (ER: frostbite slows stamina regen).
			Regen *= WRStatus::Def(WRStatus::Frost, TEXT("playerStaminaRegen"), 0.6f);
		}
		// Rested, a blizzard in the open, corruption (WRSurvival).
		if (Survival)
		{
			Regen *= Survival->StaminaRegenMult();
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
		* FMath::Max(0.1f, 1.f + UWRLifeSkillsComponent::StatFor(Owner, TEXT("hungerDrain")))
		* (UWRSurvivalComponent::For(Owner) ? UWRSurvivalComponent::For(Owner)->HungerDrainMult() : 1.f);  // Rested
	const float ThirstRate = WorldRate * 100.f / (WRTune(TEXT("survival.thirstMinutes"), 40.f) * 60.f)
		* (UWRSurvivalComponent::For(Owner) ? UWRSurvivalComponent::For(Owner)->ThirstDrainMult() : 1.f);
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
