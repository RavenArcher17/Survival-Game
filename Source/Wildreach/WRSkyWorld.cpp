// The Sky's world (see WRSkyWorld.h; cooks/build-biome-sky.md 2.2-2.10, 2.15-2.16).

#include "WRSkyWorld.h"
#include "WRSkyCreatures.h"
#include "Wildreach.h"
#include "WRSkyIsleTypes.h"
#include "WRSkyLook.h"
#include "WRSkyAir.h"
#include "WRSkySceneryParts.h"
#include "WRCharacter.h"
#include "WRCharacterBase.h"
#include "WRFarm.h"
#include "WRFx.h"
#include "WRGameState.h"
#include "WRInventory.h"
#include "WRLand.h"
#include "WRLifeSkills.h"
#include "WRPathComponent.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRResourceNode.h"
#include "WRSound.h"
#include "WRSurvival.h"
#include "WRToolMesh.h"
#include "WRTuning.h"
#include "WRWorldSystems.h"
#include "WRGameplayTags.h"
#include "WRBuildLook.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"

using namespace WRSkyLook;

namespace WRSkyWorldImpl
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
	float Now(const UObject* Ctx) { return float(WRSkyIsles::ServerTime(Ctx)); }
	FString ItemName(FName Id)
	{
		const FWRItemDef* D = FWRItemRegistry::Get().Find(Id);
		return D ? D->Name : Id.ToString();
	}
	FString PlayerOf(const AActor* A) { return WRWorld::PlayerOf(A); }
	float Ease(float T) { T = FMath::Clamp(T, 0.f, 1.f); return T * T * (3.f - 2.f * T); }
}

using namespace WRSkyWorldImpl;

// ================================================================================================ AWRSkyState

AWRSkyState::AWRSkyState()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;
	SetNetUpdateFrequency(2.f);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRSkyState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRSkyState, TurnSignAt);
	DOREPLIFETIME(AWRSkyState, DeckOpen);
	DOREPLIFETIME(AWRSkyState, TurnStartAt);
	DOREPLIFETIME(AWRSkyState, TurnEndAt);
	DOREPLIFETIME(AWRSkyState, bTollLit);
	DOREPLIFETIME(AWRSkyState, BonebellAt);
	DOREPLIFETIME(AWRSkyState, RoseTurnedAt);
	DOREPLIFETIME(AWRSkyState, PlumeAt);
	DOREPLIFETIME(AWRSkyState, PlumeVent);
}

AWRSkyState* AWRSkyState::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World) return nullptr;
	for (TActorIterator<AWRSkyState> It(const_cast<UWorld*>(World)); It; ++It) return *It;
	return nullptr;
}

void AWRSkyState::OnRep_Deck()
{
	// The deck's open slabs: hidden, no collision (the scenery built each slab as its own part).
	for (TActorIterator<AWRSkyScenery> It(GetWorld()); It; ++It)
	{
		for (int32 i = 0; i < It->Slabs.Num(); ++i)
		{
			UStaticMeshComponent* M = It->Slabs[i];
			if (!M) continue;
			const bool bOpen = DeckOpen.IsValidIndex(i) && DeckOpen[i] != 0;
			M->SetVisibility(!bOpen);
			M->SetCollisionEnabled(bOpen ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
		}
	}
}

float AWRSkyState::TurnAmount(float NowS) const
{
	if (TurnStartAt <= 0.f) return 0.f;
	const float Swing = Tn(TEXT("skyWorld.turningWind.swingSeconds"), 120.f);
	if (NowS < TurnStartAt) return 0.f;
	if (NowS < TurnEndAt) return Ease((NowS - TurnStartAt) / Swing);
	return 1.f - Ease((NowS - TurnEndAt) / Swing);
}

float AWRSkyState::DriftAmount(float NowS, float SplineLength) const
{
	if (TurnStartAt <= 0.f || NowS < TurnStartAt) return 0.f;
	const float Speed = Tn(TEXT("skyWorld.turningWind.driftSpeed"), 100.f);
	const float Travel = FMath::Max(1.f, SplineLength / Speed);
	if (NowS < TurnEndAt) return Ease((NowS - TurnStartAt) / Travel);
	return 1.f - Ease((NowS - TurnEndAt) / Travel);
}

float AWRSkyState::BridgeAmount(float NowS) const
{
	if (TurnStartAt <= 0.f || NowS < TurnStartAt) return 0.f;
	const float Form = Tn(TEXT("skyWorld.turningWind.bridgeForm"), 60.f);
	const float Fade = Tn(TEXT("skyWorld.turningWind.bridgeFade"), 60.f);
	if (NowS < TurnEndAt) return FMath::Clamp((NowS - TurnStartAt) / Form, 0.f, 1.f);
	return FMath::Clamp(1.f - (NowS - TurnEndAt) / Fade, 0.f, 1.f);
}

void AWRSkyState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// The Turning Wind is a world event (world.json events.turning_wind): the server copies its clock here.
	if (HasAuthority())
	{
		if (const AWRWorldState* WS = AWRWorldState::Get(this))
		{
			const FWRWorldEventRep* E = WS->Event(TEXT("turning_wind"));
			if (E && E->StartAt > 0.f)
			{
				const float Warn = WRWorld::Num(TEXT("events.warnSeconds"), 300.f);
				if (TurnStartAt != E->StartAt) { TurnStartAt = E->StartAt; TurnEndAt = E->EndAt; TurnSignAt = E->StartAt - Warn; }
			}
		}
	}
	// Every machine: the wind's swing (180 degrees, from the west) for everything that reads WRSkyIsles::WindYaw.
	WRSkyIsles::SetTurn(180.f * TurnAmount(Now(this)));
}

// ================================================================================================ AWRSkySpot

AWRSkySpot::AWRSkySpot()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.2f;
	SetNetUpdateFrequency(2.f);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRSkySpot::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRSkySpot, Kind);
	DOREPLIFETIME(AWRSkySpot, SpotId);
	DOREPLIFETIME(AWRSkySpot, Title);
	DOREPLIFETIME(AWRSkySpot, Verb);
	DOREPLIFETIME(AWRSkySpot, Seconds);
	DOREPLIFETIME(AWRSkySpot, DoneAt);
	DOREPLIFETIME(AWRSkySpot, Doer);
}

bool AWRSkySpot::IsDoneBy(const FString& Player, double NowHours) const
{
	const double* At = DoneBy.Find(Player);
	if (!At) return false;
	if (RespawnHours <= 0.f) return true;
	return NowHours - *At < RespawnHours;
}

FString AWRSkySpot::Start(AWRPlayerController* PC)
{
	AWRCharacterBase* Me = PC ? Cast<AWRCharacterBase>(PC->GetPawn()) : nullptr;
	if (!Me) return FString();
	const FString Who = PlayerOf(Me);
	if (DoneAt > 0.f) return Doer == Who ? TEXT("Hold still...") : FString::Printf(TEXT("%s is at it"), *Doer);
	if (Kind == TEXT("rose"))
	{
		// The Wind Rose (SKY 2.8): at noon, the carved wind the Great Kite points away from (the one the wind comes from)
		// is the one that turns. Stand on it.
		const double Hour = FMath::Fmod(GameHours(this), 24.0);
		if (Hour < 11.0 || Hour > 13.0) return TEXT("Eight winds carved in the floor. The stone in the middle is loose.");
		const float From = WRSkyIsles::WindYaw(this) + 180.f;
		const FVector2D Off(Me->GetActorLocation() - GetActorLocation());
		const float StandYaw = FMath::RadiansToDegrees(FMath::Atan2(Off.Y, Off.X));
		if (Off.Size() < 250.f || FMath::Abs(FMath::FindDeltaAngleDegrees(StandYaw, From)) > 22.5f) return TEXT("The plate doesn't move. You're not standing on the right wind.");
	}
	if (Kind == TEXT("bonebell"))
	{
		if (UWRSkyWorld* W = UWRSkyWorld::Get(this)) W->StrikeBonebell(Me);
		return FString();
	}
	if (IsDoneBy(Who, GameHours(this)))
	{
		return Kind == TEXT("gossamer") ? TEXT("You've taken the silk here; it gathers again by tomorrow")
			: Kind == TEXT("moss") ? TEXT("You've peeled what's ripe here; it grows back by tomorrow") : TEXT("You've had what's here");
	}
	float Secs = Seconds;
	if (Kind == TEXT("gossamer") && Me->GetVelocity().Size2D() > 50.f) return TEXT("Stand still to wind the silk");
	DoneAt = Now(this) + Secs;
	Doer = Who;
	DoerPawn = Me;
	DoerFrom = Me->GetActorLocation();
	SetActorTickEnabled(true);
	if (Kind == TEXT("gossamer")) WRSound::Broadcast(this, TEXT("sky.silk_wind"));
	return FString::Printf(TEXT("%s..."), *Verb);
}

void AWRSkySpot::BeginPlay()
{
	Super::BeginPlay();
	if (Kind == TEXT("moss") && GetNetMode() != NM_DedicatedServer)
	{
		BuildMossLook();
		SetActorTickInterval(0.f);   // its bob and sway are drawn every frame (TickMossLook culls by distance)
	}
}

void AWRSkySpot::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (MossFloat) TickMossLook(DeltaSeconds);
	if (!HasAuthority() || DoneAt <= 0.f) return;
	AWRCharacterBase* P = DoerPawn.Get();
	if (!P || !P->IsAlive() || FVector::Dist(P->GetActorLocation(), DoerFrom) > 120.f || FVector::Dist(P->GetActorLocation(), GetActorLocation()) > Reach + 150.f)
	{
		if (P) Tell(P, TEXT("You let go"));
		DoneAt = 0.f;
		Doer.Reset();
		DoerPawn = nullptr;
		return;
	}
	if (Now(this) >= DoneAt) Finish();
}

void AWRSkySpot::Finish()
{
	AWRCharacterBase* P = DoerPawn.Get();
	DoneAt = 0.f;
	Doer.Reset();
	DoerPawn = nullptr;
	if (!P) return;
	const FString Who = PlayerOf(P);
	DoneBy.Add(Who, GameHours(this));
	UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(P);
	TArray<FString> Got;
	// An egg out of a Windmane nest: the herd comes for the thief (WRSkyCreatures).
	if (Kind == TEXT("nest")) if (UWRSkyLife* L = UWRSkyLife::Get(this)) L->EggTaken(GetActorLocation(), P);
	for (const TPair<FName, int32>& G : Gives)
	{
		int32 N = G.Value;
		if (Kind == TEXT("gossamer"))
		{
			// 2-4 silk a drift, twice that in Rain (SKY 2.6), x3 levistone... no: silk's own rule; the Turning Wind
			// pays levistone, not silk.
			N = FMath::RandRange(int32(Tn(TEXT("skyWorld.gossamer.min"), 2.f)), int32(Tn(TEXT("skyWorld.gossamer.max"), 4.f)));
			const FWRWeatherKind* K = UWRWeatherSubsystem::ActiveKind(this, WRSkyIsles::Biome());
			if (K && K->Id == TEXT("rain")) N *= int32(Tn(TEXT("skyWorld.gossamer.rainMult"), 2.f));
		}
		if (Inv && N > 0)
		{
			const int32 Left = Inv->AddItem(G.Key, N);
			if (N - Left > 0) Got.Add(FString::Printf(TEXT("%d %s"), N - Left, *ItemName(G.Key)));
		}
	}
	if (Xp > 0)
	{
		if (AWRPlayerState* PS = P->GetPlayerState<AWRPlayerState>())
		{
			if (UWRPathComponent* Paths = PS->GetPaths())
			{
				// The Wind path's XP when you walk it; your main path's otherwise.
				const FName Main = Paths->GetMainPath();
				if (!Main.IsNone()) Paths->AwardXP(Main, Xp);
			}
		}
	}
	FString Seen;
	if (RevealRadius > 0.f)
	{
		// A vista: from here you can name what you see (every place within its reach).
		TArray<FString> Names;
		for (const FWRSkyIsleData::FSite& S : FWRSkyIsleData::Get().Sites)
		{
			if (S.Name.IsEmpty() || S.bSecret || S.Kind == TEXT("weeping_cave") || S.Island == TEXT("kite_barrow") || S.Island == TEXT("tube_isle")) continue;
			if (FVector::Dist2D(S.At, GetActorLocation()) < RevealRadius) Names.AddUnique(S.Name);
		}
		if (Names.Num()) Seen = TEXT(". From here you can see ") + FString::Join(Names, TEXT(", "));
	}
	if (!Quest.IsNone())
	{
		if (UWRWorldSystems* Sys = GetWorld()->GetSubsystem<UWRWorldSystems>())
		{
			if (AWRPlayerState* PS = P->GetPlayerState<AWRPlayerState>()) Sys->QuestEvent(PS, TEXT("sky"), Quest);
		}
	}
	if (Kind == TEXT("rose"))
	{
		if (AWRSkyState* S = AWRSkyState::Get(this)) S->RoseTurnedAt = Now(this);
		WRSound::Broadcast(this, TEXT("sky.rose_turn"));
	}
	if (Kind == TEXT("bowls"))
	{
		// Taking the toll wakes the Sentinel at once (WRSkyBosses listens for it).
		WRSound::Broadcast(this, TEXT("sky.sentinel_wake"));
		if (UWRSkyWorld* W = UWRSkyWorld::Get(this)) W->OnTollTaken.Broadcast(P);
	}
	FString Msg = Title;
	if (Got.Num()) Msg += TEXT(": ") + FString::Join(Got, TEXT(", "));
	if (Xp > 0) Msg += FString::Printf(TEXT(" (+%d path XP)"), Xp);
	Tell(P, Msg + Seen);
}

// ================================================================================================ AWRSkyLift

AWRSkyLift::AWRSkyLift()
{
	bReplicates = true;
	bAlwaysRelevant = false;
	SetNetCullDistanceSquared(FMath::Square(120000.f));
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;
	SetNetUpdateFrequency(4.f);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRSkyLift::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRSkyLift, Rise);
	DOREPLIFETIME(AWRSkyLift, FaceYaw);
	DOREPLIFETIME(AWRSkyLift, bUp);
	DOREPLIFETIME(AWRSkyLift, bMoving);
	DOREPLIFETIME(AWRSkyLift, TripStart);
	DOREPLIFETIME(AWRSkyLift, TripFrom);
}

float AWRSkyLift::Seconds() const { return Tn(TEXT("skyWorld.lift.seconds"), 40.f); }

float AWRSkyLift::PositionAt(float S) const
{
	if (!bMoving) return TripFrom;
	// A steady crank, eased 1.5 s at each end of the whole rise (never a snap at a landing).
	const float Total = Seconds();
	const float Ramp = Tn(TEXT("skyWorld.lift.ease"), 1.5f) / Total;
	const float Dist = bUp ? 1.f - TripFrom : TripFrom;
	const float Elapsed = FMath::Max(0.f, S - TripStart) / Total;
	const float Travelled = FMath::Min(Dist, Elapsed);
	float P = bUp ? TripFrom + Travelled : TripFrom - Travelled;
	// Soft ends: within the ramp of a landing the cage slows into it.
	const float ToEnd = bUp ? 1.f - P : P;
	if (ToEnd < Ramp) { const float K = Ramp > 0.f ? ToEnd / Ramp : 0.f; P = bUp ? 1.f - Ramp * K * K : Ramp * K * K; }
	return FMath::Clamp(P, 0.f, 1.f);
}

float AWRSkyLift::CurrentPosition() const { return PositionAt(Now(this)); }

void AWRSkyLift::OnRep_Trip() {}

bool AWRSkyLift::Near(const FVector& At) const
{
	const FVector Foot = GetActorLocation();
	const FVector Top = Foot + FVector(0, 0, Rise);
	const FVector Cg = Foot + FVector(0, 0, Rise * CurrentPosition());
	const float R = Tn(TEXT("skyWorld.lift.reach"), 350.f);
	return FVector::Dist(At, Foot + FRotator(0.f, FaceYaw, 0.f).RotateVector(FVector(-420.f, 300.f, 100.f))) < R
		|| FVector::Dist(At, Top + FRotator(0.f, FaceYaw, 0.f).RotateVector(FVector(-420.f, 300.f, 100.f))) < R
		|| (FVector::Dist2D(At, Cg) < 160.f && At.Z > Cg.Z - 50.f && At.Z < Cg.Z + 300.f)
		|| FVector::Dist(At, Foot) < R || FVector::Dist(At, Top) < R;
}

FString AWRSkyLift::Use(AWRCharacterBase* Who)
{
	if (!HasAuthority() || !Who) return FString();
	const float S = Now(this);
	const float Pos = PositionAt(S);
	if (bMoving && ((bUp && Pos < 1.f) || (!bUp && Pos > 0.f)))
	{
		// A crank mid-trip reverses it.
		TripFrom = Pos;
		TripStart = S;
		bUp = !bUp;
		Cranker = Who;
		WRSound::Broadcast(this, TEXT("sky.lift_pawl"));
		return bUp ? TEXT("You throw the winch over: up") : TEXT("You throw the winch over: down");
	}
	TripFrom = Pos;
	TripStart = S;
	bUp = Pos < 0.5f;
	bMoving = true;
	Cranker = Who;
	WRSound::Broadcast(this, TEXT("sky.lift_start"));
	return bUp ? TEXT("You crank the lift: stay at the winch or in the cage (40 s up)") : TEXT("You let the cage down: stay at the winch or in it");
}

void AWRSkyLift::BeginPlay()
{
	Super::BeginPlay();
	BuildLook();
}

void AWRSkyLift::BuildLook()
{
	if (bBuilt) return;
	bBuilt = true;
	const bool bServer = GetNetMode() == NM_DedicatedServer;
	const FRotator Face(0.f, FaceYaw, 0.f);
	SetActorRotation(Face);
	// The headframe on the island's lip (Rise up): two timber A-frames 9 m, a cross-head, the 3.2 m iron sheave over the
	// cage and the counterweight's falls; the winch (drum, crank, pawl) beside it. The foot stage on the cliff top.
	AWRSkyScenery* Frame = nullptr;
	{
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Frame = GetWorld()->SpawnActor<AWRSkyScenery>(AWRSkyScenery::StaticClass(), GetActorTransform(), SP);
	}
	if (Frame)
	{
		WRForestLook::FBuild B(Frame, 0.f);
		const FVector Top(0, 0, Rise);
		// Stages: the foot's timber deck on the cliff top, the top's deck out over the lip.
		for (const FVector& St : { FVector(0, 0, 0), Top })
		{
			B.K.Add(SkPlank(0.85f), FVector(520.f, 420.f, 20.f), St + FVector(-150.f, 0, -10.f), FRotator::ZeroRotator);
			B.Box(St + FVector(-150.f, 0, -10.f), FVector(260.f, 210.f, 10.f));
			for (int32 c = 0; c < 4; ++c) B.K.Rod(SkCedar(0.8f), St + FVector(-150.f + (c % 2 ? 250.f : -250.f), (c / 2 ? 200.f : -200.f), -10.f), St + FVector(-150.f + (c % 2 ? 250.f : -250.f), (c / 2 ? 200.f : -200.f), -350.f), 14.f);
			// The gate rail round the stage's open edge (the cage's side open).
			B.K.Rod(SkCedar(0.85f), St + FVector(-400.f, -200.f, 100.f), St + FVector(-400.f, 200.f, 100.f), 8.f);
		}
		// The headframe: A-frames either side of the cage shaft.
		for (int32 e = -1; e <= 1; e += 2)
		{
			const FVector Y(0, 220.f * e, 0);
			B.K.Rod(SkCedar(0.85f), Top + Y + FVector(-260.f, 0, 0), Top + Y + FVector(0, 0, 900.f), 26.f, true);
			B.K.Rod(SkCedar(0.85f), Top + Y + FVector(260.f, 0, 0), Top + Y + FVector(0, 0, 900.f), 26.f, true);
			B.K.Rod(SkCedar(0.8f), Top + Y + FVector(-160.f, 0, 350.f), Top + Y + FVector(160.f, 0, 350.f), 16.f, true);
			B.Box(Top + Y + FVector(0, 0, 450.f), FVector(30.f, 20.f, 450.f));
		}
		B.K.Rod(SkCedar(0.9f), Top + FVector(0, -260.f, 900.f), Top + FVector(0, 260.f, 900.f), 34.f, true);
		// The winch beside the frame: drum, spokes, a crank handle, the pawl on its ratchet.
		const FVector Wn = Top + FVector(-420.f, 300.f, 0.f);
		B.K.Add(SkCedar(0.85f), FVector(120.f, 120.f, 160.f), Wn + FVector(0, 0, 90.f), FRotator(0.f, 0.f, 90.f), EWRLookMesh::Cylinder);
		for (int32 e = -1; e <= 1; e += 2) B.K.Add(SkCedar(0.75f), FVector(30.f, 160.f, 140.f), Wn + FVector(0, 90.f * e, 70.f), FRotator::ZeroRotator);
		B.K.Disc(SkRustIron(), Wn + FVector(0, 96.f, 90.f), FVector(0, 1, 0), 140.f, 10.f);
		B.Box(Wn + FVector(0, 0, 80.f), FVector(70.f, 100.f, 80.f));
		const FVector WnF = FVector(-420.f, 300.f, 0.f);
		B.K.Add(SkCedar(0.85f), FVector(120.f, 120.f, 160.f), WnF + FVector(0, 0, 90.f), FRotator(0.f, 0.f, 90.f), EWRLookMesh::Cylinder);
		B.Box(WnF + FVector(0, 0, 80.f), FVector(70.f, 100.f, 80.f));
		// The brazier on the headframe's top (L6's night read).
		SkBrazier(B, Top + FVector(-200.f, -320.f, 0.f), 3200.f);
		B.Finish();
		Frame->SetOwner(this);
	}
	// The moving parts: the cage (iron frame, plank floor, waist rails, a gate on the stage side), the counterweight
	// (a 2 m stone block in an iron cradle), the sheave (turns by the chain's travel), the crank on the top winch.
	auto Part = [this](const FVector& At) { USceneComponent* C = NewObject<USceneComponent>(this); C->SetupAttachment(GetRootComponent()); C->SetRelativeLocation(At); C->RegisterComponent(); return C; };
	Cage = Part(FVector::ZeroVector);
	Weight = Part(FVector(260.f, 0, Rise));
	Sheave = Part(FVector(0, 0, Rise + 900.f));
	Crank = Part(FVector(-420.f, 400.f, Rise + 90.f));
	auto Blocks = [this, bServer](USceneComponent* Under, TFunctionRef<void(WRLookKit::FKit&)> Fill)
	{
		if (bServer || !Under) return;
		TArray<FWRLookBlock> Bl;
		WRLookKit::FKit K(Under->GetComponentTransform(), Bl);
		Fill(K);
		FWRBuildLook::Spawn(this, Under, Bl, FLinearColor(0.25f, 0.2f, 0.15f), false);
	};
	Blocks(Cage, [](WRLookKit::FKit& K)
	{
		for (int32 c = 0; c < 4; ++c) K.Rod(SkRustIron(), FVector((c % 2 ? 120.f : -120.f), (c / 2 ? 120.f : -120.f), 0), FVector((c % 2 ? 120.f : -120.f), (c / 2 ? 120.f : -120.f), 260.f), 8.f, true);
		for (int32 r = 0; r < 4; ++r)
		{
			const float Z = r < 2 ? 100.f + r * 10.f : 255.f;
			K.Rod(SkRustIron(), FVector(-120, -120, Z), FVector(120, -120, Z), 5.f);
			K.Rod(SkRustIron(), FVector(-120, 120, Z), FVector(120, 120, Z), 5.f);
			K.Rod(SkRustIron(), FVector(120, -120, Z), FVector(120, 120, Z), 5.f);
			if (r != 0) K.Rod(SkRustIron(), FVector(-120, -120, Z), FVector(-120, 120, Z), 5.f);
		}
		K.Add(SkPlank(0.85f), FVector(240.f, 240.f, 10.f), FVector(0, 0, 0), FRotator::ZeroRotator);
		K.Rod(SkRustIron(), FVector(0, 0, 260.f), FVector(0, 0, 300.f), 10.f);
		K.Add(SkLampGlow(5.f), FVector(20.f, 20.f, 30.f), FVector(110.f, 110.f, 230.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
	});
	// The cage's collision: floor and the three rails (the open side is the gate onto each stage).
	for (const FVector& Bx : { FVector(0, 0, 0) })
	{
		UBoxComponent* F = NewObject<UBoxComponent>(this);
		F->SetupAttachment(Cage);
		F->SetBoxExtent(FVector(120.f, 120.f, 6.f));
		F->SetRelativeLocation(Bx);
		F->SetCollisionProfileName(TEXT("BlockAll"));
		F->RegisterComponent();
	}
	for (int32 s = 0; s < 3; ++s)
	{
		UBoxComponent* Rail = NewObject<UBoxComponent>(this);
		Rail->SetupAttachment(Cage);
		Rail->SetBoxExtent(s < 2 ? FVector(120.f, 4.f, 55.f) : FVector(4.f, 120.f, 55.f));
		Rail->SetRelativeLocation(s == 0 ? FVector(0, -120.f, 55.f) : s == 1 ? FVector(0, 120.f, 55.f) : FVector(120.f, 0, 55.f));
		Rail->SetCollisionProfileName(TEXT("BlockAll"));
		Rail->RegisterComponent();
	}
	Blocks(Weight, [](WRLookKit::FKit& K)
	{
		K.Add(SkCliffGranite(0.9f), FVector(200.f, 150.f, 200.f), FVector(0, 0, -120.f), FRotator::ZeroRotator);
		for (int32 b = 0; b < 3; ++b) K.Add(SkRustIron(), FVector(212.f, 162.f, 14.f), FVector(0, 0, -200.f + b * 80.f), FRotator::ZeroRotator);
		K.Rod(SkRustIron(), FVector(0, 0, -20.f), FVector(0, 0, 40.f), 10.f);
	});
	Blocks(Sheave, [](WRLookKit::FKit& K)
	{
		K.Disc(SkRustIron(), FVector::ZeroVector, FVector(0, 1, 0), 320.f, 14.f);
		for (int32 s = 0; s < 8; ++s) { const float A = s * PI / 4.f; K.Rod(SkRustIron(), FVector::ZeroVector, FVector(FMath::Cos(A) * 150.f, 0, FMath::Sin(A) * 150.f), 8.f); }
		K.Add(SkBronze(), FVector(40.f, 60.f, 40.f), FVector::ZeroVector, FRotator(0.f, 0.f, 90.f), EWRLookMesh::Cylinder);
	});
	Blocks(Crank, [](WRLookKit::FKit& K)
	{
		K.Rod(SkRustIron(), FVector(0, 0, 0), FVector(0, 0, 60.f), 6.f);
		K.Rod(SkCedar(0.9f), FVector(0, 0, 60.f), FVector(0, 30.f, 60.f), 7.f);
	});
	// The chains: from the sheave down to the cage's top, and its far fall to the counterweight (rebuilt as they run).
	if (!bServer)
	{
		CageChains = NewObject<UProceduralMeshComponent>(this);
		CageChains->SetupAttachment(GetRootComponent());
		CageChains->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		CageChains->RegisterComponent();
		WeightChains = NewObject<UProceduralMeshComponent>(this);
		WeightChains->SetupAttachment(GetRootComponent());
		WeightChains->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WeightChains->RegisterComponent();
	}
	Place(CurrentPosition());
}

void AWRSkyLift::Place(float Pos)
{
	if (!Cage) return;
	const float CageZ = Rise * Pos;
	Cage->SetRelativeLocation(FVector(0, 0, CageZ));
	// The counterweight goes the other way the same distance.
	Weight->SetRelativeLocation(FVector(260.f, 0, Rise - CageZ + 400.f));
	// The sheave turns by the chain's travel (circumference 3.2 m x pi); the crank 6 turns a metre (geared down).
	const float Travel = CageZ;
	Sheave->SetRelativeRotation(FRotator(-FMath::RadiansToDegrees(Travel / 160.f), 0.f, 0.f));
	Crank->SetRelativeRotation(FRotator(0.f, 0.f, Travel * 0.06f * 360.f / 100.f));
	if (CageChains && FMath::Abs(Pos - LastPos) > 0.0005f)
	{
		LastPos = Pos;
		FWRToolMesh M;
		const FVector Sh(0, 0, Rise + 900.f);
		for (int32 e = -1; e <= 1; e += 2)
		{
			M.Tube({ Sh + FVector(-150.f, 20.f * e, 0), FVector(0, 20.f * e, CageZ + 300.f) }, 3.5f, 6, SkChainC(), 0.7f);
			M.Tube({ Sh + FVector(150.f, 20.f * e, 0), FVector(260.f, 20.f * e, Rise - CageZ + 440.f) }, 3.5f, 6, SkChainC(), 0.7f);
		}
		M.ApplyTo(CageChains);
	}
}

void AWRSkyLift::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float S = Now(this);
	const float Pos = PositionAt(S);
	Place(Pos);
	const bool bRunning = bMoving && ((bUp && Pos < 1.f) || (!bUp && Pos > 0.f));
	// The chain's rattle and the winch's clack while it runs (every machine).
	if (GetNetMode() != NM_DedicatedServer)
	{
		if (bRunning && !Rattle) Rattle = WRSound::StartLoop(this, TEXT("sky.lift_chain"));
		if (!bRunning && Rattle) { Rattle->FadeOut(0.4f, 0.f); Rattle = nullptr; WRSound::PlayAt(this, TEXT("sky.lift_stop"), GetActorLocation() + FVector(0, 0, Rise * Pos)); }
		if (Rattle) Rattle->SetWorldLocation(GetActorLocation() + FVector(0, 0, Rise * Pos + 150.f));
	}
	if (!HasAuthority() || !bMoving) return;
	if (!bRunning)
	{
		bMoving = false;
		TripFrom = Pos >= 0.5f ? 1.f : 0.f;
		Cranker = nullptr;
		return;
	}
	// The pawl holds it the moment nobody is cranking: the cranker left the winch and the cage.
	AWRCharacterBase* C = Cranker.Get();
	if (!C || !C->IsAlive() || !Near(C->GetActorLocation()))
	{
		bool bRider = false;
		for (TActorIterator<AWRCharacter> It(GetWorld()); It; ++It)
		{
			if (It->IsAlive() && FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) < 160.f && FMath::Abs(It->GetActorLocation().Z - (GetActorLocation().Z + Rise * Pos + 90.f)) < 200.f)
			{
				bRider = true;
				Cranker = *It;
				break;
			}
		}
		if (!bRider)
		{
			bMoving = false;
			TripFrom = Pos;
			WRSound::Broadcast(this, TEXT("sky.lift_pawl"));
		}
	}
}

// ================================================================================================ AWRSkyShard

AWRSkyShard::AWRSkyShard()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	SetNetUpdateFrequency(2.f);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRSkyShard::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRSkyShard, BornAt);
	DOREPLIFETIME(AWRSkyShard, Origin);
	DOREPLIFETIME(AWRSkyShard, Count);
	DOREPLIFETIME(AWRSkyShard, ItemId);
}

FVector AWRSkyShard::WhereAt(float S) const
{
	// Up 1 m over 0.5 s, hangs (bobbing) until 3 s, then drifts up and away on the wind, gone by 8 s.
	const float T = S - BornAt;
	const float Hang = Tn(TEXT("skyWorld.shard.grabSeconds"), 3.f);
	FVector P = FVector(Origin) + FVector(0, 0, 100.f * Ease(T / 0.5f) + 6.f * FMath::Sin(T * 3.f));
	if (T > Hang)
	{
		const float U = T - Hang;
		const FVector2D W = WRSkyIsles::WindDir(this);
		P += FVector(W * 80.f * U * U * 0.5f, 120.f * U * U);
	}
	return P;
}

void AWRSkyShard::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer) return;
	Look = NewObject<USceneComponent>(this);
	Look->SetupAttachment(GetRootComponent());
	Look->RegisterComponent();
	TArray<FWRLookBlock> Bl;
	WRLookKit::FKit K(GetActorTransform(), Bl);
	for (int32 c = 0; c < FMath::Clamp(Count, 1, 6); ++c)
	{
		const FVector D = FVector(FMath::Cos(c * 2.1f) * 0.4f, FMath::Sin(c * 2.1f) * 0.4f, 1.f).GetSafeNormal();
		K.Spike(SkLiftGlow(3.f), FVector(FMath::Cos(c * 2.1f) * 10.f, FMath::Sin(c * 2.1f) * 10.f, -10.f), D, 10.f, 30.f + 6.f * c);
		K.Spike(SkLiftGlow(3.f), FVector(FMath::Cos(c * 2.1f) * 10.f, FMath::Sin(c * 2.1f) * 10.f, -10.f), -D, 8.f, 14.f);
	}
	FWRBuildLook::Spawn(this, Look, Bl, FLinearColor::Black, false);
	Glow = NewObject<UPointLightComponent>(this);
	Glow->SetupAttachment(Look);
	Glow->SetLightColor(SkSRGB(0xCF, 0xE9, 0xFF));
	Glow->SetIntensity(1800.f);
	Glow->SetAttenuationRadius(500.f);
	Glow->SetCastShadows(false);
	Glow->RegisterComponent();
	WRSound::PlayAt(this, TEXT("sky.shard_float"), GetActorLocation());
}

bool AWRSkyShard::TryTake(AWRCharacterBase* Who)
{
	if (!HasAuthority() || !Who || IsActorBeingDestroyed()) return false;
	const float T = Now(this) - BornAt;
	if (T > Tn(TEXT("skyWorld.shard.grabSeconds"), 3.f) + 0.6f) return false;
	if (T < 1.f && Striker.IsValid() && Striker.Get() != Who) return false;
	UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Who);
	if (!Inv) return false;
	const int32 Left = Inv->AddItem(ItemId, Count);
	if (Left >= Count) { Tell(Who, TEXT("Your bag is full")); return false; }
	Tell(Who, FString::Printf(TEXT("+%d %s"), Count - Left, *ItemName(ItemId)));
	WRSound::PlayAt(this, TEXT("sky.shard_take"), WhereAt(Now(this)));
	Destroy();
	return true;
}

void AWRSkyShard::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float S = Now(this);
	const FVector P = WhereAt(S);
	if (Look) { Look->SetWorldLocation(P); Look->AddLocalRotation(FRotator(0.f, 40.f * DeltaSeconds, 0.f)); }
	if (Glow) Glow->SetIntensity(1800.f * (0.75f + 0.25f * FMath::Sin(S * 6.f)));
	if (!HasAuthority()) return;
	// Walking into it takes it.
	for (TActorIterator<AWRCharacter> It(GetWorld()); It; ++It)
	{
		if (It->IsAlive() && FVector::Dist(It->GetActorLocation(), P) < 140.f && TryTake(*It)) return;
	}
	if (S - BornAt > Tn(TEXT("skyWorld.shard.goneSeconds"), 8.f)) Destroy();
}

// ================================================================================================ AWRSkyLine

AWRSkyLine::AWRSkyLine()
{
	bReplicates = false;
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

FVector AWRSkyLine::PointAt(float T) const
{
	return FMath::Lerp(A, B, T) - FVector(0, 0, 4.f * Sag * T * (1.f - T));
}

float AWRSkyLine::Nearest(const FVector& P, float& OutDist) const
{
	float BestT = 0.f, Best = TNumericLimits<float>::Max();
	for (int32 i = 0; i <= 64; ++i)
	{
		const float T = i / 64.f;
		const float D = FVector::Dist(P, PointAt(T));
		if (D < Best) { Best = D; BestT = T; }
	}
	// Refine on the segment round the best.
	for (int32 i = -8; i <= 8; ++i)
	{
		const float T = FMath::Clamp(BestT + i / 512.f, 0.f, 1.f);
		const float D = FVector::Dist(P, PointAt(T));
		if (D < Best) { Best = D; BestT = T; }
	}
	OutDist = Best;
	return BestT;
}

bool AWRSkyLine::Holds(const AActor* Who) const
{
	return Climbers.ContainsByPredicate([Who](const FClimber& C) { return C.Who.Get() == Who; });
}

FString AWRSkyLine::Grab(AWRCharacterBase* Who)
{
	if (!Who) return FString();
	if (Holds(Who)) { Release(Who, false); return TEXT("You let go of the line"); }
	float Dist = 0.f;
	const float T = Nearest(Who->GetActorLocation(), Dist);
	if (Dist > Tn(TEXT("skyWorld.line.reach"), 260.f)) return TEXT("Too far from the line");
	Climbers.Add({ Who, T });
	if (UCharacterMovementComponent* M = Who->GetCharacterMovement())
	{
		M->StopMovementImmediately();
		M->SetMovementMode(MOVE_Flying);
	}
	WRSound::Broadcast(Who, TEXT("sky.line_grab"));
	return TEXT("On the line: climb up or down, Jump to let go");
}

void AWRSkyLine::Release(AWRCharacterBase* Who, bool bJump)
{
	Climbers.RemoveAll([Who](const FClimber& C) { return C.Who.Get() == Who || !C.Who.IsValid(); });
	if (!Who) return;
	if (UCharacterMovementComponent* M = Who->GetCharacterMovement())
	{
		M->SetMovementMode(MOVE_Falling);
		if (bJump) Who->LaunchCharacter(FVector(0, 0, 300.f) - (B - A).GetSafeNormal2D() * 250.f, true, true);
	}
}

void AWRSkyLine::BuildLook()
{
	if (GetNetMode() == NM_DedicatedServer) return;
	// Silk-and-chain: a chain core with silk wound round it in long spirals, knots every 2 m to hold on by.
	FWRToolMesh M;
	TArray<FVector> Pts;
	const int32 N = FMath::Clamp(int32(FVector::Dist(A, B) / 300.f), 8, 120);
	for (int32 i = 0; i <= N; ++i) Pts.Add(PointAt(float(i) / N) - GetActorLocation());
	M.Tube(Pts, 3.f, 6, SkChainC(), 0.7f);
	TArray<FVector> Spiral;
	for (int32 i = 0; i <= N * 4; ++i)
	{
		const float T = float(i) / (N * 4);
		const FVector P = PointAt(T) - GetActorLocation();
		Spiral.Add(P + FVector(FMath::Cos(i * 1.3f) * 4.f, FMath::Sin(i * 1.3f) * 4.f, 0.f));
	}
	M.Tube(Spiral, 1.2f, 4, SkSRGB(0xE8, 0xEE, 0xF2) * 0.6f, 0.9f);
	UProceduralMeshComponent* P = NewObject<UProceduralMeshComponent>(this);
	P->SetupAttachment(GetRootComponent());
	P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	P->RegisterComponent();
	M.ApplyTo(P);
}

void AWRSkyLine::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority()) return;
	// Holds every climber on the line: 3 m/s up or down by their move input, no stamina; Jump lets go; a hit knocks
	// them off. The pawn is placed by the server (its movement replicates as any does).
	const float Speed = Tn(TEXT("skyWorld.line.climbSpeed"), 300.f) / FMath::Max(100.f, FVector::Dist(A, B));
	for (int32 i = Climbers.Num() - 1; i >= 0; --i)
	{
		FClimber& C = Climbers[i];
		AWRCharacterBase* Who = C.Who.Get();
		if (!Who || !Who->IsAlive() || Who->HasTag(WRTags::Status_Staggered)) { Release(Who, false); continue; }
		UCharacterMovementComponent* M = Who->GetCharacterMovement();
		if (Who->bPressedJump) { Who->bPressedJump = false; Release(Who, true); continue; }
		const FVector Acc = M ? M->GetCurrentAcceleration() : FVector::ZeroVector;
		const FVector Up = (PointAt(FMath::Min(1.f, C.T + 0.01f)) - PointAt(FMath::Max(0.f, C.T - 0.01f))).GetSafeNormal();
		float Dir = 0.f;
		if (Acc.SizeSquared() > 1.f)
		{
			// Forward on the stick climbs toward B if the line runs away from the camera, else down (the line's
			// up is what forward means when you face it).
			const FVector Look = Who->GetControlRotation().Vector();
			Dir = FVector::DotProduct(Acc.GetSafeNormal(), Look.GetSafeNormal2D()) >= 0.f ? 1.f : -1.f;
			if (FVector::DotProduct(Up.GetSafeNormal2D(), Look.GetSafeNormal2D()) < -0.2f) Dir = -Dir;
			if (FMath::Abs(Up.Z) > 0.8f) Dir = FVector::DotProduct(Acc.GetSafeNormal(), Look.GetSafeNormal2D()) >= 0.f ? (Up.Z > 0.f ? 1.f : -1.f) : (Up.Z > 0.f ? -1.f : 1.f);
		}
		C.T = FMath::Clamp(C.T + Dir * Speed * DeltaSeconds, 0.f, 1.f);
		const float Half = Who->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const FVector Side = FVector::CrossProduct(Up, FVector::UpVector).GetSafeNormal();
		Who->SetActorLocation(PointAt(C.T) + Side * 30.f - FVector(0, 0, Half * 0.3f), false, nullptr, ETeleportType::TeleportPhysics);
		if (M) M->Velocity = FVector::ZeroVector;
		// At an end: step off onto the ground there.
		if ((C.T >= 1.f && Dir > 0.f) || (C.T <= 0.f && Dir < 0.f))
		{
			const FVector End = C.T >= 1.f ? B : A;
			Who->SetActorLocation(End + FVector(0, 0, Half + 20.f) - (C.T >= 1.f ? (B - A).GetSafeNormal2D() * -150.f : (B - A).GetSafeNormal2D() * 150.f), false, nullptr, ETeleportType::TeleportPhysics);
			Release(Who, false);
		}
	}
}

// ================================================================================================ AWRSkyStones

AWRSkyStones::AWRSkyStones()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	SetNetUpdateFrequency(4.f);
	SetNetCullDistanceSquared(FMath::Square(100000.f));
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRSkyStones::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRSkyStones, Line);
	DOREPLIFETIME(AWRSkyStones, SetOffAt);
}

float AWRSkyStones::DropAt(float Since)
{
	// 1.0 s tell (flicker, dust, groan), a 1 m dip, sinking 4 m/s for 6 s, holding 4 s, rising back over 20 s.
	const float Tell = Tn(TEXT("hazards.sky.stone.tell"), 1.f);
	const float Sink = Tn(TEXT("hazards.sky.stone.sinkSeconds"), 6.f), Speed = Tn(TEXT("hazards.sky.stone.sinkSpeed"), 400.f);
	const float Hold = Tn(TEXT("hazards.sky.stone.hold"), 4.f), Rise = Tn(TEXT("hazards.sky.stone.riseSeconds"), 20.f);
	if (Since < 0.f) return 0.f;
	if (Since < Tell) return 0.f;
	const float T = Since - Tell;
	const float Deep = 100.f + Speed * Sink;
	if (T < 0.4f) return 100.f * Ease(T / 0.4f);
	if (T < 0.4f + Sink) return 100.f + Speed * (T - 0.4f);
	if (T < 0.4f + Sink + Hold) return Deep;
	return Deep * (1.f - Ease((T - 0.4f - Sink - Hold) / Rise));
}

void AWRSkyStones::BeginPlay()
{
	Super::BeginPlay();
	const FWRSkyIsleData& D = FWRSkyIsleData::Get();
	for (int32 i = 0; i < D.Stones.Num(); ++i) if (D.Stones[i].Line == Line) Stones.Add(i);
	if (HasAuthority()) SetOffAt.Init(-1000.f, Stones.Num());
	BuildLook();
}

void AWRSkyStones::BuildLook()
{
	const FWRSkyIsleData& D = FWRSkyIsleData::Get();
	const bool bServer = GetNetMode() == NM_DedicatedServer;
	for (int32 k = 0; k < Stones.Num(); ++k)
	{
		const FWRSkyIsleData::FStone& S = D.Stones[Stones[k]];
		USceneComponent* P = NewObject<USceneComponent>(this);
		P->SetupAttachment(GetRootComponent());
		P->SetWorldLocation(S.At);
		P->RegisterComponent();
		Parts.Add(P);
		// A flat levistone stone: a slab of pale rock with a keel of blue crystal under it (tired: dull in places).
		UBoxComponent* Bx = NewObject<UBoxComponent>(this);
		Bx->SetupAttachment(P);
		Bx->SetBoxExtent(FVector(S.Size * 0.42f, S.Size * 0.36f, 40.f));
		Bx->SetRelativeLocation(FVector(0, 0, -40.f));
		Bx->SetCollisionProfileName(TEXT("BlockAll"));
		Bx->RegisterComponent();
		Boxes.Add(Bx);
		USceneComponent* G = NewObject<USceneComponent>(this);
		G->SetupAttachment(P);
		G->RegisterComponent();
		Glows.Add(G);
		Told.Add(false);
		if (bServer) continue;
		TArray<FWRLookBlock> Bl, Gl;
		WRLookKit::FKit K(P->GetComponentTransform(), Bl), KG(G->GetComponentTransform(), Gl);
		K.Add(SkLimestone(0.85f), FVector(S.Size * 0.85f, S.Size * 0.72f, 80.f), FVector(0, 0, -40.f), FRotator(0.f, k * 37.f, 0.f), EWRLookMesh::Cylinder);
		for (int32 c = 0; c < 5; ++c)
		{
			const FVector Dir = FVector(FMath::Cos(c * 1.3f) * 0.4f, FMath::Sin(c * 1.3f) * 0.4f, -1.f).GetSafeNormal();
			K.Spike(SkLevistone(0.5f), FVector(FMath::Cos(c * 1.3f) * S.Size * 0.15f, FMath::Sin(c * 1.3f) * S.Size * 0.15f, -70.f), Dir, S.Size * 0.12f, S.Size * 0.35f);
			KG.Spike(SkLiftGlow(5.f), FVector(FMath::Cos(c * 1.3f) * S.Size * 0.15f, FMath::Sin(c * 1.3f) * S.Size * 0.15f, -70.f), Dir, S.Size * 0.07f, S.Size * 0.36f);
		}
		FWRBuildLook::Spawn(this, P, Bl, FLinearColor::Black, false);
		FWRBuildLook::Spawn(this, G, Gl, FLinearColor::Black, false);
	}
}

void AWRSkyStones::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const FWRSkyIsleData& D = FWRSkyIsleData::Get();
	const float S = Now(this);
	const float Tell = Tn(TEXT("hazards.sky.stone.tell"), 1.f);
	for (int32 k = 0; k < Parts.Num() && k < Stones.Num(); ++k)
	{
		const float At = SetOffAt.IsValidIndex(k) ? SetOffAt[k] : -1000.f;
		const float Since = S - At;
		const FVector Home = D.Stones[Stones[k]].At;
		// A gentle float when at rest (they bob a hand's width), the drop when set off.
		const float Bob = 4.f * FMath::Sin(S * 0.8f + k);
		if (Parts[k]) Parts[k]->SetWorldLocation(Home - FVector(0, 0, DropAt(Since) - Bob));
		// The tell: the keel's glow flickers and dims, dust trickles, it groans (every machine, once per set-off).
		const bool bTelling = Since >= 0.f && Since < Tell + 0.4f + Tn(TEXT("hazards.sky.stone.sinkSeconds"), 6.f);
		if (Glows.IsValidIndex(k) && Glows[k])
		{
			const bool bShow = !bTelling || FMath::Frac(S * 7.f + k * 0.3f) > (Since < Tell ? 0.5f : 0.85f);
			if (Glows[k]->IsVisible() != bShow) Glows[k]->SetVisibility(bShow, true);
		}
		if (bTelling && Since < Tell && !Told[k])
		{
			Told[k] = true;
			if (GetNetMode() != NM_DedicatedServer)
			{
				WRSound::PlayAt(this, TEXT("sky.stone_groan"), Home);
				WRFx::SpawnAt(this, TEXT("sky.stoneDust"), Home - FVector(0, 0, 60.f));
			}
		}
		if (!bTelling) Told[k] = false;
	}
	if (!HasAuthority()) return;
	// Land on a stone at rest and it starts (anyone standing on it: a survivor or a creature).
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		const UCharacterMovementComponent* M = It->GetCharacterMovement();
		if (!M || !M->IsMovingOnGround() || !It->IsAlive()) continue;
		const UObject* Base = It->GetMovementBaseObject();
		for (int32 k = 0; k < Boxes.Num(); ++k)
		{
			if (Base != Boxes[k]) continue;
			if (S - SetOffAt[k] > DropTotal()) { SetOffAt[k] = S; ForceNetUpdate(); }
		}
	}
}

float AWRSkyStones::DropTotal()
{
	return Tn(TEXT("hazards.sky.stone.tell"), 1.f) + 0.4f + Tn(TEXT("hazards.sky.stone.sinkSeconds"), 6.f) + Tn(TEXT("hazards.sky.stone.hold"), 4.f) + Tn(TEXT("hazards.sky.stone.riseSeconds"), 20.f);
}

// ================================================================================================ AWRSkyEventLook

AWRSkyEventLook::AWRSkyEventLook()
{
	bReplicates = false;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRSkyEventLook::BuildLook()
{
	if (bBuilt) return;
	bBuilt = true;
	// The six cloud bridges, in 8 m sections: each section a walkable box and a lit cloud body (fx + glow blocks), off
	// until the Turning Wind forms them from the ends in.
	const FWRSkyIsleData& D = FWRSkyIsleData::Get();
	const bool bServer = GetNetMode() == NM_DedicatedServer;
	for (int32 b = 0; b < D.CloudBridges.Num(); ++b)
	{
		const FWRSkyIsleData::FCloudBridge& Cb = D.CloudBridges[b];
		const float L = FVector::Dist(Cb.A, Cb.B);
		const int32 N = FMath::Max(2, int32(L / 800.f));
		for (int32 s = 0; s < N; ++s)
		{
			FSeg Sg;
			Sg.Bridge = b;
			Sg.T0 = float(s) / N;
			Sg.T1 = float(s + 1) / N;
			const FVector P0 = FMath::Lerp(Cb.A, Cb.B, Sg.T0) - FVector(0, 0, 4.f * L * 0.01f * Sg.T0 * (1.f - Sg.T0));
			const FVector P1 = FMath::Lerp(Cb.A, Cb.B, Sg.T1) - FVector(0, 0, 4.f * L * 0.01f * Sg.T1 * (1.f - Sg.T1));
			UBoxComponent* Bx = NewObject<UBoxComponent>(this);
			Bx->SetupAttachment(GetRootComponent());
			Bx->SetWorldLocation((P0 + P1) * 0.5f - FVector(0, 0, 30.f));
			Bx->SetWorldRotation((P1 - P0).Rotation());
			Bx->SetBoxExtent(FVector(FVector::Dist(P0, P1) * 0.5f + 20.f, Cb.Width * 0.5f, 30.f));
			Bx->SetCollisionProfileName(TEXT("BlockAll"));
			Bx->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Bx->RegisterComponent();
			SegBoxes.Add(Bx);
			USceneComponent* Lk = NewObject<USceneComponent>(this);
			Lk->SetupAttachment(Bx);
			Lk->RegisterComponent();
			USceneComponent* Fr = NewObject<USceneComponent>(this);
			Fr->SetupAttachment(Bx);
			Fr->RegisterComponent();
			SegLooks.Add(Lk);
			SegFray.Add(Fr);
			Segs.Add(Sg);
			if (!bServer)
			{
				TArray<FWRLookBlock> Bl, Fl;
				WRLookKit::FKit K(Lk->GetComponentTransform(), Bl), KF(Fr->GetComponentTransform(), Fl);
				const float Len = FVector::Dist(P0, P1);
				for (int32 p = 0; p < 4; ++p)
				{
					K.Add(FSurf::Lit(SkSRGB(0xF4, 0xF6, 0xF8) * 0.7f, 1.2f), FVector(Len * 0.4f, Cb.Width * (0.7f + 0.2f * (p % 2)), 80.f), FVector((p - 1.5f) * Len * 0.25f, 0, 10.f + 10.f * (p % 2)), FRotator(0.f, p * 20.f, 0.f), EWRLookMesh::Cylinder);
					KF.Add(FSurf::Lit(SkSRGB(0x9A, 0x9E, 0xA4) * 0.5f, 0.3f), FVector(Len * 0.4f, Cb.Width * 0.6f, 70.f), FVector((p - 1.5f) * Len * 0.25f, 0, 6.f), FRotator(0.f, p * 20.f, 0.f), EWRLookMesh::Cylinder);
				}
				FWRBuildLook::Spawn(this, Lk, Bl, FLinearColor::Black, false);
				FWRBuildLook::Spawn(this, Fr, Fl, FLinearColor::Black, false);
			}
			Lk->SetVisibility(false, true);
			Fr->SetVisibility(false, true);
		}
	}
}

void AWRSkyEventLook::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bBuilt) BuildLook();
	const AWRSkyState* St = AWRSkyState::Get(this);
	const float S = Now(this);
	const float Amount = St ? St->BridgeAmount(S) : 0.f;
	// Sections form from both ends in and fade from the ends in; each section's last second before going is grey and
	// fraying (the tell), then it goes and anything on it falls.
	const float FrayS = Tn(TEXT("skyWorld.turningWind.frayTell"), 1.f) / FMath::Max(1.f, Tn(TEXT("skyWorld.turningWind.bridgeFade"), 60.f));
	const bool bFading = St && S >= St->TurnEndAt && St->TurnEndAt > 0.f;
	for (int32 i = 0; i < Segs.Num(); ++i)
	{
		const FSeg& Sg = Segs[i];
		const float Mid = (Sg.T0 + Sg.T1) * 0.5f;
		const float FromEnd = FMath::Min(Mid, 1.f - Mid) * 2.f;     // 0 at the ends, 1 in the middle
		const bool bOn = Amount > 0.f && (bFading ? FromEnd < Amount : FromEnd < Amount + 0.001f);
		const bool bFray = bFading && bOn && FromEnd > Amount - FrayS;
		if (SegBoxes[i])
		{
			const ECollisionEnabled::Type Want = bOn ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision;
			if (SegBoxes[i]->GetCollisionEnabled() != Want) SegBoxes[i]->SetCollisionEnabled(Want);
		}
		if (SegLooks[i] && SegLooks[i]->IsVisible() != (bOn && !bFray)) SegLooks[i]->SetVisibility(bOn && !bFray, true);
		if (SegFray[i] && SegFray[i]->IsVisible() != bFray) SegFray[i]->SetVisibility(bFray, true);
	}
}

// ================================================================================================ UWRSkyWorld

UWRSkyWorld* UWRSkyWorld::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UWRSkyWorld>() : nullptr;
}

bool UWRSkyWorld::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UWRSkyWorld::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	FString Map = UWorld::RemovePIEPrefix(InWorld.GetMapName());
	Map.RemoveFromStart(InWorld.StreamingLevelsPrefix);
	if (!Map.EndsWith(FWRTuning::Get().Str(TEXT("skyWorld.map"), TEXT("L_Island"))) || !WRSkyIsles::Live()) return;
	bActive = true;
	UE_LOG(LogWildreach, Log, TEXT("SKY live on %s: waiting for the cape's ground"), *Map);
}

void UWRSkyWorld::BuildScenery()
{
	// Every place, in a fixed order (the same on every machine, so each builds the same collision): the islands, the
	// Flue, the bank, the sites, the kit, the bridges, the poles, the falls.
	const FWRSkyIsleData& D = FWRSkyIsleData::Get();
	for (const FWRSkyIsleData::FIsland& I : D.Islands) Pieces.Add({ TEXT("island"), I.Id, I.Id, I.Centre, I.Yaw, I.Index, 0 });
	Pieces.Add({ TEXT("flue"), TEXT("flue"), NAME_None, FVector(D.FlueAt, D.FlueFloor), 0.f, 0, 0 });
	Pieces.Add({ TEXT("bank"), TEXT("bank"), NAME_None, D.UnderAt, 0.f, 0, 0 });
	for (const FWRSkyIsleData::FSite& S : D.Sites) Pieces.Add({ S.Kind, S.Id, S.Island, S.At, S.Yaw, -1, S.Variant });
	for (const FWRSkyIsleData::FSite& S : D.Kit) Pieces.Add({ S.Kind, S.Id, S.Island, S.At, S.Yaw, -1, S.Variant });
	for (int32 i = 0; i < D.Bridges.Num(); ++i)
	{
		const FWRSkyIsleData::FBridge& B = D.Bridges[i];
		const FVector Dir = (B.B - B.A).GetSafeNormal2D();
		Pieces.Add({ TEXT("bridge"), B.Id, NAME_None, B.A, float(FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X))), i, 0 });
	}
	for (int32 i = 0; i < D.Poles.Num(); ++i)
	{
		FVector At = D.Poles[i].At;
		if (const FWRSkyIsleData::FIsland* I = D.IslandAt(At, 400.f, 3000.f, 3000.f)) At.Z = I->TopZ(At);
		Pieces.Add({ TEXT("pole"), NAME_None, NAME_None, At + FVector(0, 0, 0.f), D.Poles[i].Yaw, i, 0 });
	}
	for (int32 i = 0; i < D.Falls.Num(); ++i) Pieces.Add({ TEXT("falls"), NAME_None, NAME_None, D.Falls[i].At, D.Falls[i].Yaw, i, 0 });
	// The landmarks that build off their site: the Great Kite's own piece is its site (L5), the Titan's is L4.
	UE_LOG(LogWildreach, Log, TEXT("SKY scenery queued: %d places"), Pieces.Num());
}

void UWRSkyWorld::StepBuild()
{
	UWorld* World = GetWorld();
	if (!World) return;
	if (!bQueued)
	{
		WaitClock += World->GetDeltaSeconds();
		const FWRLandDef* Def = nullptr;
		for (const FWRLandDef& L : WRLand::All()) if (L.Biome == WRSkyIsles::Biome()) Def = &L;
		AWRLand* Land = Def ? UWRLandSubsystem::FindLand(World, Def->Id) : nullptr;
		if (!Land || !Land->IsGroundReady())
		{
			if (WaitClock > 60.f) { WaitClock = 0.f; UE_LOG(LogWildreach, Warning, TEXT("SKY still waiting for the cape's ground (Data/lands/sky.json)")); }
			return;
		}
		BuildScenery();
		bQueued = true;
	}
	if (!bSceneryDone)
	{
		const int32 PerFrame = FMath::Max(1, int32(Tn(TEXT("skyWorld.buildPerFrame"), 3.f)));
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		for (int32 n = 0; n < PerFrame && PiecesBuilt < Pieces.Num(); ++n, ++PiecesBuilt)
		{
			const FPiece& Pc = Pieces[PiecesBuilt];
			AWRSkyScenery* A = World->SpawnActor<AWRSkyScenery>(AWRSkyScenery::StaticClass(), FTransform(FRotator(0.f, Pc.Yaw, 0.f), Pc.At), P);
			if (!A) continue;
			A->Kind = Pc.Kind;
			A->SiteId = Pc.Id;
			A->Island = Pc.Island;
			A->Variant = Pc.Variant;
			A->Index = Pc.Index;
			A->Home = Pc.At;
			A->BuildSky();
			Scenery.Add(A);
			if (Pc.Kind == TEXT("island")) IslandActors.Add(Pc.Id, A);
			// Pieces standing on a drifting island ride its actor.
			else if (!Pc.Island.IsNone())
			{
				if (const TWeakObjectPtr<AWRSkyScenery>* Isl = IslandActors.Find(Pc.Island); Isl && Isl->IsValid())
				{
					A->AttachToActor(Isl->Get(), FAttachmentTransformRules::KeepWorldTransform);
				}
			}
		}
		if (PiecesBuilt < Pieces.Num()) return;
		bSceneryDone = true;
		Pieces.Empty();
		LightClock = 0.f;
		// Every machine: the climbable lines (the Last Kite's, the Winch ladder, the keel vault's chain ledge), the event's
		// cloud bridges.
		const FWRSkyIsleData& D = FWRSkyIsleData::Get();
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		for (const FWRSkyIsleData::FBridge& B : D.Bridges)
		{
			if (B.Kind != TEXT("line") && B.Kind != TEXT("ladder")) continue;
			AWRSkyLine* L = World->SpawnActor<AWRSkyLine>(AWRSkyLine::StaticClass(), FTransform(B.A), SP);
			if (!L) continue;
			L->A = B.A + FVector(0, 0, 40.f);
			L->B = B.B + FVector(0, 0, 40.f);
			L->Sag = B.Kind == TEXT("line") ? FVector::Dist(B.A, B.B) * 0.04f : 20.f;
			L->BuildLook();
			if (B.Id == TEXT("last_kite_line")) KiteLine = L;
		}
		EventLook = World->SpawnActor<AWRSkyEventLook>(AWRSkyEventLook::StaticClass(), FTransform::Identity, SP);
		UE_LOG(LogWildreach, Log, TEXT("SKY scenery built: %d places"), Scenery.Num());
	}
	if (!bServerDone)
	{
		bServerDone = true;
		if (World->GetNetMode() != NM_Client) SpawnServerActors();
	}
}

AWRResourceNode* UWRSkyWorld::SpawnNode(FName Item, const FVector& At, int32 Hits, float PerHit, float RespawnSeconds, float Yaw)
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

AWRWorldCache* UWRSkyWorld::SpawnCache(const FVector& At, float Yaw, const FString& Title, const TMap<FName, int32>& Items, const FString& Lore, float RefillHours)
{
	UWorld* World = GetWorld();
	const FTransform Tr(FRotator(0.f, Yaw, 0.f), At);
	AWRWorldCache* C = World->SpawnActorDeferred<AWRWorldCache>(AWRWorldCache::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!C) return nullptr;
	C->Kind = TEXT("chest");
	C->Biome = WRSkyIsles::Biome();
	C->Title = Title;
	C->Items = Items;
	C->Lore = Lore;
	C->RefillHours = RefillHours;
	C->FinishSpawning(Tr);
	Caches.Add(C);
	return C;
}

AWRSkySpot* UWRSkyWorld::SpawnSpot(FName Kind, FName Id, const FVector& At, const FString& Title, const FString& Verb, float Seconds)
{
	UWorld* World = GetWorld();
	const FTransform Tr(FRotator::ZeroRotator, At);
	AWRSkySpot* S = World->SpawnActorDeferred<AWRSkySpot>(AWRSkySpot::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
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

AWRSkyShard* UWRSkyWorld::SpawnShard(const FVector& At, FName Item, int32 Count, AActor* Striker)
{
	UWorld* World = GetWorld();
	const FTransform Tr(At);
	AWRSkyShard* S = World->SpawnActorDeferred<AWRSkyShard>(AWRSkyShard::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!S) return nullptr;
	S->BornAt = Now(this);
	S->Origin = At;
	S->Count = FMath::Max(1, Count);
	S->ItemId = Item;
	S->Striker = Striker;
	S->FinishSpawning(Tr);
	Shards.Add(S);
	return S;
}

void UWRSkyWorld::SpawnServerActors()
{
	UWorld* World = GetWorld();
	const FWRSkyIsleData& D = FWRSkyIsleData::Get();
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	State = World->SpawnActor<AWRSkyState>(AWRSkyState::StaticClass(), FTransform::Identity, SP);

	// ---- the Kitewright lifts (L6): each camp's cage up to the island over it ----
	for (const FWRSkyIsleData::FLift& L : D.Lifts)
	{
		const FWRSkyIsleData::FIsland* I = D.FindIsland(L.Island);
		if (!I) continue;
		const FVector Foot = L.Foot;
		const float Top = I->TopZ(FVector(Foot.X, Foot.Y, 0.f));
		const FVector Out = (FVector(Foot.X, Foot.Y, 0.f) - FVector(I->Centre.X, I->Centre.Y, 0.f)).GetSafeNormal2D();
		const FTransform Tr(FRotator::ZeroRotator, FVector(Foot.X, Foot.Y, Foot.Z + 10.f));
		AWRSkyLift* Lf = World->SpawnActorDeferred<AWRSkyLift>(AWRSkyLift::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Lf) continue;
		Lf->Rise = FMath::Max(1000.f, Top - Foot.Z - 10.f);
		Lf->FaceYaw = FMath::RadiansToDegrees(FMath::Atan2(Out.Y, Out.X)) + 180.f;
		Lf->LiftId = L.Id;
		Lf->FinishSpawning(Tr);
		Lifts.Add(Lf);
	}
	// ---- the sinking stones (W2): one actor a line ----
	{
		TSet<int32> LinesSeen;
		for (const FWRSkyIsleData::FStone& S : D.Stones) LinesSeen.Add(S.Line);
		for (const int32 Ln : LinesSeen)
		{
			AWRSkyStones* St = World->SpawnActorDeferred<AWRSkyStones>(AWRSkyStones::StaticClass(), FTransform::Identity, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (!St) continue;
			St->Line = Ln;
			St->FinishSpawning(FTransform::Identity);
			StoneLines.Add(St);
		}
	}

	// ---- the gathering (SKY 2.6, build 2.8; numbers in tuning skyWorld.nodes) ----
	const auto Node = [&](const TCHAR* Kind, FName Item, const FVector& At, float Yaw = -1.f)
	{
		const FString K = FString::Printf(TEXT("skyWorld.nodes.%s."), Kind);
		return SpawnNode(Item, At, int32(Tn(*(K + TEXT("hits")), 4.f)), Tn(*(K + TEXT("perHit")), 1.f), Tn(*(K + TEXT("respawn")), 900.f), Yaw);
	};
	const auto List = [&](const TCHAR* Key) -> const TArray<FWRSkyIsleData::FNode>& { static const TArray<FWRSkyIsleData::FNode> None; const TArray<FWRSkyIsleData::FNode>* L = D.Resources.Find(FName(Key)); return L ? *L : None; };
	const auto Drifts = [&](FName Island) { return D.Drift.ContainsByPredicate([Island](const FWRSkyIsleData::FDrift& Dr) { return Dr.Island == Island; }); };
	// SkLevistone (the signature, never farmable): struck loose it floats up 1 m and hangs 3 s; grab it or it's gone.
	const auto Levi = [this](AWRResourceNode* N, int32 PerShard)
	{
		if (!N) return;
		TWeakObjectPtr<AWRResourceNode> WeakN = N;
		N->Divert = [this, WeakN, PerShard](AWRCharacterBase* Who, bool bHeavy)
		{
			AWRResourceNode* Nd = WeakN.Get();
			if (!Nd || !Who) return;
			const float Scale = WRWorld::GatherScale(Who, TEXT("levistone"), Who->GetActorLocation()) * WRTune(TEXT("server.gatherMultiplier"), 1.f);
			const int32 Count = FMath::Max(1, FMath::RoundToInt(PerShard * (bHeavy ? 2.f : 1.f) * Scale));
			const FVector Toward = (Who->GetActorLocation() - Nd->GetActorLocation()).GetSafeNormal2D();
			SpawnShard(Nd->GetActorLocation() + Toward * 60.f + FVector(0, 0, 60.f), TEXT("levistone"), Count, Who);
			UWRLifeSkillsComponent::Award(Who, TEXT("mining"), TEXT("mine"), 4.f);
			FWRGathering::WearHeld(Who, true);
		};
	};
	for (const FWRSkyIsleData::FNode& N : List(TEXT("levistone_shard"))) Levi(Node(TEXT("shard"), TEXT("levistone"), N.At), 1);
	for (const FWRSkyIsleData::FNode& N : List(TEXT("keel")))
	{
		// Keels hang under the islands: the node sits on the root's flank under the rim, over the kit's hanging ledge.
		const FWRSkyIsleData::FIsland* I = D.FindIsland(N.Island);
		if (!I || Drifts(N.Island)) continue;
		const FVector At(N.At.X, N.At.Y, I->Centre.Z - FMath::Clamp(I->R() * 0.12f, 800.f, 2500.f) - 1450.f);
		Levi(Node(TEXT("keel"), TEXT("levistone"), At), 2);
		// Levistone moss (Aaron 11-16, r-moss: "Yes add the moss to collect. Make it look good."): beside each keel a
		// lump of levistone the moss has overgrown floats on its own lift, out from the root over open air; hold E to
		// peel a mat off it (any hand), a game day before it's ripe again for you (skyWorld.moss).
		const FVector Out = (FVector(N.At.X, N.At.Y, 0.f) - FVector(I->Centre.X, I->Centre.Y, 0.f)).GetSafeNormal2D();
		const FVector MossAt = At + Out * Tn(TEXT("skyWorld.moss.outCm"), 330.f) + FVector(0.f, 0.f, Tn(TEXT("skyWorld.moss.upCm"), 60.f));
		if (AWRSkySpot* M = SpawnSpot(TEXT("moss"), TEXT("levistone_moss"), MossAt, TEXT("Levistone moss"), TEXT("Peel the moss"), Tn(TEXT("skyWorld.moss.seconds"), 2.f)))
		{
			M->Gives.Add(TEXT("levistone_moss"), FMath::Max(1, FMath::RoundToInt(Tn(TEXT("skyWorld.moss.gives"), 4.f))));
			M->RespawnHours = Tn(TEXT("skyWorld.moss.respawnHours"), 24.f);
			M->Reach = Tn(TEXT("skyWorld.moss.reachCm"), 380.f);
		}
	}
	for (const FWRSkyIsleData::FNode& N : List(TEXT("core")))
	{
		// Cores in the Weeping Isle's caves: the chamber's back wall (by cave, slot).
		const FWRSkyIsleData::FSite* Cave = nullptr;
		for (const FWRSkyIsleData::FSite& S : D.Sites) if (S.Kind == TEXT("weeping_cave") && S.Cave == N.Cave) Cave = &S;
		const FWRSkyIsleData::FIsland* I = D.FindIsland(TEXT("weeping_isle"));
		if (!Cave || !I) continue;
		const FVector2D L = I->Local(Cave->At);
		const float A = FMath::Atan2(L.Y, L.X);
		const FVector2D Out(FMath::Cos(A), FMath::Sin(A));
		const float Edge = I->EdgeRadius(A);
		const FVector2D In = Out * (Edge - 2500.f - 900.f - 700.f) + FVector2D(-Out.Y, Out.X) * (N.Slot - 1.5f) * 350.f;
		const FVector2D W = I->World2D(In);
		Node(TEXT("core"), TEXT("levistone_core"), FVector(W, I->Centre.Z - 1300.f));
	}
	for (const TCHAR* Ore : { TEXT("iron"), TEXT("tin") })
	{
		for (const FWRSkyIsleData::FNode& N : List(Ore))
		{
			const FWRSkyIsleData::FIsland* I = D.FindIsland(TEXT("weeping_isle"));
			const FWRSkyIsleData::FSite* Cave = nullptr;
			for (const FWRSkyIsleData::FSite& S : D.Sites) if (S.Kind == TEXT("weeping_cave") && S.Cave == N.Cave) Cave = &S;
			if (!Cave || !I) continue;
			const FVector2D L = I->Local(Cave->At);
			const float A = FMath::Atan2(L.Y, L.X);
			const FVector2D Out(FMath::Cos(A), FMath::Sin(A));
			const FVector2D In = Out * (I->EdgeRadius(A) - 2500.f - 300.f) + FVector2D(-Out.Y, Out.X) * (N.Slot == 2 ? 600.f : -600.f);
			Node(Ore, FName(*FString::Printf(TEXT("%s_ore"), Ore)), FVector(I->World2D(In), I->Centre.Z - 1300.f));
		}
	}
	for (const FWRSkyIsleData::FNode& N : List(TEXT("copper"))) Node(TEXT("copper"), TEXT("copper_ore"), N.At);
	for (const FWRSkyIsleData::FNode& N : List(TEXT("cloudpine"))) if (!Drifts(N.Island)) Node(TEXT("cloudpine"), TEXT("cloudpine"), N.At);
	for (const TCHAR* Crop : { TEXT("windflax"), TEXT("cloudberry") })
	{
		for (const FWRSkyIsleData::FNode& N : List(Crop))
		{
			AWRResourceNode* Nd = Node(Crop, FName(Crop), N.At);
			if (!Nd) continue;
			const FName Seed(*FString::Printf(TEXT("%s_seeds"), Crop));
			const float Chance = Tn(*FString::Printf(TEXT("skyWorld.nodes.%s.seedChance"), Crop), 0.35f);
			Nd->OnHarvested = [Seed, Chance](AWRCharacterBase* Who)
			{
				if (FMath::FRand() < Chance) if (UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Who)) Inv->AddItem(Seed, 1);
			};
		}
	}
	for (const FWRSkyIsleData::FNode& N : List(TEXT("feather"))) Node(TEXT("feather"), TEXT("puffwing_feathers"), N.At);
	// Gossamer drifts: hold E 4 s to wind silk onto a spindle (any tool), back a game day per survivor.
	for (const FWRSkyIsleData::FNode& N : List(TEXT("gossamer")))
	{
		AWRSkySpot* S = SpawnSpot(TEXT("gossamer"), TEXT("gossamer"), N.At + FVector(0, 0, 120.f), TEXT("Gossamer drift"), TEXT("Wind the silk"), Tn(TEXT("skyWorld.gossamer.seconds"), 4.f));
		if (!S) continue;
		S->Gives.Add(TEXT("skysilk"), 3);
		S->RespawnHours = Tn(TEXT("skyWorld.gossamer.respawnHours"), 24.f);
		S->Reach = 380.f;
	}

	// ---- caches, keepsakes and the places you use (SKY 2.4, 2.7, 2.8, 2.14) ----
	const auto At = [&](FName Site, float Fwd = 0.f, float Side = 0.f, float Up = 60.f) -> FVector
	{
		const FWRSkyIsleData::FSite* S = D.FindSite(Site);
		if (!S) return FVector::ZeroVector;
		const FVector Off = FRotator(0.f, S->Yaw, 0.f).RotateVector(FVector(Fwd, Side, 0.f));
		FVector P = S->At + Off;
		if (const FWRSkyIsleData::FIsland* I = D.FindIsland(S->Island)) P.Z = I->TopZ(P);
		else { float Z = 0.f; if (WRLand::GroundUnder(P + FVector(0, 0, 20000.f), Z)) P.Z = Z; }
		return P + FVector(0, 0, Up);
	};
	const float Refill = Tn(TEXT("skyWorld.cacheRefillHours"), 72.f);
	{
		TMap<FName, int32> H2; H2.Add(TEXT("iron_ingot"), 8); H2.Add(TEXT("chain"), 2); H2.Add(TEXT("lore_kite_of_the_given"), 1);
		SpawnCache(At(TEXT("H2"), 0.f, 0.f, 40.f), 0.f, TEXT("The fallen cage"), H2, TEXT("Two of them, holding each other. A bone tube in one hand: a yellow kite rolled inside."), 0.f);
		TMap<FName, int32> H3; H3.Add(TEXT("glider_spar"), 1); H3.Add(TEXT("cooked_meat"), 3); H3.Add(TEXT("waterskin"), 1);
		SpawnCache(At(TEXT("H3"), 60.f, -40.f, 1500.f), 0.f, TEXT("A newcomer's pack"), H3, TEXT("Snagged on the highest thorn. Whoever wore it never came down for it."), 0.f);
		TMap<FName, int32> H7; H7.Add(TEXT("skysilk"), 4); H7.Add(TEXT("scrap_skysilk_weave"), 1);
		SpawnCache(At(TEXT("H7"), 120.f, 0.f, 40.f), 0.f, TEXT("The loom"), H7, TEXT("A teal scarf half-woven on the loom, the shuttle still in the shed."), 0.f);
		TMap<FName, int32> H11; H11.Add(TEXT("galeharrier_feather"), 6);
		if (FMath::FRand() < Tn(TEXT("skyWorld.eyrieEggChance"), 0.25f)) H11.Add(TEXT("egg_windmane"), 1);
		SpawnCache(At(TEXT("H11"), 0.f, 0.f, 60.f), 0.f, TEXT("The eyrie"), H11, TEXT("Planks, chain, kite cloth. The skulls round the rim are all Windmane."), Refill);
		TMap<FName, int32> H13; H13.Add(TEXT("bronze_ingot"), 10); H13.Add(TEXT("lore_wheelwrights_plan"), 1);
		SpawnCache(At(TEXT("H13"), 600.f, 0.f, 40.f), 0.f, TEXT("The wheelwright's chest"), H13, TEXT("A plan of the wheel: twelve vanes, the hub over the Flue, arrows drawn blowing down."), 0.f);
		TMap<FName, int32> H14; H14.Add(TEXT("deepcap_spores"), 4); H14.Add(TEXT("lore_gardeners_mask"), 1);
		SpawnCache(At(TEXT("H14"), 0.f, 300.f, 30.f), 0.f, TEXT("A gardener's satchel"), H14, TEXT("Cloth masks, tied tight. It didn't help."), Refill);
		TMap<FName, int32> U2; U2.Add(TEXT("skysilk"), 6); U2.Add(TEXT("lore_bone_tube"), 4);
		SpawnCache(At(TEXT("U2"), 0.f, 0.f, 60.f), 0.f, TEXT("The bone tubes"), U2, TEXT("Each tube sealed with wax, each a kite rolled inside in somebody's colours."), 0.f);
		TMap<FName, int32> U5; U5.Add(TEXT("lore_unfinished_kite"), 1); U5.Add(TEXT("cloudberry_heirloom_seeds"), 1);
		SpawnCache(At(TEXT("U5"), 0.f, 320.f, 200.f), 0.f, TEXT("The sixth frame"), U5, TEXT("Five kites on the wall. The sixth has its spars and no cloth."), 0.f);
		TMap<FName, int32> U7; U7.Add(TEXT("crude_glider"), 1); U7.Add(TEXT("cooked_meat"), 4); U7.Add(TEXT("waterskin"), 1);
		SpawnCache(At(TEXT("U7"), 0.f, 0.f, 30.f), 0.f, TEXT("A newcomer's pack"), U7, TEXT("Three bedrolls. A shirt sewn into a kite. They tried."), 0.f);
		TMap<FName, int32> U8; U8.Add(TEXT("levistone_core"), 6); U8.Add(TEXT("kitewright_glaive"), 1);
		SpawnCache(At(TEXT("U8"), -200.f, 0.f, -2150.f), 0.f, TEXT("The Keel Vault"), U8, TEXT("Cut into the keel itself. Somebody wanted this kept where nobody would stand."), 0.f);
		TMap<FName, int32> H1; H1.Add(TEXT("lore_kitewright_bowl"), 1);
		SpawnCache(At(TEXT("H1"), 900.f, -500.f, 380.f), 0.f, TEXT("Nailed to the floor"), H1, TEXT("The table's on the ceiling. The bowls never fell."), 0.f);
	}
	// Round houses: linen, rope, bronze; a lore item 15% of the time (SKY 2.4 kit).
	for (const FWRSkyIsleData::FSite& S : D.Kit)
	{
		if (S.Kind != TEXT("round_house")) continue;
		TMap<FName, int32> Items;
		Items.Add(TEXT("linen"), FMath::RandRange(2, 4));
		Items.Add(TEXT("rope"), FMath::RandRange(1, 3));
		Items.Add(TEXT("bronze_ingot"), FMath::RandRange(1, 2));
		if (FMath::FRand() < Tn(TEXT("skyWorld.roundHouseLoreChance"), 0.15f)) Items.Add(TEXT("lore_bone_tube"), 1);
		SpawnCache(S.At + FVector(0, 0, 60.f), S.Yaw, TEXT("A Kitewright house's chest"), Items, TEXT("Kite cloth folded small, a spindle, a bowl."), Refill);
	}
	// The pearl lagoon's sunken skiff (12 m down in Gulper's Bight) and the Wind Rose's plate cache.
	if (const FWRSkyIsleData::FIsland* Lg = D.FindIsland(TEXT("rain_lagoon")))
	{
		TMap<FName, int32> Pearl;
		for (const FWRSkyIsleData::FSecret& Se : D.Secrets) if (Se.Id == TEXT("pearl_lagoon")) Pearl = Se.Gives;
		SpawnCache(FVector(Lg->World2D(FVector2D(Lg->RMin() * 0.3f, 0.f)), Lg->WaterZ() - 1150.f), 30.f, TEXT("A sunken skiff's bronze chest"), Pearl, TEXT("They had boats on the lagoons. This one went down with its catch."), 0.f);
	}
	{
		TMap<FName, int32> Rose;
		for (const FWRSkyIsleData::FSecret& Se : D.Secrets) if (Se.Id == TEXT("wind_rose")) Rose = Se.Gives;
		AWRSkySpot* S = SpawnSpot(TEXT("rose"), TEXT("wind_rose"), At(TEXT("S_still_pool"), 300.f, 0.f, 10.f), TEXT("The Wind Rose"), TEXT("Lean on the plate"), 2.f);
		if (S) { S->Gives = Rose; S->Reach = 520.f; }
	}
	// Vistas: the Kite Field (1,000 XP and three bone tubes), the Great Kite's anchor drum (reveals W4), the First Kite
	// (1,500 XP and the Serpent anchor rubbing, Yara's sixth), the two vista stones (reveal 400 m).
	if (AWRSkySpot* S = SpawnSpot(TEXT("vista"), TEXT("kite_field"), At(TEXT("H8"), D.FindIsland(TEXT("kitefield")) ? D.FindIsland(TEXT("kitefield"))->R() * 0.2f : 0.f, 0.f, 60.f), TEXT("The Kite Field"), TEXT("Look out over the kites"), 3.f))
	{
		S->Xp = int32(Tn(TEXT("skyWorld.xp.kiteField"), 1000.f));
		S->Gives.Add(TEXT("lore_bone_tube"), 3);
		S->RevealRadius = 40000.f;
		S->Quest = TEXT("vista_kite_field");
	}
	if (AWRSkySpot* S = SpawnSpot(TEXT("drum"), TEXT("anchor_drum"), At(TEXT("L5"), 0.f, 0.f, 220.f), TEXT("The Great Kite's anchor drum"), TEXT("Follow the line up"), 3.f))
	{
		S->RevealRadius = 90000.f;
		S->Reach = 420.f;
	}
	if (AWRSkySpot* S = SpawnSpot(TEXT("vista"), TEXT("first_kite"), At(TEXT("U6"), 0.f, 0.f, 120.f), TEXT("The First Kite"), TEXT("Take a rubbing of the serpent"), 4.f))
	{
		S->Xp = int32(Tn(TEXT("skyWorld.xp.firstKite"), 1500.f));
		S->Gives.Add(TEXT("serpent_anchor_rubbing"), 1);
		S->Quest = TEXT("first_kite");
	}
	// The Windmanes' ledge nests (SKY 2.9: steal an egg; the herd defends it together): an egg on each, back a game day.
	for (const FWRSkyIsleData::FSite& S : D.Kit)
	{
		if (S.Kind != TEXT("mane_ledge")) continue;
		if (AWRSkySpot* N = SpawnSpot(TEXT("nest"), S.Id, S.At + FVector(0, 0, 50.f), TEXT("A Windmane nest"), TEXT("Take the egg"), Tn(TEXT("skyWorld.nestSeconds"), 2.f)))
		{
			N->Gives.Add(TEXT("egg_windmane"), 1);
			N->RespawnHours = Tn(TEXT("skyWorld.nestRespawnHours"), 24.f);
			N->Reach = 350.f;
		}
	}
	for (const FWRSkyIsleData::FSite& S : D.Kit)
	{
		if (S.Kind != TEXT("vista")) continue;
		if (AWRSkySpot* V = SpawnSpot(TEXT("vista"), S.Id, S.At + FVector(0, 0, 80.f), TEXT("A lung ta stone"), TEXT("Look out"), 2.f)) V->RevealRadius = 40000.f;
	}
	// The Bonebell, the Toll House's bowls, the Lift Station's blueprint, keepsakes.
	if (AWRSkySpot* S = SpawnSpot(TEXT("bonebell"), TEXT("bonebell"), At(TEXT("U3"), -300.f, 0.f, 200.f), TEXT("The Bonebell"), TEXT("Strike the bell"), 0.f)) S->Reach = 420.f;
	if (AWRSkySpot* S = SpawnSpot(TEXT("bowls"), TEXT("toll_bowls"), At(TEXT("H10"), 380.f, 530.f, 70.f), TEXT("The toll bowls"), TEXT("Take the copper"), 1.5f))
	{
		S->Gives.Add(TEXT("copper_ingot"), int32(Tn(TEXT("skyWorld.tollCopper"), 12.f)));
		S->RespawnHours = Refill;
	}
	if (AWRSkySpot* S = SpawnSpot(TEXT("station"), TEXT("lift_station"), At(TEXT("H5"), -580.f, 140.f, 90.f), TEXT("The winch house"), TEXT("Take the plan from the bench"), 2.f)) S->Gives.Add(TEXT("bp_updraft_lift"), 1);
	for (const FWRSkyIsleData::FLore& L : D.Lore)
	{
		if (L.Site == TEXT("H2") || L.Site == TEXT("H13") || L.Site == TEXT("H14") || L.Site == TEXT("U5") || L.Site == TEXT("U2") || L.Site == TEXT("H8") || L.Site == TEXT("H1") || L.Site == TEXT("U6")) continue;   // in their caches / vistas
		AWRSkySpot* S = SpawnSpot(TEXT("keepsake"), L.Item, At(L.Site, 120.f, 120.f, 30.f), ItemName(FName(*(TEXT("lore_") + L.Item.ToString()))), TEXT("Take it"), 1.f);
		if (S) S->Gives.Add(FName(*(TEXT("lore_") + L.Item.ToString())), 1);
	}
	// The Glider Graves' message stone (H12): "it's further than it looks".
	{
		const FVector P = At(TEXT("H12"), 0.f, 220.f, 0.f);
		AWRMessageStone* M = World->SpawnActorDeferred<AWRMessageStone>(AWRMessageStone::StaticClass(), FTransform(P), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (M)
		{
			M->Text = TEXT("it's further than it looks");
			M->Author = TEXT("a Kitewright hand");
			M->Biome = WRSkyIsles::Biome();
			M->FinishSpawning(FTransform(P));
		}
	}
	UE_LOG(LogWildreach, Log, TEXT("SKY server things: %d lifts, %d stone lines, %d caches, %d spots"), Lifts.Num(), StoneLines.Num(), Caches.Num(), Spots.Num());
}

AWRSkyScenery* UWRSkyWorld::IslandActor(FName Island) const
{
	const TWeakObjectPtr<AWRSkyScenery>* A = IslandActors.Find(Island);
	return A ? A->Get() : nullptr;
}

void UWRSkyWorld::SetTollLit()
{
	if (AWRSkyState* S = State ? State.Get() : AWRSkyState::Get(this)) S->bTollLit = true;
}

void UWRSkyWorld::StrikeBonebell(AWRCharacterBase* Who)
{
	AWRSkyState* S = State ? State.Get() : AWRSkyState::Get(this);
	if (!S || !Who) return;
	const float T = Now(this);
	if (T - S->BonebellAt < Tn(TEXT("skyWorld.bonebell.cooldown"), 20.f)) { Tell(Who, TEXT("The bell is still ringing")); return; }
	S->BonebellAt = T;
	S->ForceNetUpdate();
	// Heard through the fog for 300 m; every kite in the bank shakes (the clients' look); the roost wakes (WRSkyCreatures
	// listens on OnBonebell); in the Turning Wind the kites rise out of the cloud over the Kitefield (the echo: once each).
	WRSound::Broadcast(Who, TEXT("sky.bonebell"));
	OnBonebell.Broadcast(Who);
	if (S->TurningNow(T))
	{
		const FString P = PlayerOf(Who);
		if (!BonebellEcho.Contains(P))
		{
			BonebellEcho.Add(P);
			if (UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Who)) Inv->AddItem(TEXT("talisman_last_kite"), 1);
			Tell(Who, TEXT("The kites rise out of the cloud, a column of them over the Kitefield. One comes down to you: the Kitewright's last kite."));
			WRFx::Broadcast(this, TEXT("sky.kiteColumn"), FVector(FWRSkyIsleData::Get().UnderGap, FWRSkyIsleData::Get().UnderTop), FRotator::ZeroRotator, 4.f);
		}
	}
}

AWRSkySpot* UWRSkyWorld::SpotNear(const FVector& At) const
{
	AWRSkySpot* Best = nullptr;
	float BestD = TNumericLimits<float>::Max();
	for (TActorIterator<AWRSkySpot> It(GetWorld()); It; ++It)
	{
		const float D = FVector::Dist(It->GetActorLocation(), At);
		if (D < It->Reach && D < BestD) { BestD = D; Best = *It; }
	}
	return Best;
}

AWRSkyLift* UWRSkyWorld::LiftNear(const FVector& At) const
{
	for (TActorIterator<AWRSkyLift> It(GetWorld()); It; ++It) if (It->Near(At)) return *It;
	return nullptr;
}

AWRSkyLine* UWRSkyWorld::LineNear(const FVector& At, bool bHeld, const AActor* Who) const
{
	for (TActorIterator<AWRSkyLine> It(GetWorld()); It; ++It)
	{
		if (bHeld) { if (It->Holds(Who)) return *It; continue; }
		float Dist = 0.f;
		It->Nearest(At, Dist);
		if (Dist < Tn(TEXT("skyWorld.line.reach"), 260.f)) return *It;
	}
	return nullptr;
}

AWRSkyShard* UWRSkyWorld::ShardNear(const FVector& At, float Reach) const
{
	for (TActorIterator<AWRSkyShard> It(GetWorld()); It; ++It)
	{
		if (FVector::Dist(It->WhereAt(Now(this)), At) < Reach) return *It;
	}
	return nullptr;
}

void UWRSkyWorld::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	StepBuild();
	if (!bSceneryDone) return;
	StepDrift();
	StepLights(DeltaTime);
	if (GetWorld()->GetNetMode() != NM_Client) StepServer(DeltaTime);
}

void UWRSkyWorld::StepDrift()
{
	// The Turning Wind's small islands (SKY 2.15): every machine moves them along their splines from the state's times
	// (1 m/s, then hold; home again after), and everything standing on them rides along (based movement).
	const AWRSkyState* S = AWRSkyState::Get(this);
	const FWRSkyIsleData& D = FWRSkyIsleData::Get();
	const float T = Now(this);
	for (const FWRSkyIsleData::FDrift& Dr : D.Drift)
	{
		const FWRSkyIsleData::FIsland* I = D.FindIsland(Dr.Island);
		if (!I) continue;
		const FVector Full = Dr.To - I->Centre;
		const float K = S ? S->DriftAmount(T, Full.Size2D()) : 0.f;
		const FVector Off = Full * K;
		WRSkyIsles::SetDrift(I->Index, Off);
		if (AWRSkyScenery* A = IslandActor(Dr.Island))
		{
			const FVector Want = I->Centre + Off;
			if (!A->GetActorLocation().Equals(Want, 0.5f)) A->SetActorLocation(Want);
		}
	}
}

void UWRSkyWorld::StepLights(float Dt)
{
	// Night blocks (keel glow, braziers, lamps) show from dusk; in the Undercloud the glow is always on (it's dark in
	// the cloud). The kites in the bank shake for 20 s after the Bonebell (every machine).
	LightClock -= Dt;
	if (LightClock > 0.f) return;
	LightClock = 0.5f;
	UWorld* World = GetWorld();
	const bool bNight = AWRGameState::IsNightIn(World);
	for (const TWeakObjectPtr<AWRSkyScenery>& W : Scenery)
	{
		AWRSkyScenery* S = W.Get();
		if (!S) continue;
		const FWRSkyIsleData::FIsland* I = FWRSkyIsleData::Get().FindIsland(S->Island.IsNone() ? S->SiteId : S->Island);
		const bool bLit = bNight || (I && I->Zone == '5');
		for (int32 i = 0; i < S->NightLights.Num(); ++i)
		{
			if (UPointLightComponent* L = S->NightLights[i]) L->SetIntensity(bLit ? (S->NightPower.IsValidIndex(i) ? S->NightPower[i] : 1000.f) : 0.f);
		}
		if (S->NightRoot && S->NightRoot->IsVisible() != bLit) S->NightRoot->SetVisibility(bLit, true);
	}
}

void UWRSkyWorld::StepServer(float Dt)
{
	ServerClock += Dt;
	if (ServerClock < 2.f) return;
	ServerClock = 0.f;
	// Caches fill again on their hours; the Wind Rose's plate turns back after an hour.
	const double Hours = GameHours(this);
	for (const TWeakObjectPtr<AWRWorldCache>& W : Caches)
	{
		AWRWorldCache* C = W.Get();
		if (C && C->bTaken && C->RefillHours > 0.f && C->RefillAtHours > 0.0 && Hours >= C->RefillAtHours) C->Refill();
	}
	// The Turning Wind's shard falls (SKY 2.15): drifting islands brush and shed levistone: 20 falls over the hour.
	if (AWRSkyState* S = State ? State.Get() : nullptr)
	{
		const float T = Now(this);
		if (S->TurningNow(T))
		{
			const FWRSkyIsleData& D = FWRSkyIsleData::Get();
			const float Span = FMath::Max(60.f, S->TurnEndAt - S->TurnStartAt);
			const int32 Due = FMath::Min(D.ShardFalls.Num(), int32(D.ShardFalls.Num() * (T - S->TurnStartAt) / Span) + 1);
			while (ShardFallsDone < Due)
			{
				const FWRSkyIsleData::FNode& N = D.ShardFalls[ShardFallsDone++];
				const int32 Count = FMath::RandRange(int32(Tn(TEXT("skyWorld.turningWind.shardMin"), 6.f)), int32(Tn(TEXT("skyWorld.turningWind.shardMax"), 10.f)));
				for (int32 k = 0; k < 4; ++k) SpawnShard(N.At + FVector(FMath::FRandRange(-250.f, 250.f), FMath::FRandRange(-250.f, 250.f), 60.f), TEXT("levistone"), FMath::Max(1, Count / 4), nullptr);
				WRSound::PlayAt(this, TEXT("sky.shard_fall"), N.At);
			}
		}
		else ShardFallsDone = 0;
	}
}

// ================================================================================================ interact

bool UWRSkyWorld::Prompt(const AWRPlayerController* PC, FString& OutTarget, FString& OutVerb)
{
	const UWRSkyWorld* W = Get(PC);
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	if (!W || !W->bActive || !Me) return false;
	const FVector P = Me->GetActorLocation();
	if (W->ShardNear(P, 260.f)) { OutTarget = TEXT("Levistone"); OutVerb = TEXT("Grab it before it floats away"); return true; }
	if (const AWRSkyLift* L = W->LiftNear(P))
	{
		OutTarget = TEXT("Kitewright lift");
		const float Pos = L->CurrentPosition();
		OutVerb = L->bMoving ? TEXT("Throw the winch over") : Pos < 0.5f ? TEXT("Crank it up") : TEXT("Let it down");
		return true;
	}
	if (W->LineNear(P))
	{
		OutTarget = TEXT("Line");
		OutVerb = TEXT("Take hold");
		return true;
	}
	if (const AWRSkySpot* S = W->SpotNear(P))
	{
		OutTarget = S->Title;
		OutVerb = S->Verb;
		return true;
	}
	return false;
}

bool UWRSkyWorld::ClientInteract(AWRPlayerController* PC)
{
	const UWRSkyWorld* W = Get(PC);
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	if (!W || !W->bActive || !Me) return false;
	const FVector P = Me->GetActorLocation();
	if (W->ShardNear(P, 260.f)) { PC->SendCommand(TEXT("world sky shard")); return true; }
	if (W->LiftNear(P)) { PC->SendCommand(TEXT("world sky lift")); return true; }
	if (W->LineNear(P)) { PC->SendCommand(TEXT("world sky line")); return true; }
	if (W->SpotNear(P)) { PC->SendCommand(TEXT("world sky spot")); return true; }
	return false;
}

FString UWRSkyWorld::Command(AWRPlayerController* PC, const TArray<FString>& Args)
{
	UWRSkyWorld* W = Get(PC);
	AWRCharacterBase* Me = PC ? Cast<AWRCharacterBase>(PC->GetPawn()) : nullptr;
	if (!W || !W->bActive || !Me) return TEXT("Not here");
	const FString Verb = Args.IsValidIndex(2) ? Args[2] : FString();
	const FVector P = Me->GetActorLocation();
	if (Verb == TEXT("shard"))
	{
		if (AWRSkyShard* S = W->ShardNear(P, 300.f)) S->TryTake(Me);
		return FString();
	}
	if (Verb == TEXT("lift"))
	{
		if (AWRSkyLift* L = W->LiftNear(P)) return L->Use(Me);
		return TEXT("No lift here");
	}
	if (Verb == TEXT("line"))
	{
		if (AWRSkyLine* L = W->LineNear(P, true, Me)) { L->Release(Me, false); return TEXT("You let go"); }
		if (AWRSkyLine* L = W->LineNear(P)) return L->Grab(Me);
		return TEXT("No line here");
	}
	if (Verb == TEXT("spot"))
	{
		if (AWRSkySpot* S = W->SpotNear(P)) return S->Start(PC);
		return TEXT("Nothing here");
	}
	return TEXT("world sky shard | lift | line | spot");
}
