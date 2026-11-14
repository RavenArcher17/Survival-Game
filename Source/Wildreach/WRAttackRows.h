#pragma once

#include "CoreMinimal.h"

/**
 * One creature or boss attack (Data/attacks.json; COMBAT-AND-LESSONS 1.13, cooks/combat.md 2.18.1, 2.17,
 * cooks/build-combat-full.md 2.15-2.16). Creatures fight by their rows (AWRCreatureAI::TickRowFight); bosses read theirs
 * in AWRBoss::BuildAttacks. Every number is a first draft in the file.
 */
struct FWRAttackRow
{
	FName Id;
	/** What the body does: bite, claw, sweep, kick, lunge, charge, roll, slam, spit, stomp, burst, buffet, howl, dust. */
	FName Motion;
	float MinRange = 0.f;
	float MaxRange = 300.f;
	float Windup = 0.5f;
	/** Share of the wind-up spent turning toward the target (then it commits). */
	float TrackUntil = 0.5f;
	/** Held at the top of the swing (a delayed attack): the final cue plays 0.2 s before contact. */
	float Hold = 0.f;
	float Active = 0.2f;
	/** The recovery after it: no attack, open (the punish window). */
	float Punish = 0.8f;
	float Dmg = 1.f;              // creatures: x their bite damage; bosses: flat damage
	float Poise = 20.f;
	uint8 Impact = 2;
	float StaminaDamage = -1.f;   // < 0: from the damage
	TMap<FName, float> Buildups;
	uint8 PhysType = 0;           // 1 slash, 2 strike, 3 pierce
	bool bParryable = false;
	bool bLow = false;
	bool bUnblockable = false;
	bool bGrab = false;
	bool bIgnoreHyperarmor = false;
	bool bBehindOnly = false;
	bool bReadsAction = false;
	float ProjectileSpeed = 1000.f;
	float Knockback = 0.f;
	float Reach = 60.f;           // past its own radius (bite, claw, sweep, kick, contact for lunges and charges)
	float Arc = 45.f;             // half-angle of a bite / claw / sweep (degrees)
	float Radius = 0.f;           // stomp ring, burst, dust cloud, slam line width
	float Speed = 0.f;            // charge / roll / slam travel (cm/s; 0 = x2 its speed)
	float WallStun = 0.f;         // > 0: a charge into a wall stuns it this long
	FName Next;                   // the combo's next row
	float NextChance = 0.f;
	float Cooldown = 3.f;
	float WClose = 1.f, WMid = 1.f, WFar = 1.f;  // pick weights by range band (< 400, < 1200, beyond)
	FName TellBone = TEXT("head");
	uint8 Glint = 1;              // 0 none, 1 white (blockable), 2 crimson (unblockable)
	FName Clip;                   // which attack clip (attack / attack2 / attack3); none: in turn
	// --- Bosses ---
	FName Shape;                  // cone, line, self, target
	float Range = 400.f;          // the pick test and the reach limit
	float Size = 300.f;           // radius / half-angle / half-width
	int32 Repeats = 1;
	float Gap = 0.55f;
	int32 Phase = 1;              // from this phase
	int32 MaxPhase = 9;           // until this phase
	uint8 Height = 0;             // 0 ground, 1 airborne, 2 either
	bool bTamePunish = false;     // goes for the furthest tame first
	bool bTransition = false;     // the phase-change attack (once, at the threshold)
	float DamageTaken = 1.f;      // the boss's damage taken while it plays (a transition 0.5)
	float HoldOn = -1.f;          // a combo's held hit: Hold applies on this repeat (1-based); < 0 every repeat
	bool bThrown = false;         // a projectile on a target circle: a falling shadow shows where
	bool bMelee = true;           // blockable as a melee blow
};

class WILDREACH_API FWRAttackRows
{
public:
	static FWRAttackRows& Get();
	void Reload();
	/** A creature's rows by id (null: it fights the old way). */
	const TMap<FName, FWRAttackRow>* Creature(FName CreatureId) const { return Creatures.Find(CreatureId); }
	/** A boss's rows, in file order (null: its built-in moves). */
	const TArray<FWRAttackRow>* Boss(FName BossId) const { return Bosses.Find(BossId); }

private:
	FWRAttackRows();
	TMap<FName, TMap<FName, FWRAttackRow>> Creatures;
	TMap<FName, TArray<FWRAttackRow>> Bosses;
};
