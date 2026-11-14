#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "WRCombatTypes.h"
#include "WRLag.generated.h"

class AWRCharacterBase;
class UAbilitySystemComponent;

/**
 * The server's memory of the last second of every fight (COMBAT-AND-LESSONS 1.17, cooks/build-combat-core.md 3.4):
 *  - bodies: every character's capsule sampled at net.sampleHz, so a player's swing and crit are judged against where
 *    that player saw its target (its one-way ping + the interpolation delay, at most net.rewindMax);
 *  - defences: when each character's guard, parry window, i-frames and jump went up and down; a remote player's are
 *    back-dated by its one-way ping (it pressed that long before the server heard);
 *  - held blows: a creature's or boss's blow on a remote player that isn't defended right now waits for the defence it
 *    may already have pressed on its screen, then resolves at the moment that player saw it land.
 * Server only (nothing is created on clients).
 */
UCLASS()
class WILDREACH_API UWRLagSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UWRLagSubsystem* Get(const UObject* WorldContext);

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableInEditor() const override { return false; }

	/** One-way delay to a player's machine (0 for creatures and the host's own pawn), seconds. */
	static float OneWay(const AWRCharacterBase* Who);
	/** How far behind the server a player sees other bodies: one-way + interpolation, capped at net.rewindMax. */
	static float SeenDelay(const AWRCharacterBase* Viewer);
	/** Server leeway when a remote player's press arrives against a window the server opened later. */
	static float WindowTolerance(const AWRCharacterBase* Who);

	/** Where Who's capsule was at Time (centre, radius, half height); false if there's no history (use the present). */
	bool BodyAt(const AWRCharacterBase* Who, double Time, FVector& OutCentre, float& OutRadius, float& OutHalfHeight, FRotator* OutRotation = nullptr) const;
	/** Was Who defended by Tag (State.Invulnerable / Parrying / Blocking / JumpLegsInvuln) at Time (back-dated history)? */
	bool WasDefended(const AWRCharacterBase* Who, const FGameplayTag& Tag, double Time) const;
	/** When Who's current defence of this kind began (back-dated), or < 0. */
	double DefenceStart(const AWRCharacterBase* Who, const FGameplayTag& Tag, double AtTime) const;

	/** Should this blow wait for the target's defence? (A remote player, not defended now, hit by a creature, boss or hazard.) */
	bool ShouldHold(const AWRCharacterBase* Target, const FWRHit& Hit) const;
	/** Hold it; it resolves through UWRCombatLibrary::ApplyHit later and calls OnResolved with what happened. */
	void Hold(AWRCharacterBase* Target, const FWRHit& Hit, TFunction<void(EWRHitResult)> OnResolved);

private:
	struct FSample { double Time = 0.0; FVector Centre = FVector::ZeroVector; FRotator Rotation = FRotator::ZeroRotator; float Radius = 40.f; float HalfHeight = 90.f; };
	struct FSpan { FGameplayTag Tag; double Start = 0.0; double End = -1.0; };   // End < 0: still up
	struct FTrack
	{
		TArray<FSample> Samples;     // oldest first
		TArray<FSpan> Defences;      // oldest first
		TWeakObjectPtr<UAbilitySystemComponent> Watched;
		TArray<FDelegateHandle> Handles;
	};
	struct FHeld
	{
		TWeakObjectPtr<AWRCharacterBase> Target;
		FWRHit Hit;
		double ResolveAt = 0.0;
		TFunction<void(EWRHitResult)> OnResolved;
	};

	void Watch(AWRCharacterBase* Who, FTrack& Track);
	void OnDefenceChanged(TWeakObjectPtr<AWRCharacterBase> Who, FGameplayTag Tag, int32 Count);

	TMap<TWeakObjectPtr<AWRCharacterBase>, FTrack> Tracks;
	TArray<FHeld> Held;
	double NextSample = 0.0;
};
