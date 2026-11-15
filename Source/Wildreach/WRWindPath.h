#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

class AActor;
class ACharacter;
class AWRCharacterBase;
class AWRProjectile;
class UMaterialInstanceDynamic;
struct FWRHit;
struct FWRItemDef;

/**
 * The Wind path's rules (cooks/biome-sky.md 2.18-2.19, combat.md 2.11-2.13): Momentum (a spender that builds while you're
 * in the air or sprinting), the Featherfall root passive, the half-effect rule (no dead button: a Wind ability short of
 * Momentum still goes off at half strength for its stamina), knockback (Wind's hit property, through WRSkyAir::Push), the
 * passives the combat and ranged code ask about (Off balance, Lift, Aerial mastery, Eye of the storm, Swift draw,
 * Piercing gale, Crosswind, Tailwind shot), and the eight Wind combos (Data/combos.json). Numbers: tuning
 * resources.momentum, passives.wind, nodeEffects.wind.*, spells.*, wildbrands.*, knockback, combos.json. Everything that
 * changes the world runs on the server; what prediction needs (jump height, the second hop, air dash once per airtime)
 * every machine works out the same way from replicated state.
 */
namespace WRWind
{
	// The path's own tags (defined in WRWindPath.cpp).
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Element_Wind);           // a Wind spell's or Wildbrand's element
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Passive_Featherfall);    // the root passive (main path only)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Spinning);         // Whirlwind's spin, a Vortex being cast (Eye of the storm)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_SkyLord);          // Sky lord's hover (every machine: the look, the swings)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_HighMomentum);     // >= momentum highAt in a fight (the look, every machine)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_AirDash);          // dashing through the air
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_Firestorm);        // Fire + Wind 25
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_EmberRain);        // Fire + Wind 50
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_IceArrows);        // Wind + Frost 25
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_AvalanchePlunge);  // Wind + Frost 50
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_Tailcharge);       // Lightning + Wind 25
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_JetStream);        // Lightning + Wind 50
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_BleedingArrows);   // Wind + Blood 25
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_HexStorm);         // Wind + Blood 50
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_LeafGlide);        // Nature + Wind 25
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_WindborneSeeds);   // Nature + Wind 50
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_SoulWind);         // Wind + Necromancy 25 (data-ready: the Necromancy path's soul pickup reads SoulPickupScale)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_ReapersCyclone);   // Wind + Necromancy 50 (data-ready: its Scythe sweep reads the tag)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_SandBlast);        // Sun & sand + Wind 25
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_Sunshot);          // Sun & sand + Wind 50
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_SeaSpray);         // Wind + Tide 25
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_HurricaneEye);     // Wind + Tide 50

	WILDREACH_API FName PathId();
	/** This survivor walks the Wind path (main or subclass): only they build and spend Momentum. */
	WILDREACH_API bool Walks(const AActor* Who);

	// ---------------------------------------------------------------- Featherfall and the air (WRSkyAir asks these)
	/** Featherfall (main path): no fall damage from any height, also while Sky lord hovers and on the fall after it ends
	 *  (until the next landing). WRSkyAir's fall damage asks this before anything else. */
	WILDREACH_API bool NoFallDamage(const AActor* Who);
	/** Featherfall: glider stamina x this (0.5), 1 otherwise. */
	WILDREACH_API float GliderStaminaScale(const AActor* Who);
	/** Path Glide (the "wind_glide" node): Who may glide with no glider. Its numbers (spells.windGlide forward / sink, cm/s);
	 *  a carried levistone glider's numbers win where they're better (WRSkyAir picks). Leaf glide (Nature + Wind 25)
	 *  takes 40% off the sink in the forest or among trees. */
	WILDREACH_API bool CanPathGlide(const AActor* Who);
	WILDREACH_API void PathGlideStats(const AActor* Who, float& OutForward, float& OutSink);
	/** Any glide's sink x this (Leaf glide: 0.6 in the forest or within 20 m of 5+ trees; 1 otherwise). Server and owner. */
	WILDREACH_API float GlideSinkScale(const AActor* Who);
	/** Server, every tick of a path glide: pays 4 Momentum a second (then 3 stamina a second once it's empty); nothing in
	 *  an updraft column. False: nothing left to pay with, the glide closes. */
	WILDREACH_API bool PathGlideTick(AWRCharacterBase* Who, float DeltaTime, bool bInColumn);
	/** Every machine: Who entered an updraft column (WRSkyAir): Air dash may be used again this airtime. */
	WILDREACH_API void OnEnteredUpdraft(const AActor* Who);
	/** Lift rank 3: Who may take a second, small jump in the air now (the CMC's jump count allows it; WRSkyAir must treat
	 *  a jump press that the count still allows as a jump, not a glide opening). */
	WILDREACH_API bool HasAirHop(const AActor* Who);

	// ---------------------------------------------------------------- Momentum
	WILDREACH_API float MomentumValue(const AActor* Who);
	/** Momentum / MaxMomentum (0..1) on any machine that has the attributes (the owner and the server). */
	WILDREACH_API float MomentumFraction(const AActor* Who);
	/** Momentum >= resources.momentum.highAt (60): knockback x1.25, stamina regen +15%, the bar's air streaks. */
	WILDREACH_API bool HighMomentum(const AActor* Who);
	/** Server: add Momentum (it stops at full). */
	WILDREACH_API void GainMomentum(AWRCharacterBase* Who, float Amount);
	/**
	 * Server: a Wind ability's price (the half-effect rule, cooks/biome-sky.md 2.18.2): with at least Cost it's taken and
	 * the ability goes off in full (1); short of it nothing is taken and it goes off at resources.momentum.halfEffect
	 * (0.5): half damage, half push. Sky lord makes Gust and Wind cut free (bFreeInSkyLord). Returns the effect scale.
	 */
	WILDREACH_API float PayMomentum(AWRCharacterBase* Who, float Cost, bool bFreeInSkyLord = false);
	/** Server: take up to Amount (a held Whirlwind's extra spins); returns what was taken. */
	WILDREACH_API float SpendMomentum(AWRCharacterBase* Who, float Amount);
	/** Server, every tick for a survivor (the status component): Momentum's gains (airborne, sprinting) and decay, the
	 *  high-Momentum fight look, Tailcharge (Lightning + Wind 25). */
	void TickOwner(AWRCharacterBase* Who, float DeltaTime);

	// ---------------------------------------------------------------- every machine, every frame (AWRCharacterBase::Tick)
	/** Lift's jump height and the rank-3 hop (the same on the owner and the server: prediction), the landing that resets
	 *  Air dash and Sky lord's fall cover, Jet stream's Bolt dash clock. */
	WILDREACH_API void TickMovement(ACharacter* Who, float DeltaTime);
	/** Air dash once per airtime: used / may use (both machines). */
	WILDREACH_API bool AirDashReady(const AActor* Who);
	WILDREACH_API void NoteAirDash(const AActor* Who);
	/** Jet stream (Lightning + Wind 50): a Bolt dash ended within combo jet_stream.window s on this machine. */
	WILDREACH_API bool BoltDashJustEnded(const AActor* Who);
	/** Sky lord ended: no fall damage until Who lands. */
	WILDREACH_API void CoverFall(const AActor* Who);
	WILDREACH_API bool InSkyLord(const AActor* Who);

	// ---------------------------------------------------------------- knockback (combat.md 2.11: Wind's hit property)
	/**
	 * Server: push Target Distance cm along Dir over Seconds, the Wind way: through WRSkyAir::Push (brace, anchor boots,
	 * a roll's i-frames, the edge-fall credit), x the tier's share (knockback.lowland/midland/highland; bosses never), x
	 * momentum.knockbackAtHigh at high Momentum. A body it drives into a wall within the push takes knockback.wallSlamStance
	 * stance (+ Off balance) when it gets there. Firestorm (Fire + Wind 25): a burning body spreads burn where it stops.
	 */
	WILDREACH_API void Knock(AWRCharacterBase* By, AWRCharacterBase* Target, const FVector& Dir, float Distance, float Seconds, FName Source);
	/** Server: one Wind hit (spells, waves, gales): element, the path's XP, the half-effect scale, the combos' statuses
	 *  (Firestorm burn and Sand blast blind on Gust / Vortex: bGustLike; Sea spray soak). Returns the result's damage path. */
	void WindHit(AWRCharacterBase* By, AWRCharacterBase* Target, float Damage, float Poise, float StanceMult, const FVector& Origin,
		bool bSpell, bool bGustLike, bool bDamageOverTime = false);
	/** A boss (mini or major) by the stance rules: never knocked, never lifted. */
	WILDREACH_API bool IsBoss(const AWRCharacterBase* Who);
	/** A creature's tier (lowland / midland / highland), highland for survivors and anything without a row. */
	WILDREACH_API FName TierOf(const AWRCharacterBase* Who);

	// ---------------------------------------------------------------- passives the combat code asks
	/** Damage x: Aerial mastery (+6% a rank while the attacker is in the air), Eye of the storm (-20% taken while the
	 *  target spins: Whirlwind, a Vortex cast, standing in your own Tornado's eye). */
	float DamageScale(const AWRCharacterBase* Attacker, const AWRCharacterBase* Target, const FWRHit& Hit);
	/** Stance x: Off balance (+10% a rank) on Wind hits. */
	float StanceScale(const AWRCharacterBase* Attacker, const FWRHit& Hit);
	/** Stamina regen x: +15% at high Momentum. */
	WILDREACH_API float StaminaRegenScale(const AActor* Who);
	/** Server: a blow landed: +4 Momentum when it was struck from the air. */
	void AfterHit(AWRCharacterBase* Target, const FWRHit& Hit, float Damage);

	// ---------------------------------------------------------------- the ranged code asks (WRRanged.cpp, WRProjectile.cpp)
	/** Swift draw: a shot's reload and a throw's wind-up x this (-6% a rank). */
	WILDREACH_API float DrawTimeScale(const AActor* Who);
	/** Crosswind: projectile speed / damage x these (+10% / +5% a rank). */
	WILDREACH_API float ProjectileSpeedScale(const AActor* Who);
	WILDREACH_API float ProjectileDamageScale(const AActor* Who);
	/** How many bodies a shot of AmmoId (or a thrown weapon: None) goes through: the ammo's own "pierce" (items.json) +
	 *  Piercing gale's one. */
	WILDREACH_API int32 ShotPierce(const AActor* Who, FName AmmoId);
	/** Server: a survivor's normal shot left the bow (aimed: drawn full): Tailwind shot's step back unless Sprint is held;
	 *  Windborne seeds counts it. */
	WILDREACH_API void OnShotFired(AWRCharacterBase* Who, AWRProjectile* Shot, bool bAimed);
	/** Server: an arrow of Who's came down where it landed (Windborne seeds plants every 4th). */
	WILDREACH_API void OnArrowLanded(AWRCharacterBase* Who, const FVector& At);

	// ---------------------------------------------------------------- combos read elsewhere
	/** Soul wind (Wind + Necromancy 25): the Necromancy path's soul pickup radius x this. */
	WILDREACH_API float SoulPickupScale(const AActor* Who);
	/** Hurricane eye (Wind + Tide 50) and Eye of the storm read who stands in Who's tornado (the tornado registers here). */
	void SetTornado(const AWRCharacterBase* Caster, AActor* Tornado, float Radius);
	bool InOwnTornado(const AWRCharacterBase* Who);

	// ---------------------------------------------------------------- the body (cooks/biome-sky.md 2.18.5)
	/** What changes the Wind look on this machine (the high-Momentum fight look). */
	uint32 LookSig(const AActor* Hero);
	/** Every machine: the live parts over what the player_art row drew: in a fight at >= 60 Momentum the spirals drift x4
	 *  and gain 1 nit, the hair streams as if in a 10 m/s wind. */
	void ApplyLook(const AActor* Hero, const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Skin, const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Hair,
		FName Main, float MainS, FName Sub, float SubS);
}
