// The Desert's creatures (cooks/biome-desert.md 2.12; cooks/build-biome-desert.md 2.12). The Duskjackal and the Carrion
// kite stand on real rigs with real clips (the Barghest and the Griffon, re-dyed as a black gilt-dusted jackal and a
// rust-stained lammergeier); the other eight are built in code the Ocean's way (UWRDesertBeast: a body re-posed every
// frame from how the real animal moves). Their state is a replicated body component (UWRDesertBody), their special moves
// hooks beside the other lands' in AWRCreatureAI (FWRDesertAI): the Dunejaw swimming under soft sand on the fin, drawn by a
// sprint's vibration, bursting up from below and beached on firm ground; the Glasstail's glare by daylight (only if you
// face it), its night ambush from the sand and its grab-and-sting; the kite's circle and call over the wounded and its
// bone drop; the jackals' chorus and flank hamstring; the Gildback's false head and roll; the Dromad's dulla and spit;
// the Shimmerbuck's mirage; the Scarab digger's ball; the Wellfrog in its clay. Populations by sub-zone and the clock
// with the living beats (UWRDesertLife). Numbers in tuning desertLife.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ProceduralMeshComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRDesertCreatures.generated.h"

class AWRCreature;
class AWRCreatureAI;
class AWRCharacterBase;
class AWRPlayerController;
class UMaterialInstanceDynamic;
struct FWRAttackRow;
struct FWRHit;
enum class EWRHitResult : uint8;

namespace WRDesertFlag
{
	enum : uint32
	{
		Diving    = 1 << 0,   // a Dunejaw swimming under the soft sand: only its grit fin shows
		Buried    = 1 << 1,   // a Glasstail waiting under the sand at night / a Wellfrog in its clay
		Beached   = 1 << 2,   // a Dunejaw lying out after a burst onto firm ground (belly up, the lock point)
		Jaw       = 1 << 3,   // a bite's jaws open / a pincer snapped / a horn thrust
		Raised    = 1 << 4,   // the Glasstail's sunglass bulb raised to the sun (the glare's tell) / the sting cocked
		Grip      = 1 << 5,   // pincers closed on someone
		Ducked    = 1 << 6,   // a Gildback's real head tucked under its false-head hump
		Rolling   = 1 << 7,   // a Gildback curled into a spiked ball
		Flaring   = 1 << 8,   // spines blazing white (the flare's tell)
		Dulla     = 1 << 9,   // a Dromad bull's pink throat sac blown out
		Circling  = 1 << 10,  // a Carrion kite circling a wounded body (or a kill)
		Ball      = 1 << 11,  // a Scarab digger rolling its ball of sand
		Asleep    = 1 << 12,  // a herd's night rest (Shimmerbucks in their rings, Dromads couched)
		Basking   = 1 << 13,  // a Gildback basking on the Pan at noon (calm)
		Glowing   = 1 << 14,  // the spines' stored light / the Glasstail's moonlit cyan
		Spray     = 1 << 15,  // a Dunejaw kicking sand with its front paddles
	};
}

/**
 * A desert creature's code-built body (every drawing machine): Kind scorpion / skink / devil / camel / oryx / scarab /
 * grouse / frog, scaled; its parts tagged so they move as the animal does (eight legs stepping in alternating fours and a
 * tail that arches and strikes; a skink's sideways S-wave and the fin over the sand; a thorny devil's rocking jerky walk
 * and its curl; a camel's pace with its neck bobbing and the dulla; an oryx's gait and its horns; a dung beetle walking
 * backwards behind its ball; a sandgrouse's flapping; a frog's squat and hop). Section 0 is M_HeldTool (colour + roughness
 * in the vertex colour), section 1 the lights (M_Glow: the sunglass bulb, the spines, the night cyan).
 */
UCLASS()
class WILDREACH_API UWRDesertBeast : public UProceduralMeshComponent
{
	GENERATED_BODY()

public:
	UWRDesertBeast(const FObjectInitializer& ObjectInitializer);
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	void Init(AWRCreature* InOwner, FName InKind, float InScale, int32 InSeed);
	/** A still copy (the Shimmerbuck's mirage): posed once, no shadow, a shimmer, gone in Seconds. */
	void InitMirage(AWRCreature* Of, FName InKind, float InScale, int32 InSeed, float Seconds);
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
	float Clock = 0.f, Time = 0.f, Amp = 0.f, Freq = 1.f, JawOut = 0.f, RaiseAmt = 0.f, GripAmt = 0.f, Dead = 0.f, Bury = 0.f, Dive = 0.f, Beach = 0.f;
	float Duck = 0.f, Roll = 0.f, RollSpin = 0.f, FlareAmt = 0.f, DullaAmt = 0.f, Sleep = 0.f, Fly = 0.f, SprayAmt = 0.f, PoseClock = 0.f, GlowNow = 0.f;
	float MinX = 0.f, MaxX = 1.f, MirageLeft = 0.f;
	bool bBuilt = false, bMirage = false;
};

UCLASS(ClassGroup=(Creature))
class WILDREACH_API UWRDesertBody : public UActorComponent
{
	GENERATED_BODY()

public:
	UWRDesertBody();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	static UWRDesertBody* Get(const AActor* Actor);
	static TArray<TWeakObjectPtr<UWRDesertBody>> Live;
	static bool IsDesertKind(FName CreatureId);
	/** Server, at spawn (AWRCreature): give a desert creature its body. */
	static void Attach(AWRCreature* Creature);

	UPROPERTY(ReplicatedUsing=OnRep_Flags) uint32 Flags = 0;
	UPROPERTY(Replicated) int32 Seed = 0;
	/** Who its pincers hold (every machine: the held one shows it). */
	UPROPERTY(Replicated) TObjectPtr<AActor> Holding;
	bool Has(uint32 Flag) const { return (Flags & Flag) != 0; }
	void Set(uint32 Flag, bool bOn);
	UFUNCTION(NetMulticast, Unreliable) void MulticastMoment(FName Effect, float Scale, FName Sound);
	/** The Shimmerbuck's mirage: every machine stands a still, shadowless copy of it here for Seconds. */
	UFUNCTION(NetMulticast, Unreliable) void MulticastMirage(FVector At, float Yaw, float Seconds);

	// Server state (FWRDesertAI).
	FVector Home = FVector::ZeroVector;
	TWeakObjectPtr<AWRCharacterBase> HeldBody;
	double HoldUntil = 0.0, BeachedUntil = 0.0, NextHissAt = 0.0, NextVoiceAt = 0.0, CircleUntil = 0.0, CircleStart = 0.0, ChorusAt = -100.0;
	double MirageReadyAt = 0.0, RollUntil = 0.0, DuckUntil = 0.0, BaskCheckAt = 0.0, FaceEastUntil = 0.0;
	float FreeDamage = 0.f;
	float CircleAngle = 0.f;
	TWeakObjectPtr<AActor> CircleOver;
	bool bChorused = false;

private:
	UFUNCTION() void OnRep_Flags();
	void BuildLook();
	UPROPERTY(Transient) TObjectPtr<UWRDesertBeast> Beast;
	int32 LookTries = 0;
};

struct WILDREACH_API FWRDesertAI
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
	/** Server, every tick (the body): the grip's sting, beaching, the kite's circle, the Gildback's glow and bask. */
	static void TickBody(AWRCreature* Self, UWRDesertBody* Body, float DeltaTime);
	/** Every tick of a row's strike (the Gildback's roll, the burst's rise). */
	static void TickStrike(AWRCreatureAI* AI, AWRCreature* Self, const FWRAttackRow& Row, float DeltaSeconds);
	/** A Glasstail holding Victim (the held can't act; an ally's blow on its tail frees them). */
	static AWRCreature* HolderOf(const AActor* Victim);
	/** A sand-swimmer (Dunejaw, Ssarrok) can dive here: soft sand under it, never wet sand, hardpan or rock. */
	static bool CanDive(const AActor* Who);
	/** E on a wild Wellfrog in its clay: squeeze it for a drink (it lives). */
	static bool SqueezePrompt(const AWRCreature* Self, const AActor* Who, FString& OutVerb);
	static bool TrySqueeze(AWRCreature* Wild, AWRPlayerController* Player, FString& OutResult);
};

/**
 * The Desert's populations (server; cooks/biome-desert.md 2.12.3): every few seconds, near survivors, each sub-zone's day
 * or night table fills its share out of sight (Dunejaws only on soft sand, Wellfrogs in the clay pans and oases, kites in
 * the air over the Needles and the dunes, the herds at the water) and empties what no one is near; the living-world beats
 * (Dunejaws hunting Shimmerbucks in the troughs, jackals harrying Dromad stragglers at night, kites circling every kill,
 * Gildbacks basking on the Pan at noon, the Sandhens' dawn flights to water, the Dromads turning east at Ahrumet's dawn
 * song). Numbers in tuning desertLife.
 */
UCLASS()
class WILDREACH_API UWRDesertLife : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;
	static UWRDesertLife* Get(const UObject* WorldContext);
	AWRCreature* SpawnOne(FName Kind, const FVector& At, FName Zone);
	/** Server: a creature died in the desert (the kites come to circle it). */
	void NoteDeath(AWRCreature* Dead, AActor* Killer);
	/** Half the spawns of these kinds dig in (the Long Noon's Glasstails and Dunejaws), x more of these (its kites). */
	TMap<FName, float> KindScale;
	TArray<TWeakObjectPtr<AWRCreature>> Spawned;

private:
	void StepPopulations();
	void StepBeats();
	bool FindSpot(FName Kind, FName Zone, const FVector& Near, const FVector& Facing, FVector& Out) const;
	float Clock = 0.f, BeatClock = 0.f;
	int32 LastDawnDay = -1;
};
