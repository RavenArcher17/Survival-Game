#include "WRAudio.h"
#include "WRIslandPlaces.h"
#include "WRCrystalTypes.h"
#include "WRFungalTypes.h"
#include "WRUnderwater.h"
#include "WRTide.h"
#include "WROceanTypes.h"
#include "WRDesertHazards.h"
#include "WRDesertTypes.h"
#include "WRFungalHazards.h"
#include "WRStormTypes.h"
#include "WRSwampTypes.h"
#include "WRWorldDressing.h"
#include "WRLand.h"
#include "WRSkyIsleTypes.h"
#include "WRSkyWorld.h"
#include "GameFramework/GameStateBase.h"
#include "WRPortals.h"
#include "WRSurvival.h"
#include "WRSound.h"
#include "Wildreach.h"
#include "WRBoss.h"
#include "WRForestTypes.h"
#include "WRCharacter.h"
#include "WRCharacterBase.h"
#include "WRCreature.h"
#include "WRGameState.h"
#include "WRGameplayTags.h"
#include "WRMainMenu.h"
#include "WRTuning.h"
#include "WRWorldDressing.h"
#include "WRWorldSystems.h"
#include "WRBuilding.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/ReverbEffect.h"
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
	if (!Space.IsNone() && GetWorld()) UGameplayStatics::DeactivateReverbEffect(GetWorld(), TEXT("WRSpace"));
	Space = NAME_None;
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
	// The score is always audible: no moment takes it down more than 4 dB, anywhere (FULL-GAME 3.3, fire-frost 3.5-3.6;
	// cookpass-audio 4.1.2).
	Amount = FMath::Min(Amount, Tune(TEXT("music.duckMax"), 0.37f));
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
	Audio->StepRises();
	Audio->ApplyDynamics(World);
	UpdateAmbient(DeltaTime);
	UpdateSpace();
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
	// The island's beds give way to Storm peaks' across the Ash Sound and the neck, and to the Swamp's east of the Crown.
	const float Isl = FMath::Max(0.f, 1.f - Storm - Swamp);
	const float Volc = (1.f - Tundra) * Isl * (1.f - Forest);
	const float TundraW = Tundra * Isl * (1.f - Forest);
	const float Night = 1.f - Daylight;
	static const FName Volcanic(TEXT("volcanic")), TundraL(TEXT("tundra")), CoastL(TEXT("coast")), Day(TEXT("day")), NightL(TEXT("night")),
		VolcanicDay(TEXT("volcanic.day")), VolcanicNight(TEXT("volcanic.night")), TundraDay(TEXT("tundra.day")), TundraNight(TEXT("tundra.night")),
		CaveL(TEXT("cave")), LavaL(TEXT("lava")), HighL(TEXT("high")), Wind(TEXT("wind")), Always(TEXT("always"));
	if (Layer == Volcanic) return Out * Volc;
	if (Layer == TundraL) return Out * TundraW;
	if (Layer == CoastL) return Out * Coast;
	if (Layer == Day) return Out * Daylight * (1.f - Forest);   // the forest sings its own birds (forest.day)
	if (Layer == NightL) return Out * Night;
	if (Layer == VolcanicDay) return Out * Volc * Daylight;
	if (Layer == VolcanicNight) return Out * Volc * Night;
	if (Layer == TundraDay) return Out * TundraW * Daylight;
	if (Layer == TundraNight) return Out * TundraW * Night;
	if (Layer == CaveL) return Cave;
	if (Layer == LavaL) return Out * Lava;
	if (Layer == HighL) return Out * High;
	if (Layer == Wind) return Out * FMath::Max(TundraW, High);
	// The Fire and Frost sub-zones' own beds (cooks/cookpass-world.md 3.2: layer ffzone.<V1..T5>), over the island's.
	{
		const FString LZ = Layer.ToString();
		if (LZ.StartsWith(TEXT("ffzone."))) return FName(*LZ.RightChop(7)) == IsleZone ? Out * Isl * (1.f - Forest) : 0.f;
	}
	// Storm peaks' beds (Data/Audio/sounds.json ambient: layer storm_peaks / storm_peaks.day|night / storm.<zone>).
	static const FName StormL(TEXT("storm_peaks")), StormDay(TEXT("storm_peaks.day")), StormNight(TEXT("storm_peaks.night")),
		Passes(TEXT("storm.passes")), Cliffs(TEXT("storm.cliffs")), Bridges(TEXT("storm.bridges")), Crags(TEXT("storm.crags")),
		Shrines(TEXT("storm.shrines")), RodfieldL(TEXT("storm.rodfield")), HollowL(TEXT("storm.hollow"));
	if (Layer == StormL) return Out * Storm;
	if (Layer == StormDay) return Out * Storm * Daylight;
	if (Layer == StormNight) return Out * Storm * Night;
	if (Layer == Passes) return Out * StormZone[1];
	if (Layer == Cliffs) return Out * StormZone[2];
	if (Layer == Bridges) return Out * StormZone[3];
	if (Layer == Crags) return Out * FMath::Max(StormZone[4], StormZone[6]);
	if (Layer == Shrines) return Out * StormZone[5];
	if (Layer == RodfieldL) return Out * Rodfield;
	if (Layer == HollowL) return 0.f;   // the Hollow Spire isn't on the map yet
	// The Swamp's beds (layer swamp / swamp.day|night / swamp.<zone> / swamp.falls / swamp.bleedwater / swamp.coven /
	// swamp.red_tide; cooks/biome-swamp.md 2.2, 3.4). Down in the Sunken Coven the land's open air gives way to its own bed.
	static const FName SwampL(TEXT("swamp")), SwampDay(TEXT("swamp.day")), SwampNight(TEXT("swamp.night")),
		Reeds(TEXT("swamp.reeds")), ReedsNight(TEXT("swamp.reeds.night")), Mangroves(TEXT("swamp.mangroves")),
		Boardwalks(TEXT("swamp.boardwalks")), Pools(TEXT("swamp.pools")), PoolsNight(TEXT("swamp.pools.night")),
		Bones(TEXT("swamp.bones")), FallsL(TEXT("swamp.falls")), BleedL(TEXT("swamp.bleedwater")), CovenL(TEXT("swamp.coven")),
		TideL(TEXT("swamp.red_tide"));
	{
		const float OutS = Out * (1.f - SwampCoven);
		if (Layer == SwampL) return OutS * Swamp;
		if (Layer == SwampDay) return OutS * Swamp * Daylight;
		if (Layer == SwampNight) return OutS * Swamp * Night;
		if (Layer == Reeds) return OutS * SwampZone[1];
		if (Layer == ReedsNight) return OutS * SwampZone[1] * Night;   // the frog chorus from dusk
		if (Layer == Mangroves) return OutS * SwampZone[2];
		if (Layer == Boardwalks) return OutS * SwampZone[3];
		if (Layer == Pools) return OutS * SwampZone[4];
		if (Layer == PoolsNight) return OutS * SwampZone[4] * Night;
		if (Layer == Bones) return OutS * SwampZone[5];
		if (Layer == FallsL) return OutS * SwampFalls;
		if (Layer == BleedL) return OutS * SwampBleed;
		if (Layer == TideL) return OutS * SwampTide;
		if (Layer == CovenL) return SwampCoven;
	}
	// The Ancient forest's beds (layer forest.<zone> / forest.meadows.day / forest.streams.night / forest.day / forest.night
	// / forest.falls_near / forest.hollow; cooks/biome-forest.md 2.2, 3.4). Birdsong thins as the canopy closes (full in
	// F1-F3, further off in F4, sparse in F5); the owls are loudest in the deep wood. Down in the Hollow Root the open air
	// gives way to its own bed.
	static const FName ForestDay(TEXT("forest.day")), ForestNight(TEXT("forest.night")), Meadows(TEXT("forest.meadows")),
		MeadowsDay(TEXT("forest.meadows.day")), StreamsL(TEXT("forest.streams")), StreamsNight(TEXT("forest.streams.night")),
		Villages(TEXT("forest.villages")), Oldgrowth(TEXT("forest.oldgrowth")), CanopyL(TEXT("forest.canopy")),
		ForestHollow(TEXT("forest.hollow")), FallsNear(TEXT("forest.falls_near"));
	{
		const float OutF = Out * (1.f - ForestZone[6]);
		if (Layer == ForestDay) return OutF * Daylight * (ForestZone[1] + ForestZone[2] + ForestZone[3] + 0.6f * ForestZone[4] + 0.3f * ForestZone[5]);
		if (Layer == ForestNight) return OutF * Night * (0.7f * (ForestZone[1] + ForestZone[2] + ForestZone[3]) + ForestZone[4] + ForestZone[5]);
		if (Layer == Meadows) return OutF * ForestZone[1];
		if (Layer == MeadowsDay) return OutF * ForestZone[1] * Daylight;   // skylarks and bees over the hay
		if (Layer == StreamsL) return OutF * ForestZone[2];
		if (Layer == StreamsNight) return OutF * ForestZone[2] * Night;   // frogs along the Alder after dark
		if (Layer == Villages) return OutF * ForestZone[3];
		if (Layer == Oldgrowth) return OutF * ForestZone[4];
		if (Layer == CanopyL) return OutF * ForestZone[5];
		if (Layer == FallsNear) return OutF * ForestFalls;
		if (Layer == ForestHollow) return ForestZone[6];
	}
	if (Layer == Always) return 1.f;
	// The Ocean's underwater bed (the hum of the sea under the surface; pressure past 20 m is its own one-shot).
	if (Layer == FName(TEXT("ocean.underwater"))) return UnderBlend;
	// The Fungal caverns' beds (fungal.<zone>, fungal.F1.night, fungal.flue_roar; cooks/biome-fungal.md 2.15-2.16): the
	// plateau's wind and the doline glow's night by day and night, each cavern zone's own air under it (long tails in the
	// Glow-moss Halls, tight in the Webs), and the Flue's roar on the Fall Field, which its planned silence leaves alone.
	{
		const FString LF = Layer.ToString();
		if (LF.StartsWith(TEXT("fungal.")))
		{
			if (LF == TEXT("fungal.flue_roar")) return FlueRoar;
			if (FungalZone.IsNone()) return 0.f;
			TArray<FString> Parts;
			LF.ParseIntoArray(Parts, TEXT("."));
			if (Parts.Num() < 2 || FName(*Parts[1]) != FungalZone) return 0.f;
			const float W = Parts.Num() >= 3 ? (Parts[2] == TEXT("night") ? 1.f - Daylight : Parts[2] == TEXT("day") ? Daylight : 1.f) : 1.f;
			return W * (1.f - Hush);
		}
	}
	// Weather beds ("weather.<kind>", WRSurvival): rising over the 60 s warning, full while it blows, muffled in a cave.
	// Music is never touched by weather (it keeps playing; survival-crafting 2.4).
	const FString L = Layer.ToString();
	// A runtime land's beds, and its silence over all of them (biome-void 2.9: Dead calm).
	if (L.StartsWith(TEXT("land.")) || L.StartsWith(TEXT("zone.")))
	{
		if (LandBiome.IsNone()) return 0.f;
		TArray<FString> Parts;
		L.ParseIntoArray(Parts, TEXT("."));
		float W = 0.f;
		if (Parts[0] == TEXT("land") && Parts.Num() >= 2 && FName(*Parts[1]) == LandBiome)
		{
			W = Parts.Num() >= 3 ? (Parts[2] == TEXT("night") ? Night : Parts[2] == TEXT("day") ? Daylight : 1.f) : 1.f;
		}
		else if (Parts[0] == TEXT("zone") && Parts.Num() >= 2 && FName(*Parts[1]) == LandZone) W = Parts.Num() >= 3 ? (Parts[2] == TEXT("night") ? Night : Parts[2] == TEXT("day") ? Daylight : 1.f) : 1.f;   // zone.<id>[.day|.night]
		return W * Out * (1.f - Hush);
	}
	if (!LandBiome.IsNone() && !L.StartsWith(TEXT("weather.")) && Layer != Always && Layer != CaveL && Layer != CoastL) return 0.f;
	if (!LandBiome.IsNone() && Layer == CoastL) return 0.f + 0.f * Out;   // (the strand's surf is the land's own bed: zone.V2)
	if (L.StartsWith(TEXT("weather.")))
	{
		// weather.<kind> anywhere, or weather.<kind>.<biome> only in that land (a thunderstorm's rain in the peaks).
		FString KindStr = L.RightChop(8), OnlyIn;
		KindStr.Split(TEXT("."), &KindStr, &OnlyIn);
		if (!L.RightChop(8).Contains(TEXT("."))) { KindStr = L.RightChop(8); OnlyIn.Reset(); }
		const FName Kind(*KindStr);
		const FName Here = WRBiome::At(GetWorld(), Listener);
		if (!OnlyIn.IsEmpty() && Here != FName(*OnlyIn)) return 0.f;
		const FWRWeatherNow* W = UWRWeatherSubsystem::For(GetWorld(), Here);
		if (!W || W->Kind != Kind) return 0.f;
		const float Now = UWRWeatherSubsystem::Now(GetWorld());
		return FMath::Max(W->Strength(Now), W->Approach(Now) * 0.6f) * FMath::Lerp(1.f, Tune(TEXT("ambient.weatherInCave"), 0.15f), Cave);
	}
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
	// Under the water every bed and every tune goes thick and low (the underwater bed itself stays clear).
	// In the Desert's storm belt (or a sandstorm) the tunes go low under the roar (never muted: the music rule).
	const float Thick = FMath::Max(UnderBlend, BeltBlend * Tune(TEXT("ambient.beltLowPassShare"), 0.8f));
	C->SetLowPassFilterEnabled(Thick > 0.01f);
	if (Thick > 0.01f) C->SetLowPassFilterFrequency(FMath::Lerp(20000.f, UnderBlend >= BeltBlend ? Tune(TEXT("ambient.underwaterLowPass"), 700.f) : Tune(TEXT("ambient.beltLowPass"), 1200.f), Thick));
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
	// Storm peaks: its air north of the island, its sub-zones' beds eased over 3 s as you cross (they blend 60-120 m in
	// the land itself), the Rodfield's aeolian wind-song within 250 m of the rods.
	Storm = FMath::SmoothStep(0.f, 1.f, float((WRTune(TEXT("world.islandNorthEdge"), -201600.f) + Tune(TEXT("ambient.stormBlendFrom"), 10000.f) - Listener.Y) / Tune(TEXT("ambient.stormBlend"), 30000.f)));
	// The Swamp lies north of the island too (east of the Crown): its air takes over from the peaks' there.
	UpdateSwamp(DeltaTime);
	Storm *= 1.f - Swamp;
	{
		const TCHAR Z = WRStorm::ZoneAt(World, Listener);
		for (int32 i = 1; i <= 6; ++i)
		{
			const float Want = (Z == TCHAR('0' + i)) ? Storm : 0.f;
			StormZone[i] = FMath::FInterpConstantTo(StormZone[i], Want, DeltaTime, 1.f / 3.f);
		}
		Rodfield = Storm * (1.f - Smooth(Tune(TEXT("ambient.rodfieldNear"), 12000.f), Tune(TEXT("ambient.rodfieldFar"), 25000.f), FVector::Dist2D(Listener, FVector(-115000.f, -315000.f, 0.f))));
	}
	// The Ancient forest lies south of the island: its air, its sub-zones, the Great Falls, the Hollow Root.
	UpdateForest(DeltaTime);
	if (Now >= NextCaveScan)
	{
		NextCaveScan = Now + 5.0;
		Caves.Reset();
		for (TActorIterator<AWRCave> It(World); It; ++It) Caves.Add(*It);
	}
	// Inside a portal's fold (its bay hangs high over the sea east of the island): its own land's colour, no high wind,
	// and the closed stone of a cave round you (WRPortals).
	const FName FoldLand = WRPortals::FoldBiome(World, Listener);
	if (!FoldLand.IsNone())
	{
		Tundra = FoldLand == TEXT("tundra") ? 1.f : 0.f;
		High = 0.f;
	}
	Cave = FMath::FInterpConstantTo(Cave, FoldLand.IsNone() ? CaveFactor() : 1.f, DeltaTime, 1.f / Tune(TEXT("ambient.caveFade"), 1.5f));
	{
		const APlayerController* UPC = World->GetFirstPlayerController();
		const APawn* UMe = UPC ? UPC->GetPawn() : nullptr;
		UnderBlend = FMath::FInterpConstantTo(UnderBlend, UMe && WRUnderwater::IsUnder(UMe) ? 1.f : 0.f, DeltaTime, 1.f / Tune(TEXT("ambient.underwaterFade"), 0.4f));
		float BeltVision = 0.f;
		BeltBlend = FMath::FInterpConstantTo(BeltBlend, WRDesertHz::BeltAmount(UMe, BeltVision), DeltaTime, 0.5f);
	}
	// A runtime land (the Corrupted wasteland): its own beds ("land.<biome>", "zone.<id>"), none of the island's; and its
	// planned silence (a weather row with "silence": Dead calm) fading every bed and the music out over its seconds.
	{
		const FWRLandDef* LD = WRLand::At(Listener);
		LandBiome = LD ? LD->Biome : NAME_None;
		LandZone = WRLand::ZoneAt(Listener);
		IsleZone = LD ? NAME_None : WRIsle::ZoneAt(Listener);
		// The Sky (cooks/biome-sky.md 2.12): over the islands as on the cape, its own beds by sub-zone (zone.W1..W5).
		if (WRSkyIsles::Contains(Listener))
		{
			LandBiome = WRSkyIsles::Biome();
			const TCHAR Z = WRSkyIsles::ZoneAt(Listener);
			LandZone = Z >= TEXT('1') && Z <= TEXT('5') ? FName(*FString::Printf(TEXT("W%c"), Z)) : FName(TEXT("Wf"));
		}
		float Want = 0.f;
		if (LD)
		{
			const FWRWeatherNow* W = UWRWeatherSubsystem::For(World, LD->Biome);
			const FWRWeatherKind* K = W ? FWRWeatherDefs::Get().Kind(LD->Biome, W->Kind) : nullptr;
			if (K && K->bSilence) Want = W->Strength(UWRWeatherSubsystem::Now(World));
		}
		// The Silent Vault (cooks/biome-crystal.md 2.5): the one planned silence of the caves, no music and no beds.
		if (WRCrystal::InVault(Listener)) Want = 1.f;
		// The Fungal caverns: under the plateau, its cavern zone; on it, F1. Stepping onto the Fall Field the music fades
		// over 4 s and leaves only the Flue's roar for 20 s, then the Fall Field's own cue (cooks/biome-fungal.md 2.16).
		float FadeSeconds = Tune(TEXT("ambient.silenceFade"), 8.f);
		FungalZone = NAME_None;
		if (WRFungal::Live())
		{
			if (WRFungal::InCave(Listener)) FungalZone = WRFungal::ZoneAt(Listener);
			else if (LandBiome == WRFungal::Biome()) FungalZone = TEXT("F1");
			if (!FungalZone.IsNone()) { LandBiome = WRFungal::Biome(); LandZone = FungalZone; }
			const bool bField = WRFungal::OnFallField(Listener);
			if (bField && FallFieldSince < 0.0) FallFieldSince = Now;
			if (!bField) FallFieldSince = -1.0;
			FlueRoar = FMath::FInterpConstantTo(FlueRoar, bField ? 1.f : 0.f, DeltaTime, 0.5f);
			if (bField && Now - FallFieldSince < Tune(TEXT("ambient.fallFieldSilence"), 20.f))
			{
				Want = 1.f;
				FadeSeconds = Tune(TEXT("ambient.fallFieldFade"), 4.f);
			}
		}
		Hush = FMath::FInterpConstantTo(Hush, Want, DeltaTime, 1.f / FMath::Max(0.5f, FadeSeconds));
	}
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

	// Spot sounds (ambient.spots): the owls at night, the crater's breath, the lake ice singing...
	UpdateSpots(Now);
}

void UWRAudioDirector::UpdateSwamp(float DeltaTime)
{
	// Where the listener is in the Swamp (cooks/biome-swamp.md 2.2): its air eased over a few seconds as you cross the
	// 60-120 m blend, each sub-zone's bed eased over 3 s, the five red falls' roar rising toward the Weeping Wall, the
	// Sunken Coven's rooms, bleedwater round you (the leeches' low hum), the Red Tide's boil in M3-M5.
	UWorld* World = GetWorld();
	const FWRSwampData& D = FWRSwampData::Get();
	const bool bLive = WRSwamp::IsLive();
	const bool bIn = bLive && WRSwamp::Contains(Listener);
	Swamp = FMath::FInterpConstantTo(Swamp, bIn ? 1.f : 0.f, DeltaTime, 1.f / FMath::Max(Tune(TEXT("ambient.swampFade"), 4.f), 0.1f));
	TCHAR Z = bIn ? D.ZoneAt(Listener) : TEXT('~');
	if (Z == TEXT('f')) Z = TEXT('4');   // the frost border is Murk Pools' cypress bog, iced
	for (int32 i = 1; i <= 5; ++i)
	{
		SwampZone[i] = FMath::FInterpConstantTo(SwampZone[i], Z == TCHAR('0' + i) ? Swamp : 0.f, DeltaTime, 1.f / 3.f);
	}
	if (!bLive && Swamp <= 0.f && SwampCoven <= 0.f)
	{
		SwampFalls = SwampBleed = SwampTide = 0.f;
		return;
	}
	float Fall = 0.f;
	for (const FWRSwampData::FFall& F : D.Falls)
	{
		Fall = FMath::Max(Fall, 1.f - Smooth(Tune(TEXT("ambient.swampFallsNear"), 6000.f), Tune(TEXT("ambient.swampFallsFar"), 60000.f), FVector::Dist2D(F.Top, Listener)));
	}
	SwampFalls = Swamp * Fall;
	bool bCoven = false;
	const float Margin = Tune(TEXT("ambient.swampCovenMargin"), 600.f);
	for (const FWRSwampData::FRoom& R : D.CovenRooms)
	{
		const FVector Half = R.Size * 0.5f + FVector(Margin);
		const FVector Off = (Listener - R.At).GetAbs();
		if (Off.X <= Half.X && Off.Y <= Half.Y && Off.Z <= Half.Z) { bCoven = true; break; }
	}
	SwampCoven = FMath::FInterpConstantTo(SwampCoven, bLive && bCoven ? 1.f : 0.f, DeltaTime, 1.f / 1.5f);
	const AWRSwampState* State = bLive ? AWRSwampState::Get(World) : nullptr;
	const bool bTide = State && State->IsTide();
	// Bleedwater under you and round you (12 m): the water grid as drawn, plus the tide's turned murk in M3-M5.
	int32 Bleed = 0;
	const float R12 = Tune(TEXT("ambient.swampBleedReach"), 1200.f);
	const FVector Probe[5] = { Listener, Listener + FVector(R12, 0, 0), Listener - FVector(R12, 0, 0), Listener + FVector(0, R12, 0), Listener - FVector(0, R12, 0) };
	for (const FVector& P : Probe)
	{
		const EWRSwampWater W = bIn ? D.BaseWaterAt(P) : EWRSwampWater::Dry;
		const TCHAR PZ = D.ZoneAt(P);
		if (W == EWRSwampWater::Bleedwater || (bTide && (W == EWRSwampWater::Murk || W == EWRSwampWater::Duckweed) && (PZ == TEXT('3') || PZ == TEXT('4') || PZ == TEXT('5')))) ++Bleed;
	}
	SwampBleed = FMath::FInterpConstantTo(SwampBleed, Swamp * Bleed / 5.f, DeltaTime, 0.5f);
	// The Red Tide: the leech boil over the turned water (M3-M5), a faint one elsewhere in the swamp; half while it warns.
	const float TideOn = !State ? 0.f : State->RedTide == EWRRedTide::Tide ? 1.f : State->RedTide == EWRRedTide::Warning ? 0.4f : 0.f;
	const float TideZone = (Z == TEXT('3') || Z == TEXT('4') || Z == TEXT('5')) ? 1.f : 0.3f;
	SwampTide = FMath::FInterpConstantTo(SwampTide, Swamp * TideOn * TideZone, DeltaTime, 0.25f);
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
		float Threat = 0.f;
		bool bAny = false;
		if (Me)
		{
			for (TActorIterator<AWRCreature> It(World); It; ++It)
			{
				AWRCreature* C = *It;
				if (!C->IsAlive() || C->IsTamed() || C->IsUnconscious()) continue;
				const float D2 = FVector::DistSquared(C->GetActorLocation(), Me->GetActorLocation());
				if (AWRBoss* B = Cast<AWRBoss>(C))
				{
					// As the HUD's boss bar: an engaged boss close by. The music follows it (its fall: the stinger and the hush).
					if (B->bEngaged && D2 < FMath::Square(BossRange)) { bBoss = true; BossId = B->CreatureId; BossPhase = B->Phase; MusicBoss = B; }
					continue;
				}
				const AActor* Target = C->GetAggroRep();
				if (Target && (Target == Me || Target == Mount) && D2 < FMath::Square(Range))
				{
					// A fight worth the music (BotW): a lone critter isn't, a wolf is (cookpass-audio 4.1.3).
					Threat += ThreatOf(C);
					bAny = true;
				}
			}
		}
		bEngaged = Threat >= Tune(TEXT("music.combatThreat"), 1.f)
			|| (bAny && Now - HitAt < Tune(TEXT("music.hitHold"), 4.f))
			|| Now - PlayerHitAt < Tune(TEXT("music.pvpHold"), 10.f);
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
	// A land's own pools first (music.pools <pool>.<biome>; cooks/biome-lightning.md 2.12): its bosses' (boss.<id>, its
	// minis' mini.<biome>), its places' (the Hanging Monastery, the camps, Stormgut), then its explore / night / combat.
	const TArray<FWRMusicTrack>* Pool = nullptr;
	const FName Land = Which == EMusic::Menu ? NAME_None : WRBiome::At(World, Listener);
	if (!Land.IsNone())
	{
		TArray<FName> Try;
		if (Which == EMusic::Boss && !BossId.IsNone())
		{
			// A boss with music per phase (boss.<id>.p<n>: Vharra's march, flood and blood) plays the phase it's in.
			Try.Add(FName(*FString::Printf(TEXT("boss.%s.p%d"), *BossId.ToString(), BossPhase)));
			MusicPhase = BossPhase;
			Try.Add(FName(*FString::Printf(TEXT("boss.%s"), *BossId.ToString())));
			const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(BossId);
			if (Def && Def->Role.Contains(TEXT("mini"))) Try.Add(FName(*FString::Printf(TEXT("mini.%s"), *Land.ToString())));
		}
		if (Which == EMusic::Boss && BossId == TEXT("sylvaroth"))
		{
			// Sylvaroth's three phases each have their piece (boss.sylvaroth.p1-p3, cooks/biome-forest.md 2.16): the phase
			// it's in now comes first.
			ForestMusicPhase = ForestBossPhaseNow();
			Try.Insert(FName(*FString::Printf(TEXT("boss.sylvaroth.p%d"), ForestMusicPhase)), 0);
		}
		// Fire and Frost (cooks/fire-frost.md 3.6): the hidden tubes' own cues (the Kiln Roads, the Blue Below), then the
		// sub-zone C pieces (V4 the Solfatara; T4 the Ice Shelves and T5 the Glacier Plateau), then the land's pools.
		if ((Which == EMusic::Explore) && (Land == TEXT("volcanic") || Land == TEXT("tundra")))
		{
			if (!Tube.IsNone()) Try.Add(FName(*FString::Printf(TEXT("sub.%s"), *Tube.ToString())));
			if (PoolName == TEXT("explore"))
			{
				if (IsleZone == TEXT("V4")) Try.Add(TEXT("explore.volcanic.c"));
				if (IsleZone == TEXT("T4") || IsleZone == TEXT("T5")) Try.Add(TEXT("explore.tundra.c"));
			}
		}
		if (Which == EMusic::Explore)
		{
			const TCHAR Z = WRStorm::ZoneAt(World, Listener);
			if (Z == TCHAR('5')) Try.Add(TEXT("sub.monastery"));
			for (const FWRPoi& P : FWRPoi::All) if (P.Kind == TEXT("camp") && FVector::Dist2D(P.Location, Listener) < Tune(TEXT("music.campRange"), 6000.f)) { Try.Add(FName(*FString::Printf(TEXT("camp.%s"), *Land.ToString()))); break; }
			if (FVector::Dist2D(Listener, FVector(-30000.f, -490000.f, 0.f)) < Tune(TEXT("music.stormgutRange"), 60000.f) && Listener.X > -70000.f) Try.Add(TEXT("pass.stormgut"));
			// The Swamp (cooks/biome-swamp.md 2.16): the Sunken Coven, its camps, then explore A (Reed Flats, Mangroves),
			// B (Boardwalks, Murk Pools) or C (Bone Mounds) by day; night has its own pool.
			if (Land == TEXT("swamp"))
			{
				if (SwampCoven > 0.5f) Try.Add(TEXT("sub.sunken_coven"));
				for (const FVector& C : FWRSwampData::Get().Camps)
				{
					if (FVector::Dist2D(C, Listener) < Tune(TEXT("music.campRange"), 6000.f)) { Try.Add(TEXT("camp.swamp")); break; }
				}
				if (PoolName == TEXT("explore"))
				{
					const TCHAR SZ = FWRSwampData::Get().ZoneAt(Listener);
					Try.Add(SZ == TEXT('1') || SZ == TEXT('2') ? FName(TEXT("explore.swamp.a")) : SZ == TEXT('5') ? FName(TEXT("explore.swamp.c")) : FName(TEXT("explore.swamp.b")));
				}
			}
			// The Ancient forest (cooks/biome-forest.md 2.16): the Hollow Root's cue down there, the newcomer village's and
			// the camps' warm pieces, then explore A (F1 Meadows, F2 Streams), B (F3 Villages, F4 Old-Growth) or C (F5 the
			// Elder Canopy) by day; night has its own pool (night.forest).
			if (Land == TEXT("forest"))
			{
				if (ForestZone[6] > 0.5f) Try.Add(TEXT("sub.hollow_root"));
				const FWRForestData& FD = FWRForestData::Get();
				if (FD.VillageRadius > 0.f && !FD.Village.IsZero() && FVector::Dist2D(FD.Village, Listener) < FD.VillageRadius) Try.Add(TEXT("village.forest"));
				for (const FWRForestData::FSite& S : FD.Sites)
				{
					if (S.Kind == TEXT("camp") && FVector::Dist2D(S.At, Listener) < Tune(TEXT("music.campRange"), 6000.f)) { Try.Add(TEXT("camp.forest")); break; }
				}
				if (PoolName == TEXT("explore"))
				{
					const TCHAR FZ = WRForest::ZoneAt(World, Listener);
					Try.Add(FZ == TEXT('3') || FZ == TEXT('4') ? FName(TEXT("explore.forest.b")) : FZ == TEXT('5') || FZ == TEXT('s') ? FName(TEXT("explore.forest.c")) : FName(TEXT("explore.forest.a")));
				}
			}
		}
		// The Fungal caverns (cooks/biome-fungal.md 2.16): the Heart Grotto's piano, the Fall Field's cue after its silence,
		// the camps', then explore by zone: the plateau (by day; its night pool by night), A the Spore Gardens, B the
		// Glow-moss Halls, C the Mycelium Webs, D the Flooded Grottos (underground they play day and night).
		if (Land == WRFungal::Biome() && Which == EMusic::Explore)
		{
			if (WRFungal::InHeartGrotto(Listener)) Try.Add(TEXT("sub.heart_grotto"));
			if (WRFungal::OnFallField(Listener)) Try.Add(TEXT("sub.fall_field"));
			for (const FWRFungalData::FSite& S : FWRFungalData::Get().Sites)
			{
				if (S.bCamp && FVector::Dist2D(S.At, Listener) < Tune(TEXT("music.campRange"), 6000.f)) { Try.Add(TEXT("camp.fungal")); break; }
			}
			const FName FZ = WRFungal::InCave(Listener) ? WRFungal::ZoneAt(Listener) : FName(TEXT("F1"));
			if (FZ == TEXT("F1")) Try.Add(PoolName == TEXT("night") ? FName(TEXT("night.fungal")) : FName(TEXT("explore.fungal.surface")));
			else Try.Add(FZ == TEXT("F2") ? FName(TEXT("explore.fungal.a")) : FZ == TEXT("F3") ? FName(TEXT("explore.fungal.b")) : FZ == TEXT("F4") ? FName(TEXT("explore.fungal.c")) : FName(TEXT("explore.fungal.d")));
		}
		// The Sky: inside the Undercloud its own intimate piece (sub.undercloud, cooks/biome-sky.md 2.12), not in a fight.
		if (Land == WRSkyIsles::Biome() && PoolName != TEXT("combat") && WRSkyIsles::InUndercloud(Listener)) Try.Add(TEXT("sub.undercloud"));
		// The Ocean (cooks/biome-ocean.md 2.16): the Drowned Hall's own cue, a camp's, the pass's (Saltgate), then by sub-zone.
		if (WROcean::Live() && Land == WROcean::Biome() && PoolName != TEXT("combat"))
		{
			if (WROcean::InHall(Listener)) Try.Add(TEXT("sub.drowned_hall"));
			for (const FWROceanData::FPlace& S : FWROceanData::Get().Sites)
			{
				if (S.bCamp && FVector::Dist2D(S.At, Listener) < Tune(TEXT("music.campRange"), 6000.f)) { Try.Add(TEXT("camp.ocean")); break; }
			}
			if (WROcean::ZoneAt(Listener) == TEXT("O3") && Listener.X < 110000.f) Try.Add(TEXT("pass.saltgate"));
			if (PoolName == TEXT("explore"))
			{
				const FName OZ = WROcean::ZoneAt(Listener);
				Try.Add(OZ == TEXT("O1") || OZ == TEXT("O2") ? FName(TEXT("explore.ocean.a")) : OZ == TEXT("O3") ? FName(TEXT("explore.ocean.b")) : FName(TEXT("explore.ocean.c")));
			}
		}
		// The Desert (cooks/biome-desert.md 2.16): the Hourhold's own cue, the Wall's storm piece, a camp's, the pass's
		// (Saltgate's west mouth), then by sub-zone.
		if (WRDesert::Live() && Land == WRDesert::Biome() && PoolName != TEXT("combat"))
		{
			if (WRDesert::InHourhold(Listener)) Try.Add(TEXT("sub.hourhold"));
			if (WRDesert::InBelt(Listener)) Try.Add(TEXT("sub.haboob_wall"));
			for (const FWRDesertData::FPlace& S : FWRDesertData::Get().Sites)
			{
				if (S.bCamp && FVector::Dist2D(S.At, Listener) < Tune(TEXT("music.campRange"), 6000.f)) { Try.Add(TEXT("camp.desert")); break; }
			}
			const FName DZ = WRDesert::ZoneAt(Listener);
			if (Listener.X > FWRTuning::Get().Num(TEXT("desertWorld.saltgateMusicX"), -60000.f)) Try.Add(TEXT("pass.saltgate"));
			if (PoolName == TEXT("explore")) Try.Add(DZ == TEXT("D1") || DZ == TEXT("D2") ? FName(TEXT("explore.desert.a")) : DZ == TEXT("D3") ? FName(TEXT("explore.desert.b")) : FName(TEXT("explore.desert.c")));
		}
		Try.Add(FName(*FString::Printf(TEXT("%s.%s"), *PoolName.ToString(), *Land.ToString())));
		for (const FName& T : Try)
		{
			const TArray<FWRMusicTrack>* P = Audio->GetMusicPool(T);
			if (P && P->Num() > 0) { Pool = P; PoolName = T; break; }
		}
	}
	if (!Pool) Pool = Audio->GetMusicPool(PoolName);
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
	S.Bpm = Track.Bpm;
	S.Beats = Track.Beats;
	S.Lead = Track.Lead;
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
	// The bar clock: each piece's own time heard (a paused game pauses the music too).
	{
		const double RealDt = LastRealTime > 0.0 ? FMath::Clamp(Now - LastRealTime, 0.0, 0.5) : 0.0;
		LastRealTime = Now;
		if (!World->IsPaused())
		{
			for (FStream& S : Music) S.Played += RealDt;
		}
	}
	// Dead (or between lives): the music dips by the cap and no new piece starts until you're back.
	const APawn* DeadPawn = MyPawn.Get();
	const AWRCharacterBase* DeadBody = Cast<AWRCharacterBase>(DeadPawn);
	const bool bDead = !SWRMainMenu::IsOpen() && (!DeadPawn || (DeadBody && !DeadBody->IsAlive()));
	DeadDip = FMath::FInterpConstantTo(DeadDip, bDead ? 1.f - Tune(TEXT("music.duckMax"), 0.37f) : 1.f, DeltaTime, 0.5f);

	// A change of mode waits for the playing piece's beat (into a fight) or bar line (anything else): cookpass-audio 4.1.4.
	const EMusic Want = WantedMusic();
	if (Want == Mode)
	{
		bPending = false;
	}
	else
	{
		if (!bPending || PendingMode != Want)
		{
			bPending = true;
			PendingMode = Want;
			float Wait = 0.f;
			const FStream* Cur = Music.Num() > 0 && Music.Last().Target > 0.f ? &Music.Last() : nullptr;
			if (Cur && Mode != EMusic::Menu && Want != EMusic::Menu && ForcedMode.IsNone())
			{
				Wait = NextBoundary(*Cur, !(Want == EMusic::Combat || Want == EMusic::Boss));
			}
			PendingAt = Now + Wait;
		}
		if (Now >= PendingAt)
		{
			bPending = false;
			SwitchMode(Want, Now);
		}
	}
	if (Mode == EMusic::Boss && BossPhase != MusicPhase && !BossId.IsNone()
		&& UWRAudioSubsystem::Get(World)->GetMusicPool(FName(*FString::Printf(TEXT("boss.%s.p%d"), *BossId.ToString(), BossPhase))))
	{
		// The boss turned a phase and has music for it: on the old piece's next bar line the old phase's piece fades as
		// the new one comes in (fire-frost 3.6: boss sections resequence on the bar).
		if (PhaseSwitchAt <= 0.0)
		{
			const FStream* Cur = Music.Num() > 0 && Music.Last().Target > 0.f ? &Music.Last() : nullptr;
			PhaseSwitchAt = Now + (Cur ? NextBoundary(*Cur, true) : 0.f);
		}
		if (Now >= PhaseSwitchAt)
		{
			PhaseSwitchAt = 0.0;
			for (FStream& S : Music)
			{
				S.Target = 0.f;
				S.FadeSeconds = Tune(TEXT("music.combatFadeOut"), 3.f);
			}
			StartMusic(EMusic::Boss);
		}
	}
	else
	{
		PhaseSwitchAt = 0.0;
	}
	MusicPhase = Mode == EMusic::Boss ? MusicPhase : BossPhase;
	// Fades waiting for their bar line.
	for (FStream& S : Music)
	{
		if (S.FadeAt > 0.0 && Now >= S.FadeAt)
		{
			S.FadeAt = 0.0;
			S.Target = 0.f;
		}
	}
	if (Mode == EMusic::Explore)
	{
		FStream* Current = Music.Num() > 0 && Music.Last().Target > 0.f ? &Music.Last() : nullptr;
		if (!Current && Now >= NextExploreAt && Now >= HushUntil && !bDead)
		{
			StartMusic(EMusic::Explore);
		}
		else if (Current && Current->FadeAt <= 0.0)
		{
			const bool bOver = !Current->Comp.IsValid() || !Current->Comp->IsPlaying();
			if (bOver || Now - Current->StartedAt > Tune(TEXT("music.exploreMaxSeconds"), 240.f))
			{
				// The piece ends (or, a long one, on a bar line after a while), then a breath and the next: the score is
				// always there (FULL-GAME 3.3: no engine silences; music.gap is a breath of a few seconds).
				if (bOver) Current->Target = 0.f;
				else FadeOnBar(*Current, Tune(TEXT("music.fadeOut"), 4.f));
				Current->FadeSeconds = Tune(TEXT("music.fadeOut"), 4.f);
				const double From = Current->FadeAt > 0.0 ? Current->FadeAt : Now;
				NextExploreAt = From + FMath::FRandRange(Tune(TEXT("music.gap.min"), 4.f), Tune(TEXT("music.gap.max"), 10.f));
			}
		}
	}

	// Ducking under big moments: dips fast, comes back slowly.
	if (Now > DuckUntil)
	{
		DuckTarget = 1.f;
	}
	DuckLevel = FMath::FInterpTo(DuckLevel, DuckTarget, DeltaTime, DuckTarget < DuckLevel ? 10.f : 1.2f);
	// The planned silence takes the music too (8 s out, 8 s back); a fight inside it keeps half (biome-void 2.9, 2.16).
	const float Silence = Mode == EMusic::Combat || Mode == EMusic::Boss ? Hush * 0.5f : Hush;
	const float Gain = WRSound::CategoryGain(TEXT("music")) * DuckLevel * DeadDip * (1.f - Silence);
	// Fire and Frost's events (cooks/fire-frost.md 3.6): the Eruption's war drums and the Whiteout's low pulse over whatever
	// plays in their land, from the warning (five minutes ahead) to the end.
	{
		const AWRWorldState* WS = AWRWorldState::Get(World);
		const FName HereLand = WRBiome::At(World, Listener);
		const auto EventOn = [WS, &HereLand](const TCHAR* Id)
		{
			const FWRWorldEventRep* E = WS ? WS->Event(FName(Id)) : nullptr;
			return E && E->Stage <= 1 && E->Biome == HereLand;
		};
		StepMusicLayer(EruptionLayer, EventOn(TEXT("eruption")), TEXT("layer.eruption"), DeltaTime, Gain);
		StepMusicLayer(WhiteoutLayer, EventOn(TEXT("whiteout")), TEXT("layer.whiteout"), DeltaTime, Gain);
	}
	// The Great Storm: war drums and choir over whatever plays while it rages round you (DC 2.15).
	StepMusicLayer(GreatStormLayer, WRStorm::GreatStorm(World) && WRBiome::At(World, Listener) == TEXT("storm_peaks"), TEXT("layer.great_storm"), DeltaTime, Gain);
	// The Red Tide: a sub-bass pulse and a second, faster heartbeat over whatever plays in the swamp (cooks/biome-swamp.md 2.16).
	{
		const AWRSwampState* State = Swamp > 0.f ? AWRSwampState::Get(World) : nullptr;
		StepMusicLayer(RedTideLayer, State && State->RedTide != EWRRedTide::None && Swamp > 0.5f, TEXT("layer.red_tide"), DeltaTime, Gain);
	}
	// The Ancient forest's layers over whatever plays (cooks/biome-forest.md 2.16): the Bloom's celesta, flute and fuller
	// birdsong; while the Wild Hunt rides (the Bloom's night) its galloping drums and horn take the Bloom's place.
	{
		const bool bHere = Forest > 0.5f;
		const bool bHunt = bHere && WRForest::WildHuntActive(World);
		StepMusicLayer(WildHuntLayer, bHunt, TEXT("layer.wild_hunt"), DeltaTime, Gain);
		StepMusicLayer(BloomLayer, bHere && !bHunt && WRForest::Bloom(World), TEXT("layer.bloom"), DeltaTime, Gain);
	}
	// The King Tide (cooks/biome-ocean.md 2.16): its layer over whatever plays on the coast while the sea is out.
	{
		const AWROceanState* OS = AWROceanState::Get(World);
		const APlayerController* KPC = World->GetFirstPlayerController();
		const APawn* KMe = KPC ? KPC->GetPawn() : nullptr;
		StepMusicLayer(KingTideLayer, OS && OS->KingTide != EWRKingTide::None && KMe && WROcean::Contains(KMe->GetActorLocation()), TEXT("layer.king_tide"), DeltaTime, Gain);
	}
	// The Long Noon (cooks/biome-desert.md 2.16): its layer (a held high drone) over whatever plays in the Desert.
	{
		const APlayerController* NPC = World->GetFirstPlayerController();
		const APawn* NMe = NPC ? NPC->GetPawn() : nullptr;
		StepMusicLayer(LongNoonLayer, WRDesertHz::LongNoon(World) && NMe && WRDesert::Contains(NMe->GetActorLocation()), TEXT("layer.long_noon"), DeltaTime, Gain);
	}
	// The Sporefall: the Heart pulse as a deep drum over whatever plays in the caverns (cooks/biome-fungal.md 2.16).
	StepMusicLayer(SporefallLayer, !FungalZone.IsNone() && WRFungal::Sporefall(World) >= 2, TEXT("layer.sporefall"), DeltaTime, Gain);
	// The Turning Wind: its canon over whatever plays in the Sky (the theme against itself backwards; biome-sky 2.12).
	{
		const AWRSkyState* SkyS = AWRSkyState::Get(World);
		const float SkyNow = World->GetGameState() ? World->GetGameState()->GetServerWorldTimeSeconds() : 0.f;
		StepMusicLayer(TurningWindLayer, SkyS && SkyS->SignNow(SkyNow) && WRSkyIsles::Contains(Listener), TEXT("layer.turning_wind"), DeltaTime, Gain);
	}
	// Sylvaroth turned a phase and its music hasn't followed yet (boss.sylvaroth.p<n>): after a short wait (another
	// phase-music switch may already be on it) the old phase's piece fades as the new one comes in.
	if (Mode == EMusic::Boss && BossId == TEXT("sylvaroth"))
	{
		if (Now >= NextForestPhaseCheck)
		{
			NextForestPhaseCheck = Now + 0.5;
			const int32 P = ForestBossPhaseNow();
			if (P == ForestMusicPhase)
			{
				ForestPhaseSwitchAt = 0.0;
			}
			else if (ForestPhaseSwitchAt <= 0.0)
			{
				ForestPhaseSwitchAt = Now + 0.6;
			}
			else if (Now >= ForestPhaseSwitchAt)
			{
				ForestPhaseSwitchAt = 0.0;
				const TArray<FWRMusicTrack>* PhasePool = UWRAudioSubsystem::Get(World)->GetMusicPool(FName(*FString::Printf(TEXT("boss.sylvaroth.p%d"), P)));
				if (PhasePool && PhasePool->Num() > 0)
				{
					for (FStream& S : Music)
					{
						S.Target = 0.f;
						S.FadeSeconds = Tune(TEXT("music.combatFadeOut"), 3.f);
					}
					StartMusic(EMusic::Boss);   // picks the new phase's piece and remembers the phase
				}
				else
				{
					ForestMusicPhase = P;
				}
			}
		}
	}
	else
	{
		ForestPhaseSwitchAt = 0.0;
	}
	for (int32 i = Music.Num() - 1; i >= 0; --i)
	{
		if (StepStream(Music[i], DeltaTime, Gain))
		{
			Music.RemoveAt(i);
		}
	}
}

void UWRAudioDirector::StepMusicLayer(TArray<FStream>& Layer, bool bWant, FName Pool, float DeltaTime, float Gain)
{
	UWorld* World = GetWorld();
	if (bWant && Layer.Num() == 0)
	{
		if (const TArray<FWRMusicTrack>* L = UWRAudioSubsystem::Get(World)->GetMusicPool(Pool); L && L->Num() > 0)
		{
			FStream S;
			S.Comp = UWRAudioSubsystem::Get(World)->CreateStream(World, (*L)[0].File, true);
			if (S.Comp.IsValid())
			{
				Held.Add(S.Comp.Get());
				S.File = (*L)[0].File;
				S.Base = (*L)[0].Volume;
				S.Target = 1.f;
				S.FadeSeconds = Tune(TEXT("music.fadeIn"), 4.f);
				Layer.Add(S);
			}
		}
	}
	for (int32 i = Layer.Num() - 1; i >= 0; --i)
	{
		if (!bWant) { Layer[i].Target = 0.f; Layer[i].FadeSeconds = Tune(TEXT("music.fadeOut"), 4.f) * 2.f; }
		if (StepStream(Layer[i], DeltaTime, Gain)) Layer.RemoveAt(i);
	}
}

// ---------------------------------------------------------------- the Ancient forest

void UWRAudioDirector::UpdateForest(float DeltaTime)
{
	// Where the listener is in the forest (cooks/biome-forest.md 2.2): its air eased over a few seconds as you cross the
	// neck or the Pine Sound, each sub-zone's bed eased over 3 s (they blend 60-120 m in the land itself), the Great
	// Falls' roar rising within 250 m of the scarp, and the Hollow Root's rooms under the old growth.
	UWorld* World = GetWorld();
	const bool bIn = WRForest::Contains(Listener);
	Forest = FMath::FInterpConstantTo(Forest, bIn ? 1.f : 0.f, DeltaTime, 1.f / FMath::Max(Tune(TEXT("ambient.forestFade"), 4.f), 0.1f));
	if (!bIn && Forest <= 0.f && ForestZone[6] <= 0.f)
	{
		for (float& W : ForestZone) W = 0.f;
		ForestFalls = 0.f;
		return;
	}
	const FWRForestData& D = FWRForestData::Get();
	TCHAR Z = bIn ? WRForest::ZoneAt(World, Listener) : TCHAR(0);
	if (Z == TEXT('s')) Z = TEXT('5');   // the Spine's face rises straight out of the Elder Canopy
	if (Z == TEXT('~')) Z = TEXT('1');   // wading off the meadow coast or the Pine Sound
	// The Hollow Root: its grid cells, or inside one of its rooms (with a margin, so its bed is up as you come down a way in).
	bool bHollow = Z == TEXT('h');
	if (bIn && !bHollow)
	{
		const float Margin = Tune(TEXT("ambient.forestHollowMargin"), 600.f);
		for (const FWRForestData::FRoom& R : D.Hollow)
		{
			const FVector L = FRotator(0.f, R.Yaw, 0.f).UnrotateVector(Listener - R.At);
			if (FMath::Abs(L.X) <= R.Half.X + Margin && FMath::Abs(L.Y) <= R.Half.Y + Margin && FMath::Abs(L.Z - R.Half.Z) <= R.Half.Z + Margin) { bHollow = true; break; }
		}
	}
	for (int32 i = 1; i <= 5; ++i)
	{
		ForestZone[i] = FMath::FInterpConstantTo(ForestZone[i], (!bHollow && Z == TCHAR('0' + i)) ? Forest : 0.f, DeltaTime, 1.f / 3.f);
	}
	ForestZone[6] = FMath::FInterpConstantTo(ForestZone[6], bIn && bHollow ? 1.f : 0.f, DeltaTime, 1.f / 1.5f);
	// The Great Falls: the Alder drops 45 m off the basalt scarp at (+300, +2,650) m. Distance to the falling water (lip to
	// plunge pool) of the land's fall nearest that point; full roar within 60 m, gone at 250 m.
	const FVector GreatAt(Tune(TEXT("ambient.forestFallsX"), 30000.f), Tune(TEXT("ambient.forestFallsY"), 265000.f), 0.f);
	float Dist = FVector::Dist2D(Listener, GreatAt);
	const FWRForestData::FFall* Great = nullptr;
	for (const FWRForestData::FFall& F : D.Falls)
	{
		if (!Great || FVector::Dist2D(F.Top, GreatAt) < FVector::Dist2D(Great->Top, GreatAt)) Great = &F;
	}
	if (Great && FVector::Dist2D(Great->Top, GreatAt) < 20000.f)
	{
		Dist = FVector::Dist(Listener, FMath::ClosestPointOnSegment(Listener, Great->Top, Great->Bottom));
	}
	ForestFalls = Forest * (1.f - Smooth(Tune(TEXT("ambient.forestFallsNear"), 6000.f), Tune(TEXT("ambient.forestFallsFar"), 25000.f), Dist));
}

int32 UWRAudioDirector::ForestBossPhaseNow() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 1;
	}
	const float Range = Tune(TEXT("music.bossRange"), 4500.f);
	int32 Phase = 1;
	float Best = FMath::Square(Range);
	for (TActorIterator<AWRBoss> It(World); It; ++It)
	{
		const AWRBoss* B = *It;
		if (B->CreatureId != TEXT("sylvaroth") || !B->bEngaged || !B->IsAlive()) continue;
		const float D2 = FVector::DistSquared(B->GetActorLocation(), Listener);
		if (D2 < Best) { Best = D2; Phase = FMath::Clamp(B->Phase, 1, 3); }
	}
	return Phase;
}

// ---------------------------------------------------------------- bodies

void UWRAudioDirector::OnHitShown(const AActor* Target, float Damage, EWRHitResult Result)
{
	const AWRCharacterBase* C = Cast<AWRCharacterBase>(Target);
	if (C && C->GetWorld() == GetWorld() && Result != EWRHitResult::Ignored && GetWorld())
	{
		// The music's view of a fight (cookpass-audio 4.1.3): a blow on you, and any blow between you and another player.
		const double Now = GetWorld()->GetRealTimeSeconds();
		const APawn* Me = MyPawn.Get();
		const AActor* Source = C->HitSourceShown.Get();
		if (Me && C == Me)
		{
			HitAt = Now;
			if (Source && Source != Me && Source->IsA<AWRCharacter>()) PlayerHitAt = Now;
		}
		else if (Me && Source == Me && C->IsA<AWRCharacter>())
		{
			PlayerHitAt = Now;
		}
	}
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
			New.Mood = Creature ? Creature->MoodRep : 0;
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
				if (!Aggro && !Creature->bAsleep && !Creature->IsUnconscious() && D2 < FMath::Square(IdleRange))
				{
					WRSound::Voice(Creature, TEXT("idle"));
					QueueAnswer(Creature, Now);
				}
			}
			// Asleep: a slow breath now and then, heard only close (cookpass-audio 4.8): small animals breathe fast, big
			// ones slow. creature.<size>.sleep (or the species' or family's own row).
			if (Creature->bAsleep && !Creature->IsUnconscious() && D2 < FMath::Square(Tune(TEXT("ambient.sleepRange"), 1200.f)))
			{
				if (Now >= B->NextBreath)
				{
					if (B->NextBreath > 0.0) WRSound::Voice(Creature, TEXT("sleep"));
					const FWRCreatureDef* SDef = Creature->GetDef();
					const float SizeCm = SDef ? FMath::Max(SDef->Radius, SDef->HalfHeight * 0.8f) : 50.f;
					const float Every = SizeCm < 35.f ? Tune(TEXT("ambient.sleepEvery.small"), 2.2f) : SizeCm < 70.f ? Tune(TEXT("ambient.sleepEvery.medium"), 3.4f) : Tune(TEXT("ambient.sleepEvery.large"), 5.5f);
					B->NextBreath = Now + Every * FMath::FRandRange(0.8f, 1.2f);
				}
			}
			else
			{
				B->NextBreath = 0.0;
			}
			// Its mood's calls (cookpass-creatures 3.4): the alarm when it turns wary of someone (not too often), a cry as
			// it bolts.
			const uint8 Mood = Creature->MoodRep;
			if (Mood != B->Mood && !Boss)
			{
				if (Mood == static_cast<uint8>(EWRCreatureMood::Wary) && Now >= B->NextAlert)
				{
					WRSound::Voice(Creature, TEXT("alert"));
					B->NextAlert = Now + Tune(TEXT("ambient.alertEvery"), 8.f);
				}
				else if (Mood == static_cast<uint8>(EWRCreatureMood::Flee))
				{
					WRSound::Voice(Creature, TEXT("flee"));
				}
			}
			B->Mood = Mood;
		}
		B->bFalling = bFalling;
	}
	for (auto It = Bodies.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid()) It.RemoveCurrent();
	}
	for (int32 i = Answers.Num() - 1; i >= 0; --i)
	{
		if (Now < Answers[i].Value)
		{
			continue;
		}
		if (AWRCreature* Mate = Answers[i].Key.Get(); Mate && Mate->IsAlive() && !Mate->bAsleep)
		{
			WRSound::Voice(Mate, TEXT("answer"));
		}
		Answers.RemoveAt(i);
	}
}

void UWRAudioDirector::QueueAnswer(AWRCreature* Caller, double Now)
{
	// Contact calls (cooks/cookpass-creatures.md 3.4): a packmate of the same kind in earshot, awake and calm, answers
	// now and then a moment later (wolves, flocks, herds keep in touch this way).
	if (!Caller || FMath::FRand() >= Tune(TEXT("ambient.answerChance"), 0.45f))
	{
		return;
	}
	const float Radius = Tune(TEXT("ambient.answerRadius"), 2500.f);
	AWRCreature* Best = nullptr;
	float BestD2 = FMath::Square(Radius);
	for (TActorIterator<AWRCreature> It(GetWorld()); It; ++It)
	{
		AWRCreature* Mate = *It;
		if (Mate == Caller || Mate->CreatureId != Caller->CreatureId || !Mate->IsAlive() || Mate->bAsleep || Mate->IsUnconscious()
			|| Mate->GetAggroRep() || Mate->IsTamed() != Caller->IsTamed())
		{
			continue;
		}
		const float D2 = float(FVector::DistSquared(Mate->GetActorLocation(), Caller->GetActorLocation()));
		if (D2 < BestD2)
		{
			BestD2 = D2;
			Best = Mate;
		}
	}
	if (Best)
	{
		Answers.Add({ Best, Now + FMath::FRandRange(Tune(TEXT("ambient.answerDelayMin"), 0.5f), Tune(TEXT("ambient.answerDelayMax"), 1.4f)) });
		if (FBody* B = Bodies.Find(Best))
		{
			B->NextIdle = FMath::Max(B->NextIdle, Now + 6.0);   // its answer is its call for now
		}
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
	if (bPending && GetWorld()) Out += FString::Printf(TEXT(" (change in %.1fs)"), PendingAt - GetWorld()->GetRealTimeSeconds());
	if (!Space.IsNone()) Out += FString::Printf(TEXT(" space %s"), *Space.ToString());
	if (!Tube.IsNone()) Out += FString::Printf(TEXT(" tube %s"), *Tube.ToString());
	Out += FString::Printf(TEXT(" | at %.0f,%.0f,%.0f tundra %.2f day %.2f coast %.2f cave %.2f lava %.2f high %.2f | beds"), Listener.X, Listener.Y, Listener.Z,
		Tundra, Daylight, Coast, Cave, Lava, High);
	if (Forest > 0.f || ForestZone[6] > 0.f) Out += FString::Printf(TEXT(" forest %.2f zones %.2f/%.2f/%.2f/%.2f/%.2f hollow %.2f falls %.2f"), Forest,
		ForestZone[1], ForestZone[2], ForestZone[3], ForestZone[4], ForestZone[5], ForestZone[6], ForestFalls);
	if (Swamp > 0.f || SwampCoven > 0.f) Out += FString::Printf(TEXT(" swamp %.2f falls %.2f coven %.2f bleed %.2f tide %.2f"), Swamp, SwampFalls, SwampCoven, SwampBleed, SwampTide);
	for (const TPair<FName, FStream>& B : Beds)
	{
		if (B.Value.Comp.IsValid()) Out += FString::Printf(TEXT(" %s@%.2f"), *B.Key.ToString(), B.Value.Gain);
	}
	return Out;
}

// ---------------------------------------------------------------- the bar clock, mode switches, threat (cookpass-audio 4.1)

float UWRAudioDirector::NextBoundary(const FStream& S, bool bBar) const
{
	if (S.Bpm <= 1.f)
	{
		return 0.f;
	}
	const double Unit = 60.0 / S.Bpm * (bBar ? FMath::Max(1, S.Beats) : 1);
	const double T = S.Played - S.Lead;
	double Wait = T < 0.0 ? -T : Unit - FMath::Fmod(T, Unit);
	if (Wait < 0.04 || Wait > Unit - 0.001) Wait = 0.0;   // right on it
	return FMath::Min(static_cast<float>(Wait), Tune(TEXT("music.syncMax"), 4.f));
}

void UWRAudioDirector::FadeOnBar(FStream& S, float Seconds)
{
	S.FadeSeconds = Seconds;
	const float Wait = NextBoundary(S, true);
	if (Wait <= 0.f || !GetWorld())
	{
		S.Target = 0.f;
		S.FadeAt = 0.0;
		return;
	}
	S.FadeAt = GetWorld()->GetRealTimeSeconds() + Wait;
}

void UWRAudioDirector::SwitchMode(EMusic Want, double Now)
{
	UWorld* World = GetWorld();
	const EMusic Was = Mode;
	Mode = Want;
	const bool bIntoFight = Want == EMusic::Combat || Want == EMusic::Boss;
	// The boss the music followed has fallen (not just walked off): Dark Souls' breath, MH's cadence.
	const bool bFell = Was == EMusic::Boss && Want != EMusic::Boss && MusicBoss.IsValid() && !MusicBoss->IsAlive();
	for (FStream& S : Music)
	{
		S.Target = 0.f;
		S.FadeAt = 0.0;
		// We're on its beat (into a fight: the old piece releases under the stinger) or its bar line (anything else).
		S.FadeSeconds = bIntoFight ? Tune(TEXT("music.releaseIntoFight"), 2.f)
			: bFell ? Tune(TEXT("music.bossFallRelease"), 2.f)
			: Tune(Was == EMusic::Combat || Was == EMusic::Boss ? TEXT("music.combatFadeOut") : TEXT("music.fadeOut"), 4.f);
	}
	if (Want == EMusic::Explore)
	{
		// After a fight, a breath; at the start, a breath to take in the island (no engine silences: FULL-GAME 3.3).
		const bool bAfterFight = Was == EMusic::Combat || Was == EMusic::Boss;
		NextExploreAt = Now + (bAfterFight ? FMath::FRandRange(Tune(TEXT("music.afterCombatGap.min"), 4.f), Tune(TEXT("music.afterCombatGap.max"), 9.f))
			: FMath::FRandRange(Tune(TEXT("music.firstGap.min"), 3.f), Tune(TEXT("music.firstGap.max"), 8.f)));
		if (bFell)
		{
			// The boss's piece ends, its cadence plays, and the land holds its breath before its own music comes back: the
			// one engine silence left, planned (cookpass-audio 4.1.6).
			WRSound::PlayUI(World, TEXT("music.sting.boss_fall"));
			HushUntil = Now + Tune(TEXT("music.bossHush"), 12.f);
			NextExploreAt = FMath::Max(NextExploreAt, HushUntil);
		}
		if (!ForcedMode.IsNone()) NextExploreAt = Now;
	}
	else
	{
		StartMusic(Want);
		if (Want == EMusic::Combat && Was != EMusic::Boss)
		{
			// The entry hit hides the change (RDR2): the land's own drum, else the shared one.
			const FName Land = WRBiome::At(World, Listener);
			const FName Own(*FString::Printf(TEXT("music.sting.combat.%s"), *Land.ToString()));
			WRSound::PlayUI(World, WRSound::Has(Own) ? Own : FName(TEXT("music.sting.combat")));
		}
	}
	if (Want != EMusic::Boss && !bFell) MusicBoss.Reset();
	UE_LOG(LogWildreach, Log, TEXT("MUSIC mode %d -> %d%s"), static_cast<int32>(Was), static_cast<int32>(Want), bFell ? TEXT(" (the boss fell)") : TEXT(""));
}

float UWRAudioDirector::ThreatOf(const AWRCreature* C) const
{
	const FWRCreatureDef* Def = C ? C->GetDef() : nullptr;
	if (!Def)
	{
		return Tune(TEXT("music.threat.medium"), 1.f);
	}
	if (Def->Role.Contains(TEXT("boss")) || Def->Tier == TEXT("boss")) return Tune(TEXT("music.threat.mini"), 3.f);
	if (Def->Archetype == TEXT("critter")) return Tune(TEXT("music.threat.critter"), 0.25f);
	const float SizeCm = FMath::Max(Def->Radius, Def->HalfHeight * 0.8f);
	return SizeCm < 35.f ? Tune(TEXT("music.threat.small"), 0.5f) : SizeCm < 70.f ? Tune(TEXT("music.threat.medium"), 1.f) : Tune(TEXT("music.threat.large"), 1.5f);
}

FName UWRAudioDirector::TubeAt() const
{
	// The hidden sub-biomes (world.json hidden): a rock-roofed tube along its route; inside it within music.tubeReach of
	// the route and not far above or below it.
	const float Reach = Tune(TEXT("music.tubeReach"), 450.f);
	for (const TWeakObjectPtr<AActor>& Weak : Tubes)
	{
		const AWRHiddenTube* T = Cast<AWRHiddenTube>(Weak.Get());
		if (!T || T->Route.Num() < 2) continue;
		for (int32 i = 0; i + 1 < T->Route.Num(); ++i)
		{
			const FVector P = FMath::ClosestPointOnSegment(Listener, T->Route[i], T->Route[i + 1]);
			if (FVector::Dist2D(P, Listener) < Reach && FMath::Abs(Listener.Z - P.Z) < Tune(TEXT("music.tubeHeight"), 900.f))
			{
				return T->TubeId;
			}
		}
	}
	return NAME_None;
}

// ---------------------------------------------------------------- spaces (cookpass-audio 4.3)

bool UWRAudioDirector::UnderBuiltRoof() const
{
	// A built room: a piece overhead within 6 m and walls (built pieces) on at least three of four sides within 6 m.
	const UWorld* World = GetWorld();
	const APawn* Me = MyPawn.Get();
	if (!World || !Me)
	{
		return false;
	}
	FCollisionQueryParams Q(SCENE_QUERY_STAT(WRRoom), false, Me);
	const FVector Head = Listener + FVector(0.f, 0.f, 60.f);
	const float Reach = Tune(TEXT("ambient.roomReach"), 600.f);
	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, Head, Head + FVector(0.f, 0.f, Reach), ECC_Visibility, Q) || !Cast<AWRBuildPiece>(Hit.GetActor()))
	{
		return false;
	}
	int32 Walls = 0;
	const FVector Dirs[4] = { FVector(1.f, 0.f, 0.f), FVector(-1.f, 0.f, 0.f), FVector(0.f, 1.f, 0.f), FVector(0.f, -1.f, 0.f) };
	for (const FVector& D : Dirs)
	{
		if (World->LineTraceSingleByChannel(Hit, Head, Head + D * Reach, ECC_Visibility, Q) && Cast<AWRBuildPiece>(Hit.GetActor())) ++Walls;
	}
	return Walls >= 3;
}

FName UWRAudioDirector::SpaceNow() const
{
	// Highest first (the table order in cooks/cookpass-audio.md 4.3): water, the dead vault, the named halls, the
	// caverns, the tubes, a fold, a cave, a built room; open air has none.
	if (UnderBlend > 0.5f) return TEXT("underwater");
	if (WRCrystal::Live() && WRCrystal::InVault(Listener)) return TEXT("vault");
	if (WROcean::Live() && WROcean::InHall(Listener)) return TEXT("drowned_hall");
	if (SwampCoven > 0.5f) return TEXT("coven");
	if (ForestZone[6] > 0.5f) return TEXT("hollow_root");
	if (WRFungal::Live() && WRFungal::InCave(Listener))
	{
		return FungalZone == TEXT("F3") ? FName(TEXT("fungal_hall")) : FungalZone == TEXT("F4") ? FName(TEXT("fungal_web")) : FName(TEXT("fungal_cave"));
	}
	if (WRCrystal::Live() && WRCrystal::InCave(Listener)) return TEXT("crystal");
	if (!Tube.IsNone()) return TEXT("tube");
	if (!WRPortals::FoldBiome(GetWorld(), Listener).IsNone()) return TEXT("fold");
	if (Cave > 0.5f) return TEXT("cave");
	if (bRoof) return TEXT("room");
	return NAME_None;
}

void UWRAudioDirector::UpdateSpace()
{
	UWorld* World = GetWorld();
	UWRAudioSubsystem* Audio = UWRAudioSubsystem::Get(World);
	if (!World || !Audio)
	{
		return;
	}
	const double Now = World->GetRealTimeSeconds();
	if (Now >= NextTubeScan)
	{
		NextTubeScan = Now + 5.0;
		Tubes.Reset();
		for (TActorIterator<AWRHiddenTube> It(World); It; ++It) Tubes.Add(*It);
	}
	Tube = TubeAt();
	if (Now >= NextRoofCheck)
	{
		NextRoofCheck = Now + 0.5;
		bRoof = UnderBuiltRoof();
	}
	const FName Want = SWRMainMenu::IsOpen() ? NAME_None : SpaceNow();
	if (Want == Space)
	{
		return;
	}
	Space = Want;
	const FWRSpaceDef* Def = Space.IsNone() ? nullptr : Audio->FindSpace(Space);
	UReverbEffect* Effect = Def && !Def->bDry ? Audio->SpaceEffect(Space) : nullptr;
	if (Effect)
	{
		UGameplayStatics::ActivateReverbEffect(World, Effect, TEXT("WRSpace"), 1.f, FMath::Clamp(Def->Wet, 0.f, 1.f), Def->Fade);
	}
	else
	{
		UGameplayStatics::DeactivateReverbEffect(World, TEXT("WRSpace"));
	}
	if (Audio->IsLogging()) Audio->Log(FString::Printf(TEXT("space %s"), Space.IsNone() ? TEXT("open air") : *Space.ToString()));
}

// ---------------------------------------------------------------- spot sounds (cookpass-audio 4.6)

void UWRAudioDirector::UpdateSpots(double Now)
{
	UWorld* World = GetWorld();
	const UWRAudioSubsystem* Audio = UWRAudioSubsystem::Get(World);
	if (!Audio || SWRMainMenu::IsOpen() || Hush > 0.5f)
	{
		return;
	}
	for (const FWRAmbientSpot& Spot : Audio->GetSpots())
	{
		double& Next = SpotNext.FindOrAdd(Spot.Id, 0.0);
		if (Next <= 0.0)
		{
			// First seen: somewhere in its first interval, so spots don't all fire together on arrival.
			Next = Now + FMath::FRandRange(Spot.Every.X * 0.3f, Spot.Every.Y);
			continue;
		}
		if (Now < Next)
		{
			continue;
		}
		Next = Now + FMath::FRandRange(Spot.Every.X, FMath::Max(Spot.Every.X, Spot.Every.Y));
		float W = 0.f;
		for (const FName& L : Spot.Layers) W = FMath::Max(W, LayerWeight(L));
		if (Spot.DayNight == 1) W *= Daylight;
		if (Spot.DayNight == 2) W *= 1.f - Daylight;
		if (W < Spot.Min)
		{
			continue;
		}
		FVector Where;
		if (Spot.bAt)
		{
			// From its place (the crater itself): only within reach, on the ground there plus its height.
			const FVector Place(Spot.At.X * 100.f, Spot.At.Y * 100.f, Listener.Z);
			if (FVector::Dist2D(Place, Listener) > Spot.Reach * 100.f) continue;
			FHitResult Hit;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(WRSpot), false);
			const float Ground = World->LineTraceSingleByChannel(Hit, Place + FVector(0.f, 0.f, 60000.f), Place - FVector(0.f, 0.f, 60000.f), ECC_WorldStatic, Q) ? Hit.ImpactPoint.Z : Listener.Z;
			Where = FVector(Place.X, Place.Y, Ground + FMath::FRandRange(Spot.Up.X, Spot.Up.Y) * 100.f);
		}
		else
		{
			const float A = FMath::FRandRange(0.f, 2.f * PI);
			const float D = FMath::FRandRange(Spot.Dist.X, FMath::Max(Spot.Dist.X, Spot.Dist.Y)) * 100.f;
			Where = Listener + FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, FMath::FRandRange(Spot.Up.X, Spot.Up.Y) * 100.f);
		}
		WRSound::PlayAt(World, Spot.Event, Where);
	}
}
