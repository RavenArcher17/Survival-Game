#include "WRPlayerController.h"
#include "Wildreach.h"
#include "WRAltar.h"
#include "WRAttributeSet.h"
#include "WRCharacter.h"
#include "WRCombatTypes.h"
#include "WRCreature.h"
#include "WRGameplayTags.h"
#include "WRPathComponent.h"
#include "WRPlayerState.h"
#include "WRStatusComponent.h"
#include "WRTuning.h"
#include "AbilitySystemComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"

AWRPlayerController::AWRPlayerController()
{
	bBot = FParse::Param(FCommandLine::Get(), TEXT("wrbot")) || FParse::Param(FCommandLine::Get(), TEXT("wrbotpvp"));
	bBotPvP = FParse::Param(FCommandLine::Get(), TEXT("wrbotpvp"));
	bBotHunt = FParse::Param(FCommandLine::Get(), TEXT("wrbothunt"));
}

void AWRPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &AWRPlayerController::ToggleLockOn);
	InputComponent->BindKey(EKeys::E, IE_Pressed, this, &AWRPlayerController::WRInteract);
}

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

void AWRPlayerController::WRInteract() { ServerCommand(TEXT("interact")); }
void AWRPlayerController::WRBotPause(int32 bPause) { bBotPaused = bPause != 0; }

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
	if (!Command.StartsWith(TEXT("interact")))
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
	else if (Cmd == TEXT("interact"))
	{
		// Interaction is a gameplay action, not a dev command: it stays available in shipping (see ServerCommand).
		AWRBossAltar* Altar = FindNearbyAltar();
		Result = Altar ? Altar->Interact(this) : TEXT("nothing to interact with");
		if (Altar)
		{
			ClientNotify(Result);
		}
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
	if (!Survivor || !Survivor->IsAlive())
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
	if (bBotPaused)
	{
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
		static const int32 Pattern[] = { 0, 1, 0, 1, 2, 3, 2, 3 };
		BotNextSpell = BotClock + 1.3f;
		BotPress(static_cast<int32>(EWRAbilityInput::Slot1) + Pattern[BotSpellSlot++ % 8], 0.05f);
		return;
	}
	if (Dist <= 260.f && BotClock >= BotNextAttack)
	{
		BotNextAttack = BotClock + 0.8f;
		const bool bHeavy = FMath::RandRange(0, 3) == 0;
		BotPress(static_cast<int32>(bHeavy ? EWRAbilityInput::Heavy : EWRAbilityInput::Light), 0.05f);
	}
}
