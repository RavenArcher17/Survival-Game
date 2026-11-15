// Storm peaks on the map (cooks/biome-lightning.md 2.2-2.8, cooks/build-biome-lightning.md 2.4-2.5). See WRStormWorld.h.
// Everything here is placed from Data/storm_peaks.json (Tools/terrain/storm_peaks.mjs) and every number is tuning
// (storm.*): nothing in this file decides where a thing goes, only how it is built and how it behaves.

#include "WRStormWorld.h"
#include "Wildreach.h"
#include "WRBuildLook.h"
#include "WRBuildLookKit.h"
#include "WRCharacter.h"
#include "WRCharacterBase.h"
#include "WRCombatLibrary.h"
#include "WRCombatTypes.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRGameState.h"
#include "WRGameplayTags.h"
#include "WRInventory.h"
#include "WRPathComponent.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRProjectile.h"
#include "WRResourceNode.h"
#include "WRSound.h"
#include "WRSurvival.h"
#include "WRTuning.h"
#include "WRWorldDressing.h"
#include "WRWorldSystems.h"
#include "WRBuilding.h"
#include "WRSocial.h"
#include "WRFarm.h"
#include "AbilitySystemComponent.h"
#include "Components/BoxComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "GameFramework/GameStateBase.h"

namespace WRStormLook
{
	using WRLookKit::FSurf;
	using WRLookKit::FKit;

	// The Storm palette (DC 2.1): slate, granite, storm violet-white, verdigris, lichen ochre, rye gold; the
	// Rodwardens' copper, bronze, whitewash and timber (DC 2.2 S5).
	const FLinearColor Slate = FLinearColor(FColor(62, 70, 80));
	const FLinearColor Granite = FLinearColor(FColor(138, 141, 145));
	const FLinearColor Violet = FLinearColor(FColor(185, 167, 255));
	const FLinearColor Verdigris = FLinearColor(FColor(78, 154, 132));
	const FLinearColor Ochre = FLinearColor(FColor(183, 154, 74));
	const FLinearColor Rye = FLinearColor(FColor(201, 178, 106));
	const FLinearColor Copper = FLinearColor(FColor(184, 115, 51));
	const FLinearColor Bronze = FLinearColor(FColor(140, 106, 58));
	const FLinearColor Whitewash = FLinearColor(FColor(217, 213, 204));
	const FLinearColor Timber = FLinearColor(FColor(90, 70, 54));
	const FLinearColor RopeTan = FLinearColor(FColor(156, 132, 96));
	const FLinearColor Rust = FLinearColor(FColor(154, 74, 36));
	const FLinearColor BoneWhite = FLinearColor(FColor(216, 210, 196));
	const FLinearColor Char = FLinearColor(0.012f, 0.011f, 0.01f);

	FSurf Stone(const FLinearColor& C, float Rough = 0.9f) { return FSurf::Stone(C, Rough); }
	FSurf Green() { FSurf S = FSurf::Steel(Verdigris * 0.55f, 0.62f); S.Metal = 0.45f; S.Pattern = WRLookPattern::Mottle; return S; }
	FSurf CopperBright() { return FSurf::Steel(Copper * 0.75f, 0.38f); }
	FSurf BronzeS() { return FSurf::Steel(Bronze * 0.6f, 0.5f); }
	FSurf Iron() { return FSurf::Steel(FLinearColor(0.035f, 0.033f, 0.032f), 0.65f); }
	FSurf Wood(float K = 1.f) { return FSurf::Wood(Timber * 0.45f * K); }
	FSurf Rope() { return FSurf::Cloth(RopeTan * 0.5f); }
	FSurf Glass(float Glow = 0.f) { return FSurf::Glass(FLinearColor(0.42f, 0.46f, 0.55f), Glow); }
	FSurf Lime() { return FSurf::Stone(Whitewash * 0.85f, 0.94f); }
	FSurf BoneS() { return FSurf::Stone(BoneWhite * 0.7f, 0.8f); }

	UStaticMesh* RockMesh(int32 i)
	{
		static const TCHAR* Names[] = { TEXT("01A"), TEXT("02A"), TEXT("05A"), TEXT("08A"), TEXT("09A"), TEXT("12A") };
		const TCHAR* N = Names[((i % 6) + 6) % 6];
		return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/SimpleRocks/3D_Assets/Meshes/Rocks/SM_SimpleRock_%s.SM_SimpleRock_%s"), N, N));
	}

	UMaterialInstanceDynamic* RockMat(UObject* Outer, const FLinearColor& Tint)
	{
		UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_Rock.M_Rock"));
		if (!Base) return nullptr;
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, Outer);
		M->SetVectorParameterValue(TEXT("Tint"), Tint);
		return M;
	}

	/** A rock of the set fitted to Size (cm), its bottom at Base (local), sunk Sink cm. */
	UStaticMeshComponent* Rock(AActor* Owner, int32 Which, const FVector& Size, const FVector& Base, const FRotator& Rot, UMaterialInterface* Mat, bool bCollide, float Sink = 10.f)
	{
		UStaticMesh* Mesh = RockMesh(Which);
		if (!Mesh) return nullptr;
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner);
		C->SetStaticMesh(Mesh);
		C->SetupAttachment(Owner->GetRootComponent());
		const FBox B = Mesh->GetBoundingBox();
		const FVector S = B.GetSize();
		const FVector Scale(Size.X / FMath::Max(1.f, S.X), Size.Y / FMath::Max(1.f, S.Y), Size.Z / FMath::Max(1.f, S.Z));
		C->SetRelativeScale3D(Scale);
		C->SetRelativeRotation(Rot);
		C->SetRelativeLocation(Base + Rot.RotateVector(FVector(-B.GetCenter().X * Scale.X, -B.GetCenter().Y * Scale.Y, -B.Min.Z * Scale.Z - Sink)));
		if (Mat) C->SetMaterial(0, Mat);
		C->SetCollisionProfileName(bCollide ? TEXT("BlockAll") : TEXT("NoCollision"));
		C->SetCanEverAffectNavigation(false);
		C->SetCullDistance(WRTune(TEXT("storm.sceneryCull"), 60000.f));
		C->RegisterComponent();
		return C;
	}

	UBoxComponent* Block(AActor* Owner, const FVector& Centre, const FVector& Half, const FRotator& Rot = FRotator::ZeroRotator)
	{
		UBoxComponent* B = NewObject<UBoxComponent>(Owner);
		B->SetupAttachment(Owner->GetRootComponent());
		B->SetBoxExtent(Half);
		B->SetRelativeLocation(Centre);
		B->SetRelativeRotation(Rot);
		B->SetCollisionProfileName(TEXT("BlockAll"));
		B->SetCanEverAffectNavigation(false);
		B->RegisterComponent();
		return B;
	}

	UPointLightComponent* Light(AActor* Owner, const FVector& At, const FLinearColor& Colour, float Intensity, float Radius)
	{
		UPointLightComponent* L = NewObject<UPointLightComponent>(Owner);
		L->SetupAttachment(Owner->GetRootComponent());
		L->SetRelativeLocation(At);
		L->SetLightColor(Colour);
		L->SetIntensity(Intensity);
		L->SetAttenuationRadius(Radius);
		L->SetCastShadows(false);
		L->RegisterComponent();
		return L;
	}

	/** The ground under a local XY of an actor, as a local Z (pieces sit on uneven ground). */
	float GroundZ(const AActor* Owner, const FVector2D& Local, float Fallback = 0.f)
	{
		const FTransform T = Owner->GetActorTransform();
		const FVector W = T.TransformPosition(FVector(Local, 0.f));
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRStormGround), false, Owner);
		if (Owner->GetWorld()->LineTraceSingleByChannel(Hit, W + FVector(0, 0, 3000), W - FVector(0, 0, 6000), ECC_WorldStatic, Q) && !Cast<APawn>(Hit.GetActor()))
		{
			return T.InverseTransformPosition(Hit.ImpactPoint).Z;
		}
		return Fallback;
	}

	/** A copper rod: a tapered shaft in green-copper, a cast collar, the tip (bright where strikes have scoured it). */
	void RodLook(FKit& K, const FVector& Base, float H, float Diam = 9.f)
	{
		K.Rod(Green(), Base, Base + FVector(0, 0, H * 0.55f), Diam);
		K.Rod(Green(), Base + FVector(0, 0, H * 0.55f), Base + FVector(0, 0, H * 0.97f), Diam * 0.7f);
		K.Disc(BronzeS(), Base + FVector(0, 0, 12.f), FVector::UpVector, Diam * 2.6f, 18.f);
		K.Spike(CopperBright(), Base + FVector(0, 0, H * 0.97f), FVector::UpVector, Diam * 0.7f, H * 0.05f + 10.f);
	}

	/** The thunder mark: a six-petal rosette cut into a lintel (Slavic gromovnik), in the plane facing Normal. */
	void ThunderMark(FKit& K, const FVector& Centre, const FVector& Normal, float R)
	{
		const FRotator Face = Normal.Rotation();
		for (int32 i = 0; i < 6; ++i)
		{
			const float A = 60.f * i;
			const FVector Dir = Face.RotateVector(FRotator(0.f, 0.f, A).RotateVector(FVector(0, 0, 1)));
			K.Add(Stone(Slate * 0.3f), FVector(1.5f, R * 0.32f, R * 0.95f), Centre + Dir * (R * 0.48f), FRotator(Face.Pitch, Face.Yaw, A));
		}
		K.Disc(Stone(Slate * 0.3f), Centre, Normal, R * 2.05f, 1.5f);
	}

	/** A Lichtenberg fern burned into stone: branches forking out from Centre across the surface (X, Y local). */
	void Fern(FKit& K, const FVector& Centre, float Reach, int32 Seed, float Glow = 0.f)
	{
		struct FB { FVector2D P; float A; float L; int32 D; };
		TArray<FB> Stack;
		for (int32 i = 0; i < 5; ++i) Stack.Add({ FVector2D(Centre), 72.f * i + Seed * 13.f, Reach * 0.32f, 0 });
		int32 Guard = 0;
		const FSurf Burn = Glow > 0.f ? FSurf::Lit(Violet * 0.5f, Glow) : Stone(Char, 0.98f);
		while (Stack.Num() && ++Guard < 160)
		{
			const FB B = Stack.Pop();
			const FVector2D Dir(FMath::Cos(FMath::DegreesToRadians(B.A)), FMath::Sin(FMath::DegreesToRadians(B.A)));
			const FVector2D End = B.P + Dir * B.L;
			K.Add(Burn, FVector(B.L, FMath::Max(1.2f, 7.f - 2.f * B.D), 1.2f), FVector((B.P + End) * 0.5f, Centre.Z + 0.6f), FRotator(0.f, B.A, 0.f));
			if (B.D < 4)
			{
				const float J = FMath::Frac(FMath::Sin(B.A * 12.9898f + Seed) * 43758.5453f);
				Stack.Add({ End, B.A + 18.f + 22.f * J, B.L * 0.68f, B.D + 1 });
				Stack.Add({ End, B.A - 18.f - 22.f * (1.f - J), B.L * 0.62f, B.D + 1 });
			}
		}
	}

	/** One whitewashed Rodwarden cell: four walls with a door and a window, a slate roof on timber, a balcony out over
	 *  the drop on two beams socketed into the rock. Local frame: door faces +X. */
	void Cell(FKit& K, const FVector& Base, float Yaw, bool bRoof, float W = 500.f, float D = 420.f, float H = 340.f)
	{
		const FRotator R(0.f, Yaw, 0.f);
		const auto P = [&](float X, float Y, float Z) { return Base + R.RotateVector(FVector(X, Y, Z)); };
		const FSurf Wall = Lime();
		K.Add(Stone(Slate, 0.92f), FVector(D + 40.f, W + 40.f, 40.f), P(0, 0, -10.f), R);                                     // footing
		K.Add(Wall, FVector(30.f, W, H), P(-D * 0.5f, 0, H * 0.5f), R);                                                         // back
		K.Add(Wall, FVector(D, 30.f, H), P(0, -W * 0.5f, H * 0.5f), R);
		K.Add(Wall, FVector(D, 30.f, H), P(0, W * 0.5f, H * 0.5f), R);
		K.Add(Wall, FVector(30.f, W * 0.5f - 60.f, H), P(D * 0.5f, -W * 0.25f - 30.f, H * 0.5f), R);                           // front, door gap
		K.Add(Wall, FVector(30.f, W * 0.5f - 60.f, H), P(D * 0.5f, W * 0.25f + 30.f, H * 0.5f), R);
		K.Add(Wall, FVector(30.f, 120.f, H - 230.f), P(D * 0.5f, 0, 230.f + (H - 230.f) * 0.5f), R);                          // over the door
		K.Add(Stone(Granite * 0.8f), FVector(40.f, 170.f, 26.f), P(D * 0.5f + 6.f, 0, 236.f), R);                             // lintel
		ThunderMark(K, P(D * 0.5f + 20.f, 0, 236.f), R.Vector(), 9.f);
		K.Add(Wood(0.8f), FVector(8.f, 110.f, 220.f), P(D * 0.5f + 2.f, 0, 110.f), FRotator(0.f, Yaw + 25.f, 0.f));           // the door, ajar
		K.Add(Stone(Char, 0.98f), FVector(34.f, 60.f, 70.f), P(0, W * 0.5f, 190.f), R);                                       // window
		if (bRoof)
		{
			for (int32 i = 0; i < 5; ++i) K.Add(Wood(), FVector(D + 120.f, 16.f, 16.f), P(20.f, -W * 0.5f + W * i / 4.f, H + 8.f), FRotator(-8.f, Yaw, 0.f));
			for (int32 r = 0; r < 6; ++r)
			{
				K.Add(Stone(Slate * 0.75f, 0.85f), FVector((D + 140.f) / 6.f + 6.f, W + 80.f, 5.f), P(-D * 0.5f - 40.f + (D + 140.f) * (r + 0.5f) / 6.f, 0, H + 28.f - (r * (D + 140.f) / 6.f) * 0.14f), FRotator(-8.f, Yaw, 0.f));
			}
		}
		// The balcony: two beams out over the drop, a plank floor and a rail of copper chain.
		for (const float Y : { -W * 0.35f, W * 0.35f }) K.Add(Wood(0.9f), FVector(260.f, 20.f, 22.f), P(D * 0.5f + 110.f, Y, -12.f), R);
		for (int32 i = 0; i < 9; ++i) K.Add(Wood(1.1f), FVector(26.f, W * 0.9f, 5.f), P(D * 0.5f + 20.f + i * 26.f, 0, 2.f), R);
		K.Rod(Green(), P(D * 0.5f + 240.f, -W * 0.45f, 100.f), P(D * 0.5f + 240.f, W * 0.45f, 100.f), 3.f);
		for (const float Y : { -W * 0.45f, 0.f, W * 0.45f }) K.Rod(Green(), P(D * 0.5f + 240.f, Y, 0.f), P(D * 0.5f + 240.f, Y, 104.f), 4.f);
	}

	/** A one-room dry-stone hut with a thunder-marked lintel and a thunderstone niche (the waystation kit). */
	void Hut(FKit& K, AActor* Owner, const FVector& Base, float Yaw, int32 Var)
	{
		const FRotator R(0.f, Yaw, 0.f);
		const auto P = [&](float X, float Y, float Z) { return Base + R.RotateVector(FVector(X, Y, Z)); };
		const FSurf Dry = Stone(Slate * (0.95f + 0.1f * Var), 0.95f);
		for (int32 c = 0; c < 7; ++c)    // seven courses, each a ring of rough blocks, the corners bonded
		{
			const float Z = 30.f + c * 38.f;
			for (int32 s = 0; s < 4; ++s)
			{
				const FRotator Side(0.f, Yaw + 90.f * s, 0.f);
				const FVector Mid = P(0, 0, Z) + Side.RotateVector(FVector(220.f, 0, 0));
				if (s == 0 && c < 5) { K.Add(Dry, FVector(55.f, 120.f, 36.f), Mid + Side.RotateVector(FVector(0, -150.f, 0)), Side); K.Add(Dry, FVector(55.f, 120.f, 36.f), Mid + Side.RotateVector(FVector(0, 150.f, 0)), Side); continue; }
				K.Add(Dry.Dim(0.9f + 0.2f * K.Rand(Mid, c)), FVector(55.f, 470.f + (c % 2) * 30.f, 36.f), Mid, Side);
			}
		}
		K.Add(Stone(Granite * 0.8f), FVector(60.f, 220.f, 34.f), P(222.f, 0, 228.f), R);
		ThunderMark(K, P(254.f, 0, 228.f), R.Vector(), 12.f);
		// Corbelled roof of slabs closing in, turfed.
		for (int32 r = 0; r < 4; ++r) K.Add(Stone(Slate * 0.7f), FVector(480.f - r * 110.f, 480.f - r * 110.f, 24.f), P(0, 0, 300.f + r * 22.f), R);
		K.Add(FSurf::Cloth(FLinearColor(0.07f, 0.09f, 0.05f)), FVector(160.f, 160.f, 30.f), P(0, 0, 395.f), R);
		// Inside: the niche in the back wall with its thunderstone (an old polished axe-head), a hearth stone.
		K.Add(Stone(Char), FVector(30.f, 50.f, 40.f), P(-200.f, 0, 120.f), R);
		K.Add(Stone(FLinearColor(0.05f, 0.05f, 0.06f), 0.3f), FVector(18.f, 10.f, 26.f), P(-198.f, 0, 116.f), FRotator(0.f, Yaw, 20.f));
		K.Add(Stone(Slate * 0.6f), FVector(120.f, 120.f, 10.f), P(-40.f, 0, 4.f), R);
		Block(Owner, P(0, 0, 170.f) - Owner->GetActorLocation() + Owner->GetActorLocation(), FVector(10.f), R);   // keeps the actor's bounds sane
	}
}
using namespace WRStormLook;

// ================================================================================================ data

const FWRStormData& FWRStormData::Get()
{
	static FWRStormData D;
	static bool bTried = false;
	if (bTried) return D;
	bTried = true;
	const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("storm_peaks.json"));
	if (!Root.IsValid()) return D;
	const auto Num = [](const TSharedPtr<FJsonObject>& O, const TCHAR* K, double F = 0.0) { double V = F; if (O.IsValid()) O->TryGetNumberField(K, V); return V; };
	const auto V3 = [](const TArray<TSharedPtr<FJsonValue>>& A) { return FVector(A.Num() > 0 ? A[0]->AsNumber() * 100.0 : 0.0, A.Num() > 1 ? A[1]->AsNumber() * 100.0 : 0.0, A.Num() > 2 ? A[2]->AsNumber() * 100.0 : 0.0); };
	const TSharedPtr<FJsonObject>* Frame = nullptr;
	if (Root->TryGetObjectField(TEXT("frame"), Frame))
	{
		const double X0 = Num(*Frame, TEXT("x0")), Y0 = Num(*Frame, TEXT("y0")), Step = Num(*Frame, TEXT("step"), 2.0), N = Num(*Frame, TEXT("samples"), 2017.0);
		D.FrameMin = FVector2D(X0 * 100.0, Y0 * 100.0);
		D.FrameMax = FVector2D((X0 + (N - 1) * Step) * 100.0, (Y0 + (N - 1) * Step) * 100.0);
	}
	const TSharedPtr<FJsonObject>* Zones = nullptr;
	if (Root->TryGetObjectField(TEXT("zones"), Zones))
	{
		D.ZoneCell = Num(*Zones, TEXT("cell"), 32.0) * 100.0;
		D.ZoneOrigin = FVector2D(Num(*Zones, TEXT("x0")) * 100.0, Num(*Zones, TEXT("y0")) * 100.0);
		D.ZoneW = (int32)Num(*Zones, TEXT("width"));
		D.ZoneH = (int32)Num(*Zones, TEXT("height"));
		(*Zones)->TryGetStringArrayField(TEXT("rows"), D.ZoneRows);
	}
	const auto ReadSites = [&](const TCHAR* Field, TArray<FSite>& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (!Root->TryGetArrayField(Field, Arr)) return;
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			if (!O.IsValid()) continue;
			FSite S;
			FString Str;
			if (O->TryGetStringField(TEXT("id"), Str)) S.Id = FName(*Str);
			if (O->TryGetStringField(TEXT("kind"), Str)) S.Kind = FName(*Str);
			O->TryGetStringField(TEXT("name"), S.Name);
			S.At = FVector(Num(O, TEXT("x")) * 100.0, Num(O, TEXT("y")) * 100.0, Num(O, TEXT("z")) * 100.0);
			S.Yaw = Num(O, TEXT("yaw"));
			S.Variant = (int32)Num(O, TEXT("variant"));
			O->TryGetBoolField(TEXT("landmark"), S.bLandmark);
			if (O->TryGetStringField(TEXT("cave"), Str)) S.Cave = FName(*Str);
			const TSharedPtr<FJsonObject>* Loot = nullptr;
			if (O->TryGetObjectField(TEXT("loot"), Loot))
			{
				for (const auto& KV : (*Loot)->Values) S.Loot.Add(FName(*KV.Key), (int32)KV.Value->AsNumber());
			}
			const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
			if (O->TryGetArrayField(TEXT("from"), A)) S.Extra = V3(*A);
			if (O->TryGetArrayField(TEXT("to"), A)) S.Extra2 = V3(*A);
			if (O->TryGetArrayField(TEXT("lip"), A)) S.Extra = V3(*A);
			if (O->TryGetArrayField(TEXT("wreck"), A)) S.Extra = V3(*A);
			Out.Add(S);
		}
	};
	ReadSites(TEXT("sites"), D.Sites);
	ReadSites(TEXT("kit"), D.Kit);
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (Root->TryGetArrayField(TEXT("rods"), Arr))
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			FRod R;
			R.Base = FVector(Num(O, TEXT("x")) * 100.0, Num(O, TEXT("y")) * 100.0, Num(O, TEXT("z")) * 100.0);
			R.Height = Num(O, TEXT("h"), 7.0) * 100.0;
			FString K; if (O->TryGetStringField(TEXT("kind"), K)) R.Kind = FName(*K);
			if (O->TryGetStringField(TEXT("route"), K)) R.Route = FName(*K);
			D.Rods.Add(R);
		}
	}
	if (Root->TryGetArrayField(TEXT("bridges"), Arr))
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			FBridge B;
			FString S; if (O->TryGetStringField(TEXT("id"), S)) B.Id = FName(*S);
			O->TryGetStringField(TEXT("name"), B.Name);
			const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
			if (O->TryGetArrayField(TEXT("from"), A)) B.From = V3(*A);
			if (O->TryGetArrayField(TEXT("to"), A)) B.To = V3(*A);
			B.Spans = (int32)Num(O, TEXT("spans"), 1.0);
			if (O->TryGetArrayField(TEXT("broken"), A)) for (const auto& E : *A) B.Broken.Add((int32)E->AsNumber());
			O->TryGetBoolField(TEXT("winches"), B.bWinches);
			D.Bridges.Add(B);
		}
	}
	const auto ReadPaths = [&](const TCHAR* Field, TArray<FPath>& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* P = nullptr;
		if (!Root->TryGetArrayField(Field, P)) return;
		for (const TSharedPtr<FJsonValue>& V : *P)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			FPath Path;
			FString S;
			if (O->TryGetStringField(TEXT("id"), S) || O->TryGetStringField(TEXT("route"), S)) Path.Id = FName(*S);
			if (O->TryGetStringField(TEXT("kind"), S)) Path.Kind = FName(*S);
			Path.Width = Num(O, TEXT("w"), 6.0) * 100.0;
			const TArray<TSharedPtr<FJsonValue>>* Pts = nullptr;
			if (O->TryGetArrayField(TEXT("pts"), Pts)) for (const auto& E : *Pts) Path.Points.Add(V3(E->AsArray()));
			Out.Add(Path);
		}
	};
	ReadPaths(TEXT("routes"), D.Routes);
	ReadPaths(TEXT("gusts"), D.Gusts);
	if (Root->TryGetArrayField(TEXT("overhangs"), Arr))
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			D.Overhangs.Add({ FVector(Num(O, TEXT("x")) * 100.0, Num(O, TEXT("y")) * 100.0, Num(O, TEXT("z")) * 100.0), (float)Num(O, TEXT("face")), (float)Num(O, TEXT("wall")) * 100.f });
		}
	}
	if (Root->TryGetArrayField(TEXT("shrines"), Arr))
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr) D.Shrines.Add(FVector(Num(V->AsObject(), TEXT("x")) * 100.0, Num(V->AsObject(), TEXT("y")) * 100.0, Num(V->AsObject(), TEXT("z")) * 100.0));
	}
	D.bLoaded = D.ZoneRows.Num() > 0;
	UE_LOG(LogWildreach, Log, TEXT("STORM data: %d sites, %d kit, %d rods, %d bridges, %d routes, %d overhangs"), D.Sites.Num(), D.Kit.Num(), D.Rods.Num(), D.Bridges.Num(), D.Routes.Num(), D.Overhangs.Num());
	return D;
}

TCHAR FWRStormData::ZoneAt(const FVector& Pos) const
{
	if (!bLoaded) return 0;
	const int32 I = FMath::FloorToInt((Pos.X - ZoneOrigin.X) / ZoneCell), J = FMath::FloorToInt((Pos.Y - ZoneOrigin.Y) / ZoneCell);
	if (I < 0 || J < 0 || J >= ZoneRows.Num() || I >= ZoneRows[J].Len()) return 0;
	return ZoneRows[J][I];
}

// ================================================================================================ WRStorm (shared calls)

namespace WRStormDetail
{
	FName StormBiome() { static const FName B(TEXT("storm_peaks")); return B; }
	FName Thunderstorm() { static const FName K(TEXT("thunderstorm")); return K; }

	/** Grounded (the Lightning path's root passive, main path) or a grounding charm worn: no stun, half damage. */
	bool IsGrounded(const AWRCharacterBase* V)
	{
		const AWRPlayerState* PS = V ? V->GetPlayerState<AWRPlayerState>() : nullptr;
		if (!PS) return false;
		if (PS->GetPaths() && PS->GetPaths()->GetMainPath() == TEXT("lightning")) return true;
		const UWRInventoryComponent* Inv = PS->GetInventory();
		return (Inv && Inv->GetEquipped(EWREquipSlot::Talisman).ItemId == TEXT("grounding_charm")) || WRStorm::HasBuff(V, TEXT("grounding"));
	}

	/** Holding a guard up and not swinging: the brace against a gust. */
	bool IsBracing(const AWRCharacterBase* C)
	{
		const UAbilitySystemComponent* ASC = C ? C->GetAbilitySystemComponent() : nullptr;
		return ASC && ASC->HasMatchingGameplayTag(WRTags::State_Blocking);
	}

	/** The creature this character is riding, if any. */
	const AWRCreature* MountOf(const AWRCharacterBase* C)
	{
		if (!C) return nullptr;
		for (TActorIterator<AWRCreature> It(C->GetWorld()); It; ++It) if (It->GetRider() == C) return *It;
		return nullptr;
	}
}

bool WRStorm::CallStrike(UObject* WorldContext, const FWRStrikeCall& Call)
{
	UWRStormWorld* S = UWRStormWorld::Get(WorldContext);
	return S && S->CallStrike(Call);
}

WRStorm::FOnStrikeLanded& WRStorm::OnStrikeLanded()
{
	static FOnStrikeLanded D;
	return D;
}

TCHAR WRStorm::ZoneAt(const UObject* WorldContext, const FVector& Pos)
{
	const FWRStormData& D = FWRStormData::Get();
	if (!D.bLoaded || !D.InFrame(Pos)) return 0;
	return D.ZoneAt(Pos);
}

bool WRStorm::GreatStorm(const UObject* WorldContext)
{
	// The Great Storm (world.json events.great_storm, run by the world systems' event step): its middle forty minutes,
	// between the Front and the Clearing, are when strikes fall everywhere (DC 2.15).
	const AWRWorldState* WS = AWRWorldState::Get(WorldContext);
	const FWRWorldEventRep* E = WS ? WS->Event(TEXT("great_storm")) : nullptr;
	if (!E || E->Stage != 1) return false;
	const float Now = UWRWeatherSubsystem::Now(WorldContext);
	return Now >= E->StartAt + WRTune(TEXT("storm.greatStorm.front"), 600.f) && Now < E->EndAt - WRTune(TEXT("storm.greatStorm.clearing"), 600.f);
}

bool WRStorm::InStorm(const UObject* WorldContext, const FVector& Pos)
{
	if (GreatStorm(WorldContext) && WRBiome::At(WorldContext, Pos) == WRStormDetail::StormBiome()) return true;
	const FWRWeatherKind* K = UWRWeatherSubsystem::ActiveKind(WorldContext, WRBiome::At(WorldContext, Pos));
	return K && K->Id == WRStormDetail::Thunderstorm();
}

bool WRStorm::NearestRod(const UObject* WorldContext, const FVector& Pos, float Range, FVector& OutTip)
{
	const UWRStormWorld* S = UWRStormWorld::Get(WorldContext);
	return S && S->NearestRod(Pos, Range, OutTip);
}

int32 WRStorm::AddRod(UWorld* World, AActor* Owner, const FVector& Tip)
{
	UWRStormWorld* S = UWRStormWorld::Get(World);
	return S ? S->AddRod(Owner, Tip) : INDEX_NONE;
}

void WRStorm::RemoveRod(UWorld* World, int32 Handle)
{
	if (UWRStormWorld* S = UWRStormWorld::Get(World)) S->RemoveRod(Handle);
}

int32 WRStorm::RodStrikes(UWorld* World, int32 Handle)
{
	const UWRStormWorld* S = UWRStormWorld::Get(World);
	return S ? S->RodStrikes(Handle) : 0;
}

namespace WRStormDetail
{
	TMap<TWeakObjectPtr<const AActor>, TMap<FName, double>>& Buffs() { static TMap<TWeakObjectPtr<const AActor>, TMap<FName, double>> B; return B; }
}

void WRStorm::GrantBuff(AActor* Who, FName Id, float Seconds)
{
	if (!Who || !Who->GetWorld()) return;
	WRStormDetail::Buffs().FindOrAdd(Who).Add(Id, Who->GetWorld()->GetTimeSeconds() + Seconds);
	if (AWRPlayerController* PC = Cast<AWRPlayerController>(Who->GetInstigatorController()))
	{
		PC->ClientNotify(FString::Printf(TEXT("%s for %d minutes"), Id == TEXT("grounding") ? TEXT("Grounded: no strike will stun you")
			: Id == TEXT("curse_ward") ? TEXT("Warded: curse builds at half") : TEXT("Warded: shock builds at half"), FMath::RoundToInt(Seconds / 60.f)));
	}
}

FName WRStorm::TakeOneBuff(AActor* Who)
{
	if (!Who || !Who->GetWorld()) return NAME_None;
	TMap<FName, double>* M = WRStormDetail::Buffs().Find(Who);
	if (!M) return NAME_None;
	// The newest still running goes first (the Starving One eats what you took last).
	FName Best;
	double BestUntil = Who->GetWorld()->GetTimeSeconds();
	for (const TPair<FName, double>& KV : *M) if (KV.Value > BestUntil) { BestUntil = KV.Value; Best = KV.Key; }
	if (!Best.IsNone()) M->Remove(Best);
	return Best;
}

bool WRStorm::HasBuff(const AActor* Who, FName Id)
{
	if (!Who || !Who->GetWorld()) return false;
	const TMap<FName, double>* M = WRStormDetail::Buffs().Find(Who);
	const double* Until = M ? M->Find(Id) : nullptr;
	return Until && Who->GetWorld()->GetTimeSeconds() < *Until;
}

bool WRStorm::ResistsStrikes(const AActor* Actor)
{
	const AWRCreature* C = Cast<AWRCreature>(Actor);
	const FWRCreatureDef* Def = C ? C->GetDef() : nullptr;
	return Def && (Def->Resists.Contains(TEXT("strike")) || Def->Biome == WRStormDetail::StormBiome());
}

// ================================================================================================ AWRStormState

AWRStormState::AWRStormState()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(2.f);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRStormState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRStormState, GlowUntilHours);
}

AWRStormState* AWRStormState::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World) return nullptr;
	for (TActorIterator<AWRStormState> It(const_cast<UWorld*>(World)); It; ++It) return *It;
	return nullptr;
}

void AWRStormState::MulticastPreTell_Implementation(AActor* Picked, float Seconds)
{
	if (GetNetMode() == NM_DedicatedServer || !Picked) return;
	// St Elmo's fire on the weapon hand and the shoulders, the hair lifting (the path marks' hair lift channel), and a
	// buzz rising over the seconds left (BotW: your own gear tells you first).
	const ACharacter* Ch = Cast<ACharacter>(Picked);
	USceneComponent* Body = Ch && Ch->GetMesh() ? (USceneComponent*)Ch->GetMesh() : Picked->GetRootComponent();
	for (const FName Socket : { FName(TEXT("hand_r")), FName(TEXT("hand_l")), FName(TEXT("head")), FName(TEXT("spine_05")) })
	{
		if (USceneComponent* Fx = WRFx::Attach(Body, TEXT("storm.pretell"), Socket, Socket == TEXT("head") ? 1.3f : 1.f))
		{
			FTimerHandle H;
			TWeakObjectPtr<USceneComponent> Weak(Fx);
			GetWorldTimerManager().SetTimer(H, [Weak]() { if (Weak.IsValid()) WRFx::Stop(Weak.Get()); }, FMath::Max(0.2f, Seconds), false);
		}
	}
	if (AWRCharacter* Hero = Cast<AWRCharacter>(Picked))
	{
		Hero->StrikePreTell(Seconds);
	}
	WRSound::PlayOn(Picked, TEXT("storm.pretell"));
}

void AWRStormState::MulticastGroundTell_Implementation(FVector_NetQuantize Spot, float Seconds, float Radius, uint8 Kind)
{
	if (GetNetMode() == NM_DedicatedServer) return;
	WRFx::SpawnFor(this, Kind == 1 ? FName(TEXT("storm.groundtell.crimson")) : FName(TEXT("storm.groundtell")), Spot, FRotator::ZeroRotator, Radius / 300.f, Seconds);
	WRSound::PlayAt(this, TEXT("storm.spark"), Spot);
	// The stepped leader: a faint branching channel crawls down in the glow's last 0.3 s.
	FTimerHandle H;
	const FVector At = Spot;
	TWeakObjectPtr<AWRStormState> Self(this);
	GetWorldTimerManager().SetTimer(H, [Self, At]() { if (Self.IsValid()) WRFx::SpawnAt(Self.Get(), TEXT("storm.leader"), At); }, FMath::Max(0.05f, Seconds - 0.3f), false);
}

void AWRStormState::MulticastStroke_Implementation(FVector_NetQuantize Spot, bool bRod, bool bWet)
{
	if (GetNetMode() == NM_DedicatedServer) return;
	WRFx::SpawnAt(this, TEXT("storm.stroke"), Spot);
	if (bWet) WRFx::SpawnAt(this, TEXT("storm.current"), Spot);
	if (bRod) WRFx::SpawnAt(this, TEXT("storm.rodflash"), Spot);
	WRFx::Shake(this, WRTune(TEXT("storm.strikeShake"), 0.35f), Spot, WRTune(TEXT("storm.strikeShakeRadius"), 6000.f));
	PlayThunderLater(Spot);
}

void AWRStormState::PlayThunderLater(const FVector& Spot)
{
	// Close: the crack at once. Far: the roll after distance / 340 m/s (3 s per km), quieter with distance.
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	FVector Ear = Spot;
	if (PC && PC->PlayerCameraManager) Ear = PC->PlayerCameraManager->GetCameraLocation();
	const float Dist = FVector::Dist(Ear, Spot);
	if (Dist < WRTune(TEXT("storm.crackRange"), 15000.f)) WRSound::PlayAt(this, TEXT("storm.strike"), Spot);
	const float Delay = Dist / WRTune(TEXT("storm.soundSpeed"), 34000.f);
	const float Vol = FMath::Clamp(1.f - Dist / WRTune(TEXT("storm.thunderRange"), 900000.f), 0.15f, 1.f);
	FTimerHandle H;
	TWeakObjectPtr<AWRStormState> Self(this);
	GetWorldTimerManager().SetTimer(H, [Self, Spot, Vol]()
	{
		if (!Self.IsValid()) return;
		const APlayerController* P = Self->GetWorld()->GetFirstPlayerController();
		const FVector At = P && P->PlayerCameraManager ? FMath::Lerp(P->PlayerCameraManager->GetCameraLocation(), Spot, 0.08f) : Spot;
		WRSound::PlayAt(Self.Get(), TEXT("storm.thunder"), At, Vol);
	}, FMath::Max(0.05f, Delay), false);
}

void AWRStormState::MulticastGustTell_Implementation(FVector_NetQuantize At, FVector_NetQuantizeNormal Dir)
{
	if (GetNetMode() == NM_DedicatedServer) return;
	WRFx::SpawnAt(this, TEXT("storm.gust"), At, Dir.Rotation());
	WRSound::PlayAt(this, TEXT("storm.gust"), At);
}

void AWRStormState::MulticastRockfall_Implementation(FVector_NetQuantize At, float TellSeconds)
{
	if (GetNetMode() == NM_DedicatedServer) return;
	WRFx::SpawnFor(this, TEXT("storm.trickle"), At + FVector(0, 0, 600.f), FRotator::ZeroRotator, 1.f, TellSeconds);
	WRFx::SpawnFor(this, TEXT("storm.rockwarn"), At, FRotator::ZeroRotator, 1.f, TellSeconds);
	WRSound::PlayAt(this, TEXT("storm.rockfall_trickle"), At);
	FTimerHandle H;
	TWeakObjectPtr<AWRStormState> Self(this);
	const FVector Spot = At;
	GetWorldTimerManager().SetTimer(H, [Self, Spot]()
	{
		if (!Self.IsValid()) return;
		WRFx::SpawnAt(Self.Get(), TEXT("storm.rockfall"), Spot);
		WRSound::PlayAt(Self.Get(), TEXT("storm.rockfall_impact"), Spot);
	}, FMath::Max(0.05f, TellSeconds), false);
}

// ================================================================================================ AWRStormScenery

AWRStormScenery::AWRStormScenery()
{
	bReplicates = false;
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Static);
}

void AWRStormScenery::Build()
{
	const bool bServer = GetNetMode() == NM_DedicatedServer;
	TArray<FWRLookBlock> Blocks;
	FKit K(GetActorTransform(), Blocks);
	const FTransform T = GetActorTransform();
	const auto G = [this](float X, float Y) { return GroundZ(this, FVector2D(X, Y)); };
	const auto At = [&](float X, float Y, float Lift = 0.f) { return FVector(X, Y, G(X, Y) + Lift); };
	const int32 Seed = int32(GetTypeHash(GetActorLocation().ToString()) % 997u);
	UMaterialInstanceDynamic* SlateRock = bServer ? nullptr : RockMat(this, Slate * 1.6f);
	UMaterialInstanceDynamic* GraniteRock = bServer ? nullptr : RockMat(this, Granite * 1.2f);
	UMaterialInstanceDynamic* CharRock = bServer ? nullptr : RockMat(this, Char * 6.f);
	const FName Kd = Kind;

	if (Kd == TEXT("bell_rod"))
	{
		// L2: a 60 m copper mast on its headland, green with age, three bronze bell-cages; guyed by four chains to rock
		// anchors so it stands in the gales; the keeper's ladder runs to the first cage and is broken above it (ST-S2).
		const float H = WRTune(TEXT("storm.bellRodHeight"), 6000.f);
		K.Hex(Stone(Granite * 0.7f), 0, 0, G(0, 0) - 60.f, G(0, 0) + 120.f, 220.f);
		K.Hex(Stone(Granite * 0.8f), 0, 0, G(0, 0) + 120.f, G(0, 0) + 180.f, 150.f);
		const float Z0 = G(0, 0) + 180.f;
		for (int32 s = 0; s < 6; ++s)
		{
			const float A = s / 6.f, B = (s + 1) / 6.f;
			K.Rod(Green(), FVector(0, 0, Z0 + H * A), FVector(0, 0, Z0 + H * B), FMath::Lerp(64.f, 22.f, A));
			K.Disc(BronzeS(), FVector(0, 0, Z0 + H * B), FVector::UpVector, FMath::Lerp(78.f, 30.f, B), 14.f);
		}
		for (int32 i = 0; i < 4; ++i) K.Spike(CopperBright(), FVector(0, 0, Z0 + H), FRotator(70.f, 90.f * i, 0.f).Vector(), 6.f, 120.f);
		K.Spike(CopperBright(), FVector(0, 0, Z0 + H), FVector::UpVector, 14.f, 260.f);
		for (const float Hc : { 0.33f, 0.63f, 0.93f })
		{
			const FVector C(0, 0, Z0 + H * Hc - 160.f);
			for (int32 b = 0; b < 8; ++b)
			{
				const FVector O = FRotator(0.f, 45.f * b, 0.f).Vector() * 150.f;
				K.Rod(BronzeS(), C + O, C + O + FVector(0, 0, 260.f), 6.f);
			}
			K.Disc(BronzeS(), C + FVector(0, 0, 262.f), FVector::UpVector, 320.f, 10.f);
			K.Disc(BronzeS(), C, FVector::UpVector, 320.f, 10.f);
			// The bell inside: a bronze cone flared at the lip, hung off-axis so it clears the mast.
			K.Add(BronzeS(), FVector(110.f, 110.f, 120.f), C + FVector(105.f, 0, 120.f), FRotator(180.f, 0.f, 0.f), EWRLookMesh::Cone);
			K.Disc(BronzeS(), C + FVector(105.f, 0, 62.f), FVector::UpVector, 124.f, 8.f);
		}
		for (int32 i = 0; i < 4; ++i)
		{
			const FVector Dir = FRotator(0.f, 45.f + 90.f * i, 0.f).Vector();
			const FVector Anchor = At(Dir.X * 1800.f, Dir.Y * 1800.f);
			K.Rod(Iron(), FVector(0, 0, Z0 + H * 0.66f), Anchor + FVector(0, 0, 60.f), 4.f);
			if (!bServer) Rock(this, Seed + i, FVector(160, 140, 110), Anchor, FRotator(0.f, 30.f * i, 0.f), SlateRock, true, 30.f);
			K.Disc(Iron(), Anchor + FVector(0, 0, 70.f), FVector::RightVector, 30.f, 8.f);
		}
		for (int32 r = 0; r < 34; ++r) K.Add(Iron(), FVector(10.f, 70.f, 8.f), FVector(-70.f, 0, Z0 + 60.f + r * 58.f), FRotator::ZeroRotator);
		for (const float Y : { -38.f, 38.f }) K.Rod(Iron(), FVector(-70.f, Y, Z0), FVector(-70.f, Y, Z0 + 2000.f), 6.f);
		K.Rod(Iron(), FVector(-70.f, -38.f, Z0 + 2000.f), FVector(-140.f, -60.f, Z0 + 1880.f), 6.f);   // the rail torn loose
		Block(this, FVector(0, 0, Z0 + H * 0.5f), FVector(40.f, 40.f, H * 0.5f));
		Block(this, FVector(0, 0, G(0, 0) + 60.f), FVector(200.f, 200.f, 120.f));
		if (!bServer) StormGlow.Add(Light(this, FVector(0, 0, Z0 + H + 200.f), Violet, 0.f, 3000.f));
	}
	else if (Kd == TEXT("strikerock"))
	{
		// L3: the tor split by one strike: glass down both inner faces of the notch, the climbers' camp in the split.
		for (const float Side : { -1.f, 1.f })
		{
			for (int32 i = 0; i < 9; ++i)
			{
				const float Y = -1600.f + i * 400.f;
				const float Top = G(Side * 900.f, Y);
				K.Add(Glass(0.08f), FVector(14.f, 380.f, FMath::Max(200.f, Top - G(0, Y))), FVector(Side * 290.f, Y, (Top + G(0, Y)) * 0.5f), FRotator(0.f, 0.f, Side * 6.f));
			}
		}
		// Ropes fixed down the faces on pitons (Vesna's, new hemp next to the Rodwardens' green chains).
		for (int32 i = 0; i < 4; ++i)
		{
			const float Y = -900.f + i * 600.f;
			const float Top = G(-600.f, Y);
			K.Rod(Rope(), FVector(-280.f, Y, Top), FVector(-280.f, Y, G(0, Y) + 20.f), 2.5f);
			K.Rod(Green(), FVector(-282.f, Y + 80.f, Top - 100.f), FVector(-282.f, Y + 80.f, G(0, Y) + 300.f), 3.5f);
			for (int32 p = 0; p < 5; ++p) K.Add(Iron(), FVector(16.f, 3.f, 3.f), FVector(-284.f, Y, FMath::Lerp(Top, G(0, Y), p / 5.f)), FRotator::ZeroRotator);
		}
		// Vesna's lean-to: two A-frames of pine poles, a hide stretched over, a bedroll, a ring of fire stones.
		const FVector L = At(120.f, 900.f);
		for (const float Y : { -140.f, 140.f })
		{
			K.Rod(Wood(), L + FVector(-150.f, Y, 0), L + FVector(0, Y, 220.f), 9.f);
			K.Rod(Wood(), L + FVector(150.f, Y, 0), L + FVector(0, Y, 220.f), 9.f);
		}
		K.Rod(Wood(), L + FVector(0, -170.f, 222.f), L + FVector(0, 170.f, 222.f), 8.f);
		K.Add(FSurf::Cloth(FLinearColor(0.16f, 0.1f, 0.06f)), FVector(260.f, 320.f, 4.f), L + FVector(-75.f, 0, 112.f), FRotator(56.f, 0.f, 0.f));
		K.Add(FSurf::Cloth(FLinearColor(0.12f, 0.12f, 0.15f)), FVector(70.f, 190.f, 12.f), L + FVector(-60.f, 0, 6.f));
		for (int32 i = 0; i < 9; ++i) K.Add(Stone(Slate), FVector(24.f, 18.f, 14.f), L + FVector(260.f, 0, 6.f) + FRotator(0.f, 40.f * i, 0.f).Vector() * 55.f, FRotator(0.f, 40.f * i, 0.f));
		K.Add(Stone(Char), FVector(60.f, 60.f, 5.f), L + FVector(260.f, 0, 3.f));
		for (int32 i = 0; i < 3; ++i) K.Disc(Rope(), L + FVector(40.f, -120.f + i * 70.f, 14.f), FVector::UpVector, 60.f, 22.f);
		if (!bServer) NightLights.Add(Light(this, L + FVector(260.f, 0, 60.f), FLinearColor(1.f, 0.55f, 0.25f), 3500.f, 900.f));
	}
	else if (Kd == TEXT("arch"))
	{
		// L4: a natural granite arch over the cliff path; from its keystone the Rodwardens' great thunderstone, an 8 m
		// polished axe-head, hangs on two copper chains; under it the hollow where the small thunderstones are offered.
		const float Span = 1600.f, Spring = 1200.f, R = Span * 0.5f;
		for (const float Side : { -1.f, 1.f })
		{
			const float Y = Side * (R + 200.f);
			const float Gz = G(0, Y);
			K.Add(Stone(Granite * 0.62f), FVector(420.f, 400.f, Spring - Gz + 300.f), FVector(0, Y, (Gz - 150.f + Spring + 150.f + G(0, 0)) * 0.5f));
			Block(this, FVector(0, Y, (Gz + Spring + G(0, 0)) * 0.5f), FVector(210.f, 200.f, (Spring + G(0, 0) - Gz) * 0.5f + 150.f));
			if (!bServer) Rock(this, Seed + (Side > 0 ? 1 : 2), FVector(520, 480, 520), FVector(0, Y + Side * 120.f, Gz), FRotator(0.f, 20.f * Side, 4.f), GraniteRock, true, 60.f);
		}
		const FVector Centre(0, 0, G(0, 0) + Spring);
		// The arch ring in the YZ plane: rotate the kit's XZ ring by 90 deg (blocks set along Y).
		K.Frame = FTransform(FRotator(0.f, 90.f, 0.f));
		K.Ring(Stone(Granite * 0.66f), Centre, R, 360.f, 400.f, 13, 0.f, 180.f, 0.f);
		K.Frame = FTransform::Identity;
		Block(this, Centre + FVector(0, 0, R + 180.f), FVector(200.f, R + 200.f, 180.f));
		const FVector Key = Centre + FVector(0, 0, R);
		const FVector Hang = Key - FVector(0, 0, 360.f);
		for (const float Y : { -120.f, 120.f })
		{
			for (int32 l = 0; l < 9; ++l)
			{
				const FVector A = FMath::Lerp(Key + FVector(0, Y, 0), Hang + FVector(0, Y * 0.6f, 0), l / 9.f);
				K.Add(Green(), FVector(14.f, 4.f, 40.f), A - FVector(0, 0, 20.f), FRotator(0.f, l % 2 ? 90.f : 0.f, 0.f));
			}
		}
		// The axe-head: a long polished wedge, its blade edge down, its butt drilled for the chains.
		const FSurf Polished = FSurf::Glaze(FLinearColor(0.03f, 0.035f, 0.04f), 0.18f);
		K.Add(Polished, FVector(110.f, 260.f, 800.f), Hang - FVector(0, 0, 400.f));
		K.Spike(Polished, Hang - FVector(0, 0, 780.f), FVector(0, 0, -1), 108.f, 120.f);
		K.Add(Polished, FVector(140.f, 300.f, 60.f), Hang - FVector(0, 0, 30.f));
		ThunderMark(K, Hang - FVector(-58.f, 0, 260.f), FVector(1, 0, 0), 30.f);
		// The offering hollow and its small thunderstones.
		const FVector Hollow = At(0, 0, 0.f);
		for (int32 i = 0; i < 14; ++i) K.Add(Stone(Granite * 0.55f), FVector(40.f, 30.f, 26.f), Hollow + FRotator(0.f, 360.f / 14 * i, 0.f).Vector() * 160.f + FVector(0, 0, 10.f), FRotator(0.f, 360.f / 14 * i, 0.f));
		for (int32 i = 0; i < 6; ++i) K.Add(Polished, FVector(12.f, 7.f, 20.f), Hollow + FRotator(0.f, 60.f * i + 20.f, 0.f).Vector() * 70.f + FVector(0, 0, 8.f), FRotator(0.f, 60.f * i, 70.f));
		if (!bServer) StormGlow.Add(Light(this, Hang - FVector(0, 0, 200.f), Violet, 0.f, 1600.f));
	}
	else if (Kd == TEXT("eyrie"))
	{
		// L5: the needle's top (terrain) and its banner cloud streaming east (Galvara's arena; the boss builder draws her
		// cores in it). Up the outside: the cut stair's edge stones and a chain rail on iron posts.
		const TArray<FWRStormData::FPath>& Routes = FWRStormData::Get().Routes;
		for (const FWRStormData::FPath& P : Routes)
		{
			if (P.Id != TEXT("eyrie_stair")) continue;
			for (int32 i = 0; i + 1 < P.Points.Num(); ++i)
			{
				const FVector A = T.InverseTransformPosition(P.Points[i]), B = T.InverseTransformPosition(P.Points[i + 1]);
				const FVector Out = FVector(A.X, A.Y, 0.f).GetSafeNormal();
				K.Add(Stone(Granite * 0.6f), FVector(FVector::Dist2D(A, B) + 10.f, 40.f, 50.f), (A + B) * 0.5f + Out * 260.f + FVector(0, 0, 10.f), (B - A).Rotation());
				if (i % 3 == 0)
				{
					K.Rod(Iron(), A + Out * 280.f, A + Out * 280.f + FVector(0, 0, 110.f), 6.f);
					K.Rod(Green(), A + Out * 280.f + FVector(0, 0, 100.f), B + Out * 280.f + FVector(0, 0, 100.f), 3.f);
				}
			}
		}
		const FVector Top = At(0, 0);
		for (int32 i = 0; i < 18; ++i) K.Add(BoneS(), FVector(12.f, 12.f, 60.f + 40.f * K.Rand(FVector(i, 0, 0), 2)), Top + FVector(FMath::Cos(i * 1.7f) * 900.f, FMath::Sin(i * 2.3f) * 600.f, 8.f), FRotator(80.f, i * 37.f, 0.f), EWRLookMesh::Cylinder);
		if (!bServer)
		{
			if (USceneComponent* Cloud = WRFx::Attach(RootComponent, TEXT("storm.banner_cloud")))
			{
				Cloud->SetRelativeLocation(Top + FVector(1500.f, 0, 3000.f));
			}
		}
	}
	else if (Kd == TEXT("copperfall"))
	{
		// L7: a 90 m fall stained green-blue by copper into a scree bowl of verdigris pools; old mine props half-buried.
		const FVector Lip = T.InverseTransformPosition(Extra);
		if (!bServer)
		{
			if (USceneComponent* Fall = WRFx::Attach(RootComponent, TEXT("storm.waterfall")))
			{
				Fall->SetRelativeLocation(Lip + FVector(0, 0, 40.f));
				Fall->SetRelativeRotation((FVector(0, 0, 0) - Lip).GetSafeNormal2D().Rotation());
			}
			if (USceneComponent* Spray = WRFx::Attach(RootComponent, TEXT("storm.spray"))) Spray->SetRelativeLocation(FVector(Lip.X * 0.6f, Lip.Y * 0.6f, G(Lip.X * 0.6f, Lip.Y * 0.6f) + 50.f));
		}
		for (int32 i = 0; i < 6; ++i)
		{
			const FVector P = At(FMath::Cos(i * 1.1f + 0.4f) * (600.f + 300.f * (i % 3)), FMath::Sin(i * 1.1f + 0.4f) * (500.f + 250.f * (i % 2)), 2.f);
			FSurf Pool = FSurf::Glaze(FLinearColor(0.02f, 0.18f, 0.16f), 0.06f);
			Pool.Glow = 0.3f;
			K.Disc(Pool, P, FVector::UpVector, 380.f + 160.f * (i % 3), 6.f);
			K.Disc(FSurf::Stone(Verdigris * 0.5f, 0.7f), P - FVector(0, 0, 2.f), FVector::UpVector, 440.f + 160.f * (i % 3), 4.f);
		}
		for (int32 i = 0; i < 5; ++i)
		{
			const FVector P = At(-1400.f + i * 160.f, 1100.f + i * 90.f);
			K.Rod(Wood(0.7f), P - FVector(0, 0, 60.f), P + FVector(30.f, 0, 200.f), 22.f);
			if (i % 2 == 0) K.Rod(Wood(0.7f), P + FVector(30.f, 0, 200.f), P + FVector(190.f, 90.f, 190.f), 18.f);
		}
		// A mine cart on its side, its wheels to the sky, spilling copper float.
		const FVector Cart = At(-900.f, 1300.f, 40.f);
		K.Add(Iron(), FVector(160.f, 100.f, 80.f), Cart, FRotator(0.f, 30.f, 80.f));
		for (int32 w = 0; w < 4; ++w) K.Disc(Iron(), Cart + FRotator(0.f, 30.f, 0.f).RotateVector(FVector(w < 2 ? -50.f : 50.f, 0, 0)) + FVector(0, w % 2 ? 60.f : -60.f, 50.f), FVector(0, 1, 0), 50.f, 10.f);
		for (int32 i = 0; i < 12; ++i) K.Add(FSurf::Stone(Copper * 0.35f, 0.6f), FVector(26.f, 20.f, 16.f), Cart + FVector(80.f + 18.f * i, -30.f + 9.f * (i % 5), -30.f), FRotator(0.f, i * 33.f, i * 11.f));
		if (!bServer) NightLights.Add(Light(this, FVector(0, 0, G(0, 0) + 80.f), FLinearColor(0.2f, 0.8f, 0.75f), 1800.f, 2600.f));
	}
	else if (Kd == TEXT("glass_scar"))
	{
		// L8: the 400 m groove of fused glass down the granite slab where the Great Storm always strikes first; it holds
		// a violet glow for 2 game hours after any storm (UWRStormWorld wakes StormGlow).
		const FVector A = T.InverseTransformPosition(FVector(Extra.X, Extra.Y, 0.f)), B = T.InverseTransformPosition(FVector(Extra2.X, Extra2.Y, 0.f));
		const int32 N = FMath::Max(2, FMath::RoundToInt(FVector::Dist2D(A, B) / 450.f));
		for (int32 i = 0; i < N; ++i)
		{
			const float F = (i + 0.5f) / N;
			const FVector2D P2(FMath::Lerp(A.X, B.X, F) + 120.f * FMath::Sin(F * 23.f), FMath::Lerp(A.Y, B.Y, F));
			const float Z = G(P2.X, P2.Y);
			const float Zn = G(P2.X, P2.Y + 450.f);
			const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(Zn - Z, 450.f));
			K.Add(Glass(0.04f), FVector(330.f + 200.f * FMath::Abs(FMath::Sin(F * 9.f)), 480.f, 16.f), FVector(P2.X, P2.Y, Z + 6.f), FRotator(0.f, 90.f, Pitch));
			if (i % 6 == 0) Fern(K, FVector(P2.X + 350.f, P2.Y, Z + 2.f), 700.f, Seed + i);
		}
		if (!bServer) for (int32 i = 0; i < 4; ++i) StormGlow.Add(Light(this, FMath::Lerp(A, B, (i + 0.5f) / 4.f) + FVector(0, 0, G(FMath::Lerp(A.X, B.X, (i + 0.5f) / 4.f), FMath::Lerp(A.Y, B.Y, (i + 0.5f) / 4.f)) + 200.f), Violet, 0.f, 4500.f));
	}
	else if (Kd == TEXT("monastery"))
	{
		// L9 / S5: the Hanging Monastery: whitewashed cells on the carved ledges of the Crown's west face (the ledges are
		// the generator's anchors and the stair route), galleries looking west, chain walks between them. Its braziers
		// burn for good once the Hanging Shrine is lit.
		struct FC { FVector2D W; float Yaw; bool bRoof; };
		const FC Cells[] = { { FVector2D(-83000.f, -330000.f), 180.f, true }, { FVector2D(-76000.f, -329000.f), 185.f, true }, { FVector2D(-80000.f, -333000.f), 180.f, true }, { FVector2D(-70000.f, -333000.f), 175.f, true } };
		FVector Prev = FVector::ZeroVector;
		for (const FC& C : Cells)
		{
			const FVector L = T.InverseTransformPosition(FVector(C.W, 0.f));
			const FVector Base = At(L.X, L.Y, 4.f);
			Cell(K, Base, C.Yaw, C.bRoof);
			Block(this, Base + FVector(0, 0, 170.f), FVector(220.f, 260.f, 170.f), FRotator(0.f, C.Yaw, 0.f));
			if (!bServer) NightLights.Add(Light(this, Base + FRotator(0.f, C.Yaw, 0.f).RotateVector(FVector(330.f, 0, 160.f)), FLinearColor(1.f, 0.6f, 0.3f), 4000.f, 1500.f));
			if (!Prev.IsZero()) for (const float Z : { 90.f, 110.f }) K.Rod(Green(), Prev + FVector(0, 0, Z), Base + FVector(0, 0, Z), 3.f);
			Prev = Base;
			// Copper wind-chimes on the gallery (they sing on the wind: the S5 bed).
			for (int32 c = 0; c < 5; ++c) K.Rod(CopperBright(), Base + FRotator(0.f, C.Yaw, 0.f).RotateVector(FVector(360.f, -120.f + c * 60.f, 240.f)), Base + FRotator(0.f, C.Yaw, 0.f).RotateVector(FVector(360.f, -120.f + c * 60.f, 180.f - c * 12.f)), 3.f);
		}
	}
	else if (Kd == TEXT("storm_mill"))
	{
		// L10: the ruined storm mill: stone tower, sails torn, its copper rod still standing and melted at the tip.
		const float Z = G(0, 0);
		for (int32 c = 0; c < 10; ++c) K.Hex(Stone(Slate * (0.9f + 0.05f * (c % 3))), 0, 0, Z - 40.f + c * 130.f, Z + 90.f + c * 130.f, 430.f - c * 9.f, c * 7.f);
		K.Add(Stone(Char), FVector(30.f, 140.f, 220.f), FVector(420.f, 0, Z + 110.f));
		K.Add(Wood(0.8f), FVector(14.f, 120.f, 200.f), FVector(430.f, 30.f, Z + 100.f), FRotator(0.f, 30.f, 0.f));
		K.Add(Wood(0.6f), FVector(700.f, 700.f, 220.f), FVector(0, 0, Z + 1360.f), FRotator::ZeroRotator, EWRLookMesh::Cone);
		const FVector Hub(400.f, 0, Z + 1250.f);
		K.Disc(Wood(0.6f), Hub, FVector(1, 0, 0), 90.f, 60.f);
		for (int32 s = 0; s < 4; ++s)
		{
			const float A = 45.f + 90.f * s + 7.f;
			const FVector Dir = FRotator(0.f, 0.f, A).RotateVector(FVector(0, 0, 1));
			K.Rod(Wood(0.7f), Hub, Hub + Dir * (s == 2 ? 520.f : 900.f), 16.f);
			for (int32 b = 1; b < (s == 2 ? 3 : 6); ++b)
			{
				K.Rod(Wood(0.7f), Hub + Dir * (150.f * b), Hub + Dir * (150.f * b) + FRotator(0.f, 0.f, A + 90.f).RotateVector(FVector(0, 0, 160.f)), 6.f);
				if ((b + s) % 2) K.Add(FSurf::Cloth(FLinearColor(0.32f, 0.3f, 0.26f)), FVector(3.f, 70.f, 110.f), Hub + Dir * (150.f * b + 40.f) + FRotator(0.f, 0.f, A + 90.f).RotateVector(FVector(0, 0, 80.f)) + FVector(10.f, 0, -40.f), FRotator(0.f, 0.f, A));
			}
		}
		RodLook(K, FVector(-60.f, 0, Z + 1460.f), 600.f, 10.f);
		K.Add(FSurf::Steel(Copper * 0.4f, 0.3f), FVector(40.f, 34.f, 30.f), FVector(-60.f, 0, Z + 2060.f), FRotator(0.f, 0.f, 20.f), EWRLookMesh::Cylinder);   // the melted tip
		Block(this, FVector(0, 0, Z + 650.f), FVector(380.f, 380.f, 700.f));
		if (!bServer) StormGlow.Add(Light(this, FVector(-60.f, 0, Z + 2080.f), Violet, 0.f, 1200.f));
	}
	else if (Kd == TEXT("summit_nest"))
	{
		// L1's top: Kharvox's nest on the bowl's north side, a 14 m ring of whole pine trunks with hundreds of stolen
		// copper rods twisted in like twigs; the ossuary slab on the east, smashed bone and torcs among it.
		const FVector NestC = At(0, -1800.f);
		for (int32 l = 0; l < 4; ++l)
		{
			for (int32 i = 0; i < 16; ++i)
			{
				const float A = 360.f / 16 * i + l * 11.f;
				const FVector P = NestC + FRotator(0.f, A, 0.f).Vector() * (620.f - l * 60.f) + FVector(0, 0, 50.f + l * 70.f);
				const FVector Along = FRotator(l * 4.f - 6.f, A + 90.f + (l % 2 ? 14.f : -14.f), 0.f).Vector();
				K.Rod(Wood(0.55f), P - Along * 420.f, P + Along * 420.f, 44.f - l * 4.f);
			}
		}
		for (int32 i = 0; i < 90; ++i)
		{
			const float A = i * 4.f + K.Rand(FVector(i, 1, 0), 3) * 40.f;
			const FVector P = NestC + FRotator(0.f, A, 0.f).Vector() * (450.f + 230.f * K.Rand(FVector(i, 2, 0), 4)) + FVector(0, 0, 60.f + 200.f * K.Rand(FVector(i, 3, 0), 5));
			const FVector D = FRotator(-30.f + 60.f * K.Rand(FVector(i, 4, 0), 6), A + 70.f, 0.f).Vector();
			K.Rod(i % 3 ? Green() : CopperBright(), P - D * 260.f, P + D * 260.f, 6.f);
		}
		Block(this, NestC + FVector(0, 0, 160.f), FVector(560.f, 560.f, 160.f));
		const FVector Slab = At(1800.f, -400.f);
		if (!bServer) Rock(this, Seed, FVector(620, 420, 90), Slab, FRotator(0.f, 20.f, 0.f), GraniteRock, true, 20.f);
		for (int32 i = 0; i < 40; ++i)
		{
			const FVector P = Slab + FVector(-260.f + 520.f * K.Rand(FVector(i, 5, 0), 1), -180.f + 360.f * K.Rand(FVector(i, 6, 0), 2), 75.f);
			K.Add(BoneS(), FVector(8.f + 6.f * (i % 3), 8.f, 30.f + 30.f * K.Rand(P, 3)), P, FRotator(90.f, i * 47.f, 0.f), EWRLookMesh::Cylinder);
		}
		for (int32 i = 0; i < 7; ++i)
		{
			K.Frame = FTransform(FRotator(90.f, i * 40.f, 0.f), Slab + FVector(-200.f + 70.f * i, 40.f * (i % 3), 82.f));
			K.Ring(CopperBright(), FVector::ZeroVector, 9.f, 3.f, 3.f, 10, 0.f, 330.f, 0.4f);
			K.Frame = FTransform::Identity;
		}
	}
	else if (Kd == TEXT("keeper_hut") || Kd == TEXT("waystation"))
	{
		Hut(K, this, At(0, 0), GetActorRotation().Yaw * 0.f + 0.f, Variant);
		Block(this, FVector(0, 0, G(0, 0) + 150.f), FVector(250.f, 250.f, 150.f));
		if (Kd == TEXT("keeper_hut"))
		{
			// A sea-chest of burnt bell clappers outside the door.
			const FVector C = At(380.f, 120.f, 30.f);
			K.Add(Wood(0.7f), FVector(110.f, 60.f, 60.f), C);
			for (int32 i = 0; i < 4; ++i) K.Rod(Stone(Char), C + FVector(-40.f + 25.f * i, 0, 30.f), C + FVector(-40.f + 25.f * i, 30.f, 80.f), 10.f);
		}
		if (!bServer) NightLights.Add(Light(this, FVector(-150.f, 0, G(0, 0) + 140.f), FLinearColor(1.f, 0.6f, 0.3f), 1200.f, 700.f));
	}
	else if (Kd == TEXT("rye_terraces") || Kd == TEXT("puffram_fold") || Kd == TEXT("bellwether_fold"))
	{
		// Dry-stone walls: terrace walls along the contour (rye behind them, flattened and standing back up), a sheep
		// fold with its lambing shed, or the Bellwether's ruined fold, its gate smashed outward.
		const bool bTerr = Kd == TEXT("rye_terraces"), bBell = Kd == TEXT("bellwether_fold");
		const int32 Rings = bTerr ? 4 : 1;
		for (int32 r = 0; r < Rings; ++r)
		{
			const float Rad = bTerr ? 1400.f + r * 700.f : (bBell ? 1500.f : 900.f);
			const int32 N = bTerr ? 24 : 40;
			for (int32 i = 0; i < N; ++i)
			{
				const float A = bTerr ? -60.f + 120.f * i / N : 360.f * i / N;
				if (!bTerr && i < (bBell ? 4 : 2)) continue;              // the gate
				if (bBell && (i % 9 == 4)) continue;                       // fallen gaps
				const FVector P2 = FRotator(0.f, A, 0.f).Vector() * Rad + (bTerr ? FVector(-Rad, 0, 0) : FVector::ZeroVector);
				const FVector P = At(P2.X, P2.Y);
				K.Add(Stone(Slate * (0.9f + 0.2f * K.Rand(P, 1)), 0.95f), FVector(70.f, 2.f * PI * Rad / N + 20.f, 110.f + 20.f * K.Rand(P, 2)), P + FVector(0, 0, 40.f), FRotator(0.f, A, 0.f));
				K.Add(Stone(Slate * 0.8f, 0.95f), FVector(40.f, 2.f * PI * Rad / N, 18.f), P + FVector(0, 0, 105.f), FRotator(0.f, A, 0.f));
				if (bTerr && i % 2 == 0)
				{
					for (int32 s = 0; s < 6; ++s)
					{
						const FVector Q = P + FRotator(0.f, A, 0.f).RotateVector(FVector(-80.f - 40.f * s, -40.f + 13.f * s, 0.f));
						K.Rod(FSurf::Cloth(Rye * 0.55f), Q, Q + FVector(20.f * FMath::Sin(s * 1.3f), 10.f, 80.f + 20.f * (s % 3)), 2.f);
					}
				}
			}
		}
		if (Kd == TEXT("puffram_fold"))
		{
			const FVector S = At(-500.f, 0.f);
			for (const float Y : { -260.f, 260.f }) K.Rod(Wood(), S + FVector(0, Y, 0), S + FVector(0, Y, 220.f), 12.f);
			K.Add(Stone(Slate * 0.6f), FVector(320.f, 640.f, 10.f), S + FVector(-130.f, 0, 200.f), FRotator(20.f, 0.f, 0.f));
		}
		if (bBell)
		{
			// The gate's planks thrown out down the slope, and the bronze bell-clapper, as long as an arm, in the grass.
			for (int32 i = 0; i < 6; ++i) K.Add(Wood(0.75f), FVector(240.f, 22.f, 5.f), At(1700.f + 140.f * i, -120.f + 60.f * (i % 4), 6.f), FRotator(0.f, 30.f * i, 4.f));
			const FVector Cl = At(1450.f, 200.f, 12.f);
			K.Rod(BronzeS(), Cl, Cl + FVector(90.f, 30.f, 0.f), 9.f);
			K.Add(BronzeS(), FVector(26.f, 26.f, 26.f), Cl + FVector(100.f, 34.f, 0.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		}
	}
	else if (Kd == TEXT("feeding_stones"))
	{
		// Six flat slabs facing the Spire, iron hooks, flensing knives, bone splinters and small copper torcs in the cracks.
		const FVector ToSpire = (FVector(-20000.f, -330000.f, 0.f) - GetActorLocation()).GetSafeNormal2D();
		const float Yaw = ToSpire.Rotation().Yaw;
		for (int32 i = 0; i < 6; ++i)
		{
			const FVector P = At(FRotator(0.f, Yaw + 90.f, 0.f).Vector().X * (-500.f + 200.f * i), FRotator(0.f, Yaw + 90.f, 0.f).Vector().Y * (-500.f + 200.f * i));
			K.Add(Stone(Granite * 0.7f), FVector(220.f, 120.f, 26.f), P + FVector(0, 0, 12.f), FRotator(0.f, Yaw, 0.f));
			K.Rod(Iron(), P + FRotator(0.f, Yaw, 0.f).RotateVector(FVector(130.f, 0, 0)), P + FRotator(0.f, Yaw, 0.f).RotateVector(FVector(130.f, 0, 140.f)), 5.f);
			K.Rod(Iron(), P + FRotator(0.f, Yaw, 0.f).RotateVector(FVector(130.f, 0, 140.f)), P + FRotator(0.f, Yaw, 0.f).RotateVector(FVector(110.f, 0, 120.f)), 4.f);
			K.Add(Iron(), FVector(30.f, 4.f, 2.f), P + FVector(30.f, 20.f, 27.f), FRotator(0.f, Yaw + 50.f * i, 0.f));
			for (int32 b = 0; b < 4; ++b) K.Add(BoneS(), FVector(4.f, 4.f, 16.f), P + FVector(-40.f + 25.f * b, -20.f + 10.f * b, 26.f), FRotator(90.f, b * 61.f, 0.f), EWRLookMesh::Cylinder);
			K.Frame = FTransform(FRotator(90.f, i * 31.f, 0.f), P + FVector(60.f, -30.f, 27.f));
			K.Ring(CopperBright(), FVector::ZeroVector, 6.f, 2.f, 2.f, 8, 0.f, 320.f, 0.3f);
			K.Frame = FTransform::Identity;
		}
	}
	else if (Kd == TEXT("melted_cell"))
	{
		// The top cell, its roof open to the sky; a torc melted into the floor at the heart of a Lichtenberg burn that
		// spreads across the floor and up the walls.
		const FVector Base = At(0, 0, 4.f);
		Cell(K, Base, 180.f, false);
		Block(this, Base + FVector(0, 0, 170.f), FVector(220.f, 260.f, 170.f), FRotator(0.f, 180.f, 0.f));
		Fern(K, Base + FVector(0, 0, 2.f), 520.f, Seed);
		K.Add(FSurf::Steel(Copper * 0.4f, 0.25f), FVector(40.f, 30.f, 5.f), Base + FVector(0, 0, 3.f), FRotator(0.f, 20.f, 0.f), EWRLookMesh::Cylinder);
	}
	else if (Kd == TEXT("kitchens"))
	{
		// Long tables with the bowls still set, an oven, and the chain winch that hauled food up the cliff (its chain
		// runs from the drum over the edge, down toward the lower path).
		const FVector Base = At(0, 0);
		for (int32 t = 0; t < 2; ++t)
		{
			const FVector Tb = Base + FVector(-150.f + 300.f * t, 0, 0);
			K.Add(Wood(1.05f), FVector(120.f, 520.f, 10.f), Tb + FVector(0, 0, 80.f));
			for (const float Y : { -220.f, 220.f }) for (const float X : { -45.f, 45.f }) K.Rod(Wood(), Tb + FVector(X, Y, 0), Tb + FVector(X, Y, 76.f), 9.f, true);
			for (int32 b = 0; b < 6; ++b) K.Disc(Stone(Granite * 0.5f, 0.6f), Tb + FVector(b % 2 ? 30.f : -30.f, -200.f + 80.f * b, 88.f), FVector::UpVector, 26.f, 8.f);
		}
		const FVector Oven = Base + FVector(0, 420.f, 0);
		for (int32 r = 0; r < 5; ++r) K.Hex(Stone(Slate * 0.9f), Oven.X, Oven.Y, Oven.Z + r * 35.f, Oven.Z + (r + 1) * 35.f, 140.f - r * 22.f, r * 20.f);
		K.Add(Stone(Char), FVector(40.f, 60.f, 50.f), Oven + FVector(120.f, 0, 40.f));
		const FVector Drum = Base + FVector(260.f, -420.f, 0);
		for (const float Y : { -70.f, 70.f }) K.Add(Wood(0.8f), FVector(30.f, 20.f, 140.f), Drum + FVector(0, Y, 70.f));
		K.Disc(Wood(0.9f), Drum + FVector(0, 0, 120.f), FVector(0, 1, 0), 70.f, 130.f);
		K.Rod(Iron(), Drum + FVector(0, 80.f, 120.f), Drum + FVector(-40.f, 110.f, 160.f), 5.f);
		K.Rod(Green(), Drum + FVector(30.f, 0, 120.f), Drum + FVector(380.f, 0, -900.f), 4.f);
		Block(this, Base + FVector(0, 420.f, 90.f), FVector(150.f, 150.f, 90.f));
		if (!bServer) NightLights.Add(Light(this, Oven + FVector(140.f, 0, 50.f), FLinearColor(1.f, 0.5f, 0.2f), 1500.f, 700.f));
	}
	else if (Kd == TEXT("kite_nests") || Kd == TEXT("kite_roost"))
	{
		// Stormkite nests of sticks and copper wire (on the gorge rim, or in a dead pine snag); eggshells, dropped fish.
		if (Kd == TEXT("kite_roost"))
		{
			const FVector B = At(0, 0);
			K.Rod(Wood(0.4f), B - FVector(0, 0, 40.f), B + FVector(30.f, 0, 900.f), 40.f);
			for (int32 i = 0; i < 6; ++i) K.Rod(Wood(0.4f), B + FVector(10.f, 0, 300.f + i * 100.f), B + FVector(10.f, 0, 300.f + i * 100.f) + FRotator(20.f, i * 71.f, 0.f).Vector() * (220.f - i * 25.f), 10.f);
			Block(this, B + FVector(0, 0, 450.f), FVector(30.f, 30.f, 450.f));
		}
		const int32 Nests = Kd == TEXT("kite_roost") ? 2 : 5;
		for (int32 n = 0; n < Nests; ++n)
		{
			const FVector C = Kd == TEXT("kite_roost") ? At(0, 0, 560.f + 260.f * n) + FVector(60.f * n, 0, 0) : At(FMath::Cos(n * 1.4f) * 700.f, FMath::Sin(n * 1.4f) * 500.f, 10.f);
			for (int32 s = 0; s < 18; ++s)
			{
				const float A = s * 20.f;
				K.Rod(s % 4 ? Wood(0.6f) : CopperBright(), C + FRotator(0.f, A, 0.f).Vector() * 70.f + FVector(0, 0, 10.f), C + FRotator(0.f, A + 60.f, 0.f).Vector() * 70.f + FVector(0, 0, 22.f), 4.f);
			}
			for (int32 e = 0; e < 3; ++e) K.Add(FSurf::Stone(BoneWhite * 0.9f, 0.5f), FVector(14.f, 10.f, 6.f), C + FVector(-20.f + 18.f * e, 8.f * e, 14.f), FRotator(0.f, e * 50.f, 30.f));
		}
	}
	else if (Kd == TEXT("fort"))
	{
		// The Rodwarden fort at Stormgut's west mouth, facing east: a curtain wall across the gut with its gate, two
		// towers each crowned with a rod, Coven bone hooks driven into the east face, the willow-stake shrine smashed.
		for (const float Side : { -1.f, 1.f })
		{
			const FVector Tw = At(0, Side * 1100.f);
			for (int32 c = 0; c < 8; ++c) K.Hex(Stone(Granite * (0.62f + 0.04f * (c % 2))), Tw.X, Tw.Y, Tw.Z - 60.f + c * 150.f, Tw.Z + 90.f + c * 150.f, 360.f, c * 4.f);
			for (int32 m = 0; m < 6; ++m) K.Add(Stone(Granite * 0.6f), FVector(90.f, 90.f, 90.f), Tw + FVector(0, 0, 1290.f) + FRotator(0.f, 60.f * m, 0.f).Vector() * 300.f);
			RodLook(K, Tw + FVector(0, 0, 1240.f), 900.f, 10.f);
			Block(this, Tw + FVector(0, 0, 600.f), FVector(330.f, 330.f, 660.f));
			const float WallLen = 1100.f - 380.f - 260.f;
			const FVector Wm = At(0, Side * (260.f + WallLen * 0.5f));
			K.Add(Stone(Granite * 0.66f), FVector(260.f, WallLen, 760.f), Wm + FVector(0, 0, 330.f));
			Block(this, Wm + FVector(0, 0, 330.f), FVector(130.f, WallLen * 0.5f, 380.f));
			for (int32 h = 0; h < 6; ++h)
			{
				const FVector Hk = Wm + FVector(140.f, -WallLen * 0.4f + WallLen * 0.16f * h, 250.f + 80.f * (h % 3));
				K.Rod(BoneS(), Hk, Hk + FVector(70.f, 0, -20.f), 9.f);
				K.Spike(BoneS(), Hk + FVector(70.f, 0, -20.f), FVector(0.2f, 0, -1.f), 9.f, 40.f);
			}
		}
		K.Add(Wood(0.6f), FVector(30.f, 500.f, 460.f), At(-40.f, 0, 230.f));   // the gate (closed bar lifted: a passage beside it)
		const FVector Sh = At(900.f, 300.f);
		for (int32 i = 0; i < 9; ++i)
		{
			const float A = 40.f * i;
			const FVector B = Sh + FRotator(0.f, A, 0.f).Vector() * 160.f;
			K.Rod(Wood(0.45f), B, B + FRotator(30.f + 40.f * (i % 3), A, 0.f).Vector() * (60.f + 90.f * (i % 2)), 10.f);
		}
	}
	else if (Kd == TEXT("burnt_drake"))
	{
		// A 30 m leech-drake skeleton burnt black in the gut, a Lichtenberg scorch fanning from it, rods driven through
		// its ribs into the stone.
		const FSurf Burnt = Stone(Char * 1.6f, 0.85f);
		FVector Prev = At(-1500.f, 0, 40.f);
		for (int32 v = 0; v < 30; ++v)
		{
			const float X = -1500.f + v * 100.f;
			const FVector P = At(X, 160.f * FMath::Sin(v * 0.35f), 40.f + 60.f * FMath::Sin(v * 0.21f));
			K.Add(Burnt, FVector(80.f, 70.f, 60.f), P, (P - Prev).Rotation());
			K.Spike(Burnt, P + FVector(0, 0, 30.f), FVector::UpVector, 26.f, 70.f);
			if (v > 6 && v < 22)
			{
				for (const float Side : { -1.f, 1.f })
				{
					const float Rr = 240.f - FMath::Abs(v - 13) * 14.f;
					FVector Last = P;
					for (int32 s = 1; s <= 5; ++s)
					{
						const float A = s / 5.f * 160.f;
						const FVector Q = P + FVector(-30.f * s, Side * Rr * FMath::Sin(FMath::DegreesToRadians(A)), Rr * (1.f - FMath::Cos(FMath::DegreesToRadians(A))) - Rr * 0.9f + 60.f);
						K.Rod(Burnt, Last, Q, 16.f);
						Last = Q;
					}
				}
				if (v % 4 == 0) RodLook(K, P + FVector(0, 0, -200.f), 600.f, 9.f);
			}
			Prev = P;
		}
		const FVector Skull = At(1550.f, 0, 120.f);
		K.Add(Burnt, FVector(260.f, 160.f, 140.f), Skull);
		K.Add(Burnt, FVector(240.f, 130.f, 40.f), Skull + FVector(150.f, 0, -70.f), FRotator(-14.f, 0.f, 0.f));
		for (int32 t = 0; t < 8; ++t) K.Spike(BoneS(), Skull + FVector(130.f + 30.f * t, t % 2 ? 50.f : -50.f, -60.f), FVector(0, 0, -1), 10.f, 50.f);
		Fern(K, At(0, 0, 3.f), 2800.f, Seed);
		Block(this, At(0, 0, 150.f), FVector(1500.f, 200.f, 150.f));
	}
	else if (Kd == TEXT("rust_pools"))
	{
		// Iron-red mud pools on the ledge under the Spire, giant feathers stuck in the mud, deep talon prints.
		for (int32 i = 0; i < 4; ++i)
		{
			const FVector P = At(-600.f + 400.f * i, 200.f * FMath::Sin(i * 1.7f), 3.f);
			K.Disc(FSurf::Glaze(Rust * 0.45f, 0.2f), P, FVector::UpVector, 380.f + 90.f * (i % 2), 7.f);
			K.Disc(FSurf::Stone(Rust * 0.3f, 0.95f), P - FVector(0, 0, 3.f), FVector::UpVector, 470.f + 90.f * (i % 2), 4.f);
			const FVector F = P + FVector(120.f, -60.f, 0.f);
			K.Add(FSurf::Cloth(Slate * 0.4f), FVector(16.f, 60.f, 260.f), F + FVector(0, 0, 120.f), FRotator(18.f, 40.f * i, 6.f));
			K.Rod(BoneS(), F, F + FRotator(72.f, 40.f * i, 0.f).Vector() * 250.f, 4.f);
		}
		for (int32 p = 0; p < 3; ++p)
		{
			const FVector P = At(-300.f + 500.f * p, 500.f, 1.f);
			for (int32 t = 0; t < 3; ++t) K.Add(Stone(Rust * 0.2f), FVector(150.f, 26.f, 3.f), P + FRotator(0.f, -30.f + 30.f * t, 0.f).Vector() * 75.f, FRotator(0.f, -30.f + 30.f * t, 0.f));
			K.Add(Stone(Rust * 0.2f), FVector(60.f, 24.f, 3.f), P - FVector(70.f, 0, 0));
		}
	}
	else if (Kd == TEXT("struck_climbers"))
	{
		// Three roped climbers dead at exactly 600 m, metal axes still in hand, burn ferns on the rock round them.
		for (int32 c = 0; c < 3; ++c)
		{
			const FVector B = At(-180.f + 180.f * c, 40.f * c, 8.f);
			const FRotator R(0.f, 20.f * c + 80.f, 0.f);
			const auto P = [&](float X, float Y, float Z) { return B + R.RotateVector(FVector(X, Y, Z)); };
			K.Add(BoneS(), FVector(22.f, 18.f, 22.f), P(80.f, 0, 10.f));                                         // skull
			for (int32 r = 0; r < 6; ++r) for (const float S : { -1.f, 1.f }) K.Rod(BoneS(), P(20.f - r * 10.f, 0, 4.f), P(20.f - r * 10.f, S * 18.f, 12.f), 2.5f);
			K.Rod(BoneS(), P(60.f, 0, 6.f), P(-60.f, 0, 6.f), 4.f);                                              // spine
			for (const float S : { -1.f, 1.f })
			{
				K.Rod(BoneS(), P(-60.f, S * 12.f, 4.f), P(-140.f, S * 25.f, 4.f), 4.f);
				K.Rod(BoneS(), P(-140.f, S * 25.f, 4.f), P(-210.f, S * 30.f, 6.f), 3.5f);
				K.Rod(BoneS(), P(40.f, S * 22.f, 6.f), P(10.f, S * 55.f, 6.f), 3.5f);
			}
			K.Rod(Wood(0.8f), P(10.f, 55.f, 6.f), P(-60.f, 80.f, 8.f), 4.f);                                      // the axe in hand
			K.Add(Iron(), FVector(30.f, 4.f, 14.f), P(-60.f, 86.f, 10.f), R);
			if (c < 2) K.Rod(Rope(), P(-60.f, 0, 8.f), B + FRotator(0.f, 20.f * (c + 1) + 80.f, 0.f).RotateVector(FVector(-60.f, 0, 8.f)) + FVector(180.f, 40.f, 0.f), 2.f);
			Fern(K, B, 380.f, Seed + c);
		}
	}
	else if (Kd == TEXT("rams_ring"))
	{
		for (int32 i = 0; i < 9; ++i)
		{
			const FVector P = At(FMath::Cos(i * 2.1f) * 1300.f * K.Rand(FVector(i, 0, 0), 1), FMath::Sin(i * 2.1f) * 1100.f * K.Rand(FVector(i, 1, 0), 2), 8.f);
			K.Add(FSurf::Stone(FLinearColor(FColor(168, 130, 62)) * 0.55f, 0.6f), FVector(26.f, 26.f, 70.f), P, FRotator(80.f, i * 40.f, 0.f), EWRLookMesh::Cone);
		}
		const FVector H = At(200.f, -300.f, 20.f);
		for (int32 s = 0; s < 8; ++s)
		{
			const FVector A = H + FRotator(0.f, s * 40.f, 0.f).Vector() * 70.f + FVector(0, 0, s * 8.f);
			const FVector B = H + FRotator(0.f, (s + 1) * 40.f, 0.f).Vector() * 70.f + FVector(0, 0, (s + 1) * 8.f);
			K.Rod(FSurf::Stone(FLinearColor(FColor(168, 130, 62)) * 0.55f, 0.6f), A, B, 38.f - s * 3.f);
		}
	}
	else if (Kd == TEXT("storm_stair"))
	{
		// The Storm stair of the Twelve: its edging stones up the carved stair, broken off at 700 m where the last
		// blocks have fallen down the slope.
		for (const FWRStormData::FPath& P : FWRStormData::Get().Routes)
		{
			if (P.Id != TEXT("storm_stair")) continue;
			for (int32 i = 0; i + 1 < P.Points.Num(); ++i)
			{
				const FVector A = T.InverseTransformPosition(P.Points[i]), B = T.InverseTransformPosition(P.Points[i + 1]);
				const FVector Side = FVector::CrossProduct((B - A).GetSafeNormal(), FVector::UpVector);
				for (const float S : { -1.f, 1.f }) K.Add(Stone(Granite * 0.58f), FVector(FVector::Dist(A, B) + 6.f, 40.f, 50.f), (A + B) * 0.5f + Side * S * 230.f + FVector(0, 0, 10.f), (B - A).Rotation());
			}
		}
		for (int32 i = 0; i < 7; ++i) K.Add(Stone(Granite * 0.55f), FVector(120.f, 90.f, 60.f), At(500.f + 140.f * i, -200.f + 90.f * (i % 3), 20.f), FRotator(10.f * i, 23.f * i, 5.f));
	}
	else if (Kd == TEXT("neck_cairns"))
	{
		// The neck road: Rodwarden rods and Forgebound slag cairns side by side, ash drifted on the road.
		for (int32 i = 0; i < 3; ++i)
		{
			const FVector P = At(-400.f + 400.f * i, 250.f);
			RodLook(K, P, 500.f, 8.f);
			for (int32 s = 0; s < 3; ++s) K.Add(Stone(Char * 2.f), FVector(60.f - 12.f * s, 55.f - 10.f * s, 36.f), P + FVector(150.f, 0, 18.f + 36.f * s), FRotator(0.f, s * 41.f, 0.f));
			K.Disc(FSurf::Stone(FLinearColor(0.06f, 0.06f, 0.06f), 0.98f), At(-400.f + 400.f * i, -100.f, 1.f), FVector::UpVector, 380.f, 3.f);
		}
	}
	else if (Kd == TEXT("warden_door"))
	{
		// The Warden Door on the Ring: a bronze door half open in a granite frame cut into the Spire's foot, thunder
		// marks on its lintel; beyond it, darkness (the Hollow Spire).
		const FVector B = At(0, 0);
		const FRotator Face = (FVector(-20000.f, -330000.f, 0.f) - GetActorLocation()).GetSafeNormal2D().Rotation();
		const auto P = [&](float X, float Y, float Z) { return B + Face.RotateVector(FVector(X, Y, Z)); };
		for (const float Y : { -260.f, 260.f }) K.Add(Stone(Granite * 0.6f), FVector(160.f, 90.f, 620.f), P(0, Y, 310.f), Face);
		K.Add(Stone(Granite * 0.62f), FVector(180.f, 640.f, 110.f), P(0, 0, 660.f), Face);
		ThunderMark(K, P(-92.f, -160.f, 660.f), -Face.Vector(), 26.f);
		ThunderMark(K, P(-92.f, 160.f, 660.f), -Face.Vector(), 26.f);
		K.Add(Stone(Char), FVector(40.f, 430.f, 600.f), P(60.f, 0, 300.f), Face);
		K.Add(BronzeS(), FVector(14.f, 210.f, 580.f), P(-40.f, -110.f, 292.f), FRotator(0.f, Face.Yaw - 50.f, 0.f));
		K.Add(BronzeS(), FVector(14.f, 210.f, 580.f), P(-10.f, 105.f, 292.f), Face);
		Block(this, P(40.f, 0, 340.f), FVector(80.f, 340.f, 360.f), Face);
	}
	else if (Kd == TEXT("ring_west"))
	{
		// The Lightning pillar of the Twelve on the Ring: a granite shaft banded in violet glass that burns in the Great Storm.
		const FVector B = At(0, 0);
		K.Hex(Stone(Granite * 0.55f), B.X, B.Y, B.Z - 60.f, B.Z + 120.f, 260.f);
		for (int32 s = 0; s < 6; ++s) K.Hex(Stone(Granite * (0.6f + 0.03f * s)), B.X, B.Y, B.Z + 120.f + s * 280.f, B.Z + 400.f + s * 280.f, 160.f - s * 6.f, s * 10.f);
		for (int32 s = 1; s < 6; ++s) K.Hex(Glass(0.15f), B.X, B.Y, B.Z + 120.f + s * 280.f - 12.f, B.Z + 120.f + s * 280.f + 12.f, 168.f - s * 6.f, s * 10.f);
		RodLook(K, B + FVector(0, 0, 1800.f), 500.f, 12.f);
		Block(this, B + FVector(0, 0, 900.f), FVector(150.f, 150.f, 960.f));
		if (!bServer) StormGlow.Add(Light(this, B + FVector(0, 0, 2400.f), Violet, 0.f, 8000.f));
	}
	else if (Kd == TEXT("camp"))
	{
		// The castaways' camp: the wreck that brought them on the cove below (hull ribs, keel, the mast down), a fire
		// ring, crates, a tarp lean-to.
		const FVector W = T.InverseTransformPosition(FVector(Extra.X, Extra.Y, 0.f));
		const FVector Wk = At(W.X, W.Y, -40.f);
		const FRotator Hull(0.f, 15.f, 18.f);
		K.Rod(Wood(0.5f), Wk + Hull.RotateVector(FVector(-900.f, 0, 0)), Wk + Hull.RotateVector(FVector(900.f, 0, 60.f)), 50.f);
		for (int32 r = 0; r < 12; ++r)
		{
			const float X = -800.f + r * 145.f;
			for (const float S : { -1.f, 1.f })
			{
				const float Wd = 300.f * FMath::Sin(PI * (r + 1) / 13.f);
				K.Rod(Wood(0.5f), Wk + Hull.RotateVector(FVector(X, 0, 20.f)), Wk + Hull.RotateVector(FVector(X, S * Wd, 280.f)), 20.f);
				if (S > 0 || r % 3 == 0) K.Add(Wood(0.55f), FVector(140.f, 8.f, 30.f), Wk + Hull.RotateVector(FVector(X, S * Wd * 0.8f, 150.f)), FRotator(Hull.Pitch, Hull.Yaw, S * 60.f));
			}
		}
		K.Rod(Wood(0.5f), Wk + FVector(100.f, 0, 200.f), Wk + FVector(1400.f, 500.f, 30.f), 28.f);
		const FVector F = At(0, 0);
		for (int32 i = 0; i < 10; ++i) K.Add(Stone(Slate), FVector(26.f, 18.f, 16.f), F + FRotator(0.f, 36.f * i, 0.f).Vector() * 60.f + FVector(0, 0, 8.f), FRotator(0.f, 36.f * i, 0.f));
		for (int32 c = 0; c < 3; ++c) K.Add(Wood(0.8f), FVector(80.f, 70.f, 60.f), F + FVector(-300.f, -200.f + 90.f * c, 30.f + (c == 1 ? 60.f : 0.f)), FRotator(0.f, 10.f * c, 0.f));
		for (const float Y : { -200.f, 200.f }) K.Rod(Wood(), F + FVector(250.f, Y, 0), F + FVector(250.f, Y, 210.f), 9.f);
		K.Add(FSurf::Cloth(FLinearColor(0.3f, 0.26f, 0.18f)), FVector(300.f, 440.f, 4.f), F + FVector(150.f, 0, 120.f), FRotator(-40.f, 0.f, 0.f));
		if (!bServer)
		{
			if (USceneComponent* Fire = WRFx::Attach(RootComponent, TEXT("held.torch"), NAME_None, 3.f)) Fire->SetRelativeLocation(F + FVector(0, 0, 20.f));
			NightLights.Add(Light(this, F + FVector(0, 0, 80.f), FLinearColor(1.f, 0.55f, 0.25f), 5000.f, 1400.f));
		}
	}
	else if (Kd == TEXT("smelter"))
	{
		// The Rodwardens' hidden smelter: a stone bloomery with its bellows pit, a slag heap gone green, moulds for rods.
		const FVector B = At(0, 0);
		for (int32 r = 0; r < 6; ++r) K.Hex(Stone(Slate * 0.8f), B.X, B.Y, B.Z + r * 40.f, B.Z + (r + 1) * 40.f, 110.f - r * 10.f, r * 15.f);
		K.Add(Stone(Char), FVector(40.f, 50.f, 60.f), B + FVector(110.f, 0, 50.f));
		for (int32 i = 0; i < 9; ++i) K.Add(FSurf::Stone(Verdigris * 0.35f, 0.7f), FVector(50.f, 40.f, 26.f), B + FVector(-200.f + 30.f * i, 150.f + 20.f * (i % 3), 13.f + 12.f * (i % 2)), FRotator(0.f, i * 37.f, 10.f));
		for (int32 m = 0; m < 3; ++m) K.Add(Stone(Granite * 0.5f), FVector(220.f, 30.f, 16.f), B + FVector(0, -200.f - 40.f * m, 8.f));
		K.Add(Wood(0.6f), FVector(90.f, 70.f, 30.f), B + FVector(170.f, -90.f, 15.f));
	}
	// ----------------------------------------------------------------------------------------- kit
	else if (Kd == TEXT("bidental"))
	{
		// A fenced ring where a strike killed someone: short stone posts with copper wire between them, the centre stone
		// over a buried thunderstone, a burn fern under it (Roman bidental).
		const int32 N = 8 + Variant * 2;
		const float R = 220.f + Variant * 40.f;
		FVector Prev;
		for (int32 i = 0; i <= N; ++i)
		{
			const float A = 360.f * i / N;
			const FVector P = At(FMath::Cos(FMath::DegreesToRadians(A)) * R, FMath::Sin(FMath::DegreesToRadians(A)) * R);
			if (i < N) K.Add(Stone(Granite * 0.6f), FVector(26.f, 26.f, 90.f), P + FVector(0, 0, 40.f), FRotator(0.f, A, 0.f));
			if (i > 0) K.Rod(Green(), Prev + FVector(0, 0, 70.f), P + FVector(0, 0, 70.f), 1.6f);
			Prev = P;
		}
		const FVector C = At(0, 0);
		K.Add(Stone(Granite * 0.5f), FVector(90.f, 70.f, 24.f), C + FVector(0, 0, 8.f));
		Fern(K, C, 300.f, Seed);
	}
	else if (Kd == TEXT("fulgurite_field"))
	{
		// Sand where lightning keeps striking: hollow glass tubes standing out of it (the stormglass nodes are added by the
		// server; these are the old, spent ones), the sand glazed in patches.
		for (int32 i = 0; i < 26; ++i)
		{
			const FVector P = At(FMath::Cos(i * 2.4f) * (150.f + 22.f * i), FMath::Sin(i * 2.4f) * (130.f + 20.f * i));
			const float H = 20.f + 50.f * K.Rand(P, 1);
			K.Rod(Glass(0.05f), P - FVector(0, 0, 10.f), P + FRotator(70.f + 15.f * K.Jit(P, 2), i * 47.f, 0.f).Vector() * H, 5.f + 4.f * K.Rand(P, 3));
		}
		for (int32 i = 0; i < 5; ++i) K.Disc(Glass(0.02f), At(FMath::Cos(i * 1.3f) * 400.f, FMath::Sin(i * 1.3f) * 300.f, 1.f), FVector::UpVector, 120.f + 40.f * i, 2.f);
	}
	else if (Kd == TEXT("outcrop"))
	{
		// A copper or tin outcrop: broken slate with the vein's colour streaked through it (the ore node itself is the
		// server's resource node in the middle).
		const FLinearColor Vein = Variant % 2 ? FLinearColor(0.2f, 0.17f, 0.12f) : Verdigris * 0.5f;
		if (!bServer) for (int32 i = 0; i < 3; ++i) Rock(this, Seed + i, FVector(320 - 60 * i, 260 - 40 * i, 220 - 50 * i), At(-250.f + 250.f * i, 120.f * (i % 2)), FRotator(6.f, 70.f * i, 3.f), SlateRock, true, 30.f);
		for (int32 i = 0; i < 6; ++i) K.Add(FSurf::Stone(Vein, 0.55f), FVector(90.f, 10.f, 30.f), At(-220.f + 90.f * i, 80.f - 30.f * i, 60.f + 20.f * (i % 3)), FRotator(25.f, 15.f * i, 0.f));
	}
	else if (Kd == TEXT("basking_rock"))
	{
		if (!bServer) Rock(this, Seed, FVector(520, 420, 260 + 80 * Variant), At(0, 0), FRotator(4.f, Seed * 1.f, 2.f), GraniteRock, true, 40.f);
		Fern(K, At(0, 0, 0.f) + FVector(0, 0, 220.f + 80.f * Variant), 140.f, Seed);
	}
	else if (Kd == TEXT("tarn"))
	{
		// A snowmelt pool in a hollow: dark still water, its stony rim, sedge at the edge.
		FSurf Water = FSurf::Glaze(FLinearColor(0.012f, 0.025f, 0.035f), 0.03f);
		const FVector C = At(0, 0, 4.f);
		K.Disc(Water, C, FVector::UpVector, 900.f + 300.f * Variant, 4.f);
		for (int32 i = 0; i < 22; ++i)
		{
			const float A = 360.f / 22 * i;
			const FVector P = At(FMath::Cos(FMath::DegreesToRadians(A)) * (470.f + 150.f * Variant), FMath::Sin(FMath::DegreesToRadians(A)) * (470.f + 150.f * Variant));
			K.Add(Stone(Slate * 0.8f), FVector(60.f, 44.f, 26.f), P + FVector(0, 0, 6.f), FRotator(0.f, A, 0.f));
			for (int32 s = 0; s < 3; ++s) K.Rod(FSurf::Cloth(FLinearColor(0.08f, 0.1f, 0.04f)), P + FVector(20.f * s, 10.f, 0), P + FVector(20.f * s + 6.f, 14.f, 60.f + 10.f * s), 1.5f);
		}
	}
	else if (Kd == TEXT("mound_field"))
	{
		for (int32 i = 0; i < 6 + Variant * 2; ++i)
		{
			const FVector P = At(FMath::Cos(i * 2.2f) * (200.f + 60.f * i), FMath::Sin(i * 2.2f) * (180.f + 50.f * i));
			if (!bServer) Rock(this, Seed + i, FVector(150, 140, 50), P, FRotator(0.f, i * 50.f, 0.f), RockMat(this, FLinearColor(0.09f, 0.07f, 0.05f)), false, 25.f);
			K.Disc(Stone(FLinearColor(0.03f, 0.025f, 0.02f)), P + FVector(0, 0, 26.f), FVector::UpVector, 30.f, 6.f);
		}
	}
	else if (Kd == TEXT("kill_scene"))
	{
		// A Voltfang pack's kill: a Puffram's wool scattered, the carcass opened, scorch where the static took it.
		const FVector C = At(0, 0, 10.f);
		K.Add(FSurf::Cloth(FLinearColor(0.75f, 0.74f, 0.7f)), FVector(120.f, 80.f, 50.f), C, FRotator(0.f, 30.f, 0.f), EWRLookMesh::Cylinder);
		for (int32 r = 0; r < 6; ++r) for (const float S : { -1.f, 1.f }) K.Rod(BoneS(), C + FVector(-40.f + 14.f * r, 0, 26.f), C + FVector(-40.f + 14.f * r, S * 40.f, 50.f), 2.f);
		for (int32 i = 0; i < 14; ++i) K.Add(FSurf::Cloth(FLinearColor(0.7f, 0.7f, 0.66f)), FVector(20.f, 18.f, 8.f), At(FMath::Cos(i * 1.9f) * (80.f + 20.f * i), FMath::Sin(i * 1.9f) * (70.f + 18.f * i), 4.f));
		K.Disc(Stone(FLinearColor(0.12f, 0.01f, 0.01f), 0.4f), C - FVector(0, 0, 8.f), FVector::UpVector, 200.f, 2.f);
		Fern(K, C - FVector(0, 0, 8.f), 220.f, Seed);
	}
	else if (Kd == TEXT("struck_tree"))
	{
		// A pine split down the middle and charred; resin bleeding from the split, charcoal at the foot.
		const FVector B = At(0, 0);
		for (const float S : { -1.f, 1.f })
		{
			K.Rod(Stone(Char * 2.f, 0.9f), B + FVector(0, S * 14.f, -20.f), B + FVector(S * 40.f, S * 60.f, 520.f + 120.f * (S > 0)), 26.f);
			K.Rod(FSurf::Glaze(FLinearColor(0.4f, 0.2f, 0.03f), 0.2f), B + FVector(0, S * 2.f, 120.f), B + FVector(0, S * 2.f, 40.f), 4.f);
		}
		for (int32 i = 0; i < 8; ++i) K.Add(Stone(Char), FVector(30.f, 14.f, 12.f), B + FRotator(0.f, i * 45.f, 0.f).Vector() * 80.f + FVector(0, 0, 4.f), FRotator(0.f, i * 45.f, 0.f));
		Block(this, B + FVector(0, 0, 300.f), FVector(40.f, 40.f, 300.f));
	}
	else if (Kd == TEXT("goatherd"))
	{
		// The goatherd's cave: a Puffram pen of posts and rails, a cold fire, gentian drying on strings.
		const FVector B = At(0, 0);
		for (int32 i = 0; i < 8; ++i)
		{
			const FVector P = B + FRotator(0.f, 45.f * i, 0.f).Vector() * 420.f + FVector(300.f, 0, 0);
			K.Rod(Wood(0.8f), FVector(P.X, P.Y, G(P.X, P.Y) - 20.f), FVector(P.X, P.Y, G(P.X, P.Y) + 120.f), 10.f);
			const FVector Q = B + FRotator(0.f, 45.f * (i + 1), 0.f).Vector() * 420.f + FVector(300.f, 0, 0);
			if (i != 3) for (const float Z : { 50.f, 100.f }) K.Rod(Wood(0.85f), FVector(P.X, P.Y, G(P.X, P.Y) + Z), FVector(Q.X, Q.Y, G(Q.X, Q.Y) + Z), 6.f);
		}
		const FVector Str0 = B + FVector(-200.f, -200.f, 190.f), Str1 = B + FVector(-200.f, 200.f, 190.f);
		K.Rod(Rope(), Str0, Str1, 1.5f);
		for (int32 i = 1; i < 9; ++i) K.Add(FSurf::Cloth(FLinearColor(0.08f, 0.1f, 0.4f)), FVector(10.f, 10.f, 26.f), FMath::Lerp(Str0, Str1, i / 9.f) - FVector(0, 0, 16.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		for (const FVector& E : { Str0, Str1 }) K.Rod(Wood(), E - FVector(0, 0, 190.f), E, 8.f);
		for (int32 i = 0; i < 8; ++i) K.Add(Stone(Slate), FVector(22.f, 16.f, 14.f), B + FVector(100.f, 0, 8.f) + FRotator(0.f, 45.f * i, 0.f).Vector() * 50.f, FRotator(0.f, 45.f * i, 0.f));
		K.Add(Stone(FLinearColor(0.02f, 0.02f, 0.02f)), FVector(50.f, 50.f, 4.f), B + FVector(100.f, 0, 3.f));
	}
	else if (Kd == TEXT("cave_mouth"))
	{
		// The mouth: timbered for the Copper Adit, salt-pink for the Halite Cave, wet slate for the grotto. The cave
		// itself is the server's AWRCave.
		const FLinearColor Tone = Extra.Z > 0.f ? FLinearColor(0.5f, 0.3f, 0.3f) : Slate;
		if (!bServer) for (int32 i = 0; i < 4; ++i) Rock(this, Seed + i, FVector(420, 380, 360), At(-300.f + 200.f * i, i % 2 ? 380.f : -380.f), FRotator(0.f, 40.f * i, 5.f), RockMat(this, Tone * 1.4f), true, 60.f);
		for (const float Y : { -220.f, 220.f }) K.Rod(Wood(0.7f), At(500.f, Y), At(500.f, Y, 320.f), 24.f);
		K.Rod(Wood(0.7f), At(500.f, -260.f, 320.f), At(500.f, 260.f, 320.f), 24.f);
	}
	FWRBuildLook::Spawn(this, RootComponent, Blocks, FLinearColor(0.25f, 0.22f, 0.18f), false);
}

// ================================================================================================ AWRStormRods

AWRStormRods::AWRStormRods()
{
	bReplicates = false;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 1.f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRStormRods::Build(const TArray<FWRStormData::FRod>& Rods)
{
	const bool bServer = GetNetMode() == NM_DedicatedServer;
	UStaticMesh* Cyl = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* Cone = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Building/M_BuildTinted.M_BuildTinted"));
	const auto Make = [&](UStaticMesh* Mesh, const FLinearColor& C, float Metal, float Rough, float Glow, bool bCollide)
	{
		UHierarchicalInstancedStaticMeshComponent* H = NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
		H->SetStaticMesh(Mesh);
		H->SetupAttachment(RootComponent);
		H->SetCollisionProfileName(bCollide ? TEXT("BlockAll") : TEXT("NoCollision"));
		H->SetCullDistances(0, WRTune(TEXT("storm.rodCull"), 40000.f));
		H->SetCanEverAffectNavigation(false);
		if (Base && !bServer)
		{
			UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, this);
			M->SetVectorParameterValue(TEXT("Color"), C);
			M->SetScalarParameterValue(TEXT("Metal"), Metal);
			M->SetScalarParameterValue(TEXT("Rough"), Rough);
			M->SetScalarParameterValue(TEXT("Glow"), Glow);
			H->SetMaterial(0, M);
		}
		H->RegisterComponent();
		return H;
	};
	Shafts = Make(Cyl, Verdigris * 0.55f, 0.45f, 0.62f, 0.f, true);
	Collars = Make(Cyl, Bronze * 0.6f, 0.85f, 0.5f, 0.f, false);
	Tips = Make(Cone, Copper * 0.75f, 0.85f, 0.38f, 0.f, false);
	for (const FWRStormData::FRod& R : Rods)
	{
		const float D = R.Kind == TEXT("field") ? 7.f : 10.f;
		Shafts->AddInstance(FTransform(FRotator::ZeroRotator, R.Base + FVector(0, 0, R.Height * 0.5f - 30.f), FVector(D / 100.f, D / 100.f, (R.Height + 60.f) / 100.f)));
		Collars->AddInstance(FTransform(FRotator::ZeroRotator, R.Base + FVector(0, 0, 10.f), FVector(D * 2.8f / 100.f, D * 2.8f / 100.f, 0.2f)));
		Tips->AddInstance(FTransform(FRotator::ZeroRotator, R.Base + FVector(0, 0, R.Height + 15.f), FVector(D * 0.8f / 100.f, D * 0.8f / 100.f, 0.32f)));
		TipsAt.Add(R.Base + FVector(0, 0, R.Height + 30.f));
	}
	if (HasAuthority())
	{
		for (const FVector& Tip : TipsAt) Handles.Add(WRStorm::AddRod(GetWorld(), this, Tip));
	}
}

void AWRStormRods::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// St Elmo's fire: in a storm every rod tip glows (DC 2.1 night light); the tip material brightens with it.
	if (GetNetMode() == NM_DedicatedServer || !Tips || TipsAt.Num() == 0) return;
	const float Want = WRStorm::InStorm(this, TipsAt[0]) ? 1.f : 0.f;
	if (FMath::IsNearlyEqual(Want, GlowLevel)) return;
	GlowLevel = Want;
	if (UMaterialInstanceDynamic* M = Cast<UMaterialInstanceDynamic>(Tips->GetMaterial(0)))
	{
		M->SetScalarParameterValue(TEXT("Glow"), Want * WRTune(TEXT("storm.rodTipGlow"), 6.f));
		M->SetVectorParameterValue(TEXT("Color"), Want > 0.f ? Violet : Copper * 0.75f);
	}
}

void AWRStormRods::EndPlay(const EEndPlayReason::Type Reason)
{
	for (const int32 H : Handles) WRStorm::RemoveRod(GetWorld(), H);
	Handles.Reset();
	Super::EndPlay(Reason);
}

// ================================================================================================ AWRStormBridge

AWRStormBridge::AWRStormBridge()
{
	bReplicates = true;
	SetNetUpdateFrequency(4.f);
	SetNetCullDistanceSquared(FMath::Square(60000.f));
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRStormBridge::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRStormBridge, BridgeId);
	DOREPLIFETIME(AWRStormBridge, From);
	DOREPLIFETIME(AWRStormBridge, To);
	DOREPLIFETIME(AWRStormBridge, Spans);
	DOREPLIFETIME(AWRStormBridge, bWinches);
	DOREPLIFETIME(AWRStormBridge, BrokenSpans);
	DOREPLIFETIME(AWRStormBridge, Planks);
	DOREPLIFETIME(AWRStormBridge, LanternsLit);
}

FVector AWRStormBridge::DeckAt(float T) const
{
	// The deck hangs from tower to tower in a catenary-like sag in each span; the towers hold it level at the ends.
	const float SpanF = T * Spans;
	const int32 S = FMath::Clamp(FMath::FloorToInt(SpanF), 0, Spans - 1);
	const float Local = SpanF - S;
	const float Length = FVector::Dist2D(From, To) / Spans;
	const float Sag = FMath::Clamp(Length * WRTune(TEXT("storm.bridgeSag"), 0.045f), 20.f, 260.f);
	const FVector P = FMath::Lerp(FVector(From), FVector(To), T);
	return P - FVector(0, 0, Sag * 4.f * Local * (1.f - Local));
}

FVector AWRStormBridge::PlankCentre(int32 i) const
{
	return DeckAt((i + 0.5f) / FMath::Max(1, Planks.Num()));
}

void AWRStormBridge::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && Planks.Num() == 0)
	{
		// One plank every 60 cm; 1 in 12 rotten, never two in a row (seeded by the bridge so it's the same each start).
		const int32 N = FMath::Max(4, FMath::RoundToInt(FVector::Dist2D(From, To) / WRTune(TEXT("storm.plankPitch"), 60.f)));
		Planks.SetNumZeroed(N);
		FRandomStream R(GetTypeHash(BridgeId));
		const float Every = WRTune(TEXT("storm.rottenEvery"), 12.f);
		for (int32 i = 1; i < N - 1; ++i)
		{
			if (Planks[i - 1] == 0 && R.FRand() < 1.f / Every) Planks[i] = 1;
		}
		for (const int32 S : BrokenSpans)
		{
			for (int32 i = 0; i < N; ++i) if (FMath::FloorToInt((i + 0.5f) / N * Spans) == S - 1) Planks[i] = 3;   // 3: the whole span is down
		}
		BreakAt.Init(-1.0, N);
		MendAtHours.Init(-1.0, N);
	}
	if (!bLookBuilt) BuildLook();
	OnRep_Planks();
}

void AWRStormBridge::BuildLook()
{
	bLookBuilt = true;
	const bool bServer = GetNetMode() == NM_DedicatedServer;
	TArray<FWRLookBlock> Blocks;
	FKit K(FTransform::Identity, Blocks);
	const FVector Dir = (FVector(To) - FVector(From)).GetSafeNormal2D();
	const FVector Side = FVector::CrossProduct(Dir, FVector::UpVector);
	const float HalfW = WRTune(TEXT("storm.bridgeHalfWidth"), 75.f);
	const int32 Samples = Spans * 16;
	// Towers at both ends and on every pillar: two timber posts lashed with copper bands, a cross-beam the hand ropes run
	// over; on the Nine Spans each pillar carries a winch hut whose drum takes up the foot ropes.
	for (int32 s = 0; s <= Spans; ++s)
	{
		const FVector P = FMath::Lerp(FVector(From), FVector(To), float(s) / Spans);
		for (const float Sd : { -1.f, 1.f })
		{
			K.Rod(Wood(0.7f), P + Side * Sd * HalfW - FVector(0, 0, 120.f), P + Side * Sd * HalfW + FVector(0, 0, 160.f), 22.f);
			K.Disc(Green(), P + Side * Sd * HalfW + FVector(0, 0, 40.f), FVector::UpVector, 28.f, 10.f);
		}
		K.Rod(Wood(0.7f), P + Side * HalfW + FVector(0, 0, 150.f), P - Side * HalfW + FVector(0, 0, 150.f), 16.f);
		if (bWinches && s > 0 && s < Spans)
		{
			const FVector H = P + Side * 260.f;
			for (int32 c = 0; c < 4; ++c) K.Add(Stone(Slate * 0.9f), FVector(200.f, 200.f, 40.f), H + FVector(0, 0, -20.f + c * 40.f), FRotator(0.f, Dir.Rotation().Yaw, 0.f));
			K.Add(Wood(0.8f), FVector(210.f, 210.f, 12.f), H + FVector(0, 0, 150.f), FRotator(0.f, Dir.Rotation().Yaw, 6.f));
			K.Disc(Wood(0.9f), H + FVector(0, 0, 60.f) - Side * 120.f, Side, 70.f, 50.f);
			K.Disc(Iron(), H + FVector(0, 0, 60.f) - Side * 90.f, Side, 90.f, 6.f);
			K.Rod(Iron(), H + FVector(0, 0, 60.f) - Side * 86.f, H + FVector(0, 0, 100.f) - Side * 86.f + Dir * 40.f, 5.f);
		}
	}
	// Hand ropes and foot ropes, sagging with the deck; the netting of light cord between them.
	for (int32 i = 0; i < Samples; ++i)
	{
		const float A = float(i) / Samples, B = float(i + 1) / Samples;
		const int32 SpanIdx = FMath::FloorToInt(A * Spans) + 1;
		if (BrokenSpans.Contains(SpanIdx)) continue;
		for (const float Sd : { -1.f, 1.f })
		{
			K.Rod(Rope(), DeckAt(A) + Side * Sd * HalfW - FVector(0, 0, 6.f), DeckAt(B) + Side * Sd * HalfW - FVector(0, 0, 6.f), 5.f);
			K.Rod(Rope(), DeckAt(A) + Side * Sd * HalfW + FVector(0, 0, 105.f), DeckAt(B) + Side * Sd * HalfW + FVector(0, 0, 105.f), 4.f);
			K.Rod(Rope(), DeckAt(A) + Side * Sd * HalfW, DeckAt(A) + Side * Sd * HalfW + FVector(0, 0, 104.f), 1.5f);
		}
	}
	// A broken span hangs down the gorge wall from the near tower, its rope still tied (ST-S1).
	for (const int32 S : BrokenSpans)
	{
		const FVector Top = FMath::Lerp(FVector(From), FVector(To), float(S - 1) / Spans);
		for (int32 r = 0; r < 24; ++r)
		{
			const FVector A = Top + FVector(0, 0, -r * 80.f) + Dir * (FMath::Sin(r * 0.4f) * 20.f);
			K.Rod(Rope(), A + Side * HalfW, A + Side * HalfW - FVector(0, 0, 80.f), 4.f);
			if (r % 2) K.Add(Wood(0.6f), FVector(4.f, HalfW * 2.f, 26.f), A - FVector(0, 0, 40.f));
		}
	}
	FWRBuildLook::Spawn(this, RootComponent, Blocks, FLinearColor(0.25f, 0.22f, 0.18f), false);
	if (!bServer && bWinches)
	{
		for (int32 s = 1; s < Spans; ++s)
		{
			UPointLightComponent* L = Light(this, FMath::Lerp(FVector(From), FVector(To), float(s) / Spans) + Side * 260.f + FVector(0, 0, 120.f), FLinearColor(1.f, 0.6f, 0.3f), 0.f, 900.f);
			L->ComponentTags.Add(TEXT("winch_lantern"));
		}
	}
	BuildPlanks();
}

void AWRStormBridge::BuildPlanks()
{
	const int32 N = Planks.Num();
	if (N == 0) return;
	const FVector Dir = (FVector(To) - FVector(From)).GetSafeNormal2D();
	const float HalfW = WRTune(TEXT("storm.bridgeHalfWidth"), 75.f);
	const float Len = FVector::Dist2D(From, To) / N;
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Building/M_BuildTinted.M_BuildTinted"));
	// Two instance sets: sound planks and rotten ones (a shade darker so they read before you reach them).
	for (int32 k = 0; k < 2; ++k)
	{
		UHierarchicalInstancedStaticMeshComponent* H = NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
		H->SetStaticMesh(Cube);
		H->SetupAttachment(RootComponent);
		H->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (Base && GetNetMode() != NM_DedicatedServer)
		{
			UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, this);
			M->SetVectorParameterValue(TEXT("Color"), Timber * (k == 0 ? 0.5f : 0.22f));
			M->SetScalarParameterValue(TEXT("Rough"), 0.8f);
			H->SetMaterial(0, M);
		}
		H->RegisterComponent();
		PlankMeshes.Add(H);
	}
	for (int32 i = 0; i < N; ++i)
	{
		const FVector C = PlankCentre(i);
		const FVector Nx = PlankCentre(FMath::Min(N - 1, i + 1)) - PlankCentre(FMath::Max(0, i - 1));
		const FRotator Rot = Nx.Rotation();
		UBoxComponent* B = NewObject<UBoxComponent>(this);
		B->SetupAttachment(RootComponent);
		B->SetBoxExtent(FVector(Len * 0.5f - 2.f, HalfW, 4.f));
		B->SetWorldLocationAndRotation(C, Rot);
		B->SetCollisionProfileName(TEXT("BlockAll"));
		B->SetCanEverAffectNavigation(false);
		B->RegisterComponent();
		PlankBoxes.Add(B);
	}
}

void AWRStormBridge::OnRep_Planks()
{
	if (PlankBoxes.Num() != Planks.Num() || PlankMeshes.Num() < 2) return;
	const float HalfW = WRTune(TEXT("storm.bridgeHalfWidth"), 75.f);
	const float Len = FVector::Dist2D(From, To) / Planks.Num();
	PlankMeshes[0]->ClearInstances();
	PlankMeshes[1]->ClearInstances();
	for (int32 i = 0; i < Planks.Num(); ++i)
	{
		const bool bThere = Planks[i] < 2;
		PlankBoxes[i]->SetCollisionEnabled(bThere ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		if (!bThere) continue;
		const FTransform Tr(PlankBoxes[i]->GetComponentRotation(), PlankBoxes[i]->GetComponentLocation(), FVector((Len - 5.f) / 100.f, HalfW * 2.f / 100.f, 0.06f));
		PlankMeshes[Planks[i] == 1 ? 1 : 0]->AddInstance(Tr, true);
	}
}

int32 AWRStormBridge::GapNear(const FVector& At, float Reach) const
{
	int32 Best = INDEX_NONE;
	float BestD = Reach;
	for (int32 i = 0; i < Planks.Num(); ++i)
	{
		if (Planks[i] != 2) continue;
		const float D = FVector::Dist(At, PlankCentre(i));
		if (D < BestD) { BestD = D; Best = i; }
	}
	return Best;
}

FString AWRStormBridge::Mend(AWRPlayerController* PC, int32 Plank)
{
	AWRPlayerState* PS = PC ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
	if (!PS || !Planks.IsValidIndex(Plank) || Planks[Plank] != 2) return TEXT("Nothing to mend there");
	const int32 Cost = (int32)WRTune(TEXT("storm.plankWood"), 2.f);
	UWRInventoryComponent* Inv = PS->GetInventory();
	if (!Inv || Inv->CountOf(TEXT("wood")) < Cost) return FString::Printf(TEXT("A new plank needs %d wood"), Cost);
	Inv->RemoveItem(TEXT("wood"), Cost);
	Planks[Plank] = 0;
	MendAtHours[Plank] = -1.0;
	OnRep_Planks();
	WRSound::PlayAt(this, TEXT("build.place"), PlankCentre(Plank));
	return TEXT("You lay a new plank across the gap");
}

void AWRStormBridge::OnRep_Lanterns()
{
	int32 i = 0;
	for (UActorComponent* C : GetComponents())
	{
		UPointLightComponent* L = Cast<UPointLightComponent>(C);
		if (!L || !L->ComponentHasTag(TEXT("winch_lantern"))) continue;
		L->SetIntensity((LanternsLit & (1 << i)) ? WRTune(TEXT("storm.lanternLight"), 2600.f) : 0.f);
		++i;
	}
}

void AWRStormBridge::StepLanterns()
{
	// Someone carrying a light past a winch hut lights its lantern (it stays lit for everyone); each one they light
	// counts toward Vesna's "Nine spans".
	const FVector Dir = (FVector(To) - FVector(From)).GetSafeNormal2D();
	const FVector Side = FVector::CrossProduct(Dir, FVector::UpVector);
	UWRWorldSystems* WS = GetWorld()->GetSubsystem<UWRWorldSystems>();
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		AWRPlayerState* PS = It->GetPlayerState<AWRPlayerState>();
		if (!PS || !It->IsAlive()) continue;
		const UWRInventoryComponent* Inv = PS->GetInventory();
		if (!Inv || Inv->GetEquipped(EWREquipSlot::Light).ItemId.IsNone()) continue;
		for (int32 s = 1; s < Spans; ++s)
		{
			const FVector Hut = FMath::Lerp(FVector(From), FVector(To), float(s) / Spans) + Side * 260.f;
			if (FVector::Dist(It->GetActorLocation(), Hut) > WRTune(TEXT("storm.lanternReach"), 450.f)) continue;
			const int32 Bit = 1 << (s - 1);
			if (!(LanternsLit & Bit)) { LanternsLit |= Bit; OnRep_Lanterns(); WRSound::PlayAt(this, TEXT("storm.spark"), Hut); }
			int32& Mine = LitBy.FindOrAdd(PS->GetPlayerName());
			if (!(Mine & Bit)) { Mine |= Bit; if (WS) WS->QuestEvent(PS, TEXT("lantern"), BridgeId); }
		}
	}
}

void AWRStormBridge::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || Planks.Num() == 0) return;
	if (bWinches) StepLanterns();
	const double Now = GetWorld()->GetTimeSeconds();
	const AWRGameState* GS = AWRGameState::Get(this);
	const double Hours = GS ? GS->GetTotalHours() : 0.0;
	bool bChanged = false;
	// Rotten planks someone stood on: creak now, gone a second later.
	CheckClock -= DeltaSeconds;
	if (CheckClock <= 0.f)
	{
		CheckClock = 0.1f;
		for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
		{
			const FVector Feet = It->GetActorLocation() - FVector(0, 0, It->GetSimpleCollisionHalfHeight());
			if (FVector::Dist2D(Feet, FMath::Lerp(FVector(From), FVector(To), 0.5f)) > FVector::Dist2D(From, To) * 0.5f + 300.f) continue;
			for (int32 i = 0; i < Planks.Num(); ++i)
			{
				if (Planks[i] != 1 || BreakAt[i] > 0.0) continue;
				const FVector C = PlankCentre(i);
				if (FVector::Dist2D(Feet, C) < 60.f && FMath::Abs(Feet.Z - C.Z) < 60.f && !It->GetCharacterMovement()->IsFalling())
				{
					BreakAt[i] = Now + WRTune(TEXT("storm.plankBreak"), 1.0f);
					WRSound::PlayAt(this, TEXT("storm.plank_creak"), C);
				}
			}
		}
	}
	for (int32 i = 0; i < Planks.Num(); ++i)
	{
		if (BreakAt[i] > 0.0 && Now >= BreakAt[i])
		{
			BreakAt[i] = -1.0;
			Planks[i] = 2;
			MendAtHours[i] = Hours + WRTune(TEXT("storm.plankMendDays"), 2.f) * 24.0;
			WRSound::PlayAt(this, TEXT("storm.plank_snap"), PlankCentre(i));
			WRFx::Broadcast(this, TEXT("storm.plank_snap"), PlankCentre(i));
			bChanged = true;
		}
		else if (Planks[i] == 2 && MendAtHours[i] > 0.0 && Hours >= MendAtHours[i])
		{
			// Mended by whoever keeps the bridges now: a sound plank, not another rotten one.
			Planks[i] = 0;
			MendAtHours[i] = -1.0;
			bChanged = true;
		}
	}
	if (bChanged) OnRep_Planks();
}

// ================================================================================================ AWRBallLightning

AWRBallLightning::AWRBallLightning()
{
	bReplicates = true;
	SetReplicateMovement(false);
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetNetCullDistanceSquared(FMath::Square(25000.f));
}

void AWRBallLightning::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRBallLightning, Start);
	DOREPLIFETIME(AWRBallLightning, Dir);
	DOREPLIFETIME(AWRBallLightning, BornAt);
}

void AWRBallLightning::BeginPlay()
{
	Super::BeginPlay();
	SetActorLocation(Start);
	if (GetNetMode() != NM_DedicatedServer)
	{
		Fx = WRFx::Attach(RootComponent, TEXT("storm.ball"));
		if (!Light) Light = WRStormLook::Light(this, FVector::ZeroVector, Violet, 6000.f, 900.f);
		WRSound::StartLoop(this, TEXT("storm.ball_hum"));
	}
}

void AWRBallLightning::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bPopped) return;
	// Server time on every machine (it was born at a server time), so the ball is in the same place everywhere.
	const AGameStateBase* GSB = GetWorld()->GetGameState();
	const float Age = (GSB ? GSB->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds()) - BornAt;
	const float Form = WRTune(TEXT("storm.ball.form"), 1.5f);
	const float Speed = WRTune(TEXT("storm.ball.speed"), 200.f);
	// Forms in place for 1.5 s (a hum, the sphere swelling), then straight on at its speed: it never turns.
	const float Moved = FMath::Max(0.f, Age - Form) * Speed;
	const FVector P = FVector(Start) + FVector(Dir) * Moved + FVector(0, 0, 15.f * FMath::Sin(Age * 3.1f));
	SetActorLocation(P);
	if (Fx) Fx->SetRelativeScale3D(FVector(FMath::Clamp(Age / Form, 0.2f, 1.f)));
	if (!HasAuthority()) return;
	if (Age > Form + WRTune(TEXT("storm.ball.life"), 20.f)) { Pop(nullptr); return; }
	if (Age < Form) return;
	const float Touch = WRTune(TEXT("storm.ball.touch"), 70.f);
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		if (It->IsAlive() && FVector::Dist(It->GetActorLocation(), P) < Touch + It->GetSimpleCollisionRadius()) { Pop(*It); return; }
	}
	// An arrow or a spell through it sets it off from range.
	for (TActorIterator<AWRProjectile> It(GetWorld()); It; ++It)
	{
		if (FVector::Dist(It->GetActorLocation(), P) < Touch + 30.f) { Pop(It->GetInstigator()); return; }
	}
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(WRBall), false, this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, P, P + FVector(Dir) * 60.f, ECC_WorldStatic, Q) && !Cast<APawn>(Hit.GetActor())) Pop(nullptr);
}

void AWRBallLightning::Pop(AActor* By)
{
	if (bPopped || !HasAuthority()) return;
	bPopped = true;
	const FVector P = GetActorLocation();
	const float R = WRTune(TEXT("storm.ball.radius"), 300.f);
	WRFx::Broadcast(this, TEXT("storm.ball_pop"), P, FRotator::ZeroRotator, R / 300.f, TEXT("storm.ball_pop"));
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		if (!It->IsAlive() || FVector::Dist(It->GetActorLocation(), P) > R) continue;
		if (WRStorm::ResistsStrikes(*It)) continue;
		FWRHit H;
		H.Damage = WRTune(TEXT("storm.ball.damage"), 40.f);
		H.Element = WRTags::Element_Lightning;
		H.Buildups.Add(TEXT("shock"), WRTune(TEXT("storm.ball.shock"), 60.f));
		H.Origin = P;
		H.bUnblockable = true;
		H.Impact = 2;
		H.Tier = EWRHitTier::Light;
		UWRCombatLibrary::ApplyHit(*It, H);
	}
	SetLifeSpan(0.5f);
}

// ================================================================================================ UWRStormWorld

UWRStormWorld* UWRStormWorld::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UWRStormWorld>() : nullptr;
}

bool UWRStormWorld::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UWRStormWorld::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	const FString Map = UWorld::RemovePIEPrefix(InWorld.GetMapName());
	if (Map != FWRTuning::Get().Str(TEXT("storm.map"), TEXT("L_Island")) || !WRBiome::IsLive(TEXT("storm_peaks"))) return;
	const FWRStormData& D = FWRStormData::Get();
	if (!D.bLoaded) return;
	bActive = true;
	BuildScenery();
	if (InWorld.GetNetMode() != NM_Client) SpawnServerActors();
	UE_LOG(LogWildreach, Log, TEXT("STORM peaks up on %s"), *Map);
}

void UWRStormWorld::BuildScenery()
{
	UWorld* World = GetWorld();
	const FWRStormData& D = FWRStormData::Get();
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	int32 Built = 0;
	const auto Place = [&](const FWRStormData::FSite& S)
	{
		if (S.Kind == TEXT("bridge_nine") || S.Kind == TEXT("rodfield")) return;   // the bridge actor / the rods draw these
		AWRStormScenery* A = World->SpawnActor<AWRStormScenery>(AWRStormScenery::StaticClass(), FTransform(FRotator(0.f, S.Yaw, 0.f), S.At), P);
		if (!A) return;
		A->Kind = S.Kind;
		A->Variant = S.Variant;
		A->Extra = S.Extra;
		A->Extra2 = S.Extra2;
		if (S.Cave == TEXT("halite")) A->Extra.Z = 1.f;
		A->Build();
		Scenery.Add(A);
		++Built;
	};
	for (const FWRStormData::FSite& S : D.Sites) Place(S);
	for (const FWRStormData::FSite& S : D.Kit) Place(S);
	if (AWRStormRods* R = World->SpawnActor<AWRStormRods>(AWRStormRods::StaticClass(), FTransform::Identity, P)) R->Build(D.Rods);
	UE_LOG(LogWildreach, Log, TEXT("STORM scenery: %d places, %d rods"), Built, D.Rods.Num());
}

void UWRStormWorld::SpawnServerActors()
{
	UWorld* World = GetWorld();
	const FWRStormData& D = FWRStormData::Get();
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	State = World->SpawnActor<AWRStormState>(AWRStormState::StaticClass(), FTransform::Identity, P);
	for (const FWRStormData::FBridge& B : D.Bridges)
	{
		AWRStormBridge* Br = World->SpawnActorDeferred<AWRStormBridge>(AWRStormBridge::StaticClass(), FTransform::Identity, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Br) continue;
		Br->BridgeId = B.Id;
		Br->From = B.From;
		Br->To = B.To;
		Br->Spans = B.Spans;
		Br->bWinches = B.bWinches;
		Br->BrokenSpans = B.Broken;
		Br->FinishSpawning(FTransform::Identity);
	}
	// Site caches: what each hand-made site pays (DC 2.4 "Pays"); they fill again after storm.siteRefillHours.
	for (const FWRStormData::FSite& S : D.Sites)
	{
		if (S.Loot.Num() == 0) continue;
		const FTransform Tr(FRotator(0.f, S.Yaw, 0.f), S.At + FVector(150.f, 80.f, 40.f));
		AWRWorldCache* C = World->SpawnActorDeferred<AWRWorldCache>(AWRWorldCache::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!C) continue;
		C->Kind = TEXT("chest");
		C->Biome = TEXT("storm_peaks");
		C->Title = S.Name;
		C->Items = S.Loot;
		C->RefillHours = WRTune(TEXT("storm.siteRefillHours"), 72.f);
		C->FinishSpawning(Tr);
		Caches.Add(C);
	}
	// Kit POIs that pay (DC 2.4 / 2.6): a waystation's niche and a bidental's centre stone hold a thunderstone; a struck
	// pine bleeds resin and leaves charcoal at its foot.
	for (const FWRStormData::FSite& S : D.Kit)
	{
		TMap<FName, int32> Loot;
		FString Title;
		if (S.Kind == TEXT("waystation")) { Loot.Add(TEXT("thunderstone"), 1); Title = TEXT("Waystation niche"); }
		else if (S.Kind == TEXT("bidental")) { Loot.Add(TEXT("thunderstone"), 1); Title = TEXT("Under the centre stone"); }
		else if (S.Kind == TEXT("struck_tree")) { Loot.Add(TEXT("resin"), 3); Loot.Add(TEXT("charcoal"), 2); Title = TEXT("Struck pine"); }
		if (Loot.Num() == 0) continue;
		const FTransform Tr(FRotator(0.f, S.Yaw, 0.f), S.At + FVector(S.Kind == TEXT("waystation") ? -150.f : 60.f, 0.f, 40.f));
		AWRWorldCache* C = World->SpawnActorDeferred<AWRWorldCache>(AWRWorldCache::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!C) continue;
		C->Kind = TEXT("chest");
		C->Biome = TEXT("storm_peaks");
		C->Title = Title;
		C->Items = Loot;
		C->RefillHours = WRTune(TEXT("storm.siteRefillHours"), 72.f);
		C->FinishSpawning(Tr);
		Caches.Add(C);
	}
	// ST-S3, behind the Copperfall: the Rodwardens' smelter cut into the wall behind the falls (walk in behind the water).
	for (const FWRStormData::FSite& S : D.Sites)
	{
		if (S.Kind != TEXT("copperfall")) continue;
		const FVector Lip = S.Extra;
		const FVector Behind = Lip + (Lip - S.At).GetSafeNormal2D() * WRTune(TEXT("storm.smelterBack"), 600.f) - FVector(0, 0, 8000.f);
		FHitResult Hit;
		FVector Spot = Behind;
		if (World->LineTraceSingleByChannel(Hit, Behind + FVector(0, 0, 9000.f), Behind - FVector(0, 0, 4000.f), ECC_WorldStatic)) Spot = Hit.ImpactPoint;
		AWRStormScenery* Sm = World->SpawnActor<AWRStormScenery>(AWRStormScenery::StaticClass(), FTransform(FRotator(0.f, (S.At - Lip).Rotation().Yaw, 0.f), Spot), P);
		if (Sm) { Sm->Kind = TEXT("smelter"); Sm->Build(); }
		const FTransform Tr(FRotator::ZeroRotator, Spot + FVector(0, 0, 40.f));
		if (AWRWorldCache* C = World->SpawnActorDeferred<AWRWorldCache>(AWRWorldCache::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			C->Kind = TEXT("chest");
			C->Biome = TEXT("storm_peaks");
			C->Title = TEXT("The smelter behind the falls");
			C->Items = { { TEXT("copper_ingot"), 6 }, { TEXT("tin_ore"), 4 }, { TEXT("upgrade_shard"), 1 } };
			C->Lore = TEXT("Slag in the hearth, still green. They smelted here where the water hid the smoke.");
			C->RefillHours = WRTune(TEXT("storm.siteRefillHours"), 72.f);
			C->FinishSpawning(Tr);
			Caches.Add(C);
		}
	}
	// ST-S5, the Thunderstone's hollow: lightning on the Arch's hanging stone rings it, and a niche opens in the arch's
	// foot (a strike called down on it: Tempest, Thunder god, the Great Storm).
	if (!StrikeHandle.IsValid())
	{
		TWeakObjectPtr<UWRStormWorld> Self(this);
		StrikeHandle = WRStorm::OnStrikeLanded().AddLambda([Self](UWorld* W, const FWRStrikeCall& Call)
		{
			if (!Self.IsValid() || W != Self->GetWorld() || Self->bHollowOpen) return;
			for (const FWRStormData::FSite& S : FWRStormData::Get().Sites)
			{
				if (S.Kind != TEXT("arch")) continue;
				const FVector Stone = S.At + FVector(0, 0, WRTune(TEXT("storm.archStoneZ"), 1400.f));
				if (FVector::Dist2D(Call.Spot, Stone) > WRTune(TEXT("storm.archStoneReach"), 1000.f)) continue;
				Self->bHollowOpen = true;
				WRFx::Broadcast(W, TEXT("storm.rodflash"), Stone, FRotator::ZeroRotator, 3.f, TEXT("storm.bell_toll"));
				const FTransform Tr(FRotator::ZeroRotator, S.At + FRotator(0.f, S.Yaw, 0.f).RotateVector(FVector(0, 1050.f, 60.f)));
				if (AWRWorldCache* C = W->SpawnActorDeferred<AWRWorldCache>(AWRWorldCache::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
				{
					C->Kind = TEXT("chest");
					C->Biome = TEXT("storm_peaks");
					C->Title = TEXT("The Thunderstone's hollow");
					C->Items = { { TEXT("storm_essence"), 3 }, { TEXT("thunderstone"), 2 } };
					C->Lore = TEXT("The stone rang, and the arch's foot opened: an offering hollow for those who could call the sky.");
					C->FinishSpawning(Tr);
					Self->Caches.Add(C);
				}
			}
		});
	}
	// Caves: the Copper Adit, the Halite Cave, Thundergorge Grotto (AWRCave's Storm look and ore).
	for (const FWRStormData::FSite& S : D.Sites)
	{
		if (S.Cave.IsNone()) continue;
		const FTransform Tr(FRotator(0.f, S.Yaw, 0.f), S.At);
		AWRCave* Cave = World->SpawnActorDeferred<AWRCave>(AWRCave::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Cave) continue;
		Cave->Biome = TEXT("storm_peaks");
		Cave->Theme = S.Cave;
		Cave->FinishSpawning(Tr);
	}
	// Ore at the outcrops (copper rich here: x1.3) and tin; fulgurite fields fill after storms (StepNodes).
	for (const FWRStormData::FSite& S : D.Kit)
	{
		if (S.Kind != TEXT("outcrop")) continue;
		const FTransform Tr(FRotator(0.f, S.Yaw, 0.f), S.At + FVector(0, 0, 20.f));
		AWRResourceNode* N = World->SpawnActorDeferred<AWRResourceNode>(AWRResourceNode::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!N) continue;
		N->ItemId = S.Variant % 2 ? FName(TEXT("tin_ore")) : FName(TEXT("copper_ore"));
		N->PerHit = FMath::RoundToInt(N->PerHit * (S.Variant % 2 ? 1.f : WRTune(TEXT("storm.copperYield"), 1.3f)));
		N->FinishSpawning(Tr);
	}
	for (const FWRStormData::FSite& S : D.Kit) if (S.Kind == TEXT("fulgurite_field")) for (int32 i = 0; i < 4; ++i) SpawnNode(S.At + FVector(FMath::Cos(i * 1.7f) * 400.f, FMath::Sin(i * 1.7f) * 350.f, 0.f), true);
	NextBall = World->GetTimeSeconds() + 10.0;
	NextBellStrike = World->GetTimeSeconds() + 120.0;
}

void UWRStormWorld::Deinitialize()
{
	if (StrikeHandle.IsValid()) WRStorm::OnStrikeLanded().Remove(StrikeHandle);
	StrikeHandle.Reset();
	Super::Deinitialize();
}

bool UWRStormWorld::IsStormOver(const FVector& Pos) const
{
	return WRStorm::InStorm(this, Pos);
}

int32 UWRStormWorld::AddRod(AActor* Owner, const FVector& Tip)
{
	return RodList.Add({ Owner, Tip, 0, true });
}

void UWRStormWorld::RemoveRod(int32 Handle)
{
	if (RodList.IsValidIndex(Handle)) RodList[Handle].bLive = false;
}

int32 UWRStormWorld::RodStrikes(int32 Handle) const
{
	return RodList.IsValidIndex(Handle) ? RodList[Handle].Strikes : 0;
}

bool UWRStormWorld::NearestRod(const FVector& Pos, float Range, FVector& OutTip, int32* OutHandle) const
{
	float Best = Range;
	int32 BestI = INDEX_NONE;
	for (int32 i = 0; i < RodList.Num(); ++i)
	{
		const FRodEntry& R = RodList[i];
		if (!R.bLive) continue;
		const float D = FVector::Dist2D(R.Tip, Pos);
		if (D < Best && FMath::Abs(R.Tip.Z - Pos.Z) < 2500.f) { Best = D; BestI = i; }
	}
	if (BestI == INDEX_NONE) return false;
	OutTip = RodList[BestI].Tip;
	if (OutHandle) *OutHandle = BestI;
	return true;
}

AWRStormBridge* UWRStormWorld::BridgeGapNear(const FVector& At, int32& OutPlank) const
{
	for (TActorIterator<AWRStormBridge> It(GetWorld()); It; ++It)
	{
		const int32 G = It->GapNear(At, WRTune(TEXT("storm.plankReach"), 250.f));
		if (G != INDEX_NONE) { OutPlank = G; return *It; }
	}
	OutPlank = INDEX_NONE;
	return nullptr;
}

bool UWRStormWorld::CallStrike(const FWRStrikeCall& Call)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client || !bActive) return false;
	// Never inside a spawn camp's safe zone; the storm (not a boss, not a player) also spares claims.
	for (TActorIterator<AWRSafeZone> It(World); It; ++It)
	{
		if (FVector::Dist2D(It->GetActorLocation(), Call.Spot) < It->Radius) return false;
	}
	if (Call.Source == EWRStrikeSource::Weather && AWRClaimTotem::FindAt(World, Call.Spot) != nullptr) return false;
	FPending P;
	P.Call = Call;
	const double Now = World->GetTimeSeconds();
	P.TellAt = Now + FMath::Max(0.f, Call.PreTell);
	P.LandsAt = P.TellAt + FMath::Max(0.f, Call.Tell);
	if (!State) State = AWRStormState::Get(this);
	if (State && Call.PreTell > 0.f && Call.Picked.IsValid()) State->MulticastPreTell(Call.Picked.Get(), Call.PreTell);
	Pending.Add(P);
	return true;
}

void UWRStormWorld::Land(const FPending& P)
{
	UWorld* World = GetWorld();
	FWRStrikeCall Call = P.Call;
	// A rod within its shadow takes the bolt (the Rodfield, waymarks, Kharvox's rim rods): the ground under the rod is
	// safe, the rod counts it.
	FVector Tip;
	int32 Handle = INDEX_NONE;
	bool bRod = false;
	if (Call.bRodsTake && NearestRod(Call.Spot, WRTune(TEXT("storm.rodShadow"), 300.f), Tip, &Handle))
	{
		bRod = true;
		RodList[Handle].Strikes++;
		Call.Spot = Tip;
	}
	const FWRWeatherKind* K = UWRWeatherSubsystem::ActiveKind(this, WRBiome::At(this, Call.Spot));
	const bool bWet = Call.bGroundEffects && K && (K->Id == TEXT("thunderstorm") || K->Id == TEXT("rain"));
	if (State) State->MulticastStroke(Call.Spot, bRod, bWet && !bRod);
	if (!bRod)
	{
		for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
		{
			AWRCharacterBase* V = *It;
			if (!V->IsAlive()) continue;
			const FVector L = V->GetActorLocation();
			const float D2 = FVector::Dist2D(L, Call.Spot);
			const bool bDirect = D2 <= Call.Radius && FMath::Abs(L.Z - Call.Spot.Z) < 400.f;
			// The ground current on wet ground: a ring running out at 1,200 cm/s to 4 m; a jump clears it (low).
			const bool bCurrent = !bDirect && bWet && D2 <= WRTune(TEXT("storm.current.radius"), 400.f) && FMath::Abs(L.Z - Call.Spot.Z) < 300.f;
			if (!bDirect && !bCurrent) continue;
			if (Call.Source != EWRStrikeSource::Player && WRStorm::ResistsStrikes(V)) continue;
			if (Call.Source == EWRStrikeSource::Player && Call.Instigator.IsValid() && V == Call.Instigator.Get()) continue;
			if (Call.Source == EWRStrikeSource::Creature && Call.Instigator.IsValid() && V == Call.Instigator.Get()) continue;
			const bool bGrounded = WRStormDetail::IsGrounded(V);
			FWRHit H;
			H.Instigator = Call.Instigator.Get();
			H.Element = WRTags::Element_Lightning;
			H.Origin = Call.Spot + FVector(0, 0, 3000.f);
			H.bUnblockable = true;
			if (bDirect)
			{
				H.Damage = Call.Damage * (bGrounded ? WRTune(TEXT("storm.groundedMult"), 0.5f) : 1.f);
				if (Call.Shock > 0.f) H.Buildups.Add(TEXT("shock"), Call.Shock);
				H.Impact = bGrounded || Call.Stun <= 0.f ? 1 : 3;
				H.PoiseDamage = bGrounded || Call.Stun <= 0.f ? 0.f : 999.f;
				H.Tier = EWRHitTier::Heavy;
			}
			else
			{
				H.Damage = WRTune(TEXT("storm.current.damage"), 15.f);
				H.bLow = true;
				H.Impact = 1;
				H.Tier = EWRHitTier::Light;
				if (V->GetCharacterMovement() && V->GetCharacterMovement()->IsFalling()) continue;   // jumped it
			}
			UWRCombatLibrary::ApplyHit(V, H);
		}
		// A strike on sand or granite may leave fulgurite (35%), most of all in the Great Storm (x3 payout).
		const TCHAR Zone = WRStorm::ZoneAt(this, Call.Spot);
		if (Call.bGroundEffects && Zone >= TCHAR('1') && Zone <= TCHAR('6'))
		{
			const bool bGranite = Call.Spot.Z > WRTune(TEXT("storm.graniteFrom"), 56000.f) || Call.Spot.Y < WRTune(TEXT("storm.sandBelowY"), -525000.f);
			float Chance = WRTune(TEXT("storm.nodeChance"), 0.35f) * (WRStorm::GreatStorm(this) ? WRTune(TEXT("storm.greatStormNodes"), 3.f) : 1.f);
			if (bGranite && FMath::FRand() < FMath::Min(1.f, Chance)) SpawnNode(Call.Spot, false);
		}
	}
	// The picked one lived through the warning (they moved off the spot, or a rod took it): Vesna's "Hair on end".
	if (Call.Source == EWRStrikeSource::Weather && P.Call.Picked.IsValid() && P.Call.Picked->IsAlive()
		&& (bRod || FVector::Dist2D(P.Call.Picked->GetActorLocation(), Call.Spot) > Call.Radius))
	{
		if (AWRPlayerState* PS = P.Call.Picked->GetPlayerState<AWRPlayerState>())
		{
			if (UWRWorldSystems* WS = World->GetSubsystem<UWRWorldSystems>()) WS->QuestEvent(PS, TEXT("pretell"), NAME_None);
		}
	}
	WRStorm::OnStrikeLanded().Broadcast(World, Call);
}

void UWRStormWorld::StepRodRoad(float Dt)
{
	// Vesna's "The rod-road": touch (stand within reach of) the rods along it, each counted once.
	RodRoadClock -= Dt;
	if (RodRoadClock > 0.f) return;
	RodRoadClock = 0.5f;
	const FWRStormData& D = FWRStormData::Get();
	UWRWorldSystems* WS = GetWorld()->GetSubsystem<UWRWorldSystems>();
	if (!WS) return;
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		AWRPlayerState* PS = It->GetPlayerState<AWRPlayerState>();
		if (!PS || !It->IsAlive()) continue;
		for (int32 i = 0; i < D.Rods.Num(); ++i)
		{
			const FWRStormData::FRod& R = D.Rods[i];
			if (R.Route != TEXT("rod_road") || FVector::Dist2D(It->GetActorLocation(), R.Base) > WRTune(TEXT("storm.rodTouch"), 250.f)) continue;
			TSet<int32>& Mine = RodsTouched.FindOrAdd(PS->GetPlayerName());
			if (Mine.Contains(i)) continue;
			Mine.Add(i);
			WS->QuestEvent(PS, TEXT("rodroad"), NAME_None);
			if (AWRPlayerController* PC = Cast<AWRPlayerController>(PS->GetOwner())) WRSound::PlayAt(PC, TEXT("storm.spark"), R.Base + FVector(0, 0, R.Height));
		}
	}
}

void UWRStormWorld::SpawnNode(const FVector& At, bool bField)
{
	UWorld* World = GetWorld();
	FHitResult Hit;
	FVector Spot = At;
	if (World->LineTraceSingleByChannel(Hit, At + FVector(0, 0, 2000.f), At - FVector(0, 0, 4000.f), ECC_WorldStatic)) Spot = Hit.ImpactPoint;
	const FTransform Tr(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), Spot);
	AWRResourceNode* N = World->SpawnActorDeferred<AWRResourceNode>(AWRResourceNode::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!N) return;
	N->ItemId = TEXT("stormglass");
	N->Hits = (int32)WRTune(TEXT("storm.nodeHits"), 2.f);
	N->PerHit = FMath::Max(1, FMath::RoundToInt(WRTune(TEXT("storm.nodeYield"), 3.f) / N->Hits * (bWasStorm ? 1.f : WRTune(TEXT("storm.plentyYield"), 1.5f))));
	N->RespawnSeconds = 1e9f;
	N->FinishSpawning(Tr);
	Nodes.Add({ N, -1.0, bField });
	if (GetWorld()->GetNetMode() != NM_DedicatedServer) WRFx::SpawnFor(this, TEXT("storm.fulgurite"), Spot, FRotator::ZeroRotator, 1.f, 30.f);
}

void UWRStormWorld::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	if (!World) return;
	if (World->GetNetMode() != NM_DedicatedServer) StepLights(DeltaTime);
	if (World->GetNetMode() == NM_Client) return;
	const double Now = World->GetTimeSeconds();
	if (!State) State = AWRStormState::Get(this);
	for (int32 i = Pending.Num() - 1; i >= 0; --i)
	{
		FPending& P = Pending[i];
		if (!P.bTold && Now >= P.TellAt)
		{
			P.bTold = true;
			// The ground glow: on the spot (or the rod's foot it will take), 1.0 s; crimson for a boss's unblockable rain.
			if (State) State->MulticastGroundTell(P.Call.Spot, P.Call.Tell, P.Call.Radius, P.Call.Source == EWRStrikeSource::Creature && P.Call.Damage >= 70.f ? 1 : 0);
		}
		if (Now >= P.LandsAt)
		{
			const FPending Copy = P;
			Pending.RemoveAtSwap(i);
			Land(Copy);
		}
	}
	StepGusts(DeltaTime);
	StepRockfall(DeltaTime);
	StepBalls(DeltaTime);
	StepNodes(DeltaTime);
	StepBellRod(DeltaTime);
	StepRodRoad(DeltaTime);
}

void UWRStormWorld::StepLights(float Dt)
{
	// Night lights (camp fires, hut lamps, the monastery braziers once the Hanging Shrine is lit: DC 2.3 L9) and the
	// storm glow (the Bell Rod's tip, the Arch's chains, the Storm Mill's rod in a storm; the Glass Scar and the Lightning
	// pillar for 2 game hours after one: AWRStormState::GlowUntilHours).
	LightClock -= Dt;
	if (LightClock > 0.f) return;
	LightClock = 1.f;
	UWorld* World = GetWorld();
	const bool bNight = AWRGameState::IsNightIn(World);
	const AWRGameState* GS = AWRGameState::Get(World);
	const double Hours = GS ? GS->GetTotalHours() : 0.0;
	if (!State) State = AWRStormState::Get(this);
	const bool bAfter = State && State->GlowUntilHours > 0.f && Hours < State->GlowUntilHours;
	bool bHangingLit = false;
	for (TActorIterator<AWRShrine> It(World); It; ++It) if (It->ShrineId == TEXT("hanging")) bHangingLit = It->bLit;
	for (const TWeakObjectPtr<AWRStormScenery>& W : Scenery)
	{
		AWRStormScenery* S = W.Get();
		if (!S) continue;
		const bool bMonastery = S->Kind == TEXT("monastery") || S->Kind == TEXT("kitchens");
		const bool bLit = bMonastery ? bHangingLit : bNight;
		for (UPointLightComponent* L : S->NightLights) if (L) L->SetIntensity(bLit ? (bMonastery ? 4000.f : 3000.f) : 0.f);
		if (S->StormGlow.Num() == 0) continue;
		const bool bStorm = WRStorm::InStorm(this, S->GetActorLocation());
		const bool bAfterGlow = S->Kind == TEXT("glass_scar") || S->Kind == TEXT("ring_west");
		const float Glow = (bStorm || (bAfterGlow && bAfter)) ? WRTune(TEXT("storm.glowLight"), 12000.f) * (bStorm ? 0.6f + 0.4f * FMath::FRand() : 1.f) : 0.f;
		for (UPointLightComponent* L : S->StormGlow) if (L) L->SetIntensity(Glow);
	}
}

void UWRStormWorld::StepGusts(float Dt)
{
	// Cliff-path gusts (DC 2.5): on the marked paths, every 8-15 s a 1.0 s rising whistle with dust streaking across,
	// then a 900 cm/s shove for 0.6 s toward the drop. Brace (hold Block, not attacking) x0.3; crouching x0.5; riding a
	// Galeback: nothing. Falls kill.
	UWorld* World = GetWorld();
	const FWRStormData& D = FWRStormData::Get();
	const double Now = World->GetTimeSeconds();
	for (int32 g = 0; g < D.Gusts.Num(); ++g)
	{
		const FWRStormData::FPath& Path = D.Gusts[g];
		// Only paths someone is on.
		TArray<AWRCharacterBase*> On;
		int32 NearPoint = INDEX_NONE;
		for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
		{
			if (!It->IsAlive() || !It->GetPlayerState()) continue;
			for (int32 i = 0; i < Path.Points.Num(); ++i)
			{
				if (FVector::Dist(It->GetActorLocation(), Path.Points[i]) < WRTune(TEXT("storm.gust.width"), 700.f)) { On.Add(*It); NearPoint = i; break; }
			}
		}
		if (On.Num() == 0) { NextGust.Remove(g); continue; }
		double& Next = NextGust.FindOrAdd(g, Now + FMath::FRandRange(WRTune(TEXT("storm.gust.min"), 8.f), WRTune(TEXT("storm.gust.max"), 15.f)));
		if (Now < Next) continue;
		Next = Now + WRTune(TEXT("storm.gust.tell"), 1.0f) + FMath::FRandRange(WRTune(TEXT("storm.gust.min"), 8.f), WRTune(TEXT("storm.gust.max"), 15.f));
		// Toward the drop: across the path, to the side where the ground falls away.
		const FVector A = Path.Points[FMath::Max(0, NearPoint - 1)], B = Path.Points[FMath::Min(Path.Points.Num() - 1, NearPoint + 1)];
		FVector Across = FVector::CrossProduct((B - A).GetSafeNormal(), FVector::UpVector).GetSafeNormal2D();
		FHitResult HL, HR;
		const FVector Mid = Path.Points[NearPoint];
		const float ZL = World->LineTraceSingleByChannel(HL, Mid + Across * 1200.f + FVector(0, 0, 3000.f), Mid + Across * 1200.f - FVector(0, 0, 50000.f), ECC_WorldStatic) ? HL.ImpactPoint.Z : -1e9f;
		const float ZR = World->LineTraceSingleByChannel(HR, Mid - Across * 1200.f + FVector(0, 0, 3000.f), Mid - Across * 1200.f - FVector(0, 0, 50000.f), ECC_WorldStatic) ? HR.ImpactPoint.Z : -1e9f;
		if (ZR < ZL) Across = -Across;
		if (State) State->MulticastGustTell(Mid, Across);
		TArray<TWeakObjectPtr<AWRCharacterBase>> Who;
		for (AWRCharacterBase* C : On) Who.Add(C);
		FTimerHandle H;
		World->GetTimerManager().SetTimer(H, [Who, Across]()
		{
			for (const TWeakObjectPtr<AWRCharacterBase>& W : Who)
			{
				AWRCharacterBase* C = W.Get();
				if (!C || !C->IsAlive()) continue;
				float Mult = 1.f;
				if (WRStormDetail::IsBracing(C)) Mult = WRTune(TEXT("storm.gust.brace"), 0.3f);
				else if (C->bIsCrouched) Mult = WRTune(TEXT("storm.gust.crouch"), 0.5f);
				if (const AWRCreature* Mount = WRStormDetail::MountOf(C))
				{
					if (WRTune(*FString::Printf(TEXT("storm.gust.immune.%s"), *Mount->CreatureId.ToString()), 0.f) > 0.f) continue;
				}
				const float Push = WRTune(TEXT("storm.gust.push"), 900.f) * Mult;
				C->LaunchCharacter(Across * Push * WRTune(TEXT("storm.gust.seconds"), 0.6f) * 1.6f + FVector(0, 0, 40.f), false, false);
			}
		}, WRTune(TEXT("storm.gust.tell"), 1.0f), false);
	}
}

void UWRStormWorld::StepRockfall(float Dt)
{
	// Rockfall under overhangs (DC 2.5): pebbles trickle for 1.2 s, then a 3 m fall: 45 and a knockdown; twice as
	// often in rain. Only near someone.
	UWorld* World = GetWorld();
	const FWRStormData& D = FWRStormData::Get();
	const double Now = World->GetTimeSeconds();
	for (int32 o = 0; o < D.Overhangs.Num(); ++o)
	{
		const FWRStormData::FOverhang& Oh = D.Overhangs[o];
		AWRCharacterBase* Near = nullptr;
		for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
		{
			if (It->IsAlive() && It->GetPlayerState() && FVector::Dist(It->GetActorLocation(), Oh.At) < WRTune(TEXT("storm.rockfall.wake"), 2500.f)) { Near = *It; break; }
		}
		if (!Near) { NextRock.Remove(o); continue; }
		const bool bRain = IsStormOver(Oh.At);
		double& Next = NextRock.FindOrAdd(o, Now + FMath::FRandRange(15.f, 40.f));
		if (Now < Next) continue;
		Next = Now + FMath::FRandRange(WRTune(TEXT("storm.rockfall.min"), 25.f), WRTune(TEXT("storm.rockfall.max"), 60.f)) * (bRain ? 0.5f : 1.f);
		// It falls near the nearest walker's path point, not on them (they read the trickle and step off it).
		const FVector Spot = Oh.At + FVector(FMath::FRandRange(-250.f, 250.f), FMath::FRandRange(-250.f, 250.f), 0.f);
		const float Tell = WRTune(TEXT("storm.rockfall.tell"), 1.2f);
		if (State) State->MulticastRockfall(Spot, Tell);
		TWeakObjectPtr<UWRStormWorld> Self(this);
		FTimerHandle H;
		World->GetTimerManager().SetTimer(H, [Self, Spot]()
		{
			if (!Self.IsValid()) return;
			for (TActorIterator<AWRCharacterBase> It(Self->GetWorld()); It; ++It)
			{
				if (!It->IsAlive() || FVector::Dist2D(It->GetActorLocation(), Spot) > WRTune(TEXT("storm.rockfall.radius"), 300.f) || FMath::Abs(It->GetActorLocation().Z - Spot.Z) > 500.f) continue;
				FWRHit Hit;
				Hit.Damage = WRTune(TEXT("storm.rockfall.damage"), 45.f);
				Hit.Origin = Spot + FVector(0, 0, 800.f);
				Hit.PhysType = 2;
				Hit.Impact = 3;
				Hit.PoiseDamage = 999.f;
				Hit.Tier = EWRHitTier::Heavy;
				UWRCombatLibrary::ApplyHit(*It, Hit);
			}
		}, Tell, false);
	}
}

void UWRStormWorld::StepBalls(float Dt)
{
	// Ball lightning (DC 2.5): in a Thunderstorm on the Struck Crags (and the Cliff Shrines in the Great Storm), now
	// and then one forms at a rod tip near a player and drifts off in a straight line.
	UWorld* World = GetWorld();
	const double Now = World->GetTimeSeconds();
	Balls.RemoveAll([](const TWeakObjectPtr<AWRBallLightning>& B) { return !B.IsValid(); });
	if (Now < NextBall) return;
	NextBall = Now + FMath::FRandRange(WRTune(TEXT("storm.ball.everyMin"), 20.f), WRTune(TEXT("storm.ball.everyMax"), 45.f));
	if (Balls.Num() >= (int32)WRTune(TEXT("storm.ball.max"), 4.f)) return;
	for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
	{
		if (!It->IsAlive() || !It->GetPlayerState()) continue;
		const FVector L = It->GetActorLocation();
		const TCHAR Z = WRStorm::ZoneAt(this, L);
		if (!(Z == TCHAR('4') || (WRStorm::GreatStorm(this) && Z == TCHAR('5'))) || !IsStormOver(L)) continue;
		FVector Tip;
		if (!NearestRod(L + FVector(FMath::FRandRange(-1500.f, 1500.f), FMath::FRandRange(-1500.f, 1500.f), 0.f), 3000.f, Tip)) continue;
		if (FVector::Dist(Tip, L) < 600.f) continue;
		AWRBallLightning* B = World->SpawnActorDeferred<AWRBallLightning>(AWRBallLightning::StaticClass(), FTransform(Tip), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!B) continue;
		// It drifts in a fixed direction chosen at random, never aimed at anyone (it never turns toward you).
		const FVector Dir = FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f).Vector();
		B->Start = Tip - FVector(0, 0, FMath::FRandRange(100.f, 300.f));
		B->Dir = Dir;
		B->BornAt = World->GetGameState() ? World->GetGameState()->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
		B->FinishSpawning(FTransform(Tip));
		Balls.Add(B);
		break;
	}
}

void UWRStormWorld::StepNodes(float Dt)
{
	// After a storm (Plenty): fulgurite fields refill, nodes from strikes last until 2 game hours after it ended, the
	// Glass Scar and the rod feet glow for those 2 hours. In clear weather nothing new appears.
	UWorld* World = GetWorld();
	const FWRStormData& D = FWRStormData::Get();
	const AWRGameState* GS = AWRGameState::Get(this);
	const double Hours = GS ? GS->GetTotalHours() : 0.0;
	const bool bStorm = WRStorm::InStorm(this, FVector(-120000.f, -330000.f, 80000.f));
	if (bWasStorm && !bStorm)
	{
		const double Gone = Hours + WRTune(TEXT("storm.nodeHoursAfter"), 2.f);
		for (FNode& N : Nodes) if (!N.bField && N.GoneAtHours < 0.0) N.GoneAtHours = Gone;
		if (State) State->GlowUntilHours = Gone;
		for (const FWRStormData::FSite& S : D.Kit)
		{
			if (S.Kind != TEXT("fulgurite_field")) continue;
			int32 Have = 0;
			for (const FNode& N : Nodes) if (N.bField && N.Node.IsValid() && FVector::Dist2D(N.Node->GetActorLocation(), S.At) < 900.f) ++Have;
			for (int32 i = Have; i < (int32)WRTune(TEXT("storm.fieldNodes"), 5.f); ++i) SpawnNode(S.At + FVector(FMath::FRandRange(-500.f, 500.f), FMath::FRandRange(-500.f, 500.f), 0.f), true);
		}
		// The Rodfield: one node per six rods at the rod feet; the Glass Scar crust.
		int32 RodN = 0;
		for (const FWRStormData::FRod& R : D.Rods) if (R.Kind == TEXT("field") && (++RodN % 6) == 0) SpawnNode(R.Base + FVector(60.f, 0, 0), false);
		for (const FWRStormData::FSite& S : D.Sites) if (S.Kind == TEXT("glass_scar")) for (int32 i = 0; i < 5; ++i) SpawnNode(FMath::Lerp(S.Extra, S.Extra2, (i + 0.5f) / 5.f), false);
	}
	bWasStorm = bStorm;
	for (int32 i = Nodes.Num() - 1; i >= 0; --i)
	{
		FNode& N = Nodes[i];
		if (!N.Node.IsValid()) { Nodes.RemoveAtSwap(i); continue; }
		if (N.GoneAtHours > 0.0 && Hours >= N.GoneAtHours) { N.Node->Destroy(); Nodes.RemoveAtSwap(i); }
	}
	for (const TWeakObjectPtr<AWRWorldCache>& C : Caches)
	{
		if (C.IsValid() && C->bTaken && C->RefillAtHours > 0.0 && Hours >= C->RefillAtHours) C->Refill();
	}
}

void UWRStormWorld::StepBellRod(float Dt)
{
	// L2: the Bell Rod takes a strike in every storm (its bells ring after); the Storm Mill's rod sparks.
	UWorld* World = GetWorld();
	const double Now = World->GetTimeSeconds();
	if (Now < NextBellStrike) return;
	NextBellStrike = Now + FMath::FRandRange(WRTune(TEXT("storm.bellRodEveryMin"), 90.f), WRTune(TEXT("storm.bellRodEveryMax"), 180.f));
	for (const FWRStormData::FSite& S : FWRStormData::Get().Sites)
	{
		if (S.Kind != TEXT("bell_rod") || !IsStormOver(S.At)) continue;
		FWRStrikeCall C;
		C.Spot = S.At + FVector(0, 0, 180.f + WRTune(TEXT("storm.bellRodHeight"), 6000.f));
		C.Tell = 1.f;
		C.bRodsTake = false;
		C.bGroundEffects = false;
		C.Damage = 0.f;
		C.Radius = 50.f;
		CallStrike(C);
		FTimerHandle H;
		const FVector Bells = S.At + FVector(0, 0, 3000.f);
		TWeakObjectPtr<AWRStormState> St(State);
		World->GetTimerManager().SetTimer(H, [St, Bells]() { if (St.IsValid()) WRFx::Broadcast(St.Get(), TEXT("storm.rodflash"), Bells, FRotator::ZeroRotator, 2.f, TEXT("storm.bell_toll")); }, 1.6f, false);
	}
}
