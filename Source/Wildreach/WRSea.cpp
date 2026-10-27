#include "WRSea.h"
#include "Wildreach.h"
#include "WRGameState.h"
#include "WRSky.h"
#include "WRTuning.h"
#include "ProceduralMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"

// ---------------------------------------------------------------- mask and quadtree

bool FWRSeaMask::Parse(const FJsonObject& Json)
{
	Width = static_cast<int32>(Json.GetNumberField(TEXT("width")));
	Height = static_cast<int32>(Json.GetNumberField(TEXT("height")));
	Cell = Json.GetNumberField(TEXT("cell"));
	Origin = FVector2D(Json.GetNumberField(TEXT("originX")), Json.GetNumberField(TEXT("originY")));
	const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
	if (Width <= 0 || Height <= 0 || Cell <= 0.f || !Json.TryGetArrayField(TEXT("rows"), Rows) || Rows->Num() != Height)
	{
		return false;
	}
	Sea.SetNumZeroed(Width * Height);
	for (int32 Y = 0; Y < Height; ++Y)
	{
		TArray<FString> Runs;
		(*Rows)[Y]->AsString().ParseIntoArray(Runs, TEXT(" "));
		int32 X = 0;
		for (int32 r = 0; r < Runs.Num(); ++r)
		{
			const int32 Len = FCString::Atoi(*Runs[r]);
			for (int32 k = 0; k < Len && X < Width; ++k, ++X)
			{
				Sea[Y * Width + X] = (r % 2 == 0) ? 1 : 0;  // runs alternate sea, land, sea...
			}
		}
		if (X != Width)
		{
			return false;
		}
	}
	return true;
}

void FWRSeaMask::ComputeDistance()
{
	// Two-pass chamfer distance (1 along, 1.414 across) from every sea cell to the nearest land cell.
	const float Far = 1e6f;
	Dist.SetNumUninitialized(Width * Height);
	for (int32 i = 0; i < Width * Height; ++i) Dist[i] = Sea[i] ? Far : 0.f;
	const auto Relax = [this](int32 X, int32 Y, int32 DX, int32 DY, float Cost, float& Best)
	{
		const int32 NX = X + DX, NY = Y + DY;
		if (NX >= 0 && NY >= 0 && NX < Width && NY < Height) Best = FMath::Min(Best, Dist[NY * Width + NX] + Cost);
	};
	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			float& D = Dist[Y * Width + X];
			Relax(X, Y, -1, 0, 1.f, D); Relax(X, Y, 0, -1, 1.f, D); Relax(X, Y, -1, -1, 1.414f, D); Relax(X, Y, 1, -1, 1.414f, D);
		}
	}
	for (int32 Y = Height - 1; Y >= 0; --Y)
	{
		for (int32 X = Width - 1; X >= 0; --X)
		{
			float& D = Dist[Y * Width + X];
			Relax(X, Y, 1, 0, 1.f, D); Relax(X, Y, 0, 1, 1.f, D); Relax(X, Y, 1, 1, 1.414f, D); Relax(X, Y, -1, 1, 1.414f, D);
		}
	}
}

float FWRSeaMask::DistanceAt(int32 X, int32 Y) const
{
	const int32 CX = FMath::Clamp(X, 0, Width - 1), CY = FMath::Clamp(Y, 0, Height - 1);
	const float Edge = Dist.IsValidIndex(CY * Width + CX) ? Dist[CY * Width + CX] : 0.f;
	return Edge + FMath::Sqrt(static_cast<float>(FMath::Square(X - CX) + FMath::Square(Y - CY)));
}

namespace WRSeaDetail
{
	/** The leaf covering a cell, if any (dropped land and outside the root: none). */
	static const FWRSeaLeaf* FindLeaf(const TSet<FWRSeaLeaf>& Leaves, int32 Start, int32 MaxCells, int32 CX, int32 CY)
	{
		for (int32 S = 1; S <= MaxCells; S *= 2)
		{
			const FWRSeaLeaf Key{ Start + FMath::FloorToInt((CX - Start) / static_cast<float>(S)) * S, Start + FMath::FloorToInt((CY - Start) / static_cast<float>(S)) * S, S };
			if (const FWRSeaLeaf* Found = Leaves.Find(Key)) return Found;
		}
		return nullptr;
	}

	/** True if every cell of the square is inside the mask and land (nothing of the sea to draw there). */
	static bool AllLand(const FWRSeaMask& Mask, int32 X, int32 Y, int32 Size)
	{
		for (int32 j = Y; j < Y + Size; ++j) for (int32 i = X; i < X + Size; ++i) if (Mask.IsSea(i, j)) return false;
		return true;
	}

	/** The cells along each side just outside a square (every cell for small squares, every half for big ones). */
	static void ForEachOutside(const FWRSeaLeaf& L, TFunctionRef<void(int32 Side, int32 CX, int32 CY)> Fn)
	{
		const int32 Step = L.Size <= 64 ? 1 : L.Size / 2;
		for (int32 k = 0; k < L.Size; k += Step)
		{
			Fn(0, L.X + k, L.Y - 1);          // south (-Y)
			Fn(1, L.X + L.Size, L.Y + k);     // east (+X)
			Fn(2, L.X + k, L.Y + L.Size);     // north
			Fn(3, L.X - 1, L.Y + k);          // west
		}
	}
}

TArray<FWRSeaLeaf> FWRSeaRules::BuildLeaves(const FWRSeaMask& Mask, int32 RootCells, float Falloff, int32 MaxCells)
{
	using namespace WRSeaDetail;
	const int32 Start = RootStart(Mask, RootCells);
	const auto Desired = [Falloff, MaxCells](float D)
	{
		// The biggest power of two not over D / Falloff (at least one cell).
		const uint32 V = static_cast<uint32>(FMath::Max(1.f, D / FMath::Max(0.1f, Falloff)));
		return FMath::Clamp(static_cast<int32>(1u << FMath::FloorLog2(V)), 1, MaxCells);
	};
	TSet<FWRSeaLeaf> Leaves;
	TArray<FWRSeaLeaf> Stack = { { Start, Start, RootCells } };
	while (Stack.Num() > 0)
	{
		const FWRSeaLeaf N = Stack.Pop(EAllowShrinking::No);
		const bool bOverlaps = N.X < Mask.Width && N.Y < Mask.Height && N.X + N.Size > 0 && N.Y + N.Size > 0;
		bool bSplit = false;
		if (!bOverlaps)
		{
			// Open sea off the grid: as coarse as its distance from the island allows.
			const int32 NX = N.X >= Mask.Width ? N.X : N.X + N.Size <= 0 ? N.X + N.Size - 1 : FMath::Clamp(0, N.X, N.X + N.Size - 1);
			const int32 NY = N.Y >= Mask.Height ? N.Y : N.Y + N.Size <= 0 ? N.Y + N.Size - 1 : FMath::Clamp(0, N.Y, N.Y + N.Size - 1);
			bSplit = N.Size > Desired(Mask.DistanceAt(NX, NY));
		}
		else if (N.Size > 64)
		{
			bSplit = true;
		}
		else
		{
			float MinDist = TNumericLimits<float>::Max();
			bool bLand = true;
			for (int32 j = N.Y; j < N.Y + N.Size; ++j)
			{
				for (int32 i = N.X; i < N.X + N.Size; ++i)
				{
					bLand &= !Mask.IsSea(i, j);
					MinDist = FMath::Min(MinDist, Mask.IsSea(i, j) ? Mask.DistanceAt(i, j) : 0.f);
				}
			}
			if (bLand)
			{
				continue;  // dry land: no sea here
			}
			bSplit = N.Size > 1 && N.Size > Desired(MinDist);
		}
		if (bSplit && N.Size > 1)
		{
			const int32 H = N.Size / 2;
			Stack.Append({ { N.X, N.Y, H }, { N.X + H, N.Y, H }, { N.X, N.Y + H, H }, { N.X + H, N.Y + H, H } });
		}
		else
		{
			Leaves.Add(N);
		}
	}

	// Balance: a square next to one less than half its size splits, until neighbours differ by one size at most.
	TArray<FWRSeaLeaf> Work = Leaves.Array();
	while (Work.Num() > 0)
	{
		const FWRSeaLeaf L = Work.Pop(EAllowShrinking::No);
		if (!Leaves.Contains(L) || L.Size == 1)
		{
			continue;
		}
		bool bTooBig = false;
		ForEachOutside(L, [&](int32, int32 CX, int32 CY)
		{
			const FWRSeaLeaf* Nb = bTooBig ? nullptr : FindLeaf(Leaves, Start, MaxCells, CX, CY);
			bTooBig |= Nb && Nb->Size * 2 < L.Size;
		});
		if (!bTooBig)
		{
			continue;
		}
		Leaves.Remove(L);
		const int32 H = L.Size / 2;
		for (const FWRSeaLeaf& C : { FWRSeaLeaf{ L.X, L.Y, H }, FWRSeaLeaf{ L.X + H, L.Y, H }, FWRSeaLeaf{ L.X, L.Y + H, H }, FWRSeaLeaf{ L.X + H, L.Y + H, H } })
		{
			if (!AllLand(Mask, C.X, C.Y, C.Size))
			{
				Leaves.Add(C);
				Work.Add(C);
			}
		}
		// Its bigger neighbours may now be too big in turn.
		ForEachOutside(L, [&](int32, int32 CX, int32 CY)
		{
			if (const FWRSeaLeaf* Nb = FindLeaf(Leaves, Start, MaxCells, CX, CY))
			{
				if (Nb->Size > H) Work.Add(*Nb);
			}
		});
	}
	return Leaves.Array();
}

namespace WRSeaDetail
{
	static FLinearColor ParseSRGB(const FString& Text, const FLinearColor& Fallback)
	{
		TArray<FString> Parts;
		Text.ParseIntoArray(Parts, TEXT(","));
		return Parts.Num() < 3 ? Fallback : FLinearColor(FColor(FCString::Atoi(*Parts[0]), FCString::Atoi(*Parts[1]), FCString::Atoi(*Parts[2])));
	}

	/** Look checks and tuning: wr.SeaParam <material scalar> <value> sets one of the sea material's numbers live. */
	static void SeaParam(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 2 || !World)
		{
			return;
		}
		for (TActorIterator<AWRSea> It(World); It; ++It)
		{
			It->SetMaterialScalar(FName(*Args[0]), FCString::Atof(*Args[1]));
		}
	}
	static FAutoConsoleCommandWithWorldAndArgs SeaParamCommand(TEXT("wr.SeaParam"),
		TEXT("wr.SeaParam <name> <value>: set a sea material scalar live (look checks; tuning.json sea.scalars keeps it)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SeaParam));
}

void AWRSea::SetMaterialScalar(FName Name, float Value)
{
	if (Material)
	{
		Material->SetScalarParameterValue(Name, Value);
		UE_LOG(LogWildreach, Log, TEXT("SEA %s = %.3f"), *Name.ToString(), Value);
	}
}

// ---------------------------------------------------------------- sea

AWRSea::AWRSea()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;  // only the day / night dimming and the sun direction
	bReplicates = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRSea::BeginPlay()
{
	Super::BeginPlay();
	const FWRTuning& T = FWRTuning::Get();
	// The pack's ocean through the project's cooked instance (Content/Sea/MI_Ocean, Tools/make_sea_material.py);
	// a fresh clone without the git-ignored pack keeps the map's flat plane.
	UMaterialInterface* Ocean = LoadObject<UMaterialInterface>(nullptr, *T.Str(TEXT("sea.material"), TEXT("/Game/Sea/MI_Ocean.MI_Ocean")));
	// (An instance whose parent is missing reports the engine's default material as its base.)
	if (!Ocean || !Ocean->GetBaseMaterial() || !Ocean->GetBaseMaterial()->GetName().StartsWith(TEXT("M_Ocean")))
	{
		UE_LOG(LogWildreach, Warning, TEXT("SEA no ocean material (WaterMaterials pack missing?): keeping the map's plane"));
		SetActorTickEnabled(false);
		return;
	}
	Material = UMaterialInstanceDynamic::Create(Ocean, this);
	// Any of the material's parameters can be tuned by name: "scalars" {name: value}, "colours" {name: "r,g,b"} (sRGB).
	if (const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("tuning.json")))
	{
		const TSharedPtr<FJsonObject>* SeaObj = nullptr;
		if (Root->TryGetObjectField(TEXT("sea"), SeaObj))
		{
			const TSharedPtr<FJsonObject>* Scalars = nullptr;
			if ((*SeaObj)->TryGetObjectField(TEXT("scalars"), Scalars))
			{
				for (const TPair<FString, TSharedPtr<FJsonValue>>& P : (*Scalars)->Values)
				{
					Material->SetScalarParameterValue(FName(*P.Key), P.Value->AsNumber());
				}
			}
			const TSharedPtr<FJsonObject>* Dimmed = nullptr;
			if ((*SeaObj)->TryGetObjectField(TEXT("dim"), Dimmed))
			{
				for (const TPair<FString, TSharedPtr<FJsonValue>>& P : (*Dimmed)->Values)
				{
					DayValues.Add(FName(*P.Key), P.Value->AsNumber());
				}
			}
			const TSharedPtr<FJsonObject>* Colours = nullptr;
			if ((*SeaObj)->TryGetObjectField(TEXT("colours"), Colours))
			{
				for (const TPair<FString, TSharedPtr<FJsonValue>>& P : (*Colours)->Values)
				{
					Material->SetVectorParameterValue(FName(*P.Key), WRSeaDetail::ParseSRGB(P.Value->AsString(), FLinearColor::Blue));
				}
			}
		}
	}
	Globals = LoadObject<UMaterialParameterCollection>(nullptr, TEXT("/Game/WaterMaterials/Materials/MPC_Globals.MPC_Globals"));
	if (!BuildMesh())
	{
		UE_LOG(LogWildreach, Warning, TEXT("SEA no Data/sea_mask.json: keeping the map's plane"));
		SetActorTickEnabled(false);
		return;
	}

	// The map's flat plane goes (it's the static mesh actor wearing MI_Sea).
	int32 Hidden = 0;
	for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
	{
		const UStaticMeshComponent* C = It->GetStaticMeshComponent();
		const UMaterialInterface* M = C ? C->GetMaterial(0) : nullptr;
		if (M && M->GetName() == TEXT("MI_Sea"))
		{
			It->SetActorHiddenInGame(true);
			++Hidden;
		}
	}
	bActive = true;
	UE_LOG(LogWildreach, Log, TEXT("SEA up (%s, the map's plane hidden: %d)"), *Ocean->GetName(), Hidden);
}

bool AWRSea::BuildMesh()
{
	const double Began = FPlatformTime::Seconds();
	const TSharedPtr<FJsonObject> File = FWRTuning::LoadDataFile(TEXT("sea_mask.json"));
	FWRSeaMask Mask;
	if (!File.IsValid() || !Mask.Parse(*File))
	{
		return false;
	}
	Mask.ComputeDistance();
	const FWRTuning& T = FWRTuning::Get();
	const int32 MaxCells = static_cast<int32>(FMath::RoundUpToPowerOfTwo(static_cast<uint32>(FMath::Max(1.f, T.Num(TEXT("sea.maxSquare"), 409600.f) / Mask.Cell))));
	const int32 RootCells = static_cast<int32>(FMath::RoundUpToPowerOfTwo(static_cast<uint32>(FMath::Max(1.f, T.Num(TEXT("sea.extent"), 6553600.f) / Mask.Cell))));
	const TArray<FWRSeaLeaf> Leaves = FWRSeaRules::BuildLeaves(Mask, RootCells, T.Num(TEXT("sea.falloff"), 6.f), MaxCells);
	const int32 Start = FWRSeaRules::RootStart(Mask, RootCells);
	const TSet<FWRSeaLeaf> Set(Leaves);
	const float Level = T.Num(TEXT("sea.level"), -20.f);
	const float UVScale = T.Num(TEXT("sea.uvScale"), 1500.f);
	const int32 TileCells = FMath::Max(8, FMath::RoundToInt(T.Num(TEXT("sea.tile"), 51200.f) / Mask.Cell));

	// One mesh per tile (TileCells square; squares bigger than a tile share one far-out mesh), so the engine can skip
	// the tiles that are off screen. Corners are shared within a tile; where a neighbour is half the size the side gets
	// its midpoint too (and the square becomes a fan around its centre), so the waves never open a crack.
	struct FTile { TArray<FVector> Verts; TArray<int32> Tris; TMap<FIntPoint, int32> Index; };
	TMap<FIntPoint, FTile> Built;
	const FIntPoint FarKey(TNumericLimits<int32>::Max(), 0);
	int32 Triangles = 0;
	for (const FWRSeaLeaf& L : Leaves)
	{
		const FIntPoint Key = L.Size > TileCells ? FarKey
			: FIntPoint(FMath::FloorToInt((L.X - Start) / static_cast<float>(TileCells)), FMath::FloorToInt((L.Y - Start) / static_cast<float>(TileCells)));
		FTile& Tile = Built.FindOrAdd(Key);
		// Vertices on a half-cell lattice (the centre of a one-cell square is half a cell in).
		const auto Vert = [&Tile, &Mask, Level](int32 HX, int32 HY)
		{
			if (const int32* Found = Tile.Index.Find(FIntPoint(HX, HY))) return *Found;
			const int32 I = Tile.Verts.Add(FVector(Mask.Origin.X + HX * 0.5f * Mask.Cell, Mask.Origin.Y + HY * 0.5f * Mask.Cell, Level));
			Tile.Index.Add(FIntPoint(HX, HY), I);
			return I;
		};
		const int32 X2 = L.X * 2, Y2 = L.Y * 2, S2 = L.Size * 2, H = L.Size / 2;
		// A finer neighbour along a side: the half-size square just outside it, at either half.
		const auto Finer = [&](int32 AX, int32 AY, int32 BX, int32 BY)
		{
			if (L.Size == 1) return false;
			const FWRSeaLeaf* NA = WRSeaDetail::FindLeaf(Set, Start, MaxCells, AX, AY);
			const FWRSeaLeaf* NB = WRSeaDetail::FindLeaf(Set, Start, MaxCells, BX, BY);
			return (NA && NA->Size < L.Size) || (NB && NB->Size < L.Size);
		};
		const bool bS = Finer(L.X, L.Y - 1, L.X + H, L.Y - 1);
		const bool bE = Finer(L.X + L.Size, L.Y, L.X + L.Size, L.Y + H);
		const bool bN = Finer(L.X, L.Y + L.Size, L.X + H, L.Y + L.Size);
		const bool bW = Finer(L.X - 1, L.Y, L.X - 1, L.Y + H);
		// The rim counter-clockwise from the -X -Y corner (seen from above with +Y up), as the old disc's rings ran.
		TArray<int32, TInlineAllocator<8>> Rim;
		Rim.Add(Vert(X2, Y2));
		if (bS) Rim.Add(Vert(X2 + S2 / 2, Y2));
		Rim.Add(Vert(X2 + S2, Y2));
		if (bE) Rim.Add(Vert(X2 + S2, Y2 + S2 / 2));
		Rim.Add(Vert(X2 + S2, Y2 + S2));
		if (bN) Rim.Add(Vert(X2 + S2 / 2, Y2 + S2));
		Rim.Add(Vert(X2, Y2 + S2));
		if (bW) Rim.Add(Vert(X2, Y2 + S2 / 2));
		// Faces point up: centre, next, this (each triangle's (P1 - P0) x (P2 - P0) points down, which Unreal draws
		// facing up, as the lava surface in WRWorldDressing).
		if (Rim.Num() == 4)
		{
			Tile.Tris.Append({ Rim[0], Rim[2], Rim[1], Rim[0], Rim[3], Rim[2] });
			Triangles += 2;
		}
		else
		{
			const int32 C = Vert(X2 + S2 / 2, Y2 + S2 / 2);
			for (int32 i = 0; i < Rim.Num(); ++i)
			{
				Tile.Tris.Append({ C, Rim[(i + 1) % Rim.Num()], Rim[i] });
			}
			Triangles += Rim.Num();
		}
	}
	for (TPair<FIntPoint, FTile>& P : Built)
	{
		FTile& Tile = P.Value;
		TArray<FVector> Normals;
		TArray<FProcMeshTangent> Tangents;
		TArray<FVector2D> UVs;
		Normals.Init(FVector::UpVector, Tile.Verts.Num());
		Tangents.Init(FProcMeshTangent(1.f, 0.f, 0.f), Tile.Verts.Num());
		UVs.Reserve(Tile.Verts.Num());
		for (const FVector& V : Tile.Verts) UVs.Add(FVector2D(V.X, V.Y) / UVScale);
		UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this);
		Mesh->SetupAttachment(RootComponent);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCastShadow(false);
		Mesh->bAffectDistanceFieldLighting = false;
		Mesh->SetCanEverAffectNavigation(false);
		Mesh->RegisterComponent();
		Mesh->CreateMeshSection(0, Tile.Verts, Tile.Tris, Normals, UVs, TArray<FColor>(), Tangents, false);
		Mesh->SetMaterial(0, Material);
		Tiles.Add(Mesh);
	}
	UE_LOG(LogWildreach, Log, TEXT("SEA mesh: %d squares, %d triangles in %d tiles, %.0f ms"), Leaves.Num(), Triangles, Tiles.Num(), (FPlatformTime::Seconds() - Began) * 1000.0);
	return true;
}

void AWRSea::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bActive)
	{
		const AWRGameState* State = AWRGameState::Get(this);
		ApplyLook(State ? State->GetDaylight() : 1.f);
	}
}

void AWRSea::ApplyLook(float Daylight)
{
	// The pack's sea glows a little, paints on highlights and mirrors a bright sky cubemap (tuning sea.dim, their daytime
	// values): all dim with the daylight, so the sea goes dark at night instead of shining.
	if (FMath::Abs(Daylight - LastDaylight) > 0.01f)
	{
		LastDaylight = Daylight;
		const FWRTuning& T = FWRTuning::Get();
		const float Dim = FMath::Lerp(T.Num(TEXT("sea.nightDim"), 0.08f), 1.f, Daylight);
		for (const TPair<FName, float>& P : DayValues)
		{
			Material->SetScalarParameterValue(P.Key, P.Value * Dim);
		}
	}
	// Its painted-on highlights follow the real sun (the pack sets this from its own sun blueprint).
	const UWRSkySubsystem* Sub = GetWorld()->GetSubsystem<UWRSkySubsystem>();
	const AWRSky* Sky = Sub ? Sub->GetSky() : nullptr;
	if (Globals && Sky && !Sky->GetSunDirection().IsZero())
	{
		if (UMaterialParameterCollectionInstance* Inst = GetWorld()->GetParameterCollectionInstance(Globals))
		{
			const FVector D = Sky->GetSunDirection();
			Inst->SetVectorParameterValue(TEXT("LightVector"), FLinearColor(D.X, D.Y, D.Z, 0.f));
		}
	}
}
