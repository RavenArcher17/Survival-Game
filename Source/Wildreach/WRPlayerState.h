#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "WRPlayerState.generated.h"

class UAbilitySystemComponent;
class UWRAttributeSet;
class UWRPathComponent;

/** Owns the survivor's ability system, attributes and path progress so they persist across death. */
UCLASS()
class WILDREACH_API AWRPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AWRPlayerState();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return AbilitySystem; }
	UWRAttributeSet* GetAttributes() const { return Attributes; }
	UWRPathComponent* GetPaths() const { return Paths; }

	/** Basic weapon abilities are granted once per player, not per pawn. */
	bool bDefaultAbilitiesGranted = false;

private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UAbilitySystemComponent> AbilitySystem;
	UPROPERTY() TObjectPtr<UWRAttributeSet> Attributes;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UWRPathComponent> Paths;
};
