// The Ancient forest drawn with Epic's Electric Dreams scans (cooks/wild-answers-1116.md 4.1). See WRForestArt.h.

#include "WRForestArt.h"
#include "WRTuning.h"
#include "Dom/JsonObject.h"

namespace WRForestArtImpl
{
	struct FTable
	{
		bool bLoaded = false;
		TMap<FString, WRForestArt::FKind> Kinds;
	};

	FVector Vec(const TSharedPtr<FJsonObject>& O, const TCHAR* Field)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (!O->TryGetArrayField(Field, A) || A->Num() < 3) return FVector::ZeroVector;
		return FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), (*A)[2]->AsNumber());
	}

	const FTable& Table()
	{
		static FTable T;
		if (T.bLoaded) return T;
		T.bLoaded = true;
		const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("forest_art.json"));
		const TSharedPtr<FJsonObject>* Kinds = nullptr;
		if (!Root.IsValid() || !Root->TryGetObjectField(TEXT("kinds"), Kinds)) return T;
		for (const TPair<FString, TSharedPtr<FJsonValue>>& KV : (*Kinds)->Values)
		{
			const TSharedPtr<FJsonObject> O = KV.Value.IsValid() ? KV.Value->AsObject() : nullptr;
			const TArray<TSharedPtr<FJsonValue>>* Meshes = nullptr;
			if (!O.IsValid() || !O->TryGetArrayField(TEXT("meshes"), Meshes) || Meshes->Num() == 0) continue;
			WRForestArt::FKind& K = T.Kinds.Add(KV.Key);
			K.Cull = float(O->GetNumberField(TEXT("cull")));
			O->TryGetBoolField(TEXT("shadow"), K.bShadow);
			double D = 0.0;
			if (O->TryGetNumberField(TEXT("sink"), D)) K.Sink = float(D);
			if (O->TryGetNumberField(TEXT("maxFit"), D)) K.MaxFit = float(D);
			FString Fit;
			K.bFitToKit = !O->TryGetStringField(TEXT("fit"), Fit) || Fit != TEXT("native");
			for (const TSharedPtr<FJsonValue>& V : *Meshes)
			{
				const TSharedPtr<FJsonObject> MO = V.IsValid() ? V->AsObject() : nullptr;
				if (!MO.IsValid()) continue;
				WRForestArt::FMesh& M = K.Meshes.AddDefaulted_GetRef();
				M.Path = MO->GetStringField(TEXT("path"));
				M.Height = FMath::Max(1.f, float(MO->GetNumberField(TEXT("height"))));
				M.MinZ = float(MO->GetNumberField(TEXT("minZ")));
				const TArray<TSharedPtr<FJsonValue>>* Cols = nullptr;
				if (MO->TryGetArrayField(TEXT("colliders"), Cols))
				{
					for (const TSharedPtr<FJsonValue>& CV : *Cols)
					{
						const TSharedPtr<FJsonObject> CO = CV.IsValid() ? CV->AsObject() : nullptr;
						if (!CO.IsValid()) continue;
						FWRTreeCollider& C = M.Colliders.AddDefaulted_GetRef();
						C.A = Vec(CO, TEXT("a"));
						C.B = Vec(CO, TEXT("b"));
						C.Radius = float(CO->GetNumberField(TEXT("r")));
					}
				}
				const TSharedPtr<FJsonObject>* Mats = nullptr;
				if (MO->TryGetObjectField(TEXT("materials"), Mats))
				{
					for (const TPair<FString, TSharedPtr<FJsonValue>>& MV : (*Mats)->Values)
					{
						M.Materials.Add(FCString::Atoi(*MV.Key), MV.Value->AsString());
					}
				}
			}
			if (K.Meshes.Num() == 0) T.Kinds.Remove(KV.Key);
		}
		return T;
	}
}

const WRForestArt::FKind* WRForestArt::Find(const FString& Kind)
{
	return WRForestArtImpl::Table().Kinds.Find(Kind);
}

FTransform WRForestArt::Place(const FKind& K, const FMesh& M, const FVector& Ground, float Yaw, float Scale, float KitHeight, const FVector& Normal)
{
	float S = Scale;
	if (K.bFitToKit)
	{
		// The height the generator wanted (its scale x the kit species' reference height), from this scan's own height,
		// no more than MaxFit x the scan (a scan blown far past its size shows: leaves the size of plates).
		S = Scale * KitHeight / M.Height;
		if (K.MaxFit > 0.f) S = FMath::Min(S, K.MaxFit);
	}
	S = FMath::Max(S, 0.05f);
	// Stood on its base (the scan's lowest point at the ground), pressed in by Sink of its drawn height.
	const FQuat Q = Normal.Equals(FVector::UpVector, 1e-3)
		? FRotator(0.f, Yaw, 0.f).Quaternion()
		: FRotationMatrix::MakeFromZX(Normal.GetSafeNormal(), FRotator(0.f, Yaw, 0.f).Vector()).ToQuat();
	const FVector Up = Q.GetUpVector();
	return FTransform(Q, Ground + Up * ((-M.MinZ - K.Sink * M.Height) * S), FVector(S));
}
