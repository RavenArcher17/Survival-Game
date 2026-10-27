#include "WRAudio.h"
#include "WRSound.h"
#include "Wildreach.h"
#include "WRBoss.h"
#include "WRCharacter.h"
#include "WRCharacterBase.h"
#include "WRCreature.h"
#include "WRGameState.h"
#include "WRGameplayTags.h"
#include "WRMainMenu.h"
#include "WRTuning.h"
#include "WRWorldDressing.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

namespace WRAudioDirectorDetail
{
	static float Smooth(float A, float B, float X)
	{
		const float T = FMath::Clamp((X - A) / FMath::Max(B - A, 1.f), 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	}
	// Coast sampling: 16 directions at three distances (m): 25, 60, 120.
	static constexpr int32 CoastDirs = 16;
	static constexpr float CoastRadii[3] = { 2500.f, 6000.f, 12000.f };
	static constexpr float CoastWeights[3] = { 1.f, 0.7f, 0.4f };
}

using namespace WRAudioDirectorDetail;

bool UWRAudioDirector::ShouldCreateSubsystem(UObject* Outer) const
{
	if (IsRunningDedicatedServer())
	{
		return false;
	}
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UWRAudioDirector::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	HitHandle = AWRCharacterBase::OnHitShown.AddUObject(this, &UWRAudioDirector::OnHitShown);
	CoastSamples.Init(0, CoastDirs * 3);
}

void UWRAudioDirector::Deinitialize()
{
	AWRCharacterBase::OnHitShown.Remove(HitHandle);
	for (TPair<FName, FStream>& B : Beds)
	{
		if (B.Value.Comp.IsValid()) B.Value.Comp->Stop();
	}
	for (FStream& S : Music)
	{
		if (S.Comp.IsValid()) S.Comp->Stop();
	}
	Beds.Empty();
	Music.Empty();
	Held.Empty();
	bStarted = false;
	Super::Deinitialize();
}

void UWRAudioDirector::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	bStarted = InWorld.GetNetMode() != NM_DedicatedServer;
	const double Now = InWorld.GetRealTimeSeconds();
	NextExploreAt = Now + FMath::FRandRange(Tune(TEXT("music.firstGap.min"), 25.f), Tune(TEXT("music.firstGap.max"), 50.f));
}

TStatId UWRAudioDirector::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWRAudioDirector, STATGROUP_Tickables);
}

UWRAudioDirector* UWRAudioDirector::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UWRAudioDirector>() : nullptr;
}

float UWRAudioDirector::Tune(const TCHAR* Key, float Fallback) const
{
	const UWRAudioSubsystem* Audio = UWRAudioSubsystem::Get(GetWorld());
	return Audio ? Audio->Tune(FName(Key), Fallback) : Fallback;
}

void UWRAudioDirector::Duck(float Amount, float Seconds)
{
	const double Now = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.0;
	DuckTarget = FMath::Min(DuckTarget, 1.f - FMath::Clamp(Amount, 0.f, 1.f));
	DuckUntil = FMath::Max(DuckUntil, Now + Seconds);
}

void UWRAudioDirector::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	UWRAudioSubsystem* Audio = UWRAudioSubsystem::Get(World);
	if (!World || World->GetNetMode() == NM_DedicatedServer || !Audio || !Audio->EnsureLoaded())
	{
		return;
	}
	UpdateListener();
	if (!bHasListener)
	{
		return;
	}
	UpdateAmbient(DeltaTime);
	UpdateMusic(DeltaTime);
	WatchBodies();
	// With logging on, the mixer's state every 10 s (day/night and biome changes show in the log).
	const double Now = World->GetRealTimeSeconds();
	if (Audio->IsLogging() && Now > NextDescribe)
	{
		NextDescribe = Now + 10.0;
		Audio->Log(TEXT("state ") + Describe());
	}
}

// ---------------------------------------------------------------- listener

void UWRAudioDirector::UpdateListener()
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC || !PC->IsLocalController())
	{
		bHasListener = false;
		return;
	}
	APawn* Pawn = PC->GetPawn();
	MyPawn = Pawn;
	if (!Pawn && !SWRMainMenu::IsOpen())
	{
		return;   // joining or between lives: keep the last place rather than hear the world's origin
	}
	FVector Cam;
	FRotator Rot;
	PC->GetPlayerViewPoint(Cam, Rot);
	// Where the player is decides the biome and cave; the menu (no pawn yet) uses its camera.
	Listener = Pawn ? Pawn->GetActorLocation() : Cam;
	bHasListener = true;
}

// ---------------------------------------------------------------- ambient

float UWRAudioDirector::CaveFactor() const
{
	// Cave local layout (AWRCave::BuildShell): chamber x/y -900..900, 0..700 high; the entrance tunnel runs out along +X.
	float Best = 0.f;
	for (const TWeakObjectPtr<AActor>& Weak : Caves)
	{
		const AActor* C = Weak.Get();
		if (!C || FVector::DistSquared(C->GetActorLocation(), Listener) > FMath::Square(4000.f)) continue;
		const FVector L = C->GetActorTransform().InverseTransformPosition(Listener);
		if (L.Z < -400.f || L.Z > 1000.f || FMath::Abs(L.Y) > 1000.f) continue;
		if (L.X >= -1000.f && L.X <= 1000.f) Best = 1.f;
		else if (L.X > 1000.f && L.X < 2300.f && FMath::Abs(L.Y) < 600.f) Best = FMath::Max(Best, 1.f - (L.X - 1000.f) / 1300.f);
	}
	return Best;
}

float UWRAudioDirector::LavaFactor() const
{
	const TMap<FIntPoint, float>& Cells = FWRBiomeHazards::Cells(TEXT("lava"));
	const float Cell = FWRBiomeHazards::CellSize();
	if (Cells.Num() == 0 || Cell <= 0.f)
	{
		return 0.f;
	}
	const float Reach = Tune(TEXT("ambient.lavaReach"), 3500.f);
	const int32 R = FMath::CeilToInt(Reach / Cell);
	const int32 CX = FMath::FloorToInt(Listener.X / Cell), CY = FMath::FloorToInt(Listener.Y / Cell);
	float Nearest = Reach * 2.f;
	for (int32 DX = -R; DX <= R; ++DX)
	{
		for (int32 DY = -R; DY <= R; ++DY)
		{
			if (const float* Z = Cells.Find(FIntPoint(CX + DX, CY + DY)))
			{
				const FVector Centre((CX + DX + 0.5f) * Cell, (CY + DY + 0.5f) * Cell, *Z);
				Nearest = FMath::Min(Nearest, static_cast<float>(FVector::Dist(Centre, Listener)));
			}
		}
	}
	return 1.f - Smooth(600.f, Reach, Nearest);
}

float UWRAudioDirector::SampleCoast()
{
	// A few downward traces a tick around the listener: no landscape (or only seabed) under a point means sea there.
	UWorld* World = GetWorld();
	const float Sea = WRTune(TEXT("audio.seaLevel"), -20.f) + 150.f;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(WRCoast), false);
	for (int32 n = 0; n < 4; ++n)
	{
		const int32 Index = CoastCursor++ % CoastSamples.Num();
		const int32 Dir = Index % CoastDirs, Ring = Index / CoastDirs;
		const float A = 2.f * PI * (Dir + 0.5f * Ring) / CoastDirs;
		const FVector P = Listener + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * CoastRadii[Ring];
		FHitResult Hit;
		// Only seabed counts as sea: a miss means the ground there hasn't streamed in yet (just spawned or teleported), so
		// that sample keeps what it had rather than reading as open water.
		if (World->LineTraceSingleByChannel(Hit, FVector(P.X, P.Y, Listener.Z + 30000.f), FVector(P.X, P.Y, -20000.f), ECC_WorldStatic, Query))
		{
			CoastSamples[Index] = Hit.ImpactPoint.Z < Sea ? 1 : 0;
		}
	}
	float Best = 0.f;
	for (int32 Ring = 0; Ring < 3; ++Ring)
	{
		int32 Water = 0;
		for (int32 Dir = 0; Dir < CoastDirs; ++Dir) Water += CoastSamples[Ring * CoastDirs + Dir];
		Best = FMath::Max(Best, static_cast<float>(Water) / CoastDirs * CoastWeights[Ring]);
	}
	return FMath::Clamp(Best * 2.5f, 0.f, 1.f);
}

float UWRAudioDirector::LayerWeight(FName Layer) const
{
	const float Out = 1.f - Cave;
	const float Volc = 1.f - Tundra;
	const float Night = 1.f - Daylight;
	static const FName Volcanic(TEXT("volcanic")), TundraL(TEXT("tundra")), CoastL(TEXT("coast")), Day(TEXT("day")), NightL(TEXT("night")),
		VolcanicDay(TEXT("volcanic.day")), VolcanicNight(TEXT("volcanic.night")), TundraDay(TEXT("tundra.day")), TundraNight(TEXT("tundra.night")),
		CaveL(TEXT("cave")), LavaL(TEXT("lava")), HighL(TEXT("high")), Wind(TEXT("wind")), Always(TEXT("always"));
	if (Layer == Volcanic) return Out * Volc;
	if (Layer == TundraL) return Out * Tundra;
	if (Layer == CoastL) return Out * Coast;
	if (Layer == Day) return Out * Daylight;
	if (Layer == NightL) return Out * Night;
	if (Layer == VolcanicDay) return Out * Volc * Daylight;
	if (Layer == VolcanicNight) return Out * Volc * Night;
	if (Layer == TundraDay) return Out * Tundra * Daylight;
	if (Layer == TundraNight) return Out * Tundra * Night;
	if (Layer == CaveL) return Cave;
	if (Layer == LavaL) return Out * Lava;
	if (Layer == HighL) return Out * High;
	if (Layer == Wind) return Out * FMath::Max(Tundra, High);
	if (Layer == Always) return 1.f;
	return 0.f;
}

bool UWRAudioDirector::StepStream(FStream& S, float DeltaTime, float Gain)
{
	S.Gain = FMath::FInterpConstantTo(S.Gain, S.Target, DeltaTime, 1.f / FMath::Max(S.FadeSeconds, 0.05f));
	UAudioComponent* C = S.Comp.Get();
	if (!C)
	{
		return true;
	}
	if (S.Target <= 0.f && S.Gain <= 0.0001f)
	{
		C->Stop();
		Held.Remove(C);
		return true;   // faded out: let it go
	}
	C->SetVolumeMultiplier(FMath::Max(S.Base * S.Gain * Gain, 0.001f));
	return false;
}

void UWRAudioDirector::UpdateAmbient(float DeltaTime)
{
	UWorld* World = GetWorld();
	UWRAudioSubsystem* Audio = UWRAudioSubsystem::Get(World);
	const double Now = World->GetRealTimeSeconds();

	// Where the listener is. The ground's biome blend (build_terrain_materials.py) runs across 80 m at the ridge (x = 0).
	Tundra = FMath::Clamp(Listener.X / Tune(TEXT("ambient.biomeBlend"), 8000.f) + 0.5f, 0.f, 1.f);
	const AWRGameState* GS = AWRGameState::Get(World);
	Daylight = GS ? GS->GetDaylight() : 1.f;
	High = Smooth(Tune(TEXT("ambient.highFrom"), 25000.f), Tune(TEXT("ambient.highTo"), 45000.f), Listener.Z);
	if (Now >= NextCaveScan)
	{
		NextCaveScan = Now + 5.0;
		Caves.Reset();
		for (TActorIterator<AWRCave> It(World); It; ++It) Caves.Add(*It);
	}
	Cave = FMath::FInterpConstantTo(Cave, CaveFactor(), DeltaTime, 1.f / Tune(TEXT("ambient.caveFade"), 1.5f));
	if (Now >= NextCoastSample)
	{
		NextCoastSample = Now + 0.25;
		CoastTarget = SampleCoast();
		if (Now > NextLavaCheck)
		{
			NextLavaCheck = Now + 0.5;
			Lava = FMath::Lerp(Lava, LavaFactor(), 0.5f);
		}
	}
	Coast = FMath::FInterpConstantTo(Coast, CoastTarget, DeltaTime, 0.35f);

	// Beds fade toward their layer's weight; silent ones stop and start again when wanted.
	const float Fade = Tune(TEXT("ambient.fadeSeconds"), 3.f);
	const float Gain = WRSound::CategoryGain(TEXT("ambient"));
	for (const FWRAmbientBed& Bed : Audio->GetBeds())
	{
		const float Target = LayerWeight(Bed.Layer);
		FStream* S = Beds.Find(Bed.Id);
		if (!S && Target <= 0.02f)
		{
			continue;
		}
		if (!S)
		{
			S = &Beds.Add(Bed.Id);
			S->File = Bed.File;
			S->Base = Bed.Volume;
		}
		if (!S->Comp.IsValid() && Target > 0.02f)
		{
			S->Comp = Audio->CreateStream(World, Bed.File, true);
			S->Gain = 0.f;
			if (S->Comp.IsValid()) Held.Add(S->Comp.Get());
			if (!S->Comp.IsValid())
			{
				S->Target = 0.f;
				continue;
			}
		}
		S->Target = Target;
		S->FadeSeconds = Fade;
		if (StepStream(*S, DeltaTime, Gain))
		{
			S->Comp.Reset();
		}
	}

	// Spot sounds: an owl now and then at night, somewhere off in the dark.
	if (Now >= NextSpot)
	{
		NextSpot = Now + FMath::FRandRange(Tune(TEXT("ambient.owlEvery.min"), 25.f), Tune(TEXT("ambient.owlEvery.max"), 70.f));
		if (LayerWeight(TEXT("night")) > 0.6f && !SWRMainMenu::IsOpen())
		{
			const float A = FMath::FRandRange(0.f, 2.f * PI);
			WRSound::PlayAt(World, TEXT("amb.owl"), Listener + FVector(FMath::Cos(A), FMath::Sin(A), 0.3f) * FMath::FRandRange(2500.f, 5000.f));
		}
	}
}

// ---------------------------------------------------------------- music

UWRAudioDirector::EMusic UWRAudioDirector::WantedMusic()
{
	if (!ForcedMode.IsNone())
	{
		return ForcedMode == TEXT("menu") ? EMusic::Menu : ForcedMode == TEXT("combat") ? EMusic::Combat : ForcedMode == TEXT("boss") ? EMusic::Boss : EMusic::Explore;
	}
	if (SWRMainMenu::IsOpen())
	{
		return EMusic::Menu;
	}
	UWorld* World = GetWorld();
	const double Now = World->GetRealTimeSeconds();
	if (Now >= NextEngageCheck)
	{
		NextEngageCheck = Now + 0.5;
		bEngaged = false;
		bBoss = false;
		const APawn* Me = MyPawn.Get();
		const AActor* Mount = Me ? Me->GetAttachParentActor() : nullptr;
		const float Range = Tune(TEXT("music.combatRange"), 5000.f);
		const float BossRange = Tune(TEXT("music.bossRange"), 4500.f);
		if (Me)
		{
			for (TActorIterator<AWRCreature> It(World); It; ++It)
			{
				const AWRCreature* C = *It;
				if (!C->IsAlive() || C->IsTamed() || C->IsUnconscious()) continue;
				const float D2 = FVector::DistSquared(C->GetActorLocation(), Me->GetActorLocation());
				if (const AWRBoss* B = Cast<AWRBoss>(C))
				{
					// As the HUD's boss bar: an engaged boss close by.
					if (B->bEngaged && D2 < FMath::Square(BossRange)) bBoss = true;
					continue;
				}
				const AActor* Target = C->GetAggroRep();
				if (Target && (Target == Me || Target == Mount) && D2 < FMath::Square(Range)) bEngaged = true;
			}
		}
	}
	if (bBoss)
	{
		return EMusic::Boss;
	}
	if (bEngaged)
	{
		CombatUntil = Now + Tune(TEXT("music.combatHold"), 8.f);
	}
	return Now < CombatUntil ? EMusic::Combat : EMusic::Explore;
}

void UWRAudioDirector::StartMusic(EMusic Which)
{
	UWorld* World = GetWorld();
	UWRAudioSubsystem* Audio = UWRAudioSubsystem::Get(World);
	FName PoolName = Which == EMusic::Menu ? FName(TEXT("menu")) : Which == EMusic::Combat ? FName(TEXT("combat")) : Which == EMusic::Boss ? FName(TEXT("boss"))
		: Daylight < Tune(TEXT("music.nightBelow"), 0.35f) ? FName(TEXT("night")) : FName(TEXT("explore"));
	const TArray<FWRMusicTrack>* Pool = Audio->GetMusicPool(PoolName);
	if ((!Pool || Pool->Num() == 0) && PoolName == TEXT("night"))
	{
		Pool = Audio->GetMusicPool(TEXT("explore"));
	}
	if (!Pool || Pool->Num() == 0)
	{
		return;
	}
	int32 Pick = FMath::RandHelper(Pool->Num());
	if (Pool->Num() > 1 && (*Pool)[Pick].File == LastExplore) Pick = (Pick + 1) % Pool->Num();
	const FWRMusicTrack& Track = (*Pool)[Pick];
	const bool bLoop = Which != EMusic::Explore;
	FStream S;
	S.Comp = Audio->CreateStream(World, Track.File, bLoop);
	if (!S.Comp.IsValid())
	{
		return;
	}
	Held.Add(S.Comp.Get());
	S.File = Track.File;
	S.Base = Track.Volume;
	S.Gain = 0.f;
	S.Target = 1.f;
	S.FadeSeconds = Which == EMusic::Combat || Which == EMusic::Boss ? Tune(TEXT("music.combatFadeIn"), 1.2f) : Tune(TEXT("music.fadeIn"), 4.f);
	S.StartedAt = World->GetRealTimeSeconds();
	if (Which == EMusic::Explore) LastExplore = Track.File;
	Music.Add(S);
	UE_LOG(LogWildreach, Log, TEXT("MUSIC %s: %s"), *PoolName.ToString(), *Track.File);
}

void UWRAudioDirector::UpdateMusic(float DeltaTime)
{
	UWorld* World = GetWorld();
	const double Now = World->GetRealTimeSeconds();
	const EMusic Want = WantedMusic();
	if (Want != Mode)
	{
		const EMusic Was = Mode;
		Mode = Want;
		for (FStream& S : Music)
		{
			S.Target = 0.f;
			S.FadeSeconds = Tune(Was == EMusic::Combat || Was == EMusic::Boss ? TEXT("music.combatFadeOut") : TEXT("music.fadeOut"), 4.f);
		}
		if (Want == EMusic::Explore)
		{
			// After a fight, a little quiet before the next tune; at the start, a while to take in the island.
			const bool bAfterFight = Was == EMusic::Combat || Was == EMusic::Boss;
			NextExploreAt = Now + (bAfterFight ? FMath::FRandRange(Tune(TEXT("music.afterCombatGap.min"), 40.f), Tune(TEXT("music.afterCombatGap.max"), 90.f))
				: FMath::FRandRange(Tune(TEXT("music.firstGap.min"), 25.f), Tune(TEXT("music.firstGap.max"), 50.f)));
			if (!ForcedMode.IsNone()) NextExploreAt = Now;
		}
		else
		{
			StartMusic(Want);
		}
		UE_LOG(LogWildreach, Log, TEXT("MUSIC mode %d -> %d"), static_cast<int32>(Was), static_cast<int32>(Want));
	}
	if (Mode == EMusic::Explore)
	{
		FStream* Current = Music.Num() > 0 && Music.Last().Target > 0.f ? &Music.Last() : nullptr;
		if (!Current && Now >= NextExploreAt)
		{
			StartMusic(EMusic::Explore);
		}
		else if (Current)
		{
			const bool bOver = !Current->Comp.IsValid() || !Current->Comp->IsPlaying();
			if (bOver || Now - Current->StartedAt > Tune(TEXT("music.exploreMaxSeconds"), 200.f))
			{
				// Sparse, not constant: the tune ends (or fades after a while), then quiet for a few minutes.
				Current->Target = 0.f;
				Current->FadeSeconds = Tune(TEXT("music.fadeOut"), 4.f);
				NextExploreAt = Now + FMath::FRandRange(Tune(TEXT("music.gap.min"), 120.f), Tune(TEXT("music.gap.max"), 300.f));
			}
		}
	}

	// Ducking under big moments: dips fast, comes back slowly.
	if (Now > DuckUntil)
	{
		DuckTarget = 1.f;
	}
	DuckLevel = FMath::FInterpTo(DuckLevel, DuckTarget, DeltaTime, DuckTarget < DuckLevel ? 10.f : 1.2f);
	const float Gain = WRSound::CategoryGain(TEXT("music")) * DuckLevel;
	for (int32 i = Music.Num() - 1; i >= 0; --i)
	{
		if (StepStream(Music[i], DeltaTime, Gain))
		{
			Music.RemoveAt(i);
		}
	}
}

// ---------------------------------------------------------------- bodies

void UWRAudioDirector::OnHitShown(const AActor* Target, float Damage, EWRHitResult Result)
{
	const AWRCharacterBase* C = Cast<AWRCharacterBase>(Target);
	if (Result != EWRHitResult::Hit || !C || C->GetWorld() != GetWorld() || !C->IsAlive())
	{
		return;   // a block or parry has its own clang (the hit site plays it); the dead get a death call instead
	}
	WRSound::Voice(const_cast<AWRCharacterBase*>(C), TEXT("hurt"));
}

void UWRAudioDirector::WatchBodies()
{
	UWorld* World = GetWorld();
	const double Now = World->GetTimeSeconds();
	if (Now < NextWatch)
	{
		return;
	}
	NextWatch = Now + 0.1;
	const float Range = Tune(TEXT("ambient.watchRange"), 4500.f);
	const float IdleRange = Tune(TEXT("ambient.idleRange"), 3000.f);
	const float IdleMin = Tune(TEXT("ambient.idleEvery.min"), 12.f), IdleMax = Tune(TEXT("ambient.idleEvery.max"), 35.f);
	for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
	{
		AWRCharacterBase* C = *It;
		const float D2 = FVector::DistSquared(C->GetActorLocation(), Listener);
		if (D2 > FMath::Square(Range))
		{
			Bodies.Remove(C);   // seen again later: its state is read fresh, without a sound
			continue;
		}
		const bool bDead = !C->IsAlive();
		const UCharacterMovementComponent* Move = C->GetCharacterMovement();
		const bool bFalling = Move && Move->IsFalling();
		AWRCreature* Creature = Cast<AWRCreature>(C);
		FBody* B = Bodies.Find(C);
		if (!B)
		{
			FBody& New = Bodies.Add(C);
			New.bDead = bDead;
			New.bFalling = bFalling;
			New.bDodging = C->HasTag(WRTags::State_Dodging);
			New.Aggro = Creature ? Creature->GetAggroRep() : nullptr;
			New.NextIdle = Now + FMath::FRandRange(3.f, IdleMax);
			New.Seen = Now;
			continue;
		}
		B->bDead = bDead;   // the death call itself comes from the death multicast (AWRCharacterBase)
		if (bDead)
		{
			continue;
		}
		if (!Creature)
		{
			// Survivors: take-off, landing (harder the faster the fall), dodge rolls.
			if (bFalling && !B->bFalling)
			{
				B->FallStart = Now;
				B->FallSpeed = 0.f;
				if (C->GetVelocity().Z > 150.f) WRSound::PlayOn(C, TEXT("jump"));
			}
			if (bFalling)
			{
				B->FallSpeed = FMath::Max(B->FallSpeed, static_cast<float>(-C->GetVelocity().Z));
			}
			else if (B->bFalling && Now - B->FallStart > 0.25 && Move && !Move->IsSwimming())
			{
				const float Hard = FMath::Clamp(B->FallSpeed / 900.f, 0.35f, 1.5f);
				WRSound::PlayOn(C, TEXT("land"), Hard);
				WRSound::Footstep(C, Hard);
			}
			const bool bDodging = C->HasTag(WRTags::State_Dodging);
			if (bDodging && !B->bDodging) WRSound::PlayOn(C, TEXT("dodge"));
			B->bDodging = bDodging;
		}
		else
		{
			// Creatures: a call when they turn on someone, and now and then while they go about their day. A boss picks its
			// targets its own way: its roar comes when the fight starts.
			const AWRBoss* Boss = Cast<AWRBoss>(Creature);
			AActor* Aggro = Boss ? (Boss->bEngaged ? Creature : nullptr) : Creature->GetAggroRep();
			if (Aggro && Aggro != B->Aggro.Get()) WRSound::Voice(Creature, TEXT("aggro"));
			B->Aggro = Aggro;
			if (Now >= B->NextIdle)
			{
				B->NextIdle = Now + FMath::FRandRange(IdleMin, IdleMax) * (Creature->IsTamed() ? 2.f : 1.f);
				if (!Aggro && !Creature->bAsleep && !Creature->IsUnconscious() && D2 < FMath::Square(IdleRange)) WRSound::Voice(Creature, TEXT("idle"));
			}
		}
		B->bFalling = bFalling;
	}
	for (auto It = Bodies.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid()) It.RemoveCurrent();
	}
}

FString UWRAudioDirector::Describe() const
{
	const TCHAR* Modes[] = { TEXT("none"), TEXT("menu"), TEXT("explore"), TEXT("combat"), TEXT("boss") };
	FString Out = FString::Printf(TEXT("music %s"), Modes[static_cast<int32>(Mode)]);
	for (const FStream& S : Music)
	{
		Out += FString::Printf(TEXT(" %s@%.2f"), *S.File, S.Gain);
	}
	if (Mode == EMusic::Explore && (Music.Num() == 0 || Music.Last().Target <= 0.f) && GetWorld())
	{
		Out += FString::Printf(TEXT(" (next in %.0fs)"), NextExploreAt - GetWorld()->GetRealTimeSeconds());
	}
	if (DuckLevel < 0.99f) Out += FString::Printf(TEXT(" duck %.2f"), DuckLevel);
	Out += FString::Printf(TEXT(" | at %.0f,%.0f,%.0f tundra %.2f day %.2f coast %.2f cave %.2f lava %.2f high %.2f | beds"), Listener.X, Listener.Y, Listener.Z,
		Tundra, Daylight, Coast, Cave, Lava, High);
	for (const TPair<FName, FStream>& B : Beds)
	{
		if (B.Value.Comp.IsValid()) Out += FString::Printf(TEXT(" %s@%.2f"), *B.Key.ToString(), B.Value.Gain);
	}
	return Out;
}
