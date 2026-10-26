#include "WRPlayerUI.h"
#include "Wildreach.h"
#include "WRAbilities.h"
#include "WRCharacter.h"
#include "WRCombatTypes.h"
#include "WRInventory.h"
#include "WRKeyBindings.h"
#include "WRMainMenu.h"
#include "WRPathComponent.h"
#include "WRPathTypes.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRUIStyle.h"
#include "AbilitySystemComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/ConfigCacheIni.h"

#define LOCTEXT_NAMESPACE "WRUI"

// ---------------------------------------------------------------- screen registry

static TMap<FName, FWRScreenFactory>& ScreenMap() { static TMap<FName, FWRScreenFactory> M; return M; }
static TMap<FName, FWRScreenFactory>& TabMap() { static TMap<FName, FWRScreenFactory> M; return M; }
void FWRScreens::Register(FName Name, FWRScreenFactory Factory) { ScreenMap().Add(Name, MoveTemp(Factory)); }
void FWRScreens::RegisterTab(FName Tab, FWRScreenFactory Factory) { TabMap().Add(Tab, MoveTemp(Factory)); }
const FWRScreenFactory* FWRScreens::Find(FName Name) { return ScreenMap().Find(Name); }
const FWRScreenFactory* FWRScreens::FindTab(FName Tab) { return TabMap().Find(Tab); }

static const TCHAR* UISection = TEXT("Wildreach.UI");

FWRPlayerUI::FWRPlayerUI(AWRPlayerController* InPC) : PC(InPC)
{
	Hotbar.Init(NAME_None, 8);
	if (const FWRScreenFactory* Hud = FWRScreens::Find(TEXT("hud")))
	{
		if (GEngine && GEngine->GameViewport)
		{
			HudWidget = (*Hud)(InPC);
			GEngine->GameViewport->AddViewportWidgetContent(HudWidget.ToSharedRef(), 10);
		}
	}
	if (const FWRScreenFactory* Wheel = FWRScreens::Find(TEXT("wheel")))
	{
		if (GEngine && GEngine->GameViewport)
		{
			WheelWidget = (*Wheel)(InPC);
			GEngine->GameViewport->AddViewportWidgetContent(WheelWidget.ToSharedRef(), 40);
		}
	}
}

FWRPlayerUI::~FWRPlayerUI()
{
	if (GEngine && GEngine->GameViewport)
	{
		for (TSharedPtr<SWidget>* W : { &HudWidget, &ScreenWidget, &WheelWidget })
		{
			if (W->IsValid()) GEngine->GameViewport->RemoveViewportWidgetContent(W->ToSharedRef());
		}
	}
}

// ---------------------------------------------------------------- tick, screens

void FWRPlayerUI::Tick(float DeltaTime)
{
	AWRPlayerController* P = PC.Get();
	if (!P)
	{
		return;
	}
	if (HudWidget.IsValid())
	{
		// The main menu draws over the island: no HUD under it.
		HudWidget->SetVisibility(SWRMainMenu::IsOpen() ? EVisibility::Collapsed : EVisibility::SelfHitTestInvisible);
	}
	if (!bHotbarLoaded) LoadHotbar();
	if (!bWheelsLoaded) LoadWheels();
	AutoFillHotbar();
	SyncScreen();

	const double Now = FPlatformTime::Seconds();
	// A wheel key held past a tap opens its wheel; the cursor appears in the middle to pick with.
	if (WheelDownKey >= 0 && OpenWheel < 0 && Now - WheelDownAt > 0.2)
	{
		OpenWheel = WheelDownKey;
		WheelHover = WheelPicked[OpenWheel];
		CentreMouse();
	}
	if (OpenWheel >= 0 && !bDebugWheel) UpdateWheelHover(WheelHover, 8);
	if (bTameKeyDown && !bTameWheel && Now - TameDownAt > 0.25)
	{
		bTameWheel = true;
		bTameSticky = false;
		TameHover = -1;
		CentreMouse();
	}
	if (bTameWheel && !bDebugWheel) UpdateWheelHover(TameHover, 8);
	Pickups.RemoveAll([Now](const FPickup& X) { return Now - X.Time > 3.5; });
	// Abilities can have their own keys (Settings): make sure each learned one has an entry.
	if (Now > NextAbilityKeyCheck)
	{
		NextAbilityKeyCheck = Now + 1.0;
		for (const FWRCastable& C : GetCastables()) FWRKeyBindings::Get().EnsureAbility(C.NodeId, C.Name);
	}
}

void FWRPlayerUI::CentreMouse()
{
	AWRPlayerController* P = PC.Get();
	int32 SX = 0, SY = 0;
	P->GetViewportSize(SX, SY);
	P->SetMouseLocation(SX / 2, SY / 2);
	UpdateInputMode();
}

void FWRPlayerUI::SyncScreen()
{
	AWRPlayerController* P = PC.Get();
	FName Want;
	switch (P->GetMenu())
	{
	case EWRMenu::Inventory: case EWRMenu::Craft: case EWRMenu::Skills: case EWRMenu::Paths: case EWRMenu::Map: Want = TEXT("journal"); break;
	case EWRMenu::Build: Want = TEXT("build"); break;
	case EWRMenu::Chest: Want = TEXT("storage"); break;
	case EWRMenu::Claim: Want = TEXT("claim"); break;
	case EWRMenu::Hearth: Want = TEXT("hearth"); break;
	case EWRMenu::Taming: Want = TEXT("taming"); break;
	case EWRMenu::Settings: Want = TEXT("settings"); break;
	default: break;
	}
	if (!Want.IsNone() && !FWRScreens::Find(Want))
	{
		Want = NAME_None;   // not built yet: the old text panel (AWRHUD) still draws it
	}
	if (P->GetMenu() == EWRMenu::Tame && !bTameWheel)
	{
		// Tapping E on your tame: the order wheel stays up until you click an order (or press E / Esc).
		bTameWheel = true;
		bTameSticky = true;
		TameHover = -1;
		CentreMouse();
	}
	else if (P->GetMenu() != EWRMenu::Tame && bTameWheel && bTameSticky)
	{
		bTameWheel = false;
	}
	if (Want != ScreenName)
	{
		CloseScreen();
		if (!Want.IsNone()) ShowScreen(Want);
	}
	UpdateInputMode();
}

void FWRPlayerUI::ShowScreen(FName Name)
{
	AWRPlayerController* P = PC.Get();
	const FWRScreenFactory* Factory = FWRScreens::Find(Name);
	if (!Factory || !GEngine || !GEngine->GameViewport) return;
	ScreenWidget = (*Factory)(P);
	ScreenName = Name;
	GEngine->GameViewport->AddViewportWidgetContent(ScreenWidget.ToSharedRef(), 20);
	UE_LOG(LogWildreach, Log, TEXT("UI screen %s"), *Name.ToString());
}

void FWRPlayerUI::CloseScreen()
{
	if (ScreenWidget.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(ScreenWidget.ToSharedRef());
	}
	ScreenWidget.Reset();
	ScreenName = NAME_None;
}

void FWRPlayerUI::UpdateInputMode()
{
	AWRPlayerController* P = PC.Get();
	// Menus want the cursor and stop the camera; the build bar only while no piece is being placed.
	const bool bMenu = ScreenWidget.IsValid() && !(ScreenName == TEXT("build") && P->IsPlacing());
	const bool bWheel = OpenWheel >= 0 || bTameWheel;
	const bool bWant = bMenu || bWheel;
	const bool bStill = bMenu && ScreenName != TEXT("build");
	if (bWant != bScreenWantsCursor || bStill != bScreenStopsMove)
	{
		bScreenWantsCursor = bWant;
		bScreenStopsMove = bStill;
		P->bShowMouseCursor = bWant;
		// The ignore flags are counters: reset, then set once.
		P->ResetIgnoreLookInput();
		P->ResetIgnoreMoveInput();
		if (bWant) P->SetIgnoreLookInput(true);
		if (bStill) P->SetIgnoreMoveInput(true);
		if (bWant)
		{
			FInputModeGameAndUI Mode;
			Mode.SetHideCursorDuringCapture(false);
			Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			P->SetInputMode(Mode);
		}
		else
		{
			P->SetInputMode(FInputModeGameOnly());
		}
	}
}

// ---------------------------------------------------------------- notices

void FWRPlayerUI::NoteNotice(const FString& Text)
{
	// "+3 Wood" style pickups stack above the hotbar; everything else is a centre notice (the HUD reads it).
	if (Text.StartsWith(TEXT("+")))
	{
		Pickups.Add({ Text, FPlatformTime::Seconds() });
		if (Pickups.Num() > 5) Pickups.RemoveAt(0);
	}
}

// ---------------------------------------------------------------- hotbar

void FWRPlayerUI::LoadHotbar()
{
	bHotbarLoaded = true;
	FString Saved;
	if (GConfig->GetString(UISection, TEXT("Hotbar"), Saved, GGameUserSettingsIni))
	{
		TArray<FString> Parts;
		Saved.ParseIntoArray(Parts, TEXT(","), false);
		for (int32 i = 0; i < 8 && i < Parts.Num(); ++i) Hotbar[i] = Parts[i].IsEmpty() ? NAME_None : FName(*Parts[i]);
	}
	if (GConfig->GetString(UISection, TEXT("HotbarRemoved"), Saved, GGameUserSettingsIni))
	{
		TArray<FString> Parts;
		Saved.ParseIntoArray(Parts, TEXT(","), true);
		for (const FString& P : Parts) RemovedFromBar.Add(FName(*P));
	}
}

void FWRPlayerUI::SaveHotbar() const
{
	TArray<FString> Parts;
	for (const FName& N : Hotbar) Parts.Add(N.IsNone() ? FString() : N.ToString());
	GConfig->SetString(UISection, TEXT("Hotbar"), *FString::Join(Parts, TEXT(",")), GGameUserSettingsIni);
	TArray<FString> Removed;
	for (const FName& N : RemovedFromBar) Removed.Add(N.ToString());
	GConfig->SetString(UISection, TEXT("HotbarRemoved"), *FString::Join(Removed, TEXT(",")), GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void FWRPlayerUI::AutoFillHotbar()
{
	const AWRPlayerState* PS = PC.IsValid() ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
	const UWRInventoryComponent* Inv = PS ? PS->GetInventory() : nullptr;
	if (!Inv) return;
	bool bAnyOnBar = false;
	for (const FName& N : Hotbar) bAnyOnBar |= !N.IsNone();
	if (bAnyOnBar)
	{
		// A weapon, tool or consumable in the bag that isn't on the bar takes a free key, unless you took it off.
		bool bChanged = false;
		for (const FWRItemStack& S : Inv->GetSlots())
		{
			if (S.IsEmpty() || Hotbar.Contains(S.ItemId) || RemovedFromBar.Contains(S.ItemId)) continue;
			const FWRItemDef* Def = FWRItemRegistry::Get().Find(S.ItemId);
			const bool bBarItem = Def && (Def->Category == TEXT("weapon") || Def->Category == TEXT("ranged") || Def->Category == TEXT("tool") || Def->Category == TEXT("food") || Def->Category == TEXT("drink") || Def->Category == TEXT("potion"));
			const int32 Free = Hotbar.IndexOfByKey(NAME_None);
			if (bBarItem && Free != INDEX_NONE)
			{
				Hotbar[Free] = S.ItemId;
				bChanged = true;
			}
		}
		if (bChanged) SaveHotbar();
		return;
	}
	TArray<FName> Picks;
	for (const EWREquipSlot S : { EWREquipSlot::Weapon, EWREquipSlot::Ranged })
	{
		if (!Inv->GetEquipped(S).IsEmpty()) Picks.AddUnique(Inv->GetEquipped(S).ItemId);
	}
	const TCHAR* Order[] = { TEXT("weapon"), TEXT("ranged"), TEXT("tool"), TEXT("food"), TEXT("drink"), TEXT("potion") };
	for (const TCHAR* Cat : Order)
	{
		for (const FWRItemStack& S : Inv->GetSlots())
		{
			const FWRItemDef* Def = S.IsEmpty() ? nullptr : FWRItemRegistry::Get().Find(S.ItemId);
			if (Def && Def->Category == Cat && Picks.Num() < 8) Picks.AddUnique(S.ItemId);
		}
	}
	if (Picks.Num() == 0) return;
	for (int32 i = 0; i < Picks.Num() && i < 8; ++i) Hotbar[i] = Picks[i];
	SaveHotbar();
}

void FWRPlayerUI::SetHotbarItem(int32 Index, FName ItemId)
{
	if (!Hotbar.IsValidIndex(Index)) return;
	// An item lives on one key: moving it clears its old key. What's pushed off the bar stays off (until you put it back).
	if (!Hotbar[Index].IsNone() && Hotbar[Index] != ItemId) RemovedFromBar.Add(Hotbar[Index]);
	for (FName& N : Hotbar) if (N == ItemId) N = NAME_None;
	Hotbar[Index] = ItemId;
	if (!ItemId.IsNone()) RemovedFromBar.Remove(ItemId);
	SaveHotbar();
}

void FWRPlayerUI::UseHotbar(int32 Index)
{
	AWRPlayerController* P = PC.Get();
	const FName Id = GetHotbarItem(Index);
	const AWRPlayerState* PS = P ? P->GetPlayerState<AWRPlayerState>() : nullptr;
	const UWRInventoryComponent* Inv = PS ? PS->GetInventory() : nullptr;
	const FWRItemDef* Def = Id.IsNone() ? nullptr : FWRItemRegistry::Get().Find(Id);
	if (!Inv || !Def) return;
	AWRCharacter* Survivor = Cast<AWRCharacter>(P->GetPawn());
	if (Def->IsEquipment())
	{
		HotbarSelected = Index;
		const FWRItemStack& Worn = Inv->GetEquipped(Def->Slot);
		if (!Worn.IsEmpty() && Worn.ItemId == Id)
		{
			// Already in hand: a ranged weapon comes out (or goes away); a melee weapon puts the ranged one away.
			if (Survivor && Def->Slot == EWREquipSlot::Ranged) Survivor->SetRangedMode(!Survivor->IsRangedOut());
			else if (Survivor && Def->Slot == EWREquipSlot::Weapon && Survivor->IsRangedOut()) Survivor->SetRangedMode(false);
			return;
		}
	}
	if (Def->Category == TEXT("tool"))
	{
		P->ShowLocalNotice(TEXT("Tools work from your bag: gather and the best one is used"));
		return;
	}
	const TArray<FWRItemStack>& Slots = Inv->GetSlots();
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		if (!Slots[i].IsEmpty() && Slots[i].ItemId == Id)
		{
			P->SendCommand(FString::Printf(TEXT("slot %d"), i));
			if (Survivor && Def->Slot == EWREquipSlot::Weapon && Survivor->IsRangedOut()) Survivor->SetRangedMode(false);
			return;
		}
	}
	P->ShowLocalNotice(FString::Printf(TEXT("No %s in your bag"), *Def->Name));
}

void FWRPlayerUI::CycleHotbar(int32 Dir)
{
	// The wheel steps through the weapons on the bar (what "selected" means for the hand).
	for (int32 Step = 1; Step <= 8; ++Step)
	{
		const int32 i = ((HotbarSelected + Dir * Step) % 8 + 8) % 8;
		const FWRItemDef* Def = Hotbar[i].IsNone() ? nullptr : FWRItemRegistry::Get().Find(Hotbar[i]);
		if (Def && Def->IsEquipment() && Def->Slot != EWREquipSlot::Armor)
		{
			UseHotbar(i);
			return;
		}
	}
}

FWRSlotView FWRPlayerUI::HotbarView(int32 Index) const
{
	FWRSlotView V;
	V.Key = WRUI::KeyName(FWRKeyBindings::Get().Key(FName(*FString::Printf(TEXT("Hotbar%d"), Index + 1))));
	V.ItemId = GetHotbarItem(Index);
	const AWRPlayerState* PS = PC.IsValid() ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
	const UWRInventoryComponent* Inv = PS ? PS->GetInventory() : nullptr;
	const FWRItemDef* Def = V.ItemId.IsNone() ? nullptr : FWRItemRegistry::Get().Find(V.ItemId);
	if (!Inv || !Def) return V;
	V.Count = Inv->CountOf(V.ItemId);
	if (Def->IsEquipment())
	{
		const FWRItemStack& Worn = Inv->GetEquipped(Def->Slot);
		const bool bWorn = !Worn.IsEmpty() && Worn.ItemId == V.ItemId;
		const FWRItemStack* Inst = bWorn ? &Worn : Inv->GetSlots().FindByPredicate([&V](const FWRItemStack& S) { return !S.IsEmpty() && S.ItemId == V.ItemId; });
		if (Inst && Def->MaxDurability > 0.f) V.Durability = Inst->Durability / Def->MaxDurability;
		// The slot's rim only marks the better grades (crude and common look plain), as the inventory shows them.
		if (Inst && Def->IsEquipment()) V.Quality = Inst->Quality == EWRQuality::Masterwork ? 2 : Inst->Quality == EWRQuality::Fine ? 1 : 0;
		const AWRCharacter* Survivor = Cast<AWRCharacter>(PC->GetPawn());
		V.bSelected = bWorn && (Def->Slot == EWREquipSlot::Ranged ? (Survivor && Survivor->IsRangedOut()) : (Def->Slot == EWREquipSlot::Weapon && !(Survivor && Survivor->IsRangedOut())));
		if (Def->IsThrowable()) { V.Count = (bWorn ? Worn.Count : 0) + Inv->CountOf(V.ItemId); V.bShowCount = true; }
		else if (Def->IsRanged())
		{
			// A bow shows its loaded ammo count.
			const FName Ammo = Survivor ? Survivor->GetSelectedAmmo() : NAME_None;
			V.Count = Ammo.IsNone() ? 0 : Inv->CountOf(Ammo);
			V.bShowCount = bWorn;
		}
		else V.Count = 0;
		V.bDimmed = !bWorn && !Inst;
	}
	else
	{
		V.bShowCount = true;
		V.bDimmed = V.Count <= 0;
	}
	return V;
}

// ---------------------------------------------------------------- abilities

TArray<FWRCastable> FWRPlayerUI::GetCastables() const
{
	TArray<FWRCastable> Out;
	const AWRCharacter* Survivor = PC.IsValid() ? Cast<AWRCharacter>(PC->GetPawn()) : nullptr;
	const UWRPathComponent* Paths = Survivor ? UWRPathComponent::FindFor(Survivor) : nullptr;
	if (!Paths) return Out;
	const UAbilitySystemComponent* ASC = Survivor->GetAbilitySystemComponent();
	const TArray<FName>& Slots = Paths->GetQuickSlots();
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		if (Slots[i].IsNone()) continue;
		FString PathStr, NodeStr;
		Slots[i].ToString().Split(TEXT("."), &PathStr, &NodeStr);
		const FWRPathDef* Def = FWRPathRegistry::Get().Find(FName(*PathStr));
		const FWRPathNodeDef* Node = Def ? Def->FindNode(FName(*NodeStr)) : nullptr;
		FWRCastable C;
		C.NodeId = Slots[i];
		C.Path = FName(*PathStr);
		C.Name = FText::FromString(Node ? Node->Name : NodeStr);
		C.InputIndex = i;
		if (ASC)
		{
			const int32 InputId = static_cast<int32>(EWRAbilityInput::Slot1) + i;
			for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
			{
				if (Spec.InputID != InputId) continue;
				if (const UWRGameplayAbility* Inst = Cast<UWRGameplayAbility>(Spec.GetPrimaryInstance())) C.Cooldown = Inst->GetCooldownRemaining();
			}
		}
		Out.Add(C);
	}
	return Out;
}

const FWRCastable* FWRPlayerUI::FindCastable(FName NodeId, const TArray<FWRCastable>& In) const
{
	return In.FindByPredicate([NodeId](const FWRCastable& C) { return C.NodeId == NodeId; });
}

void FWRPlayerUI::LoadWheels()
{
	bWheelsLoaded = true;
	for (int32 W = 0; W < 2; ++W)
	{
		FString Saved;
		if (GConfig->GetString(UISection, *FString::Printf(TEXT("Wheel%d"), W + 1), Saved, GGameUserSettingsIni))
		{
			TArray<FString> Parts;
			Saved.ParseIntoArray(Parts, TEXT(","), false);
			for (int32 i = 0; i < 8 && i < Parts.Num(); ++i) Wheels[W][i] = Parts[i].IsEmpty() ? NAME_None : FName(*Parts[i]);
		}
		GConfig->GetInt(UISection, *FString::Printf(TEXT("WheelPicked%d"), W + 1), WheelPicked[W], GGameUserSettingsIni);
		WheelPicked[W] = FMath::Clamp(WheelPicked[W], 0, 7);
	}
}

void FWRPlayerUI::SaveWheels() const
{
	for (int32 W = 0; W < 2; ++W)
	{
		TArray<FString> Parts;
		for (int32 i = 0; i < 8; ++i) Parts.Add(Wheels[W][i].IsNone() ? FString() : Wheels[W][i].ToString());
		GConfig->SetString(UISection, *FString::Printf(TEXT("Wheel%d"), W + 1), *FString::Join(Parts, TEXT(",")), GGameUserSettingsIni);
		GConfig->SetInt(UISection, *FString::Printf(TEXT("WheelPicked%d"), W + 1), WheelPicked[W], GGameUserSettingsIni);
	}
	GConfig->Flush(false, GGameUserSettingsIni);
}

FName FWRPlayerUI::GetWheelAbility(int32 Wheel, int32 Pos) const
{
	if (Wheel < 0 || Wheel > 1 || Pos < 0 || Pos > 7) return NAME_None;
	const FName Stored = Wheels[Wheel][Pos];
	if (!Stored.IsNone()) return Stored;
	if (Wheel == 0)
	{
		// Unassigned positions of wheel 1 show the learned abilities in order, skipping ones placed elsewhere.
		const TArray<FWRCastable> All = GetCastables();
		TArray<FName> Free;
		for (const FWRCastable& C : All)
		{
			bool bPlaced = false;
			for (int32 W = 0; W < 2; ++W) for (int32 i = 0; i < 8; ++i) bPlaced |= Wheels[W][i] == C.NodeId;
			if (!bPlaced) Free.Add(C.NodeId);
		}
		int32 EmptyBefore = 0;
		for (int32 i = 0; i < Pos; ++i) EmptyBefore += Wheels[0][i].IsNone();
		return Free.IsValidIndex(EmptyBefore) ? Free[EmptyBefore] : NAME_None;
	}
	return NAME_None;
}

void FWRPlayerUI::SetWheelAbility(int32 Wheel, int32 Pos, FName NodeId)
{
	if (Wheel < 0 || Wheel > 1 || Pos < 0 || Pos > 7) return;
	for (int32 W = 0; W < 2; ++W) for (int32 i = 0; i < 8; ++i) if (Wheels[W][i] == NodeId) Wheels[W][i] = NAME_None;
	Wheels[Wheel][Pos] = NodeId;
	SaveWheels();
}

void FWRPlayerUI::SetWheelPicked(int32 Wheel, int32 Pos)
{
	WheelPicked[FMath::Clamp(Wheel, 0, 1)] = FMath::Clamp(Pos, 0, 7);
	SaveWheels();
}

void FWRPlayerUI::CastAbility(FName NodeId)
{
	AWRCharacter* Survivor = PC.IsValid() ? Cast<AWRCharacter>(PC->GetPawn()) : nullptr;
	const TArray<FWRCastable> All = GetCastables();
	const FWRCastable* C = FindCastable(NodeId, All);
	if (!Survivor || !C)
	{
		if (PC.IsValid()) PC->ShowLocalNotice(TEXT("Nothing on that slot: learn abilities in Paths"));
		return;
	}
	const EWRAbilityInput Input = static_cast<EWRAbilityInput>(static_cast<int32>(EWRAbilityInput::Slot1) + C->InputIndex);
	Survivor->PressAbilityInput(Input);
	Survivor->ReleaseAbilityInput(Input);
}

void FWRPlayerUI::WheelKey(int32 Wheel, bool bDown)
{
	if (FWRKeyBindings::Get().GetCastMode() == EWRCastMode::Keys) return;
	AWRPlayerController* P = PC.Get();
	if (bDown)
	{
		if (WheelDownKey < 0)
		{
			WheelDownKey = Wheel;
			WheelDownAt = FPlatformTime::Seconds();
		}
		return;
	}
	if (WheelDownKey != Wheel) return;
	WheelDownKey = -1;
	if (OpenWheel == Wheel)
	{
		// Released over a wedge: that ability is the picked one (and is cast, if Settings say so).
		if (WheelHover >= 0 && !GetWheelAbility(Wheel, WheelHover).IsNone())
		{
			SetWheelPicked(Wheel, WheelHover);
			if (FWRKeyBindings::Get().CastOnRelease()) CastAbility(GetWheelAbility(Wheel, WheelHover));
		}
		OpenWheel = -1;
		WheelHover = -1;
		UpdateInputMode();
		return;
	}
	CastAbility(GetWheelAbility(Wheel, WheelPicked[Wheel]));
}

void FWRPlayerUI::UpdateWheelHover(int32& OutHover, int32 Count) const
{
	const AWRPlayerController* P = PC.Get();
	float MX = 0.f, MY = 0.f;
	int32 SX = 0, SY = 0;
	if (!P->GetMousePosition(MX, MY)) return;
	P->GetViewportSize(SX, SY);
	const FVector2D D(MX - SX * 0.5f, MY - SY * 0.5f);
	// Scale the dead zone with the screen (the wheel's centre disc).
	if (D.Size() < SY * 0.07f) return;
	const float Deg = FMath::Fmod(FMath::RadiansToDegrees(FMath::Atan2(D.X, -D.Y)) + 360.f, 360.f);
	OutHover = FMath::RoundToInt(Deg / (360.f / Count)) % Count;
}

void FWRPlayerUI::DebugWheel(int32 Which, int32 Hover)
{
	bDebugWheel = Which >= 0;
	OpenWheel = Which == 0 || Which == 1 ? Which : -1;
	WheelHover = Which <= 1 ? Hover : -1;
	bTameWheel = Which == 2;
	bTameSticky = false;
	TameHover = Which == 2 ? Hover : -1;
	UpdateInputMode();
}

const TArray<TPair<FName, FText>>& FWRPlayerUI::TameOrders()
{
	// Wheel positions clockwise from the top; empty names are blank wedges.
	static const TArray<TPair<FName, FText>> Orders = {
		{ TEXT("follow"), LOCTEXT("Follow", "Follow") },
		{ TEXT("stay"), LOCTEXT("Stay", "Stay") },
		{ TEXT("ride"), LOCTEXT("Ride", "Ride") },
		{ NAME_None, FText::GetEmpty() },
		{ TEXT("release"), LOCTEXT("Release", "Release") },
		{ NAME_None, FText::GetEmpty() },
		{ TEXT("passive"), LOCTEXT("Passive", "Passive") },
		{ TEXT("aggressive"), LOCTEXT("Aggressive", "Aggressive") },
	};
	return Orders;
}

void FWRPlayerUI::TameKey(bool bDown)
{
	AWRPlayerController* P = PC.Get();
	if (bDown)
	{
		bTameKeyDown = true;
		TameDownAt = FPlatformTime::Seconds();
		return;
	}
	const bool bWasHeld = bTameKeyDown;
	bTameKeyDown = false;
	if (!bWasHeld) return;
	if (!bTameWheel)
	{
		// A tap: the usual interaction (your tame's order wheel, staying up until you pick).
		P->InteractNow();
		return;
	}
	if (!bTameSticky) PickTameOrder(TameHover);
}

void FWRPlayerUI::PickTameOrder(int32 Pos)
{
	AWRPlayerController* P = PC.Get();
	const TArray<TPair<FName, FText>>& Orders = TameOrders();
	if (Pos >= 0 && Orders.IsValidIndex(Pos) && !Orders[Pos].Key.IsNone())
	{
		P->SendCommand(TEXT("tame ") + Orders[Pos].Key.ToString());
	}
	bTameWheel = false;
	bTameSticky = false;
	TameHover = -1;
	if (P->GetMenu() == EWRMenu::Tame) P->SetMenu(EWRMenu::None);
	UpdateInputMode();
}

#undef LOCTEXT_NAMESPACE
