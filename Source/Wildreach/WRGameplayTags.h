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

	// Status effects (design review #6: full list official, fire/frost set built first)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Burn);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Frostbite);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Slowed);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Frozen);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Staggered);
	/** Knocked out by torpor (taming). */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Unconscious);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Element_Physical);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Element_Fire);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Element_Frost);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Path_Fire);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Path_Frost);

	// Root passives (main path only) and combos (both paths >= threshold)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Passive_BurningBlood);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Passive_Permafrost);
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

	/** Tag for a path id ("fire" -> Path.Fire); invalid tag for unknown ids. */
	FGameplayTag PathTag(FName PathId);
}
