// The Void's one currency of pain and its tears (cooks/biome-void.md 2.6, 2.8; cooks/build-biome-void.md 2.6): max health
// taken into MaxHealthLost (rifts, Gnawers, Vorhollow, your own void abilities), given back by Rest; and the rifts
// themselves: tears with a drain zone, a pull, a touch that burns, a pulse that drops their floating stones, a ring of
// voidglass. Server authority: drains, pulls' damage and pulses are the server's; the pull's movement is computed the
// same on the server and the owning client (a pure function of position), so it never corrects.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WRVoid.generated.h"

class AWRCharacterBase;
class AWRPlayerState;
class UProceduralMeshComponent;
struct FWRItemDef;

namespace WRVoid
{
	/** Server: the Void takes Amount max health (into MaxHealthLost). At the end of it you are Unmade (no floor). */
	WILDREACH_API void LoseMaxHealth(AWRCharacterBase* Who, float Amount, AActor* Source = nullptr, bool bQuiet = false);
	/** Server: give back max health the Void took (Consume, Feed, the Void heart, Rest gives all of it). */
	WILDREACH_API void RestoreMaxHealth(AWRCharacterBase* Who, float Amount);
	/** How much of a rift's drain reaches this player: 0 when Empty vessel passes it; nullward / Hollowplate halve it
	 *  (gear riftDrain), anchor salt halves it again (brew riftDrain). */
	WILDREACH_API float RiftDrainMult(const AWRCharacterBase* Who);
	/** A timed brew's stat on a player (anchor salt: riftDrain -0.5 for 5 min), or 0. */
	WILDREACH_API float BrewStat(const AActor* Who, FName Stat);
	/** Server: start an item's brew (items.json use.brew + use.minutes) on the drinker. Returns its line for the player. */
	WILDREACH_API FString Brew(AWRCharacterBase* Who, const FWRItemDef& Def);
	/** The rift pull at a point (cm/s), the same on every machine: toward the nearest tear inside its pull radius. */
	WILDREACH_API FVector PullAt(const UWorld* World, const FVector& Feet);
	/** Binds Rest (once). */
	WILDREACH_API void Start(UWorld* World);
	/** Server: the land's stable rifts (and the kit pass's small ones) once the land's places are known. */
	WILDREACH_API void SpawnRifts(UWorld* World, FName LandId);
	/** A rift zone covers this point (claims can't be placed there: "The ground won't hold it"). */
	WILDREACH_API bool InRiftZone(const UWorld* World, const FVector& At);
}

/** A tear in the air whose bottom tip touches the ground, ringed with the voidglass it fused. */
UCLASS(NotPlaceable)
class WILDREACH_API AWRRift : public AActor
{
	GENERATED_BODY()

public:
	AWRRift();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(Replicated) FName RiftId;
	UPROPERTY(Replicated) bool bLarge = false;
	UPROPERTY(Replicated) bool bSteady = false;
	/** cm */
	UPROPERTY(Replicated) float ZoneRadius = 2500.f;
	UPROPERTY(Replicated) float DrainMult = 1.f;
	/** Server clock of the next pulse (the 1.0 s silence, then the stones drop for 2 s). */
	UPROPERTY(Replicated) double NextPulse = 0.0;
	/** The floating stones it holds up (look and the pulse's danger: the same seeds on every machine). */
	UPROPERTY(Replicated) int32 Rubble = 3;

	float TearHeight() const { return bLarge ? 1000.f : 420.f; }
	float TearWidth() const { return bLarge ? 260.f : 120.f; }
	/** The tear's middle (world). */
	FVector TearCentre() const { return GetActorLocation() + FVector(0.f, 0.f, TearHeight() * 0.5f); }
	/** Stone i's resting place (local) and size (cm). */
	void StoneSpot(int32 i, FVector& OutLocal, float& OutSize) const;
	/** How far down the stones are right now (0 floating, 1 on the ground): from the pulse clock. */
	float DropNow(double Now) const;

protected:
	virtual void BeginPlay() override;

private:
	void BuildLook();
	void ServerStep(float Dt);
	double Now() const;
	bool bBuilt = false;
	float StepAcc = 0.f;
	double LastPulseHit = 0.0;
	TMap<TWeakObjectPtr<AActor>, double> TouchedAt;
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> Stones;
	UPROPERTY() TObjectPtr<class UAudioComponent> Inrush;
	TArray<float> StoneBase;
};
