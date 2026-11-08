#include "WRGameMode.h"
#include "WRBeds.h"
#include "WRGameState.h"
#include "Wildreach.h"
#include "WRCharacter.h"
#include "WRCreature.h"
#include "WRHUD.h"
#include "WRBuilding.h"
#include "WRInventory.h"
#include "WRLifeSkills.h"
#include "WRSocial.h"
#include "WRPathComponent.h"
#include "WRPathTypes.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRRaids.h"
#include "WRTuning.h"
#include "WRWater.h"
#include "WRWildlife.h"
#include "WRWorlds.h"
#include "Dom/JsonObject.h"
#include "Misc/PackageName.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerStart.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "LandscapeHeightfieldCollisionComponent.h"

AWRGameMode::AWRGameMode()
{
	DefaultPawnClass = AWRCharacter::StaticClass();
	HUDClass = AWRHUD::StaticClass();
	PlayerStateClass = AWRPlayerState::StaticClass();
	PlayerControllerClass = AWRPlayerController::StaticClass();
	GameStateClass = AWRGameState::StaticClass();
}

void AWRGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	// Which world this is: the menu's Host names it (?World=), tests and dedicated servers pass -wrworld=, and any other
	// server plays the map's own (L_Island: "Island", where 0.5's save moved). The menu's island on its own (not
	// hosting: no ?listen yet at this point, which comes after InitGame) is no world and saves nothing.
	FString Id = UGameplayStatics::ParseOption(Options, TEXT("World"));
	if (Id.IsEmpty()) FParse::Value(FCommandLine::Get(), TEXT("-wrworld="), Id);
	if (Id.IsEmpty() && (IsRunningDedicatedServer() || UGameplayStatics::HasOption(Options, TEXT("listen"))))
	{
		Id = UWorld::RemovePIEPrefix(FPackageName::GetShortName(MapName));
		Id.RemoveFromStart(TEXT("L_"));
	}
	Id = WRWorlds::SafeId(Id);
	if (Id == TEXT("Island"))
	{
		WRWorlds::MigrateOldSave();
	}
	WRWorlds::SetCurrent(Id);
	FWRTuning::Get().Reload();   // the world's settings.json
	FWRClans::Get().Load();
	UE_LOG(LogWildreach, Log, TEXT("WORLD %s"), Id.IsEmpty() ? TEXT("none (the menu's island: nothing is loaded or saved)") : *FString::Printf(TEXT("'%s' (Saved/Worlds/%s), preset %s"), *Id, *Id, *FWRTuning::Get().GetPreset()));
}

FString AWRGameMode::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal)
{
	const FString Result = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);

	// Main path is picked at creation. Until there's a creation screen it comes from the
	// join URL (?Path=frost), defaulting to fire.
	FString Path = UGameplayStatics::ParseOption(Options, TEXT("Path")).ToLower();
	if (!FWRPathRegistry::Get().Find(FName(*Path)))
	{
		Path = TEXT("fire");
	}
	if (AWRPlayerState* PS = NewPlayerController ? NewPlayerController->GetPlayerState<AWRPlayerState>() : nullptr)
	{
		// Without an online subsystem the engine names players after the machine plus a random
		// suffix, which changes every launch. ?WRName= gives a stable name (and save key) until
		// real account ids arrive (EOS/Steam, M7).
		const FString WRName = UGameplayStatics::ParseOption(Options, TEXT("WRName"));
		if (!WRName.IsEmpty())
		{
			ChangeName(NewPlayerController, WRName.Left(24), false);
		}
		PS->GetPaths()->SetMainPath(FName(*Path));
		const FString Body = UGameplayStatics::ParseOption(Options, TEXT("Body")).ToLower().Left(8);
		PS->BodyStyle = Body.IsEmpty() ? NAME_None : FName(*Body);
		// A returning player's saved paths override the join URL.
		LoadPlayer(PS);
		// The engine picked the start spot inside Super::InitNewPlayer, before the main path was known, so the first
		// spawn landed at any camp. Pick again now: the path's biome (or a saved bed).
		// (FindPlayerStart hands back an existing StartSpot, so it's cleared first.)
		NewPlayerController->StartSpot = nullptr;
		FString StartError;
		UpdatePlayerStartSpot(NewPlayerController, Portal, StartError);
	}
	return Result;
}

void AWRGameMode::BeginPlay()
{
	Super::BeginPlay();
	LoadWorld();
	WRWildlife::Start(GetWorld());
	WRRaids::Start(GetWorld());
	if (!WRWorlds::Current().IsEmpty())
	{
		const float Every = FMath::Max(0.1f, WRTune(TEXT("wildlife.stepSeconds"), 20.f));
		GetWorldTimerManager().SetTimer(WildlifeTimer, FTimerDelegate::CreateWeakLambda(this, [this, Every]() { WRWildlife::Step(GetWorld(), Every); }), Every, true);
		GetWorldTimerManager().SetTimer(RaidTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { WRRaids::Step(GetWorld()); }),
			FMath::Max(0.25f, WRTune(TEXT("raid.stepSeconds"), 2.f)), true);
	}
	GetWorldTimerManager().SetTimer(AutosaveTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { SaveAllPlayers(true); }), 10.f, true);
	GetWorldTimerManager().SetTimer(WorldSaveTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { SaveWorld(); }), 60.f, true);
	GetWorldTimerManager().SetTimer(UndergroundTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { RescueUnderground(); }), 1.f, true);
}

void AWRGameMode::RescueUnderground()
{
	// Someone who arrives before the ground's collision is there (a teleport or a respawn far from everyone) falls
	// through the island, where it can't be seen from below. The landscape is a heightfield, so ground above you always
	// means you're under it: put them back on top.
	const float Margin = WRTune(TEXT("world.undergroundMargin"), 300.f);
	// In the water (WRWater) any ground overhead will do: open water has nothing over it, so a swimmer under the
	// landscape fell through the sea floor into the water below it, and would float up against the ground's underside.
	const float SwimMargin = WRTune(TEXT("water.rescueMargin"), 0.f);
	// Traced from over the island's highest ground however deep the fall, and held at voidZ (nothing is ground below
	// the sea floor) until the ground under them streams in: the map's old flat plane under the middle of the island
	// used to catch some of them, and is gone now.
	const float TopZ = WRTune(TEXT("water.rescueTraceTop"), 150000.f);
	const float VoidZ = WRTune(TEXT("water.voidZ"), -5000.f);
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		AWRCharacterBase* C = *It;
		// A burrower lying in wait has no collision on purpose (AWRCreature hides it underground).
		if (!C->IsAlive() || !C->GetActorEnableCollision())
		{
			continue;
		}
		const FVector L = C->GetActorLocation();
		const float Need = UWRWaterSubsystem::IsInWater(C) ? SwimMargin : Margin;
		TArray<FHitResult> Hits;
		const FCollisionQueryParams Query(SCENE_QUERY_STAT(WRUnderground), false, C);
		GetWorld()->LineTraceMultiByObjectType(Hits, FVector(L.X, L.Y, FMath::Max(L.Z + 80000.f, TopZ)), L, FCollisionObjectQueryParams(ECC_WorldStatic), Query);
		bool bRescued = false;
		for (const FHitResult& Hit : Hits)
		{
			if (Cast<ULandscapeHeightfieldCollisionComponent>(Hit.GetComponent()) && Hit.ImpactPoint.Z > L.Z + Need)
			{
				const float Half = C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
				C->TeleportTo(FVector(L.X, L.Y, Hit.ImpactPoint.Z + Half + 30.f), C->GetActorRotation(), false, true);
				C->GetCharacterMovement()->Velocity = FVector::ZeroVector;
				UE_LOG(LogWildreach, Warning, TEXT("RESCUE %s was %.0f m under the ground at %s%s: put back on top"),
					*C->GetName(), (Hit.ImpactPoint.Z - L.Z) / 100.f, *L.ToString(), Need < Margin ? TEXT(" (in the water)") : TEXT(""));
				bRescued = true;
				break;
			}
		}
		if (!bRescued && L.Z < VoidZ)
		{
			C->TeleportTo(FVector(L.X, L.Y, VoidZ), C->GetActorRotation(), false, true);
			C->GetCharacterMovement()->Velocity = FVector::ZeroVector;
			UE_LOG(LogWildreach, Warning, TEXT("RESCUE %s fell under the world at %s with no ground over it yet: held at z %.0f"), *C->GetName(), *L.ToString(), VoidZ);
		}
	}
}

void AWRGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
	SaveAllPlayers(false);
	SaveWorld();
	Super::EndPlay(Reason);
}

FString AWRGameMode::WorldSavePath() const
{
	const FString Dir = WRWorlds::CurrentDir();
	return Dir.IsEmpty() ? FString() : FPaths::Combine(Dir, TEXT("world.json"));
}

void AWRGameMode::SaveWorld()
{
	if (WorldSavePath().IsEmpty())
	{
		return;
	}
	CountPlayTime();
	TArray<TSharedPtr<FJsonValue>> Pieces;
	for (TActorIterator<AWRBuildPiece> It(GetWorld()); It; ++It)
	{
		if (!It->bGhost && !It->IsActorBeingDestroyed())
		{
			Pieces.Add(MakeShared<FJsonValueObject>(It->ToJson()));
		}
	}
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("version"), 1);
	Root->SetArrayField(TEXT("pieces"), Pieces);
	TArray<TSharedPtr<FJsonValue>> Tames;
	for (TActorIterator<AWRCreature> It(GetWorld()); It; ++It)
	{
		if (It->IsTamed() && It->IsAlive())
		{
			const TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
			O->SetStringField(TEXT("id"), It->CreatureId.ToString());
			O->SetStringField(TEXT("owner"), It->TamedBy);
			O->SetBoolField(TEXT("stay"), It->TameOrder == EWRTameOrder::Stay);
			O->SetBoolField(TEXT("aggressive"), It->bTameAggressive);
			O->SetNumberField(TEXT("bonus"), It->TameBonus);
			const FVector L = It->GetActorLocation();
			O->SetNumberField(TEXT("x"), L.X); O->SetNumberField(TEXT("y"), L.Y); O->SetNumberField(TEXT("z"), L.Z - It->GetSimpleCollisionHalfHeight());
			Tames.Add(MakeShared<FJsonValueObject>(O));
		}
	}
	Root->SetArrayField(TEXT("tames"), Tames);
	if (const AWRGameState* State = GetGameState<AWRGameState>())
	{
		Root->SetNumberField(TEXT("clockHours"), State->GetTotalHours());  // the day and time carry over a restart
	}
	FString Text;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
	FJsonSerializer::Serialize(Root, Writer);
	if (FFileHelper::SaveStringToFile(Text, *WorldSavePath()))
	{
		UE_LOG(LogWildreach, Log, TEXT("WORLD saved %d pieces"), Pieces.Num());
	}
	WRWildlife::Save(GetWorld());
}

void AWRGameMode::ChangeWorldSettings(const FWRWorldSettings& Settings)
{
	if (WRWorlds::Current().IsEmpty() || !WRWorlds::WriteSettings(WRWorlds::Current(), Settings))
	{
		UE_LOG(LogWildreach, Warning, TEXT("WORLD settings weren't saved (world '%s')"), *WRWorlds::Current());
		return;
	}
	AWRGameState* State = GetGameState<AWRGameState>();
	const double Hours = State ? State->GetTotalHours() : 0.0;   // at the old day length
	FWRTuning::Get().Reload();
	if (State)
	{
		State->RefreshServerSettings();
		State->SetTotalHours(Hours);
	}
	UE_LOG(LogWildreach, Log, TEXT("WORLD settings changed: preset %s (%s)"), *FWRTuning::Get().GetPreset(), *FString::Join(FWRTuning::Get().ExportServerSettings(), TEXT(", ")));
}

void AWRGameMode::AnnounceWorldSettings()
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		AWRPlayerController* PC = Cast<AWRPlayerController>(It->Get());
		if (PC && !PC->IsLocalController())
		{
			PC->ClientNotify(TEXT("The host changed the world's settings"));
		}
	}
}

void AWRGameMode::CountPlayTime(const AController* Leaving)
{
	// The time since last counted, if anyone was here for it: asked each save, and as players come and go (by the save
	// at the end of a game the host may have logged out already).
	const double Now = FPlatformTime::Seconds();
	if (bPlayersHere)
	{
		WRWorlds::Touch(Now - PlayCountedAt);
	}
	PlayCountedAt = Now;
	bPlayersHere = false;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		bPlayersHere |= PC && PC != Leaving && PC->PlayerState;
	}
}

void AWRGameMode::LoadWorld()
{
	FString Text;
	if (WorldSavePath().IsEmpty() || !FFileHelper::LoadFileToString(Text, *WorldSavePath()))
	{
		return;
	}
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	const TArray<TSharedPtr<FJsonValue>>* Pieces = nullptr;
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid() || !Root->TryGetArrayField(TEXT("pieces"), Pieces))
	{
		UE_LOG(LogWildreach, Error, TEXT("WORLD save is corrupt: %s"), *WorldSavePath());
		return;
	}
	int32 Loaded = 0;
	for (const TSharedPtr<FJsonValue>& V : *Pieces)
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		const FName PieceId(*O->GetStringField(TEXT("piece")));
		if (!FWRBuildRegistry::Get().Find(PieceId))
		{
			continue;
		}
		const FTransform T(FRotator(0.f, O->GetNumberField(TEXT("yaw")), 0.f),
			FVector(O->GetNumberField(TEXT("x")), O->GetNumberField(TEXT("y")), O->GetNumberField(TEXT("z"))));
		AWRBuildPiece* Piece = GetWorld()->SpawnActorDeferred<AWRBuildPiece>(AWRBuildPiece::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		Piece->InitPiece(PieceId, O->GetStringField(TEXT("owner")));
		Piece->FinishSpawning(T);
		Piece->FromJson(O);
		++Loaded;
	}
	AWRBuildPiece::RecomputeSupport(GetWorld(), FVector::ZeroVector, 1.0e7f);
	const TArray<TSharedPtr<FJsonValue>>* Tames = nullptr;
	int32 LoadedTames = 0;
	if (Root->TryGetArrayField(TEXT("tames"), Tames))
	{
		for (const TSharedPtr<FJsonValue>& V : *Tames)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			const FVector L(O->GetNumberField(TEXT("x")), O->GetNumberField(TEXT("y")), O->GetNumberField(TEXT("z")));
			if (AWRCreature* Tame = AWRCreature::SpawnCreature(GetWorld(), FName(*O->GetStringField(TEXT("id"))), L, FRotator::ZeroRotator))
			{
				Tame->CompleteTame(O->GetStringField(TEXT("owner")));
				Tame->TameOrder = O->GetBoolField(TEXT("stay")) ? EWRTameOrder::Stay : EWRTameOrder::Follow;
				Tame->bTameAggressive = O->GetBoolField(TEXT("aggressive"));
				Tame->ApplyTameBonus(O->HasField(TEXT("bonus")) ? O->GetNumberField(TEXT("bonus")) : 0.f);  // older saves: none
				++LoadedTames;
			}
		}
	}
	double ClockHours = 0.0;
	AWRGameState* State = GetGameState<AWRGameState>();
	if (State && Root->TryGetNumberField(TEXT("clockHours"), ClockHours))
	{
		State->SetTotalHours(ClockHours);
	}
	UE_LOG(LogWildreach, Log, TEXT("WORLD loaded %d pieces, %d tames, day %d %05.2f"), Loaded, LoadedTames, State ? State->GetDay() : 0, State ? State->GetHour() : -1.f);
}

FString AWRGameMode::PlayerSavePath(const FString& PlayerName)
{
	const FString Dir = WRWorlds::CurrentDir();
	if (Dir.IsEmpty())
	{
		return FString();
	}
	FString Safe;
	for (const TCHAR C : PlayerName)
	{
		Safe.AppendChar(FChar::IsAlnum(C) || C == TEXT('_') || C == TEXT('-') ? C : TEXT('_'));
	}
	return FPaths::Combine(Dir, TEXT("players"), Safe + TEXT(".json"));
}

void AWRGameMode::SavePlayer(AWRPlayerState* PS)
{
	if (!PS || PS->GetPlayerName().IsEmpty() || WRWorlds::CurrentDir().IsEmpty())
	{
		return;
	}
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("version"), 1);
	Root->SetStringField(TEXT("name"), PS->GetPlayerName());
	Root->SetObjectField(TEXT("paths"), PS->GetPaths()->ToJson());
	Root->SetObjectField(TEXT("inventory"), PS->GetInventory()->ToJson());
	Root->SetObjectField(TEXT("lifeSkills"), PS->GetLifeSkills()->ToJson());
	if (const TSharedPtr<FJsonObject> Bed = WRBeds::SaveDefault(PS))
	{
		Root->SetObjectField(TEXT("bed"), Bed);
	}

	FString Text;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
	FJsonSerializer::Serialize(Root, Writer);
	const FString Path = PlayerSavePath(PS->GetPlayerName());
	if (FFileHelper::SaveStringToFile(Text, *Path))
	{
		PS->GetPaths()->ClearDirty();
		UE_LOG(LogWildreach, Log, TEXT("SAVE %s -> %s"), *PS->GetPlayerName(), *Path);
	}
	else
	{
		UE_LOG(LogWildreach, Error, TEXT("SAVE failed for %s (%s)"), *PS->GetPlayerName(), *Path);
	}
}

void AWRGameMode::LoadPlayer(AWRPlayerState* PS)
{
	FString Text;
	if (!PS || PlayerSavePath(PS->GetPlayerName()).IsEmpty() || !FFileHelper::LoadFileToString(Text, *PlayerSavePath(PS->GetPlayerName())))
	{
		return;
	}
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogWildreach, Error, TEXT("LOAD %s: save file is corrupt, starting fresh"), *PS->GetPlayerName());
		return;
	}
	const TSharedPtr<FJsonObject>* Inventory = nullptr;
	if (Root->TryGetObjectField(TEXT("inventory"), Inventory))
	{
		PS->GetInventory()->FromJson(*Inventory);
	}
	const TSharedPtr<FJsonObject>* Skills = nullptr;
	if (Root->TryGetObjectField(TEXT("lifeSkills"), Skills))
	{
		PS->GetLifeSkills()->FromJson(*Skills);
	}
	const TSharedPtr<FJsonObject>* Paths = nullptr;
	if (Root->TryGetObjectField(TEXT("paths"), Paths))
	{
		PS->GetPaths()->FromJson(*Paths);
		UE_LOG(LogWildreach, Log, TEXT("LOAD %s: main=%s(%d) sub=%s(%d)"), *PS->GetPlayerName(),
			*PS->GetPaths()->GetMainPath().ToString(), PS->GetPaths()->GetSpent(PS->GetPaths()->GetMainPath()),
			*PS->GetPaths()->GetSubPath().ToString(), PS->GetPaths()->GetSpent(PS->GetPaths()->GetSubPath()));
	}
	// The bed you wake at by default: the world's beds remember who set them, so it's found again among them.
	const TSharedPtr<FJsonObject>* Bed = nullptr;
	if (Root->TryGetObjectField(TEXT("bed"), Bed))
	{
		WRBeds::LoadDefault(PS, *Bed);
	}
}

void AWRGameMode::SaveAllPlayers(bool bOnlyDirty)
{
	if (!GameState)
	{
		return;
	}
	for (APlayerState* Base : GameState->PlayerArray)
	{
		AWRPlayerState* PS = Cast<AWRPlayerState>(Base);
		// Inventory changes often; saving every player each autosave is cheap at this scale.
		if (PS && (!bOnlyDirty || PS->GetPaths()->IsDirty() || PS->GetInventory()))
		{
			SavePlayer(PS);
		}
	}
}

void AWRGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	CountPlayTime();
	FWRClans::Get().RefreshPlayerStates(GetWorld());
	AWRPlayerState* PS = NewPlayer ? NewPlayer->GetPlayerState<AWRPlayerState>() : nullptr;
	WRBeds::Refresh(PS);
	UE_LOG(LogWildreach, Log, TEXT("Player joined: %s main=%s (%d connected)"),
		PS ? *PS->GetPlayerName() : TEXT("?"), PS ? *PS->GetPaths()->GetMainPath().ToString() : TEXT("?"), GetNumPlayers());
}

void AWRGameMode::Logout(AController* Exiting)
{
	UE_LOG(LogWildreach, Log, TEXT("Player left: %s"), *GetNameSafe(Exiting));
	SavePlayer(Exiting ? Exiting->GetPlayerState<AWRPlayerState>() : nullptr);
	Super::Logout(Exiting);
	CountPlayTime(Exiting);
}

AActor* AWRGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	// A bed or bedroll you've set wins (on top of it: its origin is on the floor); otherwise spawn in the main path's
	// biome (player starts are tagged "volcanic" / "tundra").
	const AWRPlayerState* PS = Player ? Player->GetPlayerState<AWRPlayerState>() : nullptr;
	if (PS && PS->RespawnPoint.IsValid())
	{
		const ACharacter* Body = DefaultPawnClass ? Cast<ACharacter>(DefaultPawnClass->GetDefaultObject()) : nullptr;
		const float HalfHeight = Body && Body->GetCapsuleComponent() ? Body->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f;
		if (AActor* Spot = WRBeds::SpotAbove(PS->RespawnPoint.Get(), HalfHeight, 30.f))
		{
			return Spot;
		}
		return PS->RespawnPoint.Get();
	}
	const FWRPathDef* Def = PS ? FWRPathRegistry::Get().Find(PS->GetPaths()->GetMainPath()) : nullptr;
	if (Def)
	{
		TArray<APlayerStart*> Matching;
		for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
		{
			if (It->PlayerStartTag == Def->Biome)
			{
				Matching.Add(*It);
			}
		}
		if (Matching.Num() > 0)
		{
			return Matching[FMath::RandRange(0, Matching.Num() - 1)];
		}
	}
	return Super::ChoosePlayerStart_Implementation(Player);
}

void AWRGameMode::RequestRespawn(AController* Controller)
{
	if (!Controller)
	{
		return;
	}
	const float Wait = WRTune(TEXT("player.respawnSeconds"), 10.f);
	AWRPlayerState* PS = Controller->GetPlayerState<AWRPlayerState>();
	if (PS)
	{
		PS->WakeReadyAt = GetWorld()->GetTimeSeconds() + Wait;
	}
	// Without a player state of ours there's no death screen to press Wake on: back after the wait.
	const float Auto = PS ? WRTune(TEXT("player.autoWakeSeconds"), 0.f) : Wait;
	if (Auto > 0.f)
	{
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, WeakController = TWeakObjectPtr<AController>(Controller)]()
		{
			if (AController* C = WeakController.Get()) Wake(C);
		}), FMath::Max(Auto, Wait), false);
	}
}

bool AWRGameMode::Wake(AController* Controller)
{
	if (!Controller)
	{
		return false;
	}
	AWRPlayerState* PS = Controller->GetPlayerState<AWRPlayerState>();
	if (PS)
	{
		// A second's grace: the death screen counts on the client's clock, and the world's time runs a little behind
		// real time whenever the server hitches (a long frame is clamped), so a press right at zero came in "early".
		if (PS->WakeReadyAt < 0.0 || GetWorld()->GetTimeSeconds() < PS->WakeReadyAt - 1.0) return false;
		PS->WakeReadyAt = -1.0;
		// Stand on the pick afresh: the spot above it lives only a while, and the death screen can be open longer.
		if (PS->WakeActors.IsValidIndex(PS->WakePick)) WRBeds::Choose(Controller, PS, PS->WakePick);
	}
	RestartPlayer(Controller);
	// The bed they woke at rests a while; a bedroll is used up.
	WRBeds::OnWoke(PS);
	return true;
}
