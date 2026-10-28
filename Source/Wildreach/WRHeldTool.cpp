// Tools held in the weapon hand (Data/items.json "tool" + "equip": {"slot": "weapon"}): picking a hatchet or a pick on
// the hotbar puts it in the right hand in place of the sword, and it's what you swing. There are no tool models in
// Content, so each is built here from the engine's basic shapes (a wooden handle, a stone head, a lashing), held the
// way the sword is: in the fist, the handle along the sword's blade, the head's edge on the side the fingers point
// to (the side a downward chop leads with). Only clients draw it; the server just replicates MeleeItem.
#include "WRCharacter.h"
#include "WRResourceNode.h"
#include "WRTuning.h"
#include "AnimationRuntime.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace WRHeldToolDetail
{
	enum class EShape : uint8 { Cube, Cylinder, Cone };

	/** One piece of a tool in its own frame: X from the grip toward the head, Y toward the striking side (cm). */
	struct FPart
	{
		EShape Shape = EShape::Cube;
		FVector Size;                // cube: X/Y/Z; cylinder and cone: diameter, diameter, length (along their axis)
		FVector Centre;
		FVector Axis = FVector::ZAxisVector;  // where a cylinder's / cone's length (its tip) points
		FLinearColor Color;
	};

	UStaticMesh* ShapeMesh(EShape Shape)
	{
		const TCHAR* Path = Shape == EShape::Cylinder ? TEXT("/Engine/BasicShapes/Cylinder.Cylinder")
			: Shape == EShape::Cone ? TEXT("/Engine/BasicShapes/Cone.Cone") : TEXT("/Engine/BasicShapes/Cube.Cube");
		return LoadObject<UStaticMesh>(nullptr, Path);
	}

	/** The pieces of a hatchet or a pick, sized by gather.toolLook in tuning.json. */
	TArray<FPart> PartsFor(const FWRToolDef& Tool)
	{
		const FWRTuning& T = FWRTuning::Get();
		const bool bPick = Tool.Kind == TEXT("pick");
		const float Length = T.Num(bPick ? TEXT("gather.toolLook.pickLength") : TEXT("gather.toolLook.hatchetLength"), bPick ? 62.f : 52.f);
		const float Back = T.Num(TEXT("gather.toolLook.gripBack"), 8.f);       // handle left behind the fist
		const float Thick = T.Num(TEXT("gather.toolLook.handleThickness"), 3.4f);
		const float Head = T.Num(TEXT("gather.toolLook.headScale"), 1.f);
		const FLinearColor Wood(0.23f, 0.13f, 0.06f);
		const FLinearColor Lash(0.42f, 0.33f, 0.18f);
		const FLinearColor Edge = Tool.Head * 1.6f;
		const float HeadX = Length - Back - 6.f * Head;  // the head sits just short of the handle's end

		TArray<FPart> Out;
		FPart& Handle = Out.AddDefaulted_GetRef();
		Handle.Shape = EShape::Cylinder;
		Handle.Size = FVector(Thick, Thick, Length);
		Handle.Centre = FVector(Length * 0.5f - Back, 0.f, 0.f);
		Handle.Axis = FVector::XAxisVector;
		Handle.Color = Wood;
		if (bPick)
		{
			// A bar across the handle's end, drawn to a point each way.
			FPart& Bar = Out.AddDefaulted_GetRef();
			Bar.Size = FVector(4.5f, 30.f, 4.f) * Head;
			Bar.Centre = FVector(HeadX, 0.f, 0.f);
			Bar.Color = Tool.Head;
			for (const float Side : { 1.f, -1.f })
			{
				FPart& Point = Out.AddDefaulted_GetRef();
				Point.Shape = EShape::Cone;
				Point.Size = FVector(4.2f, 4.2f, 11.f) * Head;
				Point.Centre = FVector(HeadX, Side * (15.f + 5.5f) * Head, 0.f);
				Point.Axis = FVector(0.f, Side, 0.f);
				Point.Color = Side > 0.f ? Edge : Tool.Head;
			}
		}
		else
		{
			// A wedge of stone: a thick poll on the handle, a thinner, wider bit out on the striking side.
			FPart& Poll = Out.AddDefaulted_GetRef();
			Poll.Size = FVector(8.f, 12.f, 3.6f) * Head;
			Poll.Centre = FVector(HeadX, 2.f * Head, 0.f);
			Poll.Color = Tool.Head;
			FPart& Bit = Out.AddDefaulted_GetRef();
			Bit.Size = FVector(12.f, 6.f, 1.8f) * Head;
			Bit.Centre = FVector(HeadX, 10.5f * Head, 0.f);
			Bit.Color = Edge;
		}
		// Fibre lashing the head on.
		FPart& Ring = Out.AddDefaulted_GetRef();
		Ring.Shape = EShape::Cylinder;
		Ring.Size = FVector(Thick + 1.4f, Thick + 1.4f, 3.f);
		Ring.Centre = FVector(HeadX - 6.f * Head, 0.f, 0.f);
		Ring.Axis = FVector::XAxisVector;
		Ring.Color = Lash;
		return Out;
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

void AWRCharacter::UpdateToolLook(bool bShow)
{
	using namespace WRHeldToolDetail;
	const FWRToolDef* Tool = bShow ? FWRGathering::FindTool(MeleeItem) : nullptr;
	const USkeletalMesh* BodyAsset = Body->GetSkeletalMeshAsset();
	const FName Socket = WeaponMesh->GetAttachSocketName();
	const int32 HandBone = BodyAsset && !Socket.IsNone() ? BodyAsset->GetRefSkeleton().FindBoneIndex(Socket) : INDEX_NONE;
	if (!Tool || HandBone == INDEX_NONE)
	{
		for (UStaticMeshComponent* Part : ToolParts)
		{
			if (Part) Part->SetVisibility(false);
		}
		return;
	}
	const TArray<FPart> Parts = PartsFor(*Tool);
	if (ToolPartsFor != MeleeItem || ToolParts.Num() != Parts.Num())
	{
		for (UStaticMeshComponent* Part : ToolParts)
		{
			if (Part) Part->DestroyComponent();
		}
		ToolParts.Reset();
		for (int32 i = 0; i < Parts.Num(); ++i)
		{
			UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this, *FString::Printf(TEXT("ToolPart%d"), i));
			Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Part->SetGenerateOverlapEvents(false);
			Part->SetupAttachment(Body, Socket);
			Part->RegisterComponent();
			ToolParts.Add(Part);
		}
		ToolPartsFor = MeleeItem;
	}

	// The fist's frame, worked out as the sword's is (AWRCharacter::LoadBodyArt): in the reference pose a held blade points
	// along the body's forward and the fingers run out along the arm; grip in the palm, partway to the index finger.
	const FReferenceSkeleton& Ref = BodyAsset->GetRefSkeleton();
	const FTransform HandCS = FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, HandBone);
	const int32 FingerBone = Ref.FindBoneIndex(FName(*(Socket.ToString() + TEXT("Index1"))));
	const FVector FingerCS = FingerBone != INDEX_NONE ? FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, FingerBone).GetLocation() : HandCS.GetLocation();
	const FVector BodyForward = FRotator(0.f, BodyYaw, 0.f).UnrotateVector(FVector::ForwardVector);
	const FVector Blade = HandCS.InverseTransformVectorNoScale(BodyForward).GetSafeNormal();
	const FVector Fingers = HandCS.InverseTransformPositionNoScale(FingerCS);
	const FVector Grip = HandCS.InverseTransformPositionNoScale(FMath::Lerp(HandCS.GetLocation(), FingerCS, 0.6f));
	const FTransform Fist(FQuat(FRotationMatrix::MakeFromXY(Blade, Fingers)), Grip);
	UMaterialInterface* Shape = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	for (int32 i = 0; i < Parts.Num(); ++i)
	{
		const FPart& P = Parts[i];
		UStaticMeshComponent* Comp = ToolParts[i];
		UStaticMesh* ShapeAsset = ShapeMesh(P.Shape);
		if (!Comp || !ShapeAsset)
		{
			continue;
		}
		Comp->SetStaticMesh(ShapeAsset);
		// Scale the shape to its size and put its bounds' centre on the part's centre, its length (Z) along its axis.
		const FBox Bounds = ShapeAsset->GetBoundingBox();
		const FVector Scale = P.Size / Bounds.GetSize().ComponentMax(FVector(1.f));
		const FQuat Rot = P.Shape == EShape::Cube ? FQuat::Identity : FQuat(FRotationMatrix::MakeFromZ(P.Axis));
		const FVector Loc = P.Centre - Rot.RotateVector(Bounds.GetCenter() * Scale);
		Comp->SetRelativeTransform(FTransform(Rot, Loc, Scale) * Fist);
		if (Shape)
		{
			Comp->SetMaterial(0, Shape);
			if (UMaterialInstanceDynamic* Mat = Comp->CreateAndSetMaterialInstanceDynamic(0))
			{
				Mat->SetVectorParameterValue(TEXT("Color"), P.Color);
			}
		}
		Comp->SetVisibility(true);
	}
}
