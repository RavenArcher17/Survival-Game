// NPC strongholds: see WRStronghold.h (Aaron 11-16, r-siege-pve and q-stronghold-garrison; cooks/wild-answers-1115.md 2.7,
// cooks/build-wild-strongholds.md).

#include "WRStronghold.h"
#include "Wildreach.h"
#include "WRBoss.h"
#include "WRAttributeSet.h"
#include "WRBuilding.h"
#include "WRBuildIntegrity.h"
#include "WRCreature.h"
#include "WRCharacterBase.h"
#include "WRGameState.h"
#include "WRGarrison.h"
#include "WRInventory.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRRaiding.h"
#include "WRStore.h"
#include "WRTuning.h"
#include "WRFx.h"
#include "WRSound.h"
#include "WRWorldDressing.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
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

	FVector VecField(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		return O.IsValid() && O->TryGetArrayField(Key, A) ? Vec(*A) : FVector::ZeroVector;
	}

	FVector XYZ(const TSharedPtr<FJsonObject>& O)
	{
		return FVector(O->GetNumberField(TEXT("x")), O->GetNumberField(TEXT("y")), O->GetNumberField(TEXT("z")));
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
			O->TryGetStringField(TEXT("faction"), D.Faction);
			FString Biome;
			if (O->TryGetStringField(TEXT("biome"), Biome)) D.Biome = FName(*Biome);
			D.Centre = VecField(O, TEXT("centre"));
			double N = 0.0;
			if (O->TryGetNumberField(TEXT("yaw"), N)) D.Yaw = float(N);
			if (O->TryGetNumberField(TEXT("clearRadius"), N)) D.ClearRadius = float(N);
			const TSharedPtr<FJsonObject>* Ward = nullptr;
			if (O->TryGetObjectField(TEXT("ward"), Ward))
			{
				D.WardCentre = VecField(*Ward, TEXT("centre"));
				const FVector Half = VecField(*Ward, TEXT("half"));
				D.WardHalf = FVector2D(Half.X, Half.Y);
			}
			const TSharedPtr<FJsonObject>* Runback = nullptr;
			if (O->TryGetObjectField(TEXT("runback"), Runback))
			{
				D.RunbackAt = VecField(*Runback, TEXT("at"));
				double Y = 0.0;
				if ((*Runback)->TryGetNumberField(TEXT("yaw"), Y)) D.RunbackYaw = float(Y);
			}
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
					HP.At = XYZ(P);
					HP.Yaw = float(P->GetNumberField(TEXT("yaw")));
					P->TryGetBoolField(TEXT("embed"), HP.bEmbed);
					P->TryGetBoolField(TEXT("turret"), HP.bTurret);
					const FVector In = VecField(P, TEXT("inward"));
					HP.Inward = FVector(In.X, In.Y, 0.f).GetSafeNormal();
					D.Pieces.Add(HP);
				}
			}
			const TArray<TSharedPtr<FJsonValue>>* Ladders = nullptr;
			if (O->TryGetArrayField(TEXT("ladders"), Ladders))
			{
				for (const TSharedPtr<FJsonValue>& LV : *Ladders)
				{
					const TSharedPtr<FJsonObject> L = LV->AsObject();
					if (!L.IsValid()) continue;
					FWRHoldLadder HL;
					HL.Foot = XYZ(L);
					HL.Yaw = float(L->GetNumberField(TEXT("yaw")));
					HL.Length = float(L->GetNumberField(TEXT("length")));
					HL.FootOut = float(L->GetNumberField(TEXT("footOut")));
					HL.Against = int32(L->GetNumberField(TEXT("against")));
					D.Ladders.Add(HL);
				}
			}
			const TArray<TSharedPtr<FJsonValue>>* Garrison = nullptr;
			if (O->TryGetArrayField(TEXT("garrison"), Garrison))
			{
				for (const TSharedPtr<FJsonValue>& GV : *Garrison)
				{
					const TSharedPtr<FJsonObject> G = GV->AsObject();
					if (!G.IsValid()) continue;
					FWRHoldSoldier S;
					S.Id = FName(*G->GetStringField(TEXT("id")));
					S.Rank = FName(*G->GetStringField(TEXT("rank")));
					G->TryGetStringField(TEXT("name"), S.Name);
					S.At = XYZ(G);
					S.Yaw = float(G->GetNumberField(TEXT("yaw")));
					FString Str;
					if (G->TryGetStringField(TEXT("weapon"), Str)) S.Weapon = FName(*Str);
					if (G->TryGetStringField(TEXT("metal"), Str)) S.Metal = FName(*Str);
					if (G->TryGetStringField(TEXT("bow"), Str)) S.Bow = FName(*Str);
					if (G->TryGetStringField(TEXT("sex"), Str)) S.Sex = FName(*Str);
					if (G->TryGetStringField(TEXT("warlord"), Str)) S.Warlord = FName(*Str);
					G->TryGetBoolField(TEXT("shield"), S.bShield);
					double Num = 0.0;
					if (G->TryGetNumberField(TEXT("face"), Num)) S.Face = int32(Num);
					if (G->TryGetNumberField(TEXT("build"), Num)) S.Build = int32(Num);
					const TArray<TSharedPtr<FJsonValue>>* Armor = nullptr;
					if (G->TryGetArrayField(TEXT("armor"), Armor))
					{
						for (int32 i = 0; i < 4 && i < Armor->Num(); ++i)
						{
							const FString A = (*Armor)[i]->AsString();
							S.Armor[i] = A.IsEmpty() ? NAME_None : FName(*A);
						}
					}
					const TArray<TSharedPtr<FJsonValue>>* Route = nullptr;
					if (G->TryGetArrayField(TEXT("route"), Route))
					{
						for (const TSharedPtr<FJsonValue>& RV : *Route) S.Route.Add(Vec(RV->AsArray()));
					}
					D.Garrison.Add(S);
				}
			}
			const TArray<TSharedPtr<FJsonValue>>* Hounds = nullptr;
			if (O->TryGetArrayField(TEXT("hounds"), Hounds))
			{
				for (const TSharedPtr<FJsonValue>& HV : *Hounds)
				{
					const TSharedPtr<FJsonObject> H = HV->AsObject();
					if (!H.IsValid()) continue;
					FWRHoldHounds Hd;
					Hd.Creature = FName(*H->GetStringField(TEXT("creature")));
					Hd.Count = int32(H->GetNumberField(TEXT("count")));
					Hd.At = VecField(H, TEXT("at"));
					D.Hounds.Add(Hd);
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
	bool IsFloorRole(FName Role) { return Role == TEXT("keep_floor") || Role == TEXT("ward_floor") || Role == TEXT("walk") || Role == TEXT("keep_upper") || Role == TEXT("keep_roof") || Role == TEXT("keep_landing"); }
	/** The piece the fort's state shows on: lit / hung while it's held, gone once it's taken. */
	bool IsHeldSign(FName Role) { return Role == TEXT("beacon") || Role == TEXT("banner"); }

	void Tell(UWorld* World, const FVector& At, float Range, const FString& Text)
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			AWRPlayerController* PC = Cast<AWRPlayerController>(It->Get());
			const APawn* Me = PC ? PC->GetPawn() : nullptr;
			if (Me && FVector::Dist2D(Me->GetActorLocation(), At) < Range) PC->ClientNotify(Text);
		}
	}

	/** Where a piece stands in the world for its row (its ground or floor, sunk a little if it's a ground piece). */
	float PieceZ(const FWRHoldPiece& P, float DZ)
	{
		return P.At.Z + DZ - (P.bEmbed ? Tn(TEXT("embedCm"), 25.f) : 0.f);
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

FName FWRStrongholds::HoldOf(const AActor* Actor)
{
	if (const AWRGarrison* G = Cast<AWRGarrison>(Actor)) return G->HoldId;
	if (const AWRCreature* C = Cast<AWRCreature>(Actor))
	{
		for (const FName& Tag : C->Tags)
		{
			const FString S = Tag.ToString();
			if (S.StartsWith(TEXT("@hold:"))) return FName(*S.Mid(6));
		}
	}
	return NAME_None;
}

const FWRHoldPiece* FWRStrongholds::RowOf(const AWRBuildPiece* Piece, const FWRHoldDef** OutFort)
{
	if (!IsHoldPiece(Piece)) return nullptr;
	const FWRHoldDef* D = Find(FName(*Piece->OwnerName.Mid(6)));
	if (!D) return nullptr;
	if (OutFort) *OutFort = D;
	const FVector At = Piece->GetActorLocation();
	const FWRHoldPiece* Best = nullptr;
	float BestD = 80.f;
	for (const FWRHoldPiece& P : D->Pieces)
	{
		if (P.Piece != Piece->PieceId) continue;
		const float Dist = FVector::Dist2D(P.At, At);
		if (Dist < BestD && FMath::Abs(P.At.Z - At.Z) < 400.f) { BestD = Dist; Best = &P; }
	}
	return Best;
}

bool FWRStrongholds::IsInside(const AWRBuildPiece* Door, const APawn* By)
{
	const FWRHoldPiece* Row = RowOf(Door);
	if (!Row || Row->Inward.IsNearlyZero() || !By) return false;
	return FVector::DotProduct(By->GetActorLocation() - Door->GetActorLocation(), Row->Inward) > 0.f;
}

FString FWRStrongholds::DoorPrompt(const AWRBuildPiece* Door, const APawn* By)
{
	const FWRHoldPiece* Row = RowOf(Door);
	const bool bBarDoor = Row && !Row->Inward.IsNearlyZero();
	if (!bBarDoor) return TEXT("Barred: break it down");
	if (!Door->bLocked) return Door->bOpen ? TEXT("Close") : TEXT("Open");
	return IsInside(Door, By) ? TEXT("Lift the bar") : TEXT("Barred from the other side");
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

UWRStrongholdSubsystem::FFort* UWRStrongholdSubsystem::FortOf(FName Hold, const FWRHoldDef** OutDef)
{
	const FWRHoldDef* D = FWRStrongholds::Find(Hold);
	if (OutDef) *OutDef = D;
	return D ? Forts.Find(Hold) : nullptr;
}

void UWRStrongholdSubsystem::ClearDressing()
{
	// Every machine: the island's decoration inside a fort's ground goes (trees through the palisade, a boulder in the
	// keep), as the generator keeps places clear. The dressing is built at world start; wait for it.
	AWRDressing* Dressing = UWRDressingSubsystem::FindDressing(GetWorld());
	if (!Dressing || Dressing->GetInstanceCount() <= 0) return;
	UStaticMesh* Rock = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/SimpleRocks/3D_Assets/Meshes/Rocks/SM_SimpleRock_09A.SM_SimpleRock_09A"));
	UMaterialInterface* RockMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_Rock.M_Rock"));
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
		// The breach's rubble (the side path): the fallen wall's stones, low and flat across the gap, walkable, the
		// same on every machine (it's decoration, so each machine lays its own).
		if (!Rock) continue;
		const FRotator Frame(0.f, D.Yaw, 0.f);
		const FVector Local[] = { FVector(-1180.f, 1060.f, 0.f), FVector(-940.f, 1130.f, 0.f), FVector(-700.f, 1050.f, 0.f), FVector(-500.f, 1140.f, 0.f), FVector(-860.f, 960.f, 0.f) };
		for (int32 i = 0; i < UE_ARRAY_COUNT(Local); ++i)
		{
			FVector At = D.Centre + Frame.RotateVector(Local[i]);
			FHitResult Hit;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(WRHoldRubble), false);
			if (GetWorld()->LineTraceSingleByChannel(Hit, At + FVector(0.f, 0.f, 3000.f), At - FVector(0.f, 0.f, 3000.f), ECC_WorldStatic, Q)) At = Hit.ImpactPoint;
			else At.Z = D.Centre.Z;
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (AStaticMeshActor* A = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), At - FVector(0.f, 0.f, 20.f), FRotator(0.f, D.Yaw + 37.f * i, 0.f), Params))
			{
				A->SetReplicates(false);
				A->SetMobility(EComponentMobility::Movable);
				A->GetStaticMeshComponent()->SetStaticMesh(Rock);
				if (RockMat) A->GetStaticMeshComponent()->SetMaterial(0, RockMat);
				const FBox B = Rock->GetBoundingBox();
				const FVector S = B.GetSize();
				const float Wide = 120.f + 25.f * (i % 3), Tall = 35.f + 12.f * (i % 2);
				A->SetActorScale3D(FVector(Wide / FMath::Max(1.f, S.X), Wide * 0.8f / FMath::Max(1.f, S.Y), Tall / FMath::Max(1.f, S.Z)));
			}
		}
	}
	bDressingCleared = true;
}

void UWRStrongholdSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World) return;
	if (!bDressingCleared) ClearDressing();
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
		F.Unbarred.Reset();
		F.DeadUntil.Reset();
		F.FallenAt = -1.0;
		F.bWarlordDown = false;
		F.AlarmUntil = 0.0;
		F.bHasSavedChest = false;
		F.SavedChest.Reset();
	}
	// The ground under the centre now against the data's (the landscape is the heightmap's, so this is ~0): every piece
	// moves by the same amount, so the keep's and the ward's floor stay shared and level.
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
		// Taken: its beacon is out and its banners are down.
		if (F.bWarlordDown && IsHeldSign(P.Role)) continue;
		if (!FWRBuildRegistry::Get().Find(P.Piece)) continue;
		const float Z = PieceZ(P, DZ);
		const FTransform T(FRotator(0.f, P.Yaw, 0.f), FVector(P.At.X, P.At.Y, Z));
		AWRBuildPiece* Piece = World->SpawnActorDeferred<AWRBuildPiece>(AWRBuildPiece::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Piece) continue;
		Piece->InitPiece(P.Piece, Owner);
		Piece->Support = 0;
		const FWRPieceDef* Def = Piece->GetDef();
		// The land's building set stands at kitHealthScale (old walls, poorly kept: a trebuchet's work, not a day's).
		Piece->ScaleHealth(Tn(TEXT("healthScale"), 1.f) * (Def && !Def->Tier.IsNone() ? Tn(TEXT("kitHealthScale"), 0.5f) : 1.f));
		if (Def && Def->Kind == TEXT("door"))
		{
			// Barred from the inside: no code opens it. The keep's door comes down to a ram, a charge or a stone; the gate and
			// the postern can also be unbarred from their inside (E).
			Piece->bLocked = !F.Unbarred.Contains(i);
			Piece->LockCode = FGuid::NewGuid().ToString();
		}
		if (P.Role == TEXT("war_chest"))
		{
			Piece->CustomName = FString::Printf(TEXT("%s war chest"), *D.Name);
			Piece->bLocked = F.FallenAt < 0.0;   // barred until the keep is breached or the warlord falls
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
		if (P.bTurret)
		{
			// A bolt turret on the tower's deck, facing out of the fort.
			const FTransform TT(FRotator(0.f, D.Yaw, 0.f), FVector(P.At.X, P.At.Y, Z + Tn(TEXT("deckTopCm"), 520.f)));
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
	// The fixed ladders (towers, the wall walk, the keep's landing, the postern's step): they fall with what they lean on.
	F.Ladders.Reset();
	for (const FWRHoldLadder& L : D.Ladders)
	{
		AWRBuildPiece* Against = F.Pieces.IsValidIndex(L.Against) ? F.Pieces[L.Against].Get() : nullptr;
		if (!Against) continue;
		const FTransform LT(FRotator(0.f, L.Yaw, 0.f), L.Foot + FVector(0.f, 0.f, DZ - 5.f));
		AWRSiegeLadder* Ladder = World->SpawnActorDeferred<AWRSiegeLadder>(AWRSiegeLadder::StaticClass(), LT, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Ladder) continue;
		Ladder->Length = L.Length;
		Ladder->FootOut = L.FootOut;
		Ladder->Health = Tn(TEXT("ladderHealth"), 600.f);
		Ladder->Against = Against;
		Ladder->Placer = Owner;
		Ladder->DieAt = TNumericLimits<float>::Max();
		Ladder->FinishSpawning(LT);
		F.Ladders.Add(Ladder);
	}
	// The run-back stone outside the postern (Elden Ring's stake): wake there after a death in the ward.
	if (!F.Runback.IsValid() && !D.RunbackAt.IsNearlyZero())
	{
		FVector At = D.RunbackAt;
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRHoldRunback), false);
		if (World->LineTraceSingleByChannel(Hit, At + FVector(0.f, 0.f, 3000.f), At - FVector(0.f, 0.f, 3000.f), ECC_WorldStatic, Q)) At = Hit.ImpactPoint;
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FRotator Face = (D.WardCentre - At).GetSafeNormal2D().Rotation();
		if (AWRRunbackStone* Stone = World->SpawnActor<AWRRunbackStone>(AWRRunbackStone::StaticClass(), At, FRotator(0.f, Face.Yaw, 0.f), Params))
		{
			Stone->BossId = D.Id;
			F.Runback = Stone;
		}
	}
	F.bBuilt = true;
	TopUpGarrison(D, F, true);
	if (F.FallenAt < 0.0) TopUpHounds(D, F);
	UE_LOG(LogWildreach, Log, TEXT("HOLD %s built (%d pieces, %d broken, %d ladders, %d soldiers%s)"), *D.Id.ToString(), D.Pieces.Num() - F.Broken.Num(), F.Broken.Num(), F.Ladders.Num(), F.Soldiers.Num(), F.FallenAt >= 0.0 ? TEXT(", fallen") : TEXT(""));
}

AWRGarrison* UWRStrongholdSubsystem::SpawnSoldier(const FWRHoldDef& D, const FWRHoldSoldier& Row)
{
	UWorld* World = GetWorld();
	const FTransform T(FRotator(0.f, Row.Yaw, 0.f), Row.At + FVector(0.f, 0.f, WRHoldDetail::Tn(TEXT("spawnLift"), 100.f)));
	AWRGarrison* G = World->SpawnActorDeferred<AWRGarrison>(AWRGarrison::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!G) return nullptr;
	G->Setup(D.Id, Row, D.Faction);
	G->FinishSpawning(T);
	return G;
}

void UWRStrongholdSubsystem::TopUpGarrison(const FWRHoldDef& D, FFort& F, bool bAll)
{
	const double H = Hours();
	for (const FWRHoldSoldier& Row : D.Garrison)
	{
		const TWeakObjectPtr<AWRGarrison>* Have = F.Soldiers.Find(Row.Id);
		if (Have && Have->IsValid() && (*Have)->IsAlive()) continue;
		// The fallen warlord stays fallen until the fort stands again; a dead soldier comes back after its hours.
		if (!Row.Warlord.IsNone() && F.bWarlordDown) continue;
		if (const double* Until = F.DeadUntil.Find(Row.Id))
		{
			if (H < *Until) continue;
		}
		else if (!bAll && Have)
		{
			// Gone without a death noted (it fell out of the world, a rebuild of its post): it comes back like the dead.
			F.DeadUntil.Add(Row.Id, H + WRHoldDetail::Tn(TEXT("respawnHours"), 2.f));
			continue;
		}
		if (AWRGarrison* G = SpawnSoldier(D, Row))
		{
			F.Soldiers.Add(Row.Id, G);
			F.DeadUntil.Remove(Row.Id);
		}
	}
}

void UWRStrongholdSubsystem::TopUpHounds(const FWRHoldDef& D, FFort& F)
{
	UWorld* World = GetWorld();
	F.Hounds.RemoveAll([](const TWeakObjectPtr<AWRCreature>& C) { return !C.IsValid() || !C->IsAlive(); });
	int32 Wanted = 0;
	for (const FWRHoldHounds& Row : D.Hounds) Wanted += Row.Count;
	if (F.Hounds.Num() >= Wanted) return;
	TMap<FName, int32> Have;
	for (const TWeakObjectPtr<AWRCreature>& C : F.Hounds) Have.FindOrAdd(C->CreatureId)++;
	int32 k = 0;
	for (const FWRHoldHounds& Row : D.Hounds)
	{
		for (int32 n = Have.FindRef(Row.Creature); n < Row.Count; ++n, ++k)
		{
			// Curled up in the kennel by the breach, a little apart.
			FVector At = Row.At + FRotator(0.f, D.Yaw + 120.f * k, 0.f).RotateVector(FVector(110.f, 0.f, 0.f));
			FHitResult Hit;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(WRHoldSpawn), false);
			if (World->LineTraceSingleByChannel(Hit, At + FVector(0.f, 0.f, 3000.f), At - FVector(0.f, 0.f, 3000.f), ECC_WorldStatic, Q)) At = Hit.ImpactPoint + FVector(0.f, 0.f, 120.f);
			if (AWRCreature* C = AWRCreature::SpawnCreature(World, Row.Creature, At, FRotator(0.f, D.Yaw + 60.f * k, 0.f)))
			{
				C->SetHome(Row.At);
				C->Tags.Add(FName(*FWRStrongholds::OwnerFor(D.Id)));
				C->SetAsleep(true);
				F.Hounds.Add(C);
			}
		}
	}
}

void UWRStrongholdSubsystem::Step(const FWRHoldDef& D, FFort& F)
{
	using namespace WRHoldDetail;
	UWorld* World = GetWorld();
	// What has come down since the last look (stale pointers); what stood on it comes down with it.
	bool bKeepBreached = false, bDoorBroke = false;
	for (int32 i = 0; i < F.Pieces.Num(); ++i)
	{
		if (F.Broken.Contains(i) || F.Pieces[i].IsValid()) continue;
		const FWRHoldPiece& P = D.Pieces[i];
		// A sign taken down by the fort's fall isn't "broken" (the rebuild hangs it again).
		if (F.bWarlordDown && IsHeldSign(P.Role)) continue;
		F.Broken.Add(i);
		if (TWeakObjectPtr<AWRBuildPiece>* T = P.bTurret ? F.Turrets.Find(i) : nullptr; T && T->IsValid())
		{
			FWRIntegrity::Collapse(T->Get());   // the turret on its deck falls with the tower
		}
		if (IsFloorRole(P.Role))
		{
			// What stood on that floor's cell falls with it.
			for (int32 j = 0; j < F.Pieces.Num(); ++j)
			{
				const FWRHoldPiece& Q = D.Pieces[j];
				if (j == i || !F.Pieces[j].IsValid() || IsFloorRole(Q.Role)) continue;
				if (FVector::Dist2D(Q.At, P.At) < 290.f && Q.At.Z >= P.At.Z - 10.f && Q.At.Z <= P.At.Z + 320.f) FWRIntegrity::Collapse(F.Pieces[j].Get());
			}
		}
		if (P.Role == TEXT("gate") || P.Role == TEXT("keep_door")) bDoorBroke = true;
	}
	for (int32 i : F.Broken) bKeepBreached |= IsKeepRole(D.Pieces[i].Role) || D.Pieces[i].Role == TEXT("keep_floor");
	// The gate or the keep's door broken in: the sally (the whole fort turns out on whoever is there).
	if (bDoorBroke && F.FallenAt < 0.0)
	{
		AActor* Near = nullptr;
		float Best = 6000.f;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APawn* Pw = It->Get() ? It->Get()->GetPawn() : nullptr;
			const float Dd = Pw ? FVector::Dist2D(Pw->GetActorLocation(), D.Centre) : TNumericLimits<float>::Max();
			if (Dd < Best) { Best = Dd; Near = Pw; }
		}
		SoundAlarm(D.Id, Near ? Near->GetActorLocation() : D.Centre, Near, false);
	}
	if (bKeepBreached && F.FallenAt < 0.0) Fall(D, F, FString::Printf(TEXT("%s's keep is breached: its war chest is open"), *D.Name));
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
	const bool bQuiet = !PlayerWithin(D.Centre, Tn(TEXT("quietRange"), 15000.f));
	if (!bQuiet) F.LastNear = Hours();
	// Standing: its dead come back after their hours, and its hounds, while nobody is near (never in front of you).
	if (F.FallenAt < 0.0 && bQuiet)
	{
		TopUpGarrison(D, F, false);
		TopUpHounds(D, F);
	}
	// Fallen, or broken anywhere, and left alone long enough: it stands again, whole.
	if ((F.FallenAt >= 0.0 || F.Broken.Num() > 0) && bQuiet && Hours() - FMath::Max(F.LastNear, F.FallenAt) >= Tn(TEXT("rebuildHours"), 12.f)) Rebuild(D, F);
}

void UWRStrongholdSubsystem::Fall(const FWRHoldDef& D, FFort& F, const FString& Why)
{
	using namespace WRHoldDetail;
	F.FallenAt = Hours();
	for (const TWeakObjectPtr<AWRBuildPiece>& W : F.Pieces)
	{
		if (W.IsValid() && W->bLocked && FWRStrongholds::IsBarredChest(W.Get())) { W->bLocked = false; W->ForceNetUpdate(); }
	}
	Tell(GetWorld(), D.Centre, Tn(TEXT("tellRange"), 20000.f), Why);
	// A crash of dust where the keep gave (heard and seen by everyone near).
	for (const TWeakObjectPtr<AWRBuildPiece>& W : F.Pieces)
	{
		if (!W.IsValid()) continue;
		const FVector Keep = D.Centre + FRotator(0.f, D.Yaw, 0.f).RotateVector(FVector(-920.f, 0.f, 300.f));
		WRFx::Broadcast(W.Get(), TEXT("build.collapse"), Keep, FRotator::ZeroRotator, 2.f, TEXT("raid.blast"));
		break;
	}
	UE_LOG(LogWildreach, Log, TEXT("HOLD %s fell (%s)"), *D.Id.ToString(), *Why);
}

void UWRStrongholdSubsystem::Rebuild(const FWRHoldDef& D, FFort& F)
{
	for (TWeakObjectPtr<AWRBuildPiece>& W : F.Pieces) if (W.IsValid()) W->Destroy();
	for (TPair<int32, TWeakObjectPtr<AWRBuildPiece>>& T : F.Turrets) if (T.Value.IsValid()) T.Value->Destroy();
	for (TWeakObjectPtr<AWRSiegeLadder>& L : F.Ladders) if (L.IsValid()) L->Destroy();
	for (TWeakObjectPtr<AWRCreature>& C : F.Hounds) if (C.IsValid()) C->Destroy();
	for (TPair<FName, TWeakObjectPtr<AWRGarrison>>& S : F.Soldiers) if (S.Value.IsValid()) S.Value->Destroy();
	F.Pieces.Reset();
	F.Turrets.Reset();
	F.Ladders.Reset();
	F.Hounds.Reset();
	F.Soldiers.Reset();
	Build(D, F, true);
	UE_LOG(LogWildreach, Log, TEXT("HOLD %s stands again"), *D.Id.ToString());
}

void UWRStrongholdSubsystem::Unbar(const FWRHoldDef& D, FFort& F, int32 Index, APawn* By)
{
	AWRBuildPiece* Door = F.Pieces.IsValidIndex(Index) ? F.Pieces[Index].Get() : nullptr;
	if (!Door) return;
	F.Unbarred.Add(Index);
	Door->bLocked = false;
	FString Why;
	Door->SwingFor(By, true, Why);
	Door->ForceNetUpdate();
	WRSound::Broadcast(Door, TEXT("hold.bar"));
	UE_LOG(LogWildreach, Log, TEXT("HOLD %s: %s unbarred by %s"), *D.Id.ToString(), *D.Pieces[Index].Role.ToString(), *GetNameSafe(By));
}

FString UWRStrongholdSubsystem::UseDoor(AWRBuildPiece* Door, APawn* By)
{
	const FWRHoldDef* D = nullptr;
	const FWRHoldPiece* Row = FWRStrongholds::RowOf(Door, &D);
	FFort* F = D ? Forts.Find(D->Id) : nullptr;
	if (!Row || !F) return TEXT("Barred from the inside: break it down (a ram, a charge, a trebuchet stone)");
	if (Row->Inward.IsNearlyZero()) return TEXT("Barred from the inside: break it down (a ram, a charge, a trebuchet stone)");
	const int32 Index = int32(Row - D->Pieces.GetData());
	if (!Door->bLocked)
	{
		FString Why;
		if (!Door->SwingFor(By, !Door->bOpen, Why)) return Why;
		return Door->bOpen ? TEXT("Opened") : TEXT("Closed");
	}
	if (!FWRStrongholds::IsInside(Door, By)) return TEXT("Barred from the other side");
	Unbar(*D, *F, Index, By);
	return Row->Role == TEXT("gate") ? TEXT("You lift the gate's bar") : TEXT("You lift the bar: the way back is open");
}

void UWRStrongholdSubsystem::SoundAlarm(FName Hold, const FVector& Where, AActor* Seen, bool bHorn)
{
	const FWRHoldDef* D = nullptr;
	FFort* F = FortOf(Hold, &D);
	if (!F || !D) return;
	const double T = GetWorld()->GetTimeSeconds();
	const bool bFresh = T >= F->AlarmUntil;
	F->AlarmUntil = T + WRHoldDetail::Tn(TEXT("alarmSeconds"), 60.f);
	for (TPair<FName, TWeakObjectPtr<AWRGarrison>>& S : F->Soldiers)
	{
		if (S.Value.IsValid() && S.Value->IsAlive()) S.Value->Alarm(Where, Seen);
	}
	// The hounds are loosed.
	for (const TWeakObjectPtr<AWRCreature>& C : F->Hounds)
	{
		if (!C.IsValid() || !C->IsAlive()) continue;
		C->SetAsleep(false);
		if (Seen) C->SetAggroTarget(Seen);
	}
	if (bFresh) UE_LOG(LogWildreach, Log, TEXT("HOLD %s roused (%s)"), *Hold.ToString(), bHorn ? TEXT("the horn") : TEXT("a door broken"));
}

void UWRStrongholdSubsystem::Shout(FName Hold, const FVector& Where, AActor* Foe, float Range)
{
	const FWRHoldDef* D = nullptr;
	FFort* F = FortOf(Hold, &D);
	if (!F) return;
	for (TPair<FName, TWeakObjectPtr<AWRGarrison>>& S : F->Soldiers)
	{
		AWRGarrison* G = S.Value.Get();
		if (!G || !G->IsAlive() || G->GetTarget() || G->IsWarlord() || FVector::Dist(G->GetActorLocation(), Where) > Range) continue;
		G->Engage(Foe);
	}
	for (const TWeakObjectPtr<AWRCreature>& C : F->Hounds)
	{
		if (!C.IsValid() || !C->IsAlive() || FVector::Dist(C->GetActorLocation(), Where) > Range) continue;
		C->SetAsleep(false);
		if (Foe && !C->GetAggroTarget()) C->SetAggroTarget(Foe);
	}
}

void UWRStrongholdSubsystem::SoldierDied(AWRGarrison* Soldier, AActor* Killer)
{
	const FWRHoldDef* D = nullptr;
	FFort* F = Soldier ? FortOf(Soldier->HoldId, &D) : nullptr;
	if (!F) return;
	F->DeadUntil.Add(Soldier->GetSoldierId(), Hours() + WRHoldDetail::Tn(TEXT("respawnHours"), 2.f));
}

void UWRStrongholdSubsystem::WarlordDied(AWRGarrison* Warlord, AActor* Killer)
{
	using namespace WRHoldDetail;
	const FWRHoldDef* D = nullptr;
	FFort* F = Warlord ? FortOf(Warlord->HoldId, &D) : nullptr;
	if (!F || !D) return;
	const FWRWarlordDef* W = FWRGarrisonData::Get().Warlord(Warlord->GetWarlordId());
	F->bWarlordDown = true;
	F->DeadUntil.Remove(Warlord->GetSoldierId());
	// His pay to every fighter who did a tenth of his health (the boss ledger rule), and the first kill's prize, the
	// first time each survivor takes him.
	const float Needed = Warlord->GetAttributes() ? Warlord->GetAttributes()->GetMaxHealth() * WRTune(TEXT("bosses.lootDamageFraction"), 0.1f) : 0.f;
	const FName KillKey = FName(*FString::Printf(TEXT("hold_%s"), *Warlord->GetWarlordId().ToString()));
	for (APlayerState* P : GetWorld()->GetGameState()->PlayerArray)
	{
		AWRPlayerState* PS = Cast<AWRPlayerState>(P);
		const float* Dealt = PS ? Warlord->Ledger.Find(PS->GetPlayerName()) : nullptr;
		if (!Dealt || *Dealt < Needed) continue;
		if (W && PS->GetInventory()) for (const TPair<FName, int32>& L : W->Loot) if (FWRItemRegistry::Get().Find(L.Key)) PS->GetInventory()->AddItem(L.Key, L.Value);
		if (W && !W->FirstKill.IsNone() && !PS->BossKills.Contains(KillKey))
		{
			PS->BossKills.Add(KillKey);
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (AWRFirstKillDrop* Drop = GetWorld()->SpawnActor<AWRFirstKillDrop>(AWRFirstKillDrop::StaticClass(), Warlord->GetActorLocation(), FRotator::ZeroRotator, Params))
			{
				Drop->ForPawn = PS->GetPawn();
				Drop->Item = W->FirstKill;
				Drop->BossName = W->Name;
				Drop->OwnerName = PS->GetPlayerName();
			}
		}
	}
	// The fort is taken: the beacon goes dark and the banners come down (they hang again when it stands again).
	for (int32 i = 0; i < F->Pieces.Num(); ++i)
	{
		AWRBuildPiece* Piece = F->Pieces[i].Get();
		if (!Piece || !IsHeldSign(D->Pieces[i].Role)) continue;
		WRFx::Broadcast(Piece, TEXT("build.collapse"), Piece->GetActorLocation() + FVector(0.f, 0.f, 120.f), FRotator::ZeroRotator, 0.8f, NAME_None);
		Piece->Destroy();
	}
	WRSound::PlayAt(Warlord, TEXT("hold.taken"), Warlord->GetActorLocation());
	if (F->FallenAt < 0.0) Fall(*D, *F, FString::Printf(TEXT("%s is taken: %s has fallen and the war chest is open"), *D->Name, W ? *W->Name : TEXT("its warlord")));
	else Tell(GetWorld(), D->Centre, Tn(TEXT("tellRange"), 20000.f), FString::Printf(TEXT("%s is taken: %s has fallen"), *D->Name, W ? *W->Name : TEXT("its warlord")));
	F->FallenAt = Hours();
}

// ---------------------------------------------------------------- the world's save

void UWRStrongholdSubsystem::Save(UWorld* World)
{
	UWRStrongholdSubsystem* S = Get(World);
	if (!S) return;
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("version"), 2);
	const TSharedRef<FJsonObject> Forts = MakeShared<FJsonObject>();
	for (const FWRHoldDef& D : FWRStrongholds::All())
	{
		const FFort* F = S->Forts.Find(D.Id);
		if (!F || !F->bBuilt) continue;
		const TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> Broken, Unbarred;
		for (int32 i : F->Broken) Broken.Add(MakeShared<FJsonValueNumber>(i));
		for (int32 i : F->Unbarred) Unbarred.Add(MakeShared<FJsonValueNumber>(i));
		O->SetArrayField(TEXT("broken"), Broken);
		O->SetArrayField(TEXT("unbarred"), Unbarred);
		O->SetNumberField(TEXT("fallenAt"), F->FallenAt);
		O->SetBoolField(TEXT("warlordDown"), F->bWarlordDown);
		const TSharedRef<FJsonObject> Dead = MakeShared<FJsonObject>();
		for (const TPair<FName, double>& E : F->DeadUntil) Dead->SetNumberField(E.Key.ToString(), E.Value);
		O->SetObjectField(TEXT("dead"), Dead);
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
	// Version 1 forts were a different layout (a ring of 16 palisades): their piece indices mean nothing now, so they
	// start whole.
	double Version = 1.0;
	Root->TryGetNumberField(TEXT("version"), Version);
	if (Version < 2.0)
	{
		UE_LOG(LogWildreach, Log, TEXT("HOLD: the saved strongholds are the old layout: they stand whole"));
		return;
	}
	const TSharedPtr<FJsonObject>* Forts = nullptr;
	if (!Root->TryGetObjectField(TEXT("forts"), Forts)) return;
	for (const TPair<FString, TSharedPtr<FJsonValue>>& E : (*Forts)->Values)
	{
		const TSharedPtr<FJsonObject> O = E.Value->AsObject();
		const FWRHoldDef* D = FWRStrongholds::Find(FName(*E.Key));
		if (!O.IsValid() || !D) continue;
		FFort& F = S->Forts.FindOrAdd(FName(*E.Key));
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (O->TryGetArrayField(TEXT("broken"), Arr)) for (const TSharedPtr<FJsonValue>& V : *Arr) { const int32 i = int32(V->AsNumber()); if (D->Pieces.IsValidIndex(i)) F.Broken.Add(i); }
		if (O->TryGetArrayField(TEXT("unbarred"), Arr)) for (const TSharedPtr<FJsonValue>& V : *Arr) { const int32 i = int32(V->AsNumber()); if (D->Pieces.IsValidIndex(i)) F.Unbarred.Add(i); }
		O->TryGetNumberField(TEXT("fallenAt"), F.FallenAt);
		O->TryGetBoolField(TEXT("warlordDown"), F.bWarlordDown);
		const TSharedPtr<FJsonObject>* Dead = nullptr;
		if (O->TryGetObjectField(TEXT("dead"), Dead)) for (const TPair<FString, TSharedPtr<FJsonValue>>& DE : (*Dead)->Values) F.DeadUntil.Add(FName(*DE.Key), DE.Value->AsNumber());
		if (O->TryGetArrayField(TEXT("chest"), Arr)) { F.SavedChest = *Arr; F.bHasSavedChest = true; }
		F.LastNear = S->Hours();
	}
	UE_LOG(LogWildreach, Log, TEXT("HOLD loaded %d strongholds' state"), S->Forts.Num());
}
