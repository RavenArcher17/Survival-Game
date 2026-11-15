#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

class AActor;
class AWRCharacterBase;
class UMaterialInstanceDynamic;
struct FWRHit;

/**
 * The Nature path's rules (cooks/biome-forest.md 2.17-2.18, combat.md 2.11-2.13): Growth (a spender that regrows by the
 * ground around you), the Rooted root passive, Entangled (the WRForest contract, on top of Status.Rooted), the passive
 * nodes the combat code asks about (Entangle chance, Lifebloom, Purify, Barbs, Thorned skin, Briar spread, Pollen), the
 * procs on a landed blow, and the four Nature combos (Data/combos.json). Numbers: tuning resources.growth*, passives.*,
 * nodeEffects.nature.*, spells.*, wildbrands.*, status.entangled, combos.json. Everything that changes the world runs on
 * the server; what every machine must read (the ground under you, Rooted's heal) rides on replicated loose tags.
 */
namespace WRNature
{
	// The path's own tags (defined in WRNaturePath.cpp).
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Element_Nature);         // a Nature spell's or Wildbrand's element
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Passive_Rooted);         // the root passive (main path only)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_RootedHeal);      // Rooted is healing right now (the bark's green tint, every machine)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ground_Lush);            // the ground the sampler read last (the HUD's ground icon)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ground_Normal);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ground_Barren);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_BriarStrike);    // while a Briar strike swing is out
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_VineLash);       // while a Vine lash is out
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_SmoulderRoots);    // Fire + Nature 25
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_ForestFire);       // Fire + Nature 50
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_Evergreen);        // Frost + Nature 25
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_FrozenGrove);      // Frost + Nature 50
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_Quickening);       // Lightning + Nature 25
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_LivingConductor);  // Lightning + Nature 50
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_RedSap);           // Nature + Blood 25 (data-ready: the Blood path is the swamp's)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_ThornFeast);       // Nature + Blood 50

	/** The actor tag every plant carries (plant summons; crops, hedges and forest trees may carry it too): within the
	 *  sampler's radius it makes the ground Lush. */
	WILDREACH_API FName PlantTag();

	/** What the ground around a survivor gives (cooks/biome-forest.md 2.17: Lush 6/s, Normal 3/s, Barren 1/s). */
	enum class EGround : uint8 { Barren, Normal, Lush };

	// ---------------------------------------------------------------- Growth
	/** This survivor walks the Nature path (main or subclass): only they gather and spend Growth. */
	WILDREACH_API bool Walks(const AActor* Who);
	WILDREACH_API float GrowthValue(const AActor* Who);
	/** Growth / MaxGrowth (0..1) on any machine that has the attributes (the owner and the server). */
	WILDREACH_API float GrowthFraction(const AActor* Who);
	/** The owner and the server: Who has at least Amount of Growth. */
	WILDREACH_API bool HasGrowth(const AActor* Who, float Amount);
	/** Server: take Amount of Growth (all or nothing). True when it was paid. */
	WILDREACH_API bool SpendGrowth(AWRCharacterBase* Who, float Amount);
	/** The owning machine: a press was refused for Growth (the gauge flashes, no dead button). */
	WILDREACH_API void NoteRefused(const AActor* Who);
	/** 0..1: how much of the "not enough Growth" flash is left on this machine. */
	WILDREACH_API float RefusedFlash(const AActor* Who);
	/** The ground under Who by the last sample: the server's own, a client's from the replicated tag. */
	WILDREACH_API EGround GroundOf(const AActor* Who);
	/** Server: classify the ground within the sampler's radius of Feet (biome, forest sub-zone, caves, floors, plants). */
	WILDREACH_API EGround ClassifyGround(const AActor* Who, const FVector& Feet);

	/** Server, every tick for a survivor (the status component): the Growth sampler and regen, Rooted. */
	void TickOwner(AWRCharacterBase* Who, float DeltaTime);

	// ---------------------------------------------------------------- healing (Lifebloom, Purify, Red sap)
	/** Server: Nature healing from Healer onto Target (a plant's or a spell's): x Lifebloom, x the target's healing taken.
	 *  bPurify: a Regrowth / Sanctuary / World tree tick (Purify takes 20 poison and 20 rot buildup off; bFirstTick also
	 *  ends an active poison or rot). Red sap heals the healer 30% of what others got. Returns what it healed. */
	WILDREACH_API float Heal(AWRCharacterBase* Healer, AWRCharacterBase* Target, float Amount, bool bPurify, bool bFirstTick);
	/** The survivor behind a blow or a heal: a plant's planter, else the actor itself. */
	WILDREACH_API AWRCharacterBase* Caster(AActor* By);
	/** Is Who a friend of Healer's for a heal (not hostile, alive, not a wild creature)? */
	WILDREACH_API bool IsAlly(const AWRCharacterBase* Healer, const AWRCharacterBase* Who);

	/** Server: a healing zone (Sanctuary's pool, a World tree's crown) of Owner's at Zone's location: World tree's damage
	 *  taken and Frozen grove read them. Gone when Zone goes. bShield: damage taken x (1 + spells.worldTree.damageTaken). */
	void AddHealZone(AActor* Zone, AWRCharacterBase* Owner, float Radius, bool bShield);

	// ---------------------------------------------------------------- passives the combat code asks
	/** Damage taken x for Target (an ally inside a World tree's crown: -20%). */
	float DamageTakenScale(const AWRCharacterBase* Target, const FWRHit& Hit);
	/** Extra poise while braced (Frozen grove: allies in your Sanctuary or under your World tree +15). */
	float BonusPoise(const AWRCharacterBase* Target);
	/** Status buildup taken x (Frozen grove: frostbite x0.5 in your zones). Id: WRStatus::EId. */
	float BuildupTakenScale(const AWRCharacterBase* Who, int32 Id);
	/** Move speed x: Pollen near an enemy's plants (x0.8, bosses x0.9) and an Entangle's boss slow (server). Both
	 *  machines read the pollen (the plants and their pollen flag replicate), so a player's prediction agrees. */
	WILDREACH_API float MoveSpeedScale(const AWRCharacterBase* Who);

	/** Server: a blow landed (Result Hit, not damage over time): Entangle chance, Briar strike's roots. */
	void AfterHit(AWRCharacterBase* Target, const FWRHit& Hit, float Damage);
	/** Server: after any blow's damage (also blocked ones): Thorned skin bites back on a landed melee blow; any damage
	 *  resets Rooted's stillness. */
	void AfterDamage(AWRCharacterBase* Target, const FWRHit& Hit, float Damage, bool bLanded);
	/** Server, just before a blow's buildups go on: remembers a Nature hit's bleed (Barbs, Thorn feast read it). */
	void NoteBuildups(AWRCharacterBase* Target, const FWRHit& Hit);
	/** Server: a bleed burst went off on Owner from Source (the status component): Barbs, Thorn feast. */
	void OnBleedBurst(AWRCharacterBase* Owner, AWRCharacterBase* Source, float Burst);
	/** Server: a shock proc on Owner from Source: Quickening (your plants fire faster). */
	void OnShockProc(AWRCharacterBase* Owner, AWRCharacterBase* Source);

	/** Server: one Nature hit (spells, vines, procs): element, bleed buildup, the path's XP. */
	void NatureHit(AWRCharacterBase* By, AWRCharacterBase* Target, float Damage, float Bleed, const FVector& Origin, bool bSpell, bool bReaction,
		float StanceDamage = -1.f, float Poise = 8.f, uint8 PhysType = 0);
	/** Server: the Entangle with every option (the contract's WRForest::Entangle is this with bIgnoreImmunity false).
	 *  BossSlow / BossSlowSeconds < 0: status.entangled.bossSlow / bossSlowSeconds. */
	bool EntangleEx(AActor* Target, float Seconds, AActor* Source, bool bIgnoreBossRule, float BossSlow = -1.f, float BossSlowSeconds = -1.f);
	/** Server: a slow on a body (an Entangle's boss fallback, Strangling vines on a major boss): Mult for Seconds. */
	void SlowBody(AWRCharacterBase* Who, float Mult, float Seconds);
	/** Briar spread: root areas x (1 + 0.15 a rank). */
	WILDREACH_API float RootRadiusScale(const AActor* Who);

	// ---------------------------------------------------------------- the body (cooks/biome-forest.md 2.18)
	/** What changes the Nature look on this machine (Rooted's tint): a part of the hero's path-mark signature. */
	uint32 LookSig(const AActor* Hero);
	/** Every machine: the Nature stages and live parts over what the player_art row drew (bark tint, iris by stage,
	 *  sun-warm skin at 0.6, Rooted's green tint while it heals). */
	void ApplyLook(const AActor* Hero, const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Skin, const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Eyes,
		const TMap<FName, FLinearColor>& EyeDefaultColors, FName Main, float MainS, FName Sub, float SubS);
	/** Every machine with looks: the small leaf buds at the temples from strength 1.0 (main path; 2.18), two modelled buds
	 *  riding Body's head bone (player_art paths.nature.look.templeBud), hidden when bShow is false (first person). */
	void ApplyTempleBuds(AActor* Hero, class USkeletalMeshComponent* Body, bool bShow, FName Main, float MainS);
}
