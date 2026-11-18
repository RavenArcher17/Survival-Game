// The Unmade Place and the Descent lift (see WRVoidUnmade.h; cooks/biome-void.md 2.5).
#include "WRVoidUnmade.h"
#include "Wildreach.h"
#include "WRBuildLook.h"
#include "WRBuildLookKit.h"
#include "WRCharacter.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRInventory.h"
#include "WRLand.h"
#include "WRPlayerController.h"
#include "WRPortalsInternal.h"
#include "WRResourceNode.h"
#include "WRSocial.h"
#include "WRSound.h"
#include "WRTuning.h"
#include "WRVoid.h"
#include "WRVoidWorld.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

namespace WRUnmadeImpl
{
	using WRLookKit::FSurf;
	using WRLookKit::FKit;
	float UmT(const TCHAR* Key, float F) { return WRTune(*FString::Printf(TEXT("voidUnmade.%s"), Key), F); }
	const FLinearColor UmDolo = FLinearColor(FColor(201, 195, 184));
	const FLinearColor UmViolet = FLinearColor(FColor(0x7B, 0x4F, 0xD6));

	/** The land's Great Rift: its centre (map m), radius (m), lip height (m). */
	bool UmRift(const FWRLandDef& D, FVector2D& C, float& R, float& Lip)
	{
		const FWRLandFeature* F = D.Feature(TEXT("great_rift"));
		if (!F || F->Levels.Num() == 0) return false;
		C = F->A;
		R = F->R;
		Lip = F->Levels[0] - F->LipDrop;
		return true;
	}
	FVector UmAt(const FWRLandDef& D, const FVector2D& M, float Lip, float Depth)
	{
		return D.ToWorld(M, Lip - Depth);
	}
}
using namespace WRUnmadeImpl;

// ================================================================================================ the place

AWRUnmadePlace::AWRUnmadePlace()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRUnmadePlace::Spawn(AWRLand* Land)
{
	const FWRLandDef* D = Land ? Land->Def() : nullptr;
	if (!D || Land->GetNetMode() == NM_Client || !D->Section(TEXT("unmade")).IsValid()) return;
	for (TActorIterator<AWRUnmadePlace> It(Land->GetWorld()); It; ++It) return;
	const FTransform T(D->Origin);
	if (AWRUnmadePlace* P = Land->GetWorld()->SpawnActorDeferred<AWRUnmadePlace>(AWRUnmadePlace::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		P->LandId = Land->LandId;
		P->FinishSpawning(T);
	}
	// The lift on the north lip (X5).
	const TArray<TSharedPtr<FJsonValue>>* Pois = nullptr;
	if (D->Json->TryGetArrayField(TEXT("pois"), Pois))
	{
		for (const TSharedPtr<FJsonValue>& V : *Pois)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			if (O->GetStringField(TEXT("kind")) != TEXT("descent_lift")) continue;
			FVector2D C; float R = 0.f, Lip = 0.f;
			if (!UmRift(*D, C, R, Lip)) break;
			const FVector2D A = WRLand::Vec2(O->TryGetField(TEXT("at")));
			const FVector2D Face = WRLand::Vec2(O->TryGetField(TEXT("face")));
			const FTransform LT(FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(Face.Y - A.Y, Face.X - A.X)), 0.f), D->GroundPoint(A));
			Land->GetWorld()->SpawnActor<AWRDescentLift>(AWRDescentLift::StaticClass(), LT);
		}
	}
}

void AWRUnmadePlace::BeginPlay()
{
	Super::BeginPlay();
	if (LandId.IsNone()) for (const FWRLandDef& D : WRLand::All()) if (D.Section(TEXT("unmade")).IsValid()) LandId = D.Id;
	Build();
}

void AWRUnmadePlace::Build()
{
	const FWRLandDef* Dp = WRLand::Find(LandId);
	if (!Dp) return;
	const FWRLandDef& D = *Dp;
	FVector2D C; float R = 0.f, Lip = 0.f;
	if (!UmRift(D, C, R, Lip)) return;
	LipZ = D.Origin.Z + Lip * 100.f;
	Centre = D.ToWorld(C, Lip);
	RadiusCm = R * 100.f;
	const TSharedPtr<FJsonObject> UmSec = D.Section(TEXT("unmade"));
	const bool bDraw = GetNetMode() != NM_DedicatedServer;
	UMaterialInterface* Stone = WRFoldLook::RockMat(this, UmDolo * 0.4f);
	UMaterialInterface* Dark = WRFoldLook::RockMat(this, FLinearColor(0.05f, 0.048f, 0.046f));
	UMaterialInterface* Glass = WRFoldLook::RockMat(this, FLinearColor(0.004f, 0.0035f, 0.006f));
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));

	// The Choir's Descent: 0.35 m steps cut into the north wall, 120 m down, spiralling round the rift (instanced, solid).
	if (Cube)
	{
		UInstancedStaticMeshComponent* Steps = NewObject<UInstancedStaticMeshComponent>(this);
		Steps->SetStaticMesh(Cube);
		Steps->SetupAttachment(RootComponent);
		Steps->SetCollisionProfileName(TEXT("BlockAll"));
		Steps->SetMaterial(0, Stone);
		Steps->SetUsingAbsoluteLocation(true);
		Steps->SetUsingAbsoluteRotation(true);
		Steps->RegisterComponent();
		const float StairR = R - 6.f, Rise = 0.35f, Tread = 1.25f;
		const int32 N = FMath::CeilToInt(UmT(TEXT("stairDepth"), 120.f) / Rise);
		const float A0 = FMath::Atan2(-1.f, 0.f);   // the north lip (map -Y)
		for (int32 i = 0; i < N; ++i)
		{
			const float A = A0 + i * Tread / StairR;
			const FVector2D M = C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * StairR;
			const FVector P = UmAt(D, M, Lip, i * Rise + 0.15f);
			Steps->AddInstance(FTransform(FRotator(0.f, FMath::RadiansToDegrees(A) + 90.f, 0.f), P, FVector(Tread * 1.02f, 2.2f, 0.35f + i * 0.0f) * 1.f), true);
		}
		if (!bDraw) Steps->SetVisibility(false);
	}
	// The floating stones and the eight places.
	const TArray<TSharedPtr<FJsonValue>>* L = nullptr;
	if (!UmSec->TryGetArrayField(TEXT("pois"), L)) return;
	TMap<FString, FVector> Place;
	for (const TSharedPtr<FJsonValue>& V : *L)
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		Place.Add(O->GetStringField(TEXT("id")), UmAt(D, WRLand::Vec2(O->TryGetField(TEXT("at"))), Lip, float(O->GetNumberField(TEXT("depth")))));
	}
	// The stair's foot (where the rock ends) and the tour between the places.
	{
		const float StairR = R - 6.f;
		const int32 N = FMath::CeilToInt(UmT(TEXT("stairDepth"), 120.f) / 0.35f);
		const float A = FMath::Atan2(-1.f, 0.f) + N * 1.25f / StairR;
		Place.Add(TEXT("foot"), UmAt(D, C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * (StairR - 3.f), Lip, UmT(TEXT("stairDepth"), 120.f)));
	}
	TArray<TPair<FString, FString>> Legs = { { TEXT("U1"), TEXT("U4") }, { TEXT("U4"), TEXT("U2") }, { TEXT("U2"), TEXT("U3") }, { TEXT("U3"), TEXT("U5") }, { TEXT("U5"), TEXT("U7") },
		{ TEXT("U7"), TEXT("U6") }, { TEXT("U6"), TEXT("U8") }, { TEXT("foot"), TEXT("U6") } };
	int32 Seed = 0;
	for (const TPair<FString, FString>& Leg : Legs)
	{
		const FVector* A = Place.Find(Leg.Key);
		const FVector* B = Place.Find(Leg.Value);
		if (!A || !B) continue;
		const float Dist = FVector::Dist2D(*A, *B);
		const int32 N = FMath::Max(FMath::CeilToInt(Dist / 520.f), FMath::CeilToInt(FMath::Abs(B->Z - A->Z) / 90.f));
		for (int32 k = 1; k < N; ++k)
		{
			const float T = float(k) / N;
			const FVector P = FMath::Lerp(*A, *B, T) + FVector(FMath::Sin(k * 1.7f + Seed) * 120.f, FMath::Cos(k * 1.3f + Seed) * 120.f, 0.f);
			// A flat-topped stone 3-4 m across, its voidglass keel showing below (the floating rule).
			const float S = 300.f + 80.f * FMath::Abs(FMath::Sin(k * 2.1f + Seed));
			WRFoldLook::Rock(this, RootComponent, (k + Seed) % 5, FVector(S, S * 0.9f, 140.f), P - FVector(0, 0, 120.f) - GetActorLocation(), FRotator(0.f, k * 37.f, 0.f), Stone, true, 0.f);
			if (bDraw) WRFoldLook::Shape(this, RootComponent, TEXT("Cone"), FVector(70.f, 70.f, 160.f + 60.f * (k % 3)), P - FVector(0, 0, 250.f) - GetActorLocation(), FRotator(180.f, 0.f, 0.f), Glass);
		}
		++Seed;
	}
	TArray<FWRLookBlock> Blocks;
	FKit K(FTransform::Identity, Blocks);
	const FSurf DoloS = FSurf::Stone(UmDolo, 0.85f), Ash = FSurf::Stone(FLinearColor(FColor(150, 146, 140)), 0.95f), GlassS = FSurf::Glaze(FLinearColor(0.004f, 0.0035f, 0.006f), 0.08f);
	const FSurf Lit = FSurf::Lit(UmViolet, 2.5f);
	const auto Platform = [&](const FVector& P, float W, float L, float Yaw)
	{
		// A slab of Choir paving on its rock body with a voidglass keel.
		WRFoldLook::Shape(this, RootComponent, TEXT("Cube"), FVector(W, L, 80.f), P - FVector(0, 0, 40.f) - GetActorLocation(), FRotator(0.f, Yaw, 0.f), Stone)->SetCollisionProfileName(TEXT("BlockAll"));
		if (bDraw)
		{
			WRFoldLook::Shape(this, RootComponent, TEXT("Cone"), FVector(W * 0.9f, L * 0.9f, 600.f), P - FVector(0, 0, 380.f) - GetActorLocation(), FRotator(180.f, Yaw, 0.f), Dark);
			WRFoldLook::Shape(this, RootComponent, TEXT("Cone"), FVector(120.f, 120.f, 700.f), P - FVector(0, 0, 900.f) - GetActorLocation(), FRotator(180.f, 0.f, 0.f), Glass);
		}
	};
	const FVector LocalOf = GetActorLocation();
	if (const FVector* P = Place.Find(TEXT("U1")))
	{
		// U1, the Descent's foot: the landing ledge, the cage's rest, a rope anchor, a brazier.
		Platform(*P, 1200.f, 800.f, 0.f);
		K.Add(DoloS, FVector(80.f, 80.f, 120.f), *P - LocalOf + FVector(300.f, 200.f, 60.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		K.Add(Ash, FVector(120.f, 120.f, 20.f), *P - LocalOf + FVector(300.f, 200.f, 128.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		K.Add(DoloS, FVector(40.f, 40.f, 160.f), *P - LocalOf + FVector(-400.f, -200.f, 80.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		if (bDraw)
		{
			if (USceneComponent* F = WRFx::Attach(RootComponent, TEXT("brazier.fire"), NAME_None, 1.4f)) { F->SetUsingAbsoluteLocation(true); F->SetWorldLocation(*P + FVector(300.f, 200.f, 150.f)); }
			WRFoldLook::Light(this, RootComponent, *P - LocalOf + FVector(300.f, 200.f, 300.f), FLinearColor(1.f, 0.6f, 0.3f), 3000.f, 1500.f);
		}
	}
	if (const FVector* P = Place.Find(TEXT("U2")))
	{
		// U2, the Seated Choir: 200 dead on floating terraces in a great curve facing one point of black, mouths open.
		for (int32 tier = 0; tier < 4; ++tier)
		{
			const float RR = 1400.f + 300.f * tier;
			Platform(*P + FVector(0, 0, tier * 120.f) + FVector(-RR, 0, 0) * 0.f, 2.f * RR * 0.6f, 280.f, 0.f);
			for (int32 s = 0; s < 50; ++s)
			{
				const float A = FMath::DegreesToRadians(-50.f + 100.f * s / 49.f);
				const FVector Seat = *P - LocalOf + FVector(FMath::Cos(A) * RR * 0.5f - RR * 0.25f, FMath::Sin(A) * RR * 0.55f, tier * 120.f + 10.f);
				const float Yaw = FMath::RadiansToDegrees(A) + 180.f;
				K.Add(Ash, FVector(40.f, 34.f, 60.f), Seat + FVector(0, 0, 30.f), FRotator(0.f, Yaw, 0.f));
				K.Add(Ash, FVector(22.f, 22.f, 26.f), Seat + FVector(0, 0, 76.f), FRotator(-15.f, Yaw, 0.f), EWRLookMesh::Cylinder);
				K.Add(FSurf::Stone(FLinearColor(0.02f, 0.02f, 0.02f), 0.9f), FVector(6.f, 8.f, 6.f), Seat + FVector(0, 0, 72.f) + FRotator(0.f, Yaw, 0.f).Vector() * 11.f);
			}
		}
		if (bDraw) WRFoldLook::Light(this, RootComponent, *P - LocalOf + FVector(0, 0, 900.f), UmViolet, 2500.f, 3500.f);
	}
	if (const FVector* P = Place.Find(TEXT("U3")))
	{
		// U3, the pool of the Fish That Isn't: a floating basin of black water reflecting stars that aren't there.
		Platform(*P, 1100.f, 1100.f, 0.f);
		K.Add(DoloS, FVector(900.f, 900.f, 60.f), *P - LocalOf + FVector(0, 0, 20.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		K.Add(GlassS, FVector(820.f, 820.f, 8.f), *P - LocalOf + FVector(0, 0, 52.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		for (int32 s = 0; s < 30; ++s) K.Add(FSurf::Lit(FLinearColor(0.9f, 0.88f, 1.f), 6.f), FVector(4.f, 4.f, 1.f), *P - LocalOf + FVector(FMath::Sin(s * 2.3f) * 350.f, FMath::Cos(s * 1.9f) * 350.f, 57.f));
	}
	if (const FVector* P = Place.Find(TEXT("U4")))
	{
		// U4, the Inverted Tower: a horn-tower hanging point-down from its floating foundation, a stair down to its mouth.
		Platform(*P, 900.f, 900.f, 0.f);
		for (int32 r = 0; r < 12; ++r)
		{
			const float Rr = 380.f - r * 26.f;
			K.Add(DoloS, FVector(Rr * 2.f, Rr * 2.f, 200.f), *P - LocalOf - FVector(0, 0, 200.f + r * 200.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		}
		K.Add(Lit, FVector(60.f, 60.f, 10.f), *P - LocalOf - FVector(0, 0, 2600.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
	}
	if (const FVector* P = Place.Find(TEXT("U5")))
	{
		// U5, the rubble garden: floating stones on voidglass keels 1-2 m long, the richest voidglass (x2).
		Platform(*P, 1400.f, 1000.f, 15.f);
		if (HasAuthority())
		{
			for (int32 n = 0; n < 10; ++n)
			{
				const FVector NP = *P + FVector(FMath::Cos(n * 0.63f) * 450.f, FMath::Sin(n * 0.63f) * 350.f, 0.f);
				if (AWRResourceNode* Node = GetWorld()->SpawnActorDeferred<AWRResourceNode>(AWRResourceNode::StaticClass(), FTransform(NP), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
				{
					Node->ItemId = TEXT("voidglass");
					Node->Hits = 4;
					Node->PerHit = int32(UmT(TEXT("gardenPerHit"), 6.f));
					Node->RespawnSeconds = UmT(TEXT("gardenRespawn"), 1800.f);
					Node->GemChance = 0.f;
					Node->FinishSpawning(FTransform(NP));
				}
			}
		}
	}
	if (const FVector* P = Place.Find(TEXT("U6")))
	{
		// U6, the Riftmaw drift: a nest slab with three sleeping Riftmaws (the hardest Riftmaw fight).
		Platform(*P, 1800.f, 1400.f, 30.f);
		for (int32 b = 0; b < 14; ++b) K.Add(FSurf::Stone(FLinearColor(FColor(217, 207, 184)), 0.8f), FVector(80.f, 20.f, 14.f), *P - LocalOf + FVector(FMath::Sin(b * 2.f) * 500.f, FMath::Cos(b * 1.5f) * 400.f, 10.f), FRotator(0.f, b * 41.f, 0.f));
		if (HasAuthority())
		{
			const FTransform ST(*P + FVector(0, 0, 150.f));
			if (AWRCreatureSpawner* Sp = GetWorld()->SpawnActorDeferred<AWRCreatureSpawner>(AWRCreatureSpawner::StaticClass(), ST, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
			{
				Sp->CreatureId = TEXT("riftmaw");
				Sp->Count = 3;
				Sp->Radius = 500.f;
				Sp->RespawnSeconds = 3600.f;
				Sp->ActivationRadius = 9000.f;
				Sp->bTraceGround = true;
				Sp->FinishSpawning(ST);
			}
		}
	}
	if (const FVector* P = Place.Find(TEXT("U7")))
	{
		// U7, the Cantor's reliquary: a small sealed chapel on its own stone, a stone box inside.
		Platform(*P, 1000.f, 1000.f, 0.f);
		for (const float Side : { -1.f, 1.f })
		{
			WRFoldLook::Shape(this, RootComponent, TEXT("Cube"), FVector(600.f, 40.f, 400.f), *P - LocalOf + FVector(0, Side * 300.f, 200.f), FRotator::ZeroRotator, Stone)->SetCollisionProfileName(TEXT("BlockAll"));
		}
		WRFoldLook::Shape(this, RootComponent, TEXT("Cube"), FVector(40.f, 640.f, 400.f), *P - LocalOf + FVector(300.f, 0, 200.f), FRotator::ZeroRotator, Stone)->SetCollisionProfileName(TEXT("BlockAll"));
		WRFoldLook::Shape(this, RootComponent, TEXT("Cube"), FVector(680.f, 680.f, 40.f), *P - LocalOf + FVector(0, 0, 420.f), FRotator::ZeroRotator, Stone);
		K.Add(DoloS, FVector(160.f, 80.f, 80.f), *P - LocalOf + FVector(150.f, 0, 40.f));
		if (HasAuthority())
		{
			TArray<FWRItemStack> Cache;
			Cache.Add(FWRInventoryRules::MakeInstance(TEXT("void_essence")));
			Cache[0].Count = int32(UmT(TEXT("reliquaryEssence"), 6.f));
			if (AWRLootBag* Bag = AWRLootBag::Spill(GetWorld(), *P + FVector(150.f, 0, 100.f), FString(), Cache)) Bag->bOwnerOnly = false;
		}
	}
	if (const FVector* P = Place.Find(TEXT("U8")))
	{
		// U8, the last note: the farthest stone, and one woman standing alone at its edge holding a carved stone up toward
		// the black, as if offering it.
		U8 = *P;
		Platform(*P, 700.f, 600.f, 0.f);
		const FVector F = *P - LocalOf + FVector(250.f, 0, 0);
		K.Add(Ash, FVector(30.f, 40.f, 90.f), F + FVector(0, 0, 45.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		K.Add(Ash, FVector(34.f, 46.f, 60.f), F + FVector(0, 0, 120.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		K.Add(Ash, FVector(20.f, 20.f, 24.f), F + FVector(0, 0, 162.f), FRotator(-20.f, 0.f, 0.f), EWRLookMesh::Cylinder);
		K.Rod(Ash, F + FVector(0, 15.f, 140.f), F + FVector(30.f, 10.f, 200.f), 7.f);
		K.Rod(Ash, F + FVector(0, -15.f, 140.f), F + FVector(30.f, -10.f, 200.f), 7.f);
		K.Add(FSurf::Stone(FLinearColor(0.15f, 0.14f, 0.15f), 0.6f), FVector(14.f, 12.f, 10.f), F + FVector(32.f, 0, 206.f));
		if (bDraw) WRFoldLook::Light(this, RootComponent, F + FVector(0, 0, 400.f), FLinearColor(0.85f, 0.82f, 1.f), 1500.f, 900.f);
	}
	if (bDraw && Blocks.Num()) FWRBuildLook::Spawn(this, RootComponent, Blocks, FLinearColor(FColor(150, 146, 140)), false);
}

void AWRUnmadePlace::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || LipZ == 0.f) return;
	// Inside the pocket the drain is x1.5 (the passes apply); reaching the last note is Maren's fifth quest.
	const float Top = LipZ - UmT(TEXT("top"), 5000.f);
	for (TActorIterator<AWRCharacter> It(GetWorld()); It; ++It)
	{
		// Only inside the Great Rift itself: InHole alone is true in every land's caves (the Crystal and Fungal caverns
		// sit below the lip too), which drained anyone exploring them to death (landmarks audit 11-18).
		if (!It->IsAlive() || !WRLand::InHole(It->GetActorLocation()) || It->GetActorLocation().Z > Top) continue;
		if (RadiusCm <= 0.f || FVector::Dist2D(It->GetActorLocation(), Centre) > RadiusCm + UmT(TEXT("drainMargin"), 2000.f)) continue;
		const float Mult = WRVoid::RiftDrainMult(*It);
		if (Mult > 0.f) WRVoid::LoseMaxHealth(*It, WRTune(TEXT("void.riftDrain"), 2.f) * UmT(TEXT("drainMult"), 1.5f) * Mult * DeltaSeconds, this, true);
		if (!U8.IsZero() && FVector::Dist(It->GetActorLocation(), U8) < UmT(TEXT("lastNoteReach"), 800.f) && It->GetPlayerState())
		{
			const FString Who = It->GetPlayerState()->GetPlayerName();
			if (!ReachedLastNote.Contains(Who))
			{
				ReachedLastNote.Add(Who);
				WRVoidWorld::QuestEvent(*It, TEXT("last_note"));
				if (AWRPlayerController* PC = Cast<AWRPlayerController>(It->GetController())) PC->ClientNotify(TEXT("The last note: she holds it out to the black, still."));
			}
		}
	}
}

// ================================================================================================ the Descent lift

AWRDescentLift::AWRDescentLift()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRDescentLift::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRDescentLift, bRepaired);
	DOREPLIFETIME(AWRDescentLift, CageDepth);
	DOREPLIFETIME(AWRDescentLift, Moving);
}

AWRDescentLift* AWRDescentLift::Find(const UWorld* World)
{
	if (!World) return nullptr;
	for (TActorIterator<AWRDescentLift> It(World); It; ++It) return *It;
	return nullptr;
}

void AWRDescentLift::BeginPlay()
{
	Super::BeginPlay();
	Drop = UmT(TEXT("liftDrop"), 100.f);
	Build();
}

void AWRDescentLift::Build()
{
	// Local frame: X out over the rift (the jib reaches that way). On the lip: two stone bearing blocks carrying an iron
	// axle; on it the timber drum 2 m across with a ratchet wheel at one end and its pawl on a pin; a crank handle at each
	// end; a cantilevered timber jib over the edge with a sheave at its tip; the rope from the drum over the sheave down to
	// a basket cage of iron straps. Snapped, a frayed end hangs from the sheave and the cage rests at the bottom.
	UMaterialInterface* Stone = WRFoldLook::RockMat(this, UmDolo * 0.4f);
	UMaterialInterface* Wood = WRFoldLook::RockMat(this, FLinearColor(0.12f, 0.1f, 0.08f));
	UMaterialInterface* Iron = WRFoldLook::RockMat(this, FLinearColor(0.035f, 0.033f, 0.035f));
	UMaterialInterface* Hemp = WRFoldLook::RockMat(this, FLinearColor(0.35f, 0.3f, 0.2f));
	const auto S = [&](USceneComponent* Parent, const TCHAR* Shape, const FVector& Size, const FVector& At, const FRotator& Rot, UMaterialInterface* M, bool bCollide)
	{
		UStaticMeshComponent* C = WRFoldLook::Shape(this, Parent, Shape, Size, At, Rot, M);
		if (C && bCollide) { C->SetCollisionProfileName(TEXT("BlockAll")); C->SetMobility(EComponentMobility::Movable); }
		return C;
	};
	for (const float Side : { -1.f, 1.f }) S(RootComponent, TEXT("Cube"), FVector(80.f, 70.f, 160.f), FVector(-200.f, Side * 160.f, 80.f), FRotator::ZeroRotator, Stone, true);
	S(RootComponent, TEXT("Cylinder"), FVector(14.f, 14.f, 380.f), FVector(-200.f, 0, 150.f), FRotator(0.f, 0.f, 90.f), Iron, false);
	Drum = NewObject<USceneComponent>(this);
	Drum->SetupAttachment(RootComponent);
	Drum->SetRelativeLocation(FVector(-200.f, 0, 150.f));
	Drum->RegisterComponent();
	S(Drum, TEXT("Cylinder"), FVector(200.f, 200.f, 220.f), FVector::ZeroVector, FRotator(0.f, 0.f, 90.f), Wood, false);
	for (int32 r = 0; r < 12; ++r) S(Drum, TEXT("Cube"), FVector(16.f, 230.f, 10.f), FRotator(r * 30.f, 0.f, 0.f).RotateVector(FVector(0, 0, 100.f)), FRotator(r * 30.f, 0.f, 0.f), Wood, false);
	S(Drum, TEXT("Cylinder"), FVector(240.f, 240.f, 12.f), FVector(0, 125.f, 0), FRotator(0.f, 0.f, 90.f), Iron, false);   // the ratchet wheel
	for (int32 t = 0; t < 16; ++t) S(Drum, TEXT("Cone"), FVector(16.f, 12.f, 20.f), FVector(0, 125.f, 0) + FRotator(t * 22.5f, 0.f, 0.f).RotateVector(FVector(0, 0, 125.f)), FRotator(t * 22.5f, 0.f, 0.f), Iron, false);
	for (const float Side : { -1.f, 1.f })
	{
		S(Drum, TEXT("Cube"), FVector(12.f, 12.f, 70.f), FVector(0, Side * 190.f, 35.f), FRotator::ZeroRotator, Iron, false);
		S(Drum, TEXT("Cylinder"), FVector(10.f, 10.f, 40.f), FVector(0, Side * 210.f, 70.f), FRotator(0.f, 0.f, 90.f), Wood, false);
	}
	Pawl = NewObject<USceneComponent>(this);
	Pawl->SetupAttachment(RootComponent);
	Pawl->SetRelativeLocation(FVector(-200.f, 125.f, 290.f));
	Pawl->RegisterComponent();
	S(Pawl, TEXT("Cube"), FVector(50.f, 10.f, 10.f), FVector(20.f, 0, 0), FRotator(-25.f, 0.f, 0.f), Iron, false);
	// The jib: two timbers raking out over the edge, braced, a sheave at the tip.
	S(RootComponent, TEXT("Cube"), FVector(560.f, 24.f, 24.f), FVector(80.f, -40.f, 330.f), FRotator(12.f, 0.f, 0.f), Wood, false);
	S(RootComponent, TEXT("Cube"), FVector(560.f, 24.f, 24.f), FVector(80.f, 40.f, 330.f), FRotator(12.f, 0.f, 0.f), Wood, false);
	S(RootComponent, TEXT("Cube"), FVector(24.f, 24.f, 330.f), FVector(-150.f, 0, 165.f), FRotator(0.f, 0.f, 0.f), Wood, false);
	S(RootComponent, TEXT("Cylinder"), FVector(60.f, 60.f, 20.f), FVector(340.f, 0, 390.f), FRotator(0.f, 0.f, 90.f), Iron, false);
	Rope = NewObject<USceneComponent>(this);
	Rope->SetupAttachment(RootComponent);
	Rope->SetRelativeLocation(FVector(340.f, 0, 360.f));
	Rope->RegisterComponent();
	S(Rope, TEXT("Cylinder"), FVector(5.f, 5.f, 100.f), FVector(0, 0, -50.f), FRotator::ZeroRotator, Hemp, false);
	SnappedRope = NewObject<USceneComponent>(this);
	SnappedRope->SetupAttachment(RootComponent);
	SnappedRope->SetRelativeLocation(FVector(340.f, 0, 360.f));
	SnappedRope->RegisterComponent();
	S(SnappedRope, TEXT("Cylinder"), FVector(5.f, 5.f, 300.f), FVector(0, 0, -150.f), FRotator::ZeroRotator, Hemp, false);
	for (int32 f = 0; f < 4; ++f) S(SnappedRope, TEXT("Cone"), FVector(4.f, 4.f, 30.f), FVector(FMath::Sin(float(f)) * 4.f, FMath::Cos(float(f)) * 4.f, -310.f), FRotator(180.f + 15.f * f, 90.f * f, 0.f), Hemp, false);
	// The basket cage: a floor and iron straps, open on the jib side (4 can ride).
	Cage = NewObject<USceneComponent>(this);
	Cage->SetupAttachment(RootComponent);
	Cage->SetMobility(EComponentMobility::Movable);
	Cage->SetRelativeLocation(FVector(340.f, 0, 0.f));
	Cage->RegisterComponent();
	S(Cage, TEXT("Cube"), FVector(320.f, 320.f, 20.f), FVector(0, 0, -10.f), FRotator::ZeroRotator, Wood, true);
	for (int32 p = 0; p < 4; ++p)
	{
		const FVector Corner((p & 1 ? 1.f : -1.f) * 150.f, (p & 2 ? 1.f : -1.f) * 150.f, 110.f);
		S(Cage, TEXT("Cube"), FVector(10.f, 10.f, 220.f), Corner, FRotator::ZeroRotator, Iron, false);
	}
	for (const float Side : { -1.f, 1.f }) S(Cage, TEXT("Cube"), FVector(320.f, 8.f, 120.f), FVector(0, Side * 155.f, 60.f), FRotator::ZeroRotator, Iron, true);
	S(Cage, TEXT("Cube"), FVector(8.f, 320.f, 120.f), FVector(155.f, 0, 60.f), FRotator::ZeroRotator, Iron, true);
	S(Cage, TEXT("Cube"), FVector(320.f, 320.f, 8.f), FVector(0, 0, 220.f), FRotator::ZeroRotator, Iron, false);
	ShownDepth = CageDepth;
}

FVector AWRDescentLift::CageTop() const
{
	return GetActorTransform().TransformPosition(FVector(340.f, 0, -ShownDepth * 100.f));
}

bool AWRDescentLift::NearCrank(const FVector& At) const
{
	return FVector::Dist(GetActorTransform().TransformPosition(FVector(-200.f, 0, 100.f)), At) < 350.f;
}

bool AWRDescentLift::InCage(const FVector& At) const
{
	const FVector L = GetActorTransform().InverseTransformPosition(At);
	return FMath::Abs(L.X - 340.f) < 190.f && FMath::Abs(L.Y) < 190.f && FMath::Abs(L.Z + CageDepth * 100.f - 90.f) < 250.f;
}

bool AWRDescentLift::PromptFor(const FVector& At, FString& OutVerb) const
{
	if (!NearCrank(At) && !InCage(At)) return false;
	if (!bRepaired)
	{
		if (!NearCrank(At)) return false;
		OutVerb = TEXT("Hold E: mend the rope (rope 10, iron 4; 5 s)");
		return true;
	}
	OutVerb = Moving != 0 ? TEXT("E: let go of the crank (the pawl holds it)") : CageDepth < 1.f ? TEXT("E: crank the cage down") : TEXT("E: crank the cage up");
	return true;
}

FString AWRDescentLift::Use(AWRPlayerController* By)
{
	APawn* Me = By ? By->GetPawn() : nullptr;
	if (!Me || !HasAuthority()) return FString();
	if (!bRepaired)
	{
		if (!NearCrank(Me->GetActorLocation())) return TEXT("Too far from the winch.");
		UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Me);
		if (!Inv || Inv->CountOf(TEXT("rope")) < 10 || Inv->CountOf(TEXT("iron_ingot")) < 4) return TEXT("Mending it needs rope 10 and iron 4.");
		Repairer = Me;
		RepairStarted = GetWorld()->GetTimeSeconds();
		return TEXT("You splice the rope and set the iron. Stay with it.");
	}
	if (Moving != 0) { Moving = 0; Cranker = nullptr; return TEXT("The pawl drops: it holds."); }
	Cranker = Me;
	Moving = CageDepth < Drop * 0.5f ? 1 : -1;
	return Moving > 0 ? TEXT("You lean on the crank: the cage goes down.") : TEXT("You haul on the crank: the cage comes up.");
}

void AWRDescentLift::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority())
	{
		// The mending: 5 s beside the winch with the rope and iron in your bag.
		if (APawn* R = Repairer.Get())
		{
			if (!NearCrank(R->GetActorLocation()) || R->GetVelocity().Size2D() > 150.f) Repairer = nullptr;
			else if (GetWorld()->GetTimeSeconds() - RepairStarted >= UmT(TEXT("repairSeconds"), 5.f))
			{
				if (UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(R); Inv && Inv->CountOf(TEXT("rope")) >= 10 && Inv->CountOf(TEXT("iron_ingot")) >= 4)
				{
					Inv->RemoveItem(TEXT("rope"), 10);
					Inv->RemoveItem(TEXT("iron_ingot"), 4);
					bRepaired = true;
					CageDepth = Drop;   // the cage was lying at the bottom
					WRSound::PlayAt(this, TEXT("void.lift.mended"), GetActorLocation());
				}
				Repairer = nullptr;
			}
		}
		// The crank: the cage moves 1.2 m/s while someone keeps at it (beside the drum or in the cage); let go and the pawl holds.
		if (Moving != 0)
		{
			const APawn* C = Cranker.Get();
			if (!C || (!NearCrank(C->GetActorLocation()) && !InCage(C->GetActorLocation()))) { Moving = 0; Cranker = nullptr; }
			else
			{
				CageDepth = FMath::Clamp(CageDepth + Moving * UmT(TEXT("liftSpeed"), 1.2f) * DeltaSeconds, 0.f, Drop);
				if (CageDepth <= 0.f || CageDepth >= Drop) { Moving = 0; Cranker = nullptr; }
			}
		}
	}
	// Every machine: the cage eases to where the server has it; the drum turns with the rope it winds (2 m across: the
	// rope's length over its circumference), the pawl clicks every 20 cm, the rope runs from the sheave to the cage.
	ShownDepth = HasAuthority() ? CageDepth : FMath::FInterpTo(ShownDepth, CageDepth, DeltaSeconds, 6.f);
	if (Cage) Cage->SetRelativeLocation(FVector(340.f, 0, -ShownDepth * 100.f));
	if (Drum) Drum->SetRelativeRotation(FRotator(0.f, 0.f, -ShownDepth * 100.f / (PI * 200.f) * 360.f));
	if (Rope) { Rope->SetVisibility(bRepaired, true); Rope->SetRelativeScale3D(FVector(1.f, 1.f, FMath::Max(0.01f, (ShownDepth * 100.f + 360.f) / 100.f))); }
	if (SnappedRope) SnappedRope->SetVisibility(!bRepaired, true);
	if (GetNetMode() != NM_DedicatedServer && FMath::Abs(ShownDepth - LastClick) >= 0.2f)
	{
		LastClick = ShownDepth;
		WRSound::PlayAt(this, TEXT("void.lift.pawl"), GetActorTransform().TransformPosition(FVector(-200.f, 125.f, 290.f)));
		if (Pawl) Pawl->SetRelativeRotation(FRotator(FMath::FRandRange(-6.f, 0.f), 0.f, 0.f));
	}
}
