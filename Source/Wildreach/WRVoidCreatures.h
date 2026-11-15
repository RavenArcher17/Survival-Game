#pragma once

// The creatures of the Corrupted wasteland (cooks/biome-void.md 2.12; cooks/build-biome-void.md 2.13, 3b): what they do
// beyond their attack rows (Data/attacks.json) and how they look on the stand-in rigs. The Swamp's pattern
// (WRSwampCreatures): a replicated body component for state and look, the AI's hooks, and the land's dens.
//
//  - UWRVoidBody: on every void creature (creatures.json biome "wasteland"). Replicated state (flags, what it holds,
//    where a blink will land) and, on machines that draw, the cook's palette as a tint over the rig's texture plus the
//    silhouette parts built in code on bone frames: the Hollowfang's eye pits and black-glass teeth, the Riftmaw's four
//    jaw-plates round its tooth ring, the Nullwing's torn membranes with the void showing through, the Gnawer's chisel
//    incisors, the Hush's ear fans and long finger, the Voidstrider's drifting filament mane, the Riftback's lens seam,
//    the Driftmote's bell, seed and light rows, the Withergoat's four horns and long ears; the Riftborn layer (desaturated,
//    black-violet eyes, voidglass shards along the spine).
//  - FWRVoidAI: the hooks AWRCreatureAI calls (friend): the blink (tell, ripple, smear), the Riftmaw's rift (lying in it,
//    bursting out of the nearest, going back, stranded on land), the Gnawer latch that eats max health, the Hush's hearing,
//    rise and scream, the Nullwing's void trail and grounding, the Voidstrider's blink kick and ridden blink, catches.
//  - UWRVoidLife: the server's dens over the land (Data/lands/wasteland.json "spawns": day and night tables per zone,
//    Riftmaws in the large rifts, the Hush on the Gauntlet Road at night, Riftborn shares).
//  - AWRVoidTrail: a Nullwing's laid ribbon of void (a placed hazard, not a projectile).

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRVoidCreatures.generated.h"

class AWRCreature;
class AWRCreatureAI;
class AWRCharacterBase;
class AWRLand;
class AWRRift;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UProceduralMeshComponent;
class USkeletalMeshComponent;
struct FWRAttackRow;
struct FWRHit;
struct FWRLookBlock;
enum class EWRHitResult : uint8;

/** What a void body shows, replicated as bits (UWRVoidBody::Flags). */
namespace WRVoidFlag
{
	enum : uint16
	{
		Riftborn = 1 << 0,   // came back from a rift changed: desaturated, black-violet eyes, shards on the spine
		Shudder  = 1 << 1,   // a blink's tell: the body shudders (one hard reversed breath)
		InRift   = 1 << 2,   // a Riftmaw lying in its tear (hidden; only the tooth ring's glint shows in the black)
		Latched  = 1 << 3,   // a Gnawer on a body
		Risen    = 1 << 4,   // the Hush up to full height, ear fans spread (hears x2)
		Holding  = 1 << 5,   // a catch: the Riftmaw's four jaws, the Hush's lift
		Gape     = 1 << 6,   // jaw-plates flared / arms open wide (a grab's tell)
		Trail    = 1 << 7,   // a Nullwing's wing holes flaring as it banks to lay its trail
		Grounded = 1 << 8,   // a Nullwing down (after a dive, a wing broken)
		Drifting = 1 << 9,   // a Driftmote risen high and drifting off (after its pulse)
		Screaming = 1 << 10, // the Hush's ear fans flared, chest heaving
	};
}

/** Every machine: the state of a void creature; on machines that draw, its modelled look. */
UCLASS(ClassGroup=(Creature))
class WILDREACH_API UWRVoidBody : public UActorComponent
{
	GENERATED_BODY()

public:
	UWRVoidBody();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	static UWRVoidBody* Get(const AActor* Actor);
	/** A creatures.json row of the Wasteland (biome "wasteland", not a boss). */
	static bool IsVoidKind(FName CreatureId);
	/** Server: the component on a new void creature (AWRCreature::BeginPlay). */
	static void Attach(AWRCreature* Creature);
	/** Server: the look-only component on a Wasteland boss (WRVoidBosses: its palette and parts; the boss machine fights). */
	static void AttachLook(AWRCreature* Boss);
	/** This machine's void bodies (latches and catches are looked up here, not in a world sweep). */
	static TArray<TWeakObjectPtr<UWRVoidBody>> Live;

	UPROPERTY(Replicated) uint16 Flags = 0;
	/** The body in its jaws or hands (a catch) or the one a Gnawer is latched on. */
	UPROPERTY(Replicated) TObjectPtr<AActor> Holding;
	/** Where a planned blink lands, and the server time it does (every machine draws the ripple there until then). */
	UPROPERTY(Replicated) FVector_NetQuantize BlinkTo;
	UPROPERTY(Replicated) double BlinkAt = 0.0;
	/** The Starving One's growth (each stack +10% size, drawn by every machine). */
	UPROPERTY(Replicated) uint8 Stacks = 0;

	bool Has(uint16 Flag) const { return (Flags & Flag) != 0; }
	void Set(uint16 Flag, bool bOn) { Flags = bOn ? uint16(Flags | Flag) : uint16(Flags & ~Flag); }
	bool IsRiftborn() const { return Has(WRVoidFlag::Riftborn); }

	/** Every machine: a moment worth a burst on the body (a smear, a shriek): an effect at a bone, a sound. */
	UFUNCTION(NetMulticast, Unreliable) void MulticastMoment(FName Effect, FName Bone, float Scale, FName Sound);

	// --- Server state ---
	double BlinkFrom = 0.0;             // when the planned blink's tell began
	bool bBlinkPending = false;
	FName BlinkThen;                    // the row it goes into after the blink (the blink kick's kick), or None
	double NextRiftbornBlinkAt = 0.0;
	TWeakObjectPtr<AWRCharacterBase> HeldBody;
	double HoldUntil = 0.0;
	FName HoldKind;
	TWeakObjectPtr<AWRCharacterBase> LatchedOn;
	double LatchedAt = 0.0;
	float LatchTick = 0.f;
	FVector LatchOffset = FVector::ZeroVector;
	TWeakObjectPtr<AWRRift> HomeRift;
	int32 AttacksOut = 0;               // a Riftmaw's attacks since it came out (it goes back after 3)
	double BackInUntil = 0.0;           // inside its rift until (then it may burst again)
	double RisenUntil = 0.0;
	double ScreamUntil = 0.0;
	double NextListenAt = 0.0;
	double HeardAt = 0.0;               // last time it heard its target
	FVector HeardWhere = FVector::ZeroVector;
	double GroundedUntil = 0.0;
	uint8 WingsSeen = 0;
	double DriftUntil = 0.0;
	double GapeUntil = 0.0;
	double TrailUntil = 0.0;
	double NextVoiceAt = 0.0;
	bool bRolledRiftborn = false;
	/** Server: the Riftborn roll (its den's share), once. */
	static void RollRiftborn(AWRCreature* C, UWRVoidBody* Body);

private:
	void BuildLook();
	void ClearLook();
	void TickLook(float DeltaTime);
	USceneComponent* AddGroup(FName Bone, const FVector& WorldAt, const FRotator& WorldRot, const TArray<FWRLookBlock>& Blocks, int32 Kind);
	void SetGroupGlow(int32 Kind, float Glow);
	USkeletalMeshComponent* Skel() const;

	FName LookFor;
	bool bLookRiftborn = false;
	struct FGroup { TWeakObjectPtr<USceneComponent> Frame; int32 Kind = 0; FRotator BaseRot = FRotator::ZeroRotator; FVector BaseLoc = FVector::ZeroVector; FVector BaseScale = FVector::OneVector; float Glow = -1.f; float Phase = 0.f; };
	TArray<FGroup> Groups;
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> LookComps;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> TintMats;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> Glow;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> RippleFx;
	double RippleShownFor = -1.0;
	float Clock = 0.f, Hover = 0.f, PulseClock = 0.f, RiseAlpha = 0.f, GapeAlpha = 0.f;
	FVector MeshBase = FVector::ZeroVector;
	FVector MeshScale0 = FVector::ZeroVector;
	bool bMeshBaseSaved = false;
	int32 LookTries = 0;
	bool bPawnIgnored = false;
};

/** The hooks AWRCreatureAI calls for void creatures (friend of AWRCreatureAI). */
struct WILDREACH_API FWRVoidAI
{
	/** Wild brain: true when it ran this tick itself (in its rift, a pending blink, a latch, a catch, listening). */
	static bool TickWild(AWRCreatureAI* AI, AWRCreature* Self, float DeltaSeconds);
	static bool RowAllowed(const AWRCreatureAI* AI, const AWRCreature* Self, const FWRAttackRow& Row, const AWRCharacterBase* Target, bool bReading);
	static void RowStarted(AWRCreatureAI* AI, AWRCreature* Self, const FWRAttackRow& Row, AWRCharacterBase* Target);
	/** A row's strike: true when this took it over (blink, latch, trail, rise, scream, retreat, the rift burst, void spit). */
	static bool StrikeRow(AWRCreatureAI* AI, AWRCreature* Self, const FWRAttackRow& Row, AWRCharacterBase* Target);
	/** How a row's blow came out: the catches and the hamstring's slow. */
	static void Resolved(AWRCreature* Self, AWRCharacterBase* Victim, FName RowId, EWRHitResult Result);
	/** A Nullwing down on the ground. */
	static bool Grounded(const AWRCreature* Self);
	/** Damage on a void body before it lands (nothing reaches a Riftmaw inside its tear). */
	static float Incoming(const AWRCreature* Self, const FWRHit& Hit, float Damage);
	/** At its death (server): lets go; a Riftborn's extra void essence. */
	static void Died(AWRCreature* Self, AActor* Killer);
	/** Its row blows: void element on its bites, a Riftborn's curse. */
	static void ModifyHit(const AWRCreature* Self, FWRHit& Hit, const FWRAttackRow* Row);
	/** Server: the body's own life each tick (blinks landing, latches eating, catches held, the Riftmaw's rift). */
	static void TickBody(AWRCreature* Self, UWRVoidBody* Body, float DeltaTime);
	/** A ridden Voidstrider can blink (Dodge while mounted). */
	static bool WantsRideBlink(const AWRCreature* Self);
	/** Server: the ridden blink: 5 m ahead, the rider's i-frames over the travel. */
	static void RideBlink(AWRCreature* Self);
	/** The Hush's hearing (every machine can ask; the server decides): how far Who is heard now, cm. */
	static float NoiseReach(const AWRCharacterBase* Who);
	/** Server: knock every Gnawer off a body (a roll, a swing). Returns how many came off. */
	static int32 ShakeOff(AActor* Victim);
	static int32 LatchCount(const AActor* Victim);

	// The body's moves (UWRVoidBody and the brain call them).
	static void PlanBlink(AWRCreature* Self, UWRVoidBody* Body, AWRCharacterBase* Target, float Lead, bool bBehind, FName Then);
	static void LandBlink(AWRCreature* Self, UWRVoidBody* Body);
	static void StartHold(AWRCreature* Self, UWRVoidBody* Body, AWRCharacterBase* Victim, FName Kind, float Seconds);
	static void Release(AWRCreature* Self, UWRVoidBody* Body, bool bThrow);
	static bool StartLatch(AWRCreature* Self, UWRVoidBody* Body, AWRCharacterBase* Victim);
	static void Unlatch(AWRCreature* Self, UWRVoidBody* Body, bool bThrown);
	static void IntoRift(AWRCreature* Self, UWRVoidBody* Body, AWRRift* Rift);
	static void OutOfRift(AWRCreature* Self, UWRVoidBody* Body, const FVector& At);
	static void LayTrail(AWRCreature* Self, const FWRAttackRow& Row, AWRCharacterBase* Target);
	static void Scream(AWRCreature* Self, const FWRAttackRow& Row);
};

/** A Nullwing's void trail: a 30 m black ribbon with a lensing edge, laid across a route 1-4 m up, for 6 s. Its ends,
 *  height and start time replicate; every machine draws the same ribbon; the server judges touches. */
UCLASS(NotPlaceable)
class WILDREACH_API AWRVoidTrail : public AActor
{
	GENERATED_BODY()

public:
	AWRVoidTrail();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(Replicated) FVector_NetQuantize A;
	UPROPERTY(Replicated) FVector_NetQuantize B;
	UPROPERTY(Replicated) float Thick = 100.f;
	UPROPERTY(Replicated) float Seconds = 6.f;
	UPROPERTY(Replicated) double Born = 0.0;
	TWeakObjectPtr<AActor> Layer;
	float TickDamage = 8.f, TickEvery = 0.5f, CursePerSecond = 30.f;

protected:
	virtual void BeginPlay() override;

private:
	void BuildRibbon();
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Ribbon;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Mat;
	TMap<TWeakObjectPtr<AActor>, double> LastTick;
	double Now() const;
};

/** Server: the land's dens (Data/lands/wasteland.json "spawns"). */
UCLASS()
class WILDREACH_API UWRVoidLife : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	/** Server, once the land's places and rifts exist (WRVoidWorld::LandReady). */
	static void PlaceDens(AWRLand* Land);
};
