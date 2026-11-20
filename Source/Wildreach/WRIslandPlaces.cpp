// The Fire and Frost places on the Old Island (cooks/cookpass-world.md): loading the places file, the sub-zones, the
// deferred builds (each place waits for the ground under it), the places' clock-driven looks (night lights, the Bellows'
// breath, the waymarks' ember cups, the Slag Light's brazier, glowmoths) and the landmarks' map entries.
#include "WRIslandPlaces.h"
#include "WRIslandPlacesLook.h"
#include "Wildreach.h"
#include "WRFx.h"
#include "WRGameState.h"
#include "WRSound.h"
#include "WRTuning.h"
#include "WRLand.h"
#include "WRWorldDressing.h"
#include "WRWorldSystems.h"
#include "WRBuildLook.h"
#include "WRPortalsInternal.h"
#include "Components/BoxComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "LandscapeProxy.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"

namespace WRIslePlacesImpl
{
	struct FIsleZone
	{
		FName Id, Biome;
		FString Name;
		FLinearColor Colour = FLinearColor::White;
		bool bRing = false;
		FVector4 Shape = FVector4(0, 0, 0, 0);   // ring: cx, cy, r0, r1 (cm); rect: x0, y0, x1, y1 (cm)
	};
	TArray<FIsleZone>& IsleZones() { static TArray<FIsleZone> Z; return Z; }
	TMap<FString, FString>& IsleLines() { static TMap<FString, FString> L; return L; }

	float IsleNum(const TSharedPtr<FJsonObject>& O, const TCHAR* K, float F)
	{
		double V = 0.0;
		return O.IsValid() && O->TryGetNumberField(K, V) ? float(V) : F;
	}
	FString IsleStr(const TSharedPtr<FJsonObject>& O, const TCHAR* K)
	{
		FString S;
		if (O.IsValid()) O->TryGetStringField(K, S);
		return S;
	}
	bool IslePt(const TSharedPtr<FJsonObject>& O, const TCHAR* K, FVector2D& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (!O.IsValid() || !O->TryGetArrayField(K, A) || A->Num() < 2) return false;
		Out = FVector2D((*A)[0]->AsNumber() * 100.0, (*A)[1]->AsNumber() * 100.0);
		return true;
	}

	/** The landscape's height under a world point (cm), ignoring everything built on it; false until it streams in. */
	bool IsleLandZ(UWorld* World, const FVector2D& P, float& OutZ)
	{
		// On a map with lands of its own (the round island) the ground is the land's height there: its collision is built
		// only near players, and a place far off must still build.
		if (WRLand::OwnLandMap())
		{
			const float LZ = WRLand::GroundZAt(FVector(P, 0.f), TNumericLimits<float>::Lowest());
			if (LZ > TNumericLimits<float>::Lowest()) { OutZ = LZ; return true; }
			return false;
		}
		TArray<FHitResult> Hits;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRIsleGround), false);
		World->LineTraceMultiByObjectType(Hits, FVector(P, 200000.f), FVector(P, -30000.f), FCollisionObjectQueryParams(ECC_WorldStatic), Q);
		for (const FHitResult& H : Hits)
		{
			if (H.GetActor() && H.GetActor()->IsA<ALandscapeProxy>())
			{
				OutZ = float(H.ImpactPoint.Z);
				return true;
			}
		}
		return false;
	}

	/** The local survivor (a client's view of the world; null on a dedicated server). */
	APawn* IsleLocalPawn(const UWorld* World)
	{
		const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		return PC && PC->IsLocalController() ? PC->GetPawn() : nullptr;
	}
}
using namespace WRIslePlacesImpl;

// ============================================================================================================ WRIsle

bool WRIsle::Live(const UObject* WorldContext)
{
	const UWRIslandPlaces* S = UWRIslandPlaces::Get(WorldContext);
	return S && S->IsLive();
}

FName WRIsle::ZoneAt(const FVector& At)
{
	// The round island: the land's own sub-zone there (V1..V7, T1..T7), when the places file names it.
	if (WRLand::OwnLandMap())
	{
		const FWRLandDef* L = WRLand::At(At);
		const FWRLandZone* LZ = L ? L->ZoneAt(L->ToMap(At)) : nullptr;
		if (LZ) for (const FIsleZone& Z : IsleZones()) if (Z.Id == LZ->Id) return Z.Id;
		return NAME_None;
	}
	for (const FIsleZone& Z : IsleZones())
	{
		if (Z.bRing)
		{
			const float D = FVector2D::Distance(FVector2D(At), FVector2D(Z.Shape.X, Z.Shape.Y));
			if (D >= Z.Shape.Z && D <= Z.Shape.W) return Z.Id;
		}
		else if (At.X >= Z.Shape.X && At.Y >= Z.Shape.Y && At.X <= Z.Shape.Z && At.Y <= Z.Shape.W)
		{
			// Only on the Old Island itself (the lands north and south of it have their own zones).
			const float North = WRTune(TEXT("world.islandNorthEdge"), -201600.f), South = WRTune(TEXT("world.islandSouthEdge"), 201600.f);
			if (At.Y < North || At.Y > South) return NAME_None;
			return Z.Id;
		}
	}
	return NAME_None;
}

FString WRIsle::ZoneName(FName Zone)
{
	for (const FIsleZone& Z : IsleZones()) if (Z.Id == Zone) return Z.Name;
	return FString();
}

FName WRIsle::ZoneBiome(FName Zone)
{
	for (const FIsleZone& Z : IsleZones()) if (Z.Id == Zone) return Z.Biome;
	return NAME_None;
}

FLinearColor WRIsle::ZoneColour(FName Zone)
{
	for (const FIsleZone& Z : IsleZones()) if (Z.Id == Zone) return Z.Colour;
	return FLinearColor::White;
}

FString WRIsle::LandmarkLine(const FString& Name)
{
	const FString* L = IsleLines().Find(Name);
	return L ? *L : FString();
}

double WRIsle::Now(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	return GS ? GS->GetServerWorldTimeSeconds() : World ? World->GetTimeSeconds() : 0.0;
}

// ============================================================================================================ actors

AWRIslandPlacesActor::AWRIslandPlacesActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

AWRIsleState::AWRIsleState()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(4.f);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRIsleState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRIsleState, Breaks);
}

void AWRIsleState::OnRep_Breaks()
{
	UWRIslandPlaces* S = UWRIslandPlaces::Get(this);
	if (!S) return;
	S->SetState(this);
	for (const FWRIsleBreak& B : Breaks) S->OnBreak(B);
}

void AWRIsleState::Set(int32 Key, uint8 InState, float At)
{
	FWRIsleBreak* B = Breaks.FindByPredicate([Key](const FWRIsleBreak& Q) { return Q.Key == Key; });
	if (!B)
	{
		B = &Breaks.AddDefaulted_GetRef();
		B->Key = Key;
	}
	B->State = InState;
	B->At = At;
	ForceNetUpdate();
	if (UWRIslandPlaces* S = UWRIslandPlaces::Get(this)) S->OnBreak(*B);
}

// ============================================================================================================ the site

FWRIsleSite::FWRIsleSite(UWRIslandPlaces* InSys, AWRIslandPlacesActor* InOwner, const FTransform& InWorld, bool bInDraw, int32 InKey)
	: Sys(InSys), Owner(InOwner), World(InWorld), K(World, Blocks), bDraw(bInDraw), Key(InKey)
{
	Root = NewObject<USceneComponent>(Owner);
	Root->SetupAttachment(Owner->GetRootComponent());
	Root->SetWorldTransform(World);
	Root->RegisterComponent();
}

float FWRIsleSite::Gz(float X, float Y)
{
	// Remembered per metre of the place's frame (the builders ask the same spots many times).
	const FIntPoint Cell(FMath::RoundToInt(X / 100.f), FMath::RoundToInt(Y / 100.f));
	if (const float* Z = GroundCache.Find(Cell)) return *Z;
	const FVector W = World.TransformPosition(FVector(Cell.X * 100.f, Cell.Y * 100.f, 0.f));
	float Z = World.GetLocation().Z;
	IsleLandZ(Owner->GetWorld(), FVector2D(W), Z);
	const float Out = Z - float(World.GetLocation().Z);
	GroundCache.Add(Cell, Out);
	return Out;
}

UBoxComponent* FWRIsleSite::Solid(const FVector& Centre, const FVector& Half, const FRotator& Rot)
{
	UBoxComponent* B = WRFoldLook::Block(Owner, Root, Centre, Half, Rot);
	if (B) B->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	return B;
}

UPointLightComponent* FWRIsleSite::Lamp(const FVector& At, const FLinearColor& Colour, float Intensity, float Radius, bool bAlways)
{
	if (!bDraw) return nullptr;
	UPointLightComponent* L = WRFoldLook::Light(Owner, Root, At, Colour, bAlways ? Intensity : 0.f, Radius);
	if (!L) return nullptr;
	L->SetMobility(EComponentMobility::Movable);
	L->MaxDrawDistance = WRTune(TEXT("places.lightDistance"), 25000.f);
	L->MaxDistanceFadeRange = 3000.f;
	if (!bAlways)
	{
		FWRIsleAnim& A = Sys->Anims.AddDefaulted_GetRef();
		A.Kind = FWRIsleAnim::EKind::NightLight;
		A.Light = L;
		A.Base = Intensity;
		A.Phase = R(At, 91) * 6.f;
	}
	return L;
}

USceneComponent* FWRIsleSite::Fx(const FVector& At, FName FxRow, float Scale)
{
	if (!bDraw) return nullptr;
	USceneComponent* F = WRFx::Attach(Root, FxRow, NAME_None, Scale);
	if (F) F->SetRelativeLocation(At);
	return F;
}

void FWRIsleSite::NightFx(const FVector& At, FName FxRow, float Scale)
{
	if (!bDraw) return;
	FWRIsleAnim& A = Sys->Anims.AddDefaulted_GetRef();
	A.Kind = FWRIsleAnim::EKind::NightFx;
	A.At = ToWorld(At);
	A.Name = FxRow;
	A.Base = Scale;
}

void FWRIsleSite::Breath(const FVector& At, FName FxRow, float Period, float Phase, float Length, float Intensity, float Radius, const FLinearColor& Colour)
{
	if (!bDraw) return;
	FWRIsleAnim& A = Sys->Anims.AddDefaulted_GetRef();
	A.Kind = FWRIsleAnim::EKind::Breath;
	A.Light = WRFoldLook::Light(Owner, Root, At, Colour, 0.f, Radius);
	if (A.Light.IsValid()) A.Light->MaxDrawDistance = WRTune(TEXT("places.breathLightDistance"), 60000.f);
	A.At = ToWorld(At);
	A.Name = FxRow;
	A.Base = Intensity;
	A.Period = Period;
	A.Phase = Phase;
	A.Length = Length;
}

void FWRIsleSite::PassCup(const FVector& At, const FLinearColor& Colour, float Intensity, float Radius)
{
	if (!bDraw) return;
	FWRIsleAnim& A = Sys->Anims.AddDefaulted_GetRef();
	A.Kind = FWRIsleAnim::EKind::PassCup;
	A.Light = WRFoldLook::Light(Owner, Root, At, Colour, 0.f, Radius);
	if (A.Light.IsValid()) A.Light->MaxDrawDistance = WRTune(TEXT("places.lightDistance"), 25000.f);
	A.At = ToWorld(At);
	A.Base = Intensity;
}

void FWRIsleSite::ShrineLight(const FVector& At, FName Shrine, float Intensity, float Radius)
{
	if (!bDraw) return;
	FWRIsleAnim& A = Sys->Anims.AddDefaulted_GetRef();
	A.Kind = FWRIsleAnim::EKind::ShrineLight;
	A.Light = WRFoldLook::Light(Owner, Root, At, WRIsleLook::Lin(255, 150, 70), 0.f, Radius);
	if (A.Light.IsValid()) A.Light->MaxDrawDistance = WRTune(TEXT("places.beaconDistance"), 400000.f);
	A.At = ToWorld(At);
	A.Name = Shrine;
	A.Base = Intensity;
	// Dark until the first check finds the shrine lit (Period: wanted 0/1, Phase: shown, Length: seconds to the next check).
	A.Period = 0.f;
	A.Phase = 0.f;
	A.Length = 0.f;
}

void FWRIsleSite::Glowmoth(const FVector& At, float Phase)
{
	if (!bDraw) return;
	FWRIsleAnim& A = Sys->Anims.AddDefaulted_GetRef();
	A.Kind = FWRIsleAnim::EKind::Glowmoth;
	A.Light = WRFoldLook::Light(Owner, Root, At, WRIsleLook::Lin(214, 240, 190), 0.f, WRTune(TEXT("places.glowmothRadius"), 600.f));
	if (A.Light.IsValid()) A.Light->MaxDrawDistance = WRTune(TEXT("places.lightDistance"), 25000.f);
	A.Base = WRTune(TEXT("places.glowmothLux"), 400.f);
	A.Phase = Phase;
	A.Period = 4.f + Phase;
}

void FWRIsleSite::Rock(int32 Which, const FVector& Size, const FVector& Base, const FRotator& Rot, const FLinearColor& Tint, bool bCollide, float Sink)
{
	if (!bDraw && !bCollide) return;
	const FString TintKey = Tint.ToString();
	UMaterialInterface* M = nullptr;
	if (UMaterialInterface** Found = RockMats.Find(TintKey)) M = *Found;
	else
	{
		M = WRFoldLook::RockMat(Owner, Tint);
		RockMats.Add(TintKey, M);
	}
	if (UStaticMeshComponent* C = WRFoldLook::Rock(Owner, Root, Which, Size, Base, Rot, M, bCollide, Sink))
	{
		C->SetCullDistance(WRTune(TEXT("places.siteCull"), 60000.f));
		C->SetCachedMaxDrawDistance(WRTune(TEXT("places.siteCull"), 60000.f));
		if (!bDraw) C->SetHiddenInGame(true);
		C->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}
}

void FWRIsleSite::Meshes(const TCHAR* Path, const TArray<FTransform>& Local, float Height, float Cull, const FLinearColor* Coat)
{
	if (!bDraw || Local.Num() == 0) return;
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, Path);
	if (!Mesh) return;   // the pack isn't here: the place keeps its built parts
	UHierarchicalInstancedStaticMeshComponent* H = NewObject<UHierarchicalInstancedStaticMeshComponent>(Owner);
	H->SetStaticMesh(Mesh);
	H->SetupAttachment(Root);
	H->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	H->SetCullDistances(0, int32(Cull));
	H->SetCachedMaxDrawDistance(Cull);
	H->SetCanEverAffectNavigation(false);
	if (Coat)
	{
		for (int32 Slot = 0; Slot < Mesh->GetStaticMaterials().Num(); ++Slot)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Mesh->GetMaterial(Slot), Owner);
			MID->SetVectorParameterValue(TEXT("Snow Color"), *Coat);
			H->SetMaterial(Slot, MID);
		}
	}
	H->RegisterComponent();
	const FBox B = Mesh->GetBoundingBox();
	const float Fit = Height / FMath::Max(1.f, float(B.GetSize().Z));
	TArray<FTransform> Out;
	for (const FTransform& T : Local)
	{
		const float S = Fit * float(T.GetScale3D().Z);
		Out.Emplace(T.GetRotation(), T.GetLocation() + FVector(0.f, 0.f, -B.Min.Z * S), FVector(S));
	}
	H->AddInstances(Out, false, false);
}

FWRIsleHazard& FWRIsleSite::Hazard(FWRIsleHazard::EKind Kind, const FVector& LocalAt, float Radius)
{
	FWRIsleHazard& H = Sys->Hazards.AddDefaulted_GetRef();
	H.Kind = Kind;
	H.Key = Key * 64 + (HazardCount++);
	H.At = ToWorld(LocalAt);
	H.Radius = Radius;
	H.Phase = double(FWRBuildLook::Hash01(H.At, 37)) * 1000.0;
	return H;
}

void FWRIsleSite::Done(float Cull)
{
	if (!bDraw || Blocks.Num() == 0)
	{
		Blocks.Reset();
		return;
	}
	const int32 Before = Root->GetNumChildrenComponents();
	FWRBuildLook::Spawn(Owner, Root, Blocks, WRIsleLook::Lin(163, 158, 149), false);
	for (int32 i = Before; i < Root->GetNumChildrenComponents(); ++i)
	{
		if (UInstancedStaticMeshComponent* I = Cast<UInstancedStaticMeshComponent>(Root->GetChildComponent(i)))
		{
			I->SetCullDistances(0, int32(Cull));
			I->SetCachedMaxDrawDistance(Cull);
			I->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	Blocks.Reset();
}

bool WRIsleLook::RowPoint(const FWRIsleSite& S, const TCHAR* Obj, const TCHAR* Field, FVector2D& Out)
{
	if (!S.Row.IsValid()) return false;
	if (!Obj) return IslePt(S.Row, Field, Out);
	const TSharedPtr<FJsonObject>* O = nullptr;
	return S.Row->TryGetObjectField(Obj, O) && IslePt(*O, Field, Out);
}

bool WRIsleLook::Build(FWRIsleSite& S, const FString& Kind)
{
	return BuildVolcanic(S, Kind) || BuildTundra(S, Kind) || BuildKit(S, Kind);
}

// ============================================================================================================ the subsystem

bool UWRIslandPlaces::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

UWRIslandPlaces* UWRIslandPlaces::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UWRIslandPlaces>() : nullptr;
}

TStatId UWRIslandPlaces::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWRIslandPlaces, STATGROUP_Tickables);
}

void UWRIslandPlaces::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	FString Map = InWorld.GetMapName();
	Map.RemoveFromStart(InWorld.StreamingLevelsPrefix);
	// The Old Island's places, or a map's own (the round island: Data/wildreach_places.json, made by
	// Tools/terrain/fire_frost_dressing.mjs).
	PlacesFile = Map == TEXT("L_Island") ? FString(TEXT("island_places.json")) : (Map.StartsWith(TEXT("L_")) ? Map.Mid(2).ToLower() : Map.ToLower()) + TEXT("_places.json");
	if (WRTune(TEXT("places.enabled"), 1.f) <= 0.f || !FWRTuning::LoadDataFile(PlacesFile).IsValid()) return;
	bServer = InWorld.GetNetMode() != NM_Client;
	bDraw = InWorld.GetNetMode() != NM_DedicatedServer;
	Load();
	if (!bLive) return;
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Owner = InWorld.SpawnActor<AWRIslandPlacesActor>(AWRIslandPlacesActor::StaticClass(), FTransform::Identity, P);
	if (bServer) State = InWorld.SpawnActor<AWRIsleState>(AWRIsleState::StaticClass(), FTransform::Identity, P);
	if (!Owner.IsValid()) bLive = false;
}

void UWRIslandPlaces::Load()
{
	const TSharedPtr<FJsonObject> File = FWRTuning::LoadDataFile(PlacesFile);
	if (!File.IsValid())
	{
		UE_LOG(LogWildreach, Warning, TEXT("PLACES: no Data/%s; the Fire and Frost places aren't built"), *PlacesFile);
		return;
	}
	IsleZones().Reset();
	IsleLines().Reset();
	Pending.Reset();
	Hazards.Reset();
	Anims.Reset();
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (File->TryGetArrayField(TEXT("zones"), Arr))
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			FIsleZone& Z = IsleZones().AddDefaulted_GetRef();
			Z.Id = FName(*IsleStr(O, TEXT("id")));
			Z.Name = IsleStr(O, TEXT("name"));
			Z.Biome = FName(*IsleStr(O, TEXT("biome")));
			Z.Colour = FLinearColor(FColor::FromHex(IsleStr(O, TEXT("colour"))));
			const TArray<TSharedPtr<FJsonValue>>* S = nullptr;
			if (O->TryGetArrayField(TEXT("ring"), S) && S->Num() >= 4)
			{
				Z.bRing = true;
				Z.Shape = FVector4((*S)[0]->AsNumber() * 100.0, (*S)[1]->AsNumber() * 100.0, (*S)[2]->AsNumber() * 100.0, (*S)[3]->AsNumber() * 100.0);
			}
			else if (O->TryGetArrayField(TEXT("rect"), S) && S->Num() >= 4)
			{
				Z.Shape = FVector4((*S)[0]->AsNumber() * 100.0, (*S)[1]->AsNumber() * 100.0, (*S)[2]->AsNumber() * 100.0, (*S)[3]->AsNumber() * 100.0);
			}
		}
	}
	// Every place gets a key from its row (the same on every machine): places 1.., waymarks 1000.., kit 2000..,
	// hazard spots 5000.. (a hazard's own key is the place key x 64 + its index).
	const auto Queue = [this](const FString& Kind, int32 Index, const TSharedPtr<FJsonObject>& O, bool bLandmark)
	{
		FPending& P = Pending.AddDefaulted_GetRef();
		P.Kind = Kind;
		P.Index = Index;
		FVector2D At;
		IslePt(O, TEXT("at"), At);
		P.At = At;
		P.Yaw = IsleNum(O, TEXT("yaw"), 0.f);
		P.bLandmark = bLandmark;
		P.Row = O;
		P.Variant = int32(IsleNum(O, TEXT("v"), 0.f));
	};
	if (File->TryGetArrayField(TEXT("places"), Arr))
	{
		for (int32 i = 0; i < Arr->Num(); ++i)
		{
			const TSharedPtr<FJsonObject> O = (*Arr)[i]->AsObject();
			bool bLandmark = false;
			O->TryGetBoolField(TEXT("landmark"), bLandmark);
			Queue(IsleStr(O, TEXT("kind")), 1 + i, O, bLandmark);
			const FString Line = IsleStr(O, TEXT("line"));
			if (bLandmark && !Line.IsEmpty()) IsleLines().Add(IsleStr(O, TEXT("name")), Line);
		}
	}
	if (File->TryGetArrayField(TEXT("roads"), Arr))
	{
		int32 n = 0;
		for (const TSharedPtr<FJsonValue>& R : *Arr)
		{
			const TArray<TSharedPtr<FJsonValue>>* Marks = nullptr;
			if (!R->AsObject()->TryGetArrayField(TEXT("marks"), Marks)) continue;
			for (const TSharedPtr<FJsonValue>& M : *Marks) Queue(IsleStr(M->AsObject(), TEXT("kind")), 1000 + n++, M->AsObject(), false);
		}
	}
	// Ground features (the sastrugi fields): keys 4000.., built like the kit.
	if (File->TryGetArrayField(TEXT("ground"), Arr))
	{
		for (int32 i = 0; i < Arr->Num(); ++i) Queue(IsleStr((*Arr)[i]->AsObject(), TEXT("kind")), 4000 + i, (*Arr)[i]->AsObject(), false);
	}
	if (File->TryGetArrayField(TEXT("kit"), Arr))
	{
		for (int32 i = 0; i < Arr->Num(); ++i) Queue(IsleStr((*Arr)[i]->AsObject(), TEXT("kind")), 2000 + i, (*Arr)[i]->AsObject(), false);
	}
	const TSharedPtr<FJsonObject>* Hz = nullptr;
	if (File->TryGetObjectField(TEXT("hazards"), Hz))
	{
		const TSharedPtr<FJsonObject>* Kat = nullptr;
		if ((*Hz)->TryGetObjectField(TEXT("katabatic"), Kat))
		{
			IslePt(*Kat, TEXT("centre"), GustCentre);
			GustInner = IsleNum(*Kat, TEXT("inner"), 0.f) * 100.f;
			GustOuter = IsleNum(*Kat, TEXT("outer"), 0.f) * 100.f;
		}
		int32 n = 0;
		for (const TCHAR* Kind : { TEXT("thinIce"), TEXT("fumaroles"), TEXT("geysers") })
		{
			if (!(*Hz)->TryGetArrayField(Kind, Arr)) continue;
			const FString K = FString(Kind) == TEXT("thinIce") ? TEXT("thin_ice") : FString(Kind) == TEXT("fumaroles") ? TEXT("fumarole") : TEXT("geyser");
			for (const TSharedPtr<FJsonValue>& V : *Arr) Queue(K, 5000 + n++, V->AsObject(), false);
		}
	}
	bLive = Pending.Num() > 0;
	UE_LOG(LogWildreach, Log, TEXT("PLACES: %d Fire and Frost places to build, %d sub-zones"), Pending.Num(), IsleZones().Num());
}

bool UWRIslandPlaces::BuildOne(const FPending& P)
{
	UWorld* World = GetWorld();
	float Z = 0.f;
	if (!IsleLandZ(World, P.At, Z)) return false;   // its ground hasn't streamed in yet
	const FTransform T(FRotator(0.f, P.Yaw, 0.f), FVector(P.At, Z));
	const int32 HazardsBefore = Hazards.Num();
	FWRIsleSite S(this, Owner.Get(), T, bDraw, P.Index);
	S.Row = P.Row;
	S.Variant = P.Variant;
	if (!WRIsleLook::Build(S, P.Kind))
	{
		UE_LOG(LogWildreach, Warning, TEXT("PLACES: no builder for kind '%s'"), *P.Kind);
		return true;
	}
	// A client that built this place late: its people-reacting hazards take the state the server already sent.
	if (const AWRIsleState* St = State.Get())
	{
		for (int32 h = HazardsBefore; h < Hazards.Num(); ++h)
		{
			if (const FWRIsleBreak* B = St->Find(Hazards[h].Key)) OnBreak(*B);
		}
	}
	if (P.bLandmark)
	{
		// The fill-in map and the discovery banner know the landmarks (kind "landmark"); kit places stay off the compass.
		FWRPoi& Poi = FWRPoi::All.AddDefaulted_GetRef();
		Poi.Name = IsleStr(P.Row, TEXT("name"));
		Poi.Kind = TEXT("landmark");
		Poi.Location = T.GetLocation();
	}
	++Built;
	return true;
}

void UWRIslandPlaces::Tick(float DeltaTime)
{
	if (!bLive || !Owner.IsValid()) return;
	// Build what's waiting, a few a frame, each once the ground under it is there (a pass that finds nothing ready waits
	// half a second before trying again).
	RetryIn -= DeltaTime;
	if (Pending.Num() > 0 && RetryIn <= 0.f)
	{
		const double Budget = WRTune(TEXT("places.buildMsPerFrame"), 4.f) / 1000.0;
		const double Start = FPlatformTime::Seconds();
		int32 Done = 0;
		bool bAll = true;
		for (int32 i = 0; i < Pending.Num(); )
		{
			if (FPlatformTime::Seconds() - Start >= Budget) { bAll = false; break; }
			if (BuildOne(Pending[i])) { Pending.RemoveAt(i, EAllowShrinking::No); ++Done; }
			else ++i;
		}
		if (bAll && Done == 0) RetryIn = 0.5f;
		if (Pending.Num() == 0 && Built > 0 && !bPoisAdded)
		{
			bPoisAdded = true;
			UE_LOG(LogWildreach, Log, TEXT("PLACES: built %d places, %d hazards, %d lights and motions"), Built, Hazards.Num(), Anims.Num());
		}
	}
	if (bServer) StepServer(DeltaTime);
	if (bDraw)
	{
		StepClient(DeltaTime);
		TickLooks(DeltaTime);
	}
}

void UWRIslandPlaces::TickLooks(float Dt)
{
	LookAccum += Dt;
	if (LookAccum < 1.f / 30.f) return;
	const float Step = LookAccum;
	LookAccum = 0.f;
	UWorld* World = GetWorld();
	const AWRGameState* GS = World->GetGameState<AWRGameState>();
	const float Daylight = GS ? GS->GetDaylight() : 1.f;
	const float NightOn = WRTune(TEXT("places.nightFrom"), 0.3f);
	// 0 by day, 1 by night, eased over the dusk (the lights come up as the land darkens).
	const float Night = FMath::Clamp((NightOn - Daylight) / FMath::Max(0.05f, NightOn), 0.f, 1.f);
	const double Now = WRIsle::Now(World);
	const APawn* Me = IsleLocalPawn(World);
	const FVector MeAt = Me ? Me->GetActorLocation() : FVector(1e9);
	const float CupReach = WRTune(TEXT("places.cupReach"), 3000.f), CupSeconds = WRTune(TEXT("places.cupMinutes"), 10.f) * 60.f;
	for (FWRIsleAnim& A : Anims)
	{
		switch (A.Kind)
		{
		case FWRIsleAnim::EKind::NightLight:
			if (A.Light.IsValid())
			{
				// A flame's slow breathing on top (a few percent), so the land's fires don't read as bulbs.
				const float Flick = 1.f + 0.06f * FMath::Sin(float(Now) * 2.3f + A.Phase) + 0.03f * FMath::Sin(float(Now) * 5.1f + A.Phase * 1.7f);
				A.Light->SetIntensity(A.Base * Night * Flick);
				A.Light->SetVisibility(Night > 0.01f);
			}
			break;
		case FWRIsleAnim::EKind::Breath:
		{
			// The breath's light swells and eases (smoothstep up, longer fall) on the server's clock: every machine sees the
			// same stack breathe at the same moment; the fx burst plays as each breath starts.
			const double T = Now + A.Phase;
			const int64 Cycle = int64(FMath::FloorToDouble(T / A.Period));
			const float Into = float(T - double(Cycle) * A.Period);
			const float Rise = A.Length * 0.3f;
			const float K = Into < Rise ? FMath::SmoothStep(0.f, 1.f, Into / Rise) : Into < A.Length ? 1.f - FMath::SmoothStep(0.f, 1.f, (Into - Rise) / (A.Length - Rise)) : 0.f;
			if (A.Light.IsValid()) A.Light->SetIntensity(A.Base * (0.08f + 0.92f * K) * (0.35f + 0.65f * FMath::Max(Night, 0.4f)));
			if (Cycle != A.LastPuff && Into < A.Length && FVector::Dist(MeAt, A.At) < WRTune(TEXT("places.breathFxReach"), 60000.f))
			{
				A.LastPuff = Cycle;
				WRFx::SpawnAt(World, A.Name, A.At);
				if (FVector::Dist(MeAt, A.At) < 9000.f) WRSound::PlayAt(World, TEXT("ff.bellows"), A.At);
			}
			break;
		}
		case FWRIsleAnim::EKind::PassCup:
			if (A.Light.IsValid())
			{
				if (Night > 0.2f && FVector::Dist(MeAt, A.At) < CupReach && A.LitUntil < Now)
				{
					if (A.LitUntil < Now - 1.0) WRSound::PlayAt(World, TEXT("ff.cup_light"), A.At);
					A.LitUntil = Now + CupSeconds;
				}
				const float Left = float(A.LitUntil - Now);
				const float On = Left > 0.f ? FMath::Min(1.f, Left / 20.f) * FMath::Min(1.f, (CupSeconds - Left) / 1.5f + 0.f) : 0.f;
				A.Light->SetIntensity(A.Base * On * Night * (1.f + 0.08f * FMath::Sin(float(Now) * 6.f + A.Phase)));
				A.Light->SetVisibility(On * Night > 0.01f);
				// Its flame fx with it.
				if (On * Night > 0.05f && !A.Fx.IsValid()) A.Fx = WRFx::Attach(A.Light.Get(), TEXT("ff.cup_flame"), NAME_None, 1.f);
				else if (On * Night <= 0.05f && A.Fx.IsValid()) { WRFx::Stop(A.Fx.Get()); A.Fx.Reset(); }
			}
			break;
		case FWRIsleAnim::EKind::ShrineLight:
			if (A.Light.IsValid())
			{
				// Checked a few times a second at most: the shrine's lit state replicates (AWRShrine::bLit).
				A.Length -= Step;
				if (A.Length <= 0.f)
				{
					A.Length = 1.f;
					bool bLit = false;
					for (TActorIterator<AWRShrine> It(World); It; ++It) if (It->ShrineId == A.Name) { bLit = It->bLit; break; }
					A.Period = bLit ? 1.f : 0.f;
				}
				A.Phase = FMath::FInterpConstantTo(A.Phase, A.Period, Step, 0.25f);
				A.Light->SetIntensity(A.Base * A.Phase * (0.25f + 0.75f * Night) * (1.f + 0.05f * FMath::Sin(float(Now) * 4.f)));
				A.Light->SetVisibility(A.Phase > 0.01f);
				if (A.Phase > 0.5f && !A.Fx.IsValid()) A.Fx = WRFx::Attach(A.Light.Get(), TEXT("brazier.fire"), NAME_None, 2.2f);
				else if (A.Phase <= 0.5f && A.Fx.IsValid()) { WRFx::Stop(A.Fx.Get()); A.Fx.Reset(); }
			}
			break;
		case FWRIsleAnim::EKind::NightFx:
		{
			const bool bWant = Night > 0.15f && FVector::Dist(MeAt, A.At) < WRTune(TEXT("places.nightFxReach"), 40000.f);
			if (bWant && !A.Fx.IsValid()) A.Fx = WRFx::Attach(Owner->GetRootComponent(), A.Name, NAME_None, A.Base);
			if (A.Fx.IsValid())
			{
				if (!bWant) { WRFx::Stop(A.Fx.Get()); A.Fx.Reset(); }
				else A.Fx->SetWorldLocation(A.At);
			}
			break;
		}
		case FWRIsleAnim::EKind::Glowmoth:
			if (A.Light.IsValid())
			{
				// A slow firefly pulse: up for a beat, down for longer, each moth on its own phase.
				const float P = FMath::Fmod(float(Now) + A.Phase * 3.f, A.Period) / A.Period;
				const float Pulse = FMath::Pow(FMath::Max(0.f, FMath::Sin(P * PI)), 3.f);
				A.Light->SetIntensity(A.Base * Night * (0.25f + 0.75f * Pulse));
				A.Light->SetVisibility(Night > 0.01f);
			}
			break;
		}
	}
}
