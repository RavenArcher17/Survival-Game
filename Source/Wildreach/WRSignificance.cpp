#include "WRSignificance.h"
#include "Wildreach.h"
#include "WRBoss.h"
#include "WRCreature.h"
#include "WRTuning.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"

bool FWRSignificanceRules::IsFar(float NearestPlayerDist, bool bWasFar, float FarRadius, float NearRadius)
{
	return bWasFar ? NearestPlayerDist > FMath::Min(NearRadius, FarRadius) : NearestPlayerDist > FarRadius;
}

bool UWRSignificanceSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

TStatId UWRSignificanceSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWRSignificanceSubsystem, STATGROUP_Tickables);
}

UWRSignificanceSubsystem* UWRSignificanceSubsystem::Find(const UWorld* World)
{
	return World ? World->GetSubsystem<UWRSignificanceSubsystem>() : nullptr;
}

void UWRSignificanceSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	Update();
	// Ride camera arms, every frame so mounting gets its camera at once: only a creature a player controls needs one.
	for (const FTracked& T : Tracked)
	{
		AWRCreature* Creature = T.Creature.Get();
		USpringArmComponent* Arm = T.Arm.Get();
		if (Creature && Arm)
		{
			const AController* C = Creature->GetController();
			const bool bRidden = C && C->IsPlayerController();
			if (Arm->IsComponentTickEnabled() != bRidden)
			{
				Arm->SetComponentTickEnabled(bRidden);
			}
		}
	}
}

void UWRSignificanceSubsystem::ApplyRate(AWRCreature* Creature, bool bFar, float Interval, float FarStep, float NetRateFar, bool bDedicated)
{
	const float Want = bFar ? Interval : 0.f;
	auto SetActor = [Want](AActor* A)
	{
		if (A && !FMath::IsNearlyEqual(A->GetActorTickInterval(), Want))
		{
			A->SetActorTickInterval(Want);
		}
	};
	auto SetComponent = [Want](UActorComponent* C)
	{
		if (C && !FMath::IsNearlyEqual(C->GetComponentTickInterval(), Want))
		{
			C->SetComponentTickInterval(Want);
		}
	};
	SetActor(Creature);
	// Its brain: AddMovementInput each think, which the movement below uses up (the controller ticks first).
	AController* Brain = Creature->GetController();
	if (Brain && !Brain->IsPlayerController())
	{
		SetActor(Brain);
	}
	if (UCharacterMovementComponent* Move = Creature->GetCharacterMovement())
	{
		SetComponent(Move);
		// A longer tick moves in fewer, longer steps (up to 8 per tick before the engine warns and drops time).
		const float Step = bFar ? FarStep : GetDefault<UCharacterMovementComponent>()->MaxSimulationTimeStep;
		Move->MaxSimulationTimeStep = Step;
	}
	if (bDedicated)
	{
		// Nothing is drawn on a dedicated server: its meshes only keep their bones (unused far from everyone).
		TInlineComponentArray<USkeletalMeshComponent*> Meshes(Creature);
		for (USkeletalMeshComponent* Mesh : Meshes)
		{
			SetComponent(Mesh);
		}
	}
	// Nobody it could be sent to is near, so the replication pass needn't look at it every frame; it's back to the
	// usual rate well before a player comes within the 150 m it replicates within.
	const float NetRate = bFar ? NetRateFar : Creature->GetClass()->GetDefaultObject<AActor>()->GetNetUpdateFrequency();
	if (Creature->GetIsReplicated() && NetRate > 0.f && !FMath::IsNearlyEqual(Creature->GetNetUpdateFrequency(), NetRate))
	{
		Creature->SetNetUpdateFrequency(NetRate);
	}
}

void UWRSignificanceSubsystem::Update()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	// One slice of the creatures per frame (every significance.slices frames each is looked at once). Besides keeping
	// each frame's share small, it spreads the moments creatures go far over several frames, so their slower ticks
	// don't all land in the same frame (one heavy frame every quarter second instead of a lighter one every frame).
	const int32 Slices = FMath::Clamp(FMath::RoundToInt(WRTune(TEXT("significance.slices"), 8.f)), 1, 64);
	Slice = (Slice + 1) % Slices;
	if (Slice == 0 || Tracked.Num() == 0)
	{
		// New creatures (a pack coming out, a tame hatching) join the list once a pass; gone ones drop out.
		TMap<const AWRCreature*, FTracked> Old;
		for (const FTracked& T : Tracked)
		{
			if (const AWRCreature* C = T.Creature.Get())
			{
				Old.Add(C, T);
			}
		}
		TArray<FTracked> Next;
		for (TActorIterator<AWRCreature> It(World); It; ++It)
		{
			if (const FTracked* Was = Old.Find(*It))
			{
				Next.Add(*Was);
			}
			else
			{
				FTracked T;
				T.Creature = *It;
				T.Arm = It->FindComponentByClass<USpringArmComponent>();
				Next.Add(T);
			}
		}
		Tracked = MoveTemp(Next);
	}

	const ENetMode Mode = World->GetNetMode();
	if (Mode == NM_Client)
	{
		return;  // a client only looks after the camera arms (Tick); the authority runs the creatures
	}
	const bool bEnabled = WRTune(TEXT("significance.enabled"), 1.f) > 0.f;
	// Remote players see creatures out to the 150 m they're replicated within; a player on this machine (a listen
	// server's host, a game on its own) has the whole island loaded and can see further, so gets a wider radius.
	const float FarRadius = WRTune(TEXT("significance.farRadius"), 20000.f);
	const float LocalFarRadius = FMath::Max(FarRadius, WRTune(TEXT("significance.localFarRadius"), 35000.f));
	const float Margin = FMath::Max(0.f, WRTune(TEXT("significance.margin"), 2000.f));
	const float Interval = FMath::Clamp(WRTune(TEXT("significance.farTickInterval"), 0.25f), 0.f, 0.5f);
	const float FarStep = FMath::Clamp(WRTune(TEXT("significance.farMaxStep"), 0.1f), 0.01f, 0.2f);
	const float NetRateFar = FMath::Max(1.f, WRTune(TEXT("significance.farNetUpdateRate"), 4.f));

	// Where the players are: their body, or what they're looking at while dead or spectating.
	TArray<TPair<FVector, float>, TInlineAllocator<16>> Players;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		if (!PC)
		{
			continue;
		}
		const float Radius = PC->IsLocalController() ? LocalFarRadius : FarRadius;
		if (const APawn* Pawn = PC->GetPawnOrSpectator())
		{
			Players.Add({ Pawn->GetActorLocation(), Radius });
		}
		else if (const AActor* View = PC->GetViewTarget())
		{
			Players.Add({ View->GetActorLocation(), Radius });
		}
	}

	for (int32 Index = Slice; Index < Tracked.Num(); Index += Slices)
	{
		FTracked& T = Tracked[Index];
		AWRCreature* Creature = T.Creature.Get();
		if (!Creature)
		{
			continue;
		}
		const AController* C = Creature->GetController();
		// Far only when far from every player (2D, as the wildlife sites measure).
		bool bFar = bEnabled && !Creature->IsA<AWRBoss>() && !(C && C->IsPlayerController());
		for (int32 i = 0; i < Players.Num() && bFar; ++i)
		{
			const float D = FVector::Dist2D(Players[i].Key, Creature->GetActorLocation());
			bFar = FWRSignificanceRules::IsFar(D, T.bFar, Players[i].Value, Players[i].Value - Margin);
		}
		T.bFar = bFar;
		ApplyRate(Creature, bFar, Interval, FarStep, NetRateFar, Mode == NM_DedicatedServer);
	}

	Far = 0;
	for (const FTracked& T : Tracked)
	{
		Far += T.bFar && T.Creature.IsValid();
	}
	const double Now = World->GetTimeSeconds();
	if (Slice == 0 && Now >= NextLogAt && Tracked.Num() > 0)
	{
		NextLogAt = Now + 30.0;
		UE_LOG(LogWildreach, Log, TEXT("SIGNIFICANCE %d creatures, %d far from every player (beyond %.0f m: ticking every %.2f s)"),
			Tracked.Num(), Far, FarRadius / 100.f, Interval);
	}
}
