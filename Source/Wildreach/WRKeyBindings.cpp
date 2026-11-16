#include "WRKeyBindings.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/IConsoleManager.h"
#include "Engine/UserInterfaceSettings.h"

static const TCHAR* KeySection = TEXT("Wildreach.Keys");

namespace WRKeyPrefs
{
	/** The preference table (cookpass-ui 3.1): id, default, range, the settings page it sits on. */
	struct FDef { const TCHAR* Id; float Default; float Min; float Max; const TCHAR* Page; };
	static const FDef Defs[] = {
		{ TEXT("MouseAimSens"), 0.7f, 0.2f, 1.5f, TEXT("look") },
		{ TEXT("PadAimSens"), 0.6f, 0.2f, 1.5f, TEXT("look") },
		{ TEXT("InvertPadX"), 0.f, 0.f, 1.f, TEXT("look") },
		{ TEXT("PadDeadZone"), 0.15f, 0.05f, 0.35f, TEXT("look") },
		{ TEXT("PadCurve"), 1.f, 0.f, 2.f, TEXT("look") },
		{ TEXT("Rumble"), 100.f, 0.f, 100.f, TEXT("look") },
		{ TEXT("SprintToggle"), 0.f, 0.f, 1.f, TEXT("look") },
		{ TEXT("GuardToggle"), 0.f, 0.f, 1.f, TEXT("look") },
		{ TEXT("AimToggle"), 0.f, 0.f, 1.f, TEXT("look") },
		{ TEXT("FovThird"), 90.f, 70.f, 100.f, TEXT("camera") },
		{ TEXT("FovFirst"), 90.f, 75.f, 110.f, TEXT("camera") },
		{ TEXT("Shake"), 100.f, 0.f, 100.f, TEXT("camera") },
		{ TEXT("MotionBlur"), 0.f, 0.f, 2.f, TEXT("camera") },
		{ TEXT("CameraSide"), 0.f, 0.f, 2.f, TEXT("camera") },
		{ TEXT("UiScale"), 100.f, 85.f, 130.f, TEXT("interface") },
		{ TEXT("HudScale"), 100.f, 75.f, 130.f, TEXT("interface") },
		{ TEXT("HudMode"), 1.f, 0.f, 2.f, TEXT("interface") },
		{ TEXT("Compass"), 1.f, 0.f, 1.f, TEXT("interface") },
		{ TEXT("Names"), 1.f, 0.f, 2.f, TEXT("interface") },
		{ TEXT("Captions"), 0.f, 0.f, 2.f, TEXT("audio") },     // sound captions: off, important, all (cookpass-audio 4.9)
		{ TEXT("AudioRange"), 0.f, 0.f, 1.f, TEXT("audio") },   // dynamic range: wide, night (cookpass-audio 4.4)
	};

	static const FDef* Find(FName Id)
	{
		for (const FDef& D : Defs) if (Id == D.Id) return &D;
		return nullptr;
	}
}

FWRKeyBindings& FWRKeyBindings::Get()
{
	static FWRKeyBindings Instance;
	return Instance;
}

FWRKeyBindings::FWRKeyBindings()
{
	const FName Movement(TEXT("Movement")), Combat(TEXT("Combat")), Items(TEXT("Items")), Menus(TEXT("Menus"));
	Add(TEXT("Forward"), TEXT("Move forward"), Movement, EKeys::W);
	Add(TEXT("Back"), TEXT("Move back"), Movement, EKeys::S);
	Add(TEXT("Left"), TEXT("Move left"), Movement, EKeys::A);
	Add(TEXT("Right"), TEXT("Move right"), Movement, EKeys::D);
	Add(TEXT("Jump"), TEXT("Jump"), Movement, EKeys::SpaceBar);
	Add(TEXT("Sprint"), TEXT("Sprint"), Movement, EKeys::LeftShift);
	Add(TEXT("Dodge"), TEXT("Dodge roll (in the water: tap to kick, hold to dive)"), Movement, EKeys::C);
	Add(TEXT("Dive"), TEXT("Dive (hold, in the water; Jump rises)"), Movement, EKeys::LeftControl);
	Add(TEXT("LockOn"), TEXT("Lock on"), Movement, EKeys::Tab);
	Add(TEXT("View"), TEXT("First / third person"), Movement, EKeys::V);
	Add(TEXT("Light"), TEXT("Light attack / fire"), Combat, EKeys::LeftMouseButton);
	Add(TEXT("Heavy"), TEXT("Heavy attack"), Combat, EKeys::R);
	Add(TEXT("Block"), TEXT("Block / parry / aim"), Combat, EKeys::RightMouseButton);
	Add(TEXT("SwapRanged"), TEXT("Swap to ranged weapon"), Combat, EKeys::G);
	Add(TEXT("TwoHand"), TEXT("Two-hand the weapon (hold)"), Combat, EKeys::Y);
	Add(TEXT("Wheel1"), TEXT("Ability wheel 1"), Combat, EKeys::Q);
	Add(TEXT("Wheel2"), TEXT("Ability wheel 2"), Combat, EKeys::Z);
	Add(TEXT("Interact"), TEXT("Interact (hold on your tame: orders)"), Items, EKeys::E);
	Add(TEXT("Whistle"), TEXT("Whistle to your tames (hold: orders for all)"), Items, EKeys::H);
	Add(TEXT("LightToggle"), TEXT("Light or put out your torch / lantern"), Items, EKeys::L);
	const FKey Numbers[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight };
	for (int32 i = 0; i < 8; ++i)
	{
		Add(FName(*FString::Printf(TEXT("Hotbar%d"), i + 1)), *FString::Printf(TEXT("Hotbar slot %d"), i + 1), Items, Numbers[i]);
	}
	Add(TEXT("HotbarNext"), TEXT("Next hotbar slot (ammo while aiming)"), Items, EKeys::MouseScrollDown);
	Add(TEXT("HotbarPrev"), TEXT("Previous hotbar slot"), Items, EKeys::MouseScrollUp);
	Add(TEXT("Inventory"), TEXT("Inventory"), Menus, EKeys::I);
	Add(TEXT("Crafting"), TEXT("Crafting"), Menus, EKeys::J);
	Add(TEXT("Build"), TEXT("Build"), Menus, EKeys::B);
	Add(TEXT("Paths"), TEXT("Paths"), Menus, EKeys::P);
	Add(TEXT("Skills"), TEXT("Life skills"), Menus, EKeys::K);
	Add(TEXT("Map"), TEXT("Map"), Menus, EKeys::M);
	Add(TEXT("Clan"), TEXT("Clan"), Menus, EKeys::O);
	Add(TEXT("Quests"), TEXT("Quests (path masters, shrines, standing)"), Menus, EKeys::U);
	Add(TEXT("Artifacts"), TEXT("Artifacts (your charms, geis records, the band)"), Menus, EKeys::N);
	Add(TEXT("Deeds"), TEXT("Deeds (achievements, kill times, titles, cosmetics, prestige marks)"), Menus, EKeys::Period);
	Add(TEXT("Chat"), TEXT("Chat"), Menus, EKeys::T);
	Add(TEXT("Help"), TEXT("Controls page"), Menus, EKeys::F1);
	Add(TEXT("Report"), TEXT("Report a problem"), Menus, EKeys::F8);
	Add(TEXT("Snap"), TEXT("Building: snap on / off"), Menus, EKeys::F);
	Add(TEXT("Demolish"), TEXT("Building: demolish mode"), Menus, EKeys::X);
	Add(TEXT("BackToMenu"), TEXT("Leave to the main menu"), Menus, EKeys::F10);
	Load();
}

void FWRKeyBindings::Add(FName Id, const TCHAR* Label, FName Group, const FKey& Default)
{
	List.Add({ Id, FText::FromString(Label), Group, Default, Default });
}

const FWRActionKey* FWRKeyBindings::Find(FName Id) const
{
	return List.FindByPredicate([Id](const FWRActionKey& A) { return A.Id == Id; });
}

FKey FWRKeyBindings::Key(FName Id) const
{
	const FWRActionKey* A = Find(Id);
	return A ? A->Key : EKeys::Invalid;
}

FName FWRKeyBindings::WhoUses(const FKey& InKey, FName Except) const
{
	if (!InKey.IsValid()) return NAME_None;
	for (const FWRActionKey& A : List)
	{
		if (A.Id != Except && A.Key == InKey) return A.Id;
	}
	return NAME_None;
}

FName FWRKeyBindings::Bind(FName Id, const FKey& NewKey)
{
	FWRActionKey* A = List.FindByPredicate([Id](const FWRActionKey& X) { return X.Id == Id; });
	if (!A) return NAME_None;
	const FName Clash = WhoUses(NewKey, Id);
	if (FWRActionKey* Other = List.FindByPredicate([Clash](const FWRActionKey& X) { return X.Id == Clash; }))
	{
		Other->Key = EKeys::Invalid;
	}
	A->Key = NewKey;
	Save();
	OnChanged.Broadcast();
	return Clash;
}

void FWRKeyBindings::Unbind(FName Id)
{
	if (FWRActionKey* A = List.FindByPredicate([Id](const FWRActionKey& X) { return X.Id == Id; }))
	{
		A->Key = EKeys::Invalid;
		Save();
		OnChanged.Broadcast();
	}
}

void FWRKeyBindings::ResetAll()
{
	for (FWRActionKey& A : List) A.Key = A.Default;
	CastMode = EWRCastMode::Both;
	bCastOnRelease = false;
	bShowCrosshair = true;
	bDamageNumbers = false;
	MouseSens = PadSens = 1.f;
	bInvertMouse = bInvertPad = false;
	PadLabelSet = 0;
	bPadRumble = true;
	Save();
	OnChanged.Broadcast();
}

void FWRKeyBindings::EnsureAbility(FName NodeId, const FText& Label)
{
	const FName Id = AbilityAction(NodeId);
	if (Find(Id)) return;
	FWRActionKey& A = List.Add_GetRef({ Id, Label, FName(TEXT("Abilities")), EKeys::Invalid, EKeys::Invalid });
	FString Saved;
	if (GConfig->GetString(KeySection, *Id.ToString(), Saved, GGameUserSettingsIni))
	{
		A.Key = Saved.IsEmpty() ? EKeys::Invalid : FKey(*Saved);
	}
}

void FWRKeyBindings::SetCastMode(EWRCastMode Mode)
{
	CastMode = Mode;
	Save();
	OnChanged.Broadcast();
}

void FWRKeyBindings::SetCastOnRelease(bool b)
{
	bCastOnRelease = b;
	Save();
	OnChanged.Broadcast();
}

void FWRKeyBindings::SetDamageNumbers(bool b)
{
	bDamageNumbers = b;
	Save();
}

void FWRKeyBindings::SetShowCrosshair(bool b)
{
	bShowCrosshair = b;
	Save();
	OnChanged.Broadcast();
}

void FWRKeyBindings::SetMouseSensitivity(float V) { MouseSens = FMath::Clamp(V, 0.1f, 4.f); Save(); OnChanged.Broadcast(); }
void FWRKeyBindings::SetPadSensitivity(float V) { PadSens = FMath::Clamp(V, 0.1f, 4.f); Save(); OnChanged.Broadcast(); }
void FWRKeyBindings::SetInvertMouse(bool b) { bInvertMouse = b; Save(); OnChanged.Broadcast(); }
void FWRKeyBindings::SetInvertPad(bool b) { bInvertPad = b; Save(); OnChanged.Broadcast(); }
void FWRKeyBindings::SetPadLabels(int32 Set) { PadLabelSet = FMath::Clamp(Set, 0, 1); Save(); OnChanged.Broadcast(); }
void FWRKeyBindings::SetPadRumble(bool b) { bPadRumble = b; Save(); OnChanged.Broadcast(); }

float FWRKeyBindings::Pref(FName Id) const
{
	if (const float* V = Prefs.Find(Id)) return *V;
	const WRKeyPrefs::FDef* D = WRKeyPrefs::Find(Id);
	return D ? D->Default : 0.f;
}

void FWRKeyBindings::SetPref(FName Id, float Value)
{
	const WRKeyPrefs::FDef* D = WRKeyPrefs::Find(Id);
	if (!D) return;
	const float V = FMath::Clamp(Value, D->Min, D->Max);
	if (Id == TEXT("Rumble") && V > 0.f) bPadRumble = true;   // the old on/off switch is folded into the strength
	Prefs.Add(Id, V);
	Save();
	ApplyEnginePrefs();
	OnChanged.Broadcast();
}

void FWRKeyBindings::ResetPage(FName Page)
{
	for (const WRKeyPrefs::FDef& D : WRKeyPrefs::Defs)
	{
		if (Page == D.Page) Prefs.Remove(D.Id);
	}
	if (Page == TEXT("look")) bPadRumble = true;
	Save();
	ApplyEnginePrefs();
	OnChanged.Broadcast();
}

void FWRKeyBindings::ApplyEnginePrefs() const
{
	// Interface size: the engine's application scale multiplies the DPI scale of every screen in the game window (the
	// main menu included). Not in the editor: it would resize the editor's own play-in-editor viewport UI.
	if (!GIsEditor)
	{
		if (UUserInterfaceSettings* UI = GetMutableDefault<UUserInterfaceSettings>())
		{
			UI->ApplicationScale = Pref(TEXT("UiScale")) / 100.f;
		}
	}
	// Motion blur: the amount overrides whatever the post-process volumes or the quality level ask for.
	if (IConsoleVariable* Blur = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MotionBlur.Amount")))
	{
		static const float Amounts[] = { 0.f, 0.25f, 0.5f };
		Blur->Set(Amounts[FMath::Clamp(PrefInt(TEXT("MotionBlur")), 0, 2)], ECVF_SetByGameSetting);
	}
}

void FWRKeyBindings::Load()
{
	for (FWRActionKey& A : List)
	{
		FString Saved;
		if (GConfig->GetString(KeySection, *A.Id.ToString(), Saved, GGameUserSettingsIni))
		{
			A.Key = Saved.IsEmpty() ? EKeys::Invalid : FKey(*Saved);
		}
	}
	int32 Mode = static_cast<int32>(CastMode);
	GConfig->GetInt(KeySection, TEXT("CastMode"), Mode, GGameUserSettingsIni);
	CastMode = static_cast<EWRCastMode>(FMath::Clamp(Mode, 0, 2));
	GConfig->GetBool(KeySection, TEXT("CastOnRelease"), bCastOnRelease, GGameUserSettingsIni);
	GConfig->GetBool(KeySection, TEXT("ShowCrosshair"), bShowCrosshair, GGameUserSettingsIni);
	GConfig->GetBool(KeySection, TEXT("DamageNumbers"), bDamageNumbers, GGameUserSettingsIni);
	GConfig->GetFloat(KeySection, TEXT("MouseSensitivity"), MouseSens, GGameUserSettingsIni);
	GConfig->GetFloat(KeySection, TEXT("PadSensitivity"), PadSens, GGameUserSettingsIni);
	GConfig->GetBool(KeySection, TEXT("InvertMouse"), bInvertMouse, GGameUserSettingsIni);
	GConfig->GetBool(KeySection, TEXT("InvertPad"), bInvertPad, GGameUserSettingsIni);
	GConfig->GetInt(KeySection, TEXT("PadLabels"), PadLabelSet, GGameUserSettingsIni);
	GConfig->GetBool(KeySection, TEXT("PadRumble"), bPadRumble, GGameUserSettingsIni);
	MouseSens = FMath::Clamp(MouseSens, 0.1f, 4.f);
	PadSens = FMath::Clamp(PadSens, 0.1f, 4.f);
	PadLabelSet = FMath::Clamp(PadLabelSet, 0, 1);
	for (const WRKeyPrefs::FDef& D : WRKeyPrefs::Defs)
	{
		float V = D.Default;
		if (GConfig->GetFloat(KeySection, *(FString(TEXT("Pref.")) + D.Id), V, GGameUserSettingsIni))
		{
			Prefs.Add(D.Id, FMath::Clamp(V, D.Min, D.Max));
		}
	}
	ApplyEnginePrefs();
}

void FWRKeyBindings::Save() const
{
	for (const FWRActionKey& A : List)
	{
		// Only what differs from the default is written, so new defaults reach players who never rebound.
		if (A.Key != A.Default)
		{
			GConfig->SetString(KeySection, *A.Id.ToString(), A.Key.IsValid() ? *A.Key.ToString() : TEXT(""), GGameUserSettingsIni);
		}
		else
		{
			GConfig->RemoveKey(KeySection, *A.Id.ToString(), GGameUserSettingsIni);
		}
	}
	GConfig->SetInt(KeySection, TEXT("CastMode"), static_cast<int32>(CastMode), GGameUserSettingsIni);
	GConfig->SetBool(KeySection, TEXT("CastOnRelease"), bCastOnRelease, GGameUserSettingsIni);
	GConfig->SetBool(KeySection, TEXT("ShowCrosshair"), bShowCrosshair, GGameUserSettingsIni);
	GConfig->SetBool(KeySection, TEXT("DamageNumbers"), bDamageNumbers, GGameUserSettingsIni);
	GConfig->SetFloat(KeySection, TEXT("MouseSensitivity"), MouseSens, GGameUserSettingsIni);
	GConfig->SetFloat(KeySection, TEXT("PadSensitivity"), PadSens, GGameUserSettingsIni);
	GConfig->SetBool(KeySection, TEXT("InvertMouse"), bInvertMouse, GGameUserSettingsIni);
	GConfig->SetBool(KeySection, TEXT("InvertPad"), bInvertPad, GGameUserSettingsIni);
	GConfig->SetInt(KeySection, TEXT("PadLabels"), PadLabelSet, GGameUserSettingsIni);
	GConfig->SetBool(KeySection, TEXT("PadRumble"), bPadRumble, GGameUserSettingsIni);
	for (const WRKeyPrefs::FDef& D : WRKeyPrefs::Defs)
	{
		const FString Key = FString(TEXT("Pref.")) + D.Id;
		const float* V = Prefs.Find(D.Id);
		if (V && !FMath::IsNearlyEqual(*V, D.Default)) GConfig->SetFloat(KeySection, *Key, *V, GGameUserSettingsIni);
		else GConfig->RemoveKey(KeySection, *Key, GGameUserSettingsIni);
	}
	GConfig->Flush(false, GGameUserSettingsIni);
}
