#include "WRWorldDressing.h"
#include "Wildreach.h"
#include "WRBoss.h"
#include "WRCreature.h"
#include "WRCharacter.h"
#include "WRCombatLibrary.h"
#include "WRGameplayTags.h"
#include "WRInventory.h"
#include "WRResourceNode.h"
#include "WRTuning.h"
#include "WRFx.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"

static UMaterialInterface* ShapeMaterial()
{
	return LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
}

// ---------------------------------------------------------------- biome hazards

TArray<FWRPoi> FWRPoi::All;
TMap<FIntPoint, float> FWRBiomeHazards::Lava;
TMap<FIntPoint, float> FWRBiomeHazards::Ice;
FWRBiomeHazards::FLavaGrid FWRBiomeHazards::LavaFlow;
float FWRBiomeHazards::Cell = 600.f;

float FWRBiomeHazards::FLavaGrid::Lift(float Cover) const
{
	return Sink + (Top - Sink) * FMath::SmoothStep(CoverLow, CoverHigh, Cover);
}

bool FWRBiomeHazards::FLavaGrid::SurfaceAt(const FVector2D& At, float& OutGround, float& OutLift) const
{
	if (Points.Num() == 0 || Spacing <= 0.f)
	{
		return false;
	}
	const double GX = (At.X + Half) / Spacing, GY = (At.Y + Half) / Spacing;
	const int32 I = FMath::FloorToInt(GX), J = FMath::FloorToInt(GY);
	const float FX = GX - I, FY = GY - J;
	// Corners outside the flow count as well outside its edge; with no corner in the flow there's no lava here.
	float Ground[4], Lifted[4];
	int32 Found = 0;
	float GroundSum = 0.f;
	const FIntPoint Corners[4] = { FIntPoint(I, J), FIntPoint(I + 1, J), FIntPoint(I, J + 1), FIntPoint(I + 1, J + 1) };
	for (int32 k = 0; k < 4; ++k)
	{
		if (const FVector2f* P = Points.Find(Corners[k]))
		{
			Ground[k] = P->X;
			Lifted[k] = Lift(P->Y);
			GroundSum += P->X;
			++Found;
		}
		else
		{
			Ground[k] = TNumericLimits<float>::Max();
			Lifted[k] = Sink;
		}
	}
	if (Found == 0)
	{
		return false;
	}
	for (int32 k = 0; k < 4; ++k)
	{
		if (Ground[k] == TNumericLimits<float>::Max()) Ground[k] = GroundSum / Found;
	}
	const auto Bilerp = [FX, FY](const float V[4]) { return FMath::Lerp(FMath::Lerp(V[0], V[1], FX), FMath::Lerp(V[2], V[3], FX), FY); };
	OutGround = Bilerp(Ground);
	OutLift = Bilerp(Lifted);
	return true;
}

void FWRBiomeHazards::Load(const TSharedPtr<FJsonObject>& Hazards)
{
	Lava.Reset();
	Ice.Reset();
	LavaFlow.Points.Reset();
	if (!Hazards.IsValid())
	{
		return;
	}
	const TSharedPtr<FJsonObject>* Grid = nullptr;
	if (Hazards->TryGetObjectField(TEXT("lavaGrid"), Grid))
	{
		FLavaGrid& F = LavaFlow;
		F.Spacing = (*Grid)->GetNumberField(TEXT("spacing"));
		F.Half = (*Grid)->GetNumberField(TEXT("half"));
		F.CoverLow = (*Grid)->GetNumberField(TEXT("coverLow"));
		F.Sink = (*Grid)->GetNumberField(TEXT("sink"));
		F.CoverHigh = (*Grid)->GetNumberField(TEXT("coverHigh"));
		F.Top = (*Grid)->GetNumberField(TEXT("top"));
		const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
		if ((*Grid)->TryGetArrayField(TEXT("rows"), Rows))
		{
			// [row j, first column i, count n, then n pairs (ground cm, cover dm)]
			for (const TSharedPtr<FJsonValue>& RowValue : *Rows)
			{
				const TArray<TSharedPtr<FJsonValue>>& Row = RowValue->AsArray();
				if (Row.Num() < 3) continue;
				const int32 J = Row[0]->AsNumber(), I0 = Row[1]->AsNumber(), N = Row[2]->AsNumber();
				for (int32 k = 0; k < N && 4 + k * 2 < Row.Num(); ++k)
				{
					F.Points.Add(FIntPoint(I0 + k, J), FVector2f(Row[3 + k * 2]->AsNumber(), Row[4 + k * 2]->AsNumber() / 10.f));
				}
			}
		}
	}
	double Size = 600.0;
	Hazards->TryGetNumberField(TEXT("cell"), Size);
	Cell = Size;
	for (const TPair<const TCHAR*, TMap<FIntPoint, float>*> Kind : { TPair<const TCHAR*, TMap<FIntPoint, float>*>(TEXT("lava"), &Lava), TPair<const TCHAR*, TMap<FIntPoint, float>*>(TEXT("ice"), &Ice) })
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (!Hazards->TryGetArrayField(Kind.Key, Arr)) continue;
		for (int32 i = 0; i + 2 < Arr->Num(); i += 3)
		{
			const float X = (*Arr)[i]->AsNumber(), Y = (*Arr)[i + 1]->AsNumber();
			Kind.Value->Add(FIntPoint(FMath::FloorToInt(X / Cell), FMath::FloorToInt(Y / Cell)), (*Arr)[i + 2]->AsNumber());
		}
	}
	UE_LOG(LogWildreach, Log, TEXT("HAZARDS loaded %d lava and %d ice cells (lava flow: %d grid points)"), Lava.Num(), Ice.Num(), LavaFlow.Points.Num());
}

const TMap<FIntPoint, float>& FWRBiomeHazards::Cells(FName Kind)
{
	return Kind == TEXT("lava") ? Lava : Ice;
}

FName FWRBiomeHazards::KindAt(const FVector& Feet)
{
	const FIntPoint Key(FMath::FloorToInt(Feet.X / Cell), FMath::FloorToInt(Feet.Y / Cell));
	if (const float* Z = Lava.Find(Key))
	{
		float Ground = 0.f, Lift = 0.f;
		if (LavaFlow.Points.Num() == 0)
		{
			if (FMath::Abs(Feet.Z - *Z) < 250.f) return TEXT("lava");  // an older dressing file: whole cells
		}
		// Standing in the drawn lava: where its surface is over the ground, and on that ground (not on a bridge over it).
		else if (LavaFlow.SurfaceAt(FVector2D(Feet), Ground, Lift) && Lift > WRTune(TEXT("hazards.lavaMinDepth"), 3.f)
			&& Feet.Z < Ground + Lift + 80.f && Feet.Z > Ground - 150.f)
		{
			return TEXT("lava");
		}
	}
	if (const float* Z = Ice.Find(Key)) { if (FMath::Abs(Feet.Z - *Z) < 250.f) return TEXT("ice"); }
	return NAME_None;
}

bool FWRBiomeHazards::Resists(const AWRCharacterBase* Who, FName Kind)
{
	if (!Who || Who->GetAttachParentActor())
	{
		return true;  // riding: the mount takes it
	}
	const bool bLava = Kind == TEXT("lava");
	if (Who->HasTag(bLava ? WRTags::Resist_Lava : WRTags::Resist_Ice))
	{
		return true;
	}
	// Infused armor: fire-infused resists lava, frost-infused resists ice.
	const UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Who);
	return Inv && Inv->GetEquipped(EWREquipSlot::Armor).Element == (bLava ? FName(TEXT("fire")) : FName(TEXT("frost")));
}

// ---------------------------------------------------------------- subsystem

bool UWRDressingSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UWRDressingSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	FString Map = InWorld.GetMapName();
	Map.RemoveFromStart(InWorld.StreamingLevelsPrefix);
	Map.RemoveFromStart(TEXT("L_"));
	const TSharedPtr<FJsonObject> File = FWRTuning::LoadDataFile(FString::Printf(TEXT("%s_dressing.json"), *Map.ToLower()));
	if (!File.IsValid())
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	FWRPoi::All.Reset();
	const TArray<TSharedPtr<FJsonValue>>* Pois = nullptr;
	if (File->TryGetArrayField(TEXT("pois"), Pois))
	{
		for (const TSharedPtr<FJsonValue>& V : *Pois)
		{
			const TSharedPtr<FJsonObject> P = V->AsObject();
			FWRPoi& Poi = FWRPoi::All.AddDefaulted_GetRef();
			Poi.Name = P->GetStringField(TEXT("name"));
			Poi.Kind = P->GetStringField(TEXT("kind"));
			Poi.Location = FVector(P->GetNumberField(TEXT("x")), P->GetNumberField(TEXT("y")), P->GetNumberField(TEXT("z")));
		}
	}
	const TSharedPtr<FJsonObject>* Hazards = nullptr;
	FWRBiomeHazards::Load(File->TryGetObjectField(TEXT("hazards"), Hazards) ? *Hazards : nullptr);
	const TSharedPtr<FJsonObject>* Instances = nullptr;
	if (File->TryGetObjectField(TEXT("instances"), Instances))
	{
		if (AWRDressing* Built = InWorld.SpawnActor<AWRDressing>(AWRDressing::StaticClass(), FTransform::Identity, Params))
		{
			Built->Build(*Instances);
			Built->BuildHazardVisuals();
			Dressing = Built;
			// What's been gathered away replicates through the ledger (the server's; a client may already have it).
			if (InWorld.GetNetMode() != NM_Client)
			{
				Ledger = InWorld.SpawnActor<AWRHarvestLedger>(AWRHarvestLedger::StaticClass(), FTransform::Identity, Params);
			}
			if (AWRHarvestLedger* L = Ledger.Get())
			{
				Built->SyncGone(L->GetGone());
			}
		}
	}
	// Caves replicate: only the server spawns them.
	const TArray<TSharedPtr<FJsonValue>>* Caves = nullptr;
	if (InWorld.GetNetMode() != NM_Client && File->TryGetArrayField(TEXT("caves"), Caves))
	{
		for (const TSharedPtr<FJsonValue>& V : *Caves)
		{
			const TSharedPtr<FJsonObject> C = V->AsObject();
			const FVector At(C->GetNumberField(TEXT("x")), C->GetNumberField(TEXT("y")), C->GetNumberField(TEXT("z")));
			// The generator fits each cave to its ground (variant, turn, size, height) so its tunnel clears the landscape.
			double Scale = 1.0, Variant = -1.0;
			C->TryGetNumberField(TEXT("scale"), Scale);
			C->TryGetNumberField(TEXT("variant"), Variant);
			const FTransform T(FRotator(0.f, C->GetNumberField(TEXT("yaw")), 0.f), At, FVector(FMath::Clamp(Scale, 0.5, 3.0)));
			if (AWRCave* Cave = InWorld.SpawnActorDeferred<AWRCave>(AWRCave::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
			{
				Cave->Biome = FName(*C->GetStringField(TEXT("biome")));
				Cave->Variant = FMath::RoundToInt(Variant);
				Cave->FinishSpawning(T);
			}
		}
		UE_LOG(LogWildreach, Log, TEXT("DRESSING spawned %d caves"), Caves->Num());
	}
}

AWRDressing* UWRDressingSubsystem::FindDressing(const UWorld* World)
{
	const UWRDressingSubsystem* Sub = World ? World->GetSubsystem<UWRDressingSubsystem>() : nullptr;
	return Sub ? Sub->GetDressing() : nullptr;
}

// ---------------------------------------------------------------- decoration

// Decorations a swing gathers from: kind in the dressing file, tuning group (gathering.<group>.*), item, and the
// trunk / footprint radius (cm, times the instance's scale) measured to from the swinging character.
struct FWRHarvestSpec { const TCHAR* Kind; const TCHAR* Group; const TCHAR* Item; float Radius; };
static const FWRHarvestSpec HarvestSpecs[] = {
	{ TEXT("char_tree"), TEXT("tree"), TEXT("wood"), 30.f }, { TEXT("pine"), TEXT("tree"), TEXT("wood"), 30.f },
	{ TEXT("basalt_rock"), TEXT("rock"), TEXT("stone"), 110.f }, { TEXT("ice_rock"), TEXT("rock"), TEXT("stone"), 90.f },
	{ TEXT("ember_shrub"), TEXT("shrub"), TEXT("fiber"), 55.f }, { TEXT("snow_shrub"), TEXT("shrub"), TEXT("fiber"), 60.f },
};

AWRDressing::AWRDressing()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	bReplicates = false;
	RootComponent->SetMobility(EComponentMobility::Static);
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.5f;
}

UHierarchicalInstancedStaticMeshComponent* AWRDressing::MakePart(const TCHAR* MeshPath, const FLinearColor& Color, bool bCollision, float CullDistance)
{
	UHierarchicalInstancedStaticMeshComponent* Part = MakeMeshPart(LoadObject<UStaticMesh>(nullptr, MeshPath), bCollision, CullDistance);
	if (UMaterialInterface* Mat = ShapeMaterial())
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Mat, this);
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		Part->SetMaterial(0, MID);
	}
	return Part;
}

UHierarchicalInstancedStaticMeshComponent* AWRDressing::MakeMeshPart(UStaticMesh* Mesh, bool bCollision, float CullDistance)
{
	UHierarchicalInstancedStaticMeshComponent* Part = NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
	Part->SetStaticMesh(Mesh);
	Part->SetupAttachment(RootComponent);
	Part->SetMobility(EComponentMobility::Static);
	Part->SetCollisionEnabled(bCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	if (bCollision) Part->SetCollisionProfileName(TEXT("BlockAll"));
	// Trees, rocks and cliffs drew only out to 120-300 m, so hills further off stood bare; about 0.3 ms at 3x.
	Part->SetCullDistances(0, CullDistance * WRTune(TEXT("world.decorationDistanceScale"), 2.5f));
	Part->bAffectDistanceFieldLighting = false;
	Part->SetCastShadow(bCollision || CullDistance > 20000.f);
	Part->RegisterComponent();
	return Part;
}

void AWRDressing::Build(const TSharedPtr<FJsonObject>& Instances)
{
	const bool bServer = GetWorld()->GetNetMode() == NM_DedicatedServer;
	static const TCHAR* Cyl = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	static const TCHAR* Cone = TEXT("/Engine/BasicShapes/Cone.Cone");
	static const TCHAR* Sphere = TEXT("/Engine/BasicShapes/Sphere.Sphere");
	static const TCHAR* Cube = TEXT("/Engine/BasicShapes/Cube.Cube");

	// One part of a decoration: mesh, colour, size (engine shapes are 100 units), offset above the ground.
	struct FPart { const TCHAR* Mesh; FLinearColor Color; FVector Size; float Lift; bool bCollision; float Cull; FVector Tilt; };
	const TMap<FString, TArray<FPart>> Kinds = {
		{ TEXT("char_tree"), { { Cyl, FLinearColor(0.02f, 0.015f, 0.012f), FVector(40, 40, 520), 240, true, 24000, FVector::ZeroVector },
		                       { Cone, FLinearColor(0.03f, 0.025f, 0.02f), FVector(180, 180, 260), 520, false, 24000, FVector::ZeroVector } } },
		{ TEXT("pine"), { { Cyl, FLinearColor(0.12f, 0.06f, 0.03f), FVector(45, 45, 300), 140, true, 24000, FVector::ZeroVector },
		                  { Cone, FLinearColor(0.02f, 0.12f, 0.05f), FVector(300, 300, 620), 520, false, 24000, FVector::ZeroVector },
		                  { Cone, FLinearColor(0.92f, 0.95f, 0.98f), FVector(150, 150, 220), 760, false, 24000, FVector::ZeroVector } } },
		{ TEXT("basalt_rock"), { { Sphere, FLinearColor(0.025f, 0.022f, 0.022f), FVector(260, 220, 150), 30, true, 24000, FVector(8, 0, 0) } } },
		{ TEXT("ice_rock"), { { Cube, FLinearColor(0.45f, 0.65f, 0.85f), FVector(200, 170, 160), 50, true, 24000, FVector(12, 0, 18) } } },
		{ TEXT("ember_shrub"), { { Sphere, FLinearColor(0.6f, 0.12f, 0.02f), FVector(110, 110, 60), 15, false, 12000, FVector::ZeroVector } } },
		{ TEXT("snow_shrub"), { { Sphere, FLinearColor(0.75f, 0.82f, 0.8f), FVector(120, 120, 70), 15, false, 12000, FVector::ZeroVector } } },
		// Ridge faces: decoration only (no collision), so the ridge stays a wall rather than gaining footholds.
		{ TEXT("basalt_cliff"), { { Sphere, FLinearColor(0.03f, 0.028f, 0.027f), FVector(500, 440, 400), 60, false, 30000, FVector(12, 0, 0) } } },
		{ TEXT("ice_cliff"), { { Sphere, FLinearColor(0.45f, 0.55f, 0.65f), FVector(500, 440, 400), 60, false, 30000, FVector(12, 0, 0) } } },
	};
	// Real meshes from the art packs / AI props replace the shapes above where we have them.
	// Size is what each mesh is fitted to (cm, before the instance's own scale): all three axes for rocks, so they keep
	// the placeholders' footprint and collision, or just the height for plants. Material (optional) replaces the mesh's
	// own materials (boulders use the project's world-aligned M_Rock); Params / Scalars tint whichever is used.
	struct FParam { const TCHAR* Name; FLinearColor Value; };
	struct FScalar { const TCHAR* Name; float Value; };
	struct FArt { TArray<FString> Meshes; FVector Size; float Sink; bool bCollision; float Cull; FVector Tilt; bool bHeightOnly;
	              const TCHAR* Material; TArray<FParam> Params; TArray<FScalar> Scalars;
	              bool bShapeCollision = false; };  // keep the shapes' colliding parts, hidden (tree trunks)
	static const TCHAR* RockMat = TEXT("/Game/Terrain/M_Rock.M_Rock");
	TArray<FString> Rocks, Shrubs;
	for (const TCHAR* N : { TEXT("01A"), TEXT("02A"), TEXT("03A"), TEXT("05A"), TEXT("08A"), TEXT("12A") })
	{
		Rocks.Add(FString::Printf(TEXT("/Game/SimpleRocks/3D_Assets/Meshes/Rocks/SM_SimpleRock_%s.SM_SimpleRock_%s"), N, N));
	}
	for (const TCHAR* N : { TEXT("A"), TEXT("B"), TEXT("D") })
	{
		Shrubs.Add(FString::Printf(TEXT("/Game/GV_FreeShrubsPack/Meshes/Shrubs/Wind/Shrub_%s/GV_Vol7_Shrub_%s_full_type1.GV_Vol7_Shrub_%s_full_type1"), N, N, N));
	}
	const auto Prop = [](const TCHAR* Folder, const TCHAR* Name) { return FString::Printf(TEXT("/Game/Props/%s/%s/StaticMeshes/%s.%s"), Folder, Name, Name, Name); };
	const TMap<FString, FArt> Art = {
		{ TEXT("char_tree"), { { Prop(TEXT("CharTreeA"), TEXT("char_tree_a_textured")), Prop(TEXT("CharTreeB"), TEXT("char_tree_b_textured")),
		    Prop(TEXT("CharTreeC"), TEXT("char_tree_c_textured")), Prop(TEXT("CharTreeD"), TEXT("char_tree_d_textured")) },
			FVector(0, 0, 720), 25, false, 24000, FVector::ZeroVector, true, nullptr, {}, {}, true } },
		{ TEXT("pine"), { { Prop(TEXT("PineA"), TEXT("pine_a_cropped")), Prop(TEXT("PineB"), TEXT("pine_b_textured")), Prop(TEXT("PineC"), TEXT("pine_c_textured")),
		    Prop(TEXT("FirA"), TEXT("fir_a_textured")), Prop(TEXT("BirchA"), TEXT("birch_a_textured")) }, FVector(0, 0, 1000), 25, false, 24000, FVector::ZeroVector, true, nullptr, {}, {}, true } },
		{ TEXT("basalt_rock"), { Rocks, FVector(260, 220, 150), 25, true, 24000, FVector(8, 0, 0), false, RockMat, { { TEXT("Tint"), FLinearColor(0.11f, 0.1f, 0.095f) } }, {} } },
		{ TEXT("ice_rock"), { Rocks, FVector(200, 170, 160), 25, true, 24000, FVector(12, 0, 18), false, RockMat, { { TEXT("Tint"), FLinearColor(0.62f, 0.74f, 0.88f) } }, {} } },
		{ TEXT("basalt_cliff"), { Rocks, FVector(520, 460, 440), 140, false, 30000, FVector(12, 0, 18), false, RockMat, { { TEXT("Tint"), FLinearColor(0.13f, 0.12f, 0.11f) } }, {} } },
		{ TEXT("ice_cliff"), { Rocks, FVector(520, 460, 440), 140, false, 30000, FVector(12, 0, 18), false, RockMat, { { TEXT("Tint"), FLinearColor(0.58f, 0.68f, 0.8f) } }, {} } },
		// Scorched: dark bark, dry rust-brown leaves. The leaves' own colour is "Color Tint" / "Color Tint B" and their
		// back-lit glow "SSS Color Tint": setting only "Color" (the bark) left them bright green, more so now there are
		// many more of them.
		{ TEXT("ember_shrub"), { Shrubs, FVector(0, 0, 130), 5, false, 12000, FVector::ZeroVector, true, nullptr,
			{ { TEXT("Color"), FLinearColor(0.05f, 0.04f, 0.035f) }, { TEXT("Color Tint"), FLinearColor(0.11f, 0.06f, 0.03f) },
			  { TEXT("Color Tint B"), FLinearColor(0.16f, 0.07f, 0.025f) }, { TEXT("SSS Color Tint"), FLinearColor(0.14f, 0.045f, 0.01f) },
			  { TEXT("SSS Color Tint B"), FLinearColor(0.12f, 0.04f, 0.01f) } },
			{ { TEXT("Color Intensity"), 0.55f }, { TEXT("Sature Max"), 0.65f }, { TEXT("SSS Color Intensity"), 0.35f } } } },
		{ TEXT("snow_shrub"), { Shrubs, FVector(0, 0, 120), 5, false, 12000, FVector::ZeroVector, true, nullptr,  // frosted: pale
			{ { TEXT("Snow Color"), FLinearColor(0.95f, 0.97f, 1.f) } }, { { TEXT("Color Intensity"), 1.25f }, { TEXT("Sature Max"), 0.5f } } } },
	};
	// Harvestable kinds keep their ground points (bucketed for the swing lookup) and the parts that draw them, on every
	// machine in the same order, so a ledger key means the same decoration everywhere.
	Harvestables.Reset();
	for (const FWRHarvestSpec& Spec : HarvestSpecs)
	{
		FHarvestKind& HK = Harvestables.AddDefaulted_GetRef();
		HK.Kind = Spec.Kind;
		HK.Group = Spec.Group;
		HK.Item = FName(Spec.Item);
		HK.Radius = Spec.Radius;
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (!Instances->TryGetArrayField(Spec.Kind, Arr))
		{
			continue;
		}
		for (int32 i = 0; i + 4 < Arr->Num(); i += 5)
		{
			const FVector Ground((*Arr)[i]->AsNumber(), (*Arr)[i + 1]->AsNumber(), (*Arr)[i + 2]->AsNumber());
			HK.Cells.FindOrAdd(FIntPoint(FMath::FloorToInt(Ground.X / HarvestCell), FMath::FloorToInt(Ground.Y / HarvestCell))).Add(HK.Ground.Num());
			HK.Ground.Add(Ground);
			HK.Scale.Add((*Arr)[i + 4]->AsNumber());
		}
	}
	const auto HarvestKind = [this](const FString& Kind) -> FHarvestKind*
	{
		return Harvestables.FindByPredicate([&Kind](const FHarvestKind& HK) { return HK.Kind == Kind; });
	};

	const auto BuildArt = [&](const FArt& A, const TArray<TSharedPtr<FJsonValue>>& Arr, FHarvestKind* HK) -> bool
	{
		TArray<UStaticMesh*> Meshes;
		for (const FString& Path : A.Meshes)
		{
			if (UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *Path)) Meshes.Add(M);
		}
		if (Meshes.Num() == 0)
		{
			return false;  // pack not installed: fall back to the shapes
		}
		if (bServer && !A.bCollision)
		{
			return true;
		}
		const int32 Count = Arr.Num() / 5;
		for (int32 m = 0; m < Meshes.Num(); ++m)
		{
			const FBox Box = Meshes[m]->GetBoundingBox();
			const FVector Ext = Box.GetSize().ComponentMax(FVector(1.f));
			const FVector Fit = A.bHeightOnly ? FVector(A.Size.Z / Ext.Z) : A.Size / Ext;
			TArray<FTransform> Transforms;
			for (int32 i = m; i < Count; i += Meshes.Num())  // instances take turns across the meshes
			{
				const float S = Arr[i * 5 + 4]->AsNumber();
				const float Yaw = Arr[i * 5 + 3]->AsNumber();
				const FVector Ground(Arr[i * 5]->AsNumber(), Arr[i * 5 + 1]->AsNumber(), Arr[i * 5 + 2]->AsNumber());
				const FRotator Rot(A.Tilt.X * FMath::Sin(Yaw), Yaw, A.Tilt.Z * FMath::Cos(Yaw * 1.7f));
				const FVector Scale = Fit * S;
				Transforms.Emplace(Rot, Ground + FVector(0.f, 0.f, -Box.Min.Z * Scale.Z - A.Sink * S), Scale);
			}
			UHierarchicalInstancedStaticMeshComponent* Part = MakeMeshPart(Meshes[m], A.bCollision, A.Cull);
			UMaterialInterface* Override = A.Material ? LoadObject<UMaterialInterface>(nullptr, A.Material) : nullptr;
			for (int32 Slot = 0; Slot < Meshes[m]->GetStaticMaterials().Num() && (Override || A.Params.Num() + A.Scalars.Num() > 0); ++Slot)
			{
				UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Override ? Override : Meshes[m]->GetMaterial(Slot), this);
				for (const FParam& P : A.Params) MID->SetVectorParameterValue(P.Name, P.Value);  // ignored where a material lacks it
				for (const FScalar& P : A.Scalars) MID->SetScalarParameterValue(P.Name, P.Value);
				Part->SetMaterial(Slot, MID);
			}
			Part->AddInstances(Transforms, false, true);
			if (HK)
			{
				HK->Parts.Add({ Part, Meshes.Num(), m });
			}
		}
		return true;
	};

	for (const TPair<FString, TArray<FPart>>& Kind : Kinds)
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (!Instances->TryGetArrayField(Kind.Key, Arr) || Arr->Num() < 5)
		{
			continue;
		}
		const int32 Count = Arr->Num() / 5;
		const FArt* A = Art.Find(Kind.Key);
		FHarvestKind* HK = HarvestKind(Kind.Key);
		const bool bArt = A && BuildArt(*A, *Arr, HK);
		for (const FPart& Part : Kind.Value)
		{
			if (bArt && !(A->bShapeCollision && Part.bCollision))
			{
				continue;  // drawn by the art mesh
			}
			if (bServer && !Part.bCollision)
			{
				continue;  // the server only needs what blocks movement
			}
			TArray<FTransform> Transforms;
			Transforms.Reserve(Count);
			for (int32 i = 0; i < Count; ++i)
			{
				const float S = (*Arr)[i * 5 + 4]->AsNumber();
				const float Yaw = (*Arr)[i * 5 + 3]->AsNumber();
				const FVector Ground((*Arr)[i * 5]->AsNumber(), (*Arr)[i * 5 + 1]->AsNumber(), (*Arr)[i * 5 + 2]->AsNumber());
				// Small per-instance tilt for rocks, from the yaw so every machine agrees.
				const FRotator Rot(Part.Tilt.X * FMath::Sin(Yaw), Yaw, Part.Tilt.Z * FMath::Cos(Yaw * 1.7f));
				Transforms.Emplace(Rot, Ground + FVector(0.f, 0.f, Part.Lift * S - 20.f), Part.Size / 100.f * S);
			}
			UHierarchicalInstancedStaticMeshComponent* Shape = MakePart(Part.Mesh, Part.Color, Part.bCollision, Part.Cull);
			if (bArt)
			{
				Shape->SetHiddenInGame(true);  // invisible collision under an art mesh
				Shape->SetCastShadow(false);
			}
			Shape->AddInstances(Transforms, false, true);
			if (HK)
			{
				HK->Parts.Add({ Shape, 1, 0 });
			}
		}
		Total += Count;
	}
	UE_LOG(LogWildreach, Log, TEXT("DRESSING built %d decorations (%s)"), Total, bServer ? TEXT("collision only") : TEXT("full"));
}

void AWRDressing::BuildHazardVisuals()
{
	if (GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	// One continuous surface per hazard, a little above the river bed: each cell is a quad whose corners are shared
	// with its neighbours and sit at the average height of the cells around them, so slopes are smooth (no steps).
	// Only neighbours within MaxStep of the cell count, so the sheet splits at real drops (a plateau edge) instead of
	// stretching up into the air between them.
	const bool bLavaFlow = FWRBiomeHazards::Flow().Points.Num() > 0;
	if (bLavaFlow)
	{
		BuildLavaFlow();
	}
	for (const FName Kind : { FName(TEXT("lava")), FName(TEXT("ice")) })
	{
		const bool bLava = Kind == TEXT("lava");
		if (bLava && bLavaFlow)
		{
			continue;  // drawn from the flow's own grid instead of the coarse cells
		}
		const TMap<FIntPoint, float>& Cells = FWRBiomeHazards::Cells(Kind);
		if (Cells.Num() == 0)
		{
			continue;
		}
		const float S = FWRBiomeHazards::CellSize();
		const float Lift = bLava ? 40.f : 20.f;
		TArray<FVector> Verts;
		TArray<int32> Tris;
		const float MaxStep = 700.f;  // joins steep river beds (a 6 m cell rising 7 m), splits plateau edges
		TMap<FIntVector, int32> CornerIndex;  // corner point + its height (10 cm steps): cells that agree share a vertex
		const auto Corner = [&](const FIntPoint& P, float CellZ) -> int32
		{
			float Sum = 0.f;
			int32 N = 0;
			for (const FIntPoint& Cell : { P, P - FIntPoint(1, 0), P - FIntPoint(0, 1), P - FIntPoint(1, 1) })
			{
				const float* Z = Cells.Find(Cell);
				if (Z && FMath::Abs(*Z - CellZ) <= MaxStep) { Sum += *Z; ++N; }
			}
			const float Height = Sum / FMath::Max(1, N);
			const FIntVector Key(P.X, P.Y, FMath::RoundToInt(Height / 10.f));
			if (const int32* Found = CornerIndex.Find(Key)) return *Found;
			return CornerIndex.Add(Key, Verts.Add(FVector(P.X * S, P.Y * S, Height + Lift)));
		};
		for (const TPair<FIntPoint, float>& Cell : Cells)
		{
			const FIntPoint K = Cell.Key;
			const int32 A = Corner(K, Cell.Value), B = Corner(K + FIntPoint(1, 0), Cell.Value), C2 = Corner(K + FIntPoint(1, 1), Cell.Value), D = Corner(K + FIntPoint(0, 1), Cell.Value);
			Tris.Append({ A, C2, B, A, D, C2 });  // counter-clockwise from above, so the faces point up
		}
		TArray<FVector> Normals;
		Normals.Init(FVector::ZeroVector, Verts.Num());
		for (int32 i = 0; i < Tris.Num(); i += 3)
		{
			const FVector N = FVector::CrossProduct(Verts[Tris[i + 2]] - Verts[Tris[i]], Verts[Tris[i + 1]] - Verts[Tris[i]]);
			for (int32 k = 0; k < 3; ++k) Normals[Tris[i + k]] += N;
		}
		TArray<FVector2D> UVs;
		for (int32 i = 0; i < Verts.Num(); ++i)
		{
			Normals[i] = Normals[i].GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
			if (Normals[i].Z < 0.f) Normals[i] = -Normals[i];
			UVs.Add(FVector2D(Verts[i].X, Verts[i].Y) / S);
		}
		UProceduralMeshComponent* Surface = NewObject<UProceduralMeshComponent>(this);
		Surface->SetupAttachment(RootComponent);
		Surface->RegisterComponent();
		Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);  // the hazard itself is cell-based on the server
		Surface->SetCastShadow(false);
		Surface->CreateMeshSection(0, Verts, Tris, Normals, UVs, TArray<FColor>(), TArray<FProcMeshTangent>(), false);
		Surface->SetCullDistance(bLava ? 24000.f : 9000.f);
		// Glowing world-aligned lava / glossy ice (Tools/terrain/build_terrain_materials.py), flat colours if missing.
		UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, bLava ? TEXT("/Game/Terrain/M_Lava.M_Lava") : TEXT("/Game/Terrain/M_Ice.M_Ice"));
		if (!Mat)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(ShapeMaterial(), this);
			MID->SetVectorParameterValue(TEXT("Color"), bLava ? FLinearColor(0.95f, 0.22f, 0.02f) : FLinearColor(0.4f, 0.68f, 0.95f));
			Mat = MID;
		}
		Surface->SetMaterial(0, Mat);
	}
}

void AWRDressing::BuildLavaFlow()
{
	// The lava flow: a sheet whose points sit on landscape vertices (every other one) at the ground plus the lava's lift,
	// so it lies along the terrain on any slope instead of flat squares that float over it, and its rim, sunk under the
	// ground, ends wherever the terrain cuts it (a ragged shore, no straight edges). Its own material glows from the
	// vertex colours: molten in the middle, cooling to crust toward the rim. Chunks so the far ones cull.
	const FWRBiomeHazards::FLavaGrid& Flow = FWRBiomeHazards::Flow();
	const float S = Flow.Spacing;
	const int32 ChunkPoints = FMath::Max(4, FMath::RoundToInt(WRTune(TEXT("lava.chunkPoints"), 32.f)));
	const auto SurfaceZ = [&Flow](const FIntPoint& P, float& Out) -> bool
	{
		const FVector2f* V = Flow.Points.Find(P);
		if (V) Out = V->X + Flow.Lift(V->Y);
		return V != nullptr;
	};
	struct FChunk
	{
		TArray<FVector> Verts;
		TArray<int32> Tris;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FLinearColor> Colors;
		TMap<FIntPoint, int32> Index;
	};
	TMap<FIntPoint, FChunk> Chunks;
	const auto Vertex = [&](FChunk& Into, const FIntPoint& P) -> int32
	{
		if (const int32* Found = Into.Index.Find(P))
		{
			return *Found;
		}
		const FVector2f V = Flow.Points.FindChecked(P);
		const float Z = V.X + Flow.Lift(V.Y);
		float L = Z, R = Z, D = Z, U = Z;  // a missing neighbour counts as level
		SurfaceZ(P - FIntPoint(1, 0), L);
		SurfaceZ(P + FIntPoint(1, 0), R);
		SurfaceZ(P - FIntPoint(0, 1), D);
		SurfaceZ(P + FIntPoint(0, 1), U);
		const FVector At(P.X * S - Flow.Half, P.Y * S - Flow.Half, Z);
		const int32 I = Into.Verts.Add(At);
		Into.Normals.Add(FVector((L - R) / (2.f * S), (D - U) / (2.f * S), 1.f).GetSafeNormal());
		Into.UVs.Add(FVector2D(At.X, At.Y) / 400.f);
		// R = heat (crust at the rim, molten toward the middle), G = 0 where the sheet dips under the ground.
		Into.Colors.Add(FLinearColor(FMath::SmoothStep(0.6f, 5.f, V.Y), FMath::SmoothStep(-0.4f, 1.6f, V.Y), 0.f, 1.f));
		Into.Index.Add(P, I);
		return I;
	};
	int32 Squares = 0;
	for (const TPair<FIntPoint, FVector2f>& It : Flow.Points)
	{
		const FIntPoint P = It.Key, P1 = P + FIntPoint(1, 0), P2 = P + FIntPoint(1, 1), P3 = P + FIntPoint(0, 1);
		const FVector2f* V1 = Flow.Points.Find(P1);
		const FVector2f* V2 = Flow.Points.Find(P2);
		const FVector2f* V3 = Flow.Points.Find(P3);
		if (!V1 || !V2 || !V3)
		{
			continue;
		}
		if (FMath::Max(FMath::Max(Flow.Lift(It.Value.Y), Flow.Lift(V1->Y)), FMath::Max(Flow.Lift(V2->Y), Flow.Lift(V3->Y))) <= 0.f)
		{
			continue;  // wholly under the ground
		}
		FChunk& Into = Chunks.FindOrAdd(FIntPoint(FMath::FloorToInt(P.X / float(ChunkPoints)), FMath::FloorToInt(P.Y / float(ChunkPoints))));
		const int32 A = Vertex(Into, P), B = Vertex(Into, P1), C = Vertex(Into, P2), D = Vertex(Into, P3);
		Into.Tris.Append({ A, C, B, A, D, C });  // counter-clockwise from above, so the faces point up
		++Squares;
	}
	// Glowing lava (Tools/terrain/build_terrain_materials.py), flat orange if it's missing.
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_Lava.M_Lava"));
	UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(Base ? Base : ShapeMaterial(), this);
	if (Base)
	{
		Mat->SetScalarParameterValue(TEXT("Glow"), WRTune(TEXT("lava.glow"), 1.f));
		Mat->SetScalarParameterValue(TEXT("FarLift"), WRTune(TEXT("lava.farLiftPer100m"), 45.f));
	}
	else
	{
		Mat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.95f, 0.22f, 0.02f));
	}
	const float Cull = WRTune(TEXT("lava.cullDistance"), 30000.f);
	for (TPair<FIntPoint, FChunk>& It : Chunks)
	{
		FChunk& C = It.Value;
		UProceduralMeshComponent* Surface = NewObject<UProceduralMeshComponent>(this);
		Surface->SetupAttachment(RootComponent);
		Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);  // the hazard is decided from the same grid, not by touch
		Surface->SetCastShadow(false);
		Surface->bAffectDistanceFieldLighting = false;
		Surface->SetBoundsScale(1.2f);  // the material lifts far lava over the coarser far landscape
		Surface->RegisterComponent();
		Surface->CreateMeshSection_LinearColor(0, C.Verts, C.Tris, C.Normals, C.UVs, C.Colors, TArray<FProcMeshTangent>(), false);
		Surface->SetCullDistance(Cull);
		Surface->SetMaterial(0, Mat);
	}
	UE_LOG(LogWildreach, Log, TEXT("LAVA flow drawn: %d squares in %d chunks"), Squares, Chunks.Num());
}

void AWRDressing::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (GetWorld()->GetNetMode() == NM_Client)
	{
		return;
	}
	// Server: lava burns anyone standing in it without lava resistance.
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		AWRCharacterBase* Who = *It;
		if (!Who->IsAlive() || !Who->GetCharacterMovement()->IsMovingOnGround())
		{
			continue;
		}
		const FVector Feet = Who->GetActorLocation() - FVector(0.f, 0.f, Who->GetSimpleCollisionHalfHeight());
		if (FWRBiomeHazards::KindAt(Feet) != TEXT("lava") || FWRBiomeHazards::Resists(Who, TEXT("lava")))
		{
			continue;
		}
		FWRHit Hit;
		Hit.Damage = WRTune(TEXT("hazards.lavaDps"), 12.f) * DeltaSeconds;
		Hit.Element = WRTags::Element_Fire;
		Hit.BurnBuildup = WRTune(TEXT("hazards.lavaBurnPerSecond"), 25.f) * DeltaSeconds;
		Hit.bDamageOverTime = true;
		Hit.Origin = Feet;
		UWRCombatLibrary::ApplyHit(Who, Hit);
	}
}

// ---------------------------------------------------------------- gathering from the decoration

bool AWRDressing::HarvestInFront(AWRCharacterBase* Who, float Reach, bool bHeavy)
{
	UWRInventoryComponent* Inventory = Who ? UWRInventoryComponent::FindFor(Who) : nullptr;
	if (!Inventory || GetWorld()->GetNetMode() == NM_Client)
	{
		return false;  // only survivors gather (a creature's bite near a tree doesn't fell it)
	}
	const FVector Here = Who->GetActorLocation();
	const float FeetZ = Here.Z - Who->GetSimpleCollisionHalfHeight();
	const FVector Forward = Who->GetActorForwardVector().GetSafeNormal2D();
	const FIntPoint Cell(FMath::FloorToInt(Here.X / HarvestCell), FMath::FloorToInt(Here.Y / HarvestCell));
	int32 BestKind = INDEX_NONE, BestIndex = INDEX_NONE;
	float BestGap = Reach;
	for (int32 k = 0; k < Harvestables.Num(); ++k)
	{
		const FHarvestKind& HK = Harvestables[k];
		for (int32 dx = -1; dx <= 1; ++dx)
		{
			for (int32 dy = -1; dy <= 1; ++dy)
			{
				const TArray<int32>* InCell = HK.Cells.Find(Cell + FIntPoint(dx, dy));
				if (!InCell)
				{
					continue;
				}
				for (int32 i : *InCell)
				{
					const FVector To = HK.Ground[i] - Here;
					const float Gap = To.Size2D() - HK.Radius * HK.Scale[i];
					// In reach, roughly at your feet (slopes), and in front unless you're standing in it.
					if (Gap > BestGap || HK.Ground[i].Z < FeetZ - 200.f || HK.Ground[i].Z > FeetZ + 250.f
						|| (Gap > 0.f && FVector::DotProduct(To.GetSafeNormal2D(), Forward) < 0.4f) || Hidden.Contains((k << KeyShift) | i))
					{
						continue;
					}
					BestGap = Gap;
					BestKind = k;
					BestIndex = i;
				}
			}
		}
	}
	if (BestKind == INDEX_NONE)
	{
		return false;
	}
	const FHarvestKind& HK = Harvestables[BestKind];
	const FString Tune = TEXT("gathering.") + HK.Group;
	const int32 Key = (BestKind << KeyShift) | BestIndex;
	FWRGathering::Gather(Who, HK.Item, FWRTuning::Get().Num(Tune + TEXT(".perHit"), 2.f), bHeavy, FString::Printf(TEXT("%s %d"), *HK.Kind, BestIndex));
	if (HK.Group == TEXT("shrub") && FMath::FRand() < FWRTuning::Get().Num(Tune + TEXT(".berryChance"), 0.3f))
	{
		Inventory->AddItem(TEXT("berries"), 1);
	}
	int32& Taken = HitsTaken.FindOrAdd(Key);
	const bool bUsedUp = Taken + 1 >= FMath::Max(1, FMath::RoundToInt(FWRTuning::Get().Num(Tune + TEXT(".hits"), 6.f)));
	{
		// Chips fly back at the gatherer from the side of the trunk / rock / shrub they hit; the last blow throws more.
		const FVector Ground = HK.Ground[BestIndex];
		const FVector Toward = (Here - Ground).GetSafeNormal2D();
		const bool bTree = HK.Group == TEXT("tree"), bRock = HK.Group == TEXT("rock");
		const FName Effect = bTree ? TEXT("gather.wood") : bRock ? TEXT("gather.stone") : TEXT("gather.fiber");
		const FVector At = Ground + Toward * HK.Radius * HK.Scale[BestIndex] * 0.8f + FVector(0.f, 0.f, bTree ? 100.f : 45.f);
		WRFx::Broadcast(this, Effect, At, Toward.Rotation(), bUsedUp ? 1.8f : (bHeavy ? 1.3f : 1.f), Effect);
	}
	if (++Taken >= FMath::Max(1, FMath::RoundToInt(FWRTuning::Get().Num(Tune + TEXT(".hits"), 6.f))))
	{
		HitsTaken.Remove(Key);
		const UWRDressingSubsystem* Sub = GetWorld()->GetSubsystem<UWRDressingSubsystem>();
		if (AWRHarvestLedger* Ledger = Sub ? Sub->GetLedger() : nullptr)
		{
			Ledger->Fell(Key, FWRTuning::Get().Num(Tune + TEXT(".respawnMinutes"), 15.f) * 60.f);
		}
		UE_LOG(LogWildreach, Log, TEXT("HARVEST %s used up %s %d at %s"), *Who->GetName(), *HK.Kind, BestIndex, *HK.Ground[BestIndex].ToCompactString());
	}
	return true;
}

bool AWRDressing::FindNearest(const FVector& From, const FString& Group, float Range, FVector& OutGround) const
{
	float Best = Range;
	const FIntPoint Cell(FMath::FloorToInt(From.X / HarvestCell), FMath::FloorToInt(From.Y / HarvestCell));
	const int32 Span = FMath::CeilToInt(Range / HarvestCell);
	for (int32 k = 0; k < Harvestables.Num(); ++k)
	{
		const FHarvestKind& HK = Harvestables[k];
		if (!Group.IsEmpty() && HK.Group != Group)
		{
			continue;
		}
		for (int32 dx = -Span; dx <= Span; ++dx)
		{
			for (int32 dy = -Span; dy <= Span; ++dy)
			{
				const TArray<int32>* InCell = HK.Cells.Find(Cell + FIntPoint(dx, dy));
				if (!InCell)
				{
					continue;
				}
				for (int32 i : *InCell)
				{
					const float D = FVector::Dist2D(HK.Ground[i], From);
					if (D < Best && FMath::Abs(HK.Ground[i].Z - From.Z) < 600.f && !Hidden.Contains((k << KeyShift) | i))
					{
						Best = D;
						OutGround = HK.Ground[i];
					}
				}
			}
		}
	}
	return Best < Range;
}

void AWRDressing::SyncGone(const TArray<int32>& Gone)
{
	const TSet<int32> Want(Gone);
	TArray<int32> Back;
	for (const TPair<int32, TArray<FTransform>>& H : Hidden)
	{
		if (!Want.Contains(H.Key)) Back.Add(H.Key);
	}
	for (int32 Key : Back)
	{
		SetGone(Key, false);
	}
	for (int32 Key : Gone)
	{
		SetGone(Key, true);
	}
}

void AWRDressing::SetGone(int32 Key, bool bGone)
{
	const int32 k = Key >> KeyShift, i = Key & ((1 << KeyShift) - 1);
	if (!Harvestables.IsValidIndex(k) || !Harvestables[k].Ground.IsValidIndex(i) || bGone == Hidden.Contains(Key))
	{
		return;
	}
	// Gone = its instances (the art mesh and any hidden collider) moved far below the island; back = put where they were.
	TArray<FTransform> Saved;
	const TArray<FTransform>* Restore = bGone ? nullptr : Hidden.Find(Key);
	int32 n = 0;
	for (const FPartRef& Ref : Harvestables[k].Parts)
	{
		UHierarchicalInstancedStaticMeshComponent* Part = Ref.Part.Get();
		if (!Part || i % Ref.Stride != Ref.Phase)
		{
			continue;
		}
		const int32 Index = (i - Ref.Phase) / Ref.Stride;
		if (bGone)
		{
			FTransform T = FTransform::Identity;
			Part->GetInstanceTransform(Index, T, true);
			Saved.Add(T);
			T.AddToTranslation(FVector(0.f, 0.f, -100000.f));
			Part->UpdateInstanceTransform(Index, T, true, true, true);
		}
		else if (Restore && Restore->IsValidIndex(n))
		{
			Part->UpdateInstanceTransform(Index, (*Restore)[n], true, true, true);
		}
		++n;
	}
	if (bGone)
	{
		Hidden.Add(Key, MoveTemp(Saved));
	}
	else
	{
		Hidden.Remove(Key);
	}
}

AWRHarvestLedger::AWRHarvestLedger()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(4.f);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 1.f;
}

void AWRHarvestLedger::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRHarvestLedger, Gone);
}

void AWRHarvestLedger::BeginPlay()
{
	Super::BeginPlay();
	if (UWRDressingSubsystem* Sub = GetWorld()->GetSubsystem<UWRDressingSubsystem>())
	{
		Sub->SetLedger(this);
	}
	OnRep_Gone();
}

void AWRHarvestLedger::OnRep_Gone()
{
	if (AWRDressing* Dressing = UWRDressingSubsystem::FindDressing(GetWorld()))
	{
		Dressing->SyncGone(Gone);
	}
	UE_LOG(LogWildreach, Log, TEXT("HARVEST ledger: %d decorations gathered away"), Gone.Num());
}

void AWRHarvestLedger::Fell(int32 Key, float Seconds)
{
	if (!HasAuthority())
	{
		return;
	}
	Gone.AddUnique(Key);
	GrowBackAt.Add(Key, GetWorld()->GetTimeSeconds() + Seconds);
	OnRep_Gone();
}

void AWRHarvestLedger::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || GrowBackAt.Num() == 0)
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	TArray<int32> Ready;
	for (const TPair<int32, double>& P : GrowBackAt)
	{
		if (P.Value <= Now) Ready.Add(P.Key);
	}
	if (Ready.Num() == 0)
	{
		return;
	}
	AWRDressing* Dressing = UWRDressingSubsystem::FindDressing(GetWorld());
	for (int32 Key : Ready)
	{
		GrowBackAt.Remove(Key);
		Gone.RemoveSwap(Key);
		if (Dressing) Dressing->GrewBack(Key);
	}
	OnRep_Gone();
}

// ---------------------------------------------------------------- caves

AWRCave::AWRCave()
{
	bReplicates = true;
	SetNetCullDistanceSquared(FMath::Square(25000.f));  // matches the decoration / terrain streaming range
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Rock = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Rock"));
	Rock->SetupAttachment(RootComponent);
	Rock->SetCollisionProfileName(TEXT("BlockAll"));
	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(RootComponent);
	Glow->SetRelativeLocation(FVector(0.f, 0.f, 550.f));
	Glow->SetIntensity(1200.f);
	Glow->SetAttenuationRadius(1600.f);
}

void AWRCave::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRCave, Biome);
	DOREPLIFETIME(AWRCave, Variant);
}

void AWRCave::BeginPlay()
{
	Super::BeginPlay();
	BuildShell();
	if (HasAuthority())
	{
		SpawnContents();
	}
}

void AWRCave::BuildShell()
{
	// Local space: chamber interior x/y -900..900, 0..700 high; the entrance tunnel runs out along +X.
	// Walls reach 4 m underground so uneven ground never leaves gaps.
	Rock->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	const bool bFrost = Biome == TEXT("tundra");
	// World-aligned rock (Tools/terrain/build_terrain_materials.py) so the blocks read as stone; flat colour if missing.
	UMaterialInterface* RockMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_Rock.M_Rock"));
	if (UMaterialInterface* Mat = RockMat ? RockMat : ShapeMaterial())
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Mat, this);
		MID->SetVectorParameterValue(RockMat ? TEXT("Tint") : TEXT("Color"),
			bFrost ? FLinearColor(0.7f, 0.79f, 0.9f) : (RockMat ? FLinearColor(0.13f, 0.12f, 0.11f) : FLinearColor(0.04f, 0.035f, 0.03f)));
		Rock->SetMaterial(0, MID);
	}
	Glow->SetLightColor(bFrost ? FLinearColor(0.5f, 0.7f, 1.f) : FLinearColor(1.f, 0.45f, 0.15f));

	// Blender-sculpted cave (Tools/art/cave_gen.py): a rock mound with the chamber and a ragged entrance tunnel carved
	// out, in the same local layout as the blocks below. Its collision is the mesh itself, and the landscape is the floor.
	// Three variants: the one the dressing file chose (replicated), else picked by position so every machine agrees; the
	// blocks are the fallback if they're missing.
	static const TCHAR* Variants[] = { TEXT("/Game/Props/Caves/CaveA/cave_a.cave_a"), TEXT("/Game/Props/Caves/CaveB/cave_b.cave_b"),
	                                   TEXT("/Game/Props/Caves/CaveC/cave_c.cave_c") };
	const int32 Pick = Variant >= 0 ? Variant % UE_ARRAY_COUNT(Variants) : GetTypeHash(GetActorLocation().GridSnap(100.f)) % UE_ARRAY_COUNT(Variants);
	if (UStaticMesh* CaveMesh = LoadObject<UStaticMesh>(nullptr, Variants[Pick]))
	{
		UStaticMeshComponent* Shell = NewObject<UStaticMeshComponent>(this);
		Shell->SetStaticMesh(CaveMesh);
		Shell->SetupAttachment(RootComponent);
		Shell->SetCollisionProfileName(TEXT("BlockAll"));
		Shell->SetMaterial(0, Rock->GetMaterial(0));  // the biome-tinted rock
		Shell->RegisterComponent();
		return;
	}

	const auto Block = [this](const FVector& Center, const FVector& Size) { Rock->AddInstance(FTransform(FRotator::ZeroRotator, Center, Size / 100.f)); };
	Block(FVector(-1050, 0, 300), FVector(300, 2400, 1400));       // back wall
	Block(FVector(0, -1050, 300), FVector(2400, 300, 1400));       // side walls
	Block(FVector(0, 1050, 300), FVector(2400, 300, 1400));
	Block(FVector(1050, -725, 300), FVector(300, 950, 1400));      // front wall either side of the opening
	Block(FVector(1050, 725, 300), FVector(300, 950, 1400));
	Block(FVector(1050, 0, 725), FVector(300, 500, 550));          // over the opening
	Block(FVector(0, 0, 1150), FVector(2400, 2400, 300));          // roof
	Block(FVector(1600, -400, 175), FVector(800, 300, 1150));      // tunnel
	Block(FVector(1600, 400, 175), FVector(800, 300, 1150));
	Block(FVector(1600, 0, 600), FVector(800, 800, 300));

	// Boulders heaped behind, beside and over the shell so it reads as a rock outcrop rather than a box. Seeded from the
	// cave's position, so every machine builds the same rocks (and collision). The tunnel mouth (+X) stays clear.
	TArray<UStaticMesh*> Boulders;
	for (const TCHAR* N : { TEXT("01A"), TEXT("02A"), TEXT("05A"), TEXT("08A"), TEXT("12A") })
	{
		if (UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/SimpleRocks/3D_Assets/Meshes/Rocks/SM_SimpleRock_%s.SM_SimpleRock_%s"), N, N))) Boulders.Add(M);
	}
	if (Boulders.Num() > 0)
	{
		// Size range is the boulder's largest dimension (cm); Jitter is how far it may wander from its spot.
		struct FSpot { FVector At; float MinSize, MaxSize, Jitter; };
		TArray<FSpot> Spots;
		const auto Big = [&Spots](float X, float Y, float Z) { Spots.Add({ FVector(X, Y, Z), 700.f, 1000.f, 150.f }); };
		for (float Y = -1050.f; Y <= 1060.f; Y += 700.f) Big(-1350.f, Y, 150.f);                                  // behind
		for (float X = -1000.f; X <= 1010.f; X += 666.f) { Big(X, -1350.f, 150.f); Big(X, 1350.f, 150.f); }      // sides
		for (float X = -700.f; X <= 710.f; X += 700.f) for (float Y = -700.f; Y <= 710.f; Y += 700.f) Big(X, Y, 1300.f);  // roof
		for (float X = 1300.f; X <= 1910.f; X += 600.f) { Big(X, -950.f, 150.f); Big(X, 950.f, 150.f); }          // beside the tunnel
		Big(1550.f, 0.f, 900.f);                                                                                  // over the tunnel
		for (const float Y : { -650.f, 0.f, 650.f }) Big(1300.f, Y, 1050.f);                                       // the wall above the tunnel
		// A frame of smaller boulders around the tunnel mouth (opening: |y| < 250, up to 450 high). Their inner edges stay
		// clear of it: half the largest size plus the jitter is under the gap to the opening.
		Spots.Add({ FVector(2150.f, -600.f, 180.f), 500.f, 600.f, 40.f });
		Spots.Add({ FVector(2150.f, 600.f, 180.f), 500.f, 600.f, 40.f });
		Spots.Add({ FVector(2150.f, -600.f, 600.f), 500.f, 600.f, 40.f });
		Spots.Add({ FVector(2150.f, 600.f, 600.f), 500.f, 600.f, 40.f });
		Spots.Add({ FVector(2150.f, 0.f, 900.f), 600.f, 700.f, 40.f });                                          // lintel
		FRandomStream Rand(GetTypeHash(GetActorLocation().GridSnap(100.f)));
		TArray<UInstancedStaticMeshComponent*> Parts;
		for (UStaticMesh* M : Boulders)
		{
			UInstancedStaticMeshComponent* Part = NewObject<UInstancedStaticMeshComponent>(this);
			Part->SetStaticMesh(M);
			Part->SetupAttachment(RootComponent);
			Part->SetCollisionProfileName(TEXT("BlockAll"));
			Part->SetMaterial(0, Rock->GetMaterial(0));  // same tinted rock as the walls
			Part->RegisterComponent();
			Parts.Add(Part);
		}
		for (const FSpot& Spot : Spots)
		{
			UInstancedStaticMeshComponent* Part = Parts[Rand.RandHelper(Parts.Num())];
			const float Size = FMath::Max(1.f, Part->GetStaticMesh()->GetBoundingBox().GetSize().GetMax());
			const FVector Jitter(Rand.FRandRange(-Spot.Jitter, Spot.Jitter), Rand.FRandRange(-Spot.Jitter, Spot.Jitter), Rand.FRandRange(-80.f, 80.f));
			const FRotator Rot(Rand.FRandRange(-15.f, 15.f), Rand.FRandRange(0.f, 360.f), Rand.FRandRange(-15.f, 15.f));
			Part->AddInstance(FTransform(Rot, Spot.At + Jitter, FVector(Rand.FRandRange(Spot.MinSize, Spot.MaxSize) / Size)));
		}
	}
}

void AWRCave::SpawnContents()
{
	const bool bFrost = Biome == TEXT("tundra");
	const auto Floor = [this](const FVector& Local)
	{
		const FVector Top = GetActorTransform().TransformPosition(FVector(Local.X, Local.Y, 650.f));
		FHitResult Hit;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(WRCave), false, this);
		return GetWorld()->LineTraceSingleByChannel(Hit, Top, Top - FVector(0, 0, 2000), ECC_WorldStatic, Query) ? Hit.ImpactPoint : Top - FVector(0, 0, 650);
	};
	// Rich ore: the better metals live in caves, with three times the usual gem odds.
	const TArray<TPair<FName, FVector>> Ores = {
		{ TEXT("iron_ore"), FVector(-550, -550, 0) }, { TEXT("iron_ore"), FVector(-550, 550, 0) }, { TEXT("tin_ore"), FVector(0, -650, 0) },
		{ TEXT("copper_ore"), FVector(300, 620, 0) }, { bFrost ? FName(TEXT("lichen")) : FName(TEXT("sulfur_crystal")), FVector(-700, 0, 0) },
	};
	for (const TPair<FName, FVector>& Ore : Ores)
	{
		const FTransform T(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), Floor(Ore.Value));
		if (AWRResourceNode* Node = GetWorld()->SpawnActorDeferred<AWRResourceNode>(AWRResourceNode::StaticClass(), T, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			Node->ItemId = Ore.Key;
			Node->GemChance = WRTune(TEXT("caves.gemChance"), 0.12f);
			Node->FinishSpawning(T);
		}
	}
	// Cave hazard: burning sulfur gas (volcanic) or slick ice (tundra). Permanent.
	AWRHazard::Spawn(GetWorld(), nullptr, Floor(FVector(250, 0, 0)), bFrost ? 450.f : 260.f, 0.f,
		WRTune(TEXT("caves.gasDps"), 6.f), bFrost ? FName(TEXT("ice")) : FName(TEXT("gas")));
	// A burrower colony (Kilnmites nest here and mine ore once tamed; Glacimaws lurk under the ice).
	const FTransform SpawnAt(FRotator::ZeroRotator, Floor(FVector(-300, 0, 0)));
	if (AWRCreatureSpawner* Spawner = GetWorld()->SpawnActorDeferred<AWRCreatureSpawner>(AWRCreatureSpawner::StaticClass(), SpawnAt, this))
	{
		Spawner->CreatureId = bFrost ? FName(TEXT("glacimaw")) : FName(TEXT("kilnmite"));
		Spawner->Count = bFrost ? 1 : 2;
		Spawner->Radius = 300.f;
		Spawner->RespawnSeconds = 300.f;
		Spawner->FinishSpawning(SpawnAt);
	}
	UE_LOG(LogWildreach, Log, TEXT("CAVE %s (%s) at %s yaw %.0f, variant %d, size x%.2f: %d ore nodes"), *GetName(), *Biome.ToString(),
		*GetActorLocation().ToCompactString(), GetActorRotation().Yaw, Variant, GetActorScale3D().X, Ores.Num());
}
