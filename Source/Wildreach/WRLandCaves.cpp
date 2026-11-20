// The underground caves' rock (see WRLandCaves.h).

#include "WRLandCaves.h"
#include "Wildreach.h"
#include "WRLand.h"
#include "WRToolMesh.h"
#include "WRTuning.h"
#include "Async/ParallelFor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"

namespace WRLandCavesImpl
{
	FLinearColor LcLin(uint8 R, uint8 G, uint8 B) { return FLinearColor(FColor(R, G, B)); }
	float LcSm(float E0, float E1, float X) { const float T = FMath::Clamp((X - E0) / (E1 - E0), 0.f, 1.f); return T * T * (3.f - 2.f * T); }
	float LcN2(float X, float Y)
	{
		const int32 X0 = FMath::FloorToInt(X), Y0 = FMath::FloorToInt(Y);
		const float FX = X - X0, FY = Y - Y0;
		const auto H = [](int32 A, int32 B) { uint32 V = uint32(A) * 0x8da6b343u ^ uint32(B) * 0xd8163841u ^ 0x2c1b3u; V ^= V >> 13; V *= 0x5bd1e995u; V ^= V >> 15; return float(V & 0xffff) / 32767.5f - 1.f; };
		const float SX = FX * FX * (3.f - 2.f * FX), SY = FY * FY * (3.f - 2.f * FY);
		return FMath::Lerp(FMath::Lerp(H(X0, Y0), H(X0 + 1, Y0), SX), FMath::Lerp(H(X0, Y0 + 1), H(X0 + 1, Y0 + 1), SX), SY);
	}
	float LcN3(const FVector& P) { return LcN2(P.X + P.Z * 0.37f, P.Y - P.Z * 0.61f) * 0.6f + LcN2(P.Y * 0.73f + 11.f, P.Z + P.X * 0.29f) * 0.4f; }

	/** The ground (world cm) at a world point's column: on the land's own triangles (each grid square split on its
	 *  flatter diagonal, as WRLand builds its chunks), so the skin's ground lies exactly on the land's. */
	float LcGroundZ(const FWRLandDef* L, float X, float Y)
	{
		const FVector2D M = L->ToMap(FVector(X, Y, 0.f));
		const float FX = (M.X - L->HfOrigin.X) / L->HfStep, FY = (M.Y - L->HfOrigin.Y) / L->HfStep;
		const int32 I = FMath::FloorToInt(FX), J = FMath::FloorToInt(FY);
		const float TX = FX - I, TY = FY - J;
		const float A = L->HfSample(I, J), B = L->HfSample(I + 1, J), C = L->HfSample(I, J + 1), E = L->HfSample(I + 1, J + 1);
		float H;
		if (FMath::Abs(A - E) <= FMath::Abs(B - C)) H = TX >= TY ? A + (B - A) * TX + (E - B) * TY : A + (C - A) * TY + (E - C) * TX;
		else H = TX + TY <= 1.f ? A + (B - A) * TX + (C - A) * TY : E + (C - E) * (1.f - TX) + (B - E) * (1.f - TY);
		return L->ToWorld(M, H).Z;
	}
	/** The land draws its own ground over this point (no corner of its grid square is cut). */
	bool LcLandDraws(const FWRLandDef* L, float X, float Y)
	{
		const FVector2D M = L->ToMap(FVector(X, Y, 0.f));
		const FVector2D G = (M - L->HfOrigin) / L->HfStep;
		const int32 I = FMath::FloorToInt(G.X), J = FMath::FloorToInt(G.Y);
		for (int32 dj = 0; dj <= 1; ++dj) for (int32 di = 0; di <= 1; ++di)
		{
			if (L->HfSurfaceAt(I + di, J + dj) == FWRLandDef::HfHidden) return false;
		}
		return true;
	}
	/** The caves' air at a world point (cm, negative inside), the rock's noise on the walls (less on the floor). */
	float LcAir(const FWRLandDef* L, const TArray<int32>& Caves, const FVector& P, int32* OutCave = nullptr, float* OutFloorCm = nullptr)
	{
		const FVector2D M = L->ToMap(P);
		const FVector Pm(M.X, M.Y, (P.Z - L->Origin.Z) / 100.f);
		float Best = TNumericLimits<float>::Max(), Floor = 0.f;
		int32 Which = INDEX_NONE;
		for (int32 c : Caves)
		{
			float F = 0.f;
			const float A = L->Caves[c].Air(Pm, &F);
			if (A < Best) { Best = A; Floor = F; Which = c; }
		}
		if (Which == INDEX_NONE) return 1.0e6f;
		const float Amp = 0.12f + 0.38f * LcSm(0.4f, 1.6f, float(Pm.Z) - Floor);
		Best += Amp * (LcN3(P / 220.f) + 0.35f * LcN3(P / 70.f + FVector(5.f, 1.f, 3.f)));
		if (OutCave) *OutCave = Which;
		if (OutFloorCm) *OutFloorCm = L->Origin.Z + Floor * 100.f;
		return Best * 100.f;
	}
}

AWRLandCaves::AWRLandCaves()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRLandCaves::BeginPlay()
{
	Super::BeginPlay();
	Cell = FMath::Max(60.f, WRTune(TEXT("landCaves.cell"), 100.f));
	Cells = FMath::Max(8, int32(WRTune(TEXT("landCaves.cells"), 20.f)));
	for (const FWRLandDef& D : WRLand::All())
	{
		if (D.bHeightfield && D.Caves.Num() > 0 && WRLand::IsLive(D)) { Land = &D; break; }
	}
	if (!Land) { SetActorTickEnabled(false); return; }
	const float S = Cell * Cells;
	TSet<FIntVector> Keys;
	for (const FWRLandCave& C : Land->Caves)
	{
		// The cave's air and the ground round its mouth (the lip's skin), in game cm.
		const FBox Bm = C.Bounds(9.f);
		const FVector A = Land->ToWorld(FVector2D(Bm.Min), Bm.Min.Z), B = Land->ToWorld(FVector2D(Bm.Max), Bm.Max.Z + 3.f);
		const FBox Bw(A.ComponentMin(B) - FVector(Cell * 2.f), A.ComponentMax(B) + FVector(Cell * 2.f));
		const FIntVector Lo(FMath::FloorToInt(Bw.Min.X / S), FMath::FloorToInt(Bw.Min.Y / S), FMath::FloorToInt(Bw.Min.Z / S));
		const FIntVector Hi(FMath::FloorToInt(Bw.Max.X / S), FMath::FloorToInt(Bw.Max.Y / S), FMath::FloorToInt(Bw.Max.Z / S));
		for (int32 z = Lo.Z; z <= Hi.Z; ++z) for (int32 y = Lo.Y; y <= Hi.Y; ++y) for (int32 x = Lo.X; x <= Hi.X; ++x) Keys.Add(FIntVector(x, y, z));
	}
	Queue = Keys.Array();
	bQueued = true;
	UE_LOG(LogWildreach, Log, TEXT("LAND CAVES %s: %d caves, %d rock cubes of %.0f m to build"), *Land->Id.ToString(), Land->Caves.Num(), Queue.Num(), S / 100.f);
}

void AWRLandCaves::EndPlay(const EEndPlayReason::Type Reason)
{
	// The workers touch only their own result and the land's static data: wait for them so none outlives the world.
	for (FJob& J : InFlight) J.Task.Wait();
	InFlight.Reset();
	Super::EndPlay(Reason);
}

void AWRLandCaves::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bQueued) return;
	const double Budget = WRTune(TEXT("landCaves.msPerFrame"), 4.f) / 1000.0;
	const double Start = FPlatformTime::Seconds();
	for (int32 i = 0; i < InFlight.Num() && FPlatformTime::Seconds() - Start < Budget;)
	{
		if (!InFlight[i].Task.IsCompleted()) { ++i; continue; }
		const FCubePtr C = InFlight[i].Cube;
		InFlight.RemoveAtSwap(i);
		if (C.IsValid()) FinishCube(*C);
	}
	const int32 MaxInFlight = FMath::Max(1, int32(WRTune(TEXT("landCaves.inFlight"), 4.f)));
	const float GroundSkin = WRTune(TEXT("landCaves.groundSkin"), 120.f);
	while (Next < Queue.Num() && InFlight.Num() < MaxInFlight)
	{
		FCubePtr C = MakeShared<FCube, ESPMode::ThreadSafe>();
		C->Mesh.Key = Queue[Next++];
		const float Ce = Cell;
		const int32 N = Cells;
		const FVector Loc = GetActorLocation();
		const FWRLandDef* L = Land;
		FJob& J = InFlight.AddDefaulted_GetRef();
		J.Cube = C;
		J.Task = UE::Tasks::Launch(UE_SOURCE_LOCATION, [C, L, Ce, N, Loc, GroundSkin]() { ComputeCube(L, C->Mesh.Key, Ce, N, Loc, GroundSkin, *C); });
	}
	if (IsBuilt())
	{
		SetActorTickEnabled(false);
		UE_LOG(LogWildreach, Log, TEXT("LAND CAVES built: %d rock meshes"), Meshes);
	}
}

void AWRLandCaves::ComputeCube(const FWRLandDef* L, const FIntVector& Key, float Cell, int32 Cells, const FVector& ActorLoc, float GroundSkin, FCube& Out)
{
	using namespace WRLandCavesImpl;
	const float S = Cell * Cells;
	const FVector O(Key.X * S, Key.Y * S, Key.Z * S);
	// The caves whose air comes near this cube.
	TArray<int32> Caves;
	for (int32 c = 0; c < L->Caves.Num(); ++c)
	{
		const FBox Bm = L->Caves[c].Bounds(9.f);
		const FVector A = L->ToWorld(FVector2D(Bm.Min), Bm.Min.Z), B = L->ToWorld(FVector2D(Bm.Max), Bm.Max.Z + 3.f);
		if (FBox(A.ComponentMin(B), A.ComponentMax(B)).Intersect(FBox(O - FVector(Cell * 2.f), O + FVector(S + Cell * 2.f)))) Caves.Add(c);
	}
	if (Caves.Num() == 0) { Out.Mesh.bSkipped = true; return; }
	Out.Biome = L->Caves[Caves[0]].Biome;
	const int32 NS = Cells + 3, NC = Cells + 2;
	TArray<float> Ground;
	Ground.SetNumUninitialized(NS * NS);
	for (int32 j = 0; j < NS; ++j) for (int32 i = 0; i < NS; ++i) Ground[j * NS + i] = LcGroundZ(L, O.X + (i - 1) * Cell, O.Y + (j - 1) * Cell);
	// The field: the caves' air unioned with the air over the ground (negative in either).
	TArray<float> F;
	F.SetNumUninitialized(NS * NS * NS);
	ParallelFor(NS, [&](int32 k)
	{
		for (int32 j = 0; j < NS; ++j) for (int32 i = 0; i < NS; ++i)
		{
			const FVector P = O + FVector((i - 1) * Cell, (j - 1) * Cell, (k - 1) * Cell);
			F[(k * NS + j) * NS + i] = FMath::Min(LcAir(L, Caves, P), Ground[j * NS + i] - float(P.Z));
		}
	});
	const auto Fv = [&](int32 i, int32 j, int32 k) { return F[((k + 1) * NS + (j + 1)) * NS + (i + 1)]; };
	// Nothing crosses zero in this cube (all rock or all air): no skin.
	{
		bool bNeg = false, bPos = false;
		for (const float V : F) { bNeg |= V < 0.f; bPos |= V >= 0.f; if (bNeg && bPos) break; }
		if (!bNeg || !bPos) { Out.Mesh.bSkipped = true; return; }
	}
	TArray<int32> CellVert;
	CellVert.Init(INDEX_NONE, NC * NC * NC);
	TArray<FVector> Verts, Normals;
	TArray<FColor> Colours;
	TArray<FVector2D> UVs;
	TArray<bool> UnderLand, GroundSkinV;
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
			Sum += FMath::Lerp(FVector(Corner[Edge[e][0]][0], Corner[Edge[e][0]][1], Corner[Edge[e][0]][2]), FVector(Corner[Edge[e][1]][0], Corner[Edge[e][1]][1], Corner[Edge[e][1]][2]), T);
			++Cnt;
		}
		FVector P = O + (FVector(i, j, k) + Sum / FMath::Max(1, Cnt)) * Cell;
		// The ground's skin (its term the nearer to zero: not the cave's wall or thin roof), and of that, the part on ground
		// the land draws itself (sunk 4 cm under the land's, which wins where both are).
		const float GroundTerm = LcGroundZ(L, P.X, P.Y) - float(P.Z);
		const bool bGroundSkin = LcAir(L, Caves, P) > GroundTerm + 5.f;
		const bool bUnderLand = bGroundSkin && FMath::Abs(GroundTerm) < GroundSkin && LcLandDraws(L, P.X, P.Y);
		if (bUnderLand) P.Z -= 4.f;
		const FVector G((Cv[1] - Cv[0]) + (Cv[3] - Cv[2]) + (Cv[5] - Cv[4]) + (Cv[7] - Cv[6]), (Cv[2] - Cv[0]) + (Cv[3] - Cv[1]) + (Cv[6] - Cv[4]) + (Cv[7] - Cv[5]),
			(Cv[4] - Cv[0]) + (Cv[5] - Cv[1]) + (Cv[6] - Cv[2]) + (Cv[7] - Cv[3]));
		const FVector Nrm = (-G).GetSafeNormal();
		CellVert[((k + 1) * NC + (j + 1)) * NC + (i + 1)] = Verts.Num();
		Verts.Add(P - ActorLoc);
		UnderLand.Add(bUnderLand);
		GroundSkinV.Add(bGroundSkin);
		Normals.Add(Nrm);
		// The land's own vertex data here, bilinear from its four samples (its UV: map metres / 4).
		const FVector2D M = L->ToMap(P);
		UVs.Add(M / 4.f);
		const float GX = (M.X - L->HfOrigin.X) / L->HfStep, GY = (M.Y - L->HfOrigin.Y) / L->HfStep;
		const int32 I0 = FMath::FloorToInt(GX), J0 = FMath::FloorToInt(GY);
		const float TX = GX - I0, TY = GY - J0;
		FLinearColor C(0.f, 0.f, 0.f, 0.f);
		float Mix[6] = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
		for (int32 c = 0; c < 4; ++c)
		{
			const int32 SI = I0 + (c & 1), SJ = J0 + (c >> 1);
			const float Wt = ((c & 1) ? TX : 1.f - TX) * ((c >> 1) ? TY : 1.f - TY);
			float Rough = 0.9f;
			FLinearColor Gc = L->HfGround(SI, SJ, Nrm, Rough);
			Gc.A = 1.f - Rough;
			C += Gc * Wt;
			if (L->bLayered) { float Lm[6]; L->HfLayers(SI, SJ, Lm); for (int32 m = 0; m < 6; ++m) Mix[m] += Lm[m] * Wt; }
		}
		Out.Colours.Add(C);
		Out.UV1.Add(FVector2D(Mix[0], Mix[1]));
		Out.UV2.Add(FVector2D(Mix[2], Mix[3]));
		Out.UV3.Add(FVector2D(Mix[4], Mix[5]));
		Colours.Add(FWRToolMesh::Pack(C, 1.f - C.A));
	}
	const auto CV = [&](int32 i, int32 j, int32 k) { return CellVert[((k + 1) * NC + (j + 1)) * NC + (i + 1)]; };
	TArray<int32> Tris, Lip;
	const auto Quad = [&](int32 A, int32 B, int32 C, int32 E, const FVector& RockDir)
	{
		if (A < 0 || B < 0 || C < 0 || E < 0) return;
		const FVector QM = (Verts[A] + Verts[B] + Verts[C] + Verts[E]) * 0.25f + ActorLoc;
		// The skin lying on ground the land draws itself: the land's.
		if (UnderLand[A] && UnderLand[B] && UnderLand[C] && UnderLand[E]) return;
		// The ground's skin is the lip (drawn as the land), the rest the cave's rock.
		TArray<int32>& Into = int32(GroundSkinV[A]) + int32(GroundSkinV[B]) + int32(GroundSkinV[C]) + int32(GroundSkinV[E]) >= 3 ? Lip : Tris;
		// Each triangle faces the air on its own, judged by the surface's own normal at its corners (toward the air): the
		// grid axis of its edge misjudged triangles lying edge-on to it, and those showed as holes you saw the sky through.
		const auto Tri = [&](int32 X, int32 Y, int32 Z)
		{
			const FVector Air = Normals[X] + Normals[Y] + Normals[Z];
			const FVector Toward = Air.IsNearlyZero() ? -RockDir : Air;
			if (FVector::DotProduct(FVector::CrossProduct(Verts[Y] - Verts[X], Verts[Z] - Verts[X]), -Toward) >= 0.f) Into.Append({ X, Y, Z });
			else Into.Append({ X, Z, Y });
		};
		Tri(A, B, C);
		Tri(A, C, E);
	};
	for (int32 k = 0; k < Cells; ++k) for (int32 j = 0; j < Cells; ++j) for (int32 i = 0; i < Cells; ++i)
	{
		const float V0 = Fv(i, j, k);
		const float Sign = V0 < 0.f ? 1.f : -1.f;
		if ((V0 < 0.f) != (Fv(i + 1, j, k) < 0.f)) Quad(CV(i, j - 1, k - 1), CV(i, j, k - 1), CV(i, j, k), CV(i, j - 1, k), FVector(Sign, 0.f, 0.f));
		if ((V0 < 0.f) != (Fv(i, j + 1, k) < 0.f)) Quad(CV(i - 1, j, k - 1), CV(i - 1, j, k), CV(i, j, k), CV(i, j, k - 1), FVector(0.f, Sign, 0.f));
		if ((V0 < 0.f) != (Fv(i, j, k + 1) < 0.f)) Quad(CV(i - 1, j - 1, k), CV(i, j - 1, k), CV(i, j, k), CV(i - 1, j, k), FVector(0.f, 0.f, Sign));
	}
	if (Tris.Num() == 0 && Lip.Num() == 0) return;
	Out.Mesh.Verts = MoveTemp(Verts);
	Out.Mesh.Tris = MoveTemp(Tris);
	Out.Lip = MoveTemp(Lip);
	Out.Mesh.Normals = MoveTemp(Normals);
	Out.Mesh.UVs = MoveTemp(UVs);
	Out.Mesh.Colours = MoveTemp(Colours);
}

UMaterialInterface* AWRLandCaves::WallMaterial(FName Biome)
{
	if (const TObjectPtr<UMaterialInterface>* Found = Walls.Find(Biome)) return *Found;
	UMaterialInterface* Out = FWRToolMesh::Material();
	if (Biome == TEXT("tundra"))
	{
		// Ice: the seracs' and the Blue Below's blue-white.
		if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
		{
			UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, this);
			M->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.5f, 0.68f, 0.84f));
			Out = M;
		}
	}
	else if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_Rock.M_Rock")))
	{
		// The world-aligned rock, dark basalt (the Volcanic's site rock).
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, this);
		M->SetVectorParameterValue(TEXT("Tint"), FLinearColor(0.3f, 0.27f, 0.25f));
		Out = M;
	}
	Walls.Add(Biome, Out);
	return Out;
}

void AWRLandCaves::FinishCube(const FCube& C)
{
	if (C.Mesh.bSkipped || (C.Mesh.Tris.Num() == 0 && C.Lip.Num() == 0)) return;
	UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this);
	Mesh->SetupAttachment(RootComponent);
	Mesh->bUseAsyncCooking = true;
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	Mesh->SetCollisionObjectType(ECC_WorldStatic);
	Mesh->SetCanEverAffectNavigation(false);
	int32 Section = 0;
	if (C.Mesh.Tris.Num() > 0)
	{
		Mesh->CreateMeshSection(Section, C.Mesh.Verts, C.Mesh.Tris, C.Mesh.Normals, C.Mesh.UVs, C.Mesh.Colours, TArray<FProcMeshTangent>(), true);
		Mesh->SetMaterial(Section++, WallMaterial(C.Biome));
	}
	if (C.Lip.Num() > 0)
	{
		// The lip as the land draws its ground: its material and vertex data (WRLand BuildChunk).
		if (!GroundLook)
		{
			for (TActorIterator<AWRLand> It(GetWorld()); It; ++It) if (It->LandId == Land->Id) { GroundLook = It->GroundLook(); break; }
		}
		const TArray<FProcMeshTangent> Tangents;
		if (Land->bLayered) Mesh->CreateMeshSection_LinearColor(Section, C.Mesh.Verts, C.Lip, C.Mesh.Normals, C.Mesh.UVs, C.UV1, C.UV2, C.UV3, C.Colours, Tangents, true);
		else Mesh->CreateMeshSection_LinearColor(Section, C.Mesh.Verts, C.Lip, C.Mesh.Normals, C.Mesh.UVs, C.Colours, Tangents, true);
		Mesh->SetMaterial(Section, GroundLook ? GroundLook.Get() : FWRToolMesh::Material());
	}
	Mesh->SetCullDistance(WRTune(TEXT("landCaves.cullDistance"), 60000.f));
	Mesh->RegisterComponent();
	Rock.Add(Mesh);
	++Meshes;
}
