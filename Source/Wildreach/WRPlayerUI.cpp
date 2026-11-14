#include "WRPlayerUI.h"
#include "WRSurvival.h"
#include "Wildreach.h"
#include "WRAbilities.h"
#include "WRBuilding.h"
#include "WRCharacter.h"
#include "WRCombatTypes.h"
#include "WRInventory.h"
#include "WRKeyBindings.h"
#include "WRMainMenu.h"
#include "WRPadInput.h"
#include "WRReport.h"
#include "WRPathComponent.h"
#include "WRPathTypes.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRSound.h"
#include "WRTuning.h"
#include "WRUIFlow.h"
#include "WRUIMotion.h"
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
	Flow = MakeShared<FWRFlow>(InPC);
	// Controller support, F8 and F1 (one input processor for the whole game).
	WRInput::Install();
}

FWRPlayerUI::~FWRPlayerUI()
{
	Flow.Reset();
	if (GEngine && GEngine->GameViewport)
	{
		TArray<TSharedPtr<SWidget>> All = { HudWidget, ScreenHost, WheelWidget };
		for (const TSharedPtr<SWRAppear>& W : LeavingScreens) All.Add(W);
		for (const TSharedPtr<SWidget>& W : All)
		{
			if (W.IsValid()) GEngine->GameViewport->RemoveViewportWidgetContent(W.ToSharedRef());
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
		bWhistle = bTameKeyWhistle;
		bTameSticky = false;
		TameHover = -1;
		CentreMouse();
	}
	if (bWhistleKeyDown && !bTameWheel && Now - WhistleDownAt > WRTune(TEXT("taming.whistleHoldSeconds"), 0.25f))
	{
		bTameWheel = true;
		bWhistle = true;
		bTameSticky = false;
		TameHover = -1;
		CentreMouse();
	}
	if (bTameWheel && !bDebugWheel) UpdateWheelHover(TameHover, 8);
	if (bDoorKeyDown && Now - DoorDownAt > WRTune(TEXT("ui.doorHoldSeconds"), 0.35f))
	{
		// Held on your own code-lock door: its keypad, to set or remove the code.
		bDoorKeyDown = false;
		if (DoorKeyPiece.IsValid()) P->OpenDoorKeypad(DoorKeyPiece.Get());
	}
	const double UINow = WRUIMotion::Now();
	Pickups.RemoveAll([UINow](const FPickup& X) { return UINow - X.Time > 3.5; });
	UpdateNotices();
	// Closed screens fade out on their own; drop them once they're gone (or if they stopped ticking).
	for (int32 i = LeavingScreens.Num() - 1; i >= 0; --i)
	{
		if (!LeavingScreens[i].IsValid() || LeavingScreens[i]->IsGone())
		{
			if (LeavingScreens[i].IsValid() && GEngine && GEngine->GameViewport) GEngine->GameViewport->RemoveViewportWidgetContent(LeavingScreens[i].ToSharedRef());
			LeavingScreens.RemoveAt(i);
		}
	}
	if (Flow)
	{
		Flow->Tick(DeltaTime);
		UpdateInputMode();  // (acts only on a change: the death screen's choices come and go)
	}
	UpdateIsolation();
	// Abilities can have their own keys (Settings): make sure each learned one has an entry.
	if (Now > NextAbilityKeyCheck)
	{
		NextAbilityKeyCheck = Now + 1.0;
		for (const FWRCastable& C : GetCastables()) FWRKeyBindings::Get().EnsureAbility(C.NodeId, C.Name);
		PlaceStarterSpell();
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
	case EWRMenu::Inventory: case EWRMenu::Craft: case EWRMenu::Skills: case EWRMenu::Paths: case EWRMenu::Map: case EWRMenu::Clan: Want = TEXT("journal"); break;
	case EWRMenu::Build: Want = TEXT("build"); break;
	case EWRMenu::Chest: Want = TEXT("storage"); break;
	case EWRMenu::Claim: Want = TEXT("claim"); break;
	case EWRMenu::Hearth: Want = TEXT("hearth"); break;
	case EWRMenu::Taming: Want = TEXT("taming"); break;
	case EWRMenu::Settings: Want = TEXT("settings"); break;
	case EWRMenu::Door: Want = TEXT("door"); break;
	case EWRMenu::Cook: Want = TEXT("cook"); break;
	case EWRMenu::Machine: Want = TEXT("machine"); break;
	case EWRMenu::TameCard: Want = TEXT("tamecard"); break;
	case EWRMenu::Incubator: Want = TEXT("incubator"); break;
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
		bWhistle = false;
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
		WRSound::PlayUI(P, Want.IsNone() ? FName(TEXT("ui.close")) : FName(TEXT("ui.open")));
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
	// Screens ease in (a short fade and rise); the build bar is a strip, so it only fades.
	const bool bBar = Name == TEXT("build");
	ScreenHost = SNew(SWRAppear).Seconds(WRTune(TEXT("flow.screenInSeconds"), 0.15f)).Offset(FVector2D(0.f, bBar ? 0.f : WRTune(TEXT("flow.screenRise"), 14.f)))[ScreenWidget.ToSharedRef()];
	GEngine->GameViewport->AddViewportWidgetContent(ScreenHost.ToSharedRef(), 20);
	WRInput::AddNavRoot(ScreenHost.ToSharedRef(), 20);
	UE_LOG(LogWildreach, Log, TEXT("UI screen %s"), *Name.ToString());
	WRReport::Event(TEXT("screen"), Name.ToString());
}

void FWRPlayerUI::CloseScreen()
{
	if (ScreenHost.IsValid())
	{
		WRInput::RemoveNavRoot(ScreenHost);
		WRReport::Event(TEXT("screen"), TEXT("closed ") + ScreenName.ToString());
		// It fades out where it is and takes no more clicks; Tick removes it when it's gone.
		ScreenHost->PlayOut(WRTune(TEXT("flow.screenOutSeconds"), 0.12f));
		LeavingScreens.Add(ScreenHost);
	}
	ScreenHost.Reset();
	ScreenWidget.Reset();
	ScreenName = NAME_None;
}

void FWRPlayerUI::UpdateInputMode()
{
	AWRPlayerController* P = PC.Get();
	// Menus want the cursor and stop the camera; the build bar only while no piece is being placed.
	const bool bMenu = ScreenWidget.IsValid() && !(ScreenName == TEXT("build") && P->IsPlacing());
	const bool bWheel = OpenWheel >= 0 || bTameWheel;
	// The death screen while it offers places to wake.
	const bool bDeath = Flow && Flow->WantsCursor();
	const bool bWant = bMenu || bWheel || bDeath;
	const bool bStill = bMenu && ScreenName != TEXT("build");
	bScreenCursor = bMenu || bDeath;
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

void FWRPlayerUI::UpdateIsolation()
{
	// Held-input isolation (WRInput::Transition): entering a mode that takes the buttons drops what's held; leaving it,
	// gameplay presses wait a moment. Screens and pages also flush the keys, so a stick or key held into a menu doesn't
	// keep the survivor walking under it.
	AWRPlayerController* P = PC.Get();
	const bool bBusy = bScreenCursor || WRReport::IsOpen() || WRControlsPage::IsOpen() || SWRChatBox::IsOpen() || SWRMainMenu::IsOpen();
	const bool bWheel = OpenWheel >= 0 || bTameWheel;
	const bool bPlacing = P->IsPlacing();
	APawn* Pawn = P->GetPawn();
	if (bBusy != bWasBusy)
	{
		if (bBusy)
		{
			if (AWRCharacter* C = Cast<AWRCharacter>(Pawn)) C->DropHeldInputs();
			P->FlushPressedKeys();
		}
		else WRInput::Transition(P, TEXT("left a menu"));
		bWasBusy = bBusy;
	}
	if (bWheel != bWasWheel)
	{
		if (bWheel) { if (AWRCharacter* C = Cast<AWRCharacter>(Pawn)) C->DropHeldInputs(); }
		else WRInput::Transition(P, TEXT("closed a wheel"));
		bWasWheel = bWheel;
	}
	if (bPlacing != bWasPlacing)
	{
		WRInput::Transition(P, bPlacing ? TEXT("build placing") : TEXT("stopped placing"));
		bWasPlacing = bPlacing;
	}
	if (Pawn != LastPawn.Get())
	{
		// Mounting, dismounting, a new body after death.
		if (LastPawn.IsValid() || Pawn) WRInput::Transition(P, Pawn ? *(TEXT("now controlling ") + Pawn->GetName()) : TEXT("no body"));
		LastPawn = Pawn;
	}
}

void FWRPlayerUI::ToggleHelp()
{
	WRControlsPage::Toggle();
}

void FWRPlayerUI::PadOpenMenus()
{
	AWRPlayerController* P = PC.Get();
	if (!P) return;
	// A piece being placed is put away first (the build bar's ghost never stays out under another page).
	if (P->IsPlacing()) P->CancelPlacement();
	const EWRMenu Want = PadLastTab == 0 ? EWRMenu::Inventory : static_cast<EWRMenu>(PadLastTab);
	P->SetMenu(Want);
}

void FWRPlayerUI::PadTab(int32 Dir)
{
	AWRPlayerController* P = PC.Get();
	if (!P) return;
	static const EWRMenu Order[] = { EWRMenu::Inventory, EWRMenu::Craft, EWRMenu::Build, EWRMenu::Paths, EWRMenu::Skills, EWRMenu::Map, EWRMenu::Clan, EWRMenu::Settings };
	const int32 N = UE_ARRAY_COUNT(Order);
	int32 At = INDEX_NONE;
	for (int32 i = 0; i < N; ++i) if (Order[i] == P->GetMenu()) At = i;
	if (At == INDEX_NONE || P->IsPlacing()) return;   // a station's screen, a chest, a keypad: LB / RB have no pages there
	const EWRMenu Next = Order[((At + Dir) % N + N) % N];
	// A crafting station's page stays that station's; the journal's Crafting page is crafting by hand or at the nearest.
	P->SetMenu(Next);
	PadLastTab = static_cast<uint8>(Next == EWRMenu::Settings || Next == EWRMenu::Build ? EWRMenu::Inventory : Next);
	WRSound::PlayUI(P, TEXT("ui.click"));
}

void FWRPlayerUI::PadQuickStep(int32 Dir)
{
	// Both wheels as one ring of 16 quick slots, skipping empty ones and abilities not learned.
	const TArray<FWRCastable> All = GetCastables();
	const int32 Wheel = WRInput::PadQuickWheel();
	const int32 From = Wheel * 8 + WheelPicked[Wheel];
	for (int32 Step = 1; Step <= 16; ++Step)
	{
		const int32 i = ((From + Dir * Step) % 16 + 16) % 16;
		const FWRCastable* C = FindCastable(GetWheelAbility(i / 8, i % 8), All);
		if (!C || !C->bLearned) continue;
		WRInput::SetPadQuickWheel(i / 8);
		SetWheelPicked(i / 8, i % 8);
		if (AWRPlayerController* P = PC.Get())
		{
			WRSound::PlayUI(P, TEXT("ui.click"));
			P->ShowLocalNotice(C->Name.ToString());
		}
		return;
	}
	if (AWRPlayerController* P = PC.Get()) P->ShowLocalNotice(TEXT("No abilities on your wheels: learn them in Paths"));
}

void FWRPlayerUI::CastAbilityHeld(FName NodeId)
{
	bQuickHeld = true;
	CastAbility(NodeId);
	bQuickHeld = false;
}

void FWRPlayerUI::ReleaseQuickSlot()
{
	AWRCharacter* Survivor = PC.IsValid() ? Cast<AWRCharacter>(PC->GetPawn()) : nullptr;
	if (Survivor && HeldQuickInput != EWRAbilityInput::None)
	{
		Survivor->ReleaseAbilityInput(HeldQuickInput);
	}
	HeldQuickInput = EWRAbilityInput::None;
}

void FWRPlayerUI::PadQuickFire()
{
	const int32 Wheel = WRInput::PadQuickWheel();
	if (!FindCastable(GetWheelAbility(Wheel, WheelPicked[Wheel]), GetCastables()))
	{
		// The slot the pad was on emptied (or was never filled): the first one there is.
		PadQuickStep(1);
	}
	const int32 Now = WRInput::PadQuickWheel();
	CastAbilityHeld(GetWheelAbility(Now, WheelPicked[Now]));
}

void FWRPlayerUI::PadBeltStep(int32 Dir)
{
	for (int32 Step = 1; Step <= 8; ++Step)
	{
		const int32 i = ((PadBelt + Dir * Step) % 8 + 8) % 8;
		const FWRItemDef* Def = Hotbar[i].IsNone() ? nullptr : FWRItemRegistry::Get().Find(Hotbar[i]);
		if (!Def) continue;
		PadBelt = i;
		if (AWRPlayerController* P = PC.Get())
		{
			WRSound::PlayUI(P, TEXT("ui.click"));
			P->ShowLocalNotice(Def->Name);
		}
		return;
	}
	if (AWRPlayerController* P = PC.Get()) P->ShowLocalNotice(TEXT("Your belt is empty: put items on the hotbar in the Inventory"));
}

void FWRPlayerUI::PadBeltUse()
{
	if (GetHotbarItem(PadBelt).IsNone()) PadBeltStep(1);
	if (!GetHotbarItem(PadBelt).IsNone()) UseHotbar(PadBelt);
}

void FWRPlayerUI::ReapplyInputMode()
{
	// UpdateInputMode only acts on a change, so make what it last set look different.
	bScreenWantsCursor = !bScreenWantsCursor;
	UpdateInputMode();
}

// ---------------------------------------------------------------- notices

void FWRPlayerUI::NoteNotice(const FString& Text)
{
	const double Now = WRUIMotion::Now();
	// "+3 Wood" style pickups stack above the hotbar; everything else is a centre notice (the HUD reads it).
	if (Text.StartsWith(TEXT("+")))
	{
		// "+3 Wood  (Stone hatchet)": more wood while its line is up adds to it ("+5 Wood") instead of a new line.
		FPickup New;
		New.Text = Text;
		New.Time = Now;
		FString Rest = Text.Mid(1);
		int32 Space = INDEX_NONE;
		if (Rest.FindChar(TEXT(' '), Space) && Rest.Left(Space).IsNumeric())
		{
			New.Count = FCString::Atoi(*Rest.Left(Space));
			Rest = Rest.Mid(Space + 1);
			const int32 Paren = Rest.Find(TEXT("  ("));
			New.Item = Paren == INDEX_NONE ? Rest : Rest.Left(Paren);
			New.Tail = Paren == INDEX_NONE ? FString() : Rest.Mid(Paren);
		}
		if (FPickup* Same = New.Item.IsEmpty() ? nullptr : Pickups.FindByPredicate([&New](const FPickup& P) { return P.Item == New.Item; }))
		{
			Same->Count += New.Count;
			Same->Tail = New.Tail;
			Same->Text = FString::Printf(TEXT("+%d %s%s"), Same->Count, *Same->Item, *Same->Tail);
			Same->Time = Now;
			Same->Bumped = Now;
			return;
		}
		Pickups.Add(New);
		PickupAddedAt = Now;
		if (Pickups.Num() > 5) Pickups.RemoveAt(0);
		return;
	}
	if (Text.IsEmpty()) return;
	// Notices take turns: the same one again just stays up longer; a new one cuts the current one short (after a
	// minimum, so each can be read) instead of replacing it at once.
	const float Hold = WRTune(TEXT("flow.noticeSeconds"), 4.f);
	if (Text == CurNotice && Now < CurNoticeEnd)
	{
		CurNoticeEnd = FMath::Max(CurNoticeEnd, Now + Hold);
		return;
	}
	if (NoticeQueue.Contains(Text)) return;
	NoticeQueue.Add(Text);
	if (NoticeQueue.Num() > 5) NoticeQueue.RemoveAt(0);
	if (!CurNotice.IsEmpty())
	{
		CurNoticeEnd = FMath::Min(CurNoticeEnd, FMath::Max(CurNoticeAt + WRTune(TEXT("flow.noticeMinSeconds"), 1.6f), Now + 0.25));
	}
}

void FWRPlayerUI::UpdateNotices()
{
	const double Now = WRUIMotion::Now();
	if (!CurNotice.IsEmpty() && Now >= CurNoticeEnd)
	{
		CurNotice.Empty();
	}
	if (CurNotice.IsEmpty() && NoticeQueue.Num() > 0)
	{
		CurNotice = NoticeQueue[0];
		NoticeQueue.RemoveAt(0);
		CurNoticeAt = Now;
		CurNoticeEnd = Now + (NoticeQueue.Num() > 0 ? WRTune(TEXT("flow.noticeMinSeconds"), 1.6f) : WRTune(TEXT("flow.noticeSeconds"), 4.f));
	}
}

const FString& FWRPlayerUI::GetCentreNotice(float& OutAlpha) const
{
	const double Now = WRUIMotion::Now();
	OutAlpha = CurNotice.IsEmpty() ? 0.f : FMath::Min(WRUIMotion::EaseOut(float(Now - CurNoticeAt) / 0.15f), FMath::Clamp(float(CurNoticeEnd - Now) / 0.25f, 0.f, 1.f));
	return CurNotice;
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
			else if (Def->Slot == EWREquipSlot::Light) P->SendCommand(TEXT("light"));   // the torch in hand: light it / put it out
			return;
		}
	}
	// A tool is weapon-hand equipment: picking it puts it in the right hand in place of the sword (and back).
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
		// Weapons, tools and ranged: not armour, talismans, rings or lights.
		if (Def && (Def->Slot == EWREquipSlot::Weapon || Def->Slot == EWREquipSlot::Ranged))
		{
			UseHotbar(i);
			return;
		}
	}
}

FWRSlotView FWRPlayerUI::HotbarView(int32 Index) const
{
	FWRSlotView V;
	// On a pad the belt's chosen slot shows the button that uses it (Y), the others nothing.
	V.Key = WRInput::UsingPad() ? (Index == PadBelt ? WRInput::PadButton(TEXT("UseBelt")) : FText::GetEmpty())
		: WRUI::KeyName(FWRKeyBindings::Get().Key(FName(*FString::Printf(TEXT("Hotbar%d"), Index + 1))));
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
		const UWRSurvivalComponent* Survival = PS ? PS->GetSurvival() : nullptr;
		V.bSelected = bWorn && (Def->Slot == EWREquipSlot::Ranged ? (Survivor && Survivor->IsRangedOut())
			: Def->Slot == EWREquipSlot::Light ? (Survival && Survival->IsLightOn())
			: (Def->Slot == EWREquipSlot::Weapon && !(Survivor && Survivor->IsRangedOut())));
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
		// A Wildbrand that doesn't fit what's in your hand greys out with the reason (combat.md 2.12).
		UWRGA_Melee::WildbrandFits(Survivor, FName(*NodeStr), C.Blocked);
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

void FWRPlayerUI::PlaceStarterSpell()
{
	// The main path's starter spell goes on wheel 1, on the picked wedge, so a tap of the wheel key (Q) casts it from
	// the first minute. Placed once per spell on this machine: moved or taken off later, it stays where the player put it.
	const AWRCharacter* Survivor = PC.IsValid() ? Cast<AWRCharacter>(PC->GetPawn()) : nullptr;
	const UWRPathComponent* Paths = Survivor ? UWRPathComponent::FindFor(Survivor) : nullptr;
	const FWRPathDef* Def = Paths ? FWRPathRegistry::Get().Find(Paths->GetMainPath()) : nullptr;
	if (!Def || Def->Starter.IsNone())
	{
		return;
	}
	const FName Starter(*FString::Printf(TEXT("%s.%s"), *Def->Id.ToString(), *Def->Starter.ToString()));
	const TArray<FWRCastable> All = GetCastables();
	if (!FindCastable(Starter, All))
	{
		return;  // not granted to this client yet
	}
	int32 StarterPos = INDEX_NONE;
	bool bOnWheel = false;
	for (int32 W = 0; W < 2; ++W) for (int32 i = 0; i < 8; ++i)
	{
		if (Wheels[W][i] == Starter)
		{
			bOnWheel = true;
			StarterPos = W == 0 ? i : StarterPos;
		}
	}
	FString Done;
	GConfig->GetString(UISection, TEXT("StarterWheeled"), Done, GGameUserSettingsIni);
	TArray<FString> DoneList;
	Done.ParseIntoArray(DoneList, TEXT(","));
	if (!bOnWheel && !DoneList.Contains(Starter.ToString()))
	{
		// The picked wedge, unless the player put another ability there: then the first empty wedge.
		const int32 Picked = WheelPicked[0];
		const bool bPickedTaken = !Wheels[0][Picked].IsNone() && FindCastable(Wheels[0][Picked], All);
		StarterPos = bPickedTaken ? INDEX_NONE : Picked;
		for (int32 i = 0; i < 8 && StarterPos == INDEX_NONE; ++i)
		{
			if (Wheels[0][i].IsNone()) StarterPos = i;
		}
		if (StarterPos != INDEX_NONE)
		{
			SetWheelAbility(0, StarterPos, Starter);
			DoneList.AddUnique(Starter.ToString());
			GConfig->SetString(UISection, TEXT("StarterWheeled"), *FString::Join(DoneList, TEXT(",")), GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
			UE_LOG(LogWildreach, Log, TEXT("UI starter %s on wheel 1 wedge %d (picked %d)"), *Starter.ToString(), StarterPos, WheelPicked[0]);
		}
	}
	// A tap of Q never lands on nothing while the starter is on wheel 1 (the picked wedge may hold another path's spell).
	if (StarterPos != INDEX_NONE && !FindCastable(GetWheelAbility(0, WheelPicked[0]), All))
	{
		SetWheelPicked(0, StarterPos);
		UE_LOG(LogWildreach, Log, TEXT("UI wheel 1 picks the starter %s (wedge %d)"), *Starter.ToString(), StarterPos);
	}
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
	if (!C->Blocked.IsEmpty())
	{
		if (PC.IsValid()) PC->ShowLocalNotice(FString::Printf(TEXT("%s: %s"), *C->Name.ToString(), *C->Blocked));
		return;
	}
	const EWRAbilityInput Input = static_cast<EWRAbilityInput>(static_cast<int32>(EWRAbilityInput::Slot1) + C->InputIndex);
	Survivor->PressAbilityInput(Input);
	// A held quick-slot key or D-pad up charges what can be charged (Molten cleave, Ice lance...): its release comes
	// from ReleaseQuickSlot; a click or a wheel pick casts at once.
	if (!bQuickHeld)
	{
		Survivor->ReleaseAbilityInput(Input);
	}
	else
	{
		HeldQuickInput = Input;
	}
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

const TArray<TPair<FName, FText>>& FWRPlayerUI::WhistleOrders()
{
	static const TArray<TPair<FName, FText>> Orders = {
		{ TEXT("follow"), LOCTEXT("WhistleFollow", "Come / follow") },
		{ TEXT("stay"), LOCTEXT("WhistleStay", "Stay") },
		{ NAME_None, FText::GetEmpty() },
		{ NAME_None, FText::GetEmpty() },
		{ NAME_None, FText::GetEmpty() },
		{ NAME_None, FText::GetEmpty() },
		{ TEXT("passive"), LOCTEXT("WhistlePassive", "Passive") },
		{ TEXT("aggressive"), LOCTEXT("WhistleAggressive", "Aggressive") },
	};
	return Orders;
}

const TArray<TPair<FName, FText>>& FWRPlayerUI::TameOrders()
{
	// Wheel positions clockwise from the top; empty names are blank wedges.
	static const TArray<TPair<FName, FText>> Orders = {
		{ TEXT("follow"), LOCTEXT("Follow", "Follow") },
		{ TEXT("stay"), LOCTEXT("Stay", "Stay") },
		{ TEXT("ride"), LOCTEXT("Ride", "Ride") },
		{ TEXT("tend"), LOCTEXT("Tend", "Tend") },
		{ TEXT("release"), LOCTEXT("Release", "Release") },
		{ NAME_None, FText::GetEmpty() },
		{ TEXT("passive"), LOCTEXT("Passive", "Passive") },
		{ TEXT("aggressive"), LOCTEXT("Aggressive", "Aggressive") },
	};
	return Orders;
}

void FWRPlayerUI::WhistleKey(bool bDown)
{
	AWRPlayerController* P = PC.Get();
	if (!P)
	{
		return;
	}
	if (bDown)
	{
		if (P->GetMenu() != EWRMenu::None || bTameWheel || OpenWheel >= 0) return;
		bWhistleKeyDown = true;
		WhistleDownAt = FPlatformTime::Seconds();
		return;
	}
	if (!bWhistleKeyDown) return;
	bWhistleKeyDown = false;
	if (bTameWheel && bWhistle)
	{
		PickTameOrder(TameHover);   // released on an order (or in the middle: nothing)
		return;
	}
	// A tap: every tame of yours in earshot comes to you.
	P->SendCommand(TEXT("whistle follow"));
}

void FWRPlayerUI::TameKey(bool bDown, bool bWhistleMode)
{
	AWRPlayerController* P = PC.Get();
	if (bDown)
	{
		bTameKeyDown = true;
		bTameKeyWhistle = bWhistleMode;
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

void FWRPlayerUI::DoorKey(bool bDown, AWRBuildPiece* Door)
{
	if (bDown)
	{
		bDoorKeyDown = true;
		DoorDownAt = FPlatformTime::Seconds();
		DoorKeyPiece = Door;
		return;
	}
	if (!bDoorKeyDown) return;   // the hold already opened the keypad
	bDoorKeyDown = false;
	if (AWRPlayerController* P = PC.Get()) P->InteractNow();   // a tap: open or shut it
}

void FWRPlayerUI::PickTameOrder(int32 Pos)
{
	AWRPlayerController* P = PC.Get();
	const TArray<TPair<FName, FText>>& Orders = bWhistle ? WhistleOrders() : TameOrders();
	// Tend: the tame's card (its stats, skills, tack, family and packs: cooks/build-taming.md 2.8), opened here.
	const bool bTend = !bWhistle && Pos >= 0 && Orders.IsValidIndex(Pos) && Orders[Pos].Key == TEXT("tend") && P->GetMenuCreature();
	if (!bTend && Pos >= 0 && Orders.IsValidIndex(Pos) && !Orders[Pos].Key.IsNone() && (!bWhistle || P->TamesInEarshot() > 0))
	{
		P->SendCommand((bWhistle ? TEXT("whistle ") : TEXT("tame ")) + Orders[Pos].Key.ToString());
	}
	bWhistle = false;
	bTameWheel = false;
	bTameSticky = false;
	TameHover = -1;
	if (bTend) P->SetMenu(EWRMenu::TameCard);
	else if (P->GetMenu() == EWRMenu::Tame) P->SetMenu(EWRMenu::None);
	UpdateInputMode();
}

#undef LOCTEXT_NAMESPACE
