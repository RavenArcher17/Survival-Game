// The Swamp's bosses and their arenas (cooks/biome-swamp.md 2.13-2.14, 2.10, 2.21; cooks/build-biome-swamp.md 2.7, 2.10;
// cooks/combat.md 2.17). See WRSwampBosses.h. Mother Maw, the Hex-Warden and the Bloat live here; Vharra, her rods and
// her flood in WRSwampVharra.cpp (every public hook forwards to her own first). The state and helpers both files share
// are in WRSwampBossDetail.h.
#include "WRSwampBosses.h"
#include "WRSwampBossDetail.h"
#include "WRSwampTypes.h"
#include "WRSwampSystems.h"
#include "WRSwampCreatures.h"
#include "WRStormTypes.h"
#include "WRBoss.h"
#include "WRAttributeSet.h"
#include "WRBuildLook.h"
#include "WRBuildLookKit.h"
#include "WRCharacter.h"
#include "WRCombatLibrary.h"
#include "WRFx.h"
#include "WRGameState.h"
#include "WRGameplayTags.h"
#include "WRInventory.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRTuning.h"
#include "Wildreach.h"
#include "AbilitySystemComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture.h"
#include "Materials/MaterialInterface.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "WRStrikeFront.h"

using namespace WRSwampBossDetail;

namespace WRSwbLocal
{
	using namespace WRLookKit;

	/** What each modelled group on a boss is (TickLook drives them by kind). */
	enum EGroup : int32
	{
		GrpStatic = 0, GrpEyes = 1, GrpMat = 2, GrpMask = 3, GrpMaskBroken = 4, GrpCloak = 5, GrpCast = 6,
		GrpVein0 = 7, GrpVein1 = 8, GrpVein2 = 9, GrpTorn0 = 10, GrpTorn1 = 11, GrpTorn2 = 12, GrpDisc = 13, GrpSickle = 14, GrpMarks = 15,
	};

	/** Mother Maw's deck width (the square's sections). */
	float SwlDeckWidth() { return SwbIT(TEXT("mother_maw"), TEXT("deckWidth"), 240.f); }

	/** Server: a creature's own call on every machine (creature.<id>.<kind>), silent if the table has none. */
	void SwlVoice(AWRCreature* C, const TCHAR* Kind)
	{
		if (!C) return;
		const FName E(*FString::Printf(TEXT("creature.%s.%s"), *C->CreatureId.ToString(), Kind));
		if (WRSound::Has(E)) WRSound::Broadcast(C, E);
	}

	/** The compost nest mound's surface over its foot (WRSwampScenery maw_nest: 600 cm round, 220 cm high, 20 cm sunk). */
	float SwlMoundZ(float R)
	{
		const float K = FMath::Clamp(R / 600.f, 0.f, 1.f);
		return -20.f + 220.f * FMath::Sqrt(FMath::Max(0.f, 1.f - K * K));
	}

	/** A crow's skull (the Coven's sign) at P facing Yaw: the dome, the long beak, the dark sockets; a garnet glint in
	 *  each socket when Glow > 0 (drawn in the glow kit). */
	void SwlCrowSkull(FKit& K, FKit* GlowK, const FVector& P, float YawDeg, float S, float Glow)
	{
		const FSurf BoneW = FSurf::Glaze(SwbHex(TEXT("#E2DACA")), 0.55f);
		const FSurf Socket = FSurf::Stone(SwbHex(TEXT("#0B0A0A")), 0.9f);
		const FVector F = FRotator(0.f, YawDeg, 0.f).Vector();
		const FVector R = FRotator(0.f, YawDeg + 90.f, 0.f).Vector();
		K.Disc(BoneW, P, FVector(F.X * 0.3f, F.Y * 0.3f, 1.f), 9.f * S, 7.f * S);
		K.Spike(BoneW, P + F * 3.5f * S + FVector(0.f, 0.f, -0.5f * S), F + FVector(0.f, 0.f, -0.18f), 3.4f * S, 11.f * S);
		K.Spike(BoneW.Dim(0.85f), P + F * 3.f * S + FVector(0.f, 0.f, -2.f * S), F + FVector(0.f, 0.f, -0.3f), 2.4f * S, 8.f * S);
		for (const float Side : { -1.f, 1.f })
		{
			K.Disc(Socket, P + F * 2.6f * S + R * Side * 2.2f * S + FVector(0.f, 0.f, 0.8f * S), F + R * Side * 0.6f, 2.4f * S, 1.2f * S);
			if (GlowK && Glow > 0.f) GlowK->Disc(FSurf::Lit(SwbHex(TEXT("#7A0A14")), Glow), P + F * 2.9f * S + R * Side * 2.3f * S + FVector(0.f, 0.f, 0.8f * S), F + R * Side * 0.6f, 1.1f * S, 0.6f * S);
		}
	}

	/** The rig's hand bones (left, right): names ending in "hand" (Mixamo LeftHand, hand_l, hand.R...), no fingers. */
	void SwlHands(const USkeletalMeshComponent* Body, FName& OutLeft, FName& OutRight)
	{
		OutLeft = OutRight = NAME_None;
		if (!Body || !Body->GetSkeletalMeshAsset()) return;
		const FReferenceSkeleton& Ref = Body->GetSkeletalMeshAsset()->GetRefSkeleton();
		for (int32 i = 0; i < Ref.GetNum(); ++i)
		{
			const FString N = Ref.GetBoneName(i).ToString().ToLower();
			const bool bHand = N.EndsWith(TEXT("hand")) || N.EndsWith(TEXT("hand_l")) || N.EndsWith(TEXT("hand_r")) || N.EndsWith(TEXT("hand.l")) || N.EndsWith(TEXT("hand.r"))
				|| N.EndsWith(TEXT("hand_left")) || N.EndsWith(TEXT("hand_right"));
			if (!bHand) continue;
			const bool bLeft = N.Contains(TEXT("left")) || N.EndsWith(TEXT("_l")) || N.EndsWith(TEXT(".l")) || N.StartsWith(TEXT("l_"));
			if (bLeft && OutLeft.IsNone()) OutLeft = Ref.GetBoneName(i);
			else if (!bLeft && OutRight.IsNone()) OutRight = Ref.GetBoneName(i);
		}
	}

	/** The rig's forearm bone on a side (for the arm's line), none if not found. */
	FName SwlForearm(const USkeletalMeshComponent* Body, bool bLeft)
	{
		if (!Body || !Body->GetSkeletalMeshAsset()) return NAME_None;
		const FReferenceSkeleton& Ref = Body->GetSkeletalMeshAsset()->GetRefSkeleton();
		for (int32 i = 0; i < Ref.GetNum(); ++i)
		{
			const FString N = Ref.GetBoneName(i).ToString().ToLower();
			if (!N.Contains(TEXT("forearm")) && !N.Contains(TEXT("lowerarm"))) continue;
			if (N.Contains(TEXT("twist"))) continue;
			const bool bL = N.Contains(TEXT("left")) || N.EndsWith(TEXT("_l")) || N.EndsWith(TEXT(".l")) || N.StartsWith(TEXT("l_"));
			if (bL == bLeft) return Ref.GetBoneName(i);
		}
		return NAME_None;
	}

	/** Its palette (the cook's colours) over the rig's own texture: M_CreatureTint, two tones from the texture's light. */
	void SwlTint(AWRCreature* C, USkeletalMeshComponent* Body, const TSharedPtr<FJsonObject>& Look, TArray<TObjectPtr<UMaterialInstanceDynamic>>& Out)
	{
		Out.Reset();
		const TArray<TSharedPtr<FJsonValue>>* Tints = nullptr;
		UMaterialInterface* Master = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Creatures/M_CreatureTint.M_CreatureTint"));
		if (!Look.IsValid() || !Master || !Look->TryGetArrayField(TEXT("tint"), Tints))
		{
			return;
		}
		const TArray<FName> SlotNames = Body->GetMaterialSlotNames();
		for (int32 Slot = 0; Slot < Body->GetNumMaterials(); ++Slot)
		{
			const FString SlotName = SlotNames.IsValidIndex(Slot) ? SlotNames[Slot].ToString().ToLower() : FString();
			TSharedPtr<FJsonObject> Pick;
			for (const TSharedPtr<FJsonValue>& V : *Tints)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				FString Match;
				if (!O.IsValid()) continue;
				if (O->TryGetStringField(TEXT("match"), Match) && !Match.IsEmpty())
				{
					if (SlotName.Contains(Match.ToLower())) { Pick = O; break; }
				}
				else if (!Pick.IsValid())
				{
					Pick = O;
				}
			}
			if (!Pick.IsValid()) continue;
			UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Master, C);
			FString Path;
			const auto Tex = [&](const TCHAR* Field, const TCHAR* Param, const TCHAR* Fallback)
			{
				const FString P = Pick->TryGetStringField(Field, Path) ? Path : FString(Fallback);
				if (UTexture* Tx = P.IsEmpty() ? nullptr : LoadObject<UTexture>(nullptr, *P)) M->SetTextureParameterValue(Param, Tx);
			};
			Tex(TEXT("tex"), TEXT("BaseColorTex"), TEXT(""));
			Tex(TEXT("normal"), TEXT("NormalTex"), TEXT("/Engine/EngineMaterials/DefaultNormal.DefaultNormal"));
			Tex(TEXT("glowTex"), TEXT("GlowTex"), TEXT("/Engine/EngineResources/Black.Black"));
			M->SetScalarParameterValue(TEXT("Desaturation"), float(SwbNum(Pick, TEXT("desat"), 1.0)));
			M->SetScalarParameterValue(TEXT("Brightness"), float(SwbNum(Pick, TEXT("bright"), 1.0)));
			M->SetScalarParameterValue(TEXT("TwoTone"), float(SwbNum(Pick, TEXT("twoTone"), 0.0)));
			M->SetScalarParameterValue(TEXT("LightFrom"), float(SwbNum(Pick, TEXT("lightFrom"), 0.0)));
			M->SetScalarParameterValue(TEXT("Roughness"), float(SwbNum(Pick, TEXT("rough"), 0.75)));
			M->SetScalarParameterValue(TEXT("GlowStrength"), float(SwbNum(Pick, TEXT("glowStrength"), 0.0)));
			M->SetVectorParameterValue(TEXT("Tint"), SwbColor(Pick, TEXT("tint"), FLinearColor::White));
			M->SetVectorParameterValue(TEXT("TintLight"), SwbColor(Pick, TEXT("light"), SwbColor(Pick, TEXT("tint"), FLinearColor::White)));
			M->SetVectorParameterValue(TEXT("GlowColor"), SwbColor(Pick, TEXT("glow"), SwbHex(TEXT("#7A0A14"))));
			Body->SetMaterial(Slot, M);
			Out.Add(M);
		}
	}

	/** A part's point on the body in body units (the server's own way, also used for the veins' look). */
	FVector SwlPartPoint(const AWRCreature* C, const FWRCreatureDef::FBodyPoint& P)
	{
		const float R = C->GetSimpleCollisionRadius(), HH = C->GetSimpleCollisionHalfHeight();
		return C->GetActorLocation() + C->GetActorForwardVector() * P.At.X * R + C->GetActorRightVector() * P.At.Y * R + FVector(0.f, 0.f, P.At.Z * HH);
	}

	/** The Bloat's three veins (its parts with onBreak "vein"), in file order. */
	TArray<int32> SwlVeinParts(const FWRCreatureDef* Def)
	{
		TArray<int32> Out;
		if (Def) for (int32 i = 0; i < Def->Parts.Num() && Out.Num() < 3; ++i) if (Def->Parts[i].OnBreak == TEXT("vein")) Out.Add(i);
		return Out;
	}
}

// ================================================================================================ the body

UWRSwampBossBody::UWRSwampBossBody()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.f;
	SetIsReplicatedByDefault(true);
}

void UWRSwampBossBody::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UWRSwampBossBody, Flags);
	DOREPLIFETIME(UWRSwampBossBody, Gorge);
	DOREPLIFETIME(UWRSwampBossBody, Sac0);
	DOREPLIFETIME(UWRSwampBossBody, Sac1);
	DOREPLIFETIME(UWRSwampBossBody, Sac2);
	DOREPLIFETIME(UWRSwampBossBody, Burst);
	DOREPLIFETIME(UWRSwampBossBody, Lash);
	DOREPLIFETIME(UWRSwampBossBody, Anchor);
}

UWRSwampBossBody* UWRSwampBossBody::Get(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UWRSwampBossBody>() : nullptr;
}

UWRSwampBossBody* UWRSwampBossBody::Attach(AWRCreature* Creature)
{
	if (!Creature || !Creature->HasAuthority())
	{
		return Get(Creature);
	}
	if (UWRSwampBossBody* Existing = Get(Creature))
	{
		return Existing;
	}
	UWRSwampBossBody* Body = NewObject<UWRSwampBossBody>(Creature, TEXT("SwampBossBody"));
	Body->RegisterComponent();
	Creature->AddInstanceComponent(Body);
	return Body;
}

void UWRSwampBossBody::EndPlay(const EEndPlayReason::Type Reason)
{
	ClearLook();
	Super::EndPlay(Reason);
}

USkeletalMeshComponent* UWRSwampBossBody::Skel() const
{
	const AWRCreature* C = Cast<AWRCreature>(GetOwner());
	return C ? C->GetCreatureSkel() : nullptr;
}

void UWRSwampBossBody::MulticastMoment_Implementation(FName Effect, FVector_NetQuantize At, float Scale, FName Sound)
{
	if (GetNetMode() == NM_DedicatedServer || !GetOwner())
	{
		return;
	}
	if (!Effect.IsNone()) WRFx::SpawnAt(GetOwner(), Effect, At, FRotator::ZeroRotator, Scale);
	if (!Sound.IsNone()) WRSound::PlayAt(GetOwner(), Sound, At);
}

void UWRSwampBossBody::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!FWRSwampBoss::IsVharra(GetOwner())) Clock += DeltaTime;   // (Vharra's look keeps its own clock)
	if (GetNetMode() != NM_DedicatedServer)
	{
		TickLook(DeltaTime);
	}
}

USceneComponent* UWRSwampBossBody::AddGroup(FName Bone, const FVector& WorldAt, const FRotator& WorldRot, const TArray<FWRLookBlock>& Blocks, int32 Kind)
{
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	USkeletalMeshComponent* Body = Skel();
	if (!C || !Body || Blocks.Num() == 0)
	{
		return nullptr;
	}
	USceneComponent* Frame = NewObject<USceneComponent>(C);
	Frame->RegisterComponent();
	Frame->AttachToComponent(Body, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, false), Bone);
	Frame->SetWorldScale3D(FVector(1.f));
	Frame->SetWorldLocationAndRotation(WorldAt, WorldRot);
	LookComps.Add(Frame);
	FWRBuildLook::Spawn(C, Frame, Blocks, FLinearColor::White, false);
	TArray<USceneComponent*> Children;
	Frame->GetChildrenComponents(false, Children);
	for (USceneComponent* Ch : Children) LookComps.Add(Ch);
	FGroup G;
	G.Frame = Frame;
	G.Kind = Kind;
	G.BaseRot = Frame->GetRelativeRotation();
	G.BaseLoc = Frame->GetRelativeLocation();
	G.BaseScale = Frame->GetRelativeScale3D();
	Groups.Add(G);
	return Frame;
}

void UWRSwampBossBody::SetGroupGlow(int32 Kind, float Glow)
{
	for (FGroup& G : Groups)
	{
		if (G.Kind != Kind || !G.Frame.IsValid() || FMath::IsNearlyEqual(G.Glow, Glow, 0.01f)) continue;
		G.Glow = Glow;
		TArray<USceneComponent*> Children;
		G.Frame->GetChildrenComponents(false, Children);
		for (USceneComponent* Ch : Children)
		{
			if (UInstancedStaticMeshComponent* ISM = Cast<UInstancedStaticMeshComponent>(Ch); ISM && ISM->NumCustomDataFloats >= 10)
			{
				for (int32 i = 0; i < ISM->GetInstanceCount(); ++i) ISM->SetCustomDataValue(i, 8, Glow, i == ISM->GetInstanceCount() - 1);
			}
		}
	}
}

void UWRSwampBossBody::SetGroupVisible(int32 Kind, bool bVisible)
{
	for (FGroup& G : Groups)
	{
		if (G.Kind != Kind || !G.Frame.IsValid() || G.bVisible == bVisible) continue;
		G.bVisible = bVisible;
		G.Frame->SetVisibility(bVisible, true);
	}
}

void UWRSwampBossBody::ClearLook()
{
	for (USceneComponent* Comp : LookComps) if (Comp) Comp->DestroyComponent();
	LookComps.Reset();
	for (USceneComponent* Comp : Trunk) if (Comp) Comp->DestroyComponent();
	Trunk.Reset();
	TrunkPts.Reset();
	Groups.Reset();
	if (LoopFx) { WRFx::Stop(LoopFx); LoopFx = nullptr; }
	LoopFxName = NAME_None;
	if (GlowLight) { GlowLight->DestroyComponent(); GlowLight = nullptr; }
	LookFor = NAME_None;
	GorgeShown = -1.f;
}

void UWRSwampBossBody::HideStandIn()
{
	// A hex totem is a hit body inside its pole: the creature's stand-in shapes go (the pole is the totem prop).
	AActor* Owner = GetOwner();
	if (!Owner || bStandInHidden)
	{
		return;
	}
	TArray<UStaticMeshComponent*> Meshes;
	Owner->GetComponents(Meshes);
	for (UStaticMeshComponent* M : Meshes)
	{
		if (M && !LookComps.Contains(static_cast<USceneComponent*>(M))) M->SetVisibility(false, true);
	}
	if (USkeletalMeshComponent* Body = Skel()) Body->SetVisibility(false, true);
	TArray<UPointLightComponent*> Lights;
	Owner->GetComponents(Lights);
	for (UPointLightComponent* L : Lights) if (L) L->SetVisibility(false);
	bStandInHidden = true;
}

void UWRSwampBossBody::BuildLook()
{
	using namespace WRLookKit;
	namespace L = WRSwbLocal;
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	const FWRCreatureDef* Def = C ? C->GetDef() : nullptr;
	if (!C || !Def)
	{
		return;
	}
	if (C->CreatureId == TEXT("hex_totem"))
	{
		HideStandIn();
		LookFor = C->CreatureId;
		return;
	}
	if (FWRSwampBoss::IsVharra(C))
	{
		// Vharra's palette, hood, sacs and trunk are hers (WRSwampVharra.cpp).
		ClearLook();
		FWRSwampBoss::VharraBuildLook(this);
		LookFor = C->CreatureId;
		return;
	}
	USkeletalMeshComponent* Body = Skel();
	if (!Body || !Body->GetSkeletalMeshAsset() || !Body->IsVisible())
	{
		return;
	}
	ClearLook();
	LookFor = C->CreatureId;
	const TSharedPtr<FJsonObject> Look = SwbLook(C->CreatureId);
	L::SwlTint(C, Body, Look, TintMats);
	// A low wedge (look.flatten cm): the fitted body squashed to that height, its strides unchanged (Mother Maw's
	// floating-island profile); the fit stood the mesh's bottom on the capsule's, so it stays there.
	if (const float Want = float(SwbNum(Look, TEXT("flatten"), 0.0)); Want > 0.f)
	{
		const FBox Box0 = Body->GetSkeletalMeshAsset()->GetBounds().GetBox();
		FVector S0 = Body->GetRelativeScale3D();
		const float Base0 = float(FMath::Min(S0.X, S0.Y));
		const float Height0 = FMath::Max(1.f, float(Box0.Max.Z - Box0.Min.Z) * Base0);
		const float NewZ = Base0 * FMath::Clamp(Want / Height0, 0.1f, 1.f);
		if (!FMath::IsNearlyEqual(float(S0.Z), NewZ, 0.0005f))
		{
			FVector Loc0 = Body->GetRelativeLocation();
			Loc0.Z -= Box0.Min.Z * (NewZ - S0.Z);
			S0.Z = NewZ;
			Body->SetRelativeScale3D(S0);
			Body->SetRelativeLocation(Loc0);
		}
	}
	BaseMeshScale = Body->GetRelativeScale3D();
	BaseMeshRot = Body->GetRelativeRotation();
	BaseMeshLoc = Body->GetRelativeLocation();
	bBaseSaved = true;

	const float Rad = Def->Radius;
	const FRotator Yaw(0.f, C->GetActorRotation().Yaw, 0.f);
	const FVector Fwd = Yaw.Vector(), Up = FVector::UpVector;
	const FTransform W = FTransform::Identity;
	const FBox AssetBox = Body->GetSkeletalMeshAsset()->GetBounds().GetBox();
	const FVector BS = Body->GetRelativeScale3D();
	const float BodyH = FMath::Max(10.f, (AssetBox.Max.Z - AssetBox.Min.Z) * BS.Z);
	const float H = BodyH * 0.5f;
	const float FloorZ = C->GetActorLocation().Z - C->GetSimpleCollisionHalfHeight();
	const float TopZ = FloorZ + BodyH;
	const auto BoneAt = [&](const TCHAR* Hint, FVector& Out) -> FName
	{
		const FName B = C->FindBone(FName(Hint));
		if (!B.IsNone()) Out = Body->GetBoneLocation(B);
		return B;
	};
	const auto BonesWith = [&](const TCHAR* Hint) -> TArray<TPair<FName, FVector>>
	{
		TArray<TPair<FName, FVector>> Out;
		const FReferenceSkeleton& Ref = Body->GetSkeletalMeshAsset()->GetRefSkeleton();
		for (int32 i = 0; i < Ref.GetNum(); ++i)
		{
			const FString N = Ref.GetBoneName(i).ToString().ToLower();
			if (!N.Contains(Hint) || N.Contains(TEXT("end")) || N.Contains(TEXT("nub"))) continue;
			Out.Add({ Ref.GetBoneName(i), Body->GetBoneLocation(Ref.GetBoneName(i)) });
		}
		return Out;
	};
	const auto FrontAt = [&](float Z) -> FVector
	{
		const FBoxSphereBounds B = Body->Bounds;
		const float Ext = FMath::Abs(Fwd.X) * B.BoxExtent.X + FMath::Abs(Fwd.Y) * B.BoxExtent.Y;
		const float Along = FVector::DotProduct(B.Origin - C->GetActorLocation(), Fwd) + Ext;
		FVector P = C->GetActorLocation() + Fwd * Along;
		P.Z = Z;
		return P;
	};
	const auto Blocks = [&](TFunctionRef<void(FKit&)> Fn) { TArray<FWRLookBlock> Out; FKit Kit(W, Out); Fn(Kit); return Out; };
	FVector Head = C->GetActorLocation() + Fwd * Rad;
	const FName HeadBone = BoneAt(TEXT("head"), Head);
	const FVector Tip = FrontAt(Head.Z);
	const float Snout = FMath::Max(Rad * 0.5f, float(FVector::Dist2D(Head, Tip)));
	static const TCHAR* Spine[] = { TEXT("neck"), TEXT("spine3"), TEXT("spine2"), TEXT("spine1"), TEXT("pelvis") };

	if (C->CreatureId == TEXT("mother_maw"))
	{
		// A Murkjaw grown to 13 m: the false-gharial bar of teeth (taller than you when she rears), the snout scarred
		// white, old broken arrows and a Coven bone hook in her neck plates, the living mat of duckweed and bloodmoss on
		// her back that makes her a floating island until she moves.
		const FSurf Tooth = FSurf::Glaze(SwbHex(TEXT("#CFC6AE")), 0.35f);
		const FSurf Seam = FSurf::Stone(SwbHex(TEXT("#15160F")), 0.7f);
		const FSurf Hide = FSurf::Stone(SwbHex(TEXT("#2A2E22")), 0.6f);
		const FSurf ScarS = FSurf::Glaze(SwbHex(TEXT("#D8D4C8")), 0.6f);
		const float HalfW0 = FMath::Max(H * 0.42f, Rad * 0.32f), HalfW1 = HalfW0 * 0.45f;
		const float JawZ = -H * 0.18f;
		AddGroup(HeadBone, Head, Yaw, Blocks([&](FKit& Kit)
		{
			const int32 N = 19;
			for (int32 i = 0; i < N; ++i)
			{
				const float T = 0.16f + 0.82f * i / (N - 1);
				const float X = Snout * T;
				const float Wd = FMath::Lerp(HalfW0, HalfW1, T);
				const float Len = H * (0.2f + 0.09f * Kit.Rand(FVector(X, 0.f, 0.f), 3)) * (T > 0.8f ? 1.3f : 1.f);
				for (const float Side : { -1.f, 1.f })
				{
					Kit.Spike(Tooth, FVector(X, Side * Wd * 0.92f, JawZ + H * 0.02f), FVector(0.12f, Side * 0.85f, -0.75f), H * 0.06f, Len);
					Kit.Spike(Tooth.Dim(0.9f), FVector(X + Snout * 0.02f, Side * Wd * 0.86f, JawZ - H * 0.04f), FVector(0.08f, Side * 0.8f, 0.7f), H * 0.055f, Len * 0.9f);
					Kit.Rod(Seam, FVector(X - Snout * 0.025f, Side * Wd, JawZ), FVector(X + Snout * 0.025f, Side * Wd * 0.98f, JawZ), H * 0.03f);
				}
			}
			// The white scars across the snout: old fights, drawn as raised pale weals.
			for (int32 s = 0; s < 7; ++s)
			{
				const float X = Snout * (0.22f + 0.1f * s);
				const float Wd = FMath::Lerp(HalfW0, HalfW1, X / Snout) * 0.8f;
				const float Skew = Kit.Jit(FVector(X, 1.f, 0.f), 5) * Snout * 0.08f;
				Kit.Rod(ScarS, FVector(X - Skew, -Wd, H * 0.14f), FVector(X + Skew, Wd * (0.4f + 0.5f * Kit.Rand(FVector(X, 2.f, 0.f), 6)), H * 0.16f), H * 0.035f);
			}
			Kit.Rod(ScarS, FVector(Snout * 0.3f, -HalfW0 * 0.2f, H * 0.17f), FVector(Snout * 0.9f, HalfW1 * 0.3f, H * 0.12f), H * 0.03f);
			Kit.Disc(Hide, FVector(Snout * 0.97f, 0.f, H * 0.06f), Up, HalfW1 * 1.6f, H * 0.13f);
			for (const float Side : { -1.f, 1.f }) Kit.Disc(Seam, FVector(Snout * 0.99f, Side * HalfW1 * 0.3f, H * 0.14f), Up, H * 0.05f, H * 0.02f);
			for (const float Side : { -1.f, 1.f }) Kit.Disc(Hide, FVector(-Snout * 0.04f, Side * HalfW0 * 0.55f, H * 0.2f), FVector(0.f, Side * 0.3f, 1.f), H * 0.22f, H * 0.13f);
		}), L::GrpStatic);
		AddGroup(HeadBone, Head, Yaw, Blocks([&](FKit& Kit)
		{
			for (const float Side : { -1.f, 1.f }) Kit.Disc(FSurf::Lit(SwbHex(TEXT("#7A0A14")), 1.f), FVector(-Snout * 0.01f, Side * HalfW0 * 0.62f, H * 0.25f), FVector(0.5f, Side, 0.2f), H * 0.09f, 1.4f);
		}), L::GrpEyes);
		// The neck plates: six old arrows (two still fletched, the rest snapped) and the Coven's bone hook on its cord.
		FVector NeckAt = C->GetActorLocation() + Fwd * Rad * 0.6f;
		FName NeckBone = BoneAt(TEXT("neck"), NeckAt);
		if (NeckBone.IsNone()) NeckBone = BoneAt(TEXT("spine3"), NeckAt);
		NeckAt.Z = TopZ - H * 0.1f;
		const FSurf Shaft = FSurf::Wood(SwbHex(TEXT("#5A4128")));
		const FSurf Fletch = FSurf::Cloth(SwbHex(TEXT("#1E1C1A")));
		const FSurf HookS = FSurf::Glaze(SwbHex(TEXT("#D8D2C4")), 0.5f);
		const FSurf SCord = FSurf::Cloth(SwbHex(TEXT("#3A2E22")));
		AddGroup(NeckBone, NeckAt, Yaw, Blocks([&](FKit& Kit)
		{
			for (int32 i = 0; i < 6; ++i)
			{
				const float Side = (i % 2) ? 1.f : -1.f;
				const FVector P(Kit.Jit(FVector(float(i), 0.f, 0.f), 1) * Rad * 0.35f, Side * H * (0.12f + 0.2f * Kit.Rand(FVector(float(i), 1.f, 0.f), 2)), 0.f);
				const FVector Dir = FVector(-0.35f + 0.3f * Kit.Jit(P, 3), Side * (0.35f + 0.2f * Kit.Rand(P, 4)), 0.85f).GetSafeNormal();
				const float Len = i < 2 ? 75.f : 22.f + 25.f * Kit.Rand(P, 5);
				Kit.Rod(Shaft, P - Dir * 12.f, P + Dir * Len, 2.4f);
				if (i < 2)
				{
					for (int32 f = 0; f < 3; ++f)
					{
						const FRotator Turn = FRotationMatrix::MakeFromZ(Dir).Rotator() + FRotator(0.f, 0.f, 120.f * f);
						Kit.Add(Fletch, FVector(0.6f, 3.5f, 11.f), P + Dir * (Len - 7.f) + Turn.RotateVector(FVector(0.f, 2.2f, 0.f)), Turn);
					}
				}
				else
				{
					Kit.Spike(Shaft.Dim(0.8f), P + Dir * Len, Dir + FVector(Kit.Jit(P, 6) * 0.5f, 0.f, 0.f), 2.2f, 5.f);   // the splintered break
				}
			}
			// The hook: a curved bone with a barb, its cord trailing down the neck.
			const FVector HookAt(-Rad * 0.08f, H * 0.22f, H * 0.06f);
			FVector Prev = HookAt + FVector(FMath::Cos(PI * 1.15f), 0.f, FMath::Sin(PI * 1.15f)) * H * 0.18f;
			for (int32 s = 1; s <= 8; ++s)
			{
				const float A = PI * (1.15f - 1.05f * s / 8.f);
				const FVector P = HookAt + FVector(FMath::Cos(A), 0.f, FMath::Sin(A)) * H * 0.18f;
				Kit.Rod(HookS, Prev, P, FMath::Lerp(5.f, 2.6f, s / 8.f));
				Prev = P;
			}
			Kit.Spike(HookS, Prev, FVector(-0.6f, 0.f, -0.8f), 2.6f, 7.f);
			const FVector Eye = HookAt + FVector(FMath::Cos(PI * 1.15f), 0.f, FMath::Sin(PI * 1.15f)) * H * 0.18f;
			Kit.Rod(SCord, Eye, Eye + FVector(-14.f, 4.f, -10.f), 1.2f);
			Kit.Rod(SCord, Eye + FVector(-14.f, 4.f, -10.f), Eye + FVector(-30.f, 10.f, -26.f), 1.1f);
			Kit.Rod(SCord, Eye + FVector(-30.f, 10.f, -26.f), Eye + FVector(-36.f, 14.f, -40.f), 0.9f);
		}), L::GrpStatic);
		// The back: keeled scutes, and over them the living mat (duckweed fronds, bloodmoss in garnet tufts, a few reed
		// stalks rooted in it, a lily pad), heaviest over the shoulders and hips.
		const FSurf Scute = FSurf::Stone(SwbHex(TEXT("#22261B")), 0.55f);
		const FSurf Duck = FSurf::Cloth(SwbHex(TEXT("#6E8B2E")));
		const FSurf DuckL = FSurf::Cloth(SwbHex(TEXT("#8FA83A")));
		const FSurf Moss = FSurf::Cloth(SwbHex(TEXT("#6A1E1E")));
		const FSurf MossDark = FSurf::Cloth(SwbHex(TEXT("#4A0A0F")));
		const FSurf Reed = FSurf::Cloth(SwbHex(TEXT("#7A7A4A")));
		const FSurf Lily = FSurf::Glaze(SwbHex(TEXT("#3F5E22")), 0.4f);
		TArray<TPair<FName, FVector>> Back;
		for (const TCHAR* S : Spine) { FVector At; const FName B = BoneAt(S, At); if (!B.IsNone()) Back.Add({ B, At }); }
		TArray<TPair<FName, FVector>> Tail = BonesWith(TEXT("tail"));
		for (int32 t = 0; t < Tail.Num() && t < 3; ++t) Back.Add(Tail[t]);
		for (int32 b = 0; b < Back.Num(); ++b)
		{
			const bool bTail = Back[b].Key.ToString().ToLower().Contains(TEXT("tail"));
			FVector At = Back[b].Value;
			At.Z = bTail ? At.Z + H * 0.24f : TopZ - H * 0.04f;
			const float Seg = Rad * (bTail ? 0.85f : 0.6f);
			AddGroup(Back[b].Key, At, Yaw, Blocks([&](FKit& Kit)
			{
				for (int32 i = 0; i < 3; ++i)
				{
					const float X = -Seg * i / 3.f;
					for (const float Side : { -1.f, 1.f })
					{
						const FVector P(X, Side * (bTail ? H * 0.12f : H * 0.3f), 0.f);
						const float Tall = H * (bTail ? 0.28f : 0.15f) * (0.85f + 0.3f * Kit.Rand(P, 7));
						Kit.Add(Scute, FVector(Seg / 3.f * 0.8f, H * 0.12f, Tall), P + FVector(0.f, 0.f, Tall * 0.3f), FRotator(0.f, 0.f, Side * (bTail ? 4.f : 14.f)));
					}
				}
			}), L::GrpStatic);
			AddGroup(Back[b].Key, At + FVector(0.f, 0.f, H * 0.06f), Yaw, Blocks([&](FKit& Kit)
			{
				const int32 N = bTail ? 14 : 34;
				const float Wide = bTail ? H * 0.2f : H * 0.48f;
				for (int32 i = 0; i < N; ++i)
				{
					const FVector P(-Seg * Kit.Rand(FVector(float(i), float(b), 0.f), 11), Kit.Jit(FVector(float(i), float(b), 1.f), 12) * Wide, Kit.Rand(FVector(float(i), float(b), 2.f), 13) * H * 0.05f);
					Kit.Disc(i % 3 ? Duck : DuckL, P, FVector(Kit.Jit(P, 1) * 0.15f, Kit.Jit(P, 2) * 0.15f, 1.f), H * (0.07f + 0.09f * Kit.Rand(P, 3)), 1.4f);
				}
				for (int32 m = 0; m < (bTail ? 3 : 9); ++m)
				{
					const FVector Q(-Seg * Kit.Rand(FVector(float(m), float(b), 5.f), 14), Kit.Jit(FVector(float(m), float(b), 6.f), 15) * Wide * 0.8f, H * 0.03f);
					Kit.Spike(m % 2 ? Moss : MossDark, Q, FVector(Kit.Jit(Q, 1) * 0.4f, Kit.Jit(Q, 2) * 0.4f, 1.f), H * 0.09f, H * (0.07f + 0.06f * Kit.Rand(Q, 3)));
				}
				if (!bTail && b % 2 == 0)
				{
					for (int32 r = 0; r < 3; ++r)
					{
						const FVector Q(-Seg * (0.2f + 0.25f * r), Kit.Jit(FVector(float(r), float(b), 7.f), 16) * Wide * 0.6f, 0.f);
						Kit.Rod(Reed, Q, Q + FVector(Kit.Jit(Q, 4) * 12.f, Kit.Jit(Q, 5) * 12.f, 45.f + 50.f * Kit.Rand(Q, 6)), 1.6f);
					}
					Kit.Disc(Lily, FVector(-Seg * 0.5f, Wide * 0.4f, H * 0.04f), Up, H * 0.3f, 1.f);
				}
			}), L::GrpMat);
		}
	}
	else if (C->CreatureId == TEXT("hex_warden"))
	{
		// The last Coven witch: the crow-skull mask (the brightest point in the dark), the long cloak of black crow
		// feathers that flares into wings, the bone-and-leather corset, the garnet vein-rings on her bare arms, a bone
		// sickle in each hand. Under the mask (when it breaks) garnet eyes.
		const FSurf MaskS = FSurf::Glaze(SwbHex(TEXT("#E8E2D2")), 0.5f);
		const FSurf Socket = FSurf::Stone(SwbHex(TEXT("#0B0A0A")), 0.9f);
		const FSurf Strap = FSurf::Stone(SwbHex(TEXT("#2A1E16")), 0.7f);
		const FSurf Feather = FSurf::Glaze(SwbHex(TEXT("#0E0D10")), 0.6f);
		const FSurf Feather2 = FSurf::Glaze(SwbHex(TEXT("#1A1820")), 0.55f);
		const FSurf SBone = FSurf::Glaze(SwbHex(TEXT("#CFC6AE")), 0.5f);
		const FSurf Leather = FSurf::Stone(SwbHex(TEXT("#3A2A22")), 0.65f);
		const FSurf Edge = FSurf::Glaze(SwbHex(TEXT("#F0EADC")), 0.3f);
		const float Hs = H * 0.2f;   // her head's size unit (a head is about a fifth of half her height)
		AddGroup(HeadBone, Head, Yaw, Blocks([&](FKit& Kit)
		{
			// The skull's dome over her brow, the long beak down over her nose, the sockets her eyes look through.
			Kit.Disc(MaskS, FVector(Hs * 0.15f, 0.f, Hs * 0.35f), FVector(0.25f, 0.f, 1.f), Hs * 1.05f, Hs * 0.55f);
			Kit.Add(MaskS, FVector(Hs * 0.35f, Hs * 0.8f, Hs * 0.45f), FVector(Hs * 0.45f, 0.f, Hs * 0.15f));
			Kit.Spike(MaskS, FVector(Hs * 0.6f, 0.f, Hs * 0.08f), FVector(1.f, 0.f, -0.3f), Hs * 0.32f, Hs * 1.25f);
			Kit.Spike(MaskS.Dim(0.88f), FVector(Hs * 0.58f, 0.f, -Hs * 0.05f), FVector(1.f, 0.f, -0.45f), Hs * 0.22f, Hs * 0.85f);
			for (const float Side : { -1.f, 1.f })
			{
				Kit.Disc(Socket, FVector(Hs * 0.6f, Side * Hs * 0.22f, Hs * 0.22f), FVector(1.f, Side * 0.35f, 0.f), Hs * 0.24f, Hs * 0.06f);
				// The straps round her head to the knot at the back.
				Kit.Rod(Strap, FVector(Hs * 0.45f, Side * Hs * 0.42f, Hs * 0.2f), FVector(-Hs * 0.1f, Side * Hs * 0.5f, Hs * 0.18f), Hs * 0.06f);
				Kit.Rod(Strap, FVector(-Hs * 0.1f, Side * Hs * 0.5f, Hs * 0.18f), FVector(-Hs * 0.5f, Side * Hs * 0.12f, Hs * 0.22f), Hs * 0.06f);
			}
			// Crow feathers fanned back from the crown.
			for (int32 f = 0; f < 9; ++f)
			{
				const float A = -60.f + 15.f * f;
				const FVector D = FRotator(35.f + 10.f * FMath::Abs(f - 4) / 4.f, 180.f + A, 0.f).Vector();
				Kit.Spike(f % 2 ? Feather : Feather2, FVector(-Hs * 0.15f, 0.f, Hs * 0.45f) + D * Hs * 0.2f, D, Hs * 0.14f, Hs * (0.9f + 0.4f * Kit.Rand(D, 1)));
			}
		}), L::GrpMask);
		AddGroup(HeadBone, Head, Yaw, Blocks([&](FKit& Kit)
		{
			for (const float Side : { -1.f, 1.f }) Kit.Disc(FSurf::Lit(SwbHex(TEXT("#7A0A14")), 0.f), FVector(Hs * 0.63f, Side * Hs * 0.22f, Hs * 0.22f), FVector(1.f, Side * 0.35f, 0.f), Hs * 0.12f, Hs * 0.03f);
		}), L::GrpCast);
		AddGroup(HeadBone, Head, Yaw, Blocks([&](FKit& Kit)
		{
			// The mask gone: her garnet eyes, and a shard of it still caught in the strap at her temple.
			for (const float Side : { -1.f, 1.f }) Kit.Disc(FSurf::Lit(SwbHex(TEXT("#7A0A14")), 5.f), FVector(Hs * 0.5f, Side * Hs * 0.2f, Hs * 0.2f), FVector(1.f, Side * 0.3f, 0.f), Hs * 0.09f, Hs * 0.02f);
			Kit.Add(MaskS, FVector(Hs * 0.2f, Hs * 0.08f, Hs * 0.3f), FVector(Hs * 0.2f, Hs * 0.5f, Hs * 0.3f), FRotator(10.f, 20.f, -15.f));
			Kit.Rod(Strap, FVector(Hs * 0.2f, Hs * 0.5f, Hs * 0.2f), FVector(-Hs * 0.4f, Hs * 0.2f, Hs * 0.2f), Hs * 0.05f);
		}), L::GrpMaskBroken);
		// The cloak: overlapping black feather blades from the shoulders to the ankles, two wings of it at her back.
		FVector Chest = C->GetActorLocation() + FVector(0.f, 0.f, H * 0.5f);
		FName ChestBone = BoneAt(TEXT("spine2"), Chest);
		if (ChestBone.IsNone()) ChestBone = BoneAt(TEXT("spine"), Chest);
		const float ShoulderZ = Head.Z - Hs * 1.2f - Chest.Z;
		const float HemZ = FloorZ + 12.f - Chest.Z;
		for (const float Side : { -1.f, 1.f })
		{
			AddGroup(ChestBone, Chest, Yaw, Blocks([&](FKit& Kit)
			{
				const int32 Rows = 9, Cols = 6;
				for (int32 r = 0; r < Rows; ++r)
				{
					const float Z = FMath::Lerp(ShoulderZ, HemZ, (r + 0.5f) / Rows);
					const float Spread = FMath::Lerp(Rad * 0.55f, Rad * 1.05f, float(r) / Rows);
					for (int32 c = 0; c < Cols; ++c)
					{
						const float U = (c + 0.5f) / Cols;
						const FVector P(-Rad * 0.35f - U * Rad * 0.25f * (1.f - float(r) / Rows), Side * U * Spread, Z);
						const float Len = (HemZ - ShoulderZ) / Rows * -2.1f;
						Kit.Add((r + c) % 3 ? Feather : Feather2, FVector(2.5f, 9.f + 4.f * Kit.Rand(P, 1), Len), P, FRotator(Kit.Jit(P, 2) * 6.f, Side * (8.f + 22.f * U), Kit.Jit(P, 3) * 5.f));
					}
				}
				// The collar of upright feathers round her neck.
				for (int32 c = 0; c < 6; ++c)
				{
					const FVector P(-Rad * 0.15f - c * 2.f, Side * (Rad * 0.15f + c * Rad * 0.06f), ShoulderZ + 5.f);
					Kit.Spike(c % 2 ? Feather : Feather2, P, FVector(-0.3f, Side * 0.4f, 1.f), 6.f, 22.f + 4.f * c);
				}
			}), L::GrpCloak);
		}
		// The corset: bone stays round the waist between leather bands, laced at the back.
		AddGroup(ChestBone, Chest, Yaw, Blocks([&](FKit& Kit)
		{
			const float Rx = Rad * 0.42f, Ry = Rad * 0.36f;
			const float Z0 = -H * 0.32f, Z1 = H * 0.02f;
			for (int32 s = 0; s < 16; ++s)
			{
				const float A = 2.f * PI * s / 16.f;
				const FVector P(FMath::Cos(A) * Rx, FMath::Sin(A) * Ry, 0.f);
				Kit.Rod(SBone, P + FVector(0.f, 0.f, Z0), P * 0.92f + FVector(0.f, 0.f, Z1), 2.2f);
			}
			for (const float Z : { Z0, Z1, (Z0 + Z1) * 0.5f })
			{
				for (int32 s = 0; s < 16; ++s)
				{
					const float A0 = 2.f * PI * s / 16.f, A1 = 2.f * PI * (s + 1) / 16.f;
					const float K = Z == Z1 ? 0.93f : 1.02f;
					Kit.Rod(Leather, FVector(FMath::Cos(A0) * Rx * K, FMath::Sin(A0) * Ry * K, Z), FVector(FMath::Cos(A1) * Rx * K, FMath::Sin(A1) * Ry * K, Z), Z == (Z0 + Z1) * 0.5f ? 1.4f : 3.f);
				}
			}
			for (int32 l = 0; l < 6; ++l)
			{
				const float Z = FMath::Lerp(Z0 + 4.f, Z1 - 4.f, l / 5.f);
				Kit.Rod(Strap, FVector(-Rx * 1.03f, -3.f, Z), FVector(-Rx * 1.03f, 3.f, Z + (Z1 - Z0) / 10.f), 0.7f);
				Kit.Rod(Strap, FVector(-Rx * 1.03f, 3.f, Z), FVector(-Rx * 1.03f, -3.f, Z + (Z1 - Z0) / 10.f), 0.7f);
			}
		}), L::GrpStatic);
		// Her bare arms: garnet vein-rings round the forearms; a bone sickle in each hand.
		FName HandL, HandR;
		L::SwlHands(Body, HandL, HandR);
		for (int32 Hd = 0; Hd < 2; ++Hd)
		{
			const FName Hand = Hd == 0 ? HandL : HandR;
			const FName Fore = L::SwlForearm(Body, Hd == 0);
			if (Hand.IsNone()) continue;
			const FVector HandAt = Body->GetBoneLocation(Hand);
			const FVector ForeAt = Fore.IsNone() ? HandAt + FVector(0.f, 0.f, 25.f) : Body->GetBoneLocation(Fore);
			const FVector ArmW = (HandAt - ForeAt).GetSafeNormal();
			const FVector Arm = Yaw.UnrotateVector(ArmW.IsNearlyZero() ? FVector(0.f, 0.f, -1.f) : ArmW);
			if (!Fore.IsNone())
			{
				const float Len = FVector::Dist(HandAt, ForeAt);
				AddGroup(Fore, ForeAt, Yaw, Blocks([&](FKit& Kit)
				{
					for (int32 r = 0; r < 4; ++r) Kit.Disc(FSurf::Lit(SwbHex(TEXT("#7A0A14")), 0.8f), Arm * Len * (0.2f + 0.18f * r), Arm, 8.5f - r * 0.6f, 0.9f);
					Kit.Rod(FSurf::Lit(SwbHex(TEXT("#4A0A0F")), 0.4f), Arm * Len * 0.15f + FVector(0.f, 0.f, 3.f), Arm * Len * 0.8f + FVector(0.f, 1.5f, 3.6f), 0.7f);
				}), L::GrpMarks);
			}
			AddGroup(Hand, HandAt, Yaw, Blocks([&](FKit& Kit)
			{
				// The grip along the arm's line, bound in leather; the blade a crescent of polished bone curving forward.
				const FVector Grip0 = Arm * 2.f, Grip1 = Arm * 28.f;
				Kit.Rod(SBone.Dim(0.85f), Grip0, Grip1, 3.2f);
				for (int32 w = 0; w < 5; ++w) Kit.Disc(Leather, FMath::Lerp(Grip0, Grip1, 0.15f + 0.15f * w), Arm, 3.9f, 1.5f);
				const FVector Fw(1.f, 0.f, 0.f);
				FVector Prev = Grip1;
				for (int32 s = 1; s <= 9; ++s)
				{
					const float A = PI * 0.95f * s / 9.f;
					const FVector P = Grip1 + Arm * FMath::Sin(A) * 22.f + Fw * (1.f - FMath::Cos(A)) * 20.f;
					Kit.Rod(SBone, Prev, P, FMath::Lerp(4.2f, 1.6f, s / 9.f), true);
					Kit.Rod(Edge, Prev + Fw * 1.2f, P + Fw * 1.2f, 0.7f);
					Prev = P;
				}
				Kit.Spike(SBone, Prev, (Prev - Grip1).GetSafeNormal() * -1.f + Fw * 0.3f, 1.6f, 5.f);
			}), L::GrpSickle);
		}
	}
	else if (C->CreatureId == TEXT("bloat"))
	{
		// A Gorgeleech swollen to 6 m: the skin stretched thin enough that the stolen blood shows (the tint's glow, by
		// its gorge), ring annuli, the stripes pulled wide apart, ten eyes in a crescent, the Y of three jaws in the
		// front sucker, three veins standing proud on its flanks.
		const FSurf Band = FSurf::Glaze(SwbHex(TEXT("#1E1612")), 0.2f);
		const FSurf Stripe = FSurf::Glaze(SwbHex(TEXT("#8A3B1E")), 0.25f);
		const FSurf EyeS = FSurf::Glaze(SwbHex(TEXT("#050505")), 0.05f);
		const FSurf Jaw = FSurf::Glaze(SwbHex(TEXT("#C9B49A")), 0.3f);
		const FSurf Rim = FSurf::Glaze(SwbHex(TEXT("#3A2A22")), 0.2f);
		const FSurf SwellS = FSurf::Glaze(SwbHex(TEXT("#2B1A18")), 0.15f);
		const float D = FMath::Max(20.f, BodyH * 0.95f);
		TArray<TPair<FName, FVector>> Segs = BonesWith(TEXT("spine"));
		Segs.Sort([&](const TPair<FName, FVector>& A, const TPair<FName, FVector>& B) { return FVector::DistSquared(A.Value, Head) < FVector::DistSquared(B.Value, Head); });
		for (int32 s = 0; s < Segs.Num(); ++s)
		{
			FVector At = Segs[s].Value;
			At.Z = FloorZ + BodyH * 0.5f;
			AddGroup(Segs[s].Key, At, Yaw, Blocks([&](FKit& Kit)
			{
				for (int32 r = 0; r < 4; ++r) Kit.Disc(Band, FVector(-Rad * 0.12f * r, 0.f, 0.f), FVector(1.f, 0.f, 0.f), D * 1.03f, 2.6f);
				for (const float Side : { -1.f, 1.f })
				{
					Kit.Add(Stripe, FVector(Rad * 0.26f, D * 0.07f, 1.4f), FVector(-Rad * 0.15f, Side * D * 0.28f, D * 0.45f), FRotator(0.f, 0.f, Side * 32.f));
					Kit.Add(Stripe.Dim(0.85f), FVector(Rad * 0.2f, D * 0.05f, 1.2f), FVector(-Rad * 0.15f, Side * D * 0.47f, D * 0.18f), FRotator(0.f, 0.f, Side * 70.f));
				}
			}), L::GrpStatic);
		}
		AddGroup(HeadBone, Head, Yaw, Blocks([&](FKit& Kit)
		{
			// Ten eyes in a crescent over the head.
			for (int32 e = 0; e < 10; ++e)
			{
				const float A = FMath::DegreesToRadians(-70.f + 140.f * e / 9.f);
				const FVector P(Snout * 0.45f + FMath::Cos(A) * D * 0.12f, FMath::Sin(A) * D * 0.28f, D * 0.36f + FMath::Cos(A) * D * 0.04f);
				Kit.Disc(EyeS, P, FVector(0.4f, FMath::Sin(A), 0.8f), D * 0.045f, D * 0.02f);
			}
		}), L::GrpStatic);
		const FVector Mouth = FrontAt(FloorZ + BodyH * 0.45f);
		AddGroup(HeadBone, Mouth, Yaw, Blocks([&](FKit& Kit)
		{
			Kit.Disc(Rim, FVector(-2.f, 0.f, 0.f), FVector(1.f, 0.f, 0.f), D * 0.5f, 8.f);
			for (int32 j = 0; j < 3; ++j)
			{
				const float A = 90.f + 120.f * j;
				const FVector Out(0.f, FMath::Cos(FMath::DegreesToRadians(A)), FMath::Sin(FMath::DegreesToRadians(A)));
				Kit.Add(Jaw, FVector(D * 0.05f, D * 0.04f, D * 0.17f), Out * D * 0.1f + FVector(3.f, 0.f, 0.f), FRotationMatrix::MakeFromZ(Out).Rotator());
				for (int32 t = 0; t < 5; ++t) Kit.Spike(Jaw.Dim(0.95f), Out * D * (0.03f + 0.035f * t) + FVector(5.f, 0.f, 0.f), FVector(1.f, 0.f, 0.f), D * 0.015f, D * 0.03f);
			}
		}), L::GrpDisc);
		// The veins (its part points, as the server finds them): a raised swelling with the vein winding over it; and
		// the torn flap each leaves when it ruptures.
		const TArray<int32> Veins = L::SwlVeinParts(Def);
		for (int32 k = 0; k < Veins.Num(); ++k)
		{
			const FWRCreatureDef::FBodyPoint& P = Def->Parts[Veins[k]];
			const FVector At = L::SwlPartPoint(C, P);
			const FName VBone = C->FindBone(P.Bone);
			const float Side = P.At.Y >= 0.f ? 1.f : -1.f;
			AddGroup(VBone, At, Yaw, Blocks([&](FKit& Kit)
			{
				Kit.Disc(SwellS, FVector::ZeroVector, FVector(0.f, Side, 0.25f), D * 0.32f, D * 0.06f);
				FVector Prev(-D * 0.2f, Side * D * 0.04f, -D * 0.05f);
				for (int32 s = 1; s <= 10; ++s)
				{
					const float T = s / 10.f;
					const FVector Pn(FMath::Lerp(-D * 0.2f, D * 0.2f, T), Side * (D * 0.045f + D * 0.012f * FMath::Sin(T * 9.f)), D * 0.08f * FMath::Sin(T * 6.3f));
					Kit.Rod(FSurf::Lit(SwbHex(TEXT("#7A0A14")), 1.f), Prev, Pn, D * 0.03f * (1.2f - 0.4f * T));
					if (s % 3 == 0) Kit.Rod(FSurf::Lit(SwbHex(TEXT("#7A0A14")), 1.f), Pn, Pn + FVector(D * 0.05f, Side * D * 0.01f, D * 0.07f * (s % 2 ? 1.f : -1.f)), D * 0.014f);
					Prev = Pn;
				}
			}), L::GrpVein0 + k);
			AddGroup(VBone, At, Yaw, Blocks([&](FKit& Kit)
			{
				Kit.Disc(FSurf::Stone(SwbHex(TEXT("#0E0405")), 0.95f), FVector(0.f, Side * D * 0.02f, 0.f), FVector(0.f, Side, 0.2f), D * 0.16f, D * 0.03f);
				for (int32 f = 0; f < 5; ++f)
				{
					const float A = 72.f * f;
					const FVector Dir(FMath::Cos(FMath::DegreesToRadians(A)), 0.f, FMath::Sin(FMath::DegreesToRadians(A)));
					Kit.Add(FSurf::Glaze(SwbHex(TEXT("#4A0A0F")), 0.3f), FVector(D * 0.06f, 1.5f, D * 0.03f), Dir * D * 0.1f + FVector(0.f, Side * D * 0.04f, 0.f), FRotator(A, 0.f, Side * 30.f));
				}
			}), L::GrpTorn0 + k);
		}
		// The blood inside, lit: a garnet light in its belly that fills as it gorges (blazes on the burst's tell).
		GlowLight = NewObject<UPointLightComponent>(C);
		GlowLight->SetupAttachment(Body);
		GlowLight->RegisterComponent();
		GlowLight->SetWorldLocation(C->GetActorLocation() + FVector(0.f, 0.f, H * 0.1f));
		GlowLight->SetLightColor(SwbHex(TEXT("#7A0A14")));
		GlowLight->SetAttenuationRadius(D * 3.f);
		GlowLight->SetIntensity(0.f);
		GlowLight->SetCastShadows(false);
	}
	SetGroupVisible(L::GrpMaskBroken, false);
	SetGroupVisible(L::GrpTorn0, false);
	SetGroupVisible(L::GrpTorn1, false);
	SetGroupVisible(L::GrpTorn2, false);
	BuiltBurst = 0;
	bMaskShown = true;
	UE_LOG(LogWildreach, Verbose, TEXT("SWAMPBOSS look built for %s (%d groups)"), *C->CreatureId.ToString(), Groups.Num());
}

void UWRSwampBossBody::TickLook(float DeltaTime)
{
	namespace L = WRSwbLocal;
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	if (!C)
	{
		return;
	}
	if (C->CreatureId == TEXT("hex_totem"))
	{
		if (!bStandInHidden || LookFor != C->CreatureId) BuildLook();
		return;
	}
	if (FWRSwampBoss::IsVharra(C))
	{
		if (LookFor != C->CreatureId)
		{
			if (Skel() && Skel()->GetSkeletalMeshAsset() && ++LookTries > 2) BuildLook();
			return;
		}
		FWRSwampBoss::VharraTickLook(this, DeltaTime);
		return;
	}
	USkeletalMeshComponent* Body = Skel();
	if (!Body)
	{
		return;
	}
	if (LookFor != C->CreatureId)
	{
		if (Body->GetSkeletalMeshAsset() && Body->IsVisible() && ++LookTries > 2) BuildLook();
		return;
	}
	const bool bAlive = C->IsAlive();
	FxClock += DeltaTime;
	const FWRCreatureDef* Def = C->GetDef();
	const float HH = C->GetSimpleCollisionHalfHeight();
	const FVector Feet = C->GetActorLocation() - FVector(0.f, 0.f, HH);

	if (C->CreatureId == TEXT("mother_maw"))
	{
		// The creature's fit ran again (its id arrived late): her wedge and parts are built again on the new fit.
		if (bBaseSaved && RearAlpha < 0.01f && LashNow == 0.f && !Body->GetRelativeScale3D().Equals(BaseMeshScale, 0.001f))
		{
			BuildLook();
			return;
		}
		// Eyes: the red tapetum in the dark, blazing when she rears or rolls.
		const float Night = AWRGameState::IsNightIn(C) ? 4.f : 0.f;
		const float EyeWant = !bAlive ? 0.f : Has(WRSwampBossFlag::Rearing) || Has(WRSwampBossFlag::Latched) ? 7.f : 0.4f + Night;
		SetGroupGlow(L::GrpEyes, EyeWant);
		// The rear: her front half up out of the water on her tail (belly toward you), and down again.
		RearAlpha = FMath::FInterpTo(RearAlpha, bAlive && Has(WRSwampBossFlag::Rearing) ? 1.f : 0.f, DeltaTime, 5.f);
		const bool bRoll = bAlive && Has(WRSwampBossFlag::Latched);
		if (bRoll)
		{
			// The death roll: over and over about her long axis.
			LashNow = FMath::Fmod(LashNow + DeltaTime * SwbIT(TEXT("mother_maw"), TEXT("rollDegrees"), 400.f), 360.f);
			const FVector Pivot = BaseMeshLoc + FVector(0.f, 0.f, HH * 0.8f);
			const FQuat Q(C->GetActorRotation().UnrotateVector(C->GetActorForwardVector()), FMath::DegreesToRadians(LashNow));
			Body->SetRelativeLocationAndRotation(Pivot + Q.RotateVector(BaseMeshLoc - Pivot), Q * BaseMeshRot.Quaternion());
			if (FxClock > 0.3f)
			{
				FxClock = 0.f;
				WRFx::SpawnAt(C, TEXT("mire.splash"), Feet + FVector(0.f, 0.f, 60.f), FRotator::ZeroRotator, 0.9f);
			}
		}
		else if (bBaseSaved)
		{
			LashNow = 0.f;
			const float Pitch = SwbIT(TEXT("mother_maw"), TEXT("rearPitch"), 32.f) * RearAlpha;
			const FVector Pivot = BaseMeshLoc + C->GetActorRotation().UnrotateVector(-C->GetActorForwardVector()) * C->GetSimpleCollisionRadius() * 1.1f;
			const FQuat Q(C->GetActorRotation().UnrotateVector(C->GetActorRightVector()), -FMath::DegreesToRadians(Pitch));
			Body->SetRelativeLocationAndRotation(Pivot + Q.RotateVector(BaseMeshLoc - Pivot) + FVector(0.f, 0.f, HH * 0.35f * RearAlpha), Q * BaseMeshRot.Quaternion());
		}
		// The mat rides her back: it sways a little as she swims, still when she lies in wait.
		const float Speed = C->GetVelocity().Size2D();
		for (FGroup& G : Groups)
		{
			if (G.Kind != L::GrpMat || !G.Frame.IsValid()) continue;
			G.Frame->SetRelativeLocation(G.BaseLoc + FVector(0.f, 0.f, FMath::Sin(Clock * 1.3f + G.BaseLoc.X * 0.01f) * (1.f + Speed * 0.01f)));
		}
		// In the water and moving: her wake; risen on the planks: water runs off her.
		if (bAlive && FxClock > 0.45f && Has(WRSwampBossFlag::InWater) && Speed > 80.f && !bRoll)
		{
			FxClock = 0.f;
			WRFx::SpawnAt(C, TEXT("mire.wake"), FVector(C->GetActorLocation().X, C->GetActorLocation().Y, FWRSwampData::Get().SeaZ) + C->GetActorForwardVector() * C->GetSimpleCollisionRadius(), C->GetActorRotation(), 2.2f);
		}
		else if (bAlive && FxClock > 0.35f && RearAlpha > 0.5f)
		{
			FxClock = 0.f;
			WRFx::SpawnAt(C, TEXT("swamp.drip"), C->GetActorLocation() + C->GetActorForwardVector() * C->GetSimpleCollisionRadius() * 0.8f + FVector(0.f, 0.f, HH), FRotator::ZeroRotator, 2.f);
		}
	}
	else if (C->CreatureId == TEXT("hex_warden"))
	{
		const bool bScattered = Has(WRSwampBossFlag::Scattered);
		if (bScattered != bWasScattered)
		{
			bWasScattered = bScattered;
			Body->SetVisibility(!bScattered, true);
		}
		const bool bBroken = Has(WRSwampBossFlag::MaskBroken);
		if (bBroken == bMaskShown)
		{
			bMaskShown = !bBroken;
			SetGroupVisible(L::GrpMask, !bBroken);
			SetGroupVisible(L::GrpCast, !bBroken);
			SetGroupVisible(L::GrpMaskBroken, bBroken);
		}
		CastAlpha = FMath::FInterpTo(CastAlpha, bAlive && Has(WRSwampBossFlag::Casting) ? 1.f : 0.f, DeltaTime, 6.f);
		SetGroupGlow(L::GrpCast, 8.f * CastAlpha);
		if (bBroken) SetGroupGlow(L::GrpMaskBroken, 4.f + 1.5f * FMath::Sin(Clock * 3.f));
		// The vein-rings pulse with her heart (and burn brighter when she feeds).
		SetGroupGlow(L::GrpMarks, bAlive ? 0.7f + 0.4f * FMath::Pow(FMath::Abs(FMath::Sin(Clock * 2.6f)), 6.f) + 2.f * CastAlpha : 0.f);
		// The cloak flares into wings when she casts or scatters, and trails out behind her when she runs.
		const float Speed = C->GetVelocity().Size2D();
		CloakAlpha = FMath::FInterpTo(CloakAlpha, bAlive ? FMath::Max(CastAlpha, FMath::Clamp(Speed / 900.f, 0.f, 0.35f)) : 0.f, DeltaTime, 7.f);
		for (FGroup& G : Groups)
		{
			if (G.Kind != L::GrpCloak || !G.Frame.IsValid()) continue;
			G.Frame->SetRelativeScale3D(G.BaseScale * FVector(1.f + 0.25f * CloakAlpha, 1.f + 0.9f * CloakAlpha, 1.f - 0.15f * CloakAlpha));
		}
		// Her Red Tide walk: a garnet haze low round her.
		const FName Want = bAlive && Has(WRSwampBossFlag::HexWalk) && !bScattered ? FName(TEXT("swampboss.hex_haze")) : NAME_None;
		if (Want != LoopFxName)
		{
			if (LoopFx) { WRFx::Stop(LoopFx); LoopFx = nullptr; }
			LoopFxName = Want;
			if (!Want.IsNone()) LoopFx = WRFx::Attach(C->GetRootComponent(), Want, NAME_None, 1.f);
			if (LoopFx) LoopFx->SetRelativeLocation(FVector(0.f, 0.f, -HH + 20.f));
		}
	}
	else if (C->CreatureId == TEXT("bloat"))
	{
		// The gorge on the body (no UI): it swells and the stolen blood shows brighter through the skin.
		const float G = bAlive ? Gorge / 100.f : 0.f;
		GorgeShown = GorgeShown < 0.f ? G : FMath::FInterpTo(GorgeShown, G, DeltaTime, 2.5f);
		GlowAlpha = FMath::FInterpTo(GlowAlpha, bAlive && Has(WRSwampBossFlag::Glowing) ? 1.f : 0.f, DeltaTime, 4.f);
		const float Pump = Has(WRSwampBossFlag::Latched) ? 0.035f * FMath::Sin(Clock * 9.f) : Has(WRSwampBossFlag::Asleep) ? 0.015f * FMath::Sin(Clock * 1.1f) : 0.f;
		if (bBaseSaved && Body->GetSkeletalMeshAsset())
		{
			const float SwellK = SwbIT(TEXT("bloat"), TEXT("swellMax"), 0.35f) * GorgeShown + 0.06f * GlowAlpha + Pump;
			const FVector S = BaseMeshScale * FVector(1.f + SwellK, 1.f + SwellK * 1.1f, 1.f + SwellK * 0.85f);
			const FBox Box = Body->GetSkeletalMeshAsset()->GetBounds().GetBox();
			FVector Lc = BaseMeshLoc;
			Lc.Z -= Box.Min.Z * (S.Z - BaseMeshScale.Z);
			Body->SetRelativeScale3D(S);
			Body->SetRelativeLocation(Lc);
		}
		const float Glow = 0.12f + 3.2f * FMath::Pow(GorgeShown, 1.5f) + GlowAlpha * (9.f + 3.f * FMath::Sin(Clock * 18.f));
		for (UMaterialInstanceDynamic* M : TintMats)
		{
			if (M) M->SetScalarParameterValue(TEXT("GlowStrength"), Glow);
		}
		for (int32 k = 0; k < 3; ++k)
		{
			const bool bTorn = (Burst & (1 << k)) != 0;
			SetGroupVisible(L::GrpVein0 + k, !bTorn);
			SetGroupVisible(L::GrpTorn0 + k, bTorn);
			if (!bTorn) SetGroupGlow(L::GrpVein0 + k, 0.6f + 6.f * GorgeShown + 8.f * GlowAlpha);
		}
		if (Burst != BuiltBurst && GetNetMode() != NM_DedicatedServer)
		{
			BuiltBurst = Burst;
		}
		if (GlowLight)
		{
			GlowLight->SetIntensity(bAlive ? 1500.f + 26000.f * GorgeShown * GorgeShown + 90000.f * GlowAlpha : 0.f);
			GlowLight->SetLightColor(FMath::Lerp(SwbHex(TEXT("#7A0A14")), SwbHex(TEXT("#FF8A8A")), GlowAlpha));
		}
		JawAlpha = FMath::FInterpTo(JawAlpha, bAlive && (Has(WRSwampBossFlag::JawOpen) || Has(WRSwampBossFlag::Latched)) ? 1.f : 0.f, DeltaTime, 9.f);
		for (FGroup& Gr : Groups)
		{
			if (Gr.Kind != L::GrpDisc || !Gr.Frame.IsValid()) continue;
			const float K = FMath::Lerp(0.6f, 1.4f, JawAlpha) + (Has(WRSwampBossFlag::Latched) ? 0.07f * FMath::Sin(Clock * 9.f) : 0.f);
			Gr.Frame->SetRelativeScale3D(Gr.BaseScale * FVector(1.f, K, K));
		}
		// Burning: no latch, and the fat in its skin spits.
		if (bAlive && FxClock > 0.6f && Has(WRSwampBossFlag::Burning))
		{
			FxClock = 0.f;
			WRFx::SpawnAt(C, TEXT("fire.impact"), C->GetActorLocation() + FVector(FMath::FRandRange(-1.f, 1.f) * C->GetSimpleCollisionRadius() * 0.6f, FMath::FRandRange(-1.f, 1.f) * C->GetSimpleCollisionRadius() * 0.6f, HH * 0.6f), FRotator::ZeroRotator, 0.6f);
		}
		else if (bAlive && FxClock > 1.1f && GorgeShown > 0.6f)
		{
			FxClock = 0.f;
			WRFx::SpawnAt(C, TEXT("mire.bloodfeed"), C->GetActorLocation() - C->GetActorForwardVector() * C->GetSimpleCollisionRadius() * 0.5f, FRotator::ZeroRotator, 0.5f);
		}
		(void)Def;
	}
}

// ================================================================================================ arena props

AWRSwampBossProp::AWRSwampBossProp()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRSwampBossProp::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRSwampBossProp, Kind);
	DOREPLIFETIME(AWRSwampBossProp, State);
	DOREPLIFETIME(AWRSwampBossProp, Size);
	DOREPLIFETIME(AWRSwampBossProp, Variant);
	DOREPLIFETIME(AWRSwampBossProp, Rise);
}

void AWRSwampBossProp::BeginPlay()
{
	Super::BeginPlay();
	OnRep_Look();
}

void AWRSwampBossProp::SetState(uint8 NewState)
{
	if (State == NewState && BuiltState == NewState)
	{
		return;
	}
	State = NewState;
	if (Kind == TEXT("walk") && NewState != 0 && Mender.IsValid())
	{
		Mender = nullptr;   // a section broken under a mend starts over
	}
	OnRep_Look();
}

void AWRSwampBossProp::MakeHazard(float InBleedPerSecond, bool bSlow, float Seconds, AActor* Source)
{
	BleedPerSecond = InBleedPerSecond;
	bSlows = bSlow;
	HazardSource = Source;
	HazardClock = 0.f;
	if (Seconds > 0.f) SetLifeSpan(Seconds);
}

bool AWRSwampBossProp::OnDeck(const FVector& Feet) const
{
	if (Kind != TEXT("walk"))
	{
		return false;
	}
	const FVector L = GetActorTransform().InverseTransformPosition(Feet);
	const float HalfW = WRSwbLocal::SwlDeckWidth() * 0.5f;
	return FMath::Abs(L.X) <= Size * 0.5f + 25.f && FMath::Abs(L.Y) <= HalfW + 25.f && L.Z > -60.f && L.Z < 140.f;
}

void AWRSwampBossProp::OnRep_Look()
{
	SetCollision();
	if (GetNetMode() == NM_DedicatedServer || (BuiltKind == Kind && BuiltState == State && BuiltVariant == Variant && FMath::IsNearlyEqual(BuiltSize, Size)))
	{
		return;
	}
	Build();
}

void AWRSwampBossProp::SetCollision()
{
	// A whole or mended deck is walkable; a broken one is a gap you fall through into the water. Mounds are heaps you
	// stand on, a crust holds for its seconds. Totem poles carry no collision: the hex totem's own body is the hit shape.
	for (UBoxComponent* B : Boxes) if (B) B->DestroyComponent();
	Boxes.Reset();
	const auto AddBox = [this](const FVector& Ext, const FVector& At, const FRotator& Rot)
	{
		UBoxComponent* B = NewObject<UBoxComponent>(this);
		B->SetBoxExtent(Ext);
		B->SetCollisionProfileName(TEXT("BlockAll"));
		B->SetCanEverAffectNavigation(false);
		B->SetupAttachment(RootComponent);
		B->RegisterComponent();
		B->SetRelativeLocationAndRotation(At, Rot);
		Boxes.Add(B);
	};
	if (Kind == TEXT("walk") && State != 1)
	{
		AddBox(FVector(Size * 0.5f, WRSwbLocal::SwlDeckWidth() * 0.5f, 7.f), FVector(0.f, 0.f, -7.f), FRotator::ZeroRotator);
	}
	else if (Kind == TEXT("crust"))
	{
		AddBox(FVector(Size * 0.7f, Size * 0.7f, 6.f), FVector(0.f, 0.f, -2.f), FRotator::ZeroRotator);
	}
	else if (Kind == TEXT("mound"))
	{
		// The heap stands Rise high (cm): a low walkable box over its core.
		const float Heap = Rise > 0.f ? Rise : Size * 0.3f;
		AddBox(FVector(Size * 0.6f, Size * 0.6f, Heap * 0.4f), FVector(0.f, 0.f, Heap * 0.4f), FRotator::ZeroRotator);
	}
}

void AWRSwampBossProp::Build()
{
	using namespace WRLookKit;
	for (USceneComponent* C : LookComps) if (C) C->DestroyComponent();
	LookComps.Reset();
	for (USceneComponent* F : FxComps) if (F) WRFx::Stop(F);
	FxComps.Reset();
	if (LoopFx) { WRFx::Stop(LoopFx); LoopFx = nullptr; }
	if (PropLight) { PropLight->DestroyComponent(); PropLight = nullptr; }
	BuiltKind = Kind;
	BuiltState = State;
	BuiltVariant = Variant;
	BuiltSize = Size;
	const float SeaLocal = FWRSwampData::Get().SeaZ - GetActorLocation().Z;   // the water's surface in this prop's space
	TArray<FWRLookBlock> Blocks, Glow;
	FKit Kit(GetActorTransform(), Blocks);
	FKit GlowKit(GetActorTransform(), Glow);
	const FSurf OldWood = FSurf::Wood(SwbHex(TEXT("#4A3A2A")));
	const FSurf Grey = FSurf::Wood(SwbHex(TEXT("#5E5548")));
	const FSurf NewWood = FSurf::Wood(SwbHex(TEXT("#9C7A52")));
	const FSurf NewWood2 = FSurf::Wood(SwbHex(TEXT("#A88A60")));
	const FSurf SCypress = FSurf::Wood(SwbHex(TEXT("#3E3428")));
	const FSurf SRope = FSurf::Cloth(SwbHex(TEXT("#5A4A34")));
	const FSurf Algae = FSurf::Cloth(SwbHex(TEXT("#28351C")));
	const FSurf SMoss = FSurf::Cloth(SwbHex(TEXT("#4E6A2A")));
	const FSurf Fungus = FSurf::Stone(SwbHex(TEXT("#B79C6A")), 0.8f);
	const FSurf SBone = FSurf::Glaze(SwbHex(TEXT("#D8D2C4")), 0.6f);
	const FSurf Garnet = FSurf::Glaze(SwbHex(TEXT("#7A0A14")), 0.3f);
	const FSurf Oxblood = FSurf::Glaze(SwbHex(TEXT("#4A0A0F")), 0.2f);
	const FSurf Mud = FSurf::Stone(SwbHex(TEXT("#2E241A")), 0.95f);

	if (Kind == TEXT("walk"))
	{
		// One section of Mother Maw's square: cypress posts sunk into the pool bed in pairs, a stringer along each side
		// lashed to them, cross-braced, and the deck of planks laid across. Old wood is grey-brown with moss and shelf
		// fungus on the stringers; broken, the planks are gone from the middle, the stringers snapped and hanging into
		// the water; mended, new pale planks lashed over the old frame.
		const float HalfL = Size * 0.5f, HalfW = WRSwbLocal::SwlDeckWidth() * 0.5f;
		const bool bBroken = State == 1, bMended = State == 2;
		const bool bGapPlus = (Variant & 4) != 0, bGapMinus = (Variant & 8) != 0;
		const float Bottom = -FMath::Max(80.f, Rise) - 70.f;   // posts go into the mud
		const int32 Bays = FMath::Max(2, FMath::RoundToInt(Size / 190.f));
		for (int32 p = 0; p <= Bays; ++p)
		{
			const float X = -HalfL + Size * p / Bays;
			for (const float Side : { -1.f, 1.f })
			{
				const FVector Top(X, Side * (HalfW - 18.f), -16.f);
				const FVector Foot(X + Kit.Jit(Top, 1) * 8.f, Side * (HalfW - 18.f) + Kit.Jit(Top, 2) * 8.f, Bottom);
				Kit.Rod(SCypress, Foot, Top, 18.f + 4.f * Kit.Rand(Top, 3));
				Kit.Disc(SCypress.Dim(1.2f), Top + FVector(0.f, 0.f, 2.f), FVector::UpVector, 17.f, 3.f);
				// The water line: a dark band of algae where the surface sits on every post.
				if (SeaLocal > Bottom && SeaLocal < -10.f) Kit.Disc(Algae, FVector(Foot.X, Foot.Y, SeaLocal), FVector::UpVector, 21.f, 14.f);
				Kit.Disc(SRope, Top + FVector(0.f, 0.f, -10.f), FVector::UpVector, 21.f, 4.f);
			}
			// Cross braces on every other bay, under the deck.
			if (p < Bays && p % 2 == 0)
			{
				const float X1 = -HalfL + Size * (p + 1) / Bays;
				const float Low = FMath::Max(Bottom + 60.f, SeaLocal - 40.f);
				for (const float Side : { -1.f, 1.f })
				{
					Kit.Rod(Grey, FVector(X, Side * (HalfW - 18.f), -30.f), FVector(X1, Side * (HalfW - 18.f), Low), 7.f);
					Kit.Rod(Grey.Dim(0.9f), FVector(X, Side * (HalfW - 18.f), Low), FVector(X1, Side * (HalfW - 18.f), -30.f), 7.f);
				}
			}
		}
		// The stringers.
		for (const float Side : { -1.f, 1.f })
		{
			const float Y = Side * (HalfW - 18.f);
			if (!bBroken)
			{
				Kit.Add(OldWood, FVector(Size + 20.f, 14.f, 20.f), FVector(0.f, Y, -18.f));
				for (int32 f = 0; f < Bays; ++f)
				{
					const FVector P(-HalfL + Size * (f + 0.5f) / Bays, Y + Side * 9.f, -14.f);
					if (!bMended && Kit.Rand(P, 4) < 0.6f) Kit.Disc(Fungus, P, FVector(0.f, Side, 0.15f), 14.f + 10.f * Kit.Rand(P, 5), 4.f);
				}
			}
			else
			{
				// Snapped near the middle: each half sags down into the water, the ends splintered.
				const float Snap = Kit.Jit(FVector(0.f, Y, 0.f), 6) * Size * 0.15f;
				const FVector A0(-HalfL - 10.f, Y, -18.f), A1(Snap - 20.f, Y + Side * 6.f, -18.f - FMath::Min(160.f, Rise * 0.8f));
				const FVector B0(HalfL + 10.f, Y, -18.f), B1(Snap + 20.f, Y - Side * 4.f, -18.f - FMath::Min(130.f, Rise * 0.7f));
				Kit.Rod(OldWood, A0, A1, 16.f, true);
				Kit.Rod(OldWood, B0, B1, 16.f, true);
				for (int32 sp = 0; sp < 4; ++sp)
				{
					Kit.Spike(OldWood.Dim(1.15f), A1, (A1 - A0).GetSafeNormal() + FVector(Kit.Jit(A1, sp) * 0.4f, Kit.Jit(A1, sp + 9) * 0.4f, 0.2f), 4.f, 12.f + 6.f * sp);
					Kit.Spike(OldWood.Dim(1.15f), B1, (B1 - B0).GetSafeNormal() + FVector(Kit.Jit(B1, sp) * 0.4f, Kit.Jit(B1, sp + 9) * 0.4f, 0.2f), 4.f, 12.f + 6.f * sp);
				}
			}
		}
		// The deck.
		const float Pitch = 24.f;
		const int32 N = FMath::Max(4, FMath::FloorToInt(Size / Pitch));
		for (int32 i = 0; i < N; ++i)
		{
			const float X = -HalfL + Pitch * (i + 0.5f);
			const float T = (i + 0.5f) / N;
			if (bBroken && T > 0.18f && T < 0.82f) continue;
			const bool bEndSnap = (bGapPlus && T > 0.94f) || (bGapMinus && T < 0.06f);
			if (bEndSnap) continue;
			const FVector P(X, Kit.Jit(FVector(X, 0.f, 0.f), 7) * 2.f, -3.f);
			const FSurf& S = bMended ? (i % 2 ? NewWood : NewWood2) : (Kit.Rand(P, 8) < 0.4f ? Grey : OldWood);
			const float Len = WRSwbLocal::SwlDeckWidth() - (Kit.Rand(P, 9) < 0.25f && !bMended ? 18.f : 4.f);
			Kit.Add(S, FVector(Pitch - 2.f, Len, 6.f), P, FRotator(0.f, Kit.Jit(P, 10) * 1.2f, Kit.Jit(P, 11) * 1.f));
			if (!bMended && Kit.Rand(P, 12) < 0.18f) Kit.Disc(SMoss, P + FVector(0.f, Kit.Jit(P, 13) * HalfW * 0.6f, 3.4f), FVector::UpVector, 20.f + 20.f * Kit.Rand(P, 14), 1.f);
			if (bMended) for (const float Side : { -1.f, 1.f }) Kit.Disc(SRope, P + FVector(0.f, Side * (HalfW - 18.f), 1.f), FVector(1.f, 0.f, 0.f), 7.f, 3.f);   // lashed down
		}
		if (bBroken)
		{
			// Planks hanging from what's left into the water, and two floating in it.
			for (int32 h = 0; h < 4; ++h)
			{
				const float X = (h < 2 ? -HalfL * 0.62f : HalfL * 0.62f) + Kit.Jit(FVector(float(h), 0.f, 0.f), 15) * 30.f;
				const float Y = Kit.Jit(FVector(float(h), 1.f, 0.f), 16) * HalfW * 0.6f;
				const float Drop = FMath::Min(Rise, 140.f);
				Kit.Add(OldWood, FVector(22.f, 6.f, Drop + 40.f), FVector(X + (h < 2 ? 30.f : -30.f), Y, -Drop * 0.5f), FRotator(0.f, 0.f, h < 2 ? -28.f : 28.f) + FRotator(0.f, 90.f, 0.f));
			}
			for (int32 f = 0; f < 2; ++f)
			{
				const FVector P(Kit.Jit(FVector(float(f), 2.f, 0.f), 17) * HalfL * 0.4f, Kit.Jit(FVector(float(f), 3.f, 0.f), 18) * HalfW * 1.4f, SeaLocal + 2.f);
				Kit.Add(OldWood.Dim(0.85f), FVector(22.f, WRSwbLocal::SwlDeckWidth() * 0.8f, 5.f), P, FRotator(0.f, 30.f + 60.f * f, 0.f));
			}
		}
		// The ends where the square has its gaps: the boardwalk ends in a snapped stub over the water.
		for (int32 e = 0; e < 2; ++e)
		{
			if (!(e == 0 ? bGapPlus : bGapMinus)) continue;
			const float Ex = e == 0 ? HalfL - 12.f : -HalfL + 12.f, Out = e == 0 ? 1.f : -1.f;
			for (const float Side : { -1.f, 1.f }) Kit.Spike(OldWood.Dim(1.1f), FVector(Ex, Side * HalfW * 0.5f, -3.f), FVector(Out, Side * 0.2f, 0.3f), 6.f, 22.f);
			Kit.Add(OldWood, FVector(22.f, 6.f, 110.f), FVector(Ex + Out * 18.f, HalfW * 0.3f, -50.f), FRotator(0.f, 90.f, Out * 25.f));
		}
	}
	else if (Kind == TEXT("nest"))
	{
		// Mother Maw's nest on the compost mound (the mound is the land's): its crown of rotting reeds heaped over the
		// clutch, five eggs half-buried in the warm hollow, dry sheaves round the rim. Lit, it burns and smokes.
		const FSurf ReedDry = FSurf::Cloth(SwbHex(TEXT("#8A7A4E")));
		const FSurf ReedRot = FSurf::Cloth(SwbHex(TEXT("#4A4030")));
		const FSurf SChar = FSurf::Stone(SwbHex(TEXT("#1A1612")), 0.95f);
		const FSurf Egg = FSurf::Glaze(SwbHex(TEXT("#E6DFCC")), 0.55f);
		const bool bBurning = State == 1;
		for (int32 s = 0; s < 22; ++s)
		{
			const float A = s * 2.39996f;
			const float R = 120.f + 300.f * FMath::Sqrt((s + 0.5f) / 22.f);
			const FVector Base(FMath::Cos(A) * R, FMath::Sin(A) * R, WRSwbLocal::SwlMoundZ(R) - 6.f);
			const FVector Lean = FVector(FMath::Cos(A), FMath::Sin(A), 1.8f).GetSafeNormal();
			for (int32 k = 0; k < 5; ++k)
			{
				const FVector Off(Kit.Jit(Base, k) * 10.f, Kit.Jit(Base, k + 5) * 10.f, 0.f);
				Kit.Rod(bBurning ? SChar : (k % 2 ? ReedDry : ReedRot), Base + Off, Base + Off + (Lean + FVector(Kit.Jit(Base, k + 9) * 0.3f, Kit.Jit(Base, k + 13) * 0.3f, 0.f)) * (55.f + 25.f * Kit.Rand(Base, k)), 2.2f);
			}
			Kit.Disc(SRope, Base + Lean * 22.f, Lean, 13.f, 3.f);
		}
		const float TopZ = WRSwbLocal::SwlMoundZ(0.f);
		Kit.Disc(Mud, FVector(0.f, 0.f, TopZ - 8.f), FVector::UpVector, 150.f, 8.f);
		for (int32 e = 0; e < 5; ++e)
		{
			const float A = 1.25f * e;
			const FVector P(FMath::Cos(A) * 34.f, FMath::Sin(A) * 34.f, TopZ - 2.f);
			Kit.Add(Egg, FVector(15.f, 15.f, 22.f), P, FRotator(70.f, A * 57.f, 0.f), EWRLookMesh::Cylinder);
		}
		if (bBurning)
		{
			for (int32 f = 0; f < 5; ++f)
			{
				const float A = 2.f * PI * f / 5.f + 0.4f;
				const float R = f == 0 ? 0.f : 230.f;
				if (USceneComponent* Fire = WRFx::Attach(RootComponent, TEXT("hearth.fire"), NAME_None, f == 0 ? 2.4f : 1.7f))
				{
					Fire->SetRelativeLocation(FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, WRSwbLocal::SwlMoundZ(R) + 30.f));
					FxComps.Add(Fire);
				}
			}
			PropLight = NewObject<UPointLightComponent>(this);
			PropLight->SetupAttachment(RootComponent);
			PropLight->RegisterComponent();
			PropLight->SetRelativeLocation(FVector(0.f, 0.f, TopZ + 160.f));
			PropLight->SetLightColor(FLinearColor(1.f, 0.48f, 0.15f));
			PropLight->SetAttenuationRadius(2400.f);
			PropLight->SetIntensity(60000.f);
			PropLight->SetCastShadows(false);
		}
	}
	else if (Kind == TEXT("totem"))
	{
		// A fetish totem on the Ossuary's shoulder: a lean cypress pole in its socket of set stones, a crossbar near the
		// top, and its fetish: crow (two crows nailed wings-out, crow skulls up the pole), bone (long bones lashed in a
		// lattice, a horned skull), blood (a stretched hide bag stained garnet, runnels down the pole, the bowl at its
		// foot), voice (hollow bone pipes on cords that moan in the wind, a jawbone, a carved mouth). Garnet glints in
		// the skulls' sockets while it stands. Broken, the pole is snapped at a third and the top lies beside it.
		const bool bBroken = State == 1;
		const float Tall = Size;
		const float Cut = bBroken ? Tall * 0.36f : Tall;
		const FSurf Pole = FSurf::Wood(SwbHex(TEXT("#2F2820")));
		const FSurf Feather = FSurf::Glaze(SwbHex(TEXT("#0E0D10")), 0.6f);
		const FSurf Hide = FSurf::Stone(SwbHex(TEXT("#6A4A3A")), 0.8f);
		const FSurf Stain = FSurf::Glaze(SwbHex(TEXT("#4A0A0F")), 0.25f);
		const FSurf SStone = FSurf::Stone(SwbHex(TEXT("#3A3A38")), 0.9f);
		Kit.Rod(Pole, FVector(0.f, 0.f, -70.f), FVector(4.f, 2.f, Cut), 24.f);
		for (int32 w = 0; w < 5; ++w) Kit.Disc(SRope, FVector(1.f, 0.5f, 30.f + w * 55.f), FVector::UpVector, 27.f, 4.f);
		if (bBroken)
		{
			for (int32 sp = 0; sp < 5; ++sp) Kit.Spike(Pole.Dim(1.2f), FVector(4.f, 2.f, Cut), FVector(Kit.Jit(FVector(float(sp)), 1) * 0.5f, Kit.Jit(FVector(float(sp)), 2) * 0.5f, 1.f), 6.f, 10.f + 8.f * sp);
		}
		// The top half (standing, or lying where it fell).
		const FTransform TopT = bBroken ? FTransform(FRotator(-82.f, 35.f, 0.f), FVector(60.f, 30.f, 18.f)) : FTransform(FVector(4.f, 2.f, 0.f));
		const auto Up = [&](const FVector& L) { return TopT.TransformPosition(L - FVector(0.f, 0.f, bBroken ? Cut : 0.f)); };
		const auto UpDir = [&](const FVector& D) { return TopT.TransformVectorNoScale(D); };
		if (bBroken) Kit.Rod(Pole, Up(FVector(0.f, 0.f, Cut)), Up(FVector(0.f, 0.f, Tall)), 20.f);
		const float BarZ = Tall * 0.8f;
		Kit.Rod(Pole.Dim(0.9f), Up(FVector(0.f, -70.f, BarZ)), Up(FVector(0.f, 70.f, BarZ)), 12.f);
		Kit.Disc(SRope, Up(FVector(0.f, 0.f, BarZ)), UpDir(FVector(1.f, 0.f, 0.f)), 18.f, 6.f);
		const float GlowOn = bBroken ? 0.f : 4.f;
		if (Variant == 0)
		{
			for (const float Side : { -1.f, 1.f })
			{
				const FVector Body = Up(FVector(4.f, Side * 50.f, BarZ - 22.f));
				Kit.Disc(Feather, Body, UpDir(FVector(1.f, 0.f, 0.f)), 18.f, 10.f);
				for (int32 f = 0; f < 6; ++f)
				{
					const FVector D = UpDir(FVector(0.1f, Side * (0.4f + 0.12f * f), 0.9f - 0.3f * f).GetSafeNormal());
					Kit.Add(Feather, FVector(2.f, 9.f, 36.f + 6.f * f), Body + D * (22.f + 3.f * f), FRotationMatrix::MakeFromZ(D).Rotator());
				}
				WRSwbLocal::SwlCrowSkull(Kit, &GlowKit, Body + UpDir(FVector(8.f, 0.f, 10.f)), FMath::RadiansToDegrees(FMath::Atan2(UpDir(FVector(1.f, 0.f, 0.f)).Y, UpDir(FVector(1.f, 0.f, 0.f)).X)), 1.1f, GlowOn);
			}
			for (int32 s = 0; s < 6; ++s)
			{
				const float Z = 60.f + (BarZ - 80.f) * s / 5.f;
				const float A = 60.f * s;
				const FVector Off = UpDir(FVector(FMath::Cos(FMath::DegreesToRadians(A)) * 14.f, FMath::Sin(FMath::DegreesToRadians(A)) * 14.f, 0.f));
				if (bBroken && Z < Cut) WRSwbLocal::SwlCrowSkull(Kit, nullptr, FVector(0.f, 0.f, Z) + FVector(FMath::Cos(FMath::DegreesToRadians(A)) * 14.f, FMath::Sin(FMath::DegreesToRadians(A)) * 14.f, 0.f), A, 1.f, 0.f);
				else WRSwbLocal::SwlCrowSkull(Kit, &GlowKit, Up(FVector(0.f, 0.f, Z)) + Off, A + (bBroken ? 35.f : 0.f), 1.f, GlowOn * 0.6f);
			}
			WRSwbLocal::SwlCrowSkull(Kit, &GlowKit, Up(FVector(6.f, 0.f, Tall + 8.f)), bBroken ? 35.f : 0.f, 2.2f, GlowOn);
		}
		else if (Variant == 1)
		{
			// Long bones lashed in diamonds round the pole, a horned skull on the top, finger-bone rattles.
			for (int32 b = 0; b < 8; ++b)
			{
				const float Z = 40.f + (BarZ - 60.f) * b / 8.f;
				const float A = 90.f * b;
				const FVector D = FRotator(0.f, A, 0.f).Vector();
				Kit.Rod(SBone, Up(FVector(0.f, 0.f, Z) + D * 16.f), Up(FVector(0.f, 0.f, Z + 60.f) + D.RotateAngleAxis(90.f, FVector::UpVector) * 16.f), 5.f);
				Kit.Disc(SBone.Dim(1.05f), Up(FVector(0.f, 0.f, Z) + D * 16.f), UpDir(D), 8.f, 6.f);
			}
			const FVector Sk = Up(FVector(4.f, 0.f, Tall + 14.f));
			Kit.Add(SBone, FVector(34.f, 26.f, 24.f), Sk, FRotator(0.f, 0.f, 0.f));
			Kit.Spike(SBone, Sk + UpDir(FVector(14.f, 0.f, -6.f)), UpDir(FVector(1.f, 0.f, -0.3f)), 16.f, 26.f);
			for (const float Side : { -1.f, 1.f })
			{
				Kit.Spike(SBone.Dim(0.85f), Sk + UpDir(FVector(-4.f, Side * 12.f, 8.f)), UpDir(FVector(-0.3f, Side * 1.f, 0.7f)), 8.f, 46.f);
				GlowKit.Disc(FSurf::Lit(SwbHex(TEXT("#7A0A14")), GlowOn), Sk + UpDir(FVector(15.f, Side * 7.f, 4.f)), UpDir(FVector(1.f, Side * 0.4f, 0.f)), 4.f, 1.f);
			}
			for (int32 r = 0; r < 7; ++r)
			{
				const float Y = -60.f + 20.f * r;
				Kit.Rod(SRope, Up(FVector(0.f, Y, BarZ - 4.f)), Up(FVector(0.f, Y, BarZ - 30.f)), 0.8f);
				Kit.Rod(SBone, Up(FVector(0.f, Y, BarZ - 30.f)), Up(FVector(0.f, Y + 1.f, BarZ - 42.f)), 2.2f);
			}
		}
		else if (Variant == 2)
		{
			// The blood bag: a hide stretched over a frame and hung from the bar, stained, dripping; runnels down the pole
			// to a stone bowl black with old blood.
			const FVector Bag = Up(FVector(10.f, 0.f, BarZ - 50.f));
			Kit.Add(Hide, FVector(40.f, 46.f, 60.f), Bag, FRotator(0.f, 0.f, 0.f), EWRLookMesh::Cylinder);
			Kit.Add(Stain, FVector(42.f, 48.f, 30.f), Bag + UpDir(FVector(0.f, 0.f, -18.f)), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			for (const float Side : { -1.f, 1.f }) Kit.Rod(SRope, Bag + UpDir(FVector(0.f, Side * 18.f, 28.f)), Up(FVector(0.f, Side * 30.f, BarZ)), 1.2f);
			for (int32 r = 0; r < 6; ++r)
			{
				const float A = 60.f * r + 15.f;
				const FVector D = FRotator(0.f, A, 0.f).Vector();
				const float Top = BarZ - 10.f - 20.f * Kit.Rand(D, 1);
				const float Bot = 10.f + 60.f * Kit.Rand(D, 2);
				Kit.Add(Stain, FVector(5.f, 1.5f, Top - Bot), FVector(0.f, 0.f, (Top + Bot) * 0.5f) + D * 12.5f, FRotator(0.f, A, 0.f));
			}
			GlowKit.Disc(FSurf::Lit(SwbHex(TEXT("#7A0A14")), GlowOn * 0.6f), Bag + UpDir(FVector(21.f, 0.f, -10.f)), UpDir(FVector(1.f, 0.f, 0.f)), 10.f, 1.f);
			Kit.Hex(SStone, 40.f, 0.f, -6.f, 18.f, 26.f);
			Kit.Disc(FSurf::Glaze(SwbHex(TEXT("#140405")), 0.3f), FVector(40.f, 0.f, 19.f), FVector::UpVector, 40.f, 2.f);
		}
		else
		{
			// The voice: hollow bones on cords from the bar (they moan in the wind), a jawbone, bundled throat-skins, and
			// a mouth carved in the pole with garnet in it.
			for (int32 p = 0; p < 9; ++p)
			{
				const float Y = -64.f + 16.f * p;
				const float Len = 26.f + 34.f * Kit.Rand(FVector(Y, 0.f, 0.f), 3);
				Kit.Rod(SRope, Up(FVector(0.f, Y, BarZ - 4.f)), Up(FVector(0.f, Y, BarZ - 20.f)), 0.7f);
				Kit.Rod(SBone, Up(FVector(0.f, Y, BarZ - 20.f)), Up(FVector(0.f, Y, BarZ - 20.f - Len)), 4.5f);
				Kit.Disc(FSurf::Stone(SwbHex(TEXT("#0B0A0A")), 0.9f), Up(FVector(0.f, Y, BarZ - 20.f - Len)), UpDir(FVector::UpVector), 3.f, 0.6f);
			}
			const FVector Jaw = Up(FVector(14.f, 0.f, Tall * 0.55f));
			for (const float Side : { -1.f, 1.f }) Kit.Rod(SBone, Jaw, Jaw + UpDir(FVector(14.f, Side * 16.f, -10.f)), 4.f);
			for (int32 t = 0; t < 6; ++t) Kit.Spike(SBone.Dim(0.95f), Jaw + UpDir(FVector(4.f + 2.f * t, (t - 2.5f) * 4.f, -2.f)), UpDir(FVector(0.f, 0.f, 1.f)), 2.f, 4.f);
			for (int32 s = 0; s < 4; ++s) Kit.Rod(Hide.Dim(0.8f), Up(FVector(-10.f, -8.f + 5.f * s, BarZ - 60.f)), Up(FVector(-12.f, -8.f + 5.f * s, BarZ - 120.f)), 4.f);
			const FVector Mouth = Up(FVector(12.5f, 0.f, Tall * 0.68f));
			Kit.Disc(FSurf::Stone(SwbHex(TEXT("#0B0A0A")), 0.9f), Mouth, UpDir(FVector(1.f, 0.f, 0.f)), 12.f, 3.f);
			GlowKit.Disc(FSurf::Lit(SwbHex(TEXT("#7A0A14")), GlowOn * 0.5f), Mouth + UpDir(FVector(1.f, 0.f, 0.f)), UpDir(FVector(1.f, 0.f, 0.f)), 6.f, 1.f);
		}
	}
	else if (Kind == TEXT("mound"))
	{
		// A heap of tithe-bones: long bones crosswise, skulls in it, settled into the mud, a dark crust where blood ran.
		const float Heap = Rise > 0.f ? Rise : Size * 0.3f;
		for (int32 i = 0; i < 90; ++i)
		{
			const float A = i * 2.39996f;
			const float R = Size * FMath::Sqrt((i + 0.5f) / 90.f);
			const float Z = Heap * FMath::Sqrt(FMath::Max(0.f, 1.f - FMath::Square(R / Size)));
			const FVector P(FMath::Cos(A) * R, FMath::Sin(A) * R, Z);
			const FVector D = FRotator(Kit.Jit(P, 1) * 30.f, Kit.Rand(P, 2) * 360.f, 0.f).Vector();
			if (i % 7 == 0) Kit.Add(SBone, FVector(22.f, 17.f, 18.f), P + FVector(0.f, 0.f, 6.f), FRotator(Kit.Jit(P, 3) * 40.f, Kit.Rand(P, 4) * 360.f, 0.f), EWRLookMesh::Cylinder);
			else Kit.Rod(SBone.Dim(0.8f + 0.25f * Kit.Rand(P, 5)), P - D * 28.f, P + D * 28.f, 4.f + 3.f * Kit.Rand(P, 6));
		}
		Kit.Disc(Mud, FVector(0.f, 0.f, 4.f), FVector::UpVector, Size * 2.f, 8.f);
		Kit.Disc(Oxblood, FVector(Size * 0.2f, -Size * 0.1f, 9.f), FVector::UpVector, Size * 0.7f, 1.f);
	}
	else if (Kind == TEXT("crust"))
	{
		// The clotted crust over the blood flood: plates of black-red clot with cracks of wet red between them.
		const int32 Plates = FMath::Clamp(FMath::RoundToInt(Size / 40.f), 8, 60);
		for (int32 i = 0; i < Plates; ++i)
		{
			const float A = i * 2.39996f;
			const float R = Size * FMath::Sqrt((i + 0.5f) / Plates);
			const FVector P(FMath::Cos(A) * R, FMath::Sin(A) * R, 0.f);
			Kit.Add(FSurf::Stone(SwbHex(TEXT("#2A0508")), 0.75f), FVector(70.f + 30.f * Kit.Rand(P, 1), 60.f + 30.f * Kit.Rand(P, 2), 6.f), P, FRotator(Kit.Jit(P, 3) * 2.f, Kit.Rand(P, 4) * 360.f, Kit.Jit(P, 5) * 2.f));
		}
		Kit.Disc(Garnet, FVector(0.f, 0.f, -2.f), FVector::UpVector, Size * 2.f, 2.f);
	}
	else if (Kind == TEXT("puddle"))
	{
		// A slick of bleedwater (garnet-black, glossy, leech ripples) or a clot (dull black-red): an uneven edge of
		// overlapping pools.
		const bool bClot = Variant == 1;
		const FSurf S = bClot ? FSurf::Stone(SwbHex(TEXT("#2A0508")), 0.7f) : FSurf::Glaze(SwbHex(TEXT("#4A0A0F")), 0.08f);
		Kit.Disc(S, FVector(0.f, 0.f, 2.f), FVector::UpVector, Size * 1.6f, 1.5f);
		for (int32 i = 0; i < 9; ++i)
		{
			const float A = 2.f * PI * i / 9.f + Kit.Jit(FVector(float(i)), 1) * 0.3f;
			const FVector P(FMath::Cos(A) * Size * 0.62f, FMath::Sin(A) * Size * 0.62f, 2.5f);
			Kit.Disc(S.Dim(0.9f + 0.2f * Kit.Rand(P, 2)), P, FVector::UpVector, Size * (0.5f + 0.35f * Kit.Rand(P, 3)), 1.5f);
		}
		if (!bClot)
		{
			if (USceneComponent* Rip = WRFx::Attach(RootComponent, TEXT("swampboss.puddle"), NAME_None, Size / 100.f))
			{
				Rip->SetRelativeLocation(FVector(0.f, 0.f, 4.f));
				FxComps.Add(Rip);
			}
		}
	}
	else if (Kind == TEXT("lair"))
	{
		// The Wallow round its husk (the land's): the mud churned into ridges where the Bloat drags itself in and out,
		// a sheen of its slime over them, the small drained husks of what it fed on, and the red seep of old blood.
		const FSurf Slime = FSurf::Glaze(SwbHex(TEXT("#3A3424")), 0.05f);
		for (int32 r = 0; r < 26; ++r)
		{
			const float A = r * 2.39996f;
			const float R = Size * (0.45f + 0.5f * FMath::Sqrt((r + 0.5f) / 26.f));
			const FVector P(FMath::Cos(A) * R, FMath::Sin(A) * R, 0.f);
			Kit.Add(Mud, FVector(180.f + 120.f * Kit.Rand(P, 1), 60.f + 40.f * Kit.Rand(P, 2), 30.f + 25.f * Kit.Rand(P, 3)), P + FVector(0.f, 0.f, 4.f), FRotator(0.f, FMath::RadiansToDegrees(A) + 90.f, Kit.Jit(P, 4) * 6.f), EWRLookMesh::Cylinder);
			if (r % 3 == 0) Kit.Disc(Slime, P + FVector(0.f, 0.f, 22.f), FVector::UpVector, 90.f + 60.f * Kit.Rand(P, 5), 1.f);
		}
		for (int32 h = 0; h < 7; ++h)
		{
			const float A = 0.9f * h + 0.3f;
			const FVector P(FMath::Cos(A) * Size * 0.7f, FMath::Sin(A) * Size * 0.7f, 6.f);
			Kit.Add(Mud.Dim(1.4f), FVector(40.f, 26.f, 12.f), P, FRotator(0.f, A * 57.f, 0.f));
			for (int32 b = 0; b < 4; ++b) Kit.Rod(SBone.Dim(0.85f), P + FVector(-12.f + 8.f * b, -14.f, 6.f), P + FVector(-12.f + 8.f * b, 14.f, 2.f), 2.f);
		}
		for (int32 s = 0; s < 5; ++s)
		{
			const FVector P(Kit.Jit(FVector(float(s), 4.f, 0.f), 6) * Size * 0.4f, Kit.Jit(FVector(float(s), 5.f, 0.f), 7) * Size * 0.4f, 3.f);
			Kit.Disc(Garnet.Dim(0.7f), P, FVector::UpVector, 50.f + 60.f * Kit.Rand(P, 8), 1.f);
		}
	}
	const auto Spawn = [this](const TArray<FWRLookBlock>& B)
	{
		if (B.Num() == 0) return;
		USceneComponent* Frame = NewObject<USceneComponent>(this);
		Frame->SetupAttachment(RootComponent);
		Frame->RegisterComponent();
		LookComps.Add(Frame);
		FWRBuildLook::Spawn(this, Frame, B, FLinearColor::White, false);
		TArray<USceneComponent*> Kids;
		Frame->GetChildrenComponents(false, Kids);
		for (USceneComponent* Kd : Kids) LookComps.Add(Kd);
	};
	Spawn(Blocks);
	Spawn(Glow);
}

void AWRSwampBossProp::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority())
	{
		// A bleedwater slick or clot: bleed into whoever stands in it, every half second.
		if (Kind == TEXT("puddle") && BleedPerSecond > 0.f)
		{
			HazardClock += DeltaSeconds;
			if (HazardClock >= 0.5f)
			{
				HazardClock -= 0.5f;
				const FVector At = GetActorLocation();
				for (AWRCharacterBase* V : SwbNear(GetWorld(), At, Size * 0.9f, HazardSource.Get()))
				{
					if (FMath::Abs(SwbFeet(V).Z - At.Z) > 160.f || V->GetAttachParentActor()) continue;
					const float Pass = UWRInventoryComponent::GearStatFor(V, TEXT("passBleedwater")) > 0.f || UWRSwampBody::MountPasses(V, TEXT("bleedwater")) ? 0.f
						: WRStorm::HasBuff(V, TEXT("leech_tar")) ? WRTune(TEXT("swampHazards.bleedwater.tarMultiplier"), 0.3f) : 1.f;
					SwbBleed(V, BleedPerSecond * 0.5f * Pass, HazardSource.Get());
					if (bSlows && V->Status) V->Status->ApplyStatus(WRTags::Status_Slowed, 0.7f);
				}
			}
		}
		// A survivor mending a broken section.
		if (Kind == TEXT("walk") && Mender.IsValid())
		{
			FWRSwampBoss::TickMends(GetWorld());
		}
		// The nest burns its 8 s, then smoulders out.
		if (Kind == TEXT("nest") && State == 1 && GetWorld()->GetTimeSeconds() >= BurnUntil)
		{
			SetState(0);
			WRFx::Broadcast(this, TEXT("swamp.smoke"), GetActorLocation() + FVector(0.f, 0.f, WRSwbLocal::SwlMoundZ(0.f) + 40.f), FRotator::ZeroRotator, 2.f);
		}
	}
	if (GetNetMode() != NM_DedicatedServer && PropLight && Kind == TEXT("nest"))
	{
		FxClock += DeltaSeconds;
		PropLight->SetIntensity(52000.f + 14000.f * FMath::Sin(FxClock * 11.f) * FMath::Sin(FxClock * 4.3f));
	}
}

// ================================================================================================ thrown things

AWRSwampShot::AWRSwampShot()
{
	bReplicates = true;
	SetReplicateMovement(false);
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRSwampShot::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRSwampShot, Kind);
	DOREPLIFETIME(AWRSwampShot, Start);
	DOREPLIFETIME(AWRSwampShot, Velocity);
	DOREPLIFETIME(AWRSwampShot, Gravity);
	DOREPLIFETIME(AWRSwampShot, LaunchAt);
}

void AWRSwampShot::BeginPlay()
{
	Super::BeginPlay();
	Last = Start;
	SetActorLocation(Start);
	if (GetNetMode() != NM_DedicatedServer)
	{
		Fx = WRFx::Attach(RootComponent, Kind == TEXT("clot") ? FName(TEXT("swampboss.clot")) : FName(TEXT("swampboss.hexbolt")), NAME_None, 1.f);
	}
	if (HasAuthority())
	{
		SetLifeSpan(12.f);
	}
}

FVector AWRSwampShot::At(float T) const
{
	return FVector(Start) + FVector(Velocity) * T - FVector(0.f, 0.f, 0.5f * Gravity * T * T);
}

void AWRSwampShot::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bDone)
	{
		return;
	}
	// Every machine flies it from its launch on the server's clock: the same arc everywhere, never homing.
	const float T = FMath::Max(0.f, SwbServerTime(this) - LaunchAt);
	const FVector Now = At(T);
	SetActorLocation(Now);
	if (!FVector(Velocity).IsNearlyZero()) SetActorRotation((FVector(Velocity) - FVector(0.f, 0.f, Gravity * T)).Rotation());
	if (!HasAuthority())
	{
		Last = Now;
		return;
	}
	// Damage only where it really meets someone (a sweep of its body along this frame's path) or, for a clot, where it
	// splashes down (Radius round the spot, once each).
	AActor* By = GetInstigator() ? Cast<AActor>(GetInstigator()) : Hit.Instigator.Get();
	FCollisionQueryParams Q(SCENE_QUERY_STAT(WRSwampShot), false, this);
	if (By) Q.AddIgnoredActor(By);
	FHitResult WorldHit;
	const bool bWorld = GetWorld()->LineTraceSingleByChannel(WorldHit, Last, Now, ECC_WorldStatic, Q);
	const FVector End = bWorld ? FVector(WorldHit.ImpactPoint) : Now;
	AWRCharacterBase* Struck = nullptr;
	float Best = TNumericLimits<float>::Max();
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		if (*It == By || !It->IsAlive() || It->IsA<AWRBoss>()) continue;
		if (const AWRCreature* Cr = Cast<AWRCreature>(*It); Cr && !Cr->IsTamed()) continue;
		FVector OnPath, OnBody;
		const FVector Top = It->GetActorLocation() + FVector(0.f, 0.f, It->GetSimpleCollisionHalfHeight());
		const FVector Bot = It->GetActorLocation() - FVector(0.f, 0.f, It->GetSimpleCollisionHalfHeight());
		FMath::SegmentDistToSegmentSafe(Last, End, Bot, Top, OnPath, OnBody);
		const float D = FVector::Dist(OnPath, OnBody);
		if (D < It->GetSimpleCollisionRadius() + (Kind == TEXT("clot") ? 45.f : Radius) && FVector::DistSquared(Last, OnPath) < Best)
		{
			Best = FVector::DistSquared(Last, OnPath);
			Struck = *It;
		}
	}
	const float Flown = FVector::Dist2D(FVector(Start), Now);
	const auto Splash = [this, By](const FVector& Spot, const AActor* Already)
	{
		if (Kind != TEXT("clot")) return;
		for (AWRCharacterBase* V : SwbNear(GetWorld(), Spot, Radius, By))
		{
			if (V == Already || FMath::Abs(SwbFeet(V).Z - Spot.Z) > 220.f) continue;
			FWRHit Wet = Hit;
			Wet.Origin = Spot;
			Wet.ImpactPoint = V->GetActorLocation();
			Wet.bProjectile = true;
			UWRCombatLibrary::ApplyHit(V, Wet);
		}
	};
	if (Struck)
	{
		FWRHit Blow = Hit;
		Blow.Origin = Last;
		Blow.ImpactPoint = Struck->GetActorLocation();
		Blow.bProjectile = true;
		UWRCombatLibrary::ApplyHit(Struck, Blow);
		Splash(SwbFeet(Struck), Struck);
		WRFx::Broadcast(this, Kind == TEXT("clot") ? FName(TEXT("blood.hit")) : FName(TEXT("swampboss.hexbolt_hit")), Struck->GetActorLocation(), FRotator::ZeroRotator, 1.f,
			Kind == TEXT("clot") ? FName(TEXT("blood.hit")) : SwbSound(TEXT("blood.hex"), TEXT("blood.hit")));
		if (OnLanded) OnLanded(SwbFeet(Struck));
		bDone = true;
		Destroy();
		return;
	}
	if (bWorld || Flown > MaxRange || T > 10.f)
	{
		WRFx::Broadcast(this, Kind == TEXT("clot") ? FName(TEXT("blood.boil")) : FName(TEXT("swampboss.hexbolt_hit")), End, FRotator::ZeroRotator, Kind == TEXT("clot") ? Radius / 100.f : 0.8f,
			Kind == TEXT("clot") ? FName(TEXT("blood.boil_pop")) : SwbSound(TEXT("blood.hex"), TEXT("blood.hit")));
		Splash(End, nullptr);
		if (OnLanded) OnLanded(End);
		bDone = true;
		Destroy();
		return;
	}
	Last = Now;
}

// ================================================================================================ the bosses: life

bool FWRSwampBoss::IsSwampBoss(const AWRBoss* Boss)
{
	return Boss && !SwbKind(Boss).IsNone();
}

FWRBossAttack* FWRSwampBoss::FindMove(AWRBoss* Boss, FName Id)
{
	if (!Boss) return nullptr;
	for (FWRBossAttack& Move : Boss->Attacks) if (Move.Id == Id) return &Move;
	return nullptr;
}

bool FWRSwampBoss::StartRow(AWRBoss* Boss, FName Id, AWRCharacterBase* Target)
{
	FWRBossAttack* Move = FindMove(Boss, Id);
	if (!Move || !Target || !Target->IsAlive())
	{
		return false;
	}
	Boss->StartAttack(*Move, Target);
	return true;
}

void FWRSwampBoss::CutMove(AWRBoss* Boss, float Seconds)
{
	const double T = SwbNow(Boss);
	Boss->Current = INDEX_NONE;
	Boss->StrikeState = AWRBoss::EStrike::None;
	Boss->SpeedBoost = 1.f;
	Boss->BusyUntil = FMath::Max(Boss->BusyUntil, T + Seconds);
	Boss->SetCombatTag(WRTags::State_Windup, false);
}

void FWRSwampBoss::BuildRoute(AWRBoss* Boss, FWRSwampBossState& St)
{
	// Its loop from the Wallow (swamp.json arenas.bloat.route), and the Drowned Causeway it walks on the Red Tide.
	St.Route = WRSwamp::Points(WRSwamp::Arena(TEXT("bloat")), TEXT("route"));
	if (St.Route.Num() < 2)
	{
		if (const FWRCreatureDef* Def = Boss->GetDef()) St.Route = Def->Roam;
	}
	St.Causeway.Reset();
	for (const FWRSwampData::FRoute& R : FWRSwampData::Get().Routes)
	{
		if (R.Id == TEXT("drowned_causeway")) { St.Causeway = R.Points; break; }
	}
	if (St.Route.Num() > 0)
	{
		St.RouteIndex = SwbNearestIndex(St.Route, Boss->GetActorLocation());
	}
}

void FWRSwampBoss::EnsureArena(AWRBoss* Boss, FWRSwampBossState& St)
{
	if (!Boss || St.bArenaFound)
	{
		return;
	}
	const double T = SwbNow(Boss);
	if (T < St.NextNestCheck)
	{
		return;
	}
	St.NextNestCheck = T + 2.0;
	UWorld* World = Boss->GetWorld();
	const FName Id = Boss->CreatureId;
	const TSharedPtr<FJsonObject> Row = WRSwamp::Arena(Id);
	if (Id == TEXT("mother_maw"))
	{
		St.NestAt = Row.IsValid() && SwbHasPoint(Row, TEXT("nest")) ? WRSwamp::Point(Row, TEXT("nest")) : Boss->GetHome();
		St.PoolRadius = Row.IsValid() ? float(SwbNum(Row, TEXT("pool"), 30.0)) * 100.f : 3000.f;
		St.NestRadius = SwbST(Boss, TEXT("nestRadius"), 650.f);
		St.Sections.Reset();
		for (TActorIterator<AWRSwampBossProp> It(World); It; ++It)
		{
			if (FVector::Dist2D(It->GetActorLocation(), St.NestAt) > St.PoolRadius + 800.f) continue;
			if (It->Kind == TEXT("walk")) St.Sections.Add(*It);
			else if (It->Kind == TEXT("nest")) St.Nest = *It;
		}
		const TSharedPtr<FJsonObject>* Square = nullptr;
		const float Half = Row.IsValid() && Row->TryGetObjectField(TEXT("square"), Square) ? float(SwbNum(*Square, TEXT("half"), 22.0)) * 100.f : 2200.f;
		St.RestAt = St.NestAt + FVector(0.f, 1.f, 0.f) * (St.NestRadius + (Half - St.NestRadius) * 0.5f);
		St.bArenaFound = St.Sections.Num() > 0;
	}
	else if (Id == TEXT("hex_warden"))
	{
		St.OssuaryAt = SwbCentre(Row);
		if (Row.IsValid() && Row->HasField(TEXT("top"))) St.OssuaryAt.Z = float(SwbNum(Row, TEXT("top"), 0.0)) * 100.f;
		if (St.OssuaryAt.IsZero()) St.OssuaryAt = Boss->GetHome();
		int32 Found = 0;
		for (TActorIterator<AWRSwampBossProp> It(World); It; ++It)
		{
			if (It->Kind == TEXT("totem") && FVector::Dist2D(It->GetActorLocation(), St.OssuaryAt) < 4000.f)
			{
				St.TotemProps[It->Variant & 3] = *It;
				++Found;
			}
		}
		St.bArenaFound = Found > 0;
		if (St.bArenaFound) SpawnTotems(Boss, St);
	}
	else if (Id == TEXT("bloat"))
	{
		BuildRoute(Boss, St);
		St.bArenaFound = St.Route.Num() > 1;
	}
}

void FWRSwampBoss::Begin(AWRBoss* Boss)
{
	if (!IsSwampBoss(Boss) || !Boss->HasAuthority())
	{
		return;
	}
	// The body every machine draws the boss from (Vharra's too: her own look is built on it).
	UWRSwampBossBody::Attach(Boss);
	if (IsVharra(Boss))
	{
		VharraBegin(Boss);
		return;
	}
	FWRSwampBossState& St = SwbS(Boss);
	St.bArenaFound = false;
	St.NextNestCheck = 0.0;
	EnsureArena(Boss, St);
	if (Boss->CreatureId == TEXT("mother_maw"))
	{
		// She comes back to a whole square and a cold nest.
		for (const TWeakObjectPtr<AWRSwampBossProp>& S : St.Sections) if (S.IsValid()) { S->Mender = nullptr; S->SetState(0); }
		if (St.Nest.IsValid()) St.Nest->SetState(0);
	}
	else if (Boss->CreatureId == TEXT("bloat"))
	{
		for (int32 k = 0; k < 3; ++k) St.VeinHP[k] = SwbST(Boss, TEXT("veinHealth"), 300.f);
		St.Gorge = 0.f;
		St.NextPreyLook = SwbNow(Boss) + FMath::FRandRange(30.f, 90.f);
		St.NextClashAt = SwbNow(Boss) + 60.0;
	}
	UE_LOG(LogWildreach, Log, TEXT("SWAMPBOSS %s begins (arena %s)"), *Boss->GetDisplayLabel(), St.bArenaFound ? TEXT("found") : TEXT("waiting for its props"));
}

void FWRSwampBoss::Reset(AWRBoss* Boss)
{
	if (IsVharra(Boss))
	{
		VharraReset(Boss);
		return;
	}
	if (!IsSwampBoss(Boss))
	{
		return;
	}
	FWRSwampBossState& St = SwbS(Boss);
	const double T = SwbNow(Boss);
	ReleaseGrab(Boss, St);
	St.Stage = 0;
	St.bWave = false;
	St.bBurnSeen = St.bFrostSeen = St.bStanceSeen = false;
	UWRSwampBossBody* Body = UWRSwampBossBody::Get(Boss);
	if (Boss->CreatureId == TEXT("mother_maw"))
	{
		// The square stands whole again, the nest goes out, her young go back into the reeds.
		for (const TWeakObjectPtr<AWRSwampBossProp>& S : St.Sections) if (S.IsValid()) { S->Mender = nullptr; S->SetState(0); }
		if (St.Nest.IsValid()) { St.Nest->BurnUntil = 0.0; St.Nest->SetState(0); }
		for (const TWeakObjectPtr<AWRCreature>& Y : St.Young) if (Y.IsValid()) Y->Destroy();
		St.Young.Reset();
		St.bYoung60 = St.bYoung30 = false;
		St.bRearing = St.bRearPunish = false;
		St.BellyUntil = St.DefendUntil = 0.0;
		if (Body) Body->Flags = 0;
	}
	else if (Boss->CreatureId == TEXT("hex_warden"))
	{
		if (St.bScattered)
		{
			Reform(Boss, St);
		}
		TickMimic(Boss, St, true);
		St.bMaskBroken = false;
		St.HitCount = 0;
		St.BleedingNear.Reset();
		if (Body)
		{
			Body->Set(WRSwampBossFlag::MaskBroken, false);
			Body->Set(WRSwampBossFlag::Casting, false);
			Body->Set(WRSwampBossFlag::Scattered, false);
		}
		SpawnTotems(Boss, St);
		if (!St.bWalking && !St.OssuaryAt.IsZero())
		{
			Boss->SetHome(St.OssuaryAt);
			Boss->bReturning = FVector::Dist2D(Boss->GetActorLocation(), St.OssuaryAt) > 300.f;
			Boss->ReturnStarted = T;
		}
	}
	else if (Boss->CreatureId == TEXT("bloat"))
	{
		// Pulled off its loop (or left alone): its veins close, its gorge stays what it drank, and it goes back to the
		// nearest point of its route (or of the invasion's way).
		for (int32 k = 0; k < 3; ++k) St.VeinHP[k] = SwbST(Boss, TEXT("veinHealth"), 300.f);
		St.bBurstQueued = false;
		if (Body)
		{
			Body->Burst = 0;
			Body->Set(WRSwampBossFlag::Glowing, false);
			Body->Set(WRSwampBossFlag::Latched, false);
			Body->Set(WRSwampBossFlag::JawOpen, false);
		}
		if (St.Route.Num() == 0) BuildRoute(Boss, St);
		const bool bWay = St.InvadeWay.Num() > 0;
		const TArray<FVector>& Way = bWay ? St.InvadeWay : St.Route;
		if (Way.Num() > 0)
		{
			const int32 Near = SwbNearestIndex(Way, Boss->GetActorLocation());
			if (bWay) St.InvadeIndex = Near; else St.RouteIndex = Near;
			Boss->SetHome(FVector(Way[Near].X, Way[Near].Y, Boss->GetActorLocation().Z));
			Boss->bReturning = FVector::Dist2D(Boss->GetActorLocation(), Way[Near]) > 300.f;
			Boss->ReturnStarted = T;
		}
		St.Prey = nullptr;
	}
	UE_LOG(LogWildreach, Log, TEXT("SWAMPBOSS %s reset"), *Boss->GetDisplayLabel());
}

void FWRSwampBoss::EndPlay(AWRBoss* Boss)
{
	if (IsVharra(Boss))
	{
		VharraEndPlay(Boss);
		SwbStates().Remove(Boss);
		return;
	}
	if (FWRSwampBossState* St = SwbStates().Find(Boss))
	{
		if (Boss && Boss->HasAuthority())
		{
			if (St->Grabbed.IsValid()) ReleaseGrab(Boss, *St);
			for (const TWeakObjectPtr<AWRCreature>& C : St->Totems) if (C.IsValid()) C->Destroy();
			if (Boss->CreatureId == TEXT("hex_warden"))
			{
				TickMimic(Boss, *St, true);
				EndHex(Boss->GetWorld());
				if (AWRSwampState* State = AWRSwampState::Get(Boss)) State->bHexWalk = false;
			}
		}
	}
	SwbStates().Remove(Boss);
}

void FWRSwampBoss::Died(AWRBoss* Boss)
{
	if (IsVharra(Boss))
	{
		VharraDied(Boss);
		return;
	}
	if (!IsSwampBoss(Boss))
	{
		return;
	}
	FWRSwampBossState& St = SwbS(Boss);
	ReleaseGrab(Boss, St);
	UWRSwampBossBody* Body = UWRSwampBossBody::Get(Boss);
	if (Boss->CreatureId == TEXT("hex_warden"))
	{
		// Her death ends the hex: every hexed beast's eyes go dark, the totems' curses with her.
		if (St.bScattered) Reform(Boss, St);
		TickMimic(Boss, St, true);
		EndHex(Boss->GetWorld());
		St.bWalking = false;
		if (AWRSwampState* State = AWRSwampState::Get(Boss)) State->bHexWalk = false;
		for (const TWeakObjectPtr<AWRCreature>& C : St.Totems) if (C.IsValid() && C->IsAlive()) C->Die(Boss);
		for (const TWeakObjectPtr<AWRSwampBossProp>& P : St.TotemProps) if (P.IsValid()) P->SetState(1);
		if (Body) Body->Set(WRSwampBossFlag::HexWalk, false);
	}
	else if (Boss->CreatureId == TEXT("bloat"))
	{
		// What it drank runs out of it where it falls.
		if (St.Gorge > SwbST(Boss, TEXT("deathPuddleGorge"), 20.f))
		{
			Puddle(Boss->GetWorld(), TEXT("bleedwater"), SwbFeet(Boss), SwbST(Boss, TEXT("deathPuddleRadius"), 500.f) * (0.5f + St.Gorge / 200.f), SwbST(Boss, TEXT("deathPuddleSeconds"), 40.f), SwbST(Boss, TEXT("burstPuddleBleed"), 10.f), false, Boss);
		}
		WRFx::Broadcast(Boss, TEXT("swampboss.burst"), Boss->GetActorLocation(), FRotator::ZeroRotator, 0.6f + St.Gorge / 100.f, TEXT("swamp.bloat.burst"));
		St.bInvading = false;
		St.InvadeWay.Reset();
	}
	if (Body)
	{
		Body->Set(WRSwampBossFlag::Rearing, false);
		Body->Set(WRSwampBossFlag::Latched, false);
		Body->Set(WRSwampBossFlag::Glowing, false);
		Body->Set(WRSwampBossFlag::Casting, false);
	}
}

// ================================================================================================ out of the fight

bool FWRSwampBoss::TickIdle(AWRBoss* Boss, float DeltaSeconds, bool& bSkipWake)
{
	if (IsVharra(Boss))
	{
		return VharraTickIdle(Boss, DeltaSeconds, bSkipWake);
	}
	if (!IsSwampBoss(Boss))
	{
		return false;
	}
	FWRSwampBossState& St = SwbS(Boss);
	UWRSwampBossBody* Body = UWRSwampBossBody::Get(Boss);
	EnsureArena(Boss, St);
	const double T = SwbNow(Boss);
	if (Boss->CreatureId == TEXT("mother_maw"))
	{
		// She lies in the pool off her nest, a floating island of duckweed; a fire on the nest brings her out.
		const bool bWater = MawInWater(Boss, St);
		if (Body)
		{
			Body->Set(WRSwampBossFlag::InWater, bWater);
			Body->Set(WRSwampBossFlag::Defending, T < St.DefendUntil);
			Body->Set(WRSwampBossFlag::Rearing, false);
		}
		const FVector Out = (Boss->GetActorLocation() - St.NestAt).GetSafeNormal2D();
		const FVector Goal = T < St.DefendUntil ? St.NestAt + (Out.IsNearlyZero() ? FVector(1.f, 0.f, 0.f) : Out) * St.NestRadius * 0.8f : St.RestAt;
		if (!Goal.IsZero() && !Boss->bReturning) SwbWalkTo(Boss, Goal, T < St.DefendUntil ? 900.f : SwbST(Boss, TEXT("idleSpeed"), 160.f), 220.f);
		return true;
	}
	if (Boss->CreatureId == TEXT("hex_warden"))
	{
		return IdleWarden(Boss, St, Body, DeltaSeconds);
	}
	return IdleBloat(Boss, St, Body, DeltaSeconds, bSkipWake);
}

bool FWRSwampBoss::IdleWarden(AWRBoss* Boss, FWRSwampBossState& St, UWRSwampBossBody* Body, float DeltaSeconds)
{
	const double T = SwbNow(Boss);
	UWorld* World = Boss->GetWorld();
	// A totem broken while she's away still counts (it stays down until the fight resets).
	for (int32 i = 0; i < 4; ++i)
	{
		const AWRCreature* C = St.Totems[i].Get();
		if ((St.TotemsDown & (1 << i)) == 0 && C && !C->IsAlive()) TotemBroken(Boss, St, i);
	}
	if (St.bScattered)
	{
		Reform(Boss, St);
	}
	TickMimic(Boss, St, true);
	// The Red Tide's night walk (cook 2.10): with nobody fighting her, she walks the boardwalks from the Thorn Gate to
	// the Tithe Mound and back, and every Bogstalker and Murkjaw within 30 m of her is hexed for the night.
	const TArray<FVector>& Walk = FWRSwampData::Get().HexWalk;
	const bool bWant = Walk.Num() > 1 && WRSwamp::IsTide(Boss) && AWRGameState::IsNightIn(Boss);
	AWRSwampState* State = AWRSwampState::GetOrSpawn(World);
	if (bWant && !St.bWalking)
	{
		St.bWalking = true;
		St.WalkIndex = SwbNearestIndex(Walk, Boss->GetActorLocation());
		St.WalkDir = St.WalkIndex >= Walk.Num() - 1 ? -1 : 1;
		St.NextHexAt = T;
		WRSwbLocal::SwlVoice(Boss, TEXT("signature"));
		UE_LOG(LogWildreach, Log, TEXT("SWAMPBOSS the Hex-Warden leaves her Ossuary on the Red Tide's night walk"));
	}
	else if (!bWant && St.bWalking)
	{
		// Dawn (or the tide's end): the hex lifts and she goes home.
		St.bWalking = false;
		EndHex(World);
		Boss->SetHome(St.OssuaryAt.IsZero() ? Boss->GetHome() : St.OssuaryAt);
		Boss->bReturning = true;
		Boss->ReturnStarted = T;
		UE_LOG(LogWildreach, Log, TEXT("SWAMPBOSS the Hex-Warden goes back to her Ossuary"));
	}
	if (State) State->bHexWalk = St.bWalking;
	if (Body) Body->Set(WRSwampBossFlag::HexWalk, St.bWalking);
	if (!St.bWalking)
	{
		return false;
	}
	// Along the walk at a slow procession's pace, turning at either end; her fight range goes with her.
	St.WalkIndex = FMath::Clamp(St.WalkIndex, 0, Walk.Num() - 1);
	const FVector Goal = Walk[St.WalkIndex];
	if (SwbWalkTo(Boss, Goal, SwbST(Boss, TEXT("walkSpeed"), 220.f), 220.f))
	{
		if ((St.WalkIndex == Walk.Num() - 1 && St.WalkDir > 0) || (St.WalkIndex == 0 && St.WalkDir < 0)) St.WalkDir = -St.WalkDir;
		St.WalkIndex += St.WalkDir;
	}
	Boss->SetHome(Boss->GetActorLocation());
	Boss->bReturning = false;
	if (T >= St.NextHexAt)
	{
		St.NextHexAt = T + SwbST(Boss, TEXT("hexEvery"), 1.f);
		const float Radius = SwbST(Boss, TEXT("hexRadius"), 3000.f);
		const float Seconds = SwbST(Boss, TEXT("hexSeconds"), 900.f);
		for (TActorIterator<AWRCreature> It(World); It; ++It)
		{
			if (!SwbIsHexKind(It->CreatureId) || !It->IsAlive() || It->IsTamed() || FVector::Dist(It->GetActorLocation(), Boss->GetActorLocation()) > Radius) continue;
			const double* Until = SwbHexed().Find(*It);
			if (Until && *Until > T + Seconds * 0.5) continue;
			FWRSwampAI::SetHexed(*It, Seconds);
			SwbHexed().Add(*It, T + Seconds);
			WRFx::Broadcast(World, TEXT("mire.curse"), It->GetActorLocation(), (It->GetActorLocation() - Boss->GetActorLocation()).Rotation(), 0.3f, TEXT("blood.hex"));
		}
	}
	(void)DeltaSeconds;
	return true;
}

bool FWRSwampBoss::IdleBloat(AWRBoss* Boss, FWRSwampBossState& St, UWRSwampBossBody* Body, float DeltaSeconds, bool& bSkipWake)
{
	const double T = SwbNow(Boss);
	UWorld* World = Boss->GetWorld();
	if (St.Route.Num() < 2)
	{
		BuildRoute(Boss, St);
		if (St.Route.Num() < 2) return false;
	}
	const FWRCreatureDef* Def = Boss->GetDef();
	const float Walk = SwbST(Boss, TEXT("walkSpeed"), 150.f);
	const bool bTide = WRSwamp::IsTide(Boss) && St.Causeway.Num() > 1;
	const bool bNight = AWRGameState::IsNightIn(Boss);
	if (Body)
	{
		Body->Set(WRSwampBossFlag::Burning, Boss->HasTag(WRTags::Status_Burn));
		Body->Set(WRSwampBossFlag::Glowing, false);
		Body->Set(WRSwampBossFlag::JawOpen, false);
	}
	// It gorges as it wades bleedwater (+1% a second), x2 on the Red Tide.
	if (T >= St.NextWadeTick)
	{
		St.NextWadeTick = T + 1.0;
		const FVector Feet = SwbFeet(Boss);
		if (WRSwamp::IsWet(Feet) && WRSwamp::HazardWaterAt(Boss, Feet) == EWRSwampWater::Bleedwater) AddGorge(Boss, St, SwbST(Boss, TEXT("gorgeWade"), 1.f));
	}
	const auto SetAwake = [Boss, Body]()
	{
		if (Boss->IsAsleep()) Boss->SetAsleep(false);
		if (Body) Body->Set(WRSwampBossFlag::Asleep, false);
	};
	// An invasion's way (or the way home from one) comes first.
	if (St.InvadeWay.Num() > 0)
	{
		SetAwake();
		St.InvadeIndex = FMath::Clamp(St.InvadeIndex, 0, St.InvadeWay.Num() - 1);
		const FVector Goal = St.InvadeWay[St.InvadeIndex];
		if (SwbWalkTo(Boss, Goal, SwbST(Boss, TEXT("invadeSpeed"), 260.f), 300.f))
		{
			if (St.InvadeIndex < St.InvadeWay.Num() - 1) ++St.InvadeIndex;
			else if (!St.bInvading) { St.InvadeWay.Reset(); St.RouteIndex = SwbNearestIndex(St.Route, Boss->GetActorLocation()); }
		}
		Boss->SetHome(Boss->GetActorLocation());
		Boss->bReturning = false;
		return true;
	}
	// A clash with Mother Maw when their ways cross: they face off, rear and bellow, and part.
	AWRBoss* Maw = St.ClashWith.Get();
	if (T < St.ClashUntil && Maw && Maw->IsAlive() && !Maw->bEngaged)
	{
		SetAwake();
		const FVector To = Maw->GetActorLocation() - Boss->GetActorLocation();
		Boss->SetActorRotation(FRotator(0.f, To.Rotation().Yaw, 0.f));
		Maw->SetActorRotation(FRotator(0.f, (-To).Rotation().Yaw, 0.f));
		if (To.Size2D() > Boss->GetSimpleCollisionRadius() + Maw->GetSimpleCollisionRadius() + 300.f) SwbWalkTo(Boss, Maw->GetActorLocation(), Walk * 1.5f, 400.f);
		if (T >= St.NextTickAt)
		{
			St.NextTickAt = T + FMath::FRandRange(1.6f, 2.6f);
			AWRBoss* Who = FMath::RandBool() ? Maw : Boss;
			Who->PlaySwing(EWRSwingKind::Heavy, 1.f);
			WRSwbLocal::SwlVoice(Who, TEXT("aggro"));
			WRFx::Broadcast(World, TEXT("mire.splash"), (Boss->GetActorLocation() + Maw->GetActorLocation()) * 0.5f - FVector(0.f, 0.f, Boss->GetSimpleCollisionHalfHeight()), FRotator::ZeroRotator, 1.6f, SwbSound(TEXT("water.splash"), TEXT("stomp")));
		}
		return true;
	}
	if (T >= St.NextClashAt && !bTide)
	{
		St.NextClashAt = T + 5.0;
		AWRBoss* Found = SwbFindBoss(World, TEXT("mother_maw"));
		if (Found && !Found->bEngaged && FVector::Dist2D(Found->GetActorLocation(), Boss->GetActorLocation()) < SwbST(Boss, TEXT("clashRadius"), 2500.f))
		{
			St.ClashWith = Found;
			St.ClashUntil = T + SwbST(Boss, TEXT("clashSeconds"), 8.f);
			St.NextClashAt = T + SwbST(Boss, TEXT("clashEvery"), 300.f);
			UE_LOG(LogWildreach, Log, TEXT("SWAMPBOSS the Bloat meets Mother Maw: they clash"));
			return true;
		}
	}
	// Feeding: it latches onto a Mudhulk on its way and drains it (+4% a second); the herd turns on it.
	if (AWRCreature* Prey = St.Prey.Get())
	{
		if (!Prey->IsAlive() || Prey->IsTamed() || T > St.PreyUntil || Boss->HasTag(WRTags::Status_Burn))
		{
			St.Prey = nullptr;
			St.NextPreyLook = T + SwbST(Boss, TEXT("preyEvery"), 120.f);
			if (Body) Body->Set(WRSwampBossFlag::Latched, false);
		}
		else
		{
			SetAwake();
			const float Contact = Boss->GetSimpleCollisionRadius() + Prey->GetSimpleCollisionRadius() + 80.f;
			const FVector To = Prey->GetActorLocation() - Boss->GetActorLocation();
			const bool bOn = To.Size2D() <= Contact;
			if (!bOn) SwbWalkTo(Boss, Prey->GetActorLocation(), Walk * 1.6f, Contact * 0.8f);
			Boss->SetActorRotation(FRotator(0.f, To.Rotation().Yaw, 0.f));
			if (Body) Body->Set(WRSwampBossFlag::Latched, bOn);
			if (bOn)
			{
				AddGorge(Boss, St, SwbST(Boss, TEXT("gorgeLatchCreature"), 4.f) * DeltaSeconds);
				if (T >= St.GrabTickAt)
				{
					St.GrabTickAt = T + 0.5;
					FWRHit Drain;
					Drain.Instigator = Boss;
					Drain.Damage = SwbST(Boss, TEXT("preyDps"), 10.f) * 0.5f;
					Drain.bDamageOverTime = true;
					Drain.Element = WRTags::Element_Physical;
					Drain.Origin = Boss->GetActorLocation();
					Drain.Buildups.Add(TEXT("bleed"), SwbST(Boss, TEXT("latchBleedPerSecond"), 20.f) * 0.5f);
					UWRCombatLibrary::ApplyHit(Prey, Drain);
					Prey->NotifyAttackedBy(Boss);
				}
			}
			return true;
		}
	}
	if (T >= St.NextPreyLook && !bTide && bNight)
	{
		St.NextPreyLook = T + 10.0;
		AWRCreature* Best = nullptr;
		float BestD = SwbST(Boss, TEXT("preyRadius"), 1500.f);
		for (TActorIterator<AWRCreature> It(World); It; ++It)
		{
			if (It->CreatureId != TEXT("mudhulk") || !It->IsAlive() || It->IsTamed()) continue;
			const float D = FVector::Dist2D(It->GetActorLocation(), Boss->GetActorLocation());
			if (D < BestD) { BestD = D; Best = *It; }
		}
		if (Best)
		{
			St.Prey = Best;
			St.PreyUntil = T + SwbST(Boss, TEXT("preySeconds"), 12.f);
			St.GrabTickAt = T + 1.0;
			WRSwbLocal::SwlVoice(Boss, TEXT("tell"));
			UE_LOG(LogWildreach, Log, TEXT("SWAMPBOSS the Bloat goes for a Mudhulk"));
			return true;
		}
	}
	// The Red Tide: it leaves its loop and walks the Drowned Causeway, gorging twice as fast.
	if (bTide)
	{
		SetAwake();
		if (!St.bWasTide)
		{
			St.bWasTide = true;
			St.CausewayIndex = SwbNearestIndex(St.Causeway, Boss->GetActorLocation());
			St.CausewayDir = St.CausewayIndex > St.Causeway.Num() / 2 ? -1 : 1;
		}
		St.CausewayIndex = FMath::Clamp(St.CausewayIndex, 0, St.Causeway.Num() - 1);
		const FVector Goal = St.Causeway[St.CausewayIndex];
		if (SwbWalkTo(Boss, Goal, Walk, 260.f))
		{
			if ((St.CausewayIndex == St.Causeway.Num() - 1 && St.CausewayDir > 0) || (St.CausewayIndex == 0 && St.CausewayDir < 0)) St.CausewayDir = -St.CausewayDir;
			St.CausewayIndex += St.CausewayDir;
		}
		Boss->SetHome(FVector(Goal.X, Goal.Y, Boss->GetActorLocation().Z));
		return true;
	}
	if (St.bWasTide)
	{
		St.bWasTide = false;
		St.RouteIndex = SwbNearestIndex(St.Route, Boss->GetActorLocation());
	}
	const FVector Lair = St.Route[0];
	const bool bAtLair = FVector::Dist2D(Boss->GetActorLocation(), Lair) < SwbST(Boss, TEXT("lairRadius"), 600.f);
	// Nocturnal: by day it sleeps at the Wallow (asleep it notices you late: a sneak's crit opens the fight).
	if (!bNight)
	{
		if (!bAtLair)
		{
			SetAwake();
			SwbWalkTo(Boss, Lair, Walk * 1.3f, 300.f);
			Boss->SetHome(FVector(Lair.X, Lair.Y, Boss->GetActorLocation().Z));
			return true;
		}
		Boss->SetAsleep(true);
		if (Body) Body->Set(WRSwampBossFlag::Asleep, true);
		St.RouteIndex = 0;
		bSkipWake = true;
		const float Wake = (Def ? Def->AggroRadius : 1800.f) * SwbST(Boss, TEXT("sleepAggroMult"), 0.35f);
		for (TActorIterator<AWRCharacter> It(World); It; ++It)
		{
			if (It->IsAlive() && FVector::Dist(It->GetActorLocation(), Boss->GetActorLocation()) < Wake) bSkipWake = false;
		}
		return true;
	}
	SetAwake();
	// Its rest at the Wallow, once a loop.
	if (bAtLair && T - St.LastRest > SwbST(Boss, TEXT("restSeconds"), 240.f) * 2.0)
	{
		St.LastRest = T;
		St.RestUntil = T + SwbST(Boss, TEXT("restSeconds"), 240.f);
	}
	if (T < St.RestUntil)
	{
		return true;
	}
	St.RouteIndex = FMath::Clamp(St.RouteIndex, 0, St.Route.Num() - 1);
	const FVector Goal = St.Route[St.RouteIndex];
	if (SwbWalkTo(Boss, Goal, Walk, 260.f))
	{
		St.RouteIndex = (St.RouteIndex + 1) % St.Route.Num();
	}
	Boss->SetHome(FVector(Goal.X, Goal.Y, Boss->GetActorLocation().Z));
	return true;
}

// ================================================================================================ in the fight

bool FWRSwampBoss::Tick(AWRBoss* Boss, float DeltaSeconds)
{
	if (IsVharra(Boss))
	{
		return VharraTick(Boss, DeltaSeconds);
	}
	if (!IsSwampBoss(Boss))
	{
		return false;
	}
	FWRSwampBossState& St = SwbS(Boss);
	UWRSwampBossBody* Body = UWRSwampBossBody::Get(Boss);
	const double T = SwbNow(Boss);
	EnsureArena(Boss, St);
	TickWave(Boss, St);
	// A grab holds the fight still: the death roll, the blood harvest, the latch.
	if (St.Grabbed.IsValid())
	{
		TickGrab(Boss, St, DeltaSeconds);
		if (St.Grabbed.IsValid()) return true;
	}
	// What lands on it, as it lands: a burn proc, frostbite, a stance break.
	const bool bBurn = Boss->HasTag(WRTags::Status_Burn);
	const bool bFrost = Boss->HasTag(WRTags::Status_Frostbite) || Boss->HasTag(WRTags::Status_Rimed);
	const bool bStance = Boss->HasTag(WRTags::Status_StanceBroken);
	if (Boss->CreatureId == TEXT("bloat"))
	{
		if (bBurn && !St.bBurnSeen)
		{
			// Fire: gorge -20% a proc, and a burning Bloat can't latch.
			AddGorge(Boss, St, -SwbST(Boss, TEXT("gorgeFireLoss"), 20.f));
			WRSwbLocal::SwlVoice(Boss, TEXT("hurt"));
		}
		if (bFrost && !St.bFrostSeen)
		{
			// Frost: gorge gain stops for 12 s.
			St.NoGainUntil = T + SwbST(Boss, TEXT("frostLockSeconds"), 12.f);
		}
	}
	if (Boss->CreatureId == TEXT("hex_warden") && bStance && !St.bStanceSeen && !St.bMaskBroken)
	{
		// The mask breaks on a stance break: beautiful under it, garnet-eyed, and she laughs.
		St.bMaskBroken = true;
		if (Body)
		{
			Body->Set(WRSwampBossFlag::MaskBroken, true);
			Body->MulticastMoment(TEXT("swampboss.mask_shatter"), Boss->GetActorLocation() + Boss->GetActorForwardVector() * 25.f + FVector(0.f, 0.f, Boss->GetSimpleCollisionHalfHeight() * 0.8f), 1.f, SwbSound(TEXT("part.break"), TEXT("stomp")));
		}
		WRSwbLocal::SwlVoice(Boss, TEXT("signature"));
		UE_LOG(LogWildreach, Log, TEXT("SWAMPBOSS the Hex-Warden's mask breaks"));
	}
	St.bBurnSeen = bBurn;
	St.bFrostSeen = bFrost;
	St.bStanceSeen = bStance;
	if (Boss->CreatureId == TEXT("mother_maw")) return TickMaw(Boss, St, Body, DeltaSeconds);
	if (Boss->CreatureId == TEXT("hex_warden")) return TickWarden(Boss, St, Body, DeltaSeconds);
	return TickBloat(Boss, St, Body, DeltaSeconds);
}

bool FWRSwampBoss::MawInWater(const AWRBoss* Boss, const FWRSwampBossState& St)
{
	if (!Boss) return false;
	const FVector Feet = SwbFeet(Boss);
	const float D = FVector::Dist2D(Feet, St.NestAt);
	return D < St.PoolRadius + 300.f && D > St.NestRadius * 0.85f && WRSwamp::IsWet(Feet + FVector(0.f, 0.f, SwbST(Boss, TEXT("waterDepth"), 40.f))) && !SwbOnSection(Boss);
}

bool FWRSwampBoss::TickMaw(AWRBoss* Boss, FWRSwampBossState& St, UWRSwampBossBody* Body, float DeltaSeconds)
{
	const double T = SwbNow(Boss);
	const bool bWater = MawInWater(Boss, St);
	St.Young.RemoveAll([](const TWeakObjectPtr<AWRCreature>& Y) { return !Y.IsValid() || !Y->IsAlive(); });
	// The rear's own punish: every rear onto the planks leaves her up out of the water, belly open, 1.2 s longer.
	if (St.bRearPunish && Boss->Current == INDEX_NONE)
	{
		St.bRearPunish = false;
		St.bRearing = false;
		Boss->BusyUntil = FMath::Max(Boss->BusyUntil, T) + SwbST(Boss, TEXT("rearPunish"), 1.2f);
		St.BellyUntil = Boss->BusyUntil;
	}
	if (Body)
	{
		Body->Set(WRSwampBossFlag::InWater, bWater);
		Body->Set(WRSwampBossFlag::Rearing, St.bRearing || T < St.BellyUntil);
		Body->Set(WRSwampBossFlag::Defending, T < St.DefendUntil);
	}
	const bool bFree = Boss->Current == INDEX_NONE && !Boss->HasTag(WRTags::Status_Staggered) && !Boss->HasTag(WRTags::Status_StanceBroken);
	// Hurt to 60% and to 30%: the nest roar, two young out of the reeds.
	const float Frac = SwbHealthFrac(Boss);
	if (bFree)
	{
		AWRCharacterBase* Target = Boss->PickBossTarget();
		if (!St.bYoung60 && Frac < SwbST(Boss, TEXT("youngAt1"), 0.6f) && Target && StartRow(Boss, TEXT("nest_roar"), Target)) { St.bYoung60 = true; return true; }
		if (!St.bYoung30 && Frac < SwbST(Boss, TEXT("youngAt2"), 0.3f) && Target && StartRow(Boss, TEXT("nest_roar"), Target)) { St.bYoung30 = true; return true; }
	}
	// Fire on the nest: she leaves the water to stand over it for 8 s (out of the water: no burst, no roll).
	if (T < St.DefendUntil)
	{
		const FVector Out = (Boss->GetActorLocation() - St.NestAt).GetSafeNormal2D();
		const FVector Guard = St.NestAt + (Out.IsNearlyZero() ? FVector(1.f, 0.f, 0.f) : Out) * St.NestRadius * 0.8f;
		if (bFree && FVector::Dist2D(Boss->GetActorLocation(), Guard) > 260.f)
		{
			SwbWalkTo(Boss, Guard, 2000.f, 200.f);
			return true;
		}
		AWRCharacterBase* Target = Boss->PickBossTarget();
		if (bFree && Target && FVector::Dist2D(Target->GetActorLocation(), Boss->GetActorLocation()) > SwbST(Boss, TEXT("defendReach"), 750.f))
		{
			Boss->SetActorRotation(FRotator(0.f, (Target->GetActorLocation() - Boss->GetActorLocation()).Rotation().Yaw, 0.f));
			if (T >= St.NextTickAt)
			{
				St.NextTickAt = T + 3.0;
				Boss->MulticastTell(0, TEXT("head"), NAME_None);
				WRSwbLocal::SwlVoice(Boss, TEXT("aggro"));
			}
			return true;
		}
	}
	(void)DeltaSeconds;
	return false;
}

bool FWRSwampBoss::TickWarden(AWRBoss* Boss, FWRSwampBossState& St, UWRSwampBossBody* Body, float DeltaSeconds)
{
	const double T = SwbNow(Boss);
	if (Body)
	{
		Body->Set(WRSwampBossFlag::HexWalk, false);
		Body->Set(WRSwampBossFlag::Scattered, St.bScattered);
	}
	// The totems: a broken one strips its curse for good (this fight) and staggers her if she's near.
	for (int32 i = 0; i < 4; ++i)
	{
		const AWRCreature* C = St.Totems[i].Get();
		if ((St.TotemsDown & (1 << i)) == 0 && C && !C->IsAlive()) TotemBroken(Boss, St, i);
	}
	TickMimic(Boss, St, false);
	TickBloodTotem(Boss, St);
	// Forty crows, then her again at a standing totem.
	if (St.bScattered)
	{
		if (T >= St.ReformAt) Reform(Boss, St);
		return true;
	}
	(void)DeltaSeconds;
	return false;
}

bool FWRSwampBoss::TickBloat(AWRBoss* Boss, FWRSwampBossState& St, UWRSwampBossBody* Body, float DeltaSeconds)
{
	const double T = SwbNow(Boss);
	// Pulled past 65 m from where the fight began: it gives up and goes back to its route.
	if (FVector::Dist2D(Boss->GetActorLocation(), Boss->GetHome()) > SwbST(Boss, TEXT("resetDistance"), 6500.f))
	{
		UE_LOG(LogWildreach, Log, TEXT("SWAMPBOSS the Bloat was pulled off its route: back to it"));
		Boss->ResetFight();
		return true;
	}
	if (Boss->IsAsleep()) Boss->SetAsleep(false);
	if (Body)
	{
		Body->Set(WRSwampBossFlag::Asleep, false);
		Body->Set(WRSwampBossFlag::Burning, Boss->HasTag(WRTags::Status_Burn));
	}
	// Wading bleedwater in the fight feeds it too.
	if (T >= St.NextWadeTick)
	{
		St.NextWadeTick = T + 1.0;
		const FVector Feet = SwbFeet(Boss);
		if (WRSwamp::IsWet(Feet) && WRSwamp::HazardWaterAt(Boss, Feet) == EWRSwampWater::Bleedwater) AddGorge(Boss, St, SwbST(Boss, TEXT("gorgeWade"), 1.f));
	}
	// Full: the burst, at the first moment it's free.
	if (St.Gorge >= 100.f) St.bBurstQueued = true;
	if (St.bBurstQueued && Boss->Current == INDEX_NONE && !Boss->HasTag(WRTags::Status_Staggered) && !Boss->HasTag(WRTags::Status_StanceBroken))
	{
		if (AWRCharacterBase* Target = Boss->PickBossTarget())
		{
			if (StartRow(Boss, TEXT("burst"), Target)) return true;
		}
	}
	(void)DeltaSeconds;
	return false;
}

AWRCharacterBase* FWRSwampBoss::PickTarget(AWRBoss* Boss, AWRCharacterBase* Nearest)
{
	if (IsVharra(Boss))
	{
		return VharraPickTarget(Boss, Nearest);
	}
	return Nearest;
}

// ================================================================================================ the attack machine

bool FWRSwampBoss::Allowed(const AWRBoss* Boss, const FWRBossAttack& Attack, const AWRCharacterBase* Target)
{
	if (IsVharra(Boss))
	{
		return VharraAllowed(Boss, Attack, Target);
	}
	if (!IsSwampBoss(Boss))
	{
		return true;
	}
	const FWRSwampBossState* St = SwbFind(Boss);
	const double T = SwbNow(Boss);
	const FName Id = Attack.Id;
	const float Dist = Target ? FVector::Dist2D(Target->GetActorLocation(), Boss->GetActorLocation()) : 0.f;
	if (Boss->CreatureId == TEXT("mother_maw"))
	{
		if (Id == TEXT("nest_roar")) return false;   // only at 60% and 30%
		if (Id == TEXT("duckweed_burst") || Id == TEXT("death_roll"))
		{
			// From open water only, never under the planks (she can't dive under them), never while she guards her nest.
			return St && MawInWater(Boss, *St) && Target && !SwbOnSection(Target) && T >= St->DefendUntil;
		}
	}
	else if (Boss->CreatureId == TEXT("hex_warden"))
	{
		if (Id == TEXT("reform_strike")) return false;   // only out of a scatter
		if (Id == TEXT("crow_scatter")) return St && !St->bScattered && St->Totems[0].IsValid() && St->Totems[0]->IsAlive();
		if (Id == TEXT("mimic_call"))
		{
			if (!St || !St->Totems[3].IsValid() || !St->Totems[3]->IsAlive()) return false;
			for (AWRCharacterBase* V : SwbFoes(Boss, Boss->ChaseRadius))
			{
				if (const AWRCreature* C = Cast<AWRCreature>(V); C && C->IsTamed()) return true;
			}
			return false;
		}
		if (Id == TEXT("hex_bolt")) return Dist >= SwbST(Boss, TEXT("boltMinRange"), 500.f);
	}
	else if (Boss->CreatureId == TEXT("bloat"))
	{
		if (Id == TEXT("burst")) return false;   // only when full
		if (Id == TEXT("latch")) return !Boss->HasTag(WRTags::Status_Burn);
		if (Id == TEXT("spray"))
		{
			const UWRSwampBossBody* Body = UWRSwampBossBody::Get(Boss);
			return !Body || (Body->Burst & 7) != 7;   // it sprays from a vein: none left, no spray
		}
	}
	return true;
}

void FWRSwampBoss::AttackStarted(AWRBoss* Boss, FWRBossAttack& Attack, AWRCharacterBase* Target)
{
	if (IsVharra(Boss))
	{
		VharraAttackStarted(Boss, Attack, Target);
		return;
	}
	if (!IsSwampBoss(Boss))
	{
		return;
	}
	FWRSwampBossState& St = SwbS(Boss);
	UWRSwampBossBody* Body = UWRSwampBossBody::Get(Boss);
	const double T = SwbNow(Boss);
	const FName Id = Attack.Id;
	St.Stage = 0;
	St.StageAt = T;
	St.Struck.Reset();
	St.Aim = Boss->AttackPoint;
	if (Boss->CreatureId == TEXT("mother_maw"))
	{
		// On the planks she has to rear up out of the water to reach you: belly open (x1.3), and a longer punish.
		const bool bRearMove = Id == TEXT("snap_a") || Id == TEXT("snap_b") || Id == TEXT("head_slam");
		St.bRearing = bRearMove && Target && MawInWater(Boss, St) && SwbOnSection(Target);
		if (St.bRearing)
		{
			St.BellyUntil = T + Attack.Windup + 3.0;
			if (Body) Body->Set(WRSwampBossFlag::Rearing, true);
			WRFx::Broadcast(Boss, TEXT("mire.splash"), SwbFeet(Boss) + Boss->GetActorForwardVector() * Boss->GetSimpleCollisionRadius() + FVector(0.f, 0.f, 80.f), FRotator::ZeroRotator, 1.4f, SwbSound(TEXT("water.splash"), TEXT("stomp")));
		}
		if (Id == TEXT("duckweed_burst") && Target)
		{
			// A line of duckweed parting toward you across the pool over the wind-up (bubbles, red eyeshine).
			const FVector From = Boss->GetActorLocation(), To = Target->GetActorLocation();
			const int32 Steps = 6;
			const float WaterZ0 = FWRSwampData::Get().SeaZ + 2.f;
			for (int32 s = 0; s < Steps; ++s)
			{
				const FVector P = FMath::Lerp(From, To, (s + 1.f) / (Steps + 1.f));
				const float Delay = FMath::Max(0.01f, Attack.Windup * s / Steps);
				TWeakObjectPtr<AWRBoss> WeakBoss(Boss);
				FTimerHandle H;
				Boss->GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(Boss, [WeakBoss, P, WaterZ0, From, To]()
				{
					if (AWRBoss* B = WeakBoss.Get()) WRFx::Broadcast(B, TEXT("mire.duckweed"), FVector(P.X, P.Y, WaterZ0), (To - From).Rotation(), 2.f, TEXT("swamp.bleed.suck"));
				}), Delay, false);
			}
			WRSwbLocal::SwlVoice(Boss, TEXT("tell"));
		}
		else if (Id == TEXT("head_slam") && Target)
		{
			// The slam comes down where you stood at the wind-up (within her reach).
			const FVector Feet = SwbFeet(Target);
			const FVector Dir = (Feet - Boss->GetActorLocation()).GetSafeNormal2D();
			const float Reach = FMath::Min(float(FVector::Dist2D(Feet, Boss->GetActorLocation())), Attack.Range);
			St.Aim = FVector(Boss->GetActorLocation().X, Boss->GetActorLocation().Y, Feet.Z) + Dir * Reach;
		}
		else if (Id == TEXT("nest_roar"))
		{
			WRSwbLocal::SwlVoice(Boss, TEXT("signature"));
			WRFx::Broadcast(Boss, TEXT("mire.splash"), SwbFeet(Boss) + FVector(0.f, 0.f, 60.f), FRotator::ZeroRotator, 2.5f, NAME_None);
		}
		else if (Id == TEXT("death_roll"))
		{
			WRSwbLocal::SwlVoice(Boss, TEXT("tell"));
		}
	}
	else if (Boss->CreatureId == TEXT("hex_warden"))
	{
		const FVector Palm = Boss->GetActorLocation() + Boss->GetActorForwardVector() * (Boss->GetSimpleCollisionRadius() + 30.f) + Boss->GetActorRightVector() * 25.f + FVector(0.f, 0.f, Boss->GetSimpleCollisionHalfHeight() * 0.3f);
		if (Id == TEXT("hex_bolt"))
		{
			// The mask's eyes glow and a black-purple sigil gathers in her palm.
			if (Body)
			{
				Body->Set(WRSwampBossFlag::Casting, true);
				Body->MulticastMoment(TEXT("swampboss.hex_sigil"), Palm, 1.f, SwbSound(TEXT("blood.cast"), TEXT("blood.hex")));
			}
			St.Aim = Target ? Target->GetActorLocation() : Boss->AttackPoint;
		}
		else if (Id == TEXT("mimic_call"))
		{
			// She cups her mouth and your tame's own call comes out of her.
			if (Body) Body->Set(WRSwampBossFlag::Casting, true);
			TArray<AWRCreature*> Tames;
			for (AWRCharacterBase* V : SwbFoes(Boss, Boss->ChaseRadius))
			{
				if (AWRCreature* C = Cast<AWRCreature>(V); C && C->IsTamed()) Tames.Add(C);
			}
			Tames.Sort([Boss](const AWRCreature& A, const AWRCreature& B) { return FVector::DistSquared(A.GetActorLocation(), Boss->GetActorLocation()) < FVector::DistSquared(B.GetActorLocation(), Boss->GetActorLocation()); });
			St.Mimicked.RemoveAll([](const FWRSwampBossState::FMimic& Old) { return !Old.Tame.IsValid() || Old.Until <= 0.0; });   // a call cut off before
			const int32 MaxTames = FMath::RoundToInt(SwbST(Boss, TEXT("mimicTames"), 2.f));
			for (int32 i = 0; i < Tames.Num() && St.Mimicked.Num() < MaxTames; ++i)
			{
				if (St.Mimicked.ContainsByPredicate([&](const FWRSwampBossState::FMimic& M) { return M.Tame.Get() == Tames[i]; })) continue;
				FWRSwampBossState::FMimic M;
				M.Tame = Tames[i];
				M.Until = 0.0;   // pending: taken when her call ends
				St.Mimicked.Add(M);
				const FName Cry(*FString::Printf(TEXT("creature.%s.idle"), *Tames[i]->CreatureId.ToString()));
				if (Body && WRSound::Has(Cry)) Body->MulticastMoment(NAME_None, Boss->GetActorLocation() + FVector(0.f, 0.f, Boss->GetSimpleCollisionHalfHeight() * 0.8f), 1.f, Cry);
			}
		}
		else if (Id == TEXT("crow_scatter"))
		{
			// The cloak flares.
			if (Body) Body->MulticastMoment(TEXT("swampboss.cloak_flare"), Boss->GetActorLocation(), 1.f, SwbSound(TEXT("swamp.hex.wings"), TEXT("swamp.crows")));
		}
		else if (Id == TEXT("blood_harvest"))
		{
			WRSwbLocal::SwlVoice(Boss, TEXT("tell"));
		}
	}
	else if (Boss->CreatureId == TEXT("bloat"))
	{
		if (Id == TEXT("burst"))
		{
			// The body glows to near white-garnet with a high stretched-skin whine (crimson: get out of 8 m).
			St.bBurstQueued = false;
			if (Body) Body->Set(WRSwampBossFlag::Glowing, true);
			WRSwbLocal::SwlVoice(Boss, TEXT("signature"));
			UE_LOG(LogWildreach, Log, TEXT("SWAMPBOSS the Bloat glows: it will burst"));
		}
		else if (Id == TEXT("latch"))
		{
			// The front disc opens, a Y of three jaws.
			if (Body) Body->Set(WRSwampBossFlag::JawOpen, true);
		}
		else if (Id == TEXT("spray"))
		{
			// A vein pulses at the near end: the brightest one left.
			const FWRCreatureDef* Def = Boss->GetDef();
			const TArray<int32> Veins = WRSwbLocal::SwlVeinParts(Def);
			float BestD = TNumericLimits<float>::Max();
			St.Aim2 = Boss->GetActorLocation() + Boss->GetActorForwardVector() * Boss->GetSimpleCollisionRadius() * 0.6f;
			for (int32 k = 0; k < Veins.Num(); ++k)
			{
				if (Boss->IsPartBroken(Def->Parts[Veins[k]].Id)) continue;
				const FVector P = WRSwbLocal::SwlPartPoint(Boss, Def->Parts[Veins[k]]);
				const float D = Target ? FVector::DistSquared(P, Target->GetActorLocation()) : 0.f;
				if (D < BestD) { BestD = D; St.Aim2 = P; }
			}
			if (Body) Body->MulticastMoment(TEXT("swampboss.vein_pulse"), St.Aim2, 1.f, NAME_None);
			WRSwbLocal::SwlVoice(Boss, TEXT("tell"));
		}
	}
}

bool FWRSwampBoss::TickWindup(AWRBoss* Boss, FWRBossAttack& Attack, float DeltaSeconds)
{
	if (IsVharra(Boss))
	{
		return VharraTickWindup(Boss, Attack, DeltaSeconds);
	}
	if (!IsSwampBoss(Boss))
	{
		return false;
	}
	FWRSwampBossState& St = SwbS(Boss);
	const double T = SwbNow(Boss);
	if (Boss->CreatureId == TEXT("hex_warden") && Attack.Id == TEXT("hex_bolt"))
	{
		// The aim locks two thirds through the wind-up on where you are then: never homing after.
		if (St.Stage == 0 && T - Boss->StrikeStarted >= (Boss->ResolveAt - Boss->StrikeStarted) * SwbST(Boss, TEXT("boltLockShare"), 0.65f))
		{
			St.Stage = 1;
			if (AWRCharacterBase* Target = Boss->AttackTarget.Get()) St.Aim = Target->GetActorLocation();
		}
	}
	(void)DeltaSeconds;
	return false;
}

bool FWRSwampBoss::BeginStrike(AWRBoss* Boss, FWRBossAttack& Attack)
{
	if (IsVharra(Boss))
	{
		return VharraBeginStrike(Boss, Attack);
	}
	if (!IsSwampBoss(Boss))
	{
		return false;
	}
	FWRSwampBossState& St = SwbS(Boss);
	UWRSwampBossBody* Body = UWRSwampBossBody::Get(Boss);
	UWorld* World = Boss->GetWorld();
	const double T = SwbNow(Boss);
	const FName Id = Attack.Id;
	const float Active = Attack.bRow ? FMath::Max(0.1f, Attack.Row.Active) : 0.25f;
	if (Boss->CreatureId == TEXT("mother_maw"))
	{
		if (Id == TEXT("head_slam"))
		{
			// The head comes down: everyone within the slam takes it, and the planks it lands on break.
			const float R = SwbST(Boss, TEXT("slamRadius"), 260.f);
			BreakSectionAt(Boss, St.Aim);
			WRFx::Broadcast(Boss, TEXT("mire.splash"), St.Aim + FVector(0.f, 0.f, 20.f), FRotator::ZeroRotator, R / 120.f, SwbSound(TEXT("water.splash"), TEXT("stomp")));
			for (AWRCharacterBase* V : SwbFoes(Boss, Boss->ChaseRadius))
			{
				if (FVector::Dist2D(V->GetActorLocation(), St.Aim) < R + V->GetSimpleCollisionRadius() && FMath::Abs(SwbFeet(V).Z - St.Aim.Z) < 260.f) Boss->StrikeHit(V);
			}
			Boss->StrikeEnds = T + Active;
			return true;
		}
		if (Id == TEXT("nest_roar"))
		{
			// The young come out of the reeds round the nest.
			const FVector At = St.NestAt.IsZero() ? Boss->GetActorLocation() : St.NestAt;
			SpawnYoung(Boss, TEXT("murkjaw_young"), FMath::RoundToInt(SwbST(Boss, TEXT("youngCount"), 2.f)), SwbST(Boss, TEXT("youngHealth"), 120.f), At);
			WRFx::Broadcast(Boss, TEXT("roar.blast"), SwbFeet(Boss), FRotator::ZeroRotator, 2.f, NAME_None);
			Boss->StrikeEnds = T + Active;
			return true;
		}
		if (Id == TEXT("tail_club"))
		{
			WRFx::Broadcast(Boss, MawInWater(Boss, St) ? TEXT("mire.splash") : TEXT("mire.mud"), SwbFeet(Boss) - Boss->GetActorForwardVector() * Boss->GetSimpleCollisionRadius() + FVector(0.f, 0.f, 40.f), FRotator::ZeroRotator, 1.8f, SwbSound(TEXT("water.splash"), TEXT("stomp")));
			Boss->StrikeEnds = T + Active;
			return true;   // its 300 degree sweep runs in TickStrike
		}
		if (Id == TEXT("duckweed_burst"))
		{
			WRFx::Broadcast(Boss, TEXT("mire.splash"), SwbFeet(Boss) + FVector(0.f, 0.f, 60.f), Boss->AttackFacing, 1.6f, SwbSound(TEXT("water.splash"), TEXT("stomp")));
			return false;   // the lunge runs its line on the boss machine
		}
		return false;
	}
	if (Boss->CreatureId == TEXT("hex_warden"))
	{
		if (Id == TEXT("hex_bolt"))
		{
			// Straight and level at 1,000 cm/s from her palm at where you were when it locked: never homing.
			const FVector Palm = Boss->GetActorLocation() + Boss->GetActorForwardVector() * (Boss->GetSimpleCollisionRadius() + 30.f) + Boss->GetActorRightVector() * 25.f + FVector(0.f, 0.f, Boss->GetSimpleCollisionHalfHeight() * 0.3f);
			FWRHit Hit;
			Hit.Instigator = Boss;
			Hit.Damage = Attack.Damage * SwbScale();
			Hit.PoiseDamage = Attack.Poise;
			Hit.Impact = Attack.Impact;
			Hit.bProjectile = true;
			Hit.Tier = EWRHitTier::Heavy;
			Hit.Element = WRTags::Element_Physical;
			if (Attack.bRow) for (const TPair<FName, float>& B : Attack.Row.Buildups) Hit.Buildups.Add(B.Key, B.Value);
			const float Speed = Attack.bRow && Attack.Row.ProjectileSpeed > 0.f ? Attack.Row.ProjectileSpeed : SwbST(Boss, TEXT("boltSpeed"), 1000.f);
			FireShot(Boss, TEXT("hex_bolt"), Palm, St.Aim.IsZero() ? Boss->AttackPoint + FVector(0.f, 0.f, 90.f) : St.Aim, Hit, Speed, false);
			if (Body) Body->Set(WRSwampBossFlag::Casting, false);
			Boss->StrikeEnds = T + Active;
			return true;
		}
		if (Id == TEXT("crow_scatter"))
		{
			Scatter(Boss, St);
			Boss->StrikeEnds = T + 0.05;
			return true;
		}
		if (Id == TEXT("mimic_call"))
		{
			// Up to two tames leave you for her side for 6 s: they don't attack you, they just go.
			for (FWRSwampBossState::FMimic& M : St.Mimicked)
			{
				AWRCreature* Tame = M.Tame.Get();
				if (!Tame || M.Until > 0.0) continue;
				M.Order = Tame->TameOrder;
				M.bAggressive = Tame->bTameAggressive;
				M.Until = T + SwbST(Boss, TEXT("mimicSeconds"), 6.f);
				const float Side = (St.Mimicked.IndexOfByPredicate([Tame](const FWRSwampBossState::FMimic& X) { return X.Tame.Get() == Tame; }) % 2) ? -1.f : 1.f;
				M.Side = Boss->GetActorLocation() + Boss->GetActorRightVector() * Side * (Boss->GetSimpleCollisionRadius() + Tame->GetSimpleCollisionRadius() + 150.f);
				Tame->TameOrder = EWRTameOrder::Stay;
				Tame->bTameAggressive = false;
				Tame->SetAggroTarget(nullptr);
				WRFx::Broadcast(World, TEXT("status.curse"), Tame->GetActorLocation(), FRotator::ZeroRotator, 1.f, NAME_None);
				if (AWRPlayerController* Owner = SwbOwnerPC(World, Tame->TamedBy)) Owner->ClientNotify(FString::Printf(TEXT("%s hears its own call in the witch's mouth and goes to her."), *Tame->GetDisplayLabel()));
				UE_LOG(LogWildreach, Log, TEXT("SWAMPBOSS the Hex-Warden's mimic call pulls %s"), *Tame->GetName());
			}
			St.Mimicked.RemoveAll([](const FWRSwampBossState::FMimic& M) { return !M.Tame.IsValid() || M.Until <= 0.0; });
			if (Body) Body->Set(WRSwampBossFlag::Casting, false);
			Boss->StrikeEnds = T + Active;
			return true;
		}
		return false;
	}
	if (Boss->CreatureId == TEXT("bloat"))
	{
		if (Id == TEXT("burst"))
		{
			// It bursts: an 8 m ring of stolen blood (unblockable), an 8 m bleedwater puddle for 20 s; empty, it
			// collapses (staggered 6 s: the big punish).
			const float R = Attack.Size;
			WRFx::Broadcast(Boss, TEXT("swampboss.burst"), Boss->GetActorLocation(), FRotator::ZeroRotator, R / 800.f, TEXT("swamp.bloat.burst"));
			// The blood sprays out from the body as a ring (cookpass-combat 3.10): it takes you where it reaches.
			{
				TWeakObjectPtr<AWRBoss> WeakB(Boss);
				if (AWRStrikeFront* Spray = AWRStrikeFront::Ring(Boss, SwbFeet(Boss), R, SwbST(Boss, TEXT("burstSpeed"), 1400.f), 220.f,
					[WeakB](AWRCharacterBase* V) { if (AWRBoss* B = WeakB.Get(); B && B->IsHostileTo(V)) B->StrikeHit(V); }, TEXT("swampboss.burst"), NAME_None))
				{
					Spray->bGround = false;   // a spray of blood, not a ground wave
				}
				Boss->StrikeEnds = FMath::Max(Boss->StrikeEnds, T + R / FMath::Max(100.f, SwbST(Boss, TEXT("burstSpeed"), 1400.f)) + 0.1f);
			}
			Puddle(World, TEXT("bleedwater"), SwbFeet(Boss), SwbST(Boss, TEXT("burstPuddleRadius"), 800.f), SwbST(Boss, TEXT("burstPuddleSeconds"), 20.f), SwbST(Boss, TEXT("burstPuddleBleed"), 10.f), false, Boss);
			St.Gorge = 0.f;
			if (Body)
			{
				Body->Gorge = 0;
				Body->Set(WRSwampBossFlag::Glowing, false);
			}
			Boss->StrikeEnds = T + Active;
			return true;
		}
		if (Id == TEXT("spray"))
		{
			WRFx::Broadcast(Boss, TEXT("swampboss.vein_spray"), St.Aim2, Boss->AttackFacing, Attack.Range / 600.f, TEXT("swamp.bloat.spew"));
			Boss->StrikeEnds = T + Active;
			return true;   // the cone runs in TickStrike
		}
		if (Id == TEXT("latch") && Body)
		{
			Body->Set(WRSwampBossFlag::JawOpen, true);
		}
		return false;
	}
	return false;
}

bool FWRSwampBoss::TickStrike(AWRBoss* Boss, FWRBossAttack& Attack, float DeltaSeconds)
{
	if (IsVharra(Boss))
	{
		return VharraTickStrike(Boss, Attack, DeltaSeconds);
	}
	if (!IsSwampBoss(Boss))
	{
		return false;
	}
	FWRSwampBossState& St = SwbS(Boss);
	const double T = SwbNow(Boss);
	const FName Id = Attack.Id;
	const float Span = FMath::Max(0.05f, float(Boss->StrikeEnds - Boss->StrikeStarted));
	const float A = FMath::Clamp(float(T - Boss->StrikeStarted) / Span, 0.f, 1.f);
	if (Boss->CreatureId == TEXT("mother_maw"))
	{
		if (Id == TEXT("head_slam") || Id == TEXT("nest_roar")) return true;
		if (Id == TEXT("tail_club"))
		{
			// The tail goes round 300 degrees from one side of her back to the other (from the water it sweeps the
			// boardwalk's edge): low, jump it.
			const float Arc = SwbST(Boss, TEXT("tailArc"), 300.f);
			const float Edge = -Arc * 0.5f + Arc * A;
			for (AWRCharacterBase* V : SwbFoes(Boss, Boss->ChaseRadius))
			{
				const float D = FVector::Dist2D(V->GetActorLocation(), Boss->GetActorLocation()) - V->GetSimpleCollisionRadius();
				if (D > Attack.Size) continue;
				const float Rel = FMath::FindDeltaAngleDegrees(Boss->AttackFacing.Yaw + 180.f, (V->GetActorLocation() - Boss->GetActorLocation()).Rotation().Yaw);
				if (Rel >= -Arc * 0.5f - 8.f && Rel <= Edge + 8.f) Boss->StrikeHit(V);
			}
			return true;
		}
		return false;
	}
	if (Boss->CreatureId == TEXT("hex_warden"))
	{
		return Id == TEXT("hex_bolt") || Id == TEXT("crow_scatter") || Id == TEXT("mimic_call");
	}
	if (Boss->CreatureId == TEXT("bloat"))
	{
		if (Id == TEXT("burst")) return true;
		if (Id == TEXT("spray"))
		{
			// A 6 m cone from the vein, sweeping a little as the vein empties: bleed on whoever it really wets.
			const float Sweep = FMath::Lerp(-Attack.Size * 0.5f, Attack.Size * 0.5f, A);
			const FVector Dir = Boss->AttackFacing.Vector().RotateAngleAxis(Sweep, FVector::UpVector);
			for (AWRCharacterBase* V : SwbFoes(Boss, Boss->ChaseRadius))
			{
				const FVector To = V->GetActorLocation() - St.Aim2;
				if (To.Size2D() > Attack.Range + V->GetSimpleCollisionRadius()) continue;
				const float Ang = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(float(FVector::DotProduct(To.GetSafeNormal2D(), Dir.GetSafeNormal2D())), -1.f, 1.f)));
				if (Ang <= Attack.Size * 0.6f) Boss->StrikeHit(V);
			}
			return true;
		}
		return false;
	}
	(void)DeltaSeconds;
	return false;
}

void FWRSwampBoss::EndStrike(AWRBoss* Boss, const FWRBossAttack& Attack)
{
	if (IsVharra(Boss))
	{
		VharraEndStrike(Boss, Attack);
		return;
	}
	if (!IsSwampBoss(Boss))
	{
		return;
	}
	FWRSwampBossState& St = SwbS(Boss);
	UWRSwampBossBody* Body = UWRSwampBossBody::Get(Boss);
	const FName Id = Attack.Id;
	if (Boss->CreatureId == TEXT("mother_maw"))
	{
		if (St.bRearing) St.bRearPunish = true;   // applied when the string ends (TickMaw)
	}
	else if (Boss->CreatureId == TEXT("hex_warden"))
	{
		if (Id == TEXT("sickle_c"))
		{
			// The dance ends facing away from you: the punish.
			Boss->SetActorRotation(FRotator(0.f, Boss->AttackFacing.Yaw + 180.f, 0.f));
		}
		if (Body && (Id == TEXT("hex_bolt") || Id == TEXT("mimic_call"))) Body->Set(WRSwampBossFlag::Casting, false);
	}
	else if (Boss->CreatureId == TEXT("bloat"))
	{
		if (Body) Body->Set(WRSwampBossFlag::JawOpen, false);
		if (Id == TEXT("burst"))
		{
			Boss->Status->Stagger(SwbST(Boss, TEXT("burstStagger"), 6.f));
			UE_LOG(LogWildreach, Log, TEXT("SWAMPBOSS the Bloat burst and collapses"));
		}
	}
}

void FWRSwampBoss::ModifyHit(const AWRBoss* Boss, const FWRBossAttack& Attack, FWRHit& Hit)
{
	if (IsVharra(Boss))
	{
		VharraModifyHit(Boss, Attack, Hit);
		return;
	}
	if (!IsSwampBoss(Boss))
	{
		return;
	}
	const FWRSwampBossState* St = SwbFind(Boss);
	if (Boss->CreatureId == TEXT("hex_warden") && St)
	{
		// The bone totem: her sickles carry curse 30 while it stands.
		const bool bSickle = Attack.Id == TEXT("sickle_a") || Attack.Id == TEXT("sickle_b") || Attack.Id == TEXT("sickle_c") || Attack.Id == TEXT("reform_strike");
		if (bSickle && St->Totems[1].IsValid() && St->Totems[1]->IsAlive())
		{
			Hit.Buildups.FindOrAdd(TEXT("curse")) += SwbST(Boss, TEXT("boneCurse"), 30.f);
		}
	}
}

void FWRSwampBoss::HitLanded(AWRBoss* Boss, AWRCharacterBase* Victim, FName AttackId, EWRHitResult Result)
{
	if (IsVharra(Boss))
	{
		VharraHitLanded(Boss, Victim, AttackId, Result);
		return;
	}
	if (!IsSwampBoss(Boss) || !Victim || !Boss->IsAlive())
	{
		return;
	}
	FWRSwampBossState& St = SwbS(Boss);
	const bool bLanded = Result == EWRHitResult::Hit || Result == EWRHitResult::GuardBroken;
	if (AttackId == TEXT("death_roll") && Result == EWRHitResult::Hit && Victim->IsAlive())
	{
		// Caught: dragged under and rolled (20 a second for 2.5 s). Allies' blows can free you.
		StartGrab(Boss, St, Victim, TEXT("roll"), SwbST(Boss, TEXT("rollSeconds"), 2.5f), SwbST(Boss, TEXT("rollDps"), 20.f));
		WRFx::Broadcast(Boss, TEXT("mire.splash"), SwbFeet(Victim) + FVector(0.f, 0.f, 50.f), FRotator::ZeroRotator, 2.f, SwbSound(TEXT("water.splash"), TEXT("stomp")));
	}
	else if (AttackId == TEXT("blood_harvest") && Result == EWRHitResult::Hit && Victim->IsAlive())
	{
		// Both sickles in: she drinks (heals 300).
		StartGrab(Boss, St, Victim, TEXT("harvest"), SwbST(Boss, TEXT("harvestHold"), 1.2f), 0.f);
		SwbHeal(Boss, SwbST(Boss, TEXT("harvestHeal"), 300.f));
		const FVector D = Boss->GetActorLocation() - Victim->GetActorLocation();
		WRFx::Broadcast(Boss, TEXT("blood.transfusion"), Victim->GetActorLocation(), D.Rotation(), D.Size() / 100.f, SwbSound(TEXT("blood.lifesteal"), TEXT("blood.hit")));
	}
	else if (AttackId == TEXT("latch"))
	{
		if (Result == EWRHitResult::Hit && Victim->IsAlive() && !Boss->HasTag(WRTags::Status_Burn))
		{
			// The three jaws close on you: 10 a second, bleed 20 a second, and it gorges. Roll: 60% it comes off.
			StartGrab(Boss, St, Victim, TEXT("latch"), SwbST(Boss, TEXT("latchSeconds"), 4.f), SwbST(Boss, TEXT("latchDps"), 10.f));
		}
		else if (Result == EWRHitResult::Blocked || Result == EWRHitResult::Parried)
		{
			// Knocked off the guard.
			WRFx::Broadcast(Boss, TEXT("blood.hit"), Victim->GetActorLocation(), FRotator::ZeroRotator, 0.6f, NAME_None);
			if (UWRSwampBossBody* Body = UWRSwampBossBody::Get(Boss)) Body->Set(WRSwampBossFlag::JawOpen, false);
		}
	}
	(void)bLanded;
}

float FWRSwampBoss::Incoming(AWRBoss* Boss, const FWRHit& Hit, float Damage)
{
	if (IsVharra(Boss))
	{
		return VharraIncoming(Boss, Hit, Damage);
	}
	if (!IsSwampBoss(Boss))
	{
		return Damage;
	}
	const FWRSwampBossState* St = SwbFind(Boss);
	const double T = SwbNow(Boss);
	// Weak to Lightning (x1.5): a Lightning blow, or one carrying shock.
	if (Hit.Element == WRTags::Element_Lightning || Hit.PathId == TEXT("lightning") || Hit.Buildups.FindRef(TEXT("shock")) > 0.f)
	{
		Damage *= WRTune(TEXT("bosses.weaknessMultiplier"), 1.5f);
	}
	if (!St)
	{
		return Damage;
	}
	if (Boss->CreatureId == TEXT("mother_maw") && (St->bRearing || T < St->BellyUntil))
	{
		// Her belly, open while she rears on the planks or rolls: x1.3 where it lands on it.
		const bool bBelly = Hit.ImpactPoint.IsZero() || Boss->IsNearLockPoint(Hit.ImpactPoint, TEXT("belly"), Boss->GetSimpleCollisionRadius() * 0.9f);
		if (bBelly) Damage *= SwbST(Boss, TEXT("bellyMult"), 1.3f);
	}
	if (Boss->CreatureId == TEXT("hex_warden") && St->bScattered)
	{
		return 0.f;   // forty crows: nothing to hit
	}
	return Damage;
}

bool FWRSwampBoss::OwnWeakness(const AWRBoss* Boss)
{
	if (IsVharra(Boss))
	{
		return VharraOwnWeakness(Boss);
	}
	return IsSwampBoss(Boss);
}

void FWRSwampBoss::Damaged(AWRBoss* Boss, const FWRHit& Hit, float Damage)
{
	if (IsVharra(Boss))
	{
		VharraDamaged(Boss, Hit, Damage);
		return;
	}
	if (!IsSwampBoss(Boss))
	{
		return;
	}
	FWRSwampBossState& St = SwbS(Boss);
	// An ally's blows free a grabbed friend (the roll, the harvest, the latch).
	if (AWRCharacterBase* Held = St.Grabbed.Get(); Held && Hit.Instigator.Get() != Held && SwbIsSurvivorSide(Hit.Instigator))
	{
		St.GrabFree += Damage;
		if (St.GrabFree >= SwbST(Boss, TEXT("grabFreeDamage"), 300.f))
		{
			ReleaseGrab(Boss, St);
			Boss->Status->Stagger(SwbST(Boss, TEXT("grabFreeStagger"), 1.f));
			UE_LOG(LogWildreach, Log, TEXT("SWAMPBOSS %s lets go: freed by an ally"), *Boss->GetDisplayLabel());
		}
	}
	if (Hit.bDamageOverTime)
	{
		return;
	}
	if (Boss->CreatureId == TEXT("hex_warden") && St.TotemsDown == 0xF && SwbIsSurvivorSide(Hit.Instigator))
	{
		// All four totems down: she can't scatter, and every 5th hit staggers her.
		if (++St.HitCount % FMath::Max(1, FMath::RoundToInt(SwbST(Boss, TEXT("noTotemStaggerEvery"), 5.f))) == 0)
		{
			Boss->Status->Stagger(SwbST(Boss, TEXT("noTotemStagger"), 1.5f));
		}
	}
	if (Boss->CreatureId == TEXT("bloat") && !Hit.ImpactPoint.IsZero())
	{
		// The three brightest veins (part HP 300 each): the blows that land on them.
		const FWRCreatureDef* Def = Boss->GetDef();
		const TArray<int32> Veins = WRSwbLocal::SwlVeinParts(Def);
		for (int32 k = 0; k < Veins.Num(); ++k)
		{
			const FWRCreatureDef::FBodyPoint& P = Def->Parts[Veins[k]];
			if (Boss->IsPartBroken(P.Id) || Veins[k] >= 8) continue;
			if (FVector::Dist(WRSwbLocal::SwlPartPoint(Boss, P), Hit.ImpactPoint) > SwbST(Boss, TEXT("veinRadius"), 220.f)) continue;
			St.VeinHP[k] -= Damage;
			if (St.VeinHP[k] <= 0.f)
			{
				Boss->PartsBroken |= uint8(1 << Veins[k]);
				Boss->OnRep_PartsBroken();
				UE_LOG(LogWildreach, Log, TEXT("PART %s's %s ruptured"), *Boss->GetDisplayLabel(), *P.Id.ToString());
				Boss->OnPartBroken(P, Hit.Instigator);
			}
			break;
		}
	}
}

void FWRSwampBoss::PartBroken(AWRBoss* Boss, const FWRCreatureDef::FBodyPoint& Part, AActor* By)
{
	if (IsVharra(Boss))
	{
		VharraPartBroken(Boss, Part, By);
		return;
	}
	if (!IsSwampBoss(Boss) || Part.OnBreak != TEXT("vein"))
	{
		return;
	}
	// A vein ruptures: it sprays bleed 40 around it and the Bloat loses 20% of its gorge.
	FWRSwampBossState& St = SwbS(Boss);
	const FWRCreatureDef* Def = Boss->GetDef();
	const TArray<int32> Veins = WRSwbLocal::SwlVeinParts(Def);
	int32 K = INDEX_NONE;
	for (int32 k = 0; k < Veins.Num(); ++k) if (Def->Parts[Veins[k]].Id == Part.Id) K = k;
	const FVector At = WRSwbLocal::SwlPartPoint(Boss, Part);
	if (UWRSwampBossBody* Body = UWRSwampBossBody::Get(Boss); Body && K != INDEX_NONE) Body->Burst |= uint8(1 << K);
	for (AWRCharacterBase* V : SwbNear(Boss->GetWorld(), At, SwbST(Boss, TEXT("veinSprayRadius"), 400.f), Boss))
	{
		SwbBleed(V, SwbST(Boss, TEXT("veinBleed"), 40.f), Boss);
	}
	AddGorge(Boss, St, -SwbST(Boss, TEXT("veinGorgeLoss"), 20.f));
	WRFx::Broadcast(Boss, TEXT("swampboss.vein_burst"), At, (At - Boss->GetActorLocation()).Rotation(), 1.f, TEXT("swamp.bloat.spew"));
	WRSwbLocal::SwlVoice(Boss, TEXT("hurt"));
	(void)By;
}

void FWRSwampBoss::PhaseChanged(AWRBoss* Boss, int32 NewPhase)
{
	if (IsVharra(Boss))
	{
		VharraPhaseChanged(Boss, NewPhase);
	}
	// The minis have no phases (cook 2.13).
}

bool FWRSwampBoss::PlaceRunback(AWRBoss* Boss)
{
	if (IsVharra(Boss))
	{
		return VharraPlaceRunback(Boss);
	}
	if (!IsSwampBoss(Boss))
	{
		return false;
	}
	// Its shrine-stone at the arena's data spot (swamp.json arenas.<id>.runback), on the way you came in.
	const TSharedPtr<FJsonObject> Row = WRSwamp::Arena(Boss->CreatureId);
	if (!SwbHasPoint(Row, TEXT("runback")))
	{
		return false;
	}
	for (TActorIterator<AWRRunbackStone> It(Boss->GetWorld()); It; ++It)
	{
		if (It->BossId == Boss->CreatureId) { Boss->Runback = *It; return true; }
	}
	const FVector At = WRSwamp::Point(Row, TEXT("runback"));
	bool bOk = false;
	const FVector G = SwbLand(Boss->GetWorld(), At, bOk, 3000.f, 6000.f);
	if (!bOk)
	{
		return false;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	const FRotator Face(0.f, (Boss->GetHome() - G).Rotation().Yaw, 0.f);
	if (AWRRunbackStone* Stone = Boss->GetWorld()->SpawnActor<AWRRunbackStone>(AWRRunbackStone::StaticClass(), G, Face, Params))
	{
		Stone->BossId = Boss->CreatureId;
		Boss->Runback = Stone;
		UE_LOG(LogWildreach, Log, TEXT("SWAMPBOSS %s shrine-stone at %s"), *Boss->GetDisplayLabel(), *G.ToCompactString());
	}
	return true;
}

FName FWRSwampBoss::BossPart(const AWRBoss* Boss, bool bScarred, FName Fallback)
{
	if (IsVharra(Boss))
	{
		return VharraBossPart(Boss, bScarred, Fallback);
	}
	return Fallback;
}

float FWRSwampBoss::StanceTaken(AWRCharacterBase* Target, const FWRHit& Hit, float Stance)
{
	if (IsVharra(Target))
	{
		return VharraStanceTaken(Target, Hit, Stance);
	}
	const AWRBoss* Boss = Cast<AWRBoss>(Target);
	if (!IsSwampBoss(Boss))
	{
		return 1.f;
	}
	const FWRSwampBossState* St = SwbFind(Boss);
	// Mother Maw's open belly takes stance as it takes damage (x1.3).
	if (St && Boss->CreatureId == TEXT("mother_maw") && (St->bRearing || SwbNow(Boss) < St->BellyUntil))
	{
		if (Hit.ImpactPoint.IsZero() || Boss->IsNearLockPoint(Hit.ImpactPoint, TEXT("belly"), Boss->GetSimpleCollisionRadius() * 0.9f)) return SwbST(Boss, TEXT("bellyMult"), 1.3f);
	}
	(void)Stance;
	return 1.f;
}

float FWRSwampBoss::BuildupThresholdScale(const AWRCharacterBase* Who, int32 StatusId)
{
	if (IsVharra(Who))
	{
		return VharraBuildupThresholdScale(Who, StatusId);
	}
	return 1.f;
}

float FWRSwampBoss::CreatureIncoming(const AWRCreature* Creature, const FWRHit& Hit, float Damage)
{
	if (!Creature || Creature->CreatureId != TEXT("hex_totem"))
	{
		return Damage;
	}
	// A totem takes x2 from fire (x2 blunt is its physResist).
	const bool bFire = Hit.Element == WRTags::Element_Fire || Hit.BurnBuildup > 0.f || Hit.Buildups.FindRef(TEXT("burn")) > 0.f;
	return bFire ? Damage * SwbIT(TEXT("hex_warden"), TEXT("totemFireMult"), 2.f) : Damage;
}

float FWRSwampBoss::WadeScale(const AWRCharacterBase* Who)
{
	// Wading the Throat's risen flood (Vharra's phase 2): run x0.75. Both machines read the replicated flood.
	if (!Who || !Who->GetWorld() || !(Who->GetPlayerState() || (Cast<AWRCreature>(Who) && Cast<AWRCreature>(Who)->IsTamed())))
	{
		return 1.f;
	}
	const AWRSwampFlood* Flood = SwbFlood(Who->GetWorld(), Who->GetActorLocation(), 9000.f);
	if (!Flood || !Flood->IsFlooded())
	{
		return 1.f;
	}
	const FVector Feet = SwbFeet(Who);
	if (!Flood->InRisen(Feet) || UWRSwampBody::MountPasses(Who, TEXT("quag")))
	{
		return 1.f;
	}
	return SwbIT(TEXT("vharra"), TEXT("wadeRun"), 0.75f);
}

// ================================================================================================ for the other swamp systems

void FWRSwampBoss::FireAt(UWorld* World, const FVector& At, AActor* By)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}
	const double T = World->GetTimeSeconds();
	// Mother Maw's nest catches: she leaves the water to stand over it for 8 s.
	for (TActorIterator<AWRSwampBossProp> It(World); It; ++It)
	{
		AWRSwampBossProp* Nest = *It;
		if (Nest->Kind != TEXT("nest") || Nest->State != 0) continue;
		if (FVector::Dist2D(At, Nest->GetActorLocation()) > Nest->Size + SwbIT(TEXT("mother_maw"), TEXT("nestFireReach"), 150.f)) continue;
		if (FMath::Abs(At.Z - Nest->GetActorLocation().Z) > 600.f) continue;
		if (Nest->BurnUntil > 0.0 && T < Nest->BurnUntil + SwbIT(TEXT("mother_maw"), TEXT("nestRelight"), 20.f)) continue;   // still wet and smoking
		Nest->BurnUntil = T + SwbIT(TEXT("mother_maw"), TEXT("nestFireSeconds"), 8.f);
		Nest->SetState(1);
		WRFx::Broadcast(World, TEXT("fire.impact"), Nest->GetActorLocation() + FVector(0.f, 0.f, WRSwbLocal::SwlMoundZ(0.f) + 40.f), FRotator::ZeroRotator, 3.f, SwbSound(TEXT("firewall"), TEXT("stomp")));
		if (AWRBoss* Maw = SwbFindBoss(World, TEXT("mother_maw")); Maw && FVector::Dist2D(Maw->GetActorLocation(), Nest->GetActorLocation()) < 8000.f)
		{
			FWRSwampBossState& St = SwbS(Maw);
			St.DefendUntil = Nest->BurnUntil;
			WRSwbLocal::SwlVoice(Maw, TEXT("signature"));
			if (!Maw->bEngaged && SwbIsSurvivorSide(By)) Maw->Engage();
		}
		UE_LOG(LogWildreach, Log, TEXT("SWAMPBOSS Mother Maw's nest is burning (%s)"), *GetNameSafe(By));
	}
	// The Bloat set alight from under it (lit marsh gas, a fire pot at its feet).
	if (AWRBoss* Bloat = SwbFindBoss(World, TEXT("bloat")))
	{
		if (FVector::Dist(SwbFeet(Bloat), At) < Bloat->GetSimpleCollisionRadius() + SwbIT(TEXT("bloat"), TEXT("fireReach"), 300.f) && Bloat->Status)
		{
			Bloat->Status->ApplyStatus(WRTags::Status_Burn, SwbIT(TEXT("bloat"), TEXT("fireSeconds"), 4.f));
		}
	}
}

void FWRSwampBoss::Salted(AWRCharacterBase* Target, AActor* By)
{
	AWRBoss* Boss = Cast<AWRBoss>(Target);
	if (!Boss || Boss->CreatureId != TEXT("bloat") || !Boss->IsAlive() || !Boss->HasAuthority())
	{
		return;
	}
	// Salt on a leech: it vomits 40% of its gorge in a 4 m spray in front of it and is staggered 3 s.
	FWRSwampBossState& St = SwbS(Boss);
	if (St.Grabbed.IsValid()) ReleaseGrab(Boss, St);
	Vomit(Boss, St, SwbST(Boss, TEXT("vomitShare"), 0.4f));
	if (!Boss->bEngaged && SwbIsSurvivorSide(By)) Boss->Engage();
}

void FWRSwampBoss::Vomit(AWRBoss* Boss, FWRSwampBossState& St, float Share)
{
	const float Lost = St.Gorge * Share;
	AddGorge(Boss, St, -Lost);
	const float Reach = SwbST(Boss, TEXT("vomitRadius"), 400.f);
	const FVector Mouth = Boss->GetActorLocation() + Boss->GetActorForwardVector() * Boss->GetSimpleCollisionRadius();
	const FVector Spot = SwbFeet(Boss) + Boss->GetActorForwardVector() * (Boss->GetSimpleCollisionRadius() + Reach * 0.5f);
	for (AWRCharacterBase* V : SwbNear(Boss->GetWorld(), Spot, Reach, Boss))
	{
		const FVector To = (V->GetActorLocation() - Mouth).GetSafeNormal2D();
		if (FVector::DotProduct(To, Boss->GetActorForwardVector().GetSafeNormal2D()) < 0.2f) continue;
		SwbBleed(V, SwbST(Boss, TEXT("vomitBleed"), 40.f), Boss);
	}
	WRFx::Broadcast(Boss, TEXT("swampboss.vomit"), Mouth, Boss->GetActorRotation(), Reach / 400.f, TEXT("swamp.bloat.spew"));
	Puddle(Boss->GetWorld(), TEXT("bleedwater"), Spot, Reach * 0.7f, SwbST(Boss, TEXT("vomitPuddleSeconds"), 10.f), SwbST(Boss, TEXT("burstPuddleBleed"), 10.f), false, Boss);
	CutMove(Boss, 0.f);
	Boss->Status->Stagger(SwbST(Boss, TEXT("saltStagger"), 3.f));
	WRSwbLocal::SwlVoice(Boss, TEXT("hurt"));
	UE_LOG(LogWildreach, Log, TEXT("SWAMPBOSS the Bloat vomits %.0f%% of its gorge (salt)"), Lost);
}

void FWRSwampBoss::AddGorge(AWRBoss* Boss, FWRSwampBossState& St, float Amount)
{
	if (Amount > 0.f)
	{
		if (SwbNow(Boss) < St.NoGainUntil) return;   // frost: no gain
		Amount *= WRSwamp::BloatGorgeMult(Boss);
	}
	St.Gorge = FMath::Clamp(St.Gorge + Amount, 0.f, 100.f);
	if (UWRSwampBossBody* Body = UWRSwampBossBody::Get(Boss)) Body->Gorge = uint8(FMath::RoundToInt(St.Gorge));
}

bool FWRSwampBoss::IsHexed(const AActor* Creature)
{
	if (!Creature) return false;
	if (const UWRSwampBody* Body = UWRSwampBody::Get(Creature); Body && Body->IsHexed()) return true;
	const double* Until = SwbHexed().Find(Creature);
	return Until && *Until > SwbNow(Creature);
}

float FWRSwampBoss::HexAggroScale(const AActor* Creature)
{
	return IsHexed(Creature) ? SwbIT(TEXT("hex_warden"), TEXT("hexAggroMult"), 1.3f) : 1.f;
}

float FWRSwampBoss::HexCurse(const AActor* Creature)
{
	return IsHexed(Creature) ? SwbIT(TEXT("hex_warden"), TEXT("hexCurse"), 25.f) : 0.f;
}

void FWRSwampBoss::EndHex(UWorld* World)
{
	for (const TPair<TWeakObjectPtr<const AActor>, double>& H : SwbHexed())
	{
		if (AActor* C = const_cast<AActor*>(H.Key.Get())) FWRSwampAI::SetHexed(C, 0.f);
	}
	SwbHexed().Reset();
	(void)World;
}

AWRBoss* FWRSwampBoss::Bloat(const UWorld* World)
{
	return SwbFindBoss(World, TEXT("bloat"));
}

bool FWRSwampBoss::LeadInvasion(UWorld* World, const FVector& Target)
{
	AWRBoss* Boss = SwbFindBoss(World, TEXT("bloat"));
	if (!Boss || Boss->bEngaged || !Boss->HasAuthority())
	{
		return false;
	}
	// Out through Stormgut (its foot, its mouth) to the place it leads the Blood invasion to.
	FWRSwampBossState& St = SwbS(Boss);
	St.bInvading = true;
	St.InvadeTo = Target;
	St.InvadeWay.Reset();
	const FWRSwampData& D = FWRSwampData::Get();
	const TSharedPtr<FJsonObject>* Gut = nullptr;
	if (D.Root.IsValid() && D.Root->TryGetObjectField(TEXT("gut"), Gut))
	{
		if (SwbHasPoint(*Gut, TEXT("foot"))) St.InvadeWay.Add(WRSwamp::Point(*Gut, TEXT("foot")));
		if (SwbHasPoint(*Gut, TEXT("mouth"))) St.InvadeWay.Add(WRSwamp::Point(*Gut, TEXT("mouth")));
	}
	St.InvadeWay.Add(Target);
	St.InvadeIndex = 0;
	St.Prey = nullptr;
	St.ClashUntil = 0.0;
	Boss->SetAsleep(false);
	WRSwbLocal::SwlVoice(Boss, TEXT("aggro"));
	UE_LOG(LogWildreach, Log, TEXT("SWAMPBOSS the Bloat leads the Blood invasion to %s"), *Target.ToCompactString());
	return true;
}

void FWRSwampBoss::EndInvasion(UWorld* World)
{
	AWRBoss* Boss = SwbFindBoss(World, TEXT("bloat"));
	if (!Boss || !Boss->HasAuthority())
	{
		return;
	}
	// Home the way it came: back through Stormgut to its loop.
	FWRSwampBossState& St = SwbS(Boss);
	if (!St.bInvading)
	{
		return;
	}
	St.bInvading = false;
	TArray<FVector> Back;
	for (int32 i = St.InvadeWay.Num() - 2; i >= 0; --i) Back.Add(St.InvadeWay[i]);
	if (St.Route.Num() == 0) BuildRoute(Boss, St);
	if (St.Route.Num() > 0) Back.Add(St.Route[0]);
	St.InvadeWay = Back;
	St.InvadeIndex = 0;
	UE_LOG(LogWildreach, Log, TEXT("SWAMPBOSS the Bloat goes home"));
}

// ================================================================================================ grabs and waves

void FWRSwampBoss::StartGrab(AWRBoss* Boss, FWRSwampBossState& St, AWRCharacterBase* Victim, FName Kind, float Seconds, float Dps)
{
	if (!Boss || !Victim || !Victim->IsAlive())
	{
		return;
	}
	if (St.Grabbed.IsValid()) ReleaseGrab(Boss, St);
	const double T = SwbNow(Boss);
	St.Grabbed = Victim;
	St.GrabKind = Kind;
	St.GrabUntil = T + Seconds;
	St.GrabTickAt = T + 0.5;
	St.GrabDps = Dps;
	St.GrabFree = 0.f;
	St.bGrabWasDodging = Victim->HasTag(WRTags::State_Dodging) || Victim->HasTag(WRTags::State_IFrames);
	UWRSwampBossBody* Body = UWRSwampBossBody::Get(Boss);
	if (Kind == TEXT("latch"))
	{
		// The latch holds you where you stand (you can still swing, and roll for it).
		St.bGrabAttached = false;
		if (Victim->Status) Victim->Status->ApplyStatus(WRTags::Status_Rooted, Seconds + 0.2f);
		if (Body) Body->Set(WRSwampBossFlag::Latched, true);
	}
	else
	{
		St.bGrabAttached = true;
		const bool bRoll = Kind == TEXT("roll");
		const FVector Rel(Boss->GetSimpleCollisionRadius() * (bRoll ? 1.05f : 0.8f), 0.f, bRoll ? -Boss->GetSimpleCollisionHalfHeight() * 0.25f : 0.f);
		SwbHold(Victim, Boss, Rel, true);
		if (Victim->Status) Victim->Status->Stagger(Seconds + 0.4f);
		if (bRoll)
		{
			St.BellyUntil = St.GrabUntil;
			if (Body) Body->Set(WRSwampBossFlag::Latched, true);
		}
	}
	if (Boss->Attacks.IsValidIndex(Boss->Current) && Boss->StrikeState == AWRBoss::EStrike::Strike)
	{
		Boss->StrikeEnds = FMath::Max(Boss->StrikeEnds, St.GrabUntil);
	}
	UE_LOG(LogWildreach, Log, TEXT("SWAMPBOSS %s %s %s (%.1f s)"), *Boss->GetDisplayLabel(), *Kind.ToString(), *Victim->GetName(), Seconds);
}

void FWRSwampBoss::TickGrab(AWRBoss* Boss, FWRSwampBossState& St, float DeltaSeconds)
{
	AWRCharacterBase* V = St.Grabbed.Get();
	const double T = SwbNow(Boss);
	if (!V || !V->IsAlive() || !Boss->IsAlive() || T >= St.GrabUntil)
	{
		ReleaseGrab(Boss, St);
		return;
	}
	const FVector To = V->GetActorLocation() - Boss->GetActorLocation();
	Boss->SetActorRotation(FRotator(0.f, To.Rotation().Yaw, 0.f));
	if (St.GrabKind == TEXT("latch"))
	{
		// Burning, it can't hold; a roll shakes it off 60% of the time; pulled out of reach, it lets go.
		const bool bDodge = V->HasTag(WRTags::State_Dodging) || V->HasTag(WRTags::State_IFrames);
		if (Boss->HasTag(WRTags::Status_Burn)
			|| (bDodge && !St.bGrabWasDodging && FMath::FRand() < SwbST(Boss, TEXT("latchRollFree"), 0.6f))
			|| To.Size2D() > Boss->GetSimpleCollisionRadius() + V->GetSimpleCollisionRadius() + SwbST(Boss, TEXT("latchReach"), 260.f))
		{
			WRFx::Broadcast(Boss, TEXT("blood.hit"), V->GetActorLocation(), FRotator::ZeroRotator, 0.7f, NAME_None);
			ReleaseGrab(Boss, St);
			return;
		}
		St.bGrabWasDodging = bDodge;
		// It drinks: +8% a second off a survivor or a tame (+4% off a beast).
		AddGorge(Boss, St, (V->GetPlayerState() ? SwbST(Boss, TEXT("gorgeLatchPlayer"), 8.f) : SwbST(Boss, TEXT("gorgeLatchCreature"), 4.f)) * DeltaSeconds);
	}
	if (T >= St.GrabTickAt)
	{
		St.GrabTickAt = T + 0.5;
		const bool bLatch = St.GrabKind == TEXT("latch");
		if (St.GrabDps > 0.f || bLatch)
		{
			SwbHit(Boss, V, St.GrabDps * 0.5f, bLatch ? SwbST(Boss, TEXT("latchBleedPerSecond"), 20.f) * 0.5f : 0.f, 0.f, true, false, 1, true);
		}
		if (St.GrabKind == TEXT("roll")) WRFx::Broadcast(Boss, TEXT("mire.splash"), SwbFeet(Boss) + FVector(0.f, 0.f, 60.f), FRotator::ZeroRotator, 1.2f, NAME_None);
		else if (bLatch) WRFx::Broadcast(Boss, TEXT("mire.bloodfeed"), V->GetActorLocation(), FRotator::ZeroRotator, 0.8f, NAME_None);
	}
}

void FWRSwampBoss::ReleaseGrab(AWRBoss* Boss, FWRSwampBossState& St)
{
	AWRCharacterBase* V = St.Grabbed.Get();
	const FName Kind = St.GrabKind;
	const bool bAttached = St.bGrabAttached;
	St.Grabbed = nullptr;
	St.GrabKind = NAME_None;
	St.bGrabAttached = false;
	if (!Boss)
	{
		return;
	}
	if (UWRSwampBossBody* Body = UWRSwampBossBody::Get(Boss))
	{
		Body->Set(WRSwampBossFlag::Latched, false);
		Body->Set(WRSwampBossFlag::JawOpen, false);
	}
	if (V)
	{
		if (bAttached)
		{
			SwbHold(V, Boss, FVector::ZeroVector, false);
			if (V->IsAlive())
			{
				// Spat out of the roll, flung off the sickles.
				const float K = Kind == TEXT("roll") ? SwbST(Boss, TEXT("rollThrow"), 450.f) : SwbST(Boss, TEXT("harvestThrow"), 550.f);
				V->LaunchCharacter((V->GetActorLocation() - Boss->GetActorLocation()).GetSafeNormal2D() * K + FVector(0.f, 0.f, K * 0.5f), true, true);
			}
		}
		else if (V->Status)
		{
			V->Status->ClearStatus(WRTags::Status_Rooted);
		}
	}
	if (Kind != NAME_None && Boss->HasAuthority())
	{
		const FWRBossAttack* Move = Boss->Attacks.IsValidIndex(Boss->Current) ? &Boss->Attacks[Boss->Current] : nullptr;
		CutMove(Boss, Move && Move->bRow ? Move->Row.Punish : 1.f);
	}
}

void FWRSwampBoss::StartWave(AWRBoss* Boss, FWRSwampBossState& St, const FVector& From, float Radius, float Damage, float Height, bool bLow, float Knock)
{
	St.bWave = true;
	St.bWaveLow = bLow;
	St.WaveAt = SwbNow(Boss);
	St.WaveFrom = From;
	St.WaveRadius = Radius;
	St.WaveDamage = Damage;
	St.WaveHeight = Height;
	St.WaveKnock = Knock;
	St.WaveHit.Reset();
	WRFx::Broadcast(Boss, TEXT("shockwave"), From, FRotator::ZeroRotator, Radius / 100.f, SwbSound(TEXT("water.splash"), TEXT("stomp")));
}

void FWRSwampBoss::TickWave(AWRBoss* Boss, FWRSwampBossState& St)
{
	if (!St.bWave)
	{
		return;
	}
	// A ring rolling out at the boss wave speed: whoever its front passes takes it once (a low one passes under a jump).
	const float Front = float(SwbNow(Boss) - St.WaveAt) * WRTune(TEXT("bosses.waveSpeed"), 1200.f);
	const float Depth = WRTune(TEXT("bosses.waveDepth"), 180.f);
	for (AWRCharacterBase* V : SwbNear(Boss->GetWorld(), St.WaveFrom, St.WaveRadius, Boss))
	{
		if (St.WaveHit.Contains(V)) continue;
		const float D = FVector::Dist2D(V->GetActorLocation(), St.WaveFrom) - V->GetSimpleCollisionRadius();
		if (D > Front || D < Front - Depth) continue;
		if (FMath::Abs(SwbFeet(V).Z - St.WaveFrom.Z) > St.WaveHeight + 150.f) continue;
		St.WaveHit.Add(V);
		if (St.bWaveLow && V->HasTag(WRTags::State_JumpLegsInvuln)) continue;
		SwbHit(Boss, V, St.WaveDamage, 0.f, 0.f, false, St.bWaveLow, 3, false, St.WaveKnock);
	}
	if (Front > St.WaveRadius + Depth) St.bWave = false;
}

// ================================================================================================ Mother Maw's square

bool FWRSwampBoss::BreakSectionAt(AWRBoss* Boss, const FVector& At)
{
	// The head-slam breaks the boardwalk section it lands on: the planks go, whoever stood there drops into the water.
	AWRSwampBossProp* Best = nullptr;
	float BestD = TNumericLimits<float>::Max();
	const float HalfW = WRSwbLocal::SwlDeckWidth() * 0.5f;
	for (TActorIterator<AWRSwampBossProp> It(Boss->GetWorld()); It; ++It)
	{
		if (It->Kind != TEXT("walk") || It->State == 1) continue;
		const FVector L = It->GetActorTransform().InverseTransformPosition(At);
		if (FMath::Abs(L.X) > It->Size * 0.5f + 100.f || FMath::Abs(L.Y) > HalfW + SwbST(Boss, TEXT("slamBreakReach"), 160.f) || FMath::Abs(L.Z) > 400.f) continue;
		const float D = FVector::Dist2D(It->GetActorLocation(), At);
		if (D < BestD) { BestD = D; Best = *It; }
	}
	if (!Best)
	{
		return false;
	}
	Best->Mender = nullptr;
	Best->SetState(1);
	WRFx::Broadcast(Boss, TEXT("swamp.plank_snap"), Best->GetActorLocation(), Best->GetActorRotation(), Best->Size / 300.f, TEXT("swamp.plank.break"));
	WRFx::Broadcast(Boss, TEXT("mire.splash"), FVector(Best->GetActorLocation().X, Best->GetActorLocation().Y, FWRSwampData::Get().SeaZ), FRotator::ZeroRotator, 2.f, NAME_None);
	UE_LOG(LogWildreach, Log, TEXT("SWAMPBOSS Mother Maw breaks a section of the square"));
	return true;
}

void FWRSwampBoss::TickMends(UWorld* World)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}
	// 2 wood and 3 s of work beside a broken section (not on it): it holds again. Moving off, being knocked down or a
	// fresh break ends the mend.
	const double T = World->GetTimeSeconds();
	const float Need = SwbIT(TEXT("mother_maw"), TEXT("mendSeconds"), 3.f);
	const int32 Wood = FMath::RoundToInt(SwbIT(TEXT("mother_maw"), TEXT("mendWood"), 2.f));
	for (TActorIterator<AWRSwampBossProp> It(World); It; ++It)
	{
		AWRSwampBossProp* Sec = *It;
		if (Sec->Kind != TEXT("walk") || !Sec->Mender.IsValid()) continue;
		AWRCharacterBase* Who = Sec->Mender.Get();
		AWRPlayerController* PC = Cast<AWRPlayerController>(Who->GetController());
		if (!Who->IsAlive() || Sec->State != 1)
		{
			Sec->Mender = nullptr;
			continue;
		}
		if (FVector::Dist2D(Who->GetActorLocation(), Sec->MendFrom) > SwbIT(TEXT("mother_maw"), TEXT("mendMoveCancel"), 150.f) || Who->HasTag(WRTags::Status_Staggered) || Who->HasTag(WRTags::State_Attacking))
		{
			Sec->Mender = nullptr;
			if (PC) PC->ClientNotify(TEXT("The mend was broken off."));
			continue;
		}
		if (T - Sec->MendStart < Need) continue;
		UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Who);
		Sec->Mender = nullptr;
		if (!Inv || Inv->CountOf(TEXT("wood")) < Wood)
		{
			if (PC) PC->ClientNotify(FString::Printf(TEXT("You need %d wood to mend it."), Wood));
			continue;
		}
		Inv->RemoveItem(TEXT("wood"), Wood);
		Sec->SetState(2);
		WRFx::Broadcast(World, TEXT("gather.wood"), Sec->GetActorLocation(), Sec->GetActorRotation(), 2.f, TEXT("swamp.plank.repair"));
		if (PC) PC->ClientNotify(TEXT("The section holds again."));
		UE_LOG(LogWildreach, Log, TEXT("SWAMPBOSS %s mends a section of Mother Maw's square"), *Who->GetName());
	}
}

void FWRSwampBoss::SpawnYoung(AWRBoss* Boss, FName CreatureId, int32 Count, float Health, const FVector& At)
{
	UWorld* World = Boss->GetWorld();
	FWRSwampBossState& St = SwbS(Boss);
	// The tame-punish echo: the young go for the furthest tame first.
	AWRCharacterBase* FarTame = nullptr;
	float FarD = 0.f;
	for (AWRCharacterBase* V : SwbFoes(Boss, Boss->ChaseRadius))
	{
		const AWRCreature* C = Cast<AWRCreature>(V);
		const float D = FVector::Dist2D(V->GetActorLocation(), Boss->GetActorLocation());
		if (C && C->IsTamed() && D > FarD) { FarD = D; FarTame = V; }
	}
	AWRCharacterBase* Prey = FarTame ? FarTame : Boss->AttackTarget.Get();
	for (int32 i = 0; i < Count; ++i)
	{
		const float A = 2.f * PI * (i + 0.25f) / FMath::Max(1, Count) + FMath::FRandRange(-0.3f, 0.3f);
		bool bOk = false;
		const FVector Spot = SwbGround(World, At + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * (St.NestRadius + 200.f), bOk);
		if (AWRCreature* Young = AWRCreature::SpawnCreature(World, CreatureId, bOk ? Spot : At, FRotator(0.f, FMath::RadiansToDegrees(A), 0.f)))
		{
			SwbSetHealth(Young, Health);
			if (Prey) Young->SetAggroTarget(Prey);
			St.Young.Add(Young);
			WRFx::Broadcast(World, TEXT("mire.splash"), Young->GetActorLocation(), FRotator::ZeroRotator, 1.2f, SwbSound(TEXT("creature.murkjaw.aggro"), TEXT("stomp")));
		}
	}
	UE_LOG(LogWildreach, Log, TEXT("SWAMPBOSS Mother Maw calls %d young from the nest"), Count);
}

// ================================================================================================ the Hex-Warden

void FWRSwampBoss::SpawnTotems(AWRBoss* Boss, FWRSwampBossState& St)
{
	UWorld* World = Boss->GetWorld();
	if (!FWRCreatureRegistry::Get().Find(TEXT("hex_totem")))
	{
		return;
	}
	const float Health = SwbST(Boss, TEXT("totemHealth"), 400.f);
	for (int32 i = 0; i < 4; ++i)
	{
		AWRSwampBossProp* Pole = St.TotemProps[i].Get();
		if (!Pole) continue;
		Pole->SetState(0);
		AWRCreature* C = St.Totems[i].Get();
		if (!C || !C->IsAlive())
		{
			if (C) C->Destroy();
			C = AWRCreature::SpawnCreature(World, TEXT("hex_totem"), Pole->GetActorLocation(), Pole->GetActorRotation());
			if (C)
			{
				C->SetHome(C->GetActorLocation());
				C->GetCharacterMovement()->DisableMovement();
				UWRSwampBossBody::Attach(C);
			}
		}
		if (C) SwbSetHealth(C, Health);
		St.Totems[i] = C;
		Pole->TotemBody = C;
	}
	St.TotemsDown = 0;
	St.HitCount = 0;
}

void FWRSwampBoss::TotemBroken(AWRBoss* Boss, FWRSwampBossState& St, int32 Index)
{
	St.TotemsDown |= uint8(1 << Index);
	AWRSwampBossProp* Pole = St.TotemProps[Index].Get();
	static const TCHAR* Names[] = { TEXT("crow"), TEXT("bone"), TEXT("blood"), TEXT("voice") };
	if (Pole)
	{
		Pole->SetState(1);
		WRFx::Broadcast(Boss, TEXT("part.break"), Pole->GetActorLocation() + FVector(0.f, 0.f, Pole->Size * 0.4f), FRotator::ZeroRotator, 2.f, SwbSound(TEXT("part.break"), TEXT("stomp")));
		if (Index == 0) WRFx::Broadcast(Boss, TEXT("swampboss.crows"), Pole->GetActorLocation() + FVector(0.f, 0.f, Pole->Size), FRotator::ZeroRotator, 0.6f, TEXT("swamp.crows"));
	}
	UE_LOG(LogWildreach, Log, TEXT("SWAMPBOSS the %s totem breaks: its curse is gone"), Names[Index & 3]);
	if (!Boss->IsAlive())
	{
		return;
	}
	// The crow totem gone mid-scatter: she can't hold the crows.
	if (St.bScattered && Index == 0) Reform(Boss, St);
	if (Pole && FVector::Dist(Boss->GetActorLocation(), Pole->GetActorLocation()) < SwbST(Boss, TEXT("totemStaggerRadius"), 1500.f))
	{
		CutMove(Boss, 0.f);
		Boss->Status->Stagger(SwbST(Boss, TEXT("totemStagger"), 2.f));
		WRSwbLocal::SwlVoice(Boss, TEXT("hurt"));
	}
	if (Index == 3) TickMimic(Boss, St, true);   // the voice is gone: the tames come back
}

void FWRSwampBoss::Scatter(AWRBoss* Boss, FWRSwampBossState& St)
{
	// Forty crows (no body to hit), reforming 1.2 s later at a standing totem, nearest the one she's after.
	const double T = SwbNow(Boss);
	AWRCharacterBase* Target = Boss->AttackTarget.Get();
	int32 Pick = INDEX_NONE;
	float BestD = TNumericLimits<float>::Max();
	for (int32 i = 0; i < 4; ++i)
	{
		if (!St.TotemProps[i].IsValid() || !St.Totems[i].IsValid() || !St.Totems[i]->IsAlive()) continue;
		const float D = Target ? FVector::Dist2D(St.TotemProps[i]->GetActorLocation(), Target->GetActorLocation()) + FMath::FRandRange(0.f, 600.f) : FMath::FRand();
		if (D < BestD) { BestD = D; Pick = i; }
	}
	if (Pick == INDEX_NONE)
	{
		return;   // nothing to reform at
	}
	St.bScattered = true;
	St.ReformTotem = Pick;
	St.ReformAt = T + SwbST(Boss, TEXT("scatterReform"), 1.2f);
	if (UAbilitySystemComponent* ASC = Boss->GetAbilitySystemComponent()) ASC->AddLooseGameplayTag(WRTags::State_Invulnerable, 1, EGameplayTagReplicationState::TagOnly);
	const FVector From = Boss->GetActorLocation();
	const FVector To = St.TotemProps[Pick]->GetActorLocation() + FVector(0.f, 0.f, 200.f);
	WRFx::Broadcast(Boss, TEXT("swampboss.crows"), From, FRotator::ZeroRotator, 1.f, TEXT("swamp.crows"));
	WRFx::Broadcast(Boss, TEXT("swampboss.crow_stream"), From, (To - From).Rotation(), FVector::Dist(From, To) / 100.f, SwbSound(TEXT("swamp.hex.wings"), TEXT("swamp.crows")));
	Boss->SetActorHiddenInGame(true);
	Boss->SetActorEnableCollision(false);
	Boss->GetCharacterMovement()->DisableMovement();
	if (UWRSwampBossBody* Body = UWRSwampBossBody::Get(Boss)) Body->Set(WRSwampBossFlag::Scattered, true);
	UE_LOG(LogWildreach, Log, TEXT("SWAMPBOSS the Hex-Warden scatters into crows (reforms at the %d totem)"), Pick);
}

void FWRSwampBoss::Reform(AWRBoss* Boss, FWRSwampBossState& St)
{
	if (!St.bScattered)
	{
		return;
	}
	St.bScattered = false;
	AWRCharacterBase* Target = Boss->AttackTarget.IsValid() ? Boss->AttackTarget.Get() : Boss->PickBossTarget();
	const AWRSwampBossProp* Pole = St.TotemProps[FMath::Clamp(St.ReformTotem, 0, 3)].Get();
	FVector Spot = Pole ? Pole->GetActorLocation() : Boss->GetActorLocation();
	if (Pole && Target)
	{
		Spot += (Target->GetActorLocation() - Pole->GetActorLocation()).GetSafeNormal2D() * SwbST(Boss, TEXT("reformOffset"), 260.f);
	}
	bool bOk = false;
	const FVector G = SwbGround(Boss->GetWorld(), Spot, bOk, 1500.f, 3000.f);
	Boss->SetActorLocation((bOk ? G : Spot) + FVector(0.f, 0.f, Boss->GetSimpleCollisionHalfHeight() + 5.f), false, nullptr, ETeleportType::TeleportPhysics);
	if (Target) Boss->SetActorRotation(FRotator(0.f, (Target->GetActorLocation() - Boss->GetActorLocation()).Rotation().Yaw, 0.f));
	Boss->SetActorHiddenInGame(false);
	Boss->SetActorEnableCollision(true);
	Boss->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	if (UAbilitySystemComponent* ASC = Boss->GetAbilitySystemComponent()) ASC->RemoveLooseGameplayTag(WRTags::State_Invulnerable, 100);
	if (UWRSwampBossBody* Body = UWRSwampBossBody::Get(Boss)) Body->Set(WRSwampBossFlag::Scattered, false);
	WRFx::Broadcast(Boss, TEXT("swampboss.crows"), Boss->GetActorLocation(), FRotator::ZeroRotator, 0.8f, SwbSound(TEXT("swamp.hex.wings"), TEXT("swamp.crows")));
	St.ReformTotem = INDEX_NONE;
	// Out of the crows, straight into the reform strike (if she has someone to strike).
	if (Boss->bEngaged && Boss->IsAlive() && Target && !Boss->HasTag(WRTags::Status_Staggered))
	{
		StartRow(Boss, TEXT("reform_strike"), Target);
	}
}

void FWRSwampBoss::TickMimic(AWRBoss* Boss, FWRSwampBossState& St, bool bEnd)
{
	const double T = SwbNow(Boss);
	for (int32 i = St.Mimicked.Num() - 1; i >= 0; --i)
	{
		FWRSwampBossState::FMimic& M = St.Mimicked[i];
		AWRCreature* Tame = M.Tame.Get();
		if (!Tame || !Tame->IsAlive())
		{
			St.Mimicked.RemoveAt(i);
			continue;
		}
		if (M.Until <= 0.0)
		{
			if (bEnd) St.Mimicked.RemoveAt(i);   // her call was cut off before it took them
			continue;
		}
		if (bEnd || T >= M.Until || !Boss->IsAlive())
		{
			// Given back: its order and its fight as they were.
			Tame->TameOrder = M.Order;
			Tame->bTameAggressive = M.bAggressive;
			St.Mimicked.RemoveAt(i);
			continue;
		}
		// Held at her side: it doesn't fight anyone, it just goes and stands there.
		Tame->SetAggroTarget(nullptr);
		const FVector To = (M.Side - Tame->GetActorLocation()) * FVector(1.f, 1.f, 0.f);
		if (To.Size2D() > 150.f) Tame->AddMovementInput(To.GetSafeNormal2D(), 1.f);
	}
}

void FWRSwampBoss::TickBloodTotem(AWRBoss* Boss, FWRSwampBossState& St)
{
	if (!St.Totems[2].IsValid() || !St.Totems[2]->IsAlive())
	{
		St.BleedingNear.Reset();
		return;
	}
	const double T = SwbNow(Boss);
	if (T < St.NextBloodLook)
	{
		return;
	}
	St.NextBloodLook = T + 0.2;
	// The blood totem: whenever anyone near her procs bleed, she heals 2% of her max.
	TSet<TWeakObjectPtr<AWRCharacterBase>> Now;
	for (AWRCharacterBase* V : SwbNear(Boss->GetWorld(), Boss->GetActorLocation(), SwbST(Boss, TEXT("bloodNearRadius"), 1500.f), Boss))
	{
		if (!V->HasTag(WRTags::Status_Bleeding)) continue;
		Now.Add(V);
		if (St.BleedingNear.Contains(V)) continue;
		SwbHeal(Boss, SwbMaxHealth(Boss) * SwbST(Boss, TEXT("bloodHealShare"), 0.02f));
		const FVector D = Boss->GetActorLocation() - V->GetActorLocation();
		WRFx::Broadcast(Boss, TEXT("blood.thread"), V->GetActorLocation(), D.Rotation(), D.Size() / 100.f, SwbSound(TEXT("blood.lifesteal"), TEXT("blood.hit")));
	}
	St.BleedingNear = Now;
}

// ================================================================================================ thrown things and puddles

void FWRSwampBoss::FireShot(AWRBoss* Boss, FName Kind, const FVector& From, const FVector& To, const FWRHit& Hit, float Speed, bool bLob)
{
	UWorld* World = Boss ? Boss->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}
	const FVector D = To - From;
	const float Flat = FMath::Max(1.f, float(D.Size2D()));
	const float S = FMath::Max(100.f, Speed);
	FVector Vel;
	float G = 0.f;
	if (bLob)
	{
		// A lob: the horizontal speed given, the arc's lift worked out to come down on the spot.
		G = 980.f;
		const float Time = Flat / S;
		Vel = D.GetSafeNormal2D() * S;
		Vel.Z = (float(D.Z) + 0.5f * G * Time * Time) / Time;
	}
	else
	{
		Vel = D.GetSafeNormal() * S;
	}
	const FTransform Where(Vel.Rotation(), From);
	AWRSwampShot* Shot = World->SpawnActorDeferred<AWRSwampShot>(AWRSwampShot::StaticClass(), Where, Boss, Boss, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Shot)
	{
		return;
	}
	Shot->Kind = Kind;
	Shot->Start = From;
	Shot->Velocity = Vel;
	Shot->Gravity = G;
	Shot->LaunchAt = SwbServerTime(World);
	Shot->Hit = Hit;
	Shot->Hit.bProjectile = true;
	Shot->Hit.bReflectable = !Hit.bReaction;   // a hex bolt can be thrown back by a reflector (cookpass-combat 3.12)
	Shot->MaxRange = bLob ? Flat * 1.3f + 400.f : SwbST(Boss, TEXT("boltRange"), 2500.f);
	Shot->Radius = Kind == TEXT("clot") ? 90.f : SwbST(Boss, TEXT("boltRadius"), 45.f);
	Shot->FinishSpawning(Where);
}

AWRSwampBossProp* FWRSwampBoss::Puddle(UWorld* World, FName Kind, const FVector& At, float Radius, float Seconds, float BleedPerSecond, bool bSlow, AActor* Source)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}
	bool bOk = false;
	const FVector G = SwbGround(World, At, bOk, 300.f, 1500.f);
	// On water it floats on the surface; on mud it lies on it.
	FVector Spot = bOk ? G : At;
	if (WRSwamp::IsWet(Spot)) Spot.Z = FWRSwampData::Get().SeaZ;
	const FTransform Where(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), Spot);
	AWRSwampBossProp* P = World->SpawnActorDeferred<AWRSwampBossProp>(AWRSwampBossProp::StaticClass(), Where, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!P)
	{
		return nullptr;
	}
	P->Kind = TEXT("puddle");
	P->Variant = Kind == TEXT("clot") ? 1 : 0;
	P->Size = Radius;
	P->FinishSpawning(Where);
	P->MakeHazard(BleedPerSecond, bSlow, Seconds, Source);
	return P;
}

// ================================================================================================ interact: mending the square

namespace WRSwbLocal
{
	/** How far a point is from a section's deck (2D, 0 inside its footprint). */
	float SwlDeckDist(const AWRSwampBossProp* Sec, const FVector& P)
	{
		const FVector L = Sec->GetActorTransform().InverseTransformPosition(P);
		const float Dx = FMath::Max(0.f, float(FMath::Abs(L.X)) - Sec->Size * 0.5f);
		const float Dy = FMath::Max(0.f, float(FMath::Abs(L.Y)) - SwlDeckWidth() * 0.5f);
		return FMath::Sqrt(Dx * Dx + Dy * Dy) + (FMath::Abs(L.Z) > 450.f ? 10000.f : 0.f);
	}

	/** The broken section of Mother Maw's square within reach of a survivor, nearest first. */
	AWRSwampBossProp* SwlBrokenNear(const AActor* Who)
	{
		if (!Who) return nullptr;
		AWRSwampBossProp* Best = nullptr;
		float BestD = SwbIT(TEXT("mother_maw"), TEXT("mendReach"), 350.f);
		for (TActorIterator<AWRSwampBossProp> It(Who->GetWorld()); It; ++It)
		{
			if (It->Kind != TEXT("walk") || It->State != 1) continue;
			const float D = SwlDeckDist(*It, Who->GetActorLocation());
			if (D < BestD) { BestD = D; Best = *It; }
		}
		return Best;
	}
}

bool FWRSwampBoss::Prompt(const AWRPlayerController* PC, FString& OutTarget, FString& OutVerb)
{
	if (VharraPrompt(PC, OutTarget, OutVerb))
	{
		return true;
	}
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn || !WRSwbLocal::SwlBrokenNear(Pawn))
	{
		return false;
	}
	const UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Pawn);
	const int32 Wood = FMath::RoundToInt(SwbIT(TEXT("mother_maw"), TEXT("mendWood"), 2.f));
	OutTarget = TEXT("Broken boardwalk");
	OutVerb = Inv && Inv->CountOf(TEXT("wood")) >= Wood ? FString::Printf(TEXT("Mend (%d wood, %.0f s)"), Wood, SwbIT(TEXT("mother_maw"), TEXT("mendSeconds"), 3.f)) : FString::Printf(TEXT("Mend: needs %d wood"), Wood);
	return true;
}

bool FWRSwampBoss::ClientInteract(AWRPlayerController* PC)
{
	if (VharraClientInteract(PC))
	{
		return true;
	}
	if (!PC || !WRSwbLocal::SwlBrokenNear(PC->GetPawn()))
	{
		return false;
	}
	PC->SendCommand(TEXT("world swamp boss mend"));   // WRWorld -> WRSwamp::Command -> Command(PC, {swampboss, mend})
	return true;
}

FString FWRSwampBoss::Command(AWRPlayerController* PC, const TArray<FString>& Args)
{
	const FString Vh = VharraCommand(PC, Args);
	if (!Vh.IsEmpty())
	{
		return Vh;
	}
	if (!PC || !Args.Contains(TEXT("mend")))
	{
		return FString();
	}
	AWRCharacterBase* Who = Cast<AWRCharacterBase>(PC->GetPawn());
	if (!Who || !Who->IsAlive())
	{
		return TEXT("You can't do that now.");
	}
	AWRSwampBossProp* Sec = WRSwbLocal::SwlBrokenNear(Who);
	if (!Sec)
	{
		return TEXT("No broken section of the boardwalk within reach.");
	}
	if (Sec->OnDeck(SwbFeet(Who)))
	{
		return TEXT("Stand beside the gap to mend it, not in it.");
	}
	if (Sec->Mender.IsValid() && Sec->Mender.Get() != Who)
	{
		return TEXT("Someone is already mending that section.");
	}
	const int32 Wood = FMath::RoundToInt(SwbIT(TEXT("mother_maw"), TEXT("mendWood"), 2.f));
	const UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Who);
	if (!Inv || Inv->CountOf(TEXT("wood")) < Wood)
	{
		return FString::Printf(TEXT("You need %d wood to mend it."), Wood);
	}
	Sec->Mender = Who;
	Sec->MendStart = Who->GetWorld()->GetTimeSeconds();
	Sec->MendFrom = Who->GetActorLocation();
	WRFx::Broadcast(Who, TEXT("gather.wood"), Sec->GetActorLocation(), Sec->GetActorRotation(), 1.f, TEXT("swamp.plank.creak"));
	return FString::Printf(TEXT("Mending the boardwalk (%.0f s): hold still."), SwbIT(TEXT("mother_maw"), TEXT("mendSeconds"), 3.f));
}

// ================================================================================================ placing the arenas

bool UWRSwampArenas::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UWRSwampArenas::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (InWorld.GetNetMode() == NM_Client || !WRSwamp::IsLive() || FWRSwampData::Get().Arenas.Num() == 0)
	{
		return;
	}
	StrikeHandle = WRStorm::OnStrikeLanded().AddStatic(&FWRSwampBoss::StrikeLanded);
	ShockHandle = WRSwamp::OnWaterShocked().AddUObject(this, &UWRSwampArenas::OnWaterShocked);
	SaltHandle = WRSwamp::OnSalted().AddUObject(this, &UWRSwampArenas::OnSalted);
	InWorld.GetTimerManager().SetTimer(StepTimer, FTimerDelegate::CreateUObject(this, &UWRSwampArenas::Step), 5.f, true, 2.f);
}

void UWRSwampArenas::Deinitialize()
{
	if (StrikeHandle.IsValid()) { WRStorm::OnStrikeLanded().Remove(StrikeHandle); StrikeHandle.Reset(); }
	if (ShockHandle.IsValid()) { WRSwamp::OnWaterShocked().Remove(ShockHandle); ShockHandle.Reset(); }
	if (SaltHandle.IsValid()) { WRSwamp::OnSalted().Remove(SaltHandle); SaltHandle.Reset(); }
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(StepTimer);
	Super::Deinitialize();
}

void UWRSwampArenas::Step()
{
	static const FName Ids[] = { TEXT("vharra"), TEXT("mother_maw"), TEXT("hex_warden"), TEXT("bloat") };
	for (const FName Id : Ids)
	{
		if (Placed.Contains(Id)) continue;
		const TSharedPtr<FJsonObject> Row = WRSwamp::Arena(Id);
		if (!Row.IsValid() || !FWRCreatureRegistry::Get().Find(Id)) { Placed.Add(Id); continue; }
		if (Place(Id, Row)) Placed.Add(Id);
	}
	if (Placed.Num() >= UE_ARRAY_COUNT(Ids))
	{
		GetWorld()->GetTimerManager().ClearTimer(StepTimer);
	}
}

bool UWRSwampArenas::Place(FName Arena, const TSharedPtr<FJsonObject>& Row)
{
	if (Arena == TEXT("vharra"))
	{
		return FWRSwampBoss::VharraPlaceArena(this, Row);
	}
	UWorld* World = GetWorld();
	FVector At = SwbCentre(Row);
	if (Arena == TEXT("mother_maw") && SwbHasPoint(Row, TEXT("nest"))) At = WRSwamp::Point(Row, TEXT("nest"));
	if (Arena == TEXT("bloat") && SwbHasPoint(Row, TEXT("lair"))) At = WRSwamp::Point(Row, TEXT("lair"));
	if (Arena == TEXT("hex_warden") && Row->HasField(TEXT("top"))) At.Z = float(SwbNum(Row, TEXT("top"), 0.0)) * 100.f;
	// Only once its ground is in (a survivor near enough to stream it) and only once a world.
	bool bNear = false;
	for (TActorIterator<AWRCharacter> It(World); It && !bNear; ++It) bNear = FVector::Dist2D(It->GetActorLocation(), At) < SwbT(TEXT("placeRadius"), 30000.f);
	bool bOk = false;
	const FVector G = SwbLand(World, At, bOk, 6000.f, 12000.f);
	if (!bNear || !bOk)
	{
		return false;
	}
	for (TActorIterator<AWRCreatureSpawner> It(World); It; ++It)
	{
		if (It->CreatureId == Arena) return true;   // already here (a reload)
	}
	const auto Prop = [World](FName Kind, const FVector& P, float Yaw, float Size, uint8 Variant, float Rise) -> AWRSwampBossProp*
	{
		const FTransform T(FRotator(0.f, Yaw, 0.f), P);
		AWRSwampBossProp* A = World->SpawnActorDeferred<AWRSwampBossProp>(AWRSwampBossProp::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (A)
		{
			A->Kind = Kind;
			A->Size = Size;
			A->Variant = Variant;
			A->Rise = Rise;
			A->FinishSpawning(T);
		}
		return A;
	};
	FVector Start = G;
	if (Arena == TEXT("mother_maw"))
	{
		// The broken boardwalk square round her pool (four sides, the gaps the data names), each side cut into
		// sections she can break and you can mend; the nest's crown on the compost mound.
		const TSharedPtr<FJsonObject>* Square = nullptr;
		const bool bSquare = Row->TryGetObjectField(TEXT("square"), Square);
		const float Half = bSquare ? float(SwbNum(*Square, TEXT("half"), 22.0)) * 100.f : 2200.f;
		const float DeckZ = bSquare ? float(SwbNum(*Square, TEXT("z"), 1.25)) * 100.f : FWRSwampData::Get().SeaZ + 65.f;
		const float Width = WRSwbLocal::SwlDeckWidth();
		const float Want = SwbIT(TEXT("mother_maw"), TEXT("sectionLength"), 580.f);
		TSet<FString> Gaps;
		const TArray<TSharedPtr<FJsonValue>>* GapList = nullptr;
		if (bSquare && (*Square)->TryGetArrayField(TEXT("gaps"), GapList)) for (const TSharedPtr<FJsonValue>& V : *GapList) Gaps.Add(V->AsString().ToLower());
		static const TCHAR* SideNames[] = { TEXT("n"), TEXT("e"), TEXT("s"), TEXT("w") };
		int32 Made = 0;
		for (int32 Side = 0; Side < 4; ++Side)
		{
			// North is -Y on this map; n/s run along X (corners included), e/w along Y between them.
			const bool bAlongX = Side == 0 || Side == 2;
			const float Len = bAlongX ? Half * 2.f + Width : Half * 2.f - Width;
			const int32 Count = FMath::Max(3, FMath::RoundToInt(Len / Want));
			const float Each = Len / Count;
			TSet<int32> Gap;
			if (Gaps.Contains(SideNames[Side]))
			{
				if (Count % 2 == 0) { Gap.Add(Count / 2 - 1); Gap.Add(Count / 2); } else Gap.Add(Count / 2);
			}
			for (int32 i = 0; i < Count; ++i)
			{
				if (Gap.Contains(i)) continue;
				const float U = -Len * 0.5f + Each * (i + 0.5f);
				const FVector Mid = bAlongX ? FVector(At.X + U, At.Y + (Side == 0 ? -Half : Half), DeckZ) : FVector(At.X + (Side == 1 ? Half : -Half), At.Y + U, DeckZ);
				bool bLand = false;
				const FVector Under = SwbLand(World, Mid, bLand, 2000.f, 6000.f);
				const float Rise = FMath::Max(40.f, DeckZ - float(bLand ? Under.Z : DeckZ - 200.f));
				const uint8 Variant = uint8(Side | (Gap.Contains(i + 1) ? 4 : 0) | (Gap.Contains(i - 1) ? 8 : 0));
				if (Prop(TEXT("walk"), Mid, bAlongX ? 0.f : 90.f, Each - 4.f, Variant, Rise)) ++Made;
			}
		}
		bool bNest = false;
		const FVector NestG = SwbLand(World, At, bNest, 3000.f, 6000.f);
		Prop(TEXT("nest"), bNest ? NestG : At, 0.f, SwbIT(TEXT("mother_maw"), TEXT("nestRadius"), 650.f), 0, 0.f);
		// She lies in the water inside the square, off the mound.
		const float NestR = SwbIT(TEXT("mother_maw"), TEXT("nestRadius"), 650.f);
		bool bRest = false;
		const FVector Rest = SwbLand(World, At + FVector(0.f, 1.f, 0.f) * (NestR + (Half - NestR) * 0.5f), bRest, 3000.f, 6000.f);
		if (bRest) Start = Rest;
		UE_LOG(LogWildreach, Log, TEXT("SWAMP Mother Maw's square: %d sections"), Made);
	}
	else if (Arena == TEXT("hex_warden"))
	{
		// The four fetish totems on the Ossuary's shoulders (crow, bone, blood, voice), each facing the mound's top.
		const TArray<FVector> Totems = WRSwamp::Points(Row, TEXT("totems"));
		for (int32 i = 0; i < Totems.Num() && i < 4; ++i)
		{
			bool bT = false;
			const FVector TG = SwbLand(World, Totems[i], bT, 3000.f, 6000.f);
			Prop(TEXT("totem"), bT ? TG : Totems[i], (FVector(At.X, At.Y, 0.f) - FVector(Totems[i].X, Totems[i].Y, 0.f)).Rotation().Yaw, SwbIT(TEXT("hex_warden"), TEXT("totemHeight"), 440.f), uint8(i), 0.f);
		}
	}
	else if (Arena == TEXT("bloat"))
	{
		Prop(TEXT("lair"), G, 0.f, SwbIT(TEXT("bloat"), TEXT("lairSize"), 900.f), 0, 0.f);
	}
	// The boss's spawner (it brings the boss back server.bossRespawnHours after it falls).
	const FTransform Where(FRotator::ZeroRotator, Start + FVector(0.f, 0.f, 50.f));
	if (AWRCreatureSpawner* Spawner = World->SpawnActorDeferred<AWRCreatureSpawner>(AWRCreatureSpawner::StaticClass(), Where, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		Spawner->CreatureId = Arena;
		Spawner->Count = 1;
		Spawner->Radius = 1.f;
		Spawner->RespawnSeconds = WRTune(TEXT("server.bossRespawnHours"), 4.f) * 3600.f;
		Spawner->ActivationRadius = 0.f;
		Spawner->bTraceGround = true;
		Spawner->FinishSpawning(Where);
	}
	UE_LOG(LogWildreach, Log, TEXT("SWAMP arena %s placed at %s"), *Arena.ToString(), *Start.ToCompactString());
	return true;
}

void UWRSwampArenas::OnWaterShocked(UWorld* World, const FVector& Center, float Radius, float Seconds, AActor* Instigator)
{
	FWRSwampBoss::VharraWaterShocked(World, Center, Radius, Seconds, Instigator);
}

void UWRSwampArenas::OnSalted(AWRCharacterBase* Target, AActor* Instigator, const FVector& At)
{
	FWRSwampBoss::Salted(Target, Instigator);
	(void)At;
}
