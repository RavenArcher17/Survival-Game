#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

/** One rebindable action: what it's called, its group in Settings, the default key and the current one. */
struct FWRActionKey
{
	FName Id;
	FText Label;
	FName Group;   // Movement, Combat, Items, Menus, Abilities
	FKey Default;
	FKey Key;
};

/** How path abilities are cast (Settings > Controls). */
enum class EWRCastMode : uint8 { Wheel, Keys, Both };

/**
 * Every key the game listens to, rebindable (saved in GameUserSettings.ini, [Wildreach.Keys]). The character's
 * input mapping and the controller's key bindings are built from it and rebuilt when it changes (OnChanged).
 * Abilities get their own optional keys too ("Ability.<node id>", unbound by default).
 */
class WILDREACH_API FWRKeyBindings
{
public:
	static FWRKeyBindings& Get();

	const TArray<FWRActionKey>& Actions() const { return List; }
	const FWRActionKey* Find(FName Id) const;
	FKey Key(FName Id) const;
	/** Give an action a key. Another action that had it is unbound; its id is returned (NAME_None if none). */
	FName Bind(FName Id, const FKey& NewKey);
	/** The action using a key (other than Except), or NAME_None. */
	FName WhoUses(const FKey& InKey, FName Except = NAME_None) const;
	void Unbind(FName Id);
	void ResetAll();
	/** Make sure an ability has an (unbound by default) entry. */
	void EnsureAbility(FName NodeId, const FText& Label);
	static FName AbilityAction(FName NodeId) { return FName(*(TEXT("Ability.") + NodeId.ToString())); }

	EWRCastMode GetCastMode() const { return CastMode; }
	void SetCastMode(EWRCastMode Mode);
	bool CastOnRelease() const { return bCastOnRelease; }
	void SetCastOnRelease(bool b);
	/** The crosshair stays up all the time (off: only while aiming, placing or in first person). */
	bool ShowCrosshair() const { return bShowCrosshair; }
	void SetShowCrosshair(bool b);

	// Look (Settings > Look & controller). Sensitivity is a multiplier on the base speeds in tuning "look".
	float MouseSensitivity() const { return MouseSens; }
	float PadSensitivity() const { return PadSens; }
	bool InvertMouse() const { return bInvertMouse; }
	bool InvertPad() const { return bInvertPad; }
	/** 0: Xbox button names (A, B, LB...), 1: PlayStation (Cross, Circle, L1...). */
	int32 PadLabels() const { return PadLabelSet; }
	/** Controller vibration on hits and heavy moments. */
	bool PadRumble() const { return bPadRumble; }
	void SetMouseSensitivity(float V);
	void SetPadSensitivity(float V);
	void SetInvertMouse(bool b);
	void SetInvertPad(bool b);
	void SetPadLabels(int32 Set);
	void SetPadRumble(bool b);

	void Save() const;
	FSimpleMulticastDelegate OnChanged;

private:
	FWRKeyBindings();
	void Add(FName Id, const TCHAR* Label, FName Group, const FKey& Default);
	void Load();
	TArray<FWRActionKey> List;
	EWRCastMode CastMode = EWRCastMode::Both;
	bool bCastOnRelease = false;
	bool bShowCrosshair = true;
	float MouseSens = 1.f;
	float PadSens = 1.f;
	bool bInvertMouse = false;
	bool bInvertPad = false;
	int32 PadLabelSet = 0;
	bool bPadRumble = true;
};
