// The underground caves' rock (see WRLandCaves.h).

#include "WRLandCaves.h"
#include "Wildreach.h"
#include "WRLand.h"
#include "WRToolMesh.h"
#include "WRBuildLookKit.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
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
	/** The sample whose ground a lip shows at sample (I, J): itself, or, where it is cut (a mouth's sample has no surface
	 *  of its own), the nearest drawn one. (The cut samples fell back to the zone's bare ground: in the Blue Below's
	 *  corridor a room's doorway showed squares of it in the corridor's ice floor.) */
	FIntPoint LcGroundSample(const FWRLandDef* L, int32 I, int32 J)
	{
		if (L->HfSurfaceAt(I, J) != FWRLandDef::HfHidden) return FIntPoint(I, J);
		for (int32 R = 1; R <= 4; ++R)
		{
			int32 BestD = MAX_int32;
			FIntPoint Best(I, J);
			for (int32 dj = -R; dj <= R; ++dj) for (int32 di = -R; di <= R; ++di)
			{
				if (FMath::Max(FMath::Abs(di), FMath::Abs(dj)) != R || L->HfSurfaceAt(I + di, J + dj) == FWRLandDef::HfHidden) continue;
				if (di * di + dj * dj < BestD) { BestD = di * di + dj * dj; Best = FIntPoint(I + di, J + dj); }
			}
			if (BestD < MAX_int32) return Best;
		}
		return FIntPoint(I, J);
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
		float Best = TNumericLimits<float>::Max(), Floor = 0.f, Width = 1.f;
		int32 Which = INDEX_NONE;
		for (int32 c : Caves)
		{
			float F = 0.f, W = 1.f;
			const float A = L->Caves[c].Air(Pm, &F, &W);
			if (A < Best) { Best = A; Floor = F; Width = W; Which = c; }
		}
		if (Which == INDEX_NONE) return 1.0e6f;
		// The rock's noise: up to 0.7 m on a big chamber's walls, less in a narrow passage (a squeeze stays passable), a
		// little on the floors.
		const float Wall = FMath::Clamp(0.22f * Width, 0.18f, 0.7f);
		const float Amp = FMath::Lerp(0.12f, Wall, LcSm(0.4f, 1.6f, float(Pm.Z) - Floor));
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
	BuildFormations();
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
	const bool bIce = Out.Biome == TEXT("tundra");
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
	TArray<bool> UnderLand;
	TArray<float> Depth;   // each vertex's depth under the ground's surface (cm)
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
		const FVector G((Cv[1] - Cv[0]) + (Cv[3] - Cv[2]) + (Cv[5] - Cv[4]) + (Cv[7] - Cv[6]), (Cv[2] - Cv[0]) + (Cv[3] - Cv[1]) + (Cv[6] - Cv[4]) + (Cv[7] - Cv[5]),
			(Cv[4] - Cv[0]) + (Cv[5] - Cv[1]) + (Cv[6] - Cv[2]) + (Cv[7] - Cv[3]));
		const FVector Nrm = (-G).GetSafeNormal();
		// The ground's skin (its term the nearer to zero: not the cave's wall or thin roof), and of that, the part on ground
		// the land draws itself (sunk 4 cm under the land's, which wins where both are). A floor lying at the ground's own
		// level is the ground's too: a passage's first metres run in at the ground's height, and drawn as the cave's rock
		// they showed as squares of ice in a tube corridor's floor at each room's doorway.
		const float GroundTerm = LcGroundZ(L, P.X, P.Y) - float(P.Z);
		const bool bGroundSkin = LcAir(L, Caves, P) > GroundTerm + 5.f || (Nrm.Z > 0.7f && FMath::Abs(GroundTerm) < 30.f);
		const bool bUnderLand = bGroundSkin && FMath::Abs(GroundTerm) < GroundSkin && LcLandDraws(L, P.X, P.Y);
		if (bUnderLand) P.Z -= 4.f;
		CellVert[((k + 1) * NC + (j + 1)) * NC + (i + 1)] = Verts.Num();
		Verts.Add(P - ActorLoc);
		UnderLand.Add(bUnderLand);
		Depth.Add(GroundTerm);
		Normals.Add(Nrm);
		// The rock's UV: slowly along the ground, height up the wall (M_HeldTool's grain runs along U, so the ice's grain
		// lies in level strata; projected from above it ran down every wall in streaks). The lip's are made again triangle by
		// triangle below, as the land's.
		const FVector2D M = L->ToMap(P);
		UVs.Add(FVector2D((M.X + M.Y) / 40.f, float(P.Z) / 400.f));
		// The land's own vertex data here, bilinear from its four samples.
		const float GX = (M.X - L->HfOrigin.X) / L->HfStep, GY = (M.Y - L->HfOrigin.Y) / L->HfStep;
		const int32 I0 = FMath::FloorToInt(GX), J0 = FMath::FloorToInt(GY);
		const float TX = GX - I0, TY = GY - J0;
		FLinearColor C(0.f, 0.f, 0.f, 0.f);
		float Mix[6] = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
		for (int32 c = 0; c < 4; ++c)
		{
			const FIntPoint GS = LcGroundSample(L, I0 + (c & 1), J0 + (c >> 1));
			const int32 SI = GS.X, SJ = GS.Y;
			const float Wt = ((c & 1) ? TX : 1.f - TX) * ((c >> 1) ? TY : 1.f - TY);
			float Rough = 0.9f;
			FLinearColor Gc = L->HfGround(SI, SJ, Nrm, Rough);
			Gc.A = 1.f - Rough;
			C += Gc * Wt;
			if (L->bLayered) { float Lm[6]; L->HfLayers(SI, SJ, Lm); for (int32 m = 0; m < 6; ++m) Mix[m] += Lm[m] * Wt; }
		}
		Out.Colours.Add(C);
		if (bIce)
		{
			// Glacier ice: layers of pale and deep blue (bands a metre or so thick, bent by noise), the floor drifted snow,
			// the ceiling a little deeper; the snow's own colour toward the surface (a mouth's lip).
			const float Mz = float(P.Z) / 100.f;
			const float Band = FMath::Frac(Mz / (1.1f + 0.4f * LcN2(P.X / 900.f, P.Y / 900.f)) + 0.3f * LcN3(P / 400.f));
			FLinearColor Ice = FMath::Lerp(LcLin(70, 128, 196), LcLin(176, 214, 240), LcSm(0.25f, 0.75f, Band));
			Ice = FMath::Lerp(Ice, LcLin(40, 92, 170), 0.35f * LcSm(0.55f, 0.85f, LcN3(P / 160.f) * 0.5f + 0.5f));
			float IceRough = 0.22f;
			if (Nrm.Z > 0.6f) { Ice = FMath::Lerp(LcLin(214, 226, 238), LcLin(190, 206, 224), LcSm(-0.3f, 0.5f, LcN3(P / 120.f))); IceRough = 0.8f; }
			else if (Nrm.Z < -0.55f) Ice *= 0.85f;
			const float Lip = LcSm(-250.f, -60.f, -GroundTerm);
			Ice = FMath::Lerp(Ice, C, Lip);
			Out.IceColours.Add(FWRToolMesh::Pack(Ice, FMath::Lerp(IceRough, 1.f - C.A, Lip)));
		}
		Out.UV1.Add(FVector2D(Mix[0], Mix[1]));
		Out.UV2.Add(FVector2D(Mix[2], Mix[3]));
		Out.UV3.Add(FVector2D(Mix[4], Mix[5]));
		Colours.Add(FWRToolMesh::Pack(C, 1.f - C.A));
	}
	const auto CV = [&](int32 i, int32 j, int32 k) { return CellVert[((k + 1) * NC + (j + 1)) * NC + (i + 1)]; };
	TArray<int32> Tris, Lip;
	// The lip (drawn as the land) is the skin less than LipDepth under the ground's surface: the ground round a mouth or a
	// doorway and the first of its rim; deeper is the cave's rock. A triangle across that depth is cut where it crosses
	// it, so the land's look gives way to the rock's along a smooth line (taken quad by quad, it changed in 1 m steps, and
	// the Tundra's mouths showed a stair of snow against the ice). The ice's colours fade to the ground's by then.
	constexpr float LipDepth = 90.f;
	TMap<uint64, int32> Mids;
	const auto LerpPacked = [](const FColor& A, const FColor& B, float T)
	{
		return FColor(uint8(FMath::RoundToInt(FMath::Lerp(float(A.R), float(B.R), T))), uint8(FMath::RoundToInt(FMath::Lerp(float(A.G), float(B.G), T))),
			uint8(FMath::RoundToInt(FMath::Lerp(float(A.B), float(B.B), T))), uint8(FMath::RoundToInt(FMath::Lerp(float(A.A), float(B.A), T))));
	};
	// The vertex where an edge crosses LipDepth (made once per edge, so the triangles either side share it: no crack).
	const auto Mid = [&](int32 X, int32 Y)
	{
		const uint64 Key = (uint64(FMath::Min(X, Y)) << 32) | uint64(FMath::Max(X, Y));
		if (const int32* Found = Mids.Find(Key)) return *Found;
		const int32 Lo = FMath::Min(X, Y), Hi = FMath::Max(X, Y);
		const float SL = LipDepth - Depth[Lo], SH = LipDepth - Depth[Hi];
		const float T = FMath::Clamp(SL / (SL - SH), 0.f, 1.f);
		const int32 N = Verts.Num();
		const FVector V = FMath::Lerp(Verts[Lo], Verts[Hi], T), Nm = FMath::Lerp(Normals[Lo], Normals[Hi], T).GetSafeNormal();
		const FVector2D U = FMath::Lerp(UVs[Lo], UVs[Hi], T);
		const FColor Cp = LerpPacked(Colours[Lo], Colours[Hi], T);
		const FLinearColor Cl = FMath::Lerp(Out.Colours[Lo], Out.Colours[Hi], T);
		const FVector2D U1 = FMath::Lerp(Out.UV1[Lo], Out.UV1[Hi], T), U2 = FMath::Lerp(Out.UV2[Lo], Out.UV2[Hi], T), U3 = FMath::Lerp(Out.UV3[Lo], Out.UV3[Hi], T);
		Verts.Add(V);
		Normals.Add(Nm);
		UVs.Add(U);
		Colours.Add(Cp);
		Out.Colours.Add(Cl);
		Out.UV1.Add(U1);
		Out.UV2.Add(U2);
		Out.UV3.Add(U3);
		if (Out.IceColours.Num() > Hi) { const FColor Ic = LerpPacked(Out.IceColours[Lo], Out.IceColours[Hi], T); Out.IceColours.Add(Ic); }
		UnderLand.Add(false);
		Depth.Add(LipDepth);
		Mids.Add(Key, N);
		return N;
	};
	// A triangle (in its facing order) into the lip, the rock, or cut between them.
	const auto Emit = [&](int32 X, int32 Y, int32 Z)
	{
		const int32 V[3] = { X, Y, Z };
		bool bLip[3];
		for (int32 n = 0; n < 3; ++n) bLip[n] = Depth[V[n]] <= LipDepth;
		if (bLip[0] && bLip[1] && bLip[2]) { Lip.Append({ X, Y, Z }); return; }
		if (!bLip[0] && !bLip[1] && !bLip[2]) { Tris.Append({ X, Y, Z }); return; }
		int32 Lone = 0;
		for (int32 n = 0; n < 3; ++n) if (bLip[n] != bLip[(n + 1) % 3] && bLip[n] != bLip[(n + 2) % 3]) Lone = n;
		const int32 A = V[Lone], B = V[(Lone + 1) % 3], C = V[(Lone + 2) % 3];
		const int32 AB = Mid(A, B), AC = Mid(A, C);
		TArray<int32>& Own = bLip[Lone] ? Lip : Tris;
		TArray<int32>& Rest = bLip[Lone] ? Tris : Lip;
		Own.Append({ A, AB, AC });
		Rest.Append({ AB, B, C });
		Rest.Append({ AB, C, AC });
	};
	const auto Quad = [&](int32 A, int32 B, int32 C, int32 E, const FVector& RockDir)
	{
		if (A < 0 || B < 0 || C < 0 || E < 0) return;
		// The skin lying on ground the land draws itself: the land's.
		if (UnderLand[A] && UnderLand[B] && UnderLand[C] && UnderLand[E]) return;
		// Each triangle faces the air on its own, judged by the surface's own normal at its corners (toward the air): the
		// grid axis of its edge misjudged triangles lying edge-on to it, and those showed as holes you saw the sky through.
		const auto Tri = [&](int32 X, int32 Y, int32 Z)
		{
			const FVector Air = Normals[X] + Normals[Y] + Normals[Z];
			const FVector Toward = Air.IsNearlyZero() ? -RockDir : Air;
			if (FVector::DotProduct(FVector::CrossProduct(Verts[Y] - Verts[X], Verts[Z] - Verts[X]), -Toward) >= 0.f) Emit(X, Y, Z);
			else Emit(X, Z, Y);
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
	// The lip's UVs, triangle by triangle: from above on ground, from the side on a steep face (map metres / 4, as the
	// land's), each triangle with corners of its own. (With the projection chosen per corner, a triangle whose corners
	// were projected two ways smeared the ground's texture across it in streaks; down a steep face projected from above
	// it ran in streaks too.)
	if (Lip.Num() > 0)
	{
		const auto Copy = [&](int32 V, const FVector2D& U)
		{
			const FVector Vp = Verts[V], Nm = Normals[V];
			const FColor Cp = Colours[V];
			const FLinearColor Cl = Out.Colours[V];
			const FVector2D U1 = Out.UV1[V], U2 = Out.UV2[V], U3 = Out.UV3[V];
			const bool bIceV = Out.IceColours.IsValidIndex(V);
			const FColor Ic = bIceV ? Out.IceColours[V] : FColor::White;
			const float Dp = Depth[V];
			const int32 N = Verts.Num();
			Verts.Add(Vp);
			Normals.Add(Nm);
			UVs.Add(U);
			Colours.Add(Cp);
			Out.Colours.Add(Cl);
			Out.UV1.Add(U1);
			Out.UV2.Add(U2);
			Out.UV3.Add(U3);
			if (bIceV) Out.IceColours.Add(Ic);
			UnderLand.Add(false);
			Depth.Add(Dp);
			return N;
		};
		TArray<int32> Own;
		Own.Reserve(Lip.Num());
		for (int32 t = 0; t + 2 < Lip.Num(); t += 3)
		{
			const int32 Tri[3] = { Lip[t], Lip[t + 1], Lip[t + 2] };
			const FVector Fn = FVector::CrossProduct(Verts[Tri[1]] - Verts[Tri[0]], Verts[Tri[2]] - Verts[Tri[0]]).GetSafeNormal();
			const int32 Proj = FMath::Abs(Fn.Z) >= 0.6f ? 0 : FMath::Abs(Fn.X) > FMath::Abs(Fn.Y) ? 1 : 2;
			for (const int32 V : Tri)
			{
				const FVector P = Verts[V] + ActorLoc;
				const FVector2D M = L->ToMap(P);
				const float Zm = float(P.Z) / 100.f;
				Own.Add(Copy(V, Proj == 0 ? M / 4.f : Proj == 1 ? FVector2D(M.Y, Zm) / 4.f : FVector2D(M.X, Zm) / 4.f));
			}
		}
		Lip = MoveTemp(Own);
	}
	if (Tris.Num() == 0 && Lip.Num() == 0) return;
	Out.Mesh.Verts = MoveTemp(Verts);
	Out.Mesh.Tris = MoveTemp(Tris);
	Out.Lip = MoveTemp(Lip);
	Out.Mesh.Normals = MoveTemp(Normals);
	Out.Mesh.UVs = MoveTemp(UVs);
	Out.Mesh.Colours = MoveTemp(Colours);
}

void AWRLandCaves::BuildFormations()
{
	using namespace WRLookKit;
	const bool bDraw = GetNetMode() != NM_DedicatedServer;
	TArray<FWRLookBlock> Blocks;
	FKit K(GetActorTransform(), Blocks);
	int32 Count = 0, Solid = 0;
	// A pillar you can't walk through (stalagmites, columns, frozen falls): a capsule, every machine.
	const auto Block = [this, &Solid](const FVector& Base, float RadiusCm, float HeightCm)
	{
		if (RadiusCm < 20.f || HeightCm < 60.f) return;
		UCapsuleComponent* Cap = NewObject<UCapsuleComponent>(this);
		Cap->SetupAttachment(RootComponent);
		Cap->InitCapsuleSize(RadiusCm, FMath::Max(RadiusCm, HeightCm * 0.5f));
		Cap->SetRelativeLocation(Base + FVector(0.f, 0.f, HeightCm * 0.5f));
		Cap->SetCollisionProfileName(TEXT("BlockAll"));
		Cap->SetCanEverAffectNavigation(false);
		Cap->RegisterComponent();
		++Solid;
	};
	// A rock of the set, its bottom at Base (sunk a fifth), Size (cm) along its own axes, with collision.
	static const TCHAR* RockNames[] = { TEXT("01A"), TEXT("02A"), TEXT("05A"), TEXT("08A"), TEXT("09A"), TEXT("12A") };
	const auto SetRock = [this](int32 Which, const FVector& Size, const FVector& Base, float Yaw, UMaterialInterface* Mat)
	{
		const TCHAR* N = RockNames[((Which % 6) + 6) % 6];
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/SimpleRocks/3D_Assets/Meshes/Rocks/SM_SimpleRock_%s.SM_SimpleRock_%s"), N, N));
		if (!Mesh) return;
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
		C->SetStaticMesh(Mesh);
		C->SetupAttachment(RootComponent);
		const FBox B = Mesh->GetBoundingBox();
		const FVector S = B.GetSize(), Scale(Size.X / FMath::Max(1.f, float(S.X)), Size.Y / FMath::Max(1.f, float(S.Y)), Size.Z / FMath::Max(1.f, float(S.Z)));
		const FRotator Rot(0.f, Yaw, 0.f);
		C->SetRelativeScale3D(Scale);
		C->SetRelativeRotation(Rot);
		C->SetRelativeLocation(Base + Rot.RotateVector(FVector(-B.GetCenter().X * Scale.X, -B.GetCenter().Y * Scale.Y, -B.Min.Z * Scale.Z - Size.Z * 0.2f)));
		if (Mat) C->SetMaterial(0, Mat);
		C->SetCollisionProfileName(TEXT("BlockAll"));
		C->SetCanEverAffectNavigation(false);
		C->RegisterComponent();
	};
	for (const FWRLandCave& Cave : Land->Caves)
	{
		const bool bIce = Cave.Biome == TEXT("tundra");
		UMaterialInterface* RockMat = WallMaterial(Cave.Biome);
		const FSurf Basalt = FSurf::Stone(FLinearColor(0.07f, 0.064f, 0.06f), 0.9f), Dripstone = FSurf::Stone(FLinearColor(0.05f, 0.042f, 0.038f), 0.85f);
		const FSurf Ice = FSurf::Glass(FLinearColor(0.62f, 0.8f, 0.95f), 0.06f), DeepIce = FSurf::Glass(FLinearColor(0.42f, 0.65f, 0.9f), 0.1f);
		const FSurf Ember = FSurf::Lit(FLinearColor(1.f, 0.36f, 0.06f), 0.9f), Sulfur = FSurf::Stone(FLinearColor(0.42f, 0.33f, 0.05f), 0.75f);
		const FSurf Bone = FSurf::Stone(FLinearColor(0.72f, 0.66f, 0.55f), 0.6f), Pool = FSurf::Glass(FLinearColor(0.5f, 0.72f, 0.88f), 0.04f);
		int32 Seed = 0;
		for (const FWRLandCaveFeat& F : Cave.Feats)
		{
			++Seed;
			const FVector At = Land->ToWorld(FVector2D(F.At), F.At.Z) - GetActorLocation();
			const float A = F.A * 100.f, B = F.B * 100.f;
			const FName Kd = F.Kind;
			if (Kd == TEXT("drip") || Kd == TEXT("icicle"))
			{
				// Hanging from the ceiling (its root sunk into the rock).
				if (bDraw) K.Spike(Kd == TEXT("icicle") ? Ice : Dripstone, At, -FVector::UpVector, B * (Kd == TEXT("icicle") ? 2.f : 3.2f), A);
			}
			else if (Kd == TEXT("mite") || Kd == TEXT("icemite"))
			{
				// A stalagmite: a cone on a wider foot.
				const FSurf& Sf = Kd == TEXT("icemite") ? Ice : Dripstone;
				if (bDraw) { K.Spike(Sf, At, FVector::UpVector, B * 2.f, A); K.Spike(Sf, At, FVector::UpVector, B * 3.2f, A * 0.35f); }
				Block(At, B * 0.8f, A);
			}
			else if (Kd == TEXT("column") || Kd == TEXT("icecolumn"))
			{
				// A column: a stalagmite and a stalactite grown together, waisted where they met.
				const FSurf& Sf = Kd == TEXT("icecolumn") ? Ice : Dripstone;
				if (bDraw)
				{
					K.Spike(Sf, At, FVector::UpVector, B * 2.6f, A * 0.62f);
					K.Spike(Sf, At + FVector(0.f, 0.f, A), -FVector::UpVector, B * 2.2f, A * 0.62f);
					K.Rod(Sf, At, At + FVector(0.f, 0.f, A), B * 0.9f);
				}
				Block(At, B, A);
			}
			else if (Kd == TEXT("rock") || Kd == TEXT("rimrock"))
			{
				SetRock(Seed, FVector(A, A * 0.85f, B), At, F.Yaw, RockMat);
			}
			else if (Kd == TEXT("bench"))
			{
				// A lava bench: the old flow's shelf along the wall, long and flat-topped.
				SetRock(Seed + 3, FVector(A, 130.f, B), At, F.Yaw, RockMat);
			}
			else if (Kd == TEXT("crack") && bDraw)
			{
				// A seam of the floor still glowing: a lit strip in a black lip.
				const FRotator R(0.f, F.Yaw, 0.f);
				const FVector Mid = At + R.Vector() * (A * 0.5f);
				K.Add(Basalt, FVector(A + 10.f, B * 3.f, 6.f), Mid, R);
				K.Add(Ember, FVector(A, B, 7.f), Mid + FVector(0.f, 0.f, 1.f), R);
			}
			else if (Kd == TEXT("sulfur") && bDraw)
			{
				// A sulfur crust: a cluster of small ochre crystals round a vent.
				for (int32 k = 0; k < 4; ++k)
				{
					const FVector O = FRotator(0.f, float(Seed * 37 + k * 90 + 20), 0.f).Vector() * (A * 0.35f) * (k ? 1.f : 0.f);
					K.Spike(Sulfur, At + O - FVector(0.f, 0.f, 4.f), FRotator(70.f + 8.f * k, float(Seed * 53 + k * 70), 0.f).Vector(), A * 0.18f, A * (0.3f + 0.1f * k));
				}
			}
			else if (Kd == TEXT("icefall"))
			{
				// A frozen fall: ice poured down the wall and set, thicker at its foot.
				if (bDraw)
				{
					K.Add(Ice, FVector(B * 2.f, B * 1.6f, A), At + FVector(0.f, 0.f, A * 0.5f), FRotator(0.f, float(Seed * 53 % 360), 0.f), EWRLookMesh::Cylinder);
					K.Spike(DeepIce, At, FVector::UpVector, B * 3.f, A * 0.3f);
					K.Spike(Ice, At + FVector(0.f, 0.f, A * 0.55f), -FVector::UpVector, B * 1.6f, A * 0.35f);
				}
				Block(At, B * 0.9f, A);
			}
			else if (Kd == TEXT("pool") && bDraw)
			{
				K.Add(Pool, FVector(A * 2.f, A * 1.7f, 5.f), At, FRotator(0.f, float(Seed * 29 % 360), 0.f), EWRLookMesh::Cylinder);
			}
			else if (Kd == TEXT("trapped") && bDraw)
			{
				// A wall of clear ice with something old in it: a great tusk and ribs half out of the ice.
				const FRotator R(0.f, F.Yaw, 0.f);
				const FVector Fwd = R.Vector(), Side = FVector::CrossProduct(FVector::UpVector, Fwd);
				K.Add(DeepIce, FVector(90.f, 420.f, 300.f), At + FVector(0.f, 0.f, 150.f), R);
				K.Rod(Bone, At + Fwd * 20.f + Side * -120.f + FVector(0.f, 0.f, 60.f), At + Fwd * 110.f + Side * 40.f + FVector(0.f, 0.f, 150.f), 22.f);
				K.Rod(Bone, At + Fwd * 110.f + Side * 40.f + FVector(0.f, 0.f, 150.f), At + Fwd * 150.f + Side * 120.f + FVector(0.f, 0.f, 120.f), 16.f);
				for (int32 r = 0; r < 4; ++r) K.Rod(Bone, At + Fwd * 10.f + Side * (60.f + 30.f * r) + FVector(0.f, 0.f, 40.f), At + Fwd * 70.f + Side * (60.f + 30.f * r) + FVector(0.f, 0.f, 120.f + 15.f * r), 8.f);
			}
			++Count;
		}
		if (bDraw)
		{
			for (const FWRLandCaveLight& Lt : Cave.Lights)
			{
				UPointLightComponent* P = NewObject<UPointLightComponent>(this);
				P->SetupAttachment(RootComponent);
				P->SetRelativeLocation(Land->ToWorld(FVector2D(Lt.At), Lt.At.Z) - GetActorLocation());
				P->SetLightColor(Lt.Color);
				P->SetIntensity(Lt.Intensity);
				P->SetAttenuationRadius(Lt.Radius * 100.f);
				P->SetCastShadows(false);
				P->RegisterComponent();
			}
		}
	}
	if (bDraw) FWRBuildLook::Spawn(this, RootComponent, Blocks, FLinearColor(0.3f, 0.25f, 0.2f), false);
	UE_LOG(LogWildreach, Log, TEXT("LAND CAVES formations: %d (%d look blocks, %d pillars)"), Count, Blocks.Num(), Solid);
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
		// The Tundra's walls are coloured ice (M_HeldTool, their vertex colours); the Volcanic's the textured basalt.
		const bool bIceWalls = C.IceColours.Num() == C.Mesh.Verts.Num() && C.IceColours.Num() > 0;
		Mesh->CreateMeshSection(Section, C.Mesh.Verts, C.Mesh.Tris, C.Mesh.Normals, C.Mesh.UVs, bIceWalls ? C.IceColours : C.Mesh.Colours, TArray<FProcMeshTangent>(), true);
		Mesh->SetMaterial(Section++, bIceWalls ? FWRToolMesh::Material() : WallMaterial(C.Biome));
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
