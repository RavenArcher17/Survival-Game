#include "WRCombatRules.h"
#include "WRCharacterBase.h"
#include "WRGameplayTags.h"
#include "WRLag.h"
#include "Engine/World.h"

namespace WRCombatRules
{
	float ToleranceFor(const AWRCharacterBase* Who)
	{
		// The server opened the window when it heard of the swing, a one-way trip after the player started it: a press
		// the player made inside its own window may reach the server just before the server's opens.
		return Who && Who->HasAuthority() ? UWRLagSubsystem::WindowTolerance(Who) : 0.f;
	}

	bool Allows(const AWRCharacterBase* Who, EAct Act, float Tolerance)
	{
		if (!Who || !Who->IsAlive())
		{
			return false;
		}
		// Held: stunned, frozen, knocked out, in someone's crit, or doing one.
		if (Who->HasTag(WRTags::Status_Staggered) || Who->HasTag(WRTags::Status_Frozen) || Who->HasTag(WRTags::Status_Unconscious)
			|| Who->HasTag(WRTags::State_BeingCrit) || Who->HasTag(WRTags::State_Critting) || Who->HasTag(WRTags::State_ParryRecovery))
		{
			return false;
		}
		// A blocked blow's push-back: the guard stays up, nothing else starts.
		if (Who->HasTag(WRTags::State_BlockStun) && Act != EAct::Guard)
		{
			return false;
		}
		// Mid-roll: nothing until it ends (what's pressed waits in the buffer).
		if (Who->HasTag(WRTags::State_Dodging))
		{
			return false;
		}
		// Mid-swing: the next chain attack from the combo window, a roll or the guard from the cancel window.
		if (Who->IsSwinging())
		{
			const double Now = Who->GetWorld()->GetTimeSeconds() + Tolerance;
			if (Act == EAct::Attack || Act == EAct::Critical)
			{
				return Now >= Who->Swing.ComboAt;
			}
			return Now >= Who->Swing.CancelAt;
		}
		return true;
	}
}
