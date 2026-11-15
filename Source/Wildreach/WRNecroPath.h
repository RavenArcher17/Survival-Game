#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

class AActor;
class AWRCharacterBase;
class AWRCreature;
class UMaterialInstanceDynamic;
class UWorld;
struct FWRHit;

/**
 * The Necromancy path's rules (cooks/biome-graveyard.md 2.17-2.18; build-biome-graveyard.md 2.18): Souls (a spender
 * collected from kills within 30 m that you or your minions hit in their last 10 s, by tier; lost on death), the
 * Deathless root passive (once per Rest a killing blow leaves you at 1 HP; main path only), raising (a creature's body
 * under 3 minutes old and not butchered stands up for you at 60% for 30 s, never a tame, at most 3 / 5), the passive
 * nodes the combat code asks about (Wither chance, Shared pain, Soul shield, Final harvest, Soul siphon, Reaping stride,
 * Death mark, Harvest, Necrotic power, Restless dead, Dead march), the Graveyard's corruption buff, and the five combos
 * built so far (Data/combos.json). Numbers: tuning necromancy.*, spells.<key>, wildbrands.<node>, combos.json numbers.
 * Everything runs on the server; what every machine draws (the marks, Lich form, a spent Deathless) rides on replicated
 * loose tags.
 */
namespace WRNecro
{
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Element_Necromancy);    // a Necromancy spell's or Wildbrand's element
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Passive_Deathless);     // the root passive (main path only)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_DeathlessSpent); // used since the last Rest (the grey bell icon)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_DeathMark);      // a Death mark's turning sigil over a body (10 s)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_LichForm);       // Lich form: spells cost no Souls (15 s)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Raised);         // a raised minion (every machine: no tame UI, the bone look)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_AshWraith);       // Fire + Necromancy 25
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_FuneralPyre);     // Fire + Necromancy 50
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_Preserved);       // Frost + Necromancy 25
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_FrozenLegion);    // Frost + Necromancy 50
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_JoltOfLife);      // Lightning + Necromancy 25
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_StormLich);       // Lightning + Necromancy 50
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_BloodBond);       // Blood + Necromancy 25
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_DeathCurse);      // Blood + Necromancy 50
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_CycleOfLife);     // Nature + Necromancy 25
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_RotAndBloom);     // Nature + Necromancy 50

	// ---------------------------------------------------------------- Souls
	/** This survivor walks the Necromancy path (main or subclass): only they collect and spend Souls. */
	WILDREACH_API bool Walks(const AActor* Who);
	WILDREACH_API float SoulsOf(const AActor* Who);
	WILDREACH_API float MaxSoulsOf(const AActor* Who);
	/** The owner and the server: Who can pay Amount (always in Lich form). */
	WILDREACH_API bool HasSouls(const AActor* Who, float Amount);
	/** Server: take Amount of Souls (free in Lich form). True when it was paid. */
	WILDREACH_API bool SpendSouls(AWRCharacterBase* Who, float Amount);
	/** Server: give Souls back (a cast that found nothing to raise). */
	WILDREACH_API void RefundSouls(AWRCharacterBase* Who, float Amount);
	/** Server: Souls flow in from From (a pale-green wisp arcs to the chest). Fractions carry (Harvest). */
	WILDREACH_API void GainSouls(AWRCharacterBase* Who, float Amount, const FVector& From, bool bBright = false);
	/** The owning machine: a press refused for Souls (the bar flashes). */
	WILDREACH_API void NoteRefused(const AActor* Who);
	WILDREACH_API float RefusedFlash(const AActor* Who);
	/** Server, every tick for a survivor (the status component): max Souls by Soul capacity, the minions' upkeep. */
	void TickOwner(AWRCharacterBase* Who, float DeltaTime);
	/** Server: Who died: their Souls are lost, their minions crumble. */
	void OwnerDied(AWRCharacterBase* Who);
	/** Server: Who finished a Rest (WRSurvival): Deathless re-arms. */
	WILDREACH_API void Rested(AWRCharacterBase* Who);

	// ---------------------------------------------------------------- raising
	/** A raised minion (server: the minion table; every machine: the Status.Necro.Raised tag). */
	WILDREACH_API bool IsRaised(const AActor* Who);
	/** The survivor a raised minion serves (server). */
	WILDREACH_API AWRCharacterBase* MasterOf(const AActor* Minion);
	/** A body that may still be raised by By: a creature's body (not a boss, a mini, a Risen or a player), under 3
	 *  minutes since it fell (10 with Preserved on a frozen death), not butchered, not already used. */
	WILDREACH_API bool CanRaise(const AWRCreature* Body, const AWRCharacterBase* By);
	/** Bodies By could raise within Radius of At, nearest first. */
	WILDREACH_API TArray<AWRCreature*> BodiesNear(const UWorld* World, const FVector& At, float Radius, const AWRCharacterBase* By);
	/** Server: raise Body for Owner (the body is used up): the same creature at necromancy.raisedStats for Seconds.
	 *  bIgnoreCap: Army of the dead / the free combo raises. StatScale < 0: the default 60%. Variant: ash / ice / blood. */
	WILDREACH_API AWRCreature* Raise(AWRCharacterBase* Owner, AWRCreature* Body, float Seconds, bool bIgnoreCap, float StatScale = -1.f, FName Variant = NAME_None);
	/** Server: the owner's raised minions, alive. */
	WILDREACH_API TArray<AWRCreature*> MinionsOf(const AWRCharacterBase* Owner);
	/** How many may stand at once (3, 5 with Second wave). */
	WILDREACH_API int32 MinionCap(const AActor* Owner);
	/** Server: a minion goes to bones now (expired, detonated, its master gone). */
	WILDREACH_API void Crumble(AWRCreature* Minion, bool bExplode);

	// ---------------------------------------------------------------- what the combat code asks
	/** Damage x for Attacker's blow (corruption buff, Necrotic power, Final harvest on bosses / players, a minion's 60% and
	 *  Dead march). */
	float DamageScale(const AWRCharacterBase* Attacker, const AWRCharacterBase* Target, const FWRHit& Hit);
	/** Server, just before a blow's damage goes on Target: Soul shield, Shared pain, Deathless. Returns what's left. */
	float Incoming(AWRCharacterBase* Target, const FWRHit& Hit, float Damage);
	/** Server: the decay a blow builds (Wither chance; an ash wraith's burn). */
	void ScaleBuildups(const AWRCharacterBase* Attacker, FWRHit& Hit);
	/** Server: a blow landed (credit for Souls, Death mark, Final harvest, Jolt of life, Blood bond). */
	void AfterHit(AWRCharacterBase* Target, const FWRHit& Hit, float Damage);
	/** Server: Target dies of Hit (Souls to every Necromancer within 30 m who had a hand in it, Soul siphon, Reaping
	 *  stride, a minion's Cycle of life, Frozen legion, Death curse; the body's time of death for raising). */
	void OnKill(AWRCharacterBase* Target, const FWRHit& Hit);
	/** Server: a creature died (any cause, WRCreature::Die): the body's clock starts; a minion crumbles (no loot). */
	void CreatureDied(AWRCreature* Creature, AActor* Killer);

	// ---------------------------------------------------------------- the body (cooks/biome-graveyard.md 2.18)
	/** What changes the Necromancy look on this machine (Lich form, the raise flare). */
	uint32 LookSig(const AActor* Hero);
	/** Every machine: Lich form shows the full ash look at any point count; the pupil ring flares x1.6 for 1.5 s after a
	 *  raise. The stages themselves are the player_art row (paths.necromancy). */
	void ApplyLook(const AActor* Hero, const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Skin, const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Eyes,
		const TMap<FName, FLinearColor>& EyeDefaultColors, FName Main, float MainS, FName Sub, float SubS);
	/** Server: a raise lit the caster's pupil ring (the look reads it through LookSig). */
	void NoteRaiseFlare(AWRCharacterBase* Who);
	/** Every machine: when the last raise flare was lit on this hero (0 none). */
	float RaiseFlareAge(const AActor* Hero);
}
