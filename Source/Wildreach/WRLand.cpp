// Runtime lands (cooks/build-biome-void.md 2.1-2.3). The ground is a pure function of the world-map's metres: a base
// height from the distance to the Spine crest (the world-map's band rule), coast falloff into the sea, low noise, then
// the data's features (the Great Rift's hole, the Severance gorge, the viaduct's ravine, flattened courts, the Gauntlet
// Road's even grade). Every machine computes the same ground, so nothing about it is sent or saved; it is built in
// time-sliced chunks so no frame hitches.
#include "WRLand.h"
#include "WRVoidWorld.h"
#include "Wildreach.h"
#include "WRLandSites.h"
#include "WRLandThings.h"
#include "WRVoid.h"
#include "WRTuning.h"
#include "WRFarm.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "HAL/FileManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	float LNum(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, float Fallback)
	{
		double V = 0.0;
		return O.IsValid() && O->TryGetNumberField(Key, V) ? float(V) : Fallback;
	}

	TArray<FVector2D> LLine(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		TArray<FVector2D> Out;
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (O.IsValid() && O->TryGetArrayField(Key, A))
		{
			for (const TSharedPtr<FJsonValue>& V : *A) Out.Add(WRLand::Vec2(V));
		}
		return Out;
	}

	bool InPoly(const FVector2D& P, const TArray<FVector2D>& Poly)
	{
		bool bIn = false;
		for (int32 i = 0, j = Poly.Num() - 1; i < Poly.Num(); j = i++)
		{
			const FVector2D& A = Poly[i];
			const FVector2D& B = Poly[j];
			if (((A.Y > P.Y) != (B.Y > P.Y)) && (P.X < (B.X - A.X) * (P.Y - A.Y) / (B.Y - A.Y) + A.X)) bIn = !bIn;
		}
		return bIn;
	}

	float Smooth(float E0, float E1, float X)
	{
		if (FMath::IsNearlyEqual(E0, E1)) return X < E0 ? 0.f : 1.f;
		const float T = FMath::Clamp((X - E0) / (E1 - E0), 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	}

	uint32 HashI(int32 X, int32 Y, int32 Seed)
	{
		uint32 H = uint32(X) * 0x8da6b343u ^ uint32(Y) * 0xd8163841u ^ uint32(Seed) * 0xcb1ab31fu;
		H ^= H >> 13; H *= 0x5bd1e995u; H ^= H >> 15;
		return H;
	}

	TArray<FWRLandDef>& Registry()
	{
		static TArray<FWRLandDef> Lands;
		static bool bLoaded = false;
		if (!bLoaded)
		{
			bLoaded = true;
			const FString Dir = FPaths::Combine(FWRTuning::DataDir(), TEXT("lands"));
			TArray<FString> Files;
			IFileManager::Get().FindFiles(Files, *FPaths::Combine(Dir, TEXT("*.json")), true, false);
			Files.Sort();
			for (const FString& File : Files)
			{
				FString Text;
				TSharedPtr<FJsonObject> J;
				if (FFileHelper::LoadFileToString(Text, *FPaths::Combine(Dir, File)))
				{
					const TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Text);
					FJsonSerializer::Deserialize(R, J);
				}
				FWRLandDef Def;
				if (J.IsValid() && Def.Parse(J)) Lands.Add(MoveTemp(Def));
				else UE_LOG(LogWildreach, Error, TEXT("LAND could not load %s"), *File);
			}
		}
		return Lands;
	}
}

// ================================================================================================ helpers

FVector2D WRLand::Vec2(const TSharedPtr<FJsonValue>& V)
{
	const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
	if (V.IsValid() && V->TryGetArray(A) && A->Num() >= 2) return FVector2D((*A)[0]->AsNumber(), (*A)[1]->AsNumber());
	return FVector2D::ZeroVector;
}

FLinearColor WRLand::Colour(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, const FColor& Fallback)
{
	const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
	if (O.IsValid() && O->TryGetArrayField(Key, A) && A->Num() >= 3)
	{
		return FLinearColor(FColor(uint8((*A)[0]->AsNumber()), uint8((*A)[1]->AsNumber()), uint8((*A)[2]->AsNumber())));
	}
	return FLinearColor(Fallback);
}

float WRLand::DistToSegment(const FVector2D& P, const FVector2D& A, const FVector2D& B, float* OutT)
{
	const FVector2D AB = B - A;
	const float L2 = AB.SizeSquared();
	const float T = L2 > KINDA_SMALL_NUMBER ? FMath::Clamp(FVector2D::DotProduct(P - A, AB) / L2, 0.f, 1.f) : 0.f;
	if (OutT) *OutT = T;
	return FVector2D::Distance(P, A + AB * T);
}

float WRLand::DistToPolyline(const FVector2D& P, const TArray<FVector2D>& Line, int32* OutSeg, float* OutT)
{
	float Best = TNumericLimits<float>::Max();
	for (int32 i = 0; i + 1 < Line.Num(); ++i)
	{
		float T = 0.f;
		const float D = DistToSegment(P, Line[i], Line[i + 1], &T);
		if (D < Best)
		{
			Best = D;
			if (OutSeg) *OutSeg = i;
			if (OutT) *OutT = T;
		}
	}
	return Best;
}

// ================================================================================================ the definition

bool FWRLandZone::Contains(const FVector2D& M) const
{
	if (Road.Num() >= 2) return WRLand::DistToPolyline(M, Road) <= RoadWidth * 0.5f;
	for (const TArray<FVector2D>& P : Polys) if (InPoly(M, P)) return true;
	return false;
}

bool FWRLandDef::Parse(const TSharedPtr<FJsonObject>& J)
{
	Json = J;
	FString S;
	if (!J->TryGetStringField(TEXT("id"), S) || S.IsEmpty()) return false;
	Id = FName(*S);
	Biome = FName(*J->GetStringField(TEXT("biome")));
	J->TryGetStringField(TEXT("name"), Name);
	const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
	if (J->TryGetArrayField(TEXT("origin"), A) && A->Num() >= 3) Origin = FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), (*A)[2]->AsNumber());
	if (J->TryGetArrayField(TEXT("mapCentre"), A)) MapCentre = WRLand::Vec2(J->TryGetField(TEXT("mapCentre")));
	Seed = int32(LNum(J, TEXT("seed"), 1.f));
	Grid = FMath::Max(1.f, LNum(J, TEXT("grid"), 5.f));
	Chunk = FMath::Max(Grid * 4.f, LNum(J, TEXT("chunk"), 200.f));
	DrawDistance = LNum(J, TEXT("drawDistance"), 900000.f);
	ChunksPerFrame = FMath::Max(1, int32(LNum(J, TEXT("chunksPerFrame"), 3.f)));
	Crest = LLine(J, TEXT("crest"));
	HeightCurve = LLine(J, TEXT("heightCurve"));
	const TSharedPtr<FJsonObject>* N = nullptr;
	if (J->TryGetObjectField(TEXT("noise"), N))
	{
		NoiseScale = FMath::Max(1.f, LNum(*N, TEXT("scale"), 180.f));
		NoiseAmp = LNum(*N, TEXT("amp"), 4.f);
		FineScale = FMath::Max(1.f, LNum(*N, TEXT("fineScale"), 55.f));
		FineAmp = LNum(*N, TEXT("fineAmp"), 1.f);
	}
	Coast = LLine(J, TEXT("coast"));
	CoastBeach = LNum(J, TEXT("coastBeach"), 90.f);
	CoastSea = LNum(J, TEXT("coastSea"), 140.f);
	SeaFloor = LNum(J, TEXT("seaFloor"), -12.f);
	for (const FVector2D& P : Coast) MapBox += P;
	if (J->TryGetObjectField(TEXT("slopeRock"), N))
	{
		SlopeRock = WRLand::Colour(*N, TEXT("color"), FColor(104, 100, 95));
		SlopeFrom = LNum(*N, TEXT("fromDeg"), 30.f);
		SlopeTo = LNum(*N, TEXT("toDeg"), 44.f);
	}
	if (J->TryGetArrayField(TEXT("zones"), A))
	{
		for (const TSharedPtr<FJsonValue>& V : *A)
		{
			const TSharedPtr<FJsonObject> Z = V->AsObject();
			if (!Z.IsValid()) continue;
			FWRLandZone& Zone = Zones.AddDefaulted_GetRef();
			Zone.Id = FName(*Z->GetStringField(TEXT("id")));
			Z->TryGetStringField(TEXT("name"), Zone.Name);
			Zone.Road = LLine(Z, TEXT("road"));
			Zone.RoadWidth = LNum(Z, TEXT("width"), 80.f);
			const TArray<TSharedPtr<FJsonValue>>* Polys = nullptr;
			if (Z->TryGetArrayField(TEXT("polys"), Polys))
			{
				for (const TSharedPtr<FJsonValue>& PV : *Polys)
				{
					TArray<FVector2D>& Poly = Zone.Polys.AddDefaulted_GetRef();
					const TArray<TSharedPtr<FJsonValue>>* Pts = nullptr;
					if (PV->TryGetArray(Pts)) for (const TSharedPtr<FJsonValue>& Pt : *Pts) Poly.Add(WRLand::Vec2(Pt));
				}
			}
			Zone.Ground = WRLand::Colour(Z, TEXT("ground"), FColor(150, 146, 140));
			Zone.GroundAlt = WRLand::Colour(Z, TEXT("groundAlt"), FColor(150, 146, 140));
			Zone.Rough = LNum(Z, TEXT("rough"), 0.9f);
			FString Str;
			if (Z->TryGetStringField(TEXT("region"), Str)) Zone.Region = FName(*Str);
			if (Z->TryGetStringField(TEXT("bed"), Str)) Zone.Bed = FName(*Str);
			const TSharedPtr<FJsonObject>* D = nullptr;
			if (Z->TryGetObjectField(TEXT("decor"), D)) Zone.Decor = *D;
		}
	}
	// Features last: their levels read the base ground (crest + noise + coast) at their own points.
	if (J->TryGetArrayField(TEXT("features"), A))
	{
		for (const TSharedPtr<FJsonValue>& V : *A)
		{
			const TSharedPtr<FJsonObject> F = V->AsObject();
			if (!F.IsValid()) continue;
			FWRLandFeature& Fe = Features.AddDefaulted_GetRef();
			Fe.Kind = FName(*F->GetStringField(TEXT("kind")));
			FString Str;
			if (F->TryGetStringField(TEXT("id"), Str)) Fe.Id = FName(*Str);
			Fe.A = WRLand::Vec2(F->TryGetField(TEXT("at")).IsValid() ? F->TryGetField(TEXT("at")) : F->TryGetField(TEXT("a")));
			Fe.B = WRLand::Vec2(F->TryGetField(TEXT("b")));
			Fe.Points = LLine(F, TEXT("points"));
			Fe.R = LNum(F, TEXT("r"), 0.f);
			Fe.Width = LNum(F, TEXT("width"), 0.f);
			Fe.Depth = LNum(F, TEXT("depth"), 0.f);
			Fe.Walls = LNum(F, TEXT("walls"), 20.f);
			Fe.Blend = LNum(F, TEXT("blend"), 30.f);
			Fe.LipDrop = LNum(F, TEXT("lipDrop"), 0.f);
			if (Fe.Kind == TEXT("flatten"))
			{
				Fe.Levels.Add(F->HasField(TEXT("level")) ? LNum(F, TEXT("level"), 0.f) : BaseHeight(Fe.A));
			}
			else if (Fe.Kind == TEXT("road"))
			{
				// An even grade between the road's points; each point at the ground's own height there.
				for (const FVector2D& P : Fe.Points) Fe.Levels.Add(BaseHeight(P));
			}
			else if (Fe.Kind == TEXT("gorge"))
			{
				const float FA = BaseHeight(Fe.A) - Fe.Depth;
				Fe.Levels = { FA, FMath::Min(FA, BaseHeight(Fe.B) - Fe.Depth) };
			}
			else if (Fe.Kind == TEXT("hole"))
			{
				// The lip: the ring's own ground, so the walls start where the ground ends.
				float Sum = 0.f;
				for (int32 i = 0; i < 16; ++i)
				{
					const float Ang = 2.f * PI * i / 16.f;
					Sum += BaseHeight(Fe.A + FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * (Fe.R + 10.f));
				}
				Fe.Levels.Add(Sum / 16.f);
			}
		}
	}
	// Each hollow-crust patch is a pocket in the ground under it (WRVoidHazards lays the crust over).
	if (J->TryGetArrayField(TEXT("crust"), A))
	{
		for (const TSharedPtr<FJsonValue>& V : *A)
		{
			const TSharedPtr<FJsonObject> C = V->AsObject();
			if (!C.IsValid()) continue;
			FWRLandFeature& Fe = Features.AddDefaulted_GetRef();
			Fe.Kind = TEXT("pit");
			Fe.A = WRLand::Vec2(C->TryGetField(TEXT("at")));
			Fe.R = LNum(C, TEXT("r"), 8.f);
			Fe.Depth = LNum(C, TEXT("depth"), 5.f);
		}
	}
	return Coast.Num() >= 3 && HeightCurve.Num() >= 2;
}

bool FWRLandDef::Contains(const FVector& W, float MarginM) const
{
	const FVector2D M = ToMap(W);
	return M.X >= MapBox.Min.X - MarginM && M.X <= MapBox.Max.X + MarginM && M.Y >= MapBox.Min.Y - MarginM && M.Y <= MapBox.Max.Y + MarginM;
}

TSharedPtr<FJsonObject> FWRLandDef::Section(const TCHAR* Key) const
{
	const TSharedPtr<FJsonObject>* O = nullptr;
	return Json.IsValid() && Json->TryGetObjectField(Key, O) ? *O : nullptr;
}

float FWRLandDef::Noise(const FVector2D& P) const
{
	const int32 X0 = FMath::FloorToInt(P.X), Y0 = FMath::FloorToInt(P.Y);
	const float FX = P.X - X0, FY = P.Y - Y0;
	const auto V = [this](int32 X, int32 Y) { return float(HashI(X, Y, Seed) & 0xffff) / 32767.5f - 1.f; };
	const float SX = FX * FX * (3.f - 2.f * FX), SY = FY * FY * (3.f - 2.f * FY);
	return FMath::Lerp(FMath::Lerp(V(X0, Y0), V(X0 + 1, Y0), SX), FMath::Lerp(V(X0, Y0 + 1), V(X0 + 1, Y0 + 1), SX), SY);
}

float FWRLandDef::CoastDistance(const FVector2D& M) const
{
	float Best = TNumericLimits<float>::Max();
	for (int32 i = 0, j = Coast.Num() - 1; i < Coast.Num(); j = i++) Best = FMath::Min(Best, WRLand::DistToSegment(M, Coast[j], Coast[i]));
	return InPoly(M, Coast) ? Best : -Best;
}

float FWRLandDef::BaseHeight(const FVector2D& M) const
{
	// The Spine: height by distance from its crest, eased between the curve's keys.
	const float D = Crest.Num() >= 2 ? WRLand::DistToPolyline(M, Crest) : 1000.f;
	float H = HeightCurve.Last().Y;
	for (int32 i = 0; i + 1 < HeightCurve.Num(); ++i)
	{
		if (D <= HeightCurve[i + 1].X)
		{
			H = FMath::Lerp(HeightCurve[i].Y, HeightCurve[i + 1].Y, Smooth(HeightCurve[i].X, HeightCurve[i + 1].X, D));
			break;
		}
	}
	if (D < HeightCurve[0].X) H = HeightCurve[0].Y;
	// Low ground rolls; the highland is rougher (the same noise, stronger with height).
	const float Rough = 1.f + FMath::Clamp(H / 150.f, 0.f, 1.5f);
	H += Noise(M / NoiseScale) * NoiseAmp * Rough + Noise(M / FineScale + FVector2D(37.1f, 11.7f)) * FineAmp;
	// The coast: a beach down to a metre over the sea, then the sea floor.
	const float S = CoastDistance(M);
	if (S < CoastBeach)
	{
		H = S >= 0.f ? FMath::Lerp(1.0f, H, Smooth(0.f, CoastBeach, S)) : FMath::Lerp(1.0f, SeaFloor, Smooth(0.f, CoastSea, -S));
	}
	return H;
}

float FWRLandDef::Height(const FVector2D& M) const
{
	float H = BaseHeight(M);
	for (const FWRLandFeature& F : Features)
	{
		if (F.Kind == TEXT("flatten"))
		{
			const float D = FVector2D::Distance(M, F.A);
			if (D < F.R + F.Blend) H = FMath::Lerp(H, F.Levels[0], 1.f - Smooth(F.R, F.R + F.Blend, D));
		}
		else if (F.Kind == TEXT("road") && F.Points.Num() >= 2)
		{
			int32 Seg = 0;
			float T = 0.f;
			const float D = WRLand::DistToPolyline(M, F.Points, &Seg, &T);
			const float Half = F.Width * 0.5f;
			if (D < Half + F.Blend) H = FMath::Lerp(H, FMath::Lerp(F.Levels[Seg], F.Levels[Seg + 1], T), 1.f - Smooth(Half, Half + F.Blend, D));
		}
		else if (F.Kind == TEXT("ravine"))
		{
			const float D = WRLand::DistToSegment(M, F.A, F.B);
			if (D < F.Width * 0.5f) H -= F.Depth * (1.f - Smooth(F.Width * 0.16f, F.Width * 0.5f, D));
		}
		else if (F.Kind == TEXT("gorge"))
		{
			float T = 0.f;
			const float D = WRLand::DistToSegment(M, F.A, F.B, &T);
			const float Half = F.Width * 0.5f;
			if (D < Half + F.Walls) H = FMath::Lerp(H, FMath::Min(H, FMath::Lerp(F.Levels[0], F.Levels[1], T)), 1.f - Smooth(Half, Half + F.Walls, D));
		}
		else if (F.Kind == TEXT("pit"))
		{
			// A pocket under hollow crust: steep sides, its floor Depth down (the crust lies over it: WRVoidHazards).
			const float D = FVector2D::Distance(M, F.A);
			if (D < F.R) H -= F.Depth * (1.f - Smooth(F.R - 1.5f, F.R, D));
		}
		else if (F.Kind == TEXT("hole"))
		{
			// The lip breaks off: the last metres before the edge drop a little (a worn rim), so it reads from afar.
			const float D = FVector2D::Distance(M, F.A);
			if (D < F.R + 12.f) H = FMath::Lerp(F.Levels[0] - F.LipDrop, H, Smooth(F.R, F.R + 12.f, D));
		}
	}
	return H;
}

const FWRLandFeature* FWRLandDef::HoleAt(const FVector2D& M, float Grow) const
{
	for (const FWRLandFeature& F : Features)
	{
		if (F.Kind == TEXT("hole") && FVector2D::Distance(M, F.A) < F.R + Grow) return &F;
	}
	return nullptr;
}

const FWRLandFeature* FWRLandDef::Feature(FName FeatureId) const
{
	return Features.FindByPredicate([FeatureId](const FWRLandFeature& F) { return F.Id == FeatureId; });
}

const FWRLandZone* FWRLandDef::ZoneAt(const FVector2D& M) const
{
	for (const FWRLandZone& Z : Zones) if (Z.Contains(M)) return &Z;
	return Zones.Num() ? &Zones.Last() : nullptr;
}

FLinearColor FWRLandDef::Ground(const FVector2D& M, const FVector& N, float& OutRough) const
{
	const FWRLandZone* Z = ZoneAt(M);
	FLinearColor C = Z ? Z->Ground : FLinearColor::Gray;
	OutRough = Z ? Z->Rough : 0.9f;
	if (Z)
	{
		// Patches of the zone's second ground (needle litter, salt crust behind the black sand, worn dust) by noise.
		const float P = Smooth(-0.1f, 0.35f, Noise(M / 38.f + FVector2D(5.3f, 2.9f)) + Noise(M / 11.f) * 0.25f);
		if (Z->Id == TEXT("V2"))
		{
			// The Grey Strand: black glass sand at the water, the salt pans' white crust only well behind it.
			const float Behind = Smooth(CoastBeach * 0.6f, CoastBeach * 2.2f, CoastDistance(M));
			C = FMath::Lerp(Z->Ground, Z->GroundAlt, P * Behind);
			OutRough = FMath::Lerp(0.35f, 0.92f, Behind);
		}
		else
		{
			C = FMath::Lerp(Z->Ground, Z->GroundAlt, P);
		}
	}
	// Glass rings round the rifts, the Glass Sea, the burned stand: from the rift list and the POIs.
	if (const TSharedPtr<FJsonObject> Rifts = Section(TEXT("rifts")))
	{
		const TArray<TSharedPtr<FJsonValue>>* L = nullptr;
		if (Rifts->TryGetArrayField(TEXT("list"), L))
		{
			for (const TSharedPtr<FJsonValue>& V : *L)
			{
				const TSharedPtr<FJsonObject> R = V->AsObject();
				const FVector2D At = WRLand::Vec2(R->TryGetField(TEXT("at")));
				const bool bLarge = R->GetStringField(TEXT("size")) == TEXT("large");
				const float Ring = bLarge ? 9.f : 5.f;
				const float D = FVector2D::Distance(M, At);
				if (D < Ring + 6.f)
				{
					const float G = 1.f - Smooth(Ring - 1.5f, Ring + 6.f, D + Noise(M / 3.f) * 1.5f);
					C = FMath::Lerp(C, FLinearColor(FColor(11, 10, 16)), G);
					OutRough = FMath::Lerp(OutRough, 0.12f, G);
				}
				else if (D < 60.f)
				{
					// Grey grass inside the zone stands straight; the dust is paler there (the void draws the colour out).
					C = FMath::Lerp(C, C * 1.08f, (1.f - Smooth(20.f, 60.f, D)) * 0.6f);
				}
			}
		}
	}
	if (const FWRLandFeature* Glass = Feature(TEXT("glass_sea")))
	{
		const float D = FVector2D::Distance(M, Glass->A);
		if (D < Glass->R + 8.f)
		{
			const float G = 1.f - Smooth(Glass->R - 6.f, Glass->R + 8.f, D + Noise(M / 6.f) * 4.f);
			C = FMath::Lerp(C, FLinearColor(FColor(12, 11, 17)), G);
			OutRough = FMath::Lerp(OutRough, 0.08f, G);
		}
	}
	if (const TArray<TSharedPtr<FJsonValue>>* Pois = nullptr; Json->TryGetArrayField(TEXT("pois"), Pois))
	{
		for (const TSharedPtr<FJsonValue>& V : *Pois)
		{
			const TSharedPtr<FJsonObject> P = V->AsObject();
			if (P->GetStringField(TEXT("kind")) != TEXT("burned_stand")) continue;
			const FVector2D At = WRLand::Vec2(P->TryGetField(TEXT("at")));
			const float R = LNum(P, TEXT("r"), 100.f);
			const float D = FVector2D::Distance(M, At);
			if (D < R + 20.f)
			{
				C = FMath::Lerp(C, FLinearColor(FColor(40, 38, 37)), 1.f - Smooth(R - 15.f, R + 20.f, D + Noise(M / 9.f) * 10.f));
			}
		}
	}
	// Under the water: the sea floor darkens and smooths.
	const float S = CoastDistance(M);
	if (S < 0.f) C = FMath::Lerp(C, FLinearColor(FColor(30, 31, 34)), Smooth(0.f, 30.f, -S));
	// Steep ground is bare rock.
	const float Slope = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(N.Z, -1.f, 1.f)));
	const float Rock = Smooth(SlopeFrom, SlopeTo, Slope);
	C = FMath::Lerp(C, SlopeRock * (0.85f + 0.3f * (Noise(M / 7.f) * 0.5f + 0.5f)), Rock);
	OutRough = FMath::Lerp(OutRough, 0.86f, Rock);
	return C;
}

// ================================================================================================ the registry

const TArray<FWRLandDef>& WRLand::All()
{
	return Registry();
}

bool WRLand::IsLive(const FWRLandDef& Def)
{
	return WRBiome::IsLive(Def.Biome);
}

const FWRLandDef* WRLand::Find(FName Id)
{
	return Registry().FindByPredicate([Id](const FWRLandDef& D) { return D.Id == Id; });
}

const FWRLandDef* WRLand::At(const FVector& World)
{
	for (const FWRLandDef& D : Registry())
	{
		if (D.Contains(World) && IsLive(D)) return &D;
	}
	return nullptr;
}

FName WRLand::BiomeAt(const FVector& World)
{
	const FWRLandDef* D = At(World);
	return D ? D->Biome : NAME_None;
}

bool WRLand::PlaceOn(FName LandId, const FVector2D& MapM, FVector& OutWorld)
{
	const FWRLandDef* D = Find(LandId);
	if (!D || !IsLive(*D)) return false;
	OutWorld = D->GroundPoint(MapM);
	return true;
}

float WRLand::GroundZAt(const FVector& World, float Fallback)
{
	const FWRLandDef* D = At(World);
	if (!D) return Fallback;
	const FVector2D M = D->ToMap(World);
	if (D->HoleAt(M)) return Fallback;
	return D->Origin.Z + D->Height(M) * 100.f;
}

FName WRLand::ZoneAt(const FVector& World)
{
	const FWRLandDef* D = At(World);
	const FWRLandZone* Z = D ? D->ZoneAt(D->ToMap(World)) : nullptr;
	return Z ? Z->Id : NAME_None;
}

bool WRLand::InHole(const FVector& World)
{
	const FWRLandDef* D = At(World);
	if (!D) return false;
	const FVector2D M = D->ToMap(World);
	const FWRLandFeature* H = D->HoleAt(M, 2.f);
	return H && World.Z < D->ToWorld(M, H->Levels[0]).Z + 200.f;
}

// ================================================================================================ the subsystem

bool UWRLandSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UWRLandSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// Lands sit in the island's sea: only the island's world holds them (test arenas and stages don't).
	FString Map = InWorld.GetMapName();
	Map.RemoveFromStart(InWorld.StreamingLevelsPrefix);
	if (!Map.Contains(TEXT("Island"))) return;
	for (const FWRLandDef& Def : WRLand::All())
	{
		if (!WRLand::IsLive(Def)) continue;
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		P.bDeferConstruction = true;
		AWRLand* Land = InWorld.SpawnActor<AWRLand>(AWRLand::StaticClass(), FTransform(Def.Origin), P);
		if (!Land) continue;
		Land->LandId = Def.Id;
		Land->FinishSpawning(FTransform(Def.Origin));
		Lands.Add(Land);
		UE_LOG(LogWildreach, Log, TEXT("LAND %s (%s) building at %s"), *Def.Id.ToString(), *Def.Biome.ToString(), *Def.Origin.ToCompactString());
	}
	// The server's live things (the crossings, rifts, machines): replicated, so only it spawns them.
	if (InWorld.GetNetMode() != NM_Client && Lands.Num()) WRLandThings::Start(&InWorld);
	WRVoid::Start(&InWorld);
}

AWRLand* UWRLandSubsystem::FindLand(const UWorld* World, FName Id)
{
	const UWRLandSubsystem* S = World ? World->GetSubsystem<UWRLandSubsystem>() : nullptr;
	if (!S) return nullptr;
	for (const TWeakObjectPtr<AWRLand>& L : S->Lands)
	{
		if (L.IsValid() && (Id.IsNone() || L->LandId == Id)) return L.Get();
	}
	return nullptr;
}

// ================================================================================================ the land actor

AWRLand::AWRLand()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Static);
}

UMaterialInterface* AWRLand::GroundMaterial() const { return GroundMat; }
UMaterialInterface* AWRLand::TearMaterial() const { return TearMat; }
UMaterialInterface* AWRLand::LightMaterial() const { return LightMat; }

double AWRLand::ServerTime() const
{
	const UWorld* W = GetWorld();
	const AGameStateBase* GS = W ? W->GetGameState() : nullptr;
	return GS ? GS->GetServerWorldTimeSeconds() : (W ? W->GetTimeSeconds() : 0.0);
}

void AWRLand::BeginPlay()
{
	Super::BeginPlay();
	GroundMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_LandGround.M_LandGround"));
	TearMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_VoidTear.M_VoidTear"));
	LightMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_VoidLight.M_VoidLight"));
	if (!GroundMat) UE_LOG(LogWildreach, Error, TEXT("LAND %s: /Game/Terrain/M_LandGround is missing (Tools/terrain/build_void_materials.py)"), *LandId.ToString());
	QueueChunks();
}

float AWRLand::GroundZ(const FVector& World) const
{
	const FWRLandDef* D = Def();
	return D ? D->ToWorld(D->ToMap(World), D->Height(D->ToMap(World))).Z : World.Z;
}

void AWRLand::QueueChunks()
{
	const FWRLandDef* D = Def();
	if (!D) return;
	const FBox2D Box(D->MapBox.Min - FVector2D(D->CoastSea), D->MapBox.Max + FVector2D(D->CoastSea));
	const int32 NX = FMath::CeilToInt((Box.Max.X - Box.Min.X) / D->Chunk);
	const int32 NY = FMath::CeilToInt((Box.Max.Y - Box.Min.Y) / D->Chunk);
	for (int32 y = 0; y < NY; ++y) for (int32 x = 0; x < NX; ++x) Queue.Add(FIntPoint(x, y));
	// Nearest the land's middle first: the camps and the crossing's cove come in before the far sea floor.
	const FVector2D Mid = D->MapBox.GetCenter();
	const FVector2D Min = Box.Min;
	const float C = D->Chunk;
	Queue.Sort([&](const FIntPoint& A, const FIntPoint& B)
	{
		return FVector2D::DistSquared(Min + FVector2D(A.X + 0.5f, A.Y + 0.5f) * C, Mid) > FVector2D::DistSquared(Min + FVector2D(B.X + 0.5f, B.Y + 0.5f) * C, Mid);
	});
}

void AWRLand::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const FWRLandDef* D = Def();
	if (!D) return;
	if (!bGroundDone)
	{
		for (int32 n = 0; n < D->ChunksPerFrame && Queue.Num(); ++n)
		{
			BuildChunk(Queue.Pop(EAllowShrinking::No));
		}
		if (Queue.Num() == 0)
		{
			BuildHoles();
			bGroundDone = true;
			UE_LOG(LogWildreach, Log, TEXT("LAND %s ground done: %d chunks"), *LandId.ToString(), Built);
		}
		return;
	}
	if (!bSitesDone)
	{
		bSitesDone = true;
		WRLandSites::Build(this);
		// The server's rifts sit on the places just made (the kit pass's rift rings among them).
		if (GetNetMode() != NM_Client) WRVoid::SpawnRifts(GetWorld(), LandId);
		// Its gathering, keepsakes and the rest of the land's live things (WRVoidWorld; the server's nodes, every
		// machine's looks).
		WRVoidWorld::LandReady(this);
		return;
	}
	// The floating things: each one's place from the server's clock (never set from a previous frame).
	const double T = ServerTime();
	for (int32 i = Movers.Num() - 1; i >= 0; --i)
	{
		const FWRLandMover& M = Movers[i];
		USceneComponent* C = M.Comp.Get();
		if (!C) { Movers.RemoveAtSwap(i); continue; }
		const float B = M.Bob * FMath::Sin(2.f * PI * (float(FMath::Fmod(T, double(M.BobPeriod))) / M.BobPeriod + M.Phase));
		const float S = FMath::Sin(2.f * PI * (float(FMath::Fmod(T, double(M.SwayPeriod))) / M.SwayPeriod + M.Phase * 0.37f));
		const FRotator R = M.Spin != 0.f ? M.Rot + FRotator(0.f, float(FMath::Fmod(T * M.Spin, 360.0)), 0.f) : M.Rot;
		C->SetRelativeLocationAndRotation(M.Base + FVector(0.f, 0.f, B) + M.Sway * S, R);
	}
}

bool AWRLand::BuildChunk(const FIntPoint& Cell)
{
	const FWRLandDef& D = *Def();
	const FVector2D Min = D.MapBox.Min - FVector2D(D.CoastSea) + FVector2D(Cell.X, Cell.Y) * D.Chunk;
	const int32 N = FMath::RoundToInt(D.Chunk / D.Grid);   // quads per side
	const int32 W = N + 3;                                    // heights with a one-cell border for normals
	TArray<float> H;
	H.SetNumUninitialized(W * W);
	float Top = -1e6f;
	for (int32 j = 0; j < W; ++j)
	{
		for (int32 i = 0; i < W; ++i)
		{
			const float Z = D.Height(Min + FVector2D(i - 1, j - 1) * D.Grid);
			H[j * W + i] = Z;
			Top = FMath::Max(Top, Z);
		}
	}
	// Open sea floor far from any coast: the sea covers it, so it isn't built.
	if (Top < D.SeaFloor + 1.5f) return false;
	const FWRLandFeature* Hole = nullptr;
	for (const FWRLandFeature& F : D.Features)
	{
		if (F.Kind == TEXT("hole") && FBox2D(Min - FVector2D(F.R), Min + FVector2D(D.Chunk + F.R)).IsInside(F.A)) Hole = &F;
	}
	TArray<FVector> Verts;
	TArray<FVector> Normals;
	TArray<FLinearColor> Colours;
	TArray<FVector2D> UV;
	TArray<FProcMeshTangent> Tangents;
	TArray<int32> Tris;
	TArray<uint8> InHole;
	const int32 V = N + 1;
	Verts.Reserve(V * V);
	for (int32 j = 0; j < V; ++j)
	{
		for (int32 i = 0; i < V; ++i)
		{
			FVector2D M = Min + FVector2D(i, j) * D.Grid;
			float Z = H[(j + 1) * W + (i + 1)];
			bool bHole = false;
			if (Hole && FVector2D::Distance(M, Hole->A) < Hole->R)
			{
				// Points inside the hole move out to its edge: the ground ends on a clean circle where the walls begin.
				M = Hole->A + (M - Hole->A).GetSafeNormal() * Hole->R;
				Z = D.Height(M);
				bHole = true;
			}
			const float DX = H[(j + 1) * W + (i + 2)] - H[(j + 1) * W + i];
			const float DY = H[(j + 2) * W + (i + 1)] - H[j * W + (i + 1)];
			const FVector Nrm = FVector(-DX, -DY, 2.f * D.Grid).GetSafeNormal();
			float Rough = 0.9f;
			FLinearColor C = D.Ground(M, Nrm, Rough);
			C.A = 1.f - Rough;
			Verts.Add(D.ToWorld(M, Z) - D.Origin);
			Normals.Add(Nrm);
			Colours.Add(C);
			UV.Add(M / 4.f);
			Tangents.Add(FProcMeshTangent(FVector(1.f, 0.f, 0.f).GetSafeNormal(), false));
			InHole.Add(bHole ? 1 : 0);
		}
	}
	Tris.Reserve(N * N * 6);
	for (int32 j = 0; j < N; ++j)
	{
		for (int32 i = 0; i < N; ++i)
		{
			const int32 A = j * V + i, B = A + 1, C = A + V, E = C + 1;
			// A quad wholly in the hole is gone; one on its edge keeps its outer corners.
			if (InHole[A] && InHole[B] && InHole[C] && InHole[E]) continue;
			if (!(InHole[A] && InHole[E] && InHole[C])) { Tris.Add(A); Tris.Add(C); Tris.Add(E); }
			if (!(InHole[A] && InHole[B] && InHole[E])) { Tris.Add(A); Tris.Add(E); Tris.Add(B); }
		}
	}
	if (Tris.Num() == 0) return false;
	UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this);
	Mesh->SetupAttachment(RootComponent);
	Mesh->bUseAsyncCooking = true;
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	Mesh->SetCollisionObjectType(ECC_WorldStatic);
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->LDMaxDrawDistance = D.DrawDistance;
	Mesh->SetCachedMaxDrawDistance(D.DrawDistance);
	Mesh->CreateMeshSection_LinearColor(0, Verts, Tris, Normals, UV, Colours, Tangents, true);
	if (GroundMat) Mesh->SetMaterial(0, GroundMat);
	Mesh->RegisterComponent();
	Chunks.Add(Mesh);
	++Built;
	return true;
}

void AWRLand::BuildHoles()
{
	const FWRLandDef& D = *Def();
	for (const FWRLandFeature& F : D.Features)
	{
		if (F.Kind != TEXT("hole")) continue;
		// The walls: fractured rock falling from the lip to the black, a ring of 160 columns, rows every 8 m, the face
		// pushed in and out by noise (overhangs near the top, a slow flare below) so it reads as torn ground, not a pipe.
		const int32 Cols = 160;
		const int32 Rows = FMath::Max(4, FMath::RoundToInt(F.Depth / 8.f));
		TArray<FVector> Verts;
		TArray<FVector> Normals;
		TArray<FLinearColor> Colours;
		TArray<FVector2D> UV;
		TArray<FProcMeshTangent> Tangents;
		TArray<int32> Tris;
		for (int32 r = 0; r <= Rows; ++r)
		{
			const float Down = F.Depth * r / Rows;
			for (int32 c = 0; c <= Cols; ++c)
			{
				const float Ang = 2.f * PI * (c % Cols) / Cols;
				const FVector2D Dir(FMath::Cos(Ang), FMath::Sin(Ang));
				const FVector2D Edge = F.A + Dir * F.R;
				const float Top = D.Height(Edge);
				const float Push = r == 0 ? 0.f : D.Noise(FVector2D(Ang * 9.f, Down / 14.f) + FVector2D(3.f, 7.f)) * 5.f + D.Noise(FVector2D(Ang * 31.f, Down / 5.f)) * 1.6f + (Down / F.Depth) * 6.f;
				const FVector2D P = F.A + Dir * (F.R + Push);
				Verts.Add(D.ToWorld(P, Top - Down) - D.Origin);
				Normals.Add(FVector(-Dir.X, -Dir.Y, 0.f));
				const float Band = 0.8f + 0.25f * D.Noise(FVector2D(Ang * 20.f, Down / 3.f));
				FLinearColor C = FLinearColor(FColor(78, 74, 72)) * Band;
				// Voidglass seams near the top (the ring's glass runs down the face), darker and wetter-looking deeper.
				C = FMath::Lerp(C, FLinearColor(FColor(14, 12, 20)), FMath::Clamp(1.f - Down / 14.f, 0.f, 1.f) * 0.85f);
				C = FMath::Lerp(C, FLinearColor(FColor(18, 16, 22)), FMath::Clamp(Down / F.Depth, 0.f, 1.f) * 0.7f);
				C.A = r < 2 ? 0.8f : 0.15f;
				Colours.Add(C);
				UV.Add(FVector2D(c * 2.f, Down / 4.f));
				Tangents.Add(FProcMeshTangent(FVector(-Dir.Y, Dir.X, 0.f), false));
			}
		}
		for (int32 r = 0; r < Rows; ++r)
		{
			for (int32 c = 0; c < Cols; ++c)
			{
				const int32 A = r * (Cols + 1) + c, B = A + 1, C = A + Cols + 1, E = C + 1;
				Tris.Append({ A, B, E, A, E, C });
			}
		}
		UProceduralMeshComponent* Wall = NewObject<UProceduralMeshComponent>(this);
		Wall->SetupAttachment(RootComponent);
		Wall->bUseAsyncCooking = true;
		Wall->SetCollisionProfileName(TEXT("BlockAll"));
		Wall->SetCollisionObjectType(ECC_WorldStatic);
		Wall->SetCanEverAffectNavigation(false);
		Wall->LDMaxDrawDistance = D.DrawDistance;
		Wall->CreateMeshSection_LinearColor(0, Verts, Tris, Normals, UV, Colours, Tangents, true);
		if (GroundMat) Wall->SetMaterial(0, GroundMat);
		Wall->RegisterComponent();
		Chunks.Add(Wall);
		// The black at the bottom: a disc of the tear's starless dark, no collision (falling past the lip is the end).
		TArray<FVector> DV;
		TArray<int32> DT;
		TArray<FVector> DN;
		const float FloorZ = F.Levels[0] - F.Depth;
		DV.Add(D.ToWorld(F.A, FloorZ) - D.Origin);
		DN.Add(FVector::UpVector);
		for (int32 c = 0; c <= 64; ++c)
		{
			const float Ang = 2.f * PI * c / 64.f;
			DV.Add(D.ToWorld(F.A + FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * (F.R + 16.f), FloorZ) - D.Origin);
			DN.Add(FVector::UpVector);
			if (c > 0) DT.Append({ 0, c + 1, c });
		}
		UProceduralMeshComponent* Floor = NewObject<UProceduralMeshComponent>(this);
		Floor->SetupAttachment(RootComponent);
		Floor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Floor->LDMaxDrawDistance = D.DrawDistance;
		Floor->CreateMeshSection_LinearColor(0, DV, DT, DN, TArray<FVector2D>(), TArray<FLinearColor>(), TArray<FProcMeshTangent>(), false);
		if (TearMat)
		{
			UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(TearMat, this);
			M->SetScalarParameterValue(TEXT("Glow"), 0.35f);
			M->SetScalarParameterValue(TEXT("Stars"), 0.f);
			Floor->SetMaterial(0, M);
		}
		Floor->RegisterComponent();
		Chunks.Add(Floor);
	}
}
