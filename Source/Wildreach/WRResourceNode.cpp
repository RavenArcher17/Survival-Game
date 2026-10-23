#include "WRResourceNode.h"
#include "Wildreach.h"
#include "WRCharacterBase.h"
#include "WRInventory.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

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
	const int32 Amount = PerHit * (bHeavy ? 2 : 1);
	const int32 Left = Inventory->AddItem(ItemId, Amount);
	UE_LOG(LogWildreach, Log, TEXT("GATHER %s got %d %s from %s (%d hits left)"), *Gatherer->GetName(), Amount - Left, *ItemId.ToString(), *GetName(), HitsLeft - 1);

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
