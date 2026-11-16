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
	/** Shown in the controls pages only to a verified tester (the god mode's key, WRGod). */
	bool bTesterOnly = false;
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
	/** Damage numbers beside what you hit (off by default, combat.md 2.14). */
	bool DamageNumbers() const { return bDamageNumbers; }
	void SetDamageNumbers(bool b);

	// Look (Settings > Look & controller). Sensitivity is a multiplier on the base speeds in tuning "look".
	float MouseSensitivity() const { return MouseSens; }
	float PadSensitivity() const { return PadSens; }
	bool InvertMouse() const { return bInvertMouse; }
	bool InvertPad() const { return bInvertPad; }
	/** 0: Xbox button names (A, B, LB...), 1: PlayStation (Cross, Circle, L1...). */
	int32 PadLabels() const { return PadLabelSet; }
	/** Controller vibration on hits and heavy moments (the Rumble pref above 0). */
	bool PadRumble() const { return bPadRumble && Pref(TEXT("Rumble")) > 0.f; }
	/** 0..1: the Rumble pref (Settings > Look and controller > Vibration). */
	float RumbleScale() const { return bPadRumble ? FMath::Clamp(Pref(TEXT("Rumble")) / 100.f, 0.f, 1.f) : 0.f; }
	void SetMouseSensitivity(float V);
	void SetPadSensitivity(float V);
	void SetInvertMouse(bool b);
	void SetInvertPad(bool b);
	void SetPadLabels(int32 Set);
	void SetPadRumble(bool b);

	/**
	 * Player preferences beyond keys (cookpass-ui): one table of numbers by id, each with its default, range and the
	 * settings page it sits on (look, camera, interface). Saved as Pref.<id> in [Wildreach.Keys], only when not the
	 * default. Ids: look MouseAimSens, PadAimSens, InvertPadX, PadDeadZone, PadCurve (0 linear, 1 standard, 2 precise),
	 * Rumble (0-100 %), SprintToggle, GuardToggle, AimToggle; camera FovThird, FovFirst, Shake (0-100 %), MotionBlur
	 * (0 off, 1 low, 2 high), CameraSide (0 right, 1 left, 2 centre); interface UiScale, HudScale (%), HudMode (0 full,
	 * 1 fade, 2 minimal), Compass, Names (0 always, 1 near, 2 off). (Show my helmet lives in the menu's Look line,
	 * SWRMainMenu::GetLook / SetLook, because the join URL carries it.)
	 */
	float Pref(FName Id) const;
	bool PrefOn(FName Id) const { return Pref(Id) > 0.5f; }
	int32 PrefInt(FName Id) const { return FMath::RoundToInt(Pref(Id)); }
	/** Clamped to the pref's range; saved and broadcast; engine-side prefs (interface size, motion blur) applied. */
	void SetPref(FName Id, float Value);
	/** Every pref on a page back to its default (look, camera, interface). */
	void ResetPage(FName Page);
	/** Push the prefs the engine holds (interface size, motion blur) into it: at start and after a change. */
	void ApplyEnginePrefs() const;

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
	bool bDamageNumbers = false;
	float MouseSens = 1.f;
	float PadSens = 1.f;
	bool bInvertMouse = false;
	bool bInvertPad = false;
	int32 PadLabelSet = 0;
	bool bPadRumble = true;
	TMap<FName, float> Prefs;
};
