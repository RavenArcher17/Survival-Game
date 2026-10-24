#include "WRCrafting.h"
#include "WRLifeSkills.h"
#include "Wildreach.h"
#include "WRCharacterBase.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRTuning.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

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
		if (R.Station == Station) Out.Add(&R);
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

// ---------------------------------------------------------------- Hearth

AWRHearth::AWRHearth()
{
	bReplicates = true;
	Mesh = MakeShape(this, TEXT("Mesh"), TEXT("/Engine/BasicShapes/Cone.Cone"));
	RootComponent = Mesh;
	Mesh->SetRelativeScale3D(FVector(1.5f, 1.5f, 1.2f));
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

// ---------------------------------------------------------------- stations

AWRStation::AWRStation()
{
	bReplicates = true;
	Mesh = MakeShape(this, TEXT("Mesh"), TEXT("/Engine/BasicShapes/Cube.Cube"));
	RootComponent = Mesh;
	Mesh->SetRelativeScale3D(FVector(1.2f, 0.8f, 0.9f));
}

void AWRStation::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRStation, StationType);
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
	return FString::Printf(TEXT("%s (tier %d)"), *Name, GetTier());
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

FString AWRStation::Craft(AWRPlayerController* Player, FName RecipeId, FName Element)
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
	if (R->Level > GetTier())
	{
		return FString::Printf(TEXT("needs a tier %d station (feed the Hearth boss cores)"), R->Level);
	}
	UWRInventoryComponent* Inv = Player->GetPlayerState<AWRPlayerState>()->GetInventory();
	for (const TPair<FName, int32>& In : R->Inputs)
	{
		if (Inv->CountOf(In.Key) < In.Value)
		{
			const FWRItemDef* Def = FWRItemRegistry::Get().Find(In.Key);
			return FString::Printf(TEXT("need %d %s"), In.Value, Def ? *Def->Name : *In.Key.ToString());
		}
	}
	const FName Essence = Element == TEXT("fire") ? FName(TEXT("ember_essence")) : Element == TEXT("frost") ? FName(TEXT("frost_essence")) : NAME_None;
	if (!Essence.IsNone() && (!R->bQuality || Inv->CountOf(Essence) < 1))
	{
		return R->bQuality ? FString::Printf(TEXT("need 1 %s to infuse"), *Essence.ToString()) : TEXT("only gear can be infused");
	}

	for (const TPair<FName, int32>& In : R->Inputs)
	{
		Inv->RemoveItem(In.Key, In.Value);
	}
	const FWRItemDef* OutDef = FWRItemRegistry::Get().Find(R->Output);
	FString Result;
	if (OutDef && OutDef->IsEquipment())
	{
		if (!Essence.IsNone())
		{
			Inv->RemoveItem(Essence, 1);
		}
		// Smithing (weapons) / Armorcraft (armor) improve the odds; the Master perks floor it at Fine.
		const bool bArmor = OutDef->Slot == EWREquipSlot::Armor;
		const UWRLifeSkillsComponent* Skills = UWRLifeSkillsComponent::FindFor(Player);
		const int32 SkillLevel = Skills ? Skills->GetLevel(bArmor ? FName(TEXT("armorcraft")) : FName(TEXT("smithing"))) : 0;
		EWRQuality Quality = R->bQuality ? FWRCraftingRules::RollQuality(FMath::FRand(), GetTier(), SkillLevel) : EWRQuality::Common;
		if (UWRLifeSkillsComponent::StatFor(Player, bArmor ? TEXT("armorMinFine") : TEXT("weaponMinFine")) > 0.f && Quality < EWRQuality::Fine)
		{
			Quality = EWRQuality::Fine;
		}
		const float Spread = FWRRecipeRegistry::Get().RollSpread;
		const float RollBonus = bArmor ? 0.f : UWRLifeSkillsComponent::StatFor(Player, TEXT("weaponRoll"));
		FWRItemStack Item = FWRInventoryRules::MakeInstance(R->Output, Quality, 1.f + FMath::FRandRange(-Spread + RollBonus, Spread));
		UWRLifeSkillsComponent::Award(Player, bArmor ? FName(TEXT("armorcraft")) : FName(TEXT("smithing")), TEXT("craftGear"), 25.f);
		Item.Element = Element;
		if (!Inv->AddInstance(Item))
		{
			return TEXT("inventory full");
		}
		Result = FString::Printf(TEXT("Crafted %s %s%s (roll %.0f%%)"), FWRItemStack::QualityName(Quality), *OutDef->Name,
			Element.IsNone() ? TEXT("") : *FString::Printf(TEXT(", %s-infused"), *Element.ToString()), Item.Roll * 100.f);
	}
	else
	{
		// Alchemy: double batch. Skill XP by what was made.
		int32 Count = R->Count;
		if (StationType == TEXT("alchemy_table") && FMath::FRand() < UWRLifeSkillsComponent::StatFor(Player, TEXT("doubleBrew")))
		{
			Count *= 2;
		}
		const bool bFood = OutDef && OutDef->Hunger > 0.f;
		const FName Skill = StationType == TEXT("alchemy_table") ? FName(TEXT("alchemy")) : bFood ? FName(TEXT("cooking"))
			: StationType == TEXT("armorer") ? FName(TEXT("armorcraft")) : FName(TEXT("smithing"));
		UWRLifeSkillsComponent::Award(Player, Skill, bFood ? TEXT("cook") : TEXT("craft"), bFood ? 15.f : 10.f);
		const int32 Left = Inv->AddItem(R->Output, Count);
		Result = FString::Printf(TEXT("Crafted %d %s%s"), Count - Left, OutDef ? *OutDef->Name : *R->Output.ToString(), Left > 0 ? TEXT(" (inventory full)") : TEXT(""));
	}
	UE_LOG(LogWildreach, Log, TEXT("CRAFT %s at %s: %s"), *GetNameSafe(Player), *StationType.ToString(), *Result);
	return Result;
}

FString AWRStation::Repair(AWRPlayerController* Player, EWREquipSlot Slot)
{
	FString Why;
	if (!HasAuthority() || !CanUse(Player, Why))
	{
		return Why;
	}
	// Repairs happen at the matching station.
	if ((Slot == EWREquipSlot::Weapon && StationType != TEXT("forge")) || (Slot == EWREquipSlot::Armor && StationType != TEXT("armorer")))
	{
		return Slot == EWREquipSlot::Weapon ? TEXT("weapons are repaired at a forge") : TEXT("armor is repaired at an armorer");
	}
	UWRInventoryComponent* Inv = Player->GetPlayerState<AWRPlayerState>()->GetInventory();
	FWRItemStack* Item = Inv->GetEquippedMutable(Slot);
	const FWRItemDef* Def = Item && !Item->IsEmpty() ? FWRItemRegistry::Get().Find(Item->ItemId) : nullptr;
	if (!Def)
	{
		return TEXT("nothing equipped");
	}
	if (Item->Durability >= Def->MaxDurability)
	{
		return TEXT("already in perfect condition");
	}
	const FWRRecipe* R = FWRRecipeRegistry::Get().ForOutput(Def->Id);
	const float Fraction = FWRRecipeRegistry::Get().RepairCostFraction;
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
	return FString::Printf(TEXT("Repaired %s"), *Def->Name);
}

FString AWRStation::Upgrade(AWRPlayerController* Player, EWREquipSlot Slot)
{
	FString Why;
	if (!HasAuthority() || !CanUse(Player, Why))
	{
		return Why;
	}
	if ((Slot == EWREquipSlot::Weapon && StationType != TEXT("forge")) || (Slot == EWREquipSlot::Armor && StationType != TEXT("armorer")))
	{
		return TEXT("upgrade weapons at a forge and armor at an armorer");
	}
	UWRInventoryComponent* Inv = Player->GetPlayerState<AWRPlayerState>()->GetInventory();
	FWRItemStack* Item = Inv->GetEquippedMutable(Slot);
	if (!Item || Item->IsEmpty())
	{
		return TEXT("nothing equipped");
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
	UWRLifeSkillsComponent::Award(Player, Slot == EWREquipSlot::Armor ? FName(TEXT("armorcraft")) : FName(TEXT("smithing")), TEXT("upgrade"), 15.f);
	return FString::Printf(TEXT("Upgraded to +%d"), Item->Upgrade);
}
