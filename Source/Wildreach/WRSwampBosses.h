#pragma once

// The Swamp's bosses (cooks/biome-swamp.md 2.13-2.14, 2.10, 2.21; cooks/build-biome-swamp.md 2.7, 2.10; cooks/combat.md 2.17):
//  - Mother Maw at her nest: the broken boardwalk square (AWRSwampBossProp "walk" sections her head-slam breaks and you
//    mend with 2 wood in 3 s), her rears on the planks (belly open x1.3, 1.2 s more punish), duckweed burst and death roll
//    from open water, fire on the nest mound drawing her out for 8 s, two young at 60% and 30%.
//  - The Hex-Warden on the Ossuary: four fetish totems (crow, bone, blood, voice: "hex_totem" creatures standing in their
//    poles), crow scatter, sickle dance, hex bolt, mimic call, blood harvest, the mask that breaks; her Red Tide night walk
//    with the hexed beasts round her.
//  - The Bloat on its loop (swamp.json arenas.bloat.route): asleep at the Wallow by day, the gorge meter on its body, three
//    veins, latch / slam / roll-crush / spray / burst, salt, fire and frost, the Mudhulk herds, Mother Maw, the Drowned
//    Causeway on the Red Tide, leading Blood invasions.
//  - Vharra, Queen of the Mire, in the Mother's Throat: anchored to the west wall by her sucker disc, the six Rodwarden
//    rods (AWRSwampRod) and the arena's own water (AWRSwampFlood) that they electrify, the Sinking, the swimming queen,
//    the hidden phase where the flood turns to blood, the sacs, the Blood path altar.
//
// Moves are rows in Data/attacks.json like every boss's; numbers are in Data/tuning.json "swampBosses"; arenas come from
// Data/swamp.json "arenas" (nothing here runs without it). UWRSwampArenas places the bosses, props and the altar.

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PhysicsVolume.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRCreature.h"
#include "WRCombatTypes.h"
#include "WRSwampBosses.generated.h"

class AWRBoss;
class AWRCharacterBase;
class AWRPlayerController;
class UWRSwampArenas;
class UBoxComponent;
class UMaterialInstanceDynamic;
class USkeletalMeshComponent;
struct FWRBossAttack;
struct FWRSwampBossState;
struct FWRStrikeCall;
struct FWRLookBlock;

/** What a swamp boss's body shows, replicated as bits (UWRSwampBossBody::Flags). */
namespace WRSwampBossFlag
{
	enum : uint16
	{
		Rearing     = 1 << 0,   // Mother Maw up out of the water on a boardwalk (her belly open)
		InWater     = 1 << 1,   // in open water (Maw: burst and roll; Vharra: shockable)
		MaskBroken  = 1 << 2,   // the Hex-Warden's crow-skull mask broke (garnet eyes)
		Scattered   = 1 << 3,   // the Hex-Warden is forty crows
		HexWalk     = 1 << 4,   // her Red Tide night walk (a garnet haze round her)
		Asleep      = 1 << 5,   // the Bloat asleep at the Wallow
		Latched     = 1 << 6,   // the Bloat's disc on something (its body pumps)
		Convulsing  = 1 << 7,   // Vharra thrashing in shocked water
		JawOpen     = 1 << 8,   // Vharra's hood split into three
		Free        = 1 << 9,   // Vharra let go of the wall (phase 2)
		Swimming    = 1 << 10,  // Vharra under the flood
		Defending   = 1 << 11,  // Mother Maw out of the water at her nest
		Glowing     = 1 << 12,  // the Bloat's burst tell (near white-garnet)
		Burning     = 1 << 13,  // the Bloat on fire (no latch)
		Casting     = 1 << 14,  // the Hex-Warden's mask eyes lit (hex bolt, mimic call)
		Rising      = 1 << 15,  // Vharra reared to full height (the Sinking's tell)
	};
}

/** A swamp boss's replicated state and, on machines that draw, its modelled look (palette over the stand-in rig and the
 *  parts built in code: Mother Maw's duckweed mat, teeth bar and hook, the Hex-Warden's mask, cloak, corset and sickles,
 *  the Bloat's veins, eyes and three-jaw disc, Vharra's hood plates, eyes, sacs and ringed trunk to her anchor disc).
 *  On a hex totem it only hides the stand-in shape (the pole is an AWRSwampBossProp; the creature is the hit body). */
UCLASS(ClassGroup=(Creature))
class WILDREACH_API UWRSwampBossBody : public UActorComponent
{
	GENERATED_BODY()
	// The boss hooks draw and drive it (Vharra's look lives in WRSwampVharra.cpp).
	friend struct FWRSwampBoss;

public:
	UWRSwampBossBody();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	static UWRSwampBossBody* Get(const AActor* Actor);
	/** Server: the component on a swamp boss or a hex totem. */
	static UWRSwampBossBody* Attach(AWRCreature* Creature);

	UPROPERTY(Replicated) uint16 Flags = 0;
	/** The Bloat's gorge (0..100). */
	UPROPERTY(Replicated) uint8 Gorge = 0;
	/** Vharra's three sacs, how full (0..255); which sacs (Vharra) or veins (the Bloat) have burst (bits). */
	UPROPERTY(Replicated) uint8 Sac0 = 0;
	UPROPERTY(Replicated) uint8 Sac1 = 0;
	UPROPERTY(Replicated) uint8 Sac2 = 0;
	UPROPERTY(Replicated) uint8 Burst = 0;
	/** Vharra's trunk: the lash's sweep (degrees off her facing, -127..127) while a tail lash runs; 0 at rest. */
	UPROPERTY(Replicated) int8 Lash = 0;
	/** Vharra's anchor disc on the wall (phase 1). */
	UPROPERTY(Replicated) FVector_NetQuantize Anchor = FVector::ZeroVector;

	bool Has(uint16 Flag) const { return (Flags & Flag) != 0; }
	void Set(uint16 Flag, bool bOn) { Flags = bOn ? uint16(Flags | Flag) : uint16(Flags & ~Flag); }
	float SacFill(int32 i) const { return (i == 0 ? Sac0 : i == 1 ? Sac1 : Sac2) / 255.f; }
	void SetSac(int32 i, float Fill) { const uint8 V = uint8(FMath::Clamp(FMath::RoundToInt(Fill * 255.f), 0, 255)); if (i == 0) Sac0 = V; else if (i == 1) Sac1 = V; else Sac2 = V; }

	/** Every machine: a moment on the body (crows bursting, a vein spraying, the mask shattering). */
	UFUNCTION(NetMulticast, Unreliable) void MulticastMoment(FName Effect, FVector_NetQuantize At, float Scale, FName Sound);

private:
	void BuildLook();
	void ClearLook();
	void TickLook(float DeltaTime);
	USkeletalMeshComponent* Skel() const;
	USceneComponent* AddGroup(FName Bone, const FVector& WorldAt, const FRotator& WorldRot, const TArray<FWRLookBlock>& Blocks, int32 Kind);
	void SetGroupGlow(int32 Kind, float Glow);
	void SetGroupVisible(int32 Kind, bool bVisible);
	void HideStandIn();
	void PlaceTrunk(float DeltaTime);

	FName LookFor;
	struct FGroup { TWeakObjectPtr<USceneComponent> Frame; int32 Kind = 0; FRotator BaseRot = FRotator::ZeroRotator; FVector BaseLoc = FVector::ZeroVector; FVector BaseScale = FVector::OneVector; float Glow = -1.f; bool bVisible = true; };
	TArray<FGroup> Groups;
public:
	/** Every look component (the Vharra track's frame helper adds to it too). */
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> LookComps;
private:
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> TintMats;
	/** Vharra's trunk segments and disc (world-placed each frame along the curve from her hips to the anchor). */
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> Trunk;
	TArray<FVector> TrunkPts;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> LoopFx;
	FName LoopFxName;
	FVector BaseMeshScale = FVector::OneVector;
	FRotator BaseMeshRot = FRotator::ZeroRotator;
	float Clock = 0.f, FxClock = 0.f, Swell = 0.f, JawAlpha = 0.f, CloakAlpha = 0.f, LashNow = 0.f, RearAlpha = 0.f;
	int32 LookTries = 0;
	uint8 BuiltBurst = 0;
	bool bStandInHidden = false;
	// --- Mother Maw, the Hex-Warden, the Bloat (WRSwampBosses.cpp) ---
	/** The skeletal mesh's own place under the capsule after the fit (the rear lifts and pitches it from here). */
	FVector BaseMeshLoc = FVector::ZeroVector;
	bool bBaseSaved = false;
	/** The Bloat's inner glow (a real light, garnet, by its gorge) and the Warden's cast light in her palm. */
	UPROPERTY(Transient) TObjectPtr<class UPointLightComponent> GlowLight;
	/** Eased looks: the Warden's mask eyes (casting), the Bloat's burst glow, the gorge shown. */
	float CastAlpha = 0.f, GlowAlpha = 0.f, GorgeShown = -1.f, ScatterAlpha = 0.f;
	bool bWasScattered = false;
	bool bMaskShown = true;
};

/**
 * A piece of a swamp arena: Mother Maw's boardwalk sections (walk: whole, broken or freshly mended; her slams break
 * them, 2 wood and 3 s mends one) and her nest's fire (nest: lit when set on fire); the Ossuary's totem poles (totem:
 * crow, bone, blood, voice; standing or broken); the bones heaped on the Throat's ossuary mounds (mound); a bleedwater
 * slick or clot puddle (puddle: it hurts while it lasts) and the clotted crust over the blood flood (crust: walkable for
 * its seconds).
 */
UCLASS()
class WILDREACH_API AWRSwampBossProp : public AActor
{
	GENERATED_BODY()
	friend struct FWRSwampBoss;

public:
	AWRSwampBossProp();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** walk, nest, totem, mound, puddle, crust. */
	UPROPERTY(ReplicatedUsing=OnRep_Look) FName Kind;
	/** walk: 0 whole, 1 broken, 2 mended (fresh wood). nest: 0, 1 burning. totem: 0 standing, 1 broken. */
	UPROPERTY(ReplicatedUsing=OnRep_Look) uint8 State = 0;
	/** walk: length (cm, along X); nest / puddle / crust / mound: radius; totem: pole height. */
	UPROPERTY(ReplicatedUsing=OnRep_Look) float Size = 600.f;
	/** totem: 0 crow, 1 bone, 2 blood, 3 voice. walk: the side (0..3). puddle: 0 bleedwater, 1 clot. */
	UPROPERTY(ReplicatedUsing=OnRep_Look) uint8 Variant = 0;
	/** walk: how high the deck stands over the ground under its middle (cm), for the posts. */
	UPROPERTY(ReplicatedUsing=OnRep_Look) float Rise = 100.f;

	/** Server. */
	void SetState(uint8 NewState);
	/** Server: a puddle that hurts (bleed buildup a second, slow) for Seconds, then goes. */
	void MakeHazard(float InBleedPerSecond, bool bSlow, float Seconds, AActor* Source);
	/** walk: a survivor mending it (server). */
	TWeakObjectPtr<AWRCharacterBase> Mender;
	double MendStart = 0.0;
	FVector MendFrom = FVector::ZeroVector;
	/** totem: the hit body standing in it. */
	TWeakObjectPtr<AWRCreature> TotemBody;
	/** nest: burning until (server). */
	double BurnUntil = 0.0;
	/** walk: is this point over the deck (on the planks, give or take a hand). */
	bool OnDeck(const FVector& Feet) const;

private:
	UFUNCTION() void OnRep_Look();
	void Build();
	void SetCollision();
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> LookComps;
	UPROPERTY(Transient) TArray<TObjectPtr<UBoxComponent>> Boxes;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> LoopFx;
	/** The nest's fires and the totems' garnet eye-lights, the puddle's ripples (each stopped on a rebuild). */
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> FxComps;
	UPROPERTY(Transient) TObjectPtr<class UPointLightComponent> PropLight;
	FName BuiltKind;
	uint8 BuiltState = 255;
	uint8 BuiltVariant = 255;
	float BuiltSize = -1.f;
	float BleedPerSecond = 0.f;
	bool bSlows = false;
	float HazardClock = 0.f;
	float FxClock = 0.f;
	TWeakObjectPtr<AActor> HazardSource;
};

/** One of the six Rodwarden copper rods round the Mother's Throat, on its stone plinth. 60 lightning damage on it within
 *  2 s (or a strike, or a stormglass jar near it) electrifies the arena's water for 1.5 s; then 20 s to re-arm, glowing
 *  blue. Rods re-arm on a reset. */
UCLASS()
class WILDREACH_API AWRSwampRod : public AActor
{
	GENERATED_BODY()

public:
	AWRSwampRod();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

	/** 0 armed, 1 cooling (blue). */
	UPROPERTY(ReplicatedUsing=OnRep_Look) uint8 State = 0;
	UPROPERTY(Replicated) float CoolEndsAt = 0.f;
	/** The rod's height over its plinth's foot (cm). */
	UPROPERTY(ReplicatedUsing=OnRep_Look) float Height = 650.f;

	FVector Tip() const;
	/** Server: lightning landed on it (a spell, a strike): counts toward the 2 s threshold; true when it fired. */
	bool TakeLightning(float Amount, AActor* By, bool bForce);
	/** Server: back to armed (a reset). */
	void Rearm();
	/** Every machine: the arcs racing from its foot across the water to these points. */
	UFUNCTION(NetMulticast, Unreliable) void MulticastArcs(const TArray<FVector_NetQuantize>& To);

private:
	UFUNCTION() void OnRep_Look();
	void Build();
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> LookComps;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> GlowRoot;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> CoolFx;
	UPROPERTY(VisibleAnywhere) TObjectPtr<class UCapsuleComponent> Shaft;
	TArray<TPair<double, float>> Taken;
	TArray<FVector> ArcTo;
	double ArcUntil = 0.0;
	float ArcClock = 0.f;
	int32 RodHandle = INDEX_NONE;
	uint8 BuiltState = 255;
};

/**
 * The Mother's Throat's own water: the flood that rises 1.2 m over 6 s at the Sinking and drains on a reset. Every
 * machine works its surface out from the replicated start time. Its swimmable part is the central pool (a water volume
 * that rises with the surface); over the terraces you wade. It draws the risen surface over the whole flooded floor:
 * murk, bleedwater on the Red Tide, blood in the hidden phase (with the shock's arcs racing across it).
 */
UCLASS(NotPlaceable)
class WILDREACH_API AWRSwampFlood : public APhysicsVolume
{
	GENERATED_BODY()

public:
	AWRSwampFlood(const FObjectInitializer& ObjectInitializer);
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** The resting surface (world Z, cm), the pool's floor, and the flood's rise (cm). */
	UPROPERTY(ReplicatedUsing=OnRep_Shape) float BaseZ = 0.f;
	UPROPERTY(ReplicatedUsing=OnRep_Shape) float FloorZ = -800.f;
	UPROPERTY(ReplicatedUsing=OnRep_Shape) float PoolRadius = 2000.f;
	UPROPERTY(ReplicatedUsing=OnRep_Shape) float FloodRadius = 4500.f;
	UPROPERTY(Replicated) float Rise = 120.f;
	/** Server time the flood began rising (0: down), and when it began draining (0: not draining). */
	UPROPERTY(Replicated) float RiseStartedAt = 0.f;
	UPROPERTY(Replicated) float DrainStartedAt = 0.f;
	UPROPERTY(Replicated) float RiseSeconds = 6.f;
	/** 0 murk, 1 bleedwater (Red Tide), 2 blood (the hidden phase). */
	UPROPERTY(ReplicatedUsing=OnRep_Look) uint8 WaterKind = 0;
	/** Electrified until (server time): the arcs show. */
	UPROPERTY(Replicated) float ShockUntil = 0.f;

	/** The surface now (every machine). */
	float SurfaceZ() const;
	bool IsFlooded() const { return RiseStartedAt > 0.f; }
	/** Feet in this arena's water (wading or swimming): inside its floor and under its surface. */
	bool InWater(const FVector& Feet) const;
	/** Feet in the risen part only (above the resting surface). */
	bool InRisen(const FVector& Feet) const;
	/** Server. */
	void StartFlood();
	void Drain();
	void SetKind(uint8 Kind);
	void Electrify(float Seconds);

private:
	UFUNCTION() void OnRep_Shape();
	UFUNCTION() void OnRep_Look();
	void BuildBody();
	void BuildSurface();
	void PlaceSurface();
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> SurfaceComps;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> SurfaceRoot;
	float ArcClock = 0.f;
	float RippleClock = 0.f;
	float BuiltRadius = -1.f;
	float PlacedZ = -1.f;
	uint8 BuiltKind = 255;
	bool bBodyOn = false;
};

/** A thrown thing of a swamp boss that every machine flies the same way from its launch (a clot lobbed on a high arc, the
 *  hex bolt straight and level): never homing, damage where it really meets someone or comes down. */
UCLASS()
class WILDREACH_API AWRSwampShot : public AActor
{
	GENERATED_BODY()

public:
	AWRSwampShot();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** clot, hex_bolt. */
	UPROPERTY(Replicated) FName Kind;
	UPROPERTY(Replicated) FVector_NetQuantize Start = FVector::ZeroVector;
	UPROPERTY(Replicated) FVector_NetQuantize10 Velocity = FVector::ZeroVector;
	UPROPERTY(Replicated) float Gravity = 0.f;
	UPROPERTY(Replicated) float LaunchAt = 0.f;
	/** Server: its blow, and the range it gives up at. */
	FWRHit Hit;
	float MaxRange = 3000.f;
	float Radius = 60.f;
	TFunction<void(const FVector&)> OnLanded;

private:
	FVector At(float T) const;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Fx;
	FVector Last = FVector::ZeroVector;
	bool bDone = false;
};

/** The hooks AWRBoss calls for the swamp's bosses (friend of AWRBoss). Same shape as FWRStormBoss. */
struct WILDREACH_API FWRSwampBoss
{
	// The square's sections run their own mends (TickMends) from their tick.
	friend class AWRSwampBossProp;

	static bool IsSwampBoss(const AWRBoss* Boss);
	static void Begin(AWRBoss* Boss);
	static void Reset(AWRBoss* Boss);
	static void EndPlay(AWRBoss* Boss);
	/** Not fighting: the Bloat's loop, sleep and herds, the Hex-Warden's night walk, Vharra on her anchor. True: skip its
	 *  roam (bSkipWake: and its wake check). Leaves bSkipWake alone for a boss that isn't the swamp's. */
	static bool TickIdle(AWRBoss* Boss, float DeltaSeconds, bool& bSkipWake);
	/** Fighting, before its attack machine: true when this ran the tick (a convulse, a scatter, Maw at her nest). */
	static bool Tick(AWRBoss* Boss, float DeltaSeconds);
	static AWRCharacterBase* PickTarget(AWRBoss* Boss, AWRCharacterBase* Nearest);
	static bool Allowed(const AWRBoss* Boss, const FWRBossAttack& Attack, const AWRCharacterBase* Target);
	static void AttackStarted(AWRBoss* Boss, FWRBossAttack& Attack, AWRCharacterBase* Target);
	static bool TickWindup(AWRBoss* Boss, FWRBossAttack& Attack, float DeltaSeconds);
	static bool BeginStrike(AWRBoss* Boss, FWRBossAttack& Attack);
	static bool TickStrike(AWRBoss* Boss, FWRBossAttack& Attack, float DeltaSeconds);
	static void EndStrike(AWRBoss* Boss, const FWRBossAttack& Attack);
	static void ModifyHit(const AWRBoss* Boss, const FWRBossAttack& Attack, FWRHit& Hit);
	static void HitLanded(AWRBoss* Boss, AWRCharacterBase* Victim, FName AttackId, EWRHitResult Result);
	static float Incoming(AWRBoss* Boss, const FWRHit& Hit, float Damage);
	/** Weak to Lightning (x1.5), not to the opposite of fire / frost. */
	static bool OwnWeakness(const AWRBoss* Boss);
	static void Damaged(AWRBoss* Boss, const FWRHit& Hit, float Damage);
	static void PartBroken(AWRBoss* Boss, const FWRCreatureDef::FBodyPoint& Part, AActor* By);
	static void PhaseChanged(AWRBoss* Boss, int32 NewPhase);
	static bool PlaceRunback(AWRBoss* Boss);
	static FName BossPart(const AWRBoss* Boss, bool bScarred, FName Fallback);
	static void Died(AWRBoss* Boss);
	/** WRCombatLibrary ApplyHit's stance hook: Vharra convulsing x2. */
	static float StanceTaken(AWRCharacterBase* Target, const FWRHit& Hit, float Stance);
	/** WRStatusComponent's threshold hook: bleed builds on Vharra at x0.5 (her threshold x2). */
	static float BuildupThresholdScale(const AWRCharacterBase* Who, int32 StatusId);
	/** AWRCreature::ModifyIncomingDamage's hook for the hex totems: x2 from fire (blunt is their physResist). */
	static float CreatureIncoming(const AWRCreature* Creature, const FWRHit& Hit, float Damage);
	/** Wading in the flooded Throat: run x0.75 (both machines read the replicated flood). */
	static float WadeScale(const AWRCharacterBase* Who);

	// --- For the other swamp systems ---
	/** Lightning landed here (a spell's impact, a strike, a stormglass jar, a Storm invader's strike): a rod within reach
	 *  takes it; water Vharra's in carries it. Server. bForce: shocks at once (a strike, a jar). */
	static void LightningAt(UWorld* World, const FVector& At, float Amount, AActor* By, bool bForce = false);
	/** Fire landed here (a fire pot, a fire spell, lit marsh gas): Mother Maw's nest catches. Server. */
	static void FireAt(UWorld* World, const FVector& At, AActor* By);
	/** Salt landed on this body (WRSwamp::OnSalted): the Bloat vomits. Server. */
	static void Salted(AWRCharacterBase* Target, AActor* By);
	/** A hexed beast (the Hex-Warden's night walk): its eyes glow garnet, its hits carry curse 25, its aggro is x1.3. */
	static bool IsHexed(const AActor* Creature);
	static float HexAggroScale(const AActor* Creature);
	static float HexCurse(const AActor* Creature);
	/** The Bloat in this world (null: not out). */
	static AWRBoss* Bloat(const UWorld* World);
	/** Blood invades Storm peaks (cook 2.21): the Bloat leaves its route and walks to Target (through Stormgut); the
	 *  invasion's pack follows it. EndInvasion sends it home. Server. */
	static bool LeadInvasion(UWorld* World, const FVector& Target);
	static void EndInvasion(UWorld* World);
	/** Where the Weeping Gate's one-way door from the Sunken Coven arrives: the Throat's bottom terrace. */
	static bool WeepingGatePoint(FVector& Out, FRotator& Facing);
	/** The interact prompt near a broken section of Mother Maw's square (client). */
	static bool Prompt(const AWRPlayerController* PC, FString& OutTarget, FString& OutVerb);
	/** Interact near a broken boardwalk section of Mother Maw's square (client): sends the mend. */
	static bool ClientInteract(AWRPlayerController* PC);
	/** Server: "world swamp boss mend" (WRSwamp::Command hands it on as {"swampboss", "mend"}). */
	static FString Command(AWRPlayerController* PC, const TArray<FString>& Args);
	/** WRStorm's strike delegate: strikes on the Throat's rods or into its water. */
	static void StrikeLanded(UWorld* World, const FWRStrikeCall& Call);

	// --- Vharra (WRSwampVharra.cpp) ---
	// Vharra, Queen of the Mire, and her brood (cook 2.14): the public hooks above forward here for her. Same arguments
	// and returns as the hooks they stand in for.
	static bool IsVharra(const AActor* Actor);
	static void VharraBegin(AWRBoss* Boss);
	static void VharraReset(AWRBoss* Boss);
	static void VharraEndPlay(AWRBoss* Boss);
	static bool VharraTickIdle(AWRBoss* Boss, float DeltaSeconds, bool& bSkipWake);
	static bool VharraTick(AWRBoss* Boss, float DeltaSeconds);
	static AWRCharacterBase* VharraPickTarget(AWRBoss* Boss, AWRCharacterBase* Nearest);
	static bool VharraAllowed(const AWRBoss* Boss, const FWRBossAttack& Attack, const AWRCharacterBase* Target);
	static void VharraAttackStarted(AWRBoss* Boss, FWRBossAttack& Attack, AWRCharacterBase* Target);
	static bool VharraTickWindup(AWRBoss* Boss, FWRBossAttack& Attack, float DeltaSeconds);
	static bool VharraBeginStrike(AWRBoss* Boss, FWRBossAttack& Attack);
	static bool VharraTickStrike(AWRBoss* Boss, FWRBossAttack& Attack, float DeltaSeconds);
	static void VharraEndStrike(AWRBoss* Boss, const FWRBossAttack& Attack);
	static void VharraModifyHit(const AWRBoss* Boss, const FWRBossAttack& Attack, FWRHit& Hit);
	static void VharraHitLanded(AWRBoss* Boss, AWRCharacterBase* Victim, FName AttackId, EWRHitResult Result);
	static float VharraIncoming(AWRBoss* Boss, const FWRHit& Hit, float Damage);
	static bool VharraOwnWeakness(const AWRBoss* Boss);
	static void VharraDamaged(AWRBoss* Boss, const FWRHit& Hit, float Damage);
	static void VharraPartBroken(AWRBoss* Boss, const FWRCreatureDef::FBodyPoint& Part, AActor* By);
	static void VharraPhaseChanged(AWRBoss* Boss, int32 NewPhase);
	static bool VharraPlaceRunback(AWRBoss* Boss);
	static FName VharraBossPart(const AWRBoss* Boss, bool bScarred, FName Fallback);
	static void VharraDied(AWRBoss* Boss);
	static float VharraStanceTaken(AWRCharacterBase* Target, const FWRHit& Hit, float Stance);
	static float VharraBuildupThresholdScale(const AWRCharacterBase* Who, int32 StatusId);
	static bool VharraPrompt(const AWRPlayerController* PC, FString& OutTarget, FString& OutVerb);
	static bool VharraClientInteract(AWRPlayerController* PC);
	/** Server: "swampboss vharra <rods|shock|flood|drain|blood|phase N>" (an empty string: not a Vharra command). */
	static FString VharraCommand(AWRPlayerController* PC, const TArray<FString>& Args);
	/** Every machine that draws: her palette, hood plates, eyes, sacs, the 14 trunk rings and the anchor disc. */
	static void VharraBuildLook(UWRSwampBossBody* Body);
	static void VharraTickLook(UWRSwampBossBody* Body, float DeltaSeconds);
	/** Server: the Throat (her spawner, the six rods, the flood, the mounds' bones, the run-back stone, the Blood altar).
	 *  True once placed. */
	static bool VharraPlaceArena(UWRSwampArenas* Arenas, const TSharedPtr<class FJsonObject>& Row);
	/** WRSwamp::OnWaterShocked (a stormglass jar): the Throat's water carries it. */
	static void VharraWaterShocked(UWorld* World, const FVector& Center, float Radius, float Seconds, AActor* Instigator);

private:
	static void EnsureArena(AWRBoss* Boss, FWRSwampBossState& St);
	static bool TickMaw(AWRBoss* Boss, FWRSwampBossState& St, UWRSwampBossBody* Body, float DeltaSeconds);
	static bool TickWarden(AWRBoss* Boss, FWRSwampBossState& St, UWRSwampBossBody* Body, float DeltaSeconds);
	static bool TickBloat(AWRBoss* Boss, FWRSwampBossState& St, UWRSwampBossBody* Body, float DeltaSeconds);
	static bool TickVharra(AWRBoss* Boss, FWRSwampBossState& St, UWRSwampBossBody* Body, float DeltaSeconds);
	static bool IdleBloat(AWRBoss* Boss, FWRSwampBossState& St, UWRSwampBossBody* Body, float DeltaSeconds, bool& bSkipWake);
	static bool IdleWarden(AWRBoss* Boss, FWRSwampBossState& St, UWRSwampBossBody* Body, float DeltaSeconds);
	static void TickGrab(AWRBoss* Boss, FWRSwampBossState& St, float DeltaSeconds);
	static void ReleaseGrab(AWRBoss* Boss, FWRSwampBossState& St);
	static void StartGrab(AWRBoss* Boss, FWRSwampBossState& St, AWRCharacterBase* Victim, FName Kind, float Seconds, float Dps);
	static void TickWave(AWRBoss* Boss, FWRSwampBossState& St);
	static void StartWave(AWRBoss* Boss, FWRSwampBossState& St, const FVector& From, float Radius, float Damage, float Height, bool bLow, float Knock);
	static void Shock(AWRBoss* Vharra, class AWRSwampRod* From, const FVector& At);
	static bool BreakSectionAt(AWRBoss* Boss, const FVector& At);
	static void SpawnYoung(AWRBoss* Boss, FName CreatureId, int32 Count, float Health, const FVector& At);
	static void Vomit(AWRBoss* Boss, FWRSwampBossState& St, float Share);
	static void AddGorge(AWRBoss* Boss, FWRSwampBossState& St, float Amount);
	static void Scatter(AWRBoss* Boss, FWRSwampBossState& St);
	static void Reform(AWRBoss* Boss, FWRSwampBossState& St);
	static void TotemBroken(AWRBoss* Boss, FWRSwampBossState& St, int32 Index);
	static void SpawnTotems(AWRBoss* Boss, FWRSwampBossState& St);
	static void EndHex(UWorld* World);
	static bool StartRow(AWRBoss* Boss, FName Id, AWRCharacterBase* Target);
	static void FireShot(AWRBoss* Boss, FName Kind, const FVector& From, const FVector& To, const FWRHit& Hit, float Speed, bool bLob);
	static AWRSwampBossProp* Puddle(UWorld* World, FName Kind, const FVector& At, float Radius, float Seconds, float BleedPerSecond, bool bSlow, AActor* Source);
	static float WaterZ(const AWRBoss* Boss);
	// --- Mother Maw, the Hex-Warden, the Bloat (WRSwampBosses.cpp) ---
	/** Mother Maw: in open water (the pool, not on a deck nor the nest mound) and her target on a whole section. */
	static bool MawInWater(const AWRBoss* Boss, const FWRSwampBossState& St);
	/** The mends running on her square's sections (server, each fight tick and idle). */
	static void TickMends(UWorld* World);
	/** The Hex-Warden's blood totem: a bleed proc near her heals her. */
	static void TickBloodTotem(AWRBoss* Boss, FWRSwampBossState& St);
	/** The mimic call's pulled tames: held at her side, then given back. */
	static void TickMimic(AWRBoss* Boss, FWRSwampBossState& St, bool bEnd);
	/** The Bloat: its loop (or the causeway, or an invasion's way) as world points. */
	static void BuildRoute(AWRBoss* Boss, FWRSwampBossState& St);
	/** Ends the current move at once and stands it open for Seconds (a scatter's reform, a burst's collapse). */
	static void CutMove(AWRBoss* Boss, float Seconds);
	/** The boss's own move by id (null: not its row). */
	static FWRBossAttack* FindMove(AWRBoss* Boss, FName Id);
};

/** Server: places the swamp's bosses (their spawners), arena props, rods, the Throat's flood and the Blood altar from
 *  Data/swamp.json "arenas", each once its ground has streamed in near a survivor. A world without swamp.json: nothing.
 *  It also listens for strikes, shocked water and salt for the bosses. */
UCLASS()
class WILDREACH_API UWRSwampArenas : public UWorldSubsystem
{
	GENERATED_BODY()
	friend struct FWRSwampBoss;

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

private:
	void Step();
	bool Place(FName Arena, const TSharedPtr<class FJsonObject>& Row);
	void OnWaterShocked(UWorld* World, const FVector& Center, float Radius, float Seconds, AActor* Instigator);
	void OnSalted(AWRCharacterBase* Target, AActor* Instigator, const FVector& At);
	TSet<FName> Placed;
	FTimerHandle StepTimer;
	FDelegateHandle StrikeHandle;
	FDelegateHandle ShockHandle;
	FDelegateHandle SaltHandle;
};
