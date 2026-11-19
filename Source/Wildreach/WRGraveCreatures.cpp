// The Graveyard's creatures (see WRGraveCreatures.h; cooks/biome-graveyard.md 2.12, build-biome-graveyard.md 2.14).

#include "WRGraveCreatures.h"
#include "Wildreach.h"
#include "WRAttackRows.h"
#include "WRAttributeSet.h"
#include "WRBoss.h"
#include "WRBuildLook.h"
#include "WRBuildLookKit.h"
#include "WRCharacter.h"
#include "WRCombatLibrary.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRGameState.h"
#include "WRGraveTypes.h"
#include "WRInventory.h"
#include "WRLand.h"
#include "WRPlayerController.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRSurvival.h"
#include "WRTuning.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"

namespace WRGraveCrLocal
{
	TSharedPtr<FJsonObject> GcRow(FName Id)
	{
		static TMap<FName, TSharedPtr<FJsonObject>> Cache;
		static bool bLoaded = false;
		if (!bLoaded)
		{
			bLoaded = true;
			const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("creatures.json"));
			const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
			if (Root.IsValid() && Root->TryGetArrayField(TEXT("creatures"), List))
			{
				for (const TSharedPtr<FJsonValue>& V : *List)
				{
					const TSharedPtr<FJsonObject> O = V->AsObject();
					FString IdStr;
					if (O.IsValid() && O->TryGetStringField(TEXT("id"), IdStr)) Cache.Add(FName(*IdStr), O);
				}
			}
		}
		const TSharedPtr<FJsonObject>* Found = Cache.Find(Id);
		return Found ? *Found : nullptr;
	}
	TSharedPtr<FJsonObject> GcLook(FName Id)
	{
		const TSharedPtr<FJsonObject> R = GcRow(Id);
		const TSharedPtr<FJsonObject>* L = nullptr;
		return R.IsValid() && R->TryGetObjectField(TEXT("look"), L) ? *L : nullptr;
	}
	FName GcKind(FName Id)
	{
		FString S;
		const TSharedPtr<FJsonObject> L = GcLook(Id);
		return L.IsValid() && L->TryGetStringField(TEXT("kind"), S) ? FName(*S) : Id;
	}
	float GcNum(const TSharedPtr<FJsonObject>& O, const TCHAR* F, float Fallback)
	{
		double V = 0.0;
		return O.IsValid() && O->TryGetNumberField(F, V) ? float(V) : Fallback;
	}
	FLinearColor GcColor(const TSharedPtr<FJsonObject>& O, const TCHAR* F, const FLinearColor& Fallback)
	{
		FString S;
		return O.IsValid() && O->TryGetStringField(F, S) ? FLinearColor(FColor::FromHex(S)) : Fallback;
	}
	FLinearColor GcHex(const TCHAR* S) { return FLinearColor(FColor::FromHex(S)); }
	/** tuning graveyardLife.<Key>. */
	float GcT(const TCHAR* Key, float Fallback) { return FWRTuning::Get().Num(FString::Printf(TEXT("graveyardLife.%s"), Key), Fallback); }
	double GcNow(const UObject* W) { return W && W->GetWorld() ? W->GetWorld()->GetTimeSeconds() : 0.0; }
	bool GcNight(const UObject* W)
	{
		const float H = AWRGameState::HourIn(W);
		return H >= GcT(TEXT("duskHour"), 18.f) || H < GcT(TEXT("dawnHour"), 5.f);
	}
	AWRPlayerController* GcPC(const AActor* A) { const APawn* P = Cast<APawn>(A); return P ? Cast<AWRPlayerController>(P->GetController()) : nullptr; }
	/** Who's chilled by Wraithmoth dust, and until when (stamina regen x0.6). */
	TMap<TWeakObjectPtr<const AActor>, double>& Chilled() { static TMap<TWeakObjectPtr<const AActor>, double> M; return M; }
	enum EGcGroup : int32 { GStatic = 0, GWhip = 1, GEyes = 2, GProboscis = 3, GMane = 4, GThreads = 5 };
}
using namespace WRGraveCrLocal;

// ================================================================================================ the body

TArray<TWeakObjectPtr<UWRGraveBody>> UWRGraveBody::Live;

UWRGraveBody::UWRGraveBody()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UWRGraveBody::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UWRGraveBody, Flags);
	DOREPLIFETIME(UWRGraveBody, Holding);
	DOREPLIFETIME(UWRGraveBody, Found);
}

UWRGraveBody* UWRGraveBody::Get(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UWRGraveBody>() : nullptr;
}

bool UWRGraveBody::IsGraveKind(FName CreatureId)
{
	const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(CreatureId);
	return Def && Def->Biome == TEXT("graveyard") && Def->Archetype != TEXT("boss");
}

bool UWRGraveBody::IsRisen(FName CreatureId)
{
	bool b = false;
	const TSharedPtr<FJsonObject> R = GcRow(CreatureId);
	return R.IsValid() && R->TryGetBoolField(TEXT("risen"), b) && b;
}

void UWRGraveBody::Attach(AWRCreature* Creature)
{
	if (!Creature || !Creature->HasAuthority() || !IsGraveKind(Creature->CreatureId) || Get(Creature) || Creature->IsA<AWRBoss>()) return;
	UWRGraveBody* Body = NewObject<UWRGraveBody>(Creature, TEXT("GraveBody"));
	Body->RegisterComponent();
	Creature->AddInstanceComponent(Body);
}

void UWRGraveBody::AttachRaised(AWRCreature* Creature, FName Variant)
{
	if (!Creature || !Creature->HasAuthority()) return;
	UWRGraveBody* Body = Get(Creature);
	if (!Body)
	{
		Body = NewObject<UWRGraveBody>(Creature, TEXT("GraveBody"));
		Body->RegisterComponent();
		Creature->AddInstanceComponent(Body);
	}
	Body->Set(WRGraveFlag::Raised, true);
	Body->Set(WRGraveFlag::Ash, Variant == TEXT("ash"));
	Body->Set(WRGraveFlag::Ice, Variant == TEXT("ice"));
}

void UWRGraveBody::Set(uint16 Flag, bool bOn)
{
	const uint16 Was = Flags;
	Flags = bOn ? uint16(Flags | Flag) : uint16(Flags & ~Flag);
	if (Was != Flags && GetNetMode() != NM_DedicatedServer) OnRep_Flags();
}

void UWRGraveBody::OnRep_Flags()
{
	if (LookFlags != (Flags & (WRGraveFlag::Raised | WRGraveFlag::Riftborn | WRGraveFlag::Ash | WRGraveFlag::Ice))) LookTries = 0;   // raised or Riftborn: rebuild
	ApplyBuriedLook();
}

void UWRGraveBody::RollRiftborn(AWRCreature* C)
{
	// The rifts in the south (the wasteland's edge past the neck) send some of the Graveyard's own back changed: one tier
	// up in health and their every blow carries curse. Only the three the cook names, only south of fromY.
	static const TSet<FName> Kinds = { FName(TEXT("gravehound")), FName(TEXT("wraithmoth")), FName(TEXT("tombback")) };
	if (!Kinds.Contains(C->CreatureId) || C->GetActorLocation().Y < GcT(TEXT("riftborn.fromY"), 445000.f)) return;
	if (FMath::FRand() * 100.f >= GcT(TEXT("riftborn.share"), 15.f)) return;
	Set(WRGraveFlag::Riftborn, true);
	if (UWRAttributeSet* A = C->GetAttributes())
	{
		if (UAbilitySystemComponent* ASC = C->GetAbilitySystemComponent())
		{
			ASC->SetNumericAttributeBase(UWRAttributeSet::GetMaxHealthAttribute(), A->GetMaxHealth() * GcT(TEXT("riftborn.healthMult"), 1.25f));
			ASC->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), A->GetMaxHealth());
		}
	}
}

void UWRGraveBody::BeginPlay()
{
	Super::BeginPlay();
	Live.RemoveAll([](const TWeakObjectPtr<UWRGraveBody>& B) { return !B.IsValid(); });
	Live.Add(this);
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	if (!C || !C->HasAuthority() || C->IsTamed()) return;
	RollRiftborn(C);
	// After its spawner sets it on the ground: a Gravehound lies down in the grave soil by day; a Cryptstalker in the
	// catacombs goes up its room's ceiling.
	TWeakObjectPtr<AWRCreature> WeakC(C);
	TWeakObjectPtr<UWRGraveBody> WeakB(this);
	FTimerHandle H;
	GetWorld()->GetTimerManager().SetTimer(H, [WeakC, WeakB]()
	{
		AWRCreature* Cr = WeakC.Get();
		UWRGraveBody* B = WeakB.Get();
		if (!Cr || !B || !Cr->IsAlive() || Cr->IsTamed() || B->Has(WRGraveFlag::Raised)) return;
		if (Cr->CreatureId == TEXT("gravehound") && !GcNight(Cr)) FWRGraveAI::Bury(Cr, B);
		if (Cr->CreatureId == TEXT("cryptstalker"))
		{
			if (const FWRGraveData::FRoom* R = WRGrave::RoomAt(Cr->GetActorLocation()); R && !R->bSpiral && R->H > 350.f)
			{
				B->CeilingAt = FVector(Cr->GetActorLocation().X, Cr->GetActorLocation().Y, R->At.Z + R->H - Cr->GetSimpleCollisionHalfHeight() - 15.f);
				Cr->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
				Cr->SetActorLocation(B->CeilingAt, false, nullptr, ETeleportType::TeleportPhysics);
				B->Set(WRGraveFlag::Ceiling, true);
			}
		}
	}, 0.6f, false);
}

void UWRGraveBody::EndPlay(const EEndPlayReason::Type Reason)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		if (AWRCreature* C = Cast<AWRCreature>(GetOwner()); C && HeldBody.IsValid()) FWRGraveAI::Release(C, this, false);
	}
	ClearLook();
	Live.RemoveAll([this](const TWeakObjectPtr<UWRGraveBody>& B) { return !B.IsValid() || B.Get() == this; });
	Super::EndPlay(Reason);
}

USkeletalMeshComponent* UWRGraveBody::Skel() const
{
	const AWRCreature* C = Cast<AWRCreature>(GetOwner());
	return C ? C->GetCreatureSkel() : nullptr;
}

void UWRGraveBody::MulticastMoment_Implementation(FName Effect, FName Bone, float Scale, FName Sound)
{
	const AWRCreature* C = Cast<AWRCreature>(GetOwner());
	if (!C || GetNetMode() == NM_DedicatedServer) return;
	const FName B = C->FindBone(Bone);
	const FVector At = B.IsNone() || !Skel() ? C->GetActorLocation() : Skel()->GetBoneLocation(B);
	if (!Effect.IsNone()) WRFx::SpawnAt(C, Effect, At, C->GetActorRotation(), Scale);
	if (!Sound.IsNone()) WRSound::PlayAt(C, Sound, At);
}

void UWRGraveBody::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	Clock += DeltaTime;
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	if (!C) return;
	if (C->HasAuthority()) FWRGraveAI::TickBody(C, this, DeltaTime);
	if (GetNetMode() != NM_DedicatedServer) TickLook(DeltaTime);
}

// ================================================================================================ the look

USceneComponent* UWRGraveBody::AddGroup(FName Bone, const FVector& WorldAt, const FRotator& WorldRot, const TArray<FWRLookBlock>& Blocks, int32 Kind)
{
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	USkeletalMeshComponent* Body = Skel();
	if (!C || !Body || Blocks.Num() == 0) return nullptr;
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
	G.Phase = FMath::FRand() * 6.28f;
	Groups.Add(G);
	return Frame;
}

void UWRGraveBody::BoneSeg(FName A, FName B, float Radius, const FLinearColor& Col, int32 Kind)
{
	USkeletalMeshComponent* Body = Skel();
	if (!Body || A.IsNone() || B.IsNone()) return;
	const FVector Pa = Body->GetBoneLocation(A), Pb = Body->GetBoneLocation(B);
	const float Len = float(FVector::Dist(Pa, Pb));
	if (Len < 2.f) return;
	const FRotator R = FRotationMatrix::MakeFromX(Pb - Pa).Rotator();
	TArray<FWRLookBlock> Out;
	WRLookKit::FKit Kit(FTransform::Identity, Out);
	const WRLookKit::FSurf S = WRLookKit::FSurf::Stone(Col, 0.75f);
	// A long bone: the shaft along +X (a cylinder turned onto X), its two ends knobbed.
	Kit.Add(S, FVector(Radius * 2.f, Radius * 2.f, Len * 0.86f), FVector(Len * 0.5f, 0.f, 0.f), FRotator(90.f, 0.f, 0.f), EWRLookMesh::Cylinder);
	Kit.Add(S, FVector(Radius * 3.f, Radius * 2.4f, Radius * 2.4f), FVector(Len * 0.06f, 0.f, 0.f));
	Kit.Add(S, FVector(Radius * 2.8f, Radius * 2.6f, Radius * 2.2f), FVector(Len * 0.94f, 0.f, 0.f));
	// Rotate the blocks into the frame (the kit placed them along local X; the frame looks from A to B).
	AddGroup(A, Pa, R, Out, Kind);
}

void UWRGraveBody::ClearLook()
{
	if (RaisedGrey)
	{
		if (USkeletalMeshComponent* Body = Skel(); Body && Body->GetOverlayMaterial() == RaisedGrey) Body->SetOverlayMaterial(nullptr);
		RaisedGrey = nullptr;
	}
	for (USceneComponent* Comp : LookComps) if (Comp) Comp->DestroyComponent();
	LookComps.Reset();
	Groups.Reset();
	if (Glow) { Glow->DestroyComponent(); Glow = nullptr; }
	if (Mound) { Mound->DestroyComponent(); Mound = nullptr; }
	LookFor = NAME_None;
}

void UWRGraveBody::BuildLook()
{
	using namespace WRLookKit;
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	const FWRCreatureDef* Def = C ? C->GetDef() : nullptr;
	USkeletalMeshComponent* Body = Skel();
	if (!Def || !Body || !Body->GetSkeletalMeshAsset()) return;
	const TSharedPtr<FJsonObject> Look = GcLook(C->CreatureId);
	// A body drawn wholly by parts on the rig's own bones (the Risen: hideMesh). The rig's mesh hides but stays its art
	// (its bones pose every frame, bone lookups work: AWRCreature::SetRigUnseen), and the parts wait for a posed skeleton.
	// Hidden first and built on bones that were never posed, every bone stood at the rig's feet: the limbs weren't drawn,
	// the rest lay in a heap on the ground (risen_hound, risen_bearer) or at the world's origin (risen_mourner).
	bool bHideMesh = false;
	if (Look.IsValid()) Look->TryGetBoolField(TEXT("hideMesh"), bHideMesh);
	if (bHideMesh)
	{
		C->SetRigUnseen(true);
		bMeshHidden = true;
		// Posed: the head's bone has its place in the pose, above the rig's feet (never posed: no pose, or all at 0).
		const FName HeadB = C->FindBone(TEXT("head"));
		const int32 HeadI = HeadB.IsNone() ? INDEX_NONE : Body->GetBoneIndex(HeadB);
		const TArray<FTransform>& Pose = Body->GetComponentSpaceTransforms();
		if (HeadI != INDEX_NONE && (!Pose.IsValidIndex(HeadI) || Pose[HeadI].GetLocation().SizeSquared() < FMath::Square(5.f))) return;   // next frame
	}
	ClearLook();
	LookFor = C->CreatureId;
	LookFlags = Flags & (WRGraveFlag::Raised | WRGraveFlag::Riftborn | WRGraveFlag::Ash | WRGraveFlag::Ice);
	const FName K = GcKind(C->CreatureId);
	const bool bRaised = Has(WRGraveFlag::Raised);
	const bool bRift = Has(WRGraveFlag::Riftborn);

	// The palette over the rig's own texture (M_CreatureTint); a raised body's hide goes grey (GY 2.17).
	TintMats.Reset();
	const TArray<TSharedPtr<FJsonValue>>* Tints = nullptr;
	UMaterialInterface* Master = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Creatures/M_CreatureTint.M_CreatureTint"));
	if (Look.IsValid() && Master && Look->TryGetArrayField(TEXT("tint"), Tints) && Tints->Num())
	{
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
				if (O->TryGetStringField(TEXT("match"), Match) && !Match.IsEmpty()) { if (SlotName.Contains(Match.ToLower())) { Pick = O; break; } }
				else if (!Pick.IsValid()) Pick = O;
			}
			if (!Pick.IsValid()) Pick = (*Tints)[0]->AsObject();
			UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Master, C);
			FString Path;
			if (UTexture* T = Pick->TryGetStringField(TEXT("tex"), Path) ? LoadObject<UTexture>(nullptr, *Path) : nullptr) M->SetTextureParameterValue(TEXT("BaseColorTex"), T);
			if (UTexture* T = LoadObject<UTexture>(nullptr, TEXT("/Engine/EngineMaterials/DefaultNormal.DefaultNormal"))) M->SetTextureParameterValue(TEXT("NormalTex"), T);
			if (UTexture* T = LoadObject<UTexture>(nullptr, TEXT("/Engine/EngineResources/Black.Black"))) M->SetTextureParameterValue(TEXT("GlowTex"), T);
			M->SetScalarParameterValue(TEXT("Desaturation"), bRaised ? 0.85f : bRift ? FMath::Max(GcNum(Pick, TEXT("desat"), 1.f), GcT(TEXT("riftborn.desat"), 0.7f)) : GcNum(Pick, TEXT("desat"), 1.f));
			M->SetScalarParameterValue(TEXT("Brightness"), GcNum(Pick, TEXT("bright"), 1.f) * (bRaised ? 0.85f : bRift ? GcT(TEXT("riftborn.bright"), 0.8f) : 1.f));
			M->SetScalarParameterValue(TEXT("TwoTone"), GcNum(Pick, TEXT("twoTone"), 0.f));
			M->SetScalarParameterValue(TEXT("LightFrom"), GcNum(Pick, TEXT("lightFrom"), 0.f));
			M->SetScalarParameterValue(TEXT("Roughness"), GcNum(Pick, TEXT("rough"), 0.75f));
			M->SetScalarParameterValue(TEXT("GlowStrength"), GcNum(Pick, TEXT("glowStrength"), 0.f));
			const FLinearColor Tint = GcColor(Pick, TEXT("tint"), FLinearColor::White);
			M->SetVectorParameterValue(TEXT("Tint"), bRaised ? FLinearColor(0.32f, 0.32f, 0.31f) : Tint);
			M->SetVectorParameterValue(TEXT("TintLight"), bRaised ? FLinearColor(0.55f, 0.55f, 0.53f) : GcColor(Pick, TEXT("light"), Tint));
			M->SetVectorParameterValue(TEXT("GlowColor"), GcColor(Pick, TEXT("glow"), GcHex(TEXT("#9FE3C0"))));
			Body->SetMaterial(Slot, M);
			TintMats.Add(M);
		}
	}
	// The rig's proportions (a Cryptstalker flat and wide, a Tombback longer).
	{
		const float Flatten = GcNum(Look, TEXT("flatten"), 1.f), Stretch = GcNum(Look, TEXT("stretch"), 1.f);
		if (Flatten != 1.f || Stretch != 1.f)
		{
			const FVector S0 = Body->GetRelativeScale3D();
			Body->SetRelativeScale3D(FVector(S0.X * Stretch, S0.Y * Stretch, S0.Z * Flatten));
		}
	}

	const auto BoneAt = [&](const TCHAR* Hint, FVector& Out) -> FName
	{
		const FName B = C->FindBone(FName(Hint));
		if (!B.IsNone()) Out = Body->GetBoneLocation(B);
		return B;
	};
	const auto Blocks = [&](TFunctionRef<void(FKit&)> Fn) { TArray<FWRLookBlock> Out; FKit Kit(FTransform::Identity, Out); Fn(Kit); return Out; };
	const FRotator Yaw(0.f, C->GetActorRotation().Yaw, 0.f);
	const FVector Fwd = Yaw.Vector(), Right = FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), Up = FVector::UpVector;
	const float Rad = Def->Radius, HH = Def->HalfHeight;
	const FLinearColor BoneC = FLinearColor(FColor(217, 207, 184)) * 0.66f;
	const FSurf BoneS = FSurf::Stone(BoneC, 0.72f);
	const FSurf EyeLit = FSurf::Lit(GcHex(TEXT("#9FE3C0")) * 0.6f, 6.f);
	const FSurf Thread = FSurf::Lit(GcHex(TEXT("#9FE3C0")) * 0.6f, 3.f);
	FVector Head = C->GetActorLocation() + Fwd * Rad;
	const FName HeadBone = BoneAt(TEXT("head"), Head);
	FVector Spine = C->GetActorLocation();
	const FName SpineBone = BoneAt(TEXT("spine2"), Spine);

	// A raised body (Necromancy): grave-light threads in its wounds, pale-green eyes (GY 2.17).
	if (bRaised)
	{
		// A body with no palette row of its own (a Fire or Frost creature, a pack rig) keeps its living hide under the
		// tint pass: a bone-grey wash over it instead (the tame-morph overlay, pale and darkened; tuning necromancy.raisedLook),
		// so every raised minion reads as dead (cooks/cookpass-creatures.md, LATER-COOKS build-biome-graveyard).
		if (TintMats.Num() == 0 && !Body->GetOverlayMaterial())
		{
			if (UMaterialInterface* Morph = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Creatures/M_TameMorph.M_TameMorph")))
			{
				RaisedGrey = UMaterialInstanceDynamic::Create(Morph, this);
				const FWRTuning& T = FWRTuning::Get();
				RaisedGrey->SetVectorParameterValue(TEXT("Tint"), FLinearColor(FColor(uint8(T.Num(TEXT("necromancy.raisedLook.r"), 184.f)), uint8(T.Num(TEXT("necromancy.raisedLook.g"), 178.f)), uint8(T.Num(TEXT("necromancy.raisedLook.b"), 162.f)))));
				RaisedGrey->SetScalarParameterValue(TEXT("Pale"), T.Num(TEXT("necromancy.raisedLook.pale"), 0.7f));
				RaisedGrey->SetScalarParameterValue(TEXT("Dark"), T.Num(TEXT("necromancy.raisedLook.dark"), 0.25f));
				RaisedGrey->SetScalarParameterValue(TEXT("Bands"), 0.f);
				RaisedGrey->SetScalarParameterValue(TEXT("GlowLine"), 0.f);
				Body->SetOverlayMaterial(RaisedGrey);
			}
		}
		// An ash wraith's threads burn ember-orange with sparks of ash on its back; an ice minion's are pale blue under a
		// crust of rime.
		const bool bAsh = Has(WRGraveFlag::Ash), bIce = Has(WRGraveFlag::Ice);
		const FSurf T = bAsh ? FSurf::Lit(GcHex(TEXT("#FF8A3A")) * 0.6f, 4.f) : bIce ? FSurf::Lit(GcHex(TEXT("#A8D8F0")) * 0.6f, 3.f) : Thread;
		AddGroup(SpineBone, Spine, Yaw, Blocks([&](FKit& Kit)
		{
			for (int32 t = 0; t < 7; ++t) Kit.Add(T, FVector(2.f, 2.f, Rad * (0.3f + 0.1f * (t % 3))), FVector((t - 3) * Rad * 0.15f, (t % 2 ? 1.f : -1.f) * Rad * 0.3f, HH * 0.2f), FRotator(20.f * t, 30.f * t, 60.f), EWRLookMesh::Cylinder);
			if (bAsh) for (int32 e = 0; e < 10; ++e) Kit.Add(FSurf::Lit(GcHex(TEXT("#FF6A20")) * 0.6f, 6.f), FVector(Rad * 0.04f), FVector((e - 4.5f) * Rad * 0.12f, (e % 3 - 1) * Rad * 0.2f, HH * 0.5f + (e % 2) * Rad * 0.05f));
			if (bIce) for (int32 r = 0; r < 8; ++r) Kit.Add(FSurf::Glass(GcHex(TEXT("#DDEFFA")) * 0.6f, 0.4f), FVector(Rad * 0.18f, Rad * 0.14f, Rad * 0.05f), FVector((r - 3.5f) * Rad * 0.14f, (r % 2 ? 1.f : -1.f) * Rad * 0.18f, HH * 0.48f), FRotator(0.f, 40.f * r, 12.f));
		}), GThreads);
		// The eyes: pale-green points (pale blue on an ice minion) where the head is.
		AddGroup(HeadBone, Head, Yaw, Blocks([&](FKit& Kit)
		{
			const FSurf Eye = FSurf::Lit(bIce ? GcHex(TEXT("#A8D8F0")) * 0.6f : GcHex(TEXT("#9FE3C0")) * 0.6f, 7.f);
			for (const float S : { -1.f, 1.f }) Kit.Add(Eye, FVector(Rad * 0.05f), FVector(Rad * 0.32f, S * Rad * 0.12f, Rad * 0.1f));
		}), GEyes);
	}
	// A Riftborn: voidglass shards grown out along its spine and black-violet glints where its eyes catch the light.
	if (bRift)
	{
		const FSurf Voidglass = FSurf::Glass(FLinearColor(FColor(26, 12, 40)), 0.6f);
		AddGroup(SpineBone, Spine, Yaw, Blocks([&](FKit& Kit)
		{
			for (int32 s = 0; s < 6; ++s)
			{
				const float X = (s - 2.5f) * Rad * 0.28f;
				Kit.Add(Voidglass, FVector(Rad * 0.08f, Rad * 0.08f, Rad * (0.22f + 0.08f * (s % 3))), FVector(X, (s % 2 ? 1.f : -1.f) * Rad * 0.06f, HH * 0.45f), FRotator(-15.f + 8.f * s, 0.f, (s % 2 ? 14.f : -14.f)), EWRLookMesh::Cone);
			}
		}), GStatic);
		AddGroup(HeadBone, Head, Yaw, Blocks([&](FKit& Kit)
		{
			for (int32 e = -1; e <= 1; e += 2) Kit.Add(FSurf::Lit(FLinearColor(FColor(70, 20, 110)), 4.f), FVector(Rad * 0.07f), FVector(Rad * 0.2f, e * Rad * 0.12f, Rad * 0.06f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		}), GEyes);
	}

	if (K == TEXT("gravehound"))
	{
		// The bone-pale skull face (the bare face shows the skull like a horse skull), the knuckled spine ridge, the ribs
		// through thin dark skin, soul-green eyeshine.
		const FVector F = Fwd;
		AddGroup(HeadBone, Head, Yaw, Blocks([&](FKit& Kit)
		{
			Kit.Add(BoneS, FVector(Rad * 0.62f, Rad * 0.42f, Rad * 0.32f), FVector(Rad * 0.18f, 0.f, Rad * 0.08f));
			Kit.Add(BoneS, FVector(Rad * 0.5f, Rad * 0.26f, Rad * 0.18f), FVector(Rad * 0.5f, 0.f, -Rad * 0.02f));
			for (const float S : { -1.f, 1.f })
			{
				Kit.Add(FSurf::Stone(FLinearColor(0.02f, 0.02f, 0.02f), 0.9f), FVector(Rad * 0.12f, Rad * 0.1f, Rad * 0.1f), FVector(Rad * 0.3f, S * Rad * 0.14f, Rad * 0.15f));
				Kit.Add(EyeLit, FVector(Rad * 0.05f, Rad * 0.05f, Rad * 0.05f), FVector(Rad * 0.34f, S * Rad * 0.14f, Rad * 0.15f));
				Kit.Add(BoneS, FVector(Rad * 0.35f, Rad * 0.06f, Rad * 0.12f), FVector(Rad * 0.25f, S * Rad * 0.2f, 0.f));   // cheekbones
			}
		}), GEyes);
		for (const TCHAR* Bn : { TEXT("Spine"), TEXT("Spine1"), TEXT("Spine2"), TEXT("Spine3"), TEXT("Neck") })
		{
			FVector P;
			const FName B = BoneAt(Bn, P);
			if (B.IsNone()) continue;
			AddGroup(B, P, Yaw, Blocks([&](FKit& Kit)
			{
				for (int32 k = 0; k < 3; ++k) Kit.Add(BoneS, FVector(Rad * 0.1f, Rad * 0.08f, Rad * 0.14f), FVector((k - 1) * Rad * 0.12f, 0.f, Rad * 0.32f), FRotator(0.f, 0.f, 0.f), EWRLookMesh::Cone);
			}), GStatic);
		}
		AddGroup(SpineBone, Spine, Yaw, Blocks([&](FKit& Kit)
		{
			for (int32 r = 0; r < 6; ++r) for (const float S : { -1.f, 1.f }) Kit.Add(FSurf::Stone(BoneC * 0.75f, 0.8f), FVector(Rad * 0.06f, Rad * 0.03f, Rad * 0.45f), FVector((r - 2.5f) * Rad * 0.12f, S * Rad * 0.36f, -Rad * 0.05f), FRotator(0.f, 0.f, S * 20.f));
		}), GStatic);
	}
	else if (K == TEXT("cryptstalker"))
	{
		// Two whip legs 5 m long (14 segments, swaying, fanned out on the sweep) and the spined raptorial palps folded at
		// the front; bone-white spines; eyeless.
		for (const float S : { -1.f, 1.f })
		{
			const FRotator Out = Yaw + FRotator(5.f, S * 35.f, 0.f);
			AddGroup(HeadBone, Head + Right * S * Rad * 0.1f, Out, Blocks([&](FKit& Kit)
			{
				float X = 0.f;
				const float Seg = GcT(TEXT("whipLength"), 500.f) / 14.f;
				for (int32 s = 0; s < 14; ++s)
				{
					const float Rr = FMath::Lerp(3.2f, 0.8f, s / 13.f);
					Kit.Add(FSurf::Stone(GcHex(TEXT("#5A5248")) * 0.6f, 0.7f), FVector(Rr * 2.f, Rr * 2.f, Seg * 1.05f), FVector(X + Seg * 0.5f, 0.f, -s * s * 0.4f), FRotator(90.f + s * 1.2f, 0.f, 0.f), EWRLookMesh::Cylinder);
					X += Seg;
				}
			}), GWhip);
			AddGroup(HeadBone, Head + Right * S * Rad * 0.18f, Yaw + FRotator(-10.f, S * 12.f, 0.f), Blocks([&](FKit& Kit)
			{
				// The palp: a thick upper segment, a folded lower one, spines along both, a hooked tip.
				Kit.Add(FSurf::Stone(GcHex(TEXT("#5A5248")) * 0.55f, 0.6f), FVector(Rad * 0.45f, 14.f, 14.f), FVector(Rad * 0.2f, 0.f, 0.f));
				Kit.Add(FSurf::Stone(GcHex(TEXT("#5A5248")) * 0.55f, 0.6f), FVector(Rad * 0.4f, 11.f, 11.f), FVector(Rad * 0.25f, -S * 14.f, -8.f), FRotator(0.f, S * 150.f, 0.f));
				for (int32 sp = 0; sp < 6; ++sp) Kit.Spike(BoneS, FVector(Rad * (0.05f + 0.07f * sp), 0.f, 6.f), FVector(0.f, -S * 0.4f, 1.f), 4.f, 16.f + 3.f * (sp % 2));
				Kit.Spike(BoneS, FVector(Rad * 0.45f, -S * 6.f, 0.f), FVector(1.f, -S * 0.5f, -0.3f), 6.f, 18.f);
			}), GStatic);
		}
	}
	else if (K == TEXT("wraithmoth"))
	{
		// The death's-head mark on the thorax (a bone-pale skull pattern on black), the bruise-violet underwings, the coiled
		// proboscis, a furred thorax; its wings are the rig's own (ash-brown with bark mottle by the tint).
		AddGroup(SpineBone.IsNone() ? BoneAt(TEXT("body"), Spine) : SpineBone, Spine, Yaw, Blocks([&](FKit& Kit)
		{
			Kit.Add(FSurf::Cloth(GcHex(TEXT("#1A1714")) * 0.5f), FVector(Rad * 0.55f, Rad * 0.32f, Rad * 0.3f), FVector(Rad * 0.05f, 0.f, 0.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			Kit.Add(BoneS, FVector(Rad * 0.26f, Rad * 0.22f, 2.f), FVector(Rad * 0.08f, 0.f, Rad * 0.16f));
			for (const float S : { -1.f, 1.f }) Kit.Add(FSurf::Stone(FLinearColor(0.01f, 0.01f, 0.01f), 0.9f), FVector(Rad * 0.06f, Rad * 0.06f, 2.5f), FVector(Rad * 0.12f, S * Rad * 0.05f, Rad * 0.165f));
			Kit.Add(FSurf::Stone(FLinearColor(0.01f, 0.01f, 0.01f), 0.9f), FVector(Rad * 0.03f, Rad * 0.08f, 2.5f), FVector(Rad * 0.0f, 0.f, Rad * 0.165f));
		}), GStatic);
		for (const TCHAR* Wb : { TEXT("wing_L"), TEXT("wing_R") })
		{
			FVector P;
			const FName B = BoneAt(Wb, P);
			if (B.IsNone()) continue;
			AddGroup(B, P, Yaw, Blocks([&](FKit& Kit)
			{
				Kit.Add(FSurf::Cloth(GcHex(TEXT("#4B4358")) * 0.55f), FVector(Rad * 0.5f, Rad * 0.6f, 1.5f), FVector(-Rad * 0.1f, 0.f, -2.f));
			}), GStatic);
		}
		AddGroup(HeadBone, Head, Yaw, Blocks([&](FKit& Kit)
		{
			for (int32 c = 0; c < 6; ++c) { const float A = c * 1.05f; Kit.Add(FSurf::Stone(GcHex(TEXT("#3A332C")) * 0.6f, 0.6f), FVector(2.f, 2.f, 6.f), FVector(Rad * 0.12f + FMath::Cos(A) * 4.f, 0.f, -Rad * 0.08f + FMath::Sin(A) * 4.f), FRotator(0.f, 0.f, FMath::RadiansToDegrees(A)), EWRLookMesh::Cylinder); }
			for (const float S : { -1.f, 1.f }) Kit.Add(FSurf::Lit(GcHex(TEXT("#C8B8A0")) * 0.5f, 1.2f), FVector(Rad * 0.06f, Rad * 0.06f, Rad * 0.06f), FVector(Rad * 0.06f, S * Rad * 0.08f, Rad * 0.02f));
		}), GProboscis);
	}
	else if (K == TEXT("palestrider"))
	{
		// A hairless bone-pale face over the skull (the Mari Lwyd) with dark hollow nostrils and pale eyes, and the long
		// ragged black mane and tail like torn ribbons.
		AddGroup(HeadBone, Head, Yaw, Blocks([&](FKit& Kit)
		{
			Kit.Add(FSurf::Stone(GcHex(TEXT("#E6E1D3")) * 0.66f, 0.65f), FVector(Rad * 0.65f, Rad * 0.3f, Rad * 0.3f), FVector(Rad * 0.25f, 0.f, 0.f));
			Kit.Add(FSurf::Stone(GcHex(TEXT("#E6E1D3")) * 0.62f, 0.65f), FVector(Rad * 0.4f, Rad * 0.22f, Rad * 0.2f), FVector(Rad * 0.6f, 0.f, -Rad * 0.06f));
			for (const float S : { -1.f, 1.f })
			{
				Kit.Add(FSurf::Stone(FLinearColor(0.01f, 0.01f, 0.01f), 0.9f), FVector(Rad * 0.06f, Rad * 0.04f, Rad * 0.06f), FVector(Rad * 0.79f, S * Rad * 0.05f, -Rad * 0.05f));
				Kit.Add(FSurf::Lit(GcHex(TEXT("#C8C8C8")) * 0.5f, 0.6f), FVector(Rad * 0.06f, Rad * 0.03f, Rad * 0.05f), FVector(Rad * 0.18f, S * Rad * 0.14f, Rad * 0.09f));
			}
		}), GStatic);
		for (const TCHAR* Nb : { TEXT("neck"), TEXT("spine3"), TEXT("tail1"), TEXT("tail2") })
		{
			FVector P;
			const FName B = BoneAt(Nb, P);
			if (B.IsNone()) continue;
			const bool bTail = FString(Nb).StartsWith(TEXT("tail"));
			AddGroup(B, P, Yaw, Blocks([&](FKit& Kit)
			{
				for (int32 r = 0; r < (bTail ? 7 : 11); ++r) Kit.Add(FSurf::Cloth(GcHex(TEXT("#141414")) * 0.5f), FVector(4.f + 3.f * (r % 3), 3.f, Rad * (bTail ? 0.6f : 0.35f) + 10.f * (r % 4)), FVector((r - 5) * 5.f, (r % 2 ? 1.f : -1.f) * 6.f, bTail ? -Rad * 0.3f : Rad * 0.05f), FRotator(0.f, 0.f, (r % 2 ? 1.f : -1.f) * 12.f));
			}), GMane);
		}
	}
	else if (K == TEXT("tombback"))
	{
		// A grave slab cemented to its back with its own wax (grey stone, moss, a worn name), the rust-orange bands across
		// its black wing cases, a short horn.
		AddGroup(SpineBone, Spine, Yaw, Blocks([&](FKit& Kit)
		{
			const FSurf Slab = FSurf::Stone(GcHex(TEXT("#8A8580")) * 0.55f, 0.9f);
			Kit.Add(Slab, FVector(Rad * 0.18f, Rad * 0.75f, Rad * 0.95f), FVector(-Rad * 0.1f, 0.f, HH * 0.9f), FRotator(-24.f, 0.f, 0.f));
			Kit.Add(Slab, FVector(Rad * 0.18f, Rad * 0.75f, Rad * 0.75f), FVector(-Rad * 0.1f + Rad * 0.38f * 0.4f, 0.f, HH * 0.9f + Rad * 0.38f), FRotator(-24.f, 0.f, 90.f), EWRLookMesh::Cylinder);
			Kit.Add(FSurf::Cloth(GcHex(TEXT("#4F5B3A")) * 0.5f), FVector(Rad * 0.19f, Rad * 0.3f, Rad * 0.25f), FVector(-Rad * 0.12f, -Rad * 0.18f, HH * 0.75f), FRotator(-24.f, 0.f, 0.f));
			Kit.Add(FSurf::Stone(GcHex(TEXT("#3A3835")) * 0.6f, 0.9f), FVector(2.f, Rad * 0.45f, Rad * 0.18f), FVector(-Rad * 0.0f, 0.f, HH * 1.1f), FRotator(-24.f, 0.f, 0.f));
			Kit.Add(FSurf::Glaze(GcHex(TEXT("#E8DDB0")) * 0.6f, 0.5f), FVector(Rad * 0.5f, Rad * 0.7f, 8.f), FVector(-Rad * 0.05f, 0.f, HH * 0.42f));
			for (const float X : { -0.35f, 0.15f }) Kit.Add(FSurf::Glaze(GcHex(TEXT("#B5562A")) * 0.55f, 0.35f), FVector(Rad * 0.16f, Rad * 0.86f, 6.f), FVector(Rad * X, 0.f, HH * 0.36f));
		}), GStatic);
		AddGroup(HeadBone, Head, Yaw, Blocks([&](FKit& Kit) { Kit.Spike(FSurf::Glaze(FLinearColor(0.02f, 0.02f, 0.02f), 0.3f), FVector(Rad * 0.1f, 0.f, Rad * 0.05f), FVector(1.f, 0.f, 0.6f), Rad * 0.12f, Rad * 0.3f); }), GStatic);
	}
	else if (K == TEXT("gloamcrow"))
	{
		// The ash-grey hood over a black crow, the jackdaw's silver-white eyes.
		AddGroup(HeadBone, Head, Yaw, Blocks([&](FKit& Kit)
		{
			for (const float S : { -1.f, 1.f }) Kit.Add(FSurf::Lit(FLinearColor(0.85f, 0.85f, 0.85f), 1.f), FVector(1.6f, 1.6f, 1.6f), FVector(Rad * 0.2f, S * Rad * 0.12f, Rad * 0.08f));
		}), GEyes);
		FVector Nk;
		const FName NeckB = BoneAt(TEXT("Neck"), Nk);
		AddGroup(NeckB, Nk, Yaw, Blocks([&](FKit& Kit) { Kit.Add(FSurf::Cloth(GcHex(TEXT("#9A958C")) * 0.5f), FVector(Rad * 0.55f, Rad * 0.5f, Rad * 0.45f), FVector::ZeroVector, FRotator::ZeroRotator, EWRLookMesh::Cylinder); }), GStatic);
	}
	else if (K == TEXT("mournhare"))
	{
		// Long upright ears with black tips, a white belly.
		AddGroup(HeadBone, Head, Yaw, Blocks([&](FKit& Kit)
		{
			for (const float S : { -1.f, 1.f })
			{
				Kit.Add(FSurf::Cloth(GcHex(TEXT("#8E8A84")) * 0.5f), FVector(4.f, 7.f, 26.f), FVector(-2.f, S * 5.f, 16.f), FRotator(0.f, 0.f, S * 8.f));
				Kit.Add(FSurf::Cloth(FLinearColor(0.01f, 0.01f, 0.01f)), FVector(4.5f, 7.5f, 6.f), FVector(-2.f, S * 6.8f, 27.f), FRotator(0.f, 0.f, S * 8.f));
			}
		}), GStatic);
		AddGroup(SpineBone, Spine, Yaw, Blocks([&](FKit& Kit) { Kit.Add(FSurf::Cloth(FLinearColor(0.6f, 0.6f, 0.58f)), FVector(Rad * 0.6f, Rad * 0.5f, 4.f), FVector(0.f, 0.f, -HH * 0.45f)); }), GStatic);
	}
	else if (K == TEXT("wickbeetle"))
	{
		// A waxy white dome on its back with drips down its sides, and the candle-yellow light organ at its tail.
		AddGroup(SpineBone, Spine, Yaw, Blocks([&](FKit& Kit)
		{
			Kit.Add(FSurf::Glaze(GcHex(TEXT("#E8DDB0")) * 0.62f, 0.55f), FVector(Rad * 1.5f, Rad * 1.3f, HH * 1.2f), FVector(0.f, 0.f, HH * 0.5f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			Kit.Add(FSurf::Glaze(GcHex(TEXT("#E8DDB0")) * 0.62f, 0.55f), FVector(Rad * 1.4f, Rad * 1.2f, HH * 0.8f), FVector(0.f, 0.f, HH * 1.3f), FRotator::ZeroRotator, EWRLookMesh::Cone);
			for (int32 d = 0; d < 6; ++d) Kit.Add(FSurf::Glaze(GcHex(TEXT("#E8DDB0")) * 0.6f, 0.55f), FVector(3.f, 3.f, 8.f + 3.f * (d % 3)), FVector(FMath::Cos(d * 1.05f) * Rad * 0.7f, FMath::Sin(d * 1.05f) * Rad * 0.6f, HH * 0.1f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			Kit.Add(FSurf::Lit(GcHex(TEXT("#FFC870")) * 0.6f, 5.f), FVector(Rad * 0.5f, Rad * 0.6f, HH * 0.6f), FVector(-Rad * 0.75f, 0.f, HH * 0.2f));
		}), GEyes);
		Glow = NewObject<UPointLightComponent>(C);
		Glow->SetupAttachment(Body, SpineBone);
		Glow->SetRelativeLocation(FVector(-Rad, 0.f, HH * 0.4f));
		Glow->SetLightColor(GcHex(TEXT("#FFC870")));
		Glow->SetIntensity(GcT(TEXT("wickbeetleLight"), 1400.f));
		Glow->SetAttenuationRadius(FWRTuning::Get().Num(TEXT("lights.creature.radius"), 1200.f));
		Glow->SetCastShadows(false);
		Glow->RegisterComponent();
	}
	else if (K == TEXT("marrowgoat"))
	{
		// Horns sweeping back and down, a bone gripped in its mouth, a long chalk-white coat hanging from the spine.
		AddGroup(HeadBone, Head, Yaw, Blocks([&](FKit& Kit)
		{
			const FSurf Horn = FSurf::Stone(GcHex(TEXT("#3A3530")) * 0.6f, 0.6f);
			for (const float S : { -1.f, 1.f })
			{
				FVector P(Rad * 0.05f, S * Rad * 0.12f, Rad * 0.2f);
				for (int32 h = 0; h < 6; ++h)
				{
					const float A = FMath::DegreesToRadians(40.f + h * 32.f);
					const FVector N = P + FVector(-FMath::Sin(A) * 9.f, S * 2.f, FMath::Cos(A) * 9.f);
					Kit.Add(Horn, FVector(FMath::Lerp(7.f, 2.5f, h / 5.f), FMath::Lerp(7.f, 2.5f, h / 5.f), 11.f), (P + N) * 0.5f, FRotationMatrix::MakeFromZ(N - P).Rotator(), EWRLookMesh::Cylinder);
					P = N;
				}
			}
			Kit.Add(FSurf::Stone(FLinearColor(FColor(225, 216, 196)) * 0.7f, 0.7f), FVector(3.f, Rad * 0.8f, 3.f), FVector(Rad * 0.35f, 0.f, -Rad * 0.1f), FRotator::ZeroRotator, EWRLookMesh::Cylinder).Rot = FRotator(0.f, 0.f, 90.f);
		}), GStatic);
		AddGroup(SpineBone, Spine, Yaw, Blocks([&](FKit& Kit)
		{
			for (int32 h = 0; h < 14; ++h) for (const float S : { -1.f, 1.f }) Kit.Add(FSurf::Cloth(GcHex(TEXT("#E6E1D3")) * 0.55f), FVector(Rad * 0.18f, 4.f, HH * 0.8f), FVector((h - 6.5f) * Rad * 0.11f, S * Rad * 0.42f, -HH * 0.25f), FRotator(0.f, 0.f, S * 8.f));
		}), GMane);
	}
	else if (K == TEXT("risen_mourner") || K == TEXT("risen_bearer"))
	{
		// A human skeleton on the Mixamo rig (its own clips drive every bone): skull, spine, ribcage, pelvis, the limbs'
		// long bones; grave wrappings, iron funeral bands, grave-light at the joints. The mourner carries a tōrō lantern on
		// a pole; the bearers (two skeletons fused at the shoulder) carry a coffin between them as a ram.
		const float S = HH / 90.f;
		const auto Pair = [&](const TCHAR* A, const TCHAR* B, float R) { BoneSeg(C->FindBone(FName(A)), C->FindBone(FName(B)), R * S, BoneC); };
		Pair(TEXT("Hips"), TEXT("Spine"), 3.f); Pair(TEXT("Spine"), TEXT("Spine1"), 3.f); Pair(TEXT("Spine1"), TEXT("Spine2"), 3.f); Pair(TEXT("Spine2"), TEXT("Neck"), 2.6f);
		Pair(TEXT("LeftArm"), TEXT("LeftForeArm"), 2.6f); Pair(TEXT("LeftForeArm"), TEXT("LeftHand"), 2.2f); Pair(TEXT("RightArm"), TEXT("RightForeArm"), 2.6f); Pair(TEXT("RightForeArm"), TEXT("RightHand"), 2.2f);
		Pair(TEXT("LeftUpLeg"), TEXT("LeftLeg"), 3.2f); Pair(TEXT("LeftLeg"), TEXT("LeftFoot"), 2.8f); Pair(TEXT("RightUpLeg"), TEXT("RightLeg"), 3.2f); Pair(TEXT("RightLeg"), TEXT("RightFoot"), 2.8f);
		Pair(TEXT("LeftFoot"), TEXT("LeftToeBase"), 2.f); Pair(TEXT("RightFoot"), TEXT("RightToeBase"), 2.f);
		Pair(TEXT("LeftShoulder"), TEXT("LeftArm"), 1.8f); Pair(TEXT("RightShoulder"), TEXT("RightArm"), 1.8f);
		FVector Hd = C->GetActorLocation(), Ch = Hd, Hp = Hd;
		const FName HdB = BoneAt(TEXT("Head"), Hd), ChB = BoneAt(TEXT("Spine1"), Ch), HpB = BoneAt(TEXT("Hips"), Hp);
		AddGroup(HdB, Hd, Yaw, Blocks([&](FKit& Kit)
		{
			Kit.Add(BoneS, FVector(20.f, 16.f, 18.f) * S, FVector(1.f, 0.f, 8.f) * S, FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			Kit.Add(BoneS, FVector(14.f, 12.f, 8.f) * S, FVector(6.f, 0.f, -2.f) * S);
			Kit.Add(BoneS, FVector(9.f, 10.f, 4.f) * S, FVector(8.f, 0.f, -7.f) * S);
			for (const float Sd : { -1.f, 1.f })
			{
				Kit.Add(FSurf::Stone(FLinearColor(0.01f, 0.01f, 0.01f), 0.9f), FVector(4.f, 4.f, 4.f) * S, FVector(9.f, Sd * 3.4f, 7.f) * S);
				Kit.Add(EyeLit, FVector(1.6f, 1.6f, 1.6f) * S, FVector(10.5f, Sd * 3.4f, 7.f) * S);
			}
		}), GEyes);
		AddGroup(ChB, Ch, Yaw, Blocks([&](FKit& Kit)
		{
			for (int32 r = 0; r < 6; ++r)
			{
				const float Z = (8.f - r * 4.5f) * S, Wd = (15.f - FMath::Abs(r - 2.f) * 1.6f) * S;
				for (const float Sd : { -1.f, 1.f })
				{
					Kit.Add(BoneS, FVector(1.6f, 1.6f, Wd) * 1.f, FVector(-2.f * S, Sd * Wd * 0.45f, Z), FRotator(0.f, 0.f, Sd * 70.f), EWRLookMesh::Cylinder);
					Kit.Add(BoneS, FVector(1.4f, 1.4f, Wd * 0.8f), FVector(6.f * S, Sd * Wd * 0.4f, Z - 1.f * S), FRotator(0.f, Sd * 70.f, 90.f), EWRLookMesh::Cylinder);
				}
			}
			Kit.Add(BoneS, FVector(3.f, 4.f, 16.f) * S, FVector(9.f, 0.f, 2.f) * S);   // sternum
			Kit.Add(FSurf::Cloth(GcHex(TEXT("#D6D0C0")) * 0.5f), FVector(18.f, 30.f, 8.f) * S, FVector(1.f, 0.f, -12.f) * S, FRotator(0.f, 0.f, 6.f));   // grave wrappings
			Kit.Add(FSurf::Steel(FLinearColor(0.04f, 0.035f, 0.03f), 0.7f), FVector(20.f, 31.f, 2.5f) * S, FVector(1.f, 0.f, -6.f) * S);                   // iron band
		}), GStatic);
		AddGroup(HpB, Hp, Yaw, Blocks([&](FKit& Kit)
		{
			Kit.Add(BoneS, FVector(12.f, 24.f, 10.f) * S, FVector(0.f, 0.f, -2.f) * S, FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			Kit.Add(FSurf::Cloth(GcHex(TEXT("#D6D0C0")) * 0.48f), FVector(22.f, 30.f, 26.f) * S, FVector(0.f, 0.f, -16.f) * S);
			for (int32 g = 0; g < 4; ++g) Kit.Add(Thread, FVector(1.f, 1.f, 10.f) * S, FVector(0.f, (g - 1.5f) * 6.f, 4.f) * S, FRotator(0.f, 0.f, 30.f * g), EWRLookMesh::Cylinder);
		}), GThreads);
		FVector Hand;
		const FName HandB = BoneAt(TEXT("RightHand"), Hand);
		if (K == TEXT("risen_mourner"))
		{
			// The lantern-pole: a 2.4 m pole in the right hand, a crook and a paper tōrō-lantern lit green.
			AddGroup(HandB, Hand, Yaw, Blocks([&](FKit& Kit)
			{
				Kit.Add(FSurf::Wood(GcHex(TEXT("#6E665C")) * 0.45f), FVector(4.f, 4.f, 240.f), FVector(0.f, 0.f, 60.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
				Kit.Add(FSurf::Wood(GcHex(TEXT("#6E665C")) * 0.45f), FVector(3.f, 3.f, 34.f), FVector(14.f, 0.f, 180.f), FRotator(90.f, 0.f, 0.f), EWRLookMesh::Cylinder);
				Kit.Add(FSurf::Cloth(GcHex(TEXT("#E8DEBE")) * 0.55f), FVector(22.f, 22.f, 28.f), FVector(30.f, 0.f, 152.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
				Kit.Add(FSurf::Lit(GcHex(TEXT("#9FE3C0")) * 0.6f, 4.f), FVector(23.f, 23.f, 26.f), FVector(30.f, 0.f, 152.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			}), GEyes);
			Glow = NewObject<UPointLightComponent>(C);
			Glow->SetupAttachment(Body, HandB);
			Glow->SetRelativeLocation(FVector(0.f, 0.f, 150.f));
			Glow->SetLightColor(GcHex(TEXT("#9FE3C0")));
			Glow->SetIntensity(GcT(TEXT("risenLanternLight"), 900.f));
			Glow->SetAttenuationRadius(700.f);
			Glow->SetCastShadows(false);
			Glow->RegisterComponent();
		}
		else
		{
			// The coffin carried as a ram (in both hands across the body), the second skeleton fused at the back.
			AddGroup(HandB, Hand, Yaw, Blocks([&](FKit& Kit)
			{
				Kit.Add(FSurf::Wood(GcHex(TEXT("#5A4E40")) * 0.45f), FVector(210.f, 62.f, 46.f), FVector(60.f, -30.f, 0.f), FRotator(0.f, -90.f, 0.f));
				Kit.Add(FSurf::Steel(FLinearColor(0.04f, 0.035f, 0.03f), 0.7f), FVector(214.f, 64.f, 4.f), FVector(60.f, -30.f, 18.f), FRotator(0.f, -90.f, 0.f));
			}), GStatic);
			AddGroup(ChB, Ch, Yaw, Blocks([&](FKit& Kit)
			{
				Kit.Add(BoneS, FVector(16.f, 13.f, 15.f) * S, FVector(-18.f, 10.f, 34.f) * S, FRotator(0.f, 160.f, 0.f), EWRLookMesh::Cylinder);
				for (int32 r = 0; r < 4; ++r) Kit.Add(BoneS, FVector(1.5f, 1.5f, 13.f * S), FVector(-16.f * S, 10.f * S, (10.f - r * 5.f) * S), FRotator(0.f, 0.f, 70.f), EWRLookMesh::Cylinder);
			}), GStatic);
		}
	}
	else if (K == TEXT("risen_hound"))
	{
		// A Gravehound's skeleton on the Barghest rig: skull with grave-light in its sockets, the spine, ribs, every limb.
		const auto Pair = [&](const TCHAR* A, const TCHAR* B, float R) { BoneSeg(C->FindBone(FName(A)), C->FindBone(FName(B)), R, BoneC); };
		Pair(TEXT("Pelvis"), TEXT("Spine"), 3.f); Pair(TEXT("Spine"), TEXT("Spine1"), 3.f); Pair(TEXT("Spine1"), TEXT("Spine2"), 3.f); Pair(TEXT("Spine2"), TEXT("Spine3"), 3.f);
		Pair(TEXT("Spine3"), TEXT("Neck"), 2.6f); Pair(TEXT("Neck"), TEXT("Neck1"), 2.4f); Pair(TEXT("Neck1"), TEXT("Head"), 2.4f);
		Pair(TEXT("L-UpperArm"), TEXT("L-Forearm"), 2.4f); Pair(TEXT("L-Forearm"), TEXT("L-Hand"), 2.f); Pair(TEXT("R-UpperArm"), TEXT("R-Forearm"), 2.4f); Pair(TEXT("R-Forearm"), TEXT("R-Hand"), 2.f);
		Pair(TEXT("L-Thigh"), TEXT("L-Calf"), 2.8f); Pair(TEXT("L-Calf"), TEXT("L-HorseLink"), 2.4f); Pair(TEXT("L-HorseLink"), TEXT("L-Foot"), 2.f);
		Pair(TEXT("R-Thigh"), TEXT("R-Calf"), 2.8f); Pair(TEXT("R-Calf"), TEXT("R-HorseLink"), 2.4f); Pair(TEXT("R-HorseLink"), TEXT("R-Foot"), 2.f);
		Pair(TEXT("Tail"), TEXT("Tail1"), 1.6f); Pair(TEXT("Tail1"), TEXT("Tail2"), 1.4f); Pair(TEXT("Tail2"), TEXT("Tail3"), 1.2f);
		AddGroup(HeadBone, Head, Yaw, Blocks([&](FKit& Kit)
		{
			Kit.Add(BoneS, FVector(Rad * 0.55f, Rad * 0.35f, Rad * 0.3f), FVector(Rad * 0.1f, 0.f, Rad * 0.05f));
			Kit.Add(BoneS, FVector(Rad * 0.45f, Rad * 0.2f, Rad * 0.14f), FVector(Rad * 0.45f, 0.f, -Rad * 0.04f));
			for (const float Sd : { -1.f, 1.f }) Kit.Add(EyeLit, FVector(Rad * 0.06f, Rad * 0.06f, Rad * 0.06f), FVector(Rad * 0.28f, Sd * Rad * 0.12f, Rad * 0.12f));
		}), GEyes);
		AddGroup(SpineBone, Spine, Yaw, Blocks([&](FKit& Kit)
		{
			for (int32 r = 0; r < 7; ++r) for (const float Sd : { -1.f, 1.f }) Kit.Add(BoneS, FVector(1.8f, 1.8f, Rad * 0.55f), FVector((r - 3) * Rad * 0.12f, Sd * Rad * 0.22f, -Rad * 0.12f), FRotator(0.f, 0.f, Sd * 35.f), EWRLookMesh::Cylinder);
		}), GStatic);
	}
	ApplyBuriedLook();
}

void UWRGraveBody::ApplyBuriedLook()
{
	// A buried hound: its body and parts out of sight, a fresh mound of grave soil where it lies (and its eyes in the dirt
	// at night: the mound's two soul-green points).
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	USkeletalMeshComponent* Body = Skel();
	if (!C || !Body || GetNetMode() == NM_DedicatedServer) return;
	const bool bBuried = Has(WRGraveFlag::Buried);
	if (bBuried == bBuriedShown && (Mound != nullptr) == bBuried) return;
	bBuriedShown = bBuried;
	Body->SetVisibility(!bBuried && !bMeshHidden, false);
	for (USceneComponent* Comp : LookComps) if (Comp) Comp->SetVisibility(!bBuried, true);
	if (bBuried && !Mound)
	{
		Mound = NewObject<USceneComponent>(C);
		Mound->SetupAttachment(C->GetRootComponent());
		Mound->SetRelativeLocation(FVector(0.f, 0.f, -C->GetSimpleCollisionHalfHeight()));
		Mound->RegisterComponent();
		TArray<FWRLookBlock> Out;
		WRLookKit::FKit Kit(FTransform::Identity, Out);
		const float R = C->GetSimpleCollisionRadius();
		Kit.Add(WRLookKit::FSurf::Cloth(GcHex(TEXT("#3B3530")) * 0.5f), FVector(R * 3.2f, R * 1.8f, 28.f), FVector(0.f, 0.f, 4.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		for (int32 k = 0; k < 6; ++k) Kit.Add(WRLookKit::FSurf::Cloth(GcHex(TEXT("#2E2924")) * 0.5f), FVector(R * 0.7f, R * 0.5f, 14.f), FVector(FMath::Cos(k * 1.05f) * R * 1.1f, FMath::Sin(k * 1.05f) * R * 0.7f, 10.f), FRotator(10.f * (k % 3 - 1), 60.f * k, 0.f));
		for (const float S : { -1.f, 1.f }) Kit.Add(WRLookKit::FSurf::Lit(GcHex(TEXT("#9FE3C0")) * 0.6f, 2.5f), FVector(3.f, 3.f, 2.f), FVector(R * 0.9f, S * 6.f, 18.f));
		FWRBuildLook::Spawn(C, Mound, Out, FLinearColor::White, false);
	}
	else if (!bBuried && Mound)
	{
		Mound->DestroyComponent();
		Mound = nullptr;
	}
}

void UWRGraveBody::TickLook(float DeltaTime)
{
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	USkeletalMeshComponent* Body = Skel();
	if (!C || !Body) return;
	if (LookFor != C->CreatureId || (Groups.Num() == 0 && !bMeshHidden) || LookFlags != (Flags & (WRGraveFlag::Raised | WRGraveFlag::Riftborn | WRGraveFlag::Ash | WRGraveFlag::Ice)))
	{
		// The rig and its materials arrive a frame or two after the creature (OnRep): try for a few seconds.
		if (LookTries < 40 && Body->GetSkeletalMeshAsset()) { ++LookTries; BuildLook(); }
		if (LookFor != C->CreatureId) return;
	}
	// A Cryptstalker on a ceiling hangs upside down.
	const bool bCeiling = Has(WRGraveFlag::Ceiling);
	const FRotator WantRoll = bCeiling ? FRotator(0.f, Body->GetRelativeRotation().Yaw, 180.f) : FRotator(0.f, Body->GetRelativeRotation().Yaw, 0.f);
	if (!Body->GetRelativeRotation().Equals(WantRoll, 1.f))
	{
		Body->SetRelativeRotation(FMath::RInterpTo(Body->GetRelativeRotation(), WantRoll, DeltaTime, 8.f));
		Body->SetRelativeLocation(FVector(Body->GetRelativeLocation().X, Body->GetRelativeLocation().Y, bCeiling ? C->GetSimpleCollisionHalfHeight() : -C->GetSimpleCollisionHalfHeight()));
	}
	const bool bSweep = Has(WRGraveFlag::Sweeping);
	const bool bNight = GcNight(C) || WRGrave::InCatacombs(C->GetActorLocation());
	for (FGroup& G : Groups)
	{
		USceneComponent* F = G.Frame.Get();
		if (!F) continue;
		if (G.Kind == GWhip)
		{
			// The whips feel the air: slow overlapping sweeps; fanned wide and fast on the whip sweep.
			const float Amp = bSweep ? 55.f : 14.f, Speed = bSweep ? 6.f : 1.3f;
			F->SetRelativeRotation(G.BaseRot + FRotator(6.f * FMath::Sin(Clock * Speed * 0.7f + G.Phase), Amp * FMath::Sin(Clock * Speed + G.Phase), 0.f));
		}
		else if (G.Kind == GMane)
		{
			F->SetRelativeRotation(G.BaseRot + FRotator(0.f, 0.f, 6.f * FMath::Sin(Clock * 2.1f + G.Phase)));
		}
		else if (G.Kind == GProboscis)
		{
			const float U = Has(WRGraveFlag::Holding) ? 1.f : 0.f;
			F->SetRelativeRotation(G.BaseRot + FRotator(-40.f * U, 0.f, 0.f));
		}
		else if (G.Kind == GEyes)
		{
			F->SetRelativeScale3D(FVector(bNight ? 1.f : 0.97f));
		}
	}
	if (Glow) Glow->SetIntensity((C->CreatureId == TEXT("wickbeetle") ? GcT(TEXT("wickbeetleLight"), 1400.f) : GcT(TEXT("risenLanternLight"), 900.f)) * (0.85f + 0.15f * FMath::Sin(Clock * 3.1f)));
}

// ================================================================================================ the brain

bool FWRGraveAI::NoBody(const AWRCreature* Self)
{
	return Self && UWRGraveBody::IsRisen(Self->CreatureId);
}

AWRCreature* FWRGraveAI::HolderOf(const AActor* Victim)
{
	for (const TWeakObjectPtr<UWRGraveBody>& B : UWRGraveBody::Live)
	{
		if (B.IsValid() && B->Holding == Victim) return Cast<AWRCreature>(B->GetOwner());
	}
	return nullptr;
}

void FWRGraveAI::Bury(AWRCreature* Self, UWRGraveBody* Body)
{
	if (!Self || !Body || Body->Has(WRGraveFlag::Buried)) return;
	Body->Set(WRGraveFlag::Buried, true);
	Self->GetCharacterMovement()->StopMovementImmediately();
	Self->GetCharacterMovement()->DisableMovement();
	Self->SetActorEnableCollision(false);
	Self->SetAggroTarget(nullptr);
}

void FWRGraveAI::Unbury(AWRCreature* Self, UWRGraveBody* Body)
{
	if (!Self || !Body || !Body->Has(WRGraveFlag::Buried)) return;
	Body->Set(WRGraveFlag::Buried, false);
	Self->SetActorEnableCollision(true);
	Self->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	Body->RiseStartedAt = GcNow(Self);
	// The soil heaves in a 1 m ring, a dry rattle of breath (the Rising's tell plays with its row).
	Body->MulticastMoment(TEXT("grave.soil_heave"), NAME_None, 1.f, TEXT("creature.gravehound.rise"));
}

int32 FWRGraveAI::WakeBuried(UWorld* World, const FVector& At, float Radius, AActor* Toward)
{
	int32 N = 0;
	for (const TWeakObjectPtr<UWRGraveBody>& B : UWRGraveBody::Live)
	{
		UWRGraveBody* Body = B.Get();
		AWRCreature* C = Body ? Cast<AWRCreature>(Body->GetOwner()) : nullptr;
		if (!C || !C->HasAuthority() || !Body->Has(WRGraveFlag::Buried) || FVector::Dist(C->GetActorLocation(), At) > Radius) continue;
		Unbury(C, Body);
		if (Toward) C->SetAggroTarget(Toward);
		++N;
	}
	return N;
}

void FWRGraveAI::StartHold(AWRCreature* Self, UWRGraveBody* Body, AWRCharacterBase* Victim, FName Kind, float Seconds)
{
	if (!Self || !Body || !Victim || !Victim->IsAlive() || HolderOf(Victim)) return;
	Body->HeldBody = Victim;
	Body->Holding = Victim;
	Body->HoldKind = Kind;
	Body->HoldUntil = GcNow(Self) + Seconds;
	Body->HoldTickAt = GcNow(Self) + 1.f;
	Body->FreeDamage = 0.f;
	Body->Set(WRGraveFlag::Holding, true);
	if (Victim->Status) Victim->Status->Stagger(Seconds + 0.1f);
	Self->SetActorRotation(FRotator(0.f, (Victim->GetActorLocation() - Self->GetActorLocation()).Rotation().Yaw, 0.f));
	// The tear-drinker on your face: your view goes dark to a sliver while it drinks (GY 2.12.2).
	if (Kind == TEXT("tear")) if (AWRPlayerController* PC = GcPC(Victim)) PC->ClientSetCameraFade(true, FColor::Black, FVector2D(0.f, GcT(TEXT("tearDark"), 0.82f)), 0.4f, false, true);
	Body->MulticastMoment(Kind == TEXT("tear") ? FName(TEXT("grave.moth_dust")) : FName(TEXT("hit.blood")), TEXT("head"), 1.f, FName(*FString::Printf(TEXT("creature.%s.signature"), *Self->CreatureId.ToString())));
	UE_LOG(LogWildreach, Log, TEXT("GRAVE %s holds %s (%s, %.1f s)"), *Self->GetName(), *Victim->GetName(), *Kind.ToString(), Seconds);
}

void FWRGraveAI::Release(AWRCreature* Self, UWRGraveBody* Body, bool bThrow)
{
	AWRCharacterBase* V = Body->HeldBody.Get();
	const FName Kind = Body->HoldKind;
	Body->HeldBody = nullptr;
	Body->Holding = nullptr;
	Body->Set(WRGraveFlag::Holding, false);
	if (!V) return;
	if (Kind == TEXT("tear")) if (AWRPlayerController* PC = GcPC(V)) PC->ClientSetCameraFade(false, FColor::Black, FVector2D(0.f, 0.f), 0.3f, false, false);
	if (V->Status) V->Status->ClearStatus(WRTags::Status_Staggered);
	if (bThrow && V->IsAlive())
	{
		const FVector Push = (V->GetActorLocation() - Self->GetActorLocation()).GetSafeNormal2D() * GcT(TEXT("throw"), 450.f) + FVector(0, 0, 200.f);
		V->LaunchCharacter(Push, true, true);
	}
}

void FWRGraveAI::Mash(AWRCharacterBase* Victim)
{
	AWRCreature* Holder = HolderOf(Victim);
	UWRGraveBody* Body = UWRGraveBody::Get(Holder);
	if (!Body) return;
	Body->HoldUntil -= Body->HoldKind == TEXT("tear") ? GcT(TEXT("tearMash"), 0.2f) : GcT(TEXT("palpMash"), 0.15f);
}

void FWRGraveAI::TickBody(AWRCreature* Self, UWRGraveBody* Body, float DeltaTime)
{
	const double Now = GcNow(Self);
	if (!Self->IsAlive()) return;
	// A hold: its damage ticks (through ApplyHit), freed by allies' blows, by mashing, by time.
	if (AWRCharacterBase* V = Body->HeldBody.Get())
	{
		const bool bTear = Body->HoldKind == TEXT("tear");
		if (!V->IsAlive() || Now >= Body->HoldUntil || Body->FreeDamage >= (bTear ? 1.f : GcT(TEXT("palpFree"), 150.f)))
		{
			Release(Self, Body, !bTear);
		}
		else if (Now >= Body->HoldTickAt)
		{
			Body->HoldTickAt = Now + 1.0;
			FWRHit Hit;
			Hit.Instigator = Self;
			Hit.Damage = bTear ? GcT(TEXT("tearDps"), 6.f) : GcT(TEXT("palpDps"), 15.f);
			Hit.bUnblockable = true;
			Hit.bMelee = true;
			Hit.Origin = Self->GetActorLocation();
			Hit.Element = WRTags::Element_Physical;
			if (bTear) Hit.StaminaDamage = GcT(TEXT("tearStamina"), 8.f);
			UWRCombatLibrary::ApplyHit(V, Hit);
			if (V->Status) V->Status->Stagger(1.1f);
		}
	}
	// A Wraithmoth down after its dive: back up when the window closes.
	if (Body->Has(WRGraveFlag::Grounded) && Now >= Body->GroundedUntil)
	{
		Body->Set(WRGraveFlag::Grounded, false);
		Self->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	}
	// A Cryptstalker coming down from its ceiling: the impact where it lands.
	if (Body->bDropping && Self->GetCharacterMovement()->IsMovingOnGround())
	{
		Body->bDropping = false;
		const TMap<FName, FWRAttackRow>* Rows = FWRAttackRows::Get().Creature(Self->CreatureId);
		const FWRAttackRow* R = Rows ? Rows->Find(TEXT("ceiling_drop")) : nullptr;
		const float Rad = R ? R->Radius : 250.f;
		Body->MulticastMoment(TEXT("grave.chalk_fall"), NAME_None, 1.4f, TEXT("creature.cryptstalker.land"));
		if (AWRCreatureAI* AI = Cast<AWRCreatureAI>(Self->GetController()))
		{
			for (TActorIterator<AWRCharacterBase> It(Self->GetWorld()); It; ++It)
			{
				if (*It != Self && It->IsAlive() && Self->IsHostileTo(*It) && FVector::Dist(It->GetActorLocation(), Self->GetActorLocation()) < Rad + Self->GetSimpleCollisionRadius()) AI->RowHit(Self, *It);
			}
		}
	}
	// A buried Gravehound: wakes at night when something comes near (or by day if stepped on); goes back under at dawn.
	if (Self->CreatureId == TEXT("gravehound") && !Self->IsTamed() && !Body->Has(WRGraveFlag::Raised))
	{
		const bool bNight = GcNight(Self);
		if (Body->Has(WRGraveFlag::Buried))
		{
			const float Wake = bNight ? GcT(TEXT("wakeNight"), 1500.f) : GcT(TEXT("wakeDay"), 350.f);
			for (FConstPlayerControllerIterator It = Self->GetWorld()->GetPlayerControllerIterator(); It; ++It)
			{
				APawn* P = It->Get() ? It->Get()->GetPawn() : nullptr;
				AWRCharacterBase* Ch = Cast<AWRCharacterBase>(P);
				if (Ch && Ch->IsAlive() && FVector::Dist(P->GetActorLocation(), Self->GetActorLocation()) < Wake)
				{
					WakeBuried(Self->GetWorld(), Self->GetActorLocation(), GcT(TEXT("denWake"), 900.f), Ch);
					break;
				}
			}
		}
		else if (!bNight && Self->SinceHit() > GcT(TEXT("reburyAfter"), 30.f) && Body->HeldBody == nullptr)
		{
			// Dawn: the pack lies down into the grave soil where it stands (not mid-fight).
			if (AWRCreatureAI* AI = Cast<AWRCreatureAI>(Self->GetController()); AI && AI->RowPhase == 0 && !Self->GetAggroTarget()) Bury(Self, Body);
		}
	}
	if (Body->Has(WRGraveFlag::Sweeping) && Now > Body->FoundUntil - 7.4) Body->Set(WRGraveFlag::Sweeping, false);
}

bool FWRGraveAI::TickWild(AWRCreatureAI* AI, AWRCreature* Self, float DeltaSeconds)
{
	UWRGraveBody* Body = UWRGraveBody::Get(Self);
	if (!Body) return false;
	const double Now = GcNow(Self);
	if (Body->Has(WRGraveFlag::Buried) || Body->HeldBody.IsValid() || Body->bDropping) return true;
	if (Body->Has(WRGraveFlag::Grounded))
	{
		Self->GetCharacterMovement()->StopMovementImmediately();
		return true;
	}
	// A hound just up out of the soil goes straight into its Rising at what woke it.
	if (Self->CreatureId == TEXT("gravehound") && Now - Body->RiseStartedAt < 0.3)
	{
		if (AWRCharacterBase* T = Cast<AWRCharacterBase>(Self->GetAggroTarget())) { AI->StartRow(Self, T, TEXT("rising"), false); return true; }
	}
	// On the ceiling: hold still above the room; drop on what walks under.
	if (Body->Has(WRGraveFlag::Ceiling))
	{
		Self->GetCharacterMovement()->StopMovementImmediately();
		Self->SetActorLocation(Body->CeilingAt);
		if (AI->RowPhase != 0) return false;
		for (TActorIterator<AWRCharacterBase> It(Self->GetWorld()); It; ++It)
		{
			if (*It == Self || !It->IsAlive() || !Self->IsHostileTo(*It) || Cast<AWRCreature>(*It)) continue;
			if (FVector::Dist2D(It->GetActorLocation(), Self->GetActorLocation()) < GcT(TEXT("dropReach"), 420.f) && It->GetActorLocation().Z < Self->GetActorLocation().Z)
			{
				Self->SetAggroTarget(*It);
				AI->StartRow(Self, *It, TEXT("ceiling_drop"), false);
				break;
			}
		}
		return true;
	}
	// A Wraithmoth flies at the brightest light within 30 m (a torch carried, a lantern, a fire): the bearer is its mark.
	if (Self->CreatureId == TEXT("wraithmoth") && Now >= Body->NextLightLookAt)
	{
		Body->NextLightLookAt = Now + 1.0;
		if (UWRLightSubsystem* L = UWRLightSubsystem::Get(Self))
		{
			if (AActor* Lit = L->BrightestNear(Self->GetActorLocation(), GcT(TEXT("mothLightRange"), 3000.f)))
			{
				if (AWRCharacterBase* Bearer = Cast<AWRCharacterBase>(Lit); Bearer && Bearer->IsAlive() && Self->IsHostileTo(Bearer)) Self->SetAggroTarget(Bearer);
				else if (!Self->GetAggroTarget()) Self->SetHome(Lit->GetActorLocation() + FVector(0.f, 0.f, 250.f));   // circles a lamp
			}
		}
	}
	// A crow at a funeral: perched silent until the killer is named, then the mob.
	if (Body->Has(WRGraveFlag::Silent))
	{
		if (Now >= Body->SilentUntil)
		{
			Body->Set(WRGraveFlag::Silent, false);
			if (AActor* K = Body->MobTarget.Get(); K && FVector::Dist(K->GetActorLocation(), Self->GetActorLocation()) < GcT(TEXT("mobReach"), 3000.f)) Self->SetAggroTarget(K);
		}
		else return true;
	}
	return false;
}

bool FWRGraveAI::TickTame(AWRCreatureAI* AI, AWRCreature* Self, float DeltaSeconds)
{
	UWRGraveBody* Body = UWRGraveBody::Get(Self);
	if (!Body) return false;
	if (Body->Has(WRGraveFlag::Buried)) Unbury(Self, Body);
	if (Body->Has(WRGraveFlag::Ceiling)) { Body->Set(WRGraveFlag::Ceiling, false); Self->GetCharacterMovement()->SetMovementMode(MOVE_Falling); }
	return false;
}

bool FWRGraveAI::RowAllowed(const AWRCreatureAI* AI, const AWRCreature* Self, const FWRAttackRow& Row, const AWRCharacterBase* Target, bool bReading)
{
	const UWRGraveBody* Body = UWRGraveBody::Get(Self);
	if (!Body) return true;
	const FName Id = Row.Id;
	const double Now = GcNow(Self);
	if (Body->Has(WRGraveFlag::Buried) || Body->Has(WRGraveFlag::Grounded)) return false;
	// Gravehound: the Rising only straight out of the soil; the Grim howl once a fight.
	if (Id == TEXT("rising")) return Now - Body->RiseStartedAt < 2.5;
	if (Id == TEXT("grim_howl")) return !Body->bHowled;
	// Cryptstalker: the drop only from a ceiling, nothing else from one; in the dark it goes only for what it found.
	if (Id == TEXT("ceiling_drop")) return Body->Has(WRGraveFlag::Ceiling);
	if (Body->Has(WRGraveFlag::Ceiling)) return false;
	if (Self->CreatureId == TEXT("cryptstalker") && Target)
	{
		const UWRSurvivalComponent* S = UWRSurvivalComponent::For(Target);
		const bool bDark = (S && S->IsUnlit()) || WRGrave::InCatacombs(Target->GetActorLocation());
		const bool bFound = Body->Found == Target && Now < Body->FoundUntil;
		if (bDark && !bFound && Id != TEXT("whip_sweep")) return false;
		if (bFound && Id == TEXT("whip_sweep")) return false;
	}
	// Wraithmoth: the tear-drinker only on a survivor already down or reeling.
	if (Id == TEXT("tear_drinker")) return Target && Target->Status && (Target->Status->HasStatus(WRTags::Status_Staggered) || Target->Status->HasStatus(WRTags::Status_StanceBroken));
	return true;
}

void FWRGraveAI::RowStarted(AWRCreatureAI* AI, AWRCreature* Self, const FWRAttackRow& Row, AWRCharacterBase* Target)
{
	UWRGraveBody* Body = UWRGraveBody::Get(Self);
	if (!Body) return;
	const FName Id = Row.Id;
	if (Id == TEXT("grim_howl"))
	{
		Body->bHowled = true;
		const int32 N = WakeBuried(Self->GetWorld(), Self->GetActorLocation(), GcT(TEXT("howlWake"), 4000.f), Target);
		Body->MulticastMoment(NAME_None, TEXT("head"), 1.f, TEXT("creature.gravehound.howl"));
		UE_LOG(LogWildreach, Log, TEXT("GRAVE %s howls: %d buried hounds rise"), *Self->GetName(), N);
	}
	else if (Id == TEXT("whip_sweep"))
	{
		Body->Set(WRGraveFlag::Sweeping, true);
		Body->FoundUntil = FMath::Max(Body->FoundUntil, GcNow(Self) + 8.0);
	}
	else if (Id == TEXT("ceiling_drop") && Target)
	{
		// Chalk dust sifting over the target, dry clicking above.
		WRFx::Broadcast(Self, TEXT("grave.chalk_sift"), Target->GetActorLocation() + FVector(0.f, 0.f, 200.f), FRotator::ZeroRotator, 1.f, TEXT("creature.cryptstalker.click"), Row.Windup);
	}
	else if (Id == TEXT("slab_slam")) Body->Set(WRGraveFlag::Rearing, true);
	else if (Id == TEXT("light_dive")) Body->MulticastMoment(NAME_None, TEXT("body"), 1.f, TEXT("creature.wraithmoth.squeak"));
	else if (Id == TEXT("scale_dust")) Body->MulticastMoment(TEXT("grave.moth_dust"), TEXT("body"), 1.4f, NAME_None);
	else if (Id == TEXT("rising")) Body->MulticastMoment(TEXT("grave.soil_heave"), NAME_None, 1.2f, TEXT("creature.gravehound.rise"));
}

bool FWRGraveAI::StrikeRow(AWRCreatureAI* AI, AWRCreature* Self, const FWRAttackRow& Row, AWRCharacterBase* Target)
{
	UWRGraveBody* Body = UWRGraveBody::Get(Self);
	if (!Body || Row.Id != TEXT("ceiling_drop")) return false;
	// Off the ceiling and down onto the mark: it falls (the body's own weight, never a snap).
	Body->Set(WRGraveFlag::Ceiling, false);
	Body->bDropping = true;
	Self->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	const FVector To = Target ? Target->GetActorLocation() - Self->GetActorLocation() : FVector::ZeroVector;
	Self->LaunchCharacter(FVector(To.X * 1.2f, To.Y * 1.2f, -300.f), true, true);
	return true;
}

void FWRGraveAI::Resolved(AWRCreature* Self, AWRCharacterBase* Victim, FName RowId, EWRHitResult Result)
{
	UWRGraveBody* Body = UWRGraveBody::Get(Self);
	if (!Body || !Victim) return;
	const bool bLanded = Result == EWRHitResult::Hit || Result == EWRHitResult::GuardBroken;
	if (RowId == TEXT("slab_slam")) Body->Set(WRGraveFlag::Rearing, false);
	if (RowId == TEXT("whip_sweep"))
	{
		// A touch: found (the click-lock).
		Body->Found = Victim;
		Body->FoundUntil = GcNow(Self) + GcT(TEXT("foundSeconds"), 8.f);
		WRFx::Broadcast(Self, NAME_None, Victim->GetActorLocation(), FRotator::ZeroRotator, 1.f, TEXT("creature.cryptstalker.found"));
		return;
	}
	if (!bLanded) return;
	if (RowId == TEXT("palp_snatch") || RowId == TEXT("palp_snatch_read")) StartHold(Self, Body, Victim, TEXT("palp"), GcT(TEXT("palpHold"), 2.f));
	else if (RowId == TEXT("tear_drinker")) StartHold(Self, Body, Victim, TEXT("tear"), GcT(TEXT("tearHold"), 2.5f));
	else if (RowId == TEXT("light_dive"))
	{
		// A hit puts out a held torch; then it's down on the ground, the window to bring it down.
		if (UWRSurvivalComponent* S = UWRSurvivalComponent::For(Victim); S && S->SnuffTorch()) WRSound::Broadcast(Victim, TEXT("light.off"));
		Body->Set(WRGraveFlag::Grounded, true);
		Body->GroundedUntil = GcNow(Self) + GcT(TEXT("mothGrounded"), 1.4f);
		Self->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	}
	else if (RowId == TEXT("scale_dust"))
	{
		Chilled().Add(Victim, GcNow(Self) + GcT(TEXT("chillSeconds"), 6.f));
	}
	else if (RowId == TEXT("grave_grasp"))
	{
		if (Victim->Status) Victim->Status->ApplyStatus(WRTags::Status_Rooted, GcT(TEXT("graspRoot"), 1.f));
	}
	else if (RowId == TEXT("wax_squirt"))
	{
		if (Victim->Status) Victim->Status->ApplyStatus(WRTags::Status_Slowed, GcT(TEXT("waxSlow"), 3.f));
	}
	else if (RowId == TEXT("hamstring"))
	{
		if (Victim->Status) Victim->Status->ApplyStatus(WRTags::Status_Slowed, GcT(TEXT("hamstringSlow"), 1.5f));
	}
}

void FWRGraveAI::ModifyHit(const AWRCreature* Self, FWRHit& Hit, const FWRAttackRow* Row)
{
	if (!UWRGraveBody::Get(Self)) return;
	// The Graveyard's own blows carry decay where the row says (attacks.json buildup); a raised body is the
	// Necromancer's (its element is necromancy: Final harvest and the minion bonuses read it).
	const UWRGraveBody* Body = UWRGraveBody::Get(Self);
	if (Body && Body->Has(WRGraveFlag::Raised)) Hit.PathId = TEXT("necromancy");
	// A Riftborn's every blow carries curse.
	if (Body && Body->Has(WRGraveFlag::Riftborn)) Hit.Buildups.FindOrAdd(TEXT("curse")) += GcT(TEXT("riftborn.curse"), 15.f);
}

float FWRGraveAI::Incoming(const AWRCreature* Self, const FWRHit& Hit, float Damage)
{
	const UWRGraveBody* Body = UWRGraveBody::Get(Self);
	if (!Body) return Damage;
	if (Body->Has(WRGraveFlag::Buried)) return 0.f;
	// The Risen: necrotic magic knits rather than breaks them (x0.7); bone breaks under blows (strike: physResist).
	if (UWRGraveBody::IsRisen(Self->CreatureId) && Hit.PathId == TEXT("necromancy") && !Hit.bMelee) Damage *= GcT(TEXT("risenNecrotic"), 0.7f);
	// The Tombback's slab: blows from in front land on the stone (x0.3) unless it's rearing (the belly x1.3).
	if (Self->CreatureId == TEXT("tombback"))
	{
		const FVector To = (Hit.Origin - Self->GetActorLocation()).GetSafeNormal2D();
		const bool bFront = FVector::DotProduct(To, Self->GetActorForwardVector()) > 0.3f;
		if (Body->Has(WRGraveFlag::Rearing)) Damage *= GcT(TEXT("bellyMult"), 1.3f);
		else if (bFront) Damage *= GcT(TEXT("slabMult"), 0.3f);
	}
	return Damage;
}

void FWRGraveAI::Damaged(AWRCreature* Self, const FWRHit& Hit, float Damage)
{
	UWRGraveBody* Body = UWRGraveBody::Get(Self);
	if (!Body) return;
	// A blow on a grab's holder frees the held (palp: 150 of it; the moth: any blow; fire burns it off).
	if (Body->HeldBody.IsValid() && Hit.Instigator != Body->HeldBody.Get()) Body->FreeDamage += Damage + (Hit.Element == WRTags::Element_Fire ? 1000.f : 0.f);
	// A Wraithmoth hit in the air comes down (an arrow, a bolt: the bring-down).
	if (Self->CreatureId == TEXT("wraithmoth") && !Hit.bMelee && !Body->Has(WRGraveFlag::Grounded))
	{
		Body->Set(WRGraveFlag::Grounded, true);
		Body->GroundedUntil = GcNow(Self) + GcT(TEXT("mothShotDown"), 2.f);
		Self->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	}
	// A Palestrider screams and the herd within 30 m turns (the death scream rally).
	if (Self->CreatureId == TEXT("palestrider") && Hit.Instigator && GcNow(Self) >= Body->NextVoiceAt)
	{
		Body->NextVoiceAt = GcNow(Self) + 4.0;
		Body->MulticastMoment(NAME_None, TEXT("head"), 1.f, TEXT("creature.palestrider.scream"));
		for (TActorIterator<AWRCreature> It(Self->GetWorld()); It; ++It)
		{
			if (It->CreatureId == TEXT("palestrider") && !It->IsTamed() && It->IsAlive() && FVector::Dist(It->GetActorLocation(), Self->GetActorLocation()) < GcT(TEXT("screamRally"), 3000.f)) It->SetAggroTarget(Hit.Instigator);
		}
	}
	// A Gloamcrow hurt: its flock mobs together.
	if (Self->CreatureId == TEXT("gloamcrow") && Hit.Instigator)
	{
		for (TActorIterator<AWRCreature> It(Self->GetWorld()); It; ++It)
		{
			if (It->CreatureId == TEXT("gloamcrow") && !It->IsTamed() && It->IsAlive() && FVector::Dist(It->GetActorLocation(), Self->GetActorLocation()) < 3000.f) It->SetAggroTarget(Hit.Instigator);
		}
	}
	// A buried hound struck (a pick into a mound): up.
	if (Body->Has(WRGraveFlag::Buried)) Unbury(Self, Body);
}

void FWRGraveAI::Died(AWRCreature* Self, AActor* Killer)
{
	UWRGraveBody* Body = UWRGraveBody::Get(Self);
	if (!Body) return;
	if (Body->HeldBody.IsValid()) Release(Self, Body, false);
	if (UWRGraveBody::IsRisen(Self->CreatureId))
	{
		// It falls to a bone pile, no corpse (GY 2.12.4): bone 1-2, a fifth of them soulstone dust (the loot goes to the
		// killer in the death path; this is the dust and the look).
		Body->MulticastMoment(TEXT("grave.bone_collapse"), NAME_None, 1.2f, TEXT("creature.risen.collapse"));
		if (UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Killer); Inv && FMath::FRand() < GcT(TEXT("risenDust"), 0.2f)) Inv->AddItem(TEXT("soulstone_dust"), 1);
		if (USkeletalMeshComponent* Sk = Self->GetCreatureSkel()) Sk->SetVisibility(false, true);
		return;
	}
	if (UWRGraveLife* L = UWRGraveLife::Get(Self)) L->NoteDeath(Self, Killer);
}

float FWRGraveAI::StaminaRegenScale(const AActor* Who)
{
	const double* Until = Chilled().Find(Who);
	if (!Until) return 1.f;
	if (GcNow(Who) > *Until) { Chilled().Remove(Who); return 1.f; }
	return GcT(TEXT("chillRegen"), 0.6f);
}

// ================================================================================================ the living world

UWRGraveLife* UWRGraveLife::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UWRGraveLife>() : nullptr;
}

bool UWRGraveLife::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

bool UWRGraveLife::IsTickable() const
{
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() != NM_Client && WRGrave::Live() && FWRGraveData::Get().bLoaded;
}

void UWRGraveLife::NoteDeath(AWRCreature* Dead, AActor* Killer)
{
	if (!Dead || !WRGrave::Contains(Dead->GetActorLocation())) return;
	Funerals.Add({ Dead->GetActorLocation(), Killer, GcNow(Dead) + GcT(TEXT("funeralSeconds"), 20.f) });
	// The crows within 60 m come and fall silent on the stones round the body; then they mob what killed it.
	for (const TWeakObjectPtr<UWRGraveBody>& B : UWRGraveBody::Live)
	{
		UWRGraveBody* Body = B.Get();
		AWRCreature* C = Body ? Cast<AWRCreature>(Body->GetOwner()) : nullptr;
		if (!C || C->CreatureId != TEXT("gloamcrow") || C->IsTamed() || FVector::Dist(C->GetActorLocation(), Dead->GetActorLocation()) > GcT(TEXT("funeralReach"), 6000.f)) continue;
		Body->Set(WRGraveFlag::Silent, true);
		Body->SilentUntil = GcNow(C) + GcT(TEXT("funeralSeconds"), 20.f);
		Body->MobTarget = Killer;
		C->SetHome(Dead->GetActorLocation() + FVector(FMath::FRandRange(-500.f, 500.f), FMath::FRandRange(-500.f, 500.f), 0.f));
	}
}

void UWRGraveLife::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	if (!bDens)
	{
		// The catacombs' dens wait for the Graveyard's scenery (their rooms' floors) to be built.
		Clock += DeltaTime;
		if (Clock < GcT(TEXT("densAfter"), 20.f)) return;
		PlaceDens();
		bDens = true;
		Clock = 0.f;
	}
	Clock -= DeltaTime;
	if (Clock > 0.f) return;
	Clock = GcT(TEXT("beatEvery"), 6.f);
	const float Hour = AWRGameState::HourIn(World);
	BeatDusk(Hour);
	BeatMist();
	BeatMoths();
	BeatTombbacks();
	Funerals.RemoveAll([World](const FFuneral& F) { return World->GetTimeSeconds() > F.Until; });
}

void UWRGraveLife::PlaceDens()
{
	// The catacombs' tables (GY 2.12.3; KEPT underground rule: their night table 22:00-04:00): a den in each room, its
	// day kind and night kind rolled by the room's level (G5 = L1, L2, L3). Out only while a player is near.
	UWorld* World = GetWorld();
	const FWRGraveData& D = FWRGraveData::Get();
	const TSharedPtr<FJsonObject> Tbl = FWRTuning::Get().Object(TEXT("graveyardLife.catacombs"));
	if (!Tbl.IsValid()) return;
	int32 Made = 0;
	FRandomStream Rng(4177);
	const auto Roll = [&](const TSharedPtr<FJsonObject>& T) -> FName
	{
		if (!T.IsValid()) return NAME_None;
		float Total = 0.f;
		for (const auto& KV : T->Values) Total += float(KV.Value->AsNumber());
		float Pick = Rng.FRand() * Total;
		for (const auto& KV : T->Values) { Pick -= float(KV.Value->AsNumber()); if (Pick <= 0.f) return FName(*KV.Key); }
		return NAME_None;
	};
	for (const FWRGraveData::FRoom& R : D.Rooms)
	{
		if (R.bSpiral || !R.Arena.IsNone() || !R.Shrine.IsNone() || R.Kind == TEXT("chapel_crypt")) continue;
		const TSharedPtr<FJsonObject>* LvP = nullptr;
		if (!Tbl->TryGetObjectField(FString::Printf(TEXT("L%d"), R.Level), LvP)) continue;
		const TSharedPtr<FJsonObject>* DayP = nullptr;
		const TSharedPtr<FJsonObject>* NightP = nullptr;
		(*LvP)->TryGetObjectField(TEXT("day"), DayP);
		(*LvP)->TryGetObjectField(TEXT("night"), NightP);
		if (Rng.FRand() > GcNum(*LvP, TEXT("share"), 0.7f)) continue;
		const FName Day = Roll(DayP ? *DayP : nullptr), Night = Roll(NightP ? *NightP : nullptr);
		const auto Spawn = [&](FName Kind, uint8 Shift, const FVector& At)
		{
			const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(Kind);
			if (!Def) return;
			const FTransform T(At + FVector(0.f, 0.f, Def->HalfHeight + 20.f));
			AWRCreatureSpawner* Sp = World->SpawnActorDeferred<AWRCreatureSpawner>(AWRCreatureSpawner::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (!Sp) return;
			Sp->CreatureId = Kind;
			Sp->Count = Kind == TEXT("cryptstalker") ? (R.Level == 3 ? 2 : 1) : FMath::RandRange(Def->PackMin, FMath::Max(Def->PackMin, Def->PackMax));
			Sp->Radius = FMath::Clamp(FMath::Min(R.W, R.bRound ? R.W : R.L) * 0.3f, 100.f, 500.f);
			Sp->RespawnSeconds = GcT(TEXT("denRespawn"), 900.f);
			Sp->ActivationRadius = GcT(TEXT("denActivation"), 5000.f);
			Sp->bTraceGround = false;
			Sp->Shift = Shift;
			Sp->Tags.Add(TEXT("grave_den"));
			Sp->FinishSpawning(T);
			++Made;
		};
		if (!Day.IsNone() && Day == Night) Spawn(Day, 0, R.At);
		else
		{
			if (!Day.IsNone()) Spawn(Day, 1, R.At);
			if (!Night.IsNone()) Spawn(Night, 2, R.At + FRotator(0.f, R.Yaw, 0.f).RotateVector(FVector(FMath::Min(250.f, R.W * 0.25f), 0.f, 0.f)));
		}
	}
	UE_LOG(LogWildreach, Log, TEXT("GRAVEYARD catacombs: %d dens"), Made);
}

void UWRGraveLife::BeatDusk(float Hour)
{
	// Dusk (GY 2.12.3 beats): Gravehound packs run down Mournhares and Marrowgoats; Palestriders run the downs all night.
	if (Hour < GcT(TEXT("duskHour"), 18.f) || Hour > GcT(TEXT("duskHour"), 18.f) + 2.f) return;
	UWorld* World = GetWorld();
	for (TActorIterator<AWRCreature> It(World); It; ++It)
	{
		if (It->CreatureId != TEXT("gravehound") || It->IsTamed() || !It->IsAlive() || It->GetAggroTarget() || FMath::FRand() > 0.3f) continue;
		AWRCreature* Best = nullptr;
		float BestD = GcT(TEXT("duskHunt"), 6000.f);
		for (TActorIterator<AWRCreature> P(World); P; ++P)
		{
			if ((P->CreatureId != TEXT("mournhare") && P->CreatureId != TEXT("marrowgoat")) || P->IsTamed() || !P->IsAlive()) continue;
			const float Dd = float(FVector::Dist(P->GetActorLocation(), It->GetActorLocation()));
			if (Dd < BestD) { BestD = Dd; Best = *P; }
		}
		if (Best) It->SetAggroTarget(Best);
	}
}

void UWRGraveLife::BeatMist()
{
	// Corpse fog (the night form of the Ground mist, GY 2.9): Gravehounds rise out of any mist within 40 m of you in the
	// grave fields and the Bone Pits, instead of a den.
	UWorld* World = GetWorld();
	if (!GcNight(World) || World->GetTimeSeconds() < NextMistAt) return;
	const FWRWeatherKind* W = UWRWeatherSubsystem::ActiveKind(World, WRGrave::Biome());
	if (!W || (W->Id != TEXT("fog") && W->Id != TEXT("corpsefog"))) return;
	NextMistAt = World->GetTimeSeconds() + FMath::FRandRange(GcT(TEXT("mistEveryMin"), 90.f), GcT(TEXT("mistEveryMax"), 150.f));
	int32 Hounds = 0;
	for (TActorIterator<AWRCreature> It(World); It; ++It) if (It->CreatureId == TEXT("gravehound") && It->IsAlive()) ++Hounds;
	if (Hounds >= int32(GcT(TEXT("mistCap"), 24.f))) return;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		AWRCharacterBase* P = Cast<AWRCharacterBase>(It->Get() ? It->Get()->GetPawn() : nullptr);
		if (!P || !P->IsAlive()) continue;
		const TCHAR Z = WRGrave::ZoneAt(P->GetActorLocation());
		if (Z != TEXT('1') && Z != TEXT('3')) continue;
		const int32 N = FMath::RandRange(3, 4);
		const float A0 = FMath::FRandRange(0.f, 360.f);
		for (int32 i = 0; i < N; ++i)
		{
			const FVector Dir = FRotator(0.f, A0 + i * 25.f, 0.f).Vector();
			FVector At = P->GetActorLocation() + Dir * FMath::FRandRange(GcT(TEXT("mistMin"), 1500.f), GcT(TEXT("mistMax"), 4000.f));
			float Gz = 0.f;
			if (!WRLand::GroundUnder(At, Gz) || !WRGrave::Contains(At)) continue;
			At.Z = Gz + 90.f;
			if (AWRCreature* H = AWRCreature::SpawnCreature(World, TEXT("gravehound"), At, (P->GetActorLocation() - At).Rotation()))
			{
				TWeakObjectPtr<AWRCreature> WeakH(H);
				TWeakObjectPtr<AWRCharacterBase> WeakP(P);
				FTimerHandle Hd;
				World->GetTimerManager().SetTimer(Hd, [WeakH, WeakP]()
				{
					AWRCreature* Hh = WeakH.Get();
					UWRGraveBody* B = UWRGraveBody::Get(Hh);
					if (!Hh || !B) return;
					FWRGraveAI::Bury(Hh, B);
					FWRGraveAI::Unbury(Hh, B);
					if (WeakP.IsValid()) Hh->SetAggroTarget(WeakP.Get());
				}, 0.7f + 0.25f * i, false);
			}
		}
		UE_LOG(LogWildreach, Log, TEXT("GRAVEYARD corpse fog: %d Gravehounds rise round %s"), N, *P->GetName());
	}
}

void UWRGraveLife::BeatMoths()
{
	// At night Wraithmoths gather on the lanterns along the roads (they circle the tōrō; any torch that passes draws them).
	UWorld* World = GetWorld();
	if (!GcNight(World)) return;
	const FWRGraveData& D = FWRGraveData::Get();
	for (TActorIterator<AWRCreature> It(World); It; ++It)
	{
		if (It->CreatureId != TEXT("wraithmoth") || It->IsTamed() || It->GetAggroTarget() || !It->IsAlive()) continue;
		float Best = 3.0e7f;
		FVector Spot = FVector::ZeroVector;
		for (const FWRGraveData::FToro& T : D.Toro)
		{
			const float Dd = float(FVector::DistSquared(T.At, It->GetActorLocation()));
			if (Dd < Best) { Best = Dd; Spot = T.At; }
		}
		if (!Spot.IsZero()) It->SetHome(Spot + FVector(0.f, 0.f, 300.f));
	}
}

void UWRGraveLife::BeatTombbacks()
{
	// Tombbacks walk to fresh kills and bury them under their slab (GY 2.12.3): a wild one near a body goes to it and
	// sits on it a while (you find mounded slabs on the fields in the morning).
	UWorld* World = GetWorld();
	for (const FFuneral& F : Funerals)
	{
		for (TActorIterator<AWRCreature> It(World); It; ++It)
		{
			if (It->CreatureId != TEXT("tombback") || It->IsTamed() || It->GetAggroTarget() || !It->IsAlive()) continue;
			if (FVector::Dist(It->GetActorLocation(), F.At) > GcT(TEXT("buryReach"), 5000.f)) continue;
			It->SetHome(F.At);
			if (UWRGraveBody* B = UWRGraveBody::Get(*It); B && FVector::Dist2D(It->GetActorLocation(), F.At) < 250.f) B->Set(WRGraveFlag::Burying, true);
		}
	}
}
