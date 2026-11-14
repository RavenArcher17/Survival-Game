#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WRLifeSkills.generated.h"

class FJsonObject;

struct FWRPerkDef
{
	FName Id;
	FString Name;
	FString Text;
	FName Stat;      // what it adds to (UWRLifeSkillsComponent::Stat), read where its system is
	float Value = 0.f;
	/** The system it needs that isn't in the game yet ("diving"): not shown, can't be chosen. Empty = it works. */
	FString WaitsFor;
	bool IsLive() const { return WaitsFor.IsEmpty(); }
};

struct FWRLifeSkillDef
{
	FName Id;
	FString Name;
	FString Group;
	FString LevelsBy;
	FString EveryLevelText;
	FName EveryLevelStat;
	float PerLevel = 0.f;
	/** Perk choices at 25, 50, 75, 100 (two options each; positions are what saves keep). */
	TArray<TArray<FWRPerkDef>> Tiers;
	/** How many of a tier's perks are in the game (0: the tier isn't shown or offered). */
	int32 LiveCount(int32 Tier) const
	{
		int32 N = 0;
		if (Tiers.IsValidIndex(Tier)) for (const FWRPerkDef& P : Tiers[Tier]) N += P.IsLive() ? 1 : 0;
		return N;
	}
};

class WILDREACH_API FWRLifeSkillRegistry
{
public:
	static FWRLifeSkillRegistry& Get();
	const FWRLifeSkillDef* Find(FName Id) const;
	const TArray<FWRLifeSkillDef>& All() const { return Defs; }
	static constexpr int32 TierLevels[4] = { 25, 50, 75, 100 };

private:
	FWRLifeSkillRegistry();
	TArray<FWRLifeSkillDef> Defs;
};

/** Pure rules (tests). */
struct WILDREACH_API FWRLifeSkillRules
{
	/** XP needed to go from Level to Level + 1 (50 + 5n). */
	static int32 XPToNext(int32 Level);
	static constexpr int32 MaxLevel = 100;
};

USTRUCT()
struct FWRSkillProgress
{
	GENERATED_BODY()

	UPROPERTY() FName Skill;
	UPROPERTY() int32 Level = 1;
	/** XP into the current level. */
	UPROPERTY() int32 XP = 0;
	/** Per perk tier: -1 not chosen, 0 or 1 = which option. */
	UPROPERTY() TArray<int8> Perks;
};

/** The 15 life skills on a player (lives on the PlayerState, saved with the player). */
UCLASS(ClassGroup=(Wildreach), meta=(BlueprintSpawnableComponent))
class WILDREACH_API UWRLifeSkillsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWRLifeSkillsComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** From a pawn, controller or player state. */
	static UWRLifeSkillsComponent* FindFor(const AActor* Actor);
	/** Sum of a stat over every skill's levels and chosen perks (0 when there's no player). */
	static float StatFor(const AActor* Actor, FName Stat);
	/** Server: award XP to a skill from an action (amount from tuning lifeSkills.xp.<key>), scaled by server.xpMultiplier. */
	static void Award(const AActor* Actor, FName Skill, const TCHAR* XPKey, float Fallback, float Times = 1.f);

	void AwardXP(FName Skill, float Amount);
	int32 GetLevel(FName Skill) const;
	float Stat(FName Stat) const;
	FString ChoosePerk(FName Skill, int32 TierLevel, int32 Option);
	const TArray<FWRSkillProgress>& GetSkills() const { return Skills; }
	const FWRSkillProgress* FindProgress(FName Skill) const;
	/** Dev: set a level outright. */
	void DevSetLevel(FName Skill, int32 Level);

	TSharedRef<FJsonObject> ToJson() const;
	void FromJson(const TSharedPtr<FJsonObject>& Json);

private:
	FWRSkillProgress& Progress(FName Skill);
	UPROPERTY(Replicated) TArray<FWRSkillProgress> Skills;
	float Fraction = 0.f;  // sub-1 XP carried between awards
	TMap<FName, float> Carry;
};
