#pragma once

// The air (cooks/build-biome-sky.md 2.6-2.7, world-map 2.5, combat SPEC): gliding, fall damage, the one entry point
// for every push, gusts on their lanes and in a Gale, lee rotors and downbursts, and the edge-fall kill credit.
//
//  - Gliding is a predicted movement mode (UWRCharacterMovementComponent::CMOVE_Glide, PhysGlide): it opens on Jump
//    while falling with 2 m of air under you, carrying a glider (the best in the bag), the Wind path's Path Glide, or
//    the Skysilk set's four pieces; WRSkyAir::GlideParams answers the same on the server and the owning client.
//  - Gust lanes, Gale gusts and lee rotors are pure functions of the server time and where you stand (WindPushAt), so
//    the server and the owning client move you the same and nothing corrects; their tells (whistle, grass wave, cloth
//    snap, Mistlarks scattering, 1 cm of shake) come from the same function on every machine (WRSkyHazards).
//  - Every other push (a creature's wing, a boss's breath) goes through Push on the server: brace, anchor boots,
//    Skysilk 4, a roll's i-frames and a sky-born creature's resistance all checked once, the pusher noted for the
//    edge credit.
//  - Fall damage is game-wide (combat SPEC): from the highest point since you last stood, none under 6 m, lethal at
//    18 m, through ApplyHit. Creatures that fall 18 m die, credited to whoever hit or pushed them in the last 6 s.

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRSkyAir.generated.h"

class ACharacter;
class AWRCharacterBase;
class UProceduralMeshComponent;

/** What a glide is made of (world-map 2.5's table; tuning glide.<kind>). */
struct FWRGlideParams
{
	FName Kind;                    // crude_glider, levistone_glider, path, skysilk
	float Forward = 800.f;         // cm/s
	float Sink = 260.f;            // cm/s
	float StaminaPerSecond = 5.f;  // 0 in a column, halved by Featherfall
	float Drift = 0.15f;           // x the wind
	float MaxSeconds = 0.f;        // Skysilk 4's 6 s (0: no limit)
	float TurnRate = 120.f;        // deg/s
	bool bFeatherfall = false;
};

namespace WRSkyAir
{
	/** Whether this character can glide now and with what (the best glider in the bag with durability, else Path Glide,
	 *  else Skysilk 4). Pure: the server and the owning client agree (the bag and the worn set replicate to the owner). */
	WILDREACH_API bool GlideParams(const ACharacter* Who, FWRGlideParams& Out);
	WILDREACH_API bool IsGliding(const ACharacter* Who);
	/** Stamina a second the glide costs now (0 in an updraft), or -1 when not gliding (UWRStatusComponent's drain). */
	WILDREACH_API float GlideDrain(const ACharacter* Who);
	/** The Wind path's Featherfall (the root): no fall damage, half the glider's stamina. */
	WILDREACH_API bool HasFeatherfall(const AActor* Who);
	/** How much of a push lands on Who: 0 immune (anchor boots, Skysilk 4, a roll's i-frames, a sky-born creature,
	 *  a gust-proof tame), 0.5 braced (crouching or holding a guard, not swinging), else 1. */
	WILDREACH_API float PushScale(const ACharacter* Who, FName Source);
	/** Server: push Who (VelocityCmPerS for Seconds) for Source, noting Pusher for the edge credit. A glider is shoved
	 *  without being closed; anyone else is launched. */
	WILDREACH_API void Push(ACharacter* Who, const FVector& VelocityCmPerS, float Seconds, FName Source, AActor* Pusher);
	/** Server: Who was hit (or pushed) by By: the edge credit's 6 s window starts; a glider closes. */
	WILDREACH_API void NoteHit(AActor* Who, AActor* By);
	/** Who gets the kill for Victim's fall now (hit or pushed it within the window), or null. */
	WILDREACH_API AActor* FallCredit(const AActor* Victim);

	/** A gust over Pos now: Phase < 0 is the tell (seconds before it bites), 0..Seconds the push; Dir the way it shoves. */
	struct FGust
	{
		bool bOn = false;
		float Phase = -100.f;
		float Seconds = 0.8f;
		FVector2D Dir = FVector2D(-1.f, 0.f);
		int32 Lane = -1;          // a lane's index, or -1 for a Gale gust / a rotor
		bool bRotor = false;
		FVector From = FVector::ZeroVector;   // where the tell comes from (upwind along the lane)
	};
	/** The gust (lane, Gale, lee rotor on foot) over Pos at the server time (every machine computes the same). */
	WILDREACH_API FGust GustAt(const UObject* WorldContext, const FVector& Pos, bool bOnFoot);
	/** The push velocity the air puts on Who now (cm/s; the gust x its PushScale), for the movement component. */
	WILDREACH_API FVector WindPushAt(const ACharacter* Who, const FVector& Pos, bool bOnFoot);
}

/**
 * The glider you see: folded on the back while carried, spread over you while gliding (eased open over 0.25 s), built
 * in code to the glider's kind: a crude wing of bone-white spars and stitched hide; a levistone wing of cloudpine spars,
 * pale skysilk panels and floating blue stones in its spar joints (cyan at night); the Path Glide's and Skysilk's are
 * the wind itself (a ribbon of light streaming off the shoulders). Lives on every machine (WRSkyAir's subsystem keeps
 * one per survivor from the replicated GliderKind and movement mode).
 */
UCLASS(ClassGroup=(Wildreach))
class WILDREACH_API UWRGliderLook : public USceneComponent
{
	GENERATED_BODY()

public:
	UWRGliderLook();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	void Build(uint8 Kind);
	uint8 BuiltKind = 0;

private:
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Folded;
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Open;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Trail;
	UPROPERTY(Transient) TObjectPtr<class UAudioComponent> Flap;
	float Spread = 0.f;
	float FlapClock = 0.f;
};

/** Runs the air on the server (fall damage, glider wear, creatures' edge falls) and keeps every survivor's glider look. */
UCLASS()
class WILDREACH_API UWRSkyAirSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UWRSkyAirSubsystem, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return !IsTemplate(); }

private:
	struct FFall { float TopZ = 0.f; bool bAir = false; float GlideClosedAt = -100.f; float GlideClosedZ = 0.f; bool bWasGliding = false; bool bInLift = false; float AirSince = 0.f; };
	TMap<TWeakObjectPtr<ACharacter>, FFall> Falls;
	/** Yara Kesh's deeds (cooks/biome-sky.md 2.13): the island last stood on and whether the air since has been clean
	 *  (First flight: no ground, no mount), the columns ridden to their tops (Read the wind), the Broken Way's crossing
	 *  (from Tollgate with the Toll Shrine lit to Brink, no lift, no mount). */
	struct FDeeds { FName Stood; bool bClean = false; TSet<int32> Tops; bool bCrossing = false; };
	TMap<TWeakObjectPtr<ACharacter>, FDeeds> Deeds;
	void StepDeeds(class AWRCharacter* Who, bool bStanding, bool bGlide, bool bHeld);
	float WearClock = 0.f;
	float LookClock = 0.f;
	void StepFalls(float Dt);
	void StepLooks();
	void Landed(ACharacter* Who, FFall& F, float Drop);
};
