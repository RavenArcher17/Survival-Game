// Worn-gear perks that hang on a moment rather than on one formula (Data/items.json worn / gem stats, Data/gear.json set
// bonuses, read through UWRInventoryComponent::GearStatFor): a dodge's next blow and its mirage, a parry's ward, a kill's
// heal and its stacking power, a blow landing twice or splitting onto a second enemy, a spell cast from a full bar, the
// Gale plume's gust, standing still, and the extra room in a path's bar. The plain "x this stat" ones live where their
// number is worked out (knockback, soak, thirst, sprint, storm picks, thornbrake...). Numbers: tuning gear.*.
// Everything here runs on the server.
#pragma once

#include "CoreMinimal.h"

class AActor;
class AWRCharacterBase;
struct FWRHit;

namespace WRGearPerks
{
	/** A dodge began (a roll, a kick under water, an air dash): afterDodge's window opens, and dodgeDecoy leaves a mirage
	 *  the creatures on you strike at instead (they lose you for that many seconds; never bosses). */
	WILDREACH_API void OnDodge(AWRCharacterBase* Who);
	/** An air dash or an airborne dodge (Sky lord's): the Gale plume throws enemies within gear.airDodgePushRadius back
	 *  airDodgePush cm. */
	WILDREACH_API void OnAirDodge(AWRCharacterBase* Who);
	/** A parry (a perfect guard): Rune of Warding's perfectGuardShield HP ward for gear.guardShieldSeconds. */
	WILDREACH_API void OnParry(AWRCharacterBase* Who);

	/** Was PathId's resource full on Who just now (Heat, Chill, Mana... Blood's is health)? */
	WILDREACH_API bool ResourceFull(const AWRCharacterBase* Who, FName PathId);
	/** A path spell was cast; bFull: its resource was full before the cost (the Prism heart reads it). */
	WILDREACH_API void NoteCast(AWRCharacterBase* Who, bool bFull);
	/** A spell blow from a cast made at a full bar within gear.fullCastSeconds, with spellCritFullResource worn: a crit
	 *  at OutMult (spellCritMult). */
	WILDREACH_API bool FullResourceCrit(const AWRCharacterBase* Attacker, const FWRHit& Hit, float& OutMult);

	/** The attacker's worn share on a landed blow (added to ApplyHit's gear sum): afterDodge (used up by the blow),
	 *  rangedFromAbove, vsSwarm, necroDamage, and the Mourner's lantern's kill stacks. */
	WILDREACH_API float DealtBonus(AWRCharacterBase* Attacker, const AWRCharacterBase* Target, const FWRHit& Hit);
	/** A landed blow: Rune of Doubling's second blow at half (doubleHit), Rune of the Prism's copy onto a second enemy
	 *  (spellSplit). Copies are reactions and never copy again. */
	WILDREACH_API void AfterHit(AWRCharacterBase* Target, const FWRHit& Hit);
	/** A killing blow: the Mire heart's heal on a bleeding body (killHeal), a Mourner's lantern stack. */
	WILDREACH_API void OnKill(AWRCharacterBase* Target, const FWRHit& Hit);

	/** A spell's stamina and path resource cost x this (spellCost: Wardcloth 4). 1 for no one. */
	WILDREACH_API float SpellCostScale(const AActor* Who);
	/** The extra room in a path's bar (pathResourceMax: an amethyst in the weapon). */
	WILDREACH_API float ResourceMaxBonus(const AActor* Who);
	/** UWRPathComponent::ApplyToCharacter put Bonus into the builder bars (Heat, Chill, Charge, Growth, Momentum). */
	WILDREACH_API void NoteResourceMax(const AWRCharacterBase* Who, float Bonus);
	/** A survivor's tick: the builder bars follow an amethyst socketed or taken out; stillRegen heals a body stood still
	 *  gear.stillSeconds (HealScale: its healing taken). */
	WILDREACH_API void TickPlayer(AWRCharacterBase* Who, float DeltaTime, float HealScale);
}
