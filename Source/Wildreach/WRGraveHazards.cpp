// The Graveyard's micro-hazards (see WRGraveHazards.h).

#include "WRGraveHazards.h"
#include "Wildreach.h"
#include "WRBuildLook.h"
#include "WRBuildLookKit.h"
#include "WRCharacter.h"
#include "WRCombatLibrary.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRGameState.h"
#include "WRGraveCreatures.h"
#include "WRGraveBosses.h"
#include "WRToolMesh.h"
#include "ProceduralMeshComponent.h"
#include "TimerManager.h"
#include "WRGraveTypes.h"
#include "WRInventory.h"
#include "WRLand.h"
#include "WRPlayerController.h"
#include "WRSound.h"
#include "WRSurvival.h"
#include "WRTuning.h"
#include "Components/BoxComponent.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

namespace WRGraveHzLocal
{
	float HzT(const TCHAR* Key, float Fallback) { return FWRTuning::Get().Num(FString::Printf(TEXT("hazards.graveyard.%s"), Key), Fallback); }
	double HzNow(const UObject* W) { return W && W->GetWorld() ? W->GetWorld()->GetTimeSeconds() : 0.0; }
	/** Drizzle on the Graveyard: the lime boils (x2). */
	bool HzRaining(const UObject* W)
	{
		const FWRWeatherKind* K = UWRWeatherSubsystem::ActiveKind(W, WRGrave::Biome());
		return K && K->Id == TEXT("drizzle");
	}
}
using namespace WRGraveHzLocal;

// ================================================================================================ the lid

AWRGraveLid::AWRGraveLid()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
	SetNetUpdateFrequency(10.f);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Plate = CreateDefaultSubobject<UBoxComponent>(TEXT("Plate"));
	Plate->SetupAttachment(RootComponent);
	Plate->SetBoxExtent(FVector(420.f, 420.f, 12.f));
	Plate->SetRelativeLocation(FVector(0.f, 0.f, -12.f));
	Plate->SetCollisionProfileName(TEXT("BlockAll"));
	Plate->SetCanEverAffectNavigation(false);
}

void AWRGraveLid::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRGraveLid, Stage);
}

void AWRGraveLid::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() != NM_DedicatedServer) BuildLook();
	ApplyStage();
}

void AWRGraveLid::BuildLook()
{
	if (bLook) return;
	bLook = true;
	// The turf over the pit: fresh-turned dark grave soil (the generator painted the ground round it the same), sods and
	// a few loose stones. Broken: the turf torn back round a ragged hole, roots and a coffin board showing in its side.
	Turf = NewObject<USceneComponent>(this);
	Turf->SetupAttachment(RootComponent);
	Turf->RegisterComponent();
	Broken = NewObject<USceneComponent>(this);
	Broken->SetupAttachment(RootComponent);
	Broken->RegisterComponent();
	TArray<FWRLookBlock> A, B;
	WRLookKit::FKit KA(GetActorTransform(), A), KB(GetActorTransform(), B);
	const WRLookKit::FSurf Soil = WRLookKit::FSurf::Cloth(FLinearColor(FColor(59, 53, 48)) * 0.5f);
	const WRLookKit::FSurf Sod = WRLookKit::FSurf::Cloth(FLinearColor(FColor(110, 122, 94)) * 0.42f);
	KA.Add(Soil, FVector(820.f, 820.f, 10.f), FVector(0.f, 0.f, -4.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
	for (int32 k = 0; k < 9; ++k)
	{
		const float Ang = k * 40.f;
		KA.Add(Sod, FVector(110.f, 70.f, 8.f), FRotator(0.f, Ang, 0.f).Vector() * (180.f + 120.f * (k % 3)) + FVector(0.f, 0.f, 1.f), FRotator(2.f * (k % 3), Ang + 20.f, 0.f));
	}
	for (int32 k = 0; k < 12; ++k)
	{
		const float Ang = k * 30.f;
		const FVector Dir = FRotator(0.f, Ang, 0.f).Vector();
		KB.Add(Soil, FVector(140.f, 60.f, 22.f), Dir * 380.f + FVector(0.f, 0.f, -2.f), FRotator(-25.f, Ang + 90.f, 0.f));
		KB.Add(Sod, FVector(100.f, 50.f, 8.f), Dir * 430.f + FVector(0.f, 0.f, 8.f), FRotator(-35.f, Ang + 90.f, 0.f));
	}
	KB.Add(WRLookKit::FSurf::Wood(FLinearColor(FColor(110, 102, 92)) * 0.45f), FVector(160.f, 20.f, 4.f), FVector(-200.f, 120.f, -90.f), FRotator(-60.f, 30.f, 0.f));
	FWRBuildLook::Spawn(this, Turf, A, FLinearColor(0.25f, 0.2f, 0.15f), false);
	FWRBuildLook::Spawn(this, Broken, B, FLinearColor(0.25f, 0.2f, 0.15f), false);
}

void AWRGraveLid::OnRep_Stage()
{
	// Every machine: the crack's ring of dust and the crunch from below (the tell), then the fall.
	if (Stage == 1) { WRFx::SpawnAt(this, TEXT("grave.subsidence_crack"), GetActorLocation(), FRotator::ZeroRotator, 1.f); WRSound::PlayAt(this, TEXT("grave.subsidence_crunch"), GetActorLocation()); }
	if (Stage == 2) { WRFx::SpawnAt(this, TEXT("grave.chalk_fall"), GetActorLocation(), FRotator::ZeroRotator, 1.5f); WRSound::PlayAt(this, TEXT("grave.subsidence_fall"), GetActorLocation()); }
	ApplyStage();
}

void AWRGraveLid::ApplyStage()
{
	const bool bOpen = Stage == 2;
	Plate->SetCollisionEnabled(bOpen ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
	if (Turf) Turf->SetVisibility(!bOpen, true);
	if (Broken) Broken->SetVisibility(bOpen, true);
}

void AWRGraveLid::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority()) return;
	const double Now = HzNow(this);
	const FVector C = GetActorLocation();
	// Who stands on it (a survivor or a heavy tame; the Graveyard's own creatures know the ground).
	AWRCharacterBase* On = nullptr;
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		if (!It->IsAlive() || FVector::Dist2D(It->GetActorLocation(), C) > HzT(TEXT("lidRadius"), 330.f)) continue;
		const float Feet = It->GetActorLocation().Z - It->GetSimpleCollisionHalfHeight();
		if (Feet < C.Z - 80.f || Feet > C.Z + 120.f) continue;
		if (const AWRCreature* Cr = Cast<AWRCreature>(*It); Cr && UWRGraveBody::IsGraveKind(Cr->CreatureId)) continue;
		On = *It;
		break;
	}
	// A tamed Gloamcrow screams a second before its owner steps on one (GY 2.12.1 utility).
	if (Stage == 0 && Now >= NextScreamAt)
	{
		for (TActorIterator<AWRCreature> It(GetWorld()); It; ++It)
		{
			if (It->CreatureId != TEXT("gloamcrow") || !It->IsTamed() || !It->IsAlive()) continue;
			for (FConstPlayerControllerIterator P = GetWorld()->GetPlayerControllerIterator(); P; ++P)
			{
				const APawn* Pawn = P->Get() ? P->Get()->GetPawn() : nullptr;
				if (!Pawn || P->Get()->PlayerState == nullptr || P->Get()->PlayerState->GetPlayerName() != It->TamedBy) continue;
				const float D = float(FVector::Dist2D(Pawn->GetActorLocation(), C));
				if (D < HzT(TEXT("crowWarn"), 700.f) && D > 330.f && FVector::Dist(It->GetActorLocation(), Pawn->GetActorLocation()) < 4000.f)
				{
					WRSound::Broadcast(*It, TEXT("creature.gloamcrow.scream"));
					NextScreamAt = Now + 6.0;
				}
			}
		}
	}
	if (Stage == 0 && On)
	{
		Stage = 1;
		StageAt = Now;
		OnRep_Stage();
	}
	else if (Stage == 1 && Now - StageAt >= HzT(TEXT("subsidenceTell"), 1.0f))
	{
		Stage = 2;
		StageAt = Now;
		OnRep_Stage();
		// Half the pits hold a sleeping den: 2-3 wake in the dark under you.
		if (bDen)
		{
			const int32 N = FMath::RandRange(2, 3);
			for (int32 i = 0; i < N; ++i)
			{
				const FVector At = FVector(C.X, C.Y, FloorZ) + FRotator(0.f, 120.f * i, 0.f).Vector() * 180.f + FVector(0.f, 0.f, 90.f);
				if (AWRCreature* H = AWRCreature::SpawnCreature(GetWorld(), TEXT("gravehound"), At, FRotator(0.f, 120.f * i + 180.f, 0.f)))
				{
					TWeakObjectPtr<AWRCreature> WeakH(H);
					FTimerHandle Hd;
					GetWorld()->GetTimerManager().SetTimer(Hd, [WeakH]()
					{
						UWRGraveBody* B = UWRGraveBody::Get(WeakH.Get());
						if (!WeakH.IsValid() || !B) return;
						FWRGraveAI::Bury(WeakH.Get(), B);
						FWRGraveAI::Unbury(WeakH.Get(), B);
					}, 1.2f + 0.3f * i, false);
				}
			}
			bDen = false;   // the den is spent until the world reloads
		}
	}
	else if (Stage == 2 && Now - StageAt >= HzT(TEXT("lidRegrow"), 120.f))
	{
		// Grows back over once nobody's in the pit.
		bool bInside = false;
		for (TActorIterator<AWRCharacterBase> It(GetWorld()); It && !bInside; ++It) bInside = It->IsAlive() && FVector::Dist2D(It->GetActorLocation(), C) < 450.f && It->GetActorLocation().Z < C.Z + 50.f;
		if (!bInside) { Stage = 0; OnRep_Stage(); }
	}
}

// ================================================================================================ the subsystem

bool UWRGraveHazards::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

bool UWRGraveHazards::IsTickable() const
{
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() != NM_Client && WRGrave::Live() && FWRGraveData::Get().bLoaded && FString(World->GetMapName()).Contains(TEXT("Island"));
}

bool UWRGraveHazards::InLime(const FVector& At, float* OutSurface)
{
	for (const FWRGraveData::FLime& L : FWRGraveData::Get().Lime)
	{
		if (FVector::Dist2D(At, L.At) > L.Radius) continue;
		if (OutSurface) *OutSurface = L.At.Z;
		return true;
	}
	return false;
}

void UWRGraveHazards::SpawnLids()
{
	UWorld* World = GetWorld();
	int32 N = 0;
	for (const FWRGraveData::FPit& P : FWRGraveData::Get().Subsidence)
	{
		const FTransform T(P.At + FVector(0.f, 0.f, 4.f));
		AWRGraveLid* L = World->SpawnActorDeferred<AWRGraveLid>(AWRGraveLid::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!L) continue;
		L->FloorZ = P.Floor;
		L->bDen = P.bDen;
		L->FinishSpawning(T);
		++N;
	}
	UE_LOG(LogWildreach, Log, TEXT("GRAVEYARD hazards: %d subsidence lids"), N);
}

void UWRGraveHazards::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!bLids)
	{
		Clock += DeltaTime;
		if (Clock < 10.f) return;
		SpawnLids();
		bLids = true;
	}
	StepLime(DeltaTime);
	StepDraughts(DeltaTime);
}

void UWRGraveHazards::StepLime(float Dt)
{
	// Wading quicklime: 0.9 s of crackle and hiss first (the tell), then 8 a second (x2 in drizzle) and armour wear.
	UWorld* World = GetWorld();
	const double Now = HzNow(this);
	for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
	{
		AWRCharacterBase* C = *It;
		float Surface = 0.f;
		const bool bIn = C->IsAlive() && InLime(C->GetActorLocation(), &Surface) && C->GetActorLocation().Z - C->GetSimpleCollisionHalfHeight() < Surface + 40.f;
		const AWRCreature* Cr = Cast<AWRCreature>(C);
		if (!bIn || (Cr && UWRGraveBody::IsGraveKind(Cr->CreatureId)))
		{
			LimeSince.Remove(C);
			continue;
		}
		double* Since = LimeSince.Find(C);
		if (!Since)
		{
			LimeSince.Add(C, Now);
			WRFx::Broadcast(C, TEXT("grave.lime_steam"), C->GetActorLocation() - FVector(0.f, 0.f, C->GetSimpleCollisionHalfHeight()), FRotator::ZeroRotator, 1.f, TEXT("grave.lime_hiss"));
			continue;
		}
		if (Now - *Since < HzT(TEXT("limeTell"), 0.9f)) continue;
		// A tick each second of standing in it.
		if (FMath::Fmod(Now - *Since, 1.0) < Dt)
		{
			FWRHit Hit;
			Hit.Damage = HzT(TEXT("limeDps"), 8.f) * (HzRaining(this) ? HzT(TEXT("limeRainMult"), 2.f) : 1.f);
			Hit.bUnblockable = true;
			Hit.bMelee = false;
			Hit.Origin = C->GetActorLocation() - FVector(0.f, 0.f, 100.f);
			Hit.Element = WRTags::Element_Physical;
			UWRCombatLibrary::ApplyHit(C, Hit);
			if (UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(C)) Inv->WearArmour(HzT(TEXT("limeWear"), 0.01f));
			WRFx::Broadcast(C, TEXT("grave.lime_steam"), C->GetActorLocation() - FVector(0.f, 0.f, C->GetSimpleCollisionHalfHeight()), FRotator::ZeroRotator, 0.7f);
		}
	}
}

void UWRGraveHazards::StepDraughts(float Dt)
{
	// The catacombs' draughts: each draught room on its own timer: a 1.2 s rising whistle with every flame leaning one
	// way, then the gust: held torches in the room go out (lanterns, Wickbeetles and grave-light don't).
	UWorld* World = GetWorld();
	const double Now = HzNow(this);
	const FWRGraveData& D = FWRGraveData::Get();
	for (int32 i = 0; i < D.Rooms.Num(); ++i)
	{
		const FWRGraveData::FRoom& R = D.Rooms[i];
		if (!R.bDraught) continue;
		double& Next = NextGust.FindOrAdd(i, Now + FMath::FRandRange(HzT(TEXT("draughtMin"), 8.f), HzT(TEXT("draughtMax"), 15.f)));
		bool& bWhistle = Whistling.FindOrAdd(i, false);
		const float Tell = HzT(TEXT("draughtTell"), 1.2f);
		if (!bWhistle && Now >= Next - Tell)
		{
			bWhistle = true;
			const FVector Mid = R.bSpiral ? FVector(R.At.X, R.At.Y, (R.At.Z + R.Bottom) * 0.5f) : R.At + FVector(0.f, 0.f, 150.f);
			WRFx::Broadcast(this, TEXT("grave.draught"), Mid, FRotator(0.f, R.Yaw, 0.f), FMath::Max(1.f, R.W / 1000.f), TEXT("grave.draught_whistle"), Tell);
		}
		if (Now < Next) continue;
		bWhistle = false;
		Next = Now + FMath::FRandRange(HzT(TEXT("draughtMin"), 8.f), HzT(TEXT("draughtMax"), 15.f));
		for (TActorIterator<AWRCharacter> It(World); It; ++It)
		{
			if (!It->IsAlive() || !R.Contains(It->GetActorLocation(), 120.f)) continue;
			if (UWRSurvivalComponent* S = UWRSurvivalComponent::For(*It); S && S->SnuffTorch())
			{
				WRSound::Broadcast(*It, TEXT("light.off"));
				if (AWRPlayerController* PC = Cast<AWRPlayerController>(It->GetController())) PC->ClientNotify(TEXT("The draught put your torch out (relight it at a flame)"));
			}
		}
	}
}

// ================================================================================================ the lime pot

bool UWRGraveHazards::BuildThrownModel(const FString& ModelPath, UProceduralMeshComponent* Mesh)
{
	if (!Mesh || ModelPath != TEXT("code:limepot")) return false;
	// The lime pot (GY 2.19): a squat unglazed clay pot 15 cm long, quicklime caked white round its lip and run down one
	// side where it was filled, the mouth stopped with a wooden bung under a tied scrap of sacking (a cord round the neck).
	FWRToolMesh M;
	const FLinearColor Clay = FLinearColor(FColor(168, 132, 98)) * 0.55f, Lime = FLinearColor(FColor(237, 235, 224)) * 0.72f;
	const FLinearColor Bung = FLinearColor(FColor(110, 84, 58)) * 0.5f, Sack = FLinearColor(FColor(150, 132, 104)) * 0.45f, Cord = FLinearColor(FColor(92, 76, 58)) * 0.5f;
	M.Lathe(FVector(-7.5f, 0.f, 0.f), FVector::ForwardVector, 15.f, { FVector(0.f, 3.5f, 3.5f), FVector(0.12f, 6.4f, 6.4f), FVector(0.5f, 7.4f, 7.4f), FVector(0.8f, 5.6f, 5.6f), FVector(0.9f, 3.4f, 3.4f), FVector(1.f, 3.8f, 3.8f) }, 12, Clay, 0.9f);
	// The white crust: a collar on the lip, a run down the belly.
	M.Lathe(FVector(5.6f, 0.f, 0.f), FVector::ForwardVector, 2.f, { FVector(0.f, 4.f, 4.f), FVector(1.f, 4.1f, 4.1f) }, 12, Lime, 0.97f);
	M.Lathe(FVector(-3.f, 0.f, 6.6f), FVector::ForwardVector, 8.f, { FVector(0.f, 0.6f, 1.6f), FVector(0.5f, 0.9f, 2.2f), FVector(1.f, 0.6f, 1.4f) }, 6, Lime, 0.97f);
	M.Lathe(FVector(7.4f, 0.f, 0.f), FVector::ForwardVector, 2.6f, { FVector(0.f, 3.f, 3.f), FVector(1.f, 2.6f, 2.6f) }, 8, Bung, 0.8f);
	M.Lathe(FVector(8.6f, 0.f, 0.f), FVector::ForwardVector, 1.6f, { FVector(0.f, 4.4f, 4.4f), FVector(0.5f, 3.6f, 3.6f), FVector(1.f, 1.f, 1.f) }, 8, Sack, 0.95f);
	M.Tube({ FVector(6.8f, 3.8f, 0.f), FVector(6.8f, 0.f, 3.9f), FVector(6.8f, -3.8f, 0.f), FVector(6.8f, 0.f, -3.9f) }, 0.45f, 5, Cord, 0.85f, true);
	M.ApplyTo(Mesh);
	WRFx::Attach(Mesh, TEXT("grave.lime_trail"));
	return true;
}

void UWRGraveHazards::LimePotLanded(UWorld* World, const FVector& At, AWRCharacterBase* Thrower, float Damage)
{
	if (!World || World->GetNetMode() == NM_Client) return;
	const float Radius = WRTune(TEXT("hazards.graveyard.limePotRadius"), 400.f);
	const float Seconds = WRTune(TEXT("hazards.graveyard.limePotSeconds"), 3.f);
	const float Dps = WRTune(TEXT("hazards.graveyard.limePotDps"), 8.f);
	WRFx::Broadcast(World, TEXT("grave.lime_burst"), At, FRotator::ZeroRotator, Radius / 200.f, TEXT("grave.lime_pot"), Seconds);
	const auto Burn = [World, At, Radius](AWRCharacterBase* By, float Amount, bool bTick)
	{
		for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
		{
			AWRCharacterBase* C = *It;
			if (!C->IsAlive() || C == By || FVector::Dist(C->GetActorLocation(), At) > Radius + C->GetSimpleCollisionRadius()) continue;
			if (By && !By->IsHostileTo(C)) continue;
			// The Graveyard's own don't mind lime (creatures.json resists); everyone else burns.
			if (const AWRCreature* Cr = Cast<AWRCreature>(C); Cr && Cr->GetDef() && Cr->GetDef()->Resists.Contains(TEXT("lime"))) continue;
			FWRHit Hit;
			Hit.Instigator = By;
			Hit.Damage = Amount;
			Hit.Origin = At;
			Hit.Element = WRTags::Element_Physical;
			Hit.bDamageOverTime = bTick;
			Hit.bProjectile = !bTick;
			Hit.bUnblockable = bTick;
			UWRCombatLibrary::ApplyHit(C, Hit);
		}
	};
	Burn(Thrower, Damage, false);
	FWRGraveBoss::LimeLanded(World, At, Thrower);
	// The cloud hangs and burns a second at a time.
	TWeakObjectPtr<AWRCharacterBase> WeakBy(Thrower);
	TWeakObjectPtr<UWorld> WeakW(World);
	for (int32 s = 1; s <= int32(Seconds); ++s)
	{
		FTimerHandle H;
		World->GetTimerManager().SetTimer(H, [WeakW, WeakBy, Burn, Dps]()
		{
			if (WeakW.IsValid()) Burn(WeakBy.Get(), Dps, true);
		}, float(s), false);
	}
}
