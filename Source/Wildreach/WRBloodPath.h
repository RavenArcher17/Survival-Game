#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"
#include "UObject/Object.h"
#include "WRCombatTypes.h"
#include "WRBloodPath.generated.h"

class AActor;
class AWRCharacterBase;
class SWidget;
class UMaterialInstanceDynamic;

/**
 * The Blood & curse path's rules (cooks/biome-swamp.md 2.17-2.18): HP is the resource. What a Blood ability costs leaves
 * your health and goes into the Spilled bar (the garnet segment at the end of the HP bar); for a rally window after the
 * last spend your lifesteal refills it at x2, then heals at x1; what isn't won back when the window ends is lost. Damage
 * taken never adds to it. Bloodthirst (the root passive), the passive nodes, the one lifesteal pipeline, the procs and
 * the Fire / Frost / Lightning + Blood combos live here. Numbers: tuning blood.*, spells.<blood spell>, nodeEffects.blood.*,
 * wildbrands.crimson_slash / leech_strike and Data/combos.json. Everything that changes the world runs on the server.
 */
namespace WRBlood
{
	// The path's own tags (defined in WRBloodPath.cpp).
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Element_Blood);           // a Blood spell's or Wildbrand's element
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_LeechStrike);     // while a Leech strike swing is out (its blow heals)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_CrimsonSlash);    // while a Crimson slash swing is out
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_SanguineLord);     // Sanguine lord's self buff
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_IceVeins);         // Ice veins' armour (Frost + Blood 50)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_HexFlame);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_BurningLifesteal);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_ColdBlooded);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_IceVeins);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_Adrenaline);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_RedLightning);

	/** The FWRHit.Buildups key a Doom bolt carries (not a status: ApplyHit skips it; AfterHit marks the target). */
	extern WILDREACH_API const FName DoomMarker;

	// ---------------------------------------------------------------- hooks the combat code calls (server)
	/** Damage x for a blow: Sanguine power on Blood spells, Ruin on a cursed target, Ice veins on the target. */
	float DamageScale(const AWRCharacterBase* Attacker, const AWRCharacterBase* Target, const FWRHit& Hit);
	/** Misfortune: a cursed body's blow visibly whiffs on its contact frame (the stumble and the hex spark play here).
	 *  True: the blow is cancelled. */
	bool Whiffs(AWRCharacterBase* Target, const FWRHit& Hit);
	/** After the damage is applied: the one lifesteal pipeline (Bloodthirst, Lifesteal, Leech strike, Sanguine lord, worn
	 *  lifesteal, Burning lifesteal; x pvpLifesteal on players; through Spilled first), Doom's tally, the fight clock. */
	void AfterDamage(AWRCharacterBase* Target, const FWRHit& Hit, float Damage, bool bLanded);
	/** A blow landed: Bleed chance, Red lightning, a Doom bolt's mark, the garnet splash. */
	void AfterHit(AWRCharacterBase* Target, const FWRHit& Hit, float Damage);
	/** Target is about to die from Hit: Feast, Contagion. */
	void OnKill(AWRCharacterBase* Target, const FWRHit& Hit);
	/** Every tick for a survivor: the rally window, Clotting's regrowth, Spilled kept inside the bar. */
	void TickOwner(AWRCharacterBase* Who, float DeltaTime);

	// ---------------------------------------------------------------- hooks the status code calls (server)
	/** A curse procs on Owner from Source: its seconds with Lingering hex; remembers who cursed it (Misfortune, Contagion,
	 *  Ruin read it) and turns Ruin's sigil over it. */
	float CurseSeconds(AWRCharacterBase* Owner, AWRCharacterBase* Source, float BaseSeconds);
	/** Hemorrhage: x on a bleed burst Source set off. ProcOn: the body it burst on, when it really bursts (the status
	 *  code): a Blood walker's bleed proc plays its cue there (blood.proc). */
	WILDREACH_API float BleedBurstScale(const AActor* Source, AActor* ProcOn = nullptr);
	/** Hex flame (Fire + Blood 25): extra burn seconds on a cursed body. */
	float BurnSecondsBonus(const AWRCharacterBase* Owner, const AWRCharacterBase* Source);
	/** Who cursed this body last (null: nobody, or it ended). */
	WILDREACH_API AWRCharacterBase* CursedBy(const AActor* Body);

	// ---------------------------------------------------------------- both machines
	/** Blood frenzy: weapon windups and recoveries x this (1 = normal). Owner and server agree (HP replicates). */
	WILDREACH_API float SwingScale(const AActor* Who);
	/** Red lightning: Chain lightning's hits inside this scope may bleed. */
	struct WILDREACH_API FChainScope
	{
		explicit FChainScope(const AActor* By);
		~FChainScope();
	};

	// ---------------------------------------------------------------- the HUD (owning client)
	/** The garnet Spilled segment drawn over the HP bar's fill (the rally window as a fading edge). */
	WILDREACH_API TSharedRef<SWidget> MakeSpilledOverlay(TFunction<const AActor*()> Who);
	/** A Blood slot that can't be paid now (HP <= its cost): OutWhy "needs 10 HP". Path/Node from the quick slot. */
	WILDREACH_API bool SlotShort(const AActor* Who, FName Path, FName Node, FString& OutWhy);
	/** 0..1: how much of a refused Blood press's shake is left on Who's slot (combat.md 2.3: no dead button). */
	WILDREACH_API float RefusedShake(const AActor* Who);
	/** The local press was refused (a cost check on the owner). */
	void NoteRefused(const AActor* Who);

	// ---------------------------------------------------------------- the body (every machine, WRHeroLook)
	/** Changes when the Blood look's live parts change (Spilled, Sanguine lord, a lifesteal flush, the low-health pulse). */
	WILDREACH_API uint32 LookSig(const AActor* Hero);
	/** The 2.18 stages over what Data/player_art.json "paths.blood" already drew: veins darkening with Spilled, the
	 *  porcelain clarity, the garnet iris ring and fill, the heartbeat pulse, the lifesteal flush, Sanguine lord's glow. */
	WILDREACH_API void ApplyLook(const AActor* Hero, const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Skin, const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Eyes,
		const TMap<FName, FLinearColor>& EyeDefaultColors, FName Main, float MainS, FName Sub, float SubS);
}

/**
 * The Blood path's spend and heal, for the abilities and for other systems (the Murkjaw's blood taming spends through
 * SpendBlood). Server-side; the queries work wherever the attributes and the path component replicate.
 */
UCLASS()
class WILDREACH_API UWRBloodPath : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Server: pay Cost HP. Fails (false, nothing taken) if HP <= Cost; else HP -= Cost (a cast may leave 1 HP: no floor),
	 * and with bSpilled it goes into the Spilled bar and the rally window starts again (8 s, 12 s with Clotting).
	 * Adrenaline / Cold blooded turn it into Charge / Chill. Cost <= 0 always succeeds.
	 */
	static bool SpendBlood(AWRCharacterBase* Who, float Cost, bool bSpilled = true);
	/** Either machine: Who could pay Cost now (HP > Cost). */
	static bool CanSpend(const AActor* Who, float Cost);
	/** Server: heal Who by Amount (x its healing taken). bLifesteal: refills Spilled at x2 inside the window first, then x1;
	 *  the threads and the body's flush play (From: where it was drawn from). */
	static void Heal(AWRCharacterBase* Who, float Amount, bool bLifesteal, const AActor* From = nullptr);

	/** HP in the Spilled bar now; seconds the rally window has left; that as 0..1 of the window. */
	static float Spilled(const AActor* Who);
	static float WindowLeft(const AActor* Who);
	static float WindowFraction(const AActor* Who);
	/** Points spent in the Blood path while Blood is Who's main path or subclass (0 otherwise): the Murkjaw's blood rite
	 *  asks 10+ (cooks/biome-swamp.md 2.12.1; pair it with SpendBlood for the 5 HP a second into its mouth). */
	static int32 BloodPoints(const AActor* Who);
	/** Blood nodes Who has learned (any rank), while Blood is their main path or subclass. */
	static int32 BloodNodes(const AActor* Who);
	/** Blood is Who's main path or subclass / its main path. */
	static bool Walks(const AActor* Who);
	static bool IsMain(const AActor* Who);
	/** A Blood node's HP cost (spells.<camelNode>.hpCost; 0 if none). */
	static float HpCost(FName NodeId);
	/** Server: Doom's mark on Target by By (bursts after spells.doom.delay s). */
	static void MarkDoom(AWRCharacterBase* By, AWRCharacterBase* Target);
	/** Server, Blood boil: the bleed burst Target would take from Source's bleed now (Hemorrhage included). */
	static float BleedBurst(const AWRCharacterBase* Target, const AActor* Source);
};
