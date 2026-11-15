// The Graveyard's boss arenas: placing each boss with its pieces once a survivor comes near (server), and the held
// actions of its fights (see WRGraveBosses.h; cooks/build-biome-graveyard.md 2.15, 2.16):
//  - dig out: a friend the Gravedigger buried (hold E 1 s beside the mound; they mash to help);
//  - pull up: a tame Nhal's Lay to rest is dragging into the floor (hold E 2 s on it);
//  - stake: drive a heartwood stake into an open chapel tomb (hold E 1.5 s; the stake is spent, the tomb sealed).
// Each held action stops if you move more than holdDrift or what you hold on is gone.

#include "WRGraveBosses.h"
#include "Wildreach.h"
#include "WRAltar.h"
#include "WRBoss.h"
#include "WRCharacter.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRGraveTypes.h"
#include "WRInventory.h"
#include "WRPlayerController.h"
#include "WRSound.h"
#include "WRTuning.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace WRGraveArenaImpl
{
	float GaT(const TCHAR* Key, float Fallback) { return FWRTuning::Get().Num(FString::Printf(TEXT("bosses.graveyard.%s"), Key), Fallback); }
	bool GaGround(const UWorld* World, const FVector& At, FVector& Out, float Up, float Down)
	{
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRGraveArenaGround), false);
		if (World && World->LineTraceSingleByChannel(Hit, At + FVector(0.f, 0.f, Up), At - FVector(0.f, 0.f, Down), ECC_WorldStatic, Q) && !Cast<APawn>(Hit.GetActor()))
		{
			Out = Hit.ImpactPoint;
			return true;
		}
		Out = At;
		return false;
	}
	void GaTell(const AActor* Who, const FString& Text)
	{
		const APawn* P = Cast<APawn>(Who);
		if (AWRPlayerController* PC = P ? Cast<AWRPlayerController>(P->GetController()) : nullptr) PC->ClientNotify(Text);
	}
	/** What a survivor here could do with E (every machine: from the replicated pieces). */
	struct FGaChoice { AWRGraveBossProp* Prop = nullptr; FName How; float Seconds = 0.f; };
	FGaChoice GaChoose(const APawn* Me)
	{
		FGaChoice Out;
		if (!Me || !WRGrave::Live()) return Out;
		const FVector At = Me->GetActorLocation();
		float Best = 1e9f;
		const float Help = GaT(TEXT("helpReach"), 250.f);
		const float Seal = FWRTuning::Get().Num(TEXT("bosses.nhal.sealReach"), 200.f) + 150.f;
		const UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Me);
		const bool bStake = Inv && Inv->CountOf(TEXT("heartwood_stake")) > 0;
		for (const TWeakObjectPtr<AWRGraveBossProp>& W : FWRGraveBoss::Props())
		{
			AWRGraveBossProp* P = W.Get();
			if (!P || P->GetWorld() != Me->GetWorld()) continue;
			const float D = float(FVector::Dist2D(P->GetActorLocation(), At));
			if (FMath::Abs(P->GetActorLocation().Z - (At.Z - Me->GetSimpleCollisionHalfHeight())) > 300.f) continue;
			// (Your own mound is under your feet: you mash, a friend digs.)
			if (P->Kind == TEXT("dug") && P->State == 1 && D > 60.f && D < Help && D < Best) { Best = D; Out = { P, TEXT("help"), GaT(TEXT("digOut"), 1.f) }; }
			else if (P->Kind == TEXT("dug") && P->State == 2 && D < Help + 150.f && D < Best) { Best = D; Out = { P, TEXT("pull"), GaT(TEXT("pullUp"), 2.f) }; }
			else if (P->Kind == TEXT("tomb") && P->State == 1 && bStake && D < Seal && D < Best) { Best = D; Out = { P, TEXT("stake"), GaT(TEXT("stake"), 1.5f) }; }
		}
		return Out;
	}
}
using namespace WRGraveArenaImpl;

UWRGraveArenas* UWRGraveArenas::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UWRGraveArenas>() : nullptr;
}

bool UWRGraveArenas::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

bool UWRGraveArenas::IsTickable() const
{
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() != NM_Client && WRGrave::Live() && FWRGraveData::Get().bLoaded;
}

bool UWRGraveArenas::Prompt(const AWRPlayerController* PC, FString& OutTarget, FString& OutVerb)
{
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	const FGaChoice C = GaChoose(Me);
	if (!C.Prop) return false;
	if (C.How == TEXT("help")) { OutTarget = TEXT("Buried survivor"); OutVerb = FString::Printf(TEXT("Dig them out (hold %.0f s)"), C.Seconds); }
	else if (C.How == TEXT("pull")) { OutTarget = TEXT("Dragged under"); OutVerb = FString::Printf(TEXT("Pull it up (hold %.0f s)"), C.Seconds); }
	else { OutTarget = TEXT("Open tomb"); OutVerb = FString::Printf(TEXT("Drive a heartwood stake (hold %.1f s)"), C.Seconds); }
	return true;
}

bool UWRGraveArenas::ClientInteract(AWRPlayerController* PC)
{
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	const FGaChoice C = GaChoose(Me);
	if (!C.Prop) return false;
	PC->SendCommand(FString::Printf(TEXT("world grave boss %s"), *C.How.ToString()));
	return true;
}

FString UWRGraveArenas::Command(AWRPlayerController* PC, const TArray<FString>& Args)
{
	AWRCharacterBase* Me = PC ? Cast<AWRCharacterBase>(PC->GetPawn()) : nullptr;
	if (!Me || !Me->IsAlive()) return TEXT("Not now");
	const FString Verb = Args.IsValidIndex(3) ? Args[3] : FString();
	UWorld* World = Me->GetWorld();
	const double Now = World->GetTimeSeconds();
	for (const FHeld& H : Held) if (H.Who.Get() == Me) return TEXT("Already at it");
	FHeld H;
	H.Who = Me;
	H.From = Me->GetActorLocation();
	if (Verb == TEXT("help") || Verb == TEXT("pull"))
	{
		FName How;
		AWRCharacterBase* Who = FWRGraveBoss::HelpableNear(World, Me->GetActorLocation(), GaT(TEXT("helpReach"), 250.f) + (Verb == TEXT("pull") ? 150.f : 0.f), How);
		if (!Who || Who == Me) return TEXT("No one to dig out here");
		H.Target = Who;
		H.How = How == TEXT("laid") ? FName(TEXT("pull")) : FName(TEXT("help"));
		H.Until = Now + (H.How == TEXT("pull") ? GaT(TEXT("pullUp"), 2.f) : GaT(TEXT("digOut"), 1.f));
		WRFx::Broadcast(Me, TEXT("grave.dig_spoil"), Who->GetActorLocation() - FVector(0.f, 0.f, Who->GetSimpleCollisionHalfHeight()), FRotator::ZeroRotator, 1.f, TEXT("grave.dig"));
		Held.Add(H);
		return H.How == TEXT("pull") ? TEXT("Pulling it up... (hold still)") : TEXT("Digging them out... (hold still)");
	}
	if (Verb == TEXT("stake"))
	{
		UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Me);
		if (!Inv || Inv->CountOf(TEXT("heartwood_stake")) <= 0) return TEXT("You need a heartwood stake");
		AWRGraveBossProp* Tomb = FWRGraveBoss::OpenTombNear(World, Me->GetActorLocation(), FWRTuning::Get().Num(TEXT("bosses.nhal.sealReach"), 200.f) + 150.f);
		if (!Tomb) return TEXT("No open tomb here");
		H.Target = Tomb;
		H.How = TEXT("stake");
		H.Until = Now + GaT(TEXT("stake"), 1.5f);
		WRFx::Broadcast(Me, NAME_None, Tomb->GetActorLocation(), FRotator::ZeroRotator, 1.f, TEXT("grave.stake_drive"));
		Held.Add(H);
		return TEXT("Driving the stake in... (hold still)");
	}
	return TEXT("Not here");
}

void UWRGraveArenas::StepHeld()
{
	const UWorld* World = GetWorld();
	const double Now = World->GetTimeSeconds();
	const float Drift = GaT(TEXT("holdDrift"), 120.f);
	for (int32 i = Held.Num() - 1; i >= 0; --i)
	{
		FHeld& H = Held[i];
		AWRCharacterBase* Who = H.Who.Get();
		AActor* Target = H.Target.Get();
		if (!Who || !Who->IsAlive() || !Target)
		{
			Held.RemoveAt(i);
			continue;
		}
		if (FVector::Dist2D(Who->GetActorLocation(), H.From) > Drift)
		{
			GaTell(Who, TEXT("You let go"));
			Held.RemoveAt(i);
			continue;
		}
		if (H.How == TEXT("stake"))
		{
			AWRGraveBossProp* Tomb = Cast<AWRGraveBossProp>(Target);
			if (!Tomb || Tomb->State != 1) { Held.RemoveAt(i); continue; }   // sealed another way meanwhile
			if (Now < H.Until) continue;
			UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Who);
			if (Inv && Inv->RemoveItem(TEXT("heartwood_stake"), 1) > 0) FWRGraveBoss::SealTomb(Tomb, Who);
			else GaTell(Who, TEXT("You need a heartwood stake"));
			Held.RemoveAt(i);
			continue;
		}
		// Dig out / pull up: still held in the earth?
		FName How;
		AWRCharacterBase* Still = FWRGraveBoss::HelpableNear(World, Target->GetActorLocation(), 30.f, How);
		if (Still != Target) { Held.RemoveAt(i); continue; }
		if (Now < H.Until) continue;
		FWRGraveBoss::Freed(Cast<AWRCharacterBase>(Target));
		GaTell(Target, TEXT("Pulled free"));
		GaTell(Who, H.How == TEXT("pull") ? TEXT("You pull it up out of the floor") : TEXT("You dig them out"));
		Held.RemoveAt(i);
	}
}

bool UWRGraveArenas::Place(FName BossId)
{
	UWorld* World = GetWorld();
	FVector Centre;
	float Radius = 0.f, Yaw = 0.f;
	if (!FWRGraveBoss::ArenaOf(BossId, Centre, Radius, Yaw) || !FWRCreatureRegistry::Get().Find(BossId)) return true;   // nothing to place
	bool bNear = false;
	for (TActorIterator<AWRCharacter> It(World); It && !bNear; ++It) bNear = It->IsAlive() && FVector::Dist2D(It->GetActorLocation(), Centre) < GaT(TEXT("placeRadius"), 30000.f);
	if (!bNear) return false;
	for (TActorIterator<AWRCreatureSpawner> It(World); It; ++It)
	{
		if (It->CreatureId == BossId) return true;   // placed already (a reload of the subsystem)
	}
	// The ground under the arena must be in (the land's chunks, the chapel's / room's floor built on this machine).
	FVector G;
	if (!GaGround(World, Centre, G, 300.f, 1500.f)) return false;
	const FRotator R(0.f, Yaw, 0.f);
	const auto SpawnProp = [World](FName Kind, const FVector& At, float PropYaw, float Size, int32 Index) -> AWRGraveBossProp*
	{
		const FTransform T(FRotator(0.f, PropYaw, 0.f), At);
		AWRGraveBossProp* A = World->SpawnActorDeferred<AWRGraveBossProp>(AWRGraveBossProp::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (A)
		{
			A->Kind = Kind;
			A->Size = Size;
			A->Index = Index;
			A->FinishSpawning(T);
		}
		return A;
	};
	FVector Start = G;
	if (BossId == TEXT("bell_ringer"))
	{
		// The bell, its wheel and the rope on the tower (the site point is the tower's centre at its foot); he stands in the
		// yard by the rope's arch.
		const FVector Tower = Centre;
		SpawnProp(TEXT("belfry"), Tower, Yaw, 100.f, 0);
		GaGround(World, Tower + R.RotateVector(FVector(WRGraveBelfry::TowerHalf + 450.f, 0.f, 0.f)), Start, 600.f, 1500.f);
	}
	else if (BossId == TEXT("crypt_mother"))
	{
		// Eight egg sacs moored to the gallery's stone: one on the inner face of each of the six pillars, one on each end
		// wall, at a height a torch can reach (their middles about 2 m up).
		const FWRGraveData::FRoom* Room = nullptr;
		for (const FWRGraveData::FRoom& Rm : FWRGraveData::Get().Rooms) if (Rm.Arena == BossId) Room = &Rm;
		if (!Room) return true;
		const float HX = Room->W * 0.5f;
		const float Moor = WRTune(TEXT("bosses.crypt_mother.sacMoor"), 300.f), Sz = WRTune(TEXT("bosses.crypt_mother.sacSize"), 170.f);
		int32 n = 0;
		for (int32 p = 0; p < 6; ++p)
		{
			const float X = -HX + 1000.f + (p % 3) * 2000.f, S = p < 3 ? -1.f : 1.f;
			const FVector Face = Room->At + R.RotateVector(FVector(X, S * (750.f - 150.f) , Moor));
			SpawnProp(TEXT("sac"), Face, Yaw + (S < 0.f ? 90.f : -90.f), Sz, n++);
		}
		for (const float S : { -1.f, 1.f })
		{
			const FVector Wall = Room->At + R.RotateVector(FVector(S * (HX - 20.f), S * 400.f, Moor));
			SpawnProp(TEXT("sac"), Wall, Yaw + (S < 0.f ? 0.f : 180.f), Sz, n++);
		}
		Start = Room->At + FVector(0.f, 0.f, 60.f);
	}
	else if (BossId == TEXT("ossuary_lord"))
	{
		// The bone chandelier on its chain from the pulley at the vault's crown; it hangs high enough that it falls
		// chandelierDrop to lie on the floor.
		const FWRGraveData::FRoom* Room = nullptr;
		for (const FWRGraveData::FRoom& Rm : FWRGraveData::Get().Rooms) if (Rm.Arena == BossId) Room = &Rm;
		if (!Room) return true;
		const float Pulley = Room->H + 100.f;
		const float Drop = WRTune(TEXT("bosses.ossuary_lord.chandelierDrop"), 900.f);
		const float Chain = FMath::Max(150.f, Pulley - Drop - 250.f);
		SpawnProp(TEXT("chandelier"), Room->At + FVector(0.f, 0.f, Pulley), Yaw, Chain, 0);
		// It stands a little off the middle (so the first fight doesn't start under the chandelier).
		Start = Room->At + R.RotateVector(FVector(900.f, 0.f, 60.f));
	}
	else if (BossId == TEXT("gravedigger"))
	{
		// He sleeps in the Watch House by day (his round is FWRGraveBoss::TickDigger).
		GaGround(World, FWRGraveData::Get().GravediggerLair, Start, 600.f, 1500.f);
	}
	else if (BossId == TEXT("nhal"))
	{
		// The twelve chest tombs in the aisles (the chapel's layout), and the Necromancy altar in the apse.
		for (int32 i = 0; i < 12; ++i)
		{
			const FVector2D L = WRGraveChapel::Tomb(i);
			FVector TG;
			GaGround(World, Centre + R.RotateVector(FVector(L.X, L.Y, 0.f)), TG, 400.f, 1500.f);
			SpawnProp(TEXT("tomb"), TG, Yaw, 300.f, i);
		}
		const FVector2D A = WRGraveChapel::Altar();
		FVector AG;
		if (GaGround(World, Centre + R.RotateVector(FVector(A.X, A.Y, 0.f)), AG, 400.f, 1500.f))
		{
			const FTransform T(FRotator(0.f, Yaw + 180.f, 0.f), AG + FVector(0.f, 0.f, 50.f));
			if (AWRBossAltar* Altar = World->SpawnActorDeferred<AWRBossAltar>(AWRBossAltar::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
			{
				Altar->PathId = TEXT("necromancy");
				Altar->FinishSpawning(T);
			}
		}
		GaGround(World, Centre + R.RotateVector(FVector(800.f, 0.f, 0.f)), Start, 400.f, 1500.f);
	}
	const FTransform Where(FRotator(0.f, Yaw + 180.f, 0.f), Start + FVector(0.f, 0.f, 50.f));
	if (AWRCreatureSpawner* Spawner = World->SpawnActorDeferred<AWRCreatureSpawner>(AWRCreatureSpawner::StaticClass(), Where, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		Spawner->CreatureId = BossId;
		Spawner->Count = 1;
		Spawner->Radius = 1.f;
		Spawner->RespawnSeconds = WRTune(TEXT("server.bossRespawnHours"), 4.f) * 3600.f;
		Spawner->ActivationRadius = 0.f;
		Spawner->bTraceGround = BossId != TEXT("crypt_mother") && BossId != TEXT("ossuary_lord");   // under the ground: keep the room's floor
		Spawner->FinishSpawning(Where);
	}
	UE_LOG(LogWildreach, Log, TEXT("GRAVE arena %s placed at %s"), *BossId.ToString(), *Start.ToCompactString());
	return true;
}

void UWRGraveArenas::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	if (!World || !World->HasBegunPlay()) return;
	StepHeld();
	PlaceClock += DeltaTime;
	if (PlaceClock < 2.f) return;
	PlaceClock = 0.f;
	static const FName Ids[] = { TEXT("bell_ringer"), TEXT("crypt_mother"), TEXT("ossuary_lord"), TEXT("gravedigger"), TEXT("nhal") };
	for (const FName Id : Ids)
	{
		if (Placed.Contains(Id)) continue;
		if (Place(Id)) Placed.Add(Id);
	}
}
