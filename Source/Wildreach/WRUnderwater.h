// Underwater play (cooks/biome-ocean.md 2.6, cooks/build-biome-ocean.md 2.7): breath, depth and crushing depth for
// survivors, and the rules every weapon, bow and spell reads under the surface. Shared by every water in the game (the
// Ocean's tide, the open sea, the Pound, the Swamp's floods): "under" is "the head is inside the highest-priority
// physics volume there and that volume is water".
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WRUnderwater.generated.h"

class ACharacter;
class APhysicsVolume;
class AWRCharacterBase;
struct FWRHit;
class UExponentialHeightFogComponent;

namespace WRUnderwater
{
	/** The water volume a point is in (the highest priority physics volume containing it, if it's water), or null. Air
	 *  pockets (non-water volumes of a higher priority) answer null. */
	WILDREACH_API APhysicsVolume* WaterAt(const UWorld* World, const FVector& Point);
	/** The surface Z (cm) of that water over the point (its brush's top at the point), or -BIG when not in water. */
	WILDREACH_API float SurfaceZ(const APhysicsVolume* Water, const FVector& Point);
	/** The character's head is under water (server and owner: the dive component's replicated flag; elsewhere worked out). */
	WILDREACH_API bool IsUnder(const AActor* Who);
	/** Swimming with the capsule wholly under the surface (CharacterMovement's immersion): the movement rules' "under". */
	WILDREACH_API bool IsSubmerged(const ACharacter* Who);
	/** Melee play rate under water for this weapon class (thrust classes and Gills 1, others underwater.meleePlayRate). */
	WILDREACH_API float MeleeRate(const AWRCharacterBase* Who, FName WeaponClass);
	/** May this weapon class charge a heavy under water (not greatswords, great hammers, axes)? */
	WILDREACH_API bool CanChargeHeavy(const AWRCharacterBase* Who, FName WeaponClass);
	/** Guard stamina x under water (no footing; Gills 1). */
	WILDREACH_API float GuardStaminaMult(const AWRCharacterBase* Who);
	/** Fire damage x under water (fizzles), and whether burn builds at all. */
	WILDREACH_API float FireMult(const AActor* Target);
	/** A spell bolt's speed x under water (Tide's own 1). */
	WILDREACH_API float BoltSpeedMult(const AActor* Caster, FName PathId);
	/** Gills (the Tide root passive, main path): no breath limit, crush at 80 m, x1.25 speed, no underwater penalties. */
	WILDREACH_API bool HasGills(const AActor* Who);
	/** Server: a lightning blow on someone in water spreads to everyone within the spread radius in the same water
	 *  (caster too, unless Grounded). Called by ApplyHit after a lightning hit lands. */
	WILDREACH_API void SpreadLightning(AWRCharacterBase* Target, const FWRHit& Hit);
	/** Every drawing machine, from the sky's tick: while the local head is under water the height fog thickens to the
	 *  water's murk (visibility by band: 40 m in the shallows and kelp, 25 m on the reef face, 12 m past the drop; x0.5 in
	 *  Sea fog), blue-green, flat (no height falloff); it eases back out when you surface. */
	WILDREACH_API void ApplyFog(UExponentialHeightFogComponent* Fog, UWorld* World, float Dt);
}

/**
 * Breath and depth on a survivor (cooks/biome-ocean.md 2.6 / 2.8). The server ticks it ten times a second: under or not,
 * depth below the surface, breath draining under and refilling at the surface or in an air pocket, drowning (10 a
 * second through ApplyHit, no floor) and crushing depth (5 a second below the limit). The owner gets breath, its max,
 * depth and the crush limit to draw the arc round the crosshair; everyone gets "under" (the gills, the bubbles, sounds).
 */
UCLASS(ClassGroup=(Wildreach), meta=(BlueprintSpawnableComponent))
class WILDREACH_API UWRDiveComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWRDiveComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	static UWRDiveComponent* Of(const AActor* Who);

	bool IsUnder() const { return bUnder; }
	float GetBreath() const { return Breath; }
	float GetMaxBreath() const { return MaxBreath; }
	/** cm below the surface (the head), 0 at or above it. */
	float GetDepth() const { return Depth; }
	/** cm below which the water crushes (a huge number with no limit). */
	float GetCrushDepth() const { return CrushDepth; }
	bool HasNoBreathLimit() const { return bNoLimit; }
	/** Seconds since the last drowning or crushing tick landed (the HUD darkens its edges for a moment), owner only. */
	float SinceHurt() const;
	/** Server: the max breath (s) this survivor has right now (Swimming level, perks, gear, brews, the path). */
	static float MaxBreathFor(const AActor* Who);
	/** Server: the depth (cm) the water starts to crush this survivor at (huge when nothing crushes). */
	static float CrushDepthFor(const AActor* Who);
	/** Server: hold the breath where it is for a while (a thrash-drag or a swallow: no refill while held). */
	void HoldBreath(float Seconds);
	/** Server: top the breath up (an air pocket's bubble, a quest). */
	void Refill() { Breath = MaxBreath; }

private:
	void ServerTick(float Dt);
	void LocalTick(float Dt);

	UPROPERTY(Replicated) bool bUnder = false;
	UPROPERTY(Replicated) float Breath = 30.f;
	UPROPERTY(Replicated) float MaxBreath = 30.f;
	UPROPERTY(Replicated) float Depth = 0.f;
	UPROPERTY(Replicated) float CrushDepth = 2500.f;
	UPROPERTY(Replicated) bool bNoLimit = false;
	UPROPERTY(ReplicatedUsing=OnRep_Hurt) uint8 HurtCount = 0;
	UFUNCTION() void OnRep_Hurt();

	float Accum = 0.f;
	float DrownClock = 0.f, CrushClock = 0.f;
	double HeldUntil = 0.0;
	double LastHurtAt = -100.0;
	// Local (owner and everyone who sees it): sounds and bubbles.
	bool bWasUnder = false;
	float HeartClock = 0.f, BubbleClock = 0.f;
	bool bPopped = false, bHumming = false;
};
