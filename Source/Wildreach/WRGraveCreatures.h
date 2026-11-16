#pragma once

// The Graveyard's nine creatures and the Risen (cooks/biome-graveyard.md 2.12; cooks/build-biome-graveyard.md 2.14): what
// they do beyond their attack rows (Data/attacks.json) and how they look on their stand-in rigs.
//
//  - UWRGraveBody: a replicated component the server adds to every Graveyard creature (creatures.json biome
//    "graveyard", not a boss). It carries what every machine draws from (flags: buried, on the ceiling, holding, rearing,
//    grounded, raised; the body it holds; the target a Cryptstalker has found) and, on machines that draw, the look: the
//    cook's palette over the rig's own texture (M_CreatureTint) and the parts that make the silhouette, modelled from the
//    look kit on frames riding the rig's bones (the Gravehound's skull face and spine ridge, the Cryptstalker's whips and
//    palps, the Wraithmoth's skull mark, the Palestrider's bone face and torn mane, the Tombback's grave slab, the
//    Gloamcrow's hood, the Mournhare's ears, the Wickbeetle's wax and tail light, the Marrowgoat's horns and its bone).
//    The Risen are whole skeletons on the Mixamo and Barghest rigs: the rig body hidden, every bone a modelled bone.
//  - FWRGraveAI: the hooks AWRCreatureAI calls (buried hounds and the Rising, the Grim howl, the whip sweep's "found",
//    the ceiling drop, the light dive at the brightest light, the tear-drinker on a downed survivor, the slab's cover, the
//    Palestrider's scream, the crow funeral, the Risen's bone collapse).
//  - UWRGraveLife: the server's living-world beats and the catacombs' dens.

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRGraveCreatures.generated.h"

class AWRCreature;
class AWRCreatureAI;
class AWRCharacterBase;
class USkeletalMeshComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;
struct FWRAttackRow;
struct FWRHit;
struct FWRLookBlock;
enum class EWRHitResult : uint8;

namespace WRGraveFlag
{
	enum : uint16
	{
		Buried   = 1 << 0,   // a Gravehound asleep under the grave soil (a mound, two eyes in the dirt at night)
		Rising   = 1 << 1,   // the soil heaving as it comes up (the Rising's wind-up)
		Ceiling  = 1 << 2,   // a Cryptstalker clinging upside down to a catacomb ceiling
		Holding  = 1 << 3,   // a grab holding someone (palp snatch, tear-drinker)
		Rearing  = 1 << 4,   // a Tombback up on its back legs (belly open)
		Grounded = 1 << 5,   // a Wraithmoth down after a dive (the bring-down window)
		Raised   = 1 << 6,   // a body raised by a Necromancer (grey hide, grave-light threads, green eyes)
		Silent   = 1 << 7,   // a Gloamcrow at a crow funeral (perched, silent)
		Burying  = 1 << 8,   // a Tombback sitting on a kill it buried
		Sweeping = 1 << 9,   // a Cryptstalker's whips fanned out
		Riftborn = 1 << 10,  // came back from the south's rifts changed (GY 2.12: Gravehound, Wraithmoth, Tombback south
		                     // of the neck): desaturated hide, black-violet eyes and voidglass on the spine, +25% HP, curse
		Ash      = 1 << 11,  // raised as an ash wraith (Fire + Necromancy: it died burning): embers in its threads
		Ice      = 1 << 12,  // raised as an ice minion (Frost + Necromancy, Frozen legion): rime and pale-blue threads
	};
}

UCLASS(ClassGroup=(Creature))
class WILDREACH_API UWRGraveBody : public UActorComponent
{
	GENERATED_BODY()

public:
	UWRGraveBody();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	static UWRGraveBody* Get(const AActor* Actor);
	/** A creatures.json row of the Graveyard that isn't a boss. */
	static bool IsGraveKind(FName CreatureId);
	/** A Risen (creatures.json "risen"): no corpse, no taming. */
	static bool IsRisen(FName CreatureId);
	/** Server: the component on a new Graveyard creature (AWRCreature::BeginPlay); also on a raised body. */
	static void Attach(AWRCreature* Creature);
	/** Server: a body a Necromancer raised (any creature, not only the Graveyard's): the raised look (grey hide where the
	 *  rig takes the tint, grave-light threads in its wounds, pale-green eyes); Variant ash / ice / blood. */
	static void AttachRaised(AWRCreature* Creature, FName Variant);
	/** Server, once at spawn: the Graveyard's Riftborn roll (graveyardLife.riftborn: its share of Gravehounds,
	 *  Wraithmoths and Tombbacks south of fromY; +25% health, curse on every blow). */
	void RollRiftborn(AWRCreature* C);
	static TArray<TWeakObjectPtr<UWRGraveBody>> Live;

	UPROPERTY(ReplicatedUsing=OnRep_Flags) uint16 Flags = 0;
	UPROPERTY(Replicated) TObjectPtr<AActor> Holding;
	UPROPERTY(Replicated) TObjectPtr<AActor> Found;

	bool Has(uint16 Flag) const { return (Flags & Flag) != 0; }
	void Set(uint16 Flag, bool bOn);

	/** Every machine: a moment (an effect at a bone, a sound). */
	UFUNCTION(NetMulticast, Unreliable) void MulticastMoment(FName Effect, FName Bone, float Scale, FName Sound);

	// --- Server state ---
	double FoundUntil = 0.0;
	TWeakObjectPtr<AWRCharacterBase> HeldBody;
	double HoldUntil = 0.0, HoldTickAt = 0.0;
	FName HoldKind;
	float FreeDamage = 0.f;
	double GroundedUntil = 0.0;
	double NextLightLookAt = 0.0;
	double RiseStartedAt = 0.0;
	double NextVoiceAt = 0.0;
	double SilentUntil = 0.0;
	TWeakObjectPtr<AActor> MobTarget;
	FVector CeilingAt = FVector::ZeroVector;
	bool bHowled = false;
	bool bDropping = false;

private:
	UFUNCTION() void OnRep_Flags();
	void BuildLook();
	void ClearLook();
	void TickLook(float DeltaTime);
	void ApplyBuriedLook();
	USceneComponent* AddGroup(FName Bone, const FVector& WorldAt, const FRotator& WorldRot, const TArray<FWRLookBlock>& Blocks, int32 Kind);
	USkeletalMeshComponent* Skel() const;
	/** A modelled bone from bone A toward bone B (a skeleton's limb), Radius thick; knobs at both ends. */
	void BoneSeg(FName A, FName B, float Radius, const FLinearColor& Col, int32 Kind = 0);

	FName LookFor;
	/** The flags the look was built for (Raised, Riftborn): a change rebuilds it. */
	uint16 LookFlags = 0;
	struct FGroup { TWeakObjectPtr<USceneComponent> Frame; int32 Kind = 0; FRotator BaseRot = FRotator::ZeroRotator; float Phase = 0.f; };
	TArray<FGroup> Groups;
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> LookComps;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> TintMats;
	/** A raised body with no palette row of its own: the bone-grey wash laid over its hide (cookpass-creatures). */
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> RaisedGrey;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> Glow;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Mound;
	float Clock = 0.f;
	int32 LookTries = 0;
	bool bBuriedShown = false;
	bool bMeshHidden = false;
};

/** The hooks AWRCreatureAI calls for Graveyard creatures (friend of AWRCreatureAI). */
struct WILDREACH_API FWRGraveAI
{
	/** Wild brain: true when it ran this tick itself (buried, on the ceiling, holding, grounded, at a funeral). */
	static bool TickWild(AWRCreatureAI* AI, AWRCreature* Self, float DeltaSeconds);
	static bool TickTame(AWRCreatureAI* AI, AWRCreature* Self, float DeltaSeconds);
	static bool RowAllowed(const AWRCreatureAI* AI, const AWRCreature* Self, const FWRAttackRow& Row, const AWRCharacterBase* Target, bool bReading);
	static void RowStarted(AWRCreatureAI* AI, AWRCreature* Self, const FWRAttackRow& Row, AWRCharacterBase* Target);
	/** A row's strike it takes over (the ceiling drop): true when it did. */
	static bool StrikeRow(AWRCreatureAI* AI, AWRCreature* Self, const FWRAttackRow& Row, AWRCharacterBase* Target);
	static void Resolved(AWRCreature* Self, AWRCharacterBase* Victim, FName RowId, EWRHitResult Result);
	static void ModifyHit(const AWRCreature* Self, FWRHit& Hit, const FWRAttackRow* Row);
	static float Incoming(const AWRCreature* Self, const FWRHit& Hit, float Damage);
	/** After a blow landed (server): a held body struck free, a moth shot down, the Palestrider's scream, the crows' mob. */
	static void Damaged(AWRCreature* Self, const FWRHit& Hit, float Damage);
	/** A survivor's stamina regen x this (a Wraithmoth's dust chills it: x0.6 for 6 s). */
	static float StaminaRegenScale(const AActor* Who);
	static void Died(AWRCreature* Self, AActor* Killer);
	/** A Risen leaves no body (it falls to a bone pile): the death path skips the carcass. */
	static bool NoBody(const AWRCreature* Self);
	static void TickBody(AWRCreature* Self, UWRGraveBody* Body, float DeltaTime);
	/** Server: every buried Gravehound within Radius of At rises (the Grim howl; a den disturbed). */
	static int32 WakeBuried(UWorld* World, const FVector& At, float Radius, AActor* Toward);
	/** Server: bury a Gravehound where it stands (its den by day). */
	static void Bury(AWRCreature* Self, UWRGraveBody* Body);
	/** Server: a buried hound comes up (unhidden, its Rising row next). */
	static void Unbury(AWRCreature* Self, UWRGraveBody* Body);
	static void StartHold(AWRCreature* Self, UWRGraveBody* Body, AWRCharacterBase* Victim, FName Kind, float Seconds);
	static void Release(AWRCreature* Self, UWRGraveBody* Body, bool bThrow);
	/** A player's button press while held (mash): the hold ends sooner. */
	static void Mash(AWRCharacterBase* Victim);
	/** The body holding this one, if any (every machine). */
	static AWRCreature* HolderOf(const AActor* Victim);
};

/** The Graveyard's living world (server): the catacombs' dens, buried dens by day, the beats at dusk and night. */
UCLASS()
class WILDREACH_API UWRGraveLife : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UWRGraveLife* Get(const UObject* WorldContext);
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UWRGraveLife, STATGROUP_Tickables); }
	virtual bool IsTickable() const override;
	/** Server: a creature died on the Graveyard: the crows come to it. */
	void NoteDeath(AWRCreature* Dead, AActor* Killer);

private:
	void PlaceDens();
	void BeatDusk(float Hour);
	void BeatMist();
	void BeatMoths();
	void BeatTombbacks();
	bool bDens = false;
	float Clock = 0.f;
	double NextMistAt = 0.0;
	struct FFuneral { FVector At = FVector::ZeroVector; TWeakObjectPtr<AActor> Killer; double Until = 0.0; };
	TArray<FFuneral> Funerals;
};
