#pragma once

#include "CoreMinimal.h"

class AWRCharacterBase;

/**
 * What a fighter may start right now (COMBAT-AND-LESSONS 1.1 "animation commitment is the rule", 1.16; cooks/
 * build-combat-core.md 3.2). One function for the abilities (server and the owner's prediction) and the input buffer,
 * so a press the buffer lets through is never refused by the ability, and vice versa.
 */
namespace WRCombatRules
{
	enum class EAct : uint8
	{
		Attack,   // light, heavy, weapon arts, spells
		Dodge,    // roll, backstep
		Guard,    // block / parry
		Critical, // riposte, backstab (from Light)
	};

	/** Tolerance: seconds a window may be early by (the server's leeway for a remote player, UWRLagSubsystem). */
	WILDREACH_API bool Allows(const AWRCharacterBase* Who, EAct Act, float Tolerance = 0.f);
	/** The tolerance the machine running the check should use for Who. */
	WILDREACH_API float ToleranceFor(const AWRCharacterBase* Who);
}
