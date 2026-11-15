// The Void's max-health cost and its rifts (cooks/biome-void.md 2.6, 2.8, 2.17.1; cooks/build-biome-void.md 2.6). One
// currency of pain: MaxHealthLost, drawn as the grey band at the right of the health bar, given back by Rest. Rifts
// are the land's tears: a zone that drains max health (Empty vessel passes, nullward / Hollowplate halve, anchor salt
// halves again), a 6 m pull toward the tear (the same rule on the server and the owning client, so it never
// corrects), a touch that burns (through ApplyHit), and a pulse every 40-90 s: the inrush stops for a second, the
// stones' shadows sharpen, and they drop for 2 s. Numbers in tuning "void".
#include "WRVoid.h"
#include "Wildreach.h"
#include "WRAttributeSet.h"
#include "WRBuildLook.h"
#include "WRBuildLookKit.h"
#include "WRCharacter.h"
#include "WRCharacterBase.h"
#include "WRCombatLibrary.h"
#include "WRCombatTypes.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRInventory.h"
#include "WRLand.h"
#include "WRLandSites.h"
#include "WRPathComponent.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRPortalsInternal.h"
#include "WRResourceNode.h"
#include "WRSound.h"
#include "WRSurvival.h"
#include "WRTuning.h"
#include "AbilitySystemComponent.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"

using WRLookKit::FSurf;
using WRLookKit::FKit;

namespace
{
	float VT(const TCHAR* Key, float Fallback) { return WRTune(*FString::Printf(TEXT("void.%s"), Key), Fallback); }
	FLinearColor VC(uint8 R, uint8 G, uint8 B) { return FLinearColor(FColor(R, G, B)); }
	uint32 VHash(FName Id, int32 i) { return HashCombine(GetTypeHash(Id), uint32(i) * 2654435761u); }
	float VRand(FName Id, int32 i, int32 Salt) { return float(VHash(Id, i * 31 + Salt) % 10007) / 10006.f; }

	/** Timed brews on players (server): stat -> (value, until server seconds). */
	struct FBrew { TMap<FName, float> Stats; double Until = 0.0; };
	TMap<TWeakObjectPtr<const AActor>, FBrew>& Brews()
	{
		static TMap<TWeakObjectPtr<const AActor>, FBrew> M;
		return M;
	}

	double WorldNow(const UWorld* W)
	{
		const AGameStateBase* GS = W ? W->GetGameState() : nullptr;
		return GS ? GS->GetServerWorldTimeSeconds() : (W ? W->GetTimeSeconds() : 0.0);
	}

	void OnRest(AWRPlayerState* PS)
	{
		AWRCharacterBase* C = PS ? Cast<AWRCharacterBase>(PS->GetPawn()) : nullptr;
		UAbilitySystemComponent* ASC = C ? C->GetAbilitySystemComponent() : nullptr;
		if (!ASC || !C->GetAttributes()) return;
		// Rest gives back everything the Void took, and empties the Void bar (survival-crafting 2.5, biome-void 2.17.1).
		if (C->GetAttributes()->GetMaxHealthLost() > 0.f)
		{
			ASC->SetNumericAttributeBase(UWRAttributeSet::GetMaxHealthLostAttribute(), 0.f);
			if (AWRPlayerController* PC = Cast<AWRPlayerController>(C->GetController())) PC->ClientNotify(TEXT("You wake whole: what the Void took is back."));
		}
		ASC->SetNumericAttributeBase(UWRAttributeSet::GetVoidAttribute(), 0.f);
	}
}

// ================================================================================================ max health

void WRVoid::LoseMaxHealth(AWRCharacterBase* Who, float Amount, AActor* Source, bool bQuiet)
{
	if (!Who || !Who->HasAuthority() || !Who->IsAlive() || Amount <= 0.f) return;
	UAbilitySystemComponent* ASC = Who->GetAbilitySystemComponent();
	const UWRAttributeSet* A = Who->GetAttributes();
	if (!ASC || !A) return;
	const float Was = A->GetMaxHealthLost();
	ASC->SetNumericAttributeBase(UWRAttributeSet::GetMaxHealthLostAttribute(), Was + Amount);
	// Health can't stay over what's left.
	if (A->GetHealth() > A->GetEffectiveMaxHealth()) ASC->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), A->GetEffectiveMaxHealth());
	if (!bQuiet && FMath::FloorToInt(Was / 5.f) != FMath::FloorToInt((Was + Amount) / 5.f)) WRSound::PlayOn(Who, TEXT("void.maxhp.crack"));
	// The end of it: nothing left to hold you (Unmade). No floor.
	if (A->GetEffectiveMaxHealth() <= 0.5f)
	{
		ASC->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), 0.f);
		if (AWRPlayerController* PC = Cast<AWRPlayerController>(Who->GetController())) PC->ClientNotify(TEXT("Unmade: the Void took all of you."));
		UE_LOG(LogWildreach, Log, TEXT("VOID %s unmade (%s)"), *Who->GetName(), *GetNameSafe(Source));
		Who->Die(Source);
	}
}

void WRVoid::RestoreMaxHealth(AWRCharacterBase* Who, float Amount)
{
	if (!Who || !Who->HasAuthority() || Amount <= 0.f) return;
	UAbilitySystemComponent* ASC = Who->GetAbilitySystemComponent();
	const UWRAttributeSet* A = Who->GetAttributes();
	if (!ASC || !A || A->GetMaxHealthLost() <= 0.f) return;
	ASC->SetNumericAttributeBase(UWRAttributeSet::GetMaxHealthLostAttribute(), FMath::Max(0.f, A->GetMaxHealthLost() - Amount));
}

float WRVoid::BrewStat(const AActor* Who, FName Stat)
{
	const FBrew* B = Brews().Find(Who);
	if (!B || !Who || WorldNow(Who->GetWorld()) > B->Until) return 0.f;
	const float* V = B->Stats.Find(Stat);
	return V ? *V : 0.f;
}

FString WRVoid::Brew(AWRCharacterBase* Who, const FWRItemDef& Def)
{
	if (!Who || Def.Brew.Num() == 0) return FString();
	FBrew& B = Brews().FindOrAdd(Who);
	B.Stats = Def.Brew;
	B.Until = WorldNow(Who->GetWorld()) + Def.BrewMinutes * 60.0;
	return FString::Printf(TEXT("%s: %d min"), *Def.Name, FMath::RoundToInt(Def.BrewMinutes));
}

float WRVoid::RiftDrainMult(const AWRCharacterBase* Who)
{
	if (!Who) return 1.f;
	// Empty vessel (the Void path's root passive, main path only) passes it entirely.
	if (const UWRPathComponent* P = UWRPathComponent::FindFor(Who))
	{
		if (P->GetMainPath() == FName(TEXT("void"))) return 0.f;
	}
	// Nullward and Hollowplate are the same halving (they don't stack: the strongest counts); the Rift heart talisman's
	// quarter rides on the same stat. Anchor salt is its own halving and does stack.
	const float Gear = FMath::Clamp(1.f + UWRInventoryComponent::GearStatFor(Who, TEXT("riftDrain")), VT(TEXT("drainFloor"), 0.5f), 1.f);
	const float Salt = FMath::Clamp(1.f + BrewStat(Who, TEXT("riftDrain")), 0.f, 1.f);
	return Gear * Salt;
}

FVector WRVoid::PullAt(const UWorld* World, const FVector& Feet)
{
	if (!World) return FVector::ZeroVector;
	const float R = VT(TEXT("pull.radius"), 600.f);
	const float Speed = VT(TEXT("pull.speed"), 180.f);
	for (TActorIterator<AWRRift> It(World); It; ++It)
	{
		const FVector T = It->GetActorLocation();
		const FVector D(T.X - Feet.X, T.Y - Feet.Y, 0.f);
		const float Dist = D.Size();
		if (Dist < R && Dist > 30.f && FMath::Abs(Feet.Z - T.Z) < It->TearHeight()) return D / Dist * Speed * FMath::Clamp(1.2f - Dist / R, 0.25f, 1.f);
	}
	return FVector::ZeroVector;
}

bool WRVoid::InRiftZone(const UWorld* World, const FVector& At)
{
	if (!World) return false;
	for (TActorIterator<AWRRift> It(World); It; ++It)
	{
		if (FVector::Dist2D(It->GetActorLocation(), At) < It->ZoneRadius && FMath::Abs(It->GetActorLocation().Z - At.Z) < 3000.f) return true;
	}
	return false;
}

void WRVoid::Start(UWorld* World)
{
	static bool bBound = false;
	if (!bBound)
	{
		bBound = true;
		UWRSurvivalComponent::OnRest.AddStatic(&OnRest);
	}
}

void WRVoid::SpawnRifts(UWorld* World, FName LandId)
{
	const FWRLandDef* D = WRLand::Find(LandId);
	const TSharedPtr<FJsonObject> Rifts = D ? D->Section(TEXT("rifts")) : nullptr;
	if (!World || World->GetNetMode() == NM_Client || !Rifts) return;
	const auto Spawn = [&](FName Id, const FVector2D& At, bool bLarge, float ZoneM, bool bSteady, int32 RubbleLo, int32 RubbleHi)
	{
		const FVector Ground = D->GroundPoint(At);
		AWRRift* R = World->SpawnActorDeferred<AWRRift>(AWRRift::StaticClass(), FTransform(Ground), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!R) return;
		R->RiftId = Id;
		R->bLarge = bLarge;
		R->bSteady = bSteady;
		R->ZoneRadius = ZoneM * 100.f;
		R->Rubble = FMath::Lerp(RubbleLo, RubbleHi, VRand(Id, 0, 7)) ;
		R->DrainMult = D->HoleAt(At, 40.f) ? 2.f : 1.f;
		R->FinishSpawning(FTransform(Ground));
		// Its ring's voidglass: nodes laid in the fused ring (4 hits, a pick or anything; back in 6 game hours).
		const int32 Nodes = bLarge ? FMath::RoundToInt(FMath::Lerp(5.f, 8.f, VRand(Id, 2, 3))) : 3;
		const float R0 = bLarge ? 500.f : 250.f, R1 = bLarge ? 900.f : 500.f;
		for (int32 n = 0; n < Nodes; ++n)
		{
			const float A = 2.f * PI * (n + 0.5f * VRand(Id, n, 71)) / Nodes;
			const float Rr = FMath::Lerp(R0, R1, 0.3f + 0.4f * VRand(Id, n, 73));
			const FVector2D NodeAt = At + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Rr / 100.f;
			const FTransform NT(FRotator(0.f, 360.f * VRand(Id, n, 79), 0.f), D->GroundPoint(NodeAt));
			if (AWRResourceNode* Node = World->SpawnActorDeferred<AWRResourceNode>(AWRResourceNode::StaticClass(), NT, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
			{
				Node->ItemId = TEXT("voidglass");
				Node->Tags.Add(TEXT("rift_ring"));   // Maren's "What the edge gives" counts these (WRVoidWorld::Harvested)
				Node->Hits = int32(VT(TEXT("ring.hits"), 4.f));
				Node->PerHit = FMath::RoundToInt(VT(TEXT("ring.perHit"), 0.6f) + VRand(Id, n, 83));
				Node->RespawnSeconds = VT(TEXT("ring.respawnSeconds"), 900.f);
				Node->GemChance = 0.f;
				Node->FinishSpawning(NT);
			}
		}
	};
	int32 N = 0;
	const TArray<TSharedPtr<FJsonValue>>* L = nullptr;
	const TSharedPtr<FJsonObject>* Big = nullptr;
	const TSharedPtr<FJsonObject>* Small = nullptr;
	Rifts->TryGetObjectField(TEXT("large"), Big);
	Rifts->TryGetObjectField(TEXT("small"), Small);
	const auto RubbleRange = [](const TSharedPtr<FJsonObject>* O, int32& Lo, int32& Hi)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (O && (*O)->TryGetArrayField(TEXT("rubble"), A) && A->Num() >= 2) { Lo = int32((*A)[0]->AsNumber()); Hi = int32((*A)[1]->AsNumber()); }
	};
	int32 BL = 5, BH = 8, SL = 2, SH = 4;
	RubbleRange(Big, BL, BH);
	RubbleRange(Small, SL, SH);
	if (Rifts->TryGetArrayField(TEXT("list"), L))
	{
		for (const TSharedPtr<FJsonValue>& V : *L)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			const bool bLarge = O->GetStringField(TEXT("size")) == TEXT("large");
			bool bSteady = false;
			O->TryGetBoolField(TEXT("steady"), bSteady);
			Spawn(FName(*O->GetStringField(TEXT("id"))), WRLand::Vec2(O->TryGetField(TEXT("at"))), bLarge, float(O->GetNumberField(TEXT("zone"))), bSteady, bLarge ? BL : SL, bLarge ? BH : SH);
			++N;
		}
	}
	// The kit pass's rift places (small rifts, rift rings, rift pools): the same seeded list every machine made.
	int32 k = 0;
	for (const WRLandSites::FKitPoi& P : WRLandSites::KitPois())
	{
		if (P.Land != LandId) continue;
		const FString Kind = P.Kind.ToString();
		if (Kind != TEXT("small_rift") && Kind != TEXT("rift_ring") && Kind != TEXT("rift_pool")) continue;
		const bool bLarge = Kind == TEXT("rift_ring") && (k % 3 == 0);
		Spawn(FName(*FString::Printf(TEXT("kit_%d"), k++)), P.At, bLarge, bLarge ? 42.f : 25.f, false, bLarge ? BL : SL, bLarge ? BH : SH);
		++N;
	}
	UE_LOG(LogWildreach, Log, TEXT("LAND %s: %d rifts"), *LandId.ToString(), N);
}

// ================================================================================================ the rift

AWRRift::AWRRift()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetNetCullDistanceSquared(FMath::Square(60000.f));
}

void AWRRift::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRRift, RiftId);
	DOREPLIFETIME(AWRRift, bLarge);
	DOREPLIFETIME(AWRRift, bSteady);
	DOREPLIFETIME(AWRRift, ZoneRadius);
	DOREPLIFETIME(AWRRift, DrainMult);
	DOREPLIFETIME(AWRRift, NextPulse);
	DOREPLIFETIME(AWRRift, Rubble);
}

double AWRRift::Now() const { return WorldNow(GetWorld()); }

void AWRRift::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority()) NextPulse = Now() + FMath::Lerp(VT(TEXT("pulse.min"), 40.f), VT(TEXT("pulse.max"), 90.f), VRand(RiftId, 1, 3));
	BuildLook();
}

void AWRRift::StoneSpot(int32 i, FVector& OutLocal, float& OutSize) const
{
	// Round the tear, 6-24 m out, 4-12 m up; bigger stones float higher (the bigger the keel, the higher: the floating rule).
	const float A = 2.f * PI * VRand(RiftId, i, 11);
	const float R = FMath::Lerp(600.f, bLarge ? 2400.f : 1300.f, VRand(RiftId, i, 13));
	OutSize = FMath::Lerp(140.f, bLarge ? 420.f : 260.f, VRand(RiftId, i, 17));
	OutLocal = FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 400.f + OutSize * 1.8f + 200.f * VRand(RiftId, i, 19));
}

float AWRRift::DropNow(double T) const
{
	if (bSteady) return 0.f;
	// The pulse: a 1.0 s tell (silence, sharpening shadows), the stones fall in 0.35 s, lie 2 s, rise over 3 s.
	const double Last = NextPulse;
	const float Tell = VT(TEXT("pulse.tell"), 1.0f), Fall = 0.35f, Lie = VT(TEXT("pulse.drop"), 2.0f), Rise = 3.f;
	// The pulse that matters is the one now running: NextPulse moves on only after it has finished (server), so the
	// window is [NextPulse, NextPulse + tell + fall + lie + rise].
	const float S = float(T - Last);
	if (S < Tell) return 0.f;
	if (S < Tell + Fall) return (S - Tell) / Fall;
	if (S < Tell + Fall + Lie) return 1.f;
	if (S < Tell + Fall + Lie + Rise) return 1.f - (S - Tell - Fall - Lie) / Rise;
	return 0.f;
}

void AWRRift::BuildLook()
{
	if (bBuilt || GetNetMode() == NM_DedicatedServer) return;
	bBuilt = true;
	const AWRLand* Land = UWRLandSubsystem::FindLand(GetWorld(), NAME_None);
	UMaterialInterface* Tear = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_VoidTear.M_VoidTear"));
	UMaterialInterface* LightM = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_VoidLight.M_VoidLight"));
	const float H = TearHeight(), W = TearWidth();
	// The tear: a thin double-convex lens standing on its lower tip (front and back faces, a sharp rim), so from the side
	// it's a black sliver and from the front a hole with stars in it.
	{
		TArray<FVector> V;
		TArray<int32> T;
		TArray<FVector> N;
		const int32 Seg = 40;
		const float Thick = W * 0.12f;
		V.Add(FVector(Thick, 0.f, H * 0.5f)); N.Add(FVector(1, 0, 0));
		V.Add(FVector(-Thick, 0.f, H * 0.5f)); N.Add(FVector(-1, 0, 0));
		for (int32 s = 0; s < Seg; ++s)
		{
			const float A = 2.f * PI * s / Seg;
			// An almond: wide in the middle, sharp at the top and bottom tips, the edge a little ragged.
			const float Y = FMath::Sin(A) * W * 0.5f * FMath::Pow(FMath::Abs(FMath::Sin(A)), 0.35f) * (1.f + 0.08f * FMath::Sin(A * 7.f));
			const float Z = H * 0.5f + FMath::Cos(A) * H * 0.5f;
			V.Add(FVector(0.f, Y, Z)); N.Add(FVector(0, FMath::Sign(Y), 0));
		}
		for (int32 s = 0; s < Seg; ++s)
		{
			const int32 A = 2 + s, B = 2 + (s + 1) % Seg;
			T.Append({ 0, A, B, 1, B, A });
		}
		UProceduralMeshComponent* M = NewObject<UProceduralMeshComponent>(this);
		M->SetupAttachment(RootComponent);
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M->CreateMeshSection_LinearColor(0, V, T, N, TArray<FVector2D>(), TArray<FLinearColor>(), TArray<FProcMeshTangent>(), false);
		if (Tear) M->SetMaterial(0, Tear);
		M->SetRelativeRotation(FRotator(0.f, 360.f * VRand(RiftId, 0, 23), 0.f));
		M->RegisterComponent();
	}
	// The lensing halo round it (1.5x the tear's height) and its light: violet, the land's night light.
	if (LightM)
	{
		UMaterialInstanceDynamic* Halo = UMaterialInstanceDynamic::Create(LightM, this);
		Halo->SetVectorParameterValue(TEXT("Color"), VC(123, 79, 214));
		Halo->SetScalarParameterValue(TEXT("Glow"), 0.45f);
		Halo->SetScalarParameterValue(TEXT("Edge"), 2.5f);
		WRFoldLook::Shape(this, RootComponent, TEXT("Sphere"), FVector(H * 0.75f, H * 0.75f, H * 1.5f), FVector(0, 0, H * 0.5f), FRotator::ZeroRotator, Halo);
	}
	WRFoldLook::Light(this, RootComponent, FVector(0, 0, H * 0.5f), VC(123, 79, 214), bLarge ? 18000.f : 7000.f, bLarge ? 6000.f : 2000.f);
	// The ring of fused glass where the tear's edge touched the ground: plates and shards laid round it, violet at their
	// edges; and dead grass standing straight inside the zone (drawn by the land's decoration).
	TArray<FWRLookBlock> B;
	FKit K(GetActorTransform(), B);
	const FSurf Glass = FSurf::Glass(VC(11, 10, 16), 0.f);
	const FSurf Edge = FSurf::Lit(VC(123, 79, 214), 0.5f);
	const float R0 = bLarge ? 500.f : 250.f, R1 = bLarge ? 900.f : 500.f;
	const int32 Plates = bLarge ? 40 : 18;
	for (int32 i = 0; i < Plates; ++i)
	{
		const float A = 2.f * PI * (i + VRand(RiftId, i, 29) * 0.6f) / Plates;
		const float R = FMath::Lerp(R0, R1, VRand(RiftId, i, 31));
		const FVector P(FMath::Cos(A) * R, FMath::Sin(A) * R, 0.f);
		const FVector G = Land ? FVector(P.X, P.Y, Land->GroundZ(GetActorLocation() + P) - GetActorLocation().Z) : P;
		const float S = FMath::Lerp(90.f, 220.f, VRand(RiftId, i, 37));
		K.Add(Glass, FVector(S, S * 0.7f, 10.f), G + FVector(0, 0, 3.f), FRotator(4.f * (VRand(RiftId, i, 41) - 0.5f), FMath::RadiansToDegrees(A) + 40.f * VRand(RiftId, i, 43), 4.f * (VRand(RiftId, i, 47) - 0.5f)));
		if (i % 3 == 0) K.Spike(Glass, G + FVector(0, 0, 4.f), FVector(FMath::Cos(A) * 0.4f, FMath::Sin(A) * 0.4f, 1.f), 26.f, 60.f + 90.f * VRand(RiftId, i, 53));
		if (i % 2 == 0) K.Add(Edge, FVector(S * 0.9f, 3.f, 2.f), G + FVector(0, 0, 9.f), FRotator(0.f, FMath::RadiansToDegrees(A) + 90.f, 0.f));
	}
	FWRBuildLook::Spawn(this, RootComponent, B, VC(163, 158, 149), false);
	// The floating stones: the pack's rocks, grey, each on a black keel (their place is a function of the clock).
	UMaterialInterface* RockMat = WRFoldLook::RockMat(this, VC(150, 146, 140) * 0.3f);
	for (int32 i = 0; i < Rubble; ++i)
	{
		FVector At;
		float S = 0.f;
		StoneSpot(i, At, S);
		USceneComponent* Holder = NewObject<USceneComponent>(this);
		Holder->SetupAttachment(RootComponent);
		Holder->SetRelativeLocation(At);
		Holder->RegisterComponent();
		WRFoldLook::Rock(this, Holder, i, FVector(S, S * 0.85f, S * 0.6f), FVector::ZeroVector, FRotator(0.f, 360.f * VRand(RiftId, i, 59), 0.f), RockMat, false, S * 0.1f);
		TArray<FWRLookBlock> KB;
		FKit KK(Holder->GetComponentTransform(), KB);
		KK.Spike(Glass, FVector(0, 0, 6.f), FVector(0, 0, -1.f), S * 0.3f, S * 0.7f);
		KK.Add(Edge, FVector(S * 0.3f, S * 0.3f, 2.f), FVector(0, 0, -S * 0.2f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		FWRBuildLook::Spawn(this, Holder, KB, VC(163, 158, 149), false);
		Stones.Add(Holder);
		StoneBase.Add(Land ? Land->GroundZ(GetActorLocation() + FVector(At.X, At.Y, 0.f)) - GetActorLocation().Z : 0.f);
	}
	// Dust and pebbles floating 10-40 cm up inside the zone, streaming in near the tear; the inrush.
	if (USceneComponent* F = WRFx::Attach(RootComponent, bLarge ? TEXT("rift.motes.large") : TEXT("rift.motes"), NAME_None, 1.f)) F->SetRelativeLocation(FVector(0, 0, 20.f));
	if (USceneComponent* F = WRFx::Attach(RootComponent, TEXT("rift.edge"), NAME_None, bLarge ? 2.f : 1.f)) F->SetRelativeLocation(FVector(0, 0, H * 0.5f));
	Inrush = WRSound::StartLoop(this, bLarge ? TEXT("void.rift.inrush.large") : TEXT("void.rift.inrush.small"));
}

void AWRRift::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double T = Now();
	if (HasAuthority())
	{
		StepAcc += DeltaSeconds;
		if (StepAcc >= 0.25f)
		{
			ServerStep(StepAcc);
			StepAcc = 0.f;
		}
	}
	if (!bBuilt) return;
	// Stones: bobbing on the clock; in a pulse they fall to the ground under them and lie there, then rise back.
	const float Drop = DropNow(T);
	for (int32 i = 0; i < Stones.Num(); ++i)
	{
		FVector At;
		float S = 0.f;
		StoneSpot(i, At, S);
		const float Bob = 25.f * FMath::Sin(float(FMath::Fmod(T, 9.0) / 9.0) * 2.f * PI + VRand(RiftId, i, 61) * 6.f);
		const float Floor = StoneBase.IsValidIndex(i) ? StoneBase[i] + S * 0.35f : 0.f;
		Stones[i]->SetRelativeLocation(FVector(At.X, At.Y, FMath::Lerp(At.Z + Bob, Floor, Drop)));
	}
	// The pulse's tell: the inrush cuts out for the second before the drop (sudden silence), the grind as they fall.
	if (Inrush)
	{
		const float S = float(T - NextPulse);
		const bool bSilent = !bSteady && S >= 0.f && S < VT(TEXT("pulse.tell"), 1.0f);
		Inrush->SetVolumeMultiplier(bSilent ? 0.f : 1.f);
	}
}

void AWRRift::ServerStep(float Dt)
{
	const double T = Now();
	const float Drain = VT(TEXT("riftDrain"), 2.f) * DrainMult;
	const FVector Me = GetActorLocation();
	for (TActorIterator<AWRCharacter> It(GetWorld()); It; ++It)
	{
		AWRCharacter* C = *It;
		if (!C->IsAlive() || !C->GetPlayerState()) continue;
		const FVector P = C->GetActorLocation();
		const float D = FVector::Dist2D(P, Me);
		if (D > ZoneRadius || FMath::Abs(P.Z - Me.Z) > 3000.f) continue;
		// The zone: max health, and the world's Corruption at the night rate.
		const float Mult = WRVoid::RiftDrainMult(C);
		if (Mult > 0.f) WRVoid::LoseMaxHealth(C, Drain * Mult * Dt, this);
		if (UWRSurvivalComponent* S = UWRSurvivalComponent::For(C)) S->AddCorruption(VT(TEXT("riftCorruption"), 0.2f) * Dt);
		// The touch: the tear burns (unblockable; it throws you out).
		const FVector Tc = TearCentre();
		if (FVector::Dist2D(P, Me) < TearWidth() * 0.6f + 40.f && P.Z < Me.Z + TearHeight() + 100.f)
		{
			double& Last = TouchedAt.FindOrAdd(C);
			if (T - Last > 1.2)
			{
				Last = T;
				FWRHit Hit;
				Hit.Instigator = this;
				Hit.Damage = VT(TEXT("touch.damage"), 40.f);
				Hit.Element = WRTags::Element_Void;
				Hit.bUnblockable = true;
				Hit.Impact = 3;
				Hit.Origin = Tc;
				Hit.Buildups.Add(TEXT("curse"), VT(TEXT("touch.curse"), 40.f));
				UWRCombatLibrary::ApplyHit(C, Hit);
				const FVector Out = (P - Me).GetSafeNormal2D();
				C->LaunchCharacter((Out.IsNearlyZero() ? FVector(1, 0, 0) : Out) * VT(TEXT("touch.knock"), 600.f) + FVector(0, 0, 250.f), true, true);
				WRFx::Broadcast(this, TEXT("hit.void"), P);
			}
		}
	}
	// The pulse: stones dropped this pulse hit whoever is under them (once), then the next pulse is set.
	if (!bSteady && Rubble > 0)
	{
		const float S = float(T - NextPulse);
		const float Tell = VT(TEXT("pulse.tell"), 1.0f);
		if (S >= Tell && LastPulseHit < NextPulse)
		{
			LastPulseHit = NextPulse;
			WRSound::PlayAt(this, TEXT("void.rift.grind"), TearCentre());
			for (int32 i = 0; i < Rubble; ++i)
			{
				FVector At;
				float Size = 0.f;
				StoneSpot(i, At, Size);
				const FVector Wd = Me + FVector(At.X, At.Y, 0.f);
				for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
				{
					if (!It->IsAlive() || FVector::Dist2D(It->GetActorLocation(), Wd) > Size * 0.5f + 40.f || FMath::Abs(It->GetActorLocation().Z - Wd.Z) > 600.f) continue;
					FWRHit Hit;
					Hit.Instigator = this;
					Hit.Damage = VT(TEXT("rubble.damage"), 45.f);
					Hit.Impact = 3;
					Hit.Origin = Wd + FVector(0, 0, 300.f);
					Hit.PhysType = 2;
					UWRCombatLibrary::ApplyHit(*It, Hit);
				}
			}
		}
		if (S > Tell + 0.35f + VT(TEXT("pulse.drop"), 2.0f) + 3.f)
		{
			NextPulse = T + FMath::Lerp(VT(TEXT("pulse.min"), 40.f), VT(TEXT("pulse.max"), 90.f), FMath::FRand());
		}
	}
}
