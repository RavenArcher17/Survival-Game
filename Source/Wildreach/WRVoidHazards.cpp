// The Corrupted wasteland's litter fire, hollow crust and the Rift Storm (see WRVoidHazards.h). Numbers: tuning
// voidHazards.*, Data/lands/wasteland.json "crust" and "riftStorm", world.json events.riftstorm.
#include "WRVoidHazards.h"
#include "Wildreach.h"
#include "WRBuilding.h"
#include "WRCharacter.h"
#include "WRCharacterBase.h"
#include "WRCombatLibrary.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRLand.h"
#include "WRPathComponent.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRPortalsInternal.h"
#include "WRResourceNode.h"
#include "WRSound.h"
#include "WRTuning.h"
#include "WRVoid.h"
#include "WRVoidBosses.h"
#include "WRVoidWorld.h"
#include "WRVoidCreatures.h"
#include "WRWorldSystems.h"
#include "WRCombatTypes.h"
#include "WRWorldSystems.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

namespace WRVoidHazardsImpl
{
	float HT(const TCHAR* Key, float F) { return WRTune(*FString::Printf(TEXT("voidHazards.%s"), Key), F); }
	const float Cell = 6.f;   // m
	double SNow(const UObject* W)
	{
		const UWorld* World = W ? W->GetWorld() : nullptr;
		const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
		return GS ? GS->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);
	}
}
using namespace WRVoidHazardsImpl;

AWRVoidState::AWRVoidState()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRVoidState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRVoidState, Fire);
	DOREPLIFETIME(AWRVoidState, CrustBroken);
	DOREPLIFETIME(AWRVoidState, StormStage);
}

double AWRVoidState::Now() const { return SNow(this); }

AWRVoidState* AWRVoidState::Find(const UWorld* World)
{
	if (!World) return nullptr;
	for (TActorIterator<AWRVoidState> It(World); It; ++It) return *It;
	return nullptr;
}

void AWRVoidState::Spawn(AWRLand* Land)
{
	if (!Land || !Land->Def() || Land->GetNetMode() == NM_Client || Find(Land->GetWorld())) return;
	const FTransform T(Land->Def()->Origin);
	if (AWRVoidState* S = Land->GetWorld()->SpawnActorDeferred<AWRVoidState>(AWRVoidState::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		S->LandId = Land->LandId;
		S->FinishSpawning(T);
	}
	// Anchor salt answers the storm: -300 on a rift heart.
	static bool bBound = false;
	if (!bBound)
	{
		bBound = true;
		WRVoidWorld::OnSalted().AddLambda([](const FVector& At, float Radius, AWRCharacterBase* By)
		{
			UWorld* W = By ? By->GetWorld() : nullptr;
			if (!W) return;
			for (TActorIterator<AWRCreature> It(W); It; ++It)
			{
				if (It->CreatureId != TEXT("rift_heart") || !It->IsAlive() || FVector::Dist(It->GetActorLocation(), At) > Radius + 200.f) continue;
				FWRHit Hit;
				Hit.Instigator = By;
				Hit.Damage = WRTune(TEXT("voidHazards.storm.saltDamage"), 300.f);
				Hit.bUnblockable = true;
				Hit.Origin = At;
				UWRCombatLibrary::ApplyHit(*It, Hit);
			}
		});
	}
}

void AWRVoidState::BeginPlay()
{
	Super::BeginPlay();
	if (LandId.IsNone())
	{
		for (const FWRLandDef& D : WRLand::All()) if (D.Biome == TEXT("wasteland")) LandId = D.Id;
	}
	BuildCrust();
}

FIntPoint AWRVoidState::CellOf(const FVector2D& M) const
{
	return FIntPoint(FMath::FloorToInt(M.X / Cell), FMath::FloorToInt(M.Y / Cell));
}

FVector AWRVoidState::CellCentre(const FIntPoint& C) const
{
	const FWRLandDef* D = WRLand::Find(LandId);
	return D ? D->GroundPoint(FVector2D((C.X + 0.5f) * Cell, (C.Y + 0.5f) * Cell)) : FVector::ZeroVector;
}

bool AWRVoidState::Burnable(const FIntPoint& C) const
{
	const FWRLandDef* D = WRLand::Find(LandId);
	if (!D) return false;
	const FVector2D M((C.X + 0.5f) * Cell, (C.Y + 0.5f) * Cell);
	const FWRLandZone* Z = D->ZoneAt(M);
	if (!Z || Z->Id != TEXT("V1") || D->CoastDistance(M) < 3.f) return false;
	// The road is bare: no litter on it.
	if (const FWRLandFeature* Road = D->Feature(TEXT("gauntlet_road"))) if (WRLand::DistToPolyline(M, Road->Points) < Road->Width * 0.5f) return false;
	const double* Burnt = BurntAt.Find(C);
	if (Burnt && Now() - *Burnt < HT(TEXT("litter.regrowSeconds"), 7200.f)) return false;
	return !Fire.ContainsByPredicate([&](const FWRFireCell& F) { return F.X == C.X && F.Y == C.Y; });
}

void AWRVoidState::FireAt(const UObject* WorldContext, const FVector& At)
{
	UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	AWRVoidState* S = Find(W);
	if (!S || !S->HasAuthority()) return;
	const FWRLandDef* D = WRLand::Find(S->LandId);
	if (!D || !D->Contains(At, 0.f)) return;
	const FIntPoint C = S->CellOf(D->ToMap(At));
	if (!S->Burnable(C) || S->Fire.Num() >= int32(HT(TEXT("litter.maxCells"), 500.f))) return;
	FWRFireCell F;
	F.X = C.X;
	F.Y = C.Y;
	F.Lit = float(S->Now());
	S->Fire.Add(F);
	WRSound::PlayAt(S, TEXT("void.litter.smoulder"), S->CellCentre(C));
	UE_LOG(LogWildreach, Log, TEXT("VOID litter catches at %s"), *At.ToCompactString());
}

bool AWRVoidState::Burning(const UWorld* World, const FVector& At)
{
	const AWRVoidState* S = Find(World);
	const FWRLandDef* D = S ? WRLand::Find(S->LandId) : nullptr;
	if (!D) return false;
	const FIntPoint C = S->CellOf(D->ToMap(At));
	const float T = float(S->Now());
	return S->Fire.ContainsByPredicate([&](const FWRFireCell& F) { return F.X == C.X && F.Y == C.Y && T - F.Lit > HT(TEXT("litter.smoulder"), 0.9f); });
}

void AWRVoidState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TickFire(DeltaSeconds);
	TickCrust(DeltaSeconds);
	if (HasAuthority()) TickStorm(DeltaSeconds);
	if (GetNetMode() != NM_DedicatedServer) DrawFire();
}

void AWRVoidState::TickFire(float Dt)
{
	if (!HasAuthority() || Fire.Num() == 0) return;
	const float T = float(Now());
	const float Smoulder = HT(TEXT("litter.smoulder"), 0.9f), Burn = HT(TEXT("litter.burnSeconds"), 60.f);
	const float Down = HT(TEXT("litter.downwind"), 1.5f), Up = HT(TEXT("litter.upwind"), 0.5f);
	FVector2D Wind(HT(TEXT("litter.windX"), 1.f), HT(TEXT("litter.windY"), 0.3f));
	Wind.Normalize();
	// The front walks: each burning cell lights its neighbours after the time the fire takes to cross 6 m that way
	// (downwind 1.5 m/s, upwind 0.5, across in between).
	TArray<FWRFireCell> New;
	for (const FWRFireCell& F : Fire)
	{
		const float Age = T - F.Lit - Smoulder;
		if (Age < 0.f) continue;
		for (int32 dx = -1; dx <= 1; ++dx) for (int32 dy = -1; dy <= 1; ++dy)
		{
			if (!dx && !dy) continue;
			const FVector2D Dir = FVector2D(dx, dy).GetSafeNormal();
			const float Align = FVector2D::DotProduct(Dir, Wind);
			const float Speed = FMath::Lerp(Up, Down, Align * 0.5f + 0.5f);
			const float Need = Cell * FVector2D(dx, dy).Size() / FMath::Max(0.1f, Speed);
			if (Age < Need) continue;
			const FIntPoint N(F.X + dx, F.Y + dy);
			if (!Burnable(N) || New.ContainsByPredicate([&](const FWRFireCell& Q) { return Q.X == N.X && Q.Y == N.Y; })) continue;
			FWRFireCell Q;
			Q.X = N.X;
			Q.Y = N.Y;
			Q.Lit = T - Smoulder;   // the front arrives burning (only the first spark smoulders)
			New.Add(Q);
		}
	}
	if (New.Num() && Fire.Num() + New.Num() <= int32(HT(TEXT("litter.maxCells"), 500.f))) Fire.Append(New);
	// Burnt out: black ground for 2 game days.
	for (int32 i = Fire.Num() - 1; i >= 0; --i)
	{
		if (T - Fire[i].Lit > Smoulder + Burn)
		{
			BurntAt.Add(FIntPoint(Fire[i].X, Fire[i].Y), Now());
			Fire.RemoveAtSwap(i);
		}
	}
	// Standing in it: 3 a second and burn 8 a second, in half-second bites.
	FireDamageClock += Dt;
	if (FireDamageClock < 0.5f) return;
	FireDamageClock = 0.f;
	const FWRLandDef* D = WRLand::Find(LandId);
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		if (!It->IsAlive() || !D || !D->Contains(It->GetActorLocation(), 0.f) || !Burning(GetWorld(), It->GetActorLocation())) continue;
		FWRHit Hit;
		Hit.Instigator = this;
		Hit.Damage = HT(TEXT("litter.dps"), 3.f) * 0.5f;
		Hit.Element = WRTags::Element_Fire;
		Hit.BurnBuildup = HT(TEXT("litter.burnPerSecond"), 8.f) * 0.5f;
		Hit.bDamageOverTime = true;
		Hit.Origin = It->GetActorLocation();
		UWRCombatLibrary::ApplyHit(*It, Hit);
	}
}

void AWRVoidState::DrawFire()
{
	// Each cell: smoke while it smoulders, flame while it burns; drawn from the replicated rows, gone when they go.
	const float T = float(Now());
	const float Smoulder = HT(TEXT("litter.smoulder"), 0.9f);
	TSet<FIntPoint> Seen;
	for (const FWRFireCell& F : Fire)
	{
		const FIntPoint C(F.X, F.Y);
		Seen.Add(C);
		const uint8 Want = T - F.Lit < Smoulder ? 1 : 2;
		uint8& Has = FlameState.FindOrAdd(C);
		if (Has == Want) continue;
		if (TWeakObjectPtr<USceneComponent>* Old = Flames.Find(C)) if (Old->IsValid()) WRFx::Stop(Old->Get());
		USceneComponent* Fx = WRFx::Attach(RootComponent, Want == 1 ? TEXT("crater.smoke") : TEXT("brazier.fire"), NAME_None, Want == 1 ? 0.6f : 2.2f);
		if (Fx)
		{
			Fx->SetUsingAbsoluteLocation(true);
			Fx->SetWorldLocation(CellCentre(C) + FVector(0, 0, 20.f));
		}
		Flames.Add(C, Fx);
		Has = Want;
	}
	for (auto It = Flames.CreateIterator(); It; ++It)
	{
		if (Seen.Contains(It->Key)) continue;
		if (It->Value.IsValid()) WRFx::Stop(It->Value.Get());
		FlameState.Remove(It->Key);
		It.RemoveCurrent();
	}
}

// ---------------------------------------------------------------- the crust

void AWRVoidState::BuildCrust()
{
	const FWRLandDef* D = WRLand::Find(LandId);
	const TArray<TSharedPtr<FJsonValue>>* L = nullptr;
	if (!D || !D->Json.IsValid() || !D->Json->TryGetArrayField(TEXT("crust"), L)) return;
	UMaterialInterface* Dust = WRFoldLook::RockMat(this, FLinearColor(FColor(185, 180, 172)) * 0.4f);
	UMaterialInterface* Crack = WRFoldLook::RockMat(this, FLinearColor(0.02f, 0.02f, 0.022f));
	const bool bDraw = GetNetMode() != NM_DedicatedServer;
	for (const TSharedPtr<FJsonValue>& V : *L)
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FCrust C;
		C.At = WRLand::Vec2(O->TryGetField(TEXT("at")));
		C.R = float(O->GetNumberField(TEXT("r")));
		C.Depth = float(O->GetNumberField(TEXT("depth")));
		O->TryGetBoolField(TEXT("rift"), C.bRift);
		// The crust lies at the ground's level round the pocket (the pocket is the land's "pit" under it).
		const float Level = D->Origin.Z + D->Height(C.At + FVector2D(C.R + 2.5f, 0.f)) * 100.f;
		const FVector Centre = D->ToWorld(C.At, 0.f);
		UStaticMeshComponent* Disc = WRFoldLook::Shape(this, RootComponent, TEXT("Cylinder"), FVector(C.R * 200.f, C.R * 200.f, 30.f), FVector::ZeroVector, FRotator::ZeroRotator, Dust);
		if (Disc)
		{
			Disc->SetUsingAbsoluteLocation(true);
			Disc->SetWorldLocation(FVector(Centre.X, Centre.Y, Level - 10.f));
			Disc->SetCollisionProfileName(TEXT("BlockAll"));
			Disc->SetVisibility(bDraw);
			C.Disc = Disc;
			// A faint grid of cracks over it (the thin floor's tell).
			if (bDraw)
			{
				for (int32 k = 0; k < 8; ++k)
				{
					const float A = 22.5f * k;
					if (UStaticMeshComponent* Line = WRFoldLook::Shape(this, Disc, TEXT("Cube"), FVector(C.R * 170.f / (C.R * 2.f), 3.f / (C.R * 2.f), 0.2f), FVector(0, 0, 0.55f), FRotator(0.f, A, 0.f), Crack)) Line->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				}
			}
		}
		Crusts.Add(C);
		// One of the pockets holds a small rift (server).
		if (C.bRift && HasAuthority())
		{
			const FVector Floor = D->GroundPoint(C.At);
			if (AWRRift* R = GetWorld()->SpawnActorDeferred<AWRRift>(AWRRift::StaticClass(), FTransform(Floor), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
			{
				R->RiftId = TEXT("crust_pocket");
				R->bLarge = false;
				R->ZoneRadius = C.R * 100.f;
				R->Rubble = 0;
				R->FinishSpawning(FTransform(Floor));
			}
		}
	}
}

void AWRVoidState::CrustStruck(const UObject* WorldContext, const FVector& At, float Radius)
{
	UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	AWRVoidState* S = Find(W);
	const FWRLandDef* D = S ? WRLand::Find(S->LandId) : nullptr;
	if (!S || !D || !S->HasAuthority()) return;
	const FVector2D M = D->ToMap(At);
	for (int32 i = 0; i < S->Crusts.Num() && i < 32; ++i)
	{
		if ((S->CrustBroken & (1u << i)) || FVector2D::Distance(S->Crusts[i].At, M) > S->Crusts[i].R + Radius / 100.f) continue;
		S->CrustBroken |= (1u << i);
		S->Crusts[i].BrokenAt = S->Now();
		WRFx::Broadcast(S, TEXT("dust.cloud"), D->ToWorld(S->Crusts[i].At, D->Height(S->Crusts[i].At + FVector2D(S->Crusts[i].R + 2.5f, 0.f))), FRotator::ZeroRotator, S->Crusts[i].R / 2.f, TEXT("void.crust.break"));
		UE_LOG(LogWildreach, Log, TEXT("VOID hollow crust %d breaks"), i);
	}
}

void AWRVoidState::TickCrust(float Dt)
{
	// Every machine: the disc as the bits say; the drum underfoot within 6 m of an unbroken one.
	for (int32 i = 0; i < Crusts.Num() && i < 32; ++i)
	{
		const bool bBroken = (CrustBroken & (1u << i)) != 0;
		if (UStaticMeshComponent* Disc = Crusts[i].Disc.Get())
		{
			if (Disc->IsCollisionEnabled() == bBroken) Disc->SetCollisionEnabled(bBroken ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
			if (GetNetMode() != NM_DedicatedServer && Disc->IsVisible() == bBroken) Disc->SetVisibility(!bBroken, true);
		}
	}
	const FWRLandDef* D = WRLand::Find(LandId);
	if (!D) return;
	if (GetNetMode() != NM_DedicatedServer)
	{
		DrumClock += Dt;
		const APlayerController* PC = GetWorld()->GetFirstPlayerController();
		const APawn* Me = PC ? PC->GetPawn() : nullptr;
		if (Me && DrumClock > 1.3f && Me->GetVelocity().Size2D() > 150.f)
		{
			for (int32 i = 0; i < Crusts.Num() && i < 32; ++i)
			{
				if (!(CrustBroken & (1u << i)) && FVector2D::Distance(D->ToMap(Me->GetActorLocation()), Crusts[i].At) < Crusts[i].R + 6.f)
				{
					DrumClock = 0.f;
					WRSound::PlayAt(Me, TEXT("void.crust.drum"), Me->GetActorLocation() - FVector(0, 0, 90.f));
					break;
				}
			}
		}
	}
	if (!HasAuthority()) return;
	// What breaks it: a roll, a heavy landing, a heavy blow, a heavy creature on it. It skins over again in time.
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		if (!It->IsAlive()) continue;
		const FVector2D M = D->ToMap(It->GetActorLocation());
		for (int32 i = 0; i < Crusts.Num() && i < 32; ++i)
		{
			if ((CrustBroken & (1u << i)) || FVector2D::Distance(M, Crusts[i].At) > Crusts[i].R) continue;
			const AWRCreature* Cr = Cast<AWRCreature>(*It);
			const bool bHeavy = It->HasTag(WRTags::State_Dodging) || FWRVoidBoss::HeavySwing(*It) || (Cr && Cr->GetSimpleCollisionRadius() >= 150.f)
				|| It->GetCharacterMovement()->Velocity.Z < -HT(TEXT("crust.landingSpeed"), 900.f);
			if (bHeavy) CrustStruck(this, It->GetActorLocation(), 0.f);
		}
	}
	for (int32 i = 0; i < Crusts.Num() && i < 32; ++i)
	{
		if ((CrustBroken & (1u << i)) && Now() - Crusts[i].BrokenAt > HT(TEXT("crust.regrowSeconds"), 1800.f)) CrustBroken &= ~(1u << i);
	}
}

// ---------------------------------------------------------------- the Rift Storm

void AWRVoidState::TickStorm(float Dt)
{
	const AWRWorldState* WS = AWRWorldState::Get(GetWorld());
	const FWRWorldEventRep* E = WS ? WS->Event(TEXT("riftstorm")) : nullptr;
	const uint8 Want = !E ? 0 : E->Stage >= 1 ? 2 : 1;
	if (Want != StormStage)
	{
		StormStage = Want;
		if (Want == 1) WRSound::PlayAt(this, TEXT("void.storm.warning"), GetActorLocation());
		if (Want == 2) OpenStorm();
		if (Want == 0) CloseStorm();
	}
	if (StormStage != 2) return;
	const double T = Now();
	const int32 Waves = int32(HT(TEXT("storm.waves"), 3.f));
	for (int32 r = 0; r < Storm.Num(); ++r)
	{
		FStormRift& S = Storm[r];
		if (S.bClosed) continue;
		if (S.WavesSent < Waves && T >= S.NextWave)
		{
			Wave(r, S.WavesSent);
			++S.WavesSent;
			S.NextWave = T + HT(TEXT("storm.waveEvery"), 240.f);
		}
	}
}

void AWRVoidState::OpenStorm()
{
	const FWRLandDef* D = WRLand::Find(LandId);
	const TSharedPtr<FJsonObject> Row = D ? D->Section(TEXT("riftStorm")) : nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Seeds = nullptr;
	if (!Row.IsValid() || !Row->TryGetArrayField(TEXT("seeds"), Seeds)) return;
	// 6 of the 10 seeds: those nearest the claims first (every claim within 1.5 km gets one within 600 m where it can),
	// then the rest by chance.
	TArray<int32> Order;
	for (int32 i = 0; i < Seeds->Num(); ++i) Order.Add(i);
	TArray<FVector> Claims;
	for (TActorIterator<AWRClaimTotem> It(GetWorld()); It; ++It) if (D->Contains(It->GetActorLocation(), 1500.f)) Claims.Add(It->GetActorLocation());
	Order.Sort([&](int32 A, int32 B)
	{
		const auto Near = [&](int32 i) { float Best = 1e9f; for (const FVector& C : Claims) Best = FMath::Min(Best, float(FVector::Dist2D(C, D->GroundPoint(WRLand::Vec2((*Seeds)[i]))))); return Best; };
		return Near(A) < Near(B);
	});
	const int32 Open = FMath::Min(int32(HT(TEXT("storm.open"), 6.f)), Order.Num());
	for (int32 k = Claims.Num() == 0 ? 0 : Open; k < Order.Num(); ++k) Order.Swap(k, FMath::RandRange(k, Order.Num() - 1));
	for (int32 k = 0; k < Open; ++k)
	{
		const FVector At = D->GroundPoint(WRLand::Vec2((*Seeds)[Order[k]]));
		FStormRift S;
		S.At = At;
		// A 10 m tear with a 40 m rift zone; its heart (a 1.5 m voidglass core) floats at the tear's centre.
		if (AWRRift* R = GetWorld()->SpawnActorDeferred<AWRRift>(AWRRift::StaticClass(), FTransform(At), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			R->RiftId = FName(*FString::Printf(TEXT("storm_%d"), k));
			R->bLarge = true;
			R->ZoneRadius = HT(TEXT("storm.zone"), 4000.f);
			R->Rubble = 4;
			R->FinishSpawning(FTransform(At));
			S.Rift = R;
		}
		S.Heart = AWRCreature::SpawnCreature(GetWorld(), TEXT("rift_heart"), At + FVector(0, 0, 500.f), FRotator::ZeroRotator);
		if (S.Heart.IsValid()) S.Heart->GetCharacterMovement()->DisableMovement();
		S.NextWave = Now() + 5.0;
		Storm.Add(S);
	}
	UE_LOG(LogWildreach, Log, TEXT("VOID the Rift Storm opens %d rifts"), Storm.Num());
}

void AWRVoidState::Wave(int32 Index, int32 N)
{
	FStormRift& S = Storm[Index];
	// Wave 1: a Gnawer swarm (8) and 2 Hollowfangs. Wave 2: 3 Hollowfangs and a Nullwing. Wave 3: a Riftmaw (it stays at
	// its rift) and 2 Riftborn. Each wave picks its target as it spawns: the nearest claim within 1.5 km, else the
	// nearest shrine; it walks there and never retargets to a farther base.
	TArray<TPair<FName, int32>> Kinds;
	if (N == 0) Kinds = { { TEXT("gnawer"), 8 }, { TEXT("hollowfang"), 2 } };
	else if (N == 1) Kinds = { { TEXT("hollowfang"), 3 }, { TEXT("nullwing"), 1 } };
	else Kinds = { { TEXT("riftmaw"), 1 }, { TEXT("hollowfang"), 2 } };
	const AWRClaimTotem* Claim = nullptr;
	float Best = HT(TEXT("storm.targetRange"), 150000.f);
	for (TActorIterator<AWRClaimTotem> It(GetWorld()); It; ++It)
	{
		const float D = FVector::Dist2D(It->GetActorLocation(), S.At);
		if (D < Best) { Best = D; Claim = *It; }
	}
	FVector Goal = S.At;
	if (!Claim)
	{
		float BestS = 1e12f;
		for (TActorIterator<AWRShrine> It(GetWorld()); It; ++It)
		{
			const float D = FVector::Dist2D(It->GetActorLocation(), S.At);
			if (D < BestS) { BestS = D; Goal = It->GetActorLocation(); }
		}
	}
	for (const TPair<FName, int32>& K : Kinds)
	{
		for (int32 i = 0; i < K.Value; ++i)
		{
			const FVector At = S.At + FVector(FMath::FRandRange(-500.f, 500.f), FMath::FRandRange(-500.f, 500.f), 200.f);
			AWRCreature* C = AWRCreature::SpawnCreature(GetWorld(), K.Key, At, FRotator::ZeroRotator);
			if (!C) continue;
			if (N == 2 && K.Key == TEXT("hollowfang")) if (UWRVoidBody* B = UWRVoidBody::Get(C)) B->Set(WRVoidFlag::Riftborn, true);
			if (K.Key == TEXT("riftmaw")) { C->SetHome(S.At); S.Live.Add(C); continue; }
			if (Claim)
			{
				C->bRaider = true;
				C->RaidBase = Claim->GetActorLocation();
				C->RaidRadius = Claim->Radius;
				C->RaidOwner = Claim->OwnerName;
				C->SetHome(Claim->GetActorLocation());
			}
			else C->SetHome(Goal);
			S.Live.Add(C);
		}
	}
	WRFx::Broadcast(this, TEXT("void.rift.burst"), S.At, FRotator::ZeroRotator, 3.f, TEXT("void.rift.burst"));
}

void AWRVoidState::CloseStorm()
{
	// At the day's end the storm rifts close; the waves already walking keep walking.
	for (FStormRift& S : Storm)
	{
		if (S.Rift.IsValid()) S.Rift->Destroy();
		if (S.Heart.IsValid()) S.Heart->Destroy();
	}
	for (TActorIterator<AWRResourceNode> It(GetWorld()); It; ++It) if (It->Tags.Contains(TEXT("storm_ring"))) It->Destroy();
	Storm.Reset();
}

void AWRVoidState::NoteHeart(AWRCreature* Heart, const FWRHit& Hit, float Damage)
{
	const APawn* By = Cast<APawn>(Hit.Instigator);
	if (const AWRCreature* Tame = Cast<AWRCreature>(Hit.Instigator); Tame && Tame->IsTamed())
	{
		for (TActorIterator<AWRCharacter> It(GetWorld()); It; ++It) if (It->GetPlayerState() && It->GetPlayerState()->GetPlayerName() == Tame->TamedBy) By = *It;
	}
	if (!By || !By->GetPlayerState()) return;
	for (FStormRift& S : Storm) if (S.Heart.Get() == Heart) S.Dealt.FindOrAdd(By->GetPlayerState()->GetPlayerName()) += Damage;
}

void AWRVoidState::HeartDied(AWRCreature* Heart)
{
	AWRVoidState* S = Find(Heart ? Heart->GetWorld() : nullptr);
	if (!S) return;
	for (FStormRift& R : S->Storm)
	{
		if (R.Heart.Get() != Heart || R.bClosed) continue;
		R.bClosed = true;
		// The rift closes; its ring stays worth x3 for the rest of the storm.
		if (R.Rift.IsValid()) R.Rift->Destroy();
		WRFx::Broadcast(S, TEXT("void.rift.plunge"), R.At, FRotator::ZeroRotator, 4.f, TEXT("void.rift.plunge"));
		const FWRLandDef* D = WRLand::Find(S->LandId);
		for (int32 n = 0; n < 8 && D; ++n)
		{
			const float A = 2.f * PI * n / 8.f;
			const FVector P = D->GroundPoint(D->ToMap(R.At) + FVector2D(FMath::Cos(A), FMath::Sin(A)) * 7.f);
			if (AWRResourceNode* Node = S->GetWorld()->SpawnActorDeferred<AWRResourceNode>(AWRResourceNode::StaticClass(), FTransform(P), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
			{
				Node->ItemId = TEXT("voidglass");
				Node->Hits = 4;
				Node->PerHit = int32(WRTune(TEXT("void.ring.perHit"), 1.6f) * HT(TEXT("storm.ringMult"), 3.f));
				Node->RespawnSeconds = 99999.f;
				Node->GemChance = 0.f;
				Node->Tags.Add(TEXT("storm_ring"));
				Node->Tags.Add(TEXT("rift_ring"));
				Node->FinishSpawning(FTransform(P));
			}
		}
		// 1,500 path XP for the closed rift, split by heart damage: to the Void path if they walk it, else half to each path.
		float Total = 0.f;
		for (const TPair<FString, float>& KV : R.Dealt) Total += KV.Value;
		for (const TPair<FString, float>& KV : R.Dealt)
		{
			const int32 Xp = FMath::RoundToInt(HT(TEXT("storm.xp"), 1500.f) * KV.Value / FMath::Max(1.f, Total));
			for (TActorIterator<AWRCharacter> It(S->GetWorld()); It; ++It)
			{
				if (!It->GetPlayerState() || It->GetPlayerState()->GetPlayerName() != KV.Key) continue;
				UWRPathComponent* P = UWRPathComponent::FindFor(*It);
				if (!P) continue;
				if (P->IsActivePath(TEXT("void"))) P->AwardXP(TEXT("void"), Xp);
				else
				{
					if (!P->GetMainPath().IsNone()) P->AwardXP(P->GetMainPath(), Xp / 2);
					if (!P->GetSubPath().IsNone()) P->AwardXP(P->GetSubPath(), Xp / 2);
				}
				if (AWRPlayerController* PC = Cast<AWRPlayerController>(It->GetController())) PC->ClientNotify(FString::Printf(TEXT("The storm rift closes: %d path XP"), Xp));
			}
		}
	}
}
