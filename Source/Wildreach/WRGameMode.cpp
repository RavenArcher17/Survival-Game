#include "WRGameMode.h"
#include "WRStronghold.h"
#include "WRSiege.h"
#include "WRMapReveal.h"
#include "WRSporePath.h"
#include "WRNecroPath.h"
#include "WRLand.h"
#include "WRStore.h"
#include "WRIdentity.h"
#include "WRAccess.h"
#include "GameFramework/GameSession.h"
#include "WRGod.h"
#include "WRRaiding.h"
#include "WRArtifacts.h"
#include "WRDeeds.h"
#include "WRWorldSystems.h"
#include "WRTaming.h"
#include "WRFarm.h"
#include "WRBeds.h"
#include "WRSurvival.h"
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
#include "Engine/LocalPlayer.h"
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
	// The world store (SQLite, Saved/Worlds/<id>/world.db): opened before anything loads; brings in 0.7's JSON saves once.
	WRStore::Open(GetWorld());
	FWRClans::Get().Load();
	// Offline raid protection counts owners' leaving times per world (cooks/cookpass-multiplayer.md 3.2).
	FWRSocialRules::ResetSession();
	// Who may come in (cooks/cookpass-multiplayer.md 3.4): access.json's cap, password, admins and bans. The engine's own
	// cap is the same number (PreLogin refuses first, with the reason).
	WRAccess::LoadCurrent();
	// The cap (cooks/build-wild-100players.md 3.1): -MaxPlayers= / ?MaxPlayers= for this run, else access.json's, else
	// 100 on a dedicated server and 8 on a listen world; the engine's own cap leaves room for the admins' slots.
	WRAccess::ReadCapOverride(Options);
	WRAccess::Publish(GetWorld());
	UE_LOG(LogWildreach, Log, TEXT("WORLD %s"), Id.IsEmpty() ? TEXT("none (the menu's island: nothing is loaded or saved)") : *FString::Printf(TEXT("'%s' (Saved/Worlds/%s), preset %s"), *Id, *Id, *FWRTuning::Get().GetPreset()));
}

namespace WRGameModeIdentity
{
	/** The online subsystem's account behind a join ("" when there's none: the Null subsystem's ids change every launch). */
	void Platform(const FUniqueNetIdRepl& UniqueId, FString& OutType, FString& OutId)
	{
		OutType.Reset();
		OutId.Reset();
		if (!UniqueId.IsValid()) return;
		const FName Type = UniqueId.GetType();
		if (Type == NAME_None || Type == FName(TEXT("NULL"))) return;
		OutType = Type.ToString();
		OutId = UniqueId->ToString();
	}
}

void AWRGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	// Banned, full, password (WRAccess), before the engine's own checks so the joiner reads why.
	if (!WRWorlds::Current().IsEmpty())
	{
		ErrorMessage = WRAccess::Refuse(UGameplayStatics::ParseOption(Options, TEXT("WRName")).Left(24), UGameplayStatics::ParseOption(Options, TEXT("WRPass")).Left(64), Address, GetNumPlayers());
		if (!ErrorMessage.IsEmpty())
		{
			return;
		}
	}
	const FString Name = UGameplayStatics::ParseOption(Options, TEXT("WRName")).Left(24);
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (ErrorMessage.IsEmpty())
	{
		FString Type, Id;
		WRGameModeIdentity::Platform(UniqueId, Type, Id);
		ErrorMessage = WRIdentity::Check(Name, UGameplayStatics::ParseOption(Options, TEXT("WRKey")).Left(64), Type, Id);
	}
	// Turned away after all: the slot WRAccess held for them goes back.
	if (!ErrorMessage.IsEmpty())
	{
		WRAccess::Arrived(Name);
	}
}

APlayerController* AWRGameMode::Login(UPlayer* NewPlayer, ENetRole InRemoteRole, const FString& Portal, const FString& Options, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	const FString Name = UGameplayStatics::ParseOption(Options, TEXT("WRName")).Left(24);
	// A joiner (not a listen host's own player) whose held slot lapsed while they loaded, into a world that filled
	// meanwhile, goes back to the queue rather than past the cap (cooks/build-wild-100players.md 3.6).
	if (NewPlayer && !NewPlayer->IsA<ULocalPlayer>())
	{
		ErrorMessage = WRAccess::CheckArrival(Name, GetNumPlayers());
		if (!ErrorMessage.IsEmpty())
		{
			return nullptr;
		}
	}
	APlayerController* PC = Super::Login(NewPlayer, InRemoteRole, Portal, Options, UniqueId, ErrorMessage);
	if (PC)
	{
		WRAccess::Arrived(Name);
	}
	return PC;
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
		// The name is this PC's now (or still): its account in the world store (WRIdentity; PreLogin checked it).
		{
			FString Type, Id;
			WRGameModeIdentity::Platform(UniqueId, Type, Id);
			WRIdentity::Bind(PS->GetPlayerName(), UGameplayStatics::ParseOption(Options, TEXT("WRKey")).Left(64), Type, Id, NewPlayerController->GetPlayerNetworkAddress());
		}
		// The key a tester's proof must be bound to (WRGod: the proof's bind is a hash of it).
		if (UWRGodComponent* God = PS->GetGod()) God->JoinKey = UGameplayStatics::ParseOption(Options, TEXT("WRKey")).Left(64);
		PS->GetPaths()->SetMainPath(FName(*Path));
		const FString Body = UGameplayStatics::ParseOption(Options, TEXT("Body")).ToLower().Left(8);
		PS->BodyStyle = Body.IsEmpty() ? NAME_None : FName(*Body);
		// ?Look=<face>.<build>[.h] (cook hero-look-2): the pawn clamps them to the faces and builds the art has.
		FString LookOpt = UGameplayStatics::ParseOption(Options, TEXT("Look")).Left(12), FaceStr, Rest;
		if (LookOpt.Split(TEXT("."), &FaceStr, &Rest))
		{
			FString BuildStr, Flags;
			if (!Rest.Split(TEXT("."), &BuildStr, &Flags))
			{
				BuildStr = Rest;
			}
			PS->HeroFace = uint8(FMath::Clamp(FCString::Atoi(*FaceStr), 0, 255));
			PS->HeroBuild = uint8(FMath::Clamp(FCString::Atoi(*BuildStr), 0, 255));
			PS->bHideHelmet = Flags.Contains(TEXT("h"));
		}
		// A returning player's saved paths override the join URL. (No save yet: new to this world, "washed ashore".)
		{
			FString Existing;
			PS->bNewToWorld = !WRWorlds::Current().IsEmpty() && !WRStore::ReadCharacter(PS->GetPlayerName(), Existing);
		}
		LoadPlayer(PS);
		if (UWRWorldSystems* WorldSys = UWRWorldSystems::Get(this)) WorldSys->PlayerJoined(PS);
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
	{
		FString BagsText;
		TSharedPtr<FJsonObject> Bags;
		if (!WRWorlds::Current().IsEmpty() && WRStore::ReadDoc(TEXT("bags"), BagsText) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(BagsText), Bags))
		{
			AWRLootBag::LoadAll(GetWorld(), Bags);
		}
	}
	FWRRaiding::Load(GetWorld());
	FWRSiege::Load(GetWorld());
	UWRStrongholdSubsystem::Load(GetWorld());
	WRWildlife::Start(GetWorld());
	WRRaids::Start(GetWorld());
	// The endgame: achievements' census, the kill-time boards and their tablets, the war-halls (WRDeeds, WRRecords, WRWarHall).
	WRDeeds::Start(GetWorld());
	if (!WRWorlds::Current().IsEmpty())
	{
		const float Every = FMath::Max(0.1f, WRTune(TEXT("wildlife.stepSeconds"), 20.f));
		GetWorldTimerManager().SetTimer(WildlifeTimer, FTimerDelegate::CreateWeakLambda(this, [this, Every]() { WRWildlife::Step(GetWorld(), Every); }), Every, true);
		GetWorldTimerManager().SetTimer(RaidTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { WRRaids::Step(GetWorld()); }),
			FMath::Max(0.25f, WRTune(TEXT("raid.stepSeconds"), 2.f)), true);
	}
	// Saves spread out (cooks/build-wild-100players.md 3.8): a few players every second, the world in slices.
	GetWorldTimerManager().SetTimer(AutosaveTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { AutosaveStep(); }), 1.f, true);
	GetWorldTimerManager().SetTimer(WorldSaveTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { BeginSlicedWorldSave(); WRStore::Tick(); }), 60.f, true);
	// The cap and the queue on the game state (the players list), and the engine's cap, kept current.
	WRAccess::Publish(GetWorld());
	GetWorldTimerManager().SetTimer(AccessTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { WRAccess::Publish(GetWorld()); }), 2.f, true);
	// Player raiding: code breakers at work, the raid record kept (WRRaiding).
	GetWorldTimerManager().SetTimer(RaidingTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { FWRRaiding::Step(GetWorld()); }), 1.f, true);
	GetWorldTimerManager().SetTimer(UndergroundTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { RescueUnderground(); }), 1.f, true);
	GetWorldTimerManager().SetTimer(RoamingBossTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { StepRoamingBosses(); }), 5.f, true);
}

void AWRGameMode::StepRoamingBosses()
{
	// The roaming minis (cooks/build-combat-full.md 2.17-2.18: Cinderhorn, Old Tusker) aren't placed in the map: each
	// walks its loop (creatures.json roam). One comes out when a survivor is near its loop and the ground there has
	// streamed in, and comes back server.bossRespawnHours after it falls.
	const double Now = GetWorld()->GetTimeSeconds();
	for (const FWRCreatureDef& Def : FWRCreatureRegistry::Get().All())
	{
		if (Def.Roam.Num() == 0 || Def.Archetype != TEXT("boss"))
		{
			continue;
		}
		// (On the round island the Old Island's loops would put Old Tusker in Hearthmoot's meadow: only a loop of this
		// map's own counts there.)
		if (WRLand::OwnLandMap() && !FWRCreatureRegistry::Get().HasMapRoam(Def.Id))
		{
			continue;
		}
		bool bAlive = false;
		for (TActorIterator<AWRCreature> It(GetWorld()); It && !bAlive; ++It)
		{
			bAlive = It->CreatureId == Def.Id && It->IsAlive();
		}
		double& Back = RoamingBossBack.FindOrAdd(Def.Id);
		if (bAlive)
		{
			Back = -1.0;
			continue;
		}
		if (Back < 0.0)
		{
			// It just fell (or the world just started: then at once).
			Back = RoamingBossSeen.Contains(Def.Id) ? Now + WRTune(TEXT("server.bossRespawnHours"), 4.f) * 3600.0 : Now;
		}
		if (Now < Back)
		{
			continue;
		}
		const FVector Start = Def.Roam[0];
		bool bNear = false;
		for (TActorIterator<AWRCharacter> It(GetWorld()); It && !bNear; ++It)
		{
			bNear = It->IsAlive() && It->GetPlayerState() && FVector::Dist2D(It->GetActorLocation(), Start) < WRTune(TEXT("wildlife.activationRadius"), 15000.f);
		}
		FHitResult Ground;
		const FCollisionQueryParams Q(SCENE_QUERY_STAT(WRRoamBoss), false);
		if (bNear && WRLand::OwnLandMap())
		{
			Ground.ImpactPoint = FVector(Start.X, Start.Y, WRLand::GroundZAt(Start, Start.Z));
		}
		else if (!bNear || !GetWorld()->LineTraceSingleByChannel(Ground, Start + FVector(0, 0, 80000), Start - FVector(0, 0, 80000), ECC_WorldStatic, Q))
		{
			continue;
		}
		if (AWRCreature* Boss = AWRCreature::SpawnCreature(GetWorld(), Def.Id, Ground.ImpactPoint + FVector(0.f, 0.f, Def.HalfHeight + 30.f), FRotator::ZeroRotator))
		{
			RoamingBossSeen.Add(Def.Id);
			Back = -1.0;
			UE_LOG(LogWildreach, Log, TEXT("BOSS %s comes out on its loop at %s"), *Def.Name, *Boss->GetActorLocation().ToCompactString());
		}
	}
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
	const float TraceTop = WRTune(TEXT("water.rescueTraceTop"), 150000.f);
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
		// Inside a land's hole (the Great Rift) the black is the floor: no rescue, the land's own rule ends the fall.
		if (WRLand::InHole(L)) continue;
		const float Need = UWRWaterSubsystem::IsInWater(C) ? SwimMargin : Margin;
		TArray<FHitResult> Hits;
		const FCollisionQueryParams Query(SCENE_QUERY_STAT(WRUnderground), false, C);
		GetWorld()->LineTraceMultiByObjectType(Hits, FVector(L.X, L.Y, FMath::Max(L.Z + 80000.f, TraceTop)), L, FCollisionObjectQueryParams(ECC_WorldStatic), Query);
		bool bRescued = false;
		for (const FHitResult& Hit : Hits)
		{
			if (Cast<ULandscapeHeightfieldCollisionComponent>(Hit.GetComponent()) && Hit.ImpactPoint.Z > L.Z + Need)
			{
				// Under the ground on purpose: a cave's chamber and tunnel (AWRCave: the Swamp's Gullet is dug in under the
				// Storm peaks' cliff), a fold, a war-hall. Its creatures belong there: "rescued", the Gullet's were lifted
				// 200-640 m onto the cliff top. Only asked when the ground is over them, so it costs nothing otherwise.
				if (WRBiome::IsUnderground(this, L))
				{
					bRescued = true;   // not lost: nothing to do
					break;
				}
				const float Half = C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
				C->TeleportTo(FVector(L.X, L.Y, Hit.ImpactPoint.Z + Half + 30.f), C->GetActorRotation(), false, true);
				C->GetCharacterMovement()->Velocity = FVector::ZeroVector;
				UE_LOG(LogWildreach, Warning, TEXT("RESCUE %s was %.0f m under the ground at %s%s: put back on top"),
					*C->GetName(), (Hit.ImpactPoint.Z - L.Z) / 100.f, *L.ToString(), Need < Margin ? TEXT(" (in the water)") : TEXT(""));
				bRescued = true;
				break;
			}
		}
		// A runtime land's ground (WRLand: procedural-mesh chunks, not a landscape, so the trace above can't tell): its
		// height is a function, so anyone well under it fell through (a chunk whose collision was still cooking). The
		// places a body rightly stands under it (the forest's Hollow Root, its squeezes) were skipped by InHole above.
		float LandZ = 0.f;
		if (!bRescued && WRLand::GroundUnder(L, LandZ) && LandZ > L.Z + Need)
		{
			const float Half = C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			C->TeleportTo(FVector(L.X, L.Y, LandZ + Half + 30.f), C->GetActorRotation(), false, true);
			C->GetCharacterMovement()->Velocity = FVector::ZeroVector;
			UE_LOG(LogWildreach, Warning, TEXT("RESCUE %s was %.0f m under a land's ground at %s: put back on top"), *C->GetName(), (LandZ - L.Z) / 100.f, *L.ToString());
			bRescued = true;
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
	WRStore::Flush(false);   // the store closes once the world's subsystems have saved too (WRStore::Open)
	Super::EndPlay(Reason);
}

FString AWRGameMode::WorldSavePath() const
{
	const FString Dir = WRWorlds::CurrentDir();
	return Dir.IsEmpty() ? FString() : FPaths::Combine(Dir, TEXT("world.json"));
}

bool AWRGameMode::ShouldSavePiece(const AWRBuildPiece* Piece)
{
	// (A stronghold's pieces are its own: they're rebuilt from Data/strongholds.json and its "strongholds" document.)
	return Piece && !Piece->bGhost && !Piece->IsActorBeingDestroyed() && !FWRStrongholds::IsHoldPiece(Piece);
}

void AWRGameMode::SaveWorld()
{
	if (WorldSavePath().IsEmpty())
	{
		return;
	}
	// Whole, now: a sliced save in flight is dropped (this one has everything it would have).
	bSliceSaving = false;
	SliceList.Reset();
	SlicePieces.Reset();
	TArray<TSharedPtr<FJsonValue>> Pieces;
	for (TActorIterator<AWRBuildPiece> It(GetWorld()); It; ++It)
	{
		if (ShouldSavePiece(*It))
		{
			Pieces.Add(MakeShared<FJsonValueObject>(It->ToJson()));
		}
	}
	FinishWorldSave(MoveTemp(Pieces));
}

void AWRGameMode::BeginSlicedWorldSave()
{
	if (WorldSavePath().IsEmpty() || bSliceSaving)
	{
		return;
	}
	bSliceSaving = true;
	SliceCursor = 0;
	SliceList.Reset();
	SlicePieces.Reset();
	for (TActorIterator<AWRBuildPiece> It(GetWorld()); It; ++It)
	{
		SliceList.Add(*It);
	}
	SlicePieces.Reserve(SliceList.Num());
	StepSlicedWorldSave();
}

void AWRGameMode::StepSlicedWorldSave()
{
	if (!bSliceSaving)
	{
		return;
	}
	// A piece gone since the list was taken is skipped; one placed since goes into the next save.
	const int32 PerFrame = FMath::Max(50, FMath::RoundToInt(WRTune(TEXT("net.save.piecesPerFrame"), 1500.f)));
	int32 Done = 0;
	while (SliceCursor < SliceList.Num() && Done < PerFrame)
	{
		const AWRBuildPiece* Piece = SliceList[SliceCursor++].Get();
		if (ShouldSavePiece(Piece))
		{
			SlicePieces.Add(MakeShared<FJsonValueObject>(Piece->ToJson()));
			++Done;
		}
	}
	if (SliceCursor < SliceList.Num())
	{
		GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]() { StepSlicedWorldSave(); }));
		return;
	}
	bSliceSaving = false;
	SliceList.Reset();
	FinishWorldSave(MoveTemp(SlicePieces));
	SlicePieces.Reset();
}

void AWRGameMode::FinishWorldSave(TArray<TSharedPtr<FJsonValue>>&& Pieces)
{
	CountPlayTime();
	// A death's belongings wait through a clean restart (cooks/cookpass-multiplayer.md 3.5).
	AWRLootBag::SaveNow(GetWorld(), true);
	const int32 NumPieces = Pieces.Num();
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("version"), 1);
	Root->SetArrayField(TEXT("pieces"), Pieces);
	TArray<TSharedPtr<FJsonValue>> Tames;
	for (TActorIterator<AWRCreature> It(GetWorld()); It; ++It)
	{
		if (It->IsTamed() && It->IsAlive() && !WRNecro::IsRaised(*It) && !WRSporePath::IsSummon(*It))   // a raised minion is never saved
		{
			const TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
			O->SetStringField(TEXT("id"), It->CreatureId.ToString());
			O->SetStringField(TEXT("owner"), It->TamedBy);
			O->SetBoolField(TEXT("stay"), It->TameOrder == EWRTameOrder::Stay);
			O->SetBoolField(TEXT("aggressive"), It->bTameAggressive);
			O->SetNumberField(TEXT("bonus"), It->TameBonus);
			// Its genes, bond, tree, tack, packs, growth, care and breeding (cooks/build-taming.md 2.9).
			for (const TPair<FString, TSharedPtr<FJsonValue>>& F : FWRTaming::SaveTame(*It)->Values) O->SetField(F.Key, F.Value);
			const FVector L = It->GetActorLocation();
			O->SetNumberField(TEXT("x"), L.X); O->SetNumberField(TEXT("y"), L.Y); O->SetNumberField(TEXT("z"), L.Z - It->GetSimpleCollisionHalfHeight());
			Tames.Add(MakeShared<FJsonValueObject>(O));
		}
	}
	Root->SetArrayField(TEXT("tames"), Tames);
	// The fallen ledger: dead tames whose collars can still be brought back (cooks/cookpass-taming.md 3.10).
	if (const UWRTamingSubsystem* TameSub = UWRTamingSubsystem::Get(GetWorld())) Root->SetObjectField(TEXT("fallenTames"), TameSub->FallenToJson());
	// Eggs your tames laid that nobody has picked up yet (cooks/build-taming.md 2.6).
	TArray<TSharedPtr<FJsonValue>> Eggs;
	for (TActorIterator<AWRLaidEgg> It(GetWorld()); It; ++It)
	{
		if (It->Egg.IsEmpty()) continue;
		const TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		const FVector L = It->GetActorLocation();
		O->SetNumberField(TEXT("x"), L.X); O->SetNumberField(TEXT("y"), L.Y); O->SetNumberField(TEXT("z"), L.Z);
		O->SetStringField(TEXT("owner"), It->OwnerName);
		O->SetObjectField(TEXT("egg"), FWRInventoryRules::StackToJson(It->Egg));
		O->SetStringField(TEXT("shell"), It->Shell.ToFColor(true).ToHex());
		Eggs.Add(MakeShared<FJsonValueObject>(O));
	}
	Root->SetArrayField(TEXT("laidEggs"), Eggs);
	if (const AWRGameState* State = GetGameState<AWRGameState>())
	{
		Root->SetNumberField(TEXT("clockHours"), State->GetTotalHours());  // the day and time carry over a restart
	}
	FString Text;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text);
	FJsonSerializer::Serialize(Root, Writer);
	WRStore::WriteDoc(TEXT("world"), Text);
	FWRRaiding::Save();
	FWRSiege::Save(GetWorld());
	UWRStrongholdSubsystem::Save(GetWorld());
	UE_LOG(LogWildreach, Log, TEXT("WORLD saved %d pieces"), NumPieces);
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
	if (WorldSavePath().IsEmpty() || !WRStore::ReadDoc(TEXT("world"), Text))
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
		double Pitch = 0.0;   // beams and braces slope (build-building)
		O->TryGetNumberField(TEXT("pitch"), Pitch);
		const FTransform T(FRotator(Pitch, O->GetNumberField(TEXT("yaw")), 0.f),
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
				Tame->CompleteTame(O->GetStringField(TEXT("owner")), false);
				Tame->TameOrder = O->GetBoolField(TEXT("stay")) ? EWRTameOrder::Stay : EWRTameOrder::Follow;
				Tame->bTameAggressive = O->GetBoolField(TEXT("aggressive"));
				Tame->ApplyTameBonus(O->HasField(TEXT("bonus")) ? O->GetNumberField(TEXT("bonus")) : 0.f);  // older saves: none
				FWRTaming::LoadTame(Tame, O);
				++LoadedTames;
			}
		}
	}
	const TSharedPtr<FJsonObject>* FallenObj = nullptr;
	if (UWRTamingSubsystem* TameSub = UWRTamingSubsystem::Get(GetWorld()); TameSub && Root->TryGetObjectField(TEXT("fallenTames"), FallenObj)) TameSub->FallenFromJson(*FallenObj);
	const TArray<TSharedPtr<FJsonValue>>* Eggs = nullptr;
	if (Root->TryGetArrayField(TEXT("laidEggs"), Eggs))
	{
		for (const TSharedPtr<FJsonValue>& V : *Eggs)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			const TSharedPtr<FJsonObject>* EggObj = nullptr;
			if (!O.IsValid() || !O->TryGetObjectField(TEXT("egg"), EggObj)) continue;
			const FWRItemStack Egg = FWRInventoryRules::StackFromJson(*EggObj);
			if (Egg.IsEmpty()) continue;
			AWRLaidEgg::Lay(GetWorld(), FVector(O->GetNumberField(TEXT("x")), O->GetNumberField(TEXT("y")), O->GetNumberField(TEXT("z"))), Egg, O->GetStringField(TEXT("owner")),
				FLinearColor(FColor::FromHex(O->GetStringField(TEXT("shell")))));
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
	Root->SetObjectField(TEXT("survival"), PS->GetSurvival()->ToJson());
	Root->SetObjectField(TEXT("food"), PS->GetFood()->ToJson());
	if (const TSharedPtr<FJsonObject> Bed = WRBeds::SaveDefault(PS))
	{
		Root->SetObjectField(TEXT("bed"), Bed);
	}
	// Named bosses this player has felled (the first-kill drop is once each: cooks/build-combat-full.md 2.16).
	TArray<TSharedPtr<FJsonValue>> Kills;
	for (const FName& Boss : PS->BossKills) Kills.Add(MakeShared<FJsonValueString>(Boss.ToString()));
	Root->SetArrayField(TEXT("bossKills"), Kills);
	TArray<TSharedPtr<FJsonValue>> Known;
	for (const FName& Bp : PS->KnownBlueprints) Known.Add(MakeShared<FJsonValueString>(Bp.ToString()));
	Root->SetArrayField(TEXT("blueprints"), Known);
	// Quests with the path masters, shrines you kindled, faction standing (WRWorldSystems).
	if (const UWRQuestLog* Log = UWRQuestLog::For(PS)) Root->SetObjectField(TEXT("world"), Log->ToJson());
	// What they've seen of the map (the fill-in map, WRMapReveal).
	if (const UWRMapRevealComponent* Reveal = UWRMapRevealComponent::For(PS)) Root->SetObjectField(TEXT("mapReveal"), Reveal->ToJson());
	// Artifacts: hooks, pouch, geis records, titles (WRArtifacts).
	if (const UWRArtifactComponent* Art = UWRArtifactComponent::For(PS)) Root->SetObjectField(TEXT("artifacts"), Art->ToJson());
	// Deeds: prestige marks and layouts, souls forged, achievements, titles, cosmetics (WRDeeds).
	if (UWRDeedsComponent* Deeds = UWRDeedsComponent::For(PS)) { Root->SetObjectField(TEXT("deeds"), Deeds->ToJson()); Deeds->bDirty = false; }

	FString Text;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text);
	FJsonSerializer::Serialize(Root, Writer);
	// Into the world store (its writer commits within store.flushSeconds; a failed commit stays pending, never lost).
	WRStore::WriteCharacter(PS->GetPlayerName(), PS->GetPaths()->GetMainPath().ToString(), Text);
	PS->GetPaths()->ClearDirty();
	UE_LOG(LogWildreach, Log, TEXT("SAVE %s (%d chars)"), *PS->GetPlayerName(), Text.Len());
}

void AWRGameMode::LoadPlayer(AWRPlayerState* PS)
{
	FString Text;
	if (!PS || PlayerSavePath(PS->GetPlayerName()).IsEmpty() || !WRStore::ReadCharacter(PS->GetPlayerName(), Text))
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
	// Survival first: whether the saved light was lit decides how the inventory's Light slot comes back.
	const TSharedPtr<FJsonObject>* Survival = nullptr;
	if (Root->TryGetObjectField(TEXT("survival"), Survival))
	{
		PS->GetSurvival()->FromJson(*Survival);
	}
	const TSharedPtr<FJsonObject>* FoodJson = nullptr;
	if (Root->TryGetObjectField(TEXT("food"), FoodJson))
	{
		PS->GetFood()->FromJson(*FoodJson);
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
	const TArray<TSharedPtr<FJsonValue>>* Kills = nullptr;
	if (Root->TryGetArrayField(TEXT("bossKills"), Kills))
	{
		for (const TSharedPtr<FJsonValue>& K : *Kills) PS->BossKills.Add(FName(*K->AsString()));
	}
	const TArray<TSharedPtr<FJsonValue>>* Known = nullptr;
	if (Root->TryGetArrayField(TEXT("blueprints"), Known))
	{
		for (const TSharedPtr<FJsonValue>& K : *Known) PS->KnownBlueprints.AddUnique(FName(*K->AsString()));
	}
	const TSharedPtr<FJsonObject>* WorldJson = nullptr;
	if (Root->TryGetObjectField(TEXT("world"), WorldJson))
	{
		if (UWRQuestLog* Log = UWRQuestLog::For(PS)) Log->FromJson(*WorldJson);
	}
	const TSharedPtr<FJsonObject>* RevealJson = nullptr;
	if (Root->TryGetObjectField(TEXT("mapReveal"), RevealJson))
	{
		if (UWRMapRevealComponent* Reveal = UWRMapRevealComponent::For(PS)) Reveal->FromJson(*RevealJson);
	}
	const TSharedPtr<FJsonObject>* ArtJson = nullptr;
	if (Root->TryGetObjectField(TEXT("artifacts"), ArtJson))
	{
		if (UWRArtifactComponent* Art = UWRArtifactComponent::For(PS)) Art->FromJson(*ArtJson);
	}
	const TSharedPtr<FJsonObject>* DeedsJson = nullptr;
	if (Root->TryGetObjectField(TEXT("deeds"), DeedsJson))
	{
		if (UWRDeedsComponent* Deeds = UWRDeedsComponent::For(PS)) Deeds->FromJson(*DeedsJson);
	}
}

void AWRGameMode::AutosaveStep()
{
	if (!GameState || GameState->PlayerArray.Num() == 0)
	{
		return;
	}
	// Each player every playerEverySeconds as before, but a share of them each second: at 100 that's 10 a second,
	// not 100 in one frame.
	// Second k saves the players whose place in the list is k mod Every: one player alone is saved every 10 s (with the
	// old ceil(N / Every) a second, anyone under 10 players was saved every second).
	const TArray<TObjectPtr<APlayerState>>& All = GameState->PlayerArray;
	const int32 Every = FMath::Max(1, FMath::RoundToInt(WRTune(TEXT("net.save.playerEverySeconds"), 10.f)));
	AutosaveCursor = (AutosaveCursor + 1) % Every;
	for (int32 i = AutosaveCursor; i < All.Num(); i += Every)
	{
		SavePlayer(Cast<AWRPlayerState>(All[i]));
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

namespace WRGameModeArrivals
{
	/**
	 * "Mira is back / left" (cooks/cookpass-multiplayer.md 3.4): to everyone in a small world; past
	 * net.arrivalsToAllUpTo players only to her clan and the world's admins, or 100 players' comings and goings
	 * would be all the chat there is (cooks/build-wild-100players.md 3.7).
	 */
	void Tell(UWorld* World, const AWRPlayerState* Who, const FString& Line, const AController* Except)
	{
		const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
		if (!GS) return;
		const bool bAll = GS->PlayerArray.Num() <= FMath::RoundToInt(WRTune(TEXT("net.arrivalsToAllUpTo"), 24.f));
		const FString Clan = Who ? Who->ClanName : FString();
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			AWRPlayerController* Other = Cast<AWRPlayerController>(It->Get());
			if (!Other || Other == Except) continue;
			const AWRPlayerState* OtherPS = Other->GetPlayerState<AWRPlayerState>();
			const bool bTheirs = OtherPS && (OtherPS->AccessRole > 0 || (!Clan.IsEmpty() && OtherPS->ClanName == Clan));
			if (bAll || bTheirs) Other->ClientChat(FString(), Line, WRChat::System);
		}
	}
}

void AWRGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	CountPlayTime();
	FWRClans::Get().RefreshPlayerStates(GetWorld());
	AWRPlayerState* PS = NewPlayer ? NewPlayer->GetPlayerState<AWRPlayerState>() : nullptr;
	if (PS) FWRSocialRules::NoteJoined(PS->GetPlayerName());
	WRAccess::RefreshRoles(GetWorld());
	// Everyone else hears who came (cooks/cookpass-multiplayer.md 3.4).
	if (PS && !WRWorlds::Current().IsEmpty())
	{
		const FString Line = PS->bNewToWorld ? FString::Printf(TEXT("%s washed ashore"), *PS->GetPlayerName()) : FString::Printf(TEXT("%s is back"), *PS->GetPlayerName());
		WRGameModeArrivals::Tell(GetWorld(), PS, Line, NewPlayer);
	}
	WRAccess::Publish(GetWorld());
	WRBeds::Refresh(PS);
	FWRRaiding::PlayerJoined(Cast<AWRPlayerController>(NewPlayer));
	// The testers' god mode: this connection's nonce; a tester's game answers with the Freedom35 site's proof (WRGod).
	WRGod::Challenge(Cast<AWRPlayerController>(NewPlayer));
	UE_LOG(LogWildreach, Log, TEXT("Player joined: %s main=%s (%d connected)"),
		PS ? *PS->GetPlayerName() : TEXT("?"), PS ? *PS->GetPaths()->GetMainPath().ToString() : TEXT("?"), GetNumPlayers());
}

void AWRGameMode::Logout(AController* Exiting)
{
	UE_LOG(LogWildreach, Log, TEXT("Player left: %s"), *GetNameSafe(Exiting));
	SavePlayer(Exiting ? Exiting->GetPlayerState<AWRPlayerState>() : nullptr);
	if (const APlayerState* LeftPS = Exiting ? Exiting->PlayerState.Get() : nullptr)
	{
		FWRSocialRules::NoteLeft(LeftPS->GetPlayerName());
		// The others hear it (not while the whole world is closing).
		if (!GetWorld()->bIsTearingDown && !WRWorlds::Current().IsEmpty())
		{
			WRGameModeArrivals::Tell(GetWorld(), Cast<AWRPlayerState>(LeftPS), FString::Printf(TEXT("%s left"), *LeftPS->GetPlayerName()), Exiting);
		}
	}
	WRStore::Flush(false);   // a leaving character is committed now, not at the next batch
	Super::Logout(Exiting);
	CountPlayTime(Exiting);
	if (!GetWorld()->bIsTearingDown) WRAccess::Publish(GetWorld());
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
	// No start of the path's own here (the round island's lands come slice by slice): Hearthmoot's (Data/lands/island.json
	// camps, tag world.cityStartTag).
	const FName City(*FWRTuning::Get().Str(TEXT("world.cityStartTag"), TEXT("hearthmoot")));
	TArray<APlayerStart*> CityStarts;
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		if (It->PlayerStartTag == City) CityStarts.Add(*It);
	}
	if (CityStarts.Num() > 0)
	{
		return CityStarts[FMath::RandRange(0, CityStarts.Num() - 1)];
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
