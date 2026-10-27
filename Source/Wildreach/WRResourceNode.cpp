#include "WRResourceNode.h"
#include "WRLifeSkills.h"
#include "WRTuning.h"
#include "WRFx.h"
#include "Wildreach.h"
#include "WRCharacterBase.h"
#include "WRInventory.h"
#include "WRPlayerController.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

// ---------------------------------------------------------------- gathering

const TArray<FWRToolDef>& FWRGathering::Tools()
{
	static TArray<FWRToolDef> List = []()
	{
		TArray<FWRToolDef> Out;
		const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("items.json"));
		const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
		if (Root.IsValid() && Root->TryGetArrayField(TEXT("items"), Items))
		{
			for (const TSharedPtr<FJsonValue>& V : *Items)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				const TSharedPtr<FJsonObject>* Tool = nullptr;
				if (O.IsValid() && O->TryGetObjectField(TEXT("tool"), Tool))
				{
					FWRToolDef& T = Out.AddDefaulted_GetRef();
					T.ItemId = FName(*O->GetStringField(TEXT("id")));
					T.Kind = FName(*(*Tool)->GetStringField(TEXT("kind")));
					double Yield = 1.0;
					(*Tool)->TryGetNumberField(TEXT("yield"), Yield);
					T.Yield = Yield;
				}
			}
		}
		return Out;
	}();
	return List;
}

FName FWRGathering::ToolKindFor(FName ItemId)
{
	const FString Id = ItemId.ToString();
	if (ItemId == TEXT("wood")) return TEXT("hatchet");
	if (ItemId == TEXT("stone") || Id.EndsWith(TEXT("_ore")) || Id.StartsWith(TEXT("sulfur"))) return TEXT("pick");
	return NAME_None;
}

const FWRToolDef* FWRGathering::BestTool(FName Kind, const TArray<FName>& Carried)
{
	const FWRToolDef* Best = nullptr;
	for (const FWRToolDef& T : Tools())
	{
		if (!Kind.IsNone() && T.Kind == Kind && Carried.Contains(T.ItemId) && (!Best || T.Yield > Best->Yield))
		{
			Best = &T;
		}
	}
	return Best;
}

int32 FWRGathering::Gather(AWRCharacterBase* Gatherer, FName ItemId, float PerHit, bool bHeavy, const FString& Source)
{
	UWRInventoryComponent* Inventory = Gatherer && Gatherer->HasAuthority() ? UWRInventoryComponent::FindFor(Gatherer) : nullptr;
	if (!Inventory)
	{
		return 0;
	}
	TArray<FName> Carried;
	for (const FWRItemStack& S : Inventory->GetSlots())
	{
		if (!S.IsEmpty()) Carried.AddUnique(S.ItemId);
	}
	const FWRToolDef* Tool = BestTool(ToolKindFor(ItemId), Carried);

	// Tool, gathering multiplier (server) and the gathering life skills.
	const bool bOre = ToolKindFor(ItemId) == TEXT("pick");
	const bool bWood = ItemId == TEXT("wood");
	const FName Skill = bOre ? FName(TEXT("mining")) : bWood ? FName(TEXT("woodcutting")) : NAME_None;
	float Raw = PerHit * (bHeavy ? 2 : 1) * WRTune(TEXT("server.gatherMultiplier"), 1.f) * (Tool ? Tool->Yield : 1.f);
	if (bOre) Raw *= 1.f + UWRLifeSkillsComponent::StatFor(Gatherer, TEXT("miningYield"));
	if (bWood) Raw *= 1.f + UWRLifeSkillsComponent::StatFor(Gatherer, TEXT("woodYield")) + UWRLifeSkillsComponent::StatFor(Gatherer, TEXT("woodExtra"));
	if (!bOre && !bWood) Raw *= 1.f + UWRLifeSkillsComponent::StatFor(Gatherer, TEXT("herbExtra"));
	int32 Amount = FMath::FloorToInt(Raw) + (FMath::FRand() < FMath::Frac(Raw) ? 1 : 0);
	if (bOre && ItemId != TEXT("stone") && FMath::FRand() < UWRLifeSkillsComponent::StatFor(Gatherer, TEXT("doubleOre")))
	{
		Amount *= 2;
	}
	if (!Skill.IsNone())
	{
		UWRLifeSkillsComponent::Award(Gatherer, Skill, bOre ? TEXT("mine") : TEXT("chop"), 4.f);
	}
	const int32 Left = Inventory->AddItem(ItemId, Amount);
	if (ItemId == TEXT("berries") && FMath::FRand() < 0.35f)
	{
		Inventory->AddItem(TEXT("seeds"), 1);
	}
	const int32 Got = Amount - Left;
	if (AWRPlayerController* PC = Cast<AWRPlayerController>(Gatherer->GetController()))
	{
		const FWRItemDef* Def = FWRItemRegistry::Get().Find(ItemId);
		const FWRItemDef* ToolDef = Tool ? FWRItemRegistry::Get().Find(Tool->ItemId) : nullptr;
		PC->ClientNotify(Got <= 0 ? TEXT("Your bag is full")
			: FString::Printf(TEXT("+%d %s%s"), Got, Def ? *Def->Name : *ItemId.ToString(), ToolDef ? *FString::Printf(TEXT("  (%s)"), *ToolDef->Name) : TEXT("")));
	}
	UE_LOG(LogWildreach, Log, TEXT("GATHER %s got %d %s from %s, tool %s"), *Gatherer->GetName(), Got, *ItemId.ToString(), *Source,
		Tool ? *Tool->ItemId.ToString() : TEXT("none"));
	return Got;
}

// ---------------------------------------------------------------- resource node

AWRResourceNode::AWRResourceNode()
{
	bReplicates = true;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (Cone.Succeeded()) Mesh->SetStaticMesh(Cone.Object);
	if (ShapeMaterial.Succeeded()) Mesh->SetMaterial(0, ShapeMaterial.Object);
}

void AWRResourceNode::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRResourceNode, ItemId);
	DOREPLIFETIME(AWRResourceNode, bDepleted);
}

void AWRResourceNode::BeginPlay()
{
	Super::BeginPlay();
	HitsLeft = Hits;
	OnRep_Look();
}

void AWRResourceNode::OnRep_Look()
{
	// Placeholder look per resource: tall green cone = tree, grey = rock, orange = copper, yellow = sulfur, pale = lichen.
	FLinearColor Color(0.5f, 0.5f, 0.5f);
	FVector Scale(1.f, 1.f, 1.f);
	if (ItemId == TEXT("wood")) { Color = FLinearColor(0.15f, 0.4f, 0.15f); Scale = FVector(1.2f, 1.2f, 4.f); }
	else if (ItemId == TEXT("stone")) { Color = FLinearColor(0.45f, 0.45f, 0.45f); Scale = FVector(2.f, 2.f, 1.f); }
	else if (ItemId == TEXT("copper_ore")) { Color = FLinearColor(0.75f, 0.4f, 0.2f); Scale = FVector(1.5f, 1.5f, 1.2f); }
	else if (ItemId == TEXT("sulfur_crystal")) { Color = FLinearColor(0.9f, 0.85f, 0.2f); Scale = FVector(0.8f, 0.8f, 1.6f); }
	else if (ItemId == TEXT("lichen")) { Color = FLinearColor(0.6f, 0.75f, 0.55f); Scale = FVector(1.5f, 1.5f, 0.4f); }
	else if (ItemId == TEXT("berries")) { Color = FLinearColor(0.45f, 0.1f, 0.25f); Scale = FVector(1.2f, 1.2f, 0.8f); }
	else if (ItemId == TEXT("fiber")) { Color = FLinearColor(0.55f, 0.6f, 0.25f); Scale = FVector(0.8f, 0.8f, 1.2f); }
	else if (ItemId == TEXT("tin_ore")) { Color = FLinearColor(0.7f, 0.72f, 0.75f); Scale = FVector(1.5f, 1.5f, 1.2f); }
	else if (ItemId == TEXT("iron_ore")) { Color = FLinearColor(0.35f, 0.2f, 0.18f); Scale = FVector(1.6f, 1.6f, 1.3f); }
	SetActorScale3D(Scale);
	if (UMaterialInstanceDynamic* Mat = Mesh->CreateAndSetMaterialInstanceDynamic(0))
	{
		Mat->SetVectorParameterValue(TEXT("Color"), Color);
	}
}

void AWRResourceNode::OnRep_Depleted()
{
	SetActorHiddenInGame(bDepleted);
	SetActorEnableCollision(!bDepleted);
}

void AWRResourceNode::Harvest(AWRCharacterBase* Gatherer, bool bHeavy)
{
	if (!HasAuthority() || bDepleted || !Gatherer)
	{
		return;
	}
	UWRInventoryComponent* Inventory = UWRInventoryComponent::FindFor(Gatherer);
	if (!Inventory)
	{
		return;
	}
	FWRGathering::Gather(Gatherer, ItemId, PerHit, bHeavy, FString::Printf(TEXT("%s (%d hits left)"), *GetName(), HitsLeft - 1));
	{
		// Wood chips, stone (and ore) chips with dust, or plant bits, toward the gatherer.
		const FName Kind = FWRGathering::ToolKindFor(ItemId);
		const FName Effect = Kind == TEXT("hatchet") ? TEXT("gather.wood") : Kind == TEXT("pick") ? TEXT("gather.stone") : TEXT("gather.fiber");
		const FVector Toward = (Gatherer->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		FVector Origin, Extent;
		GetActorBounds(false, Origin, Extent);
		const FVector At = Origin + Toward * FMath::Min(Extent.X, Extent.Y) * 0.8f;
		WRFx::Broadcast(this, Effect, At, Toward.Rotation(), HitsLeft <= 1 ? 1.8f : (bHeavy ? 1.3f : 1.f), Effect);
	}
	// Ore veins sometimes give up a gem (Crafting tab: gems come from mines and caves).
	if (ItemId.ToString().EndsWith(TEXT("_ore")) && FMath::FRand() < GemChance + UWRLifeSkillsComponent::StatFor(Gatherer, TEXT("gemChance")))
	{
		static const FName Gems[] = { TEXT("gem_ruby"), TEXT("gem_topaz"), TEXT("gem_onyx") };
		Inventory->AddItem(Gems[FMath::RandRange(0, 2)], 1);
	}

	if (--HitsLeft <= 0)
	{
		bDepleted = true;
		OnRep_Depleted();
		GetWorldTimerManager().SetTimer(RespawnTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			bDepleted = false;
			HitsLeft = Hits;
			OnRep_Depleted();
		}), RespawnSeconds, false);
	}
}
