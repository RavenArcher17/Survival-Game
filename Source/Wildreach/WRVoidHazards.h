#pragma once

// The Corrupted wasteland's two ground hazards and its biome event (cooks/biome-void.md 2.8, 2.10):
//  - litter fire (V1): fire on the Withered Forest's dry litter smoulders 0.9 s, then a burn front walks downwind at
//    1.5 m/s (upwind 0.5) over 6 m cells; each burns 60 s (3 a second, burn 8 a second to whoever stands in it) and
//    leaves black ground that won't burn for 2 game days;
//  - hollow crust (V4): thin crust over rift pockets that drums underfoot within 6 m and breaks under a heavy landing, a
//    roll, a heavy blow, a heavy creature or a thrown stone (you drop 4-8 m into the pocket);
//  - the Rift Storm (world.json events.riftstorm): 6 of the 10 seeds open as storm rifts with a rift heart (1,500 HP;
//    void x0.5, anchor salt -300), three waves 4 min apart walk to the nearest claim within 1.5 km (else the nearest
//    shrine); closed rings pay voidglass x3; 1,500 path XP a closed rift, split by heart damage.
// One replicated state actor per land (AWRVoidState); the server decides, every machine draws from the replicated rows.

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WRVoidHazards.generated.h"

class AWRLand;
class AWRCharacterBase;
class AWRCreature;
class AWRRift;
class UStaticMeshComponent;

struct FWRHit;

USTRUCT()
struct FWRFireCell
{
	GENERATED_BODY()
	UPROPERTY() int32 X = 0;
	UPROPERTY() int32 Y = 0;
	/** Server time it caught (smoulder until +0.9 s, then flame until +60 s). */
	UPROPERTY() float Lit = 0.f;
};

UCLASS(NotPlaceable)
class WILDREACH_API AWRVoidState : public AActor
{
	GENERATED_BODY()

public:
	AWRVoidState();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;
	static AWRVoidState* Find(const UWorld* World);
	/** Server: the land's state actor (WRVoidWorld::LandReady). */
	static void Spawn(AWRLand* Land);

	/** Server: fire touched the ground at At (a fire blow, a fire bolt's landing, a burning pool). */
	static void FireAt(const UObject* WorldContext, const FVector& At);
	/** Server: something heavy came down at At (a landing, a roll, a heavy blow, a thrown stone): crust there breaks. */
	static void CrustStruck(const UObject* WorldContext, const FVector& At, float Radius);
	/** A burning cell covers this point (creatures flee it). */
	static bool Burning(const UWorld* World, const FVector& At);
	/** Server: a blow on a rift heart (who dealt what: the XP split). */
	void NoteHeart(AWRCreature* Heart, const FWRHit& Hit, float Damage);
	/** Server: a rift heart broke: its rift closes, its ring pays x3, the XP is shared. */
	static void HeartDied(AWRCreature* Heart);
	/** The Rift Storm is on / its seeds (for the Driftmote and the warning). */
	bool StormOn() const { return StormStage == 2; }

	UPROPERTY(Replicated) TArray<FWRFireCell> Fire;
	UPROPERTY(Replicated) uint32 CrustBroken = 0;
	/** 0 none, 1 the warning, 2 on. */
	UPROPERTY(Replicated) uint8 StormStage = 0;
	FName LandId;

protected:
	virtual void BeginPlay() override;

private:
	void TickFire(float Dt);
	void TickCrust(float Dt);
	void TickStorm(float Dt);
	void DrawFire();
	void BuildCrust();
	void OpenStorm();
	void CloseStorm();
	void Wave(int32 Rift, int32 N);
	double Now() const;
	FIntPoint CellOf(const FVector2D& M) const;
	FVector CellCentre(const FIntPoint& C) const;
	bool Burnable(const FIntPoint& C) const;

	// Server: when each cell burned out (it won't burn again for 2 game days).
	TMap<FIntPoint, double> BurntAt;
	float FireDamageClock = 0.f;
	// Every machine: the flames drawn per cell.
	TMap<FIntPoint, TWeakObjectPtr<USceneComponent>> Flames;
	TMap<FIntPoint, uint8> FlameState;
	// The crust: patches (map centre, radius), their discs and the drum sound loop near you.
	struct FCrust { FVector2D At; float R = 8.f; float Depth = 5.f; bool bRift = false; double BrokenAt = 0.0; TWeakObjectPtr<UStaticMeshComponent> Disc; };
	TArray<FCrust> Crusts;
	float DrumClock = 0.f;
	// The Rift Storm (server).
	struct FStormRift { TWeakObjectPtr<AWRRift> Rift; TWeakObjectPtr<AWRCreature> Heart; FVector At = FVector::ZeroVector; int32 WavesSent = 0; double NextWave = 0.0; TArray<TWeakObjectPtr<AWRCreature>> Live; TMap<FString, float> Dealt; bool bClosed = false; };
	TArray<FStormRift> Storm;
	bool bStormOpened = false;
};
