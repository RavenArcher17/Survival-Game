#include "WRRoundIsland.h"
#include "Wildreach.h"
#include "WRBuildLook.h"
#include "WRLand.h"
#include "WRSea.h"
#include "WRTuning.h"
#include "WRWater.h"
#include "WRGameState.h"
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
	PrimaryActorTick.TickInterval = 0.25f;   // only the lake's day and night look
	bReplicates = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Static);
}

void AWRRoundIsland::BeginPlay()
{
	Super::BeginPlay();
	BuildCity();
	BuildWater();
}

void AWRRoundIsland::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
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
