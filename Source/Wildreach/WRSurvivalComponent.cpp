#include "WRSurvivalComponent.h"
#include "Net/UnrealNetwork.h"

UWRSurvivalComponent::UWRSurvivalComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UWRSurvivalComponent::BeginPlay()
{
	Super::BeginPlay();

	// Stats are only simulated on the server.
	SetComponentTickEnabled(GetOwner() && GetOwner()->HasAuthority());
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		Health = MaxHealth;
		Hunger = MaxHunger;
		Thirst = MaxThirst;
		Stamina = MaxStamina;
	}
}

void UWRSurvivalComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UWRSurvivalComponent, Health);
	DOREPLIFETIME_CONDITION(UWRSurvivalComponent, Hunger, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRSurvivalComponent, Thirst, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRSurvivalComponent, Stamina, COND_OwnerOnly);
}

void UWRSurvivalComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsAlive())
	{
		return;
	}

	Hunger = FMath::Max(0.f, Hunger - HungerPerSecond * DeltaTime);
	Thirst = FMath::Max(0.f, Thirst - ThirstPerSecond * DeltaTime);

	const int32 Deprivations = (Hunger <= 0.f ? 1 : 0) + (Thirst <= 0.f ? 1 : 0);
	if (Deprivations > 0)
	{
		ApplyDamage(DeprivationDamagePerSecond * Deprivations * DeltaTime);
	}

	if (bStaminaDraining)
	{
		Stamina = FMath::Max(0.f, Stamina - StaminaDrainPerSecond * DeltaTime);
		TimeSinceStaminaUse = 0.f;
	}
	else
	{
		TimeSinceStaminaUse += DeltaTime;
		if (TimeSinceStaminaUse >= StaminaRegenDelay)
		{
			Stamina = FMath::Min(MaxStamina, Stamina + StaminaRegenPerSecond * DeltaTime);
		}
	}
}

void UWRSurvivalComponent::ApplyDamage(float Amount)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !IsAlive() || Amount <= 0.f)
	{
		return;
	}

	const float OldHealth = Health;
	Health = FMath::Max(0.f, Health - Amount);
	OnRep_Health(OldHealth);
}

void UWRSurvivalComponent::Eat(float Amount)
{
	if (GetOwner() && GetOwner()->HasAuthority() && IsAlive())
	{
		Hunger = FMath::Clamp(Hunger + Amount, 0.f, MaxHunger);
	}
}

void UWRSurvivalComponent::Drink(float Amount)
{
	if (GetOwner() && GetOwner()->HasAuthority() && IsAlive())
	{
		Thirst = FMath::Clamp(Thirst + Amount, 0.f, MaxThirst);
	}
}

void UWRSurvivalComponent::OnRep_Health(float OldHealth)
{
	if (OldHealth > 0.f && Health <= 0.f)
	{
		OnDied.Broadcast();
	}
}
