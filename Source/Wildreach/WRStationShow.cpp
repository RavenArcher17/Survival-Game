// The workshop at work (cooks/cookpass-crafting.md 3.2): the forge, armorer, workbench and alchemy table show their work
// on every machine that draws, from the station's replicated queue, tray and worker (no server cost); the charcoal kiln
// and bloomery smoke while they burn. Their fixed kit (the forge's anvil on its stump, the alchemy table's trestle and
// mortar) is part of the station on every machine: the server collides with it too.
//  - Forge: banked coals when idle (a low breathing glow), a roaring bed when working (light, sparks, flue smoke, the
//    roar); the piece being made glowing in the coals, or on the anvil under a worker's hammer, cooling from orange to
//    cherry red over each heat and flaring at each blow; what's done waits cooled on the anvil, a quench hiss as it's done.
//  - Armorer: the plate, dome, cuff, greave, hide panel or talisman being shaped lies on the block's face; done ones wait.
//  - Workbench: the board or the tool's parts on the top; finished tools and arrows wait there.
//  - Alchemy table: steam off a warm bowl and a simmer while it brews; the mortar and pestle on the front strip; finished
//    potions stand in their vials.
#include "WRCrafting.h"
#include "WRBuildLook.h"
#include "WRCharacter.h"
#include "WRCraftWork.h"
#include "WRFx.h"
#include "WRHeldFood.h"
#include "WRInventory.h"
#include "WRResourceNode.h"
#include "WRSound.h"
#include "WRToolMesh.h"
#include "WRTuning.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"

namespace WRStationShowDetail
{
	const FLinearColor Bark(0.2f, 0.14f, 0.09f);
	const FLinearColor Sawn(0.5f, 0.37f, 0.23f);
	const FLinearColor Oak(0.38f, 0.25f, 0.13f);
	const FLinearColor AnvilIron(0.055f, 0.055f, 0.06f);
	const FLinearColor Hoop(0.09f, 0.085f, 0.08f);
	const FLinearColor Beech(0.47f, 0.31f, 0.17f);
	const FLinearColor Hide(0.3f, 0.18f, 0.09f);

	FTransform PieceFrame(const AWRStation* S)
	{
		return FTransform(S->GetActorRotation(), S->GetActorLocation() - FVector(0.f, 0.f, FWRBuildLook::ChildLift));
	}

	FVector Tuned(const FString& Key, const FVector& Default)
	{
		const FWRTuning& T = FWRTuning::Get();
		return FVector(T.Num(Key + TEXT(".x"), Default.X), T.Num(Key + TEXT(".y"), Default.Y), T.Num(Key + TEXT(".z"), Default.Z));
	}

	/** A horizontal rectangle ring at height Z (centre CX, 0), half sizes HX, HY: lofted up into blocks. */
	TArray<FVector> RingZ(float Z, float CX, float HX, float HY)
	{
		return { FVector(CX - HX, -HY, Z), FVector(CX + HX, -HY, Z), FVector(CX + HX, HY, Z), FVector(CX - HX, HY, Z) };
	}

	/** A rectangle ring across X at X (half Y, half Z round (X, 0, CZ)): lofted along X. */
	TArray<FVector> RingX(float X, float HY, float HZ, float CZ = 0.f)
	{
		return { FVector(X, -HY, CZ - HZ), FVector(X, HY, CZ - HZ), FVector(X, HY, CZ + HZ), FVector(X, -HY, CZ + HZ) };
	}

	/** The forge's anvil on its stump (origin on the ground under it): an oak stump with its bark and two iron hoops, a
	 *  London-pattern anvil bedded on its sawn top, the horn toward -X (the smith's left: they stand at +Y facing -Y),
	 *  the heel and its hardy hole toward +X, the face at 76 cm. */
	void Anvil(FWRToolMesh& M)
	{
		M.Lathe(FVector::ZeroVector, FVector::UpVector, 50.f, { FVector(0.f, 25.f, 25.f), FVector(0.1f, 22.5f, 22.5f), FVector(0.9f, 22.f, 22.f), FVector(1.f, 21.5f, 21.5f) }, 16, Bark, 0.95f, 0.f, 4.f);
		M.Lathe(FVector(0.f, 0.f, 49.6f), FVector::UpVector, 0.8f, { FVector(0.f, 21.5f, 21.5f), FVector(1.f, 21.f, 21.f) }, 16, Sawn, 0.8f);
		for (const float Z : { 10.f, 40.f })
		{
			M.Lathe(FVector(0.f, 0.f, Z), FVector::UpVector, 2.6f, { FVector(0.f, 22.9f, 22.9f), FVector(1.f, 22.9f, 22.9f) }, 16, Hoop, 0.55f);
		}
		// The body: a splayed foot, the waist, the body widening to the face; the face overhangs into the heel.
		M.Loft({ RingZ(50.f, 0.f, 17.f, 12.f), RingZ(55.f, 0.f, 14.f, 9.5f), RingZ(60.f, 0.f, 9.f, 6.f), RingZ(66.f, 1.f, 12.f, 6.5f),
			RingZ(71.f, 3.f, 17.f, 7.f), RingZ(76.f, 3.f, 17.f, 7.f) }, AnvilIron, 0.38f);
		// The heel: the face carried on past the body over the stump's edge.
		M.Loft({ RingZ(70.5f, 23.f, 4.f, 6.f), RingZ(76.f, 23.f, 4.f, 6.f) }, AnvilIron, 0.38f);
		// The horn: round, tapering to a point toward -X, its top a little under the face.
		M.Lathe(FVector(-14.f, 0.f, 72.5f), -FVector::ForwardVector, 20.f, { FVector(0.f, 6.f, 3.6f), FVector(0.35f, 4.6f, 3.1f), FVector(0.75f, 2.2f, 1.8f), FVector(1.f, 0.4f, 0.4f) }, 12, AnvilIron, 0.38f, -2.f);
		// The hardy and pritchel holes in the heel (dark in the face).
		M.Loft({ RingZ(76.05f, 22.f, 1.2f, 1.2f), RingZ(76.15f, 22.f, 1.2f, 1.2f) }, FLinearColor(0.01f, 0.01f, 0.01f), 1.f);
		M.Lathe(FVector(16.f, 0.f, 76.05f), FVector::UpVector, 0.1f, { FVector(0.f, 0.6f, 0.6f), FVector(1.f, 0.6f, 0.6f) }, 8, FLinearColor(0.01f, 0.01f, 0.01f), 1.f);
	}

	/** The alchemy table's trestle (origin on the ground at its centre): an oak top 108 x 98, 4 cm thick at TopZ, on two
	 *  A-trestles of turned legs with a cross bar under the top and a foot rail, a stretcher pinned between them; and the
	 *  apothecary's stone mortar on the tray's free front strip (its pestle is drawn apart: a worker holds it). */
	void Trestle(FWRToolMesh& M, float TopZ, const FVector& Mortar)
	{
		M.Loft({ RingZ(TopZ - 4.f, 0.f, 54.f, 49.f), RingZ(TopZ, 0.f, 54.f, 49.f) }, Oak, 0.75f);
		for (const float X : { -40.f, 40.f })
		{
			for (const float Y : { -1.f, 1.f })
			{
				M.Tube({ FVector(X, Y * 40.f, 0.f), FVector(X, Y * 28.f, (TopZ - 4.f) * 0.5f), FVector(X, Y * 16.f, TopZ - 4.f) }, 2.6f, 7, Oak * 0.9f, 0.8f);
			}
			M.Loft({ RingX(X - 2.5f, 26.f, 2.5f, TopZ - 10.f), RingX(X + 2.5f, 26.f, 2.5f, TopZ - 10.f) }, Oak * 0.85f, 0.8f);
			M.Loft({ RingX(X - 2.5f, 41.f, 2.f, 8.f), RingX(X + 2.5f, 41.f, 2.f, 8.f) }, Oak * 0.85f, 0.8f);
		}
		M.Loft({ { FVector(-42.f, -2.5f, 27.f), FVector(-42.f, 2.5f, 27.f), FVector(-42.f, 2.5f, 32.f), FVector(-42.f, -2.5f, 32.f) },
			{ FVector(42.f, -2.5f, 27.f), FVector(42.f, 2.5f, 27.f), FVector(42.f, 2.5f, 32.f), FVector(42.f, -2.5f, 32.f) } }, Oak * 0.85f, 0.8f);
		// The mortar: a turned grey stone bowl, its dark hollow, the pestle leaning in it.
		M.Lathe(Mortar, FVector::UpVector, 9.f, { FVector(0.f, 6.f, 6.f), FVector(0.2f, 7.2f, 7.2f), FVector(0.85f, 8.f, 8.f), FVector(1.f, 7.6f, 7.6f) }, 14, FLinearColor(0.46f, 0.44f, 0.4f), 0.85f);
		M.Lathe(Mortar + FVector(0.f, 0.f, 8.95f), FVector::UpVector, 0.1f, { FVector(0.f, 6.2f, 6.2f), FVector(1.f, 6.2f, 6.2f) }, 14, FLinearColor(0.12f, 0.11f, 0.1f), 1.f);
	}

	FLinearColor MetalColour(FName Metal)
	{
		if (Metal == TEXT("bronze")) return FLinearColor(0.42f, 0.27f, 0.1f);
		if (Metal == TEXT("iron")) return FLinearColor(0.16f, 0.16f, 0.17f);
		if (Metal == TEXT("steel")) return FLinearColor(0.3f, 0.31f, 0.33f);
		if (Metal == TEXT("starsteel")) return FLinearColor(0.06f, 0.07f, 0.11f);
		if (Metal == TEXT("copper")) return FLinearColor(0.45f, 0.18f, 0.08f);
		return FLinearColor(0.2f, 0.2f, 0.21f);
	}

	/** What a thing is made of, from its metal or its id's first word (hide_, copper_, ... ; sets are steel, ascendant
	 *  pieces starsteel). */
	FName MaterialOf(const FWRItemDef* D)
	{
		if (!D) return NAME_None;
		if (!D->Metal.IsNone()) return D->Metal;
		const FString Id = D->Id.ToString();
		for (const TCHAR* W : { TEXT("hide"), TEXT("copper"), TEXT("bronze"), TEXT("iron"), TEXT("steel"), TEXT("starsteel"), TEXT("wool"), TEXT("linen") })
		{
			if (Id.StartsWith(W)) return W;
		}
		if (Id.Contains(TEXT("ascendant"))) return TEXT("starsteel");
		if (!D->Set.IsNone()) return TEXT("steel");
		return NAME_None;
	}


	/** Adds T to M moved by At (and turned by Turn). */
	void Append(FWRToolMesh& M, const FWRToolMesh& T, const FVector& At, const FQuat& Turn = FQuat::Identity)
	{
		const int32 Base = M.Verts.Num();
		for (int32 i = 0; i < T.Verts.Num(); ++i)
		{
			M.Verts.Add(At + Turn.RotateVector(T.Verts[i]));
			M.Normals.Add(T.Normals.IsValidIndex(i) ? Turn.RotateVector(T.Normals[i]) : FVector::UpVector);
		}
		for (const int32 I : T.Tris) M.Tris.Add(Base + I);
		M.UVs.Append(T.UVs);
		M.Colors.Append(T.Colors);
	}

	/** The piece being made, or one finished, lying flat on its work surface (built round the origin on the surface,
	 *  along X, then moved to At). */
	void BuildPiece(FWRToolMesh& Out, FName Item, FName StationType, const FVector& At, float S = 1.f)
	{
		const FWRItemDef* D = FWRItemRegistry::Get().Find(Item);
		const FName Mat = MaterialOf(D);
		const bool bHide = Mat == TEXT("hide") || Mat == TEXT("wool") || Mat == TEXT("linen");
		const FLinearColor C = bHide ? Hide : MetalColour(Mat);
		const float Rough = bHide ? 0.85f : 0.45f;
		const FString Id = Item.ToString();
		FWRToolMesh M;
		if (StationType == TEXT("alchemy_table"))
		{
			FWRFoodLook::Build(M, FWRFoodLook::For(Item), FVector::ZeroVector, 1.f, 0.9f * S);
		}
		else if (D && (D->Slot == EWREquipSlot::Weapon || D->Slot == EWREquipSlot::Ranged) && !D->WeaponClass.IsNone() && FWRGathering::FindTool(Item) == nullptr)
		{
			const FName Cls = D->WeaponClass;
			if (Cls == TEXT("sword") || Cls == TEXT("dagger") || Cls == TEXT("greatsword"))
			{
				// A blade blank: the tang, the shoulders, the blade tapering to its point, flat on the face.
				const float L = (Cls == TEXT("dagger") ? 22.f : Cls == TEXT("greatsword") ? 48.f : 34.f) * S;
				const float W = (Cls == TEXT("greatsword") ? 2.6f : 2.1f) * S;
				const float H = 0.35f * S, X0 = -L * 0.5f;
				M.Loft({ RingX(X0, 0.5f * S, H, H), RingX(X0 + 7.f * S, 0.7f * S, H, H), RingX(X0 + 7.5f * S, W, H, H), RingX(X0 + L * 0.75f, W * 0.85f, H * 0.85f, H),
					RingX(X0 + L, 0.15f * S, 0.15f * S, H) }, C, 0.5f);
			}
			else if (Cls == TEXT("spear"))
			{
				// The socket and the leaf of a spearhead.
				M.Lathe(FVector(-9.f * S, 0.f, 1.2f * S), FVector::ForwardVector, 18.f * S, { FVector(0.f, 1.3f * S, 1.3f * S), FVector(0.3f, 1.1f * S, 1.1f * S), FVector(0.45f, 2.6f * S, 0.5f * S), FVector(1.f, 0.1f * S, 0.1f * S) }, 8, C, 0.5f);
			}
			else if (Cls == TEXT("scythe"))
			{
				TArray<FVector> P;
				for (int32 i = 0; i <= 10; ++i)
				{
					const float A = FMath::DegreesToRadians(-60.f + 70.f * i / 10.f);
					P.Add(FVector(FMath::Cos(A) * 28.f * S - 14.f * S, FMath::Sin(A) * 28.f * S + 14.f * S, 0.5f * S));
				}
				M.Tube(P, 1.1f * S, 4, C, 0.5f);
			}
			else
			{
				// Hammers, axes, the rest: a forged head block.
				M.Loft({ RingX(-6.f * S, 2.6f * S, 2.6f * S, 2.6f * S), RingX(6.f * S, 2.8f * S, 2.8f * S, 2.8f * S) }, C, 0.5f);
			}
		}
		else if (D && D->Slot == EWREquipSlot::Head)
		{
			M.Lathe(FVector::ZeroVector, FVector::UpVector, 9.f * S, { FVector(0.f, 11.f * S, 10.f * S), FVector(0.4f, 10.f * S, 9.f * S), FVector(0.8f, 6.f * S, 5.5f * S), FVector(1.f, 0.5f * S, 0.5f * S) }, 16, C, Rough);
		}
		else if (D && D->Slot == EWREquipSlot::Hands)
		{
			M.Lathe(FVector(-8.f * S, 0.f, 4.5f * S), FVector::ForwardVector, 16.f * S, { FVector(0.f, 4.f * S, 4.f * S), FVector(0.6f, 4.6f * S, 4.4f * S), FVector(1.f, 5.6f * S, 5.2f * S) }, 12, C, Rough);
		}
		else if (D && D->Slot == EWREquipSlot::Legs)
		{
			M.Lathe(FVector(-15.f * S, 0.f, 3.f * S), FVector::ForwardVector, 30.f * S, { FVector(0.f, 4.5f * S, 3.f * S), FVector(0.3f, 6.f * S, 3.6f * S), FVector(0.7f, 5.4f * S, 3.3f * S), FVector(1.f, 4.f * S, 2.6f * S) }, 12, C, Rough);
		}
		else if (D && WRGearSlots::IsArmour(D->Slot))
		{
			// A dished breastplate blank (or a hide panel), lying on its back.
			M.Lathe(FVector(-13.f * S, 0.f, 0.f), FVector::ForwardVector, 26.f * S, { FVector(0.f, 9.f * S, 1.2f * S), FVector(0.25f, 13.f * S, 3.4f * S), FVector(0.6f, 14.f * S, 4.2f * S), FVector(1.f, 10.f * S, 1.6f * S) }, 16, C, Rough);
		}
		else if (D && D->Slot == EWREquipSlot::Talisman)
		{
			M.Lathe(FVector::ZeroVector, FVector::UpVector, 0.8f * S, { FVector(0.f, 3.2f * S, 3.2f * S), FVector(1.f, 3.f * S, 3.f * S) }, 14, FLinearColor(0.36f, 0.25f, 0.1f), 0.5f);
			TArray<FVector> Cord;
			for (int32 i = 0; i <= 16; ++i) { const float A = 2.f * PI * i / 16.f; Cord.Add(FVector(FMath::Cos(A) * 7.f * S - 6.f * S, FMath::Sin(A) * 4.f * S, 0.4f * S)); }
			M.Tube(Cord, 0.25f * S, 4, FLinearColor(0.22f, 0.15f, 0.08f), 0.9f, true);
		}
		else if (D && WRGearSlots::IsRing(D->Slot))
		{
			TArray<FVector> Band;
			for (int32 i = 0; i <= 16; ++i) { const float A = 2.f * PI * i / 16.f; Band.Add(FVector(FMath::Cos(A) * 1.2f * S, FMath::Sin(A) * 1.2f * S, 0.35f * S)); }
			M.Tube(Band, 0.35f * S, 6, C, 0.4f, true);
		}
		else if (const FWRToolDef* Tool = FWRGathering::FindTool(Item))
		{
			// A tool laid on the bench: its own modelled head and haft, on its side.
			const FWRToolMesh T = FWRToolMesh::Tool(Tool->Kind, Id.StartsWith(TEXT("crude")), Tool->Head, 44.f * S, 6.f * S, 3.f * S, 0.8f * S);
			Append(M, T, FVector(-20.f * S, 0.f, 3.f * S), FQuat(FVector::ForwardVector, FMath::DegreesToRadians(90.f)));
		}
		else if (Id.Contains(TEXT("arrow")) || Id.Contains(TEXT("bolt")))
		{
			for (int32 i = 0; i < 3; ++i)
			{
				const FVector O(-16.f * S, (i - 1) * 2.4f * S, 0.6f * S);
				M.Lathe(O, FVector::ForwardVector, 32.f * S, { FVector(0.f, 0.45f * S, 0.45f * S), FVector(1.f, 0.45f * S, 0.45f * S) }, 6, Beech, 0.7f);
				M.Lathe(O + FVector(32.f * S, 0.f, 0.f), FVector::ForwardVector, 3.f * S, { FVector(0.f, 0.9f * S, 0.25f * S), FVector(1.f, 0.05f * S, 0.05f * S) }, 6, FLinearColor(0.12f, 0.12f, 0.13f), 0.5f);
			}
		}
		else if (Id.Contains(TEXT("gear")))
		{
			M.Lathe(FVector::ZeroVector, FVector::UpVector, 1.6f * S, { FVector(0.f, 5.f * S, 5.f * S), FVector(1.f, 5.f * S, 5.f * S) }, 20, C, 0.45f);
			for (int32 i = 0; i < 10; ++i)
			{
				const float A = 2.f * PI * i / 10.f;
				const FVector Dir(FMath::Cos(A), FMath::Sin(A), 0.f);
				M.Lathe(Dir * 4.6f * S + FVector(0.f, 0.f, 0.8f * S), Dir, 1.6f * S, { FVector(0.f, 0.9f * S, 0.8f * S), FVector(1.f, 0.6f * S, 0.8f * S) }, 4, C, 0.45f);
			}
		}
		else if (StationType == TEXT("workbench"))
		{
			// A beech board blank, 40 x 12 x 3.
			M.Loft({ RingX(-20.f * S, 6.f * S, 1.5f * S, 1.5f * S), RingX(20.f * S, 6.f * S, 1.5f * S, 1.5f * S) }, Beech, 0.7f);
		}
		else
		{
			// An ingot or bar: tapered, as cast in its mould.
			M.Loft({ RingZ(0.f, 0.f, 5.f * S, 2.2f * S), RingZ(2.6f * S, 0.f, 4.3f * S, 1.6f * S) }, C, 0.45f);
		}
		Append(Out, M, At);
	}

	UProceduralMeshComponent* Make(AWRStation* S, const TCHAR* Name)
	{
		UProceduralMeshComponent* C = NewObject<UProceduralMeshComponent>(S, Name);
		C->SetupAttachment(S->GetRootComponent());
		C->RegisterComponent();
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCanEverAffectNavigation(false);
		C->SetUsingAbsoluteLocation(true);
		C->SetUsingAbsoluteRotation(true);
		C->SetUsingAbsoluteScale(true);
		return C;
	}

	USceneComponent* Anchor(AWRStation* S, const TCHAR* Name)
	{
		USceneComponent* A = NewObject<USceneComponent>(S, Name);
		A->SetupAttachment(S->GetRootComponent());
		A->SetUsingAbsoluteLocation(true);
		A->SetUsingAbsoluteRotation(true);
		A->SetUsingAbsoluteScale(true);
		A->RegisterComponent();
		return A;
	}

	UBoxComponent* Box(AWRStation* S, const TCHAR* Name, const FTransform& Frame, const FVector& Centre, const FVector& Half)
	{
		UBoxComponent* B = NewObject<UBoxComponent>(S, Name);
		B->SetupAttachment(S->GetRootComponent());
		B->SetUsingAbsoluteLocation(true);
		B->SetUsingAbsoluteRotation(true);
		B->SetUsingAbsoluteScale(true);
		B->RegisterComponent();
		B->SetBoxExtent(Half, false);
		B->SetWorldLocationAndRotation(Frame.TransformPosition(Centre), Frame.GetRotation());
		B->SetCollisionProfileName(TEXT("BlockAll"));
		B->SetCanEverAffectNavigation(true);
		B->SetHiddenInGame(true);
		return B;
	}

	/** The piece's spot (station frame, cm): the forge's coals or its anvil's face, the block, the bench, the mortar. */
	FVector PieceSpot(FName Type, bool bWorked)
	{
		if (Type == TEXT("forge")) return bWorked ? Tuned(TEXT("craft.show.forge.anvilPiece"), FVector(-2.f, 118.f, 76.5f)) : Tuned(TEXT("craft.show.forge.coals"), FVector(0.f, 6.f, 20.f));
		if (Type == TEXT("armorer")) return Tuned(TEXT("craft.show.armorer.piece"), FVector(12.f, -14.f, 80.5f));
		if (Type == TEXT("workbench")) return Tuned(TEXT("craft.show.workbench.piece"), FVector(-14.f, -6.f, 81.5f));
		return Tuned(TEXT("craft.show.alchemy.mortar"), FVector(-44.f, -10.f, 77.f)) + FVector(0.f, 0.f, 8.f);
	}

	/** Where what's done waits (station frame), the i-th of them. */
	FVector DoneSpot(FName Type, int32 i)
	{
		if (Type == TEXT("forge")) return Tuned(TEXT("craft.show.forge.done"), FVector(12.f, 118.f, 76.5f)) + FVector(0.f, i * 5.f, 0.4f * i);
		if (Type == TEXT("armorer")) return Tuned(TEXT("craft.show.armorer.done"), FVector(-10.f, 12.f, 80.5f)) + FVector(0.f, 0.f, 3.f * i);
		if (Type == TEXT("workbench")) return Tuned(TEXT("craft.show.workbench.done"), FVector(28.f, 4.f, 81.5f)) + FVector(0.f, i * 9.f, 0.f);
		return Tuned(TEXT("craft.show.alchemy.done"), FVector(-44.f, 8.f, 77.f)) + FVector(0.f, i * 9.f, 0.f);
	}

	FName MakingNow(const AWRStation* S)
	{
		const FWRRecipe* R = S->Queue.Num() > 0 ? FWRRecipeRegistry::Get().Find(S->Queue[0].Recipe) : nullptr;
		return R ? R->Output : NAME_None;
	}
}

bool AWRStation::IsWorkShow() const
{
	return StationType == TEXT("forge") || StationType == TEXT("armorer") || StationType == TEXT("workbench") || StationType == TEXT("alchemy_table")
		|| StationType == TEXT("charcoal_kiln") || StationType == TEXT("bloomery");
}

FVector AWRStation::WorkPieceAt() const
{
	using namespace WRStationShowDetail;
	return PieceFrame(this).TransformPosition(PieceSpot(StationType, WorkerRep != nullptr));
}

void AWRStation::BuildWorkFixtures()
{
	using namespace WRStationShowDetail;
	if (FixtureFor == StationType)
	{
		return;
	}
	FixtureFor = StationType;
	if (FixtureBox) { FixtureBox->DestroyComponent(); FixtureBox = nullptr; }
	if (FixtureBox2) { FixtureBox2->DestroyComponent(); FixtureBox2 = nullptr; }
	if (FixtureMesh) { FixtureMesh->DestroyComponent(); FixtureMesh = nullptr; }
	const FTransform Frame = PieceFrame(this);
	const bool bDraw = GetNetMode() != NM_DedicatedServer;
	if (StationType == TEXT("forge"))
	{
		const FVector At = Tuned(TEXT("craft.show.forge.anvil"), FVector(0.f, 118.f, 0.f));
		// The stump and anvil collide as one block (the smith walks round it).
		FixtureBox = Box(this, TEXT("AnvilBox"), Frame, At + FVector(0.f, 0.f, 38.f), FVector(32.f, 22.f, 38.f));
		if (bDraw)
		{
			FixtureMesh = Make(this, TEXT("ForgeAnvil"));
			FWRToolMesh M;
			Anvil(M);
			M.ApplyTo(FixtureMesh);
			FixtureMesh->SetWorldLocationAndRotation(Frame.TransformPosition(At), Frame.GetRotation());
		}
	}
	else if (StationType == TEXT("alchemy_table"))
	{
		const float Top = WRTune(TEXT("craft.show.alchemy.tableTop"), 70.f);
		const FVector Mortar = Tuned(TEXT("craft.show.alchemy.mortar"), FVector(-44.f, -10.f, 77.f));
		FixtureBox = Box(this, TEXT("TrestleBox"), Frame, FVector(0.f, 0.f, Top * 0.5f), FVector(54.f, 49.f, Top * 0.5f));
		if (bDraw)
		{
			FixtureMesh = Make(this, TEXT("AlchemyTrestle"));
			FWRToolMesh M;
			Trestle(M, Top, Mortar);
			M.ApplyTo(FixtureMesh);
			FixtureMesh->SetWorldLocationAndRotation(Frame.GetLocation(), Frame.GetRotation());
		}
	}
}

void AWRStation::TickWorkShow(float DeltaSeconds)
{
	using namespace WRStationShowDetail;
	const FWRTuning& T = FWRTuning::Get();
	const UWorld* World = GetWorld();
	const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
	const float Range = T.Num(TEXT("craft.show.range"), 4000.f);
	const bool bNear = Cam && FVector::DistSquared(Cam->GetCameraLocation(), GetActorLocation()) < Range * Range;
	SetActorTickInterval(bNear ? 0.f : 0.5f);
	const AWRHearth* Hearth = AWRHearth::FindCovering(World, GetActorLocation());
	const bool bCold = Hearth && Hearth->IsBroken();
	const bool bWorking = Queue.Num() > 0 && !bCold;
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	const FTransform Frame = PieceFrame(this);
	const bool bForge = StationType == TEXT("forge");
	const bool bAlchemy = StationType == TEXT("alchemy_table");
	const bool bSmoker = StationType == TEXT("charcoal_kiln") || StationType == TEXT("bloomery");

	// ---- The forge's fire: banked coals idle, a roaring bed at work, cold when its Hearth is broken.
	if (bForge && Glow)
	{
		if (GlowBase < 0.f) GlowBase = Glow->Intensity;
		Glow->SetWorldLocation(Frame.TransformPosition(Tuned(TEXT("craft.show.forge.light"), FVector(0.f, 22.f, 30.f))));
		float Level = 0.f;
		if (!bCold)
		{
			const float Breath = 0.5f + 0.5f * FMath::Sin(Now * 2.0 * PI * T.Num(TEXT("craft.show.forge.idleHz"), 0.3f));
			const float Flicker = 0.82f + 0.1f * FMath::Sin(Now * 13.1) + 0.08f * FMath::Sin(Now * 7.7 + 1.3);
			Level = bWorking ? T.Num(TEXT("craft.show.forge.full"), 1.f) * Flicker : T.Num(TEXT("craft.show.forge.idle"), 0.18f) * (0.8f + 0.2f * Breath);
		}
		Glow->SetVisibility(Level > 0.01f);
		Glow->SetIntensity(GlowBase * Level);
	}

	// ---- Fire, smoke and steam at work: the forge's bed, flue and roar; the kiln's and bloomery's tops; the alchemy
	// table's warm bowl and its simmer.
	const bool bFx = bWorking && bNear && (bForge || bSmoker || bAlchemy);
	if (bFx != bWorkShowOn)
	{
		bWorkShowOn = bFx;
		if (bFx)
		{
			if (!ShowSmoke) ShowSmoke = Anchor(this, TEXT("WorkSmoke"));
			FVector Where;
			if (bForge) Where = Tuned(TEXT("craft.show.forge.flue"), FVector(-9.5f, -9.f, 129.f));
			else if (bAlchemy) Where = Tuned(TEXT("craft.show.alchemy.steam"), FVector(-24.f, -26.f, 89.f));
			else Where = FVector(T.Num(FString::Printf(TEXT("farm.show.smoke.%s.x"), *StationType.ToString()), 0.f), 0.f,
				T.Num(FString::Printf(TEXT("farm.show.smoke.%s.z"), *StationType.ToString()), StationType == TEXT("bloomery") ? 180.f : 188.f));
			ShowSmoke->SetWorldLocation(Frame.TransformPosition(Where));
			ShowSmokeRun = WRFx::Attach(ShowSmoke, bAlchemy ? FName(TEXT("alchemy.steam")) : FName(TEXT("kitchen.smoke")));
			if (bForge)
			{
				if (!ShowBed) ShowBed = Anchor(this, TEXT("ForgeBed"));
				ShowBed->SetWorldLocation(Frame.TransformPosition(Tuned(TEXT("craft.show.forge.bed"), FVector(0.f, 10.f, 22.f))));
				ShowBedRun = WRFx::Attach(ShowBed, TEXT("forge.bed"));
			}
			if (!ShowRoar && (bForge || bAlchemy)) ShowRoar = WRSound::StartLoop(this, bForge ? FName(TEXT("forge.roar")) : FName(TEXT("cook.bubble")));
		}
		else
		{
			if (ShowSmokeRun) { WRFx::Stop(ShowSmokeRun); ShowSmokeRun = nullptr; }
			if (ShowBedRun) { WRFx::Stop(ShowBedRun); ShowBedRun = nullptr; }
			if (ShowRoar) { ShowRoar->FadeOut(0.8f, 0.f); ShowRoar = nullptr; }
		}
	}
	if (bSmoker)
	{
		return;
	}

	// ---- The piece being made (hot on the forge), where it lies now.
	const FName Item = bWorking && bNear ? MakingNow(this) : FName();
	const bool bWorked = WorkerRep != nullptr;
	if (!Item.IsNone() && !bAlchemy)
	{
		if (!ShowPiece) ShowPiece = Make(this, TEXT("WorkPiece"));
		if (ShowPieceFor != Item || bShowPieceHot != bForge)
		{
			ShowPieceFor = Item;
			bShowPieceHot = bForge;
			FWRToolMesh M;
			BuildPiece(M, Item, StationType, FVector::ZeroVector);
			M.ApplyTo(ShowPiece);
			if (bForge)
			{
				if (!ShowPieceGlow)
				{
					if (UMaterialInterface* G = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Creatures/M_Glow.M_Glow"))) ShowPieceGlow = UMaterialInstanceDynamic::Create(G, this);
				}
				if (ShowPieceGlow) ShowPiece->SetMaterial(0, ShowPieceGlow);
			}
		}
		ShowPiece->SetWorldLocationAndRotation(Frame.TransformPosition(PieceSpot(StationType, bWorked)), Frame.GetRotation());
		ShowPiece->SetVisibility(true);
		if (bForge && ShowPieceGlow)
		{
			// Each heat: out of the coals yellow-orange, cooling to cherry red under the hammer; a flare at each blow. In the
			// coals with nobody at it: soaking at a steady orange.
			const float HeatSeconds = FMath::Max(1.f, T.Num(TEXT("craft.show.forge.heatSeconds"), 6.f));
			const float Heat = bWorked ? 1.f - FMath::Frac(static_cast<float>(Now / HeatSeconds)) * 0.75f : 0.85f;
			const float Flare = FMath::Clamp(1.f - static_cast<float>(Now - FlareAt) / 0.15f, 0.f, 1.f);
			const FLinearColor Hot = FMath::Lerp(FLinearColor(0.8f, 0.1f, 0.02f), FLinearColor(1.f, 0.58f, 0.16f), Heat);
			ShowPieceGlow->SetVectorParameterValue(TEXT("Color"), Hot);
			ShowPieceGlow->SetScalarParameterValue(TEXT("Glow"), T.Num(TEXT("craft.show.forge.glow"), 6.f) * (0.45f + 0.55f * Heat) * (1.f + 0.6f * Flare));
		}
	}
	else if (bAlchemy && bNear && !bWorked)
	{
		// The pestle leaning in the mortar against its far lip while nobody grinds (a worker holds it).
		if (!ShowPiece) ShowPiece = Make(this, TEXT("WorkPiece"));
		if (ShowPieceFor != TEXT("pestle"))
		{
			ShowPieceFor = TEXT("pestle");
			FWRToolMesh M;
			M.Lathe(FVector(-1.f, 0.f, 3.f), FVector(0.45f, 0.2f, 0.87f).GetSafeNormal(), 20.f, { FVector(0.f, 2.4f, 2.4f), FVector(0.15f, 2.6f, 2.6f), FVector(0.5f, 1.5f, 1.5f), FVector(1.f, 1.2f, 1.2f) }, 12, FLinearColor(0.42f, 0.4f, 0.37f), 0.85f);
			M.ApplyTo(ShowPiece);
		}
		ShowPiece->SetWorldLocationAndRotation(Frame.TransformPosition(Tuned(TEXT("craft.show.alchemy.mortar"), FVector(-44.f, -10.f, 77.f))), Frame.GetRotation());
		ShowPiece->SetVisibility(true);
	}
	else if (ShowPiece && ShowPiece->IsVisible())
	{
		ShowPiece->SetVisibility(false);
	}

	// ---- What's done waits on the work surface, cooled (up to 2; potions 3 in their vials).
	const FWRItemStack* First = nullptr;
	for (const FWRItemStack& Sk : Output) if (!Sk.IsEmpty()) { First = &Sk; break; }
	const int32 Count = First && bNear ? FMath::Min(First->Count, bAlchemy ? 3 : 2) : 0;
	if (Count > 0)
	{
		if (!ShowDone) ShowDone = Make(this, TEXT("DoneGoods"));
		if (ShowDoneFor != First->ItemId || ShowDoneCount != Count)
		{
			ShowDoneFor = First->ItemId;
			ShowDoneCount = Count;
			FWRToolMesh M;
			for (int32 i = 0; i < Count; ++i) BuildPiece(M, First->ItemId, StationType, DoneSpot(StationType, i), bAlchemy ? 1.f : 0.9f);
			M.ApplyTo(ShowDone);
		}
		ShowDone->SetWorldLocationAndRotation(Frame.GetLocation(), Frame.GetRotation());
		ShowDone->SetVisibility(true);
	}
	else if (ShowDone && ShowDone->IsVisible())
	{
		ShowDone->SetVisibility(false);
		ShowDoneFor = NAME_None;
	}

	// ---- One finished on the forge (its order's count went up, or the order is gone): quenched with a hiss and steam.
	const int32 Made = Queue.Num() > 0 ? Queue[0].Made : -1;
	if (bForge && bNear && ShowDoneMade >= 0 && Made != ShowDoneMade)
	{
		const FVector Spot = WorkPieceAt();
		WRSound::PlayAt(this, TEXT("craft.quench"), Spot);
		WRFx::SpawnAt(this, TEXT("craft.quench"), Spot);
	}
	ShowDoneMade = Made;
}
