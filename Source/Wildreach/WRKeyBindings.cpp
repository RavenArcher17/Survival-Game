#include "WRKeyBindings.h"
#include "Misc/ConfigCacheIni.h"

static const TCHAR* KeySection = TEXT("Wildreach.Keys");

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
	Add(TEXT("Dodge"), TEXT("Dodge roll"), Movement, EKeys::C);
	Add(TEXT("LockOn"), TEXT("Lock on"), Movement, EKeys::Tab);
	Add(TEXT("View"), TEXT("First / third person"), Movement, EKeys::V);
	Add(TEXT("Light"), TEXT("Light attack / fire"), Combat, EKeys::LeftMouseButton);
	Add(TEXT("Heavy"), TEXT("Heavy attack"), Combat, EKeys::R);
	Add(TEXT("Block"), TEXT("Block / parry / aim"), Combat, EKeys::RightMouseButton);
	Add(TEXT("SwapRanged"), TEXT("Swap to ranged weapon"), Combat, EKeys::G);
	Add(TEXT("Wheel1"), TEXT("Ability wheel 1"), Combat, EKeys::Q);
	Add(TEXT("Wheel2"), TEXT("Ability wheel 2"), Combat, EKeys::Z);
	Add(TEXT("Interact"), TEXT("Interact (hold on your tame: orders)"), Items, EKeys::E);
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
	Add(TEXT("Chat"), TEXT("Chat"), Menus, EKeys::T);
	Add(TEXT("Help"), TEXT("Controls help"), Menus, EKeys::H);
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
	GConfig->Flush(false, GGameUserSettingsIni);
}
