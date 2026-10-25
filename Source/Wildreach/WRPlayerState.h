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
	class UWRInventoryComponent* GetInventory() const { return Inventory; }
	class UWRLifeSkillsComponent* GetLifeSkills() const { return LifeSkills; }

	/** Basic weapon abilities are granted once per player, not per pawn. */
	bool bDefaultAbilitiesGranted = false;

	/** Server: bed/bedroll this player respawns at (falls back to a biome spawn). */
	TWeakObjectPtr<AActor> RespawnPoint;

	/** Server: artifact modifiers active (Full release feature; unlocks hidden boss phase 3). Dev: WRArtifacts 1. */
	bool bArtifactModifiers = false;

	/** Server: body picked in the menu (?Body= on the join URL); each new pawn takes it (AWRCharacter::BodyStyle). */
	FName BodyStyle;

	/** Replicated so clients apply clan rules too (lock-on, PvP). Server keeps it in sync with FWRClans. */
	UPROPERTY(Replicated) FString ClanName;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UAbilitySystemComponent> AbilitySystem;
	UPROPERTY() TObjectPtr<UWRAttributeSet> Attributes;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UWRPathComponent> Paths;
	UPROPERTY(VisibleAnywhere) TObjectPtr<class UWRInventoryComponent> Inventory;
	UPROPERTY(VisibleAnywhere) TObjectPtr<class UWRLifeSkillsComponent> LifeSkills;
};
