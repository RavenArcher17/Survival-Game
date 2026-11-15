#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

class AActor;
class AWRCharacterBase;
class AWRCharacter;
class UMaterialInstanceDynamic;
class UWorld;
struct FWRHit;

/**
 * The Arcane path's rules (cooks/biome-crystal.md 2.17-2.18; build-biome-crystal.md P2.9): Mana (a steady pool, 100 + 15 a
 * Mana capacity rank, refilling 6 a second in and out of a fight; Deep well, the root passive on the main path, x1.5),
 * shields (one HP layer taken after guard and before Health: the bigger one wins, never stacked; while it holds no status
 * builds; Absorb gives Mana back; Reflect throws a reflectable projectile back along its line at x0.8), Mark (the path's
 * element: Mark chance builds it faster and makes your spells bite harder on the marked), the passives the combat code asks
 * about (Focus, Echo's recast, Quick cast, Mana burn, Glyph power, Lasting runes, Rune link, Hardened ward, Ward allies),
 * the combos with the built paths (Data/combos.json) and the look's extras on a cast (a crystal chime and a fingertip trace
 * at full strength). Numbers: tuning arcane.*, spells.<key>, wildbrands.<node>, combos.json numbers. Everything runs on the
 * server; what other machines draw rides on replicated loose tags (the shield film, Inscribe's blade rune).
 */
namespace WRArcane
{
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Passive_DeepWell);      // the root passive (main path only): Mana regen x1.5
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Shielded);       // an Arcane shield holds (every machine: the faceted violet film)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Inscribed);      // Inscribe's rune on the weapon (+arcane a hit)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_InCircle);       // standing in an ally's Rune circle (+15% damage)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_ManaFlame);       // Fire + Arcane 25
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_MeteorStorm);     // Fire + Arcane 50
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_QuickMind);       // Lightning + Arcane 25
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_ManaWings);       // Wind + Arcane 25
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_RuneVolley);      // Wind + Arcane 50
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_FrostMissile);    // Arcane + Frost 25
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_FrozenRune);      // Arcane + Frost 50
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_BloodMagic);      // Arcane + Blood 25
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_BloodStar);       // Arcane + Blood 50
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_ManaSap);         // Nature + Arcane 25
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_FaerieRing);      // Nature + Arcane 50
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_BoundDead);       // Arcane + Necromancy 25

	// ---------------------------------------------------------------- the path
	/** This survivor walks the Arcane path (main or subclass). */
	WILDREACH_API bool Walks(const AActor* Who);
	/** Their rank in an Arcane node (0 when not walking the path). */
	WILDREACH_API int32 Rank(const AActor* Who, FName Node);
	/** Points spent in the Arcane path (the Prismback's path magic wants 10+). */
	WILDREACH_API int32 PointsIn(const AActor* Who);

	// ---------------------------------------------------------------- Mana
	WILDREACH_API float ManaOf(const AActor* Who);
	WILDREACH_API float MaxManaOf(const AActor* Who);
	/** The owner and the server: Who can pay Amount (Blood magic: the missing part in HP, 1 a 2 Mana, never the last). */
	WILDREACH_API bool HasMana(const AActor* Who, float Amount);
	/** Server: take Amount (Blood magic covers what's missing). True when paid. */
	WILDREACH_API bool SpendMana(AWRCharacterBase* Who, float Amount);
	WILDREACH_API void GainMana(AWRCharacterBase* Who, float Amount);
	/** The owning machine: a press refused for Mana (the bar flashes and shakes). */
	WILDREACH_API void NoteRefused(const AActor* Who);
	WILDREACH_API float RefusedFlash(const AActor* Who);
	/** Server, every tick for a survivor (the status component): Mana capacity and regen, the shield's clock. */
	void TickOwner(AWRCharacterBase* Who, float DeltaTime);

	// ---------------------------------------------------------------- shields
	WILDREACH_API float ShieldOf(const AActor* Who);
	/** Server: raise a shield of Amount HP on Target for Seconds (the bigger of it and what's there now). Source: whose
	 *  shield it is (their Hardened ward already applied by the caller; their Reflect and Absorb ride with it). */
	WILDREACH_API void GiveShield(AWRCharacterBase* Target, float Amount, float Seconds, AWRCharacterBase* Source);
	/** Server: the shield is gone (spent, expired, burnt off). bBroken: it shattered (a sound and a burst). */
	WILDREACH_API void DropShield(AWRCharacterBase* Target, bool bBroken);
	/** Server, every tick (UWRArcaneWorld): every shield's clock, Mana flame's burn on burning shields; who just dodged
	 *  (the Twin shard talisman's echo). */
	void TickShields(UWorld* World, float Dt);
	/** Hardened ward's scale for Who's shields (x1.2 a rank) and the Tyrant plate talisman's. */
	WILDREACH_API float ShieldScale(const AActor* Who);

	/**
	 * Server: throw a projectile back the way it came (crystal 2.12 reflection rule; shared with the Prismback's plates, the
	 * Geode Tyrant, the Twins' Ward guard and the Arcane Reflect): a real bolt from At flying straight back toward the
	 * shooter's line at the incoming speed, Scale x the damage, now owned by Reflector. Never homes. True when it went.
	 */
	WILDREACH_API bool Reflect(AWRCharacterBase* Reflector, const FWRHit& Incoming, float Damage, float Scale, const FVector& At);
	/** The hit is a reflectable projectile (a bolt, an arrow, a spit, a shard, a lance; not a lob, a beam, siege). */
	WILDREACH_API bool IsReflectable(const FWRHit& Hit);

	// ---------------------------------------------------------------- what the combat code asks
	/** Damage x for Attacker's blow: Focus, Mark chance on marked targets, Rune circle, Mana burn vs plate parts. */
	float DamageScale(const AWRCharacterBase* Attacker, const AWRCharacterBase* Target, const FWRHit& Hit);
	/** Server, just before a blow's damage lands on Target: the shield takes it first (Reflect, Absorb, Mana burn x2).
	 *  Returns what's left for Health. */
	float Incoming(AWRCharacterBase* Target, const FWRHit& Hit, float Damage);
	/** A shield holds on Target: statuses don't build (crystal 2.17 shields). */
	bool BlocksBuildup(const AWRCharacterBase* Target);
	/** Server: Mark chance's extra mark, Frost missile's frost. */
	void ScaleBuildups(const AWRCharacterBase* Attacker, FWRHit& Hit);
	/** Server: a blow landed: Inscribe's rune (+arcane), Mana burn's drain on players. */
	void AfterHit(AWRCharacterBase* Target, const FWRHit& Hit, float Damage);
	/** Mana burn: Who's spells do x2 to shields and shield-flagged parts (the Tyrant's plates, Lyrathis's dome). */
	WILDREACH_API bool BurnsShields(const AActor* Who, const FWRHit& Hit);

	// ---------------------------------------------------------------- casting
	/** Spell windup scale: Quick cast (-8% a rank) and Quick mind (-15% at Charge > 60). */
	float WindupScale(const AActor* Caster);
	/** Server: an Arcane spell went off (the look's chime and fingertip trace at strength 1.0; the cast pulse). */
	void OnCast(AWRCharacterBase* Caster);

	// ---------------------------------------------------------------- the Prismglass spear (crystal 2.19.2)
	/**
	 * Server: a charged heavy with the Prismglass spear: two thrust-lines of light 15 degrees either side of the blow (the
	 * spear's reach x1.6), each hitting the first body in its line for Share x the blow's damage with Mark 25.
	 */
	WILDREACH_API void PrismLines(AWRCharacterBase* Wielder, float BlowDamage);

	// ---------------------------------------------------------------- the body (cooks/biome-crystal.md 2.18)
	uint32 LookSig(const AActor* Hero);
	/** Every machine: the shield film's tint on the skin while it holds (a faint violet facet), nothing else: the stages
	 *  are the player_art row (paths.arcane) and the cast pulse is the shared 2 s pulse. */
	void ApplyLook(const AActor* Hero, const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Skin, FName Main, float MainS, FName Sub, float SubS);
}
