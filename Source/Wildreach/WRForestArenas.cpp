// The Ancient forest's boss arenas (cooks/biome-forest.md 2.3 L1/L5/L6/L7, 2.13, 2.14; cooks/build-biome-forest.md 2.10,
// 2.15): the arena pieces with state (AWRForestProp: brood combs, the waking heart-trees, the root hand, the larder's
// curtain; AWRForestRootLine: Sylvaroth's four root lines and their knots; AWRForestWalkway: the Bower's walkways and
// their rope anchors), the thrown and falling things (AWRForestLob), the four arena landmarks' static scenery
// (AWRForestArenaScenery) and UWRForestArenas, which builds the scenery on every machine and, on the server, places the
// bosses, their pieces, the Nature altar and keeps the Wild Hunt's flag. See WRForestBosses.h.
//
// Layout comes from Data/forest.json "arenas" (FWRForestData, Tools/terrain/forest.mjs): sylvaroth's points are the
// four heart-trees N, E, S, W then the four standing stones; grey_matron's the six oaks (walkways: forest.json
// "walkways" with arena grey_matron); bramblequeen's the four combs (floor 1, 2, 3, crown); old_gnarl's the boss's
// start. Where a row leaves something out the cook's numbers fill it (the same on every machine, so the scenery, the
// server's pieces and the bosses agree).
//
// Palette (DC 2.3, 2.13-2.14): heart bark #4A3A2A, amber sap #D08A1E, foxfire #7FE07A (never the UI gold), old stone
// #8A8478, moss #5E8C31, hive paper #8E8A84 / #A29E96 / #6E6A64, grub glow #E0A040, plank #8C7050, rope #9C8460,
// hide #78563A, root grey-brown #5A4A3A, crimson rim #9A1A14.

#include "WRForestBosses.h"
#include "WRForestTypes.h"
#include "WRForestWorld.h"
#include "WRBoss.h"
#include "WRAltar.h"
#include "WRBuildLook.h"
#include "WRBuildLookKit.h"
#include "WRCharacter.h"
#include "WRCombatLibrary.h"
#include "WRForestHazards.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRGameState.h"
#include "WRProjectile.h"
#include "WRSound.h"
#include "WRSurvival.h"
#include "WRToolMesh.h"
#include "WRTreeKit.h"
#include "WRTuning.h"
#include "Wildreach.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"
#include "TimerManager.h"
#include "UObject/Package.h"

namespace WRForestArenaImpl
{
	using FaSurf = WRLookKit::FSurf;
	using FaKit = WRLookKit::FKit;

	FLinearColor FaRgb(uint8 R, uint8 G, uint8 B) { return FLinearColor(FColor(R, G, B)); }

	// Surfaces (linear colour, dimmed the way the build kit's tinted blocks want it).
	FaSurf FaStone(float K = 1.f) { return FaSurf::Stone(FaRgb(138, 132, 120) * 0.55f * K, 0.92f); }
	FaSurf FaMoss(float K = 1.f) { return FaSurf::Cloth(FaRgb(94, 140, 49) * 0.42f * K); }
	FaSurf FaPlank(float K = 1.f) { return FaSurf::Wood(FaRgb(140, 112, 80) * 0.42f * K); }
	FaSurf FaTimber(float K = 1.f) { return FaSurf::Wood(FaRgb(90, 70, 54) * 0.45f * K); }
	FaSurf FaBark(float K = 1.f) { return FaSurf::Wood(FaRgb(74, 58, 42) * 0.55f * K); }
	FaSurf FaPaleWood(float K = 1.f) { return FaSurf::Wood(FaRgb(176, 140, 96) * 0.5f * K); }
	FaSurf FaRope() { return FaSurf::Cloth(FaRgb(156, 132, 96) * 0.5f); }
	FaSurf FaIron() { return FaSurf::Steel(FLinearColor(0.035f, 0.033f, 0.032f), 0.65f); }
	FaSurf FaHide(float K = 1.f) { return FaSurf::Cloth(FaRgb(120, 86, 56) * 0.45f * K); }
	FaSurf FaPaper(int32 Tone) { const FLinearColor C[3] = { FaRgb(142, 138, 132), FaRgb(162, 158, 150), FaRgb(110, 106, 100) }; return FaSurf::Cloth(C[((Tone % 3) + 3) % 3] * 0.5f); }
	FaSurf FaAmber(float Glow) { return FaSurf::Lit(FaRgb(208, 138, 30) * 0.6f, Glow); }
	FaSurf FaGrub(float Glow) { return FaSurf::Lit(FaRgb(224, 160, 64) * 0.6f, Glow); }
	FaSurf FaFoxfire(float Glow) { return FaSurf::Lit(FaRgb(127, 224, 122) * 0.55f, Glow); }
	FaSurf FaCrimson(float Glow) { return FaSurf::Lit(FaRgb(154, 26, 20) * 0.6f, Glow); }
	FaSurf FaEyeshine(float Glow) { return FaSurf::Lit(FaRgb(150, 230, 120) * 0.6f, Glow); }
	FaSurf FaWater() { return FaSurf::Glaze(FaRgb(60, 92, 104) * 0.45f, 0.05f); }
	FaSurf FaWax() { return FaSurf::Glaze(FaRgb(200, 150, 60) * 0.5f, 0.35f); }

	FLinearColor FaRootC() { return FaRgb(90, 74, 58) * 0.5f; }
	FLinearColor FaMossC() { return FaRgb(94, 140, 49) * 0.42f; }
	FLinearColor FaPaperC() { return FaRgb(142, 138, 132) * 0.5f; }
	FLinearColor FaRopeC() { return FaRgb(156, 132, 96) * 0.5f; }
	FLinearColor FaPodC() { return FaRgb(150, 96, 40) * 0.5f; }
	FLinearColor FaDeadC() { return FaRgb(52, 44, 36) * 0.55f; }

	float FaServerNow(const UObject* W)
	{
		const UWorld* World = W ? W->GetWorld() : nullptr;
		const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
		return GS ? float(GS->GetServerWorldTimeSeconds()) : (World ? World->GetTimeSeconds() : 0.f);
	}

	double FaHours(const UObject* W)
	{
		const AWRGameState* GS = AWRGameState::Get(W);
		return GS ? GS->GetTotalHours() : 0.0;
	}

	/** The ground under a point (static world only), ignoring Ignore. */
	bool FaGround(const UWorld* World, const FVector& At, FVector& Out, float Up = 4000.f, float Down = 9000.f, const AActor* Ignore = nullptr)
	{
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRForestArenaGround), false, Ignore);
		if (World && World->LineTraceSingleByChannel(Hit, At + FVector(0.f, 0.f, Up), At - FVector(0.f, 0.f, Down), ECC_WorldStatic, Q) && !Cast<APawn>(Hit.GetActor()))
		{
			Out = Hit.ImpactPoint;
			return true;
		}
		Out = At;
		return false;
	}

	/** A hit shape a swing finds (it overlaps pawns: the melee sweep runs on the Pawn channel) and an arrow stops on (it
	 *  blocks world-dynamic things: projectiles), that never blocks a body walking. */
	void FaHitShape(UPrimitiveComponent* C)
	{
		C->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		C->SetCollisionObjectType(ECC_WorldDynamic);
		C->SetCollisionResponseToAllChannels(ECR_Ignore);
		C->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
		C->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		C->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		C->SetGenerateOverlapEvents(false);
		C->SetCanEverAffectNavigation(false);
	}

	/** Builds blocks under a new scene component on Owner's root (look only, no collision). */
	USceneComponent* FaSpawnBlocks(AActor* Owner, USceneComponent* Under, const TArray<FWRLookBlock>& Blocks, TArray<TObjectPtr<USceneComponent>>& Track)
	{
		if (!Owner || Blocks.Num() == 0 || Owner->GetNetMode() == NM_DedicatedServer) return nullptr;
		USceneComponent* Frame = NewObject<USceneComponent>(Owner);
		Frame->SetupAttachment(Under ? Under : Owner->GetRootComponent());
		Frame->RegisterComponent();
		Track.Add(Frame);
		FWRBuildLook::Spawn(Owner, Frame, Blocks, FLinearColor::White, false);
		TArray<USceneComponent*> Children;
		Frame->GetChildrenComponents(false, Children);
		for (USceneComponent* Ch : Children) Track.Add(Ch);
		return Frame;
	}

	/** A code mesh (roots, ropes, the paper shell's ribs) under Under. */
	UProceduralMeshComponent* FaSpawnMesh(AActor* Owner, USceneComponent* Under, const FWRToolMesh& Mesh, TArray<TObjectPtr<USceneComponent>>& Track)
	{
		if (!Owner || Mesh.Verts.Num() == 0 || Owner->GetNetMode() == NM_DedicatedServer) return nullptr;
		UProceduralMeshComponent* P = NewObject<UProceduralMeshComponent>(Owner);
		P->SetupAttachment(Under ? Under : Owner->GetRootComponent());
		P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		P->SetCanEverAffectNavigation(false);
		P->RegisterComponent();
		Mesh.ApplyTo(P);
		Track.Add(P);
		return P;
	}

	UPointLightComponent* FaLight(AActor* Owner, USceneComponent* Under, const FVector& Rel, const FLinearColor& Colour, float Intensity, float Radius)
	{
		if (!Owner || Owner->GetNetMode() == NM_DedicatedServer) return nullptr;
		UPointLightComponent* L = NewObject<UPointLightComponent>(Owner);
		L->SetupAttachment(Under ? Under : Owner->GetRootComponent());
		L->SetRelativeLocation(Rel);
		L->SetLightColor(Colour);
		L->SetIntensity(Intensity);
		L->SetAttenuationRadius(Radius);
		L->SetCastShadows(false);
		L->RegisterComponent();
		return L;
	}

	/** Sets the build kit's glow (custom data 8) on every instance under Frame. */
	void FaSetGlow(USceneComponent* Frame, float Glow)
	{
		if (!Frame) return;
		TArray<USceneComponent*> Children;
		Frame->GetChildrenComponents(true, Children);
		for (USceneComponent* Ch : Children)
		{
			if (UInstancedStaticMeshComponent* ISM = Cast<UInstancedStaticMeshComponent>(Ch); ISM && ISM->NumCustomDataFloats >= 10)
			{
				for (int32 i = 0; i < ISM->GetInstanceCount(); ++i) ISM->SetCustomDataValue(i, 8, Glow, i == ISM->GetInstanceCount() - 1);
			}
		}
	}

	/** The rope / deck sag between two points (a parabola is close to a catenary at a walkway's sag). */
	FVector FaSag(const FVector& A, const FVector& B, float T, float Depth)
	{
		return FMath::Lerp(A, B, T) - FVector(0.f, 0.f, 4.f * Depth * T * (1.f - T));
	}

	/** The species' trunk radius grown to Height. */
	float FaTrunkR(FName Species, float Height)
	{
		const float Ref = WRTreeKit::ReferenceHeight(Species);
		return WRTreeKit::ReferenceRadius(Species) * (Ref > 1.f ? Height / Ref : 1.f);
	}

	/** The landmark site kind that holds an arena when forest.json has no arena row. */
	FName FaLandmarkOf(FName Arena)
	{
		if (Arena == TEXT("sylvaroth")) return TEXT("four_hearts");
		if (Arena == TEXT("old_gnarl")) return TEXT("clear_cut");
		if (Arena == TEXT("bramblequeen")) return TEXT("paper_hollow");
		if (Arena == TEXT("grey_matron")) return TEXT("bower");
		return NAME_None;
	}

	/** A deterministic 0..1 from an integer pair (the same on every machine). */
	float FaHash(int32 A, int32 B)
	{
		uint32 H = uint32(A) * 73856093u ^ uint32(B) * 19349663u;
		H ^= H >> 13; H *= 0x5bd1e995u; H ^= H >> 15;
		return float(H & 0xFFFFFF) / float(0xFFFFFF);
	}

	/** A small hide stretched on a frame of sticks (the Bower's flags, the larder curtain's skins). */
	void FaHide(FaKit& K, const FVector& Top, float Yaw, float W, float H, float Tone)
	{
		const FRotator R(0.f, Yaw, 0.f);
		K.Rod(FaTimber(0.8f), Top - R.RotateVector(FVector(0.f, W * 0.55f, 0.f)), Top + R.RotateVector(FVector(0.f, W * 0.55f, 0.f)), 5.f);
		K.Add(FaHide(0.85f + 0.3f * Tone), FVector(3.f, W, H), Top - FVector(0.f, 0.f, H * 0.5f + 4.f), R);
		// The legs of the skin hang below the stretched part (it was an animal).
		K.Add(FaHide(0.75f + 0.3f * Tone), FVector(3.f, W * 0.18f, H * 0.35f), Top + R.RotateVector(FVector(0.f, -W * 0.38f, 0.f)) - FVector(0.f, 0.f, H + H * 0.17f), R);
		K.Add(FaHide(0.75f + 0.3f * Tone), FVector(3.f, W * 0.18f, H * 0.35f), Top + R.RotateVector(FVector(0.f, W * 0.38f, 0.f)) - FVector(0.f, 0.f, H + H * 0.17f), R);
	}

	/** A Grovekin rain barrel: oak staves, two iron hoops, rain water at the brim. */
	void FaBarrel(FaKit& K, const FVector& Base, float Yaw)
	{
		const int32 N = 14;
		const float R = 45.f, H = 105.f;
		for (int32 i = 0; i < N; ++i)
		{
			const float A = Yaw + 360.f * i / N;
			const FVector D = FRotator(0.f, A, 0.f).Vector();
			K.Add(FaPlank(0.9f + 0.2f * (i % 2)), FVector(5.f, 2.f * PI * R / N + 1.f, H), Base + D * R + FVector(0.f, 0.f, H * 0.5f), FRotator(0.f, A, 0.f));
		}
		K.Disc(FaIron(), Base + FVector(0.f, 0.f, H * 0.22f), FVector::UpVector, R * 2.f + 8.f, 6.f);
		K.Disc(FaIron(), Base + FVector(0.f, 0.f, H * 0.8f), FVector::UpVector, R * 2.f + 8.f, 6.f);
		K.Disc(FaWater(), Base + FVector(0.f, 0.f, H - 8.f), FVector::UpVector, R * 2.f - 4.f, 2.f);
		K.Disc(FaPlank(0.7f), Base + FVector(0.f, 0.f, 3.f), FVector::UpVector, R * 2.f, 6.f);
	}
}

// ================================================================================================ the layout

bool FWRForestBoss::Arena(FName Id, FVector& Centre, float& Radius, TArray<FVector>& Points)
{
	using namespace WRForestArenaImpl;
	const FWRForestData& D = FWRForestData::Get();
	Points.Reset();
	if (const FWRForestData::FArena* A = D.Arenas.Find(Id))
	{
		Centre = A->Centre;
		Radius = A->Radius;
		Points = A->Points;
		return true;
	}
	// No arena row: the landmark's site, with the cook's radius (DC 2.3, 2.13, 2.14).
	const FName Kind = FaLandmarkOf(Id);
	for (const FWRForestData::FSite& S : D.Sites)
	{
		if (Kind.IsNone() || (S.Kind != Kind && S.Id != Kind)) continue;
		Centre = S.At;
		Radius = Id == TEXT("sylvaroth") ? 7000.f : Id == TEXT("old_gnarl") ? 12500.f : Id == TEXT("bramblequeen") ? 1600.f : 4500.f;
		return true;
	}
	return false;
}

bool FWRForestBoss::HeartsLayout(FVector& Centre, TArray<FVector>& Hearts, TArray<FVector>& Stones)
{
	float R = 0.f;
	TArray<FVector> P;
	Hearts.Reset();
	Stones.Reset();
	if (!Arena(TEXT("sylvaroth"), Centre, R, P))
	{
		return false;
	}
	// N, E, S, W (the frame: +x east, +y south) 55 m out; the stones between the lines on the 40 m ring.
	const FVector Dirs[4] = { FVector(0.f, -1.f, 0.f), FVector(1.f, 0.f, 0.f), FVector(0.f, 1.f, 0.f), FVector(-1.f, 0.f, 0.f) };
	const float HeartRing = WRTune(TEXT("bosses.sylvaroth.heartRing"), 5500.f), StoneRing = WRTune(TEXT("bosses.sylvaroth.stoneRing"), 4000.f);
	for (int32 i = 0; i < 4; ++i)
	{
		Hearts.Add(P.IsValidIndex(i) ? P[i] : Centre + Dirs[i] * HeartRing);
	}
	for (int32 i = 0; i < 4; ++i)
	{
		const FVector Between = (Dirs[i] + Dirs[(i + 1) % 4]).GetSafeNormal();
		Stones.Add(P.IsValidIndex(4 + i) ? P[4 + i] : Centre + Between * StoneRing);
	}
	return true;
}

bool FWRForestBoss::BowerLayout(TArray<FVector>& Oaks, TArray<FVector>& WalkA, TArray<FVector>& WalkB, TArray<FIntPoint>& WalkTrees)
{
	using namespace WRForestArenaImpl;
	FVector Centre;
	float R = 0.f;
	TArray<FVector> P;
	Oaks.Reset();
	WalkA.Reset();
	WalkB.Reset();
	WalkTrees.Reset();
	if (!Arena(TEXT("grey_matron"), Centre, R, P))
	{
		return false;
	}
	// Six hollow oaks 30 m apart on a ring (a hexagon's side is its radius), from the north going clockwise.
	const float Ring = WRTune(TEXT("bosses.grey_matron.oakRing"), 3000.f);
	for (int32 i = 0; i < 6; ++i)
	{
		Oaks.Add(P.IsValidIndex(i) ? P[i] : Centre + FRotator(0.f, -90.f + 60.f * i, 0.f).Vector() * Ring);
	}
	const auto Nearest = [&Oaks](const FVector& At, int32 Not)
	{
		int32 Best = 0;
		float BestD = TNumericLimits<float>::Max();
		for (int32 i = 0; i < Oaks.Num(); ++i)
		{
			const float D = FVector::Dist2D(Oaks[i], At);
			if (i != Not && D < BestD) { BestD = D; Best = i; }
		}
		return Best;
	};
	for (const FWRForestData::FWalkway& W : FWRForestData::Get().Walkways)
	{
		if (W.Arena != TEXT("grey_matron")) continue;
		const int32 TA = Nearest(W.A, -1);
		WalkA.Add(W.A);
		WalkB.Add(W.B);
		WalkTrees.Add(FIntPoint(TA, Nearest(W.B, TA)));
	}
	if (WalkA.Num() == 0)
	{
		// None in the file: the six that ring the Bower, oak to next oak, 15-25 m up, their decks ending at the trunks'
		// platforms (a trunk's radius and a platform's width out from the bark).
		const float OakH = WRTune(TEXT("bosses.grey_matron.oakHeight"), 7000.f);
		const float TrunkR = FaTrunkR(TEXT("bower_oak"), OakH);
		const float Out = TrunkR + WRTune(TEXT("bosses.grey_matron.platformWidth"), 260.f);
		for (int32 i = 0; i < 6; ++i)
		{
			const int32 j = (i + 1) % 6;
			const FVector Dir = (Oaks[j] - Oaks[i]).GetSafeNormal2D();
			const float Z = WRTune(TEXT("bosses.grey_matron.deckMin"), 1500.f) + 250.f * float((i * 3) % 5);
			WalkA.Add(Oaks[i] + Dir * Out + FVector(0.f, 0.f, Z));
			WalkB.Add(Oaks[j] - Dir * Out + FVector(0.f, 0.f, Z + 120.f * float((i % 2) ? 1 : -1)));
			WalkTrees.Add(FIntPoint(i, j));
		}
	}
	return true;
}

bool FWRForestBoss::HiveLayout(FVector& Base, TArray<float>& FloorZ, TArray<FVector>& Combs)
{
	float R = 0.f;
	TArray<FVector> P;
	FloorZ.Reset();
	Combs.Reset();
	if (!Arena(TEXT("bramblequeen"), Base, R, P))
	{
		return false;
	}
	// The ground floor is the earth inside the shell; two paper galleries above it; the crown under the dome. (The comb
	// points in forest.json predate the galleries' heights, so the combs are laid out here from the floors.)
	FloorZ.Add(0.f);
	FloorZ.Add(WRTune(TEXT("bosses.bramblequeen.floor2Z"), 630.f));
	FloorZ.Add(WRTune(TEXT("bosses.bramblequeen.floor3Z"), 1230.f));
	const float Mid = (WRTune(TEXT("bosses.bramblequeen.galleryIn"), 900.f) + WRTune(TEXT("bosses.bramblequeen.galleryOut"), 1300.f)) * 0.5f;
	// Each comb's top hangs 2.7 m over its floor on a paper stalk from what is above (a blade reaches its middle), over
	// the walkable ring, clear of the stairwells (30-100 deg on floor 2, 180-250 on floor 3) and the water butts (200,
	// 260, 320): floor 1 at 150 deg, floor 2 at 130, floor 3 at 20; the crown comb (the queen's own, the biggest) from
	// the dome over the top gallery at 110.
	const float Top = WRTune(TEXT("bosses.bramblequeen.combTop"), 270.f);
	const float Angles[4] = { 150.f, 130.f, 20.f, 110.f };
	for (int32 k = 0; k < 4; ++k)
	{
		const int32 Floor = FMath::Min(k, 2);
		Combs.Add(Base + FRotator(0.f, Angles[k], 0.f).Vector() * Mid + FVector(0.f, 0.f, FloorZ[Floor] + Top));
	}
	return true;
}

// ================================================================================================ AWRForestArenaState

AWRForestArenaState::AWRForestArenaState()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(1.f);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRForestArenaState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRForestArenaState, bWildHunt);
}

AWRForestArenaState* AWRForestArenaState::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World) return nullptr;
	for (TActorIterator<AWRForestArenaState> It(const_cast<UWorld*>(World)); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

// ================================================================================================ AWRForestProp

AWRForestProp::AWRForestProp()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;
	SetNetUpdateFrequency(4.f);
	SetNetCullDistanceSquared(FMath::Square(20000.f));
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	HitShape = CreateDefaultSubobject<USphereComponent>(TEXT("HitShape"));
	HitShape->SetupAttachment(RootComponent);
	HitShape->InitSphereRadius(100.f);
	WRForestArenaImpl::FaHitShape(HitShape);
}

void AWRForestProp::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRForestProp, Kind);
	DOREPLIFETIME(AWRForestProp, State);
	DOREPLIFETIME(AWRForestProp, Size);
	DOREPLIFETIME(AWRForestProp, Index);
	DOREPLIFETIME(AWRForestProp, SweepAt);
	DOREPLIFETIME(AWRForestProp, Hang);
}

void AWRForestProp::BeginPlay()
{
	Super::BeginPlay();
	BornAt = WRForestArenaImpl::FaServerNow(this);
	Build();
}

void AWRForestProp::EndPlay(const EEndPlayReason::Type Reason)
{
	if (HasAuthority() && Held.IsValid())
	{
		AWRCharacterBase* V = Held.Get();
		V->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		V->SetActorEnableCollision(true);
	}
	Super::EndPlay(Reason);
}

FVector AWRForestProp::HitPoint() const
{
	const FVector At = GetActorLocation();
	if (Kind == TEXT("heart_tree"))
	{
		// The amber heart: a glowing burl on the trunk's face toward the clearing, 3 m up (the actor faces the clearing).
		return At + GetActorForwardVector() * (Size + 30.f) + FVector(0.f, 0.f, WRTune(TEXT("bosses.sylvaroth.heartZ"), 300.f));
	}
	if (Kind == TEXT("root_hand"))
	{
		return At + FVector(0.f, 0.f, WRTune(TEXT("bosses.sylvaroth.wristZ"), 180.f));
	}
	if (Kind == TEXT("comb"))
	{
		return At - FVector(0.f, 0.f, Size * 0.45f);
	}
	return At;
}

void AWRForestProp::SetState(uint8 NewState)
{
	if (!HasAuthority() || State == NewState) return;
	State = NewState;
	OnRep_Look();
	ForceNetUpdate();
}

void AWRForestProp::OnRep_Look()
{
	if (State != BuiltState && GetWorld()) StateAt = GetWorld()->GetTimeSeconds();
	if (Kind != BuiltKind || State != BuiltState) Build();
}

bool AWRForestProp::TakeHit(AWRCharacterBase* By, float Damage, bool bFire, bool bAxe)
{
	if (!HasAuthority() || Damage <= 0.f) return false;
	const FVector P = HitPoint();
	if (Kind == TEXT("comb"))
	{
		if (State != 0) return false;
		// Paper and wax: fire eats it (x1.5), an axe splits it like anything else.
		Health -= Damage * (bFire ? WRTune(TEXT("bosses.bramblequeen.combFire"), 1.5f) : 1.f);
		WRFx::Broadcast(this, TEXT("queen.comb_hit"), P, FRotator::ZeroRotator, 1.f, NAME_None);
		if (Health <= 0.f)
		{
			Health = 0.f;
			SetState(1);
			WRFx::Broadcast(this, TEXT("queen.comb_break"), P, FRotator::ZeroRotator, FMath::Max(1.f, Size / 160.f), TEXT("forest.comb_break"));
			UE_LOG(LogWildreach, Log, TEXT("FOREST brood comb %d broken by %s"), Index, *GetNameSafe(By));
		}
		return true;
	}
	if (Kind == TEXT("heart_tree"))
	{
		// Only an awake heart-tree's amber heart can be cut (phase 3); asleep it's the bark of a 140 m tree.
		if (State != 1) return false;
		Health -= Damage * (bAxe ? WRTune(TEXT("bosses.sylvaroth.knotAxe"), 1.3f) : 1.f) * (bFire ? WRTune(TEXT("bosses.sylvaroth.knotFire"), 1.5f) : 1.f);
		WRFx::Broadcast(this, TEXT("sylvaroth.knot_hit"), P, GetActorRotation(), 1.f, NAME_None);
		if (Health <= 0.f)
		{
			Health = 0.f;
			SetState(2);
			WRFx::Broadcast(this, TEXT("sylvaroth.knot_break"), P, GetActorRotation(), 2.f, TEXT("forest.knot_break"));
			// Its line goes dark until it regrows (and the heart lights again with it: UWRForestArenas::Tick).
			for (TActorIterator<AWRForestRootLine> It(GetWorld()); It; ++It)
			{
				if (It->Index == Index && FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) < 4000.f)
				{
					It->Darken(WRTune(TEXT("bosses.sylvaroth.lineRegrow"), 45.f), 1);
				}
			}
			UE_LOG(LogWildreach, Log, TEXT("FOREST heart-tree %d's amber heart cut by %s"), Index, *GetNameSafe(By));
		}
		return true;
	}
	if (Kind == TEXT("root_hand"))
	{
		if (State != 1 && State != 0) return false;
		Health -= Damage * (bAxe ? WRTune(TEXT("bosses.sylvaroth.knotAxe"), 1.3f) : 1.f) * (bFire ? WRTune(TEXT("bosses.sylvaroth.knotFire"), 1.5f) : 1.f);
		WRFx::Broadcast(this, TEXT("sylvaroth.knot_hit"), P, FRotator::ZeroRotator, 0.8f, NAME_None);
		if (Health <= 0.f)
		{
			// The wrist tears: whoever it held drops (FWRForestBoss::Tick sees the hand torn and lets go).
			Health = 0.f;
			SetState(2);
			WRFx::Broadcast(this, TEXT("sylvaroth.knot_break"), P, FRotator::ZeroRotator, 1.4f, TEXT("forest.knot_break"));
			SetLifeSpan(3.f);
		}
		return true;
	}
	return false;
}

void AWRForestProp::Build()
{
	using namespace WRForestArenaImpl;
	BuiltKind = Kind;
	BuiltState = State;
	// The hit shape: where the blows land (the comb's middle, the amber heart, the wrist); the larder has none.
	const bool bHittable = (Kind == TEXT("comb") && State == 0) || (Kind == TEXT("heart_tree") && State == 1) || (Kind == TEXT("root_hand") && State <= 1);
	HitShape->SetWorldLocation(HitPoint());
	HitShape->SetSphereRadius(Kind == TEXT("comb") ? FMath::Max(60.f, Size * 0.5f) : Kind == TEXT("heart_tree") ? 130.f : 90.f);
	HitShape->SetCollisionEnabled(bHittable ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	if (HasAuthority() && MaxHealth <= 0.f)
	{
		MaxHealth = Health = Kind == TEXT("comb") ? WRTune(TEXT("bosses.bramblequeen.combHealth"), 300.f)
			: Kind == TEXT("heart_tree") ? WRTune(TEXT("bosses.sylvaroth.p3HeartHealth"), 1500.f)
			: Kind == TEXT("root_hand") ? WRTune(TEXT("bosses.sylvaroth.wristHealth"), 600.f) : 1.f;
	}
	if (GetNetMode() == NM_DedicatedServer || Kind.IsNone())
	{
		return;
	}
	for (USceneComponent* C : LookComps) if (C) C->DestroyComponent();
	LookComps.Reset();
	if (Glow) { Glow->DestroyComponent(); Glow = nullptr; }
	LimbRoot = nullptr;
	HandRoot = nullptr;
	TArray<FWRLookBlock> Blocks;
	FaKit K(GetActorTransform(), Blocks);

	if (Kind == TEXT("comb"))
	{
		// A brood comb: a paper cap and stalk up to what it hangs from, then the comb itself, a hanging slab of hex
		// cells (wax amber, the grubs' glow behind the caps), a paper skin wrapped half round it.
		const float H = Size, W = Size * 0.8f;
		// The stalk: chewed paper up to the floor above (or the dome), flaring where it's glued on.
		K.Rod(FaSurf::Cloth(FaPaperC() * 0.8f), FVector(0.f, 0.f, Hang), FVector(0.f, 0.f, -H * 0.18f), 22.f);
		if (Hang > 30.f) K.Disc(FaPaper(2), FVector(0.f, 0.f, Hang - 6.f), FVector::UpVector, 70.f, 12.f);
		K.Disc(FaPaper(1), FVector(0.f, 0.f, -H * 0.2f), FVector::UpVector, W * 0.9f, 10.f);
		if (State == 0)
		{
			for (int32 Row = 0; Row < 7; ++Row)
			{
				const float Z = -H * 0.25f - Row * H * 0.1f;
				const int32 N = 4 + (Row < 4 ? Row : 6 - Row);
				for (int32 c = 0; c < N; ++c)
				{
					const float Y = (c - (N - 1) * 0.5f) * W * 0.13f + ((Row % 2) ? W * 0.065f : 0.f);
					K.Hex(FaWax(), 8.f, Y, Z - H * 0.045f, Z + H * 0.045f, W * 0.06f, 30.f);
					K.Add(FaGrub(0.6f + 0.4f * FaHash(Row, c)), FVector(4.f, W * 0.07f, H * 0.06f), FVector(-4.f, Y, Z));
				}
			}
			K.Add(FaPaper(0), FVector(W * 0.5f, 6.f, H * 0.78f), FVector(-W * 0.18f, 0.f, -H * 0.58f), FRotator(0.f, 0.f, 0.f));
			Glow = FaLight(this, nullptr, FVector(60.f, 0.f, -H * 0.5f), FaRgb(224, 160, 64), 900.f, 450.f);
		}
		else
		{
			// Broken: the cap and a torn stub of cells; the rest lies in pieces on the floor under it.
			K.Hex(FaWax(), 0.f, 0.f, -H * 0.32f, -H * 0.24f, W * 0.18f, 15.f);
			for (int32 i = 0; i < 7; ++i)
			{
				const FVector Drop(FMath::Cos(i * 1.7f) * W * 0.6f, FMath::Sin(i * 2.3f) * W * 0.6f, 0.f);
				FVector G;
				FaGround(GetWorld(), GetActorLocation() + Drop, G, 200.f, 4000.f, this);
				K.Add(FaPaper(i), FVector(W * 0.25f, W * 0.18f, 8.f), GetActorTransform().InverseTransformPosition(G) + FVector(0.f, 0.f, 4.f), FRotator(0.f, i * 47.f, 6.f * (i % 3)));
			}
		}
	}
	else if (Kind == TEXT("heart_tree"))
	{
		// What the scenery's 140 m heart-tree doesn't have: the amber heart (a burl in its bark 3 m up, glowing), and its
		// lower limbs, three huge low branches that lie along the moss like roots until phase 3 wakes them.
		const float R = Size;
		const FVector Heart = GetActorTransform().InverseTransformPosition(HitPoint());
		const float HeartGlow = State == 1 ? 3.f : State == 2 ? 0.f : 0.6f;
		K.Disc(FaBark(0.9f), Heart - FVector(18.f, 0.f, 0.f), FVector::ForwardVector, 220.f, 70.f);
		K.Disc(FaAmber(HeartGlow), Heart, FVector::ForwardVector, 150.f, 50.f);
		K.Disc(FaAmber(HeartGlow * 1.5f), Heart + FVector(22.f, 0.f, 0.f), FVector::ForwardVector, 80.f, 20.f);
		for (int32 i = 0; i < 5; ++i)
		{
			const float A = i * 72.f;
			const FVector V(0.f, FMath::Cos(FMath::DegreesToRadians(A)), FMath::Sin(FMath::DegreesToRadians(A)));
			K.Rod(FaAmber(HeartGlow * 0.7f), Heart + V * 60.f, Heart + V * 160.f + FVector(-30.f, 0.f, 0.f), 9.f);
		}
		FaSpawnBlocks(this, nullptr, Blocks, LookComps);
		Blocks.Reset();
		Glow = FaLight(this, nullptr, Heart + FVector(120.f, 0.f, 0.f), FaRgb(208, 138, 30), State == 1 ? 9000.f : State == 2 ? 0.f : 1500.f, 1200.f);
		LimbRoot = NewObject<USceneComponent>(this);
		LimbRoot->SetupAttachment(RootComponent);
		LimbRoot->SetRelativeLocation(FVector(0.f, 0.f, 260.f));
		LimbRoot->RegisterComponent();
		LookComps.Add(LimbRoot);
		FWRToolMesh M;
		for (int32 i = 0; i < 3; ++i)
		{
			// Each limb leaves the trunk low, arches, and runs out 10-14 m along the ground toward the clearing.
			const float Yaw = -50.f + 50.f * i;
			const FVector Dir = FRotator(0.f, Yaw, 0.f).Vector();
			const float Len = 1000.f + 200.f * i;
			TArray<FVector> Pts;
			for (int32 s = 0; s <= 10; ++s)
			{
				const float T = s / 10.f;
				Pts.Add(Dir * (R * 0.8f + Len * T) + FVector(0.f, 0.f, 220.f * FMath::Sin(T * PI) * (1.f - T * 0.6f) - 200.f * T));
			}
			M.Tube(Pts, 70.f - 10.f * i, 10, FaRootC(), 0.9f);
			// Twisting side-roots off each limb, and moss on its back.
			for (int32 s = 3; s < 10; s += 3)
			{
				const FVector P = Pts[s];
				TArray<FVector> Side = { P, P + FRotator(0.f, Yaw + (s % 2 ? 40.f : -40.f), 0.f).Vector() * 260.f + FVector(0.f, 0.f, -60.f) };
				M.Tube(Side, 22.f, 8, FaRootC() * 0.9f, 0.9f);
			}
		}
		FaSpawnMesh(this, LimbRoot, M, LookComps);
		return;
	}
	else if (Kind == TEXT("root_hand"))
	{
		// The ring that trembles under the target (a crimson rim of split earth) and the hand that comes up through it:
		// a wrist of braided roots as thick as a barrel, five root-fingers, earth falling off them.
		const float R = Size;
		if (State == 0)
		{
			for (int32 i = 0; i < 28; ++i)
			{
				const float A = 360.f * i / 28.f;
				const FVector D = FRotator(0.f, A, 0.f).Vector();
				FVector G;
				FaGround(GetWorld(), GetActorLocation() + D * R, G, 300.f, 1200.f, this);
				const FVector L = GetActorTransform().InverseTransformPosition(G);
				K.Add(FaCrimson(2.5f), FVector(14.f, 2.f * PI * R / 28.f * 0.7f, 6.f), L + FVector(0.f, 0.f, 3.f), FRotator(0.f, A, 0.f));
				K.Add(FaSurf::Cloth(FaRgb(60, 44, 30) * 0.45f), FVector(30.f, 2.f * PI * R / 28.f * 0.8f, 10.f), L + D * 18.f + FVector(0.f, 0.f, 4.f), FRotator(8.f, A, 0.f));
			}
			FaSpawnBlocks(this, nullptr, Blocks, LookComps);
			Blocks.Reset();
			if (USceneComponent* Fx = WRFx::Attach(RootComponent, TEXT("sylvaroth.root_tremble"), NAME_None, R / 100.f)) LookComps.Add(Fx);
			return;
		}
		HandRoot = NewObject<USceneComponent>(this);
		HandRoot->SetupAttachment(RootComponent);
		HandRoot->RegisterComponent();
		LookComps.Add(HandRoot);
		FWRToolMesh M;
		for (int32 b = 0; b < 4; ++b)
		{
			TArray<FVector> Braid;
			for (int32 s = 0; s <= 8; ++s)
			{
				const float Z = -100.f + s * 50.f;
				const float A = b * 90.f + s * 35.f;
				Braid.Add(FVector(FMath::Cos(FMath::DegreesToRadians(A)) * 35.f, FMath::Sin(FMath::DegreesToRadians(A)) * 35.f, Z));
			}
			M.Tube(Braid, 32.f, 8, FaRootC(), 0.9f);
		}
		for (int32 f = 0; f < 5; ++f)
		{
			const float A = f * 72.f;
			const FVector D = FRotator(0.f, A, 0.f).Vector();
			TArray<FVector> Finger;
			for (int32 s = 0; s <= 6; ++s)
			{
				const float T = s / 6.f;
				Finger.Add(D * (40.f + 120.f * T) + FVector(0.f, 0.f, 300.f + 200.f * FMath::Sin(T * PI * 0.8f) - (State == 2 ? 120.f * T : 0.f)));
			}
			M.Tube(Finger, 20.f - 2.f * f, 8, FaRootC() * 1.05f, 0.9f);
		}
		FaSpawnMesh(this, HandRoot, M, LookComps);
		if (USceneComponent* Fx = WRFx::Attach(HandRoot, TEXT("sylvaroth.root_erupt"), NAME_None, 1.f)) LookComps.Add(Fx);
		return;
	}
	else if (Kind == TEXT("larder"))
	{
		// The hide curtain over the larder hollow at the foot of the Bower's tallest oak (F-S6): skins sewn edge to edge
		// on a pole across the hollow's mouth. Fallen, they lie heaped on the moss and the hollow is open.
		if (State == 0)
		{
			K.Rod(FaTimber(), FVector(0.f, -170.f, 330.f), FVector(0.f, 170.f, 330.f), 10.f);
			for (int32 i = 0; i < 4; ++i)
			{
				FaHide(K, FVector(4.f + 2.f * (i % 2), -127.f + 85.f * i, 325.f), 0.f, 90.f, 290.f, FaHash(i, 3));
			}
		}
		else
		{
			for (int32 i = 0; i < 4; ++i)
			{
				K.Add(FaHide(0.9f + 0.1f * i), FVector(150.f, 95.f, 6.f), FVector(90.f + 25.f * i, -120.f + 80.f * i, 4.f + 3.f * i), FRotator(0.f, 15.f * i - 20.f, 0.f));
			}
			K.Rod(FaTimber(), FVector(40.f, -170.f, 8.f), FVector(70.f, 170.f, 8.f), 10.f);
		}
	}
	FaSpawnBlocks(this, nullptr, Blocks, LookComps);
}

void AWRForestProp::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Now = WRForestArenaImpl::FaServerNow(this);
	if (GetNetMode() != NM_DedicatedServer)
	{
		if (Kind == TEXT("heart_tree") && LimbRoot)
		{
			// The limb sweep (every machine from the replicated start): it lifts over the windup, lashes round low, settles.
			const float Windup = WRTune(TEXT("bosses.sylvaroth.p3SweepWindup"), 0.9f);
			const float T = Now - SweepAt;
			float Lift = 0.f, Turn = 0.f;
			if (T >= 0.f && T < Windup) { const float A = T / Windup; Lift = 18.f * A; Turn = -25.f * A; }
			else if (T >= Windup && T < Windup + 0.45f) { const float A = (T - Windup) / 0.45f; Lift = 18.f * (1.f - A); Turn = -25.f + 85.f * FMath::InterpEaseOut(0.f, 1.f, A, 2.f); }
			else if (T >= Windup + 0.45f && T < Windup + 1.6f) { const float A = (T - Windup - 0.45f) / 1.15f; Turn = 60.f * (1.f - A); }
			LimbRoot->SetRelativeRotation(FRotator(Lift, Turn, 0.f));
		}
		if (Kind == TEXT("root_hand") && HandRoot)
		{
			// Up out of the earth (0.4 s) when it catches, held, then down again when it lets go (or torn, it slumps).
			const float Since = GetWorld()->GetTimeSeconds() - StateAt;
			const float Rise = FMath::Clamp(Since / 0.4f, 0.f, 1.f);
			float Z = -500.f + 500.f * FMath::InterpEaseOut(0.f, 1.f, Rise, 2.f);
			if (State == 3) Z = -FMath::Clamp(Since * 300.f, 0.f, 600.f);
			else if (State == 2) Z = -FMath::Clamp(Since * 120.f, 0.f, 260.f);
			HandRoot->SetRelativeLocation(FVector(0.f, 0.f, Z));
		}
	}
	if (HasAuthority() && Kind == TEXT("root_hand"))
	{
		// (The held survivor is attached to the hand by FWRForestBoss; the hand goes back into the earth when it's done.)
		if ((State == 3 || State == 2) && GetWorld()->GetTimeSeconds() - StateAt > 2.5f) Destroy();
	}
}

// ================================================================================================ AWRForestRootLine

AWRForestRootLine::AWRForestRootLine()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;
	SetNetUpdateFrequency(2.f);
	SetNetCullDistanceSquared(FMath::Square(30000.f));
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Knot = CreateDefaultSubobject<USphereComponent>(TEXT("Knot"));
	Knot->SetupAttachment(RootComponent);
	Knot->InitSphereRadius(130.f);
	WRForestArenaImpl::FaHitShape(Knot);
}

void AWRForestRootLine::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRForestRootLine, To);
	DOREPLIFETIME(AWRForestRootLine, KnotAt);
	DOREPLIFETIME(AWRForestRootLine, Index);
	DOREPLIFETIME(AWRForestRootLine, DarkUntil);
	DOREPLIFETIME(AWRForestRootLine, Why);
}

float AWRForestRootLine::ServerNow() const
{
	return WRForestArenaImpl::FaServerNow(this);
}

void AWRForestRootLine::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority()) KnotHealth = WRTune(TEXT("bosses.sylvaroth.knotHealth"), 900.f);
	Build();
}

void AWRForestRootLine::OnRep_Look()
{
	Build();
}

void AWRForestRootLine::Darken(float Seconds, uint8 Reason)
{
	if (!HasAuthority()) return;
	const float Now = ServerNow();
	// The longest darkness wins (a blight over a cut keeps it dead the full 90 s).
	if (Now + Seconds <= DarkUntil) return;
	const bool bWasLive = IsLive(Now);
	DarkUntil = Now + Seconds;
	Why = Reason;
	if (bWasLive && Reason != 3)
	{
		WRFx::Broadcast(this, Reason == 2 ? TEXT("sylvaroth.blight") : TEXT("sylvaroth.knot_break"), FVector(KnotAt), FRotator::ZeroRotator, 2.f, Reason == 2 ? NAME_None : FName(TEXT("forest.knot_break")));
	}
	ForceNetUpdate();
	UE_LOG(LogWildreach, Log, TEXT("FOREST root line %d dark %.0f s (%s)"), Index, Seconds, Reason == 1 ? TEXT("severed") : Reason == 2 ? TEXT("blighted") : TEXT("uprooting"));
}

bool AWRForestRootLine::TakeHit(AWRCharacterBase* By, float Damage, bool bFire, bool bAxe)
{
	if (!HasAuthority()) return false;
	const float Now = ServerNow();
	if (!IsLive(Now))
	{
		return true;   // a dead knot is still wood: the blow lands on it and does nothing
	}
	if (bHeartOnly)
	{
		// Phase 3: the line draws from the waking heart-tree now; the knot shrugs the blow off.
		WRFx::Broadcast(this, TEXT("sylvaroth.knot_hit"), FVector(KnotAt), FRotator::ZeroRotator, 0.6f, NAME_None);
		return true;
	}
	KnotHealth -= Damage * (bAxe ? WRTune(TEXT("bosses.sylvaroth.knotAxe"), 1.3f) : 1.f) * (bFire ? WRTune(TEXT("bosses.sylvaroth.knotFire"), 1.5f) : 1.f);
	WRFx::Broadcast(this, TEXT("sylvaroth.knot_hit"), FVector(KnotAt), FRotator::ZeroRotator, 1.f, NAME_None);
	if (KnotHealth <= 0.f)
	{
		KnotHealth = 0.f;
		Darken(WRTune(TEXT("bosses.sylvaroth.lineRegrow"), 45.f), 1);
		UE_LOG(LogWildreach, Log, TEXT("FOREST Root Knot %d severed by %s"), Index, *GetNameSafe(By));
	}
	return true;
}

void AWRForestRootLine::Build()
{
	using namespace WRForestArenaImpl;
	if (FVector(To).IsNearlyZero())
	{
		return;
	}
	Knot->SetWorldLocation(FVector(KnotAt) + FVector(0.f, 0.f, 60.f));
	if (GetNetMode() == NM_DedicatedServer || bBuilt)
	{
		return;
	}
	bBuilt = true;
	for (USceneComponent* C : LookComps) if (C) C->DestroyComponent();
	LookComps.Reset();
	// The root: as thick as a person, half sunk in the moss, following the ground from the heart-tree's foot to the
	// clearing's middle, swelling at the knot where it crosses the 25 m ring.
	const FVector From = GetActorLocation(), End = FVector(To);
	const FVector Side = FVector::CrossProduct((End - From).GetSafeNormal2D(), FVector::UpVector);
	const int32 N = 24;
	TArray<FVector> Pts;
	TArray<FVector> Tops;
	for (int32 i = 0; i <= N; ++i)
	{
		const float T = i / float(N);
		const FVector Flat = FMath::Lerp(From, End, T) + Side * 140.f * FMath::Sin(T * PI * 2.f) * (1.f - T);
		FVector G;
		FaGround(GetWorld(), Flat, G, 1500.f, 3000.f, this);
		Pts.Add(GetActorTransform().InverseTransformPosition(G + FVector(0.f, 0.f, 8.f)));
		Tops.Add(GetActorTransform().InverseTransformPosition(G + FVector(0.f, 0.f, 46.f)));
	}
	FWRToolMesh M;
	M.Tube(Pts, 45.f, 12, FaRootC(), 0.9f);
	// The knot: a burl of wood where the line bulges up, split with amber.
	const FVector KL = GetActorTransform().InverseTransformPosition(FVector(KnotAt));
	TArray<FVector> Prof;
	for (int32 i = 0; i <= 8; ++i) { const float T = i / 8.f; const float R = 120.f * FMath::Sin(T * PI) + 10.f; Prof.Add(FVector(T, R * (0.9f + 0.1f * (i % 2)), R)); }
	M.Lathe(KL - FVector(0.f, 0.f, 30.f), FVector::UpVector, 190.f, Prof, 14, FaRootC() * 1.1f, 0.85f);
	RootMesh = FaSpawnMesh(this, nullptr, M, LookComps);
	// The light strip: amber-green flecks along the root's back, every 70 cm (their glow pulses toward Sylvaroth).
	TArray<FWRLookBlock> Blocks;
	FaKit K(GetActorTransform(), Blocks);
	for (int32 i = 0; i < N; ++i)
	{
		for (int32 j = 0; j < 3; ++j)
		{
			const FVector P = FMath::Lerp(Tops[i], Tops[i + 1], (j + 0.5f) / 3.f);
			const FVector D = (Tops[i + 1] - Tops[i]).GetSafeNormal();
			K.Add(FaSurf::Lit(FMath::Lerp(FaRgb(208, 138, 30), FaRgb(127, 224, 122), 0.45f) * 0.6f, 0.f), FVector(28.f, 8.f, 4.f), P, D.Rotation());
		}
	}
	if (USceneComponent* Frame = FaSpawnBlocks(this, nullptr, Blocks, LookComps))
	{
		TArray<USceneComponent*> Ch;
		Frame->GetChildrenComponents(false, Ch);
		for (USceneComponent* C : Ch) if (UInstancedStaticMeshComponent* ISM = Cast<UInstancedStaticMeshComponent>(C)) { Strip = ISM; break; }
	}
	KnotLight = FaLight(this, nullptr, KL + FVector(0.f, 0.f, 220.f), FMath::Lerp(FaRgb(208, 138, 30), FaRgb(127, 224, 122), 0.4f), 3000.f, 900.f);
	LastGlow = -1.f;
}

void AWRForestRootLine::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Now = ServerNow();
	if (HasAuthority() && KnotHealth <= 0.f && IsLive(Now))
	{
		// Grown back: the knot is whole again.
		KnotHealth = WRTune(TEXT("bosses.sylvaroth.knotHealth"), 900.f);
		Why = 0;
	}
	if (GetNetMode() == NM_DedicatedServer || !Strip)
	{
		return;
	}
	// Live: pulses of light run from the heart-tree toward the clearing's middle. Dark: nothing. In its last crawl
	// seconds it lights back up from the heart-tree end, a front creeping toward the knot (the regrowth you can see).
	const float Crawl = WRTune(TEXT("bosses.sylvaroth.crawlSeconds"), 3.f);
	const bool bLive = IsLive(Now);
	const float Fill = bLive ? 1.f : FMath::Clamp(1.f - (DarkUntil - Now) / Crawl, 0.f, 1.f);
	const int32 Count = Strip->GetInstanceCount();
	const float Bloom = WRForest::Bloom(this) ? WRTune(TEXT("bosses.sylvaroth.bloomHealMult"), 1.5f) : 1.f;
	for (int32 i = 0; i < Count; ++i)
	{
		const float T = Count > 1 ? i / float(Count - 1) : 0.f;
		float G = 0.f;
		if (bLive)
		{
			const float Wave = FMath::Frac(T * 3.f - Now * 0.6f);
			G = (0.6f + 2.4f * FMath::Pow(1.f - Wave, 6.f)) * Bloom;
		}
		else if (Fill > 0.f && T <= Fill)
		{
			G = 1.2f + 2.f * FMath::Clamp(1.f - (Fill - T) * 6.f, 0.f, 1.f);
		}
		Strip->SetCustomDataValue(i, 8, G, i == Count - 1);
	}
	const float LightNow = bLive ? 3000.f * Bloom * (0.7f + 0.3f * FMath::Sin(Now * 3.f)) : Fill > 0.5f ? 1500.f * Fill : 0.f;
	if (KnotLight && !FMath::IsNearlyEqual(LightNow, LastGlow, 50.f))
	{
		KnotLight->SetIntensity(LightNow);
		LastGlow = LightNow;
	}
}

// ================================================================================================ AWRForestWalkway

AWRForestWalkway::AWRForestWalkway()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;
	SetNetUpdateFrequency(2.f);
	SetNetCullDistanceSquared(FMath::Square(25000.f));
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	AnchorA = CreateDefaultSubobject<USphereComponent>(TEXT("AnchorA"));
	AnchorA->SetupAttachment(RootComponent);
	AnchorA->InitSphereRadius(55.f);
	WRForestArenaImpl::FaHitShape(AnchorA);
	AnchorB = CreateDefaultSubobject<USphereComponent>(TEXT("AnchorB"));
	AnchorB->SetupAttachment(RootComponent);
	AnchorB->InitSphereRadius(55.f);
	WRForestArenaImpl::FaHitShape(AnchorB);
}

void AWRForestWalkway::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRForestWalkway, EndA);
	DOREPLIFETIME(AWRForestWalkway, EndB);
	DOREPLIFETIME(AWRForestWalkway, TreeA);
	DOREPLIFETIME(AWRForestWalkway, TreeB);
	DOREPLIFETIME(AWRForestWalkway, bDown);
	DOREPLIFETIME(AWRForestWalkway, FellAt);
}

void AWRForestWalkway::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority()) AnchorHealth[0] = AnchorHealth[1] = WRTune(TEXT("bosses.grey_matron.anchorHealth"), 100.f);
	Build();
}

void AWRForestWalkway::OnRep_Look()
{
	Build();
}

FVector AWRForestWalkway::DeckAt(float T) const
{
	const FVector A = FVector(EndA), B = FVector(EndB);
	return WRForestArenaImpl::FaSag(A, B, T, FVector::Dist(A, B) * WRTune(TEXT("bosses.grey_matron.sag"), 0.045f));
}

int32 AWRForestWalkway::AnchorOf(const UPrimitiveComponent* Comp) const
{
	return Comp == AnchorA ? 0 : Comp == AnchorB ? 1 : -1;
}

void AWRForestWalkway::SetDeckCollision(bool bOn)
{
	for (UBoxComponent* B : Deck) if (B) B->SetCollisionEnabled(bOn ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	AnchorA->SetCollisionEnabled(bOn ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	AnchorB->SetCollisionEnabled(bOn ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
}

bool AWRForestWalkway::TakeHit(int32 Anchor, AWRCharacterBase* By, float Damage)
{
	if (!HasAuthority() || bDown || Anchor < 0 || Anchor > 1 || Damage <= 0.f) return false;
	AnchorHealth[Anchor] -= Damage;
	const FVector At = Anchor == 0 ? FVector(EndA) : FVector(EndB);
	WRFx::Broadcast(this, TEXT("arena.rope_fray"), At, FRotator::ZeroRotator, 1.f, NAME_None);
	if (AnchorHealth[Anchor] <= 0.f)
	{
		WRFx::Broadcast(this, TEXT("arena.rope_snap"), At, FRotator::ZeroRotator, 1.f, TEXT("forest.anchor_cut"));
		Fall(By && (By->GetPlayerState() || !By->GetTameOwner().IsEmpty()), By);
	}
	return true;
}

void AWRForestWalkway::Fall(bool bByPlayer, AActor* By)
{
	if (!HasAuthority() || bDown) return;
	bDown = true;
	bCutByPlayer |= bByPlayer;
	FellAt = WRForestArenaImpl::FaServerNow(this);
	SetDeckCollision(false);
	WRFx::Broadcast(this, TEXT("arena.walkway_snap"), DeckAt(0.5f), (FVector(EndB) - FVector(EndA)).Rotation(), FVector::Dist(FVector(EndA), FVector(EndB)) / 1000.f, TEXT("forest.walkway_fall"));
	OnRep_Look();
	ForceNetUpdate();
	if (UWRForestArenas* Arenas = UWRForestArenas::Get(this)) Arenas->LastFallHours = WRForestArenaImpl::FaHours(this);
	UE_LOG(LogWildreach, Log, TEXT("FOREST Bower walkway %d-%d falls (%s)"), TreeA, TreeB, bByPlayer ? TEXT("cut") : TEXT("broken"));
}

void AWRForestWalkway::Regrow()
{
	if (!HasAuthority() || !bDown) return;
	bDown = false;
	bCutByPlayer = false;
	AnchorHealth[0] = AnchorHealth[1] = WRTune(TEXT("bosses.grey_matron.anchorHealth"), 100.f);
	SetDeckCollision(true);
	OnRep_Look();
	ForceNetUpdate();
}

void AWRForestWalkway::NotifyHit(UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
	Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);
	// An arrow thudding into the planks (the Grey Matron hates the noise: she leaps to another walkway).
	if (HasAuthority() && Cast<AWRProjectile>(Other) && AnchorOf(MyComp) < 0)
	{
		ArrowAt = GetWorld()->GetTimeSeconds();
	}
}

void AWRForestWalkway::Build()
{
	using namespace WRForestArenaImpl;
	const FVector A = FVector(EndA), B = FVector(EndB);
	if (A.IsNearlyZero() || B.IsNearlyZero())
	{
		return;
	}
	AnchorA->SetWorldLocation(A + FVector(0.f, 0.f, 110.f) - (B - A).GetSafeNormal2D() * 40.f);
	AnchorB->SetWorldLocation(B + FVector(0.f, 0.f, 110.f) + (B - A).GetSafeNormal2D() * 40.f);
	// The deck's collision: eight boxes down the sag (made once; on or off with the walkway).
	if (Deck.Num() == 0)
	{
		const int32 N = 8;
		for (int32 i = 0; i < N; ++i)
		{
			const FVector P0 = DeckAt(i / float(N)), P1 = DeckAt((i + 1) / float(N));
			const FVector D = P1 - P0;
			UBoxComponent* Box = NewObject<UBoxComponent>(this);
			Box->SetupAttachment(RootComponent);
			Box->SetBoxExtent(FVector(D.Size() * 0.5f + 6.f, 75.f, 6.f));
			Box->SetWorldLocationAndRotation((P0 + P1) * 0.5f - FVector(0.f, 0.f, 3.f), D.Rotation());
			Box->SetCollisionProfileName(TEXT("BlockAll"));
			Box->SetNotifyRigidBodyCollision(true);
			Box->SetCanEverAffectNavigation(false);
			Box->RegisterComponent();
			Deck.Add(Box);
		}
	}
	SetDeckCollision(!bDown);
	if (GetNetMode() == NM_DedicatedServer || (bBuilt && bBuiltDown == bDown))
	{
		return;
	}
	bBuilt = true;
	bBuiltDown = bDown;
	for (USceneComponent* C : LookComps) if (C) C->DestroyComponent();
	LookComps.Reset();
	DeckLook = NewObject<USceneComponent>(this);
	DeckLook->SetupAttachment(RootComponent);
	DeckLook->RegisterComponent();
	LookComps.Add(DeckLook);
	TArray<FWRLookBlock> Blocks;
	FaKit K(GetActorTransform(), Blocks);
	const FTransform& T = GetActorTransform();
	const auto L = [&T](const FVector& W) { return T.InverseTransformPosition(W); };
	const FVector Dir = (B - A).GetSafeNormal2D();
	const FVector Side = FVector::CrossProduct(Dir, FVector::UpVector);
	const float Len = FVector::Dist(A, B);
	// The anchors (both drawn, up or down): an iron ring on a heartwood peg driven into the trunk, the rope wound round
	// the peg four turns before it runs out to the deck's stringers.
	TArray<FWRLookBlock> Anchors;
	FaKit KA(GetActorTransform(), Anchors);
	for (int32 e = 0; e < 2; ++e)
	{
		const FVector End = e == 0 ? A : B;
		const FVector Out = e == 0 ? -Dir : Dir;
		const FVector Peg = End + Out * 55.f + FVector(0.f, 0.f, 110.f);
		KA.Rod(FaTimber(0.8f), L(Peg - Out * 10.f), L(Peg + Out * 70.f), 16.f);
		KA.Disc(FaIron(), L(Peg - Out * 12.f), Side, 34.f, 5.f);
		for (int32 w = 0; w < 4; ++w) KA.Disc(FaRope(), L(Peg + Out * (12.f + 9.f * w)), Out, 24.f, 7.f);
		// The platform's lip the deck meets at the trunk (the scenery's platform is the rest).
		KA.Add(FaPlank(0.9f), FVector(40.f, 170.f, 10.f), L(End + Out * 10.f - FVector(0.f, 0.f, 5.f)), Dir.Rotation());
		if (bDown)
		{
			// Cut rope hanging from the peg (3-5 m), frayed at its end.
			for (int32 s = -1; s <= 1; s += 2)
			{
				const FVector Top = Peg + Side * 20.f * s;
				const float Hang = 300.f + 150.f * (e + (s > 0 ? 1 : 0)) * 0.5f;
				KA.Rod(FaRope(), L(Top), L(Top - Out * 40.f - FVector(0.f, 0.f, Hang)), 5.f);
				KA.Spike(FaRope(), L(Top - Out * 40.f - FVector(0.f, 0.f, Hang)), FVector(0.f, 0.f, -1.f), 8.f, 18.f);
			}
		}
	}
	FaSpawnBlocks(this, nullptr, Anchors, LookComps);
	if (!bDown)
	{
		// Planks across two stringer ropes, every 32 cm, each a little different; rope rails at hip height that sag
		// with the deck, tied to the stringers by a rope upright every 1.2 m.
		const int32 Planks = FMath::Max(8, FMath::RoundToInt(Len / 32.f));
		for (int32 i = 0; i < Planks; ++i)
		{
			const float U = (i + 0.5f) / Planks;
			const FVector P = DeckAt(U);
			const FVector Tan = (DeckAt(FMath::Min(1.f, U + 0.01f)) - DeckAt(FMath::Max(0.f, U - 0.01f))).GetSafeNormal();
			const float Jit = FaHash(i, TreeA * 7 + TreeB);
			K.Add(FaPlank(0.85f + 0.3f * Jit), FVector(26.f, 140.f + 16.f * Jit, 5.f), L(P), FRotator(Tan.Rotation().Pitch, Dir.Rotation().Yaw, (Jit - 0.5f) * 3.f));
		}
		const int32 Segs = 16;
		for (int32 i = 0; i < Segs; ++i)
		{
			const FVector P0 = DeckAt(i / float(Segs)), P1 = DeckAt((i + 1) / float(Segs));
			for (int32 s = -1; s <= 1; s += 2)
			{
				K.Rod(FaRope(), L(P0 + Side * 62.f * s - FVector(0.f, 0.f, 6.f)), L(P1 + Side * 62.f * s - FVector(0.f, 0.f, 6.f)), 5.f);
				K.Rod(FaRope(), L(P0 + Side * 75.f * s + FVector(0.f, 0.f, 95.f)), L(P1 + Side * 75.f * s + FVector(0.f, 0.f, 95.f)), 4.f);
			}
		}
		const int32 Posts = FMath::Max(2, FMath::RoundToInt(Len / 120.f));
		for (int32 i = 1; i < Posts; ++i)
		{
			const FVector P = DeckAt(i / float(Posts));
			for (int32 s = -1; s <= 1; s += 2) K.Rod(FaRope(), L(P + Side * 70.f * s), L(P + Side * 75.f * s + FVector(0.f, 0.f, 95.f)), 3.5f);
		}
		FaSpawnBlocks(this, DeckLook, Blocks, LookComps);
	}
	else
	{
		// Down: the deck lies broken on the moss under where it hung, planks scattered either side of its line.
		const int32 Pieces = FMath::Max(6, FMath::RoundToInt(Len / 150.f));
		for (int32 i = 0; i < Pieces; ++i)
		{
			const float U = (i + 0.5f) / Pieces;
			const float Off = (FaHash(i, 31) - 0.5f) * 260.f;
			FVector G;
			FaGround(GetWorld(), FMath::Lerp(A, B, U) + Side * Off, G, 2500.f, 6000.f, this);
			K.Add(FaPlank(0.8f + 0.3f * FaHash(i, 5)), FVector(110.f + 60.f * FaHash(i, 9), 28.f, 5.f), L(G + FVector(0.f, 0.f, 3.f + 4.f * (i % 3))), FRotator(0.f, Dir.Rotation().Yaw + 60.f * (FaHash(i, 13) - 0.5f), 4.f * (i % 2)));
			if (i % 2 == 0) K.Rod(FaRope(), L(G + FVector(0.f, 0.f, 4.f)), L(G + Dir * 200.f + Side * 60.f * (FaHash(i, 2) - 0.5f) + FVector(0.f, 0.f, 3.f)), 4.f);
		}
		FaSpawnBlocks(this, DeckLook, Blocks, LookComps);
	}
}

void AWRForestWalkway::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (GetNetMode() == NM_DedicatedServer || !DeckLook || !bDown)
	{
		return;
	}
	// The fall (every machine from the replicated time): the planks drop the first second and a half from where the deck
	// hung, then the broken deck is what lies there. (The down look is built in place; this lifts it to the hang height
	// and lets it fall.)
	const float T = WRForestArenaImpl::FaServerNow(this) - FellAt;
	const float Fall = WRTune(TEXT("bosses.grey_matron.fallSeconds"), 1.5f);
	if (T >= 0.f && T < Fall)
	{
		const float Height = FMath::Max(0.f, DeckAt(0.5f).Z - GetActorLocation().Z);
		const float Drop = 0.5f * 980.f * T * T;
		DeckLook->SetRelativeLocation(FVector(0.f, 0.f, FMath::Max(0.f, Height - Drop)));
		DeckLook->SetRelativeRotation(FRotator(0.f, 0.f, 12.f * FMath::Sin(T * 4.f) * (1.f - T / Fall)));
	}
	else if (!DeckLook->GetRelativeLocation().IsNearlyZero())
	{
		DeckLook->SetRelativeLocation(FVector::ZeroVector);
		DeckLook->SetRelativeRotation(FRotator::ZeroRotator);
	}
}

// ================================================================================================ AWRForestLob

AWRForestLob::AWRForestLob()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;
	SetNetCullDistanceSquared(FMath::Square(20000.f));
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRForestLob::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRForestLob, Kind);
	DOREPLIFETIME(AWRForestLob, From);
	DOREPLIFETIME(AWRForestLob, To);
	DOREPLIFETIME(AWRForestLob, LaunchAt);
	DOREPLIFETIME(AWRForestLob, Flight);
	DOREPLIFETIME(AWRForestLob, Arc);
}

void AWRForestLob::BeginPlay()
{
	Super::BeginPlay();
	// A late joiner (or a client whose first bunch already carried the launch) builds its look here; OnRep covers the rest.
	if (!Kind.IsNone() && GetNetMode() != NM_DedicatedServer && !bBuilt) BuildLook();
	// A lob that never launched (its boss died between spawn and throw) cleans itself up.
	if (HasAuthority()) SetLifeSpan(30.f);
}

float AWRForestLob::ServerNow() const
{
	return WRForestArenaImpl::FaServerNow(this);
}

void AWRForestLob::Launch(FName InKind, const FVector& InFrom, const FVector& InTo, float InFlight, float InArc, float Delay)
{
	Kind = InKind;
	From = InFrom;
	To = InTo;
	Flight = FMath::Max(0.3f, InFlight);
	Arc = InArc;
	LaunchAt = ServerNow() + FMath::Max(0.f, Delay);
	LastPos = InFrom;
	SetActorLocation(InFrom);
	OnRep_Launch();
	ForceNetUpdate();
}

FVector AWRForestLob::PointAt(float T) const
{
	const FVector A = FVector(From), B = FVector(To);
	const float C = FMath::Clamp(T, 0.f, 1.f);
	if (Arc <= 0.f)
	{
		// A straight fall (a branch breaking off the canopy): it accelerates down.
		return FMath::Lerp(A, B, C * C);
	}
	return FMath::Lerp(A, B, C) + FVector(0.f, 0.f, 4.f * Arc * C * (1.f - C));
}

void AWRForestLob::OnRep_Launch()
{
	if (!bBuilt && !Kind.IsNone() && GetNetMode() != NM_DedicatedServer) BuildLook();
}

void AWRForestLob::BuildLook()
{
	using namespace WRForestArenaImpl;
	bBuilt = true;
	Look = NewObject<USceneComponent>(this);
	Look->SetupAttachment(RootComponent);
	Look->RegisterComponent();
	FWRToolMesh M;
	if (Kind == TEXT("stump"))
	{
		// A stump torn out with its root ball: the stump from the tree kit, a ball of soil and the roots splayed out.
		if (UStaticMesh* Mesh = WRTreeKit::Mesh(GetTransientPackage(), TEXT("stump"), WRTreeKit::VariantAt(FVector(From))))
		{
			UStaticMeshComponent* S = NewObject<UStaticMeshComponent>(this);
			S->SetStaticMesh(Mesh);
			S->SetupAttachment(Look);
			const float Sc = 90.f / FMath::Max(1.f, WRTreeKit::ReferenceHeight(TEXT("stump")));
			S->SetRelativeScale3D(FVector(Sc));
			S->SetRelativeLocation(FVector(0.f, 0.f, -10.f));
			S->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			S->RegisterComponent();
		}
		TArray<FVector> Prof;
		for (int32 i = 0; i <= 8; ++i) { const float T = i / 8.f; const float R = 75.f * FMath::Sin(T * PI) + 15.f; Prof.Add(FVector(T, R, R * 0.85f)); }
		M.Lathe(FVector(0.f, 0.f, -95.f), FVector::UpVector, 100.f, Prof, 12, FaRgb(70, 54, 38) * 0.45f, 0.95f);
		for (int32 r = 0; r < 7; ++r)
		{
			const FVector D = FRotator(-30.f, r * 51.f, 0.f).Vector();
			M.Tube({ FVector(0.f, 0.f, -40.f), D * 90.f + FVector(0.f, 0.f, -60.f), D * 150.f + FVector(0.f, 0.f, -110.f) }, 10.f - r * 0.6f, 6, FaRootC(), 0.9f);
		}
	}
	else if (Kind == TEXT("pod"))
	{
		// A seed pod the size of a cart wheel's hub: orange-brown, ribbed, splitting at its seams.
		TArray<FVector> Prof;
		for (int32 i = 0; i <= 10; ++i) { const float T = i / 10.f; const float R = 48.f * FMath::Pow(FMath::Max(0.f, FMath::Sin(T * PI)), 0.8f) + 3.f; Prof.Add(FVector(T, R, R * 0.9f)); }
		M.Lathe(FVector(0.f, 0.f, -55.f), FVector::UpVector, 110.f, Prof, 10, FaPodC(), 0.6f);
		M.Tube({ FVector(0.f, 0.f, 50.f), FVector(8.f, 0.f, 75.f), FVector(20.f, 4.f, 85.f) }, 6.f, 6, FaRootC(), 0.9f);
	}
	else
	{
		// A dead branch from the canopy on its back: a crooked limb with two forks and broken twig ends.
		M.Tube({ FVector(-180.f, 0.f, 0.f), FVector(-60.f, 20.f, 10.f), FVector(60.f, -15.f, 0.f), FVector(190.f, 10.f, -10.f) }, 22.f, 8, FaDeadC(), 0.95f);
		M.Tube({ FVector(-40.f, 18.f, 8.f), FVector(20.f, 90.f, 30.f), FVector(60.f, 150.f, 40.f) }, 11.f, 6, FaDeadC(), 0.95f);
		M.Tube({ FVector(80.f, -12.f, 0.f), FVector(120.f, -90.f, -20.f), FVector(150.f, -140.f, -10.f) }, 9.f, 6, FaDeadC(), 0.95f);
	}
	UProceduralMeshComponent* P = NewObject<UProceduralMeshComponent>(this);
	P->SetupAttachment(Look);
	P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	P->RegisterComponent();
	M.ApplyTo(P);
	Look->SetVisibility(false, true);
	// Its shadow on the ground where it will come down (this machine's own, grown from the replicated launch).
	if (UWorld* World = GetWorld())
	{
		const float R = Kind == TEXT("stump") ? 280.f : Kind == TEXT("pod") ? 220.f : 260.f;
		Shadow = AWRGroundMarker::Spawn(World, FVector(To) + FVector(0.f, 0.f, 4.f), FRotator::ZeroRotator, FVector2D(R, R), false, FLinearColor(0.015f, 0.015f, 0.02f), 0.f);
		if (AWRGroundMarker* S = Shadow.Get())
		{
			// This machine's own (a listen server's must not replicate on top of each client's).
			S->SetReplicates(false);
			S->SetActorScale3D(FVector(0.15f, 0.15f, 1.f));
		}
	}
}

void AWRForestLob::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Now = ServerNow();
	const float T = (Now - LaunchAt) / FMath::Max(0.1f, Flight);
	if (GetNetMode() != NM_DedicatedServer && Look)
	{
		const bool bFlying = T >= 0.f && !bLanded;
		Look->SetVisibility(bFlying, true);
		if (bFlying)
		{
			const FVector P = PointAt(T);
			Look->SetWorldLocation(P);
			// Stumps and pods tumble; a branch turns slowly as it falls.
			const float Spin = Kind == TEXT("branch") ? 60.f : 260.f;
			Look->SetWorldRotation(FRotator(Spin * Now * 0.7f, (FVector(To) - FVector(From)).Rotation().Yaw + Spin * Now * 0.3f, Spin * Now * 0.2f));
		}
		if (AWRGroundMarker* S = Shadow.Get())
		{
			// From the throw (or the crack) the shadow grows to the size of what's coming; gone on impact.
			const float Grow = FMath::Clamp((Now - (LaunchAt - (Kind == TEXT("branch") ? 1.f : 0.f))) / FMath::Max(0.2f, Flight + (Kind == TEXT("branch") ? 1.f : 0.f)), 0.f, 1.f);
			S->SetActorScale3D(FVector(0.15f + 0.85f * Grow, 0.15f + 0.85f * Grow, 1.f));
			if (T >= 1.f || bLanded) { S->Destroy(); Shadow = nullptr; }
		}
	}
	if (!HasAuthority() || bLanded || T < 0.f)
	{
		return;
	}
	// The server flies it against the world: a stump or a standing stone between it and you stops it there.
	const FVector Next = PointAt(FMath::Min(T, 1.f));
	FHitResult Blocked;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(WRForestLob), false, this);
	if (AActor* Who = Thrower.Get()) Q.AddIgnoredActor(Who);
	const float R = Kind == TEXT("stump") ? 70.f : Kind == TEXT("pod") ? 45.f : 30.f;
	if (T > 0.15f && GetWorld()->SweepSingleByChannel(Blocked, LastPos, Next, FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeSphere(R), Q) && !Cast<APawn>(Blocked.GetActor()))
	{
		Land(Blocked.ImpactPoint);
		return;
	}
	LastPos = Next;
	SetActorLocation(Next);
	if (T >= 1.f)
	{
		Land(FVector(To));
	}
}

void AWRForestLob::Land(const FVector& At)
{
	bLanded = true;
	const FName Fx = Kind == TEXT("stump") ? FName(TEXT("gnarl.stump_land")) : Kind == TEXT("pod") ? FName(TEXT("sylvaroth.seed_burst")) : FName(TEXT("sylvaroth.branch_land"));
	const FName Snd = Kind == TEXT("pod") ? FName(TEXT("forest.seed_pod")) : FName(TEXT("stomp"));
	WRFx::Broadcast(this, Fx, At, FRotator::ZeroRotator, Radius / 300.f, Snd);
	AWRBoss* Boss = Thrower.Get();
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		AWRCharacterBase* V = *It;
		if (V == Boss || !V->IsAlive() || V->GetAttachParentActor() || (Boss && !Boss->IsHostileTo(V))) continue;
		if (FVector::Dist2D(V->GetActorLocation(), At) > Radius + V->GetSimpleCollisionRadius() || FMath::Abs(V->GetActorLocation().Z - At.Z) > Radius + V->GetSimpleCollisionHalfHeight() * 2.f) continue;
		FWRHit H = Hit;
		H.Origin = At;
		UWRCombatLibrary::ApplyHit(V, H);
		if (EntangleSeconds > 0.f && V->IsAlive()) WRForest::Entangle(V, EntangleSeconds, Boss);
	}
	if (Boss) FWRForestBoss::LobLanded(Boss, Kind);
	SetLifeSpan(0.5f);
	ForceNetUpdate();
}

// ================================================================================================ the arena scenery

namespace WRForestArenaImpl
{
	/** Builds one arena landmark's look in the actor's space (every machine; the server keeps the collision). */
	struct FaBuild
	{
		AWRForestArenaScenery* A;
		FTransform T;
		TArray<FWRLookBlock> Day, Night;
		FaKit K, N;
		FWRToolMesh M;
		bool bServer;

		explicit FaBuild(AWRForestArenaScenery* InA)
			: A(InA), T(InA->GetActorTransform()), K(T, Day), N(T, Night), bServer(InA->GetNetMode() == NM_DedicatedServer) {}

		FVector Local(const FVector& W) const { return T.InverseTransformPosition(W); }
		/** The ground under a world point, as a local point. */
		FVector GroundL(const FVector& World, float Lift = 0.f) const
		{
			FVector G;
			FaGround(A->GetWorld(), World, G, 3000.f, 8000.f, A);
			return Local(G) + FVector(0.f, 0.f, Lift);
		}
	};
}

AWRForestArenaScenery::AWRForestArenaScenery()
{
	bReplicates = false;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRForestArenaScenery::EndPlay(const EEndPlayReason::Type Reason)
{
	for (USceneComponent* C : LookComps) if (C) C->DestroyComponent();
	LookComps.Reset();
	Super::EndPlay(Reason);
}

USceneComponent* AWRForestArenaScenery::AddBlocks(const TArray<FWRLookBlock>& Blocks, bool bNight)
{
	USceneComponent* F = WRForestArenaImpl::FaSpawnBlocks(this, nullptr, Blocks, LookComps);
	if (F && bNight) NightOnly.Add(F);
	return F;
}

UStaticMeshComponent* AWRForestArenaScenery::AddTree(FName Species, int32 Variant, const FVector& Base, float Height, float Yaw)
{
	// Base is world; the trunk's collision capsules on every machine, the mesh where there are eyes.
	const float Ref = FMath::Max(1.f, WRTreeKit::ReferenceHeight(Species));
	const FTransform Where(FRotator(0.f, Yaw, 0.f), Base, FVector(Height / Ref));
	WRTreeKit::AddCollision(this, Species, Variant, Where);
	if (GetNetMode() == NM_DedicatedServer) return nullptr;
	UStaticMesh* Mesh = WRTreeKit::Mesh(GetTransientPackage(), Species, Variant);
	if (!Mesh) return nullptr;
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(Mesh);
	C->SetupAttachment(RootComponent);
	C->SetWorldTransform(Where);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCanEverAffectNavigation(false);
	C->RegisterComponent();
	LookComps.Add(C);
	return C;
}

void AWRForestArenaScenery::AddCapsule(const FVector& Base, float Radius, float Height)
{
	UCapsuleComponent* C = NewObject<UCapsuleComponent>(this);
	C->SetupAttachment(RootComponent);
	C->SetCapsuleSize(Radius, FMath::Max(Radius, Height * 0.5f));
	C->SetWorldLocation(Base + FVector(0.f, 0.f, Height * 0.5f));
	C->SetCollisionProfileName(TEXT("BlockAll"));
	C->SetCanEverAffectNavigation(false);
	C->RegisterComponent();
}

void AWRForestArenaScenery::AddBox(const FVector& Centre, const FVector& Half, const FRotator& Rot)
{
	// Centre and Rot are local to the actor.
	UBoxComponent* B = NewObject<UBoxComponent>(this);
	B->SetupAttachment(RootComponent);
	B->SetBoxExtent(Half.ComponentMax(FVector(2.f)));
	B->SetRelativeLocation(Centre);
	B->SetRelativeRotation(Rot);
	B->SetCollisionProfileName(TEXT("BlockAll"));
	B->SetCanEverAffectNavigation(false);
	B->RegisterComponent();
}

void AWRForestArenaScenery::Build()
{
	if (Kind == TEXT("four_hearts")) BuildFourHearts();
	else if (Kind == TEXT("clear_cut")) BuildClearCut();
	else if (Kind == TEXT("paper_hollow")) BuildPaperHollow();
	else if (Kind == TEXT("bower")) BuildBower();
	UE_LOG(LogWildreach, Log, TEXT("FOREST arena scenery %s built at %s"), *Kind.ToString(), *GetActorLocation().ToCompactString());
}

void AWRForestArenaScenery::BuildFourHearts()
{
	using namespace WRForestArenaImpl;
	FaBuild B(this);
	FVector Centre;
	TArray<FVector> Hearts, Stones;
	FWRForestBoss::HeartsLayout(Centre, Hearts, Stones);
	const float HeartH = WRTune(TEXT("bosses.sylvaroth.heartHeight"), 14000.f);
	// The four heart-trees, 140 m: the skyline compass of the forest. Their sap veins glow amber (the kit's "Glow"
	// slot), brighter at night and brightest when the Bloom comes (Tick).
	for (int32 i = 0; i < Hearts.Num(); ++i)
	{
		FVector G;
		FaGround(GetWorld(), Hearts[i], G, 4000.f, 9000.f, this);
		if (UStaticMeshComponent* Tree = AddTree(TEXT("heart"), i % 4, G - FVector(0.f, 0.f, 40.f), HeartH, (Centre - G).Rotation().Yaw))
		{
			HeartTrees.Add(Tree);
		}
	}
	// The four Grovekin standing stones on the 40 m ring between the lines: 4 m of grey stone each, a weathered band of
	// carved knotwork round them at chest height (cover from the seed pods: the lobs stop on them).
	for (int32 i = 0; i < Stones.Num(); ++i)
	{
		const FVector Base = B.GroundL(Stones[i], -30.f);
		const float Yaw = (Centre - Stones[i]).Rotation().Yaw;
		const FRotator R(0.f, Yaw, 0.f);
		B.K.Add(FaStone(0.95f), FVector(90.f, 160.f, 420.f), Base + FVector(0.f, 0.f, 210.f), FRotator(1.5f * (i % 2 ? 1 : -1), Yaw, 2.f));
		B.K.Add(FaStone(1.08f), FVector(70.f, 130.f, 40.f), Base + FVector(0.f, 0.f, 440.f), FRotator(3.f, Yaw, -2.f));
		for (int32 k = 0; k < 6; ++k)
		{
			B.K.Add(FaStone(0.8f), FVector(96.f, 22.f, 16.f), Base + R.RotateVector(FVector(0.f, -60.f + 24.f * k, 150.f + 18.f * (k % 2))), FRotator(0.f, Yaw, 35.f * (k % 2 ? 1 : -1)));
		}
		B.K.Add(FaMoss(), FVector(100.f, 170.f, 14.f), Base + FVector(0.f, 0.f, 452.f), R);
		B.K.Add(FaMoss(0.9f), FVector(140.f, 220.f, 16.f), Base + FVector(0.f, 0.f, 26.f), R);
		AddBox(Base + FVector(0.f, 0.f, 220.f), FVector(48.f, 82.f, 220.f), R);
		// At night a faint foxfire glow in the carving (the old Grovekin marked the way in pale green, never gold).
		B.N.Add(FaFoxfire(1.2f), FVector(98.f, 6.f, 6.f), Base + R.RotateVector(FVector(0.f, 0.f, 150.f)), R);
	}
	// The Nature altar: a ring of nine small stones against the west cliff behind the west heart-tree, a slab in the
	// middle where the altar stands (AWRBossAltar, the server's).
	FVector AltarAt = FVector::ZeroVector;
	if (const FWRForestData::FArena* Row = FWRForestData::Get().Arenas.Find(TEXT("sylvaroth")); Row && !Row->Altar.IsNearlyZero()) AltarAt = Row->Altar;
	if (AltarAt.IsNearlyZero() && Hearts.IsValidIndex(3)) AltarAt = Hearts[3] + (Hearts[3] - Centre).GetSafeNormal2D() * WRTune(TEXT("bosses.sylvaroth.altarBehind"), 2200.f);
	if (!AltarAt.IsNearlyZero())
	{
		const FVector Mid = B.GroundL(AltarAt);
		for (int32 i = 0; i < 9; ++i)
		{
			const float Ang = 360.f * i / 9.f;
			const FVector P = B.GroundL(AltarAt + FRotator(0.f, Ang, 0.f).Vector() * 520.f, -20.f);
			const float H = 140.f + 60.f * FaHash(i, 77);
			B.K.Add(FaStone(0.9f + 0.2f * FaHash(i, 3)), FVector(60.f, 80.f, H), P + FVector(0.f, 0.f, H * 0.5f), FRotator(2.f, Ang, 3.f * (i % 2)));
			B.K.Add(FaMoss(), FVector(64.f, 84.f, 10.f), P + FVector(0.f, 0.f, H + 2.f), FRotator(0.f, Ang, 0.f));
			AddBox(P + FVector(0.f, 0.f, H * 0.5f), FVector(30.f, 40.f, H * 0.5f), FRotator(0.f, Ang, 0.f));
		}
		B.K.Add(FaStone(1.05f), FVector(320.f, 320.f, 30.f), Mid + FVector(0.f, 0.f, 10.f), FRotator(0.f, 22.f, 0.f));
		B.K.Add(FaMoss(0.8f), FVector(340.f, 60.f, 6.f), Mid + FVector(0.f, -150.f, 26.f), FRotator(0.f, 22.f, 0.f));
	}
	// The Fern Stair: the way in from the east, broad slabs of old stone set into the slope up to the clearing's edge,
	// ferns crowding both sides (where the run back from the shrine-stone comes in).
	const FVector East(1.f, 0.f, 0.f);
	const float Edge = WRTune(TEXT("bosses.sylvaroth.clearingRadius"), 7000.f);
	FVector Prev = FVector::ZeroVector;
	for (int32 i = 0; i < 14; ++i)
	{
		const FVector W = Centre + East * (Edge - 300.f + 130.f * i) + FVector(0.f, 40.f * FMath::Sin(i * 0.7f), 0.f);
		FVector P = B.GroundL(W, -6.f);
		if (!Prev.IsZero() && P.Z > Prev.Z - 4.f && P.Z < Prev.Z + 4.f) P.Z = Prev.Z - 8.f;
		Prev = P;
		const float Wd = 320.f - 10.f * (i % 3);
		B.K.Add(FaStone(0.85f + 0.2f * FaHash(i, 11)), FVector(120.f, Wd, 28.f), P + FVector(0.f, 0.f, 10.f), FRotator(0.f, 2.f * FMath::Sin(i * 1.3f), 0.f));
		B.K.Add(FaMoss(0.9f), FVector(40.f, Wd * 0.6f, 4.f), P + FVector(-48.f, 30.f * (i % 2 ? 1 : -1), 26.f), FRotator::ZeroRotator);
		AddBox(P + FVector(0.f, 0.f, 10.f), FVector(60.f, Wd * 0.5f, 14.f), FRotator::ZeroRotator);
		for (int32 s = -1; s <= 1; s += 2)
		{
			if (!B.bServer)
			{
				if (UStaticMesh* Fern = WRTreeKit::Mesh(GetTransientPackage(), TEXT("fern"), (i + s + 4) % 4))
				{
					UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
					C->SetStaticMesh(Fern);
					C->SetupAttachment(RootComponent);
					C->SetRelativeLocation(B.GroundL(W + FVector(0.f, (Wd * 0.5f + 70.f + 60.f * FaHash(i, s + 5)) * s, 0.f)));
					C->SetRelativeRotation(FRotator(0.f, 37.f * i, 0.f));
					C->SetRelativeScale3D(FVector((90.f + 40.f * FaHash(i, s)) / FMath::Max(1.f, WRTreeKit::ReferenceHeight(TEXT("fern")))));
					C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					C->RegisterComponent();
					LookComps.Add(C);
				}
			}
		}
	}
	// Low ferns in the moss round the clearing's rim (the middle stays open: the fight's floor).
	for (int32 i = 0; i < 70 && !B.bServer; ++i)
	{
		const float Ang = 360.f * FaHash(i, 1);
		const float R = Edge * (0.72f + 0.25f * FaHash(i, 2));
		if (UStaticMesh* Fern = WRTreeKit::Mesh(GetTransientPackage(), TEXT("fern"), i % 4))
		{
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
			C->SetStaticMesh(Fern);
			C->SetupAttachment(RootComponent);
			C->SetRelativeLocation(B.GroundL(Centre + FRotator(0.f, Ang, 0.f).Vector() * R));
			C->SetRelativeRotation(FRotator(0.f, Ang * 3.f, 0.f));
			C->SetRelativeScale3D(FVector((70.f + 50.f * FaHash(i, 4)) / FMath::Max(1.f, WRTreeKit::ReferenceHeight(TEXT("fern")))));
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->SetCullDistance(15000.f);
			C->RegisterComponent();
			LookComps.Add(C);
		}
	}
	AddBlocks(B.Day, false);
	AddBlocks(B.Night, true);
	// The pale foxfire haze over the clearing at night, and a cold green light low over it.
	if (!B.bServer)
	{
		USceneComponent* Haze = NewObject<USceneComponent>(this);
		Haze->SetupAttachment(RootComponent);
		Haze->SetWorldLocation(B.T.TransformPosition(B.GroundL(Centre, 300.f)));
		Haze->RegisterComponent();
		LookComps.Add(Haze);
		if (USceneComponent* Fx = WRFx::Attach(Haze, TEXT("sylvaroth.foxfire_haze"), NAME_None, Edge / 1000.f)) { LookComps.Add(Fx); NightOnly.Add(Fx); }
		if (UPointLightComponent* L = FaLight(this, Haze, FVector(0.f, 0.f, 600.f), FaRgb(127, 224, 122), 0.f, Edge))
		{
			NightLights.Add(L);
			NightPower.Add(6000.f);
		}
	}
}

void AWRForestArenaScenery::BuildClearCut()
{
	using namespace WRForestArenaImpl;
	FaBuild B(this);
	FVector Centre;
	float Radius = 12500.f;
	TArray<FVector> P;
	FWRForestBoss::Arena(TEXT("old_gnarl"), Centre, Radius, P);
	// 340 stumps 0.5-1 m high across the 250 m hole in the canopy (the newcomers' clear-cut), 40 with axes still in
	// them; each blocks: cover from the boulder throw. Instanced (four stump variants), collision per instance.
	const int32 Count = FMath::RoundToInt(WRTune(TEXT("bosses.old_gnarl.stumps"), 340.f));
	const float Ref = FMath::Max(1.f, WRTreeKit::ReferenceHeight(TEXT("stump")));
	TArray<UInstancedStaticMeshComponent*> Sets;
	for (int32 v = 0; v < 4; ++v)
	{
		UInstancedStaticMeshComponent* ISM = NewObject<UInstancedStaticMeshComponent>(this);
		ISM->SetupAttachment(RootComponent);
		if (UStaticMesh* Mesh = WRTreeKit::Mesh(GetTransientPackage(), TEXT("stump"), v)) ISM->SetStaticMesh(Mesh);
		ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ISM->SetCanEverAffectNavigation(false);
		ISM->SetCullDistances(0, 30000);
		ISM->RegisterComponent();
		LookComps.Add(ISM);
		Sets.Add(ISM);
	}
	const float Clear = WRTune(TEXT("bosses.old_gnarl.clearMiddle"), 1500.f);
	int32 Placed = 0, Axes = 0;
	for (int32 i = 0; Placed < Count && i < Count * 3; ++i)
	{
		// A sunflower spiral (golden angle) jittered: even cover, no rows, no clumps.
		const float Ang = i * 137.508f + 40.f * (FaHash(i, 1) - 0.5f);
		const float R = Clear + (Radius - Clear) * FMath::Sqrt((i + 0.5f) / (Count * 1.15f)) + 300.f * (FaHash(i, 2) - 0.5f);
		if (R > Radius - 200.f) continue;
		const FVector W = Centre + FRotator(0.f, Ang, 0.f).Vector() * R;
		const FVector G = B.GroundL(W, -8.f);
		const float H = 50.f + 50.f * FaHash(i, 3);
		const int32 Var = i % 4;
		const float Sc = H / Ref;
		const FTransform Inst(FRotator(0.f, 360.f * FaHash(i, 4), 0.f), G, FVector(Sc * (0.9f + 0.4f * FaHash(i, 5)), Sc * (0.9f + 0.4f * FaHash(i, 6)), Sc));
		if (!B.bServer && Sets[Var]->GetStaticMesh()) Sets[Var]->AddInstance(Inst);
		const float SR = WRTreeKit::ReferenceRadius(TEXT("stump")) * Sc * 1.1f;
		AddBox(G + FVector(0.f, 0.f, H * 0.5f), FVector(SR, SR, H * 0.5f), FRotator(0.f, 45.f * (i % 2), 0.f));
		++Placed;
		if (Axes < 40 && FaHash(i, 9) < 0.14f)
		{
			// A felling axe still bitten into the stump's cut top, its haft up at an angle (left when the camp fled).
			++Axes;
			const FRotator Lean(-55.f + 20.f * FaHash(i, 10), 360.f * FaHash(i, 11), 0.f);
			const FVector Top = G + FVector(0.f, 0.f, H - 4.f);
			B.K.Rod(FaTimber(1.1f), Top, Top + Lean.Vector() * 85.f, 5.5f);
			B.K.Add(FaIron(), FVector(18.f, 4.f, 14.f), Top - Lean.Vector() * 3.f, Lean);
		}
	}
	// The Woodcutters' Camp on the east edge: a bark-roofed lean-to on a log frame, a chopping block with a hatchet in
	// it, split logs stacked in a rick, a cold fire ring, a saw-horse with the two-man saw left on it.
	FVector CampW = Centre + FVector(1.f, 0.f, 0.f) * (Radius - 900.f);
	if (const FWRForestData::FSite* Site = FWRForestData::Get().FindSite(TEXT("woodcutters_camp")); Site && FVector::Dist2D(Site->At, Centre) < Radius + 3000.f)
	{
		CampW = Site->At;   // the camp the land laid out on the clear-cut's edge (forest.json)
	}
	const FVector Camp = B.GroundL(CampW);
	const float CY = 180.f;
	const FRotator CR(0.f, CY, 0.f);
	const auto C = [&](float X, float Y, float Z) { return Camp + CR.RotateVector(FVector(X, Y, Z)); };
	for (int32 s = -1; s <= 1; s += 2)
	{
		B.K.Rod(FaTimber(), C(0.f, 180.f * s, 0.f), C(0.f, 180.f * s, 240.f), 16.f);
		B.K.Rod(FaTimber(0.9f), C(-260.f, 180.f * s, 0.f), C(0.f, 180.f * s, 240.f), 12.f);
		AddBox(C(0.f, 180.f * s, 120.f), FVector(10.f, 10.f, 120.f), CR);
	}
	B.K.Rod(FaTimber(), C(0.f, -200.f, 240.f), C(0.f, 200.f, 240.f), 14.f);
	for (int32 k = 0; k < 8; ++k)
	{
		const float U = (k + 0.5f) / 8.f;
		B.K.Add(FaBark(0.8f + 0.25f * FaHash(k, 41)), FVector(280.f, 52.f, 6.f), C(-130.f, -180.f + 360.f * U, 120.f), FRotator(-43.f, CY, 0.f));
	}
	B.K.Add(FaSurf::Cloth(FaRgb(110, 92, 66) * 0.45f), FVector(220.f, 300.f, 12.f), C(-110.f, 0.f, 6.f), CR);
	B.K.Disc(FaPaleWood(0.9f), C(170.f, -260.f, 35.f), FVector::UpVector, 80.f, 70.f);
	B.K.Disc(FaPaleWood(1.15f), C(170.f, -260.f, 71.f), FVector::UpVector, 74.f, 3.f);
	B.K.Rod(FaTimber(1.1f), C(170.f, -260.f, 72.f), C(200.f, -280.f, 130.f), 4.f);
	B.K.Add(FaIron(), FVector(16.f, 4.f, 12.f), C(168.f, -258.f, 74.f), FRotator(-60.f, CY + 30.f, 0.f));
	AddBox(C(170.f, -260.f, 35.f), FVector(40.f, 40.f, 35.f), CR);
	for (int32 r = 0; r < 4; ++r)
	{
		for (int32 k = 0; k < 7 - r; ++k)
		{
			// Split logs in a rick against the lean-to's side: each course one log fewer, set in the gaps of the one below.
			const float Along = k * 34.f - 100.f + r * 17.f;
			B.K.Rod(FaPaleWood(0.85f + 0.15f * FaHash(r, k)), C(-60.f, 280.f + Along, 18.f + 30.f * r), C(50.f, 280.f + Along, 18.f + 30.f * r), 15.f);
		}
	}
	AddBox(C(-5.f, 280.f, 70.f), FVector(60.f, 130.f, 70.f), CR);
	for (int32 k = 0; k < 9; ++k)
	{
		const float A = 40.f * k;
		B.K.Add(FaStone(0.7f + 0.3f * FaHash(k, 61)), FVector(34.f, 26.f, 22.f), C(260.f, 120.f, 8.f) + FRotator(0.f, A, 0.f).Vector() * 70.f, FRotator(0.f, A, 0.f));
	}
	B.K.Disc(FaSurf::Cloth(FLinearColor(0.02f, 0.018f, 0.016f)), C(260.f, 120.f, 3.f), FVector::UpVector, 110.f, 4.f);
	for (int32 s = -1; s <= 1; s += 2)
	{
		B.K.Rod(FaTimber(), C(80.f + 50.f * s, 420.f, 0.f), C(80.f - 10.f * s, 420.f, 95.f), 8.f);
		B.K.Rod(FaTimber(), C(80.f + 50.f * s, 520.f, 0.f), C(80.f - 10.f * s, 520.f, 95.f), 8.f);
	}
	B.K.Rod(FaTimber(), C(80.f, 400.f, 90.f), C(80.f, 540.f, 90.f), 10.f);
	B.K.Add(FaIron(), FVector(220.f, 2.f, 20.f), C(80.f, 470.f, 108.f), FRotator(0.f, CY + 90.f, 0.f));
	B.K.Rod(FaTimber(1.1f), C(-30.f, 470.f, 108.f), C(-30.f, 470.f, 140.f), 4.f);
	B.K.Rod(FaTimber(1.1f), C(190.f, 470.f, 108.f), C(190.f, 470.f, 140.f), 4.f);
	AddBlocks(B.Day, false);
}

void AWRForestArenaScenery::BuildPaperHollow()
{
	using namespace WRForestArenaImpl;
	FaBuild B(this);
	FVector Base;
	TArray<FVector> Combs;
	if (!FWRForestBoss::HiveLayout(Base, FloorZ, Combs))
	{
		return;
	}
	const FVector Foot = B.GroundL(Base);
	const FVector FootW = B.T.TransformPosition(Foot);
	const float OakH = WRTune(TEXT("bosses.bramblequeen.oakHeight"), 5000.f);
	const float TrunkR = FaTrunkR(TEXT("dead_giant"), OakH);
	GalleryIn = WRTune(TEXT("bosses.bramblequeen.galleryIn"), 900.f);
	GalleryOut = WRTune(TEXT("bosses.bramblequeen.galleryOut"), 1300.f);
	const float ShellR = WRTune(TEXT("bosses.bramblequeen.shellRadius"), 1380.f);
	const float Top = WRTune(TEXT("bosses.bramblequeen.hiveHeight"), 2100.f);
	// The dead oak, 50 m, its bark grey: the hive is built round its lower 20 m.
	AddTree(TEXT("dead_giant"), 1, FootW - FVector(0.f, 0.f, 30.f), OakH, 30.f);
	// The shell: a ribbed lantern of grey paper, 16 ribs, banded in the strata the wasps chewed it in, widest a third of
	// the way up and closing on the trunk at the top. Three doorways at the ground (where the earth floor is).
	const int32 Ribs = 16, Rings = 12;
	const auto Radius = [&](float T) { return FMath::Lerp(ShellR, TrunkR + 60.f, FMath::SmoothStep(0.72f, 1.f, T)) * (1.f + 0.07f * FMath::Sin(T * PI * 1.2f)); };
	const auto Door = [&](int32 Rib, int32 Ring) { return Ring < 2 && (Rib == 0 || Rib == 5 || Rib == 11); };
	for (int32 k = 0; k < Rings; ++k)
	{
		const float T0 = k / float(Rings), T1 = (k + 1) / float(Rings);
		const float Z0 = Top * T0, Z1 = Top * T1, R0 = Radius(T0), R1 = Radius(T1);
		for (int32 i = 0; i < Ribs; ++i)
		{
			if (Door(i, k)) continue;
			const float Ang = 360.f * (i + 0.5f) / Ribs;
			const FVector D = FRotator(0.f, Ang, 0.f).Vector();
			const FVector P0 = Foot + D * R0 + FVector(0.f, 0.f, Z0), P1 = Foot + D * R1 + FVector(0.f, 0.f, Z1);
			const float W = 2.f * PI * FMath::Max(R0, R1) / Ribs + 6.f;
			const FVector Mid = (P0 + P1) * 0.5f;
			const FRotator Rot = FRotationMatrix::MakeFromZX(P1 - P0, D).Rotator();
			B.K.Add(FaPaper(k + (i % 2)), FVector(14.f, W, (P1 - P0).Size() + 4.f), Mid, Rot);
			AddBox(Mid, FVector(10.f, W * 0.5f, (P1 - P0).Size() * 0.5f + 2.f), Rot);
		}
	}
	for (int32 i = 0; i < Ribs; ++i)
	{
		const float Ang = 360.f * i / Ribs;
		const FVector D = FRotator(0.f, Ang, 0.f).Vector();
		TArray<FVector> Pts;
		for (int32 k = 0; k <= Rings; ++k) { const float T = k / float(Rings); Pts.Add(Foot + D * (Radius(T) + 10.f) + FVector(0.f, 0.f, Top * T)); }
		B.M.Tube(Pts, 18.f, 6, FaPaperC() * 0.75f, 0.95f);
	}
	// The doorways' lips: thick chewed-paper rims round the three openings.
	for (const int32 Rib : { 0, 5, 11 })
	{
		const float Ang = 360.f * (Rib + 0.5f) / Ribs;
		const FVector D = FRotator(0.f, Ang, 0.f).Vector();
		const FVector S = FVector::CrossProduct(D, FVector::UpVector);
		const float Half = PI * ShellR / Ribs;
		B.M.Tube({ Foot + D * ShellR - S * Half, Foot + D * ShellR - S * Half + FVector(0.f, 0.f, Top * 2.f / Rings), Foot + D * ShellR + S * Half + FVector(0.f, 0.f, Top * 2.f / Rings), Foot + D * ShellR + S * Half }, 24.f, 6, FaPaperC() * 0.65f, 0.95f);
	}
	// The two paper galleries (floor 2 and 3): rings of chewed paper from the open middle to the shell, a lip at the
	// inner edge chewed through in places; each floor has a hole over the ramp that comes up to it.
	const int32 Segs = 24;
	const float Ramp = 70.f;
	for (int32 f = 1; f < FloorZ.Num(); ++f)
	{
		const float A0 = 30.f + 150.f * (f - 1);   // the ramp up to this floor starts here on the floor below
		for (int32 i = 0; i < Segs; ++i)
		{
			const float Ang = 360.f * (i + 0.5f) / Segs;
			const float Rel = FMath::Fmod(Ang - A0 + 720.f, 360.f);
			if (Rel > 4.f && Rel < Ramp - 2.f) continue;   // the stairwell over the ramp
			const FVector D = FRotator(0.f, Ang, 0.f).Vector();
			const float Rm = (GalleryIn + GalleryOut) * 0.5f;
			const FVector P = Foot + D * Rm + FVector(0.f, 0.f, FloorZ[f]);
			const float W = 2.f * PI * GalleryOut / Segs + 8.f;
			B.K.Add(FaPaper(f + i), FVector(GalleryOut - GalleryIn + 40.f, W, 26.f), P - FVector(0.f, 0.f, 13.f), FRotator(0.f, Ang, 0.f));
			AddBox(P - FVector(0.f, 0.f, 13.f), FVector((GalleryOut - GalleryIn) * 0.5f + 30.f, W * 0.5f, 13.f), FRotator(0.f, Ang, 0.f));
			if (i % 3 != 1)
			{
				B.K.Add(FaPaper(f + i + 1), FVector(18.f, W * 0.95f, 45.f), Foot + D * (GalleryIn + 9.f) + FVector(0.f, 0.f, FloorZ[f] + 22.f), FRotator(0.f, Ang, 0.f));
				AddBox(Foot + D * (GalleryIn + 9.f) + FVector(0.f, 0.f, FloorZ[f] + 22.f), FVector(9.f, W * 0.47f, 22.f), FRotator(0.f, Ang, 0.f));
			}
			// The underside's cell texture: hanging paper scallops.
			if (i % 2 == 0) B.K.Add(FaPaper(f + 2), FVector(GalleryOut - GalleryIn - 80.f, W * 0.7f, 18.f), P - FVector(0.f, 0.f, 36.f), FRotator(0.f, Ang + 4.f, 0.f));
		}
		// The ramp from the floor below up to this one: chewed-paper planking along the gallery's middle line.
		const float Z0 = FloorZ[f - 1], Z1 = FloorZ[f];
		const int32 Steps = 10;
		for (int32 s = 0; s < Steps; ++s)
		{
			const float U0 = s / float(Steps), U1 = (s + 1) / float(Steps);
			const float Rm = (GalleryIn + GalleryOut) * 0.5f;
			const FVector Q0 = Foot + FRotator(0.f, A0 + Ramp * U0, 0.f).Vector() * Rm + FVector(0.f, 0.f, FMath::Lerp(Z0, Z1, U0));
			const FVector Q1 = Foot + FRotator(0.f, A0 + Ramp * U1, 0.f).Vector() * Rm + FVector(0.f, 0.f, FMath::Lerp(Z0, Z1, U1));
			const FVector Mid = (Q0 + Q1) * 0.5f;
			const FRotator Rot = (Q1 - Q0).Rotation();
			const float Len = (Q1 - Q0).Size() + 6.f;
			B.K.Add(FaPaper(s), FVector(Len, (GalleryOut - GalleryIn) * 0.75f, 20.f), Mid, Rot);
			AddBox(Mid, FVector(Len * 0.5f, (GalleryOut - GalleryIn) * 0.37f, 10.f), Rot);
		}
	}
	// The dome's underside: a paper cap from the shell's top onto the trunk (seen from the top gallery).
	B.K.Disc(FaPaper(2), Foot + FVector(0.f, 0.f, Top - 30.f), FVector::UpVector, (TrunkR + 120.f) * 2.f, 40.f);
	// A water butt on each floor against the shell (Grovekin rain barrels, filled through the roof by a paper gutter):
	// standing at one washes a wasp's mark off (UWRForestArenas::StepButts).
	Butts.Reset();
	for (int32 f = 0; f < FloorZ.Num(); ++f)
	{
		const float Ang = 200.f + 60.f * f;
		const FVector D = FRotator(0.f, Ang, 0.f).Vector();
		FVector At = Foot + D * (GalleryOut - 90.f) + FVector(0.f, 0.f, FloorZ[f]);
		if (f == 0) At = B.GroundL(B.T.TransformPosition(At));
		FaBarrel(B.K, At, Ang);
		AddBox(At + FVector(0.f, 0.f, 52.f), FVector(45.f, 45.f, 52.f), FRotator(0.f, Ang, 0.f));
		Butts.Add(B.T.TransformPosition(At + FVector(0.f, 0.f, 60.f)));
		B.K.Rod(FaSurf::Cloth(FaPaperC() * 0.7f), At + FVector(0.f, 0.f, 105.f), At + D * 60.f + FVector(0.f, 0.f, 420.f), 10.f);
	}
	// The grubs' glow: amber light through the cells along the shell on every floor (always a little, more at night).
	for (int32 f = 0; f < FloorZ.Num(); ++f)
	{
		for (int32 g = 0; g < 8; ++g)
		{
			const float Ang = 45.f * g + 20.f * f;
			const FVector D = FRotator(0.f, Ang, 0.f).Vector();
			const FVector P = Foot + D * (ShellR - 30.f) + FVector(0.f, 0.f, FloorZ[f] + 120.f + 160.f * FaHash(f, g));
			B.K.Hex(FaGrub(1.2f), P.X, P.Y, P.Z, P.Z + 40.f, 24.f, Ang);
			B.N.Hex(FaGrub(2.5f), P.X - D.X * 6.f, P.Y - D.Y * 6.f, P.Z + 5.f, P.Z + 35.f, 18.f, Ang);
		}
		if (UPointLightComponent* L = FaLight(this, nullptr, Foot + FVector(0.f, 0.f, FloorZ[f] + 260.f) + FRotator(0.f, 90.f * f, 0.f).Vector() * (GalleryIn + 150.f), FaRgb(224, 160, 64), 600.f, 1500.f))
		{
			NightLights.Add(L);
			NightPower.Add(2200.f);
		}
	}
	if (UProceduralMeshComponent* Shell = FaSpawnMesh(this, nullptr, B.M, LookComps)) (void)Shell;
	AddBlocks(B.Day, false);
	AddBlocks(B.Night, true);
	// The hum of the hive round it (a loop the scenery holds; the wasps themselves are creatures).
	if (!B.bServer)
	{
		if (USceneComponent* Fx = WRFx::Attach(RootComponent, TEXT("queen.hive_motes"), NAME_None, ShellR / 1000.f))
		{
			Fx->SetRelativeLocation(Foot + FVector(0.f, 0.f, Top * 0.5f));
			LookComps.Add(Fx);
		}
	}
}

void AWRForestArenaScenery::BuildBower()
{
	using namespace WRForestArenaImpl;
	FaBuild B(this);
	TArray<FVector> Oaks, WA, WB;
	TArray<FIntPoint> WT;
	if (!FWRForestBoss::BowerLayout(Oaks, WA, WB, WT))
	{
		return;
	}
	const float OakH = WRTune(TEXT("bosses.grey_matron.oakHeight"), 7000.f);
	const float TrunkR = FaTrunkR(TEXT("bower_oak"), OakH);
	FVector Centre = FVector::ZeroVector;
	for (const FVector& O : Oaks) Centre += O / Oaks.Num();
	for (int32 i = 0; i < Oaks.Num(); ++i)
	{
		FVector G;
		FaGround(GetWorld(), Oaks[i], G, 4000.f, 9000.f, this);
		// The tallest (oak 0) is a little taller: the larder hollow is in its foot.
		AddTree(TEXT("bower_oak"), i % 4, G - FVector(0.f, 0.f, 30.f), OakH * (i == 0 ? 1.08f : 0.95f + 0.08f * FaHash(i, 2)), 60.f * i + 17.f);
		const FVector Foot = B.Local(G);
		// Every walkway end at this oak gets its platform: a ring deck round the trunk on four brackets, a rope rail.
		TArray<float> Decks;
		for (int32 w = 0; w < WA.Num(); ++w)
		{
			if (WT[w].X == i) Decks.AddUnique(FMath::RoundToFloat(WA[w].Z));
			if (WT[w].Y == i) Decks.AddUnique(FMath::RoundToFloat(WB[w].Z));
		}
		const float PlatR = TrunkR + WRTune(TEXT("bosses.grey_matron.platformWidth"), 260.f);
		for (const float DeckZ : Decks)
		{
			const FVector C = B.Local(FVector(G.X, G.Y, DeckZ));
			const int32 Planks = 20;
			for (int32 p = 0; p < Planks; ++p)
			{
				const float Ang = 360.f * (p + 0.5f) / Planks;
				const FVector D = FRotator(0.f, Ang, 0.f).Vector();
				const float W = 2.f * PI * PlatR / Planks + 4.f;
				B.K.Add(FaPlank(0.85f + 0.3f * FaHash(p, i)), FVector(PlatR - TrunkR + 30.f, W, 7.f), C + D * ((PlatR + TrunkR) * 0.5f), FRotator(0.f, Ang, 0.f));
				AddBox(C + D * ((PlatR + TrunkR) * 0.5f) - FVector(0.f, 0.f, 2.f), FVector((PlatR - TrunkR) * 0.5f + 15.f, W * 0.5f, 5.f), FRotator(0.f, Ang, 0.f));
				if (p % 2 == 0) B.K.Rod(FaRope(), C + D * (PlatR - 10.f), C + D * (PlatR - 10.f) + FVector(0.f, 0.f, 95.f), 4.f);
			}
			for (int32 k = 0; k < 4; ++k)
			{
				const FVector D = FRotator(0.f, 45.f + 90.f * k, 0.f).Vector();
				B.K.Rod(FaTimber(), C + D * (TrunkR - 10.f) - FVector(0.f, 0.f, 160.f), C + D * (PlatR - 30.f) - FVector(0.f, 0.f, 8.f), 14.f);
			}
		}
		// Deer and elk hides hung from the branches like flags (Black Annis' bower), 2-4 a tree, high up.
		for (int32 h = 0; h < 2 + (i % 3); ++h)
		{
			const float Ang = 60.f * i + 110.f * h + 30.f;
			const FVector D = FRotator(0.f, Ang, 0.f).Vector();
			const FVector Top = Foot + D * (TrunkR + 300.f + 250.f * FaHash(i, h)) + FVector(0.f, 0.f, 2600.f + 900.f * FaHash(h, i));
			B.K.Rod(FaRope(), Top + FVector(0.f, 0.f, 120.f), Top, 3.f);
			FaHide(B.K, Top, Ang + 90.f, 110.f + 40.f * FaHash(i, h + 9), 160.f + 60.f * FaHash(h, 3), FaHash(i, h));
		}
		// Green eyeshine at night: pairs in the dark of the trunks' forks (Tick slides some along their branches).
		for (int32 e = 0; e < 2; ++e)
		{
			const float Ang = 60.f * i + 180.f * e + 15.f;
			const FVector D = FRotator(0.f, Ang, 0.f).Vector();
			const FVector S = FVector::CrossProduct(D, FVector::UpVector);
			const FVector P = Foot + D * (TrunkR + 40.f) + FVector(0.f, 0.f, 1800.f + 700.f * FaHash(i, e + 4));
			TArray<FWRLookBlock> Eye;
			FaKit EK(GetActorTransform(), Eye);
			EK.Disc(FaEyeshine(4.f), P + S * 7.f, D, 5.f, 2.f);
			EK.Disc(FaEyeshine(4.f), P - S * 7.f, D, 5.f, 2.f);
			if (USceneComponent* F = AddBlocks(Eye, true)) { if (e == 0) Loops.Add(F); }
		}
	}
	// A peg stair spiralling up oak 3 to its platform: heartwood pegs driven into the bark (the Grovekin's way up; the
	// walkways can be fought on and fallen from).
	if (Oaks.IsValidIndex(3))
	{
		FVector G;
		FaGround(GetWorld(), Oaks[3], G, 4000.f, 9000.f, this);
		float DeckZ = G.Z + 1500.f;
		for (int32 w = 0; w < WA.Num(); ++w) { if (WT[w].X == 3) DeckZ = WA[w].Z; else if (WT[w].Y == 3) DeckZ = WB[w].Z; }
		const int32 Pegs = FMath::Max(6, FMath::RoundToInt((DeckZ - G.Z) / 45.f));
		for (int32 p = 0; p < Pegs; ++p)
		{
			const float Ang = 200.f + p * 17.f;
			const FVector D = FRotator(0.f, Ang, 0.f).Vector();
			const FVector P = B.Local(G) + D * (TrunkR + 55.f) + FVector(0.f, 0.f, 45.f * (p + 1) - 20.f);
			B.K.Add(FaTimber(0.9f + 0.2f * FaHash(p, 8)), FVector(110.f, 46.f, 12.f), P, FRotator(0.f, Ang, 0.f));
			AddBox(P, FVector(55.f, 30.f, 6.f), FRotator(0.f, Ang, 0.f));
		}
	}
	// The larder hollow at the foot of the tallest oak: a split in the bark the height of a door, black inside (the
	// curtain is the server's AWRForestProp "larder", which falls when she dies).
	if (Oaks.IsValidIndex(0))
	{
		FVector G;
		FaGround(GetWorld(), Oaks[0], G, 4000.f, 9000.f, this);
		const FVector Face = (Centre - G).GetSafeNormal2D();
		const FVector Mouth = B.Local(G) + Face * (TrunkR * 1.08f);
		const FRotator FR(0.f, Face.Rotation().Yaw, 0.f);
		B.K.Add(FaSurf::Stone(FLinearColor(0.008f, 0.007f, 0.006f), 1.f), FVector(60.f, 220.f, 330.f), Mouth - Face * 30.f + FVector(0.f, 0.f, 165.f), FR);
		B.K.Add(FaBark(0.7f), FVector(70.f, 40.f, 360.f), Mouth + FR.RotateVector(FVector(0.f, -130.f, 180.f)), FRotator(0.f, FR.Yaw, -4.f));
		B.K.Add(FaBark(0.7f), FVector(70.f, 40.f, 360.f), Mouth + FR.RotateVector(FVector(0.f, 130.f, 180.f)), FRotator(0.f, FR.Yaw, 4.f));
		// Bones by the threshold: what she brings back.
		for (int32 k = 0; k < 5; ++k)
		{
			B.K.Rod(FaSurf::Stone(FaRgb(216, 210, 196) * 0.7f, 0.8f), Mouth + Face * (60.f + 30.f * k) + FVector(0.f, 40.f * k - 80.f, 6.f), Mouth + Face * (110.f + 30.f * k) + FVector(20.f, 40.f * k - 60.f, 8.f), 6.f);
		}
	}
	AddBlocks(B.Day, false);
}

void AWRForestArenaScenery::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (GetNetMode() == NM_DedicatedServer)
	{
		SetActorTickEnabled(false);
		return;
	}
	LightClock += DeltaSeconds;
	// The night look (lights, glows, eyeshine), checked once a second.
	if (LightClock >= 1.f || NightShown < 0)
	{
		LightClock = 0.f;
		// Overcast closes the Elder Canopy to near-dark by day (DC 2.9): an arena standing in F5 shows its night look
		// (foxfire haze, glows) as at dusk, like the rest of F5's places (UWRForestWorld::StepLights).
		const FWRWeatherKind* Weather = UWRWeatherSubsystem::ActiveKind(this, FName(TEXT("forest")));
		const bool bNight = AWRGameState::IsNightIn(this)
			|| (Weather && Weather->Id == TEXT("overcast") && WRForest::ZoneAt(this, GetActorLocation()) == TCHAR('5'));
		if (int32(bNight) != NightShown)
		{
			NightShown = bNight ? 1 : 0;
			for (USceneComponent* C : NightOnly) if (C) C->SetVisibility(bNight, true);
			for (int32 i = 0; i < NightLights.Num(); ++i)
			{
				if (NightLights[i]) NightLights[i]->SetIntensity(bNight ? NightPower[i] : (Kind == TEXT("paper_hollow") ? NightPower[i] * 0.25f : 0.f));
			}
		}
		if (Kind == TEXT("four_hearts"))
		{
			// The heart-trees' sap: dim by day, bright at night, burning bright in the Bloom's warning and the Bloom.
			const int32 Level = WRForestHazards::HeartsBright(this) ? 2 : bNight ? 1 : 0;
			if (Level != HeartGlowShown)
			{
				HeartGlowShown = Level;
				const float Strength = Level == 2 ? WRTune(TEXT("bosses.sylvaroth.sapBloom"), 9.f) : Level == 1 ? WRTune(TEXT("bosses.sylvaroth.sapNight"), 5.f) : WRTune(TEXT("bosses.sylvaroth.sapDay"), 1.5f);
				for (UStaticMeshComponent* Tree : HeartTrees)
				{
					if (!Tree) continue;
					if (UMaterialInstanceDynamic* M = WRTreeKit::GlowMaterial(Tree, WRForestArenaImpl::FaRgb(208, 138, 30), Strength)) Tree->SetMaterialByName(TEXT("Glow"), M);
				}
			}
		}
	}
	// The Bower's eyeshine pairs drift along their branch and blink (something is watching from the forks).
	for (int32 i = 0; i < Loops.Num(); ++i)
	{
		USceneComponent* C = Loops[i];
		if (!C || NightShown != 1) continue;
		const float T = GetWorld()->GetTimeSeconds() * 0.15f + i * 1.7f;
		C->SetRelativeLocation(FVector(FMath::Sin(T) * 120.f, FMath::Cos(T * 0.7f) * 60.f, 0.f));
		C->SetVisibility(FMath::Frac(T * 1.3f) > 0.06f, true);
	}
}

// ================================================================================================ UWRForestArenas

UWRForestArenas* UWRForestArenas::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UWRForestArenas>() : nullptr;
}

bool UWRForestArenas::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UWRForestArenas::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	const FWRForestData& D = FWRForestData::Get();
	// Only where the forest is a live land with its arenas laid out (forest.json); nothing runs otherwise.
	bool bAny = false;
	for (const FName Id : { FName(TEXT("sylvaroth")), FName(TEXT("old_gnarl")), FName(TEXT("bramblequeen")), FName(TEXT("grey_matron")) })
	{
		FVector C;
		float R = 0.f;
		TArray<FVector> P;
		bAny |= FWRForestBoss::Arena(Id, C, R, P) && WRForest::Contains(C);
	}
	bActive = D.bLoaded && bAny;
	if (!bActive)
	{
		return;
	}
	if (InWorld.GetNetMode() != NM_Client)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		State = InWorld.SpawnActor<AWRForestArenaState>(AWRForestArenaState::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
		BlightHandle = UWRForestWorld::OnBlightLanded().AddUObject(this, &UWRForestArenas::OnBlight);
	}
	UE_LOG(LogWildreach, Log, TEXT("FOREST arenas active (%s)"), InWorld.GetNetMode() == NM_Client ? TEXT("client") : TEXT("server"));
}

void UWRForestArenas::Deinitialize()
{
	if (BlightHandle.IsValid()) UWRForestWorld::OnBlightLanded().Remove(BlightHandle);
	BlightHandle.Reset();
	Super::Deinitialize();
}

void UWRForestArenas::OnBlight(UWorld* World, const FVector& At, AActor* Thrower)
{
	FWRForestBoss::BlightLanded(World, At, Thrower);
}

void UWRForestArenas::BuildScenery()
{
	// Each landmark is built once its ground is in on this machine (a trace under its middle hits).
	UWorld* World = GetWorld();
	static const TPair<const TCHAR*, const TCHAR*> Rows[] = { { TEXT("four_hearts"), TEXT("sylvaroth") }, { TEXT("clear_cut"), TEXT("old_gnarl") }, { TEXT("paper_hollow"), TEXT("bramblequeen") }, { TEXT("bower"), TEXT("grey_matron") } };
	int32 Done = 0;
	for (const TPair<const TCHAR*, const TCHAR*>& R : Rows)
	{
		const FName Kind(R.Key);
		if (Built.Contains(Kind)) { ++Done; continue; }
		FVector C;
		float Rad = 0.f;
		TArray<FVector> P;
		if (!FWRForestBoss::Arena(FName(R.Value), C, Rad, P) || !WRForest::Contains(C)) { Built.Add(Kind); ++Done; continue; }
		FVector G;
		if (!WRForestArenaImpl::FaGround(World, C, G, 20000.f, 40000.f)) continue;
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.bDeferConstruction = true;
		if (AWRForestArenaScenery* S = World->SpawnActor<AWRForestArenaScenery>(AWRForestArenaScenery::StaticClass(), FTransform(FRotator::ZeroRotator, G), Params))
		{
			S->Kind = Kind;
			S->FinishSpawning(FTransform(FRotator::ZeroRotator, G));
			S->Build();
			Scenery.Add(S);
		}
		Built.Add(Kind);
		++Done;
	}
	bScenery = Done >= int32(UE_ARRAY_COUNT(Rows));
}

bool UWRForestArenas::Place(FName Arena)
{
	using namespace WRForestArenaImpl;
	UWorld* World = GetWorld();
	FVector Centre;
	float Radius = 0.f;
	TArray<FVector> Points;
	if (!FWRForestBoss::Arena(Arena, Centre, Radius, Points) || !FWRCreatureRegistry::Get().Find(Arena))
	{
		return true;   // no arena or no boss row: nothing to place (and nothing to retry)
	}
	bool bNear = false;
	for (TActorIterator<AWRCharacter> It(World); It && !bNear; ++It) bNear = It->IsAlive() && FVector::Dist2D(It->GetActorLocation(), Centre) < WRTune(TEXT("bosses.forest.placeRadius"), 30000.f);
	FVector G;
	if (!bNear || !FaGround(World, Centre, G, 20000.f, 40000.f))
	{
		return false;
	}
	for (TActorIterator<AWRCreatureSpawner> It(World); It; ++It)
	{
		if (It->CreatureId == Arena) return true;   // placed already
	}
	const auto SpawnProp = [World](FName Kind, const FVector& At, float Size, float Yaw, int32 Index) -> AWRForestProp*
	{
		const FTransform T(FRotator(0.f, Yaw, 0.f), At);
		AWRForestProp* A = World->SpawnActorDeferred<AWRForestProp>(AWRForestProp::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (A)
		{
			A->Kind = Kind;
			A->Size = Size;
			A->Index = Index;
			A->FinishSpawning(T);
		}
		return A;
	};
	// Where the boss stands when it comes (its spawner brings it back server.bossRespawnHours after it falls).
	FVector Start = G;
	if (Arena == TEXT("old_gnarl") && Points.Num() > 0) FaGround(World, Points[0], Start, 20000.f, 40000.f);
	if (Arena == TEXT("bramblequeen"))
	{
		FVector Base;
		TArray<float> Floors;
		TArray<FVector> Combs;
		FWRForestBoss::HiveLayout(Base, Floors, Combs);
		FaGround(World, Base, Start, 20000.f, 40000.f);
		// The layout is over the arena row's height; the hive stands on the ground found here.
		const float Lift = Start.Z - Base.Z;
		const float Dome = WRTune(TEXT("bosses.bramblequeen.hiveHeight"), 2100.f) - 30.f;
		for (int32 i = 0; i < Combs.Num(); ++i)
		{
			const FVector CG = Combs[i] + FVector(0.f, 0.f, Lift);
			// Its stalk runs up to the underside of the floor above (the dome for the top two).
			const float Ceiling = (i < 2 && Floors.IsValidIndex(i + 1) ? Floors[i + 1] - 26.f : Dome) + Start.Z;
			const float Yaw = (FVector(Base.X, Base.Y, 0.f) - FVector(CG.X, CG.Y, 0.f)).Rotation().Yaw;
			const float Size = WRTune(TEXT("bosses.bramblequeen.combSize"), 190.f) * (i == 3 ? 1.3f : 1.f);
			const FTransform T(FRotator(0.f, Yaw, 0.f), CG);
			if (AWRForestProp* Comb = World->SpawnActorDeferred<AWRForestProp>(AWRForestProp::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
			{
				Comb->Kind = TEXT("comb");
				Comb->Size = Size;
				Comb->Index = i;
				Comb->Hang = FMath::Max(0.f, Ceiling - CG.Z);
				Comb->FinishSpawning(T);
			}
		}
		// Out in the open middle of the hive, a floor's height up (she flies).
		Start += FRotator(0.f, 300.f, 0.f).Vector() * (WRTune(TEXT("bosses.bramblequeen.galleryIn"), 900.f) * 0.6f) + FVector(0.f, 0.f, 500.f);
	}
	if (Arena == TEXT("grey_matron"))
	{
		TArray<FVector> Oaks, WA, WB;
		TArray<FIntPoint> WT;
		FWRForestBoss::BowerLayout(Oaks, WA, WB, WT);
		// She comes out of the larder oak's hollow onto the moss (then climbs to her canopy: FWRForestBoss::TickIdle).
		const FVector Larder = Oaks.IsValidIndex(0) ? Oaks[0] + (Centre - Oaks[0]).GetSafeNormal2D() * (FaTrunkR(TEXT("bower_oak"), WRTune(TEXT("bosses.grey_matron.oakHeight"), 7000.f)) + 450.f) : Centre;
		FaGround(World, Larder, Start, 20000.f, 40000.f);
		for (int32 w = 0; w < WA.Num(); ++w)
		{
			// Computed decks are heights over the ground at their oak.
			FVector A = WA[w], B = WB[w];
			FVector GA, GB;
			if (FWRForestData::Get().Walkways.Num() == 0 || !FWRForestData::Get().Walkways.ContainsByPredicate([](const FWRForestData::FWalkway& X) { return X.Arena == TEXT("grey_matron"); }))
			{
				FaGround(World, Oaks[WT[w].X], GA, 20000.f, 40000.f);
				FaGround(World, Oaks[WT[w].Y], GB, 20000.f, 40000.f);
				A.Z += GA.Z - Oaks[WT[w].X].Z;
				B.Z += GB.Z - Oaks[WT[w].Y].Z;
			}
			const FTransform T(FRotator::ZeroRotator, (A + B) * 0.5f);
			if (AWRForestWalkway* Wk = World->SpawnActorDeferred<AWRForestWalkway>(AWRForestWalkway::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
			{
				Wk->EndA = A;
				Wk->EndB = B;
				Wk->TreeA = WT[w].X;
				Wk->TreeB = WT[w].Y;
				Wk->FinishSpawning(T);
			}
		}
		// The hide curtain over the larder hollow at the tallest oak's foot, facing the ring's middle.
		if (Oaks.IsValidIndex(0))
		{
			FVector Mid = FVector::ZeroVector;
			for (const FVector& O : Oaks) Mid += O / Oaks.Num();
			const FVector Face = (Mid - Oaks[0]).GetSafeNormal2D();
			const float OakH = WRTune(TEXT("bosses.grey_matron.oakHeight"), 7000.f) * 1.08f;
			FVector LG;
			FaGround(World, Oaks[0] + Face * (FaTrunkR(TEXT("bower_oak"), OakH) * 1.08f + 10.f), LG, 20000.f, 40000.f);
			SpawnProp(TEXT("larder"), LG, 300.f, Face.Rotation().Yaw, 0);
		}
	}
	if (Arena == TEXT("sylvaroth"))
	{
		FVector C;
		TArray<FVector> Hearts, Stones;
		FWRForestBoss::HeartsLayout(C, Hearts, Stones);
		const float KnotRing = WRTune(TEXT("bosses.sylvaroth.knotRing"), 2500.f);
		const float HeartR = FaTrunkR(TEXT("heart"), WRTune(TEXT("bosses.sylvaroth.heartHeight"), 14000.f));
		for (int32 i = 0; i < Hearts.Num(); ++i)
		{
			FVector HG;
			FaGround(World, Hearts[i], HG, 20000.f, 40000.f);
			const FVector In = (G - HG).GetSafeNormal2D();
			// The heart-tree's waking parts (its amber heart and lower limbs), at its foot facing the clearing.
			SpawnProp(TEXT("heart_tree"), HG, HeartR, In.Rotation().Yaw, i);
			// Its root line: from the buttresses at its foot to the clearing's middle; the knot where it crosses 25 m.
			FVector From, KG;
			FaGround(World, HG + In * (HeartR * 1.5f), From, 20000.f, 40000.f);
			FaGround(World, G - In * KnotRing, KG, 20000.f, 40000.f);
			const FTransform T(FRotator::ZeroRotator, From);
			if (AWRForestRootLine* L = World->SpawnActorDeferred<AWRForestRootLine>(AWRForestRootLine::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
			{
				L->To = G - In * WRTune(TEXT("bosses.sylvaroth.lineEnd"), 900.f);
				L->KnotAt = KG;
				L->Index = i;
				L->FinishSpawning(T);
			}
		}
		// The Nature altar in its stone ring against the west cliff behind the west heart-tree.
		FVector AltarAt = FVector::ZeroVector;
		if (const FWRForestData::FArena* Row = FWRForestData::Get().Arenas.Find(TEXT("sylvaroth")); Row && !Row->Altar.IsNearlyZero()) AltarAt = Row->Altar;
		if (AltarAt.IsNearlyZero() && Hearts.IsValidIndex(3)) AltarAt = Hearts[3] + (Hearts[3] - C).GetSafeNormal2D() * WRTune(TEXT("bosses.sylvaroth.altarBehind"), 2200.f);
		FVector AG;
		if (!AltarAt.IsNearlyZero() && FaGround(World, AltarAt, AG, 20000.f, 40000.f))
		{
			const FTransform T(FRotator(0.f, (G - AG).Rotation().Yaw, 0.f), AG + FVector(0.f, 0.f, 50.f));
			if (AWRBossAltar* Altar = World->SpawnActorDeferred<AWRBossAltar>(AWRBossAltar::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
			{
				Altar->PathId = TEXT("nature");
				Altar->FinishSpawning(T);
			}
		}
	}
	const FTransform Where(FRotator::ZeroRotator, Start + FVector(0.f, 0.f, 50.f));
	if (AWRCreatureSpawner* Spawner = World->SpawnActorDeferred<AWRCreatureSpawner>(AWRCreatureSpawner::StaticClass(), Where, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		Spawner->CreatureId = Arena;
		Spawner->Count = 1;
		Spawner->Radius = 1.f;
		Spawner->RespawnSeconds = WRTune(TEXT("server.bossRespawnHours"), 4.f) * 3600.f;
		Spawner->ActivationRadius = 0.f;
		// The queen flies: her spawner keeps the height it was given.
		Spawner->bTraceGround = Arena != TEXT("bramblequeen");
		Spawner->FinishSpawning(Where);
	}
	UE_LOG(LogWildreach, Log, TEXT("FOREST arena %s placed at %s"), *Arena.ToString(), *G.ToCompactString());
	return true;
}

void UWRForestArenas::SpawnServerPieces()
{
	// (Everything is placed per arena in Place, once its ground is in and a survivor is near.)
	bPieces = true;
}

void UWRForestArenas::StepPlace()
{
	static const FName Ids[] = { TEXT("sylvaroth"), TEXT("old_gnarl"), TEXT("bramblequeen"), TEXT("grey_matron") };
	for (const FName Id : Ids)
	{
		if (Placed.Contains(Id)) continue;
		if (Place(Id)) Placed.Add(Id);
	}
}

void UWRForestArenas::StepButts(float Dt)
{
	// A water butt washes a wasp's mark off whoever stands at it a second (the Paper Hollow has one a floor).
	UWorld* World = GetWorld();
	const float Reach = WRTune(TEXT("bosses.bramblequeen.buttReach"), 170.f);
	TSet<TWeakObjectPtr<AActor>> Seen;
	for (const TWeakObjectPtr<AWRForestArenaScenery>& S : Scenery)
	{
		if (!S.IsValid() || S->Butts.Num() == 0) continue;
		for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
		{
			if (!It->IsAlive() || !WRForest::HasWaspMark(*It)) continue;
			for (const FVector& B : S->Butts)
			{
				if (FVector::Dist(It->GetActorLocation(), B) > Reach + It->GetSimpleCollisionRadius()) continue;
				Seen.Add(*It);
				float& T = AtButt.FindOrAdd(*It);
				T += Dt;
				if (T >= WRTune(TEXT("bosses.bramblequeen.buttSeconds"), 1.f))
				{
					WRForest::ClearWaspMark(*It);
					WRFx::Broadcast(*It, TEXT("fish.splash"), B + FVector(0.f, 0.f, 40.f), FRotator::ZeroRotator, 0.6f, TEXT("water.splash"));
					T = 0.f;
				}
				break;
			}
		}
	}
	for (auto It = AtButt.CreateIterator(); It; ++It) if (!Seen.Contains(It->Key)) It.RemoveCurrent();
}

void UWRForestArenas::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	if (!World || !World->HasBegunPlay())
	{
		return;
	}
	PlaceClock += DeltaTime;
	if (PlaceClock >= 2.f)
	{
		PlaceClock = 0.f;
		if (!bScenery) BuildScenery();
		if (World->GetNetMode() != NM_Client)
		{
			StepPlace();
			const double Hours = WRForestArenaImpl::FaHours(World);
			// The Bower's walkways grow back 4 game hours after she dies (and after a failed fight, 4 hours after the last
			// one fell, so the arena is whole for the next party).
			const float Regrow = WRTune(TEXT("bosses.grey_matron.regrowHours"), 4.f);
			bool bMatronFighting = false;
			for (TActorIterator<AWRBoss> It(World); It; ++It) bMatronFighting |= It->CreatureId == TEXT("grey_matron") && It->IsAlive() && It->bEngaged;
			const bool bAfterDeath = MatronDiedHours >= 0.0 && Hours >= MatronDiedHours + Regrow;
			const bool bAfterFall = LastFallHours >= 0.0 && Hours >= LastFallHours + Regrow && !bMatronFighting;
			if (bAfterDeath || bAfterFall)
			{
				int32 N = 0;
				for (TActorIterator<AWRForestWalkway> It(World); It; ++It) { if (It->bDown) { It->Regrow(); ++N; } }
				if (N > 0) UE_LOG(LogWildreach, Log, TEXT("FOREST the Bower's walkways grow back (%d)"), N);
				MatronDiedHours = -1.0;
				LastFallHours = -1.0;
				for (TActorIterator<AWRForestProp> It(World); It; ++It) if (It->Kind == TEXT("larder")) It->SetState(0);
			}
			// The Four Hearts' resin stops weeping after its day.
			if (ResinUntilHours >= 0.0 && Hours >= ResinUntilHours)
			{
				for (const TWeakObjectPtr<AActor>& N : ResinNodes) if (N.IsValid()) N->Destroy();
				ResinNodes.Reset();
				ResinUntilHours = -1.0;
			}
			// An amber heart that was cut wakes again with its line (phase 3 is still on).
			for (TActorIterator<AWRForestProp> It(World); It; ++It)
			{
				if (It->Kind != TEXT("heart_tree") || It->State != 2) continue;
				for (TActorIterator<AWRForestRootLine> L(World); L; ++L)
				{
					if (L->Index == It->Index && L->IsLive(L->ServerNow()) && FVector::Dist2D(L->GetActorLocation(), It->GetActorLocation()) < 4000.f)
					{
						It->Health = It->MaxHealth;
						It->SetState(1);
					}
				}
			}
		}
	}
	if (World->GetNetMode() != NM_Client)
	{
		ButtClock += DeltaTime;
		if (ButtClock >= 0.25f)
		{
			StepButts(ButtClock);
			ButtClock = 0.f;
		}
	}
}
