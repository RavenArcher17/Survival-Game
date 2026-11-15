// The Fungal caverns' world (see WRFungalWorld.h; cooks/build-biome-fungal.md 2.0-2.7).

#include "WRFungalWorld.h"
#include "Wildreach.h"
#include "WRFungalCaves.h"
#include "WRFungalLook.h"
#include "WRFungalTypes.h"
#include "WRForestHazards.h"
#include "WRGameState.h"
#include "WRLand.h"
#include "WRTuning.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"

UMaterialInterface* WRFungalLook::GlowMaterial(const UObject* WorldContext)
{
	if (UWRFungalWorld* W = UWRFungalWorld::Get(WorldContext)) if (W->GlowMID()) return W->GlowMID();
	return LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_FungalGlow.M_FungalGlow"));
}

UMaterialInterface* WRFungalLook::VeilMaterial(const UObject* WorldContext)
{
	if (UWRFungalWorld* W = UWRFungalWorld::Get(WorldContext)) if (W->VeilMID()) return W->VeilMID();
	return LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_FungalVeil.M_FungalVeil"));
}

float WRFungal::GlowNow(const UObject* WorldContext)
{
	const UWRFungalWorld* W = UWRFungalWorld::Get(WorldContext);
	return W ? W->GlowLevel() : 0.6f;
}

UWRFungalWorld* UWRFungalWorld::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UWRFungalWorld>() : nullptr;
}

bool UWRFungalWorld::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UWRFungalWorld::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	FString Map = UWorld::RemovePIEPrefix(InWorld.GetMapName());
	Map.RemoveFromStart(InWorld.StreamingLevelsPrefix);
	if (!Map.EndsWith(FWRTuning::Get().Str(TEXT("fungalWorld.map"), TEXT("L_Island"))) || !WRFungal::Live()) return;
	if (!FWRFungalData::Get().bLoaded || !FWRFungalData::Land())
	{
		UE_LOG(LogWildreach, Warning, TEXT("FUNGAL is live but Data/fungal.json or Data/lands/fungal.json didn't load: nothing of the Fungal caverns is built"));
		return;
	}
	bActive = true;
	if (InWorld.GetNetMode() != NM_DedicatedServer)
	{
		// One glowing and one see-through instance for every fungal thing (the circadian curve reaches them all at once).
		if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_FungalGlow.M_FungalGlow")))
		{
			Glow = UMaterialInstanceDynamic::Create(M, this);
			Glow->SetVectorParameterValue(TEXT("GlowColor"), FLinearColor::White);
			Glow->SetScalarParameterValue(TEXT("Shimmer"), WRTune(TEXT("fungalWorld.glow.shimmer"), 0.25f));
		}
		if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_FungalVeil.M_FungalVeil")))
		{
			Veil = UMaterialInstanceDynamic::Create(M, this);
			Veil->SetVectorParameterValue(TEXT("Color"), FLinearColor::White);
			Veil->SetScalarParameterValue(TEXT("Opacity"), WRTune(TEXT("fungalWorld.veil.opacity"), 0.5f));
			Veil->SetScalarParameterValue(TEXT("Glow"), WRTune(TEXT("fungalWorld.veil.glow"), 0.7f));
		}
	}
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	CavesActor = InWorld.SpawnActor<AWRFungalCaves>(AWRFungalCaves::StaticClass(), FTransform::Identity, P);
	SpawnWater();
	StepGlow(1.f);
	UE_LOG(LogWildreach, Log, TEXT("FUNGAL live on %s: the caverns are building, the scenery waits for the ground"), *Map);
}

void UWRFungalWorld::SpawnWater()
{
	// The caverns' water (fungal 2.2: the lakes, the river, the flooded crawls): pawns-only volumes so the swim code
	// finds them (the forest's class), on every machine (movement is predicted).
	UWorld* World = GetWorld();
	const FWRFungalData& D = FWRFungalData::Get();
	TArray<AWRForestWaterVolume::FBox2> Boxes;
	for (const FWRFungalData::FWater& W : D.Water)
	{
		AWRForestWaterVolume::FBox2 B;
		B.Half = FVector(W.Rx, W.Ry, FMath::Max(60.f, (W.Top - W.Bottom) * 0.5f));
		B.Centre = FVector(W.At.X, W.At.Y, W.Top - B.Half.Z);
		B.Yaw = W.Yaw;
		// A lake is an ellipse: a box would reach into its walls (where it doesn't matter: the rock is solid there).
		Boxes.Add(B);
	}
	if (Boxes.Num() == 0 || !World) return;
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AWRForestWaterVolume* V = World->SpawnActor<AWRForestWaterVolume>(AWRForestWaterVolume::StaticClass(), FTransform::Identity, SP))
	{
		V->Build(Boxes);
		WaterVolumes.Add(V);
	}
}

void UWRFungalWorld::BuildScenery()
{
	// Every place in a fixed order (the same on every machine, so each builds the same collision): the sites (camps,
	// landmarks, descents, hand sites, the Heart Grotto's rooms), the shrines' dressing, the kit, the halls' furniture,
	// the tubes', then the plateau's decoration by 200 m cell.
	const FWRFungalData& D = FWRFungalData::Get();
	for (const FWRFungalData::FSite& S : D.Sites) Pieces.Add({ S.Kind, S.Id, S.At, S.Yaw, -1, S.bCave });
	for (int32 i = 0; i < D.Kit.Num(); ++i) Pieces.Add({ D.Kit[i].Kind, D.Kit[i].Id, D.Kit[i].At, 0.f, i, D.Kit[i].bCave });
	for (int32 i = 0; i < D.Halls.Num(); ++i) Pieces.Add({ TEXT("hall"), D.Halls[i].Id, D.Halls[i].At, D.Halls[i].Yaw, i, true });
	for (int32 i = 0; i < D.Tubes.Num(); ++i)
	{
		const FWRFungalData::FTube& T = D.Tubes[i];
		if (T.Pts.Num()) Pieces.Add({ TEXT("tube"), T.Id, T.Pts[0], 0.f, i, true });
	}
	for (int32 i = 0; i < D.Shafts.Num(); ++i)
	{
		const FWRFungalData::FShaft& S = D.Shafts[i];
		if (S.Kind == TEXT("sieve") || S.Kind == TEXT("stair")) Pieces.Add({ TEXT("shaft"), S.Id, FVector(S.At.X, S.At.Y, S.Z1), 0.f, i, true });
	}
	if (const FWRLandDef* L = FWRFungalData::Land())
	{
		const float Span = (L->HfN - 1) * L->HfStep;
		const int32 N = FMath::CeilToInt(Span / 200.f);
		for (int32 j = 0; j < N; ++j) for (int32 i = 0; i < N; ++i)
		{
			const FVector2D M = L->HfOrigin + FVector2D((i + 0.5f) * 200.f, (j + 0.5f) * 200.f);
			if (L->HfHiddenAt(M) && L->CoastDistance(M) < 0.f) continue;
			if (L->Height(M) < 1.f) continue;
			Pieces.Add({ TEXT("cell"), NAME_None, L->ToWorld(M, L->Height(M)), 0.f, j * 1000 + i, false });
		}
	}
	UE_LOG(LogWildreach, Log, TEXT("FUNGAL scenery queued: %d places"), Pieces.Num());
}

void UWRFungalWorld::StepBuild()
{
	UWorld* World = GetWorld();
	if (!World) return;
	if (!bQueued)
	{
		WaitClock += World->GetDeltaSeconds();
		const FWRLandDef* Def = FWRFungalData::Land();
		AWRLand* Land = Def ? UWRLandSubsystem::FindLand(World, Def->Id) : nullptr;
		if (!Land || !Land->IsGroundReady())
		{
			if (WaitClock > 60.f) { WaitClock = 0.f; UE_LOG(LogWildreach, Warning, TEXT("FUNGAL still waiting for its land's ground (Data/lands/fungal.json)")); }
			return;
		}
		BuildScenery();
		bQueued = true;
	}
	if (bSceneryDone) return;
	const int32 PerFrame = FMath::Max(1, int32(WRTune(TEXT("fungalWorld.buildPerFrame"), 4.f)));
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (int32 n = 0; n < PerFrame && PiecesBuilt < Pieces.Num(); ++n, ++PiecesBuilt)
	{
		const FPiece& Pc = Pieces[PiecesBuilt];
		AWRFungalScenery* A = World->SpawnActor<AWRFungalScenery>(AWRFungalScenery::StaticClass(), FTransform(FRotator(0.f, Pc.Yaw, 0.f), Pc.At), P);
		if (!A) continue;
		A->Kind = Pc.Kind;
		A->SiteId = Pc.Id;
		A->Index = Pc.Index;
		A->bUnder = Pc.bUnder;
		A->BuildFungal();
		Scenery.Add(A);
	}
	if (PiecesBuilt < Pieces.Num()) return;
	bSceneryDone = true;
	Pieces.Empty();
	LightClock = 0.f;
	UE_LOG(LogWildreach, Log, TEXT("FUNGAL scenery built: %d places"), Scenery.Num());
}

void UWRFungalWorld::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	if (!World) return;
	StepGlow(DeltaTime);
	if (!bSceneryDone)
	{
		StepBuild();
		if (!bSceneryDone) return;
	}
	if (World->GetNetMode() != NM_DedicatedServer) StepLights(DeltaTime);
	if (World->GetNetMode() != NM_Client && !bServerDone)
	{
		bServerDone = true;
		SpawnServerActors();
	}
	if (!bHazardsDone)
	{
		bHazardsDone = true;
		SpawnHazards();
	}
	StepSporefall(DeltaTime);
	if (World->GetNetMode() != NM_Client)
	{
		StepHazards(DeltaTime);
		StepIlmari(DeltaTime);
		StepInvasionMarks(DeltaTime);
	}
	if (World->GetNetMode() != NM_DedicatedServer) StepClientLooks(DeltaTime);
}

void UWRFungalWorld::StepGlow(float Dt)
{
	// The circadian curve (fungal 2.1, world-map KEPT): bioluminescent fungi glow on a clock that lights them at night;
	// the caverns' glow follows the surface's: 0.35 at noon, 1.0 at midnight, a cosine between, eased.
	GlowClock -= Dt;
	if (GlowClock > 0.f && Dt < 1.f && BeatAmount <= 0.f) return;   // the beat is eased every frame
	GlowClock = 0.25f;
	const float Hour = AWRGameState::HourIn(this);
	const float Noon = WRTune(TEXT("fungalWorld.glow.noon"), 0.35f), Midnight = WRTune(TEXT("fungalWorld.glow.midnight"), 1.f);
	// 1 at midnight, 0 at noon.
	const float Night = 0.5f + 0.5f * FMath::Cos(Hour / 24.f * 2.f * PI);
	GlowCurrent = FMath::FInterpTo(GlowCurrent, FMath::Lerp(Noon, Midnight, Night), FMath::Max(Dt, 0.25f), 0.5f);
	if (!Glow && !(CavesActor.IsValid() && CavesActor->RockGlow())) return;
	// The sync beat (the bloom's tell, the Sporefall): every glow in the hall pulses together.
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	const float Beat = BeatAmount > 0.f ? 1.f + BeatAmount * (0.5f + 0.5f * FMath::Sin(float(Now) * 2.f * PI * BeatRate)) : 1.f;
	const float Scale = WRTune(TEXT("fungalWorld.glow.scale"), 4.f) * GlowCurrent * Beat;
	if (Glow) Glow->SetScalarParameterValue(TEXT("GlowScale"), Scale);
	if (CavesActor.IsValid() && CavesActor->RockGlow()) CavesActor->RockGlow()->SetScalarParameterValue(TEXT("GlowScale"), Scale * WRTune(TEXT("fungalWorld.glow.rockShare"), 0.75f));
}

void UWRFungalWorld::StepLights(float Dt)
{
	// Night blocks (the dolines' cyan from below, foxfire on deadwood, camp fires) show from dusk; under the ground the
	// lamps burn always (the far lights of every hall).
	LightClock -= Dt;
	if (LightClock > 0.f) return;
	LightClock = 0.5f;
	const bool bNight = AWRGameState::IsNightIn(GetWorld());
	for (const TWeakObjectPtr<AWRFungalScenery>& W : Scenery)
	{
		AWRFungalScenery* S = W.Get();
		if (!S) continue;
		const bool bLit = bNight || S->bUnder;
		for (int32 i = 0; i < S->NightLights.Num(); ++i)
		{
			if (UPointLightComponent* L = S->NightLights[i]) L->SetIntensity(bLit ? (S->NightPower.IsValidIndex(i) ? S->NightPower[i] : 1000.f) : 0.f);
		}
		if (S->NightRoot && S->NightRoot->IsVisible() != bLit) S->NightRoot->SetVisibility(bLit, true);
	}
}
