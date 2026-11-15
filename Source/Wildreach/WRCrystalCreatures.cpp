// The Crystal caves' creatures (see WRCrystalCreatures.h; cooks/biome-crystal.md 2.12, 2.12.4).

#include "WRCrystalCreatures.h"
#include "WRArcanePath.h"
#include "WRCrystalTypes.h"
#include "WRCrystalWorld.h"
#include "WRCrystalThings.h"
#include "Wildreach.h"
#include "WRAttackRows.h"
#include "WRCharacterBase.h"
#include "WRCharacter.h"
#include "WRPlayerController.h"
#include "GameFramework/PlayerState.h"
#include "WRCombatLibrary.h"
#include "WRCreature.h"
#include "WRBoss.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRGameState.h"
#include "WRPlayerState.h"
#include "WRResourceNode.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRSurvival.h"
#include "WRToolMesh.h"
#include "WRTuning.h"
#include "WRWorldSystems.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

namespace WRCrystalCrImpl
{
	float CcT(const TCHAR* Key, float Def) { return WRTune(*FString::Printf(TEXT("crystalLife.%s"), Key), Def); }
	void CzCrTell(const AActor* Pawn, const FString& Text)
	{
		const APawn* P = Cast<APawn>(Pawn);
		if (AWRPlayerController* PC = P ? Cast<AWRPlayerController>(P->GetController()) : nullptr) PC->ClientNotify(Text);
	}
	double CcNow(const UObject* Ctx) { const UWorld* W = Ctx ? Ctx->GetWorld() : nullptr; return W ? W->GetTimeSeconds() : 0.0; }
	int32 CcBit(uint8 F) { for (int32 i = 0; i < 8; ++i) if (F == (1 << i)) return i; return 0; }
	AWRCharacterBase* CcOwnerOf(const AWRCreature* Self)
	{
		if (!Self || !Self->IsTamed()) return nullptr;
		for (FConstPlayerControllerIterator It = Self->GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			const APlayerController* PC = It->Get();
			if (PC && PC->PlayerState && PC->PlayerState->GetPlayerName() == Self->TamedBy) return Cast<AWRCharacterBase>(PC->GetPawn());
		}
		return nullptr;
	}
	/** A den's table (tuning crystalLife.dens.<zone>.<day|night>). */
	TSharedPtr<FJsonObject> CcTable(FName Zone, bool bNight)
	{
		return FWRTuning::Get().Object(FString::Printf(TEXT("crystalLife.dens.%s.%s"), *Zone.ToString(), bNight ? TEXT("night") : TEXT("day")));
	}
	/** A creatures.json row as JSON (its look's tint), loaded once. */
	TSharedPtr<FJsonObject> CcRow(FName Id)
	{
		static TMap<FName, TSharedPtr<FJsonObject>> Rows;
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
					FString RowId;
					if (O.IsValid() && O->TryGetStringField(TEXT("id"), RowId) && !Rows.Contains(FName(*RowId))) Rows.Add(FName(*RowId), O);
				}
			}
		}
		const TSharedPtr<FJsonObject>* R = Rows.Find(Id);
		return R ? *R : nullptr;
	}
	/** A quartz point (the same habit as the linings), vertex alpha the glow weight. */
	void CcPoint(FWRToolMesh& M, const FVector& Root, const FVector& Dir, float Len, float Thick, const FLinearColor& Base, const FLinearColor& Tip)
	{
		FVector AY, AZ;
		Dir.FindBestAxisVectors(AY, AZ);
		const FVector Sh = Root + Dir * Len * 0.8f, Tp = Root + Dir * Len;
		FVector R0[6], R1[6];
		for (int32 k = 0; k < 6; ++k) { const float A = k * PI / 3.f; const FVector O = (AY * FMath::Cos(A) + AZ * FMath::Sin(A)) * Thick * 0.5f; R0[k] = Root + O * 0.9f; R1[k] = Sh + O; }
		const FColor C0 = FWRToolMesh::Pack(Base, 0.3f), C1 = FWRToolMesh::Pack(Tip, 1.f);
		const FVector2D Z = FVector2D::ZeroVector;
		for (int32 k = 0; k < 6; ++k) { const int32 n = (k + 1) % 6; M.Tri(R0[k], R1[n], R1[k], C0, Z, Z, Z); M.Tri(R0[k], R0[n], R1[n], C0, Z, Z, Z); M.Tri(R1[k], R1[n], Tp, C1, Z, Z, Z); }
	}
	/** A flat plate (a lozenge with thickness) facing Normal, Long along Along. */
	void CcPlate(FWRToolMesh& M, const FVector& C, const FVector& Along, const FVector& Normal, float Long, float Wide, float Thick, const FLinearColor& Face, const FLinearColor& Edge)
	{
		const FVector Side = FVector::CrossProduct(Normal, Along).GetSafeNormal();
		TArray<TArray<FVector>> Rings;
		for (const float T : { -0.5f, 0.5f })
		{
			const FVector O = C + Normal * Thick * T;
			Rings.Add({ O + Along * Long * 0.5f, O + Side * Wide * 0.5f, O - Along * Long * 0.5f, O - Side * Wide * 0.5f });
		}
		M.Loft(Rings, Face, 0.15f, true);
		(void)Edge;
	}
}
using namespace WRCrystalCrImpl;

// ================================================================================================ UWRCrystalBody

UWRCrystalBody::UWRCrystalBody()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UWRCrystalBody::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UWRCrystalBody, Flags);
	DOREPLIFETIME(UWRCrystalBody, LegsBroken);
	DOREPLIFETIME(UWRCrystalBody, LatchedOn);
	DOREPLIFETIME(UWRCrystalBody, PathTamer);
}

UWRCrystalBody* UWRCrystalBody::Get(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UWRCrystalBody>() : nullptr;
}

bool UWRCrystalBody::IsCrystalKind(FName CreatureId)
{
	const FWRCreatureDef* D = FWRCreatureRegistry::Get().Find(CreatureId);
	return D && D->Biome == WRCrystal::Biome();
}

void UWRCrystalBody::Attach(AWRCreature* Creature)
{
	if (!Creature || !Creature->HasAuthority() || Cast<AWRBoss>(Creature) || !IsCrystalKind(Creature->CreatureId) || Get(Creature)) return;
	// The Twins' double and Lyrathis's copies wear their boss's body (UWRCrystalBossBody), not a creature's.
	if (Creature->CreatureId == TEXT("mirrored_twin_double") || Creature->CreatureId == TEXT("lyrathis_copy")) return;
	UWRCrystalBody* B = NewObject<UWRCrystalBody>(Creature, TEXT("CrystalBody"));
	B->RegisterComponent();
	for (float& H : B->LegHP) H = CcT(TEXT("spindleglass.legHealth"), 120.f);
	// The hearing hunters listen to the caves (crystal 2.12: Echobats hunt only noise, a Spindleglass only footsteps).
	if (Creature->CreatureId == TEXT("echobat") || Creature->CreatureId == TEXT("spindleglass"))
	{
		if (UWRCrystalWorld* W = UWRCrystalWorld::Get(Creature))
		{
			TWeakObjectPtr<AWRCreature> Weak(Creature);
			B->Hearing = W->OnNoise.AddLambda([Weak](const FVector& At, float R, AActor* By, FName Kind) { if (Weak.IsValid()) FWRCrystalAI::Heard(Weak.Get(), At, R, By, Kind); });
			if (Creature->CreatureId == TEXT("echobat"))
			{
				// A Resonance peal deafens every Echobat for 30 s as it passes (crystal 2.10).
				W->OnPeal.AddWeakLambda(B, [Weak](int32, const FVector& Source, float Speed)
				{
					AWRCreature* C = Weak.Get();
					UWRCrystalBody* Bd = Get(C);
					if (!C || !Bd) return;
					const double At = CcNow(C) + FVector::Dist(C->GetActorLocation(), Source) / FMath::Max(Speed, 1.f);
					Bd->Set(WRCrystalFlag::Deaf, true);
					Bd->Until[CcBit(WRCrystalFlag::Deaf)] = At + CcT(TEXT("echobat.deafSeconds"), 30.f);
				});
			}
		}
	}
}

void UWRCrystalBody::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Hearing.IsValid()) if (UWRCrystalWorld* W = UWRCrystalWorld::Get(GetOwner())) W->OnNoise.Remove(Hearing);
	Super::EndPlay(Reason);
}

void UWRCrystalBody::Set(uint8 F, bool bOn)
{
	const uint8 Was = Flags;
	Flags = bOn ? (Flags | F) : (Flags & ~F);
	if (Flags != Was) { OnRep_Flags(); if (AActor* O = GetOwner()) O->ForceNetUpdate(); }
}

void UWRCrystalBody::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() != NM_DedicatedServer) BuildLook();
}

void UWRCrystalBody::OnRep_Flags()
{
	// The looks that follow a flag: an Alicanto dims, a Spindleglass clouds, a Lumenhorn's lamp, a broken leg hidden.
	const AWRCreature* C = Cast<AWRCreature>(GetOwner());
	if (!C) return;
	for (UMaterialInstanceDynamic* G : Glows)
	{
		if (!G) continue;
		float Rest = 0.35f;
		if (C->CreatureId == TEXT("alicanto")) Rest = Has(WRCrystalFlag::Dim) ? 0.02f : 0.9f;
		if (C->CreatureId == TEXT("spindleglass")) Rest = Has(WRCrystalFlag::Hunting) ? 0.6f : 0.2f;
		if (C->CreatureId == TEXT("lumenhorn") || C->CreatureId == TEXT("gemsniffer")) Rest = 0.8f;
		G->SetScalarParameterValue(TEXT("Rest"), Rest);
	}
	if (Lamp) Lamp->SetIntensity(C->CreatureId == TEXT("alicanto") && Has(WRCrystalFlag::Dim) ? 0.f : Lamp->Intensity > 0.f ? Lamp->Intensity : CcT(TEXT("lampPower"), 1800.f));
	for (int32 i = 0; i < Legs.Num(); ++i)
	{
		const bool bGone = (LegsBroken & (1 << i)) != 0;
		if (Legs[i].Upper.IsValid()) Legs[i].Upper->SetVisibility(!bGone);
		if (Legs[i].Lower.IsValid()) Legs[i].Lower->SetVisibility(!bGone);
	}
}

FName UWRCrystalBody::BoneLike(const TCHAR* Hint) const
{
	const AWRCreature* C = Cast<AWRCreature>(GetOwner());
	const USkeletalMeshComponent* S = C ? C->GetCreatureSkel() : nullptr;
	if (!S || !S->GetSkeletalMeshAsset()) return NAME_None;
	const FReferenceSkeleton& Ref = S->GetSkeletalMeshAsset()->GetRefSkeleton();
	const FString H = FString(Hint).ToLower();
	for (int32 i = 0; i < Ref.GetNum(); ++i) if (Ref.GetBoneName(i).ToString().ToLower() == H) return Ref.GetBoneName(i);
	for (int32 i = 0; i < Ref.GetNum(); ++i) if (Ref.GetBoneName(i).ToString().ToLower().Contains(H)) return Ref.GetBoneName(i);
	return NAME_None;
}

UProceduralMeshComponent* UWRCrystalBody::Part(FName Bone, const FTransform& Local, FWRToolMesh& M, bool bGlow)
{
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	USkeletalMeshComponent* S = C ? C->GetCreatureSkel() : nullptr;
	if (!S) return nullptr;
	UProceduralMeshComponent* P = NewObject<UProceduralMeshComponent>(C);
	P->SetupAttachment(S, Bone);
	P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	P->SetUsingAbsoluteScale(true);
	P->RegisterComponent();
	// Parts are modelled in the actor's frame at the creature's scale: place them in world space from the bone, upright.
	const FVector BoneAt = Bone.IsNone() ? C->GetActorLocation() : S->GetSocketLocation(Bone);
	P->SetWorldLocationAndRotation(BoneAt + C->GetActorRotation().RotateVector(Local.GetLocation()), C->GetActorRotation() + Local.Rotator());
	P->SetWorldScale3D(FVector::OneVector);
	M.ApplyTo(P);
	if (bGlow)
	{
		if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_CrystalGlow.M_CrystalGlow")))
		{
			UMaterialInstanceDynamic* G = UMaterialInstanceDynamic::Create(Mat, P);
			G->SetVectorParameterValue(TEXT("GlowColor"), FLinearColor(0.62f, 0.42f, 1.f));
			G->SetScalarParameterValue(TEXT("GlowScale"), WRTune(TEXT("crystalWorld.glow.scale"), 6.f));
			G->SetScalarParameterValue(TEXT("Rest"), 0.35f);
			P->SetMaterial(0, G);
			Glows.Add(G);
		}
	}
	Parts.Add(P);
	return P;
}

void UWRCrystalBody::ApplyTint()
{
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	USkeletalMeshComponent* Skel = C ? C->GetCreatureSkel() : nullptr;
	UMaterialInterface* Master = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Creatures/M_CreatureTint.M_CreatureTint"));
	const FWRCreatureDef* D = C ? C->GetDef() : nullptr;
	if (!Skel || !Master || !D) return;
	const TSharedPtr<FJsonObject> Row = CcRow(C->CreatureId);
	const TSharedPtr<FJsonObject>* Look = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Tints = nullptr;
	if (!Row.IsValid() || !Row->TryGetObjectField(TEXT("look"), Look) || !(*Look)->TryGetArrayField(TEXT("tint"), Tints) || Tints->Num() == 0) return;
	const TSharedPtr<FJsonObject> Pick = (*Tints)[0]->AsObject();
	const auto Col = [&](const TCHAR* F, const FLinearColor& Def) { FString S; return Pick->TryGetStringField(F, S) ? FLinearColor(FColor::FromHex(S)) : Def; };
	const auto Num = [&](const TCHAR* F, float Def) { double V; return Pick->TryGetNumberField(F, V) ? float(V) : Def; };
	for (int32 Slot = 0; Slot < Skel->GetNumMaterials(); ++Slot)
	{
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Master, C);
		FString Tex;
		if (Pick->TryGetStringField(TEXT("tex"), Tex)) if (UTexture* T = LoadObject<UTexture>(nullptr, *Tex)) M->SetTextureParameterValue(TEXT("BaseColorTex"), T);
		if (UTexture* N = LoadObject<UTexture>(nullptr, TEXT("/Engine/EngineMaterials/DefaultNormal.DefaultNormal"))) M->SetTextureParameterValue(TEXT("NormalTex"), N);
		if (UTexture* B = LoadObject<UTexture>(nullptr, TEXT("/Engine/EngineResources/Black.Black"))) M->SetTextureParameterValue(TEXT("GlowTex"), B);
		M->SetScalarParameterValue(TEXT("Desaturation"), Num(TEXT("desat"), 1.f));
		M->SetScalarParameterValue(TEXT("Brightness"), Num(TEXT("bright"), 1.f));
		M->SetScalarParameterValue(TEXT("TwoTone"), Num(TEXT("twoTone"), 0.f));
		M->SetScalarParameterValue(TEXT("LightFrom"), Num(TEXT("lightFrom"), 0.f));
		M->SetScalarParameterValue(TEXT("Roughness"), Num(TEXT("rough"), 0.75f));
		M->SetScalarParameterValue(TEXT("GlowStrength"), Num(TEXT("glowStrength"), 0.f));
		M->SetVectorParameterValue(TEXT("Tint"), Col(TEXT("tint"), FLinearColor::White));
		M->SetVectorParameterValue(TEXT("TintLight"), Col(TEXT("light"), Col(TEXT("tint"), FLinearColor::White)));
		M->SetVectorParameterValue(TEXT("GlowColor"), Col(TEXT("glow"), FLinearColor(0.85f, 0.8f, 1.f)));
		Skel->SetMaterial(Slot, M);
	}
}

void UWRCrystalBody::BuildLook()
{
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	const FWRCreatureDef* D = C ? C->GetDef() : nullptr;
	USkeletalMeshComponent* Skel = C ? C->GetCreatureSkel() : nullptr;
	if (!D || !Skel || !Skel->GetSkeletalMeshAsset()) return;
	bBuilt = true;
	ApplyTint();
	const FName Id = C->CreatureId;
	const float R = D->Radius, H = D->HalfHeight;
	const FLinearColor Violet(FColor(107, 63, 160)), Lilac(FColor(216, 208, 255)), PlateC(FColor(184, 160, 232)), Basalt(FColor(28, 28, 32));
	FRandomStream Rng(static_cast<int32>(GetTypeHash(C->GetName())));
	if (Id == TEXT("prismback"))
	{
		// Four rows of tilted mirror-plates down the spine (crystal face, lilac horn edge), the biggest over the shoulders.
		FWRToolMesh M;
		for (int32 r = 0; r < 4; ++r) for (const float S : { -1.f, 1.f })
		{
			const float X = R * (0.9f - r * 0.55f), Hgt = (r == 0 ? 160.f : 130.f - r * 15.f) * 0.5f;
			const FVector Normal = FVector(0.f, S, 0.25f).GetSafeNormal();
			CcPlate(M, FVector(X, S * 22.f, Hgt * 0.8f), FVector(0.15f, -S * 0.3f, 1.f).GetSafeNormal(), Normal, Hgt * 1.6f, 70.f - r * 8.f, 6.f, PlateC, Lilac);
		}
		Part(BoneLike(TEXT("spine2")), FTransform(FVector(0.f, 0.f, 10.f)), M, true);
	}
	else if (Id == TEXT("spindleglass"))
	{
		// The body hung 3 m up (the rig's own body, lifted), eight long banded legs to planted feet, a dark heart.
		BodyLift = CcT(TEXT("spindleglass.bodyLift"), 280.f);
		Skel->AddRelativeLocation(FVector(0.f, 0.f, BodyLift));
		FWRToolMesh Heart;
		Heart.Lathe(FVector(-12.f, 0.f, 0.f), FVector(1.f, 0.f, 0.f), 24.f, { FVector(0.f, 0.f, 0.f), FVector(0.5f, 14.f, 12.f), FVector(1.f, 0.f, 0.f) }, 10, FLinearColor(FColor(74, 42, 58)) * 0.5f, 0.4f);
		Part(BoneLike(TEXT("body")), FTransform(FVector(0.f, 0.f, 0.f)), Heart, false);
		for (int32 i = 0; i < 8; ++i)
		{
			FLeg L;
			L.Angle = (i < 4 ? 30.f + i * 35.f : -30.f - (i - 4) * 35.f);
			for (int32 s = 0; s < 2; ++s)
			{
				// A unit leg segment along +X (scaled to length), selenite white with grey growth bands.
				FWRToolMesh Seg;
				TArray<FVector> Prof;
				for (int32 k = 0; k <= 6; ++k) Prof.Add(FVector(k / 6.f, 4.f - k * 0.3f, 4.f - k * 0.3f));
				Seg.Lathe(FVector::ZeroVector, FVector(1.f, 0.f, 0.f), 100.f, Prof, 6, FLinearColor(FColor(242, 240, 232)) * 0.62f, 0.3f);
				for (int32 b = 1; b < 5; ++b) { TArray<FVector> Band; for (int32 k = 0; k <= 8; ++k) { const float A = k * PI / 4.f; Band.Add(FVector(b * 20.f, FMath::Cos(A) * 4.3f, FMath::Sin(A) * 4.3f)); } Seg.Tube(Band, 0.8f, 4, FLinearColor(FColor(150, 150, 156)) * 0.5f, 0.6f, true); }
				UProceduralMeshComponent* P = NewObject<UProceduralMeshComponent>(C);
				P->SetupAttachment(C->GetRootComponent());
				P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				P->SetUsingAbsoluteLocation(true);
				P->SetUsingAbsoluteRotation(true);
				P->SetUsingAbsoluteScale(true);
				P->RegisterComponent();
				Seg.ApplyTo(P);
				(s == 0 ? L.Upper : L.Lower) = P;
				Parts.Add(P);
			}
			const FVector Dir = FRotator(0.f, C->GetActorRotation().Yaw + L.Angle, 0.f).Vector();
			L.Foot = L.From = L.To = C->GetActorLocation() - FVector(0.f, 0.f, H) + Dir * CcT(TEXT("spindleglass.reach"), 420.f);
			Legs.Add(L);
		}
	}
	else if (Id == TEXT("echobat"))
	{
		// Ears as long as its head (pale membrane with lilac veins) and a wrinkled leaf-nose.
		FWRToolMesh M;
		for (const float S : { -1.f, 1.f }) CcPlate(M, FVector(-6.f, S * 14.f, 34.f), FVector(-0.2f, S * 0.35f, 1.f).GetSafeNormal(), FVector(0.f, S, 0.f), 70.f, 34.f, 2.f, FLinearColor(FColor(232, 224, 216)) * 0.6f, Lilac);
		CcPlate(M, FVector(22.f, 0.f, 8.f), FVector(0.f, 0.f, 1.f), FVector(1.f, 0.f, 0.f), 18.f, 12.f, 2.f, FLinearColor(FColor(200, 180, 176)) * 0.55f, Lilac);
		Part(BoneLike(TEXT("head")), FTransform::Identity, M, false);
	}
	else if (Id == TEXT("geodemite"))
	{
		// The honeypot: a clear violet abdomen of crystal behind the body.
		FWRToolMesh M;
		for (int32 i = 0; i < 7; ++i) CcPoint(M, FVector(-10.f, 0.f, 10.f), FVector(-1.f, Rng.FRandRange(-0.6f, 0.6f), Rng.FRandRange(-0.2f, 0.7f)).GetSafeNormal(), Rng.FRandRange(14.f, 22.f), 12.f, FLinearColor(FColor(138, 96, 200)), Lilac);
		Part(BoneLike(TEXT("body")), FTransform(FVector(-R * 0.4f, 0.f, 0.f)), M, true);
	}
	else if (Id == TEXT("lumenhorn"))
	{
		// A palmate spread 3.2 m across: two flat palms of crystal over bone, each fringed with tines, lilac-white.
		FWRToolMesh M;
		for (const float S : { -1.f, 1.f })
		{
			const FVector Base(0.f, S * 15.f, 20.f), Out = FVector(0.15f, S, 0.55f).GetSafeNormal();
			CcPoint(M, Base, Out, 90.f, 14.f, Violet, Lilac);
			CcPlate(M, Base + Out * 110.f, FVector(0.f, S * 0.4f, 1.f).GetSafeNormal(), FVector(0.3f, 0.f, 0.f), 110.f, 60.f, 5.f, Violet, Lilac);
			for (int32 t = 0; t < 7; ++t) CcPoint(M, Base + Out * (80.f + t * 12.f) + FVector(0.f, 0.f, 30.f + t * 6.f), FVector(Rng.FRandRange(-0.2f, 0.3f), S * Rng.FRandRange(0.2f, 0.8f), 1.f).GetSafeNormal(), Rng.FRandRange(30.f, 55.f), 7.f, Violet, Lilac);
		}
		Part(BoneLike(TEXT("head")), FTransform::Identity, M, true);
		// The rider light (15 m, steady and silent).
		Lamp = NewObject<UPointLightComponent>(C);
		Lamp->SetupAttachment(Skel, BoneLike(TEXT("head")));
		Lamp->SetRelativeLocation(FVector(0.f, 0.f, 80.f));
		Lamp->SetLightColor(Lilac);
		Lamp->SetAttenuationRadius(CcT(TEXT("lumenhorn.lightRadius"), 1500.f));
		Lamp->SetIntensity(CcT(TEXT("lampPower"), 1800.f));
		Lamp->SetCastShadows(false);
		Lamp->RegisterComponent();
	}
	else if (Id == TEXT("druseback"))
	{
		// Seven back segments, each a split geode: basalt plates with violet crystal at every seam.
		FWRToolMesh Rock, Seams;
		for (int32 s = 0; s < 7; ++s)
		{
			const float X = R * (0.85f - s * 0.28f);
			TArray<FVector> Arch;
			for (int32 k = 0; k <= 10; ++k) { const float A = k * PI / 10.f; Arch.Add(FVector(X, FMath::Cos(A) * R * 0.75f, FMath::Sin(A) * H * 0.8f + H * 0.4f)); }
			Rock.Tube(Arch, 18.f, 6, Basalt * 0.8f, 0.85f);
			for (int32 k = 1; k < 10; k += 2) { const float A = k * PI / 10.f; CcPoint(Seams, FVector(X - 18.f, FMath::Cos(A) * R * 0.75f, FMath::Sin(A) * H * 0.8f + H * 0.4f), FVector(-0.3f, FMath::Cos(A), FMath::Sin(A)).GetSafeNormal(), 22.f, 9.f, Violet, Lilac); }
		}
		Part(NAME_None, FTransform(FVector(0.f, 0.f, -H)), Rock, false);
		Part(NAME_None, FTransform(FVector(0.f, 0.f, -H)), Seams, true);
	}
	else if (Id == TEXT("gemsniffer"))
	{
		// The carbunclo's brow gem: amber-violet, glowing.
		FWRToolMesh M;
		for (int32 i = 0; i < 5; ++i) CcPoint(M, FVector(6.f, 0.f, 10.f), FVector(Rng.FRandRange(-0.3f, 0.6f), Rng.FRandRange(-0.4f, 0.4f), 1.f).GetSafeNormal(), Rng.FRandRange(8.f, 14.f), 7.f, FLinearColor(FColor(200, 120, 60)), Lilac);
		Part(BoneLike(TEXT("head")), FTransform::Identity, M, true);
	}
	else if (Id == TEXT("alicanto"))
	{
		// Wide wing fans held open as it runs, glowing silver-white (a 10 m cold light).
		FWRToolMesh M;
		for (const float S : { -1.f, 1.f }) for (int32 f = 0; f < 9; ++f)
		{
			const float A = FMath::DegreesToRadians(-20.f + f * 14.f);
			CcPlate(M, FVector(-10.f, S * (30.f + FMath::Cos(A) * 40.f), 40.f + FMath::Sin(A) * 40.f), FVector(-0.2f, S * FMath::Cos(A), FMath::Sin(A)).GetSafeNormal(), FVector(1.f, 0.f, 0.f), 70.f, 14.f, 1.5f, FLinearColor(FColor(232, 236, 246)) * 0.62f, Lilac);
		}
		Part(BoneLike(TEXT("spine")), FTransform::Identity, M, true);
		Lamp = NewObject<UPointLightComponent>(C);
		Lamp->SetupAttachment(Skel);
		Lamp->SetRelativeLocation(FVector(0.f, 0.f, H * 1.4f));
		Lamp->SetLightColor(FLinearColor(FColor(232, 238, 255)));
		Lamp->SetAttenuationRadius(CcT(TEXT("alicanto.lightRadius"), 1000.f));
		Lamp->SetIntensity(CcT(TEXT("lampPower"), 1800.f) * 0.7f);
		Lamp->SetCastShadows(false);
		Lamp->RegisterComponent();
	}
	else if (Id == TEXT("chimecrab"))
	{
		// The geode it grew into: a basalt dome on its back, open at the front on a violet druse.
		FWRToolMesh Shell, Druse;
		TArray<FVector> Prof = { FVector(0.f, 40.f, 34.f), FVector(0.4f, 44.f, 38.f), FVector(0.8f, 34.f, 30.f), FVector(1.f, 4.f, 4.f) };
		Shell.Lathe(FVector(0.f, 0.f, 0.f), FVector(0.f, 0.f, 1.f), 60.f, Prof, 12, Basalt * 0.8f, 0.85f);
		for (int32 i = 0; i < 14; ++i) CcPoint(Druse, FVector(30.f, Rng.FRandRange(-24.f, 24.f), Rng.FRandRange(8.f, 40.f)), FVector(1.f, Rng.FRandRange(-0.5f, 0.5f), Rng.FRandRange(-0.3f, 0.5f)).GetSafeNormal(), Rng.FRandRange(8.f, 16.f), 6.f, Violet, Lilac);
		Part(BoneLike(TEXT("body")), FTransform(FVector(-10.f, 0.f, 10.f)), Shell, false);
		Part(BoneLike(TEXT("body")), FTransform(FVector(-10.f, 0.f, 10.f)), Druse, true);
	}
	if (UWRCrystalWorld* W = UWRCrystalWorld::Get(C)) for (UMaterialInstanceDynamic* G : Glows) W->RegisterGlow(G, C->GetActorLocation());
	OnRep_Flags();
}

void UWRCrystalBody::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	if (!C) return;
	if (GetOwner()->HasAuthority())
	{
		// Timed flags run out.
		const double Now = CcNow(C);
		for (const uint8 F : { uint8(WRCrystalFlag::Rolled), uint8(WRCrystalFlag::Deaf), uint8(WRCrystalFlag::Dim), uint8(WRCrystalFlag::Plates), uint8(WRCrystalFlag::Hunting), uint8(WRCrystalFlag::Latched) })
		{
			if (Has(F) && Until[CcBit(F)] > 0.0 && Now >= Until[CcBit(F)]) { Set(F, false); Until[CcBit(F)] = 0.0; if (F == WRCrystalFlag::Latched) LatchedOn = nullptr; }
		}
		// A latched mite rides its victim and marks them (mark 10 a second, crystal 2.12.2).
		if (Has(WRCrystalFlag::Latched))
		{
			AWRCharacterBase* V = Cast<AWRCharacterBase>(LatchedOn);
			if (!V || !V->IsAlive()) { Set(WRCrystalFlag::Latched, false); LatchedOn = nullptr; }
			else
			{
				C->SetActorLocation(V->GetActorLocation() + FVector(0.f, 0.f, 40.f) + FRotator(0.f, float(GetTypeHash(C->GetName()) % 360), 0.f).Vector() * 30.f, false, nullptr, ETeleportType::TeleportPhysics);
				if (V->Status) V->Status->AddBuildup(WRTags::Status_Mark, CcT(TEXT("geodemite.latchMark"), 10.f) * DeltaTime, C);
			}
		}
	}
	if (GetNetMode() == NM_DedicatedServer) return;
	if (!bBuilt && ++Tries < 60) BuildLook();
	if (Legs.Num()) TickLegs(DeltaTime);
}

void UWRCrystalBody::TickLegs(float Dt)
{
	// Each foot stays planted until the body has carried its rest point 1.6 m past it, then steps there in 0.25 s on
	// an arc; opposite legs alternate (a foot only steps when its neighbours are down). The knee rides high above the
	// line from hip to foot, so the legs arch over the body like a harvestman's.
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	const FWRCreatureDef* D = C ? C->GetDef() : nullptr;
	if (!D) return;
	const FVector Hip0 = C->GetActorLocation() + FVector(0.f, 0.f, BodyLift - D->HalfHeight * 0.5f);
	const float Reach = CcT(TEXT("spindleglass.reach"), 420.f), Step = CcT(TEXT("spindleglass.stepDistance"), 160.f), StepTime = CcT(TEXT("spindleglass.stepSeconds"), 0.25f);
	const float Ground = C->GetActorLocation().Z - D->HalfHeight;
	for (int32 i = 0; i < Legs.Num(); ++i)
	{
		FLeg& L = Legs[i];
		if (LegsBroken & (1 << i)) continue;
		const FVector Dir = FRotator(0.f, C->GetActorRotation().Yaw + L.Angle, 0.f).Vector();
		const FVector Rest(C->GetActorLocation().X + Dir.X * Reach, C->GetActorLocation().Y + Dir.Y * Reach, Ground);
		const bool bNeighbourUp = Legs[(i + 1) % Legs.Num()].StepT < 1.f || Legs[(i + Legs.Num() - 1) % Legs.Num()].StepT < 1.f;
		if (L.StepT >= 1.f && FVector::Dist2D(L.Foot, Rest) > Step && !bNeighbourUp) { L.From = L.Foot; L.To = Rest; L.StepT = 0.f; }
		if (L.StepT < 1.f)
		{
			L.StepT = FMath::Min(1.f, L.StepT + Dt / StepTime);
			L.Foot = FMath::Lerp(L.From, L.To, L.StepT) + FVector(0.f, 0.f, FMath::Sin(L.StepT * PI) * 45.f);
			if (L.StepT >= 1.f) WRSound::PlayAt(C, TEXT("crystal.leg_tick"), L.Foot, 0.5f);
		}
		const FVector Hip = Hip0 + Dir * 40.f;
		const FVector Knee = (Hip + L.Foot) * 0.5f + FVector(0.f, 0.f, CcT(TEXT("spindleglass.kneeRise"), 160.f));
		const auto Place = [](UProceduralMeshComponent* P, const FVector& A, const FVector& B)
		{
			if (!P) return;
			const FVector V = B - A;
			P->SetWorldLocationAndRotation(A, V.Rotation());
			P->SetWorldScale3D(FVector(V.Size() / 100.f, 1.f, 1.f));
		};
		Place(L.Upper.Get(), Hip, Knee);
		Place(L.Lower.Get(), Knee, L.Foot);
	}
}

// ================================================================================================ FWRCrystalAI

void FWRCrystalAI::Heard(AWRCreature* Self, const FVector& At, float Radius, AActor* By, FName Kind)
{
	UWRCrystalBody* B = UWRCrystalBody::Get(Self);
	if (!Self || !B || !Self->IsAlive() || Self->IsTamed() || By == Self) return;
	if (Cast<AWRCreature>(By) && !Cast<AWRCreature>(By)->IsTamed()) return;   // wild ones don't hunt each other's noise
	if (Self->CreatureId == TEXT("echobat"))
	{
		if (B->Has(WRCrystalFlag::Deaf)) return;
		// Its hearing: the noise's own radius, x0.7 in the upper Geode Halls in rain (the drip song).
		float Hear = Radius;
		const FWRCrystalData::FHall* H = WRCrystal::HallAt(Self->GetActorLocation());
		if (H && H->Zone == TEXT("C2") && UWRCrystalWorld::Get(Self) && UWRCrystalWorld::Get(Self)->SignatureScale() > 1.01f) Hear *= CcT(TEXT("echobat.rainHearing"), 0.7f);
		if (FVector::Dist(At, Self->GetActorLocation()) > Hear) return;
	}
	else if (Self->CreatureId == TEXT("spindleglass"))
	{
		// It feels the floor, not the air: steps, rolls, landings within 8 m (crystal 2.12.2).
		if (Kind != TEXT("step") && Kind != TEXT("roll") && Kind != TEXT("land")) return;
		if (FVector::Dist2D(At, Self->GetActorLocation()) > FMath::Min(Radius + 400.f, CcT(TEXT("spindleglass.feel"), 800.f))) return;
		B->Set(WRCrystalFlag::Hunting, true);
		B->Until[CcBit(WRCrystalFlag::Hunting)] = CcNow(Self) + 8.0;
	}
	AWRCharacterBase* Who = Cast<AWRCharacterBase>(By);
	if (!Who || !Self->IsHostileTo(Who)) return;
	B->HeardFrom = Who;
	B->HeardAt = CcNow(Self);
	if (!Self->GetAggroTarget()) Self->SetAggroTarget(Who);
	// Saga's "quiet road": an Echobat that comes for you spoils the crossing.
	if (Self->CreatureId == TEXT("echobat")) if (UWRCrystalWorld* W = UWRCrystalWorld::Get(Self)) W->QuietRoadSpoiled(Who);
}

bool FWRCrystalAI::TickWild(AWRCreatureAI* AI, AWRCreature* Self, float DeltaSeconds)
{
	UWRCrystalBody* B = UWRCrystalBody::Get(Self);
	if (!B) return false;
	const double Now = CcNow(Self);
	// Curled into its ball (a Druseback struck hard): it holds still until it uncurls.
	if (B->Has(WRCrystalFlag::Rolled) || B->Has(WRCrystalFlag::Latched))
	{
		Self->GetCharacterMovement()->StopMovementImmediately();
		return true;
	}
	// A path-magic ward under way (crystal 2.12.1): it lies still under the hand; the hand held 4 s finishes it.
	if (!B->PathTamer.IsEmpty())
	{
		AWRCharacter* Tamer = nullptr;
		for (FConstPlayerControllerIterator It = Self->GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			if (It->Get() && It->Get()->PlayerState && It->Get()->PlayerState->GetPlayerName() == B->PathTamer) Tamer = Cast<AWRCharacter>(It->Get()->GetPawn());
		}
		FString Broke;
		const float Reach = Self->GetSimpleCollisionRadius() + WRTune(TEXT("arcane.pathMagic.reach"), 400.f) + 100.f;
		if (!Tamer || !Self->IsAlive() || Self->IsTamed()) Broke = TEXT("The ward breaks");
		else if (FVector::Dist2D(Tamer->GetActorLocation(), Self->GetActorLocation()) > Reach || FVector::Dist2D(Tamer->GetActorLocation(), B->HandAt) > 150.f) Broke = TEXT("You moved: the ward breaks");
		else if (Tamer->IsSwinging() || Tamer->HasTag(WRTags::State_Dodging)) Broke = TEXT("The ward breaks");
		else if (!Self->IsUnconscious() && Self->GetHealthFraction() >= WRTune(TEXT("arcane.pathMagic.below"), 0.3f)) Broke = TEXT("It's too strong: the ward breaks");
		if (!Broke.IsEmpty())
		{
			B->PathTamer.Empty();
			CzCrTell(Tamer, Broke);
			return false;
		}
		Self->SetAggroTarget(nullptr);
		Self->GetCharacterMovement()->StopMovementImmediately();
		if (Now - B->PathTameStart >= WRTune(TEXT("arcane.pathMagic.holdSeconds"), 4.f))
		{
			B->PathTamer.Empty();
			if (!WRArcane::SpendMana(Tamer, WRTune(TEXT("arcane.pathMagic.mana"), 30.f)))
			{
				CzCrTell(Tamer, FString::Printf(TEXT("You haven't the Mana (%.0f) to finish the ward"), WRTune(TEXT("arcane.pathMagic.mana"), 30.f)));
				return true;
			}
			// A full narcotic dose: under, and kept under; taming goes on as knockout and feed.
			Self->AddTorpor(Self->MaxTorpor * 2.f);
			Self->Torpor = Self->MaxTorpor;
			WRArcane::GiveShield(Self, WRTune(TEXT("spells.barrier.shield"), 30.f) * WRArcane::ShieldScale(Tamer), WRTune(TEXT("spells.barrier.seconds"), 8.f), Tamer);
			WRFx::Broadcast(Self, TEXT("arcane.path_magic"), Self->GetActorLocation(), FRotator::ZeroRotator, 1.f, TEXT("arcane.path_magic"));
			CzCrTell(Tamer, FString::Printf(TEXT("A Barrier wraps the %s and its plates go still: feed it to finish the tame"), *Self->GetDef()->Name));
			UE_LOG(LogWildreach, Log, TEXT("CRYSTAL path-magic ward done on %s by %s"), *Self->GetName(), *Tamer->GetName());
		}
		return true;
	}
	// Deaf (a Resonance peal): it drops to roost and its clicks stop; a hunt it was on is forgotten.
	if (B->Has(WRCrystalFlag::Deaf))
	{
		Self->SetAggroTarget(nullptr);
		Self->GetCharacterMovement()->StopMovementImmediately();
		return true;
	}
	// A hearing hunter forgets what it hasn't heard for a while (it can't see it).
	if ((Self->CreatureId == TEXT("echobat") || Self->CreatureId == TEXT("spindleglass")) && Self->GetAggroTarget() && Now - B->HeardAt > CcT(TEXT("forgetSeconds"), 8.f) && Self->SinceHit() > 6.f)
	{
		Self->SetAggroTarget(nullptr);
	}
	// The Alicanto (crystal 2.12.2): if it sees you following (within 15 m, inside its 120 degree view, not crouched) it
	// dims and runs for the nearest drop edge within 60 m, and stops there.
	if (Self->CreatureId == TEXT("alicanto"))
	{
		for (TActorIterator<AWRCharacterBase> It(Self->GetWorld()); It; ++It)
		{
			if (!It->GetPlayerState() || !It->IsAlive() || It->bIsCrouched) continue;
			const FVector To = It->GetActorLocation() - Self->GetActorLocation();
			if (To.Size() > CcT(TEXT("alicanto.sees"), 1500.f) || FVector::DotProduct(To.GetSafeNormal2D(), Self->GetActorForwardVector()) < FMath::Cos(FMath::DegreesToRadians(60.f))) continue;
			if (!B->Has(WRCrystalFlag::Dim))
			{
				B->Set(WRCrystalFlag::Dim, true);
				B->Until[CcBit(WRCrystalFlag::Dim)] = Now + CcT(TEXT("alicanto.dimSeconds"), 12.f);
				// The nearest edge: sample round it for the first point whose ground falls away more than 4 m.
				FVector Best = Self->GetActorLocation() - To.GetSafeNormal2D() * 3000.f;
				for (int32 a = 0; a < 16; ++a)
				{
					const FVector Dir = FRotator(0.f, a * 22.5f, 0.f).Vector();
					for (float Dd = 600.f; Dd <= CcT(TEXT("alicanto.edgeSearch"), 6000.f); Dd += 600.f)
					{
						FHitResult G;
						const FVector P = Self->GetActorLocation() + Dir * Dd;
						if (!Self->GetWorld()->LineTraceSingleByChannel(G, P + FVector(0, 0, 400.f), P - FVector(0, 0, 3000.f), ECC_WorldStatic) || G.ImpactPoint.Z < Self->GetActorLocation().Z - 400.f)
						{
							if (FVector::DotProduct(Dir, To.GetSafeNormal2D()) < 0.3f) { Best = P - Dir * 400.f; a = 99; }
							break;
						}
					}
				}
				AI->MoveToLocation(Best, 80.f);
			}
			return true;
		}
	}
	return false;
}

bool FWRCrystalAI::TickTame(AWRCreatureAI* AI, AWRCreature* Self, float DeltaSeconds)
{
	UWRCrystalBody* B = UWRCrystalBody::Get(Self);
	if (!B) return false;
	const double Now = CcNow(Self);
	AWRCharacterBase* Owner = CcOwnerOf(Self);
	if (Self->CreatureId == TEXT("gemsniffer") && Now >= B->NextSenseAt)
	{
		// It senses gems, amethyst, resonant crystal and caches within 20 m: its gem brightens and it points and scratches
		// toward the nearest (crystal 2.12.1).
		B->NextSenseAt = Now + 2.0;
		const UWRCrystalWorld* W = UWRCrystalWorld::Get(Self);
		const AActor* Near = nullptr;
		float Best = CcT(TEXT("gemsniffer.sense"), 2000.f);
		if (W) for (const TWeakObjectPtr<AWRResourceNode>& N : W->GetNodes())
		{
			if (!N.IsValid() || N->IsDepleted()) continue;
			const FName It = N->ItemId;
			if (It != TEXT("resonant_crystal") && It != TEXT("gem_amethyst") && !It.ToString().StartsWith(TEXT("gem_")) && It != TEXT("doubling_spar")) continue;
			const float Dd = FVector::Dist(N->GetActorLocation(), Self->GetActorLocation());
			if (Dd < Best) { Best = Dd; Near = N.Get(); }
		}
		for (TActorIterator<AWRWorldCache> It(Self->GetWorld()); It; ++It)
		{
			const float Dd = FVector::Dist(It->GetActorLocation(), Self->GetActorLocation());
			if (!It->bTaken && Dd < Best) { Best = Dd; Near = *It; }
		}
		if (Near)
		{
			Self->SetActorRotation(FRotator(0.f, (Near->GetActorLocation() - Self->GetActorLocation()).Rotation().Yaw, 0.f));
			WRFx::Broadcast(Self, TEXT("crystal.sense"), Self->GetActorLocation() + Self->GetActorForwardVector() * 60.f, Self->GetActorRotation(), 1.f, TEXT("crystal.scratch"));
		}
	}
	if (Self->CreatureId == TEXT("alicanto") && Owner)
	{
		// It senses ore veins within 40 m and, while you stand still a few seconds, runs to the richest and waits there.
		const bool bStill = FVector::Dist2D(Owner->GetActorLocation(), B->OwnerWas) < 60.f;
		if (!bStill) { B->OwnerWas = Owner->GetActorLocation(); B->StillSince = Now; B->LeadUntil = 0.0; return false; }
		if (Now - B->StillSince > CcT(TEXT("alicanto.leadAfter"), 3.f) && B->LeadUntil <= 0.0)
		{
			const UWRCrystalWorld* W = UWRCrystalWorld::Get(Self);
			const AWRResourceNode* Rich = nullptr;
			int32 Rank = -1;
			if (W) for (const TWeakObjectPtr<AWRResourceNode>& N : W->GetNodes())
			{
				if (!N.IsValid() || N->IsDepleted() || !N->ItemId.ToString().EndsWith(TEXT("_ore")) || FVector::Dist(N->GetActorLocation(), Owner->GetActorLocation()) > CcT(TEXT("alicanto.oreSense"), 4000.f)) continue;
				const int32 R = N->ItemId == TEXT("star_iron_ore") ? 5 : N->ItemId == TEXT("silver_ore") ? 4 : N->ItemId == TEXT("iron_ore") ? 3 : 1;
				if (R > Rank) { Rank = R; Rich = N.Get(); }
			}
			if (Rich) { AI->MoveToLocation(Rich->GetActorLocation(), 150.f); B->LeadUntil = Now + 30.0; WRSound::PlayAt(Self, TEXT("crystal.ring"), Self->GetActorLocation(), 0.5f); }
		}
		if (B->LeadUntil > 0.0 && Now < B->LeadUntil) return true;
	}
	return false;
}

bool FWRCrystalAI::RowAllowed(const AWRCreatureAI* AI, const AWRCreature* Self, const FWRAttackRow& Row, const AWRCharacterBase* Target, bool bReading)
{
	if (!UWRCrystalBody::Get(Self)) return true;
	// The Glare needs a light within 15 m of it brighter than a lit crystal (a held light, a Lumenhorn's antlers).
	if (Row.Id == TEXT("glare"))
	{
		for (TActorIterator<AWRCharacterBase> It(Self->GetWorld()); It; ++It)
		{
			if (FVector::Dist(It->GetActorLocation(), Self->GetActorLocation()) > CcT(TEXT("prismback.glareLight"), 1500.f)) continue;
			const UWRSurvivalComponent* S = UWRSurvivalComponent::For(*It);
			if ((S && S->IsLightOn()) || Cast<AWRCreature>(*It) && Cast<AWRCreature>(*It)->CreatureId == TEXT("lumenhorn")) return true;
		}
		return false;
	}
	// A Spindleglass spears only what is under its span; a collapsed one can't.
	if (Self->CreatureId == TEXT("spindleglass"))
	{
		const UWRCrystalBody* B = UWRCrystalBody::Get(Self);
		if (FMath::CountBits(B->LegsBroken) >= 3) return false;
		if (Row.Id == TEXT("leg_spear") && (!Target || FVector::Dist2D(Target->GetActorLocation(), Self->GetActorLocation()) > CcT(TEXT("spindleglass.span"), 450.f))) return false;
	}
	return true;
}

void FWRCrystalAI::Resolved(AWRCreature* Self, AWRCharacterBase* Victim, FName RowId, EWRHitResult Result)
{
	UWRCrystalBody* B = UWRCrystalBody::Get(Self);
	if (!B || !Victim) return;
	if (RowId == TEXT("glare") && Victim->Status)
	{
		// A flash, not the Blind status: the view whites out and the lock drops for a second.
		Victim->Status->ApplyStatus(WRTags::Status_Glare, CcT(TEXT("prismback.glareSeconds"), 1.f));
	}
	if (RowId == TEXT("latch") && Self->CreatureId == TEXT("geodemite"))
	{
		// At most four on one survivor.
		int32 On = 0;
		for (TActorIterator<AWRCreature> It(Self->GetWorld()); It; ++It) if (const UWRCrystalBody* O = UWRCrystalBody::Get(*It); O && O->LatchedOn == Victim) ++On;
		if (On < int32(CcT(TEXT("geodemite.maxLatched"), 4.f)))
		{
			B->LatchedOn = Victim;
			B->Set(WRCrystalFlag::Latched, true);
			B->Until[CcBit(WRCrystalFlag::Latched)] = CcNow(Self) + CcT(TEXT("geodemite.latchSeconds"), 3.f);
		}
	}
}

float FWRCrystalAI::Incoming(AWRCreature* Self, const FWRHit& Hit, float Damage)
{
	// The Twins' double and Lyrathis's copies are hard light: a blade passes through in a ripple; nothing lands.
	if (Self && (Self->CreatureId == TEXT("mirrored_twin_double") || Self->CreatureId == TEXT("lyrathis_copy")))
	{
		if (Damage > 0.f) WRFx::Broadcast(Self, TEXT("crystal.double_pass"), !Hit.ImpactPoint.IsZero() ? Hit.ImpactPoint : Self->GetActorLocation(), FRotator::ZeroRotator, 1.f, TEXT("crystal.boss.double_pass"));
		return 0.f;
	}
	UWRCrystalBody* B = UWRCrystalBody::Get(Self);
	if (!B || Damage <= 0.f) return Damage;
	if (B->Has(WRCrystalFlag::Rolled)) Damage *= CcT(TEXT("druseback.rolledTaken"), 0.3f);
	// A latched mite comes off when hit.
	if (B->Has(WRCrystalFlag::Latched)) { B->Set(WRCrystalFlag::Latched, false); B->LatchedOn = nullptr; }
	if (Self->CreatureId == TEXT("prismback") && Hit.bProjectile)
	{
		// The plated flank throws a projectile back along its line (x0.8) when the plates face it (crystal 2.12):
		// a hit from the side while its plates are turned, or anywhere but straight behind on the plates' side.
		const FVector From = (Hit.Origin - Self->GetActorLocation()).GetSafeNormal2D();
		const float Side = FMath::Abs(FVector::DotProduct(From, FRotationMatrix(Self->GetActorRotation()).GetUnitAxis(EAxis::Y)));
		if (B->Has(WRCrystalFlag::Plates) || Side > 0.6f)
		{
			if (AWRCharacterBase* Back = Cast<AWRCharacterBase>(Hit.Instigator); Back && Back != Self)
			{
				FWRHit R = Hit;
				R.Instigator = Self;
				R.Damage = Damage * CcT(TEXT("prismback.reflect"), 0.8f);
				R.Origin = Self->GetActorLocation();
				UWRCombatLibrary::ApplyHit(Back, R);
			}
			WRFx::Broadcast(Self, TEXT("crystal.reflect"), Self->GetActorLocation() + From * Self->GetSimpleCollisionRadius() + FVector(0, 0, 60.f), From.Rotation(), 1.f, TEXT("crystal.reflect"));
			return 0.f;
		}
	}
	if (Self->CreatureId == TEXT("spindleglass"))
	{
		// A blow low on it lands on a leg (strike x1.3); legs never regrow: 3 broken, it collapses; 5, it crawls.
		const FVector P = Hit.ImpactPoint.IsZero() ? Hit.Origin : Hit.ImpactPoint;
		const float BodyZ = Self->GetActorLocation().Z + CcT(TEXT("spindleglass.bodyLift"), 280.f) - Self->GetDef()->HalfHeight * 0.5f;
		if (P.Z < BodyZ - 120.f)
		{
			const FVector To = (P - Self->GetActorLocation()).GetSafeNormal2D();
			const float Yaw = FMath::UnwindDegrees(To.Rotation().Yaw - Self->GetActorRotation().Yaw);
			int32 Leg = 0;
			float BestA = 999.f;
			static const float Angles[8] = { 30.f, 65.f, 100.f, 135.f, -30.f, -65.f, -100.f, -135.f };
			for (int32 i = 0; i < 8; ++i) if (!(B->LegsBroken & (1 << i)) && FMath::Abs(FMath::FindDeltaAngleDegrees(Yaw, Angles[i])) < BestA) { BestA = FMath::Abs(FMath::FindDeltaAngleDegrees(Yaw, Angles[i])); Leg = i; }
			B->LegHP[Leg] -= Damage * (Hit.PhysType == 2 ? 1.3f : 1.f);
			if (B->LegHP[Leg] <= 0.f && !(B->LegsBroken & (1 << Leg)))
			{
				B->LegsBroken |= (1 << Leg);
				B->Refresh();
				// The shed leg falls and rings 6 s (a 12 m noise): hearing hunters go to it.
				WRFx::Broadcast(Self, TEXT("crystal.leg_shed"), P, FRotator::ZeroRotator, 1.f, TEXT("crystal.leg_shed"));
				WRCrystal::Emit(Self, P, WRCrystal::NoiseRadius(TEXT("hitCreature")), Self, TEXT("shed_leg"));
				const int32 Broken = FMath::CountBits(B->LegsBroken);
				if (Broken == 3 && Self->Status) Self->Status->ApplyStatus(WRTags::Status_StanceBroken, CcT(TEXT("spindleglass.collapseSeconds"), 4.f));
				if (Broken >= 5) Self->GetCharacterMovement()->MaxWalkSpeed *= 0.35f;
			}
			return Damage * CcT(TEXT("spindleglass.legBodyShare"), 0.2f);
		}
	}
	return Damage;
}

void FWRCrystalAI::Damaged(AWRCreature* Self, const FWRHit& Hit, float Damage)
{
	UWRCrystalBody* B = UWRCrystalBody::Get(Self);
	if (!B) return;
	const double Now = CcNow(Self);
	if (Self->CreatureId == TEXT("chimecrab"))
	{
		// Its shell rings when struck (a 10 m noise).
		WRSound::PlayAt(Self, TEXT("crystal.ring"), Self->GetActorLocation(), 1.f);
		WRCrystal::Emit(Self, Self->GetActorLocation(), CcT(TEXT("chimecrab.ring"), 1000.f), Self, TEXT("chime"));
	}
	if (Self->CreatureId == TEXT("druseback") && Hit.Impact >= 4 && !B->Has(WRCrystalFlag::Rolled))
	{
		B->Set(WRCrystalFlag::Rolled, true);
		B->Until[CcBit(WRCrystalFlag::Rolled)] = Now + CcT(TEXT("druseback.rollSeconds"), 4.f);
	}
	if (Self->CreatureId == TEXT("lumenhorn") && Now >= B->NextBellowAt)
	{
		// The herd bellows when one is hit: a 40 m noise that flares every crystal and calls the Echobats.
		B->NextBellowAt = Now + CcT(TEXT("lumenhorn.bellowEvery"), 20.f);
		WRFx::Broadcast(Self, TEXT("crystal.bellow"), Self->GetActorLocation() + FVector(0, 0, 200.f), Self->GetActorRotation(), 1.f, TEXT("crystal.bellow"));
		WRCrystal::Emit(Self, Self->GetActorLocation(), WRCrystal::NoiseRadius(TEXT("bellow")), Self, TEXT("bellow"));
	}
	if (Self->CreatureId == TEXT("prismback") && Hit.Instigator && FVector::Dist(Hit.Instigator->GetActorLocation(), Self->GetActorLocation()) < 2500.f)
	{
		// Plate turn: a shot or a cast from range swings its plated flank toward the source for 3 s.
		if (!Hit.bMelee)
		{
			B->Set(WRCrystalFlag::Plates, true);
			B->Until[CcBit(WRCrystalFlag::Plates)] = Now + CcT(TEXT("prismback.platesSeconds"), 3.f);
			const FVector To = (Hit.Instigator->GetActorLocation() - Self->GetActorLocation()).GetSafeNormal2D();
			Self->SetActorRotation(FRotator(0.f, To.Rotation().Yaw - 90.f, 0.f));
			WRSound::PlayAt(Self, TEXT("crystal.plates"), Self->GetActorLocation());
		}
	}
}

void FWRCrystalAI::Died(AWRCreature* Self, AActor* Killer)
{
	UWRCrystalBody* B = UWRCrystalBody::Get(Self);
	if (!B) return;
	if (Self->CreatureId == TEXT("geodemite"))
	{
		// Burst: the abdomen cracks with a tick and sprays shards 1.5 m round (blockable).
		const FVector At = Self->GetActorLocation();
		WRFx::Broadcast(Self, TEXT("crystal.mite_burst"), At, FRotator::ZeroRotator, 1.f, TEXT("crystal.mite_burst"));
		for (TActorIterator<AWRCharacterBase> It(Self->GetWorld()); It; ++It)
		{
			if (*It == Self || !It->IsAlive() || FVector::Dist(It->GetActorLocation(), At) > CcT(TEXT("geodemite.burstRadius"), 150.f) + It->GetSimpleCollisionRadius()) continue;
			if (const AWRCreature* O = Cast<AWRCreature>(*It); O && O->CreatureId == TEXT("geodemite")) continue;
			FWRHit H;
			H.Instigator = Self;
			H.Damage = CcT(TEXT("geodemite.burstDamage"), 5.f);
			H.Element = WRTags::Element_Arcane;
			H.Impact = 1;
			H.Origin = At;
			H.PhysType = 3;
			UWRCombatLibrary::ApplyHit(*It, H);
		}
	}
	if (B->Hearing.IsValid()) if (UWRCrystalWorld* W = UWRCrystalWorld::Get(Self)) W->OnNoise.Remove(B->Hearing);
	B->Hearing.Reset();
}

// ================================================================================================ dens and the quiet road

void UWRCrystalWorld::StepDens(float Dt)
{
	// The dens (crystal 2.12.3): every hall is a den of its sub-zone's table and every dome rim and surface site one of
	// the plateau's; a den fills when a survivor comes within crystalLife.denNear and empties when none is within
	// denFar (no creature lives where no one is). Its table is day or night by the clock (underground nights 22-04).
	DenClock -= Dt;
	if (DenClock > 0.f) return;
	DenClock = CcT(TEXT("denEvery"), 5.f);
	UWorld* World = GetWorld();
	const FWRCrystalData& D = FWRCrystalData::Get();
	if (Dens.Num() == 0)
	{
		for (const FWRCrystalData::FHall& H : D.Halls)
		{
			if (H.Zone == TEXT("V") || !H.Arena.IsNone() || H.bCup) continue;   // the Vault is empty; arenas hold their bosses
			FDen Dn;
			Dn.At = H.At + FVector(0.f, 0.f, 120.f);
			Dn.Zone = H.Zone;
			Dn.bCave = true;
			Dn.Spread = FMath::Max(200.f, FMath::Min(H.Rx, H.Ry) * 0.45f);
			Dens.Add(Dn);
		}
		for (const FWRCrystalData::FDome& Dm : D.Domes)
		{
			FDen Dn;
			Dn.At = Dm.At + FVector(Dm.R + 600.f, 0.f, 3000.f);
			Dn.Zone = TEXT("C1");
			Dn.Spread = 900.f;
			Dens.Add(Dn);
		}
		for (const FWRCrystalData::FSite& S : D.Sites)
		{
			if (S.bCave || S.bCamp || S.Kind == TEXT("descent")) continue;
			FDen Dn;
			Dn.At = S.At + FVector(0.f, 0.f, 3000.f);
			Dn.Zone = TEXT("C1");
			Dn.Spread = 1200.f;
			Dens.Add(Dn);
		}
	}
	const double Now = World->GetTimeSeconds();
	const float Hour = AWRGameState::HourIn(World);
	const float Near = CcT(TEXT("denNear"), 9000.f), Far = CcT(TEXT("denFar"), 14000.f);
	TArray<FVector> Players;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It) if (const APawn* P = It->Get() ? It->Get()->GetPawn() : nullptr) Players.Add(P->GetActorLocation());
	for (FDen& Dn : Dens)
	{
		float Closest = TNumericLimits<float>::Max();
		for (const FVector& P : Players) Closest = FMath::Min(Closest, float(FVector::Dist(P, Dn.At)));
		Dn.Alive.RemoveAll([](const TWeakObjectPtr<AWRCreature>& C) { return !C.IsValid() || !C->IsAlive(); });
		const bool bNight = Dn.bCave ? (Hour >= 22.f || Hour < 4.f) : AWRGameState::IsNightIn(World);
		if (Closest > Far || bNight != Dn.bNight)
		{
			// No one near (or the table turned): the den's wild ones that aren't hunting go back into the rock.
			for (const TWeakObjectPtr<AWRCreature>& C : Dn.Alive) if (C.IsValid() && !C->IsTamed() && !C->GetAggroTarget()) C->Destroy();
			Dn.Alive.RemoveAll([](const TWeakObjectPtr<AWRCreature>& C) { return !C.IsValid(); });
			Dn.bNight = bNight;
			if (Closest > Far) continue;
		}
		if (Closest > Near || Dn.Alive.Num() > 0 || Now < Dn.NextAt) continue;
		// Fill it from its table: crystalLife.dens.<zone>.<day|night> = {creature: [min, max]}.
		const TSharedPtr<FJsonObject> Table = WRCrystalCrImpl::CcTable(Dn.Zone, bNight);
		Dn.NextAt = Now + CcT(TEXT("denRespawn"), 600.f);
		if (!Table.IsValid()) continue;
		FRandomStream Rng(int32(Now * 10.0) ^ int32(Dn.At.X));
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Kv : Table->Values)
		{
			const TArray<TSharedPtr<FJsonValue>>* MinMax = nullptr;
			if (Kv.Key.StartsWith(TEXT("_")) || !Kv.Value->TryGetArray(MinMax) || MinMax->Num() < 2) continue;
			// A den may not have room for every kind: each kind rolls its chance (crystalLife.denChance) to be home.
			if (Rng.FRand() > CcT(TEXT("denChance"), 0.6f)) continue;
			const int32 N = Rng.RandRange(int32((*MinMax)[0]->AsNumber()), int32((*MinMax)[1]->AsNumber()));
			for (int32 i = 0; i < N; ++i)
			{
				const FVector Try = Dn.At + FRotator(0.f, Rng.FRandRange(0.f, 360.f), 0.f).Vector() * Rng.FRandRange(0.f, Dn.Spread);
				FHitResult G;
				if (!World->LineTraceSingleByChannel(G, Try + FVector(0.f, 0.f, Dn.bCave ? 200.f : 4000.f), Try - FVector(0.f, 0.f, Dn.bCave ? 800.f : 9000.f), ECC_WorldStatic)) continue;
				const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(FName(*Kv.Key));
				if (!Def) continue;
				if (AWRCreature* C = AWRCreature::SpawnCreature(World, FName(*Kv.Key), G.ImpactPoint + FVector(0.f, 0.f, Def->HalfHeight + 30.f), FRotator(0.f, Rng.FRandRange(0.f, 360.f), 0.f)))
				{
					C->SetHome(Dn.At);
					Dn.Alive.Add(C);
				}
			}
		}
	}
}

void UWRCrystalWorld::QuietRoadSpoiled(AActor* Who)
{
	const APawn* P = Cast<APawn>(Who);
	if (P && P->GetPlayerState() && QuietFrom.Contains(P->GetPlayerState()->GetPlayerName())) QuietSpoiled.Add(P->GetPlayerState()->GetPlayerName());
}

void UWRCrystalWorld::StepQuietRoad()
{
	// Saga's "The quiet road" (crystal 2.11): enter the Organ at one end, leave it at the other with no Echobat ever
	// having come for you on the way.
	const FWRCrystalData::FTube* Organ = FWRCrystalData::Get().FindTube(TEXT("organ"));
	if (!Organ || Organ->Pts.Num() < 2) return;
	const FVector Ends[2] = { Organ->Pts[0], Organ->Pts.Last() };
	const float Reach = CcT(TEXT("quietEndReach"), 1500.f);
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		APawn* P = PC ? PC->GetPawn() : nullptr;
		AWRPlayerState* PS = PC ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
		if (!P || !PS) continue;
		const FString Name = PS->GetPlayerName();
		for (int32 e = 0; e < 2; ++e)
		{
			if (FVector::Dist(P->GetActorLocation(), Ends[e]) > Reach) continue;
			const int32* From = QuietFrom.Find(Name);
			if (From && *From != e)
			{
				if (!QuietSpoiled.Contains(Name))
				{
					if (UWRWorldSystems* WS = GetWorld()->GetSubsystem<UWRWorldSystems>()) WS->QuestEvent(PS, TEXT("event"), TEXT("quiet_road"), 1);
					UE_LOG(LogWildreach, Log, TEXT("CRYSTAL %s crossed the Organ unheard"), *Name);
				}
				QuietFrom.Remove(Name);
				QuietSpoiled.Remove(Name);
			}
			if (!QuietFrom.Contains(Name)) { QuietFrom.Add(Name, e); QuietSpoiled.Remove(Name); }
		}
	}
}

// ================================================================================================ the Lumenhorn's bellow on order

bool FWRCrystalAI::Bellow(AWRCreature* Self, AActor* By)
{
	UWRCrystalBody* B = UWRCrystalBody::Get(Self);
	if (!B || !Self || !Self->HasAuthority() || Self->CreatureId != TEXT("lumenhorn") || !Self->IsAlive()) return false;
	const double Now = CcNow(Self);
	if (Now < B->NextBellowAt)
	{
		CzCrTell(By, TEXT("Its breath isn't back for another bellow yet"));
		return false;
	}
	B->NextBellowAt = Now + CcT(TEXT("lumenhorn.bellowEvery"), 20.f);
	Self->PlaySwing(EWRSwingKind::Heavy, 1.0f);
	WRFx::Broadcast(Self, TEXT("crystal.bellow"), Self->GetActorLocation() + FVector(0, 0, 200.f), Self->GetActorRotation(), 1.f, TEXT("crystal.bellow"));
	const FVector At = Self->GetActorLocation();
	const float R = WRCrystal::NoiseRadius(TEXT("bellow"));
	WRCrystal::Emit(Self, At, R, Self, TEXT("bellow"));
	// The crystals in reach stay flared for 20 s: the light renewed as it would fade.
	if (AWRCrystalState* S = AWRCrystalState::Get(Self))
	{
		TWeakObjectPtr<AWRCrystalState> WeakS(S);
		const float Hold = CcT(TEXT("lumenhorn.flareSeconds"), 20.f), Every = WRTune(TEXT("crystalWorld.light.fade"), 6.f) * 0.8f;
		for (float T = Every; T < Hold; T += Every)
		{
			FTimerHandle H;
			Self->GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([WeakS, At, R]() { if (AWRCrystalState* St = WeakS.Get()) St->MulticastNoise(At, R, 1.f); }), T, false);
		}
	}
	UE_LOG(LogWildreach, Log, TEXT("CRYSTAL a Lumenhorn bellows on order"));
	return true;
}

// ================================================================================================ path magic (Prismback)


bool FWRCrystalAI::PathTamePrompt(const AWRCreature* Self, const AActor* Who, FString& OutVerb)
{
	const UWRCrystalBody* B = UWRCrystalBody::Get(Self);
	const FWRCreatureDef* Def = Self ? Self->GetDef() : nullptr;
	if (!B || !Def || !Who || Self->CreatureId != TEXT("prismback") || Self->IsTamed() || !Self->IsAlive() || !Def->CanTameBy(TEXT("path_magic")))
	{
		return false;
	}
	const APawn* P = Cast<APawn>(Who);
	const FString Me = P && P->GetPlayerState() ? P->GetPlayerState()->GetPlayerName() : FString();
	if (!B->PathTamer.IsEmpty())
	{
		if (B->PathTamer != Me) return false;
		OutVerb = TEXT("Hold still: a Barrier is closing round it");
		return true;
	}
	if ((!Self->IsUnconscious() && Self->GetHealthFraction() >= WRTune(TEXT("arcane.pathMagic.below"), 0.3f))
		|| WRArcane::PointsIn(Who) < FMath::RoundToInt(WRTune(TEXT("arcane.pathMagic.points"), 10.f))
		|| !WRArcane::HasMana(Who, WRTune(TEXT("arcane.pathMagic.mana"), 30.f)))
	{
		return false;
	}
	OutVerb = FString::Printf(TEXT("Hold: ward it (%.0f s, %.0f Mana)"), WRTune(TEXT("arcane.pathMagic.holdSeconds"), 4.f), WRTune(TEXT("arcane.pathMagic.mana"), 30.f));
	return true;
}

bool FWRCrystalAI::TryPathTame(AWRCreature* Wild, AWRPlayerController* Player, FString& OutResult)
{
	UWRCrystalBody* B = UWRCrystalBody::Get(Wild);
	AWRCharacter* Who = Player ? Cast<AWRCharacter>(Player->GetPawn()) : nullptr;
	FString Verb;
	if (!B || !Who || !Wild->HasAuthority() || !PathTamePrompt(Wild, Who, Verb))
	{
		return false;
	}
	const FString Me = Player->PlayerState ? Player->PlayerState->GetPlayerName() : FString();
	if (B->PathTamer == Me)
	{
		OutResult = Verb;
		return true;
	}
	if (FVector::Dist2D(Who->GetActorLocation(), Wild->GetActorLocation()) > Wild->GetSimpleCollisionRadius() + WRTune(TEXT("arcane.pathMagic.reach"), 400.f))
	{
		OutResult = TEXT("Get your hand on its plates");
		return true;
	}
	B->PathTamer = Me;
	B->PathTameStart = CcNow(Wild);
	B->HandAt = Who->GetActorLocation();
	Wild->SetAggroTarget(nullptr);
	WRFx::BroadcastOn(Who, TEXT("arcane.cast"), TEXT("hand"), 1.2f, TEXT("arcane.barrier"), WRTune(TEXT("arcane.pathMagic.holdSeconds"), 4.f));
	OutResult = FString::Printf(TEXT("Hold E: draw a Barrier round it (%.0f s)"), WRTune(TEXT("arcane.pathMagic.holdSeconds"), 4.f));
	return true;
}

void FWRCrystalAI::StopPathTame(AWRPlayerController* Player)
{
	const FString Me = Player && Player->PlayerState ? Player->PlayerState->GetPlayerName() : FString();
	if (Me.IsEmpty() || !Player->GetWorld()) return;
	for (TActorIterator<AWRCreature> It(Player->GetWorld()); It; ++It)
	{
		UWRCrystalBody* B = UWRCrystalBody::Get(*It);
		if (!B || B->PathTamer != Me) continue;
		const double Held = CcNow(*It) - B->PathTameStart;
		B->PathTamer.Empty();
		Player->ClientNotify(Held < 0.35 ? It->Feed(Player) : FString(TEXT("You let go: the ward fades")));
		return;
	}
}

bool FWRCrystalAI::PathTameNear(const APlayerController* Player)
{
	const APawn* Pawn = Player ? Player->GetPawn() : nullptr;
	const FString Me = Player && Player->PlayerState ? Player->PlayerState->GetPlayerName() : FString();
	if (Me.IsEmpty() || !Pawn) return false;
	for (TActorIterator<AWRCreature> It(Pawn->GetWorld()); It; ++It)
	{
		const UWRCrystalBody* B = UWRCrystalBody::Get(*It);
		if (B && B->PathTamer == Me && FVector::Dist(It->GetActorLocation(), Pawn->GetActorLocation()) < 1500.f) return true;
	}
	return false;
}
