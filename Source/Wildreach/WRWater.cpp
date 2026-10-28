#include "WRWater.h"
#include "Wildreach.h"
#include "WRAttributeSet.h"
#include "WRCharacter.h"
#include "WRPlayerController.h"
#include "WRSound.h"
#include "WRTuning.h"
#include "AbilitySystemComponent.h"
#include "Components/BrushComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicsEngine/BodySetup.h"

// ---------------------------------------------------------------- the volume

AWRWaterVolume::AWRWaterVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bWaterVolume = true;
	bReplicates = false;   // every machine builds its own from the same data
	SetHidden(true);
	// Pawns only: the capsule's overlap finds it (that's how CharacterMovement picks its physics volume) and the
	// swim code's own pawn-channel traces see it; line traces, the camera, building and projectiles never do.
	UBrushComponent* Shape = GetBrushComponent();
	Shape->SetCollisionProfileName(UCollisionProfile::CustomCollisionProfileName);
	Shape->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Shape->SetCollisionObjectType(ECC_WorldDynamic);   // not world geometry, which the swim code's water-line trace skips
	Shape->SetCollisionResponseToAllChannels(ECR_Ignore);
	Shape->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Shape->SetGenerateOverlapEvents(true);
	Shape->SetCanEverAffectNavigation(false);
}

int32 AWRWaterVolume::Build(const FWRSeaMask& Mask, float SurfaceZ, float FloorZ, float Extent)
{
	FluidFriction = WRTune(TEXT("water.friction"), 2.f);
	UBrushComponent* Shape = GetBrushComponent();
	// No brush model (that's editor geometry): the volume's shape is a body of boxes, which the brush component uses
	// for its collision and bounds when it has no model.
	UBodySetup* Setup = NewObject<UBodySetup>(Shape);
	Setup->CollisionTraceFlag = CTF_UseSimpleAsComplex;
	Setup->bNeverNeedsCookedCollisionData = true;
	const double Height = SurfaceZ - FloorZ, MidZ = (SurfaceZ + FloorZ) * 0.5;
	const auto AddBox = [Setup, Height, MidZ](double X0, double Y0, double X1, double Y1)
	{
		FKBoxElem Box(X1 - X0, Y1 - Y0, Height);
		Box.Center = FVector((X0 + X1) * 0.5, (Y0 + Y1) * 0.5, MidZ);
		Setup->AggGeom.BoxElems.Add(Box);
	};
	const double GX0 = Mask.Origin.X, GY0 = Mask.Origin.Y;
	const double GX1 = GX0 + Mask.Width * Mask.Cell, GY1 = GY0 + Mask.Height * Mask.Cell;
	for (const FIntRect& R : FWRWaterRules::MergeCells(Mask))
	{
		AddBox(GX0 + R.Min.X * Mask.Cell, GY0 + R.Min.Y * Mask.Cell, GX0 + R.Max.X * Mask.Cell, GY0 + R.Max.Y * Mask.Cell);
	}
	// Outside the grid it's all sea: a frame of four boxes out to Extent.
	AddBox(-Extent, -Extent, GX0, Extent);
	AddBox(GX1, -Extent, Extent, Extent);
	AddBox(GX0, -Extent, GX1, GY0);
	AddBox(GX0, GY1, GX1, Extent);
	Setup->CreatePhysicsMeshes();
	Shape->BrushBodySetup = Setup;
	Shape->RecreatePhysicsState();
	Shape->UpdateBounds();
	return Setup->AggGeom.BoxElems.Num();
}

TArray<FIntRect> FWRWaterRules::MergeCells(const FWRSeaMask& Mask)
{
	// Greedy: from each sea cell not yet covered, the run of sea along the row, grown down the rows while the whole run
	// is still sea and uncovered. Few boxes (about a thousand for the island), every sea cell in exactly one.
	TArray<FIntRect> Out;
	const int32 W = Mask.Width, H = Mask.Height;
	if (W <= 0 || H <= 0 || Mask.Sea.Num() != W * H)
	{
		return Out;
	}
	TBitArray<> Used(false, W * H);
	const auto Open = [&](int32 X, int32 Y) { return Mask.Sea[Y * W + X] != 0 && !Used[Y * W + X]; };
	for (int32 Y = 0; Y < H; ++Y)
	{
		for (int32 X = 0; X < W; ++X)
		{
			if (!Open(X, Y))
			{
				continue;
			}
			int32 X1 = X;
			while (X1 < W && Open(X1, Y)) ++X1;
			int32 Y1 = Y + 1;
			for (; Y1 < H; ++Y1)
			{
				bool bRow = true;
				for (int32 K = X; K < X1 && bRow; ++K) bRow = Open(K, Y1);
				if (!bRow) break;
			}
			for (int32 J = Y; J < Y1; ++J) for (int32 K = X; K < X1; ++K) Used[J * W + K] = true;
			Out.Add(FIntRect(X, Y, X1, Y1));
		}
	}
	return Out;
}

// ---------------------------------------------------------------- subsystem

namespace WRWaterDetail
{
	/** The local player's survivor (console commands, the listener). */
	static APawn* LocalPawn(const UWorld* World)
	{
		const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		return PC ? PC->GetPawn() : nullptr;
	}

	/** wr.Water: logs what the water system sees for the local player (bot tests read it). */
	static void WaterCommand(const TArray<FString>& Args, UWorld* World)
	{
		const UWRWaterSubsystem* Water = UWRWaterSubsystem::Get(World);
		const ACharacter* Me = Cast<ACharacter>(LocalPawn(World));
		UE_LOG(LogWildreach, Log, TEXT("WATER client [%s] %s"), Args.Num() > 0 ? *Args[0] : TEXT("-"), Water && Me ? *Water->Describe(Me) : TEXT("no water or no body"));
	}
	static FAutoConsoleCommandWithWorldAndArgs WaterConsole(TEXT("wr.Water"),
		TEXT("wr.Water [tag]: log the local survivor's water state (position, movement mode, in water, depth, Water, can drink)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&WaterCommand));
}

bool UWRWaterSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

TStatId UWRWaterSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWRWaterSubsystem, STATGROUP_Tickables);
}

UWRWaterSubsystem* UWRWaterSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UWRWaterSubsystem>() : nullptr;
}

void UWRWaterSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// The mask belongs to one map (other maps, like the combat arena, have no sea to swim in).
	const TSharedPtr<FJsonObject> File = FWRTuning::LoadDataFile(TEXT("sea_mask.json"));
	FString Map;
	if (File.IsValid() && !File->TryGetStringField(TEXT("map"), Map))
	{
		Map = FWRTuning::Get().Str(TEXT("water.map"), TEXT("L_Island"));   // a mask from before it named its map
	}
	if (!File.IsValid() || Map != UWorld::RemovePIEPrefix(InWorld.GetMapName()))
	{
		return;
	}
	if (!Mask.Parse(*File))
	{
		UE_LOG(LogWildreach, Warning, TEXT("WATER Data/sea_mask.json is unreadable: no water to swim in"));
		return;
	}
	const FWRTuning& T = FWRTuning::Get();
	SurfaceZ = T.Num(TEXT("sea.level"), -20.f);
	const float FloorZ = T.Num(TEXT("water.floor"), -1500.f);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWRWaterVolume* V = InWorld.SpawnActor<AWRWaterVolume>(AWRWaterVolume::StaticClass(), FTransform::Identity, Params);
	if (!V)
	{
		return;
	}
	const int32 Boxes = V->Build(Mask, SurfaceZ, FloorZ, T.Num(TEXT("water.extent"), 1000000.f));
	Volume = V;
	bActive = true;
	bSounds = InWorld.GetNetMode() != NM_DedicatedServer;
	// The old plane is a World Partition actor: it may stream in after this, with the cell it's in.
	TurnOffOldPlane(nullptr);
	LevelAddedHandle = FWorldDelegates::LevelAddedToWorld.AddUObject(this, &UWRWaterSubsystem::OnLevelAdded);
	const AWorldSettings* Settings = InWorld.GetWorldSettings();
	UE_LOG(LogWildreach, Log, TEXT("WATER up: %d boxes over the open sea, surface z %.0f, floor %.0f; old sea plane collision turned off on %d; KillZ %.0f"),
		Boxes, SurfaceZ, FloorZ, OldPlanes, Settings ? Settings->KillZ : 0.f);
}

void UWRWaterSubsystem::Deinitialize()
{
	FWorldDelegates::LevelAddedToWorld.Remove(LevelAddedHandle);
	Super::Deinitialize();
}

void UWRWaterSubsystem::OnLevelAdded(ULevel* Level, UWorld* World)
{
	if (World == GetWorld() && Level)
	{
		TurnOffOldPlane(Level);
	}
}

void UWRWaterSubsystem::TurnOffOldPlane(ULevel* Level)
{
	// The map's flat plane (the static mesh actor wearing MI_Sea, from before the sea was drawn by WRSea) is a 1.2 km
	// square under the middle of the island, not over the sea. Nothing should stand on it: someone who falls through
	// the ground is caught by the game mode's rescue instead.
	const auto Check = [this](AActor* A)
	{
		AStaticMeshActor* Plane = Cast<AStaticMeshActor>(A);
		UStaticMeshComponent* C = Plane ? Plane->GetStaticMeshComponent() : nullptr;
		const UMaterialInterface* M = C ? C->GetMaterial(0) : nullptr;
		if (M && M->GetName() == TEXT("MI_Sea") && Plane->GetActorEnableCollision())
		{
			UE_LOG(LogWildreach, Log, TEXT("WATER old sea plane %s at %s, %.0f m wide: collision %s, now off"), *Plane->GetName(), *Plane->GetActorLocation().ToCompactString(),
				C->Bounds.BoxExtent.X * 2.f / 100.f, C->IsCollisionEnabled() ? TEXT("was on") : TEXT("already off on the mesh"));
			Plane->SetActorEnableCollision(false);
			++OldPlanes;
		}
	};
	if (Level)
	{
		for (AActor* A : Level->Actors) Check(A);
	}
	else
	{
		for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It) Check(*It);
	}
}

bool UWRWaterSubsystem::IsSeaAt(const FVector& Point) const
{
	if (!bActive)
	{
		return false;
	}
	const int32 X = FMath::FloorToInt((Point.X - Mask.Origin.X) / Mask.Cell);
	const int32 Y = FMath::FloorToInt((Point.Y - Mask.Origin.Y) / Mask.Cell);
	return Mask.IsSea(X, Y);
}

float UWRWaterSubsystem::DepthAt(const FVector& Point) const
{
	if (!IsSeaAt(Point))
	{
		return 0.f;
	}
	// The ground under the surface here: the landscape, a rock, a foundation built in the shallows.
	const float FloorZ = WRTune(TEXT("water.floor"), -1500.f);
	FHitResult Hit;
	const FCollisionQueryParams Query(SCENE_QUERY_STAT(WRWaterDepth), false);
	if (!GetWorld()->LineTraceSingleByObjectType(Hit, FVector(Point.X, Point.Y, SurfaceZ + 300.f), FVector(Point.X, Point.Y, FloorZ), FCollisionObjectQueryParams(ECC_WorldStatic), Query))
	{
		return SurfaceZ - FloorZ;
	}
	return FMath::Max(0.f, static_cast<float>(SurfaceZ - Hit.ImpactPoint.Z));
}

bool UWRWaterSubsystem::IsInWater(const ACharacter* Character)
{
	const UCharacterMovementComponent* Move = Character ? Character->GetCharacterMovement() : nullptr;
	const APhysicsVolume* V = Move ? Move->GetPhysicsVolume() : nullptr;
	return V && V->bWaterVolume;
}

bool UWRWaterSubsystem::CanDrink(const ACharacter* Character) const
{
	if (!bActive || !Character)
	{
		return false;
	}
	if (CacheFrame == GFrameCounter && CacheWho.Get() == Character)
	{
		return bCacheResult;
	}
	CacheFrame = GFrameCounter;
	CacheWho = Character;
	bCacheResult = false;
	if (IsInWater(Character))
	{
		bCacheResult = true;
		return true;
	}
	// At the edge: feet no higher above the surface than you can reach down, and open water at your feet or in front.
	const FVector Here = Character->GetActorLocation();
	const float Feet = Here.Z - Character->GetSimpleCollisionHalfHeight();
	if (Feet > SurfaceZ + WRTune(TEXT("water.drinkReach"), 80.f))
	{
		return false;
	}
	// Under your feet, then all round you within reach (whichever way you face; land points cost no trace).
	const float Range = WRTune(TEXT("water.drinkRange"), 180.f);
	const float MinDepth = WRTune(TEXT("water.drinkMinDepth"), 4.f);
	bCacheResult = DepthAt(Here) >= MinDepth;
	for (int32 Dir = 0; Dir < 8 && !bCacheResult; ++Dir)
	{
		bCacheResult = DepthAt(Here + FRotator(0.f, Character->GetActorRotation().Yaw + Dir * 45.f, 0.f).Vector() * Range) >= MinDepth;
	}
	return bCacheResult;
}

FString UWRWaterSubsystem::Drink(AWRPlayerController* Player)
{
	AWRCharacter* Survivor = Player ? Cast<AWRCharacter>(Player->GetPawn()) : nullptr;
	if (!bActive || !Survivor || !Survivor->IsAlive() || !CanDrink(Survivor))
	{
		return FString();
	}
	if (WRTune(TEXT("water.seaDrinkable"), 1.f) <= 0.f)
	{
		return TEXT("The sea is too salty to drink");
	}
	UAbilitySystemComponent* ASC = Survivor->GetAbilitySystemComponent();
	const UWRAttributeSet* A = Survivor->GetAttributes();
	if (!ASC || !A)
	{
		return FString();
	}
	if (A->GetThirst() >= 99.5f)
	{
		return TEXT("You're not thirsty");
	}
	const double Now = GetWorld()->GetTimeSeconds();
	double& Next = NextDrink.FindOrAdd(Player);
	if (Now < Next)
	{
		return TEXT("Still drinking");
	}
	Next = Now + WRTune(TEXT("water.drinkSeconds"), 1.2f);
	const float Before = A->GetThirst();
	ASC->SetNumericAttributeBase(UWRAttributeSet::GetThirstAttribute(), FMath::Min(100.f, Before + WRTune(TEXT("water.drinkThirst"), 20.f)));
	WRSound::Broadcast(Survivor, TEXT("water.scoop"));
	WRSound::Broadcast(Survivor, TEXT("drink"));
	if (!Survivor->GetCharacterMovement()->IsSwimming())
	{
		Survivor->MulticastBodyOneShot(TEXT("pickup"));   // kneels and scoops it up (a swimmer just drinks)
	}
	UE_LOG(LogWildreach, Log, TEXT("WATER %s drinks: Water %.0f -> %.0f"), *Survivor->GetName(), Before, A->GetThirst());
	return FString::Printf(TEXT("You drink sea water (Water %.0f)"), A->GetThirst());
}

FVector UWRWaterSubsystem::KeepOutOfDeepWater(const ACharacter* Creature, const FVector& Direction) const
{
	if (!bActive || !Creature || Direction.IsNearlyZero())
	{
		return Direction;
	}
	const FVector Here = Creature->GetActorLocation();
	const float Half = Creature->GetSimpleCollisionHalfHeight();
	if (Here.Z - Half > SurfaceZ + 150.f || (!IsSeaAt(Here + Direction.GetSafeNormal2D() * 1000.f) && !IsSeaAt(Here)))
	{
		return Direction;   // well above the sea, or nowhere near it: most of the island, no trace
	}
	// Wading is fine (up to about the belly); deeper than that it turns back, so it never walks the sea floor.
	const float Wade = Half * WRTune(TEXT("water.creatureWade"), 0.8f);
	const FVector Step = Direction.GetSafeNormal2D();
	const float There = DepthAt(Here + Step * (Creature->GetSimpleCollisionRadius() + WRTune(TEXT("water.creatureLookAhead"), 150.f)));
	if (There <= Wade || There < DepthAt(Here))
	{
		return Direction;
	}
	return FVector::ZeroVector;
}

FString UWRWaterSubsystem::Describe(const ACharacter* Character) const
{
	const UCharacterMovementComponent* Move = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Move)
	{
		return TEXT("no body");
	}
	const AWRCharacterBase* Body = Cast<AWRCharacterBase>(Character);
	const UWRAttributeSet* A = Body ? Body->GetAttributes() : nullptr;
	const FVector Here = Character->GetActorLocation();
	return FString::Printf(TEXT("%s at %s mode %s inwater %d depth %.0f surface %.0f water %.0f candrink %d speed %.0f"),
		*Character->GetName(), *Here.ToCompactString(), *Move->GetMovementName(), IsInWater(Character) ? 1 : 0, DepthAt(Here), SurfaceZ,
		A ? A->GetThirst() : -1.f, CanDrink(Character) ? 1 : 0, Character->GetVelocity().Size2D());
}

FString UWRWaterSubsystem::DevCommand(AWRPlayerController* Player, const TArray<FString>& Args)
{
	AWRCharacter* Survivor = Player ? Cast<AWRCharacter>(Player->GetPawn()) : nullptr;
	if (!Survivor)
	{
		return TEXT("no survivor");
	}
	const FString What = Args.IsValidIndex(1) ? Args[1] : FString(TEXT("stat"));
	if (What == TEXT("thirst") && Args.IsValidIndex(2) && Survivor->GetAbilitySystemComponent())
	{
		Survivor->GetAbilitySystemComponent()->SetNumericAttributeBase(UWRAttributeSet::GetThirstAttribute(), FMath::Clamp(FCString::Atof(*Args[2]), 0.f, 100.f));
	}
	else if (What == TEXT("sink") && Args.IsValidIndex(2))
	{
		// Through the ground, as a fall through missing collision would leave you (tests the rescue).
		Survivor->TeleportTo(Survivor->GetActorLocation() - FVector(0.f, 0.f, FCString::Atof(*Args[2])), Survivor->GetActorRotation(), false, true);
		Survivor->GetCharacterMovement()->Velocity = FVector::ZeroVector;
	}
	else if (What == TEXT("creatures"))
	{
		// Every creature within 80 m: where it stands, how it moves and how deep the water is there.
		for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
		{
			if (*It != Survivor && FVector::Dist(It->GetActorLocation(), Survivor->GetActorLocation()) < 8000.f)
			{
				UE_LOG(LogWildreach, Log, TEXT("WATER creature %s half %.0f"), *Describe(*It), It->GetSimpleCollisionHalfHeight());
			}
		}
	}
	const FString Line = Describe(Survivor);
	UE_LOG(LogWildreach, Log, TEXT("WATER server [%s] %s"), Args.IsValidIndex(2) && What == TEXT("stat") ? *Args[2] : *What, *Line);
	return Line;
}

void UWRWaterSubsystem::Tick(float DeltaTime)
{
	// Sounds where you can hear them: a splash going in (bigger after a fall), strokes while swimming along.
	UWorld* World = GetWorld();
	const double Now = World->GetTimeSeconds();
	if (Now < NextWatch)
	{
		return;
	}
	NextWatch = Now + 0.1;
	const APawn* Me = WRWaterDetail::LocalPawn(World);
	if (!Me)
	{
		return;
	}
	const float Range = WRTune(TEXT("water.soundRange"), 3500.f);
	const float StrokeEvery = WRTune(TEXT("water.strokeSeconds"), 0.85f);
	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		ACharacter* C = *It;
		const FVector At = C->GetActorLocation();
		if (FVector::DistSquared(At, Me->GetActorLocation()) > FMath::Square(Range) || At.Z > SurfaceZ + 400.f)
		{
			Swimmers.Remove(C);
			continue;
		}
		const UCharacterMovementComponent* Move = C->GetCharacterMovement();
		const bool bSwimming = Move && Move->IsSwimming();
		FSwimmer& S = Swimmers.FindOrAdd(C);
		const FVector Surface(At.X, At.Y, SurfaceZ);
		if (bSwimming && !S.bSwimming)
		{
			WRSound::PlayAt(C, TEXT("water.splash"), Surface, FMath::Clamp(static_cast<float>(-C->GetVelocity().Z) / 600.f, 0.6f, 1.6f));
			S.NextStroke = Now + StrokeEvery;
		}
		else if (bSwimming && Now >= S.NextStroke && C->GetVelocity().Size2D() > 60.f)
		{
			WRSound::PlayAt(C, TEXT("water.stroke"), Surface);
			S.NextStroke = Now + StrokeEvery * FMath::FRandRange(0.9f, 1.1f);
		}
		S.bSwimming = bSwimming;
	}
	for (auto It = Swimmers.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid()) It.RemoveCurrent();
	}
}
