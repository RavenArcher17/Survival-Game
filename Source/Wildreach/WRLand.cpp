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
#include "WRSocial.h"
#include "WRCrafting.h"
#include "WRSurvival.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "HAL/FileManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Compression.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
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

	FString& CurrentMapRef()
	{
		static FString Map;
		return Map;
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

// A heightfield land's parsing (the Ancient forest, cooks/build-biome-forest.md 2.1-2.3): its samples file, zone grid,
// surface classes, colour bands and underground places. Named apart from the anonymous helpers above (unity builds).
namespace WRLandHfImpl
{
	/** "layers": { "litter": .., "moss": .., "soil": .., "grass": .., "stones": .., "sand": .. } into the six weights. */
	bool LLayers(const TSharedPtr<FJsonObject>& O, float Out[6])
	{
		const TSharedPtr<FJsonObject>* L = nullptr;
		if (!O.IsValid() || !O->TryGetObjectField(TEXT("layers"), L)) return false;
		static const TCHAR* Names[6] = { TEXT("litter"), TEXT("moss"), TEXT("soil"), TEXT("grass"), TEXT("stones"), TEXT("sand") };
		float Sum = 0.f;
		for (int32 k = 0; k < 6; ++k)
		{
			double V = 0.0;
			(*L)->TryGetNumberField(Names[k], V);
			Out[k] = FMath::Max(0.f, float(V));
			Sum += Out[k];
		}
		if (Sum <= 0.f) return false;
		for (int32 k = 0; k < 6; ++k) Out[k] /= Sum;
		return true;
	}

	void LSpeck(const TSharedPtr<FJsonObject>& O, TArray<FLinearColor>& OutColours, float& OutAmount, float& OutScale)
	{
		const TSharedPtr<FJsonObject>* S = nullptr;
		if (!O.IsValid() || !O->TryGetObjectField(TEXT("speck"), S)) return;
		const TArray<TSharedPtr<FJsonValue>>* Cols = nullptr;
		if ((*S)->TryGetArrayField(TEXT("colors"), Cols))
		{
			for (const TSharedPtr<FJsonValue>& V : *Cols)
			{
				const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
				if (V->TryGetArray(A) && A->Num() >= 3) OutColours.Add(FLinearColor(FColor(uint8((*A)[0]->AsNumber()), uint8((*A)[1]->AsNumber()), uint8((*A)[2]->AsNumber()))));
			}
		}
		OutAmount = LNum(*S, TEXT("amount"), 0.f);
		OutScale = FMath::Max(0.5f, LNum(*S, TEXT("scale"), 5.f));
	}

	void ParseHeightfield(FWRLandDef& D, const TSharedPtr<FJsonObject>& J, const TSharedPtr<FJsonObject>& Hf)
	{
		FString File;
		if (!Hf->TryGetStringField(TEXT("file"), File)) return;
		TArray<uint8> Bytes;
		const FString Path = FPaths::Combine(FWRTuning::DataDir(), TEXT("lands"), File);
		if (!FFileHelper::LoadFileToArray(Bytes, *Path))
		{
			UE_LOG(LogWildreach, Error, TEXT("LAND %s: its heightfield %s is missing"), *D.Id.ToString(), *Path);
			return;
		}
		// A packed file (the round island's: "compressed": "zlib", "rawBytes" its unpacked size).
		FString Packing;
		if (Hf->TryGetStringField(TEXT("compressed"), Packing) && Packing == TEXT("zlib"))
		{
			// (Read as a whole number: a float can't hold 32,689,803 exactly.)
			int64 RawBytes64 = 0;
			Hf->TryGetNumberField(TEXT("rawBytes"), RawBytes64);
			const int32 RawBytes = int32(RawBytes64);
			TArray<uint8> Raw;
			Raw.SetNumUninitialized(FMath::Max(0, RawBytes));
			if (RawBytes <= 0 || !FCompression::UncompressMemory(NAME_Zlib, Raw.GetData(), RawBytes, Bytes.GetData(), Bytes.Num()))
			{
				UE_LOG(LogWildreach, Error, TEXT("LAND %s: its heightfield %s would not unpack"), *D.Id.ToString(), *Path);
				return;
			}
			Bytes = MoveTemp(Raw);
		}
		D.HfOrigin = FVector2D(LNum(Hf, TEXT("x0"), 0.f), LNum(Hf, TEXT("y0"), 0.f));
		D.HfStep = FMath::Max(0.5f, LNum(Hf, TEXT("step"), 4.f));
		D.HfN = int32(LNum(Hf, TEXT("samples"), 0.f));
		D.HfUnits = FMath::Max(1.f, LNum(Hf, TEXT("unitsPerMetre"), 16.f));
		D.HfOffset = LNum(Hf, TEXT("offset"), 32768.f);
		const int64 Count = int64(D.HfN) * D.HfN;
		bool bSurface = false;
		Hf->TryGetBoolField(TEXT("surface"), bSurface);
		if (D.HfN < 2 || Bytes.Num() < Count * 2 + (bSurface ? Count : 0))
		{
			UE_LOG(LogWildreach, Error, TEXT("LAND %s: %s holds %d bytes, not %d samples"), *D.Id.ToString(), *File, Bytes.Num(), D.HfN);
			D.HfN = 0;
			return;
		}
		// Little-endian uint16 heights row by row (the file is written that way on every platform we ship).
		D.HfHeights.SetNumUninitialized(int32(Count));
		for (int64 k = 0; k < Count; ++k) D.HfHeights[int32(k)] = uint16(Bytes[int32(k * 2)]) | (uint16(Bytes[int32(k * 2 + 1)]) << 8);
		if (bSurface) D.HfSurface = TArray<uint8>(Bytes.GetData() + Count * 2, int32(Count));
		D.bHeightfield = true;
		const float Span = (D.HfN - 1) * D.HfStep;
		D.MapBox = FBox2D(D.HfOrigin, D.HfOrigin + FVector2D(Span, Span));
		D.OwnFromY = LNum(J, TEXT("ownFromY"), -1.0e9f);
		const TSharedPtr<FJsonObject>* G = nullptr;
		if (J->TryGetObjectField(TEXT("zoneGrid"), G))
		{
			D.GridOrigin = FVector2D(LNum(*G, TEXT("x0"), D.HfOrigin.X), LNum(*G, TEXT("y0"), D.HfOrigin.Y));
			D.GridCell = FMath::Max(1.f, LNum(*G, TEXT("cell"), 16.f));
			D.GridW = int32(LNum(*G, TEXT("width"), 0.f));
			D.GridH = int32(LNum(*G, TEXT("height"), 0.f));
			const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
			if ((*G)->TryGetArrayField(TEXT("rows"), Rows)) for (const TSharedPtr<FJsonValue>& V : *Rows) D.GridRows.Add(V->AsString());
			D.GridH = FMath::Min(D.GridH, D.GridRows.Num());
		}
		D.ZoneBlend = LNum(J, TEXT("zoneBlend"), 45.f);
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (J->TryGetArrayField(TEXT("surfaces"), A))
		{
			for (const TSharedPtr<FJsonValue>& V : *A)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				if (!O.IsValid()) continue;
				FWRLandSurface& S = D.Surfaces.AddDefaulted_GetRef();
				S.Class = uint8(LNum(O, TEXT("class"), 0.f));
				S.Color = WRLand::Colour(O, TEXT("color"), FColor(128, 120, 100));
				S.Rough = LNum(O, TEXT("rough"), 0.9f);
				LSpeck(O, S.Speck, S.SpeckAmount, S.SpeckScale);
				S.bLayers = LLayers(O, S.Layers);
			}
		}
		if (J->TryGetArrayField(TEXT("bands"), A))
		{
			for (const TSharedPtr<FJsonValue>& V : *A)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				if (!O.IsValid()) continue;
				FWRLandBand& B = D.Bands.AddDefaulted_GetRef();
				B.Y0 = LNum(O, TEXT("y0"), 0.f);
				B.Y1 = LNum(O, TEXT("y1"), 0.f);
				B.Amount = LNum(O, TEXT("amount"), 0.f);
				B.NoiseM = FMath::Max(1.f, LNum(O, TEXT("noise"), 30.f));
				B.Color = WRLand::Colour(O, TEXT("color"), FColor::White);
			}
		}
		const TSharedPtr<FJsonObject>* SR = nullptr;
		if (J->TryGetObjectField(TEXT("slopeRock"), SR) && (*SR)->HasField(TEXT("ledge"))) D.LedgeColor = WRLand::Colour(*SR, TEXT("ledge"), FColor(78, 107, 42));
		const TSharedPtr<FJsonObject>* U = nullptr;
		if (J->TryGetObjectField(TEXT("underground"), U))
		{
			(*U)->TryGetStringField(TEXT("letters"), D.UndergroundLetters);
			if ((*U)->TryGetArrayField(TEXT("ways"), A))
			{
				for (const TSharedPtr<FJsonValue>& V : *A)
				{
					const TArray<TSharedPtr<FJsonValue>>* W = nullptr;
					if (V->TryGetArray(W) && W->Num() >= 5)
					{
						D.UndergroundWays.Add(FVector4f(float((*W)[0]->AsNumber()), float((*W)[1]->AsNumber()), float((*W)[2]->AsNumber()), float((*W)[3]->AsNumber())));
						D.UndergroundRadii.Add(float((*W)[4]->AsNumber()));
					}
				}
			}
		}
		D.BuildFirst = LLine(J, TEXT("buildFirst"));
		UE_LOG(LogWildreach, Log, TEXT("LAND %s: heightfield %d x %d at %.0f m from (%.0f, %.0f)"), *D.Id.ToString(), D.HfN, D.HfN, D.HfStep, D.HfOrigin.X, D.HfOrigin.Y);
	}

	/** A colour picked out of a speck list by noise (wildflowers: mostly the first, now and then the others). */
	FLinearColor SpeckPick(const TArray<FLinearColor>& Colours, float N01)
	{
		return Colours[FMath::Clamp(int32(N01 * Colours.Num()), 0, Colours.Num() - 1)];
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
	Priority = int32(LNum(J, TEXT("priority"), 0.f));
	J->TryGetStringField(TEXT("map"), Map);
	const TSharedPtr<FJsonObject>* ZB = nullptr;
	if (J->TryGetObjectField(TEXT("zoneBiomes"), ZB))
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& P : (*ZB)->Values)
		{
			if (P.Key.Len() == 1 && P.Value.IsValid()) ZoneBiomes.Add(P.Key[0], FName(*P.Value->AsString()));
		}
	}
	const TSharedPtr<FJsonObject>* Lod = nullptr;
	if (J->TryGetObjectField(TEXT("lod"), Lod))
	{
		bLod = true;
		const TArray<TSharedPtr<FJsonValue>>* R = nullptr;
		if ((*Lod)->TryGetArrayField(TEXT("ranges"), R)) for (const TSharedPtr<FJsonValue>& V : *R) LodRanges.Add(float(V->AsNumber()));
		CollisionRange = LNum(*Lod, TEXT("collisionRange"), 600.f);
	}
	if (J->TryGetArrayField(TEXT("cuts"), A))
	{
		for (const TSharedPtr<FJsonValue>& V : *A)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			const TArray<TSharedPtr<FJsonValue>>* B = nullptr;
			if (!O.IsValid() || !O->TryGetArrayField(TEXT("box"), B) || B->Num() < 4) continue;
			FCut& Cut = Cuts.AddDefaulted_GetRef();
			Cut.Land = FName(*O->GetStringField(TEXT("land")));
			Cut.Box = FBox2D(FVector2D((*B)[0]->AsNumber(), (*B)[1]->AsNumber()), FVector2D((*B)[2]->AsNumber(), (*B)[3]->AsNumber()));
		}
	}
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
			if (Z->TryGetStringField(TEXT("letter"), Str) && Str.Len() > 0) Zone.Letter = Str[0];
			WRLandHfImpl::LSpeck(Z, Zone.Speck, Zone.SpeckAmount, Zone.SpeckScale);
			Zone.bLayers = WRLandHfImpl::LLayers(Z, Zone.Layers);
		}
	}
	J->TryGetStringField(TEXT("groundMaterial"), GroundMaterial);
	for (const FWRLandZone& Z : Zones) bLayered |= Z.bLayers;
	bLayered = bLayered && !GroundMaterial.IsEmpty();
	if (J->TryGetObjectField(TEXT("heightfield"), N)) WRLandHfImpl::ParseHeightfield(*this, J, *N);
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
	if (bHeightfield) return HfN > 1 && HfHeights.Num() == HfN * HfN;
	return Coast.Num() >= 3 && HeightCurve.Num() >= 2;
}

bool FWRLandDef::Contains(const FVector& W, float MarginM) const
{
	const FVector2D M = ToMap(W);
	// A heightfield land is exactly its frame (no margin: the island's landscape meets it), and only south of where
	// another land's ground ends (the overlap strip under the island's south shore stays the island's).
	if (bHeightfield) return MapBox.IsInside(M) && M.Y >= OwnFromY;
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
	if (bHeightfield) return HfHeight(M);
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
	if (bHeightfield)
	{
		const TCHAR L = GridLetter(M);
		for (const FWRLandZone& Z : Zones) if (Z.Letter == L) return &Z;
		return Zones.Num() ? &Zones.Last() : nullptr;
	}
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

// ================================================================================================ a heightfield land

float FWRLandDef::HfHeight(const FVector2D& M) const
{
	if (HfN < 2) return SeaFloor;
	const float FX = (M.X - HfOrigin.X) / HfStep, FY = (M.Y - HfOrigin.Y) / HfStep;
	const int32 I = FMath::Clamp(FMath::FloorToInt(FX), 0, HfN - 2), J = FMath::Clamp(FMath::FloorToInt(FY), 0, HfN - 2);
	const float TX = FMath::Clamp(FX - I, 0.f, 1.f), TY = FMath::Clamp(FY - J, 0.f, 1.f);
	return FMath::Lerp(FMath::Lerp(HfSample(I, J), HfSample(I + 1, J), TX), FMath::Lerp(HfSample(I, J + 1), HfSample(I + 1, J + 1), TX), TY);
}

bool FWRLandDef::CutAt(const FVector2D& M) const
{
	// Another live land replaces this land's ground inside one of its cut boxes (strictly inside: the box's edge ring
	// stays this land's, so the two grounds share those samples). Map metres are the same frame on both lands.
	for (const FWRLandDef& Other : WRLand::All())
	{
		if (Other.Id == Id || Other.Cuts.Num() == 0 || !WRLand::IsLive(Other)) continue;
		for (const FCut& C : Other.Cuts)
		{
			if (C.Land == Id && M.X > C.Box.Min.X && M.X < C.Box.Max.X && M.Y > C.Box.Min.Y && M.Y < C.Box.Max.Y) return true;
		}
	}
	return false;
}

bool FWRLandDef::HfHiddenAt(const FVector2D& M) const
{
	if (CutAt(M)) return true;
	if (!bHeightfield || HfSurface.Num() != HfHeights.Num()) return false;
	const int32 I = FMath::RoundToInt((M.X - HfOrigin.X) / HfStep), J = FMath::RoundToInt((M.Y - HfOrigin.Y) / HfStep);
	if (I < 0 || J < 0 || I >= HfN || J >= HfN) return true;
	return HfSurface[J * HfN + I] == HfHidden;
}

FName FWRLandDef::BiomeAtMap(const FVector2D& M) const
{
	if (ZoneBiomes.Num())
	{
		if (const FName* B = ZoneBiomes.Find(GridLetter(M))) return *B;
	}
	return Biome;
}

TCHAR FWRLandDef::GridLetter(const FVector2D& M) const
{
	const int32 I = FMath::FloorToInt((M.X - GridOrigin.X) / GridCell), J = FMath::FloorToInt((M.Y - GridOrigin.Y) / GridCell);
	if (I < 0 || J < 0 || J >= GridH || I >= GridW || I >= GridRows[J].Len()) return 0;
	return GridRows[J][I];
}

bool FWRLandDef::IsUnderground(const FVector2D& M) const
{
	if (!bHeightfield) return false;
	const TCHAR L = GridLetter(M);
	int32 Found = INDEX_NONE;
	if (L && UndergroundLetters.FindChar(L, Found)) return true;
	for (int32 i = 0; i < UndergroundWays.Num() && i < UndergroundRadii.Num(); ++i)
	{
		const FVector4f& W = UndergroundWays[i];
		if (WRLand::DistToSegment(M, FVector2D(W.X, W.Y), FVector2D(W.Z, W.W)) <= UndergroundRadii[i]) return true;
	}
	return false;
}

FLinearColor FWRLandDef::HfGround(int32 I, int32 J, const FVector& Nrm, float& OutRough) const
{
	// The ground tells the zone (cooks/biome-forest.md 2.1-2.2; build plan 2.3): each sub-zone's own ground in patches of
	// its second colour, specks (wildflowers in the meadow, moss in the litter), blended across the zone grid's edges over
	// zoneBlend m (nine samples on a ring the noise turns, so the blend is ragged too), then the sample's surface (the
	// river's stones and gravel, sand and dunes, roads and trails, the Hollow Root's floor), the frost band, the sea floor
	// under the water, and rock on the steep ground with moss on the Spine's ledges.
	const FVector2D M = HfOrigin + FVector2D(float(I), float(J)) * HfStep;
	const float H = HfSample(I, J);
	const float Patch = Smooth(-0.15f, 0.35f, Noise(M / 34.f + FVector2D(5.3f, 2.9f)) + Noise(M / 9.f) * 0.3f);
	const auto ZoneColour = [&](const FWRLandZone& Z, float& OutR) -> FLinearColor
	{
		FLinearColor C = FMath::Lerp(Z.Ground, Z.GroundAlt, Patch);
		OutR = Z.Rough;
		if (Z.Speck.Num() && Z.SpeckAmount > 0.f)
		{
			const float S = Noise(M / Z.SpeckScale + FVector2D(31.7f, 4.1f)) * 0.75f + Noise(M / (Z.SpeckScale * 0.37f) + FVector2D(2.2f, 17.3f)) * 0.45f;
			const float W = Smooth(1.f - 2.2f * Z.SpeckAmount, 1.f - 1.4f * Z.SpeckAmount, S * 0.5f + 0.5f);
			if (W > 0.f) C = FMath::Lerp(C, WRLandHfImpl::SpeckPick(Z.Speck, Noise(M / 23.f + FVector2D(9.f, 9.f)) * 0.5f + 0.5f), W * 0.8f);
		}
		return C;
	};
	FLinearColor Sum(0.f, 0.f, 0.f, 0.f);
	float RSum = 0.f, WSum = 0.f, SpineW = 0.f;
	const float Turn = Noise(M / 41.f + FVector2D(1.3f, 8.8f)) * PI;
	for (int32 k = 0; k < 9; ++k)
	{
		FVector2D P = M;
		float W = 2.f;
		if (k > 0)
		{
			const float A = Turn + (k - 1) * (PI / 4.f);
			const float R = ZoneBlend * (0.55f + 0.45f * (Noise(M / 13.f + FVector2D(float(k) * 3.1f, 1.f)) * 0.5f + 0.5f));
			P = M + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R;
			W = 1.f;
		}
		const TCHAR L = GridLetter(P);
		const FWRLandZone* Z = nullptr;
		for (const FWRLandZone& Q : Zones) if (Q.Letter == L) { Z = &Q; break; }
		if (!Z) continue;
		float R = 0.9f;
		Sum += ZoneColour(*Z, R) * W;
		RSum += R * W;
		WSum += W;
		if (L == TEXT('s')) SpineW += W;
	}
	FLinearColor C = WSum > 0.f ? Sum / WSum : FLinearColor(FColor(90, 96, 70));
	OutRough = WSum > 0.f ? RSum / WSum : 0.9f;
	SpineW = WSum > 0.f ? SpineW / WSum : 0.f;
	// The sample's surface, softened where its neighbours are something else (a road's edge, a beach's back).
	const uint8 Cls = HfSurfaceAt(I, J);
	if (Cls != 0 && Cls != HfHidden)
	{
		for (const FWRLandSurface& S : Surfaces)
		{
			if (S.Class != Cls) continue;
			int32 Same = 0;
			Same += HfSurfaceAt(I + 1, J) == Cls; Same += HfSurfaceAt(I - 1, J) == Cls; Same += HfSurfaceAt(I, J + 1) == Cls; Same += HfSurfaceAt(I, J - 1) == Cls;
			FLinearColor SC = S.Color * (0.9f + 0.2f * (Noise(M / 6.f + FVector2D(4.f, 0.f)) * 0.5f + 0.5f));
			if (S.Speck.Num() && S.SpeckAmount > 0.f)
			{
				const float SN = Noise(M / S.SpeckScale + FVector2D(12.f, 3.f)) * 0.5f + 0.5f;
				SC = FMath::Lerp(SC, WRLandHfImpl::SpeckPick(S.Speck, Noise(M / 17.f) * 0.5f + 0.5f), Smooth(1.f - S.SpeckAmount, 1.f - S.SpeckAmount * 0.5f, SN) * 0.7f);
			}
			const float W = 0.5f + 0.4f * (Same / 4.f);
			C = FMath::Lerp(C, SC, W);
			OutRough = FMath::Lerp(OutRough, S.Rough, W);
			break;
		}
	}
	// Colour bands (the frost on the tundra neck's ferns), patchy, never on the water's own ground.
	for (const FWRLandBand& B : Bands)
	{
		if (H < 0.5f || Cls == 1 || Cls == 2) continue;
		const float In = Smooth(B.Y0, B.Y0 + 50.f, M.Y) * (1.f - Smooth(B.Y1 - 70.f, B.Y1, M.Y));
		if (In <= 0.f) continue;
		C = FMath::Lerp(C, B.Color, In * B.Amount * Smooth(-0.2f, 0.4f, Noise(M / B.NoiseM + FVector2D(13.f, 7.f))));
	}
	// Under the sea the ground darkens and smooths toward the sea zone's floor.
	if (H < 0.f)
	{
		for (const FWRLandZone& Q : Zones)
		{
			if (Q.Letter != TEXT('~')) continue;
			const float S = Smooth(0.f, 4.f, -H);
			C = FMath::Lerp(C, Q.Ground * (0.8f + 0.2f * Patch), S);
			OutRough = FMath::Lerp(OutRough, Q.Rough, S);
			break;
		}
	}
	// Steep ground is bare rock (basalt on the Spine's face); its ledges keep a mat of moss.
	const float Slope = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Nrm.Z, -1.f, 1.f)));
	const float Rock = Smooth(SlopeFrom, SlopeTo, Slope);
	if (LedgeColor.A > 0.f && SpineW > 0.f)
	{
		const float Ledge = SpineW * (1.f - Rock) * Smooth(8.f, SlopeFrom, Slope) * Smooth(-0.3f, 0.3f, Noise(M / 5.f + FVector2D(6.f, 6.f)));
		C = FMath::Lerp(C, LedgeColor, Ledge * 0.8f);
	}
	C = FMath::Lerp(C, SlopeRock * (0.85f + 0.3f * (Noise(M / 7.f) * 0.5f + 0.5f)), Rock);
	OutRough = FMath::Lerp(OutRough, 0.86f, Rock);
	return C;
}

void FWRLandDef::HfLayers(int32 I, int32 J, float Out[6]) const
{
	for (int32 k = 0; k < 6; ++k) Out[k] = 0.f;
	const FVector2D M = HfOrigin + FVector2D(float(I), float(J)) * HfStep;
	// The sub-zone's mix, blended across its edges on the same ragged ring of nine samples as its colour.
	float WSum = 0.f;
	const float Turn = Noise(M / 41.f + FVector2D(1.3f, 8.8f)) * PI;
	for (int32 n = 0; n < 9; ++n)
	{
		FVector2D P = M;
		float W = 2.f;
		if (n > 0)
		{
			const float A = Turn + (n - 1) * (PI / 4.f);
			const float R = ZoneBlend * (0.55f + 0.45f * (Noise(M / 13.f + FVector2D(float(n) * 3.1f, 1.f)) * 0.5f + 0.5f));
			P = M + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R;
			W = 1.f;
		}
		const TCHAR L = GridLetter(P);
		for (const FWRLandZone& Z : Zones)
		{
			if (Z.Letter != L || !Z.bLayers) continue;
			for (int32 k = 0; k < 6; ++k) Out[k] += Z.Layers[k] * W;
			WSum += W;
			break;
		}
	}
	if (WSum > 0.f) for (int32 k = 0; k < 6; ++k) Out[k] /= WSum;
	else Out[0] = 1.f;
	// Patches: within a zone the litter and the moss trade places by noise (a mossy hollow, a drift of leaves), so no
	// stretch of floor is one even mix (Electric Dreams' clustering, never uniform cover).
	const float Patch = Noise(M / 17.f + FVector2D(3.7f, 9.1f)) * 0.6f + Noise(M / 6.f + FVector2D(11.f, 2.f)) * 0.25f;
	const float Shift = FMath::Clamp(Patch, -0.5f, 0.5f) * FMath::Min(Out[0], Out[1]) * 1.6f;
	Out[0] = FMath::Max(0.f, Out[0] - Shift);
	Out[1] = FMath::Max(0.f, Out[1] + Shift);
	// The sample's surface over it (a river bed's stones, a trail's trodden soil), softened where its neighbours differ.
	const uint8 Cls = HfSurfaceAt(I, J);
	if (Cls != 0 && Cls != HfHidden)
	{
		for (const FWRLandSurface& S : Surfaces)
		{
			if (S.Class != Cls || !S.bLayers) continue;
			int32 Same = 0;
			Same += HfSurfaceAt(I + 1, J) == Cls; Same += HfSurfaceAt(I - 1, J) == Cls; Same += HfSurfaceAt(I, J + 1) == Cls; Same += HfSurfaceAt(I, J - 1) == Cls;
			const float W = 0.5f + 0.5f * (Same / 4.f);
			for (int32 k = 0; k < 6; ++k) Out[k] = FMath::Lerp(Out[k], S.Layers[k], W);
			break;
		}
	}
	// Damp ground beside water grows moss (a bank's edge; Trails' creek-bank strip).
	if (Cls == 0)
	{
		int32 Wet = 0;
		for (int32 dj = -2; dj <= 2; ++dj) for (int32 di = -2; di <= 2; ++di)
		{
			const uint8 C2 = HfSurfaceAt(I + di, J + dj);
			Wet += (C2 == 1 || C2 == 2 || C2 == 5) ? 1 : 0;   // bed, bank, marsh
		}
		const float Damp = FMath::Clamp(Wet / 8.f, 0.f, 0.7f);
		for (int32 k = 0; k < 6; ++k) Out[k] *= 1.f - Damp;
		Out[1] += Damp;
	}
	float Sum = 0.f;
	for (int32 k = 0; k < 6; ++k) Sum += Out[k];
	if (Sum > 0.f) for (int32 k = 0; k < 6; ++k) Out[k] /= Sum;
}

// ================================================================================================ the registry

const TArray<FWRLandDef>& WRLand::All()
{
	return Registry();
}

void WRLand::SetCurrentMap(const FString& Map)
{
	CurrentMapRef() = Map;
}

const FString& WRLand::CurrentMap()
{
	return CurrentMapRef();
}

bool WRLand::IsLive(const FWRLandDef& Def)
{
	// A land with its own map (the round island) is there and nowhere else; the Old Island's lands only on the island's
	// map (or before any world has begun), when their biome is live.
	const FString& Cur = CurrentMapRef();
	if (!Def.Map.IsEmpty()) return Def.Map == Cur;
	if (!Cur.IsEmpty() && !Cur.Contains(TEXT("Island"))) return false;
	return WRBiome::IsLive(Def.Biome);
}

const FWRLandDef* WRLand::Find(FName Id)
{
	return Registry().FindByPredicate([Id](const FWRLandDef& D) { return D.Id == Id; });
}

const FWRLandDef* WRLand::At(const FVector& World)
{
	// Lands may overlap (the Graveyard over the forest's west face of the Spine, the pass it cuts open): the land whose
	// ground is there wins, the higher priority on a tie; a land with no ground there is only the answer if none has.
	const FWRLandDef* Best = nullptr;
	bool bBestGround = false;
	for (const FWRLandDef& D : Registry())
	{
		if (!D.Contains(World) || !IsLive(D)) continue;
		const bool bGround = !D.HfHiddenAt(D.ToMap(World));
		if (!Best || (bGround && !bBestGround) || (bGround == bBestGround && D.Priority > Best->Priority))
		{
			Best = &D;
			bBestGround = bGround;
		}
	}
	return Best;
}

FName WRLand::BiomeAt(const FVector& World)
{
	const FWRLandDef* D = At(World);
	return D ? D->BiomeAtMap(D->ToMap(World)) : NAME_None;
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
	if (D->IsUnderground(M)) return true;
	const FWRLandFeature* H = D->HoleAt(M, 2.f);
	return H && World.Z < D->ToWorld(M, H->Levels[0]).Z + 200.f;
}

bool WRLand::GroundUnder(const FVector& World, float& OutZ)
{
	const FWRLandDef* D = At(World);
	if (!D) return false;
	const FVector2D M = D->ToMap(World);
	// Heightfield lands only (the forest): the wasteland's ground keeps its own rules, unchanged.
	if (!D->bHeightfield || D->HfHiddenAt(M)) return false;
	OutZ = D->ToWorld(M, D->Height(M)).Z;
	return true;
}

// ================================================================================================ the subsystem

bool UWRLandSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

FString UWRLandSubsystem::MapOf(const UWorld& World)
{
	FString Map = UWorld::RemovePIEPrefix(World.GetMapName());
	Map.RemoveFromStart(World.StreamingLevelsPrefix);
	return Map;
}

void UWRLandSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// Which lands are live depends on the map (WRLand::IsLive): known before any other world system begins play.
	if (const UWorld* W = GetWorld()) WRLand::SetCurrentMap(MapOf(*W));
}

void UWRLandSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// Lands sit in their map's sea: the Old Island's lands only on the island's map (test arenas and stages hold none),
	// the round island's on its own map (FWRLandDef::Map).
	const FString Map = MapOf(InWorld);
	WRLand::SetCurrentMap(Map);
	const bool bOldIsland = Map.Contains(TEXT("Island"));
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
		// Its camps (PlayerStarts, safe zones, starter stations): before the game mode picks anyone's start this frame.
		if (InWorld.GetNetMode() != NM_Client) AWRLand::SpawnCamps(&InWorld, Def);
	}
	if (!bOldIsland) return;
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
	// A layered land's own ground material (the Ancient forest's M_ForestGround); the plain one if it isn't built.
	if (const FWRLandDef* LD = Def(); LD && !LD->GroundMaterial.IsEmpty())
	{
		if (UMaterialInterface* Own = LoadObject<UMaterialInterface>(nullptr, *LD->GroundMaterial)) GroundMat = Own;
		else UE_LOG(LogWildreach, Warning, TEXT("LAND %s: %s is missing (Tools/terrain/build_forest_ground.py); the plain ground is used"), *LandId.ToString(), *LD->GroundMaterial);
	}
	TearMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_VoidTear.M_VoidTear"));
	LightMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_VoidLight.M_VoidLight"));
	if (!GroundMat) UE_LOG(LogWildreach, Error, TEXT("LAND %s: /Game/Terrain/M_LandGround is missing (Tools/terrain/build_void_materials.py)"), *LandId.ToString());
	const FWRLandDef* D = Def();
	if (D && D->bHeightfield && GroundMat && GetNetMode() != NM_DedicatedServer)
	{
		// One instance for the whole land so rain can wet all of it at once (StepWet).
		GroundMID = UMaterialInstanceDynamic::Create(GroundMat, this);
		GroundMID->SetScalarParameterValue(TEXT("Wet"), 0.f);
	}
	if (D && D->bHeightfield && D->bLod)
	{
		LodStart();
		return;
	}
	QueueChunks();
	// A heightfield land's camps are where new players of its path first stand, and the game mode places them right
	// after this (before the land's first tick): their chunks are built now, collision cooked at once, so nobody spawns
	// over nothing. The rest comes in over the next frames, nearest the camps first.
	if (D && D->bHeightfield && D->BuildFirst.Num())
	{
		const float Span = FMath::Max(1, FMath::RoundToInt(D->Chunk / D->HfStep)) * D->HfStep;
		TSet<FIntPoint> Now;
		for (const FVector2D& P : D->BuildFirst)
		{
			for (int32 dy = -1; dy <= 1; ++dy) for (int32 dx = -1; dx <= 1; ++dx)
			{
				// The camp's cell and any neighbour within 70 m of it (its safe zone and stations reach 40 m).
				const FVector2D Q = P + FVector2D(dx, dy) * 70.f;
				Now.Add(FIntPoint(FMath::FloorToInt((Q.X - D->HfOrigin.X) / Span), FMath::FloorToInt((Q.Y - D->HfOrigin.Y) / Span)));
			}
		}
		int32 Made = 0;
		for (const FIntPoint& Cell : Now)
		{
			if (Queue.Remove(Cell) > 0 && BuildHeightfieldChunk(Cell, false)) ++Made;
		}
		UE_LOG(LogWildreach, Log, TEXT("LAND %s: %d chunks round its camps built at load"), *LandId.ToString(), Made);
	}
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
	if (D->bHeightfield)
	{
		// The heightfield's own grid: chunks of Chunk m (whole samples), the last row and column what's left.
		const int32 Q = FMath::Max(1, FMath::RoundToInt(D->Chunk / D->HfStep));
		const int32 NC = FMath::DivideAndRoundUp(D->HfN - 1, Q);
		for (int32 y = 0; y < NC; ++y) for (int32 x = 0; x < NC; ++x) Queue.Add(FIntPoint(x, y));
		// Nearest a camp first (else the land's middle): where players arrive comes in before the far sea floor.
		const float Span = Q * D->HfStep;
		TArray<FVector2D> First = D->BuildFirst;
		if (First.Num() == 0) First.Add(D->MapBox.GetCenter());
		const FVector2D O = D->HfOrigin;
		const auto Dist = [&](const FIntPoint& Cl)
		{
			const FVector2D Cc = O + FVector2D(Cl.X + 0.5f, Cl.Y + 0.5f) * Span;
			float Best = TNumericLimits<float>::Max();
			for (const FVector2D& P : First) Best = FMath::Min(Best, float(FVector2D::DistSquared(Cc, P)));
			return Best;
		};
		Queue.Sort([&](const FIntPoint& A, const FIntPoint& B) { return Dist(A) > Dist(B); });
		return;
	}
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
	if (D->bHeightfield && D->bLod)
	{
		LodTick(DeltaSeconds);
		StepWet(DeltaSeconds);
		return;
	}
	if (!bGroundDone)
	{
		for (int32 n = 0; n < D->ChunksPerFrame && Queue.Num(); ++n)
		{
			const FIntPoint Cell = Queue.Pop(EAllowShrinking::No);
			if (D->bHeightfield) BuildHeightfieldChunk(Cell);
			else BuildChunk(Cell);
		}
		if (Queue.Num() == 0)
		{
			BuildHoles();
			bGroundDone = true;
			UE_LOG(LogWildreach, Log, TEXT("LAND %s ground done: %d chunks"), *LandId.ToString(), Built);
		}
		return;
	}
	StepWet(DeltaSeconds);
	if (!bSitesDone)
	{
		bSitesDone = true;
		// A heightfield land's places are its own world's (the Ancient forest: UWRForestWorld builds them from
		// Data/forest.json); the land file's pois and kit are the function lands'.
		if (D->bHeightfield) return;
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
	BuiltCells.Add(Cell);
	++Built;
	return true;
}

bool AWRLand::BuildHeightfieldChunk(const FIntPoint& Cell, bool bAsync)
{
	// One chunk of a heightfield land: its samples as vertices (so the ground is exactly the file's, every machine the
	// same), normals from the neighbouring samples, colours from the zone grid and the sample's surface, and no triangle
	// with a corner where the land has no ground (the strip under the island's own landscape).
	const FWRLandDef& D = *Def();
	const int32 Q = FMath::Max(1, FMath::RoundToInt(D.Chunk / D.HfStep));
	const int32 I0 = Cell.X * Q, J0 = Cell.Y * Q;
	const int32 NI = FMath::Min(Q, D.HfN - 1 - I0), NJ = FMath::Min(Q, D.HfN - 1 - J0);
	if (NI <= 0 || NJ <= 0) return false;
	const bool bSurface = D.HfSurface.Num() == D.HfHeights.Num();
	// Another live land cuts part of this one (the Graveyard opening Rootgrave in the forest's ground): test only chunks
	// that touch a cut box.
	bool bCuts = false;
	{
		const FBox2D ChunkBox(D.HfOrigin + FVector2D(float(I0), float(J0)) * D.HfStep, D.HfOrigin + FVector2D(float(I0 + NI), float(J0 + NJ)) * D.HfStep);
		for (const FWRLandDef& Other : WRLand::All())
		{
			if (Other.Id == D.Id || !WRLand::IsLive(Other)) continue;
			for (const FWRLandDef::FCut& C : Other.Cuts) bCuts |= C.Land == D.Id && C.Box.Intersect(ChunkBox);
		}
	}
	float Top = -1e6f;
	bool bAny = false;
	for (int32 j = 0; j <= NJ; ++j)
	{
		for (int32 i = 0; i <= NI; ++i)
		{
			Top = FMath::Max(Top, D.HfSample(I0 + i, J0 + j));
			bAny |= !bSurface || D.HfSurface[(J0 + j) * D.HfN + (I0 + i)] != FWRLandDef::HfHidden;
		}
	}
	// All of it the island's, or open sea floor the sea covers: nothing to build.
	if (!bAny || Top < D.SeaFloor + 1.5f) return false;
	const int32 VX = NI + 1, VY = NJ + 1;
	const bool bLook = GetNetMode() != NM_DedicatedServer;
	TArray<FVector> Verts;
	TArray<FVector> Normals;
	TArray<FLinearColor> Colours;
	TArray<FVector2D> UV;
	TArray<FVector2D> UV1, UV2, UV3;   // a layered ground's mix (FWRLandDef::HfLayers)
	const bool bLayers = bLook && D.bLayered;
	TArray<FProcMeshTangent> Tangents;
	TArray<int32> Tris;
	TArray<uint8> Gone;
	Verts.Reserve(VX * VY);
	Normals.Reserve(VX * VY);
	Colours.Reserve(VX * VY);
	UV.Reserve(VX * VY);
	Tangents.Reserve(VX * VY);
	Gone.Reserve(VX * VY);
	for (int32 j = 0; j < VY; ++j)
	{
		for (int32 i = 0; i < VX; ++i)
		{
			const int32 SI = I0 + i, SJ = J0 + j;
			const FVector2D M = D.HfOrigin + FVector2D(float(SI), float(SJ)) * D.HfStep;
			const float Z = D.HfSample(SI, SJ);
			const float DX = D.HfSample(SI + 1, SJ) - D.HfSample(SI - 1, SJ);
			const float DY = D.HfSample(SI, SJ + 1) - D.HfSample(SI, SJ - 1);
			const FVector Nrm = FVector(-DX, -DY, 2.f * D.HfStep).GetSafeNormal();
			// The colour is looks only: a dedicated server builds the shape (collision) and skips it.
			float Rough = 0.9f;
			FLinearColor C = bLook ? D.HfGround(SI, SJ, Nrm, Rough) : FLinearColor::White;
			C.A = 1.f - Rough;
			Verts.Add(D.ToWorld(M, Z) - D.Origin);
			Normals.Add(Nrm);
			Colours.Add(C);
			UV.Add(M / 4.f);
			if (bLayers)
			{
				float L[6];
				D.HfLayers(SI, SJ, L);
				UV1.Add(FVector2D(L[0], L[1]));
				UV2.Add(FVector2D(L[2], L[3]));
				UV3.Add(FVector2D(L[4], L[5]));
			}
			Tangents.Add(FProcMeshTangent(FVector(1.f, 0.f, 0.f), false));
			Gone.Add((bSurface && D.HfSurface[SJ * D.HfN + SI] == FWRLandDef::HfHidden) || (bCuts && D.CutAt(M)) ? 1 : 0);
		}
	}
	Tris.Reserve(NI * NJ * 6);
	for (int32 j = 0; j < NJ; ++j)
	{
		for (int32 i = 0; i < NI; ++i)
		{
			const int32 A = j * VX + i, B = A + 1, C = A + VX, E = C + 1;
			if (Gone[A] || Gone[B] || Gone[C] || Gone[E]) continue;
			Tris.Append({ A, C, E, A, E, B });
		}
	}
	if (Tris.Num() == 0) return false;
	UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this);
	Mesh->SetupAttachment(RootComponent);
	Mesh->bUseAsyncCooking = bAsync;
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	Mesh->SetCollisionObjectType(ECC_WorldStatic);
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->LDMaxDrawDistance = D.DrawDistance;
	Mesh->SetCachedMaxDrawDistance(D.DrawDistance);
	if (bLayers) Mesh->CreateMeshSection_LinearColor(0, Verts, Tris, Normals, UV, UV1, UV2, UV3, Colours, Tangents, true);
	else Mesh->CreateMeshSection_LinearColor(0, Verts, Tris, Normals, UV, Colours, Tangents, true);
	if (GroundMID) Mesh->SetMaterial(0, GroundMID);
	else if (GroundMat) Mesh->SetMaterial(0, GroundMat);
	Mesh->RegisterComponent();
	Chunks.Add(Mesh);
	BuiltCells.Add(Cell);
	++Built;
	return true;
}

// ================================================================================================ far detail levels
// The round island (FWRLandDef::bLod) is too big to build whole at full detail: each chunk is built at the detail its
// distance from your view wants (every sample near you, every 2nd, 4th, 8th further off), with a skirt hanging under its
// edge so a coarser neighbour never shows a crack, and it has collision only near a body that could stand on it (players
// within CollisionRange, anyone else on their own chunk and the next). A dedicated server draws nothing, so it only
// builds the ground under bodies. Rebuilt as you move, nearest first, a few a frame.

void AWRLand::LodStart()
{
	const FWRLandDef& D = *Def();
	const int32 Q = FMath::Max(1, FMath::RoundToInt(D.Chunk / D.HfStep));
	const int32 NC = FMath::DivideAndRoundUp(D.HfN - 1, Q);
	const float Span = Q * D.HfStep;
	for (int32 y = 0; y < NC; ++y)
	{
		for (int32 x = 0; x < NC; ++x)
		{
			float Top = -1e6f;
			for (int32 j = 0; j <= Q; j += 2) for (int32 i = 0; i <= Q; i += 2) Top = FMath::Max(Top, D.HfSample(x * Q + i, y * Q + j));
			if (Top >= D.SeaFloor + 1.5f) LodCells.Add(FIntPoint(x, y));
		}
	}
	// Where players first stand (the camps, Hearthmoot's gates): solid at once, the game mode places people right after.
	int32 Made = 0;
	for (const FVector2D& P : D.BuildFirst)
	{
		for (int32 dy = -1; dy <= 1; ++dy) for (int32 dx = -1; dx <= 1; ++dx)
		{
			const FVector2D At = P + FVector2D(dx, dy) * 70.f;
			const FIntPoint Cell(FMath::FloorToInt((At.X - D.HfOrigin.X) / Span), FMath::FloorToInt((At.Y - D.HfOrigin.Y) / Span));
			const FLodChunk* Have = LodChunks.Find(Cell);
			if ((!Have || !Have->bCollision) && LodCells.Contains(Cell) && BuildLodChunk(Cell, 0, true, false)) ++Made;
		}
	}
	UE_LOG(LogWildreach, Log, TEXT("LAND %s: %d chunks above the sea floor, %d built solid at load where players first stand"), *LandId.ToString(), LodCells.Num(), Made);
}

void AWRLand::LodTick(float DeltaSeconds)
{
	const FWRLandDef& D = *Def();
	UWorld* W = GetWorld();
	if (!W) return;
	const int32 Q = FMath::Max(1, FMath::RoundToInt(D.Chunk / D.HfStep));
	const float Span = Q * D.HfStep;
	const bool bDraw = GetNetMode() != NM_DedicatedServer;
	const auto CellOf = [&](const FVector2D& M) { return FIntPoint(FMath::FloorToInt((M.X - D.HfOrigin.X) / Span), FMath::FloorToInt((M.Y - D.HfOrigin.Y) / Span)); };
	LodCheck -= DeltaSeconds;
	if (LodCheck <= 0.f)
	{
		LodCheck = 0.4f;
		++LodPasses;
		// Where the views are (ground drawn) and where bodies are (ground to stand on), in map metres.
		TArray<FVector2D> Views;
		struct FBody { FVector2D At; float R; };
		TArray<FBody> Bodies;
		for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
		{
			const APlayerController* PC = It->Get();
			if (!PC) continue;
			if (bDraw && PC->IsLocalController())
			{
				const APawn* P = PC->GetPawn();
				const FVector Cam = PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : (P ? P->GetActorLocation() : FVector::ZeroVector);
				Views.Add(D.ToMap(Cam));
			}
			if (const APawn* P = PC->GetPawn()) Bodies.Add({ D.ToMap(P->GetActorLocation()), D.CollisionRange });
		}
		// Everyone else (creatures, mounts, other players on a client): their own chunk and the next.
		for (TActorIterator<APawn> It(W); It; ++It) Bodies.Add({ D.ToMap(It->GetActorLocation()), Span });
		// Nobody may stand on nothing: a body's own chunk without collision is made solid now, not in the queue.
		for (const FBody& B : Bodies)
		{
			const FIntPoint Cell = CellOf(B.At);
			const FLodChunk* Have = LodChunks.Find(Cell);
			if ((!Have || !Have->bCollision) && LodCells.Contains(Cell)) BuildLodChunk(Cell, 0, true, false);
		}
		const int32 MaxLevel = D.LodRanges.Num();
		const float HalfDiag = Span * 0.7072f;
		TArray<FLodJob> Jobs;
		for (const FIntPoint& Cell : LodCells)
		{
			const FVector2D C = D.HfOrigin + (FVector2D(Cell) + FVector2D(0.5f, 0.5f)) * Span;
			float DV = 1e12f;
			for (const FVector2D& V : Views) DV = FMath::Min(DV, float(FVector2D::Distance(C, V)));
			DV = FMath::Max(0.f, DV - HalfDiag);
			int32 Level = -1;
			if (Views.Num())
			{
				Level = MaxLevel;
				for (int32 k = 0; k < MaxLevel; ++k) if (DV < D.LodRanges[k]) { Level = k; break; }
			}
			bool bCol = false;
			float DB = 1e12f;
			for (const FBody& B : Bodies)
			{
				const float Dist = FMath::Max(0.f, float(FVector2D::Distance(C, B.At)) - HalfDiag);
				if (Dist < B.R) { bCol = true; DB = FMath::Min(DB, Dist); }
			}
			if (bCol) Level = 0;
			const FLodChunk* Have = LodChunks.Find(Cell);
			if ((Have ? Have->Level : -1) == Level && (Have && Have->bCollision) == bCol) continue;
			// Ground to stand on first (nearest first), then what you see (nearest first), a chunk going away last.
			Jobs.Add({ Cell, int8(Level), bCol, bCol ? DB - 1e9f : (Level < 0 ? 1e12f : DV) });
		}
		Jobs.Sort([](const FLodJob& A, const FLodJob& B) { return A.Priority > B.Priority; });
		LodJobs = MoveTemp(Jobs);
	}
	// A few chunks a frame (a far one is a fraction of a near one's work); more while the island first comes in.
	float Budget = bLodFirstPass ? 24.f : float(D.ChunksPerFrame);
	while (LodJobs.Num() && Budget > 0.f)
	{
		const FLodJob J = LodJobs.Pop(EAllowShrinking::No);
		FLodChunk* Have = LodChunks.Find(J.Cell);
		if ((Have ? Have->Level : -1) == J.Level && (Have && Have->bCollision) == J.bCollision) continue;
		if (J.Level < 0)
		{
			if (Have && Have->Mesh)
			{
				Chunks.Remove(Have->Mesh);
				Have->Mesh->DestroyComponent();
			}
			LodChunks.Remove(J.Cell);
			Budget -= 0.05f;
			continue;
		}
		BuildLodChunk(J.Cell, J.Level, J.bCollision, true);
		Budget -= 1.f / float(1 << (2 * J.Level));
	}
	if (bLodFirstPass && LodPasses > 0 && LodJobs.Num() == 0)
	{
		bLodFirstPass = false;
		bGroundDone = true;
		UE_LOG(LogWildreach, Log, TEXT("LAND %s ground in: %d chunks drawn"), *LandId.ToString(), LodChunks.Num());
	}
}

bool AWRLand::BuildLodChunk(const FIntPoint& Cell, int32 Level, bool bCollision, bool bAsync)
{
	const FWRLandDef& D = *Def();
	const int32 Q = FMath::Max(1, FMath::RoundToInt(D.Chunk / D.HfStep));
	const int32 I0 = Cell.X * Q, J0 = Cell.Y * Q;
	const int32 NI = FMath::Min(Q, D.HfN - 1 - I0), NJ = FMath::Min(Q, D.HfN - 1 - J0);
	if (NI <= 0 || NJ <= 0) return false;
	const int32 Sx = 1 << FMath::Clamp(Level, 0, 6);
	TArray<int32> Is, Js;
	for (int32 i = 0; i < NI; i += Sx) Is.Add(i);
	Is.Add(NI);
	for (int32 j = 0; j < NJ; j += Sx) Js.Add(j);
	Js.Add(NJ);
	const int32 VX = Is.Num(), VY = Js.Num();
	const bool bLook = GetNetMode() != NM_DedicatedServer;
	const bool bLayers = bLook && D.bLayered;
	const bool bSurface = D.HfSurface.Num() == D.HfHeights.Num();
	struct FSection
	{
		TArray<FVector> Verts, Normals;
		TArray<FLinearColor> Colours;
		TArray<FVector2D> UV, UV1, UV2, UV3;
		TArray<FProcMeshTangent> Tangents;
		TArray<int32> Tris;
	};
	FSection G, Sk;
	const auto AddVert = [&](FSection& S, int32 SI, int32 SJ, float DropM)
	{
		const FVector2D M = D.HfOrigin + FVector2D(float(SI), float(SJ)) * D.HfStep;
		const float Z = D.HfSample(SI, SJ) - DropM;
		const float DX = D.HfSample(SI + Sx, SJ) - D.HfSample(SI - Sx, SJ);
		const float DY = D.HfSample(SI, SJ + Sx) - D.HfSample(SI, SJ - Sx);
		const FVector Nrm = FVector(-DX, -DY, 2.f * D.HfStep * Sx).GetSafeNormal();
		float Rough = 0.9f;
		FLinearColor C = bLook ? D.HfGround(SI, SJ, Nrm, Rough) : FLinearColor::White;
		C.A = 1.f - Rough;
		S.Verts.Add(D.ToWorld(M, Z) - D.Origin);
		S.Normals.Add(Nrm);
		S.Colours.Add(C);
		S.UV.Add(M / 4.f);
		if (bLayers)
		{
			float L[6];
			D.HfLayers(SI, SJ, L);
			S.UV1.Add(FVector2D(L[0], L[1]));
			S.UV2.Add(FVector2D(L[2], L[3]));
			S.UV3.Add(FVector2D(L[4], L[5]));
		}
		S.Tangents.Add(FProcMeshTangent(FVector(1.f, 0.f, 0.f), false));
	};
	TArray<uint8> Gone;
	Gone.Reserve(VX * VY);
	for (int32 j = 0; j < VY; ++j)
	{
		for (int32 i = 0; i < VX; ++i)
		{
			const int32 SI = I0 + Is[i], SJ = J0 + Js[j];
			AddVert(G, SI, SJ, 0.f);
			Gone.Add(bSurface && D.HfSurface[SJ * D.HfN + SI] == FWRLandDef::HfHidden ? 1 : 0);
		}
	}
	for (int32 j = 0; j + 1 < VY; ++j)
	{
		for (int32 i = 0; i + 1 < VX; ++i)
		{
			const int32 A = j * VX + i, B = A + 1, C = A + VX, E = C + 1;
			if (Gone[A] || Gone[B] || Gone[C] || Gone[E]) continue;
			G.Tris.Append({ A, C, E, A, E, B });
		}
	}
	if (G.Tris.Num() == 0) return false;
	if (bLook)
	{
		// The skirt: the chunk's rim, and the same points dropped, joined both ways round (seen from either side).
		TArray<FIntPoint> Rim;
		for (int32 i = 0; i < VX; ++i) Rim.Add(FIntPoint(i, 0));
		for (int32 j = 1; j < VY; ++j) Rim.Add(FIntPoint(VX - 1, j));
		for (int32 i = VX - 2; i >= 0; --i) Rim.Add(FIntPoint(i, VY - 1));
		for (int32 j = VY - 2; j >= 0; --j) Rim.Add(FIntPoint(0, j));
		const float Drop = 2.f + D.HfStep * Sx;
		for (const FIntPoint& R : Rim)
		{
			AddVert(Sk, I0 + Is[R.X], J0 + Js[R.Y], 0.f);
			AddVert(Sk, I0 + Is[R.X], J0 + Js[R.Y], Drop);
		}
		for (int32 r = 0; r + 1 < Rim.Num(); ++r)
		{
			const int32 T0 = r * 2, B0 = T0 + 1, T1 = T0 + 2, B1 = T0 + 3;
			Sk.Tris.Append({ T0, B0, T1, T1, B0, B1, T0, T1, B0, T1, B1, B0 });
		}
	}
	FLodChunk& Have = LodChunks.FindOrAdd(Cell);
	UProceduralMeshComponent* Mesh = Have.Mesh;
	if (!Mesh)
	{
		Mesh = NewObject<UProceduralMeshComponent>(this);
		Mesh->SetupAttachment(RootComponent);
		Mesh->SetCanEverAffectNavigation(false);
		Mesh->LDMaxDrawDistance = D.DrawDistance;
		Mesh->SetCachedMaxDrawDistance(D.DrawDistance);
		Mesh->RegisterComponent();
		Chunks.Add(Mesh);
		Have.Mesh = Mesh;
	}
	else
	{
		Mesh->ClearAllMeshSections();
	}
	Mesh->bUseAsyncCooking = bAsync;
	if (bCollision)
	{
		Mesh->SetCollisionProfileName(TEXT("BlockAll"));
		Mesh->SetCollisionObjectType(ECC_WorldStatic);
	}
	else
	{
		Mesh->SetCollisionProfileName(TEXT("NoCollision"));
	}
	if (bLayers)
	{
		Mesh->CreateMeshSection_LinearColor(0, G.Verts, G.Tris, G.Normals, G.UV, G.UV1, G.UV2, G.UV3, G.Colours, G.Tangents, bCollision);
		if (Sk.Tris.Num()) Mesh->CreateMeshSection_LinearColor(1, Sk.Verts, Sk.Tris, Sk.Normals, Sk.UV, Sk.UV1, Sk.UV2, Sk.UV3, Sk.Colours, Sk.Tangents, false);
	}
	else
	{
		Mesh->CreateMeshSection_LinearColor(0, G.Verts, G.Tris, G.Normals, G.UV, G.Colours, G.Tangents, bCollision);
		if (Sk.Tris.Num()) Mesh->CreateMeshSection_LinearColor(1, Sk.Verts, Sk.Tris, Sk.Normals, Sk.UV, Sk.Colours, Sk.Tangents, false);
	}
	UMaterialInterface* Mat = GroundMID ? GroundMID.Get() : GroundMat.Get();
	if (Mat)
	{
		Mesh->SetMaterial(0, Mat);
		if (Sk.Tris.Num()) Mesh->SetMaterial(1, Mat);
	}
	Have.Level = int8(Level);
	Have.bCollision = bCollision;
	return true;
}

bool AWRLand::IsGroundBuiltAt(const FVector& World) const
{
	const FWRLandDef* D = Def();
	if (!D) return false;
	const FVector2D M = D->ToMap(World);
	if (D->bHeightfield && D->bLod)
	{
		// Built means you can stand on it there (the far chunks are looks only).
		const float Span = FMath::Max(1, FMath::RoundToInt(D->Chunk / D->HfStep)) * D->HfStep;
		const FLodChunk* C = LodChunks.Find(FIntPoint(FMath::FloorToInt((M.X - D->HfOrigin.X) / Span), FMath::FloorToInt((M.Y - D->HfOrigin.Y) / Span)));
		return C && C->bCollision;
	}
	if (D->bHeightfield)
	{
		const float Span = FMath::Max(1, FMath::RoundToInt(D->Chunk / D->HfStep)) * D->HfStep;
		return BuiltCells.Contains(FIntPoint(FMath::FloorToInt((M.X - D->HfOrigin.X) / Span), FMath::FloorToInt((M.Y - D->HfOrigin.Y) / Span)));
	}
	const FVector2D Min = D->MapBox.Min - FVector2D(D->CoastSea);
	return BuiltCells.Contains(FIntPoint(FMath::FloorToInt((M.X - Min.X) / D->Chunk), FMath::FloorToInt((M.Y - Min.Y) / D->Chunk)));
}

void AWRLand::StepWet(float DeltaSeconds)
{
	// Rain soaks the ground (darker, glossier) over ~20 s and it dries over a minute after: the land's weather row's kind
	// that waters crops (Rain) at its strength. Looks only, so no server work.
	if (!GroundMID) return;
	WetCheck -= DeltaSeconds;
	if (WetCheck > 0.f) return;
	const float Step = 0.5f;
	WetCheck = Step;
	const FWRLandDef* D = Def();
	float Want = 0.f;
	if (const FWRWeatherNow* W = D ? UWRWeatherSubsystem::For(this, D->Biome) : nullptr)
	{
		const FWRWeatherKind* K = FWRWeatherDefs::Get().Kind(D->Biome, W->Kind);
		if (K && K->bWatersCrops) Want = W->Strength(UWRWeatherSubsystem::Now(this));
	}
	const float Secs = Want > Wet ? WRTune(TEXT("world.landWetIn"), 20.f) : WRTune(TEXT("world.landDryOut"), 60.f);
	const float Next = FMath::FInterpConstantTo(Wet, Want, Step, 1.f / FMath::Max(Secs, 0.1f));
	if (Next != Wet)
	{
		Wet = Next;
		GroundMID->SetScalarParameterValue(TEXT("Wet"), Wet);
	}
}

void AWRLand::SpawnCamps(UWorld* World, const FWRLandDef& D)
{
	// The land's camps (world-map: three per land, the middle one with the starter stations): a PlayerStart tagged with
	// the biome its path starts in (WRGameMode::ChoosePlayerStart and WRBeds pick a new player's camp by it), a safe zone
	// round each (safeRadius m), and the forge, armourer, alchemy table and workbench a short walk inland of the middle
	// one, facing the camp. Only what the map does not already hold is made (a map with them placed keeps its own).
	const TSharedPtr<FJsonObject> C = D.Section(TEXT("camps"));
	if (!World || World->GetNetMode() == NM_Client || !C.IsValid()) return;
	FString TagStr = D.Biome.ToString();
	C->TryGetStringField(TEXT("tag"), TagStr);
	const FName Tag(*TagStr);
	const float SafeR = LNum(C, TEXT("safeRadius"), 40.f) * 100.f;
	const int32 StationsAt = int32(LNum(C, TEXT("stationsAt"), 1.f));
	const float BackM = LNum(C, TEXT("stationsBack"), 25.f);
	const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
	if (!C->TryGetArrayField(TEXT("list"), List)) return;
	bool bHaveStart = false;
	for (TActorIterator<APlayerStart> It(World); It; ++It) bHaveStart |= It->PlayerStartTag == Tag;
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	int32 Made = 0;
	for (int32 c = 0; c < List->Num(); ++c)
	{
		const TArray<TSharedPtr<FJsonValue>>* Row = nullptr;
		if (!(*List)[c]->TryGetArray(Row) || Row->Num() < 2) continue;
		const FVector2D M((*Row)[0]->AsNumber(), (*Row)[1]->AsNumber());
		const float Yaw = Row->Num() >= 3 ? float((*Row)[2]->AsNumber()) : 0.f;   // toward the sea the camp looks out on
		const FVector Camp = D.GroundPoint(M);
		if (!bHaveStart)
		{
			// A capsule's height over the camp's ground, facing inland (the forest the newcomer walks into).
			const FTransform T(FRotator(0.f, Yaw + 180.f, 0.f), Camp + FVector(0.f, 0.f, WRTune(TEXT("world.landStartLift"), 200.f)));
			if (APlayerStart* S = World->SpawnActor<APlayerStart>(APlayerStart::StaticClass(), T, P))
			{
				S->PlayerStartTag = Tag;
				++Made;
			}
		}
		bool bSafe = false;
		for (TActorIterator<AWRSafeZone> It(World); It; ++It) bSafe |= FVector::Dist2D(It->GetActorLocation(), Camp) < SafeR;
		if (!bSafe)
		{
			if (AWRSafeZone* Z = World->SpawnActor<AWRSafeZone>(AWRSafeZone::StaticClass(), FTransform(Camp + FVector(0.f, 0.f, 100.f)), P))
			{
				Z->Radius = SafeR;
				++Made;
			}
		}
		const TArray<TSharedPtr<FJsonValue>>* Kinds = nullptr;
		if (c == StationsAt && C->TryGetArrayField(TEXT("stations"), Kinds) && Kinds->Num())
		{
			bool bStations = false;
			for (TActorIterator<AWRStation> It(World); It; ++It) bStations |= FVector::Dist2D(It->GetActorLocation(), Camp) < SafeR;
			if (!bStations)
			{
				const FVector2D Sea(FMath::Cos(FMath::DegreesToRadians(Yaw)), FMath::Sin(FMath::DegreesToRadians(Yaw)));
				const FVector2D Side(-Sea.Y, Sea.X);
				for (int32 k = 0; k < Kinds->Num(); ++k)
				{
					// In a row across, BackM inland of the camp's middle, 8 m apart, each facing the camp.
					const FVector2D Spot = M - Sea * BackM + Side * ((k - (Kinds->Num() - 1) * 0.5f) * 8.f);
					const FTransform T(FRotator(0.f, Yaw, 0.f), D.GroundPoint(Spot) + FVector(0.f, 0.f, 40.f));
					if (AWRStation* St = World->SpawnActorDeferred<AWRStation>(AWRStation::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
					{
						St->StationType = FName(*(*Kinds)[k]->AsString());
						St->FinishSpawning(T);
						++Made;
					}
				}
			}
		}
	}
	UE_LOG(LogWildreach, Log, TEXT("LAND %s camps: %s; made %d start(s) / safe zone(s) / station(s)"), *D.Id.ToString(), bHaveStart ? TEXT("starts already in the map") : TEXT("no starts in the map"), Made);
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
				// Wound to face into the hole, as its normals do (Unreal draws a triangle whose cross product points away
				// from the viewer, see OcDomeInward): the other way round the walls faced outward, so from inside the
				// Great Rift they weren't drawn and it showed open sea and sky (landmarks audit 11-18).
				Tris.Append({ A, E, B, A, C, E });
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
