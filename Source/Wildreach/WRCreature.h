#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "WRCharacterBase.h"
#include "WRTaming.h"
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
	/** Stance at full (COMBAT-AND-LESSONS 1.4); 0 = its tier's (tuning stance.tiers). */
	float Stance = 0.f;
	/** Its own attacks carry hyperarmor: a blow that breaks its poise mid-attack doesn't stop it (big bodies, bosses). */
	bool bHyperarmor = false;
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
	// Mounts, breeding and bodies (cooks/build-taming.md)
	FName MountKind;              // ground / pack / swim / fly (creatures.json mountKind; "" = ground, or pack for haulers)
	FName Birth;                  // egg / live (creatures.json birth; "" = live)
	FName Habitat;                // shore: lives only where the land meets the sea (WRWildlife)
	float SeatFwd = 0.f;          // where the saddle sits: forward x radius, up x half height from the capsule centre
	float SeatUp = 1.f;
	float Back = 0.55f;           // half the back's width under the saddle, x radius
	float Carry = 0.f;            // kg its packs hold (haulers)
	float SwimSpeed = 0.f;        // > 0: a swimmer (cm/s at the surface); LandSpeed on land when it has one
	float LandSpeed = 0.f;
	float FlySpeed = 0.f;         // fly mounts: cm/s in the air
	FName ProcBody;               // a body built in code (seal: the Floeback, WRSealBody)
	float DiveDepth = 0.f;        // a swim mount that dives (cm below the surface; the Wavecharger 4000)
	float DepthMin = 0.f, DepthMax = 0.f;   // a wild swimmer's depth band below the surface (cm; creatures.json depthRange)
	FName Utility;                // light, mine_ore, sense_secrets
	TArray<TPair<FName, int32>> Products;
	bool bNest = false;
	bool bHearing = false;
	/** Hunts in the dark (FULL-GAME 6.1): notices an unlit player from darkness.darkHunterAggroMult x as far. */
	bool bDarkHunter = false;
	// Wild life
	TArray<FName> Attacks;        // besides the bite: lunge, charge, spit, stomp (numbers in tuning.json creatureAI.<attack>)
	FName Diet;                   // carnivore (hunts), herbivore, lithovore; from its food when not given
	FName Activity;               // diurnal (sleeps at night), nocturnal (sleeps by day), any
	FName Biome;                  // volcanic / tundra: the side of the island it lives on
	int32 PackMin = 1;            // how big its packs on the island are (WRWildlife)
	int32 PackMax = 1;
	float WanderChance = 0.f;     // the share of its packs that travel far instead of keeping to one area

	// Art: a skeletal mesh replaces the placeholder shape when set (scaled to the capsule, feet on the ground).
	FString Mesh;
	TMap<FName, FString> Anims;   // idle, walk, run, attack, hit, death; flyers also fly (moving) / hover (still) in the air
	TArray<FString> Materials;    // per-slot overrides ("" keeps the mesh's own), e.g. a tinted pack creature
	float MeshYaw = 0.f;
	float MeshScale = 1.f;        // multiplier on the auto fit
	float MeshWiden = 1.f;        // extra scale side to side only
	float Glow = 0.f;             // > 0, no mesh: a glowing orb (emissive sphere in its colour + a light this bright)
	float NightGlow = 0.f;        // > 0: its materials' GlowStrength rises to this as the daylight goes (a night hunter's eyes)
	/** An overlay over its own texture (creatures.json overlay: tint, pale, dark, bands, glowLine; M_TameMorph): a portal
	 *  boss's colouring on a rig it shares with another creature. */
	bool bOverlay = false;
	FLinearColor OverlayTint = FLinearColor::White;
	float OverlayPale = 0.f, OverlayDark = 0.f, OverlayBands = 0.f, OverlayGlowLine = 0.f;
	/** A portal-only creature (never in the wild tables); a portal boss's rank (0: not one). */
	bool bPortalOnly = false;
	int32 PortalRank = 0;
	TArray<FName> LegBones;       // a pack skeleton's legs for uneven ground: upper, lower, foot of each (front first, left then right)
	// Animation overrides (0 / None: measured from the clips, see UWRAnimInstance).
	float WalkPace = 0.f;         // cm/s the walk clip covers at play rate 1
	float RunPace = 0.f;
	float StrikeAt = 0.f;         // where in the attack clip (0..1) the bite lands
	bool bOwnWindup = false;      // its attack clip rears up on its own (Glacimaw lifts its front half): no telegraph rear-back
	FName AnimUpper;             // bone the attack / hit layer starts from (its subtree), so running legs keep running
	TArray<TPair<FName, float>> PoseLift;  // bones turned nose-up by these degrees over every clip (a head held low)
	// Its own proportions on a shared mesh (creatures.json shape; cooks/cookpass-creatures.md 3.5): the head's scale,
	// the neck's and tail's length, the wings' scale. 1 = the mesh as made.
	float ShapeHead = 1.f, ShapeNeck = 1.f, ShapeTail = 1.f, ShapeWings = 1.f;

	// --- Full combat (cooks/build-combat-full.md 2.7, 2.16) ---
	/** Damage x by physical type: [0] unused, [1] slash, [2] strike, [3] pierce (creatures.json physResist). */
	float PhysResist[4] = { 1.f, 1.f, 1.f, 1.f };
	/** What a blow on it sounds and sparks like (flesh, scale, chitin, stone, ice, metal, ember...). */
	FName PhysMaterial;
	/** Where the lock-on and the parts are: a bone the client finds by name, and the same place in body units for the
	 *  server (forward x radius, right x radius, up x half height from the capsule centre: the server has no mesh). */
	struct FBodyPoint
	{
		FName Id;
		FString Name;
		FName Bone;
		FVector At = FVector::ZeroVector;
		// Parts only: their own stance, what breaking them does and drops (combat.md 2.10).
		float Stance = 0.f;
		FName OnBreak;
		FName Drop;
	};
	TArray<FBodyPoint> LockPoints;
	TArray<FBodyPoint> Parts;
	/** Roaming mini-bosses wander a loop between these (cm, world), the arena being wherever they're met. */
	TArray<FVector> Roam;
	/** The first-kill drop's item and the name on its card (bosses). */
	FName FirstKillItem;

	/** Reads the combat fields above from its creatures.json row (WRCreatureCombat.cpp). */
	void ReadCombat(const TSharedPtr<class FJsonObject>& Row);

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

/** What a wild creature is doing, as every machine draws it and calls it (cooks/cookpass-creatures.md 3.2): the server's
 *  wild brain sets it (AWRCreature::SetMood), MoodRep carries it. */
enum class EWRCreatureMood : uint8
{
	Calm,     // going about its day (no beat)
	Graze,    // head down, chewing, a head-up check now and then
	Sniff,    // nose to the ground, working a scent
	Scan,     // looking round in held stops
	Preen,    // head to its flank
	Wary,     // staring at someone (WatchRep), still
	Threat,   // its display before the first charge: head low, eyes on its target
	Flee,     // running for it
	Eat,      // over a kill
};

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
	virtual void OnStanceBreakVisual(bool bBroken) override;
	/** AI movement curves round instead of sliding sideways: the way it walks leads its facing by at most
	 *  anim.creatureLeadDegrees; nearly stopped and facing well away, it turns on the spot first. */
	virtual void AddMovementInput(FVector WorldDirection, float ScaleValue = 1.f, bool bForce = false) override;

	const FWRCreatureDef* GetDef() const { return FWRCreatureRegistry::Get().Find(CreatureId); }
	/** Where a spit leaves it: its art's head bone if it has one (a long worm's head is metres ahead of its middle),
	 *  else the front of its capsule, halfway up. */
	FVector MouthLocation() const;
	AActor* GetAggroTarget() const { return AggroTarget.Get(); }
	void SetAggroTarget(AActor* Target) { AggroTarget = Target; }
	/** Vanish (sneaking perk): drops Who as its target and won't pick them again for Seconds (server). */
	void LoseSightOf(AActor* Who, float Seconds);
	bool HasLostSightOf(const AActor* Who) const;
	FVector GetHome() const { return Home; }
	/** Its pack has moved on (WRWildlife): home goes with it. Heading: the way the pack is walking (zero: it's staying here). */
	void FollowPack(const FVector2D& PackAt, const FVector2D& Heading);
	FVector2D GetPackHeading() const { return PackHeading; }
	float GetHealthFraction() const;
	void SetHealthFraction(float Fraction);
	/** Comes into view wanting a meal: its pack was on a hunt. */
	void MakeHungry() { HungryAt = 0.0; }
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
	/** bTamed: it was tamed just now (Taming XP, the tame moment); false for a hatch, a birth or a load. */
	void CompleteTame(const FString& PlayerName, bool bTamed = true);
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
	/** Its mood and who it's watching (cookpass-creatures 3.2): set by the server's wild brain; drawn and voiced on every
	 *  machine. A mood not refreshed for creatureAI.mood.staleSeconds falls back to Calm. */
	UPROPERTY(Replicated) uint8 MoodRep = 0;
	UPROPERTY(Replicated) TObjectPtr<AActor> WatchRep;
	void SetMood(EWRCreatureMood Mood, AActor* Watch = nullptr);
	EWRCreatureMood GetMood() const { return static_cast<EWRCreatureMood>(MoodRep); }
	/** Server: its mood unless it was set again within the stale time (biome hooks that take the brain's turn). */
	void TickMoodStale();
	double MoodSetAt = 0.0;
	/** Server: the wary stare (cookpass-creatures 3.2): who it watches and until when (held a moment after they leave). */
	TWeakObjectPtr<AActor> WaryOf;
	double WaryUntil = 0.0;
	double WaryStartAt = 0.0, WaryCalmUntil = 0.0;   // it gets used to a survivor who keeps their distance
	/** Server: a packmate bolted; it follows at HerdFleeAt (the herd goes together, a beat apart). */
	double HerdFleeAt = -1.0;
	TWeakObjectPtr<AActor> HerdFleeFrom;
	/** Every machine: an attack row's coil and release over its one attack clip (cookpass-creatures 3.3). */
	UFUNCTION(NetMulticast, Unreliable) void MulticastRowGesture(FName Motion, float Windup, int8 Side);
	/** This one's own voice (1 +- audio.voice.spread, seeded by its kind and where it came into the world). */
	float GetVoicePitch() const;

	// --- Raids (server; WRRaids) ---
	// A raider makes for the nearest piece of the claim it came for and bites it, and turns on anyone who comes near as
	// any hunter does. It never sleeps and never goes after prey; told to leave, it walks away from the base.
	bool bRaider = false;
	/** Server: it lies in a den through day and night (the Hollow Root's Mossmaul, WRForestHollow.cpp): asleep whatever
	 *  its activity says, until something hurts it or it finds a target (then it's awake as any sleeper woken). */
	bool bHibernating = false;
	/** A tame sent at a building ("Break that": FWRRaiding::OrderTamesToBreak): it bites it, then the next piece of the
	 *  same owner within raiding.tames.nextCm, until told otherwise (Come / Stay) or it's attacked. */
	TWeakObjectPtr<class AWRBuildPiece> SiegePiece;
	bool bRaidLeaving = false;
	FVector RaidBase = FVector::ZeroVector;
	/** The claim's radius: the pieces it breaks are the claim's owner's and their clan's within it. */
	float RaidRadius = 3000.f;
	FString RaidOwner;
	/** Its home becomes the base (its leash runs from there). */
	void SetHome(const FVector& At) { Home = At; }

	// --- The body (WRCarcass.cpp): a wild creature's kill stays down, to be butchered by hitting it ---
	/** Server: a melee hit on the body: a share of its loot into the hitter's bag (more with a blade in hand). */
	void HarvestBody(AWRCharacterBase* Harvester, bool bHeavy);
	/** A body with something left to take (every machine). */
	bool IsHarvestableBody() const { return BodyHitsLeft > 0; }
	int32 GetBodyHitsLeft() const { return BodyHitsLeft; }

	// --- Test control (server; dev commands WRCreatureDrive / Face / Attack / Flinch / Free, Tools\creaturevideo.ps1) ---
	// While any is set its brain is off (AWRCreatureAI::Tick runs TickTestControl instead) and it does only this.
	/** Walk / run toward world Yaw at SpeedScale x its speed (0.4 its wander walk, 1 its run) for Seconds, then stand. */
	void TestDrive(float Yaw, float Seconds, float SpeedScale);
	/** Stand still, turning on the spot (at its own turn rate) to face world Yaw. */
	void TestFace(float Yaw);
	/** Its basic bite as a real one looks (wind-up glow, call, clip, through its bite ability), standing where it is,
	 *  no target needed. False if it can't bite now. (A boss swings its next attack clip instead.) */
	virtual bool TestAttack();
	/** A hit reaction, no damage. */
	void TestFlinch();
	/** Back under its brain. */
	void TestRelease() { TestMode = 0; }
	bool IsTestControlled() const { return TestMode != 0; }
	void TickTestControl(float DeltaSeconds);

	/** Server: player mounts / dismounts. */
	FString Ride(AWRCharacter* Rider);
	void Dismount();
	AWRCharacter* GetRider() const { return Rider.Get(); }
	/** Server: a big blow throws the rider off (combat.md 2.19): dismounted and knocked down. */
	void Unhorse(AActor* By);
	/** Server: the rider's swing from the saddle (Light / Heavy on the mount: side by the camera, rear-up heavy, the
	 *  couched spear above mounted.couchSpeed). */
	UFUNCTION(Server, Reliable) void ServerRideAttack(bool bHeavy, bool bHeld);
	UFUNCTION(Server, Reliable) void ServerRideAttackRelease();
	/** A ridden Voidstrider's blink (Dodge while mounted; WRVoidCreatures). */
	UFUNCTION(Server, Reliable) void ServerRideBlink();

	// --- Lock points and parts (combat.md 2.10, 2.15) ---
	int32 NumLockPoints() const;
	/** World location of lock point i (the bone where this machine has the mesh, else the body-unit point). */
	FVector LockPointLocation(int32 Index) const;
	/** World location of a body point (lock point or part). */
	FVector BodyPointLocation(const FWRCreatureDef::FBodyPoint& Point) const;
	/** Is At within Radius of the lock point called Name (head strikes)? */
	bool IsNearLockPoint(const FVector& At, FName Name, float Radius) const;
	/** Server: a blow's stance also goes into the breakable part it landed on. */
	void DamagePart(const struct FWRHit& Hit, float Stance, AActor* By);
	/** A part has been broken this fight (its onBreak rule holds). */
	bool IsPartBroken(FName PartId) const;
	/** Every machine: the parts broken so far (bit per part). */
	UPROPERTY(ReplicatedUsing=OnRep_PartsBroken) uint8 PartsBroken = 0;
	UFUNCTION() void OnRep_PartsBroken();
	/** Server: forget the parts (a boss reset). */
	void ResetParts();

	/** Every machine: an attack's tell (cooks/build-combat-full.md 2.15): a small glint on the attacking part (0 none,
	 *  1 white = blockable, 2 crimson = unblockable) and a sound on it (its voice, a final cue). BoneHint is found in
	 *  this machine's skeleton by name (jaw, head, claw, tail, horn, hand). */
	UFUNCTION(NetMulticast, Unreliable) void MulticastTell(uint8 Glint, FName BoneHint, FName Sound);
	/** A bone whose name holds Hint (case-insensitive), or none (this machine has no mesh, or no such bone). */
	FName FindBone(FName Hint) const;
	/** Server: shelled up (Basaltback, Rimeshell): blows mostly glance off, blunt ones less so. */
	double WithdrawnUntil = 0.0;
	virtual float ModifyIncomingDamage(const struct FWRHit& Hit, float Damage) override;
	/** Server: what a part's break did (bosses add their own moves: Vulkarn drops his hammer...). */
	virtual void OnPartBroken(const FWRCreatureDef::FBodyPoint& Part, AActor* By);
	/** Server: damage it took (the Storm peaks creatures' wing hits, crackle-howl, alarm: WRStormCreatures.cpp). */
	virtual void OnDamageTaken(const struct FWRHit& Hit, float Damage) override;
	/** Its art's body (null on a dedicated server's placeholder): the Storm peaks looks attach their parts to its bones. */
	class USkeletalMeshComponent* GetCreatureSkel() const;

	UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_CreatureId, Category="Creature") FName CreatureId = TEXT("cinderfang");
	/** A fold's rank scaling (WRPortals), set before it begins play: health, bite and stance times. */
	float PortalHealth = 1.f;
	float PortalDamage = 1.f;
	float PortalStance = 1.f;
	/** A rank-coloured glow on a fold's boss or warden (alpha = how bright: the Alpha's mane lit or doused). */
	UPROPERTY(ReplicatedUsing=OnRep_PortalMark) FColor PortalMark = FColor(0, 0, 0, 0);
	UFUNCTION() void OnRep_PortalMark();
	UPROPERTY(ReplicatedUsing=OnRep_Tamed) FString TamedBy;
	UPROPERTY(Replicated) EWRTameOrder TameOrder = EWRTameOrder::Follow;
	UPROPERTY(Replicated) bool bTameAggressive = true;
	/** Server: a neutral creature set on a dark shrine's guard turned hostile (world.json shrines "aggressive": the
	 *  Toll Shrine's Windmane herd, cooks/biome-sky.md 2.13): it picks targets like an aggressive one. */
	bool bGuardAggressive = false;
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

	// --- Levels, bond, trees, tack, breeding, babies, mounts (WRTaming.cpp; cooks/build-taming.md) ---
	friend struct FWRTaming;
	/** Every machine: its levels, stats, tree, saddle, barding, growth, care, breeding and hunger. */
	UPROPERTY(ReplicatedUsing=OnRep_TameRep) FWRTameRep TameRep;
	/** A hauler's packs (every machine: the card's Cargo tab). */
	UPROPERTY(Replicated) TArray<FWRItemStack> Cargo;
	UFUNCTION() void OnRep_TameRep();
	/** Server: its genes (stats, mutations, morphs, sex) and the child she carries. */
	FWRTameGenes Genes;
	FWRTameGenes PregnantGenes;
	bool bPregnantTwins = false;
	/** Server: wild genes for its band (and now and then a wild mutant). */
	void RollWildGenes();
	void InitGenes(const FWRTameGenes& In);
	/** Server: its stats from genes, bond, tree, growth and tack (keeps its health share). */
	void RecomputeStats();
	/** Every machine: its multiplier on a stat (WRTameStat) as the server last worked it out. */
	float StatMult(int32 Stat) const;
	/** Has it learned this node of its role tree (by id)? */
	bool TameHasNode(FName NodeId) const;
	/** Server: XP from its own work (fight, ride, haul, dig, light, farm). */
	void GainBondXP(float Amount, const TCHAR* Why);
	/** Its imprint bonus now: x(1 + imprint x tame.imprint.bonus) while its imprinter rides it or is beside it. */
	float ImprintScale() const;
	FName GetMountKind() const;
	bool IsBaby() const { return TameRep.IsBaby(); }
	/** Where the saddle sits on its back (world). */
	FVector SeatLocation() const;
	/** Its packs: slots, kg it holds, kg in them. */
	int32 CargoSlots() const;
	float CargoCapacityKg() const;
	float CargoKg() const;
	/** Server: puts what fits into its packs; returns how many didn't. */
	int32 AddCargo(FName ItemId, int32 Count);
	/** Server: the care it asks for answered by Player (feed / walk / cuddle). */
	void AnswerCare(const FString& Player);
	/** Server: start a 2 s cuddle by Player (answered if they're still beside it at the end). */
	FString StartCuddle(AWRPlayerController* Player);
	/** Server: its saddle, barding and packs onto the ground (death, release). */
	void DropTack();
	/** The mate it's walking to (server). */
	TWeakObjectPtr<AWRCreature> MatePartner;
	/** Both machines that move it: ride speeds from its stats, tack, imprint, kind and load. */
	void ApplyRideSpeeds();
	/** Server: the mount takes off / lands (fly mounts). */
	UFUNCTION(Server, Reliable) void ServerFly(bool bFly);
	/** Every machine: the saddle, barding, morph overlay and baby size it shows (WRTameLook.cpp). */
	void RefreshTameLook();
	/** Called by the base's stamina regen: a ridden mount's own sprint drain and regen. */
	virtual bool MountStaminaRates(float& OutDrain, float& OutRegen) const override;
	/** Its name (its owner's, else its species'): the label, the card, the riding line, notifications. */
	FString TameName() const;
	/** Server: who asked to release it and until when (a second Release in time does it: cooks/cookpass-taming.md 3.3). */
	FString ReleaseArmedBy;
	double ReleaseArmedUntil = 0.0;
	/** Server: the player who rode it last (Call your mount). Saved. */
	FString LastRider;
	/** Server: called to its rider (it runs to CallTo's side; cleared when it gets there or is ordered). */
	TWeakObjectPtr<AActor> CallTo;
	double CallUntil = 0.0;
	/** Server: a soulbound fall's end (it wakes at tame.soulbond.wakeHealth). */
	double SoulDownUntil = 0.0;
	/** Server: brought back once by a Dadeni draught (a second death is for good). Saved. */
	bool bRevivedOnce = false;
	/** Server: where a rider getting off stands (clear ground beside it: cooks/cookpass-taming.md 3.7). */
	FVector DismountSpot(const AWRCharacter* Who) const;
	/** Every machine: where the bone its saddle rides on is now (the seated rider follows its rise and fall). */
	bool SaddleBoneLocation(FVector& Out) const;
	mutable FName SaddleBoneCache;
	mutable bool bSaddleBoneLooked = false;
	/** Server: when a rift last drained its max health (it mends once clear of them: cooks/cookpass-taming.md L13). */
	double RiftDrainedAt = -1000.0;
	/** Every machine: the courting / cuddle head dip's weight (eased), and whether the pose lift carries it now. */
	float NuzzleWeight = 0.f;
	bool bNuzzleLifted = false;

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
	void RidePadLook(const FInputActionValue& Value);

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

	TArray<float> PartStance;
	uint8 PartsShown = 0;
	double NextRiderBite = 0.0;
	double CouchUntil = 0.0;
	bool bCouchHit = false;
	TArray<TWeakObjectPtr<AActor>> RideHits;
	void TickRider(float DeltaSeconds);
	UPROPERTY(Transient) TObjectPtr<UInputAction> RideLightAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> RideHeavyAction;
	void RideLightPressed();
	void RideHeavyPressed();
	void RideHeavyReleased();

	TWeakObjectPtr<AActor> AggroTarget;
	/** Who it has lost sight of (Vanish) and until when (world seconds). */
	TMap<TWeakObjectPtr<const AActor>, double> LostSight;
	FVector Home = FVector::ZeroVector;
	FVector2D PackHeading = FVector2D::ZeroVector;
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
	/** nightGlow: its override materials as dynamic instances, their GlowStrength following the dark (0 by day and dead). */
	void TickNightGlow();
	UPROPERTY(Transient) TArray<TObjectPtr<class UMaterialInstanceDynamic>> NightGlowMats;
	float NightGlowNow = -1.f;
	float LastYaw = 0.f;      // for its turn rate (feet step while it turns on the spot)
	float EarshotClock = 0.f;
	class UWRAnimInstance* CreatureAnim() const;
	/** Stance break, the look (COMBAT-AND-LESSONS 1.4): its recoil held, the head dropped and the body sunk a little
	 *  (tuning stance.slump), and the gold weak-point mark on its head where the crit goes. */
	void TickStanceSlump(float DeltaSeconds);
	FName HeadBone() const;
	bool bStanceBrokenShown = false;
	float SlumpAlpha = 0.f;
	// The body language (WRCreatureBody.cpp, cookpass-creatures 3.1): what it looks at, its flinch, pant, slump, the
	// mood's and the row gesture's poses, drawn into the animation instance's Input.Body.
	void SetupCreatureBody(const struct FWRCreatureDef& Def);
	void TickCreatureBody(float DeltaSeconds);
	void KickFlinch();
	FVector BodyLookAt(bool& bOutHave, float& OutSpeed);
	struct FWRBodyLanguage
	{
		float LookYaw = 0.f, LookPitch = 0.f, LookAlpha = 0.f;
		float FlinchPos[3] = { 0.f, 0.f, 0.f }, FlinchVel[3] = { 0.f, 0.f, 0.f };  // spine yaw / pitch / roll spring
		float Exertion = 0.f;
		float PantPhase = 0.f;
		float Slump = 0.f;
		float MoodBlend = 0.f;            // eases each mood's pose in and out
		EWRCreatureMood MoodShown = EWRCreatureMood::Calm;
		double MoodSince = 0.0;
		float Dip = 0.f, SpinePitch = 0.f, HeadRoll = 0.f, HeadYaw = 0.f;  // the eased mood pose
		double GlanceUntil = 0.0, NextGlanceAt = 0.0;
		TWeakObjectPtr<AActor> GlanceAt;
		float ScanYaw = 0.f;
		double NextScanAt = 0.0;
		int32 ScanStep = 0;
		double NextCheckAt = 0.0, CheckUntil = 0.0;  // a grazer's head-up
		FName Gesture;
		double GestureStart = -1.0;
		float GestureWindup = 0.f;
		int8 GestureSide = 1;
		float Seed = 0.f;
		float VoicePitch = 1.f;
		FName Head;
		bool bReady = false;
	};
	FWRBodyLanguage BodyLang;
	FVector SlumpBaseLocation = FVector::ZeroVector;
	FRotator SlumpBaseRotation = FRotator::ZeroRotator;
	bool bSlumpBase = false;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> CritMark;
	/** The layered animation instance: locomotion paced to the ground speed, attacks / hits over it, crossfades. */
	void SetupCreatureAnim(const FWRCreatureDef& Def);
	void TickCreatureAnim(float DeltaSeconds);
	/** Generated skeletons on uneven ground: the ground under each foot for the animation instance (traced only near a
	 *  local view and while its pose updates; smoothed), and how much the body tilts / the legs reach (off in the air,
	 *  in the water, far away; the legs let go once it's dead). Numbers in tuning anim.creatureFeet. */
	void TickCreatureFeet(float DeltaSeconds);
	float FeetLegCm = 0.f;      // leg length (hip to ankle) in cm at the mesh's scale
	bool bFeetInRange = false;  // within anim.creatureFeet.range of a local view (checked with the earshot clock)
	float FeetLogClock = 0.f;   // wr.CreatureFeetLog
	// Test control (above).
	uint8 TestMode = 0;  // 0 off, 1 stand facing TestYaw, 2 drive
	float TestYaw = 0.f, TestScale = 0.4f;
	double TestDriveUntil = 0.0;
	UFUNCTION(NetMulticast, Unreliable) void MulticastTestFlinch();
	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> RideMapping;
	UPROPERTY(Transient) TObjectPtr<UInputAction> RideMoveAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> RideLookAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> RidePadLookAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> RideJumpAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> RideDismountAction;

	// Taming (WRTaming.cpp) and its look (WRTameLook.cpp).
	UPROPERTY(Transient) TObjectPtr<UInputAction> RideSprintAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> RideDiveAction;
	void RideSprintPressed();
	void RideSprintReleased();
	void RideJumpPressed();
	void RideJumpReleased();
	void RideDivePressed();
	void RideDiveReleased();
	bool bClimbHeld = false;
	bool bDiveHeld = false;
	/** Server: once a second (and each frame for the cheap parts) while tamed. */
	void TickTameLife(float DeltaSeconds);
	/** Both machines that move it: flight (climb, glide, ceiling, stamina), swimming, a mount's charge. */
	void TickMount(float DeltaSeconds);
	float TameClock = 0.f;
	int32 CareSlice = -1;
	double CuddleUntil = -1.0;
	FString CuddleBy;
	double StarvingSince = -1.0;
	float RideMetres = 0.f, HaulMetres = 0.f, LightClock = 0.f;
	FVector LastTameAt = FVector::ZeroVector;
	bool bHasLastTameAt = false;
	double ChargeReadyAt = 0.0, ChargeStrikeAt = -1.0;
	TWeakObjectPtr<AWRCharacterBase> ChargeTarget;
	bool bBirthHeldNotified = false;
	bool bBardingLava = false, bBardingIce = false;
	uint32 TackLookKey = 0;
	UPROPERTY(Transient) TObjectPtr<class UProceduralMeshComponent> SaddleMesh;
	UPROPERTY(Transient) TObjectPtr<class UProceduralMeshComponent> BardingMesh;
	UPROPERTY(Transient) TArray<TObjectPtr<class UProceduralMeshComponent>> BardingPlates;
	UPROPERTY(Transient) TObjectPtr<class UMaterialInstanceDynamic> MorphMat;
	UPROPERTY(Transient) TObjectPtr<class UWRSealBody> SealBody;
	float ExpectedPoints() const;
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
	// The Storm peaks creatures' and bosses' own moves run through its row machine (WRStormCreatures / WRStormBosses).
	friend struct FWRStormAI;
	friend struct FWRStormBoss;
	friend struct FWRSwampAI;
	friend struct FWRSwampBoss;
	friend struct FWRVoidAI;
	friend struct FWRForestAI;   // the Ancient forest's creatures (WRForestCreatures): drops, grabs, heat balls, bugles
	friend struct FWRCrystalAI;  // the Crystal caves' creatures (WRCrystalCreatures): hearing hunters, plates, legs, rolls
	friend struct FWRGraveAI;    // the Graveyard's creatures (WRGraveCreatures): buried hounds, ceiling drops, light dives, holds
	friend struct FWRDesertAI;   // the Desert's creatures (WRDesertCreatures): the dive, the grip, the glare, the circle
	friend struct FWROceanAI;    // the Ocean's creatures (WROceanCreatures): holes, the drag, the breach, buried crabs, herds
	friend struct FWRFungalAI;   // the Fungal caverns' creatures (WRFungalCreatures): ceiling drops, the mound, latches, the net, the leap
	friend struct FWRSkyAI;      // the Sky islands' creatures (WRSkyCreatures): soaring rings, stoops, the Undertow, holds, drift

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
	/** A raider with nobody to fight (WRRaids): to the nearest piece of the claim it came for, and bites it. */
	void TickRaid(AWRCreature* Self, float DeltaSeconds);
	/** A tame on a "Break that" order: to the piece, and bite it (FWRRaiding, bite x raiding.tameBite). */
	void TickSiege(AWRCreature* Self, float DeltaSeconds);
	class AWRBuildPiece* PickRaidPiece(AWRCreature* Self) const;
	TWeakObjectPtr<class AWRBuildPiece> RaidPiece;
	TWeakObjectPtr<class AWRBuildPiece> RaidBitePiece;
	float RaidPickClock = 0.f;
	double NextRaidBiteAt = 0.0;
	double RaidBiteLandsAt = -1.0;
	// Moods (cookpass-creatures 3.2): the threat display before its first charge, the calm beats, the wary stare.
	/** At its spot (or a tame standing by): a beat now and then (graze, sniff, scan, preen by diet); false = it may move on. */
	bool TickBeat(AWRCreature* Self, bool bStill);
	/** Passive / neutral wild ones: stare at the nearest survivor in notice range; a passive one bolts when flushed (true:
	 *  it stands staring, or it just bolted). */
	bool TickWary(AWRCreature* Self);
	double ThreatUntil = 0.0;
	TWeakObjectPtr<AActor> ThreatOf;
	double BeatUntil = 0.0, NextBeatAt = 0.0;
	EWRCreatureMood BeatMood = EWRCreatureMood::Calm;
	FName Special;
	int32 SpecialPhase = 0;  // 0 none, 1 windup, 2 under way
	float SpecialClock = 0.f;
	FVector SpecialDir = FVector::ForwardVector;
	bool bSpecialHit = false;
	TWeakObjectPtr<AWRCharacterBase> SpecialTarget;
	TMap<FName, double> SpecialReadyAt;

	// --- Attack rows (Data/attacks.json, cooks/build-combat-full.md 2.15) ---
	/** A creature with rows fights by them: pick, tell, track, hold, strike, punish, combo, breathe. */
	void TickRowFight(AWRCreature* Self, AWRCharacterBase* Target, float DeltaSeconds);
	bool StartRow(AWRCreature* Self, AWRCharacterBase* Target, FName RowId, bool bCombo);
	void TickRow(AWRCreature* Self, float DeltaSeconds);
	void StrikeRow(AWRCreature* Self);
	void EndRow(AWRCreature* Self, bool bInterrupted);
	FName PickRow(AWRCreature* Self, AWRCharacterBase* Target, bool bReading) const;
	void RowHit(AWRCreature* Self, AWRCharacterBase* Victim);
	/** Where a pack member wants to stand (leader in front, flankers at their angle, one on the far side). */
	FVector GroupSpot(AWRCreature* Self, AWRCharacterBase* Target, bool& bInPlace) const;
	FName RowId;
	int32 RowPhase = 0;          // 0 none, 1 wind-up, 2 hold, 3 strike, 4 punish
	double RowPhaseEnd = 0.0;
	double RowStarted = 0.0;
	double RowStrikeStarted = 0.0;
	bool bRowFinalCue = false;
	int32 RowCombo = 0;
	FVector RowDir = FVector::ForwardVector;
	FVector RowAim = FVector::ZeroVector;
	FVector RowOrigin = FVector::ZeroVector;
	float RowTravelled = 0.f;
	TWeakObjectPtr<AWRCharacterBase> RowTarget;
	TSet<TWeakObjectPtr<AActor>> RowHits;
	TMap<FName, double> RowReadyAt;
	double BreatheUntil = 0.0;
	float BreatheSide = 1.f;
	double ReadUsedAt = -100.0;
	double RegroupUntil = 0.0;
	FVector RegroupAt = FVector::ZeroVector;
	float StanceWindow = 0.f;    // stance lost in the last few seconds (Basaltback withdraws)
	double StanceWindowStart = 0.0;
	float LastStance = -1.f;
	double BiteTrackUntil = 0.0;  // a bite's wind-up turns toward the target until then, then commits
	double NextSnapAt = 0.0;      // its next bite at running prey
	double NextChaseLogAt = 0.0;  // verbose log while hunting
	float ThinkTimer = 0.f;
	float FlankAngle = 0.f;
	FVector WanderGoal = FVector::ZeroVector;
	float WanderClock = 0.f;
};

/** What a roaming pack is up to, in view or not (WRWildlife). */
enum class EWRPackDoing : uint8 { Grazing, Travelling, Hunting, Resting };

/**
 * Keeps a number of creatures alive around itself; nesting species get an egg nest. A map-placed one holds its place and
 * replaces its dead. On the island most are roaming packs (bRoams, WRWildlife): this actor is then the pack's record,
 * wherever it has got to, and Count is how many are left (a pack doesn't grow back: its region spawns new packs instead).
 */
UCLASS()
class WILDREACH_API AWRCreatureSpawner : public AActor
{
	GENERATED_BODY()

public:
	AWRCreatureSpawner();
	virtual void Tick(float DeltaSeconds) override;

	bool bRoams = false;
	/** The spawn region it belongs to (Data/spawn_regions.json): it counts toward that region's population wherever it roams. */
	FName Region;
	EWRPackDoing Doing = EWRPackDoing::Grazing;
	/** The middle of where a predator prowls. */
	FVector2D Territory = FVector2D::ZeroVector;
	/** Travels far instead of keeping to one area. */
	bool bWanderer = false;
	/** How hurt they were when last in view, 0..1 (mends with time). */
	float PackHealth = 1.f;
	double NextThinkAt = 0.0;
	double FedUntil = 0.0;
	/** Where it's walking, next point first. */
	TArray<FVector2D> Path;
	TWeakObjectPtr<AWRCreatureSpawner> Quarry;
	FVector2D QuarryWas = FVector2D::ZeroVector;
	/** Walks on along its path; when the pack is in view, its creatures follow. */
	void Advance(float Seconds);
	/** In view and caught up in something (a fight, a hunt, running away, a meal): it waits for them. */
	bool IsBusy() const;
	float AverageHealth() const;

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
	/** Day and night tables (spawn_regions.json night; cooks/build-world-systems.md 2.9): 0 out at all hours, 1 a day
	 *  pack (lies up while the night table runs, unless its kind sleeps where it stands), 2 a night pack (lies up by day). */
	uint8 Shift = 0;
	/** Lying up just now: not brought out near players. */
	bool IsLyingUp() const;
	/** A summoning stone (WRWorldSystems): the boss this spawner waits to bring back comes back now. False: none due. */
	bool SummonNow();

protected:
	virtual void BeginPlay() override;

private:
	void SpawnOne();
	void Activate();
	void Deactivate();
	bool PlayerNear(float Range) const;
	/** The ground under a point. bFound false: the ground isn't there yet (it hasn't streamed in). */
	FVector GroundAt(const FVector& Point, bool& bFound) const;
	TArray<TWeakObjectPtr<AWRCreature>> Alive;
	TArray<double> PendingRespawns;
	bool bActive = false;
	double AloneSince = -1.0;
	/** A roaming pack can come into view before its ground has streamed in: these wait a few seconds for it. */
	int32 SpawnsWaiting = 0;
	int32 GroundWaits = 0;
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
