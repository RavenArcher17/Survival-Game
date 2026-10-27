#include "WRBuilding.h"
#include "WRLifeSkills.h"
#include "WRSocial.h"
#include "Wildreach.h"
#include "WRCrafting.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRTuning.h"
#include "WRFx.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

// ---------------------------------------------------------------- registry

static TArray<TPair<FName, int32>> ParseCost(const TSharedPtr<FJsonObject>& O, const TCHAR* Field)
{
	TArray<TPair<FName, int32>> Out;
	const TSharedPtr<FJsonObject>* Cost = nullptr;
	if (O.IsValid() && O->TryGetObjectField(Field, Cost))
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& C : (*Cost)->Values)
		{
			Out.Add({ FName(*C.Key), static_cast<int32>(C.Value->AsNumber()) });
		}
	}
	return Out;
}

FWRBuildRegistry& FWRBuildRegistry::Get()
{
	static FWRBuildRegistry Instance;
	return Instance;
}

FWRBuildRegistry::FWRBuildRegistry()
{
	Span.Add(TEXT("wood"), 4);
	Span.Add(TEXT("stone"), 6);
	const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("pieces.json"));
	if (!Root.IsValid())
	{
		return;
	}
	Root->TryGetNumberField(TEXT("gridSize"), GridSize);
	Root->TryGetNumberField(TEXT("decayPerHourWhenUnpaid"), DecayPerHourWhenUnpaid);
	const TSharedPtr<FJsonObject>* SpanObj = nullptr;
	if (Root->TryGetObjectField(TEXT("span"), SpanObj))
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& S : (*SpanObj)->Values) Span.Add(FName(*S.Key), static_cast<int32>(S.Value->AsNumber()));
	}
	const TSharedPtr<FJsonObject>* Upkeep = nullptr;
	if (Root->TryGetObjectField(TEXT("upkeepPerPiecePerHour"), Upkeep))
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& S : (*Upkeep)->Values) UpkeepPerPiecePerHour.Add(FName(*S.Key), S.Value->AsNumber());
	}
	const TSharedPtr<FJsonObject>* Claim = nullptr;
	if (Root->TryGetObjectField(TEXT("claim"), Claim))
	{
		(*Claim)->TryGetNumberField(TEXT("radius"), ClaimRadius);
		(*Claim)->TryGetNumberField(TEXT("radiusPerUpgrade"), ClaimRadiusPerUpgrade);
		(*Claim)->TryGetNumberField(TEXT("maxRadius"), ClaimMaxRadius);
		ClaimUpgradeCost = ParseCost(*Claim, TEXT("upgradeCost"));
	}
	const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
	if (Root->TryGetArrayField(TEXT("pieces"), List))
	{
		for (const TSharedPtr<FJsonValue>& V : *List)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			FWRPieceDef D;
			D.Id = FName(*O->GetStringField(TEXT("id")));
			D.Name = O->GetStringField(TEXT("name"));
			D.Category = FName(*O->GetStringField(TEXT("category")));
			D.Kind = FName(*O->GetStringField(TEXT("kind")));
			FString S;
			if (O->TryGetStringField(TEXT("shape"), S)) D.Shape = FName(*S);
			if (O->TryGetStringField(TEXT("material"), S)) D.Material = FName(*S);
			if (O->TryGetStringField(TEXT("station"), S)) D.Station = FName(*S);
			O->TryGetNumberField(TEXT("hp"), D.HP);
			O->TryGetBoolField(TEXT("lockable"), D.bLockable);
			D.Cost = ParseCost(O, TEXT("cost"));
			Pieces.Add(D);
		}
	}
	UE_LOG(LogWildreach, Log, TEXT("Loaded %d buildable pieces"), Pieces.Num());
}

const FWRPieceDef* FWRBuildRegistry::Find(FName Id) const
{
	return Pieces.FindByPredicate([Id](const FWRPieceDef& D) { return D.Id == Id; });
}

// ---------------------------------------------------------------- rules

int32 FWRBuildRules::SpanFor(FName Material)
{
	return FWRBuildRegistry::Get().Span.FindRef(Material) > 0 ? FWRBuildRegistry::Get().Span.FindRef(Material) : 4;
}

FVector FWRBuildRules::SnapToGrid(FName Shape, const FVector& Hit, float BaseZ, float Grid, float& InOutYaw)
{
	const float Cx = FMath::RoundToFloat(Hit.X / Grid) * Grid;
	const float Cy = FMath::RoundToFloat(Hit.Y / Grid) * Grid;
	const int32 Quarter = ((FMath::RoundToInt(InOutYaw / 90.f) % 4) + 4) % 4;
	if (SlotOf(Shape) == TEXT("edge"))
	{
		// Nearest cell edge: walls run along it. A gable also keeps which end is high (R turns it round).
		const float Flip = Shape == TEXT("gable") && Quarter >= 2 ? 180.f : 0.f;
		const float Dx = Hit.X - Cx, Dy = Hit.Y - Cy;
		if (FMath::Abs(Dx) > FMath::Abs(Dy))
		{
			InOutYaw = 90.f + Flip;
			return FVector(Cx + FMath::Sign(Dx) * Grid * 0.5f, Cy, BaseZ);
		}
		InOutYaw = Flip;
		return FVector(Cx, Cy + FMath::Sign(Dy == 0.f ? 1.f : Dy) * Grid * 0.5f, BaseZ);
	}
	if (Shape == TEXT("pillar"))
	{
		InOutYaw = 0.f;
		return FVector(FMath::RoundToFloat((Hit.X - Grid * 0.5f) / Grid) * Grid + Grid * 0.5f,
			FMath::RoundToFloat((Hit.Y - Grid * 0.5f) / Grid) * Grid + Grid * 0.5f, BaseZ);
	}
	// Stairs and roofs keep which way they rise; everything else in a cell sits square.
	InOutYaw = Shape == TEXT("stairs") || Shape == TEXT("roof") ? Quarter * 90.f : 0.f;
	return FVector(Cx, Cy, BaseZ);
}

FName FWRBuildRules::SlotOf(FName Shape)
{
	static const FName Edges[] = { TEXT("wall"), TEXT("halfwall"), TEXT("door"), TEXT("doorway"), TEXT("window"), TEXT("palisade"), TEXT("gate"), TEXT("gable"), TEXT("upperwall") };
	for (const FName& E : Edges)
	{
		if (Shape == E) return TEXT("edge");
	}
	if (Shape == TEXT("pillar")) return TEXT("corner");
	if (Shape == TEXT("stairs")) return TEXT("stairs");
	return Shape.IsNone() ? NAME_None : FName(TEXT("cell"));
}

bool FWRBuildRules::IsBearing(FName Shape)
{
	return SlotOf(Shape) == TEXT("edge") || SlotOf(Shape) == TEXT("corner") || Shape == TEXT("foundation") || Shape == TEXT("tower");
}

int32 FWRBuildRules::SupportCost(const FVector& From, FName FromShape, const FVector& To, float Grid)
{
	const FVector D = To - From;
	if (D.Size2D() > Grid * 1.05f || FMath::Abs(D.Z) > Storey + 20.f)
	{
		return INDEX_NONE;
	}
	// Stacked straight on a wall / pillar (a storey up) costs nothing; beside something, one step of the material's span.
	return IsBearing(FromShape) && D.Size2D() < 30.f && D.Z > 0.f ? 0 : 1;
}

int32 FWRBuildRules::LevelFor(FName Shape, float PointZ, float Top)
{
	// A floor's surface is 20 above its storey's base, so aiming at it (or anywhere up a wall) picks that storey.
	const int32 Into = FMath::Max(0, FMath::FloorToInt((PointZ - Top + 5.f) / Storey));
	return Shape == TEXT("floor") || Shape == TEXT("roof") || Shape == TEXT("gable") || Shape == TEXT("upperwall") ? Into + 1 : Into;
}

TMap<FName, float> FWRBuildRules::UpkeepPerHour(const TMap<FName, int32>& PiecesByMaterial)
{
	TMap<FName, float> Out;
	for (const TPair<FName, int32>& P : PiecesByMaterial)
	{
		Out.FindOrAdd(P.Key) += P.Value * FWRBuildRegistry::Get().UpkeepPerPiecePerHour.FindRef(P.Key);
	}
	return Out;
}

bool FWRBuildRules::PayUpkeepMinute(TArray<FWRItemStack>& Storage, TMap<FName, float>& Owed, const TMap<FName, float>& PerHour)
{
	bool bPaid = true;
	for (const TPair<FName, float>& Cost : PerHour)
	{
		if (Cost.Value <= 0.f) continue;
		float& Due = Owed.FindOrAdd(Cost.Key);
		Due += Cost.Value / 60.f;
		const int32 Need = FMath::FloorToInt(Due + KINDA_SMALL_NUMBER);
		if (Need > 0)
		{
			if (FWRInventoryRules::CountOf(Storage, Cost.Key) >= Need) FWRInventoryRules::Remove(Storage, Cost.Key, Need);
			else bPaid = false;
			Due -= Need;
		}
		// Nothing left of a material the claim needs: unpaid from now, not only on the minutes a whole item falls due.
		if (FWRInventoryRules::CountOf(Storage, Cost.Key) <= 0) bPaid = false;
	}
	return bPaid;
}

static bool CanPlayersShare(const FString& Owner, const FString& Player)
{
	return FWRSocialRules::CanShare(Owner, Player);
}

// ---------------------------------------------------------------- piece

AWRBuildPiece::AWRBuildPiece()
{
	bReplicates = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Main = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Main"));
	Main->SetupAttachment(RootComponent);
	Leaf = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Leaf"));
	Leaf->SetupAttachment(RootComponent);
	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(RootComponent);
	Light->SetVisibility(false);
	Light->SetIntensity(6000.f);
	Light->SetAttenuationRadius(1200.f);
	Light->SetLightColor(FLinearColor(1.f, 0.6f, 0.3f));
}

void AWRBuildPiece::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRBuildPiece, PieceId);
	DOREPLIFETIME(AWRBuildPiece, OwnerName);
	DOREPLIFETIME(AWRBuildPiece, Health);
	DOREPLIFETIME(AWRBuildPiece, MaxHealth);
	DOREPLIFETIME(AWRBuildPiece, bOpen);
	DOREPLIFETIME(AWRBuildPiece, bLocked);
	DOREPLIFETIME(AWRBuildPiece, CropReadyAt);
	DOREPLIFETIME(AWRBuildPiece, bPlanted);
	DOREPLIFETIME(AWRBuildPiece, Storage);
	DOREPLIFETIME(AWRBuildPiece, Support);
}

void AWRBuildPiece::InitPiece(FName InPieceId, const FString& InOwner)
{
	PieceId = InPieceId;
	OwnerName = InOwner;
	if (const FWRPieceDef* Def = GetDef())
	{
		MaxHealth = Health = Def->HP;
		if (Def->Kind == TEXT("chest"))
		{
			Storage.SetNum(20);
		}
	}
}

void AWRBuildPiece::BeginPlay()
{
	Super::BeginPlay();
	ApplyLook();
	if (HasAuthority() && !bGhost)
	{
		SpawnChild();
	}
}

void AWRBuildPiece::EndPlay(const EEndPlayReason::Type Reason)
{
	// Demolished, smashed or collapsed: debris of its material and dust for everyone (not when the world closes).
	const UWorld* World = GetWorld();
	const FWRPieceDef* Def = GetDef();
	if (Reason == EEndPlayReason::Destroyed && HasAuthority() && !bGhost && Def && World && !World->bIsTearingDown)
	{
		FVector Origin, Extent;
		GetActorBounds(false, Origin, Extent);
		if (Extent.IsNearlyZero()) { Origin = GetActorLocation(); Extent = FVector(60.f); }
		const FName Effect = Def->Material == TEXT("wood") ? TEXT("build.break.wood") : Def->Material == TEXT("stone") ? TEXT("build.break.stone") : TEXT("build.break");
		WRFx::Broadcast(this, Effect, Origin, GetActorRotation(), FMath::Clamp(Extent.Size() / 150.f, 0.4f, 3.f), TEXT("build.demolish"));
	}
	if (AActor* Child = ChildActor.Get())
	{
		Child->Destroy();
	}
	Super::EndPlay(Reason);
}

void AWRBuildPiece::OnRep_Piece()
{
	ApplyLook();
}

void AWRBuildPiece::OnRep_Door()
{
	// The leaf swings 90 degrees about its hinge edge (the opening's -X side), clear of the doorway.
	const FVector Size = Leaf->GetRelativeScale3D() * 100.f;
	Leaf->SetRelativeLocationAndRotation(bOpen ? FVector(-Size.X * 0.5f, Size.X * 0.5f, Size.Z * 0.5f) : FVector(0.f, 0.f, Size.Z * 0.5f),
		FRotator(0.f, bOpen ? 90.f : 0.f, 0.f));
	Leaf->SetCollisionEnabled(bOpen || bGhost ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
}

void AWRBuildPiece::SpawnChild()
{
	const FWRPieceDef* Def = GetDef();
	if (!Def || ChildActor.IsValid())
	{
		return;
	}
	UClass* Class = Def->Kind == TEXT("station") ? AWRStation::StaticClass()
		: Def->Kind == TEXT("hearth") ? AWRHearth::StaticClass()
		: Def->Kind == TEXT("totem") ? AWRClaimTotem::StaticClass() : nullptr;
	if (!Class)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Child = GetWorld()->SpawnActor<AActor>(Class, GetActorLocation() + FVector(0.f, 0.f, 50.f), GetActorRotation(), Params);
	if (AWRStation* Station = Cast<AWRStation>(Child))
	{
		Station->StationType = Def->Station;
	}
	if (AWRClaimTotem* Totem = Cast<AWRClaimTotem>(Child))
	{
		Totem->OwnerName = OwnerName;
		Totem->Radius = FWRBuildRegistry::Get().ClaimRadius;
	}
	ChildActor = Child;
}

/**
 * Loaded once and rooted. A static raw pointer isn't a GC reference: once no piece is near (they leave a client's
 * relevancy as you walk away from your base) the asset was collected, and the next piece to arrive read freed memory.
 */
template <typename T>
static T* LoadRooted(const TCHAR* Path)
{
	T* Obj = LoadObject<T>(nullptr, Path);
	if (Obj) Obj->AddToRoot();
	return Obj;
}

static UStaticMesh* CubeMesh()
{
	static UStaticMesh* Mesh = LoadRooted<UStaticMesh>(TEXT("/Engine/BasicShapes/Cube.Cube"));
	return Mesh;
}

static UMaterialInterface* ShapeMat()
{
	static UMaterialInterface* Mat = LoadRooted<UMaterialInterface>(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	return Mat;
}

/** Planks for wood (and most furniture), blocks for stone and the brazier; null = keep the flat tint (crops, cloth). */
static UMaterialInterface* SurfaceFor(const FWRPieceDef& Def)
{
	if (Def.Kind == TEXT("crop") || Def.Id == TEXT("rug") || Def.Id == TEXT("banner"))
	{
		return nullptr;
	}
	static UMaterialInterface* Wood = LoadRooted<UMaterialInterface>(TEXT("/Game/Building/M_BuildWood.M_BuildWood"));
	static UMaterialInterface* Stone = LoadRooted<UMaterialInterface>(TEXT("/Game/Building/M_BuildStone.M_BuildStone"));
	return Def.Material == TEXT("stone") || Def.Id == TEXT("brazier") ? Stone : Wood;
}

void AWRBuildPiece::ApplyLook()
{
	const FWRPieceDef* Def = GetDef();
	if (!Def || bLookApplied)
	{
		return;
	}
	bLookApplied = true;

	// Parts: (size cm, centre offset cm) of cube blocks; a door/gate also gets a hinged leaf. Edge pieces run on past the
	// cell edge by half their thickness at each end, so two walls meeting at a corner close it.
	struct FPart { FVector Size; FVector Offset; FRotator Rot = FRotator::ZeroRotator; bool bCollide = true; bool bVisible = true; };
	TArray<FPart> Parts;
	FVector LeafSize = FVector::ZeroVector;
	const FName S = Def->Shape;
	const FName K = Def->Kind;
	const float Len = S == TEXT("palisade") || S == TEXT("gate") ? 440.f : 420.f;
	// A deep slab: the top is the floor (100 above the piece), the rest sinks into the ground so slopes don't show a gap.
	if (S == TEXT("foundation")) Parts.Add({ FVector(400, 400, 250), FVector(0, 0, -25) });
	else if (S == TEXT("wall")) Parts.Add({ FVector(Len, 20, 300), FVector(0, 0, 150) });
	else if (S == TEXT("halfwall")) Parts.Add({ FVector(Len, 20, 150), FVector(0, 0, 75) });
	else if (S == TEXT("upperwall")) Parts.Add({ FVector(Len, 20, 210), FVector(0, 0, 105) });  // up into the roof slab, no gap
	else if (S == TEXT("floor")) Parts.Add({ FVector(400, 400, 20), FVector(0, 0, 10) });
	else if (S == TEXT("roof"))
	{
		// Rises 200 across the cell toward +X (yaw turns it), resting on the walls at the low edge with 40 of eaves past
		// it, and runs a little past the high edge so two roofs meeting at a ridge close it. Gables close the open sides.
		const float Slope = FMath::Atan2(200.f, 400.f);
		const float MidX = -15.f;  // the slab runs -240 .. 210 along X
		const float UnderZ = 0.5f * (MidX + 200.f);
		Parts.Add({ FVector(450.f / FMath::Cos(Slope), Len, 15), FVector(MidX - 7.5f * FMath::Sin(Slope), 0, UnderZ + 7.5f * FMath::Cos(Slope)),
			FRotator(FMath::RadiansToDegrees(Slope), 0, 0) });
	}
	else if (S == TEXT("pillar")) Parts.Add({ FVector(40, 40, 300), FVector(0, 0, 150) });
	else if (S == TEXT("stairs"))
	{
		// Eight treads rising a storey across the cell toward +X (yaw turns it) between two stringers. You walk on a
		// hidden ramp through the treads' middles that ends level with the floor above.
		const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(300.f, 400.f));
		Parts.Add({ FVector(500, 200, 10), FVector(3, 0, 164.75f), FRotator(Pitch, 0, 0), true, false });
		for (int32 k = 1; k <= 8; ++k)
		{
			Parts.Add({ FVector(52, 200, 10), FVector(-225 + 50 * k, 0, 37.5f * k - 5), FRotator::ZeroRotator, false });
		}
		Parts.Add({ FVector(500, 8, 30), FVector(0, -104, 135), FRotator(Pitch, 0, 0), false });
		Parts.Add({ FVector(500, 8, 30), FVector(0, 104, 135), FRotator(Pitch, 0, 0), false });
	}
	else if (S == TEXT("window"))
	{
		Parts.Add({ FVector(Len, 20, 100), FVector(0, 0, 50) });
		Parts.Add({ FVector(Len, 20, 80), FVector(0, 0, 260) });
		Parts.Add({ FVector(130, 20, 120), FVector(-145, 0, 160) });
		Parts.Add({ FVector(130, 20, 120), FVector(145, 0, 160) });
	}
	else if (S == TEXT("door") || S == TEXT("gate") || S == TEXT("doorway"))
	{
		// A doorway is the frame without the leaf (between rooms).
		const float Opening = S == TEXT("gate") ? 300.f : 140.f;
		const float Side = (Len - Opening) * 0.5f;
		const float Height = S == TEXT("gate") ? 400.f : 300.f;
		Parts.Add({ FVector(Side, 20, Height), FVector(-Len * 0.5f + Side * 0.5f, 0, Height * 0.5f) });
		Parts.Add({ FVector(Side, 20, Height), FVector(Len * 0.5f - Side * 0.5f, 0, Height * 0.5f) });
		Parts.Add({ FVector(Opening, 20, Height - 240), FVector(0, 0, 240 + (Height - 240) * 0.5f) });
		if (S != TEXT("doorway"))
		{
			LeafSize = FVector(Opening, 10, 240);
		}
	}
	else if (S == TEXT("palisade")) Parts.Add({ FVector(Len, 40, 400), FVector(0, 0, 200) });
	else if (S == TEXT("tower"))
	{
		Parts.Add({ FVector(200, 200, 500), FVector(0, 0, 250) });
		Parts.Add({ FVector(400, 400, 20), FVector(0, 0, 510) });
	}
	else if (K == TEXT("chest")) Parts.Add({ FVector(100, 60, 60), FVector(0, 0, 30) });
	else if (K == TEXT("bed")) Parts.Add({ Def->Id == TEXT("bedroll") ? FVector(200, 80, 15) : FVector(220, 100, 50), FVector(0, 0, Def->Id == TEXT("bedroll") ? 8 : 25) });
	else if (K == TEXT("light")) Parts.Add({ Def->Id == TEXT("brazier") ? FVector(60, 60, 80) : FVector(10, 10, 150), FVector(0, 0, Def->Id == TEXT("brazier") ? 40 : 75) });
	else if (K == TEXT("crop")) Parts.Add({ FVector(400, 400, 30), FVector(0, 0, 15) });
	else if (K == TEXT("pen"))
	{
		Parts.Add({ FVector(800, 20, 120), FVector(0, -400, 60) });
		Parts.Add({ FVector(800, 20, 120), FVector(0, 400, 60) });
		Parts.Add({ FVector(20, 800, 120), FVector(-400, 0, 60) });
		Parts.Add({ FVector(20, 800, 120), FVector(400, 0, 60) });
	}
	// Stations, the Hearth and the totem are child actors only the server spawns: the build-mode ghost previews their size.
	else if (bGhost && K == TEXT("station")) Parts.Add({ FVector(120, 80, 90), FVector(0, 0, 50) });
	else if (bGhost && K == TEXT("hearth")) Parts.Add({ FVector(150, 150, 120), FVector(0, 0, 50) });
	else if (bGhost && K == TEXT("totem")) Parts.Add({ FVector(40, 40, 250), FVector(0, 0, 50) });
	else if (Def->Id == TEXT("table")) Parts.Add({ FVector(150, 90, 80), FVector(0, 0, 40) });
	else if (Def->Id == TEXT("chair")) Parts.Add({ FVector(50, 50, 90), FVector(0, 0, 45) });
	else if (Def->Id == TEXT("rug")) Parts.Add({ FVector(200, 150, 2), FVector(0, 0, 1) });
	else if (Def->Id == TEXT("banner")) Parts.Add({ FVector(10, 80, 250), FVector(0, 0, 125) });
	else if (Def->Id == TEXT("trophy_mount")) Parts.Add({ FVector(80, 20, 80), FVector(0, 0, 150) });

	FLinearColor Color = Def->Material == TEXT("stone") ? FLinearColor(0.45f, 0.45f, 0.47f) : FLinearColor(0.42f, 0.27f, 0.13f);
	if (K == TEXT("crop")) Color = FLinearColor(0.25f, 0.16f, 0.08f);
	if (Def->Id == TEXT("rug") || Def->Id == TEXT("banner")) Color = FLinearColor(0.5f, 0.1f, 0.1f);
	// Built pieces wear world-aligned planks or stone blocks (Tools/make_build_materials.py); ghosts stay a flat tint.
	UMaterialInterface* Surface = bGhost ? nullptr : SurfaceFor(*Def);
	const auto Paint = [this, Color, Surface](UMeshComponent* C)
	{
		if (Surface)
		{
			C->SetMaterial(0, Surface);
			return;
		}
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(ShapeMat(), this);
		M->SetVectorParameterValue(TEXT("Color"), Color);
		C->SetMaterial(0, M);
	};
	const auto Configure = [this, &Paint](UStaticMeshComponent* C, const FPart& P)
	{
		C->SetStaticMesh(CubeMesh());
		C->SetRelativeLocation(P.Offset);
		C->SetRelativeRotation(P.Rot);
		C->SetRelativeScale3D(P.Size / 100.f);
		C->SetCollisionProfileName(bGhost || !P.bCollide ? TEXT("NoCollision") : TEXT("BlockAll"));
		C->SetVisibility(P.bVisible);  // the stairs' walking ramp is collision only
		Paint(C);
	};

	if (Parts.Num() > 0)
	{
		Configure(Main, Parts[0]);
		for (int32 i = 1; i < Parts.Num(); ++i)
		{
			UStaticMeshComponent* Extra = NewObject<UStaticMeshComponent>(this);
			Extra->SetupAttachment(RootComponent);
			Extra->RegisterComponent();
			Configure(Extra, Parts[i]);
		}
	}
	else
	{
		Main->SetVisibility(false);
		Main->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (S == TEXT("gable"))
	{
		// A right-angled triangle on top of a wall, low at -X and 210 up at +X (yaw 180 flips it): under a roof that rises
		// the same way it closes the open side, poking a little into the roof slab so no light shows through.
		UProceduralMeshComponent* Tri = NewObject<UProceduralMeshComponent>(this);
		Tri->SetupAttachment(RootComponent);
		Tri->RegisterComponent();
		const float H = Len * 0.5f;
		const FVector A(-H, 0, 0), B(H, 0, 0), C(H, 0, H), T(0, 10, 0);
		TArray<FVector> Verts, Normals;
		TArray<int32> Tris;
		const auto Face = [&Verts, &Normals, &Tris](const TArray<FVector>& Pts, const FVector& Normal)
		{
			// Unreal draws a triangle's front where its (P1 - P0) x (P2 - P0) points away from the viewer.
			const int32 First = Verts.Num();
			for (const FVector& P : Pts) { Verts.Add(P); Normals.Add(Normal); }
			for (int32 i = 1; i + 1 < Pts.Num(); ++i)
			{
				const bool bFlip = FVector::DotProduct(FVector::CrossProduct(Pts[i] - Pts[0], Pts[i + 1] - Pts[0]), Normal) > 0.f;
				Tris.Append({ First, First + (bFlip ? i + 1 : i), First + (bFlip ? i : i + 1) });
			}
		};
		Face({ A + T, B + T, C + T }, FVector(0, 1, 0));
		Face({ A - T, B - T, C - T }, FVector(0, -1, 0));
		Face({ A - T, B - T, B + T, A + T }, FVector(0, 0, -1));
		Face({ B - T, C - T, C + T, B + T }, FVector(1, 0, 0));
		Face({ A - T, C - T, C + T, A + T }, FVector(-0.5f, 0, 1).GetSafeNormal());
		Tri->CreateMeshSection(0, Verts, Tris, Normals, TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>(), !bGhost);
		Tri->SetCollisionProfileName(bGhost ? TEXT("NoCollision") : TEXT("BlockAll"));
		Paint(Tri);
	}

	if (!LeafSize.IsZero())
	{
		// The door leaf (swings open on its hinge, see OnRep_Door).
		Configure(Leaf, { LeafSize, FVector(0.f, 0.f, LeafSize.Z * 0.5f) });
		OnRep_Door();
	}
	else
	{
		Leaf->SetVisibility(false);
		Leaf->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (K == TEXT("light"))
	{
		Light->SetVisibility(!bGhost);
		Light->SetRelativeLocation(FVector(0.f, 0.f, Def->Id == TEXT("brazier") ? 100.f : 160.f));
	}
}

void AWRBuildPiece::SetGhostValid(bool bValid)
{
	const FLinearColor Color = bValid ? FLinearColor(0.2f, 0.9f, 0.3f) : FLinearColor(0.9f, 0.2f, 0.2f);
	TArray<UMeshComponent*> Comps;
	GetComponents(Comps);
	for (UMeshComponent* C : Comps)
	{
		if (UMaterialInstanceDynamic* M = Cast<UMaterialInstanceDynamic>(C->GetMaterial(0)))
		{
			M->SetVectorParameterValue(TEXT("Color"), Color);
		}
	}
}

bool AWRBuildPiece::CanAccess(const FString& PlayerName) const
{
	return CanPlayersShare(OwnerName, PlayerName);
}

void AWRBuildPiece::TakeBuildDamage(float Amount, AActor* DamageSource)
{
	if (!HasAuthority() || Amount <= 0.f || IsActorBeingDestroyed())
	{
		return;
	}
	// Offline base protection: less damage while the owner and their clan are all offline.
	const float Protection = WRTune(TEXT("server.offlineProtection"), 0.f);
	if (Protection > 0.f && !FWRSocialRules::IsAnyoneOnline(GetWorld(), OwnerName))
	{
		Amount *= 1.f - FMath::Clamp(Protection, 0.f, 1.f);
	}
	Health -= Amount;
	if (AWRHearth* Hearth = Cast<AWRHearth>(ChildActor.Get()))
	{
		Hearth->TakeRaidDamage(Amount);
	}
	if (Health <= 0.f)
	{
		UE_LOG(LogWildreach, Log, TEXT("BUILD %s (%s) destroyed by %s"), *GetName(), *PieceId.ToString(), *GetNameSafe(DamageSource));
		const FVector Where = GetActorLocation();
		UWorld* World = GetWorld();
		Destroy();
		RecomputeSupport(World, Where, 3000.f);
	}
}

FString AWRBuildPiece::Interact(AWRPlayerController* Player)
{
	const FWRPieceDef* Def = GetDef();
	AWRPlayerState* PS = Player ? Player->GetPlayerState<AWRPlayerState>() : nullptr;
	if (!Def || !PS || !HasAuthority())
	{
		return FString();
	}
	const FString Who = PS->GetPlayerName();
	const FName K = Def->Kind;

	if (K == TEXT("door"))
	{
		if (bLocked && !CanAccess(Who) && !UnlockedFor.Contains(Who))
		{
			return TEXT("Locked. Type WRCode <code> to unlock.");
		}
		bOpen = !bOpen;
		OnRep_Door();
		return bOpen ? TEXT("Opened") : TEXT("Closed");
	}
	if (K == TEXT("bed"))
	{
		if (!CanAccess(Who))
		{
			return TEXT("Not your bed");
		}
		PS->RespawnPoint = this;
		return TEXT("Respawn point set");
	}
	if (K == TEXT("crop"))
	{
		const float Now = GetWorld()->GetTimeSeconds();
		if (!bPlanted)
		{
			if (PS->GetInventory()->CountOf(TEXT("seeds")) < 1)
			{
				return TEXT("Need seeds to plant (berry bushes drop them)");
			}
			PS->GetInventory()->RemoveItem(TEXT("seeds"), 1);
			bPlanted = true;
			CropReadyAt = Now + WRTune(TEXT("crops.commonMinutes"), 20.f) * 60.f / FMath::Max(0.1f, WRTune(TEXT("server.cropMultiplier"), 1.f))
				/ (1.f + UWRLifeSkillsComponent::StatFor(PS, TEXT("cropSpeed")));
			UWRLifeSkillsComponent::Award(PS, TEXT("farming"), TEXT("plant"), 10.f);
			return FString::Printf(TEXT("Planted. Ready in %.0f min"), (CropReadyAt - Now) / 60.f);
		}
		if (Now < CropReadyAt)
		{
			return FString::Printf(TEXT("Growing: %.0f min left"), (CropReadyAt - Now) / 60.f);
		}
		const int32 Berries = FMath::RoundToInt(6.f * (1.f + UWRLifeSkillsComponent::StatFor(PS, TEXT("harvestExtra"))) * WRTune(TEXT("server.gatherMultiplier"), 1.f));
		const int32 Seeds = 2 + FMath::RoundToInt(UWRLifeSkillsComponent::StatFor(PS, TEXT("seedExtra")));
		PS->GetInventory()->AddItem(TEXT("berries"), Berries);
		PS->GetInventory()->AddItem(TEXT("seeds"), Seeds);
		UWRLifeSkillsComponent::Award(PS, TEXT("farming"), TEXT("harvest"), 20.f);
		// Endless field: replants itself.
		bPlanted = UWRLifeSkillsComponent::StatFor(PS, TEXT("autoReplant")) > 0.f;
		CropReadyAt = bPlanted ? Now + WRTune(TEXT("crops.commonMinutes"), 20.f) * 60.f / FMath::Max(0.1f, WRTune(TEXT("server.cropMultiplier"), 1.f))
			/ (1.f + UWRLifeSkillsComponent::StatFor(PS, TEXT("cropSpeed"))) : 0.f;
		return FString::Printf(TEXT("Harvested %d berries, %d seeds%s"), Berries, Seeds, bPlanted ? TEXT(" (replanted)") : TEXT(""));
	}
	if (K == TEXT("pen"))
	{
		int32 Collected = 0;
		for (FWRItemStack& S : Storage)
		{
			if (!S.IsEmpty() && PS->GetInventory()->AddItem(S.ItemId, S.Count) == 0)
			{
				Collected += S.Count;
				S = FWRItemStack();
			}
		}
		UWRLifeSkillsComponent::Award(PS, TEXT("husbandry"), TEXT("collect"), 5.f, Collected);
		return Collected > 0 ? FString::Printf(TEXT("Collected %d animal products"), Collected)
			: TEXT("Animal pen: keep a tamed Pumicehog or Snowpuff inside and it produces over time");
	}
	if (AWRHearth* Hearth = Cast<AWRHearth>(ChildActor.Get()))
	{
		return Hearth->DepositCores(Player);
	}
	if (AWRClaimTotem* Totem = Cast<AWRClaimTotem>(ChildActor.Get()))
	{
		return CanAccess(Who) ? Totem->Deposit(Player) : Totem->Describe();
	}
	return Def->Name;
}

int32 AWRBuildPiece::SupportAt(UWorld* World, FName PieceId, const FVector& Location, const AWRBuildPiece* Ignore)
{
	const FWRPieceDef* Def = FWRBuildRegistry::Get().Find(PieceId);
	if (!Def || !Def->Snaps())
	{
		return 0;
	}
	static const FName GroundShapes[] = { TEXT("foundation"), TEXT("pillar"), TEXT("palisade"), TEXT("tower"), TEXT("gate") };
	bool bGroundShape = false;
	for (const FName& G : GroundShapes) bGroundShape |= Def->Shape == G;
	if (bGroundShape)
	{
		// Grounded if the terrain (not another piece) is right below. Foundations sit sunk into the ground
		// (building.foundationSink), so look from above that, past any pieces in the way (this one's own slab too).
		FCollisionQueryParams Query(SCENE_QUERY_STAT(WRSupport), false, Ignore);
		const FVector Top = Location + FVector(0, 0, 60 + WRTune(TEXT("building.foundationSink"), 60.f));
		FHitResult H;
		for (int32 Try = 0; Try < 4 && World->LineTraceSingleByChannel(H, Top, Location - FVector(0, 0, 160), ECC_WorldStatic, Query); ++Try)
		{
			if (H.GetActor() && !H.GetActor()->IsA<AWRBuildPiece>())
			{
				return 0;
			}
			Query.AddIgnoredActor(H.GetActor());
		}
	}
	const float Grid = FWRBuildRegistry::Get().GridSize;
	int32 Best = TNumericLimits<int32>::Max();
	for (TActorIterator<AWRBuildPiece> It(World); It; ++It)
	{
		const FWRPieceDef* Other = It->GetDef();
		if (*It == Ignore || It->bGhost || !Other || !Other->Snaps() || It->IsActorBeingDestroyed())
		{
			continue;
		}
		const int32 Cost = FWRBuildRules::SupportCost(It->GetActorLocation(), Other->Shape, Location, Grid);
		if (Cost != INDEX_NONE && It->Support < TNumericLimits<int32>::Max())
		{
			Best = FMath::Min(Best, It->Support + Cost);
		}
	}
	return Best <= FWRBuildRules::SpanFor(Def->Material) ? Best : INDEX_NONE;
}

FString AWRBuildPiece::Blocked(UWorld* World, FName PieceId, const FVector& Location, const AWRBuildPiece* Ignore)
{
	const FWRPieceDef* Def = FWRBuildRegistry::Get().Find(PieceId);
	if (!Def || !Def->Snaps())
	{
		return FString();
	}
	const FName Slot = FWRBuildRules::SlotOf(Def->Shape);
	const bool bFlat = Def->Shape == TEXT("floor") || Def->Shape == TEXT("roof");
	for (TActorIterator<AWRBuildPiece> It(World); It; ++It)
	{
		const FWRPieceDef* Other = It->GetDef();
		if (*It == Ignore || It->bGhost || !Other || !Other->Snaps() || It->IsActorBeingDestroyed())
		{
			continue;
		}
		// One piece per slot: a door doesn't go on top of a wall (demolish the wall first), a floor not into a floor.
		const FVector D = It->GetActorLocation() - Location;
		if (FWRBuildRules::SlotOf(Other->Shape) == Slot && D.Size() < 30.f)
		{
			return FString::Printf(TEXT("something is already there (%s)"), *Other->Name);
		}
		// Stairs need the opening above them.
		const bool bOtherFlat = Other->Shape == TEXT("floor") || Other->Shape == TEXT("roof");
		if (D.Size2D() < 30.f && ((bFlat && Other->Shape == TEXT("stairs") && FMath::IsNearlyEqual(-D.Z, FWRBuildRules::Storey, 40.f))
			|| (Def->Shape == TEXT("stairs") && bOtherFlat && FMath::IsNearlyEqual(D.Z, FWRBuildRules::Storey, 40.f))))
		{
			return TEXT("the stairs need this opening");
		}
	}
	return FString();
}

void AWRBuildPiece::RecomputeSupport(UWorld* World, const FVector& Around, float Radius)
{
	TArray<AWRBuildPiece*> Pieces;
	for (TActorIterator<AWRBuildPiece> It(World); It; ++It)
	{
		const FWRPieceDef* Def = It->GetDef();
		if (Def && Def->Snaps() && !It->bGhost && !It->IsActorBeingDestroyed() && FVector::Dist(It->GetActorLocation(), Around) <= Radius)
		{
			It->Support = TNumericLimits<int32>::Max();
			Pieces.Add(*It);
		}
	}
	// Seed grounded pieces, then relax until nothing changes.
	for (AWRBuildPiece* P : Pieces)
	{
		if (SupportAt(World, P->PieceId, P->GetActorLocation(), P) == 0) P->Support = 0;
	}
	bool bChanged = true;
	for (int32 Iter = 0; bChanged && Iter < 32; ++Iter)
	{
		bChanged = false;
		for (AWRBuildPiece* P : Pieces)
		{
			if (P->Support == 0) continue;
			const float Grid = FWRBuildRegistry::Get().GridSize;
			for (AWRBuildPiece* N : Pieces)
			{
				const int32 Cost = N == P ? INDEX_NONE : FWRBuildRules::SupportCost(N->GetActorLocation(), N->GetDef()->Shape, P->GetActorLocation(), Grid);
				if (Cost != INDEX_NONE && N->Support < TNumericLimits<int32>::Max() && N->Support + Cost < P->Support)
				{
					P->Support = N->Support + Cost;
					bChanged = true;
				}
			}
		}
	}
	for (AWRBuildPiece* P : Pieces)
	{
		if (P->Support > FWRBuildRules::SpanFor(P->GetDef()->Material))
		{
			UE_LOG(LogWildreach, Log, TEXT("BUILD %s collapsed (no support)"), *P->GetName());
			P->Destroy();
		}
	}
}

TSharedRef<FJsonObject> AWRBuildPiece::ToJson() const
{
	TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
	O->SetStringField(TEXT("piece"), PieceId.ToString());
	O->SetStringField(TEXT("owner"), OwnerName);
	const FVector L = GetActorLocation();
	O->SetNumberField(TEXT("x"), L.X);
	O->SetNumberField(TEXT("y"), L.Y);
	O->SetNumberField(TEXT("z"), L.Z);
	O->SetNumberField(TEXT("yaw"), GetActorRotation().Yaw);
	O->SetNumberField(TEXT("hp"), Health);
	O->SetStringField(TEXT("code"), LockCode);
	O->SetBoolField(TEXT("locked"), bLocked);
	O->SetBoolField(TEXT("planted"), bPlanted);
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	O->SetNumberField(TEXT("cropSecondsLeft"), bPlanted ? FMath::Max(0.f, CropReadyAt - Now) : 0.f);
	TArray<TSharedPtr<FJsonValue>> Items;
	for (const FWRItemStack& S : Storage)
	{
		TSharedRef<FJsonObject> I = MakeShared<FJsonObject>();
		I->SetStringField(TEXT("id"), S.IsEmpty() ? FString() : S.ItemId.ToString());
		I->SetNumberField(TEXT("count"), S.IsEmpty() ? 0 : S.Count);
		Items.Add(MakeShared<FJsonValueObject>(I));
	}
	O->SetArrayField(TEXT("storage"), Items);
	if (const AWRHearth* Hearth = Cast<AWRHearth>(ChildActor.Get()))
	{
		TArray<TSharedPtr<FJsonValue>> Cores;
		for (const FName& C : Hearth->GetCores()) Cores.Add(MakeShared<FJsonValueString>(C.ToString()));
		O->SetArrayField(TEXT("cores"), Cores);
	}
	if (const AWRClaimTotem* Totem = Cast<AWRClaimTotem>(ChildActor.Get()))
	{
		O->SetNumberField(TEXT("claimUpgrade"), Totem->UpgradeLevel);
		TArray<TSharedPtr<FJsonValue>> Upkeep;
		for (const FWRItemStack& S : Totem->UpkeepStorage)
		{
			TSharedRef<FJsonObject> I = MakeShared<FJsonObject>();
			I->SetStringField(TEXT("id"), S.IsEmpty() ? FString() : S.ItemId.ToString());
			I->SetNumberField(TEXT("count"), S.IsEmpty() ? 0 : S.Count);
			Upkeep.Add(MakeShared<FJsonValueObject>(I));
		}
		O->SetArrayField(TEXT("upkeep"), Upkeep);
	}
	return O;
}

static void ReadStacks(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, TArray<FWRItemStack>& Out)
{
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (!O->TryGetArrayField(Field, Arr))
	{
		return;
	}
	for (int32 i = 0; i < Arr->Num(); ++i)
	{
		const TSharedPtr<FJsonObject> I = (*Arr)[i]->AsObject();
		if (!Out.IsValidIndex(i)) Out.AddDefaulted();
		const FString Id = I->GetStringField(TEXT("id"));
		if (!Id.IsEmpty())
		{
			Out[i].ItemId = FName(*Id);
			Out[i].Count = static_cast<int32>(I->GetNumberField(TEXT("count")));
		}
	}
}

void AWRBuildPiece::FromJson(const TSharedPtr<FJsonObject>& O)
{
	Health = O->GetNumberField(TEXT("hp"));
	LockCode = O->GetStringField(TEXT("code"));
	O->TryGetBoolField(TEXT("locked"), bLocked);
	O->TryGetBoolField(TEXT("planted"), bPlanted);
	double Left = 0.0;
	if (O->TryGetNumberField(TEXT("cropSecondsLeft"), Left) && bPlanted)
	{
		CropReadyAt = GetWorld()->GetTimeSeconds() + Left;
	}
	ReadStacks(O, TEXT("storage"), Storage);
	if (AWRHearth* Hearth = Cast<AWRHearth>(ChildActor.Get()))
	{
		const TArray<TSharedPtr<FJsonValue>>* Cores = nullptr;
		if (O->TryGetArrayField(TEXT("cores"), Cores))
		{
			TArray<FName> Names;
			for (const TSharedPtr<FJsonValue>& C : *Cores) Names.Add(FName(*C->AsString()));
			Hearth->SetCores(Names);
		}
	}
	if (AWRClaimTotem* Totem = Cast<AWRClaimTotem>(ChildActor.Get()))
	{
		double Up = 0.0;
		if (O->TryGetNumberField(TEXT("claimUpgrade"), Up))
		{
			Totem->UpgradeLevel = static_cast<int32>(Up);
			Totem->Radius = FMath::Min(FWRBuildRegistry::Get().ClaimMaxRadius, FWRBuildRegistry::Get().ClaimRadius + Totem->UpgradeLevel * FWRBuildRegistry::Get().ClaimRadiusPerUpgrade);
		}
		ReadStacks(O, TEXT("upkeep"), Totem->UpkeepStorage);
	}
}

// ---------------------------------------------------------------- claim totem

AWRClaimTotem::AWRClaimTotem()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 1.f;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cyl(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (Cyl.Succeeded()) Mesh->SetStaticMesh(Cyl.Object);
	if (Mat.Succeeded()) Mesh->SetMaterial(0, Mat.Object);
	Mesh->SetRelativeScale3D(FVector(0.4f, 0.4f, 2.5f));
	UpkeepStorage.SetNum(6);
}

void AWRClaimTotem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRClaimTotem, OwnerName);
	DOREPLIFETIME(AWRClaimTotem, Radius);
	DOREPLIFETIME(AWRClaimTotem, UpgradeLevel);
	DOREPLIFETIME(AWRClaimTotem, bUnpaid);
	DOREPLIFETIME(AWRClaimTotem, UpkeepStorage);
}

AWRClaimTotem* AWRClaimTotem::FindAt(const UWorld* World, const FVector& Location)
{
	for (TActorIterator<AWRClaimTotem> It(World); It; ++It)
	{
		if (FVector::Dist2D(It->GetActorLocation(), Location) <= It->Radius)
		{
			return *It;
		}
	}
	return nullptr;
}

bool AWRClaimTotem::CanBuild(const UWorld* World, const FVector& Location, const FString& PlayerName, FString& OutWhy)
{
	if (FWRSocialRules::IsInSafeZone(World, Location))
	{
		OutWhy = TEXT("no building in the safe zone");
		return false;
	}
	const AWRClaimTotem* Claim = FindAt(World, Location);
	if (Claim && !CanPlayersShare(Claim->OwnerName, PlayerName))
	{
		OutWhy = FString::Printf(TEXT("%s's land claim"), *Claim->OwnerName);
		return false;
	}
	return true;
}

TMap<FName, float> AWRClaimTotem::CurrentUpkeepPerHour() const
{
	TMap<FName, int32> ByMaterial;
	for (TActorIterator<AWRBuildPiece> It(GetWorld()); It; ++It)
	{
		const FWRPieceDef* Def = It->GetDef();
		if (Def && !It->bGhost && It->OwnerName == OwnerName && FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) <= Radius
			&& (Def->Material == TEXT("wood") || Def->Material == TEXT("stone")))
		{
			ByMaterial.FindOrAdd(Def->Material)++;
		}
	}
	TMap<FName, float> PerHour = FWRBuildRules::UpkeepPerHour(ByMaterial);
	float Mult = WRTune(TEXT("server.upkeepMultiplier"), 1.f);
	// Upkeep saver perk (read from the owner while they're on).
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		if (It->GetPlayerState() && It->GetPlayerState()->GetPlayerName() == OwnerName)
		{
			Mult *= FMath::Max(0.1f, 1.f + UWRLifeSkillsComponent::StatFor(*It, TEXT("upkeepCost")));
			break;
		}
	}
	for (TPair<FName, float>& P : PerHour) P.Value *= Mult;
	return PerHour;
}

void AWRClaimTotem::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority())
	{
		return;
	}
	MinuteClock += DeltaSeconds;
	if (MinuteClock < 60.f)
	{
		return;
	}
	MinuteClock = 0.f;

	// Pay one minute of upkeep from storage, or mark the claim unpaid (its pieces decay).
	bUnpaid = !FWRBuildRules::PayUpkeepMinute(UpkeepStorage, UpkeepOwed, CurrentUpkeepPerHour());
	if (bUnpaid)
	{
		for (TActorIterator<AWRBuildPiece> It(GetWorld()); It; ++It)
		{
			if (It->OwnerName == OwnerName && FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) <= Radius && It->GetChild() != this)
			{
				It->TakeBuildDamage(It->MaxHealth * FWRBuildRegistry::Get().DecayPerHourWhenUnpaid / 60.f, this);
			}
		}
	}
}

FString AWRClaimTotem::Deposit(AWRPlayerController* Player)
{
	AWRPlayerState* PS = Player ? Player->GetPlayerState<AWRPlayerState>() : nullptr;
	if (!PS)
	{
		return FString();
	}
	int32 Moved = 0;
	for (const FName Mat : { FName(TEXT("wood")), FName(TEXT("stone")) })
	{
		const int32 Have = PS->GetInventory()->CountOf(Mat);
		const int32 Left = FWRInventoryRules::Add(UpkeepStorage, Mat, Have);
		PS->GetInventory()->RemoveItem(Mat, Have - Left);
		Moved += Have - Left;
	}
	return FString::Printf(TEXT("Stocked %d materials. %s"), Moved, *Describe());
}

FString AWRClaimTotem::Upgrade(AWRPlayerController* Player)
{
	AWRPlayerState* PS = Player ? Player->GetPlayerState<AWRPlayerState>() : nullptr;
	if (!PS || !CanPlayersShare(OwnerName, PS->GetPlayerName()))
	{
		return TEXT("not your claim");
	}
	const FWRBuildRegistry& Reg = FWRBuildRegistry::Get();
	if (Radius >= Reg.ClaimMaxRadius)
	{
		return TEXT("claim already at its largest");
	}
	for (const TPair<FName, int32>& C : Reg.ClaimUpgradeCost)
	{
		if (PS->GetInventory()->CountOf(C.Key) < C.Value) return FString::Printf(TEXT("need %d %s"), C.Value, *C.Key.ToString());
	}
	for (const TPair<FName, int32>& C : Reg.ClaimUpgradeCost) PS->GetInventory()->RemoveItem(C.Key, C.Value);
	++UpgradeLevel;
	Radius = FMath::Min(Reg.ClaimMaxRadius, Reg.ClaimRadius + UpgradeLevel * Reg.ClaimRadiusPerUpgrade);
	return FString::Printf(TEXT("Claim radius now %.0f m"), Radius / 100.f);
}

FString AWRClaimTotem::Describe() const
{
	FString Upkeep;
	for (const TPair<FName, float>& C : CurrentUpkeepPerHour())
	{
		const int32 Stock = FWRInventoryRules::CountOf(UpkeepStorage, C.Key);
		Upkeep += FString::Printf(TEXT("%s %.0f/h (%d stocked, %.1f h)  "), *C.Key.ToString(), C.Value, Stock, C.Value > 0.f ? Stock / C.Value : 0.f);
	}
	return FString::Printf(TEXT("%s's claim, %.0f m%s. Upkeep: %s"), *OwnerName, Radius / 100.f, bUnpaid ? TEXT(" (UNPAID: decaying)") : TEXT(""),
		Upkeep.IsEmpty() ? TEXT("none") : *Upkeep);
}
