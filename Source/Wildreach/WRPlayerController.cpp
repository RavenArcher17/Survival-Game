#include "WRPlayerController.h"
#include "Wildreach.h"
#include "WRAltar.h"
#include "WRCrafting.h"
#include "WRBuilding.h"
#include "WRAttributeSet.h"
#include "WRCharacter.h"
#include "WRCombatTypes.h"
#include "WRCreature.h"
#include "WRBoss.h"
#include "WRCombatLibrary.h"
#include "WRLifeSkills.h"
#include "WRWorldDressing.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "WRGameplayTags.h"
#include "WRGameState.h"
#include "WRInventory.h"
#include "WRPathComponent.h"
#include "WRPlayerState.h"
#include "WRResourceNode.h"
#include "WRSky.h"
#include "WRStatusComponent.h"
#include "WRTuning.h"
#include "AbilitySystemComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraActor.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "WRMainMenu.h"
#include "WRKeyBindings.h"
#include "WRPlayerUI.h"
#include "Kismet/GameplayStatics.h"
#include "WorldPartition/WorldPartitionStreamingSource.h"

AWRPlayerController::AWRPlayerController()
{
	bBot = FParse::Param(FCommandLine::Get(), TEXT("wrbot")) || FParse::Param(FCommandLine::Get(), TEXT("wrbotpvp"));
	bBotPvP = FParse::Param(FCommandLine::Get(), TEXT("wrbotpvp"));
	bBotHunt = FParse::Param(FCommandLine::Get(), TEXT("wrbothunt"));
	bBotGather = FParse::Param(FCommandLine::Get(), TEXT("wrbotgather"));
	bBot = bBot || bBotGather;
}

void AWRPlayerController::BeginPlay()
{
	Super::BeginPlay();
	// The island's ground streams in around each player. At the map's 256 m loading range the terrain past that wasn't
	// drawn at all (empty distance from any hill); the landscape's own LODs keep the far ground cheap.
	const float RangeScale = WRTune(TEXT("world.streamingRangeScale"), 4.f);
	if (RangeScale > 1.f)
	{
		FStreamingSourceShape Shape;
		Shape.bUseGridLoadingRange = true;
		Shape.LoadingRangeScale = RangeScale;
		StreamingSourceShapes = { Shape };
	}
	// The stone-and-leather UI for the player at this screen (-oldui keeps the text menus for comparison).
	if (IsLocalController() && GetNetMode() != NM_DedicatedServer && !FParse::Param(FCommandLine::Get(), TEXT("oldui")) && GEngine && GEngine->GameViewport)
	{
		UI = MakeShared<FWRPlayerUI>(this);
	}
	// The game on its own (not connected to anyone) shows the front end over the island.
	if (IsLocalController() && GetNetMode() == NM_Standalone && !bBot && !GIsEditor && !FParse::Param(FCommandLine::Get(), TEXT("nomenu")))
	{
		SWRMainMenu::Open(this);
	}
}

void AWRPlayerController::OpenChat()
{
	SWRChatBox::Open(this);
}

void AWRPlayerController::ServerChat_Implementation(const FString& Message)
{
	const FString Clean = Message.Left(200).TrimStartAndEnd();
	const FString From = PlayerState ? PlayerState->GetPlayerName() : TEXT("?");
	if (Clean.IsEmpty())
	{
		return;
	}
	UE_LOG(LogWildreach, Log, TEXT("CHAT %s: %s"), *From, *Clean);
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (AWRPlayerController* PC = Cast<AWRPlayerController>(It->Get()))
		{
			PC->ClientChat(From, Clean);
		}
	}
}

void AWRPlayerController::ClientChat_Implementation(const FString& From, const FString& Message)
{
	Chat.Add({ FString::Printf(TEXT("%s: %s"), *From, *Message), GetWorld()->GetTimeSeconds() });
	if (Chat.Num() > 8) Chat.RemoveAt(0);
	UE_LOG(LogWildreach, Log, TEXT("NOTICE chat %s: %s"), *From, *Message);
}

void AWRPlayerController::ReturnToMenu()
{
	if (SWRMainMenu::IsOpen())
	{
		return;
	}
	if (GetNetMode() == NM_Standalone)
	{
		SWRMainMenu::Open(this);
		return;
	}
	UGameplayStatics::OpenLevel(this, TEXT("/Game/Maps/L_Island"), true);
}

void AWRPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	BindKeys();
	if (!KeysChangedHandle.IsValid())
	{
		KeysChangedHandle = FWRKeyBindings::Get().OnChanged.AddWeakLambda(this, [this]() { BindKeys(); });
	}
}

void AWRPlayerController::BindKeys()
{
	if (!InputComponent)
	{
		return;
	}
	InputComponent->KeyBindings.Reset();
	const FWRKeyBindings& Keys = FWRKeyBindings::Get();
	const auto Bind = [this, &Keys](const TCHAR* Action, EInputEvent Event, auto Func)
	{
		const FKey Key = Keys.Key(FName(Action));
		if (Key.IsValid()) InputComponent->BindKey(Key, Event, this, Func);
	};
	Bind(TEXT("LockOn"), IE_Pressed, &AWRPlayerController::ToggleLockOn);
	Bind(TEXT("Interact"), IE_Pressed, &AWRPlayerController::InteractPressed);
	Bind(TEXT("Interact"), IE_Released, &AWRPlayerController::InteractReleased);
	Bind(TEXT("Inventory"), IE_Pressed, &AWRPlayerController::ToggleInventory);
	Bind(TEXT("Skills"), IE_Pressed, &AWRPlayerController::ToggleSkills);
	Bind(TEXT("Paths"), IE_Pressed, &AWRPlayerController::TogglePaths);
	Bind(TEXT("BackToMenu"), IE_Pressed, &AWRPlayerController::ReturnToMenu);
	Bind(TEXT("Map"), IE_Pressed, &AWRPlayerController::ToggleMap);
	Bind(TEXT("Chat"), IE_Pressed, &AWRPlayerController::OpenChat);
	Bind(TEXT("Build"), IE_Pressed, &AWRPlayerController::ToggleBuildMenu);
	Bind(TEXT("Snap"), IE_Pressed, &AWRPlayerController::ToggleSnap);
	// The old text menus' keys (bots and -oldui still use them); Esc always closes what's open.
	InputComponent->BindKey(EKeys::Up, IE_Pressed, this, &AWRPlayerController::MenuUp);
	InputComponent->BindKey(EKeys::Down, IE_Pressed, this, &AWRPlayerController::MenuDown);
	InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &AWRPlayerController::MenuConfirmKey);
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AWRPlayerController::EscapePressed);
	// Keys that pass a value: lambdas through FInputKeyBinding.
	const auto BindLambda = [this](const FKey& Key, EInputEvent Event, TFunction<void()> Fn)
	{
		if (!Key.IsValid()) return;
		FInputKeyBinding KB(FInputChord(Key), Event);
		KB.KeyDelegate.GetDelegateForManualSet().BindWeakLambda(this, MoveTemp(Fn));
		InputComponent->KeyBindings.Add(KB);
	};
	BindLambda(Keys.Key(TEXT("Crafting")), IE_Pressed, [this]() { OpenJournal(EWRMenu::Craft); });
	BindLambda(Keys.Key(TEXT("Help")), IE_Pressed, [this]() { if (UI) UI->ToggleHelp(); });
	for (int32 i = 0; i < 8; ++i)
	{
		BindLambda(Keys.Key(FName(*FString::Printf(TEXT("Hotbar%d"), i + 1))), IE_Pressed, [this, i]()
		{
			if (UI) UI->UseHotbar(i);
			else if (AWRCharacter* C = Cast<AWRCharacter>(GetPawn())) { C->PressAbilityInput(static_cast<EWRAbilityInput>(static_cast<int32>(EWRAbilityInput::Slot1) + i)); C->ReleaseAbilityInput(static_cast<EWRAbilityInput>(static_cast<int32>(EWRAbilityInput::Slot1) + i)); }
		});
	}
	for (int32 W = 0; W < 2; ++W)
	{
		const FKey WheelKey = Keys.Key(W == 0 ? TEXT("Wheel1") : TEXT("Wheel2"));
		BindLambda(WheelKey, IE_Pressed, [this, W]() { if (UI) UI->WheelKey(W, true); });
		BindLambda(WheelKey, IE_Released, [this, W]() { if (UI) UI->WheelKey(W, false); });
	}
	// Abilities with their own keys (Settings > Controls, "Own keys" or "Both").
	if (Keys.GetCastMode() != EWRCastMode::Wheel)
	{
		for (const FWRActionKey& A : Keys.Actions())
		{
			if (A.Group != TEXT("Abilities") || !A.Key.IsValid()) continue;
			const FName Node(*A.Id.ToString().RightChop(8));   // "Ability." + path.node
			BindLambda(A.Key, IE_Pressed, [this, Node]() { if (UI) UI->CastAbility(Node); });
		}
	}
}

void AWRPlayerController::InteractPressed()
{
	if (UI && Menu == EWRMenu::None)
	{
		// Your own tame in front of you: hold for the order wheel, tap for the usual (see FWRPlayerUI::TameKey).
		AWRCreature* Tame = FindNearbyCreature(true);
		AWRCreature* Wild = FindNearbyCreature(false, 400.f);
		const APawn* Me = GetPawn();
		if (Tame && Me && (!Wild || FVector::Dist2D(Tame->GetActorLocation(), Me->GetActorLocation()) < FVector::Dist2D(Wild->GetActorLocation(), Me->GetActorLocation())))
		{
			UI->TameKey(true);
			return;
		}
	}
	WRInteract();
}

void AWRPlayerController::InteractReleased()
{
	if (UI) UI->TameKey(false);
}

void AWRPlayerController::OpenJournal(EWRMenu Tab)
{
	if (!UI)
	{
		if (Tab == EWRMenu::Craft) ToggleBuildMenu();   // the old Build & craft list
		return;
	}
	if (Menu == Tab)
	{
		MenuClose();
		return;
	}
	// Crafting opens at the station you stand by (or by hand).
	if (Tab == EWRMenu::Craft && Menu != EWRMenu::Craft) MenuStation = FindNearbyStation();
	Menu = Tab;
	MenuIndex = 0;
}

void AWRPlayerController::SetMenu(EWRMenu NewMenu)
{
	if (NewMenu == EWRMenu::None)
	{
		MenuClose();
		return;
	}
	OpenJournal(NewMenu);
}

void AWRPlayerController::ShowLocalNotice(const FString& Message)
{
	Notice = Message;
	NoticeTime = GetWorld()->GetTimeSeconds();
	if (UI) UI->NoteNotice(Message);
}

void AWRPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
	if (KeysChangedHandle.IsValid())
	{
		FWRKeyBindings::Get().OnChanged.Remove(KeysChangedHandle);
		KeysChangedHandle.Reset();
	}
	UI.Reset();
	Super::EndPlay(Reason);
}

void AWRPlayerController::UsePotion() { ServerCommand(TEXT("use potion")); }
void AWRPlayerController::UseFood() { ServerCommand(TEXT("use food")); }
void AWRPlayerController::UseDrink() { ServerCommand(TEXT("use drink")); }
void AWRPlayerController::WRTeleport(float X, float Y) { ServerCommand(FString::Printf(TEXT("tp %.0f %.0f"), X, Y)); }
void AWRPlayerController::WRLook(float Yaw, float Pitch) { SetControlRotation(FRotator(Pitch, Yaw, 0.f)); }

void AWRPlayerController::WRWalkTo(float X, float Y)
{
	bWalking = true;
	WalkTarget = FVector2D(X, Y);
	WalkStart = WalkLastProgress = WalkNextLog = BotClock;
	WalkBestDist = TNumericLimits<float>::Max();
	UE_LOG(LogWildreach, Log, TEXT("WALK start toward (%.0f, %.0f)"), X, Y);
}

void AWRPlayerController::WRRanged(int32 bOn) { if (AWRCharacter* C = Cast<AWRCharacter>(GetPawn())) C->SetRangedMode(bOn != 0); }
void AWRPlayerController::WRAim(int32 bOn) { if (AWRCharacter* C = Cast<AWRCharacter>(GetPawn())) C->SetAiming(bOn != 0); }
void AWRPlayerController::WRFire() { if (AWRCharacter* C = Cast<AWRCharacter>(GetPawn())) C->FireRanged(); }
void AWRPlayerController::WRAmmo(int32 Dir) { if (AWRCharacter* C = Cast<AWRCharacter>(GetPawn())) C->CycleAmmo(Dir); }
void AWRPlayerController::WRHold(float Seconds) { ServerCommand(FString::Printf(TEXT("hold %.1f"), Seconds)); }
void AWRPlayerController::WRClearCreatures() { ServerCommand(TEXT("clearcreatures")); }
void AWRPlayerController::WRHour(float Hour) { ServerCommand(FString::Printf(TEXT("hour %.2f"), Hour)); }
void AWRPlayerController::WRWildlife() { ServerCommand(TEXT("wildlife")); }
void AWRPlayerController::WRSpawnAt(FName Creature, float X, float Y) { ServerCommand(FString::Printf(TEXT("spawnat %s %.0f %.0f"), *Creature.ToString(), X, Y)); }
void AWRPlayerController::WRCreatureHP(FName Creature, float Fraction) { ServerCommand(FString::Printf(TEXT("creaturehp %s %.3f"), *Creature.ToString(), Fraction)); }
void AWRPlayerController::WRSky()
{
	const UWRSkySubsystem* Sub = GetWorld()->GetSubsystem<UWRSkySubsystem>();
	const AWRSky* Sky = Sub ? Sub->GetSky() : nullptr;
	UE_LOG(LogWildreach, Log, TEXT("SKY %s"), Sky ? *Sky->Describe() : TEXT("none (no sky in this world)"));
}

void AWRPlayerController::WRAimAt(FName Creature)
{
	APawn* Me = GetPawn();
	if (!Me)
	{
		return;
	}
	AActor* Best = nullptr;
	float BestDist = TNumericLimits<float>::Max();
	for (TActorIterator<AWRCreature> It(GetWorld()); It; ++It)
	{
		const float Dist = FVector::Dist(It->GetActorLocation(), Me->GetActorLocation());
		if (It->IsAlive() && (Creature.IsNone() || It->CreatureId == Creature) && Dist < BestDist)
		{
			BestDist = Dist;
			Best = *It;
		}
	}
	if (Best)
	{
		FVector ViewLoc;
		FRotator ViewRot;
		GetPlayerViewPoint(ViewLoc, ViewRot);
		const AWRCharacter* Survivor = Cast<AWRCharacter>(Me);
		FVector At = Best->GetActorLocation();
		if (const FWRItemDef* Gun = Survivor && Survivor->IsRangedOut() ? FWRItemRegistry::Get().Find(Survivor->GetRangedItem()) : nullptr)
		{
			// Hold over for the shot's drop, as a player would.
			const float Flight = FVector::Dist(At, Me->GetActorLocation() + FVector(0.f, 0.f, 55.f)) / FMath::Max(100.f, Gun->ProjectileSpeed);
			At.Z += 0.5f * FMath::Abs(GetWorld()->GetGravityZ()) * Gun->ProjectileGravity * Flight * Flight;
		}
		// The survivor's crosshair camera, even while the art camera is the view.
		SetControlRotation(Survivor ? Survivor->AimRotationAt(At) : (At - ViewLoc).Rotation());
	}
}

void AWRPlayerController::WRGive(FName Item, int32 Count) { ServerCommand(FString::Printf(TEXT("give %s %d"), *Item.ToString(), FMath::Max(1, Count))); }

AWRBossAltar* AWRPlayerController::FindNearbyAltar() const
{
	const APawn* Self = GetPawn();
	if (!Self)
	{
		return nullptr;
	}
	AWRBossAltar* Best = nullptr;
	float BestDist = AWRBossAltar::InteractRange;
	for (TActorIterator<AWRBossAltar> It(GetWorld()); It; ++It)
	{
		const float Dist = FVector::Dist2D(It->GetActorLocation(), Self->GetActorLocation());
		if (Dist < BestDist)
		{
			BestDist = Dist;
			Best = *It;
		}
	}
	return Best;
}

void AWRPlayerController::ClientNotify_Implementation(const FString& Message)
{
	Notice = Message;
	NoticeTime = GetWorld()->GetTimeSeconds();
	if (UI)
	{
		UI->NoteNotice(Message);
	}
	UE_LOG(LogWildreach, Log, TEXT("NOTICE %s"), *Message);
}

void AWRPlayerController::WRBotPause(int32 bPause)
{
	bBotPaused = bPause != 0;
	if (bBotPaused)
	{
		LockTarget = nullptr;  // otherwise lock-on keeps steering the camera
	}
}

void AWRPlayerController::WRGather(const FString& What)
{
	bBotGather = What != TEXT("off");
	BotGatherGroup = bBotGather ? What.ToLower() : FString(TEXT("node"));
	LockTarget = nullptr;
}

void AWRPlayerController::WRArtCam(FName Creature, float Yaw)
{
	ArtCamTarget = nullptr;
	const APawn* Self = GetPawn();
	float BestDist = TNumericLimits<float>::Max();
	if (Creature == TEXT("me"))
	{
		ArtCamTarget = GetPawn();  // the player's own body (art checks)
		BestDist = 0.f;
	}
	for (TActorIterator<AWRCreature> It(GetWorld()); It && Self && BestDist > 0.f; ++It)
	{
		const float Dist = FVector::Dist(It->GetActorLocation(), Self->GetActorLocation());
		if (It->CreatureId == Creature && It->IsAlive() && Dist < BestDist)
		{
			BestDist = Dist;
			ArtCamTarget = *It;
		}
	}
	if (!ArtCamTarget.IsValid())
	{
		if (Self)
		{
			SetViewTarget(GetPawn());
		}
		return;
	}
	if (!ArtCam)
	{
		ArtCam = GetWorld()->SpawnActor<ACameraActor>();
	}
	ArtCamYaw = Yaw;
	SetViewTarget(ArtCam);
	UpdateArtCam(0.f);
}

void AWRPlayerController::UpdateArtCam(float DeltaTime)
{
	const AActor* Target = ArtCamTarget.Get();
	if (!Target || !ArtCam)
	{
		return;
	}
	// Far enough to fit the whole creature, a little above it, swinging with its facing so the angle stays put.
	FVector Origin, Extent;
	Target->GetActorBounds(true, Origin, Extent);
	const float Size = FMath::Max3(Extent.X, Extent.Y, Extent.Z);
	if (bArtCamFixed && DeltaTime > 0.f)
	{
		ArtCam->SetActorRotation((Origin - ArtCam->GetActorLocation()).Rotation());  // held in place, just watching
		return;
	}
	const FVector Dir = FRotator(0.f, Target->GetActorRotation().Yaw + ArtCamYaw, 0.f).Vector();
	const FVector Goal = Origin + Dir * (Size * 3.2f + 150.f) + FVector(0.f, 0.f, Size * 0.6f);
	const FVector At = DeltaTime > 0.f ? FMath::VInterpTo(ArtCam->GetActorLocation(), Goal, DeltaTime, 4.f) : Goal;
	ArtCam->SetActorLocationAndRotation(At, (Origin - At).Rotation());
}

void AWRPlayerController::WRView(FName Mode)
{
	if (AWRCharacter* Survivor = Cast<AWRCharacter>(GetPawn()))
	{
		Survivor->SetFirstPerson(Mode == TEXT("fp"));
	}
}
void AWRPlayerController::WRGoToAltar(FName Path) { ServerCommand(FString::Printf(TEXT("gotoaltar %s"), *Path.ToString())); }

void AWRPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (!IsLocalController())
	{
		return;
	}
	if (bBot)
	{
		TickBot(DeltaTime);
	}
	UpdateArtCam(DeltaTime);
	if (IsPlacing())
	{
		UpdatePlacement();
	}
	UpdateLockOn(DeltaTime);
	if (UI)
	{
		UI->Tick(DeltaTime);
	}
}

// ---------------------------------------------------------------- lock-on

AWRCharacterBase* AWRPlayerController::FindTarget(float MaxDistance, float MaxAngleDegrees) const
{
	const AWRCharacterBase* Self = Cast<AWRCharacterBase>(GetPawn());
	if (!Self)
	{
		return nullptr;
	}
	const FVector ViewDir = GetControlRotation().Vector().GetSafeNormal2D();
	const float MinCos = FMath::Cos(FMath::DegreesToRadians(MaxAngleDegrees));

	AWRCharacterBase* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		AWRCharacterBase* Other = *It;
		if (!Self->IsHostileTo(Other) || !Other->IsAlive())
		{
			continue;
		}
		if (bBot && !bBotPvP && !Other->IsCreature())
		{
			continue;
		}
		if (bBotPvP && Other->IsCreature())
		{
			continue;
		}
		if (bBotHunt && Other->IsA<AWRTrainingDummy>())
		{
			continue;
		}
		const FVector To = Other->GetActorLocation() - Self->GetActorLocation();
		const float Dist = To.Size();
		if (Dist > MaxDistance || FVector::DotProduct(To.GetSafeNormal2D(), ViewDir) < MinCos)
		{
			continue;
		}
		if (Dist < BestScore)
		{
			BestScore = Dist;
			Best = Other;
		}
	}
	return Best;
}

void AWRPlayerController::ToggleLockOn()
{
	LockTarget = LockTarget.IsValid() ? nullptr : FindTarget(2500.f, 40.f);
	LockAge = 0.f;
}

void AWRPlayerController::UpdateLockOn(float DeltaTime)
{
	AWRCharacterBase* Target = LockTarget.Get();
	APawn* Self = GetPawn();
	if (!Target || !Self)
	{
		return;
	}
	if (!Target->IsAlive() || FVector::Dist(Target->GetActorLocation(), Self->GetActorLocation()) > 3500.f)
	{
		LockTarget = nullptr;
		return;
	}
	const FVector Eye = PlayerCameraManager ? PlayerCameraManager->GetCameraLocation() : Self->GetActorLocation();
	const FRotator Desired = (Target->GetActorLocation() + FVector(0.f, 0.f, 30.f) - Eye).Rotation();
	// Soft lock: the turn toward a new target starts gently and firms up over camera.lockOnEaseSeconds (bots snap).
	LockAge += DeltaTime;
	const float Ease = FMath::SmoothStep(0.f, FMath::Max(0.01f, WRTune(TEXT("camera.lockOnEaseSeconds"), 0.5f)), LockAge);
	const float Speed = bBot ? 30.f : FMath::Lerp(WRTune(TEXT("camera.lockOnStartSpeed"), 2.5f), WRTune(TEXT("camera.lockOnSpeed"), 7.f), Ease);
	SetControlRotation(FMath::RInterpTo(GetControlRotation(), Desired, DeltaTime, Speed));
}

// ---------------------------------------------------------------- developer commands

void AWRPlayerController::WRSetMain(FName Path) { ServerCommand(FString::Printf(TEXT("setmain %s"), *Path.ToString())); }
void AWRPlayerController::WRSetSub(FName Path) { ServerCommand(FString::Printf(TEXT("setsub %s"), *Path.ToString())); }
void AWRPlayerController::WRGivePoints(FName Path, int32 Points) { ServerCommand(FString::Printf(TEXT("givepoints %s %d"), *Path.ToString(), Points)); }
void AWRPlayerController::WRAutoSpend(FName Path, int32 Points) { ServerCommand(FString::Printf(TEXT("autospend %s %d"), *Path.ToString(), Points)); }
void AWRPlayerController::WRBuy(FName Path, FName Node) { ServerCommand(FString::Printf(TEXT("buy %s %s"), *Path.ToString(), *Node.ToString())); }
void AWRPlayerController::WRWeapon(FName Metal, FName Quality, int32 Upgrade) { ServerCommand(FString::Printf(TEXT("weapon %s %s %d"), *Metal.ToString(), *Quality.ToString(), Upgrade)); }
void AWRPlayerController::WRSpawn(FName Creature, int32 Count) { ServerCommand(FString::Printf(TEXT("spawn %s %d"), *Creature.ToString(), FMath::Max(1, Count))); }
void AWRPlayerController::WRStatus(FName Status) { ServerCommand(FString::Printf(TEXT("status %s"), *Status.ToString())); }
void AWRPlayerController::WRHeal() { ServerCommand(TEXT("heal")); }
void AWRPlayerController::WRKillMe() { ServerCommand(TEXT("killme")); }
void AWRPlayerController::WRStarve() { ServerCommand(TEXT("starve")); }

void AWRPlayerController::ServerCommand_Implementation(const FString& Command)
{
#if UE_BUILD_SHIPPING
	// Only real gameplay actions go through in shipping builds; the rest are dev commands.
	static const TCHAR* GameplayCommands[] = { TEXT("interact"), TEXT("use "), TEXT("craft "), TEXT("repair "), TEXT("upgrade "), TEXT("slot "), TEXT("unequip "),
		TEXT("place "), TEXT("setcode "), TEXT("code "), TEXT("demolish"), TEXT("claim"), TEXT("store"), TEXT("take"), TEXT("tame "), TEXT("dismount"), TEXT("clan "), TEXT("perk "), TEXT("learn "),
		TEXT("move "), TEXT("split "), TEXT("drop "), TEXT("tamegive "), TEXT("tamenarcotic"), TEXT("tameall "), TEXT("hearthfeed") };  // chat is its own RPC
	bool bAllowed = false;
	for (const TCHAR* Prefix : GameplayCommands)
	{
		bAllowed |= Command.StartsWith(Prefix);
	}
	if (!bAllowed)
	{
		return;
	}
#endif
	RunServerCommand(Command);
}

void AWRPlayerController::RunServerCommand(const FString& Command)
{
	TArray<FString> Args;
	Command.ParseIntoArrayWS(Args);
	if (Args.Num() == 0)
	{
		return;
	}
	AWRPlayerState* PS = GetPlayerState<AWRPlayerState>();
	UWRPathComponent* Paths = PS ? PS->GetPaths() : nullptr;
	AWRCharacter* Survivor = Cast<AWRCharacter>(GetPawn());
	UAbilitySystemComponent* ASC = PS ? PS->GetAbilitySystemComponent() : nullptr;
	const FString& Cmd = Args[0];
	const auto Arg = [&Args](int32 i) { return Args.IsValidIndex(i) ? Args[i] : FString(); };
	FString Result = TEXT("ok");

	if (Cmd == TEXT("setmain") && Paths) { Paths->DevSetMainPath(FName(*Arg(1))); }
	else if (Cmd == TEXT("setsub") && Paths) { Result = Paths->SetSubPath(Arg(1) == TEXT("none") ? NAME_None : FName(*Arg(1))); }
	else if (Cmd == TEXT("givepoints") && Paths) { Paths->GivePoints(FName(*Arg(1)), FCString::Atoi(*Arg(2))); }
	else if (Cmd == TEXT("autospend") && Paths) { Result = FString::Printf(TEXT("spent %d"), Paths->AutoSpend(FName(*Arg(1)), FCString::Atoi(*Arg(2)))); }
	else if (Cmd == TEXT("buy") && Paths) { Result = Paths->BuyNode(FName(*Arg(1)), FName(*Arg(2))); }
	else if (Cmd == TEXT("weapon") && Survivor)
	{
		Survivor->Weapon.Metal = FName(*Arg(1));
		Survivor->Weapon.Quality = FName(*Arg(2));
		Survivor->Weapon.Upgrade = FCString::Atoi(*Arg(3));
	}
	else if (Cmd == TEXT("spawn") && Survivor)
	{
		const int32 Count = FMath::Max(1, FCString::Atoi(*Arg(2)));
		for (int32 i = 0; i < Count; ++i)
		{
			const FVector At = Survivor->GetActorLocation() + Survivor->GetActorForwardVector() * 700.f + FVector(FMath::FRandRange(-200.f, 200.f), FMath::FRandRange(-200.f, 200.f), 0.f);
			AWRCreature::SpawnCreature(GetWorld(), FName(*Arg(1)), At, FRotator(0.f, Survivor->GetActorRotation().Yaw + 180.f, 0.f));
		}
	}
	else if (Cmd == TEXT("status") && Survivor)
	{
		const FString S = Arg(1).ToLower();
		if (S == TEXT("burn")) Survivor->Status->ApplyStatus(WRTags::Status_Burn, 6.f);
		else if (S == TEXT("frostbite")) { Survivor->Status->ApplyStatus(WRTags::Status_Frostbite, 6.f); Survivor->Status->ApplyStatus(WRTags::Status_Slowed, 6.f); }
		else if (S == TEXT("frozen")) Survivor->Status->ApplyStatus(WRTags::Status_Frozen, 2.5f);
		else if (S == TEXT("stagger")) Survivor->Status->Stagger(1.5f);
		else Result = TEXT("unknown status");
	}
	else if (Cmd == TEXT("heal") && ASC && PS->GetAttributes())
	{
		const UWRAttributeSet* A = PS->GetAttributes();
		ASC->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), A->GetMaxHealth());
		ASC->SetNumericAttributeBase(UWRAttributeSet::GetStaminaAttribute(), A->GetMaxStamina());
		ASC->SetNumericAttributeBase(UWRAttributeSet::GetHungerAttribute(), 100.f);
		ASC->SetNumericAttributeBase(UWRAttributeSet::GetThirstAttribute(), 100.f);
	}
	else if (Cmd == TEXT("killme") && Survivor) { Survivor->Die(nullptr); }
	else if (Cmd == TEXT("goto") && Survivor)
	{
		AActor* Best = nullptr;
		float BestDist = TNumericLimits<float>::Max();
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			const AWRStation* Station = Cast<AWRStation>(*It);
			const AWRCreature* Creature = Cast<AWRCreature>(*It);
			const AWRNest* Nest = Cast<AWRNest>(*It);
			const bool bMatch = (Station && Station->StationType == FName(*Arg(1))) || (Arg(1) == TEXT("hearth") && It->IsA<AWRHearth>())
				|| (Creature && Creature->IsAlive() && Creature->CreatureId == FName(*Arg(1)) && (Arg(2) == TEXT("tame")) == Creature->IsTamed())
				|| (Nest && Arg(1) == TEXT("nest") && Nest->CreatureId == FName(*Arg(2)));
			const float D = FVector::Dist(It->GetActorLocation(), Survivor->GetActorLocation());
			if (bMatch && D < BestDist)
			{
				BestDist = D;
				Best = *It;
			}
		}
		if (Best)
		{
			// Stand in front of it (stations sit in a row along X).
			Survivor->TeleportTo(Best->GetActorLocation() + FVector(0.f, 200.f, 100.f), Survivor->GetActorRotation());
		}
		Result = Best ? FString::Printf(TEXT("at %s"), *Best->GetName()) : TEXT("none found");
	}
	else if (RunGameplayCommand(Args, Result)) {}
	else if (Cmd == TEXT("torpor") || Cmd == TEXT("tamestat"))
	{
		// Testing aid: knock out / inspect the nearest creature.
		AWRCreature* Best = nullptr;
		float BestDist = 1500.f;
		for (TActorIterator<AWRCreature> It(GetWorld()); It && GetPawn(); ++It)
		{
			const float D = FVector::Dist(It->GetActorLocation(), GetPawn()->GetActorLocation());
			if (It->IsAlive() && !It->IsA<AWRTrainingDummy>() && (Cmd == TEXT("tamestat") || !It->IsTamed()) && D < BestDist) { BestDist = D; Best = *It; }
		}
		if (Best && Cmd == TEXT("torpor")) Best->AddTorpor(FCString::Atof(*Arg(1)));
		Result = Best ? FString::Printf(TEXT("%s torpor %.0f/%.0f tame %.0f%% eff %.0f%% stash %d %s%s rider %s bonus +%.0f%% hp %.0f at %s"), *Best->GetDisplayLabel(),
			Best->Torpor, Best->MaxTorpor, Best->TameProgress * 100.f, Best->TameEffectiveness * 100.f, Best->TameFoodCount,
			Best->IsUnconscious() ? TEXT("unconscious ") : TEXT(""), Best->TameOrder == EWRTameOrder::Stay ? TEXT("stay") : TEXT("follow"),
			*GetNameSafe(Best->GetRider()), Best->TameBonus * 100.f, Best->GetAttributes() ? Best->GetAttributes()->GetMaxHealth() : 0.f,
			*Best->GetActorLocation().ToCompactString()) : TEXT("no creature nearby");
		ClientNotify(Result);
	}
	else if (Cmd == TEXT("spawnat"))
	{
		FVector At(FCString::Atof(*Arg(2)), FCString::Atof(*Arg(3)), 5000.f);
		FHitResult Ground;
		if (GetWorld()->LineTraceSingleByChannel(Ground, At, At - FVector(0.f, 0.f, 10000.f), ECC_WorldStatic))
		{
			At = Ground.ImpactPoint + FVector(0.f, 0.f, 100.f);
		}
		AWRCreature* Spawned = AWRCreature::SpawnCreature(GetWorld(), FName(*Arg(1)), At, FRotator::ZeroRotator);
		Result = Spawned ? FString::Printf(TEXT("%s at %s"), *Spawned->GetName(), *At.ToCompactString()) : TEXT("couldn't spawn");
	}
	else if (Cmd == TEXT("wildlife") && Survivor)
	{
		// The island's wildlife sites: how many are out, what's around you, the nearest one still quiet.
		int32 Sites = 0, Active = 0, Out = 0;
		TMap<FName, int32> Near;
		const AWRCreatureSpawner* Quiet = nullptr;
		float QuietDist = TNumericLimits<float>::Max();
		for (TActorIterator<AWRCreatureSpawner> It(GetWorld()); It; ++It)
		{
			if (It->ActivationRadius <= 0.f)
			{
				continue;
			}
			++Sites;
			const float D = FVector::Dist2D(It->GetActorLocation(), Survivor->GetActorLocation());
			if (It->IsActive())
			{
				++Active;
				Out += It->NumAlive();
			}
			else if (D < QuietDist)
			{
				QuietDist = D;
				Quiet = *It;
			}
		}
		for (TActorIterator<AWRCreature> It(GetWorld()); It; ++It)
		{
			if (It->IsAlive() && !It->IsTamed() && !It->IsA<AWRTrainingDummy>() && FVector::Dist2D(It->GetActorLocation(), Survivor->GetActorLocation()) < 30000.f)
			{
				Near.FindOrAdd(It->CreatureId)++;
			}
		}
		FString NearText;
		for (const TPair<FName, int32>& N : Near) NearText += FString::Printf(TEXT("%s%s %d"), NearText.IsEmpty() ? TEXT("") : TEXT(", "), *N.Key.ToString(), N.Value);
		Result = FString::Printf(TEXT("sites %d, active %d, out %d; within 300 m: %s; nearest quiet site %s"), Sites, Active, Out, NearText.IsEmpty() ? TEXT("none") : *NearText,
			Quiet ? *FString::Printf(TEXT("%s %.0f m at %.0f %.0f"), *Quiet->CreatureId.ToString(), QuietDist / 100.f, Quiet->GetActorLocation().X, Quiet->GetActorLocation().Y) : TEXT("none"));
		ClientNotify(Result);
	}
	else if (Cmd == TEXT("creaturehp") && Survivor)
	{
		AWRCreature* Best = nullptr;
		float BestDist = TNumericLimits<float>::Max();
		for (TActorIterator<AWRCreature> It(GetWorld()); It; ++It)
		{
			const float D = FVector::Dist(It->GetActorLocation(), Survivor->GetActorLocation());
			if (It->IsAlive() && !It->IsTamed() && It->CreatureId == FName(*Arg(1)) && D < BestDist) { BestDist = D; Best = *It; }
		}
		UAbilitySystemComponent* CreatureASC = Best ? Best->GetAbilitySystemComponent() : nullptr;
		if (CreatureASC && Best->GetAttributes())
		{
			CreatureASC->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), Best->GetAttributes()->GetMaxHealth() * FMath::Clamp(FCString::Atof(*Arg(2)), 0.01f, 1.f));
		}
		Result = Best ? FString::Printf(TEXT("%s health %.0f"), *Best->GetName(), Best->GetAttributes()->GetHealth()) : TEXT("none found");
	}
	else if (Cmd == TEXT("hour"))
	{
		if (AWRGameState* State = GetWorld()->GetGameState<AWRGameState>())
		{
			State->SetHour(FCString::Atof(*Arg(1)));
			Result = FString::Printf(TEXT("day %d %05.2f%s"), State->GetDay(), State->GetHour(), State->IsNight() ? TEXT(" (night)") : TEXT(""));
		}
	}
	else if ((Cmd == TEXT("hold") || Cmd == TEXT("clearcreatures")) && Survivor)
	{
		// Testing aid: a target that stays put (rooted) / a clean slate for the next one (training dummies too: a clear
		// line of fire).
		int32 Count = 0;
		for (TActorIterator<AWRCreature> It(GetWorld()); It; ++It)
		{
			if ((Cmd == TEXT("hold") && It->IsA<AWRTrainingDummy>()) || It->IsTamed() || FVector::Dist(It->GetActorLocation(), Survivor->GetActorLocation()) > 3000.f)
			{
				continue;
			}
			if (Cmd == TEXT("clearcreatures"))
			{
				It->Destroy();
				++Count;
			}
			else if (It->IsAlive() && It->Status)
			{
				It->Status->ApplyStatus(WRTags::Status_Rooted, FMath::Max(1.f, FCString::Atof(*Arg(1))));
				++Count;
			}
		}
		Result = FString::Printf(TEXT("%s %d creatures"), Cmd == TEXT("hold") ? TEXT("held") : TEXT("removed"), Count);
	}
	else if (Cmd == TEXT("bossdmg") && Survivor)
	{
		AWRBoss* Boss = nullptr;
		for (TActorIterator<AWRBoss> It(GetWorld()); It; ++It) if (It->IsAlive()) { Boss = *It; break; }
		if (Boss)
		{
			FWRHit Hit;
			Hit.Instigator = Survivor;
			Hit.Damage = FCString::Atof(*Arg(1)) * (Arg(1).EndsWith(TEXT("%")) ? Boss->GetAttributes()->GetMaxHealth() / 100.f : 1.f);
			Hit.Element = Arg(2) == TEXT("fire") ? WRTags::Element_Fire : Arg(2) == TEXT("frost") ? WRTags::Element_Frost : WRTags::Element_Physical;
			Hit.Origin = Boss->GetActorLocation();
			UWRCombatLibrary::ApplyHit(Boss, Hit);
			Result = FString::Printf(TEXT("%s hp %.0f/%.0f phase %d shell %.0f%% %s%s"), *Boss->GetDisplayLabel(), Boss->GetAttributes()->GetHealth(), Boss->GetAttributes()->GetMaxHealth(),
				Boss->Phase, Boss->ShellFraction * 100.f, Boss->bAirborne ? TEXT("airborne ") : TEXT(""), Boss->IsHidden() ? TEXT("hidden") : TEXT(""));
		}
		else Result = TEXT("no boss");
		ClientNotify(Result);
	}
	else if ((Cmd == TEXT("skill") || Cmd == TEXT("skillxp")) && PS)
	{
		UWRLifeSkillsComponent* Skills = PS->GetLifeSkills();
		if (Cmd == TEXT("skill")) Skills->DevSetLevel(FName(*Arg(1)), FCString::Atoi(*Arg(2)));
		else Skills->AwardXP(FName(*Arg(1)), FCString::Atof(*Arg(2)));
		const FWRSkillProgress* P = Skills->FindProgress(FName(*Arg(1)));
		Result = P ? FString::Printf(TEXT("%s level %d (%d xp)"), *Arg(1), P->Level, P->XP) : TEXT("unknown skill");
		ClientNotify(Result);
	}
	else if (Cmd == TEXT("hitplayer") && Survivor)
	{
		AWRCharacterBase* Victim = nullptr;
		for (TActorIterator<AWRCharacter> It(GetWorld()); It; ++It)
		{
			if (It->GetPlayerState() && It->GetPlayerState()->GetPlayerName() == Arg(1)) Victim = *It;
		}
		FWRHit Hit;
		Hit.Instigator = Survivor;
		Hit.Damage = FCString::Atof(*Arg(2));
		Hit.Origin = Survivor->GetActorLocation();
		Result = Victim ? FString::Printf(TEXT("hit %s: %s"), *Arg(1), UWRCombatLibrary::ResultName(UWRCombatLibrary::ApplyHit(Victim, Hit))) : TEXT("no such player");
		ClientNotify(Result);
	}
	else if (Cmd == TEXT("hazard") && Survivor)
	{
		const FVector Feet = Survivor->GetActorLocation() - FVector(0.f, 0.f, Survivor->GetSimpleCollisionHalfHeight());
		const FName Kind = FWRBiomeHazards::KindAt(Feet);
		Result = FString::Printf(TEXT("hazard %s resist %s onIce %s friction %.1f hp %.0f"), Kind.IsNone() ? TEXT("none") : *Kind.ToString(),
			FWRBiomeHazards::Resists(Survivor, Kind) ? TEXT("yes") : TEXT("no"), Survivor->IsOnIce() ? TEXT("yes") : TEXT("no"),
			Survivor->GetCharacterMovement()->GroundFriction, Survivor->GetAttributes()->GetHealth());
		ClientNotify(Result);
	}
	else if (Cmd == TEXT("skillstat") && PS)
	{
		Result = FString::Printf(TEXT("stat %s = %.3f"), *Arg(1), PS->GetLifeSkills()->Stat(FName(*Arg(1))));
		ClientNotify(Result);
	}
	else if (Cmd == TEXT("artifacts") && PS) { PS->bArtifactModifiers = Arg(1) == TEXT("1"); }
	else if (Cmd == TEXT("hitpiece"))
	{
		// Testing aid: N copper-sword hits on the nearest building piece.
		if (AWRBuildPiece* Piece = FindNearbyPiece(450.f))
		{
			Piece->TakeBuildDamage(12.f * FMath::Max(1, FCString::Atoi(*Arg(1))), GetPawn());
			Result = IsValid(Piece) && !Piece->IsActorBeingDestroyed() ? FString::Printf(TEXT("%s hp %.0f"), *Piece->PieceId.ToString(), Piece->Health) : TEXT("destroyed");
		}
		else Result = TEXT("no piece nearby");
		ClientNotify(Result);
	}
	else if (Cmd == TEXT("equip") && PS)
	{
		// Testing aid: use the first inventory slot holding this item (equip / socket / consume).
		const int32 Slot = PS->GetInventory()->GetSlots().IndexOfByPredicate([&Arg](const FWRItemStack& S) { return S.ItemId == FName(*Arg(1)); });
		Result = Slot == INDEX_NONE ? TEXT("not carried") : PS->GetInventory()->UseSlot(Slot);
		ClientNotify(Result);
	}
	else if (Cmd == TEXT("give") && PS)
	{
		const int32 Left = PS->GetInventory()->AddItem(FName(*Arg(1)), FMath::Max(1, FCString::Atoi(*Arg(2))));
		Result = Left > 0 ? FString::Printf(TEXT("%d didn't fit or unknown item"), Left) : TEXT("ok");
	}
	else if (Cmd == TEXT("tp") && Survivor)
	{
		// Drop onto whatever is below (landscape or floor).
		const FVector Top(FCString::Atof(*Arg(1)), FCString::Atof(*Arg(2)), 200000.f);
		FHitResult Ground;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(WRTeleport), false, Survivor);
		const bool bHit = GetWorld()->LineTraceSingleByChannel(Ground, Top, Top - FVector(0.f, 0.f, 400000.f), ECC_WorldStatic, Query);
		const FVector Dest = bHit ? Ground.ImpactPoint + FVector(0.f, 0.f, 120.f) : FVector(Top.X, Top.Y, 1000.f);
		Survivor->TeleportTo(Dest, Survivor->GetActorRotation());
		Result = FString::Printf(TEXT("at %s (ground %s)"), *Dest.ToCompactString(), bHit ? TEXT("found") : TEXT("NOT found"));
	}
	else if (Cmd == TEXT("gotoaltar") && Survivor)
	{
		for (TActorIterator<AWRBossAltar> It(GetWorld()); It; ++It)
		{
			if (It->PathId == FName(*Arg(1)))
			{
				Survivor->TeleportTo(It->GetActorLocation() + FVector(150.f, 0.f, 100.f), Survivor->GetActorRotation());
				break;
			}
		}
	}
	else if (Cmd == TEXT("starve") && ASC)
	{
		ASC->SetNumericAttributeBase(UWRAttributeSet::GetHungerAttribute(), 0.f);
		ASC->SetNumericAttributeBase(UWRAttributeSet::GetThirstAttribute(), 0.f);
	}
	else { Result = TEXT("unknown command or no pawn"); }

	UE_LOG(LogWildreach, Log, TEXT("CMD %s: %s -> %s"), *GetNameSafe(PS), *Command, Result.IsEmpty() ? TEXT("ok") : *Result);
}

// ---------------------------------------------------------------- test bot

void AWRPlayerController::WRArtCamFix(int32 bFix)
{
	bArtCamFixed = bFix != 0;
}

void AWRPlayerController::WRShotSeq(const FString& Prefix, int32 Count, float Every)
{
	ShotSeqPrefix = Prefix;
	ShotSeqTaken = 0;
	ShotSeqCount = FMath::Max(0, Count);
	ShotSeqEvery = FMath::Max(0.02f, Every);
	ShotSeqNext = BotClock;
}

void AWRPlayerController::WRAnimDrive(float Angle, float Seconds, float Scale, float TurnRate)
{
	DriveAngle = Angle;
	DriveUntil = BotClock + Seconds;
	DriveScale = Scale;
	DriveTurn = TurnRate;
}

void AWRPlayerController::WRPress(FName Input, float Seconds)
{
	const EWRAbilityInput Which = Input == TEXT("heavy") ? EWRAbilityInput::Heavy : Input == TEXT("block") ? EWRAbilityInput::Block
		: Input == TEXT("dodge") ? EWRAbilityInput::Dodge : EWRAbilityInput::Light;
	BotPress(static_cast<int32>(Which), Seconds);
}

void AWRPlayerController::BotPress(int32 Input, float HoldSeconds)
{
	if (AWRCharacter* Survivor = Cast<AWRCharacter>(GetPawn()))
	{
		Survivor->PressAbilityInput(static_cast<EWRAbilityInput>(Input));
		BotHeld.Add({ Input, BotClock + HoldSeconds });
	}
}

void AWRPlayerController::TickBot(float DeltaTime)
{
	BotClock += DeltaTime;
	AWRCharacter* Survivor = Cast<AWRCharacter>(GetPawn());

	for (int32 i = BotHeld.Num() - 1; i >= 0; --i)
	{
		if (BotHeld[i].Value <= BotClock)
		{
			if (Survivor)
			{
				Survivor->ReleaseAbilityInput(static_cast<EWRAbilityInput>(BotHeld[i].Key));
			}
			BotHeld.RemoveAt(i);
		}
	}
	const bool bRiding = Cast<AWRCreature>(GetPawn()) != nullptr;
	if (!bRiding && (!Survivor || !Survivor->IsAlive()))
	{
		return;
	}

	// Run the setup commands from -wrbotcmds="cmd one|wait 3|cmd two" once the bot is in the world.
	if (!bBotSetupDone && BotClock > 1.5f)
	{
		bBotSetupDone = true;
		FString Cmds;
		if (FParse::Value(FCommandLine::Get(), TEXT("wrbotcmds="), Cmds, false))
		{
			Cmds.ParseIntoArray(BotCmdQueue, TEXT("|"));
		}
	}
	while (BotCmdQueue.Num() > 0 && BotClock >= BotCmdResume && ShotSeqTaken >= ShotSeqCount)  // a shot sequence finishes first
	{
		const FString Cmd = BotCmdQueue[0].TrimStartAndEnd();
		BotCmdQueue.RemoveAt(0);
		if (Cmd.StartsWith(TEXT("wait ")))
		{
			BotCmdResume = BotClock + FCString::Atof(*Cmd.RightChop(5));
			break;
		}
		ConsoleCommand(Cmd);
	}
	if (!Survivor)
	{
		return;
	}
	if (ShotSeqTaken < ShotSeqCount && BotClock >= ShotSeqNext)
	{
		ConsoleCommand(FString::Printf(TEXT("HighResShot 1280x720 filename=%s%02d"), *ShotSeqPrefix, ShotSeqTaken++));
		ShotSeqNext += ShotSeqEvery;
	}
	if (BotClock < DriveUntil)
	{
		// WRAnimDrive: strafe / back off / circle relative to the camera (animation captures).
		SetControlRotation(GetControlRotation() + FRotator(0.f, DriveTurn * DeltaTime, 0.f));
		if (DriveScale > 0.f)
		{
			Survivor->AddMovementInput(FRotator(0.f, GetControlRotation().Yaw + DriveAngle, 0.f).Vector(), DriveScale);
		}
	}
	if (bWalking)
	{
		const FVector Here = Survivor->GetActorLocation();
		const FVector2D To = WalkTarget - FVector2D(Here.X, Here.Y);
		const float Dist = To.Size();
		if (Dist < WalkBestDist - 100.f)
		{
			WalkBestDist = Dist;
			WalkLastProgress = BotClock;
		}
		if (Dist < 400.f)
		{
			UE_LOG(LogWildreach, Log, TEXT("WALK arrived after %.0f s (%.1f min) at %s"), BotClock - WalkStart, (BotClock - WalkStart) / 60.f, *Here.ToCompactString());
			bWalking = false;
		}
		else if (BotClock - WalkLastProgress > 15.f)
		{
			UE_LOG(LogWildreach, Log, TEXT("WALK stuck at %s, %.0f m from target after %.0f s"), *Here.ToCompactString(), Dist / 100.f, BotClock - WalkStart);
			bWalking = false;
		}
		else
		{
			if (BotClock >= WalkNextLog)
			{
				WalkNextLog = BotClock + 30.f;
				UE_LOG(LogWildreach, Log, TEXT("WALK %.0f s: at %s, %.0f m to go"), BotClock - WalkStart, *Here.ToCompactString(), Dist / 100.f);
			}
			SetControlRotation(FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(To.Y, To.X)), 0.f));
			Survivor->AddMovementInput(FVector(To.GetSafeNormal(), 0.f), 1.f);
		}
		return;
	}
	if (bBotPaused)
	{
		return;
	}

	if (bBotGather)
	{
		// Walk to the nearest standing resource node (or island tree / rock / shrub, WRGather) and swing at it.
		const FVector Here = Survivor->GetActorLocation();
		FVector Best = FVector::ZeroVector;
		float BestDist = 1e9f;
		float Reach = 180.f;
		for (TActorIterator<AWRResourceNode> It(GetWorld()); It && (BotGatherGroup == TEXT("node") || BotGatherGroup == TEXT("any")); ++It)
		{
			const float D = FVector::Dist2D(It->GetActorLocation(), Here);
			if (!It->IsDepleted() && D < BestDist)
			{
				BestDist = D;
				Best = It->GetActorLocation();
			}
		}
		const AWRDressing* Dressing = BotGatherGroup == TEXT("node") ? nullptr : UWRDressingSubsystem::FindDressing(GetWorld());
		FVector Ground;
		if (Dressing && Dressing->FindNearest(Here, BotGatherGroup == TEXT("any") ? FString() : BotGatherGroup, FMath::Min(BestDist, 6000.f), Ground))
		{
			BestDist = FVector::Dist2D(Ground, Here);
			Best = Ground;
			Reach = 220.f;  // rocks are wide: stop outside them
		}
		if (BestDist < 1e9f)
		{
			const FVector To = Best - Here;
			SetControlRotation(To.Rotation());
			if (BestDist > Reach)
			{
				Survivor->AddMovementInput(To.GetSafeNormal2D(), 1.f);
			}
			else if (BotClock >= BotNextAttack)
			{
				BotNextAttack = BotClock + 0.9f;
				BotPress(static_cast<int32>(EWRAbilityInput::Light), 0.05f);
			}
		}
		return;
	}

	if (!LockTarget.IsValid() || !LockTarget->IsAlive())
	{
		LockTarget = FindTarget(8000.f, 180.f);
	}
	AWRCharacterBase* Target = LockTarget.Get();

	if (BotClock >= BotNextReport)
	{
		BotNextReport = BotClock + 5.f;
		const UWRAttributeSet* A = Survivor->GetAttributes();
		const UWRPathComponent* Paths = UWRPathComponent::FindFor(Survivor);
		UE_LOG(LogWildreach, Log, TEXT("BOT %s hp=%.0f/%.0f st=%.0f heat=%.0f chill=%.0f main=%s(%d/%d) sub=%s(%d) target=%s dist=%.0f"),
			*GetNameSafe(Survivor), A ? A->GetHealth() : -1.f, A ? A->GetMaxHealth() : -1.f, A ? A->GetStamina() : -1.f,
			A ? A->GetHeat() : -1.f, A ? A->GetChill() : -1.f,
			Paths ? *Paths->GetMainPath().ToString() : TEXT("?"), Paths ? Paths->GetSpent(Paths->GetMainPath()) : 0,
			Paths ? Paths->GetEarned(Paths->GetMainPath()) : 0,
			Paths ? *Paths->GetSubPath().ToString() : TEXT("?"), Paths ? Paths->GetSpent(Paths->GetSubPath()) : 0,
			*GetNameSafe(Target), Target ? FVector::Dist(Target->GetActorLocation(), Survivor->GetActorLocation()) : -1.f);
	}

	// Drink a potion when low (the belt cooldown paces it).
	if (const UWRAttributeSet* A = Survivor->GetAttributes())
	{
		if (A->GetHealth() < A->GetMaxHealth() * 0.4f && BotClock >= BotNextPotion)
		{
			BotNextPotion = BotClock + 3.f;
			UsePotion();
		}
	}

	if (!Target)
	{
		return;
	}

	const FVector To = Target->GetActorLocation() - Survivor->GetActorLocation();
	const float Dist = To.Size2D();
	if (Dist > 170.f)
	{
		Survivor->AddMovementInput(To.GetSafeNormal2D(), 1.f);
	}

	if (BotClock >= BotNextDodge)
	{
		BotNextDodge = BotClock + FMath::FRandRange(4.f, 7.f);
		BotPress(static_cast<int32>(EWRAbilityInput::Dodge), 0.05f);
		return;
	}
	if (Dist < 450.f && BotClock >= BotNextBlock)
	{
		// Raise the guard now and then (the first moments of a block are a parry). Hold off
		// attacking around it, since you can't block mid-swing.
		BotNextBlock = BotClock + FMath::FRandRange(3.f, 6.f);
		BotBlockAt = BotClock + 0.8f;
		BotNextAttack = FMath::Max(BotNextAttack, BotClock + 2.f);
		BotNextSpell = FMath::Max(BotNextSpell, BotClock + 2.f);
	}
	if (BotBlockAt > 0.f && BotClock >= BotBlockAt)
	{
		BotBlockAt = 0.f;
		BotPress(static_cast<int32>(EWRAbilityInput::Block), 1.0f);
		return;
	}
	if (Dist < 2500.f && BotClock >= BotNextSpell)
	{
		// Rotate through the path abilities in the quick slots (spells and weapon arts).
		// Pairs same-path abilities (shard, slash, shard, slash, bolt, slash...) like a player stacking buildup.
		// Slots 2 and 4 are weapon arts (melee), so only use those in reach.
		static const int32 Pattern[] = { 0, 1, 0, 1, 2, 3, 2, 3 };
		const int32 Slot = Pattern[BotSpellSlot++ % 8];
		if ((Slot == 1 || Slot == 3) && Dist > 260.f)
		{
			BotNextSpell = BotClock + 0.2f;
			return;
		}
		BotNextSpell = BotClock + 1.3f;
		BotPress(static_cast<int32>(EWRAbilityInput::Slot1) + Slot, 0.05f);
		return;
	}
	if (Dist <= 260.f && BotClock >= BotNextAttack)
	{
		BotNextAttack = BotClock + 0.8f;
		const bool bHeavy = FMath::RandRange(0, 3) == 0;
		BotPress(static_cast<int32>(bHeavy ? EWRAbilityInput::Heavy : EWRAbilityInput::Light), 0.05f);
	}
}
