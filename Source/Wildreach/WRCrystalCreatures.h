#pragma once

// The Crystal caves' nine creatures (cooks/biome-crystal.md 2.12; cooks/build-biome-crystal.md 2.14): what they do beyond
// their attack rows (Data/attacks.json) and how they look on their borrowed rigs.
//
//  - UWRCrystalBody: a replicated component the server adds to every Crystal creature (creatures.json biome "crystal").
//    It carries what every machine draws from (flags: rolled, deaf, dim, plates turned, hunting; which legs are broken)
//    and, on machines that draw, the look: the cook's palette over the rig's texture (M_CreatureTint) and the parts that
//    make the silhouette, modelled point by point in M_CrystalGlow (so they light by sound like the halls): the
//    Prismback's rows of mirror-plates, the Spindleglass's eight banded legs (feet planted, each stepping when the body
//    has moved 1.6 m past it) under a body hung 3 m up, the Echobat's ears and leaf-nose, the Geodemite's honeypot, the
//    Lumenhorn's antler spread and its rider light, the Druseback's split-geode segments, the Gemsniffer's brow gem, the
//    Alicanto's glowing wing fans, the Chimecrab's geode shell.
//  - FWRCrystalAI: the hooks AWRCreatureAI calls (hearing hunters, plate turns and reflection, the glare, leg breaks,
//    rolls, latches, bursts, the herd's bellow, the Alicanto's dim-and-run, the Gemsniffer's sense, the ore lead).

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WRCrystalCreatures.generated.h"

class AWRCreature;
class AWRCreatureAI;
class AWRCharacterBase;
class UProceduralMeshComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;
struct FWRAttackRow;
struct FWRHit;
enum class EWRHitResult : uint8;

namespace WRCrystalFlag
{
	enum : uint8
	{
		Rolled  = 1 << 0,   // a Druseback curled into its ball (damage x0.3)
		Deaf    = 1 << 1,   // an Echobat deafened by a Resonance peal (it drops to roost, its clicks stop)
		Dim     = 1 << 2,   // an Alicanto that has seen you following: its glow goes out and it runs
		Plates  = 1 << 3,   // a Prismback's plated flank swung to a bow draw or a cast: it reflects
		Hunting = 1 << 4,   // a Spindleglass whose body clouds pink-grey (it has felt your steps)
		Latched = 1 << 5,   // a Geodemite clamped on someone
	};
}

UCLASS(ClassGroup=(Creature))
class WILDREACH_API UWRCrystalBody : public UActorComponent
{
	GENERATED_BODY()

public:
	UWRCrystalBody();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	static UWRCrystalBody* Get(const AActor* Actor);
	static bool IsCrystalKind(FName CreatureId);
	/** Server: the component on a new Crystal creature (AWRCreature::BeginPlay). */
	static void Attach(AWRCreature* Creature);

	UPROPERTY(ReplicatedUsing=OnRep_Flags) uint8 Flags = 0;
	/** Spindleglass: which of its eight legs are broken (they never regrow in its life). */
	UPROPERTY(ReplicatedUsing=OnRep_Flags) uint8 LegsBroken = 0;
	UPROPERTY(Replicated) TObjectPtr<AActor> LatchedOn;
	/** A Prismback being warded by an Arcane walker's path magic (crystal 2.12.1): who, since when, where their hand
	 *  went on (moving off it breaks the ward). Replicated so the prompt shows on the warder's machine. */
	UPROPERTY(Replicated) FString PathTamer;
	double PathTameStart = 0.0;
	FVector HandAt = FVector::ZeroVector;
	bool Has(uint8 F) const { return (Flags & F) != 0; }
	/** The looks that follow the flags and the broken legs, now (the server after it changes them). */
	void Refresh() { OnRep_Flags(); }
	void Set(uint8 F, bool bOn);

	// Server state.
	double Until[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };   // by flag bit: when each timed flag clears
	float LegHP[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	double NextBellowAt = 0.0, NextSenseAt = 0.0, StillSince = 0.0, LeadUntil = 0.0;
	FVector OwnerWas = FVector::ZeroVector;
	FDelegateHandle Hearing;
	TWeakObjectPtr<AActor> HeardFrom;
	double HeardAt = -100.0;

private:
	UFUNCTION() void OnRep_Flags();
	void BuildLook();
	void ApplyTint();
	void TickLegs(float Dt);
	UProceduralMeshComponent* Part(FName Bone, const FTransform& Local, struct FWRToolMesh& M, bool bGlow);
	FName BoneLike(const TCHAR* Hint) const;
	UPROPERTY(Transient) TArray<TObjectPtr<UProceduralMeshComponent>> Parts;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> Glows;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> Lamp;
	// The Spindleglass's legs: an upper and a lower segment each, the foot planted in the world until it steps.
	struct FLeg { TWeakObjectPtr<UProceduralMeshComponent> Upper, Lower; FVector Foot = FVector::ZeroVector, From = FVector::ZeroVector, To = FVector::ZeroVector; float StepT = 1.f; float Angle = 0.f; };
	TArray<FLeg> Legs;
	bool bBuilt = false;
	int32 Tries = 0;
	float BodyLift = 0.f;
};

/** The hooks AWRCreatureAI and the combat code call for Crystal creatures (friend of AWRCreatureAI). */
struct WILDREACH_API FWRCrystalAI
{
	static bool TickWild(AWRCreatureAI* AI, AWRCreature* Self, float DeltaSeconds);
	static bool TickTame(AWRCreatureAI* AI, AWRCreature* Self, float DeltaSeconds);
	static bool RowAllowed(const AWRCreatureAI* AI, const AWRCreature* Self, const FWRAttackRow& Row, const AWRCharacterBase* Target, bool bReading);
	static void Resolved(AWRCreature* Self, AWRCharacterBase* Victim, FName RowId, EWRHitResult Result);
	static float Incoming(AWRCreature* Self, const FWRHit& Hit, float Damage);
	static void Damaged(AWRCreature* Self, const FWRHit& Hit, float Damage);
	static void Died(AWRCreature* Self, AActor* Killer);
	/** Server: a noise the creature may hear (an Echobat hears everything; a Spindleglass only steps, rolls, landings). */
	static void Heard(AWRCreature* Self, const FVector& At, float Radius, AActor* By, FName Kind);

	// --- The Prismback's path magic (crystal 2.12.1): an Arcane walker with 10+ points holds Interact 4 s with 30 Mana
	// on a Prismback knocked out or under 30%: a Barrier wraps it, a full narcotic dose; taming goes on as knockout and feed.
	/** Every machine: the prompt for Who at Self (false: not open to it). */
	static bool PathTamePrompt(const AWRCreature* Self, const AActor* Who, FString& OutVerb);
	/** Server, E pressed (through AWRCreature::Feed): the hand goes on. True when it handled the press. */
	static bool TryPathTame(AWRCreature* Wild, class AWRPlayerController* Player, FString& OutResult);
	/** Server: E let go early ("world cpathtame"): the ward stops (a tap feeds it as usual). */
	static void StopPathTame(class AWRPlayerController* Player);
	/** The owning machine: this player has a hand on a Prismback (E's release must tell the server). */
	static bool PathTameNear(const class APlayerController* Player);
	/** Server: a tamed Lumenhorn's Bellow on order (the whistle's Call, or its rider's): a 40 m noise that calls every
	 *  Echobat hearing it and holds every crystal in range lit for 20 s (crystal 2.12.1). True when it bellowed. */
	static bool Bellow(AWRCreature* Lumenhorn, AActor* By);
};
