#pragma once

// Farming and food (FULL-GAME 6.2; showcase-notes cooks/build-farming.md, after survival-crafting 2.7-2.11): biome
// crops in plots, water and greenhouses, farm animals in pens, bee hives, fish traps, salt pans, the kitchen's
// discovery cooking and sated meals, and the spoilage sweep. Everything it reads is data: Data/farming.json (crops,
// plots, animals, the hive, wild seeds), Data/cooking.json (food groups and dishes), items.json (keeps, sated) and
// tuning "farm" / "food". The server runs it all; plots, pens and stations replicate what everyone sees.

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "WRInventory.h"
#include "WRFarm.generated.h"

class AWRBuildPiece;
class AWRCreature;
class AWRPlayerController;
class AWRStation;
class APlayerState;
class FJsonObject;
struct FWRPieceDef;

namespace WRBiome
{
	/** A biome whose land is in the game (tuning world.liveBiomes): only those biomes' crops, animals, fish, baits and
	 *  pieces show. Adding a biome's land is adding its id there. */
	WILDREACH_API bool IsLive(FName Biome);
	/** "the volcanic slopes", "the frozen tundra"... (farming.json biomeNames). */
	WILDREACH_API FString Name(FName Biome);
}

/** One crop (farming.json crops). */
struct FWRCropDef
{
	FName Id;
	FString Name;
	FName Biome;              // its home: grows only there (a greenhouse or Grafting aside); "any" = everywhere
	bool bRare = false;       // 2 h, few, few seeds back
	TArray<FName> Plots;      // which plot types it takes: soil, ashpit, paddy, kelp, log
	FName Needs;              // light (default), dark, windy, withered
	FName Seed, Produce;
	FIntPoint Yield = FIntPoint(9, 12);
	FIntPoint SeedsBack = FIntPoint(9, 11);
	/** Its look (FWRFarmLook): the plant's form and colours, its height when ripe (cm). */
	FName Form;
	FLinearColor Leaf = FLinearColor(0.1f, 0.3f, 0.06f), Stem = FLinearColor(0.15f, 0.25f, 0.06f), Fruit = FLinearColor(0.6f, 0.05f, 0.03f), Flower = FLinearColor(0.9f, 0.9f, 0.8f);
	float Height = 60.f;
	/** Harvesting it screams (the mandrake): aggro in this radius (cm) and a stagger, unless the harvester has earplugs. */
	float Scream = 0.f;
	bool IsLive() const;
};

/** One farm animal (farming.json animals; the species is the creature id). */
struct FWRAnimalDef
{
	FName Species;
	struct FProduct { FName Item; float Every = 600.f; bool bShear = false; bool bGentle = true; };
	TArray<FProduct> Products;
	TArray<FName> Feed;       // items (or "crop" = any crop's produce, "spoiled" = spoiled food)
	FName Prize;              // Prize stock's rare product
	bool Eats(FName Item) const;
};

/** One dish (cooking.json dishes): where it's cooked, the pattern its 1-4 ingredients fill. */
struct FWRDishDef
{
	FName Id;                 // the item it makes
	FName Station;            // cooking_pot / oven
	/** Each entry: the groups or item ids that fill it ("Meat", "fire_pepper", "Dairy|Water"). */
	TArray<TArray<FName>> Pattern;
	int32 Count = 1;
	float Seconds = 20.f;
	FName Biome;              // a biome's dish shows only while that biome is live
	int32 Specific() const;   // how many entries name an item (more specific wins)
};

class WILDREACH_API FWRFarmDefs
{
public:
	static FWRFarmDefs& Get();

	const FWRCropDef* Crop(FName Id) const;
	const FWRCropDef* CropForSeed(FName Seed) const;
	const FWRCropDef* CropForProduce(FName Produce) const;
	const TArray<FWRCropDef>& Crops() const { return CropList; }
	const FWRAnimalDef* Animal(FName Species) const;
	const TArray<FWRAnimalDef>& Animals() const { return AnimalList; }
	/** Wild seeds a shrub of this biome gives: seed, chance a blow. */
	const TArray<TPair<FName, float>>* WildSeeds(FName Biome) const { return Wild.Find(Biome); }

	/** Cooking: an item's food group (Meat, Fish, Grain, Veg, Fruit, Herb, Dairy, Sweet, Mushroom, Water; none = not an ingredient). */
	FName GroupOf(FName Item) const;
	const TArray<FWRDishDef>& Dishes() const { return DishList; }
	const FWRDishDef* Dish(FName Id) const;
	/** Which dish these ingredients make at this station: the most specific pattern they fill one to one (null: none, Grey mush). */
	const FWRDishDef* Match(FName Station, const TArray<FName>& Ingredients) const;
	/** The dish's pattern in words ("Meat + Veg + Veg + Water"); Hidden entries (an unknown item) as "?". */
	FString PatternText(const FWRDishDef& Dish, bool bHideItems) const;
	/** Is this entry filled by this item? */
	bool Fills(const TArray<FName>& Entry, FName Item) const;

private:
	FWRFarmDefs();
	TArray<FWRCropDef> CropList;
	TArray<FWRAnimalDef> AnimalList;
	TMap<FName, TArray<TPair<FName, float>>> Wild;
	TMap<FName, FName> Groups;
	TArray<FWRDishDef> DishList;
};

/** A piece's farm state the prompt reads (replicated in AWRBuildPiece::FarmFlags). */
namespace WRFarmFlag
{
	constexpr uint8 Watered = 1 << 0;
	constexpr uint8 Fertilised = 1 << 1;
	constexpr uint8 Greenhouse = 1 << 2;
	constexpr uint8 Stopped = 1 << 3;       // not growing / not producing (FarmStop says why)
	constexpr uint8 Pollinated = 1 << 4;
	constexpr uint8 Grafted = 1 << 5;
	constexpr uint8 Companion = 1 << 6;
	constexpr uint8 Working = 1 << 7;       // a station / pan / hive / trap is at work (smoke, a fire)
}

/** Why a plot stopped (AWRBuildPiece::FarmStop). */
enum class EWRFarmStop : uint8
{
	None,
	WrongBiome,
	WrongPlot,
	NoDaylight,
	NeedsDark,
	NeedsWind,
	NeedsWithered,
	Ashfall,
	Hungry,         // pens: an empty trough
	NoFlowers,      // hives
	NoSun,          // salt pans at night
	NotOnBeach,
	Dry,            // fish trap out of the water
	Full,
};

/** Server rules for the farm pieces (plots, pens, hives, traps, salt pans) and the farm items. */
struct WILDREACH_API FWRFarm
{
	/** E on a farm piece: plant / fertilise / harvest, fill a trough / collect, etc. The message for the player. */
	static FString Interact(AWRBuildPiece* Piece, AWRPlayerController* Player);
	/** "Use" on an item: a seed next to an empty plot plants it, a recipe scrap is read, feed next to a pen fills its
	 *  trough. Empty when the item isn't one of those (the inventory goes on as before). */
	static FString UseItem(UWRInventoryComponent* Inventory, int32 SlotIndex);
	/** Plant this seed in that plot for this player. */
	static FString Plant(AWRBuildPiece* Plot, AWRPlayerController* Player, FName Seed);
	/** What a plot / pen / hive / trap / pan says on its prompt, from replicated state (any machine). */
	static FString Describe(const AWRBuildPiece* Piece, const UObject* WorldContext);
	/** A shrub of this biome was hit: maybe a seed of its crops (farming.json wildSeeds). */
	static void WildSeeds(UWRInventoryComponent* Inventory, FName Biome);
	/** Why this farm piece can't be placed here (biome, water depth, beach); empty = fine. */
	static FString PlaceCheck(const UObject* WorldContext, const FWRPieceDef& Def, const FVector& Location);
	/** A piece that shows only when its biome / creature exists (crops of unbuilt biomes, the hive before Hivebacks). */
	static bool IsPieceLive(const FWRPieceDef& Def);
	/** How fast this plot's crop grows now (0 = stopped, with Why), and its flags. Server. */
	static float GrowthRate(const AWRBuildPiece* Plot, EWRFarmStop& Why, uint8& Flags);
	/** The station a player cooks discovery dishes at (a pot or an oven; a campfire with Camp chef). */
	static bool IsCookStation(FName StationType, const AActor* Player);
	/** Server: cook these bag slots at that station (the Cook screen's "cook" command). Message for the player. */
	static FString Cook(AWRPlayerController* Player, AWRStation* Station, const TArray<int32>& BagSlots);
	/** Server: cook a known dish again, its ingredients picked from the bag (stalest first). */
	static FString CookKnown(AWRPlayerController* Player, AWRStation* Station, FName Dish);
	/** Ingredients from the bag that fill this dish's pattern (slot indices), or empty. */
	static TArray<int32> PickIngredients(const TArray<FWRItemStack>& Bag, const FWRDishDef& Dish);
	/** A freshly caught legendary: once a real day per player (false = it got away today), and its first-catch trophy. */
	static bool LegendAllowed(const APlayerState* Angler, FName Fish);
	static void LegendCaught(APlayerState* Angler, FName Fish, bool& bFirst);
};

/** What a player knows and is sated by (on the PlayerState; saved in the player JSON). */
UCLASS(ClassGroup=(Survival))
class WILDREACH_API UWRFoodComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWRFoodComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	static UWRFoodComponent* For(const AActor* PawnOrState);
	/** Server: hold hunger off for Minutes more (the longer of what's left and this; Cooking's every level adds 1%). */
	static void Sate(const AActor* PawnOrState, float Minutes);
	/** Hunger drain multiplier while sated (0) or not (1). */
	float HungerMult() const;
	float SatedSecondsLeft() const;

	/** Dishes found by cooking them (one-click from then on), and ones a scrap hinted at. */
	UPROPERTY(Replicated) TArray<FName> Known;
	UPROPERTY(Replicated) TArray<FName> Hinted;
	/** Server time hunger starts draining again. */
	UPROPERTY(Replicated) float SatedUntil = 0.f;
	/** The last discovery (the card that slides in), and when. */
	UPROPERTY(Replicated) FName LastDiscovery;
	UPROPERTY(Replicated) float DiscoveredAt = -100.f;

	/** Server: crops harvested at least once (the first one's XP), legends caught (ever) and the day each last bit. */
	TSet<FName> CropsHarvested;
	TSet<FName> LegendsEver;
	TMap<FName, int32> LegendDay;

	/** Server: a new dish; true when it wasn't known. */
	bool Discover(FName Dish);
	TSharedRef<FJsonObject> ToJson() const;
	void FromJson(const TSharedPtr<FJsonObject>& Json);
};

/**
 * The farm's clock (server): every plot's growth, the water reaching plots through wells and channels, pens and
 * hives producing, traps catching, salt pans drying, ice boxes burning ice, and the freshness sweep over every bag,
 * chest, tray and dropped bag. Pieces register themselves; nothing ticks per item.
 */
UCLASS()
class WILDREACH_API UWRFarmSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UWRFarmSubsystem* Get(const UObject* WorldContext);
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return !IsTemplate(); }

	void Register(AWRBuildPiece* Piece);
	void Unregister(AWRBuildPiece* Piece);
	/** Is there water for crops within Radius of this point (natural water, a well, a fed channel)? */
	bool IsWateredAt(const FVector& Point, float Radius) const;
	/** Is a working hive within Radius? */
	bool HasHiveNear(const FVector& Point, float Radius) const;
	/** A plot of a different crop within Radius (Companion planting). */
	bool HasOtherCropNear(const AWRBuildPiece* Plot, float Radius) const;
	/** The pace food keeps at in this container piece (chest 1, tundra 0.5, ice box 0.2 with ice). */
	float ContainerMult(const AWRBuildPiece* Piece) const;

private:
	TArray<TWeakObjectPtr<AWRBuildPiece>> Pieces;
	/** Each animal's clocks, one per product (seconds banked), and its young's growing. */
	TMap<TWeakObjectPtr<AWRCreature>, TArray<float>> AnimalClocks;
	TMap<TWeakObjectPtr<AWRBuildPiece>, float> BreedClock;
	float GrowClock = 0.f, WaterClock = 0.f, ProduceClock = 0.f, SpoilClock = 0.f;

	void TickPlots(float Dt);
	void TickWater();
	void TickProducers(float Dt);
	void TickPen(AWRBuildPiece* Pen, float Dt);
	void TickSpoil();
};
