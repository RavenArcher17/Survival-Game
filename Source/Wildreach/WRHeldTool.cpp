// Tools held in the weapon hand (Data/items.json "tool" + "equip": {"slot": "weapon"}): picking a hatchet, a pick or a
// fishing rod on the hotbar puts it in the right hand in place of the sword. Each is a modelled mesh (WRToolMesh: a
// turned haft, a knapped or ground head, rawhide lashing; the rod's wand, grip, spool and rings), held the way the
// sword is: in the fist, the haft along the sword's blade, the head's edge on the side the fingers point to (the side
// a downward chop leads with). Only clients draw it; the server just replicates MeleeItem.
#include "WRCharacter.h"
#include "WRResourceNode.h"
#include "WRToolMesh.h"
#include "WRInventory.h"
#include "WRTuning.h"
#include "AnimationRuntime.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "ProceduralMeshComponent.h"

namespace WRHeldToolDetail
{
	/** The tool's sizes (gather.toolLook, cm): length, how much handle sits behind the fist, thickness, head scale. */
	struct FSizes { float Length, Back, Thick, Head; };
	FSizes SizesFor(const FWRToolDef& Tool)
	{
		const FWRTuning& T = FWRTuning::Get();
		const bool bPick = Tool.Kind == TEXT("pick"), bRod = Tool.Kind == TEXT("rod");
		FSizes S;
		S.Length = bRod ? T.Num(TEXT("gather.toolLook.rodLength"), 150.f) : T.Num(bPick ? TEXT("gather.toolLook.pickLength") : TEXT("gather.toolLook.hatchetLength"), bPick ? 62.f : 52.f);
		S.Back = bRod ? T.Num(TEXT("gather.toolLook.rodGripBack"), 26.f) : T.Num(TEXT("gather.toolLook.gripBack"), 8.f);
		S.Thick = bRod ? T.Num(TEXT("gather.toolLook.rodThickness"), 3.f) : T.Num(TEXT("gather.toolLook.handleThickness"), 3.4f);
		S.Head = T.Num(TEXT("gather.toolLook.headScale"), 1.f);
		return S;
	}
}

void AWRCharacter::SetMeleeItem(FName ItemId)
{
	if (MeleeItem == ItemId)
	{
		return;
	}
	MeleeItem = ItemId;
	OnRep_RangedLook();  // a listen server or standalone game draws its own copy now
}

bool AWRCharacter::IsClassWeapon(FName ItemId)
{
	// A weapon of a class the hero has no art mesh for (the sword has its own): modelled in code (WRToolMesh::Weapon).
	const FWRItemDef* Def = ItemId.IsNone() ? nullptr : FWRItemRegistry::Get().Find(ItemId);
	return Def && !Def->WeaponClass.IsNone() && Def->WeaponClass != TEXT("sword") && Def->WeaponClass != TEXT("fists");
}

void AWRCharacter::UpdateToolLook(bool bShow)
{
	using namespace WRHeldToolDetail;
	const FWRToolDef* Tool = bShow ? FWRGathering::FindTool(MeleeItem) : nullptr;
	const FWRItemDef* WeaponDef = bShow && IsClassWeapon(MeleeItem) ? FWRItemRegistry::Get().Find(MeleeItem) : nullptr;
	const USkeletalMesh* BodyAsset = Body->GetSkeletalMeshAsset();
	const FName Socket = WeaponMesh->GetAttachSocketName();
	const bool bHand = BodyAsset && !Socket.IsNone() && (BodyAsset->FindSocket(Socket) || BodyAsset->GetRefSkeleton().FindBoneIndex(Socket) != INDEX_NONE);
	if ((!Tool && !WeaponDef) || !bHand)
	{
		if (ToolMesh) ToolMesh->SetVisibility(false);
		return;
	}
	if (!ToolMesh)
	{
		ToolMesh = NewObject<UProceduralMeshComponent>(this, TEXT("HeldTool"));
		ToolMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ToolMesh->SetGenerateOverlapEvents(false);
		ToolMesh->SetupAttachment(Body, Socket);
		ToolMesh->RegisterComponent();
	}
	if (ToolMeshFor != MeleeItem && WeaponDef)
	{
		// Its metal: copper's red-brown, bronze's gold-brown, iron's dark grey (linear), the forge's look.
		const FName Metal = WeaponDef->Metal;
		// Starsteel: meteoric blue-black, darker than steel with a cold sheen (Widmanstatten iron etched).
		const FLinearColor Colour = Metal == TEXT("bronze") ? FLinearColor(0.42f, 0.27f, 0.1f) : Metal == TEXT("iron") ? FLinearColor(0.16f, 0.16f, 0.17f)
			: Metal == TEXT("steel") ? FLinearColor(0.3f, 0.31f, 0.33f) : Metal == TEXT("starsteel") ? FLinearColor(0.06f, 0.07f, 0.11f) : FLinearColor(0.45f, 0.18f, 0.08f);
		FWRToolMesh::Weapon(WeaponDef->WeaponClass, Colour, WRTune(TEXT("gather.toolLook.weaponScale"), 1.f)).ApplyTo(ToolMesh);
		ToolMeshFor = MeleeItem;
	}
	if (ToolMeshFor != MeleeItem)
	{
		// Crude tools (made by hand) are flint on a rough stick with plant-fibre cord; stone ones a ground head on a
		// smoothed haft bound with rawhide.
		const FSizes S = SizesFor(*Tool);
		const bool bCrude = MeleeItem.ToString().StartsWith(TEXT("crude"));
		FWRToolMesh::Tool(Tool->Kind, bCrude, Tool->Head, S.Length, S.Back, S.Thick, S.Head).ApplyTo(ToolMesh);
		ToolMeshFor = MeleeItem;
	}
	// The fist's frame, as the sword's (AWRCharacter::FistFrame): the weapon socket sits in the palm, X out of the fist
	// past the index finger (the haft runs that way to the head), Y toward the fingers (the striking side).
	if (ToolMesh->GetAttachSocketName() != Socket)
	{
		ToolMesh->AttachToComponent(Body, FAttachmentTransformRules::KeepRelativeTransform, Socket);
	}
	ToolMesh->SetRelativeTransform(FistFrame());
	ToolMesh->SetVisibility(true);
}

FVector AWRCharacter::RodTipLocation() const
{
	using namespace WRHeldToolDetail;
	const FWRToolDef* Tool = FWRGathering::FindTool(MeleeItem);
	if (!Tool || Tool->Kind != TEXT("rod") || !ToolMesh || !ToolMesh->IsVisible())
	{
		// Not drawn here (a dedicated server, first person hiding it): from the hand, a rod's length ahead and up.
		const FVector Hand = Body ? Body->GetSocketLocation(WeaponMesh->GetAttachSocketName()) : GetActorLocation();
		return Hand + GetActorForwardVector() * 120.f + FVector(0.f, 0.f, 60.f);
	}
	const FSizes S = SizesFor(*Tool);
	return ToolMesh->GetComponentTransform().TransformPosition(FWRToolMesh::RodTip(S.Length, S.Back));
}
