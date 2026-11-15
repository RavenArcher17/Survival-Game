#include "WRTide.h"
#include "Wildreach.h"
#include "WRFarm.h"
#include "WRGameState.h"
#include "WRSea.h"
#include "WRTuning.h"
#include "Components/BrushComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsEngine/BodySetup.h"

// ---------------------------------------------------------------- the state

AWROceanState::AWROceanState()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(2.f);
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWROceanState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWROceanState, KingTide);
	DOREPLIFETIME(AWROceanState, KingTideStart);
	DOREPLIFETIME(AWROceanState, BoreStart);
	DOREPLIFETIME(AWROceanState, bSpawningNight);
	DOREPLIFETIME(AWROceanState, BeaconLitBy);
	DOREPLIFETIME(AWROceanState, BellRungAt);
	DOREPLIFETIME(AWROceanState, bSluicesBurst);
	DOREPLIFETIME(AWROceanState, SluicesBurstAt);
}

AWROceanState* AWROceanState::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World) return nullptr;
	static TWeakObjectPtr<AWROceanState> Cached;
	if (Cached.IsValid() && Cached->GetWorld() == World) return Cached.Get();
	for (TActorIterator<AWROceanState> It(World); It; ++It)
	{
		Cached = *It;
		return *It;
	}
	return nullptr;
}

float AWROceanState::KingTideDrop(double Now) const
{
	if (KingTide == EWRKingTide::None || KingTideStart < 0.0) return 0.f;
	const FWRTuning& T = FWRTuning::Get();
	const float Drop = -T.Num(TEXT("tide.kingTide.drop"), 600.f);
	const double Since = Now - KingTideStart;
	const float Warn = T.Num(TEXT("tide.kingTide.warnSeconds"), 300.f);
	if (Since < Warn)
	{
		// The sea draws back over the warning (smoothstep, like a real ebb: slow, faster, slow).
		const float A = FMath::Clamp(float(Since / Warn), 0.f, 1.f);
		return Drop * A * A * (3.f - 2.f * A);
	}
	if (BoreStart < 0.0 || Now < BoreStart) return Drop;
	// The bore's front comes in from the reef: the water behind it comes back over its run to the Tsunami Line.
	const float Run = FMath::Max(1.f, T.Num(TEXT("tide.kingTide.boreRun"), 150000.f) / FMath::Max(1.f, T.Num(TEXT("tide.kingTide.boreSpeed"), 600.f)));
	const float A = FMath::Clamp(float((Now - BoreStart) / Run), 0.f, 1.f);
	return Drop * (1.f - A);
}

// ---------------------------------------------------------------- the water

AWRTideWater::AWRTideWater(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bReplicates = false;   // every machine builds its own from the same data and moves it by the same clock
	SetHidden(true);
	UBrushComponent* B = GetBrushComponent();
	B->SetCollisionProfileName(UCollisionProfile::CustomCollisionProfileName);
	B->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	B->SetCollisionObjectType(ECC_WorldDynamic);
	B->SetCollisionResponseToAllChannels(ECR_Ignore);
	B->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	B->SetGenerateOverlapEvents(true);
	B->SetCanEverAffectNavigation(false);
	B->SetMobility(EComponentMobility::Movable);
}

void AWRTideWater::Shape(const FBox2D& Box, float FloorZ, float TopZ, bool bWater, int32 InPriority)
{
	XY = Box;
	Floor = FloorZ;
	Top = TopZ;
	bIsWater = bWater;
	bWaterVolume = bWater;
	Priority = InPriority;
	FluidFriction = bWater ? WRTune(TEXT("water.friction"), 2.f) : 0.3f;
	Rebuild();
}

void AWRTideWater::SetLevel(float Z)
{
	if (bIsWater) Top = Z; else Floor = Z;
	// The box is built round the actor; moving the actor moves it (cheap: no new body).
	SetActorLocation(FVector(XY.GetCenter().X, XY.GetCenter().Y, (Floor + Top) * 0.5f));
	const float H = FMath::Max(10.f, Top - Floor);
	SetActorScale3D(FVector(1.f, 1.f, H / 1000.f));
}

void AWRTideWater::Rebuild()
{
	UBrushComponent* B = GetBrushComponent();
	UBodySetup* Setup = NewObject<UBodySetup>(B);
	Setup->CollisionTraceFlag = CTF_UseSimpleAsComplex;
	Setup->bNeverNeedsCookedCollisionData = true;
	// A unit-tall box (10 m) scaled to the water's height, so the level moves by moving and scaling the actor.
	FKBoxElem Box(XY.GetSize().X, XY.GetSize().Y, 1000.f);
	Box.Center = FVector::ZeroVector;
	Setup->AggGeom.BoxElems.Add(Box);
	Setup->CreatePhysicsMeshes();
	B->BrushBodySetup = Setup;
	B->RecreatePhysicsState();
	SetLevel(bIsWater ? Top : Floor);
	B->UpdateBounds();
}

// ---------------------------------------------------------------- the level

bool WRTide::Box(FBox2D& OutBox, float& OutRamp)
{
	static FBox2D Cached(ForceInit);
	static float Ramp = 0.f;
	static bool bRead = false;
	if (!bRead)
	{
		bRead = true;
		const FWRTuning& T = FWRTuning::Get();
		const float X0 = T.Num(TEXT("tide.box.x0"), -300.f), Y0 = T.Num(TEXT("tide.box.y0"), -7955.f);
		const float X1 = T.Num(TEXT("tide.box.x1"), 4700.f), Y1 = T.Num(TEXT("tide.box.y1"), -5700.f);
		Cached = FBox2D(FVector2D(X0, Y0) * 100.0, FVector2D(X1, Y1) * 100.0);
		Ramp = T.Num(TEXT("tide.ramp"), 300.f) * 100.f;
	}
	OutBox = Cached;
	OutRamp = Ramp;
	return Cached.bIsValid;
}

bool WRTide::Active()
{
	return WRBiome::IsLive(TEXT("ocean"));
}

float WRTide::Weight(const FVector2D& XY)
{
	FBox2D B;
	float R = 0.f;
	if (!Box(B, R)) return 0.f;
	// The west side is the Spine's ridge (land): no ramp, the box simply ends there.
	const double Dx = XY.X < B.Min.X ? 1.0e9 : FMath::Max(0.0, XY.X - B.Max.X);
	const double Dy = FMath::Max3(0.0, B.Min.Y - XY.Y, XY.Y - B.Max.Y);
	const double D = FMath::Max(Dx, Dy);
	if (D <= 0.0) return 1.f;
	if (D >= R) return 0.f;
	const float A = 1.f - float(D / R);
	return A * A * (3.f - 2.f * A);
}

float WRTide::CurveAt(double TotalHours)
{
	const FWRTuning& T = FWRTuning::Get();
	const float Range = T.Num(TEXT("tide.range"), 240.f) * 0.5f;
	const float Period = FMath::Max(1.f, T.Num(TEXT("tide.periodHours"), 12.f));
	const float HighAt = T.Num(TEXT("tide.highHour"), 0.f);
	return Range * FMath::Cos(2.0 * PI * (TotalHours - HighAt) / Period);
}

float WRTide::Level(const UObject* WorldContext)
{
	if (!Active() || !WorldContext) return 0.f;
	const AWRGameState* GS = AWRGameState::Get(WorldContext);
	float L = GS ? CurveAt(GS->GetTotalHours()) : 0.f;
	if (const AWROceanState* S = AWROceanState::Get(WorldContext))
	{
		const UWorld* W = WorldContext->GetWorld();
		L += S->KingTideDrop(W && W->GetGameState() ? W->GetGameState()->GetServerWorldTimeSeconds() : 0.0);
	}
	return L;
}

float WRTide::MeanZ()
{
	const FWRTuning& T = FWRTuning::Get();
	return T.Num(TEXT("sea.level"), -20.f) + T.Num(TEXT("water.surfaceOffset"), 80.f);
}

float WRTide::SurfaceAt(const UObject* WorldContext, const FVector& Point)
{
	const float W = Weight(FVector2D(Point.X, Point.Y));
	return MeanZ() + (W > 0.f ? Level(WorldContext) * W : 0.f);
}

float WRTide::Fullness(const UObject* WorldContext)
{
	const float Range = FMath::Max(1.f, WRTune(TEXT("tide.range"), 240.f) * 0.5f);
	return FMath::Clamp(0.5f + 0.5f * Level(WorldContext) / Range, 0.f, 1.f);
}

bool WRTide::Rising(const UObject* WorldContext)
{
	const AWRGameState* GS = AWRGameState::Get(WorldContext);
	if (!GS) return false;
	const double H = GS->GetTotalHours();
	return CurveAt(H + 0.05) > CurveAt(H);
}

// ---------------------------------------------------------------- the subsystem

bool UWRTideSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* W = Cast<UWorld>(Outer);
	return W && (W->WorldType == EWorldType::Game || W->WorldType == EWorldType::PIE);
}

TStatId UWRTideSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWRTideSubsystem, STATGROUP_Tickables);
}

void UWRTideSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	const FString Map = FWRTuning::Get().Str(TEXT("water.map"), TEXT("L_Island"));
	bOn = WRTide::Active() && UWorld::RemovePIEPrefix(InWorld.GetMapName()) == Map;
}

void UWRTideSubsystem::Build()
{
	bBuilt = true;
	FBox2D B;
	float R = 0.f;
	if (!WRTide::Box(B, R)) return;
	const FWRTuning& T = FWRTuning::Get();
	const float Floor = T.Num(TEXT("tide.floor"), -11000.f);
	const float AirTop = WRTide::MeanZ() + T.Num(TEXT("tide.airAbove"), 250.f);
	const int32 Priority = FMath::RoundToInt(T.Num(TEXT("tide.priority"), 2.f));
	const int32 Rings = FMath::Clamp(FMath::RoundToInt(T.Num(TEXT("tide.rampStrips"), 6.f)), 1, 20);
	const auto Make = [&](const FBox2D& Box, float W)
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		for (const bool bWater : { true, false })
		{
			AWRTideWater* V = GetWorld()->SpawnActor<AWRTideWater>(AWRTideWater::StaticClass(), FTransform(FVector(Box.GetCenter(), 0.0)), P);
			if (!V) continue;
			V->Weight = W;
			const float Mid = WRTide::MeanZ();
			V->Shape(Box, bWater ? Floor : Mid, bWater ? Mid : AirTop, bWater, Priority);
			Volumes.Add(V);
		}
	};
	Make(B, 1.f);
	// The ramp in strips (north, south: across the box; east: down the whole grown side, corners included).
	const double Step = R / Rings;
	for (int32 i = 0; i < Rings; ++i)
	{
		const float W = WRTide::Weight(FVector2D(B.Max.X + Step * (i + 0.5), B.GetCenter().Y));
		const double A = Step * i, Z = Step * (i + 1);
		Make(FBox2D(FVector2D(B.Min.X, B.Min.Y - Z), FVector2D(B.Max.X, B.Min.Y - A)), W);
		Make(FBox2D(FVector2D(B.Min.X, B.Max.Y + A), FVector2D(B.Max.X, B.Max.Y + Z)), W);
		Make(FBox2D(FVector2D(B.Max.X + A, B.Min.Y - R), FVector2D(B.Max.X + Z, B.Max.Y + R)), W);
	}
	UE_LOG(LogWildreach, Log, TEXT("TIDE up: %d volumes over the Ocean's box (%s), ramp %.0f m"), Volumes.Num(), *B.ToString(), R / 100.f);
	Move(true);
}

void UWRTideSubsystem::Move(bool bForce)
{
	const float L = WRTide::Level(this);
	if (!bForce && FMath::Abs(L - LastLevel) < 0.5f) return;
	LastLevel = L;
	const float Mean = WRTide::MeanZ();
	for (const TWeakObjectPtr<AWRTideWater>& W : Volumes)
	{
		if (AWRTideWater* V = W.Get()) V->SetLevel(Mean + L * V->Weight);
	}
	if (GetWorld()->GetNetMode() != NM_DedicatedServer)
	{
		if (AWRSea* Sea = AWRSea::Find(GetWorld())) Sea->SetRegionOffset(L);
	}
}

void UWRTideSubsystem::Tick(float DeltaTime)
{
	if (!bBuilt)
	{
		Build();
		return;
	}
	Clock += DeltaTime;
	// Faster while the King Tide moves the sea (6 m in 5 minutes), every couple of seconds otherwise.
	const AWROceanState* S = AWROceanState::Get(this);
	const float Every = S && S->KingTide != EWRKingTide::None ? 0.25f : WRTune(TEXT("tide.moveSeconds"), 2.f);
	if (Clock >= Every)
	{
		Clock = 0.f;
		Move(false);
	}
}
