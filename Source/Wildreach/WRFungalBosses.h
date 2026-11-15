#pragma once

// The Fungal caverns' bosses and their arenas (cooks/biome-fungal.md 2.13, 2.14; cooks/build-biome-fungal.md 2.13-2.14):
// the Sporemother in the Brood Doline (six wall sacs that hatch Mycomites until they burn), the Rot King in the Steeping
// Vats (rot pools he heals in; frost freezes them, wind dries his crown), the Weaver on her loop of the Webs (she webs
// where she settles: fast on web, slow off it) and Mycelorn in the Heart chamber (rooted, then travelling the veins and
// surfacing in fairy rings, then the whole cavern; its own clouds pushed onto it by wind choke it; the Draught gates).
//
// Their moves are rows in Data/attacks.json like every boss's; this file holds:
//  - AWRFungalBossProp: the arena things with state, drawn on every machine from replicated numbers (a wall egg sac, a
//    rot pool, a Draught gate's fungal plug, a mound pod, a web sheet).
//  - UWRFungalBossBody: each boss's code-built parts over its rig (Mycelorn's whole body) and the state it shows.
//  - FWRFungalBoss: the hooks AWRBoss calls for these four, beside the Crystal caves'.
//  - UWRFungalArenas: places the bosses, their things, the run-back stones, the summoning stones and the Spore altar.
// Numbers are tuning bosses.<id>.*. Every blow goes through UWRCombatLibrary::ApplyHit on the server.

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRCombatTypes.h"
#include "WRFungalBosses.generated.h"

class AWRBoss;
class AWRCreature;
class AWRCharacterBase;
class AWRSporeCloud;
class USphereComponent;
class UProceduralMeshComponent;
class UPointLightComponent;
struct FWRBossAttack;
struct FWRFungalBossState;

/**
 * A Fungal arena thing (server-owned, replicated).
 *  sac    a Brood Doline wall egg sac (a puffball-like pod 1.5 m across, glowing cyan): State 1 burnt.
 *  pool   a rot pool (a full Steeping vat, or one the Rot King spills): State 2 frozen (walkable ice, no heal), 1 gone.
 *  plug   a Draught gate's fungal plug (6 m wide): State 1 broken (the draught blows), regrows.
 *  pod    a mound pod (Mycelorn's breathing pore, phase 2): State 1 burnt.
 *  web    a web sheet the Weaver spun: State 1 burnt or torn (gone).
 */
UCLASS(NotPlaceable)
class WILDREACH_API AWRFungalBossProp : public AActor
{
	GENERATED_BODY()

public:
	AWRFungalBossProp();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	UPROPERTY(ReplicatedUsing=OnRep_Look) FName Kind;
	UPROPERTY(ReplicatedUsing=OnRep_Look) uint8 State = 0;
	UPROPERTY(ReplicatedUsing=OnRep_Look) float Radius = 300.f;
	/** When State last changed (server clock): the burn, the break, the freeze. */
	UPROPERTY(Replicated) float StateAt = -100.f;
	UPROPERTY(Replicated) int32 Index = 0;

	// Server.
	TWeakObjectPtr<AWRBoss> OwnerBoss;
	float Health = 0.f;
	double NextAt = 0.0, Until = 0.0;
	bool bPermanent = false;
	/** Server: a blow or a shot reached it (bFire: a fire blow, a torch, a burning shot). True when it took it. */
	bool TakeHit(AActor* By, float Damage, bool bFire, bool bBlunt);
	void SetState(uint8 NewState);
	float ServerNow() const;
	/** A pool's floor contains P (2D within its radius, near its level). */
	bool Contains(const FVector& P, float Grow = 0.f) const;
	bool IsLive() const { return State == 0; }
	static TArray<TWeakObjectPtr<AWRFungalBossProp>>& All();

private:
	UFUNCTION() void OnRep_Look();
	void Build();
	UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> HitShape;
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Mesh;
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Glow;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> Lamp;
	FName BuiltKind;
	uint8 BuiltState = 255;
	float BuiltRadius = -1.f;
	float Clock = 0.f;
};

namespace WRFungalBossFlag
{
	enum : uint8
	{
		Dry     = 1 << 0,   // the Rot King's crown dried by wind (no drip, no pools)
		Choked  = 1 << 1,   // Mycelorn choking on its own cloud (the gills clench and go dark)
		NoGills = 1 << 2,   // Mycelorn's gills broken (dark 30 s)
		Under   = 1 << 3,   // Mycelorn under the mat (phase 2's travel): its body is hidden
		Split   = 1 << 4,   // Mycelorn's stem split open on the Heart-seed (phase 3)
		Drained = 1 << 5,   // the Heart pool drained (phase 2 on)
		Asleep  = 1 << 6,   // the Weaver hanging asleep in the False Sky
		Rearing = 1 << 7,   // a cap reared back, a crown tilted: the big tells
	};
}

/** Each Fungal boss's look over its rig (Mycelorn's whole body in code) and the state every machine draws from. */
UCLASS(ClassGroup=(Creature))
class WILDREACH_API UWRFungalBossBody : public UActorComponent
{
	GENERATED_BODY()

public:
	UWRFungalBossBody();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	static UWRFungalBossBody* Get(const AActor* Actor);
	static void Attach(AWRCreature* Boss);

	UPROPERTY(Replicated) uint8 Flags = 0;
	/** A pose: the Sporemother 1 rearing (press); the Weaver 1 climbing out of sight; Mycelorn 1 a club lifted, 2 the
	 *  curtain flared (spin, lash), 3 the fist (engulf), 4 the gills breathing. */
	UPROPERTY(Replicated) uint8 Pose = 0;
	bool Has(uint8 F) const { return (Flags & F) != 0; }
	void Set(uint8 F, bool bOn) { const uint8 W = Flags; Flags = bOn ? (Flags | F) : (Flags & ~F); if (W != Flags && GetOwner()) GetOwner()->ForceNetUpdate(); }

private:
	void BuildLook();
	void ApplyTint();
	void TickLook(float Dt);
	USceneComponent* AddPart(FName Bone, const FVector& Local, struct FWRToolMesh& M, bool bGlow, int32 Tag);
	FName BoneLike(const TCHAR* Hint) const;
	struct FPart { TWeakObjectPtr<USceneComponent> Comp; int32 Tag = 0; FVector BaseLoc = FVector::ZeroVector; FRotator BaseRot = FRotator::ZeroRotator; FVector BaseScale = FVector::OneVector; };
	TArray<FPart> Parts;
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> LookComps;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> Lamp;
	struct FLeg { TWeakObjectPtr<UProceduralMeshComponent> Upper, Lower; float Angle = 0.f; float Phase = 0.f; };
	TArray<FLeg> Legs;
	bool bBuilt = false;
	int32 Tries = 0;
	float Clock = 0.f, Stride = 0.f, DripClock = 0.f, Flare = 0.f, Lift = 0.f, Hidden = -1.f;
};

/** The hooks AWRBoss calls for the Fungal caverns' bosses (friend of AWRBoss), beside the Crystal caves'. */
struct WILDREACH_API FWRFungalBoss
{
	static bool IsFungalBoss(const AWRBoss* Boss);
	static void Begin(AWRBoss* Boss);
	static void Reset(AWRBoss* Boss);
	static void EndPlay(AWRBoss* Boss);
	/** Not fighting: the Weaver's loop and her sleep, the sacs' quiet. True: skip its own roam. */
	static bool TickIdle(AWRBoss* Boss, float DeltaSeconds, bool& bSkipWake);
	/** Fighting, before its attack machine: true when this ran the tick (travelling under the mat, a hold...). */
	static bool Tick(AWRBoss* Boss, float DeltaSeconds);
	static bool Allowed(const AWRBoss* Boss, const FWRBossAttack& Attack, const AWRCharacterBase* Target);
	static void AttackStarted(AWRBoss* Boss, FWRBossAttack& Attack, AWRCharacterBase* Target);
	static bool BeginStrike(AWRBoss* Boss, FWRBossAttack& Attack);
	static bool TickStrike(AWRBoss* Boss, FWRBossAttack& Attack, float DeltaSeconds);
	static void EndStrike(AWRBoss* Boss, const FWRBossAttack& Attack);
	static void ModifyHit(const AWRBoss* Boss, const FWRBossAttack& Attack, FWRHit& Hit);
	static void HitLanded(AWRBoss* Boss, AWRCharacterBase* Victim, FName AttackId, EWRHitResult Result);
	static float Incoming(AWRBoss* Boss, const FWRHit& Hit, float Damage);
	static bool OwnWeakness(const AWRBoss* Boss);
	static void Damaged(AWRBoss* Boss, const FWRHit& Hit, float Damage);
	static void PhaseChanged(AWRBoss* Boss, int32 NewPhase);
	static bool PlaceRunback(AWRBoss* Boss);
	static FName BossPart(const AWRBoss* Boss, bool bScarred, FName Fallback);
	static void Died(AWRBoss* Boss);
	/** A weapon swing or a projectile met an actor: true if it was a Fungal arena thing that took the blow. */
	static bool HitThing(AActor* Thing, UPrimitiveComponent* Comp, AWRCharacterBase* By, float Damage, bool bFire);
	/** Server, from WRFungal::FireAt / FrostAt / Wind: fire burns sacs, pods, plugs (x2), webs, frees a cocoon; frost
	 *  freezes a rot pool; wind tears webs. */
	static void FireAt(AActor* By, const FVector& At, float Radius);
	static void FrostAt(AActor* By, const FVector& At);
	static void Wind(AActor* By, const FVector& Origin, const FVector& Dir, float Range, float Cone, bool bStrong);
	/** Server, from AWRSporeCloud::Flash: a cloud flashed (the Sporemother's vent on her: 200 and a stagger; webs in it burn). */
	static void CloudFlashed(AWRSporeCloud* Cloud, const TArray<AWRCharacterBase*>& Inside);
	/** Movement x for the Weaver (on web x1.6, off x0.6), every machine. */
	static float MoveScale(const AActor* Who);
	/** Server: a survivor held by a Fungal boss (the press, the engulf, the cocoon), or null. */
	static AWRBoss* HolderOf(const AActor* Victim);
	/** Where a boss's arena is (the hall, its radius and yaw). */
	static bool ArenaOf(FName BossId, FVector& Centre, float& Radius, float& Yaw);

private:
	/** Mycelorn's fight: the choke, the Draught gates, the travel under the mat, the pods, the Whole Cavern, the summited. */
	static bool TickMycelorn(AWRBoss* Boss, FWRFungalBossState& St, float Dt);
};

/** Places the Fungal caverns' bosses and their things (server), once the caverns are built near a survivor. */
UCLASS()
class WILDREACH_API UWRFungalArenas : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UWRFungalArenas, STATGROUP_Tickables); }
	virtual bool IsTickable() const override;

private:
	bool Place(FName BossId);
	TSet<FName> Placed;
	float PlaceClock = 0.f;
};
