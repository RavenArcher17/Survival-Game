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
#include "WRInventory.h"
#include "WRPathComponent.h"
#include "WRPlayerState.h"
#include "WRResourceNode.h"
#include "WRStatusComponent.h"
#include "WRTuning.h"
#include "AbilitySystemComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraActor.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "WRMainMenu.h"
#include "Kismet/GameplayStatics.h"

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
	InputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &AWRPlayerController::ToggleLockOn);
	InputComponent->BindKey(EKeys::E, IE_Pressed, this, &AWRPlayerController::WRInteract);
	InputComponent->BindKey(EKeys::Q, IE_Pressed, this, &AWRPlayerController::UsePotion);
	InputComponent->BindKey(EKeys::Z, IE_Pressed, this, &AWRPlayerController::UseFood);
	InputComponent->BindKey(EKeys::X, IE_Pressed, this, &AWRPlayerController::UseDrink);
	InputComponent->BindKey(EKeys::I, IE_Pressed, this, &AWRPlayerController::ToggleInventory);
	InputComponent->BindKey(EKeys::K, IE_Pressed, this, &AWRPlayerController::ToggleSkills);
	InputComponent->BindKey(EKeys::P, IE_Pressed, this, &AWRPlayerController::TogglePaths);
	InputComponent->BindKey(EKeys::F10, IE_Pressed, this, &AWRPlayerController::ReturnToMenu);
	InputComponent->BindKey(EKeys::M, IE_Pressed, this, &AWRPlayerController::ToggleMap);
	InputComponent->BindKey(EKeys::T, IE_Pressed, this, &AWRPlayerController::OpenChat);
	InputComponent->BindKey(EKeys::Up, IE_Pressed, this, &AWRPlayerController::MenuUp);
	InputComponent->BindKey(EKeys::Down, IE_Pressed, this, &AWRPlayerController::MenuDown);
	InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &AWRPlayerController::MenuConfirm);
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AWRPlayerController::MenuClose);
	InputComponent->BindKey(EKeys::F, IE_Pressed, this, &AWRPlayerController::CycleInfuse);
	InputComponent->BindKey(EKeys::B, IE_Pressed, this, &AWRPlayerController::ToggleBuildMenu);
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
	SetControlRotation(FMath::RInterpTo(GetControlRotation(), Desired, DeltaTime, bBot ? 30.f : 10.f));
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
		TEXT("place "), TEXT("setcode "), TEXT("code "), TEXT("demolish"), TEXT("claim"), TEXT("store "), TEXT("take "), TEXT("tame "), TEXT("dismount"), TEXT("clan "), TEXT("perk "), TEXT("learn ") };  // chat is its own RPC
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
	while (BotCmdQueue.Num() > 0 && BotClock >= BotCmdResume)
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
