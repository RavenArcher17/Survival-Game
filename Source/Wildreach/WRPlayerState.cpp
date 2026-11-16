#include "WRPlayerState.h"
#include "WRMapReveal.h"
#include "WRArtifacts.h"
#include "WRDeeds.h"
#include "WRWorldSystems.h"
#include "WRLifeSkills.h"
#include "WRAttributeSet.h"
#include "WRInventory.h"
#include "WRPathComponent.h"
#include "WRSurvival.h"
#include "WRFarm.h"
#include "WRGod.h"
#include "AbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"

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
	Survival = CreateDefaultSubobject<UWRSurvivalComponent>(TEXT("Survival"));
	Food = CreateDefaultSubobject<UWRFoodComponent>(TEXT("Food"));
	QuestLog = CreateDefaultSubobject<UWRQuestLog>(TEXT("QuestLog"));
	MapReveal = CreateDefaultSubobject<UWRMapRevealComponent>(TEXT("MapReveal"));
	Artifacts = CreateDefaultSubobject<UWRArtifactComponent>(TEXT("Artifacts"));
	Deeds = CreateDefaultSubobject<UWRDeedsComponent>(TEXT("Deeds"));
	LifeSkills = CreateDefaultSubobject<UWRLifeSkillsComponent>(TEXT("LifeSkills"));
	God = CreateDefaultSubobject<UWRGodComponent>(TEXT("God"));
}

void AWRPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRPlayerState, ClanName);
	DOREPLIFETIME_CONDITION(AWRPlayerState, ClanLeader, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AWRPlayerState, ClanMembers, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AWRPlayerState, ClanInvited, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AWRPlayerState, ClanInvitesToMe, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AWRPlayerState, ClanInviteFrom, COND_OwnerOnly);
	DOREPLIFETIME(AWRPlayerState, AccessRole);
	DOREPLIFETIME_CONDITION(AWRPlayerState, AdminBans, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AWRPlayerState, Beds, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AWRPlayerState, KnownBlueprints, COND_OwnerOnly);
}
