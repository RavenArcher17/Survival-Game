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
	/** How long one takes at a station (recipes.json "seconds"), before the world's crafting speed. */
	float Seconds = 2.f;
	/** A life-skill stat the maker needs (recipes.json "needsPerk": Bait maker's lures); none = anyone. Hidden without it. */
	FName NeedsStat;
	FString NeedsText;
	/** A dish of the discovery kitchen (Data/cooking.json): cooked from the Cook screen's pattern, not ordered here. */
	bool bDish = false;
	/** The land its ingredients come from (recipes.json "biome"): not listed until that land is in the game. */
	FName Biome;
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

/** What the crafter brings to a recipe (their skill and perks), taken when it's ordered: a station's queue goes on
 *  making it after they've walked off or logged out. */
struct FWRCrafterHand
{
	int32 SkillLevel = 0;     // smithing or armorcraft, for gear's quality
	bool bMinFine = false;    // a Master perk: gear is Fine or better
	float RollBonus = 0.f;    // weapons' stat roll
	float DoubleBrew = 0.f;   // alchemy's chance of a double batch
	bool bBulkCook = false;   // Bulk cook (cooking perk): food recipes make two
};

/** One made: the item (a piece of gear, or a stack), the skill it trains, and the "Crafted ..." line. */
struct FWRCraftedOne
{
	FWRItemStack Item;
	bool bGear = false;
	FName Skill;
	const TCHAR* XPKey = TEXT("craft");   // lifeskills.json's XP for it (XP: the number when it has none)
	float XP = 0.f;
	FString Text;
};

/** Server: making a recipe's output (shared by stations and crafting by hand). */
struct WILDREACH_API FWRCrafting
{
	/** Checks and takes the inputs, then makes the output (quality roll for gear). Returns the message for the player. */
	static FString Make(AWRPlayerController* Player, const FWRRecipe& Recipe, FName StationType, int32 Tier, FName Element);
	/** Recipes with station "hand": anywhere, no station needed (the crude tools). */
	static FString CraftByHand(AWRPlayerController* Player, FName RecipeId);
	/** Why this recipe can't be made Times over with this infusion from what the player carries (empty: it can). */
	static FString Shortfall(AWRPlayerController* Player, const FWRRecipe& Recipe, FName Element, int32 Times);
	/** Takes the inputs (and the essence) for Times. Check Shortfall first. */
	static void TakeInputs(AWRPlayerController* Player, const FWRRecipe& Recipe, FName Element, int32 Times);
	/** What the inputs (and essence) for Times are: a cancelled order's refund. */
	static TArray<TPair<FName, int32>> InputsFor(const FWRRecipe& Recipe, FName Element, int32 Times, float Discount = 0.f);
	/** Efficient smith / Efficient armorer (perks): the share of each material this player saves on this recipe (0..0.9). */
	static float MaterialDiscount(const AActor* Player, const FWRRecipe& Recipe);
	/** One of the recipe's inputs at a discount: each rounded down, never under 1. */
	static int32 DiscountedInput(int32 Amount, float Discount);
	/** The recipe's inputs for one, as this player pays them (the crafting screen's cost). */
	static TArray<TPair<FName, int32>> CostFor(const FWRRecipe& Recipe, float Discount);
	/** How much faster this player's orders run (1 = the world's speed): engineering's every-level bonus and Quick
	 *  hands / Quick fit on weapons / armor. Overclock is added when an order is queued (it burns wood). */
	static float SpeedFor(const AActor* Player, const FWRRecipe& Recipe);
	static FWRCrafterHand HandOf(AWRPlayerController* Player, const FWRRecipe& Recipe, FName StationType);
	/** Makes one (its inputs already taken): the quality roll for gear, alchemy's double batch. */
	static FWRCraftedOne MakeOne(const FWRRecipe& Recipe, FName StationType, int32 Tier, FName Element, const FWRCrafterHand& Hand);
	/** Gear: one per craft; the rest come as a stack. */
	static bool IsGear(const FWRRecipe& Recipe);
};

/** One order in a station's queue (AWRStation::Queue): a recipe to make Left more times, one after another. */
USTRUCT()
struct FWRCraftJob
{
	GENERATED_BODY()

	UPROPERTY() FName Recipe;
	UPROPERTY() FName Element;
	UPROPERTY() FString Owner;
	UPROPERTY() int32 Left = 0;
	UPROPERTY() int32 Made = 0;
	/** Seconds each takes (the recipe's over the world's crafting speed, when it was ordered). */
	UPROPERTY() float ItemSeconds = 1.f;
	/** The one under way is done at this server time; 0 while it waits (ItemLeft then has the seconds still to go). */
	UPROPERTY() double DoneAt = 0.0;
	UPROPERTY() float ItemLeft = 0.f;
	/** Why it waits: 0 it doesn't, 1 nowhere to put what it makes (its maker isn't near and the tray is full), 2 the
	 *  Hearth is broken. */
	UPROPERTY() uint8 Waiting = 0;
	/** The maker's material discount when ordered (what a cancel gives back) and whether Overclock burns wood for it
	 *  (one per item, taken when ordered, the unmade ones' back on a cancel). */
	UPROPERTY() float Discount = 0.f;
	UPROPERTY() bool bOverclocked = false;
	/** A dish's worth (a golden crop in it: 1.5): what each one made is worth when eaten. */
	UPROPERTY() float Worth = 1.f;
	FWRCrafterHand Hand;   // server only
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
	/** Server: health as a share of full (the Hearth's build piece carries the real number and calls this). */
	void SetHealthFraction(float Fraction) { Health = MaxHealth * FMath::Clamp(Fraction, 0.f, 1.f); }
	/**
	 * Server: the Hearth screen's Repair. Back to full, the Hearth and its build piece both, for half the piece's build
	 * cost scaled by how much is missing (a broken one costs the whole half). Only its owner and their clan.
	 */
	FString RepairFor(AWRPlayerController* Player);
	/** What RepairFor would take now (empty when it's at full health). */
	/** What mending it costs (For: the player paying; Quick repair halves it). */
	TArray<TPair<FName, int32>> RepairCost(const AActor* For = nullptr) const;
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
	/**
	 * Orders Times of a recipe: the inputs are taken now and it joins the queue, made one after another
	 * (craft.timeScale 0: made at once, as before 0.7).
	 */
	FString Craft(AWRPlayerController* Player, FName RecipeId, FName Element, int32 Times = 1);
	/**
	 * The kitchen (WRFarm::Cook): a dish (or Grey mush) cooked once, its ingredients already taken by the caller, on
	 * this station's queue like any order (Count made, Seconds long, Worth carried to the food). Starts with "!" when
	 * it can't (the queue is full), else the queued line.
	 */
	FString CookOrder(AWRPlayerController* Player, FName Dish, int32 Count, float Seconds, float Worth);
	/** Is it burning (a campfire, a pot's fire, the oven, the smokehouse at work)? Every machine. */
	bool IsWorking() const { return Queue.Num() > 0; }
	/** Calls off an order (its maker, or the base's owner and clan): what wasn't made yet comes back. */
	FString CancelJob(AWRPlayerController* Player, int32 Index);
	/** Everything in the tray that fits into the player's bag. */
	FString TakeOutput(AWRPlayerController* Player);
	FString Repair(AWRPlayerController* Player, EWREquipSlot Slot);
	FString Upgrade(AWRPlayerController* Player, EWREquipSlot Slot);

	/**
	 * Crafting time (0.7): what it's making, first under way. It keeps going with nobody at it; each one goes into its
	 * maker's bag when they're within craft.deliverRange, else into the tray (Output) to be taken. Saved with its piece.
	 */
	UPROPERTY(Replicated) TArray<FWRCraftJob> Queue;
	UPROPERTY(Replicated) TArray<FWRItemStack> Output;
	/** How far the first order is through the one it's making, 0..1 (any machine: from the server's clock). */
	float JobProgress() const;
	/** Seconds one of a recipe takes here and now (the recipe's, over the world's crafting speed; 0: at once). */
	static float SecondsFor(const FWRRecipe& Recipe, const AActor* Player = nullptr);
	/** Save / load (its build piece's JSON). */
	void QueueToJson(TSharedRef<class FJsonObject> O) const;
	void QueueFromJson(const TSharedPtr<class FJsonObject>& O);
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION() void OnRep_Type();
	bool CanUse(AWRPlayerController* Player, FString& OutWhy) const;
	/** The base's owner and clan (the piece it stands on), or anyone at a station that isn't a building's. */
	bool CanManage(const FString& Player) const;
	/** Server: makes the first order's next one if it's due, and starts the next. */
	void AdvanceQueue();

	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
	/** A forge's fire (other stations stay dark). */
	UPROPERTY(VisibleAnywhere) TObjectPtr<class UPointLightComponent> Glow;
	/** Its modelled prop where it's drawn (it collides as a box fitted to the prop, FitCollision), and the type it shows. */
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Visual;
	FName LookType;
	FName FitType;   // the station type its collision box was fitted to (FWRBuildLook::FitCollision)
};
