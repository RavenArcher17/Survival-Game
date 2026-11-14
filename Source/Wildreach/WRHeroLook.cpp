// The hero's look on every machine (cook hero-look-2, hero.md 2.5 / 2.6): the worn armour's outfit, its element trim and
// Ascendant inlay, the body-hide masks under it, and the path marks on the skin, eyes and hair.
// Everything here is data (Data/player_art.json "looks", "armorLooks", "elements", "paths"); the code only applies it.
#include "WRCharacter.h"
#include "Wildreach.h"
#include "WRPathComponent.h"
#include "WRPathTypes.h"
#include "WRPlayerState.h"
#include "WRSound.h"
#include "WRTuning.h"
#include "Components/LODSyncComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GroomComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace WRHeroLookImpl
{
	static const TCHAR* SlotNames[] = { TEXT("Head"), TEXT("Chest"), TEXT("Hands"), TEXT("Legs") };
	static const TCHAR* BuildNames[] = { TEXT("Lean"), TEXT("Athletic"), TEXT("Strong") };

	FLinearColor Color(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, const FLinearColor& Fallback)
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (!Obj.IsValid() || !Obj->TryGetArrayField(Field, Arr) || Arr->Num() < 3)
		{
			return Fallback;
		}
		return FLinearColor((*Arr)[0]->AsNumber(), (*Arr)[1]->AsNumber(), (*Arr)[2]->AsNumber(), Arr->Num() > 3 ? (*Arr)[3]->AsNumber() : 1.0);
	}

	TSharedPtr<FJsonObject> Obj(const TSharedPtr<FJsonObject>& Parent, const FString& Field)
	{
		const TSharedPtr<FJsonObject>* Out = nullptr;
		return Parent.IsValid() && Parent->TryGetObjectField(Field, Out) ? *Out : nullptr;
	}

	/** hero.md 2.6: strength = smoothstep(0.08, 1.0, points / max points per path). */
	float Strength(int32 XP)
	{
		const float Max = FMath::Max(1.f, WRTune(TEXT("paths.maxPointsPerPath"), 100.f));
		const float X = FMath::Clamp(FWRPathRules::PointsForXP(XP) / Max, 0.f, 1.f);
		return FMath::SmoothStep(0.08f, 1.f, X);
	}

	/** A material slot's role on the MetaHuman head, by its slot name (player_art.json "lookSlots" lists the names). */
	bool SlotIs(const TSharedPtr<FJsonObject>& Art, const TCHAR* Role, FName SlotName)
	{
		const TSharedPtr<FJsonObject> Slots = Obj(Art, TEXT("lookSlots"));
		const TArray<TSharedPtr<FJsonValue>>* Names = nullptr;
		if (!Slots.IsValid() || !Slots->TryGetArrayField(Role, Names))
		{
			return false;
		}
		const FString S = SlotName.ToString();
		for (const TSharedPtr<FJsonValue>& V : *Names)
		{
			if (S.Equals(V->AsString(), ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}
}
using namespace WRHeroLookImpl;

// ---------------------------------------------------------------------------------------------------- server: what's worn

void AWRCharacter::SetWornArmor(FName ItemId, FName Element)
{
	if (!HasAuthority())
	{
		return;
	}
	FWRHeroLook Next = HeroLook;
	Next.Look = NAME_None;
	Next.bAscendant = false;
	Next.Element = Element;
	if (!ItemId.IsNone())
	{
		const TSharedPtr<FJsonObject> Art = FWRTuning::LoadDataFile(TEXT("player_art.json"));
		const TSharedPtr<FJsonObject> Map = Obj(Art, TEXT("armorLooks"));
		FString LookId;
		if (const TSharedPtr<FJsonObject> Row = Obj(Map, ItemId.ToString()))
		{
			// { "look": "hidewear", "byElement": { "fire": "cinderhide" }, "ascendant": true }
			Row->TryGetStringField(TEXT("look"), LookId);
			Row->TryGetBoolField(TEXT("ascendant"), Next.bAscendant);
			FString ByElement;
			if (!Element.IsNone() && Obj(Row, TEXT("byElement")).IsValid() && Obj(Row, TEXT("byElement"))->TryGetStringField(Element.ToString(), ByElement))
			{
				LookId = ByElement;
			}
		}
		else if (Map.IsValid())
		{
			Map->TryGetStringField(ItemId.ToString(), LookId);
		}
		Next.Look = LookId.IsEmpty() ? NAME_None : FName(*LookId);
	}
	if (!(Next == HeroLook))
	{
		HeroLook = Next;
		OnRep_HeroLook();  // a listen server or standalone game draws its own copy now
	}
}

void AWRCharacter::SetHeroAppearance(uint8 InFace, uint8 InBuild)
{
	if (!HasAuthority())
	{
		return;
	}
	// Clamp to what the art has for this sex (a bad or stale ?Look= never picks a face that isn't there).
	const TSharedPtr<FJsonObject> Art = FWRTuning::LoadDataFile(TEXT("player_art.json"));
	const TSharedPtr<FJsonObject> Hero = Obj(Obj(Art, TEXT("hero")), BodyStyle == TEXT("f") ? TEXT("f") : TEXT("m"));
	const TArray<TSharedPtr<FJsonValue>>* Faces = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Builds = nullptr;
	const int32 NumFaces = Hero.IsValid() && Hero->TryGetArrayField(TEXT("faces"), Faces) ? Faces->Num() : 1;
	const int32 NumBuilds = Hero.IsValid() && Hero->TryGetArrayField(TEXT("builds"), Builds) ? Builds->Num() : 1;
	FWRHeroLook Next = HeroLook;
	Next.Face = uint8(FMath::Clamp<int32>(InFace, 0, FMath::Max(NumFaces - 1, 0)));
	Next.Build = uint8(FMath::Clamp<int32>(InBuild, 0, FMath::Max(NumBuilds - 1, 0)));
	if (!(Next == HeroLook))
	{
		HeroLook = Next;
		OnRep_HeroLook();
	}
}

void AWRCharacter::ServerSetHideHelmet_Implementation(bool bHide)
{
	if (AWRPlayerState* PS = GetPlayerState<AWRPlayerState>())
	{
		PS->bHideHelmet = bHide;
	}
	if (HeroLook.bHideHelmet != bHide)
	{
		HeroLook.bHideHelmet = bHide;
		OnRep_HeroLook();
	}
}

void AWRCharacter::OnRep_HeroLook()
{
	if (!HasActorBegunPlay() || GetNetMode() == NM_DedicatedServer)
	{
		return;  // BeginPlay builds the body with whatever arrived first; the server never draws
	}
	if (!bBodyLoaded || !LoadedLook.SameBody(HeroLook))
	{
		bBodyLoaded = false;  // another face or build: the whole body is rebuilt
		LoadBodyArt();
		return;
	}
	ApplyOutfit(FWRTuning::LoadDataFile(TEXT("player_art.json")));
	ApplyPathMarks(true);
}

// ---------------------------------------------------------------------------------------------------- the outfit

void AWRCharacter::ApplyOutfit(const TSharedPtr<FJsonObject>& Art)
{
	if (!Art.IsValid() || !Body->GetSkeletalMeshAsset())
	{
		return;
	}
	const bool bFemale = BodyStyle == TEXT("f");
	const FString Sex = bFemale ? TEXT("f") : TEXT("m");
	const FString BodyKey = FString::Printf(TEXT("%s_%s"), bFemale ? TEXT("F") : TEXT("M"), BuildNames[FMath::Min<int32>(HeroLook.Build, 2)]);
	const TSharedPtr<FJsonObject> Looks = Obj(Art, TEXT("looks"));
	FName LookId = HeroLook.Look.IsNone() ? FName(TEXT("wraps")) : HeroLook.Look;
	TSharedPtr<FJsonObject> Look = Obj(Looks, LookId.ToString());
	if (!Look.IsValid())
	{
		UE_LOG(LogWildreach, Warning, TEXT("Player art: no look '%s', wearing the wraps"), *LookId.ToString());
		LookId = TEXT("wraps");
		Look = Obj(Looks, TEXT("wraps"));
	}
	LoadedOutfit = LookId;
	double Tier = 0.0;
	if (Look.IsValid())
	{
		Look->TryGetNumberField(TEXT("tier"), Tier);
	}
	const TSharedPtr<FJsonObject> Meshes = Obj(Obj(Look, TEXT("meshes")), BodyKey);
	// Legacy (schema before hero-look-2): one outfit per sex under hero.<sex>.outfit.
	const TSharedPtr<FJsonObject> Legacy = Obj(Obj(Obj(Art, TEXT("hero")), Sex), TEXT("outfit"));
	// The element's trim palette (hero.md 2.5), the main path's colour on the seams.
	const TSharedPtr<FJsonObject> Elements = Obj(Art, TEXT("elements"));
	const bool bElement = !HeroLook.Element.IsNone() && Elements.IsValid() && Elements->HasField(HeroLook.Element.ToString());
	const FLinearColor Trim = bElement ? Color(Elements, *HeroLook.Element.ToString(), FLinearColor::White) : FLinearColor::White;
	const TSharedPtr<FJsonObject> MainRow = Obj(Obj(Art, TEXT("paths")), PathMarks.Main.ToString());
	const FLinearColor PathColor = Color(MainRow, TEXT("color"), FLinearColor(0.886f, 0.329f, 0.102f));
	double SeamGlow = 0.0, InlayNits = 0.0;
	if (const TSharedPtr<FJsonObject> Glow = Obj(Art, TEXT("outfitGlow")))
	{
		Glow->TryGetNumberField(TEXT("pathSeam"), SeamGlow);
		Glow->TryGetNumberField(TEXT("ascendantInlay"), InlayNits);
	}
	const float MainS = PathMarks.MainS / 255.f;

	OutfitMIDs.Reset();
	bool bHidesHair = false;
	if (Look.IsValid())
	{
		Look->TryGetBoolField(TEXT("hidesHair"), bHidesHair);
	}
	// The hide-helmet option takes the Head slot off (tier 0 has nothing to hide).
	const bool bHeadOff = HeroLook.bHideHelmet && Tier > 0.0;
	for (int32 i = 0; i < Outfit.Num() && i < UE_ARRAY_COUNT(SlotNames); ++i)
	{
		FString PiecePath;
		if (Meshes.IsValid())
		{
			Meshes->TryGetStringField(SlotNames[i], PiecePath);
		}
		else if (Legacy.IsValid() && LookId == TEXT("wraps"))
		{
			Legacy->TryGetStringField(SlotNames[i], PiecePath);
		}
		if (i == 0 && bHeadOff)
		{
			PiecePath.Reset();
		}
		USkeletalMesh* Piece = PiecePath.IsEmpty() ? nullptr : LoadObject<USkeletalMesh>(nullptr, *PiecePath);
		if (!PiecePath.IsEmpty() && !Piece)
		{
			UE_LOG(LogWildreach, Warning, TEXT("Player art: look %s piece %s not found"), *LookId.ToString(), *PiecePath);
		}
		Outfit[i]->EmptyOverrideMaterials();
		Outfit[i]->SetSkeletalMesh(Piece);
		// A look that is another look's meshes in other materials (Cinderhide, Snowhide: hero.md 2.5) swaps by slot name.
		if (Piece)
		{
			if (const TSharedPtr<FJsonObject> Swaps = Obj(Look, TEXT("materials")))
			{
				for (const auto& Swap : Swaps->Values)
				{
					const int32 Index = Outfit[i]->GetMaterialIndex(FName(*Swap.Key));
					UMaterialInterface* Mat = Index == INDEX_NONE ? nullptr : LoadObject<UMaterialInterface>(nullptr, *Swap.Value->AsString());
					if (Mat)
					{
						Outfit[i]->SetMaterial(Index, Mat);
					}
				}
			}
		}
		Outfit[i]->SetLeaderPoseComponent(Piece ? Body.Get() : nullptr);
		Outfit[i]->SetVisibility(Piece != nullptr);
		if (!Piece)
		{
			continue;
		}
		for (int32 M = 0; M < Outfit[i]->GetNumMaterials(); ++M)
		{
			UMaterialInstanceDynamic* MID = Outfit[i]->CreateDynamicMaterialInstance(M);
			if (!MID)
			{
				continue;
			}
			if (bElement)
			{
				MID->SetVectorParameterValue(TEXT("TrimColor"), Trim);
			}
			MID->SetVectorParameterValue(TEXT("PathTint"), PathColor);
			MID->SetScalarParameterValue(TEXT("PathGlow"), float(SeamGlow) * MainS);
			MID->SetVectorParameterValue(TEXT("InlayColor"), PathColor);
			MID->SetScalarParameterValue(TEXT("InlayNits"), HeroLook.bAscendant ? float(InlayNits) : 0.f);
			OutfitMIDs.Add(MID);
		}
	}
	// The hair goes under helms and raised hoods (brows and lashes stay).
	bHairHiddenByHelm = bHidesHair && !bHeadOff;
	if (Grooms.Num() > 0)
	{
		Grooms[0]->SetVisibility(Grooms[0]->GroomAsset != nullptr && !bHairHiddenByHelm);
	}
	// Ascendant halo-crest (hero.md 2.5 tier 6) behind the head.
	USkeletalMesh* Crest = nullptr;
	if (HeroLook.bAscendant && !bHeadOff)
	{
		FString CrestPath;
		if (const TSharedPtr<FJsonObject> Crests = Obj(Art, TEXT("ascendantCrest")))
		{
			Crests->TryGetStringField(BodyKey, CrestPath);
		}
		Crest = CrestPath.IsEmpty() ? nullptr : LoadObject<USkeletalMesh>(nullptr, *CrestPath);
	}
	HaloCrest->SetSkeletalMesh(Crest);
	HaloCrest->SetLeaderPoseComponent(Crest ? Body.Get() : nullptr);
	HaloCrest->SetVisibility(Crest != nullptr);
	if (Crest)
	{
		if (UMaterialInstanceDynamic* MID = HaloCrest->CreateDynamicMaterialInstance(0))
		{
			MID->SetVectorParameterValue(TEXT("InlayColor"), PathColor);
			MID->SetScalarParameterValue(TEXT("InlayNits"), float(InlayNits));
		}
	}

	// Body-hide masks (cook hero-look-2 3.6): the skin under each worn piece is clipped, so cloth never shows skin
	// through it in motion. White = keep; the body material multiplies the four.
	if (SkinMIDs.Num() > 0 && SkinMIDs[0])
	{
		const TSharedPtr<FJsonObject> Hide = Obj(Obj(Look, TEXT("hide")), Sex);
		// "keep everything": a white linear-greyscale texture (the material's mask samplers are linear greyscale)
		FString WhitePath;
		Art->TryGetStringField(TEXT("hideWhite"), WhitePath);
		UTexture* White = WhitePath.IsEmpty() ? nullptr : LoadObject<UTexture>(nullptr, *WhitePath);
		for (int32 i = 0; i < UE_ARRAY_COUNT(SlotNames); ++i)
		{
			FString MaskPath;
			if (Hide.IsValid() && !(i == 0 && bHeadOff) && Outfit.IsValidIndex(i) && Outfit[i]->IsVisible())
			{
				Hide->TryGetStringField(SlotNames[i], MaskPath);
			}
			UTexture* Mask = MaskPath.IsEmpty() ? nullptr : LoadObject<UTexture>(nullptr, *MaskPath);
			if (Mask || White)
			{
				SkinMIDs[0]->SetTextureParameterValue(FName(*(FString(TEXT("Hide")) + SlotNames[i])), Mask ? Mask : White);
			}
		}
	}
	RefreshLODSync();
}

void AWRCharacter::RefreshLODSync()
{
	USkeletalMesh* FaceAsset = Face->GetSkeletalMeshAsset();
	USkeletalMesh* BodyAsset = Body->GetSkeletalMeshAsset();
	if (!BodyAsset)
	{
		return;
	}
	// One LOD for the whole hero, led by the face (it has the most levels): the body, outfit and hair follow.
	LODSync->ComponentsToSync.Reset();
	LODSync->CustomLODMapping.Reset();
	LODSync->NumLODs = FaceAsset ? FaceAsset->GetLODNum() : BodyAsset->GetLODNum();
	LODSync->ComponentsToSync.Add({ Face->GetFName(), FaceAsset ? ESyncOption::Drive : ESyncOption::Disabled });
	LODSync->ComponentsToSync.Add({ Body->GetFName(), FaceAsset ? ESyncOption::Passive : ESyncOption::Drive });
	const auto MapTo = [this](int32 Levels)
	{
		FLODMappingData Map;
		for (int32 L = 0; L < LODSync->NumLODs; ++L)
		{
			Map.Mapping.Add(FMath::Min(Levels - 1, L * Levels / FMath::Max(LODSync->NumLODs, 1)));
		}
		return Map;
	};
	if (FaceAsset)
	{
		LODSync->CustomLODMapping.Add(Body->GetFName(), MapTo(BodyAsset->GetLODNum()));
	}
	TArray<USkeletalMeshComponent*> Pieces;
	for (USkeletalMeshComponent* Piece : Outfit)
	{
		Pieces.Add(Piece);
	}
	Pieces.Add(HaloCrest);
	for (USkeletalMeshComponent* Piece : Pieces)
	{
		if (Piece->GetSkeletalMeshAsset())
		{
			LODSync->ComponentsToSync.Add({ Piece->GetFName(), ESyncOption::Passive });
			LODSync->CustomLODMapping.Add(Piece->GetFName(), MapTo(Piece->GetSkeletalMeshAsset()->GetLODNum()));
		}
	}
	for (UGroomComponent* Groom : Grooms)
	{
		if (Groom->GroomAsset)
		{
			LODSync->ComponentsToSync.Add({ Groom->GetFName(), ESyncOption::Passive });
		}
	}
	LODSync->RefreshSyncComponents();
}

// ---------------------------------------------------------------------------------------------------- path marks

void AWRCharacter::BuildLookMaterials()
{
	const TSharedPtr<FJsonObject> Art = FWRTuning::LoadDataFile(TEXT("player_art.json"));
	SkinMIDs.Reset();
	EyeMIDs.Reset();
	HairMIDs.Reset();
	EyeDefaults.Reset();
	EyeDefaultColors.Reset();
	PathMarkSig = 0;
	// The body's skin first (index 0 carries the hide masks), then the head's skin LODs, its eyes.
	for (int32 M = 0; M < Body->GetNumMaterials(); ++M)
	{
		if (UMaterialInstanceDynamic* MID = Body->CreateDynamicMaterialInstance(M))
		{
			SkinMIDs.Add(MID);
		}
	}
	if (Face->GetSkeletalMeshAsset())
	{
		const TArray<FName> Slots = Face->GetMaterialSlotNames();
		for (int32 M = 0; M < Slots.Num(); ++M)
		{
			const bool bSkin = SlotIs(Art, TEXT("skin"), Slots[M]);
			const bool bEye = SlotIs(Art, TEXT("eyes"), Slots[M]);
			if (!bSkin && !bEye)
			{
				continue;
			}
			UMaterialInstanceDynamic* MID = Face->CreateDynamicMaterialInstance(M);
			if (MID)
			{
				(bSkin ? SkinMIDs : EyeMIDs).Add(MID);
			}
		}
	}
	for (UGroomComponent* Groom : Grooms)
	{
		if (!Groom->GroomAsset || Groom->GetName() == TEXT("Eyelashes"))
		{
			continue;
		}
		for (int32 M = 0; M < Groom->GetNumMaterials(); ++M)
		{
			if (UMaterialInstanceDynamic* MID = Groom->CreateDynamicMaterialInstance(M))
			{
				HairMIDs.Add(MID);
			}
		}
	}
	// The face's hair colour (the grooms are the engine's shared ones; the colour is the face's).
	for (UMaterialInstanceDynamic* MID : HairMIDs)
	{
		for (const TPair<FName, float>& P : FaceHairParams)
		{
			MID->SetScalarParameterValue(P.Key, P.Value);
		}
	}
	// Remember the eye and hair materials' own values for every parameter a path row touches, so marks blend from them.
	const TSharedPtr<FJsonObject> Paths = Obj(Art, TEXT("paths"));
	if (!Paths.IsValid())
	{
		return;
	}
	for (const auto& Pair : Paths->Values)
	{
		const TSharedPtr<FJsonObject> Row = Pair.Value->AsObject();
		for (const TCHAR* Part : { TEXT("eye"), TEXT("eyeSub"), TEXT("hair") })
		{
			const TSharedPtr<FJsonObject> Block = Obj(Row, Part);
			const TArray<TObjectPtr<UMaterialInstanceDynamic>>& MIDs = FCString::Strcmp(Part, TEXT("hair")) == 0 ? HairMIDs : EyeMIDs;
			if (!Block.IsValid() || MIDs.Num() == 0)
			{
				continue;
			}
			if (const TSharedPtr<FJsonObject> Scalars = Obj(Block, TEXT("scalar")))
			{
				for (const auto& S : Scalars->Values)
				{
					const FName Key(*(FString(Part) + TEXT(":") + S.Key));
					float V = 0.f;
					if (!EyeDefaults.Contains(Key) && MIDs[0]->GetScalarParameterValue(FMaterialParameterInfo(FName(*S.Key)), V))
					{
						EyeDefaults.Add(Key, V);
					}
				}
			}
			if (const TSharedPtr<FJsonObject> Vectors = Obj(Block, TEXT("vector")))
			{
				for (const auto& S : Vectors->Values)
				{
					const FName Key(*(FString(Part) + TEXT(":") + S.Key));
					FLinearColor V;
					if (!EyeDefaultColors.Contains(Key) && MIDs[0]->GetVectorParameterValue(FMaterialParameterInfo(FName(*S.Key)), V))
					{
						EyeDefaultColors.Add(Key, V);
					}
				}
			}
		}
	}
}

void AWRCharacter::OnPathAbilityUsed(FName PathId)
{
	if (HasAuthority() && !PathId.IsNone())
	{
		MulticastPathPulse(PathId);
	}
}

void AWRCharacter::MulticastPathPulse_Implementation(FName PathId)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const TSharedPtr<FJsonObject> Row = Obj(Obj(FWRTuning::LoadDataFile(TEXT("player_art.json")), TEXT("paths")), PathId.ToString());
	double Seconds = 1.5;
	FString Sound;
	if (Row.IsValid())
	{
		Row->TryGetNumberField(TEXT("pulseSeconds"), Seconds);
		Row->TryGetStringField(TEXT("castSound"), Sound);
	}
	PulsePath = PathId;
	PulseSeconds = float(FMath::Max(Seconds, 0.05));
	PulseUntil = GetWorld()->GetTimeSeconds() + PulseSeconds;
	if (!Sound.IsEmpty())
	{
		WRSound::PlayOn(this, FName(*Sound));  // Lightning's static crackle (hero.md 2.6)
	}
	PathMarkSig = 0;
}

void AWRCharacter::TickPathMarks(float DeltaSeconds)
{
	// Server: what every machine draws (the path progress itself only replicates to its owner).
	if (HasAuthority())
	{
		PathMarkCheck -= DeltaSeconds;
		if (PathMarkCheck <= 0.f)
		{
			PathMarkCheck = 1.f;
			FWRPathMarks Next;
			if (const AWRPlayerState* PS = GetPlayerState<AWRPlayerState>())
			{
				if (const UWRPathComponent* P = PS->GetPaths())
				{
					Next.Main = P->GetMainPath();
					Next.Sub = P->GetSubPath();
					const FWRPathProgress* MP = Next.Main.IsNone() ? nullptr : P->GetProgress(Next.Main);
					const FWRPathProgress* SP = Next.Sub.IsNone() ? nullptr : P->GetProgress(Next.Sub);
					Next.MainS = uint8(FMath::RoundToInt(255.f * (MP ? Strength(MP->XP) : 0.f)));
					Next.SubS = uint8(FMath::RoundToInt(255.f * (SP ? Strength(SP->XP) : 0.f)));
				}
			}
			if (!(Next == PathMarks))
			{
				PathMarks = Next;
			}
		}
	}
	if (GetNetMode() != NM_DedicatedServer && bBodyLoaded)
	{
		ApplyPathMarks(false);
	}
}

void AWRCharacter::ApplyPathMarks(bool bForce)
{
	if (SkinMIDs.Num() == 0 && EyeMIDs.Num() == 0 && HairMIDs.Num() == 0)
	{
		return;
	}
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	const float PulseLeft = PulseUntil > Now ? float((PulseUntil - Now) / PulseSeconds) : 0.f;
	const bool bWet = GetCharacterMovement() && GetCharacterMovement()->IsSwimming();
	uint32 Sig = HashCombine(GetTypeHash(PathMarks.Main), GetTypeHash(PathMarks.Sub));
	Sig = HashCombine(Sig, (uint32(PathMarks.MainS) << 8) | PathMarks.SubS);
	Sig = HashCombine(Sig, (uint32(FMath::RoundToInt(PulseLeft * 20.f)) << 1) | (bWet ? 1u : 0u));
	Sig = HashCombine(Sig, GetTypeHash(PulsePath));
	if (!bForce && Sig == PathMarkSig)
	{
		return;
	}
	PathMarkSig = Sig;
	const TSharedPtr<FJsonObject> Art = FWRTuning::LoadDataFile(TEXT("player_art.json"));
	const TSharedPtr<FJsonObject> Paths = Obj(Art, TEXT("paths"));
	const float MainS = PathMarks.MainS / 255.f;
	const float SubS = (PathMarks.SubS / 255.f) * float(WRTune(TEXT("pathMarks.subScale"), 0.6f));

	// Skin: MF_PathMarks takes Main* and Sub* parameters (prefix + the row's "skin" keys).
	const auto SkinRow = [&](const FString& Prefix, FName PathId, float S)
	{
		const TSharedPtr<FJsonObject> Row = Obj(Paths, PathId.ToString());
		const TSharedPtr<FJsonObject> Skin = Obj(Row, TEXT("skin"));
		for (UMaterialInstanceDynamic* MID : SkinMIDs)
		{
			MID->SetScalarParameterValue(FName(*(Prefix + TEXT("Strength"))), Skin.IsValid() ? S : 0.f);
			if (!Skin.IsValid())
			{
				continue;
			}
			MID->SetVectorParameterValue(FName(*(Prefix + TEXT("Color"))), Color(Row, TEXT("color"), FLinearColor::White));
			for (const auto& Pair : Skin->Values)
			{
				const FName Name(*(Prefix + Pair.Key));
				if (Pair.Value->Type == EJson::Number)
				{
					MID->SetScalarParameterValue(Name, float(Pair.Value->AsNumber()));
				}
				else if (Pair.Value->Type == EJson::Array)
				{
					MID->SetVectorParameterValue(Name, Color(Skin, *Pair.Key, FLinearColor::Black));
				}
				else if (Pair.Value->Type == EJson::String)
				{
					if (UTexture* Tex = LoadObject<UTexture>(nullptr, *Pair.Value->AsString()))
					{
						MID->SetTextureParameterValue(Name, Tex);
					}
				}
			}
		}
	};
	SkinRow(TEXT("Main"), PathMarks.Main, MainS);
	SkinRow(TEXT("Sub"), PathMarks.Sub, SubS);
	// Void greys with corruption (hero.md 2.6); until corruption is a stat, it follows the path's strength.
	bool bByCorruption = false;
	if (const TSharedPtr<FJsonObject> Row = Obj(Paths, PathMarks.Main.ToString()))
	{
		Row->TryGetBoolField(TEXT("byCorruption"), bByCorruption);
	}
	const float Pulse = PulsePath == PathMarks.Main || PulsePath == PathMarks.Sub ? PulseLeft : 0.f;
	for (UMaterialInstanceDynamic* MID : SkinMIDs)
	{
		MID->SetScalarParameterValue(TEXT("Pulse"), Pulse);
		MID->SetScalarParameterValue(TEXT("MainByCorruption"), bByCorruption ? 1.f : 0.f);
		MID->SetScalarParameterValue(TEXT("Corruption"), MainS);
		MID->SetScalarParameterValue(TEXT("Underwater"), bWet ? 1.f : 0.f);
	}

	// Eyes and hair: each parameter blends from the material's own value toward the row's by the path's strength. The
	// main path drives the iris and the hair; the sub path's "eyeSub" block (the limbal ring) at half its strength.
	const auto Blend = [&](const TArray<TObjectPtr<UMaterialInstanceDynamic>>& MIDs, const TCHAR* Part, FName PathId, float S)
	{
		const TSharedPtr<FJsonObject> Row = Obj(Paths, PathId.ToString());
		const TSharedPtr<FJsonObject> Block = Obj(Row, Part);
		if (!Block.IsValid())
		{
			return;
		}
		const float Boost = PathId == PulsePath ? 1.f + Pulse : 1.f;
		if (const TSharedPtr<FJsonObject> Scalars = Obj(Block, TEXT("scalar")))
		{
			for (const auto& P : Scalars->Values)
			{
				const float* Def = EyeDefaults.Find(FName(*(FString(Part) + TEXT(":") + P.Key)));
				const float V = FMath::Lerp(Def ? *Def : 0.f, float(P.Value->AsNumber()) * Boost, S);
				for (UMaterialInstanceDynamic* MID : MIDs)
				{
					MID->SetScalarParameterValue(FName(*P.Key), V);
				}
			}
		}
		if (const TSharedPtr<FJsonObject> Vectors = Obj(Block, TEXT("vector")))
		{
			for (const auto& P : Vectors->Values)
			{
				const FLinearColor* Def = EyeDefaultColors.Find(FName(*(FString(Part) + TEXT(":") + P.Key)));
				const FLinearColor V = FMath::Lerp(Def ? *Def : FLinearColor::Black, Color(Vectors, *P.Key, FLinearColor::White), S);
				for (UMaterialInstanceDynamic* MID : MIDs)
				{
					MID->SetVectorParameterValue(FName(*P.Key), V);
				}
			}
		}
	};
	// Reset everything a row may have touched to the material's own value, then blend in the current paths.
	for (const auto& Pair : EyeDefaults)
	{
		FString Part, Param;
		Pair.Key.ToString().Split(TEXT(":"), &Part, &Param);
		for (UMaterialInstanceDynamic* MID : Part == TEXT("hair") ? HairMIDs : EyeMIDs)
		{
			MID->SetScalarParameterValue(FName(*Param), Pair.Value);
		}
	}
	for (const auto& Pair : EyeDefaultColors)
	{
		FString Part, Param;
		Pair.Key.ToString().Split(TEXT(":"), &Part, &Param);
		for (UMaterialInstanceDynamic* MID : Part == TEXT("hair") ? HairMIDs : EyeMIDs)
		{
			MID->SetVectorParameterValue(FName(*Param), Pair.Value);
		}
	}
	Blend(EyeMIDs, TEXT("eyeSub"), PathMarks.Sub, SubS * 0.5f);
	Blend(EyeMIDs, TEXT("eye"), PathMarks.Main, MainS);
	Blend(HairMIDs, TEXT("hair"), PathMarks.Main, MainS);
	for (UMaterialInstanceDynamic* MID : HairMIDs)
	{
		MID->SetScalarParameterValue(TEXT("PathPulse"), Pulse);
	}

	// Blood: sharper canines from strength 0.6 (main, or the sub path's share).
	bool bFangs = false;
	FString FangPath;
	for (const TPair<FName, float>& P : { TPair<FName, float>(PathMarks.Main, MainS), TPair<FName, float>(PathMarks.Sub, SubS) })
	{
		double From = 0.0;
		const TSharedPtr<FJsonObject> Row = Obj(Paths, P.Key.ToString());
		if (Row.IsValid() && Row->TryGetNumberField(TEXT("caninesFrom"), From) && P.Value >= From)
		{
			bFangs = true;
			Row->TryGetStringField(TEXT("canineMesh"), FangPath);
		}
	}
	UStaticMesh* Fangs = bFangs && !FangPath.IsEmpty() ? LoadObject<UStaticMesh>(nullptr, *FangPath) : nullptr;
	if (Canines->GetStaticMesh() != Fangs)
	{
		Canines->SetStaticMesh(Fangs);
	}
	Canines->SetVisibility(Fangs != nullptr && Face->IsVisible());

	// The worn armour's seams carry the main path's colour (hero.md 2.5).
	if (OutfitMIDs.Num() > 0)
	{
		double SeamGlow = 0.0;
		if (const TSharedPtr<FJsonObject> Glow = Obj(Art, TEXT("outfitGlow")))
		{
			Glow->TryGetNumberField(TEXT("pathSeam"), SeamGlow);
		}
		const FLinearColor PathColor = Color(Obj(Paths, PathMarks.Main.ToString()), TEXT("color"), FLinearColor(0.886f, 0.329f, 0.102f));
		for (UMaterialInstanceDynamic* MID : OutfitMIDs)
		{
			MID->SetVectorParameterValue(TEXT("PathTint"), PathColor);
			MID->SetScalarParameterValue(TEXT("PathGlow"), float(SeamGlow) * MainS * (1.f + Pulse));
			MID->SetScalarParameterValue(TEXT("SeamPulse"), Pulse);
		}
	}
}
