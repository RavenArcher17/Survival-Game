#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "WRAttributeSet.generated.h"

#define WR_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * Every attribute a survivor or creature has. Written only on the server (by the
 * combat library and status component); replicated to everyone so HUDs and
 * health bars read the same numbers.
 */
UCLASS()
class WILDREACH_API UWRAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UWRAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Health) FGameplayAttributeData Health;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxHealth) FGameplayAttributeData MaxHealth;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Stamina) FGameplayAttributeData Stamina;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxStamina) FGameplayAttributeData MaxStamina;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Poise) FGameplayAttributeData Poise;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxPoise) FGameplayAttributeData MaxPoise;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Hunger) FGameplayAttributeData Hunger;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Thirst) FGameplayAttributeData Thirst;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Heat) FGameplayAttributeData Heat;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxHeat) FGameplayAttributeData MaxHeat;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Chill) FGameplayAttributeData Chill;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxChill) FGameplayAttributeData MaxChill;
	/** The Lightning path's Charge (cooks/biome-lightning.md 2.18.2): a builder that fills fast and empties fast. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Charge) FGameplayAttributeData Charge;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxCharge) FGameplayAttributeData MaxCharge;
	/** The Nature path's Growth (cooks/biome-forest.md 2.17): a spender that regrows by the ground around you. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Growth) FGameplayAttributeData Growth;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxGrowth) FGameplayAttributeData MaxGrowth;
	/** The Necromancy path's Souls (cooks/biome-graveyard.md 2.17): collected from kills, spent to raise and empower the dead. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Souls) FGameplayAttributeData Souls;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxSouls) FGameplayAttributeData MaxSouls;
	/** Max health taken by the Void (cooks/biome-void.md 2.8, 2.17): rift zones, Gnawer latches, Vorhollow's bite and your
	 *  own void abilities. Health can't rise past MaxHealth - MaxHealthLost (the grey band); Rest gives it all back. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxHealthLost) FGameplayAttributeData MaxHealthLost;
	/** The Void path's bar (shown as "Void", never "Corruption": that's the world meter). 0..MaxVoid. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Void) FGameplayAttributeData Void;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxVoid) FGameplayAttributeData MaxVoid;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_BurnBuildup) FGameplayAttributeData BurnBuildup;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_FrostBuildup) FGameplayAttributeData FrostBuildup;
	/** Creature and boss stance (COMBAT-AND-LESSONS 1.4): hidden, so it lives on the server only; a break shows by the body
	 *  (Status.StanceBroken replicates). */
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData Stance;
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData MaxStance;

	WR_ATTRIBUTE_ACCESSORS(UWRAttributeSet, Health)
	WR_ATTRIBUTE_ACCESSORS(UWRAttributeSet, MaxHealth)
	WR_ATTRIBUTE_ACCESSORS(UWRAttributeSet, Stamina)
	WR_ATTRIBUTE_ACCESSORS(UWRAttributeSet, MaxStamina)
	WR_ATTRIBUTE_ACCESSORS(UWRAttributeSet, Poise)
	WR_ATTRIBUTE_ACCESSORS(UWRAttributeSet, MaxPoise)
	WR_ATTRIBUTE_ACCESSORS(UWRAttributeSet, Hunger)
	WR_ATTRIBUTE_ACCESSORS(UWRAttributeSet, Thirst)
	WR_ATTRIBUTE_ACCESSORS(UWRAttributeSet, Heat)
	WR_ATTRIBUTE_ACCESSORS(UWRAttributeSet, MaxHeat)
	WR_ATTRIBUTE_ACCESSORS(UWRAttributeSet, Chill)
	WR_ATTRIBUTE_ACCESSORS(UWRAttributeSet, MaxChill)
	WR_ATTRIBUTE_ACCESSORS(UWRAttributeSet, Charge)
	WR_ATTRIBUTE_ACCESSORS(UWRAttributeSet, MaxCharge)
	WR_ATTRIBUTE_ACCESSORS(UWRAttributeSet, Growth)
	WR_ATTRIBUTE_ACCESSORS(UWRAttributeSet, MaxGrowth)
	WR_ATTRIBUTE_ACCESSORS(UWRAttributeSet, Souls)
	WR_ATTRIBUTE_ACCESSORS(UWRAttributeSet, MaxSouls)
	WR_ATTRIBUTE_ACCESSORS(UWRAttributeSet, MaxHealthLost)
	WR_ATTRIBUTE_ACCESSORS(UWRAttributeSet, Void)
	WR_ATTRIBUTE_ACCESSORS(UWRAttributeSet, MaxVoid)
	/** The most health can be now: MaxHealth less what the Void took. */
	float GetEffectiveMaxHealth() const { return FMath::Max(0.f, GetMaxHealth() - GetMaxHealthLost()); }
	WR_ATTRIBUTE_ACCESSORS(UWRAttributeSet, BurnBuildup)
	WR_ATTRIBUTE_ACCESSORS(UWRAttributeSet, FrostBuildup)
	WR_ATTRIBUTE_ACCESSORS(UWRAttributeSet, Stance)
	WR_ATTRIBUTE_ACCESSORS(UWRAttributeSet, MaxStance)

private:
	UFUNCTION() void OnRep_Health(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_MaxHealth(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Stamina(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_MaxStamina(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Poise(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_MaxPoise(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Hunger(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Thirst(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Heat(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_MaxHeat(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Chill(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_MaxChill(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Charge(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_MaxCharge(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Growth(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_MaxGrowth(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Souls(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_MaxSouls(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_MaxHealthLost(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Void(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_MaxVoid(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_BurnBuildup(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_FrostBuildup(const FGameplayAttributeData& Old);
};
