// The Corrupted wasteland's creatures (see WRVoidCreatures.h). cooks/biome-void.md 2.12; numbers in tuning "voidCreatures"
// and in their rows (Data/attacks.json), the dens in Data/lands/wasteland.json "spawns".
#include "WRVoidCreatures.h"
#include "WRVoidBosses.h"
#include "WRVoid.h"
#include "WRVoidWorld.h"
#include "WRLand.h"
#include "WRCreature.h"
#include "WRBoss.h"
#include "WRAttackRows.h"
#include "WRAttributeSet.h"
#include "WRBuildLook.h"
#include "WRBuildLookKit.h"
#include "WRCharacter.h"
#include "WRCombatLibrary.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRInventory.h"
#include "WRPlayerController.h"
#include "WRProjectile.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRSurvival.h"
#include "WRTuning.h"
#include "Wildreach.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"

namespace WRVoidCreaturesLocal
{
	TSharedPtr<FJsonObject> VcRow(FName Id)
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
	float VcNum(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, float Fallback)
	{
		double V = 0.0;
		return O.IsValid() && O->TryGetNumberField(Field, V) ? float(V) : Fallback;
	}
	FLinearColor VcColor(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, const FLinearColor& Fallback)
	{
		FString S;
		if (O.IsValid() && O->TryGetStringField(Field, S)) return FLinearColor(FColor::FromHex(S));
		return Fallback;
	}
	FLinearColor VcHex(const TCHAR* S) { return FLinearColor(FColor::FromHex(S)); }
	TSharedPtr<FJsonObject> VcLook(FName Id)
	{
		const TSharedPtr<FJsonObject> R = VcRow(Id);
		const TSharedPtr<FJsonObject>* Look = nullptr;
		return R.IsValid() && R->TryGetObjectField(TEXT("look"), Look) ? *Look : nullptr;
	}
	FName VcKind(FName Id)
	{
		FString S;
		const TSharedPtr<FJsonObject> L = VcLook(Id);
		return L.IsValid() && L->TryGetStringField(TEXT("kind"), S) ? FName(*S) : Id;
	}
	/** tuning voidCreatures.<Key>. */
	float VcT(const TCHAR* Key, float Fallback) { return FWRTuning::Get().Num(FString::Printf(TEXT("voidCreatures.%s"), Key), Fallback); }
	/** The server's clock as every machine reads it (planned blinks land on it). */
	double VcServerNow(const UObject* W)
	{
		const UWorld* World = W ? W->GetWorld() : nullptr;
		const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
		return GS ? GS->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);
	}
	double VcNow(const UObject* W) { return W && W->GetWorld() ? W->GetWorld()->GetTimeSeconds() : 0.0; }
	bool VcWild(const AWRCreature* C) { return C && C->IsAlive() && !C->IsTamed(); }

	/** A row's own object (the numbers its motion reads beyond the schema: blink, trail, latch, scream). */
	TSharedPtr<FJsonObject> RowExtra(FName CreatureId, FName RowId)
	{
		static TSharedPtr<FJsonObject> Root;
		if (!Root.IsValid()) Root = FWRTuning::LoadDataFile(TEXT("attacks.json"));
		const TSharedPtr<FJsonObject>* Cs = nullptr;
		const TSharedPtr<FJsonObject>* C = nullptr;
		const TSharedPtr<FJsonObject>* R = nullptr;
		if (Root.IsValid() && Root->TryGetObjectField(TEXT("creatures"), Cs) && (*Cs)->TryGetObjectField(CreatureId.ToString(), C) && (*C)->TryGetObjectField(RowId.ToString(), R)) return *R;
		return nullptr;
	}
	TSharedPtr<FJsonObject> Sub(const TSharedPtr<FJsonObject>& O, const TCHAR* Field)
	{
		const TSharedPtr<FJsonObject>* S = nullptr;
		return O.IsValid() && O->TryGetObjectField(Field, S) ? *S : nullptr;
	}

	/** The nearest rift tear (large ones only if asked) within Max cm of a point, or null. */
	AWRRift* NearestRift(const UWorld* World, const FVector& At, float Max, bool bLargeOnly)
	{
		AWRRift* Best = nullptr;
		float BestD = Max;
		for (TActorIterator<AWRRift> It(World); It; ++It)
		{
			if (bLargeOnly && !It->bLarge) continue;
			const float D = FVector::Dist2D(It->GetActorLocation(), At);
			if (D < BestD && FMath::Abs(It->GetActorLocation().Z - At.Z) < 4000.f) { BestD = D; Best = *It; }
		}
		return Best;
	}

	/** The ground under a point: a live land's height function, else a trace, else the point. */
	FVector GroundUnder(const UWorld* World, const FVector& P, const AActor* Ignore)
	{
		const float Z = WRLand::GroundZAt(P, TNumericLimits<float>::Lowest());
		if (Z > TNumericLimits<float>::Lowest()) return FVector(P.X, P.Y, Z);
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRVoidGround), false, Ignore);
		if (World->LineTraceSingleByChannel(Hit, P + FVector(0, 0, 1500.f), P - FVector(0, 0, 4000.f), ECC_WorldStatic, Q)) return Hit.ImpactPoint;
		return P;
	}

	AWRCharacter* PlayerOf(AActor* A) { return Cast<AWRCharacter>(A); }

	enum EGroup : int32 { GrpStatic = 0, GrpJaw = 1, GrpEars = 2, GrpStrand = 3, GrpHoles = 4, GrpSeam = 5, GrpRows = 6, GrpEyes = 7 };
}
using namespace WRVoidCreaturesLocal;

// ================================================================================================ the body component

TArray<TWeakObjectPtr<UWRVoidBody>> UWRVoidBody::Live;

UWRVoidBody::UWRVoidBody()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.f;
	SetIsReplicatedByDefault(true);
}

void UWRVoidBody::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UWRVoidBody, Flags);
	DOREPLIFETIME(UWRVoidBody, Holding);
	DOREPLIFETIME(UWRVoidBody, BlinkTo);
	DOREPLIFETIME(UWRVoidBody, BlinkAt);
	DOREPLIFETIME(UWRVoidBody, Stacks);
}

UWRVoidBody* UWRVoidBody::Get(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UWRVoidBody>() : nullptr;
}

bool UWRVoidBody::IsVoidKind(FName CreatureId)
{
	const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(CreatureId);
	return Def && Def->Biome == TEXT("wasteland") && Def->Archetype != TEXT("boss");
}

void UWRVoidBody::Attach(AWRCreature* Creature)
{
	if (!Creature || !Creature->HasAuthority() || !IsVoidKind(Creature->CreatureId) || Get(Creature) || Creature->IsA<AWRBoss>())
	{
		return;
	}
	UWRVoidBody* Body = NewObject<UWRVoidBody>(Creature, TEXT("VoidBody"));
	Body->RegisterComponent();
	Creature->AddInstanceComponent(Body);
}

void UWRVoidBody::AttachLook(AWRCreature* Boss)
{
	if (!Boss || !Boss->HasAuthority() || Get(Boss)) return;
	UWRVoidBody* Body = NewObject<UWRVoidBody>(Boss, TEXT("VoidBody"));
	Body->RegisterComponent();
	Boss->AddInstanceComponent(Body);
}

void UWRVoidBody::BeginPlay()
{
	Super::BeginPlay();
	Live.RemoveAll([](const TWeakObjectPtr<UWRVoidBody>& B) { return !B.IsValid(); });
	Live.Add(this);
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	if (!C || !C->HasAuthority()) return;
	const double T = VcNow(this);
	NextListenAt = T + FMath::FRandRange(0.f, 0.5f);
	NextRiftbornBlinkAt = T + VcT(TEXT("riftborn.blinkCooldown"), 10.f);
	// Riftborn (biome-void 2.12.5): the den's share (its spawner's "riftborn:<pct>" tag) or forced ("riftborn:100").
	float Share = VcT(TEXT("riftborn.elsewhere"), 5.f);
	if (const AWRCreatureSpawner* Sp = C->Spawner.Get())
	{
		for (const FName& Tag : Sp->Tags)
		{
			const FString S = Tag.ToString();
			if (S.StartsWith(TEXT("riftborn:"))) Share = FCString::Atof(*S.Mid(9));
		}
	}
	if (!C->IsTamed() && FMath::FRand() * 100.f < Share)
	{
		Set(WRVoidFlag::Riftborn, true);
		// One tier up: highland numbers x the riftborn health multiplier.
		if (UWRAttributeSet* A = C->GetAttributes())
		{
			const float K = VcT(TEXT("riftborn.healthMult"), 1.25f);
			if (UAbilitySystemComponent* ASC = C->GetAbilitySystemComponent())
			{
				ASC->SetNumericAttributeBase(UWRAttributeSet::GetMaxHealthAttribute(), A->GetMaxHealth() * K);
				ASC->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), A->GetMaxHealth());
			}
		}
		C->Weapon.FlatDamage *= VcT(TEXT("riftborn.damageMult"), 1.2f);
	}
	// A Riftmaw lies in the large rift it was found by.
	if (C->CreatureId == TEXT("riftmaw"))
	{
		if (AWRRift* R = NearestRift(GetWorld(), C->GetActorLocation(), VcT(TEXT("riftmaw.homeSearch"), 6000.f), true))
		{
			HomeRift = R;
			FTimerHandle H;
			TWeakObjectPtr<AWRCreature> WeakC(C);
			TWeakObjectPtr<UWRVoidBody> WeakB(this);
			TWeakObjectPtr<AWRRift> WeakR(R);
			// After the spawn settles (its spawner set it on the ground): into the tear.
			GetWorld()->GetTimerManager().SetTimer(H, [WeakC, WeakB, WeakR]()
			{
				if (WeakC.IsValid() && WeakB.IsValid() && WeakR.IsValid() && WeakC->IsAlive() && !WeakC->IsTamed()) FWRVoidAI::IntoRift(WeakC.Get(), WeakB.Get(), WeakR.Get());
			}, 0.5f, false);
		}
	}
}

void UWRVoidBody::EndPlay(const EEndPlayReason::Type Reason)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		AWRCreature* C = Cast<AWRCreature>(GetOwner());
		if (C && HeldBody.IsValid()) FWRVoidAI::Release(C, this, false);
		if (C && LatchedOn.IsValid()) FWRVoidAI::Unlatch(C, this, false);
	}
	ClearLook();
	Live.RemoveAll([this](const TWeakObjectPtr<UWRVoidBody>& B) { return !B.IsValid() || B.Get() == this; });
	Super::EndPlay(Reason);
}

USkeletalMeshComponent* UWRVoidBody::Skel() const
{
	const AWRCreature* C = Cast<AWRCreature>(GetOwner());
	return C ? C->GetCreatureSkel() : nullptr;
}

void UWRVoidBody::MulticastMoment_Implementation(FName Effect, FName Bone, float Scale, FName Sound)
{
	const AWRCreature* C = Cast<AWRCreature>(GetOwner());
	if (!C || GetNetMode() == NM_DedicatedServer) return;
	const FName B = C->FindBone(Bone);
	const FVector At = B.IsNone() || !Skel() ? C->GetActorLocation() : Skel()->GetBoneLocation(B);
	if (!Effect.IsNone()) WRFx::SpawnAt(C, Effect, At, C->GetActorRotation(), Scale);
	if (!Sound.IsNone()) WRSound::PlayAt(C, Sound, At);
}

void UWRVoidBody::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	Clock += DeltaTime;
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	if (!C) return;
	// Every machine: a latched Gnawer doesn't push the body it rides.
	const bool bRiding = Has(WRVoidFlag::Latched);
	if (bRiding != bPawnIgnored && C->GetCapsuleComponent())
	{
		bPawnIgnored = bRiding;
		C->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, bRiding ? ECR_Ignore : ECR_Block);
	}
	if (C->HasAuthority()) FWRVoidAI::TickBody(C, this, DeltaTime);
	if (GetNetMode() != NM_DedicatedServer) TickLook(DeltaTime);
}

// ================================================================================================ the look

USceneComponent* UWRVoidBody::AddGroup(FName Bone, const FVector& WorldAt, const FRotator& WorldRot, const TArray<FWRLookBlock>& Blocks, int32 Kind)
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
	G.BaseLoc = Frame->GetRelativeLocation();
	G.BaseScale = Frame->GetRelativeScale3D();
	G.Phase = FMath::FRand() * 6.28f;
	Groups.Add(G);
	return Frame;
}

void UWRVoidBody::SetGroupGlow(int32 Kind, float GlowValue)
{
	for (FGroup& G : Groups)
	{
		if (G.Kind != Kind || !G.Frame.IsValid() || FMath::IsNearlyEqual(G.Glow, GlowValue, 0.02f)) continue;
		G.Glow = GlowValue;
		TArray<USceneComponent*> Children;
		G.Frame->GetChildrenComponents(false, Children);
		for (USceneComponent* Ch : Children)
		{
			if (UInstancedStaticMeshComponent* ISM = Cast<UInstancedStaticMeshComponent>(Ch); ISM && ISM->NumCustomDataFloats >= 10)
			{
				for (int32 i = 0; i < ISM->GetInstanceCount(); ++i) ISM->SetCustomDataValue(i, 8, GlowValue, i == ISM->GetInstanceCount() - 1);
			}
		}
	}
}

void UWRVoidBody::ClearLook()
{
	for (USceneComponent* Comp : LookComps) if (Comp) Comp->DestroyComponent();
	LookComps.Reset();
	Groups.Reset();
	if (Glow) { Glow->DestroyComponent(); Glow = nullptr; }
	if (RippleFx) { WRFx::Stop(RippleFx); RippleFx = nullptr; }
	LookFor = NAME_None;
}

void UWRVoidBody::BuildLook()
{
	using namespace WRLookKit;
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	const FWRCreatureDef* Def = C ? C->GetDef() : nullptr;
	USkeletalMeshComponent* Body = Skel();
	if (!Def || !Body || !Body->GetSkeletalMeshAsset() || !Body->IsVisible()) return;
	ClearLook();
	LookFor = C->CreatureId;
	bLookRiftborn = IsRiftborn();
	const TSharedPtr<FJsonObject> Look = VcLook(C->CreatureId);

	// The cook's palette over the rig's own texture (M_CreatureTint); a Riftborn's albedo desaturated 70% and darkened.
	TintMats.Reset();
	const TArray<TSharedPtr<FJsonValue>>* Tints = nullptr;
	UMaterialInterface* Master = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Creatures/M_CreatureTint.M_CreatureTint"));
	if (Look.IsValid() && Master && Look->TryGetArrayField(TEXT("tint"), Tints))
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
			if (!Pick.IsValid())
			{
				// A slot the palette doesn't name (the rig's extra parts): the first entry's colours.
				Pick = (*Tints)[0]->AsObject();
			}
			if (!Pick.IsValid()) continue;
			UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Master, C);
			FString Path;
			const auto Tex = [&](const TCHAR* Field, const TCHAR* Param, const TCHAR* Fallback)
			{
				const FString P = Pick->TryGetStringField(Field, Path) ? Path : FString(Fallback);
				if (UTexture* T = P.IsEmpty() ? nullptr : LoadObject<UTexture>(nullptr, *P)) M->SetTextureParameterValue(Param, T);
			};
			Tex(TEXT("tex"), TEXT("BaseColorTex"), TEXT(""));
			Tex(TEXT("normal"), TEXT("NormalTex"), TEXT("/Engine/EngineMaterials/DefaultNormal.DefaultNormal"));
			Tex(TEXT("glowTex"), TEXT("GlowTex"), TEXT("/Engine/EngineResources/Black.Black"));
			const float Desat = VcNum(Pick, TEXT("desat"), 1.f);
			M->SetScalarParameterValue(TEXT("Desaturation"), bLookRiftborn ? FMath::Max(Desat, VcT(TEXT("riftborn.desat"), 0.7f)) : Desat);
			M->SetScalarParameterValue(TEXT("Brightness"), VcNum(Pick, TEXT("bright"), 1.f) * (bLookRiftborn ? VcT(TEXT("riftborn.bright"), 0.8f) : 1.f));
			M->SetScalarParameterValue(TEXT("TwoTone"), VcNum(Pick, TEXT("twoTone"), 0.f));
			M->SetScalarParameterValue(TEXT("LightFrom"), VcNum(Pick, TEXT("lightFrom"), 0.f));
			M->SetScalarParameterValue(TEXT("Roughness"), VcNum(Pick, TEXT("rough"), 0.75f));
			M->SetScalarParameterValue(TEXT("GlowStrength"), VcNum(Pick, TEXT("glowStrength"), 0.f));
			M->SetVectorParameterValue(TEXT("Tint"), VcColor(Pick, TEXT("tint"), FLinearColor::White));
			M->SetVectorParameterValue(TEXT("TintLight"), VcColor(Pick, TEXT("light"), VcColor(Pick, TEXT("tint"), FLinearColor::White)));
			M->SetVectorParameterValue(TEXT("GlowColor"), VcColor(Pick, TEXT("glow"), VcHex(TEXT("#7B4FD6"))));
			Body->SetMaterial(Slot, M);
			TintMats.Add(M);
		}
	}

	// The parts, at its size, from its own bones.
	const float Rad = Def->Radius;
	const FRotator Yaw(0.f, C->GetActorRotation().Yaw, 0.f);
	const FVector Fwd = Yaw.Vector(), Right = FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y);
	const FTransform W = FTransform::Identity;
	const FName K = VcKind(C->CreatureId);
	const FBox AssetBox = Body->GetSkeletalMeshAsset()->GetBounds().GetBox();
	const FVector BS = Body->GetRelativeScale3D();
	const float BodyH = FMath::Max(10.f, (AssetBox.Max.Z - AssetBox.Min.Z) * BS.Z);
	const float H = BodyH * 0.5f;
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
	const float Snout = FMath::Max(Rad * 0.4f, float(FVector::Dist2D(Head, Tip)));
	const FSurf Glass = FSurf::Glaze(VcHex(TEXT("#0B0A10")), 0.1f);
	const FSurf VoidLit = FSurf::Lit(VcHex(TEXT("#2B1840")), 3.f);
	const FSurf EdgeLit = FSurf::Lit(VcHex(TEXT("#7B4FD6")), 2.5f);

	// A helper body drawn wholly in code (a brood rift, the Void heart): its rig stays hidden.
	bool bHideMesh = false;
	if (Look.IsValid()) Look->TryGetBoolField(TEXT("hideMesh"), bHideMesh);
	if (bHideMesh) Body->SetVisibility(false, false);
	if (K == TEXT("brood_rift"))
	{
		// A small tear standing on its point in the pen: a lens of black-violet with a lit violet rim, grit drifting in.
		AddGroup(NAME_None, C->GetActorLocation(), Yaw, Blocks([&](FKit& Kit)
		{
			for (int32 i = -6; i <= 6; ++i)
			{
				const float T = i / 6.f;
				const float W = 70.f * FMath::Sqrt(FMath::Max(0.f, 1.f - T * T)) + 2.f;
				Kit.Add(FSurf::Lit(VcHex(TEXT("#1A0E26")), 0.6f), FVector(6.f, W, 26.f), FVector(0.f, 0.f, T * 150.f));
				Kit.Add(EdgeLit, FVector(4.f, 3.f, 26.f), FVector(0.f, W * 0.5f + 1.f, T * 150.f));
				Kit.Add(EdgeLit, FVector(4.f, 3.f, 26.f), FVector(0.f, -W * 0.5f - 1.f, T * 150.f));
			}
		}), GrpRows);
	}
	else if (K == TEXT("void_heart"))
	{
		// The Void heart on its cords: a black glossy mass the size of a cart, violet in its folds, cords up into the dark.
		AddGroup(NAME_None, C->GetActorLocation(), Yaw, Blocks([&](FKit& Kit)
		{
			for (int32 i = 0; i < 9; ++i)
			{
				const float A = i * 0.7f;
				Kit.Add(Glass, FVector(140.f - 8.f * i, 120.f - 6.f * i, 150.f - 9.f * i), FVector(FMath::Cos(A) * 30.f, FMath::Sin(A) * 30.f, 10.f * (i % 3)), FRotator(10.f * i, 40.f * i, 0.f), EWRLookMesh::Cylinder);
			}
			for (int32 f = 0; f < 6; ++f) Kit.Add(EdgeLit, FVector(4.f, 4.f, 120.f), FVector(FMath::Cos(f * 1.05f) * 70.f, FMath::Sin(f * 1.05f) * 70.f, 0.f), FRotator(0.f, 0.f, 15.f));
			for (int32 c = 0; c < 4; ++c) Kit.Rod(FSurf::Stone(VcHex(TEXT("#3A3438")), 0.8f), FVector(FMath::Cos(c * 1.57f) * 60.f, FMath::Sin(c * 1.57f) * 60.f, 90.f), FVector(FMath::Cos(c * 1.57f) * 300.f, FMath::Sin(c * 1.57f) * 300.f, 1300.f), 8.f);
		}), GrpRows);
		Glow = NewObject<UPointLightComponent>(C);
		Glow->SetupAttachment(Body);
		Glow->SetIntensity(3000.f);
		Glow->SetAttenuationRadius(1600.f);
		Glow->SetLightColor(VcHex(TEXT("#7B4FD6")));
		Glow->RegisterComponent();
	}
	else if (K == TEXT("unravelled"))
	{
		// The fist-sized voidglass heart in the open ribs (it hangs alone, lit, while the pieces are apart), the 3 m iron
		// tuning-staff with two tines, the open singing mouth.
		FVector Chest = C->GetActorLocation() + FVector(0, 0, H * 0.4f);
		const FName ChestBone = BoneAt(TEXT("spine2"), Chest);
		AddGroup(ChestBone, Chest + Fwd * Rad * 0.3f, Yaw, Blocks([&](FKit& Kit)
		{
			Kit.Add(FSurf::Lit(VcHex(TEXT("#2B1840")), 4.f), FVector(22.f), FVector::ZeroVector, FRotator(35.f, 20.f, 10.f));
			Kit.Add(EdgeLit, FVector(26.f, 4.f, 4.f), FVector::ZeroVector, FRotator(0.f, 45.f, 0.f));
		}), GrpEyes);
		FVector Hand = C->GetActorLocation();
		const FName HandBone = BoneAt(TEXT("righthand"), Hand);
		if (!HandBone.IsNone())
		{
			AddGroup(HandBone, Hand, Yaw, Blocks([&](FKit& Kit)
			{
				const FSurf IronS = FSurf::Steel(VcHex(TEXT("#2A2826")), 0.6f);
				Kit.Rod(IronS, FVector(0.f, 0.f, -120.f), FVector(0.f, 0.f, 180.f), 8.f);
				Kit.Rod(IronS, FVector(0.f, -20.f, 180.f), FVector(0.f, -20.f, 250.f), 5.f);
				Kit.Rod(IronS, FVector(0.f, 20.f, 180.f), FVector(0.f, 20.f, 250.f), 5.f);
				Kit.Rod(IronS, FVector(0.f, -20.f, 180.f), FVector(0.f, 20.f, 180.f), 6.f);
			}), GrpStatic);
		}
	}
	else if (K == TEXT("null_warden"))
	{
		// The closed helm shaped like a bell, its open bell-mouth below (the crit goes through it); the 2 m black bronze bell
		// on its chain in his left hand.
		AddGroup(HeadBone, Head + FVector(0, 0, H * 0.06f), Yaw, Blocks([&](FKit& Kit)
		{
			const FSurf Bronze = FSurf::Steel(VcHex(TEXT("#1E1712")), 0.45f);
			Kit.Add(Bronze, FVector(H * 0.3f, H * 0.3f, H * 0.32f), FVector(0.f, 0.f, H * 0.04f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			Kit.Add(Bronze, FVector(H * 0.36f, H * 0.36f, H * 0.06f), FVector(0.f, 0.f, -H * 0.13f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			Kit.Add(EdgeLit, FVector(H * 0.02f, H * 0.22f, H * 0.02f), FVector(H * 0.15f, 0.f, 0.f));
		}), GrpStatic);
		FVector LHand = C->GetActorLocation();
		const FName LBone = BoneAt(TEXT("lefthand"), LHand);
		if (!LBone.IsNone())
		{
			AddGroup(LBone, LHand, Yaw, Blocks([&](FKit& Kit)
			{
				const FSurf Bronze = FSurf::Steel(VcHex(TEXT("#1E1712")), 0.45f);
				const FSurf IronS = FSurf::Steel(VcHex(TEXT("#2A2826")), 0.6f);
				Kit.Rod(IronS, FVector::ZeroVector, FVector(0.f, 0.f, -90.f), 4.f);
				Kit.Add(Bronze, FVector(160.f, 160.f, 180.f), FVector(0.f, 0.f, -190.f), FRotator::ZeroRotator, EWRLookMesh::Cone);
				Kit.Add(Bronze, FVector(170.f, 170.f, 20.f), FVector(0.f, 0.f, -280.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			}), GrpStatic);
		}
	}
	else if (K == TEXT("starving_one"))
	{
		// The jaw unhinged down to its chest: a long gaping lower jaw of grey bone and a row of worn teeth.
		AddGroup(HeadBone, Head, Yaw, Blocks([&](FKit& Kit)
		{
			const FSurf BoneS = FSurf::Stone(VcHex(TEXT("#B8B0A0")), 0.7f);
			Kit.Add(BoneS, FVector(Snout * 0.9f, H * 0.18f, H * 0.04f), FVector(Snout * 0.3f, 0.f, -H * 0.28f), FRotator(-35.f, 0.f, 0.f));
			for (int32 t = 0; t < 8; ++t) Kit.Spike(BoneS, FVector(Snout * (0.05f + 0.08f * t), (t % 2 ? 1.f : -1.f) * H * 0.06f, -H * 0.2f - t * H * 0.03f), FVector(0.f, 0.f, 1.f), H * 0.02f, H * 0.05f);
		}), GrpJaw);
	}
	else if (K == TEXT("echo"))
	{
		// The 4 m tail of mirror-black plumes raised like a lyre: what you see at night, catching the rifts' light.
		FVector Hip = C->GetActorLocation();
		const FName HipBone = BoneAt(TEXT("pelvis"), Hip).IsNone() ? BoneAt(TEXT("hips"), Hip) : C->FindBone(TEXT("pelvis"));
		AddGroup(HipBone, Hip - Fwd * Rad * 0.6f, Yaw, Blocks([&](FKit& Kit)
		{
			const FSurf Plume = FSurf::Glaze(VcHex(TEXT("#0A0A0E")), 0.05f);
			for (int32 p = 0; p < 16; ++p)
			{
				const float A = FMath::DegreesToRadians(-70.f + 140.f * p / 15.f);
				const FVector Dir(-FMath::Cos(A) * 0.4f, FMath::Sin(A), FMath::Cos(A));
				Kit.Rod(Plume, FVector::ZeroVector, Dir.GetSafeNormal() * 400.f, 6.f);
				Kit.Add(Plume, FVector(30.f, 4.f, 90.f), Dir.GetSafeNormal() * 360.f, Dir.GetSafeNormal().Rotation() + FRotator(-90.f, 0.f, 0.f));
				Kit.Add(EdgeLit, FVector(2.f, 2.f, 40.f), Dir.GetSafeNormal() * 395.f, Dir.GetSafeNormal().Rotation() + FRotator(-90.f, 0.f, 0.f));
			}
		}), GrpHoles);
	}
	else if (K == TEXT("vorhollow"))
	{
		// The 4 m hole straight through its chest showing a lensed starfield, its rib ends polished like glass; voidglass
		// spines down the back; the 1.8 m voidglass hooks on its long middle fingers.
		FVector Chest = C->GetActorLocation() + FVector(0, 0, H * 0.3f);
		const FName ChestBone = BoneAt(TEXT("spine2"), Chest);
		AddGroup(ChestBone, Chest, Yaw, Blocks([&](FKit& Kit)
		{
			Kit.Disc(FSurf::Lit(VcHex(TEXT("#08060C")), 0.f), FVector::ZeroVector, FVector(1.f, 0.f, 0.f), 380.f, 20.f);
			for (int32 s = 0; s < 40; ++s) Kit.Disc(FSurf::Lit(VcHex(TEXT("#E8E4FF")), 8.f), FVector(12.f, FMath::Sin(s * 2.4f) * 160.f, FMath::Cos(s * 1.7f) * 160.f), FVector(1.f, 0.f, 0.f), 4.f + (s % 3), 1.f);
			for (int32 r = 0; r < 10; ++r)
			{
				const float A = 2.f * PI * r / 10.f;
				Kit.Spike(Glass, FVector(0.f, FMath::Cos(A) * 200.f, FMath::Sin(A) * 200.f), FVector(0.3f, -FMath::Cos(A), -FMath::Sin(A)).GetSafeNormal(), 20.f, 60.f);
			}
			Kit.Disc(EdgeLit, FVector(-4.f, 0.f, 0.f), FVector(1.f, 0.f, 0.f), 420.f, 6.f);
		}), GrpEyes);
		for (const TPair<FName, FVector>& Sp : BonesWith(TEXT("spine")))
		{
			AddGroup(Sp.Key, Sp.Value + FVector(0, 0, H * 0.25f), Yaw, Blocks([&](FKit& Kit)
			{
				Kit.Spike(Glass, FVector::ZeroVector, FVector(-0.3f, 0.f, 1.f).GetSafeNormal(), H * 0.05f, H * 0.18f);
				Kit.Spike(EdgeLit, FVector(1.f, 0.f, 0.f), FVector(-0.3f, 0.f, 1.f).GetSafeNormal(), H * 0.012f, H * 0.16f);
			}), GrpStatic);
		}
		for (const TCHAR* Hand : { TEXT("l-hand"), TEXT("r-hand") })
		{
			FVector At = C->GetActorLocation();
			const FName B = BoneAt(Hand, At);
			if (B.IsNone()) continue;
			AddGroup(B, At, Yaw, Blocks([&](FKit& Kit)
			{
				FVector P(0.f, 0.f, 0.f);
				for (int32 s = 0; s < 5; ++s)
				{
					const float A = FMath::DegreesToRadians(20.f * s);
					const FVector Q = P + FVector(FMath::Cos(A) * 36.f, 0.f, -FMath::Sin(A) * 36.f);
					Kit.Rod(Glass, P, Q, 18.f - 3.f * s);
					P = Q;
				}
				Kit.Spike(EdgeLit, P, FVector(0.3f, 0.f, -1.f).GetSafeNormal(), 4.f, 30.f);
			}), GrpStatic);
		}
	}
	else if (K == TEXT("hollowfang"))
	{
		// The eyeless skull: two deep black-violet pits where eyes would be; black glass teeth past the lips, upper and
		// lower, the canines longest; an ash ridge of vertebrae standing proud down the spine.
		AddGroup(HeadBone, Head, Yaw, Blocks([&](FKit& Kit)
		{
			for (const float Side : { -1.f, 1.f })
			{
				Kit.Disc(VoidLit, FVector(Snout * 0.35f, Side * H * 0.12f, H * 0.08f), FVector(0.35f, Side, 0.25f), H * 0.09f, 2.f);
				Kit.Disc(FSurf::Stone(VcHex(TEXT("#1A181B")), 0.9f), FVector(Snout * 0.33f, Side * H * 0.115f, H * 0.08f), FVector(0.35f, Side, 0.25f), H * 0.13f, 1.f);
				for (int32 t = 0; t < 7; ++t)
				{
					const float X = Snout * (0.45f + 0.075f * t);
					const float Len = (t == 1 ? 0.13f : 0.07f) * H;
					Kit.Spike(Glass, FVector(X, Side * H * (0.09f - 0.006f * t), -H * 0.04f), FVector(0.1f, Side * 0.15f, -1.f).GetSafeNormal(), H * 0.025f, Len);
					Kit.Spike(Glass, FVector(X, Side * H * (0.085f - 0.006f * t), -H * 0.11f), FVector(0.1f, Side * 0.15f, 1.f).GetSafeNormal(), H * 0.022f, Len * 0.8f);
				}
			}
		}), GrpEyes);
		const TArray<TPair<FName, FVector>> Spine = BonesWith(TEXT("spine"));
		for (const TPair<FName, FVector>& Sp : Spine)
		{
			AddGroup(Sp.Key, Sp.Value + FVector(0, 0, H * 0.32f), Yaw, Blocks([&](FKit& Kit)
			{
				Kit.Add(FSurf::Stone(VcHex(TEXT("#6E6A66")), 0.8f), FVector(H * 0.08f, H * 0.05f, H * 0.07f), FVector::ZeroVector, FRotator(12.f, 0.f, 0.f));
			}), GrpStatic);
		}
	}
	else if (K == TEXT("riftmaw"))
	{
		// The lamprey mouth: a ring of hooked black-glass teeth round the snout's end, inside four bone jaw-plates that
		// fold shut over it like petals and flare open for the grab (GrpJaw turns them).
		const FVector Mouth = Tip - Fwd * Snout * 0.1f;
		const float R0 = H * 0.42f;
		AddGroup(HeadBone, Mouth, Yaw, Blocks([&](FKit& Kit)
		{
			for (int32 t = 0; t < 14; ++t)
			{
				const float A = 2.f * PI * t / 14.f;
				const FVector At(0.f, FMath::Cos(A) * R0 * 0.7f, FMath::Sin(A) * R0 * 0.7f);
				Kit.Spike(Glass, At, FVector(0.35f, -FMath::Cos(A), -FMath::Sin(A)).GetSafeNormal(), R0 * 0.09f, R0 * 0.4f);
			}
			Kit.Disc(VoidLit, FVector(-R0 * 0.2f, 0.f, 0.f), FVector(1.f, 0.f, 0.f), R0 * 1.1f, 2.f);
		}), GrpStatic);
		for (int32 p = 0; p < 4; ++p)
		{
			const float A = 90.f * p + 45.f;
			const FRotator PetalRot = Yaw + FRotator(0.f, 0.f, A);
			AddGroup(HeadBone, Mouth, PetalRot, Blocks([&](FKit& Kit)
			{
				const FSurf BoneS = FSurf::Stone(VcHex(TEXT("#D9CFB8")), 0.6f);
				Kit.Add(BoneS, FVector(R0 * 1.25f, R0 * 0.85f, R0 * 0.12f), FVector(R0 * 0.35f, 0.f, R0 * 0.62f), FRotator(-25.f, 0.f, 0.f));
				for (int32 t = 0; t < 4; ++t)
				{
					Kit.Spike(Glass, FVector(R0 * (0.7f + 0.1f * t), -R0 * 0.3f + R0 * 0.2f * t, R0 * 0.52f), FVector(0.4f, 0.f, -1.f).GetSafeNormal(), R0 * 0.07f, R0 * 0.26f);
				}
			}), GrpJaw);
		}
	}
	else if (K == TEXT("nullwing"))
	{
		// Torn membranes: on each wing bone, ragged holes with the void showing through (black discs, violet edges that
		// flare as it banks to lay its trail); a toothed beak.
		for (const TPair<FName, FVector>& Wb : BonesWith(TEXT("wing")))
		{
			AddGroup(Wb.Key, Wb.Value, Yaw, Blocks([&](FKit& Kit)
			{
				Kit.Disc(EdgeLit, FVector(0.f, 0.f, 0.f), FVector(0.f, 0.f, 1.f), H * 0.2f, 1.2f);
				Kit.Disc(FSurf::Lit(VcHex(TEXT("#0B0A10")), 0.f), FVector(0.f, 0.f, 0.6f), FVector(0.f, 0.f, 1.f), H * 0.16f, 1.2f);
			}), GrpHoles);
		}
		AddGroup(HeadBone, Head, Yaw, Blocks([&](FKit& Kit)
		{
			for (int32 t = 0; t < 6; ++t) for (const float Side : { -1.f, 1.f })
				Kit.Spike(Glass, FVector(Snout * (0.5f + 0.08f * t), Side * H * 0.04f, -H * 0.05f), FVector(0.f, 0.f, -1.f), H * 0.015f, H * 0.05f);
		}), GrpStatic);
	}
	else if (K == TEXT("gnawer"))
	{
		// The two long orange-yellow chisel incisors outside the lips, upper pair over lower pair, and two tiny dark eyes.
		AddGroup(HeadBone, Tip, Yaw, Blocks([&](FKit& Kit)
		{
			const FSurf Tooth = FSurf::Glaze(VcHex(TEXT("#C8A040")), 0.4f);
			for (const float Side : { -1.f, 1.f })
			{
				Kit.Add(Tooth, FVector(H * 0.32f, H * 0.07f, H * 0.04f), FVector(H * 0.08f, Side * H * 0.045f, H * 0.02f), FRotator(-35.f, 0.f, 0.f));
				Kit.Add(Tooth, FVector(H * 0.26f, H * 0.06f, H * 0.035f), FVector(H * 0.06f, Side * H * 0.04f, -H * 0.1f), FRotator(30.f, 0.f, 0.f));
				Kit.Disc(FSurf::Glaze(VcHex(TEXT("#120F10")), 0.1f), FVector(-Snout * 0.5f, Side * H * 0.22f, H * 0.18f), FVector(0.2f, Side, 0.2f), H * 0.05f, 0.6f);
			}
		}), GrpStatic);
	}
	else if (K == TEXT("hush"))
	{
		// Two huge ear fans either side of the bald eyeless head, veined pink-grey (they fold flat until it rises), and the
		// long tapping finger on its right hand: three joints to a black nail, a forearm's length past the others.
		const FSurf Ear = FSurf::Cloth(VcHex(TEXT("#C8B0B0")));
		const FSurf Vein = FSurf::Stone(VcHex(TEXT("#8A6E70")), 0.7f);
		for (const float Side : { -1.f, 1.f })
		{
			AddGroup(HeadBone, Head + Right * Side * H * 0.12f + FVector(0, 0, H * 0.06f), Yaw + FRotator(0.f, Side * 80.f, 0.f), Blocks([&](FKit& Kit)
			{
				Kit.Disc(Ear, FVector(0.f, 0.f, H * 0.18f), FVector(1.f, 0.f, 0.f), H * 0.42f, 1.5f);
				for (int32 v = 0; v < 5; ++v)
				{
					const float A = FMath::DegreesToRadians(-60.f + 30.f * v);
					Kit.Rod(Vein, FVector(1.f, 0.f, H * 0.02f), FVector(1.f, FMath::Sin(A) * H * 0.19f, H * 0.18f + FMath::Cos(A) * H * 0.19f), 1.2f);
				}
			}), GrpEars);
		}
		FVector Hand = C->GetActorLocation();
		const FName HandBone = BoneAt(TEXT("righthand"), Hand).IsNone() ? BoneAt(TEXT("hand_r"), Hand) : C->FindBone(TEXT("righthand"));
		if (!HandBone.IsNone())
		{
			AddGroup(HandBone, Hand, Yaw, Blocks([&](FKit& Kit)
			{
				const FSurf Skin = FSurf::Stone(VcHex(TEXT("#B4AFA8")), 0.7f);
				const FSurf Knuckle = FSurf::Stone(VcHex(TEXT("#7E7872")), 0.7f);
				FVector P(0.f, 0.f, 0.f);
				const float Seg[3] = { 0.3f, 0.26f, 0.2f };
				FVector Dir = FVector(0.95f, 0.f, -0.3f).GetSafeNormal();
				for (int32 s = 0; s < 3; ++s)
				{
					const FVector Q = P + Dir * Seg[s] * H;
					Kit.Rod(Skin, P, Q, H * 0.03f);
					Kit.Add(Knuckle, FVector(H * 0.045f), Q);
					P = Q;
					Dir = (Dir + FVector(0.f, 0.f, -0.25f)).GetSafeNormal();
				}
				Kit.Spike(FSurf::Glaze(VcHex(TEXT("#0B0A10")), 0.2f), P, Dir, H * 0.03f, H * 0.08f);
			}), GrpStatic);
		}
	}
	else if (K == TEXT("voidstrider"))
	{
		// The mane and tail: fine black filaments that drift up as if underwater, violet at the tips (each strand its own
		// frame, turned slowly by TickLook so the mane floats and never snaps).
		const TArray<TPair<FName, FVector>> Neck = BonesWith(TEXT("neck"));
		const TArray<TPair<FName, FVector>> Tail = BonesWith(TEXT("tail"));
		const auto Strand = [&](const FName Bone, const FVector& At, float Len, float Lean)
		{
			AddGroup(Bone, At, Yaw + FRotator(0.f, 0.f, Lean), Blocks([&](FKit& Kit)
			{
				FVector P(0.f, 0.f, 0.f);
				for (int32 s = 0; s < 4; ++s)
				{
					const FVector Q = P + FVector(-Len * 0.12f, 0.f, Len * 0.25f);
					Kit.Rod(FSurf::Glaze(VcHex(TEXT("#0E0C12")), 0.3f), P, Q, 1.1f - 0.2f * s);
					P = Q;
				}
				Kit.Spike(FSurf::Lit(VcHex(TEXT("#7B4FD6")), 2.f), P, FVector(-0.3f, 0.f, 1.f).GetSafeNormal(), 1.2f, Len * 0.12f);
			}), GrpStrand);
		};
		for (const TPair<FName, FVector>& N : Neck)
		{
			for (int32 s = 0; s < 4; ++s) Strand(N.Key, N.Value + FVector(0, 0, H * 0.1f) + Right * (s - 1.5f) * 2.5f, H * (0.35f + 0.06f * s), (s - 1.5f) * 10.f);
		}
		for (const TPair<FName, FVector>& T : Tail)
		{
			for (int32 s = 0; s < 3; ++s) Strand(T.Key, T.Value + Right * (s - 1.f) * 2.f, H * 0.5f, (s - 1.f) * 12.f);
		}
	}
	else if (K == TEXT("riftback"))
	{
		// The lens seam down the dome: a black-violet slit you see too far into, star points inside it; a rim of voidglass
		// edging the shell plates.
		FVector Back = C->GetActorLocation() + FVector(0, 0, H * 0.9f);
		const FName SpineBone = BoneAt(TEXT("spine"), Back);
		AddGroup(SpineBone, Back + FVector(0, 0, H * 0.55f), Yaw, Blocks([&](FKit& Kit)
		{
			for (int32 i = -4; i <= 4; ++i)
			{
				const float T = i / 4.f;
				const float Wd = H * 0.12f * FMath::Sqrt(1.f - T * T) + 1.f;
				Kit.Add(VoidLit, FVector(Rad * 0.2f, Wd, 3.f), FVector(T * Rad * 0.8f, 0.f, -FMath::Abs(T) * H * 0.15f));
			}
			for (int32 s = 0; s < 12; ++s)
			{
				Kit.Disc(FSurf::Lit(VcHex(TEXT("#E8E4FF")), 6.f), FVector((s / 11.f - 0.5f) * Rad * 1.4f, (s % 3 - 1) * H * 0.03f, 2.5f), FVector::UpVector, 2.f + (s % 2), 0.5f);
			}
		}), GrpSeam);
		AddGroup(SpineBone, Back, Yaw, Blocks([&](FKit& Kit)
		{
			for (int32 s = 0; s < 14; ++s)
			{
				const float A = 2.f * PI * s / 14.f;
				Kit.Spike(Glass, FVector(FMath::Cos(A) * Rad * 0.92f, FMath::Sin(A) * Rad * 0.62f, -H * 0.35f), FVector(FMath::Cos(A), FMath::Sin(A), 0.4f).GetSafeNormal(), H * 0.05f, H * 0.12f);
			}
		}), GrpStatic);
	}
	else if (K == TEXT("driftmote"))
	{
		// The comb-jelly light rows rippling down its sides, a dark seed at its heart, trailing filaments; a soft light.
		AddGroup(NAME_None, C->GetActorLocation(), Yaw, Blocks([&](FKit& Kit)
		{
			for (int32 r = 0; r < 8; ++r)
			{
				const float A = 2.f * PI * r / 8.f;
				Kit.Add(FSurf::Lit(VcHex(TEXT("#B8A8F0")), 3.f), FVector(1.6f, 1.6f, H * 0.9f), FVector(FMath::Cos(A) * H * 0.75f, FMath::Sin(A) * H * 0.75f, 0.f));
			}
			Kit.Add(FSurf::Glaze(VcHex(TEXT("#08070A")), 0.2f), FVector(H * 0.22f), FVector::ZeroVector, FRotator(20.f, 30.f, 10.f));
		}), GrpRows);
		for (int32 f = 0; f < 10; ++f)
		{
			const float A = 2.f * PI * f / 10.f;
			AddGroup(NAME_None, C->GetActorLocation() + FVector(FMath::Cos(A) * H * 0.5f, FMath::Sin(A) * H * 0.5f, -H * 0.7f), Yaw, Blocks([&](FKit& Kit)
			{
				FVector P(0.f, 0.f, 0.f);
				for (int32 s = 0; s < 4; ++s) { const FVector Q = P + FVector(0.f, 0.f, -40.f - 6.f * s); Kit.Rod(FSurf::Lit(VcHex(TEXT("#8E80C0")), 0.8f), P, Q, 0.5f); P = Q; }
			}), GrpStrand);
		}
		Glow = NewObject<UPointLightComponent>(C);
		Glow->SetupAttachment(Body);
		Glow->SetIntensity(400.f);
		Glow->SetAttenuationRadius(600.f);
		Glow->SetLightColor(VcHex(TEXT("#B8A8F0")));
		Glow->SetCastShadows(false);
		Glow->RegisterComponent();
	}
	else if (K == TEXT("withergoat"))
	{
		// Four black horns (a Jacob sheep's: two up and back, two curling down at the sides), long drooping ears, gold eyes.
		AddGroup(HeadBone, Head + FVector(0, 0, H * 0.2f), Yaw, Blocks([&](FKit& Kit)
		{
			const FSurf Horn = FSurf::Stone(VcHex(TEXT("#141214")), 0.5f);
			for (const float Side : { -1.f, 1.f })
			{
				FVector P(0.f, Side * H * 0.08f, 0.f);
				for (int32 s = 0; s < 4; ++s) { const FVector Q = P + FVector(-H * 0.08f, Side * H * 0.02f, H * (0.14f - 0.03f * s)); Kit.Rod(Horn, P, Q, H * (0.05f - 0.008f * s)); P = Q; }
				P = FVector(0.f, Side * H * 0.12f, -H * 0.02f);
				for (int32 s = 0; s < 5; ++s)
				{
					const float A = FMath::DegreesToRadians(60.f * s);
					const FVector Q = FVector(-H * 0.06f * FMath::Sin(A), Side * (H * 0.14f + H * 0.06f * (1.f - FMath::Cos(A))), -H * 0.08f * FMath::Sin(A * 0.5f) - H * 0.02f);
					Kit.Rod(Horn, P, Q, H * (0.045f - 0.006f * s));
					P = Q;
				}
				Kit.Add(FSurf::Cloth(VcHex(TEXT("#8E8A82"))), FVector(H * 0.07f, H * 0.03f, H * 0.32f), FVector(-H * 0.02f, Side * H * 0.16f, -H * 0.22f), FRotator(0.f, 0.f, Side * 12.f));
				Kit.Disc(FSurf::Lit(VcHex(TEXT("#C8A040")), 1.4f), FVector(Snout * 0.35f, Side * H * 0.1f, -H * 0.02f), FVector(0.3f, Side, 0.f), H * 0.05f, 0.8f);
			}
		}), GrpStatic);
	}

	// Riftborn: eye sockets solid black-violet and voidglass shards grown out along the spine (biome-void 2.12.5).
	if (bLookRiftborn)
	{
		AddGroup(HeadBone, Head, Yaw, Blocks([&](FKit& Kit)
		{
			for (const float Side : { -1.f, 1.f }) Kit.Disc(FSurf::Lit(VcHex(TEXT("#2B1840")), 5.f), FVector(Snout * 0.4f, Side * H * 0.12f, H * 0.1f), FVector(0.3f, Side, 0.2f), H * 0.08f, 1.5f);
		}), GrpEyes);
		int32 n = 0;
		for (const TPair<FName, FVector>& Sp : BonesWith(TEXT("spine")))
		{
			if (n++ > 4) break;
			AddGroup(Sp.Key, Sp.Value + FVector(0, 0, H * 0.3f), Yaw, Blocks([&](FKit& Kit)
			{
				Kit.Spike(Glass, FVector::ZeroVector, FVector(-0.3f, 0.f, 1.f).GetSafeNormal(), H * 0.05f, H * (0.14f + 0.04f * (n % 2)));
				Kit.Spike(EdgeLit, FVector(1.f, 0.f, 0.f), FVector(-0.3f, 0.f, 1.f).GetSafeNormal(), H * 0.012f, H * 0.12f);
			}), GrpStatic);
		}
	}
}

void UWRVoidBody::TickLook(float DeltaTime)
{
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	USkeletalMeshComponent* Body = Skel();
	if (!C || !Body) return;
	if ((LookFor != C->CreatureId || bLookRiftborn != IsRiftborn()) && Body->GetSkeletalMeshAsset() && LookTries < 40)
	{
		++LookTries;
		BuildLook();
	}
	if (!bMeshBaseSaved && Body->GetSkeletalMeshAsset())
	{
		bMeshBaseSaved = true;
		MeshBase = Body->GetRelativeLocation();
		MeshScale0 = Body->GetRelativeScale3D();
	}
	const double SNow = VcServerNow(this);
	RiseAlpha = FMath::FInterpTo(RiseAlpha, Has(WRVoidFlag::Risen) || Has(WRVoidFlag::Screaming) ? 1.f : 0.f, DeltaTime, 6.f);
	GapeAlpha = FMath::FInterpTo(GapeAlpha, Has(WRVoidFlag::Gape) || Has(WRVoidFlag::Holding) ? 1.f : 0.f, DeltaTime, 9.f);
	const float Scream = Has(WRVoidFlag::Screaming) ? 1.f + 0.06f * FMath::Sin(Clock * 40.f) : 1.f;
	for (FGroup& G : Groups)
	{
		USceneComponent* F = G.Frame.Get();
		if (!F) continue;
		if (G.Kind == GrpJaw)
		{
			// The petals flare open from the tooth ring (the grab's tell, and while it holds).
			F->SetRelativeRotation((FQuat(G.BaseRot) * FQuat(FRotator(-55.f * GapeAlpha, 0.f, 0.f))).Rotator());
		}
		else if (G.Kind == GrpEars)
		{
			// Folded flat against the skull until it rises; spread wide (with a leathery snap) when it listens.
			F->SetRelativeScale3D(G.BaseScale * FMath::Lerp(0.35f, 1.f, RiseAlpha) * Scream);
		}
		else if (G.Kind == GrpStrand)
		{
			// Drifting as if underwater: each strand turns on its own slow wave (never set from its last frame).
			const float T = Clock * 0.7f + G.Phase;
			F->SetRelativeRotation((FQuat(G.BaseRot) * FQuat(FRotator(9.f * FMath::Sin(T), 0.f, 7.f * FMath::Sin(T * 1.37f + 1.f)))).Rotator());
		}
	}
	if (VcKind(C->CreatureId) == TEXT("nullwing")) SetGroupGlow(GrpHoles, Has(WRVoidFlag::Trail) ? 6.f : 1.2f);
	if (C->CreatureId == TEXT("starving_one") && bMeshBaseSaved)
	{
		const float Want = 1.f + WRTune(TEXT("bosses.starving_one.sizePer"), 0.1f) * Stacks;
		Hover = FMath::FInterpTo(Hover <= 0.f ? 1.f : Hover, Want, DeltaTime, 2.f);
		Body->SetRelativeScale3D(FVector(Hover) * (MeshScale0.IsZero() ? FVector(1.f) : MeshScale0));
	}
	if (C->CreatureId == TEXT("unravelled"))
	{
		// Apart: the rig gone to a whirl of dust; the core alone lit (Gape) as it reforms.
		const bool bApart = Has(WRVoidFlag::Drifting);
		if (Body->IsVisible() == bApart) Body->SetVisibility(!bApart, false);
		SetGroupGlow(GrpEyes, Has(WRVoidFlag::Gape) ? 12.f : 3.f);
	}
	// The Driftmote floats on the voidglass grit in its bell (the floating rule): its body hovers and bobs; drifting off it
	// rises high. Its light breathes; a tame one pulses toward the nearest rift (0.5 Hz at 30 m, 3 Hz at 3 m).
	if (VcKind(C->CreatureId) == TEXT("driftmote") && bMeshBaseSaved)
	{
		const float Want = VcNum(VcLook(C->CreatureId), TEXT("hover"), 190.f) + (Has(WRVoidFlag::Drifting) ? VcT(TEXT("driftmote.driftRise"), 1000.f) : 0.f);
		Hover = FMath::FInterpTo(Hover, Want, DeltaTime, 1.2f);
		Body->SetRelativeLocation(MeshBase + FVector(0, 0, Hover + 25.f * FMath::Sin(Clock * 0.9f)));
		for (FGroup& G : Groups) if (G.Frame.IsValid() && G.Frame->GetAttachSocketName().IsNone() && G.Frame->GetAttachParent() == Body) {}
		float Hz = 0.3f;
		if (C->IsTamed())
		{
			float Best = VcT(TEXT("driftmote.senseRange"), 3000.f);
			bool bAny = false;
			for (TActorIterator<AWRRift> It(GetWorld()); It; ++It)
			{
				const float D = FVector::Dist(It->GetActorLocation(), C->GetActorLocation());
				if (D < Best) { Best = D; bAny = true; }
			}
			if (bAny) Hz = FMath::GetMappedRangeValueClamped(FVector2D(300.f, 3000.f), FVector2D(3.f, 0.5f), Best);
		}
		PulseClock += DeltaTime * Hz;
		const float Pulse = 0.5f + 0.5f * FMath::Sin(PulseClock * 2.f * PI);
		if (Glow) Glow->SetIntensity(250.f + 900.f * Pulse);
		SetGroupGlow(GrpRows, 1.5f + 3.f * Pulse);
	}
	else if (Has(WRVoidFlag::Shudder) && bMeshBaseSaved)
	{
		// The blink's tell: the whole body shudders.
		Body->SetRelativeLocation(MeshBase + FVector(FMath::FRandRange(-2.5f, 2.5f), FMath::FRandRange(-2.5f, 2.5f), FMath::FRandRange(-1.f, 1.f)));
	}
	else if (bMeshBaseSaved && !MeshBase.Equals(Body->GetRelativeLocation(), 0.1f) && VcKind(C->CreatureId) != TEXT("driftmote"))
	{
		Body->SetRelativeLocation(MeshBase);
	}
	// The ripple where a blink will land: shown from the plan until it lands (lead >= its row's tell).
	if (BlinkAt > SNow && RippleShownFor != BlinkAt)
	{
		RippleShownFor = BlinkAt;
		WRFx::SpawnFor(C, TEXT("void.blink.ripple"), BlinkTo, FRotator::ZeroRotator, 1.f, float(BlinkAt - SNow));
	}
}

// ================================================================================================ the AI

bool FWRVoidAI::Grounded(const AWRCreature* Self)
{
	const UWRVoidBody* B = UWRVoidBody::Get(Self);
	return B && B->Has(WRVoidFlag::Grounded);
}

float FWRVoidAI::NoiseReach(const AWRCharacterBase* Who)
{
	const AWRCharacter* P = Cast<AWRCharacter>(Who);
	if (!P || !P->IsAlive() || !P->GetPlayerState()) return 0.f;
	// How far you're heard (cm): still or creeping (under the creep speed: the crouch stand-in) silent, walking near,
	// running far, a swing / cast / roll loud; the ground under you (litter, glass) and the air (Dead calm) carry it.
	const float Speed = P->GetVelocity().Size2D();
	float Reach = Speed < VcT(TEXT("hush.creepSpeed"), 260.f) ? 0.f : Speed < 560.f ? VcT(TEXT("hush.walkReach"), 450.f) : VcT(TEXT("hush.runReach"), 1200.f);
	if (P->HasTag(WRTags::State_Attacking) || P->HasTag(WRTags::State_Casting)) Reach = FMath::Max(Reach, VcT(TEXT("hush.actionReach"), 1300.f));
	if (P->HasTag(WRTags::State_Dodging)) Reach = FMath::Max(Reach, VcT(TEXT("hush.rollReach"), 900.f));
	if (Reach <= 0.f) return 0.f;
	const FName Zone = WRLand::ZoneAt(P->GetActorLocation());
	if (Zone == TEXT("V1")) Reach *= VcT(TEXT("hush.litterMult"), 1.3f);
	float Share = 1.f;
	if (const UWorld* W = P->GetWorld())
	{
		for (TActorIterator<AWRRift> It(W); It; ++It)
		{
			const float D = FVector::Dist2D(It->GetActorLocation(), P->GetActorLocation());
			if (D > (It->bLarge ? 400.f : 200.f) && D < (It->bLarge ? 950.f : 550.f)) { Reach *= VcT(TEXT("hush.glassMult"), 1.5f); break; }
		}
	}
	float Strength = 0.f;
	const FWRWeatherKind* K = UWRWeatherSubsystem::ActiveKind(P, TEXT("wasteland"), &Strength);
	if (K && K->bSilence) Reach *= FMath::Lerp(1.f, VcT(TEXT("hush.deadCalmMult"), 1.5f), Strength);
	(void)Share;
	return Reach;
}

void FWRVoidAI::PlanBlink(AWRCreature* Self, UWRVoidBody* Body, AWRCharacterBase* Target, float Lead, bool bBehind, FName Then)
{
	if (!Self || !Body || !Target) return;
	const TSharedPtr<FJsonObject> B = Sub(RowExtra(Self->CreatureId, TEXT("blink")), TEXT("blink"));
	const float MinD = VcNum(B, TEXT("min"), 400.f), MaxD = VcNum(B, TEXT("max"), 600.f);
	const float F0 = VcNum(B, TEXT("flankMin"), 90.f), F1 = VcNum(B, TEXT("flankMax"), 150.f);
	const float Facing = Target->GetController() ? Target->GetControlRotation().Yaw : Target->GetActorRotation().Yaw;
	float Ang = bBehind ? 180.f : FMath::FRandRange(F0, F1) * (FMath::RandBool() ? 1.f : -1.f);
	const float Dist = bBehind ? VcT(TEXT("blink.behind"), 220.f) + Self->GetSimpleCollisionRadius() : FMath::FRandRange(MinD, MaxD);
	FVector Dest = Target->GetActorLocation() + FRotator(0.f, Facing + Ang, 0.f).Vector() * Dist;
	Dest = GroundUnder(Self->GetWorld(), Dest, Self) + FVector(0, 0, Self->GetSimpleCollisionHalfHeight() + 5.f);
	// Never into a hole or a wall: a sweep from the target to the spot; short of anything in the way.
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(WRVoidBlink), false, Self);
	Q.AddIgnoredActor(Target);
	if (Self->GetWorld()->SweepSingleByChannel(Hit, Target->GetActorLocation() + FVector(0, 0, 40.f), Dest + FVector(0, 0, 40.f), FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeSphere(Self->GetSimpleCollisionRadius() * 0.8f), Q))
	{
		Dest = Hit.Location - FVector(0, 0, 40.f);
	}
	if (WRLand::InHole(Dest)) return;
	Body->BlinkTo = Dest;
	Body->BlinkAt = VcServerNow(Self) + Lead;
	Body->BlinkFrom = VcNow(Self);
	Body->bBlinkPending = true;
	Body->BlinkThen = Then;
	Body->Set(WRVoidFlag::Shudder, true);
	// The tell: one hard reversed breath, a shudder through the body.
	Body->MulticastMoment(TEXT("void.blink.shudder"), TEXT("spine"), 1.f, TEXT("void.blink.breath"));
}

void FWRVoidAI::LandBlink(AWRCreature* Self, UWRVoidBody* Body)
{
	Body->bBlinkPending = false;
	Body->Set(WRVoidFlag::Shudder, false);
	const FVector From = Self->GetActorLocation();
	const FVector To = Body->BlinkTo;
	// Its outline smears from where it was to where it lands (both ends), a sharp inrush.
	WRFx::Broadcast(Self, TEXT("void.blink.smear"), From, (To - From).Rotation(), 1.f, TEXT("void.blink.snap"));
	Self->SetActorLocation(To, false, nullptr, ETeleportType::TeleportPhysics);
	WRFx::Broadcast(Self, TEXT("void.blink.smear"), To, (To - From).Rotation(), 1.f, NAME_None);
	if (const AActor* T = Self->GetAggroTarget()) Self->SetActorRotation(FRotator(0.f, (T->GetActorLocation() - To).Rotation().Yaw, 0.f));
	UE_LOG(LogWildreach, Log, TEXT("VOID %s blinks %.0f cm"), *Self->GetName(), FVector::Dist(From, To));
}

void FWRVoidAI::StartHold(AWRCreature* Self, UWRVoidBody* Body, AWRCharacterBase* Victim, FName Kind, float Seconds)
{
	if (!Self || !Body || !Victim || !Victim->IsAlive()) return;
	Body->HeldBody = Victim;
	Body->Holding = Victim;
	Body->HoldKind = Kind;
	Body->HoldUntil = VcNow(Self) + Seconds;
	Body->Set(WRVoidFlag::Holding, true);
	// Pinned where it caught you (no fight over your position): held for the catch, then thrown clear.
	if (Victim->Status) Victim->Status->Stagger(Seconds + 0.1f);
	Self->SetActorRotation(FRotator(0.f, (Victim->GetActorLocation() - Self->GetActorLocation()).Rotation().Yaw, 0.f));
	Body->MulticastMoment(TEXT("hit.void"), TEXT("head"), 1.4f, FName(*FString::Printf(TEXT("creature.%s.signature"), *Self->CreatureId.ToString())));
	UE_LOG(LogWildreach, Log, TEXT("VOID %s holds %s (%s, %.1f s)"), *Self->GetName(), *Victim->GetName(), *Kind.ToString(), Seconds);
}

void FWRVoidAI::Release(AWRCreature* Self, UWRVoidBody* Body, bool bThrow)
{
	AWRCharacterBase* V = Body->HeldBody.Get();
	Body->HeldBody = nullptr;
	Body->Holding = nullptr;
	Body->Set(WRVoidFlag::Holding, false);
	Body->Set(WRVoidFlag::Screaming, false);
	if (V && bThrow && V->IsAlive())
	{
		const FVector Push = (V->GetActorLocation() - Self->GetActorLocation()).GetSafeNormal2D() * VcT(TEXT("hold.throw"), 500.f) + FVector(0, 0, 220.f);
		V->LaunchCharacter(Push, true, true);
		if (V->Status) V->Status->Stagger(VcT(TEXT("hold.throwStagger"), 0.6f));
	}
}

bool FWRVoidAI::StartLatch(AWRCreature* Self, UWRVoidBody* Body, AWRCharacterBase* Victim)
{
	if (!Victim || !Victim->IsAlive() || LatchCount(Victim) >= int32(VcT(TEXT("gnawer.maxLatched"), 6.f))) return false;
	const int32 Slot = LatchCount(Victim);
	const float A = 2.f * PI * (Slot + 0.3f) / 6.f;
	Body->LatchedOn = Victim;
	Body->Holding = Victim;
	Body->LatchedAt = VcNow(Self);
	Body->LatchTick = 0.f;
	Body->LatchOffset = FVector(FMath::Cos(A) * (Victim->GetSimpleCollisionRadius() + 6.f), FMath::Sin(A) * (Victim->GetSimpleCollisionRadius() + 6.f), -Victim->GetSimpleCollisionHalfHeight() * (0.2f + 0.15f * (Slot % 3)));
	Body->Set(WRVoidFlag::Latched, true);
	Self->GetCharacterMovement()->DisableMovement();
	Self->SetActorEnableCollision(false);
	Self->AttachToActor(Victim, FAttachmentTransformRules(EAttachmentRule::KeepWorld, false));
	Self->SetActorRelativeLocation(Body->LatchOffset);
	Self->SetActorRelativeRotation(FRotator(0.f, FMath::RadiansToDegrees(A) + 180.f, 0.f));
	Body->MulticastMoment(TEXT("hit.blood"), TEXT("head"), 0.6f, TEXT("void.gnawer.latch"));
	return true;
}

void FWRVoidAI::Unlatch(AWRCreature* Self, UWRVoidBody* Body, bool bThrown)
{
	AWRCharacterBase* V = Body->LatchedOn.Get();
	Body->LatchedOn = nullptr;
	Body->Holding = nullptr;
	Body->Set(WRVoidFlag::Latched, false);
	Self->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Self->SetActorRotation(FRotator(0.f, Self->GetActorRotation().Yaw, 0.f));
	Self->SetActorEnableCollision(true);
	Self->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	if (bThrown && V)
	{
		const FVector Out = (Self->GetActorLocation() - V->GetActorLocation()).GetSafeNormal2D();
		Self->LaunchCharacter(Out * 450.f + FVector(0, 0, 260.f), true, true);
		if (Self->Status) Self->Status->Stagger(VcT(TEXT("gnawer.shakenStagger"), 1.0f));
	}
}

int32 FWRVoidAI::LatchCount(const AActor* Victim)
{
	int32 N = 0;
	for (const TWeakObjectPtr<UWRVoidBody>& B : UWRVoidBody::Live) N += B.IsValid() && Victim && B->LatchedOn.Get() == Victim;
	return N;
}

int32 FWRVoidAI::ShakeOff(AActor* Victim)
{
	int32 Off = 0;
	TArray<UWRVoidBody*> On;
	for (const TWeakObjectPtr<UWRVoidBody>& B : UWRVoidBody::Live) if (B.IsValid() && B->LatchedOn.Get() == Victim) On.Add(B.Get());
	for (UWRVoidBody* B : On)
	{
		if (AWRCreature* G = Cast<AWRCreature>(B->GetOwner())) { Unlatch(G, B, true); ++Off; }
	}
	return Off;
}

void FWRVoidAI::IntoRift(AWRCreature* Self, UWRVoidBody* Body, AWRRift* Rift)
{
	if (!Self || !Body || !Rift) return;
	Body->HomeRift = Rift;
	Body->Set(WRVoidFlag::InRift, true);
	Body->AttacksOut = 0;
	WRFx::Broadcast(Self, TEXT("void.rift.plunge"), Self->GetActorLocation(), FRotator::ZeroRotator, 1.4f, TEXT("void.rift.plunge"));
	Self->SetActorHiddenInGame(true);
	Self->SetActorEnableCollision(false);
	Self->GetCharacterMovement()->DisableMovement();
	Self->SetActorLocation(Rift->GetActorLocation() + FVector(0, 0, Self->GetSimpleCollisionHalfHeight()), false, nullptr, ETeleportType::TeleportPhysics);
}

void FWRVoidAI::OutOfRift(AWRCreature* Self, UWRVoidBody* Body, const FVector& At)
{
	Body->Set(WRVoidFlag::InRift, false);
	Self->SetActorLocation(At, false, nullptr, ETeleportType::TeleportPhysics);
	Self->SetActorHiddenInGame(false);
	Self->SetActorEnableCollision(true);
	Self->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}

void FWRVoidAI::LayTrail(AWRCreature* Self, const FWRAttackRow& Row, AWRCharacterBase* Target)
{
	const TSharedPtr<FJsonObject> T = Sub(RowExtra(Self->CreatureId, Row.Id), TEXT("trail"));
	if (!Target) return;
	// Across your route: ahead of where you're going (your velocity, else your facing), square to it, never within 3 m.
	FVector Route = Target->GetVelocity().GetSafeNormal2D();
	if (Route.IsNearlyZero()) Route = Target->GetActorForwardVector().GetSafeNormal2D();
	const float Clear = VcNum(T, TEXT("clear"), 300.f);
	const float Len = VcNum(T, TEXT("length"), 3000.f);
	const FVector Mid = Target->GetActorLocation() + Route * (Clear + VcT(TEXT("nullwing.trailAhead"), 400.f));
	const FVector Across = FVector(-Route.Y, Route.X, 0.f);
	const TArray<TSharedPtr<FJsonValue>>* Hh = nullptr;
	float H0 = 100.f, H1 = 400.f;
	if (T.IsValid() && T->TryGetArrayField(TEXT("height"), Hh) && Hh->Num() >= 2) { H0 = float((*Hh)[0]->AsNumber()); H1 = float((*Hh)[1]->AsNumber()); }
	const float Hgt = FMath::FRandRange(H0, H1);
	const FVector A = GroundUnder(Self->GetWorld(), Mid - Across * Len * 0.5f, Self) + FVector(0, 0, Hgt);
	const FVector B = GroundUnder(Self->GetWorld(), Mid + Across * Len * 0.5f, Self) + FVector(0, 0, Hgt);
	const FTransform X((B - A).Rotation(), (A + B) * 0.5f);
	if (AWRVoidTrail* Tr = Self->GetWorld()->SpawnActorDeferred<AWRVoidTrail>(AWRVoidTrail::StaticClass(), X, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		Tr->A = A;
		Tr->B = B;
		Tr->Thick = VcNum(T, TEXT("thick"), 100.f);
		Tr->Seconds = VcNum(T, TEXT("seconds"), 6.f);
		Tr->Born = VcServerNow(Self);
		Tr->Layer = Self;
		Tr->TickDamage = VcNum(T, TEXT("tickDamage"), 8.f);
		Tr->TickEvery = VcNum(T, TEXT("tick"), 0.5f);
		Tr->CursePerSecond = VcNum(T, TEXT("curse"), 30.f);
		Tr->SetLifeSpan(Tr->Seconds + 0.5f);
		Tr->FinishSpawning(X);
	}
}

void FWRVoidAI::Scream(AWRCreature* Self, const FWRAttackRow& Row)
{
	const TSharedPtr<FJsonObject> S = Sub(RowExtra(Self->CreatureId, Row.Id), TEXT("scream"));
	const float Radius = Row.Radius > 0.f ? Row.Radius : 1200.f;
	WRFx::Broadcast(Self, TEXT("void.hush.scream"), Self->GetActorLocation() + FVector(0, 0, Self->GetSimpleCollisionHalfHeight() * 0.6f), FRotator::ZeroRotator, Radius / 600.f, TEXT("void.hush.scream"));
	for (TActorIterator<AWRCharacter> It(Self->GetWorld()); It; ++It)
	{
		if (!It->IsAlive() || !Self->IsHostileTo(*It) || FVector::Dist(It->GetActorLocation(), Self->GetActorLocation()) > Radius) continue;
		const bool bGuard = It->HasTag(WRTags::State_Blocking);
		const float Drain = VcNum(S, TEXT("stamina"), 50.f) * (bGuard ? VcNum(S, TEXT("guardMult"), 0.5f) : 1.f);
		if (UAbilitySystemComponent* ASC = It->GetAbilitySystemComponent())
		{
			if (const UWRAttributeSet* A = It->GetAttributes()) ASC->SetNumericAttributeBase(UWRAttributeSet::GetStaminaAttribute(), FMath::Max(0.f, A->GetStamina() - Drain));
			// A drink or a cast under way is broken off (its cost is already paid).
			if (It->HasTag(WRTags::State_Drinking) || It->HasTag(WRTags::State_Casting))
			{
				ASC->CancelAllAbilities();
				if (AWRPlayerController* PC = Cast<AWRPlayerController>(It->GetController())) PC->ClientNotify(TEXT("The scream breaks your focus"));
			}
		}
	}
}

bool FWRVoidAI::RowAllowed(const AWRCreatureAI* AI, const AWRCreature* Self, const FWRAttackRow& Row, const AWRCharacterBase* Target, bool bReading)
{
	const UWRVoidBody* B = UWRVoidBody::Get(Self);
	if (!B) return true;
	// Forced only (the retreat, the rift burst: TickWild starts them).
	if (Row.Motion == TEXT("retreat") || Row.Id == TEXT("rift_burst")) return false;
	if (B->Has(WRVoidFlag::InRift) || B->Has(WRVoidFlag::Latched) || B->bBlinkPending) return false;
	// The Hush listens before it hunts: the rise only when it doesn't hear you.
	if (Row.Motion == TEXT("rise")) return Target && FVector::Dist(Target->GetActorLocation(), Self->GetActorLocation()) > FWRVoidAI::NoiseReach(Target);
	return true;
}

void FWRVoidAI::RowStarted(AWRCreatureAI* AI, AWRCreature* Self, const FWRAttackRow& Row, AWRCharacterBase* Target)
{
	UWRVoidBody* B = UWRVoidBody::Get(Self);
	if (!B) return;
	if (Row.Motion == TEXT("blink"))
	{
		// The tell and the ripple, the whole wind-up long (>= 0.5 s): it lands as the strike comes.
		PlanBlink(Self, B, Target, Row.Windup + Row.Hold, false, NAME_None);
	}
	if (Row.Id == TEXT("blink_kick"))
	{
		// A lensing ripple behind you for 0.5 s: it blinks there, then the rear kick plays its own wind-up.
		const TSharedPtr<FJsonObject> X = Sub(RowExtra(Self->CreatureId, Row.Id), TEXT("blinkFirst"));
		PlanBlink(Self, B, Target, VcNum(X, TEXT("tell"), 0.5f), true, NAME_None);
	}
	if (Row.bGrab) { B->Set(WRVoidFlag::Gape, true); B->GapeUntil = VcNow(Self) + Row.Windup + Row.Hold + Row.Active + 0.2f; }
	if (Row.Motion == TEXT("trail")) { B->Set(WRVoidFlag::Trail, true); B->TrailUntil = VcNow(Self) + Row.Windup + Row.Active; }
	if (Row.Motion == TEXT("scream")) { B->Set(WRVoidFlag::Screaming, true); B->ScreamUntil = VcNow(Self) + Row.Windup + Row.Active; }
	if (Row.Motion == TEXT("rise"))
	{
		B->Set(WRVoidFlag::Risen, true);
		B->RisenUntil = VcNow(Self) + Row.Windup + Row.Active;
		B->MulticastMoment(NAME_None, TEXT("head"), 1.f, TEXT("void.hush.ears"));
	}
	if (Self->CreatureId == TEXT("riftmaw") && Row.Motion != TEXT("retreat") && Row.Id != TEXT("rift_burst")) ++B->AttacksOut;
}

bool FWRVoidAI::StrikeRow(AWRCreatureAI* AI, AWRCreature* Self, const FWRAttackRow& Row, AWRCharacterBase* Target)
{
	UWRVoidBody* B = UWRVoidBody::Get(Self);
	if (!B) return false;
	const FName M = Row.Motion;
	if (M == TEXT("blink"))
	{
		if (B->bBlinkPending) LandBlink(Self, B);
		return true;
	}
	if (M == TEXT("latch"))
	{
		if (Target && FVector::Dist(Target->GetActorLocation(), Self->GetActorLocation()) < Self->GetSimpleCollisionRadius() + Target->GetSimpleCollisionRadius() + Row.Reach + 30.f
			&& Self->IsHostileTo(Target) && !Target->HasTag(WRTags::State_Dodging))
		{
			StartLatch(Self, B, Target);
		}
		return true;
	}
	if (M == TEXT("trail")) { LayTrail(Self, Row, Target); return true; }
	if (Row.Id == TEXT("dive") && Self->CreatureId == TEXT("nullwing"))
	{
		// After its dive it's down on the ground 1.5 s: the bring-down window (the lunge itself plays as usual).
		B->Set(WRVoidFlag::Grounded, true);
		B->GroundedUntil = VcNow(Self) + Row.Active + VcT(TEXT("nullwing.diveDown"), 1.5f);
		return false;
	}
	if (M == TEXT("rise")) { WRFx::Broadcast(Self, TEXT("void.hush.tap"), Self->GetActorLocation() - FVector(0, 0, Self->GetSimpleCollisionHalfHeight()), FRotator::ZeroRotator, 1.f, TEXT("void.hush.tap")); return true; }
	if (M == TEXT("scream")) { Scream(Self, Row); return true; }
	if (M == TEXT("retreat"))
	{
		if (AWRRift* R = NearestRift(Self->GetWorld(), Self->GetActorLocation(), VcT(TEXT("riftmaw.reach"), 3000.f), false))
		{
			IntoRift(Self, B, R);
			B->BackInUntil = VcNow(Self) + VcT(TEXT("riftmaw.backIn"), 6.f);
		}
		return true;
	}
	if (Row.Id == TEXT("rift_burst") && Target)
	{
		// Out of the rift nearest you: the tear's edge bulged and gasped through the wind-up; now the tooth ring and a
		// thick grey body come through it at you.
		AWRRift* R = NearestRift(Self->GetWorld(), Target->GetActorLocation(), VcT(TEXT("riftmaw.burstFrom"), 800.f) + 400.f, false);
		if (!R) R = B->HomeRift.Get();
		if (!R) return true;
		const FVector Dir = (Target->GetActorLocation() - R->GetActorLocation()).GetSafeNormal2D();
		const FVector Mouth = GroundUnder(Self->GetWorld(), R->GetActorLocation() + Dir * VcT(TEXT("riftmaw.outOfRift"), 250.f), Self) + FVector(0, 0, Self->GetSimpleCollisionHalfHeight());
		OutOfRift(Self, B, Mouth);
		Self->SetActorRotation(Dir.Rotation());
		B->HomeRift = R;
		B->AttacksOut = 0;
		WRFx::Broadcast(Self, TEXT("void.rift.burst"), Mouth, Dir.Rotation(), 1.6f, TEXT("void.rift.burst"));
		for (TActorIterator<AWRCharacterBase> It(Self->GetWorld()); It; ++It)
		{
			if (*It != Self && It->IsAlive() && Self->IsHostileTo(*It) && FVector::Dist2D(It->GetActorLocation(), Mouth + Dir * 150.f) < FMath::Max(150.f, Row.Radius) + It->GetSimpleCollisionRadius())
			{
				AI->RowHit(Self, *It);
			}
		}
		return true;
	}
	if (M == TEXT("spit") && Self->GetDef() && Self->GetDef()->Element == TEXT("void"))
	{
		// A gob of void at where you stood when it wound up; it never homes.
		const FVector From = Self->MouthLocation();
		const FVector AimAt = AI->RowAim + FVector(0.f, 0.f, 60.f);
		FActorSpawnParameters Params;
		Params.Owner = Self;
		Params.Instigator = Self;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AWRProjectile* Glob = Self->GetWorld()->SpawnActor<AWRProjectile>(AWRProjectile::StaticClass(), From, (AimAt - From).Rotation(), Params))
		{
			FWRHit Hit;
			Hit.Instigator = Self;
			Hit.Damage = Self->Weapon.FlatDamage * Row.Dmg;
			Hit.PoiseDamage = Row.Poise;
			Hit.Impact = Row.Impact;
			Hit.bProjectile = true;
			Hit.Element = WRTags::Element_Void;
			for (const TPair<FName, float>& Bu : Row.Buildups) Hit.Buildups.Add(Bu.Key, Bu.Value);
			Glob->Init(Hit, Row.ProjectileSpeed, FLinearColor(0.17f, 0.09f, 0.25f));
		}
		return true;
	}
	return false;
}

void FWRVoidAI::Resolved(AWRCreature* Self, AWRCharacterBase* Victim, FName RowId, EWRHitResult Result)
{
	UWRVoidBody* B = UWRVoidBody::Get(Self);
	if (!B || !Victim || Result != EWRHitResult::Hit) return;
	if (RowId == TEXT("four_jaw_grab")) StartHold(Self, B, Victim, RowId, VcT(TEXT("riftmaw.catch"), 1.4f));
	else if (RowId == TEXT("lift")) StartHold(Self, B, Victim, RowId, VcT(TEXT("hush.catch"), 1.6f));
	else if (RowId == TEXT("hamstring") && Victim->Status)
	{
		const TSharedPtr<FJsonObject> S = Sub(RowExtra(Self->CreatureId, RowId), TEXT("slow"));
		Victim->Status->ApplyStatus(WRTags::Status_Slowed, VcNum(S, TEXT("seconds"), 2.f));
	}
}

float FWRVoidAI::Incoming(const AWRCreature* Self, const FWRHit& Hit, float Damage)
{
	const UWRVoidBody* B = UWRVoidBody::Get(Self);
	if (!B) return Damage;
	if (B->Has(WRVoidFlag::InRift)) return 0.f;   // inside the tear: nothing reaches it
	if (Self->CreatureId == TEXT("void_heart"))
	{
		// The heart shares Vorhollow's health: every hit on it counts x1.5 there, and the heart itself never falls.
		for (TActorIterator<AWRBoss> It(Self->GetWorld()); It; ++It)
		{
			if (It->CreatureId != TEXT("vorhollow") || !It->IsAlive()) continue;
			UAbilitySystemComponent* ASC = It->GetAbilitySystemComponent();
			const UWRAttributeSet* A = It->GetAttributes();
			if (!ASC || !A) break;
			const float Share = WRTune(TEXT("bosses.vorhollow.heartShare"), 1.5f) * Damage;
			if (A->GetHealth() - Share <= 1.f)
			{
				FWRHit Kill = Hit;
				Kill.Damage = A->GetHealth() + 10.f;
				Kill.bUnblockable = true;
				UWRCombatLibrary::ApplyHit(*It, Kill);
			}
			else ASC->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), A->GetHealth() - Share);
			WRFx::Broadcast(Self, TEXT("hit.void"), Self->GetActorLocation(), FRotator::ZeroRotator, 2.f, TEXT("void.heart.beat"));
			break;
		}
		return 0.f;
	}
	// Void on void: its own element barely marks it.
	if (Hit.Element == WRTags::Element_Void) Damage *= VcT(TEXT("voidOnVoid"), 0.5f);
	return Damage;
}

void FWRVoidAI::ModifyHit(const AWRCreature* Self, FWRHit& Hit, const FWRAttackRow* Row)
{
	const UWRVoidBody* B = UWRVoidBody::Get(Self);
	if (!B) return;
	// Its bites are the Void's (the inward ink smear, curse on the bites: the rows carry it); a Riftborn's every blow
	// carries curse 15 more (biome-void 2.12.5).
	if (Hit.Element == WRTags::Element_Physical && Row && Row->Buildups.Contains(TEXT("curse"))) Hit.Element = WRTags::Element_Void;
	if (B->IsRiftborn()) Hit.Buildups.FindOrAdd(TEXT("curse")) += VcT(TEXT("riftborn.curse"), 15.f);
}

void FWRVoidAI::Died(AWRCreature* Self, AActor* Killer)
{
	UWRVoidBody* B = UWRVoidBody::Get(Self);
	if (!B) return;
	FWRVoidBoss::HelperDied(Self);
	if (B->HeldBody.IsValid()) Release(Self, B, false);
	if (B->LatchedOn.IsValid()) Unlatch(Self, B, false);
	if (B->Has(WRVoidFlag::InRift))
	{
		// It dies in the tear's mouth: it falls out onto the ring, where its body can be reached.
		if (AWRRift* R = B->HomeRift.Get()) OutOfRift(Self, B, GroundUnder(Self->GetWorld(), R->GetActorLocation() + FVector(500.f, 0.f, 0.f), Self) + FVector(0, 0, Self->GetSimpleCollisionHalfHeight()));
	}
	// A Riftborn gives its own drops and a bead of void essence.
	if (B->IsRiftborn())
	{
		APawn* KillerPawn = Cast<APawn>(Killer);
		if (const AWRCreature* Tame = Cast<AWRCreature>(Killer)) for (TActorIterator<AWRCharacter> It(Self->GetWorld()); It; ++It) if (It->GetPlayerState() && It->GetPlayerState()->GetPlayerName() == Tame->TamedBy) KillerPawn = *It;
		if (UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(KillerPawn))
		{
			Inv->AddItem(TEXT("void_essence"), int32(VcT(TEXT("riftborn.essence"), 1.f)));
			if (AWRPlayerController* PC = KillerPawn ? Cast<AWRPlayerController>(KillerPawn->GetController()) : nullptr) PC->ClientNotify(TEXT("The Riftborn leaves a bead of void essence"));
		}
	}
}

bool FWRVoidAI::WantsRideBlink(const AWRCreature* Self)
{
	return Self && Self->CreatureId == TEXT("voidstrider") && Self->GetRider() != nullptr;
}

void FWRVoidAI::RideBlink(AWRCreature* Self)
{
	UWRVoidBody* B = UWRVoidBody::Get(Self);
	AWRCharacter* Rider = Self ? Self->GetRider() : nullptr;
	if (!B || !Rider || !Self->HasAuthority()) return;
	const double Now = VcNow(Self);
	if (Now < B->NextRiftbornBlinkAt) return;   // (the same clock: one blink every rideBlink.cooldown)
	B->NextRiftbornBlinkAt = Now + VcT(TEXT("rideBlink.cooldown"), 8.f);
	const FVector Dir = Self->GetActorForwardVector().GetSafeNormal2D();
	FVector To = Self->GetActorLocation() + Dir * VcT(TEXT("rideBlink.distance"), 500.f);
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(WRVoidRideBlink), false, Self);
	Q.AddIgnoredActor(Rider);
	if (Self->GetWorld()->SweepSingleByChannel(Hit, Self->GetActorLocation(), To, FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeSphere(Self->GetSimpleCollisionRadius() * 0.8f), Q)) To = Hit.Location;
	To = GroundUnder(Self->GetWorld(), To, Self) + FVector(0, 0, Self->GetSimpleCollisionHalfHeight() + 5.f);
	if (WRLand::InHole(To)) return;
	// The rider's i-frames cover the travel.
	Rider->SetCombatTag(WRTags::State_IFrames, true);
	TWeakObjectPtr<AWRCharacter> WeakRider(Rider);
	FTimerHandle H;
	Self->GetWorldTimerManager().SetTimer(H, [WeakRider]() { if (WeakRider.IsValid()) WeakRider->SetCombatTag(WRTags::State_IFrames, false); }, VcT(TEXT("rideBlink.iframes"), 0.18f), false);
	WRFx::Broadcast(Self, TEXT("void.blink.smear"), Self->GetActorLocation(), Dir.Rotation(), 1.6f, TEXT("void.blink.snap"));
	Self->SetActorLocation(To, false, nullptr, ETeleportType::TeleportPhysics);
	WRFx::Broadcast(Self, TEXT("void.blink.smear"), To, Dir.Rotation(), 1.6f, NAME_None);
}

void FWRVoidAI::TickBody(AWRCreature* Self, UWRVoidBody* Body, float DeltaTime)
{
	const double Now = VcNow(Self);
	const double SNow = VcServerNow(Self);
	// A planned blink lands on its clock.
	if (Body->bBlinkPending && SNow >= Body->BlinkAt)
	{
		if (Self->IsAlive() && !Self->IsUnconscious() && !Self->HasTag(WRTags::Status_Staggered)) LandBlink(Self, Body);
		else { Body->bBlinkPending = false; Body->Set(WRVoidFlag::Shudder, false); }
	}
	if (Body->Has(WRVoidFlag::Gape) && Now > Body->GapeUntil) Body->Set(WRVoidFlag::Gape, false);
	if (Body->Has(WRVoidFlag::Trail) && Now > Body->TrailUntil) Body->Set(WRVoidFlag::Trail, false);
	if (Body->Has(WRVoidFlag::Risen) && Now > Body->RisenUntil) Body->Set(WRVoidFlag::Risen, false);
	if (Body->Has(WRVoidFlag::Screaming) && Now > Body->ScreamUntil && !Body->HeldBody.IsValid()) Body->Set(WRVoidFlag::Screaming, false);
	if (Body->Has(WRVoidFlag::Grounded) && Now > Body->GroundedUntil) Body->Set(WRVoidFlag::Grounded, false);
	if (Body->Has(WRVoidFlag::Drifting) && Now > Body->DriftUntil) Body->Set(WRVoidFlag::Drifting, false);
	// A catch: held, then (the Hush's lift) the ears fold over you and it screams in your face, then thrown clear.
	if (AWRCharacterBase* V = Body->HeldBody.Get())
	{
		if (!V->IsAlive() || !Self->IsAlive() || Self->HasTag(WRTags::Status_Staggered))
		{
			Release(Self, Body, false);
		}
		else
		{
			if (Body->HoldKind == TEXT("lift") && !Body->Has(WRVoidFlag::Screaming) && Now > Body->HoldUntil - VcT(TEXT("hush.liftScreamAt"), 0.5f))
			{
				Body->Set(WRVoidFlag::Screaming, true);
				Body->ScreamUntil = Body->HoldUntil;
				WRSound::PlayAt(Self, TEXT("void.hush.scream"), Self->GetActorLocation());
				if (V->Status) V->Status->Stagger(VcT(TEXT("hush.liftStagger"), 0.8f) + 0.5f);
			}
			if (Now >= Body->HoldUntil) Release(Self, Body, true);
		}
	}
	// A latch: eating you (3 a second and 3 max health a second) until you roll, swing, or it's knocked off.
	if (AWRCharacterBase* V = Body->LatchedOn.Get())
	{
		const TSharedPtr<FJsonObject> L = Sub(RowExtra(Self->CreatureId, TEXT("latch")), TEXT("latch"));
		if (!V->IsAlive() || !Self->IsAlive() || V->HasTag(WRTags::State_Dodging) || V->HasTag(WRTags::State_Attacking) || Now - Body->LatchedAt > VcT(TEXT("gnawer.maxLatch"), 15.f))
		{
			if (V->HasTag(WRTags::State_Dodging) || V->HasTag(WRTags::State_Attacking)) ShakeOff(V);
			else Unlatch(Self, Body, V->IsAlive());
		}
		else
		{
			Body->LatchTick += DeltaTime;
			if (Body->LatchTick >= 1.f)
			{
				Body->LatchTick -= 1.f;
				FWRHit Hit;
				Hit.Instigator = Self;
				Hit.Damage = VcNum(L, TEXT("dps"), 3.f);
				Hit.bDamageOverTime = true;
				Hit.bUnblockable = true;
				Hit.Element = WRTags::Element_Void;
				Hit.Impact = 1;
				Hit.Origin = Self->GetActorLocation();
				UWRCombatLibrary::ApplyHit(V, Hit);
				WRVoid::LoseMaxHealth(V, VcNum(L, TEXT("maxHpPerSecond"), 3.f), Self, true);
				Body->MulticastMoment(NAME_None, TEXT("head"), 1.f, TEXT("void.gnawer.chew"));
			}
		}
	}
	// A Nullwing with a wing broken comes down for a while (each wing once).
	if (Self->CreatureId == TEXT("nullwing"))
	{
		const uint8 Wings = uint8((Self->IsPartBroken(TEXT("wing_l")) ? 1 : 0) | (Self->IsPartBroken(TEXT("wing_r")) ? 2 : 0));
		if (Wings & ~Body->WingsSeen)
		{
			Body->WingsSeen |= Wings;
			Body->Set(WRVoidFlag::Grounded, true);
			Body->GroundedUntil = Now + VcT(TEXT("nullwing.wingDown"), 4.f);
			Self->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		}
	}
	// Unconscious in its tear: it falls out onto the ring (only then can it be tamed).
	if (Body->Has(WRVoidFlag::InRift) && Self->IsUnconscious())
	{
		if (AWRRift* R = Body->HomeRift.Get()) OutOfRift(Self, Body, GroundUnder(Self->GetWorld(), R->GetActorLocation() + FVector(500.f, 0.f, 0.f), Self) + FVector(0, 0, Self->GetSimpleCollisionHalfHeight()));
	}
}

bool FWRVoidAI::TickWild(AWRCreatureAI* AI, AWRCreature* Self, float DeltaSeconds)
{
	UWRVoidBody* B = UWRVoidBody::Get(Self);
	if (!B) return false;
	const double Now = VcNow(Self);
	if (B->Has(WRVoidFlag::Latched) || B->HeldBody.IsValid() || B->bBlinkPending) return true;   // busy with what it holds
	AWRCharacterBase* Target = Cast<AWRCharacterBase>(Self->GetAggroTarget());
	// The Riftmaw in its tear: it hears and feels what comes near the rift, and bursts out of whichever rift is nearest
	// its prey (within 8 m) once it may.
	if (Self->CreatureId == TEXT("riftmaw"))
	{
		if (B->Has(WRVoidFlag::InRift))
		{
			const AWRRift* Home = B->HomeRift.Get();
			if (!Target && Home)
			{
				for (TActorIterator<AWRCharacter> It(Self->GetWorld()); It; ++It)
				{
					if (It->IsAlive() && Self->IsHostileTo(*It) && It->GetPlayerState() && FVector::Dist2D(It->GetActorLocation(), Home->GetActorLocation()) < VcT(TEXT("riftmaw.sense"), 1400.f)) { Self->SetAggroTarget(*It); Target = *It; break; }
				}
			}
			if (Target && (!Target->IsAlive() || FVector::Dist2D(Target->GetActorLocation(), Self->GetActorLocation()) > VcT(TEXT("riftmaw.giveUp"), 5000.f))) { Self->SetAggroTarget(nullptr); Target = nullptr; }
			if (Target && Now >= B->BackInUntil && NearestRift(Self->GetWorld(), Target->GetActorLocation(), VcT(TEXT("riftmaw.burstFrom"), 800.f) + 400.f, false))
			{
				AI->StartRow(Self, Target, TEXT("rift_burst"), false);
			}
			return true;
		}
		// Out: after 3 attacks or at a quarter of its health it goes back in (if a rift is near); far from any it fights
		// on, slowed, and can't.
		const AWRRift* Near = NearestRift(Self->GetWorld(), Self->GetActorLocation(), VcT(TEXT("riftmaw.reach"), 3000.f), false);
		Self->SpeedBoost = Near ? 1.f : VcT(TEXT("riftmaw.landSpeed"), 0.6f);
		if (Near && (B->AttacksOut >= int32(VcT(TEXT("riftmaw.attacksOut"), 3.f)) || (Self->GetHealthFraction() < VcT(TEXT("riftmaw.retreatAt"), 0.25f) && B->AttacksOut > 0)) && Target)
		{
			B->AttacksOut = 0;
			AI->StartRow(Self, Target, TEXT("retreat"), false);
			return true;
		}
		if (!Target && Near && FVector::Dist2D(Near->GetActorLocation(), Self->GetActorLocation()) < 1200.f && Self->SinceHit() > 10.f)
		{
			IntoRift(Self, B, const_cast<AWRRift*>(Near));   // nothing to hunt: back into the tear
			return true;
		}
		return false;
	}
	// The Hush is blind: it hunts by ear. Each quarter second it listens; while risen it hears twice as far. It loses you
	// when you've been silent 6 s, and goes to where it last heard you.
	if (Self->CreatureId == TEXT("hush"))
	{
		if (Now >= B->NextListenAt)
		{
			B->NextListenAt = Now + 0.25;
			const float Mult = B->Has(WRVoidFlag::Risen) ? VcT(TEXT("hush.risenMult"), 2.f) : 1.f;
			AWRCharacterBase* Heard = nullptr;
			float Best = TNumericLimits<float>::Max();
			for (TActorIterator<AWRCharacter> It(Self->GetWorld()); It; ++It)
			{
				if (!It->IsAlive() || !Self->IsHostileTo(*It)) continue;
				const float D = FVector::Dist(It->GetActorLocation(), Self->GetActorLocation());
				if (D < NoiseReach(*It) * Mult && D < Best) { Best = D; Heard = *It; }
			}
			if (Heard)
			{
				B->HeardAt = Now;
				B->HeardWhere = Heard->GetActorLocation();
				if (!Target) { Self->SetAggroTarget(Heard); WRSound::PlayAt(Self, TEXT("void.hush.click"), Self->GetActorLocation()); }
			}
			else if (Target && Now - B->HeardAt > VcT(TEXT("hush.loseAfter"), 6.f) && FVector::Dist(Target->GetActorLocation(), Self->GetActorLocation()) > VcT(TEXT("hush.touchRange"), 500.f))
			{
				Self->SetAggroTarget(nullptr);
				Self->SetHome(B->HeardWhere);
			}
		}
		return false;
	}
	// A Riftborn blinks too (the Hollowfang's move, every 10 s) to the flank of what it fights, its next blow still wound up.
	if (B->IsRiftborn() && Target && Now >= B->NextRiftbornBlinkAt && FVector::Dist(Target->GetActorLocation(), Self->GetActorLocation()) < 900.f)
	{
		B->NextRiftbornBlinkAt = Now + VcT(TEXT("riftborn.blinkCooldown"), 10.f);
		PlanBlink(Self, B, Target, VcT(TEXT("blink.tell"), 0.5f), false, NAME_None);
		return true;
	}
	// A Nullwing comes down after its dive (the bring-down window).
	if (Self->CreatureId == TEXT("nullwing") && B->Has(WRVoidFlag::Grounded)) return false;
	// A Driftmote hit: its pulse, then it rises 10 m and drifts off.
	if (Self->CreatureId == TEXT("driftmote") && Self->SinceHit() < 0.2f && !B->Has(WRVoidFlag::Drifting))
	{
		B->Set(WRVoidFlag::Drifting, true);
		B->DriftUntil = Now + VcT(TEXT("driftmote.driftSeconds"), 20.f);
		Self->StartFleeing(Self->GetAggroTarget(), TEXT("drifts off"));
	}
	return false;
}

// ================================================================================================ the void trail

AWRVoidTrail::AWRVoidTrail()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(false);
	Ribbon = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Ribbon"));
	Ribbon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Ribbon->SetCastShadow(false);
	RootComponent = Ribbon;
	SetNetCullDistanceSquared(FMath::Square(15000.f));
}

void AWRVoidTrail::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRVoidTrail, A);
	DOREPLIFETIME(AWRVoidTrail, B);
	DOREPLIFETIME(AWRVoidTrail, Thick);
	DOREPLIFETIME(AWRVoidTrail, Seconds);
	DOREPLIFETIME(AWRVoidTrail, Born);
}

double AWRVoidTrail::Now() const { return VcServerNow(this); }

void AWRVoidTrail::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() != NM_DedicatedServer) BuildRibbon();
}

void AWRVoidTrail::BuildRibbon()
{
	// A twisting band of black with a lensing violet edge (M_VoidTear), laid in the Nullwing's wake: a strip of quads
	// along A..B that wavers a little, a cross-section of two faces so it reads from above and below.
	UMaterialInterface* Tear = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_VoidTear.M_VoidTear"));
	TArray<FVector> V;
	TArray<int32> T;
	TArray<FVector> N;
	TArray<FVector2D> UV;
	const FVector LA = GetActorTransform().InverseTransformPosition(FVector(A)), LB = GetActorTransform().InverseTransformPosition(FVector(B));
	const int32 Segs = 40;
	for (int32 i = 0; i <= Segs; ++i)
	{
		const float S = float(i) / Segs;
		const FVector P = FMath::Lerp(LA, LB, S) + FVector(0.f, 30.f * FMath::Sin(S * 9.f), 20.f * FMath::Sin(S * 13.f + 1.f));
		const float Twist = 0.6f * FMath::Sin(S * 6.f);
		const FVector Up(0.f, FMath::Sin(Twist), FMath::Cos(Twist));
		const float W = Thick * 0.5f * FMath::Clamp(FMath::Min(S, 1.f - S) * 8.f, 0.15f, 1.f);
		V.Add(P + Up * W);
		V.Add(P - Up * W);
		N.Add(FVector(0.f, -Up.Z, Up.Y));
		N.Add(FVector(0.f, -Up.Z, Up.Y));
		UV.Add(FVector2D(S * 12.f, 0.f));
		UV.Add(FVector2D(S * 12.f, 1.f));
		if (i > 0)
		{
			const int32 a = (i - 1) * 2, b = a + 1, c = a + 2, d = a + 3;
			T.Append({ a, c, b, b, c, d, a, b, c, b, d, c });
		}
	}
	Ribbon->CreateMeshSection(0, V, T, N, UV, TArray<FColor>(), TArray<FProcMeshTangent>(), false);
	if (Tear)
	{
		Mat = UMaterialInstanceDynamic::Create(Tear, this);
		Ribbon->SetMaterial(0, Mat);
	}
	// Its edge sheds violet wisps along the length.
	for (int32 k = 0; k < 5; ++k)
	{
		if (USceneComponent* Fx = WRFx::Attach(Ribbon, TEXT("rift.edge"), NAME_None, 0.6f))
		{
			Fx->SetRelativeLocation(FMath::Lerp(LA, LB, (k + 0.5f) / 5.f));
		}
	}
	WRSound::PlayAt(this, TEXT("void.trail.lay"), GetActorLocation());
}

void AWRVoidTrail::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Age = float(Now() - Born);
	// It opens over 0.4 s and closes over its last 0.6 s (scale across its thickness: never pops).
	const float Open = FMath::Clamp(Age / 0.4f, 0.f, 1.f) * FMath::Clamp((Seconds - Age) / 0.6f, 0.f, 1.f);
	if (Ribbon) Ribbon->SetRelativeScale3D(FVector(1.f, FMath::Max(0.05f, Open), FMath::Max(0.05f, Open)));
	if (!HasAuthority() || Open < 0.5f) return;
	// Touching it: 8 a tick (every 0.5 s) and curse 30 a second, through ApplyHit.
	const FVector PA = A, PB = B;
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		if (!It->IsAlive() || *It == Layer.Get()) continue;
		const AWRCreature* Cr = Cast<AWRCreature>(*It);
		if (Cr && UWRVoidBody::IsVoidKind(Cr->CreatureId)) continue;
		const FVector P = It->GetActorLocation();
		const FVector Q = FMath::ClosestPointOnSegment(P, PA, PB);
		if (FVector::Dist2D(P, Q) > Thick * 0.5f + It->GetSimpleCollisionRadius() || FMath::Abs(P.Z - Q.Z) > It->GetSimpleCollisionHalfHeight() + Thick * 0.5f) continue;
		double& Last = LastTick.FindOrAdd(*It);
		if (Now() - Last < TickEvery) continue;
		Last = Now();
		FWRHit Hit;
		Hit.Instigator = Layer.IsValid() ? Layer.Get() : this;
		Hit.Damage = TickDamage;
		Hit.Element = WRTags::Element_Void;
		Hit.bUnblockable = true;
		Hit.bDamageOverTime = true;
		Hit.Impact = 1;
		Hit.Origin = Q;
		Hit.Buildups.Add(TEXT("curse"), CursePerSecond * TickEvery);
		UWRCombatLibrary::ApplyHit(*It, Hit);
		WRFx::Broadcast(this, TEXT("hit.void"), Q);
	}
}

// ================================================================================================ the dens

bool UWRVoidLife::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* W = Cast<UWorld>(Outer);
	return W && (W->WorldType == EWorldType::Game || W->WorldType == EWorldType::PIE);
}

void UWRVoidLife::PlaceDens(AWRLand* Land)
{
	const FWRLandDef* D = Land ? Land->Def() : nullptr;
	UWorld* World = Land ? Land->GetWorld() : nullptr;
	const TSharedPtr<FJsonObject> S = D ? D->Section(TEXT("spawns")) : nullptr;
	if (!World || !S.IsValid() || World->GetNetMode() == NM_Client) return;
	const float Gap = VcNum(S, TEXT("denGap"), 170.f);
	const float Act = VcNum(S, TEXT("activation"), 12000.f);
	const float Respawn = VcNum(S, TEXT("respawn"), 600.f);
	const TSharedPtr<FJsonObject> Zones = Sub(S, TEXT("zones"));
	if (!Zones.IsValid()) return;
	TArray<FVector2D> Places;
	const TArray<TSharedPtr<FJsonValue>>* Pois = nullptr;
	if (D->Json->TryGetArrayField(TEXT("pois"), Pois)) for (const TSharedPtr<FJsonValue>& V : *Pois) Places.Add(WRLand::Vec2(V->AsObject()->TryGetField(TEXT("at"))));
	int32 Dens = 0;
	const auto Spawn = [&](FName Kind, const FVector& At, uint8 Shift, float Riftborn)
	{
		const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(Kind);
		if (!Def) { UE_LOG(LogWildreach, Warning, TEXT("VOID dens: no creature %s"), *Kind.ToString()); return; }
		const FTransform T(At);
		AWRCreatureSpawner* Sp = World->SpawnActorDeferred<AWRCreatureSpawner>(AWRCreatureSpawner::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Sp) return;
		Sp->CreatureId = Kind;
		Sp->Count = FMath::RandRange(Def->PackMin, FMath::Max(Def->PackMin, Def->PackMax));
		Sp->Radius = FMath::Max(250.f, 120.f * Sp->Count);
		Sp->RespawnSeconds = Respawn;
		Sp->ActivationRadius = Act;
		Sp->bTraceGround = true;
		Sp->Shift = Shift;
		Sp->Tags.Add(FName(*FString::Printf(TEXT("riftborn:%.0f"), Riftborn)));
		Sp->Tags.Add(TEXT("void_den"));
		Sp->FinishSpawning(T);
		++Dens;
	};
	// The zones' dens: a seeded pass, away from the places, each den's day kind and night kind rolled by weight.
	for (const TPair<FString, TSharedPtr<FJsonValue>>& ZV : Zones->Values)
	{
		const TSharedPtr<FJsonObject> Z = ZV.Value->AsObject();
		const FWRLandZone* Zone = D->Zones.FindByPredicate([&](const FWRLandZone& Q) { return Q.Id == FName(*ZV.Key); });
		if (!Z.IsValid() || !Zone) continue;
		const int32 Want = int32(VcNum(Z, TEXT("dens"), 8.f));
		const float Rb = VcNum(Z, TEXT("riftborn"), 5.f);
		const auto Roll = [&](const TCHAR* Table, float R) -> FName
		{
			const TSharedPtr<FJsonObject> Tb = Sub(Z, Table);
			if (!Tb.IsValid()) return NAME_None;
			float Total = 0.f;
			for (const auto& KV : Tb->Values) Total += float(KV.Value->AsNumber());
			float Pick = R * Total;
			for (const auto& KV : Tb->Values) { Pick -= float(KV.Value->AsNumber()); if (Pick <= 0.f) return FName(*KV.Key); }
			return NAME_None;
		};
		TArray<FVector2D> Taken = Places;
		int32 Placed = 0;
		for (int32 Try = 0; Try < 3000 && Placed < Want; ++Try)
		{
			const FVector2D M(FMath::Lerp(D->MapBox.Min.X, D->MapBox.Max.X, (D->Noise(FVector2D(Try * 0.71f, 9.1f)) + 1.f) * 0.5f),
				FMath::Lerp(D->MapBox.Min.Y, D->MapBox.Max.Y, (D->Noise(FVector2D(4.4f, Try * 0.67f)) + 1.f) * 0.5f));
			if (D->ZoneAt(M) != Zone || D->CoastDistance(M) < 30.f || D->HoleAt(M, 60.f)) continue;
			bool bClose = false;
			for (const FVector2D& Q : Taken) if (FVector2D::DistSquared(Q, M) < Gap * Gap) { bClose = true; break; }
			if (bClose) continue;
			Taken.Add(M);
			const float R1 = (D->Noise(M * 0.37f + FVector2D(2.f, 2.f)) + 1.f) * 0.5f, R2 = (D->Noise(M * 0.41f + FVector2D(7.f, 3.f)) + 1.f) * 0.5f;
			const FName Day = Roll(TEXT("day"), R1), Night = Roll(TEXT("night"), R2);
			const FVector At = D->GroundPoint(M) + FVector(0, 0, 100.f);
			if (!Day.IsNone() && Day == Night) Spawn(Day, At, 0, Rb);
			else
			{
				if (!Day.IsNone()) Spawn(Day, At, 1, Rb);
				if (!Night.IsNone()) Spawn(Night, At + FVector(400.f, 0.f, 0.f), 2, Rb);
			}
			++Placed;
		}
	}
	// A Riftmaw in every large rift (fixed, day and night).
	for (TActorIterator<AWRRift> It(World); It; ++It)
	{
		if (!It->bLarge || It->bSteady) continue;
		Spawn(TEXT("riftmaw"), It->GetActorLocation() + FVector(600.f, 0.f, 150.f), 0, 0.f);
	}
	// The Hush on the Gauntlet Road at night, one a segment.
	if (const FWRLandFeature* Road = D->Feature(TEXT("gauntlet_road")))
	{
		for (int32 i = 0; i + 1 < Road->Points.Num(); ++i)
		{
			const FVector2D Mid = (Road->Points[i] + Road->Points[i + 1]) * 0.5f;
			const FVector2D Dir = (Road->Points[i + 1] - Road->Points[i]).GetSafeNormal();
			Spawn(TEXT("hush"), D->GroundPoint(Mid + FVector2D(-Dir.Y, Dir.X) * 25.f) + FVector(0, 0, 100.f), 2, 0.f);
		}
	}
	UE_LOG(LogWildreach, Log, TEXT("VOID %s: %d dens"), *D->Id.ToString(), Dens);
}
