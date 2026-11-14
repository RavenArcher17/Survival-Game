#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

class AActor;
class AWRCharacterBase;
class UWRStatusComponent;
struct FWRHit;

/**
 * The Lightning path's rules (cooks/biome-lightning.md 2.18, combat.md 2.11-2.13): Charge (the builder resource), the
 * Grounded root passive, the passive nodes the combat code asks about, the procs that ride on a landed blow, and the
 * Fire + Lightning / Frost + Lightning combos. Numbers: tuning resources.charge*, passives.*, nodeEffects.lightning.*,
 * spells.*, wildbrands.*, and Data/combos.json. Everything that changes the world runs on the server.
 */
namespace WRLightning
{
	// The path's own tags (defined in WRLightningPath.cpp).
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_StaticStrike);   // while a Static strike swing is out
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_ChargedWeapon);  // while a Charged weapon swing is out
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_ThunderGod);      // Thunder god's self buff
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_BoltDash);         // dashing as lightning (passes through bodies)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_Plasma);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_Meltdown);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_BrittleShock);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_BlizzardStorm);

	// ---------------------------------------------------------------- Grounded (the root passive, main path only)
	/** The Lightning path is this survivor's main path: no stun or stagger from any lightning source (storm strikes,
	 *  Kharvox's and Galvara's strikes, shock, another player's Thunderclap). The strike code asks this. */
	WILDREACH_API bool IsGrounded(const AActor* Who);
	/** Storm-strike damage on Who: passives.groundedStrikeMult (0.5) when Grounded, else 1. */
	WILDREACH_API float StrikeDamageMult(const AActor* Who);

	// ---------------------------------------------------------------- Charge
	/** Charge / MaxCharge (0..1) on any machine that has the attributes (the owner and the server). */
	WILDREACH_API float ChargeFraction(const AActor* Who);
	WILDREACH_API float ChargeValue(const AActor* Who);
	/** Charge >= resources.chargeHighAt (60): +20% lightning damage, +5% move speed, the bar crackles. */
	WILDREACH_API bool HighCharge(const AActor* Who);
	/** Server: add Charge (Recharge, corruption, prestige and gear scale it); it stops at full. */
	WILDREACH_API void GainCharge(AWRCharacterBase* Who, float Amount);
	/** Server: take up to Amount of Charge; returns what was taken. */
	WILDREACH_API float SpendCharge(AWRCharacterBase* Who, float Amount);
	/** Server, every tick from the status component: the decay after chargeDecayDelay s without a gain. */
	void TickCharge(AWRCharacterBase* Who, float SinceGain, float DeltaTime);

	// ---------------------------------------------------------------- passives the combat code asks
	/** Damage x for a lightning blow by Attacker on Target: high Charge, Overcharge, Conductive (soaked). Also a Charged
	 *  weapon's stored Charge on its own blow. */
	float DamageScale(const AWRCharacterBase* Attacker, const AWRCharacterBase* Target, const FWRHit& Hit);
	/** Move speed x (Swift and high Charge, capped at passives.moveSpeedCap over 1): both machines read it (prediction). */
	WILDREACH_API float MoveSpeedScale(const AWRCharacterBase* Who);
	/** Rapid fire at Charge >= passives.rapidFireAt: light / heavy swings play this many times faster (1 = normal). */
	WILDREACH_API float SwingRate(const AActor* Who);
	/** Rapid fire: spell cooldowns x this (1 = normal). */
	WILDREACH_API float SpellCooldownScale(const AActor* Who);

	/** Server: a blow landed (Result Hit): Shock chance, Static strike's arc, Charged weapon's shock, Thunder god's bolt,
	 *  Plasma. Damage: what it dealt. */
	void AfterHit(AWRCharacterBase* Target, const FWRHit& Hit, float Damage);
	/** Server: Target blocked (or parried) Attacker's blow: Discharge. */
	void OnGuard(AWRCharacterBase* Target, AWRCharacterBase* Attacker, bool bParry);
	/** Server: Overheat's blast hit Victim: Meltdown chains lightning from it. */
	void OnOverheatHit(AWRCharacterBase* Caster, AWRCharacterBase* Victim);
	/** Server: a shock proc on Owner by Source: Brittle shock shatters a frozen body. */
	void OnShockProc(AWRCharacterBase* Owner, AWRCharacterBase* Source);
	/** Server, every tick for a survivor: Afterimage (a dodge began), Blizzard storm (your Blizzard is up). */
	void TickOwner(AWRCharacterBase* Who, float DeltaTime);

	// ---------------------------------------------------------------- shared by the abilities
	/** Server: one lightning hit (spells, chains, procs): element, shock buildup, the path's XP, the violet flash. */
	void Zap(AWRCharacterBase* By, AWRCharacterBase* Target, float Damage, float Shock, const FVector& Origin, bool bSpell, bool bReaction, float StanceDamage = -1.f, float Poise = 8.f);
	/** Server: chain from From through up to Jumps more enemies (nearest not yet hit within Radius of the last), each
	 *  hit by Damage and Shock, a ribbon arc drawn between them. Already: bodies to skip (the first target). */
	void Chain(AWRCharacterBase* By, AWRCharacterBase* From, int32 Jumps, float Radius, float Damage, float Shock, TSet<AWRCharacterBase*>& Already, bool bReaction);
	/** Every machine: the violet-white ribbon from A to B (Data/fx.json lightning.arc), with the zip. Server calls. */
	void Ribbon(const UObject* WorldContext, const FVector& A, const FVector& B, bool bSound);
	/** Server: call a bolt down on Spot through the storm code (WRStorm::CallStrike), as By's own ability. */
	bool CallBolt(AWRCharacterBase* By, const FVector& Spot, float Damage, float Shock, float Tell, float Radius);

	/** Server: Charged weapon's stored Charge (0..1) for By's current swing (set as the blow comes out). */
	void SetStoredCharge(const AActor* By, float Fraction);
	float StoredCharge(const AActor* By);
}
