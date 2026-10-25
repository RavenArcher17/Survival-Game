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

	// Art: a skeletal mesh replaces the placeholder shape when set (scaled to the capsule, feet on the ground).
	FString Mesh;
	TMap<FName, FString> Anims;   // idle, walk, run, attack, hit, death; flyers also fly (moving) / hover (still) in the air
	TArray<FString> Materials;    // per-slot overrides ("" keeps the mesh's own), e.g. a tinted pack creature
	float MeshYaw = 0.f;
	float MeshScale = 1.f;        // multiplier on the auto fit
	float Glow = 0.f;             // > 0, no mesh: a glowing orb (emissive sphere in its colour + a light this bright)

	bool CanTameBy(FName Method) const { return TamingMethods.Contains(Method); }
	float TameSeconds() const;
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

	TWeakObjectPtr<AActor> AggroTarget;
	FVector Home = FVector::ZeroVector;
	FString TamerName;
	float FedUntil = 0.f;
	float MealValue = 1.f;           // taming value of the meal being digested
	TMap<FName, int32> TameStash;    // knockout taming: food given, eaten a meal at a time
	bool bCalm = false;
	void UpdateTameForecast();
	float UtilityClock = 0.f;
	float ProduceClock = 0.f;
	float TamerSpeed = 1.f;

	UPROPERTY(VisibleAnywhere) TObjectPtr<USpringArmComponent> RideBoom;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> RideCamera;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> GlowLight;
	UPROPERTY(VisibleAnywhere) TObjectPtr<class USkeletalMeshComponent> Skel;
	UPROPERTY(Transient) TMap<FName, TObjectPtr<class UAnimSequenceBase>> AnimAssets;
	FName CurrentAnim;
	double OneShotUntil = 0.0;
	int32 AttackVariant = 0;  // attack, attack2, attack3 take turns
	/** Duration > 0 fits a one-shot to that many seconds (within limits), e.g. an attack to its telegraph. */
	void PlayCreatureAnim(FName Key, bool bLoop, float Duration = 0.f);
	void TickCreatureAnim();
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
	void TickTame(AWRCreature* Self, float DeltaSeconds);
	void TickWild(AWRCreature* Self, float DeltaSeconds);
	void ChaseAndBite(AWRCreature* Self, AWRCharacterBase* Target);
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

	/** Everyone in this spawner's pack turns on an attacker (herds defend; nests swarm). */
	void AlertPack(AActor* Attacker);

protected:
	virtual void BeginPlay() override;

private:
	void SpawnOne();
	TArray<TWeakObjectPtr<AWRCreature>> Alive;
	TArray<double> PendingRespawns;
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
