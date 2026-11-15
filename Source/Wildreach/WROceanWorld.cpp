#include "WROceanWorld.h"
#include "Wildreach.h"
#include "WRCharacterBase.h"
#include "WRCreature.h"
#include "WRFarm.h"
#include "WRFx.h"
#include "WRGameState.h"
#include "WRInventory.h"
#include "WRLand.h"
#include "WROceanTypes.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRResourceNode.h"
#include "WRSound.h"
#include "WRTide.h"
#include "WRTuning.h"
#include "WRUnderwater.h"
#include "WRWorldSystems.h"
#include "WRAbilitiesTide.h"
#include "WRSurvival.h"
#include "Components/PointLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

namespace WROceanParts { void Build(AWROceanScenery* A); }

namespace WROceanWorldImpl
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
	float Now(const UObject* Ctx)
	{
		const UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
		return W && W->GetGameState() ? float(W->GetGameState()->GetServerWorldTimeSeconds()) : 0.f;
	}
	FString ItemName(FName Id)
	{
		const FWRItemDef* D = FWRItemRegistry::Get().Find(Id);
		return D ? D->Name : Id.ToString();
	}
	FString PlayerOf(const AActor* A) { return WRWorld::PlayerOf(A); }
	AWRPlayerState* StateOf(const AActor* Pawn) { const APawn* P = Cast<APawn>(Pawn); return P ? P->GetPlayerState<AWRPlayerState>() : nullptr; }
	/** The clan a survivor walks with (replicated on the player state), or empty. */
	FString ClanOf(const AActor* Pawn) { const AWRPlayerState* PS = StateOf(Pawn); return PS ? PS->ClanName : FString(); }
	/** Server: a quest beat for Hae-won's quests (world.json quests.haewon objectives: event / gather / take ...). */
	void QuestEvent(const AActor* Pawn, FName Type, FName What, int32 Count)
	{
		AWRPlayerState* PS = StateOf(Pawn);
		UWRWorldSystems* WS = Pawn ? Pawn->GetWorld()->GetSubsystem<UWRWorldSystems>() : nullptr;
		if (PS && WS) WS->QuestEvent(PS, Type, What, Count);
	}
	/** A tamed creature of this species belonging to Who within Range cm (a Pearldiver's nose for pearls). */
	bool OwnsTameNear(const AActor* Who, FName Species, const FVector& At, float Range)
	{
		if (!Who) return false;
		for (TActorIterator<AWRCreature> It(Who->GetWorld()); It; ++It)
		{
			if (It->IsTamed() && It->IsAlive() && It->CreatureId == Species && It->GetTameOwner() == PlayerOf(Who) && FVector::DistSquared(It->GetActorLocation(), At) < Range * Range) return true;
		}
		return false;
	}
}
using namespace WROceanWorldImpl;

// ================================================================================================ the scenery

AWROceanScenery::AWROceanScenery()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickInterval = 0.f;
}

void AWROceanScenery::BuildOcean()
{
	WROceanParts::Build(this);
	SetActorTickEnabled(Float || Bell || Sand || Leaves.Num() > 0 || Fire);
}

void AWROceanScenery::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (GetNetMode() == NM_DedicatedServer) return;
	// The Tide Clock's float rides the level in its slot (every frame: it moves a few mm a second, smoothly).
	if (Float)
	{
		const float L = WRTide::Level(this);
		FVector P = Float->GetRelativeLocation();
		P.Z = FloatBaseZ + L;
		Float->SetRelativeLocation(P);
	}
	const AWROceanState* S = AWROceanState::Get(this);
	const float NowS = Now(this);
	// The drowned bell swings 12 s from a ring: a damped pendulum on its headstock (period 2.2 s).
	if (Bell)
	{
		const float Since = S ? NowS - float(S->BellRungAt) : 100.f;
		const float Swing = Since >= 0.f && Since < 12.f ? 18.f * FMath::Exp(-Since * 0.28f) * FMath::Sin(Since * 2.f * PI / 2.2f) : 0.f;
		Bell->SetRelativeRotation(FRotator(0.f, 0.f, Swing));
	}
	// The Low Gate's sand bank slumps down over 6 s when the King Tide's Low begins, and builds back in the Return.
	if (Sand && S)
	{
		const bool bOpen = S->KingTide == EWRKingTide::Low;
		const float Want = bOpen ? 1.f : 0.f;
		const float Cur = Sand->GetRelativeScale3D().Z < 0.2f ? 1.f : 1.f - (Sand->GetRelativeScale3D().Z - 0.15f) / 0.85f;
		const float Next = FMath::FInterpConstantTo(Cur, Want, DeltaSeconds, 1.f / 6.f);
		Sand->SetRelativeScale3D(FVector(1.f, 1.f, FMath::Lerp(1.f, 0.15f, Next)));
	}
	// The Pound's sluice leaves swing out on their hinge drums when they burst (0.8 s, ease-out), and hang open.
	if (Leaves.Num() > 0 && S)
	{
		const float Since = NowS - float(S->SluicesBurstAt);
		const float A = S->bSluicesBurst ? FMath::Clamp(Since / 0.8f, 0.f, 1.f) : 0.f;
		const float Ease = 1.f - FMath::Square(1.f - A);
		for (int32 i = 0; i < Leaves.Num(); ++i)
		{
			if (USceneComponent* L = Leaves[i]) L->SetRelativeRotation(FRotator(0.f, LeafSign.IsValidIndex(i) ? LeafSign[i] * 70.f * Ease : 0.f, 0.f));
		}
	}
	// The Beacon Stack's fire: shows for the clans that lit it (and for a lone survivor who did).
	if (Fire)
	{
		bool bMine = false;
		if (S)
		{
			const APlayerController* PC = GetWorld()->GetFirstPlayerController();
			const FString Me = PC && PC->GetPawn() ? PlayerOf(PC->GetPawn()) : FString();
			const FString Clan = PC ? ClanOf(PC->GetPawn()) : FString();
			for (const FString& By : S->BeaconLitBy) bMine |= By == Me || (!Clan.IsEmpty() && By == Clan);
		}
		if (Fire->IsVisible() != bMine) Fire->SetVisibility(bMine, true);
	}
}

// ================================================================================================ the spots

AWROceanSpot::AWROceanSpot()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	SetNetUpdateFrequency(2.f);
	SetNetCullDistanceSquared(FMath::Square(15000.f));
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWROceanSpot::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWROceanSpot, Kind);
	DOREPLIFETIME(AWROceanSpot, SpotId);
	DOREPLIFETIME(AWROceanSpot, Title);
	DOREPLIFETIME(AWROceanSpot, Verb);
	DOREPLIFETIME(AWROceanSpot, Seconds);
	DOREPLIFETIME(AWROceanSpot, DoneAt);
	DOREPLIFETIME(AWROceanSpot, Doer);
}

bool AWROceanSpot::IsDoneBy(const FString& Player, double NowHours) const
{
	const double* At = DoneBy.Find(Player);
	if (!At) return false;
	return RespawnHours <= 0.f || NowHours - *At < RespawnHours;
}

FString AWROceanSpot::Start(AWRPlayerController* PC)
{
	AWRCharacterBase* Me = PC ? Cast<AWRCharacterBase>(PC->GetPawn()) : nullptr;
	if (!Me) return FString();
	const FString Who = PlayerOf(Me);
	if (DoneAt > 0.f) return Doer == Who ? TEXT("Hold still...") : FString::Printf(TEXT("%s is at it"), *Doer);
	UWROceanWorld* W = UWROceanWorld::Get(this);
	if (Kind == TEXT("stone"))
	{
		// A tsunami stone read is never "taken": it counts toward Hae-won's fifth (all nine).
		if (W)
		{
			int32 Index = 0;
			LexFromString(Index, *SpotId.ToString().RightChop(15));
			TSet<int32>& Read = W->StonesRead.FindOrAdd(Who);
			Read.Add(Index);
			if (Read.Num() >= 9) QuestEvent(Me, TEXT("event"), TEXT("stones_say"), 1);
			return FString::Printf(TEXT("A wave carved deep, and one line under it. (%d of 9 read)"), Read.Num());
		}
		return FString();
	}
	if (Kind == TEXT("anchor"))
	{
		QuestEvent(Me, TEXT("event"), TEXT("cathedral_anchor"), 1);
		return TEXT("Cold stone, an iron ring rotted thin. The kelp leans the way the sea goes.");
	}
	if (Kind == TEXT("bell"))
	{
		// The drowned bell (cook 2.5): its ring carries over the whole reef; every Tidecoil in the Hall wakes and comes.
		if (AWROceanState* S = AWROceanState::Get(this))
		{
			if (Now(this) - S->BellRungAt < 12.f) return TEXT("It's still swinging");
			S->BellRungAt = Now(this);
		}
		WRSound::Broadcast(this, TEXT("ocean.drowned_bell"));
		QuestEvent(Me, TEXT("event"), TEXT("drowned_bell"), 1);
		return TEXT("The bell rings out over the reef. Something in the dark answers.");
	}
	if (Kind == TEXT("beacon"))
	{
		AWROceanState* S = AWROceanState::Get(this);
		const FString Clan = ClanOf(Me);
		const FString By = Clan.IsEmpty() ? Who : Clan;
		if (S && S->BeaconLitBy.Contains(By)) return TEXT("Your fire burns on the stack");
		UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Me);
		const int32 Wood = int32(Tn(TEXT("oceanWorld.beaconWood"), 4.f));
		if (!Inv || (Inv->CountOf(TEXT("wood")) < Wood && Inv->CountOf(TEXT("driftwood")) < Wood)) return FString::Printf(TEXT("The beacon needs %d wood (or driftwood)"), Wood);
	}
	if (Kind == TEXT("key"))
	{
		UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Me);
		if (Inv && Inv->CountOf(TEXT("sluice_key")) > 0) return TEXT("The lock is empty. You have the key.");
	}
	if (Kind == TEXT("secret"))
	{
		// Its window and what it needs (cook 2.20): said plainly, so a secret found early can be come back to.
		const float H = AWRGameState::HourIn(this);
		const float Full = WRTide::Fullness(this);
		if (When == TEXT("midnight_high") && !((H >= 23.f || H < 1.f) && Full > Tn(TEXT("oceanWorld.secretHighTide"), 0.8f)))
			return TEXT("The window's under the water. At midnight, at the top of the flood, it stands clear");
		if (When == TEXT("noon_clear"))
		{
			const FWRWeatherKind* Wx = UWRWeatherSubsystem::ActiveKind(this, WROcean::Biome());
			if (!(H >= 11.f && H < 13.f) || (Wx && (Wx->Id == TEXT("fog") || Wx->Id == TEXT("rain") || Wx->Id == TEXT("squall") || Wx->Id == TEXT("overcast"))))
				return TEXT("The groove in the stone means nothing now. At noon, under a clear sky, it points somewhere");
		}
		if (When == TEXT("king_tide"))
		{
			const AWROceanState* S = AWROceanState::Get(this);
			if (!S || S->KingTide != EWRKingTide::Low) return TEXT("Barnacled stone under deep water. The King Tide's Low lays it bare");
			if (Me->GetPlayerState()) QuestEvent(Me, TEXT("event"), TEXT("low_gate"), 1);
			return TEXT("The Low Gate stands out of the sea. Through it, the Drowned Hall, while the water is out");
		}
		if (Needs == TEXT("sluice_key"))
		{
			const UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Me);
			if (!Inv || Inv->CountOf(TEXT("sluice_key")) <= 0) return TEXT("A bronze door with a sluice-keeper's lock. It wants its key");
		}
		if (Needs == TEXT("pearldiver") && !OwnsTameNear(Me, TEXT("pearldiver"), GetActorLocation(), Tn(TEXT("oceanWorld.pearldiverRange"), 2000.f) * 1.5f))
			return TEXT("Black water and rock. Something with a better nose than yours could find a way in");
	}
	if (IsDoneBy(Who, GameHours(this)))
	{
		return Kind == TEXT("nest") || Kind == TEXT("clam") ? TEXT("Nothing more here today") : TEXT("You've had what's here");
	}
	DoneAt = Now(this) + Seconds;
	Doer = Who;
	DoerPawn = Me;
	DoerFrom = Me->GetActorLocation();
	SetActorTickEnabled(true);
	if (Seconds <= 0.01f) { Finish(); return FString(); }
	return FString::Printf(TEXT("%s..."), *Verb);
}

void AWROceanSpot::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || DoneAt <= 0.f) return;
	AWRCharacterBase* P = DoerPawn.Get();
	if (!P || !P->IsAlive() || FVector::Dist(P->GetActorLocation(), DoerFrom) > 150.f || FVector::Dist(P->GetActorLocation(), GetActorLocation()) > Reach + 200.f)
	{
		if (P) Tell(P, TEXT("You let go"));
		DoneAt = 0.f;
		Doer.Reset();
		DoerPawn = nullptr;
		SetActorTickEnabled(false);
		return;
	}
	if (Now(this) >= DoneAt) Finish();
}

void AWROceanSpot::Finish()
{
	AWRCharacterBase* P = DoerPawn.Get();
	DoneAt = 0.f;
	Doer.Reset();
	DoerPawn = nullptr;
	SetActorTickEnabled(false);
	if (!P) return;
	const FString Who = PlayerOf(P);
	UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(P);
	if (!Inv) return;
	TMap<FName, int32> Pay = Gives;
	if (Kind == TEXT("beacon"))
	{
		const int32 Wood = int32(Tn(TEXT("oceanWorld.beaconWood"), 4.f));
		if (Inv->CountOf(TEXT("wood")) >= Wood) Inv->RemoveItem(TEXT("wood"), Wood);
		else if (Inv->CountOf(TEXT("driftwood")) >= Wood) Inv->RemoveItem(TEXT("driftwood"), Wood);
		else { Tell(P, TEXT("You've no wood left to light it")); return; }
		if (AWROceanState* S = AWROceanState::Get(this))
		{
			const FString Clan = ClanOf(P);
			S->BeaconLitBy.AddUnique(Clan.IsEmpty() ? Who : Clan);
		}
		WRSound::Broadcast(this, TEXT("ocean.beacon_light"));
		QuestEvent(P, TEXT("event"), TEXT("beacon_lit"), 1);
		Tell(P, TEXT("The driftwood catches. The stack burns for your people all night, over the whole shelf."));
		return;
	}
	if (Kind == TEXT("clam"))
	{
		// A giant clam: its meat, and a pearl now and then (2%; x5 with your Pearldiver close by: cook 2.7).
		Pay.Add(TEXT("clam_meat"), 1);
		float Chance = Tn(TEXT("oceanWorld.pearlChance"), 0.02f);
		if (OwnsTameNear(P, TEXT("pearldiver"), GetActorLocation(), Tn(TEXT("oceanWorld.pearldiverRange"), 2000.f))) Chance *= Tn(TEXT("oceanWorld.pearldiverPearls"), 5.f);
		if (const AWROceanState* S = AWROceanState::Get(this); S && S->KingTide != EWRKingTide::None) Chance *= Tn(TEXT("tide.kingTide.pearlMult"), 2.f);
		if (FMath::FRand() < Chance) Pay.Add(TEXT("pearl"), 1);
	}
	if (const int32* Gem = Pay.Find(TEXT("@gem")))
	{
		// A gem of any kind (the secret's "@gem").
		TArray<FName> Gems;
		for (const FWRItemDef& I : FWRItemRegistry::Get().All()) if (I.Category == TEXT("gem")) Gems.Add(I.Id);
		const int32 N = *Gem;
		Pay.Remove(TEXT("@gem"));
		if (Gems.Num() > 0) Pay.FindOrAdd(Gems[FMath::RandRange(0, Gems.Num() - 1)]) += N;
	}
	DoneBy.Add(Who, GameHours(this));
	TArray<FString> Got;
	for (const TPair<FName, int32>& G : Pay)
	{
		if (G.Value <= 0) continue;
		Inv->AddItem(G.Key, G.Value);
		Got.Add(FString::Printf(TEXT("%s x%d"), *ItemName(G.Key), G.Value));
		QuestEvent(P, TEXT("gather"), G.Key, G.Value);
	}
	if (Kind == TEXT("key")) WRSound::Broadcast(this, TEXT("ocean.key_turn"));
	if (!Lore.IsEmpty()) Tell(P, Lore);
	if (Got.Num() > 0) Tell(P, FString::Printf(TEXT("%s: %s"), *Title, *FString::Join(Got, TEXT(", "))));
	UE_LOG(LogWildreach, Log, TEXT("OCEAN spot %s (%s) done by %s: %s"), *SpotId.ToString(), *Kind.ToString(), *Who, *FString::Join(Got, TEXT(", ")));
}

// ================================================================================================ the subsystem

bool UWROceanWorld::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

TStatId UWROceanWorld::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWROceanWorld, STATGROUP_Tickables);
}

UWROceanWorld* UWROceanWorld::Get(const UObject* WorldContext)
{
	const UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	return W ? W->GetSubsystem<UWROceanWorld>() : nullptr;
}

void UWROceanWorld::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	FString Map = UWorld::RemovePIEPrefix(InWorld.GetMapName());
	Map.RemoveFromStart(InWorld.StreamingLevelsPrefix);
	if (!Map.EndsWith(FWRTuning::Get().Str(TEXT("oceanWorld.map"), TEXT("L_Island"))) || !WROcean::Live()) return;
	bActive = true;
	UE_LOG(LogWildreach, Log, TEXT("OCEAN live on %s: waiting for the coast's ground"), *Map);
}

void UWROceanWorld::BuildScenery()
{
	// Every place, in a fixed order (the same on every machine, so each builds the same collision): the landmarks, the
	// sites, the kit, the caves, the Hall's rooms, the tsunami stones, the dyke, then the decoration in 250 m cells.
	const FWROceanData& D = FWROceanData::Get();
	for (int32 i = 0; i < D.Landmarks.Num(); ++i) { const auto& P = D.Landmarks[i]; Pieces.Add({ P.Kind, P.Id, P.Zone, P.At, P.Yaw, i, P.Variant, P.Size }); }
	for (int32 i = 0; i < D.Sites.Num(); ++i) { const auto& P = D.Sites[i]; Pieces.Add({ P.Kind, P.Id, P.Zone, P.At, P.Yaw, i, P.Variant, P.Size }); }
	for (int32 i = 0; i < D.Kit.Num(); ++i) { const auto& P = D.Kit[i]; Pieces.Add({ P.Kind, NAME_None, P.Zone, P.At, P.Yaw, i, P.Variant, P.Lean }); }
	for (int32 i = 0; i < D.Caves.Num(); ++i) { const auto& C = D.Caves[i]; Pieces.Add({ TEXT("cave"), C.Id, C.Zone, C.At, C.MouthYaw, i, 0, 0.f }); }
	Pieces.Add({ TEXT("hall"), TEXT("drowned_hall"), TEXT("O5"), D.HallAt, 0.f, 0, 0, 0.f });
	for (int32 i = 0; i < D.TsunamiStones.Num(); ++i) Pieces.Add({ TEXT("tsunami_stone"), NAME_None, TEXT("O3"), D.TsunamiStones[i], D.TsunamiYaw.IsValidIndex(i) ? D.TsunamiYaw[i] : 0.f, i, 0, 0.f });
	// The decoration: one actor a 250 m cell (its instances drawn as a handful of instanced parts).
	const float Cell = Tn(TEXT("oceanWorld.decorCell"), 25000.f);
	TSet<FIntPoint> Cells;
	const auto AddCells = [&](const TArray<FWROceanData::FDecor>& L) { for (const auto& R : L) Cells.Add(FIntPoint(FMath::FloorToInt(R.At.X / Cell), FMath::FloorToInt(R.At.Y / Cell))); };
	AddCells(D.Grass); AddCells(D.Driftwood); AddCells(D.Tamarisk); AddCells(D.Kelp); AddCells(D.Coral); AddCells(D.Clams);
	AddCells(D.Rocks); AddCells(D.Barnacles); AddCells(D.Urchins); AddCells(D.Mangroves);
	for (const auto& W : D.Walls) Cells.Add(FIntPoint(FMath::FloorToInt(W.A.X / Cell), FMath::FloorToInt(W.A.Y / Cell)));
	TArray<FIntPoint> Sorted = Cells.Array();
	Sorted.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.Y != B.Y ? A.Y < B.Y : A.X < B.X; });
	for (const FIntPoint& C : Sorted)
	{
		const FVector At((C.X + 0.5f) * Cell, (C.Y + 0.5f) * Cell, 0.f);
		Pieces.Add({ TEXT("decor"), NAME_None, NAME_None, At, 0.f, C.X * 10000 + (C.Y & 0xFFFF), 0, Cell });
	}
	UE_LOG(LogWildreach, Log, TEXT("OCEAN scenery queued: %d places (%d decoration cells)"), Pieces.Num(), Sorted.Num());
}

void UWROceanWorld::StepBuild()
{
	UWorld* World = GetWorld();
	if (!World) return;
	if (!bQueued)
	{
		WaitClock += World->GetDeltaSeconds();
		const FWRLandDef* Def = WRLand::Find(TEXT("ocean"));
		AWRLand* Land = Def ? UWRLandSubsystem::FindLand(World, Def->Id) : nullptr;
		if (!Land || !Land->IsGroundReady())
		{
			if (WaitClock > 60.f) { WaitClock = 0.f; UE_LOG(LogWildreach, Warning, TEXT("OCEAN still waiting for the coast's ground (Data/lands/ocean.json)")); }
			return;
		}
		BuildScenery();
		bQueued = true;
	}
	if (!bSceneryDone)
	{
		const int32 PerFrame = FMath::Max(1, int32(Tn(TEXT("oceanWorld.buildPerFrame"), 3.f)));
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		for (int32 n = 0; n < PerFrame && PiecesBuilt < Pieces.Num(); ++n, ++PiecesBuilt)
		{
			const FPiece& Pc = Pieces[PiecesBuilt];
			AWROceanScenery* A = World->SpawnActor<AWROceanScenery>(AWROceanScenery::StaticClass(), FTransform(FRotator(0.f, Pc.Yaw, 0.f), Pc.At), P);
			if (!A) continue;
			A->Kind = Pc.Kind;
			A->SiteId = Pc.Id;
			A->Zone = Pc.Zone;
			A->Variant = Pc.Variant;
			A->Index = Pc.Index;
			A->Size = Pc.Size;
			A->BuildOcean();
			Scenery.Add(A);
		}
		if (PiecesBuilt < Pieces.Num()) return;
		bSceneryDone = true;
		Pieces.Empty();
		UE_LOG(LogWildreach, Log, TEXT("OCEAN scenery built: %d places"), Scenery.Num());
	}
	if (!bServerDone)
	{
		bServerDone = true;
		if (World->GetNetMode() != NM_Client) SpawnServerActors();
	}
}

AWRResourceNode* UWROceanWorld::SpawnNode(FName Item, const FVector& At, int32 Hits, float PerHit, float RespawnSeconds, float Yaw)
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

AWRWorldCache* UWROceanWorld::SpawnCache(const FVector& At, float Yaw, const FString& Title, const TMap<FName, int32>& Items, const FString& Lore, float RefillHours)
{
	UWorld* World = GetWorld();
	const FTransform Tr(FRotator(0.f, Yaw, 0.f), At);
	AWRWorldCache* C = World->SpawnActorDeferred<AWRWorldCache>(AWRWorldCache::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!C) return nullptr;
	C->Kind = TEXT("chest");
	C->Biome = WROcean::Biome();
	C->Title = Title;
	C->Items = Items;
	C->Lore = Lore;
	C->RefillHours = RefillHours;
	C->FinishSpawning(Tr);
	Caches.Add(C);
	return C;
}

AWROceanSpot* UWROceanWorld::SpawnSpot(FName Kind, FName Id, const FVector& At, const FString& Title, const FString& Verb, float Seconds)
{
	UWorld* World = GetWorld();
	const FTransform Tr(FRotator::ZeroRotator, At);
	AWROceanSpot* S = World->SpawnActorDeferred<AWROceanSpot>(AWROceanSpot::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
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

void UWROceanWorld::SpawnServerActors()
{
	UWorld* World = GetWorld();
	const FWROceanData& D = FWROceanData::Get();
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (!AWROceanState::Get(this)) State = World->SpawnActor<AWROceanState>(AWROceanState::StaticClass(), FTransform::Identity, SP);

	// ---- the nodes (cook 2.7): their numbers from tuning oceanWorld.nodes.<item> (give, hits, perHit, respawn) ----
	const FWRTuning& T = FWRTuning::Get();
	int32 Nodes = 0, Clams = 0;
	for (const FWROceanData::FNode& N : D.Nodes)
	{
		if (N.Item == TEXT("giant_clam"))
		{
			// Clams are opened by hand (E), not struck: a spot with a day's respawn per survivor.
			if (AWROceanSpot* S = SpawnSpot(TEXT("clam"), FName(*FString::Printf(TEXT("clam_%d"), Clams)), N.At + FVector(0, 0, 40.f), TEXT("A giant clam"), TEXT("Open the clam"), 1.5f))
			{
				S->RespawnHours = T.Num(TEXT("oceanWorld.clamHours"), 24.f);
				S->Reach = 260.f + 60.f * N.Size;
				++Clams;
			}
			continue;
		}
		const FString K = FString::Printf(TEXT("oceanWorld.nodes.%s."), *N.Item.ToString());
		const FName Give(*T.Str(K + TEXT("give"), N.Item.ToString()));
		AWRResourceNode* R = SpawnNode(Give, N.At, int32(T.Num(K + TEXT("hits"), 3.f)), T.Num(K + TEXT("perHit"), 1.f), T.Num(K + TEXT("respawn"), 3600.f));
		if (!R) continue;
		++Nodes;
		const bool bUnder = N.bUnder;
		const FName Item = N.Item;
		const float SeedChance = T.Num(K + TEXT("seedChance"), 0.f);
		const FName Seed(*T.Str(K + TEXT("seed"), FString()));
		TWeakObjectPtr<AWRResourceNode> WeakR(R);
		// Tidecoral only breaks off alive, under the water (it dies into white stone in air: cook 2.7).
		if (bUnder && Item == TEXT("tidecoral"))
		{
			R->CanHarvest = [WeakR](AWRCharacterBase* Who)
			{
				const AWRResourceNode* Node = WeakR.Get();
				if (!Node) return false;
				if (WRUnderwater::WaterAt(Node->GetWorld(), Node->GetActorLocation() + FVector(0, 0, 60.f))) return true;
				Tell(Who, TEXT("Out of the water the coral has died into white stone: it breaks off only alive, under the sea"));
				return false;
			};
		}
		// The King Tide pays x3 (cook 2.10, 3.3 SPEC) on every node of the coast.
		R->YieldScale = [](AWRCharacterBase* Who)
		{
			const AWROceanState* S = Who ? AWROceanState::Get(Who) : nullptr;
			return S && S->KingTide != EWRKingTide::None ? WRTune(TEXT("tide.kingTide.payout"), 3.f) : 1.f;
		};
		R->OnHarvested = [Item, SeedChance, Seed](AWRCharacterBase* Who)
		{
			// The wild crops give their seeds now and then; every hit is a quest beat (Hae-won's fourth counts coral).
			if (SeedChance > 0.f && !Seed.IsNone() && FMath::FRand() < SeedChance)
			{
				if (UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Who)) Inv->AddItem(Seed, 1);
			}
			QuestEvent(Who, TEXT("gather"), Item, 1);
		};
	}
	// ---- the spots and caches (cook 2.4, 2.5, 2.20) from ocean.json ----
	const TSharedPtr<FJsonObject> File = FWRTuning::LoadDataFile(TEXT("ocean.json"));
	const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
	int32 NSpots = 0, NCaches = 0;
	if (File.IsValid() && File->TryGetArrayField(TEXT("spots"), A))
	{
		for (const auto& V : *A)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			const FVector At = FVector(O->GetNumberField(TEXT("x")), O->GetNumberField(TEXT("y")), O->GetNumberField(TEXT("z"))) * 100.0 + FVector(0, 0, 60.f);
			AWROceanSpot* S = SpawnSpot(FName(*O->GetStringField(TEXT("kind"))), FName(*O->GetStringField(TEXT("id"))), At, O->GetStringField(TEXT("title")), O->GetStringField(TEXT("verb")), float(O->GetNumberField(TEXT("seconds"))));
			if (!S) continue;
			++NSpots;
			const TSharedPtr<FJsonObject>* G = nullptr;
			if (O->TryGetObjectField(TEXT("gives"), G)) for (const auto& KV : (*G)->Values) S->Gives.Add(FName(*KV.Key), int32(KV.Value->AsNumber()));
			S->Lore = O->GetStringField(TEXT("lore"));
			S->RespawnHours = float(O->GetNumberField(TEXT("respawnHours")));
		}
	}
	if (File.IsValid() && File->TryGetArrayField(TEXT("caches"), A))
	{
		for (const auto& V : *A)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			const FVector At = FVector(O->GetNumberField(TEXT("x")), O->GetNumberField(TEXT("y")), O->GetNumberField(TEXT("z"))) * 100.0;
			TMap<FName, int32> Items;
			const TSharedPtr<FJsonObject>* I = nullptr;
			if (O->TryGetObjectField(TEXT("items"), I)) for (const auto& KV : (*I)->Values) Items.Add(FName(*KV.Key), int32(KV.Value->AsNumber()));
			if (SpawnCache(At, float(O->GetNumberField(TEXT("yaw"))), O->GetStringField(TEXT("title")), Items, O->GetStringField(TEXT("lore")), float(O->GetNumberField(TEXT("refillHours"))))) ++NCaches;
		}
	}
	// The castaway hulls and drowned houses each keep a little (cook 2.4 kit: "a cache"); the anchors' sunk barrels too.
	for (const FWROceanData::FPlace& K : D.Kit)
	{
		const bool bHull = K.Kind == TEXT("castaway_hull"), bHouse = K.Kind == TEXT("drowned_house"), bAnchor = K.Kind == TEXT("anchor_chain");
		if (!bHull && !bHouse && !bAnchor) continue;
		TMap<FName, int32> Items;
		if (bHull) { Items.Add(TEXT("rope"), 2); Items.Add(TEXT("linen"), 2); Items.Add(TEXT("iron_nails"), 2); }
		else if (bHouse) { Items.Add(TEXT("salt"), 3); Items.Add(TEXT("clay"), 2); Items.Add(TEXT("sea_glass"), 2); }
		else { Items.Add(TEXT("iron_nails"), 3); Items.Add(TEXT("rope"), 2); }
		if (SpawnCache(K.At + FVector(120.f, 0.f, bHouse ? 60.f : 20.f), K.Yaw, bHull ? TEXT("A castaway's sea chest") : bHouse ? TEXT("A Saltkin jar") : TEXT("A sunk barrel"), Items,
			bHull ? TEXT("Ship's biscuit gone to green, and a carved name.") : FString(), T.Num(TEXT("oceanWorld.kitRefillHours"), 96.f))) ++NCaches;
	}
	// ---- the secrets O-S1..S6 (cook 2.20): a spot each, open only in its window or with what it needs ----
	int32 NSecrets = 0;
	for (const FWROceanData::FSecret& Sec : D.Secrets)
	{
		const FName K = Sec.Kind;
		const FString Title = K == TEXT("bell_window") ? TEXT("A window in the bell") : K == TEXT("dyke_vault") ? TEXT("A sluice-keeper's vault")
			: K == TEXT("horn_ledge") ? TEXT("A ledge over the Horn") : K == TEXT("mirror_line") ? TEXT("A mirror's sighting stone")
			: K == TEXT("lazarus_ledge") ? TEXT("A ledge in the dark water") : TEXT("The Low Gate");
		const FString Verb = K == TEXT("dyke_vault") ? TEXT("Turn the key") : K == TEXT("low_gate") ? TEXT("Go through") : K == TEXT("mirror_line") ? TEXT("Set the mirror on the line") : TEXT("Search");
		AWROceanSpot* S = SpawnSpot(TEXT("secret"), Sec.Id, Sec.At + FVector(0, 0, 60.f), Title, Verb, K == TEXT("low_gate") ? 0.f : 2.f);
		if (!S) continue;
		S->When = Sec.When;
		S->Needs = Sec.Needs;
		S->Gives = Sec.Pays;
		S->RespawnHours = 0.f;
		S->Lore = K == TEXT("bell_window") ? TEXT("At the top of the flood the window stands just clear of the water, and the moon comes through onto a shelf nobody could reach.")
			: K == TEXT("dyke_vault") ? TEXT("The key turns. The keeper kept the town's last good things here, above the sluice he never opened.")
			: K == TEXT("horn_ledge") ? TEXT("Old footholds cut in the rock, and a harpooner's cache wedged in a crack.")
			: K == TEXT("mirror_line") ? TEXT("At noon the stone's groove points straight at the Pound. Someone sat here and aimed the light by hand.")
			: K == TEXT("lazarus_ledge") ? TEXT("Your Pearldiver finds the seam in the dark. Behind it, a drowned diver's kit.")
			: TEXT("The Low Gate stands out of the sea. The Drowned Hall's way is open while the water is out.");
		++NSecrets;
	}
	UE_LOG(LogWildreach, Log, TEXT("OCEAN server: %d nodes, %d clams, %d spots, %d caches, %d secrets"), Nodes, Clams, NSpots, NCaches, NSecrets);
}

void UWROceanWorld::StepLights(float Dt)
{
	// Night blocks (phosphor paint, lamp niches, the clams' mantles, the Noctiluca surf) show from dusk; in the caves and
	// the Hall they're always on (it's dark down there).
	LightClock -= Dt;
	if (LightClock > 0.f) return;
	LightClock = 0.5f;
	const bool bNight = AWRGameState::IsNightIn(GetWorld());
	for (const TWeakObjectPtr<AWROceanScenery>& W : Scenery)
	{
		AWROceanScenery* S = W.Get();
		if (!S) continue;
		const bool bLit = bNight || S->Kind == TEXT("cave") || S->Kind == TEXT("hall");
		for (int32 i = 0; i < S->NightLights.Num(); ++i)
		{
			if (UPointLightComponent* L = S->NightLights[i]) L->SetIntensity(bLit ? (S->NightPower.IsValidIndex(i) ? S->NightPower[i] : 1000.f) : 0.f);
		}
		if (S->NightRoot && S->NightRoot->IsVisible() != bLit) S->NightRoot->SetVisibility(bLit, true);
	}
}

void UWROceanWorld::StepServer(float Dt)
{
	ServerClock += Dt;
	if (ServerClock < 2.f) return;
	ServerClock = 0.f;
	const double Hours = GameHours(this);
	for (const TWeakObjectPtr<AWRWorldCache>& W : Caches)
	{
		AWRWorldCache* C = W.Get();
		if (C && C->bTaken && C->RefillHours > 0.f && C->RefillAtHours > 0.0 && Hours >= C->RefillAtHours) C->Refill();
	}
}

void UWROceanWorld::TickLocalWater(float Dt)
{
	// The camera under the surface: a blue grade and a vignette blended in (the murk itself is the sky's fog, which
	// WRUnderwater::ApplyFog thickens while the local head is under).
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	const bool bUnder = Me && WRUnderwater::IsUnder(Me);
	UnderBlend = FMath::FInterpTo(UnderBlend, bUnder ? 1.f : 0.f, Dt, 6.f);
	if (!UnderPost && UnderBlend > 0.001f)
	{
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		WaterLook = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SP);
		if (WaterLook)
		{
			USceneComponent* Root = NewObject<USceneComponent>(WaterLook);
			WaterLook->SetRootComponent(Root);
			Root->RegisterComponent();
			UnderPost = NewObject<UPostProcessComponent>(WaterLook);
			UnderPost->SetupAttachment(Root);
			UnderPost->bUnbound = true;
			UnderPost->Priority = 50.f;
			FPostProcessSettings& S = UnderPost->Settings;
			S.bOverride_ColorGain = true;
			S.ColorGain = FVector4(0.72f, 0.92f, 1.0f, 1.f);
			S.bOverride_ColorSaturation = true;
			S.ColorSaturation = FVector4(0.85f, 0.95f, 1.0f, 1.f);
			S.bOverride_VignetteIntensity = true;
			S.VignetteIntensity = 0.7f;
			S.bOverride_SceneFringeIntensity = true;
			S.SceneFringeIntensity = 1.2f;
			UnderPost->RegisterComponent();
		}
	}
	if (UnderPost) UnderPost->BlendWeight = UnderBlend;
}

void UWROceanWorld::Tick(float DeltaTime)
{
	StepBuild();
	if (!bSceneryDone) return;
	if (GetWorld()->GetNetMode() != NM_DedicatedServer)
	{
		StepLights(DeltaTime);
		TickLocalWater(DeltaTime);
	}
	if (GetWorld()->GetNetMode() != NM_Client) StepServer(DeltaTime);
}

// ================================================================================================ interact

AWROceanSpot* UWROceanWorld::SpotNear(const FVector& P, float Range) const
{
	AWROceanSpot* Best = nullptr;
	float BestD = TNumericLimits<float>::Max();
	for (const TWeakObjectPtr<AWROceanSpot>& W : Spots)
	{
		AWROceanSpot* S = W.Get();
		if (!S) continue;
		const float R = Range > 0.f ? Range : S->Reach;
		const float D = FVector::DistSquared(S->GetActorLocation(), P);
		if (D < R * R && D < BestD) { BestD = D; Best = S; }
	}
	// Clients: the spots arrive by replication and aren't in this list (only the server spawned them).
	if (!Best && GetWorld()->GetNetMode() == NM_Client)
	{
		for (TActorIterator<AWROceanSpot> It(GetWorld()); It; ++It)
		{
			const float R = Range > 0.f ? Range : It->Reach;
			const float D = FVector::DistSquared(It->GetActorLocation(), P);
			if (D < R * R && D < BestD) { BestD = D; Best = *It; }
		}
	}
	return Best;
}

AWROceanSpot* UWROceanWorld::FindSpot(FName Id) const
{
	for (const TWeakObjectPtr<AWROceanSpot>& W : Spots) if (W.IsValid() && W->SpotId == Id) return W.Get();
	return nullptr;
}

bool UWROceanWorld::Prompt(const AWRPlayerController* PC, FString& OutTarget, FString& OutVerb)
{
	const UWROceanWorld* W = Get(PC);
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	if (!W || !W->bActive || !Me) return false;
	// A harpoon line in hand by a mooring stone: tie it off (the Reef Tyrant's tether, a Whaler's harpoon's catch).
	FVector Ring;
	if (W->TieRing(Me, Ring))
	{
		OutTarget = TEXT("A mooring stone");
		OutVerb = TEXT("Tie the line off");
		return true;
	}
	if (const AWROceanSpot* S = W->SpotNear(Me->GetActorLocation()))
	{
		OutTarget = S->Title;
		OutVerb = S->DoneAt > 0.f ? FString::Printf(TEXT("%s..."), *S->Verb) : S->Verb;
		return true;
	}
	return false;
}

bool UWROceanWorld::ClientInteract(AWRPlayerController* PC)
{
	const UWROceanWorld* W = Get(PC);
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	if (!W || !W->bActive || !Me) return false;
	FVector Ring;
	if (W->TieRing(Me, Ring)) { PC->SendCommand(TEXT("world ocean tie")); return true; }
	if (W->SpotNear(Me->GetActorLocation())) { PC->SendCommand(TEXT("world ocean spot")); return true; }
	return false;
}

FString UWROceanWorld::Command(AWRPlayerController* PC, const TArray<FString>& Args)
{
	UWROceanWorld* W = Get(PC);
	AWRCharacterBase* Me = PC ? Cast<AWRCharacterBase>(PC->GetPawn()) : nullptr;
	if (!W || !W->bActive || !Me) return TEXT("Not here");
	const FString Verb = Args.IsValidIndex(2) ? Args[2] : FString();
	if (Verb == TEXT("tie"))
	{
		FVector Ring;
		AWRHarpoonLine* L = AWRHarpoonLine::HeldBy(Me);
		if (!L || !W->TieRing(Me, Ring)) return TEXT("Nothing to tie off here");
		L->TieTo(Ring);
		WRSound::PlayAt(Me, TEXT("tide.rope_creak"), Ring);
		QuestEvent(Me, TEXT("event"), TEXT("line_tied"), 1);
		if (AWRCreature* C = Cast<AWRCreature>(L->Struck)) UE_LOG(LogWildreach, Log, TEXT("OCEAN %s tied %s off on a mooring stone"), *Me->GetName(), *C->GetName());
		return TEXT("The line's tied off on the stone: whatever's on the end of it is going nowhere");
	}
	if (Verb == TEXT("spot"))
	{
		if (AWROceanSpot* S = W->SpotNear(Me->GetActorLocation())) return S->Start(PC);
		return TEXT("Nothing here");
	}
	return TEXT("world ocean spot");
}

bool UWROceanWorld::TieRing(const APawn* Who, FVector& OutRing) const
{
	const AWRHarpoonLine* L = Who ? AWRHarpoonLine::HeldBy(Who) : nullptr;
	if (!L || L->bTied || !L->Struck) return false;
	const float Reach = Tn(TEXT("oceanWorld.tieReach"), 300.f);
	for (const FVector& M : FWROceanData::Get().MooringStones)
	{
		if (FVector::Dist2D(M, Who->GetActorLocation()) < Reach && FMath::Abs(M.Z - Who->GetActorLocation().Z) < 400.f)
		{
			OutRing = M + FVector(0.f, 0.f, 120.f);
			return true;
		}
	}
	return false;
}
