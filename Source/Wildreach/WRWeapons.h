#pragma once

#include "CoreMinimal.h"
#include "WRCombatTypes.h"

class AWRCharacterBase;

/**
 * Weapon classes, their moves and the guard tools, from Data/weapons.json (cooks/build-combat-core.md 3.1). One move row
 * times a swing on every machine (the server's timers, the owner's prediction, the clip warp everyone draws), so the
 * numbers live in one place. Read once; WR.ReloadTuning re-reads it.
 */
struct FWRMove
{
	FName Id;
	float Windup = 0.25f;
	float Active = 0.18f;
	float Recovery = 0.3f;
	float Damage = 1.f;           // x the weapon's light (or heavy, bHeavy) damage
	float Stance = 5.f;           // stance damage to creatures
	float Stamina = 10.f;
	float Poise = 30.f;           // poise damage to players
	float GuardStamina = 30.f;    // a block's cost before the guard boost
	uint8 Impact = 2;
	FName Next;                   // the light chain's next move
	int32 Clip = 0;               // which of the hero's light / heavy clips
	bool bHyperarmor = false;
	bool bHeavy = false;
	EWRHitTier Tier = EWRHitTier::Light;
	// Paired crits: total length and the damage frame.
	float Total = 0.f;
	float DamageAt = 0.f;
	// --- Full attack set (cooks/build-combat-full.md 2.1-2.4) ---
	/** 1 slash, 2 strike, 3 pierce (creatures.json physResist). */
	uint8 PhysType = 1;
	/** The held-Heavy charged swing (damage grows with the hold: combat.charge). */
	bool bCharge = false;
	/** Poise multiplier of this move's hyperarmor (0: tuning poise.hyperarmor.heavy). */
	float HyperarmorMult = 0.f;
	/** Where the row is picked: run, roll, backstep, air, crouch, guard (none: the chain / heavy). */
	FName Context;
	/** Spear guard poke: the guard stays up through it. */
	bool bKeepsGuard = false;
	/** > 0: this row's own reach (a spear's longer lunge). */
	float Reach = 0.f;

	float Length() const { return Windup + Active + Recovery; }
};

/** A guard: the carried off-hand's, or the weapon's own with nothing on the left arm. */
struct FWRGuard
{
	FName Id;
	FString Name;
	float Boost = 25.f;           // Guard Boost: a block costs staminaDamage x (1 - Boost / 100)
	float Phys = 0.6f;            // physical damage stopped
	float Element = 0.3f;         // element damage stopped
	float ParryStartup = 0.13f;
	float ParryActive = 0.10f;
	float ParryRecovery = 0.5f;
	float BlockStunScale = 1.f;
	float Weight = 0.f;
	bool bCanParry = true;
};

struct FWRWeaponClass
{
	FName Id;
	FString Name;
	float Reach = 220.f;
	float SweepRadius = 45.f;
	float CritBackstab = 2.3f;
	float CritRiposte = 3.45f;
	float CritStance = 3.f;
	FWRGuard Guard;
	/** none / 1h / 2h; great weapons are always two-handed and cancel later (CancelFraction). */
	FName Hands = TEXT("1h");
	bool bGreat = false;
	float Weight = 0.f;
	/** Copper base damage of a light / heavy hit (the sword's 12 / 22). */
	float DamageLight = 12.f;
	float DamageHeavy = 22.f;
	/** > 0: the share of the recovery after which a roll or guard cuts the swing (great weapons 0.75). */
	float CancelFraction = 0.f;
	/** Player-vs-player class multiplier (tuning pvp). */
	float PvpScale = 1.f;
	/** Innate status buildup per hit (dagger bleed...). */
	TMap<FName, float> Buildups;
	TMap<FName, FWRMove> Moves;

	const FWRMove* Move(FName MoveName) const { return Moves.Find(MoveName); }
};

class WILDREACH_API FWRWeapons
{
public:
	static FWRWeapons& Get();
	void Reload();

	/** A class by id (unknown: the sword). */
	const FWRWeaponClass& Class(FName Id) const;
	/** The class of what a character swings: its equipped weapon's items.json "class" (none, tools, creatures: sword). */
	const FWRWeaponClass& ClassFor(const AWRCharacterBase* Character) const;
	/** The guard a character blocks with: its carried off-hand (survivors), else its weapon class's own. */
	FWRGuard GuardFor(const AWRCharacterBase* Character) const;
	/** An off-hand row (null if unknown). */
	const FWRGuard* Offhand(FName Id) const { return Offhands.Find(Id); }
	/** What the hero carries on the left arm (weapons.json "carried"). */
	FName Carried() const { return CarriedId; }
	/** The carried shield is on the arm: a survivor that isn't two-handing (combat.md 2.4.0). */
	bool HasShield(const AWRCharacterBase* Character) const;
	/** Two-handing: by choice (bTwoHanded), or always with a 2h / great weapon. */
	static bool IsTwoHanding(const AWRCharacterBase* Character);
	/** Copper base damage of the class's light or heavy (x the row's dmg, metal, quality...). */
	static float ClassBase(const FWRWeaponClass& Class, bool bHeavy) { return bHeavy ? Class.DamageHeavy : Class.DamageLight; }

private:
	FWRWeapons();
	TMap<FName, FWRWeaponClass> Classes;
	TMap<FName, FWRGuard> Offhands;
	FName CarriedId;
	FWRWeaponClass Fallback;
};
