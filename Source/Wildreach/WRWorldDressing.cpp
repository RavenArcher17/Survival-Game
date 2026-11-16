#include "WRWorldDressing.h"
#include "WRBuildSets.h"
#include "WRFarm.h"
#include "WRSurvival.h"
#include "Algo/Sort.h"
#include "WRLifeSkills.h"
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
#include "WRForestHazards.h"   // the forest's no-felling rule round its giants (HarvestInFront)
#include "WRTreeKit.h"         // the forest's trees, ferns and boulders (forest_* kinds)
#include "WRLand.h"            // a cave on a runtime land (the forest's) finds its floor on the land's chunks
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "TimerManager.h"

static UMaterialInterface* ShapeMaterial()
{
	return LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
}

// ---------------------------------------------------------------- biome hazards

TArray<FWRPoi> FWRPoi::All;
TMap<FIntPoint, float> FWRBiomeHazards::Lava;
TMap<FIntPoint, float> FWRBiomeHazards::Ice;
FWRBiomeHazards::FLavaGrid FWRBiomeHazards::LavaFlow;
FWRBiomeHazards::FLavaGrid FWRBiomeHazards::IceGrid;
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
	IceGrid.Points.Reset();
	if (!Hazards.IsValid())
	{
		return;
	}
	ReadGrid(Hazards, TEXT("lavaGrid"), LavaFlow);
	ReadGrid(Hazards, TEXT("iceGrid"), IceGrid);
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
	UE_LOG(LogWildreach, Log, TEXT("HAZARDS loaded %d lava and %d ice cells (lava flow: %d grid points, ice sheet: %d)"), Lava.Num(), Ice.Num(), LavaFlow.Points.Num(), IceGrid.Points.Num());
}

void FWRBiomeHazards::ReadGrid(const TSharedPtr<FJsonObject>& Hazards, const TCHAR* Field, FLavaGrid& F)
{
	const TSharedPtr<FJsonObject>* Grid = nullptr;
	if (!Hazards->TryGetObjectField(Field, Grid))
	{
		return;
	}
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
	// Infused armour: a fire-infused chest piece resists lava, a frost-infused one ice. A biome set worn to four pieces
	// passes its own land's ground (cooks/build-crafting.md 2.5: Forgeplate walks lava, Rimeguard grips ice).
	const UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Who);
	if (!Inv)
	{
		return false;
	}
	if (Inv->GetEquipped(EWREquipSlot::Armor).Element == (bLava ? FName(TEXT("fire")) : FName(TEXT("frost"))))
	{
		return true;
	}
	return Kind == TEXT("lava") ? Inv->GearStat(TEXT("passLava")) > 0.f : Kind == TEXT("ice") ? Inv->GearStat(TEXT("passIce")) > 0.f : false;
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
	// Storm peaks (Tools/terrain/storm_peaks.mjs): wind-bent mountain pine and krummholz, granite boulders, alpine scrub.
	{ TEXT("storm_pine"), TEXT("tree"), TEXT("wood"), 26.f }, { TEXT("granite_rock"), TEXT("rock"), TEXT("stone"), 100.f },
	{ TEXT("storm_shrub"), TEXT("shrub"), TEXT("fiber"), 50.f },
	// The Swamp (Tools/terrain/swamp.mjs; cooks/biome-swamp.md 2.7): bald cypress and dead cypress give cypress logs (rot-
	// proof wood), red mangrove and cypress knees wood, reeds, pitcher plants and the scrub fibre, bone piles bone.
	{ TEXT("swamp_cypress"), TEXT("tree"), TEXT("cypress_log"), 34.f }, { TEXT("swamp_mangrove"), TEXT("tree"), TEXT("wood"), 28.f },
	{ TEXT("swamp_deadcypress"), TEXT("tree"), TEXT("cypress_log"), 32.f }, { TEXT("swamp_knees"), TEXT("tree"), TEXT("wood"), 45.f },
	{ TEXT("swamp_reeds"), TEXT("shrub"), TEXT("fiber"), 60.f }, { TEXT("swamp_pitcher"), TEXT("shrub"), TEXT("fiber"), 40.f },
	{ TEXT("swamp_shrub"), TEXT("shrub"), TEXT("fiber"), 55.f }, { TEXT("swamp_bones"), TEXT("rock"), TEXT("bone"), 70.f },
	// The Ancient forest (Tools/terrain/forest_dressing.mjs; cooks/biome-forest.md 2.7): every living tree gives wood (oak,
	// beech, lime, alder, ash, holly, hazel rods, saplings, the tundra neck's pines, the newcomers' old stumps), ferns and
	// bramble fibre, mossy boulders stone. Radii are WRTreeKit's trunks at their reference height (times the instance's
	// scale). The giants and dead snags are listed so a swing finds them, but group "giant" never gives: the axe bounces
	// ("The roots hold it.", HarvestInFront). Appended last so the older kinds keep their ledger keys.
	{ TEXT("forest_oak"), TEXT("tree"), TEXT("wood"), 55.f }, { TEXT("forest_beech"), TEXT("tree"), TEXT("wood"), 45.f },
	{ TEXT("forest_lime"), TEXT("tree"), TEXT("wood"), 42.f }, { TEXT("forest_alder"), TEXT("tree"), TEXT("wood"), 22.f },
	{ TEXT("forest_alder_lean"), TEXT("tree"), TEXT("wood"), 22.f }, { TEXT("forest_ash"), TEXT("tree"), TEXT("wood"), 35.f },
	{ TEXT("forest_holly"), TEXT("tree"), TEXT("wood"), 30.f }, { TEXT("forest_hazel"), TEXT("tree"), TEXT("wood"), 60.f },
	{ TEXT("forest_sapling"), TEXT("tree"), TEXT("wood"), 14.f }, { TEXT("forest_pine"), TEXT("tree"), TEXT("wood"), 30.f },
	{ TEXT("forest_stump"), TEXT("tree"), TEXT("wood"), 50.f }, { TEXT("forest_fern"), TEXT("shrub"), TEXT("fiber"), 40.f },
	{ TEXT("forest_bramble"), TEXT("shrub"), TEXT("fiber"), 60.f }, { TEXT("forest_rock"), TEXT("rock"), TEXT("stone"), 110.f },
	{ TEXT("forest_giant"), TEXT("giant"), TEXT("wood"), 300.f }, { TEXT("forest_dead"), TEXT("giant"), TEXT("wood"), 220.f },
};

// The Ancient forest's dressing kinds, drawn from the code-built tree kit (WRTreeKit; no forest pack on this PC, no
// engine shapes): species, the kit variants the kind takes turns across (instance i draws FirstVariant + i % Variants,
// the same on every machine), cull distance (cm before world.decorationDistanceScale; 0 = never culled), and whether
// its trunk blocks (the kit's collider capsules, as hidden instanced cylinders; leaves, ferns, hazel rods and saplings
// are walked through). The giants are never culled and always cast shadows: from the island's south coast and the
// meadows they are the skyline (DC 2.1 "trunks are the skyline", plan R11/R13), 800 triangles each at that range.
struct FWRForestDressKind { const TCHAR* Kind; const TCHAR* Species; int32 FirstVariant; int32 Variants; float Cull; bool bCollide; bool bShadow; };
static const FWRForestDressKind ForestDressKinds[] = {
	{ TEXT("forest_oak"), TEXT("oak"), 0, 4, 26000.f, true, true }, { TEXT("forest_beech"), TEXT("beech"), 0, 4, 26000.f, true, true },
	{ TEXT("forest_lime"), TEXT("lime"), 0, 4, 26000.f, true, true }, { TEXT("forest_ash"), TEXT("ash"), 0, 4, 26000.f, true, true },
	// WRTreeKit's alder variants 2 and 3 lean over +X; the generator yaws them toward the water (DC 2.2).
	{ TEXT("forest_alder"), TEXT("alder"), 0, 2, 26000.f, true, true }, { TEXT("forest_alder_lean"), TEXT("alder"), 2, 2, 26000.f, true, true },
	{ TEXT("forest_holly"), TEXT("holly"), 0, 4, 14000.f, true, true }, { TEXT("forest_hazel"), TEXT("hazel"), 0, 4, 14000.f, false, true },
	{ TEXT("forest_sapling"), TEXT("sapling"), 0, 4, 10000.f, false, false }, { TEXT("forest_stump"), TEXT("stump"), 0, 4, 10000.f, true, false },
	{ TEXT("forest_fern"), TEXT("fern"), 0, 4, 4000.f, false, false }, { TEXT("forest_bramble"), TEXT("bramble"), 0, 4, 7000.f, false, false },
	{ TEXT("forest_rock"), TEXT("boulder"), 0, 4, 16000.f, true, true },
	{ TEXT("forest_giant"), TEXT("giant"), 0, 4, 0.f, true, true }, { TEXT("forest_dead"), TEXT("dead_giant"), 0, 4, 0.f, true, true },
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
	// Offset (cm, turned with the instance's yaw) and Rot place a part off the instance's middle (a mangrove's prop roots,
	// a reed clump's stems); bWithArt keeps a part drawn over an art mesh (Spanish moss hung on a cypress).
	struct FPart { const TCHAR* Mesh; FLinearColor Color; FVector Size; float Lift; bool bCollision; float Cull; FVector Tilt;
	               FVector Offset = FVector::ZeroVector; FRotator Rot = FRotator::ZeroRotator; bool bWithArt = false; };
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
		{ TEXT("storm_pine"), { { Cyl, FLinearColor(0.1f, 0.06f, 0.035f), FVector(40, 40, 280), 130, true, 24000, FVector::ZeroVector },
		                        { Cone, FLinearColor(0.025f, 0.07f, 0.045f), FVector(260, 260, 560), 480, false, 24000, FVector::ZeroVector } } },
		{ TEXT("granite_rock"), { { Sphere, FLinearColor(0.2f, 0.2f, 0.21f), FVector(240, 210, 150), 30, true, 24000, FVector(8, 0, 0) } } },
		{ TEXT("slate_scree"), { { Sphere, FLinearColor(0.05f, 0.055f, 0.065f), FVector(380, 320, 220), 40, false, 30000, FVector(10, 0, 0) } } },
		{ TEXT("storm_shrub"), { { Sphere, FLinearColor(0.12f, 0.15f, 0.1f), FVector(100, 100, 50), 12, false, 12000, FVector::ZeroVector } } },
		// The Swamp (cooks/biome-swamp.md 2.2): bald cypress on its flared buttress with the flat feathery crown in tiers and
		// grey Spanish moss hanging off it (drawn over the art tree too); dead cypress weathered grey, limbs snapped and
		// reaching up; red mangrove on its arching prop roots; reed clumps with cattails; pitcher plants, red-veined, lids
		// hooded; cypress knees standing out of the mud; bone piles (long bones, ribs, a skull); the low moss-grey scrub.
		{ TEXT("swamp_cypress"), {
			{ Cyl, FLinearColor(0.145f, 0.127f, 0.098f), FVector(70, 70, 1500), 700, true, 30000, FVector::ZeroVector },
			{ Cone, FLinearColor(0.13f, 0.115f, 0.09f), FVector(260, 260, 300), 120, false, 30000, FVector::ZeroVector },
			{ Sphere, FLinearColor(0.027f, 0.07f, 0.027f), FVector(1100, 1100, 160), 1500, false, 30000, FVector::ZeroVector },
			{ Sphere, FLinearColor(0.024f, 0.063f, 0.024f), FVector(800, 800, 140), 1300, false, 30000, FVector::ZeroVector, FVector(250, -150, 0) },
			{ Sphere, FLinearColor(0.03f, 0.077f, 0.03f), FVector(700, 700, 130), 1650, false, 30000, FVector::ZeroVector, FVector(-200, 180, 0) },
			{ Cyl, FLinearColor(0.25f, 0.28f, 0.19f), FVector(40, 40, 320), 1280, false, 20000, FVector::ZeroVector, FVector(380, 0, 0), FRotator::ZeroRotator, true },
			{ Cyl, FLinearColor(0.23f, 0.26f, 0.18f), FVector(34, 34, 260), 1220, false, 20000, FVector::ZeroVector, FVector(-300, 240, 0), FRotator::ZeroRotator, true },
			{ Cyl, FLinearColor(0.25f, 0.28f, 0.19f), FVector(36, 36, 300), 1350, false, 20000, FVector::ZeroVector, FVector(80, -380, 0), FRotator::ZeroRotator, true },
			{ Cyl, FLinearColor(0.22f, 0.25f, 0.17f), FVector(30, 30, 220), 1450, false, 20000, FVector::ZeroVector, FVector(-120, -160, 0), FRotator::ZeroRotator, true } } },
		{ TEXT("swamp_deadcypress"), {
			{ Cyl, FLinearColor(0.188f, 0.175f, 0.15f), FVector(80, 80, 1800), 880, true, 30000, FVector::ZeroVector },
			{ Cone, FLinearColor(0.17f, 0.16f, 0.135f), FVector(280, 280, 320), 140, false, 30000, FVector::ZeroVector },
			{ Cyl, FLinearColor(0.18f, 0.168f, 0.145f), FVector(26, 26, 500), 1250, false, 30000, FVector::ZeroVector, FVector(-177, 0, 0), FRotator(45, 0, 0) },
			{ Cyl, FLinearColor(0.18f, 0.168f, 0.145f), FVector(22, 22, 420), 1420, false, 30000, FVector::ZeroVector, FVector(0, -150, 0), FRotator(45, 90, 0) },
			{ Cyl, FLinearColor(0.18f, 0.168f, 0.145f), FVector(24, 24, 460), 1100, false, 30000, FVector::ZeroVector, FVector(163, 0, 0), FRotator(45, 180, 0) },
			{ Cyl, FLinearColor(0.18f, 0.168f, 0.145f), FVector(18, 18, 340), 1600, false, 30000, FVector::ZeroVector, FVector(0, 120, 0), FRotator(45, 270, 0) },
			{ Cone, FLinearColor(0.2f, 0.186f, 0.16f), FVector(60, 60, 120), 1820, false, 30000, FVector::ZeroVector, FVector::ZeroVector, FRotator(8, 30, 0) } } },
		{ TEXT("swamp_mangrove"), {
			{ Cyl, FLinearColor(0.107f, 0.042f, 0.023f), FVector(46, 46, 520), 520, true, 24000, FVector::ZeroVector },
			{ Cyl, FLinearColor(0.096f, 0.038f, 0.021f), FVector(16, 16, 300), 150, false, 24000, FVector::ZeroVector, FVector(110, 0, 0), FRotator(40, 0, 0) },
			{ Cyl, FLinearColor(0.096f, 0.038f, 0.021f), FVector(15, 15, 300), 150, false, 24000, FVector::ZeroVector, FVector(55, 95, 0), FRotator(40, 60, 0) },
			{ Cyl, FLinearColor(0.096f, 0.038f, 0.021f), FVector(16, 16, 300), 150, false, 24000, FVector::ZeroVector, FVector(-55, 95, 0), FRotator(40, 120, 0) },
			{ Cyl, FLinearColor(0.096f, 0.038f, 0.021f), FVector(14, 14, 300), 150, false, 24000, FVector::ZeroVector, FVector(-110, 0, 0), FRotator(40, 180, 0) },
			{ Cyl, FLinearColor(0.096f, 0.038f, 0.021f), FVector(16, 16, 300), 150, false, 24000, FVector::ZeroVector, FVector(-55, -95, 0), FRotator(40, 240, 0) },
			{ Cyl, FLinearColor(0.096f, 0.038f, 0.021f), FVector(15, 15, 300), 150, false, 24000, FVector::ZeroVector, FVector(55, -95, 0), FRotator(40, 300, 0) },
			{ Sphere, FLinearColor(0.027f, 0.07f, 0.027f), FVector(800, 800, 320), 820, false, 24000, FVector::ZeroVector },
			{ Sphere, FLinearColor(0.023f, 0.06f, 0.023f), FVector(600, 600, 260), 760, false, 24000, FVector::ZeroVector, FVector(200, 150, 0) } } },
		{ TEXT("swamp_reeds"), {
			{ Cyl, FLinearColor(0.456f, 0.323f, 0.102f), FVector(5, 5, 240), 110, false, 9000, FVector::ZeroVector },
			{ Cyl, FLinearColor(0.41f, 0.29f, 0.09f), FVector(5, 5, 220), 100, false, 9000, FVector::ZeroVector, FVector(30, 10, 0), FRotator(6, 20, 0) },
			{ Cyl, FLinearColor(0.48f, 0.34f, 0.11f), FVector(4, 4, 250), 115, false, 9000, FVector::ZeroVector, FVector(-25, 20, 0), FRotator(5, 140, 0) },
			{ Cyl, FLinearColor(0.43f, 0.3f, 0.095f), FVector(5, 5, 200), 90, false, 9000, FVector::ZeroVector, FVector(15, -30, 0), FRotator(7, 250, 0) },
			{ Cyl, FLinearColor(0.456f, 0.323f, 0.102f), FVector(4, 4, 230), 105, false, 9000, FVector::ZeroVector, FVector(-30, -20, 0), FRotator(4, 300, 0) },
			{ Cyl, FLinearColor(0.4f, 0.28f, 0.09f), FVector(5, 5, 210), 95, false, 9000, FVector::ZeroVector, FVector(40, -5, 0), FRotator(8, 70, 0) },
			{ Cyl, FLinearColor(0.47f, 0.33f, 0.1f), FVector(4, 4, 245), 112, false, 9000, FVector::ZeroVector, FVector(-5, 35, 0), FRotator(5, 200, 0) },
			{ Cyl, FLinearColor(0.08f, 0.04f, 0.02f), FVector(9, 9, 32), 228, false, 9000, FVector::ZeroVector },
			{ Cyl, FLinearColor(0.08f, 0.04f, 0.02f), FVector(9, 9, 30), 238, false, 9000, FVector::ZeroVector, FVector(-28, 22, 0), FRotator(5, 140, 0) },
			{ Cyl, FLinearColor(0.08f, 0.04f, 0.02f), FVector(8, 8, 28), 200, false, 9000, FVector::ZeroVector, FVector(41, -5, 0), FRotator(8, 70, 0) } } },
		{ TEXT("swamp_pitcher"), {
			{ Cyl, FLinearColor(0.117f, 0.188f, 0.032f), FVector(14, 14, 60), 28, false, 8000, FVector::ZeroVector, FVector::ZeroVector, FRotator(8, 0, 0) },
			{ Cyl, FLinearColor(0.15f, 0.12f, 0.03f), FVector(12, 12, 52), 24, false, 8000, FVector::ZeroVector, FVector(22, 10, 0), FRotator(14, 160, 0) },
			{ Cyl, FLinearColor(0.13f, 0.16f, 0.03f), FVector(13, 13, 66), 31, false, 8000, FVector::ZeroVector, FVector(-18, 16, 0), FRotator(12, 60, 0) },
			{ Cyl, FLinearColor(0.17f, 0.09f, 0.03f), FVector(11, 11, 48), 22, false, 8000, FVector::ZeroVector, FVector(6, -22, 0), FRotator(16, 280, 0) },
			{ Cyl, FLinearColor(0.195f, 0.003f, 0.007f), FVector(20, 18, 3), 60, false, 8000, FVector::ZeroVector, FVector(-4, 0, 0), FRotator(20, 0, 0) },
			{ Cyl, FLinearColor(0.195f, 0.003f, 0.007f), FVector(18, 16, 3), 50, false, 8000, FVector::ZeroVector, FVector(26, 12, 0), FRotator(24, 160, 0) },
			{ Cyl, FLinearColor(0.195f, 0.003f, 0.007f), FVector(19, 17, 3), 66, false, 8000, FVector::ZeroVector, FVector(-22, 18, 0), FRotator(22, 60, 0) } } },
		{ TEXT("swamp_knees"), {
			{ Cone, FLinearColor(0.145f, 0.127f, 0.098f), FVector(44, 44, 80), 30, false, 12000, FVector::ZeroVector },
			{ Cone, FLinearColor(0.13f, 0.115f, 0.09f), FVector(36, 36, 60), 22, false, 12000, FVector::ZeroVector, FVector(70, 30, 0) },
			{ Cone, FLinearColor(0.15f, 0.13f, 0.1f), FVector(40, 40, 95), 36, false, 12000, FVector::ZeroVector, FVector(-50, 60, 0) },
			{ Cone, FLinearColor(0.12f, 0.105f, 0.08f), FVector(30, 30, 50), 18, false, 12000, FVector::ZeroVector, FVector(20, -70, 0) } } },
		{ TEXT("swamp_bones"), {
			{ Cyl, FLinearColor(0.672f, 0.61f, 0.479f), FVector(9, 9, 70), 6, false, 9000, FVector::ZeroVector, FVector::ZeroVector, FRotator(90, 10, 0) },
			{ Cyl, FLinearColor(0.6f, 0.55f, 0.43f), FVector(8, 8, 60), 7, false, 9000, FVector::ZeroVector, FVector(20, 25, 0), FRotator(90, 70, 0) },
			{ Cyl, FLinearColor(0.65f, 0.59f, 0.46f), FVector(10, 10, 80), 8, false, 9000, FVector::ZeroVector, FVector(-25, -15, 0), FRotator(88, 130, 0) },
			{ Cyl, FLinearColor(0.55f, 0.5f, 0.39f), FVector(5, 5, 50), 12, false, 9000, FVector::ZeroVector, FVector(-10, 30, 0), FRotator(70, 200, 30) },
			{ Cyl, FLinearColor(0.55f, 0.5f, 0.39f), FVector(5, 5, 48), 13, false, 9000, FVector::ZeroVector, FVector(0, 38, 0), FRotator(70, 210, 30) },
			{ Cyl, FLinearColor(0.55f, 0.5f, 0.39f), FVector(5, 5, 46), 12, false, 9000, FVector::ZeroVector, FVector(10, 46, 0), FRotator(70, 220, 30) },
			{ Sphere, FLinearColor(0.7f, 0.64f, 0.5f), FVector(22, 18, 20), 10, false, 9000, FVector::ZeroVector, FVector(30, -30, 0) } } },
		{ TEXT("swamp_shrub"), { { Sphere, FLinearColor(0.16f, 0.19f, 0.12f), FVector(130, 130, 70), 15, false, 12000, FVector::ZeroVector } } },
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
		// Fitted heights are the meshes' bounds, about twice their leafy part: 260 cm makes a bush of a metre or so.
		// The volcanic west's scrub is dead (cooks/cookpass-world.md 3.7, G22): the GV leaves stayed green whatever their
		// tint, and greying them with their "snow" coat (world.ashCoatMin) hung the GPU in the virtual shadow maps' Nanite
		// cull (4 of 4 benchmark runs in 0.4, 2 of 3 in 0.7). So ember_shrub draws the charred-tree meshes at shrub size
		// instead: a leafless burnt scrub about 2 m (world.deadScrubHeight), in its own textures (no coat, no new material).
		// Green is kept where it's real: the Ash Pine Kipuka grows its own shrubs and pines (WRIslandPlacesLook.cpp). Still
		// a fibre harvest.
		{ TEXT("ember_shrub"), { { Prop(TEXT("CharTreeA"), TEXT("char_tree_a_textured")), Prop(TEXT("CharTreeB"), TEXT("char_tree_b_textured")),
		    Prop(TEXT("CharTreeC"), TEXT("char_tree_c_textured")), Prop(TEXT("CharTreeD"), TEXT("char_tree_d_textured")) },
			FVector(0, 0, WRTune(TEXT("world.deadScrubHeight"), 200.f)), 8, false, 12000, FVector(6, 0, 8), true, nullptr, {}, {} } },
		{ TEXT("snow_shrub"), { Shrubs, FVector(0, 0, 240), 5, false, 12000, FVector::ZeroVector, true, nullptr,
			{ { TEXT("Snow Color"), FLinearColor(0.95f, 0.97f, 1.f) } },
			{ { TEXT("Amount Min"), WRTune(TEXT("world.frostCoatMin"), 0.f) }, { TEXT("Amount Max"), 1.f } } } },
		// Storm peaks: the island's pine meshes (no birch up here), granite and slate in the Storm palette, and the
		// shrubs coated a lichen grey-green where the wind keeps them low.
		{ TEXT("storm_pine"), { { Prop(TEXT("PineA"), TEXT("pine_a_cropped")), Prop(TEXT("PineB"), TEXT("pine_b_textured")), Prop(TEXT("PineC"), TEXT("pine_c_textured")),
		    Prop(TEXT("FirA"), TEXT("fir_a_textured")) }, FVector(0, 0, 1000), 25, false, 24000, FVector::ZeroVector, true, nullptr, {}, {}, true } },
		{ TEXT("granite_rock"), { Rocks, FVector(240, 210, 150), 25, true, 24000, FVector(8, 0, 0), false, RockMat, { { TEXT("Tint"), FLinearColor(0.42f, 0.43f, 0.45f) } }, {} } },
		{ TEXT("slate_scree"), { Rocks, FVector(420, 360, 260), 120, false, 30000, FVector(12, 0, 18), false, RockMat, { { TEXT("Tint"), FLinearColor(0.14f, 0.16f, 0.2f) } }, {} } },
		{ TEXT("storm_shrub"), { Shrubs, FVector(0, 0, 200), 5, false, 12000, FVector::ZeroVector, true, nullptr,
			{ { TEXT("Snow Color"), FLinearColor(0.42f, 0.45f, 0.38f) } },
			{ { TEXT("Amount Min"), 0.f }, { TEXT("Amount Max"), WRTune(TEXT("world.stormCoatMax"), 0.6f) } } } },
		// The Swamp: the island's conifers stand in for bald cypress (a deciduous conifer; its moss is the shapes' above),
		// the scrub coated moss grey-green.
		{ TEXT("swamp_cypress"), { { Prop(TEXT("PineB"), TEXT("pine_b_textured")), Prop(TEXT("FirA"), TEXT("fir_a_textured")), Prop(TEXT("PineC"), TEXT("pine_c_textured")) },
			FVector(0, 0, 1500), 25, false, 30000, FVector::ZeroVector, true, nullptr, {}, {}, true } },
		{ TEXT("swamp_shrub"), { Shrubs, FVector(0, 0, 220), 5, false, 12000, FVector::ZeroVector, true, nullptr,
			{ { TEXT("Snow Color"), FLinearColor(0.25f, 0.28f, 0.19f) } },
			{ { TEXT("Amount Min"), WRTune(TEXT("world.swampCoatMin"), 0.3f) }, { TEXT("Amount Max"), WRTune(TEXT("world.swampCoatMax"), 0.9f) } } } },
	};
	// Harvestable kinds keep their ground points (bucketed for the swing lookup) and the parts that draw them, on every
	// machine in the same order, so a ledger key means the same decoration everywhere.
	Harvestables.Reset();
	// The forest's land is built at runtime (WRLand) only while it's live (world.liveBiomes): until then its trees would
	// stand over the sea, so none of its kinds are built or gatherable (their kinds still hold their ledger slots).
	const bool bForestLive = WRBiome::IsLive(TEXT("forest"));
	for (const FWRHarvestSpec& Spec : HarvestSpecs)
	{
		FHarvestKind& HK = Harvestables.AddDefaulted_GetRef();
		HK.Kind = Spec.Kind;
		HK.Group = Spec.Group;
		HK.Item = FName(Spec.Item);
		HK.Radius = Spec.Radius;
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if ((!bForestLive && HK.Kind.StartsWith(TEXT("forest_"))) || !Instances->TryGetArrayField(Spec.Kind, Arr))
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

	const auto BuildArt = [&](const FString& KindName, const FArt& A, const TArray<TSharedPtr<FJsonValue>>& Arr, FHarvestKind* HK) -> bool
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
			{
				// The camera passes through what's smaller than camera.blockMinExtent across (trees, shrubs, small rocks).
				float Extents[3] = { float(A.Size.X), float(A.Size.Y), float(A.Size.Z) };
				Algo::Sort(Extents);
				if (Extents[1] < WRTune(TEXT("camera.blockMinExtent"), 160.f)) Part->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
			}
			UMaterialInterface* Override = A.Material ? LoadObject<UMaterialInterface>(nullptr, A.Material) : nullptr;
			for (int32 Slot = 0; Slot < Meshes[m]->GetStaticMaterials().Num() && (Override || A.Params.Num() + A.Scalars.Num() > 0); ++Slot)
			{
				UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Override ? Override : Meshes[m]->GetMaterial(Slot), this);
				for (const FParam& P : A.Params) MID->SetVectorParameterValue(P.Name, P.Value);  // ignored where a material lacks it
				for (const FScalar& P : A.Scalars) MID->SetScalarParameterValue(P.Name, P.Value);
				Part->SetMaterial(Slot, MID);
				KindMaterials.FindOrAdd(KindName).Add(MID);
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
		const bool bArt = A && BuildArt(Kind.Key, *A, *Arr, HK);
		for (const FPart& Part : Kind.Value)
		{
			if (bArt && !Part.bWithArt && !(A->bShapeCollision && Part.bCollision))
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
				const FQuat YawQ = FRotator(0.f, Yaw, 0.f).Quaternion();
				const FRotator Rot = Part.Rot.IsZero() ? FRotator(Part.Tilt.X * FMath::Sin(Yaw), Yaw, Part.Tilt.Z * FMath::Cos(Yaw * 1.7f)) : (YawQ * Part.Rot.Quaternion()).Rotator();
				Transforms.Emplace(Rot, Ground + YawQ.RotateVector(Part.Offset * S) + FVector(0.f, 0.f, Part.Lift * S - 20.f), Part.Size / 100.f * S);
			}
			UHierarchicalInstancedStaticMeshComponent* Shape = MakePart(Part.Mesh, Part.Color, Part.bCollision, Part.Cull);
			{
				float Extents[3] = { float(Part.Size.X), float(Part.Size.Y), float(Part.Size.Z) };
				Algo::Sort(Extents);
				if (Extents[1] < WRTune(TEXT("camera.blockMinExtent"), 160.f)) Shape->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
			}
			if (bArt && !Part.bWithArt)
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

	// The Ancient forest (Tools/terrain/forest_dressing.mjs; cooks/build-biome-forest.md 2.5-2.6): each kind drawn from the
	// tree kit, one instanced part per species variant (instance i is variant FirstVariant + i % Variants), its trunk
	// collision one hidden instanced cylinder part per kit collider capsule (engine Cylinder: 100 cm tall, 50 cm radius,
	// centred), placed by each instance's yaw and scale. The server builds only the collision (no looks there).
	if (bForestLive)
	{
		// Wraps a kit collider capsule (mesh space, reference size) for one placed instance: a cylinder from A to B whose
		// ends overlap by half a radius each, the radius scaling with the tree.
		const auto CapsuleAt = [](const FWRTreeCollider& C, const FTransform& T) -> FTransform
		{
			const float S = float(T.GetScale3D().X);
			const FVector A = T.TransformPosition(C.A), B = T.TransformPosition(C.B);
			const float Rad = C.Radius * S;
			const FVector Axis = B - A;
			const float Len = float(Axis.Size());
			const FRotator Rot = Len > 1.f ? FRotationMatrix::MakeFromZ(Axis).Rotator() : FRotator::ZeroRotator;
			return FTransform(Rot, (A + B) * 0.5f, FVector(Rad / 50.f, Rad / 50.f, (Len + Rad) / 100.f));
		};
		int32 ForestCount = 0;
		for (const FWRForestDressKind& FK : ForestDressKinds)
		{
			const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
			if (!Instances->TryGetArrayField(FK.Kind, Arr) || Arr->Num() < 5)
			{
				continue;
			}
			const int32 Count = Arr->Num() / 5;
			FHarvestKind* HK = HarvestKind(FK.Kind);
			const FName Species(FK.Species);
			// A giant's trunk or a boulder is a wall to the camera too; slimmer trunks let it through (camera.blockMinExtent).
			const bool bBlockCamera = Species == TEXT("giant") || Species == TEXT("dead_giant") || Species == TEXT("boulder");
			for (int32 v = 0; v < FK.Variants; ++v)
			{
				const int32 Variant = FK.FirstVariant + v;
				TArray<FTransform> Placed;
				Placed.Reserve(Count / FK.Variants + 1);
				for (int32 i = v; i < Count; i += FK.Variants)
				{
					const float S = float((*Arr)[i * 5 + 4]->AsNumber());
					const float Yaw = float((*Arr)[i * 5 + 3]->AsNumber());
					const FVector Ground((*Arr)[i * 5]->AsNumber(), (*Arr)[i * 5 + 1]->AsNumber(), (*Arr)[i * 5 + 2]->AsNumber());
					Placed.Emplace(FRotator(0.f, Yaw, 0.f), Ground, FVector(S));
				}
				if (!bServer)
				{
					if (UStaticMesh* Mesh = WRTreeKit::Mesh(this, Species, Variant))
					{
						UHierarchicalInstancedStaticMeshComponent* Part = MakeMeshPart(Mesh, false, FK.Cull);
						Part->SetCastShadow(FK.bShadow);
						Part->AddInstances(Placed, false, true);
						if (HK)
						{
							HK->Parts.Add({ Part, FK.Variants, v });
						}
					}
				}
				if (!FK.bCollide)
				{
					continue;
				}
				for (const FWRTreeCollider& C : WRTreeKit::Colliders(Species, Variant))
				{
					TArray<FTransform> Caps;
					Caps.Reserve(Placed.Num());
					for (const FTransform& T : Placed)
					{
						Caps.Add(CapsuleAt(C, T));
					}
					UHierarchicalInstancedStaticMeshComponent* Shape = MakePart(Cyl, FLinearColor::Black, true, FK.Cull > 0.f ? FK.Cull : 30000.f);
					Shape->SetHiddenInGame(true);  // the kit's trunk is what you see; this is only what you bump into
					Shape->SetCastShadow(false);
					if (!bBlockCamera)
					{
						Shape->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
					}
					Shape->AddInstances(Caps, false, true);
					if (HK)
					{
						HK->Parts.Add({ Shape, FK.Variants, v });
					}
				}
			}
			ForestCount += Count;
		}
		// The tundra neck's pines (DC 2.2: "pines and snow patches mix into alder" north of y +2,500): the island's pine
		// meshes without their snow, fitted to 10 m (the generator's scale = height / 10 m). Without the pack they draw as
		// the kit's upright alders (narrow dark crowns), never as engine shapes. A hidden trunk cylinder blocks.
		const TArray<TSharedPtr<FJsonValue>>* PineArr = nullptr;
		if (Instances->TryGetArrayField(TEXT("forest_pine"), PineArr) && PineArr->Num() >= 5)
		{
			const int32 Count = PineArr->Num() / 5;
			FHarvestKind* HK = HarvestKind(TEXT("forest_pine"));
			if (!bServer)
			{
				TArray<UStaticMesh*> Meshes;
				TArray<float> Fits;
				for (const FString& Path : { Prop(TEXT("PineA"), TEXT("pine_a_cropped")), Prop(TEXT("PineB"), TEXT("pine_b_textured")), Prop(TEXT("PineC"), TEXT("pine_c_textured")), Prop(TEXT("FirA"), TEXT("fir_a_textured")) })
				{
					if (UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *Path))
					{
						Meshes.Add(M);
						Fits.Add(1000.f / FMath::Max(1.f, float(M->GetBoundingBox().GetSize().Z)));
					}
				}
				const bool bPack = Meshes.Num() > 0;
				if (!bPack)
				{
					for (int32 Variant = 0; Variant < 2; ++Variant)
					{
						if (UStaticMesh* M = WRTreeKit::Mesh(this, TEXT("alder"), Variant))
						{
							Meshes.Add(M);
							Fits.Add(1000.f / FMath::Max(1.f, WRTreeKit::ReferenceHeight(TEXT("alder"))));
						}
					}
				}
				for (int32 m = 0; m < Meshes.Num(); ++m)
				{
					const float MinZ = bPack ? float(Meshes[m]->GetBoundingBox().Min.Z) : 0.f;
					TArray<FTransform> Placed;
					for (int32 i = m; i < Count; i += Meshes.Num())
					{
						const float S = float((*PineArr)[i * 5 + 4]->AsNumber()) * Fits[m];
						const float Yaw = float((*PineArr)[i * 5 + 3]->AsNumber());
						const FVector Ground((*PineArr)[i * 5]->AsNumber(), (*PineArr)[i * 5 + 1]->AsNumber(), (*PineArr)[i * 5 + 2]->AsNumber());
						Placed.Emplace(FRotator(0.f, Yaw, 0.f), Ground + FVector(0.f, 0.f, -MinZ * S), FVector(S));
					}
					UHierarchicalInstancedStaticMeshComponent* Part = MakeMeshPart(Meshes[m], false, 24000.f);
					Part->SetCastShadow(true);
					Part->AddInstances(Placed, false, true);
					if (HK)
					{
						HK->Parts.Add({ Part, Meshes.Num(), m });
					}
				}
			}
			TArray<FTransform> Trunks;
			Trunks.Reserve(Count);
			for (int32 i = 0; i < Count; ++i)
			{
				const float S = float((*PineArr)[i * 5 + 4]->AsNumber());
				const FVector Ground((*PineArr)[i * 5]->AsNumber(), (*PineArr)[i * 5 + 1]->AsNumber(), (*PineArr)[i * 5 + 2]->AsNumber());
				Trunks.Emplace(FRotator::ZeroRotator, Ground + FVector(0.f, 0.f, 300.f * S), FVector(0.6f * S, 0.6f * S, 6.f * S));
			}
			UHierarchicalInstancedStaticMeshComponent* Shape = MakePart(Cyl, FLinearColor::Black, true, 24000.f);
			Shape->SetHiddenInGame(true);
			Shape->SetCastShadow(false);
			Shape->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
			Shape->AddInstances(Trunks, false, true);
			if (HK)
			{
				HK->Parts.Add({ Shape, 1, 0 });
			}
			ForestCount += Count;
		}
		Total += ForestCount;
		UE_LOG(LogWildreach, Log, TEXT("DRESSING forest: %d trees, ferns and rocks from the tree kit"), ForestCount);
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
	const bool bIceSheet = FWRBiomeHazards::IceSheet().Points.Num() > 0;
	if (bLavaFlow)
	{
		BuildLavaFlow();
	}
	if (bIceSheet)
	{
		BuildIceSheet();
	}
	for (const FName Kind : { FName(TEXT("lava")), FName(TEXT("ice")) })
	{
		const bool bLava = Kind == TEXT("lava");
		if (bLava ? bLavaFlow : bIceSheet)
		{
			continue;  // drawn from its own fine grid instead of the coarse cells
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
	// vertex colours: molten in the middle, cooling to crust toward the rim. Chunks so the far ones cull (BuildGridSheet,
	// which the ice sheet shares). Glowing lava (Tools/terrain/build_terrain_materials.py), flat orange if it's missing.
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_Lava.M_Lava"));
	UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(Base ? Base : ShapeMaterial(), this);
	KindMaterials.FindOrAdd(TEXT("lava")).Add(Mat);
	if (Base)
	{
		Mat->SetScalarParameterValue(TEXT("Glow"), WRTune(TEXT("lava.glow"), 1.f));
		Mat->SetScalarParameterValue(TEXT("FarLift"), WRTune(TEXT("lava.farLiftPer100m"), 45.f));
		Mat->SetScalarParameterValue(TEXT("HotLevel"), WRTune(TEXT("lava.hotLevel"), 6.f));
		Mat->SetScalarParameterValue(TEXT("CrustLevel"), WRTune(TEXT("lava.crustLevel"), 0.f));
		Mat->SetScalarParameterValue(TEXT("CrackPower"), WRTune(TEXT("lava.crackPower"), 2.5f));
		Mat->SetScalarParameterValue(TEXT("MoltenCore"), WRTune(TEXT("lava.moltenCore"), 0.9f));
		Mat->SetScalarParameterValue(TEXT("Steady"), WRTune(TEXT("lava.steady"), 0.85f));
	}
	else
	{
		Mat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.95f, 0.22f, 0.02f));
	}
	// R = heat (crust at the rim, molten toward the middle), G = 0 where the sheet dips under the ground.
	int32 Chunks = 0;
	const int32 Squares = BuildGridSheet(FWRBiomeHazards::Flow(), Mat, WRTune(TEXT("lava.cullDistance"), 30000.f),
		[](float Cover) { return FLinearColor(FMath::SmoothStep(0.6f, 5.f, Cover), FMath::SmoothStep(-0.4f, 1.6f, Cover), 0.f, 1.f); }, Chunks);
	UE_LOG(LogWildreach, Log, TEXT("LAVA flow drawn: %d squares in %d chunks"), Squares, Chunks);
}

void AWRDressing::BuildIceSheet()
{
	// Glassy ice (Tools/terrain/build_terrain_materials.py M_Ice: clear and dark-blue where it's thick, paler and
	// frosted toward the rim, a mirror-smooth top), pale blue if it's missing. It follows the ground like the lava does.
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_Ice.M_Ice"));
	UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(Base ? Base : ShapeMaterial(), this);
	KindMaterials.FindOrAdd(TEXT("ice")).Add(Mat);
	if (!Base)
	{
		Mat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.4f, 0.68f, 0.95f));
	}
	// R = thickness (0 at the rim, 1 a few metres in), G = 0 where the sheet dips under the ground.
	int32 Chunks = 0;
	const int32 Squares = BuildGridSheet(FWRBiomeHazards::IceSheet(), Mat, WRTune(TEXT("ice.cullDistance"), 30000.f),
		[](float Cover) { return FLinearColor(FMath::SmoothStep(0.2f, 6.f, Cover), FMath::SmoothStep(-0.3f, 1.2f, Cover), 0.f, 1.f); }, Chunks);
	UE_LOG(LogWildreach, Log, TEXT("ICE sheet drawn: %d squares in %d chunks"), Squares, Chunks);
}

int32 AWRDressing::BuildGridSheet(const FWRBiomeHazards::FLavaGrid& Flow, UMaterialInterface* Mat, float Cull, TFunctionRef<FLinearColor(float)> Colour, int32& OutChunks)
{
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
		float ZWest = Z, ZEast = Z, ZSouth = Z, ZNorth = Z;  // a missing neighbour counts as level
		SurfaceZ(P - FIntPoint(1, 0), ZWest);
		SurfaceZ(P + FIntPoint(1, 0), ZEast);
		SurfaceZ(P - FIntPoint(0, 1), ZSouth);
		SurfaceZ(P + FIntPoint(0, 1), ZNorth);
		const FVector At(P.X * S - Flow.Half, P.Y * S - Flow.Half, Z);
		const int32 I = Into.Verts.Add(At);
		Into.Normals.Add(FVector((ZWest - ZEast) / (2.f * S), (ZSouth - ZNorth) / (2.f * S), 1.f).GetSafeNormal());
		Into.UVs.Add(FVector2D(At.X, At.Y) / 400.f);
		Into.Colors.Add(Colour(V.Y));
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
	OutChunks = Chunks.Num();
	return Squares;
}

int32 AWRDressing::SetLookParam(const FString& Kind, FName Param, const TArray<float>& Values)
{
	int32 Set = 0;
	if (const TArray<TWeakObjectPtr<UMaterialInstanceDynamic>>* Mats = KindMaterials.Find(Kind))
	{
		for (const TWeakObjectPtr<UMaterialInstanceDynamic>& Weak : *Mats)
		{
			if (UMaterialInstanceDynamic* MID = Weak.Get())
			{
				if (Values.Num() >= 3) MID->SetVectorParameterValue(Param, FLinearColor(Values[0], Values[1], Values[2], Values.Num() > 3 ? Values[3] : 1.f));
				else if (Values.Num() == 1) MID->SetScalarParameterValue(Param, Values[0]);
				++Set;
			}
		}
	}
	return Set;
}

namespace WRDressingDev
{
	// Looks tuned live while the game runs (screenshots of a few settings in one go), as wr.SeaParam does for the sea.
	void SetParam(UWorld* World, const FString& Kind, const TArray<FString>& Args, int32 First)
	{
		AWRDressing* Dressing = UWRDressingSubsystem::FindDressing(World);
		if (!Dressing || Args.Num() < First + 2)
		{
			UE_LOG(LogWildreach, Warning, TEXT("LOOK usage: wr.LavaParam <param> <value | r g b> / wr.DressParam <kind> <param> <value | r g b>"));
			return;
		}
		TArray<float> Values;
		for (int32 i = First + 1; i < Args.Num(); ++i) Values.Add(FCString::Atof(*Args[i]));
		// Underscores stand for spaces ("Sature_Min"): console arguments split on spaces.
		const int32 Set = Dressing->SetLookParam(Kind, FName(*Args[First].Replace(TEXT("_"), TEXT(" "))), Values);
		UE_LOG(LogWildreach, Log, TEXT("LOOK %s %s = %s on %d materials"), *Kind, *Args[First], *FString::JoinBy(Values, TEXT(" "), [](float V) { return FString::SanitizeFloat(V); }), Set);
	}
	static FAutoConsoleCommandWithWorldAndArgs LavaParam(TEXT("wr.LavaParam"), TEXT("wr.LavaParam <param> <value | r g b>: set a parameter on the lava flow's material (Glow, HotLevel, CrustLevel, CrackPower, MoltenCore, Steady, FarLift)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World) { SetParam(World, TEXT("lava"), Args, 0); }));
	static FAutoConsoleCommandWithWorldAndArgs DressParam(TEXT("wr.DressParam"), TEXT("wr.DressParam <kind> <param> <value | r g b>: set a material parameter on every decoration of a kind (ember_shrub, snow_shrub, ...)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World) { if (Args.Num() > 0) SetParam(World, Args[0], Args, 1); }));
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
						|| (Gap > 0.f && FVector::DotProduct(To.GetSafeNormal2D(), Forward) < 0.4f) || Hidden.Contains((k << KeyShift) | i) || Cleared.Contains((k << KeyShift) | i))
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
	// The Ancient forest (cooks/biome-forest.md 2.7): no felling within 60 m of a giant's or a heart-tree's root crown.
	// The axe bounces off ("The roots hold it."); the blow is spent and the tree takes no hit.
	if (HK.Group == TEXT("tree") && WRForestHazards::RefuseFelling(Who, HK.Ground[BestIndex]))
	{
		return true;
	}
	// The forest's standing giants and dead snags (group "giant", forest_dressing.mjs) never give anything: a giant's base
	// is always inside its own 60 m, so this is the same bounce and line; the blow is spent either way.
	if (HK.Group == TEXT("giant"))
	{
		WRForestHazards::RefuseFelling(Who, HK.Ground[BestIndex]);
		return true;
	}
	const FString Tune = TEXT("gathering.") + HK.Group;
	const int32 Key = (BestKind << KeyShift) | BestIndex;
	// Clean cut / Vein breaker (life-skill perks): a tree or rock down to its last third gives the rest in this blow.
	const int32 HitsTotal = FMath::Max(1, FMath::RoundToInt(FWRTuning::Get().Num(Tune + TEXT(".hits"), 6.f)));
	int32& Taken = HitsTaken.FindOrAdd(Key);
	const float Break = HK.Group == TEXT("tree") ? UWRLifeSkillsComponent::StatFor(Who, TEXT("treeBreak"))
		: HK.Group == TEXT("rock") ? UWRLifeSkillsComponent::StatFor(Who, TEXT("veinBreak")) : 0.f;
	const int32 Remaining = HitsTotal - Taken;
	int32 Blows = 1;
	if (Break > 0.f && Remaining > 1 && Remaining <= FMath::Max(1, FMath::RoundToInt(HitsTotal * Break)))
	{
		Blows = Remaining;
		Taken = HitsTotal - 1;
	}
	FWRGathering::Gather(Who, HK.Item, FWRTuning::Get().Num(Tune + TEXT(".perHit"), 2.f) * Blows, bHeavy, FString::Printf(TEXT("%s %d"), *HK.Kind, BestIndex));
	if (HK.Group == TEXT("shrub") && FMath::FRand() < FWRTuning::Get().Num(Tune + TEXT(".berryChance"), 0.3f))
	{
		Inventory->AddItem(TEXT("berries"), 1);
	}
	// The biome's crops grow wild among its shrubs (cooks/build-farming.md 2.2): their seeds come with the fibre.
	if (HK.Group == TEXT("shrub"))
	{
		FWRFarm::WildSeeds(Inventory, WRBiome::At(this, HK.Ground[BestIndex]));
	}
	// Ice rocks give ice blocks for ice boxes (cooks/build-farming.md 2.9).
	if (HK.Kind == TEXT("ice_rock") && FMath::FRand() < FWRTuning::Get().Num(TEXT("food.iceBlockChance"), 0.35f))
	{
		Inventory->AddItem(TEXT("ice_block"), 1);
	}
	const bool bUsedUp = Taken + 1 >= HitsTotal;
	{
		// Chips fly back at the gatherer from the side of the trunk / rock / shrub they hit; the last blow throws more.
		const FVector Ground = HK.Ground[BestIndex];
		const FVector Toward = (Here - Ground).GetSafeNormal2D();
		const bool bTree = HK.Group == TEXT("tree"), bRock = HK.Group == TEXT("rock");
		const FName Effect = bTree ? TEXT("gather.wood") : bRock ? TEXT("gather.stone") : TEXT("gather.fiber");
		const FVector At = Ground + Toward * HK.Radius * HK.Scale[BestIndex] * 0.8f + FVector(0.f, 0.f, bTree ? 100.f : 45.f);
		WRFx::Broadcast(this, Effect, At, Toward.Rotation(), bUsedUp ? 1.8f : (bHeavy ? 1.3f : 1.f), Effect);
	}
	if (++Taken >= HitsTotal)
	{
		HitsTaken.Remove(Key);
		// Ancient wood (Woodcutting 100, build-building): a felled tree now and then gives a block of its land's building set.
		if (HK.Group == TEXT("tree"))
		{
			const float Chance = UWRLifeSkillsComponent::StatFor(Who, TEXT("ancientWood"));
			const FName Land = WRBiome::At(this, HK.Ground[BestIndex]);
			for (const FWRTierDef& T : FWRBuildSets::Get().All())
			{
				if (Chance > 0.f && T.Biome == Land && !T.Block.IsNone() && FMath::FRand() < Chance)
				{
					Inventory->AddItem(T.Block, 1);
					break;
				}
			}
		}
		const UWRDressingSubsystem* Sub = GetWorld()->GetSubsystem<UWRDressingSubsystem>();
		if (AWRHarvestLedger* Ledger = Sub ? Sub->GetLedger() : nullptr)
		{
			// Rich veins (life-skill perk): rocks you empty come back sooner.
			const float Sooner = HK.Group == TEXT("rock") ? FMath::Max(0.1f, 1.f + UWRLifeSkillsComponent::StatFor(Who, TEXT("veinRespawn"))) : 1.f;
			Ledger->Fell(Key, FWRTuning::Get().Num(Tune + TEXT(".respawnMinutes"), 15.f) * 60.f * Sooner);
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
		// "Any" never means the forest's giants: nothing can be gathered from them (prompts and gather bots skip them).
		if (Group.IsEmpty() ? HK.Group == TEXT("giant") : HK.Group != Group)
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
					if (D < Best && FMath::Abs(HK.Ground[i].Z - From.Z) < 600.f && !Hidden.Contains((k << KeyShift) | i) && !Cleared.Contains((k << KeyShift) | i))
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

void AWRDressing::ClearCircle(const FVector2D& Centre, float Radius)
{
	// The harvestables first (so the ledger never puts them back and nobody gathers a rock inside a wall).
	int32 Keys = 0;
	for (int32 k = 0; k < Harvestables.Num(); ++k)
	{
		const FHarvestKind& HK = Harvestables[k];
		for (int32 i = 0; i < HK.Ground.Num(); ++i)
		{
			if (FVector2D::Distance(FVector2D(HK.Ground[i]), Centre) < Radius + HK.Radius * HK.Scale[i])
			{
				Cleared.Add((k << KeyShift) | i);
				++Keys;
			}
		}
	}
	// Then every instance of every part whose base stands in the circle: moved far below the island (as a felled tree's).
	int32 Moved = 0;
	TArray<UHierarchicalInstancedStaticMeshComponent*> Parts;
	GetComponents(Parts);
	for (UHierarchicalInstancedStaticMeshComponent* Part : Parts)
	{
		bool bAny = false;
		for (int32 n = 0; n < Part->GetInstanceCount(); ++n)
		{
			FTransform T;
			if (!Part->GetInstanceTransform(n, T, true)) continue;
			const float Reach = Part->GetStaticMesh() ? Part->GetStaticMesh()->GetBounds().SphereRadius * 0.4f * T.GetScale3D().GetMax() : 0.f;
			if (T.GetLocation().Z < -500000.f || FVector2D::Distance(FVector2D(T.GetLocation()), Centre) > Radius + Reach) continue;
			T.SetLocation(T.GetLocation() - FVector(0.f, 0.f, 1000000.f));
			Part->UpdateInstanceTransform(n, T, true, false, true);
			bAny = true;
			++Moved;
		}
		if (bAny) Part->MarkRenderStateDirty();
	}
	UE_LOG(LogWildreach, Log, TEXT("DRESSING cleared %d decorations (%d harvestable) round (%.0f, %.0f) r %.0f"), Moved, Keys, Centre.X, Centre.Y, Radius);
}

void AWRDressing::SetGone(int32 Key, bool bGone)
{
	const int32 k = Key >> KeyShift, i = Key & ((1 << KeyShift) - 1);
	if (!Harvestables.IsValidIndex(k) || !Harvestables[k].Ground.IsValidIndex(i) || bGone == Hidden.Contains(Key) || Cleared.Contains(Key))
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
	DOREPLIFETIME(AWRCave, Theme);
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
		// Storm peaks: slate, pink rock salt in the Halite Cave (cooks/biome-lightning.md 2.4 #18-20).
		const bool bStorm = Biome == TEXT("storm_peaks");
		const FLinearColor StormTint = Theme == TEXT("halite") ? FLinearColor(0.62f, 0.42f, 0.42f) : FLinearColor(0.11f, 0.125f, 0.15f);
		// The Swamp's karst (cooks/biome-swamp.md 2.6): wet grey limestone, the Peat Cellar's walls dark peat.
		const bool bSwampRock = Biome == TEXT("swamp");
		const FLinearColor SwampTint = Theme == TEXT("peat_cellar") ? FLinearColor(0.07f, 0.05f, 0.038f) : FLinearColor(0.19f, 0.18f, 0.15f);
		// The Ancient forest's caves (cooks/biome-forest.md 2.6): mossy grey sandstone under the meadows and the falls,
		// the Root-Mine and the Sinkroot dark root-shot earth, the Foxfire Cellar near-black rotten wood and soil.
		const bool bForestRock = Biome == TEXT("forest");
		const FLinearColor ForestTint = Theme == TEXT("badger_sett") ? FLinearColor(0.3f, 0.25f, 0.16f)
			: Theme == TEXT("foxfire_cellar") ? FLinearColor(0.05f, 0.045f, 0.035f)
			: (Theme == TEXT("root_mine") || Theme == TEXT("sinkroot")) ? FLinearColor(0.09f, 0.075f, 0.055f) : FLinearColor(0.16f, 0.17f, 0.14f);
		MID->SetVectorParameterValue(RockMat ? TEXT("Tint") : TEXT("Color"),
			bForestRock ? ForestTint : bSwampRock ? SwampTint : bStorm ? StormTint : bFrost ? FLinearColor(0.7f, 0.79f, 0.9f) : (RockMat ? FLinearColor(0.13f, 0.12f, 0.11f) : FLinearColor(0.04f, 0.035f, 0.03f)));
		Rock->SetMaterial(0, MID);
	}
	// The forest's caves glow foxfire green (the mycelium on the walls; the Alder Grotto's glowworms read the same).
	Glow->SetLightColor(Biome == TEXT("forest") ? FLinearColor(FColor(127, 224, 122)) : Biome == TEXT("swamp") ? FLinearColor(0.75f, 0.85f, 0.25f) : Biome == TEXT("storm_peaks") ? FLinearColor(0.62f, 0.55f, 1.f) : bFrost ? FLinearColor(0.5f, 0.7f, 1.f) : FLinearColor(1.f, 0.45f, 0.15f));
	// Enough light in the chamber to find the ore by (it was black inside: 1200 over a 16 m reach).
	Glow->SetIntensity(WRTune(TEXT("caves.glowIntensity"), 8000.f));
	Glow->SetAttenuationRadius(WRTune(TEXT("caves.glowRadius"), 2400.f) * GetActorScale3D().X);

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
	// The landscape under a cave may not have streamed in when the server starts the caves: the floor traces then missed
	// and the ore went in at the cave's origin height, under the ground where it rises inside (two tundra caves had no
	// ore a survivor could reach). Wait for the ground (a second at a time, up to a minute).
	{
		const FVector Top = GetActorTransform().TransformPosition(FVector(-300.f, 0.f, 650.f));
		FHitResult Ground;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(WRCave), false, this);
		// (A runtime land's chunks are its ground too: the forest's caves stand on WRLand.)
		const bool bGround = GetWorld()->LineTraceSingleByChannel(Ground, Top, Top - FVector(0, 0, 2000), ECC_WorldStatic, Query)
			&& (Cast<ULandscapeHeightfieldCollisionComponent>(Ground.GetComponent()) || Cast<AWRLand>(Ground.GetActor()));
		if (!bGround && ++ContentTries < 60)
		{
			GetWorldTimerManager().SetTimer(ContentTimer, this, &AWRCave::SpawnContents, 1.f, false);
			return;
		}
	}
	const bool bFrost = Biome == TEXT("tundra");
	const auto Floor = [this](const FVector& Local)
	{
		const FVector Top = GetActorTransform().TransformPosition(FVector(Local.X, Local.Y, 650.f));
		FHitResult Hit;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(WRCave), false, this);
		return GetWorld()->LineTraceSingleByChannel(Hit, Top, Top - FVector(0, 0, 2000), ECC_WorldStatic, Query) ? Hit.ImpactPoint : Top - FVector(0, 0, 650);
	};
	// Rich ore: the better metals live in caves, with three times the usual gem odds.
	TArray<TPair<FName, FVector>> Ores = {
		{ TEXT("iron_ore"), FVector(-550, -550, 0) }, { TEXT("iron_ore"), FVector(-550, 550, 0) }, { TEXT("tin_ore"), FVector(0, -650, 0) },
		{ TEXT("copper_ore"), FVector(300, 620, 0) }, { bFrost ? FName(TEXT("lichen")) : FName(TEXT("sulfur_crystal")), FVector(-700, 0, 0) },
		// The raid economy's cave goods (cooks/build-server-raids.md 2.3): nitre crusting the dry rock on the far side
		// and a clay bank in the damp low corner, clear of the hazard and the veins.
		{ TEXT("saltpetre"), FVector(620, 330, 0) }, { TEXT("clay"), FVector(600, -380, 0) },
	};
	// Storm peaks' caves: the Rodwardens' copper country (DC 2.6): copper rich, tin, iron deeper; rock salt in the Halite
	// Cave; no sulfur, no lichen.
	if (Biome == TEXT("storm_peaks"))
	{
		Ores = { { TEXT("iron_ore"), FVector(-550, -550, 0) }, { TEXT("copper_ore"), FVector(-550, 550, 0) }, { TEXT("tin_ore"), FVector(0, -650, 0) },
			{ TEXT("copper_ore"), FVector(300, 620, 0) }, { TEXT("copper_ore"), FVector(-700, 0, 0) }, { TEXT("clay"), FVector(600, -380, 0) } };
		if (Theme == TEXT("halite")) { Ores[2].Key = TEXT("rock_salt"); Ores[4].Key = TEXT("rock_salt"); Ores.Add({ TEXT("rock_salt"), FVector(620, 330, 0) }); }
		if (Theme == TEXT("adit")) Ores[1].Key = TEXT("iron_ore");
		if (Theme == TEXT("grotto")) Ores[0].Key = TEXT("tin_ore");
	}
	// The Swamp's karst caves (cooks/biome-swamp.md 2.6): Reedmouth copper and tin (the newcomers' first), the Eel Hole
	// copper, the Peat Cellar tin and one iron out in the chamber (its peat-sealed back gallery, x < -600, is UWRSwampWorld's),
	// the Gullet iron x4 with the swamp's best gem odds. No sulfur, no lichen; clay banks in the wet ones.
	if (Biome == TEXT("swamp"))
	{
		if (Theme == TEXT("eel_hole")) Ores = { { TEXT("copper_ore"), FVector(-550, -550, 0) }, { TEXT("copper_ore"), FVector(-550, 550, 0) }, { TEXT("clay"), FVector(600, -380, 0) } };
		else if (Theme == TEXT("peat_cellar")) Ores = { { TEXT("tin_ore"), FVector(250, -620, 0) }, { TEXT("iron_ore"), FVector(250, 620, 0) }, { TEXT("clay"), FVector(600, -380, 0) } };
		else if (Theme == TEXT("gullet")) Ores = { { TEXT("iron_ore"), FVector(-550, -550, 0) }, { TEXT("iron_ore"), FVector(-550, 550, 0) }, { TEXT("iron_ore"), FVector(0, -650, 0) }, { TEXT("iron_ore"), FVector(-700, 0, 0) } };
		else Ores = { { TEXT("copper_ore"), FVector(-550, -550, 0) }, { TEXT("tin_ore"), FVector(-550, 550, 0) }, { TEXT("copper_ore"), FVector(0, -650, 0) }, { TEXT("clay"), FVector(600, -380, 0) } };
	}
	// The Ancient forest's caves (cooks/biome-forest.md 2.6), deeper is better: C1 the Badger Sett copper and tin (the
	// newcomers' first, a sandy sett), C2 the Alder Grotto copper, tin and one iron behind the falls, C3 the Root-Mine iron
	// x3 and its gem pocket (a stone face at the collapsed shaft: 6 hits, a gem most times), C4 the Foxfire Cellar tin and
	// iron x2 (its rhizomorph is UWRForestWorld's), C5 the Sinkroot iron x4 with the forest's best gem odds. Clay where
	// it's wet; no sulfur, no lichen.
	const bool bForestCave = Biome == TEXT("forest");
	if (bForestCave)
	{
		if (Theme == TEXT("badger_sett")) Ores = { { TEXT("copper_ore"), FVector(-550, -550, 0) }, { TEXT("tin_ore"), FVector(-550, 550, 0) }, { TEXT("copper_ore"), FVector(0, -650, 0) } };
		else if (Theme == TEXT("alder_grotto")) Ores = { { TEXT("copper_ore"), FVector(-550, -550, 0) }, { TEXT("tin_ore"), FVector(-550, 550, 0) }, { TEXT("iron_ore"), FVector(-700, 0, 0) }, { TEXT("clay"), FVector(600, -380, 0) } };
		else if (Theme == TEXT("root_mine")) Ores = { { TEXT("iron_ore"), FVector(-550, -550, 0) }, { TEXT("iron_ore"), FVector(-550, 550, 0) }, { TEXT("iron_ore"), FVector(0, -650, 0) }, { TEXT("stone"), FVector(-720, 260, 0) } };
		else if (Theme == TEXT("foxfire_cellar")) Ores = { { TEXT("tin_ore"), FVector(-550, -550, 0) }, { TEXT("iron_ore"), FVector(-550, 550, 0) }, { TEXT("iron_ore"), FVector(0, -650, 0) } };
		else Ores = { { TEXT("iron_ore"), FVector(-550, -550, 0) }, { TEXT("iron_ore"), FVector(-550, 550, 0) }, { TEXT("iron_ore"), FVector(0, -650, 0) }, { TEXT("iron_ore"), FVector(-700, 0, 0) }, { TEXT("clay"), FVector(600, -380, 0) } };
	}
	// Star iron (cooks/build-crafting.md 2.1, ores by depth band 4): the highland caves' deepest chamber, back past the
	// iron, 2.8 m clear of the other veins.
	if (GetActorLocation().Z >= WRTune(TEXT("caves.starIronMinZ"), 20000.f))
	{
		Ores.Add({ TEXT("star_iron_ore"), FVector(-650, -280, 0) });
	}
	for (const TPair<FName, FVector>& Ore : Ores)
	{
		const FTransform T(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), Floor(Ore.Value));
		if (AWRResourceNode* Node = GetWorld()->SpawnActorDeferred<AWRResourceNode>(AWRResourceNode::StaticClass(), T, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			Node->ItemId = Ore.Key;
			Node->GemChance = (Biome == TEXT("swamp") && Theme == TEXT("gullet")) ? WRTune(TEXT("swampWorld.gulletGemChance"), 0.2f) : WRTune(TEXT("caves.gemChance"), 0.12f);
			if (bForestCave && Theme == TEXT("sinkroot")) Node->GemChance = WRTune(TEXT("forestWorld.sinkrootGemChance"), 0.2f);
			if (bForestCave && Ore.Key == TEXT("stone"))
			{
				// The Root-Mine's gem pocket: a pick and six blows (cooks/biome-forest.md 2.6 C3).
				Node->Hits = int32(WRTune(TEXT("forestWorld.gemPocketHits"), 6.f));
				Node->GemChance = WRTune(TEXT("forestWorld.gemPocketChance"), 0.5f);
			}
			Node->FinishSpawning(T);
			UE_LOG(LogWildreach, Verbose, TEXT("CAVE %s: %s at %s (%.0f cm over the cave's origin)"), *GetName(), *Ore.Key.ToString(), *T.GetLocation().ToCompactString(), T.GetLocation().Z - GetActorLocation().Z);
		}
	}
	// Cave hazard: burning sulfur gas (volcanic) or slick ice (tundra). Permanent. Storm peaks' caves have none: their
	// danger is the Coilburrows and the dark.
	const bool bStormCave = Biome == TEXT("storm_peaks");
	const bool bSwampCave = Biome == TEXT("swamp");
	if (!bStormCave && !bSwampCave && !bForestCave)
	{
		AWRHazard::Spawn(GetWorld(), nullptr, Floor(FVector(250, 0, 0)), bFrost ? 450.f : 260.f, 0.f,
			WRTune(TEXT("caves.gasDps"), 6.f), bFrost ? FName(TEXT("ice")) : FName(TEXT("gas")));
	}
	// The Swamp's caves (cooks/biome-swamp.md 2.6): Reedmouth's Lanterntoads and Slimeshells, the Eel Hole's leeches, the
	// Peat Cellar's Bogstalkers and leeches, the Gullet's leeches and the Murkjaw in its sump.
	if (bSwampCave)
	{
		TArray<TPair<FName, int32>> Pack;
		if (Theme == TEXT("eel_hole")) Pack = { { TEXT("gorgeleech"), 3 } };
		else if (Theme == TEXT("peat_cellar")) Pack = { { TEXT("bogstalker"), 2 }, { TEXT("gorgeleech"), 2 } };
		else if (Theme == TEXT("gullet")) Pack = { { TEXT("gorgeleech"), 4 }, { TEXT("murkjaw"), 1 } };
		else Pack = { { TEXT("lanterntoad"), 4 }, { TEXT("slimeshell"), 3 } };
		for (int32 p = 0; p < Pack.Num(); ++p)
		{
			const FTransform PackAt(FRotator::ZeroRotator, Floor(FVector(-300.f + 500.f * p, p % 2 ? 300.f : -200.f, 0.f)));
			if (AWRCreatureSpawner* Spawner = GetWorld()->SpawnActorDeferred<AWRCreatureSpawner>(AWRCreatureSpawner::StaticClass(), PackAt, this))
			{
				Spawner->CreatureId = Pack[p].Key;
				Spawner->Count = Pack[p].Value;
				Spawner->Radius = 350.f;
				Spawner->RespawnSeconds = WRTune(TEXT("swampWorld.caveRespawnSeconds"), 420.f);
				Spawner->FinishSpawning(PackAt);
			}
		}
		UE_LOG(LogWildreach, Log, TEXT("CAVE %s (swamp, %s) at %s: %d ore nodes, %d packs"), *GetName(), *Theme.ToString(), *GetActorLocation().ToCompactString(), Ores.Num(), Pack.Num());
		return;
	}
	// The Ancient forest's caves (cooks/biome-forest.md 2.6): no gas, no ice; their danger is what lives in them. The
	// Badger Sett's Trufflers (a sounder dens there), one Rootcoil in the Alder Grotto, a Mossmaul in the Root-Mine,
	// two Rootcoils in the Foxfire Cellar, a Mossmaul and two Rootcoils at the Sinkroot's bottom.
	if (bForestCave)
	{
		TArray<TPair<FName, int32>> ForestPack;
		if (Theme == TEXT("badger_sett")) ForestPack = { { TEXT("truffler"), 3 } };
		else if (Theme == TEXT("alder_grotto")) ForestPack = { { TEXT("rootcoil"), 1 } };
		else if (Theme == TEXT("root_mine")) ForestPack = { { TEXT("mossmaul"), 1 } };
		else if (Theme == TEXT("foxfire_cellar")) ForestPack = { { TEXT("rootcoil"), 2 } };
		else ForestPack = { { TEXT("mossmaul"), 1 }, { TEXT("rootcoil"), 2 } };
		for (int32 p = 0; p < ForestPack.Num(); ++p)
		{
			const FTransform PackAt(FRotator::ZeroRotator, Floor(FVector(-350.f + 450.f * p, p % 2 ? 320.f : -260.f, 0.f)));
			if (AWRCreatureSpawner* Spawner = GetWorld()->SpawnActorDeferred<AWRCreatureSpawner>(AWRCreatureSpawner::StaticClass(), PackAt, this))
			{
				Spawner->CreatureId = ForestPack[p].Key;
				Spawner->Count = ForestPack[p].Value;
				Spawner->Radius = 350.f;
				Spawner->RespawnSeconds = WRTune(TEXT("forestWorld.caveRespawnSeconds"), 480.f);
				Spawner->FinishSpawning(PackAt);
			}
		}
		UE_LOG(LogWildreach, Log, TEXT("CAVE %s (forest, %s) at %s: %d ore nodes, %d packs"), *GetName(), *Theme.ToString(), *GetActorLocation().ToCompactString(), Ores.Num(), ForestPack.Num());
		return;
	}
	// A burrower colony (Kilnmites nest here and mine ore once tamed; Glacimaws lurk under the ice).
	const FTransform SpawnAt(FRotator::ZeroRotator, Floor(FVector(-300, 0, 0)));
	if (AWRCreatureSpawner* Spawner = GetWorld()->SpawnActorDeferred<AWRCreatureSpawner>(AWRCreatureSpawner::StaticClass(), SpawnAt, this))
	{
		Spawner->CreatureId = bStormCave ? FName(TEXT("coilburrow")) : bFrost ? FName(TEXT("glacimaw")) : FName(TEXT("kilnmite"));
		Spawner->Count = bStormCave ? 3 : bFrost ? 1 : 2;
		Spawner->Radius = 300.f;
		Spawner->RespawnSeconds = 300.f;
		Spawner->FinishSpawning(SpawnAt);
	}
	UE_LOG(LogWildreach, Log, TEXT("CAVE %s (%s) at %s yaw %.0f, variant %d, size x%.2f: %d ore nodes (ground found after %d s)"), *GetName(), *Biome.ToString(),
		*GetActorLocation().ToCompactString(), GetActorRotation().Yaw, Variant, GetActorScale3D().X, Ores.Num(), ContentTries);
}
