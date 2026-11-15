// The Graveyard's content (cooks/build-biome-graveyard.md 2.1-2.10): Data/graveyard.json parsed once, and the calls the
// Graveyard's parts share. See WRGraveTypes.h.

#include "WRGraveTypes.h"
#include "Wildreach.h"
#include "WRLand.h"
#include "WRFarm.h"
#include "WRTuning.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace WRGraveTypesImpl
{
	const FName GBiome(TEXT("graveyard"));

	double GNum(const TSharedPtr<FJsonObject>& O, const TCHAR* K, double Def = 0.0)
	{
		double V = Def;
		if (O.IsValid()) O->TryGetNumberField(K, V);
		return V;
	}
	bool GBool(const TSharedPtr<FJsonObject>& O, const TCHAR* K)
	{
		bool B = false;
		if (O.IsValid()) O->TryGetBoolField(K, B);
		return B;
	}
	FName GName(const TSharedPtr<FJsonObject>& O, const TCHAR* K)
	{
		FString S;
		if (O.IsValid() && O->TryGetStringField(K, S) && !S.IsEmpty()) return FName(*S);
		return NAME_None;
	}
	/** Map metres -> world cm. */
	FVector Cm(double X, double Y, double Z) { return FVector(X * 100.0, Y * 100.0, Z * 100.0); }
	/** [x, y, z...] -> world cm (z optional). */
	FVector PointOf(const TSharedPtr<FJsonValue>& V)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (!V.IsValid() || !V->TryGetArray(A) || A->Num() < 2) return FVector::ZeroVector;
		return Cm((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), A->Num() > 2 ? (*A)[2]->AsNumber() : 0.0);
	}
	TArray<FVector> Points(const TSharedPtr<FJsonObject>& O, const TCHAR* K)
	{
		TArray<FVector> Out;
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (O.IsValid() && O->TryGetArrayField(K, A)) for (const TSharedPtr<FJsonValue>& V : *A) Out.Add(PointOf(V));
		return Out;
	}
	FWRGraveData::FSite Site(const TSharedPtr<FJsonObject>& O)
	{
		FWRGraveData::FSite S;
		S.Id = GName(O, TEXT("id"));
		S.Kind = GName(O, TEXT("kind"));
		O->TryGetStringField(TEXT("name"), S.Name);
		S.At = Cm(GNum(O, TEXT("x")), GNum(O, TEXT("y")), GNum(O, TEXT("z")));
		S.Yaw = float(GNum(O, TEXT("yaw")));
		S.Variant = int32(GNum(O, TEXT("variant")));
		S.bLandmark = GBool(O, TEXT("landmark"));
		S.bHand = GBool(O, TEXT("hand"));
		S.bCamp = GBool(O, TEXT("camp"));
		S.bFill = GBool(O, TEXT("fill"));
		FString Z;
		if (O->TryGetStringField(TEXT("zone"), Z) && Z.Len()) S.Zone = Z[0];
		S.Arena = GName(O, TEXT("arena"));
		S.Lair = GName(O, TEXT("lair"));
		S.Room = GName(O, TEXT("room"));
		const TSharedPtr<FJsonValue> Top = O->TryGetField(TEXT("top"));
		if (Top.IsValid() && !Top->IsNull()) S.Top = PointOf(Top);
		return S;
	}
	const FWRLandDef* GLand()
	{
		static const FWRLandDef* L = nullptr;
		static bool bTried = false;
		if (!bTried)
		{
			bTried = true;
			for (const FWRLandDef& D : WRLand::All()) if (D.Biome == GBiome) L = &D;
		}
		return L;
	}
}

bool FWRGraveData::FRoom::Contains(const FVector& P, float Grow) const
{
	if (P.Z < At.Z - 150.f - Grow) return false;
	const float Top = bSpiral || bRamp ? FMath::Max(At.Z, Bottom) + 400.f : At.Z + H * 1.f;
	if (P.Z > Top + Grow) return false;
	if (bSpiral || bRamp)
	{
		if (P.Z < FMath::Min(At.Z, Bottom) - 150.f - Grow) return false;
	}
	const FVector2D Lp = Local(P);
	if (bRound) return Lp.Size() <= W * 0.5f + Grow;
	return FMath::Abs(Lp.X) <= W * 0.5f + Grow && FMath::Abs(Lp.Y) <= L * 0.5f + Grow;
}

FVector2D FWRGraveData::FRoom::Local(const FVector& P) const
{
	const FVector2D D(P.X - At.X, P.Y - At.Y);
	const float C = FMath::Cos(FMath::DegreesToRadians(Yaw)), S = FMath::Sin(FMath::DegreesToRadians(Yaw));
	return FVector2D(D.X * C + D.Y * S, -D.X * S + D.Y * C);
}

const FWRGraveData& FWRGraveData::Get()
{
	using namespace WRGraveTypesImpl;
	static FWRGraveData D;
	static bool bTried = false;
	if (bTried) return D;
	bTried = true;
	const TSharedPtr<FJsonObject> J = FWRTuning::LoadDataFile(TEXT("graveyard.json"));
	if (!J.IsValid())
	{
		UE_LOG(LogWildreach, Error, TEXT("GRAVEYARD: Data/graveyard.json is missing (node Tools/terrain/graveyard.mjs)"));
		return D;
	}
	const TSharedPtr<FJsonObject>* O = nullptr;
	if (J->TryGetObjectField(TEXT("frame"), O))
	{
		D.FrameMin = FVector2D(GNum(*O, TEXT("x0")), GNum(*O, TEXT("y0"))) * 100.f;
		D.FrameMax = FVector2D(GNum(*O, TEXT("x1")), GNum(*O, TEXT("y1"))) * 100.f;
	}
	if (J->TryGetObjectField(TEXT("borders"), O))
	{
		D.VoidFromY = float(GNum(*O, TEXT("voidFrom"), 4450.0)) * 100.f;
		D.VolcanicToY = float(GNum(*O, TEXT("volcanicTo"), 2560.0)) * 100.f;
	}
	const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
	if (J->TryGetArrayField(TEXT("sites"), A)) for (const auto& V : *A) D.Sites.Add(Site(V->AsObject()));
	if (J->TryGetArrayField(TEXT("kit"), A)) for (const auto& V : *A) D.Kit.Add(Site(V->AsObject()));
	if (J->TryGetArrayField(TEXT("shrines"), A))
	{
		for (const auto& V : *A)
		{
			const TSharedPtr<FJsonObject> S = V->AsObject();
			FShrine& Sh = D.Shrines.AddDefaulted_GetRef();
			Sh.Id = GName(S, TEXT("id"));
			S->TryGetStringField(TEXT("name"), Sh.Name);
			Sh.At = Cm(GNum(S, TEXT("x")), GNum(S, TEXT("y")), GNum(S, TEXT("z")));
			Sh.bGreat = GBool(S, TEXT("great"));
			Sh.Under = GName(S, TEXT("under"));
		}
	}
	if (J->TryGetArrayField(TEXT("runback"), A))
	{
		for (const auto& V : *A)
		{
			const TSharedPtr<FJsonObject> S = V->AsObject();
			D.Runback.Add({ GName(S, TEXT("boss")), Cm(GNum(S, TEXT("x")), GNum(S, TEXT("y")), GNum(S, TEXT("z"))) });
		}
	}
	if (J->TryGetArrayField(TEXT("roads"), A))
	{
		for (const auto& V : *A)
		{
			const TSharedPtr<FJsonObject> S = V->AsObject();
			FPath& P = D.Roads.AddDefaulted_GetRef();
			P.Id = GName(S, TEXT("id"));
			S->TryGetStringField(TEXT("name"), P.Name);
			P.Width = float(GNum(S, TEXT("w"), 4.0)) * 100.f;
			P.Points = Points(S, TEXT("pts"));
		}
	}
	if (J->TryGetArrayField(TEXT("toro"), A))
	{
		for (const auto& V : *A)
		{
			const TArray<TSharedPtr<FJsonValue>>& T = V->AsArray();
			if (T.Num() < 5) continue;
			D.Toro.Add({ Cm(T[0]->AsNumber(), T[1]->AsNumber(), T[2]->AsNumber()), float(T[3]->AsNumber()), T[4]->AsNumber() > 0.5 });
		}
	}
	if (J->TryGetArrayField(TEXT("fields"), A))
	{
		for (const auto& V : *A)
		{
			const TSharedPtr<FJsonObject> S = V->AsObject();
			FField& F = D.Fields.AddDefaulted_GetRef();
			F.Id = GName(S, TEXT("id"));
			F.Centre = FVector2D(GNum(S, TEXT("x")), GNum(S, TEXT("y"))) * 100.f;
			F.Half = FVector2D(GNum(S, TEXT("hu")), GNum(S, TEXT("hv"))) * 100.f;
			F.bWall = GBool(S, TEXT("wall"));
			F.bSmiths = GBool(S, TEXT("smiths"));
			F.bEnds = GBool(S, TEXT("ends"));
			F.bStaked = GBool(S, TEXT("staked"));
			FString Z;
			if (S->TryGetStringField(TEXT("zone"), Z) && Z.Len()) F.Zone = Z[0];
			const TArray<TSharedPtr<FJsonValue>>* R = nullptr;
			if (S->TryGetArrayField(TEXT("rows"), R))
			{
				for (const auto& RV : *R)
				{
					const TArray<TSharedPtr<FJsonValue>>& Q = RV->AsArray();
					if (Q.Num() < 5) continue;
					F.Rows.Add({ FVector2D(Q[0]->AsNumber(), Q[1]->AsNumber()) * 100.f, FVector2D(Q[2]->AsNumber(), Q[3]->AsNumber()) * 100.f, int32(Q[4]->AsNumber()) });
				}
			}
		}
	}
	if (J->TryGetArrayField(TEXT("trees"), A))
	{
		for (const auto& V : *A)
		{
			const TArray<TSharedPtr<FJsonValue>>& T = V->AsArray();
			if (T.Num() < 7) continue;
			D.Trees.Add({ FName(*T[0]->AsString()), Cm(T[1]->AsNumber(), T[2]->AsNumber(), T[3]->AsNumber()), float(T[4]->AsNumber()) * 100.f, float(T[5]->AsNumber()), float(T[6]->AsNumber()) });
		}
	}
	if (J->TryGetArrayField(TEXT("walls"), A))
	{
		for (const auto& V : *A)
		{
			const TSharedPtr<FJsonObject> S = V->AsObject();
			FWall& W = D.Walls.AddDefaulted_GetRef();
			W.Kind = GName(S, TEXT("kind"));
			W.Field = GName(S, TEXT("field"));
			for (const FVector& P : Points(S, TEXT("pts"))) W.Points.Add(FVector2D(P.X, P.Y));
		}
	}
	if (J->TryGetArrayField(TEXT("stream"), A))
	{
		for (const auto& V : *A)
		{
			const TArray<TSharedPtr<FJsonValue>>& T = V->AsArray();
			if (T.Num() >= 4) D.Stream.Add(FVector4f(float(T[0]->AsNumber() * 100.0), float(T[1]->AsNumber() * 100.0), float(T[2]->AsNumber() * 100.0), float(T[3]->AsNumber() * 100.0)));
		}
	}
	if (J->TryGetObjectField(TEXT("pond"), O))
	{
		D.PondAt = Cm(GNum(*O, TEXT("x")), GNum(*O, TEXT("y")), GNum(*O, TEXT("z")));
		D.PondRadii = FVector2D(GNum(*O, TEXT("rx"), 30.0), GNum(*O, TEXT("ry"), 20.0)) * 100.f;
	}
	if (J->TryGetArrayField(TEXT("bonePits"), A))
	{
		for (const auto& V : *A)
		{
			const TArray<TSharedPtr<FJsonValue>>& T = V->AsArray();
			if (T.Num() < 5) continue;
			D.BonePits.Add(FVector4f(float(T[0]->AsNumber() * 100.0), float(T[1]->AsNumber() * 100.0), float(T[4]->AsNumber() * 100.0), float(T[2]->AsNumber() * 100.0)));
			D.BonePitDepth.Add(float(T[3]->AsNumber() * 100.0));
		}
	}
	if (J->TryGetObjectField(TEXT("hazards"), O))
	{
		if ((*O)->TryGetArrayField(TEXT("subsidence"), A))
		{
			for (const auto& V : *A)
			{
				const TArray<TSharedPtr<FJsonValue>>& T = V->AsArray();
				if (T.Num() < 5) continue;
				D.Subsidence.Add({ Cm(T[0]->AsNumber(), T[1]->AsNumber(), T[2]->AsNumber()), float(T[3]->AsNumber() * 100.0), T[4]->AsNumber() > 0.5 });
			}
		}
		if ((*O)->TryGetArrayField(TEXT("lime"), A))
		{
			for (const auto& V : *A)
			{
				const TArray<TSharedPtr<FJsonValue>>& T = V->AsArray();
				if (T.Num() >= 4) D.Lime.Add({ Cm(T[0]->AsNumber(), T[1]->AsNumber(), T[3]->AsNumber()), float(T[2]->AsNumber() * 100.0) });
			}
		}
	}
	if (J->TryGetArrayField(TEXT("crypts"), A))
	{
		for (const auto& V : *A)
		{
			const TSharedPtr<FJsonObject> S = V->AsObject();
			FCrypt& C = D.Crypts.AddDefaulted_GetRef();
			C.Id = GName(S, TEXT("id"));
			C.Room = GName(S, TEXT("room"));
			C.At = Cm(GNum(S, TEXT("x")), GNum(S, TEXT("y")), GNum(S, TEXT("z")));
			C.Yaw = float(GNum(S, TEXT("yaw")));
			C.Drop = float(GNum(S, TEXT("drop"), 20.0)) * 100.f;
			C.Run = float(GNum(S, TEXT("run"), 30.0)) * 100.f;
			const TArray<TSharedPtr<FJsonValue>>* H = nullptr;
			if (S->TryGetArrayField(TEXT("hole"), H) && H->Num() >= 4)
			{
				C.HoleCentre = Cm((*H)[0]->AsNumber(), (*H)[1]->AsNumber(), GNum(S, TEXT("z")));
				C.HoleHalf = FVector2D((*H)[2]->AsNumber(), (*H)[3]->AsNumber()) * 100.f;
			}
		}
	}
	if (J->TryGetArrayField(TEXT("holes"), A))
	{
		for (const auto& V : *A)
		{
			const TSharedPtr<FJsonObject> S = V->AsObject();
			D.Holes.Add({ GName(S, TEXT("id")), GName(S, TEXT("room")), Cm(GNum(S, TEXT("x")), GNum(S, TEXT("y")), GNum(S, TEXT("z"))), float(GNum(S, TEXT("drop"), 20.0)) * 100.f });
		}
	}
	if (J->TryGetObjectField(TEXT("starMound"), O)) D.StarMound = Cm(GNum(*O, TEXT("x")), GNum(*O, TEXT("y")), GNum(*O, TEXT("z")));
	if (J->TryGetObjectField(TEXT("chapelStair"), O)) D.ChapelStair = Cm(GNum(*O, TEXT("x")), GNum(*O, TEXT("y")), GNum(*O, TEXT("z")));
	if (J->TryGetObjectField(TEXT("catacombs"), O))
	{
		if ((*O)->TryGetArrayField(TEXT("rooms"), A))
		{
			for (const auto& V : *A)
			{
				const TSharedPtr<FJsonObject> S = V->AsObject();
				FRoom& R = D.Rooms.AddDefaulted_GetRef();
				R.Id = GName(S, TEXT("id"));
				R.Kind = GName(S, TEXT("kind"));
				S->TryGetStringField(TEXT("name"), R.Name);
				R.Level = int32(GNum(S, TEXT("level"), 1.0));
				R.At = Cm(GNum(S, TEXT("x")), GNum(S, TEXT("y")), GNum(S, TEXT("z")));
				R.W = float(GNum(S, TEXT("w"), 10.0)) * 100.f;
				R.L = float(GNum(S, TEXT("l"), 10.0)) * 100.f;
				R.H = float(GNum(S, TEXT("h"), 4.0)) * 100.f;
				R.Yaw = float(GNum(S, TEXT("yaw")));
				R.bHand = GBool(S, TEXT("hand"));
				R.bKit = GBool(S, TEXT("kit"));
				R.bRound = GBool(S, TEXT("round"));
				R.bCorridor = GBool(S, TEXT("corridor"));
				R.bDraught = GBool(S, TEXT("draught"));
				R.bSpiral = GBool(S, TEXT("spiral"));
				R.bRamp = GBool(S, TEXT("ramp"));
				double B = 0.0;
				R.Bottom = S->TryGetNumberField(TEXT("bottom"), B) ? float(B * 100.0) : R.At.Z;
				R.Arena = GName(S, TEXT("arena"));
				R.Shrine = GName(S, TEXT("shrine"));
				R.Sealed = GName(S, TEXT("sealed"));
				R.Mouth = GName(S, TEXT("mouth"));
				R.Hole = GName(S, TEXT("hole"));
				const TSharedPtr<FJsonValue> Off = S->TryGetField(TEXT("off"));
				if (Off.IsValid() && !Off->IsNull()) { const FVector P = PointOf(Off); R.Off = FVector2D(P.X, P.Y); }
			}
		}
		if ((*O)->TryGetArrayField(TEXT("links"), A))
		{
			for (const auto& V : *A)
			{
				const TSharedPtr<FJsonObject> S = V->AsObject();
				FLink& L = D.Links.AddDefaulted_GetRef();
				L.A = GName(S, TEXT("a"));
				L.B = GName(S, TEXT("b"));
				L.Level = int32(GNum(S, TEXT("level"), 1.0));
				L.bStub = GBool(S, TEXT("stub"));
				L.Width = float(GNum(S, TEXT("w"), 2.8)) * 100.f;
				L.Height = float(GNum(S, TEXT("h"), 3.0)) * 100.f;
				L.Points = Points(S, TEXT("pts"));
			}
		}
	}
	if (J->TryGetArrayField(TEXT("stair"), A)) for (const auto& V : *A) D.Stair.Add(PointOf(V));
	if (J->TryGetObjectField(TEXT("gravedigger"), O))
	{
		const TSharedPtr<FJsonValue> L = (*O)->TryGetField(TEXT("lair"));
		D.GravediggerLair = PointOf(L);
		D.GravediggerRoute = Points(*O, TEXT("route"));
		D.GravediggerStops = Points(*O, TEXT("stops"));
	}
	if (J->TryGetArrayField(TEXT("processions"), A)) for (const auto& V : *A) D.ProcessionRoads.Add(FName(*V->AsString()));
	if (J->TryGetObjectField(TEXT("resources"), O))
	{
		for (const auto& Pair : (*O)->Values)
		{
			TArray<FNode>& List = D.Resources.FindOrAdd(FName(*Pair.Key));
			const TArray<TSharedPtr<FJsonValue>>* R = nullptr;
			if (!Pair.Value->TryGetArray(R)) continue;
			for (const auto& RV : *R)
			{
				const TArray<TSharedPtr<FJsonValue>>& Q = RV->AsArray();
				if (Q.Num() < 3) continue;
				List.Add({ Cm(Q[0]->AsNumber(), Q[1]->AsNumber(), Q[2]->AsNumber()), Q.Num() > 3 ? FName(*Q[3]->AsString()) : NAME_None });
			}
		}
	}
	if (J->TryGetArrayField(TEXT("secrets"), A))
	{
		for (const auto& V : *A)
		{
			const TSharedPtr<FJsonObject> S = V->AsObject();
			FSecret& X = D.Secrets.AddDefaulted_GetRef();
			X.Id = GName(S, TEXT("id"));
			X.Kind = GName(S, TEXT("kind"));
			X.Room = GName(S, TEXT("room"));
			X.Needs = GName(S, TEXT("needs"));
			X.Tool = GName(S, TEXT("tool"));
			X.At = Cm(GNum(S, TEXT("x")), GNum(S, TEXT("y")), GNum(S, TEXT("z")));
			X.Seconds = float(GNum(S, TEXT("dig"), GNum(S, TEXT("hold"), 3.0)));
			X.Xp = int32(GNum(S, TEXT("xp")));
			const TSharedPtr<FJsonObject>* G = nullptr;
			if (S->TryGetObjectField(TEXT("gives"), G)) for (const auto& P : (*G)->Values) X.Gives.Add(FName(*P.Key), int32(P.Value->AsNumber()));
		}
	}
	if (J->TryGetArrayField(TEXT("lore"), A))
	{
		for (const auto& V : *A)
		{
			const TSharedPtr<FJsonObject> S = V->AsObject();
			D.Lore.Add({ GName(S, TEXT("item")), GName(S, TEXT("site")), GName(S, TEXT("room")) });
		}
	}
	D.bLoaded = true;
	UE_LOG(LogWildreach, Log, TEXT("GRAVEYARD: %d sites, %d kit, %d roads, %d tōrō, %d fields, %d trees, %d rooms, %d links"),
		D.Sites.Num(), D.Kit.Num(), D.Roads.Num(), D.Toro.Num(), D.Fields.Num(), D.Trees.Num(), D.Rooms.Num(), D.Links.Num());
	return D;
}

const FWRGraveData::FSite* FWRGraveData::FindSite(FName Id) const
{
	if (const FSite* S = Sites.FindByPredicate([Id](const FSite& Q) { return Q.Id == Id; })) return S;
	return Kit.FindByPredicate([Id](const FSite& Q) { return Q.Id == Id; });
}

const FWRGraveData::FRoom* FWRGraveData::FindRoom(FName Id) const
{
	return Rooms.FindByPredicate([Id](const FRoom& R) { return R.Id == Id; });
}

const FWRGraveData::FPath* FWRGraveData::FindRoad(FName Id) const
{
	return Roads.FindByPredicate([Id](const FPath& R) { return R.Id == Id; });
}

TCHAR FWRGraveData::ZoneAt(const FVector& Pos) const
{
	const FWRLandDef* L = WRGraveTypesImpl::GLand();
	return L ? L->GridLetter(L->ToMap(Pos)) : TCHAR(0);
}

// ================================================================================================ the land's calls

FName WRGrave::Biome() { return WRGraveTypesImpl::GBiome; }

bool WRGrave::Live()
{
	return WRBiome::IsLive(WRGraveTypesImpl::GBiome);
}

bool WRGrave::Contains(const FVector& Pos)
{
	if (!Live()) return false;
	const FWRLandDef* L = WRLand::At(Pos);
	if (!L || L->Biome != WRGraveTypesImpl::GBiome) return false;
	const TCHAR Z = FWRGraveData::Get().ZoneAt(Pos);
	return Z != 0 && Z != TEXT('~');
}

const FWRGraveData::FRoom* WRGrave::RoomAt(const FVector& Pos)
{
	const FWRGraveData& D = FWRGraveData::Get();
	if (!D.bLoaded || Pos.X < D.FrameMin.X || Pos.Y < D.FrameMin.Y || Pos.X > D.FrameMax.X || Pos.Y > D.FrameMax.Y) return nullptr;
	for (const FWRGraveData::FRoom& R : D.Rooms) if (R.Contains(Pos, 50.f)) return &R;
	return nullptr;
}

bool WRGrave::InCatacombs(const FVector& Pos)
{
	if (!Live()) return false;
	const FWRGraveData& D = FWRGraveData::Get();
	if (!D.bLoaded || Pos.X < D.FrameMin.X || Pos.Y < D.FrameMin.Y || Pos.X > D.FrameMax.X || Pos.Y > D.FrameMax.Y) return false;
	// Fast out: anything well above the ground here is out in the open.
	float Ground = 0.f;
	if (WRLand::GroundUnder(Pos, Ground) && Pos.Z > Ground - 150.f) return false;
	if (RoomAt(Pos)) return true;
	for (const FWRGraveData::FLink& L : D.Links)
	{
		for (int32 i = 0; i + 1 < L.Points.Num(); ++i)
		{
			const FVector& A = L.Points[i];
			const FVector& B = L.Points[i + 1];
			if (FMath::Abs(Pos.Z - (A.Z + B.Z) * 0.5f - L.Height * 0.5f) > L.Height + 300.f) continue;
			if (FMath::PointDistToSegment(FVector(Pos.X, Pos.Y, 0.f), FVector(A.X, A.Y, 0.f), FVector(B.X, B.Y, 0.f)) < L.Width * 0.5f + 80.f) return true;
		}
	}
	for (const FWRGraveData::FCrypt& C : D.Crypts)
	{
		const FVector Dir = FRotator(0.f, C.Yaw, 0.f).Vector();
		const FVector Foot = C.At + Dir * (C.Run + 400.f) - FVector(0.f, 0.f, C.Drop);
		if (FMath::PointDistToSegment(Pos, C.At, Foot) < 350.f) return true;
	}
	for (const FWRGraveData::FHole& H : D.Holes)
	{
		if (FVector::Dist2D(Pos, H.At) < 350.f && Pos.Z < H.At.Z && Pos.Z > H.At.Z - H.Drop - 200.f) return true;
	}
	return false;
}

int32 WRGrave::LevelAt(const FVector& Pos)
{
	if (const FWRGraveData::FRoom* R = RoomAt(Pos)) return R->Level;
	if (!InCatacombs(Pos)) return 0;
	const FWRGraveData& D = FWRGraveData::Get();
	int32 Best = 1;
	float BestD = TNumericLimits<float>::Max();
	for (const FWRGraveData::FLink& L : D.Links)
	{
		for (const FVector& P : L.Points)
		{
			const float Dd = float(FVector::DistSquared(P, Pos));
			if (Dd < BestD) { BestD = Dd; Best = L.Level; }
		}
	}
	return Best;
}

bool WRGrave::InDraught(const FVector& Pos)
{
	const FWRGraveData::FRoom* R = RoomAt(Pos);
	return R && R->bDraught;
}

TCHAR WRGrave::ZoneAt(const FVector& Pos)
{
	if (!Live()) return 0;
	if (InCatacombs(Pos)) return TEXT('c');
	if (!Contains(Pos)) return 0;
	return FWRGraveData::Get().ZoneAt(Pos);
}
