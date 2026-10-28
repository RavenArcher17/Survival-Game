#include "WRResourceNode.h"
#include "WRResourceLook.h"
#include "WRLifeSkills.h"
#include "WRTuning.h"
#include "WRFx.h"
#include "WRSound.h"
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
					double Num = 1.0;
					if ((*Tool)->TryGetNumberField(TEXT("yield"), Num)) T.Yield = Num;
					if ((*Tool)->TryGetNumberField(TEXT("damage"), Num)) T.Damage = Num;
					const TArray<TSharedPtr<FJsonValue>>* Head = nullptr;
					if ((*Tool)->TryGetArrayField(TEXT("head"), Head) && Head->Num() == 3)
					{
						T.Head = FLinearColor((*Head)[0]->AsNumber(), (*Head)[1]->AsNumber(), (*Head)[2]->AsNumber());
					}
				}
			}
		}
		return Out;
	}();
	return List;
}

const FWRToolDef* FWRGathering::FindTool(FName ItemId)
{
	return ItemId.IsNone() ? nullptr : Tools().FindByPredicate([ItemId](const FWRToolDef& T) { return T.ItemId == ItemId; });
}

FName FWRGathering::ToolKindFor(FName ItemId)
{
	const FString Id = ItemId.ToString();
	if (ItemId == TEXT("wood")) return TEXT("hatchet");
	if (ItemId == TEXT("stone") || Id.EndsWith(TEXT("_ore")) || Id.StartsWith(TEXT("sulfur"))) return TEXT("pick");
	return NAME_None;
}

const FWRToolDef* FWRGathering::HeldTool(FName Kind, FName HeldItem, float Durability)
{
	// A broken tool stops giving its bonus until it's repaired (it isn't lost), as broken weapons and armor do.
	const FWRToolDef* Tool = FindTool(HeldItem);
	return Tool && !Kind.IsNone() && Tool->Kind == Kind && Durability > 0.f ? Tool : nullptr;
}

bool FWRGathering::IsSharp(const FWRItemStack& Held)
{
	if (Held.IsEmpty() || Held.Durability <= 0.f)
	{
		return false;
	}
	if (const FWRToolDef* Tool = FindTool(Held.ItemId))
	{
		return Tool->Kind == TEXT("hatchet");
	}
	const FWRItemDef* Def = FWRItemRegistry::Get().Find(Held.ItemId);
	return Def && Def->Slot == EWREquipSlot::Weapon;  // a sword
}

bool FWRGathering::WearHeld(AWRCharacterBase* Gatherer, bool bRightTool)
{
	UWRInventoryComponent* Inventory = Gatherer && Gatherer->HasAuthority() ? UWRInventoryComponent::FindFor(Gatherer) : nullptr;
	FWRItemStack* Held = Inventory ? Inventory->GetEquippedMutable(EWREquipSlot::Weapon) : nullptr;
	if (!Held || Held->IsEmpty() || Held->Durability <= 0.f)
	{
		return false;
	}
	Inventory->WearEquipped(EWREquipSlot::Weapon, bRightTool ? WRTune(TEXT("gather.tools.wearPerHit"), 1.f) : WRTune(TEXT("gather.tools.wearOther"), 1.f));
	if (Held->Durability > 0.f)
	{
		return false;
	}
	if (AWRPlayerController* PC = Cast<AWRPlayerController>(Gatherer->GetController()))
	{
		const FWRItemDef* Def = FWRItemRegistry::Get().Find(Held->ItemId);
		const FString Name = Def ? Def->Name : Held->ItemId.ToString();
		PC->ClientNotify(FindTool(Held->ItemId) ? FString::Printf(TEXT("Your %s broke: repair it at a workbench"), *Name.ToLower())
			: FString::Printf(TEXT("Your %s broke: repair it at a forge"), *Name.ToLower()));
	}
	WRSound::Broadcast(Gatherer, TEXT("hit.block"));
	return true;
}

int32 FWRGathering::Gather(AWRCharacterBase* Gatherer, FName ItemId, float PerHit, bool bHeavy, const FString& Source)
{
	UWRInventoryComponent* Inventory = Gatherer && Gatherer->HasAuthority() ? UWRInventoryComponent::FindFor(Gatherer) : nullptr;
	if (!Inventory)
	{
		return 0;
	}
	// What's in the weapon hand decides: the right tool gives its yield, anything else the base rate.
	const FWRItemStack& Held = Inventory->GetEquipped(EWREquipSlot::Weapon);
	const FWRToolDef* Tool = HeldTool(ToolKindFor(ItemId), Held.ItemId, Held.Durability);
	const FWRToolDef* HeldDef = FindTool(Held.ItemId);
	const bool bBrokenTool = HeldDef && !ToolKindFor(ItemId).IsNone() && HeldDef->Kind == ToolKindFor(ItemId) && Held.Durability <= 0.f;

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
	const FName HeldId = Held.ItemId;
	const float HeldBefore = Held.Durability;
	if (AWRPlayerController* PC = Cast<AWRPlayerController>(Gatherer->GetController()))
	{
		const FWRItemDef* Def = FWRItemRegistry::Get().Find(ItemId);
		const FWRItemDef* ToolDef = Tool ? FWRItemRegistry::Get().Find(Tool->ItemId) : nullptr;
		const FWRItemDef* BrokenDef = bBrokenTool ? FWRItemRegistry::Get().Find(HeldId) : nullptr;
		const FString Why = ToolDef ? FString::Printf(TEXT("  (%s)"), *ToolDef->Name)
			: BrokenDef ? FString::Printf(TEXT("  (%s broken: repair it)"), *BrokenDef->Name) : FString();
		PC->ClientNotify(Got <= 0 ? FString(TEXT("Your bag is full")) : FString::Printf(TEXT("+%d %s%s"), Got, Def ? *Def->Name : *ItemId.ToString(), *Why));
	}
	// The swing wears what's in hand (after the notice, so "it broke" comes second).
	if (Got > 0)
	{
		WearHeld(Gatherer, Tool != nullptr);
	}
	UE_LOG(LogWildreach, Log, TEXT("GATHER %s got %d %s from %s, tool %s, held %s %.0f"), *Gatherer->GetName(), Got, *ItemId.ToString(), *Source,
		Tool ? *Tool->ItemId.ToString() : TEXT("none"), HeldId.IsNone() ? TEXT("nothing") : *HeldId.ToString(), HeldBefore);
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
	// Each item's look (a tree, a boulder, an ore boulder, sulfur crystals, a lichen rock, a berry bush, a fibre plant)
	// over the node's mesh, which becomes its hidden collider (WRResourceLook.cpp).
	WRResourceLook::Apply(this, Mesh);
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
