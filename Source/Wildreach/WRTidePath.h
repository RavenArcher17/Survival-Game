#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

class AActor;
class AWRCharacterBase;
class UMaterialInstanceDynamic;
class UWorld;
struct FWRHit;

/**
 * The Tide path's rules (cooks/biome-ocean.md 2.17-2.18; build-biome-ocean.md 2.16). Flow is the spender: 100 + 15 a Flow
 * capacity rank; it builds while you move (5 a second over 300 cm/s on the ground, 8 sprinting, swimming or riding a swim
 * mount; Gills x1.5 in water), on every dodge or kick-dodge (+10; +15 a perfect dodge) and on weapon hits landed while you
 * move (+4), and it drains 8 a second once you've been under 150 cm/s for 1.5 s (a rooted cast pauses the drain). Gills
 * (root passive, main path only) is the tag Passive.Gills (WRUnderwater reads it through HasGills). The passives the combat
 * code asks about: Soak chance (+10% soak buildup and +6 soak on weapon hits a rank), Drowning (soaked players lose 6
 * stamina a second, soaked creatures stop regaining stance and take +15% stance damage), Heavy water (the soak slow and
 * its seconds a rank), Barbed tip, Quick reel, Mending tide, Current. Combos with the built paths (Data/combos.json).
 * Numbers: tuning tide.flow, tide.path, spells.<key>, wildbrands.<node>, nodeEffects tide.*, combos.json numbers.
 * Named WRTidePath (WRTide is the sea's tide).
 */
namespace WRTidePath
{
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Passive_Gills);         // the root passive (main path only)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Current);        // an ally's wave passed you (Current 1: +6% speed; every machine reads it)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Current2);       // Current 2: +12%
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Current3);       // Current 3: +18%
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Anchored);       // pinned by a harpoon's Anchor (rooted, the line rigid)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_HotSpring);       // Fire + Tide 25: healing waves also remove burns
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_BoilingSea);      // Fire + Tide 50: whirlpools boil and burn
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_LiveWater);       // Lightning + Tide 25: lightning chains between soaked enemies
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_ThunderTide);     // Lightning + Tide 50: your waves carry lightning
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_Rain);            // Nature + Tide 25: healing waves grow your plants
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_SwampGuardian);   // Nature + Tide 50: the Ancient guardian pulls like a whirlpool
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_FlashFreeze);     // Frost + Tide 25: soaked enemies freeze at once
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_FrozenMaelstrom); // Frost + Tide 50: Maelstrom freezes what it drags in
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_LifeTide);        // Blood + Tide 25: healing waves cost HP, heal much more
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_SharkHarpoon);    // Blood + Tide 50: harpoons bleed hard and heal you
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_TidalDead);       // Tide + Necromancy 25: healing waves heal minions
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_Depths);          // Tide + Necromancy 50: minions fight under water, drag enemies under

	// ---------------------------------------------------------------- the path
	WILDREACH_API bool Walks(const AActor* Who);
	WILDREACH_API int32 Rank(const AActor* Who, FName Node);
	WILDREACH_API int32 PointsIn(const AActor* Who);
	/** A number from the path's tuning (tide.path.<Key>). */
	WILDREACH_API float Num(const TCHAR* Key, float Fallback);

	// ---------------------------------------------------------------- Flow
	WILDREACH_API float FlowOf(const AActor* Who);
	WILDREACH_API float MaxFlowOf(const AActor* Who);
	WILDREACH_API bool HasFlow(const AActor* Who, float Amount);
	/** Server: take Amount. True when paid. */
	WILDREACH_API bool SpendFlow(AWRCharacterBase* Who, float Amount);
	WILDREACH_API void GainFlow(AWRCharacterBase* Who, float Amount);
	/** The owning machine: a press refused for Flow (the bar flashes). */
	WILDREACH_API void NoteRefused(const AActor* Who);
	WILDREACH_API float RefusedFlash(const AActor* Who);
	/** 0..1: how lately Flow was rising (the bar ripples while it fills). */
	WILDREACH_API float FillingGlow(const AActor* Who);
	/** Server, every tick for a survivor (the status component): capacity, the move gain and the still drain, the tidal
	 *  shields' bursts, the Swamp guardian's pull, Drowning's stamina drain on the soaked. */
	void TickOwner(AWRCharacterBase* Who, float DeltaTime);
	/** Server: Who dodged (a roll, a kick-dodge, a backstep): +10; a perfect dodge +15 more and the water ribbon. */
	WILDREACH_API void OnDodge(AWRCharacterBase* Who, bool bPerfect);
	/** Server: a rooted cast is playing (Maelstrom, High tide): the still drain waits. */
	WILDREACH_API void HoldDrain(AWRCharacterBase* Who, float Seconds);

	// ---------------------------------------------------------------- what the combat code asks
	/** Server: Soak chance's extra soak on Attacker's blows (and its +6 soak on weapon hits). */
	void ScaleBuildups(const AWRCharacterBase* Attacker, FWRHit& Hit);
	/** Server: a blow landed: Flow for a weapon hit while moving, Live water's chains, Flash freeze, Shark harpoon's heal,
	 *  who soaked whom (Drowning, Heavy water read it). */
	void AfterHit(AWRCharacterBase* Target, const FWRHit& Hit, float Damage);
	/** The soak slow on a soaked body (x0.8, Heavy water of whoever soaked it: 0.75 / 0.7 / 0.65; never under 0.4). */
	WILDREACH_API float SoakSlow(const AActor* Who);
	/** Extra seconds Heavy water adds to a soak Who's blows put on (1 a rank). */
	WILDREACH_API float SoakExtraSeconds(const AActor* Who);
	/** Move speed x on Who from the Tide (an ally's Current: replicated tags, so the owner predicts it alike; Heavy water's
	 *  deeper soak slow on creatures, which the server alone moves). */
	WILDREACH_API float MoveSpeedScale(const AActor* Who);
	/** Drowning: a soaked creature regains no stance (x0 here), and takes +15% stance damage (StanceTakenScale). */
	WILDREACH_API float StanceRegenScale(const AActor* Who);
	WILDREACH_API float StanceTakenScale(const AActor* Who);
	/** Mending tide on Who's heals and shields (+8% a rank). */
	WILDREACH_API float MendScale(const AActor* Who);
	/** Server: Who's tidal shield on Target (WRArcane's shared layer), with its burst when it breaks. */
	WILDREACH_API void GiveTidalShield(AWRCharacterBase* Caster, AWRCharacterBase* Target, float Amount, float Seconds);
	/** Server: the water ribbon of a Tide proc (perfect-dodge gain, Anchor, Tow...): an fx and its sound on everyone. */
	WILDREACH_API void Proc(AWRCharacterBase* Who, FName Fx, FName Sound);

	// ---------------------------------------------------------------- the body (cooks/biome-ocean.md 2.18)
	uint32 LookSig(const AActor* Hero);
	/** Every machine: Flow brightens the sheen, the gills open when the head goes under (75+), a wet-look gloss. */
	void ApplyLook(const AActor* Hero, const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Skin, FName Main, float MainS, FName Sub, float SubS);
}
