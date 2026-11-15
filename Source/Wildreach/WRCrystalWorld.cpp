// The Crystal caves' world (see WRCrystalWorld.h; cooks/build-biome-crystal.md 2.0-2.7).

#include "WRCrystalWorld.h"
#include "Wildreach.h"
#include "WRCrystalCaves.h"
#include "WRCrystalTypes.h"
#include "WRGameState.h"
#include "WRLand.h"
#include "WRTuning.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

UWRCrystalWorld* UWRCrystalWorld::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UWRCrystalWorld>() : nullptr;
}

bool UWRCrystalWorld::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UWRCrystalWorld::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	FString Map = UWorld::RemovePIEPrefix(InWorld.GetMapName());
	Map.RemoveFromStart(InWorld.StreamingLevelsPrefix);
	if (!Map.EndsWith(FWRTuning::Get().Str(TEXT("crystalWorld.map"), TEXT("L_Island"))) || !WRCrystal::Live()) return;
	if (!FWRCrystalData::Get().bLoaded || !FWRCrystalData::Land())
	{
		UE_LOG(LogWildreach, Warning, TEXT("CRYSTAL is live but Data/crystal.json or Data/lands/crystal.json didn't load: nothing of the Crystal caves is built"));
		return;
	}
	bActive = true;
	// The rock: every machine (the server for its collision, clients for the look too). It needs no ground chunks: the
	// ground's height is the land's function.
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	CavesActor = InWorld.SpawnActor<AWRCrystalCaves>(AWRCrystalCaves::StaticClass(), FTransform::Identity, P);
	UE_LOG(LogWildreach, Log, TEXT("CRYSTAL live on %s: the caves are building, the scenery waits for the ground"), *Map);
}

void UWRCrystalWorld::BuildScenery()
{
	// Every place in a fixed order (the same on every machine, so each builds the same collision): the sites (camps,
	// landmarks, descents, hand sites, the Vault's rooms), the kit, the domes, the vents, the chime stones, the halls'
	// and tubes' furniture, then the plateau's columns and grass by 200 m cell.
	const FWRCrystalData& D = FWRCrystalData::Get();
	for (const FWRCrystalData::FSite& S : D.Sites) Pieces.Add({ S.Kind, S.Id, S.At, S.Yaw, -1, S.bCave });
	for (int32 i = 0; i < D.Kit.Num(); ++i) Pieces.Add({ D.Kit[i].Kind, D.Kit[i].Id, D.Kit[i].At, 0.f, i, D.Kit[i].bCave });
	for (int32 i = 0; i < D.Domes.Num(); ++i) Pieces.Add({ TEXT("dome"), D.Domes[i].Id, D.Domes[i].At, 0.f, i, false });
	for (int32 i = 0; i < D.Vents.Num(); ++i) Pieces.Add({ TEXT("vent"), D.Vents[i].Id, FVector(D.Vents[i].At.X, D.Vents[i].At.Y, D.Vents[i].Z1), 0.f, i, false });
	for (int32 i = 0; i < D.Chimes.Num(); ++i) Pieces.Add({ TEXT("chime"), NAME_None, D.Chimes[i].At, D.Chimes[i].Yaw, i, !D.Chimes[i].bSurface });
	for (int32 i = 0; i < D.Halls.Num(); ++i) Pieces.Add({ TEXT("hall"), D.Halls[i].Id, D.Halls[i].At, D.Halls[i].Yaw, i, true });
	for (int32 i = 0; i < D.Tubes.Num(); ++i)
	{
		const FWRCrystalData::FTube& T = D.Tubes[i];
		if (T.Pts.Num() && (T.Kind == TEXT("passage") || T.Id == TEXT("organ") || T.Kind == TEXT("ramp"))) Pieces.Add({ TEXT("tube"), T.Id, T.Pts[0], 0.f, i, true });
	}
	for (int32 i = 0; i < D.Shafts.Num(); ++i)
	{
		const FWRCrystalData::FShaft& S = D.Shafts[i];
		if (S.Kind == TEXT("stair")) Pieces.Add({ TEXT("severance_stair"), S.Id, FVector(S.At.X, S.At.Y, S.Z1), 0.f, i, true });
	}
	// The plateau's decoration in 200 m cells over the land's frame (its own seeded scatter: WRCrystalScenery).
	if (const FWRLandDef* L = FWRCrystalData::Land())
	{
		const float Span = (L->HfN - 1) * L->HfStep;
		const int32 N = FMath::CeilToInt(Span / 200.f);
		for (int32 j = 0; j < N; ++j) for (int32 i = 0; i < N; ++i)
		{
			const FVector2D M = L->HfOrigin + FVector2D((i + 0.5f) * 200.f, (j + 0.5f) * 200.f);
			if (L->CoastDistance(M) < -200.f && L->HfHiddenAt(M)) continue;
			Pieces.Add({ TEXT("cell"), NAME_None, L->ToWorld(M, L->Height(M)), 0.f, j * 1000 + i, false });
		}
	}
	UE_LOG(LogWildreach, Log, TEXT("CRYSTAL scenery queued: %d places"), Pieces.Num());
}

void UWRCrystalWorld::StepBuild()
{
	UWorld* World = GetWorld();
	if (!World) return;
	if (!bQueued)
	{
		WaitClock += World->GetDeltaSeconds();
		const FWRLandDef* Def = FWRCrystalData::Land();
		AWRLand* Land = Def ? UWRLandSubsystem::FindLand(World, Def->Id) : nullptr;
		if (!Land || !Land->IsGroundReady())
		{
			if (WaitClock > 60.f) { WaitClock = 0.f; UE_LOG(LogWildreach, Warning, TEXT("CRYSTAL still waiting for its land's ground (Data/lands/crystal.json)")); }
			return;
		}
		BuildScenery();
		bQueued = true;
	}
	if (bSceneryDone) return;
	const int32 PerFrame = FMath::Max(1, int32(WRTune(TEXT("crystalWorld.buildPerFrame"), 4.f)));
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (int32 n = 0; n < PerFrame && PiecesBuilt < Pieces.Num(); ++n, ++PiecesBuilt)
	{
		const FPiece& Pc = Pieces[PiecesBuilt];
		AWRCrystalScenery* A = World->SpawnActor<AWRCrystalScenery>(AWRCrystalScenery::StaticClass(), FTransform(FRotator(0.f, Pc.Yaw, 0.f), Pc.At), P);
		if (!A) continue;
		A->Kind = Pc.Kind;
		A->SiteId = Pc.Id;
		A->Index = Pc.Index;
		A->bUnder = Pc.bUnder;
		A->BuildCrystal();
		Scenery.Add(A);
	}
	if (PiecesBuilt < Pieces.Num()) return;
	bSceneryDone = true;
	Pieces.Empty();
	LightClock = 0.f;
	UE_LOG(LogWildreach, Log, TEXT("CRYSTAL scenery built: %d places"), Scenery.Num());
}

void UWRCrystalWorld::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	if (!World) return;
	if (!bSceneryDone)
	{
		StepBuild();
		if (!bSceneryDone) return;
	}
	if (World->GetNetMode() != NM_DedicatedServer) StepLights(DeltaTime);
	// The live half (WRCrystalThings.cpp): the server's nodes, chimes and keepsakes once, its hearing, Saga's day and
	// night; every machine with a view lights the crystals.
	if (World->GetNetMode() != NM_Client)
	{
		if (!bServerDone) { bServerDone = true; SpawnServerActors(); SpawnHazards(); }
		StepServerNoise(DeltaTime);
		StepPools(DeltaTime);
		StepVenn(DeltaTime);
	}
	StepResonance(DeltaTime);
	if (World->GetNetMode() != NM_DedicatedServer) StepLight(DeltaTime);
}

void UWRCrystalWorld::StepLights(float Dt)
{
	// Night blocks (the domes' violet, the Crown's columns, camp fires) show from dusk; under the ground it is always
	// night (the caves' lamps burn all day).
	LightClock -= Dt;
	if (LightClock > 0.f) return;
	LightClock = 0.5f;
	const bool bNight = AWRGameState::IsNightIn(GetWorld());
	for (const TWeakObjectPtr<AWRCrystalScenery>& W : Scenery)
	{
		AWRCrystalScenery* S = W.Get();
		if (!S) continue;
		const bool bLit = bNight || S->bUnder;
		for (int32 i = 0; i < S->NightLights.Num(); ++i)
		{
			if (UPointLightComponent* L = S->NightLights[i]) L->SetIntensity(bLit ? (S->NightPower.IsValidIndex(i) ? S->NightPower[i] : 1000.f) : 0.f);
		}
		if (S->NightRoot && S->NightRoot->IsVisible() != bLit) S->NightRoot->SetVisibility(bLit, true);
	}
}
