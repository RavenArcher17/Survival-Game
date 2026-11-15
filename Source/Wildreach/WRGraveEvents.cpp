// The Blood Moon (see WRGraveEvents.h; cooks/biome-graveyard.md 2.10).

#include "WRGraveEvents.h"
#include "Wildreach.h"
#include "WRCharacter.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRGameState.h"
#include "WRGraveTypes.h"
#include "WRPlayerController.h"
#include "WRResourceNode.h"
#include "WRSocial.h"
#include "WRSound.h"
#include "WRSurvival.h"
#include "WRTuning.h"
#include "WRWorldSystems.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"

namespace WRGraveEventImpl
{
	float GeT(const TCHAR* Key, float Fallback) { return FWRTuning::Get().Num(FString::Printf(TEXT("graveyardEvents.bloodMoon.%s"), Key), Fallback); }
	float GeNow(const UObject* W)
	{
		const UWorld* World = W ? W->GetWorld() : nullptr;
		const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
		return GS ? float(GS->GetServerWorldTimeSeconds()) : (World ? World->GetTimeSeconds() : 0.f);
	}
	const FWRWorldEventRep* GeEvent(const UObject* W)
	{
		const AWRWorldState* WS = AWRWorldState::Get(W);
		return WS ? WS->Event(TEXT("bloodmoon")) : nullptr;
	}
	bool GeNight(const UObject* W)
	{
		const float H = AWRGameState::HourIn(W);
		return H >= GeT(TEXT("risingFrom"), 20.f) || H < GeT(TEXT("risingTo"), 5.f);
	}
	void GeTellLand(UWorld* World, const FString& Text)
	{
		for (TActorIterator<AWRCharacter> It(World); It; ++It)
		{
			if (!WRGrave::Contains(It->GetActorLocation())) continue;
			if (AWRPlayerController* PC = Cast<AWRPlayerController>(It->GetController())) PC->ClientNotify(Text);
		}
	}
	FVector GeGround(const UWorld* World, const FVector& At)
	{
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRGraveEventGround), false);
		return World->LineTraceSingleByChannel(Hit, At + FVector(0.f, 0.f, 3000.f), At - FVector(0.f, 0.f, 6000.f), ECC_WorldStatic, Q) ? FVector(Hit.ImpactPoint) : At;
	}
}
using namespace WRGraveEventImpl;

bool UWRGraveEvents::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

bool UWRGraveEvents::IsTickable() const
{
	return GetWorld() && WRGrave::Live() && FWRGraveData::Get().bLoaded;
}

bool UWRGraveEvents::Sign(const UObject* WorldContext)
{
	const FWRWorldEventRep* E = GeEvent(WorldContext);
	return E && E->Stage == 0 && GeNow(WorldContext) < E->StartAt;
}

bool UWRGraveEvents::On(const UObject* WorldContext)
{
	const FWRWorldEventRep* E = GeEvent(WorldContext);
	return E && E->Stage == 1 && GeNow(WorldContext) < E->EndAt;
}

bool UWRGraveEvents::Rising(const UObject* WorldContext)
{
	return On(WorldContext) && GeNight(WorldContext);
}

void UWRGraveEvents::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	if (!World || !World->HasBegunPlay()) return;
	// Every machine: the red moon eases in with the sign, stays through the Rising, and sets at dawn (the Mourning has
	// the Graveyard's moonless sky back).
	const float Target = Sign(this) || Rising(this) ? 1.f : 0.f;
	WRGraveSky::BloodMoon = FMath::FInterpConstantTo(WRGraveSky::BloodMoon, Target, DeltaTime, 1.f / FMath::Max(1.f, GeT(TEXT("easeSeconds"), 20.f)));
	if (World->GetNetMode() == NM_Client) return;
	StepServer(DeltaTime);
}

void UWRGraveEvents::BuildRoads()
{
	bRoads = true;
	const FWRGraveData& D = FWRGraveData::Get();
	for (const FName Id : D.ProcessionRoads)
	{
		const FWRGraveData::FPath* P = D.FindRoad(Id);
		if (!P || P->Points.Num() < 2) continue;
		FRoadLine R;
		R.P = P->Points;
		R.Acc.Add(0.f);
		for (int32 i = 1; i < R.P.Num(); ++i) R.Acc.Add(R.Acc.Last() + float(FVector::Dist2D(R.P[i], R.P[i - 1])));
		R.Len = R.Acc.Last();
		Roads.Add(R);
	}
}

FVector UWRGraveEvents::RoadAt(const FRoadLine& R, float S) const
{
	S = FMath::Clamp(S, 0.f, R.Len);
	for (int32 i = 1; i < R.P.Num(); ++i)
	{
		if (S <= R.Acc[i])
		{
			const float Seg = FMath::Max(1.f, R.Acc[i] - R.Acc[i - 1]);
			return FMath::Lerp(R.P[i - 1], R.P[i], (S - R.Acc[i - 1]) / Seg);
		}
	}
	return R.P.Last();
}

void UWRGraveEvents::StepServer(float Dt)
{
	UWorld* World = GetWorld();
	if (!bRoads) BuildRoads();
	const bool bSign = Sign(this), bRising = Rising(this);
	// The sign: every bell tolls once (the Belfry, the chapel's broken tower).
	if (bSign && !bWasSign)
	{
		const FWRGraveData& D = FWRGraveData::Get();
		for (const FWRGraveData::FSite& S : D.Sites)
		{
			if (S.Kind == TEXT("belfry")) WRFx::Broadcast(World, TEXT("grave.toll_ring"), S.At + FVector(0.f, 0.f, WRGraveBelfry::Wheel().Z), FRotator::ZeroRotator, 1.f, TEXT("grave.bell_toll"));
			if (S.Kind == TEXT("chapel")) WRFx::Broadcast(World, NAME_None, S.At + FRotator(0.f, S.Yaw, 0.f).RotateVector(FVector(WRGraveChapel::TowerX, 0.f, WRGraveChapel::TowerH)), FRotator::ZeroRotator, 1.f, TEXT("grave.bell_toll"));
		}
		GeTellLand(World, TEXT("The moon is rising over the chapel, and it is red"));
		UE_LOG(LogWildreach, Log, TEXT("GRAVEYARD the Blood Moon's sign"));
	}
	bWasSign = bSign;
	if (bRising && !bWasRising)
	{
		GeTellLand(World, TEXT("The Rising: the dead are walking the roads toward the lights"));
		// The procession slots: one every spacing along each procession road.
		Processions.Reset();
		const float Spacing = GeT(TEXT("spacing"), 15000.f);
		for (int32 r = 0; r < Roads.Num(); ++r)
		{
			for (float S = Spacing * 0.5f; S < Roads[r].Len; S += Spacing)
			{
				if (Processions.Num() >= int32(GeT(TEXT("maxProcessions"), 16.f))) break;
				FProcession P;
				P.Road = r;
				P.S = S;
				P.Dir = (Processions.Num() % 2) ? 1.f : -1.f;
				Processions.Add(P);
			}
		}
		UE_LOG(LogWildreach, Log, TEXT("GRAVEYARD the Rising: %d procession slots"), Processions.Num());
	}
	if (!bRising && bWasRising) Dawn();
	bWasRising = bRising;
	if (!bRising) return;
	Clock += Dt;
	if (Clock < 0.5f) return;
	const float Step = Clock;
	Clock = 0.f;
	const double Now = World->GetTimeSeconds();
	for (FProcession& P : Processions)
	{
		P.Members.RemoveAll([](const TWeakObjectPtr<AWRCreature>& M) { return !M.IsValid() || !M->IsAlive(); });
		if (P.bSpawned && P.Members.Num() == 0)
		{
			// Destroyed: it forms again 6 minutes later.
			P.bSpawned = false;
			P.ReformAt = Now + GeT(TEXT("reformSeconds"), 360.f);
		}
		if (!P.bSpawned)
		{
			if (Now < P.ReformAt) continue;
			// Only where a survivor is near enough to meet it.
			const FVector At = RoadAt(Roads[P.Road], P.S);
			bool bNear = false;
			for (TActorIterator<AWRCharacter> It(World); It && !bNear; ++It) bNear = It->IsAlive() && FVector::Dist2D(It->GetActorLocation(), At) < GeT(TEXT("activeRange"), 40000.f);
			if (bNear) SpawnProcession(P);
			continue;
		}
		StepProcession(P, Step);
	}
}

void UWRGraveEvents::SpawnProcession(FProcession& P)
{
	UWorld* World = GetWorld();
	const FRoadLine& R = Roads[P.Road];
	const FVector At = GeGround(World, RoadAt(R, P.S));
	const FVector Ahead = RoadAt(R, P.S + 200.f * P.Dir);
	const FRotator Face(0.f, (Ahead - At).Rotation().Yaw, 0.f);
	const int32 N = FMath::RandRange(int32(GeT(TEXT("sizeMin"), 4.f)), int32(GeT(TEXT("sizeMax"), 8.f)));
	for (int32 i = 0; i < N; ++i)
	{
		// The lantern-bearer first (a mourner with its tōrō pole), then mourners, hounds, now and then a bearer.
		const float Roll = FMath::FRand();
		const FName Kind = i == 0 ? FName(TEXT("risen_mourner")) : Roll < 0.65f ? FName(TEXT("risen_mourner")) : Roll < 0.9f ? FName(TEXT("risen_hound")) : FName(TEXT("risen_bearer"));
		const FVector Spot = GeGround(World, At - Face.Vector() * 180.f * i);
		if (AWRCreature* C = AWRCreature::SpawnCreature(World, Kind, Spot + FVector(0.f, 0.f, 120.f), Face))
		{
			C->SetHome(Spot);
			P.Members.Add(C);
		}
	}
	P.bSpawned = P.Members.Num() > 0;
	if (P.bSpawned) WRFx::Broadcast(World, TEXT("grave.soil_heave"), At, FRotator::ZeroRotator, 2.f, TEXT("grave.bone_rattle"));
}

void UWRGraveEvents::StepProcession(FProcession& P, float Dt)
{
	UWorld* World = GetWorld();
	AWRCreature* Lead = P.Members.Num() ? P.Members[0].Get() : nullptr;
	if (!Lead) return;
	const FRoadLine& R = Roads[P.Road];
	FVector Goal;
	// The nearest light within 600 m draws it; a camp's light holds it at the safe zone's edge (a vigil).
	float LightR = 0.f;
	const AActor* Light = nullptr;
	if (UWRLightSubsystem* L = UWRLightSubsystem::Get(World)) Light = L->BrightestNear(Lead->GetActorLocation(), GeT(TEXT("lightReach"), 60000.f), &LightR);
	if (Light)
	{
		Goal = Light->GetActorLocation();
		if (FWRSocialRules::IsInSafeZone(World, Goal))
		{
			// Walk in from the procession's side until the edge of the safe ground, and stand there.
			const FVector From = Lead->GetActorLocation();
			FVector Edge = Goal;
			for (int32 k = 1; k <= 40; ++k)
			{
				const FVector Try = FMath::Lerp(Goal, From, k / 40.f);
				if (!FWRSocialRules::IsInSafeZone(World, Try)) { Edge = Try; break; }
			}
			Goal = Edge;
		}
	}
	else
	{
		// No light: they walk the road at a slow procession's pace, turning at its ends.
		P.S += P.Dir * GeT(TEXT("walkSpeed"), 200.f) * Dt;
		if (P.S <= 0.f || P.S >= R.Len) { P.Dir = -P.Dir; P.S = FMath::Clamp(P.S, 0.f, R.Len); }
		// The road leads; the bearer keeps to the road's line ahead of where it is.
		Goal = RoadAt(R, P.S);
		if (FVector::Dist2D(Lead->GetActorLocation(), Goal) > 1500.f) P.S = FMath::Clamp(P.S - P.Dir * GeT(TEXT("walkSpeed"), 200.f) * Dt, 0.f, R.Len);
	}
	Lead->SetHome(GeGround(World, Goal));
	// The rest follow in file behind the bearer.
	const FVector Back = -Lead->GetActorForwardVector().GetSafeNormal2D();
	for (int32 i = 1; i < P.Members.Num(); ++i)
	{
		if (AWRCreature* M = P.Members[i].Get()) M->SetHome(GeGround(World, Lead->GetActorLocation() + Back * 180.f * i));
	}
}

void UWRGraveEvents::Dawn()
{
	UWorld* World = GetWorld();
	int32 Piles = 0;
	// Every Risen of the processions falls where it stands into a bone pile (bone, and now and then soulstone).
	for (FProcession& P : Processions)
	{
		for (const TWeakObjectPtr<AWRCreature>& W : P.Members)
		{
			AWRCreature* C = W.Get();
			if (!C || !C->IsAlive()) continue;
			const FVector Feet = GeGround(World, C->GetActorLocation());
			WRFx::Broadcast(World, TEXT("grave.bone_collapse"), Feet, FRotator::ZeroRotator, 1.2f, TEXT("creature.risen.collapse"));
			const auto Node = [World, Feet](FName Item, int32 Hits, int32 PerHit, const FVector& Off)
			{
				const FTransform Tr(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), Feet + Off);
				if (AWRResourceNode* N = World->SpawnActorDeferred<AWRResourceNode>(AWRResourceNode::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
				{
					N->ItemId = Item;
					N->Hits = Hits;
					N->PerHit = PerHit;
					N->RespawnSeconds = 1e7f;   // gone once gathered
					N->FinishSpawning(Tr);
					N->SetLifeSpan(WRTune(TEXT("graveyardEvents.bloodMoon.pileSeconds"), 3600.f));
				}
			};
			Node(TEXT("bone"), 2, int32(GeT(TEXT("pileBone"), 4.f)) / 2, FVector::ZeroVector);
			if (FMath::FRand() < GeT(TEXT("pileSoulstoneChance"), 0.5f)) Node(TEXT("soulstone"), 1, FMath::RandRange(1, int32(GeT(TEXT("pileSoulstoneMax"), 3.f))), FVector(40.f, 0.f, 0.f));
			C->Destroy();
			++Piles;
		}
	}
	Processions.Reset();
	GeTellLand(World, TEXT("Dawn: the dead fall where they stand. The Mourning hangs over the graves"));
	UE_LOG(LogWildreach, Log, TEXT("GRAVEYARD dawn after the Rising: %d bone piles"), Piles);
}
