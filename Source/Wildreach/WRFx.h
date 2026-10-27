#pragma once

#include "CoreMinimal.h"
#include "WRCombatTypes.h"

class AActor;
class AWRPlayerController;
class SWidget;
class UObject;
class USceneComponent;
struct FWRHit;

/**
 * Visual effects by name, like WRSound: gameplay names the moment, the effects pass decides what it looks like. Unknown
 * effects draw nothing and are logged once. Nothing spawns on a dedicated server (or in a -nullrhi bot). The effects are
 * recipes in Data/fx.json, drawn by a small sprite particle system (WRFxSystem.h); combat-feel numbers (hit-stop, shake)
 * are in tuning.json "fx". Effect names:
 *   combat   hit.spark (metal, block), hit.blood (flesh), hit.dust (stone, dry or soft bodies), hit.ice, hit.magic,
 *            parry.flash, dodge.dust, land.dust, death.poof, stomp (a boss or a big creature slams the ground)
 *   spells   fire.bolt, frost.shard (attached trails), fire.impact, frost.impact, firewall, lavapool, frostnova, icelance,
 *            blizzard, cast.fire, cast.frost (hand glow), status.burn, status.frozen
 *   world    gather.wood, gather.stone, gather.fiber, build.place, build.break, hearth.fire, lava.embers, torch.fire,
 *            brazier.fire
 *   weather  snow, ash (around the local camera)
 *   moments  levelup, tame.success, discover
 */
namespace WRFx
{
	/** A one-shot effect at a place. */
	WILDREACH_API void SpawnAt(const UObject* WorldContext, FName Effect, const FVector& Location, const FRotator& Rotation = FRotator::ZeroRotator, float Scale = 1.f);
	/** A looping effect at a place that ends by itself after Seconds (a fire wall's patch, a lava pool). */
	WILDREACH_API void SpawnFor(const UObject* WorldContext, FName Effect, const FVector& Location, const FRotator& Rotation, float Scale, float Seconds);
	/** A looping effect attached to a component (a bolt's trail, Hearth flames); keep it to stop it later. May be null. */
	WILDREACH_API USceneComponent* Attach(USceneComponent* Parent, FName Effect, FName Socket = NAME_None, float Scale = 1.f);
	/** Stops (and removes) an effect from Attach. What it already gave off fades out on its own. */
	WILDREACH_API void Stop(USceneComponent* Fx);

	/**
	 * Server: show an effect to every player and play Sound there (WRSound) for something only the server sees happen:
	 * a chopped tree, a placed wall, an area spell. Seconds > 0 keeps a looping effect up that long. Also runs here on a
	 * listen server or standalone. One small reliable message; clients far from it skip the effect.
	 */
	WILDREACH_API void Broadcast(const UObject* WorldContext, FName Effect, const FVector& Location, const FRotator& Rotation = FRotator::ZeroRotator,
		float Scale = 1.f, FName Sound = NAME_None, float Seconds = 0.f);
	/** Server: the same, attached to an actor (its skeletal mesh's Socket when it has one): a caster's hand glow. */
	WILDREACH_API void BroadcastOn(AActor* Target, FName Effect, FName Socket, float Scale = 1.f, FName Sound = NAME_None, float Seconds = 0.f);

	/** This machine: shake the local camera. Strength 0..1 at Epicentre, fading to nothing at Radius (0 = everywhere). */
	WILDREACH_API void Shake(const UObject* WorldContext, float Strength, const FVector& Epicentre = FVector::ZeroVector, float Radius = 0.f);
	/** This machine: freeze these actors' animation for a moment (hit-stop). Visual only: movement and the server go on. */
	WILDREACH_API void HitStop(AActor* A, AActor* B, float Seconds);

	/** What a hit carries from the server to every machine's hit presentation (AWRCharacterBase::MulticastHit). */
	enum EHitFlags : uint16
	{
		Hit_Heavy = 1 << 0,     // heavy swing, weapon art, boss blow, meteor
		Hit_Crit = 1 << 1,
		Hit_Riposte = 1 << 2,   // critical on a staggered target
		Hit_Weak = 1 << 3,      // the target is weak to it (a boss and the opposite element)
		Hit_Shatter = 1 << 4,   // a frozen target shattered
		Hit_Melee = 1 << 5,
		Hit_Dot = 1 << 6,       // burn ticks, hazards
		Hit_Fire = 1 << 7,
		Hit_Frost = 1 << 8,
		Hit_Armored = 1 << 9,   // the target wears armour
	};
	/** Server: the flags for a resolved hit. */
	WILDREACH_API uint16 HitFlagsFor(const FWRHit& Hit, bool bCrit, bool bRiposte, bool bWeak, bool bShatter, bool bArmored);
	/**
	 * Every machine: present a hit that landed: impact sparks, blood or dust by creature, the hit sound, hit-stop and a
	 * camera shake for the people in it, the crosshair marker and the damage number.
	 */
	WILDREACH_API void ShowHit(AActor* Target, AActor* Source, float Damage, EWRHitResult Result, uint16 Flags, const FVector& Origin);

	/** The Slate HUD's layer for damage numbers, the hit marker and the parry flash (WRUIHud puts it under its panels). */
	WILDREACH_API TSharedRef<SWidget> MakeHudOverlay(TWeakObjectPtr<AWRPlayerController> PC);
}
