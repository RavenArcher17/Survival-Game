#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "WRBeds.h"
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
	/** Rest, Rested, the held light, darkness, weather on you, corruption (WRSurvival). */
	class UWRSurvivalComponent* GetSurvival() const { return Survival; }
	/** Dishes known and hinted, sated, crops and legends (WRFarm). */
	class UWRFoodComponent* GetFood() const { return Food; }

	/** Basic weapon abilities are granted once per player, not per pawn. */
	bool bDefaultAbilitiesGranted = false;

	/** Server: the bed/bedroll this player wakes at by default (WRBeds; falls back to a camp). */
	TWeakObjectPtr<AActor> RespawnPoint;
	/** Server: named bosses this player has felled (saved): each one's first-kill drop comes once. */
	TSet<FName> BossKills;
	/** Blueprints this survivor has read (saved; cooks/build-crafting.md 2.9): their recipes are open to them for good.
	 *  Only their own client gets them (the Crafting tab). */
	UPROPERTY(Replicated) TArray<FName> KnownBlueprints;
	/** Server: the shrine-stones by boss arenas this player has rested at (wake choices on the death screen). */
	TArray<TWeakObjectPtr<AActor>> BoundStones;
	/** The beds you've set and your clan's, for your map and death screen (only your own client gets them). */
	UPROPERTY(Replicated) TArray<FWRBedSpot> Beds;
	/** Server: where the death screen offered to wake (in its order), what's picked, and the body's half height. */
	TArray<TWeakObjectPtr<AActor>> WakeActors;
	int32 WakePick = INDEX_NONE;
	float WakeHalfHeight = 90.f;
	/** Server: dead and waiting to wake; from this world time Wake is allowed (< 0: not waiting). */
	double WakeReadyAt = -1.0;
	/** Server: beds resting after you woke at them, until this world time. */
	TMap<TWeakObjectPtr<AActor>, double> BedRestUntil;

	/** Server: artifact modifiers active (Full release feature; unlocks hidden boss phase 3). Dev: WRArtifacts 1. */
	bool bArtifactModifiers = false;

	/** Server: body picked in the menu (?Body= on the join URL); each new pawn takes it (AWRCharacter::BodyStyle). */
	FName BodyStyle;
	/** Server: the face and build from the menu (?Look=<face>.<build>, the creator writes it) and the hide-helmet option. */
	uint8 HeroFace = 0;
	uint8 HeroBuild = 1;
	bool bHideHelmet = false;

	/** Replicated so clients apply clan rules too (lock-on, PvP). Server keeps it in sync with FWRClans. */
	UPROPERTY(Replicated) FString ClanName;
	/** Your own clan for the Clan tab (only your own client gets them; FWRClans::RefreshPlayerStates fills them): its
	 *  leader and members, the players it has invited, and the clans that have invited you. */
	UPROPERTY(Replicated) FString ClanLeader;
	UPROPERTY(Replicated) TArray<FString> ClanMembers;
	UPROPERTY(Replicated) TArray<FString> ClanInvited;
	UPROPERTY(Replicated) TArray<FString> ClanInvitesToMe;
	/** Each of those clans' leader (the same order). */
	UPROPERTY(Replicated) TArray<FString> ClanInviteFrom;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UAbilitySystemComponent> AbilitySystem;
	UPROPERTY() TObjectPtr<UWRAttributeSet> Attributes;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UWRPathComponent> Paths;
	UPROPERTY(VisibleAnywhere) TObjectPtr<class UWRInventoryComponent> Inventory;
	UPROPERTY(VisibleAnywhere) TObjectPtr<class UWRLifeSkillsComponent> LifeSkills;
	UPROPERTY(VisibleAnywhere) TObjectPtr<class UWRSurvivalComponent> Survival;
	UPROPERTY(VisibleAnywhere) TObjectPtr<class UWRFoodComponent> Food;
};
