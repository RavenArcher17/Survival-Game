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
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_BurnBuildup) FGameplayAttributeData BurnBuildup;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_FrostBuildup) FGameplayAttributeData FrostBuildup;

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
	WR_ATTRIBUTE_ACCESSORS(UWRAttributeSet, BurnBuildup)
	WR_ATTRIBUTE_ACCESSORS(UWRAttributeSet, FrostBuildup)

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
	UFUNCTION() void OnRep_BurnBuildup(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_FrostBuildup(const FGameplayAttributeData& Old);
};
