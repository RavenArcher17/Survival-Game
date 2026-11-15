// The Sky islands' creatures: the body and its look (see WRSkyCreatures.h; cooks/biome-sky.md 2.9, build-biome-sky.md
// 2.11). Their brains and the living world are in WRSkyCreatureAI.cpp.

#include "WRSkyCreatures.h"
#include "Wildreach.h"
#include "WRBoss.h"
#include "WRBuildLook.h"
#include "WRBuildLookKit.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRGameState.h"
#include "WRSkyBeast.h"
#include "WRSound.h"
#include "WRTuning.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"

namespace WRSkyCrLook
{
	TSharedPtr<FJsonObject> ScRow(FName Id)
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
	TSharedPtr<FJsonObject> ScLook(FName Id)
	{
		const TSharedPtr<FJsonObject> R = ScRow(Id);
		const TSharedPtr<FJsonObject>* L = nullptr;
		return R.IsValid() && R->TryGetObjectField(TEXT("look"), L) ? *L : nullptr;
	}
	float ScNum(const TSharedPtr<FJsonObject>& O, const TCHAR* F, float Fallback)
	{
		double V = 0.0;
		return O.IsValid() && O->TryGetNumberField(F, V) ? float(V) : Fallback;
	}
	FLinearColor ScColor(const TSharedPtr<FJsonObject>& O, const TCHAR* F, const FLinearColor& Fallback)
	{
		FString S;
		return O.IsValid() && O->TryGetStringField(F, S) ? FLinearColor(FColor::FromHex(S)) : Fallback;
	}
	FLinearColor ScHex(const TCHAR* S) { return FLinearColor(FColor::FromHex(S)); }
	/** Groups that move (TickLook): fore-wing, hind-wing, crest, mane, soft wing (Puffwing), shrike wing, gape, disc. */
	enum EScGroup : int32 { SkGStatic = 0, SkGWingF = 1, SkGWingH = 2, SkGCrest = 3, SkGMane = 4, SkGSoftWing = 5, SkGShrikeWing = 6, SkGGape = 7, SkGDisc = 8, SkGTail = 9, SkGVanes = 10 };
}
using namespace WRSkyCrLook;

TArray<TWeakObjectPtr<UWRSkyBody>> UWRSkyBody::Live;

UWRSkyBody::UWRSkyBody()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UWRSkyBody::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UWRSkyBody, Flags);
	DOREPLIFETIME(UWRSkyBody, Holding);
	DOREPLIFETIME(UWRSkyBody, Seed);
}

UWRSkyBody* UWRSkyBody::Get(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UWRSkyBody>() : nullptr;
}

bool UWRSkyBody::IsSkyKind(FName CreatureId)
{
	// The nine and the four bosses (the bosses' brains are WRSkyBosses; the body here carries their look and flags).
	const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(CreatureId);
	return Def && Def->Biome == TEXT("sky_islands");
}

void UWRSkyBody::Attach(AWRCreature* Creature)
{
	if (!Creature || !Creature->HasAuthority() || !IsSkyKind(Creature->CreatureId) || Get(Creature)) return;
	UWRSkyBody* Body = NewObject<UWRSkyBody>(Creature, TEXT("SkyBody"));
	Body->Seed = FMath::Rand();
	// A bloom's bells come in two hands (the man o' war): left-handed drift left of the wind, right-handed right.
	if (Creature->CreatureId == TEXT("sailbell") && (Body->Seed & 1)) Body->Flags |= WRSkyFlag::LeftHand;
	Body->RegisterComponent();
	Creature->AddInstanceComponent(Body);
}

void UWRSkyBody::Set(uint32 Flag, bool bOn)
{
	const uint32 Was = Flags;
	Flags = bOn ? (Flags | Flag) : (Flags & ~Flag);
	if (Was != Flags && GetNetMode() != NM_DedicatedServer) OnRep_Flags();
}

void UWRSkyBody::OnRep_Flags()
{
}

void UWRSkyBody::BeginPlay()
{
	Super::BeginPlay();
	Live.RemoveAll([](const TWeakObjectPtr<UWRSkyBody>& B) { return !B.IsValid(); });
	Live.Add(this);
}

void UWRSkyBody::EndPlay(const EEndPlayReason::Type Reason)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		if (AWRCreature* C = Cast<AWRCreature>(GetOwner()); C && HeldBody.IsValid()) FWRSkyAI::Release(C, this, false);
	}
	ClearLook();
	Live.RemoveAll([this](const TWeakObjectPtr<UWRSkyBody>& B) { return !B.IsValid() || B.Get() == this; });
	Super::EndPlay(Reason);
}

USkeletalMeshComponent* UWRSkyBody::Skel() const
{
	const AWRCreature* C = Cast<AWRCreature>(GetOwner());
	return C ? C->GetCreatureSkel() : nullptr;
}

void UWRSkyBody::MulticastMoment_Implementation(FName Effect, FName Bone, float Scale, FName Sound)
{
	const AWRCreature* C = Cast<AWRCreature>(GetOwner());
	if (!C || GetNetMode() == NM_DedicatedServer) return;
	FVector At = C->GetActorLocation();
	if (Beast) At = Beast->MouthWorld();
	else if (!Bone.IsNone() && Skel())
	{
		const FName B = C->FindBone(Bone);
		if (!B.IsNone()) At = Skel()->GetBoneLocation(B);
	}
	if (!Effect.IsNone()) WRFx::SpawnAt(C, Effect, At, C->GetActorRotation(), Scale);
	if (!Sound.IsNone()) WRSound::PlayAt(C, Sound, At);
}

void UWRSkyBody::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	Clock += DeltaTime;
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	if (!C) return;
	if (C->HasAuthority() && !C->IsA<AWRBoss>()) FWRSkyAI::TickBody(C, this, DeltaTime);
	if (GetNetMode() != NM_DedicatedServer) TickLook(DeltaTime);
}

// ================================================================================================ the look

USceneComponent* UWRSkyBody::AddGroup(FName Bone, const FVector& WorldAt, const FRotator& WorldRot, const TArray<FWRLookBlock>& Blocks, int32 Kind)
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
	G.BaseScale = Frame->GetRelativeScale3D();
	G.Phase = FMath::FRand() * 6.28f;
	Groups.Add(G);
	return Frame;
}

void UWRSkyBody::ClearLook()
{
	for (USceneComponent* Comp : LookComps) if (Comp) Comp->DestroyComponent();
	LookComps.Reset();
	Groups.Reset();
	LookFor = NAME_None;
}

void UWRSkyBody::BuildLook()
{
	using namespace WRLookKit;
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	const FWRCreatureDef* Def = C ? C->GetDef() : nullptr;
	if (!Def) return;

	// The bodies no rig has: built in code, the capsule's placeholder hidden.
	if (!Def->ProcBody.IsNone() && (Def->ProcBody == TEXT("ray") || Def->ProcBody == TEXT("whale") || Def->ProcBody == TEXT("bell") || Def->ProcBody == TEXT("serpent")))
	{
		if (!Beast)
		{
			// Old Gulper is a Cloudback x3.14, scarred (creatures.json look beastScale / scar).
			const TSharedPtr<FJsonObject> BLook = ScLook(C->CreatureId);
			bool bScar = false;
			if (BLook.IsValid()) BLook->TryGetBoolField(TEXT("scar"), bScar);
			Beast = NewObject<UWRSkyBeast>(C, TEXT("SkyBeast"));
			Beast->SetupAttachment(C->GetRootComponent());
			Beast->RegisterComponent();
			Beast->Init(C, Def->ProcBody, ScNum(BLook, TEXT("beastScale"), 1.f), Seed, bScar);
			// The whale's belly sits 2 m over what it floats above; the ray and the bell ride the capsule's middle.
			for (UActorComponent* Comp : C->GetComponents())
			{
				UStaticMeshComponent* SM = Cast<UStaticMeshComponent>(Comp);
				if (SM && SM->GetAttachParent() == C->GetRootComponent() && SM != static_cast<UPrimitiveComponent*>(Beast)) SM->SetVisibility(false, true);
			}
			if (USkeletalMeshComponent* Sk = Skel()) Sk->SetVisibility(false, true);
		}
		LookFor = C->CreatureId;
		return;
	}

	USkeletalMeshComponent* Body = Skel();
	if (!Body || !Body->GetSkeletalMeshAsset()) return;
	ClearLook();
	LookFor = C->CreatureId;
	const TSharedPtr<FJsonObject> Look = ScLook(C->CreatureId);

	// The cook's palette over the rig's own texture (M_CreatureTint).
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
			UTexture* Nrm = Pick->TryGetStringField(TEXT("normal"), Path) ? LoadObject<UTexture>(nullptr, *Path) : nullptr;
			if (!Nrm) Nrm = LoadObject<UTexture>(nullptr, TEXT("/Engine/EngineMaterials/DefaultNormal.DefaultNormal"));
			if (Nrm) M->SetTextureParameterValue(TEXT("NormalTex"), Nrm);
			if (UTexture* T = LoadObject<UTexture>(nullptr, TEXT("/Engine/EngineResources/Black.Black"))) M->SetTextureParameterValue(TEXT("GlowTex"), T);
			M->SetScalarParameterValue(TEXT("Desaturation"), ScNum(Pick, TEXT("desat"), 1.f));
			M->SetScalarParameterValue(TEXT("Brightness"), ScNum(Pick, TEXT("bright"), 1.f));
			M->SetScalarParameterValue(TEXT("TwoTone"), ScNum(Pick, TEXT("twoTone"), 0.f));
			M->SetScalarParameterValue(TEXT("LightFrom"), ScNum(Pick, TEXT("lightFrom"), 0.f));
			M->SetScalarParameterValue(TEXT("Roughness"), ScNum(Pick, TEXT("rough"), 0.75f));
			M->SetScalarParameterValue(TEXT("GlowStrength"), 0.f);
			const FLinearColor Tint = ScColor(Pick, TEXT("tint"), FLinearColor::White);
			M->SetVectorParameterValue(TEXT("Tint"), Tint);
			M->SetVectorParameterValue(TEXT("TintLight"), ScColor(Pick, TEXT("light"), Tint));
			M->SetVectorParameterValue(TEXT("GlowColor"), FLinearColor::Black);
			Body->SetMaterial(Slot, M);
			TintMats.Add(M);
		}
	}

	const auto BoneAt = [&](const TCHAR* Hint, FVector& Out) -> FName
	{
		const FName B = C->FindBone(FName(Hint));
		if (!B.IsNone()) Out = Body->GetBoneLocation(B);
		return B;
	};
	/** Every bone whose name holds Part (all four feet, both wings). */
	const auto BonesWith = [&](const TCHAR* Part) -> TArray<FName>
	{
		TArray<FName> Out;
		const FReferenceSkeleton& Ref = Body->GetSkeletalMeshAsset()->GetRefSkeleton();
		const FString Want = FString(Part).ToLower();
		for (int32 i = 0; i < Ref.GetNum(); ++i)
		{
			const FString N = Ref.GetBoneName(i).ToString().ToLower();
			if (N.Contains(Want) && !N.Contains(TEXT("end")) && !N.Contains(TEXT("nub")) && !N.Contains(TEXT("toe")) && !N.Contains(TEXT("finger"))) Out.Add(Ref.GetBoneName(i));
		}
		return Out;
	};
	const auto Blocks = [&](TFunctionRef<void(FKit&)> Fn) { TArray<FWRLookBlock> Out; FKit Kit(FTransform::Identity, Out); Fn(Kit); return Out; };
	const FRotator Yaw(0.f, C->GetActorRotation().Yaw, 0.f);
	const FVector Fwd = Yaw.Vector(), Right = FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y);
	const float Rad = Def->Radius, HH = Def->HalfHeight;
	FVector Head = C->GetActorLocation() + Fwd * Rad;
	const FName HeadBone = BoneAt(TEXT("head"), Head);
	// A boss on a creature's look (Kyrra is a Galeharrier grown old): the look's kind, and its own extras after.
	FName K = C->CreatureId;
	{
		FString LK;
		if (Look.IsValid() && Look->TryGetStringField(TEXT("kind"), LK)) K = FName(*LK);
	}
	const auto S = [](const TCHAR* H, float Dim = 0.62f) { return FSurf::Cloth(ScHex(H) * Dim); };
	if (C->CreatureId == TEXT("kyrra"))
	{
		// Her crest white with age; one talon a stump bound in old Kitewright chain (links wound round the stump).
		TArray<FName> Feet = BonesWith(TEXT("foot"));
		if (Feet.Num())
		{
			const FVector P = Body->GetBoneLocation(Feet[0]);
			AddGroup(Feet[0], P, Yaw, Blocks([&](FKit& Kit)
			{
				Kit.Add(FSurf::Glaze(ScHex(TEXT("D9B23A")) * 0.5f, 0.6f), FVector(Rad * 0.18f, Rad * 0.16f, Rad * 0.12f), FVector::ZeroVector);
				for (int32 l = 0; l < 7; ++l)
				{
					const float A = l * 0.9f;
					Kit.Add(FSurf::Steel(ScHex(TEXT("5C4A3C")) * 0.5f, 0.75f), FVector(Rad * 0.08f, Rad * 0.03f, Rad * 0.05f), FVector(FMath::Cos(A) * Rad * 0.1f, FMath::Sin(A) * Rad * 0.1f, -Rad * 0.02f + l * Rad * 0.015f), FRotator(0.f, FMath::RadiansToDegrees(A) + 90.f, l % 2 ? 90.f : 0.f));
				}
				// The broken end of the chain trailing.
				for (int32 l = 0; l < 4; ++l) Kit.Add(FSurf::Steel(ScHex(TEXT("5C4A3C")) * 0.5f, 0.75f), FVector(Rad * 0.08f, Rad * 0.03f, Rad * 0.05f), FVector(-Rad * (0.1f + 0.07f * l), 0.f, -Rad * (0.06f + 0.05f * l)), FRotator(30.f, 0.f, l % 2 ? 90.f : 0.f));
			}), SkGStatic);
		}
	}
	else if (C->CreatureId == TEXT("bridge_sentinel"))
	{
		// The halo of eight stone wind-vanes behind its head, on a granite ring: they turn as it walks and sing (its
		// harp chord); they stop and point down before the Vane slam, spin up to a whine for the Vane spin.
		FVector Hd = Head;
		const FName HdB = HeadBone;
		AddGroup(HdB, Hd - Fwd * Rad * 0.45f + FVector(0, 0, HH * 0.12f), FRotator(0.f, Yaw.Yaw, 0.f), Blocks([&](FKit& Kit)
		{
			const FSurf Gran = FSurf::Stone(ScHex(TEXT("E2DED6")) * 0.62f, 0.85f);
			for (int32 r = 0; r < 16; ++r)
			{
				const float A = r * PI / 8.f;
				Kit.Add(Gran, FVector(14.f, 24.f, 22.f), FVector(0.f, FMath::Cos(A) * 110.f, FMath::Sin(A) * 110.f), FRotator(0.f, 0.f, FMath::RadiansToDegrees(A)));
			}
			for (int32 v = 0; v < 8; ++v)
			{
				const float A = v * PI / 4.f;
				const FVector Dir(0.f, FMath::Cos(A), FMath::Sin(A));
				Kit.Add(Gran, FVector(6.f, 22.f, 90.f), Dir * 165.f, FRotator(0.f, 0.f, FMath::RadiansToDegrees(A) - 90.f));
				Kit.Add(FSurf::Stone(ScHex(TEXT("9A958C")) * 0.55f, 0.9f), FVector(8.f, 30.f, 8.f), Dir * 112.f, FRotator(0.f, 0.f, FMath::RadiansToDegrees(A) - 90.f));
			}
		}), SkGVanes);
	}

	if (K == TEXT("galeharrier"))
	{
		// The double crest: two fans of stiff feathers raked back from the crown, pale-teal tips (they flare before a
		// stoop); a black hood with a white face-ring round the eyes; a hooked yellow-based beak.
		const float U = Rad * 0.42f;
		AddGroup(HeadBone, Head, Yaw, Blocks([&](FKit& Kit)
		{
			for (int32 Row = 0; Row < 2; ++Row)
			{
				for (int32 f = 0; f < 5; ++f)
				{
					const float Lat = (f - 2) * U * 0.09f;
					const float Len = U * (0.55f + 0.12f * (2 - FMath::Abs(f - 2)) + Row * 0.18f);
					const FVector Base(-U * 0.05f - Row * U * 0.12f, Lat, U * 0.3f);
					const FVector Dir = FVector(-0.8f, Lat * 0.01f, 0.6f + Row * 0.15f).GetSafeNormal();
					Kit.Rod(S(TEXT("1E2023"), 0.6f), Base, Base + Dir * Len, U * 0.05f, true);
					Kit.Add(FSurf::Cloth(ScHex(TEXT("9FD6CF")) * 0.62f), FVector(U * 0.12f, U * 0.05f, U * 0.05f), Base + Dir * Len, Dir.Rotation());
				}
			}
		}), SkGCrest);
		AddGroup(HeadBone, Head, Yaw, Blocks([&](FKit& Kit)
		{
			Kit.Add(S(TEXT("121314")), FVector(U * 0.8f, U * 0.75f, U * 0.45f), FVector(-U * 0.05f, 0.f, U * 0.18f));
			for (const float Sd : { -1.f, 1.f })
			{
				Kit.Disc(FSurf::Cloth(ScHex(TEXT("F4F3EE")) * 0.66f), FVector(U * 0.22f, Sd * U * 0.36f, U * 0.08f), FVector(0.f, Sd, 0.f), U * 0.32f, 1.5f);
				Kit.Add(FSurf::Glaze(ScHex(TEXT("E0B12A")) * 0.6f, 0.3f), FVector(U * 0.09f), FVector(U * 0.24f, Sd * U * 0.38f, U * 0.1f));
				Kit.Add(FSurf::Glaze(FLinearColor(0.01f, 0.01f, 0.01f), 0.2f), FVector(U * 0.045f), FVector(U * 0.27f, Sd * U * 0.4f, U * 0.1f));
			}
			Kit.Spike(FSurf::Glaze(ScHex(TEXT("2A2A2A")) * 0.6f, 0.35f), FVector(U * 0.45f, 0.f, -U * 0.02f), FVector(1.f, 0.f, -0.55f), U * 0.18f, U * 0.32f);
		}), SkGStatic);
		// The feet too big for it: yellow scaled toes and 7 cm black talons on every foot.
		for (const FName& F : BonesWith(TEXT("foot")))
		{
			const FVector P = Body->GetBoneLocation(F);
			AddGroup(F, P, Yaw, Blocks([&](FKit& Kit)
			{
				Kit.Add(FSurf::Glaze(ScHex(TEXT("D9B23A")) * 0.6f, 0.45f), FVector(U * 0.3f, U * 0.24f, U * 0.12f), FVector(U * 0.06f, 0.f, 0.f));
				for (int32 t = 0; t < 4; ++t)
				{
					const float A = t < 3 ? (t - 1) * 0.5f : PI;
					const FVector Dir(FMath::Cos(A), FMath::Sin(A), -0.55f);
					Kit.Spike(FSurf::Glaze(FLinearColor(0.01f, 0.01f, 0.01f), 0.25f), FVector(U * 0.08f, 0.f, -U * 0.04f) + Dir * U * 0.12f, Dir, U * 0.05f, U * 0.18f);
				}
			}), SkGStatic);
		}
	}
	else if (K == TEXT("mistlark"))
	{
		// A fanned, notched tail and the mist-blue throat.
		const float U = 30.f;
		FVector Tl = C->GetActorLocation() - Fwd * Rad;
		const FName TailB = BoneAt(TEXT("tail"), Tl);
		AddGroup(TailB, Tl, Yaw, Blocks([&](FKit& Kit)
		{
			for (int32 f = 0; f < 7; ++f) Kit.Add(S(f == 0 || f == 6 ? TEXT("F2F2F2") : TEXT("7E8288")), FVector(U * 0.45f, U * 0.08f, 0.6f), FVector(-U * 0.2f, 0.f, 0.f), FRotator(0.f, 180.f + (f - 3) * 11.f, 0.f));
		}), SkGTail);
		AddGroup(HeadBone, Head, Yaw, Blocks([&](FKit& Kit) { Kit.Add(S(TEXT("8FB6D2"), 0.66f), FVector(U * 0.22f, U * 0.18f, U * 0.18f), FVector(-U * 0.05f, 0.f, -U * 0.12f)); }), SkGStatic);
	}
	else if (K == TEXT("windmane"))
	{
		// Four feathered wings: long fore-wings from the shoulders (6 m across), shorter hind-wings from the thighs
		// (3.5 m): silver-white coverts over grey-barred flight feathers; a long silver mane; the antlers' dark tips.
		FVector Sh = C->GetActorLocation() + Fwd * Rad * 0.6f;
		const FName ShB = BoneAt(TEXT("spine3"), Sh);
		FVector Hp = C->GetActorLocation() - Fwd * Rad * 0.6f;
		const FName HpB = BoneAt(TEXT("pelvis"), Hp);
		const auto Wing = [&](float Span, float Chord) -> TArray<FWRLookBlock>
		{
			return Blocks([&](FKit& Kit)
			{
				// Laid out along +Y (outward), trailing edge toward -X; the frame turns per side.
				Kit.Add(FSurf::Cloth(ScHex(TEXT("ECEBE6")) * 0.66f), FVector(Chord * 0.45f, Span * 0.42f, 5.f), FVector(-Chord * 0.15f, Span * 0.21f, 0.f));
				Kit.Add(FSurf::Cloth(ScHex(TEXT("E3E2DD")) * 0.64f), FVector(Chord * 0.3f, Span * 0.35f, 4.f), FVector(-Chord * 0.05f, Span * 0.55f, 0.f));
				for (int32 f = 0; f < 11; ++f)
				{
					const float T = f / 10.f;
					const FVector Root(-Chord * 0.3f, Span * (0.15f + 0.8f * T), 0.f);
					const float Len = Chord * (0.75f + 0.55f * T);
					const FVector Dir = FVector(-1.f, 0.35f * T * T, 0.f).GetSafeNormal();
					// Flight feathers barred grey: the vane in bands.
					for (int32 b = 0; b < 4; ++b)
					{
						const FSurf Band = (b % 2) ? FSurf::Cloth(ScHex(TEXT("8A8E92")) * 0.6f) : FSurf::Cloth(ScHex(TEXT("D7D9DB")) * 0.64f);
						Kit.Add(Band, FVector(Len * 0.25f, Chord * 0.09f, 2.5f), Root + Dir * Len * (0.125f + 0.25f * b), Dir.Rotation());
					}
				}
			});
		};
		const float Fore = 300.f, Hind = 175.f;
		for (const float Sd : { -1.f, 1.f })
		{
			// The +Y layout mirrored to the left by turning the frame half round (the trailing edge then faces +X: flip).
			const FRotator WingRot = Sd > 0.f ? Yaw : FRotator(0.f, Yaw.Yaw, 180.f);
			if (AddGroup(ShB, Sh + Right * Sd * Rad * 0.35f + FVector(0, 0, HH * 0.25f), WingRot, Wing(Fore, 120.f), SkGWingF)) Groups.Last().Side = Sd;
			if (AddGroup(HpB, Hp + Right * Sd * Rad * 0.3f + FVector(0, 0, HH * 0.1f), WingRot, Wing(Hind, 80.f), SkGWingH)) Groups.Last().Side = Sd;
		}
		FVector Nk = Head - Fwd * Rad * 0.4f;
		const FName NkB = BoneAt(TEXT("neck"), Nk);
		AddGroup(NkB, Nk, Yaw, Blocks([&](FKit& Kit)
		{
			for (int32 r = 0; r < 14; ++r) Kit.Add(FSurf::Cloth(ScHex(r % 3 ? TEXT("EEEDEA") : TEXT("C9CACC")) * 0.64f), FVector(5.f, 3.f, 40.f + 18.f * (r % 4)), FVector((r - 7) * 6.f, (r % 2 ? 1.f : -1.f) * 4.f, 12.f), FRotator(-40.f, 0.f, (r % 2 ? 1.f : -1.f) * 10.f));
		}), SkGMane);
		AddGroup(HeadBone, Head, Yaw, Blocks([&](FKit& Kit)
		{
			for (const float Sd : { -1.f, 1.f }) for (int32 t = 0; t < 3; ++t)
			{
				const FVector Tip(-Rad * (0.2f + 0.15f * t), Sd * Rad * (0.35f + 0.1f * t), Rad * (0.75f + 0.12f * t));
				Kit.Spike(FSurf::Glaze(ScHex(TEXT("3A332C")) * 0.6f, 0.4f), Tip, FVector(-0.3f, Sd * 0.3f, 1.f), 4.f, 12.f);
			}
		}), SkGStatic);
	}
	else if (K == TEXT("puffwing"))
	{
		// Oversized soft wings at its sides (white glider-grade feathers under them), a red bill and frontal shield, the blue breast.
		FVector Sp = C->GetActorLocation();
		const FName SpB = BoneAt(TEXT("spine"), Sp);
		for (const float Sd : { -1.f, 1.f })
		{
			if (AddGroup(SpB, Sp + Right * Sd * Rad * 0.7f + FVector(0, 0, HH * 0.15f), FRotator(0.f, Yaw.Yaw, Sd * 6.f), Blocks([&](FKit& Kit)
			{
				Kit.Add(FSurf::Cloth(ScHex(TEXT("3F5F35")) * 0.62f), FVector(Rad * 1.1f, Rad * 0.15f, Rad * 0.6f), FVector(-Rad * 0.15f, 0.f, 0.f));
				Kit.Add(FSurf::Cloth(ScHex(TEXT("F4F4F0")) * 0.66f), FVector(Rad * 1.0f, Rad * 0.08f, Rad * 0.5f), FVector(-Rad * 0.15f, -Sd * Rad * 0.08f, -Rad * 0.05f));
			}), SkGSoftWing)) Groups.Last().Side = Sd;
		}
		AddGroup(HeadBone, Head, Yaw, Blocks([&](FKit& Kit)
		{
			Kit.Add(FSurf::Glaze(ScHex(TEXT("C8261E")) * 0.62f, 0.3f), FVector(Rad * 0.32f, Rad * 0.16f, Rad * 0.2f), FVector(Rad * 0.28f, 0.f, -Rad * 0.02f));
			Kit.Add(FSurf::Glaze(ScHex(TEXT("C8261E")) * 0.62f, 0.3f), FVector(Rad * 0.18f, Rad * 0.18f, Rad * 0.12f), FVector(Rad * 0.14f, 0.f, Rad * 0.14f));
		}), SkGStatic);
		AddGroup(SpB, Sp + Fwd * Rad * 0.55f, Yaw, Blocks([&](FKit& Kit) { Kit.Add(FSurf::Cloth(ScHex(TEXT("3D6FB0")) * 0.62f), FVector(Rad * 0.2f, Rad * 0.8f, Rad * 0.7f), FVector::ZeroVector, FRotator::ZeroRotator, EWRLookMesh::Cylinder); }), SkGStatic);
	}
	else if (K == TEXT("gallowshrike"))
	{
		// A shrike's head: the black bandit mask over the eyes, white throat, a hooked beak with its tomial tooth stained
		// dark; short stiff black wings; the rust-red under the tail; a 12 cm black dagger claw on every inner toe.
		const float U = 26.f;
		AddGroup(HeadBone, Head, Yaw, Blocks([&](FKit& Kit)
		{
			Kit.Add(S(TEXT("0E0E0E")), FVector(U * 0.7f, U * 1.25f, U * 0.28f), FVector(U * 0.05f, 0.f, U * 0.12f));
			Kit.Add(S(TEXT("ECEAE4"), 0.66f), FVector(U * 0.6f, U * 0.8f, U * 0.4f), FVector(U * 0.0f, 0.f, -U * 0.35f));
			Kit.Spike(FSurf::Glaze(ScHex(TEXT("2A2522")) * 0.6f, 0.3f), FVector(U * 0.45f, 0.f, 0.f), FVector(1.f, 0.f, -0.25f), U * 0.32f, U * 0.75f);
			Kit.Spike(FSurf::Glaze(ScHex(TEXT("2A2522")) * 0.6f, 0.3f), FVector(U * 1.1f, 0.f, -U * 0.18f), FVector(0.2f, 0.f, -1.f), U * 0.12f, U * 0.22f);
			for (const float Sd : { -1.f, 1.f }) Kit.Add(FSurf::Glaze(ScHex(TEXT("6A6A6A")) * 0.5f, 0.6f), FVector(U * 0.16f, U * 0.05f, U * 0.16f), FVector(U * 0.2f, Sd * U * 0.62f, U * 0.13f));
		}), SkGStatic);
		FVector Sp = C->GetActorLocation() + FVector(0, 0, HH * 0.2f);
		const FName SpB = BoneAt(TEXT("spine2"), Sp);
		for (const float Sd : { -1.f, 1.f })
		{
			if (AddGroup(SpB, Sp + Right * Sd * Rad * 0.6f, FRotator(0.f, Yaw.Yaw, 0.f), Blocks([&](FKit& Kit)
			{
				for (int32 f = 0; f < 6; ++f) Kit.Add(S(TEXT("101010")), FVector(U * (1.6f - f * 0.12f), U * 0.08f, U * 0.25f), FVector(-U * 0.5f - f * U * 0.05f, Sd * U * 0.04f * f, -f * U * 0.12f), FRotator(-15.f, 0.f, 0.f));
			}), SkGShrikeWing)) Groups.Last().Side = Sd;
		}
		FVector Tl = C->GetActorLocation() - Fwd * Rad;
		const FName TlB = BoneAt(TEXT("tail1"), Tl);
		AddGroup(TlB, Tl, Yaw, Blocks([&](FKit& Kit) { Kit.Add(S(TEXT("9A4A2A")), FVector(U * 0.8f, U * 0.5f, U * 0.25f), FVector(0.f, 0.f, -U * 0.25f)); }), SkGStatic);
		for (const FName& F : BonesWith(TEXT("foot")))
		{
			const FVector P = Body->GetBoneLocation(F);
			AddGroup(F, P, Yaw, Blocks([&](FKit& Kit) { Kit.Spike(FSurf::Glaze(FLinearColor(0.01f, 0.01f, 0.01f), 0.2f), FVector(U * 0.2f, 0.f, 0.f), FVector(1.f, 0.f, -0.35f), 4.f, 12.f); }), SkGStatic);
		}
	}
	else if (K == TEXT("hushwing"))
	{
		// The frogmouth head: wide and flat with a pale facial disc and huge amber eyes; the gape's pink-black lining (the
		// lower plate drops when it splits open); fringed soft edges along the wings.
		const float U = 40.f;
		AddGroup(HeadBone, Head, Yaw, Blocks([&](FKit& Kit)
		{
			Kit.Add(S(TEXT("4A3E33")), FVector(U * 0.9f, U * 1.5f, U * 0.45f), FVector(U * 0.1f, 0.f, 0.f));
			Kit.Disc(FSurf::Cloth(ScHex(TEXT("D9D3C7")) * 0.66f), FVector(U * 0.55f, 0.f, U * 0.05f), FVector(1.f, 0.f, 0.f), U * 1.2f, 2.f);
			for (const float Sd : { -1.f, 1.f })
			{
				Kit.Add(FSurf::Lit(ScHex(TEXT("E8A33A")) * 0.6f, 2.f), FVector(U * 0.2f), FVector(U * 0.58f, Sd * U * 0.3f, U * 0.12f));
				Kit.Add(FSurf::Glaze(FLinearColor(0.01f, 0.01f, 0.01f), 0.1f), FVector(U * 0.08f, U * 0.08f, U * 0.14f), FVector(U * 0.68f, Sd * U * 0.3f, U * 0.12f));
			}
		}), SkGDisc);
		AddGroup(HeadBone, Head, Yaw, Blocks([&](FKit& Kit)
		{
			Kit.Add(S(TEXT("3E342B")), FVector(U * 0.85f, U * 1.45f, U * 0.12f), FVector(U * 0.45f, 0.f, -U * 0.2f));
			Kit.Add(FSurf::Glaze(ScHex(TEXT("5A2A35")) * 0.6f, 0.5f), FVector(U * 0.8f, U * 1.35f, 2.f), FVector(U * 0.45f, 0.f, -U * 0.13f));
		}), SkGGape);
		for (const TCHAR* Wb : { TEXT("wing_L"), TEXT("wing_R") })
		{
			FVector Wp;
			const FName WB = BoneAt(Wb, Wp);
			if (WB.IsNone()) continue;
			AddGroup(WB, Wp, Yaw, Blocks([&](FKit& Kit)
			{
				for (int32 f = 0; f < 14; ++f) Kit.Add(FSurf::Cloth(ScHex(f % 2 ? TEXT("8E8A83") : TEXT("5C5148")) * 0.6f), FVector(14.f, 6.f, 2.f), FVector(-Rad * 0.6f - (f % 3) * 4.f, (f - 7) * 20.f, 0.f), FRotator(0.f, (f - 7) * 4.f, 0.f));
			}), SkGStatic);
		}
	}
}

void UWRSkyBody::TickLook(float DeltaTime)
{
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	if (!C) return;
	if (LookFor != C->CreatureId)
	{
		// The rig and its materials arrive a frame or two after the creature (OnRep): try for a few seconds.
		const USkeletalMeshComponent* Body = Skel();
		const FWRCreatureDef* Def = C->GetDef();
		const bool bProc = Def && !Def->ProcBody.IsNone();
		if (LookTries < 40 && (bProc || (Body && Body->GetSkeletalMeshAsset()))) { ++LookTries; BuildLook(); }
		if (LookFor != C->CreatureId) return;
	}
	// Wings: the Windmane's four beat in flight (fore and hind out of phase), fold back along its body on the ground; the
	// Puffwing's spread for its parachute; the shrike's spread to hop-glide; the crest flares; the mane streams.
	const bool bFly = Has(WRSkyFlag::Flying) || (C->GetCharacterMovement() && C->GetCharacterMovement()->IsFlying());
	Beat = FMath::FInterpTo(Beat, bFly ? 1.f : 0.f, DeltaTime, 2.5f);
	const float Speed = C->GetVelocity().Size();
	for (FGroup& G : Groups)
	{
		USceneComponent* F = G.Frame.Get();
		if (!F) continue;
		if (G.Kind == SkGWingF || G.Kind == SkGWingH)
		{
			const bool bHind = G.Kind == SkGWingH;
			const float Flap = Beat * (bHind ? 38.f : 52.f) * FMath::Sin(Clock * 2.f * PI * (bHind ? 1.3f : 1.15f) + (bHind ? PI : 0.f));
			const float Fold = (1.f - Beat) * (bHind ? 70.f : 78.f);
			const float Hood = Has(WRSkyFlag::Hood) && bHind ? 50.f : 0.f;
			F->SetRelativeRotation(G.BaseRot + FRotator(0.f, -G.Side * (Fold - Hood), G.Side * (Flap + (1.f - Beat) * 18.f)));
		}
		else if (G.Kind == SkGSoftWing)
		{
			const float Spread = Has(WRSkyFlag::Parachute) ? 75.f : 6.f + 4.f * FMath::Sin(Clock * 1.4f + G.Phase);
			F->SetRelativeRotation(FMath::RInterpTo(F->GetRelativeRotation(), G.BaseRot + FRotator(0.f, 0.f, G.Side * Spread), DeltaTime, 6.f));
		}
		else if (G.Kind == SkGShrikeWing)
		{
			const float Spread = Has(WRSkyFlag::Flap) || Has(WRSkyFlag::Holding) ? 70.f : 0.f;
			F->SetRelativeRotation(FMath::RInterpTo(F->GetRelativeRotation(), G.BaseRot + FRotator(0.f, 0.f, G.Side * Spread), DeltaTime, 8.f));
		}
		else if (G.Kind == SkGCrest)
		{
			const float Flare = Has(WRSkyFlag::Crest) || Has(WRSkyFlag::Stoop) ? 1.f : 0.f;
			F->SetRelativeRotation(FMath::RInterpTo(F->GetRelativeRotation(), G.BaseRot + FRotator(35.f * Flare, 0.f, 0.f), DeltaTime, 10.f));
			F->SetRelativeScale3D(G.BaseScale * (1.f + 0.25f * Flare));
		}
		else if (G.Kind == SkGMane)
		{
			const float Stream = FMath::Clamp(Speed / 1200.f, 0.f, 1.f);
			F->SetRelativeRotation(G.BaseRot + FRotator(-25.f * Stream, 0.f, 6.f * FMath::Sin(Clock * (2.f + 4.f * Stream) + G.Phase)));
		}
		else if (G.Kind == SkGGape)
		{
			F->SetRelativeRotation(FMath::RInterpTo(F->GetRelativeRotation(), G.BaseRot + FRotator(Has(WRSkyFlag::Gape) ? -55.f : 0.f, 0.f, 0.f), DeltaTime, 9.f));
		}
		else if (G.Kind == SkGDisc)
		{
			// The facial disc flashes white as it strikes: the head swells a touch on the swoop.
			F->SetRelativeScale3D(FMath::VInterpTo(F->GetRelativeScale3D(), G.BaseScale * (Has(WRSkyFlag::Stoop) ? 1.12f : 1.f), DeltaTime, 10.f));
		}
		else if (G.Kind == SkGVanes)
		{
			// The halo turns with its walk; stopped and pointed down for the slam (Hood); whining fast for the spin (Flap).
			const float Rate = Has(WRSkyFlag::Hood) ? 0.f : Has(WRSkyFlag::Flap) ? 720.f : 25.f + Speed * 0.15f;
			G.Phase += DeltaTime * Rate;
			F->SetRelativeRotation(G.BaseRot + FRotator(0.f, 0.f, Has(WRSkyFlag::Hood) ? 0.f : FMath::Fmod(G.Phase, 360.f)));
		}
		else if (G.Kind == SkGTail)
		{
			F->SetRelativeRotation(G.BaseRot + FRotator(0.f, 6.f * FMath::Sin(Clock * 3.f + G.Phase), 0.f));
		}
	}
}
