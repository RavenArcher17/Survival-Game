#include "WRPlayerState.h"
#include "WRLifeSkills.h"
#include "WRAttributeSet.h"
#include "WRInventory.h"
#include "WRPathComponent.h"
#include "AbilitySystemComponent.h"

AWRPlayerState::AWRPlayerState()
{
	// GAS on a PlayerState needs a high update rate or predicted abilities feel laggy.
	SetNetUpdateFrequency(100.f);

	AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystem->SetIsReplicated(true);
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	Attributes = CreateDefaultSubobject<UWRAttributeSet>(TEXT("Attributes"));
	Paths = CreateDefaultSubobject<UWRPathComponent>(TEXT("Paths"));
	Inventory = CreateDefaultSubobject<UWRInventoryComponent>(TEXT("Inventory"));
	LifeSkills = CreateDefaultSubobject<UWRLifeSkillsComponent>(TEXT("LifeSkills"));
}
