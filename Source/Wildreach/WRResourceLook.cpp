#include "WRResourceLook.h"
#include "WROceanTypes.h"
#include "WRCrystalTypes.h"
#include "WRFungalTypes.h"
#include "WRDesertTypes.h"
#include "WRForestTypes.h"
#include "WRTreeKit.h"
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
	UMaterialInterface* RockMaterial(UObject* Outer, const FLinearColor& Tint, float Snow = 0.f, float Lichen = 0.f, float Dust = 0.f)
	{
		UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_Rock.M_Rock"));
		if (!Base)
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, Outer);
		MID->SetVectorParameterValue(TEXT("Tint"), Tint);
		// M_Rock's coats on the faces turned up (11-22): snow, lichen crust, settled ash.
		MID->SetScalarParameterValue(TEXT("Snow"), Snow);
		MID->SetScalarParameterValue(TEXT("Lichen"), Lichen);
		MID->SetScalarParameterValue(TEXT("Dust"), Dust);
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
		// The node's own scale sizes its collider, not the look: the look is placed in world units at the node (relative
		// locations under the scaled collider came out sunk by about the look's own height).
		Made->SetUsingAbsoluteScale(true);
		Made->SetUsingAbsoluteLocation(true);
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
		else if (ItemId == TEXT("saltpetre")) { Color = FLinearColor(0.9f, 0.9f, 0.86f); Scale = FVector(1.3f, 1.3f, 0.9f); }
		else if (ItemId == TEXT("clay")) { Color = FLinearColor(0.32f, 0.27f, 0.21f); Scale = FVector(1.8f, 1.8f, 0.5f); }
		else if (ItemId == TEXT("berries")) { Color = FLinearColor(0.45f, 0.1f, 0.25f); Scale = FVector(1.2f, 1.2f, 0.8f); }
		else if (ItemId == TEXT("fiber")) { Color = FLinearColor(0.55f, 0.6f, 0.25f); Scale = FVector(0.8f, 0.8f, 1.2f); }
		else if (ItemId == TEXT("tin_ore")) { Color = FLinearColor(0.7f, 0.72f, 0.75f); Scale = FVector(1.5f, 1.5f, 1.2f); }
		else if (ItemId == TEXT("iron_ore")) { Color = FLinearColor(0.35f, 0.2f, 0.18f); Scale = FVector(1.6f, 1.6f, 1.3f); }
		else if (ItemId == TEXT("star_iron_ore")) { Color = FLinearColor(0.15f, 0.17f, 0.25f); Scale = FVector(1.6f, 1.6f, 1.3f); }
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
	// Storm peaks, north of the island (cooks/build-biome-lightning.md): grey granite rock, mountain pine.
	const bool bStorm = At.Y < WRTune(TEXT("world.islandNorthEdge"), -201600.f);
	// The Ancient forest, south of the island (cooks/build-biome-forest.md 2.4): its wood is oak and beech from the
	// forest's own tree kit (WRTreeKit), its stone a grey boulder gone green with moss.
	const bool bForest = WRForest::Contains(At);
	const uint32 Seed = GetTypeHash(FIntVector(FMath::FloorToInt(At.X / 50.f), FMath::FloorToInt(At.Y / 50.f), 0));
	FRandomStream Rand(Seed);

	// Collider: the node's engine mesh, now a hidden cylinder about the look's footprint (cm: width, height).
	const bool bOre = Id.EndsWith(TEXT("_ore")) || Item == TEXT("rock_salt");
	// The cylinder is centred on the node's ground point, so half of it stands above the ground: at least 1.1 m, the height
	// a swing sweeps at (a 45 cm lichen collider sat under the swing on uneven cave floors and never got hit).
	FVector2D Body(110.f, 240.f);
	if (Item == TEXT("wood")) Body = FVector2D(90.f, 420.f);
	else if (Item == TEXT("stone")) Body = FVector2D(200.f, 260.f);
	else if (bOre) Body = FVector2D(190.f, 260.f);
	else if (Item == TEXT("sulfur_crystal")) Body = FVector2D(110.f, 280.f);
	else if (Item == TEXT("lichen")) Body = FVector2D(150.f, 240.f);
	else if (Item == TEXT("stormglass")) Body = FVector2D(140.f, 220.f);
	// The Corrupted wasteland's (cooks/build-biome-void.md 3b): a dead pine, a glass sheet, a salt pan, grain, a root.
	else if (Item == TEXT("greywood")) Body = FVector2D(90.f, 420.f);
	else if (Item == TEXT("voidglass")) Body = FVector2D(170.f, 230.f);
	else if (Item == TEXT("grey_salt")) Body = FVector2D(260.f, 220.f);
	else if (Item == TEXT("ashen_grain") || Item == TEXT("mandrake")) Body = FVector2D(120.f, 230.f);
	// The Graveyard's (cooks/build-biome-graveyard.md 2.9): soulstone on its bone bed, a bone heap, limestone, a
	// mortsafe over its grave (broken for grave iron), rye and asphodel stands, a wax nest, a young churchyard yew.
	else if (Item == TEXT("soulstone")) Body = FVector2D(160.f, 220.f);
	else if (Item == TEXT("bone")) Body = FVector2D(180.f, 200.f);
	else if (Item == TEXT("limestone")) Body = FVector2D(200.f, 260.f);
	else if (Item == TEXT("grave_iron")) Body = FVector2D(170.f, 240.f);
	else if (Item == TEXT("black_rye") || Item == TEXT("asphodel")) Body = FVector2D(120.f, 230.f);
	else if (Item == TEXT("wax")) Body = FVector2D(140.f, 220.f);
	else if (Item == TEXT("yew_log")) Body = FVector2D(110.f, 420.f);
	// The Crystal caves' (cooks/build-biome-crystal.md 2.7): a resonant cluster on its rind, an amethyst druse, selenite
	// stubs, doubling spar, a dust drift, an ochre floor, the dark-grown crops.
	else if (Item == TEXT("resonant_crystal")) Body = FVector2D(200.f, 300.f);
	else if (Item == TEXT("gem_amethyst") || Item == TEXT("doubling_spar")) Body = FVector2D(140.f, 200.f);
	else if (Item == TEXT("selenite")) Body = FVector2D(150.f, 220.f);
	else if (Item == TEXT("crystal_dust") || Item == TEXT("ochre")) Body = FVector2D(220.f, 200.f);
	else if (Item == TEXT("candle_rhubarb") || Item == TEXT("glowmelon")) Body = FVector2D(130.f, 230.f);
	// The Fungal caverns' (cooks/build-biome-fungal.md 2.7): a deepcap on its bones, glow-moss, a hoof fungus on its stump,
	// a rhizomorph knot, a silk bundle, a grounded puffball, a log of oyster caps, a lion's mane.
	else if (Item == TEXT("deepcap")) Body = FVector2D(130.f, 210.f);
	else if (Item == TEXT("glow_moss")) Body = FVector2D(200.f, 200.f);
	else if (Item == TEXT("amadou") || Item == TEXT("lions_mane")) Body = FVector2D(130.f, 230.f);
	else if (Item == TEXT("bootlace_rhizomorph")) Body = FVector2D(160.f, 240.f);
	else if (Item == TEXT("webspinner_silk") || Item == TEXT("spore_sac") || Item == TEXT("oyster_cap")) Body = FVector2D(140.f, 220.f);
	// The Ocean's (cooks/build-biome-ocean.md 2.5): tidecoral, sea kelp, sea grapes, oysters, urchins, salt, sea-glass,
	// driftwood, kelp ash, bronze scrap, the caves' topaz (WROceanNodes.cpp models them).
	const bool bOceanNode = WROcean::IsNodeItem(Item) && WROcean::Contains(At);
	if (bOceanNode) Body = WROcean::NodeBody(Item);
	// The Desert's (cooks/build-biome-desert.md 2.5): sunstone, sunglass, rock salt, nitre, clay, red ochre, gold flakes,
	// the umbrella thorns, date palms and prickly pear (WRDesertNodes.cpp models them).
	const bool bDesertNode = WRDesert::IsNodeItem(Item) && WRDesert::Contains(At);
	if (bDesertNode) Body = WRDesert::NodeBody(Item);
	UStaticMesh* Cylinder = LoadMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* Sphere = LoadMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (!Cylinder || !Sphere || WRTune(TEXT("world.nodeLooks"), 1.f) <= 0.f)  // nodeLooks 0: the old cones (comparisons)
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
		Main = (bTundra || bStorm) ? LoadMesh(Prop(Pine[Seed % 3][0], Pine[Seed % 3][1])) : LoadMesh(Prop(Char[Seed % 4][0], Char[Seed % 4][1]));
		if (bForest)
		{
			if (UStaticMesh* Broadleaf = WRTreeKit::Mesh(Node, Seed % 3 ? FName(TEXT("oak")) : FName(TEXT("beech")), int32(Seed % 4))) Main = Broadleaf;
		}
	}
	else if (Item == TEXT("greywood"))
	{
		// The Withered Forest's standing dead (Chernobyl's Red Forest: grey pines that never rot): the char trees, bare.
		static const TCHAR* Char[][2] = { { TEXT("CharTreeA"), TEXT("char_tree_a_textured") }, { TEXT("CharTreeB"), TEXT("char_tree_b_textured") },
			{ TEXT("CharTreeC"), TEXT("char_tree_c_textured") }, { TEXT("CharTreeD"), TEXT("char_tree_d_textured") } };
		Main = LoadMesh(Prop(Char[Seed % 4][0], Char[Seed % 4][1]));
	}
	else if (Item == TEXT("ashen_grain") || Item == TEXT("mandrake"))
	{
		Main = LoadMesh(Shrub(Item == TEXT("mandrake") ? TEXT("B") : (Seed % 2 ? TEXT("D") : TEXT("C"))));
	}
	else if (Item == TEXT("black_rye") || Item == TEXT("asphodel"))
	{
		Main = LoadMesh(Shrub(Item == TEXT("asphodel") ? TEXT("B") : (Seed % 2 ? TEXT("D") : TEXT("C"))));
	}
	else if (Item == TEXT("yew_log"))
	{
		Main = WRTreeKit::Mesh(Node, TEXT("yew"), int32(Seed % 4));
		if (!Main) Main = Boulder;
	}
	else if (Item == TEXT("wax"))
	{
		Main = Sphere;
	}
	else if (Item == TEXT("candle_rhubarb") || Item == TEXT("glowmelon"))
	{
		Main = LoadMesh(Shrub(Item == TEXT("glowmelon") ? TEXT("A") : TEXT("D")));
	}
	else if (Item == TEXT("berries") || Item == TEXT("fiber"))
	{
		// Leafy A / B for berries, the taller, twiggier C / D for fibre (E and F carry a suffix in their names).
		Main = LoadMesh(Shrub(Item == TEXT("berries") ? (Seed % 2 ? TEXT("B") : TEXT("A")) : (Seed % 2 ? TEXT("D") : TEXT("C"))));
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
	// The Fungal caverns' nodes are modelled in code (WRFungal::NodeLook): nothing of the pack look below.
	if (WRFungal::NodeLook(Node, Item, Seed)) return;
	// The Desert's nodes are modelled in code too (WRDesert::NodeLook).
	if (bDesertNode) { WRDesert::NodeLook(Node, Item, Seed); return; }

	UStaticMeshComponent* Look = Part<UStaticMeshComponent>(Node, Collider, TEXT("WRLook"));
	UInstancedStaticMeshComponent* Bits = Part<UInstancedStaticMeshComponent>(Node, Collider, TEXT("WRLookBits"));
	Bits->ClearInstances();
	Look->SetStaticMesh(Main);
	Look->SetWorldScale3D(FVector::OneVector);
	Look->SetWorldLocation(At);
	Look->SetRelativeRotation(FRotator::ZeroRotator);
	for (int32 Slot = 0; Slot < Look->GetNumMaterials(); ++Slot) Look->SetMaterial(Slot, nullptr);  // the mesh's own, unless set below
	const auto Place = [&](const FTransform& T)
	{
		Look->SetWorldLocation(At + Node->GetActorRotation().RotateVector(T.GetLocation()));
		Look->SetRelativeRotation(T.GetRotation());
		Look->SetWorldScale3D(T.GetScale3D());
	};
	const auto Tint = [&](UMaterialInterface* M) { for (int32 Slot = 0; Slot < FMath::Max(1, Look->GetNumMaterials()); ++Slot) Look->SetMaterial(Slot, M); };
	const FLinearColor BasaltTint(0.11f, 0.1f, 0.095f), FrostTint(0.62f, 0.74f, 0.88f), GraniteTint(0.42f, 0.43f, 0.45f);
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
	Bits->SetWorldLocation(At);

	if (Item == TEXT("wood"))
	{
		// A standing tree like the island's own (the node's random yaw turns it); the hidden trunk collider is the target.
		Place(Fit(Main, FVector(0.f, 0.f, bForest ? 1100.f : bStorm ? 760.f : bTundra ? 950.f : 720.f), 30.f));
		Bits->SetStaticMesh(nullptr);
	}
	else if (Item == TEXT("stone") || bOre)
	{
		Place(Fit(Main, FVector(230.f, 200.f, 150.f), 35.f, FRotator(Rand.FRandRange(-6.f, 6.f), 0.f, Rand.FRandRange(-6.f, 6.f))));
		if (bOre)
		{
			// An ore boulder: darker rock with lumps of the metal breaking its surface (copper's green-stained lumps,
			// silvery tin, rusty iron), so it reads as ore from a distance and not as one more grey stone.
			FLinearColor RockTint(0.3f, 0.27f, 0.25f), Lump(1.f, 1.f, 1.f), Lump2 = Lump;  // over 1: the rock texture is dark
			if (Item == TEXT("copper_ore")) { RockTint = FLinearColor(0.3f, 0.24f, 0.2f); Lump = FLinearColor(1.9f, 0.75f, 0.25f); Lump2 = FLinearColor(0.3f, 1.3f, 0.85f); }
			else if (Item == TEXT("tin_ore")) { RockTint = FLinearColor(0.42f, 0.43f, 0.46f); Lump = FLinearColor(2.4f, 2.45f, 2.6f); Lump2 = Lump; }
			else if (Item == TEXT("iron_ore")) { RockTint = FLinearColor(0.26f, 0.19f, 0.17f); Lump = FLinearColor(1.5f, 0.55f, 0.2f); Lump2 = Lump; }
			// Halite: pink rock salt in the Halite Cave's walls, glassy pale-pink lumps.
			else if (Item == TEXT("rock_salt")) { RockTint = FLinearColor(0.5f, 0.34f, 0.34f); Lump = FLinearColor(2.8f, 2.0f, 2.0f); Lump2 = Lump; }
			// Meteoric iron: near-black blue rock with pale silver-blue lumps that catch the lamp light.
			else if (Item == TEXT("star_iron_ore")) { RockTint = FLinearColor(0.12f, 0.13f, 0.17f); Lump = FLinearColor(2.2f, 2.4f, 2.9f); Lump2 = FLinearColor(1.2f, 1.5f, 2.4f); }
			// Silver (the Ossuary's veins): pale limestone with bright grey-white metal lumps.
			else if (Item == TEXT("silver_ore")) { RockTint = FLinearColor(0.5f, 0.49f, 0.46f); Lump = FLinearColor(3.0f, 3.05f, 3.2f); Lump2 = FLinearColor(2.2f, 2.25f, 2.4f); }
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
				Stain->SetWorldLocation(At);
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
			Tint(RockMaterial(Node, bForest ? FLinearColor(0.3f, 0.34f, 0.26f) : bStorm ? GraniteTint : bTundra ? FrostTint : BasaltTint, bTundra ? 0.8f : 0.f, bForest ? 0.6f : 0.f, !bForest && !bStorm && !bTundra ? 0.5f : 0.f));
			Bits->SetStaticMesh(nullptr);
		}
	}
	else if (Item == TEXT("sulfur_crystal"))
	{
		// A sulfur vent's crust: a low rock caked yellow, small crystals crowded on its top, and yellow crust spread on the
		// ground round it. (11-22: it was a fan of 2-3 m golden shards that read as plastic.)
		Place(Fit(Main, FVector(130.f, 115.f, 48.f), 12.f));
		Tint(RockMaterial(Node, FLinearColor(1.35f, 1.05f, 0.22f)));
		// The crystals: sulfur's double points (Tools/art/make_meshes.py), else the rock shards.
		UStaticMesh* Crystal = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Art/Gen/SM_SulfurCrystal.SM_SulfurCrystal"));
		Bits->SetStaticMesh(Crystal ? Crystal : Shard);
		Bits->SetMaterial(0, RockMaterial(Node, FLinearColor(2.6f, 2.0f, 0.12f)));  // bright sulfur yellow over the dark rock texture
		for (int32 k = 0; k < 22; ++k)
		{
			const float A = Rand.FRandRange(0.f, 2.f * PI), D = Rand.FRandRange(0.f, 42.f);
			const float Lean = Rand.FRandRange(5.f, 45.f);
			const float Len = Rand.FRandRange(24.f, 60.f), W = Len * Rand.FRandRange(0.22f, 0.32f);
			const FVector Foot(FMath::Cos(A) * D, FMath::Sin(A) * D * 0.9f, 30.f - D * 0.25f);
			const FQuat Q = FQuat(FVector(-FMath::Sin(A), FMath::Cos(A), 0.f), FMath::DegreesToRadians(Lean));
			Bit(Foot + Q.RotateVector(FVector(0.f, 0.f, Len * 0.35f)), Q.Rotator(), FVector(W, W * 0.85f, Len));
		}
		for (int32 k = 0; k < 6; ++k)
		{
			const float A = (k + Rand.FRandRange(-0.3f, 0.3f)) * 2.f * PI / 6.f, D = Rand.FRandRange(80.f, 125.f), S = Rand.FRandRange(50.f, 85.f);
			Bit(FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, -2.f), FRotator(Rand.FRandRange(-3.f, 3.f), Rand.FRandRange(0.f, 360.f), 0.f), FVector(S, S * 0.8f, 7.f));
		}
	}
	else if (Item == TEXT("greywood"))
	{
		// A withered pine: the dead tree's bark bleached to silver-grey (#A39E95), standing in its own litter; a few
		// broken limbs lie round its foot. Tinted, not swapped: the bark's grain still shows through.
		Place(Fit(Main, FVector(0.f, 0.f, 820.f + 260.f * Rand.FRand()), 30.f));
		Tint(RockMaterial(Node, FLinearColor(FColor(163, 158, 149)) * 1.6f));
		Bits->SetStaticMesh(Shard);
		Bits->SetMaterial(0, RockMaterial(Node, FLinearColor(FColor(154, 149, 140)) * 1.4f));
		for (int32 k = 0; k < 4; ++k)
		{
			const float A = Rand.FRandRange(0.f, 2.f * PI), D = Rand.FRandRange(110.f, 220.f), Len = Rand.FRandRange(90.f, 170.f);
			Bit(FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, 6.f), FRotator(88.f, FMath::RadiansToDegrees(A) + Rand.FRandRange(-40.f, 40.f), 0.f), FVector(9.f, 8.f, Len));
		}
	}
	else if (bOceanNode)
	{
		// The look is the Ocean's own code model; nothing of the boulder shows.
		Look->SetStaticMesh(nullptr);
		Bits->SetStaticMesh(nullptr);
		WROcean::NodeLook(Node, Item, Seed);
	}
	else if (Item == TEXT("resonant_crystal") || Item == TEXT("gem_amethyst") || Item == TEXT("selenite") || Item == TEXT("doubling_spar"))
	{
		// The rind the crystal grows from (a geode wall's basalt and agate, crystal 2.6) and the crystal itself, modelled
		// point by point in M_CrystalGlow so it lights by sound with its hall (WRCrystal::NodeLook).
		const bool bBig = Item == TEXT("resonant_crystal");
		Place(Fit(Main, FVector(bBig ? 210.f : 140.f, bBig ? 180.f : 120.f, bBig ? 60.f : 40.f), 20.f, FRotator(Rand.FRandRange(-5.f, 5.f), 0.f, Rand.FRandRange(-5.f, 5.f))));
		Tint(RockMaterial(Node, Item == TEXT("selenite") ? FLinearColor(FColor(64, 60, 58)) * 0.6f : FLinearColor(FColor(28, 28, 32)) * 0.7f));
		Bits->SetStaticMesh(Shard);
		// Agate bands where the rind broke: cream and rust chips round the foot.
		Bits->SetMaterial(0, RockMaterial(Node, (Seed % 2 ? FLinearColor(FColor(230, 216, 192)) : FLinearColor(FColor(168, 90, 58))) * 1.6f));
		for (int32 k = 0; k < 5; ++k)
		{
			const float A = Rand.FRandRange(0.f, 2.f * PI), D = Rand.FRandRange(bBig ? 90.f : 60.f, bBig ? 130.f : 80.f);
			Bit(FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, 8.f), FRotator(Rand.FRandRange(-20.f, 20.f), Rand.FRandRange(0.f, 360.f), 0.f), FVector(22.f, 14.f, 9.f));
		}
		WRCrystal::NodeLook(Node, Item, Seed);
	}
	else if (Item == TEXT("crystal_dust"))
	{
		// A dust drift under a lining (crystal 2.7): lilac-white powder heaped against the wall, fallen points in it.
		Place(Fit(Main, FVector(260.f, 200.f, 34.f), 12.f));
		Tint(RockMaterial(Node, FLinearColor(FColor(216, 208, 255)) * 2.4f));
		Bits->SetStaticMesh(Shard);
		Bits->SetMaterial(0, RockMaterial(Node, FLinearColor(FColor(150, 110, 210)) * 1.8f));
		for (int32 k = 0; k < 8; ++k)
		{
			const float A = Rand.FRandRange(0.f, 2.f * PI), D = Rand.FRandRange(10.f, 100.f);
			Bit(FVector(FMath::Cos(A) * D, FMath::Sin(A) * D * 0.8f, 12.f), FRotator(Rand.FRandRange(60.f, 90.f), Rand.FRandRange(0.f, 360.f), 0.f), FVector(5.f, 5.f, Rand.FRandRange(14.f, 30.f)));
		}
	}
	else if (Item == TEXT("ochre"))
	{
		// An ochre floor in a lava tube (crystal 2.7): red earth over the black glaze, scraped into ridges.
		Place(Fit(Main, FVector(280.f, 220.f, 22.f), 12.f));
		Tint(RockMaterial(Node, FLinearColor(FColor(150, 64, 36)) * 1.5f));
		Bits->SetStaticMesh(Shard);
		Bits->SetMaterial(0, RockMaterial(Node, FLinearColor(FColor(178, 86, 48)) * 1.7f));
		for (int32 k = 0; k < 6; ++k) Bit(FVector(-100.f + k * 40.f, Rand.FRandRange(-30.f, 30.f), 10.f), FRotator(0.f, Rand.FRandRange(-15.f, 15.f), 0.f), FVector(16.f, 120.f, 8.f));
	}
	else if (Item == TEXT("candle_rhubarb"))
	{
		// Candle rhubarb (survival-crafting: grown in the dark): forced stalks, crimson, with small pale-yellow leaves
		// that never greened, standing up out of the Wax Fields' dark soil.
		Place(Fit(Main, FVector(120.f, 120.f, 70.f), 4.f));
		Tint(RockMaterial(Node, FLinearColor(FColor(214, 206, 120)) * 1.6f));
		Bits->SetStaticMesh(Cylinder);
		Bits->SetMaterial(0, RockMaterial(Node, FLinearColor(FColor(176, 30, 52)) * 1.9f));
		for (int32 k = 0; k < 11; ++k)
		{
			const float A = k * 2.f * PI / 11.f + Rand.FRandRange(-0.2f, 0.2f), H = Rand.FRandRange(50.f, 95.f);
			Bit(FVector(FMath::Cos(A) * 12.f, FMath::Sin(A) * 12.f, H * 0.5f), FRotator(FMath::Cos(A) * 9.f, 0.f, FMath::Sin(A) * 9.f), FVector(4.f, 4.f, H));
		}
	}
	else if (Item == TEXT("glowmelon"))
	{
		// Glowmelons in the Steam Baths' warm dark: pale green rinds on a sprawling vine, lit faintly from inside.
		Place(Fit(Main, FVector(150.f, 150.f, 50.f), 4.f));
		Tint(RockMaterial(Node, FLinearColor(FColor(70, 96, 64))));
		Bits->SetStaticMesh(Sphere);
		Bits->SetMaterial(0, RockMaterial(Node, FLinearColor(FColor(170, 236, 170)) * 3.f));
		for (int32 k = 0; k < 4; ++k)
		{
			const float A = k * 1.57f + Rand.FRandRange(-0.4f, 0.4f), D = Rand.FRandRange(30.f, 60.f);
			Bit(FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, 14.f), FRotator(0.f, Rand.FRandRange(0.f, 360.f), 0.f), FVector(34.f, 28.f, 26.f));
		}
	}
	else if (Item == TEXT("voidglass"))
	{
		// Rift-ring voidglass (biome-void 2.6): a black fused-glass sheet poured over the ground where the tear touched, with
		// 5-9 shards standing up out of it, leaning away from the rift like frozen splashes; their broken edges catch a
		// faint violet. The sheet is near-black and glossy (#0B0A10), so it reads as wet glass beside the grey dust.
		Place(Fit(Main, FVector(210.f, 180.f, 34.f), 18.f, FRotator(Rand.FRandRange(-3.f, 3.f), 0.f, Rand.FRandRange(-3.f, 3.f))));
		Tint(RockMaterial(Node, FLinearColor(0.012f, 0.011f, 0.016f)));
		Bits->SetStaticMesh(Shard);
		Bits->SetMaterial(0, RockMaterial(Node, FLinearColor(0.03f, 0.022f, 0.06f)));
		const int32 N = 5 + int32(Seed % 5);
		for (int32 k = 0; k < N; ++k)
		{
			const float A = (k + Rand.FRandRange(-0.35f, 0.35f)) * 2.f * PI / N;
			const float Lean = Rand.FRandRange(12.f, 48.f);
			const float Len = Rand.FRandRange(45.f, 135.f);
			const float W = Len * Rand.FRandRange(0.16f, 0.26f);
			const FVector Foot(FMath::Cos(A) * Rand.FRandRange(10.f, 70.f), FMath::Sin(A) * Rand.FRandRange(10.f, 60.f), 14.f);
			const FQuat Q = FQuat(FVector(-FMath::Sin(A), FMath::Cos(A), 0.f), FMath::DegreesToRadians(Lean));
			Bit(Foot + Q.RotateVector(FVector(0.f, 0.f, Len * 0.45f)), Q.Rotator(), FVector(W, W * 0.55f, Len));
		}
	}
	else if (Item == TEXT("grey_salt"))
	{
		// A Grey Strand salt pan (biome-void 2.4): a shallow rimmed crust of grey-white salt on black sand, raked into
		// low cones along one side, a crust that cracks into plates where it's dried.
		Place(Fit(Main, FVector(300.f, 250.f, 22.f), 14.f));
		Tint(RockMaterial(Node, FLinearColor(FColor(44, 44, 48)) * 0.6f));
		Bits->SetStaticMesh(Shard);
		Bits->SetMaterial(0, RockMaterial(Node, FLinearColor(FColor(236, 234, 228)) * 2.6f));
		for (int32 k = 0; k < 14; ++k)
		{
			const float A = Rand.FRandRange(0.f, 2.f * PI), D = Rand.FRandRange(0.f, 95.f), S = Rand.FRandRange(42.f, 70.f);
			Bit(FVector(FMath::Cos(A) * D, FMath::Sin(A) * D * 0.8f, 10.f), FRotator(Rand.FRandRange(-3.f, 3.f), Rand.FRandRange(0.f, 360.f), 0.f), FVector(S, S * 0.8f, 5.f));
		}
		for (int32 k = 0; k < 3; ++k)
		{
			const float S = Rand.FRandRange(34.f, 48.f);
			Bit(FVector(-70.f + 70.f * k, 120.f, 16.f), FRotator(0.f, Rand.FRandRange(0.f, 360.f), 0.f), FVector(S, S, S * 0.7f));
		}
	}
	else if (Item == TEXT("ashen_grain") || Item == TEXT("mandrake"))
	{
		const bool bMandrake = Item == TEXT("mandrake");
		// Ashen grain: a wild stand of grey grain in a forest clearing, its heads pale ash. Mandrake: a low rosette of
		// dark leaves with the forked root's shoulders showing at the soil (it screams when pulled: WRVoidWorld).
		Place(Fit(Main, FVector(0.f, 0.f, bMandrake ? 140.f : 260.f), 8.f));
		Look->SetCastShadow(false);
		for (int32 Slot = 0; Slot < Look->GetNumMaterials(); ++Slot)
		{
			if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Main->GetMaterial(Slot), Node))
			{
				MID->SetVectorParameterValue(TEXT("Color"), bMandrake ? FLinearColor(0.035f, 0.05f, 0.03f) : FLinearColor(0.16f, 0.155f, 0.145f));
				MID->SetVectorParameterValue(TEXT("Snow Color"), bMandrake ? FLinearColor(0.05f, 0.04f, 0.05f) : FLinearColor(0.34f, 0.33f, 0.31f));
				Look->SetMaterial(Slot, MID);
			}
		}
		Bits->SetStaticMesh(Shard);
		Bits->SetMaterial(0, RockMaterial(Node, bMandrake ? FLinearColor(0.75f, 0.6f, 0.45f) : FLinearColor(1.1f, 1.08f, 1.0f)));
		if (bMandrake)
		{
			// The root's two shoulders breaking the soil, pale and wrinkled.
			Bit(FVector(-8.f, 0.f, 4.f), FRotator(20.f, 0.f, 10.f), FVector(16.f, 14.f, 20.f));
			Bit(FVector(9.f, 4.f, 3.f), FRotator(-18.f, 0.f, -8.f), FVector(13.f, 12.f, 17.f));
		}
		else
		{
			for (int32 k = 0; k < 16; ++k)
			{
				const float A = Rand.FRandRange(0.f, 2.f * PI), D = Rand.FRandRange(0.f, 45.f);
				Bit(FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, Rand.FRandRange(75.f, 105.f)), FRotator(Rand.FRandRange(-15.f, 15.f), 0.f, Rand.FRandRange(-15.f, 15.f)), FVector(4.f, 4.f, 14.f));
			}
		}
	}
	else if (Item == TEXT("stormglass"))
	{
		// Fulgurite (cooks/biome-lightning.md 2.6): a granite crust glazed where the bolt hit, with hollow glass tubes
		// standing out of it, branching like the strike's roots; fresh ones are faintly violet (the FX sparkle on top).
		Place(Fit(Main, FVector(150.f, 130.f, 40.f), 14.f));
		Tint(RockMaterial(Node, GraniteTint));
		Bits->SetStaticMesh(Shard);
		Bits->SetMaterial(0, RockMaterial(Node, FLinearColor(1.6f, 1.5f, 2.4f)));   // pale glass with a violet cast
		const int32 N = 11;
		for (int32 k = 0; k < N; ++k)
		{
			const float A = (k + Rand.FRandRange(-0.4f, 0.4f)) * 2.f * PI / N;
			const float Lean = Rand.FRandRange(8.f, 55.f);
			const float Len = Rand.FRandRange(60.f, 150.f);
			const float W = Len * Rand.FRandRange(0.1f, 0.16f);
			const FVector Foot(FMath::Cos(A) * Rand.FRandRange(10.f, 60.f), FMath::Sin(A) * Rand.FRandRange(10.f, 50.f), 18.f);
			const FQuat Q = FQuat(FVector(-FMath::Sin(A), FMath::Cos(A), 0.f), FMath::DegreesToRadians(Lean));
			Bit(Foot + Q.RotateVector(FVector(0.f, 0.f, Len * 0.45f)), Q.Rotator(), FVector(W, W, Len));
		}
	}
	else if (Item == TEXT("saltpetre"))
	{
		// Nitre: a low, dry cave rock furred with white crust (cooks/build-server-raids.md 1.4): many thin, short needles
		// lying close along its flanks and top, thickest in its hollows, never standing out like crystals.
		Place(Fit(Main, FVector(150.f, 130.f, 70.f), 14.f, FRotator(Rand.FRandRange(-5.f, 5.f), 0.f, Rand.FRandRange(-5.f, 5.f))));
		Tint(RockMaterial(Node, FLinearColor(0.16f, 0.15f, 0.15f)));
		Bits->SetStaticMesh(Shard);
		Bits->SetMaterial(0, RockMaterial(Node, FLinearColor(3.2f, 3.2f, 3.05f)));   // over 1: the rock texture is dark
		const int32 N = 26;
		for (int32 k = 0; k < N; ++k)
		{
			const float A = Rand.FRandRange(0.f, 2.f * PI);
			const float H = Rand.FRandRange(0.15f, 0.95f);
			const float Rr = FMath::Lerp(0.46f, 0.12f, H);
			const FVector Where(FMath::Cos(A) * 150.f * Rr, FMath::Sin(A) * 130.f * Rr, 70.f * H - 10.f);
			const float L = Rand.FRandRange(10.f, 22.f);
			// Lying along the surface: tipped out from the middle by how far down the flank it grows.
			Bit(Where, FRotator(FMath::Lerp(70.f, 15.f, H) + Rand.FRandRange(-12.f, 12.f), FMath::RadiansToDegrees(A), Rand.FRandRange(0.f, 180.f)), FVector(L * 0.18f, L * 0.5f, L));
		}
	}
	else if (Item == TEXT("clay"))
	{
		// A clay bank: a low, wide, smooth wet mound of grey-brown clay on the cave floor, with dug scoops and spoil lumps
		// round its foot where it's been taken before.
		Place(Fit(Main, FVector(240.f, 210.f, 55.f), 10.f, FRotator(0.f, 0.f, Rand.FRandRange(-3.f, 3.f))));
		Tint(RockMaterial(Node, FLinearColor(0.42f, 0.35f, 0.27f)));
		Bits->SetStaticMesh(Shard);
		Bits->SetMaterial(0, RockMaterial(Node, FLinearColor(0.62f, 0.52f, 0.4f)));
		for (int32 k = 0; k < 6; ++k)
		{
			const float A = (k + Rand.FRandRange(-0.3f, 0.3f)) * 2.f * PI / 6.f;
			const FVector Where(FMath::Cos(A) * 128.f, FMath::Sin(A) * 112.f, 4.f);
			const float S = Rand.FRandRange(18.f, 30.f);
			Bit(Where, FRotator(Rand.FRandRange(-10.f, 10.f), FMath::RadiansToDegrees(A), 0.f), FVector(S, S * 0.9f, S * 0.45f));
		}
	}
	else if (Item == TEXT("lichen"))
	{
		// A frosted rock crusted with lichen: pale sage blotches and orange spots in the rock's own surface (M_Rock's lichen
		// coat), a little snow on its top, a few lichened stones round its foot. (11-22: the lichen was green slabs on it.)
		Place(Fit(Main, FVector(160.f, 140.f, 85.f), 20.f));
		Tint(RockMaterial(Node, FLinearColor(0.62f, 0.64f, 0.66f), 0.35f, 1.f));
		Bits->SetStaticMesh(Shard);
		Bits->SetMaterial(0, RockMaterial(Node, FLinearColor(0.6f, 0.62f, 0.64f), 0.2f, 1.f));
		for (int32 k = 0; k < 5; ++k)
		{
			const float A = (k + Rand.FRandRange(-0.3f, 0.3f)) * 2.f * PI / 5.f;
			const float D = Rand.FRandRange(95.f, 130.f), S = Rand.FRandRange(22.f, 40.f);
			Bit(FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, 2.f), FRotator(Rand.FRandRange(-10.f, 10.f), Rand.FRandRange(0.f, 360.f), 0.f), FVector(S, S * 0.8f, S * 0.5f));
		}
	}
	else if (Item == TEXT("berries"))
	{
		// A leafy bush hung with red berries (the dressing's shrubs give fibre; this one is the berry bush).
		// GV shrub bounds are about twice their leafy part: 260 cm of bounds is a bush about a metre high.
		const float BushH = 260.f, LeafH = BushH * 0.45f;
		Place(Fit(Main, FVector(0.f, 0.f, BushH), 8.f));
		Look->SetCastShadow(false);  // like the island's shrubs: wind-blown leaf cards in the shadow maps cost far more than they show
		for (int32 Slot = 0; Slot < Look->GetNumMaterials(); ++Slot)
		{
			if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Main->GetMaterial(Slot), Node))
			{
				// No "snow" coat: a fresh green bush, the one green thing among the ashen shrubs, hung with berries.
				MID->SetScalarParameterValue(TEXT("Amount Min"), 0.f);
				MID->SetScalarParameterValue(TEXT("Amount Max"), 0.f);
				Look->SetMaterial(Slot, MID);
			}
		}
		Bits->SetStaticMesh(Sphere);
		// The rock material can draw instanced (the engine's shape material can't): a deep red over its dark texture.
		Bits->SetMaterial(0, RockMaterial(Node, FLinearColor(2.2f, 0.06f, 0.12f)));
		const FBox Box = Main->GetBoundingBox();
		const float Width = FMath::Max(Box.GetSize().X, Box.GetSize().Y) * (BushH / FMath::Max(1.f, Box.GetSize().Z)) * 0.45f;  // the leafy part
		// Bunches of three to six small berries (they were single 10-14 cm balls: apples).
		for (int32 k = 0; k < 14; ++k)
		{
			const float A = Rand.FRandRange(0.f, 2.f * PI), H = Rand.FRandRange(0.3f, 0.95f);
			const float R = Width * 0.5f * FMath::Lerp(0.9f, 0.5f, H) * Rand.FRandRange(0.8f, 1.05f);
			const FVector C(FMath::Cos(A) * R, FMath::Sin(A) * R, LeafH * H - 8.f);
			for (int32 b = 3 + Rand.RandHelper(4); b > 0; --b)
			{
				const float S = Rand.FRandRange(3.2f, 4.6f);
				Bit(C + FVector(Rand.FRandRange(-4.f, 4.f), Rand.FRandRange(-4.f, 4.f), Rand.FRandRange(-5.f, 2.f)), FRotator::ZeroRotator, FVector(S));
			}
		}
	}
	else if (Item == TEXT("fiber"))
	{
		// A tall, dry, olive-straw fibrous plant: the GV leaves' green is their texture's saturation (Sature Min..Max).
		Place(Fit(Main, FVector(0.f, 0.f, 300.f), 8.f));  // bounds: about 1.4 m of plant
		Look->SetCastShadow(false);
		Bits->SetStaticMesh(nullptr);
		for (int32 Slot = 0; Slot < Look->GetNumMaterials(); ++Slot)
		{
			if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Main->GetMaterial(Slot), Node))
			{
				MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.2f, 0.15f, 0.08f));
				// The GV leaves take their colour from a "snow" coat (Snow Color over a random share of the plant): straw.
				MID->SetVectorParameterValue(TEXT("Snow Color"), FLinearColor(0.3f, 0.24f, 0.1f));

				Look->SetMaterial(Slot, MID);
			}
		}
	}
	else if (Item == TEXT("soulstone"))
	{
		// A slumped grave's bone bed crusted with soulstone (GY 2.7; vivianite on buried bone): pale bones in dark wet
		// grave soil, clear crystals standing out of them with a pale-green glow (over 1: the rock texture is dark).
		Place(Fit(Main, FVector(190.f, 160.f, 40.f), 16.f, FRotator(Rand.FRandRange(-5.f, 5.f), 0.f, Rand.FRandRange(-5.f, 5.f))));
		Tint(RockMaterial(Node, FLinearColor(FColor(59, 53, 48)) * 0.9f));
		Bits->SetStaticMesh(Shard);
		Bits->SetMaterial(0, RockMaterial(Node, FLinearColor(1.5f, 3.2f, 2.5f)));
		for (int32 k = 0; k < 13; ++k)
		{
			const float A = (k + Rand.FRandRange(-0.4f, 0.4f)) * 2.f * PI / 13.f;
			const float Len = Rand.FRandRange(18.f, 52.f), W = Len * Rand.FRandRange(0.22f, 0.32f);
			const FQuat Q = FQuat(FVector(-FMath::Sin(A), FMath::Cos(A), 0.f), FMath::DegreesToRadians(Rand.FRandRange(5.f, 40.f)));
			Bit(FVector(FMath::Cos(A) * Rand.FRandRange(10.f, 70.f), FMath::Sin(A) * Rand.FRandRange(10.f, 60.f), 26.f) + Q.RotateVector(FVector(0.f, 0.f, Len * 0.45f)), Q.Rotator(), FVector(W, W, Len));
		}
	}
	else if (Item == TEXT("bone"))
	{
		// A heap of old bones (the Bone Pits' overflow, the Risen's leavings): a low bone-white mound, long bones on top.
		Place(Fit(Main, FVector(200.f, 170.f, 60.f), 20.f));
		Tint(RockMaterial(Node, FLinearColor(FColor(217, 207, 184)) * 1.5f));
		Bits->SetStaticMesh(Cylinder);
		Bits->SetMaterial(0, RockMaterial(Node, FLinearColor(FColor(225, 216, 196)) * 1.8f));
		for (int32 k = 0; k < 14; ++k)
		{
			const float A = Rand.FRandRange(0.f, 2.f * PI), D = Rand.FRandRange(0.f, 70.f);
			Bit(FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, 34.f - D * 0.3f), FRotator(Rand.FRandRange(70.f, 95.f), Rand.FRandRange(0.f, 360.f), 0.f), FVector(6.f, 6.f, Rand.FRandRange(30.f, 48.f)));
		}
	}
	else if (Item == TEXT("limestone"))
	{
		// White quarry stone at the Spine's foot: a pale block-jointed boulder, flakes at its foot.
		Place(Fit(Main, FVector(240.f, 200.f, 160.f), 35.f, FRotator(Rand.FRandRange(-6.f, 6.f), 0.f, Rand.FRandRange(-6.f, 6.f))));
		Tint(RockMaterial(Node, FLinearColor(FColor(180, 175, 164)) * 1.7f));
		Bits->SetStaticMesh(Shard);
		Bits->SetMaterial(0, RockMaterial(Node, FLinearColor(FColor(220, 215, 204)) * 1.9f));
		for (int32 k = 0; k < 6; ++k)
		{
			const float A = Rand.FRandRange(0.f, 2.f * PI), D = Rand.FRandRange(110.f, 170.f);
			Bit(FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, 6.f), FRotator(Rand.FRandRange(-10.f, 10.f), Rand.FRandRange(0.f, 360.f), 0.f), FVector(30.f, 22.f, 10.f));
		}
	}
	else if (Item == TEXT("grave_iron"))
	{
		// A mortsafe (GY 2.7; the resurrection men's iron cages): bars at 10 cm round a 1.9 x 0.8 x 1.1 m cage over a grave
		// slab, crossed straps over its lid. Break it for its grave iron (the node's yaw lays it along the row).
		Place(Fit(Main, FVector(200.f, 90.f, 16.f), 8.f));
		Tint(RockMaterial(Node, FLinearColor(0.32f, 0.31f, 0.3f)));
		Bits->SetStaticMesh(Cylinder);
		Bits->SetMaterial(0, RockMaterial(Node, FLinearColor(0.05f, 0.045f, 0.04f)));
		const float Lx = 190.f, Ly = 80.f, Hz = 110.f;
		for (float X = -Lx * 0.5f; X <= Lx * 0.5f + 1.f; X += 10.f) for (const float Y : { -Ly * 0.5f, Ly * 0.5f }) Bit(FVector(X, Y, Hz * 0.5f), FRotator::ZeroRotator, FVector(2.5f, 2.5f, Hz));
		for (float Y = -Ly * 0.5f + 10.f; Y < Ly * 0.5f; Y += 10.f) for (const float X : { -Lx * 0.5f, Lx * 0.5f }) Bit(FVector(X, Y, Hz * 0.5f), FRotator::ZeroRotator, FVector(2.5f, 2.5f, Hz));
		for (const float Z : { 4.f, Hz })
		{
			for (const float Y : { -Ly * 0.5f, Ly * 0.5f }) Bit(FVector(0.f, Y, Z), FRotator(90.f, 0.f, 0.f), FVector(5.f, 5.f, Lx));
			for (const float X : { -Lx * 0.5f, Lx * 0.5f }) Bit(FVector(X, 0.f, Z), FRotator(0.f, 0.f, 90.f), FVector(5.f, 5.f, Ly));
		}
		Bit(FVector(0.f, 0.f, Hz + 3.f), FRotator(90.f, 23.f, 0.f), FVector(6.f, 3.f, Lx));
		Bit(FVector(0.f, 0.f, Hz + 3.f), FRotator(90.f, -23.f, 0.f), FVector(6.f, 3.f, Lx));
	}
	else if (Item == TEXT("black_rye") || Item == TEXT("asphodel"))
	{
		// Black rye gone to ergot in the burned cottage's field (dark stems, black spurs in the heads); asphodel, the
		// pale lily of the meadows: grey-green leaves and white flower spikes.
		const bool bAsph = Item == TEXT("asphodel");
		Place(Fit(Main, FVector(0.f, 0.f, bAsph ? 160.f : 250.f), 8.f));
		Look->SetCastShadow(false);
		for (int32 Slot = 0; Slot < Look->GetNumMaterials(); ++Slot)
		{
			if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Main->GetMaterial(Slot), Node))
			{
				MID->SetVectorParameterValue(TEXT("Color"), bAsph ? FLinearColor(0.08f, 0.1f, 0.07f) : FLinearColor(0.07f, 0.06f, 0.05f));
				MID->SetVectorParameterValue(TEXT("Snow Color"), bAsph ? FLinearColor(0.16f, 0.17f, 0.14f) : FLinearColor(0.12f, 0.1f, 0.07f));
				Look->SetMaterial(Slot, MID);
			}
		}
		Bits->SetStaticMesh(Sphere);
		Bits->SetMaterial(0, RockMaterial(Node, bAsph ? FLinearColor(3.f, 2.95f, 2.8f) : FLinearColor(0.05f, 0.045f, 0.04f)));
		for (int32 k = 0; k < (bAsph ? 9 : 16); ++k)
		{
			const float A = Rand.FRandRange(0.f, 2.f * PI), D = Rand.FRandRange(0.f, 40.f);
			for (int32 f = 0; f < (bAsph ? 5 : 1); ++f) Bit(FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, (bAsph ? 70.f : 85.f) + f * 7.f + Rand.FRandRange(0.f, 20.f)), FRotator::ZeroRotator, bAsph ? FVector(5.f) : FVector(3.f, 3.f, 9.f));
		}
	}
	else if (Item == TEXT("wax"))
	{
		// A Wickbeetle nest: domed white wax crusted on the stone, wax cells clustered round it.
		Place(Fit(Main, FVector(150.f, 130.f, 70.f), 20.f));
		Tint(RockMaterial(Node, FLinearColor(FColor(232, 221, 176)) * 2.2f));
		Bits->SetStaticMesh(Sphere);
		Bits->SetMaterial(0, RockMaterial(Node, FLinearColor(FColor(240, 230, 190)) * 2.4f));
		for (int32 k = 0; k < 12; ++k)
		{
			const float A = Rand.FRandRange(0.f, 2.f * PI), D = Rand.FRandRange(50.f, 90.f);
			Bit(FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, Rand.FRandRange(4.f, 30.f)), FRotator::ZeroRotator, FVector(Rand.FRandRange(14.f, 26.f)));
		}
	}
	else if (Item == TEXT("yew_log"))
	{
		// A young churchyard yew (the tree kit's yew): cut it for yew logs; it grows back over two game days.
		Place(Fit(Main, FVector(0.f, 0.f, 900.f + 300.f * Rand.FRand()), 30.f));
		Bits->SetStaticMesh(nullptr);
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
	// One line per item kind (the first node of each): what the look came out as, for checking it from a log.
	static TSet<FName> Logged;
	if (!Logged.Contains(Item))
	{
		Logged.Add(Item);
		const FBox Box = Look->Bounds.GetBox();
		UE_LOG(LogTemp, Log, TEXT("NODELOOK %s at %s: %s, look %s (scale %s, %.0f cm tall, base %.0f cm from the node), %d bits"), *Id, *At.ToCompactString(),
			*GetNameSafe(Main), *GetNameSafe(Look->GetStaticMesh()), *Look->GetComponentScale().ToCompactString(), Box.GetSize().Z, Box.Min.Z - At.Z,
			Bits->GetInstanceCount());
	}
}
