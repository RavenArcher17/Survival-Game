// The Crystal caves' content (cooks/build-biome-crystal.md 2.0-2.13): Data/crystal.json parsed once, the caves' air
// shapes as one signed distance, and the calls the Crystal caves' parts share. See WRCrystalTypes.h.

#include "WRCrystalTypes.h"
#include "Wildreach.h"
#include "WRLand.h"
#include "WRFarm.h"
#include "WRTuning.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace WRCrystalTypesImpl
{
	const FName CBiome(TEXT("crystal"));

	double CNum(const TSharedPtr<FJsonObject>& O, const TCHAR* K, double Def = 0.0)
	{
		double V = Def;
		if (O.IsValid()) O->TryGetNumberField(K, V);
		return V;
	}
	bool CBool(const TSharedPtr<FJsonObject>& O, const TCHAR* K)
	{
		bool B = false;
		if (O.IsValid()) O->TryGetBoolField(K, B);
		return B;
	}
	FName CName(const TSharedPtr<FJsonObject>& O, const TCHAR* K)
	{
		FString S;
		if (O.IsValid() && O->TryGetStringField(K, S) && !S.IsEmpty()) return FName(*S);
		return NAME_None;
	}
	FString CStr(const TSharedPtr<FJsonObject>& O, const TCHAR* K)
	{
		FString S;
		if (O.IsValid()) O->TryGetStringField(K, S);
		return S;
	}
	/** [x, y, z] (map metres) -> world cm. */
	FVector CPoint(const TSharedPtr<FJsonValue>& V)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (!V.IsValid() || !V->TryGetArray(A) || A->Num() < 2) return FVector::ZeroVector;
		return FWRCrystalData::ToWorld((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), A->Num() > 2 ? (*A)[2]->AsNumber() : 0.0);
	}
	FVector CAt(const TSharedPtr<FJsonObject>& O, const TCHAR* ZKey = TEXT("z"))
	{
		return FWRCrystalData::ToWorld(CNum(O, TEXT("x")), CNum(O, TEXT("y")), CNum(O, ZKey));
	}
	TArray<TSharedPtr<FJsonValue>> CArr(const TSharedPtr<FJsonObject>& O, const TCHAR* K)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (O.IsValid() && O->TryGetArrayField(K, A)) return *A;
		return {};
	}

	float CSmooth(float E0, float E1, float X)
	{
		const float T = FMath::Clamp((X - E0) / (E1 - E0), 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	}
	/** 3D value noise in [-1, 1] (integer hash; P in noise cells). */
	float CHash(int32 X, int32 Y, int32 Z)
	{
		uint32 H = uint32(X) * 0x8da6b343u ^ uint32(Y) * 0xd8163841u ^ uint32(Z) * 0xcb1ab31fu ^ 0x7219u;
		H ^= H >> 13; H *= 0x5bd1e995u; H ^= H >> 15;
		return float(H & 0xffff) / 32767.5f - 1.f;
	}
	float CNoise3(const FVector& P)
	{
		const int32 X0 = FMath::FloorToInt(P.X), Y0 = FMath::FloorToInt(P.Y), Z0 = FMath::FloorToInt(P.Z);
		const float FX = P.X - X0, FY = P.Y - Y0, FZ = P.Z - Z0;
		const float SX = FX * FX * (3.f - 2.f * FX), SY = FY * FY * (3.f - 2.f * FY), SZ = FZ * FZ * (3.f - 2.f * FZ);
		const auto L2 = [&](int32 Z) {
			return FMath::Lerp(FMath::Lerp(CHash(X0, Y0, Z), CHash(X0 + 1, Y0, Z), SX), FMath::Lerp(CHash(X0, Y0 + 1, Z), CHash(X0 + 1, Y0 + 1, Z), SX), SY);
		};
		return FMath::Lerp(L2(Z0), L2(Z0 + 1), SZ);
	}
	/** An ellipsoid's signed distance (cm, the usual close bound): L in its own frame, R its radii. */
	float Ellipsoid(const FVector& L, const FVector& R)
	{
		const FVector Q(L.X / R.X, L.Y / R.Y, L.Z / R.Z);
		const FVector Q2(L.X / (R.X * R.X), L.Y / (R.Y * R.Y), L.Z / (R.Z * R.Z));
		const float K0 = Q.Size(), K1 = Q2.Size();
		if (K1 < 1.e-6f) return -FMath::Min3(R.X, R.Y, R.Z);
		return K0 * (K0 - 1.f) / K1;
	}
	float BoxSdf(const FVector& L, const FVector& Half)
	{
		const FVector Q(FMath::Abs(L.X) - Half.X, FMath::Abs(L.Y) - Half.Y, FMath::Abs(L.Z) - Half.Z);
		const FVector Out(FMath::Max(Q.X, 0.f), FMath::Max(Q.Y, 0.f), FMath::Max(Q.Z, 0.f));
		return Out.Size() + FMath::Min(FMath::Max3(Q.X, Q.Y, Q.Z), 0.f);
	}
	const FWRLandDef* CLand()
	{
		static const FWRLandDef* L = nullptr;
		static bool bTried = false;
		if (!bTried)
		{
			bTried = true;
			for (const FWRLandDef& D : WRLand::All()) if (D.Biome == CBiome) L = &D;
		}
		return L;
	}
	/** Pools of each hall (their basins are part of the hall's air). */
	TMap<int32, TArray<int32>>& HallPools()
	{
		static TMap<int32, TArray<int32>> M;
		return M;
	}
}

const FWRLandDef* FWRCrystalData::Land() { return WRCrystalTypesImpl::CLand(); }

FVector FWRCrystalData::ToWorld(double X, double Y, double Z)
{
	if (const FWRLandDef* L = Land()) return L->ToWorld(FVector2D(X, Y), float(Z));
	// No land file: the Wasteland's frame (origin (0, 9000 m), mapCentre (-850, +5,300)).
	return FVector((X + 850.0) * 100.0, (Y - 5300.0) * 100.0 + 900000.0, Z * 100.0);
}

FVector2D FWRCrystalData::FHall::Local(const FVector& P) const
{
	const FVector2D D(P.X - At.X, P.Y - At.Y);
	const float C = FMath::Cos(FMath::DegreesToRadians(Yaw)), S = FMath::Sin(FMath::DegreesToRadians(Yaw));
	return FVector2D(D.X * C + D.Y * S, -D.X * S + D.Y * C);
}

bool FWRCrystalData::FHall::Contains(const FVector& P, float Grow) const
{
	if (P.Z < At.Z - 300.f - Grow || P.Z > At.Z + H + Grow) return false;
	const FVector2D L = Local(P);
	if (IsHewn()) return FMath::Abs(L.X) <= Rx + Grow && FMath::Abs(L.Y) <= Ry + Grow;
	return FMath::Square(L.X / (Rx + Grow)) + FMath::Square(L.Y / (Ry + Grow)) <= 1.f;
}

float FWRCrystalData::FTube::Nearest(const FVector& P, float& OutFloor, int32& OutSeg, float& OutT, FVector& OutDir) const
{
	// Horizontal distance to the floor line (passages are hewn level across; tubes use it for their benches), the floor
	// height at the nearest point. Groups of 16 points are skipped by their box when they can't be nearer.
	float Best = TNumericLimits<float>::Max();
	OutSeg = 0;
	OutT = 0.f;
	const int32 N = Pts.Num();
	for (int32 G = 0; G + 1 < N; G += 16)
	{
		const int32 E = FMath::Min(N - 1, G + 16);
		FBox2D B(ForceInit);
		for (int32 i = G; i <= E; ++i) B += FVector2D(Pts[i]);
		const FVector2D P2(P);
		const float Dx = FMath::Max3<double>(B.Min.X - P2.X, 0.0, P2.X - B.Max.X), Dy = FMath::Max3<double>(B.Min.Y - P2.Y, 0.0, P2.Y - B.Max.Y);
		if (Dx * Dx + Dy * Dy > Best * Best) continue;
		for (int32 i = G; i < E; ++i)
		{
			const FVector2D A(Pts[i]), Bp(Pts[i + 1]), D = Bp - A;
			const float L2 = D.SizeSquared();
			const float T = L2 > 1.f ? FMath::Clamp(FVector2D::DotProduct(P2 - A, D) / L2, 0.f, 1.f) : 0.f;
			const float Dist = FVector2D::Distance(P2, A + D * T);
			if (Dist < Best) { Best = Dist; OutSeg = i; OutT = T; }
		}
	}
	if (N >= 2)
	{
		OutFloor = FMath::Lerp(Pts[OutSeg].Z, Pts[OutSeg + 1].Z, OutT);
		OutDir = (Pts[OutSeg + 1] - Pts[OutSeg]).GetSafeNormal();
	}
	else
	{
		OutFloor = N ? Pts[0].Z : 0.f;
		OutDir = FVector::ForwardVector;
	}
	return Best;
}

const FWRCrystalData& FWRCrystalData::Get()
{
	using namespace WRCrystalTypesImpl;
	static FWRCrystalData D;
	static bool bTried = false;
	if (bTried) return D;
	bTried = true;
	const TSharedPtr<FJsonObject> J = FWRTuning::LoadDataFile(TEXT("crystal.json"));
	if (!J.IsValid())
	{
		UE_LOG(LogWildreach, Error, TEXT("CRYSTAL: Data/crystal.json is missing (node Tools/terrain/crystal.mjs)"));
		return D;
	}
	for (const TSharedPtr<FJsonValue>& V : CArr(J, TEXT("halls")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FHall& H = D.Halls.AddDefaulted_GetRef();
		H.Id = CName(O, TEXT("id"));
		H.Kind = CName(O, TEXT("kind"));
		H.Zone = CName(O, TEXT("zone"));
		H.Name = CStr(O, TEXT("name"));
		H.At = CAt(O, TEXT("floor"));
		H.Rx = float(CNum(O, TEXT("rx"), 10.0)) * 100.f;
		H.Ry = float(CNum(O, TEXT("ry"), 10.0)) * 100.f;
		H.H = float(CNum(O, TEXT("h"), 10.0)) * 100.f;
		H.Yaw = float(CNum(O, TEXT("yaw")));
		H.bCup = CBool(O, TEXT("cup"));
		H.bAlcove = CBool(O, TEXT("alcove"));
		H.bLake = CBool(O, TEXT("lake"));
		H.Site = CName(O, TEXT("site"));
		H.Shrine = CName(O, TEXT("shrine"));
		H.Arena = CName(O, TEXT("arena"));
		H.Lair = CName(O, TEXT("lair"));
		H.Gallery = CName(O, TEXT("gallery"));
		H.Landmark = CName(O, TEXT("landmark"));
		const TSharedPtr<FJsonValue> K = O->TryGetField(TEXT("keystone"));
		if (K.IsValid() && !K->IsNull()) { H.bKeystone = true; H.Keystone = CPoint(K); }
	}
	for (const TSharedPtr<FJsonValue>& V : CArr(J, TEXT("tubes")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FTube& T = D.Tubes.AddDefaulted_GetRef();
		T.Id = CName(O, TEXT("id"));
		T.Kind = CName(O, TEXT("kind"));
		T.Zone = CName(O, TEXT("zone"));
		T.Blend = CName(O, TEXT("blend"));
		T.Door = CName(O, TEXT("door"));
		T.Descent = CName(O, TEXT("descent"));
		T.Gallery = CName(O, TEXT("gallery"));
		T.R = float(CNum(O, TEXT("r"))) * 100.f;
		T.W = float(CNum(O, TEXT("w"))) * 100.f;
		T.H = float(CNum(O, TEXT("h"))) * 100.f;
		T.bBenches = CBool(O, TEXT("benches"));
		for (const TSharedPtr<FJsonValue>& P : CArr(O, TEXT("pts"))) T.Pts.Add(CPoint(P));
		const float Grow = T.Kind == TEXT("passage") ? FMath::Max(T.W, T.H) : T.R * 1.7f;
		for (const FVector& P : T.Pts) T.Bounds += FBox(P - FVector(Grow, Grow, Grow), P + FVector(Grow, Grow, Grow * 1.6f));
	}
	for (const TSharedPtr<FJsonValue>& V : CArr(J, TEXT("shafts")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FShaft& S = D.Shafts.AddDefaulted_GetRef();
		S.Id = CName(O, TEXT("id"));
		S.Kind = CName(O, TEXT("kind"));
		S.Hall = CName(O, TEXT("hall"));
		S.How = CName(O, TEXT("how"));
		S.At = CAt(O, TEXT("z0"));
		S.R = float(CNum(O, TEXT("r"))) * 100.f;
		S.Z0 = float(S.At.Z);
		S.Z1 = float(ToWorld(0.0, 0.0, CNum(O, TEXT("z1"))).Z);
		if (S.Kind == TEXT("vent")) D.Vents.Add(S);
	}
	for (const TSharedPtr<FJsonValue>& V : CArr(J, TEXT("descents")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FDescent& S = D.Descents.AddDefaulted_GetRef();
		S.Id = CName(O, TEXT("id"));
		S.Hall = CName(O, TEXT("hall"));
		S.How = CName(O, TEXT("how"));
		S.Name = CStr(O, TEXT("name"));
		S.At = CAt(O);
		S.R = float(CNum(O, TEXT("r"))) * 100.f;
	}
	for (const TSharedPtr<FJsonValue>& V : CArr(J, TEXT("domes")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FDome& S = D.Domes.AddDefaulted_GetRef();
		S.Id = CName(O, TEXT("id"));
		S.Site = CName(O, TEXT("site"));
		S.At = CAt(O);
		S.R = float(CNum(O, TEXT("r"))) * 100.f;
		S.Depth = float(CNum(O, TEXT("d"))) * 100.f;
		S.Rim = float(ToWorld(0.0, 0.0, CNum(O, TEXT("rim"))).Z);
	}
	for (const TSharedPtr<FJsonValue>& V : CArr(J, TEXT("sites")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FSite& S = D.Sites.AddDefaulted_GetRef();
		S.Id = CName(O, TEXT("id"));
		S.Kind = CName(O, TEXT("kind"));
		S.Hall = CName(O, TEXT("hall"));
		S.Tube = CName(O, TEXT("tube"));
		S.How = CName(O, TEXT("how"));
		S.Name = CStr(O, TEXT("name"));
		S.At = CAt(O);
		S.Yaw = float(CNum(O, TEXT("yaw")));
		S.R = float(CNum(O, TEXT("r"))) * 100.f;
		S.bLandmark = CBool(O, TEXT("landmark"));
		S.bHand = CBool(O, TEXT("hand"));
		S.bCamp = CBool(O, TEXT("camp"));
		S.bCave = CBool(O, TEXT("cave"));
		S.bVault = CBool(O, TEXT("vault"));
		const TSharedPtr<FJsonValue> Cove = O->TryGetField(TEXT("cove"));
		if (Cove.IsValid() && !Cove->IsNull()) S.Cove = CPoint(Cove);
		for (const TSharedPtr<FJsonValue>& P : CArr(O, TEXT("spires")))
		{
			const TSharedPtr<FJsonObject> Q = P->AsObject();
			FSpire& Sp = S.Spires.AddDefaulted_GetRef();
			Sp.At = CAt(Q);
			Sp.H = float(CNum(Q, TEXT("h"), 50.0)) * 100.f;
			Sp.Tilt = float(CNum(Q, TEXT("tilt"), 4.0));
			Sp.Yaw = float(CNum(Q, TEXT("yaw")));
		}
		for (const TSharedPtr<FJsonValue>& P : CArr(O, TEXT("cairns")))
		{
			const TSharedPtr<FJsonObject> Q = P->AsObject();
			FCairn& C = S.Cairns.AddDefaulted_GetRef();
			C.At = CAt(Q);
			C.bTorn = CBool(Q, TEXT("torn"));
			C.Yaw = float(CNum(Q, TEXT("yaw")));
		}
	}
	for (const TSharedPtr<FJsonValue>& V : CArr(J, TEXT("shrines")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FShrine& S = D.Shrines.AddDefaulted_GetRef();
		S.Id = CName(O, TEXT("id"));
		S.Hall = CName(O, TEXT("hall"));
		S.Name = CStr(O, TEXT("name"));
		S.At = CAt(O);
		S.bGreat = CBool(O, TEXT("great"));
		S.bBeam = CBool(O, TEXT("beam"));
		S.Cage = CName(O, TEXT("cage"));
	}
	for (const TSharedPtr<FJsonValue>& V : CArr(J, TEXT("runback")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FRunback& R = D.Runback.AddDefaulted_GetRef();
		R.Boss = CName(O, TEXT("boss"));
		R.Door = CName(O, TEXT("door"));
		R.At = CAt(O);
	}
	for (const TSharedPtr<FJsonValue>& V : CArr(J, TEXT("kit")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FKit& K = D.Kit.AddDefaulted_GetRef();
		K.Id = CName(O, TEXT("id"));
		K.Kind = CName(O, TEXT("kind"));
		K.Hall = CName(O, TEXT("hall"));
		K.Dome = CName(O, TEXT("dome"));
		K.At = CAt(O);
		K.bSurface = CBool(O, TEXT("surface"));
		K.bCave = CBool(O, TEXT("cave"));
		K.Rx = float(CNum(O, TEXT("rx"))) * 100.f;
		K.Ry = float(CNum(O, TEXT("ry"))) * 100.f;
		K.Reveal = float(CNum(O, TEXT("reveal"))) * 100.f;
	}
	for (const TSharedPtr<FJsonValue>& V : CArr(J, TEXT("roads")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FRoad& R = D.Roads.AddDefaulted_GetRef();
		R.Id = CName(O, TEXT("id"));
		R.Name = CStr(O, TEXT("name"));
		for (const TSharedPtr<FJsonValue>& P : CArr(O, TEXT("pts"))) R.Pts.Add(CPoint(P));
	}
	for (const TSharedPtr<FJsonValue>& V : CArr(J, TEXT("chimes")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FChime& C = D.Chimes.AddDefaulted_GetRef();
		C.At = CAt(O);
		C.Yaw = float(CNum(O, TEXT("yaw")));
		C.bSurface = CBool(O, TEXT("surface"));
		C.Tube = CName(O, TEXT("tube"));
	}
	for (const TSharedPtr<FJsonValue>& V : CArr(J, TEXT("chandeliers")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FChandelier& C = D.Chandeliers.AddDefaulted_GetRef();
		C.Id = CName(O, TEXT("id"));
		C.Hall = CName(O, TEXT("hall"));
		C.Top = CAt(O, TEXT("top"));
		C.Floor = float(ToWorld(0.0, 0.0, CNum(O, TEXT("floor"))).Z);
		C.bFallen = CBool(O, TEXT("fallen"));
	}
	for (const TSharedPtr<FJsonValue>& V : CArr(J, TEXT("pools")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FPool& P = D.Pools.AddDefaulted_GetRef();
		P.Hall = CName(O, TEXT("hall"));
		P.Kind = CName(O, TEXT("kind"));
		P.At = CAt(O);
		P.Rx = float(CNum(O, TEXT("rx"), 5.0)) * 100.f;
		P.Ry = float(CNum(O, TEXT("ry"), 5.0)) * 100.f;
		P.Depth = float(CNum(O, TEXT("depth"), 1.3)) * 100.f;
	}
	for (const TSharedPtr<FJsonValue>& V : CArr(J, TEXT("beams")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FBeam& B = D.Beams.AddDefaulted_GetRef();
		B.Hall = CName(O, TEXT("hall"));
		B.A = CPoint(O->TryGetField(TEXT("a")));
		B.B = CPoint(O->TryGetField(TEXT("b")));
		B.Thick = float(CNum(O, TEXT("thick"), 1.4)) * 100.f;
		B.bWalk = CBool(O, TEXT("walk"));
	}
	for (const TSharedPtr<FJsonValue>& V : CArr(J, TEXT("thin")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FThin& T = D.Thin.AddDefaulted_GetRef();
		T.Id = CName(O, TEXT("id"));
		T.Hall = CName(O, TEXT("hall"));
		T.Cup = CName(O, TEXT("cup"));
		T.At = CAt(O);
		T.R = float(CNum(O, TEXT("r"), 4.0)) * 100.f;
		T.Drop = float(CNum(O, TEXT("drop"), 10.0)) * 100.f;
	}
	for (const TSharedPtr<FJsonValue>& V : CArr(J, TEXT("wards")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FWard& W = D.Wards.AddDefaulted_GetRef();
		W.Id = CName(O, TEXT("id"));
		W.Tube = CName(O, TEXT("tube"));
		W.Door = CName(O, TEXT("door"));
		W.Cache = CName(O, TEXT("cache"));
		W.At = CAt(O);
		W.Yaw = float(CNum(O, TEXT("yaw")));
		W.W = float(CNum(O, TEXT("w"), 6.0)) * 100.f;
		W.H = float(CNum(O, TEXT("h"), 7.0)) * 100.f;
		W.Key = CPoint(O->TryGetField(TEXT("key")));
		const FString Pat = CStr(O, TEXT("pattern"));
		W.bA = Pat.Contains(TEXT("A"));
		W.bB = Pat.Contains(TEXT("B"));
		W.bGate = CBool(O, TEXT("gate"));
		W.bShift = CBool(O, TEXT("shift"));
	}
	if (const TSharedPtr<FJsonObject>* R = nullptr; J->TryGetObjectField(TEXT("resources"), R))
	{
		for (const auto& Pair : (*R)->Values)
		{
			TArray<FNode>& L = D.Resources.FindOrAdd(FName(*Pair.Key));
			const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
			if (!Pair.Value->TryGetArray(A)) continue;
			for (const TSharedPtr<FJsonValue>& P : *A)
			{
				const TArray<TSharedPtr<FJsonValue>>* Row = nullptr;
				if (!P->TryGetArray(Row) || Row->Num() < 3) continue;
				FNode& N = L.AddDefaulted_GetRef();
				N.At = ToWorld((*Row)[0]->AsNumber(), (*Row)[1]->AsNumber(), (*Row)[2]->AsNumber());
				if (Row->Num() > 3) N.Of = FName(*(*Row)[3]->AsString());
			}
		}
	}
	for (const TSharedPtr<FJsonValue>& V : CArr(J, TEXT("lore")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		D.Lore.Add({ CName(O, TEXT("item")), CName(O, TEXT("site")) });
	}
	if (const TSharedPtr<FJsonObject>* P = nullptr; J->TryGetObjectField(TEXT("pool"), P))
	{
		D.PoolAt = CAt(*P);
		D.PoolR = FVector2D(CNum(*P, TEXT("rx"), 18.0), CNum(*P, TEXT("ry"), 12.0)) * 100.f;
	}
	if (const TSharedPtr<FJsonObject>* P = nullptr; J->TryGetObjectField(TEXT("vista"), P)) D.VistaAt = ToWorld(CNum(*P, TEXT("x")), CNum(*P, TEXT("y")), CNum(*P, TEXT("top")));
	if (const TSharedPtr<FJsonObject>* G = nullptr; J->TryGetObjectField(TEXT("gorge"), G))
	{
		D.GorgeX0 = float(ToWorld(CNum(*G, TEXT("x0")), 0.0, 0.0).X);
		D.GorgeX1 = float(ToWorld(CNum(*G, TEXT("x1")), 0.0, 0.0).X);
		D.GorgeTop = float(ToWorld(CNum(*G, TEXT("xTop")), 0.0, 0.0).X);
	}
	// Each pool's basin belongs to its hall's air.
	for (int32 p = 0; p < D.Pools.Num(); ++p)
	{
		for (int32 h = 0; h < D.Halls.Num(); ++h) if (D.Halls[h].Id == D.Pools[p].Hall) HallPools().FindOrAdd(h).Add(p);
	}
	D.Index();
	D.bLoaded = D.Halls.Num() > 0;
	UE_LOG(LogWildreach, Log, TEXT("CRYSTAL content: %d halls, %d tubes, %d shafts, %d sites, %d kit, %d wards, %d chimes"), D.Halls.Num(), D.Tubes.Num(), D.Shafts.Num(), D.Sites.Num(), D.Kit.Num(), D.Wards.Num(), D.Chimes.Num());
	return D;
}

void FWRCrystalData::Index()
{
	Bounds.Reset();
	CaveBounds = FBox(ForceInit);
	for (int32 s = 0; s < NumShapes(); ++s)
	{
		FBox B(ForceInit);
		if (s < Halls.Num())
		{
			const FHall& H = Halls[s];
			const float R = FMath::Max(H.Rx, H.Ry) * (H.IsHewn() ? 1.42f : 1.12f) + 200.f;
			B = FBox(H.At - FVector(R, R, 400.f), H.At + FVector(R, R, H.H + 300.f));
		}
		else if (s < Halls.Num() + Tubes.Num())
		{
			B = Tubes[s - Halls.Num()].Bounds;
		}
		else
		{
			const FShaft& S = Shafts[s - Halls.Num() - Tubes.Num()];
			B = FBox(FVector(S.At.X - S.R - 150.f, S.At.Y - S.R - 150.f, S.Z0 - 100.f), FVector(S.At.X + S.R + 150.f, S.At.Y + S.R + 150.f, S.Z1 + 100.f));
		}
		Bounds.Add(B);
		CaveBounds += B;
		const int32 X0 = FMath::FloorToInt(B.Min.X / 6400.f), X1 = FMath::FloorToInt(B.Max.X / 6400.f);
		const int32 Y0 = FMath::FloorToInt(B.Min.Y / 6400.f), Y1 = FMath::FloorToInt(B.Max.Y / 6400.f);
		for (int32 y = Y0; y <= Y1; ++y) for (int32 x = X0; x <= X1; ++x) Grid.FindOrAdd(FIntPoint(x, y)).Add(s);
	}
}

FBox FWRCrystalData::ShapeBounds(int32 Shape) const
{
	return Bounds.IsValidIndex(Shape) ? Bounds[Shape] : FBox(ForceInit);
}

FName FWRCrystalData::ShapeZone(int32 Shape) const
{
	if (Shape < 0) return NAME_None;
	if (Shape < Halls.Num()) return Halls[Shape].Zone;
	if (Shape < Halls.Num() + Tubes.Num()) return Tubes[Shape - Halls.Num()].Zone;
	const FShaft& S = Shafts[Shape - Halls.Num() - Tubes.Num()];
	if (const FHall* H = FindHall(S.Hall)) return H->Zone;
	if (S.Kind == TEXT("vent")) return TEXT("C4");
	if (S.Kind == TEXT("light")) return TEXT("C5");
	return TEXT("C2");
}

void FWRCrystalData::ShapesIn(const FBox& Box, TArray<int32>& Out) const
{
	Out.Reset();
	const int32 X0 = FMath::FloorToInt(Box.Min.X / 6400.f), X1 = FMath::FloorToInt(Box.Max.X / 6400.f);
	const int32 Y0 = FMath::FloorToInt(Box.Min.Y / 6400.f), Y1 = FMath::FloorToInt(Box.Max.Y / 6400.f);
	for (int32 y = Y0; y <= Y1; ++y) for (int32 x = X0; x <= X1; ++x)
	{
		if (const TArray<int32>* L = Grid.Find(FIntPoint(x, y)))
		{
			for (int32 s : *L) if (Bounds[s].Intersect(Box)) Out.AddUnique(s);
		}
	}
}

float FWRCrystalData::ShapeAir(int32 Shape, const FVector& P) const
{
	using namespace WRCrystalTypesImpl;
	if (Shape < Halls.Num())
	{
		const FHall& H = Halls[Shape];
		const FVector2D L2 = H.Local(P);
		float D;
		if (H.IsHewn())
		{
			// A hewn room: straight walls and a flat ceiling, the chisel's few centimetres.
			D = BoxSdf(FVector(L2.X, L2.Y, P.Z - H.At.Z - H.H * 0.5f), FVector(H.Rx, H.Ry, H.H * 0.5f)) + CNoise3(P / 90.f) * 3.f;
			return D;
		}
		const FVector C = H.Centre();
		D = Ellipsoid(FVector(L2.X, L2.Y, P.Z - C.Z), FVector(H.Rx, H.Ry, H.Rz()));
		// A geode is never a clean shell: its walls bulge and hollow by noise (8% of its size, and a finer knobbling).
		const float Size = FMath::Min(H.Rx, H.Ry);
		D += CNoise3(P / 520.f) * 0.075f * Size + CNoise3(P / 140.f + FVector(7.f)) * 28.f;
		// The floor: quartz sand worn level, a few centimetres of ripple.
		const float Floor = H.At.Z + CNoise3(P / 300.f + FVector(3.f)) * 8.f;
		D = FMath::Max(D, Floor - float(P.Z));
		// Its pools' basins (scalding pools, the wallow, a lake): flattened bowls under the floor.
		if (const TArray<int32>* Pl = HallPools().Find(Shape))
		{
			for (int32 p : *Pl)
			{
				const FPool& Q = Pools[p];
				const FVector L(P.X - Q.At.X, P.Y - Q.At.Y, P.Z - (H.At.Z + 20.f));
				const float B = FMath::Max(Ellipsoid(L, FVector(Q.Rx, Q.Ry, Q.Depth + 55.f)), float(P.Z) - (H.At.Z + 60.f));
				D = FMath::Min(D, B);
			}
		}
		return D;
	}
	if (Shape < Halls.Num() + Tubes.Num())
	{
		const FTube& T = Tubes[Shape - Halls.Num()];
		float Floor = 0.f, Tt = 0.f;
		int32 Seg = 0;
		FVector Dir;
		const float Lat = T.Nearest(P, Floor, Seg, Tt, Dir);
		if (T.Kind == TEXT("passage"))
		{
			// Hewn: a flat floor, straight sides, a low barrel vault (80 cm of rise across).
			const float Half = T.W * 0.5f;
			const float Ceil = Floor + T.H - 80.f * FMath::Square(FMath::Clamp(Lat / Half, 0.f, 1.f));
			float D = FMath::Max3(Lat - Half, Floor - float(P.Z), float(P.Z) - Ceil);
			// The ends: a passage stops square at its end points (inside the rooms it joins).
			return D + CNoise3(P / 80.f) * 2.f;
		}
		// A lava tube: round, its axis 0.55 r over the floor; the floor flat, with benches 1.2 m up at the sides where
		// old lava stood (the Organ), glazed walls a little uneven.
		const float AxisZ = Floor + T.R * 0.55f;
		const float Dist = FMath::Sqrt(Lat * Lat + FMath::Square(float(P.Z) - AxisZ));
		float D = Dist - T.R * (1.f + 0.06f * CNoise3(P / 450.f)) - CNoise3(P / 120.f + FVector(11.f)) * 14.f;
		float Fl = Floor + CNoise3(P / 250.f) * 6.f;
		if (T.bBenches) Fl += 120.f * CSmooth(T.R * 0.56f, T.R * 0.7f, Lat);
		return FMath::Max(D, Fl - float(P.Z));
	}
	const FShaft& S = Shafts[Shape - Halls.Num() - Tubes.Num()];
	const float Dist = FVector2D::Distance(FVector2D(P), FVector2D(S.At));
	const float Wobble = S.Kind == TEXT("light") ? 0.f : CNoise3(P / 260.f) * 22.f;
	return FMath::Max3(Dist - S.R - Wobble, S.Z0 - float(P.Z), float(P.Z) - S.Z1);
}

float FWRCrystalData::Air(const FVector& P, int32* OutNearest) const
{
	float Best = 1.0e6f;
	int32 BestShape = INDEX_NONE;
	if (const TArray<int32>* L = Grid.Find(FIntPoint(FMath::FloorToInt(P.X / 6400.f), FMath::FloorToInt(P.Y / 6400.f))))
	{
		for (int32 s : *L)
		{
			if (!Bounds[s].IsInsideOrOn(P)) continue;
			const float D = ShapeAir(s, P);
			if (D < Best) { Best = D; BestShape = s; }
		}
	}
	if (OutNearest) *OutNearest = BestShape;
	return Best;
}

const FWRCrystalData::FHall* FWRCrystalData::FindHall(FName Id) const
{
	return Halls.FindByPredicate([Id](const FHall& H) { return H.Id == Id; });
}

const FWRCrystalData::FSite* FWRCrystalData::FindSite(FName Id) const
{
	return Sites.FindByPredicate([Id](const FSite& S) { return S.Id == Id; });
}

const FWRCrystalData::FTube* FWRCrystalData::FindTube(FName Id) const
{
	return Tubes.FindByPredicate([Id](const FTube& T) { return T.Id == Id; });
}

// ================================================================================================ WRCrystal

FName WRCrystal::Biome() { return WRCrystalTypesImpl::CBiome; }

bool WRCrystal::Live() { return WRBiome::IsLive(WRCrystalTypesImpl::CBiome); }

bool WRCrystal::InCave(const FVector& Pos)
{
	if (!Live()) return false;
	const FWRCrystalData& D = FWRCrystalData::Get();
	if (!D.bLoaded || !D.CaveBounds.IsInsideOrOn(Pos)) return false;
	if (D.Air(Pos) > 60.f) return false;
	// Under the ground (a descent's open mouth is still the sky's).
	if (const FWRLandDef* L = FWRCrystalData::Land())
	{
		const FVector2D M = L->ToMap(Pos);
		if (Pos.Z > L->ToWorld(M, L->Height(M)).Z - 150.f && !L->HfHiddenAt(M)) return false;
	}
	return true;
}

const FWRCrystalData::FHall* WRCrystal::HallAt(const FVector& Pos)
{
	if (!Live()) return nullptr;
	const FWRCrystalData& D = FWRCrystalData::Get();
	if (!D.bLoaded || !D.CaveBounds.IsInsideOrOn(Pos)) return nullptr;
	const FWRCrystalData::FHall* Best = nullptr;
	float BestD = 60.f;
	for (int32 h = 0; h < D.Halls.Num(); ++h)
	{
		if (!D.ShapeBounds(h).IsInsideOrOn(Pos)) continue;
		const float A = D.ShapeAir(h, Pos);
		if (A < BestD) { BestD = A; Best = &D.Halls[h]; }
	}
	return Best;
}

bool WRCrystal::InVault(const FVector& Pos)
{
	if (!Live()) return false;
	const FWRCrystalData& D = FWRCrystalData::Get();
	if (!D.bLoaded || !D.CaveBounds.IsInsideOrOn(Pos)) return false;
	int32 S = INDEX_NONE;
	return D.Air(Pos, &S) < 60.f && D.ShapeZone(S) == TEXT("V");
}

bool WRCrystal::Contains(const FVector& Pos)
{
	if (!Live()) return false;
	return WRLand::BiomeAt(Pos) == Biome() || InCave(Pos);
}

FName WRCrystal::ZoneAt(const FVector& Pos)
{
	if (!Live()) return NAME_None;
	if (InCave(Pos))
	{
		int32 S = INDEX_NONE;
		FWRCrystalData::Get().Air(Pos, &S);
		return FWRCrystalData::Get().ShapeZone(S);
	}
	if (WRLand::BiomeAt(Pos) != Biome()) return NAME_None;
	const FName Z = WRLand::ZoneAt(Pos);
	return Z == TEXT("severance") ? FName(TEXT("severance")) : FName(TEXT("C1"));
}
