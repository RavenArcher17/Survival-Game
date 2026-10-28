#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "WRCharacterBase.h"
#include "WRCreature.generated.h"

class AWRCreature;
class AWRCharacter;
class AWRPlayerController;
class UCameraComponent;
class UPointLightComponent;
class USpringArmComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/** One creature definition from Data/creatures.json. */
struct FWRCreatureDef
{
	FName Id;
	FString Name;
	FName Tier;
	FName Behavior;   // aggressive / neutral / passive
	FName Archetype;  // pack_hunter, ground_predator, flyer, burrower, herd, critter, dummy
	FString Role;
	float Health = 70.f;
	float Damage = 12.f;
	float Speed = 450.f;
	float Poise = 30.f;
	float Radius = 40.f;
	float HalfHeight = 60.f;
	float AggroRadius = 1500.f;
	float AttackRange = 190.f;
	FLinearColor Color = FLinearColor::Gray;
	FName Element;
	TArray<FName> Resists;
	TArray<TPair<FName, int32>> Loot;
	// Taming
	TArray<FName> TamingMethods;  // knockout_feed, offer_food, steal_egg, path_magic
	FName Difficulty;             // easy / medium / hard
	TArray<FName> Food;
	bool bMount = false;
	float RideSpeed = 600.f;
	FName Utility;                // light, mine_ore, sense_secrets
	TArray<TPair<FName, int32>> Products;
	bool bNest = false;
	bool bHearing = false;
	// Wild life
	TArray<FName> Attacks;        // besides the bite: lunge, charge, spit, stomp (numbers in tuning.json creatureAI.<attack>)
	FName Diet;                   // carnivore (hunts), herbivore, lithovore; from its food when not given
	FName Activity;               // diurnal (sleeps at night), nocturnal (sleeps by day), any

	// Art: a skeletal mesh replaces the placeholder shape when set (scaled to the capsule, feet on the ground).
	FString Mesh;
	TMap<FName, FString> Anims;   // idle, walk, run, attack, hit, death; flyers also fly (moving) / hover (still) in the air
	TArray<FString> Materials;    // per-slot overrides ("" keeps the mesh's own), e.g. a tinted pack creature
	float MeshYaw = 0.f;
	float MeshScale = 1.f;        // multiplier on the auto fit
	float Glow = 0.f;             // > 0, no mesh: a glowing orb (emissive sphere in its colour + a light this bright)
	// Animation overrides (0 / None: measured from the clips, see UWRAnimInstance).
	float WalkPace = 0.f;         // cm/s the walk clip covers at play rate 1
	float RunPace = 0.f;
	float StrikeAt = 0.f;         // where in the attack clip (0..1) the bite lands
	FName AnimUpper;              // bone the attack / hit layer starts from (its subtree), so running legs keep running

	bool CanTameBy(FName Method) const { return TamingMethods.Contains(Method); }
	float TameSeconds() const;
	/** Taming value of a food: kibble x taming.kibbleValue, what it eats x1, anything else 0. */
	float FoodValue(FName Item) const;
	/** Whether a hungry hunter goes after this prey: a carnivore, prey not a carnivore / boss / minion / dummy, and no
	 *  bigger than creatureAI.preySizeRatio x the hunter (packPreySizeRatio for pack hunters). */
	static bool CanHunt(const FWRCreatureDef& Hunter, const FWRCreatureDef& Prey);
};

class WILDREACH_API FWRCreatureRegistry
{
public:
	static FWRCreatureRegistry& Get();
	const FWRCreatureDef* Find(FName Id) const;
	const TArray<FWRCreatureDef>& All() const { return Defs; }
	void Reload();

private:
	FWRCreatureRegistry();
	TArray<FWRCreatureDef> Defs;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FWROnCreatureDied, AWRCreature*);

UENUM()
enum class EWRTameOrder : uint8
{
	Follow,
	Stay,
};

/** Creature body: owns its ability system, takes its stats from a creature definition; can be tamed and ridden. */
UCLASS()
class WILDREACH_API AWRCreature : public AWRCharacterBase
{
	GENERATED_BODY()

public:
	AWRCreature(const FObjectInitializer& ObjectInitializer);

	/** Spawn a creature by id (picks the dummy class for the training dummy). Server only. */
	static AWRCreature* SpawnCreature(UWorld* World, FName CreatureId, const FVector& Location, const FRotator& Rotation);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool IsCreature() const override { return true; }
	virtual FString GetTameOwner() const override { return TamedBy; }
	virtual bool IsHostileTo(const AWRCharacterBase* Other) const override;
	virtual void NotifyAttackedBy(AActor* Attacker) override;
	virtual FString GetDisplayLabel() const override;
	virtual void AddTorpor(float Amount) override;
	virtual bool TryPathMagic(const FWRHit& Hit) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void OnSwingVisual(EWRSwingKind Kind, float Duration) override;
	virtual void OnDeathVisual() override;
	virtual void OnHitVisual(EWRHitResult Result) override;
	/** AI movement curves round instead of sliding sideways: the way it walks leads its facing by at most
	 *  anim.creatureLeadDegrees; nearly stopped and facing well away, it turns on the spot first. */
	virtual void AddMovementInput(FVector WorldDirection, float ScaleValue = 1.f, bool bForce = false) override;

	const FWRCreatureDef* GetDef() const { return FWRCreatureRegistry::Get().Find(CreatureId); }
	AActor* GetAggroTarget() const { return AggroTarget.Get(); }
	void SetAggroTarget(AActor* Target) { AggroTarget = Target; }
	FVector GetHome() const { return Home; }
	bool IsTamed() const { return !TamedBy.IsEmpty(); }
	bool IsUnconscious() const;

	// --- Taming (server) ---
	/** E: offer food (calm creatures), or put the best food you have in a knocked-out one's stash. Returns a message. */
	FString Feed(AWRPlayerController* Player);
	/** Knockout taming, ARK-style: all of one food (kibble or what it eats) into its stash; it eats a meal at a time. */
	FString GiveTameFood(AWRPlayerController* Player, FName Item);
	/** Knockout taming: a narcotic keeps it under (+torpor). */
	FString GiveNarcotic(AWRPlayerController* Player);
	/** Taming value of a food for this creature: kibble x2, what it eats x1, anything else 0. */
	float FoodValue(FName Item) const;
	void CompleteTame(const FString& PlayerName);
	/** Tames come out stronger the better they were tamed: health and bite x (1 + bonus). */
	void ApplyTameBonus(float Bonus);
	FString Order(AWRPlayerController* Player, const FString& Command);
	/** Number of tames a player owns. */
	static int32 CountTames(const UWorld* World, const FString& PlayerName);
	/** Tames a player may keep (taming.tameLimit + Bonded / Beast lord perks). */
	static int32 TameLimitFor(const APlayerState* PS);
	// --- Wild life (server) ---
	/** The prey it's hunting. A predator and its prey fight each other; wild creatures otherwise never do. */
	TWeakObjectPtr<AWRCreature> HuntTarget;
	bool IsHungry() const;
	bool IsEating() const;
	bool IsFleeing() const;
	/** Prey bolt in bursts: rested enough since its last run to bolt from a hunter again. */
	bool CanBoltAgain() const;
	AActor* GetFleeFrom() const { return FleeFrom.Get(); }
	/** Run from a threat for creatureAI.fleeSeconds (bosses, minions and dummies never do). */
	void StartFleeing(AActor* From, const TCHAR* Why);
	/** A packmate just died: a hurt creature breaks and runs sooner. */
	void NotePackLoss();
	bool RecentPackLoss() const;
	/** Seconds since something last hurt it. */
	float SinceHit() const;
	/** It killed its prey: eats for a while, then isn't hungry for creatureAI.huntIntervalMinutes. */
	void Ate(AWRCreature* Prey);
	/** Gave up a hunt (prey got away): tries again a little later. */
	void HuntFailed();
	bool IsAsleep() const { return bAsleep; }
	void SetAsleep(bool bSleep);
	bool WasWokenRecently() const;
	UPROPERTY(Replicated) bool bAsleep = false;
	/** Who it's after, for every machine (the server's AggroTarget, copied as it changes): aggro calls, combat music. */
	AActor* GetAggroRep() const { return AggroRep; }
	UPROPERTY(Replicated) TObjectPtr<AActor> AggroRep;

	// --- The body (WRCarcass.cpp): a wild creature's kill stays down, to be butchered by hitting it ---
	/** Server: a melee hit on the body: a share of its loot into the hitter's bag (more with a blade in hand). */
	void HarvestBody(AWRCharacterBase* Harvester, bool bHeavy);
	/** A body with something left to take (every machine). */
	bool IsHarvestableBody() const { return BodyHitsLeft > 0; }
	int32 GetBodyHitsLeft() const { return BodyHitsLeft; }

	/** Server: player mounts / dismounts. */
	FString Ride(AWRCharacter* Rider);
	void Dismount();
	AWRCharacter* GetRider() const { return Rider.Get(); }

	UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_CreatureId, Category="Creature") FName CreatureId = TEXT("cinderfang");
	UPROPERTY(ReplicatedUsing=OnRep_Tamed) FString TamedBy;
	UPROPERTY(Replicated) EWRTameOrder TameOrder = EWRTameOrder::Follow;
	UPROPERTY(Replicated) bool bTameAggressive = true;
	UPROPERTY(Replicated) float Torpor = 0.f;
	UPROPERTY(Replicated) float MaxTorpor = 100.f;
	UPROPERTY(Replicated) float TameProgress = 0.f;
	UPROPERTY(Replicated) float TameEffectiveness = 1.f;  // hits while it's out and plain food lower it
	UPROPERTY(Replicated) float TameBonus = 0.f;          // a tame's health / bite bonus (from effectiveness)
	UPROPERTY(Replicated) int32 TameFoodCount = 0;        // food left in its stash
	UPROPERTY(Replicated) FName TameFoodItem;             // what it eats next
	UPROPERTY(Replicated) float TameEta = -1.f;           // seconds to tamed if the food lasts, -1 = it won't
	UPROPERTY(Replicated) int32 TameFoodNeeded = 0;       // more of that food it needs to finish
	UPROPERTY(Replicated) float MealLeft = 0.f;           // seconds left on the meal it's digesting
	UPROPERTY(Replicated) float WakeIn = 0.f;             // seconds until a knocked-out creature wakes (the server's drain)
	UPROPERTY(Replicated) TWeakObjectPtr<AWRCharacter> Rider;

	FWROnCreatureDied OnCreatureDied;
	/** Spawner that owns this creature (nests aggro its pack). */
	TWeakObjectPtr<class AWRCreatureSpawner> Spawner;

protected:
	virtual void BeginPlay() override;
	virtual void OnDeath(AActor* Killer) override;
	void ApplyDef();
	/** Visual only: play a one-shot clip from the creature's anims (e.g. a boss's "roar"), if it has one. */
	void PlayCreatureOneShot(FName Key);

private:
	UFUNCTION() void OnRep_CreatureId();
	UFUNCTION() void OnRep_Tamed();
	UFUNCTION(Server, Reliable) void ServerDismount();
	void RideMove(const FInputActionValue& Value);
	void RideLook(const FInputActionValue& Value);

	// The body (WRCarcass.cpp).
	/** Server, at death: a wild creature with loot becomes a body to butcher. False: the old way (loot to the killer). */
	bool BecomeBody(AActor* Killer);
	UFUNCTION() void OnRep_Body();
	UPROPERTY(ReplicatedUsing=OnRep_Body) uint8 BodyHitsLeft = 0;
	struct FBodyLoot { FName Item; int32 Left = 0; int32 Total = 0; };
	TArray<FBodyLoot> BodyLoot;
	FString BodyClaim;             // the killer (or the killing tame's owner) has it first
	double BodyClaimUntil = 0.0;
	float BodyCarry = 0.f;         // bare hands keep part of a unit: the rest of it carries to the next hit
	bool bDeathShown = false;      // this machine has shown the death (the clip holds its last pose)

	TWeakObjectPtr<AActor> AggroTarget;
	FVector Home = FVector::ZeroVector;
	FString TamerName;
	float FedUntil = 0.f;
	float MealValue = 1.f;           // taming value of the meal being digested
	TMap<FName, int32> TameStash;    // knockout taming: food given, eaten a meal at a time
	bool bCalm = false;
	double HungryAt = 0.0;
	double EatingUntil = 0.0;
	double FleeUntil = 0.0;
	TWeakObjectPtr<AActor> FleeFrom;
	double PackLossAt = -1000.0;
	double LastHitAt = -1000.0;
	double WokenUntil = 0.0;
	void UpdateTameForecast();
	float UtilityClock = 0.f;
	float ProduceClock = 0.f;
	float TamerSpeed = 1.f;

	UPROPERTY(VisibleAnywhere) TObjectPtr<USpringArmComponent> RideBoom;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> RideCamera;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> GlowLight;
	UPROPERTY(VisibleAnywhere) TObjectPtr<class USkeletalMeshComponent> Skel;
	UPROPERTY(Transient) TMap<FName, TObjectPtr<class UAnimSequenceBase>> AnimAssets;
	double OneShotUntil = 0.0;
	int32 AttackVariant = 0;  // attack, attack2, attack3 take turns
	// Attack wind-up telegraph (look only, on every machine that draws): from a swing's start until its move lands the
	// creature rears back, glows (M_WindupGlow over its own materials) and gives a warning call (creature.windup).
	// Numbers in tuning.json telegraph.*; the wind-up lengths are the attacks' own (creature.biteTiming, creatureAI.<kind>).
	void StartTelegraph(EWRSwingKind Kind, float Duration);
	void TickTelegraph();
	void EndTelegraph();
	double TelegraphStart = -1.0;
	double TelegraphStrike = -1.0;
	FName TelegraphMove;
	bool bTelegraphPosed = false;
	FVector SkelBaseLocation = FVector::ZeroVector;   // the fitted mesh transform the rear-back offsets
	FRotator SkelBaseRotation = FRotator::ZeroRotator;
	UPROPERTY(Transient) TObjectPtr<class UMaterialInstanceDynamic> TelegraphGlow;
	UPROPERTY(Transient) TObjectPtr<class UMeshComponent> TelegraphMesh;
	float LastYaw = 0.f;      // for its turn rate (feet step while it turns on the spot)
	float EarshotClock = 0.f;
	class UWRAnimInstance* CreatureAnim() const;
	/** The layered animation instance: locomotion paced to the ground speed, attacks / hits over it, crossfades. */
	void SetupCreatureAnim(const FWRCreatureDef& Def);
	void TickCreatureAnim(float DeltaSeconds);
	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> RideMapping;
	UPROPERTY(Transient) TObjectPtr<UInputAction> RideMoveAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> RideLookAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> RideJumpAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> RideDismountAction;
};

/** Stationary target for testing combat. Never dies; resets to full health a few seconds after the last hit. */
UCLASS()
class WILDREACH_API AWRTrainingDummy : public AWRCreature
{
	GENERATED_BODY()

public:
	AWRTrainingDummy(const FObjectInitializer& ObjectInitializer);
	virtual void Tick(float DeltaSeconds) override;
	virtual void Die(AActor* Killer) override;
	virtual void NotifyAttackedBy(AActor* Attacker) override;

private:
	double LastHitTime = -1000.0;
};

/** Server-side creature brain: wild behaviours by archetype, and tame orders (follow/stay/fight). */
UCLASS()
class WILDREACH_API AWRCreatureAI : public AAIController
{
	GENERATED_BODY()

public:
	AWRCreatureAI();
	virtual void Tick(float DeltaSeconds) override;

private:
	AActor* PickTarget(AWRCreature* Self) const;
	/** A hungry carnivore's prey: the nearest wild non-carnivore small enough to take (pack hunters take bigger). */
	AWRCreature* PickPrey(AWRCreature* Self) const;
	void TickTame(AWRCreature* Self, float DeltaSeconds);
	void TickWild(AWRCreature* Self, float DeltaSeconds);
	void ChaseAndBite(AWRCreature* Self, AWRCharacterBase* Target);
	/** Day animals sleep at night, night hunters by day (unless something's going on). */
	void UpdateSleep(AWRCreature* Self);
	// Special attacks (creatureAI.<kind> in tuning.json): a telegraphed windup, then the move.
	bool TryStartSpecial(AWRCreature* Self, AWRCharacterBase* Target, float Dist);
	void TickSpecial(AWRCreature* Self, float DeltaSeconds);
	void EndSpecial(AWRCreature* Self);
	FName Special;
	int32 SpecialPhase = 0;  // 0 none, 1 windup, 2 under way
	float SpecialClock = 0.f;
	FVector SpecialDir = FVector::ForwardVector;
	bool bSpecialHit = false;
	TWeakObjectPtr<AWRCharacterBase> SpecialTarget;
	TMap<FName, double> SpecialReadyAt;
	double BiteTrackUntil = 0.0;  // a bite's wind-up turns toward the target until then, then commits
	double NextSnapAt = 0.0;      // its next bite at running prey
	double NextChaseLogAt = 0.0;  // verbose log while hunting
	float ThinkTimer = 0.f;
	float FlankAngle = 0.f;
	FVector WanderGoal = FVector::ZeroVector;
	float WanderClock = 0.f;
};

/** Keeps a number of creatures alive around itself; nesting species get an egg nest. */
UCLASS()
class WILDREACH_API AWRCreatureSpawner : public AActor
{
	GENERATED_BODY()

public:
	AWRCreatureSpawner();
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category="Spawner") FName CreatureId = TEXT("cinderfang");
	UPROPERTY(EditAnywhere, Category="Spawner") int32 Count = 3;
	UPROPERTY(EditAnywhere, Category="Spawner") float Radius = 400.f;
	UPROPERTY(EditAnywhere, Category="Spawner") float RespawnSeconds = 20.f;
	/** 0: the pack is always out. Otherwise it only comes out while a player is within this range (cm) and fades away
	 *  after wildlife.despawnSeconds with nobody near (the island's wildlife sites). */
	UPROPERTY(EditAnywhere, Category="Spawner") float ActivationRadius = 0.f;
	/** Set each creature (and the nest) on the ground under its spawn point: sites on open terrain. */
	UPROPERTY(EditAnywhere, Category="Spawner") bool bTraceGround = false;

	/** Everyone in this spawner's pack turns on an attacker (herds defend; nests swarm). */
	void AlertPack(AActor* Attacker);
	/** One of the pack died: the rest lose heart (see AWRCreature::NotePackLoss). */
	void NotifyPackLoss(const AWRCreature* Dead);
	bool IsActive() const { return bActive; }
	int32 NumAlive() const;

	/** Server start: a spawner for every wildlife site in Data/island_spawns.json made for this map. Returns the count. */
	static int32 SpawnSitesFromData(UWorld* World);

protected:
	virtual void BeginPlay() override;

private:
	void SpawnOne();
	void Activate();
	void Deactivate();
	bool PlayerNear(float Range) const;
	FVector GroundAt(const FVector& Point) const;
	TArray<TWeakObjectPtr<AWRCreature>> Alive;
	TArray<double> PendingRespawns;
	bool bActive = false;
	double AloneSince = -1.0;
};

/** Egg nest for nesting species (Kilnmite, Rimeshell). Taking the egg angers the pack. */
UCLASS()
class WILDREACH_API AWRNest : public AActor
{
	GENERATED_BODY()

public:
	AWRNest();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated) FName CreatureId;
	UPROPERTY(Replicated) bool bHasEgg = true;
	TWeakObjectPtr<AWRCreatureSpawner> Spawner;

	FString TakeEgg(AWRPlayerController* Player);

private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<class UStaticMeshComponent> Mesh;
	FTimerHandle RegrowTimer;
};
