#pragma once

// The Sky islands' bosses and their arenas (cooks/biome-sky.md 2.10, 2.11; cooks/build-biome-sky.md 2.12, 2.13;
// cooks/combat.md 2.17):
//  - Kyrra, the Tempest Harrier, roaming her 2.4 km loop over the Ruined Sky-bridges, landing every 6 min on the widest
//    island on her way; arrows to the wings ground her; her snatch carries you 20 m up and drops you.
//  - The Bridge Sentinel on the standing bridge from Tollgate to Span North: lure its Vane slam onto one of the five weak
//    sections and the deck gives way under it (it falls 35 m to Winch Isle, a quarter of its health gone, kneeling 10 s);
//    its deck throws leave holes; the copper taken from the Toll House's bowls wakes it.
//  - Old Gulper over Gulper's Bight: the Inhale pulls you in and swallows you; fire in its mouth while it inhales chokes
//    it down onto the lagoon, mouth open; two chokes in a minute crack its hump.
//  - Aeolith, the Sky Serpent, at the Serpent's Ring over the Flue: eight white platforms with wind-stones, the Flue's
//    spore plumes (its counter: spores weigh its wings), the Gale Crown, the Unbinding (the platforms drift apart, columns
//    open in the gaps), the hidden Last Circle (ride its back, break its three crest vents, brace through its rolls).
// Their moves are rows in Data/attacks.json; numbers in tuning bosses.<id>.*.

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRCombatTypes.h"
#include "WRSkyBosses.generated.h"

class AWRBoss;
class AWRCharacterBase;
class AWRPlayerController;
class UBoxComponent;
class UPointLightComponent;
struct FWRBossAttack;

/**
 * The Serpent's Ring (L1): eight round white platforms 14-18 m across, 28 m out round the Flue's axis at 1,250 m, each
 * with a low wind-stone (1.5 x 4 m) on its outer rim and a brazier; chains between them while it's bound. After the
 * Unbinding they drift: each orbits on its own, the gaps opening and closing between 4 and 14 m on a 20 s cycle while the
 * ring slowly turns, and an updraft column stands in every gap. Where everything is, is a pure function of the server time
 * and the replicated UnboundAt (every machine places the same, nothing snaps; standing on one carries you). The Flue's
 * three spore vents breathe a plume up the axis every 40 s (30 in the Sporefall): the same function of the time.
 */
UCLASS(NotPlaceable)
class WILDREACH_API AWRSkyRing : public AActor
{
	GENERATED_BODY()

public:
	AWRSkyRing();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Server time of the Unbinding (0: bound). */
	UPROPERTY(Replicated) float UnboundAt = 0.f;
	/** The platform i's centre (world, its floor's top) at Now. */
	FVector PlatformAt(int32 i, float Now) const;
	float PlatformRadius(int32 i) const;
	int32 Count() const;
	/** The plume: which vent breathes (0-2) and its phase (< 0 the 1 s tell, 0..6 live); -1 none. */
	int32 PlumeNow(float Now, float& OutPhase) const;
	/** Is a wind-stone between From and To (the gale breath, the quills)? */
	bool StoneBetween(const FVector& From, const FVector& To, float Now) const;
	/** The platform nearest P (its index) and its centre. */
	int32 NearestPlatform(const FVector& P, float Now, FVector& OutCentre) const;
	FVector Centre() const { return GetActorLocation(); }
	float ServerNow() const;

private:
	void Build();
	void Place(float Now);
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> Roots;
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> Chains;
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> Columns;
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> Plumes;
	UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> Lights;
	float Clock = 0.f;
	float ColumnClock = 0.f;
	bool bBuilt = false;
	bool bChainsShown = true;
	int32 PlumeShown = -1;
};

/** The hooks AWRBoss calls for the Sky's four (friend of AWRBoss). */
struct WILDREACH_API FWRSkyBoss
{
	static bool IsSkyBoss(const AWRBoss* Boss);
	static void Begin(AWRBoss* Boss);
	static void Reset(AWRBoss* Boss);
	static void EndPlay(AWRBoss* Boss);
	/** Not fighting: Kyrra's loop and landings, the Sentinel kneeling on its bridge till woken, Gulper floating, Aeolith
	 *  circling the Flue. True: skip its own roam. */
	static bool TickIdle(AWRBoss* Boss, float DeltaSeconds, bool& bSkipWake);
	/** Fighting, before its attack machine: true when this ran the tick (a carry, a fall, a choke, a pass...). */
	static bool Tick(AWRBoss* Boss, float DeltaSeconds);
	static bool Allowed(const AWRBoss* Boss, const FWRBossAttack& Attack, const AWRCharacterBase* Target);
	static void AttackStarted(AWRBoss* Boss, FWRBossAttack& Attack, AWRCharacterBase* Target);
	static bool BeginStrike(AWRBoss* Boss, FWRBossAttack& Attack);
	static bool TickStrike(AWRBoss* Boss, FWRBossAttack& Attack, float DeltaSeconds);
	static void EndStrike(AWRBoss* Boss, const FWRBossAttack& Attack);
	static void HitLanded(AWRBoss* Boss, AWRCharacterBase* Victim, FName AttackId, EWRHitResult Result);
	static float Incoming(AWRBoss* Boss, const FWRHit& Hit, float Damage);
	static bool OwnWeakness(const AWRBoss* Boss);
	static void Damaged(AWRBoss* Boss, const FWRHit& Hit, float Damage);
	static void PhaseChanged(AWRBoss* Boss, int32 NewPhase);
	static bool PlaceRunback(AWRBoss* Boss);
	static FName BossPart(const AWRBoss* Boss, bool bScarred, FName Fallback);
	static void Died(AWRBoss* Boss);
	/** Server: a held survivor pressed a button (mash) or a rider jumped (lets go). True when it was one of these. */
	static bool Mash(AWRCharacterBase* Victim);
	/** Every machine: carried in a snatch or swallowed (input does nothing but mash). */
	static bool IsHeld(const AActor* Who);
	/** Every machine: riding Aeolith's back (Jump lets go; blows land on its vents). */
	static bool IsRiding(const AActor* Who);
	/** A spore pot broke (WRProjectile's sporepot): on or near Aeolith it weighs its wings (+150). */
	static void SporeLanded(UWorld* World, const FVector& At, AActor* Thrower);
	/** Every machine: the spore pot's model ("code:sporepot"): a stoppered clay pot, deepcap spores cyan in its glaze. */
	static bool BuildThrownModel(const FString& ModelPath, class UProceduralMeshComponent* Mesh);
	/** The Ring (spawned by UWRSkyArenas). */
	static AWRSkyRing* Ring(const UObject* WorldContext);
};

/** Places the Sky's bosses, the Ring, the Wind altar and the stones (server); the E prompt to grab Aeolith's back. */
UCLASS()
class WILDREACH_API UWRSkyArenas : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UWRSkyArenas, STATGROUP_Tickables); }
	virtual bool IsTickable() const override;
	static UWRSkyArenas* Get(const UObject* WorldContext);

	/** Client: "Grab on" when Aeolith's back passes within reach in the Last Circle. */
	static bool Prompt(const AWRPlayerController* PC, FString& OutTarget, FString& OutVerb);
	static bool ClientInteract(AWRPlayerController* PC);
	/** Server: "world sky boss grab" / "world sky boss shift". */
	FString Command(AWRPlayerController* PC, const TArray<FString>& Args);

private:
	bool Place(FName BossId);
	TSet<FName> Placed;
	float PlaceClock = 0.f;
};
