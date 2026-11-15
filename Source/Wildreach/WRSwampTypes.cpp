#include "WRSwampTypes.h"

#include "WRFarm.h"
#include "WRTuning.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

namespace WRSwampData
{
	double Num(const TSharedPtr<FJsonObject>& O, const TCHAR* K, double F = 0.0)
	{
		double V = F;
		if (O.IsValid()) O->TryGetNumberField(K, V);
		return V;
	}

	FVector V3(const TArray<TSharedPtr<FJsonValue>>& A)
	{
		return FVector(A.Num() > 0 ? A[0]->AsNumber() * 100.0 : 0.0, A.Num() > 1 ? A[1]->AsNumber() * 100.0 : 0.0,
			A.Num() > 2 ? A[2]->AsNumber() * 100.0 : 0.0);
	}

	TArray<FVector> V3s(const TArray<TSharedPtr<FJsonValue>>& A)
	{
		TArray<FVector> Out;
		for (const TSharedPtr<FJsonValue>& V : A)
		{
			const TArray<TSharedPtr<FJsonValue>>* P = nullptr;
			if (V.IsValid() && V->TryGetArray(P)) Out.Add(V3(*P));
		}
		return Out;
	}

	TArray<int32> Ints(const TSharedPtr<FJsonObject>& O, const TCHAR* K)
	{
		TArray<int32> Out;
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (O.IsValid() && O->TryGetArrayField(K, A)) for (const TSharedPtr<FJsonValue>& V : *A) Out.Add((int32)V->AsNumber());
		return Out;
	}

	void ReadGrid(const TSharedPtr<FJsonObject>& G, FVector2D& Origin, float& Cell, int32& W, int32& H, TArray<FString>& Rows, double DefCell)
	{
		if (!G.IsValid()) return;
		Cell = Num(G, TEXT("cell"), DefCell) * 100.0;
		Origin = FVector2D(Num(G, TEXT("x0")) * 100.0, Num(G, TEXT("y0")) * 100.0);
		W = (int32)Num(G, TEXT("width"));
		H = (int32)Num(G, TEXT("height"));
		G->TryGetStringArrayField(TEXT("rows"), Rows);
	}

	TCHAR GridAt(const FVector& Pos, const FVector2D& Origin, float Cell, int32 W, int32 H, const TArray<FString>& Rows, TCHAR Outside)
	{
		if (Cell <= 0.f || W <= 0 || H <= 0) return Outside;
		const int32 I = FMath::FloorToInt((Pos.X - Origin.X) / Cell), J = FMath::FloorToInt((Pos.Y - Origin.Y) / Cell);
		if (I < 0 || J < 0 || I >= W || J >= H || !Rows.IsValidIndex(J)) return Outside;
		const FString& Row = Rows[J];
		return Row.IsValidIndex(I) ? Row[I] : Outside;
	}
}

const FWRSwampData& FWRSwampData::Get()
{
	using namespace WRSwampData;
	static FWRSwampData D;
	static bool bTried = false;
	if (bTried) return D;
	bTried = true;
	const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("swamp.json"));
	if (!Root.IsValid()) return D;
	D.Root = Root;
	D.bLoaded = true;

	const TSharedPtr<FJsonObject>* Obj = nullptr;
	if (Root->TryGetObjectField(TEXT("frame"), Obj))
	{
		const double X0 = Num(*Obj, TEXT("x0")), Y0 = Num(*Obj, TEXT("y0")), Step = Num(*Obj, TEXT("step"), 2.0), N = Num(*Obj, TEXT("samples"), 1891.0);
		D.FrameMin = FVector2D(X0 * 100.0, Y0 * 100.0);
		D.FrameMax = FVector2D((X0 + (N - 1) * Step) * 100.0, (Y0 + (N - 1) * Step) * 100.0);
	}
	D.SeaZ = Num(Root, TEXT("seaZ"), 0.6) * 100.0;
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (Root->TryGetArrayField(TEXT("boundary"), Arr))
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TArray<TSharedPtr<FJsonValue>>* P = nullptr;
			if (V->TryGetArray(P) && P->Num() >= 2) D.Boundary.Add(FVector2D((*P)[0]->AsNumber() * 100.0, (*P)[1]->AsNumber() * 100.0));
		}
		D.Boundary.Sort([](const FVector2D& A, const FVector2D& B) { return A.X < B.X; });
	}
	if (Root->TryGetObjectField(TEXT("zones"), Obj)) ReadGrid(*Obj, D.ZoneOrigin, D.ZoneCell, D.ZoneW, D.ZoneH, D.ZoneRows, 32.0);
	if (Root->TryGetObjectField(TEXT("water"), Obj)) ReadGrid(*Obj, D.WaterOrigin, D.WaterCell, D.WaterW, D.WaterH, D.WaterRows, 8.0);

	const auto ReadSites = [&](const TCHAR* Field, TArray<FSite>& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (!Root->TryGetArrayField(Field, A)) return;
		for (const TSharedPtr<FJsonValue>& V : *A)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			if (!O.IsValid()) continue;
			FSite S;
			FString Str;
			if (O->TryGetStringField(TEXT("id"), Str)) S.Id = FName(*Str);
			if (O->TryGetStringField(TEXT("kind"), Str)) S.Kind = FName(*Str);
			if (O->TryGetStringField(TEXT("zone"), Str)) S.Zone = FName(*Str);
			O->TryGetStringField(TEXT("name"), S.Name);
			S.At = FVector(Num(O, TEXT("x")) * 100.0, Num(O, TEXT("y")) * 100.0, Num(O, TEXT("z")) * 100.0);
			S.Yaw = Num(O, TEXT("yaw"));
			S.Variant = (int32)Num(O, TEXT("variant"));
			O->TryGetBoolField(TEXT("landmark"), S.bLandmark);
			const TSharedPtr<FJsonObject>* Loot = nullptr;
			if (O->TryGetObjectField(TEXT("loot"), Loot))
			{
				for (const auto& KV : (*Loot)->Values) S.Loot.Add(FName(*KV.Key), (int32)KV.Value->AsNumber());
			}
			S.Row = O;
			Out.Add(S);
		}
	};
	ReadSites(TEXT("sites"), D.Sites);
	ReadSites(TEXT("kit"), D.Kit);

	if (Root->TryGetArrayField(TEXT("shrines"), Arr))
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			if (!O.IsValid()) continue;
			FShrine S;
			FString Str;
			if (O->TryGetStringField(TEXT("id"), Str)) S.Id = FName(*Str);
			O->TryGetStringField(TEXT("name"), S.Name);
			S.At = FVector(Num(O, TEXT("x")) * 100.0, Num(O, TEXT("y")) * 100.0, Num(O, TEXT("z")) * 100.0);
			TArray<FString> G;
			O->TryGetStringArrayField(TEXT("guard"), G);
			for (const FString& Gs : G) S.Guard.Add(FName(*Gs));
			D.Shrines.Add(S);
		}
	}
	if (Root->TryGetArrayField(TEXT("routes"), Arr))
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			if (!O.IsValid()) continue;
			FRoute R;
			FString Str;
			if (O->TryGetStringField(TEXT("id"), Str)) R.Id = FName(*Str);
			if (O->TryGetStringField(TEXT("kind"), Str)) R.Kind = FName(*Str);
			R.Width = Num(O, TEXT("w"), 2.0) * 100.0;
			const TArray<TSharedPtr<FJsonValue>>* P = nullptr;
			if (O->TryGetArrayField(TEXT("pts"), P)) R.Points = V3s(*P);
			R.Rotten = Ints(O, TEXT("rotten"));
			R.Broken = Ints(O, TEXT("broken"));
			D.Routes.Add(R);
		}
	}
	if (Root->TryGetArrayField(TEXT("gas"), Arr))
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			if (!O.IsValid()) continue;
			FGasField G;
			G.At = FVector(Num(O, TEXT("x")) * 100.0, Num(O, TEXT("y")) * 100.0, Num(O, TEXT("z")) * 100.0);
			const TArray<TSharedPtr<FJsonValue>>* P = nullptr;
			if (O->TryGetArrayField(TEXT("vents"), P))
			{
				for (FVector& Vent : V3s(*P)) { if (Vent.Z == 0.0) Vent.Z = G.At.Z; G.Vents.Add(Vent); }
			}
			D.Gas.Add(G);
		}
	}
	if (Root->TryGetArrayField(TEXT("falls"), Arr))
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			if (!O.IsValid()) continue;
			FFall F;
			F.Top = FVector(Num(O, TEXT("x")) * 100.0, Num(O, TEXT("y")) * 100.0, Num(O, TEXT("top")) * 100.0);
			F.Bottom = Num(O, TEXT("bottom")) * 100.0;
			F.Width = Num(O, TEXT("width"), 20.0) * 100.0;
			D.Falls.Add(F);
		}
	}
	if (Root->TryGetArrayField(TEXT("camps"), Arr))
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			if (O.IsValid()) D.Camps.Add(FVector(Num(O, TEXT("x")) * 100.0, Num(O, TEXT("y")) * 100.0, Num(O, TEXT("z")) * 100.0));
		}
	}
	if (Root->TryGetObjectField(TEXT("arenas"), Obj))
	{
		for (const auto& KV : (*Obj)->Values)
		{
			const TSharedPtr<FJsonObject> O = KV.Value.IsValid() ? KV.Value->AsObject() : nullptr;
			if (O.IsValid()) D.Arenas.Add(FName(*KV.Key), O);
		}
	}
	if (Root->TryGetArrayField(TEXT("hexwalk"), Arr)) D.HexWalk = V3s(*Arr);
	if (Root->TryGetObjectField(TEXT("candle"), Obj))
	{
		D.CandleFrom = WRSwamp::Point(*Obj, TEXT("from"));
		D.CandleIsland = WRSwamp::Point(*Obj, TEXT("island"));
		D.CandleLine = WRSwamp::Points(*Obj, TEXT("pts"));
	}
	if (Root->TryGetObjectField(TEXT("coven"), Obj))
	{
		D.DeepPool = WRSwamp::Point(*Obj, TEXT("deepPool"));
		D.Chimney = WRSwamp::Point(*Obj, TEXT("chimney"));
		D.Cellar = WRSwamp::Point(*Obj, TEXT("cellar"));
		const TArray<TSharedPtr<FJsonValue>>* Rooms = nullptr;
		if ((*Obj)->TryGetArrayField(TEXT("rooms"), Rooms))
		{
			for (const TSharedPtr<FJsonValue>& V : *Rooms)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				if (!O.IsValid()) continue;
				FRoom R;
				FString Str;
				if (O->TryGetStringField(TEXT("id"), Str)) R.Id = FName(*Str);
				R.At = FVector(Num(O, TEXT("x")) * 100.0, Num(O, TEXT("y")) * 100.0, Num(O, TEXT("z")) * 100.0);
				R.Size = FVector(Num(O, TEXT("w"), 10.0) * 100.0, Num(O, TEXT("d"), 10.0) * 100.0, Num(O, TEXT("h"), 4.0) * 100.0);
				D.CovenRooms.Add(R);
			}
		}
	}
	return D;
}

TCHAR FWRSwampData::ZoneAt(const FVector& Pos) const
{
	return WRSwampData::GridAt(Pos, ZoneOrigin, ZoneCell, ZoneW, ZoneH, ZoneRows, TEXT('~'));
}

EWRSwampWater FWRSwampData::BaseWaterAt(const FVector& Pos) const
{
	switch (WRSwampData::GridAt(Pos, WaterOrigin, WaterCell, WaterW, WaterH, WaterRows, TEXT('.')))
	{
	case TEXT('m'): return EWRSwampWater::Murk;
	case TEXT('d'): return EWRSwampWater::Duckweed;
	case TEXT('b'): return EWRSwampWater::Bleedwater;
	case TEXT('q'): return EWRSwampWater::Quag;
	case TEXT('k'): return EWRSwampWater::Brackish;
	case TEXT('g'): return EWRSwampWater::Gas;
	default: return EWRSwampWater::Dry;
	}
}

float FWRSwampData::BoundaryXAt(float Y) const
{
	if (Boundary.Num() == 0) return 1.0e9f;
	if (Y <= Boundary[0].X) return Boundary[0].Y;
	for (int32 i = 1; i < Boundary.Num(); ++i)
	{
		if (Y <= Boundary[i].X)
		{
			const float T = (Y - Boundary[i - 1].X) / FMath::Max(1.f, Boundary[i].X - Boundary[i - 1].X);
			return FMath::Lerp(Boundary[i - 1].Y, Boundary[i].Y, T);
		}
	}
	return Boundary.Last().Y;
}

const FWRSwampData::FSite* FWRSwampData::FindSite(FName Id) const
{
	for (const FSite& S : Sites) if (S.Id == Id) return &S;
	return nullptr;
}

bool WRSwamp::IsLive()
{
	return FWRSwampData::Get().bLoaded && WRBiome::IsLive(TEXT("swamp"));
}

bool WRSwamp::Contains(const FVector& Pos)
{
	if (!IsLive()) return false;
	const FWRSwampData& D = FWRSwampData::Get();
	if (!D.InFrame(Pos)) return false;
	if (Pos.Y >= WRTune(TEXT("world.islandNorthEdge"), -201600.f))
	{
		// South of the island's frame edge only the frost-bog neck is the swamp's (it lies over the island's sea).
		return D.ZoneAt(Pos) == TEXT('f');
	}
	return Pos.X > D.BoundaryXAt(Pos.Y);
}

EWRSwampWater WRSwamp::WaterAt(const UObject* WorldContext, const FVector& Pos)
{
	const FWRSwampData& D = FWRSwampData::Get();
	if (!D.bLoaded) return EWRSwampWater::Dry;
	const EWRSwampWater Base = D.BaseWaterAt(Pos);
	if (Base == EWRSwampWater::Murk || Base == EWRSwampWater::Duckweed)
	{
		const AWRSwampState* State = AWRSwampState::Get(WorldContext);
		if (State && State->IsTide())
		{
			const TCHAR Z = D.ZoneAt(Pos);
			// M3-M5 turn; M1 and M2 are brackish and salt kills leeches (cook 2.10).
			if (Z == TEXT('3') || Z == TEXT('4') || Z == TEXT('5')) return EWRSwampWater::Bleedwater;
		}
	}
	return Base;
}

bool WRSwamp::IsWet(const FVector& FeetPos)
{
	return FeetPos.Z < FWRSwampData::Get().SeaZ;
}

FVector WRSwamp::Point(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field)
{
	const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
	return Obj.IsValid() && Obj->TryGetArrayField(Field, A) ? WRSwampData::V3(*A) : FVector::ZeroVector;
}

TArray<FVector> WRSwamp::Points(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field)
{
	const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
	return Obj.IsValid() && Obj->TryGetArrayField(Field, A) ? WRSwampData::V3s(*A) : TArray<FVector>();
}

TSharedPtr<FJsonObject> WRSwamp::Arena(FName Id)
{
	const TSharedPtr<FJsonObject>* Found = FWRSwampData::Get().Arenas.Find(Id);
	return Found ? *Found : nullptr;
}

// ================================================================================================ state

AWRSwampState::AWRSwampState()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = false;
	SetNetUpdateFrequency(2.f);
}

void AWRSwampState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRSwampState, RedTide);
	DOREPLIFETIME(AWRSwampState, PhaseStartedAt);
	DOREPLIFETIME(AWRSwampState, TideEndsAt);
	DOREPLIFETIME(AWRSwampState, bHexWalk);
	DOREPLIFETIME(AWRSwampState, bReliquaryOpen);
}

AWRSwampState* AWRSwampState::Get(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World) return nullptr;
	for (TActorIterator<AWRSwampState> It(World); It; ++It) return *It;
	return nullptr;
}

AWRSwampState* AWRSwampState::GetOrSpawn(UWorld* World)
{
	if (!World || World->GetNetMode() == NM_Client) return Get(World);
	if (AWRSwampState* Existing = Get(World)) return Existing;
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<AWRSwampState>(FVector::ZeroVector, FRotator::ZeroRotator, P);
}
