#include "WRDesertWorld.h"
#include "WRDesertHazards.h"
#include "Wildreach.h"
#include "WRAttributeSet.h"
#include "WRBoss.h"
#include "AbilitySystemComponent.h"
#include "WRCharacter.h"
#include "WRCharacterBase.h"
#include "WRCreature.h"
#include "WRDesertTypes.h"
#include "WRGameState.h"
#include "WRInventory.h"
#include "WRLand.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRResourceNode.h"
#include "WRSky.h"
#include "WRSound.h"
#include "WRSurvival.h"
#include "WRTuning.h"
#include "WRWorldSystems.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

namespace WRDesertParts { void Build(AWRDesertScenery* A); }

namespace WRDesertWorldImpl
{
	float DwTn(const TCHAR* Key, float Def) { return WRTune(Key, Def); }
	AWRPlayerController* DwControllerOf(const AActor* Pawn)
	{
		const APawn* P = Cast<APawn>(Pawn);
		return P ? Cast<AWRPlayerController>(P->GetController()) : nullptr;
	}
	void DwTell(const AActor* Pawn, const FString& Text)
	{
		if (AWRPlayerController* PC = DwControllerOf(Pawn)) PC->ClientNotify(Text);
	}
	double DwGameHours(const UObject* Ctx)
	{
		const UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
		const AWRGameState* GS = W ? W->GetGameState<AWRGameState>() : nullptr;
		return GS ? GS->GetTotalHours() : 0.0;
	}
	float DwNow(const UObject* Ctx)
	{
		const UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
		return W && W->GetGameState() ? float(W->GetGameState()->GetServerWorldTimeSeconds()) : 0.f;
	}
	FString DwItemName(FName Id)
	{
		const FWRItemDef* D = FWRItemRegistry::Get().Find(Id);
		return D ? D->Name : Id.ToString();
	}
	FString DwPlayerOf(const AActor* A) { return WRWorld::PlayerOf(A); }
	AWRPlayerState* DwStateOf(const AActor* Pawn) { const APawn* P = Cast<APawn>(Pawn); return P ? P->GetPlayerState<AWRPlayerState>() : nullptr; }
	/** Server: a quest beat for Amastan's quests (world.json quests.amastan objectives). */
	void DwQuestEvent(const AActor* Pawn, FName Type, FName What, int32 Count)
	{
		AWRPlayerState* PS = DwStateOf(Pawn);
		UWRWorldSystems* WS = Pawn ? Pawn->GetWorld()->GetSubsystem<UWRWorldSystems>() : nullptr;
		if (PS && WS) WS->QuestEvent(PS, Type, What, Count);
	}
	/** A tamed creature of this species belonging to Who within Range cm (a Scarab digger's nose for buried things). */
	bool DwOwnsTameNear(const AActor* Who, FName Species, const FVector& At, float Range)
	{
		if (!Who) return false;
		for (TActorIterator<AWRCreature> It(Who->GetWorld()); It; ++It)
		{
			if (It->IsTamed() && It->IsAlive() && It->CreatureId == Species && It->GetTameOwner() == DwPlayerOf(Who) && FVector::DistSquared(It->GetActorLocation(), At) < Range * Range) return true;
		}
		return false;
	}
	/** Holds a digging tool (a pick of any kind, or a shovel) in the bag. */
	bool DwHasDigger(const AActor* Who)
	{
		const UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Who);
		if (!Inv) return false;
		for (const FWRItemDef& I : FWRItemRegistry::Get().All())
		{
			if ((I.Id.ToString().EndsWith(TEXT("_pick")) || I.Id.ToString().Contains(TEXT("shovel")) || I.Id.ToString().Contains(TEXT("pickaxe"))) && Inv->CountOf(I.Id) > 0) return true;
		}
		return false;
	}
}
using namespace WRDesertWorldImpl;

// ================================================================================================ the state

AWRDesertState::AWRDesertState()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(2.f);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	MirrorStep.Init(0, 4);
	MirrorFrom.Init(0, 4);
	MirrorAt.Init(-100.f, 4);
}

void AWRDesertState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRDesertState, LeeOpenUntil);
	DOREPLIFETIME(AWRDesertState, LeeChangedAt);
	DOREPLIFETIME(AWRDesertState, bDigDone);
	DOREPLIFETIME(AWRDesertState, DigDoneAt);
	DOREPLIFETIME(AWRDesertState, MirrorStep);
	DOREPLIFETIME(AWRDesertState, MirrorFrom);
	DOREPLIFETIME(AWRDesertState, MirrorAt);
	DOREPLIFETIME(AWRDesertState, bVaultOpen);
	DOREPLIFETIME(AWRDesertState, VaultOpenAt);
	DOREPLIFETIME(AWRDesertState, SignalAt);
	DOREPLIFETIME(AWRDesertState, bLongNoon);
	DOREPLIFETIME(AWRDesertState, NoonStage);
	DOREPLIFETIME(AWRDesertState, NoonStageAt);
	DOREPLIFETIME(AWRDesertState, FloodStart);
	DOREPLIFETIME(AWRDesertState, BrineBrokenUntil);
	DOREPLIFETIME(AWRDesertState, Sinkers);
	DOREPLIFETIME(AWRDesertState, bGnomonUprooted);
}

AWRDesertState* AWRDesertState::Get(const UObject* WorldContext)
{
	static TWeakObjectPtr<AWRDesertState> Cached;
	UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!W) return nullptr;
	if (Cached.IsValid() && Cached->GetWorld() == W) return Cached.Get();
	for (TActorIterator<AWRDesertState> It(W); It; ++It) { Cached = *It; return *It; }
	return nullptr;
}

bool AWRDesertState::GalleryAligned() const
{
	// The cook's Mirror Gallery (2.5 room 13): the shaft from the roof turns 4 corners to the vault's lock. Each mirror
	// has one right step (desert.json's spots carry it; tuning desertWorld.galleryRight overrides).
	static const int32 Right[4] = { 2, 5, 7, 1 };
	for (int32 i = 0; i < 4 && i < MirrorStep.Num(); ++i)
	{
		const int32 Want = int32(DwTn(*FString::Printf(TEXT("desertWorld.galleryRight.%d"), i), float(Right[i])));
		if (MirrorStep[i] != Want) return false;
	}
	return MirrorStep.Num() >= 4;
}

float AWRDesertState::MirrorAngle(int32 Index, float Now) const
{
	if (!MirrorStep.IsValidIndex(Index)) return 0.f;
	const float From = (MirrorFrom.IsValidIndex(Index) ? MirrorFrom[Index] : 0) * 45.f;
	float To = MirrorStep[Index] * 45.f;
	if (To < From - 1.f) To += 360.f;                 // always turns the same way round
	const float T = FMath::Clamp((Now - (MirrorAt.IsValidIndex(Index) ? MirrorAt[Index] : -100.f)) / 0.6f, 0.f, 1.f);
	const float Ease = T * T * (3.f - 2.f * T);
	return FMath::Lerp(From, To, Ease);
}

// ================================================================================================ the scenery

AWRDesertScenery::AWRDesertScenery()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickInterval = 0.f;
}

void AWRDesertScenery::BuildDesert()
{
	WRDesertParts::Build(this);
	SetActorTickEnabled(Wheel || Signal || Dust.Num() > 0 || Mirrors.Num() > 0 || VaultDoor || SeventhDoor || GnomonPart || GnomonBox || LeeSand || DigSand || Shafts.Num() > 0 || Spin);
}

void AWRDesertScenery::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (GetNetMode() == NM_DedicatedServer && !LeeSand && !DigSand && !VaultDoor && !SeventhDoor && !GnomonBox) return;
	const float NowS = DwNow(this);
	const AWRDesertState* S = AWRDesertState::Get(this);
	// The qanat tower's bucket wheel: 6 rpm, a pure function of the server clock (every machine shows the same turn).
	if (Wheel) Wheel->SetRelativeRotation(FRotator(FMath::Fmod(NowS * DwTn(TEXT("desertWorld.wheelRpm"), 6.f) * 6.f, 360.f), 0.f, 0.f));
	// The Lookout's signal mirror: follows the sun's bearing (its pivot turns the face to bounce the light toward the
	// pass), swinging slowly back to rest at night.
	if (Signal)
	{
		const float H = AWRGameState::HourIn(this);
		const float Yaw = H > 6.f && H < 20.f ? FMath::Lerp(-70.f, 70.f, (H - 6.f) / 14.f) : 0.f;
		const FRotator Cur = Signal->GetRelativeRotation();
		Signal->SetRelativeRotation(FRotator(Cur.Pitch, FMath::FInterpTo(Cur.Yaw, Yaw, DeltaSeconds, 0.5f), Cur.Roll));
	}
	// The Haboob Wall's dust rows slide along the wall (east to west, each at its own speed) and wrap over its span.
	for (int32 i = 0; i < Dust.Num(); ++i)
	{
		USceneComponent* D = Dust[i];
		if (!D || DustSpan <= 0.f) continue;
		FVector P = D->GetRelativeLocation();
		const float Speed = DustSpeed.IsValidIndex(i) ? DustSpeed[i] : 300.f;
		P.X = FMath::Fmod(NowS * Speed + i * 3137.f, DustSpan) - DustSpan * 0.5f;
		D->SetRelativeLocation(P);
	}
	// The Mirror Gallery's mirrors: each yaws to its replicated step, eased over 0.6 s from the last turn.
	if (S)
	{
		for (int32 i = 0; i < Mirrors.Num(); ++i) if (USceneComponent* M = Mirrors[i]) M->SetRelativeRotation(FRotator(0.f, S->MirrorAngle(i, NowS), 0.f));
		const bool bAligned = S->GalleryAligned() || S->bVaultOpen;
		for (USceneComponent* B : Beam) if (B && B->IsVisible() != bAligned) B->SetVisibility(bAligned, true);
	}
	// The Sun-Vault's stone door slides aside (3 s) once the beam reaches its lock.
	if (!bDoorBases)
	{
		bDoorBases = true;
		if (VaultDoor) VaultDoorBase = VaultDoor->GetRelativeLocation();
		if (SeventhDoor) SeventhDoorBase = SeventhDoor->GetRelativeLocation();
	}
	if (VaultDoor && S)
	{
		// (It slides along its own wall, 3.3 m aside.)
		const float T = S->bVaultOpen ? FMath::Clamp((NowS - S->VaultOpenAt) / 3.f, 0.f, 1.f) : 0.f;
		const float Ease = T * T * (3.f - 2.f * T);
		VaultDoor->SetRelativeLocation(VaultDoorBase + FVector(Ease * 330.f, 0.f, 0.f));
		if (UPrimitiveComponent* Pr = Cast<UPrimitiveComponent>(VaultDoor)) Pr->SetCollisionEnabled(T >= 1.f ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
	}
	// The gnomon: gone from the dial while Ahrumet carries it (phase 2), back when the fight resets.
	if (S && (GnomonPart || GnomonBox))
	{
		const bool bGone = S->bGnomonUprooted;
		if (GnomonPart && GnomonPart->IsVisible() == bGone) GnomonPart->SetVisibility(!bGone, true);
		if (GnomonBox) GnomonBox->SetCollisionEnabled(bGone ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
	}
	// The Seventh Vault's door: open while the Long Noon holds (4 s each way).
	if (SeventhDoor && S)
	{
		SeventhOpen = FMath::FInterpConstantTo(SeventhOpen, S->bLongNoon ? 1.f : 0.f, DeltaSeconds, 0.25f);
		const float Ease = SeventhOpen * SeventhOpen * (3.f - 2.f * SeventhOpen);
		SeventhDoor->SetRelativeLocation(SeventhDoorBase + FVector(Ease * 330.f, 0.f, 0.f));
		if (UPrimitiveComponent* Pr = Cast<UPrimitiveComponent>(SeventhDoor)) Pr->SetCollisionEnabled(SeventhOpen >= 0.99f ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
	}
	// The Lee Door's sand bank slumps down over 6 s when it opens (after a Sandstorm block) and pours back when it closes;
	// the Dig's sand sinks once (4 s). Their collision goes with them.
	const auto Sink = [&](USceneComponent* Sand, bool bOpen, float ChangedAt, float Seconds)
	{
		if (!Sand) return;
		const float T = FMath::Clamp((NowS - ChangedAt) / Seconds, 0.f, 1.f);
		const float Open = bOpen ? T : 1.f - T;
		const float Ease = Open * Open * (3.f - 2.f * Open);
		Sand->SetRelativeScale3D(FVector(1.f, 1.f, FMath::Lerp(1.f, 0.05f, Ease)));
		for (USceneComponent* Child : Sand->GetAttachChildren())
		{
			if (UPrimitiveComponent* Pr = Cast<UPrimitiveComponent>(Child)) Pr->SetCollisionEnabled(Ease > 0.6f ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
		}
		if (UPrimitiveComponent* Pr = Cast<UPrimitiveComponent>(Sand)) Pr->SetCollisionEnabled(Ease > 0.6f ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
	};
	if (S)
	{
		const bool bLeeOpen = S->LeeOpenUntil > 0.f && DwGameHours(this) < S->LeeOpenUntil;
		Sink(LeeSand, bLeeOpen, S->LeeChangedAt, 6.f);
		Sink(DigSand, S->bDigDone, S->DigDoneAt, 4.f);
	}
	if (GetNetMode() == NM_DedicatedServer) return;
	// The Hall of Hours' light shafts: aimed down the real sun's direction (through the roof holes), gone at night.
	if (Shafts.Num() > 0)
	{
		const FWRTuning& T = FWRTuning::Get();
		const float H = AWRGameState::HourIn(this);
		const FVector Sun = FWRSkyRules::SunDirection(H, T.Num(TEXT("clock.dawnHour"), 6.f), T.Num(TEXT("clock.duskHour"), 20.f), T.Num(TEXT("sky.noonElevation"), 65.f), T.Num(TEXT("sky.midnightDepth"), 50.f));
		const bool bDay = Sun.Z > 0.08f;
		const FRotator R = (-Sun).Rotation();
		for (USceneComponent* Sh : Shafts)
		{
			if (!Sh) continue;
			if (Sh->IsVisible() != bDay) Sh->SetVisibility(bDay, true);
			Sh->SetWorldRotation(FRotator(R.Pitch + 90.f, R.Yaw, 0.f));
		}
	}
	// A dust devil's column spins (about 1.4 turns a second).
	if (Spin) Spin->AddLocalRotation(FRotator(0.f, DeltaSeconds * 500.f, 0.f));
}

// ================================================================================================ the spots

AWRDesertSpot::AWRDesertSpot()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	SetNetUpdateFrequency(2.f);
	SetNetCullDistanceSquared(FMath::Square(15000.f));
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRDesertSpot::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRDesertSpot, Kind);
	DOREPLIFETIME(AWRDesertSpot, SpotId);
	DOREPLIFETIME(AWRDesertSpot, Title);
	DOREPLIFETIME(AWRDesertSpot, Verb);
	DOREPLIFETIME(AWRDesertSpot, Seconds);
	DOREPLIFETIME(AWRDesertSpot, DoneAt);
	DOREPLIFETIME(AWRDesertSpot, Doer);
	DOREPLIFETIME(AWRDesertSpot, bConcealed);
}

bool AWRDesertSpot::IsDoneBy(const FString& Player, double NowHours) const
{
	const double* At = DoneBy.Find(Player);
	if (!At) return false;
	return RespawnHours <= 0.f || NowHours - *At < RespawnHours;
}

FString AWRDesertSpot::Start(AWRPlayerController* PC)
{
	AWRCharacterBase* Me = PC ? Cast<AWRCharacterBase>(PC->GetPawn()) : nullptr;
	if (!Me || bConcealed) return FString();
	const FString Who = DwPlayerOf(Me);
	if (DoneAt > 0.f) return Doer == Who ? TEXT("Hold still...") : FString::Printf(TEXT("%s is at it"), *Doer);
	AWRDesertState* S = AWRDesertState::Get(this);
	if (Kind == TEXT("mirror"))
	{
		// A bronze mirror on its pivot (the Mirror Gallery): one step of 45 degrees a turn, for anyone; the beam carries
		// round the corners as they line up and opens the Sun-Vault.
		if (!S || Mirror == INDEX_NONE) return FString();
		if (S->bVaultOpen) return TEXT("The light runs round every corner into the vault. It's open");
		const float NowS = DwNow(this);
		if (S->MirrorAt.IsValidIndex(Mirror) && NowS - S->MirrorAt[Mirror] < 0.6f) return TEXT("It's still turning");
		S->MirrorFrom[Mirror] = S->MirrorStep[Mirror];
		S->MirrorStep[Mirror] = uint8((S->MirrorStep[Mirror] + 1) % 8);
		S->MirrorAt[Mirror] = NowS;
		WRSound::PlayAt(Me, TEXT("desert.mirror_turn"), GetActorLocation());
		if (S->GalleryAligned())
		{
			S->bVaultOpen = true;
			S->VaultOpenAt = NowS + 0.6f;
			WRSound::Broadcast(this, TEXT("desert.vault_open"));
			DwQuestEvent(Me, TEXT("event"), TEXT("sun_vault"), 1);
			return TEXT("The shaft of light runs the length of the gallery and strikes the sun disc on the door. Stone grinds on stone.");
		}
		return TEXT("The mirror turns on its bronze pivot.");
	}
	if (Kind == TEXT("dig"))
	{
		if (S && S->bDigDone) return TEXT("The steps go down under the sand");
		if (!DwHasDigger(Me)) return TEXT("Packed sand to the lintel. You need a pick or a shovel to clear it");
	}
	if (Kind == TEXT("fill_jars"))
	{
		const UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Me);
		if (!Inv || Inv->CountOf(TEXT("water_jar")) <= 0) return TEXT("Cold, deep water. You need a water jar to carry it (clay, made by hand)");
	}
	if (Kind == TEXT("seventh_vault") && !IsDoneBy(Who, DwGameHours(this)))
	{
		const UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Me);
		if (!Inv || Inv->CountOf(TEXT("seventh_key")) <= 0) return TEXT("A lock shaped like a sun disc. The key would be where the seventh stela's noon shadow ends");
	}
	if (Kind == TEXT("squeeze") && IsDoneBy(Who, DwGameHours(this))) return TEXT("It's gone back into the clay. Leave it be a while");
	if (IsDoneBy(Who, DwGameHours(this)))
	{
		return Kind == TEXT("squeeze") || Kind == TEXT("dig_mound") ? TEXT("Nothing more here today") : TEXT("You've had what's here");
	}
	DoneAt = DwNow(this) + Seconds;
	Doer = Who;
	DoerPawn = Me;
	DoerFrom = Me->GetActorLocation();
	SetActorTickEnabled(true);
	if (Kind == TEXT("dig")) WRSound::PlayAt(Me, TEXT("desert.dig"), GetActorLocation());
	if (Seconds <= 0.01f) { Finish(); return FString(); }
	return FString::Printf(TEXT("%s..."), *Verb);
}

void AWRDesertSpot::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || DoneAt <= 0.f) return;
	AWRCharacterBase* P = DoerPawn.Get();
	if (!P || !P->IsAlive() || FVector::Dist(P->GetActorLocation(), DoerFrom) > 150.f || FVector::Dist(P->GetActorLocation(), GetActorLocation()) > Reach + 200.f)
	{
		if (P) DwTell(P, TEXT("You let go"));
		DoneAt = 0.f;
		Doer.Reset();
		DoerPawn = nullptr;
		SetActorTickEnabled(false);
		return;
	}
	if (DwNow(this) >= DoneAt) Finish();
}

void AWRDesertSpot::Finish()
{
	AWRCharacterBase* P = DoerPawn.Get();
	DoneAt = 0.f;
	Doer.Reset();
	DoerPawn = nullptr;
	SetActorTickEnabled(false);
	if (!P) return;
	const FString Who = DwPlayerOf(P);
	UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(P);
	if (!Inv) return;
	TMap<FName, int32> Pay = Gives;
	if (Kind == TEXT("dig"))
	{
		// H-E3: the Lost Expedition's pit cleared once, for the whole server (the sand sinks, the steps go down).
		if (AWRDesertState* S = AWRDesertState::Get(this)) { S->bDigDone = true; S->DigDoneAt = DwNow(this); }
		WRSound::Broadcast(this, TEXT("desert.sand_pour"));
		DwQuestEvent(P, TEXT("event"), TEXT("hourhold_dig"), 1);
	}
	if (Kind == TEXT("fill_jars"))
	{
		DwQuestEvent(P, TEXT("event"), TEXT("water_jars"), 1);
		WRSound::PlayAt(P, TEXT("tide.splash"), GetActorLocation());
		DwTell(P, TEXT("You lower the jar and draw it up full: cold water that tastes of stone"));
	}
	if (Kind == TEXT("seventh_vault"))
	{
		// The key stays in the lock.
		if (Inv->CountOf(TEXT("seventh_key")) <= 0) return;
		Inv->RemoveItem(TEXT("seventh_key"), 1);
		WRSound::Broadcast(this, TEXT("desert.vault_open"));
	}
	if (Kind == TEXT("squeeze"))
	{
		// A Wellfrog in its clay (cook 2.12.1): one full drink, and it lives (back into the clay for 2 game hours).
		if (UAbilitySystemComponent* ASC = P->GetAbilitySystemComponent())
		{
			if (const UWRAttributeSet* At = P->GetAttributes()) ASC->SetNumericAttributeBase(UWRAttributeSet::GetThirstAttribute(), FMath::Min(At->GetThirst() + DwTn(TEXT("desertWorld.wellfrogDrink"), 30.f), 100.f));
		}
		WRSound::PlayAt(P, TEXT("desert.wellfrog_squeeze"), GetActorLocation());
	}
	if (const int32* Gem = Pay.Find(TEXT("@gem")))
	{
		TArray<FName> Gems;
		for (const FWRItemDef& I : FWRItemRegistry::Get().All()) if (I.Category == TEXT("gem")) Gems.Add(I.Id);
		const int32 N = *Gem;
		Pay.Remove(TEXT("@gem"));
		if (Gems.Num() > 0) Pay.FindOrAdd(Gems[FMath::RandRange(0, Gems.Num() - 1)]) += N;
	}
	DoneBy.Add(Who, DwGameHours(this));
	TArray<FString> Got;
	for (const TPair<FName, int32>& G : Pay)
	{
		if (G.Value <= 0 || !FWRItemRegistry::Get().Find(G.Key)) continue;
		Inv->AddItem(G.Key, G.Value);
		Got.Add(FString::Printf(TEXT("%s x%d"), *DwItemName(G.Key), G.Value));
		DwQuestEvent(P, TEXT("gather"), G.Key, G.Value);
	}
	if (Kind == TEXT("rubbing")) DwQuestEvent(P, TEXT("event"), TEXT("blue_hands"), 1);
	DwQuestEvent(P, TEXT("event"), SpotId, 1);
	if (!Lore.IsEmpty()) DwTell(P, Lore);
	if (Got.Num() > 0) DwTell(P, FString::Printf(TEXT("%s: %s"), *Title, *FString::Join(Got, TEXT(", "))));
	UE_LOG(LogWildreach, Log, TEXT("DESERT spot %s (%s) done by %s: %s"), *SpotId.ToString(), *Kind.ToString(), *Who, *FString::Join(Got, TEXT(", ")));
}

// ================================================================================================ the subsystem

bool UWRDesertWorld::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

TStatId UWRDesertWorld::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWRDesertWorld, STATGROUP_Tickables);
}

UWRDesertWorld* UWRDesertWorld::Get(const UObject* WorldContext)
{
	const UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	return W ? W->GetSubsystem<UWRDesertWorld>() : nullptr;
}

void UWRDesertWorld::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	FString Map = UWorld::RemovePIEPrefix(InWorld.GetMapName());
	Map.RemoveFromStart(InWorld.StreamingLevelsPrefix);
	if (!Map.EndsWith(FWRTuning::Get().Str(TEXT("desertWorld.map"), TEXT("L_Island"))) || !WRDesert::Live()) return;
	bActive = true;
	UE_LOG(LogWildreach, Log, TEXT("DESERT live on %s: waiting for the desert's ground"), *Map);
}

void UWRDesertWorld::BuildScenery()
{
	// Every place, in a fixed order (the same on every machine, so each builds the same collision): the landmarks, the
	// sites, the kit, the caves, the Hourhold's rooms, galleries and ways, the brine crusts, then the decoration in
	// 250 m cells.
	const FWRDesertData& D = FWRDesertData::Get();
	for (int32 i = 0; i < D.Landmarks.Num(); ++i) { const auto& P = D.Landmarks[i]; Pieces.Add({ P.Kind, P.Id, P.Zone, P.At, P.Yaw, i, P.Variant, P.Size }); }
	for (int32 i = 0; i < D.Sites.Num(); ++i) { const auto& P = D.Sites[i]; Pieces.Add({ P.Kind, P.Id, P.Zone, P.At, P.Yaw, i, P.Variant, P.Size }); }
	for (int32 i = 0; i < D.Kit.Num(); ++i) { const auto& P = D.Kit[i]; Pieces.Add({ P.Kind, NAME_None, P.Zone, P.At, P.Yaw, i, P.Variant, P.Lean }); }
	for (int32 i = 0; i < D.Caves.Num(); ++i) { const auto& C = D.Caves[i]; Pieces.Add({ TEXT("cave"), C.Id, C.Zone, C.At, 0.f, i, 0, 0.f }); }
	for (int32 i = 0; i < D.Rooms.Num(); ++i) { const auto& R = D.Rooms[i]; Pieces.Add({ TEXT("hh_room"), R.Id, TEXT("D2"), R.At, 0.f, i, 0, 0.f }); }
	for (int32 i = 0; i < D.Links.Num(); ++i)
	{
		const FWRDesertData::FRoom* A = D.FindRoom(D.Links[i].Key);
		if (A) Pieces.Add({ TEXT("hh_link"), NAME_None, TEXT("D2"), A->At, 0.f, i, 0, 0.f });
	}
	for (int32 i = 0; i < D.Ways.Num(); ++i) { const auto& W = D.Ways[i]; Pieces.Add({ TEXT("hh_way"), W.Id, TEXT("D2"), W.From, 0.f, i, 0, 0.f }); }
	for (int32 i = 0; i < D.Brine.Num(); ++i) Pieces.Add({ TEXT("brine"), NAME_None, TEXT("D1"), D.Brine[i].At, 0.f, i, 0, D.Brine[i].R });
	for (int32 i = 0; i < D.Floods.Num(); ++i) if (D.Floods[i].Pts.Num() > 0) Pieces.Add({ TEXT("flood_wadi"), D.Floods[i].Id, TEXT("D4"), D.Floods[i].Pts[0], 0.f, i, 0, 0.f });
	// The decoration: one actor a 250 m cell (its instances drawn as a handful of parts).
	const float Cell = DwTn(TEXT("desertWorld.decorCell"), 25000.f);
	TSet<FIntPoint> Cells;
	const auto AddCells = [&](const TArray<FWRDesertData::FDecor>& L) { for (const auto& R : L) Cells.Add(FIntPoint(FMath::FloorToInt(R.At.X / Cell), FMath::FloorToInt(R.At.Y / Cell))); };
	AddCells(D.Grass); AddCells(D.Driftwood); AddCells(D.Saltbush); AddCells(D.Palms); AddCells(D.Reeds); AddCells(D.Acacias);
	AddCells(D.Pears); AddCells(D.Rocks); AddCells(D.Pebbles); AddCells(D.Hoodoos); AddCells(D.Shells);
	TArray<FIntPoint> Sorted = Cells.Array();
	Sorted.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.Y != B.Y ? A.Y < B.Y : A.X < B.X; });
	for (const FIntPoint& C : Sorted)
	{
		const FVector At((C.X + 0.5f) * Cell, (C.Y + 0.5f) * Cell, 0.f);
		Pieces.Add({ TEXT("decor"), NAME_None, NAME_None, At, 0.f, C.X * 10000 + (C.Y & 0xFFFF), 0, Cell });
	}
	UE_LOG(LogWildreach, Log, TEXT("DESERT scenery queued: %d places (%d decoration cells)"), Pieces.Num(), Sorted.Num());
}

void UWRDesertWorld::StepBuild()
{
	UWorld* World = GetWorld();
	if (!World) return;
	if (!bQueued)
	{
		WaitClock += World->GetDeltaSeconds();
		const FWRLandDef* Def = WRLand::Find(TEXT("desert"));
		AWRLand* Land = Def ? UWRLandSubsystem::FindLand(World, Def->Id) : nullptr;
		if (!Land || !Land->IsGroundReady())
		{
			if (WaitClock > 60.f) { WaitClock = 0.f; UE_LOG(LogWildreach, Warning, TEXT("DESERT still waiting for the desert's ground (Data/lands/desert.json)")); }
			return;
		}
		BuildScenery();
		bQueued = true;
	}
	if (!bSceneryDone)
	{
		const int32 PerFrame = FMath::Max(1, int32(DwTn(TEXT("desertWorld.buildPerFrame"), 3.f)));
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		for (int32 n = 0; n < PerFrame && PiecesBuilt < Pieces.Num(); ++n, ++PiecesBuilt)
		{
			const FPiece& Pc = Pieces[PiecesBuilt];
			AWRDesertScenery* A = World->SpawnActor<AWRDesertScenery>(AWRDesertScenery::StaticClass(), FTransform(FRotator(0.f, Pc.Yaw, 0.f), Pc.At), P);
			if (!A) continue;
			A->Kind = Pc.Kind;
			A->SiteId = Pc.Id;
			A->Zone = Pc.Zone;
			A->Variant = Pc.Variant;
			A->Index = Pc.Index;
			A->Size = Pc.Size;
			A->BuildDesert();
			Scenery.Add(A);
		}
		if (PiecesBuilt < Pieces.Num()) return;
		bSceneryDone = true;
		Pieces.Empty();
		UE_LOG(LogWildreach, Log, TEXT("DESERT scenery built: %d places"), Scenery.Num());
	}
	if (!bServerDone)
	{
		bServerDone = true;
		if (World->GetNetMode() != NM_Client) SpawnServerActors();
	}
}

AWRResourceNode* UWRDesertWorld::SpawnNode(FName Item, const FVector& At, int32 Hits, float PerHit, float RespawnSeconds, float Yaw)
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

AWRWorldCache* UWRDesertWorld::SpawnCache(const FVector& At, float Yaw, const FString& Title, const TMap<FName, int32>& Items, const FString& Lore, float RefillHours)
{
	UWorld* World = GetWorld();
	const FTransform Tr(FRotator(0.f, Yaw, 0.f), At);
	AWRWorldCache* C = World->SpawnActorDeferred<AWRWorldCache>(AWRWorldCache::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!C) return nullptr;
	C->Kind = TEXT("chest");
	C->Biome = WRDesert::Biome();
	C->Title = Title;
	// Only items the game knows (a lore piece or blueprint a later step adds stays out until it exists).
	for (const TPair<FName, int32>& I : Items) if (FWRItemRegistry::Get().Find(I.Key)) C->Items.Add(I.Key, I.Value);
	C->Lore = Lore;
	C->RefillHours = RefillHours;
	C->FinishSpawning(Tr);
	Caches.Add(C);
	return C;
}

AWRDesertSpot* UWRDesertWorld::SpawnSpot(FName Kind, FName Id, const FVector& At, const FString& Title, const FString& Verb, float Seconds)
{
	UWorld* World = GetWorld();
	const FTransform Tr(FRotator::ZeroRotator, At);
	AWRDesertSpot* S = World->SpawnActorDeferred<AWRDesertSpot>(AWRDesertSpot::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
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

float UWRDesertWorld::SunstoneYield(const UObject* WorldContext)
{
	// The plenty after a storm (world-map KEPT): +50% the block after a Sandstorm. The Long Noon's x3 is the event's.
	float Mult = 1.f;
	const UWRDesertWorld* W = Get(WorldContext);
	if (W && W->LastWeather == TEXT("after_sandstorm")) Mult *= DwTn(TEXT("desertWorld.stormPlenty"), 1.5f);
	if (WRDesertHz::LongNoon(WorldContext)) Mult *= DwTn(TEXT("desertEvents.longNoon.sunstoneMult"), 3.f);
	return Mult;
}

float UWRDesertWorld::SunHoursToday(const UObject* WorldContext)
{
	// Hours of direct sun so far today: the sun is up 06:00-20:00 (14 h), so a sunstone's stored light at night is full
	// after a clear day (KEPT: brightness = hours of sun / 14).
	const float H = AWRGameState::HourIn(WorldContext);
	const FWRTuning& T = FWRTuning::Get();
	const float Dawn = T.Num(TEXT("clock.dawnHour"), 6.f), Dusk = T.Num(TEXT("clock.duskHour"), 20.f);
	if (H < Dawn) return Dusk - Dawn;           // the night after a day: what the day stored
	return FMath::Clamp(H - Dawn, 0.f, Dusk - Dawn);
}

void UWRDesertWorld::SpawnServerActors()
{
	UWorld* World = GetWorld();
	const FWRDesertData& D = FWRDesertData::Get();
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (!AWRDesertState::Get(this)) State = World->SpawnActor<AWRDesertState>(AWRDesertState::StaticClass(), FTransform::Identity, SP);

	// ---- the nodes (cook 2.7): their numbers from tuning desertWorld.nodes.<item> (give, hits, perHit, respawn, seed,
	// seedChance, extra) ----
	const FWRTuning& T = FWRTuning::Get();
	int32 Nodes = 0;
	for (const FWRDesertData::FNode& N : D.Nodes)
	{
		const FString K = FString::Printf(TEXT("desertWorld.nodes.%s."), *N.Item.ToString());
		const FName Give(*T.Str(K + TEXT("give"), N.Item.ToString()));
		if (!FWRItemRegistry::Get().Find(Give)) continue;
		// The node is its item (the look is keyed on it: an acacia_log node is an acacia, dates a date palm).
		AWRResourceNode* R = SpawnNode(Give, N.At, int32(T.Num(K + TEXT("hits"), 3.f)), T.Num(K + TEXT("perHit"), 1.f), T.Num(K + TEXT("respawn"), 3600.f));
		if (!R) continue;
		++Nodes;
		const FName Item = N.Item;
		const float SeedChance = T.Num(K + TEXT("seedChance"), 0.f);
		const FName Seed(*T.Str(K + TEXT("seed"), FString()));
		const FName Extra(*T.Str(K + TEXT("extra"), FString()));
		const float ExtraChance = T.Num(K + TEXT("extraChance"), 0.f);
		if (Item == TEXT("sunstone")) R->YieldScale = [](AWRCharacterBase* Who) { return UWRDesertWorld::SunstoneYield(Who); };
		R->OnHarvested = [Item, SeedChance, Seed, Extra, ExtraChance](AWRCharacterBase* Who)
		{
			// The wild crops give their seeds now and then (KEPT wildSeeds), the palms their fibre; every hit is a quest beat.
			UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Who);
			if (Inv && SeedChance > 0.f && !Seed.IsNone() && FMath::FRand() < SeedChance) Inv->AddItem(Seed, 1);
			if (Inv && ExtraChance > 0.f && !Extra.IsNone() && FWRItemRegistry::Get().Find(Extra) && FMath::FRand() < ExtraChance) Inv->AddItem(Extra, 1);
			DwQuestEvent(Who, TEXT("gather"), Item, 1);
		};
	}
	// ---- the spots and caches (cook 2.4, 2.5, 2.20) from desert.json ----
	const TSharedPtr<FJsonObject> File = FWRTuning::LoadDataFile(TEXT("desert.json"));
	const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
	int32 NSpots = 0, NCaches = 0, NStones = 0;
	if (File.IsValid() && File->TryGetArrayField(TEXT("spots"), A))
	{
		for (const auto& V : *A)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			const FVector At = FVector(O->GetNumberField(TEXT("x")), O->GetNumberField(TEXT("y")), O->GetNumberField(TEXT("z"))) * 100.0 + FVector(0, 0, 60.f);
			const FName Kind(*O->GetStringField(TEXT("kind")));
			if (Kind == TEXT("runback"))
			{
				// A run-back point (the plaza's stone, the Spring's brazier): the bosses' run-back stone, bound by touch.
				FRotator Face = FRotator::ZeroRotator;
				if (AWRRunbackStone* Stone = World->SpawnActor<AWRRunbackStone>(AWRRunbackStone::StaticClass(), At - FVector(0, 0, 60.f), Face, SP))
				{
					Stone->BossId = O->GetStringField(TEXT("id")) == TEXT("plaza_runback") ? FName(TEXT("ahrumet")) : FName(TEXT("hourhold"));
					++NStones;
				}
				continue;
			}
			AWRDesertSpot* S = SpawnSpot(Kind, FName(*O->GetStringField(TEXT("id"))), At, O->GetStringField(TEXT("title")), O->GetStringField(TEXT("verb")), float(O->GetNumberField(TEXT("seconds"))));
			if (!S) continue;
			++NSpots;
			const TSharedPtr<FJsonObject>* G = nullptr;
			if (O->TryGetObjectField(TEXT("gives"), G)) for (const auto& KV : (*G)->Values) S->Gives.Add(FName(*KV.Key), int32(KV.Value->AsNumber()));
			S->Lore = O->GetStringField(TEXT("lore"));
			S->RespawnHours = float(O->GetNumberField(TEXT("respawnHours")));
			double Mirror = -1.0;
			if (O->TryGetNumberField(TEXT("mirror"), Mirror)) { S->Mirror = int32(Mirror); S->Reach = 220.f; }
		}
	}
	if (File.IsValid() && File->TryGetArrayField(TEXT("caches"), A))
	{
		for (const auto& V : *A)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			const FVector At = FVector(O->GetNumberField(TEXT("x")), O->GetNumberField(TEXT("y")), O->GetNumberField(TEXT("z"))) * 100.0;
			TMap<FName, int32> Items;
			const TSharedPtr<FJsonObject>* I = nullptr;
			if (O->TryGetObjectField(TEXT("items"), I)) for (const auto& KV : (*I)->Values) Items.Add(FName(*KV.Key), int32(KV.Value->AsNumber()));
			if (SpawnCache(At, float(O->GetNumberField(TEXT("yaw"))), O->GetStringField(TEXT("title")), Items, O->GetStringField(TEXT("lore")), float(O->GetNumberField(TEXT("refillHours"))))) ++NCaches;
		}
	}
	// The kit's wrecks, caravans and ruins each keep a little (cook 2.4: "a cache" in each kit kind that would hold one).
	for (const FWRDesertData::FPlace& K : D.Kit)
	{
		const bool bWreck = K.Kind == TEXT("wreck_fragment"), bCaravan = K.Kind == TEXT("caravan"), bRuin = K.Kind == TEXT("ruin") && K.Variant == 2;
		if (!bWreck && !bCaravan && !bRuin) continue;
		if ((K.At.X + K.At.Y) / 100.f - FMath::FloorToFloat((K.At.X + K.At.Y) / 300.f) * 3.f > 1.5f && !bRuin) continue;   // about half of them
		TMap<FName, int32> Items;
		if (bWreck) { Items.Add(TEXT("rope"), 2); Items.Add(TEXT("linen"), 1); Items.Add(TEXT("iron_nails"), 2); }
		else if (bCaravan) { Items.Add(TEXT("linen"), 2); Items.Add(TEXT("dates"), 2); Items.Add(TEXT("rock_salt"), 2); }
		else { Items.Add(TEXT("clay"), 2); Items.Add(TEXT("copper_ore"), 2); }
		if (SpawnCache(K.At + FVector(150.f, 0.f, 30.f), K.Yaw, bWreck ? TEXT("A sea chest in the sand") : bCaravan ? TEXT("A caravan's pannier") : TEXT("A cistern niche"), Items,
			bWreck ? TEXT("Salt-stiff cloth, and a name carved in the lid.") : FString(), T.Num(TEXT("desertWorld.kitRefillHours"), 96.f))) ++NCaches;
	}
	// The Long Noon's seven tilted stelae caches (hidden spots until the event: WRDesertEvents opens them).
	int32 NTilted = 0;
	for (int32 i = 0; i < D.Tilted.Num(); ++i)
	{
		const FWRDesertData::FTilted& Tl = D.Tilted[i];
		AWRDesertSpot* S = SpawnSpot(TEXT("stela_cache"), FName(*FString::Printf(TEXT("tilted_%d"), i)), Tl.Cache + FVector(0, 0, 60.f), Tl.bSeventh ? TEXT("Where the seventh shadow points") : TEXT("Where the stela's shadow points"), TEXT("Dig where the shadow ends"), 3.f);
		if (!S) continue;
		S->bConcealed = true;
		S->RespawnHours = 0.f;
		S->Gives.Add(TEXT("sunstone"), 4);
		S->Gives.Add(TEXT("@gem"), 1);
		S->Gives.Add(TEXT("gold_flakes"), 3);
		S->Lore = Tl.bSeventh ? TEXT("A bronze key in the shape of a sun disc, and a line of glyphs: the seventh vault under the city, open only while the sun stands still.") : TEXT("A Sunwatch cache, sealed with wax, buried where the noon shadow ends.");
		if (Tl.bSeventh) S->Gives.Add(TEXT("seventh_key"), 1);
		++NTilted;
	}
	// The Sevenfold Wells' lowest step (Amastan's first quest): fill a water jar there.
	if (const FWRDesertData::FPlace* Wells = D.FindLandmark(TEXT("sevenfold_wells")))
	{
		if (AWRDesertSpot* S = SpawnSpot(TEXT("fill_jars"), TEXT("sevenfold_fill"), Wells->At + FVector(0, 0, 60.f), TEXT("The Sevenfold Wells' water"), TEXT("Fill a water jar"), 1.5f))
		{
			S->RespawnHours = T.Num(TEXT("desertWorld.fillHours"), 0.05f);
			S->Reach = 600.f;
		}
	}
	// The Seventh Vault (the Hourhold's room 16): its door slides aside only in the Long Noon; the sun-disc key from
	// the seventh tilted stela's cache opens the king's chest inside (cook 2.10: an Ascendant sunstone cache until
	// artifacts drop here).
	if (const FWRDesertData::FRoom* Rm = D.FindRoom(TEXT("seventh_vault")))
	{
		if (AWRDesertSpot* S = SpawnSpot(TEXT("seventh_vault"), TEXT("seventh_vault"), Rm->At + FVector(0, Rm->D * 0.15f, 60.f), TEXT("The last king's chest"), TEXT("Turn the sun-disc key"), 2.f))
		{
			S->bConcealed = true;
			S->RespawnHours = 0.f;
			S->Gives.Add(TEXT("sunstone"), int32(T.Num(TEXT("desertEvents.longNoon.vaultSunstone"), 16.f)));
			S->Gives.Add(TEXT("radiant_essence"), 3);
			S->Gives.Add(TEXT("gold_flakes"), 8);
			S->Gives.Add(TEXT("@gem"), 2);
			S->Gives.Add(TEXT("lore_last_king_disc"), 1);
			S->Lore = TEXT("A gold sun-disc as wide as your hand, a crown cut into its rim, laid face down: the last king of the Sunwatch turned his sun away.");
		}
	}
	// The Scarab digger's finds (cook 2.12.1): a sand mound every ~200 m over D1-D3, shown and dug only with a tamed
	// Scarab digger near (WRDesertLife marks them).
	int32 NDigs = 0;
	for (int32 i = 0; i < D.Digs.Num(); ++i)
	{
		AWRDesertSpot* S = SpawnSpot(TEXT("dig_mound"), FName(*FString::Printf(TEXT("dig_%d"), i)), D.Digs[i] + FVector(0, 0, 40.f), TEXT("A sand mound your Scarab digger found"), TEXT("Dig it up"), 2.5f);
		if (!S) continue;
		S->bConcealed = true;
		S->RespawnHours = T.Num(TEXT("desertWorld.digHours"), 72.f);
		S->Gives.Add(i % 3 == 0 ? TEXT("copper_ingot") : i % 3 == 1 ? TEXT("gold_flakes") : TEXT("sunstone"), 2);
		S->Gives.Add(TEXT("bone"), 1);
		++NDigs;
	}
	UE_LOG(LogWildreach, Log, TEXT("DESERT server: %d nodes, %d spots, %d caches, %d run-back stones, %d tilted caches, %d dig mounds"), Nodes, NSpots, NCaches, NStones, NTilted, NDigs);
}

void UWRDesertWorld::StepLights(float Dt)
{
	// Night blocks (sunstone glow, lamp niches, braziers' embers) show from dusk; in the caves and the Hourhold they're
	// always on (it's dark down there).
	LightClock -= Dt;
	if (LightClock > 0.f) return;
	LightClock = 0.5f;
	const bool bNight = AWRGameState::IsNightIn(GetWorld());
	for (const TWeakObjectPtr<AWRDesertScenery>& W : Scenery)
	{
		AWRDesertScenery* S = W.Get();
		if (!S) continue;
		const bool bLit = bNight || S->Kind == TEXT("cave") || S->Kind == TEXT("hh_room") || S->Kind == TEXT("hh_link") || S->Kind == TEXT("hh_way");
		for (int32 i = 0; i < S->NightLights.Num(); ++i)
		{
			if (UPointLightComponent* L = S->NightLights[i]) L->SetIntensity(bLit ? (S->NightPower.IsValidIndex(i) ? S->NightPower[i] : 1000.f) : 0.f);
		}
		if (S->NightRoot && S->NightRoot->IsVisible() != bLit) S->NightRoot->SetVisibility(bLit, true);
	}
}

void UWRDesertWorld::StepServer(float Dt)
{
	ServerClock += Dt;
	if (ServerClock < 2.f) return;
	ServerClock = 0.f;
	const double Hours = DwGameHours(this);
	for (const TWeakObjectPtr<AWRWorldCache>& W : Caches)
	{
		AWRWorldCache* C = W.Get();
		if (C && C->bTaken && C->RefillHours > 0.f && C->RefillAtHours > 0.0 && Hours >= C->RefillAtHours) C->Refill();
	}
	// The weather: a Sandstorm block ending opens the Lee Door for 3 game hours (the dunes walked off it: Kolmanskop),
	// and the next block's sunstone pays +50% (world-map's plenty after the storm).
	const FWRWeatherKind* Wx = UWRWeatherSubsystem::ActiveKind(this, WRDesert::Biome());
	const FName Now = Wx ? Wx->Id : FName(TEXT("clear"));
	if (AWRDesertState* S = AWRDesertState::Get(this))
	{
		if (LastWeather == TEXT("sandstorm") && Now != TEXT("sandstorm"))
		{
			S->LeeOpenUntil = float(Hours + DwTn(TEXT("desertWorld.leeOpenHours"), 3.f));
			S->LeeChangedAt = DwNow(this);
			WRSound::Broadcast(S, TEXT("desert.sand_pour"));
			UE_LOG(LogWildreach, Log, TEXT("DESERT the storm has passed: the Lee Door stands clear until hour %.1f"), S->LeeOpenUntil);
		}
		if (S->LeeOpenUntil > 0.f && Hours >= S->LeeOpenUntil)
		{
			S->LeeOpenUntil = 0.f;
			S->LeeChangedAt = DwNow(this);
		}
	}
	if (Now != LastWeather) LastWeather = LastWeather == TEXT("sandstorm") ? FName(TEXT("after_sandstorm")) : Now;
}

void UWRDesertWorld::Tick(float DeltaTime)
{
	StepBuild();
	if (!bSceneryDone) return;
	if (GetWorld()->GetNetMode() != NM_DedicatedServer) StepLights(DeltaTime);
	if (GetWorld()->GetNetMode() != NM_Client) StepServer(DeltaTime);
}

// ================================================================================================ interact

AWRDesertSpot* UWRDesertWorld::SpotNear(const FVector& P, float Range) const
{
	AWRDesertSpot* Best = nullptr;
	float BestD = TNumericLimits<float>::Max();
	const auto Consider = [&](AWRDesertSpot* S)
	{
		if (!S || S->bConcealed) return;
		const float R = Range > 0.f ? Range : S->Reach;
		const float D = FVector::DistSquared(S->GetActorLocation(), P);
		if (D < R * R && D < BestD) { BestD = D; Best = S; }
	};
	for (const TWeakObjectPtr<AWRDesertSpot>& W : Spots) Consider(W.Get());
	// Clients: the spots arrive by replication and aren't in this list (only the server spawned them).
	if (!Best && GetWorld()->GetNetMode() == NM_Client)
	{
		for (TActorIterator<AWRDesertSpot> It(GetWorld()); It; ++It) Consider(*It);
	}
	return Best;
}

AWRDesertSpot* UWRDesertWorld::FindSpot(FName Id) const
{
	for (const TWeakObjectPtr<AWRDesertSpot>& W : Spots) if (W.IsValid() && W->SpotId == Id) return W.Get();
	return nullptr;
}

bool UWRDesertWorld::Prompt(const AWRPlayerController* PC, FString& OutTarget, FString& OutVerb)
{
	const UWRDesertWorld* W = Get(PC);
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	if (!W || !W->bActive || !Me) return false;
	if (WRDesertHz::PullPrompt(PC, OutTarget, OutVerb)) return true;
	if (const AWRDesertSpot* S = W->SpotNear(Me->GetActorLocation()))
	{
		OutTarget = S->Title;
		OutVerb = S->DoneAt > 0.f ? FString::Printf(TEXT("%s..."), *S->Verb) : S->Verb;
		return true;
	}
	return false;
}

bool UWRDesertWorld::ClientInteract(AWRPlayerController* PC)
{
	const UWRDesertWorld* W = Get(PC);
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	if (!W || !W->bActive || !Me) return false;
	FString PullTarget, PullVerb;
	if (WRDesertHz::PullPrompt(PC, PullTarget, PullVerb)) { PC->SendCommand(TEXT("world desert pull")); return true; }
	if (W->SpotNear(Me->GetActorLocation())) { PC->SendCommand(TEXT("world desert spot")); return true; }
	return false;
}

FString UWRDesertWorld::Command(AWRPlayerController* PC, const TArray<FString>& Args)
{
	UWRDesertWorld* W = Get(PC);
	AWRCharacterBase* Me = PC ? Cast<AWRCharacterBase>(PC->GetPawn()) : nullptr;
	if (!W || !W->bActive || !Me) return TEXT("Not here");
	const FString Verb = Args.IsValidIndex(2) ? Args[2] : FString();
	if (Verb == TEXT("spot"))
	{
		if (AWRDesertSpot* S = W->SpotNear(Me->GetActorLocation())) return S->Start(PC);
		return TEXT("Nothing here");
	}
	if (Verb == TEXT("pull")) return WRDesertHz::StartPull(PC);
	return TEXT("world desert spot | pull");
}
