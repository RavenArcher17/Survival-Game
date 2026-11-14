// Controller support, the device in use and held-input isolation (WRPadInput.h has the overview).

#include "WRPadInput.h"
#include "Wildreach.h"
#include "WRCharacter.h"
#include "WRKeyBindings.h"
#include "WRMainMenu.h"
#include "WRPlayerController.h"
#include "WRPlayerUI.h"
#include "WRReport.h"
#include "WRTuning.h"
#include "WRUIWidgets.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "GameFramework/InputSettings.h"
#include "Layout/WidgetPath.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"

#define LOCTEXT_NAMESPACE "WRPad"

// ---------------------------------------------------------------- the layout (Data/gamepad.json)

namespace WRPadDetail
{
	/** Used when Data/gamepad.json is missing a verb (the file is the source; this keeps a broken file playable). */
	const TPair<const TCHAR*, const TCHAR*> DefaultVerbs[] = {
		{ TEXT("Light"), TEXT("Gamepad_RightShoulder") }, { TEXT("Heavy"), TEXT("Gamepad_RightTrigger") },
		{ TEXT("Block"), TEXT("Gamepad_LeftShoulder") }, { TEXT("Sprint"), TEXT("Gamepad_LeftTrigger") },
		{ TEXT("Jump"), TEXT("Gamepad_FaceButton_Bottom") }, { TEXT("Dodge"), TEXT("Gamepad_FaceButton_Right") },
		{ TEXT("Interact"), TEXT("Gamepad_FaceButton_Left") }, { TEXT("UseBelt"), TEXT("Gamepad_FaceButton_Top") },
		{ TEXT("QuickFire"), TEXT("Gamepad_DPad_Up") }, { TEXT("QuickPrev"), TEXT("Gamepad_DPad_Left") },
		{ TEXT("QuickNext"), TEXT("Gamepad_DPad_Right") }, { TEXT("BeltNext"), TEXT("Gamepad_DPad_Down") },
		{ TEXT("LockOn"), TEXT("Gamepad_RightThumbstick") }, { TEXT("View"), TEXT("Gamepad_LeftThumbstick") },
		{ TEXT("Journal"), TEXT("Gamepad_Special_Right") }, { TEXT("Report"), TEXT("Gamepad_Special_Left") },
		{ TEXT("Move"), TEXT("Gamepad_Left2D") }, { TEXT("Look"), TEXT("Gamepad_Right2D") },
		{ TEXT("BuildSnap"), TEXT("Gamepad_DPad_Down") }, { TEXT("BuildDemolish"), TEXT("Gamepad_FaceButton_Top") },
		{ TEXT("BuildCancel"), TEXT("Gamepad_FaceButton_Right") },
		{ TEXT("MenuClick"), TEXT("Gamepad_FaceButton_Bottom") }, { TEXT("MenuSecond"), TEXT("Gamepad_FaceButton_Left") },
		{ TEXT("MenuBack"), TEXT("Gamepad_FaceButton_Right") }, { TEXT("MenuControls"), TEXT("Gamepad_FaceButton_Top") },
		{ TEXT("MenuClose"), TEXT("Gamepad_Special_Right") }, { TEXT("TabPrev"), TEXT("Gamepad_LeftShoulder") },
		{ TEXT("TabNext"), TEXT("Gamepad_RightShoulder") },
	};
}

const FWRPadLayout& FWRPadLayout::Get()
{
	static FWRPadLayout Instance;
	return Instance;
}

FWRPadLayout::FWRPadLayout()
{
	for (const TPair<const TCHAR*, const TCHAR*>& V : WRPadDetail::DefaultVerbs) Verbs.Add(FName(V.Key), FKey(FName(V.Value)));
	const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("gamepad.json"));
	if (!Root.IsValid())
	{
		UE_LOG(LogWildreach, Warning, TEXT("PAD Data/gamepad.json missing or unreadable: the built-in layout is used"));
		return;
	}
	const TSharedPtr<FJsonObject>* Obj = nullptr;
	if (Root->TryGetObjectField(TEXT("verbs"), Obj))
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& V : (*Obj)->Values)
		{
			const FKey K(FName(*V.Value->AsString()));
			if (K.IsValid()) Verbs.Add(FName(*V.Key), K);
			else UE_LOG(LogWildreach, Warning, TEXT("PAD gamepad.json: %s has an unknown button %s"), *V.Key, *V.Value->AsString());
		}
	}
	if (Root->TryGetObjectField(TEXT("prompts"), Obj))
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& V : (*Obj)->Values) Prompts.Add(FName(*V.Key), FName(*V.Value->AsString()));
	}
	if (Root->TryGetObjectField(TEXT("names"), Obj))
	{
		const TCHAR* Sets[2] = { TEXT("xbox"), TEXT("playstation") };
		for (int32 s = 0; s < 2; ++s)
		{
			const TSharedPtr<FJsonObject>* Set = nullptr;
			if (!(*Obj)->TryGetObjectField(Sets[s], Set)) continue;
			for (const TPair<FString, TSharedPtr<FJsonValue>>& V : (*Set)->Values) Names[s].Add(V.Key, V.Value->AsString());
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* Page = nullptr;
	if (Root->TryGetArrayField(TEXT("page"), Page))
	{
		for (const TSharedPtr<FJsonValue>& G : *Page)
		{
			const TSharedPtr<FJsonObject> GO = G->AsObject();
			if (!GO.IsValid()) continue;
			FGroup& Group = Groups.AddDefaulted_GetRef();
			Group.Title = FText::FromString(GO->GetStringField(TEXT("title")));
			const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
			if (!GO->TryGetArrayField(TEXT("rows"), Rows)) continue;
			for (const TSharedPtr<FJsonValue>& R : *Rows)
			{
				const TSharedPtr<FJsonObject> RO = R->AsObject();
				if (!RO.IsValid()) continue;
				FRow Row;
				FString Verb;
				if (RO->TryGetStringField(TEXT("verb"), Verb)) Row.Verb = FName(*Verb);
				RO->TryGetStringField(TEXT("button"), Row.Button);
				Row.Does = FText::FromString(RO->GetStringField(TEXT("does")));
				Group.Rows.Add(Row);
			}
		}
	}
}

FKey FWRPadLayout::Key(FName Verb) const
{
	const FKey* K = Verbs.Find(Verb);
	return K ? *K : EKeys::Invalid;
}

FName FWRPadLayout::PromptVerb(FName Action) const
{
	const FName* V = Prompts.Find(Action);
	return V ? *V : NAME_None;
}

FText FWRPadLayout::Label(const FKey& InKey) const
{
	if (!InKey.IsValid()) return FText::GetEmpty();
	const int32 Set = FWRKeyBindings::Get().PadLabels();
	if (const FString* N = Names[Set].Find(InKey.GetFName().ToString())) return FText::FromString(*N);
	if (const FString* N = Names[0].Find(InKey.GetFName().ToString())) return FText::FromString(*N);
	return InKey.GetDisplayName(false);
}

// ---------------------------------------------------------------- device, prompts, look

namespace WRPadDetail
{
	EWRDevice GDevice = EWRDevice::KeyboardMouse;
	double GBlockUntil = 0.0;
	bool GKeyCapture = false;
	int32 GQuickWheel = 0;
	struct FRoot { TWeakPtr<SWidget> Widget; int32 Priority = 0; };
	TArray<FRoot> GRoots;
	struct FHandler { const void* Owner = nullptr; TFunction<bool(FName)> Fn; };
	TArray<FHandler> GHandlers;

	APlayerController* LocalPC()
	{
		UWorld* World = GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWorld() : nullptr;
		return World ? World->GetFirstPlayerController() : nullptr;
	}
}

EWRDevice WRInput::Device() { return WRPadDetail::GDevice; }
bool WRInput::UsingPad() { return WRPadDetail::GDevice == EWRDevice::Gamepad; }

void WRInput::NoteDevice(EWRDevice NewDevice)
{
	if (WRPadDetail::GDevice == NewDevice) return;
	WRPadDetail::GDevice = NewDevice;
	UE_LOG(LogWildreach, Log, TEXT("PAD device: %s"), NewDevice == EWRDevice::Gamepad ? TEXT("controller") : TEXT("keyboard and mouse"));
	WRReport::Event(TEXT("device"), NewDevice == EWRDevice::Gamepad ? TEXT("controller") : TEXT("keyboard and mouse"));
}

FText WRInput::PadButton(FName Verb)
{
	const FWRPadLayout& L = FWRPadLayout::Get();
	return L.Label(L.Key(Verb));
}

FText WRInput::ActionKey(FName Action)
{
	if (!UsingPad()) return WRUI::KeyName(FWRKeyBindings::Get().Key(Action));
	// The pad casts from one quick slot at a time (D-pad up), on whichever wheel the D-pad left / right last picked.
	if (Action == TEXT("Wheel1") || Action == TEXT("Wheel2"))
	{
		return (Action == TEXT("Wheel1")) == (WRPadDetail::GQuickWheel == 0) ? PadButton(TEXT("QuickFire")) : FText::GetEmpty();
	}
	const FName Verb = FWRPadLayout::Get().PromptVerb(Action);
	return Verb.IsNone() ? FText::GetEmpty() : PadButton(Verb);
}

void WRInput::MouseLook(APawn* Pawn, FVector2D Delta)
{
	if (!Pawn) return;
	const FWRKeyBindings& K = FWRKeyBindings::Get();
	const float S = K.MouseSensitivity() * WRTune(TEXT("look.mouseScale"), 1.f);
	Pawn->AddControllerYawInput(Delta.X * S);
	Pawn->AddControllerPitchInput((K.InvertMouse() ? Delta.Y : -Delta.Y) * S);
}

void WRInput::PadLook(APawn* Pawn, FVector2D Stick, float Dt)
{
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC) return;
	const float Mag = Stick.Size();
	const float Dead = WRTune(TEXT("look.padDeadZone"), 0.15f);
	if (Mag <= Dead) return;
	const float N = FMath::Clamp((Mag - Dead) / FMath::Max(0.01f, 1.f - Dead), 0.f, 1.f);
	const FVector2D Dir = Stick / Mag * FMath::Pow(N, WRTune(TEXT("look.padCurve"), 1.8f));
	const FWRKeyBindings& K = FWRKeyBindings::Get();
	const float S = K.PadSensitivity() * Dt;
	const float Yaw = Dir.X * WRTune(TEXT("look.padYawDegrees"), 230.f) * S;
	float Pitch = Dir.Y * WRTune(TEXT("look.padPitchDegrees"), 150.f) * S;
	if (K.InvertPad()) Pitch = -Pitch;
	// AddYaw/PitchInput multiply by the controller's legacy input scales (DefaultInput.ini): divide them out so these are degrees.
	const bool bLegacy = GetDefault<UInputSettings>()->bEnableLegacyInputScales;
	const float YS = bLegacy && !FMath::IsNearlyZero(PC->InputYawScale_DEPRECATED) ? PC->InputYawScale_DEPRECATED : 1.f;
	const float PS = bLegacy && !FMath::IsNearlyZero(PC->InputPitchScale_DEPRECATED) ? PC->InputPitchScale_DEPRECATED : 1.f;
	Pawn->AddControllerYawInput(Yaw / YS);
	// Stick up looks up: a positive pitch on the rotation.
	Pawn->AddControllerPitchInput(Pitch / PS);
}

void WRInput::Rumble(UWorld* World, float Strength, float Seconds)
{
	if (!World || !FWRKeyBindings::Get().PadRumble() || !UsingPad() || Strength <= 0.f || Seconds <= 0.f) return;
	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC || !PC->IsLocalController()) return;
	const float S = FMath::Clamp(Strength, 0.f, 1.f);
	PC->PlayDynamicForceFeedback(S, Seconds, true, S > 0.5f, true, true);
}

void WRInput::RumbleLight(UWorld* World, float Strength, float Seconds)
{
	if (!World || !FWRKeyBindings::Get().PadRumble() || !UsingPad() || Strength <= 0.f || Seconds <= 0.f) return;
	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC || !PC->IsLocalController()) return;
	PC->PlayDynamicForceFeedback(FMath::Clamp(Strength, 0.f, 1.f), Seconds, false, true, false, true);
}

void WRInput::Transition(APlayerController* PC, const TCHAR* Why)
{
	// Leaving a mode: a moment with no gameplay presses, so the press that closed it never also acts.
	WRPadDetail::GBlockUntil = FPlatformTime::Seconds() + WRTune(TEXT("look.isolationSeconds"), 0.14f);
	if (AWRCharacter* C = PC ? Cast<AWRCharacter>(PC->GetPawn()) : nullptr) C->DropHeldInputs();
	WRReport::Event(TEXT("mode"), Why);
}

bool WRInput::AllowPress(const APlayerController* PC)
{
	if (FPlatformTime::Seconds() < WRPadDetail::GBlockUntil) return false;
	if (WRReport::IsOpen() || WRControlsPage::IsOpen() || SWRMainMenu::IsOpen() || SWRChatBox::IsOpen()) return false;
	const AWRPlayerController* W = Cast<AWRPlayerController>(PC);
	const FWRPlayerUI* UI = W ? W->GetUI() : nullptr;
	return !(UI && UI->WantsCursor() && !UI->IsWheelOpen());
}

void WRInput::AddNavRoot(const TSharedRef<SWidget>& Root, int32 Priority)
{
	WRPadDetail::GRoots.RemoveAll([](const WRPadDetail::FRoot& R) { return !R.Widget.IsValid(); });
	WRPadDetail::GRoots.Add({ Root, Priority });
}

void WRInput::RemoveNavRoot(const TSharedPtr<SWidget>& Root)
{
	WRPadDetail::GRoots.RemoveAll([&Root](const WRPadDetail::FRoot& R) { return !R.Widget.IsValid() || R.Widget.Pin() == Root; });
}

void WRInput::PushPadHandler(const void* Owner, TFunction<bool(FName)> Handler)
{
	WRPadDetail::GHandlers.Add({ Owner, MoveTemp(Handler) });
}

void WRInput::PopPadHandler(const void* Owner)
{
	WRPadDetail::GHandlers.RemoveAll([Owner](const WRPadDetail::FHandler& H) { return H.Owner == Owner; });
}

void WRInput::SetKeyCapture(bool bCapturing) { WRPadDetail::GKeyCapture = bCapturing; }
int32 WRInput::PadQuickWheel() { return WRPadDetail::GQuickWheel; }
void WRInput::SetPadQuickWheel(int32 Wheel) { WRPadDetail::GQuickWheel = FMath::Clamp(Wheel, 0, 1); }

// ---------------------------------------------------------------- overlays (the report and controls pages)

namespace WRPadDetail
{
	int32 GOverlays = 0;
	TWeakPtr<SWidget> GFocusBefore;
	bool GCursorBefore = false;
}

void WRInput::BeginOverlay(const TSharedPtr<SWidget>& Focus)
{
	APlayerController* PC = WRPadDetail::LocalPC();
	if (WRPadDetail::GOverlays++ == 0)
	{
		WRPadDetail::GFocusBefore = FSlateApplication::IsInitialized() ? FSlateApplication::Get().GetKeyboardFocusedWidget() : nullptr;
		WRPadDetail::GCursorBefore = PC && PC->bShowMouseCursor;
		if (AWRCharacter* C = PC ? Cast<AWRCharacter>(PC->GetPawn()) : nullptr) C->DropHeldInputs();
		if (PC) PC->FlushPressedKeys();
	}
	if (PC)
	{
		FInputModeUIOnly Mode;
		if (Focus.IsValid()) Mode.SetWidgetToFocus(Focus);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(Mode);
		PC->bShowMouseCursor = true;
	}
	if (Focus.IsValid() && FSlateApplication::IsInitialized()) FSlateApplication::Get().SetKeyboardFocus(Focus, EFocusCause::SetDirectly);
}

void WRInput::EndOverlay()
{
	if (WRPadDetail::GOverlays <= 0 || --WRPadDetail::GOverlays > 0) return;
	APlayerController* PC = WRPadDetail::LocalPC();
	if (PC)
	{
		if (SWRMainMenu::IsOpen() || SWRChatBox::IsOpen())
		{
			// The menu (or chat line) under the page takes the keys and the cursor back.
			FInputModeUIOnly Mode;
			Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			PC->SetInputMode(Mode);
			PC->bShowMouseCursor = true;
		}
		else
		{
			PC->bShowMouseCursor = WRPadDetail::GCursorBefore;
			PC->SetInputMode(FInputModeGameOnly());
			if (const AWRPlayerController* W = Cast<AWRPlayerController>(PC)) { if (FWRPlayerUI* UI = W->GetUI()) UI->ReapplyInputMode(); }
		}
	}
	if (TSharedPtr<SWidget> Before = WRPadDetail::GFocusBefore.Pin(); Before.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetKeyboardFocus(Before, EFocusCause::SetDirectly);
	}
	WRPadDetail::GFocusBefore.Reset();
	Transition(PC, TEXT("page closed"));
}

// ---------------------------------------------------------------- the input processor

namespace WRPadDetail
{
	/** The widget kinds the D-pad lands on (besides anything with a hand cursor). Namespaces are stripped first. */
	bool IsTargetType(const FString& Type)
	{
		static const TSet<FString> Types = {
			TEXT("SButton"), TEXT("SCheckBox"), TEXT("SSlider"), TEXT("SEditableTextBox"), TEXT("SEditableText"),
			TEXT("SMultiLineEditableTextBox"), TEXT("SComboButton"), TEXT("SWRSlot"), TEXT("SWRButton"),
			TEXT("SWRClickBase"), TEXT("SWRSegment"), TEXT("SWRArrowButton"), TEXT("SWRSwitch"), TEXT("SWRKeyButton"),
			TEXT("SWRCastOption"), TEXT("SWRNavItem"), TEXT("SWRRuleStep"), TEXT("SWRCloseButton"), TEXT("SWRVolumeBar"),
			TEXT("SWRRuleBar"), TEXT("SWRWorldClick"), TEXT("SWRWorldRow"), TEXT("SWRPresetTile"), TEXT("SWRCloseBox"),
			TEXT("SWRPathNode"), TEXT("SWRWheelSlot"), TEXT("SWRSkillCard"), TEXT("SWRInvAmountSlider"), TEXT("SWRMenuButton"),
			TEXT("SWRChoice"), TEXT("SWRReportKind"), TEXT("SWRPadTarget"),
		};
		int32 Colon = INDEX_NONE;
		return Types.Contains(Type.FindLastChar(TEXT(':'), Colon) ? Type.Mid(Colon + 1) : Type);
	}

	bool IsTarget(const SWidget& W)
	{
		if (!W.IsEnabled() || !W.GetVisibility().IsHitTestVisible()) return false;
		if (IsTargetType(W.GetType().ToString())) return true;
		// Anything that shows the hand cursor (Raven's clickable pieces all do).
		const FCursorReply Reply = W.OnCursorQuery(W.GetCachedGeometry(), FPointerEvent());
		return Reply.IsEventHandled() && Reply.GetCursorType() == EMouseCursor::Hand;
	}

	void Collect(const TSharedRef<SWidget>& W, TArray<TSharedRef<SWidget>>& Out, int32& Budget)
	{
		if (--Budget < 0 || !W->GetVisibility().IsVisible()) return;
		if (IsTarget(*W))
		{
			Out.Add(W);
			return;
		}
		FChildren* Children = W->GetChildren();
		if (!Children) return;
		for (int32 i = 0; i < Children->Num(); ++i) Collect(Children->GetChildAt(i), Out, Budget);
	}

	FSlateRect ViewportRect()
	{
		if (GEngine && GEngine->GameViewport)
		{
			if (TSharedPtr<SViewport> V = GEngine->GameViewport->GetGameViewportWidget())
			{
				const FGeometry& G = V->GetCachedGeometry();
				const FVector2D Pos = G.GetAbsolutePosition(), Size = G.GetAbsoluteSize();
				if (Size.X > 1.f && Size.Y > 1.f) return FSlateRect(Pos.X, Pos.Y, Pos.X + Size.X, Pos.Y + Size.Y);
			}
		}
		return FSlateRect(0.f, 0.f, 1920.f, 1080.f);
	}

	TSharedPtr<SWidget> NavRoot()
	{
		const FRoot* Best = nullptr;
		for (const FRoot& R : GRoots)
		{
			const TSharedPtr<SWidget> W = R.Widget.Pin();
			if (W.IsValid() && W->GetVisibility().IsVisible() && (!Best || R.Priority >= Best->Priority)) Best = &R;
		}
		if (Best) return Best->Widget.Pin();
		return GEngine && GEngine->GameViewport ? StaticCastSharedPtr<SWidget>(GEngine->GameViewport->GetGameViewportWidget()) : nullptr;
	}

	bool RunHandler(FName Verb)
	{
		for (int32 i = GHandlers.Num() - 1; i >= 0; --i)
		{
			// A copy: the handler may close its page, which takes it off the list.
			const TFunction<bool(FName)> Fn = GHandlers[i].Fn;
			if (Fn && Fn(Verb)) return true;
		}
		return false;
	}
}

class FWRPadProcessor : public IInputProcessor
{
public:
	virtual void Tick(const float DeltaTime, FSlateApplication& App, TSharedRef<ICursor> Cursor) override
	{
		// Work asked for by key handlers runs here, outside Slate's event routing.
		TArray<TFunction<void()>> Run = MoveTemp(Queue);
		Queue.Reset();
		for (TFunction<void()>& Fn : Run) Fn();

		WRReport::Tick(DeltaTime);

		APlayerController* PC = WRPadDetail::LocalPC();
		if (!CursorMode(PC))
		{
			RepeatKey = EKeys::Invalid;
			ScrollAcc = 0.f;
			return;
		}
		TSharedPtr<FSlateUser> User = App.GetCursorUser();
		if (!User.IsValid()) return;
		const FSlateRect Rect = WRPadDetail::ViewportRect();
		const FVector2D Pos = User->GetCursorPosition();
		const AWRPlayerController* W = Cast<AWRPlayerController>(PC);
		const FWRPlayerUI* UI = W ? W->GetUI() : nullptr;

		if (UI && UI->IsWheelOpen() && !UI->WantsScreenCursor())
		{
			// A wheel: the stick points at a wedge (the cursor sits out along it, where the wheel reads it).
			if (Left.Size() > 0.5f)
			{
				const FVector2D Centre = Rect.GetCenter();
				const FVector2D Dir(Left.X, -Left.Y);
				MoveCursor(App, Centre + Dir.GetSafeNormal() * Rect.GetSize().Y * WRTune(TEXT("pad.wheelRadius"), 0.2f));
			}
		}
		else
		{
			const float Mag = Left.Size();
			const float Dead = WRTune(TEXT("pad.cursorDeadZone"), 0.18f);
			if (Mag > Dead)
			{
				const float N = FMath::Clamp((Mag - Dead) / (1.f - Dead), 0.f, 1.f);
				float Speed = WRTune(TEXT("pad.cursorSpeed"), 1300.f) * (Rect.GetSize().Y / 1080.f) * FMath::Pow(N, WRTune(TEXT("pad.cursorCurve"), 2.f));
				if (OverTarget(App, Pos)) Speed *= WRTune(TEXT("pad.cursorOverTarget"), 0.5f);
				const FVector2D Step = FVector2D(Left.X, -Left.Y) / Mag * Speed * DeltaTime;
				FVector2D NewPos = Pos + Step;
				NewPos.X = FMath::Clamp(NewPos.X, Rect.Left + 1.f, Rect.Right - 2.f);
				NewPos.Y = FMath::Clamp(NewPos.Y, Rect.Top + 1.f, Rect.Bottom - 2.f);
				MoveCursor(App, NewPos);
			}
		}

		// The right stick scrolls what's under the cursor (lists, the map's zoom).
		if (FMath::Abs(Right.Y) > WRTune(TEXT("pad.scrollDeadZone"), 0.25f))
		{
			ScrollAcc += Right.Y * WRTune(TEXT("pad.scrollNotches"), 12.f) * DeltaTime;
			while (FMath::Abs(ScrollAcc) >= 1.f)
			{
				const float Sign = FMath::Sign(ScrollAcc);
				Wheel(App, Sign);
				ScrollAcc -= Sign;
			}
		}
		else
		{
			ScrollAcc = 0.f;
		}

		if (RepeatKey.IsValid() && FPlatformTime::Seconds() >= RepeatAt)
		{
			RepeatAt = FPlatformTime::Seconds() + WRTune(TEXT("pad.repeatSeconds"), 0.11f);
			if (!WRPadDetail::RunHandler(DVerb(RepeatKey))) DPad(App, RepeatKey);
		}
	}

	virtual bool HandleKeyDownEvent(FSlateApplication& App, const FKeyEvent& E) override
	{
		if (bSynth) return false;
		const FKey K = E.GetKey();
		const FWRPadLayout& L = FWRPadLayout::Get();
		APlayerController* PC = WRPadDetail::LocalPC();
		AWRPlayerController* W = Cast<AWRPlayerController>(PC);
		if (!K.IsGamepadKey())
		{
			WRInput::NoteDevice(EWRDevice::KeyboardMouse);
			if (WRPadDetail::GKeyCapture || E.IsRepeat()) return false;
			const FWRKeyBindings& Keys = FWRKeyBindings::Get();
			if (K == Keys.Key(TEXT("Report")))
			{
				Queue.Add([]() { WRReport::Open(TEXT("F8")); });
				return true;
			}
			if (K == Keys.Key(TEXT("Help")) && !WRReport::IsOpen())
			{
				Queue.Add([]() { WRControlsPage::Toggle(); });
				return true;
			}
			return false;
		}

		WRInput::NoteDevice(EWRDevice::Gamepad);
		if (K == L.Key(TEXT("Report")))
		{
			Eaten.Add(K);
			if (!E.IsRepeat()) Queue.Add([]() { WRReport::Open(TEXT("pad")); });
			return true;
		}
		if (!CursorMode(PC))
		{
			// In play the pad's buttons are the game's (the character's mapping, the controller's bindings), except:
			if (W && W->IsPlacing() && !E.IsRepeat())
			{
				// Build placing: B cancels the piece (as Esc), Y switches to demolishing and back, D-pad down the snap.
				if (K == L.Key(TEXT("BuildCancel"))) { Eaten.Add(K); Queue.Add([this]() { SendKey(EKeys::Escape); }); return true; }
				if (K == L.Key(TEXT("BuildDemolish"))) { Eaten.Add(K); TWeakObjectPtr<AWRPlayerController> WW(W); Queue.Add([WW]() { if (WW.IsValid()) WW->ToggleDemolish(); }); return true; }
				if (K == L.Key(TEXT("BuildSnap"))) { Eaten.Add(K); TWeakObjectPtr<AWRPlayerController> WW(W); Queue.Add([WW]() { if (WW.IsValid()) WW->CycleSnap(); }); return true; }
			}
			if (K == L.Key(TEXT("Journal")) && W && W->GetUI() && !E.IsRepeat())
			{
				Eaten.Add(K);
				TWeakObjectPtr<AWRPlayerController> WW(W);
				Queue.Add([WW]() { if (WW.IsValid() && WW->GetUI() && WRInput::AllowPress(WW.Get())) WW->GetUI()->PadOpenMenus(); });
				return true;
			}
			return false;
		}

		// A menu, wheel or page is up: every pad button is the menu's.
		Eaten.Add(K);
		if (E.IsRepeat()) return true;   // (the D-pad repeats on its own clock)
		const FName Verb = K == L.Key(TEXT("MenuClick")) ? FName(TEXT("MenuClick"))
			: K == L.Key(TEXT("MenuSecond")) ? FName(TEXT("MenuSecond"))
			: K == L.Key(TEXT("MenuBack")) ? FName(TEXT("MenuBack"))
			: K == L.Key(TEXT("MenuClose")) ? FName(TEXT("MenuClose"))
			: K == L.Key(TEXT("MenuControls")) ? FName(TEXT("MenuControls"))
			: K == L.Key(TEXT("TabPrev")) ? FName(TEXT("TabPrev"))
			: K == L.Key(TEXT("TabNext")) ? FName(TEXT("TabNext"))
			: K == EKeys::Gamepad_DPad_Left ? FName(TEXT("DLeft")) : K == EKeys::Gamepad_DPad_Right ? FName(TEXT("DRight"))
			: K == EKeys::Gamepad_DPad_Up ? FName(TEXT("DUp")) : K == EKeys::Gamepad_DPad_Down ? FName(TEXT("DDown")) : NAME_None;
		if (Verb.IsNone()) return true;
		// An open page (the report, the controls) may want the button first.
		if (Verb != TEXT("MenuClick") && Verb != TEXT("MenuSecond") && WRPadDetail::RunHandler(Verb)) return true;
		if (Verb == TEXT("MenuClick")) Click(App, EKeys::LeftMouseButton, true);
		else if (Verb == TEXT("MenuSecond")) Click(App, EKeys::RightMouseButton, true);
		else if (Verb == TEXT("MenuBack") || Verb == TEXT("MenuClose"))
		{
			// What Esc does there: close the menu, cancel the key being listened for, step back in the main menu.
			if (!(Verb == TEXT("MenuClose") && SWRMainMenu::IsOpen())) Queue.Add([this]() { SendKey(EKeys::Escape); });
		}
		else if (Verb == TEXT("MenuControls")) Queue.Add([]() { WRControlsPage::Toggle(); });
		else if (Verb == TEXT("TabPrev") || Verb == TEXT("TabNext"))
		{
			const int32 Dir = Verb == TEXT("TabNext") ? 1 : -1;
			TWeakObjectPtr<AWRPlayerController> WW(W);
			if (W && W->GetUI() && !SWRMainMenu::IsOpen()) Queue.Add([WW, Dir]() { if (WW.IsValid() && WW->GetUI()) WW->GetUI()->PadTab(Dir); });
		}
		else
		{
			DPad(App, K);
			RepeatKey = K;
			RepeatAt = FPlatformTime::Seconds() + WRTune(TEXT("pad.repeatDelay"), 0.35f);
		}
		return true;
	}

	virtual bool HandleKeyUpEvent(FSlateApplication& App, const FKeyEvent& E) override
	{
		if (bSynth) return false;
		const FKey K = E.GetKey();
		// A button pressed in play is let go in play, and one pressed in a menu is let go there.
		if (!Eaten.Remove(K)) return false;
		const FWRPadLayout& L = FWRPadLayout::Get();
		if (K == L.Key(TEXT("MenuClick")) && App.GetPressedMouseButtons().Contains(EKeys::LeftMouseButton)) Click(App, EKeys::LeftMouseButton, false);
		else if (K == L.Key(TEXT("MenuSecond")) && App.GetPressedMouseButtons().Contains(EKeys::RightMouseButton)) Click(App, EKeys::RightMouseButton, false);
		if (K == RepeatKey) RepeatKey = EKeys::Invalid;
		return true;
	}

	virtual bool HandleAnalogInputEvent(FSlateApplication& App, const FAnalogInputEvent& E) override
	{
		const FKey K = E.GetKey();
		const float V = E.GetAnalogValue();
		if (K == EKeys::Gamepad_LeftX) Left.X = V;
		else if (K == EKeys::Gamepad_LeftY) Left.Y = V;
		else if (K == EKeys::Gamepad_RightX) Right.X = V;
		else if (K == EKeys::Gamepad_RightY) Right.Y = V;
		if (FMath::Abs(V) > WRTune(TEXT("pad.deviceThreshold"), 0.35f)) WRInput::NoteDevice(EWRDevice::Gamepad);
		// In menus the sticks are the cursor's and the scroll's: the survivor stands still.
		return CursorMode(WRPadDetail::LocalPC()) && K.IsGamepadKey();
	}

	virtual bool HandleMouseMoveEvent(FSlateApplication& App, const FPointerEvent& E) override
	{
		if (!bSynth && E.GetCursorDelta().SizeSquared() > 9.f && FVector2D::DistSquared(E.GetScreenSpacePosition(), LastSynthPos) > 9.f)
		{
			WRInput::NoteDevice(EWRDevice::KeyboardMouse);
		}
		return false;
	}

	virtual bool HandleMouseButtonDownEvent(FSlateApplication& App, const FPointerEvent& E) override
	{
		if (!bSynth) WRInput::NoteDevice(EWRDevice::KeyboardMouse);
		return false;
	}

	virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication& App, const FPointerEvent& E, const FPointerEvent* Gesture) override
	{
		if (!bSynth) WRInput::NoteDevice(EWRDevice::KeyboardMouse);
		return false;
	}

private:
	FVector2D Left = FVector2D::ZeroVector, Right = FVector2D::ZeroVector;
	FVector2D LastSynthPos = FVector2D(-1000.f, -1000.f);
	bool bSynth = false;
	TSet<FKey> Eaten;
	FKey RepeatKey;
	double RepeatAt = 0.0;
	float ScrollAcc = 0.f;
	TArray<TFunction<void()>> Queue;

	static FName DVerb(const FKey& Key)
	{
		return Key == EKeys::Gamepad_DPad_Left ? FName(TEXT("DLeft")) : Key == EKeys::Gamepad_DPad_Right ? FName(TEXT("DRight"))
			: Key == EKeys::Gamepad_DPad_Up ? FName(TEXT("DUp")) : FName(TEXT("DDown"));
	}

	static bool CursorMode(const APlayerController* PC)
	{
		return WRReport::IsOpen() || WRControlsPage::IsOpen() || (PC && PC->bShowMouseCursor);
	}

	void MoveCursor(FSlateApplication& App, const FVector2D& NewPos)
	{
		TSharedPtr<FSlateUser> User = App.GetCursorUser();
		if (!User.IsValid()) return;
		const FVector2D Old = User->GetCursorPosition();
		User->SetCursorPosition(FMath::RoundToInt(NewPos.X), FMath::RoundToInt(NewPos.Y));
		LastSynthPos = User->GetCursorPosition();
		const FPointerEvent Ev(User->GetUserIndex(), FSlateApplication::CursorPointerIndex, LastSynthPos, Old, App.GetPressedMouseButtons(), EKeys::Invalid, 0.f, App.GetModifierKeys());
		TGuardValue<bool> Guard(bSynth, true);
		App.ProcessMouseMoveEvent(Ev);
	}

	void Click(FSlateApplication& App, const FKey& Button, bool bDown)
	{
		TSharedPtr<FSlateUser> User = App.GetCursorUser();
		if (!User.IsValid()) return;
		const FVector2D Pos = User->GetCursorPosition();
		TSet<FKey> Pressed = App.GetPressedMouseButtons();
		if (bDown) Pressed.Add(Button);
		const FPointerEvent Ev(User->GetUserIndex(), FSlateApplication::CursorPointerIndex, Pos, Pos, Pressed, Button, 0.f, App.GetModifierKeys());
		TGuardValue<bool> Guard(bSynth, true);
		if (bDown) App.ProcessMouseButtonDownEvent(nullptr, Ev);
		else App.ProcessMouseButtonUpEvent(Ev);
	}

	void Wheel(FSlateApplication& App, float Notches)
	{
		TSharedPtr<FSlateUser> User = App.GetCursorUser();
		if (!User.IsValid()) return;
		const FVector2D Pos = User->GetCursorPosition();
		const FPointerEvent Ev(User->GetUserIndex(), FSlateApplication::CursorPointerIndex, Pos, Pos, App.GetPressedMouseButtons(), EKeys::Invalid, Notches, App.GetModifierKeys());
		TGuardValue<bool> Guard(bSynth, true);
		App.ProcessMouseWheelOrGestureEvent(Ev, nullptr);
	}

	void SendKey(const FKey& Key)
	{
		if (!FSlateApplication::IsInitialized()) return;
		FSlateApplication& App = FSlateApplication::Get();
		const FKeyEvent Ev(Key, App.GetModifierKeys(), App.GetUserIndexForKeyboard(), false, 0, 0);
		TGuardValue<bool> Guard(bSynth, true);
		App.ProcessKeyDownEvent(Ev);
		App.ProcessKeyUpEvent(Ev);
	}

	bool OverTarget(FSlateApplication& App, const FVector2D& Pos) const
	{
		const FWidgetPath Path = App.LocateWindowUnderMouse(Pos, App.GetInteractiveTopLevelWindows());
		for (int32 i = Path.Widgets.Num() - 1; i >= 0; --i)
		{
			if (WRPadDetail::IsTarget(Path.Widgets[i].Widget.Get())) return true;
		}
		return false;
	}

	/** The D-pad: the cursor jumps to the nearest clickable thing that way (on the topmost screen); with none, the list
	 *  under the cursor scrolls that way. */
	void DPad(FSlateApplication& App, const FKey& Key)
	{
		const FVector2D Dir = Key == EKeys::Gamepad_DPad_Left ? FVector2D(-1.f, 0.f) : Key == EKeys::Gamepad_DPad_Right ? FVector2D(1.f, 0.f)
			: Key == EKeys::Gamepad_DPad_Up ? FVector2D(0.f, -1.f) : FVector2D(0.f, 1.f);
		TSharedPtr<FSlateUser> User = App.GetCursorUser();
		const TSharedPtr<SWidget> Root = WRPadDetail::NavRoot();
		if (!User.IsValid() || !Root.IsValid()) return;
		const FVector2D Pos = User->GetCursorPosition();
		const FSlateRect Rect = WRPadDetail::ViewportRect();
		TArray<TSharedRef<SWidget>> Found;
		int32 Budget = 6000;
		WRPadDetail::Collect(Root.ToSharedRef(), Found, Budget);
		const TArray<TSharedRef<SWindow>> Windows = App.GetInteractiveTopLevelWindows();
		const float Cross = WRTune(TEXT("pad.navCrossWeight"), 2.2f);
		float Best = TNumericLimits<float>::Max();
		FVector2D BestAt = Pos;
		for (const TSharedRef<SWidget>& W : Found)
		{
			const FGeometry& G = W->GetCachedGeometry();
			const FVector2D Size = G.GetAbsoluteSize();
			if (Size.X < 4.f || Size.Y < 4.f) continue;
			const FVector2D C = G.GetAbsolutePositionAtCoordinates(FVector2D(0.5f, 0.5f));
			if (!Rect.ContainsPoint(C)) continue;
			const FVector2D D = C - Pos;
			const float Along = FVector2D::DotProduct(D, Dir);
			if (Along <= FMath::Min(Size.X, Size.Y) * 0.25f + 4.f) continue;
			const float Off = FMath::Abs(FVector2D::CrossProduct(D, Dir));
			const float Score = Along + Off * Cross;
			if (Score >= Best) continue;
			// Only what's really on screen and on top there (not behind another window, scrolled away or faded out).
			const FWidgetPath Path = App.LocateWindowUnderMouse(C, Windows);
			if (!Path.ContainsWidget(&W.Get())) continue;
			Best = Score;
			BestAt = C;
		}
		if (Best < TNumericLimits<float>::Max())
		{
			MoveCursor(App, BestAt);
			return;
		}
		if (Dir.Y != 0.f) Wheel(App, Dir.Y < 0.f ? 2.f : -2.f);
	}
};

namespace WRPadDetail
{
	TSharedPtr<FWRPadProcessor> GProcessor;
}

void WRInput::Install()
{
	if (WRPadDetail::GProcessor.IsValid() || !FSlateApplication::IsInitialized()) return;
	WRPadDetail::GProcessor = MakeShared<FWRPadProcessor>();
	FSlateApplication::Get().RegisterInputPreProcessor(WRPadDetail::GProcessor, 0);
	WRReport::InstallLog();
	UE_LOG(LogWildreach, Log, TEXT("PAD controller support on (Data/gamepad.json)"));
}

#undef LOCTEXT_NAMESPACE
