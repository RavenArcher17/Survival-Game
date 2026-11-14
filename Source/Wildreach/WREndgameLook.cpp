// How the endgame looks (cooks/build-endgame.md 2.8, 2.9).
//  - A boss monument: a granite plinth in two courses with corner pilasters and a front niche under a round arch (the
//    Roquepertuse sanctuary kept heads in niches), the boss's sigil cut into both flanks, a cushion stone on top with a
//    bronze collar, and in the collar the boss's real head: its own skeletal mesh, posed so every bone but the head and
//    what hangs off it folds into the neck (a poseable mesh, component-space bone transforms set root to leaves), in
//    its own materials. In the niche a bronze tabula ansata (the dovetail-eared Roman plaque) with the boss's name,
//    the owner's best kill time and its names, and the highest Artifact Level; at AL 15 a knot border runs round it.
//  - A banner's sigil: a dark woven square over the plain emblem with the sigil's cells woven in pale linen.
//  - The hero: a weapon glow (the M_WindupGlow rim over the held weapon in the glow's colour), an armour dye (the
//    outfit's trim), a raid mark (the Smith's molten seams; the Wyrm's rime inlay), the prestige gold threads.
#include "WREndgameLook.h"
#include "WRDeeds.h"
#include "WRDeedsInternal.h"
#include "Wildreach.h"
#include "WRBuilding.h"
#include "WRBuildLook.h"
#include "WRBuildLookKit.h"
#include "WRCharacter.h"
#include "WRCreature.h"
#include "WRPlayerState.h"
#include "WRRecords.h"
#include "WRTuning.h"
#include "AnimationRuntime.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"

using namespace WRDeedsImpl;
using WRLookKit::FSurf;
using WRLookKit::FKit;

namespace WREndgameLookImpl
{
	struct FMonumentTune { float Scale = 0.4f; float Lift = 0.f; float Yaw = 0.f; float Pitch = 0.f; float Collar = 50.f; };
	FMonumentTune MonumentTune(FName Boss)
	{
		FMonumentTune T;
		const TSharedPtr<FJsonObject> Row = DChild(FWRTuning::Get().Object(TEXT("monuments")), Boss.ToString());
		T.Scale = DNum(Row, TEXT("scale"), T.Scale);
		T.Lift = DNum(Row, TEXT("lift"), T.Lift);
		T.Yaw = DNum(Row, TEXT("yaw"), T.Yaw);
		T.Pitch = DNum(Row, TEXT("pitch"), T.Pitch);
		T.Collar = DNum(Row, TEXT("collar"), T.Collar);
		return T;
	}
	/** The head bone: the first bone (root down) whose name has "head" in it and isn't a tip helper. */
	int32 HeadBone(const FReferenceSkeleton& Ref)
	{
		for (int32 i = 0; i < Ref.GetNum(); ++i)
		{
			const FString N = Ref.GetBoneName(i).ToString().ToLower();
			if (N.Contains(TEXT("head")) && !N.Contains(TEXT("top")) && !N.EndsWith(TEXT("_end"))) return i;
		}
		return INDEX_NONE;
	}
	bool Under(const FReferenceSkeleton& Ref, int32 Bone, int32 Root)
	{
		for (int32 b = Bone; b != INDEX_NONE; b = Ref.GetParentIndex(b)) if (b == Root) return true;
		return false;
	}
	/** Plinth top (the collar's rim), piece space. */
	constexpr float TopZ = 182.f;

	void Sigil(FKit& K, const FSurf& S, FName Id, const FVector& Centre, const FVector& Right, const FVector& Up, const FVector& Out, float Cell, float Depth)
	{
		const TArray<FString> Rows = WRDeeds::Sigil(Id);
		for (int32 r = 0; r < Rows.Num(); ++r)
		{
			for (int32 c = 0; c < Rows[r].Len(); ++c)
			{
				if (Rows[r][c] != TCHAR('#')) continue;
				const FVector P = Centre + Right * ((c - 4) * Cell) + Up * (-(r - 4) * Cell) + Out * (Depth * 0.5f);
				const FRotator Rot = FRotationMatrix::MakeFromXZ(Out, Up).Rotator();
				K.Add(S, FVector(FMath::Abs(Depth), Cell - 0.5f, Cell - 0.5f), P, Rot);
			}
		}
	}
}
using namespace WREndgameLookImpl;

// ================================================================================================ monuments

void WREndgameLook::BuildMonument(AWRBuildPiece* Piece, const FWRPieceDef& Def, bool bGhost)
{
	if (!Piece) return;
	const FName Boss = Def.Monument;
	// The server keeps the plaque line (who, how fast, at what AL) on the piece; clients draw it.
	if (Piece->HasAuthority() && !bGhost) SetPlaqueLine(Piece);
	if (Piece->GetNetMode() == NM_DedicatedServer) return;
	TArray<FWRLookBlock> Blocks;
	FKit K(Piece->GetActorTransform(), Blocks);
	const FSurf Granite = FSurf::Stone(FLinearColor(0.19f, 0.19f, 0.2f), 0.8f);
	const FSurf Dark = FSurf::Stone(FLinearColor(0.09f, 0.09f, 0.1f), 0.9f);
	const FSurf Bronze = FSurf::Steel(FLinearColor(0.42f, 0.24f, 0.11f), 0.38f);
	const FSurf Verdigris = FSurf::Stone(FLinearColor(0.16f, 0.28f, 0.22f), 0.7f);
	// The two courses: a wide base and a step, each a hand proud of the next (a dressed-stone plinth).
	K.Box(Granite.Dim(0.86f), FVector(-75.f, -75.f, -20.f), FVector(75.f, 75.f, 30.f));
	K.Box(Granite.Dim(0.94f), FVector(-64.f, -64.f, 30.f), FVector(64.f, 64.f, 48.f));
	// The shaft with a pilaster at each corner.
	K.Box(Granite, FVector(-52.f, -52.f, 48.f), FVector(52.f, 52.f, 150.f));
	for (const float X : { -1.f, 1.f }) for (const float Y : { -1.f, 1.f })
		K.Box(Granite.Dim(1.06f), FVector(X * 52.f - 7.f, Y * 52.f - 7.f, 48.f), FVector(X * 52.f + 7.f, Y * 52.f + 7.f, 150.f));
	// The front niche: sunk 5 cm, square below and arched above, framed by a raised moulding.
	K.Box(Dark, FVector(50.f, -32.f, 60.f), FVector(53.f, 32.f, 118.f));
	K.Add(Dark, FVector(64.f, 64.f, 3.f), FVector(51.5f, 0.f, 118.f), FRotator(90.f, 0.f, 0.f), EWRLookMesh::Cylinder);
	for (const float Y : { -36.f, 32.f }) K.Box(Granite.Dim(1.1f), FVector(52.f, Y, 58.f), FVector(55.f, Y + 4.f, 120.f));
	K.Box(Granite.Dim(1.1f), FVector(52.f, -36.f, 56.f), FVector(55.f, 36.f, 60.f));
	// The cap and the cushion stone the head rests on.
	K.Box(Granite.Dim(0.97f), FVector(-62.f, -62.f, 150.f), FVector(62.f, 62.f, 162.f));
	K.Box(Granite.Dim(0.9f), FVector(-46.f, -46.f, 162.f), FVector(46.f, 46.f, 172.f));
	// The sigil cut into both flanks.
	Sigil(K, Dark, Boss, FVector(0.f, 53.f, 100.f), FVector(1.f, 0.f, 0.f), FVector(0.f, 0.f, 1.f), FVector(0.f, -1.f, 0.f), 6.f, 1.4f);
	Sigil(K, Dark, Boss, FVector(0.f, -53.f, 100.f), FVector(-1.f, 0.f, 0.f), FVector(0.f, 0.f, 1.f), FVector(0.f, 1.f, 0.f), 6.f, 1.4f);
	const FMonumentTune T = MonumentTune(Boss);
	// The bronze collar the neck sits in (a turned ring with a verdigris bead).
	K.Add(Bronze, FVector(T.Collar, T.Collar, 10.f), FVector(0.f, 0.f, TopZ - 5.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
	K.Add(Verdigris, FVector(T.Collar + 4.f, T.Collar + 4.f, 2.f), FVector(0.f, 0.f, TopZ - 9.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
	// The plaque: a bronze tabula ansata in the niche, its two dovetail ears pinned to the stone.
	K.Box(Bronze, FVector(53.f, -26.f, 74.f), FVector(55.f, 26.f, 104.f));
	for (const float S : { -1.f, 1.f })
	{
		K.Add(Bronze, FVector(2.f, 9.f, 9.f), FVector(54.f, S * 29.5f, 89.f), FRotator(0.f, 0.f, 45.f));
		K.Add(Verdigris, FVector(2.5f, 2.f, 2.f), FVector(54.6f, S * 31.f, 89.f), FRotator(90.f, 0.f, 0.f), EWRLookMesh::Cylinder);
	}
	FWRBuildLook::Spawn(Piece, Piece->GetRootComponent(), Blocks, FLinearColor(0.3f, 0.25f, 0.2f), false);
	if (bGhost) return;

	// The head: the boss's own mesh, every bone but the head's folded into the neck.
	const FWRCreatureDef* C = FWRCreatureRegistry::Get().Find(Boss);
	USkeletalMesh* Mesh = C && !C->Mesh.IsEmpty() ? LoadObject<USkeletalMesh>(nullptr, *C->Mesh) : nullptr;
	if (Mesh)
	{
		UPoseableMeshComponent* Head = NewObject<UPoseableMeshComponent>(Piece, TEXT("MonumentHead"));
		Head->SetupAttachment(Piece->GetRootComponent());
		Head->SetSkinnedAssetAndUpdate(Mesh);
		for (int32 Slot = 0; Slot < C->Materials.Num(); ++Slot)
		{
			if (UMaterialInterface* M = C->Materials[Slot].IsEmpty() ? nullptr : LoadObject<UMaterialInterface>(nullptr, *C->Materials[Slot])) Head->SetMaterial(Slot, M);
		}
		Head->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Head->RegisterComponent();
		const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
		const int32 HeadIdx = HeadBone(Ref);
		if (HeadIdx != INDEX_NONE)
		{
			const FTransform HeadCS = FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, HeadIdx);
			for (int32 i = 0; i < Ref.GetNum(); ++i)
			{
				const FTransform CS = FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, i);
				const bool bKeep = Under(Ref, i, HeadIdx);
				const FTransform Want = bKeep ? CS : FTransform(CS.GetRotation(), HeadCS.GetLocation(), FVector(0.001f));
				Head->SetBoneTransformByName(Ref.GetBoneName(i), Want, EBoneSpaces::ComponentSpace);
			}
			// Seated so the head's joint sits in the collar, facing out over the plaque.
			const FRotator Rot(T.Pitch, C->MeshYaw + T.Yaw, 0.f);
			const FVector Scaled = Rot.RotateVector(HeadCS.GetLocation() * T.Scale);
			Head->SetRelativeScale3D(FVector(T.Scale));
			Head->SetRelativeRotation(Rot);
			Head->SetRelativeLocation(FVector(0.f, 0.f, TopZ + T.Lift) - Scaled);
		}
		else
		{
			UE_LOG(LogWildreach, Warning, TEXT("MONUMENT %s: its mesh has no head bone"), *Boss.ToString());
			Head->DestroyComponent();
		}
	}
	// The plaque's letters (cast in the bronze: dark, a little proud).
	const FColor Cast(58, 34, 18);
	const auto Line = [Piece, Cast](const TCHAR* Name, float Z, float Size)
	{
		UTextRenderComponent* Txt = NewObject<UTextRenderComponent>(Piece, Name);
		Txt->SetupAttachment(Piece->GetRootComponent());
		Txt->SetRelativeLocation(FVector(55.4f, 0.f, Z));
		Txt->SetHorizontalAlignment(EHTA_Center);
		Txt->SetVerticalAlignment(EVRTA_TextCenter);
		Txt->SetWorldSize(Size);
		Txt->SetTextRenderColor(Cast);
		Txt->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Txt->SetCastShadow(false);
		Txt->RegisterComponent();
	};
	Line(TEXT("PlaqueName"), 98.f, 6.f);
	Line(TEXT("PlaqueKill"), 88.f, 3.4f);
	Line(TEXT("PlaqueWho"), 81.f, 2.8f);
	RefreshPlaque(Piece);
}

void WREndgameLook::SetPlaqueLine(AWRBuildPiece* Piece)
{
	const FWRPieceDef* Def = Piece ? Piece->GetDef() : nullptr;
	if (!Def || Def->Monument.IsNone() || !Piece->HasAuthority()) return;
	const FString Line = WRRecords::PlaqueText(Piece->GetWorld(), Def->Monument, Piece->OwnerName);
	if (Line != Piece->CustomName) { Piece->CustomName = Line; Piece->ForceNetUpdate(); }
}

void WREndgameLook::RefreshPlaque(AWRBuildPiece* Piece)
{
	if (!Piece || Piece->GetNetMode() == NM_DedicatedServer) return;
	const FWRPieceDef* Def = Piece->GetDef();
	if (!Def) return;
	TArray<UTextRenderComponent*> Texts;
	Piece->GetComponents(Texts);
	TArray<FString> Parts;
	Piece->CustomName.ParseIntoArray(Parts, TEXT("|"), false);
	const int32 AL = Parts.IsValidIndex(2) ? FCString::Atoi(*Parts[2]) : 0;
	for (UTextRenderComponent* T : Texts)
	{
		const FString N = T->GetName();
		if (N.Contains(TEXT("PlaqueName"))) T->SetText(FText::FromString(WRRecords::BoardName(Def->Monument).ToUpper()));
		else if (N.Contains(TEXT("PlaqueKill"))) T->SetText(FText::FromString(Parts.Num() >= 1 && !Parts[0].IsEmpty()
			? FString::Printf(TEXT("Felled in %s%s"), *Parts[0], AL > 0 ? *FString::Printf(TEXT("  ·  Artifact Level %d"), AL) : TEXT(""))
			: FString(TEXT("Its fall is not on the boards yet"))));
		else if (N.Contains(TEXT("PlaqueWho"))) T->SetText(FText::FromString(Parts.IsValidIndex(1) ? Parts[1] : Piece->OwnerName));
	}
	// The geis-knot border at AL 15: a run of raised bronze knots round the plaque (built once).
	if (AL >= 15 && !Piece->FindComponentByTag<USceneComponent>(TEXT("GeisKnots")))
	{
		USceneComponent* Root = NewObject<USceneComponent>(Piece, TEXT("GeisKnotRoot"));
		Root->ComponentTags.Add(TEXT("GeisKnots"));
		Root->SetupAttachment(Piece->GetRootComponent());
		Root->RegisterComponent();
		TArray<FWRLookBlock> Blocks;
		FKit K(Piece->GetActorTransform(), Blocks);
		const FSurf Bronze = FSurf::Steel(FLinearColor(0.5f, 0.3f, 0.13f), 0.3f);
		for (int32 i = 0; i < 13; ++i)
		{
			const float Y = -24.f + 4.f * i;
			K.Add(Bronze, FVector(1.f, 2.6f, 2.6f), FVector(55.6f, Y, 106.f), FRotator(0.f, 0.f, 45.f + 45.f * (i % 2)));
			K.Add(Bronze, FVector(1.f, 2.6f, 2.6f), FVector(55.6f, Y, 72.f), FRotator(0.f, 0.f, 45.f + 45.f * ((i + 1) % 2)));
		}
		for (int32 i = 0; i < 8; ++i)
		{
			const float Z = 75.f + 4.f * i;
			for (const float Y : { -28.f, 28.f }) K.Add(Bronze, FVector(1.f, 2.6f, 2.6f), FVector(55.6f, Y, Z), FRotator(0.f, 0.f, 45.f + 45.f * (i % 2)));
		}
		FWRBuildLook::Spawn(Piece, Root, Blocks, FLinearColor(0.3f, 0.25f, 0.2f), false);
	}
}

// ================================================================================================ banners

FName WREndgameLook::SigilOf(const UWorld* World, const FString& Owner)
{
	const UWRDeedsComponent* D = UWRDeedsComponent::For(DStateNamed(World, Owner));
	return D ? D->BannerSigil : NAME_None;
}

void WREndgameLook::DressBanner(AWRBuildPiece* Piece)
{
	if (!Piece || Piece->GetNetMode() == NM_DedicatedServer) return;
	for (UActorComponent* Old : Piece->EmblemParts)
	{
		if (USceneComponent* S = Cast<USceneComponent>(Old))
		{
			TArray<USceneComponent*> Kids;
			S->GetChildrenComponents(true, Kids);
			for (USceneComponent* Kid : Kids) Kid->DestroyComponent();
		}
		if (Old) Old->DestroyComponent();
	}
	Piece->EmblemParts.Reset();
	if (Piece->Emblem.IsNone() || WRDeeds::Sigil(Piece->Emblem).Num() == 0) return;
	USceneComponent* Root = NewObject<USceneComponent>(Piece);
	Root->SetupAttachment(Piece->GetRootComponent());
	Root->RegisterComponent();
	Piece->EmblemParts.Add(Root);
	TArray<FWRLookBlock> Blocks;
	FKit K(Piece->GetActorTransform(), Blocks);
	// A dark woven square laid over the plain emblem, then the sigil's cells in pale linen thread.
	const FSurf Ground = FSurf::Cloth(FLinearColor(0.035f, 0.03f, 0.028f));
	const FSurf Thread = FSurf::Cloth(FLinearColor(0.78f, 0.72f, 0.6f));
	K.Box(Ground, FVector(9.1f, -30.f, 150.f), FVector(9.9f, 30.f, 210.f));
	Sigil(K, Thread, Piece->Emblem, FVector(10.3f, 0.f, 180.f), FVector(0.f, 1.f, 0.f), FVector(0.f, 0.f, 1.f), FVector(1.f, 0.f, 0.f), 6.f, 0.7f);
	FWRBuildLook::Spawn(Piece, Root, Blocks, FWRBuildLook::ClothColor(*Piece->GetDef()), false);
}

// ================================================================================================ the hero

uint32 WREndgameLook::LookSig(const AActor* Hero)
{
	const UWRDeedsComponent* D = UWRDeedsComponent::For(Hero);
	if (!D) return 0;
	uint32 Sig = GetTypeHash(D->WornMark);
	for (const FWRPrestigeRep& R : D->Marks) Sig = HashCombine(Sig, HashCombine(GetTypeHash(R.Path), R.Total()));
	return Sig;
}

float WREndgameLook::Gold(const AActor* Hero, FName Path) { return WRDeeds::GoldThreads(Hero, Path); }

FLinearColor WREndgameLook::GoldColour()
{
	const TArray<TSharedPtr<FJsonValue>>* A = DArr(FWRTuning::Get().Object(TEXT("prestige")), TEXT("goldColour"));
	const FColor C = A && A->Num() >= 3 ? FColor(uint8((*A)[0]->AsNumber()), uint8((*A)[1]->AsNumber()), uint8((*A)[2]->AsNumber())) : FColor(214, 170, 70);
	return FLinearColor(C);
}

bool WREndgameLook::SeamMark(const AActor* Hero, FLinearColor& OutColour, float& OutGlow)
{
	const UWRDeedsComponent* D = UWRDeedsComponent::For(Hero);
	const TSharedPtr<FJsonObject> Row = D && !D->WornMark.IsNone() ? WRDeeds::Cosmetic(D->WornMark) : nullptr;
	const TSharedPtr<FJsonObject> Seam = DChild(Row, TEXT("seam"));
	if (!Seam.IsValid()) return false;
	OutColour = FLinearColor(DColour(Seam, TEXT("colour"), FColor::White));
	OutGlow = DNum(Seam, TEXT("glow"), 1.f);
	return true;
}

void AWRCharacter::ApplyDeedsLook()
{
	if (GetNetMode() == NM_DedicatedServer) return;
	const UWRDeedsComponent* D = UWRDeedsComponent::For(this);
	// The weapon glow: the rim of the held weapon in its colour (the sword's mesh or a modelled one).
	const TSharedPtr<FJsonObject> Glow = D && !D->WornGlow.IsNone() ? WRDeeds::Cosmetic(D->WornGlow) : nullptr;
	if (Glow.IsValid() && !DeedGlowMaterial)
	{
		if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/FX/M_WindupGlow.M_WindupGlow"))) DeedGlowMaterial = UMaterialInstanceDynamic::Create(Base, this);
	}
	if (Glow.IsValid() && DeedGlowMaterial)
	{
		DeedGlowMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(DColour(Glow, TEXT("colour"), FColor::White)));
		DeedGlowMaterial->SetScalarParameterValue(TEXT("Glow"), DNum(Glow, TEXT("glow"), 0.9f));
		DeedGlowMaterial->SetScalarParameterValue(TEXT("Opacity"), DNum(Glow, TEXT("opacity"), 0.16f));
		DeedGlowMaterial->SetScalarParameterValue(TEXT("Wash"), 0.f);
	}
	UMaterialInterface* Overlay = Glow.IsValid() ? DeedGlowMaterial.Get() : nullptr;
	// The guard edge owns the shield; the glow is only the weapon's.
	if (WeaponMesh) WeaponMesh->SetOverlayMaterial(Overlay);
	if (ToolMesh) ToolMesh->SetOverlayMaterial(Overlay);

	// The armour: a dye recolours the trim, a raid mark lays its inlay. A change rebuilds the outfit fresh, then lays them.
	const TSharedPtr<FJsonObject> Dye = D && !D->WornDye.IsNone() ? WRDeeds::Cosmetic(D->WornDye) : nullptr;
	const TSharedPtr<FJsonObject> Mark = D && !D->WornMark.IsNone() ? WRDeeds::Cosmetic(D->WornMark) : nullptr;
	const uint32 Key = HashCombine(GetTypeHash(D ? D->WornDye : NAME_None), GetTypeHash(D ? D->WornMark : NAME_None));
	if (Key != DeedsOutfitKey && !bInDeedsOutfit && bBodyLoaded)
	{
		DeedsOutfitKey = Key;
		bInDeedsOutfit = true;
		ApplyOutfit(FWRTuning::LoadDataFile(TEXT("player_art.json")));   // calls back here with the fresh materials
		bInDeedsOutfit = false;
		ApplyPathMarks(true);
		return;
	}
	const TSharedPtr<FJsonObject> Inlay = DChild(Mark, TEXT("inlay"));
	for (UMaterialInstanceDynamic* MID : OutfitMIDs)
	{
		if (!MID) continue;
		if (Dye.IsValid()) MID->SetVectorParameterValue(TEXT("TrimColor"), FLinearColor(DColour(Dye, TEXT("colour"), FColor::White)));
		if (Inlay.IsValid())
		{
			MID->SetVectorParameterValue(TEXT("InlayColor"), FLinearColor(DColour(Inlay, TEXT("colour"), FColor::White)));
			MID->SetScalarParameterValue(TEXT("InlayNits"), DNum(Inlay, TEXT("nits"), 1.f));
		}
	}
	if (!bInDeedsOutfit) ApplyPathMarks(true);
}
