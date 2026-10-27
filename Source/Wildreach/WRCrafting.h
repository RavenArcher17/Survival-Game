#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WRInventory.h"
#include "WRCrafting.generated.h"

class AWRPlayerController;
class UStaticMeshComponent;

/** One recipe from Data/recipes.json. */
struct FWRRecipe
{
	FName Id;
	FName Station;
	int32 Level = 1;
	TArray<TPair<FName, int32>> Inputs;
	FName Output;
	int32 Count = 1;
	bool bQuality = false;
};

class WILDREACH_API FWRRecipeRegistry
{
public:
	static FWRRecipeRegistry& Get();
	const FWRRecipe* Find(FName Id) const;
	TArray<const FWRRecipe*> ForStation(FName Station) const;
	/** Recipe that makes this item (for repair costs). */
	const FWRRecipe* ForOutput(FName ItemId) const;

	TArray<float> BaseOdds;
	float PerHearthLevel = 0.04f;
	float PerSkillLevel = 0.004f;
	float RollSpread = 0.1f;
	float RepairCostFraction = 0.3f;

private:
	FWRRecipeRegistry();
	TArray<FWRRecipe> Recipes;
};

/** Server: making a recipe's output (shared by stations and crafting by hand). */
struct WILDREACH_API FWRCrafting
{
	/** Checks and takes the inputs, then makes the output (quality roll for gear). Returns the message for the player. */
	static FString Make(AWRPlayerController* Player, const FWRRecipe& Recipe, FName StationType, int32 Tier, FName Element);
	/** Recipes with station "hand": anywhere, no station needed (the crude tools). */
	static FString CraftByHand(AWRPlayerController* Player, FName RecipeId);
};

/** Pure crafting math (tests). */
struct WILDREACH_API FWRCraftingRules
{
	/** Quality from a 0..1 random value; better odds with Hearth level and crafting skill. */
	static EWRQuality RollQuality(float Random01, int32 HearthLevel, int32 SkillLevel);
	/** Hearth tech level from the number of different major boss cores (Crafting tab: 0/1/2/4/6 -> 1..5). */
	static int32 TechLevelForCores(int32 DistinctCores);
	/** +N upgrade cap for a Hearth level (+2 per level). */
	static int32 UpgradeCap(int32 HearthLevel) { return HearthLevel * 2; }
	/** Shards for the next upgrade step (higher steps cost more). */
	static int32 UpgradeCost(int32 CurrentUpgrade) { return CurrentUpgrade + 1; }
};

/**
 * One per land claim. Feeding it boss cores raises its tech level, which sets the tier of every
 * station in the claim. Raiders can break it (stops crafting); its cores are never lost.
 */
UCLASS()
class WILDREACH_API AWRHearth : public AActor
{
	GENERATED_BODY()

public:
	AWRHearth();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	int32 GetTechLevel() const { return FWRCraftingRules::TechLevelForCores(Cores.Num()); }
	bool IsBroken() const { return Health <= 0.f; }
	float GetHealth() const { return Health; }
	const TArray<FName>& GetCores() const { return Cores; }

	/** Server: deposit every boss core the player carries. */
	FString DepositCores(AWRPlayerController* Player);
	/** Server: damage from raiders. */
	void TakeRaidDamage(float Amount);
	void Repair() { Health = MaxHealth; }
	void SetCores(const TArray<FName>& InCores) { Cores = InCores; }

	/** Nearest Hearth whose claim covers this point (null = no claim: tier 1). */
	static AWRHearth* FindCovering(const UWorld* World, const FVector& Location);

	UPROPERTY(EditAnywhere, Category="Hearth") float CoverRadius = 3000.f;
	UPROPERTY(EditAnywhere, Category="Hearth") float MaxHealth = 2000.f;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
	/** Its fire: a warm light, what a base looks like from afar at night. */
	UPROPERTY(VisibleAnywhere) TObjectPtr<class UPointLightComponent> Glow;
	UPROPERTY(Replicated) TArray<FName> Cores;
	UPROPERTY(Replicated) float Health = 2000.f;
};

/** Forge, armorer, alchemy table or workbench. Its tier is the covering Hearth's tech level. */
UCLASS()
class WILDREACH_API AWRStation : public AActor
{
	GENERATED_BODY()

public:
	AWRStation();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_Type, Category="Station") FName StationType = TEXT("workbench");

	int32 GetTier() const;
	FString GetDisplayName() const;
	static constexpr float UseRange = 350.f;

	// --- Server actions (validated) ---
	FString Craft(AWRPlayerController* Player, FName RecipeId, FName Element);
	FString Repair(AWRPlayerController* Player, EWREquipSlot Slot);
	FString Upgrade(AWRPlayerController* Player, EWREquipSlot Slot);

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION() void OnRep_Type();
	bool CanUse(AWRPlayerController* Player, FString& OutWhy) const;

	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
	/** A forge's fire (other stations stay dark). */
	UPROPERTY(VisibleAnywhere) TObjectPtr<class UPointLightComponent> Glow;
	/** Its modelled prop where it's drawn (the box above stays as the collision), and the type it shows. */
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Visual;
	FName LookType;
};
