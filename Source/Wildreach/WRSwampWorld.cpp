// The Swamp on the map (cooks/biome-swamp.md 2.1-2.8, 2.11, 2.20; cooks/build-biome-swamp.md 2.2-2.4, 2.10): the runtime
// world built from Data/swamp.json. The scenery's looks are WRSwampScenery.cpp; this file places it (all of it on the
// server once the ground is in, each piece on a client as its viewer nears it), spawns the stateful actors (the Coven
// boardwalks and their planks, the caves and the Peat Cellar's wall, the resource nodes, the site caches, the camps'
// starts as a safety net), floods the Eel Hole, draws the water mats, lights the nights and turns the Red Tide's look.
// Numbers: tuning.json "swampWorld". See WRSwampWorld.h.

#include "WRSwampWorld.h"
#include "Wildreach.h"
#include "WRBuildLook.h"
#include "WRBuildLookKit.h"
#include "WRCharacterBase.h"
#include "WRCrafting.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRGameState.h"
#include "WRGameplayTags.h"
#include "WRInventory.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRResourceNode.h"
#include "WRSocial.h"
#include "WRSound.h"
#include "WRSwampSystems.h"
#include "WRTuning.h"
#include "WRWorldDressing.h"
#include "WRWorldSystems.h"
#include "AbilitySystemComponent.h"
#include "Components/BoxComponent.h"
#include "Components/BrushComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerStart.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsEngine/BodySetup.h"

namespace WRSwampWorldLocal
{
	using WRLookKit::FSurf;
	using WRLookKit::FKit;

	FLinearColor SwwRgb(uint8 R, uint8 G, uint8 B) { return FLinearColor(FColor(R, G, B)); }

	// The boardwalk's palette (cook 2.2, 2.8): sound planks grey-green, rotten black with pale shelf fungus, mended fresh
	// light wood, the black cypress posts.
	FSurf SwwSoundWood(float K = 1.f) { return FSurf::Wood(SwwRgb(78, 82, 72) * 0.5f * K); }
	FSurf SwwRottenWood(float K = 1.f) { return FSurf::Wood(SwwRgb(26, 24, 21) * 0.6f * K); }
	FSurf SwwMendedWood(float K = 1.f) { return FSurf::Wood(SwwRgb(168, 134, 92) * 0.5f * K); }
	FSurf SwwPostWood(float K = 1.f) { return FSurf::Wood(SwwRgb(38, 33, 29) * 0.6f * K); }
	FSurf SwwFungus(float K = 1.f) { return FSurf::Stone(SwwRgb(196, 170, 120) * 0.5f * K, 0.8f); }
	FSurf SwwRope() { return FSurf::Cloth(SwwRgb(120, 100, 70) * 0.4f); }
	FSurf SwwBoneW(float K = 1.f) { return FSurf::Stone(SwwRgb(214, 205, 184) * 0.62f * K, 0.78f); }
	FSurf SwwPeatW(float K = 1.f) { return FSurf::Stone(SwwRgb(46, 33, 24) * 0.6f * K, 0.96f); }
	FSurf SwwRootW(float K = 1.f) { return FSurf::Wood(SwwRgb(92, 58, 42) * 0.48f * K); }
	FSurf SwwGarnetW(float K = 1.f) { return FSurf::Cloth(SwwRgb(122, 10, 20) * 0.6f * K); }
	FSurf SwwMossW(float K = 1.f) { return FSurf::Cloth(SwwRgb(138, 144, 120) * 0.5f * K); }
	FSurf SwwRustW(float K = 1.f) { FSurf S = FSurf::Steel(SwwRgb(138, 59, 30) * 0.32f * K, 0.85f); S.Metal = 0.35f; S.Pattern = WRLookPattern::Mottle; return S; }
	FSurf SwwSaltW(float K = 1.f) { return FSurf::Glaze(SwwRgb(230, 225, 211) * 0.8f * K, 0.55f); }
	FSurf SwwClayW(float K = 1.f) { return FSurf::Stone(SwwRgb(150, 112, 82) * 0.5f * K, 0.8f); }
	FSurf SwwWaterFilm(const FLinearColor& C) { return FSurf::Glaze(C, 0.1f); }

	float SwwHash(int32 A, int32 B) { return FMath::Frac(FMath::Sin(float(A * 12.9898 + B * 78.233)) * 43758.5453f); }

	float SwwDaySeconds() { return FMath::Max(1.f, WRTune(TEXT("clock.dayMinutes"), 60.f) * 60.f); }

	double SwwHours(const UObject* Ctx)
	{
		const AWRGameState* GS = AWRGameState::Get(Ctx);
		return GS ? GS->GetTotalHours() : 0.0;
	}

	/** Ground under a point (any static world), or the fallback height. */
	float SwwGroundZ(UWorld* World, const FVector& At, float Fallback, const AActor* Ignore = nullptr)
	{
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRSwampWorldGround), false, Ignore);
		if (World && World->LineTraceSingleByChannel(Hit, At + FVector(0, 0, 3000.f), At - FVector(0, 0, 6000.f), ECC_WorldStatic, Q) && !Cast<APawn>(Hit.GetActor())) return float(Hit.ImpactPoint.Z);
		return Fallback;
	}

	/** This machine's viewer (a client's camera), or false on a dedicated server. */
	bool SwwViewPoint(const UWorld* World, FVector& Out)
	{
		if (!World || World->GetNetMode() == NM_DedicatedServer) return false;
		APlayerController* PC = World->GetFirstPlayerController();
		if (!PC || !PC->IsLocalController()) return false;
		FRotator Rot;
		PC->GetPlayerViewPoint(Out, Rot);
		return true;
	}

	/** A material for a flat water surface: the WaterMaterials lake (animated normals), tinted; the tinted build material
	 *  if the pack is missing. */
	UMaterialInterface* SwwLakeMat(UObject* Outer, const FLinearColor& Colour, const FLinearColor& Deep, float Speed)
	{
		UMaterialInterface* Lake = LoadObject<UMaterialInterface>(nullptr, *FWRTuning::Get().Str(TEXT("swampWorld.waterMaterial"), TEXT("/Game/WaterMaterials/Materials/M_Lake.M_Lake")));
		if (Lake)
		{
			UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Lake, Outer);
			M->SetVectorParameterValue(TEXT("Colour"), Colour);
			M->SetVectorParameterValue(TEXT("ColourDeep"), Deep);
			M->SetScalarParameterValue(TEXT("Lake_Speed"), Speed);
			return M;
		}
		UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Building/M_BuildTinted.M_BuildTinted"));
		if (!Base) return nullptr;
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, Outer);
		M->SetVectorParameterValue(TEXT("Color"), Colour);
		M->SetScalarParameterValue(TEXT("Rough"), 0.08f);
		return M;
	}
}
using namespace WRSwampWorldLocal;

// ================================================================================================ AWRSwampBoardwalk

AWRSwampBoardwalk::AWRSwampBoardwalk()
{
	bReplicates = true;
	bAlwaysRelevant = true;   // a kilometre long: relevant from anywhere along it (a few hundred bytes)
	SetNetUpdateFrequency(4.f);
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRSwampBoardwalk::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRSwampBoardwalk, RouteId);
	DOREPLIFETIME(AWRSwampBoardwalk, Bays);
	DOREPLIFETIME(AWRSwampBoardwalk, BackAt);
}

void AWRSwampBoardwalk::ReadRoute()
{
	const FWRSwampData& D = FWRSwampData::Get();
	const FWRSwampData::FRoute* Route = nullptr;
	for (const FWRSwampData::FRoute& R : D.Routes) if (R.Id == RouteId) Route = &R;
	Line.Reset();
	LineDist.Reset();
	DeckZ.Reset();
	Base.Reset();
	Bounds = FBox2D(ForceInit);
	if (!Route || Route->Points.Num() < 2) return;
	HalfW = Route->Width * 0.5f;
	// The deck: clear of the water by waterClear and of the ground by groundClear, eased over its neighbours so it
	// rolls instead of stepping.
	const float WaterClear = WRTune(TEXT("swampWorld.deckWaterClear"), 80.f), GroundClear = WRTune(TEXT("swampWorld.deckGroundClear"), 40.f);
	TArray<float> Raw;
	for (const FVector& P : Route->Points) Raw.Add(FMath::Max(float(P.Z) + GroundClear, D.SeaZ + WaterClear));
	for (int32 i = 0; i < Raw.Num(); ++i)
	{
		const float A = Raw[FMath::Max(0, i - 1)], Bz = Raw[i], Cz = Raw[FMath::Min(Raw.Num() - 1, i + 1)];
		DeckZ.Add(FMath::Max(Bz, (A + Bz * 2.f + Cz) * 0.25f));
	}
	float Run = 0.f;
	for (int32 i = 0; i < Route->Points.Num(); ++i)
	{
		const FVector P(Route->Points[i].X, Route->Points[i].Y, DeckZ[i]);
		if (i > 0) Run += FVector::Dist2D(P, Line.Last());
		Line.Add(P);
		LineDist.Add(Run);
		Bounds += FVector2D(P.X, P.Y);
	}
	Length = Run;
	Bay = Line.Num() > 1 ? Length / (Line.Num() - 1) : 240.f;
	// What each bay goes back to: rotten (the data's rotten segments, never two side by side), a gap from the start (its
	// broken places), or sound.
	Base.Init(Sound, Line.Num() - 1);
	for (const int32 R : Route->Rotten) if (Base.IsValidIndex(R)) Base[R] = Rotten;
	for (int32 i = 1; i < Base.Num(); ++i) if (Base[i] == Rotten && Base[i - 1] == Rotten) Base[i] = Sound;
	for (const int32 G : Route->Broken) if (Base.IsValidIndex(G)) Base[G] = Gap;
}

FVector AWRSwampBoardwalk::BayCentre(int32 i) const
{
	if (!Line.IsValidIndex(i) || !Line.IsValidIndex(i + 1)) return GetActorLocation();
	return (Line[i] + Line[i + 1]) * 0.5f;
}

FVector AWRSwampBoardwalk::DeckAt(float Dist) const
{
	if (Line.Num() == 0) return GetActorLocation();
	if (Dist <= 0.f) return Line[0];
	for (int32 i = 1; i < Line.Num(); ++i)
	{
		if (Dist <= LineDist[i])
		{
			const float Seg = FMath::Max(1.f, LineDist[i] - LineDist[i - 1]);
			return FMath::Lerp(Line[i - 1], Line[i], (Dist - LineDist[i - 1]) / Seg);
		}
	}
	return Line.Last();
}

FVector AWRSwampBoardwalk::DirAt(float Dist) const
{
	const FVector A = DeckAt(Dist - 60.f), Bp = DeckAt(Dist + 60.f);
	return (Bp - A).GetSafeNormal2D(1.e-4f, FVector::ForwardVector);
}

int32 AWRSwampBoardwalk::BayAt(const FVector& At) const
{
	if (Line.Num() < 2) return INDEX_NONE;
	const FBox2D Grown(Bounds.Min - FVector2D(HalfW + 100.f), Bounds.Max + FVector2D(HalfW + 100.f));
	if (!Grown.IsInside(FVector2D(At.X, At.Y))) return INDEX_NONE;
	int32 Best = INDEX_NONE;
	float BestD = HalfW + 30.f;
	for (int32 i = 0; i + 1 < Line.Num(); ++i)
	{
		const FVector P0 = Line[i], P1 = Line[i + 1];
		const FVector D = P1 - P0;
		const float L2 = float(D.SizeSquared2D());
		if (L2 < 1.f) continue;
		const float T = FMath::Clamp(float(((At.X - P0.X) * D.X + (At.Y - P0.Y) * D.Y) / L2), 0.f, 1.f);
		const FVector On = P0 + D * T;
		const float Lat = float(FVector::Dist2D(At, On));
		const float Up = float(At.Z - On.Z);
		if (Lat < BestD && Up > -60.f && Up < 120.f) { BestD = Lat; Best = i; }
	}
	return Best;
}

int32 AWRSwampBoardwalk::GapNear(const FVector& At, float Reach) const
{
	int32 Best = INDEX_NONE;
	float BestD = Reach;
	for (int32 i = 0; i < Bays.Num(); ++i)
	{
		if (!IsRepairable(i)) continue;
		const float D = float(FVector::Dist(At, BayCentre(i)));
		if (D < BestD) { BestD = D; Best = i; }
	}
	return Best;
}

bool AWRSwampBoardwalk::IsOccupied(int32 InBay) const
{
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		if (!It->IsAlive()) continue;
		const FVector Feet = It->GetActorLocation() - FVector(0, 0, It->GetSimpleCollisionHalfHeight());
		if (BayAt(Feet) == InBay) return true;
	}
	return false;
}

void AWRSwampBoardwalk::BeginPlay()
{
	Super::BeginPlay();
	if (Line.Num() == 0) ReadRoute();
	if (HasAuthority() && Bays.Num() == 0)
	{
		Bays = Base;
		BackAt.Init(0.f, Bays.Num());
	}
	BreakAt.Init(-1.0, Bays.Num());
	if (GetNetMode() != NM_DedicatedServer) BuildFrame();
	BuildDeck();
	UE_LOG(LogWildreach, Log, TEXT("SWAMP boardwalk %s: %d bays, %.0f m"), *RouteId.ToString(), Bays.Num(), Length / 100.f);
}

void AWRSwampBoardwalk::BuildFrame()
{
	// The frame that never changes: paired black-cypress posts at every joint sunk a metre into the mud (the data's
	// ground under each point), a cap beam across each pair under the stringers, an X of braces between the pair where
	// it stands tall, a diagonal along every fourth bay; every twentieth post runs up past the deck as a Coven waymark
	// with a crow skull and a bottle charm.
	if (bBuilt || Line.Num() < 2) return;
	bBuilt = true;
	const FWRSwampData& D = FWRSwampData::Get();
	const FWRSwampData::FRoute* Route = nullptr;
	for (const FWRSwampData::FRoute& R : D.Routes) if (R.Id == RouteId) Route = &R;
	if (!Route) return;
	TArray<FWRLookBlock> Blocks;
	FKit K(FTransform::Identity, Blocks);
	const float Sink = WRTune(TEXT("swampWorld.postSink"), 100.f);
	for (int32 i = 0; i < Line.Num(); ++i)
	{
		const FVector Dir = DirAt(LineDist[i]);
		const FVector Side = FVector::CrossProduct(FVector::UpVector, Dir);
		const float Ground = float(Route->Points[i].Z);
		const FVector Top = Line[i] - FVector(0, 0, 28.f);
		for (const float Sd : { -1.f, 1.f })
		{
			const FVector P = Top + Side * Sd * (HalfW - 16.f);
			K.Rod(SwwPostWood(0.85f + 0.3f * SwwHash(i, Sd > 0.f ? 1 : 2)), FVector(P.X, P.Y, Ground - Sink), P, 22.f);
			// A ring of slime and weed where the post meets the water.
			if (Ground < D.SeaZ) K.Disc(SwwMossW(0.6f), FVector(P.X, P.Y, D.SeaZ + 2.f), FVector::UpVector, 30.f, 6.f);
		}
		K.Add(SwwPostWood(0.9f), FVector(26.f, HalfW * 2.f + 20.f, 18.f), Top - FVector(0, 0, 4.f), FRotator(0.f, Dir.Rotation().Yaw, 0.f));
		const float Tall = Top.Z - Ground;
		if (Tall > 90.f)
		{
			const FVector A0 = Top + Side * (HalfW - 16.f), A1 = Top - Side * (HalfW - 16.f);
			const float Low = FMath::Max(Ground + 10.f, float(Top.Z) - Tall * 0.85f);
			K.Rod(SwwPostWood(0.8f), FVector(A0.X, A0.Y, Low), A1 - FVector(0, 0, 20.f), 9.f);
			K.Rod(SwwPostWood(0.8f), FVector(A1.X, A1.Y, Low), A0 - FVector(0, 0, 20.f), 9.f);
		}
		if (i % 4 == 0 && i + 1 < Line.Num() && Tall > 60.f)
		{
			const FVector Next = Line[i + 1] - FVector(0, 0, 40.f);
			K.Rod(SwwPostWood(0.75f), FVector(Top.X, Top.Y, Ground + 20.f) + Side * (HalfW - 16.f), Next + Side * (HalfW - 16.f), 8.f);
		}
		if (i % 20 == 10)
		{
			// The waymark post: up past the deck, a crow skull facing the Throat, a bottle charm on a cord.
			const FVector P = Line[i] + Side * (HalfW - 16.f);
			K.Rod(SwwPostWood(1.1f), P - FVector(0, 0, 30.f), P + FVector(0, 0, 150.f), 18.f);
			K.Add(SwwBoneW(1.08f), FVector(10.f, 8.f, 8.f), P + FVector(0, 0, 160.f), FRotator(0.f, 180.f, 0.f), EWRLookMesh::Cylinder);
			K.Spike(FSurf::Stone(FLinearColor(0.02f, 0.02f, 0.022f), 0.35f), P + FVector(-5.f, 0, 160.f), FVector(-1.f, 0.f, -0.15f), 5.f, 14.f);
			K.Rod(SwwRope(), P + FVector(0, 0, 130.f) + Side * 12.f, P + FVector(0, 0, 90.f) + Side * 14.f, 0.8f);
			K.Add(FSurf::Glass(FLinearColor(0.05f, 0.12f, 0.06f), 0.f), FVector(7.f, 7.f, 14.f), P + FVector(0, 0, 82.f) + Side * 14.f, FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		}
	}
	FWRBuildLook::Spawn(this, RootComponent, Blocks, FLinearColor(0.2f, 0.18f, 0.15f), false);
	for (UActorComponent* C : GetComponents())
	{
		if (UInstancedStaticMeshComponent* I = Cast<UInstancedStaticMeshComponent>(C)) I->SetCullDistances(0, (int32)WRTune(TEXT("swampWorld.boardwalkCull"), 40000.f));
	}
}

void AWRSwampBoardwalk::BuildDeck()
{
	if (Line.Num() < 2) return;
	const int32 N = Line.Num() - 1;
	const bool bDraw = GetNetMode() != NM_DedicatedServer;
	// One collision box per bay, on every machine (walking on it is predicted); a gone bay's is off.
	if (BayBoxes.Num() != N)
	{
		for (UBoxComponent* Old : BayBoxes) if (Old) Old->DestroyComponent();
		BayBoxes.Reset();
		for (int32 i = 0; i < N; ++i)
		{
			const FVector P0 = Line[i], P1 = Line[i + 1];
			const FVector D = P1 - P0;
			const float L = float(D.Size());
			UBoxComponent* Box = NewObject<UBoxComponent>(this);
			Box->SetupAttachment(RootComponent);
			Box->SetBoxExtent(FVector(L * 0.5f + 2.f, HalfW, 6.f));
			Box->SetWorldLocationAndRotation((P0 + P1) * 0.5f - FVector(0, 0, 6.f), D.Rotation());
			Box->SetCollisionProfileName(TEXT("BlockAll"));
			Box->SetCanEverAffectNavigation(false);
			Box->RegisterComponent();
			BayBoxes.Add(Box);
		}
	}
	for (int32 i = 0; i < N && i < Bays.Num(); ++i)
	{
		const bool bThere = Bays[i] != Gone && Bays[i] != Gap;
		BayBoxes[i]->SetCollisionEnabled(bThere ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	}
	if (!bDraw) return;
	if (!DeckRoot)
	{
		DeckRoot = NewObject<USceneComponent>(this);
		DeckRoot->SetupAttachment(RootComponent);
		DeckRoot->RegisterComponent();
	}
	const int32 Chunk = FMath::Max(4, (int32)WRTune(TEXT("swampWorld.deckChunk"), 16.f));
	const int32 Chunks = (N + Chunk - 1) / Chunk;
	while (DeckChunks.Num() < Chunks)
	{
		USceneComponent* C = NewObject<USceneComponent>(this);
		C->SetupAttachment(DeckRoot);
		C->RegisterComponent();
		DeckChunks.Add(C);
	}
	if (Shown.Num() != N) Shown.Init(255, N);
	const float Sag = WRTune(TEXT("swampWorld.rottenSag"), 6.f);
	const float Cull = WRTune(TEXT("swampWorld.boardwalkCull"), 40000.f);
	for (int32 c = 0; c < Chunks; ++c)
	{
		bool bChanged = false;
		for (int32 i = c * Chunk; i < FMath::Min(N, (c + 1) * Chunk); ++i) bChanged |= Bays.IsValidIndex(i) && Shown[i] != Bays[i];
		if (!bChanged) continue;
		TArray<USceneComponent*> Old;
		DeckChunks[c]->GetChildrenComponents(false, Old);
		for (USceneComponent* O : Old) if (O) O->DestroyComponent();
		TArray<FWRLookBlock> Blocks;
		FKit K(FTransform::Identity, Blocks);
		for (int32 i = c * Chunk; i < FMath::Min(N, (c + 1) * Chunk); ++i)
		{
			if (!Bays.IsValidIndex(i)) continue;
			Shown[i] = Bays[i];
			const uint8 S = Bays[i];
			const FVector P0 = Line[i], P1 = Line[i + 1];
			const FVector D = P1 - P0;
			const float L = float(D.Size2D());
			if (L < 1.f) continue;
			const FVector Dir = D.GetSafeNormal2D();
			const FVector Side = FVector::CrossProduct(FVector::UpVector, Dir);
			const float Yaw = Dir.Rotation().Yaw;
			const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(float(D.Z), L));
			const FRotator R(Pitch, Yaw, 0.f);
			const bool bRotten = S == Rotten;
			const bool bMended = S == Mended || S == MendedGap;
			// The stringers under the boards (a gap from the start has lost them too: two snapped ends hang down).
			for (const float Sd : { -1.f, 1.f })
			{
				const FVector Off = Side * Sd * (HalfW - 30.f) - FVector(0, 0, 14.f);
				if (S == Gap)
				{
					K.Rod(SwwPostWood(0.7f), P0 + Off, P0 + Off + Dir * 50.f - FVector(0, 0, 40.f), 14.f, true);
					K.Rod(SwwPostWood(0.7f), P1 + Off, P1 + Off - Dir * 40.f - FVector(0, 0, 50.f), 14.f, true);
					continue;
				}
				K.Add(bMended && S == MendedGap ? SwwMendedWood(0.9f) : SwwPostWood(0.8f), FVector(L + 4.f, 14.f, 18.f), (P0 + P1) * 0.5f + Off, R);
				if (bRotten)
				{
					// Pale shelf fungus in tiers on the stringer's outer face.
					for (int32 f = 0; f < 3; ++f)
					{
						const FVector At = FMath::Lerp(P0, P1, 0.2f + 0.3f * f) + Off + Side * Sd * 9.f + FVector(0, 0, -4.f + 5.f * (f % 2));
						K.Add(SwwFungus(0.85f + 0.3f * SwwHash(i, f)), FVector(18.f + 6.f * SwwHash(f, i), 12.f, 3.5f), At + Side * Sd * 5.f, FRotator(0.f, Yaw, Sd * 8.f), EWRLookMesh::Cylinder);
					}
				}
			}
			if (S == Gone || S == Gap)
			{
				// Snapped boards hanging off either end of the hole.
				if (S == Gone)
				{
					K.Add(SwwRottenWood(0.9f), FVector(20.f, HalfW * 1.4f, 4.f), P0 + Dir * 14.f - FVector(0, 0, 30.f) + Side * 20.f, FRotator(Pitch - 60.f, Yaw, 6.f));
					K.Add(SwwRottenWood(0.8f), FVector(20.f, HalfW * 0.9f, 4.f), P1 - Dir * 14.f - FVector(0, 0, 26.f) - Side * 30.f, FRotator(Pitch + 55.f, Yaw, -8.f));
				}
				continue;
			}
			// The boards across: grey-green sound, black and sagging rotten, fresh pale wood where someone mended it.
			const int32 NB = FMath::Max(2, FMath::RoundToInt(L / 24.f));
			for (int32 b = 0; b < NB; ++b)
			{
				const float T = (b + 0.5f) / NB;
				const float Down = bRotten ? Sag * FMath::Sin(T * PI) : 0.f;
				const FSurf Surf = bRotten ? SwwRottenWood(0.8f + 0.4f * SwwHash(i * 31 + b, 7)) : bMended ? SwwMendedWood(0.85f + 0.3f * SwwHash(i * 31 + b, 9)) : SwwSoundWood(0.8f + 0.4f * SwwHash(i * 31 + b, 3));
				const float Jit = 3.f * (SwwHash(i * 17 + b, 1) - 0.5f);
				K.Add(Surf, FVector(L / NB - 2.f, HalfW * 2.f - 4.f + Jit * 4.f, 5.f), FMath::Lerp(P0, P1, T) - FVector(0, 0, 2.5f + Down), FRotator(Pitch, Yaw + Jit, bRotten ? 2.f * (SwwHash(b, i) - 0.5f) : 0.f));
			}
			if (bRotten)
			{
				// Moss in the joints and a darker stain spreading from the middle.
				K.Add(SwwMossW(0.5f), FVector(L * 0.6f, HalfW * 0.8f, 1.f), (P0 + P1) * 0.5f - FVector(0, 0, Sag - 0.2f), R);
			}
			if (bMended)
			{
				// The new plank's nails: four iron heads at each end.
				for (const float E : { 0.08f, 0.92f }) for (int32 n = 0; n < 4; ++n) K.Add(FSurf::Steel(SwwRgb(90, 90, 92) * 0.4f, 0.5f), FVector(2.f, 2.f, 1.f), FMath::Lerp(P0, P1, E) + Side * (-HalfW + 30.f + (HalfW * 2.f - 60.f) * n / 3.f) + FVector(0, 0, 0.6f), R, EWRLookMesh::Cylinder);
			}
		}
		FWRBuildLook::Spawn(this, DeckChunks[c], Blocks, FLinearColor(0.2f, 0.18f, 0.15f), false);
		TArray<USceneComponent*> Made;
		DeckChunks[c]->GetChildrenComponents(false, Made);
		for (USceneComponent* M : Made) if (UInstancedStaticMeshComponent* I = Cast<UInstancedStaticMeshComponent>(M)) I->SetCullDistances(0, (int32)Cull);
	}
}

void AWRSwampBoardwalk::OnRep_Bays()
{
	if (Line.Num() == 0) ReadRoute();
	if (BreakAt.Num() != Bays.Num()) BreakAt.Init(-1.0, Bays.Num());
	BuildDeck();
}

void AWRSwampBoardwalk::MulticastPlank_Implementation(int32 InBay, uint8 What)
{
	if (GetNetMode() == NM_DedicatedServer || !Line.IsValidIndex(InBay + 1)) return;
	const FVector At = BayCentre(InBay);
	if (What == 0)
	{
		// The tell: a loud creak (louder in the rain), the boards bowing a hand under the weight.
		WRSound::PlayAt(this, TEXT("swamp.plank.creak"), At, WRSwamp::IsRaining(this) ? WRTune(TEXT("swampWorld.creakRain"), 1.4f) : 1.f);
		WRFx::SpawnAt(this, TEXT("swamp.plank_snap"), At - FVector(0, 0, 8.f), FRotator::ZeroRotator, 0.25f);
	}
	else if (What == 2)
	{
		WRSound::PlayAt(this, TEXT("swamp.plank.repair"), At);
	}
}

bool AWRSwampBoardwalk::BreakBay(int32 InBay)
{
	if (!HasAuthority() || !Bays.IsValidIndex(InBay) || Bays[InBay] == Gone || Bays[InBay] == Gap) return false;
	Bays[InBay] = Gone;
	if (BackAt.Num() != Bays.Num()) BackAt.Init(0.f, Bays.Num());
	BackAt[InBay] = float(SwwHours(this) + WRTune(TEXT("swampWorld.plankRegrowDays"), 4.f) * 24.0);
	if (BreakAt.IsValidIndex(InBay)) BreakAt[InBay] = -1.0;
	const FVector At = BayCentre(InBay);
	WRFx::Broadcast(this, TEXT("swamp.plank_snap"), At, FRotator::ZeroRotator, 1.f, TEXT("swamp.plank.break"));
	OnRep_Bays();
	return true;
}

FString AWRSwampBoardwalk::Mend(AWRPlayerController* PC, int32 InBay)
{
	AWRPlayerState* PS = PC ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!PS || !Pawn || !IsRepairable(InBay)) return TEXT("Nothing to mend there");
	if (FVector::Dist(Pawn->GetActorLocation(), BayCentre(InBay)) > WRTune(TEXT("swampWorld.plankReach"), 300.f) + 120.f) return TEXT("Get closer to the plank");
	if (IsOccupied(InBay)) return Bays[InBay] == Rotten ? TEXT("Step off the plank first") : TEXT("Someone's in the way");
	const int32 Cost = (int32)WRTune(TEXT("swampWorld.plankWood"), 2.f);
	UWRInventoryComponent* Inv = PS->GetInventory();
	if (!Inv || Inv->CountOf(TEXT("wood")) < Cost) return FString::Printf(TEXT("A new plank needs %d wood"), Cost);
	Inv->RemoveItem(TEXT("wood"), Cost);
	const bool bGap = Bays[InBay] == Gap;
	Bays[InBay] = bGap ? MendedGap : Mended;
	if (BackAt.Num() != Bays.Num()) BackAt.Init(0.f, Bays.Num());
	BackAt[InBay] = float(SwwHours(this) + WRTune(TEXT("swampWorld.plankHoldDays"), 4.f) * 24.0);
	if (BreakAt.IsValidIndex(InBay)) BreakAt[InBay] = -1.0;
	OnRep_Bays();
	MulticastPlank(InBay, 2);
	return bGap ? TEXT("You lay a new plank across the gap. It'll hold a few days") : TEXT("You lay a sound plank over the rotten one. It'll hold a few days");
}

void AWRSwampBoardwalk::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || Bays.Num() == 0 || Line.Num() < 2) return;
	UWorld* World = GetWorld();
	const double Now = World->GetTimeSeconds();
	CheckClock -= DeltaSeconds;
	if (CheckClock <= 0.f)
	{
		CheckClock = WRTune(TEXT("swampWorld.plankCheck"), 0.1f);
		// A rotten plank goes under a run, a roll, a heavy landing, a big body or two survivors at once: first the creak
		// (every machine hears it), then BreakAt later it's gone.
		const float RunSpeed = WRTune(TEXT("swampWorld.plankRunSpeed"), 480.f), LandSpeed = WRTune(TEXT("swampWorld.plankLandSpeed"), 650.f);
		const float HeavyRadius = WRTune(TEXT("swampWorld.plankHeavyRadius"), 70.f);
		const FBox2D Grown(Bounds.Min - FVector2D(600.f), Bounds.Max + FVector2D(600.f));
		TMap<int32, int32> Survivors;
		TSet<int32> Trigger;
		for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
		{
			AWRCharacterBase* C = *It;
			if (!C->IsAlive()) continue;
			const FVector Loc = C->GetActorLocation();
			if (!Grown.IsInside(FVector2D(Loc.X, Loc.Y))) { LastFallSpeed.Remove(C); continue; }
			const UCharacterMovementComponent* Move = C->GetCharacterMovement();
			const bool bFalling = Move && Move->IsFalling();
			float Landed = 0.f;
			if (bFalling) LastFallSpeed.FindOrAdd(C) = FMath::Max(0.f, float(-C->GetVelocity().Z));
			else if (const float* Was = LastFallSpeed.Find(C)) { Landed = *Was; LastFallSpeed.Remove(C); }
			if (bFalling) continue;
			const FVector Feet = Loc - FVector(0, 0, C->GetSimpleCollisionHalfHeight());
			const int32 On = BayAt(Feet);
			if (On == INDEX_NONE || Bays[On] != Rotten) continue;
			if (C->GetPlayerState()) ++Survivors.FindOrAdd(On);
			const UAbilitySystemComponent* ASC = C->GetAbilitySystemComponent();
			const bool bRoll = ASC && ASC->HasMatchingGameplayTag(WRTags::State_Dodging);
			const bool bRun = C->GetVelocity().Size2D() > RunSpeed;
			const bool bHard = Landed > LandSpeed;
			const bool bBig = C->GetSimpleCollisionRadius() >= HeavyRadius;
			if (bRoll || bRun || bHard || bBig) Trigger.Add(On);
		}
		for (const TPair<int32, int32>& S : Survivors) if (S.Value >= 2) Trigger.Add(S.Key);
		for (const int32 T : Trigger)
		{
			if (!BreakAt.IsValidIndex(T) || BreakAt[T] > 0.0) continue;
			BreakAt[T] = Now + WRTune(TEXT("swampWorld.plankTell"), 0.9f);
			MulticastPlank(T, 0);
		}
	}
	for (int32 i = 0; i < BreakAt.Num(); ++i)
	{
		if (BreakAt[i] > 0.0 && Now >= BreakAt[i])
		{
			BreakAt[i] = -1.0;
			if (Bays.IsValidIndex(i) && Bays[i] == Rotten) BreakBay(i);
		}
	}
	// Time: a mended plank lets go after its days (rotten again; a gap from the start opens again); a gone one grows
	// back rotten after its own.
	MendClock -= DeltaSeconds;
	if (MendClock > 0.f) return;
	MendClock = 2.f;
	const double H = SwwHours(this);
	bool bChanged = false;
	for (int32 i = 0; i < Bays.Num() && i < BackAt.Num(); ++i)
	{
		if (BackAt[i] <= 0.f || H < BackAt[i]) continue;
		const uint8 S = Bays[i];
		if (S != Mended && S != MendedGap && S != Gone) continue;
		Bays[i] = Base.IsValidIndex(i) ? Base[i] : uint8(Rotten);
		BackAt[i] = 0.f;
		bChanged = true;
	}
	if (bChanged) OnRep_Bays();
}

// ================================================================================================ AWRSwampCaveTide

AWRSwampCaveTide::AWRSwampCaveTide(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bWaterVolume = true;
	bReplicates = false;   // every machine floods it from the same clock
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.2f;
	// Pawns only (as the sea's volume): the capsule's overlap finds it, traces and the camera never do.
	UBrushComponent* Shape = GetBrushComponent();
	Shape->SetCollisionProfileName(UCollisionProfile::CustomCollisionProfileName);
	Shape->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Shape->SetCollisionObjectType(ECC_WorldDynamic);
	Shape->SetCollisionResponseToAllChannels(ECR_Ignore);
	Shape->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Shape->SetGenerateOverlapEvents(true);
	Shape->SetCanEverAffectNavigation(false);
	// Setup moves it down onto the cave's floor after it's spawned: a static brush refuses the move (the tide stood at the
	// chamber's mid-height instead of rising from the floor).
	Shape->SetMobility(EComponentMobility::Movable);
}

float AWRSwampCaveTide::Fullness(const UObject* WorldContext)
{
	// Each 12 game hours the sea comes in for one: rising over its first tenth, falling over its last.
	const double H = SwwHours(WorldContext);
	const double Every = FMath::Max(1.0, (double)WRTune(TEXT("swampWorld.eelTideEvery"), 12.f));
	const double Dur = FMath::Clamp((double)WRTune(TEXT("swampWorld.eelTideHours"), 1.f), 0.05, Every);
	const double Ramp = FMath::Clamp((double)WRTune(TEXT("swampWorld.eelTideRamp"), 0.1f), 0.01, Dur * 0.5);
	double T = FMath::Fmod(H - (double)WRTune(TEXT("swampWorld.eelTideOffset"), 6.f), Every);
	if (T < 0.0) T += Every;
	if (T > Dur) return 0.f;
	return float(FMath::Clamp(FMath::Min(T / Ramp, (Dur - T) / Ramp), 0.0, 1.0));
}

void AWRSwampCaveTide::Setup(const FTransform& Cave, const FVector& Half, float FloorZ, float Depth)
{
	BoxHalf = Half;
	Floor = FloorZ;
	MaxDepth = Depth;
	FluidFriction = WRTune(TEXT("water.friction"), 2.f);
	SetActorTransform(FTransform(Cave.GetRotation(), FVector(Cave.GetLocation().X, Cave.GetLocation().Y, FloorZ)));
	if (GetNetMode() != NM_DedicatedServer && !Surface)
	{
		// The surface: brackish teal-black water with the lake's moving normals.
		Surface = NewObject<UStaticMeshComponent>(this);
		Surface->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")));
		Surface->SetupAttachment(RootComponent);
		Surface->SetUsingAbsoluteScale(true);
		Surface->SetWorldScale3D(FVector(BoxHalf.X / 50.f, BoxHalf.Y / 50.f, 1.f));
		Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Surface->SetCastShadow(false);
		if (UMaterialInterface* M = SwwLakeMat(this, SwwRgb(30, 43, 42), SwwRgb(10, 16, 16), 0.6f)) Surface->SetMaterial(0, M);
		Surface->RegisterComponent();
		Surface->SetVisibility(false);
	}
	Shown = -1.f;
}

void AWRSwampCaveTide::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Full = Fullness(this);
	const float Height = MaxDepth * Full;
	if (Shown >= 0.f && FMath::Abs(Height - Shown) < WRTune(TEXT("swampWorld.eelTideStep"), 4.f) && !(Height <= 0.f && Shown > 0.f)) return;
	Shown = Height;
	UBrushComponent* Shape = GetBrushComponent();
	UBodySetup* Body = NewObject<UBodySetup>(Shape);
	Body->CollisionTraceFlag = CTF_UseSimpleAsComplex;
	Body->bNeverNeedsCookedCollisionData = true;
	if (Height > 1.f)
	{
		FKBoxElem Box(BoxHalf.X * 2.f, BoxHalf.Y * 2.f, Height);
		Box.Center = FVector(0.f, 0.f, Height * 0.5f);
		Body->AggGeom.BoxElems.Add(Box);
	}
	Body->CreatePhysicsMeshes();
	Shape->BrushBodySetup = Body;
	Shape->RecreatePhysicsState();
	Shape->UpdateBounds();
	if (Surface)
	{
		Surface->SetRelativeLocation(FVector(0.f, 0.f, Height + 2.f));
		Surface->SetVisibility(Height > 5.f);
	}
}

// ================================================================================================ AWRSwampPeatWall

AWRSwampPeatWall::AWRSwampPeatWall()
{
	bReplicates = true;
	SetNetCullDistanceSquared(FMath::Square(25000.f));
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRSwampPeatWall::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRSwampPeatWall, bOpen);
	DOREPLIFETIME(AWRSwampPeatWall, WallHalf);
}

void AWRSwampPeatWall::BeginPlay()
{
	Super::BeginPlay();
	BuildLook();
	OnRep_Open();
}

void AWRSwampPeatWall::BuildLook()
{
	// A face of cut peat closing the side gallery (cook 2.6): turves laid in courses like a wall someone built and the
	// bog grew over, roots threading them, a bog-iron seep in one course, spade cuts on the face; when dug through, a
	// slumped heap of turves and roots on the floor.
	if (bBuilt) return;
	bBuilt = true;
	const FVector H = WallHalf;
	Blocker = NewObject<UBoxComponent>(this);
	Blocker->SetupAttachment(RootComponent);
	Blocker->SetBoxExtent(H);
	Blocker->SetRelativeLocation(FVector(0, 0, H.Z));
	Blocker->SetCollisionProfileName(TEXT("BlockAll"));
	Blocker->SetCanEverAffectNavigation(false);
	Blocker->RegisterComponent();
	if (GetNetMode() == NM_DedicatedServer) return;
	Face = NewObject<USceneComponent>(this);
	Face->SetupAttachment(RootComponent);
	Face->RegisterComponent();
	Heap = NewObject<USceneComponent>(this);
	Heap->SetupAttachment(RootComponent);
	Heap->RegisterComponent();
	const int32 Salt = int32(GetTypeHash(GetActorLocation().GridSnap(10.f).ToString()) % 997u);
	{
		TArray<FWRLookBlock> Blocks;
		FKit K(GetActorTransform(), Blocks);
		int32 Row = 0;
		for (float Z = 0.f; Z < H.Z * 2.f; Z += 30.f, ++Row)
		{
			float Y = -H.Y + ((Row % 2) ? 20.f : 0.f);
			int32 i = 0;
			while (Y < H.Y)
			{
				const float Len = 55.f + 25.f * SwwHash(Salt + Row, i);
				const float Y1 = FMath::Min(float(H.Y), Y + Len);
				const bool bIron = Row == 5 && i % 3 == 1;
				K.Add(bIron ? SwwRustW(0.9f) : SwwPeatW(0.65f + 0.5f * SwwHash(Salt * 3 + i, Row)), FVector(H.X * 2.f - 6.f * SwwHash(i, Row), Y1 - Y - 3.f, 28.f), FVector(4.f * (SwwHash(Row, i + Salt) - 0.5f), (Y + Y1) * 0.5f, Z + 15.f));
				Y = Y1;
				++i;
			}
		}
		for (int32 r = 0; r < 9; ++r)
		{
			const FVector Top(H.X + 2.f, -H.Y + H.Y * 2.f * SwwHash(r, Salt), H.Z * 2.f - 20.f * SwwHash(r + 3, Salt));
			K.Rod(SwwRootW(0.7f + 0.3f * SwwHash(r, 5)), Top, Top - FVector(-4.f, 10.f * (SwwHash(r, 6) - 0.5f), 60.f + 160.f * SwwHash(r, 7)), 3.f + 4.f * SwwHash(r, 8));
		}
		for (int32 c = 0; c < 5; ++c) K.Add(SwwPeatW(0.4f), FVector(2.f, 18.f, 40.f), FVector(H.X + 1.f, -H.Y * 0.5f + H.Y * 0.25f * c, 80.f + 30.f * (c % 2)), FRotator(0.f, 0.f, 12.f));   // spade cuts
		FWRBuildLook::Spawn(this, Face, Blocks, FLinearColor(0.2f, 0.18f, 0.15f), false);
	}
	{
		TArray<FWRLookBlock> Blocks;
		FKit K(GetActorTransform(), Blocks);
		for (int32 t = 0; t < 26; ++t)
		{
			const float Y = (SwwHash(t, Salt + 1) - 0.5f) * H.Y * 1.8f;
			const float X = 40.f + 180.f * SwwHash(t, Salt + 2);
			K.Add(SwwPeatW(0.6f + 0.5f * SwwHash(t, 3)), FVector(56.f, 40.f, 28.f), FVector(X, Y, 14.f + 26.f * SwwHash(t, 4) * (1.f - X / 260.f)), FRotator(25.f * (SwwHash(t, 5) - 0.5f), 360.f * SwwHash(t, 6), 25.f * (SwwHash(t, 7) - 0.5f)));
		}
		for (int32 r = 0; r < 6; ++r) K.Rod(SwwRootW(0.7f), FVector(20.f, -H.Y + H.Y * 0.4f * r, 4.f), FVector(160.f + 40.f * SwwHash(r, 1), -H.Y + H.Y * 0.4f * r + 40.f, 10.f), 5.f);
		FWRBuildLook::Spawn(this, Heap, Blocks, FLinearColor(0.2f, 0.18f, 0.15f), false);
	}
}

void AWRSwampPeatWall::SetOpen(bool bInOpen)
{
	if (!HasAuthority() || bOpen == bInOpen) return;
	bOpen = bInOpen;
	OnRep_Open();
	if (bOpen) WRFx::Broadcast(this, TEXT("gather.stone"), GetActorLocation() + FVector(60.f, 0, WallHalf.Z), FRotator::ZeroRotator, 2.5f, TEXT("swamp.quag.suck"));
}

void AWRSwampPeatWall::OnRep_Open()
{
	if (!bBuilt) BuildLook();
	if (Blocker) Blocker->SetCollisionEnabled(bOpen ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
	if (Face) Face->SetVisibility(!bOpen, true);
	if (Heap) Heap->SetVisibility(bOpen, true);
}

// ================================================================================================ AWRSwampWaterMats

AWRSwampWaterMats::AWRSwampWaterMats()
{
	bReplicates = false;
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Static);
}

void AWRSwampWaterMats::Rebuild(bool bTide)
{
	// The water's four looks (cook 2.1 rule 3) drawn over the water-kind grid a hand above the sea's surface, as merged
	// rectangles per chunk and kind: clear black murk, the duckweed carpet, oxblood bleedwater with its ripples moving,
	// glossy grey quag, the brackish teal of the mangrove channels. In the Red Tide the M3-M5 murk and duckweed run as
	// bleedwater. The Deep Pool's circle is left to its peat-mat lid (the deep_pool scenery).
	if (GetNetMode() == NM_DedicatedServer) return;
	const FWRSwampData& D = FWRSwampData::Get();
	if (!D.bLoaded || D.WaterW <= 0 || D.WaterH <= 0) return;
	for (UActorComponent* P : Parts) if (P) P->DestroyComponent();
	Parts.Reset();
	bBuilt = true;
	bBuiltTide = bTide;
	const int32 W = D.WaterW, H = D.WaterH;
	const float Cell = D.WaterCell;
	Kinds.SetNumZeroed(W * H);
	const FVector2D Deep(D.DeepPool.X, D.DeepPool.Y);
	const float DeepR = (WRTune(TEXT("swampWorld.deepPoolRadius"), 2000.f) + Cell) ;
	for (int32 j = 0; j < H; ++j)
	{
		for (int32 i = 0; i < W; ++i)
		{
			const FVector C(D.WaterOrigin.X + (i + 0.5f) * Cell, D.WaterOrigin.Y + (j + 0.5f) * Cell, 0.f);
			EWRSwampWater K = D.BaseWaterAt(C);
			if (K == EWRSwampWater::Gas) K = EWRSwampWater::Dry;
			if (bTide && (K == EWRSwampWater::Murk || K == EWRSwampWater::Duckweed))
			{
				const TCHAR Z = D.ZoneAt(C);
				if (Z == TEXT('3') || Z == TEXT('4') || Z == TEXT('5')) K = EWRSwampWater::Bleedwater;
			}
			if (!D.DeepPool.IsZero() && FVector2D::Distance(FVector2D(C.X, C.Y), Deep) < DeepR) K = EWRSwampWater::Dry;
			Kinds[j * W + i] = uint8(K);
		}
	}
	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (!Plane) return;
	// One material per kind.
	TMap<uint8, UMaterialInterface*> Mats;
	Mats.Add(uint8(EWRSwampWater::Murk), SwwLakeMat(this, SwwRgb(18, 20, 20), SwwRgb(8, 9, 9), 0.4f));
	Mats.Add(uint8(EWRSwampWater::Brackish), SwwLakeMat(this, SwwRgb(30, 43, 42), SwwRgb(12, 18, 18), 0.8f));
	Mats.Add(uint8(EWRSwampWater::Bleedwater), SwwLakeMat(this, SwwRgb(74, 26, 20), SwwRgb(40, 10, 10), WRTune(TEXT("swampWorld.bleedRippleSpeed"), 2.2f)));
	{
		UMaterialInterface* Scum = LoadObject<UMaterialInterface>(nullptr, *FWRTuning::Get().Str(TEXT("swampWorld.duckweedMaterial"), TEXT("/Game/WaterMaterials/Materials/MIC_PondScum.MIC_PondScum")));
		if (Scum)
		{
			UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Scum, this);
			M->SetVectorParameterValue(TEXT("SurfacePlants_Tint"), SwwRgb(141, 176, 64));
			M->SetVectorParameterValue(TEXT("Base_Algae_Tint"), SwwRgb(60, 82, 30));
			Mats.Add(uint8(EWRSwampWater::Duckweed), M);
		}
		else Mats.Add(uint8(EWRSwampWater::Duckweed), SwwLakeMat(this, SwwRgb(96, 128, 40), SwwRgb(40, 60, 20), 0.2f));
	}
	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Building/M_BuildTinted.M_BuildTinted")))
	{
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, this);
		M->SetVectorParameterValue(TEXT("Color"), SwwRgb(110, 108, 100) * 0.4f);
		M->SetScalarParameterValue(TEXT("Rough"), 0.12f);
		Mats.Add(uint8(EWRSwampWater::Quag), M);
	}
	struct FLift { uint8 Kind; float Z; };
	const FLift Lifts[] = { { uint8(EWRSwampWater::Murk), 3.f }, { uint8(EWRSwampWater::Brackish), 3.5f }, { uint8(EWRSwampWater::Bleedwater), 5.f },
		{ uint8(EWRSwampWater::Duckweed), 6.f }, { uint8(EWRSwampWater::Quag), 4.f } };
	const int32 Chunk = FMath::Max(8, (int32)WRTune(TEXT("swampWorld.matsChunk"), 32.f));
	const float Cull = WRTune(TEXT("swampWorld.matsCull"), 60000.f);
	int32 Rects = 0;
	TArray<uint8> Done;
	for (int32 cy = 0; cy < H; cy += Chunk)
	{
		for (int32 cx = 0; cx < W; cx += Chunk)
		{
			const int32 X1 = FMath::Min(W, cx + Chunk), Y1 = FMath::Min(H, cy + Chunk);
			for (const FLift& L : Lifts)
			{
				UMaterialInterface** M = Mats.Find(L.Kind);
				if (!M || !*M) continue;
				// Greedy: from each cell of the kind not yet covered, its run along the row, grown down the rows.
				Done.Init(0, (X1 - cx) * (Y1 - cy));
				UInstancedStaticMeshComponent* Ism = nullptr;
				for (int32 j = cy; j < Y1; ++j)
				{
					for (int32 i = cx; i < X1; ++i)
					{
						const int32 Li = (j - cy) * (X1 - cx) + (i - cx);
						if (Done[Li] || Kinds[j * W + i] != L.Kind) continue;
						int32 RunEnd = i;
						while (RunEnd + 1 < X1 && !Done[(j - cy) * (X1 - cx) + (RunEnd + 1 - cx)] && Kinds[j * W + RunEnd + 1] == L.Kind) ++RunEnd;
						int32 Down = j;
						bool bGrow = true;
						while (bGrow && Down + 1 < Y1)
						{
							for (int32 k = i; k <= RunEnd; ++k)
							{
								if (Done[(Down + 1 - cy) * (X1 - cx) + (k - cx)] || Kinds[(Down + 1) * W + k] != L.Kind) { bGrow = false; break; }
							}
							if (bGrow) ++Down;
						}
						for (int32 jj = j; jj <= Down; ++jj) for (int32 k = i; k <= RunEnd; ++k) Done[(jj - cy) * (X1 - cx) + (k - cx)] = 1;
						if (!Ism)
						{
							Ism = NewObject<UInstancedStaticMeshComponent>(this);
							Ism->SetStaticMesh(Plane);
							Ism->SetupAttachment(RootComponent);
							Ism->SetMobility(EComponentMobility::Static);
							Ism->SetCollisionEnabled(ECollisionEnabled::NoCollision);
							Ism->SetCastShadow(false);
							Ism->bAffectDistanceFieldLighting = false;
							Ism->SetMaterial(0, *M);
							Ism->SetCullDistances(0, (int32)Cull);
							Ism->RegisterComponent();
							Parts.Add(Ism);
						}
						const float MinX = D.WaterOrigin.X + i * Cell, MinY = D.WaterOrigin.Y + j * Cell;
						const float SX = (RunEnd - i + 1) * Cell, SY = (Down - j + 1) * Cell;
						Ism->AddInstance(FTransform(FRotator::ZeroRotator, FVector(MinX + SX * 0.5f, MinY + SY * 0.5f, D.SeaZ + L.Z), FVector(SX / 100.f, SY / 100.f, 1.f)), true);
						++Rects;
					}
				}
			}
		}
	}
	UE_LOG(LogWildreach, Log, TEXT("SWAMP water mats%s: %d rectangles in %d parts"), bTide ? TEXT(" (Red Tide)") : TEXT(""), Rects, Parts.Num());
}

void AWRSwampWaterMats::NearCells(const FVector& At, float Radius, TArray<FVector>& OutBleed, TArray<FVector>& OutQuag) const
{
	const FWRSwampData& D = FWRSwampData::Get();
	if (!bBuilt || Kinds.Num() != D.WaterW * D.WaterH || D.WaterCell <= 0.f) return;
	const int32 I0 = FMath::Max(0, FMath::FloorToInt((At.X - Radius - D.WaterOrigin.X) / D.WaterCell)), I1 = FMath::Min(D.WaterW - 1, FMath::FloorToInt((At.X + Radius - D.WaterOrigin.X) / D.WaterCell));
	const int32 J0 = FMath::Max(0, FMath::FloorToInt((At.Y - Radius - D.WaterOrigin.Y) / D.WaterCell)), J1 = FMath::Min(D.WaterH - 1, FMath::FloorToInt((At.Y + Radius - D.WaterOrigin.Y) / D.WaterCell));
	for (int32 j = J0; j <= J1; ++j)
	{
		for (int32 i = I0; i <= I1; ++i)
		{
			const uint8 K = Kinds[j * D.WaterW + i];
			if (K != uint8(EWRSwampWater::Bleedwater) && K != uint8(EWRSwampWater::Quag)) continue;
			const FVector C(D.WaterOrigin.X + (i + 0.5f) * D.WaterCell, D.WaterOrigin.Y + (j + 0.5f) * D.WaterCell, D.SeaZ);
			if (FVector::DistSquared2D(C, At) > FMath::Square(Radius)) continue;
			(K == uint8(EWRSwampWater::Bleedwater) ? OutBleed : OutQuag).Add(C);
		}
	}
}

// ================================================================================================ UWRSwampWorld

UWRSwampWorld* UWRSwampWorld::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UWRSwampWorld>() : nullptr;
}

bool UWRSwampWorld::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && WRSwamp::IsLive();
}

void UWRSwampWorld::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	const FString Map = UWorld::RemovePIEPrefix(InWorld.GetMapName());
	if (Map != FWRTuning::Get().Str(TEXT("swampWorld.map"), TEXT("L_Island")) || !WRSwamp::IsLive()) return;
	bActive = true;
	if (InWorld.GetNetMode() != NM_Client) AWRSwampState::GetOrSpawn(&InWorld);
	WaitClock = 0.f;
	UE_LOG(LogWildreach, Log, TEXT("SWAMP world up on %s (waiting for its ground)"), *Map);
}

bool UWRSwampWorld::GroundReady()
{
	// The swamp's ground at the Walking House (the middle of the Mire) streamed in: the scenery's ground traces and the
	// nodes' places are right from then on.
	const FWRSwampData& D = FWRSwampData::Get();
	const FWRSwampData::FSite* Mid = D.FindSite(TEXT("walking_house"));
	const FVector At = Mid ? Mid->At : (D.Camps.Num() ? D.Camps[0] : FVector::ZeroVector);
	FHitResult Hit;
	return GetWorld()->LineTraceSingleByChannel(Hit, At + FVector(0, 0, 20000.f), At - FVector(0, 0, 20000.f), ECC_WorldStatic);
}

void UWRSwampWorld::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	if (!World || !bActive) return;
	const bool bServer = World->GetNetMode() != NM_Client;
	if (!bBuilt)
	{
		WaitClock -= DeltaTime;
		if (WaitClock > 0.f) return;
		WaitClock = 1.f;
		// A client draws what it can see as it comes (StepPending traces each piece's own ground), so it needn't wait.
		if (bServer && !GroundReady() && ++WaitTries < (int32)WRTune(TEXT("swampWorld.groundWaitSeconds"), 60.f)) return;
		bBuilt = true;
		BuildScenery();
		if (bServer) SpawnServerActors();
		return;
	}
	StepPending(DeltaTime);
	if (bServer) StepServer(DeltaTime);
	if (World->GetNetMode() != NM_DedicatedServer)
	{
		StepTide(DeltaTime);
		StepLights(DeltaTime);
		StepWaterLife(DeltaTime);
		StepNodeLooks(DeltaTime);
	}
}

AWRSwampScenery* UWRSwampWorld::SpawnScenery(FName Kind, FName Id, int32 Variant, const FTransform& T, const TSharedPtr<FJsonObject>& Row, const TArray<FVector>* Points, float Width)
{
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWRSwampScenery* A = GetWorld()->SpawnActor<AWRSwampScenery>(AWRSwampScenery::StaticClass(), T, P);
	if (!A) return nullptr;
	A->Kind = Kind;
	A->Id = Id;
	A->Variant = Variant;
	A->Row = Row;
	if (Points) A->Points = *Points;
	A->Width = Width;
	A->Build();
	Scenery.Add(A);
	return A;
}

void UWRSwampWorld::QueueScenery()
{
	const FWRSwampData& D = FWRSwampData::Get();
	const float SiteRange = WRTune(TEXT("swampWorld.siteCull"), 60000.f) * 1.1f, KitRange = WRTune(TEXT("swampWorld.kitCull"), 35000.f) * 1.1f;
	const float RouteRange = WRTune(TEXT("swampWorld.routeCull"), 30000.f) * 1.1f, CovenRange = WRTune(TEXT("swampWorld.covenRange"), 30000.f);
	const auto Add = [&](FName Kind, FName Id, int32 Variant, const FTransform& T, const TSharedPtr<FJsonObject>& Row, const TArray<FVector>& Pts, float Width, const FVector& Probe, float Range)
	{
		FPendingScenery& S = PendingScenery.AddDefaulted_GetRef();
		S.Kind = Kind; S.Id = Id; S.Variant = Variant; S.T = T; S.Row = Row; S.Points = Pts; S.Width = Width; S.Probe = Probe; S.Range = Range;
	};
	const TArray<FVector> None;
	// The Weeping Wall's five falls: one actor at the first fall's top, unturned (its ribbons trace the cliff).
	if (D.Falls.Num() > 0) Add(TEXT("falls"), TEXT("falls"), 0, FTransform(FVector(D.Falls[0].Top.X, D.Falls[0].Top.Y, D.Falls[0].Bottom)), nullptr, None, 0.f, FVector(D.Falls[D.Falls.Num() / 2].Top.X, D.Falls[D.Falls.Num() / 2].Top.Y, D.Falls[0].Bottom), 0.f);
	for (const FWRSwampData::FSite& S : D.Sites)
	{
		const bool bLandmark = S.bLandmark;
		Add(S.Kind, S.Id, S.Variant, FTransform(FRotator(0.f, S.Yaw, 0.f), S.At), S.Row, None, 0.f, S.At, bLandmark ? 0.f : SiteRange);
	}
	for (const FWRSwampData::FSite& S : D.Kit)
	{
		if (S.Kind == TEXT("gas_field")) continue;   // the marsh-gas fields are the Systems' AWRMarshGas
		Add(S.Kind, S.Id, S.Variant, FTransform(FRotator(0.f, S.Yaw, 0.f), S.At), S.Row, None, 0.f, S.At, KitRange);
	}
	// The dry routes' edges in pieces of about 300 m, each built as its viewer nears it.
	const int32 Piece = FMath::Max(4, (int32)WRTune(TEXT("swampWorld.routePiecePoints"), 25.f));
	for (const FWRSwampData::FRoute& R : D.Routes)
	{
		if (R.Kind == TEXT("boardwalk") || R.Points.Num() < 2) continue;
		for (int32 s = 0; s < R.Points.Num() - 1; s += Piece)
		{
			TArray<FVector> Pts;
			for (int32 i = s; i <= FMath::Min(R.Points.Num() - 1, s + Piece); ++i) Pts.Add(R.Points[i]);
			const FVector Mid = Pts[Pts.Num() / 2];
			Add(FName(*(TEXT("route_") + R.Kind.ToString())), FName(*FString::Printf(TEXT("%s_%d"), *R.Id.ToString(), s)), 0, FTransform(FVector(Pts[0].X, Pts[0].Y, Pts[0].Z)), nullptr, Pts, R.Width, Mid, RouteRange);
		}
	}
	// M-S1 and M-S4.
	if (D.CandleLine.Num() >= 2) Add(TEXT("candle_path"), TEXT("candle_path"), 0, FTransform(D.CandleLine[0]), nullptr, D.CandleLine, 0.f, D.CandleLine[D.CandleLine.Num() / 2], KitRange);
	if (const FWRSwampData::FSite* Gullet = D.FindSite(TEXT("gullet"))) Add(TEXT("fall_ledge"), TEXT("fall_ledge"), 0, FTransform(Gullet->At), nullptr, None, 0.f, Gullet->At, SiteRange);
	// The Sunken Coven: its rooms, the three landings, the lanes (in its own space: built when someone's in it).
	if (D.CovenRooms.Num() > 0)
	{
		const float LaneW = WRTune(TEXT("swampWorld.covenLaneWidth"), 360.f);
		TArray<FName> Places;
		for (const FWRSwampData::FRoom& R : D.CovenRooms) Places.Add(R.Id);
		for (const TCHAR* E : { TEXT("mouth"), TEXT("chimney"), TEXT("cellar") }) Places.Add(FName(E));
		for (const FName& Pl : Places)
		{
			FVector Floor, Half;
			if (AWRSwampScenery::CovenPoint(Pl, Floor, Half)) Add(TEXT("coven_room"), Pl, 0, FTransform(Floor), nullptr, None, 0.f, Floor, CovenRange);
		}
		TArray<TPair<FName, FName>> Lanes;
		AWRSwampScenery::CovenLanes(Lanes);
		for (const TPair<FName, FName>& L : Lanes)
		{
			const FVector A = AWRSwampScenery::CovenLaneEnd(L.Key, L.Value), Bp = AWRSwampScenery::CovenLaneEnd(L.Value, L.Key);
			if (A.IsZero() || Bp.IsZero()) continue;
			const TArray<FVector> Pts = { A, Bp };
			Add(TEXT("coven_lane"), FName(*FString::Printf(TEXT("%s-%s"), *L.Key.ToString(), *L.Value.ToString())), 0, FTransform(A), nullptr, Pts, LaneW, (A + Bp) * 0.5f, CovenRange + float(FVector::Dist(A, Bp)) * 0.5f);
		}
	}
}

void UWRSwampWorld::BuildScenery()
{
	UWorld* World = GetWorld();
	const bool bDraw = World->GetNetMode() != NM_DedicatedServer;
	QueueScenery();
	// The server builds it all now (collision for everyone, the caches' spots); a client as it comes into view.
	if (World->GetNetMode() != NM_Client)
	{
		for (const FPendingScenery& S : PendingScenery) SpawnScenery(S.Kind, S.Id, S.Variant, S.T, S.Row, S.Points.Num() ? &S.Points : nullptr, S.Width > 0.f ? S.Width : 200.f);
		PendingScenery.Reset();
	}
	else
	{
		// Landmarks read from a kilometre and more: build them at once.
		for (int32 i = PendingScenery.Num() - 1; i >= 0; --i)
		{
			const FPendingScenery& S = PendingScenery[i];
			if (S.Range > 0.f) continue;
			SpawnScenery(S.Kind, S.Id, S.Variant, S.T, S.Row, S.Points.Num() ? &S.Points : nullptr, S.Width > 0.f ? S.Width : 200.f);
			PendingScenery.RemoveAtSwap(i);
		}
	}
	for (const TWeakObjectPtr<AWRSwampScenery>& W : Scenery)
	{
		if (!W.IsValid()) continue;
		if (W->Kind == TEXT("cage_gallows") || W->Kind == TEXT("broken_punt")) KitScenery.Add(W);
		if (!W->Id.IsNone()) SiteScenery.Add(W->Id, W);
	}
	if (bDraw)
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Mats = World->SpawnActor<AWRSwampWaterMats>(AWRSwampWaterMats::StaticClass(), FTransform::Identity, P);
		const AWRSwampState* State = AWRSwampState::Get(World);
		if (Mats.IsValid()) Mats->Rebuild(State && State->IsTide());
		// The corpse candles' host (the Mere's drifting cold flames).
		CandleHost = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, P);
		if (CandleHost)
		{
			USceneComponent* Root = NewObject<USceneComponent>(CandleHost);
			CandleHost->SetRootComponent(Root);
			Root->RegisterComponent();
			const FWRSwampData::FSite* Mere = FWRSwampData::Get().FindSite(TEXT("corpse_mere"));
			const int32 N = (int32)WRTune(TEXT("swampWorld.corpseCandles"), 5.f);
			for (int32 c = 0; Mere && c < N; ++c)
			{
				USceneComponent* F = WRFx::Attach(Root, TEXT("swamp.candle"), NAME_None, WRTune(TEXT("swampWorld.corpseCandleScale"), 0.8f));
				if (!F) continue;
				F->SetVisibility(false, true);
				FCandle Cd;
				Cd.Fx = F;
				Cd.Centre = FVector(Mere->At.X, Mere->At.Y, FWRSwampData::Get().SeaZ);
				Cd.Radius = WRTune(TEXT("swampWorld.corpseCandleRadius"), 9000.f) * (0.55f + 0.45f * SwwHash(c, 3));
				Cd.Phase = 2.f * PI * c / FMath::Max(1, N);
				Cd.Speed = WRTune(TEXT("swampWorld.corpseCandleSpeed"), 0.02f) * (0.7f + 0.6f * SwwHash(c, 5)) * (c % 2 ? 1.f : -1.f);
				Candles.Add(Cd);
			}
		}
	}
	SpawnCaveTides();
	UE_LOG(LogWildreach, Log, TEXT("SWAMP scenery: %d built, %d waiting for a viewer"), Scenery.Num(), PendingScenery.Num());
}

void UWRSwampWorld::StepPending(float Dt)
{
	if (PendingScenery.Num() == 0) return;
	PendingClock -= Dt;
	if (PendingClock > 0.f) return;
	PendingClock = 0.25f;
	FVector View;
	if (!SwwViewPoint(GetWorld(), View)) return;
	// A few a step, nearest first, so arriving never hitches.
	int32 Budget = (int32)WRTune(TEXT("swampWorld.buildPerStep"), 3.f);
	PendingScenery.Sort([&View](const FPendingScenery& A, const FPendingScenery& B) { return FVector::DistSquared(A.Probe, View) < FVector::DistSquared(B.Probe, View); });
	const float Near = WRTune(TEXT("swampWorld.buildBlindRange"), 8000.f);
	for (int32 i = 0; i < PendingScenery.Num() && Budget > 0;)
	{
		const FPendingScenery& S = PendingScenery[i];
		const float Dist = float(FVector::Dist(S.Probe, View));
		if (Dist > S.Range) break;
		const bool bCoven = S.Kind == TEXT("coven_room") || S.Kind == TEXT("coven_lane");
		// Wait for the ground under it to stream in (its traces seat it), unless the viewer's right on top of it.
		if (!bCoven && Dist > Near)
		{
			FHitResult Hit;
			if (!GetWorld()->LineTraceSingleByChannel(Hit, S.Probe + FVector(0, 0, 8000.f), S.Probe - FVector(0, 0, 8000.f), ECC_WorldStatic)) { ++i; continue; }
		}
		AWRSwampScenery* A = SpawnScenery(S.Kind, S.Id, S.Variant, S.T, S.Row, S.Points.Num() ? &S.Points : nullptr, S.Width > 0.f ? S.Width : 200.f);
		if (A && !A->Id.IsNone()) SiteScenery.Add(A->Id, A);
		if (A && bTideLookShown) A->SetTideLook(true);
		PendingScenery.RemoveAt(i);
		--Budget;
	}
}

void UWRSwampWorld::SpawnServerActors()
{
	UWorld* World = GetWorld();
	const FWRSwampData& D = FWRSwampData::Get();
	AWRSwampState::GetOrSpawn(World);
	// The Coven's boardwalks, their planks' state the server's.
	for (const FWRSwampData::FRoute& R : D.Routes)
	{
		if (R.Kind != TEXT("boardwalk") || R.Points.Num() < 2) continue;
		AWRSwampBoardwalk* B = World->SpawnActorDeferred<AWRSwampBoardwalk>(AWRSwampBoardwalk::StaticClass(), FTransform::Identity, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!B) continue;
		B->RouteId = R.Id;
		B->FinishSpawning(FTransform::Identity);
		Boardwalks.Add(B);
	}
	SpawnStarts();
	SpawnCaves();
	SpawnNodes();
	SpawnCaches();
	UE_LOG(LogWildreach, Log, TEXT("SWAMP server: %d boardwalks, %d caches, %d peat walls"), Boardwalks.Num(), Caches.Num(), PeatWalls.Num());
}

void UWRSwampWorld::SpawnStarts()
{
	// The camps (world-map 2.2) are placed in the map by Tools/terrain/import_swamp.py: a PlayerStart tagged swamp, a safe
	// zone each, the middle camp's starter stations. This is the safety net for a map that hasn't had the swamp imported:
	// only what's missing is made.
	UWorld* World = GetWorld();
	const FWRSwampData& D = FWRSwampData::Get();
	if (bStartsChecked || D.Camps.Num() == 0) return;
	bStartsChecked = true;
	const FName Tag(TEXT("swamp"));
	bool bHaveStart = false;
	for (TActorIterator<APlayerStart> It(World); It; ++It) bHaveStart |= It->PlayerStartTag == Tag;
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	int32 Made = 0;
	for (int32 c = 0; c < D.Camps.Num(); ++c)
	{
		const FVector Camp = D.Camps[c];
		const float Ground = SwwGroundZ(World, Camp, Camp.Z);
		if (!bHaveStart)
		{
			// On the camp's plank platform (60 cm over the ground), a capsule's height up.
			const FVector At(Camp.X, Camp.Y, FMath::Max(Ground, float(Camp.Z)) + WRTune(TEXT("swampWorld.startLift"), 200.f));
			if (APlayerStart* S = World->SpawnActor<APlayerStart>(APlayerStart::StaticClass(), FTransform(FRotator(0.f, 90.f, 0.f), At), P))
			{
				S->PlayerStartTag = Tag;
				++Made;
			}
		}
		bool bSafe = false;
		for (TActorIterator<AWRSafeZone> It(World); It; ++It) bSafe |= FVector::Dist2D(It->GetActorLocation(), Camp) < WRTune(TEXT("swampWorld.campSafeRadius"), 3000.f);
		if (!bSafe)
		{
			if (AWRSafeZone* Z = World->SpawnActor<AWRSafeZone>(AWRSafeZone::StaticClass(), FTransform(FVector(Camp.X, Camp.Y, Ground + 100.f)), P))
			{
				Z->Radius = WRTune(TEXT("swampWorld.campSafeRadius"), 3000.f);
				++Made;
			}
		}
		if (c == D.Camps.Num() / 2)
		{
			bool bStations = false;
			for (TActorIterator<AWRStation> It(World); It; ++It) bStations |= FVector::Dist2D(It->GetActorLocation(), Camp) < 4000.f;
			if (!bStations)
			{
				static const TCHAR* Kinds[] = { TEXT("forge"), TEXT("armorer"), TEXT("alchemy_table"), TEXT("workbench") };
				for (int32 k = 0; k < 4; ++k)
				{
					const FVector Spot(Camp.X - 2500.f, Camp.Y - 1200.f + k * 800.f, 0.f);
					const FTransform T(FRotator(0.f, 0.f, 0.f), FVector(Spot.X, Spot.Y, SwwGroundZ(World, FVector(Spot.X, Spot.Y, Camp.Z), Camp.Z) + 40.f));
					if (AWRStation* St = World->SpawnActorDeferred<AWRStation>(AWRStation::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
					{
						St->StationType = FName(Kinds[k]);
						St->FinishSpawning(T);
						++Made;
					}
				}
			}
		}
	}
	UE_LOG(LogWildreach, Log, TEXT("SWAMP camps: %s; made %d missing start(s) / safe zone(s) / station(s)"), bHaveStart ? TEXT("starts in the map") : TEXT("no starts in the map"), Made);
}

void UWRSwampWorld::SpawnCaves()
{
	// The four karst caves (cook 2.6) as AWRCave in the swamp's look, each by its theme (ore and creatures: AWRCave);
	// the Peat Cellar's back gallery sealed by a wall of cut peat with its peat node in front and its gem cache behind.
	UWorld* World = GetWorld();
	const FWRSwampData& D = FWRSwampData::Get();
	for (const FWRSwampData::FSite& S : D.Sites)
	{
		if (S.Kind != TEXT("cave")) continue;
		FString Theme = S.Id.ToString();
		if (S.Row.IsValid()) S.Row->TryGetStringField(TEXT("cave"), Theme);
		const FTransform T(FRotator(0.f, S.Yaw, 0.f), S.At);
		AWRCave* Cave = World->SpawnActorDeferred<AWRCave>(AWRCave::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Cave) continue;
		Cave->Biome = TEXT("swamp");
		Cave->Theme = FName(*Theme);
		Cave->FinishSpawning(T);
		if (Theme != TEXT("peat_cellar")) continue;
		const auto Floor = [&](float X, float Y)
		{
			const FVector P = T.TransformPosition(FVector(X, Y, 650.f));
			FHitResult Hit;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(WRSwampPeat), false);
			return World->LineTraceSingleByChannel(Hit, P, P - FVector(0, 0, 2000.f), ECC_WorldStatic, Q) ? Hit.ImpactPoint : FVector(P.X, P.Y, S.At.Z);
		};
		const float WallX = WRTune(TEXT("swampWorld.peatWallX"), -600.f);
		const FVector WallAt = Floor(WallX, 0.f) - FVector(0, 0, 20.f);
		const FTransform WT(FRotator(0.f, S.Yaw, 0.f), WallAt);
		AWRSwampPeatWall* Wall = World->SpawnActorDeferred<AWRSwampPeatWall>(AWRSwampPeatWall::StaticClass(), WT, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Wall) continue;
		Wall->WallHalf = FVector(WRTune(TEXT("swampWorld.peatWallThick"), 40.f), WRTune(TEXT("swampWorld.peatWallHalfWidth"), 940.f), WRTune(TEXT("swampWorld.peatWallHalfHeight"), 420.f));
		Wall->FinishSpawning(WT);
		AWRResourceNode* Node = SpawnNode(TEXT("peat"), Floor(WallX + 130.f, 0.f), (int32)WRTune(TEXT("swampWorld.peatWallHits"), 4.f), 2, WRTune(TEXT("swampWorld.peatWallDays"), 3.f));
		FPeat Pt;
		Pt.Node = Node;
		Pt.Wall = Wall;
		PeatWalls.Add(Pt);
		// The gallery behind: a gem and a little iron, left there by someone who sealed it in.
		TMap<FName, int32> Gem;
		Gem.Add(TEXT("gem_ruby"), 1);
		Gem.Add(TEXT("iron_ore"), 3);
		SpawnCache(Floor(WallX - 220.f, 120.f) + FVector(0, 0, 30.f), S.Yaw + 180.f, TEXT("The sealed gallery"), Gem, TEXT("Peat packed in by hand against the dark, and behind it what someone meant to come back for."));
	}
}

void UWRSwampWorld::SpawnCaveTides()
{
	// The Eel Hole (cook 2.6) floods with the tide; every machine makes its own from the clock.
	UWorld* World = GetWorld();
	const FWRSwampData& D = FWRSwampData::Get();
	for (const FWRSwampData::FSite& S : D.Sites)
	{
		FString Theme;
		if (S.Kind != TEXT("cave") || !S.Row.IsValid() || !S.Row->TryGetStringField(TEXT("cave"), Theme) || Theme != TEXT("eel_hole")) continue;
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FTransform Cave(FRotator(0.f, S.Yaw, 0.f), S.At);
		// The chamber and its tunnel (AWRCave's layout: chamber -900..900, the tunnel out along +X to 2 km).
		const FVector Mid = Cave.TransformPosition(FVector(WRTune(TEXT("swampWorld.eelTideCentreX"), 500.f), 0.f, 0.f));
		AWRSwampCaveTide* Tide = World->SpawnActor<AWRSwampCaveTide>(AWRSwampCaveTide::StaticClass(), FTransform(Cave.GetRotation(), Mid), P);
		if (!Tide) continue;
		Tide->Setup(FTransform(Cave.GetRotation(), Mid), FVector(WRTune(TEXT("swampWorld.eelTideHalfX"), 1500.f), WRTune(TEXT("swampWorld.eelTideHalfY"), 1000.f), 0.f),
			SwwGroundZ(World, Mid, S.At.Z) - WRTune(TEXT("swampWorld.eelTideBelow"), 60.f), WRTune(TEXT("swampWorld.eelTideDepth"), 260.f));
		Tides.Add(Tide);
	}
}

AWRResourceNode* UWRSwampWorld::SpawnNode(FName Item, const FVector& At, int32 Hits, int32 PerHit, float RespawnDays)
{
	UWorld* World = GetWorld();
	// Down here in the Coven's own space there's no ground under it but its floor: no trace.
	const bool bCoven = At.Z > WRSwamp::CovenLift() - 5000.f;
	const FVector Spot = bCoven ? At : FVector(At.X, At.Y, SwwGroundZ(World, At, At.Z));
	const FTransform T(FRotator(0.f, 360.f * SwwHash(int32(At.X), int32(At.Y)), 0.f), Spot);
	AWRResourceNode* N = World->SpawnActorDeferred<AWRResourceNode>(AWRResourceNode::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!N) return nullptr;
	N->ItemId = Item;
	N->Hits = FMath::Max(1, Hits);
	N->PerHit = FMath::Max(1, PerHit);
	N->RespawnSeconds = FMath::Max(30.f, RespawnDays * SwwDaySeconds());
	N->FinishSpawning(T);
	return N;
}

void UWRSwampWorld::SpawnNodes()
{
	// The Mire's resources (cook 2.7): bloodmoss by hand (2 a node, back in 2 game days) on the hummocks, the Bloodmoss
	// Dome and the edges of the bleedwater pools; bog iron (a pick, 3 hits, regrows in 4 days) in the red shallows and the
	// Weeping Pools; peat in the cut and the stacks; salt at the Lick; clay at the pit; iron in the Ringing Room's fallen
	// wall. Copper, tin and iron in the caves are AWRCave's.
	const FWRSwampData& D = FWRSwampData::Get();
	const float MossDays = WRTune(TEXT("swampWorld.bloodmossDays"), 2.f), IronDays = WRTune(TEXT("swampWorld.bogIronDays"), 4.f);
	const int32 MossHits = (int32)WRTune(TEXT("swampWorld.bloodmossHits"), 1.f), MossPer = (int32)WRTune(TEXT("swampWorld.bloodmossPerHit"), 2.f);
	const int32 IronHits = (int32)WRTune(TEXT("swampWorld.bogIronHits"), 3.f), IronPer = (int32)WRTune(TEXT("swampWorld.bogIronPerHit"), 2.f);
	int32 Made = 0;
	const auto Ring = [&](FName Item, const FVector& Centre, int32 Count, float R, int32 Hits, int32 Per, float Days, int32 Salt)
	{
		for (int32 i = 0; i < Count; ++i)
		{
			const float A = i * 2.39996f + Salt;
			const float Dd = R * FMath::Sqrt((i + 0.5f) / FMath::Max(1, Count));
			if (SpawnNode(Item, Centre + FVector(FMath::Cos(A) * Dd, FMath::Sin(A) * Dd, 0.f), Hits, Per, Days)) ++Made;
		}
	};
	for (const FWRSwampData::FSite& S : D.Kit)
	{
		if (S.Kind == TEXT("bloodmoss_hummock")) Ring(TEXT("bloodmoss"), S.At, 6 + 2 * (S.Variant % 3), 280.f, MossHits, MossPer, MossDays, S.Variant);
		else if (S.Kind == TEXT("bog_iron_shallows")) Ring(TEXT("bog_iron"), S.At, 3, 400.f, IronHits, IronPer, IronDays, S.Variant);
		else if (S.Kind == TEXT("peat_stack")) Ring(TEXT("peat"), S.At + FVector(-100.f, -150.f, 0.f), 2, 200.f, 3, 2, 2.f, S.Variant);
	}
	for (const FWRSwampData::FSite& S : D.Sites)
	{
		double Radius = 100.0, Nodes = 24.0;
		if (S.Row.IsValid()) { S.Row->TryGetNumberField(TEXT("radius"), Radius); S.Row->TryGetNumberField(TEXT("nodes"), Nodes); }
		if (S.Kind == TEXT("bloodmoss_dome")) Ring(TEXT("bloodmoss"), S.At, (int32)Nodes, float(Radius) * 100.f * 0.8f, MossHits, MossPer, MossDays, 7);
		else if (S.Kind == TEXT("weeping_pools")) Ring(TEXT("bog_iron"), S.At, 6, 1500.f, IronHits, IronPer, IronDays, 3);
		else if (S.Kind == TEXT("peat_cut")) for (int32 p = 0; p < 6; ++p) { if (SpawnNode(TEXT("peat"), S.At + FRotator(0.f, S.Yaw, 0.f).RotateVector(FVector(-2500.f + 1000.f * p, (p % 2 ? 1.f : -1.f) * 230.f, 0.f)), 3, 2, 2.f)) ++Made; }
		else if (S.Kind == TEXT("salt_lick")) Ring(TEXT("salt"), S.At, 5, 600.f, 2, 2, 2.f, 11);
		else if (S.Kind == TEXT("clay_pit")) Ring(TEXT("clay"), S.At, 5, 700.f, 3, 2, 2.f, 13);
	}
	// Bloodmoss at the edges of the bleedwater pools: bleed cells with dry ground beside them, kept apart.
	if (D.WaterW > 0)
	{
		const int32 MaxEdge = (int32)WRTune(TEXT("swampWorld.edgeMossNodes"), 40.f);
		const float Apart = WRTune(TEXT("swampWorld.edgeMossApart"), 4000.f);
		TArray<FVector> Placed;
		for (int32 j = 1; j < D.WaterH - 1 && Placed.Num() < MaxEdge; j += 3)
		{
			for (int32 i = 1; i < D.WaterW - 1 && Placed.Num() < MaxEdge; i += 3)
			{
				if (!D.WaterRows.IsValidIndex(j) || !D.WaterRows[j].IsValidIndex(i) || D.WaterRows[j][i] != TEXT('b')) continue;
				FIntPoint Dry(-1, -1);
				for (const FIntPoint& O : { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) })
				{
					const int32 X = i + O.X, Y = j + O.Y;
					if (D.WaterRows.IsValidIndex(Y) && D.WaterRows[Y].IsValidIndex(X) && D.WaterRows[Y][X] == TEXT('.')) { Dry = FIntPoint(X, Y); break; }
				}
				if (Dry.X < 0) continue;
				const FVector C(D.WaterOrigin.X + (Dry.X + 0.5f) * D.WaterCell, D.WaterOrigin.Y + (Dry.Y + 0.5f) * D.WaterCell, D.SeaZ);
				bool bFar = true;
				for (const FVector& P : Placed) bFar &= FVector::DistSquared2D(P, C) > FMath::Square(Apart);
				if (!bFar) continue;
				Placed.Add(C);
				if (SpawnNode(TEXT("bloodmoss"), C, MossHits, MossPer, MossDays)) ++Made;
			}
		}
	}
	// The Ringing Room's collapsed wall: two iron veins in the fallen stone.
	FVector RoomFloor, Half;
	if (AWRSwampScenery::CovenPoint(TEXT("ringing_room"), RoomFloor, Half))
	{
		for (const float Y : { -220.f, 220.f }) if (SpawnNode(TEXT("iron_ore"), RoomFloor + FVector(-Half.X + 200.f, Y, 0.f), 6, 2, 1.f)) ++Made;
	}
	UE_LOG(LogWildreach, Log, TEXT("SWAMP nodes: %d"), Made);
}

AWRWorldCache* UWRSwampWorld::SpawnCache(const FVector& At, float Yaw, const FString& Title, const TMap<FName, int32>& Items, const FString& Lore)
{
	if (Items.Num() == 0) return nullptr;
	const FTransform T(FRotator(0.f, Yaw, 0.f), At);
	AWRWorldCache* C = GetWorld()->SpawnActorDeferred<AWRWorldCache>(AWRWorldCache::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!C) return nullptr;
	C->Kind = TEXT("chest");
	C->Biome = TEXT("swamp");
	C->Title = Title;
	C->Items = Items;
	C->Lore = Lore;
	C->RefillHours = WRTune(TEXT("swampWorld.siteRefillHours"), 72.f);
	C->FinishSpawning(T);
	Caches.Add(C);
	return C;
}

void UWRSwampWorld::SpawnCaches()
{
	// What each hand-made site pays (cook 2.4 "Pays", swamp.json loot), in a cache where its scenery keeps one (a cabin, a
	// fallen cage) or beside its middle; the kit's fallen cages and broken punts hold a little each. Lore lines from
	// tuning swampWorld.cacheLore. They fill again after siteRefillHours.
	const FWRSwampData& D = FWRSwampData::Get();
	const TSharedPtr<FJsonObject> LoreRows = FWRTuning::Get().Object(TEXT("swampWorld.cacheLore"));
	const auto Spot = [&](FName Id, const FVector& Fallback) -> FVector
	{
		const TWeakObjectPtr<AWRSwampScenery>* W = SiteScenery.Find(Id);
		if (W && W->IsValid() && !(*W)->CacheSpot.IsZero()) return (*W)->GetActorTransform().TransformPosition((*W)->CacheSpot);
		return Fallback;
	};
	for (const FWRSwampData::FSite& S : D.Sites)
	{
		if (S.Loot.Num() == 0) continue;
		const FVector Beside = S.At + FRotator(0.f, S.Yaw, 0.f).RotateVector(FVector(WRTune(TEXT("swampWorld.cacheBesideX"), 250.f), WRTune(TEXT("swampWorld.cacheBesideY"), 160.f), 0.f));
		FVector At = Spot(S.Id, FVector(Beside.X, Beside.Y, SwwGroundZ(GetWorld(), Beside, S.At.Z) + 30.f));
		FString Lore;
		if (LoreRows.IsValid()) LoreRows->TryGetStringField(S.Id.ToString(), Lore);
		SpawnCache(At, S.Yaw + 180.f, S.Name, S.Loot, Lore);
	}
	const TSharedPtr<FJsonObject> KitLoot = FWRTuning::Get().Object(TEXT("swampWorld.kitLoot"));
	int32 KitN = 0;
	for (const TWeakObjectPtr<AWRSwampScenery>& W : KitScenery)
	{
		if (!W.IsValid() || W->CacheSpot.IsZero()) continue;
		TMap<FName, int32> Items;
		const TSharedPtr<FJsonObject>* Row = nullptr;
		if (KitLoot.IsValid() && KitLoot->TryGetObjectField(W->Kind.ToString(), Row))
		{
			for (const auto& KV : (*Row)->Values) if (KV.Value.IsValid()) Items.Add(FName(*KV.Key), (int32)KV.Value->AsNumber());
		}
		if (Items.Num() == 0)
		{
			if (W->Kind == TEXT("cage_gallows")) { Items.Add(TEXT("bone"), 4); Items.Add(TEXT("rope"), 1); }
			else { Items.Add(TEXT("rope"), 2); Items.Add(TEXT("peat"), 4); }
		}
		const bool bCage = W->Kind == TEXT("cage_gallows");
		const FVector L = W->GetActorLocation();
		SpawnCache(W->GetActorTransform().TransformPosition(W->CacheSpot), W->GetActorRotation().Yaw, FString::Printf(TEXT("%s (%.0f, %.0f)"), bCage ? TEXT("A fallen cage") : TEXT("A punt's locker"), L.X / 100.f, L.Y / 100.f),
			Items, bCage ? TEXT("The cage burst when it hit the mud. What it held is still in it.") : TEXT("Pitch-sealed against the wet. Whoever poled this punt meant to come back."));
		++KitN;
	}
	UE_LOG(LogWildreach, Log, TEXT("SWAMP caches: %d (%d in the kit)"), Caches.Num(), KitN);
}

void UWRSwampWorld::StepServer(float Dt)
{
	ServerClock -= Dt;
	if (ServerClock > 0.f) return;
	ServerClock = 0.5f;
	UWorld* World = GetWorld();
	if (!bStartsChecked) SpawnStarts();
	// The Peat Cellar's wall falls in when its peat is dug out (four pick hits on the node, or a tamed Bogsnout digging
	// at it for digSeconds) and packs back when the peat grows back, as long as nobody stands in it.
	for (FPeat& P : PeatWalls)
	{
		AWRSwampPeatWall* Wall = P.Wall.Get();
		AWRResourceNode* Node = P.Node.Get();
		if (!Wall) continue;
		if (Node && !Node->IsDepleted())
		{
			bool bSnout = false;
			for (TActorIterator<AWRCreature> It(World); It; ++It)
			{
				if (It->CreatureId == TEXT("bogsnout") && It->IsTamed() && It->IsAlive() && FVector::Dist2D(It->GetActorLocation(), Node->GetActorLocation()) < WRTune(TEXT("swampWorld.snoutDigRange"), 500.f))
				{
					bSnout = true;
					P.Dug += 0.5f;
					if (P.Dug >= WRTune(TEXT("swampWorld.snoutDigSeconds"), 20.f))
					{
						Node->Blast(*It, FString(), 1.f);
						P.Dug = 0.f;
					}
					break;
				}
			}
			if (!bSnout) P.Dug = 0.f;
		}
		const bool bOpen = !Node || Node->IsDepleted();
		if (bOpen != Wall->bOpen)
		{
			bool bBlocked = false;
			if (!bOpen)
			{
				const FBox Box = FBox::BuildAABB(Wall->GetActorLocation() + FVector(0, 0, Wall->WallHalf.Z), FVector(FMath::Max(Wall->WallHalf.X, Wall->WallHalf.Y)) + FVector(60.f));
				for (TActorIterator<AWRCharacterBase> It(World); It && !bBlocked; ++It) bBlocked = Box.IsInside(It->GetActorLocation()) && FVector::Dist2D(It->GetActorLocation(), Wall->GetActorLocation()) < Wall->WallHalf.X + 80.f;
			}
			if (!bBlocked) Wall->SetOpen(bOpen);
		}
	}
	// Caches that fill again.
	const double H = SwwHours(this);
	for (const TWeakObjectPtr<AWRWorldCache>& C : Caches)
	{
		if (C.IsValid() && C->bTaken && C->RefillHours > 0.f && C->RefillAtHours >= 0.0 && H >= C->RefillAtHours) C->Refill();
	}
}

void UWRSwampWorld::StepTide(float Dt)
{
	// The Red Tide's look (cook 2.10): from its warning the falls run oxblood and the Throat's mist doubles; in the tide
	// itself the M3-M5 water mats turn bleedwater.
	const AWRSwampState* State = AWRSwampState::Get(this);
	const bool bLook = State && State->RedTide != EWRRedTide::None;
	const int32 Tide = State && State->IsTide() ? 1 : 0;
	if (bLook != bTideLookShown)
	{
		bTideLookShown = bLook;
		for (const TWeakObjectPtr<AWRSwampScenery>& W : Scenery) if (W.IsValid()) W->SetTideLook(bLook);
	}
	if (Tide != LastTide)
	{
		const bool bFirst = LastTide < 0;
		LastTide = Tide;
		if (Mats.IsValid() && (!bFirst || Mats->bBuiltTide != (Tide == 1))) Mats->Rebuild(Tide == 1);
	}
}

void UWRSwampWorld::StepLights(float Dt)
{
	// Night lights (cook 2.1): toad rings pulsing green-gold, the Walking House's window and the camps, the Throat's mist
	// glow (always: SetTideLook doubles it), corpse candles over the Mere; the Coven's toads burn day and night; the
	// lightning flickering in Stormgut behind the Bled Roc; things on chains swaying near the viewer; the third fall
	// thinning at sunset (M-S4).
	UWorld* World = GetWorld();
	SwayTime += Dt;
	const AWRGameState* GS = AWRGameState::Get(World);
	const float Daylight = GS ? GS->GetDaylight() : 1.f;
	const float Night = 1.f - Daylight;
	const bool bNight = Night > 0.5f;
	FVector View = FVector::ZeroVector;
	const bool bView = SwwViewPoint(World, View);
	const float SwayRange = WRTune(TEXT("swampWorld.swayRange"), 8000.f);
	const float Wind = WRSwamp::WeatherNow(World) == TEXT("rain") ? 1.8f : 1.f;
	LightClock -= Dt;
	const bool bSlow = LightClock <= 0.f;
	if (bSlow) LightClock = 0.1f;
	// The third fall's narrowing: an hour and a half before dusk to half an hour after.
	const float Hour = GS ? GS->GetHour() : 12.f;
	const float Dusk = WRTune(TEXT("clock.duskHour"), 20.f);
	const bool bThin = Hour > Dusk - WRTune(TEXT("swampWorld.fallThinBefore"), 1.5f) && Hour < Dusk + WRTune(TEXT("swampWorld.fallThinAfter"), 0.5f);
	const float ThinTarget = bThin ? WRTune(TEXT("swampWorld.fallThinScale"), 0.35f) : 1.f;
	FallThinShown = FMath::FInterpTo(FallThinShown, ThinTarget, Dt, 0.4f);
	for (int32 s = Scenery.Num() - 1; s >= 0; --s)
	{
		AWRSwampScenery* S = Scenery[s].Get();
		if (!S) { Scenery.RemoveAtSwap(s); continue; }
		const float Lit = S->bUnder ? 1.f : FMath::Clamp((Night - 0.2f) / 0.6f, 0.f, 1.f);
		if (bSlow)
		{
			for (int32 i = 0; i < S->NightLights.Num(); ++i) if (S->NightLights[i]) S->NightLights[i]->SetIntensity(S->NightBase.IsValidIndex(i) ? S->NightBase[i] * Lit : 0.f);
			for (USceneComponent* F : S->NightFx) if (F && F->IsVisible() != (Lit > 0.05f)) F->SetVisibility(Lit > 0.05f, true);
			for (int32 i = 0; i < S->FlickerLights.Num(); ++i)
			{
				UPointLightComponent* L = S->FlickerLights[i];
				if (!L) continue;
				const bool bFlash = bNight && FMath::FRand() < WRTune(TEXT("swampWorld.gutFlickerChance"), 0.05f);
				L->SetIntensity(bFlash ? (S->FlickerBase.IsValidIndex(i) ? S->FlickerBase[i] : 0.f) * FMath::FRandRange(0.4f, 1.f) : L->Intensity * 0.4f);
			}
		}
		// The toads' throat sacs swell and fade, each to its own beat.
		if (Lit > 0.01f && S->PulseLights.Num() > 0 && (!bView || FVector::DistSquared(View, S->GetActorLocation()) < FMath::Square(WRTune(TEXT("swampWorld.pulseRange"), 30000.f))))
		{
			for (int32 i = 0; i < S->PulseLights.Num(); ++i)
			{
				if (!S->PulseLights[i]) continue;
				const float Beat = 0.55f + 0.45f * FMath::Sin(SwayTime * (1.6f + 0.5f * SwwHash(i, s)) + 6.28f * SwwHash(s, i));
				S->PulseLights[i]->SetIntensity((S->PulseBase.IsValidIndex(i) ? S->PulseBase[i] : 0.f) * Beat * Lit);
			}
		}
		else if (bSlow && Lit <= 0.01f) for (UPointLightComponent* L : S->PulseLights) if (L && L->Intensity > 0.f) L->SetIntensity(0.f);
		// Cages, charms and chimes swing a little about their hooks near the viewer.
		if (bView && S->Swingers.Num() > 0 && FVector::DistSquared(View, S->GetActorLocation()) < FMath::Square(SwayRange + 6000.f))
		{
			for (int32 i = 0; i < S->Swingers.Num(); ++i)
			{
				USceneComponent* C = S->Swingers[i];
				if (!C || FVector::DistSquared(View, C->GetComponentLocation()) > FMath::Square(SwayRange)) continue;
				const float Ph = 6.28f * SwwHash(i, s + 3);
				C->SetRelativeRotation(FRotator(2.2f * Wind * FMath::Sin(SwayTime * 0.9f + Ph), 0.f, 2.8f * Wind * FMath::Sin(SwayTime * 0.7f + Ph * 1.3f)));
			}
		}
		for (USceneComponent* T : S->FallThin) if (T && !FMath::IsNearlyEqual(float(T->GetRelativeScale3D().Y), FallThinShown, 0.002f)) T->SetRelativeScale3D(FVector(1.f, FallThinShown, 1.f));
	}
	// The corpse candles drift low round the Mere at night (by day in miasma), each on its own loop, bobbing.
	const bool bCandles = Night > 0.6f || WRSwamp::CandlesByDay(World);
	if (!bCandlesShown || bNightShown != bCandles)
	{
		bCandlesShown = true;
		bNightShown = bCandles;
		for (const FCandle& C : Candles) if (C.Fx.IsValid()) C.Fx->SetVisibility(bCandles, true);
	}
	if (bCandles)
	{
		for (FCandle& C : Candles)
		{
			if (!C.Fx.IsValid()) continue;
			C.Phase += C.Speed * Dt;
			const float Wob = 0.75f + 0.25f * FMath::Sin(C.Phase * 3.1f);
			C.Fx->SetWorldLocation(C.Centre + FVector(FMath::Cos(C.Phase) * C.Radius * Wob, FMath::Sin(C.Phase) * C.Radius * Wob, WRTune(TEXT("swampWorld.corpseCandleHeight"), 90.f) + 25.f * FMath::Sin(SwayTime * 1.3f + C.Phase * 5.f)));
		}
	}
}

void UWRSwampWorld::StepWaterLife(float Dt)
{
	// Near the viewer: bleedwater's leech ripples that never stop (cook 2.1), quag's slow fat bubbles.
	LifeClock -= Dt;
	if (LifeClock > 0.f || !Mats.IsValid()) return;
	LifeClock = WRTune(TEXT("swampWorld.waterLifeEvery"), 0.3f);
	FVector View;
	if (!SwwViewPoint(GetWorld(), View) || !WRSwamp::Contains(View)) return;
	TArray<FVector> Bleed, Quag;
	Mats->NearCells(View, WRTune(TEXT("swampWorld.waterLifeRange"), 2500.f), Bleed, Quag);
	const float Cell = FWRSwampData::Get().WaterCell;
	const int32 Per = (int32)WRTune(TEXT("swampWorld.ripplesPerStep"), 4.f);
	for (int32 k = 0; k < Per && Bleed.Num() > 0; ++k)
	{
		const FVector C = Bleed[FMath::RandRange(0, Bleed.Num() - 1)] + FVector(FMath::FRandRange(-0.5f, 0.5f) * Cell, FMath::FRandRange(-0.5f, 0.5f) * Cell, 6.f);
		WRFx::SpawnAt(this, TEXT("swamp.ripple"), C, FRotator::ZeroRotator, FMath::FRandRange(0.7f, 1.3f));
	}
	for (int32 k = 0; k < Per / 2 && Quag.Num() > 0; ++k)
	{
		const FVector C = Quag[FMath::RandRange(0, Quag.Num() - 1)] + FVector(FMath::FRandRange(-0.5f, 0.5f) * Cell, FMath::FRandRange(-0.5f, 0.5f) * Cell, 5.f);
		WRFx::SpawnAt(this, TEXT("swamp.bubble"), C, FRotator::ZeroRotator, FMath::FRandRange(0.8f, 1.6f));
	}
}

void UWRSwampWorld::StepNodeLooks(float Dt)
{
	// The swamp's own node looks over AWRResourceNode's generic boulder: a hummock of sphagnum garnet on top (bloodmoss),
	// rust-crusted stones in a hand of red water (bog iron), a cut bank of peat with its turves (peat), a white salt
	// crust heaped on the pan (salt). Clay keeps the node's own clay bank.
	NodeClock -= Dt;
	if (NodeClock > 0.f) return;
	NodeClock = 2.f;
	static const FName Items[] = { TEXT("bloodmoss"), TEXT("bog_iron"), TEXT("peat"), TEXT("salt") };
	for (TActorIterator<AWRResourceNode> It(GetWorld()); It; ++It)
	{
		AWRResourceNode* N = *It;
		const FName Item = N->ItemId;
		if (Item != Items[0] && Item != Items[1] && Item != Items[2] && Item != Items[3]) continue;
		// The generic look (tags WRLook / WRLookBits) stays hidden, even if the node redraws it.
		for (UActorComponent* C : N->GetComponents())
		{
			if (UPrimitiveComponent* P = Cast<UPrimitiveComponent>(C); P && (P->ComponentHasTag(TEXT("WRLook")) || P->ComponentHasTag(TEXT("WRLookBits"))) && P->IsVisible()) P->SetVisibility(false);
		}
		if (Dressed.Contains(N)) continue;
		Dressed.Add(N);
		USceneComponent* Look = NewObject<USceneComponent>(N);
		Look->SetupAttachment(N->GetRootComponent());
		Look->SetUsingAbsoluteScale(true);
		Look->RegisterComponent();
		Look->SetWorldScale3D(FVector::OneVector);
		const int32 Sd = int32(GetTypeHash(N->GetActorLocation().GridSnap(50.f).ToString()) % 97u);
		TArray<FWRLookBlock> Blocks;
		FKit K(FTransform(N->GetActorRotation(), N->GetActorLocation()), Blocks);
		if (Item == Items[0])
		{
			K.Disc(SwwMossW(0.9f), FVector(0, 0, 10.f), FVector::UpVector, 150.f, 26.f);
			for (int32 i = 0; i < 9; ++i)
			{
				const float A = i * 2.39996f + Sd;
				const float R = 55.f * FMath::Sqrt((i + 0.5f) / 9.f);
				K.Add(SwwGarnetW(0.75f + 0.45f * SwwHash(i, Sd)), FVector(34.f + 14.f * SwwHash(Sd, i), 30.f, 12.f + 6.f * SwwHash(i, 2)), FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 24.f + 4.f * SwwHash(i, 3)), FRotator(0.f, A * 57.f, 0.f), EWRLookMesh::Cylinder);
			}
			for (int32 t = 0; t < 14; ++t) K.Add(SwwGarnetW(1.2f), FVector(3.f, 3.f, 8.f), FVector(FMath::Cos(t * 1.7f) * 50.f * SwwHash(t, Sd), FMath::Sin(t * 1.7f) * 50.f * SwwHash(t, Sd), 32.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		}
		else if (Item == Items[1])
		{
			for (int32 i = 0; i < 6; ++i)
			{
				const float A = i * 1.05f + Sd;
				K.Add(SwwRustW(0.8f + 0.4f * SwwHash(i, Sd)), FVector(46.f + 20.f * SwwHash(i, 1), 38.f, 20.f + 10.f * SwwHash(i, 2)), FVector(FMath::Cos(A) * 45.f, FMath::Sin(A) * 45.f, 8.f), FRotator(10.f * SwwHash(i, 3), A * 57.f, 8.f * SwwHash(i, 4)));
			}
			K.Add(SwwWaterFilm(SwwRgb(138, 59, 30) * 0.25f), FVector(170.f, 140.f, 1.f), FVector(0, 0, 4.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		}
		else if (Item == Items[2])
		{
			// A cut bank: courses of turves with one cut face, a few lifted turves lying by it.
			for (int32 c = 0; c < 4; ++c) for (int32 b = 0; b < 3; ++b) K.Add(SwwPeatW(0.6f + 0.4f * SwwHash(c * 3 + b, Sd)), FVector(56.f, 40.f, 22.f), FVector(-30.f + 6.f * c, -45.f + 45.f * b, 11.f + 22.f * c), FRotator(0.f, 3.f * (b - 1), 0.f));
			for (int32 t = 0; t < 3; ++t) K.Add(SwwPeatW(0.8f), FVector(36.f, 16.f, 12.f), FVector(60.f + 20.f * t, -30.f + 30.f * t, 6.f), FRotator(0.f, 30.f * t, 0.f));
		}
		else
		{
			for (int32 l = 0; l < 3; ++l) K.Disc(SwwSaltW(1.f - 0.08f * l), FVector(0, 0, 4.f + 12.f * l), FVector::UpVector, 150.f - 40.f * l, 12.f);
			for (int32 c = 0; c < 8; ++c) K.Add(SwwSaltW(1.1f), FVector(14.f, 12.f, 10.f), FVector(FMath::Cos(c * 0.8f) * 60.f, FMath::Sin(c * 0.8f) * 60.f, 8.f), FRotator(20.f * SwwHash(c, Sd), c * 45.f, 0.f));
		}
		FWRBuildLook::Spawn(N, Look, Blocks, FLinearColor(0.2f, 0.18f, 0.15f), false);
	}
}

AWRSwampBoardwalk* UWRSwampWorld::BoardwalkGapNear(const FVector& At, int32& OutBay) const
{
	OutBay = INDEX_NONE;
	AWRSwampBoardwalk* Best = nullptr;
	float BestD = WRTune(TEXT("swampWorld.plankReach"), 300.f);
	for (TActorIterator<AWRSwampBoardwalk> It(GetWorld()); It; ++It)
	{
		const int32 G = It->GapNear(At, BestD);
		if (G == INDEX_NONE) continue;
		const float D = float(FVector::Dist(At, It->BayCentre(G)));
		if (D <= BestD) { BestD = D; Best = *It; OutBay = G; }
	}
	return Best;
}

AWRSwampBoardwalk* UWRSwampWorld::BoardwalkAt(const FVector& At, int32& OutBay) const
{
	OutBay = INDEX_NONE;
	for (TActorIterator<AWRSwampBoardwalk> It(GetWorld()); It; ++It)
	{
		const int32 B = It->BayAt(At);
		if (B != INDEX_NONE) { OutBay = B; return *It; }
	}
	return nullptr;
}

namespace WRSwampWorldLocal
{
	/** The Peat Cellar's wall a player stands at (shut), or null. */
	AWRSwampPeatWall* SwwPeatWallNear(const APawn* Pawn)
	{
		if (!Pawn) return nullptr;
		const float Reach = WRTune(TEXT("swampWorld.peatWallReach"), 400.f);
		for (TActorIterator<AWRSwampPeatWall> It(Pawn->GetWorld()); It; ++It)
		{
			if (It->bOpen) continue;
			const FVector Local = It->GetActorTransform().InverseTransformPosition(Pawn->GetActorLocation());
			if (Local.X > -60.f && Local.X < It->WallHalf.X + Reach && FMath::Abs(Local.Y) < It->WallHalf.Y + 50.f && Local.Z > -100.f && Local.Z < It->WallHalf.Z * 2.f + 100.f) return *It;
		}
		return nullptr;
	}

	bool SwwHoldsPick(const AWRPlayerController* PC)
	{
		const AWRPlayerState* PS = PC ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
		const UWRInventoryComponent* Inv = PS ? PS->GetInventory() : nullptr;
		if (!Inv) return false;
		const FWRToolDef* Tool = FWRGathering::FindTool(Inv->GetEquipped(EWREquipSlot::Weapon).ItemId);
		return Tool && Tool->Kind == TEXT("pick");
	}
}

bool UWRSwampWorld::PlankPrompt(const AWRPlayerController* PC, FString& OutTarget, FString& OutVerb)
{
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const UWRSwampWorld* SW = Get(PC);
	if (!Pawn || !SW) return false;
	int32 Bay = INDEX_NONE;
	if (AWRSwampBoardwalk* B = SW->BoardwalkGapNear(Pawn->GetActorLocation(), Bay))
	{
		const int32 Cost = (int32)WRTune(TEXT("swampWorld.plankWood"), 2.f);
		const uint8 S = B->Bays[Bay];
		OutTarget = S == AWRSwampBoardwalk::Rotten ? TEXT("A rotten plank") : TEXT("A gap in the boardwalk");
		const FVector Feet = Pawn->GetActorLocation() - FVector(0, 0, Pawn->GetSimpleCollisionHalfHeight());
		if (B->BayAt(Feet) == Bay) OutVerb = TEXT("Step off it to lay a new one");
		else OutVerb = S == AWRSwampBoardwalk::Rotten ? FString::Printf(TEXT("Replace it (%d wood)"), Cost) : FString::Printf(TEXT("Lay a plank (%d wood)"), Cost);
		return true;
	}
	if (SwwPeatWallNear(Pawn))
	{
		OutTarget = TEXT("A wall of cut peat");
		OutVerb = SwwHoldsPick(PC) ? TEXT("Dig at it") : TEXT("Dig (a pick in hand)");
		return true;
	}
	return false;
}

FString UWRSwampWorld::PlankCommand(AWRPlayerController* PC)
{
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const UWRSwampWorld* SW = Get(PC);
	if (!Pawn || !SW) return TEXT("Nothing to mend here");
	int32 Bay = INDEX_NONE;
	if (AWRSwampBoardwalk* B = SW->BoardwalkGapNear(Pawn->GetActorLocation(), Bay)) return B->Mend(PC, Bay);
	if (SwwPeatWallNear(Pawn))
	{
		if (!SwwHoldsPick(PC)) return TEXT("Peat this packed needs a pick");
		for (const FPeat& P : SW->PeatWalls)
		{
			AWRResourceNode* Node = P.Node.Get();
			AWRCharacterBase* Digger = Cast<AWRCharacterBase>(Pawn);
			if (!Node || !Digger || !P.Wall.IsValid() || P.Wall->bOpen) continue;
			if (FVector::Dist(Node->GetActorLocation(), Pawn->GetActorLocation()) > WRTune(TEXT("swampWorld.peatWallReach"), 400.f) + 400.f) continue;
			if (Node->IsDepleted()) return TEXT("It's already giving way");
			Node->Harvest(Digger, false);
			WRFx::Broadcast(Pawn, TEXT("gather.stone"), Node->GetActorLocation() + FVector(0, 0, 80.f), FRotator::ZeroRotator, 1.2f, NAME_None);
			return Node->IsDepleted() ? TEXT("The peat gives and the wall falls in") : TEXT("You cut into the peat");
		}
		return TEXT("Nothing to dig here");
	}
	return TEXT("Nothing to mend here");
}
