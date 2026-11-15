// The Graveyard's world (see WRGraveWorld.h; cooks/build-biome-graveyard.md 2.3-2.9).

#include "WRGraveWorld.h"
#include "Wildreach.h"
#include "WRCharacter.h"
#include "WRFarm.h"
#include "WRFx.h"
#include "WRGameState.h"
#include "WRGraveTypes.h"
#include "WRInventory.h"
#include "WRLand.h"
#include "WRPathComponent.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRResourceNode.h"
#include "WRSound.h"
#include "WRSurvival.h"
#include "WRTuning.h"
#include "WRWorldSystems.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

namespace WRGraveWorldImpl
{
	float Tn(const TCHAR* Key, float Def) { return WRTune(Key, Def); }

	AWRPlayerController* ControllerOf(const AActor* Pawn)
	{
		const APawn* P = Cast<APawn>(Pawn);
		return P ? Cast<AWRPlayerController>(P->GetController()) : nullptr;
	}
	void Tell(const AActor* Pawn, const FString& Text)
	{
		if (AWRPlayerController* PC = ControllerOf(Pawn)) PC->ClientNotify(Text);
	}
	double GameHours(const UObject* Ctx)
	{
		const UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
		const AWRGameState* GS = W ? W->GetGameState<AWRGameState>() : nullptr;
		return GS ? GS->GetTotalHours() : 0.0;
	}
	/** A pick in the weapon hand (a pick tool of any grade, not broken, or a pick-hammer weapon). */
	bool HoldsPick(const AActor* Who)
	{
		const UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Who);
		const FWRItemStack* H = Inv ? &Inv->GetEquipped(EWREquipSlot::Weapon) : nullptr;
		if (!H || H->IsEmpty()) return false;
		if (const FWRToolDef* Tool = FWRGathering::FindTool(H->ItemId)) return Tool->Kind == TEXT("pick") && H->Durability > 0.f;
		const FWRItemDef* Def = FWRItemRegistry::Get().Find(H->ItemId);
		return Def && (Def->WeaponClass == TEXT("pick_hammer") || Def->WeaponClass == TEXT("pick"));
	}
	/** A flame in hand: a lit torch or lantern (the Light slot). */
	bool HoldsFlame(const AActor* Who)
	{
		const UWRSurvivalComponent* S = UWRSurvivalComponent::For(Who);
		return S && S->IsLightOn();
	}
	FString ItemName(FName Id)
	{
		const FWRItemDef* D = FWRItemRegistry::Get().Find(Id);
		return D ? D->Name : Id.ToString();
	}
}

// ================================================================================================ AWRGraveState

AWRGraveState::AWRGraveState()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = false;
	SetNetUpdateFrequency(1.f);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRGraveState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRGraveState, StairGoldUntilHours);
	DOREPLIFETIME(AWRGraveState, bChapelCryptOpen);
	DOREPLIFETIME(AWRGraveState, bSeventhOpen);
}

AWRGraveState* AWRGraveState::Get(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World) return nullptr;
	for (TActorIterator<AWRGraveState> It(World); It; ++It) return *It;
	return nullptr;
}

bool AWRGraveState::StairGold() const
{
	return StairGoldUntilHours > 0.f && WRGraveWorldImpl::GameHours(this) < StairGoldUntilHours;
}

void AWRGraveState::OnRep_State()
{
	// The scenery reads the state when it steps its lights (the Stair's gold), and the crypt's stair actor reads
	// bChapelCryptOpen itself; nothing to rebuild here.
}

// ================================================================================================ AWRGraveSpot

AWRGraveSpot::AWRGraveSpot()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.2f;
	SetNetUpdateFrequency(4.f);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRGraveSpot::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRGraveSpot, Kind);
	DOREPLIFETIME(AWRGraveSpot, SpotId);
	DOREPLIFETIME(AWRGraveSpot, Title);
	DOREPLIFETIME(AWRGraveSpot, Verb);
	DOREPLIFETIME(AWRGraveSpot, Seconds);
	DOREPLIFETIME(AWRGraveSpot, DoneAt);
	DOREPLIFETIME(AWRGraveSpot, Doer);
	DOREPLIFETIME(AWRGraveSpot, bLit);
}

void AWRGraveSpot::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() != NM_DedicatedServer) BuildLook();
}

void AWRGraveSpot::BuildLook()
{
	if (bLookBuilt) return;
	bLookBuilt = true;
	// A keepsake or a find glints faintly (the scenery draws the thing itself: the bat on the stones, the tally on the
	// wall); a lantern spot carries its own small flame once lit.
	if (Kind == TEXT("keepsake") || Kind == TEXT("chamber"))
	{
		Glint = WRFx::Attach(GetRootComponent(), TEXT("grave.keepsake"), NAME_None, 1.f);
	}
	if (Kind == TEXT("lantern"))
	{
		Light = NewObject<UPointLightComponent>(this);
		Light->SetupAttachment(GetRootComponent());
		Light->SetRelativeLocation(FVector(0.f, 0.f, 128.f));
		Light->SetLightColor(SpotId == TEXT("S3") ? FLinearColor(FColor(255, 196, 96)) : FLinearColor(FColor(159, 227, 192)));
		Light->SetAttenuationRadius(900.f);
		Light->SetCastShadows(false);
		Light->SetIntensity(bLit ? 1500.f : 0.f);
		Light->RegisterComponent();
	}
}

void AWRGraveSpot::OnRep_Lit()
{
	if (Light) Light->SetIntensity(bLit ? 1500.f : 0.f);
}

FString AWRGraveSpot::Start(AWRPlayerController* PC)
{
	using namespace WRGraveWorldImpl;
	AWRCharacterBase* Me = PC ? Cast<AWRCharacterBase>(PC->GetPawn()) : nullptr;
	AWRPlayerState* PS = PC ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
	if (!Me || !PS) return TEXT("Not now");
	if (FVector::Dist(Me->GetActorLocation(), GetActorLocation()) > Reach + 150.f) return TEXT("Too far");
	const FString Name = PS->GetPlayerName();
	if (DoneAt > 0.f) return Doer == Name ? TEXT("Hold still...") : FString::Printf(TEXT("%s is at it"), *Doer);
	if (Kind == TEXT("lantern") && bLit && SpotId != TEXT("S3")) return TEXT("It's burning");
	if (IsDoneBy(Name) && Kind != TEXT("lantern")) return Kind == TEXT("dig") ? TEXT("You've dug here") : TEXT("Nothing more here");
	if (bNightOnly && !AWRGameState::IsNightIn(this)) return TEXT("The Mourners lit them at dusk: come back after dark");
	if (Tool == TEXT("pick") && !HoldsPick(Me)) return TEXT("Dig with a pick in hand");
	if (bFlame && !HoldsFlame(Me)) return TEXT("Hold a lit torch or lantern to it");
	if (!Needs.IsNone() && (!PS->GetInventory() || PS->GetInventory()->CountOf(Needs) <= 0)) return FString::Printf(TEXT("You need %s"), *ItemName(Needs));
	if (Seconds <= 0.05f)
	{
		Doer = Name;
		DoerPawn = Me;
		Finish();
		return FString();
	}
	const UWorld* World = GetWorld();
	DoneAt = World->GetGameState() ? World->GetGameState()->GetServerWorldTimeSeconds() + Seconds : World->GetTimeSeconds() + Seconds;
	Doer = Name;
	DoerPawn = Me;
	DoerFrom = Me->GetActorLocation();
	if (Kind == TEXT("dig")) WRSound::PlayAt(this, TEXT("grave.dig"), GetActorLocation());
	return Verb + TEXT("...");
}

void AWRGraveSpot::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || DoneAt <= 0.f) return;
	const UWorld* World = GetWorld();
	const float Now = World->GetGameState() ? float(World->GetGameState()->GetServerWorldTimeSeconds()) : World->GetTimeSeconds();
	AWRCharacterBase* Who = DoerPawn.Get();
	// Walk off, get knocked away or die: it stops (stand still while you work).
	if (!Who || !Who->IsAlive() || FVector::Dist2D(Who->GetActorLocation(), DoerFrom) > WRGraveWorldImpl::Tn(TEXT("graveyardWorld.holdDrift"), 120.f))
	{
		WRGraveWorldImpl::Tell(Who, TEXT("You stopped"));
		DoneAt = 0.f;
		Doer.Empty();
		DoerPawn = nullptr;
		return;
	}
	if (Kind == TEXT("dig") && FMath::Fmod(Now, 0.8f) < DeltaSeconds) WRSound::PlayAt(this, TEXT("grave.dig"), GetActorLocation());
	if (Now >= DoneAt) Finish();
}

void AWRGraveSpot::Finish()
{
	using namespace WRGraveWorldImpl;
	AWRCharacterBase* Who = DoerPawn.Get();
	AWRPlayerController* PC = ControllerOf(Who);
	AWRPlayerState* PS = PC ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
	const FString Name = Doer;
	DoneAt = 0.f;
	Doer.Empty();
	DoerPawn = nullptr;
	if (!PS) return;
	const bool bFirst = !DoneBy.Contains(Name);
	DoneBy.Add(Name);
	UWRInventoryComponent* Inv = PS->GetInventory();
	FString Got;
	if (bFirst && Inv)
	{
		for (const TPair<FName, int32>& G : Gives)
		{
			const int32 Added = Inv->AddItem(G.Key, G.Value);
			if (Added > 0) Got += FString::Printf(TEXT("%s%s x%d"), Got.IsEmpty() ? TEXT("") : TEXT(", "), *ItemName(G.Key), Added);
		}
	}
	if (bFirst && Xp > 0)
	{
		if (UWRPathComponent* Paths = PS->GetPaths())
		{
			const FName Main = Paths->GetMainPath();
			if (!Main.IsNone()) Paths->AwardXP(Main, Xp);
		}
	}
	if (!Quest.IsNone())
	{
		if (UWRWorldSystems* WS = GetWorld()->GetSubsystem<UWRWorldSystems>()) WS->QuestEvent(PS, TEXT("event"), Quest, 1);
	}
	if (Kind == TEXT("lantern"))
	{
		bLit = true;
		OnRep_Lit();
		WRSound::PlayAt(this, TEXT("grave.toro_light"), GetActorLocation() + FVector(0, 0, 120.f));
		if (SpotId == TEXT("S3"))
		{
			// The one unlit lantern at the Stair's break (GY 2.6 S3): every tōrō on the Stair burns gold for a game day.
			if (AWRGraveState* S = AWRGraveState::Get(this))
			{
				S->StairGoldUntilHours = float(GameHours(this) + Tn(TEXT("graveyardWorld.stairGoldHours"), 24.f));
				S->OnRep_State();
			}
			Tell(Who, bFirst ? TEXT("The lanterns of the Stair burn gold, all the way down") : TEXT("The Stair burns gold again"));
		}
		return;
	}
	if (Kind == TEXT("carve"))
	{
		// The seventh name (GY 2.6 S4): the bronze door opens on the seventh crypt.
		if (AWRGraveState* S = AWRGraveState::Get(this)) { S->bSeventhOpen = true; S->OnRep_State(); }
		WRSound::PlayAt(this, TEXT("grave.bronze_door"), GetActorLocation());
	}
	if (!Got.IsEmpty()) Tell(Who, FString::Printf(TEXT("%s: %s"), *Title, *Got));
	else if (!bFirst) Tell(Who, TEXT("Nothing more here"));
}

// ================================================================================================ the subsystem

UWRGraveWorld* UWRGraveWorld::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UWRGraveWorld>() : nullptr;
}

bool UWRGraveWorld::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UWRGraveWorld::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	FString Map = UWorld::RemovePIEPrefix(InWorld.GetMapName());
	Map.RemoveFromStart(InWorld.StreamingLevelsPrefix);
	if (!Map.EndsWith(FWRTuning::Get().Str(TEXT("graveyardWorld.map"), TEXT("L_Island"))) || !WRGrave::Live()) return;
	if (!FWRGraveData::Get().bLoaded)
	{
		UE_LOG(LogWildreach, Warning, TEXT("GRAVEYARD is live but Data/graveyard.json didn't load: nothing of the Graveyard is built"));
		return;
	}
	bActive = true;
	UE_LOG(LogWildreach, Log, TEXT("GRAVEYARD live on %s: waiting for the ground"), *Map);
}

void UWRGraveWorld::BuildScenery()
{
	// Every place, in a fixed order (the same on every machine, so each builds the same collision): sites, the kit, the
	// fields, the roads' tōrō, the trees (by 200 m cell), the stream, the Stair, then the catacombs' rooms and links.
	const FWRGraveData& D = FWRGraveData::Get();
	for (const FWRGraveData::FSite& S : D.Sites) Pieces.Add({ S.Kind, S.Id, S.At, S.Yaw, -1, S.Variant });
	for (const FWRGraveData::FSite& S : D.Kit) Pieces.Add({ S.Kind, S.Id, S.At, S.Yaw, -1, S.Variant });
	for (int32 i = 0; i < D.Fields.Num(); ++i)
	{
		const FWRGraveData::FField& F = D.Fields[i];
		FVector At(F.Centre, 0.f);
		float Z = 0.f;
		WRLand::GroundUnder(At, Z);
		Pieces.Add({ TEXT("field"), F.Id, FVector(F.Centre, Z), 0.f, i, 0 });
	}
	for (int32 i = 0; i < D.Roads.Num(); ++i) if (D.Roads[i].Points.Num()) Pieces.Add({ TEXT("road"), D.Roads[i].Id, D.Roads[i].Points[0], 0.f, i, 0 });
	{
		// Trees in cells: one place per 200 m cell (culling and the build queue stay small).
		TMap<FIntPoint, TArray<int32>> Cells;
		for (int32 i = 0; i < D.Trees.Num(); ++i) Cells.FindOrAdd(FIntPoint(FMath::FloorToInt(D.Trees[i].Base.X / 20000.f), FMath::FloorToInt(D.Trees[i].Base.Y / 20000.f))).Add(i);
		TArray<FIntPoint> Keys;
		Cells.GetKeys(Keys);
		Keys.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.Y != B.Y ? A.Y < B.Y : A.X < B.X; });
		for (const FIntPoint& K : Keys) Pieces.Add({ TEXT("trees"), NAME_None, D.Trees[Cells[K][0]].Base, 0.f, K.X * 1000 + K.Y, 0 });
	}
	if (D.Stream.Num()) Pieces.Add({ TEXT("stream"), NAME_None, FVector(D.Stream[0].X, D.Stream[0].Y, D.Stream[0].Z), 0.f, 0, 0 });
	if (D.Stair.Num()) Pieces.Add({ TEXT("stair"), TEXT("L5"), D.Stair[0], 0.f, 0, 0 });
	for (int32 i = 0; i < D.BonePits.Num(); ++i) Pieces.Add({ TEXT("bone_pit"), NAME_None, FVector(D.BonePits[i].X, D.BonePits[i].Y, D.BonePits[i].Z), 0.f, i, 0 });
	for (int32 i = 0; i < D.Lime.Num(); ++i) Pieces.Add({ TEXT("lime_pit"), NAME_None, D.Lime[i].At, 0.f, i, 0 });
	for (int32 i = 0; i < D.Rooms.Num(); ++i) Pieces.Add({ TEXT("room"), D.Rooms[i].Id, D.Rooms[i].At, D.Rooms[i].Yaw, i, 0 });
	for (int32 i = 0; i < D.Links.Num(); ++i) if (D.Links[i].Points.Num()) Pieces.Add({ TEXT("link"), NAME_None, D.Links[i].Points[0], 0.f, i, 0 });
	UE_LOG(LogWildreach, Log, TEXT("GRAVEYARD scenery queued: %d places"), Pieces.Num());
}

void UWRGraveWorld::StepBuild()
{
	UWorld* World = GetWorld();
	if (!World) return;
	if (!bQueued)
	{
		WaitClock += World->GetDeltaSeconds();
		const FWRLandDef* Def = nullptr;
		for (const FWRLandDef& L : WRLand::All()) if (L.Biome == WRGrave::Biome()) Def = &L;
		AWRLand* Land = Def ? UWRLandSubsystem::FindLand(World, Def->Id) : nullptr;
		if (!Land || !Land->IsGroundReady())
		{
			if (WaitClock > 60.f) { WaitClock = 0.f; UE_LOG(LogWildreach, Warning, TEXT("GRAVEYARD still waiting for its land's ground (Data/lands/graveyard.json)")); }
			return;
		}
		BuildScenery();
		bQueued = true;
	}
	if (!bSceneryDone)
	{
		const int32 PerFrame = FMath::Max(1, int32(WRTune(TEXT("graveyardWorld.buildPerFrame"), 4.f)));
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		for (int32 n = 0; n < PerFrame && PiecesBuilt < Pieces.Num(); ++n, ++PiecesBuilt)
		{
			const FPiece& Pc = Pieces[PiecesBuilt];
			AWRGraveScenery* A = World->SpawnActor<AWRGraveScenery>(AWRGraveScenery::StaticClass(), FTransform(FRotator(0.f, Pc.Yaw, 0.f), Pc.At), P);
			if (!A) continue;
			A->Kind = Pc.Kind;
			A->SiteId = Pc.Id;
			A->Variant = Pc.Variant;
			A->Index = Pc.Index;
			A->BuildGrave();
			Scenery.Add(A);
			for (int32 i = 0; i < A->NearLamps.Num(); ++i)
			{
				AllNear.Add(A->NearLamps[i]);
				AllNearPower.Add(A->NearPower.IsValidIndex(i) ? A->NearPower[i] : 1000.f);
			}
		}
		if (PiecesBuilt < Pieces.Num()) return;
		bSceneryDone = true;
		Pieces.Empty();
		LightClock = 0.f;
		UE_LOG(LogWildreach, Log, TEXT("GRAVEYARD scenery built: %d places, %d road lamps"), Scenery.Num(), AllNear.Num());
	}
	if (!bServerDone)
	{
		bServerDone = true;
		if (World->GetNetMode() != NM_Client) SpawnServerActors();
	}
}

AWRResourceNode* UWRGraveWorld::SpawnNode(FName Item, const FVector& At, int32 Hits, float PerHit, float RespawnSeconds, float Yaw)
{
	UWorld* World = GetWorld();
	const FTransform Tr(FRotator(0.f, Yaw < 0.f ? FMath::FRandRange(0.f, 360.f) : Yaw, 0.f), At);
	AWRResourceNode* N = World->SpawnActorDeferred<AWRResourceNode>(AWRResourceNode::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!N) return nullptr;
	N->ItemId = Item;
	N->Hits = FMath::Max(1, Hits);
	N->PerHit = FMath::Max(1, FMath::RoundToInt(PerHit));
	N->RespawnSeconds = RespawnSeconds;
	N->FinishSpawning(Tr);
	return N;
}

AWRWorldCache* UWRGraveWorld::SpawnCache(const FVector& At, float Yaw, const FString& Title, const TMap<FName, int32>& Items, const FString& Lore, float RefillHours)
{
	UWorld* World = GetWorld();
	const FTransform Tr(FRotator(0.f, Yaw, 0.f), At);
	AWRWorldCache* C = World->SpawnActorDeferred<AWRWorldCache>(AWRWorldCache::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!C) return nullptr;
	C->Kind = TEXT("chest");
	C->Biome = WRGrave::Biome();
	C->Title = Title;
	C->Items = Items;
	C->Lore = Lore;
	C->RefillHours = RefillHours;
	C->FinishSpawning(Tr);
	Caches.Add(C);
	return C;
}

AWRGraveSpot* UWRGraveWorld::SpawnSpot(FName Kind, FName Id, const FVector& At, const FString& Title, const FString& Verb, float Seconds)
{
	UWorld* World = GetWorld();
	const FTransform Tr(FRotator::ZeroRotator, At);
	AWRGraveSpot* S = World->SpawnActorDeferred<AWRGraveSpot>(AWRGraveSpot::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!S) return nullptr;
	S->Kind = Kind;
	S->SpotId = Id;
	S->Title = Title;
	S->Verb = Verb;
	S->Seconds = Seconds;
	S->FinishSpawning(Tr);
	Spots.Add(S);
	return S;
}

void UWRGraveWorld::SpawnServerActors()
{
	using namespace WRGraveWorldImpl;
	UWorld* World = GetWorld();
	const FWRGraveData& D = FWRGraveData::Get();
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	State = World->SpawnActor<AWRGraveState>(AWRGraveState::StaticClass(), FTransform::Identity, SP);

	// ---- the gathering (GY 2.7; numbers in tuning graveyardWorld.nodes) ----
	const auto Node = [&](const TCHAR* Kind, FName Item, const FVector& At, float Yaw = -1.f)
	{
		const FString K = FString::Printf(TEXT("graveyardWorld.nodes.%s."), Kind);
		return SpawnNode(Item, At, int32(Tn(*(K + TEXT("hits")), 4.f)), Tn(*(K + TEXT("perHit")), 1.f), Tn(*(K + TEXT("respawn")), 900.f), Yaw);
	};
	int32 Made = 0;
	const auto List = [&](const TCHAR* Key) -> const TArray<FWRGraveData::FNode>& { static const TArray<FWRGraveData::FNode> None; const TArray<FWRGraveData::FNode>* L = D.Resources.Find(FName(Key)); return L ? *L : None; };
	for (const FWRGraveData::FNode& N : List(TEXT("soulstone"))) { Node(TEXT("soulstone"), TEXT("soulstone"), N.At); ++Made; }
	for (const FWRGraveData::FNode& N : List(TEXT("bone"))) { Node(TEXT("bone"), TEXT("bone"), N.At); ++Made; }
	for (const FWRGraveData::FNode& N : List(TEXT("limestone"))) { Node(TEXT("limestone"), TEXT("limestone"), N.At); ++Made; }
	for (const FWRGraveData::FNode& N : List(TEXT("mortsafe"))) { Node(TEXT("mortsafe"), TEXT("grave_iron"), N.At, 90.f); ++Made; }
	for (const FWRGraveData::FNode& N : List(TEXT("rye"))) { Node(TEXT("rye"), TEXT("black_rye"), N.At); ++Made; }
	for (const FWRGraveData::FNode& N : List(TEXT("asphodel"))) { Node(TEXT("asphodel"), TEXT("asphodel"), N.At); ++Made; }
	// Below: soulstone grave seams, silver veins, wax nests, the Starfall Pit's star iron: a few nodes round each room's
	// walls (the room's own builder leaves its wall clear there).
	const auto RoomNodes = [&](const TCHAR* Key, const TCHAR* Kind, FName Item, int32 Count)
	{
		for (const FWRGraveData::FNode& N : List(Key))
		{
			const FWRGraveData::FRoom* R = D.FindRoom(N.Of);
			if (!R) continue;
			for (int32 i = 0; i < Count; ++i)
			{
				const float A = FMath::DegreesToRadians(R->Yaw) + (i + 0.5f) * 2.f * PI / Count;
				const float Rx = (R->bRound ? R->W * 0.5f : R->W * 0.5f) - 120.f, Ry = (R->bRound ? R->W * 0.5f : R->L * 0.5f) - 120.f;
				Node(Kind, Item, R->At + FVector(FMath::Cos(A) * FMath::Max(80.f, Rx), FMath::Sin(A) * FMath::Max(80.f, Ry), 20.f));
				++Made;
			}
		}
	};
	RoomNodes(TEXT("seam"), TEXT("seam"), TEXT("soulstone"), 2);
	RoomNodes(TEXT("silver"), TEXT("silver"), TEXT("silver_ore"), 2);
	RoomNodes(TEXT("wax"), TEXT("wax"), TEXT("wax"), 2);
	RoomNodes(TEXT("star"), TEXT("star"), TEXT("star_iron_ore"), 3);
	// Young churchyard yews (6 logs each, back in 2 game days): the yews of the tree list (the scenery draws the old
	// hawthorns and elms; a yew is a node so it can be cut and grow back).
	for (const FWRGraveData::FTree& T : D.Trees)
	{
		if (T.Kind != TEXT("yew")) continue;
		Node(TEXT("yew"), TEXT("yew_log"), T.Base, T.Yaw);
		++Made;
	}

	// ---- caches (GY 2.4-2.5) ----
	for (const FWRGraveData::FSite& S : D.Sites)
	{
		if (S.Id == TEXT("H5")) SpawnCache(S.At + FRotator(0.f, S.Yaw, 0.f).RotateVector(FVector(260.f, -340.f, 60.f)), S.Yaw, TEXT("The smith's anvil"), { { TEXT("iron_ingot"), 6 } }, TEXT("An anvil set on a smith's grave. The iron's still good."), Tn(TEXT("graveyardWorld.anvilRefillHours"), 168.f));
		if (S.Id == TEXT("H9")) SpawnCache(S.At + FRotator(0.f, S.Yaw, 0.f).RotateVector(FVector(-600.f, 500.f, 40.f)), S.Yaw, TEXT("Lime barrows"), { { TEXT("quicklime"), 6 } }, TEXT("Quicklime in barrows by the warm kiln, raked out and never carted."), Tn(TEXT("graveyardWorld.limeRefillHours"), 24.f));
	}
	for (const FWRGraveData::FRoom& R : D.Rooms)
	{
		if (R.Kind == TEXT("family_vault")) SpawnCache(R.At + FRotator(0.f, R.Yaw, 0.f).RotateVector(FVector(-R.W * 0.5f + 140.f, 0.f, 50.f)), R.Yaw, TEXT("A family's grave goods"), { { TEXT("silver_ingot"), 2 }, { TEXT("bone"), 6 }, { TEXT("wax"), 2 } }, TEXT("Rings, a comb, a lantern: what a family laid by its own."), Tn(TEXT("graveyardWorld.vaultRefillHours"), 96.f));
		if (R.Kind == TEXT("niche_corridor")) SpawnCache(R.At + FRotator(0.f, R.Yaw, 0.f).RotateVector(FVector(R.W * 0.5f - 120.f, 0.f, 40.f)), R.Yaw + 180.f, TEXT("A niche's offerings"), { { TEXT("soulstone_dust"), 3 }, { TEXT("wax"), 3 } }, TEXT("Offerings pushed into an empty niche: wax, coins gone green, soulstone dust."), Tn(TEXT("graveyardWorld.nicheRefillHours"), 48.f));
		if (R.Kind == TEXT("watchers_vault")) SpawnCache(R.At + FRotator(0.f, R.Yaw, 0.f).RotateVector(FVector(-R.W * 0.5f + 120.f, R.L * 0.5f - 120.f, 40.f)), R.Yaw, TEXT("A spent ration crate"), { { TEXT("hardtack"), 2 }, { TEXT("wax"), 2 } }, TEXT("The watchers' last crate. Two tins left, both opened."), Tn(TEXT("graveyardWorld.vaultRefillHours"), 96.f));
	}

	// ---- spots: keepsakes, digs, lanterns, the seventh name, the drowned bell (GY 2.6, 2.20) ----
	for (const FWRGraveData::FLore& L : D.Lore)
	{
		FVector At = FVector::ZeroVector;
		if (const FWRGraveData::FSite* S = L.Site.IsNone() ? nullptr : D.FindSite(L.Site)) At = S->At + FRotator(0.f, S->Yaw, 0.f).RotateVector(WRTune(*FString::Printf(TEXT("graveyardWorld.keepsakeAt.%s.x"), *L.Site.ToString()), 300.f) * FVector::ForwardVector) + FVector(0.f, 0.f, 40.f);
		else if (const FWRGraveData::FRoom* R = L.Room.IsNone() ? nullptr : D.FindRoom(L.Room)) At = R->At + FVector(0.f, 0.f, 95.f);
		if (At.IsZero()) continue;
		AWRGraveSpot* Sp = SpawnSpot(TEXT("keepsake"), L.Item, At, ItemName(L.Item), TEXT("Take"), 0.f);
		if (Sp) Sp->Gives.Add(L.Item, 1);
	}
	for (const FWRGraveData::FSecret& X : D.Secrets)
	{
		if (X.Kind == TEXT("yew_grave") || X.Kind == TEXT("full_grave"))
		{
			AWRGraveSpot* Sp = SpawnSpot(TEXT("dig"), X.Id, X.At, X.Kind == TEXT("yew_grave") ? TEXT("A grave-slab among the roots") : TEXT("A Mourner's grave"), TEXT("Dig"), X.Seconds);
			if (Sp) { Sp->Gives = X.Gives; Sp->Tool = TEXT("pick"); Sp->Quest = X.Kind == TEXT("yew_grave") ? FName(TEXT("yew_grave")) : NAME_None; }
		}
		else if (X.Kind == TEXT("unlit_lantern"))
		{
			AWRGraveSpot* Sp = SpawnSpot(TEXT("lantern"), X.Id, X.At, TEXT("The last lantern"), TEXT("Light it"), X.Seconds);
			if (Sp) { Sp->bFlame = true; Sp->Xp = X.Xp; }
		}
		else if (X.Kind == TEXT("seventh_name"))
		{
			AWRGraveSpot* Sp = SpawnSpot(TEXT("carve"), X.Id, X.At + FVector(0.f, 0.f, 120.f), TEXT("The seventh plate, blank"), TEXT("Carve the seventh name"), X.Seconds);
			if (Sp) { Sp->Gives = X.Gives; Sp->Needs = X.Needs; }
		}
		else if (X.Kind == TEXT("drowned_bell"))
		{
			const FWRGraveData::FRoom* R = D.FindRoom(X.Room);
			if (!R) continue;
			// The third cistern's bell lies on the bottom at its east end; its air pocket is a dry chamber in the mouth.
			const FVector Bell = R->At + FRotator(0.f, R->Yaw, 0.f).RotateVector(FVector(R->W * 0.5f - 600.f, 0.f, 60.f));
			AWRGraveSpot* Sp = SpawnSpot(TEXT("chamber"), X.Id, Bell, TEXT("The drowned bell's chamber"), TEXT("Search"), 2.f);
			if (Sp) Sp->Gives = X.Gives;
		}
	}
	// The cenotaphs of the Row of the Named (H2): dig any of three and find a lantern with a name tag, no body.
	if (const FWRGraveData::FSite* Row = D.FindSite(TEXT("H2")))
	{
		for (int32 i = 0; i < 3; ++i)
		{
			const FVector At = Row->At + FRotator(0.f, Row->Yaw, 0.f).RotateVector(FVector(-1300.f + 1200.f * i, -460.f + 380.f * i, 0.f));
			AWRGraveSpot* Sp = SpawnSpot(TEXT("dig"), FName(*FString::Printf(TEXT("H2_cenotaph_%d"), i)), At, TEXT("A named grave"), TEXT("Dig"), 3.f);
			if (Sp) { Sp->Tool = TEXT("pick"); Sp->Gives.Add(TEXT("lore_lantern_tag"), 1); Sp->Gives.Add(TEXT("paper_lantern"), 1); }
		}
		// Morwen's first quest (GY 2.11 "Keep the light"): five tōrō on the Row to light at night.
		for (int32 i = 0; i < 5; ++i)
		{
			const FVector At = Row->At + FRotator(0.f, Row->Yaw, 0.f).RotateVector(FVector(-2400.f + 1200.f * i, -1150.f, 0.f));
			AWRGraveSpot* Sp = SpawnSpot(TEXT("lantern"), FName(*FString::Printf(TEXT("H2_toro_%d"), i)), At, TEXT("A dark tōrō"), TEXT("Light it"), 1.5f);
			if (Sp) { Sp->bFlame = true; Sp->bNightOnly = true; Sp->Quest = TEXT("toro_lit"); }
		}
	}
	UE_LOG(LogWildreach, Log, TEXT("GRAVEYARD server: %d nodes, %d caches, %d spots"), Made, Caches.Num(), Spots.Num());
}

void UWRGraveWorld::OpenChapelCrypt()
{
	if (!State) State = AWRGraveState::Get(this);
	if (!State || State->bChapelCryptOpen) return;
	State->bChapelCryptOpen = true;
	State->OnRep_State();
	UE_LOG(LogWildreach, Log, TEXT("GRAVEYARD: the chapel crypt's stair is open"));
}

void UWRGraveWorld::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	if (!World) return;
	if (!bSceneryDone || !bServerDone)
	{
		StepBuild();
		if (!bSceneryDone) return;
	}
	if (World->GetNetMode() != NM_DedicatedServer) StepLights(DeltaTime);
	StepState();
	if (World->GetNetMode() == NM_Client) return;
	StepServer(DeltaTime);
}

void UWRGraveWorld::StepState()
{
	// Every machine, collision included: the parts that follow the Graveyard's state (the chapel crypt house's door,
	// the seventh crypt's bronze leaves).
	if (!State) State = AWRGraveState::Get(this);
	if (!State) return;
	for (const TWeakObjectPtr<AWRGraveScenery>& W : Scenery)
	{
		AWRGraveScenery* S = W.Get();
		if (!S || S->StateFlag.IsNone()) continue;
		const bool bOn = S->StateFlag == TEXT("crypt") ? State->bChapelCryptOpen : S->StateFlag == TEXT("seventh") ? State->bSeventhOpen : false;
		if (bOn != S->bStateShown) S->ApplyState(bOn);
	}
}

void UWRGraveWorld::StepLights(float Dt)
{
	// Night blocks (lit fire boxes, grave-light, candles) show from dusk; real lights only near the viewer: the nearest
	// graveyardWorld.nearLamps road tōrō within nearLampRange (the far ones read by their glowing boxes alone).
	LightClock -= Dt;
	if (LightClock > 0.f) return;
	LightClock = 0.5f;
	UWorld* World = GetWorld();
	const bool bNight = AWRGameState::IsNightIn(World);
	// Under the ground it is always night (the catacombs' candles and grave-light burn all day).
	FVector View = FVector::ZeroVector;
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		FRotator R;
		PC->GetPlayerViewPoint(View, R);
	}
	const bool bStairGold = State ? State->StairGold() : (AWRGraveState::Get(this) && AWRGraveState::Get(this)->StairGold());
	for (const TWeakObjectPtr<AWRGraveScenery>& W : Scenery)
	{
		AWRGraveScenery* S = W.Get();
		if (!S) continue;
		const bool bUnder = S->Kind == TEXT("room") || S->Kind == TEXT("link");
		const bool bLit = bNight || bUnder;
		for (int32 i = 0; i < S->NightLights.Num(); ++i)
		{
			if (UPointLightComponent* L = S->NightLights[i]) L->SetIntensity(bLit ? (S->NightPower.IsValidIndex(i) ? S->NightPower[i] : 1000.f) : 0.f);
		}
		if (S->NightRoot && S->NightRoot->IsVisible() != bLit) S->NightRoot->SetVisibility(bLit, true);
		// The Stair's gold (S3): its lamps warm and its fire boxes burn gold instead of green.
		if (S->Kind == TEXT("stair"))
		{
			if (S->AltNightRoot)
			{
				if (S->NightRoot) S->NightRoot->SetVisibility(bLit && !bStairGold, true);
				S->AltNightRoot->SetVisibility(bLit && bStairGold, true);
			}
			for (UPointLightComponent* L : S->NearLamps) if (L) L->SetLightColor(bStairGold ? FLinearColor(FColor(255, 196, 96)) : FLinearColor(FColor(159, 227, 192)));
		}
	}
	// The light budget: the nearest lamps on.
	const int32 Budget = int32(WRTune(TEXT("graveyardWorld.nearLamps"), 28.f));
	const float Range = WRTune(TEXT("graveyardWorld.nearLampRange"), 9000.f);
	TArray<TPair<float, int32>> Near;
	for (int32 i = 0; i < AllNear.Num(); ++i)
	{
		UPointLightComponent* L = AllNear[i].Get();
		if (!L) continue;
		const float D2 = float(FVector::DistSquared(L->GetComponentLocation(), View));
		if (bNight && D2 < Range * Range) Near.Add({ D2, i });
		else if (L->IsVisible()) L->SetVisibility(false);
	}
	Near.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; });
	for (int32 n = 0; n < Near.Num(); ++n)
	{
		UPointLightComponent* L = AllNear[Near[n].Value].Get();
		const bool bOn = n < Budget;
		if (L->IsVisible() != bOn) L->SetVisibility(bOn);
		if (bOn) L->SetIntensity(AllNearPower[Near[n].Value]);
	}
}

void UWRGraveWorld::StepServer(float Dt)
{
	ServerClock -= Dt;
	if (ServerClock > 0.f) return;
	ServerClock = 2.f;
	const double Hours = WRGraveWorldImpl::GameHours(this);
	for (const TWeakObjectPtr<AWRWorldCache>& C : Caches)
	{
		if (C.IsValid() && C->bTaken && C->RefillAtHours > 0.0 && Hours >= C->RefillAtHours) C->Refill();
	}
	// The Row of the Named's tōrō go dark again at dawn (Morwen lights them each dusk; you light them for her).
	if (!AWRGameState::IsNightIn(this))
	{
		for (const TWeakObjectPtr<AWRGraveSpot>& S : Spots)
		{
			if (S.IsValid() && S->bLit && S->bNightOnly) { S->bLit = false; S->DoneBy.Reset(); }
		}
	}
}

AWRGraveSpot* UWRGraveWorld::SpotNear(const FVector& At) const
{
	AWRGraveSpot* Best = nullptr;
	float BestD = TNumericLimits<float>::Max();
	for (TActorIterator<AWRGraveSpot> It(GetWorld()); It; ++It)
	{
		const float D = float(FVector::Dist(It->GetActorLocation(), At));
		if (D <= It->Reach && D < BestD) { Best = *It; BestD = D; }
	}
	return Best;
}

bool UWRGraveWorld::Prompt(const AWRPlayerController* PC, FString& OutTarget, FString& OutVerb)
{
	const UWRGraveWorld* G = Get(PC);
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	if (!G || !G->bActive || !Me) return false;
	const AWRGraveSpot* S = G->SpotNear(Me->GetActorLocation());
	if (!S) return false;
	const AWRPlayerState* PS = PC->GetPlayerState<AWRPlayerState>();
	OutTarget = S->Title;
	if (S->DoneAt > 0.f) OutVerb = S->Verb + TEXT("... (hold still)");
	else if (S->Kind == TEXT("lantern") && S->bLit) OutVerb = TEXT("Burning");
	else OutVerb = S->Seconds > 0.05f ? FString::Printf(TEXT("%s (%.0f s)"), *S->Verb, S->Seconds) : S->Verb;
	(void)PS;
	return true;
}

bool UWRGraveWorld::ClientInteract(AWRPlayerController* PC)
{
	const UWRGraveWorld* G = Get(PC);
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	if (!G || !G->bActive || !Me) return false;
	const AWRGraveSpot* S = G->SpotNear(Me->GetActorLocation());
	if (!S) return false;
	PC->SendCommand(FString::Printf(TEXT("world grave spot %s"), *S->GetName()));
	return true;
}

FString UWRGraveWorld::Command(AWRPlayerController* PC, const TArray<FString>& Args)
{
	UWRGraveWorld* G = Get(PC);
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	if (!G || !G->bActive || !Me) return TEXT("Not here");
	const FString Verb = Args.IsValidIndex(2) ? Args[2] : FString();
	if (Verb == TEXT("spot"))
	{
		// The named spot if it's in reach (a little slack), else the nearest.
		AWRGraveSpot* S = nullptr;
		if (Args.IsValidIndex(3))
		{
			for (TActorIterator<AWRGraveSpot> It(PC->GetWorld()); It; ++It) if (It->GetName() == Args[3]) S = *It;
		}
		if (!S) S = G->SpotNear(Me->GetActorLocation());
		return S ? S->Start(PC) : TEXT("Nothing here");
	}
	return TEXT("?");
}

FString UWRGraveWorld::UseItem(AWRPlayerState* PS, FName ItemId)
{
	// The Graveyard's bag items with a use of their own are the necromancy and boss tracks' (the heartwood stake on a
	// tomb, the lime pot thrown): those answer in their own files. Nothing here yet.
	(void)PS;
	(void)ItemId;
	return FString();
}
