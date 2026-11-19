#pragma once

// World systems (cooks/build-world-systems.md, Data/world.json): shrines and fast travel, path masters and their quests,
// villages and factions, the traveling merchants, message stones, Starfall, the biome events (Eruption, Whiteout),
// opposite-biome invasions, the hidden sub-biomes and the summoning stones. The server runs them all from
// UWRWorldSystems and keeps them in the world's world_systems.json; what clients need replicates on AWRWorldState and
// the actors themselves; each player's quests, found shrines and standing ride on UWRQuestLog (their PlayerState).

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "WRCharacter.h"
#include "WRWorldSystems.generated.h"

class AWRPlayerController;
class AWRPlayerState;
class AWRCreature;
class AWRCharacterBase;
class FJsonObject;
class UPointLightComponent;
class UStaticMeshComponent;
struct FWRHit;

namespace WRWorld
{
	/** Data/world.json (loaded once, reloaded with the tuning). */
	WILDREACH_API TSharedPtr<FJsonObject> Data();
	/** A dotted path into world.json ("events.eruption"), or null. */
	WILDREACH_API TSharedPtr<FJsonObject> Obj(const FString& Path);
	WILDREACH_API float Num(const FString& Path, float Fallback);

	/** Server, from ApplyHit before anything lands: true = this blow does nothing (a path master, a merchant hit by a
	 *  survivor, a summoned boss sealed to another party). Also notes who hurt the invaders. */
	WILDREACH_API bool BlocksHit(const AWRCharacterBase* Target, const FWRHit& Hit);
	/** Server, from AWRCharacterBase::Die before the tags clear (quests: kills with a status; invasions; caravans). */
	WILDREACH_API void OnKilled(AWRCharacterBase* Victim, AActor* Killer);
	/** Server, when a wild creature becomes someone's tame. */
	WILDREACH_API void OnTamed(AWRCreature* Creature, const FString& Player);
	/** Server, when a survivor earns a boss's loot (the 10% rule): its effigy recipe is theirs from now on. */
	WILDREACH_API void OnBossCredit(AWRPlayerState* PS, FName BossId);
	/** What a biome event's plenty multiplies a gather by (1 most of the time). */
	WILDREACH_API float GatherScale(const UObject* WorldContext, FName ItemId, const FVector& At);
	/** The night spawn table runs in this biome now (night, or a whiteout by day). */
	WILDREACH_API bool IsNightTable(const UObject* WorldContext, FName Biome);
	/** The player a killer / hitter stands for: a survivor's name, a tame's owner, or empty. */
	WILDREACH_API FString PlayerOf(const AActor* Actor);
	/** Server: "world <verb> ..." commands from the screens (shrine, travel, quest, trade, msg, summon, cache, dev). */
	WILDREACH_API FString Command(AWRPlayerController* PC, const TArray<FString>& Args);
	/** Owning client, E pressed with no menu: a world thing in reach opens its screen or sends its command. */
	WILDREACH_API bool ClientInteract(AWRPlayerController* PC);
	/** Owning client: the interact prompt for the nearest world thing (target name, verb). */
	WILDREACH_API bool Prompt(const AWRPlayerController* PC, FString& OutTarget, FString& OutVerb);
	/** Server: a Scratch-stone was used from the bag. */
	WILDREACH_API FString UseScratchStone(AWRPlayerState* PS);
	/** sRGB triple [r,g,b] from a json field. */
	WILDREACH_API FColor Rgb(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, FColor Fallback);
	/** The biome row (world.json biomes.<id>). */
	WILDREACH_API TSharedPtr<FJsonObject> Biome(FName Id);
	/** Display name of an item id. */
	WILDREACH_API FString ItemName(FName Id);
}

/** One event on now (or warning), for every machine: HUD lines, the sky, music. */
USTRUCT()
struct FWRWorldEventRep
{
	GENERATED_BODY()

	UPROPERTY() FName Id;
	UPROPERTY() FName Biome;
	/** 0 warning, 1 on, 2 the after (cores cooling, the aurora night). */
	UPROPERTY() uint8 Stage = 0;
	/** Server time it starts (after the warning) and ends. */
	UPROPERTY() float StartAt = 0.f;
	UPROPERTY() float EndAt = 0.f;
	UPROPERTY() FVector Centre = FVector::ZeroVector;
};

/** The invasion under way (at most one). */
USTRUCT()
struct FWRInvasionRep
{
	GENERATED_BODY()

	UPROPERTY() bool bOn = false;
	UPROPERTY() FName From;
	UPROPERTY() FName To;
	UPROPERTY() FString Pass;
	/** 0 warning, 1 marching, 2 holding its target. */
	UPROPERTY() uint8 Stage = 0;
	UPROPERTY() float Until = 0.f;
	UPROPERTY() uint8 Left = 0;
	UPROPERTY() uint8 Total = 0;
	UPROPERTY() FName HeldShrine;
	UPROPERTY() FVector Target = FVector::ZeroVector;
};

/** What every machine knows of the world systems (one, always relevant). */
UCLASS(NotPlaceable)
class WILDREACH_API AWRWorldState : public AActor
{
	GENERATED_BODY()

public:
	AWRWorldState();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	static AWRWorldState* Get(const UObject* WorldContext);
	const FWRWorldEventRep* Event(FName Id) const;
	/** An event of this id on (stage 1) in this biome (any biome: None). */
	bool IsOn(FName Id, FName Biome = NAME_None) const;

	UPROPERTY(Replicated) TArray<FWRWorldEventRep> Events;
	UPROPERTY(Replicated) FWRInvasionRep Invasion;
	/** Biomes whose aurora night (after a whiteout) is tonight. */
	UPROPERTY(Replicated) TArray<FName> AuroraBiomes;

private:
	/** Client: the meteor streaks across the sky while Starfall's warning runs. */
	float StreakClock = 0.f;
	float TremorClock = 0.f;
};

/** A quest a player holds with one master. */
USTRUCT()
struct FWRQuestRep
{
	GENERATED_BODY()

	UPROPERTY() FName Master;
	/** Which of the master's quests (in order); past the last = all done. */
	UPROPERTY() int32 Index = 0;
	UPROPERTY() bool bTaken = false;
	UPROPERTY() int32 Progress = 0;
	/** sneak: 1 once you've been close. */
	UPROPERTY() uint8 Stage = 0;
};

USTRUCT()
struct FWRStanding
{
	GENERATED_BODY()

	UPROPERTY() FName Faction;
	UPROPERTY() int32 Value = 0;
};

/**
 * A player's part in the world systems (on their PlayerState, saved in their file, sent only to them): the masters'
 * quests, the shrines they or their clan have found, the shrines they kindled themselves, faction standing.
 */
UCLASS(ClassGroup=(World))
class WILDREACH_API UWRQuestLog : public UActorComponent
{
	GENERATED_BODY()

public:
	UWRQuestLog();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	static UWRQuestLog* For(const AActor* PlayerOrState);

	UPROPERTY(Replicated) TArray<FWRQuestRep> Quests;
	UPROPERTY(Replicated) TArray<FName> FoundShrines;
	UPROPERTY(Replicated) TArray<FName> LitByMe;
	UPROPERTY(Replicated) TArray<FWRStanding> Standing;
	/** Messages of yours appraised while you were away (told when you come back). */
	int32 AppraisedAway = 0;

	const FWRQuestRep* QuestWith(FName Master) const;
	FWRQuestRep& QuestMutable(FName Master);
	int32 StandingOf(FName Faction) const;
	/** 0 Stranger .. 3 Kin (world.json factions.tiers). */
	int32 TierOf(FName Faction) const;
	void AddStanding(FName Faction, int32 Amount);
	bool HasFound(FName Shrine) const { return FoundShrines.Contains(Shrine); }

	TSharedRef<FJsonObject> ToJson() const;
	void FromJson(const TSharedPtr<FJsonObject>& O);
};

/** A shrine (world.json shrines): guarded while dark, kindled to light it, path XP, fast travel between found ones. */
UCLASS(NotPlaceable)
class WILDREACH_API AWRShrine : public AActor
{
	GENERATED_BODY()

public:
	AWRShrine();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(Replicated) FName ShrineId;
	UPROPERTY(Replicated) FName Biome;
	UPROPERTY(Replicated) FString DisplayName;
	UPROPERTY(Replicated) bool bGreat = false;
	UPROPERTY(ReplicatedUsing=OnRep_Fire) bool bLit = false;
	/** Held by invaders: dark while they hold it, travel to it shut. */
	UPROPERTY(ReplicatedUsing=OnRep_Fire) bool bHeld = false;
	/** Guards out and alive (the bowl can't be kindled). */
	UPROPERTY(Replicated) bool bGuarded = false;
	/** Server time a kindle under way finishes (0: none), and who. */
	UPROPERTY(Replicated) float KindleDoneAt = 0.f;
	UPROPERTY(Replicated) FString Kindler;

	/** Server state (saved by UWRWorldSystems). */
	double ReguardAtHours = -1.0;
	bool bEverLit = false;
	TArray<TWeakObjectPtr<AWRCreature>> Guards;
	FVector KindleFrom = FVector::ZeroVector;
	TWeakObjectPtr<AWRCharacter> KindlerPawn;

	FVector BowlLocation() const;
	FString StartKindle(AWRPlayerController* PC);

private:
	UFUNCTION() void OnRep_Fire();
	void BuildLook();
	void StepGuards();
	void FinishKindle();
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Fire;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> FireLight;
	UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> Runes;
	bool bLookBuilt = false;
	float GuardClock = 0.f;
};

/** A boss's summoning stone (world.json summoning): offer its effigy to raise it now, sealed to your party. */
UCLASS(NotPlaceable)
class WILDREACH_API AWRSummoningStone : public AActor
{
	GENERATED_BODY()

public:
	AWRSummoningStone();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	UPROPERTY(Replicated) FName BossId;
	UPROPERTY(Replicated) FName Biome;
	UPROPERTY(ReplicatedUsing=OnRep_Flare) float FlareAt = 0.f;
	FString Offer(AWRPlayerController* PC);

private:
	UFUNCTION() void OnRep_Flare();
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> Glyph;
};

/** A message cut into a stone by a player (world.json messages). */
UCLASS(NotPlaceable)
class WILDREACH_API AWRMessageStone : public AActor
{
	GENERATED_BODY()

public:
	AWRMessageStone();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	UPROPERTY(Replicated) FString Text;
	UPROPERTY(Replicated) FString Author;
	UPROPERTY(Replicated) int32 Good = 0;
	UPROPERTY(Replicated) int32 Poor = 0;
	UPROPERTY(Replicated) FName Biome;
	/** Server: who has rated it (once each). */
	TArray<FString> RatedBy;
	double PlacedAt = 0.0;
};

/** A lava bomb (the Eruption): a glowing rock on a ballistic arc, the boom 1.2 s before it lands, a glow the last 0.9 s. */
UCLASS(NotPlaceable)
class WILDREACH_API AWRLavaBomb : public AActor
{
	GENERATED_BODY()

public:
	AWRLavaBomb();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	UPROPERTY(Replicated) FVector From = FVector::ZeroVector;
	UPROPERTY(Replicated) FVector To = FVector::ZeroVector;
	UPROPERTY(Replicated) float LaunchAt = 0.f;
	UPROPERTY(Replicated) float Travel = 1.2f;
	UPROPERTY(Replicated) bool bMeteor = false;

private:
	void Land();
	bool bGlowShown = false;
	bool bLanded = false;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Trail;
};

/** What a bomb or a fallen star leaves: a glowing core to take (Interact), cooling away. */
UCLASS(NotPlaceable)
class WILDREACH_API AWRWorldCache : public AActor
{
	GENERATED_BODY()

public:
	AWRWorldCache();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	/** core (bomb core), crater (fallen star), chest (a hidden room's cache). */
	UPROPERTY(Replicated) FName Kind;
	UPROPERTY(Replicated) FName Biome;
	UPROPERTY(Replicated) FString Title;
	UPROPERTY(ReplicatedUsing=OnRep_Taken) bool bTaken = false;
	TMap<FName, int32> Items;
	FString Lore;
	/** A hidden room's: it fills again after this many game hours (0: once, then it goes). */
	float RefillHours = 0.f;
	double RefillAtHours = -1.0;
	FString Take(AWRPlayerController* PC);
	void Refill();

private:
	UFUNCTION() void OnRep_Taken();
	void BuildLook();
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> Light;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Smoke;
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> Glowing;
};

/** A village (world.json villages): its dwellings round its Great Shrine, a safe zone, the merchant's stall. */
UCLASS(NotPlaceable)
class WILDREACH_API AWRVillage : public AActor
{
	GENERATED_BODY()

public:
	AWRVillage();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	UPROPERTY(Replicated) FName VillageId;
	UPROPERTY(Replicated) FName Biome;
	UPROPERTY(Replicated) FName Style;
	UPROPERTY(Replicated) int32 Huts = 5;
	UPROPERTY(Replicated) float Ring = 1600.f;
	UPROPERTY(Replicated) FString DisplayName;
	/** Where the stall stands (world). */
	FVector StallAt() const;

private:
	void BuildLook();
};

/** A hidden sub-biome (world.json hidden): a rock-roofed tube along a route with skylights and rooms. */
UCLASS(NotPlaceable)
class WILDREACH_API AWRHiddenTube : public AActor
{
	GENERATED_BODY()

public:
	AWRHiddenTube();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	UPROPERTY(Replicated) FName TubeId;
	/** The route on the ground (world, settled), from the server. */
	UPROPERTY(ReplicatedUsing=OnRep_Route) TArray<FVector> Route;

private:
	UFUNCTION() void OnRep_Route();
	void BuildLook();
	bool bBuilt = false;
};

/** The Sealed Vault's door: a basalt slab that sinks while an eruption is on. */
UCLASS(NotPlaceable)
class WILDREACH_API AWRVaultDoor : public AActor
{
	GENERATED_BODY()

public:
	AWRVaultDoor();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	UPROPERTY(Replicated) bool bOpen = false;
	UPROPERTY(Replicated) FName Biome;

private:
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Leaf;
	UPROPERTY(Transient) TObjectPtr<class UBoxComponent> Block;
	float Sink = 0.f;
};

/**
 * A person of the world (path masters, merchants and their outriders): the hero's body (MetaHuman rig, its real clips),
 * no controller of a player. Masters can't be hurt; merchants only by creatures.
 */
UCLASS(NotPlaceable)
class WILDREACH_API AWRWorldNpc : public AWRCharacter
{
	GENERATED_BODY()

public:
	AWRWorldNpc(const FObjectInitializer& ObjectInitializer);
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual FString GetDisplayLabel() const override;

	/** master / merchant / outrider; Hearthmoot's people (WRRoundIsland): townsfolk / seller / guard. */
	UPROPERTY(Replicated) FName NpcRole;
	/** Master id (world.json masters) or caravan id. */
	UPROPERTY(Replicated) FName NpcId;
	UPROPERTY(Replicated) FString NpcName;
	UPROPERTY(Replicated) FName Biome;
	/** Merchants: stopped at a stop (trade open). Market sellers: always. */
	UPROPERTY(Replicated) bool bTrading = false;
	/** The trade table (world.json trade) a seller deals from; none: the caravan's. */
	UPROPERTY(Replicated) FName TradeTable;
	/** One of the city's people (townsfolk, seller, guard): can't be hurt, cheap to animate, strolls. */
	bool IsTownsfolk() const { return NpcRole == TEXT("townsfolk") || NpcRole == TEXT("seller") || NpcRole == TEXT("guard"); }

	/** Server: the look (sex m/f, face, build, the four armour pieces and their elements). */
	void SetLook(FName Sex, int32 InFace, int32 InBuild, const FName (&Armor)[4], const FName (&Elements)[4]);
	/** Server, merchants: walk toward this point (none: stand). */
	FVector WalkTo = FVector::ZeroVector;
	bool bWalking = false;
	/** Server, masters: where they work and face. */
	FVector WorkAt = FVector::ZeroVector;
	FRotator WorkFacing = FRotator::ZeroRotator;
	FName WorkClip;
	float WorkEvery = 3.f;
	FName WorkSound;
	FName WorkFx;
	TMap<FString, double> GreetedAt;
	bool HasVitals() const { return bVitals; }
	/** Clients: the city's people tick and pose less often the further they are from the view. */
	void StepCrowdCost();

protected:
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnDeath(AActor* Killer) override;

private:
	bool bVitals = false;
	float WorkClock = 0.f;
	float GreetClock = 0.f;
	float StuckClock = 0.f;
	FVector LastPos = FVector::ZeroVector;
};

/**
 * The server's world systems (and the client's lookup of them). Spawns the shrines, villages, masters, stones and tubes
 * once their ground has streamed in; runs Starfall, the biome events, invasions and the caravans; saves to the world's
 * world_systems.json; fast travel.
 */
UCLASS()
class WILDREACH_API UWRWorldSystems : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableInEditor() const override { return false; }

	static UWRWorldSystems* Get(const UObject* WorldContext);
	bool IsServerRunning() const { return bServer; }

	/** Server: write world_systems.json now (also every minute and at the end). */
	void Save();
	/** Server: a player came in: their shrines (theirs and their clan's) and any appraisals while away. */
	void PlayerJoined(AWRPlayerState* PS);

	AWRShrine* FindShrine(FName Id) const;
	/** Server: discovered by a player (their clan sees it too): the world's record. */
	void NoteFound(FName Shrine, const FString& Player);
	/** Server: refresh a player's FoundShrines from the record (them and their clan). */
	void RefreshFound(AWRPlayerState* PS);
	FString Travel(AWRPlayerController* PC, FName To);
	/** Why travel to this shrine isn't possible for this player from where they stand (empty: it is). Server and owner. */
	static FString TravelBlock(const AWRPlayerController* PC, const AWRShrine* To);

	/** Server: quest progress. */
	void QuestEvent(AWRPlayerState* PS, FName Type, FName What, int32 Count = 1);

	/** Server: a summoned boss and the party it's sealed to. */
	TMap<TWeakObjectPtr<AWRCreature>, TArray<FString>> Sealed;
	struct FSummon { FName Boss; TArray<FString> Party; double At = 0.0; FVector Stone = FVector::ZeroVector; };
	TArray<FSummon> PendingSummons;

	/** Server: messages (world save). */
	TArray<TWeakObjectPtr<AWRMessageStone>> Messages;
	FString PlaceMessage(AWRPlayerController* PC, const TArray<int32>& Picks);

	/** Invasion damage ledger (player -> damage) and the invaders. */
	TMap<FString, float> InvasionLedger;
	TArray<TWeakObjectPtr<AWRCreature>> Invaders;

	/** A dev command (world dev ...). */
	FString Dev(AWRPlayerController* PC, const TArray<FString>& Args);

	/** Played seconds of this world (only while someone's on; saved). */
	double Clock = 0.0;
	/** The Sealed Vault opens for this many game hours (eruption), until this total hour. */
	double VaultOpenUntilHours = -1.0;

private:
	bool bServer = false;
	bool bPlaced = false;
	float PlaceClock = 0.f;
	float SaveClock = 0.f;
	float StepClock = 0.f;
	TSharedPtr<FJsonObject> Loaded;

	TMap<FName, TWeakObjectPtr<AWRShrine>> Shrines;
	TMap<FName, TWeakObjectPtr<AWRVillage>> Villages;
	TMap<FName, TSet<FString>> FoundBy;

	void Load();
	bool TryPlace();
	void PlaceShrines();
	void PlaceMasters();
	void PlaceStones();
	void PlaceTubes();
	void StepShrinesAndKindles(float Dt);
	void StepMasters(float Dt);

	// Events.
	TMap<FName, double> NextEventAt;
	struct FEventRun { FName Id; FName Biome; double WarnAt = 0.0; double StartAt = 0.0; double EndAt = 0.0; FVector Centre; TMap<TWeakObjectPtr<APawn>, double> NextBomb; double NextTremor = 0.0; double NextStreak = 0.0; int32 Meteors = 0; bool bForced = false; };
	TArray<FEventRun> Runs;
	void StepEvents(float Dt);
	bool StartEvent(FName Id, bool bNow);
	void StepEruption(FEventRun& R, double Now);
	void StepStarfall(FEventRun& R, double Now);
	void EndEvent(FEventRun& R);
	void DropMeteor(FName Biome);
	void SyncEventRep();
	struct FCrater { TWeakObjectPtr<AWRWorldCache> Cache; double LandedHours = 0.0; bool bDrew = false; FName Biome; };
	TArray<FCrater> Craters;
	bool bWasNight = false;
	bool bStarfallDue = false;

	// Invasions.
	struct FInvasion { bool bOn = false; FName From, To; FString Pass; FVector AttackMouth, DefendMouth, Target; FName TargetShrine; int32 Stage = 0; double StageEnd = 0.0; TArray<TPair<FName, int32>> Kinds; FName Leader; };
	FInvasion Inv;
	TMap<FString, double> NextInvasionAt;
	void StepInvasion(float Dt);
	bool StartInvasion(int32 PairIndex, bool bNow);
	void EndInvasion(bool bWon);

	// Caravans.
	struct FCaravan { FName Id; TArray<FVector> Stops; TArray<FString> StopNames; double DeadUntilHours = -1.0; TWeakObjectPtr<AWRWorldNpc> Merchant; TWeakObjectPtr<AWRWorldNpc> Outrider; double StartHours = 0.0; };
	TArray<FCaravan> Caravans;
	void StepCaravans(float Dt);
	/** Where a caravan is on its loop at this game hour (and whether it's stopped, at which stop). */
	FVector CaravanAt(const FCaravan& C, double Hours, bool& bStopped, int32& OutStop, FVector& OutHeading) const;

	// Masters.
	TArray<TWeakObjectPtr<AWRWorldNpc>> Masters;

	void WriteJson(TSharedRef<FJsonObject> Root) const;
	friend class AWRShrine;
	friend class AWRWorldNpc;
	friend struct FWRWorldSystemsAccess;
};
