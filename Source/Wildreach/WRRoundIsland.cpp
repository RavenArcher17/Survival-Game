#include "WRRoundIsland.h"
#include "Wildreach.h"
#include "WRBuildLook.h"
#include "WRLand.h"
#include "WRSea.h"
#include "WRTuning.h"
#include "WRWater.h"
#include "WRGameState.h"
#include "WRSocial.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"

namespace WRRoundIslandImpl
{
	float At(const TArray<TSharedPtr<FJsonValue>>& Row, int32 i) { return Row.IsValidIndex(i) ? float(Row[i]->AsNumber()) : 0.f; }

	/** The land of this map (the round island's ground and its lake), or null. */
	const FWRLandDef* MapLand()
	{
		for (const FWRLandDef& D : WRLand::All())
		{
			if (!D.Map.IsEmpty() && D.Map == WRLand::CurrentMap()) return &D;
		}
		return nullptr;
	}
}

// ================================================================================================ the subsystem

bool UWRRoundIslandSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UWRRoundIslandSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!WRRoundIslandImpl::MapLand()) return;
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	InWorld.SpawnActor<AWRRoundIsland>(AWRRoundIsland::StaticClass(), FTransform::Identity, P);
}

// ================================================================================================ the actor

AWRRoundIsland::AWRRoundIsland()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;   // the lake's day and night look, the low plants round the view
	bReplicates = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Static);
}

void AWRRoundIsland::BeginPlay()
{
	Super::BeginPlay();
	BuildCity();
	BuildWater();
	BuildSafeZone();
	if (GetNetMode() != NM_DedicatedServer)
	{
		// The grass: thin blades of the building look's tinted material (each blade its own green from custom data).
		BladeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		BladeMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Building/M_BuildTinted.M_BuildTinted"));
	}
}

void AWRRoundIsland::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	StepCover();
	if (!LakeMat || LakeDay.Num() == 0) return;
	const AWRGameState* State = AWRGameState::Get(this);
	const float Daylight = State ? State->GetDaylight() : 1.f;
	if (FMath::Abs(Daylight - LastDaylight) < 0.01f) return;
	LastDaylight = Daylight;
	const float Dim = FMath::Lerp(FWRTuning::Get().Num(TEXT("sea.nightDim"), 0.08f), 1.f, Daylight);
	for (const TPair<FName, float>& P : LakeDay) LakeMat->SetScalarParameterValue(P.Key, P.Value * Dim);
}

void AWRRoundIsland::BuildCity()
{
	using namespace WRRoundIslandImpl;
	const TSharedPtr<FJsonObject> J = FWRTuning::LoadDataFile(TEXT("hearthmoot.json"));
	const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
	if (!J.IsValid() || !J->TryGetArrayField(TEXT("blocks"), Rows))
	{
		UE_LOG(LogWildreach, Warning, TEXT("ROUND no Data/hearthmoot.json (node Tools/terrain/hearthmoot.mjs): no Hearthmoot"));
		return;
	}
	struct FPal { FLinearColor C; float Metal, Rough, Glow, Pattern; };
	TArray<FPal> Pal;
	const TArray<TSharedPtr<FJsonValue>>* PalRows = nullptr;
	if (J->TryGetArrayField(TEXT("palette"), PalRows))
	{
		for (const TSharedPtr<FJsonValue>& V : *PalRows)
		{
			const TArray<TSharedPtr<FJsonValue>>& R = V->AsArray();
			Pal.Add({ FLinearColor(FColor(uint8(At(R, 0)), uint8(At(R, 1)), uint8(At(R, 2)))), At(R, 3), At(R, 4), At(R, 5), At(R, 6) });
		}
	}
	// Every block as the building look draws a built piece's: its shade from where it stands, its grain along its
	// longest side; coloured ones (roofs, banners, iron, glass, the hearth's fire) in the tinted look.
	TArray<FWRLookBlock> Blocks;
	Blocks.Reserve(Rows->Num());
	for (const TSharedPtr<FJsonValue>& V : *Rows)
	{
		const TArray<TSharedPtr<FJsonValue>>& R = V->AsArray();
		FWRLookBlock B;
		B.Mesh = static_cast<EWRLookMesh>(FMath::Clamp(int32(At(R, 0)), 0, 2));
		B.Mat = static_cast<EWRLookMat>(FMath::Clamp(int32(At(R, 1)), 0, 4));
		B.Centre = FVector(At(R, 2), At(R, 3), At(R, 4));
		B.Size = FVector(At(R, 5), At(R, 6), At(R, 7)).ComponentMax(FVector(1.f));
		B.Rot = FRotator(At(R, 9), At(R, 8), At(R, 10));
		const int32 PalIdx = int32(At(R, 11));
		if (Pal.IsValidIndex(PalIdx))
		{
			if (B.Mat == EWRLookMat::Cloth || B.Mat == EWRLookMat::Wood) B.Mat = EWRLookMat::Tinted;
			const FPal& P = Pal[PalIdx];
			B.Color = P.C;
			B.Metal = P.Metal;
			B.Rough = P.Rough;
			B.Glow = P.Glow;
			B.Pattern = P.Pattern;
		}
		B.Tone = FWRBuildLook::Hash01(B.Centre, static_cast<int32>(B.Mat) * 7 + 1);
		const FVector Long = B.Mesh != EWRLookMesh::Cube ? FVector::ZAxisVector
			: B.Size.X >= B.Size.Y && B.Size.X >= B.Size.Z ? FVector::XAxisVector : B.Size.Y >= B.Size.Z ? FVector::YAxisVector : FVector::ZAxisVector;
		const FVector Dir = B.Rot.RotateVector(Long).GetAbs();
		B.Axis = Dir.Z > 0.7f ? 2.f : Dir.X >= Dir.Y ? 0.f : 1.f;
		Blocks.Add(B);
	}
	const int32 Parts = FWRBuildLook::Spawn(this, RootComponent, Blocks, FLinearColor(0.3f, 0.1f, 0.08f), false);
	// What you bump into: one hidden box per wall length, tower, house, hall (every machine, so movement agrees).
	const TArray<TSharedPtr<FJsonValue>>* SolidRows = nullptr;
	int32 NumSolids = 0;
	if (J->TryGetArrayField(TEXT("solids"), SolidRows) && SolidRows->Num())
	{
		Solids = NewObject<UInstancedStaticMeshComponent>(this, TEXT("Solids"));
		Solids->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
		Solids->SetupAttachment(RootComponent);
		Solids->SetVisibility(false);
		Solids->SetCastShadow(false);
		Solids->SetCollisionProfileName(TEXT("BlockAll"));
		Solids->SetCollisionObjectType(ECC_WorldStatic);
		Solids->SetCanEverAffectNavigation(false);
		Solids->RegisterComponent();
		TArray<FTransform> T;
		T.Reserve(SolidRows->Num());
		for (const TSharedPtr<FJsonValue>& V : *SolidRows)
		{
			const TArray<TSharedPtr<FJsonValue>>& R = V->AsArray();
			T.Emplace(FRotator(0.f, At(R, 6), 0.f), FVector(At(R, 0), At(R, 1), At(R, 2)), FVector(At(R, 3), At(R, 4), At(R, 5)).ComponentMax(FVector(1.f)) / 100.f);
		}
		Solids->AddInstances(T, false, false, false);
		NumSolids = T.Num();
	}
	UE_LOG(LogWildreach, Log, TEXT("ROUND Hearthmoot: %d blocks in %d parts, %d solids"), Blocks.Num(), Parts, NumSolids);
	BuildPlants(J);
}

void AWRRoundIsland::BuildPlants(const TSharedPtr<FJsonObject>& J)
{
	using namespace WRRoundIslandImpl;
	const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
	if (!J.IsValid() || !J->TryGetArrayField(TEXT("plants"), Rows) || GetNetMode() == NM_DedicatedServer) return;
	// Each kind's meshes: Electric Dreams' hornbeams and elderberries where the kit is installed (the Ancient forest's),
	// else the island's own birch and the shrubs pack.
	const auto Load = [](const FString& Path) { return LoadObject<UStaticMesh>(nullptr, *Path); };
	TArray<UStaticMesh*> Kinds[3];
	for (int32 i = 1; i <= 8; ++i) if (UStaticMesh* M = Load(FString::Printf(TEXT("/Game/Megascans/3D_Plants/EuropeanHornbeam/Geometry/SimpleWind/SM_EuropeanHornbeam_Forest_%02d.SM_EuropeanHornbeam_Forest_%02d"), i, i))) Kinds[0].Add(M);
	for (int32 i = 1; i <= 6; ++i) if (UStaticMesh* M = Load(FString::Printf(TEXT("/Game/Megascans/3D_Plants/EuropeanHornbeam/Geometry/SimpleWind/SM_EuropeanHornbeam_Sapling_%02d.SM_EuropeanHornbeam_Sapling_%02d"), i, i))) Kinds[1].Add(M);
	for (const int32 i : { 1, 2, 3, 4, 5, 6, 10, 11, 12, 13 }) if (UStaticMesh* M = Load(FString::Printf(TEXT("/Game/Megascans/3D_Plants/Elderberry/SM_Elderberry_%02d.SM_Elderberry_%02d"), i, i))) Kinds[2].Add(M);
	float FallbackScale[3] = { 1.f, 1.f, 1.f };
	if (Kinds[0].Num() == 0) { if (UStaticMesh* M = Load(TEXT("/Game/Props/BirchA/birch_a_textured/StaticMeshes/birch_a_textured.birch_a_textured"))) Kinds[0].Add(M); }
	if (Kinds[1].Num() == 0) { Kinds[1] = Kinds[0]; FallbackScale[1] = 0.25f; }
	if (Kinds[2].Num() == 0) { if (UStaticMesh* M = Load(TEXT("/Game/GV_FreeShrubsPack/Meshes/Shrubs/Wind/Shrub_A/GV_Vol7_Shrub_A_full_type1.GV_Vol7_Shrub_A_full_type1"))) Kinds[2].Add(M); }
	const FWRTuning& T = FWRTuning::Get();
	const float Cull[3] = { T.Num(TEXT("roundIsland.treeCull"), 350000.f), T.Num(TEXT("roundIsland.saplingCull"), 120000.f), T.Num(TEXT("roundIsland.shrubCull"), 90000.f) };
	TMap<UStaticMesh*, UHierarchicalInstancedStaticMeshComponent*> Parts;
	TMap<UStaticMesh*, TArray<FTransform>> Xf;
	TMap<UStaticMesh*, int32> KindOf;
	for (const TSharedPtr<FJsonValue>& V : *Rows)
	{
		const TArray<TSharedPtr<FJsonValue>>& R = V->AsArray();
		const int32 Kind = FMath::Clamp(int32(At(R, 0)), 0, 2);
		if (Kinds[Kind].Num() == 0) continue;
		UStaticMesh* M = Kinds[Kind][int32(At(R, 1)) % Kinds[Kind].Num()];
		Xf.FindOrAdd(M).Emplace(FRotator(0.f, At(R, 5), 0.f), FVector(At(R, 2), At(R, 3), At(R, 4)), FVector(At(R, 6) * FallbackScale[Kind]));
		KindOf.Add(M, Kind);
	}
	int32 Count = 0;
	for (TPair<UStaticMesh*, TArray<FTransform>>& P : Xf)
	{
		UHierarchicalInstancedStaticMeshComponent* H = NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
		H->SetStaticMesh(P.Key);
		H->SetupAttachment(RootComponent);
		H->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		H->SetCanEverAffectNavigation(false);
		const int32 Kind = KindOf.FindRef(P.Key);
		H->SetCullDistances(0, int32(Cull[Kind]));
		H->SetCastShadow(Kind != 2 || T.Num(TEXT("roundIsland.shrubShadows"), 1.f) > 0.f);
		H->RegisterComponent();
		H->AddInstances(P.Value, false, true, false);
		Count += P.Value.Num();
	}
	UE_LOG(LogWildreach, Log, TEXT("ROUND plants: %d in %d meshes (trees %d kinds, saplings %d, shrubs %d)"), Count, Xf.Num(), Kinds[0].Num(), Kinds[1].Num(), Kinds[2].Num());
}

void AWRRoundIsland::BuildSafeZone()
{
	// The meadow is the Commons (Raven's answers: fully safe, nothing owned): one safe zone over it, from the server.
	if (GetNetMode() == NM_Client) return;
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AWRSafeZone* Z = GetWorld()->SpawnActor<AWRSafeZone>(AWRSafeZone::StaticClass(), FTransform(FVector(0.f, 0.f, 4100.f)), P))
	{
		Z->Radius = FWRTuning::Get().Num(TEXT("roundIsland.safeRadius"), 130000.f);
		UE_LOG(LogWildreach, Log, TEXT("ROUND the meadow is safe: %.0f m round Hearthmoot"), Z->Radius / 100.f);
	}
}

void AWRRoundIsland::StepCover()
{
	// Low plants in 24 m cells round the view (roundIsland.coverCell / coverRange / coverPerCell), on the meadow and the
	// yards (zone letters M, Y, C) where the ground is plain (no road, field, sand or bed), never in the lake.
	using namespace WRRoundIslandImpl;
	if (!BladeMesh || !BladeMat) return;
	const FWRLandDef* D = MapLand();
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!D || !D->bHeightfield || !PC || !PC->PlayerCameraManager) return;
	const FWRTuning& T = FWRTuning::Get();
	const float Cell = T.Num(TEXT("roundIsland.coverCell"), 24.f), Range = T.Num(TEXT("roundIsland.coverRange"), 75.f);
	const int32 PerCell = int32(T.Num(TEXT("roundIsland.coverPerCell"), 180.f));
	const FVector2D View = D->ToMap(PC->PlayerCameraManager->GetCameraLocation());
	const float ViewZ = PC->PlayerCameraManager->GetCameraLocation().Z / 100.f;
	TSet<FIntPoint> Want;
	// High above (a camera over the meadow) the low plants wouldn't show: none.
	if (ViewZ - D->Height(View) < T.Num(TEXT("roundIsland.coverMaxHeight"), 60.f))
	{
		const int32 C0x = FMath::FloorToInt((View.X - Range) / Cell), C1x = FMath::FloorToInt((View.X + Range) / Cell);
		const int32 C0y = FMath::FloorToInt((View.Y - Range) / Cell), C1y = FMath::FloorToInt((View.Y + Range) / Cell);
		for (int32 y = C0y; y <= C1y; ++y) for (int32 x = C0x; x <= C1x; ++x)
		{
			if (FVector2D::Distance(View, (FVector2D(x, y) + FVector2D(0.5f, 0.5f)) * Cell) < Range + Cell * 0.71f) Want.Add(FIntPoint(x, y));
		}
	}
	for (auto It = CoverCells.CreateIterator(); It; ++It)
	{
		if (Want.Contains(It.Key())) continue;
		for (UInstancedStaticMeshComponent* C : It.Value()) if (C) C->DestroyComponent();
		It.RemoveCurrent();
	}
	int32 Made = 0;
	for (const FIntPoint& C : Want)
	{
		if (CoverCells.Contains(C) || Made >= 8) continue;
		++Made;
		TArray<TObjectPtr<UInstancedStaticMeshComponent>>& Parts = CoverCells.Add(C);
		TArray<FTransform> Xf;
		TArray<FLinearColor> Col;
		FRandomStream Rng(HashCombine(GetTypeHash(C.X * 92821), GetTypeHash(C.Y * 68917)));
		const float BladeW = T.Num(TEXT("roundIsland.grassBladeWidth"), 2.2f), BladeT = 0.8f;
		const FVector2D BladeH(T.Num(TEXT("roundIsland.grassMin"), 20.f), T.Num(TEXT("roundIsland.grassMax"), 50.f));
		for (int32 n = 0; n < PerCell; ++n)
		{
			const FVector2D M = (FVector2D(C) + FVector2D(Rng.FRand(), Rng.FRand())) * Cell;
			const TCHAR L = D->GridLetter(M);
			if (L != TEXT('M') && L != TEXT('Y') && L != TEXT('C')) continue;
			const int32 I = FMath::RoundToInt((M.X - D->HfOrigin.X) / D->HfStep), J = FMath::RoundToInt((M.Y - D->HfOrigin.Y) / D->HfStep);
			if (D->HfSurfaceAt(I, J) != 0) continue;
			// A tuft: blades fanned out from one root, leaning away from it, mostly green, now and then a dry one.
			const FVector Root = D->ToWorld(M, D->Height(M) - 0.02f);
			const int32 Blades = 9 + Rng.RandHelper(7);
			const FLinearColor Green(Rng.FRandRange(0.045f, 0.09f), Rng.FRandRange(0.15f, 0.24f), Rng.FRandRange(0.025f, 0.05f));
			for (int32 b = 0; b < Blades; ++b)
			{
				const float H = Rng.FRandRange(BladeH.X, BladeH.Y), Yaw = Rng.FRand() * 360.f, Lean = Rng.FRandRange(6.f, 30.f);
				const FRotator Rot(Lean, Yaw, 0.f);
				const FVector Off(Rng.FRandRange(-6.f, 6.f), Rng.FRandRange(-6.f, 6.f), 0.f);
				Xf.Emplace(Rot, Root + Off + Rot.RotateVector(FVector(0.f, 0.f, H * 0.5f)), FVector(BladeW, BladeT, H) / 100.f);
				Col.Add(Rng.FRand() < 0.14f ? FLinearColor(0.2f, 0.19f, 0.07f) : Green * Rng.FRandRange(0.85f, 1.2f));
			}
		}
		if (Xf.Num())
		{
			UInstancedStaticMeshComponent* P = NewObject<UInstancedStaticMeshComponent>(this);
			P->SetStaticMesh(BladeMesh);
			P->SetMaterial(0, BladeMat);
			P->SetupAttachment(RootComponent);
			P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			P->SetCanEverAffectNavigation(false);
			P->SetCastShadow(false);
			P->SetCullDistances(0, int32((Range + Cell) * 100.f));
			P->SetNumCustomDataFloats(10);
			P->RegisterComponent();
			P->AddInstances(Xf, false, false, false);
			for (int32 i = 0; i < Xf.Num(); ++i)
			{
				// M_BuildTinted: shade, grain axis, brightness, colour, metallic, roughness, glow, pattern (smooth).
				const float Data[10] = { 0.5f, 2.f, 1.f, Col[i].R, Col[i].G, Col[i].B, 0.f, 0.9f, 0.f, 3.f };
				P->SetCustomData(i, Data, i == Xf.Num() - 1);
			}
			Parts.Add(P);
		}
	}
}

void AWRRoundIsland::BuildWater()
{
	using namespace WRRoundIslandImpl;
	const FWRLandDef* D = MapLand();
	const TSharedPtr<FJsonObject> L = D ? D->Section(TEXT("lake")) : nullptr;
	if (!L.IsValid()) return;
	const FWRTuning& T = FWRTuning::Get();
	const float Level = float(L->GetNumberField(TEXT("level"))) * 100.f;
	TArray<FVector2D> Outline;
	const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
	if (L->TryGetArrayField(TEXT("outline"), A)) for (const TSharedPtr<FJsonValue>& V : *A) Outline.Add(WRLand::Vec2(V) * 100.f);
	FVector2D Mid = FVector2D::ZeroVector;
	if (L->TryGetArrayField(TEXT("at"), A)) Mid = WRLand::Vec2(L->TryGetField(TEXT("at"))) * 100.f;
	Water = NewObject<UProceduralMeshComponent>(this, TEXT("Water"));
	Water->SetupAttachment(RootComponent);
	Water->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Water->SetCastShadow(false);
	Water->SetCanEverAffectNavigation(false);
	Water->RegisterComponent();
	const bool bLook = GetNetMode() != NM_DedicatedServer;
	// The lake: a fan from its middle to its shore (it is round enough from there), a little past the shore so the
	// bank meets the water rather than a gap.
	if (Outline.Num() >= 3 && bLook)
	{
		TArray<FVector> V;
		TArray<int32> Tri;
		TArray<FVector> N;
		TArray<FVector2D> UV;
		TArray<FLinearColor> Col;
		TArray<FProcMeshTangent> Tan;
		V.Add(FVector(Mid, Level));
		for (const FVector2D& P : Outline) V.Add(FVector(Mid + (P - Mid) * 1.04f, Level));
		for (const FVector& P : V) { N.Add(FVector::UpVector); UV.Add(FVector2D(P.X, P.Y) / 1500.f); Col.Add(FLinearColor::White); Tan.Add(FProcMeshTangent(1.f, 0.f, 0.f)); }
		// Both windings: the water faces up to you on the shore and down to you under it, whichever way the outline runs.
		for (int32 i = 0; i < Outline.Num(); ++i) Tri.Append({ 0, 1 + (i + 1) % Outline.Num(), 1 + i, 0, 1 + i, 1 + (i + 1) % Outline.Num() });
		Water->CreateMeshSection_LinearColor(0, V, Tri, N, UV, Col, Tan, false);
		// The sea's own water (its reflections and ripples) in a fresh lake's colours (roundIsland.lakeColour /
		// lakeColourDeep, sRGB; lakeOpacity).
		if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, *T.Str(TEXT("roundIsland.lakeMaterial"), *T.Str(TEXT("sea.material"), TEXT("/Game/Sea/MI_Ocean.MI_Ocean")))))
		{
			const auto Rgb = [&T](const TCHAR* Key, const TCHAR* Fallback)
			{
				TArray<FString> P;
				T.Str(Key, Fallback).ParseIntoArray(P, TEXT(","));
				return P.Num() >= 3 ? FLinearColor(FColor(uint8(FCString::Atoi(*P[0])), uint8(FCString::Atoi(*P[1])), uint8(FCString::Atoi(*P[2])))) : FLinearColor::Blue;
			};
			LakeMat = UMaterialInstanceDynamic::Create(Base, this);
			LakeMat->SetVectorParameterValue(TEXT("Colour"), Rgb(TEXT("roundIsland.lakeColour"), TEXT("44,104,104")));
			LakeMat->SetVectorParameterValue(TEXT("ColourDeep"), Rgb(TEXT("roundIsland.lakeColourDeep"), TEXT("12,44,52")));
			LakeMat->SetVectorParameterValue(TEXT("ColourVariation"), Rgb(TEXT("roundIsland.lakeColourVariation"), TEXT("30,80,84")));
			if (const TSharedPtr<FJsonObject> Sc = T.Object(TEXT("sea.scalars"))) for (const TPair<FString, TSharedPtr<FJsonValue>>& P : Sc->Values) LakeMat->SetScalarParameterValue(FName(*P.Key), P.Value->AsNumber());
			if (const TSharedPtr<FJsonObject> Dm = T.Object(TEXT("sea.dim"))) for (const TPair<FString, TSharedPtr<FJsonValue>>& P : Dm->Values) LakeDay.Add(FName(*P.Key), float(P.Value->AsNumber()));
			LakeMat->SetScalarParameterValue(TEXT("Opacity"), T.Num(TEXT("roundIsland.lakeOpacity"), 0.8f));
			Water->SetMaterial(0, LakeMat);
		}
	}
	// The stream: a ribbon of water down its bed from the lake to the sea (each point's level from the land file).
	const TArray<TSharedPtr<FJsonValue>>* S = nullptr;
	if (bLook && L->TryGetArrayField(TEXT("stream"), S) && S->Num() >= 2)
	{
		TArray<FVector> Pts;
		for (const TSharedPtr<FJsonValue>& V : *S)
		{
			const TArray<TSharedPtr<FJsonValue>>& R = V->AsArray();
			Pts.Add(FVector(At(R, 0), At(R, 1), At(R, 2)) * 100.f);
		}
		const float Half = T.Num(TEXT("roundIsland.streamWidth"), 700.f) * 0.5f;
		TArray<FVector> V, N;
		TArray<int32> Tri;
		TArray<FVector2D> UV;
		TArray<FLinearColor> Col;
		TArray<FProcMeshTangent> Tan;
		float Along = 0.f;
		for (int32 i = 0; i < Pts.Num(); ++i)
		{
			if (i) Along += FVector::Dist2D(Pts[i], Pts[i - 1]);
			const FVector Dir = (Pts[FMath::Min(i + 1, Pts.Num() - 1)] - Pts[FMath::Max(i - 1, 0)]).GetSafeNormal2D();
			const FVector Side(-Dir.Y, Dir.X, 0.f);
			for (int32 k = 0; k < 3; ++k)
			{
				V.Add(Pts[i] + Side * ((k - 1) * Half));
				N.Add(FVector::UpVector);
				UV.Add(FVector2D(Along / 800.f, k * 0.5f));
				Col.Add(FLinearColor::White);
				Tan.Add(FProcMeshTangent(Dir, false));
			}
		}
		for (int32 i = 0; i + 1 < Pts.Num(); ++i) for (int32 k = 0; k < 2; ++k)
		{
			const int32 a = i * 3 + k, b = a + 1, c = a + 3, d = a + 4;
			Tri.Append({ a, c, b, b, c, d });
		}
		Water->CreateMeshSection_LinearColor(1, V, Tri, N, UV, Col, Tan, false);
		if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, *T.Str(TEXT("roundIsland.streamMaterial"), TEXT("/Game/WaterMaterials/Materials/M_River.M_River"))))
		{
			StreamMat = UMaterialInstanceDynamic::Create(Base, this);
			StreamMat->SetVectorParameterValue(TEXT("Colour"), FLinearColor(0.05f, 0.085f, 0.07f));
			StreamMat->SetVectorParameterValue(TEXT("ColourDeep"), FLinearColor(0.012f, 0.03f, 0.025f));
			StreamMat->SetScalarParameterValue(TEXT("River_Speed"), T.Num(TEXT("roundIsland.streamSpeed"), 0.5f));
			Water->SetMaterial(1, StreamMat);
		}
	}
	// The lake you swim in: its 4 m cells inside the shore, merged into boxes, from its bed up to its surface.
	if (Outline.Num() >= 3)
	{
		FBox2D Box(ForceInit);
		for (const FVector2D& P : Outline) Box += P;
		FWRSeaMask Mask;
		Mask.Cell = 400.f;
		Mask.Origin = Box.Min;
		Mask.Width = FMath::CeilToInt((Box.Max.X - Box.Min.X) / Mask.Cell);
		Mask.Height = FMath::CeilToInt((Box.Max.Y - Box.Min.Y) / Mask.Cell);
		Mask.Sea.SetNumZeroed(Mask.Width * Mask.Height);
		int32 Cells = 0;
		for (int32 y = 0; y < Mask.Height; ++y) for (int32 x = 0; x < Mask.Width; ++x)
		{
			const FVector2D C = Mask.Origin + FVector2D(x + 0.5f, y + 0.5f) * Mask.Cell;
			bool bIn = false;
			for (int32 i = 0, j = Outline.Num() - 1; i < Outline.Num(); j = i++)
			{
				if ((Outline[i].Y > C.Y) != (Outline[j].Y > C.Y) && C.X < (Outline[j].X - Outline[i].X) * (C.Y - Outline[i].Y) / (Outline[j].Y - Outline[i].Y) + Outline[i].X) bIn = !bIn;
			}
			Mask.Sea[y * Mask.Width + x] = bIn ? 1 : 0;
			Cells += bIn ? 1 : 0;
		}
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AWRWaterVolume* Vol = GetWorld()->SpawnActor<AWRWaterVolume>(AWRWaterVolume::StaticClass(), FTransform::Identity, P))
		{
			const int32 Boxes = Vol->Build(Mask, Level + T.Num(TEXT("roundIsland.lakeSurfaceOffset"), 0.f), Level - 900.f, 0.f, false);
			UE_LOG(LogWildreach, Log, TEXT("ROUND the lake: %d cells, %d swim boxes, level %.0f cm"), Cells, Boxes, Level);
		}
	}
}
