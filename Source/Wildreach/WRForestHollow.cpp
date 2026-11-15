// The Hollow Root and the Ancient forest's secrets (cooks/biome-forest.md 2.5, 2.22, 2.4 #14/#16/#26; cooks/build-biome-
// forest.md 2.8, 2.15, 2.16). See WRForestHollow.h. Placement from Data/forest.json (its raw "hollow", "routes", "spots",
// "rivers" and "sinkroot" rows, read here because FWRForestData keeps only part of them), numbers from tuning
// hazards.hollow.* and hazards.secrets.*.

#include "WRForestHollow.h"
#include "Wildreach.h"
#include "WRForestHazards.h"
#include "WRForestTypes.h"
#include "WRForestWorld.h"
#include "WRForestCreatures.h"
#include "WRTreeKit.h"
#include "WRToolMesh.h"
#include "WRTuning.h"
#include "WRSound.h"
#include "WRFx.h"
#include "WRLand.h"
#include "WRCreature.h"
#include "WRCharacter.h"
#include "WRCharacterBase.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRInventory.h"
#include "WRGameState.h"
#include "WRSurvival.h"
#include "WRWorldSystems.h"
#include "WRResourceNode.h"
#include "WRUIMap.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"

namespace WRForestHollowImpl
{
	float Tn(const TCHAR* Key, float Fallback) { return WRTune(*(FString(TEXT("hazards.hollow.")) + Key), Fallback); }
	float Ts(const TCHAR* Key, float Fallback) { return WRTune(*(FString(TEXT("hazards.secrets.")) + Key), Fallback); }
	FString Str(const TCHAR* Path, const TCHAR* Fallback) { return FWRTuning::Get().Str(Path, Fallback); }
	float Now(const UObject* Ctx) { return UWRWeatherSubsystem::Now(Ctx); }
	double Hours(const UObject* Ctx) { const AWRGameState* GS = AWRGameState::Get(Ctx); return GS ? GS->GetTotalHours() : 0.0; }
	int32 Day(const UObject* Ctx) { const AWRGameState* GS = AWRGameState::Get(Ctx); return GS ? GS->GetDay() : 0; }
	FLinearColor Rgb(uint8 R, uint8 G, uint8 B) { return FLinearColor(FColor(R, G, B)); }

	// The Hollow Root's palette (cooks/build-biome-forest.md 2.3, DC 2.5): root bark brown over black soil, foxfire green,
	// glowworm blue-green, old bone, Grovekin amber sap and fired clay.
	const FLinearColor RootBark = Rgb(74, 58, 44);
	const FLinearColor RootDark = Rgb(46, 36, 28);
	const FLinearColor Soil = Rgb(40, 30, 22);
	const FLinearColor Moss = Rgb(94, 140, 49);
	const FLinearColor Bone = Rgb(214, 204, 180);
	const FLinearColor BoneOld = Rgb(160, 150, 128);
	const FLinearColor Amber = Rgb(208, 138, 30);
	const FLinearColor Stone = Rgb(120, 116, 108);
	const FLinearColor Clay = Rgb(150, 96, 62);
	const FLinearColor Paper = Rgb(150, 146, 136);
	const FLinearColor WaspGold = Rgb(196, 160, 40);
	const FLinearColor Black = Rgb(20, 18, 16);
	const FLinearColor Wicker = Rgb(132, 104, 64);
	const FLinearColor Iron = Rgb(110, 70, 52);
	const FColor FoxfireLight(127, 224, 122);
	const FColor GlowwormLight(110, 216, 232);

	// ---------------------------------------------------------------------------------------------- the raw data
	struct FRoomX
	{
		FName Id; FString Name;
		FVector2D C = FVector2D::ZeroVector;
		float Floor = 0.f, Entry = 0.f, Roof = 0.f, Hx = 1000.f, Hy = 1000.f, Yaw = 0.f, Water = 0.f, Chasm = 0.f;
		/** Room-local (along its yaw) of a world point. */
		FVector2D Local(const FVector2D& P) const
		{
			const float c = FMath::Cos(FMath::DegreesToRadians(Yaw)), s = FMath::Sin(FMath::DegreesToRadians(Yaw));
			const FVector2D D = P - C;
			return FVector2D(D.X * c + D.Y * s, -D.X * s + D.Y * c);
		}
		FVector2D World(const FVector2D& L) const
		{
			const float c = FMath::Cos(FMath::DegreesToRadians(Yaw)), s = FMath::Sin(FMath::DegreesToRadians(Yaw));
			return C + FVector2D(L.X * c - L.Y * s, L.X * s + L.Y * c);
		}
		float Bottom() const { return Floor; }   // "z" in the data is already the chasm's bottom
	};
	struct FWay { FName Id; FName Kind; float Width = 800.f; TArray<FVector> Pts; };
	struct FSpotX { FName Kind; FVector At = FVector::ZeroVector; float R = 0.f, Depth = 0.f, Yaw = 0.f; };
	struct FData
	{
		bool bLoaded = false;
		TArray<FRoomX> Rooms;
		TArray<FWay> Ways;        // routes kind hollow (hollow_way, heart_root) and lost_path (lost_path_loop, lost_path_true)
		TArray<FSpotX> Spots;
		bool bSink = false;
		FVector2D Sink = FVector2D::ZeroVector;
		float SinkFloor = 0.f, SinkRim = 0.f, SinkRFloor = 2800.f;
		float PoolZ = 0.f;        // rivers kind pool "drowned_root": its surface

		int32 RoomIndex(FName Id) const { for (int32 i = 0; i < Rooms.Num(); ++i) if (Rooms[i].Id == Id) return i; return INDEX_NONE; }
		const FRoomX* Room(FName Id) const { const int32 i = RoomIndex(Id); return i == INDEX_NONE ? nullptr : &Rooms[i]; }
		const FWay* Way(FName Id) const { for (const FWay& W : Ways) if (W.Id == Id) return &W; return nullptr; }
		const FSpotX* Spot(std::initializer_list<const TCHAR*> Kinds) const
		{
			for (const TCHAR* K : Kinds) for (const FSpotX& S : Spots) if (S.Kind == FName(K)) return &S;
			return nullptr;
		}
	};

	float JNum(const TSharedPtr<FJsonObject>& O, const TCHAR* K, float Fb)
	{
		double V = Fb;
		return O.IsValid() && O->TryGetNumberField(K, V) ? float(V) : Fb;
	}
	FVector JPoint(const TSharedPtr<FJsonValue>& V)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (!V.IsValid() || !V->TryGetArray(A) || A->Num() < 2) return FVector::ZeroVector;
		return FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), A->Num() > 2 ? (*A)[2]->AsNumber() : 0.0) * 100.0;
	}

	const FData& Data()
	{
		static FData D;
		if (D.bLoaded) return D;
		D.bLoaded = true;
		const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("forest.json"));
		if (!Root.IsValid()) { D.bLoaded = false; return D; }
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (Root->TryGetArrayField(TEXT("hollow"), Arr))
		{
			for (const TSharedPtr<FJsonValue>& V : *Arr)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				if (!O.IsValid()) continue;
				FRoomX R;
				R.Id = FName(*O->GetStringField(TEXT("id")));
				O->TryGetStringField(TEXT("name"), R.Name);
				R.C = FVector2D(JNum(O, TEXT("x"), 0.f), JNum(O, TEXT("y"), 0.f)) * 100.f;
				R.Floor = JNum(O, TEXT("z"), 0.f) * 100.f;
				R.Entry = JNum(O, TEXT("entry"), R.Floor / 100.f) * 100.f;
				R.Roof = JNum(O, TEXT("roof"), R.Floor / 100.f + 10.f) * 100.f;
				R.Hx = JNum(O, TEXT("hx"), 10.f) * 100.f;
				R.Hy = JNum(O, TEXT("hy"), 10.f) * 100.f;
				R.Yaw = JNum(O, TEXT("yaw"), 0.f);
				R.Water = JNum(O, TEXT("water"), 0.f) * 100.f;
				R.Chasm = JNum(O, TEXT("chasm"), 0.f) * 100.f;
				D.Rooms.Add(R);
			}
		}
		if (Root->TryGetArrayField(TEXT("routes"), Arr))
		{
			for (const TSharedPtr<FJsonValue>& V : *Arr)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				FString Kind;
				if (!O.IsValid() || !O->TryGetStringField(TEXT("kind"), Kind) || (Kind != TEXT("hollow") && Kind != TEXT("lost_path"))) continue;
				FWay W;
				W.Id = FName(*O->GetStringField(TEXT("id")));
				W.Kind = FName(*Kind);
				W.Width = JNum(O, TEXT("w"), 8.f) * 100.f;
				const TArray<TSharedPtr<FJsonValue>>* Pts = nullptr;
				if (O->TryGetArrayField(TEXT("pts"), Pts)) for (const TSharedPtr<FJsonValue>& P : *Pts) W.Pts.Add(JPoint(P));
				if (W.Pts.Num() >= 2) D.Ways.Add(W);
			}
		}
		if (Root->TryGetArrayField(TEXT("spots"), Arr))
		{
			for (const TSharedPtr<FJsonValue>& V : *Arr)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				if (!O.IsValid()) continue;
				FSpotX S;
				S.Kind = FName(*O->GetStringField(TEXT("kind")));
				S.At = FVector(JNum(O, TEXT("x"), 0.f), JNum(O, TEXT("y"), 0.f), JNum(O, TEXT("z"), 0.f)) * 100.f;
				S.R = JNum(O, TEXT("r"), 0.f) * 100.f;
				S.Depth = JNum(O, TEXT("depth"), 0.f) * 100.f;
				S.Yaw = JNum(O, TEXT("yaw"), 0.f);
				D.Spots.Add(S);
			}
		}
		if (Root->TryGetArrayField(TEXT("rivers"), Arr))
		{
			for (const TSharedPtr<FJsonValue>& V : *Arr)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				FString Kind;
				const TArray<TSharedPtr<FJsonValue>>* Pts = nullptr;
				if (O.IsValid() && O->TryGetStringField(TEXT("kind"), Kind) && Kind == TEXT("pool") && O->TryGetArrayField(TEXT("pts"), Pts) && Pts->Num() > 0)
				{
					D.PoolZ = JPoint((*Pts)[0]).Z;
				}
			}
		}
		const TSharedPtr<FJsonObject>* Sink = nullptr;
		if (Root->TryGetObjectField(TEXT("sinkroot"), Sink) && Sink->IsValid())
		{
			D.bSink = true;
			D.Sink = FVector2D(JNum(*Sink, TEXT("x"), 0.f), JNum(*Sink, TEXT("y"), 0.f)) * 100.f;
			D.SinkFloor = JNum(*Sink, TEXT("floor"), 0.f) * 100.f;
			D.SinkRim = JNum(*Sink, TEXT("rim"), 0.f) * 100.f;
			D.SinkRFloor = JNum(*Sink, TEXT("rFloor"), 28.f) * 100.f;
		}
		UE_LOG(LogWildreach, Log, TEXT("FOREST hollow data: %d rooms, %d ways, %d spots"), D.Rooms.Num(), D.Ways.Num(), D.Spots.Num());
		return D;
	}

	/** Ground height under a point: the runtime land's own heights first (no collision needed), then a trace. */
	bool GroundZ(const UWorld* W, const FVector2D& XY, float From, float& OutZ)
	{
		if (WRLand::GroundUnder(FVector(XY, From), OutZ)) return true;
		if (!W) return false;
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRHollowGround), false);
		if (W->LineTraceSingleByChannel(Hit, FVector(XY, From + 3000.f), FVector(XY, From - 9000.f), ECC_WorldStatic, Q) && !Cast<APawn>(Hit.GetActor()))
		{
			OutZ = Hit.ImpactPoint.Z;
			return true;
		}
		return false;
	}

	/** A floor point inside the Hollow Root (a trace between roof and floor, so the roof slabs above are never hit). */
	FVector Floor(const UWorld* W, const FVector2D& XY, float Z)
	{
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRHollowFloor), false);
		if (W && W->LineTraceSingleByChannel(Hit, FVector(XY, Z + 250.f), FVector(XY, Z - 900.f), ECC_WorldStatic, Q) && !Cast<APawn>(Hit.GetActor())) return Hit.ImpactPoint;
		float G = Z;
		if (WRLand::GroundUnder(FVector(XY, Z), G) && FMath::Abs(G - Z) < 800.f) return FVector(XY, G);
		return FVector(XY, Z);
	}

	float Jit(const FVector& P, int32 Salt)
	{
		const float S = FMath::Sin(P.X * 0.0123f + P.Y * 0.0457f + P.Z * 0.0031f + Salt * 12.9898f) * 43758.5453f;
		return FMath::Frac(S) * 2.f - 1.f;
	}

	UProceduralMeshComponent* NewMesh(AActor* Owner, const TCHAR* Name)
	{
		UProceduralMeshComponent* M = NewObject<UProceduralMeshComponent>(Owner, MakeUniqueObjectName(Owner, UProceduralMeshComponent::StaticClass(), FName(Name)));
		M->SetupAttachment(Owner->GetRootComponent());
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M->SetCanEverAffectNavigation(false);
		M->RegisterComponent();
		return M;
	}

	/** A mesh drawn in the held-tool material, or glowing (foxfire, glowworms, sap: WRTreeKit's glow material). */
	UProceduralMeshComponent* Draw(AActor* Owner, const FWRToolMesh& M, const TCHAR* Name, const FLinearColor* GlowTint = nullptr, float Glow = 1.f)
	{
		if (M.Verts.Num() == 0) return nullptr;
		UProceduralMeshComponent* C = NewMesh(Owner, Name);
		M.ApplyTo(C);
		if (GlowTint)
		{
			C->SetCastShadow(false);
			if (UMaterialInstanceDynamic* Mi = WRTreeKit::GlowMaterial(Owner, *GlowTint, Glow)) C->SetMaterial(0, Mi);
		}
		return C;
	}

	UBoxComponent* Solid(AActor* Owner, const FTransform& At, const FVector& Half)
	{
		UBoxComponent* B = NewObject<UBoxComponent>(Owner);
		B->SetupAttachment(Owner->GetRootComponent());
		B->SetUsingAbsoluteLocation(true);
		B->SetUsingAbsoluteRotation(true);
		B->SetBoxExtent(Half.ComponentMax(FVector(2.f)));
		B->SetWorldTransform(FTransform(At.GetRotation(), At.GetLocation()));
		B->SetCollisionProfileName(TEXT("BlockAll"));
		B->SetCanEverAffectNavigation(false);
		B->SetHiddenInGame(true);
		B->RegisterComponent();
		return B;
	}

	/** A box between two points (a step, a beam, a ramp): HalfW across, HalfH up. */
	UBoxComponent* SolidSeg(AActor* Owner, const FVector& P0, const FVector& P1, float HalfW, float HalfH)
	{
		const FVector D = P1 - P0;
		if (D.Size() < 1.f) return nullptr;
		return Solid(Owner, FTransform(D.Rotation(), (P0 + P1) * 0.5f), FVector(D.Size() * 0.5f, HalfW, HalfH));
	}

	UPointLightComponent* Lamp(AActor* Owner, const FVector& At, const FColor& Colour, float Intensity, float Radius)
	{
		UPointLightComponent* L = NewObject<UPointLightComponent>(Owner);
		L->SetupAttachment(Owner->GetRootComponent());
		L->SetUsingAbsoluteLocation(true);
		L->SetWorldLocation(At);
		L->SetLightColor(FLinearColor(Colour));
		L->SetIntensity(Intensity);
		L->SetAttenuationRadius(Radius);
		L->SetCastShadows(false);
		L->RegisterComponent();
		return L;
	}

	void Ball(FWRToolMesh& M, const FVector& C, float R, const FLinearColor& Col, float Rough, float Squash = 1.f)
	{
		M.Lathe(C - FVector(0, 0, R * Squash), FVector::UpVector, 2.f * R * Squash, { FVector(0.f, R * 0.15f, R * 0.15f), FVector(0.25f, R * 0.87f, R * 0.87f), FVector(0.5f, R, R), FVector(0.75f, R * 0.87f, R * 0.87f), FVector(1.f, R * 0.15f, R * 0.15f) }, 8, Col, Rough);
	}

	/** A root: a tapering tube along a path (thick at its first point), knuckled a little. */
	void Root(FWRToolMesh& M, const TArray<FVector>& Pts, float R0, float R1, const FLinearColor& Col)
	{
		if (Pts.Num() < 2) return;
		// Tube takes one radius: draw it as stretches that taper from R0 to R1.
		for (int32 i = 0; i + 1 < Pts.Num(); ++i)
		{
			const float A = FMath::Lerp(R0, R1, float(i) / (Pts.Num() - 1)), B = FMath::Lerp(R0, R1, float(i + 1) / (Pts.Num() - 1));
			const FVector D = Pts[i + 1] - Pts[i];
			const float L = D.Size();
			if (L < 1.f) continue;
			M.Lathe(Pts[i] - D / L * (A * 0.3f), D / L, L + A * 0.3f, { FVector(0.f, A, A * 0.92f), FVector(0.5f, (A + B) * 0.52f, (A + B) * 0.48f), FVector(1.f, B, B * 0.92f) }, 8, Col, 0.92f, 0.f, float(i));
		}
	}

	/** Foxfire on a surface: a cluster of little glowing brackets and a thread of mycelium running from it. */
	void Foxfire(FWRToolMesh& G, const FVector& At, const FVector& Out, int32 Seed)
	{
		const FVector N = Out.GetSafeNormal();
		const FVector Side = FVector::CrossProduct(N, FVector::UpVector).GetSafeNormal();
		for (int32 i = 0; i < 6; ++i)
		{
			const FVector P = At + Side * (Jit(At, Seed + i) * 40.f) + FVector(0, 0, Jit(At, Seed + 20 + i) * 30.f);
			G.Lathe(P, (N + FVector(0, 0, 0.3f)).GetSafeNormal(), 4.f + 3.f * (i % 2), { FVector(0.f, 6.f, 3.f), FVector(1.f, 3.f, 1.5f) }, 6, FLinearColor(0.55f, 1.f, 0.5f), 0.6f);
		}
		G.Tube({ At, At + Side * 60.f + FVector(0, 0, -20.f), At + Side * 110.f + FVector(0, 0, 15.f) }, 0.9f, 4, FLinearColor(0.5f, 0.9f, 0.45f), 0.6f);
	}

	/** A glowworm curtain: dozens of beaded silk threads hanging from a point under the roof (Arachnocampa: blue-green
	 *  bulbs at the top, sticky droplets down each thread), drawn in the curtain's own frame (origin = its top). */
	void Curtain(FWRToolMesh& G, float Radius, int32 Seed)
	{
		const FVector O(Seed, Seed * 3, 0);
		for (int32 t = 0; t < 34; ++t)
		{
			const float A = (Jit(O, t) * 0.5f + 0.5f) * 2.f * PI, R = Radius * FMath::Sqrt(Jit(O, t + 50) * 0.5f + 0.5f);
			const FVector Top(FMath::Cos(A) * R, FMath::Sin(A) * R, 0.f);
			const float Len = 30.f + 90.f * (Jit(O, t + 100) * 0.5f + 0.5f);
			G.Tube({ Top, Top - FVector(0, 0, Len) }, 0.35f, 3, FLinearColor(0.55f, 0.9f, 1.f), 0.5f);
			for (float d = 8.f; d < Len; d += 11.f) Ball(G, Top - FVector(0, 0, d), 0.9f, FLinearColor(0.6f, 0.95f, 1.f), 0.4f);
			Ball(G, Top - FVector(0, 0, 2.f), 2.2f, FLinearColor(0.45f, 0.95f, 1.f), 0.4f);
		}
	}

	/** Fitted roof frame: a plane through the real ground round a rect (so the roof meets the forest floor at its edges
	 *  even on a slope), centred on the rect, turned by its yaw. Samples 4 m outside the rect; samples that fall into
	 *  another pit (well under the floor's top) are dropped. */
	FTransform RoofFrame(const UWorld* W, const FVector2D& Centre, float Yaw, float HalfX, float HalfY, float FloorZ, float FallbackZ, float& OutCentreZ)
	{
		const FRotator YawR(0.f, Yaw, 0.f);
		double S[3][3] = { { 0 } }, R[3] = { 0 };
		int32 N = 0;
		for (int32 i = -1; i <= 1; ++i)
		{
			for (int32 j = -1; j <= 1; ++j)
			{
				if (i == 0 && j == 0) continue;
				const FVector L(i * (HalfX + 400.f), j * (HalfY + 400.f), 0.f);
				const FVector P = YawR.RotateVector(L) + FVector(Centre, 0.f);
				float Z = 0.f;
				if (!GroundZ(W, FVector2D(P), FallbackZ, Z) || Z < FloorZ + 300.f) continue;
				const double X[3] = { 1.0, L.X, L.Y };
				for (int32 a = 0; a < 3; ++a) { for (int32 b = 0; b < 3; ++b) S[a][b] += X[a] * X[b]; R[a] += X[a] * Z; }
				++N;
			}
		}
		double A0 = FallbackZ, B = 0.0, C = 0.0;
		if (N >= 3)
		{
			// Cramer's rule on the 3x3 normal equations.
			const auto Det = [](const double M[3][3]) { return M[0][0] * (M[1][1] * M[2][2] - M[1][2] * M[2][1]) - M[0][1] * (M[1][0] * M[2][2] - M[1][2] * M[2][0]) + M[0][2] * (M[1][0] * M[2][1] - M[1][1] * M[2][0]); };
			const double D0 = Det(S);
			if (FMath::Abs(D0) > 1e-6)
			{
				double Sol[3];
				for (int32 k = 0; k < 3; ++k)
				{
					double M[3][3];
					for (int32 a = 0; a < 3; ++a) for (int32 b = 0; b < 3; ++b) M[a][b] = b == k ? R[a] : S[a][b];
					Sol[k] = Det(M) / D0;
				}
				A0 = Sol[0]; B = FMath::Clamp(Sol[1], -0.45, 0.45); C = FMath::Clamp(Sol[2], -0.45, 0.45);
			}
		}
		else if (N > 0)
		{
			A0 = R[0] / N;
		}
		OutCentreZ = float(A0);
		const FVector Xa = FVector(1.f, 0.f, float(B)).GetSafeNormal();
		const FVector Ya0 = FVector(0.f, 1.f, float(C)).GetSafeNormal();
		const FVector Za = FVector::CrossProduct(Xa, Ya0).GetSafeNormal();
		const FVector Ya = FVector::CrossProduct(Za, Xa).GetSafeNormal();
		const FQuat Tilt = FMatrix(Xa, Ya, Za, FVector::ZeroVector).ToQuat();
		return FTransform(FQuat(YawR) * Tilt, FVector(Centre, float(A0)));
	}

	/** The Truffler's soft spot (cooks/biome-forest.md 2.5, way 3): the generator's shaft at (-220, +3,080), 2 m round,
	 *  dug down to the Cradle Vault's roof level (2.5 m under the forest floor); the plug sits on that level, a root slide
	 *  under it running on into the vault (Tools/terrain/forest.mjs, "The Truffler's dig"). OutLocal is the shaft in the
	 *  vault's frame (outside its rect: the slide's roof joins the vault's). */
	bool PlugSpot(FVector& OutAt, FVector2D& OutLocal, int32& OutRoom)
	{
		const FData& D = Data();
		OutRoom = D.RoomIndex(TEXT("cradle_vault"));
		if (OutRoom == INDEX_NONE) return false;
		const FRoomX& R = D.Rooms[OutRoom];
		const FSpotX* S = D.Spot({ TEXT("truffler_dig"), TEXT("hollow_dig") });
		const FVector2D Want = S ? FVector2D(S->At) : FVector2D(-22000.f, 308000.f);
		const float Top = S ? S->At.Z : R.Roof;
		OutLocal = R.Local(Want);
		// The generator's plug level: 2.5 m under the floor it was dug from, never below the vault's roof level.
		const float Hz = FMath::Clamp((R.Roof - R.Entry) * 0.5f, 400.f, 1200.f);
		float Level = FMath::Min(Top - 250.f, R.Entry + 2.f * Hz);
		float Carved = 0.f;
		if (WRLand::GroundUnder(FVector(Want, Top), Carved) && Carved < Top - 100.f && Carved > Level - 300.f) Level = Carved;
		OutAt = FVector(Want, Level);
		return true;
	}

	/** Is the plug's shaft over the vault's own rect (an older generator's layout)? Then the vault's roof parts round it. */
	bool PlugOverRoom(const FRoomX& R, const FVector2D& Local, float Pad)
	{
		return FMath::Abs(Local.X) <= R.Hx + Pad && FMath::Abs(Local.Y) <= R.Hy + Pad;
	}

	/** The Heart Root's one-way door: where the tunnel's floor comes within doorDepth of its mouth (from inside only). */
	bool HeartDoor(FVector& OutAt, FVector& OutOut, float& OutWidth)
	{
		const FData& D = Data();
		const FWay* W = D.Way(TEXT("heart_root"));
		if (!W || W->Pts.Num() < 2) return false;
		const float Mouth = W->Pts.Last().Z;
		const float Want = Mouth - Tn(TEXT("doorDepth"), 500.f);
		for (int32 i = W->Pts.Num() - 2; i >= 0; --i)
		{
			const FVector A = W->Pts[i], B = W->Pts[i + 1];
			if (A.Z <= Want && B.Z >= Want)
			{
				const float T = FMath::Clamp((Want - A.Z) / FMath::Max(1.f, B.Z - A.Z), 0.f, 1.f);
				OutAt = FMath::Lerp(A, B, T);
				OutOut = (B - A).GetSafeNormal2D();
				OutWidth = W->Width;
				return true;
			}
		}
		OutAt = W->Pts.Last();
		OutOut = (W->Pts.Last() - W->Pts[W->Pts.Num() - 2]).GetSafeNormal2D();
		OutWidth = W->Width;
		return true;
	}

	/** The Lost Path's hunter's cache: in a buttress hollow just past the gap in the thornbrake wall at the true path's
	 *  end (F-S1), on the far side from the last bend. */
	bool LostCacheSpot(const UWorld* W, FVector& OutAt, float& OutYaw)
	{
		const FData& D = Data();
		const FWay* True = D.Way(TEXT("lost_path_true"));
		if (!True || True->Pts.Num() < 3) return false;
		const FWRForestData& FD = FWRForestData::Get();
		const FWRForestData::FBrake* Wall = nullptr;
		for (const FWRForestData::FBrake& B : FD.Brakes) if (B.Guards == TEXT("lost_path_end")) Wall = &B;
		const FVector Before = True->Pts[True->Pts.Num() - 2];
		FVector Centre = True->Pts.Last();
		FVector Normal = (Centre - Before).GetSafeNormal2D();
		float Depth = 600.f;
		if (Wall)
		{
			Centre = Wall->Centre;
			Depth = Wall->Depth;
			FVector N2 = FRotator(0.f, Wall->Yaw + 90.f, 0.f).Vector();
			if (FVector::DotProduct(N2, Centre - Before) < 0.f) N2 = -N2;
			Normal = N2;
		}
		const FVector2D P = FVector2D(Centre + Normal * (Depth * 0.5f + Ts(TEXT("lostCacheBeyond"), 500.f)));
		float Z = Centre.Z;
		GroundZ(W, P, Centre.Z, Z);
		OutAt = FVector(P, Z);
		OutYaw = Normal.Rotation().Yaw + 180.f;   // the hollow opens back toward the gap
		return true;
	}

	/** The bell (F-S5): where the bell hangs on the Bell-Tree (the kit's anchor, as the landmark draws it) and the trunk. */
	bool BellSpot(FVector& OutBell, FVector& OutBase, FRotator& OutRot, float& OutRadius, float& OutScale, int32& OutVar, float& OutHeight)
	{
		for (const FWRForestData::FTree& T : FWRForestData::Get().Giants)
		{
			if (T.Kind != TEXT("bell_tree")) continue;
			OutBase = T.Base;
			OutRot = FRotator(0.f, T.Yaw, 0.f);
			OutRadius = FMath::Max(60.f, T.Radius);
			OutVar = ((T.Variant % 4) + 4) % 4;
			OutHeight = T.Height;
			const float Ref = FMath::Max(1.f, WRTreeKit::ReferenceHeight(TEXT("bell_tree")));
			OutScale = T.Height / Ref;
			const FVector A0 = WRTreeKit::Anchor(TEXT("bell_tree"), OutVar, TEXT("bell"));
			OutBell = A0.IsNearlyZero() ? T.Base + OutRot.RotateVector(FVector(OutRadius + 600.f, 0.f, WRTune(TEXT("forestWorld.bellHeight"), 4000.f))) : T.Base + OutRot.RotateVector(A0 * OutScale);
			return true;
		}
		return false;
	}

	/** The trunk's centre and radius at a height (the kit's own trunk capsules, so the holds hug a leaning bole). */
	void TrunkAt(const FVector& Base, const FRotator& Rot, float Scale, int32 Var, float Z, float FallbackR, FVector& OutC, float& OutR)
	{
		OutC = FVector(Base.X, Base.Y, Z);
		OutR = FallbackR;
		float Best = 1e9f;
		for (const FWRTreeCollider& C : WRTreeKit::Colliders(TEXT("bell_tree"), Var))
		{
			const FVector A = Base + Rot.RotateVector(C.A * Scale), B = Base + Rot.RotateVector(C.B * Scale);
			const FVector D = B - A;
			if (FMath::Abs(D.GetSafeNormal().Z) < 0.5f) continue;   // a limb, not the bole
			const float Lo = FMath::Min(A.Z, B.Z), Hi = FMath::Max(A.Z, B.Z);
			const float Off = Z < Lo ? Lo - Z : Z > Hi ? Z - Hi : 0.f;
			if (Off >= Best) continue;
			Best = Off;
			const float T = FMath::Clamp((Z - A.Z) / (FMath::Abs(D.Z) > 1.f ? D.Z : 1.f), 0.f, 1.f);
			OutC = FMath::Lerp(A, B, T);
			OutC.Z = Z;
			OutR = C.Radius * Scale;
		}
	}

	/** Bread-stone Lane's barred house-tree door (F-S4): the lane's scenery draws a bar of collision across that one
	 *  doorway (70 x 10 x 105 cm half size); find it near Around. */
	UBoxComponent* FindDoorBar(const UWorld* W, const FVector& Around, float Range)
	{
		UBoxComponent* Best = nullptr;
		float BestD = Range;
		for (TActorIterator<AWRForestScenery> It(const_cast<UWorld*>(W)); It; ++It)
		{
			if (It->Kind != TEXT("bread_stone_lane") && It->SiteId != TEXT("bread_stone_lane")) continue;
			TArray<UBoxComponent*> Boxes;
			It->GetComponents<UBoxComponent>(Boxes);
			for (UBoxComponent* B : Boxes)
			{
				const FVector E = B->GetUnscaledBoxExtent();
				if (FMath::Abs(E.X - 70.f) > 20.f || E.Y > 25.f || FMath::Abs(E.Z - 105.f) > 25.f) continue;
				const float D = FVector::Dist(B->GetComponentLocation(), Around);
				if (D < BestD) { BestD = D; Best = B; }
			}
		}
		return Best;
	}

	/** An item from the tuning if items.json has it, else the fallback (secrets invent a few ids the world track adds). */
	FName ItemOr(const TCHAR* TuneKey, const TCHAR* Want, const TCHAR* Fallback)
	{
		const FName Id(*Str(TuneKey, Want));
		if (FWRItemRegistry::Get().Find(Id)) return Id;
		UE_LOG(LogWildreach, Warning, TEXT("FOREST secrets: no item '%s' in items.json, giving '%s'"), *Id.ToString(), Fallback);
		return FName(Fallback);
	}

	FString ItemName(FName Id)
	{
		const FWRItemDef* Def = FWRItemRegistry::Get().Find(Id);
		return Def ? Def->Name : Id.ToString();
	}

	AWRPlayerState* StateOf(AWRPlayerController* PC) { return PC ? PC->GetPlayerState<AWRPlayerState>() : nullptr; }

	/** A blade in hand (the knife rule for the bootlace laces, DC 2.7). */
	bool HoldsBlade(const AActor* Who)
	{
		const UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Who);
		if (!Inv) return false;
		const FWRItemStack& H = Inv->GetEquipped(EWREquipSlot::Weapon);
		if (H.IsEmpty()) return false;
		if (FWRGathering::IsSharp(H)) return true;
		const FWRItemDef* Def = FWRItemRegistry::Get().Find(H.ItemId);
		return Def && (Def->WeaponClass == TEXT("dagger") || Def->WeaponClass == TEXT("axe") || Def->WeaponClass == TEXT("greatsword"));
	}

	void Tell(const AActor* Pawn, const FString& Text)
	{
		const APawn* P = Cast<APawn>(Pawn);
		if (AWRPlayerController* PC = P ? Cast<AWRPlayerController>(P->GetController()) : nullptr) PC->ClientNotify(Text);
	}

	AWRResourceNode* Node(UWorld* W, FName Item, const FVector& At, int32 Hits, int32 PerHit, float Respawn, float Gem = -1.f)
	{
		const FTransform Tr(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), At);
		AWRResourceNode* N = W->SpawnActorDeferred<AWRResourceNode>(AWRResourceNode::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!N) return nullptr;
		N->ItemId = Item;
		N->Hits = FMath::Max(1, Hits);
		N->PerHit = FMath::Max(1, PerHit);
		N->RespawnSeconds = Respawn;
		if (Gem >= 0.f) N->GemChance = Gem;
		N->FinishSpawning(Tr);
		return N;
	}

	AWRWorldCache* Cache(UWorld* W, const FVector& At, float Yaw, const FString& Title, const TMap<FName, int32>& Items, const FString& Lore, float RefillHours)
	{
		const FTransform Tr(FRotator(0.f, Yaw, 0.f), At);
		AWRWorldCache* C = W->SpawnActorDeferred<AWRWorldCache>(AWRWorldCache::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!C) return nullptr;
		C->Kind = TEXT("chest");
		C->Biome = TEXT("forest");
		C->Title = Title;
		C->Items = Items;
		C->Lore = Lore;
		C->RefillHours = RefillHours;
		C->FinishSpawning(Tr);
		return C;
	}

	float GameHoursToSeconds(float H) { return H * WRTune(TEXT("clock.dayMinutes"), 60.f) * 60.f / 24.f; }
}

// ================================================================================================ queries

bool WRForestHollowContains(const FVector& Pos)
{
	using namespace WRForestHollowImpl;
	const FData& D = Data();
	if (D.Rooms.Num() == 0) return false;
	for (const FRoomX& R : D.Rooms)
	{
		const FVector2D L = R.Local(FVector2D(Pos));
		if (FMath::Abs(L.X) <= R.Hx + 100.f && FMath::Abs(L.Y) <= R.Hy + 100.f && Pos.Z < R.Roof - 50.f && Pos.Z > R.Floor - 300.f) return true;
	}
	for (const FWay& W : D.Ways)
	{
		if (W.Kind != TEXT("hollow")) continue;
		for (int32 i = 0; i + 1 < W.Pts.Num(); ++i)
		{
			float T = 0.f;
			const float Dist = WRLand::DistToSegment(FVector2D(Pos), FVector2D(W.Pts[i]), FVector2D(W.Pts[i + 1]), &T);
			if (Dist > W.Width * 0.5f + 100.f) continue;
			const float FloorZ = FMath::Lerp(W.Pts[i].Z, W.Pts[i + 1].Z, T);
			// Under the roof: the passages run 10-60 m down; their mouths (within ~4 m of the surface) are open cuts.
			if (Pos.Z > FloorZ - 300.f && Pos.Z < FloorZ + Tn(TEXT("passageHeadroom"), 900.f)) return true;
		}
	}
	return false;
}

FName WRForestHollow::RareSeed()
{
	// The rare row of every biome's wild seeds (farming.json wildSeeds: the one with the lowest chance), one at random.
	static TArray<FName> Pool;
	if (Pool.Num() == 0)
	{
		const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("farming.json"));
		const TSharedPtr<FJsonObject>* Wild = nullptr;
		if (Root.IsValid() && Root->TryGetObjectField(TEXT("wildSeeds"), Wild) && Wild->IsValid())
		{
			for (const auto& KV : (*Wild)->Values)
			{
				const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
				if (!KV.Value->TryGetArray(Rows)) continue;
				FString Best;
				double BestChance = 2.0;
				for (const TSharedPtr<FJsonValue>& V : *Rows)
				{
					const TSharedPtr<FJsonObject> O = V->AsObject();
					double C = 1.0;
					FString Seed;
					if (O.IsValid() && O->TryGetStringField(TEXT("seed"), Seed) && O->TryGetNumberField(TEXT("chance"), C) && C < BestChance) { BestChance = C; Best = Seed; }
				}
				if (!Best.IsEmpty()) Pool.AddUnique(FName(*Best));
			}
		}
	}
	return Pool.Num() > 0 ? Pool[FMath::RandRange(0, Pool.Num() - 1)] : NAME_None;
}

namespace WRForestHollowImpl
{
	AWRForestSecret* FindSecret(const UObject* Ctx, FName Kind)
	{
		const UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
		if (!W) return nullptr;
		for (TActorIterator<AWRForestSecret> It(const_cast<UWorld*>(W)); It; ++It) if (It->Kind == Kind) return *It;
		return nullptr;
	}
}

bool WRForestHollow::DigSpot(const UObject* WorldContext, FVector& OutAt, float& OutRadius)
{
	const AWRForestSecret* Plug = WRForestHollowImpl::FindSecret(WorldContext, TEXT("root_plug"));
	if (!Plug || Plug->State != 0) return false;
	OutAt = Plug->GetActorLocation();
	OutRadius = WRForestHollowImpl::Tn(TEXT("digRadius"), 400.f);
	return true;
}

bool WRForestHollow::IsDugOpen(const UObject* WorldContext)
{
	const AWRForestSecret* Plug = WRForestHollowImpl::FindSecret(WorldContext, TEXT("root_plug"));
	return Plug && Plug->State == 1;
}

float WRForestHollow::TrufflerDigging(AActor* Truffler, float DeltaSeconds)
{
	using namespace WRForestHollowImpl;
	AWRCreature* C = Cast<AWRCreature>(Truffler);
	if (!C || !C->HasAuthority() || !C->IsAlive() || !C->IsTamed()) return -1.f;
	if (C->CreatureId != FName(*Str(TEXT("hazards.hollow.digCreature"), TEXT("truffler")))) return -1.f;
	AWRForestSecret* Plug = FindSecret(C, TEXT("root_plug"));
	if (!Plug || Plug->State != 0) return -1.f;
	const FVector P = Plug->GetActorLocation();
	if (FVector::Dist2D(C->GetActorLocation(), P) > Tn(TEXT("digRadius"), 400.f) || FMath::Abs(C->GetActorLocation().Z - P.Z) > 500.f) return -1.f;
	const float Need = FMath::Max(1.f, Tn(TEXT("digSeconds"), 20.f));
	Plug->DigSeconds += DeltaSeconds;
	Plug->Burnt = FMath::Clamp(Plug->DigSeconds / Need, 0.f, 1.f);
	const float T = Now(C);
	if (T - Plug->LastDigAt > 1.2f)
	{
		// Soil flies from its snout every second or so: everyone near sees and hears the dig.
		Plug->LastDigAt = T;
		WRFx::Broadcast(C, TEXT("forest.hollow_dig"), C->GetActorLocation() + C->GetActorForwardVector() * 60.f, C->GetActorRotation(), 1.f, TEXT("forest.dig"));
		Plug->ForceNetUpdate();
	}
	if (Plug->DigSeconds >= Need)
	{
		Plug->State = 1;
		Plug->StateAt = T;
		Plug->ForceNetUpdate();
		WRFx::Broadcast(C, TEXT("forest.hollow_plug_fall"), P, FRotator::ZeroRotator, 1.f, TEXT("forest.dig"));
		UE_LOG(LogWildreach, Log, TEXT("FOREST hollow: a Truffler (%s) dug the root plug through at %s"), *C->GetTameOwner(), *P.ToCompactString());
		return 1.f;
	}
	return Plug->Burnt;
}

void WRForestHollow::FireAt(UWorld* World, const FVector& At, float Seconds)
{
	if (!World || World->GetNetMode() == NM_Client) return;
	AWRForestSecret* Door = WRForestHollowImpl::FindSecret(World, TEXT("barred_door"));
	if (!Door || Door->State != 0 || Door->Size <= 0.f) return;
	if (FVector::Dist(Door->GetActorLocation() + FVector(0, 0, 100.f), At) > WRForestHollowImpl::Ts(TEXT("doorFireReach"), 400.f)) return;
	const float Need = FMath::Max(0.5f, WRForestHollowImpl::Ts(TEXT("doorBurnSeconds"), 6.f));
	Door->TorchClock += Seconds;
	Door->Burnt = FMath::Clamp(Door->TorchClock / Need, 0.f, 1.f);
	Door->ForceNetUpdate();
}

// ================================================================================================ AWRHollowRoot

namespace WRForestHollowImpl
{
	UHierarchicalInstancedStaticMeshComponent* KitPart(AActor* A, FName Species, int32 V)
	{
		const FName Name(*FString::Printf(TEXT("Hollow_%s_%d"), *Species.ToString(), V));
		TArray<UHierarchicalInstancedStaticMeshComponent*> Have;
		A->GetComponents<UHierarchicalInstancedStaticMeshComponent>(Have);
		for (UHierarchicalInstancedStaticMeshComponent* H : Have) if (H->GetFName() == Name) return H;
		UStaticMesh* Mesh = WRTreeKit::Mesh(A, Species, V);
		if (!Mesh) return nullptr;
		UHierarchicalInstancedStaticMeshComponent* H = NewObject<UHierarchicalInstancedStaticMeshComponent>(A, Name);
		H->SetupAttachment(A->GetRootComponent());
		H->SetStaticMesh(Mesh);
		H->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		H->SetCanEverAffectNavigation(false);
		const int32 Cull = int32(Tn(TEXT("cull"), 20000.f));
		H->SetCullDistances(Cull - 2000, Cull);
		H->RegisterComponent();
		// Foxfire in the kit's glow strips (the mats and roots carry them where the fungus runs).
		H->SetMaterialByName(TEXT("Glow"), WRTreeKit::GlowMaterial(A, FLinearColor(FoxfireLight), Tn(TEXT("kitGlow"), 1.2f)));
		return H;
	}

	/** One roofed rect (frame-local [X0,X1] x [Y0,Y1]): its hidden walk slab with its top on the frame's plane, and the
	 *  woven root-mat tiles under it (the root_mat kit piece, at most 24 m a tile so the weave keeps its scale). */
	void RoofRect(AActor* A, TArray<TObjectPtr<UBoxComponent>>& Slabs, const FTransform& Frame, float X0, float X1, float Y0, float Y1, bool bDraw, int32 Seed)
	{
		if (X1 - X0 < 20.f || Y1 - Y0 < 20.f) return;
		const FVector C((X0 + X1) * 0.5f, (Y0 + Y1) * 0.5f, -30.f);
		Slabs.Add(Solid(A, FTransform(Frame.GetRotation(), Frame.TransformPosition(C)), FVector((X1 - X0) * 0.5f, (Y1 - Y0) * 0.5f, 30.f)));
		if (!bDraw) return;
		const int32 Nx = FMath::Max(1, FMath::CeilToInt((X1 - X0) / 2400.f)), Ny = FMath::Max(1, FMath::CeilToInt((Y1 - Y0) / 2400.f));
		const float Sx = (X1 - X0) / Nx, Sy = (Y1 - Y0) / Ny;
		const float Ref = FMath::Max(1.f, WRTreeKit::ReferenceHeight(TEXT("root_mat")));
		for (int32 i = 0; i < Nx; ++i)
		{
			for (int32 j = 0; j < Ny; ++j)
			{
				const int32 V = (Seed + i * 7 + j * 3) & 3;
				UHierarchicalInstancedStaticMeshComponent* H = KitPart(A, TEXT("root_mat"), V);
				if (!H) continue;
				// Sunk so its dome's crown sits at the forest floor and its edges dip into the ground; flattened a little.
				const FVector L(X0 + Sx * (i + 0.5f), Y0 + Sy * (j + 0.5f), -Tn(TEXT("matSink"), 110.f));
				const FTransform T(Frame.GetRotation(), Frame.TransformPosition(L), FVector((Sx + 260.f) / Ref, (Sy + 260.f) / Ref, Tn(TEXT("matFlatten"), 0.6f)));
				H->AddInstance(T, false);
			}
		}
	}

	/** Inside a room's rect (pad cm round it)? */
	bool InRoomRect(const FVector2D& P, float Pad)
	{
		for (const FRoomX& R : Data().Rooms)
		{
			const FVector2D L = R.Local(P);
			if (FMath::Abs(L.X) <= R.Hx + Pad && FMath::Abs(L.Y) <= R.Hy + Pad) return true;
		}
		return false;
	}

	/** A great root pillar from floor to roof (the Eldest's roots, "each wider than a house" in the Root Hall), flared at
	 *  both ends where it spreads into floor and ceiling, foxfire on its face. Collision: a capsule. */
	void Pillar(AActor* A, FWRToolMesh& M, FWRToolMesh& G, const FVector& Foot, float TopZ, float R, int32 Seed, bool bDraw)
	{
		const float H = FMath::Max(300.f, TopZ - Foot.Z + 80.f);
		UCapsuleComponent* Cap = NewObject<UCapsuleComponent>(A);
		Cap->SetupAttachment(A->GetRootComponent());
		Cap->SetUsingAbsoluteLocation(true);
		Cap->InitCapsuleSize(R * 0.92f, H * 0.5f);
		Cap->SetWorldLocation(Foot + FVector(0, 0, H * 0.5f - 40.f));
		Cap->SetCollisionProfileName(TEXT("BlockAll"));
		Cap->SetCanEverAffectNavigation(false);
		Cap->SetHiddenInGame(true);
		Cap->RegisterComponent();
		if (!bDraw) return;
		M.Lathe(Foot - FVector(0, 0, 60.f), FVector::UpVector, H + 60.f, { FVector(0.f, R * 1.7f, R * 1.5f), FVector(0.08f, R * 1.15f, R * 1.05f), FVector(0.5f, R, R * 0.92f), FVector(0.9f, R * 1.1f, R), FVector(1.f, R * 1.8f, R * 1.6f) },
			14, RootBark, 0.92f, R * 0.25f, float(Seed));
		// Lesser roots twisting round it, and splaying out across the floor.
		for (int32 k = 0; k < 4; ++k)
		{
			const float Ang = (k * 90.f + Seed * 37.f) * PI / 180.f;
			const FVector Dir(FMath::Cos(Ang), FMath::Sin(Ang), 0.f);
			Root(M, { Foot + Dir * R * 0.8f + FVector(0, 0, 160.f), Foot + Dir * (R * 1.6f) + FVector(0, 0, 30.f), Foot + Dir * (R * 2.6f + 150.f) - FVector(0, 0, 20.f) }, R * 0.22f, R * 0.06f, RootDark);
			Root(M, { Foot + Dir * R * 0.9f + FVector(0, 0, H * 0.25f), Foot + FVector(Dir.Y, -Dir.X, 0.f) * R * 1.05f + FVector(0, 0, H * 0.55f), Foot - Dir * R * 0.95f + FVector(0, 0, H * 0.85f) }, R * 0.12f, R * 0.08f, RootBark * 0.85f);
			Foxfire(G, Foot + Dir * R * 1.02f + FVector(0, 0, 60.f + 90.f * k), Dir, Seed * 10 + k);
		}
		Ball(M, Foot + FVector(R * 0.5f, R * 0.4f, 25.f), R * 0.5f, Moss * 0.7f, 0.98f, 0.25f);
	}

	/** A root arch across a passage (the kit's root_arch: both ends in the floor, its crown under the roof). */
	void Arch(AActor* A, const FVector& Mid, const FVector& Along, float Width, float Clear, int32 Seed)
	{
		UHierarchicalInstancedStaticMeshComponent* H = KitPart(A, TEXT("root_arch"), Seed & 3);
		if (!H) return;
		const FVector Across(-Along.Y, Along.X, 0.f);
		const float Span = Width + 150.f;
		const float Ref = FMath::Max(1.f, WRTreeKit::ReferenceHeight(TEXT("root_arch")));
		const float Up = FMath::Clamp((Clear - 120.f) / 500.f, 0.4f, 1.f);
		H->AddInstance(FTransform(Across.Rotation(), Mid - Across * Span * 0.5f, FVector(Span / Ref, 1.f, Up)), false);
	}
}

AWRHollowRoot::AWRHollowRoot()
{
	bReplicates = false;
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRHollowRoot::EndPlay(const EEndPlayReason::Type Reason)
{
	if (WaterVolume) WaterVolume->Destroy();
	if (Heartbeat) Heartbeat->Stop();
	Super::EndPlay(Reason);
}

void AWRHollowRoot::AddLight(const FVector& At, float Scale)
{
	if (GetNetMode() == NM_DedicatedServer) return;
	// Foxfire's 12% light (DC 2.5): a dim green glow every ~15 m, never shadowed; only the ones near you burn (Tick).
	UPointLightComponent* L = WRForestHollowImpl::Lamp(this, At, WRForestHollowImpl::FoxfireLight, WRForestHollowImpl::Tn(TEXT("lightIntensity"), 700.f) * Scale, WRForestHollowImpl::Tn(TEXT("lightRadius"), 1500.f) * FMath::Sqrt(Scale));
	L->SetVisibility(false);
	Lights.Add(L);
	LightAt.Add(At);
}

void AWRHollowRoot::Build()
{
	using namespace WRForestHollowImpl;
	if (bBuilt) return;
	const FData& D = Data();
	if (D.Rooms.Num() == 0) { bBuilt = true; return; }
	// The land has to be there to fit the roofs to it (WRLand builds at runtime; its heights come first, then traces).
	float Z = 0.f;
	if (!GroundZ(GetWorld(), D.Rooms[0].C + FVector2D(D.Rooms[0].Hx + 800.f, 0.f), D.Rooms[0].Roof, Z) && BuildTries > 0) return;
	bBuilt = true;
	const bool bDraw = GetNetMode() != NM_DedicatedServer;
	BuildRoofs(bDraw);
	for (int32 i = 0; i < D.Rooms.Num(); ++i) BuildRoom(i, bDraw);
	BuildPassages(bDraw);
	BuildWaysIn(bDraw);
	BuildGlows(bDraw);
	UE_LOG(LogWildreach, Log, TEXT("FOREST hollow root built: %d rooms, %d roof slabs, %d lights, %d glowworm curtains"), D.Rooms.Num(), Slabs.Num(), Lights.Num(), Curtains.Num());
}

void AWRHollowRoot::BuildRoofs(bool bDraw)
{
	using namespace WRForestHollowImpl;
	const FData& D = Data();
	Roofs.Init(0.f, D.Rooms.Num());
	FVector PlugAt; FVector2D PlugLocal; int32 PlugRoom = INDEX_NONE;
	const bool bPlug = PlugSpot(PlugAt, PlugLocal, PlugRoom);
	for (int32 i = 0; i < D.Rooms.Num(); ++i)
	{
		const FRoomX& R = D.Rooms[i];
		Roofs[i] = R.Roof;
		// A room at the forest floor (the Heart Root's mouth in the clearing) has nothing over it.
		if (R.Roof - R.Entry < Tn(TEXT("openBelow"), 450.f)) continue;
		const float HX = R.Hx + 300.f, HY = R.Hy + 300.f;
		float Cz = R.Roof;
		const FTransform Frame = RoofFrame(GetWorld(), R.C, R.Yaw, HX, HY, R.Entry, R.Roof, Cz);
		Roofs[i] = Cz;
		if (bPlug && i == PlugRoom && PlugOverRoom(R, PlugLocal, 300.f))
		{
			// The Truffler's soft spot: the roof parts round it (the plug fills the hole: AWRForestSecret root_plug).
			const float H = Tn(TEXT("digHole"), 170.f);
			RoofRect(this, Slabs, Frame, -HX, PlugLocal.X - H, -HY, HY, bDraw, i);
			RoofRect(this, Slabs, Frame, PlugLocal.X + H, HX, -HY, HY, bDraw, i + 1);
			RoofRect(this, Slabs, Frame, PlugLocal.X - H, PlugLocal.X + H, -HY, PlugLocal.Y - H, bDraw, i + 2);
			RoofRect(this, Slabs, Frame, PlugLocal.X - H, PlugLocal.X + H, PlugLocal.Y + H, HY, bDraw, i + 3);
		}
		else
		{
			RoofRect(this, Slabs, Frame, -HX, HX, -HY, HY, bDraw, i);
		}
	}
	// The Truffler's slide (way 3): from the shaft's rim to where the vault's roof begins, the trench the generator cut
	// for it is roofed like a passage (sloping from the forest floor at the shaft to the vault's roof), so the only way
	// in from above is down the shaft, through the plug.
	if (bPlug && D.Rooms.IsValidIndex(PlugRoom) && !PlugOverRoom(D.Rooms[PlugRoom], PlugLocal, 300.f))
	{
		const FRoomX& R = D.Rooms[PlugRoom];
		const FSpotX* S = D.Spot({ TEXT("truffler_dig"), TEXT("hollow_dig") });
		const float Top = S ? S->At.Z : PlugAt.Z + 250.f;
		const FVector2D From(PlugAt), To = R.C;
		const FVector2D Dir = (To - From).GetSafeNormal();
		const float Full = FVector2D::Distance(From, To);
		// Where the line enters the vault's roofed rect (its rect + 3 m).
		float Enter = Full;
		for (float T = 0.f; T < Full; T += 50.f)
		{
			if (PlugOverRoom(R, R.Local(From + Dir * T), 300.f)) { Enter = T; break; }
		}
		const float Start = Tn(TEXT("digHole"), 170.f) + 80.f;
		if (Enter - Start > 100.f)
		{
			const float ZA = Top, ZB = Roofs.IsValidIndex(PlugRoom) ? Roofs[PlugRoom] : R.Roof;
			const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(ZB - ZA, Enter - Start));
			const FVector A(From + Dir * Start, ZA);
			const FTransform SlideFrame(FRotator(Pitch, FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)), 0.f), A);
			const float Len = FMath::Sqrt(FMath::Square(Enter - Start) + FMath::Square(ZB - ZA)) + 150.f;
			RoofRect(this, Slabs, SlideFrame, 0.f, Len, -Tn(TEXT("slideHalfWidth"), 320.f), Tn(TEXT("slideHalfWidth"), 320.f), bDraw, 977);
		}
	}
}

void AWRHollowRoot::BuildPassages(bool bDraw)
{
	using namespace WRForestHollowImpl;
	const FData& D = Data();
	FWRToolMesh M, G;
	int32 Piece = 0;
	for (const FWay& W : D.Ways)
	{
		if (W.Kind != TEXT("hollow")) continue;
		float SinceLight = 0.f;
		for (int32 s = 0; s + 1 < W.Pts.Num(); ++s)
		{
			const FVector A = W.Pts[s], B = W.Pts[s + 1];
			const float Len = FVector::Dist2D(A, B);
			if (Len < 50.f) continue;
			const FVector Along = (B - A).GetSafeNormal2D();
			const FVector Across(-Along.Y, Along.X, 0.f);
			const int32 N = FMath::Max(1, FMath::CeilToInt(Len / 2000.f));
			for (int32 k = 0; k < N; ++k, ++Piece)
			{
				const FVector P0 = FMath::Lerp(A, B, float(k) / N), P1 = FMath::Lerp(A, B, float(k + 1) / N);
				const FVector Mid = (P0 + P1) * 0.5f;
				const float PieceLen = FVector::Dist2D(P0, P1);
				SinceLight += PieceLen;
				if (InRoomRect(FVector2D(Mid), 150.f)) continue;   // the room's own roof covers it
				float Cz = Mid.Z + 1000.f;
				const FTransform Frame = RoofFrame(GetWorld(), FVector2D(Mid), Along.Rotation().Yaw, PieceLen * 0.5f + 150.f, W.Width * 0.5f + 300.f, Mid.Z, Mid.Z + 1000.f, Cz);
				const float Clear = Cz - Mid.Z;
				// A stretch within ~4.5 m of the forest floor is an open cut (the Eldest's Door's ramp, the Heart Root's
				// mouth): the ways in stay open to the sky; everything deeper is roofed.
				if (Clear < Tn(TEXT("openBelow"), 450.f)) continue;
				RoofRect(this, Slabs, Frame, -PieceLen * 0.5f - 150.f, PieceLen * 0.5f + 150.f, -W.Width * 0.5f - 300.f, W.Width * 0.5f + 300.f, bDraw, Piece);
				if (!bDraw) continue;
				const FVector Floor0 = Floor(GetWorld(), FVector2D(Mid), Mid.Z);
				Arch(this, Floor0, Along, W.Width, Clear, Piece);
				// Roots down the passage walls, foxfire on them; roots dropping from the roof.
				for (const float S : { -1.f, 1.f })
				{
					const FVector Wall = Floor0 + Across * S * (W.Width * 0.5f - 40.f);
					Root(M, { Wall - Along * PieceLen * 0.4f + FVector(0, 0, Clear - 120.f), Wall + FVector(0, 0, Clear * 0.5f), Wall + Along * PieceLen * 0.4f + FVector(0, 0, 20.f) }, 32.f, 12.f, RootBark);
					Foxfire(G, Wall + Along * (PieceLen * 0.2f * S) + FVector(0, 0, 90.f), -Across * S, Piece * 7 + int32(S));
				}
				for (int32 r = 0; r < 4; ++r)
				{
					const FVector Top = Floor0 + Along * (Jit(Mid, r) * PieceLen * 0.45f) + Across * (Jit(Mid, r + 9) * W.Width * 0.4f) + FVector(0, 0, Clear - 160.f);
					Root(M, { Top, Top + FVector(Jit(Top, 1) * 30.f, Jit(Top, 2) * 30.f, -80.f - 120.f * (Jit(Top, 3) * 0.5f + 0.5f)) }, 6.f, 1.5f, RootDark);
				}
				if (SinceLight >= Tn(TEXT("lightEvery"), 1500.f))
				{
					SinceLight = 0.f;
					AddLight(Floor0 + FVector(0, 0, 280.f), 1.f);
				}
			}
		}
	}
	if (bDraw)
	{
		Meshes.Add(Draw(this, M, TEXT("PassageRoots")));
		const FLinearColor Fox(FoxfireLight);
		Meshes.Add(Draw(this, G, TEXT("PassageFoxfire"), &Fox, Tn(TEXT("foxfireGlow"), 2.5f)));
	}
}

namespace WRForestHollowImpl
{
	/** A glowworm curtain hung from Top (its own component, so it sways). */
	USceneComponent* MakeCurtain(AActor* A, const FVector& Top, float Radius, int32 Seed)
	{
		USceneComponent* S = NewObject<USceneComponent>(A);
		S->SetupAttachment(A->GetRootComponent());
		S->SetUsingAbsoluteLocation(true);
		S->SetWorldLocation(Top);
		S->RegisterComponent();
		FWRToolMesh G;
		Curtain(G, Radius, Seed);
		UProceduralMeshComponent* C = NewObject<UProceduralMeshComponent>(A);
		C->SetupAttachment(S);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCanEverAffectNavigation(false);
		C->RegisterComponent();
		G.ApplyTo(C);
		C->SetCastShadow(false);
		if (UMaterialInstanceDynamic* Mi = WRTreeKit::GlowMaterial(A, FLinearColor(GlowwormLight), Tn(TEXT("glowwormGlow"), 3.f))) C->SetMaterial(0, Mi);
		return S;
	}

	/** A bone with knobbed ends (the giant's skeleton, the den's leavings). */
	void BoneSeg(FWRToolMesh& M, const FVector& A, const FVector& B, float R, const FLinearColor& Col)
	{
		const FVector D = B - A;
		const float L = D.Size();
		if (L < 1.f) return;
		M.Lathe(A, D / L, L, { FVector(0.f, R * 1.7f, R * 1.4f), FVector(0.1f, R, R * 0.9f), FVector(0.5f, R * 0.8f, R * 0.75f), FVector(0.9f, R, R * 0.9f), FVector(1.f, R * 1.6f, R * 1.3f) }, 7, Col, 0.85f);
	}

	/** A sealed Grovekin seed jar (fired clay, a wooden lid under a wax seal). */
	void Jar(FWRToolMesh& M, const FVector& Foot, float H, int32 Seed)
	{
		const float R = H * 0.36f;
		M.Lathe(Foot, FVector::UpVector, H, { FVector(0.f, R * 0.6f, R * 0.6f), FVector(0.35f, R, R), FVector(0.7f, R * 0.85f, R * 0.85f), FVector(0.88f, R * 0.45f, R * 0.45f), FVector(1.f, R * 0.5f, R * 0.5f) }, 10, Clay * (0.85f + 0.15f * Jit(Foot, Seed)), 0.8f);
		M.Lathe(Foot + FVector(0, 0, H), FVector::UpVector, 5.f, { FVector(0.f, R * 0.55f, R * 0.55f), FVector(1.f, R * 0.5f, R * 0.5f) }, 10, RootBark, 0.85f);
		Ball(M, Foot + FVector(0, 0, H + 6.f), R * 0.28f, Rgb(120, 40, 26), 0.5f, 0.5f);
	}

	/** A rock lump (rubble, a basin's stone). */
	void Lump(FWRToolMesh& M, const FVector& C, float R, const FLinearColor& Col, int32 Seed)
	{
		M.Lathe(C - FVector(0, 0, R * 0.7f), FVector(Jit(C, Seed) * 0.3f, Jit(C, Seed + 1) * 0.3f, 1.f).GetSafeNormal(), R * 1.4f,
			{ FVector(0.f, R * 0.7f, R * 0.6f), FVector(0.4f, R, R * 0.85f), FVector(0.8f, R * 0.75f, R * 0.7f), FVector(1.f, R * 0.2f, R * 0.2f) }, 5, Col, 0.9f, R * 0.15f, float(Seed));
	}
}

void AWRHollowRoot::BuildRoom(int32 Index, bool bDraw)
{
	using namespace WRForestHollowImpl;
	const FData& D = Data();
	if (!D.Rooms.IsValidIndex(Index)) return;
	const FRoomX& R = D.Rooms[Index];
	UWorld* W = GetWorld();
	const float RoofZ = Roofs.IsValidIndex(Index) ? Roofs[Index] : R.Roof;
	const float Ceil = RoofZ - Tn(TEXT("matUnder"), 180.f);   // the mat's underside
	const bool bOpen = R.Roof - R.Entry < Tn(TEXT("openBelow"), 450.f);
	const FRotator Yaw(0.f, R.Yaw, 0.f);
	const FVector Ax = Yaw.Vector(), Ay = FRotator(0.f, R.Yaw + 90.f, 0.f).Vector();
	const auto At = [&](float LX, float LY, float Lift) { return Floor(W, R.World(FVector2D(LX, LY)), R.Floor) + FVector(0, 0, Lift); };
	FWRToolMesh M, G, Amb;   // M bark/bone/clay, G foxfire green, Amb amber sap
	FRandomStream Rs(int32(GetTypeHash(R.Id)) & 0x7fffffff);

	if (!bOpen)
	{
		// Every room: foxfire along the walls and roots hanging from the mat, a light every ~15 m, glowworms under the roof.
		for (int32 k = 0; k < 10; ++k)
		{
			const float A = 2.f * PI * k / 10.f + Rs.FRandRange(-0.2f, 0.2f);
			const float LX = FMath::Cos(A) * R.Hx * 0.95f, LY = FMath::Sin(A) * R.Hy * 0.95f;
			const FVector P = At(LX, LY, Rs.FRandRange(60.f, 260.f));
			Foxfire(G, P, -(Ax * FMath::Cos(A) + Ay * FMath::Sin(A)), Index * 50 + k);
		}
		for (int32 k = 0; k < 14; ++k)
		{
			const FVector Top(R.World(FVector2D(Rs.FRandRange(-R.Hx, R.Hx) * 0.9f, Rs.FRandRange(-R.Hy, R.Hy) * 0.9f)), Ceil + 40.f);
			Root(M, { Top, Top + FVector(Rs.FRandRange(-40.f, 40.f), Rs.FRandRange(-40.f, 40.f), -Rs.FRandRange(80.f, 260.f)) }, 7.f, 1.5f, RootDark);
		}
		for (float LX = -R.Hx + 750.f; LX <= R.Hx - 400.f; LX += Tn(TEXT("lightEvery"), 1500.f))
			for (float LY = -R.Hy + 750.f; LY <= R.Hy - 400.f; LY += Tn(TEXT("lightEvery"), 1500.f)) AddLight(At(LX, LY, 300.f), 1.f);
		if (bDraw)
		{
			for (int32 c = 0; c < 2; ++c)
			{
				const FVector Top(R.World(FVector2D(Rs.FRandRange(-0.6f, 0.6f) * R.Hx, Rs.FRandRange(-0.6f, 0.6f) * R.Hy)), Ceil);
				Curtains.Add(MakeCurtain(this, Top, Tn(TEXT("curtainRadius"), 140.f), Index * 10 + c));
				CurtainPhase.Add(Rs.FRandRange(0.f, 6.f));
			}
		}
	}

	if (R.Id == TEXT("root_hall"))
	{
		// DC 2.5: a hall 40 m long whose pillars are the Eldest's roots, each wider than a house. Two rows of three.
		const float PR = Tn(TEXT("hallPillarRadius"), 280.f);
		for (int32 c = 0; c < 3; ++c)
			for (const float S : { -1.f, 1.f })
				Pillar(this, M, G, At((c - 1) * R.Hx * 0.6f, S * R.Hy * 0.5f, 0.f), Ceil, PR * Rs.FRandRange(0.85f, 1.15f), c * 2 + int32(S > 0.f), bDraw);
		// The hall's floor is root over root: the roots between the pillars, half sunk.
		for (int32 k = 0; k < 6; ++k)
			Root(M, { At(-R.Hx * 0.6f + k * 200.f, -R.Hy * 0.5f, -20.f), At(Rs.FRandRange(-R.Hx, R.Hx) * 0.5f, 0.f, 30.f), At((k - 2) * R.Hx * 0.3f, R.Hy * 0.5f, -20.f) }, 60.f, 30.f, RootBark * 0.9f);
	}
	else if (R.Id == TEXT("cradle_vault"))
	{
		// DC 2.5: the Grovekin dead, wrapped in bark and laid in root cradles along both long walls, three tiers high; the
		// roots have grown round them like hands. The oldest (low and at the far end) are nearly roots themselves.
		for (const float S : { -1.f, 1.f })
		{
			for (int32 Tier = 0; Tier < 3; ++Tier)
			{
				const float Z = 40.f + 150.f * Tier;
				// The root shelf the tier lies on.
				Root(M, { At(-R.Hx + 80.f, S * (R.Hy - 60.f), Z - 30.f), At(0.f, S * (R.Hy - 40.f), Z - 40.f), At(R.Hx - 80.f, S * (R.Hy - 60.f), Z - 30.f) }, 34.f, 26.f, RootBark);
				int32 n = 0;
				for (float LX = -R.Hx + 160.f; LX < R.Hx - 120.f; LX += 175.f, ++n)
				{
					const FVector Mid = At(LX, S * (R.Hy - 110.f), Z + 20.f);
					const float Old = FMath::Clamp((LX + R.Hx) / (2.f * R.Hx) + Tier * -0.2f + Rs.FRandRange(-0.15f, 0.15f), 0.f, 1.f);
					const FLinearColor Wrap = FMath::Lerp(Rgb(122, 92, 62), RootBark * 0.8f, Old);
					M.Lathe(Mid - Ax * 85.f, Ax, 170.f, { FVector(0.f, 10.f, 9.f), FVector(0.12f, 24.f, 20.f), FVector(0.5f, 27.f, 22.f), FVector(0.85f, 22.f, 19.f), FVector(1.f, 8.f, 8.f) }, 8, Wrap, 0.9f);
					for (int32 b = 1; b < 5; ++b) M.Lathe(Mid - Ax * 85.f + Ax * (34.f * b) - Ax * 2.f, Ax, 4.f, { FVector(0.f, 26.f, 22.f), FVector(1.f, 26.f, 22.f) }, 8, Rgb(70, 60, 44), 0.9f);
					// The hand: three root fingers from the shelf curling up round the bundle.
					for (int32 f = 0; f < 3; ++f)
					{
						const FVector Base = Mid + Ax * ((f - 1) * 55.f) - FVector(0, 0, 40.f) + Ay * S * 30.f;
						Root(M, { Base, Base - Ay * S * 45.f - FVector(0, 0, 5.f), Base - Ay * S * 55.f + FVector(0, 0, 35.f), Base - Ay * S * 20.f + FVector(0, 0, 58.f) }, 9.f, 3.f, RootBark * (1.f - 0.25f * Old));
					}
					if (Old > 0.6f) Root(M, { Mid - Ax * 80.f + FVector(0, 0, 20.f), Mid + FVector(0, 0, 30.f), Mid + Ax * 80.f + FVector(0, 0, 18.f) }, 12.f, 6.f, RootDark);
				}
			}
		}
	}
	else if (R.Id == TEXT("grove_of_bones"))
	{
		// DC 2.5, 2.20: a Grovekin giant's skeleton the roots pushed up and grew through; a heartwood spear still in its hand.
		const FVector O = At(0.f, 0.f, 0.f);
		const auto P = [&](float X, float Y, float Z) { return O + Ax * X + Ay * Y + FVector(0, 0, Z); };
		Ball(M, P(-470.f, 0.f, 60.f), 70.f, BoneOld, 0.85f, 0.85f);                   // the skull, on its side
		M.Lathe(P(-430.f, 40.f, 20.f), (Ax + Ay * 0.4f).GetSafeNormal(), 90.f, { FVector(0.f, 30.f, 20.f), FVector(1.f, 20.f, 14.f) }, 7, BoneOld, 0.85f);   // the jaw
		for (float X = -390.f; X <= 230.f; X += 32.f) Ball(M, P(X, 0.f, 28.f), 15.f, BoneOld, 0.85f, 0.8f);   // the spine
		for (int32 r = 0; r < 9; ++r)
		{
			const float X = -340.f + 28.f * r;
			for (const float S : { -1.f, 1.f })
				M.Tube({ P(X, 0.f, 30.f), P(X + 6.f, S * 80.f, 110.f), P(X + 10.f, S * 140.f, 90.f), P(X + 12.f, S * 155.f, 25.f) }, 6.5f, 6, Bone * 0.9f, 0.85f);
		}
		BoneSeg(M, P(230.f, -60.f, 30.f), P(230.f, 60.f, 30.f), 30.f, BoneOld);         // the pelvis
		for (const float S : { -1.f, 1.f })
		{
			BoneSeg(M, P(250.f, S * 50.f, 30.f), P(560.f, S * 90.f, 25.f), 16.f, Bone * 0.9f);
			BoneSeg(M, P(560.f, S * 90.f, 25.f), P(840.f, S * 70.f, 20.f), 13.f, Bone * 0.9f);
		}
		// The arm: shoulder, elbow, the hand round the spear.
		BoneSeg(M, P(-340.f, 120.f, 40.f), P(-150.f, 260.f, 30.f), 15.f, Bone * 0.9f);
		BoneSeg(M, P(-150.f, 260.f, 30.f), P(30.f, 310.f, 35.f), 11.f, Bone * 0.9f);
		for (int32 f = 0; f < 4; ++f) M.Tube({ P(40.f, 300.f + f * 9.f, 30.f), P(60.f, 300.f + f * 9.f, 60.f), P(48.f, 300.f + f * 9.f, 75.f) }, 3.5f, 5, Bone * 0.9f, 0.85f);
		// The spear: a heartwood shaft (amber-cored, dark grain) with a leaf blade of the same wood, fire-hardened.
		const FVector S0 = P(-240.f, 330.f, 26.f), S1 = P(320.f, 290.f, 66.f);
		M.Lathe(S0, (S1 - S0).GetSafeNormal(), FVector::Dist(S0, S1), { FVector(0.f, 4.f, 4.f), FVector(0.95f, 4.5f, 4.5f), FVector(1.f, 3.f, 3.f) }, 7, Rgb(150, 88, 34), 0.6f);
		M.Lathe(S1, (S1 - S0).GetSafeNormal(), 70.f, { FVector(0.f, 3.f, 2.f), FVector(0.3f, 11.f, 2.2f), FVector(1.f, 0.5f, 0.5f) }, 6, Rgb(96, 52, 22), 0.5f);
		// The roots that pushed it up: through the ribcage, the eye, the pelvis, up into the roof.
		for (const FVector& Through : { P(-280.f, 30.f, 50.f), P(-200.f, -40.f, 60.f), P(-470.f, 20.f, 70.f), P(230.f, 0.f, 40.f), P(-120.f, 0.f, 50.f) })
			Root(M, { Through - FVector(0, 0, 120.f), Through, Through + FVector(Rs.FRandRange(-80.f, 80.f), Rs.FRandRange(-80.f, 80.f), (Ceil - Through.Z) * 0.6f), FVector(Through.X, Through.Y, Ceil + 60.f) }, 28.f, 16.f, RootBark);
	}
	else if (R.Id == TEXT("weeping_roots"))
	{
		// DC 2.5: roots dripping amber sap into ten stone basins round the room (the resin nodes are the server's).
		for (int32 b = 0; b < 10; ++b)
		{
			const float A = 2.f * PI * b / 10.f;
			const FVector Foot = At(FMath::Cos(A) * R.Hx * 0.68f, FMath::Sin(A) * R.Hy * 0.68f, -5.f);
			M.Lathe(Foot, FVector::UpVector, 45.f, { FVector(0.f, 40.f, 40.f), FVector(0.6f, 58.f, 58.f), FVector(0.92f, 62.f, 62.f), FVector(1.f, 52.f, 52.f) }, 10, Stone, 0.85f);
			Amb.Lathe(Foot + FVector(0, 0, 38.f), FVector::UpVector, 4.f, { FVector(0.f, 50.f, 50.f), FVector(1.f, 49.f, 49.f) }, 12, FLinearColor(1.f, 0.62f, 0.15f), 0.2f);
			const FVector Drip = Foot + FVector(0, 0, 200.f);
			Root(M, { FVector(Foot.X, Foot.Y, Ceil + 40.f) + FVector(60.f, 0, 0), Drip + FVector(30.f, 10.f, 60.f), Drip }, 26.f, 10.f, RootBark);
			Amb.Tube({ Drip, Drip - FVector(0, 0, 70.f), Drip - FVector(0, 0, 150.f) }, 1.4f, 4, FLinearColor(1.f, 0.6f, 0.12f), 0.2f);
			Ball(Amb, Drip - FVector(0, 0, 60.f), 3.f, FLinearColor(1.f, 0.65f, 0.2f), 0.2f);
		}
	}
	else if (R.Id == TEXT("mossmaul_den"))
	{
		// DC 2.5: a hibernation hollow: a moss-and-fern bed in the middle, the bones of what it ate round it.
		const FVector C = At(0.f, 0.f, 0.f);
		Ball(M, C + FVector(0, 0, 20.f), 330.f, Moss * 0.6f, 0.98f, 0.12f);
		Ball(M, C + FVector(60.f, -40.f, 45.f), 200.f, Rgb(70, 92, 44), 0.98f, 0.12f);
		for (int32 b = 0; b < 34; ++b)
		{
			const float A = Rs.FRandRange(0.f, 2.f * PI), D2 = Rs.FRandRange(380.f, FMath::Min(R.Hx, R.Hy) * 0.9f);
			const FVector P0 = C + (Ax * FMath::Cos(A) + Ay * FMath::Sin(A)) * D2 + FVector(0, 0, 8.f);
			const FVector Dir = FRotator(0.f, Rs.FRandRange(0.f, 360.f), 0.f).Vector();
			BoneSeg(M, P0, P0 + Dir * Rs.FRandRange(25.f, 70.f), Rs.FRandRange(2.5f, 5.f), b % 3 ? Bone * 0.85f : BoneOld);
			if (b % 9 == 0) Ball(M, P0 + FVector(0, 0, 10.f), 14.f, BoneOld, 0.85f, 0.8f);   // a deer's skull
		}
		// The claw-scored root it sharpens on.
		Root(M, { At(-R.Hx * 0.8f, R.Hy * 0.6f, 0.f), At(-R.Hx * 0.5f, R.Hy * 0.7f, 140.f), At(-R.Hx * 0.3f, R.Hy * 0.85f, 300.f) }, 45.f, 25.f, RootBark * 1.15f);
	}
	else if (R.Id == TEXT("bootlace_gallery"))
	{
		// DC 2.5: black Armillaria rhizomorphs as thick as ropes hanging from the roof, glowing green at the tips.
		for (int32 k = 0; k < 56; ++k)
		{
			const FVector Top(R.World(FVector2D(Rs.FRandRange(-R.Hx, R.Hx) * 0.92f, Rs.FRandRange(-R.Hy, R.Hy) * 0.92f)), Ceil + 30.f);
			const float Len = Rs.FRandRange(150.f, FMath::Max(200.f, Ceil - R.Floor - 120.f));
			const FVector Sw(Rs.FRandRange(-40.f, 40.f), Rs.FRandRange(-40.f, 40.f), 0.f);
			const FVector Tip = Top + Sw - FVector(0, 0, Len);
			M.Tube({ Top, Top + Sw * 0.3f - FVector(0, 0, Len * 0.35f), Top + Sw * 0.8f - FVector(0, 0, Len * 0.7f), Tip }, Rs.FRandRange(1.8f, 3.2f), 5, Black, 0.7f);
			if (k % 3 == 0) M.Tube({ Top + Sw * 0.3f - FVector(0, 0, Len * 0.35f), Top + Sw * 0.3f + FVector(30.f, -20.f, -Len * 0.55f) }, 1.4f, 4, Black, 0.7f);
			Ball(G, Tip, 3.2f, FLinearColor(0.55f, 1.f, 0.45f), 0.5f, 1.6f);
		}
	}
	else if (R.Id == TEXT("thrumming_root"))
	{
		// DC 2.5: one root, 6 m thick, lying across the room and pulsing slowly like a heartbeat (Sylvaroth's). Walk round
		// its ends; its face toward the way in is where you lay a hand (the secret).
		const float RR = Tn(TEXT("thrumRadius"), 300.f);
		const FVector C = At(0.f, 0.f, RR * 0.55f);
		PulseAt = C + FVector(0, 0, RR);
		const float Half = R.Hx * 0.7f;
		UCapsuleComponent* Cap = NewObject<UCapsuleComponent>(this);
		Cap->SetupAttachment(RootComponent);
		Cap->SetUsingAbsoluteLocation(true);
		Cap->SetUsingAbsoluteRotation(true);
		Cap->InitCapsuleSize(RR * 0.95f, Half + RR);
		Cap->SetWorldLocationAndRotation(C, FRotator(90.f, R.Yaw, 0.f));
		Cap->SetCollisionProfileName(TEXT("BlockAll"));
		Cap->SetCanEverAffectNavigation(false);
		Cap->SetHiddenInGame(true);
		Cap->RegisterComponent();
		if (bDraw)
		{
			PulseRoot = NewObject<USceneComponent>(this);
			PulseRoot->SetupAttachment(RootComponent);
			PulseRoot->SetUsingAbsoluteLocation(true);
			PulseRoot->SetUsingAbsoluteRotation(true);
			PulseRoot->SetWorldLocationAndRotation(C, Yaw);
			PulseRoot->RegisterComponent();
			FWRToolMesh RM, RG;
			RM.Lathe(FVector(-Half - RR, 0.f, 0.f), FVector::ForwardVector, 2.f * (Half + RR), { FVector(0.f, RR * 0.5f, RR * 0.5f), FVector(0.1f, RR * 0.95f, RR * 0.9f), FVector(0.5f, RR, RR * 0.95f), FVector(0.9f, RR * 0.95f, RR * 0.9f), FVector(1.f, RR * 0.6f, RR * 0.55f) }, 18, RootBark, 0.9f, RR * 0.2f, 7.f);
			for (int32 v = 0; v < 7; ++v)
			{
				// The sap veins along it, amber, the light that beats.
				TArray<FVector> Vein;
				for (int32 s = 0; s <= 16; ++s)
				{
					const float X = -Half + 2.f * Half * s / 16.f;
					const float A = v * 0.9f + s * 0.25f;
					Vein.Add(FVector(X, FMath::Cos(A) * RR * 1.01f, FMath::Sin(A) * RR * 0.97f));
				}
				RG.Tube(Vein, 5.f, 4, FLinearColor(1.f, 0.6f, 0.15f), 0.3f);
			}
			for (UProceduralMeshComponent* PM : { NewObject<UProceduralMeshComponent>(this), NewObject<UProceduralMeshComponent>(this) })
			{
				PM->SetupAttachment(PulseRoot);
				PM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				PM->RegisterComponent();
				Meshes.Add(PM);
			}
			RM.ApplyTo(Meshes[Meshes.Num() - 2]);
			RG.ApplyTo(Meshes.Last());
			Meshes.Last()->SetCastShadow(false);
			if (UMaterialInstanceDynamic* Mi = WRTreeKit::GlowMaterial(this, FLinearColor(1.f, 0.6f, 0.15f), Tn(TEXT("thrumGlow"), 3.f))) Meshes.Last()->SetMaterial(0, Mi);
			PulseLight = Lamp(this, PulseAt + FVector(0, 0, 80.f), FColor(255, 170, 70), Tn(TEXT("thrumLight"), 1200.f), 1600.f);
			Heartbeat = WRSound::StartLoop(this, TEXT("forest.root_pulse"));
			if (Heartbeat) Heartbeat->SetWorldLocation(C);
		}
	}
	else if (R.Id == TEXT("drowned_root"))
	{
		// DC 2.5: a flooded chamber where the Alder sinks underground: 15 m of black water to swim, held by root dams at
		// its two passages (the way on runs lower than the water: the roots dammed it).
		const float Surface = D.PoolZ > 0.f ? D.PoolZ : R.Floor + FMath::Max(250.f, R.Water);
		AWRForestWaterVolume* Vol = W->SpawnActor<AWRForestWaterVolume>(AWRForestWaterVolume::StaticClass(), FTransform::Identity);
		if (Vol)
		{
			AWRForestWaterVolume::FBox2 Box;
			Box.Half = FVector(R.Hx, R.Hy, (Surface - R.Floor + 200.f) * 0.5f);
			Box.Centre = FVector(R.C, Surface - Box.Half.Z);
			Box.Yaw = R.Yaw;
			Vol->Build({ Box });
			WaterVolume = Vol;
		}
		// The dams where the chain's passages meet the room's edge.
		const int32 Me = Index;
		for (const int32 Other : { Me - 1, Me + 1 })
		{
			if (!D.Rooms.IsValidIndex(Other)) continue;
			const FVector2D Dir = (D.Rooms[Other].C - R.C).GetSafeNormal();
			const FVector2D L = R.Local(R.C + Dir * 100.f) / 100.f;
			const float T = FMath::Min(FMath::Abs(L.X) > 1e-3f ? R.Hx / FMath::Abs(L.X) : 1e9f, FMath::Abs(L.Y) > 1e-3f ? R.Hy / FMath::Abs(L.Y) : 1e9f);
			const FVector2D Edge = R.C + Dir * T;
			const FVector Out(Dir, 0.f);
			const FVector Across(-Dir.Y, Dir.X, 0.f);
			const float Wd = 1000.f;
			const FVector DamTop(Edge, Surface + 15.f);
			Solid(this, FTransform(Out.Rotation(), DamTop - FVector(0, 0, 160.f)), FVector(100.f, Wd * 0.5f, 160.f));
			// The dry side: a root ramp down to the passage floor (it runs lower than the water).
			const FVector Foot = Floor(W, Edge + Dir * 700.f, Surface - 200.f);
			SolidSeg(this, DamTop + Out * 100.f - FVector(0, 0, 20.f), Foot + FVector(0, 0, -10.f), Wd * 0.45f, 20.f);
			for (int32 r = 0; r < 6; ++r)
			{
				const float Z = Surface + 10.f - r * 55.f;
				Root(M, { FVector(Edge, Z) - Across * Wd * 0.55f, FVector(Edge, Z + 8.f) + Out * 20.f, FVector(Edge, Z) + Across * Wd * 0.55f }, 30.f, 26.f, r % 2 ? RootBark : RootDark);
			}
			Root(M, { DamTop + Out * 80.f, Foot + FVector(0, 0, 10.f) }, 40.f, 30.f, RootBark);
		}
		if (bDraw)
		{
			// The water: a still black-green sheet over the room (the forest's water material, its colours turned down).
			TArray<FVector> V; TArray<int32> Tr = { 0, 2, 1, 1, 2, 3, 0, 1, 2, 1, 3, 2 }; TArray<FVector> N; TArray<FVector2D> UV; TArray<FColor> Col;
			for (int32 j = 0; j < 2; ++j) for (int32 i = 0; i < 2; ++i)
			{
				V.Add(FVector(R.World(FVector2D((i ? 1.f : -1.f) * (R.Hx + 200.f), (j ? 1.f : -1.f) * (R.Hy + 200.f))), Surface));
				N.Add(FVector::UpVector); UV.Add(FVector2D(i * R.Hx / 400.f, j * R.Hy / 400.f)); Col.Add(FColor::White);
			}
			UProceduralMeshComponent* Sheet = NewMesh(this, TEXT("DrownedWater"));
			Sheet->CreateMeshSection(0, V, Tr, N, UV, Col, TArray<FProcMeshTangent>(), false);
			UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, *Str(TEXT("hazards.water.material"), TEXT("/Game/WaterMaterials/Materials/M_River.M_River")));
			if (!Base) Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/WaterMaterials/Materials/M_Lake.M_Lake"));
			if (UMaterialInstanceDynamic* Mi = Base ? UMaterialInstanceDynamic::Create(Base, this) : nullptr)
			{
				Mi->SetVectorParameterValue(TEXT("Colour"), FLinearColor(0.012f, 0.03f, 0.022f));
				Mi->SetVectorParameterValue(TEXT("ColourDeep"), FLinearColor(0.004f, 0.01f, 0.008f));
				Mi->SetScalarParameterValue(TEXT("River_Speed"), 0.05f);
				Sheet->SetMaterial(0, Mi);
			}
			Sheet->SetCastShadow(false);
			Meshes.Add(Sheet);
			// Drowned roots breaking the surface.
			for (int32 k = 0; k < 8; ++k)
			{
				const FVector P0 = At(Rs.FRandRange(-R.Hx, R.Hx) * 0.8f, Rs.FRandRange(-R.Hy, R.Hy) * 0.8f, 0.f);
				Root(M, { P0, FVector(P0.X, P0.Y, Surface + 60.f) + FVector(Rs.FRandRange(-90.f, 90.f), Rs.FRandRange(-90.f, 90.f), 0.f), FVector(P0.X, P0.Y, Ceil + 50.f) }, 30.f, 18.f, RootDark);
			}
		}
	}
	else if (R.Id == TEXT("seed_store"))
	{
		// DC 2.5: a Grovekin granary in a dry root pocket: sealed clay jars on root shelves round three walls, a few broken.
		for (int32 Wall = 0; Wall < 3; ++Wall)
		{
			for (int32 Tier = 0; Tier < 2; ++Tier)
			{
				const float Z = 10.f + 110.f * Tier;
				for (int32 j = 0; j < 7; ++j)
				{
					const float T = -0.8f + 1.6f * j / 6.f;
					const FVector2D L = Wall == 0 ? FVector2D(T * R.Hx, R.Hy - 70.f) : Wall == 1 ? FVector2D(T * R.Hx, -R.Hy + 70.f) : FVector2D(R.Hx - 70.f, T * R.Hy);
					const FVector Foot = At(L.X, L.Y, Z);
					if (Tier > 0 || j == 0) Root(M, { Foot - FVector(0, 0, 14.f) - (Wall == 2 ? Ay : Ax) * 90.f, Foot - FVector(0, 0, 18.f) + (Wall == 2 ? Ay : Ax) * 90.f }, 16.f, 16.f, RootBark);
					if ((j + Tier + Wall) % 5 == 4)
					{
						for (int32 s = 0; s < 5; ++s) Lump(M, Foot + FVector(Rs.FRandRange(-30.f, 30.f), Rs.FRandRange(-30.f, 30.f), 2.f), 9.f, Clay, s);
						continue;
					}
					Jar(M, Foot, Rs.FRandRange(55.f, 80.f), Index * 100 + j);
				}
			}
		}
	}
	else if (R.Id == TEXT("wasp_grave"))
	{
		// DC 2.5: a paper nest the size of a cart, long abandoned, torn open; inside, dead Bramblewasps balled round a dead
		// Hiveback: the bee ball the other way round.
		const FVector C = At(0.f, 0.f, 120.f);
		M.Lathe(C - Ax * 140.f, Ax, 280.f, { FVector(0.f, 30.f, 30.f), FVector(0.2f, 100.f, 90.f), FVector(0.55f, 120.f, 105.f), FVector(0.85f, 95.f, 85.f), FVector(1.f, 20.f, 20.f) }, 14, Paper, 0.9f, 25.f, 3.f);
		for (int32 b = 1; b < 7; ++b) M.Lathe(C - Ax * 140.f + Ax * (40.f * b), Ax, 6.f, { FVector(0.f, 112.f, 98.f), FVector(1.f, 113.f, 99.f) }, 14, Paper * 0.75f, 0.9f);
		// The torn side: a dark hole and the ball inside it.
		const FVector Hole = C + Ay * 110.f;
		M.Lathe(Hole, Ay, 8.f, { FVector(0.f, 80.f, 70.f), FVector(1.f, 70.f, 60.f) }, 12, Black, 0.95f);
		const FVector BallC = Hole + Ay * 30.f - FVector(0, 0, 40.f);
		Ball(M, BallC, 26.f, Rgb(120, 82, 30), 0.95f);   // the Hiveback, furred amber-brown
		for (int32 w = 0; w < 140; ++w)
		{
			const FVector Dir = FVector(Rs.FRandRange(-1.f, 1.f), Rs.FRandRange(-1.f, 1.f), Rs.FRandRange(-1.f, 1.f)).GetSafeNormal();
			const FVector P0 = BallC + Dir * Rs.FRandRange(30.f, 62.f);
			const FVector Lie = FVector::CrossProduct(Dir, FVector(0.3f, 0.7f, 0.2f)).GetSafeNormal();
			M.Lathe(P0 - Lie * 3.f, Lie, 7.f, { FVector(0.f, 1.5f, 1.5f), FVector(0.5f, 2.2f, 2.2f), FVector(1.f, 1.f, 1.f) }, 5, w % 2 ? WaspGold : Black, 0.6f);
		}
		// Paper flakes on the floor where it has rotted.
		for (int32 f = 0; f < 18; ++f) M.Lathe(At(Rs.FRandRange(-300.f, 300.f), Rs.FRandRange(-300.f, 300.f), 2.f), FVector::UpVector, 1.5f, { FVector(0.f, 25.f, 18.f), FVector(1.f, 24.f, 17.f) }, 6, Paper * 0.8f, 0.9f);
	}
	else if (R.Id == TEXT("glowworm_chasm"))
	{
		// DC 2.5: a 25 m drop lit like stars by glowworms; a root stair spirals down its wall (the ledge the generator cut:
		// from the way in, 1.1 turns down to the bottom). No rail: a fall is death.
		const int32 Prev = Index - 1;
		const float A0 = D.Rooms.IsValidIndex(Prev) ? FMath::Atan2(D.Rooms[Prev].C.Y - R.C.Y, D.Rooms[Prev].C.X - R.C.X) : 0.f;
		const float R0 = FMath::Min(R.Hx, R.Hy);
		const float Ring = R0 - 175.f;
		const float Len = 2.f * PI * Ring * 1.1f;
		const int32 Steps = FMath::Max(10, FMath::RoundToInt(Len / Tn(TEXT("treadEvery"), 130.f)));
		for (int32 s = 0; s <= Steps; ++s)
		{
			const float F = float(s) / Steps;
			const float A = A0 + 2.f * PI * 1.1f * F;
			const FVector2D Dir(FMath::Cos(A), FMath::Sin(A));
			const FVector2D XY = R.C + Dir * Ring;
			const float Want = R.Entry - R.Chasm * FMath::Min(1.f, F);
			const FVector P0 = Floor(W, XY, Want);
			const FVector Rad(Dir, 0.f);
			Root(M, { P0 - Rad * 120.f + FVector(0, 0, 6.f), P0 + FVector(0, 0, 10.f), P0 + Rad * 160.f + FVector(0, 0, 30.f) }, 17.f, 12.f, s % 2 ? RootBark : RootBark * 0.85f);
			if (s % 6 == 0) Foxfire(G, P0 + Rad * 170.f + FVector(0, 0, 80.f), -Rad, s);
			if (s % 9 == 0) AddLight(P0 + FVector(0, 0, 220.f), 0.6f);
		}
		if (bDraw)
		{
			// The stars: curtains all down the walls of the drop (and the data's own, BuildGlows).
			for (int32 c = 0; c < 10; ++c)
			{
				const float A = A0 + c * 0.7f;
				Curtains.Add(MakeCurtain(this, FVector(R.C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * (R0 - 40.f), R.Entry - 200.f - c * 180.f), 120.f, 700 + c));
				CurtainPhase.Add(c * 0.6f);
			}
		}
	}
	else if (R.Id == TEXT("lattice_chamber"))
	{
		// DC 2.5: the roots fuse into a strangler lattice round a void, a heart-tree's host that rotted out (Sylvaroth's
		// body is the same lattice).
		const FVector C = At(0.f, 0.f, 0.f);
		const float LR = FMath::Min(R.Hx, R.Hy) * 0.45f;
		const float H = FMath::Max(400.f, Ceil - C.Z + 60.f);
		for (int32 Dir = 0; Dir < 2; ++Dir)
		{
			for (int32 k = 0; k < 9; ++k)
			{
				TArray<FVector> Hx;
				for (int32 s = 0; s <= 18; ++s)
				{
					const float F = float(s) / 18.f;
					const float A = 2.f * PI * k / 9.f + (Dir ? 1.f : -1.f) * F * PI * 1.2f;
					const float Rr = LR * (1.f + 0.25f * FMath::Square(1.f - 2.f * F));   // flaring at its foot and top
					Hx.Add(C + FVector(FMath::Cos(A) * Rr, FMath::Sin(A) * Rr, F * H - 30.f));
				}
				Root(M, Hx, 34.f, 22.f, Dir ? RootBark : RootBark * 0.9f);
			}
		}
		for (int32 k = 0; k < 12; ++k)
		{
			const float A = 2.f * PI * k / 12.f;
			Foxfire(G, C + FVector(FMath::Cos(A) * LR * 1.08f, FMath::Sin(A) * LR * 1.08f, 80.f + 60.f * (k % 4)), FVector(FMath::Cos(A), FMath::Sin(A), 0.f), 900 + k);
		}
	}
	else if (R.Id == TEXT("iron_seam"))
	{
		// DC 2.5, 2.6: a collapsed wall: rubble fanned across the far end, the rust-red seam showing in the break.
		for (int32 k = 0; k < 28; ++k)
		{
			const float LX = R.Hx - Rs.FRandRange(60.f, 420.f);
			Lump(M, At(LX, Rs.FRandRange(-R.Hy, R.Hy) * 0.85f, Rs.FRandRange(10.f, 40.f)), Rs.FRandRange(25.f, 70.f), k % 4 ? Stone * 0.7f : Iron, k);
		}
		for (int32 v = 0; v < 5; ++v)
		{
			const FVector P0 = At(R.Hx - 40.f, -R.Hy * 0.7f + v * R.Hy * 0.35f, 60.f + 40.f * v);
			M.Tube({ P0, P0 + Ay * 80.f + FVector(0, 0, 40.f), P0 + Ay * 170.f + FVector(0, 0, 10.f) }, 9.f, 5, Iron, 0.8f);
		}
	}

	if (bDraw)
	{
		Meshes.Add(Draw(this, M, *FString::Printf(TEXT("Room%d"), Index)));
		const FLinearColor Fox(FoxfireLight), Sap(1.f, 0.62f, 0.15f);
		Meshes.Add(Draw(this, G, *FString::Printf(TEXT("RoomFox%d"), Index), &Fox, Tn(TEXT("foxfireGlow"), 2.5f)));
		Meshes.Add(Draw(this, Amb, *FString::Printf(TEXT("RoomSap%d"), Index), &Sap, Tn(TEXT("sapGlow"), 2.f)));
	}
}

void AWRHollowRoot::BuildWaysIn(bool bDraw)
{
	using namespace WRForestHollowImpl;
	const FData& D = Data();
	UWorld* W = GetWorld();
	FWRToolMesh M, G;
	// The Eldest's Door (way 1): the crack's ramp down to the Root Hall is an open cut; foxfire marks it from the crack.
	if (const FSpotX* Door = D.Spot({ TEXT("hollow_door") }))
	{
		const FVector Out = FRotator(0.f, Door->Yaw, 0.f).Vector();
		for (int32 k = 0; k < 4; ++k) Foxfire(G, Floor(W, FVector2D(Door->At + Out * (300.f + 250.f * k)), Door->At.Z - 150.f * k) + FVector(0, 0, 60.f), FVector(-Out.Y, Out.X, 0.f) * (k % 2 ? 1.f : -1.f), 600 + k);
		AddLight(Door->At + Out * 600.f + FVector(0, 0, 100.f), 0.5f);
	}
	// The Sinkroot squeeze (way 2, F-S3): a 30-40 m crawl under a root from the Sinkroot's floor to the Iron Seam, its
	// ceiling roots hanging so low you go stooped (hazards.hollow.squeezeHeight clear). Roofed with walk slabs like the
	// rest, so it stays dark and closed.
	if (const FSpotX* Sq = D.Spot({ TEXT("sinkroot_squeeze"), TEXT("hollow_squeeze") }))
	{
		const FRoomX* Seam = D.Room(TEXT("iron_seam"));
		// The generator's carve runs from the shaft's floor at its east foot straight to the seam's middle, 2 m wide, its
		// floor graded from one to the other; the roof follows that grade at squeezeHeight clear.
		const FVector A = Sq->At;
		const FVector B = Seam ? FVector(Seam->C, Seam->Floor) : Sq->At + FRotator(0.f, Sq->Yaw, 0.f).Vector() * FMath::Max(800.f, Sq->Depth);
		const FVector Dir = (B - A).GetSafeNormal2D();
		const float Full = FMath::Max(1.f, FVector::Dist2D(A, B));
		float Len = Full;
		if (Seam)
		{
			for (float T = 0.f; T < Full; T += 50.f)
			{
				const FVector2D L = Seam->Local(FVector2D(A) + FVector2D(Dir) * T);
				if (FMath::Abs(L.X) <= Seam->Hx + 300.f && FMath::Abs(L.Y) <= Seam->Hy + 300.f) { Len = T + 100.f; break; }
			}
		}
		const float Clear = Tn(TEXT("squeezeHeight"), 210.f);
		const FVector Across(-Dir.Y, Dir.X, 0.f);
		for (float S = 0.f; S < Len; S += 500.f)
		{
			const float E = FMath::Min(Len, S + 500.f);
			const FVector P0 = FMath::Lerp(A, B, S / Full), P1 = FMath::Lerp(A, B, E / Full);
			Slabs.Add(SolidSeg(this, P0 + FVector(0, 0, Clear + 30.f), P1 + FVector(0, 0, Clear + 30.f), 220.f, 30.f));
			for (int32 r = 0; r < 3; ++r)
			{
				const FVector Q = FMath::Lerp(P0, P1, (r + 0.5f) / 3.f) + FVector(0, 0, Clear + 10.f);
				Root(M, { Q - Across * 180.f + FVector(0, 0, -60.f), Q - FVector(0, 0, 18.f), Q + Across * 180.f + FVector(0, 0, -70.f) }, 22.f, 18.f, r % 2 ? RootBark : RootDark);
			}
			Foxfire(G, P0 + Across * 90.f + FVector(0, 0, 40.f), -Across, int32(S));
		}
		AddLight(A + Dir * Len * 0.5f + FVector(0, 0, 120.f), 0.35f);
	}
	if (bDraw)
	{
		Meshes.Add(Draw(this, M, TEXT("WaysRoots")));
		const FLinearColor Fox(FoxfireLight);
		Meshes.Add(Draw(this, G, TEXT("WaysFoxfire"), &Fox, Tn(TEXT("foxfireGlow"), 2.5f)));
	}
}

void AWRHollowRoot::BuildGlows(bool bDraw)
{
	using namespace WRForestHollowImpl;
	// The forest's dark lights from the data's spots (R11): foxfire banks (fallen wood, old roads, the Hollow Root's
	// rooms) and glowworm curtains (stream banks, the falls' overhang, cave mouths, the Glowworm Chasm). Nobody else
	// draws these spots: they're this actor's, inside the Hollow Root and out.
	if (!bDraw) return;
	const FData& D = Data();
	FWRToolMesh G, M;
	int32 n = 0;
	for (const FSpotX& S : D.Spots)
	{
		++n;
		if (S.Kind == TEXT("foxfire"))
		{
			const FVector P = Floor(GetWorld(), FVector2D(S.At), S.At.Z);
			// A bank: a rotten log end furred with glowing brackets, threads of mycelium across the litter.
			const FVector Dir = FRotator(0.f, S.Yaw + 37.f * n, 0.f).Vector();
			Root(M, { P - Dir * 120.f + FVector(0, 0, 10.f), P + Dir * 140.f + FVector(0, 0, 18.f) }, 22.f, 18.f, RootDark);
			Foxfire(G, P + FVector(0, 0, 30.f), FVector(-Dir.Y, Dir.X, 0.f), n);
			Foxfire(G, P + Dir * 80.f + FVector(0, 0, 26.f), FVector(Dir.Y, -Dir.X, 0.f), n + 500);
			AddLight(P + FVector(0, 0, 80.f), WRForestHollowContains(P + FVector(0, 0, 100.f)) ? 0.6f : 0.35f);
		}
		else if (S.Kind == TEXT("glowworm"))
		{
			// Hung from a root bracket 2.5 m up (an overhang's lip, a bank's root).
			const FVector Top = S.At + FVector(0, 0, Tn(TEXT("curtainLift"), 250.f));
			const FVector Dir = FRotator(0.f, S.Yaw, 0.f).Vector();
			Root(M, { Top + Dir * 160.f + FVector(0, 0, 30.f), Top + FVector(0, 0, 8.f), Top - Dir * 60.f }, 14.f, 6.f, RootBark);
			Curtains.Add(MakeCurtain(this, Top, FMath::Max(80.f, S.R * 0.5f), 300 + n));
			CurtainPhase.Add(n * 0.37f);
			AddLight(Top - FVector(0, 0, 60.f), 0.25f);
		}
	}
	Meshes.Add(Draw(this, M, TEXT("GlowRoots")));
	const FLinearColor Fox(FoxfireLight);
	Meshes.Add(Draw(this, G, TEXT("GlowFoxfire"), &Fox, Tn(TEXT("foxfireGlow"), 2.5f)));
}

void AWRHollowRoot::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bBuilt)
	{
		BuildClock -= DeltaSeconds;
		if (BuildClock > 0.f) return;
		BuildClock = 1.f;
		--BuildTries;
		Build();
		return;
	}
	if (GetNetMode() == NM_DedicatedServer) { SetActorTickEnabled(false); return; }
	const float T = GetWorld()->GetTimeSeconds();
	LightClock -= DeltaSeconds;
	if (LightClock <= 0.f)
	{
		LightClock = 0.5f;
		// Only the lights near you burn (there are a few hundred foxfire glows; shadowless, but not free).
		const APlayerController* PC = GetWorld()->GetFirstPlayerController();
		FVector View; FRotator Rot;
		if (PC && PC->IsLocalController())
		{
			PC->GetPlayerViewPoint(View, Rot);
			const float Range = WRForestHollowImpl::Tn(TEXT("lightRange"), 6000.f);
			for (int32 i = 0; i < Lights.Num(); ++i) if (Lights[i]) Lights[i]->SetVisibility(FVector::DistSquared(View, LightAt[i]) < Range * Range);
		}
	}
	// The curtains stir in the cave's draught.
	SwayClock += DeltaSeconds;
	if (SwayClock > 0.05f)
	{
		SwayClock = 0.f;
		for (int32 i = 0; i < Curtains.Num(); ++i)
		{
			if (!Curtains[i]) continue;
			const float P = CurtainPhase.IsValidIndex(i) ? CurtainPhase[i] : 0.f;
			Curtains[i]->SetRelativeRotation(FRotator(1.4f * FMath::Sin(T * 0.7f + P), 0.f, 1.4f * FMath::Cos(T * 0.53f + P * 1.3f)));
		}
	}
	// The Thrumming Root's heartbeat: lub-dub every 1.6 s, the root swelling a hair and its sap veins flaring.
	if (PulseRoot)
	{
		const float Period = WRForestHollowImpl::Tn(TEXT("beatSeconds"), 1.6f);
		const float F = FMath::Frac(T / Period);
		const float Beat = FMath::Exp(-FMath::Square(F / 0.05f)) + 0.6f * FMath::Exp(-FMath::Square((F - 0.18f) / 0.05f));
		PulseRoot->SetRelativeScale3D(FVector(1.f, 1.f + 0.015f * Beat, 1.f + 0.015f * Beat));
		if (PulseLight) PulseLight->SetIntensity(WRForestHollowImpl::Tn(TEXT("thrumLight"), 1200.f) * (0.3f + Beat));
	}
}

// ================================================================================================ AWRForestSecret

namespace WRForestHollowImpl
{
	float ReachOf(FName Kind)
	{
		if (Kind == TEXT("bell")) return Ts(TEXT("bellReach"), 450.f);
		if (Kind == TEXT("heart_door")) return Tn(TEXT("doorReach"), 450.f);
		if (Kind == TEXT("thrumming_root")) return Tn(TEXT("thrumReach"), 500.f);
		if (Kind == TEXT("root_plug")) return Tn(TEXT("digRadius"), 400.f);
		return Ts(TEXT("reach"), 300.f);
	}

	/** The Cradle Vault's roof frame (the same fit AWRHollowRoot makes, from the land's own heights). */
	bool VaultFrame(const UWorld* W, FTransform& OutFrame, FVector2D& OutLocal, const FRoomX*& OutRoom)
	{
		FVector At; int32 Ri = INDEX_NONE;
		if (!PlugSpot(At, OutLocal, Ri)) return false;
		const FRoomX& R = Data().Rooms[Ri];
		float Cz = R.Roof;
		OutFrame = RoofFrame(W, R.C, R.Yaw, R.Hx + 300.f, R.Hy + 300.f, R.Entry, R.Roof, Cz);
		OutRoom = &R;
		return true;
	}

	void AddIfItem(TMap<FName, int32>& Items, const TCHAR* TuneKey, const TCHAR* Want, int32 Count)
	{
		const FName Id(*Str(TuneKey, Want));
		if (FWRItemRegistry::Get().Find(Id)) Items.Add(Id, Count);
		else UE_LOG(LogWildreach, Warning, TEXT("FOREST secrets: no item '%s' in items.json; left out of its cache"), *Id.ToString());
	}
}

AWRForestSecret::AWRForestSecret()
{
	bReplicates = true;
	SetReplicatingMovement(true);   // the barred door is placed once the lane's real doorway is found
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Block = CreateDefaultSubobject<UBoxComponent>(TEXT("Block"));
	Block->SetupAttachment(RootComponent);
	Block->SetCollisionProfileName(TEXT("BlockAll"));
	Block->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Block->SetCanEverAffectNavigation(false);
	SetNetCullDistanceSquared(FMath::Square(30000.f));
}

void AWRForestSecret::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRForestSecret, Kind);
	DOREPLIFETIME(AWRForestSecret, State);
	DOREPLIFETIME(AWRForestSecret, StateAt);
	DOREPLIFETIME(AWRForestSecret, Size);
	DOREPLIFETIME(AWRForestSecret, Burnt);
}

void AWRForestSecret::BeginPlay()
{
	Super::BeginPlay();
	if (Kind == TEXT("root_plug"))
	{
		// Where its ramp lands: toward the vault's middle, on its floor (the same on every machine, from the data).
		FTransform Frame; FVector2D Local; const WRForestHollowImpl::FRoomX* Room = nullptr;
		if (WRForestHollowImpl::VaultFrame(GetWorld(), Frame, Local, Room))
		{
			const FVector2D Toward = (-Local).GetSafeNormal();
			FVector2D FootL = Local + Toward * WRForestHollowImpl::Tn(TEXT("rampLength"), 2000.f);
			FootL.X = FMath::Clamp(FootL.X, -Room->Hx + 200.f, Room->Hx - 200.f);
			FootL.Y = FMath::Clamp(FootL.Y, -Room->Hy + 200.f, Room->Hy - 200.f);
			RampFoot = FVector(Room->World(FootL), Room->Floor);
		}
	}
	BuildSolids();
	LookBuiltAt = GetActorLocation();
	if (GetNetMode() != NM_DedicatedServer) BuildLook();
	OnRep_State();
}

bool AWRForestSecret::InReach(const FVector& At, float Slack) const
{
	const FVector Me = GetActorLocation();
	if (Kind == TEXT("barred_door") && Size <= 0.f) return false;   // not placed on its doorway yet
	if (FVector::Dist(At, Me + FVector(0, 0, 100.f)) > WRForestHollowImpl::ReachOf(Kind) + Slack) return false;
	if (Kind == TEXT("heart_door"))
	{
		// One way: only from the tunnel's side (the actor faces out into the clearing).
		return GetActorTransform().InverseTransformPositionNoScale(At).X < 0.f;
	}
	return true;
}

FString AWRForestSecret::PromptVerb(const AActor* Who, FString& OutTarget) const
{
	const bool bInside = Who && GetActorTransform().InverseTransformPositionNoScale(Who->GetActorLocation()).X < 0.f;
	if (Kind == TEXT("heart_door"))
	{
		OutTarget = TEXT("A door of living roots");
		return State ? FString(TEXT("Open: the way to the Four Hearts")) : bInside ? FString(TEXT("Part the roots")) : FString(TEXT("Grown shut: it opens from the other side"));
	}
	if (Kind == TEXT("root_plug"))
	{
		OutTarget = TEXT("Soft earth bound with roots");
		return State ? FString(TEXT("Dug through: a way down into the dark")) : FString(TEXT("Hollow under your feet: a Truffler could dig it out"));
	}
	if (Kind == TEXT("thrumming_root")) { OutTarget = TEXT("The Thrumming Root"); return TEXT("Lay a hand on it"); }
	if (Kind == TEXT("seed_store")) { OutTarget = TEXT("Grovekin seed jars"); return TEXT("Break a seal (one seed a day)"); }
	if (Kind == TEXT("bone_spear")) { OutTarget = TEXT("A heartwood spear in a giant's hand"); return TEXT("Ease it free"); }
	if (Kind == TEXT("barred_door"))
	{
		if (State) return FString();   // burnt open: what's inside has its own prompt
		OutTarget = TEXT("A door barred from inside, grown shut");
		return Burnt > 0.01f ? FString::Printf(TEXT("Burning (%d%%): keep the torch to it"), FMath::RoundToInt(Burnt * 100.f)) : FString(TEXT("Its bark is alive: hold a torch to it"));
	}
	if (Kind == TEXT("bell")) { OutTarget = TEXT("The Grovekin bell"); return TEXT("Ring it"); }
	if (Kind == TEXT("eel_trap")) { OutTarget = TEXT("A Grovekin eel trap, still baited"); return State ? FString(TEXT("Take the catch")) : FString(TEXT("Empty: it fishes again by tomorrow")); }
	if (Kind == TEXT("grotto_shrine")) { OutTarget = TEXT("A Grovekin fishing shrine"); return TEXT("Take the hook from the offering stone"); }
	return FString();
}

FString AWRForestSecret::Use(AWRPlayerController* PC)
{
	using namespace WRForestHollowImpl;
	AWRPlayerState* PS = StateOf(PC);
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!PS || !Pawn || !HasAuthority()) return FString();
	const FString Who = PS->GetPlayerName();
	UWRInventoryComponent* Bag = PS->GetInventory();
	UWorld* W = GetWorld();
	if (!Bag || !W) return FString();
	const float T = Now(this);
	if (Kind == TEXT("heart_door"))
	{
		if (State) return TEXT("The roots stand open");
		if (!InReach(Pawn->GetActorLocation(), 150.f)) return TEXT("The roots are grown shut from this side");
		// Elden Ring's rule for a shortcut: once opened from inside, it stays open for everyone.
		State = 1;
		StateAt = T;
		ForceNetUpdate();
		OnRep_State();
		WRFx::Broadcast(this, TEXT("forest.secret_roots_part"), GetActorLocation() + FVector(0, 0, 150.f), GetActorRotation(), 1.f, TEXT("forest.vine_creep"));
		UE_LOG(LogWildreach, Log, TEXT("FOREST hollow: %s opened the Heart Root"), *Who);
		return TEXT("The roots draw back: daylight, and the Four Hearts' clearing");
	}
	if (Kind == TEXT("root_plug")) return State ? TEXT("Drop down: the soil's piled into a ramp below") : TEXT("Something under the roots sounds hollow. A tamed Truffler sent here would dig it out");
	if (Kind == TEXT("thrumming_root"))
	{
		if (Told.Contains(Who)) return TEXT("The pulse runs on through the roots, south-west, toward the Four Hearts");
		const FName Lore = ItemOr(TEXT("hazards.hollow.thrumLore"), TEXT("lore_root_pulse"), TEXT("resin"));
		if (Bag->AddItem(Lore, 1) > 0) return TEXT("Inventory full");
		Told.Add(Who);
		return FString::Printf(TEXT("Under your palm the root beats, slow as a sleeper's heart. You keep a sliver of its bark (%s)"), *ItemName(Lore));
	}
	if (Kind == TEXT("seed_store"))
	{
		const int32 Today = Day(this);
		int32& Last = DayBy.FindOrAdd(Who, -1);
		if (Last == Today) return TEXT("You've opened a jar today: the rest keep");
		const FName Seed = WRForestHollow::RareSeed();
		if (Seed.IsNone() || !FWRItemRegistry::Get().Find(Seed)) return TEXT("The jars hold only dust");
		if (Bag->AddItem(Seed, 1) > 0) return TEXT("Inventory full");
		Last = Today;
		WRFx::Broadcast(this, TEXT("forest.secret_dust"), GetActorLocation() + FVector(0, 0, 60.f), FRotator::ZeroRotator, 0.6f, TEXT("pickup"));
		return FString::Printf(TEXT("Under the wax, seed kept dry for a hundred years: %s"), *ItemName(Seed));
	}
	if (Kind == TEXT("bone_spear"))
	{
		if (Told.Contains(Who)) return TEXT("The hand holds what's left of it");
		const FName Spear = ItemOr(TEXT("hazards.hollow.spearItem"), TEXT("heartwood_spear"), TEXT("heartwood"));
		const int32 N = Spear == TEXT("heartwood") ? int32(Tn(TEXT("spearHeartwood"), 8.f)) : 1;
		if (Bag->AddItem(Spear, N) > 0) return TEXT("Inventory full");
		Told.Add(Who);
		return FString::Printf(TEXT("The finger bones give one by one. %s"), *ItemName(Spear));
	}
	if (Kind == TEXT("barred_door")) return State ? FString() : TEXT("Grown shut. The bark over it is alive: a torch held to it 6 s would burn it through");
	if (Kind == TEXT("bell"))
	{
		// F-S5: rung once a game day it calls every Glimmerfinch within 300 m to the nearest ripe plant or hive; at night it
		// brings the Grey Matron out of the Bower to hunt the bell.
		WRFx::Broadcast(this, TEXT("forest.secret_bell"), GetActorLocation() + FVector(0, 0, 300.f), FRotator::ZeroRotator, 1.f, TEXT("forest.bell"));
		++State;
		StateAt = T;
		ForceNetUpdate();
		const double H = Hours(this);
		if (NextAtHours > 0.0 && H < NextAtHours) return TEXT("The note rolls out over the canopy, and dies. The wood heard it today already");
		NextAtHours = (FMath::FloorToDouble(H / 24.0) + 1.0) * 24.0;
		const float Range = Ts(TEXT("bellRange"), 30000.f);
		int32 Finches = 0;
		const FName FinchId(*Str(TEXT("hazards.secrets.bellCreature"), TEXT("glimmerfinch")));
		TArray<FString> Lures;
		FString Ripe = Str(TEXT("hazards.secrets.bellLures"), TEXT("honey,meadow_oats,idunn_apple"));
		Ripe.ParseIntoArray(Lures, TEXT(","));
		for (TActorIterator<AWRCreature> It(W); It; ++It)
		{
			AWRCreature* C = *It;
			if (C->CreatureId != FinchId || !C->IsAlive() || FVector::Dist(C->GetActorLocation(), GetActorLocation()) > Range) continue;
			++Finches;
			if (C->IsTamed()) { FWRForestAI::Whistled(C, Pawn); continue; }
			// A wild flock goes to the nearest ripe wild plant or hive and hovers there (its home moves to it).
			AWRResourceNode* Best = nullptr;
			float BestD = Range;
			for (TActorIterator<AWRResourceNode> N(W); N; ++N)
			{
				if (N->IsDepleted() || !Lures.Contains(N->ItemId.ToString())) continue;
				const float Dd = FVector::Dist(N->GetActorLocation(), C->GetActorLocation());
				if (Dd < BestD) { BestD = Dd; Best = *N; }
			}
			if (Best) C->SetHome(Best->GetActorLocation() + FVector(0, 0, 200.f));
		}
		if (AWRGameState::IsNightIn(this)) WRForest::SummonMatronToBell(W, GetActorLocation());
		UE_LOG(LogWildreach, Log, TEXT("FOREST bell rung by %s (%d finches; night %d)"), *Who, Finches, AWRGameState::IsNightIn(this) ? 1 : 0);
		return AWRGameState::IsNightIn(this) ? TEXT("The bell's note goes out over the dark wood. Something in the south-west answers it") : TEXT("The bell rings out, and the canopy fills with finches");
	}
	if (Kind == TEXT("eel_trap"))
	{
		if (!State) return TEXT("Empty. The bait's fresh: it fishes again by tomorrow");
		const FName Eel = ItemOr(TEXT("hazards.secrets.eelItem"), TEXT("fish_eel"), TEXT("fish_brook_trout"));
		if (Bag->AddItem(Eel, 1) > 0) return TEXT("Inventory full");
		State = 0;
		StateAt = T;
		NextAtHours = double(Day(this) + 1);   // the day it fishes again
		ForceNetUpdate();
		OnRep_State();
		return FString::Printf(TEXT("You tip the trap out: %s"), *ItemName(Eel));
	}
	if (Kind == TEXT("grotto_shrine"))
	{
		if (Told.Contains(Who)) return TEXT("The offering stone is bare: you took its hook");
		const FName Hook = ItemOr(TEXT("hazards.secrets.hookItem"), TEXT("sturgeon_hook"), TEXT("fish_brook_trout"));
		if (Bag->AddItem(Hook, 1) > 0) return TEXT("Inventory full");
		Told.Add(Who);
		return FString::Printf(TEXT("Under the fish bones, wrapped in oiled gut: %s"), *ItemName(Hook));
	}
	return FString();
}

void AWRForestSecret::BuildSolids()
{
	using namespace WRForestHollowImpl;
	for (UPrimitiveComponent* S : Solids) if (S) S->DestroyComponent();
	Solids.Reset();
	const FTransform Tr = GetActorTransform();
	const auto L = [&Tr](float X, float Y, float Z) { return Tr.TransformPositionNoScale(FVector(X, Y, Z)); };
	if (Kind == TEXT("heart_door"))
	{
		// A wall of woven root across the tunnel with the door in its middle (3 m), roofed back 8 m at the forest floor so
		// nobody comes over it from the clearing's side.
		const float Half = FMath::Max(300.f, Size * 0.5f + 100.f);
		const float Up = Tn(TEXT("doorDepth"), 500.f) + 80.f;
		for (const float S : { -1.f, 1.f }) Solids.Add(Solid(this, FTransform(Tr.GetRotation(), L(0.f, S * (Half + 150.f) * 0.5f, Up * 0.5f)), FVector(60.f, (Half - 150.f) * 0.5f, Up * 0.5f)));
		Solids.Add(Solid(this, FTransform(Tr.GetRotation(), L(0.f, 0.f, (Up + 300.f) * 0.5f)), FVector(60.f, 160.f, (Up - 300.f) * 0.5f)));
		Solids.Add(Solid(this, FTransform(Tr.GetRotation(), L(-420.f, 0.f, Up - 30.f)), FVector(420.f, Half + 200.f, 30.f)));
		Block->SetRelativeLocation(FVector(0.f, 0.f, 150.f));
		Block->SetBoxExtent(FVector(40.f, 155.f, 150.f));
	}
	else if (Kind == TEXT("root_plug"))
	{
		const float H = Tn(TEXT("digHole"), 170.f);
		Block->SetRelativeLocation(FVector(0.f, 0.f, -30.f));
		Block->SetBoxExtent(FVector(H + 10.f, H + 10.f, 30.f));
		// The ramp the plug and its soil make when it falls (off until it's dug through).
		if (UBoxComponent* Ramp = SolidSeg(this, L(0.f, 0.f, -170.f), RampFoot + FVector(0, 0, -15.f), 200.f, 25.f))
		{
			Ramp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Solids.Add(Ramp);
		}
	}
	else if (Kind == TEXT("bell"))
	{
		// F-S5's root holds: a living root climbs the Bell-Tree in a spiral from its foot to a shelf under the bell, one
		// hold every 35 cm of rise (a stair you can run), hugging the kit's own bole (it leans).
		FVector Bell, Base; FRotator Rot; float R = 300.f, Scale = 1.f, Height = 7000.f; int32 Var = 0;
		if (!BellSpot(Bell, Base, Rot, R, Scale, Var, Height)) return;
		const FVector Land = GetActorLocation();
		const float Rise = Ts(TEXT("holdRise"), 35.f), Tread = Ts(TEXT("holdTread"), 85.f);
		const int32 N = FMath::Max(4, FMath::CeilToInt((Land.Z - Base.Z) / Rise));
		const float EndA = FMath::Atan2(Land.Y - Base.Y, Land.X - Base.X);
		float A = EndA;
		TArray<FVector> Holds;
		for (int32 i = N; i >= 0; --i)
		{
			const float Z = Base.Z + (Land.Z - Base.Z) * float(i) / N;
			FVector C; float Rr = R;
			TrunkAt(Base, Rot, Scale, Var, Z, R, C, Rr);
			const float Ring = Rr + 70.f;
			Holds.Insert(FVector(C.X + FMath::Cos(A) * Ring, C.Y + FMath::Sin(A) * Ring, Z), 0);
			A -= Tread / FMath::Max(100.f, Ring);
		}
		for (int32 i = 0; i < Holds.Num(); ++i)
		{
			const FVector P = Holds[i];
			const FVector Out = FVector(P.X - Base.X, P.Y - Base.Y, 0.f).GetSafeNormal();
			const FVector Tang = FVector::CrossProduct(FVector::UpVector, Out).GetSafeNormal();
			Solids.Add(Solid(this, FTransform(Tang.Rotation(), P - FVector(0, 0, 10.f)), FVector(Tread * 0.5f + 8.f, 65.f, 10.f)));
		}
		// The shelf under the bell.
		Solids.Add(Solid(this, FTransform(FRotator(0.f, FMath::RadiansToDegrees(EndA), 0.f), Land - FVector(0, 0, 15.f)), FVector(170.f, 170.f, 15.f)));
	}
	else if (Kind == TEXT("grotto_shrine"))
	{
		// F-S2's back room: a dry stone room 6 x 5 m stacked from fallen basalt under the falls' overhang, its only way in a
		// squeeze 0.95 m wide at its front-left (the glowworms over it sway in the draught: the tell).
		const float HX = 300.f, HY = 250.f, Hh = 290.f;
		Solids.Add(Solid(this, FTransform(Tr.GetRotation(), L(0.f, 0.f, Hh + 25.f)), FVector(HX + 60.f, HY + 60.f, 25.f)));           // roof slab
		Solids.Add(Solid(this, FTransform(Tr.GetRotation(), L(-HX - 30.f, 0.f, Hh * 0.5f)), FVector(30.f, HY + 60.f, Hh * 0.5f)));     // back
		for (const float S : { -1.f, 1.f }) Solids.Add(Solid(this, FTransform(Tr.GetRotation(), L(0.f, S * (HY + 30.f), Hh * 0.5f)), FVector(HX + 60.f, 30.f, Hh * 0.5f)));
		// The front wall, with the squeeze at Y in [-HY+40, -HY+135].
		Solids.Add(Solid(this, FTransform(Tr.GetRotation(), L(HX + 30.f, 67.5f, Hh * 0.5f)), FVector(30.f, HY - 67.5f + 30.f, Hh * 0.5f)));
		Solids.Add(Solid(this, FTransform(Tr.GetRotation(), L(HX + 30.f, -HY + 10.f, Hh * 0.5f)), FVector(30.f, 30.f, Hh * 0.5f)));
		Solids.Add(Solid(this, FTransform(Tr.GetRotation(), L(HX + 30.f, -HY + 87.f, (Hh + 205.f) * 0.5f)), FVector(30.f, 50.f, (Hh - 205.f) * 0.5f)));
	}
}

void AWRForestSecret::BuildLook()
{
	using namespace WRForestHollowImpl;
	if (bLookBuilt) return;
	bLookBuilt = true;
	const FTransform Tr = GetActorTransform();
	FWRToolMesh M, G, Mv, Ct;
	const auto L = [&Tr](float X, float Y, float Z) { return Tr.TransformPositionNoScale(FVector(X, Y, Z)); };
	FRandomStream Rs(int32(GetTypeHash(GetActorLocation().ToCompactString())) & 0x7fffffff);
	// Every mesh here is drawn in world space on components at the actor's origin with identity relative transforms.
	if (Kind == TEXT("heart_door"))
	{
		const float Half = FMath::Max(300.f, Size * 0.5f + 100.f), Up = Tn(TEXT("doorDepth"), 500.f) + 80.f;
		// The wall: roots woven across the tunnel, thickest at its foot, grown into the walls and the ceiling.
		for (int32 r = 0; r < 14; ++r)
		{
			const float Y = -Half + 2.f * Half * r / 13.f;
			if (FMath::Abs(Y) < 150.f) continue;
			Root(M, { L(0.f, Y, -30.f), L(Rs.FRandRange(-20.f, 20.f), Y + Rs.FRandRange(-40.f, 40.f), Up * 0.5f), L(0.f, Y * 0.9f, Up + 40.f) }, 34.f, 20.f, r % 2 ? RootBark : RootDark);
		}
		for (int32 r = 0; r < 6; ++r) Root(M, { L(0.f, -Half, 60.f + r * Up / 6.f), L(15.f, 0.f, 80.f + r * Up / 6.f + (r < 3 ? 260.f : 0.f)), L(0.f, Half, 60.f + r * Up / 6.f) }, 26.f, 26.f, RootBark * 0.9f);
		// The door: a curtain of hanging roots (Moving: they draw up into the lintel when it opens).
		for (int32 r = 0; r < 12; ++r)
		{
			const float Y = -135.f + 270.f * r / 11.f;
			Mv.Tube({ FVector(0.f, Y, 300.f), FVector(6.f * FMath::Sin(r * 1.3f), Y + 8.f, 150.f), FVector(0.f, Y - 6.f, 0.f) }, 10.f + 3.f * (r % 3), 6, r % 2 ? RootBark : RootDark, 0.9f);
		}
		Foxfire(G, L(-40.f, -200.f, 120.f), -Tr.GetRotation().GetForwardVector(), 77);
		Foxfire(G, L(-40.f, 200.f, 160.f), -Tr.GetRotation().GetForwardVector(), 78);
	}
	else if (Kind == TEXT("root_plug"))
	{
		// The plug: soil and fine roots matted into the gap in the vault's roof, a ring of torn root ends round it (the
		// soft spot reads as darker, sunken ground from above); falls through when dug.
		const float H = Tn(TEXT("digHole"), 170.f);
		Mv.Lathe(FVector(0, 0, -70.f), FVector::UpVector, 70.f, { FVector(0.f, H * 0.7f, H * 0.7f), FVector(0.7f, H * 1.05f, H * 1.05f), FVector(1.f, H * 1.1f, H * 1.1f) }, 12, Soil, 0.98f);
		for (int32 k = 0; k < 10; ++k)
		{
			const float A = 2.f * PI * k / 10.f;
			Mv.Tube({ FVector(FMath::Cos(A) * H * 1.1f, FMath::Sin(A) * H * 1.1f, -5.f), FVector(FMath::Cos(A + 0.4f) * H * 0.4f, FMath::Sin(A + 0.4f) * H * 0.4f, 4.f) }, 5.f, 5, RootDark, 0.9f);
		}
		// The pile it leaves on the vault's floor (drawn once dug): a slope of soil, roots and stones.
		const FVector Top = L(0.f, 0.f, -170.f);
		const FVector Dir = (RampFoot - Top);
		Ct.Lathe(Top, Dir.GetSafeNormal(), Dir.Size() + 80.f, { FVector(0.f, 160.f, 40.f), FVector(0.5f, 230.f, 55.f), FVector(1.f, 260.f, 30.f) }, 12, Soil * 1.2f, 0.98f);
		for (int32 k = 0; k < 8; ++k) Root(Ct, { FMath::Lerp(Top, RampFoot, Rs.FRandRange(0.f, 1.f)) + FVector(Rs.FRandRange(-120.f, 120.f), Rs.FRandRange(-120.f, 120.f), 40.f), FMath::Lerp(Top, RampFoot, Rs.FRandRange(0.f, 1.f)) + FVector(0, 0, 50.f) }, 10.f, 4.f, RootDark);
	}
	else if (Kind == TEXT("thrumming_root"))
	{
		// Where hands have rested on it: the bark worn smooth and dark, a ring of offerings at its foot.
		for (int32 k = 0; k < 5; ++k) Ball(G, L(-10.f, (k - 2) * 40.f, 120.f + 30.f * (k % 2)), 9.f, FLinearColor(1.f, 0.62f, 0.15f), 0.3f, 0.3f);
		for (int32 k = 0; k < 7; ++k) Lump(M, L(Rs.FRandRange(30.f, 90.f), Rs.FRandRange(-120.f, 120.f), 6.f), 8.f, Stone, k);
	}
	else if (Kind == TEXT("seed_store"))
	{
		// The jar the Grovekin kept open: a big one with a seed carved on its belly, its seal broken and re-pressed.
		Jar(M, L(0.f, 0.f, 0.f), 95.f, 3);
		Ball(M, L(30.f, 0.f, 50.f), 7.f, Rgb(84, 60, 36), 0.7f, 1.4f);
	}
	else if (Kind == TEXT("bell"))
	{
		// The root holds (the collision is BuildSolids'): a living root climbing the bole, a shelf of root at each hold.
		TArray<FVector> Vine;
		for (UPrimitiveComponent* S : Solids)
		{
			const UBoxComponent* B = Cast<UBoxComponent>(S);
			if (!B || B->GetUnscaledBoxExtent().Y > 100.f) continue;
			const FVector P = B->GetComponentLocation() + FVector(0, 0, 10.f);
			const FVector Tg = B->GetComponentQuat().GetForwardVector();
			const FVector Out = FVector::CrossProduct(Tg, FVector::UpVector);
			M.Lathe(P - Out * 70.f - FVector(0, 0, 8.f), Out, 140.f, { FVector(0.f, 30.f, 14.f), FVector(0.5f, 40.f, 12.f), FVector(1.f, 18.f, 8.f) }, 7, RootBark, 0.9f, 4.f, P.Z);
			Vine.Add(P - Out * 60.f - FVector(0, 0, 25.f));
		}
		if (Vine.Num() > 1) for (int32 i = 0; i + 1 < Vine.Num(); i += 6) Root(M, { Vine[i], Vine[FMath::Min(i + 6, Vine.Num() - 1)] }, 16.f, 14.f, RootDark);
		M.Lathe(GetActorLocation() - FVector(0, 0, 30.f), FVector::UpVector, 30.f, { FVector(0.f, 170.f, 170.f), FVector(1.f, 180.f, 180.f) }, 10, RootBark, 0.9f);
	}
	else if (Kind == TEXT("barred_door"))
	{
		// Inside (F-S4): a Grovekin family seated round their table, grown into the chairs by the roots, hands joined.
		const FVector C = L(-250.f, 0.f, 0.f);
		M.Lathe(C, FVector::UpVector, 75.f, { FVector(0.f, 12.f, 12.f), FVector(0.9f, 10.f, 10.f), FVector(0.92f, 60.f, 60.f), FVector(1.f, 60.f, 60.f) }, 12, Rgb(104, 78, 52), 0.8f);
		Ball(M, C + FVector(10, 5, 82.f), 12.f, Rgb(150, 110, 60), 0.7f, 0.6f);   // the last loaf, stone-hard
		for (int32 s = 0; s < 4; ++s)
		{
			const FVector Dir = FRotator(0.f, 45.f + 90.f * s, 0.f).RotateVector(Tr.GetRotation().GetForwardVector());
			const FVector Seat = C + Dir * 95.f;
			M.Lathe(Seat, FVector::UpVector, 45.f, { FVector(0.f, 22.f, 22.f), FVector(1.f, 20.f, 20.f) }, 8, RootBark, 0.9f);
			M.Lathe(Seat + FVector(0, 0, 45.f), FVector::UpVector, 60.f, { FVector(0.f, 17.f, 13.f), FVector(0.7f, 15.f, 12.f), FVector(1.f, 8.f, 8.f) }, 8, BoneOld * 0.8f, 0.9f);   // the body, bark-wrapped
			Ball(M, Seat + FVector(0, 0, 118.f), 11.f, BoneOld * 0.9f, 0.9f);
			const FVector Next = C + FRotator(0.f, 90.f, 0.f).RotateVector(Dir) * 95.f;
			M.Tube({ Seat + FVector(0, 0, 90.f), FMath::Lerp(Seat, Next, 0.5f) + FVector(0, 0, 78.f) - Dir * 10.f, Next + FVector(0, 0, 90.f) }, 3.5f, 5, BoneOld * 0.85f, 0.9f);   // hands joined
			Root(M, { Seat + Dir * 40.f - FVector(0, 0, 10.f), Seat + FVector(0, 0, 60.f), Seat + FVector(0, 0, 130.f) - Dir * 10.f }, 7.f, 3.f, RootDark);
		}
		// The burnt-through door: a charred ring round the opening, embers along its edge (shown once it's open).
		for (int32 k = 0; k < 16; ++k)
		{
			const float A = 2.f * PI * k / 16.f;
			const FVector P = L(15.f, FMath::Cos(A) * 70.f, 105.f + FMath::Sin(A) * 105.f);
			Ct.Lathe(P - Tr.GetRotation().GetForwardVector() * 4.f, Tr.GetRotation().GetForwardVector(), 10.f, { FVector(0.f, 18.f, 14.f), FVector(1.f, 14.f, 10.f) }, 6, Black, 0.95f);
			if (k % 2 == 0) Ball(G, P + Tr.GetRotation().GetForwardVector() * 8.f, 3.f, FLinearColor(1.f, 0.35f, 0.08f), 0.5f);
		}
	}
	else if (Kind == TEXT("eel_trap"))
	{
		// DC 2.4 #14: a Grovekin wicker funnel trap lashed to an ash stake in the steps' pool, its mouth upstream.
		M.Lathe(L(0.f, 0.f, -120.f), FVector::UpVector, 230.f, { FVector(0.f, 5.f, 5.f), FVector(1.f, 3.5f, 3.5f) }, 6, Rgb(150, 132, 104), 0.85f);
		for (int32 b = 0; b < 9; ++b)
		{
			const float X = -20.f + 15.f * b, R = 30.f - 2.5f * b;
			M.Lathe(L(X, 0.f, -10.f), Tr.GetRotation().GetForwardVector(), 4.f, { FVector(0.f, R, R), FVector(1.f, R, R) }, 10, Wicker * (b % 2 ? 0.85f : 1.f), 0.9f);
		}
		for (int32 s = 0; s < 10; ++s)
		{
			const float A = 2.f * PI * s / 10.f;
			M.Tube({ L(-25.f, FMath::Cos(A) * 30.f, -10.f + FMath::Sin(A) * 30.f), L(115.f, FMath::Cos(A) * 8.f, -10.f + FMath::Sin(A) * 8.f) }, 1.2f, 3, Wicker, 0.9f);
		}
		M.Tube({ L(100.f, 0.f, -10.f), L(5.f, 30.f, 30.f), L(0.f, 5.f, 60.f) }, 1.5f, 4, Rgb(90, 76, 52), 0.9f);   // the lashing
		// The catch (shown while there is one): a dark eel coiled in the funnel.
		TArray<FVector> Eel;
		for (int32 k = 0; k <= 10; ++k) Eel.Add(L(10.f + 9.f * k, 10.f * FMath::Sin(k * 0.9f), -14.f + 3.f * FMath::Cos(k * 0.9f)));
		Ct.Tube(Eel, 3.5f, 6, Rgb(46, 42, 30), 0.4f);
	}
	else if (Kind == TEXT("grotto_shrine"))
	{
		// The back room's stone (matches BuildSolids): courses of black basalt blocks, a slab roof, a dry gravel floor.
		const float HX = 300.f, HY = 250.f, Hh = 290.f;
		const FLinearColor Basalt = Rgb(52, 52, 54);
		const auto Course = [&](const FVector& A, const FVector& B)
		{
			const float Len = FVector::Dist(A, B);
			for (float Z = 0.f; Z < Hh; Z += 48.f)
				for (float S = 0.f; S < Len; S += 70.f)
					Lump(M, FMath::Lerp(A, B, (S + (int32(Z / 48.f) % 2) * 35.f) / FMath::Max(1.f, Len)) + FVector(0, 0, Z + 24.f), 34.f, Basalt * (0.85f + 0.15f * Jit(A + FVector(S, Z, 0), 3)), int32(S + Z));
		};
		Course(L(-HX - 30.f, -HY - 30.f, 0.f), L(-HX - 30.f, HY + 30.f, 0.f));
		Course(L(-HX, -HY - 30.f, 0.f), L(HX + 30.f, -HY - 30.f, 0.f));
		Course(L(-HX, HY + 30.f, 0.f), L(HX + 30.f, HY + 30.f, 0.f));
		Course(L(HX + 30.f, -HY + 135.f, 0.f), L(HX + 30.f, HY + 30.f, 0.f));
		M.Lathe(L(0.f, 0.f, Hh), FVector::UpVector, 50.f, { FVector(0.f, HX + 80.f, HY + 80.f), FVector(1.f, HX + 60.f, HY + 60.f) }, 8, Basalt * 0.8f, 0.95f);
		// The shrine: a stone with a carved sturgeon, fish bones heaped as offerings, a Grovekin rod rack on the wall.
		const FVector St = L(-HX + 80.f, 0.f, 0.f);
		M.Lathe(St, FVector::UpVector, 90.f, { FVector(0.f, 45.f, 40.f), FVector(1.f, 38.f, 34.f) }, 7, Stone, 0.85f);
		M.Lathe(St + FVector(0, 0, 70.f) - Tr.GetRotation().GetRightVector() * 30.f, Tr.GetRotation().GetRightVector(), 60.f, { FVector(0.f, 3.f, 6.f), FVector(0.4f, 6.f, 12.f), FVector(1.f, 1.f, 1.f) }, 6, Stone * 0.7f, 0.8f);
		for (int32 b = 0; b < 16; ++b) BoneSeg(M, St + FVector(Rs.FRandRange(30.f, 70.f), Rs.FRandRange(-50.f, 50.f), 92.f), St + FVector(Rs.FRandRange(30.f, 70.f), Rs.FRandRange(-50.f, 50.f), 94.f), 1.2f, Bone);
		for (int32 r = 0; r < 3; ++r) M.Lathe(L(-HX + 20.f, -120.f + 60.f * r, 60.f), FRotator(70.f, Tr.Rotator().Yaw + 90.f, 0.f).Vector(), 240.f, { FVector(0.f, 2.f, 2.f), FVector(1.f, 1.f, 1.f) }, 5, Rgb(150, 88, 34), 0.6f);
		// The draught tell: glowworms on the squeeze's lintel, swaying (Clapper sways them).
		Clapper = MakeCurtain(this, L(HX + 60.f, -HY + 87.f, 215.f), 60.f, 4242);
	}
	Mesh = Draw(this, M, TEXT("SecretLook"));
	if (Mv.Verts.Num() > 0)
	{
		Moving = NewObject<USceneComponent>(this);
		Moving->SetupAttachment(RootComponent);
		Moving->RegisterComponent();
		MovingMesh = NewObject<UProceduralMeshComponent>(this);
		MovingMesh->SetupAttachment(Moving);
		MovingMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MovingMesh->RegisterComponent();
		Mv.ApplyTo(MovingMesh);   // local to the actor (drawn in its frame)
	}
	if (Ct.Verts.Num() > 0)
	{
		Catch = NewMesh(this, TEXT("CatchMesh"));
		Ct.ApplyTo(Catch);
	}
	const FLinearColor Fox(FoxfireLight), Ember(1.f, 0.35f, 0.08f), Sap(1.f, 0.62f, 0.15f);
	Char = Draw(this, G, TEXT("SecretGlow"), Kind == TEXT("barred_door") ? &Ember : Kind == TEXT("thrumming_root") ? &Sap : &Fox, 2.5f);
	// The world-space meshes ride with the actor (their components sit at its origin): undo the actor's transform.
	for (UProceduralMeshComponent* PM : { Mesh.Get(), Catch.Get(), Char.Get() })
	{
		if (!PM) continue;
		PM->SetUsingAbsoluteLocation(true);
		PM->SetUsingAbsoluteRotation(true);
		PM->SetWorldLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
	}
	if (Kind == TEXT("barred_door"))
	{
		Glow = Lamp(this, L(30.f, 0.f, 100.f), FColor(255, 120, 40), 0.f, 450.f);
	}
	if (Kind == TEXT("thrumming_root") && Char) Char->SetCastShadow(false);
}

void AWRForestSecret::SetDoorBar(bool bOn)
{
	UBoxComponent* Bar = WRForestHollowImpl::FindDoorBar(GetWorld(), GetActorLocation() + FVector(0, 0, 105.f), 300.f);
	if (!Bar) return;
	Bar->SetCollisionEnabled(bOn ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	bDoorBarOff = !bOn;
}

void AWRForestSecret::OnRep_State()
{
	const bool bOpen = State != 0;
	if (Kind == TEXT("heart_door")) Block->SetCollisionEnabled(bOpen ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
	else if (Kind == TEXT("root_plug"))
	{
		Block->SetCollisionEnabled(bOpen ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
		for (UPrimitiveComponent* S : Solids) if (S) S->SetCollisionEnabled(bOpen ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		if (Catch) Catch->SetVisibility(bOpen);
		if (bOpen && GetNetMode() != NM_DedicatedServer && WRForestHollowImpl::Now(this) - StateAt < 3.f)
			WRFx::SpawnAt(this, TEXT("forest.hollow_plug_fall"), GetActorLocation(), FRotator::ZeroRotator, 1.f);
	}
	else if (Kind == TEXT("barred_door"))
	{
		if (bOpen) SetDoorBar(false);
		if (Catch) Catch->SetVisibility(bOpen);
	}
	else if (Kind == TEXT("eel_trap"))
	{
		if (Catch) Catch->SetVisibility(bOpen);
	}
}

void AWRForestSecret::StepLook(float Dt)
{
	const float T = WRForestHollowImpl::Now(this);
	LookClock += Dt;
	if (Kind == TEXT("heart_door") && Moving)
	{
		// The roots draw up into the lintel over 2 s from the replicated moment (late joiners see them already up).
		const float A = State ? FMath::Clamp((T - StateAt) / 2.f, 0.f, 1.f) : 0.f;
		const float E = A * A * (3.f - 2.f * A);
		Moving->SetRelativeScale3D(FVector(1.f, 1.f, FMath::Lerp(1.f, 0.08f, E)));
		Moving->SetRelativeLocation(FVector(0.f, 0.f, FMath::Lerp(0.f, 280.f, E)));
	}
	else if (Kind == TEXT("root_plug") && Moving)
	{
		if (State)
		{
			// It falls: gravity, then it's part of the pile below.
			const float S = FMath::Max(0.f, T - StateAt);
			const float Drop = FMath::Min(FMath::Max(100.f, GetActorLocation().Z - RampFoot.Z), 0.5f * 980.f * S * S);
			Moving->SetRelativeLocation(FVector(0.f, 0.f, -Drop));
			Moving->SetRelativeRotation(FRotator(20.f * FMath::Min(1.f, S), 0.f, 12.f * FMath::Min(1.f, S)));
			Moving->SetVisibility(S < 1.6f, true);
		}
		else if (Burnt > 0.f)
		{
			// Being dug: the plug shudders as the soil goes.
			Moving->SetRelativeLocation(FVector(0.f, 0.f, -10.f * Burnt + 1.5f * FMath::Sin(T * 30.f)));
		}
	}
	else if (Kind == TEXT("barred_door") && Glow)
	{
		Glow->SetIntensity(State ? 300.f + 120.f * FMath::Sin(T * 3.f) : Burnt * 2500.f * (0.8f + 0.2f * FMath::Sin(T * 17.f)));
		if (State && !bDoorBarOff && LookClock > 1.f) { LookClock = 0.f; SetDoorBar(false); }   // the lane's scenery came after us
	}
	if (Clapper) Clapper->SetRelativeRotation(FRotator(3.f * FMath::Sin(T * 2.1f), 0.f, 2.f * FMath::Cos(T * 1.7f)));
}

void AWRForestSecret::StepServer(float Dt)
{
	using namespace WRForestHollowImpl;
	ServerClock += Dt;
	if (Kind == TEXT("barred_door"))
	{
		if (Size <= 0.f)
		{
			// Find the lane's real doorway (its scenery's bar) and stand on it; give up after a while and use the estimate.
			if (UBoxComponent* Bar = FindDoorBar(GetWorld(), GetActorLocation() + FVector(0, 0, 105.f), Ts(TEXT("doorSearch"), 900.f)))
			{
				SetActorLocation(Bar->GetComponentLocation() - FVector(0, 0, Bar->GetScaledBoxExtent().Z));
				Size = 1.f;
				ForceNetUpdate();   // the look follows in Tick (it watches where it was drawn)
			}
			else if (ServerClock > 60.f) Size = 1.f;
			if (Size > 0.f)
			{
				// The family's things on the table (F-S4): a heartwood circlet and the "Last loaf".
				TMap<FName, int32> Items;
				AddIfItem(Items, TEXT("hazards.secrets.circletItem"), TEXT("heartwood_circlet"), 1);
				AddIfItem(Items, TEXT("hazards.secrets.loafItem"), TEXT("lore_last_loaf"), 1);
				if (Items.Num() > 0) Cache(GetWorld(), GetActorTransform().TransformPositionNoScale(FVector(-210.f, 60.f, 80.f)), GetActorRotation().Yaw, TEXT("On the family's table"), Items, TEXT("Four of them, hands joined round the table. The door was barred from inside."), Ts(TEXT("cacheRefillHours"), 72.f));
			}
			return;
		}
		if (State) return;
		// A torch held to the living bark for 6 s burns through (DC 2.22 F-S4); fire spells add through FireAt.
		bool bTorch = false;
		for (TActorIterator<AWRCharacter> It(GetWorld()); It; ++It)
		{
			if (!It->IsAlive() || FVector::Dist(It->GetActorLocation(), GetActorLocation() + FVector(0, 0, 100.f)) > Ts(TEXT("doorTorchReach"), 280.f)) continue;
			if (UWRSurvivalComponent::HoldsTorch(*It)) bTorch = true;
		}
		const float Need = FMath::Max(0.5f, Ts(TEXT("doorBurnSeconds"), 6.f));
		if (bTorch)
		{
			TorchClock += Dt;
			Burnt = FMath::Clamp(TorchClock / Need, 0.f, 1.f);
			if (FMath::Fmod(ServerClock, 1.f) < Dt) { ForceNetUpdate(); WRFx::Broadcast(this, TEXT("forest.thorn_burn"), GetActorLocation() + FVector(0, 0, 100.f), GetActorRotation(), 0.5f, TEXT("forest.thorn_burn")); }
		}
		if (Burnt >= 1.f)
		{
			State = 1;
			StateAt = Now(this);
			ForceNetUpdate();
			OnRep_State();
			WRFx::Broadcast(this, TEXT("forest.secret_door_burn"), GetActorLocation() + FVector(0, 0, 100.f), GetActorRotation(), 1.f, TEXT("forest.thorn_burn"));
			UE_LOG(LogWildreach, Log, TEXT("FOREST secret: the barred door at Bread-stone Lane burnt open"));
		}
	}
	else if (Kind == TEXT("eel_trap"))
	{
		// A catch every game day: refills on the day after it was emptied.
		if (!State && Day(this) >= int32(NextAtHours)) { State = 1; StateAt = Now(this); ForceNetUpdate(); OnRep_State(); }
	}
}

void AWRForestSecret::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority()) StepServer(DeltaSeconds);
	if (GetNetMode() == NM_DedicatedServer) return;
	if (bLookBuilt && FVector::DistSquared(LookBuiltAt, GetActorLocation()) > FMath::Square(50.f))
	{
		// The server placed it on the lane's real doorway after we drew it: draw it again there.
		LookBuiltAt = GetActorLocation();
		for (UProceduralMeshComponent* PM : { Mesh.Get(), Catch.Get(), Char.Get() }) if (PM) PM->DestroyComponent();
		if (Glow) Glow->DestroyComponent();
		bLookBuilt = false;
		BuildLook();
		OnRep_State();
	}
	if (bLookBuilt) StepLook(DeltaSeconds);
}

// ================================================================================================ AWRLostPathLook

AWRLostPathLook::AWRLostPathLook()
{
	bReplicates = false;
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRLostPathLook::Build(const TArray<FVector>& Chain)
{
	using namespace WRForestHollowImpl;
	const FData& D = Data();
	UWorld* W = GetWorld();
	const bool bDraw = GetNetMode() != NM_DedicatedServer;
	FWRToolMesh M, G;
	// The loop (DC 2.4 #16, the leshy): foxfire every few metres along a path that comes back on itself, and wisps drifting
	// round it for ever: it is the bright way, and the wrong one.
	if (const FWay* LoopWay = D.Way(TEXT("lost_path_loop")))
	{
		Loop = LoopWay->Pts;
		float Acc = 0.f;
		for (int32 i = 0; i + 1 < Loop.Num(); ++i)
		{
			const FVector A = Loop[i], B = Loop[i + 1];
			const float Len = FVector::Dist2D(A, B);
			for (float S = Acc; S < Len; S += 550.f)
			{
				const FVector P = Floor(W, FVector2D(FMath::Lerp(A, B, S / FMath::Max(1.f, Len))), A.Z);
				const FVector Dir = (B - A).GetSafeNormal2D();
				const FVector Side(-Dir.Y, Dir.X, 0.f);
				const float Sd = (int32(S) / 550) % 2 ? 1.f : -1.f;
				Lump(M, P + Side * Sd * 220.f + FVector(0, 0, 10.f), 22.f, Rgb(70, 66, 58), int32(S));
				Foxfire(G, P + Side * Sd * 200.f + FVector(0, 0, 25.f), -Side * Sd, i * 31 + int32(S));
			}
			Acc = FMath::Max(0.f, Acc - Len);
		}
		if (bDraw) for (int32 w = 0; w < 6; ++w) Wisps.Add(Lamp(this, Loop[0] + FVector(0, 0, 120.f), FoxfireLight, Ts(TEXT("wispLight"), 900.f), 700.f));
	}
	// The true way (F-S1): a Grovekin trail tree bent at the knee at every bend, its tip pointing on to the next.
	for (int32 i = 1; i + 1 < Chain.Num(); ++i)
	{
		const FVector Next = (Chain[i + 1] - Chain[i]).GetSafeNormal2D();
		const FVector Base = Floor(W, FVector2D(Chain[i]), Chain[i].Z);
		if (UStaticMesh* Mesh = WRTreeKit::Mesh(this, TEXT("trail_tree"), i % 3))
		{
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
			C->SetupAttachment(RootComponent);
			C->SetStaticMesh(Mesh);
			C->SetUsingAbsoluteLocation(true);
			C->SetUsingAbsoluteRotation(true);
			C->SetWorldLocationAndRotation(Base - FVector(0, 0, 10.f), Next.Rotation());
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->RegisterComponent();
		}
		WRTreeKit::AddCollision(this, TEXT("trail_tree"), i % 3, FTransform(Next.Rotation(), Base - FVector(0, 0, 10.f)));
	}
	// The hunter's cache's buttress hollow beyond the gap: two buttress fins framing a hollow of bark.
	FVector CacheAt; float CacheYaw = 0.f;
	if (LostCacheSpot(W, CacheAt, CacheYaw))
	{
		const FVector Back = -FRotator(0.f, CacheYaw, 0.f).Vector();
		for (const float S : { -1.f, 1.f })
		{
			if (UStaticMesh* Fin = WRTreeKit::Mesh(this, TEXT("buttress_root"), S > 0.f ? 1 : 2))
			{
				UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
				C->SetupAttachment(RootComponent);
				C->SetStaticMesh(Fin);
				C->SetUsingAbsoluteLocation(true);
				C->SetUsingAbsoluteRotation(true);
				const FVector Side(-Back.Y * S, Back.X * S, 0.f);
				C->SetWorldLocationAndRotation(CacheAt + Back * 120.f + Side * 110.f - FVector(0, 0, 10.f), (-Back + Side * 0.6f).GetSafeNormal().Rotation());
				C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				C->RegisterComponent();
			}
		}
		M.Lathe(CacheAt + Back * 160.f - FVector(0, 0, 20.f), FVector::UpVector, 260.f, { FVector(0.f, 150.f, 120.f), FVector(0.6f, 120.f, 100.f), FVector(1.f, 30.f, 30.f) }, 12, RootBark * 0.8f, 0.92f, 20.f, 5.f);
		Foxfire(G, CacheAt + Back * 40.f + FVector(0, 0, 140.f), -Back, 4321);
	}
	if (!bDraw) return;
	Draw(this, M, TEXT("LostPathLook"));
	const FLinearColor Fox(FoxfireLight);
	Draw(this, G, TEXT("LostPathFoxfire"), &Fox, Tn(TEXT("foxfireGlow"), 2.5f));
}

void AWRLostPathLook::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Loop.Num() < 2 || Wisps.Num() == 0) return;
	Clock += DeltaSeconds;
	// The wisps drift round the loop (1.2 m/s), bobbing: whichever way you walk it, they're ahead of you.
	float Total = 0.f;
	for (int32 i = 0; i + 1 < Loop.Num(); ++i) Total += FVector::Dist(Loop[i], Loop[i + 1]);
	if (Total < 1.f) return;
	for (int32 w = 0; w < Wisps.Num(); ++w)
	{
		float D = FMath::Fmod(Clock * WRForestHollowImpl::Ts(TEXT("wispSpeed"), 120.f) + Total * w / Wisps.Num(), Total);
		FVector P = Loop[0];
		for (int32 i = 0; i + 1 < Loop.Num(); ++i)
		{
			const float L = FVector::Dist(Loop[i], Loop[i + 1]);
			if (D <= L) { P = FMath::Lerp(Loop[i], Loop[i + 1], D / FMath::Max(1.f, L)); break; }
			D -= L;
		}
		if (Wisps[w]) Wisps[w]->SetWorldLocation(P + FVector(0, 0, 110.f + 25.f * FMath::Sin(Clock * 1.3f + w)));
	}
}

// ================================================================================================ spawning and the server's step

namespace WRForestHollowImpl
{
	AWRForestSecret* SpawnSecret(UWorld* W, FName Kind, const FVector& At, const FRotator& Rot, float Size)
	{
		const FTransform Tr(Rot, At);
		AWRForestSecret* S = W->SpawnActorDeferred<AWRForestSecret>(AWRForestSecret::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!S) return nullptr;
		S->Kind = Kind;
		S->Size = Size;
		S->FinishSpawning(Tr);
		if (UWRForestHazards* H = UWRForestHazards::Get(W)) H->Secrets.Add(S);
		return S;
	}

	void SpawnSleeper(UWorld* W, UWRForestHazards::FSleeper& S)
	{
		AWRCreature* C = AWRCreature::SpawnCreature(W, S.Creature, S.At, FRotator(0.f, S.Yaw, 0.f));
		if (!C)
		{
			UE_LOG(LogWildreach, Warning, TEXT("FOREST hollow: no '%s' creature row for its sleeper"), *S.Creature.ToString());
			S.BackAtHours = Hours(W) + 1.0;
			return;
		}
		C->SetHome(S.At);
		if (S.bAsleep)
		{
			// The Mossmaul Den (DC 2.5): asleep in its hollow day and night; sneak past or wake it.
			C->bHibernating = true;
			C->SetAsleep(true);
		}
		S.Body = C;
		S.BackAtHours = -1.0;
	}

	/** The four heart-trees round the Clearing of Four Hearts (the boss arena's points 0-3, else the giants of kind heart). */
	TArray<FVector> HeartTrees()
	{
		TArray<FVector> Out;
		const FWRForestData& FD = FWRForestData::Get();
		if (const FWRForestData::FArena* A = FD.Arenas.Find(TEXT("sylvaroth")))
			for (int32 i = 0; i < 4 && i < A->Points.Num(); ++i) Out.Add(A->Points[i]);
		if (Out.Num() < 4)
		{
			Out.Reset();
			for (const FWRForestData::FTree& T : FD.Giants) if (T.Kind == TEXT("heart")) Out.Add(T.Base);
		}
		return Out;
	}

	AWRForestSecret* SecretInReach(const APawn* Me, bool bLoose)
	{
		if (!Me) return nullptr;
		AWRForestSecret* Best = nullptr;
		float BestD = 1e9f;
		const FVector At = Me->GetActorLocation();
		for (TActorIterator<AWRForestSecret> It(Me->GetWorld()); It; ++It)
		{
			const bool bIn = It->InReach(At) || (bLoose && It->Kind == TEXT("heart_door") && FVector::Dist(At, It->GetActorLocation() + FVector(0, 0, 100.f)) < ReachOf(It->Kind));
			if (!bIn) continue;
			const float D = FVector::Dist(At, It->GetActorLocation());
			if (D < BestD) { BestD = D; Best = *It; }
		}
		return Best;
	}
}

void WRForestHollow::SpawnServer(UWorld* World)
{
	using namespace WRForestHollowImpl;
	if (!World || World->GetNetMode() == NM_Client) return;
	UWRForestHazards* H = UWRForestHazards::Get(World);
	if (!H) return;
	const FData& D = Data();
	const FWRForestData& FD = FWRForestData::Get();
	const float Day1 = GameHoursToSeconds(24.f);

	// ---- the Hollow Root's ways in that have state
	{
		FVector At, Out; float Width = 800.f;
		if (HeartDoor(At, Out, Width)) SpawnSecret(World, TEXT("heart_door"), At, Out.Rotation(), Width);   // way 4: one-way out
	}
	{
		FVector PlugAt; FVector2D PlugLocal; int32 PlugRoom = INDEX_NONE;
		if (PlugSpot(PlugAt, PlugLocal, PlugRoom) && D.Rooms.IsValidIndex(PlugRoom))
		{
			const FVector2D Toward = D.Rooms[PlugRoom].C - FVector2D(PlugAt);
			SpawnSecret(World, TEXT("root_plug"), PlugAt, FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(Toward.Y, Toward.X)), 0.f), 0.f);   // way 3
		}
	}
	// ---- the rooms' things
	for (int32 i = 0; i < D.Rooms.Num(); ++i)
	{
		const FRoomX& R = D.Rooms[i];
		const auto At = [&R, World](float LX, float LY, float Lift) { return Floor(World, R.World(FVector2D(LX, LY)), R.Floor) + FVector(0, 0, Lift); };
		if (R.Id == TEXT("root_hall"))
		{
			// Two Rootcoil resting along the hall between the pillar rows (DC 2.5).
			for (const float S : { -1.f, 1.f })
			{
				UWRForestHazards::FSleeper& Sl = H->Sleepers.AddDefaulted_GetRef();
				Sl.Creature = FName(*Str(TEXT("hazards.hollow.hallCreature"), TEXT("rootcoil")));
				Sl.At = At(S * R.Hx * 0.3f, S * 120.f, 60.f);
				Sl.Yaw = R.Yaw + (S > 0.f ? 0.f : 180.f);
				SpawnSleeper(World, Sl);
			}
		}
		else if (R.Id == TEXT("mossmaul_den"))
		{
			UWRForestHazards::FSleeper& Sl = H->Sleepers.AddDefaulted_GetRef();
			Sl.Creature = FName(*Str(TEXT("hazards.hollow.denCreature"), TEXT("mossmaul")));
			Sl.At = At(0.f, 0.f, 80.f);
			Sl.Yaw = R.Yaw;
			Sl.bAsleep = true;
			SpawnSleeper(World, Sl);
		}
		else if (R.Id == TEXT("weeping_roots"))
		{
			// Heartwood resin x10 (DC 2.5, 2.7: knife or hand, 2 a node), one in each basin; they weep full again in a day.
			for (int32 b = 0; b < 10; ++b)
			{
				const float A = 2.f * PI * b / 10.f;
				Node(World, FName(*Str(TEXT("hazards.hollow.resinItem"), TEXT("resin"))), At(FMath::Cos(A) * R.Hx * 0.68f, FMath::Sin(A) * R.Hy * 0.68f, 40.f), 1, int32(Tn(TEXT("resinPerNode"), 2.f)), Day1);
			}
		}
		else if (R.Id == TEXT("bootlace_gallery"))
		{
			// Bootlace rhizomorph x6 (DC 2.5, 2.7: a blade, 2 a node): the Bootlace blight's source.
			const FName Lace(*Str(TEXT("hazards.hollow.laceItem"), TEXT("bootlace_rhizomorph")));
			for (int32 k = 0; k < 6; ++k)
			{
				AWRResourceNode* N = Node(World, Lace, At(-R.Hx * 0.65f + R.Hx * 1.3f * k / 5.f, (k % 2 ? 1.f : -1.f) * R.Hy * 0.3f, 20.f), 1, int32(Tn(TEXT("lacePerNode"), 2.f)), Day1);
				if (!N) continue;
				N->CanHarvest = [](AWRCharacterBase* Who)
				{
					if (WRForestHollowImpl::HoldsBlade(Who)) return true;
					WRForestHollowImpl::Tell(Who, TEXT("The laces are tough as rope: cut them with a blade"));
					return false;
				};
			}
		}
		else if (R.Id == TEXT("iron_seam"))
		{
			// Iron x4 and a gem chance in the broken wall (DC 2.5).
			for (int32 k = 0; k < 4; ++k)
				Node(World, FName(*Str(TEXT("hazards.hollow.seamItem"), TEXT("iron_ore"))), At(R.Hx - 260.f, -R.Hy * 0.6f + R.Hy * 1.2f * k / 3.f, 30.f), int32(Tn(TEXT("seamHits"), 6.f)), 2, GameHoursToSeconds(Tn(TEXT("seamRespawnHours"), 72.f)), Tn(TEXT("seamGemChance"), 0.15f));
		}
		else if (R.Id == TEXT("thrumming_root"))
		{
			// Its face toward the way in (the room before it), where a hand goes.
			const int32 Prev = i - 1;
			const float S = D.Rooms.IsValidIndex(Prev) && R.Local(D.Rooms[Prev].C).Y < 0.f ? -1.f : 1.f;
			const FVector P = At(0.f, S * (Tn(TEXT("thrumRadius"), 300.f) + 90.f), 0.f);
			SpawnSecret(World, TEXT("thrumming_root"), P, FRotator(0.f, R.Yaw + (S > 0.f ? -90.f : 90.f), 0.f), 0.f);
		}
		else if (R.Id == TEXT("seed_store"))
		{
			SpawnSecret(World, TEXT("seed_store"), At(R.Hx - 130.f, 0.f, 0.f), FRotator(0.f, R.Yaw + 180.f, 0.f), 0.f);
		}
		else if (R.Id == TEXT("grove_of_bones"))
		{
			// The giant's hand round the heartwood spear (AWRHollowRoot draws the bones and the spear).
			SpawnSecret(World, TEXT("bone_spear"), At(30.f, 310.f, 0.f), FRotator(0.f, R.Yaw, 0.f), 0.f);
		}
	}

	// ---- F-S1 the Lost Path: the true chain, its wall (the generator's, Guards lost_path_end) and the hunter's cache.
	if (const FWay* True = D.Way(TEXT("lost_path_true")))
	{
		H->LostChain = True->Pts;
		for (const TWeakObjectPtr<AWRThornbrake>& B : H->Brakes) if (B.IsValid() && B->Guards == TEXT("lost_path_end")) H->LostBrake = B;
		FVector At; float Yaw = 0.f;
		if (LostCacheSpot(World, At, Yaw))
		{
			TMap<FName, int32> Items;
			AddIfItem(Items, TEXT("hazards.secrets.lostBlueprint"), TEXT("bp_heartwood_longbow"), 1);
			AddIfItem(Items, TEXT("hazards.secrets.lostArrows"), TEXT("thorned_arrow"), int32(Ts(TEXT("lostArrowCount"), 30.f)));
			AddIfItem(Items, TEXT("hazards.secrets.lostGem"), TEXT("gem_topaz"), 1);
			if (AWRWorldCache* C = Cache(World, At + FVector(0, 0, 10.f), Yaw, TEXT("A Grovekin hunter's cache"), Items, TEXT("Wrapped in oiled bark in the hollow of the roots: the kit of someone who knew the way."), Ts(TEXT("cacheRefillHours"), 72.f)))
				H->AddGuardedCache(C, TEXT("lost_path_end"));
		}
	}

	// ---- F-S4 the barred door: an estimate from the lane's layout until the server finds the real doorway (its Tick).
	if (const FWRForestData::FSite* Lane = FD.FindSite(TEXT("bread_stone_lane")))
	{
		const FRotator Rot(0.f, Lane->Yaw, 0.f);
		const FVector Est = Lane->At + Rot.RotateVector(FVector(Ts(TEXT("doorLocalX"), 770.f), Ts(TEXT("doorLocalY"), 620.f), 0.f));
		float Z = Est.Z;
		GroundZ(World, FVector2D(Est), Est.Z, Z);
		SpawnSecret(World, TEXT("barred_door"), FVector(Est.X, Est.Y, Z), FRotator(0.f, Lane->Yaw - 90.f, 0.f), 0.f);
	}
	// ---- F-S5 the bell: a shelf of root under the bell, 40 m up the Bell-Tree.
	{
		FVector Bell, Base; FRotator Rot; float R = 300.f, Scale = 1.f, Height = 7000.f; int32 Var = 0;
		if (BellSpot(Bell, Base, Rot, R, Scale, Var, Height))
		{
			const FVector Out = (Bell - Base).GetSafeNormal2D().IsNearlyZero() ? Rot.Vector() : (Bell - Base).GetSafeNormal2D();
			FVector C; float Rr = R;
			TrunkAt(Base, Rot, Scale, Var, Bell.Z, R, C, Rr);
			const FVector Land = FVector(C.X, C.Y, Bell.Z - Ts(TEXT("bellUnder"), 420.f)) + Out * (Rr + Ts(TEXT("bellShelfOut"), 250.f));
			SpawnSecret(World, TEXT("bell"), Land, Out.Rotation(), 0.f);
		}
	}
	// ---- the eel trap at Otter Steps (DC 2.4 #14): in the pool under the steps' last drop.
	if (const FWRForestData::FSite* Eel = FD.FindSite(TEXT("eel_trap")))
		SpawnSecret(World, TEXT("eel_trap"), Eel->At, FRotator(0.f, Eel->Yaw, 0.f), 0.f);
	// ---- F-S2 the grotto's back room: beside the Alder Grotto's mouth under the falls' overhang (the cliff behind the
	// falls is solid land, so the room is built against it, its squeeze facing the grotto).
	if (const FWRForestData::FSite* Grotto = FD.FindSite(TEXT("alder_grotto")))
	{
		const FRotator Rot(0.f, Grotto->Yaw, 0.f);
		const FVector P = Grotto->At + Rot.RotateVector(FVector(Ts(TEXT("grottoLocalX"), -200.f), Ts(TEXT("grottoLocalY"), 1100.f), 0.f));
		float Z = P.Z;
		GroundZ(World, FVector2D(P), P.Z, Z);
		SpawnSecret(World, TEXT("grotto_shrine"), FVector(P.X, P.Y, Z), FRotator(0.f, Grotto->Yaw + 180.f, 0.f), 0.f);
	}
	UE_LOG(LogWildreach, Log, TEXT("FOREST hollow + secrets: %d secrets, %d sleepers, lost path %d points"), H->Secrets.Num(), H->Sleepers.Num(), H->LostChain.Num());
}

void WRForestHollow::BuildLocal(UWorld* World)
{
	using namespace WRForestHollowImpl;
	if (!World) return;
	for (TActorIterator<AWRHollowRoot> It(World); It; ++It) return;   // once a world
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (Data().Rooms.Num() > 0)
	{
		if (AWRHollowRoot* Root = World->SpawnActor<AWRHollowRoot>(AWRHollowRoot::StaticClass(), FTransform::Identity, P)) Root->Build();
	}
	if (const FWay* True = Data().Way(TEXT("lost_path_true")))
	{
		if (AWRLostPathLook* Look = World->SpawnActor<AWRLostPathLook>(AWRLostPathLook::StaticClass(), FTransform::Identity, P)) Look->Build(True->Pts);
	}
}

void WRForestHollow::StepServer(UWorld* World, float Dt)
{
	using namespace WRForestHollowImpl;
	UWRForestHazards* H = UWRForestHazards::Get(World);
	if (!H || World->GetNetMode() == NM_Client) return;
	static float Clock = 0.f;
	Clock += Dt;
	if (Clock < 0.5f) return;
	Clock = 0.f;
	const double Hrs = Hours(World);
	// The Hollow's residents come back a while after they die (never in front of someone).
	for (UWRForestHazards::FSleeper& S : H->Sleepers)
	{
		AWRCreature* C = S.Body.Get();
		if (C && C->IsAlive()) continue;
		if (S.BackAtHours < 0.0) { S.BackAtHours = Hrs + Tn(TEXT("sleeperBackHours"), 24.f); continue; }
		if (Hrs < S.BackAtHours) continue;
		bool bSeen = false;
		for (TActorIterator<AWRCharacter> It(World); It && !bSeen; ++It) bSeen = It->IsAlive() && FVector::Dist(It->GetActorLocation(), S.At) < Tn(TEXT("sleeperClear"), 4000.f);
		if (!bSeen) SpawnSleeper(World, S);
	}
	// F-S1: who is walking the true way. Each bend reached in order counts; leave the chain for the foxfire loop and you
	// start again. Through the last bend to the gap: the secret is theirs (Rowan's "The bent trees").
	const TArray<FVector>& Chain = H->LostChain;
	if (Chain.Num() < 3) return;
	const FWay* Loop = Data().Way(TEXT("lost_path_loop"));
	const double T = Now(World);
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		AWRPlayerController* PC = Cast<AWRPlayerController>(It->Get());
		const AWRCharacter* Me = PC ? Cast<AWRCharacter>(PC->GetPawn()) : nullptr;
		AWRPlayerState* PS = StateOf(PC);
		if (!Me || !PS || !Me->IsAlive()) continue;
		const FVector P = Me->GetActorLocation();
		const FString Name = PS->GetPlayerName();
		if (FVector::Dist2D(P, Chain[0]) > Ts(TEXT("lostRange"), 40000.f)) { H->LostWalkers.Remove(Name); continue; }
		UWRForestHazards::FLostWalker& Wk = H->LostWalkers.FindOrAdd(Name);
		if (Wk.bThrough) continue;
		const float Hit = Ts(TEXT("lostBendRadius"), 1000.f);
		if (Wk.Stage + 1 < Chain.Num() && FVector::Dist2D(P, Chain[Wk.Stage + 1]) < Hit)
		{
			++Wk.Stage;
			if (Wk.Stage + 1 < Chain.Num())
			{
				PC->ClientNotify(FString::Printf(TEXT("The trail tree's knee points on, away from the foxfire (%d of %d)"), Wk.Stage, Chain.Num() - 2));
			}
			else
			{
				Wk.bThrough = true;
				PC->ClientNotify(TEXT("Past the last bend: a gap in the brambles, and a hollow in the roots beyond"));
				if (UWRWorldSystems* WS = UWRWorldSystems::Get(World)) WS->QuestEvent(PS, TEXT("forest_lost_path"), NAME_None);
				UE_LOG(LogWildreach, Log, TEXT("FOREST lost path: %s followed the bends through"), *Name);
			}
			continue;
		}
		// Off the true way and back on the loop: the foxfire has you again.
		if (Wk.Stage > 0 && Loop)
		{
			const float Off = WRLand::DistToSegment(FVector2D(P), FVector2D(Chain[Wk.Stage - 1]), FVector2D(Chain[Wk.Stage]));
			const float Ahead = WRLand::DistToSegment(FVector2D(P), FVector2D(Chain[Wk.Stage]), FVector2D(Chain[FMath::Min(Wk.Stage + 1, Chain.Num() - 1)]));
			TArray<FVector2D> L2;
			for (const FVector& V : Loop->Pts) L2.Add(FVector2D(V));
			const float OnLoop = WRLand::DistToPolyline(FVector2D(P), L2);
			if (FMath::Min(Off, Ahead) > Ts(TEXT("lostStray"), 2500.f) && OnLoop < 1500.f)
			{
				Wk.Stage = 0;
				if (T - Wk.LastTold > 30.0) { Wk.LastTold = T; PC->ClientNotify(TEXT("The foxfire has led you round again")); }
			}
		}
	}
}

// ================================================================================================ interaction

bool WRForestHollow::Prompt(const AWRPlayerController* PC, FString& OutTarget, FString& OutVerb)
{
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	const AWRForestSecret* S = WRForestHollowImpl::SecretInReach(Me, true);
	if (!S) return false;
	const FString Verb = S->PromptVerb(Me, OutTarget);
	if (Verb.IsEmpty()) return false;
	OutVerb = Verb;
	return true;
}

bool WRForestHollow::ClientInteract(AWRPlayerController* PC)
{
	using namespace WRForestHollowImpl;
	APawn* Me = PC ? PC->GetPawn() : nullptr;
	AWRForestSecret* S = SecretInReach(Me, true);
	if (!S) return false;
	FString Target;
	if (S->PromptVerb(Me, Target).IsEmpty()) return false;
	if (S->Kind == TEXT("heart_door") && !S->InReach(Me->GetActorLocation()))
	{
		PC->ShowLocalNotice(TEXT("The roots are grown shut from this side"));
		return true;
	}
	if (S->Kind == TEXT("thrumming_root"))
	{
		// The vision (DC 2.5): for three seconds the map shows the four heart-trees the root runs to. Here at once; the
		// lore item is the server's.
		PC->SetMenu(EWRMenu::Map);
		WRUIMap::Flash(HeartTrees(), TEXT("A heart-tree"), Tn(TEXT("visionSeconds"), 3.f));
		WRSound::PlayAt(PC, TEXT("forest.root_pulse"), Me->GetActorLocation());
	}
	const FVector L = S->GetActorLocation();
	PC->SendCommand(FString::Printf(TEXT("world fhaz secret %.1f %.1f %.1f"), L.X, L.Y, L.Z));
	return true;
}

FString WRForestHollow::Command(AWRPlayerController* PC, const TArray<FString>& Args)
{
	// "world fhaz secret x y z"
	APawn* Me = PC ? PC->GetPawn() : nullptr;
	if (!Me || Args.Num() < 6) return FString();
	const FVector At(FCString::Atof(*Args[3]), FCString::Atof(*Args[4]), FCString::Atof(*Args[5]));
	AWRForestSecret* Best = nullptr;
	float BestD = 300.f;
	for (TActorIterator<AWRForestSecret> It(Me->GetWorld()); It; ++It)
	{
		const float D = FVector::Dist(It->GetActorLocation(), At);
		if (D < BestD) { BestD = D; Best = *It; }
	}
	if (!Best) return TEXT("Nothing here");
	if (!Best->InReach(Me->GetActorLocation(), 200.f))
	{
		return Best->Kind == TEXT("heart_door") ? TEXT("The roots are grown shut from this side") : TEXT("Too far away");
	}
	return Best->Use(PC);
}
