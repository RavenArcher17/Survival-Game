#include "WRGameplayTags.h"

namespace WRTags
{
	UE_DEFINE_GAMEPLAY_TAG(State_Dead, "State.Dead");
	UE_DEFINE_GAMEPLAY_TAG(State_Invulnerable, "State.Invulnerable");
	UE_DEFINE_GAMEPLAY_TAG(State_Blocking, "State.Blocking");
	UE_DEFINE_GAMEPLAY_TAG(State_Parrying, "State.Parrying");
	UE_DEFINE_GAMEPLAY_TAG(State_Attacking, "State.Attacking");
	UE_DEFINE_GAMEPLAY_TAG(State_Dodging, "State.Dodging");

	UE_DEFINE_GAMEPLAY_TAG(Status_Burn, "Status.Burn");
	UE_DEFINE_GAMEPLAY_TAG(Status_Frostbite, "Status.Frostbite");
	UE_DEFINE_GAMEPLAY_TAG(Status_Slowed, "Status.Slowed");
	UE_DEFINE_GAMEPLAY_TAG(Status_Frozen, "Status.Frozen");
	UE_DEFINE_GAMEPLAY_TAG(Status_Staggered, "Status.Staggered");

	UE_DEFINE_GAMEPLAY_TAG(Element_Physical, "Element.Physical");
	UE_DEFINE_GAMEPLAY_TAG(Element_Fire, "Element.Fire");
	UE_DEFINE_GAMEPLAY_TAG(Element_Frost, "Element.Frost");

	UE_DEFINE_GAMEPLAY_TAG(Path_Fire, "Path.Fire");
	UE_DEFINE_GAMEPLAY_TAG(Path_Frost, "Path.Frost");

	UE_DEFINE_GAMEPLAY_TAG(Passive_BurningBlood, "Passive.BurningBlood");
	UE_DEFINE_GAMEPLAY_TAG(Passive_Permafrost, "Passive.Permafrost");
	UE_DEFINE_GAMEPLAY_TAG(Combo_Thaw, "Combo.Thaw");
	UE_DEFINE_GAMEPLAY_TAG(Combo_Balance, "Combo.Balance");

	UE_DEFINE_GAMEPLAY_TAG(Resist_Lava, "Resist.Lava");
	UE_DEFINE_GAMEPLAY_TAG(Resist_Ice, "Resist.Ice");

	UE_DEFINE_GAMEPLAY_TAG(Ability_Light, "Ability.Light");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Heavy, "Ability.Heavy");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Dodge, "Ability.Dodge");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Block, "Ability.Block");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Path, "Ability.Path");

	FGameplayTag PathTag(FName PathId)
	{
		if (PathId == TEXT("fire")) return Path_Fire;
		if (PathId == TEXT("frost")) return Path_Frost;
		return FGameplayTag();
	}
}
