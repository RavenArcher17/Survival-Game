#include "WRCrafting.h"
#include "WRTaming.h"
#include "WRGear.h"
#include "WRFishing.h"
#include "WRFarm.h"
#include "WRSurvival.h"
#include "WRSound.h"
#include "WRBuildLook.h"
#include "WRBuilding.h"
#include "WRInventory.h"
#include "WRLifeSkills.h"
#include "Wildreach.h"
#include "WRCharacterBase.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRResourceNode.h"
#include "WRSocial.h"
#include "WRTuning.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

// (Named, not anonymous: unity builds put several files in one translation unit.)
namespace WRCraftDetail
{
	/** "8 s", "1 min 20 s", "12 min". */
	FString Duration(float Seconds)
	{
		const int32 S = FMath::Max(1, FMath::CeilToInt(Seconds));
		if (S < 60) return FString::Printf(TEXT("%d s"), S);
		return S % 60 == 0 || S >= 600 ? FString::Printf(TEXT("%d min"), FMath::RoundToInt(S / 60.f)) : FString::Printf(TEXT("%d min %d s"), S / 60, S % 60);
	}

	/** "forge", "alchemy table". */
	FString PlainName(FName Type)
	{
		return Type.ToString().Replace(TEXT("_"), TEXT(" "));
	}
}

// ---------------------------------------------------------------- recipes

FWRRecipeRegistry& FWRRecipeRegistry::Get()
{
	static FWRRecipeRegistry Instance;
	return Instance;
}

FWRRecipeRegistry::FWRRecipeRegistry()
{
	BaseOdds = { 0.25f, 0.55f, 0.17f, 0.03f };
	const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("recipes.json"));
	if (!Root.IsValid())
	{
		return;
	}
	const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
	if (Root->TryGetArrayField(TEXT("recipes"), List))
	{
		for (const TSharedPtr<FJsonValue>& V : *List)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			FWRRecipe R;
			R.Id = FName(*O->GetStringField(TEXT("id")));
			R.Station = FName(*O->GetStringField(TEXT("station")));
			R.Level = static_cast<int32>(O->GetNumberField(TEXT("level")));
			R.Output = FName(*O->GetStringField(TEXT("output")));
			R.Count = FMath::Max(1, static_cast<int32>(O->GetNumberField(TEXT("count"))));
			O->TryGetBoolField(TEXT("quality"), R.bQuality);
			double Seconds = 0.0;
			if (O->TryGetNumberField(TEXT("seconds"), Seconds)) R.Seconds = static_cast<float>(Seconds);
			FString Needs;
			if (O->TryGetStringField(TEXT("needsPerk"), Needs)) R.NeedsStat = FName(*Needs);
			O->TryGetStringField(TEXT("needsText"), R.NeedsText);
			FString RecipeBiome;
			if (O->TryGetStringField(TEXT("biome"), RecipeBiome)) R.Biome = FName(*RecipeBiome);
			FString Str;
			if (O->TryGetStringField(TEXT("blueprint"), Str)) R.Blueprint = FName(*Str);
			if (O->TryGetStringField(TEXT("xp"), Str)) R.XPKey = FName(*Str);
			if (O->TryGetStringField(TEXT("skill"), Str)) R.Skill = FName(*Str);
			O->TryGetStringField(TEXT("waitsFor"), R.WaitsFor);
			const TSharedPtr<FJsonObject>* Inputs = nullptr;
			if (O->TryGetObjectField(TEXT("inputs"), Inputs))
			{
				for (const TPair<FString, TSharedPtr<FJsonValue>>& In : (*Inputs)->Values)
				{
					R.Inputs.Add({ FName(*In.Key), static_cast<int32>(In.Value->AsNumber()) });
				}
			}
			Recipes.Add(R);
		}
	}
	// The kitchen (cooks/build-farming.md 2.8): every dish of Data/cooking.json is a recipe its station can make (the
	// Cook screen orders it once the pattern matches), Grey mush included; and the fish recipes are written once in
	// cooking.json fishRecipes, here made for every common and uncommon fish of fish.json.
	if (const TSharedPtr<FJsonObject> Cook = FWRTuning::LoadDataFile(TEXT("cooking.json")))
	{
		const TArray<TSharedPtr<FJsonValue>>* Dishes = nullptr;
		if (Cook->TryGetArrayField(TEXT("dishes"), Dishes))
		{
			for (const TSharedPtr<FJsonValue>& V : *Dishes)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				if (!O.IsValid() || !O->HasField(TEXT("id"))) continue;
				FWRRecipe R;
				R.Output = FName(*O->GetStringField(TEXT("id")));
				R.Id = FName(*(TEXT("dish_") + R.Output.ToString()));
				FString S;
				R.Station = O->TryGetStringField(TEXT("station"), S) ? FName(*S) : FName(TEXT("cooking_pot"));
				double N = 0.0;
				if (O->TryGetNumberField(TEXT("count"), N)) R.Count = FMath::Max(1, int32(N));
				R.Seconds = O->TryGetNumberField(TEXT("seconds"), N) ? float(N) : 20.f;
				R.bDish = true;
				Recipes.Add(R);
			}
		}
		FWRRecipe Mush;
		Mush.Id = TEXT("dish_grey_mush");
		Mush.Station = TEXT("cooking_pot");
		Mush.Output = TEXT("grey_mush");
		Mush.bDish = true;
		Recipes.Add(Mush);
		const TArray<TSharedPtr<FJsonValue>>* FishRecipes = nullptr;
		if (Cook->TryGetArrayField(TEXT("fishRecipes"), FishRecipes))
		{
			for (const TSharedPtr<FJsonValue>& V : *FishRecipes)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				if (!O.IsValid()) continue;
				int32 FishCount = 1;
				double N = 0.0;
				if (O->TryGetNumberField(TEXT("fish"), N)) FishCount = FMath::Max(1, int32(N));
				for (const FWRFishDef& F : FWRFishRegistry::Get().All())
				{
					if (F.Rarity != TEXT("common") && F.Rarity != TEXT("uncommon")) continue;
					FWRRecipe R;
					R.Id = FName(*FString::Printf(TEXT("%s_%s"), *O->GetStringField(TEXT("prefix")), *F.Id.ToString()));
					R.Station = FName(*O->GetStringField(TEXT("station")));
					R.Level = O->TryGetNumberField(TEXT("level"), N) ? FMath::Max(1, int32(N)) : 1;
					R.Output = FName(*O->GetStringField(TEXT("output")));
					R.Count = O->TryGetNumberField(TEXT("count"), N) ? FMath::Max(1, int32(N)) : 1;
					R.Seconds = O->TryGetNumberField(TEXT("seconds"), N) ? float(N) : 15.f;
					R.Inputs.Add({ F.Item, FishCount });
					const TSharedPtr<FJsonObject>* Inputs = nullptr;
					if (O->TryGetObjectField(TEXT("inputs"), Inputs))
					{
						for (const TPair<FString, TSharedPtr<FJsonValue>>& In : (*Inputs)->Values) R.Inputs.Add({ FName(*In.Key), static_cast<int32>(In.Value->AsNumber()) });
					}
					Recipes.Add(R);
				}
			}
		}
	}
	const TSharedPtr<FJsonObject>* Q = nullptr;
	if (Root->TryGetObjectField(TEXT("quality"), Q))
	{
		const TArray<TSharedPtr<FJsonValue>>* Odds = nullptr;
		if ((*Q)->TryGetArrayField(TEXT("baseOdds"), Odds) && Odds->Num() == 4)
		{
			for (int32 i = 0; i < 4; ++i) BaseOdds[i] = (*Odds)[i]->AsNumber();
		}
		(*Q)->TryGetNumberField(TEXT("perHearthLevel"), PerHearthLevel);
		(*Q)->TryGetNumberField(TEXT("perSkillLevel"), PerSkillLevel);
		(*Q)->TryGetNumberField(TEXT("rollSpread"), RollSpread);
	}
	Root->TryGetNumberField(TEXT("repairCostFraction"), RepairCostFraction);
	UE_LOG(LogWildreach, Log, TEXT("Loaded %d recipes"), Recipes.Num());
}

const FWRRecipe* FWRRecipeRegistry::Find(FName Id) const
{
	return Recipes.FindByPredicate([Id](const FWRRecipe& R) { return R.Id == Id; });
}

const FWRRecipe* FWRRecipeRegistry::ForOutput(FName ItemId) const
{
	return Recipes.FindByPredicate([ItemId](const FWRRecipe& R) { return R.Output == ItemId; });
}

TArray<const FWRRecipe*> FWRRecipeRegistry::ForStation(FName Station) const
{
	TArray<const FWRRecipe*> Out;
	for (const FWRRecipe& R : Recipes)
	{
		// Discovery dishes aren't listed: they're found in the Cook screen's pot.
		// The lava forge makes everything a forge does, and its own steel (cooks/build-machinery.md 2.5).
		const bool bHere = R.Station == Station || (Station == TEXT("lava_forge") && R.Station == TEXT("forge"));
		if (bHere && !R.bDish && WRBiome::IsLive(R.Biome) && R.WaitsFor.IsEmpty()) Out.Add(&R);
	}
	return Out;
}

EWRQuality FWRCraftingRules::RollQuality(float Random01, int32 HearthLevel, int32 SkillLevel)
{
	const FWRRecipeRegistry& Reg = FWRRecipeRegistry::Get();
	// Shift probability from crude toward fine/masterwork.
	const float Shift = FMath::Clamp((HearthLevel - 1) * Reg.PerHearthLevel + SkillLevel * Reg.PerSkillLevel, 0.f, 0.5f);
	float Odds[4] = { Reg.BaseOdds[0], Reg.BaseOdds[1], Reg.BaseOdds[2], Reg.BaseOdds[3] };
	const float FromCrude = FMath::Min(Odds[0], Shift);
	Odds[0] -= FromCrude;
	Odds[2] += FromCrude * 0.7f;
	Odds[3] += FromCrude * 0.3f;
	float Acc = 0.f;
	for (int32 i = 0; i < 4; ++i)
	{
		Acc += Odds[i];
		if (Random01 < Acc) return static_cast<EWRQuality>(i);
	}
	return EWRQuality::Masterwork;
}

int32 FWRCraftingRules::TechLevelForCores(int32 DistinctCores)
{
	if (DistinctCores >= 6) return 5;
	if (DistinctCores >= 4) return 4;
	if (DistinctCores >= 2) return 3;
	if (DistinctCores >= 1) return 2;
	return 1;
}

static UStaticMeshComponent* MakeShape(AActor* Owner, const TCHAR* Name, const TCHAR* MeshPath)
{
	UStaticMeshComponent* Mesh = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
	ConstructorHelpers::FObjectFinder<UStaticMesh> Shape(MeshPath);
	ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (Shape.Succeeded()) Mesh->SetStaticMesh(Shape.Object);
	if (Mat.Succeeded()) Mesh->SetMaterial(0, Mat.Object);
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	return Mesh;
}

static void Tint(UStaticMeshComponent* Mesh, const FLinearColor& Color)
{
	if (UMaterialInstanceDynamic* M = Mesh->CreateAndSetMaterialInstanceDynamic(0))
	{
		M->SetVectorParameterValue(TEXT("Color"), Color);
	}
}

/** A fire's warm light (the Hearth, a forge): no shadows, and the scaled shape doesn't stretch it. */
static UPointLightComponent* MakeGlow(AActor* Owner, USceneComponent* Parent, float Height, float Intensity, float Radius)
{
	UPointLightComponent* Light = Owner->CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Light->SetupAttachment(Parent);
	Light->SetUsingAbsoluteScale(true);
	Light->SetRelativeLocation(FVector(0.f, 0.f, Height));
	Light->SetLightColor(FLinearColor(1.f, 0.5f, 0.2f));
	Light->SetIntensity(Intensity);
	Light->SetAttenuationRadius(Radius);
	Light->SetCastShadows(false);
	return Light;
}

// ---------------------------------------------------------------- Hearth

AWRHearth::AWRHearth()
{
	bReplicates = true;
	bAlwaysRelevant = true;   // "Your Hearth" on your map from anywhere (its build piece is too: AWRBuildPiece::InitPiece)
	Mesh = MakeShape(this, TEXT("Mesh"), TEXT("/Engine/BasicShapes/Cone.Cone"));
	RootComponent = Mesh;
	Mesh->SetRelativeScale3D(FVector(1.5f, 1.5f, 1.2f));
	Glow = MakeGlow(this, Mesh, 90.f, 9000.f, 1800.f);
}

void AWRHearth::BeginPlay()
{
	Super::BeginPlay();
	// The stone fire ring (pieces.json "mesh") over the plain cone; it collides as the ring (every machine).
	const FWRPieceDef* Def = FWRBuildLook::KindDef(TEXT("hearth"));
	if (Def) FWRBuildLook::FitCollision(this, Mesh, *Def, -FWRBuildLook::ChildLift);
	if (Def && FWRBuildLook::AttachProp(this, Mesh, *Def, -FWRBuildLook::ChildLift))
	{
		Mesh->SetVisibility(false);
	}
}

void AWRHearth::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRHearth, Cores);
	DOREPLIFETIME(AWRHearth, Health);
}

AWRHearth* AWRHearth::FindCovering(const UWorld* World, const FVector& Location)
{
	AWRHearth* Best = nullptr;
	float BestDist = TNumericLimits<float>::Max();
	for (TActorIterator<AWRHearth> It(World); It; ++It)
	{
		const float D = FVector::Dist2D(It->GetActorLocation(), Location);
		if (D <= It->CoverRadius && D < BestDist)
		{
			BestDist = D;
			Best = *It;
		}
	}
	return Best;
}

FString AWRHearth::DepositCores(AWRPlayerController* Player)
{
	AWRPlayerState* PS = Player ? Player->GetPlayerState<AWRPlayerState>() : nullptr;
	if (!PS || !HasAuthority())
	{
		return FString();
	}
	int32 Added = 0;
	for (const FName Core : { FName(TEXT("core_vulkarn")), FName(TEXT("core_skaldrith")) })
	{
		// Each level needs cores from *different* major bosses, so only the first of each counts.
		if (PS->GetInventory()->CountOf(Core) > 0 && !Cores.Contains(Core))
		{
			PS->GetInventory()->RemoveItem(Core, 1);
			Cores.Add(Core);
			++Added;
		}
	}
	UE_LOG(LogWildreach, Log, TEXT("HEARTH %s: %d cores added, tech level %d"), *GetName(), Added, GetTechLevel());
	if (Added > 0)
	{
		WRSound::Broadcast(Player->GetPawn(), TEXT("hearth.feed"));
	}
	return Added > 0 ? FString::Printf(TEXT("The Hearth burns brighter: tech level %d"), GetTechLevel())
		: FString::Printf(TEXT("Hearth tech level %d (feed it cores from different major bosses)"), GetTechLevel());
}

void AWRHearth::TakeRaidDamage(float Amount)
{
	if (HasAuthority())
	{
		Health = FMath::Max(0.f, Health - Amount);
	}
}

TArray<TPair<FName, int32>> AWRHearth::RepairCost(const AActor* For) const
{
	TArray<TPair<FName, int32>> Out;
	const AWRBuildPiece* Piece = Cast<AWRBuildPiece>(GetOwner());
	const FWRPieceDef* Def = Piece ? Piece->GetDef() : nullptr;
	const float Missing = MaxHealth > 0.f ? FMath::Clamp(1.f - Health / MaxHealth, 0.f, 1.f) : 0.f;
	if (!Def || Missing <= KINDA_SMALL_NUMBER) return Out;
	const float Share = WRTune(TEXT("hearth.repairShare"), 0.5f) * Missing * FMath::Max(0.1f, 1.f + UWRLifeSkillsComponent::StatFor(For, TEXT("repairCost")));
	for (const TPair<FName, int32>& C : Def->Cost) Out.Add({ C.Key, FMath::Max(1, FMath::CeilToInt(C.Value * Share)) });
	return Out;
}

FString AWRHearth::RepairFor(AWRPlayerController* Player)
{
	AWRPlayerState* PS = Player ? Player->GetPlayerState<AWRPlayerState>() : nullptr;
	AWRBuildPiece* Piece = Cast<AWRBuildPiece>(GetOwner());
	if (!PS || !Piece || !HasAuthority()) return TEXT("no Hearth here");
	if (!Piece->CanAccess(PS->GetPlayerName())) return TEXT("Only its owner and their clan can repair this Hearth");
	const TArray<TPair<FName, int32>> Cost = RepairCost(Player);
	if (Cost.Num() == 0) return TEXT("The Hearth is already at full health");
	UWRInventoryComponent* Inv = PS->GetInventory();
	for (const TPair<FName, int32>& C : Cost)
	{
		if (Inv->CountOf(C.Key) < C.Value)
		{
			const FWRItemDef* Item = FWRItemRegistry::Get().Find(C.Key);
			return FString::Printf(TEXT("Repairing it takes %d %s: you have %d"), C.Value, Item ? *Item->Name.ToLower() : *C.Key.ToString(), Inv->CountOf(C.Key));
		}
	}
	for (const TPair<FName, int32>& C : Cost) Inv->RemoveItem(C.Key, C.Value);
	const bool bWasBroken = IsBroken();
	Piece->Health = Piece->MaxHealth;
	Repair();
	UE_LOG(LogWildreach, Log, TEXT("HEARTH %s repaired by %s (was %s)"), *GetName(), *PS->GetPlayerName(), bWasBroken ? TEXT("broken") : TEXT("damaged"));
	WRSound::Broadcast(Player->GetPawn(), TEXT("hearth.feed"));
	return bWasBroken ? TEXT("The Hearth is mended and burning: its stations work again") : TEXT("The Hearth is back to full health");
}

// ---------------------------------------------------------------- stations

AWRStation::AWRStation()
{
	bReplicates = true;
	// Its queue runs on the server a few times a second, and only while there's one.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickInterval = 0.2f;
	Mesh = MakeShape(this, TEXT("Mesh"), TEXT("/Engine/BasicShapes/Cube.Cube"));
	RootComponent = Mesh;
	Mesh->SetRelativeScale3D(FVector(1.2f, 0.8f, 0.9f));
	Glow = MakeGlow(this, Mesh, 70.f, 7000.f, 1200.f);
	Glow->SetVisibility(false);
}

void AWRStation::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRStation, StationType);
	DOREPLIFETIME(AWRStation, Queue);
	DOREPLIFETIME(AWRStation, Output);
	DOREPLIFETIME(AWRStation, AppliedBoost);
}

float AWRStation::SpeedHere() const
{
	const AWRBuildPiece* Piece = Cast<AWRBuildPiece>(GetOwner());
	const UWRMachineSubsystem* Machines = UWRMachineSubsystem::Get(this);
	float Speed = Machines && Piece ? Machines->StationBoost(Piece) : 1.f;
	if (StationType == TEXT("lava_forge")) Speed *= WRTune(TEXT("craft.lavaForgeSpeed"), 2.f);
	return FMath::Max(0.1f, Speed);
}

void AWRStation::BeginPlay()
{
	Super::BeginPlay();
	OnRep_Type();
}

void AWRStation::OnRep_Type()
{
	FLinearColor C(0.5f, 0.35f, 0.2f);
	if (StationType == TEXT("forge")) C = FLinearColor(0.35f, 0.12f, 0.08f);
	else if (StationType == TEXT("armorer")) C = FLinearColor(0.4f, 0.4f, 0.45f);
	else if (StationType == TEXT("alchemy_table")) C = FLinearColor(0.3f, 0.15f, 0.4f);
	Tint(Mesh, C);
	Glow->SetVisibility(StationType == TEXT("forge"));
	// The station's modelled prop (pieces.json "mesh") over the tinted box; it collides as the prop (every machine).
	if (FitType != StationType)
	{
		FitType = StationType;
		if (const FWRPieceDef* FitDef = FWRBuildLook::StationDef(StationType)) FWRBuildLook::FitCollision(this, Mesh, *FitDef, -FWRBuildLook::ChildLift);
	}
	if (Visual && Visual->GetStaticMesh() && LookType == StationType)
	{
		return;
	}
	if (Visual)
	{
		Visual->DestroyComponent();
		Visual = nullptr;
	}
	LookType = StationType;
	const FWRPieceDef* Def = FWRBuildLook::StationDef(StationType);
	Visual = Def ? FWRBuildLook::AttachProp(this, Mesh, *Def, -FWRBuildLook::ChildLift) : nullptr;
	Mesh->SetVisibility(Visual == nullptr);
	// The kitchen's stations (campfire, cooking pot, oven, smokehouse, salting barrel, drying rack, compost bin) are
	// drawn and collide as their build piece's own blocks (WRBuildLook FarmBlocks): no box of their own. The ones with
	// a fire carry its light where the fire is (tuning farm.fireHeight.<type>, cm above the ground); WRFxAmbient puts
	// the flames on that light.
	if (Def && Def->Mesh.IsEmpty() && StationType != TEXT("workbench") && StationType != TEXT("forge") && StationType != TEXT("armorer") && StationType != TEXT("alchemy_table"))
	{
		Mesh->SetVisibility(false);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		const float FireZ = FWRTuning::Get().Num(FString::Printf(TEXT("farm.fireHeight.%s"), *StationType.ToString()), -1.f);
		Glow->SetVisibility(FireZ >= 0.f);
		if (FireZ >= 0.f) Glow->SetWorldLocation(GetActorLocation() + FVector(0.f, 0.f, FireZ - FWRBuildLook::ChildLift));
	}
}

int32 AWRStation::GetTier() const
{
	const AWRHearth* Hearth = AWRHearth::FindCovering(GetWorld(), GetActorLocation());
	return Hearth && !Hearth->IsBroken() ? Hearth->GetTechLevel() : 1;
}

FString AWRStation::GetDisplayName() const
{
	FString Name = StationType.ToString().Replace(TEXT("_"), TEXT(" "));
	Name[0] = FChar::ToUpper(Name[0]);
	// A drive shaft runs into it: say it's powered (and how fast it's going).
	return AppliedBoost > 1.01f ? FString::Printf(TEXT("%s (tier %d, powered x%.1f)"), *Name, GetTier(), AppliedBoost) : FString::Printf(TEXT("%s (tier %d)"), *Name, GetTier());
}

bool AWRStation::CanUse(AWRPlayerController* Player, FString& OutWhy) const
{
	const APawn* Pawn = Player ? Player->GetPawn() : nullptr;
	if (!Pawn || FVector::Dist(Pawn->GetActorLocation(), GetActorLocation()) > UseRange + 100.f)
	{
		OutWhy = TEXT("too far from the station");
		return false;
	}
	const AWRHearth* Hearth = AWRHearth::FindCovering(GetWorld(), GetActorLocation());
	if (Hearth && Hearth->IsBroken())
	{
		OutWhy = TEXT("the Hearth is broken; repair it to craft");
		return false;
	}
	return true;
}

FString AWRStation::Craft(AWRPlayerController* Player, FName RecipeId, FName Element, int32 Times)
{
	FString Why;
	if (!HasAuthority() || !CanUse(Player, Why))
	{
		return Why;
	}
	const FWRRecipe* R = FWRRecipeRegistry::Get().Find(RecipeId);
	if (!R || R->Station != StationType)
	{
		return TEXT("not made here");
	}
	if (R->bDish)
	{
		return TEXT("dishes are found by cooking: put ingredients in the pot");
	}
	// A drying rack dries in the open air only (cooks/build-farming.md 2.8): not under a roof.
	if (StationType == TEXT("drying_rack") && WRBiome::IsUnderBuiltRoof(this, GetActorLocation() + FVector(0.f, 0.f, 150.f), WRTune(TEXT("farm.roofHeight"), 1500.f), GetOwner()))
	{
		return TEXT("A drying rack needs the open air: it won't dry under a roof");
	}
	if (R->Level > GetTier())
	{
		return FString::Printf(TEXT("needs a tier %d station (feed the Hearth boss cores)"), R->Level);
	}
	// Chief engineer (engineering perk): bigger orders and one more queue slot for you.
	const bool bChief = UWRLifeSkillsComponent::StatFor(Player, TEXT("chiefEngineer")) > 0.f;
	Times = FMath::Clamp(Times, 1, FMath::RoundToInt(WRTune(bChief ? TEXT("craft.maxPerOrderChief") : TEXT("craft.maxPerOrder"), bChief ? 99.f : 50.f)));
	float Seconds = SecondsFor(*R, Player);
	if (Seconds <= 0.f)
	{
		// At once, one after another (craft.timeScale 0).
		FString Result;
		for (int32 i = 0; i < Times; ++i) Result = FWRCrafting::Make(Player, *R, StationType, GetTier(), Element);
		return Result;
	}
	const FString Short = FWRCrafting::Shortfall(Player, *R, Element, Times);
	if (!Short.IsEmpty())
	{
		return Short;
	}
	const AWRPlayerState* PS = Player->GetPlayerState<AWRPlayerState>();
	const FString Who = PS ? PS->GetPlayerName() : FString();
	const float Discount = FWRCrafting::MaterialDiscount(Player, *R);
	// Overclock (engineering perk): this order runs much faster, burning a piece of wood per item; without the wood
	// it runs at the usual speed. (Wood that's also an input stays for the recipe.)
	const float Overclock = UWRLifeSkillsComponent::StatFor(Player, TEXT("overclock"));
	int32 WoodForRecipe = 0;
	for (const TPair<FName, int32>& In : FWRCrafting::InputsFor(*R, Element, Times, Discount)) if (In.Key == TEXT("wood")) WoodForRecipe += In.Value;
	const bool bOverclock = Overclock > 0.f && PS && PS->GetInventory()->CountOf(TEXT("wood")) >= WoodForRecipe + Times;
	if (bOverclock)
	{
		Seconds /= 1.f + Overclock;
	}
	// The same again from the same maker goes on the end of their last order.
	FWRCraftJob* Last = Queue.Num() > 0 ? &Queue.Last() : nullptr;
	const bool bJoin = Last && Last->Recipe == R->Id && Last->Element == Element && Last->Owner == Who && Last->Left + Times <= 99
		&& FMath::IsNearlyEqual(Last->Discount, Discount) && Last->bOverclocked == bOverclock && FMath::IsNearlyEqual(Last->ItemSeconds, Seconds);
	if (!bJoin && Queue.Num() >= FMath::RoundToInt(WRTune(TEXT("craft.queueOrders"), 6.f)) + (bChief ? 1 : 0))
	{
		return FString::Printf(TEXT("The %s's queue is full: wait for an order to finish, or cancel one"), *WRCraftDetail::PlainName(StationType));
	}
	FWRCrafting::TakeInputs(Player, *R, Element, Times);
	if (bOverclock)
	{
		Player->GetPlayerState<AWRPlayerState>()->GetInventory()->RemoveItem(TEXT("wood"), Times);
	}
	if (bJoin)
	{
		Last->Left += Times;
	}
	else
	{
		FWRCraftJob& Job = Queue.AddDefaulted_GetRef();
		Job.Recipe = R->Id;
		Job.Element = Element;
		Job.Owner = Who;
		Job.Left = Times;
		Job.ItemSeconds = Seconds;
		Job.ItemLeft = Seconds;
		Job.Discount = Discount;
		Job.bOverclocked = bOverclock;
		Job.Hand = FWRCrafting::HandOf(Player, *R, StationType);
	}
	SetActorTickEnabled(true);
	AdvanceQueue();
	const FWRItemDef* Out = FWRItemRegistry::Get().Find(R->Output);
	const FString Name = Out ? Out->Name : R->Output.ToString();
	UE_LOG(LogWildreach, Log, TEXT("CRAFT %s queued %d x %s at %s (%.1f s each; %d orders)"), *Who, Times, *R->Id.ToString(), *StationType.ToString(), Seconds, Queue.Num());
	return FString::Printf(TEXT("Queued %d %s: %s%s"), Times, *Name, *WRCraftDetail::Duration(Seconds * Times),
		bOverclock ? *FString::Printf(TEXT(" (overclocked: %d wood burned)"), Times) : TEXT(""));
}

FString AWRStation::CookOrder(AWRPlayerController* Player, FName Dish, int32 Count, float Seconds, float Worth)
{
	FString Why;
	if (!HasAuthority() || !CanUse(Player, Why))
	{
		return TEXT("!") + Why;
	}
	const FWRRecipe* R = FWRRecipeRegistry::Get().Find(FName(*(TEXT("dish_") + Dish.ToString())));
	if (!R)
	{
		return TEXT("!that isn't a dish");
	}
	if (Queue.Num() >= FMath::RoundToInt(WRTune(TEXT("craft.queueOrders"), 6.f)))
	{
		return FString::Printf(TEXT("!The %s is full: wait for what's cooking"), *WRCraftDetail::PlainName(StationType));
	}
	const AWRPlayerState* PS = Player->GetPlayerState<AWRPlayerState>();
	FWRCraftJob& Job = Queue.AddDefaulted_GetRef();
	Job.Recipe = R->Id;
	Job.Owner = PS ? PS->GetPlayerName() : FString();
	Job.Left = 1;
	Job.ItemSeconds = FMath::Max(0.f, Seconds * WRTune(TEXT("craft.timeScale"), 1.f) / (FMath::Max(0.1f, WRTune(TEXT("server.craftMultiplier"), 1.f)) * FWRCrafting::SpeedFor(Player, *R)));
	Job.ItemLeft = Job.ItemSeconds;
	Job.Worth = Worth;
	Job.Hand = FWRCrafting::HandOf(Player, *R, StationType);
	SetActorTickEnabled(true);
	AdvanceQueue();
	const FWRItemDef* Out = FWRItemRegistry::Get().Find(Dish);
	return FString::Printf(TEXT("Cooking %s: %s"), Out ? *Out->Name : *Dish.ToString(), *WRCraftDetail::Duration(Job.ItemSeconds));
}

float AWRStation::SecondsFor(const FWRRecipe& Recipe, const AActor* Player)
{
	const float Speed = FMath::Max(0.1f, WRTune(TEXT("server.craftMultiplier"), 1.f)) * FWRCrafting::SpeedFor(Player, Recipe);
	return FMath::Max(0.f, Recipe.Seconds * WRTune(TEXT("craft.timeScale"), 1.f) / Speed);
}

bool AWRStation::CanManage(const FString& Player) const
{
	const AWRBuildPiece* Piece = Cast<AWRBuildPiece>(GetOwner());
	return !Piece || Piece->CanAccess(Player);
}

FString AWRStation::CancelJob(AWRPlayerController* Player, int32 Index)
{
	AWRPlayerState* PS = Player ? Player->GetPlayerState<AWRPlayerState>() : nullptr;
	FString Why;
	if (!HasAuthority() || !PS || !CanUse(Player, Why) && Why != TEXT("the Hearth is broken; repair it to craft"))
	{
		return Why;
	}
	if (!Queue.IsValidIndex(Index))
	{
		return TEXT("No such order");
	}
	const FString Who = PS->GetPlayerName();
	FWRCraftJob Job = Queue[Index];
	if (Job.Owner != Who && !CanManage(Who))
	{
		return FString::Printf(TEXT("That's %s's order"), *Job.Owner);
	}
	const FWRRecipe* R = FWRRecipeRegistry::Get().Find(Job.Recipe);
	Queue.RemoveAt(Index);
	int32 Spilled = 0;
	if (R)
	{
		// What wasn't made yet comes back (the one under way too); what doesn't fit goes in a bag at the station.
		TArray<FWRItemStack> Over;
		TArray<TPair<FName, int32>> Back = FWRCrafting::InputsFor(*R, Job.Element, Job.Left, Job.Discount);
		if (Job.bOverclocked) Back.Add({ FName(TEXT("wood")), Job.Left });   // the fuel for what wasn't made
		for (const TPair<FName, int32>& In : Back)
		{
			const int32 Left = PS->GetInventory()->AddItem(In.Key, In.Value);
			if (Left > 0) { FWRItemStack& S = Over.AddDefaulted_GetRef(); S.ItemId = In.Key; S.Count = Left; Spilled += Left; }
		}
		AWRLootBag::Spill(GetWorld(), GetActorLocation() + FVector(0.f, 0.f, 40.f), Who, Over);
	}
	if (Index == 0) AdvanceQueue();
	UE_LOG(LogWildreach, Log, TEXT("CRAFT %s cancelled %s's order of %d %s at %s"), *Who, *Job.Owner, Job.Left, *Job.Recipe.ToString(), *StationType.ToString());
	const FWRItemDef* Out = R ? FWRItemRegistry::Get().Find(R->Output) : nullptr;
	return FString::Printf(TEXT("Cancelled %d %s: its materials are back%s"), Job.Left, Out ? *Out->Name : *Job.Recipe.ToString(), Spilled > 0 ? TEXT(" (what didn't fit is in a bag here)") : TEXT(""));
}

FString AWRStation::TakeOutput(AWRPlayerController* Player)
{
	AWRPlayerState* PS = Player ? Player->GetPlayerState<AWRPlayerState>() : nullptr;
	const APawn* Pawn = Player ? Player->GetPawn() : nullptr;
	if (!HasAuthority() || !PS || !Pawn || FVector::Dist(Pawn->GetActorLocation(), GetActorLocation()) > UseRange + 100.f)
	{
		return TEXT("too far from the station");
	}
	if (!CanManage(PS->GetPlayerName()) && !Queue.ContainsByPredicate([&PS](const FWRCraftJob& J) { return J.Owner == PS->GetPlayerName(); })
		&& Cast<AWRBuildPiece>(GetOwner()))
	{
		return TEXT("Only the base's people can take from it");
	}
	int32 Taken = 0, Kept = 0;
	for (FWRItemStack& S : Output)
	{
		if (S.IsEmpty()) continue;
		const FWRItemDef* Def = FWRItemRegistry::Get().Find(S.ItemId);
		if (Def && Def->IsEquipment() && Def->StackSize <= 1)
		{
			if (PS->GetInventory()->AddInstance(S)) { Taken += S.Count; S = FWRItemStack(); }
			else Kept += S.Count;
		}
		else
		{
			const int32 Left = PS->GetInventory()->AddItem(S.ItemId, S.Count);
			Taken += S.Count - Left;
			Kept += Left;
			S.Count = Left;
			if (Left <= 0) S = FWRItemStack();
		}
	}
	Output.RemoveAll([](const FWRItemStack& S) { return S.IsEmpty(); });
	if (Taken > 0) AdvanceQueue();   // a full tray had it waiting
	UE_LOG(LogWildreach, Log, TEXT("CRAFT %s took %d from the %s's tray (%d left)"), *PS->GetPlayerName(), Taken, *StationType.ToString(), Kept);
	return Taken == 0 ? (Kept > 0 ? TEXT("Your bag is full") : TEXT("Nothing to take")) : Kept > 0 ? FString::Printf(TEXT("Took %d: your bag is full"), Taken) : FString::Printf(TEXT("Took %d"), Taken);
}

float AWRStation::JobProgress() const
{
	if (Queue.Num() == 0) return 0.f;
	const FWRCraftJob& J = Queue[0];
	const UWorld* World = GetWorld();
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	const double Now = GS ? GS->GetServerWorldTimeSeconds() : World ? World->GetTimeSeconds() : 0.0;
	const float Left = J.DoneAt > 0.0 ? static_cast<float>(J.DoneAt - Now) * AppliedBoost : J.ItemLeft;
	return FMath::Clamp(1.f - Left / FMath::Max(0.01f, J.ItemSeconds), 0.f, 1.f);
}

void AWRStation::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority())
	{
		SetActorTickEnabled(false);
		return;
	}
	AdvanceQueue();
	if (Queue.Num() == 0) SetActorTickEnabled(false);
}

void AWRStation::AdvanceQueue()
{
	if (!HasAuthority())
	{
		return;
	}
	UWorld* World = GetWorld();
	const double Now = World->GetTimeSeconds();
	for (int32 Guard = 0; Queue.Num() > 0 && Guard < 100; ++Guard)
	{
		FWRCraftJob& Job = Queue[0];
		const FWRRecipe* R = FWRRecipeRegistry::Get().Find(Job.Recipe);
		if (!R || Job.Left <= 0)
		{
			Queue.RemoveAt(0);
			continue;
		}
		// A broken Hearth stops its stations: the one under way holds where it got to.
		const AWRHearth* Hearth = AWRHearth::FindCovering(World, GetActorLocation());
		const uint8 Wait = Hearth && Hearth->IsBroken() ? 2 : 0;
		if (Wait != 0)
		{
			if (Job.DoneAt > 0.0) { Job.ItemLeft = FMath::Max(0.f, static_cast<float>(Job.DoneAt - Now) * AppliedBoost); Job.DoneAt = 0.0; }
			Job.Waiting = Wait;
			return;
		}
		// Power through a drive shaft (or the lava forge's heat) speeds it: the one under way is retimed when that changes.
		const float Boost = SpeedHere();
		if (Job.DoneAt > 0.0 && !FMath::IsNearlyEqual(Boost, AppliedBoost, 0.01f))
		{
			const float Work = FMath::Max(0.f, static_cast<float>(Job.DoneAt - Now)) * AppliedBoost;
			Job.DoneAt = Now + Work / Boost;
			AppliedBoost = Boost;
		}
		if (Job.DoneAt <= 0.0)
		{
			AppliedBoost = Boost;
			Job.DoneAt = Now + FMath::Max(0.f, Job.ItemLeft) / Boost;
			Job.Waiting = 0;
		}
		if (Now < Job.DoneAt)
		{
			return;
		}
		// Due. Into its maker's bag when they're near, else the tray; neither has room: it waits for some.
		AWRPlayerController* Maker = nullptr;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			AWRPlayerController* PC = Cast<AWRPlayerController>(It->Get());
			if (PC && PC->PlayerState && PC->PlayerState->GetPlayerName() == Job.Owner) { Maker = PC; break; }
		}
		const APawn* MakerPawn = Maker ? Maker->GetPawn() : nullptr;
		AWRPlayerState* MakerPS = Maker ? Maker->GetPlayerState<AWRPlayerState>() : nullptr;
		const bool bNear = MakerPawn && MakerPS && FVector::Dist(MakerPawn->GetActorLocation(), GetActorLocation()) <= WRTune(TEXT("craft.deliverRange"), 2000.f);
		FWRCraftedOne One = FWRCrafting::MakeOne(*R, StationType, GetTier(), Job.Element, Job.Hand);
		One.Item.Roll = One.bGear ? One.Item.Roll : Job.Worth;
		const auto Fits = [&One](TArray<FWRItemStack> Slots) { return One.bGear ? FWRInventoryRules::AddInstance(Slots, One.Item) : FWRInventoryRules::Add(Slots, One.Item.ItemId, One.Item.Count) == 0; };
		bool bToBag = bNear && Fits(MakerPS->GetInventory()->GetSlots());
		TArray<FWRItemStack> Tray = Output;
		Tray.SetNum(FMath::Max(Tray.Num(), FMath::RoundToInt(WRTune(TEXT("craft.traySlots"), 8.f))));
		if (!bToBag && !Fits(Tray))
		{
			Job.Waiting = 1;
			Job.ItemLeft = 0.f;
			return;
		}
		if (bToBag)
		{
			if (One.bGear) MakerPS->GetInventory()->AddInstance(One.Item);
			else MakerPS->GetInventory()->AddFood(One.Item.ItemId, One.Item.Count, 1.f, Job.Worth);
		}
		else
		{
			if (One.bGear) FWRInventoryRules::AddInstance(Tray, One.Item);
			else FWRInventoryRules::AddFood(Tray, One.Item.ItemId, One.Item.Count, 1.f, Job.Worth);
			Tray.RemoveAll([](const FWRItemStack& S) { return S.IsEmpty(); });
			Output = Tray;
		}
		if (Maker)
		{
			UWRLifeSkillsComponent::Award(Maker, One.Skill, One.XPKey, One.XP);
			// Engineering levels by running machines: every item a station finishes for you.
			UWRLifeSkillsComponent::Award(Maker, TEXT("engineering"), TEXT("machineItem"), 3.f);
			if (bToBag) Maker->ClientNotify(One.Text);
		}
		WRSound::Broadcast(this, TEXT("craft.done"));
		UE_LOG(LogWildreach, Log, TEXT("CRAFT %s's %s at %s: %s, into %s (%d to go)"), *Job.Owner, *Job.Recipe.ToString(), *StationType.ToString(), *One.Text,
			bToBag ? TEXT("their bag") : TEXT("the tray"), Job.Left - 1);
		Job.Made += 1;
		Job.Left -= 1;
		Job.Waiting = 0;
		Job.ItemLeft = Job.ItemSeconds;
		Job.DoneAt = 0.0;
		if (Job.Left <= 0)
		{
			const FWRItemDef* Out = FWRItemRegistry::Get().Find(R->Output);
			if (Maker && !bToBag) Maker->ClientNotify(FString::Printf(TEXT("Your %d %s are done: take them from the %s"), Job.Made, Out ? *Out->Name : *R->Output.ToString(), *WRCraftDetail::PlainName(StationType)));
			Queue.RemoveAt(0);
		}
		// (Loops on: a station that was left waiting catches up at once.)
	}
}

void AWRStation::EndPlay(const EEndPlayReason::Type Reason)
{
	// Smashed or demolished: the tray and what the orders still had in them are left in a bag where it stood.
	const UWorld* World = GetWorld();
	if (Reason == EEndPlayReason::Destroyed && HasAuthority() && World && !World->bIsTearingDown)
	{
		TArray<FWRItemStack> Left = Output;
		for (const FWRCraftJob& Job : Queue)
		{
			if (const FWRRecipe* R = FWRRecipeRegistry::Get().Find(Job.Recipe))
			{
				for (const TPair<FName, int32>& In : FWRCrafting::InputsFor(*R, Job.Element, Job.Left, Job.Discount)) FWRInventoryRules::Add(Left, In.Key, In.Value);
				if (Job.bOverclocked) FWRInventoryRules::Add(Left, TEXT("wood"), Job.Left);
			}
		}
		const AWRBuildPiece* Piece = Cast<AWRBuildPiece>(GetOwner());
		if (AWRLootBag::Spill(GetWorld(), GetActorLocation() + FVector(0.f, 0.f, 40.f), Piece ? Piece->OwnerName : FString(), Left))
		{
			UE_LOG(LogWildreach, Log, TEXT("CRAFT the %s's tray and orders spilled (%d orders)"), *StationType.ToString(), Queue.Num());
		}
	}
	Super::EndPlay(Reason);
}

void AWRStation::QueueToJson(TSharedRef<FJsonObject> O) const
{
	TArray<TSharedPtr<FJsonValue>> Jobs;
	for (const FWRCraftJob& J : Queue)
	{
		const TSharedRef<FJsonObject> Q = MakeShared<FJsonObject>();
		Q->SetStringField(TEXT("recipe"), J.Recipe.ToString());
		if (!J.Element.IsNone()) Q->SetStringField(TEXT("element"), J.Element.ToString());
		Q->SetStringField(TEXT("owner"), J.Owner);
		Q->SetNumberField(TEXT("left"), J.Left);
		Q->SetNumberField(TEXT("made"), J.Made);
		Q->SetNumberField(TEXT("each"), J.ItemSeconds);
		// The one under way: how long it still has (the server's clock starts again with the next game).
		Q->SetNumberField(TEXT("itemLeft"), J.DoneAt > 0.0 ? FMath::Max(0.0, J.DoneAt - GetWorld()->GetTimeSeconds()) * AppliedBoost : J.ItemLeft);
		Q->SetNumberField(TEXT("skill"), J.Hand.SkillLevel);
		Q->SetBoolField(TEXT("minFine"), J.Hand.bMinFine);
		Q->SetNumberField(TEXT("rollBonus"), J.Hand.RollBonus);
		Q->SetNumberField(TEXT("doubleBrew"), J.Hand.DoubleBrew);
		if (!FMath::IsNearlyEqual(J.Worth, 1.f)) Q->SetNumberField(TEXT("worth"), J.Worth);
		Jobs.Add(MakeShared<FJsonValueObject>(Q));
	}
	if (Jobs.Num()) O->SetArrayField(TEXT("queue"), Jobs);
	TArray<TSharedPtr<FJsonValue>> Tray;
	for (const FWRItemStack& S : Output)
	{
		if (S.IsEmpty()) continue;
		const TSharedRef<FJsonObject> I = MakeShared<FJsonObject>();
		I->SetStringField(TEXT("id"), S.ItemId.ToString());
		I->SetNumberField(TEXT("count"), S.Count);
		I->SetNumberField(TEXT("quality"), static_cast<int32>(S.Quality));
		I->SetNumberField(TEXT("roll"), S.Roll);
		I->SetNumberField(TEXT("durability"), S.Durability);
		if (!S.Element.IsNone()) I->SetStringField(TEXT("element"), S.Element.ToString());
		if (FWRSpoil::Perishes(S.ItemId)) I->SetNumberField(TEXT("fresh"), FWRSpoil::FreshOf(S, FWRSpoil::ServerNow));
		Tray.Add(MakeShared<FJsonValueObject>(I));
	}
	if (Tray.Num()) O->SetArrayField(TEXT("tray"), Tray);
}

void AWRStation::QueueFromJson(const TSharedPtr<FJsonObject>& O)
{
	Queue.Reset();
	Output.Reset();
	const TArray<TSharedPtr<FJsonValue>>* Jobs = nullptr;
	if (O->TryGetArrayField(TEXT("queue"), Jobs))
	{
		for (const TSharedPtr<FJsonValue>& V : *Jobs)
		{
			const TSharedPtr<FJsonObject> Q = V->AsObject();
			if (!Q.IsValid() || !FWRRecipeRegistry::Get().Find(FName(*Q->GetStringField(TEXT("recipe"))))) continue;
			FWRCraftJob& J = Queue.AddDefaulted_GetRef();
			J.Recipe = FName(*Q->GetStringField(TEXT("recipe")));
			FString Element;
			if (Q->TryGetStringField(TEXT("element"), Element)) J.Element = FName(*Element);
			J.Owner = Q->GetStringField(TEXT("owner"));
			J.Left = Q->GetIntegerField(TEXT("left"));
			J.Made = Q->GetIntegerField(TEXT("made"));
			J.ItemSeconds = Q->GetNumberField(TEXT("each"));
			J.ItemLeft = Q->GetNumberField(TEXT("itemLeft"));
			J.Hand.SkillLevel = Q->GetIntegerField(TEXT("skill"));
			J.Hand.bMinFine = Q->GetBoolField(TEXT("minFine"));
			J.Hand.RollBonus = Q->GetNumberField(TEXT("rollBonus"));
			J.Hand.DoubleBrew = Q->GetNumberField(TEXT("doubleBrew"));
			double Worth = 1.0;
			if (Q->TryGetNumberField(TEXT("worth"), Worth)) J.Worth = float(Worth);
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* Tray = nullptr;
	if (O->TryGetArrayField(TEXT("tray"), Tray))
	{
		for (const TSharedPtr<FJsonValue>& V : *Tray)
		{
			const TSharedPtr<FJsonObject> I = V->AsObject();
			if (!I.IsValid()) continue;
			FWRItemStack& S = Output.AddDefaulted_GetRef();
			S.ItemId = FName(*I->GetStringField(TEXT("id")));
			S.Count = I->GetIntegerField(TEXT("count"));
			S.Quality = static_cast<EWRQuality>(I->GetIntegerField(TEXT("quality")));
			S.Roll = I->GetNumberField(TEXT("roll"));
			double Fresh = 1.0;
			if (FWRSpoil::Perishes(S.ItemId))
			{
				I->TryGetNumberField(TEXT("fresh"), Fresh);
				FWRSpoil::StampNew(S, float(Fresh), FWRSpoil::ServerNow);
			}
			S.Durability = I->GetNumberField(TEXT("durability"));
			FString Element;
			if (I->TryGetStringField(TEXT("element"), Element)) S.Element = FName(*Element);
		}
	}
	if (Queue.Num() > 0 && HasAuthority()) SetActorTickEnabled(true);
}

FString FWRCrafting::CraftByHand(AWRPlayerController* Player, FName RecipeId)
{
	const FWRRecipe* R = FWRRecipeRegistry::Get().Find(RecipeId);
	if (!R || R->Station != TEXT("hand"))
	{
		return TEXT("that needs a station");
	}
	if (!Player || !Player->HasAuthority() || !Player->GetPawn())
	{
		return FString();
	}
	return Make(Player, *R, TEXT("hand"), 1, NAME_None);
}

namespace WRCraftDetail
{
	FName EssenceFor(FName Element)
	{
		return Element == TEXT("fire") ? FName(TEXT("ember_essence")) : Element == TEXT("frost") ? FName(TEXT("frost_essence")) : NAME_None;
	}
}

bool FWRCrafting::IsGear(const FWRRecipe& Recipe)
{
	// Spears and bolas stack: made like ammo. So are tools: no quality roll, just a new one at full durability.
	const FWRItemDef* OutDef = FWRItemRegistry::Get().Find(Recipe.Output);
	// Saddles and barding are made like armour: a quality roll, Armorcraft XP (cooks/build-taming.md 2.3).
	return OutDef && ((OutDef->IsEquipment() && OutDef->StackSize <= 1 && OutDef->Category != TEXT("tool")) || FWRTaming::Tack(OutDef->Id));
}

bool FWRCrafting::IsArmourMade(const FWRItemDef& Def)
{
	return WRGearSlots::IsArmour(Def.Slot) || Def.Slot == EWREquipSlot::Talisman || FWRTaming::Tack(Def.Id);
}

FName FWRCrafting::GearStation(const FWRItemDef& Def)
{
	if (Def.Category == TEXT("tool")) return TEXT("workbench");
	if (IsArmourMade(Def)) return TEXT("armorer");
	if (Def.Slot == EWREquipSlot::Weapon || WRGearSlots::IsRing(Def.Slot)) return TEXT("forge");
	const FWRRecipe* R = FWRRecipeRegistry::Get().ForOutput(Def.Id);
	return R ? R->Station : FName(TEXT("workbench"));
}

bool FWRCrafting::IsOpenTo(const AActor* Player, const FWRRecipe& Recipe)
{
	return WRBiome::IsLive(Recipe.Biome) && Recipe.WaitsFor.IsEmpty() && (Recipe.Blueprint.IsNone() || FWRBlueprints::Knows(Player, Recipe.Blueprint));
}

FString FWRCrafting::Shortfall(AWRPlayerController* Player, const FWRRecipe& Recipe, FName Element, int32 Times)
{
	const AWRPlayerState* PS = Player ? Player->GetPlayerState<AWRPlayerState>() : nullptr;
	if (!PS)
	{
		return TEXT("no one to craft");
	}
	if (!Recipe.NeedsStat.IsNone() && UWRLifeSkillsComponent::StatFor(Player, Recipe.NeedsStat) <= 0.f)
	{
		return Recipe.NeedsText.IsEmpty() ? FString(TEXT("needs a perk you don't have")) : Recipe.NeedsText;
	}
	if (!IsOpenTo(Player, Recipe))
	{
		return !Recipe.Blueprint.IsNone() && !FWRBlueprints::Knows(Player, Recipe.Blueprint) ? TEXT("you haven't learned that blueprint") : TEXT("not in this world yet");
	}
	const UWRInventoryComponent* Inv = PS->GetInventory();
	const float Discount = MaterialDiscount(Player, Recipe);
	for (const TPair<FName, int32>& In : Recipe.Inputs)
	{
		const int32 Need = DiscountedInput(In.Value, Discount) * Times;
		if (Inv->CountOf(In.Key) < Need)
		{
			const FWRItemDef* Def = FWRItemRegistry::Get().Find(In.Key);
			return FString::Printf(TEXT("need %d %s"), Need, Def ? *Def->Name : *In.Key.ToString());
		}
	}
	const FName Essence = WRCraftDetail::EssenceFor(Element);
	if (!Essence.IsNone() && (!Recipe.bQuality || Inv->CountOf(Essence) < Times))
	{
		return Recipe.bQuality ? FString::Printf(TEXT("need %d %s to infuse"), Times, *Essence.ToString()) : TEXT("only gear can be infused");
	}
	// Ranged hits are physical (WRRanged): an infused bow would only use up the essence.
	const FWRItemDef* InfuseDef = FWRItemRegistry::Get().Find(Recipe.Output);
	if (!Essence.IsNone() && InfuseDef && InfuseDef->IsRanged())
	{
		return TEXT("ranged weapons can't carry an infusion yet");
	}
	// Rings and talismans aren't infused; a boss weapon carries its own element already.
	if (!Essence.IsNone() && InfuseDef && (!InfuseDef->InnateElement.IsNone() || InfuseDef->Slot == EWREquipSlot::Talisman || WRGearSlots::IsRing(InfuseDef->Slot)))
	{
		return InfuseDef->InnateElement.IsNone() ? TEXT("only weapons and armour take an infusion") : TEXT("it carries its own element");
	}
	return FString();
}

int32 FWRCrafting::DiscountedInput(int32 Amount, float Discount)
{
	return Discount <= 0.f ? Amount : FMath::Max(1, FMath::FloorToInt(Amount * (1.f - Discount)));
}

float FWRCrafting::MaterialDiscount(const AActor* Player, const FWRRecipe& Recipe)
{
	const FWRItemDef* OutDef = FWRItemRegistry::Get().Find(Recipe.Output);
	if (!Player || !OutDef || !OutDef->IsEquipment() || OutDef->Category == TEXT("tool"))
	{
		return 0.f;
	}
	const FName Stat = IsArmourMade(*OutDef) ? FName(TEXT("armorMaterials")) : FName(TEXT("weaponMaterials"));
	return FMath::Clamp(-UWRLifeSkillsComponent::StatFor(Player, Stat), 0.f, 0.9f);
}

TArray<TPair<FName, int32>> FWRCrafting::CostFor(const FWRRecipe& Recipe, float Discount)
{
	TArray<TPair<FName, int32>> Out;
	for (const TPair<FName, int32>& In : Recipe.Inputs) Out.Add({ In.Key, DiscountedInput(In.Value, Discount) });
	return Out;
}

float FWRCrafting::SpeedFor(const AActor* Player, const FWRRecipe& Recipe)
{
	if (!Player || Recipe.Station == TEXT("hand"))
	{
		return 1.f;
	}
	float Speed = 1.f + UWRLifeSkillsComponent::StatFor(Player, TEXT("machineSpeed"));
	const FWRItemDef* OutDef = FWRItemRegistry::Get().Find(Recipe.Output);
	if (OutDef && OutDef->IsEquipment() && OutDef->Category != TEXT("tool"))
	{
		Speed += UWRLifeSkillsComponent::StatFor(Player, IsArmourMade(*OutDef) ? TEXT("armorSpeed") : TEXT("weaponSpeed"));
	}
	return FMath::Max(0.1f, Speed);
}

TArray<TPair<FName, int32>> FWRCrafting::InputsFor(const FWRRecipe& Recipe, FName Element, int32 Times, float Discount)
{
	TArray<TPair<FName, int32>> Out;
	for (const TPair<FName, int32>& In : Recipe.Inputs) Out.Add({ In.Key, DiscountedInput(In.Value, Discount) * Times });
	const FName Essence = WRCraftDetail::EssenceFor(Element);
	if (!Essence.IsNone() && IsGear(Recipe)) Out.Add({ Essence, Times });
	return Out;
}

void FWRCrafting::TakeInputs(AWRPlayerController* Player, const FWRRecipe& Recipe, FName Element, int32 Times)
{
	AWRPlayerState* PS = Player ? Player->GetPlayerState<AWRPlayerState>() : nullptr;
	if (!PS) return;
	for (const TPair<FName, int32>& In : InputsFor(Recipe, Element, Times, MaterialDiscount(Player, Recipe))) PS->GetInventory()->RemoveItem(In.Key, In.Value);
}

FWRCrafterHand FWRCrafting::HandOf(AWRPlayerController* Player, const FWRRecipe& Recipe, FName StationType)
{
	// Smithing (weapons) / Armorcraft (armor) improve gear's odds; the Master perks floor it at Fine.
	FWRCrafterHand Hand;
	const FWRItemDef* OutDef = FWRItemRegistry::Get().Find(Recipe.Output);
	const bool bArmor = OutDef && IsArmourMade(*OutDef);
	const UWRLifeSkillsComponent* Skills = UWRLifeSkillsComponent::FindFor(Player);
	Hand.SkillLevel = Skills ? Skills->GetLevel(bArmor ? FName(TEXT("armorcraft")) : FName(TEXT("smithing"))) : 0;
	Hand.bMinFine = UWRLifeSkillsComponent::StatFor(Player, bArmor ? TEXT("armorMinFine") : TEXT("weaponMinFine")) > 0.f;
	Hand.RollBonus = bArmor ? 0.f : UWRLifeSkillsComponent::StatFor(Player, TEXT("weaponRoll"));
	Hand.DoubleBrew = StationType == TEXT("alchemy_table") ? UWRLifeSkillsComponent::StatFor(Player, TEXT("doubleBrew")) : 0.f;
	Hand.bBulkCook = UWRLifeSkillsComponent::StatFor(Player, TEXT("bulkCook")) > 0.f;
	return Hand;
}

FWRCraftedOne FWRCrafting::MakeOne(const FWRRecipe& Recipe, FName StationType, int32 Tier, FName Element, const FWRCrafterHand& Hand)
{
	FWRCraftedOne One;
	const FWRItemDef* OutDef = FWRItemRegistry::Get().Find(Recipe.Output);
	One.bGear = IsGear(Recipe);
	if (One.bGear)
	{
		const bool bArmor = IsArmourMade(*OutDef);
		EWRQuality Quality = Recipe.bQuality ? FWRCraftingRules::RollQuality(FMath::FRand(), Tier, Hand.SkillLevel) : EWRQuality::Common;
		if (Hand.bMinFine && Quality < EWRQuality::Fine)
		{
			Quality = EWRQuality::Fine;
		}
		const float Spread = FWRRecipeRegistry::Get().RollSpread;
		One.Item = FWRInventoryRules::MakeInstance(Recipe.Output, Quality, 1.f + FMath::FRandRange(-Spread + Hand.RollBonus, Spread));
		// A boss weapon's element is its own (Cinderbrand burns, Rimefang freezes).
		One.Item.Element = Element.IsNone() ? OutDef->InnateElement : Element;
		One.Skill = bArmor ? FName(TEXT("armorcraft")) : FName(TEXT("smithing"));
		One.XPKey = TEXT("craftGear");
		One.XP = 25.f;
		One.Text = FString::Printf(TEXT("Crafted %s %s%s (roll %.0f%%)"), FWRItemStack::QualityName(Quality), *OutDef->Name,
			Element.IsNone() ? TEXT("") : *FString::Printf(TEXT(", %s-infused"), *Element.ToString()), One.Item.Roll * 100.f);
		return One;
	}
	// Alchemy: a double batch now and then. Skill XP by what was made.
	int32 Count = Recipe.Count;
	if (StationType == TEXT("alchemy_table") && FMath::FRand() < Hand.DoubleBrew)
	{
		Count *= 2;
	}
	const bool bFood = OutDef && OutDef->Hunger > 0.f;
	if (bFood && Hand.bBulkCook)
	{
		Count *= 2;   // Bulk cook
	}
	One.Skill = StationType == TEXT("alchemy_table") ? FName(TEXT("alchemy")) : bFood ? FName(TEXT("cooking"))
		: StationType == TEXT("armorer") ? FName(TEXT("armorcraft")) : FName(TEXT("smithing"));
	One.XPKey = bFood ? TEXT("cook") : TEXT("craft");
	One.XP = bFood ? 15.f : 10.f;
	if (Recipe.XPKey == TEXT("steel"))
	{
		One.XPKey = TEXT("steel");   // steel and starsteel ingots (cooks/survival-crafting.md 2.23)
		One.XP = 5.f;
	}
	if (!Recipe.Skill.IsNone())
	{
		// Its own skill: gears are machine parts, Engineering 15 (cooks/build-machinery.md 2.9).
		One.Skill = Recipe.Skill;
		if (Recipe.XPKey == TEXT("machinePart")) { One.XPKey = TEXT("machinePart"); One.XP = 15.f; }
	}
	One.Item.ItemId = Recipe.Output;
	One.Item.Count = Count;
	One.Text = FString::Printf(TEXT("Crafted %d %s"), Count, OutDef ? *OutDef->Name : *Recipe.Output.ToString());
	return One;
}

FString FWRCrafting::Make(AWRPlayerController* Player, const FWRRecipe& Recipe, FName StationType, int32 Tier, FName Element)
{
	AWRPlayerState* PS = Player ? Player->GetPlayerState<AWRPlayerState>() : nullptr;
	if (!PS)
	{
		return FString();
	}
	const FString Short = Shortfall(Player, Recipe, Element, 1);
	if (!Short.IsEmpty())
	{
		return Short;
	}
	TakeInputs(Player, Recipe, Element, 1);
	UWRInventoryComponent* Inv = PS->GetInventory();
	const FWRCraftedOne One = MakeOne(Recipe, StationType, Tier, Element, HandOf(Player, Recipe, StationType));
	UWRLifeSkillsComponent::Award(Player, One.Skill, One.XPKey, One.XP);
	FString Result = One.Text;
	if (One.bGear)
	{
		if (!Inv->AddInstance(One.Item))
		{
			return TEXT("inventory full");
		}
	}
	else
	{
		const int32 Left = Inv->AddItem(One.Item.ItemId, One.Item.Count);
		const FWRItemDef* OutDef = FWRItemRegistry::Get().Find(Recipe.Output);
		Result = FString::Printf(TEXT("Crafted %d %s%s"), One.Item.Count - Left, OutDef ? *OutDef->Name : *Recipe.Output.ToString(), Left > 0 ? TEXT(" (inventory full)") : TEXT(""));
	}
	UE_LOG(LogWildreach, Log, TEXT("CRAFT %s at %s: %s"), *GetNameSafe(Player), *StationType.ToString(), *Result);
	WRSound::Broadcast(Player->GetPawn(), TEXT("craft.done"));
	return Result;
}

FString AWRStation::Repair(AWRPlayerController* Player, EWREquipSlot Slot)
{
	FString Why;
	if (!HasAuthority() || !CanUse(Player, Why))
	{
		return Why;
	}
	UWRInventoryComponent* Inv = Player->GetPlayerState<AWRPlayerState>()->GetInventory();
	FWRItemStack* Item = Inv->GetEquippedMutable(Slot);
	const FWRItemDef* Def = Item && !Item->IsEmpty() ? FWRItemRegistry::Get().Find(Item->ItemId) : nullptr;
	if (!Def)
	{
		return TEXT("nothing equipped");
	}
	if (Def->MaxDurability <= 0.f)
	{
		return FString::Printf(TEXT("the %s doesn't wear"), *Def->Name);
	}
	// Repairs happen at the matching station: the tool in your hand at a workbench, weapons at a forge, armour at an
	// armorer (FWRCrafting::GearStation).
	const FName At = FWRCrafting::GearStation(*Def);
	if (Slot != EWREquipSlot::Ranged && At != StationType)
	{
		return FString::Printf(TEXT("the %s is repaired at a %s"), *Def->Name, *At.ToString().Replace(TEXT("_"), TEXT(" ")));
	}
	if (Item->Durability >= Def->MaxDurability)
	{
		return TEXT("already in perfect condition");
	}
	const FWRRecipe* R = FWRRecipeRegistry::Get().ForOutput(Def->Id);
	if (Slot == EWREquipSlot::Ranged && R && R->Station != StationType)
	{
		return FString::Printf(TEXT("the %s is repaired where it's made (%s)"), *Def->Name, *R->Station.ToString().Replace(TEXT("_"), TEXT(" ")));
	}
	// Quick repair (building perk): repairs cost less.
	const float Fraction = FWRRecipeRegistry::Get().RepairCostFraction * FMath::Max(0.1f, 1.f + UWRLifeSkillsComponent::StatFor(Player, TEXT("repairCost")));
	if (R)
	{
		for (const TPair<FName, int32>& In : R->Inputs)
		{
			if (Inv->CountOf(In.Key) < FMath::CeilToInt(In.Value * Fraction))
			{
				return FString::Printf(TEXT("need %d %s to repair"), FMath::CeilToInt(In.Value * Fraction), *In.Key.ToString());
			}
		}
		for (const TPair<FName, int32>& In : R->Inputs)
		{
			Inv->RemoveItem(In.Key, FMath::CeilToInt(In.Value * Fraction));
		}
	}
	Item->Durability = Def->MaxDurability;
	Inv->ApplyEquipment();
	UE_LOG(LogWildreach, Log, TEXT("REPAIR %s: %s at %s"), *GetNameSafe(Player), *Def->Id.ToString(), *StationType.ToString());
	return FString::Printf(TEXT("Repaired %s"), *Def->Name);
}

FString AWRStation::Upgrade(AWRPlayerController* Player, EWREquipSlot Slot)
{
	FString Why;
	if (!HasAuthority() || !CanUse(Player, Why))
	{
		return Why;
	}
	if (Slot != EWREquipSlot::Weapon && !WRGearSlots::IsArmour(Slot))
	{
		return TEXT("weapons and armour are upgraded");
	}
	UWRInventoryComponent* Inv = Player->GetPlayerState<AWRPlayerState>()->GetInventory();
	FWRItemStack* Item = Inv->GetEquippedMutable(Slot);
	if (!Item || Item->IsEmpty())
	{
		return TEXT("nothing equipped");
	}
	if (const FWRItemDef* UpDef = FWRItemRegistry::Get().Find(Item->ItemId); UpDef && FWRCrafting::GearStation(*UpDef) != StationType && !FWRGathering::FindTool(Item->ItemId))
	{
		return TEXT("upgrade weapons at a forge and armour at an armorer");
	}
	if (Slot == EWREquipSlot::Weapon && FWRGathering::FindTool(Item->ItemId))
	{
		return TEXT("tools can't be upgraded (a better tool gathers more)");
	}
	const int32 Cap = FWRCraftingRules::UpgradeCap(GetTier());
	if (Item->Upgrade >= FMath::Min(10, Cap))
	{
		return FString::Printf(TEXT("+%d is the cap at Hearth level %d"), Cap, GetTier());
	}
	const int32 Cost = FWRCraftingRules::UpgradeCost(Item->Upgrade);
	if (Inv->CountOf(TEXT("upgrade_shard")) < Cost)
	{
		return FString::Printf(TEXT("need %d upgrade shards (mini-boss drops)"), Cost);
	}
	Inv->RemoveItem(TEXT("upgrade_shard"), Cost);
	++Item->Upgrade;
	Inv->ApplyEquipment();
	UWRLifeSkillsComponent::Award(Player, WRGearSlots::IsArmour(Slot) ? FName(TEXT("armorcraft")) : FName(TEXT("smithing")), TEXT("upgrade"), 15.f);
	return FString::Printf(TEXT("Upgraded to +%d"), Item->Upgrade);
}
