#include "WRAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "WRTuning.h"

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
	InitCharge(0.f);
	InitMaxCharge(100.f);
	InitGrowth(0.f);
	InitMaxGrowth(100.f);
	InitSouls(0.f);
	InitMaxSouls(20.f);
	InitMaxHealthLost(0.f);
	InitVoid(0.f);
	InitMaxVoid(100.f);
	InitBurnBuildup(0.f);
	InitFrostBuildup(0.f);
	InitStance(60.f);
	InitMaxStance(60.f);
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
	// The grey band and the Void bar drive the Void body change on everyone (biome-void 2.18).
	WR_REP(MaxHealthLost);
	WR_REP(Void);
	WR_REP(MaxVoid);

	// Personal resources only matter to the owner's HUD.
	WR_REP_OWNER(Stamina);
	WR_REP_OWNER(MaxStamina);
	WR_REP_OWNER(Hunger);
	WR_REP_OWNER(Thirst);
	WR_REP_OWNER(Heat);
	WR_REP_OWNER(MaxHeat);
	WR_REP_OWNER(Chill);
	WR_REP_OWNER(MaxChill);
	WR_REP_OWNER(Charge);
	WR_REP_OWNER(MaxCharge);
	WR_REP_OWNER(Growth);
	WR_REP_OWNER(MaxGrowth);
	WR_REP_OWNER(Souls);
	WR_REP_OWNER(MaxSouls);
}

#undef WR_REP
#undef WR_REP_OWNER

void UWRAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute()) NewValue = FMath::Clamp(NewValue, 0.f, GetEffectiveMaxHealth());
	else if (Attribute == GetMaxHealthLostAttribute()) NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	else if (Attribute == GetVoidAttribute()) NewValue = FMath::Clamp(NewValue, 0.f, GetMaxVoid());
	// Stamina may dip below zero on the last action (COMBAT-AND-LESSONS 1.6, stamina.graceFloor): nothing starts until it's back over.
	else if (Attribute == GetStaminaAttribute()) NewValue = FMath::Clamp(NewValue, FMath::Min(0.f, WRTune(TEXT("stamina.graceFloor"), -20.f)), GetMaxStamina());
	else if (Attribute == GetStanceAttribute()) NewValue = FMath::Clamp(NewValue, 0.f, GetMaxStance());
	else if (Attribute == GetPoiseAttribute()) NewValue = FMath::Clamp(NewValue, 0.f, GetMaxPoise());
	else if (Attribute == GetHeatAttribute()) NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHeat());
	else if (Attribute == GetChillAttribute()) NewValue = FMath::Clamp(NewValue, 0.f, GetMaxChill());
	else if (Attribute == GetChargeAttribute()) NewValue = FMath::Clamp(NewValue, 0.f, GetMaxCharge());
	else if (Attribute == GetGrowthAttribute()) NewValue = FMath::Clamp(NewValue, 0.f, GetMaxGrowth());
	else if (Attribute == GetSoulsAttribute()) NewValue = FMath::Clamp(NewValue, 0.f, GetMaxSouls());
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
WR_ONREP(MaxHealthLost)
WR_ONREP(Void)
WR_ONREP(MaxVoid)
WR_ONREP(MaxChill)
WR_ONREP(Charge)
WR_ONREP(MaxCharge)
WR_ONREP(Growth)
WR_ONREP(MaxGrowth)
WR_ONREP(Souls)
WR_ONREP(MaxSouls)
WR_ONREP(BurnBuildup)
WR_ONREP(FrostBuildup)
#undef WR_ONREP
