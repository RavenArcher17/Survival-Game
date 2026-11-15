// The Fungal caverns' doors and secrets (see WRFungalHazards.h AWRFungalSpot; cooks/biome-fungal.md 2.5, 2.20): the
// Sealed Gate's web sheet, the mole wall, the fifth tunnel, the Ring of Caps' altar, the Heart Gate and the Seed Vault;
// and the E prompt chain (UWRFungalWorld::Prompt / ClientInteract / Command, "world fungal spot").

#include "WRFungalHazards.h"
#include "WRFungalWorld.h"
#include "WRFungalLook.h"
#include "WRFungalTypes.h"
#include "Wildreach.h"
#include "WRCharacterBase.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRGameState.h"
#include "WRInventory.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRResourceNode.h"
#include "WRSound.h"
#include "WRSurvival.h"
#include "WRToolMesh.h"
#include "WRTuning.h"
#include "WRWorldSystems.h"
#include "Components/BoxComponent.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"

namespace WRFungalSpotImpl
{
	/** A tame of this kind belonging to the player within Range of At (the mole that digs, the toad on the altar). */
	AWRCreature* TameNear(UWorld* W, const AWRPlayerController* PC, FName Kind, const FVector& At, float Range)
	{
		const APlayerState* PS = PC ? PC->PlayerState : nullptr;
		if (!W || !PS) return nullptr;
		for (TActorIterator<AWRCreature> It(W); It; ++It)
		{
			if (It->CreatureId == Kind && It->IsAlive() && It->IsTamed() && It->GetTameOwner() == PS->GetPlayerName() && FVector::Dist(It->GetActorLocation(), At) < Range) return *It;
		}
		return nullptr;
	}
	bool HoldsPick(const AActor* Who)
	{
		const UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Who);
		if (!Inv) return false;
		const FWRItemStack& H = Inv->GetEquipped(EWREquipSlot::Weapon);
		const FWRToolDef* T = H.IsEmpty() ? nullptr : FWRGathering::FindTool(H.ItemId);
		return T && T->Kind == TEXT("pick") && H.Durability > 0.f;
	}
}

AWRFungalSpot::AWRFungalSpot()
{
	bReplicates = true;
	SetReplicateMovement(false);
	SetNetCullDistanceSquared(FMath::Square(25000.f));
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRFungalSpot::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRFungalSpot, Kind);
	DOREPLIFETIME(AWRFungalSpot, Title);
	DOREPLIFETIME(AWRFungalSpot, Verb);
	DOREPLIFETIME(AWRFungalSpot, bOpen);
	DOREPLIFETIME(AWRFungalSpot, OpenAt);
	DOREPLIFETIME(AWRFungalSpot, Progress);
}

const TArray<TWeakObjectPtr<AWRFungalSpot>>& AWRFungalSpot::All()
{
	static TArray<TWeakObjectPtr<AWRFungalSpot>> L;
	return L;
}

void AWRFungalSpot::BeginPlay()
{
	Super::BeginPlay();
	const_cast<TArray<TWeakObjectPtr<AWRFungalSpot>>&>(All()).AddUnique(this);
	Yaw = GetActorRotation().Yaw;
	BuildLook();
	UpdateLook();
}

void AWRFungalSpot::EndPlay(const EEndPlayReason::Type Reason)
{
	const_cast<TArray<TWeakObjectPtr<AWRFungalSpot>>&>(All()).Remove(this);
	Super::EndPlay(Reason);
}

void AWRFungalSpot::BuildLook()
{
	using namespace WRFungalLook;
	// What blocks the way (collision on every machine): a box across the passage; it goes when open.
	UBoxComponent* B = NewObject<UBoxComponent>(this);
	B->SetupAttachment(GetRootComponent());
	B->SetBoxExtent(FVector(40.f, Width * 0.5f, HeightCm * 0.5f));
	B->SetRelativeLocation(FVector(0.f, 0.f, HeightCm * 0.5f));
	B->SetCollisionProfileName(TEXT("BlockAll"));
	B->RegisterComponent();
	Blockers.Add(B);
	if (GetNetMode() == NM_DedicatedServer) return;
	Moving = NewObject<USceneComponent>(this);
	Moving->SetupAttachment(GetRootComponent());
	Moving->RegisterComponent();
	FWRToolMesh M;
	if (Kind == TEXT("sealed_gate") || Kind == TEXT("web_sheet"))
	{
		// A web sheet 12 m across: layer on layer of silk across the round door, sagging, glinting (the veil mesh).
		FWRToolMesh V;
		TArray<TArray<FVector>> Rings;
		for (int32 r = 0; r <= 6; ++r)
		{
			TArray<FVector> Ring;
			const float Z = HeightCm * r / 6.f;
			for (int32 s = 0; s <= 8; ++s) Ring.Add(FVector(10.f * FMath::Sin(r * 1.3f + s), (s / 8.f - 0.5f) * Width, Z - 30.f * FMath::Sin(s / 8.f * PI)));
			Rings.Add(Ring);
		}
		V.Loft(Rings, FLinearColor(0.92f, 0.92f, 0.88f), 0.18f, false);
		Sheet = NewObject<UProceduralMeshComponent>(this);
		Sheet->SetupAttachment(Moving);
		Sheet->RegisterComponent();
		V.ApplyTo(Sheet);
		Sheet->SetMaterial(0, VeilMaterial(this));
		return;
	}
	if (Kind == TEXT("mole_wall") || Kind == TEXT("fifth_tunnel"))
	{
		// Collapsed rubble (the mole's crawl) or a blank wall that sounds hollow (the fifth tunnel's).
		for (int32 r = 0; r < 9; ++r)
		{
			const float Y = (r % 3 - 1) * Width * 0.3f, Z = (r / 3) * HeightCm * 0.3f + 50.f;
			M.Lathe(FVector(0.f, Y, Z - 60.f), FVector::UpVector, 120.f, { FVector(0.f, 70.f, 60.f), FVector(0.5f, 90.f, 80.f), FVector(1.f, 50.f, 40.f) }, 7, (Kind == TEXT("mole_wall") ? Rgb(46, 40, 34) : Rgb(150, 140, 120)) * 0.6f, 0.9f, 0.f, float(r + 2));
		}
	}
	else if (Kind == TEXT("ring_altar"))
	{
		// The altar slab: it slides back over 4 s, the stair under it.
		M.Lathe(FVector(0.f, 0.f, 0.f), FVector::UpVector, 60.f, { FVector(0.f, 170.f, 100.f), FVector(1.f, 160.f, 92.f) }, 4, StoneC(), 0.85f);
	}
	else
	{
		// A round stone door in its frame (the Heart Gate, the Seed Vault): it rolls aside along its groove.
		M.Lathe(FVector(0.f, 0.f, HeightCm * 0.5f), FVector::ForwardVector, 50.f, { FVector(0.f, HeightCm * 0.5f, HeightCm * 0.5f), FVector(1.f, HeightCm * 0.5f, HeightCm * 0.5f) }, 22, Rgb(150, 140, 120) * 0.6f, 0.85f);
		for (int32 g = 0; g < 7; ++g) M.Lathe(FVector(-6.f, 0.f, HeightCm * 0.5f), FVector::ForwardVector, 4.f, { FVector(0.f, HeightCm * (0.1f + g * 0.055f), HeightCm * (0.1f + g * 0.055f)), FVector(1.f, HeightCm * (0.1f + g * 0.055f) - 4.f, HeightCm * (0.1f + g * 0.055f) - 4.f) }, 22, Rgb(100, 92, 80) * 0.6f, 0.9f);
	}
	if (M.Verts.Num())
	{
		UProceduralMeshComponent* P = NewObject<UProceduralMeshComponent>(this);
		P->SetupAttachment(Moving);
		P->RegisterComponent();
		M.ApplyTo(P);
	}
}

void AWRFungalSpot::OnRep_Open()
{
	UpdateLook();
}

void AWRFungalSpot::UpdateLook()
{
	for (UPrimitiveComponent* B : Blockers) if (B) B->SetCollisionEnabled(bOpen ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
	if (bOpen != bLookOpen)
	{
		bLookOpen = bOpen;
		if (GetNetMode() != NM_DedicatedServer)
		{
			if (Kind == TEXT("sealed_gate") || Kind == TEXT("web_sheet")) WRFx::SpawnAt(this, bOpen ? TEXT("fungal.webFall") : TEXT("fungal.webGrow"), GetActorLocation() + FVector(0.f, 0.f, HeightCm * 0.5f), GetActorRotation(), Width / 600.f);
			else if (Kind == TEXT("mole_wall") || Kind == TEXT("fifth_tunnel")) WRFx::SpawnAt(this, TEXT("fungal.rubbleBreak"), GetActorLocation() + FVector(0.f, 0.f, 100.f), GetActorRotation(), 1.f);
		}
	}
}

void AWRFungalSpot::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Now = WRFungal::Now(this);
	if (Moving)
	{
		// Doors move on their mechanism, eased: a slab slides back along its bed, a round door rolls aside in its groove, a
		// web falls away, rubble is dug out (hidden as it breaks).
		const float T = bOpen ? FMath::Clamp((Now - OpenAt) / 4.f, 0.f, 1.f) : 0.f;
		const float E = T * T * (3.f - 2.f * T);
		if (Kind == TEXT("ring_altar")) Moving->SetRelativeLocation(FVector(-340.f * E, 0.f, 0.f));
		else if (Kind == TEXT("heart_gate") || Kind == TEXT("seed_vault")) { Moving->SetRelativeLocation(FVector(0.f, Width * 0.9f * E, 0.f)); Moving->SetRelativeRotation(FRotator(0.f, 0.f, -E * 200.f)); }
		else Moving->SetVisibility(!bOpen || T < 0.05f, true);
		if (Sheet && !bOpen) Sheet->SetRelativeScale3D(FVector(1.f, 1.f, 1.f - 0.4f * Progress));
	}
	if (!HasAuthority()) return;
	// The web's burning (3 s from a flame), a mole's dig (20 s), closing again.
	if (BurnLeft > 0.f)
	{
		BurnLeft -= DeltaSeconds;
		Progress = 1.f - BurnLeft / WRTune(TEXT("hazards.fungal.gate.burnSeconds"), 3.f);
		if (BurnLeft <= 0.f) Open(BurnBy.Get(), WRTune(TEXT("hazards.fungal.gate.openHours"), 2.f) * WRTune(TEXT("clock.dayMinutes"), 60.f) * 2.5f);
	}
	if (bOpen && CloseAt > 0.f && Now >= CloseAt) Close();
	if (Kind == TEXT("seed_vault"))
	{
		const bool bWant = WRFungal::Sporefall(this) == 2;
		if (bWant && !bOpen) Open(nullptr);
		else if (!bWant && bOpen) Close();
	}
	if (Kind == TEXT("ring_altar") && bOpen && !AWRGameState::IsNightIn(this)) Close();
}

void AWRFungalSpot::Open(AActor* By, float Seconds)
{
	if (!HasAuthority() || bOpen) return;
	bOpen = true;
	OpenAt = WRFungal::Now(this);
	CloseAt = Seconds > 0.f ? OpenAt + Seconds : 0.f;
	BurnLeft = 0.f;
	Progress = 1.f;
	UpdateLook();
	WRSound::PlayAt(this, Kind == TEXT("sealed_gate") ? TEXT("fungal.web_tear") : Kind == TEXT("ring_altar") ? TEXT("fungal.altar_slide") : TEXT("fungal.door_roll"), GetActorLocation());
	UE_LOG(LogWildreach, Log, TEXT("FUNGAL %s opens (%s)"), *Kind.ToString(), By ? *By->GetName() : TEXT("by itself"));
}

void AWRFungalSpot::Close()
{
	if (!HasAuthority() || !bOpen) return;
	bOpen = false;
	Progress = 0.f;
	CloseAt = 0.f;
	UpdateLook();
}

void AWRFungalSpot::Wind(AActor* By)
{
	// Any wind hit (a gust, the Gale flask, a Draught gate) tears the web sheet down (fungal 2.5 way 1).
	if (Kind == TEXT("sealed_gate") || Kind == TEXT("web_sheet")) Open(By, WRTune(TEXT("hazards.fungal.gate.openHours"), 2.f) * WRTune(TEXT("clock.dayMinutes"), 60.f) * 2.5f);
}

void AWRFungalSpot::Fire(AActor* By)
{
	if ((Kind != TEXT("sealed_gate") && Kind != TEXT("web_sheet")) || bOpen || BurnLeft > 0.f) return;
	BurnLeft = WRTune(TEXT("hazards.fungal.gate.burnSeconds"), 3.f);
	BurnBy = By;
	WRFx::Broadcast(this, TEXT("fungal.webBurn"), GetActorLocation() + FVector(0.f, 0.f, HeightCm * 0.4f), GetActorRotation(), Width / 600.f, TEXT("fungal.web_burn"));
}

FString AWRFungalSpot::Use(AWRPlayerController* PC)
{
	using namespace WRFungalSpotImpl;
	AWRCharacterBase* Me = PC ? Cast<AWRCharacterBase>(PC->GetPawn()) : nullptr;
	if (!Me || !HasAuthority()) return TEXT("Nothing here");
	UWorld* W = GetWorld();
	const FVector At = GetActorLocation();
	if (Kind == TEXT("sealed_gate"))
	{
		if (bOpen) return TEXT("The way into the Heart Grotto is open");
		if (WRFungal::CarriesFire(Me)) { Fire(Me); return TEXT("The silk catches: it's burning"); }
		return TEXT("Webbed shut: burn it (a torch, any fire) or blow it down (any wind)");
	}
	if (Kind == TEXT("mole_wall"))
	{
		if (bOpen) return TEXT("The crawl is open");
		AWRCreature* Mole = TameNear(W, PC, TEXT("truffle_mole"), At, 1500.f);
		if (!Mole) return TEXT("Collapsed. A tame Truffle mole could dig through it");
		if (Progress > 0.f && Progress < 1.f) return TEXT("Your mole is digging");
		// The dig: 20 s, the mole at the rubble (it walks there; WRFungalCreatures plays the dig).
		Progress = 0.01f;
		Mole->SetActorLocation(At + GetActorForwardVector() * -150.f, true);
		TWeakObjectPtr<AWRFungalSpot> Weak(this);
		TWeakObjectPtr<AWRCharacterBase> Who(Me);
		FTimerHandle H;
		W->GetTimerManager().SetTimer(H, [Weak, Who]() { if (Weak.IsValid()) Weak->Open(Who.Get()); }, WRTune(TEXT("hazards.fungal.moleDig"), 20.f), false);
		WRFx::Broadcast(this, TEXT("fungal.moleDig"), At + FVector(0.f, 0.f, 60.f), GetActorRotation(), 1.f, TEXT("fungal.mole_dig"));
		return TEXT("Your mole sets to digging (20 s)");
	}
	if (Kind == TEXT("fifth_tunnel"))
	{
		if (bOpen) return TEXT("The fifth tunnel is open");
		if (Progress <= 0.f)
		{
			if (!TameNear(W, PC, TEXT("truffle_mole"), At, 1500.f)) return TEXT("A blank wall. It sounds like a wall");
			Progress = 0.001f;
			WRSound::PlayAt(this, TEXT("fungal.mole_knock"), At);
			return TEXT("Your mole knocks at the wall... knock-knock, knock-knock-knock. It's hollow behind: break it with a pick");
		}
		if (!HoldsPick(Me)) return TEXT("Hollow behind. Break it with a pick");
		Progress = FMath::Min(1.f, Progress + 1.f / WRTune(TEXT("hazards.fungal.fifthHits"), 6.f));
		WRFx::Broadcast(this, TEXT("gather.stone"), At + FVector(0.f, 0.f, 120.f), GetActorRotation(), 1.f, TEXT("gather.pick"));
		if (Progress >= 0.999f) { Open(Me); return TEXT("The wall gives way"); }
		return TEXT("The wall cracks");
	}
	if (Kind == TEXT("ring_altar"))
	{
		if (bOpen) return TEXT("The stair under the altar is open");
		// Midnight with a Tunnel toad: the toads croak in turn, the stones light one by one, the altar slides (F-S1).
		const float Hour = AWRGameState::HourIn(this);
		const bool bMidnight = Hour >= 23.5f || Hour < 0.5f;
		if (!TameNear(W, PC, TEXT("tunnel_toad"), At, 3000.f)) return TEXT("A flat altar stone, carved with a toad's foot");
		if (!bMidnight) return TEXT("Your toad croaks once. The stones stay dark (come back at midnight)");
		WRFx::Broadcast(this, TEXT("fungal.ringStones"), At, FRotator::ZeroRotator, 1.f, TEXT("fungal.ring_croak"));
		Open(Me);
		return TEXT("The toads croak in turn round the ring; the stones light one by one; the altar slides back");
	}
	if (Kind == TEXT("heart_gate"))
	{
		if (bOpen) return TEXT("Open");
		// It opens from inside only (the Heart Grotto's side: behind its face).
		const FVector Face = GetActorForwardVector();
		if (FVector::DotProduct(Me->GetActorLocation() - At, Face) < 0.f) return TEXT("A round stone door. It doesn't open from this side");
		Open(Me, WRTune(TEXT("hazards.fungal.heartGateSeconds"), 30.f));
		return TEXT("The Heart Gate rolls aside");
	}
	if (Kind == TEXT("seed_vault")) return bOpen ? TEXT("The Seed Vault stands open") : TEXT("Sealed. It opens when the Deep fruits (the Sporefall)");
	return TEXT("Nothing here");
}

// ================================================================================================ the E prompt chain

namespace WRFungalSpotImpl
{
	AWRFungalSpot* SpotNear(const UWorld* W, const FVector& At)
	{
		AWRFungalSpot* Best = nullptr;
		float BestD = TNumericLimits<float>::Max();
		for (const TWeakObjectPtr<AWRFungalSpot>& S : AWRFungalSpot::All())
		{
			if (!S.IsValid() || S->GetWorld() != W) continue;
			const float D = FVector::Dist(S->GetActorLocation() + FVector(0.f, 0.f, 100.f), At);
			if (D < S->Reach && D < BestD) { BestD = D; Best = S.Get(); }
		}
		return Best;
	}
}

bool UWRFungalWorld::Prompt(const AWRPlayerController* PC, FString& OutTarget, FString& OutVerb)
{
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	const UWRFungalWorld* W = Get(PC);
	if (!W || !W->bActive || !Me) return false;
	const AWRFungalSpot* S = WRFungalSpotImpl::SpotNear(Me->GetWorld(), Me->GetActorLocation());
	if (!S) return false;
	OutTarget = S->Title;
	OutVerb = S->bOpen ? FString(TEXT("Open")) : S->Verb;
	return true;
}

bool UWRFungalWorld::ClientInteract(AWRPlayerController* PC)
{
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	const UWRFungalWorld* W = Get(PC);
	if (!W || !W->bActive || !Me) return false;
	if (!WRFungalSpotImpl::SpotNear(Me->GetWorld(), Me->GetActorLocation())) return false;
	PC->SendCommand(TEXT("world fungal spot"));
	return true;
}

FString UWRFungalWorld::Command(AWRPlayerController* PC, const TArray<FString>& Args)
{
	UWRFungalWorld* W = Get(PC);
	AWRCharacterBase* Me = PC ? Cast<AWRCharacterBase>(PC->GetPawn()) : nullptr;
	if (!W || !W->bActive || !Me) return TEXT("Not here");
	const FString Verb = Args.IsValidIndex(2) ? Args[2] : FString();
	if (Verb == TEXT("spot"))
	{
		if (AWRFungalSpot* S = WRFungalSpotImpl::SpotNear(Me->GetWorld(), Me->GetActorLocation())) return S->Use(PC);
		return TEXT("Nothing here");
	}
	return TEXT("world fungal spot");
}
