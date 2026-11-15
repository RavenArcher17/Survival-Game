#pragma once

// The Fungal caverns' hazards and their machines (cooks/biome-fungal.md 2.8-2.10; cooks/build-biome-fungal.md 2.8-2.10).
//
//  - AWRSporeCloud: a pocket of spores 6-20 m across (never area fog). Inside: 12 poison buildup a second after a 0.9 s
//    grace (motes gather on you first), rot 12/s as well after 10 s inside without a 3 s break; it can kill. Passes: the
//    Immune system (spore.immune_system), a filter mask or Sporeweave 4 (gear passSpores), the spore wad (x0.3), every
//    fungal creature. Its three answers: wind pushes it along the gust (6 m, 15 m strong) eased over 0.8 s, never a
//    snap; fire inside it (a fire blow, a burn on someone in it, a torch carried in, a fire spell) flashes it after a 0.9 s
//    rising whoomp: 40 + burn 40 to everyone inside, gone 2 game hours; frost settles it as spore snow, gone 1 game hour.
//    A cloud pushed by wind remembers it for 2 s (Mycelorn chokes on one). Path clouds (Spore cloud, Blight, Living
//    colony) are clouds too, owned: harmless to their owner and party.
//  - AWRLureLines: a hall's glowing beaded threads (the Waitomo glowworm's snares). A line you brush twangs and brightens
//    0.9 s, then grips: no moving 2 s, then it reels you 3 m up over 1.5 s and lets go. A light swing within 1.5 m cuts,
//    fire burns 3 m, wind tears 8 m, a Tunnel toad's croak snaps 8 m; cut lines regrow in a game hour. Crouch passes under.
//  - AWRSpringcap: a bulging pressure cap (Pilobolus): land on it and it creaks 0.6 s; jump in that window and it throws
//    you 9 m up (20 degrees of aim by your stick); a drop of 6 m onto it fires it anyway; a blow on it throws whoever
//    stands on it 5 m sideways. Regrows its charge in 20 s.
//  - AWRCapFall: a rot-softened ceiling slab in the Webs. Loud work within 15 m (blows, picks) sets it knocking (two
//    knocks, then three: the knockers), grit trickles and a dust shaft marks the 3 m circle for 1 s, then it falls: 70 and
//    a knockdown. A Knocker idol worn nearby gives a second more; a tame Truffle mole knocks back 10 s early.
//  - AWRBreathVent: one of the three bellows-stones on the Fall Field: a carved collar over a fissure, a hinged bronze flap
//    on its pivot pin. One exhales every 40 s (30 in the Sporefall) for 6 s up the Flue (its plume is a cloud column),
//    on the same clock the Sky lane's Aeolith ring reads.
//  - AWRFungalSpot: the caverns' doors and secrets with an E prompt: the Sealed Gate's web sheet (burn 3 s or any wind),
//    the mole wall's rubble (a tame Truffle mole digs it, 20 s), the fifth tunnel's blank wall (a mole knocks, a pick
//    breaks it), the Ring of Caps' altar (midnight and a toad tame: it slides), the Heart Gate (opens from inside), the
//    Seed Vault (only in the Sporefall).
//  - AWRFungalState: what every machine needs of the caverns' live state (the Sporefall, the bloom clock offset).

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "WRFungalHazards.generated.h"

class AWRCharacterBase;
class AWRCreature;
class AWRPlayerController;
class UProceduralMeshComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;
struct FWRHit;

namespace WRFungalTags
{
	/** In foul air (fungal 2.8): no sprint, stamina regen x0.5, no fire (both machines read it). */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(FoulAir);
	/** A creature carrying a Sporefall fruiting cap (fungal 2.10). */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Bloomed);
	/** A tame Mycelorn summited (fungal 2.14): it climbs a terrace wall and clamps there. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Summited);
	/** Glow paste on blade and helm: a cold cyan light 6 m (the server sets it from the buff; every machine draws it). */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GlowPaste);
}

/** The caverns' replicated live state (one per world). */
UCLASS(NotPlaceable)
class WILDREACH_API AWRFungalState : public AActor
{
	GENERATED_BODY()

public:
	AWRFungalState();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	static AWRFungalState* Get(const UObject* WorldContext);
	/** The Sporefall: 0 none, 1 warning, 2 on. */
	UPROPERTY(Replicated) uint8 Sporefall = 0;
	UPROPERTY(Replicated) float SporefallStart = 0.f;
	/** An invasion up or down the Flue (the column and the bridges, the downdraft): 0 none, 1 Spore going up, 2 Wind coming down. */
	UPROPERTY(Replicated) uint8 FlueInvasion = 0;
};

UCLASS(NotPlaceable)
class WILDREACH_API AWRSporeCloud : public AActor
{
	GENERATED_BODY()

public:
	AWRSporeCloud();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Server: a cloud. Kind: world (a place's, it regrows), drift (Spore drift weather), vent (a Breath plume column),
	 *  flush (the Sporefall), path (a Spore walker's), boss, creature (a burst, a shake, a garden puff), pot (a spore pot).
	 *  Lifetime 0: it stays (world clouds go and come back). Height > 0: a column (the plumes). */
	static AWRSporeCloud* Spawn(UWorld* World, const FVector& At, float Radius, FName Kind, float Lifetime, AActor* Owner = nullptr, float Height = 0.f);
	/** Every live cloud in this world (every machine). */
	static const TArray<TWeakObjectPtr<AWRSporeCloud>>& All();
	/** The live cloud containing this point (grown by Grow cm), or null. */
	static AWRSporeCloud* At(const UObject* WorldContext, const FVector& Pos, float Grow = 0.f);

	UPROPERTY(Replicated) FName Kind;
	UPROPERTY(Replicated) float Radius = 800.f;
	UPROPERTY(Replicated) float Height = 0.f;
	/** 0 live, 1 the flash's tell (0.9 s), 2 gone (settled or flashed, waiting to regrow), 3 the settling snow (3 s). */
	UPROPERTY(ReplicatedUsing=OnRep_State) uint8 State = 0;
	UPROPERTY(Replicated) float StateAt = 0.f;
	/** The push: where it is going (offset from Home) and where it was when the push began, and when (eased 0.8 s). */
	UPROPERTY(Replicated) FVector Home = FVector::ZeroVector;
	UPROPERTY(Replicated) FVector Offset = FVector::ZeroVector;
	UPROPERTY(Replicated) FVector OffsetFrom = FVector::ZeroVector;
	UPROPERTY(Replicated) float OffsetAt = -100.f;
	/** Who made it (a Spore walker: harmless to them and their party; a boss: its own clouds). */
	UPROPERTY(Replicated) TObjectPtr<AActor> Maker;
	/** Its size multiplier now (the Spore bloom underground, the Sporefall's +50% on Mycelorn's). */
	UPROPERTY(Replicated) float Scale = 1.f;
	/** Server: the last time wind moved it (Mycelorn chokes on a cloud pushed within 2 s). */
	float LastWindAt = -100.f;
	AActor* LastWindBy = nullptr;
	/** Server: it goes at this server time (0: never; world clouds regrow instead). */
	float DieAt = 0.f;
	/** Its centre now (Home + the eased push), every machine. */
	FVector Centre() const;
	float RadiusNow() const { return Radius * Scale; }
	bool IsLive() const { return State == 0 || State == 1; }
	bool Contains(const FVector& P, float Grow = 0.f) const;

	/** Server: wind along Dir, Dist cm (eased 0.8 s; clamped so a world cloud never leaves its hall). */
	void Push(const FVector& Dir, float Dist, AActor* By);
	/** Server: fire in it: the tell, then the flash. */
	void Ignite(AActor* By);
	/** Server: frost in it: it falls as spore snow. */
	void Settle(AActor* By);
	/** Server: harmless to this character (the owner and their party, its own kind of creature)? */
	bool Spares(const AWRCharacterBase* Who) const;

private:
	UFUNCTION() void OnRep_State();
	void UpdateLook();
	void ServerTick(float Dt);
	void Flash();
	struct FInside { float In = 0.f; float Continuous = 0.f; float Out = 0.f; };
	TMap<TWeakObjectPtr<AWRCharacterBase>, FInside> Inside;
	TWeakObjectPtr<AActor> IgnitedBy;
	float FireHeld = 0.f;
	float CheckClock = 0.f;
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> Fx;
	uint8 LookState = 255;
	float LookRadius = -1.f;
};

UCLASS(NotPlaceable)
class WILDREACH_API AWRLureLines : public AActor
{
	GENERATED_BODY()

public:
	AWRLureLines();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	static const TArray<TWeakObjectPtr<AWRLureLines>>& All();

	/** Which line set of fungal.json lines it draws (replicated: every machine builds the same lines from it). */
	UPROPERTY(ReplicatedUsing=OnRep_Set) int32 SetIndex = -1;
	/** Cut lines (one byte each: 1 cut). */
	UPROPERTY(ReplicatedUsing=OnRep_Cut) TArray<uint8> Cut;

	struct FLine { FVector Top; FVector Bottom; };
	const TArray<FLine>& GetLines() const { return Lines; }
	/** Server: cut every line within Radius of At (a blade, a burn, a gust, a croak). Returns how many. */
	int32 CutNear(const FVector& At, float Radius, FName Why);
	/** Server: lines a survivor is stuck on reel them; the nearest Webspinner within 30 m is told (WRFungalCreatures). */
	FVector LastTwangAt = FVector::ZeroVector;
	float LastTwangTime = -100.f;
	/** Every machine: the twang (brighten 0.9 s) on a line. */
	UFUNCTION(NetMulticast, Unreliable) void MulticastTwang(int32 Line);

private:
	UFUNCTION() void OnRep_Set();
	UFUNCTION() void OnRep_Cut();
	void MakeLines();
	void BuildLook();
	void ServerTick(float Dt);
	TArray<FLine> Lines;
	TArray<float> RegrowAt;
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Mesh;
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> TwangMesh;
	int32 TwangLine = -1;
	float TwangUntil = 0.f;
	struct FGrip { int32 Line = -1; float Touch = 0.f; float Stuck = 0.f; bool bReeling = false; };
	TMap<TWeakObjectPtr<AWRCharacterBase>, FGrip> Grips;
	float CheckClock = 0.f;
};

UCLASS(NotPlaceable)
class WILDREACH_API AWRSpringcap : public AActor
{
	GENERATED_BODY()

public:
	AWRSpringcap();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	static const TArray<TWeakObjectPtr<AWRSpringcap>>& All();
	/** Server time it last fired (it is spent until FiredAt + regrow), and when someone landed on it (the creak). */
	UPROPERTY(Replicated) float FiredAt = -100.f;
	UPROPERTY(Replicated) float LandedAt = -100.f;
	/** Server: a blow on it (it fires, throwing whoever stands on it sideways). */
	void Struck(AActor* By);
	bool IsCharged(float Now) const;

private:
	void Fire(AWRCharacterBase* Rider, bool bJumped, bool bSideways);
	void BuildLook();
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> CapMesh;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> CapMID;
	TWeakObjectPtr<AWRCharacterBase> Rider;
	float RiderSince = 0.f;
	float RiderFellFrom = 0.f;
	TMap<TWeakObjectPtr<AWRCharacterBase>, float> AirTop;
};

UCLASS(NotPlaceable)
class WILDREACH_API AWRCapFall : public AActor
{
	GENERATED_BODY()

public:
	AWRCapFall();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	static const TArray<TWeakObjectPtr<AWRCapFall>>& All();
	/** The ceiling over it (cm, world z). */
	UPROPERTY(Replicated) float Roof = 0.f;
	/** Server time the knocking began (the tell runs until FallAt), when it fell, when it grows back. */
	UPROPERTY(Replicated) float KnockAt = -100.f;
	UPROPERTY(Replicated) float FallAt = -100.f;
	UPROPERTY(Replicated) float RegrowAt = -100.f;
	/** Server: a noise here (loud work). */
	void Heard(const FVector& At, AActor* By);
	/** Server: a tame Truffle mole knocks back (it marks the circle 10 s before). */
	void MoleWarn();
	bool IsArmed(float Now) const { return Now >= RegrowAt && KnockAt < 0.f; }

private:
	void BuildLook();
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Slab;
	int32 KnocksPlayed = 0;
	bool bLanded = false;
};

UCLASS(NotPlaceable)
class WILDREACH_API AWRBreathVent : public AActor
{
	GENERATED_BODY()

public:
	AWRBreathVent();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	/** Which of the three (its turn in the rotation). */
	int32 VentIndex = 0;
	/** The rotation, every machine: which vent exhales now (-1 none) and how far into its exhale (s; < 0 the tell). The
	 *  Sky lane's Aeolith ring reads the same clock (AWRSkyRing::PlumeNow). */
	static int32 Phase(const UObject* WorldContext, float& OutPhase);

private:
	void BuildLook();
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Flap;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> Glow;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> PlumeFx;
	TWeakObjectPtr<AWRSporeCloud> Column;
	float FlapAngle = 0.f;
	int32 LastHuff = -1;
};

UCLASS(NotPlaceable)
class WILDREACH_API AWRFungalSpot : public AActor
{
	GENERATED_BODY()

public:
	AWRFungalSpot();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;
	static const TArray<TWeakObjectPtr<AWRFungalSpot>>& All();
	/** sealed_gate, mole_wall, fifth_tunnel, ring_altar, heart_gate, seed_vault. */
	UPROPERTY(Replicated) FName Kind;
	UPROPERTY(Replicated) FString Title;
	UPROPERTY(Replicated) FString Verb;
	/** Open (burned, dug, slid, unbarred); OpenAt: server time it opened (eased looks), CloseAt: it closes again then. */
	UPROPERTY(ReplicatedUsing=OnRep_Open) bool bOpen = false;
	UPROPERTY(Replicated) float OpenAt = -100.f;
	float CloseAt = 0.f;
	/** Progress of a held job (burning the web, digging): 0..1, replicated for the look. */
	UPROPERTY(Replicated) float Progress = 0.f;
	/** The way through it (yaw of the passage, its width and height, cm) and what it blocks with collision. */
	float Yaw = 0.f, Width = 600.f, HeightCm = 600.f;
	float Reach = 450.f;
	/** Server: E (or a hit, a gust, a mole's dig) on it. */
	FString Use(AWRPlayerController* PC);
	void Open(AActor* By, float Seconds = 0.f);
	void Close();
	/** Server: wind on it (the web sheet tears). */
	void Wind(AActor* By);
	/** Server: fire on it (the web sheet burns 3 s). */
	void Fire(AActor* By);

private:
	UFUNCTION() void OnRep_Open();
	void BuildLook();
	void UpdateLook();
	UPROPERTY(Transient) TArray<TObjectPtr<UPrimitiveComponent>> Blockers;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Moving;
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Sheet;
	float BurnLeft = 0.f;
	TWeakObjectPtr<AActor> BurnBy;
	bool bLookOpen = false;
};

namespace WRFungal
{
	/** Server: wind from Origin along Dir (a gust's cone: Range cm, Cone degrees): clouds in it move 6 m (15 m Strong),
	 *  lure lines in it tear (8 m), web sheets fall (the Sealed Gate, the Weaver's). */
	WILDREACH_API void Wind(AActor* By, const FVector& Origin, const FVector& Dir, float Range, float Cone, bool bStrong);
	/** Server: fire landed here (a fire blow, a burst, a projectile): a cloud it's in flashes, lines within 3 m burn, a
	 *  web sheet burns. */
	WILDREACH_API void FireAt(AActor* By, const FVector& At, float Radius = 300.f);
	/** Server: frost landed here: a cloud it's in settles. */
	WILDREACH_API void FrostAt(AActor* By, const FVector& At);
	/** Server: every blow through ApplyHit (its element at the target; a loud noise for the ceilings). */
	WILDREACH_API void OnHit(AWRCharacterBase* Target, const FWRHit& Hit);
	/** Server: a melee swing from Start to End (radius R): lines within 1.5 m cut, a springcap in it fires. */
	WILDREACH_API void Swing(AWRCharacterBase* By, const FVector& Start, const FVector& End, float R);
	/** Server: a noise (loud work) at At: the cap-fall ceilings within 15 m start knocking. */
	WILDREACH_API void Noise(const UObject* WorldContext, const FVector& At, AActor* By);
	/** This character carries fire (a lit torch or a fire lantern, a burning blade, the Fire path's heat over half). */
	WILDREACH_API bool CarriesFire(const AActor* Who);
	/** This character carries cold light (a glow-cap / lumen / foxfire lantern, glow paste, a Glowgrub tame near). */
	WILDREACH_API bool CarriesColdLight(const AActor* Who);
	/** A fire ability fizzles here (foul air: no fire can be lit). */
	WILDREACH_API bool SpellFails(const AActor* Who, const FGameplayTag& Element);
	/** Movement x for the caverns' holds (foul air: just under 1, so no sprint). */
	WILDREACH_API float MoveScale(const AActor* Who);
	/** The Spore bloom underground (fungal 2.9: every 20 min, 3 min): on now; its 30 s tell now. Every machine (the
	 *  server's clock). */
	WILDREACH_API bool BloomOn(const UObject* WorldContext);
	WILDREACH_API bool BloomTell(const UObject* WorldContext);
	/** The Sporefall (fungal 2.10): 0 none, 1 warning, 2 on. */
	WILDREACH_API uint8 Sporefall(const UObject* WorldContext);
	/** The Breath vents' rotation period now (40 s, 30 in the Sporefall): the Sky's Aeolith ring reads it. */
	WILDREACH_API float VentEvery(const UObject* WorldContext);
	/** The server's clock as every machine reads it. */
	WILDREACH_API float Now(const UObject* WorldContext);
	/** Server: a creature died (a bloomed one bursts into a 4 m cloud for 6 s). */
	WILDREACH_API void CreatureDied(AWRCreature* Self, AActor* Killer);
	/** Every machine: the thrown things' models (the Gale flask: "code:galeflask"). */
	WILDREACH_API bool BuildThrownModel(const FString& ModelPath, UProceduralMeshComponent* Mesh);
	/** Spore buildup on Who is taken at x this (passes and the wad), 0 = immune. */
	WILDREACH_API float SporeTaken(const AWRCharacterBase* Who);
}
