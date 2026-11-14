#pragma once
// Dungeon portals (FULL-GAME 8.2; cooks/build-portals-artifacts.md, cooks/endgame.md 2.5, cooks/biome-desert.md 2.22).
// A portal (AWRPortal) is a tear in its land, ranked by colour, that breaks after its real-day timer and spills its
// creatures first. Entering puts the party in its fold (AWRFold): handmade rooms from Data/portal_rooms/<biome>.json laid
// along one of the kit's handmade graphs, built in a bay high above the sea east of the island (Valheim's dungeons sit
// 5 km over their doors), closed on every side. Doors, the entry stone, chests, traps, puzzles and the boss arena's
// props are AWRFoldThing; the boss's mechanic is UWRPortalBossBrain. UWRPortalSystem runs it all on the server and keeps
// it in the world's portals.json. Numbers: Data/portals.json.
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRPortals.generated.h"

class AWRPlayerController;
class AWRPlayerState;
class AWRCreature;
class AWRBoss;
class AWRCharacterBase;
class AWRCharacter;
class AWRFold;
class AWRPortal;
class FJsonObject;
class UPointLightComponent;
class UStaticMeshComponent;
class UBoxComponent;
class UAudioComponent;
struct FWRHit;

namespace WRPortals
{
	/** Data/portals.json (read once per game). */
	WILDREACH_API TSharedPtr<FJsonObject> Data();
	WILDREACH_API TSharedPtr<FJsonObject> Obj(const FString& Path);
	WILDREACH_API float Num(const FString& Path, float Fallback);
	/** A land's room kit (Data/portal_rooms/<biome>.json), or null if it has none yet. */
	WILDREACH_API TSharedPtr<FJsonObject> Kit(FName Biome);
	WILDREACH_API TSharedPtr<FJsonObject> Room(FName Biome, FName RoomId);
	/** portals.json ranks[rank-1]. */
	WILDREACH_API TSharedPtr<FJsonObject> Rank(int32 Rank);
	WILDREACH_API FColor RankColour(int32 Rank);
	WILDREACH_API FString RankName(int32 Rank);
	WILDREACH_API FString BiomeName(FName Biome);
	WILDREACH_API FString ModifierName(FName Id);
	WILDREACH_API FString ModifierLine(FName Id);

	/** The fold this point is inside (every machine), or null. */
	WILDREACH_API AWRFold* FoldAt(const UObject* WorldContext, const FVector& Point);
	/** The fold an actor (a player, their pawn or state, a creature) stands in. */
	WILDREACH_API AWRFold* FoldOf(const AActor* Who);
	/** True anywhere in the bays (far east, high up), with or without a fold built there. */
	WILDREACH_API bool InBaySpace(const FVector& Point);
	/** A modifier is on in the fold this actor is in. */
	WILDREACH_API bool Modifier(const AActor* Who, FName Mod);
	WILDREACH_API float ModNum(FName Mod, const TCHAR* Field, float Fallback);

	// --- Hooks the rest of the game calls (server unless said) ---
	/** ApplyHit's multipliers: the fold's Double damage. */
	WILDREACH_API float DamageScale(const AWRCharacterBase* Target, const AWRCharacterBase* Attacker, const FWRHit& Hit);
	/** A creature or boss of a fold took this blow (a boss's arena mechanic shapes it). */
	WILDREACH_API float BossIncoming(AWRBoss* Boss, const FWRHit& Hit, float Damage);
	/** A boss's outgoing blow (the Alpha's lit mane). */
	WILDREACH_API float BossOutgoing(const AWRCharacterBase* Attacker);
	WILDREACH_API void OnBossAttack(AWRBoss* Boss, FName AttackId);
	/** A boss may choose this move now (Brannoch's breath stops with both vents broken). */
	WILDREACH_API bool AttackAllowed(const AWRBoss* Boss, FName AttackId);
	/** Stance damage a fold boss takes (the Alpha's ice plates, the Mould-Born hardened by frost). */
	WILDREACH_API float StanceScale(const AWRCharacterBase* Target);
	WILDREACH_API void OnBossPart(AWRBoss* Boss, FName PartId, FName OnBreak);
	/** From AWRCharacterBase::Die (before tags clear). */
	WILDREACH_API void OnKilled(AWRCharacterBase* Victim, AActor* Killer);
	/** A weapon swing or a shot met a fold thing (a clutch, an icicle, a rope, a frost bolt): true = it took it. */
	WILDREACH_API bool HitThing(AActor* Thing, AWRCharacterBase* By, float Damage, bool bFire);
	/** Heals do nothing (No healing). */
	WILDREACH_API bool HealBlocked(const AActor* Who);
	/** Light radius scale in a fold (Darkness). */
	WILDREACH_API float LightScale(const AActor* Who);
	/** Status buildup a creature lays on a player (Venomous). */
	WILDREACH_API float BuildupScale(const AActor* Attacker, const AActor* Target);
	/** Gear wear (Brittle gear). */
	WILDREACH_API float WearScale(const AActor* Who);
	/** Creature move / attack pace (Frenzy). */
	WILDREACH_API float CreaturePace(const AActor* Creature);
	/** Creatures shrug off flinches (Iron hide). */
	WILDREACH_API bool CreatureIronHide(const AActor* Creature);
	/** No building, no beds, no bombs: a fold counts as underground. */
	WILDREACH_API bool IsFold(const UObject* WorldContext, const FVector& Point);
	/** The land a fold belongs to (its sound, cold, dark), or None outside one. */
	WILDREACH_API FName FoldBiome(const UObject* WorldContext, const FVector& Point);

	/** Owning client, E with no menu: a portal or a fold thing in reach. */
	WILDREACH_API bool ClientInteract(AWRPlayerController* PC);
	WILDREACH_API bool Prompt(const AWRPlayerController* PC, FString& OutTarget, FString& OutVerb);
	/** Server: "portal ..." commands from the screens. */
	WILDREACH_API FString Command(AWRPlayerController* PC, const TArray<FString>& Args);
	/** Server: a portal key used from the bag. */
	WILDREACH_API FString UseKey(AWRPlayerState* PS, FName ItemId);
	/** Owning client: the fold's top line ("Rare fold · Volcanic · No healing"), empty outside. */
	WILDREACH_API FString HudLine(const AWRPlayerController* PC);
	/** Why this player can't enter this portal now (empty: they can). Every machine. */
	WILDREACH_API FString EnterBlock(const AWRPlayerController* PC, const AWRPortal* Portal);
	/** The rooms of the fold this player has walked into (the fold map), owning client. */
	WILDREACH_API TArray<int32> SeenRooms(const AWRPlayerController* PC);
}

/** A portal in the world. */
UCLASS(NotPlaceable)
class WILDREACH_API AWRPortal : public AActor
{
	GENERATED_BODY()
public:
	AWRPortal();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(Replicated) int32 PortalId = 0;
	UPROPERTY(ReplicatedUsing=OnRep_Look) FName Biome;
	UPROPERTY(ReplicatedUsing=OnRep_Look) uint8 Rank = 1;
	UPROPERTY(Replicated) TArray<FName> Modifiers;
	/** 0 stable, 1 cracking, 2 spilling. */
	UPROPERTY(ReplicatedUsing=OnRep_Stage) uint8 Stage = 0;
	/** A key portal: sealed to its opener's party. */
	UPROPERTY(Replicated) bool bKey = false;
	UPROPERTY(Replicated) FString OwnerName;
	/** A key portal: who may go in now (the opener, their clan, their band), kept by the server. */
	UPROPERTY(Replicated) TArray<FString> Allowed;
	/** Real UTC seconds it breaks (0: never). */
	UPROPERTY(Replicated) double BreakAtUtc = 0.0;
	/** A run is open inside; its boss door sealed; the run cleared (closing). */
	UPROPERTY(Replicated) bool bRunOpen = false;
	UPROPERTY(Replicated) bool bSealed = false;
	UPROPERTY(ReplicatedUsing=OnRep_Stage) bool bClosing = false;
	/** Opening (a key's tear grows over keyOpenSeconds): server time it's open. */
	UPROPERTY(Replicated) float OpenAt = 0.f;
	/** Its run's fold. */
	UPROPERTY(Replicated) TObjectPtr<AWRFold> Fold;

	/** Server. */
	int32 Slot = -1;
	double OpenedUtc = 0.0;
	double NextSpillHours = -1.0;
	bool bWardenOut = false;
	TWeakObjectPtr<AWRCreature> Warden;
	TArray<TWeakObjectPtr<AWRCreature>> Spilled;
	/** Party members of a key portal's opener when it opened (with the clan and band, read live). */
	TArray<FString> KeyParty;

	/** Where you arrive when you come out (in front of the tear). */
	FVector Front() const;
	/** Seconds left on its timer (every machine), < 0 never. */
	double SecondsToBreak() const;
	FString StateLine() const;
	bool IsSealedTo(const FString& Player) const;

private:
	UFUNCTION() void OnRep_Look();
	UFUNCTION() void OnRep_Stage();
	void BuildLook();
	bool bLookBuilt = false;
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> TearPlanes;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Pillar;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> Light;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Hum;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Motes;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Cracks;
	UPROPERTY(Transient) TObjectPtr<class UMaterialInstanceDynamic> TearMat;
	UPROPERTY(Transient) TObjectPtr<class UMaterialInstanceDynamic> PillarMat;
	float Clock = 0.f;
};

/** One room of a fold: which handmade room, where its west door's foot is, how it hangs on the line. */
USTRUCT()
struct FWRFoldRoomRep
{
	GENERATED_BODY()
	UPROPERTY() FName RoomId;
	/** World: the floor at the middle of its west wall (the room's frame: +X into the room, +Y south). */
	UPROPERTY() FVector Origin = FVector::ZeroVector;
	/** 0 main line, 1 a branch north of its host, 2 south. */
	UPROPERTY() uint8 Side = 0;
	/** A branch's host room (main index). */
	UPROPERTY() uint8 Host = 0;
	/** Main line: the branch doors it has (bit 1 north, 2 south). */
	UPROPERTY() uint8 BranchDoors = 0;
	/** Its type (entry, combat, trap, puzzle, rest, treasure, boss). */
	UPROPERTY() FName Type;
	UPROPERTY() float W = 1600.f;
	UPROPERTY() float D = 1600.f;
	UPROPERTY() float H = 700.f;
};

/** A run's interior: its rooms and corridors (collision on every machine, the look on clients). */
UCLASS(NotPlaceable)
class WILDREACH_API AWRFold : public AActor
{
	GENERATED_BODY()
public:
	AWRFold();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	UPROPERTY(Replicated) uint8 Bay = 0;
	UPROPERTY(Replicated) FName Biome;
	UPROPERTY(Replicated) uint8 Rank = 1;
	UPROPERTY(Replicated) TArray<FName> Modifiers;
	UPROPERTY(ReplicatedUsing=OnRep_Rooms) TArray<FWRFoldRoomRep> Rooms;
	UPROPERTY(Replicated) TObjectPtr<AWRPortal> Portal;
	/** Sent to whoever is about to come in (in its roster) as well as to those near: their fade has it built. */
	virtual bool IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const override;
	/** The boss is dead: server time the fold closes. */
	UPROPERTY(Replicated) float CloseAt = 0.f;
	/** The boss door has dropped behind someone. */
	UPROPERTY(Replicated) bool bSealed = false;

	static TArray<TWeakObjectPtr<AWRFold>>& All();
	/** The rooms' outline (every machine), a little grown. */
	FBox Bounds;
	/** Which room a point is in (main and branch), or INDEX_NONE. */
	int32 RoomAt(const FVector& Point, float Grow = 0.f) const;
	FTransform RoomFrame(int32 Index) const;
	TSharedPtr<FJsonObject> RoomRow(int32 Index) const;
	/** World point of room metres (x along, y across) at height z cm. */
	FVector RoomPoint(int32 Index, float XM, float YM, float ZCm = 0.f) const;
	FVector EntryPoint() const;
	int32 BossRoom() const;

	// --- Server: the run ---
	TArray<FString> Roster;
	TSet<int32> Woken;
	TSet<int32> Cleared;
	TMap<int32, TArray<TWeakObjectPtr<AWRCreature>>> RoomCreatures;
	TWeakObjectPtr<AWRBoss> Boss;
	double LastOccupiedUtc = 0.0;
	/** Each player's rooms walked into (the fold map), sent to them. */
	TMap<FString, TArray<int32>> SeenBy;
	/** Hazards drawn into its rooms (every machine): lava runnels (segments), black-water pits, dark thick ice. */
	TArray<FVector> LavaA, LavaB;
	TArray<FBox> Pits;
	TArray<FBox> ThickIce;
	/** Server: the hazards' clock. */
	float HazardClock = 0.f;

private:
	UFUNCTION() void OnRep_Rooms();
	void Build();
	void BuildRoom(int32 Index);
	void BuildCorridor(const FVector& A, const FVector& B, float H);
	bool bBuilt = false;
};

/** A thing in a fold: a door, the entry stone, a chest, a trap part, a puzzle part, a boss arena prop. */
UCLASS(NotPlaceable)
class WILDREACH_API AWRFoldThing : public AActor
{
	GENERATED_BODY()
public:
	AWRFoldThing();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** door, bossdoor, stone, chest, vault, grate, block, chute, icicle, pane, quench, trough, bellows, hammer, forge,
	 *  brazier, flame, prism, socket, skylight, clutch, crucible, tank, bolt, window, arenabrazier, spring. */
	UPROPERTY(ReplicatedUsing=OnRep_Look) FName Kind;
	UPROPERTY(ReplicatedUsing=OnRep_Look) FName Biome;
	UPROPERTY(Replicated) TObjectPtr<AWRFold> Fold;
	UPROPERTY(Replicated) uint8 Room = 0;
	/** Its place in its group (trough 0 water / 1 oil / 2 brine; bellows 0..2; brazier order; prism 0..2). */
	UPROPERTY(ReplicatedUsing=OnRep_State) uint8 Sub = 0;
	/** What it's doing (by kind): a door 1 open; a puzzle part's setting; a chest unused. */
	UPROPERTY(ReplicatedUsing=OnRep_State) int32 State = 0;
	/** Server times (by kind): a trap's next tell / when it fires; a door's change. */
	UPROPERTY(ReplicatedUsing=OnRep_State) float TimeA = 0.f;
	UPROPERTY(ReplicatedUsing=OnRep_State) float TimeB = 0.f;
	/** Done / broken / spent (by kind). */
	UPROPERTY(ReplicatedUsing=OnRep_State) bool bDone = false;
	/** Who has opened a personal chest; who carries a quench blank. */
	UPROPERTY(ReplicatedUsing=OnRep_State) TArray<FString> Names;
	/** A colour by kind (the lintel's temper band, a prism's beam, the aurora band). */
	UPROPERTY(ReplicatedUsing=OnRep_Look) FColor Tint = FColor::White;
	/** A size by kind (a pane, a pool) in cm. */
	UPROPERTY(ReplicatedUsing=OnRep_Look) FVector Size = FVector(100.f);

	/** Server. */
	float Health = 0.f;
	float Period = 4.5f;
	/** A puzzle's answer as every machine needs it to draw it (the aurora's band order, the hammer's rhythm). */
	UPROPERTY(Replicated) TArray<uint8> Seq;
	/** The part of a group that holds its state (a trough's quench controller, a prism's skylight, a hammer's forge). */
	UPROPERTY(Replicated) TObjectPtr<AWRFoldThing> Link;
	FVector Goal = FVector::ZeroVector;
	FString Interact(AWRPlayerController* PC);
	FString Verb(const AWRPlayerController* PC) const;
	FString Title() const;
	bool CanInteract(const AWRPlayerController* PC) const;
	void TakeHit(AWRCharacterBase* By, float Damage, bool bFire);
	/** A door: open or shut it (server). */
	void SetOpen(bool bOpen);

private:
	UFUNCTION() void OnRep_Look();
	UFUNCTION() void OnRep_State();
	void BuildLook();
	void ShowState();
	void StepServer(float Dt);
	bool bLookBuilt = false;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Moving;
	UPROPERTY(Transient) TObjectPtr<UBoxComponent> Block;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> Light;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Glows;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Fx;
	UPROPERTY(Transient) TObjectPtr<class UMaterialInstanceDynamic> GlowMat;
	float MoveNow = 0.f;
	float LastShown = -1.f;
	int32 ShownState = -100;
	bool bFired = false;
	bool bTold = false;
	float Clock = 0.f;
	friend class UWRPortalSystem;
	friend class UWRPortalBossBrain;
};

/** A portal boss's arena mechanic (server, on the boss): what its props do to it and what it does with them. */
UCLASS()
class WILDREACH_API UWRPortalBossBrain : public UActorComponent
{
	GENERATED_BODY()
public:
	UWRPortalBossBrain();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	TWeakObjectPtr<AWRFold> Fold;
	int32 Room = -1;
	/** Outside a fold (an escaped boss): no arena, just the body. */
	bool bEscaped = false;
	FVector Home = FVector::ZeroVector;

	float Incoming(const FWRHit& Hit, float Damage);
	float Outgoing() const;
	void OnAttack(FName AttackId);
	void OnPart(FName PartId, FName OnBreak);
	void Spawned();

	// State by boss.
	double ManeOutUntil = 0.0;
	bool bManeLit = true;
	double HardenedUntil = 0.0;
	double ShellAt = 0.0;
	bool bShell = false;
	double CooledUntil = 0.0;
	double PlatesMeltedUntil = 0.0;
	double NextLay = 0.0;
	double NextSnuff = 0.0;
	double StunnedUntil = 0.0;
	int32 VentsBroken = 0;
	TArray<TWeakObjectPtr<AWRFoldThing>> Props;
	TArray<TWeakObjectPtr<AWRCreature>> Adds;
	TArray<TWeakObjectPtr<AActor>> Clones;
	double CloneUntil = 0.0;
private:
	FName Id() const;
	AWRBoss* Boss() const;
	void Stagger(float Seconds);
	void Summon(FName Kind, int32 Count, float Ring);
	double Now() const;
};

/**
 * The server's portals: slots by land, placement, the real-day clocks, spilling, escaped bosses, keys, runs in bays,
 * entry and the party's accept, clearing, closing, death bags, rewards, the save. Clients use it only for lookups.
 */
UCLASS()
class WILDREACH_API UWRPortalSystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableInEditor() const override { return false; }
	static UWRPortalSystem* Get(const UObject* WorldContext);

	bool IsServer() const { return bServer; }
	/** Real time now (UTC seconds since 1970). */
	static double UtcNow();

	/** Server: open a portal (a slot's wild one, or a key's). */
	AWRPortal* OpenPortal(FName Biome, int32 Rank, const FVector& At, float Yaw, int32 Slot, const FString& KeyOwner, const TArray<FName>* Mods = nullptr);
	/** Server: entry. Enter asks the party; Go puts one player in. */
	FString Enter(AWRPlayerController* PC, AWRPortal* Portal);
	FString Accept(AWRPlayerController* PC, int32 PortalId, bool bCome);
	FString Leave(AWRPlayerController* PC);
	void PutInside(AWRPlayerController* PC, AWRPortal* Portal, bool bFirst);
	/** Server: the boss died (its fold's clear). */
	void BossDied(AWRBoss* Boss);
	/** Server: an escaped boss died. */
	void EscapedDied(AWRCreature* Boss);
	AWRPortal* FindPortal(int32 Id) const;
	/** Why a rank of a land is held shut (an escaped boss), or empty. */
	FString RankBlocked(FName Biome, int32 Rank) const;
	/** The open runs (bays in use). */
	int32 RunsOpen() const;
	void Save();
	/** Dev: portal dev open <biome> <rank> | clear | break | crack | spill | key. */
	FString Dev(AWRPlayerController* PC, const TArray<FString>& Args);

	/** Pending party invites: portal id -> who asked, who's asked, until when. */
	struct FInvite { int32 PortalId = 0; FString From; TArray<FString> Asked; TArray<FString> Coming; double Until = 0.0; };
	TArray<FInvite> Invites;

private:
	bool bServer = false;
	bool bLoaded = false;
	float StepClock = 0.f;
	float SaveClock = 0.f;
	float PlaceClock = 0.f;
	int32 NextId = 1;
	TSharedPtr<FJsonObject> Loaded;

	TArray<TWeakObjectPtr<AWRPortal>> Portals;
	/** Slots: biome -> per slot the UTC second it refills (an open one: 0). */
	TMap<FName, TArray<double>> SlotRefill;
	struct FEscaped { FName Biome; int32 Rank = 0; FName BossId; FVector At = FVector::ZeroVector; TWeakObjectPtr<AWRCreature> Body; };
	TArray<FEscaped> Escaped;
	/** Players inside a fold when the server stopped (put back at their portal when they come). */
	TMap<FString, FVector> InsideAtStop;
	struct FScorch { FVector At; double UntilHours = 0.0; TWeakObjectPtr<AActor> Mark; };
	TArray<FScorch> Scorches;

	void Load();
	void Restore();
	void StepSlots();
	void StepPortal(AWRPortal* P, float Dt);
	void StepRuns(float Dt);
	void StepInvites();
	void StepEscaped();
	void StepRescue();
	bool FindSpot(FName Biome, int32 Rank, FVector& OutAt, float& OutYaw) const;
	bool SpotOk(FName Biome, const FVector& At, int32 Rank, FString* Why) const;
	void Break(AWRPortal* P);
	void SpillPack(AWRPortal* P, bool bWarden);
	void ClosePortal(AWRPortal* P, bool bCleared);
	AWRFold* BuildFold(AWRPortal* P);
	void TearDownFold(AWRFold* F, const FVector& PutAt);
	void WakeRoom(AWRFold* F, int32 Room);
	void PopulateThings(AWRFold* F);
	void SpawnBoss(AWRFold* F);
	void RoomCleared(AWRFold* F, int32 Room);
	int32 FreeBay() const;
	void Reward(AWRFold* F);
	void Announce(const FString& Line, FName Sound);
	friend class AWRFoldThing;
	friend class UWRPortalBossBrain;
	friend struct FWRPortalsAccess;
};
