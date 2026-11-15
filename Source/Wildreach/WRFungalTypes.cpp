// The Fungal caverns' content (cooks/build-biome-fungal.md 2.0-2.14): Data/fungal.json parsed once, the caverns' air
// shapes as one signed distance, and the calls the Fungal caverns' parts share. See WRFungalTypes.h.

#include "WRFungalTypes.h"
#include "Wildreach.h"
#include "WRLand.h"
#include "WRFarm.h"
#include "WRTuning.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace WRFungalTypesImpl
{
	const FName FgBiome(TEXT("fungal"));

	double FgNum(const TSharedPtr<FJsonObject>& O, const TCHAR* K, double Def = 0.0)
	{
		double V = Def;
		if (O.IsValid()) O->TryGetNumberField(K, V);
		return V;
	}
	bool FgBool(const TSharedPtr<FJsonObject>& O, const TCHAR* K)
	{
		bool B = false;
		if (O.IsValid()) O->TryGetBoolField(K, B);
		return B;
	}
	FName FgName(const TSharedPtr<FJsonObject>& O, const TCHAR* K)
	{
		FString S;
		if (O.IsValid() && O->TryGetStringField(K, S) && !S.IsEmpty()) return FName(*S);
		return NAME_None;
	}
	FString FgStr(const TSharedPtr<FJsonObject>& O, const TCHAR* K)
	{
		FString S;
		if (O.IsValid()) O->TryGetStringField(K, S);
		return S;
	}
	FVector FgAt(const TSharedPtr<FJsonObject>& O, const TCHAR* ZKey = TEXT("z"))
	{
		return FWRFungalData::ToWorld(FgNum(O, TEXT("x")), FgNum(O, TEXT("y")), FgNum(O, ZKey));
	}
	TArray<TSharedPtr<FJsonValue>> FgArr(const TSharedPtr<FJsonObject>& O, const TCHAR* K)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (O.IsValid() && O->TryGetArrayField(K, A)) return *A;
		return {};
	}
	/** [x, y, z] (map metres) -> world cm. */
	FVector FgPoint(const TSharedPtr<FJsonValue>& V)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (!V.IsValid() || !V->TryGetArray(A) || A->Num() < 2) return FVector::ZeroVector;
		return FWRFungalData::ToWorld((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), A->Num() > 2 ? (*A)[2]->AsNumber() : 0.0);
	}
	TMap<FName, float> FgTable(const TSharedPtr<FJsonObject>& O)
	{
		TMap<FName, float> M;
		if (!O.IsValid()) return M;
		for (const auto& KV : O->Values) M.Add(FName(*KV.Key), float(KV.Value->AsNumber()));
		return M;
	}
	float FgSmooth(float E0, float E1, float X)
	{
		const float T = FMath::Clamp((X - E0) / (E1 - E0), 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	}
	float FgHash(int32 X, int32 Y, int32 Z)
	{
		uint32 H = uint32(X) * 0x8da6b343u ^ uint32(Y) * 0xd8163841u ^ uint32(Z) * 0xcb1ab31fu ^ 0x8821u;
		H ^= H >> 13; H *= 0x5bd1e995u; H ^= H >> 15;
		return float(H & 0xffff) / 32767.5f - 1.f;
	}
	/** 3D value noise in [-1, 1] (P in noise cells). */
	float FgNoise3(const FVector& P)
	{
		const int32 X0 = FMath::FloorToInt(P.X), Y0 = FMath::FloorToInt(P.Y), Z0 = FMath::FloorToInt(P.Z);
		const float FX = P.X - X0, FY = P.Y - Y0, FZ = P.Z - Z0;
		const float SX = FX * FX * (3.f - 2.f * FX), SY = FY * FY * (3.f - 2.f * FY), SZ = FZ * FZ * (3.f - 2.f * FZ);
		const auto L2 = [&](int32 Z) {
			return FMath::Lerp(FMath::Lerp(FgHash(X0, Y0, Z), FgHash(X0 + 1, Y0, Z), SX), FMath::Lerp(FgHash(X0, Y0 + 1, Z), FgHash(X0 + 1, Y0 + 1, Z), SX), SY);
		};
		return FMath::Lerp(L2(Z0), L2(Z0 + 1), SZ);
	}
	float FgEllipsoid(const FVector& L, const FVector& R)
	{
		const FVector Q(L.X / R.X, L.Y / R.Y, L.Z / R.Z);
		const FVector Q2(L.X / (R.X * R.X), L.Y / (R.Y * R.Y), L.Z / (R.Z * R.Z));
		const float K0 = Q.Size(), K1 = Q2.Size();
		if (K1 < 1.e-6f) return -FMath::Min3(R.X, R.Y, R.Z);
		return K0 * (K0 - 1.f) / K1;
	}
	const FWRLandDef* FgLand()
	{
		static const FWRLandDef* L = nullptr;
		static bool bTried = false;
		if (!bTried)
		{
			bTried = true;
			for (const FWRLandDef& D : WRLand::All()) if (D.Biome == FgBiome) L = &D;
		}
		return L;
	}
}

const FWRLandDef* FWRFungalData::Land() { return WRFungalTypesImpl::FgLand(); }

FVector FWRFungalData::ToWorld(double X, double Y, double Z)
{
	if (const FWRLandDef* L = Land()) return L->ToWorld(FVector2D(X, Y), float(Z));
	// No land file: the real-metres frame (game cm = map metres x 100).
	return FVector(X * 100.0, Y * 100.0, Z * 100.0);
}

FVector2D FWRFungalData::FHall::Local(const FVector& P) const
{
	const FVector2D D(P.X - At.X, P.Y - At.Y);
	const float C = FMath::Cos(FMath::DegreesToRadians(Yaw)), S = FMath::Sin(FMath::DegreesToRadians(Yaw));
	return FVector2D(D.X * C + D.Y * S, -D.X * S + D.Y * C);
}

bool FWRFungalData::FHall::Contains(const FVector& P, float Grow) const
{
	if (P.Z < At.Z - 300.f - Grow || P.Z > (IsDoline() ? Top : At.Z + H) + Grow) return false;
	const FVector2D L = Local(P);
	return FMath::Square(L.X / (Rx + Grow)) + FMath::Square(L.Y / (Ry + Grow)) <= 1.f;
}

float FWRFungalData::FTube::Nearest(const FVector& P, float& OutFloor, int32& OutSeg, float& OutT, FVector& OutDir) const
{
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
			// Steep tubes (the moat's wet stair, the spiral to the Webs) measure in 3D so a coil's turns stay apart.
			const FVector A(Pts[i]), Bp(Pts[i + 1]), D = Bp - A;
			const float L2 = FVector2D(D).SizeSquared();
			const float T = L2 > 1.f ? FMath::Clamp(FVector2D::DotProduct(P2 - FVector2D(A), FVector2D(D)) / L2, 0.f, 1.f) : 0.f;
			const float Dh = FVector2D::Distance(P2, FVector2D(A + D * T));
			const float Fz = FMath::Lerp(A.Z, Bp.Z, T);
			const float Dv = FMath::Max(0.f, FMath::Abs(float(P.Z) - (Fz + Roof() * 0.5f)) - Roof());
			const float Dist = FMath::Sqrt(Dh * Dh + Dv * Dv);
			if (Dist < Best) { Best = Dist; OutSeg = i; OutT = T; }
		}
	}
	if (N >= 2)
	{
		OutFloor = FMath::Lerp(Pts[OutSeg].Z, Pts[OutSeg + 1].Z, OutT);
		OutDir = (Pts[OutSeg + 1] - Pts[OutSeg]).GetSafeNormal();
		const FVector2D A(Pts[OutSeg]), D = FVector2D(Pts[OutSeg + 1]) - A;
		Best = FVector2D::Distance(FVector2D(P), A + D * OutT);
	}
	else
	{
		OutFloor = N ? Pts[0].Z : 0.f;
		OutDir = FVector::ForwardVector;
	}
	return Best;
}

const FWRFungalData& FWRFungalData::Get()
{
	using namespace WRFungalTypesImpl;
	static FWRFungalData D;
	static bool bTried = false;
	if (bTried) return D;
	bTried = true;
	const TSharedPtr<FJsonObject> J = FWRTuning::LoadDataFile(TEXT("fungal.json"));
	if (!J.IsValid())
	{
		UE_LOG(LogWildreach, Error, TEXT("FUNGAL: Data/fungal.json is missing (node Tools/terrain/fungal.mjs)"));
		return D;
	}
	for (const TSharedPtr<FJsonValue>& V : FgArr(J, TEXT("halls")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FHall& H = D.Halls.AddDefaulted_GetRef();
		H.Id = FgName(O, TEXT("id"));
		H.Kind = FgName(O, TEXT("kind"));
		H.Zone = FgName(O, TEXT("zone"));
		H.Name = FgStr(O, TEXT("name"));
		H.At = FgAt(O, TEXT("floor"));
		H.Rx = float(FgNum(O, TEXT("rx"), 10.0)) * 100.f;
		H.Ry = float(FgNum(O, TEXT("ry"), 10.0)) * 100.f;
		H.H = float(FgNum(O, TEXT("h"), 10.0)) * 100.f;
		H.Yaw = float(FgNum(O, TEXT("yaw")));
		H.Top = float(FgNum(O, TEXT("top"), FgNum(O, TEXT("floor")) + FgNum(O, TEXT("h"), 10.0))) * 100.f;
		H.Walk = O->HasField(TEXT("walk")) ? float(FgNum(O, TEXT("walk"))) * 100.f : 0.f;
		H.Lake = float(FgNum(O, TEXT("lake"))) * 100.f;
		H.bAlcove = FgBool(O, TEXT("alcove"));
		H.bRoom = FgBool(O, TEXT("room"));
		H.bVault = FgBool(O, TEXT("vault"));
		H.bFoul = FgBool(O, TEXT("foul"));
		H.Site = FgName(O, TEXT("site"));
		H.Shrine = FgName(O, TEXT("shrine"));
		H.Arena = FgName(O, TEXT("arena"));
		H.Lair = FgName(O, TEXT("lair"));
		H.Landmark = FgName(O, TEXT("landmark"));
		H.Gallery = FgName(O, TEXT("gallery"));
		H.Secret = FgName(O, TEXT("secret"));
		H.Light = FgName(O, TEXT("light"));
		H.Tube = FgName(O, TEXT("tube"));
		if (H.IsDoline()) H.Ry = H.Rx;
	}
	for (const TSharedPtr<FJsonValue>& V : FgArr(J, TEXT("tubes")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FTube& T = D.Tubes.AddDefaulted_GetRef();
		T.Id = FgName(O, TEXT("id"));
		T.Kind = FgName(O, TEXT("kind"));
		T.Zone = FgName(O, TEXT("zone"));
		T.Blend = FgName(O, TEXT("blend"));
		T.Door = FgName(O, TEXT("door"));
		T.Descent = FgName(O, TEXT("descent"));
		T.Secret = FgName(O, TEXT("secret"));
		T.R = float(FgNum(O, TEXT("r"), 5.0)) * 100.f;
		T.W = float(FgNum(O, TEXT("w"), 6.0)) * 100.f;
		T.H = float(FgNum(O, TEXT("h"), 7.0)) * 100.f;
		T.Flooded = float(FgNum(O, TEXT("flooded"))) * 100.f;
		T.bRiver = FgBool(O, TEXT("river"));
		T.bSealed = FgBool(O, TEXT("sealed"));
		T.bMole = FgBool(O, TEXT("mole"));
		T.bStreet = FgBool(O, TEXT("street"));
		T.bLandings = FgBool(O, TEXT("landings"));
		T.bFlue = FgBool(O, TEXT("flue"));
		for (const TSharedPtr<FJsonValue>& P : FgArr(O, TEXT("pts"))) T.Pts.Add(FgPoint(P));
		const float Grow = T.Half() + 400.f;
		for (const FVector& P : T.Pts) T.Bounds += FBox(P - FVector(Grow, Grow, 300.f), P + FVector(Grow, Grow, T.Roof() + 300.f));
	}
	for (const TSharedPtr<FJsonValue>& V : FgArr(J, TEXT("shafts")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FShaft& S = D.Shafts.AddDefaulted_GetRef();
		S.Id = FgName(O, TEXT("id"));
		S.Kind = FgName(O, TEXT("kind"));
		S.Hall = FgName(O, TEXT("hall"));
		S.How = FgName(O, TEXT("how"));
		S.At = ToWorld(FgNum(O, TEXT("x")), FgNum(O, TEXT("y")), FgNum(O, TEXT("z0")));
		S.R = float(FgNum(O, TEXT("r"), 5.0)) * 100.f;
		S.Z0 = S.At.Z;
		S.Z1 = float(ToWorld(0.0, 0.0, FgNum(O, TEXT("z1"))).Z);
		S.bSealed = FgBool(O, TEXT("sealed"));
	}
	for (const TSharedPtr<FJsonValue>& V : FgArr(J, TEXT("descents")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		D.Descents.Add({ FgName(O, TEXT("id")), FgName(O, TEXT("hall")), FgName(O, TEXT("how")), FgStr(O, TEXT("name")), FgAt(O), float(FgNum(O, TEXT("r"), 10.0)) * 100.f });
	}
	for (const TSharedPtr<FJsonValue>& V : FgArr(J, TEXT("sites")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FSite& S = D.Sites.AddDefaulted_GetRef();
		S.Id = FgName(O, TEXT("id"));
		S.Kind = FgName(O, TEXT("kind"));
		S.Hall = FgName(O, TEXT("hall"));
		S.How = FgName(O, TEXT("how"));
		S.Shrine = FgName(O, TEXT("shrine"));
		S.Name = FgStr(O, TEXT("name"));
		S.At = FgAt(O);
		S.Yaw = float(FgNum(O, TEXT("yaw")));
		S.R = float(FgNum(O, TEXT("r"))) * 100.f;
		S.bLandmark = FgBool(O, TEXT("landmark"));
		S.bHand = FgBool(O, TEXT("hand"));
		S.bCamp = FgBool(O, TEXT("camp"));
		S.bCave = FgBool(O, TEXT("cave"));
		S.bGrotto = FgBool(O, TEXT("grotto"));
		if (O->HasField(TEXT("cove"))) S.Cove = FgPoint(O->TryGetField(TEXT("cove")));
		const TArray<TSharedPtr<FJsonValue>> Bowl = FgArr(O, TEXT("bowl"));
		if (Bowl.Num() >= 3)
		{
			const FVector B = ToWorld(Bowl[0]->AsNumber(), Bowl[1]->AsNumber(), 0.0);
			S.Bowl = FVector(B.X, B.Y, float(Bowl[2]->AsNumber()) * 100.f);
		}
	}
	for (const TSharedPtr<FJsonValue>& V : FgArr(J, TEXT("shrines")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FShrine& S = D.Shrines.AddDefaulted_GetRef();
		S.Id = FgName(O, TEXT("id"));
		S.Hall = FgName(O, TEXT("hall"));
		S.Name = FgStr(O, TEXT("name"));
		S.At = FgAt(O);
		S.bGreat = FgBool(O, TEXT("great"));
		const TSharedPtr<FJsonObject>* G = nullptr;
		if (O->TryGetObjectField(TEXT("guards"), G)) for (const auto& KV : FgTable(*G)) S.Guards.Add(KV.Key, int32(KV.Value));
	}
	for (const TSharedPtr<FJsonValue>& V : FgArr(J, TEXT("runback")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		D.Runback.Add({ FgName(O, TEXT("boss")), FgAt(O) });
	}
	for (const TSharedPtr<FJsonValue>& V : FgArr(J, TEXT("kit")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FKit& K = D.Kit.AddDefaulted_GetRef();
		K.Id = FgName(O, TEXT("id"));
		K.Kind = FgName(O, TEXT("kind"));
		K.Hall = FgName(O, TEXT("hall"));
		K.Tube = FgName(O, TEXT("tube"));
		K.Over = FgName(O, TEXT("over"));
		K.At = FgAt(O);
		K.V = int32(FgNum(O, TEXT("v")));
		K.bSurface = FgBool(O, TEXT("surface"));
		K.bCave = FgBool(O, TEXT("cave"));
	}
	for (const TSharedPtr<FJsonValue>& V : FgArr(J, TEXT("routes")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FRoute& R = D.Routes.AddDefaulted_GetRef();
		R.Id = FgName(O, TEXT("id"));
		R.Name = FgStr(O, TEXT("name"));
		for (const TSharedPtr<FJsonValue>& P : FgArr(O, TEXT("pts"))) { const FVector W = FgPoint(P); R.Pts.Add(FVector2D(W)); }
	}
	if (const TSharedPtr<FJsonObject>* Res = nullptr; J->TryGetObjectField(TEXT("resources"), Res))
	{
		for (const auto& KV : (*Res)->Values)
		{
			TArray<FNode>& L = D.Resources.FindOrAdd(FName(*KV.Key));
			for (const TSharedPtr<FJsonValue>& P : KV.Value->AsArray())
			{
				const TArray<TSharedPtr<FJsonValue>>& A = P->AsArray();
				if (A.Num() < 3) continue;
				L.Add({ ToWorld(A[0]->AsNumber(), A[1]->AsNumber(), A[2]->AsNumber()), A.Num() > 3 ? FName(*A[3]->AsString()) : NAME_None });
			}
		}
	}
	for (const TSharedPtr<FJsonValue>& V : FgArr(J, TEXT("clouds")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		D.Clouds.Add({ FgAt(O), float(FgNum(O, TEXT("r"), 8.0)) * 100.f, FgName(O, TEXT("kind")), FgName(O, TEXT("of")), FgBool(O, TEXT("breathe")) });
	}
	for (const TSharedPtr<FJsonValue>& V : FgArr(J, TEXT("lines")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FLineSet& L = D.Lines.AddDefaulted_GetRef();
		L.Hall = FgName(O, TEXT("hall"));
		L.Web = FgName(O, TEXT("web"));
		L.At = ToWorld(FgNum(O, TEXT("x")), FgNum(O, TEXT("y")), FgNum(O, TEXT("floor")));
		L.Rx = float(FgNum(O, TEXT("rx"), 10.0)) * 100.f;
		L.Ry = float(FgNum(O, TEXT("ry"), 10.0)) * 100.f;
		L.Yaw = float(FgNum(O, TEXT("yaw")));
		L.Floor = L.At.Z;
		L.Roof = float(ToWorld(0.0, 0.0, FgNum(O, TEXT("roof"))).Z);
		L.Density = float(FgNum(O, TEXT("density"), 0.1));
		L.Low = float(FgNum(O, TEXT("low"), 1.6)) * 100.f;
		L.Stars = int32(FgNum(O, TEXT("stars")));
	}
	for (const TSharedPtr<FJsonValue>& V : FgArr(J, TEXT("springcaps")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		D.Springcaps.Add({ FgAt(O), FgName(O, TEXT("of")), FgBool(O, TEXT("ledge")) });
	}
	for (const TSharedPtr<FJsonValue>& V : FgArr(J, TEXT("capfalls")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		D.CapFalls.Add({ FgAt(O), float(ToWorld(0.0, 0.0, FgNum(O, TEXT("roof"))).Z), FgName(O, TEXT("hall")) });
	}
	for (const TSharedPtr<FJsonValue>& V : FgArr(J, TEXT("foul")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		D.Foul.Add({ FgAt(O), float(FgNum(O, TEXT("rx"), 10.0)) * 100.f, float(FgNum(O, TEXT("ry"), 10.0)) * 100.f, float(FgNum(O, TEXT("h"), 9.0)) * 100.f, float(FgNum(O, TEXT("yaw"))), FgName(O, TEXT("hall")), FgName(O, TEXT("tube")) });
	}
	for (const TSharedPtr<FJsonValue>& V : FgArr(J, TEXT("vents"))) D.Vents.Add(FgAt(V->AsObject()));
	for (const TSharedPtr<FJsonValue>& V : FgArr(J, TEXT("water")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FWater& W = D.Water.AddDefaulted_GetRef();
		W.Hall = FgName(O, TEXT("hall"));
		W.Tube = FgName(O, TEXT("tube"));
		W.At = ToWorld(FgNum(O, TEXT("x")), FgNum(O, TEXT("y")), FgNum(O, TEXT("top")));
		W.Rx = float(FgNum(O, TEXT("rx"), 5.0)) * 100.f;
		W.Ry = float(FgNum(O, TEXT("ry"), 5.0)) * 100.f;
		W.Yaw = float(FgNum(O, TEXT("yaw")));
		W.Top = W.At.Z;
		W.Bottom = float(ToWorld(0.0, 0.0, FgNum(O, TEXT("bottom"))).Z);
		W.bRiver = FgBool(O, TEXT("river"));
	}
	for (const TSharedPtr<FJsonValue>& V : FgArr(J, TEXT("dens")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FDen& Dn = D.Dens.AddDefaulted_GetRef();
		Dn.At = FgAt(O);
		Dn.Zone = FgName(O, TEXT("zone"));
		Dn.Hall = FgName(O, TEXT("hall"));
		Dn.Spread = float(FgNum(O, TEXT("spread"), 10.0)) * 100.f;
		Dn.bCave = FgBool(O, TEXT("cave"));
		Dn.Max = int32(FgNum(O, TEXT("max")));
		const TSharedPtr<FJsonObject>* T = nullptr;
		if (O->TryGetObjectField(TEXT("table"), T))
		{
			const TSharedPtr<FJsonObject>* Dy = nullptr, *Nt = nullptr;
			if ((*T)->TryGetObjectField(TEXT("day"), Dy)) Dn.Day = FgTable(*Dy);
			if ((*T)->TryGetObjectField(TEXT("night"), Nt)) Dn.Night = FgTable(*Nt);
		}
	}
	if (const TSharedPtr<FJsonObject>* T = nullptr; J->TryGetObjectField(TEXT("denTables"), T))
	{
		for (const auto& KV : (*T)->Values)
		{
			const TSharedPtr<FJsonObject> Z = KV.Value->AsObject();
			const TSharedPtr<FJsonObject>* Dy = nullptr, *Nt = nullptr;
			TPair<TMap<FName, float>, TMap<FName, float>>& P = D.DenTables.FindOrAdd(FName(*KV.Key));
			if (Z->TryGetObjectField(TEXT("day"), Dy)) P.Key = FgTable(*Dy);
			if (Z->TryGetObjectField(TEXT("night"), Nt)) P.Value = FgTable(*Nt);
		}
	}
	for (const TSharedPtr<FJsonValue>& V : FgArr(J, TEXT("lore")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		D.Lore.Add({ FgName(O, TEXT("item")), FgName(O, TEXT("site")) });
	}
	for (const TSharedPtr<FJsonValue>& V : FgArr(J, TEXT("secrets")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FSecret& S = D.Secrets.AddDefaulted_GetRef();
		S.Id = FgName(O, TEXT("id"));
		S.Site = FgName(O, TEXT("site"));
		S.Hall = FgName(O, TEXT("hall"));
		S.Need = FgName(O, TEXT("need"));
		S.Name = FgStr(O, TEXT("name"));
		if (O->HasField(TEXT("x"))) S.At = FgAt(O);
	}
	if (const TSharedPtr<FJsonObject>* G = nullptr; J->TryGetObjectField(TEXT("great"), G)) { D.GreatAt = FgAt(*G, TEXT("floor")); D.GreatR = float(FgNum(*G, TEXT("r"), 130.0)) * 100.f; }
	for (const TSharedPtr<FJsonValue>& V : FgArr(J, TEXT("sieves")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		const FVector P = FgAt(O);
		D.Sieves.Add(P);
	}
	if (const TSharedPtr<FJsonObject>* R = nullptr; J->TryGetObjectField(TEXT("ringOfCaps"), R)) { D.RingAt = ToWorld(FgNum(*R, TEXT("x")), FgNum(*R, TEXT("y")), 0.0); D.RingR = float(FgNum(*R, TEXT("r"), 100.0)) * 100.f; }
	if (const TSharedPtr<FJsonObject>* R = nullptr; J->TryGetObjectField(TEXT("breathVents"), R)) { D.BreathAt = ToWorld(FgNum(*R, TEXT("x")), FgNum(*R, TEXT("y")), 0.0); D.BreathR = float(FgNum(*R, TEXT("r"), 60.0)) * 100.f; }
	if (const TSharedPtr<FJsonObject>* R = nullptr; J->TryGetObjectField(TEXT("cloudMouth"), R)) { const FVector P = ToWorld(FgNum(*R, TEXT("x")), FgNum(*R, TEXT("y")), 0.0); D.CloudMouth = FVector(P.X, P.Y, float(FgNum(*R, TEXT("r"), 20.0)) * 100.f); }
	if (const TSharedPtr<FJsonObject>* F = nullptr; J->TryGetObjectField(TEXT("flue"), F))
	{
		D.FlueAt = FgAt(*F, TEXT("floor"));
		D.FlueR = float(FgNum(*F, TEXT("r"), 133.0)) * 100.f;
		D.FlueMouthYaw = float(FgNum(*F, TEXT("mouthYaw"), 180.0));
		D.FlueMouthHalf = float(FgNum(*F, TEXT("mouthHalf"), 6.5));
		D.FlueMouthTop = float(FgNum(*F, TEXT("mouthTop"), 16.0)) * 100.f;
	}
	if (const TSharedPtr<FJsonObject>* A = nullptr; J->TryGetObjectField(TEXT("arenas"), A))
	{
		const auto Angled = [](const TArray<TSharedPtr<FJsonValue>>& L, TArray<FAngled>& Out, const TCHAR* AngleKey)
		{
			for (const TSharedPtr<FJsonValue>& V : L)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				FAngled& G = Out.AddDefaulted_GetRef();
				G.I = int32(FgNum(O, TEXT("i")));
				G.Angle = float(FgNum(O, AngleKey));
				G.At = FgAt(O);
				G.R = float(FgNum(O, TEXT("r"))) * 100.f;
				G.bFull = FgBool(O, TEXT("full"));
				G.Id = FgName(O, TEXT("id"));
			}
		};
		const TSharedPtr<FJsonObject>* S = nullptr;
		if ((*A)->TryGetObjectField(TEXT("sporemother"), S)) Angled(FgArr(*S, TEXT("sacs")), D.Sacs, TEXT("a"));
		if ((*A)->TryGetObjectField(TEXT("rot_king"), S)) Angled(FgArr(*S, TEXT("vats")), D.Vats, TEXT("a"));
		if ((*A)->TryGetObjectField(TEXT("weaver"), S)) for (const TSharedPtr<FJsonValue>& P : FgArr(*S, TEXT("route"))) D.WeaverRoute.Add(FgPoint(P));
		if ((*A)->TryGetObjectField(TEXT("mycelorn"), S))
		{
			const FHall* HC = nullptr;
			for (const FHall& H : D.Halls) if (H.Id == TEXT("HC")) HC = &H;
			const float Floor = HC ? HC->At.Z : 0.f;
			Angled(FgArr(*S, TEXT("terraces")), D.Terraces, TEXT("a"));
			Angled(FgArr(*S, TEXT("gates")), D.Gates, TEXT("a"));
			Angled(FgArr(*S, TEXT("pods")), D.Pods, TEXT("a"));
			for (FAngled& P : D.Pods) P.At.Z = Floor;
			const TSharedPtr<FJsonObject>* P = nullptr;
			if ((*S)->TryGetObjectField(TEXT("pool"), P)) { D.PoolAt = ToWorld(FgNum(*P, TEXT("x")), FgNum(*P, TEXT("y")), 0.0); D.PoolAt.Z = Floor; D.PoolR = float(FgNum(*P, TEXT("r"), 20.0)) * 100.f; }
			if ((*S)->TryGetObjectField(TEXT("altar"), P)) { D.AltarAt = ToWorld(FgNum(*P, TEXT("x")), FgNum(*P, TEXT("y")), 0.0); D.AltarAt.Z = Floor; }
			const TSharedPtr<FJsonObject>* Vn = nullptr;
			if ((*S)->TryGetObjectField(TEXT("veins"), Vn))
			{
				for (const TSharedPtr<FJsonValue>& Nd : FgArr(*Vn, TEXT("nodes"))) { FVector W = FgPoint(Nd); W.Z = Floor; D.VeinNodes.Add(W); }
				for (const TSharedPtr<FJsonValue>& E : FgArr(*Vn, TEXT("edges"))) { const TArray<TSharedPtr<FJsonValue>>& Ab = E->AsArray(); if (Ab.Num() == 2) D.VeinEdges.Add(FIntPoint(int32(Ab[0]->AsNumber()), int32(Ab[1]->AsNumber()))); }
			}
		}
	}
	D.Index();
	D.bLoaded = D.Halls.Num() > 0;
	UE_LOG(LogWildreach, Log, TEXT("FUNGAL content: %d halls, %d tubes, %d shafts, %d sites, %d kit, %d clouds, %d dens"), D.Halls.Num(), D.Tubes.Num(), D.Shafts.Num(), D.Sites.Num(), D.Kit.Num(), D.Clouds.Num(), D.Dens.Num());
	return D;
}

void FWRFungalData::Index()
{
	Bounds.Reset();
	CaveBounds = FBox(ForceInit);
	for (int32 s = 0; s < NumShapes(); ++s)
	{
		FBox B(ForceInit);
		if (s < Halls.Num())
		{
			const FHall& H = Halls[s];
			const float R = FMath::Max(H.Rx, H.Ry) * 1.15f + 300.f;
			B = FBox(H.At - FVector(R, R, 400.f), H.At + FVector(R, R, (H.IsDoline() ? H.Top - H.At.Z : H.H) + 300.f));
		}
		else if (s < Halls.Num() + Tubes.Num())
		{
			B = Tubes[s - Halls.Num()].Bounds;
		}
		else
		{
			const FShaft& S = Shafts[s - Halls.Num() - Tubes.Num()];
			B = FBox(FVector(S.At.X - S.R - 200.f, S.At.Y - S.R - 200.f, S.Z0 - 100.f), FVector(S.At.X + S.R + 200.f, S.At.Y + S.R + 200.f, S.Z1 + 100.f));
		}
		Bounds.Add(B);
		CaveBounds += B;
		const int32 X0 = FMath::FloorToInt(B.Min.X / 6400.f), X1 = FMath::FloorToInt(B.Max.X / 6400.f);
		const int32 Y0 = FMath::FloorToInt(B.Min.Y / 6400.f), Y1 = FMath::FloorToInt(B.Max.Y / 6400.f);
		for (int32 y = Y0; y <= Y1; ++y) for (int32 x = X0; x <= X1; ++x) Grid.FindOrAdd(FIntPoint(x, y)).Add(s);
	}
}

FBox FWRFungalData::ShapeBounds(int32 Shape) const
{
	return Bounds.IsValidIndex(Shape) ? Bounds[Shape] : FBox(ForceInit);
}

FName FWRFungalData::ShapeZone(int32 Shape) const
{
	if (Shape < 0) return NAME_None;
	if (Shape < Halls.Num()) return Halls[Shape].Zone;
	if (Shape < Halls.Num() + Tubes.Num()) return Tubes[Shape - Halls.Num()].Zone;
	const FShaft& S = Shafts[Shape - Halls.Num() - Tubes.Num()];
	if (const FHall* H = FindHall(S.Hall)) return H->Zone;
	return TEXT("F2");
}

void FWRFungalData::ShapesIn(const FBox& Box, TArray<int32>& Out) const
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

float FWRFungalData::ShapeAir(int32 Shape, const FVector& P) const
{
	using namespace WRFungalTypesImpl;
	if (Shape < Halls.Num())
	{
		const FHall& H = Halls[Shape];
		const FVector2D L2 = H.Local(P);
		if (H.IsDoline())
		{
			// A doline: a round shaft from its floor up through the ground, its wall undercut 4% at the foot (the water
			// that dissolved it worked hardest at the bottom), knobbled by noise; the floor nearly flat (fallen soil).
			const float Up = FMath::Clamp((float(P.Z) - H.At.Z) / FMath::Max(100.f, H.Top - H.At.Z), 0.f, 1.f);
			const float R = H.Rx * (1.f + 0.04f * (1.f - FgSmooth(0.f, 0.45f, Up))) * (1.f + 0.05f * FgNoise3(P / 700.f)) + FgNoise3(P / 160.f + FVector(5.f)) * 35.f;
			const float Dist = FVector2D(L2).Size() - R;
			const float Floor = H.At.Z + FgNoise3(P / 400.f + FVector(2.f)) * 25.f;
			return FMath::Max3(Dist, Floor - float(P.Z), float(P.Z) - H.Top);
		}
		const FVector C = H.Centre();
		float D = FgEllipsoid(FVector(L2.X, L2.Y, P.Z - C.Z), FVector(H.Rx, H.Ry, H.Rz()));
		// Limestone halls are never a clean shell: flowstone bulges, solution pockets (6% of the hall's size), and a
		// finer knobbling; the Heart chamber and the city's rooms are smoother (grown, not dissolved).
		const float Size = FMath::Min(H.Rx, H.Ry);
		const bool bGrown = H.Zone == TEXT("HG");
		D += FgNoise3(P / 560.f) * (bGrown ? 0.03f : 0.065f) * Size + FgNoise3(P / 150.f + FVector(7.f)) * (bGrown ? 12.f : 30.f);
		// The floor: worn level (a chasm's is talus, rougher).
		const float Rough = H.IsChasm() ? 90.f : 10.f;
		const float Floor = H.At.Z + FgNoise3(P / 320.f + FVector(3.f)) * Rough;
		return FMath::Max(D, Floor - float(P.Z));
	}
	if (Shape < Halls.Num() + Tubes.Num())
	{
		const FTube& T = Tubes[Shape - Halls.Num()];
		float Floor = 0.f, Tt = 0.f;
		int32 Seg = 0;
		FVector Dir;
		const float Lat = T.Nearest(P, Floor, Seg, Tt, Dir);
		if (T.Kind != TEXT("tube"))
		{
			// Hewn or low: a flat floor, near-straight sides, a low vault (a crawl's roof sags 40 cm at its middle).
			const float Half = T.W * 0.5f;
			const float Sag = T.Kind == TEXT("crawl") ? 40.f : 70.f;
			const float Ceil = Floor + T.H - Sag * (1.f - FMath::Square(FMath::Clamp(Lat / Half, 0.f, 1.f)));
			const float D = FMath::Max3(Lat - Half, Floor - float(P.Z), float(P.Z) - Ceil);
			return D + FgNoise3(P / 90.f) * (T.Kind == TEXT("crawl") ? 9.f : 3.f);
		}
		// A dissolved tube: round, its axis 0.55 r over the floor; the floor flat (a river's gravel on the river ones).
		const float AxisZ = Floor + T.R * 0.55f;
		const float Dist = FMath::Sqrt(Lat * Lat + FMath::Square(float(P.Z) - AxisZ));
		const float D = Dist - T.R * (1.f + 0.07f * FgNoise3(P / 480.f)) - FgNoise3(P / 130.f + FVector(11.f)) * 18.f;
		const float Fl = Floor + FgNoise3(P / 260.f) * (T.bRiver ? 14.f : 7.f);
		return FMath::Max(D, Fl - float(P.Z));
	}
	const FShaft& S = Shafts[Shape - Halls.Num() - Tubes.Num()];
	const float Dist = FVector2D::Distance(FVector2D(P), FVector2D(S.At));
	const float Wobble = FgNoise3(P / 280.f) * (S.Kind == TEXT("stair") ? 6.f : 26.f);
	return FMath::Max3(Dist - S.R - Wobble, S.Z0 - float(P.Z), float(P.Z) - S.Z1);
}

float FWRFungalData::Air(const FVector& P, int32* OutNearest) const
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

const FWRFungalData::FHall* FWRFungalData::FindHall(FName Id) const
{
	return Halls.FindByPredicate([Id](const FHall& H) { return H.Id == Id; });
}

int32 FWRFungalData::HallIndex(FName Id) const
{
	return Halls.IndexOfByPredicate([Id](const FHall& H) { return H.Id == Id; });
}

const FWRFungalData::FSite* FWRFungalData::FindSite(FName Id) const
{
	return Sites.FindByPredicate([Id](const FSite& S) { return S.Id == Id; });
}

const FWRFungalData::FTube* FWRFungalData::FindTube(FName Id) const
{
	return Tubes.FindByPredicate([Id](const FTube& T) { return T.Id == Id; });
}

// ================================================================================================ WRFungal

FName WRFungal::Biome() { return WRFungalTypesImpl::FgBiome; }

bool WRFungal::Live() { return WRBiome::IsLive(WRFungalTypesImpl::FgBiome); }

bool WRFungal::InCave(const FVector& Pos)
{
	if (!Live()) return false;
	const FWRFungalData& D = FWRFungalData::Get();
	if (!D.bLoaded || !D.CaveBounds.IsInsideOrOn(Pos)) return false;
	int32 S = INDEX_NONE;
	if (D.Air(Pos, &S) > 60.f) return false;
	// Under the ground (a doline's floor is the sky's when you can see it: the open top makes its floor daylit, but it
	// is still the caverns for everything else; a descent's mouth above the ground is the surface).
	if (const FWRLandDef* L = FWRFungalData::Land())
	{
		const FVector2D M = L->ToMap(Pos);
		if (Pos.Z > L->ToWorld(M, L->Height(M)).Z - 150.f && !L->HfHiddenAt(M)) return false;
	}
	return true;
}

const FWRFungalData::FHall* WRFungal::HallAt(const FVector& Pos)
{
	if (!Live()) return nullptr;
	const FWRFungalData& D = FWRFungalData::Get();
	if (!D.bLoaded || !D.CaveBounds.IsInsideOrOn(Pos)) return nullptr;
	const FWRFungalData::FHall* Best = nullptr;
	float BestD = 60.f;
	for (int32 h = 0; h < D.Halls.Num(); ++h)
	{
		if (!D.ShapeBounds(h).IsInsideOrOn(Pos)) continue;
		const float A = D.ShapeAir(h, Pos);
		if (A < BestD) { BestD = A; Best = &D.Halls[h]; }
	}
	return Best;
}

bool WRFungal::OnFallField(const FVector& Pos)
{
	if (!Live()) return false;
	const FWRFungalData& D = FWRFungalData::Get();
	return FVector2D::Distance(FVector2D(Pos), FVector2D(D.FlueAt)) < 13000.f && Pos.Z > D.FlueAt.Z - 500.f && Pos.Z < D.FlueAt.Z + 4000.f;
}

bool WRFungal::InHeartGrotto(const FVector& Pos)
{
	if (!Live()) return false;
	const FWRFungalData& D = FWRFungalData::Get();
	if (!D.bLoaded || !D.CaveBounds.IsInsideOrOn(Pos)) return false;
	int32 S = INDEX_NONE;
	return D.Air(Pos, &S) < 60.f && D.ShapeZone(S) == TEXT("HG");
}

bool WRFungal::Contains(const FVector& Pos)
{
	if (!Live()) return false;
	return WRLand::BiomeAt(Pos) == Biome() || InCave(Pos) || OnFallField(Pos);
}

bool WRFungal::UnderRock(const FVector& Pos)
{
	if (!InCave(Pos)) return false;
	const FWRFungalData::FHall* H = HallAt(Pos);
	return !(H && H->IsDoline());
}

bool WRFungal::SkyAbove(const FVector& Pos)
{
	if (!Live()) return false;
	const FWRLandDef* L = FWRFungalData::Land();
	if (!L || !L->Contains(Pos, 0.f)) return false;
	const FVector2D M = L->ToMap(Pos);
	if (L->HfHiddenAt(M)) return false;
	return Pos.Z > L->ToWorld(M, L->Height(M)).Z + WRTune(TEXT("fungalWorld.skyAbove"), 30000.f);
}

bool WRFungal::FlueMouth(float& OutYaw, float& OutHalfDeg, float& OutTop)
{
	if (!Live()) return false;
	const FWRFungalData& D = FWRFungalData::Get();
	if (!D.bLoaded) return false;
	OutYaw = D.FlueMouthYaw;
	OutHalfDeg = D.FlueMouthHalf;
	OutTop = D.FlueMouthTop;
	return true;
}

FName WRFungal::ZoneAt(const FVector& Pos)
{
	if (!Live()) return NAME_None;
	if (InCave(Pos))
	{
		int32 S = INDEX_NONE;
		FWRFungalData::Get().Air(Pos, &S);
		return FWRFungalData::Get().ShapeZone(S);
	}
	if (OnFallField(Pos)) return TEXT("F5");
	if (WRLand::BiomeAt(Pos) != Biome()) return NAME_None;
	return TEXT("F1");
}
