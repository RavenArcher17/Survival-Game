#include "WRBuilding.h"
#include "WRLifeSkills.h"
#include "WRSocial.h"
#include "Wildreach.h"
#include "WRCrafting.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRTuning.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
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
	static const FName EdgeShapes[] = { TEXT("wall"), TEXT("halfwall"), TEXT("door"), TEXT("window"), TEXT("palisade"), TEXT("gate") };
	for (const FName& Edge : EdgeShapes)
	{
		if (Shape == Edge)
		{
			// Nearest cell edge: walls run along it.
			const float Dx = Hit.X - Cx, Dy = Hit.Y - Cy;
			if (FMath::Abs(Dx) > FMath::Abs(Dy))
			{
				InOutYaw = 90.f;
				return FVector(Cx + FMath::Sign(Dx) * Grid * 0.5f, Cy, BaseZ);
			}
			InOutYaw = 0.f;
			return FVector(Cx, Cy + FMath::Sign(Dy == 0.f ? 1.f : Dy) * Grid * 0.5f, BaseZ);
		}
	}
	if (Shape == TEXT("pillar"))
	{
		InOutYaw = 0.f;
		return FVector(FMath::RoundToFloat((Hit.X - Grid * 0.5f) / Grid) * Grid + Grid * 0.5f,
			FMath::RoundToFloat((Hit.Y - Grid * 0.5f) / Grid) * Grid + Grid * 0.5f, BaseZ);
	}
	if (Shape != TEXT("stairs"))
	{
		InOutYaw = 0.f;
	}
	else
	{
		InOutYaw = FMath::RoundToFloat(InOutYaw / 90.f) * 90.f;
	}
	return FVector(Cx, Cy, BaseZ);
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

static UStaticMesh* CubeMesh()
{
	static UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	return Mesh;
}

static UMaterialInterface* ShapeMat()
{
	static UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	return Mat;
}

void AWRBuildPiece::ApplyLook()
{
	const FWRPieceDef* Def = GetDef();
	if (!Def || bLookApplied)
	{
		return;
	}
	bLookApplied = true;

	// Parts: (size cm, centre offset cm) of cube blocks; a door/gate also gets a hinged leaf.
	struct FPart { FVector Size; FVector Offset; FRotator Rot = FRotator::ZeroRotator; };
	TArray<FPart> Parts;
	FVector LeafSize = FVector::ZeroVector;
	const FName S = Def->Shape;
	const FName K = Def->Kind;
	// A deep slab: the top is the floor (100 above the piece), the rest sinks into the ground so slopes don't show a gap.
	if (S == TEXT("foundation")) Parts.Add({ FVector(400, 400, 250), FVector(0, 0, -25) });
	else if (S == TEXT("wall")) Parts.Add({ FVector(400, 20, 300), FVector(0, 0, 150) });
	else if (S == TEXT("halfwall")) Parts.Add({ FVector(400, 20, 150), FVector(0, 0, 75) });
	else if (S == TEXT("floor") || S == TEXT("roof")) Parts.Add({ FVector(400, 400, 20), FVector(0, 0, 10) });
	else if (S == TEXT("pillar")) Parts.Add({ FVector(40, 40, 300), FVector(0, 0, 150) });
	else if (S == TEXT("stairs")) Parts.Add({ FVector(500, 200, 20), FVector(0, 0, 150), FRotator(36.87f, 0, 0) });
	else if (S == TEXT("window"))
	{
		Parts.Add({ FVector(400, 20, 100), FVector(0, 0, 50) });
		Parts.Add({ FVector(400, 20, 80), FVector(0, 0, 260) });
		Parts.Add({ FVector(120, 20, 120), FVector(-140, 0, 160) });
		Parts.Add({ FVector(120, 20, 120), FVector(140, 0, 160) });
	}
	else if (S == TEXT("door") || S == TEXT("gate"))
	{
		const float Opening = S == TEXT("gate") ? 300.f : 140.f;
		const float Side = (400.f - Opening) * 0.5f;
		const float Height = S == TEXT("gate") ? 400.f : 300.f;
		Parts.Add({ FVector(Side, 20, Height), FVector(-200 + Side * 0.5f, 0, Height * 0.5f) });
		Parts.Add({ FVector(Side, 20, Height), FVector(200 - Side * 0.5f, 0, Height * 0.5f) });
		Parts.Add({ FVector(Opening, 20, Height - 240), FVector(0, 0, 240 + (Height - 240) * 0.5f) });
		LeafSize = FVector(Opening, 10, 240);
	}
	else if (S == TEXT("palisade")) Parts.Add({ FVector(400, 40, 400), FVector(0, 0, 200) });
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

	const auto Configure = [this, Color](UStaticMeshComponent* C, const FVector& Size, const FVector& Offset, const FRotator& Rot)
	{
		C->SetStaticMesh(CubeMesh());
		C->SetMaterial(0, ShapeMat());
		C->SetRelativeLocation(Offset);
		C->SetRelativeRotation(Rot);
		C->SetRelativeScale3D(Size / 100.f);
		C->SetCollisionProfileName(bGhost ? TEXT("NoCollision") : TEXT("BlockAll"));
		if (UMaterialInstanceDynamic* M = C->CreateAndSetMaterialInstanceDynamic(0))
		{
			M->SetVectorParameterValue(TEXT("Color"), Color);
		}
	};

	if (Parts.Num() > 0)
	{
		Configure(Main, Parts[0].Size, Parts[0].Offset, Parts[0].Rot);
		for (int32 i = 1; i < Parts.Num(); ++i)
		{
			UStaticMeshComponent* Extra = NewObject<UStaticMeshComponent>(this);
			Extra->SetupAttachment(RootComponent);
			Extra->RegisterComponent();
			Configure(Extra, Parts[i].Size, Parts[i].Offset, Parts[i].Rot);
		}
	}
	else
	{
		Main->SetVisibility(false);
		Main->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (!LeafSize.IsZero())
	{
		// Placeholder door: a leaf in the opening that turns edge-on (and stops blocking) when open.
		Configure(Leaf, LeafSize, FVector(0.f, 0.f, LeafSize.Z * 0.5f), FRotator::ZeroRotator);
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
	TArray<UStaticMeshComponent*> Comps;
	GetComponents(Comps);
	for (UStaticMeshComponent* C : Comps)
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
		const FVector D = It->GetActorLocation() - Location;
		if (D.Size2D() <= Grid * 1.05f && FMath::Abs(D.Z) <= 320.f && It->Support < TNumericLimits<int32>::Max())
		{
			Best = FMath::Min(Best, It->Support + 1);
		}
	}
	return Best <= FWRBuildRules::SpanFor(Def->Material) ? Best : INDEX_NONE;
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
				const FVector D = N->GetActorLocation() - P->GetActorLocation();
				if (N != P && N->Support < TNumericLimits<int32>::Max() && D.Size2D() <= Grid * 1.05f && FMath::Abs(D.Z) <= 320.f && N->Support + 1 < P->Support)
				{
					P->Support = N->Support + 1;
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
	bool bPaid = true;
	for (const TPair<FName, float>& Cost : CurrentUpkeepPerHour())
	{
		const int32 Need = FMath::CeilToInt(Cost.Value / 60.f);
		if (FWRInventoryRules::CountOf(UpkeepStorage, Cost.Key) >= Need) FWRInventoryRules::Remove(UpkeepStorage, Cost.Key, Need);
		else bPaid = false;
	}
	bUnpaid = !bPaid;
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
