#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

class AActor;
class AWRCharacterBase;
class UMaterialInstanceDynamic;
class UWorld;
struct FWRHit;

/**
 * The Sun & sand path's rules (cooks/biome-desert.md 2.17-2.18; build-biome-desert.md 2.16). Radiance is the spender
 * (DD SPEC: "charges in daylight, weak at night"): 100 + 10 a Radiance capacity rank; by day it fills 2.5 a second while
 * you stand exposed to the sky (no roof within 30 m straight up), half that in shade, +3 a landed melee hit; weather
 * scales it (overcast x0.6, heat wave x1.2, the sandstorm belt and the Dimming 0, the Long Noon x1.5 and no drain). At
 * night it fills only from a lit sunstone lamp or a tamed Gildback within 6 m (0.5 a second), Dunewrap's four pieces
 * (half the day rate) and kills (+5), and drains 1.5 a second (Dawn: x0.3). Underground counts as night. Sandwalker (root
 * passive, main path only) is the tag Passive.Sandwalker: no soft-sand slow or sprint cost, no sandstorm slow, 60 m sight
 * in storms, no sinking in quicksand (the movement and the hazards read it). The passives the combat code asks about:
 * Blind chance, Brilliance, Searing light, Sun-forged, Zenith, Gleam (on a parry), Heat shimmer (the roll reads
 * nodeEffects), Scour (the storms' damage). Combos with the built paths (Data/combos.json). Numbers: tuning sun.radiance,
 * sun.path, spells.<key>, wildbrands.<node>, nodeEffects sun.*, combos.json numbers.
 */
namespace WRSun
{
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Passive_Sandwalker);    // the root passive (main path only)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_DesertHeat);      // Fire + Sun 25: Heat builds faster in sunlight
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_Supernova);       // Fire + Sun 50: full Heat and Radiance release a blinding explosion
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_Thunderflash);    // Lightning + Sun 25: shocks also blind
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_SolarStorm);      // Lightning + Sun 50: lightning strikes inside your sandstorms
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_DesertBloom);     // Sun + Nature 25: your plants survive in sand and storms
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_SunGarden);       // Sun + Nature 50: healing zones also blind enemies
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_FrozenSand);      // Sun + Frost 25: quicksand freezes enemies stuck in it
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_SunAndSnow);      // Sun + Frost 50: shattering a blinded enemy flashes
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_DryBlood);        // Sun + Blood 25: bleeds deal more in sunlight
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_SunSacrifice);    // Sun + Blood 50: spend HP to fill Radiance (an action)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_DesertGraves);    // Sun + Necromancy 25: raise from the sand anywhere
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_ScarabSwarm);     // Sun + Necromancy 50: Corpse explosion releases scarabs

	// ---------------------------------------------------------------- the path
	WILDREACH_API bool Walks(const AActor* Who);
	WILDREACH_API int32 Rank(const AActor* Who, FName Node);
	WILDREACH_API int32 PointsIn(const AActor* Who);
	/** A number from the path's tuning (sun.path.<Key>). */
	WILDREACH_API float Num(const TCHAR* Key, float Fallback);
	/** Sandwalker: the main path's root passive (soft sand, storms, quicksand are nothing to you). */
	WILDREACH_API bool Sandwalker(const AActor* Who);

	// ---------------------------------------------------------------- the sun over you
	/** The sun is up where Who stands (the sky's clock; the Long Noon holds the desert at noon). */
	WILDREACH_API bool Daylight(const AActor* Who);
	/** Nothing over Who for 30 m straight up (world-map "exposed"): a roof, an overhang, a palm crown are shade. */
	WILDREACH_API bool Exposed(const AActor* Who);
	/** A lit sunstone lamp, a Sun Beacon or a tamed Gildback within 6 m: Radiance's night fill. */
	WILDREACH_API bool NearSunLight(const AActor* Who);
	/** Is there sand under Who to Burrow through (any land's sand: the desert's soft sand and beaches, the coast's)? */
	WILDREACH_API bool SandUnder(const AActor* Who);

	// ---------------------------------------------------------------- Radiance
	WILDREACH_API float RadianceOf(const AActor* Who);
	WILDREACH_API float MaxRadianceOf(const AActor* Who);
	WILDREACH_API bool HasRadiance(const AActor* Who, float Amount);
	/** Server: take Amount. True when paid. */
	WILDREACH_API bool SpendRadiance(AWRCharacterBase* Who, float Amount);
	WILDREACH_API void GainRadiance(AWRCharacterBase* Who, float Amount);
	/** The owning machine: a press refused for Radiance (the bar flashes). */
	WILDREACH_API void NoteRefused(const AActor* Who);
	WILDREACH_API float RefusedFlash(const AActor* Who);
	/** 0..1: how lately Radiance was rising (the bar brightens while it fills). */
	WILDREACH_API float FillingGlow(const AActor* Who);
	/** 0..1: the bar's dim edge at night (draining). */
	WILDREACH_API float NightDim(const AActor* Who);
	/** Server, every tick for a survivor (the status component): capacity, the day fill and the night drain, the night
	 *  sources, Supernova. */
	void TickOwner(AWRCharacterBase* Who, float DeltaTime);

	// ---------------------------------------------------------------- what the combat code asks
	/** Server: Blind chance's extra blind on Attacker's blows (and its +5 on weapon hits); Thunderflash's blind on shocks;
	 *  Sunglass and Empress talismans' blind on crits. */
	void ScaleBuildups(const AWRCharacterBase* Attacker, FWRHit& Hit);
	/** Brilliance, Sun-forged, Zenith, Searing light, Dry blood. */
	float DamageScale(const AWRCharacterBase* Attacker, const AWRCharacterBase* Target, const FWRHit& Hit);
	/** Server: a blow landed: Radiance for a melee hit and for a kill, the blind proc's flash. */
	void AfterHit(AWRCharacterBase* Target, const FWRHit& Hit, float Damage);
	/** Server: Defender parried Attacker: Gleam's blind. */
	void OnParry(AWRCharacterBase* Defender, AWRCharacterBase* Attacker);
	/** Zenith is lit on Who (Radiance >= 90% and the node): the weapon's gold edge (every machine reads it). */
	WILDREACH_API bool ZenithLit(const AActor* Who);
	/** Server: the Sun proc's flash (blind, Gleam, Supernova): an fx and its sound on everyone. */
	WILDREACH_API void Proc(AWRCharacterBase* Who, FName Fx, FName Sound, const FVector& At);

	// ---------------------------------------------------------------- the body (cooks/biome-desert.md 2.18)
	uint32 LookSig(const AActor* Hero);
	/** Every machine: the gold flecks hold a stored glow at night (by Radiance), the full-Radiance warm shimmer at 100. */
	void ApplyLook(const AActor* Hero, const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Skin, FName Main, float MainS, FName Sub, float SubS);
}
