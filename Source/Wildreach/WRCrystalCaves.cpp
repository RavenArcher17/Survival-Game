// The Crystal caves' rock and crystal linings (see WRCrystalCaves.h; cooks/build-biome-crystal.md 2.3, 2.6).

#include "WRCrystalCaves.h"
#include "Wildreach.h"
#include "WRCrystalTypes.h"
#include "WRLand.h"
#include "WRToolMesh.h"
#include "WRTuning.h"
#include "Async/ParallelFor.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"

namespace WRCrystalCavesImpl
{
	FLinearColor Lin(uint8 R, uint8 G, uint8 B) { return FLinearColor(FColor(R, G, B)); }
	float Sm(float E0, float E1, float X) { const float T = FMath::Clamp((X - E0) / (E1 - E0), 0.f, 1.f); return T * T * (3.f - 2.f * T); }
	float N2(float X, float Y)
	{
		const int32 X0 = FMath::FloorToInt(X), Y0 = FMath::FloorToInt(Y);
		const float FX = X - X0, FY = Y - Y0;
		const auto H = [](int32 A, int32 B) { uint32 V = uint32(A) * 0x8da6b343u ^ uint32(B) * 0xd8163841u ^ 0x51ed27u; V ^= V >> 13; V *= 0x5bd1e995u; V ^= V >> 15; return float(V & 0xffff) / 32767.5f - 1.f; };
		const float SX = FX * FX * (3.f - 2.f * FX), SY = FY * FY * (3.f - 2.f * FY);
		return FMath::Lerp(FMath::Lerp(H(X0, Y0), H(X0 + 1, Y0), SX), FMath::Lerp(H(X0, Y0 + 1), H(X0 + 1, Y0 + 1), SX), SY);
	}
	float N3(const FVector& P) { return N2(P.X + P.Z * 0.37f, P.Y - P.Z * 0.61f) * 0.6f + N2(P.Y * 0.73f + 11.f, P.Z + P.X * 0.29f) * 0.4f; }

	const FWRLandDef* Waste()
	{
		const FWRLandDef* W = WRLand::Find(TEXT("wasteland"));
		return W && WRLand::IsLive(*W) ? W : nullptr;
	}
	/** The ground (world cm) over the caves: the Crystal land's, and the Wasteland's Spine tip where it stands higher. */
	float GroundZ(const FWRLandDef* L, const FWRLandDef* W, float X, float Y)
	{
		float Z = -1.0e7f;
		if (L) { const FVector2D M = L->ToMap(FVector(X, Y, 0.f)); Z = L->ToWorld(M, L->Height(M)).Z; }
		if (W)
		{
			const FVector2D M = W->ToMap(FVector(X, Y, 0.f));
			if (W->CoastDistance(M) > -40.f) Z = FMath::Max(Z, float(W->ToWorld(M, W->Height(M)).Z));
		}
		return Z;
	}
	/** Whether a land draws its own ground at this point (the caves' skin there is dropped). */
	bool LandDraws(const FWRLandDef* L, const FWRLandDef* W, float X, float Y)
	{
		if (L)
		{
			const FVector2D M = L->ToMap(FVector(X, Y, 0.f));
			const float S = L->HfStep;
			const FVector2D G = (M - L->HfOrigin) / S;
			const int32 I = FMath::FloorToInt(G.X), J = FMath::FloorToInt(G.Y);
			bool bAll = true;
			for (int32 dj = 0; dj <= 1 && bAll; ++dj) for (int32 di = 0; di <= 1; ++di)
			{
				const FVector2D C = L->HfOrigin + FVector2D(float(I + di), float(J + dj)) * S;
				if (L->HfHiddenAt(C)) { bAll = false; break; }
			}
			if (bAll) return true;
		}
		if (W) return W->CoastDistance(W->ToMap(FVector(X, Y, 0.f))) > 0.f;
		return false;
	}

	/** The rock's colour (linear) and gloss at a point of the cave's skin, from the shape it belongs to. */
	FLinearColor RockColour(const FWRCrystalData& D, int32 Shape, const FVector& P, const FVector& N, float GroundZw, float& OutGloss)
	{
		const float Patch = N2(P.X / 410.f, P.Y / 410.f) * 0.6f + N2(P.X / 97.f + 3.f, P.Y / 97.f) * 0.4f;
		const bool bFloor = N.Z > 0.62f;
		// The skin that rolls over a mouth's lip is the plateau's basalt.
		if (P.Z > GroundZw - 300.f)
		{
			OutGloss = 0.15f;
			return Lin(28, 28, 32) * (0.9f + 0.2f * Patch);
		}
		const FName Zone = D.ShapeZone(Shape);
		FName Kind = NAME_None;
		float Floor = P.Z, H = 1000.f;
		float Blend = 0.f;
		if (Shape >= 0 && Shape < D.Halls.Num())
		{
			const FWRCrystalData::FHall& Hl = D.Halls[Shape];
			Kind = Hl.Kind;
			Floor = Hl.At.Z;
			H = Hl.H;
		}
		else if (Shape >= D.Halls.Num() && Shape < D.Halls.Num() + D.Tubes.Num())
		{
			const FWRCrystalData::FTube& T = D.Tubes[Shape - D.Halls.Num()];
			Kind = T.Kind;
			float Fl = 0.f, Tt = 0.f; int32 Seg = 0; FVector Dir;
			T.Nearest(P, Fl, Seg, Tt, Dir);
			Floor = Fl;
			H = T.Kind == TEXT("passage") ? T.H : T.R * 1.55f;
			// The throat from the Geode Halls: violet fades into the tubes' black over its length (crystal 2.2 blends).
			if (!T.Blend.IsNone() && T.Pts.Num() > 1) Blend = 1.f - Sm(0.15f, 0.85f, (Seg + Tt) / float(T.Pts.Num() - 1));
		}
		const float Up = FMath::Max(0.f, float(P.Z) - Floor);
		const float Rel = Up / FMath::Max(100.f, H);
		const auto Geode = [&](float& G) -> FLinearColor
		{
			if (bFloor)
			{
				// Quartz sand worn level, crystal dust drifted into lilac-white patches.
				G = 0.62f;
				return FMath::Lerp(Lin(196, 188, 180), Lin(216, 208, 255), Sm(0.15f, 0.55f, Patch)) * 0.62f;
			}
			if (Rel < 0.3f)
			{
				// Agate bands round the lower walls (the real cross-section): cream, rust, blue-grey in 30-80 cm bands.
				const float Band = FMath::Frac(Up / (52.f + 18.f * N2(P.X / 700.f, P.Y / 700.f)) + 0.15f * N3(P / 300.f));
				const FLinearColor C = Band < 0.4f ? Lin(230, 216, 192) : Band < 0.72f ? Lin(168, 90, 58) : Lin(142, 138, 154);
				G = 0.35f;
				return FMath::Lerp(C, Lin(237, 234, 244), Sm(0.22f, 0.3f, Rel)) * 0.6f;
			}
			// Quartz white giving way to amethyst violet up the dome.
			G = 0.5f;
			return FMath::Lerp(Lin(237, 234, 244) * 0.55f, FMath::Lerp(Lin(107, 63, 160), Lin(78, 46, 122), Sm(-0.3f, 0.5f, Patch)) * 0.7f, Sm(0.3f, 0.45f, Rel));
		};
		const auto Tubes = [&](float& G) -> FLinearColor
		{
			if (bFloor)
			{
				// Old lava shelf; guano grey drifted in patches under the roosts.
				G = 0.7f;
				return FMath::Lerp(Lin(30, 28, 32), Lin(140, 134, 120), Sm(0.25f, 0.6f, Patch)) * 0.62f;
			}
			// Lava glaze: black with a glassy sheen.
			G = 0.22f;
			return Lin(18, 18, 22) * (0.9f + 0.25f * Patch);
		};
		const auto Hewn = [&](float& G) -> FLinearColor
		{
			if (bFloor)
			{
				// Polished black basalt, the Chladni dust lying lilac-white on it.
				G = 0.2f;
				return FMath::Lerp(Lin(20, 20, 24), Lin(216, 208, 255) * 0.7f, Sm(0.35f, 0.75f, Patch) * 0.6f) * 0.7f;
			}
			// Dressed basalt in courses (a darker joint every 60 cm).
			G = 0.82f;
			const float Course = FMath::Frac(P.Z / 60.f);
			return Lin(42, 42, 48) * (Course < 0.06f ? 0.55f : 0.9f + 0.15f * Patch);
		};
		FLinearColor C;
		if (Zone == TEXT("V"))
		{
			C = Hewn(OutGloss) * 0.8f;
		}
		else if (Kind == TEXT("chamber") || Kind == TEXT("passage"))
		{
			C = Hewn(OutGloss);
		}
		else if (Kind == TEXT("prism"))
		{
			if (bFloor) { OutGloss = 0.15f; C = Lin(22, 22, 26) * (0.8f + 0.2f * Patch); }
			else { OutGloss = 0.45f; C = FMath::Lerp(Lin(107, 63, 160), Lin(60, 34, 98), Sm(-0.2f, 0.6f, Patch)) * 0.7f; }
		}
		else if (Zone == TEXT("C3"))
		{
			if (bFloor)
			{
				// Mineral crust: pale, with gold where the hot water stood.
				OutGloss = 0.5f;
				C = FMath::Lerp(Lin(214, 205, 176), Lin(200, 160, 72), Sm(0.3f, 0.7f, Patch)) * 0.6f;
			}
			else
			{
				// Gypsum walls: mist grey with white growth bands.
				OutGloss = 0.55f;
				const float Band = FMath::Frac(Up / 140.f + 0.2f * Patch);
				C = (Band < 0.3f ? Lin(242, 240, 232) : Lin(184, 188, 192)) * 0.62f;
			}
		}
		else if (Zone == TEXT("C4"))
		{
			C = Tubes(OutGloss);
			if (Blend > 0.f)
			{
				float G2 = 0.5f;
				C = FMath::Lerp(C, Geode(G2), Blend);
				OutGloss = FMath::Lerp(OutGloss, G2, Blend);
			}
		}
		else
		{
			C = Geode(OutGloss);
		}
		return C;
	}

	// ---- the crystal lining's mesh (flat-shaded prisms, M_CrystalGlow vertex colours: rgb colour, alpha glow)
	struct FLining
	{
		TArray<FVector> V;
		TArray<int32> I;
		TArray<FVector> N;
		TArray<FColor> C;
		TArray<FVector2D> UV;
		/** A face seen from outside its solid: Out is the outward side. */
		void Tri(const FVector& A, const FVector& B, const FVector& Cc, const FColor& CA, const FColor& CB, const FColor& CC, const FVector& Out)
		{
			FVector Nrm = FVector::CrossProduct(B - A, Cc - A).GetSafeNormal();
			const int32 F = V.Num();
			// Unreal draws the side where the stored order's cross points away from the viewer: store it pointing in.
			if (FVector::DotProduct(Nrm, Out) > 0.f) { V.Append({ A, Cc, B }); C.Append({ CA, CC, CB }); }
			else { V.Append({ A, B, Cc }); C.Append({ CA, CB, CC }); Nrm = -Nrm; }
			if (FVector::DotProduct(Nrm, Out) < 0.f) Nrm = -Nrm;
			N.Append({ Nrm, Nrm, Nrm });
			UV.Append({ FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector });
			I.Append({ F, F + 1, F + 2 });
		}
		/** One quartz point: a six-sided prism from Root along Dir, its six-faced pyramid tip (the real habit). */
		void Point(const FVector& Root, const FVector& Dir, float Len, float Thick, const FLinearColor& Base, const FLinearColor& Mid, const FLinearColor& Tip, float Turn)
		{
			FVector AY, AZ;
			Dir.FindBestAxisVectors(AY, AZ);
			const float R = Thick * 0.5f;
			const FVector Sh = Root + Dir * (Len * 0.8f), Tp = Root + Dir * Len;
			const FColor C0 = FWRToolMesh::Pack(Base, 0.3f), C1 = FWRToolMesh::Pack(Mid, 0.75f), C2 = FWRToolMesh::Pack(Tip, 1.f);
			FVector Ring0[6], Ring1[6];
			for (int32 k = 0; k < 6; ++k)
			{
				const float A = FMath::DegreesToRadians(Turn + 60.f * k);
				const FVector O = AY * FMath::Cos(A) + AZ * FMath::Sin(A);
				Ring0[k] = Root + O * R * 0.92f;
				Ring1[k] = Sh + O * R;
			}
			for (int32 k = 0; k < 6; ++k)
			{
				const int32 k1 = (k + 1) % 6;
				const FVector Out = ((Ring1[k] + Ring1[k1]) * 0.5f - Sh).GetSafeNormal();
				Tri(Ring0[k], Ring0[k1], Ring1[k1], C0, C0, C1, Out);
				Tri(Ring0[k], Ring1[k1], Ring1[k], C0, C1, C1, Out);
				const FVector OutT = (((Ring1[k] + Ring1[k1]) * 0.5f - Sh).GetSafeNormal() + Dir * 0.6f).GetSafeNormal();
				Tri(Ring1[k], Ring1[k1], Tp, C1, C1, C2, OutT);
			}
		}
	};
}

AWRCrystalCaves::AWRCrystalCaves()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

float AWRCrystalCaves::CubeSize()
{
	return FMath::Max(50.f, WRTune(TEXT("crystalWorld.caves.cell"), 125.f)) * FMath::Max(8, int32(WRTune(TEXT("crystalWorld.caves.cells"), 25.f)));
}

void AWRCrystalCaves::BeginPlay()
{
	Super::BeginPlay();
	Cell = FMath::Max(50.f, WRTune(TEXT("crystalWorld.caves.cell"), 125.f));
	Cells = FMath::Max(8, int32(WRTune(TEXT("crystalWorld.caves.cells"), 25.f)));
	QueueCubes();
}

void AWRCrystalCaves::QueueCubes()
{
	const FWRCrystalData& D = FWRCrystalData::Get();
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
	// The descents and the camps' ways down first (where a newcomer goes in), then outward.
	FVector Focus = D.CaveBounds.GetCenter();
	if (const FWRCrystalData::FDescent* Dn = D.Descents.Num() ? &D.Descents[0] : nullptr) Focus = Dn->At;
	Queue.Sort([S, Focus](const FIntVector& A, const FIntVector& B)
	{
		const FVector CA((A.X + 0.5f) * S, (A.Y + 0.5f) * S, (A.Z + 0.5f) * S), CB((B.X + 0.5f) * S, (B.Y + 0.5f) * S, (B.Z + 0.5f) * S);
		return FVector::DistSquared(CA, Focus) < FVector::DistSquared(CB, Focus);
	});
	for (int32 h = 0; h < D.Halls.Num(); ++h)
	{
		const FName K = D.Halls[h].Kind;
		if (K == TEXT("geode") || K == TEXT("beams") || K == TEXT("prism") || K == TEXT("alcove")) HallOrder.Add(h);
	}
	bQueued = true;
	UE_LOG(LogWildreach, Log, TEXT("CRYSTAL caves: %d rock cubes of %.0f m and %d linings to build"), Queue.Num(), S / 100.f, HallOrder.Num());
}

bool AWRCrystalCaves::IsBuiltAt(const FVector& World) const
{
	const float S = Cell * Cells;
	return Done.Contains(FIntVector(FMath::FloorToInt(World.X / S), FMath::FloorToInt(World.Y / S), FMath::FloorToInt(World.Z / S)));
}

UMaterialInstanceDynamic* AWRCrystalCaves::HallGlow(int32 Hall) const
{
	const TObjectPtr<UMaterialInstanceDynamic>* M = Glows.Find(Hall);
	return M ? M->Get() : nullptr;
}

void AWRCrystalCaves::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bQueued) return;
	// A time budget a frame (the cubes cost a few milliseconds each), so loading in never hitches.
	const double Budget = WRTune(TEXT("crystalWorld.caves.msPerFrame"), 6.f) / 1000.0;
	const double Start = FPlatformTime::Seconds();
	while (Next < Queue.Num() && FPlatformTime::Seconds() - Start < Budget)
	{
		BuildCube(Queue[Next]);
		Done.Add(Queue[Next]);
		++Next;
	}
	if (Next >= Queue.Num() && GetNetMode() != NM_DedicatedServer)
	{
		while (NextHall < HallOrder.Num() && FPlatformTime::Seconds() - Start < Budget) BuildLining(HallOrder[NextHall++]);
	}
	else if (Next >= Queue.Num()) NextHall = HallOrder.Num();
	if (IsBuilt())
	{
		SetActorTickEnabled(false);
		UE_LOG(LogWildreach, Log, TEXT("CRYSTAL caves built: %d rock meshes, %d linings"), Rock.Num(), Linings.Num());
	}
}

bool AWRCrystalCaves::BuildCube(const FIntVector& Key)
{
	using namespace WRCrystalCavesImpl;
	const FWRCrystalData& D = FWRCrystalData::Get();
	const FWRLandDef* L = FWRCrystalData::Land();
	const FWRLandDef* W = Waste();
	const float S = Cell * Cells;
	const FVector O(Key.X * S, Key.Y * S, Key.Z * S);
	TArray<int32> Shapes;
	D.ShapesIn(FBox(O - FVector(Cell * 2.f), O + FVector(S + Cell * 2.f)), Shapes);
	if (Shapes.Num() == 0) return false;
	const int32 NS = Cells + 3, NC = Cells + 2;
	// The ground over each column of samples (only matters where a cube reaches up near it: a mouth).
	TArray<float> Ground;
	Ground.SetNumUninitialized(NS * NS);
	float MinGround = 1.0e9f;
	for (int32 j = 0; j < NS; ++j) for (int32 i = 0; i < NS; ++i)
	{
		const float G = GroundZ(L, W, O.X + (i - 1) * Cell, O.Y + (j - 1) * Cell);
		Ground[j * NS + i] = G;
		MinGround = FMath::Min(MinGround, G);
	}
	const bool bNearGround = O.Z + S + Cell * 2.f > MinGround - 400.f;
	TArray<float> F;
	F.SetNumUninitialized(NS * NS * NS);
	ParallelFor(NS, [&](int32 k)
	{
		for (int32 j = 0; j < NS; ++j) for (int32 i = 0; i < NS; ++i)
		{
			const FVector P = O + FVector((i - 1) * Cell, (j - 1) * Cell, (k - 1) * Cell);
			float V = 1.0e6f;
			for (int32 s : Shapes) V = FMath::Min(V, D.ShapeAir(s, P));
			// The open air over the ground is air too (negative above it), so a mouth's rock rolls into the ground.
			if (bNearGround) V = FMath::Min(V, Ground[j * NS + i] - float(P.Z));
			F[(k * NS + j) * NS + i] = V;
		}
	});
	const auto Fv = [&](int32 i, int32 j, int32 k) { return F[((k + 1) * NS + (j + 1)) * NS + (i + 1)]; };
	// Surface nets: one vertex in every cell the surface crosses, at the mean of its edges' crossings.
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
		// The normal points into the air (down the field's slope), from the cell's corner differences.
		const FVector G(
			(Cv[1] - Cv[0]) + (Cv[3] - Cv[2]) + (Cv[5] - Cv[4]) + (Cv[7] - Cv[6]),
			(Cv[2] - Cv[0]) + (Cv[3] - Cv[1]) + (Cv[6] - Cv[4]) + (Cv[7] - Cv[5]),
			(Cv[4] - Cv[0]) + (Cv[5] - Cv[1]) + (Cv[6] - Cv[2]) + (Cv[7] - Cv[3]));
		const FVector Nrm = (-G).GetSafeNormal();
		CellVert[((k + 1) * NC + (j + 1)) * NC + (i + 1)] = Verts.Num();
		Verts.Add(P - GetActorLocation());
		Normals.Add(Nrm);
		UVs.Add(FVector2D(P.X / 400.f, P.Y / 400.f));
		int32 Near = INDEX_NONE;
		float Best = 1.0e9f;
		for (int32 s : Shapes) { const float A = D.ShapeAir(s, P); if (A < Best) { Best = A; Near = s; } }
		const float Gz = GroundZ(L, W, P.X, P.Y);
		float Gloss = 0.5f;
		const FLinearColor C = RockColour(D, Near, P, Nrm, Gz, Gloss);
		Colours.Add(FWRToolMesh::Pack(C, 1.f - Gloss));
	}
	const auto CV = [&](int32 i, int32 j, int32 k) { return CellVert[((k + 1) * NC + (j + 1)) * NC + (i + 1)]; };
	TArray<int32> Tris;
	const auto Quad = [&](int32 A, int32 B, int32 C, int32 E, const FVector& RockDir)
	{
		if (A < 0 || B < 0 || C < 0 || E < 0) return;
		// The skin that lies on ground a land already draws is that land's (dropped here, so nothing doubles up).
		const FVector Mid = (Verts[A] + Verts[B] + Verts[C] + Verts[E]) * 0.25f + GetActorLocation();
		if (bNearGround && FMath::Abs(Mid.Z - GroundZ(L, W, Mid.X, Mid.Y)) < WRTune(TEXT("crystalWorld.caves.groundSkin"), 120.f) && LandDraws(L, W, Mid.X, Mid.Y)) return;
		// Unreal draws the side where the stored order's cross points away from the viewer (who is in the air).
		const FVector Cr = FVector::CrossProduct(Verts[B] - Verts[A], Verts[C] - Verts[A]);
		if (FVector::DotProduct(Cr, RockDir) >= 0.f) Tris.Append({ A, B, C, A, C, E });
		else Tris.Append({ A, C, B, A, E, C });
	};
	for (int32 k = 0; k < Cells; ++k) for (int32 j = 0; j < Cells; ++j) for (int32 i = 0; i < Cells; ++i)
	{
		const float V0 = Fv(i, j, k);
		const float Sign = V0 < 0.f ? 1.f : -1.f;   // from the air at p0 the rock lies along +axis
		if ((V0 < 0.f) != (Fv(i + 1, j, k) < 0.f)) Quad(CV(i, j - 1, k - 1), CV(i, j, k - 1), CV(i, j, k), CV(i, j - 1, k), FVector(Sign, 0.f, 0.f));
		if ((V0 < 0.f) != (Fv(i, j + 1, k) < 0.f)) Quad(CV(i - 1, j, k - 1), CV(i - 1, j, k), CV(i, j, k), CV(i, j, k - 1), FVector(0.f, Sign, 0.f));
		if ((V0 < 0.f) != (Fv(i, j, k + 1) < 0.f)) Quad(CV(i - 1, j - 1, k), CV(i, j - 1, k), CV(i, j, k), CV(i - 1, j, k), FVector(0.f, 0.f, Sign));
	}
	if (Tris.Num() == 0) return false;
	UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this);
	Mesh->SetupAttachment(RootComponent);
	Mesh->bUseAsyncCooking = true;
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	Mesh->SetCollisionObjectType(ECC_WorldStatic);
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->CreateMeshSection(0, Verts, Tris, Normals, UVs, Colours, TArray<FProcMeshTangent>(), true);
	Mesh->SetMaterial(0, FWRToolMesh::Material());
	Mesh->SetCullDistance(WRTune(TEXT("crystalWorld.caves.cullDistance"), 30000.f));
	Mesh->RegisterComponent();
	Rock.Add(Mesh);
	return true;
}

void AWRCrystalCaves::BuildLining(int32 Hall)
{
	using namespace WRCrystalCavesImpl;
	const FWRCrystalData& D = FWRCrystalData::Get();
	if (!D.Halls.IsValidIndex(Hall)) return;
	const FWRCrystalData::FHall& H = D.Halls[Hall];
	FRandomStream R(int32(GetTypeHash(H.Id)) & 0x7fffffff);
	const FVector C = H.Centre();
	const FVector Rad(H.Rx, H.Ry, H.Rz());
	// Clusters by wall area (crystal 2.2: points 0.5-2 m long, clusters of 3-9 round a common root).
	const float A = 4.f * PI * FMath::Pow((FMath::Pow(Rad.X * Rad.Y, 1.6f) + FMath::Pow(Rad.X * Rad.Z, 1.6f) + FMath::Pow(Rad.Y * Rad.Z, 1.6f)) / 3.f, 1.f / 1.6f) / 10000.f;
	const bool bSelenite = H.Zone == TEXT("C3");
	const float PerM2 = WRTune(bSelenite ? TEXT("crystalWorld.lining.selenitePerM2") : TEXT("crystalWorld.lining.clustersPerM2"), bSelenite ? 0.012f : 0.03f);
	const int32 Want = FMath::Clamp(int32(A * 0.7f * PerM2), 6, int32(WRTune(TEXT("crystalWorld.lining.maxClusters"), 260.f)));
	const float Size = FMath::Clamp(FMath::Min(H.Rx, H.Ry) / 2500.f, 0.45f, 1.6f);
	FLinearColor Base = Lin(62, 34, 102), Mid = Lin(107, 63, 160), Tip = Lin(205, 184, 242);
	FLinearColor GlowCol = Lin(158, 108, 255);
	if (bSelenite) { Base = Lin(196, 192, 182); Mid = Lin(232, 228, 216); Tip = Lin(246, 244, 238); GlowCol = Lin(255, 236, 196); }
	else if (H.Zone == TEXT("C4")) { Base = Lin(120, 110, 150); Mid = Lin(200, 190, 240); Tip = Lin(216, 208, 255); GlowCol = Lin(216, 208, 255); }
	FLining M;
	TArray<TPair<float, FVector>> Big;
	int32 Made = 0;
	for (int32 Try = 0; Try < Want * 4 && Made < Want; ++Try)
	{
		// A direction weighted to the dome (most on the walls and ceiling, a few round the floor's edge).
		const float Az = R.FRandRange(0.f, 2.f * PI);
		const float El = FMath::Asin(R.FRandRange(-0.25f, 1.f));
		const FVector U(FMath::Cos(El) * FMath::Cos(Az), FMath::Cos(El) * FMath::Sin(Az), FMath::Sin(El));
		const FVector Ud = FVector(FRotator(0.f, H.Yaw, 0.f).RotateVector(FVector(U.X * Rad.X, U.Y * Rad.Y, U.Z * Rad.Z))).GetSafeNormal();
		const float MaxR = FMath::Max3(Rad.X, Rad.Y, Rad.Z) * 1.4f;
		float Lo = 0.f, Hi = -1.f;
		for (float T = 100.f; T < MaxR; T += 60.f)
		{
			if (D.Air(C + Ud * T) >= 0.f) { Hi = T; break; }
			Lo = T;
		}
		if (Hi < 0.f) continue;
		for (int32 b = 0; b < 7; ++b) { const float Md = (Lo + Hi) * 0.5f; (D.Air(C + Ud * Md) >= 0.f ? Hi : Lo) = Md; }
		const FVector W = C + Ud * Lo;
		int32 Near = INDEX_NONE;
		D.Air(W, &Near);
		if (Near != Hall) continue;   // a tube's mouth or a shaft: no lining there
		const FVector G(D.Air(W + FVector(30, 0, 0)) - D.Air(W - FVector(30, 0, 0)), D.Air(W + FVector(0, 30, 0)) - D.Air(W - FVector(0, 30, 0)), D.Air(W + FVector(0, 0, 30)) - D.Air(W - FVector(0, 0, 30)));
		const FVector Nrm = (-G).GetSafeNormal();
		if (Nrm.IsNearlyZero()) continue;
		// Never on the floor you walk (it would trip you): only where the wall meets it.
		if (W.Z < H.At.Z + 150.f && Nrm.Z > 0.45f) continue;
		const int32 K = R.RandRange(3, 9);
		const float L0 = R.FRandRange(50.f, 200.f) * Size;
		for (int32 p = 0; p < K; ++p)
		{
			FVector Dir = Nrm.RotateAngleAxis(R.FRandRange(-25.f, 25.f), FVector::CrossProduct(Nrm, FVector(R.FRandRange(-1.f, 1.f), R.FRandRange(-1.f, 1.f), R.FRandRange(-1.f, 1.f))).GetSafeNormal());
			if (Dir.IsNearlyZero()) Dir = Nrm;
			FVector T1, T2;
			Nrm.FindBestAxisVectors(T1, T2);
			const float Len = L0 * (p == 0 ? 1.f : R.FRandRange(0.35f, 0.85f));
			const FVector Root = W - Nrm * 25.f + (T1 * R.FRandRange(-1.f, 1.f) + T2 * R.FRandRange(-1.f, 1.f)) * L0 * 0.22f;
			const float Th = Len * R.FRandRange(bSelenite ? 0.12f : 0.18f, bSelenite ? 0.2f : 0.3f);
			M.Point(Root - GetActorLocation(), Dir, Len, Th, Base, Mid, Tip, R.FRandRange(0.f, 60.f));
		}
		Big.Add(TPair<float, FVector>(L0 * K * (W.Z > H.At.Z + H.H * 0.4f ? 1.5f : 1.f), W));
		++Made;
	}
	if (M.V.Num() == 0) return;
	UProceduralMeshComponent* P = NewObject<UProceduralMeshComponent>(this);
	P->SetupAttachment(RootComponent);
	P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	P->SetCanEverAffectNavigation(false);
	P->CreateMeshSection(0, M.V, M.I, M.N, M.UV, M.C, TArray<FProcMeshTangent>(), false);
	P->SetCastShadow(false);
	P->SetCullDistance(WRTune(TEXT("crystalWorld.caves.cullDistance"), 30000.f));
	if (UMaterialInterface* Base0 = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_CrystalGlow.M_CrystalGlow")))
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base0, this);
		MID->SetVectorParameterValue(TEXT("GlowColor"), GlowCol);
		MID->SetScalarParameterValue(TEXT("GlowScale"), WRTune(TEXT("crystalWorld.glow.scale"), 6.f));
		MID->SetScalarParameterValue(TEXT("Rest"), WRTune(TEXT("crystalWorld.glow.rest"), 0.15f));
		MID->SetScalarParameterValue(TEXT("Lit"), 0.f);
		P->SetMaterial(0, MID);
		Glows.Add(Hall, MID);
	}
	else
	{
		P->SetMaterial(0, FWRToolMesh::Material());
	}
	P->RegisterComponent();
	Linings.Add(P);
	Big.Sort([](const TPair<float, FVector>& X, const TPair<float, FVector>& Y) { return X.Key > Y.Key; });
	TArray<FVector>& Pts = Clusters.FindOrAdd(Hall);
	for (int32 i = 0; i < Big.Num() && Pts.Num() < 3; ++i)
	{
		bool bFar = true;
		for (const FVector& Q : Pts) bFar &= FVector::Dist(Q, Big[i].Value) > FMath::Min(H.Rx, H.Ry) * 0.6f;
		if (bFar) Pts.Add(Big[i].Value);
	}
}
