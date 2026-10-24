#include "WRGameMode.h"
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
#include "WRTuning.h"
#include "Dom/JsonObject.h"
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

AWRGameMode::AWRGameMode()
{
	DefaultPawnClass = AWRCharacter::StaticClass();
	HUDClass = AWRHUD::StaticClass();
	PlayerStateClass = AWRPlayerState::StaticClass();
	PlayerControllerClass = AWRPlayerController::StaticClass();
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
		// A returning player's saved paths override the join URL.
		LoadPlayer(PS);
	}
	return Result;
}

void AWRGameMode::BeginPlay()
{
	Super::BeginPlay();
	LoadWorld();
	GetWorldTimerManager().SetTimer(AutosaveTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { SaveAllPlayers(true); }), 10.f, true);
	GetWorldTimerManager().SetTimer(WorldSaveTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { SaveWorld(); }), 60.f, true);
}

void AWRGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
	SaveAllPlayers(false);
	SaveWorld();
	Super::EndPlay(Reason);
}

FString AWRGameMode::WorldSavePath() const
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ServerDB"), FString::Printf(TEXT("world_%s.json"), *GetWorld()->GetMapName().Replace(TEXT("UEDPIE_0_"), TEXT(""))));
}

void AWRGameMode::SaveWorld()
{
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
			const FVector L = It->GetActorLocation();
			O->SetNumberField(TEXT("x"), L.X); O->SetNumberField(TEXT("y"), L.Y); O->SetNumberField(TEXT("z"), L.Z - It->GetSimpleCollisionHalfHeight());
			Tames.Add(MakeShared<FJsonValueObject>(O));
		}
	}
	Root->SetArrayField(TEXT("tames"), Tames);
	FString Text;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
	FJsonSerializer::Serialize(Root, Writer);
	if (FFileHelper::SaveStringToFile(Text, *WorldSavePath()))
	{
		UE_LOG(LogWildreach, Log, TEXT("WORLD saved %d pieces"), Pieces.Num());
	}
}

void AWRGameMode::LoadWorld()
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *WorldSavePath()))
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
				++LoadedTames;
			}
		}
	}
	UE_LOG(LogWildreach, Log, TEXT("WORLD loaded %d pieces, %d tames"), Loaded, LoadedTames);
}

FString AWRGameMode::PlayerSavePath(const FString& PlayerName)
{
	FString Safe;
	for (const TCHAR C : PlayerName)
	{
		Safe.AppendChar(FChar::IsAlnum(C) || C == TEXT('_') || C == TEXT('-') ? C : TEXT('_'));
	}
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ServerDB"), TEXT("players"), Safe + TEXT(".json"));
}

void AWRGameMode::SavePlayer(AWRPlayerState* PS)
{
	if (!PS || PS->GetPlayerName().IsEmpty())
	{
		return;
	}
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("version"), 1);
	Root->SetStringField(TEXT("name"), PS->GetPlayerName());
	Root->SetObjectField(TEXT("paths"), PS->GetPaths()->ToJson());
	Root->SetObjectField(TEXT("inventory"), PS->GetInventory()->ToJson());
	Root->SetObjectField(TEXT("lifeSkills"), PS->GetLifeSkills()->ToJson());

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
	if (!PS || !FFileHelper::LoadFileToString(Text, *PlayerSavePath(PS->GetPlayerName())))
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
	const AWRPlayerState* PS = NewPlayer ? NewPlayer->GetPlayerState<AWRPlayerState>() : nullptr;
	UE_LOG(LogWildreach, Log, TEXT("Player joined: %s main=%s (%d connected)"),
		PS ? *PS->GetPlayerName() : TEXT("?"), PS ? *PS->GetPaths()->GetMainPath().ToString() : TEXT("?"), GetNumPlayers());
}

void AWRGameMode::Logout(AController* Exiting)
{
	UE_LOG(LogWildreach, Log, TEXT("Player left: %s"), *GetNameSafe(Exiting));
	SavePlayer(Exiting ? Exiting->GetPlayerState<AWRPlayerState>() : nullptr);
	Super::Logout(Exiting);
}

AActor* AWRGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	// A bed or bedroll you've set wins; otherwise spawn in the main path's biome
	// (player starts are tagged "volcanic" / "tundra").
	const AWRPlayerState* PS = Player ? Player->GetPlayerState<AWRPlayerState>() : nullptr;
	if (PS && PS->RespawnPoint.IsValid())
	{
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

	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, WeakController = TWeakObjectPtr<AController>(Controller)]()
	{
		if (AController* C = WeakController.Get())
		{
			RestartPlayer(C);
		}
	}), WRTune(TEXT("player.respawnSeconds"), 10.f), false);
}
