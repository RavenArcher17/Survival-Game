// The Ocean's hazards and the King Tide. See WROceanHazards.h; cooks/biome-ocean.md 2.8-2.10. Numbers: tuning
// oceanHazards and tide.kingTide.

#include "WROceanHazards.h"
#include "Wildreach.h"
#include "WRBuilding.h"
#include "WRCharacter.h"
#include "WRCombatLibrary.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRGameState.h"
#include "WRInventory.h"
#include "WRLand.h"
#include "WRLifeSkills.h"
#include "WROceanCreatures.h"
#include "WROceanTypes.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRResourceNode.h"
#include "WRSkyAir.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRSurvival.h"
#include "WRTide.h"
#include "WRToolMesh.h"
#include "WRTuning.h"
#include "WRUnderwater.h"
#include "WRWorldSystems.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "ProceduralMeshComponent.h"

namespace WROceanHzImpl
{
	float HzTn(const TCHAR* Key, float Fallback) { return FWRTuning::Get().Num(FString::Printf(TEXT("oceanHazards.%s"), Key), Fallback); }
	float KtTn(const TCHAR* Key, float Fallback) { return FWRTuning::Get().Num(FString::Printf(TEXT("tide.kingTide.%s"), Key), Fallback); }
	float HzHash(int32 I, int32 Salt) { return FMath::Frac(FMath::Sin(float(I) * 12.9898f + float(Salt) * 78.233f) * 43758.5453f); }
	void HzTell(const AActor* Who, const FString& Text)
	{
		const APawn* P = Cast<APawn>(Who);
		if (AWRPlayerController* PC = P ? Cast<AWRPlayerController>(P->GetController()) : nullptr) PC->ClientNotify(Text);
	}
	void HzQuest(const AActor* Pawn, FName Type, FName What, int32 Count = 1)
	{
		const APawn* P = Cast<APawn>(Pawn);
		AWRPlayerState* PS = P ? P->GetPlayerState<AWRPlayerState>() : nullptr;
		UWRWorldSystems* WS = Pawn ? Pawn->GetWorld()->GetSubsystem<UWRWorldSystems>() : nullptr;
		if (PS && WS) WS->QuestEvent(PS, Type, What, Count);
	}
	FString HzName(const AActor* Who)
	{
		const APawn* P = Cast<APawn>(Who);
		return P && P->GetPlayerState() ? P->GetPlayerState()->GetPlayerName() : FString();
	}
	void HzHit(AWRCharacterBase* V, float Damage, const FVector& Origin, const TMap<FName, float>& Buildups = {})
	{
		FWRHit Hit;
		Hit.Instigator = nullptr;
		Hit.Damage = Damage;
		Hit.bUnblockable = true;
		Hit.Origin = Origin;
		Hit.Element = WRTags::Element_Physical;
		for (const TPair<FName, float>& B : Buildups) Hit.Buildups.Add(B.Key, B.Value);
		UWRCombatLibrary::ApplyHit(V, Hit);
	}
	bool HzLive(const UWorld* W) { return W && WROcean::Live(); }
	/** Every survivor and creature alive in the world (the hazards don't care which). */
	template <typename F> void HzEachBody(UWorld* W, F&& Fn)
	{
		for (TActorIterator<AWRCharacterBase> It(W); It; ++It) if (It->IsAlive()) Fn(*It);
	}
}
using namespace WROceanHzImpl;

UWROceanHazards* UWROceanHazards::Get(const UObject* WorldContext)
{
	const UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	return W ? W->GetSubsystem<UWROceanHazards>() : nullptr;
}

bool UWROceanHazards::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* W = Cast<UWorld>(Outer);
	return W && W->IsGameWorld();
}

TStatId UWROceanHazards::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWROceanHazards, STATGROUP_Tickables);
}

bool UWROceanHazards::IsTickable() const
{
	return HzLive(GetWorld());
}

double UWROceanHazards::ServerNow() const
{
	const UWorld* W = GetWorld();
	return W && W->GetGameState() ? W->GetGameState()->GetServerWorldTimeSeconds() : 0.0;
}

float UWROceanHazards::TsunamiLineZ()
{
	return WRTide::MeanZ() + KtTn(TEXT("lineContour"), 600.f);
}

// ================================================================================================ rips (every machine)

bool UWROceanHazards::InRip(const UObject* WorldContext, const FVector& P, int32& OutIndex, float& OutAlong, float& OutAcross)
{
	const FWROceanData& D = FWROceanData::Get();
	for (int32 i = 0; i < D.Rips.Num(); ++i)
	{
		const FWROceanData::FRip& R = D.Rips[i];
		// Each runs straight out to sea (+X) from its bar gap.
		const float Along = P.X - R.At.X;
		const float Across = P.Y - R.At.Y;
		if (Along < 0.f || Along > R.Length || FMath::Abs(Across) > R.Width * 0.5f) continue;
		OutIndex = i;
		OutAlong = Along;
		OutAcross = Across;
		return true;
	}
	return false;
}

FVector UWROceanHazards::CurrentAt(const UObject* WorldContext, const FVector& P)
{
	if (!WorldContext || !WROcean::Live() || !WROcean::Contains(P)) return FVector::ZeroVector;
	int32 I = 0;
	float Along = 0.f, Across = 0.f;
	if (!InRip(WorldContext, P, I, Along, Across)) return FVector::ZeroVector;
	// Only in the water (the surface over you); the King Tide's Low drains them dry.
	if (P.Z > WRTide::SurfaceAt(WorldContext, P) + 50.f) return FVector::ZeroVector;
	const FWROceanData::FRip& R = FWROceanData::Get().Rips[I];
	const float Speed = WRTide::Rising(WorldContext) ? HzTn(TEXT("rip.rising"), 170.f) : HzTn(TEXT("rip.falling"), 250.f);
	// Full in the lane, easing over its last fifth out past the bar and over its edges' last metre.
	const float AlongK = FMath::Clamp((R.Length - Along) / (R.Length * 0.2f), 0.f, 1.f);
	const float EdgeK = FMath::Clamp((R.Width * 0.5f - FMath::Abs(Across)) / 100.f, 0.f, 1.f);
	return FVector(1.f, 0.f, 0.f) * Speed * AlongK * EdgeK;
}

float UWROceanHazards::RipStaminaMult(const AActor* Who)
{
	const ACharacter* C = Cast<ACharacter>(Who);
	if (!C || !C->GetCharacterMovement() || !C->GetCharacterMovement()->IsSwimming()) return 1.f;
	const FVector Cur = CurrentAt(Who, C->GetActorLocation());
	if (Cur.IsNearlyZero()) return 1.f;
	// Straight against it (heading within 45 degrees of the shore): twice the stamina.
	const FVector V = C->GetCharacterMovement()->GetCurrentAcceleration().GetSafeNormal2D();
	return FVector::DotProduct(V, Cur.GetSafeNormal()) < -0.7f ? HzTn(TEXT("rip.againstStamina"), 2.f) : 1.f;
}

// ================================================================================================ setup

void UWROceanHazards::Setup()
{
	bSetup = true;
	UWorld* World = GetWorld();
	const FWROceanData& D = FWROceanData::Get();
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// The air pockets: an air volume over the water inside them (every machine: "under" reads it).
	for (const FWROceanData::FPocket& P : D.Pockets)
	{
		AWRTideWater* Air = World->SpawnActor<AWRTideWater>(AWRTideWater::StaticClass(), FTransform(P.At), SP);
		if (!Air) continue;
		Air->Shape(FBox2D(FVector2D(P.At) - FVector2D(P.R, P.R), FVector2D(P.At) + FVector2D(P.R, P.R)), P.At.Z, FMath::Max(P.Top, P.At.Z + 200.f), false, int32(HzTn(TEXT("pocketPriority"), 6.f)));
		Pockets.Add(Air);
	}
	// The man o' war: each fleet's members round its centre.
	for (int32 f = 0; f < D.Fleets.Num(); ++f)
	{
		for (int32 m = 0; m < D.Fleets[f].Count; ++m)
		{
			FMan M;
			M.Base = D.Fleets[f].At;
			const float A = HzHash(m, f * 31 + 1) * 2.f * PI;
			const float R = FMath::Sqrt(HzHash(m, f * 31 + 2)) * HzTn(TEXT("manowar.spread"), 4000.f);
			M.Offset = FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 0.f);
			M.Phase = HzHash(m, f * 31 + 3) * 2.f * PI;
			M.Fleet = f;
			Men.Add(M);
		}
	}
	BlowNext.Init(-1.0, D.Blowholes.Num());
	BlowTold.Init(false, D.Blowholes.Num());
	SurgeStart.Init(-1.0, D.Surges.Num());
	SurgeNext.Init(-1.0, D.Surges.Num());
	SurgeHit.SetNum(D.Surges.Num());
	if (World->GetNetMode() == NM_DedicatedServer) return;
	// Every drawing machine: the rips' tells (foam and kelp drifting out along the lane, the hiss) and the fleets.
	for (const FWROceanData::FRip& R : D.Rips)
	{
		AActor* Look = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(FVector(R.At.X + R.Length * 0.5f, R.At.Y, WRTide::MeanZ())), SP);
		if (!Look) continue;
		USceneComponent* Root = NewObject<USceneComponent>(Look, TEXT("Root"));
		Look->SetRootComponent(Root);
		Root->RegisterComponent();
		Look->SetActorLocation(FVector(R.At.X + R.Length * 0.5f, R.At.Y, WRTide::MeanZ()));
		if (USceneComponent* Fx = WRFx::Attach(Root, TEXT("ocean.rip_foam"), NAME_None, R.Length / 6000.f)) Fx->SetRelativeLocation(FVector::ZeroVector);
		RipLooks.Add(Look);
	}
	for (int32 i = 0; i < Men.Num(); ++i)
	{
		AActor* Look = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(Men[i].Base), SP);
		if (!Look) continue;
		UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(Look, TEXT("Sail"));
		Look->SetRootComponent(Mesh);
		Mesh->RegisterComponent();
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCastShadow(false);
		// A blue-violet gas float 30 cm tall with its crest (the sail), pink-violet tentacles trailing 8 m down.
		FWRToolMesh M;
		const FLinearColor Float = FLinearColor(FColor(0x6E, 0x7C, 0xE6)) * 0.7f, Crest = FLinearColor(FColor(0xB0, 0x6E, 0xE0)) * 0.7f, Tent = FLinearColor(FColor(0x8A, 0x4A, 0xB8)) * 0.6f;
		M.Lathe(FVector(-18.f, 0, 2.f), FVector(1, 0, 0), 36.f, { FVector(0.f, 3.f, 3.f), FVector(0.3f, 9.f, 8.f), FVector(0.7f, 8.f, 7.f), FVector(1.f, 2.f, 2.f) }, 10, Float, 0.15f);
		const FColor P = FWRToolMesh::Pack(Crest, 0.2f);
		for (int32 s = -1; s <= 1; s += 2) M.Tri(FVector(-14.f, s * 0.5f, 8.f), FVector(14.f, s * 0.5f, 8.f), FVector(-4.f, s * 0.5f, 30.f), P, FVector2D(0, 0), FVector2D(1, 0), FVector2D(0.5f, 1));
		FRandomStream Rng(i * 7 + 3);
		for (int32 t = 0; t < 7; ++t)
		{
			TArray<FVector> Line;
			const FVector Root(Rng.FRandRange(-10.f, 10.f), Rng.FRandRange(-6.f, 6.f), -2.f);
			const float Len = Rng.FRandRange(300.f, 800.f);
			for (int32 k = 0; k <= 10; ++k) { const float A = k / 10.f; Line.Add(Root + FVector(FMath::Sin(A * 6.f + t) * 30.f * A, FMath::Cos(A * 5.f + t) * 25.f * A, -Len * A)); }
			M.Tube(Line, t == 0 ? 2.f : 1.f, 4, Tent, 0.3f);
		}
		M.ApplyTo(Mesh);
		ManLooks.Add(Look);
	}
}

FVector UWROceanHazards::ManAt(int32 Index, double T) const
{
	if (!Men.IsValidIndex(Index)) return FVector::ZeroVector;
	const FMan& M = Men[Index];
	// The fleet drifts a slow loop with the wind (20 minutes round, 60 m across), each sail bobbing on its own.
	const float W = 2.f * PI / FMath::Max(60.f, HzTn(TEXT("manowar.loopSeconds"), 1200.f));
	const float R = HzTn(TEXT("manowar.loop"), 6000.f);
	FVector At = M.Base + M.Offset + FVector(FMath::Cos(float(T) * W + M.Fleet) * R, FMath::Sin(float(T) * W * 0.7f + M.Fleet) * R, 0.f);
	// A Squall blows one sail of each fleet inshore onto the strand (it still stings there).
	const FWRWeatherKind* K = UWRWeatherSubsystem::ActiveKind(this, WROcean::Biome());
	if (K && K->Id == TEXT("squall") && Index % 6 == 0)
	{
		const FWROceanData& D = FWROceanData::Get();
		if (D.Rips.Num() > 0)
		{
			const FWROceanData::FRip& Rip = D.Rips[(Index / 6) % D.Rips.Num()];
			At = FVector(Rip.At.X - 600.f + M.Offset.X * 0.05f, Rip.At.Y + 1500.f + M.Offset.Y * 0.1f, 0.f);
		}
	}
	const float Surface = WRTide::SurfaceAt(this, At);
	const float Ground = WRLand::GroundZAt(At, Surface - 1000.f);
	At.Z = FMath::Max(Surface, Ground + 10.f) + FMath::Sin(float(T) * 1.3f + M.Phase) * 12.f;
	return At;
}

// ================================================================================================ tick

void UWROceanHazards::Tick(float DeltaTime)
{
	if (!bSetup) Setup();
	UWorld* World = GetWorld();
	if (World->GetNetMode() != NM_Client)
	{
		StepKingTide(DeltaTime);
		StepRips(DeltaTime);
		StepBlowholes();
		StepSurges();
		FleetClock += DeltaTime;
		if (FleetClock >= 0.25f) { FleetClock = 0.f; StepFleets(); }
	}
	if (World->GetNetMode() != NM_DedicatedServer) { StepFleetLooks(DeltaTime); StepSalvager(DeltaTime); }
}

void UWROceanHazards::StepRips(float Dt)
{
	RipClock += Dt;
	if (RipClock < 0.25f) return;
	RipClock = 0.f;
	// Hae-won's "Sideways": caught in a rip and out of it by its side (not out past the bar, not back up the beach).
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APawn* P = It->Get() ? It->Get()->GetPawn() : nullptr;
		ACharacter* C = Cast<ACharacter>(P);
		if (!C) continue;
		int32 I = 0;
		float Along = 0.f, Across = 0.f;
		const bool bIn = C->GetCharacterMovement()->IsSwimming() && !CurrentAt(C, C->GetActorLocation()).IsNearlyZero() && InRip(C, C->GetActorLocation(), I, Along, Across);
		FRipTrack* Was = InRips.Find(C);
		if (bIn)
		{
			if (!Was) HzTell(C, TEXT("The water's pulling you out to sea. Don't fight it: swim along the shore"));
			InRips.Add(C, { I, Along, Across });
			continue;
		}
		if (!Was) continue;
		const FWROceanData& D = FWROceanData::Get();
		if (D.Rips.IsValidIndex(Was->Index))
		{
			const FWROceanData::FRip& R = D.Rips[Was->Index];
			const float NowAlong = C->GetActorLocation().X - R.At.X;
			const float NowAcross = C->GetActorLocation().Y - R.At.Y;
			if (NowAlong > 0.f && NowAlong < R.Length && FMath::Abs(NowAcross) >= R.Width * 0.5f)
			{
				HzQuest(C, TEXT("event"), TEXT("rip_escape"));
				HzTell(C, TEXT("Out of the rip, sideways. That's the way."));
			}
		}
		InRips.Remove(C);
	}
}

void UWROceanHazards::StepBlowholes()
{
	const FWROceanData& D = FWROceanData::Get();
	const double T = ServerNow();
	UWorld* World = GetWorld();
	const AWROceanState* S = AWROceanState::Get(this);
	// Live from mid-rising to high water (never in the King Tide's Low: the sea is far below them).
	const float Full = WRTide::Fullness(this);
	const bool bLive = (WRTide::Rising(this) && Full > HzTn(TEXT("blowhole.risingFrom"), 0.5f)) || Full > HzTn(TEXT("blowhole.highFrom"), 0.85f);
	const bool bKingLow = S && S->KingTide != EWRKingTide::None;
	for (int32 i = 0; i < D.Blowholes.Num(); ++i)
	{
		const FWROceanData::FBlowhole& B = D.Blowholes[i];
		if (!bLive || bKingLow) { BlowNext[i] = -1.0; BlowTold[i] = false; continue; }
		const float Every = FMath::Lerp(HzTn(TEXT("blowhole.everyMin"), 9.f), HzTn(TEXT("blowhole.everyMax"), 16.f), HzHash(i, 7));
		if (BlowNext[i] < 0.0) { BlowNext[i] = T + Every * HzHash(i, 9) + 1.0; BlowTold[i] = false; }
		const float TellSecs = HzTn(TEXT("blowhole.tell"), 1.0f);
		// Nobody within 120 m: it still breathes, unseen (no effects sent).
		bool bNear = false;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const APawn* P = It->Get() ? It->Get()->GetPawn() : nullptr;
			if (P && FVector::Dist(P->GetActorLocation(), B.At) < HzTn(TEXT("blowhole.near"), 12000.f)) bNear = true;
		}
		if (!BlowTold[i] && T >= BlowNext[i] - TellSecs)
		{
			BlowTold[i] = true;
			// The tell: a rising boom and hiss, spray puffing from the vent, the rock shaking underfoot.
			if (bNear) WRFx::Broadcast(World, TEXT("ocean.blowhole_tell"), B.At, FRotator::ZeroRotator, B.bHorn ? 2.f : 1.f, B.bHorn ? FName(TEXT("ocean.horn_boom")) : FName(TEXT("ocean.blowhole_boom")), TellSecs);
		}
		if (T < BlowNext[i]) continue;
		BlowNext[i] = T + Every;
		BlowTold[i] = false;
		if (!bNear) continue;
		const float H = B.bHorn ? HzTn(TEXT("blowhole.hornHeight"), 4000.f) : HzTn(TEXT("blowhole.height"), 600.f);
		WRFx::Broadcast(World, B.bHorn ? FName(TEXT("ocean.horn_column")) : FName(TEXT("ocean.blowhole_column")), B.At, FRotator::ZeroRotator, H / 600.f, TEXT("ocean.blowhole_burst"));
		const float R = B.bHorn ? HzTn(TEXT("blowhole.hornRadius"), 400.f) : HzTn(TEXT("blowhole.radius"), 200.f);
		HzEachBody(World, [&](AWRCharacterBase* V)
		{
			const FVector Feet = V->GetActorLocation() - FVector(0, 0, V->GetSimpleCollisionHalfHeight());
			if (FVector::Dist2D(Feet, B.At) > R + V->GetSimpleCollisionRadius() || Feet.Z < B.At.Z - 150.f || Feet.Z > B.At.Z + H) return;
			if (UWROceanBody::Get(V)) return;   // the sea's own know the vents
			// Jumped just before it (in the air going up, or just off the rock): ride it unhurt.
			UCharacterMovementComponent* M = V->GetCharacterMovement();
			const bool bRide = M && M->IsFalling() && Feet.Z - B.At.Z < 200.f;
			const float Up = B.bHorn ? HzTn(TEXT("blowhole.hornLaunch"), 1800.f) : HzTn(TEXT("blowhole.launch"), 600.f);
			FVector Vel(0.f, 0.f, FMath::Sqrt(2.f * 980.f * Up));
			if (B.bHorn && !B.Ledge.IsZero())
			{
				// The Horn's ride carries you over onto the ledge (O-S3): the throw aimed so its top is over it.
				const float Rise = Vel.Z / 980.f;
				const FVector2D Over = FVector2D(B.Ledge - B.At);
				const float Fall = FMath::Sqrt(FMath::Max(0.f, 2.f * (Up - (B.Ledge.Z - B.At.Z)) / 980.f));
				Vel += FVector(Over / FMath::Max(0.5f, Rise + Fall), 0.f);
			}
			if (!bRide) HzHit(V, HzTn(TEXT("blowhole.damage"), 40.f), B.At);
			V->LaunchCharacter(Vel, true, true);
			if (bRide) HzTell(V, B.bHorn ? TEXT("The Horn throws you high over the shelf") : TEXT("You ride the spout"));
		});
	}
}

void UWROceanHazards::StepSurges()
{
	const FWROceanData& D = FWROceanData::Get();
	const double T = ServerNow();
	UWorld* World = GetWorld();
	const AWROceanState* S = AWROceanState::Get(this);
	const bool bLive = WRTide::Fullness(this) > HzTn(TEXT("surge.from"), 0.4f) && !(S && S->KingTide != EWRKingTide::None);
	const float Speed = HzTn(TEXT("surge.speed"), 600.f);
	for (int32 i = 0; i < D.Surges.Num(); ++i)
	{
		const FWROceanData::FSurge& C = D.Surges[i];
		const FVector Dir = FRotator(0.f, C.Yaw, 0.f).Vector();
		if (!bLive) { SurgeStart[i] = -1.0; SurgeNext[i] = -1.0; continue; }
		const float Every = HzTn(TEXT("surge.every"), 14.f) + 4.f * HzHash(i, 13);
		if (SurgeNext[i] < 0.0) SurgeNext[i] = T + Every * HzHash(i, 17) + 1.0;
		if (SurgeStart[i] < 0.0 && T >= SurgeNext[i] - 1.0)
		{
			// The tell: the channel's mouth heaves white and a low roar rolls up it (1 s).
			WRFx::Broadcast(World, TEXT("ocean.surge_tell"), C.At, Dir.Rotation(), 1.f, TEXT("ocean.surge"), 1.f);
			SurgeStart[i] = SurgeNext[i];
			SurgeNext[i] = T + 1.0 + Every;
			SurgeHit[i].Reset();
		}
		if (SurgeStart[i] < 0.0 || T < SurgeStart[i]) continue;
		const float Run = float(T - SurgeStart[i]) * Speed;
		if (Run > C.Length) { SurgeStart[i] = -1.0; continue; }
		if (FMath::Fmod(float(T - SurgeStart[i]), 0.5f) < 0.06f) WRFx::Broadcast(World, TEXT("ocean.surge_wave"), C.At + Dir * Run, Dir.Rotation(), 1.f, NAME_None);
		const FVector Front = C.At + Dir * Run;
		HzEachBody(World, [&](AWRCharacterBase* V)
		{
			if (UWROceanBody::Get(V) || SurgeHit[i].Contains(V)) return;
			const FVector Rel = V->GetActorLocation() - Front;
			const float Along = FVector::DotProduct(Rel, Dir);
			const float Across = FVector::CrossProduct(Dir, Rel).Z;
			if (FMath::Abs(Along) > 200.f || FMath::Abs(Across) > HzTn(TEXT("surge.halfWidth"), 300.f) || FMath::Abs(Rel.Z) > 400.f) return;
			SurgeHit[i].Add(V);
			HzHit(V, HzTn(TEXT("surge.damage"), 30.f), Front);
			if (V->Status) V->Status->Stagger(HzTn(TEXT("surge.knockdown"), 1.0f));
			// Dragged 6 m back down the channel, out to sea.
			WRSkyAir::Push(V, -Dir * HzTn(TEXT("surge.drag"), 600.f) / 0.5f, 0.5f, TEXT("surge"), nullptr);
		});
	}
}

void UWROceanHazards::StepFleets()
{
	const double T = ServerNow();
	UWorld* World = GetWorld();
	const float Reach = HzTn(TEXT("manowar.reach"), 120.f), Down = HzTn(TEXT("manowar.tentacles"), 800.f);
	for (int32 i = 0; i < Men.Num(); ++i)
	{
		const FVector M = ManAt(i, T);
		bool bNear = false;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const APawn* P = It->Get() ? It->Get()->GetPawn() : nullptr;
			if (P && FVector::Dist2D(P->GetActorLocation(), M) < 3000.f) bNear = true;
		}
		if (!bNear) continue;
		HzEachBody(World, [&](AWRCharacterBase* V)
		{
			if (UWROceanBody::Get(V)) return;
			const FVector P = V->GetActorLocation();
			if (FVector::Dist2D(P, M) > Reach + V->GetSimpleCollisionRadius() || P.Z > M.Z + 150.f || P.Z < M.Z - Down) return;
			double& Last = StungAt.FindOrAdd(V);
			if (T - Last < 1.0) return;
			Last = T;
			// A swim mount's rider or Reefmail 2: half.
			float K = 1.f;
			if (const AWRCreature* Mount = Cast<AWRCreature>(V->GetAttachParentActor()); Mount && Mount->GetRider() == V) K = HzTn(TEXT("manowar.mountMult"), 0.5f);
			if (const UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(V); Inv && Inv->SetPieces(TEXT("reefmail")) >= 2) K = FMath::Min(K, HzTn(TEXT("manowar.reefmailMult"), 0.5f));
			HzHit(V, HzTn(TEXT("manowar.damage"), 25.f) * K, M, { { TEXT("poison"), HzTn(TEXT("manowar.poison"), 50.f) * K } });
			WRFx::Broadcast(V, TEXT("ocean.manowar_sting"), P, FRotator::ZeroRotator, 1.f, TEXT("ocean.manowar_sting"));
		});
	}
}

void UWROceanHazards::StepFleetLooks(float Dt)
{
	const double T = ServerNow();
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	for (int32 i = 0; i < ManLooks.Num() && i < Men.Num(); ++i)
	{
		AActor* L = ManLooks[i].Get();
		if (!L) continue;
		const FVector At = ManAt(i, T);
		// Far ones aren't drawn (a few hundred metres round you).
		const bool bShow = !Me || FVector::Dist2D(Me->GetActorLocation(), At) < HzTn(TEXT("manowar.drawReach"), 40000.f);
		L->SetActorHiddenInGame(!bShow);
		if (!bShow) continue;
		L->SetActorLocationAndRotation(At, FRotator(FMath::Sin(float(T) * 1.1f + i) * 8.f, float(T) * 3.f + i * 40.f, FMath::Sin(float(T) * 0.9f + i) * 10.f));
		// The glassy tick within 6 m.
		if (Me && FVector::Dist(Me->GetActorLocation(), At) < 600.f && FMath::Fmod(float(T) + i * 0.37f, 2.5f) < Dt) WRSound::PlayAt(Me, TEXT("ocean.manowar_tick"), At);
	}
}

// ================================================================================================ the King Tide (server)

float UWROceanHazards::TameSpeedMult(const AWRCreature* Creature)
{
	const UWROceanBody* B = UWROceanBody::Get(Creature);
	if (!B) return 1.f;
	if (B->bStrandedFlat) return KtTn(TEXT("strandedTameMult"), 3.f);
	const AWROceanState* S = AWROceanState::Get(Creature);
	if (S && S->bSpawningNight && Creature->CreatureId == TEXT("glowray")) return KtTn(TEXT("spawningTameMult"), 2.f);
	return 1.f;
}

void UWROceanHazards::StepKingTide(float Dt)
{
	AWROceanState* S = AWROceanState::Get(this);
	if (!S) return;
	UWorld* World = GetWorld();
	const AWRWorldState* WS = AWRWorldState::Get(World);
	const FWRWorldEventRep* E = WS ? WS->Event(TEXT("king_tide")) : nullptr;
	const double T = ServerNow();
	uint8 Want = 0;
	if (E)
	{
		const double Bore = E->EndAt - KtTn(TEXT("boreMinutes"), 3.f) * 60.0;
		const double Line = E->EndAt - KtTn(TEXT("returnMinutes"), 10.f) * 60.0;
		if (E->Stage == 0 && T < E->StartAt) Want = 1;
		else if (E->Stage == 1 && T < E->EndAt) Want = T >= Bore ? 4 : T >= Line ? 3 : 2;
		if (Want == 1 && S->KingTideStart < 0.0) S->KingTideStart = E->StartAt - KtTn(TEXT("warnSeconds"), 300.f);
		if (Want >= 2 && S->KingTideStart < 0.0) S->KingTideStart = E->StartAt - KtTn(TEXT("warnSeconds"), 300.f);
		if (Want == 4) S->BoreStart = Bore;
	}
	if (Want == KingStage)
	{
		if (KingStage == 4) BoreHit(T);
		return;
	}
	const uint8 Was = KingStage;
	KingStage = Want;
	if (Want == 0) { EndKingTide(); return; }
	S->KingTide = Want == 1 ? EWRKingTide::Warning : Want == 2 ? EWRKingTide::Low : EWRKingTide::Return;
	const FWROceanData& D = FWROceanData::Get();
	if (Want == 1)
	{
		// The warning: the sea draws back, every chime beacon rings with no wind, the drowned bell rings under the reef.
		S->BellRungAt = T;
		for (const FWROceanData::FPlace& K : D.Kit) if (K.Kind == TEXT("chime_beacon")) WRFx::Broadcast(World, TEXT("ocean.chime_ring"), K.At + FVector(0, 0, 400.f), FRotator::ZeroRotator, 1.f, TEXT("ocean.chimes_all"));
		for (const FWROceanData::FPlace& K : D.Sites) if (K.Kind == TEXT("bell_tower") || K.Id == TEXT("bell_tower")) WRFx::Broadcast(World, TEXT("ocean.drowned_bell"), K.At, FRotator::ZeroRotator, 2.f, TEXT("ocean.drowned_bell"));
		for (const FWROceanData::FPlace& K : D.Landmarks) if (K.Kind == TEXT("bell_tower")) WRFx::Broadcast(World, TEXT("ocean.drowned_bell"), K.At, FRotator::ZeroRotator, 2.f, TEXT("ocean.drowned_bell"));
		// The flocks lift inland: the Tidefowl and Gullraiders leave the shore.
		for (const TWeakObjectPtr<UWROceanBody>& W : UWROceanBody::Live)
		{
			AWRCreature* C = W.IsValid() ? Cast<AWRCreature>(W->GetOwner()) : nullptr;
			if (!C || C->IsTamed() || !C->IsAlive()) continue;
			if (C->CreatureId == TEXT("tidefowl") || C->CreatureId == TEXT("gullraider")) { C->SetHome(C->GetActorLocation() - FVector(8000.f, 0, 0)); W->Home = C->GetHome(); }
		}
		UE_LOG(LogWildreach, Log, TEXT("OCEAN the King Tide's warning: the sea draws back"));
	}
	if (Want >= 2 && Was < 2) StartLow();
	if (Want == 3)
	{
		// T-10 min: a white line of breaking water forms out past the reef and a roar builds.
		const float ReefX = HzTn(TEXT("kingTide.lineX"), 300000.f);
		for (int32 k = 0; k < 8; ++k) WRFx::Broadcast(World, TEXT("ocean.bore_line"), FVector(ReefX, FMath::Lerp(-790000.f, -580000.f, k / 7.f), WRTide::MeanZ() - KtTn(TEXT("drop"), 600.f)), FRotator(0.f, 180.f, 0.f), 4.f, k == 0 ? FName(TEXT("ocean.bore_roar")) : NAME_None, 420.f);
		UE_LOG(LogWildreach, Log, TEXT("OCEAN the King Tide's Return: the white line"));
	}
	if (Want == 4)
	{
		BoreHitSet.Reset();
		PiecesHit.Reset();
		for (int32 k = 0; k < 4; ++k) WRFx::Broadcast(World, NAME_None, FVector(HzTn(TEXT("kingTide.lineX"), 300000.f) - 60000.f, FMath::Lerp(-780000.f, -600000.f, k / 3.f), WRTide::MeanZ()), FRotator::ZeroRotator, 1.f, TEXT("ocean.bore_roar"));
		UE_LOG(LogWildreach, Log, TEXT("OCEAN the bore is coming in"));
	}
}

void UWROceanHazards::StartLow()
{
	UWorld* World = GetWorld();
	AWROceanState* S = AWROceanState::Get(this);
	UWROceanLife* Life = UWROceanLife::Get(this);
	const FWROceanData& D = FWROceanData::Get();
	// The spawning night: if the Low falls between 20:00 and 06:00 the reef spawns: every tidecoral regrows at once.
	const float H = AWRGameState::HourIn(this);
	S->bSpawningNight = H >= 20.f || H < 6.f;
	if (S->bSpawningNight)
	{
		for (TActorIterator<AWRResourceNode> It(World); It; ++It)
		{
			if (It->ItemId == TEXT("tidecoral") && It->IsDepleted()) It->RegrowNow();
		}
		for (int32 k = 0; k < 10; ++k) WRFx::Broadcast(World, TEXT("ocean.spawning"), FVector(FMath::FRandRange(220000.f, 300000.f), FMath::FRandRange(-780000.f, -600000.f), WRTide::MeanZ() - 400.f), FRotator::ZeroRotator, 3.f, NAME_None, 600.f);
	}
	if (!Life) return;
	// Stranded Kelpgrazers and Driftbacks on the flats; pool-bound Reefjaws in the deep pools left behind; crabs swarm.
	auto Flat = [&](bool bWet, FVector& Out) -> bool
	{
		for (int32 Try = 0; Try < 40; ++Try)
		{
			const FVector P(FMath::FRandRange(D.CoastX(-700000.f) + 500.f, 230000.f), FMath::FRandRange(-780000.f, -580000.f), 0.f);
			if (WROcean::ZoneAt(P) != TEXT("O4")) continue;
			const float Surface = WRTide::SurfaceAt(this, P);
			const float Ground = WRLand::GroundZAt(P, Surface - 10000.f);
			// After the drop: dry flat (the ground over the water) or a pool (water 3 m deep or more still).
			if (bWet ? Surface - Ground < 300.f : Ground < Surface + 20.f) continue;
			Out = FVector(P.X, P.Y, bWet ? Ground + 150.f : Ground + 60.f);
			return true;
		}
		return false;
	};
	const int32 Stranded = FMath::RandRange(3, 5), Pools = FMath::RandRange(4, 6), Crabs = FMath::RandRange(6, 9);
	for (int32 n = 0; n < Stranded; ++n)
	{
		FVector P;
		if (!Flat(false, P)) continue;
		if (AWRCreature* C = Life->SpawnOne(n % 2 ? TEXT("driftback") : TEXT("kelpgrazer"), P + FVector(0, 0, 150.f), TEXT("O4")))
		{
			if (UWROceanBody* B = UWROceanBody::Get(C)) { B->bStrandedFlat = true; B->Set(WROceanFlag::Stranded, true); B->StrandedUntil = 1.0e12; }
			KingSpawned.Add(C);
		}
	}
	for (int32 n = 0; n < Pools; ++n)
	{
		FVector P;
		if (!Flat(true, P)) continue;
		if (AWRCreature* C = Life->SpawnOne(TEXT("reefjaw"), P, TEXT("O4")))
		{
			if (UWROceanBody* B = UWROceanBody::Get(C)) { B->bPoolBound = true; B->CruiseDepth = 150.f; }
			C->SetHome(P);
			KingSpawned.Add(C);
		}
	}
	for (int32 n = 0; n < Crabs; ++n)
	{
		FVector P;
		if (!Flat(false, P)) continue;
		if (AWRCreature* C = Life->SpawnOne(TEXT("spinecrab"), P + FVector(0, 0, 80.f), TEXT("O4"))) KingSpawned.Add(C);
	}
	UE_LOG(LogWildreach, Log, TEXT("OCEAN the King Tide's Low (%s): %d stranded, %d pool Reefjaws, %d crabs"), S->bSpawningNight ? TEXT("a spawning night") : TEXT("day"), Stranded, Pools, Crabs);
}

void UWROceanHazards::BoreHit(double T)
{
	AWROceanState* S = AWROceanState::Get(this);
	if (!S || S->BoreStart < 0.0) return;
	UWorld* World = GetWorld();
	// The bore's front: from the reef toward the land at 600 cm/s, 3 m high, to the Tsunami Line and no further.
	const float Speed = KtTn(TEXT("boreSpeed"), 600.f);
	const float StartX = HzTn(TEXT("kingTide.lineX"), 300000.f);
	const float FrontX = StartX - float(T - S->BoreStart) * Speed;
	const float Line = TsunamiLineZ();
	if (FMath::Fmod(float(T - S->BoreStart), 1.f) < 0.06f)
	{
		for (int32 k = 0; k < 6; ++k) WRFx::Broadcast(World, TEXT("ocean.bore"), FVector(FrontX, FMath::Lerp(-790000.f, -580000.f, (k + 0.5f) / 6.f), WRTide::MeanZ()), FRotator(0.f, 180.f, 0.f), 4.f, NAME_None);
	}
	HzEachBody(World, [&](AWRCharacterBase* V)
	{
		const FVector P = V->GetActorLocation();
		if (!WROcean::Contains(P) || UWROceanBody::Get(V)) return;
		const float Feet = P.Z - V->GetSimpleCollisionHalfHeight();
		// Above the line: safe (Hae-won's fifth asks you to stand there when it comes).
		if (Feet >= Line)
		{
			if (V->GetPlayerState() && FMath::Abs(P.X - FrontX) < 6000.f && !LineStood.Contains(HzName(V)))
			{
				LineStood.Add(HzName(V));
				HzQuest(V, TEXT("event"), TEXT("stones_say"));
				HzTell(V, TEXT("The sea comes in under you and stops at the stones"));
			}
			return;
		}
		if (FMath::Abs(P.X - FrontX) > 300.f || BoreHitSet.Contains(V)) return;
		BoreHitSet.Add(V);
		HzHit(V, KtTn(TEXT("boreDamage"), 80.f), FVector(FrontX, P.Y, P.Z), { { TEXT("soak"), 100.f } });
		WRSkyAir::Push(V, FVector(-KtTn(TEXT("boreCarry"), 2000.f) / 1.0f, 0.f, 100.f), 1.0f, TEXT("bore"), nullptr);
	});
	// Pieces below the line in its path: 200 once.
	for (TActorIterator<AWRBuildPiece> It(World); It; ++It)
	{
		const FVector P = It->GetActorLocation();
		if (!WROcean::Contains(P) || P.Z >= Line || PiecesHit.Contains(*It) || FMath::Abs(P.X - FrontX) > 300.f) continue;
		PiecesHit.Add(*It);
		It->TakeBuildDamage(KtTn(TEXT("pieceDamage"), 200.f), nullptr);
	}
}

void UWROceanHazards::EndKingTide()
{
	AWROceanState* S = AWROceanState::Get(this);
	if (S)
	{
		S->KingTide = EWRKingTide::None;
		S->KingTideStart = -1.0;
		S->BoreStart = -1.0;
		S->bSpawningNight = false;
	}
	// The stranded swim off on the water's return; the pools are sea again.
	for (const TWeakObjectPtr<AWRCreature>& W : KingSpawned)
	{
		AWRCreature* C = W.Get();
		UWROceanBody* B = UWROceanBody::Get(C);
		if (!B) continue;
		B->bStrandedFlat = false;
		B->bPoolBound = false;
		B->Set(WROceanFlag::Stranded, false);
	}
	KingSpawned.Reset();
	LineStood.Reset();
	UE_LOG(LogWildreach, Log, TEXT("OCEAN the King Tide is over"));
}

void UWROceanHazards::StepSalvager(float Dt)
{
	// Salvager (Swimming 75): every few seconds the sunken caches within 40 m of you show a thread of bubbles (yours only).
	SalvageClock += Dt;
	if (SalvageClock < HzTn(TEXT("salvager.every"), 6.f)) return;
	SalvageClock = 0.f;
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	if (!Me || UWRLifeSkillsComponent::StatFor(Me, TEXT("salvager")) <= 0.f || !WROcean::Contains(Me->GetActorLocation())) return;
	for (TActorIterator<AWRWorldCache> It(GetWorld()); It; ++It)
	{
		const FVector P = It->GetActorLocation();
		if (FVector::Dist(P, Me->GetActorLocation()) > HzTn(TEXT("salvager.reach"), 4000.f) || !WRUnderwater::WaterAt(GetWorld(), P + FVector(0, 0, 60.f))) continue;
		WRFx::SpawnAt(this, TEXT("ocean.salvage_thread"), P + FVector(0, 0, 40.f));
	}
}
