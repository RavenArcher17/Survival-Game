#include "WRGameplayTags.h"

namespace WRTags
{
	UE_DEFINE_GAMEPLAY_TAG(State_Dead, "State.Dead");
	UE_DEFINE_GAMEPLAY_TAG(State_Invulnerable, "State.Invulnerable");
	UE_DEFINE_GAMEPLAY_TAG(State_Blocking, "State.Blocking");
	UE_DEFINE_GAMEPLAY_TAG(State_Parrying, "State.Parrying");
	UE_DEFINE_GAMEPLAY_TAG(State_Attacking, "State.Attacking");
	UE_DEFINE_GAMEPLAY_TAG(State_Dodging, "State.Dodging");
	UE_DEFINE_GAMEPLAY_TAG(State_ComboWindow, "State.ComboWindow");
	UE_DEFINE_GAMEPLAY_TAG(State_CancelWindow, "State.CancelWindow");
	UE_DEFINE_GAMEPLAY_TAG(State_IFrames, "State.IFrames");
	UE_DEFINE_GAMEPLAY_TAG(State_TraceActive, "State.TraceActive");
	UE_DEFINE_GAMEPLAY_TAG(State_Windup, "State.Windup");
	UE_DEFINE_GAMEPLAY_TAG(State_Hyperarmor, "State.Hyperarmor");
	UE_DEFINE_GAMEPLAY_TAG(State_BlockStun, "State.BlockStun");
	UE_DEFINE_GAMEPLAY_TAG(State_GuardCounterWindow, "State.GuardCounterWindow");
	UE_DEFINE_GAMEPLAY_TAG(State_ParryRecovery, "State.ParryRecovery");
	UE_DEFINE_GAMEPLAY_TAG(State_JumpLegsInvuln, "State.JumpLegsInvuln");
	UE_DEFINE_GAMEPLAY_TAG(State_BeingCrit, "State.BeingCrit");
	UE_DEFINE_GAMEPLAY_TAG(State_Critting, "State.Critting");

	UE_DEFINE_GAMEPLAY_TAG(Status_Burn, "Status.Burn");
	UE_DEFINE_GAMEPLAY_TAG(Status_Frostbite, "Status.Frostbite");
	UE_DEFINE_GAMEPLAY_TAG(Status_Slowed, "Status.Slowed");
	UE_DEFINE_GAMEPLAY_TAG(Status_Rooted, "Status.Rooted");
	UE_DEFINE_GAMEPLAY_TAG(Status_Frozen, "Status.Frozen");
	UE_DEFINE_GAMEPLAY_TAG(Status_Staggered, "Status.Staggered");
	UE_DEFINE_GAMEPLAY_TAG(Status_Unconscious, "Status.Unconscious");
	UE_DEFINE_GAMEPLAY_TAG(Status_StanceBroken, "Status.StanceBroken");
	UE_DEFINE_GAMEPLAY_TAG(Status_GuardBroken, "Status.GuardBroken");
	UE_DEFINE_GAMEPLAY_TAG(Status_Exposed, "Status.Exposed");
	UE_DEFINE_GAMEPLAY_TAG(Status_Flinch, "Status.Flinch");

	UE_DEFINE_GAMEPLAY_TAG(Status_Poison, "Status.Poison");
	UE_DEFINE_GAMEPLAY_TAG(Status_Rot, "Status.Rot");
	UE_DEFINE_GAMEPLAY_TAG(Status_Shock, "Status.Shock");
	UE_DEFINE_GAMEPLAY_TAG(Status_Soak, "Status.Soak");
	UE_DEFINE_GAMEPLAY_TAG(Status_Blind, "Status.Blind");
	UE_DEFINE_GAMEPLAY_TAG(Status_Decay, "Status.Decay");
	UE_DEFINE_GAMEPLAY_TAG(Status_Mark, "Status.Mark");
	UE_DEFINE_GAMEPLAY_TAG(Status_Curse, "Status.Curse");
	UE_DEFINE_GAMEPLAY_TAG(Status_Rimed, "Status.Rimed");
	UE_DEFINE_GAMEPLAY_TAG(Status_Bleeding, "Status.Bleeding");
	UE_DEFINE_GAMEPLAY_TAG(State_Drinking, "State.Drinking");
	UE_DEFINE_GAMEPLAY_TAG(State_Charging, "State.Charging");
	UE_DEFINE_GAMEPLAY_TAG(State_Casting, "State.Casting");
	UE_DEFINE_GAMEPLAY_TAG(State_Mounted, "State.Mounted");
	UE_DEFINE_GAMEPLAY_TAG(State_CombatTagged, "State.CombatTagged");

	UE_DEFINE_GAMEPLAY_TAG(Element_Physical, "Element.Physical");
	UE_DEFINE_GAMEPLAY_TAG(Element_Fire, "Element.Fire");
	UE_DEFINE_GAMEPLAY_TAG(Element_Frost, "Element.Frost");
	UE_DEFINE_GAMEPLAY_TAG(Element_Lightning, "Element.Lightning");
	UE_DEFINE_GAMEPLAY_TAG(Element_Water, "Element.Water");

	UE_DEFINE_GAMEPLAY_TAG(Path_Fire, "Path.Fire");
	UE_DEFINE_GAMEPLAY_TAG(Path_Frost, "Path.Frost");

	UE_DEFINE_GAMEPLAY_TAG(Passive_BurningBlood, "Passive.BurningBlood");
	UE_DEFINE_GAMEPLAY_TAG(Passive_Permafrost, "Passive.Permafrost");
	UE_DEFINE_GAMEPLAY_TAG(Passive_Grounded, "Passive.Grounded");
	UE_DEFINE_GAMEPLAY_TAG(Combo_Thaw, "Combo.Thaw");
	UE_DEFINE_GAMEPLAY_TAG(Combo_Balance, "Combo.Balance");

	UE_DEFINE_GAMEPLAY_TAG(Resist_Lava, "Resist.Lava");
	UE_DEFINE_GAMEPLAY_TAG(Resist_Ice, "Resist.Ice");

	UE_DEFINE_GAMEPLAY_TAG(Ability_Light, "Ability.Light");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Heavy, "Ability.Heavy");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Dodge, "Ability.Dodge");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Block, "Ability.Block");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Path, "Ability.Path");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Melee, "Ability.Melee");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Critical, "Ability.Critical");
	UE_DEFINE_GAMEPLAY_TAG(Event_PerfectDodge, "Event.PerfectDodge");

	FGameplayTag PathTag(FName PathId)
	{
		if (PathId == TEXT("fire")) return Path_Fire;
		if (PathId == TEXT("frost")) return Path_Frost;
		return FGameplayTag();
	}
}
