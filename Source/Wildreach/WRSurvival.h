#pragma once

// Survival, FULL-GAME 6.1 (cooks/build-survival.md, following cooks/survival-crafting.md 2.2-2.6):
//  - Rest and Rested: E on your bed lies you down; a finished 10 s sleep is a Rest (Decay back, corruption halved,
//    OnRest for the paths that hang off it) and starts Rested (stamina regen, slower drains, life-skill XP), longer
//    for a furnished room (comfort: decor under a roof near the bed, best piece per group).
//  - Darkness: a player in a cave (or a dark biome at night) with no light reaching them is unlit, and dark-hunters
//    smell them from twice as far. Lights: placed torches and braziers, Hearths, glowing creatures, and the held
//    torch / horn lantern in the new Light slot.
//  - Weather (Data/weather.json): each biome rolls every 30 min; signs 60 s ahead; every effect is a gear check or a
//    visibility change with a counter (no temperature meter, Raven's rule).
//  - Corruption in the dark biomes: a 0-100 meter with steps of small penalties, or small path-damage gains for the
//    matching path.
// The server decides everything; the component replicates what the HUD, the hero's look and the sound read.

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "WRSurvival.generated.h"

class AWRCharacterBase;
class AWRBuildPiece;
class AWRPlayerState;
class FJsonObject;
struct FWRCreatureDef;

/** Where things are on the map (one place, so the 12-wedge world swaps in here). */
namespace WRBiome
{
	/** The biome at a point: today's island is volcanic west of the ridge (sky.biomeCentre), tundra east of it. */
	WILDREACH_API FName At(const UObject* WorldContext, const FVector& Pos);
	/** Inside one of the island's caves (AWRCave's chamber or its entrance tunnel). */
	WILDREACH_API bool IsUnderground(const UObject* WorldContext, const FVector& Pos);
	/** Something solid within Height straight above (a roof, a floor, the rock of a cave). */
	WILDREACH_API bool IsCovered(const UObject* WorldContext, const FVector& Pos, float Height, const AActor* Ignore = nullptr);
	/** A built piece (a roof, a floor above) within Height straight above: what counts as "under a roof" for comfort. */
	WILDREACH_API bool IsUnderBuiltRoof(const UObject* WorldContext, const FVector& Pos, float Height, const AActor* Ignore = nullptr);
}

/** One weather of a biome (Data/weather.json). */
struct FWRWeatherKind
{
	FName Id;
	FString Name;
	/** The 60 s warning ("A blizzard is coming") and what it does / how to beat it (the HUD's line). */
	FString Coming, Does;
	float Chance = 0.f;
	FVector2D Minutes = FVector2D(10.f, 20.f);
	bool bDayOnly = false, bNightOnly = false;

	// Effects (only on players it reaches: outdoors, not countered).
	float Vision = 0.f;              // metres of visibility (the look); 0 = unchanged
	float ThirstMult = 1.f;
	bool bSunOnly = false;           // only in direct sun (a heat wave): shade or a roof is enough
	float StaminaRegenMult = 1.f;
	float RunMult = 1.f;             // deep snow
	float LightRadiusMult = 1.f;     // every light in the biome (corpse fog)
	float CorruptionMult = 1.f;
	float BurnMult = 1.f;            // a held torch burns this much faster (rain)
	float SwimStaminaMult = 1.f;
	float AggroMult = 1.f;           // these creatures notice you from further (fog ambushers)
	TArray<FName> AggroCreatures;
	bool bCropsPause = false;        // outdoor crop plots stop growing
	// Lightning strikes on the highest player in a cell (BotW's tell: a ground glow, then the bolt).
	bool bStrikes = false;
	float StrikeMinZ = 0.f;
	FVector2D StrikeEvery = FVector2D(20.f, 40.f);
	float StrikeWarn = 1.f, StrikeDamage = 60.f, StrikeStagger = 0.8f, StrikeRadius = 300.f, StrikeCell = 6000.f;
	// Gusts that shove exposed players along the storm's wind (a whistle GustWarn s before).
	bool bGust = false;
	FVector2D GustEvery = FVector2D(8.f, 15.f);
	float GustWarn = 1.f, GustPush = 900.f, GustMinZ = -1.0e7f;

	// Counters: any one of these and it doesn't touch you.
	float RoofWithin = 0.f;          // cm; something solid this close above you (0 = a roof doesn't help)
	TArray<FName> Resist;            // infused armour: frost / lava (FWRBiomeHazards::Resists)
	TArray<FName> Armor;             // worn armour ids
	TArray<FName> PathNodes;         // "path.node" you hold (Grounded, Sandwalker...)
	TArray<FName> Items;             // carried in the bag (a grounding charm)

	// Look and sound (each machine).
	FName Fx;                        // falling weather emitter at the camera (fx.json), scaled by FxRate
	float FxRate = 1.f;
	FName BaseFx;                    // the biome's everyday fall it thickens (snow / ash), x BaseRate
	float BaseRate = 1.f;
	float FogDensity = 0.f;
	FLinearColor FogColor = FLinearColor::Gray;
	float SkyDim = 0.f;
	FName Glyph;
};

/** A biome's survival row: its weathers, whether its nights or all of it are dark, and its corruption. */
struct FWRBiomeSurvival
{
	FName Biome;
	FString Name;
	bool bDarkNights = false;
	bool bUnderground = false;
	bool bCorrupt = false;
	/** The path that gains here instead of suffering (necromancy in the Graveyard, void in the Wasteland). */
	FName CorruptionPath;
	TArray<FWRWeatherKind> Kinds;
};

class WILDREACH_API FWRWeatherDefs
{
public:
	static FWRWeatherDefs& Get();
	void Load();
	const FWRBiomeSurvival* Biome(FName Id) const;
	const FWRWeatherKind* Kind(FName BiomeId, FName KindId) const;
	const TArray<FWRBiomeSurvival>& All() const { return Biomes; }

	float RollMinutes = 30.f;
	float WarnSeconds = 60.f;
	float RampSeconds = 20.f;
	float FadeSeconds = 30.f;

private:
	TArray<FWRBiomeSurvival> Biomes;
	bool bLoaded = false;
};

/** The weather of one biome right now (replicated in AWRGameState; times are the server's world seconds). */
USTRUCT()
struct FWRWeatherNow
{
	GENERATED_BODY()

	UPROPERTY() FName Biome;
	UPROPERTY() FName Kind;
	UPROPERTY() float WarnAt = 0.f;
	UPROPERTY() float StartAt = 0.f;
	UPROPERTY() float EndAt = 0.f;

	/** 0..1: ramps in over rampSeconds from StartAt, out over fadeSeconds before EndAt. */
	float Strength(float Now) const;
	/** 0..1 across the warning (the signs on the horizon), 1 once it has started, 0 when it's over. */
	float Approach(float Now) const;
	bool IsComing(float Now) const { return !Kind.IsNone() && Now >= WarnAt && Now < StartAt; }
	bool IsOn(float Now) const { return !Kind.IsNone() && Now >= StartAt && Now < EndAt; }
};

/**
 * The weather (server): every biome rolls every weather.rollMinutes; a hit is announced weather.warnSeconds ahead and
 * runs its minutes. Also what weather does to the world rather than to a player: lightning strikes (through ApplyHit)
 * and outdoor crop plots pausing. Everyone reads the state from AWRGameState.
 */
UCLASS()
class WILDREACH_API UWRWeatherSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableInEditor() const override { return false; }

	/** Server time as every machine sees it (the game state's). */
	static float Now(const UObject* WorldContext);
	/** The biome's weather row (or null), on any machine. */
	static const FWRWeatherNow* For(const UObject* WorldContext, FName Biome);
	/** The kind on now at full enough strength to act (>= 0.5), or null. */
	static const FWRWeatherKind* ActiveKind(const UObject* WorldContext, FName Biome, float* OutStrength = nullptr);

	/** Testing (WRCmd weather <biome> <kind> [minutes] / clear <biome>): start one now (no warning). */
	void Force(FName Biome, FName Kind, float Minutes);

private:
	void Roll(FName Biome, float At);
	void TickStrikes(float DeltaTime);
	void TickCrops(float DeltaTime);
	TMap<FName, float> NextRoll;
	struct FPendingStrike { FVector At; float LandsAt = 0.f; FName Biome; FName Kind; };
	TArray<FPendingStrike> Strikes;
	TMap<FIntPoint, float> NextStrikeInCell;
	float StrikeClock = 0.f;
	float CropClock = 0.f;
	TArray<TWeakObjectPtr<AWRBuildPiece>> CropPlots;
	float CropScan = 0.f;
};

/** Lights in the world (pieces, Hearths, glowing creatures, held lights) and what they reach (server and clients). */
UCLASS()
class WILDREACH_API UWRLightSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UWRLightSubsystem* Get(const UObject* WorldContext);
	/** A light reaches this point (weather can shrink every light in a biome). */
	bool IsLit(const FVector& Pos);
	/** Within a fire that stops corruption building (brazier, placed torch, Hearth). */
	bool IsWarded(const FVector& Pos);
	/** Near a light that halves corruption (a glowing creature, a lit lantern). */
	bool IsHalving(const FVector& Pos);

private:
	struct FSource { TWeakObjectPtr<AActor> Actor; float Radius = 0.f; bool bWard = false; bool bHalves = false; };
	void Refresh();
	bool Any(const FVector& Pos, TFunctionRef<bool(const FSource&)> Want);
	TArray<FSource> Sources;
	double ScannedAt = -100.0;
};

/** Comfort at a bed (survival-crafting 2.2): decor within comfort.radius under a built roof, best piece per group
 *  (pieces.json comfort), up to comfort.trophyMax different trophies, a near Hearth as a fireplace, capped. */
struct WILDREACH_API FWRComfort
{
	static int32 At(const AActor* Bed);
};

/** Called once for each finished sleep (Rest): Deathless re-arms, Void gives back max health. */
DECLARE_MULTICAST_DELEGATE_OneParam(FWROnRest, AWRPlayerState*);

/**
 * One player's survival state (on the PlayerState: it outlives deaths, saves with the player). The server ticks it
 * four times a second; what others need (sleeping, the held light) replicates to everyone, the meters to the owner.
 */
UCLASS()
class WILDREACH_API UWRSurvivalComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWRSurvivalComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** The survival component of a player's pawn or player state (null for creatures). */
	static UWRSurvivalComponent* For(const AActor* Actor);
	static FWROnRest OnRest;

	// --- sleep (server)
	/** E on a bed you may use: lie down, or the reason you can't ("" = asleep now). */
	FString TryStartSleep(AWRBuildPiece* Bed);
	/** Get up (moved, hit, hungry...); no Rest. */
	void Wake(const FString& Why);
	bool IsSleeping() const { return bSleeping; }
	float GetSleepEndsAt() const { return SleepEndsAt; }
	float GetSleepStartedAt() const { return SleepStartedAt; }

	// --- rested
	bool IsRested() const;
	/** Seconds of Rested left (this machine's view of the server clock). */
	float RestedSecondsLeft() const;
	int32 GetRestedMinutes() const { return RestedMinutes; }
	/** Comfort of the nearest usable bed within comfort.radius (-1: none near). */
	int32 GetComfortHere() const { return ComfortHere; }

	// --- light
	bool IsUnlit() const { return bUnlit; }
	FName GetLightItem() const { return LightItem; }
	bool IsLightOn() const { return bLightOn && !LightItem.IsNone(); }
	bool HoldsTorch() const;
	static bool HoldsTorch(const AActor* Character);
	/** Server: the Light slot changed (the inventory's ApplyEquipment); a newly worn light comes on. */
	void OnLightSlotChanged();
	/** Server: light it / put it out (the L key; the bag's use). "" when nothing to say. */
	FString ToggleLight();

	// --- corruption
	float GetCorruption() const { return Corruption; }
	/** 0-4: the steps of corruption.steps passed. */
	int32 CorruptionStep() const;
	bool MatchesCorruption() const;
	/** Server: lower (a cleansing drink) or raise it. */
	void AddCorruption(float Amount);

	// --- what the rest of the game asks (server and the owner's prediction)
	float StaminaRegenMult() const;
	float HungerDrainMult() const;
	float ThirstDrainMult() const;
	float SkillXpMult() const;
	float HealingTakenMult() const;
	float MaxStaminaPenalty() const;
	float ResourceGainMult() const;
	float PathDamageMult() const;
	float SwimStaminaMult() const;
	/** How much further this creature notices the player: dark-hunters on the unlit, the Marked, fog ambushers. */
	static float AggroMultFor(const AActor* Player, const FWRCreatureDef& Creature, FName CreatureBiome);

	// --- weather on this player (owner, for the HUD)
	FName GetWeatherBiome() const { return WeatherBiome; }
	bool IsWeatherCountered() const { return bWeatherCountered; }
	bool IsSheltered() const { return bSheltered; }
	FName GetBiomeHere() const { return BiomeHere; }

	TSharedRef<FJsonObject> ToJson() const;
	void FromJson(const TSharedPtr<FJsonObject>& Json);

	UFUNCTION() void OnRep_Sleeping();
	UFUNCTION() void OnRep_Light();

private:
	AWRPlayerState* GetPS() const;
	AWRCharacterBase* GetPawnCharacter() const;
	void TickSleep(AWRCharacterBase* Pawn, float Now);
	void FinishSleep(AWRCharacterBase* Pawn);
	void TickComfort(AWRCharacterBase* Pawn);
	void TickWeather(AWRCharacterBase* Pawn, float Now);
	void TickLight(AWRCharacterBase* Pawn, float DeltaTime);
	void TickCorruption(AWRCharacterBase* Pawn, float DeltaTime);
	void SetLightOn(bool bOn);
	void Notify(const FString& Message) const;

	// Sleeping (everyone: the body on the bed).
	UPROPERTY(ReplicatedUsing=OnRep_Sleeping) bool bSleeping = false;
	UPROPERTY(Replicated) float SleepStartedAt = 0.f;
	UPROPERTY(Replicated) float SleepEndsAt = 0.f;
	TWeakObjectPtr<AWRBuildPiece> SleepBed;
	FVector SleepSpot = FVector::ZeroVector;
	float SleepHealth = 0.f;
	int32 SleepComfort = 0;

	// Rested (owner).
	UPROPERTY(Replicated) float RestedUntil = 0.f;
	UPROPERTY(Replicated) int32 RestedMinutes = 0;
	UPROPERTY(Replicated) int32 ComfortHere = -1;
	/** Rested seconds carried in from the save until the pawn exists. */
	float LoadedRestedSeconds = 0.f;

	// Light (everyone: the torch in the hand).
	UPROPERTY(ReplicatedUsing=OnRep_Light) FName LightItem;
	UPROPERTY(ReplicatedUsing=OnRep_Light) bool bLightOn = false;
	UPROPERTY(Replicated) bool bUnlit = false;
	/** The save had the light put out: the Light slot's item comes back unlit. */
	bool bLoadedLightOff = false;

	// Corruption (owner).
	UPROPERTY(Replicated) float Corruption = 0.f;
	/** The corrupt biome it last built in: its path is the one that gains from it. */
	UPROPERTY(Replicated) FName CorruptionFrom;
	int32 LastCorruptionStep = 0;

	// Weather on this player (owner).
	UPROPERTY(Replicated) FName WeatherBiome;
	UPROPERTY(Replicated) FName BiomeHere;
	UPROPERTY(Replicated) bool bWeatherCountered = false;
	UPROPERTY(Replicated) bool bSheltered = false;
	// The active effects for the hooks (server; the owner reads them through the same functions but only the server's matter).
	float WThirst = 1.f, WRegen = 1.f, WRun = 1.f, WBurn = 1.f, WSwim = 1.f, WCorruption = 1.f;
	/** The next gust on this player (server time; < 0 none due) and whether its whistle has sounded. */
	float NextGustAt = -1.f;
	bool bGustWarned = false;

	float Clock = 0.f;
	float ComfortClock = 0.f;
};
