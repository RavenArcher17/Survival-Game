#include "WRResourceLook.h"
#include "WRResourceNode.h"
#include "WRTuning.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace WRResourceLookParts
{
	UStaticMesh* LoadMesh(const FString& Path) { return LoadObject<UStaticMesh>(nullptr, *Path); }
	FString Rock(const TCHAR* N) { return FString::Printf(TEXT("/Game/SimpleRocks/3D_Assets/Meshes/Rocks/SM_SimpleRock_%s.SM_SimpleRock_%s"), N, N); }
	FString Shrub(const TCHAR* L) { return FString::Printf(TEXT("/Game/GV_FreeShrubsPack/Meshes/Shrubs/Wind/Shrub_%s/GV_Vol7_Shrub_%s_full_type1.GV_Vol7_Shrub_%s_full_type1"), L, L, L); }
	FString Prop(const TCHAR* Folder, const TCHAR* Name) { return FString::Printf(TEXT("/Game/Props/%s/%s/StaticMeshes/%s.%s"), Folder, Name, Name, Name); }

	/** A tinted copy of the project's world-aligned rock material (the island's boulders use the same). */
	UMaterialInterface* RockMaterial(UObject* Outer, const FLinearColor& Tint)
	{
		UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_Rock.M_Rock"));
		if (!Base)
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, Outer);
		MID->SetVectorParameterValue(TEXT("Tint"), Tint);
		return MID;
	}

	UMaterialInterface* ShapeColour(UObject* Outer, const FLinearColor& Color)
	{
		UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		if (!Base)
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, Outer);
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		return MID;
	}

	/** One of the node's look components (made once, cleared on a rebuild: the look can be applied more than once). */
	template <typename T>
	T* Part(AWRResourceNode* Node, USceneComponent* Parent, FName Tag)
	{
		for (UActorComponent* C : Node->GetComponents())
		{
			if (C->ComponentHasTag(Tag))
			{
				if (T* Found = Cast<T>(C)) return Found;
			}
		}
		T* Made = NewObject<T>(Node);
		Made->ComponentTags.Add(Tag);
		Made->SetupAttachment(Parent);
		Made->SetUsingAbsoluteScale(true);  // the node's own scale sizes its collider, not the look
		Made->SetCollisionEnabled(ECollisionEnabled::NoCollision);  // the hidden collider is what swings and feet hit
		Made->bAffectDistanceFieldLighting = false;
		Made->RegisterComponent();
		Node->AddInstanceComponent(Made);
		return Made;
	}

	/** The old placeholder: the node's cone in a flat colour per item (when the art packs aren't installed). */
	void Placeholder(AWRResourceNode* Node, UStaticMeshComponent* Collider)
	{
		const FName ItemId = Node->ItemId;
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
		Node->SetActorScale3D(Scale);
		Collider->SetHiddenInGame(false);
		if (UMaterialInstanceDynamic* Mat = Collider->CreateAndSetMaterialInstanceDynamic(0))
		{
			Mat->SetVectorParameterValue(TEXT("Color"), Color);
		}
	}

	/** Fit a mesh to a size (cm; a zero axis follows the others' scale) and stand it on the ground under the node. */
	FTransform Fit(UStaticMesh* Mesh, const FVector& Size, float Sink, const FRotator& Rot = FRotator::ZeroRotator, const FVector& Offset = FVector::ZeroVector)
	{
		const FBox Box = Mesh->GetBoundingBox();
		const FVector Ext = Box.GetSize().ComponentMax(FVector(1.f));
		FVector Scale = Size / Ext;
		const float Uniform = Size.Z > 0.f ? Scale.Z : FMath::Max(Scale.X, Scale.Y);
		if (Size.X <= 0.f) Scale.X = Uniform;
		if (Size.Y <= 0.f) Scale.Y = Uniform;
		if (Size.Z <= 0.f) Scale.Z = Uniform;
		const FVector Pivot(-(Box.Min.X + Box.Max.X) * 0.5f * Scale.X, -(Box.Min.Y + Box.Max.Y) * 0.5f * Scale.Y, -Box.Min.Z * Scale.Z - Sink);
		return FTransform(Rot, Offset + Rot.RotateVector(Pivot), Scale);
	}
}

void WRResourceLook::Apply(AWRResourceNode* Node, UStaticMeshComponent* Collider)
{
	using namespace WRResourceLookParts;
	if (!Node || !Collider)
	{
		return;
	}
	const FName Item = Node->ItemId;
	const FString Id = Item.ToString();
	const FVector At = Node->GetActorLocation();
	const bool bTundra = At.X > 0.f;  // the ridge runs down the island's middle (world X 0): ash west, snow east
	const uint32 Seed = GetTypeHash(FIntVector(FMath::FloorToInt(At.X / 50.f), FMath::FloorToInt(At.Y / 50.f), 0));
	FRandomStream Rand(Seed);

	// Collider: the node's engine mesh, now a hidden cylinder about the look's footprint (cm: width, height).
	const bool bOre = Id.EndsWith(TEXT("_ore"));
	FVector2D Body(110.f, 120.f);
	if (Item == TEXT("wood")) Body = FVector2D(90.f, 420.f);
	else if (Item == TEXT("stone")) Body = FVector2D(200.f, 140.f);
	else if (bOre) Body = FVector2D(190.f, 150.f);
	else if (Item == TEXT("sulfur_crystal")) Body = FVector2D(110.f, 150.f);
	else if (Item == TEXT("lichen")) Body = FVector2D(150.f, 90.f);
	UStaticMesh* Cylinder = LoadMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* Sphere = LoadMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (!Cylinder || !Sphere)
	{
		Placeholder(Node, Collider);
		return;
	}

	// The look's meshes: everything below comes from packs the island's dressing already uses.
	const TCHAR* RockNames[] = { TEXT("01A"), TEXT("02A"), TEXT("03A"), TEXT("05A"), TEXT("08A"), TEXT("12A") };
	UStaticMesh* Boulder = LoadMesh(Rock(RockNames[Seed % UE_ARRAY_COUNT(RockNames)]));
	UStaticMesh* Shard = LoadMesh(Rock(TEXT("08A")));
	UStaticMesh* Main = nullptr;
	if (Item == TEXT("wood"))
	{
		static const TCHAR* Char[][2] = { { TEXT("CharTreeA"), TEXT("char_tree_a_textured") }, { TEXT("CharTreeB"), TEXT("char_tree_b_textured") },
			{ TEXT("CharTreeC"), TEXT("char_tree_c_textured") }, { TEXT("CharTreeD"), TEXT("char_tree_d_textured") } };
		static const TCHAR* Pine[][2] = { { TEXT("PineB"), TEXT("pine_b_textured") }, { TEXT("PineC"), TEXT("pine_c_textured") }, { TEXT("FirA"), TEXT("fir_a_textured") } };
		Main = bTundra ? LoadMesh(Prop(Pine[Seed % 3][0], Pine[Seed % 3][1])) : LoadMesh(Prop(Char[Seed % 4][0], Char[Seed % 4][1]));
	}
	else if (Item == TEXT("berries") || Item == TEXT("fiber"))
	{
		Main = LoadMesh(Shrub(Item == TEXT("berries") ? (Seed % 2 ? TEXT("B") : TEXT("D")) : (Seed % 2 ? TEXT("E") : TEXT("C"))));
	}
	else
	{
		Main = Boulder;
	}
	if (!Main || !Shard)
	{
		Placeholder(Node, Collider);
		return;
	}
	Node->SetActorScale3D(FVector(Body.X / 100.f, Body.X / 100.f, Body.Y / 100.f));
	Collider->SetStaticMesh(Cylinder);
	Collider->SetHiddenInGame(true);
	Collider->SetCastShadow(false);
	const UWorld* World = Node->GetWorld();
	if (World && World->GetNetMode() == NM_DedicatedServer)
	{
		return;  // the server only needs the collider
	}

	UStaticMeshComponent* Look = Part<UStaticMeshComponent>(Node, Collider, TEXT("WRLook"));
	UInstancedStaticMeshComponent* Bits = Part<UInstancedStaticMeshComponent>(Node, Collider, TEXT("WRLookBits"));
	Bits->ClearInstances();
	Look->SetStaticMesh(Main);
	Look->SetWorldScale3D(FVector::OneVector);
	Look->SetRelativeLocation(FVector::ZeroVector);
	Look->SetRelativeRotation(FRotator::ZeroRotator);
	for (int32 Slot = 0; Slot < Look->GetNumMaterials(); ++Slot) Look->SetMaterial(Slot, nullptr);  // the mesh's own, unless set below
	// The look's transform is worked out in world units at the node, then expressed under the scaled collider.
	const FVector ParentScale = Node->GetActorScale3D().ComponentMax(FVector(0.01f));
	const auto Place = [&](const FTransform& T)
	{
		Look->SetRelativeLocation(T.GetLocation() / ParentScale);
		Look->SetRelativeRotation(T.GetRotation());
		Look->SetWorldScale3D(T.GetScale3D());
	};
	const auto Tint = [&](UMaterialInterface* M) { for (int32 Slot = 0; Slot < FMath::Max(1, Look->GetNumMaterials()); ++Slot) Look->SetMaterial(Slot, M); };
	const FLinearColor BasaltTint(0.11f, 0.1f, 0.095f), FrostTint(0.62f, 0.74f, 0.88f);
	// Bits share one mesh per node: shards and lumps are stretched / squashed boulders, berries are small spheres.
	const auto Bit = [&](const FVector& Where, const FRotator& Rot, const FVector& Size)
	{
		const FBox Box = Bits->GetStaticMesh()->GetBoundingBox();
		const FVector Ext = Box.GetSize().ComponentMax(FVector(1.f));
		const FVector Scale = Size / Ext;
		const FVector Pivot(-(Box.Min.X + Box.Max.X) * 0.5f * Scale.X, -(Box.Min.Y + Box.Max.Y) * 0.5f * Scale.Y, -(Box.Min.Z + Box.Max.Z) * 0.5f * Scale.Z);
		Bits->AddInstance(FTransform(Rot, Where + Rot.RotateVector(Pivot), Scale));
	};
	// Instances are in cm around the node (the bits keep a unit world scale, so a tilted shard isn't sheared).
	Bits->SetWorldScale3D(FVector::OneVector);

	if (Item == TEXT("wood"))
	{
		// A standing tree like the island's own (the node's random yaw turns it); the hidden trunk collider is the target.
		Place(Fit(Main, FVector(0.f, 0.f, bTundra ? 950.f : 720.f), 30.f));
		Bits->SetStaticMesh(nullptr);
	}
	else if (Item == TEXT("stone") || bOre)
	{
		Place(Fit(Main, FVector(230.f, 200.f, 150.f), 35.f, FRotator(Rand.FRandRange(-6.f, 6.f), 0.f, Rand.FRandRange(-6.f, 6.f))));
		if (bOre)
		{
			// An ore boulder: darker rock with lumps of the metal breaking its surface (copper's green-stained lumps,
			// silvery tin, rusty iron), so it reads as ore from a distance and not as one more grey stone.
			FLinearColor RockTint(0.12f, 0.1f, 0.09f), Lump(0.5f, 0.5f, 0.5f), Lump2 = Lump;
			if (Item == TEXT("copper_ore")) { RockTint = FLinearColor(0.15f, 0.11f, 0.09f); Lump = FLinearColor(0.62f, 0.3f, 0.12f); Lump2 = FLinearColor(0.12f, 0.42f, 0.3f); }
			else if (Item == TEXT("tin_ore")) { RockTint = FLinearColor(0.16f, 0.16f, 0.17f); Lump = FLinearColor(0.72f, 0.74f, 0.78f); Lump2 = FLinearColor(0.5f, 0.52f, 0.56f); }
			else if (Item == TEXT("iron_ore")) { RockTint = FLinearColor(0.13f, 0.085f, 0.07f); Lump = FLinearColor(0.5f, 0.2f, 0.08f); Lump2 = FLinearColor(0.32f, 0.13f, 0.07f); }
			Tint(RockMaterial(Node, RockTint));
			Bits->SetStaticMesh(Shard);
			Bits->SetMaterial(0, RockMaterial(Node, Lump));
			const int32 N = 7;
			for (int32 k = 0; k < N; ++k)
			{
				const float A = (k + Rand.FRandRange(-0.3f, 0.3f)) * 2.f * PI / N;
				const float H = Rand.FRandRange(0.25f, 0.75f);
				// On the boulder's flank (its sides lean in toward the top), half buried in it.
				const float R = FMath::Lerp(0.42f, 0.24f, H);
				const FVector Where(FMath::Cos(A) * 230.f * R, FMath::Sin(A) * 200.f * R, 150.f * H - 35.f);
				const float S = Rand.FRandRange(34.f, 56.f);
				Bit(Where, FRotator(Rand.FRandRange(-40.f, 40.f), FMath::RadiansToDegrees(A), Rand.FRandRange(-40.f, 40.f)), FVector(S, S * 0.8f, S * 0.7f));
			}
			if (Item == TEXT("copper_ore"))
			{
				// Copper's second colour (malachite) on a couple of the lumps: a second set over the first.
				UInstancedStaticMeshComponent* Stain = Part<UInstancedStaticMeshComponent>(Node, Collider, TEXT("WRLookBits2"));
				Stain->ClearInstances();
				Stain->SetWorldScale3D(FVector::OneVector);
				Stain->SetStaticMesh(Shard);
				Stain->SetMaterial(0, RockMaterial(Node, Lump2));
				for (int32 k = 0; k < 3; ++k)
				{
					const float A = (k + 0.5f) * 2.f * PI / 3.f;
					const FVector Where(FMath::Cos(A) * 230.f * 0.36f, FMath::Sin(A) * 200.f * 0.36f, 150.f * 0.42f - 35.f);
					const FBox Box = Shard->GetBoundingBox();
					const FVector Scale = FVector(40.f, 34.f, 24.f) / Box.GetSize().ComponentMax(FVector(1.f));
					Stain->AddInstance(FTransform(FRotator(20.f, FMath::RadiansToDegrees(A), 10.f), Where, Scale));
				}
			}
		}
		else
		{
			Tint(RockMaterial(Node, bTundra ? FrostTint : BasaltTint));
			Bits->SetStaticMesh(nullptr);
		}
	}
	else if (Item == TEXT("sulfur_crystal"))
	{
		// A cluster of yellow sulfur crystals growing out of a low dark rock: stretched rock shards leaning outward.
		Place(Fit(Main, FVector(130.f, 115.f, 55.f), 15.f));
		Tint(RockMaterial(Node, BasaltTint));
		Bits->SetStaticMesh(Shard);
		Bits->SetMaterial(0, RockMaterial(Node, FLinearColor(0.85f, 0.66f, 0.04f)));
		const int32 N = 9;
		for (int32 k = 0; k < N; ++k)
		{
			const float A = (k + Rand.FRandRange(-0.35f, 0.35f)) * 2.f * PI / N;
			const float Lean = k == 0 ? 0.f : Rand.FRandRange(12.f, 38.f);
			const float Len = k == 0 ? 150.f : Rand.FRandRange(60.f, 125.f);
			const float W = Len * Rand.FRandRange(0.18f, 0.26f);
			const FVector Foot(FMath::Cos(A) * (k == 0 ? 0.f : 26.f), FMath::Sin(A) * (k == 0 ? 0.f : 22.f), 20.f);
			// Tip away from the middle: pitch the shard's long (Z) axis outward along A.
			const FQuat Q = FQuat(FVector(-FMath::Sin(A), FMath::Cos(A), 0.f), FMath::DegreesToRadians(Lean));
			const FVector Centre = Foot + Q.RotateVector(FVector(0.f, 0.f, Len * 0.45f));
			Bit(Centre, Q.Rotator() + FRotator(0.f, 0.f, 0.f), FVector(W, W * 0.85f, Len));
		}
	}
	else if (Item == TEXT("lichen"))
	{
		// A frosted rock crusted with pale green lichen: flat pads over its top and spreading on the snow at its foot.
		Place(Fit(Main, FVector(160.f, 140.f, 85.f), 20.f));
		Tint(RockMaterial(Node, FLinearColor(0.42f, 0.46f, 0.5f)));
		Bits->SetStaticMesh(Shard);
		Bits->SetMaterial(0, RockMaterial(Node, FLinearColor(0.36f, 0.48f, 0.22f)));
		Bit(FVector(0.f, 0.f, 60.f), FRotator(0.f, Rand.FRandRange(0.f, 360.f), 0.f), FVector(95.f, 80.f, 16.f));
		for (int32 k = 0; k < 6; ++k)
		{
			const float A = (k + Rand.FRandRange(-0.3f, 0.3f)) * 2.f * PI / 6.f;
			const float D = Rand.FRandRange(70.f, 115.f), S = Rand.FRandRange(55.f, 90.f);
			Bit(FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, 0.f), FRotator(Rand.FRandRange(-4.f, 4.f), Rand.FRandRange(0.f, 360.f), 0.f), FVector(S, S * 0.75f, 10.f));
		}
	}
	else if (Item == TEXT("berries"))
	{
		// A leafy bush hung with red berries (the dressing's shrubs give fibre; this one is the berry bush).
		Place(Fit(Main, FVector(0.f, 0.f, 125.f), 8.f));
		Bits->SetStaticMesh(Sphere);
		Bits->SetMaterial(0, ShapeColour(Node, FLinearColor(0.42f, 0.015f, 0.04f)));
		const FBox Box = Main->GetBoundingBox();
		const float Width = FMath::Max(Box.GetSize().X, Box.GetSize().Y) * (125.f / FMath::Max(1.f, Box.GetSize().Z));
		for (int32 k = 0; k < 18; ++k)
		{
			const float A = Rand.FRandRange(0.f, 2.f * PI), H = Rand.FRandRange(0.35f, 0.9f);
			const float R = Width * 0.5f * FMath::Lerp(0.85f, 0.45f, H) * Rand.FRandRange(0.75f, 1.f);
			const float S = Rand.FRandRange(8.f, 12.f);
			Bit(FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 125.f * H - 8.f), FRotator::ZeroRotator, FVector(S));
		}
	}
	else if (Item == TEXT("fiber"))
	{
		// A tall, straw-coloured fibrous plant.
		Place(Fit(Main, FVector(0.f, 0.f, 150.f), 8.f));
		Bits->SetStaticMesh(nullptr);
		for (int32 Slot = 0; Slot < Look->GetNumMaterials(); ++Slot)
		{
			if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Main->GetMaterial(Slot), Node))
			{
				// GV shrub parameters: "Color" is the bark, "Color Tint (B)" the leaves, "SSS Color Tint" their back-lit glow.
				MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.2f, 0.15f, 0.08f));
				MID->SetVectorParameterValue(TEXT("Color Tint"), FLinearColor(0.42f, 0.34f, 0.12f));
				MID->SetVectorParameterValue(TEXT("Color Tint B"), FLinearColor(0.5f, 0.4f, 0.16f));
				MID->SetVectorParameterValue(TEXT("SSS Color Tint"), FLinearColor(0.4f, 0.3f, 0.08f));
				MID->SetVectorParameterValue(TEXT("SSS Color Tint B"), FLinearColor(0.4f, 0.3f, 0.08f));
				Look->SetMaterial(Slot, MID);
			}
		}
	}
	else
	{
		Place(Fit(Main, FVector(200.f, 180.f, 130.f), 30.f));
		Tint(RockMaterial(Node, bTundra ? FrostTint : BasaltTint));
		Bits->SetStaticMesh(nullptr);
	}
	for (UActorComponent* C : Node->GetComponents())
	{
		if (UPrimitiveComponent* P = Cast<UPrimitiveComponent>(C); P && P != Collider) P->SetCullDistance(WRTune(TEXT("world.nodeCullDistance"), 30000.f));
	}
}
