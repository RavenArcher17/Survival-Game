#pragma once

// The Fungal caverns' ten creatures and the bosses' Sporelings (cooks/biome-fungal.md 2.12; cooks/build-biome-fungal.md
// 2.12): what they do beyond their attack rows (Data/attacks.json) and how they look on their borrowed rigs.
//
//  - UWRFungalBody: a replicated component the server adds to every Fungal creature (creatures.json biome "fungal").
//    It carries what every machine draws from (flags: on a ceiling, a mound, latched, holding, tucked, crest up, gills
//    flared; who it is latched on or holding) and, on machines that draw, the look: the cook's palette over the rig's
//    texture (M_CreatureTint) and the parts that make the silhouette, modelled in code in M_FungalGlow (vertex colour,
//    alpha the glow, so the cyan tips glow on the caverns' clock): the Sporestalker's crest of stalks and gill ruff,
//    the Rotmaw's mound of caps and its glowing lure, the Mycomite's cap, the Webspinner's eight long banded legs, eyes
//    and dotted abdomen, the Tunnel toad's throat sac, the Shroomback's oyster garden and club, the Truffle mole's star
//    nose, the Glowgrub's lantern, the Capling's spotted cap, the Puffball's whole floating sac and tendrils.
//  - FWRFungalAI: the hooks AWRCreatureAI and the combat code call (ceiling drops, the mound and its lure, latches, the
//    lure snap's hold, the web net, the stalk volley, the croak, the leap, the belch, the reactions, the warm light
//    that draws Mycomites, the mole's sense and knocks, the toad's charged leap, the living-world beats, the dens).

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WRFungalCreatures.generated.h"

class AWRCreature;
class AWRCreatureAI;
class AWRCharacterBase;
class UProceduralMeshComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;
struct FWRAttackRow;
struct FWRHit;
struct FWRToolMesh;
enum class EWRHitResult : uint8;

namespace WRFungalFlag
{
	enum : uint8
	{
		Ceiling = 1 << 0,   // a Sporestalker or Webspinner up on a cave ceiling, waiting
		Mound   = 1 << 1,   // a Rotmaw settled into its mound, the lure out
		Latched = 1 << 2,   // a Mycomite clamped on someone
		Holding = 1 << 3,   // a Rotmaw with someone in its beak (the lure snap)
		Tucked  = 1 << 4,   // a Shroomback withdrawn into its shell (damage x0.2, strike x1.5)
		Crest   = 1 << 5,   // a Sporestalker's crest locked upright (the stalk volley's tell)
		Flared  = 1 << 6,   // a Sporestalker's gill ruff open (the gill bite's tell), a toad's sac swollen
		Riding  = 1 << 7,   // a Capling riding a Shroomback's shell
	};
}

UCLASS(ClassGroup=(Creature))
class WILDREACH_API UWRFungalBody : public UActorComponent
{
	GENERATED_BODY()

public:
	UWRFungalBody();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	static UWRFungalBody* Get(const AActor* Actor);
	static bool IsFungalKind(FName CreatureId);
	/** Server: the component on a new Fungal creature (AWRCreature::BeginPlay). */
	static void Attach(AWRCreature* Creature);
	/** Every live body (every machine). */
	static const TArray<TWeakObjectPtr<UWRFungalBody>>& All();
	/** Mycomites latched on this survivor now (every machine: their speed x0.9 each). */
	static int32 LatchedOn(const AActor* Victim);
	/** The Rotmaw holding this survivor, if any. */
	static AWRCreature* HolderOf(const AActor* Victim);

	UPROPERTY(ReplicatedUsing=OnRep_Flags) uint8 Flags = 0;
	UPROPERTY(Replicated) TObjectPtr<AActor> Victim;
	bool Has(uint8 F) const { return (Flags & F) != 0; }
	void Set(uint8 F, bool bOn);
	/** Where its lure is (a Rotmaw's: before its beak), every machine. */
	FVector LurePoint() const;

	// Server state.
	double Until[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };   // by flag bit: when each timed flag clears
	FVector CeilingAt = FVector::ZeroVector;
	bool bDropping = false, bLeaping = false, bRideLeap = false;
	float FallFromZ = 0.f;
	double HoldTickAt = 0.0, NextShakeAt = 0.0, NextPuffAt = 0.0, NextSenseAt = 0.0, NextBeatAt = 0.0, MoundAt = 0.0, NextClimbAt = 0.0;
	double DamageWindowAt = 0.0, StanceWindowAt = 0.0;
	float DamageWindow = 0.f, StanceWindow = 0.f;
	double LatchTickAt = 0.0;
	FString LureStruckBy;
	TWeakObjectPtr<AActor> RideOn;
	TWeakObjectPtr<AActor> Lifting;

private:
	UFUNCTION() void OnRep_Flags();
	void BuildLook();
	void ApplyTint();
	void TickLook(float Dt);
	UProceduralMeshComponent* Part(FName Bone, const FVector& Local, FWRToolMesh& M, bool bGlow);
	FName BoneLike(const TCHAR* Hint) const;
	UPROPERTY(Transient) TArray<TObjectPtr<UProceduralMeshComponent>> Parts;
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Crest;
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Ruff;
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Mound;
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Lure;
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Sac;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> Lamp;
	struct FLeg { TWeakObjectPtr<UProceduralMeshComponent> Upper, Lower; float Angle = 0.f; float Phase = 0.f; };
	TArray<FLeg> Legs;
	bool bBuilt = false;
	int32 Tries = 0;
	float LookTime = 0.f, CrestT = 0.f, RuffT = 0.f, MoundT = 0.f;
	float Stride = 0.f;
	static TArray<TWeakObjectPtr<UWRFungalBody>> Live;
};

/** The hooks AWRCreatureAI and the combat code call for Fungal creatures (friend of AWRCreatureAI). */
struct WILDREACH_API FWRFungalAI
{
	static bool TickWild(AWRCreatureAI* AI, AWRCreature* Self, float DeltaSeconds);
	static bool TickTame(AWRCreatureAI* AI, AWRCreature* Self, float DeltaSeconds);
	static bool RowAllowed(const AWRCreatureAI* AI, const AWRCreature* Self, const FWRAttackRow& Row, const AWRCharacterBase* Target, bool bReading);
	static void RowStarted(AWRCreatureAI* AI, AWRCreature* Self, const FWRAttackRow& Row, AWRCharacterBase* Target);
	/** A row's strike it takes over (a drop, the volley, the net, the croak, the leap, the belch): true when it did. */
	static bool StrikeRow(AWRCreatureAI* AI, AWRCreature* Self, const FWRAttackRow& Row, AWRCharacterBase* Target);
	static void Resolved(AWRCreature* Self, AWRCharacterBase* Victim, FName RowId, EWRHitResult Result);
	static float Incoming(AWRCreature* Self, const FWRHit& Hit, float Damage);
	static void Damaged(AWRCreature* Self, const FWRHit& Hit, float Damage);
	static void Died(AWRCreature* Self, AActor* Killer);
	/** Server, every body tick: drops and leaps landing, holds and latches, the toad's ridden leap. */
	static void TickBody(AWRCreature* Self, UWRFungalBody* Body, float DeltaTime);
	/** A survivor's button press while webbed or in a beak (mash): free sooner. True when it was one. */
	static bool Mash(AWRCharacterBase* Victim);
	/** Server: web a survivor (a Webspinner's net, the Weaver's): rooted, can still swing; mash frees sooner. */
	static void Web(AWRCharacterBase* Victim, float Seconds, AActor* By);
	/** Movement x on a survivor for the latched Mycomites (0.9 each), every machine. */
	static float MoveScale(const AActor* Who);
	/** Server: a Sporeling (the bosses' swarm) at At. */
	static AWRCreature* SpawnSporeling(UWorld* World, const FVector& At, AActor* Toward);
	/** Server: a Sporestalker or Webspinner climbs to the ceiling over it (a cave hall's roof within 12 m). */
	static bool Climb(AWRCreature* Self, UWRFungalBody* Body);
	/** Server: a Rotmaw settles into its mound. */
	static void Settle(AWRCreature* Self, UWRFungalBody* Body);
};
