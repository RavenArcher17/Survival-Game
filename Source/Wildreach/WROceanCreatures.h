// The Ocean's creatures (cooks/biome-ocean.md 2.12; cooks/build-biome-ocean.md 2.12): no sea pack exists in Content/, so
// all eleven are built in code the Sky's way (UWROceanBeast: a procedural body re-posed every frame from how the real
// animal moves), their state on a replicated body component (UWROceanBody), their special moves as hooks beside the other
// lands' in AWRCreatureAI (FWROceanAI), and their populations by sub-zone, depth and the clock (UWROceanLife).
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ProceduralMeshComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "WROceanCreatures.generated.h"

class AWRCreature;
class AWRCreatureAI;
class AWRCharacterBase;
class UMaterialInstanceDynamic;
struct FWRAttackRow;
struct FWRHit;
enum class EWRHitResult : uint8;

namespace WROceanFlag
{
	enum : uint32
	{
		Jaw       = 1 << 0,   // the Reefjaw's jaw shot out / the Tidecoil's mouth wide / the Wavecharger's head back
		Gape      = 1 << 1,   // the slow tell gape (thrash-drag, second jaw: the inner jaw shows)
		Buried    = 1 << 2,   // a Spinecrab under the sand: only the eyestalks show
		Guard     = 1 << 3,   // a Spinecrab's claws folded over its face
		Flipped   = 1 << 4,   // a Spinecrab on its back (its guard broken)
		Cocked    = 1 << 5,   // the club claw drawn back (the ratchet)
		Bands     = 1 << 6,   // a Tidecoil's organs lit (the shock pulse)
		Flare     = 1 << 7,   // a Glowray's spots flaring
		Stranded  = 1 << 8,   // beached after a surf lunge (belly up)
		Asleep    = 1 << 9,   // a Wavecharger curled on a stipe, an otter wrapped in kelp
		Pouch     = 1 << 10,  // a Gullraider's throat pouch swollen / a male Wavecharger's brood pouch full
		Carrying  = 1 << 11,  // a Gullraider flying off with what it stole
		InHole    = 1 << 12,  // a Tidecoil drawn back into its hole
		BaitBall  = 1 << 13,  // a Ripplefin school balled up (a shark is near)
		Hauled    = 1 << 14,  // a Driftback hauled out on the beach
		Ring      = 1 << 15,  // a Glowray circling in its night ring
	};
}

/**
 * A sea creature's code-built body (every drawing machine): Kind shark / eel / crab / gull / seahorse / turtle / manta /
 * otter / seacow / puffin / school, scaled; its parts tagged so they move as the animal does (a body wave running to the
 * tail, the slingshot jaw, legs stepping in alternating fours, wings beating, a manta's wave, flippers rowing). Section 0
 * is M_HeldTool (colour + roughness in the vertex colour), section 1 the lights (M_Glow, brighter at night).
 */
UCLASS()
class WILDREACH_API UWROceanBeast : public UProceduralMeshComponent
{
	GENERATED_BODY()

public:
	UWROceanBeast(const FObjectInitializer& ObjectInitializer);
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	void Init(AWRCreature* InOwner, FName InKind, float InScale, int32 InSeed);
	FName GetKind() const { return Kind; }

	struct FV { FVector Rest; uint8 Part = 0; float A = 0.f; float B = 0.f; };
	struct FSection
	{
		TArray<FV> Rest;
		TArray<int32> Tris;
		TArray<FColor> Colors;
		TArray<FVector2D> UVs;
		TArray<FVector> Normals;
		TArray<FVector> Verts;
	};

private:
	void Build();
	void Pose(float Dt);
	FVector Place(const FV& V) const;
	void Commit(bool bCreate);

	TWeakObjectPtr<AWRCreature> Creature;
	FName Kind;
	float Scale = 1.f;
	int32 Seed = 0;
	FSection Body, Glow;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> GlowMat;
	// Pose (eased from the creature's flags and motion).
	float Clock = 0.f, Time = 0.f, Amp = 0.f, Freq = 1.f, JawOut = 0.f, GapeAmt = 0.f, Dead = 0.f, Bury = 0.f, Guard = 0.f, Flip = 0.f;
	float Cock = 0.f, Bands = 0.f, Flare = 0.f, PouchAmt = 0.f, Pitch = 0.f, Bank = 0.f, LastYaw = 0.f, PoseClock = 0.f, GlowNow = 0.f;
	float MinX = 0.f, MaxX = 1.f;
	bool bBuilt = false;
};

UCLASS(ClassGroup=(Creature))
class WILDREACH_API UWROceanBody : public UActorComponent
{
	GENERATED_BODY()

public:
	UWROceanBody();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	static UWROceanBody* Get(const AActor* Actor);
	static TArray<TWeakObjectPtr<UWROceanBody>> Live;
	static bool IsOceanKind(FName CreatureId);
	/** Server, at spawn (AWRCreature): give an Ocean creature its body. */
	static void Attach(AWRCreature* Creature);

	UPROPERTY(ReplicatedUsing=OnRep_Flags) uint32 Flags = 0;
	UPROPERTY(Replicated) int32 Seed = 0;
	/** A Gullraider's stolen stack (shown in its bill; dropped where it dies within 60 s). */
	UPROPERTY(Replicated) FName StolenItem;
	/** A Reefjaw's path-magic calm under way (the Tide walker's name). */
	UPROPERTY(Replicated) FString PathTamer;
	/** Who it holds (every machine: the held one's presses mash the hold). */
	UPROPERTY(Replicated) TObjectPtr<AActor> Holding;
	/** A male Wavecharger's egg being eased out of its pouch (the thief's name). */
	UPROPERTY(Replicated) FString EggThief;
	bool Has(uint32 Flag) const { return (Flags & Flag) != 0; }
	void Set(uint32 Flag, bool bOn);
	UFUNCTION(NetMulticast, Unreliable) void MulticastMoment(FName Effect, float Scale, FName Sound);

	// Server state (FWROceanAI).
	FVector Home = FVector::ZeroVector;        // a Tidecoil's hole, a herd's anchor, a raft's spot
	int32 StolenCount = 0;
	FString StolenFrom;
	double StolenAt = -100.0;
	TWeakObjectPtr<AWRCharacterBase> HeldBody;
	double HoldUntil = 0.0, HoldTickAt = 0.0;
	FName HoldKind;
	float FreeDamage = 0.f;
	double StrandedUntil = 0.0, GuardUntil = 0.0, FlippedUntil = 0.0, ColumnAt = 0.0;
	FVector ColumnSpot = FVector::ZeroVector;
	double NextVoiceAt = 0.0;
	double MobCalledAt = -100.0;
	double NextPingAt = 0.0;
	double PathTameStart = 0.0, EggStart = 0.0;
	FVector HandAt = FVector::ZeroVector;
	FVector DragFrom = FVector::ZeroVector;
	float CruiseDepth = 400.f;     // a swimmer's depth below the surface when it has nothing to do
	float RingPhase = 0.f;
	double NextFindAt = 0.0, NextTrailAt = 0.0, NextSlitherAt = 0.0, BaitUntil = 0.0;
	float GuardStance = 0.f;
	bool bMobCalled = false;
	bool bPoolBound = false;   // a King Tide's pool Reefjaw (x1.3 damage taken)
	bool bStrandedFlat = false; // a King Tide's stranded Kelpgrazer / Driftback

private:
	UFUNCTION() void OnRep_Flags();
	void BuildLook();
	UPROPERTY(Transient) TObjectPtr<UWROceanBeast> Beast;
	int32 LookTries = 0;
};

struct WILDREACH_API FWROceanAI
{
	static bool TickWild(AWRCreatureAI* AI, AWRCreature* Self, float DeltaSeconds);
	static bool TickTame(AWRCreatureAI* AI, AWRCreature* Self, float DeltaSeconds);
	static bool RowAllowed(const AWRCreatureAI* AI, const AWRCreature* Self, const FWRAttackRow& Row, const AWRCharacterBase* Target, bool bReading);
	static void RowStarted(AWRCreatureAI* AI, AWRCreature* Self, const FWRAttackRow& Row, AWRCharacterBase* Target);
	static bool StrikeRow(AWRCreatureAI* AI, AWRCreature* Self, const FWRAttackRow& Row, AWRCharacterBase* Target);
	static void Resolved(AWRCreature* Self, AWRCharacterBase* Victim, FName RowId, EWRHitResult Result);
	static float Incoming(const AWRCreature* Self, const FWRHit& Hit, float Damage);
	static void Damaged(AWRCreature* Self, const FWRHit& Hit, float Damage);
	static void Died(AWRCreature* Self, AActor* Killer);
	/** Server, every tick (the body): holds, strandings, the Gullraider's flight home, the herd's sleep, the school. */
	static void TickBody(AWRCreature* Self, UWROceanBody* Body, float DeltaTime);
	static void Release(AWRCreature* Self, UWROceanBody* Body);
	static void Mash(AWRCharacterBase* Victim);
	static AWRCreature* HolderOf(const AActor* Victim);
	/** Every tick of a row's strike (the drag's line, the breach's rise). */
	static void TickStrike(AWRCreatureAI* AI, AWRCreature* Self, const FWRAttackRow& Row, float DeltaSeconds);
	/** A sea creature that never leaves the water on its own (all but crabs, birds and a hauling Driftback). */
	static bool WaterOnly(const AWRCreature* Self);
	/** Server: E on a sleeping male Wavecharger (its pouch full): eases an egg out over eggSeconds; false = not that. */
	static bool TryStealEgg(AWRCreature* Male, class AWRPlayerController* Player, FString& OutResult);
	static bool EggPrompt(const AWRCreature* Male, const AActor* Who, FString& OutVerb);
	/** The Reefjaw's path magic (10+ Tide points, in the water with it, knocked out or soaked under 30%): held 4 s. */
	static bool PathTamePrompt(const AWRCreature* Self, const AActor* Who, FString& OutVerb);
	static bool TryPathTame(AWRCreature* Wild, class AWRPlayerController* Player, FString& OutResult);
	static void StopPathTame(class AWRPlayerController* Player);
	static bool PathTameNear(const class APlayerController* Player);
};

/**
 * The Ocean's populations (server; cooks/biome-ocean.md 2.12.3): every few seconds, near survivors, each sub-zone's day
 * or night table fills its share out of sight (swimmers in water within their depth band, crabs on the sand, birds on the
 * cliffs, schools in the kelp) and empties what no one is near; the living-world beats (the kills at the kelp edge, the
 * otter rafts, the turtles hauling out, the Glowray rings, the bait balls). Numbers in tuning oceanLife.
 */
UCLASS()
class WILDREACH_API UWROceanLife : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;
	static UWROceanLife* Get(const UObject* WorldContext);
	/** Server: spawn one of Kind at At (in its place's way: a hole for a Tidecoil, buried for a crab...). */
	AWRCreature* SpawnOne(FName Kind, const FVector& At, FName Zone);
	/** Server: an Ocean creature died (the kill at the kelp edge: a blood cloud and the gulls come). */
	void NoteDeath(AWRCreature* Dead, AActor* Killer);
	TArray<TWeakObjectPtr<AWRCreature>> Spawned;

private:
	void StepPopulations();
	void StepBeats();
	bool FindSpot(FName Kind, FName Zone, const FVector& Near, const FVector& Facing, FVector& Out) const;
	bool bOn = false;
	float Clock = 0.f, BeatClock = 0.f;
};
