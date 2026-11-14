#pragma once

#include "NativeGameplayTags.h"

namespace WRTags
{
	// States set while acting
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Invulnerable);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Blocking);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Parrying);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Attacking);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dodging);
	// Combat windows carried by the hero's clips (UWRANS_Window, Data/anim_windows.json).
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_ComboWindow);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_CancelWindow);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_IFrames);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_TraceActive);
	// Combat core (cooks/build-combat-core.md): set by the abilities and the combat library, replicated loose tags.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Windup);              // a swing before its strike (counter-hits, backstab rule)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Hyperarmor);          // poise x the move's hyperarmor, damage taken a little less
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_BlockStun);           // a blocked blow's push-back: no swing or roll
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_GuardCounterWindow);  // after block stun: Heavy is the guard counter
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_ParryRecovery);       // a parry tap's committed recovery
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_JumpLegsInvuln);      // airborne: low blows pass under
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_BeingCrit);           // held in a riposte / backstab
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Critting);            // performing one

	// Status effects (design review #6: full list official, fire/frost set built first)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Burn);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Frostbite);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Slowed);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Rooted);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Frozen);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Staggered);
	/** Knocked out by torpor (taming). */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Unconscious);
	/** Stance at zero (or a breaking parry): stunned, open to a critical from the front. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_StanceBroken);
	/** A player whose guard broke: open to a riposte for a moment (PvP). */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_GuardBroken);
	/** Just parried: takes more damage from everyone (Valheim's group reward). */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Exposed);
	/** Poise broke: a short reaction (flinch / stagger by the blow's impact). */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Flinch);

	// --- Full combat (cooks/build-combat-full.md): every status, the drink, charging, mounted, combat tag ---
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Poison);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Rot);        // also halves healing taken
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Shock);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Soak);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Blind);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Decay);      // healing x0.5 while it lasts (max HP loss stays until the fight / rest)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Mark);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Curse);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Rimed);      // bosses in place of Frozen: stance taken x1.5
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Bleeding);   // a moment after a bleed burst (Grimjaw's regen check, the HUD)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Drinking);    // a potion's committed drink (AI reads it)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Charging);    // a held charged heavy / Wildbrand / spell (AI reads it)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Casting);     // a spell's wind-up (AI reads it)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Mounted);     // riding: only mounted-flagged abilities
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_CombatTagged);// hit or hit by a player in the last pvp.combatTagSeconds

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Element_Physical);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Element_Fire);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Element_Frost);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Element_Lightning);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Element_Water);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Path_Fire);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Path_Frost);

	// Root passives (main path only) and combos (both paths >= threshold)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Passive_BurningBlood);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Passive_Permafrost);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Passive_Grounded);   // Lightning root passive: no shock stagger
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_Thaw);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_Balance);

	// Resistances (hazard gear checks)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Resist_Lava);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Resist_Ice);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Light);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Heavy);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Dodge);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Block);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Path);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Melee);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Critical);
	/** Gameplay event: a blow taken by a roll's i-frames right as it started (Frozen moment and other dodge nodes). */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_PerfectDodge);

	/** Tag for a path id ("fire" -> Path.Fire); invalid tag for unknown ids. */
	FGameplayTag PathTag(FName PathId);
}
