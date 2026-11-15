// The Ancient forest's land data and the land's shared calls (cooks/biome-forest.md 2.1-2.2; cooks/build-biome-forest.md
// 2.2, 2.4, 2.15): Data/forest.json parsed once into game cm (the file is map metres, and the forest's runtime land
// (WRLand, Data/lands/forest.json) is built so map metres x 100 are game cm), and the questions every other part of the
// forest asks of it: is this point the forest's, which sub-zone, which band, how closed is the canopy, may a tree be
// felled here.
#include "WRForestTypes.h"
#include "Wildreach.h"
#include "WRFarm.h"
#include "WRForestHollow.h"
#include "WRTuning.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace WRForestTypesImpl
{
	float Num(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, float Fallback)
	{
		double V = 0.0;
		return O.IsValid() && O->TryGetNumberField(Key, V) ? float(V) : Fallback;
	}

	FString Str(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		FString S;
		if (O.IsValid()) O->TryGetStringField(Key, S);
		return S;
	}

	/** [x, y, z] metres -> cm (a missing z is 0). */
	FVector P3(const TSharedPtr<FJsonValue>& V)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (!V.IsValid() || !V->TryGetArray(A) || A->Num() < 2) return FVector::ZeroVector;
		return FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), A->Num() >= 3 ? (*A)[2]->AsNumber() : 0.0) * 100.0;
	}

	/** A row's x, y, z (metres) -> cm. */
	FVector XYZ(const TSharedPtr<FJsonObject>& O)
	{
		return FVector(Num(O, TEXT("x"), 0.f), Num(O, TEXT("y"), 0.f), Num(O, TEXT("z"), 0.f)) * 100.0;
	}

	const TArray<TSharedPtr<FJsonValue>>& Arr(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		static const TArray<TSharedPtr<FJsonValue>> Empty;
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		return O.IsValid() && O->TryGetArrayField(Key, A) ? *A : Empty;
	}

	void ReadSite(const TSharedPtr<FJsonObject>& O, FWRForestData::FSite& S)
	{
		S.Id = FName(*Str(O, TEXT("id")));
		S.Kind = FName(*Str(O, TEXT("kind")));
		S.Name = Str(O, TEXT("name"));
		S.At = XYZ(O);
		S.Yaw = Num(O, TEXT("yaw"), 0.f);
		S.Variant = int32(Num(O, TEXT("variant"), 0.f));
		bool bLm = false;
		S.bLandmark = O->TryGetBoolField(TEXT("landmark"), bLm) && bLm;
		const FString Z = Str(O, TEXT("zone"));
		S.Zone = Z.Len() ? Z[0] : TCHAR(0);
		const TSharedPtr<FJsonObject>* Loot = nullptr;
		if (O->TryGetObjectField(TEXT("loot"), Loot))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& KV : (*Loot)->Values) S.Loot.Add(FName(*KV.Key), int32(KV.Value->AsNumber()));
		}
		const FString Cave = Str(O, TEXT("cave"));
		if (!Cave.IsEmpty()) S.Cave = FName(*Cave);
	}

	void ReadPath(const TSharedPtr<FJsonObject>& O, FWRForestData::FPath& P, bool bRiver)
	{
		P.Id = FName(*Str(O, TEXT("id")));
		P.Kind = FName(*Str(O, TEXT("kind")));
		for (const TSharedPtr<FJsonValue>& V : Arr(O, TEXT("pts"))) P.Points.Add(P3(V));
		if (bRiver)
		{
			for (const TSharedPtr<FJsonValue>& V : Arr(O, TEXT("widths"))) P.Widths.Add(float(V->AsNumber()) * 100.f);
			P.Width = P.Widths.Num() ? P.Widths[0] : 1500.f;
		}
		else
		{
			P.Width = Num(O, TEXT("w"), 4.f) * 100.f;
		}
	}

	/** Canopy cover by sub-zone letter (forest.json "canopy") and the light gaps cut round the places (cm: x, y, r). */
	struct FCanopy
	{
		TMap<TCHAR, float> ByLetter;
		TArray<FVector> Gaps;
	};
	FCanopy& CanopyData()
	{
		static FCanopy C;
		return C;
	}

	bool Live()
	{
		static const FName Forest(TEXT("forest"));
		return WRBiome::IsLive(Forest);
	}

	/** In the frame, south of the island's landscape edge (the strip under the island's south shore is the island's). */
	bool InLand(const FWRForestData& D, const FVector& Pos)
	{
		return D.bLoaded && D.InFrame(Pos) && Pos.Y >= WRTune(TEXT("world.islandSouthEdge"), 201600.f);
	}
}

// ================================================================================================ the data

const FWRForestData& FWRForestData::Get()
{
	using namespace WRForestTypesImpl;
	static FWRForestData D;
	static bool bTried = false;
	if (bTried) return D;
	bTried = true;
	const TSharedPtr<FJsonObject> J = FWRTuning::LoadDataFile(TEXT("forest.json"));
	if (!J.IsValid())
	{
		UE_LOG(LogWildreach, Warning, TEXT("FOREST Data/forest.json is missing (node Tools/terrain/forest.mjs): the forest has no land"));
		return D;
	}
	const TSharedPtr<FJsonObject>* F = nullptr;
	if (J->TryGetObjectField(TEXT("frame"), F))
	{
		const float X0 = Num(*F, TEXT("x0"), 0.f), Y0 = Num(*F, TEXT("y0"), 0.f);
		const float Span = (Num(*F, TEXT("samples"), 1.f) - 1.f) * Num(*F, TEXT("step"), 2.f);
		D.FrameMin = FVector2D(X0, Y0) * 100.f;
		D.FrameMax = FVector2D(X0 + Span, Y0 + Span) * 100.f;
	}
	if (J->TryGetObjectField(TEXT("zones"), F))
	{
		D.ZoneOrigin = FVector2D(Num(*F, TEXT("x0"), 0.f), Num(*F, TEXT("y0"), 0.f)) * 100.f;
		D.ZoneCell = FMath::Max(100.f, Num(*F, TEXT("cell"), 16.f) * 100.f);
		D.ZoneW = int32(Num(*F, TEXT("width"), 0.f));
		for (const TSharedPtr<FJsonValue>& V : Arr(*F, TEXT("rows"))) D.ZoneRows.Add(V->AsString());
		D.ZoneH = D.ZoneRows.Num();
	}
	FCanopy& Cn = WRForestTypesImpl::CanopyData();
	if (J->TryGetObjectField(TEXT("canopy"), F))
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& KV : (*F)->Values) if (KV.Key.Len()) Cn.ByLetter.Add(KV.Key[0], float(KV.Value->AsNumber()));
	}
	for (const TSharedPtr<FJsonValue>& V : Arr(J, TEXT("sites"))) ReadSite(V->AsObject(), D.Sites.AddDefaulted_GetRef());
	for (const TSharedPtr<FJsonValue>& V : Arr(J, TEXT("kit"))) ReadSite(V->AsObject(), D.Kit.AddDefaulted_GetRef());
	for (const TSharedPtr<FJsonValue>& V : Arr(J, TEXT("shrines")))
	{
		FSite& S = D.Shrines.AddDefaulted_GetRef();
		ReadSite(V->AsObject(), S);
		S.Kind = TEXT("shrine");
	}
	for (const TSharedPtr<FJsonValue>& V : Arr(J, TEXT("giants")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FTree& T = D.Giants.AddDefaulted_GetRef();
		T.Kind = FName(*Str(O, TEXT("kind")));
		T.Base = XYZ(O);
		T.Height = Num(O, TEXT("h"), 60.f) * 100.f;
		T.Radius = Num(O, TEXT("r"), 2.5f) * 100.f;
		T.Yaw = Num(O, TEXT("yaw"), 0.f);
		T.Variant = int32(Num(O, TEXT("variant"), 0.f));
	}
	for (const TSharedPtr<FJsonValue>& V : Arr(J, TEXT("fallen")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FLog& L = D.Fallen.AddDefaulted_GetRef();
		L.A = P3(O->TryGetField(TEXT("a")));
		L.B = P3(O->TryGetField(TEXT("b")));
		L.Radius = Num(O, TEXT("r"), 1.5f) * 100.f;
		L.Nodes = int32(Num(O, TEXT("nodes"), 6.f));
		bool bNurse = false;
		L.bNurse = O->TryGetBoolField(TEXT("nurse"), bNurse) && bNurse;
	}
	for (const TSharedPtr<FJsonValue>& V : Arr(J, TEXT("routes"))) ReadPath(V->AsObject(), D.Routes.AddDefaulted_GetRef(), false);
	for (const TSharedPtr<FJsonValue>& V : Arr(J, TEXT("rivers"))) ReadPath(V->AsObject(), D.Rivers.AddDefaulted_GetRef(), true);
	for (const TSharedPtr<FJsonValue>& V : Arr(J, TEXT("falls")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FFall& Fa = D.Falls.AddDefaulted_GetRef();
		Fa.Id = FName(*Str(O, TEXT("id")));
		Fa.Top = P3(O->TryGetField(TEXT("top")));
		Fa.Bottom = P3(O->TryGetField(TEXT("bottom")));
		Fa.Width = Num(O, TEXT("width"), 15.f) * 100.f;
	}
	for (const TSharedPtr<FJsonValue>& V : Arr(J, TEXT("brakes")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FBrake& B = D.Brakes.AddDefaulted_GetRef();
		B.Centre = XYZ(O);
		B.Radius = Num(O, TEXT("r"), 0.f) * 100.f;
		B.Depth = Num(O, TEXT("depth"), 6.f) * 100.f;
		B.Length = Num(O, TEXT("length"), 0.f) * 100.f;
		B.Yaw = Num(O, TEXT("yaw"), 0.f);
		B.Height = Num(O, TEXT("h"), 3.f) * 100.f;
		bool bRing = true;
		O->TryGetBoolField(TEXT("ring"), bRing);
		B.bRing = bRing;
		const FString G = Str(O, TEXT("guards"));
		if (!G.IsEmpty()) B.Guards = FName(*G);
	}
	for (const TSharedPtr<FJsonValue>& V : Arr(J, TEXT("spots")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FSpot& S = D.Spots.AddDefaulted_GetRef();
		S.Kind = FName(*Str(O, TEXT("kind")));
		S.At = XYZ(O);
		S.Radius = Num(O, TEXT("r"), 0.f) * 100.f;
		S.Depth = Num(O, TEXT("depth"), 0.f) * 100.f;
		S.Yaw = Num(O, TEXT("yaw"), 0.f);
	}
	for (const TSharedPtr<FJsonValue>& V : Arr(J, TEXT("walkways")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FWalkway& W = D.Walkways.AddDefaulted_GetRef();
		W.Id = FName(*Str(O, TEXT("id")));
		W.A = P3(O->TryGetField(TEXT("a")));
		W.B = P3(O->TryGetField(TEXT("b")));
		const FString Ar = Str(O, TEXT("arena"));
		if (!Ar.IsEmpty()) W.Arena = FName(*Ar);
	}
	if (J->TryGetObjectField(TEXT("arenas"), F))
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& KV : (*F)->Values)
		{
			const TSharedPtr<FJsonObject> O = KV.Value->AsObject();
			if (!O.IsValid()) continue;
			FArena A;
			A.Id = FName(*KV.Key);
			A.Centre = XYZ(O);
			A.Radius = Num(O, TEXT("radius"), 50.f) * 100.f;
			A.Runback = P3(O->TryGetField(TEXT("runback")));
			A.Altar = P3(O->TryGetField(TEXT("altar")));   // null (no altar) reads as zero
			for (const TSharedPtr<FJsonValue>& P : Arr(O, TEXT("points"))) A.Points.Add(P3(P));
			D.Arenas.Add(A.Id, MoveTemp(A));
		}
	}
	for (const TSharedPtr<FJsonValue>& V : Arr(J, TEXT("hollow")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FRoom& R = D.Hollow.AddDefaulted_GetRef();
		R.Id = FName(*Str(O, TEXT("id")));
		R.Name = Str(O, TEXT("name"));
		R.At = XYZ(O);
		R.Half = FVector(Num(O, TEXT("hx"), 15.f), Num(O, TEXT("hy"), 15.f), Num(O, TEXT("hz"), 8.f)) * 100.0;
		R.Yaw = Num(O, TEXT("yaw"), 0.f);
	}
	if (J->TryGetObjectField(TEXT("village"), F))
	{
		D.Village = XYZ(*F);
		D.VillageRadius = Num(*F, TEXT("radius"), 120.f) * 100.f;
	}
	// The light gaps (cooks/biome-forest.md 2.1: every POI sits in a light gap): each place, each fallen giant's length,
	// the arenas' clearings and the village's terrace open the canopy over them.
	for (const FSite& S : D.Sites) Cn.Gaps.Add(FVector(S.At.X, S.At.Y, S.bLandmark ? 4000.f : 2800.f));
	for (const FSite& S : D.Shrines) Cn.Gaps.Add(FVector(S.At.X, S.At.Y, 2500.f));
	for (const FLog& L : D.Fallen)
	{
		for (int32 k = 0; k <= 4; ++k)
		{
			const FVector P = FMath::Lerp(L.A, L.B, k / 4.f);
			Cn.Gaps.Add(FVector(P.X, P.Y, 1800.f));
		}
	}
	for (const TPair<FName, FArena>& A : D.Arenas) Cn.Gaps.Add(FVector(A.Value.Centre.X, A.Value.Centre.Y, A.Value.Radius));
	Cn.Gaps.Add(FVector(D.Village.X, D.Village.Y, D.VillageRadius));
	D.bLoaded = D.ZoneRows.Num() > 0 && D.FrameMax.X > D.FrameMin.X;
	UE_LOG(LogWildreach, Log, TEXT("FOREST data: %d sites, %d kit, %d giants, %d fallen, %d brakes, %d spots, %d rooms"),
		D.Sites.Num(), D.Kit.Num(), D.Giants.Num(), D.Fallen.Num(), D.Brakes.Num(), D.Spots.Num(), D.Hollow.Num());
	return D;
}

TCHAR FWRForestData::ZoneAt(const FVector& Pos) const
{
	const int32 I = FMath::FloorToInt((Pos.X - ZoneOrigin.X) / ZoneCell), J = FMath::FloorToInt((Pos.Y - ZoneOrigin.Y) / ZoneCell);
	if (I < 0 || J < 0 || J >= ZoneH || I >= ZoneW || I >= ZoneRows[J].Len()) return 0;
	return ZoneRows[J][I];
}

const FWRForestData::FSite* FWRForestData::FindSite(FName Id) const
{
	if (const FSite* S = Sites.FindByPredicate([Id](const FSite& Q) { return Q.Id == Id; })) return S;
	if (const FSite* S = Kit.FindByPredicate([Id](const FSite& Q) { return Q.Id == Id; })) return S;
	return Shrines.FindByPredicate([Id](const FSite& Q) { return Q.Id == Id; });
}

// ================================================================================================ the land's calls

bool WRForest::Contains(const FVector& Pos)
{
	using namespace WRForestTypesImpl;
	if (!Live()) return false;
	const FWRForestData& D = FWRForestData::Get();
	if (!InLand(D, Pos)) return false;
	const TCHAR Z = D.ZoneAt(Pos);
	return Z != 0 && Z != TEXT('~');
}

TCHAR WRForest::ZoneAt(const UObject* WorldContext, const FVector& Pos)
{
	using namespace WRForestTypesImpl;
	if (!Live()) return 0;
	const FWRForestData& D = FWRForestData::Get();
	if (!InLand(D, Pos)) return 0;
	// Under the old growth's roof the Hollow Root's rooms and passages are their own place, whatever the grid says
	// over them (WRForestHollow.cpp knows the roofs and floors).
	if (WRForestHollowContains(Pos)) return TEXT('h');
	return D.ZoneAt(Pos);
}

FName WRForest::Band(const FVector& Pos)
{
	// cooks/biome-forest.md 2.1: the forest is wider than the band rule, so the bands are fixed lines of x.
	static const FName Lowland(TEXT("lowland")), Midland(TEXT("midland")), Highland(TEXT("highland"));
	const float XM = float(Pos.X) / 100.f;
	if (XM > WRTune(TEXT("forest.bands.lowlandFrom"), 450.f)) return Lowland;
	if (XM < WRTune(TEXT("forest.bands.highlandTo"), -400.f)) return Highland;
	return Midland;
}

float WRForest::Canopy(const FVector& Pos)
{
	// The canopy is a gradient (cooks/biome-forest.md 2.1): 0 at the coast, 0.4 in F2/F3, 0.75 in F4, 0.9 in F5, light
	// and fog following. Sampled on a ring the size of the zone blend (60-120 m across an edge, like the ground's colour),
	// then opened over the places (a shaft of light means something's there).
	using namespace WRForestTypesImpl;
	if (!Live()) return 0.f;
	const FWRForestData& D = FWRForestData::Get();
	if (!InLand(D, Pos)) return 0.f;
	const FCanopy& Cn = WRForestTypesImpl::CanopyData();
	if (WRForestHollowContains(Pos))
	{
		const float* H = Cn.ByLetter.Find(TEXT('h'));
		return H ? *H : 0.95f;
	}
	const float R = WRTune(TEXT("forest.canopy.blendRadius"), 4500.f);
	float Sum = 0.f, WSum = 0.f;
	for (int32 k = 0; k < 9; ++k)
	{
		const FVector P = k == 0 ? Pos : Pos + FVector(FMath::Cos(k * PI / 4.f), FMath::Sin(k * PI / 4.f), 0.f) * R;
		const TCHAR L = D.ZoneAt(P);
		const float* V = L ? Cn.ByLetter.Find(L) : nullptr;
		const float W = k == 0 ? 2.f : 1.f;
		Sum += (V ? *V : 0.f) * W;
		WSum += W;
	}
	float C = WSum > 0.f ? Sum / WSum : 0.f;
	// The light gaps: full open over the place's middle, closing over 15 m past its radius.
	const float Open = WRTune(TEXT("forest.canopy.gapOpen"), 0.6f);
	float Gap = 0.f;
	for (const FVector& G : Cn.Gaps)
	{
		const float Dist = FVector::Dist2D(Pos, FVector(G.X, G.Y, 0.f));
		if (Dist > G.Z + 1500.f) continue;
		Gap = FMath::Max(Gap, 1.f - FMath::SmoothStep(float(G.Z), float(G.Z) + 1500.f, Dist));
	}
	C *= 1.f - Open * Gap;
	return FMath::Clamp(C, 0.f, 0.95f);
}

bool WRForest::NoFelling(const FVector& Pos)
{
	// "The roots hold it." (cooks/biome-forest.md 2.7): no felling within 60 m of a giant's or a heart-tree's root crown
	// (the crown reaches 1.6 trunk radii out). The Ringfort's pleached beeches are a hedge, not giants.
	if (!Contains(Pos)) return false;
	const FWRForestData& D = FWRForestData::Get();
	const float Reach = WRTune(TEXT("forest.noFellingRadius"), 6000.f);
	static const FName Hedge(TEXT("ringfort_beech"));
	for (const FWRForestData::FTree& T : D.Giants)
	{
		if (T.Kind == Hedge) continue;
		const float Crown = T.Radius * 1.6f;
		if (FMath::Abs(T.Base.X - Pos.X) > Reach + Crown || FMath::Abs(T.Base.Y - Pos.Y) > Reach + Crown) continue;
		if (FVector::Dist2D(T.Base, Pos) < Reach + Crown) return true;
	}
	return false;
}
