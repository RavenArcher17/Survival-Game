// The Sky islands' content (cooks/build-biome-sky.md 2.1-2.10): Data/sky.json parsed once, the islands' shape, and the
// calls the Sky's parts share (where, which zone, the wind, the updrafts, the rotors and downbursts). See WRSkyIsleTypes.h.

#include "WRSkyIsleTypes.h"
#include "Wildreach.h"
#include "WRLand.h"
#include "WRFarm.h"
#include "WRSurvival.h"
#include "WRTuning.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"

namespace WRSkyIsleImpl
{
	const FName SBiome(TEXT("sky_islands"));
	TArray<FVector> GDrift;

	double SNum(const TSharedPtr<FJsonObject>& O, const TCHAR* K, double Def = 0.0)
	{
		double V = Def;
		if (O.IsValid()) O->TryGetNumberField(K, V);
		return V;
	}
	bool SBool(const TSharedPtr<FJsonObject>& O, const TCHAR* K)
	{
		bool B = false;
		if (O.IsValid() && !O->TryGetBoolField(K, B))
		{
			double N = 0.0;
			if (O->TryGetNumberField(K, N)) B = N != 0.0;
		}
		return B;
	}
	FName SName(const TSharedPtr<FJsonObject>& O, const TCHAR* K)
	{
		FString S;
		if (O.IsValid() && O->TryGetStringField(K, S) && !S.IsEmpty()) return FName(*S);
		return NAME_None;
	}
	FVector Cm(double X, double Y, double Z) { return FVector(X * 100.0, Y * 100.0, Z * 100.0); }
	/** [x, y, z...] -> world cm (z optional). */
	FVector PointOf(const TSharedPtr<FJsonValue>& V)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (!V.IsValid() || !V->TryGetArray(A) || A->Num() < 2) return FVector::ZeroVector;
		const double Z = A->Num() > 2 && (*A)[2]->Type == EJson::Number ? (*A)[2]->AsNumber() : 0.0;
		return Cm((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), Z);
	}
	FVector PointField(const TSharedPtr<FJsonObject>& O, const TCHAR* K)
	{
		return O.IsValid() && O->HasField(K) ? PointOf(O->TryGetField(K)) : FVector::ZeroVector;
	}
	TMap<FName, int32> Gives(const TSharedPtr<FJsonObject>& O)
	{
		TMap<FName, int32> Out;
		const TSharedPtr<FJsonObject>* G = nullptr;
		if (O.IsValid() && O->TryGetObjectField(TEXT("gives"), G))
		{
			for (const auto& Pair : (*G)->Values) Out.Add(FName(*Pair.Key), int32(Pair.Value->AsNumber()));
		}
		return Out;
	}
	TCHAR ZoneLetter(const FString& Z) { return Z.Len() >= 2 && Z[0] == 'W' ? Z[1] : (Z.Len() ? Z[0] : TCHAR('1')); }
	/** A seeded wobble in -1..1 round an outline (three harmonics). */
	float Wobble(int32 Seed, float A)
	{
		const float S1 = float(Seed % 97) * 0.37f, S2 = float(Seed % 61) * 0.71f, S3 = float(Seed % 43) * 1.13f;
		return 0.5f * FMath::Sin(3.f * A + S1) + 0.3f * FMath::Sin(5.f * A + S2) + 0.2f * FMath::Sin(7.f * A + S3);
	}
	FWRSkyIsleData::FSite Site(const TSharedPtr<FJsonObject>& O, const FWRSkyIsleData& D)
	{
		FWRSkyIsleData::FSite S;
		S.Id = SName(O, TEXT("id"));
		S.Kind = SName(O, TEXT("kind"));
		O->TryGetStringField(TEXT("name"), S.Name);
		S.Island = SName(O, TEXT("island"));
		S.Shrine = SName(O, TEXT("shrine"));
		S.Yaw = float(SNum(O, TEXT("yaw")));
		S.Variant = int32(SNum(O, TEXT("variant")));
		S.Cave = int32(SNum(O, TEXT("cave")));
		S.bLandmark = SBool(O, TEXT("landmark"));
		S.bHand = SBool(O, TEXT("hand"));
		S.bCamp = SBool(O, TEXT("camp"));
		S.bFill = SBool(O, TEXT("fill"));
		S.bGround = SBool(O, TEXT("ground"));
		S.bUnder = SBool(O, TEXT("under"));
		S.bSecret = SBool(O, TEXT("secret"));
		S.At = Cm(SNum(O, TEXT("x")), SNum(O, TEXT("y")), SNum(O, TEXT("z")));
		if (const FWRSkyIsleData::FIsland* I = D.FindIsland(S.Island))
		{
			S.At.Z = I->TopZ(S.At);
		}
		else if (!O->HasField(TEXT("z")))
		{
			float Z = 0.f;
			if (WRLand::GroundUnder(FVector(S.At.X, S.At.Y, 2.0e5f), Z)) S.At.Z = Z;
		}
		if (O->HasField(TEXT("to"))) S.To = PointField(O, TEXT("to"));
		S.ToIsland = SName(O, TEXT("toIsland"));
		if (const FWRSkyIsleData::FIsland* T = D.FindIsland(S.ToIsland)) S.To.Z = T->TopZ(S.To);
		return S;
	}
	float Smooth(float E0, float E1, float X)
	{
		const float T = FMath::Clamp((X - E0) / (E1 - E0), 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	}
}

// ================================================================================================ an island's shape

FVector WRSkyIsles::DriftOf(int32 IslandIndex)
{
	return WRSkyIsleImpl::GDrift.IsValidIndex(IslandIndex) ? WRSkyIsleImpl::GDrift[IslandIndex] : FVector::ZeroVector;
}

void WRSkyIsles::SetDrift(int32 IslandIndex, const FVector& Offset)
{
	if (IslandIndex < 0) return;
	if (WRSkyIsleImpl::GDrift.Num() <= IslandIndex) WRSkyIsleImpl::GDrift.SetNumZeroed(IslandIndex + 1);
	WRSkyIsleImpl::GDrift[IslandIndex] = Offset;
}

FVector FWRSkyIsleData::FIsland::Now() const
{
	return Centre + WRSkyIsles::DriftOf(Index);
}

FVector2D FWRSkyIsleData::FIsland::Local(const FVector& P) const
{
	const FVector C = Now();
	const FVector2D D(P.X - C.X, P.Y - C.Y);
	const float Cs = FMath::Cos(FMath::DegreesToRadians(Yaw)), Sn = FMath::Sin(FMath::DegreesToRadians(Yaw));
	return FVector2D(D.X * Cs + D.Y * Sn, -D.X * Sn + D.Y * Cs);
}

FVector2D FWRSkyIsleData::FIsland::World2D(const FVector2D& L) const
{
	const FVector C = Now();
	const float Cs = FMath::Cos(FMath::DegreesToRadians(Yaw)), Sn = FMath::Sin(FMath::DegreesToRadians(Yaw));
	return FVector2D(C.X + L.X * Cs - L.Y * Sn, C.Y + L.X * Sn + L.Y * Cs);
}

float FWRSkyIsleData::FIsland::EdgeRadius(float A) const
{
	const float Ca = FMath::Cos(A) / R(), Sa = FMath::Sin(A) / RMin();
	const float Ellipse = 1.f / FMath::Max(1e-6f, FMath::Sqrt(Ca * Ca + Sa * Sa));
	return Ellipse * (0.98f + 0.06f * WRSkyIsleImpl::Wobble(Seed, A));
}

float FWRSkyIsleData::FIsland::Rho(const FVector& P) const
{
	const FVector2D L = Local(P);
	const float D = L.Size();
	if (D < 1.f) return 0.f;
	return D / EdgeRadius(FMath::Atan2(L.Y, L.X));
}

float FWRSkyIsleData::FIsland::Dome() const
{
	if (bSpire) return 900.f;
	return FMath::Clamp(Size * 0.03f, 300.f, 1200.f);
}

float FWRSkyIsleData::FIsland::RootDepth() const
{
	// Floating-island roots taper 1.5-3x the radius under the rim (the Hallelujah pillars' proportions), seeded.
	const float K = 1.5f + 1.5f * FMath::Frac(float(Seed) * 0.618034f);
	return FMath::Clamp(RMin() * K + Beard * 0.5f, 2500.f, 42000.f);
}

float FWRSkyIsleData::FIsland::WaterZ() const
{
	if (bLagoon) return Centre.Z + WRSkyIsles::DriftOf(Index).Z + 100.f;
	if (bPool) return Centre.Z + WRSkyIsles::DriftOf(Index).Z + Dome() * (1.f - 0.25f * 0.25f) - 30.f;
	return -1.0e9f;
}

float FWRSkyIsleData::FIsland::TopZ(const FVector& P) const
{
	using WRSkyIsleImpl::Smooth;
	const FVector C = Now();
	const float Rh = FMath::Min(Rho(P), 1.05f);
	const float D = Dome();
	float H = D * (1.f - Rh * Rh);
	// The lip rolls over the last 6% (turf overhanging the rim, the Faroe cliffs' way).
	H -= 60.f * Smooth(0.94f, 1.f, Rh);
	// Terraces (the Kitewrights' farm steps): the dome cut into flats with 35-degree risers a walk can take.
	if (Terraces > 0)
	{
		const float Step = D / float(Terraces + 1);
		const float Lvl = H / Step;
		const float Base = FMath::FloorToFloat(Lvl);
		const float Fr = Lvl - Base;
		H = (Base + Smooth(0.72f, 1.f, Fr)) * Step;
	}
	// The Kitefield's hill, up which the death-kites stand.
	if (bHill)
	{
		const FVector2D L = Local(P);
		const float Dh = FVector2D::Distance(L, FVector2D(R() * 0.2f, 0.f));
		H += 2500.f * FMath::Exp(-FMath::Square(Dh / (RMin() * 0.55f)));
	}
	// A rain lagoon: a basin 12 m deep inside a rim of turf (Gulper's Bight).
	if (bLagoon)
	{
		const float W = WaterZ() - C.Z;
		const float Basin = W - 1200.f * Smooth(0.78f, 0.45f, Rh);
		H = FMath::Lerp(H, FMath::Min(H, Basin), Smooth(0.86f, 0.74f, Rh));
	}
	// A still pool in the middle (2.5 m deep).
	if (bPool)
	{
		const float W = WaterZ() - C.Z;
		H = FMath::Min(H, FMath::Lerp(H, W - 250.f, Smooth(0.27f, 0.12f, Rh)));
	}
	// A tilted island (the Drowned Garden, its keel half spent): the whole top leans along its long axis.
	if (FMath::Abs(Tilt) > 0.1f)
	{
		H += Local(P).X * FMath::Tan(FMath::DegreesToRadians(Tilt));
	}
	return C.Z + H;
}

// ================================================================================================ the file

const FWRSkyIsleData& FWRSkyIsleData::Get()
{
	using namespace WRSkyIsleImpl;
	static FWRSkyIsleData D;
	static bool bTried = false;
	if (bTried) return D;
	bTried = true;
	const TSharedPtr<FJsonObject> J = FWRTuning::LoadDataFile(TEXT("sky.json"));
	if (!J.IsValid())
	{
		UE_LOG(LogWildreach, Error, TEXT("SKY: Data/sky.json is missing (node Tools/terrain/sky.mjs)"));
		return D;
	}
	const TSharedPtr<FJsonObject>* O = nullptr;
	if (J->TryGetObjectField(TEXT("frame"), O))
	{
		D.FrameMin = FVector2D(SNum(*O, TEXT("x0")), SNum(*O, TEXT("y0"))) * 100.f;
		D.FrameMax = FVector2D(SNum(*O, TEXT("x1")), SNum(*O, TEXT("y1"))) * 100.f;
	}
	if (J->TryGetObjectField(TEXT("flue"), O))
	{
		D.FlueAt = FVector2D(SNum(*O, TEXT("x")), SNum(*O, TEXT("y"))) * 100.f;
		D.FlueThroat = float(SNum(*O, TEXT("throat"), 125.0)) * 100.f;
		D.FlueRim = float(SNum(*O, TEXT("rim"), 700.0)) * 100.f;
		D.FlueFloor = float(SNum(*O, TEXT("floor"), -40.0)) * 100.f;
	}
	const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
	if (J->TryGetArrayField(TEXT("islands"), A))
	{
		for (const auto& V : *A)
		{
			const TSharedPtr<FJsonObject> S = V->AsObject();
			FIsland& I = D.Islands.AddDefaulted_GetRef();
			I.Index = D.Islands.Num() - 1;
			I.Id = SName(S, TEXT("id"));
			I.Kind = SName(S, TEXT("kind"));
			S->TryGetStringField(TEXT("name"), I.Name);
			FString Z;
			S->TryGetStringField(TEXT("zone"), Z);
			I.Zone = ZoneLetter(Z);
			I.Centre = Cm(SNum(S, TEXT("x")), SNum(S, TEXT("y")), SNum(S, TEXT("z")));
			I.Size = float(SNum(S, TEXT("size"), 100.0)) * 100.f;
			I.Aspect = float(SNum(S, TEXT("aspect"), 1.2));
			I.Yaw = float(SNum(S, TEXT("yaw")));
			I.Seed = int32(SNum(S, TEXT("seed"), 1.0));
			I.bClaim = SBool(S, TEXT("claim"));
			I.Terraces = int32(SNum(S, TEXT("terraces")));
			I.Falls = int32(SNum(S, TEXT("falls")));
			I.Caves = int32(SNum(S, TEXT("caves")));
			I.Beard = float(SNum(S, TEXT("beard"))) * 100.f;
			I.Tilt = float(SNum(S, TEXT("tilt")));
			I.bPool = SBool(S, TEXT("pool"));
			I.bLagoon = SBool(S, TEXT("lagoon"));
			I.bHill = SBool(S, TEXT("hill"));
			I.bSpore = SBool(S, TEXT("spore"));
			I.bSpire = SBool(S, TEXT("spire"));
			I.bOverhang = SBool(S, TEXT("overhang"));
			I.bLedges = SBool(S, TEXT("ledges"));
		}
	}
	if (J->TryGetObjectField(TEXT("ring"), O))
	{
		D.RingAt = Cm(SNum(*O, TEXT("x")), SNum(*O, TEXT("y")), SNum(*O, TEXT("z")));
		D.RingRadius = float(SNum(*O, TEXT("radius"), 28.0)) * 100.f;
		const TArray<TSharedPtr<FJsonValue>>* P = nullptr;
		if ((*O)->TryGetArrayField(TEXT("platforms"), P))
		{
			for (const auto& V : *P)
			{
				FPlatform& Pl = D.Platforms.AddDefaulted_GetRef();
				Pl.Angle = float(SNum(V->AsObject(), TEXT("a")));
				Pl.Size = float(SNum(V->AsObject(), TEXT("size"), 16.0)) * 100.f;
			}
		}
	}
	if (J->TryGetObjectField(TEXT("undercloud"), O))
	{
		D.UnderAt = Cm(SNum(*O, TEXT("x")), SNum(*O, TEXT("y")), SNum(*O, TEXT("bottom")));
		D.UnderRadius = float(SNum(*O, TEXT("r"), 500.0)) * 100.f;
		D.UnderBottom = float(SNum(*O, TEXT("bottom"), 500.0)) * 100.f;
		D.UnderTop = float(SNum(*O, TEXT("top"), 600.0)) * 100.f;
		const FVector G = PointField(*O, TEXT("gap"));
		D.UnderGap = FVector2D(G.X, G.Y);
	}
	if (J->TryGetArrayField(TEXT("camps"), A)) for (const auto& V : *A) D.Camps.Add(PointOf(V));
	if (J->TryGetArrayField(TEXT("road"), A)) for (const auto& V : *A) D.Road.Add(PointOf(V));
	if (J->TryGetArrayField(TEXT("sites"), A)) for (const auto& V : *A) D.Sites.Add(Site(V->AsObject(), D));
	if (J->TryGetArrayField(TEXT("kit"), A)) for (const auto& V : *A) D.Kit.Add(Site(V->AsObject(), D));
	if (FSite* L1 = const_cast<FSite*>(D.FindSite(TEXT("L1")))) L1->At = D.RingAt;
	if (J->TryGetArrayField(TEXT("lifts"), A))
	{
		for (const auto& V : *A)
		{
			const TSharedPtr<FJsonObject> S = V->AsObject();
			FLift& L = D.Lifts.AddDefaulted_GetRef();
			L.Id = SName(S, TEXT("id"));
			L.Camp = SName(S, TEXT("camp"));
			L.Island = SName(S, TEXT("island"));
			L.Foot = PointField(S, TEXT("foot"));
			L.TopZ = float(SNum(S, TEXT("top"))) * 100.f;
		}
	}
	if (J->TryGetArrayField(TEXT("bridges"), A))
	{
		for (const auto& V : *A)
		{
			const TSharedPtr<FJsonObject> S = V->AsObject();
			FBridge& B = D.Bridges.AddDefaulted_GetRef();
			B.Id = SName(S, TEXT("id"));
			B.Kind = SName(S, TEXT("kind"));
			B.From = SName(S, TEXT("from"));
			B.To = SName(S, TEXT("to"));
			B.A = PointField(S, TEXT("a"));
			B.B = PointField(S, TEXT("b"));
			B.Width = float(SNum(S, TEXT("width"), 4.0)) * 100.f;
			B.Reach = float(SNum(S, TEXT("reach"), 1.0));
			B.Hang = float(SNum(S, TEXT("hang"))) * 100.f;
			const TArray<TSharedPtr<FJsonValue>>* W = nullptr;
			if (S->TryGetArrayField(TEXT("weak"), W)) for (const auto& X : *W) B.Weak.Add(float(X->AsNumber()));
			// The deck's ends stand on their islands' tops (the generator wrote the rim altitude).
			if (const FIsland* I = D.FindIsland(B.From)) B.A.Z = FMath::Max(B.A.Z, I->TopZ(B.A));
			if (const FIsland* I = D.FindIsland(B.To); I && B.Kind != TEXT("ladder")) B.B.Z = FMath::Max(B.B.Z, I->TopZ(B.B));
		}
	}
	if (J->TryGetArrayField(TEXT("poles"), A))
	{
		for (const auto& V : *A)
		{
			const TArray<TSharedPtr<FJsonValue>>& P = V->AsArray();
			if (P.Num() < 4) continue;
			FPole& Pl = D.Poles.AddDefaulted_GetRef();
			Pl.At = Cm(P[0]->AsNumber(), P[1]->AsNumber(), P[2]->AsNumber());
			Pl.Yaw = float(P[3]->AsNumber());
		}
	}
	if (J->TryGetArrayField(TEXT("updrafts"), A))
	{
		for (const auto& V : *A)
		{
			const TSharedPtr<FJsonObject> S = V->AsObject();
			FUpdraft& U = D.Updrafts.AddDefaulted_GetRef();
			U.Kind = SName(S, TEXT("kind"));
			U.Island = SName(S, TEXT("island"));
			U.At = FVector2D(SNum(S, TEXT("x")), SNum(S, TEXT("y"))) * 100.f;
			U.Radius = float(SNum(S, TEXT("r"), 22.0)) * 100.f;
			U.Lift = float(SNum(S, TEXT("lift"), 10.0)) * 100.f;
			U.Base = float(SNum(S, TEXT("base"))) * 100.f;
			U.Top = float(SNum(S, TEXT("top"))) * 100.f;
			const FVector N = PointField(S, TEXT("normal"));
			U.Normal = FVector2D(N.X, N.Y) / 100.f;
		}
	}
	if (J->TryGetArrayField(TEXT("gustLanes"), A))
	{
		for (const auto& V : *A)
		{
			const TSharedPtr<FJsonObject> S = V->AsObject();
			FLane& L = D.Lanes.AddDefaulted_GetRef();
			L.Id = SName(S, TEXT("id"));
			L.Island = SName(S, TEXT("island"));
			const TArray<TSharedPtr<FJsonValue>>* P = nullptr;
			if (S->TryGetArrayField(TEXT("points"), P)) for (const auto& X : *P) L.Points.Add(PointOf(X));
			const FVector Dr = PointField(S, TEXT("drop"));
			L.Drop = FVector2D(Dr.X, Dr.Y).GetSafeNormal();
			L.Width = float(SNum(S, TEXT("width"), 8.0)) * 100.f;
			L.bTeacher = SBool(S, TEXT("teacher"));
			// Lanes along an island's lip ride its top.
			if (const FIsland* I = D.FindIsland(L.Island)) for (FVector& X : L.Points) X.Z = I->TopZ(X);
		}
	}
	if (J->TryGetArrayField(TEXT("rotors"), A))
	{
		for (const auto& V : *A)
		{
			const TSharedPtr<FJsonObject> S = V->AsObject();
			FRotor& R = D.Rotors.AddDefaulted_GetRef();
			R.Island = SName(S, TEXT("island"));
			R.At = Cm(SNum(S, TEXT("x")), SNum(S, TEXT("y")), SNum(S, TEXT("z")));
			R.Radius = float(SNum(S, TEXT("r"), 40.0)) * 100.f;
			R.Height = float(SNum(S, TEXT("h"), 70.0)) * 100.f;
		}
	}
	if (J->TryGetArrayField(TEXT("stones"), A))
	{
		int32 Line = 0;
		for (const auto& V : *A)
		{
			const TArray<TSharedPtr<FJsonValue>>* P = nullptr;
			if (V->AsObject()->TryGetArrayField(TEXT("stones"), P))
			{
				for (const auto& X : *P)
				{
					const TArray<TSharedPtr<FJsonValue>>& Q = X->AsArray();
					if (Q.Num() < 4) continue;
					FStone& St = D.Stones.AddDefaulted_GetRef();
					St.At = Cm(Q[0]->AsNumber(), Q[1]->AsNumber(), Q[2]->AsNumber());
					St.Size = float(Q[3]->AsNumber()) * 100.f;
					St.Line = Line;
				}
			}
			++Line;
		}
	}
	if (J->TryGetArrayField(TEXT("downbursts"), A))
	{
		for (const auto& V : *A)
		{
			const TSharedPtr<FJsonObject> S = V->AsObject();
			FDownburst& B = D.Downbursts.AddDefaulted_GetRef();
			B.At = PointField(S, TEXT("at"));
			B.Radius = float(SNum(S, TEXT("r"), 15.0)) * 100.f;
			B.From = SName(S, TEXT("from"));
			B.To = SName(S, TEXT("to"));
		}
	}
	if (J->TryGetArrayField(TEXT("falls"), A))
	{
		for (const auto& V : *A)
		{
			const TSharedPtr<FJsonObject> S = V->AsObject();
			FFall& F = D.Falls.AddDefaulted_GetRef();
			F.Island = SName(S, TEXT("island"));
			F.At = Cm(SNum(S, TEXT("x")), SNum(S, TEXT("y")), SNum(S, TEXT("z")));
			F.Width = float(SNum(S, TEXT("width"), 6.0)) * 100.f;
			F.Drop = float(SNum(S, TEXT("drop"), 150.0)) * 100.f;
			F.Yaw = float(SNum(S, TEXT("yaw")));
		}
	}
	if (J->TryGetObjectField(TEXT("resources"), O))
	{
		for (const auto& Pair : (*O)->Values)
		{
			TArray<FNode>& Out = D.Resources.FindOrAdd(FName(*Pair.Key));
			const TArray<TSharedPtr<FJsonValue>>* P = nullptr;
			if (!Pair.Value->TryGetArray(P)) continue;
			for (const auto& X : *P)
			{
				const TArray<TSharedPtr<FJsonValue>>& Q = X->AsArray();
				if (Q.Num() < 2) continue;
				FNode& N = Out.AddDefaulted_GetRef();
				N.At = Cm(Q[0]->AsNumber(), Q[1]->AsNumber(), 0.0);
				if (Q.Num() > 2)
				{
					if (Q[2]->Type == EJson::String) N.Island = FName(*Q[2]->AsString());
					else N.At.Z = Q[2]->AsNumber() * 100.0;
				}
				if (Q.Num() > 3) N.Cave = int32(Q[3]->AsNumber());
				if (Q.Num() > 4) N.Slot = int32(Q[4]->AsNumber());
				if (const FIsland* I = D.FindIsland(N.Island)) N.At.Z = I->TopZ(N.At);
			}
		}
	}
	if (J->TryGetObjectField(TEXT("graph"), O))
	{
		const TArray<TSharedPtr<FJsonValue>>* P = nullptr;
		if ((*O)->TryGetArrayField(TEXT("nodes"), P))
		{
			for (const auto& X : *P)
			{
				const TArray<TSharedPtr<FJsonValue>>& Q = X->AsArray();
				if (Q.Num() < 5) continue;
				FGraphNode& N = D.Graph.AddDefaulted_GetRef();
				N.At = Cm(Q[0]->AsNumber(), Q[1]->AsNumber(), Q[2]->AsNumber());
				N.Island = FName(*Q[3]->AsString());
				N.Kind = FName(*Q[4]->AsString());
			}
		}
		D.GraphLinks.SetNum(D.Graph.Num());
		if ((*O)->TryGetArrayField(TEXT("edges"), P))
		{
			for (const auto& X : *P)
			{
				const TArray<TSharedPtr<FJsonValue>>& Q = X->AsArray();
				if (Q.Num() < 2) continue;
				const int32 Ia = int32(Q[0]->AsNumber()), Ib = int32(Q[1]->AsNumber());
				if (!D.GraphLinks.IsValidIndex(Ia) || !D.GraphLinks.IsValidIndex(Ib)) continue;
				D.GraphLinks[Ia].Add(Ib);
				D.GraphLinks[Ib].Add(Ia);
			}
		}
	}
	if (J->TryGetObjectField(TEXT("arenas"), O))
	{
		for (const auto& Pair : (*O)->Values)
		{
			const TSharedPtr<FJsonObject> S = Pair.Value->AsObject();
			FArena& Ar = D.Arenas.Add(FName(*Pair.Key));
			Ar.Id = FName(*Pair.Key);
			Ar.At = Cm(SNum(S, TEXT("x")), SNum(S, TEXT("y")), SNum(S, TEXT("z")));
			Ar.Radius = float(SNum(S, TEXT("radius"), 65.0)) * 100.f;
			Ar.Runback = PointField(S, TEXT("runback"));
			Ar.Altar = PointField(S, TEXT("altar"));
			Ar.Door = PointField(S, TEXT("door"));
			Ar.FallTo = PointField(S, TEXT("fallTo"));
			for (FVector* P : { &Ar.Runback, &Ar.Altar, &Ar.Door, &Ar.FallTo })
			{
				if (P->IsZero()) continue;
				if (const FIsland* I = D.IslandAt(*P, 500.f, 20000.f, 20000.f)) P->Z = I->TopZ(*P);
			}
		}
	}
	if (J->TryGetObjectField(TEXT("kyrra"), O))
	{
		const TArray<TSharedPtr<FJsonValue>>* P = nullptr;
		if ((*O)->TryGetArrayField(TEXT("route"), P)) for (const auto& X : *P) D.KyrraRoute.Add(PointOf(X));
		if ((*O)->TryGetArrayField(TEXT("lands"), P)) for (const auto& X : *P) D.KyrraLands.Add(FName(*X->AsString()));
	}
	if (J->TryGetObjectField(TEXT("event"), O))
	{
		const TArray<TSharedPtr<FJsonValue>>* P = nullptr;
		if ((*O)->TryGetArrayField(TEXT("drift"), P))
		{
			for (const auto& X : *P)
			{
				FDrift& Dr = D.Drift.AddDefaulted_GetRef();
				Dr.Island = SName(X->AsObject(), TEXT("island"));
				Dr.To = PointField(X->AsObject(), TEXT("to"));
			}
		}
		if ((*O)->TryGetArrayField(TEXT("cloudBridges"), P))
		{
			for (const auto& X : *P)
			{
				const TSharedPtr<FJsonObject> S = X->AsObject();
				FCloudBridge& B = D.CloudBridges.AddDefaulted_GetRef();
				B.From = SName(S, TEXT("from"));
				B.To = SName(S, TEXT("to"));
				B.A = PointField(S, TEXT("a"));
				B.B = PointField(S, TEXT("b"));
				B.Width = float(SNum(S, TEXT("width"), 4.0)) * 100.f;
				if (const FIsland* I = D.FindIsland(B.From)) B.A.Z = I->TopZ(B.A);
				if (const FIsland* I = D.FindIsland(B.To)) B.B.Z = I->TopZ(B.B);
			}
		}
		if ((*O)->TryGetArrayField(TEXT("shardFalls"), P))
		{
			for (const auto& X : *P)
			{
				const TArray<TSharedPtr<FJsonValue>>& Q = X->AsArray();
				if (Q.Num() < 3) continue;
				FNode& N = D.ShardFalls.AddDefaulted_GetRef();
				N.At = Cm(Q[0]->AsNumber(), Q[1]->AsNumber(), 0.0);
				N.Island = FName(*Q[2]->AsString());
				if (const FIsland* I = D.FindIsland(N.Island)) N.At.Z = I->TopZ(N.At);
			}
		}
	}
	if (J->TryGetArrayField(TEXT("secrets"), A))
	{
		for (const auto& V : *A)
		{
			const TSharedPtr<FJsonObject> S = V->AsObject();
			FSecret& Se = D.Secrets.AddDefaulted_GetRef();
			Se.Id = SName(S, TEXT("id"));
			Se.Kind = SName(S, TEXT("kind"));
			Se.Island = SName(S, TEXT("island"));
			Se.Bridge = SName(S, TEXT("bridge"));
			Se.At = PointField(S, TEXT("at"));
			Se.Under = float(SNum(S, TEXT("under"))) * 100.f;
			Se.Depth = float(SNum(S, TEXT("depth"))) * 100.f;
			Se.Gives = Gives(S);
		}
	}
	if (J->TryGetArrayField(TEXT("lore"), A))
	{
		for (const auto& V : *A)
		{
			const TArray<TSharedPtr<FJsonValue>>& Q = V->AsArray();
			if (Q.Num() < 2) continue;
			D.Lore.Add({ FName(*Q[0]->AsString()), FName(*Q[1]->AsString()) });
		}
	}
	D.bLoaded = D.Islands.Num() > 0;
	UE_LOG(LogWildreach, Log, TEXT("SKY: %d islands, %d sites, %d kit, %d bridges, %d updrafts, %d lanes"), D.Islands.Num(), D.Sites.Num(), D.Kit.Num(), D.Bridges.Num(), D.Updrafts.Num(), D.Lanes.Num());
	return D;
}

const FWRSkyIsleData::FIsland* FWRSkyIsleData::FindIsland(FName Id) const
{
	if (Id.IsNone()) return nullptr;
	return Islands.FindByPredicate([Id](const FIsland& I) { return I.Id == Id; });
}

const FWRSkyIsleData::FSite* FWRSkyIsleData::FindSite(FName Id) const
{
	if (const FSite* S = Sites.FindByPredicate([Id](const FSite& X) { return X.Id == Id; })) return S;
	return Kit.FindByPredicate([Id](const FSite& X) { return X.Id == Id; });
}

const FWRSkyIsleData::FBridge* FWRSkyIsleData::FindBridge(FName Id) const
{
	return Bridges.FindByPredicate([Id](const FBridge& B) { return B.Id == Id; });
}

const FWRSkyIsleData::FArena* FWRSkyIsleData::FindArena(FName Id) const
{
	return Arenas.Find(Id);
}

const FWRSkyIsleData::FIsland* FWRSkyIsleData::IslandAt(const FVector& P, float Grow, float Above, float Below) const
{
	for (const FIsland& I : Islands)
	{
		const FVector C = I.Now();
		if (FMath::Abs(P.X - C.X) > I.R() * 1.1f + Grow || FMath::Abs(P.Y - C.Y) > I.R() * 1.1f + Grow) continue;
		const FVector2D L = I.Local(P);
		const float Edge = I.EdgeRadius(FMath::Atan2(L.Y, L.X)) + Grow;
		if (L.Size() > Edge) continue;
		const float Top = I.TopZ(P);
		if (P.Z < Top - Below || P.Z > Top + Above) continue;
		return &I;
	}
	return nullptr;
}

const FWRSkyIsleData::FIsland* FWRSkyIsleData::NearestIsland(const FVector& P, float* OutGap) const
{
	const FIsland* Best = nullptr;
	float BestGap = TNumericLimits<float>::Max();
	for (const FIsland& I : Islands)
	{
		const FVector2D L = I.Local(P);
		const float Edge = I.EdgeRadius(FMath::Atan2(L.Y, L.X));
		const float Gap2D = FMath::Max(0.f, L.Size() - Edge);
		const float Gz = FMath::Max(0.f, FMath::Abs(P.Z - I.Now().Z) - I.Dome());
		const float Gap = FMath::Sqrt(Gap2D * Gap2D + Gz * Gz);
		if (Gap < BestGap) { BestGap = Gap; Best = &I; }
	}
	if (OutGap) *OutGap = BestGap;
	return Best;
}

FVector FWRSkyIsleData::OnIsland(const FIsland& I, float U, float V) const
{
	const FVector2D W = I.World2D(FVector2D(U * I.R(), V * I.RMin()));
	const FVector P(W.X, W.Y, 0.f);
	return FVector(W.X, W.Y, I.TopZ(P));
}

// ================================================================================================ where

FName WRSkyIsles::Biome() { return WRSkyIsleImpl::SBiome; }

bool WRSkyIsles::Live()
{
	static int32 Cached = -1;
	if (Cached < 0) Cached = WRBiome::IsLive(WRSkyIsleImpl::SBiome) && FWRSkyIsleData::Get().bLoaded ? 1 : 0;
	return Cached == 1;
}

bool WRSkyIsles::Contains(const FVector& Pos)
{
	if (!Live()) return false;
	const FWRSkyIsleData& D = FWRSkyIsleData::Get();
	return Pos.X >= D.FrameMin.X && Pos.X <= D.FrameMax.X && Pos.Y >= D.FrameMin.Y && Pos.Y <= D.FrameMax.Y;
}

bool WRSkyIsles::InUndercloud(const FVector& Pos)
{
	if (!Live()) return false;
	const FWRSkyIsleData& D = FWRSkyIsleData::Get();
	if (Pos.Z < D.UnderBottom - 4000.f || Pos.Z > D.UnderTop) return false;
	return FVector2D::Distance(FVector2D(Pos), FVector2D(D.UnderAt)) < D.UnderRadius;
}

bool WRSkyIsles::InFlue(const FVector& Pos)
{
	if (!Live()) return false;
	const FWRSkyIsleData& D = FWRSkyIsleData::Get();
	return Pos.Z < D.FlueRim && Pos.Z > D.FlueFloor - 500.f && FVector2D::Distance(FVector2D(Pos), D.FlueAt) < D.FlueThroat;
}

TCHAR WRSkyIsles::ZoneAt(const FVector& Pos)
{
	if (!Contains(Pos) && WRLand::BiomeAt(Pos) != WRSkyIsleImpl::SBiome) return 0;
	if (InUndercloud(Pos)) return '5';
	const FWRSkyIsleData& D = FWRSkyIsleData::Get();
	// Near an island (300 m): its zone. The air between islands is the nearest island's.
	float Gap = 0.f;
	const FWRSkyIsleData::FIsland* I = D.NearestIsland(Pos, &Gap);
	float GroundZ = 0.f;
	const bool bGround = WRLand::GroundUnder(Pos, GroundZ) && Pos.Z < GroundZ + 25000.f;
	if (I && (Gap < 30000.f || !bGround)) return I->Zone;
	const FName Z = WRLand::ZoneAt(Pos);
	if (Z == TEXT("W1")) return '1';
	if (!Z.IsNone()) return 'f';
	return I ? I->Zone : '1';
}

bool WRSkyIsles::GroundUnder(const FVector& Pos, float& OutZ)
{
	if (const FWRSkyIsleData::FIsland* I = FWRSkyIsleData::Get().IslandAt(Pos, 0.f, 1.0e6f, 50.f))
	{
		OutZ = I->TopZ(Pos);
		return true;
	}
	return WRLand::GroundUnder(Pos, OutZ);
}

// ================================================================================================ the air

double WRSkyIsles::ServerTime(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World) return 0.0;
	if (const AGameStateBase* GS = World->GetGameState()) return GS->GetServerWorldTimeSeconds();
	return World->GetTimeSeconds();
}

namespace WRSkyIsleImpl
{
	/** The Turning Wind's swing (0..180 degrees), set by WRSkyEvents on every machine. */
	float GTurn = 0.f;
}

void WRSkyIsles::SetTurn(float Degrees) { WRSkyIsleImpl::GTurn = Degrees; }
float WRSkyIsles::Turn() { return WRSkyIsleImpl::GTurn; }

float WRSkyIsles::WindYaw(const UObject* WorldContext)
{
	const double T = ServerTime(WorldContext);
	const float Wander = WRTune(TEXT("skyWorld.wind.wander"), 15.f) * FMath::Sin(float(FMath::Fmod(T, 86400.0)) * 2.f * PI / WRTune(TEXT("skyWorld.wind.wanderSeconds"), 600.f));
	return FRotator::NormalizeAxis(WRTune(TEXT("skyWorld.wind.yaw"), 180.f) + Wander + WRSkyIsleImpl::GTurn);
}

FVector2D WRSkyIsles::WindDir(const UObject* WorldContext)
{
	const float Y = FMath::DegreesToRadians(WindYaw(WorldContext));
	return FVector2D(FMath::Cos(Y), FMath::Sin(Y));
}

float WRSkyIsles::WindStrength(const UObject* WorldContext)
{
	if (const FWRWeatherKind* K = UWRWeatherSubsystem::ActiveKind(WorldContext, WRSkyIsleImpl::SBiome))
	{
		if (K->Id == TEXT("gale")) return WRTune(TEXT("skyWorld.wind.gale"), 2.f);
		if (K->Id == TEXT("overcast") || K->Id == TEXT("rain")) return WRTune(TEXT("skyWorld.wind.rain"), 1.2f);
	}
	return 1.f;
}

float WRSkyIsles::LiftAt(const UObject* WorldContext, const FVector& Pos)
{
	if (!Contains(Pos) && !InFlue(Pos)) return 0.f;
	using WRSkyIsleImpl::Smooth;
	const FWRSkyIsleData& D = FWRSkyIsleData::Get();
	const float FadeTop = WRTune(TEXT("skyWorld.updraft.fadeTop"), 3000.f);
	float Best = 0.f;
	FVector2D Wind = FVector2D::ZeroVector;
	bool bWind = false;
	for (const FWRSkyIsleData::FUpdraft& U : D.Updrafts)
	{
		if (Pos.Z < U.Base || Pos.Z > U.Top) continue;
		FVector2D C = U.At;
		if (const FWRSkyIsleData::FIsland* I = D.FindIsland(U.Island)) C += FVector2D(WRSkyIsles::DriftOf(I->Index));
		const float Dist = FVector2D::Distance(FVector2D(Pos), C);
		if (Dist > U.Radius) continue;
		float Lift = U.Lift;
		if (U.Kind == TEXT("ridge"))
		{
			// Ridge lift only while the wind blows onto the cliff (from the sea: its angle to the cliff's seaward normal).
			if (!bWind) { Wind = WindDir(WorldContext); bWind = true; }
			const FVector2D N = U.Normal.IsNearlyZero() ? FVector2D(1.f, 0.f) : U.Normal;
			const float Onto = FVector2D::DotProduct(-Wind, N);
			if (Onto < FMath::Cos(FMath::DegreesToRadians(WRTune(TEXT("skyWorld.updraft.ridgeAngle"), 60.f)))) continue;
			Lift *= WindStrength(WorldContext);
		}
		// Fades over the top and the outer fifth of the radius.
		Lift *= Smooth(U.Top, U.Top - FadeTop, Pos.Z) * Smooth(U.Radius, U.Radius * 0.8f, Dist);
		Best = FMath::Max(Best, Lift);
	}
	return Best;
}

float WRSkyIsles::SinkAt(const UObject* WorldContext, const FVector& Pos, FVector& OutShove)
{
	OutShove = FVector::ZeroVector;
	if (!Contains(Pos)) return 0.f;
	const FWRSkyIsleData& D = FWRSkyIsleData::Get();
	const double T = ServerTime(WorldContext);
	float Sink = 0.f;
	// Lee rotors (W1): live while the wind puts them in the lee (blowing west, toward them, from their island). Each
	// pulses on its own clock: tell (1.0 s), the shove (0.6 s) and +3 m/s sink for 2 s from the shove.
	const FVector2D Wind = WindDir(WorldContext);
	const float Period = WRTune(TEXT("hazards.sky.rotor.period"), 4.f);
	const float Tell = WRTune(TEXT("hazards.sky.rotor.tell"), 1.f);
	for (int32 i = 0; i < D.Rotors.Num(); ++i)
	{
		const FWRSkyIsleData::FRotor& R = D.Rotors[i];
		if (Pos.Z < R.At.Z - R.Height * 0.5f || Pos.Z > R.At.Z + R.Height * 0.5f) continue;
		if (FVector2D::Distance(FVector2D(Pos), FVector2D(R.At)) > R.Radius) continue;
		const FWRSkyIsleData::FIsland* I = D.FindIsland(R.Island);
		if (I && FVector2D::DotProduct(Wind, (FVector2D(R.At) - FVector2D(I->Now())).GetSafeNormal()) < 0.3f) continue;
		const float Ph = float(FMath::Fmod(T + i * 1.37, double(Period)));
		if (Ph >= Tell && Ph < Tell + WRTune(TEXT("hazards.sky.rotor.shoveSeconds"), 0.6f))
		{
			const FVector2D Side(-Wind.Y, Wind.X);
			OutShove = FVector(Side * WRTune(TEXT("hazards.sky.rotor.shove"), 600.f) * ((i % 2) ? 1.f : -1.f), 0.f);
		}
		if (Ph >= Tell && Ph < Tell + WRTune(TEXT("hazards.sky.rotor.sinkSeconds"), 2.f)) Sink = FMath::Max(Sink, WRTune(TEXT("hazards.sky.rotor.sink"), 300.f));
	}
	// Downbursts (W3): each cycles 28-44 s; live the last 8 s (the virga hangs, a 30 m column sinks gliders +6 m/s).
	for (int32 i = 0; i < D.Downbursts.Num(); ++i)
	{
		const FWRSkyIsleData::FDownburst& B = D.Downbursts[i];
		if (FVector2D::Distance(FVector2D(Pos), FVector2D(B.At)) > B.Radius) continue;
		if (Pos.Z > B.At.Z + 4000.f || Pos.Z < B.At.Z - 40000.f) continue;
		const float Cycle = WRTune(TEXT("hazards.sky.downburst.cycleMin"), 28.f) + float((i * 7) % 17);
		const float Live = WRTune(TEXT("hazards.sky.downburst.live"), 8.f);
		const float Ph = float(FMath::Fmod(T + i * 11.3, double(Cycle)));
		if (Ph >= Cycle - Live) Sink = FMath::Max(Sink, WRTune(TEXT("hazards.sky.downburst.sink"), 600.f));
	}
	return Sink;
}
