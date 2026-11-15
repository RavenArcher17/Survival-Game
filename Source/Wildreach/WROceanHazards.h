#pragma once

// The Ocean's hazards and its event (cooks/biome-ocean.md 2.8-2.10; cooks/build-biome-ocean.md 2.8-2.10):
//
//  - Rips (5 at the O1 bar gaps): an 8 x 120 m seaward lane, 250 cm/s on the falling tide, 170 rising. A pure function
//    of the tide and where you swim (CurrentAt), so the owner and the server push the same and it never corrects
//    (UWRCharacterMovementComponent). Straight against it the fast stroke costs x2 and makes no headway; out by its side
//    frees you (Hae-won's second quest checks the side). Tells: the surf strip breaks there, foam drifting out, a hiss.
//  - Blowholes (the shelf's fields and the Spouting Horn): live from mid-rising to high water, each on its own 9-16 s
//    beat: a 1.0 s tell (boom, hiss, spray puffs), then a 6 m column (40 m at the Horn): 40 and thrown 6 m up (18 m at the
//    Horn). Jump just before it and you ride it unhurt; the Horn's ride lands on secret O-S3's ledge.
//  - Surge channels: a wave runs up each channel at 600 cm/s every ~14 s (1 s tell): 30, a knockdown, dragged 6 m out.
//  - Man o' war fleets (two on O5, drifting with the wind; a Squall blows one onto the strand): blue-violet sails bobbing,
//    tentacles 8 m down. A touch: 25 and poison 50 (a swim mount or Reefmail 2: half). Under 8 m or round them is safe.
//  - Air pockets (the Pearl Cave, the Drowned Hall's dome and dry rooms): air volumes over the water (breath refills).
//  - The King Tide (world.json events.king_tide; AWROceanState): the warning (the sea draws back, every chime rings, the
//    drowned bell), the Low (-6 m: the flats, stranded grazers, pool-bound Reefjaws, crabs on the flats, the Low Gate),
//    the spawning night (20:00-06:00: every tidecoral regrows, Glowray rings tame x2), the Return (the white line and the
//    roar at T-10 min; the 3 m bore at T-3 min running to the Tsunami Line: 80, carried 20 m, soaked; pieces 200 once).

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WROceanHazards.generated.h"

class AWRCharacterBase;
class AWRTideWater;

UCLASS()
class WILDREACH_API UWROceanHazards : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;
	static UWROceanHazards* Get(const UObject* WorldContext);

	/** Every machine: the rip current (cm/s) on a swimmer at P; zero outside every rip. */
	static FVector CurrentAt(const UObject* WorldContext, const FVector& P);
	/** The rip at P (its index, how far along it and across it, cm), or false. */
	static bool InRip(const UObject* WorldContext, const FVector& P, int32& OutIndex, float& OutAlong, float& OutAcross);
	/** The fast stroke's stamina x (2 swimming straight against a rip). */
	static float RipStaminaMult(const AActor* Who);
	/** Where man o' war Index floats at server time T (every machine draws the same). */
	FVector ManAt(int32 Index, double T) const;
	/** Offer-food taming x for a creature (a King Tide's stranded grazer x3, a spawning night's Glowray x2). */
	static float TameSpeedMult(const class AWRCreature* Creature);
	/** The Tsunami Line's height (cm) over mean sea level. */
	static float TsunamiLineZ();

private:
	void Setup();
	void StepRips(float Dt);
	void StepBlowholes();
	void StepSurges();
	void StepFleets();
	void StepFleetLooks(float Dt);
	void StepKingTide(float Dt);
	void BoreHit(double T);
	void StartLow();
	void EndKingTide();

	bool bSetup = false;
	double ServerNow() const;
	struct FMan { FVector Base = FVector::ZeroVector; FVector Offset = FVector::ZeroVector; float Phase = 0.f; int32 Fleet = 0; };
	TArray<FMan> Men;
	TArray<TWeakObjectPtr<AActor>> ManLooks;
	TArray<TWeakObjectPtr<AActor>> RipLooks;
	UPROPERTY(Transient) TArray<TObjectPtr<AWRTideWater>> Pockets;
	struct FRipTrack { int32 Index = -1; float Along = 0.f; float Across = 0.f; };
	TMap<TWeakObjectPtr<AActor>, FRipTrack> InRips;
	TArray<double> BlowNext;       // server: the next column's time per blowhole
	TArray<bool> BlowTold;
	TArray<double> SurgeStart;     // server: the running wave's start per channel (< 0 none)
	TArray<double> SurgeNext;
	TArray<TSet<TWeakObjectPtr<AActor>>> SurgeHit;
	TMap<TWeakObjectPtr<AActor>, double> StungAt;
	float FleetClock = 0.f, RipClock = 0.f, SalvageClock = 0.f;
	void StepSalvager(float Dt);
	// The King Tide (server).
	uint8 KingStage = 0;           // 0 none, 1 warning, 2 the Low, 3 the Return's line, 4 the bore
	TSet<TWeakObjectPtr<AActor>> BoreHitSet;
	TSet<TWeakObjectPtr<AActor>> PiecesHit;
	TSet<FString> LineStood;
	TArray<TWeakObjectPtr<class AWRCreature>> KingSpawned;
};
