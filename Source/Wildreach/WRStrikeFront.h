#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WRCombatTypes.h"
#include "WRStrikeFront.generated.h"

class AWRCharacterBase;

/**
 * A blow that isn't a body: a shockwave, a split running through the ground, a bore, a ripple of sound, a limb sweeping
 * round (cooks/cookpass-combat.md 3.10). It travels and hits only where its front is, once per body, so it can be read,
 * jumped (ground fronts are low), rolled or outrun: Elden Ring's ground eruptions, Monster Hunter's slam waves.
 *
 * Server only (no replication): the look is fx rows broadcast along the front (WRFx::Broadcast), the hit goes through
 * the owner's callback (a boss's StrikeHit, a creature's RowHit) or, with no callback, the template FWRHit to ApplyHit.
 */
UCLASS()
class WILDREACH_API AWRStrikeFront : public AActor
{
	GENERATED_BODY()
public:
	AWRStrikeFront();

	using FOnHit = TFunction<void(AWRCharacterBase*)>;

	/** A ring rolling out from Centre to MaxRadius at Speed cm/s (Speed < 0: closing in from MaxRadius toward the centre),
	 *  Depth cm thick. */
	static AWRStrikeFront* Ring(AActor* Owner, const FVector& Centre, float MaxRadius, float Speed, float Depth, FOnHit OnHit, FName Fx, FName Sound, float Delay = 0.f);
	/** A head running along the ground from From to To at Speed, hitting within HalfWidth of it; EndRing > 0 bursts a ring
	 *  of that radius where it arrives. */
	static AWRStrikeFront* Line(AActor* Owner, const FVector& From, const FVector& To, float Speed, float HalfWidth, FOnHit OnHit, FName Fx, FName Sound, float EndRing = 0.f, float Delay = 0.f);
	/** A limb or tail sweeping round Centre out to Radius from FromYaw through Sweep degrees over Seconds. */
	static AWRStrikeFront* Arc(AActor* Owner, const FVector& Centre, float Radius, float FromYaw, float Sweep, float Seconds, FOnHit OnHit, FName Fx, FName Sound, float Delay = 0.f);

	/** The plain template hit (no owner callback): every body the front meets takes it (hostile to the template's
	 *  instigator if it has one, else every character but the owner). */
	static FOnHit HitWith(const FWRHit& Template);

	virtual void Tick(float DeltaSeconds) override;

	/** A ground front (the default) misses a body in the air over it; a front of light or sound (false) doesn't. */
	bool bGround = true;

private:
	enum class EKind : uint8 { Ring, Line, Arc };
	static AWRStrikeFront* Make(AActor* Owner, EKind Kind, const FVector& At, FOnHit OnHit, FName Fx, FName Sound, float Delay);
	void HitAround(const TFunction<bool(const AWRCharacterBase*)>& Inside);
	FVector Floor(const FVector& At) const;

	EKind Kind = EKind::Ring;
	TWeakObjectPtr<AActor> Source;
	FOnHit OnHit;
	FName Fx, Sound;
	FVector Centre = FVector::ZeroVector, To = FVector::ZeroVector;
	float Radius = 0.f, Speed = 1000.f, Depth = 180.f, HalfWidth = 150.f, EndRing = 0.f;
	float FromYaw = 0.f, Sweep = 360.f, Seconds = 1.f;
	double StartAt = 0.0;
	double ArrivedAt = -1.0;
	float FxDone = 0.f;
	int32 FxCount = 0;
	TSet<TWeakObjectPtr<AWRCharacterBase>> Struck;
};
