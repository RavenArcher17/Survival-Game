// NPC strongholds: see WRStronghold.h (Aaron 11-16, r-siege-pve; cooks/wild-answers-1115.md 2.7).

#include "WRStronghold.h"
#include "Wildreach.h"
#include "WRBuilding.h"
#include "WRBuildIntegrity.h"
#include "WRCreature.h"
#include "WRCharacterBase.h"
#include "WRGameState.h"
#include "WRInventory.h"
#include "WRPlayerController.h"
#include "WRStore.h"
#include "WRTuning.h"
#include "WRFx.h"
#include "WRSound.h"
#include "WRWorldDressing.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace WRHoldDetail
{
	float Tn(const TCHAR* Key, float Def) { return WRTune(*(FString(TEXT("strongholds.")) + Key), Def); }

	FVector Vec(const TArray<TSharedPtr<FJsonValue>>& A)
	{
		return FVector(A.Num() > 0 ? A[0]->AsNumber() : 0.0, A.Num() > 1 ? A[1]->AsNumber() : 0.0, A.Num() > 2 ? A[2]->AsNumber() : 0.0);
	}

	TArray<FWRHoldDef> Parse()
	{
		TArray<FWRHoldDef> Out;
		const TSharedPtr<FJsonObject> J = FWRTuning::LoadDataFile(TEXT("strongholds.json"));
		const TArray<TSharedPtr<FJsonValue>>* Forts = nullptr;
		if (!J.IsValid() || !J->TryGetArrayField(TEXT("forts"), Forts))
		{
			UE_LOG(LogWildreach, Warning, TEXT("HOLD: Data/strongholds.json is missing (node Tools/terrain/strongholds.mjs)"));
			return Out;
		}
		for (const TSharedPtr<FJsonValue>& V : *Forts)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			if (!O.IsValid()) continue;
			FWRHoldDef D;
			D.Id = FName(*O->GetStringField(TEXT("id")));
			D.Name = O->GetStringField(TEXT("name"));
			O->TryGetStringField(TEXT("lore"), D.Lore);
			FString Biome;
			if (O->TryGetStringField(TEXT("biome"), Biome)) D.Biome = FName(*Biome);
			const TArray<TSharedPtr<FJsonValue>>* C = nullptr;
			if (O->TryGetArrayField(TEXT("centre"), C)) D.Centre = Vec(*C);
			double N = 0.0;
			if (O->TryGetNumberField(TEXT("yaw"), N)) D.Yaw = float(N);
			if (O->TryGetNumberField(TEXT("ringRadius"), N)) D.RingRadius = float(N);
			if (O->TryGetNumberField(TEXT("clearRadius"), N)) D.ClearRadius = float(N);
			const TArray<TSharedPtr<FJsonValue>>* Pieces = nullptr;
			if (O->TryGetArrayField(TEXT("pieces"), Pieces))
			{
				for (const TSharedPtr<FJsonValue>& PV : *Pieces)
				{
					const TSharedPtr<FJsonObject> P = PV->AsObject();
					if (!P.IsValid()) continue;
					FWRHoldPiece HP;
					HP.Piece = FName(*P->GetStringField(TEXT("piece")));
					HP.Role = FName(*P->GetStringField(TEXT("role")));
					HP.At = FVector(P->GetNumberField(TEXT("x")), P->GetNumberField(TEXT("y")), P->GetNumberField(TEXT("z")));
					HP.Yaw = float(P->GetNumberField(TEXT("yaw")));
					D.Pieces.Add(HP);
				}
			}
			const TArray<TSharedPtr<FJsonValue>>* Def = nullptr;
			if (O->TryGetArrayField(TEXT("defenders"), Def))
			{
				for (const TSharedPtr<FJsonValue>& DV : *Def)
				{
					const TSharedPtr<FJsonObject> Q = DV->AsObject();
					if (Q.IsValid()) D.Defenders.Add({ FName(*Q->GetStringField(TEXT("creature"))), int32(Q->GetNumberField(TEXT("count"))) });
				}
			}
			const TSharedPtr<FJsonObject>* Chest = nullptr;
			if (O->TryGetObjectField(TEXT("chest"), Chest))
			{
				for (const TPair<FString, TSharedPtr<FJsonValue>>& E : (*Chest)->Values) D.Chest.Add({ FName(*E.Key), int32(E.Value->AsNumber()) });
			}
			Out.Add(MoveTemp(D));
		}
		UE_LOG(LogWildreach, Log, TEXT("HOLD: %d strongholds"), Out.Num());
		return Out;
	}

	bool IsKeepRole(FName Role) { return Role == TEXT("keep_wall") || Role == TEXT("keep_door"); }
	bool OnFloor(FName Role) { return Role == TEXT("keep_wall") || Role == TEXT("keep_door") || Role == TEXT("war_chest"); }

	void Tell(UWorld* World, const FVector& At, float Range, const FString& Text)
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			AWRPlayerController* PC = Cast<AWRPlayerController>(It->Get());
			const APawn* Me = PC ? PC->GetPawn() : nullptr;
			if (Me && FVector::Dist2D(Me->GetActorLocation(), At) < Range) PC->ClientNotify(Text);
		}
	}
}

// ---------------------------------------------------------------- the data and the rules

const TArray<FWRHoldDef>& FWRStrongholds::All()
{
	static const TArray<FWRHoldDef> Forts = WRHoldDetail::Parse();
	return Forts;
}

const FWRHoldDef* FWRStrongholds::Find(FName Id)
{
	return All().FindByPredicate([Id](const FWRHoldDef& D) { return D.Id == Id; });
}

bool FWRStrongholds::IsHoldPiece(const AWRBuildPiece* Piece)
{
	return Piece && IsHoldOwner(Piece->OwnerName);
}

FString FWRStrongholds::DisplayName(const FString& Owner)
{
	if (!IsHoldOwner(Owner)) return Owner;
	const FWRHoldDef* D = Find(FName(*Owner.Mid(6)));
	return D ? D->Name : Owner.Mid(6);
}

const FWRHoldDef* FWRStrongholds::At(const FVector& Where, float Extra)
{
	for (const FWRHoldDef& D : All())
	{
		if (FVector::Dist2D(D.Centre, Where) < D.ClearRadius + Extra) return &D;
	}
	return nullptr;
}

FString FWRStrongholds::BuildBlock(const FVector& Where)
{
	const FWRHoldDef* D = At(Where, WRHoldDetail::Tn(TEXT("noBuildMargin"), 1500.f));
	return D ? FString::Printf(TEXT("%s stands here: no building near a stronghold"), *D->Name) : FString();
}

bool FWRStrongholds::IsBarredChest(const AWRBuildPiece* Piece)
{
	const FWRPieceDef* Def = Piece ? Piece->GetDef() : nullptr;
	return IsHoldPiece(Piece) && Def && Def->Kind == TEXT("chest") && Piece->bLocked;
}

// ---------------------------------------------------------------- the subsystem

bool UWRStrongholdSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	if (!World || !World->IsGameWorld()) return false;
	// The forts stand on the Old Island's landscape (their heights come from it).
	return World->GetMapName().Contains(FWRTuning::Get().Str(TEXT("strongholds.map"), TEXT("L_Island"))) && FWRStrongholds::All().Num() > 0;
}

TStatId UWRStrongholdSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWRStrongholdSubsystem, STATGROUP_Tickables);
}

UWRStrongholdSubsystem* UWRStrongholdSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UWRStrongholdSubsystem>() : nullptr;
}

double UWRStrongholdSubsystem::Hours() const
{
	const AWRGameState* GS = GetWorld() ? GetWorld()->GetGameState<AWRGameState>() : nullptr;
	return GS ? GS->GetTotalHours() : 0.0;
}

bool UWRStrongholdSubsystem::PlayerWithin(const FVector& At, float Range) const
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APawn* P = It->Get() ? It->Get()->GetPawn() : nullptr;
		if (P && FVector::Dist2D(P->GetActorLocation(), At) < Range) return true;
	}
	return false;
}

void UWRStrongholdSubsystem::ClearDressing()
{
	// Every machine: the island's decoration inside a fort's ground goes (trees through the palisade, a boulder in the
	// keep), as the generator keeps places clear. The dressing is built at world start; wait for it.
	AWRDressing* Dressing = UWRDressingSubsystem::FindDressing(GetWorld());
	if (!Dressing || Dressing->GetInstanceCount() <= 0) return;
	for (const FWRHoldDef& D : FWRStrongholds::All())
	{
		Dressing->ClearCircle(FVector2D(D.Centre), D.ClearRadius);
		// On the map and the discovery banner like the island's landmarks.
		if (!FWRPoi::All.ContainsByPredicate([&D](const FWRPoi& P) { return P.Name == D.Name; }))
		{
			FWRPoi& Poi = FWRPoi::All.AddDefaulted_GetRef();
			Poi.Name = D.Name;
			Poi.Kind = TEXT("landmark");
			Poi.Location = D.Centre;
		}
	}
	bDressingCleared = true;
}

void UWRStrongholdSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World) return;
	if (!bDressingCleared && World->GetNetMode() != NM_DedicatedServer) ClearDressing();
	if (!bDressingCleared && World->GetNetMode() == NM_DedicatedServer && UWRDressingSubsystem::FindDressing(World)) ClearDressing();
	if (World->GetNetMode() == NM_Client) return;
	Clock -= DeltaTime;
	if (Clock > 0.f) return;
	Clock = WRHoldDetail::Tn(TEXT("stepSeconds"), 1.f);
	for (const FWRHoldDef& D : FWRStrongholds::All())
	{
		FFort& F = Forts.FindOrAdd(D.Id);
		if (!F.bBuilt)
		{
			// Built when someone comes near and its ground has streamed in (World Partition loads the landscape round players).
			if (!PlayerWithin(D.Centre, WRHoldDetail::Tn(TEXT("buildRange"), 30000.f))) continue;
			FHitResult Hit;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(WRHoldGround), false);
			if (!World->LineTraceSingleByChannel(Hit, D.Centre + FVector(0.f, 0.f, 5000.f), D.Centre - FVector(0.f, 0.f, 5000.f), ECC_WorldStatic, Q)) continue;
			Build(D, F, false);
			continue;
		}
		Step(D, F);
	}
}

void UWRStrongholdSubsystem::Build(const FWRHoldDef& D, FFort& F, bool bFresh)
{
	using namespace WRHoldDetail;
	UWorld* World = GetWorld();
	if (bFresh)
	{
		F.Broken.Reset();
		F.FallenAt = -1.0;
		F.bHasSavedChest = false;
		F.SavedChest.Reset();
	}
	// The ground under the centre now against the data's (the landscape is the heightmap's, so this is ~0): every piece
	// moves by the same amount, so the keep's floor stays shared and level.
	float DZ = 0.f;
	{
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRHoldGround), false);
		if (World->LineTraceSingleByChannel(Hit, D.Centre + FVector(0.f, 0.f, 5000.f), D.Centre - FVector(0.f, 0.f, 5000.f), ECC_WorldStatic, Q))
		{
			DZ = FMath::Clamp(float(Hit.ImpactPoint.Z - D.Centre.Z), -300.f, 300.f);
		}
	}
	const FString Owner = FWRStrongholds::OwnerFor(D.Id);
	F.Pieces.SetNum(D.Pieces.Num());
	F.Turrets.Reset();
	for (int32 i = 0; i < D.Pieces.Num(); ++i)
	{
		const FWRHoldPiece& P = D.Pieces[i];
		if (F.Broken.Contains(i)) continue;
		if (!FWRBuildRegistry::Get().Find(P.Piece)) continue;
		// Ground pieces stand into the ground a little (no daylight under a post on a slope: Electric Dreams' embed);
		// the keep's foundations put their floor (100 above the piece) at the shared floor height; walls, the door and
		// the chest stand on that floor.
		float Z = P.At.Z + DZ;
		if (P.Role == TEXT("palisade") || P.Role == TEXT("gate") || P.Role == TEXT("tower")) Z -= Tn(TEXT("embedCm"), 25.f);
		else if (P.Role == TEXT("keep_floor")) Z -= 100.f;
		const FTransform T(FRotator(0.f, P.Yaw, 0.f), FVector(P.At.X, P.At.Y, Z));
		AWRBuildPiece* Piece = World->SpawnActorDeferred<AWRBuildPiece>(AWRBuildPiece::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Piece) continue;
		Piece->InitPiece(P.Piece, Owner);
		Piece->Support = 0;
		Piece->ScaleHealth(Tn(TEXT("healthScale"), 1.f));
		const FWRPieceDef* Def = Piece->GetDef();
		if (Def && Def->Kind == TEXT("door"))
		{
			// Barred from the inside: no code opens it; it comes down to a ram, a charge or a stone.
			Piece->bLocked = true;
			Piece->LockCode = FGuid::NewGuid().ToString();
		}
		if (P.Role == TEXT("war_chest"))
		{
			Piece->CustomName = FString::Printf(TEXT("%s war chest"), *D.Name);
			Piece->bLocked = F.FallenAt < 0.0;   // barred until the keep is breached
			if (F.bHasSavedChest)
			{
				for (int32 s = 0; s < F.SavedChest.Num() && s < Piece->Storage.Num(); ++s)
				{
					const TSharedPtr<FJsonObject> SO = F.SavedChest[s].IsValid() ? F.SavedChest[s]->AsObject() : nullptr;
					if (SO.IsValid()) Piece->Storage[s] = FWRInventoryRules::StackFromJson(SO);
				}
			}
			else
			{
				int32 s = 0;
				for (const TPair<FName, int32>& C : D.Chest)
				{
					if (!Piece->Storage.IsValidIndex(s)) break;
					FWRItemStack& Stack = Piece->Storage[s++];
					Stack.ItemId = C.Key;
					Stack.Count = C.Value;
				}
			}
		}
		Piece->FinishSpawning(T);
		F.Pieces[i] = Piece;
		if (P.Role == TEXT("tower"))
		{
			// A bolt turret on the tower's deck (the deck's top is 520 up), facing out of the fort.
			const FVector Out = (FVector(P.At.X, P.At.Y, 0.f) - FVector(D.Centre.X, D.Centre.Y, 0.f)).GetSafeNormal2D();
			const FTransform TT(Out.Rotation(), FVector(P.At.X, P.At.Y, Z + Tn(TEXT("deckTopCm"), 520.f)));
			AWRBuildPiece* Turret = World->SpawnActorDeferred<AWRBuildPiece>(AWRBuildPiece::StaticClass(), TT, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (Turret)
			{
				Turret->InitPiece(TEXT("bolt_turret"), Owner);
				Turret->Support = 0;
				Turret->Mach.bOn = true;
				Turret->Mach.Recipe = TEXT("crossbow_bolt");
				Turret->Mach.FuelLeft = 99.f;
				Turret->FinishSpawning(TT);
				F.Turrets.Add(i, Turret);
			}
		}
	}
	F.bBuilt = true;
	if (F.FallenAt < 0.0) TopUpDefenders(D, F, true);
	UE_LOG(LogWildreach, Log, TEXT("HOLD %s built (%d pieces, %d broken%s)"), *D.Id.ToString(), D.Pieces.Num() - F.Broken.Num(), F.Broken.Num(), F.FallenAt >= 0.0 ? TEXT(", fallen") : TEXT(""));
}

void UWRStrongholdSubsystem::TopUpDefenders(const FWRHoldDef& D, FFort& F, bool bAll)
{
	UWorld* World = GetWorld();
	F.Defenders.RemoveAll([](const TWeakObjectPtr<AWRCreature>& C) { return !C.IsValid() || !C->IsAlive(); });
	// The yard between the gate and the keep, out of each other's way.
	const FRotator Face(0.f, D.Yaw, 0.f);
	int32 Wanted = 0, k = 0;
	for (const TPair<FName, int32>& Row : D.Defenders) Wanted += Row.Value;
	if (!bAll && F.Defenders.Num() >= Wanted) return;
	TMap<FName, int32> Have;
	for (const TWeakObjectPtr<AWRCreature>& C : F.Defenders) Have.FindOrAdd(C->CreatureId)++;
	for (const TPair<FName, int32>& Row : D.Defenders)
	{
		for (int32 n = Have.FindRef(Row.Key); n < Row.Value; ++n, ++k)
		{
			const FVector Local(FMath::Lerp(150.f, 650.f, (k % 3) / 2.f), (k % 2 ? 1.f : -1.f) * (150.f + 120.f * (k / 3)), 0.f);
			FVector At = D.Centre + Face.RotateVector(Local);
			FHitResult Hit;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(WRHoldSpawn), false);
			if (World->LineTraceSingleByChannel(Hit, At + FVector(0.f, 0.f, 3000.f), At - FVector(0.f, 0.f, 3000.f), ECC_WorldStatic, Q)) At = Hit.ImpactPoint + FVector(0.f, 0.f, 150.f);
			if (AWRCreature* C = AWRCreature::SpawnCreature(World, Row.Key, At, FRotator(0.f, D.Yaw + 180.f, 0.f)))
			{
				C->SetHome(D.Centre + Face.RotateVector(FVector(400.f, 0.f, 0.f)));
				C->Tags.Add(FName(*FWRStrongholds::OwnerFor(D.Id)));
				F.Defenders.Add(C);
			}
		}
	}
}

void UWRStrongholdSubsystem::Step(const FWRHoldDef& D, FFort& F)
{
	using namespace WRHoldDetail;
	UWorld* World = GetWorld();
	// What has come down since the last look (stale pointers); what stood on it comes down with it.
	bool bKeepBreached = false;
	for (int32 i = 0; i < F.Pieces.Num(); ++i)
	{
		if (F.Broken.Contains(i) || F.Pieces[i].IsValid()) continue;
		F.Broken.Add(i);
		const FWRHoldPiece& P = D.Pieces[i];
		if (TWeakObjectPtr<AWRBuildPiece>* T = P.Role == TEXT("tower") ? F.Turrets.Find(i) : nullptr; T && T->IsValid())
		{
			FWRIntegrity::Collapse(T->Get());   // the turret on its deck falls with the tower
		}
		if (P.Role == TEXT("keep_floor"))
		{
			// The walls, door and chest on that foundation's cell fall with it.
			for (int32 j = 0; j < F.Pieces.Num(); ++j)
			{
				if (OnFloor(D.Pieces[j].Role) && F.Pieces[j].IsValid() && FVector::Dist2D(D.Pieces[j].At, P.At) < 290.f) FWRIntegrity::Collapse(F.Pieces[j].Get());
			}
		}
	}
	for (int32 i : F.Broken) bKeepBreached |= IsKeepRole(D.Pieces[i].Role) || D.Pieces[i].Role == TEXT("keep_floor");
	// The keep breached: the war chest opens, and everyone near hears it (once).
	if (bKeepBreached && F.FallenAt < 0.0)
	{
		F.FallenAt = Hours();
		for (const TWeakObjectPtr<AWRBuildPiece>& W : F.Pieces)
		{
			if (W.IsValid() && W->bLocked && FWRStrongholds::IsBarredChest(W.Get())) { W->bLocked = false; W->ForceNetUpdate(); }
		}
		Tell(World, D.Centre, Tn(TEXT("tellRange"), 20000.f), FString::Printf(TEXT("%s's keep is breached: its war chest is open"), *D.Name));
		// A crash of dust where the keep gave (heard and seen by everyone near).
		for (const TWeakObjectPtr<AWRBuildPiece>& W : F.Pieces)
		{
			if (!W.IsValid()) continue;
			const FVector Keep = D.Centre + FRotator(0.f, D.Yaw, 0.f).RotateVector(FVector(-20.f, -200.f, 150.f));
			WRFx::Broadcast(W.Get(), TEXT("build.collapse"), Keep, FRotator::ZeroRotator, 2.f, TEXT("raid.blast"));
			break;
		}
		UE_LOG(LogWildreach, Log, TEXT("HOLD %s fell (keep breached)"), *D.Id.ToString());
	}
	// Its name and story to anyone who comes up to it the first time (this session).
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		AWRPlayerController* PC = Cast<AWRPlayerController>(It->Get());
		const APawn* Me = PC ? PC->GetPawn() : nullptr;
		if (!Me || !PC->PlayerState || FVector::Dist2D(Me->GetActorLocation(), D.Centre) > Tn(TEXT("storyRange"), 9000.f)) continue;
		if (F.Told.Contains(PC->PlayerState->GetPlayerName())) continue;
		F.Told.Add(PC->PlayerState->GetPlayerName());
		PC->ClientNotify(FString::Printf(TEXT("%s. %s"), *D.Name, *D.Lore));
	}
	// Standing: its beasts come back to it while nobody is near (a fight in progress doesn't refill the yard).
	const bool bQuiet = !PlayerWithin(D.Centre, Tn(TEXT("quietRange"), 15000.f));
	if (F.FallenAt < 0.0 && bQuiet) TopUpDefenders(D, F, false);
	// Fallen and left alone long enough: it stands again.
	if (F.FallenAt >= 0.0 && Hours() - F.FallenAt >= Tn(TEXT("rebuildHours"), 12.f) && bQuiet) Rebuild(D, F);
}

void UWRStrongholdSubsystem::Rebuild(const FWRHoldDef& D, FFort& F)
{
	for (TWeakObjectPtr<AWRBuildPiece>& W : F.Pieces) if (W.IsValid()) W->Destroy();
	for (TPair<int32, TWeakObjectPtr<AWRBuildPiece>>& T : F.Turrets) if (T.Value.IsValid()) T.Value->Destroy();
	for (TWeakObjectPtr<AWRCreature>& C : F.Defenders) if (C.IsValid()) C->Destroy();
	F.Pieces.Reset();
	F.Turrets.Reset();
	F.Defenders.Reset();
	Build(D, F, true);
	UE_LOG(LogWildreach, Log, TEXT("HOLD %s stands again"), *D.Id.ToString());
}

// ---------------------------------------------------------------- the world's save

void UWRStrongholdSubsystem::Save(UWorld* World)
{
	UWRStrongholdSubsystem* S = Get(World);
	if (!S) return;
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("version"), 1);
	const TSharedRef<FJsonObject> Forts = MakeShared<FJsonObject>();
	for (const FWRHoldDef& D : FWRStrongholds::All())
	{
		const FFort* F = S->Forts.Find(D.Id);
		if (!F || !F->bBuilt) continue;
		const TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> Broken;
		for (int32 i : F->Broken) Broken.Add(MakeShared<FJsonValueNumber>(i));
		O->SetArrayField(TEXT("broken"), Broken);
		O->SetNumberField(TEXT("fallenAt"), F->FallenAt);
		for (int32 i = 0; i < F->Pieces.Num(); ++i)
		{
			const AWRBuildPiece* P = F->Pieces[i].Get();
			if (!P || D.Pieces[i].Role != TEXT("war_chest")) continue;
			TArray<TSharedPtr<FJsonValue>> Chest;
			for (const FWRItemStack& St : P->Storage) Chest.Add(MakeShared<FJsonValueObject>(FWRInventoryRules::StackToJson(St)));
			O->SetArrayField(TEXT("chest"), Chest);
		}
		Forts->SetObjectField(D.Id.ToString(), O);
	}
	Root->SetObjectField(TEXT("forts"), Forts);
	FString Text;
	FJsonSerializer::Serialize(Root, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text));
	WRStore::WriteDoc(TEXT("strongholds"), Text);
}

void UWRStrongholdSubsystem::Load(UWorld* World)
{
	UWRStrongholdSubsystem* S = Get(World);
	FString Text;
	TSharedPtr<FJsonObject> Root;
	if (!S || !WRStore::ReadDoc(TEXT("strongholds"), Text) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid()) return;
	const TSharedPtr<FJsonObject>* Forts = nullptr;
	if (!Root->TryGetObjectField(TEXT("forts"), Forts)) return;
	for (const TPair<FString, TSharedPtr<FJsonValue>>& E : (*Forts)->Values)
	{
		const TSharedPtr<FJsonObject> O = E.Value->AsObject();
		if (!O.IsValid() || !FWRStrongholds::Find(FName(*E.Key))) continue;
		FFort& F = S->Forts.FindOrAdd(FName(*E.Key));
		const TArray<TSharedPtr<FJsonValue>>* Broken = nullptr;
		if (O->TryGetArrayField(TEXT("broken"), Broken)) for (const TSharedPtr<FJsonValue>& V : *Broken) F.Broken.Add(int32(V->AsNumber()));
		O->TryGetNumberField(TEXT("fallenAt"), F.FallenAt);
		const TArray<TSharedPtr<FJsonValue>>* Chest = nullptr;
		if (O->TryGetArrayField(TEXT("chest"), Chest)) { F.SavedChest = *Chest; F.bHasSavedChest = true; }
	}
	UE_LOG(LogWildreach, Log, TEXT("HOLD loaded %d strongholds' state"), S->Forts.Num());
}
