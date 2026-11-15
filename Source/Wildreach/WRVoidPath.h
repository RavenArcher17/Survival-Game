#pragma once

// The Void path's rules (cooks/biome-void.md 2.17-2.18): the Void bar (Corruption, never called that: the world meter
// is), the tolerance under which void abilities are free, the max-HP cost past it (into MaxHealthLost: the grey band),
// the self-cost limit, the bar's damage, Empty vessel, the passive nodes (Unravel chance, Abyssal power, Dark matter,
// Event horizon, Fade, Unseen, Gluttony, Feed, Insatiable), Nowhere's crit and Sacrifice's buff, and the four Void combos
// whose partner paths exist (Fire, Lightning, Frost, Blood). Numbers: tuning voidPath.*, spells.<void spell>,
// nodeEffects.void.*, Data/combos.json. Everything that changes the world runs on the server.

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"
#include "WRCombatTypes.h"

class AActor;
class AWRCharacterBase;
class UMaterialInstanceDynamic;

namespace WRVoidPath
{
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Unravelled);   // armour cut, physResist 1.0, stance taken +15% (Unravel chance)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Phased);       // Phase: blows and grabs pass through
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Nowhere);      // Nowhere: untargetable; the next hit crits
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Sacrifice);    // Sacrifice: +30% damage
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_NullStrike);  // while a Null strike swing is out (ignores 30% armour)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_BlackFlame);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_VoidFurnace);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_DarkLightning);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_SingularitySpark);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_FrozenBlink);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_IceMaw);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_VoidLeech);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_AbyssalHunger);

	/** The path walks (main or subclass) / is the main path / points spent while it walks. */
	WILDREACH_API bool Walks(const AActor* Who);
	WILDREACH_API bool IsMain(const AActor* Who);
	WILDREACH_API int32 Points(const AActor* Who);
	/** Your tolerance (20 + Void tolerance's 10 a rank). */
	WILDREACH_API float Tolerance(const AActor* Who);
	/** The max HP a void ability would cost now (0 under tolerance; Empty vessel, gear, Void furnace applied).
	 *  bAlways: Sacrifice (tolerance doesn't apply). */
	WILDREACH_API float CostNow(const AActor* Who, FName NodeId, bool bAlways = false);
	/** Either machine: can Who use the node now? False with OutWhy when the self-cost limit refuses it. */
	WILDREACH_API bool CanUse(const AActor* Who, FName NodeId, bool bAlways, FString& OutWhy);
	/** Server: pay a void ability: its max HP (past tolerance) into MaxHealthLost and its Void into the bar (Consume
	 *  takes it out); the decay clock restarts; Void furnace's Heat. */
	WILDREACH_API void Pay(AWRCharacterBase* Who, FName NodeId, bool bAlways = false);
	/** Server: Nowhere's next blow is critical (used up by DamageScale). */
	WILDREACH_API void ArmCrit(AWRCharacterBase* Who);
	/** The node's spells.<key>. */
	WILDREACH_API float SpellNum(FName NodeId, const TCHAR* Field, float Fallback);

	// ---------------------------------------------------------------- the combat code's hooks (server)
	/** Damage x: the bar's +0.3% a point and Abyssal power on void blows, Gluttony, Sacrifice, Dark matter on armour, Black
	 *  flame's burns on armour, Nowhere's crit (used up). */
	float DamageScale(AWRCharacterBase* Attacker, const AWRCharacterBase* Target, const FWRHit& Hit);
	/** Armour ignored by this blow (Null strike 30%, Dark lightning 30%, an Unravelled target's 20%). */
	float ArmourPen(const AWRCharacterBase* Attacker, const AWRCharacterBase* Target, const FWRHit& Hit);
	/** Stance damage x on the target (Unravelled +15%). */
	float StanceScale(const AWRCharacterBase* Target);
	/** A blow landed: Unravel chance; Unseen after a blink. */
	void AfterHit(AWRCharacterBase* Target, const FWRHit& Hit, float Damage);
	/** Target is about to die from Hit: Feed's max HP back. */
	void OnKill(AWRCharacterBase* Target, const FWRHit& Hit);
	/** Phase / Nowhere: this blow passes through Target (true: it doesn't land at all). */
	bool PassesThrough(const AWRCharacterBase* Target, const FWRHit& Hit);
	/** Lifesteal healing Amount on Who: Void leech turns 30% of it into max HP back first; returns what's left to heal. */
	float LeechHeal(AWRCharacterBase* Who, float Amount);
	/** Every tick for a survivor: the bar's decay, its size. */
	void TickOwner(AWRCharacterBase* Who, float DeltaTime);
	/** Fade's extra i-frames on dodges and blinks (s). */
	WILDREACH_API float FadeSeconds(const AActor* Who);
	/** Unseen: creatures within 15 m of Who lose it for a while (bosses less). */
	void Unseen(AWRCharacterBase* Who);
	/** Max health won back on a kill of this body (Feed's table by its tier). */
	float FeedFor(const AWRCharacterBase* Killed);

	// ---------------------------------------------------------------- the body (every machine, WRHeroLook)
	WILDREACH_API uint32 LookSig(const AActor* Hero);
	/** 2.18's live driver: the stage (MainS / SubS) is the cap; the Void bar and the band move the look within it. */
	WILDREACH_API void ApplyLook(const AActor* Hero, const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Skin, FName Main, float MainS, FName Sub, float SubS);
}
