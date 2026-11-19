#include "WROceanTypes.h"
#include "Wildreach.h"
#include "WRFarm.h"
#include "WRLand.h"
#include "WRTide.h"
#include "WRTuning.h"
#include "Dom/JsonObject.h"

namespace WROceanImpl
{
	static const FName OBiome(TEXT("ocean"));
	double ONum(const TSharedPtr<FJsonObject>& O, const TCHAR* K, double F = 0.0) { double V = F; return O.IsValid() && O->TryGetNumberField(K, V) ? V : F; }
	FString OStr(const TSharedPtr<FJsonObject>& O, const TCHAR* K) { FString V; if (O.IsValid()) O->TryGetStringField(K, V); return V; }
	FVector OPt(const TArray<TSharedPtr<FJsonValue>>& A, int32 From = 0)
	{
		return FVector(A.Num() > From ? A[From]->AsNumber() : 0.0, A.Num() > From + 1 ? A[From + 1]->AsNumber() : 0.0, A.Num() > From + 2 ? A[From + 2]->AsNumber() : 0.0) * 100.0;
	}
	FVector OPtField(const TSharedPtr<FJsonObject>& O, const TCHAR* K)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		return O.IsValid() && O->TryGetArrayField(K, A) ? OPt(*A) : FVector::ZeroVector;
	}
	FWROceanData::FPlace OPlace(const TSharedPtr<FJsonObject>& O)
	{
		FWROceanData::FPlace P;
		P.Id = FName(*OStr(O, TEXT("id")));
		P.Kind = FName(*OStr(O, TEXT("kind")));
		P.Name = OStr(O, TEXT("name"));
		P.Zone = FName(*OStr(O, TEXT("zone")));
		P.At = FVector(ONum(O, TEXT("x")), ONum(O, TEXT("y")), ONum(O, TEXT("z"))) * 100.0;
		P.Yaw = float(ONum(O, TEXT("yaw")));
		P.Variant = int32(ONum(O, TEXT("variant")));
		P.Size = float(ONum(O, TEXT("size"))) * 100.f;
		P.Lean = float(ONum(O, TEXT("lean")));
		P.Stack = float(ONum(O, TEXT("stack"))) * 100.f;
		bool B = false;
		if (O->TryGetBoolField(TEXT("sea"), B)) P.bSea = B;
		if (O->TryGetBoolField(TEXT("camp"), B)) P.bCamp = B;
		return P;
	}
	void ODecor(const TSharedPtr<FJsonObject>& Decor, const TCHAR* Key, TArray<FWROceanData::FDecor>& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (!Decor.IsValid() || !Decor->TryGetArrayField(Key, A)) return;
		Out.Reserve(A->Num());
		for (const auto& V : *A)
		{
			const TArray<TSharedPtr<FJsonValue>>& R = V->AsArray();
			FWROceanData::FDecor D;
			D.At = OPt(R);
			D.Yaw = R.Num() > 3 ? float(R[3]->AsNumber()) : 0.f;
			D.Scale = R.Num() > 4 ? float(R[4]->AsNumber()) : 1.f;
			D.Variant = R.Num() > 5 ? int32(R[5]->AsNumber()) : 0;
			Out.Add(D);
		}
	}
	void OPoints(const TSharedPtr<FJsonObject>& J, const TCHAR* Key, TArray<FVector>& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (!J->TryGetArrayField(Key, A)) return;
		for (const auto& V : *A) Out.Add(OPt(V->AsArray()));
	}
	void OGives(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, TMap<FName, int32>& Out)
	{
		const TSharedPtr<FJsonObject>* P = nullptr;
		if (!O.IsValid() || !O->TryGetObjectField(Key, P)) return;
		for (const auto& KV : (*P)->Values) Out.Add(FName(*KV.Key), int32(KV.Value->AsNumber()));
	}
}

const FWROceanData& FWROceanData::Get()
{
	static FWROceanData D;
	static bool bTried = false;
	if (bTried) return D;
	bTried = true;
	const TSharedPtr<FJsonObject> J = FWRTuning::LoadDataFile(TEXT("ocean.json"));
	if (!J.IsValid())
	{
		UE_LOG(LogWildreach, Error, TEXT("OCEAN: Data/ocean.json is missing (node Tools/terrain/ocean.mjs)"));
		return D;
	}
	D.bLoaded = D.Parse(J);
	return D;
}

bool FWROceanData::Parse(const TSharedPtr<FJsonObject>& J)
{
	using namespace WROceanImpl;
	SeaM = float(ONum(J, TEXT("sea"), 0.6));
	const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
	if (J->TryGetArrayField(TEXT("frame"), A) && A->Num() >= 4)
	{
		Frame = FBox2D(FVector2D((*A)[0]->AsNumber(), (*A)[1]->AsNumber()) * 100.0, FVector2D((*A)[2]->AsNumber(), (*A)[3]->AsNumber()) * 100.0);
	}
	if (const TSharedPtr<FJsonObject>* Z = nullptr; J->TryGetObjectField(TEXT("zones"), Z))
	{
		for (const auto& KV : (*Z)->Values) ZoneNames.Add(FName(*KV.Key), KV.Value->AsString());
	}
	OPoints(J, TEXT("coast"), Coast);
	const auto Places = [&J](const TCHAR* Key, TArray<FPlace>& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* L = nullptr;
		if (!J->TryGetArrayField(Key, L)) return;
		for (const auto& V : *L) Out.Add(OPlace(V->AsObject()));
	};
	Places(TEXT("landmarks"), Landmarks);
	Places(TEXT("sites"), Sites);
	Places(TEXT("kit"), Kit);
	if (const TSharedPtr<FJsonObject>* Dc = nullptr; J->TryGetObjectField(TEXT("decor"), Dc))
	{
		ODecor(*Dc, TEXT("grass"), Grass);
		ODecor(*Dc, TEXT("driftwood"), Driftwood);
		ODecor(*Dc, TEXT("tamarisk"), Tamarisk);
		ODecor(*Dc, TEXT("kelp"), Kelp);
		ODecor(*Dc, TEXT("coral"), Coral);
		ODecor(*Dc, TEXT("clam"), Clams);
		ODecor(*Dc, TEXT("rock"), Rocks);
		ODecor(*Dc, TEXT("barnacle"), Barnacles);
		ODecor(*Dc, TEXT("urchin"), Urchins);
		ODecor(*Dc, TEXT("mangrove"), Mangroves);
		const TArray<TSharedPtr<FJsonValue>>* W = nullptr;
		if ((*Dc)->TryGetArrayField(TEXT("wall"), W))
		{
			for (const auto& V : *W) { const auto& R = V->AsArray(); FWall Wl; Wl.A = OPt(R, 0); Wl.B = OPt(R, 3); Walls.Add(Wl); }
		}
	}
	if (J->TryGetArrayField(TEXT("caves"), A))
	{
		for (const auto& V : *A)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			FCave C;
			C.Id = FName(*OStr(O, TEXT("id")));
			C.Name = OStr(O, TEXT("name"));
			C.Zone = FName(*OStr(O, TEXT("zone")));
			C.At = FVector(ONum(O, TEXT("x")), ONum(O, TEXT("y")), ONum(O, TEXT("z"))) * 100.0;
			C.Roof = float(ONum(O, TEXT("roof"), 6.0)) * 100.f;
			bool B = false;
			if (O->TryGetBoolField(TEXT("dry"), B)) C.bDry = B;
			const TArray<TSharedPtr<FJsonValue>>* R = nullptr;
			if (O->TryGetArrayField(TEXT("rooms"), R)) for (const auto& Rv : *R) C.Rooms.Add(OPt(Rv->AsArray()));
			if (const TSharedPtr<FJsonObject>* M = nullptr; O->TryGetObjectField(TEXT("mouth"), M))
			{
				C.MouthOffset = FVector(ONum(*M, TEXT("dx")), ONum(*M, TEXT("dy")), 0.0) * 100.0;
				C.MouthYaw = float(ONum(*M, TEXT("yaw")));
				C.MouthW = float(ONum(*M, TEXT("w"), 6.0)) * 100.f;
				C.MouthH = float(ONum(*M, TEXT("h"), 5.0)) * 100.f;
			}
			C.Arena = FName(*OStr(O, TEXT("arena")));
			C.Legendary = FName(*OStr(O, TEXT("legendary")));
			C.Side = OPtField(O, TEXT("side"));
			if (O->TryGetArrayField(TEXT("islands"), R)) for (const auto& Rv : *R) C.Islands.Add(OPt(Rv->AsArray()));
			OGives(O, TEXT("creatures"), C.Creatures);
			Caves.Add(C);
		}
	}
	if (const TSharedPtr<FJsonObject>* H = nullptr; J->TryGetObjectField(TEXT("hall"), H))
	{
		HallAt = OPtField(*H, TEXT("at"));
		if ((*H)->TryGetArrayField(TEXT("rooms"), A))
		{
			for (const auto& V : *A)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				FHallRoom R;
				R.Id = FName(*OStr(O, TEXT("id")));
				R.Name = OStr(O, TEXT("name"));
				R.At = FVector(HallAt.X + ONum(O, TEXT("dx")) * 100.0, HallAt.Y + ONum(O, TEXT("dy")) * 100.0, ONum(O, TEXT("z")) * 100.0);
				R.R = float(ONum(O, TEXT("r"), 8.0)) * 100.f;
				FString Air;
				bool bAir = false;
				if (O->TryGetBoolField(TEXT("air"), bAir)) R.bAir = bAir;
				else if (O->TryGetStringField(TEXT("air"), Air) && Air == TEXT("dome")) { R.bAir = true; R.bDome = true; }
				R.AirTop = float(ONum(O, TEXT("airTop"))) * 100.f;
				R.WaterTop = float(ONum(O, TEXT("waterTop"))) * 100.f;
				HallRooms.Add(R);
			}
		}
		if ((*H)->TryGetArrayField(TEXT("ways"), A))
		{
			for (const auto& V : *A)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				const FString Id = OStr(O, TEXT("id"));
				if (Id == TEXT("low_gate")) LowGateAt = OPtField(O, TEXT("at"));
				else if (Id == TEXT("sluice_door")) SluiceDoorTo = OPtField(O, TEXT("to"));
				else if (Id == TEXT("bell_dive")) BellDiveFrom = OPtField(O, TEXT("from"));
			}
		}
	}
	if (J->TryGetArrayField(TEXT("resources"), A))
	{
		Nodes.Reserve(A->Num());
		for (const auto& V : *A)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			FNode N;
			N.Item = FName(*OStr(O, TEXT("item")));
			N.At = FVector(ONum(O, TEXT("x")), ONum(O, TEXT("y")), ONum(O, TEXT("z"))) * 100.0;
			bool B = false;
			if (O->TryGetBoolField(TEXT("under"), B)) N.bUnder = B;
			if (O->TryGetBoolField(TEXT("wild"), B)) N.bWild = B;
			N.Cave = FName(*OStr(O, TEXT("cave")));
			N.Size = float(ONum(O, TEXT("size"), 1.0));
			Nodes.Add(N);
		}
	}
	if (const TSharedPtr<FJsonObject>* T = nullptr; J->TryGetObjectField(TEXT("tsunami"), T))
	{
		TsunamiContour = float(ONum(*T, TEXT("contour"), 6.0)) * 100.f;
		if ((*T)->TryGetArrayField(TEXT("stones"), A))
		{
			for (const auto& V : *A) { const auto& R = V->AsArray(); TsunamiStones.Add(OPt(R)); TsunamiYaw.Add(R.Num() > 3 ? float(R[3]->AsNumber()) : 0.f); }
		}
	}
	if (const TSharedPtr<FJsonObject>* Dk = nullptr; J->TryGetObjectField(TEXT("dyke"), Dk))
	{
		OPoints(*Dk, TEXT("towers"), DykeTowers);
		DykeWallTop = float(ONum(*Dk, TEXT("wallTop"), -1.0)) * 100.f;
	}
	OPoints(J, TEXT("sluiceway"), Sluiceway);
	OPoints(J, TEXT("inlet"), Inlet);
	OPoints(J, TEXT("road"), Road);
	OPoints(J, TEXT("saltgate"), Saltgate);
	OPoints(J, TEXT("mooringStones"), MooringStones);
	if (J->TryGetArrayField(TEXT("rips"), A))
	{
		for (const auto& V : *A) { const auto O = V->AsObject(); FRip R; R.At = FVector(ONum(O, TEXT("x")), ONum(O, TEXT("y")), 0.0) * 100.0; R.Length = float(ONum(O, TEXT("length"), 120.0)) * 100.f; R.Width = float(ONum(O, TEXT("width"), 8.0)) * 100.f; Rips.Add(R); }
	}
	if (J->TryGetArrayField(TEXT("blowholes"), A))
	{
		for (const auto& V : *A)
		{
			const auto O = V->AsObject(); FBlowhole B;
			B.At = FVector(ONum(O, TEXT("x")), ONum(O, TEXT("y")), ONum(O, TEXT("z"))) * 100.0;
			bool H = false; if (O->TryGetBoolField(TEXT("horn"), H)) B.bHorn = H;
			B.Ledge = OPtField(O, TEXT("ledge"));
			Blowholes.Add(B);
		}
	}
	if (J->TryGetArrayField(TEXT("surges"), A))
	{
		for (const auto& V : *A) { const auto O = V->AsObject(); FSurge S; S.At = FVector(ONum(O, TEXT("x")), ONum(O, TEXT("y")), ONum(O, TEXT("z"))) * 100.0; S.Length = float(ONum(O, TEXT("length"), 60.0)) * 100.f; S.Yaw = float(ONum(O, TEXT("yaw"), 180.0)); Surges.Add(S); }
	}
	if (J->TryGetArrayField(TEXT("fleets"), A))
	{
		for (const auto& V : *A) { const auto O = V->AsObject(); FFleet F; F.At = FVector(ONum(O, TEXT("x")), ONum(O, TEXT("y")), 0.0) * 100.0; F.Count = int32(ONum(O, TEXT("count"), 10.0)); Fleets.Add(F); }
	}
	if (J->TryGetArrayField(TEXT("airPockets"), A))
	{
		for (const auto& V : *A)
		{
			const auto O = V->AsObject(); FPocket P;
			P.Cave = FName(*OStr(O, TEXT("cave")));
			P.Hall = FName(*OStr(O, TEXT("hall")));
			P.At = FVector(ONum(O, TEXT("x")), ONum(O, TEXT("y")), ONum(O, TEXT("z"))) * 100.0;
			P.R = float(ONum(O, TEXT("r"), 10.0)) * 100.f;
			P.Top = float(ONum(O, TEXT("top"), ONum(O, TEXT("z")) + 4.0)) * 100.f;
			Pockets.Add(P);
		}
	}
	if (const TSharedPtr<FJsonObject>* R = nullptr; J->TryGetObjectField(TEXT("routes"), R))
	{
		for (const auto& KV : (*R)->Values)
		{
			TArray<FVector>& Pts = Routes.Add(FName(*KV.Key));
			for (const auto& V : KV.Value->AsArray()) Pts.Add(OPt(V->AsArray()));
		}
	}
	if (J->TryGetArrayField(TEXT("secrets"), A))
	{
		for (const auto& V : *A)
		{
			const auto O = V->AsObject(); FSecret S;
			S.Id = FName(*OStr(O, TEXT("id")));
			S.Kind = FName(*OStr(O, TEXT("kind")));
			S.At = OPtField(O, TEXT("at"));
			S.When = FName(*OStr(O, TEXT("when")));
			S.Needs = FName(*OStr(O, TEXT("needs")));
			OGives(O, TEXT("pays"), S.Pays);
			Secrets.Add(S);
		}
	}
	if (J->TryGetArrayField(TEXT("portalSlots"), A))
	{
		for (const auto& V : *A) { const auto O = V->AsObject(); FSlot S; S.Rank = FName(*OStr(O, TEXT("rank"))); S.At = FVector(ONum(O, TEXT("x")), ONum(O, TEXT("y")), ONum(O, TEXT("z"))) * 100.0; PortalSlots.Add(S); }
	}
	const TSharedPtr<FJsonObject>* Ar = nullptr;
	if (J->TryGetObjectField(TEXT("arenas"), Ar)) Arenas = *Ar;
	UE_LOG(LogWildreach, Log, TEXT("OCEAN data: %d landmarks, %d sites, %d kit, %d caves, %d hall rooms, %d nodes, %d decor (kelp %d, coral %d)"),
		Landmarks.Num(), Sites.Num(), Kit.Num(), Caves.Num(), HallRooms.Num(), Nodes.Num(),
		Grass.Num() + Driftwood.Num() + Tamarisk.Num() + Kelp.Num() + Coral.Num() + Clams.Num() + Rocks.Num() + Barnacles.Num() + Urchins.Num() + Mangroves.Num(), Kelp.Num(), Coral.Num());
	return Sites.Num() > 0;
}

const FWROceanData::FPlace* FWROceanData::FindSite(FName Id) const { return Sites.FindByPredicate([Id](const FPlace& P) { return P.Id == Id; }); }
const FWROceanData::FPlace* FWROceanData::FindLandmark(FName Id) const { return Landmarks.FindByPredicate([Id](const FPlace& P) { return P.Id == Id; }); }
const FWROceanData::FCave* FWROceanData::FindCave(FName Id) const { return Caves.FindByPredicate([Id](const FCave& C) { return C.Id == Id; }); }
const FWROceanData::FHallRoom* FWROceanData::FindHallRoom(FName Id) const { return HallRooms.FindByPredicate([Id](const FHallRoom& R) { return R.Id == Id; }); }

FVector FWROceanData::PoundAt() const
{
	FVector P;
	if (!ArenaPoint(TEXT("thalassor"), TEXT("at"), P)) return FVector::ZeroVector;
	P.Z = ArenaNum(TEXT("thalassor"), TEXT("flat"), -0.1f) * 100.f;
	return P;
}

float FWROceanData::PoundRadius() const { return ArenaNum(TEXT("thalassor"), TEXT("wall"), 120.f) * 100.f; }

float FWROceanData::ArenaNum(const TCHAR* Arena, const TCHAR* Key, float Fallback) const
{
	const TSharedPtr<FJsonObject>* O = nullptr;
	double V = Fallback;
	return Arenas.IsValid() && Arenas->TryGetObjectField(Arena, O) && (*O)->TryGetNumberField(Key, V) ? float(V) : Fallback;
}

bool FWROceanData::ArenaPoint(const TCHAR* Arena, const TCHAR* Key, FVector& Out) const
{
	const TSharedPtr<FJsonObject>* O = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
	if (!Arenas.IsValid() || !Arenas->TryGetObjectField(Arena, O) || !(*O)->TryGetArrayField(Key, A) || A->Num() < 2) return false;
	Out = WROceanImpl::OPt(*A);
	return true;
}

TArray<FVector> FWROceanData::ArenaPoints(const TCHAR* Arena, const TCHAR* Key) const
{
	TArray<FVector> Out;
	const TSharedPtr<FJsonObject>* O = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
	if (!Arenas.IsValid() || !Arenas->TryGetObjectField(Arena, O) || !(*O)->TryGetArrayField(Key, A)) return Out;
	for (const auto& V : *A) Out.Add(WROceanImpl::OPt(V->AsArray()));
	return Out;
}

float FWROceanData::CoastX(float Y) const
{
	if (Coast.Num() < 2) return 185000.f;
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

FName WROcean::Biome() { return WROceanImpl::OBiome; }

bool WROcean::Live()
{
	// Per map: the round island's map has none of the Old Island's lands (WRLand::OwnLandMap).
	static int32 Cached = -1;
	static FString CachedMap;
	if (Cached < 0 || CachedMap != WRLand::CurrentMap()) { CachedMap = WRLand::CurrentMap(); Cached = WRBiome::IsLive(WROceanImpl::OBiome) && FWROceanData::Get().bLoaded ? 1 : 0; }
	return Cached == 1;
}

bool WROcean::Contains(const FVector& World)
{
	if (!Live()) return false;
	const FWROceanData& D = FWROceanData::Get();
	return D.Frame.IsInside(FVector2D(World.X, World.Y));
}

FName WROcean::ZoneAt(const FVector& World)
{
	if (!Contains(World)) return NAME_None;
	const FWROceanData& D = FWROceanData::Get();
	static const FName O1(TEXT("O1")), O2(TEXT("O2")), O3(TEXT("O3")), O4(TEXT("O4")), O5(TEXT("O5"));
	if (World.X > 250000.f) return O5;
	if (World.X > D.CoastX(World.Y)) return O4;
	// The shelf: north of the cook's y -6,950 (squeezed: -6,865 m), east of x +1,100.
	const bool bShelf = World.Y < -686500.f;
	if (World.X < 130000.f && !(bShelf && World.X > 110000.f)) return O3;
	return bShelf ? O2 : O1;
}

bool WROcean::InSea(const FVector& World)
{
	if (!Contains(World)) return false;
	return World.X > FWROceanData::Get().CoastX(World.Y) || World.Z < WRTide::MeanZ() - 100.f;
}

bool WROcean::InCave(const FVector& World)
{
	if (!Contains(World)) return false;
	const FWROceanData& D = FWROceanData::Get();
	for (const FWROceanData::FCave& C : D.Caves)
	{
		for (const FVector& R : C.Rooms)
		{
			const FVector Centre(C.At.X + R.X, C.At.Y + R.Y, C.At.Z);
			if (FVector::Dist2D(World, Centre) < R.Z + 400.f && World.Z > C.At.Z - 300.f && World.Z < C.At.Z + C.Roof + 300.f) return true;
		}
	}
	return InHall(World);
}

bool WROcean::InHall(const FVector& World)
{
	if (!Contains(World)) return false;
	const FWROceanData& D = FWROceanData::Get();
	for (const FWROceanData::FHallRoom& R : D.HallRooms)
	{
		if (FVector::Dist2D(World, R.At) < R.R + 300.f && World.Z > R.At.Z - 400.f && World.Z < R.At.Z + 900.f) return true;
	}
	return false;
}

bool WROcean::BelowTsunamiLine(const UObject* WorldContext, const FVector& World)
{
	if (!Contains(World)) return false;
	// Seaward of the stones' line and lower than the contour (the bore runs up to the line and no further).
	const float Ground = WRLand::GroundZAt(World, World.Z);
	return Ground < FWROceanData::Get().TsunamiContour + 30.f && World.X > 90000.f;
}
