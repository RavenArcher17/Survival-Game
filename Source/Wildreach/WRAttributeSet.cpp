#include "WRAttributeSet.h"
#include "Net/UnrealNetwork.h"

UWRAttributeSet::UWRAttributeSet()
{
	InitHealth(100.f);
	InitMaxHealth(100.f);
	InitStamina(100.f);
	InitMaxStamina(100.f);
	InitPoise(40.f);
	InitMaxPoise(40.f);
	InitHunger(100.f);
	InitThirst(100.f);
	InitHeat(0.f);
	InitMaxHeat(100.f);
	InitChill(0.f);
	InitMaxChill(100.f);
	InitBurnBuildup(0.f);
	InitFrostBuildup(0.f);
}

#define WR_REP(Prop) DOREPLIFETIME_CONDITION_NOTIFY(UWRAttributeSet, Prop, COND_None, REPNOTIFY_Always)
#define WR_REP_OWNER(Prop) DOREPLIFETIME_CONDITION_NOTIFY(UWRAttributeSet, Prop, COND_OwnerOnly, REPNOTIFY_Always)

void UWRAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Health and the status meters are visible on everyone (health bars, status icons).
	WR_REP(Health);
	WR_REP(MaxHealth);
	WR_REP(BurnBuildup);
	WR_REP(FrostBuildup);
	WR_REP(Poise);
	WR_REP(MaxPoise);

	// Personal resources only matter to the owner's HUD.
	WR_REP_OWNER(Stamina);
	WR_REP_OWNER(MaxStamina);
	WR_REP_OWNER(Hunger);
	WR_REP_OWNER(Thirst);
	WR_REP_OWNER(Heat);
	WR_REP_OWNER(MaxHeat);
	WR_REP_OWNER(Chill);
	WR_REP_OWNER(MaxChill);
}

#undef WR_REP
#undef WR_REP_OWNER

void UWRAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute()) NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	else if (Attribute == GetStaminaAttribute()) NewValue = FMath::Clamp(NewValue, 0.f, GetMaxStamina());
	else if (Attribute == GetPoiseAttribute()) NewValue = FMath::Clamp(NewValue, 0.f, GetMaxPoise());
	else if (Attribute == GetHeatAttribute()) NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHeat());
	else if (Attribute == GetChillAttribute()) NewValue = FMath::Clamp(NewValue, 0.f, GetMaxChill());
	else if (Attribute == GetHungerAttribute() || Attribute == GetThirstAttribute()) NewValue = FMath::Clamp(NewValue, 0.f, 100.f);
	else if (Attribute == GetBurnBuildupAttribute() || Attribute == GetFrostBuildupAttribute()) NewValue = FMath::Max(0.f, NewValue);
}

#define WR_ONREP(Prop) void UWRAttributeSet::OnRep_##Prop(const FGameplayAttributeData& Old) { GAMEPLAYATTRIBUTE_REPNOTIFY(UWRAttributeSet, Prop, Old); }
WR_ONREP(Health)
WR_ONREP(MaxHealth)
WR_ONREP(Stamina)
WR_ONREP(MaxStamina)
WR_ONREP(Poise)
WR_ONREP(MaxPoise)
WR_ONREP(Hunger)
WR_ONREP(Thirst)
WR_ONREP(Heat)
WR_ONREP(MaxHeat)
WR_ONREP(Chill)
WR_ONREP(MaxChill)
WR_ONREP(BurnBuildup)
WR_ONREP(FrostBuildup)
#undef WR_ONREP
