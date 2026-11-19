#include "WRDesertTypes.h"
#include "Wildreach.h"
#include "WRFarm.h"
#include "WRLand.h"
#include "WRTuning.h"
#include "Dom/JsonObject.h"

namespace WRDesertImpl
{
	static const FName DBiome(TEXT("desert"));
	double DNum(const TSharedPtr<FJsonObject>& O, const TCHAR* K, double F = 0.0) { double V = F; return O.IsValid() && O->TryGetNumberField(K, V) ? V : F; }
	FString DStr(const TSharedPtr<FJsonObject>& O, const TCHAR* K) { FString V; if (O.IsValid()) O->TryGetStringField(K, V); return V; }
	bool DBool(const TSharedPtr<FJsonObject>& O, const TCHAR* K) { bool B = false; return O.IsValid() && O->TryGetBoolField(K, B) && B; }
	FVector DPt(const TArray<TSharedPtr<FJsonValue>>& A, int32 From = 0)
	{
		return FVector(A.Num() > From ? A[From]->AsNumber() : 0.0, A.Num() > From + 1 ? A[From + 1]->AsNumber() : 0.0, A.Num() > From + 2 ? A[From + 2]->AsNumber() : 0.0) * 100.0;
	}
	FVector DPtField(const TSharedPtr<FJsonObject>& O, const TCHAR* K)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		return O.IsValid() && O->TryGetArrayField(K, A) ? DPt(*A) : FVector::ZeroVector;
	}
	FWRDesertData::FPlace DPlace(const TSharedPtr<FJsonObject>& O)
	{
		FWRDesertData::FPlace P;
		P.Id = FName(*DStr(O, TEXT("id")));
		P.Kind = FName(*DStr(O, TEXT("kind")));
		P.Name = DStr(O, TEXT("name"));
		P.Zone = FName(*DStr(O, TEXT("zone")));
		P.At = FVector(DNum(O, TEXT("x")), DNum(O, TEXT("y")), DNum(O, TEXT("z"))) * 100.0;
		P.Yaw = float(DNum(O, TEXT("yaw")));
		P.Variant = int32(DNum(O, TEXT("variant")));
		P.Size = float(DNum(O, TEXT("size"))) * 100.f;
		P.Lean = float(DNum(O, TEXT("lean")));
		P.bCamp = DBool(O, TEXT("camp"));
		P.bTilted = DBool(O, TEXT("tilted"));
		const FString Ore = DStr(O, TEXT("ore"));
		if (!Ore.IsEmpty()) P.Ore = FName(*Ore);
		const FString Route = DStr(O, TEXT("route"));
		if (!Route.IsEmpty()) P.Route = FName(*Route);
		P.Ship = DPtField(O, TEXT("ship"));
		return P;
	}
	void DDecor(const TSharedPtr<FJsonObject>& Decor, const TCHAR* Key, TArray<FWRDesertData::FDecor>& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (!Decor.IsValid() || !Decor->TryGetArrayField(Key, A)) return;
		Out.Reserve(A->Num());
		for (const auto& V : *A)
		{
			const TArray<TSharedPtr<FJsonValue>>& R = V->AsArray();
			FWRDesertData::FDecor D;
			D.At = DPt(R);
			D.Yaw = R.Num() > 3 ? float(R[3]->AsNumber()) : 0.f;
			D.Scale = R.Num() > 4 ? float(R[4]->AsNumber()) : 1.f;
			Out.Add(D);
		}
	}
	void DPoints(const TSharedPtr<FJsonObject>& J, const TCHAR* Key, TArray<FVector>& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (!J.IsValid() || !J->TryGetArrayField(Key, A)) return;
		for (const auto& V : *A) Out.Add(DPt(V->AsArray()));
	}
	void DPits(const TSharedPtr<FJsonObject>& J, const TCHAR* Key, TArray<FWRDesertData::FPit>& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (!J->TryGetArrayField(Key, A)) return;
		for (const auto& V : *A)
		{
			const auto& R = V->AsArray();
			FWRDesertData::FPit P;
			P.At = DPt(R);
			P.R = R.Num() > 3 ? float(R[3]->AsNumber()) * 100.f : 400.f;
			Out.Add(P);
		}
	}
	bool PointInPoly(const FVector2D& P, const TArray<FVector2D>& Poly)
	{
		bool bIn = false;
		for (int32 i = 0, j = Poly.Num() - 1; i < Poly.Num(); j = i++)
		{
			const FVector2D& A = Poly[i];
			const FVector2D& B = Poly[j];
			if ((A.Y > P.Y) != (B.Y > P.Y) && P.X < (B.X - A.X) * (P.Y - A.Y) / (B.Y - A.Y) + A.X) bIn = !bIn;
		}
		return bIn;
	}
}

const FWRDesertData& FWRDesertData::Get()
{
	static FWRDesertData D;
	static bool bTried = false;
	if (bTried) return D;
	bTried = true;
	const TSharedPtr<FJsonObject> J = FWRTuning::LoadDataFile(TEXT("desert.json"));
	if (!J.IsValid())
	{
		UE_LOG(LogWildreach, Error, TEXT("DESERT: Data/desert.json is missing (node Tools/terrain/desert.mjs)"));
		return D;
	}
	D.bLoaded = D.Parse(J);
	return D;
}

bool FWRDesertData::Parse(const TSharedPtr<FJsonObject>& J)
{
	using namespace WRDesertImpl;
	PanZ = float(DNum(J, TEXT("pan"), 26.0)) * 100.f;
	const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
	if (J->TryGetArrayField(TEXT("frame"), A) && A->Num() >= 4)
	{
		Frame = FBox2D(FVector2D((*A)[0]->AsNumber(), (*A)[1]->AsNumber()) * 100.0, FVector2D((*A)[2]->AsNumber(), (*A)[3]->AsNumber()) * 100.0);
	}
	if (const TSharedPtr<FJsonObject>* Z = nullptr; J->TryGetObjectField(TEXT("zones"), Z))
	{
		for (const auto& KV : (*Z)->Values) ZoneNames.Add(FName(*KV.Key), KV.Value->AsString());
	}
	DPoints(J, TEXT("coast"), Coast);
	DPoints(J, TEXT("wadi"), Wadi);
	DPoints(J, TEXT("saltgate"), Saltgate);
	DPoints(J, TEXT("whaleRoad"), WhaleRoad);
	{
		TArray<FVector> Poly;
		DPoints(J, TEXT("panPoly"), Poly);
		for (const FVector& P : Poly) PanPoly.Add(FVector2D(P.X, P.Y));
	}
	if (J->TryGetArrayField(TEXT("mother"), A) && A->Num() >= 4)
	{
		MotherAt = FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), 0.0) * 100.0;
		MotherH = float((*A)[2]->AsNumber()) * 100.f;
		for (const auto& V : (*A)[3]->AsArray()) MotherArms.Add(float(V->AsNumber()));
	}
	const auto Places = [&J](const TCHAR* Key, TArray<FPlace>& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* L = nullptr;
		if (!J->TryGetArrayField(Key, L)) return;
		for (const auto& V : *L) Out.Add(DPlace(V->AsObject()));
	};
	Places(TEXT("landmarks"), Landmarks);
	Places(TEXT("sites"), Sites);
	Places(TEXT("kit"), Kit);
	if (const TSharedPtr<FJsonObject>* Dc = nullptr; J->TryGetObjectField(TEXT("decor"), Dc))
	{
		DDecor(*Dc, TEXT("grass"), Grass);
		DDecor(*Dc, TEXT("driftwood"), Driftwood);
		DDecor(*Dc, TEXT("saltbush"), Saltbush);
		DDecor(*Dc, TEXT("palm"), Palms);
		DDecor(*Dc, TEXT("reed"), Reeds);
		DDecor(*Dc, TEXT("acacia"), Acacias);
		DDecor(*Dc, TEXT("pear"), Pears);
		DDecor(*Dc, TEXT("rock"), Rocks);
		DDecor(*Dc, TEXT("pebbles"), Pebbles);
		DDecor(*Dc, TEXT("hoodoo"), Hoodoos);
		DDecor(*Dc, TEXT("shell"), Shells);
	}
	if (J->TryGetArrayField(TEXT("caves"), A))
	{
		for (const auto& V : *A)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			FCave C;
			C.Id = FName(*DStr(O, TEXT("id")));
			C.Name = DStr(O, TEXT("name"));
			C.Zone = FName(*DStr(O, TEXT("zone")));
			C.At = FVector(DNum(O, TEXT("x")), DNum(O, TEXT("y")), DNum(O, TEXT("z"))) * 100.0;
			C.Floor = float(DNum(O, TEXT("floor"))) * 100.f;
			C.Roof = float(DNum(O, TEXT("roof"), 6.0)) * 100.f;
			C.bSandFloor = DBool(O, TEXT("sandFloor"));
			C.bShaft = DBool(O, TEXT("shaft"));
			const TArray<TSharedPtr<FJsonValue>>* R = nullptr;
			if (O->TryGetArrayField(TEXT("rooms"), R)) for (const auto& Rv : *R) C.Rooms.Add(DPt(Rv->AsArray()));
			if (O->TryGetArrayField(TEXT("flooded"), R)) for (const auto& Rv : *R) { const auto& F = Rv->AsArray(); if (F.Num() >= 4) C.Flooded.Add(FVector4f(F[0]->AsNumber() * 100.f, F[1]->AsNumber() * 100.f, F[2]->AsNumber() * 100.f, F[3]->AsNumber() * 100.f)); }
			const TSharedPtr<FJsonObject>* M = nullptr;
			if (O->TryGetObjectField(TEXT("mouth"), M))
			{
				C.MouthOffset = FVector(DNum(*M, TEXT("dx")), DNum(*M, TEXT("dy")), 0.0) * 100.0;
				C.MouthYaw = float(DNum(*M, TEXT("yaw")));
				C.MouthW = float(DNum(*M, TEXT("w"), 6.0)) * 100.f;
				C.MouthH = float(DNum(*M, TEXT("h"), 5.0)) * 100.f;
			}
			C.Arena = FName(*DStr(O, TEXT("arena")));
			const TSharedPtr<FJsonObject>* Cr = nullptr;
			if (O->TryGetObjectField(TEXT("creatures"), Cr)) for (const auto& KV : (*Cr)->Values) C.Creatures.Add(FName(*KV.Key), int32(KV.Value->AsNumber()));
			Caves.Add(C);
		}
	}
	if (const TSharedPtr<FJsonObject>* H = nullptr; J->TryGetObjectField(TEXT("hourhold"), H))
	{
		const TSharedPtr<FJsonObject> O = *H;
		HourholdAt = DPtField(O, TEXT("at"));
		HourholdSurface = float(DNum(O, TEXT("surface"))) * 100.f;
		HourholdAt.Z = HourholdSurface;
		const TArray<TSharedPtr<FJsonValue>>* R = nullptr;
		if (O->TryGetArrayField(TEXT("rooms"), R))
		{
			for (const auto& Rv : *R)
			{
				const TSharedPtr<FJsonObject> Ro = Rv->AsObject();
				FRoom Rm;
				Rm.Id = FName(*DStr(Ro, TEXT("id")));
				Rm.Name = DStr(Ro, TEXT("name"));
				Rm.At = FVector(HourholdAt.X + DNum(Ro, TEXT("dx")) * 100.0, HourholdAt.Y + DNum(Ro, TEXT("dy")) * 100.0, DNum(Ro, TEXT("z")) * 100.0);
				Rm.W = float(DNum(Ro, TEXT("w"), 10.0)) * 100.f;
				Rm.D = float(DNum(Ro, TEXT("d"), 10.0)) * 100.f;
				Rm.H = float(DNum(Ro, TEXT("h"), 6.0)) * 100.f;
				Rm.Pillars = int32(DNum(Ro, TEXT("pillars")));
				Rm.Shafts = int32(DNum(Ro, TEXT("shafts")));
				Rm.Ushabti = int32(DNum(Ro, TEXT("ushabti")));
				Rm.Mirrors = int32(DNum(Ro, TEXT("mirrors")));
				double Wt = 0.0;
				if (Ro->TryGetNumberField(TEXT("water"), Wt)) Rm.Water = float(Wt) * 100.f;
				const TArray<TSharedPtr<FJsonValue>>* Wl = nullptr;
				if (Ro->TryGetArrayField(TEXT("well"), Wl) && Wl->Num() >= 2) Rm.Well = FVector((*Wl)[0]->AsNumber(), (*Wl)[1]->AsNumber(), 0.0) * 100.0;
				Rm.Sealed = FName(*DStr(Ro, TEXT("sealed")));
				Rm.bQuicksand = DBool(Ro, TEXT("quicksand"));
				Rm.bRope = DBool(Ro, TEXT("rope"));
				Rm.bSpring = DBool(Ro, TEXT("spring"));
				Rm.bSandRamp = DBool(Ro, TEXT("sandRamp"));
				Rooms.Add(Rm);
			}
		}
		if (O->TryGetArrayField(TEXT("links"), R))
		{
			for (const auto& Lv : *R) { const auto& L = Lv->AsArray(); if (L.Num() >= 2) Links.Add(TPair<FName, FName>(FName(*L[0]->AsString()), FName(*L[1]->AsString()))); }
		}
		if (O->TryGetArrayField(TEXT("ways"), R))
		{
			for (const auto& Wv : *R)
			{
				const TSharedPtr<FJsonObject> Wo = Wv->AsObject();
				FWay W;
				W.Id = FName(*DStr(Wo, TEXT("id")));
				W.Kind = FName(*DStr(Wo, TEXT("kind")));
				W.From = DPtField(Wo, TEXT("from"));
				W.To = FName(*DStr(Wo, TEXT("to")));
				W.ToAt = DPtField(Wo, TEXT("toAt"));
				W.Opens = FName(*DStr(Wo, TEXT("opens")));
				Ways.Add(W);
			}
		}
	}
	if (J->TryGetArrayField(TEXT("resources"), A))
	{
		for (const auto& V : *A)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			FNode N;
			N.Item = FName(*DStr(O, TEXT("item")));
			N.At = FVector(DNum(O, TEXT("x")), DNum(O, TEXT("y")), DNum(O, TEXT("z"))) * 100.0;
			N.bTree = DBool(O, TEXT("tree"));
			N.bWild = DBool(O, TEXT("wild"));
			N.bHall = DBool(O, TEXT("hall"));
			N.Cave = FName(*DStr(O, TEXT("cave")));
			N.Scale = float(DNum(O, TEXT("scale"), 1.0));
			Nodes.Add(N);
		}
	}
	DPoints(J, TEXT("bloom"), Bloom);
	DPits(J, TEXT("quicksand"), Quicksand);
	DPits(J, TEXT("brine"), Brine);
	if (const TSharedPtr<FJsonObject>* B = nullptr; J->TryGetObjectField(TEXT("belt"), B))
	{
		BeltSouth = float(DNum(*B, TEXT("south"))) * 100.f;
		BeltNorth = float(DNum(*B, TEXT("north"))) * 100.f;
		BeltWest = float(DNum(*B, TEXT("west"))) * 100.f;
		BeltEast = float(DNum(*B, TEXT("east"))) * 100.f;
	}
	if (J->TryGetArrayField(TEXT("floods"), A))
	{
		for (const auto& V : *A)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			FFlood F;
			F.Id = FName(*DStr(O, TEXT("id")));
			DPoints(O, TEXT("pts"), F.Pts);
			F.Width = float(DNum(O, TEXT("width"), 10.0)) * 100.f;
			DPoints(O, TEXT("ledges"), F.Ledges);
			Floods.Add(F);
		}
	}
	if (const TSharedPtr<FJsonObject>* Dv = nullptr; J->TryGetObjectField(TEXT("devils"), Dv))
	{
		TArray<FVector> Poly;
		DPoints(*Dv, TEXT("poly"), Poly);
		for (const FVector& P : Poly) DevilPoly.Add(FVector2D(P.X, P.Y));
		const TArray<TSharedPtr<FJsonValue>>* Hr = nullptr;
		if ((*Dv)->TryGetArrayField(TEXT("hours"), Hr) && Hr->Num() >= 2) { DevilFrom = float((*Hr)[0]->AsNumber()); DevilTo = float((*Hr)[1]->AsNumber()); }
	}
	DPoints(J, TEXT("digs"), Digs);
	if (J->TryGetArrayField(TEXT("tilted"), A))
	{
		for (const auto& V : *A)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			FTilted T;
			T.Stela = DPtField(O, TEXT("stela"));
			T.Cache = DPtField(O, TEXT("cache"));
			T.bSeventh = DBool(O, TEXT("seventh"));
			Tilted.Add(T);
		}
	}
	if (const TSharedPtr<FJsonObject>* Ro = nullptr; J->TryGetObjectField(TEXT("routes"), Ro))
	{
		for (const auto& KV : (*Ro)->Values)
		{
			TArray<FVector>& L = Routes.Add(FName(*KV.Key));
			for (const auto& P : KV.Value->AsArray()) L.Add(DPt(P->AsArray()));
		}
	}
	if (J->TryGetArrayField(TEXT("portalSlots"), A))
	{
		for (const auto& V : *A) { const auto O = V->AsObject(); FSlot S; S.Rank = FName(*DStr(O, TEXT("rank"))); S.At = FVector(DNum(O, TEXT("x")), DNum(O, TEXT("y")), DNum(O, TEXT("z"))) * 100.0; PortalSlots.Add(S); }
	}
	const TSharedPtr<FJsonObject>* Ar = nullptr;
	if (J->TryGetObjectField(TEXT("arenas"), Ar)) Arenas = *Ar;
	UE_LOG(LogWildreach, Log, TEXT("DESERT data: %d landmarks, %d sites, %d kit, %d caves, %d Hourhold rooms, %d nodes, %d quicksand, %d brine, %d decor"),
		Landmarks.Num(), Sites.Num(), Kit.Num(), Caves.Num(), Rooms.Num(), Nodes.Num(), Quicksand.Num(), Brine.Num(),
		Grass.Num() + Driftwood.Num() + Saltbush.Num() + Palms.Num() + Reeds.Num() + Acacias.Num() + Pears.Num() + Rocks.Num() + Pebbles.Num() + Hoodoos.Num() + Shells.Num());
	return Sites.Num() > 0;
}

const FWRDesertData::FPlace* FWRDesertData::FindSite(FName Id) const { return Sites.FindByPredicate([Id](const FPlace& P) { return P.Id == Id; }); }
const FWRDesertData::FPlace* FWRDesertData::FindLandmark(FName Id) const { return Landmarks.FindByPredicate([Id](const FPlace& P) { return P.Id == Id; }); }
const FWRDesertData::FCave* FWRDesertData::FindCave(FName Id) const { return Caves.FindByPredicate([Id](const FCave& C) { return C.Id == Id; }); }
const FWRDesertData::FRoom* FWRDesertData::FindRoom(FName Id) const { return Rooms.FindByPredicate([Id](const FRoom& R) { return R.Id == Id; }); }

bool FWRDesertData::InPan(const FVector2D& P) const { return WRDesertImpl::PointInPoly(P, PanPoly); }

float FWRDesertData::ArenaNum(const TCHAR* Arena, const TCHAR* Key, float Fallback) const
{
	const TSharedPtr<FJsonObject>* O = nullptr;
	double V = Fallback;
	return Arenas.IsValid() && Arenas->TryGetObjectField(Arena, O) && (*O)->TryGetNumberField(Key, V) ? float(V) : Fallback;
}

bool FWRDesertData::ArenaPoint(const TCHAR* Arena, const TCHAR* Key, FVector& Out) const
{
	const TSharedPtr<FJsonObject>* O = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
	if (!Arenas.IsValid() || !Arenas->TryGetObjectField(Arena, O) || !(*O)->TryGetArrayField(Key, A) || A->Num() < 2) return false;
	Out = WRDesertImpl::DPt(*A);
	return true;
}

TArray<FVector> FWRDesertData::ArenaPoints(const TCHAR* Arena, const TCHAR* Key) const
{
	TArray<FVector> Out;
	const TSharedPtr<FJsonObject>* O = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
	if (!Arenas.IsValid() || !Arenas->TryGetObjectField(Arena, O) || !(*O)->TryGetArrayField(Key, A)) return Out;
	for (const auto& V : *A) Out.Add(WRDesertImpl::DPt(V->AsArray()));
	return Out;
}

float FWRDesertData::CoastX(float Y) const
{
	if (Coast.Num() < 2) return -295000.f;
	for (int32 i = 0; i + 1 < Coast.Num(); ++i)
	{
		const FVector& A = Coast[i];
		const FVector& B = Coast[i + 1];
		if ((Y >= A.Y && Y <= B.Y) || (Y <= A.Y && Y >= B.Y))
		{
			const float T = FMath::Abs(B.Y - A.Y) > 1.f ? (Y - A.Y) / (B.Y - A.Y) : 0.f;
			return FMath::Lerp(A.X, B.X, T);
		}
	}
	return Y < Coast[0].Y ? Coast[0].X : Coast.Last().X;
}

// ================================================================================================ the rules

FName WRDesert::Biome() { return WRDesertImpl::DBiome; }

bool WRDesert::Loaded() { return FWRDesertData::Get().bLoaded; }

bool WRDesert::Live()
{
	// Per map: the round island's map has none of the Old Island's lands (WRLand::OwnLandMap).
	static int32 Cached = -1;
	static FString CachedMap;
	if (Cached < 0 || CachedMap != WRLand::CurrentMap()) { CachedMap = WRLand::CurrentMap(); Cached = WRBiome::IsLive(WRDesertImpl::DBiome) && FWRDesertData::Get().bLoaded ? 1 : 0; }
	return Cached == 1;
}

bool WRDesert::Contains(const FVector& World)
{
	if (!Live()) return false;
	const FWRDesertData& D = FWRDesertData::Get();
	const FVector2D P(World.X, World.Y);
	// The ground's frame, plus the belt's strip north over the Fungal's dunes.
	return D.Frame.IsInside(P) || (P.X > D.BeltWest && P.X < D.BeltEast && P.Y > D.BeltNorth && P.Y < D.BeltSouth);
}

FName WRDesert::ZoneAt(const FVector& World)
{
	if (!Contains(World)) return NAME_None;
	// The land's zone grid is the truth (desert.mjs writes it from the same rule).
	static const FName D1(TEXT("D1")), D2(TEXT("D2")), D3(TEXT("D3")), D4(TEXT("D4")), D5(TEXT("D5"));
	const FWRDesertData& D = FWRDesertData::Get();
	if (World.Y < D.BeltSouth) return World.X < D.BeltWest ? D1 : D5;
	const FWRLandDef* L = WRLand::Find(WRDesertImpl::DBiome);
	const TCHAR Letter = L ? L->GridLetter(L->ToMap(World)) : 0;
	switch (Letter)
	{
	case 's': return D1;
	case 'e': return D2;
	case 'o': case 'p': return D3;
	case 'c': return D4;
	case 'w': return D5;
	default: break;
	}
	return World.X < D.CoastX(World.Y) + 30000.f ? D1 : NAME_None;
}

uint8 WRDesert::SurfaceAt(const FVector& World)
{
	if (!Live()) return EWRDesertSurf::None;
	const FWRLandDef* L = WRLand::Find(WRDesertImpl::DBiome);
	if (!L || !L->bHeightfield || L->HfSurface.Num() != L->HfHeights.Num()) return EWRDesertSurf::None;
	const FVector2D M = L->ToMap(World);
	const int32 I = FMath::RoundToInt((M.X - L->HfOrigin.X) / L->HfStep), J = FMath::RoundToInt((M.Y - L->HfOrigin.Y) / L->HfStep);
	if (I < 0 || J < 0 || I >= L->HfN || J >= L->HfN) return EWRDesertSurf::None;
	return L->HfSurface[J * L->HfN + I];
}

bool WRDesert::IsSoftSand(uint8 Surf)
{
	return Surf == EWRDesertSurf::SoftSand || Surf == EWRDesertSurf::Slipface || Surf == EWRDesertSurf::Blown || Surf == EWRDesertSurf::ShellGrit || Surf == EWRDesertSurf::Grass;
}

bool WRDesert::InBelt(const FVector& World)
{
	if (!Contains(World)) return false;
	const FWRDesertData& D = FWRDesertData::Get();
	return World.Y < D.BeltSouth && World.Y > D.BeltNorth && World.X > D.BeltWest && World.X < D.BeltEast && !InCave(World);
}

bool WRDesert::InPan(const FVector& World)
{
	return Contains(World) && FWRDesertData::Get().InPan(FVector2D(World.X, World.Y));
}

bool WRDesert::InCave(const FVector& World)
{
	if (!Contains(World)) return false;
	const FWRDesertData& D = FWRDesertData::Get();
	for (const FWRDesertData::FCave& C : D.Caves)
	{
		for (const FVector& R : C.Rooms)
		{
			const FVector Centre(C.At.X + R.X, C.At.Y + R.Y, C.Floor);
			if (FVector::Dist2D(World, Centre) < R.Z + 400.f && World.Z > C.Floor - 300.f && World.Z < C.Floor + C.Roof + 300.f) return true;
		}
	}
	return InHourhold(World);
}

bool WRDesert::InHourhold(const FVector& World)
{
	if (!Contains(World)) return false;
	const FWRDesertData& D = FWRDesertData::Get();
	if (FVector::Dist2D(World, D.HourholdAt) > 60000.f) return false;
	for (const FWRDesertData::FRoom& R : D.Rooms)
	{
		if (FMath::Abs(World.X - R.At.X) < R.W * 0.5f + 300.f && FMath::Abs(World.Y - R.At.Y) < R.D * 0.5f + 300.f && World.Z > R.At.Z - 400.f && World.Z < R.At.Z + R.H + 300.f) return true;
	}
	// The galleries between the rooms and the ways in (below the surface there).
	const auto NearSeg = [&World](const FVector& A, const FVector& B, float R)
	{
		const FVector C = FMath::ClosestPointOnSegment(World, A, B);
		return FVector::Dist(World, C) < R;
	};
	for (const TPair<FName, FName>& L : D.Links)
	{
		const FWRDesertData::FRoom* A = D.FindRoom(L.Key);
		const FWRDesertData::FRoom* B = D.FindRoom(L.Value);
		if (A && B && NearSeg(A->At + FVector(0, 0, 200.f), B->At + FVector(0, 0, 200.f), 600.f)) return true;
	}
	for (const FWRDesertData::FWay& W : D.Ways)
	{
		if (NearSeg(W.From - FVector(0, 0, 300.f), W.ToAt + FVector(0, 0, 200.f), 550.f) && FVector::Dist2D(World, W.From) > 400.f) return true;
	}
	return false;
}

int32 WRDesert::QuicksandAt(const FVector& World)
{
	if (!Contains(World)) return INDEX_NONE;
	const TArray<FWRDesertData::FPit>& L = FWRDesertData::Get().Quicksand;
	for (int32 i = 0; i < L.Num(); ++i) if (FVector::Dist2D(World, L[i].At) < L[i].R && FMath::Abs(World.Z - L[i].At.Z) < 400.f) return i;
	return INDEX_NONE;
}

int32 WRDesert::BrineAt(const FVector& World)
{
	if (!Contains(World)) return INDEX_NONE;
	const TArray<FWRDesertData::FPit>& L = FWRDesertData::Get().Brine;
	for (int32 i = 0; i < L.Num(); ++i) if (FVector::Dist2D(World, L[i].At) < L[i].R && FMath::Abs(World.Z - L[i].At.Z) < 400.f) return i;
	return INDEX_NONE;
}
