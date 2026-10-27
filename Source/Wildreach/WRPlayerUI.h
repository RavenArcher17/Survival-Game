#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "WRUIWidgets.h"

class AWRPlayerController;
class SWidget;

/**
 * The stone-and-leather UI for one local player: the HUD overlay, the screen that matches the controller's menu
 * state (journal tabs, storage, Hearth, claim, taming, build bar, settings), the item hotbar and the ability wheels.
 * Screens and journal tabs live in their own files and register a factory by name (WR_REGISTER_SCREEN /
 * WR_REGISTER_JOURNAL_TAB), so new screens don't touch this one. Gameplay still goes through the controller's
 * server commands; the UI only reads replicated state and sends those commands.
 */

using FWRScreenFactory = TFunction<TSharedRef<SWidget>(AWRPlayerController*)>;

struct WILDREACH_API FWRScreens
{
	static void Register(FName Name, FWRScreenFactory Factory);
	static void RegisterTab(FName Tab, FWRScreenFactory Factory);
	static const FWRScreenFactory* Find(FName Name);
	static const FWRScreenFactory* FindTab(FName Tab);
};

struct FWRScreenAutoRegister
{
	FWRScreenAutoRegister(FName Name, FWRScreenFactory Factory, bool bTab) { bTab ? FWRScreens::RegisterTab(Name, MoveTemp(Factory)) : FWRScreens::Register(Name, MoveTemp(Factory)); }
};
#define WR_REGISTER_SCREEN(Name, Factory) static FWRScreenAutoRegister UE_JOIN(GWRScreen_, __COUNTER__)(FName(TEXT(Name)), Factory, false)
#define WR_REGISTER_JOURNAL_TAB(Name, Factory) static FWRScreenAutoRegister UE_JOIN(GWRTab_, __COUNTER__)(FName(TEXT(Name)), Factory, true)

/** A castable path ability as the wheel and the HUD show it. */
struct FWRCastable
{
	FName NodeId;          // the path node ("fire_bolt")
	FText Name;
	FName Path;            // fire / frost
	int32 InputIndex = -1; // quick slot 0-7 (the ability's GAS input id is Slot1 + this)
	float Cooldown = 0.f;  // seconds left
	float Cost = 0.f;      // Heat / Chill
	bool bLearned = true;
};

class WILDREACH_API FWRPlayerUI
{
public:
	explicit FWRPlayerUI(AWRPlayerController* InPC);
	~FWRPlayerUI();

	void Tick(float DeltaTime);
	/** The screen shown right now (NAME_None: just the HUD). */
	FName GetScreen() const { return ScreenName; }
	bool IsScreenOpen() const { return ScreenWidget.IsValid(); }
	AWRPlayerController* GetPC() const { return PC.Get(); }

	// --- Item hotbar (1-8): each key holds an item id; pressing it equips or uses that item from the bag.
	FName GetHotbarItem(int32 Index) const { return Hotbar.IsValidIndex(Index) ? Hotbar[Index] : NAME_None; }
	int32 GetHotbarSelected() const { return HotbarSelected; }
	void SetHotbarItem(int32 Index, FName ItemId);
	void UseHotbar(int32 Index);
	void CycleHotbar(int32 Dir);
	FWRSlotView HotbarView(int32 Index) const;

	// --- Ability wheels: two wheels of 8, each with a picked ability that a tap of its key casts.
	TArray<FWRCastable> GetCastables() const;
	FName GetWheelAbility(int32 Wheel, int32 Pos) const;
	void SetWheelAbility(int32 Wheel, int32 Pos, FName NodeId);
	int32 GetWheelPicked(int32 Wheel) const { return WheelPicked[FMath::Clamp(Wheel, 0, 1)]; }
	void SetWheelPicked(int32 Wheel, int32 Pos);
	/** Wheel key pressed / released (a tap casts the picked ability, a hold opens the wheel). */
	void WheelKey(int32 Wheel, bool bDown);
	int32 GetOpenWheel() const { return OpenWheel; }
	int32 GetWheelHover() const { return WheelHover; }
	void CastAbility(FName NodeId);
	const FWRCastable* FindCastable(FName NodeId, const TArray<FWRCastable>& In) const;

	// --- Tame order wheel (hold E on your tame).
	/** Interact key on your own tame: a hold opens the order wheel (release on an order), a tap interacts as usual. */
	void TameKey(bool bDown);
	bool IsTameWheelOpen() const { return bTameWheel; }
	/** The wheel was opened by a tap and waits for a click. */
	bool IsTameWheelSticky() const { return bTameSticky; }
	int32 GetTameHover() const { return TameHover; }
	/** Give the order at a wheel position (a click on the wheel, or the hold released) and close it. */
	void PickTameOrder(int32 Pos);
	static const TArray<TPair<FName, FText>>& TameOrders();

	/** Short-lived "+3 Wood" lines (from the server's notices); more of the same item stacks onto its line. */
	struct FPickup
	{
		FString Text;
		double Time = 0.0;      // last added to (WRUIMotion::Now)
		FString Item;           // "Wood" (empty: a line that doesn't stack)
		int32 Count = 0;
		FString Tail;           // "  (Stone hatchet)"
		double Bumped = -100.0; // when it last stacked (a little pulse)
	};
	const TArray<FPickup>& GetPickups() const { return Pickups; }
	/** When the newest line came in (the stack eases up to make room). */
	double GetPickupAddedAt() const { return PickupAddedAt; }
	void NoteNotice(const FString& Text);
	/** The centre notice showing now and its fade (0..1); empty when none. Notices queue, so none is lost. */
	const FString& GetCentreNotice(float& OutAlpha) const;
	/** The moments between play: death and loading covers, banners, hints (WRUIFlow). */
	class FWRFlow* GetFlow() const { return Flow.Get(); }

	/** Testing / screenshots: hold a wheel open (0, 1 = ability wheels, 2 = tame orders) with a wedge lit; -1 closes. */
	void DebugWheel(int32 Which, int32 Hover);

	/** Something else set the input mode (the chat box or the main menu closing sets game-only): put back what the
	 *  open screen needs, or a menu left open has no cursor and the camera swings with the mouse. */
	void ReapplyInputMode();

	/** Help overlay (H). */
	bool IsHelpShown() const { return bHelp; }
	void ToggleHelp() { bHelp = !bHelp; }

private:
	void SyncScreen();
	void ShowScreen(FName Name);
	void CloseScreen();
	void UpdateInputMode();
	void LoadHotbar();
	void SaveHotbar() const;
	void AutoFillHotbar();
	void UpdateWheelHover(int32& OutHover, int32 Count) const;
	void CentreMouse();

	TWeakObjectPtr<AWRPlayerController> PC;
	TSharedPtr<SWidget> HudWidget;
	TSharedPtr<SWidget> ScreenWidget;
	/** The screen's motion wrapper (what the viewport holds), and closed screens still fading out. */
	TSharedPtr<class SWRAppear> ScreenHost;
	TArray<TSharedPtr<class SWRAppear>> LeavingScreens;
	TSharedPtr<SWidget> WheelWidget;
	TSharedPtr<class FWRFlow> Flow;
	FName ScreenName;
	bool bScreenWantsCursor = false;
	bool bScreenStopsMove = false;
	double NextAbilityKeyCheck = 0.0;

	TArray<FName> Hotbar;
	int32 HotbarSelected = 0;
	bool bHotbarLoaded = false;
	/** Items you took off the bar: they don't come back on their own. */
	TSet<FName> RemovedFromBar;

	FName Wheels[2][8];
	int32 WheelPicked[2] = { 0, 0 };
	bool bWheelsLoaded = false;
	int32 OpenWheel = -1;
	int32 WheelHover = -1;
	int32 WheelDownKey = -1;
	double WheelDownAt = 0.0;
	bool bTameWheel = false;
	bool bTameSticky = false;
	bool bTameKeyDown = false;
	double TameDownAt = 0.0;
	int32 TameHover = -1;

	TArray<FPickup> Pickups;
	double PickupAddedAt = -100.0;
	void UpdateNotices();
	TArray<FString> NoticeQueue;
	FString CurNotice;
	double CurNoticeAt = -100.0;
	double CurNoticeEnd = -100.0;
	bool bHelp = false;
	bool bDebugWheel = false;
	void LoadWheels();
	void SaveWheels() const;
};
