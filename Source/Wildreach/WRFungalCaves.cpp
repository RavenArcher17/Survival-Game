// The Fungal caverns' rock (see WRFungalCaves.h; cooks/build-biome-fungal.md 2.2-2.4).

#include "WRFungalCaves.h"
#include "Wildreach.h"
#include "WRFungalTypes.h"
#include "WRLand.h"
#include "WRToolMesh.h"
#include "WRTuning.h"
#include "Async/ParallelFor.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"

namespace WRFungalCavesImpl
{
	FLinearColor FcLin(uint8 R, uint8 G, uint8 B) { return FLinearColor(FColor(R, G, B)); }
	float FcSm(float E0, float E1, float X) { const float T = FMath::Clamp((X - E0) / (E1 - E0), 0.f, 1.f); return T * T * (3.f - 2.f * T); }
	float FcN2(float X, float Y)
	{
		const int32 X0 = FMath::FloorToInt(X), Y0 = FMath::FloorToInt(Y);
		const float FX = X - X0, FY = Y - Y0;
		const auto H = [](int32 A, int32 B) { uint32 V = uint32(A) * 0x8da6b343u ^ uint32(B) * 0xd8163841u ^ 0x8821u; V ^= V >> 13; V *= 0x5bd1e995u; V ^= V >> 15; return float(V & 0xffff) / 32767.5f - 1.f; };
		const float SX = FX * FX * (3.f - 2.f * FX), SY = FY * FY * (3.f - 2.f * FY);
		return FMath::Lerp(FMath::Lerp(H(X0, Y0), H(X0 + 1, Y0), SX), FMath::Lerp(H(X0, Y0 + 1), H(X0 + 1, Y0 + 1), SX), SY);
	}
	float FcN3(const FVector& P) { return FcN2(P.X + P.Z * 0.37f, P.Y - P.Z * 0.61f) * 0.6f + FcN2(P.Y * 0.73f + 11.f, P.Z + P.X * 0.29f) * 0.4f; }

	float FcGroundZ(const FWRLandDef* L, float X, float Y)
	{
		if (!L) return -1.0e7f;
		const FVector2D M = L->ToMap(FVector(X, Y, 0.f));
		return L->ToWorld(M, L->Height(M)).Z;
	}
	bool FcLandDraws(const FWRLandDef* L, float X, float Y)
	{
		if (!L) return false;
		const FVector2D M = L->ToMap(FVector(X, Y, 0.f));
		const float S = L->HfStep;
		const FVector2D G = (M - L->HfOrigin) / S;
		const int32 I = FMath::FloorToInt(G.X), J = FMath::FloorToInt(G.Y);
		for (int32 dj = 0; dj <= 1; ++dj) for (int32 di = 0; di <= 1; ++di)
		{
			const FVector2D C = L->HfOrigin + FVector2D(float(I + di), float(J + dj)) * S;
			if (L->HfHiddenAt(C)) return false;
		}
		return true;
	}

	/** The rock's colour (linear) and how much of it glows at a point of the skin, from the shape it belongs to. */
	FLinearColor FcRock(const FWRFungalData& D, int32 Shape, const FVector& P, const FVector& N, float GroundZw, float& OutGlow)
	{
		const float Patch = FcN2(P.X / 430.f, P.Y / 430.f) * 0.6f + FcN2(P.X / 93.f + 3.f, P.Y / 93.f) * 0.4f;
		const float Fine = FcN3(P / 60.f);
		const bool bFloor = N.Z > 0.6f, bCeil = N.Z < -0.55f;
		OutGlow = 0.f;
		// The skin that rolls over a mouth's lip is the plateau's limestone, mossed at the lip.
		if (P.Z > GroundZw - 400.f)
		{
			const float Lip = FcSm(GroundZw - 400.f, GroundZw - 50.f, P.Z);
			return FMath::Lerp(FcLin(167, 164, 152), FcLin(94, 122, 58), Lip * FcSm(-0.2f, 0.4f, Patch)) * (0.9f + 0.2f * Fine);
		}
		FName Zone = D.ShapeZone(Shape);
		float Floor = P.Z, H = 1000.f, Blend = 0.f;
		bool bDoline = false, bRiver = false, bChasm = false;
		FName BlendZone = NAME_None;
		if (Shape >= 0 && Shape < D.Halls.Num())
		{
			const FWRFungalData::FHall& Hl = D.Halls[Shape];
			Floor = Hl.At.Z;
			H = Hl.IsDoline() ? FMath::Max(1000.f, Hl.Top - Hl.At.Z) : Hl.H;
			bDoline = Hl.IsDoline();
			bChasm = Hl.IsChasm();
		}
		else if (Shape >= D.Halls.Num() && Shape < D.Halls.Num() + D.Tubes.Num())
		{
			const FWRFungalData::FTube& T = D.Tubes[Shape - D.Halls.Num()];
			float Fl = 0.f, Tt = 0.f; int32 Seg = 0; FVector Dir;
			T.Nearest(P, Fl, Seg, Tt, Dir);
			Floor = Fl;
			H = T.Roof();
			bRiver = T.bRiver;
			// A tube that joins two zones fades from one to the other along its length (fungal 2.2 blends).
			if (!T.Blend.IsNone() && T.Pts.Num() > 1) { Blend = 1.f - FcSm(0.15f, 0.85f, (Seg + Tt) / float(T.Pts.Num() - 1)); BlendZone = T.Blend; }
		}
		const float Up = FMath::Max(0.f, float(P.Z) - Floor);
		const float Rel = Up / FMath::Max(100.f, H);
		const auto ZoneColour = [&](FName Z, float& G) -> FLinearColor
		{
			if (Z == TEXT("F2"))
			{
				// The Gardens: loam and moss on the floor where the daylight lands; limestone walls going from fern-green
				// moss at the foot to cyan glow-moss in the shade higher up (a doline's walls) or cream flowstone (a hall).
				if (bFloor) { G = 0.04f * FcSm(0.3f, 0.8f, Patch); return FMath::Lerp(FcLin(70, 56, 40), FcLin(94, 122, 58), FcSm(-0.4f, 0.3f, Patch)); }
				if (bCeil) { G = 0.18f + 0.2f * FcSm(0.2f, 0.7f, Fine); return FMath::Lerp(FcLin(196, 188, 170), FcLin(57, 230, 194), 0.35f); }
				const float Moss = FcSm(0.25f, -0.15f, Rel - 0.3f * Patch);
				const float Glow = bDoline ? FcSm(0.35f, 0.7f, Rel) * FcSm(-0.2f, 0.4f, Patch) : FcSm(0.0f, 0.6f, Patch) * 0.6f;
				G = 0.45f * Glow;
				FLinearColor C = FMath::Lerp(FcLin(196, 190, 172), FcLin(94, 122, 58), Moss * 0.8f);
				return FMath::Lerp(C, FcLin(57, 230, 194), Glow * 0.7f);
			}
			if (Z == TEXT("F3"))
			{
				// The Halls: floors carpeted in cyan glow-moss, flowstone cream walls in bands, cave black ceilings with
				// white starthread specks (every ceiling glows a little: its height must read).
				if (bFloor)
				{
					const float Carpet = FcSm(-0.35f, 0.15f, Patch + 0.3f * Fine);
					G = 0.6f * Carpet;
					return FMath::Lerp(FcLin(70, 66, 58), FcLin(57, 230, 194), Carpet);
				}
				if (bCeil || Rel > 0.7f)
				{
					const float Star = FcSm(0.55f, 0.75f, FcN3(P / 35.f));
					G = 0.12f + 0.8f * Star;
					return FMath::Lerp(FcLin(18, 20, 22), FcLin(242, 246, 240), Star);
				}
				const float Band = FMath::Frac(Up / (90.f + 30.f * FcN2(P.X / 600.f, P.Y / 600.f)) + 0.12f * FcN3(P / 280.f));
				G = 0.08f * FcSm(0.5f, 0.9f, Patch);
				return (Band < 0.55f ? FcLin(216, 203, 176) : FcLin(176, 160, 132)) * (0.9f + 0.15f * Fine);
			}
			if (Z == TEXT("F4"))
			{
				// The Webs: black rhizomorph-veined rock, mycelium sheets in white patches, amber light leaking from fissures.
				const float Sheet = FcSm(0.25f, 0.55f, Patch);
				const float Fissure = FcSm(0.82f, 0.92f, FMath::Abs(FcN3(P / 140.f)) * 2.f);
				G = 0.7f * Fissure + 0.05f * Sheet;
				FLinearColor C = FMath::Lerp(FcLin(26, 23, 20), FcLin(232, 228, 218), Sheet * 0.85f);
				if (bFloor && bChasm) C = FMath::Lerp(FcLin(46, 40, 34), FcLin(184, 184, 176), Sheet * 0.5f);   // talus, silk-strewn
				return FMath::Lerp(C, FcLin(232, 160, 64), Fissure);
			}
			if (Z == TEXT("F5"))
			{
				// The Grottos: rimstone cream floors and dams, teal-tinted walls, a glowing teal band at the waterline.
				if (bFloor) { G = bRiver ? 0.06f : 0.02f; return bRiver ? FMath::Lerp(FcLin(224, 212, 188), FcLin(120, 112, 98), FcSm(0.f, 0.6f, Patch)) : FcLin(224, 212, 188) * (0.9f + 0.1f * Fine); }
				const float Line = FcSm(300.f, 60.f, FMath::Abs(Up - 150.f));
				G = 0.4f * Line + (bCeil ? 0.12f : 0.f);
				return FMath::Lerp(FMath::Lerp(FcLin(150, 170, 160), FcLin(200, 196, 180), FcSm(-0.3f, 0.5f, Patch)), FcLin(30, 140, 134), Line * 0.7f);
			}
			if (Z == TEXT("HG"))
			{
				// The Heart Grotto: a grown city: a living mycelium membrane, warm amber veins, honey light everywhere.
				if (bFloor) { G = 0.18f * FcSm(0.2f, 0.7f, Fine); return FMath::Lerp(FcLin(196, 186, 160), FcLin(232, 160, 64), 0.25f + 0.2f * Fine); }
				const float Vein = FcSm(0.75f, 0.9f, FMath::Abs(FcN3(P / 110.f)) * 2.f);
				G = 0.2f + 0.6f * Vein;
				return FMath::Lerp(FcLin(214, 196, 160), FcLin(232, 160, 64), 0.3f + 0.6f * Vein);
			}
			G = 0.f;
			return FcLin(167, 164, 152);
		};
		FLinearColor C = ZoneColour(Zone, OutGlow);
		if (Blend > 0.f && !BlendZone.IsNone())
		{
			float G2 = 0.f;
			C = FMath::Lerp(C, ZoneColour(BlendZone, G2), Blend);
			OutGlow = FMath::Lerp(OutGlow, G2, Blend);
		}
		return C * 0.95f;
	}
}

AWRFungalCaves::AWRFungalCaves()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

float AWRFungalCaves::CubeSize()
{
	return FMath::Max(80.f, WRTune(TEXT("fungalWorld.caves.cell"), 150.f)) * FMath::Max(8, int32(WRTune(TEXT("fungalWorld.caves.cells"), 20.f)));
}

void AWRFungalCaves::BeginPlay()
{
	Super::BeginPlay();
	Cell = FMath::Max(80.f, WRTune(TEXT("fungalWorld.caves.cell"), 150.f));
	Cells = FMath::Max(8, int32(WRTune(TEXT("fungalWorld.caves.cells"), 20.f)));
	if (GetNetMode() != NM_DedicatedServer)
	{
		if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_FungalGlow.M_FungalGlow")))
		{
			RockMID = UMaterialInstanceDynamic::Create(M, this);
			RockMID->SetVectorParameterValue(TEXT("GlowColor"), FLinearColor::White);
			RockMID->SetScalarParameterValue(TEXT("GlowScale"), WRTune(TEXT("fungalWorld.glow.rock"), 3.f));
			RockMID->SetScalarParameterValue(TEXT("Shimmer"), 0.2f);
		}
	}
	QueueCubes();
}

void AWRFungalCaves::QueueCubes()
{
	const FWRFungalData& D = FWRFungalData::Get();
	if (!D.bLoaded) return;
	const float S = Cell * Cells;
	TSet<FIntVector> Keys;
	for (int32 s = 0; s < D.NumShapes(); ++s)
	{
		const FBox B = D.ShapeBounds(s).ExpandBy(Cell * 2.f);
		const FIntVector A(FMath::FloorToInt(B.Min.X / S), FMath::FloorToInt(B.Min.Y / S), FMath::FloorToInt(B.Min.Z / S));
		const FIntVector Z(FMath::FloorToInt(B.Max.X / S), FMath::FloorToInt(B.Max.Y / S), FMath::FloorToInt(B.Max.Z / S));
		for (int32 z = A.Z; z <= Z.Z; ++z) for (int32 y = A.Y; y <= Z.Y; ++y) for (int32 x = A.X; x <= Z.X; ++x) Keys.Add(FIntVector(x, y, z));
	}
	Queue = Keys.Array();
	// The descents first (where a newcomer goes in: the Garden Doline), then outward.
	FVector Focus = D.CaveBounds.GetCenter();
	if (const FWRFungalData::FDescent* Dn = D.Descents.Num() ? &D.Descents[0] : nullptr) Focus = Dn->At;
	Queue.Sort([S, Focus](const FIntVector& A, const FIntVector& B)
	{
		const FVector CA((A.X + 0.5f) * S, (A.Y + 0.5f) * S, (A.Z + 0.5f) * S), CB((B.X + 0.5f) * S, (B.Y + 0.5f) * S, (B.Z + 0.5f) * S);
		return FVector::DistSquared(CA, Focus) < FVector::DistSquared(CB, Focus);
	});
	bQueued = true;
	UE_LOG(LogWildreach, Log, TEXT("FUNGAL caverns: %d rock cubes of %.0f m to build"), Queue.Num(), S / 100.f);
}

bool AWRFungalCaves::IsBuiltAt(const FVector& World) const
{
	const float S = Cell * Cells;
	return Done.Contains(FIntVector(FMath::FloorToInt(World.X / S), FMath::FloorToInt(World.Y / S), FMath::FloorToInt(World.Z / S)));
}

void AWRFungalCaves::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bQueued) return;
	// Worked out on worker threads, a few cubes in flight; the game thread only makes the finished ones into meshes,
	// inside its budget (WRCaveBuild.h).
	const double Budget = WRTune(TEXT("fungalWorld.caves.msPerFrame"), 6.f) / 1000.0;
	const double Start = FPlatformTime::Seconds();
	for (int32 i = 0; i < InFlight.Num() && FPlatformTime::Seconds() - Start < Budget;)
	{
		if (!InFlight[i].Task.IsCompleted()) { ++i; continue; }
		const FWRCaveMeshPtr M = InFlight[i].Mesh;
		InFlight.RemoveAtSwap(i);
		if (M.IsValid()) { FinishCube(*M); Done.Add(M->Key); }
	}
	const int32 MaxInFlight = FMath::Max(1, int32(WRTune(TEXT("fungalWorld.caves.inFlight"), 4.f)));
	const float GroundSkin = WRTune(TEXT("fungalWorld.caves.groundSkin"), 120.f);
	while (Next < Queue.Num() && InFlight.Num() < MaxInFlight)
	{
		FWRCaveMeshPtr M = MakeShared<FWRCaveMesh, ESPMode::ThreadSafe>();
		M->Key = Queue[Next++];
		const float C = Cell;
		const int32 N = Cells;
		const FVector Loc = GetActorLocation();
		FWRCaveJob& J = InFlight.AddDefaulted_GetRef();
		J.Mesh = M;
		J.Task = UE::Tasks::Launch(UE_SOURCE_LOCATION, [M, C, N, Loc, GroundSkin]() { ComputeCube(M->Key, C, N, Loc, GroundSkin, *M); });
	}
	if (IsBuilt())
	{
		SetActorTickEnabled(false);
		UE_LOG(LogWildreach, Log, TEXT("FUNGAL caverns built: %d rock meshes (%d empty cubes skipped)"), Rock.Num(), Skipped);
	}
}

void AWRFungalCaves::EndPlay(const EEndPlayReason::Type Reason)
{
	// The workers only touch their own result and the static fungal data; wait for them so none outlives the world.
	for (FWRCaveJob& J : InFlight) J.Task.Wait();
	InFlight.Reset();
	Super::EndPlay(Reason);
}

void AWRFungalCaves::ComputeCube(const FIntVector& Key, float Cell, int32 Cells, const FVector& ActorLoc, float GroundSkin, FWRCaveMesh& Out)
{
	using namespace WRFungalCavesImpl;
	const FWRFungalData& D = FWRFungalData::Get();
	const FWRLandDef* L = FWRFungalData::Land();
	const float S = Cell * Cells;
	const FVector O(Key.X * S, Key.Y * S, Key.Z * S);
	TArray<int32> Shapes;
	D.ShapesIn(FBox(O - FVector(Cell * 2.f), O + FVector(S + Cell * 2.f)), Shapes);
	if (Shapes.Num() == 0) return;
	const FVector Mid = O + FVector(S * 0.5f);
	const float HalfDiag = S * 0.8660254f + Cell * 2.f;
	// The ground over the cube's columns (only matters where a cube reaches up near it: a mouth).
	const int32 NS = Cells + 3, NC = Cells + 2;
	TArray<float> Ground;
	Ground.SetNumUninitialized(NS * NS);
	float MinGround = 1.0e9f;
	for (int32 j = 0; j < NS; ++j) for (int32 i = 0; i < NS; ++i)
	{
		const float G = FcGroundZ(L, O.X + (i - 1) * Cell, O.Y + (j - 1) * Cell);
		Ground[j * NS + i] = G;
		MinGround = FMath::Min(MinGround, G);
	}
	const bool bNearGround = O.Z + S + Cell * 2.f > MinGround - 500.f;
	// The Flue (the Sky lane's shaft) is air too where a cube reaches it, so the river's mouth chamber opens into it; the
	// skin its cylinder would make is the Sky lane's wall and floor, dropped below.
	const FVector2D FlueXY(D.FlueAt);
	const float FlueR = D.FlueR, FlueFloor = D.FlueAt.Z - 600.f;
	const bool bFlue = !D.FlueAt.IsZero() && FVector2D::Distance(FVector2D(Mid), FlueXY) < FlueR + S;
	// A cube far from every shape's boundary holds no surface (the shapes' distances are close bounds): skip it.
	if (!bNearGround && !bFlue)
	{
		float Centre = 1.0e6f;
		for (int32 s : Shapes) Centre = FMath::Min(Centre, D.ShapeAir(s, Mid));
		if (FMath::Abs(Centre) > HalfDiag * 1.6f) { Out.bSkipped = true; return; }
	}
	TArray<float> F;
	F.SetNumUninitialized(NS * NS * NS);
	ParallelFor(NS, [&](int32 k)
	{
		for (int32 j = 0; j < NS; ++j) for (int32 i = 0; i < NS; ++i)
		{
			const FVector P = O + FVector((i - 1) * Cell, (j - 1) * Cell, (k - 1) * Cell);
			float V = 1.0e6f;
			for (int32 s : Shapes) V = FMath::Min(V, D.ShapeAir(s, P));
			if (bNearGround) V = FMath::Min(V, Ground[j * NS + i] - float(P.Z));
			if (bFlue) V = FMath::Min(V, FMath::Max(FVector2D::Distance(FVector2D(P), FlueXY) - FlueR, FlueFloor - float(P.Z)));
			F[(k * NS + j) * NS + i] = V;
		}
	});
	const auto Fv = [&](int32 i, int32 j, int32 k) { return F[((k + 1) * NS + (j + 1)) * NS + (i + 1)]; };
	TArray<int32> CellVert;
	CellVert.Init(INDEX_NONE, NC * NC * NC);
	TArray<FVector> Verts, Normals;
	TArray<FColor> Colours;
	TArray<FVector2D> UVs;
	static const int32 Corner[8][3] = { {0,0,0},{1,0,0},{0,1,0},{1,1,0},{0,0,1},{1,0,1},{0,1,1},{1,1,1} };
	static const int32 Edge[12][2] = { {0,1},{2,3},{4,5},{6,7},{0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7} };
	for (int32 k = -1; k <= Cells; ++k) for (int32 j = -1; j <= Cells; ++j) for (int32 i = -1; i <= Cells; ++i)
	{
		float Cv[8];
		int32 Neg = 0;
		for (int32 c = 0; c < 8; ++c) { Cv[c] = Fv(i + Corner[c][0], j + Corner[c][1], k + Corner[c][2]); Neg += Cv[c] < 0.f; }
		if (Neg == 0 || Neg == 8) continue;
		FVector Sum = FVector::ZeroVector;
		int32 Cnt = 0;
		for (int32 e = 0; e < 12; ++e)
		{
			const float A = Cv[Edge[e][0]], B = Cv[Edge[e][1]];
			if ((A < 0.f) == (B < 0.f)) continue;
			const float T = A / (A - B);
			const FVector PA(Corner[Edge[e][0]][0], Corner[Edge[e][0]][1], Corner[Edge[e][0]][2]);
			const FVector PB(Corner[Edge[e][1]][0], Corner[Edge[e][1]][1], Corner[Edge[e][1]][2]);
			Sum += FMath::Lerp(PA, PB, T);
			++Cnt;
		}
		const FVector Local = FVector(i, j, k) + Sum / FMath::Max(1, Cnt);
		const FVector P = O + Local * Cell;
		const FVector G(
			(Cv[1] - Cv[0]) + (Cv[3] - Cv[2]) + (Cv[5] - Cv[4]) + (Cv[7] - Cv[6]),
			(Cv[2] - Cv[0]) + (Cv[3] - Cv[1]) + (Cv[6] - Cv[4]) + (Cv[7] - Cv[5]),
			(Cv[4] - Cv[0]) + (Cv[5] - Cv[1]) + (Cv[6] - Cv[2]) + (Cv[7] - Cv[3]));
		const FVector Nrm = (-G).GetSafeNormal();
		CellVert[((k + 1) * NC + (j + 1)) * NC + (i + 1)] = Verts.Num();
		Verts.Add(P - ActorLoc);
		Normals.Add(Nrm);
		UVs.Add(FVector2D(P.X / 400.f, P.Y / 400.f));
		int32 Near = INDEX_NONE;
		float Best = 1.0e9f;
		for (int32 s : Shapes) { const float A = D.ShapeAir(s, P); if (A < Best) { Best = A; Near = s; } }
		float Glow = 0.f;
		const FLinearColor C = FcRock(D, Near, P, Nrm, FcGroundZ(L, P.X, P.Y), Glow);
		Colours.Add(FWRToolMesh::Pack(C, FMath::Clamp(Glow, 0.f, 1.f)));
	}
	const auto CV = [&](int32 i, int32 j, int32 k) { return CellVert[((k + 1) * NC + (j + 1)) * NC + (i + 1)]; };
	TArray<int32> Tris;
	const auto Quad = [&](int32 A, int32 B, int32 C, int32 E, const FVector& RockDir)
	{
		if (A < 0 || B < 0 || C < 0 || E < 0) return;
		const FVector QM = (Verts[A] + Verts[B] + Verts[C] + Verts[E]) * 0.25f + ActorLoc;
		if (bNearGround && FMath::Abs(QM.Z - FcGroundZ(L, QM.X, QM.Y)) < GroundSkin && FcLandDraws(L, QM.X, QM.Y)) return;
		if (bFlue)
		{
			const float Rq = FVector2D::Distance(FVector2D(QM), FlueXY);
			if (Rq > FlueR - 450.f && Rq < FlueR + 300.f && QM.Z > FlueFloor - 200.f) return;   // the shaft's wall: the Sky's
			if (Rq < FlueR && QM.Z < FlueFloor + 300.f) return;                                // its floor: the Sky's slab
		}
		const FVector Cr = FVector::CrossProduct(Verts[B] - Verts[A], Verts[C] - Verts[A]);
		if (FVector::DotProduct(Cr, RockDir) >= 0.f) Tris.Append({ A, B, C, A, C, E });
		else Tris.Append({ A, C, B, A, E, C });
	};
	for (int32 k = 0; k < Cells; ++k) for (int32 j = 0; j < Cells; ++j) for (int32 i = 0; i < Cells; ++i)
	{
		const float V0 = Fv(i, j, k);
		const float Sign = V0 < 0.f ? 1.f : -1.f;
		if ((V0 < 0.f) != (Fv(i + 1, j, k) < 0.f)) Quad(CV(i, j - 1, k - 1), CV(i, j, k - 1), CV(i, j, k), CV(i, j - 1, k), FVector(Sign, 0.f, 0.f));
		if ((V0 < 0.f) != (Fv(i, j + 1, k) < 0.f)) Quad(CV(i - 1, j, k - 1), CV(i - 1, j, k), CV(i, j, k), CV(i, j, k - 1), FVector(0.f, Sign, 0.f));
		if ((V0 < 0.f) != (Fv(i, j, k + 1) < 0.f)) Quad(CV(i - 1, j - 1, k), CV(i, j - 1, k), CV(i, j, k), CV(i - 1, j, k), FVector(0.f, 0.f, Sign));
	}
	if (Tris.Num() == 0) return;
	Out.Verts = MoveTemp(Verts);
	Out.Tris = MoveTemp(Tris);
	Out.Normals = MoveTemp(Normals);
	Out.UVs = MoveTemp(UVs);
	Out.Colours = MoveTemp(Colours);
}

bool AWRFungalCaves::FinishCube(const FWRCaveMesh& M)
{
	if (M.bSkipped) ++Skipped;
	if (M.Tris.Num() == 0) return false;
	UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this);
	Mesh->SetupAttachment(RootComponent);
	Mesh->bUseAsyncCooking = true;
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	Mesh->SetCollisionObjectType(ECC_WorldStatic);
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->CreateMeshSection(0, M.Verts, M.Tris, M.Normals, M.UVs, M.Colours, TArray<FProcMeshTangent>(), true);
	Mesh->SetMaterial(0, RockMID ? static_cast<UMaterialInterface*>(RockMID) : FWRToolMesh::Material());
	Mesh->SetCullDistance(WRTune(TEXT("fungalWorld.caves.cullDistance"), 40000.f));
	Mesh->RegisterComponent();
	Rock.Add(Mesh);
	return true;
}
