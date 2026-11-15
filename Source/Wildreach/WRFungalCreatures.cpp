// The Fungal caverns' creatures (see WRFungalCreatures.h; cooks/biome-fungal.md 2.12).

#include "WRFungalCreatures.h"
#include "WRFungalHazards.h"
#include "WRFungalLook.h"
#include "WRFungalTypes.h"
#include "WRFungalWorld.h"
#include "Wildreach.h"
#include "WRAttackRows.h"
#include "WRCharacter.h"
#include "WRCharacterBase.h"
#include "WRCombatLibrary.h"
#include "WRCreature.h"
#include "WRBoss.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRGameState.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRProjectile.h"
#include "WRResourceNode.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRToolMesh.h"
#include "WRTuning.h"
#include "WRWorldSystems.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"

TArray<TWeakObjectPtr<UWRFungalBody>> UWRFungalBody::Live;

namespace WRFungalCrImpl
{
	float FkT(const TCHAR* Key, float Def) { return WRTune(*FString::Printf(TEXT("fungalLife.%s"), Key), Def); }
	double FkNow(const UObject* Ctx) { const UWorld* W = Ctx ? Ctx->GetWorld() : nullptr; return W ? W->GetTimeSeconds() : 0.0; }
	int32 FkBit(uint8 F) { for (int32 i = 0; i < 8; ++i) if (F == (1 << i)) return i; return 0; }
	FLinearColor FkRgb(int32 R, int32 G, int32 B) { return FLinearColor(FColor(uint8(R), uint8(G), uint8(B))); }
	void FkTell(const AActor* Pawn, const FString& Text)
	{
		const APawn* P = Cast<APawn>(Pawn);
		if (AWRPlayerController* PC = P ? Cast<AWRPlayerController>(P->GetController()) : nullptr) PC->ClientNotify(Text);
	}
	bool FkIs(const AWRCreature* C, const TCHAR* Id) { return C && C->CreatureId == FName(Id); }
	/** A creatures.json row as JSON (its look's tint), loaded once. */
	TSharedPtr<FJsonObject> FkRow(FName Id)
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
	/** A row's numbers by id (attacks.json). */
	const FWRAttackRow* FkAttack(const AWRCreature* C, FName Id)
	{
		const TMap<FName, FWRAttackRow>* Rows = C ? FWRAttackRows::Get().Creature(C->CreatureId) : nullptr;
		return Rows ? Rows->Find(Id) : nullptr;
	}
	/** A blow from Self that isn't a row's (a tick of gas, a ring, a hold's squeeze): straight through ApplyHit. */
	void FkHit(AWRCreature* Self, AWRCharacterBase* Victim, float Damage, float Poise, uint8 Impact, const TMap<FName, float>& Buildups, bool bUnblockable = false, bool bLow = false, bool bMelee = true)
	{
		if (!Self || !Victim || !Victim->IsAlive()) return;
		FWRHit H;
		H.Instigator = Self;
		H.Damage = Damage;
		H.PoiseDamage = Poise;
		H.Impact = Impact;
		H.bUnblockable = bUnblockable;
		H.bLow = bLow;
		H.bMelee = bMelee;
		H.Origin = Self->GetActorLocation();
		H.Element = WRTags::Element_Physical;
		for (const TPair<FName, float>& B : Buildups) H.Buildups.Add(B.Key, B.Value);
		UWRCombatLibrary::ApplyHit(Victim, H);
	}
	/** A puff of spores round At (a shake, a garden puff, a burst): a real cloud (it flashes with fire, wind moves it)
	 *  and the poison it lets go of at once on whoever stands in it (fungal creatures shrug it off). */
	void FkPuff(AWRCreature* Self, const FVector& At, float Radius, float Seconds, float Poison, FName Fx)
	{
		if (!Self) return;
		UWorld* W = Self->GetWorld();
		AWRSporeCloud::Spawn(W, At + FVector(0.f, 0.f, 60.f), Radius, TEXT("creature"), Seconds, Self);
		WRFx::Broadcast(Self, Fx.IsNone() ? FName(TEXT("fungal.puff")) : Fx, At, FRotator::ZeroRotator, FMath::Max(0.5f, Radius / 200.f), TEXT("fungal.puff"));
		for (TActorIterator<AWRCharacterBase> It(W); It; ++It)
		{
			if (*It == Self || !It->IsAlive() || !It->Status || FVector::Dist(It->GetActorLocation(), At) > Radius + It->GetSimpleCollisionRadius()) continue;
			const float Taken = WRFungal::SporeTaken(*It);
			if (Taken > 0.f && Poison > 0.f) It->Status->AddStatusBuildup(WRStatus::Poison, Poison * Taken, Self);
		}
	}
	AWRCharacterBase* FkOwnerOf(const AWRCreature* Self)
	{
		if (!Self || !Self->IsTamed()) return nullptr;
		for (FConstPlayerControllerIterator It = Self->GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			const APlayerController* PC = It->Get();
			if (PC && PC->PlayerState && PC->PlayerState->GetPlayerName() == Self->TamedBy) return Cast<AWRCharacterBase>(PC->GetPawn());
		}
		return nullptr;
	}
	/** Webbed survivors: until when (server). */
	TMap<TWeakObjectPtr<AWRCharacterBase>, double>& FkWebbed() { static TMap<TWeakObjectPtr<AWRCharacterBase>, double> M; return M; }
	/** A mushroom cap: a lathe dome on a short stem along +Z (vertex alpha the glow on a glow part, else roughness). */
	void FkCap(FWRToolMesh& M, const FVector& At, float R, float H, const FLinearColor& Cap, const FLinearColor& Gill, float Glow, float Stem = 0.f, const FVector& Up = FVector::UpVector)
	{
		if (Stem > 0.f) M.Lathe(At, Up, Stem, { FVector(0.f, R * 0.22f, R * 0.22f), FVector(1.f, R * 0.18f, R * 0.18f) }, 6, Gill, Glow > 0.f ? Glow * 0.4f : 0.7f);
		const FVector Base = At + Up * Stem;
		M.Lathe(Base, Up, H, { FVector(0.f, R * 0.25f, R * 0.25f), FVector(0.05f, R, R), FVector(0.45f, R * 0.92f, R * 0.92f), FVector(0.85f, R * 0.5f, R * 0.5f), FVector(1.f, 0.f, 0.f) }, 10, Cap, Glow > 0.f ? Glow : 0.75f);
	}
	/** A short tube of segments from A toward B (a stalk, a tentacle, a tendril). */
	void FkStalk(FWRToolMesh& M, const FVector& A, const FVector& B, float R0, float R1, const FLinearColor& C, float Alpha, float Wave = 0.f)
	{
		TArray<FVector> Prof;
		for (int32 k = 0; k <= 5; ++k) Prof.Add(FVector(k / 5.f, FMath::Lerp(R0, R1, k / 5.f), FMath::Lerp(R0, R1, k / 5.f)));
		const FVector D = B - A;
		M.Lathe(A, D.GetSafeNormal(), D.Size(), Prof, 5, C, Alpha, Wave);
	}
}
using namespace WRFungalCrImpl;

// ================================================================================================ UWRFungalBody

UWRFungalBody::UWRFungalBody()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UWRFungalBody::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UWRFungalBody, Flags);
	DOREPLIFETIME(UWRFungalBody, Victim);
}

UWRFungalBody* UWRFungalBody::Get(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UWRFungalBody>() : nullptr;
}

const TArray<TWeakObjectPtr<UWRFungalBody>>& UWRFungalBody::All()
{
	Live.RemoveAll([](const TWeakObjectPtr<UWRFungalBody>& B) { return !B.IsValid(); });
	return Live;
}

bool UWRFungalBody::IsFungalKind(FName CreatureId)
{
	const FWRCreatureDef* D = FWRCreatureRegistry::Get().Find(CreatureId);
	return D && D->Biome == WRFungal::Biome();
}

void UWRFungalBody::Attach(AWRCreature* Creature)
{
	if (!Creature || !Creature->HasAuthority() || Cast<AWRBoss>(Creature) || !IsFungalKind(Creature->CreatureId) || Get(Creature)) return;
	UWRFungalBody* B = NewObject<UWRFungalBody>(Creature, TEXT("FungalBody"));
	B->RegisterComponent();
}

int32 UWRFungalBody::LatchedOn(const AActor* Victim)
{
	if (!Victim) return 0;
	int32 N = 0;
	for (const TWeakObjectPtr<UWRFungalBody>& B : Live) if (B.IsValid() && B->Has(WRFungalFlag::Latched) && B->Victim == Victim) ++N;
	return N;
}

AWRCreature* UWRFungalBody::HolderOf(const AActor* Victim)
{
	if (!Victim) return nullptr;
	for (const TWeakObjectPtr<UWRFungalBody>& B : Live) if (B.IsValid() && B->Has(WRFungalFlag::Holding) && B->Victim == Victim) return Cast<AWRCreature>(B->GetOwner());
	return nullptr;
}

void UWRFungalBody::Set(uint8 F, bool bOn)
{
	const uint8 Was = Flags;
	Flags = bOn ? (Flags | F) : (Flags & ~F);
	if (Flags != Was) { OnRep_Flags(); if (AActor* O = GetOwner()) O->ForceNetUpdate(); }
}

FVector UWRFungalBody::LurePoint() const
{
	const AWRCreature* C = Cast<AWRCreature>(GetOwner());
	if (!C) return FVector::ZeroVector;
	return C->GetActorLocation() + C->GetActorForwardVector() * (C->GetSimpleCollisionRadius() + FkT(TEXT("rotmaw.lureOut"), 70.f)) - FVector(0.f, 0.f, C->GetSimpleCollisionHalfHeight() - 5.f);
}

void UWRFungalBody::BeginPlay()
{
	Super::BeginPlay();
	Live.RemoveAll([](const TWeakObjectPtr<UWRFungalBody>& B) { return !B.IsValid(); });
	Live.Add(this);
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	if (!C) return;
	// The toad's leap (every machine that moves it agrees: a rider's machine predicts its mount): Jump held charges it,
	// up to 0.6 s (the engine's own held jump: a tap is a hop, a full hold clears 6 m up and, at a gallop, 12 m across).
	if (FkIs(C, TEXT("tunnel_toad")))
	{
		C->GetCharacterMovement()->JumpZVelocity = FkT(TEXT("toad.jumpZ"), 750.f);
		C->JumpMaxHoldTime = FkT(TEXT("toad.jumpHold"), 0.6f);
		C->GetCharacterMovement()->AirControl = FkT(TEXT("toad.airControl"), 0.6f);
	}
	if (GetNetMode() != NM_DedicatedServer) BuildLook();
	if (!C->HasAuthority() || C->IsTamed()) return;
	// After its spawner sets it down: a Sporestalker or Webspinner in a cave goes up its hall's ceiling; a Rotmaw
	// settles into its mound.
	TWeakObjectPtr<AWRCreature> WeakC(C);
	TWeakObjectPtr<UWRFungalBody> WeakB(this);
	FTimerHandle H;
	GetWorld()->GetTimerManager().SetTimer(H, [WeakC, WeakB]()
	{
		AWRCreature* Cr = WeakC.Get();
		UWRFungalBody* B = WeakB.Get();
		if (!Cr || !B || !Cr->IsAlive() || Cr->IsTamed()) return;
		if (FkIs(Cr, TEXT("sporestalker")) || FkIs(Cr, TEXT("webspinner"))) FWRFungalAI::Climb(Cr, B);
		if (FkIs(Cr, TEXT("rotmaw"))) FWRFungalAI::Settle(Cr, B);
	}, 0.7f, false);
}

void UWRFungalBody::EndPlay(const EEndPlayReason::Type Reason)
{
	Live.Remove(this);
	Super::EndPlay(Reason);
}

void UWRFungalBody::OnRep_Flags()
{
	const AWRCreature* C = Cast<AWRCreature>(GetOwner());
	if (!C) return;
	if (Lure) Lure->SetVisibility(Has(WRFungalFlag::Mound));
}

FName UWRFungalBody::BoneLike(const TCHAR* Hint) const
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

UProceduralMeshComponent* UWRFungalBody::Part(FName Bone, const FVector& Local, FWRToolMesh& M, bool bGlow)
{
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	USkeletalMeshComponent* S = C ? C->GetCreatureSkel() : nullptr;
	if (!C) return nullptr;
	UProceduralMeshComponent* P = NewObject<UProceduralMeshComponent>(C);
	if (S && !Bone.IsNone()) P->SetupAttachment(S, Bone);
	else P->SetupAttachment(C->GetRootComponent());
	P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	P->SetUsingAbsoluteScale(true);
	P->RegisterComponent();
	// Parts are modelled in the actor's frame: placed in world space from the bone (or the capsule), upright.
	const FVector At = (S && !Bone.IsNone()) ? S->GetSocketLocation(Bone) : C->GetActorLocation();
	P->SetWorldLocationAndRotation(At + C->GetActorRotation().RotateVector(Local), C->GetActorRotation());
	P->SetWorldScale3D(FVector::OneVector);
	M.ApplyTo(P);
	if (bGlow) if (UMaterialInterface* G = WRFungalLook::GlowMaterial(C)) P->SetMaterial(0, G);
	Parts.Add(P);
	return P;
}

void UWRFungalBody::ApplyTint()
{
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	USkeletalMeshComponent* Skel = C ? C->GetCreatureSkel() : nullptr;
	UMaterialInterface* Master = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Creatures/M_CreatureTint.M_CreatureTint"));
	if (!Skel || !Master) return;
	const TSharedPtr<FJsonObject> Row = FkRow(C->CreatureId);
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
		M->SetVectorParameterValue(TEXT("GlowColor"), Col(TEXT("glow"), FLinearColor(0.22f, 0.9f, 0.76f)));
		Skel->SetMaterial(Slot, M);
	}
}

void UWRFungalBody::BuildLook()
{
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	const FWRCreatureDef* D = C ? C->GetDef() : nullptr;
	USkeletalMeshComponent* Skel = C ? C->GetCreatureSkel() : nullptr;
	if (!D || !Skel) return;
	const FName Id = C->CreatureId;
	if (Id != TEXT("puffball") && !Skel->GetSkeletalMeshAsset()) return;
	bBuilt = true;
	ApplyTint();
	const float R = D->Radius, H = D->HalfHeight;
	// The cook's palette (fungal 2.12.1): every creature's spores cyan, crimson only on unblockable glints.
	const FLinearColor Cyan = FkRgb(57, 230, 194), Bone = FkRgb(232, 226, 208), Ivory = FkRgb(216, 203, 176), Umber = FkRgb(74, 58, 44), Orange = FkRgb(217, 119, 43);
	const FLinearColor Pink = FkRgb(232, 154, 154), Rust = FkRgb(168, 74, 48), Cream = FkRgb(240, 230, 208), Oyster = FkRgb(154, 164, 170), Shell = FkRgb(46, 36, 28);
	FRandomStream Rng(static_cast<int32>(GetTypeHash(C->GetName())));
	if (Id == TEXT("sporestalker"))
	{
		// A crest of Ophiocordyceps stalks 30-60 cm down the spine: bone-white, the tips cyan and glowing; under the jaw
		// a ruff of orange gills that flares open for the Gill bite.
		FWRToolMesh M;
		for (int32 i = 0; i < 16; ++i)
		{
			const float X = FMath::Lerp(R * 0.7f, -R * 0.9f, i / 15.f) + Rng.FRandRange(-6.f, 6.f);
			const float Len = Rng.FRandRange(30.f, 60.f) * (1.f - FMath::Abs(i / 15.f - 0.4f) * 0.6f);
			const FVector A(X, Rng.FRandRange(-10.f, 10.f), H * 0.55f);
			const FVector B = A + FVector(Rng.FRandRange(-12.f, -4.f), Rng.FRandRange(-8.f, 8.f), Len);
			FkStalk(M, A, B, 2.2f, 0.9f, Bone * 0.6f, 0.f);
			M.Lathe(B, (B - A).GetSafeNormal(), 7.f, { FVector(0.f, 2.f, 2.f), FVector(0.5f, 2.6f, 2.6f), FVector(1.f, 0.f, 0.f) }, 5, Cyan, 1.f);
		}
		Crest = Part(NAME_None, FVector::ZeroVector, M, true);
		FWRToolMesh G;
		for (int32 i = 0; i < 12; ++i)
		{
			const float A = FMath::DegreesToRadians(-150.f + i * 25.f);
			const FVector Root(0.f, FMath::Cos(A) * 14.f, FMath::Sin(A) * 12.f - 10.f);
			FkStalk(G, Root, Root + FVector(-6.f, FMath::Cos(A) * 16.f, FMath::Sin(A) * 14.f - 6.f), 4.f, 1.f, Orange * 0.7f, 0.15f);
		}
		Ruff = Part(BoneLike(TEXT("head")), FVector(-10.f, 0.f, -8.f), G, true);
	}
	else if (Id == TEXT("rotmaw"))
	{
		// A living mound of ivory and tan caps over the black-brown shell, and the lure: a pink worm on the tongue that
		// shows exactly like a deepcap (a pale stem, a translucent cyan cap), out only while it waits as a mound.
		FWRToolMesh M;
		for (int32 i = 0; i < 26; ++i)
		{
			const float A = Rng.FRandRange(0.f, 2.f * PI), Rr = FMath::Sqrt(Rng.FRand()) * R * 0.85f;
			const FVector At(FMath::Cos(A) * Rr, FMath::Sin(A) * Rr * 0.8f, H * (0.9f - Rr / R * 0.7f));
			const float Size = Rng.FRandRange(14.f, 34.f) * (1.2f - Rr / R * 0.5f);
			FkCap(M, At, Size, Size * 0.55f, Rng.FRand() < 0.5f ? Ivory * 0.6f : FkRgb(176, 150, 112) * 0.6f, Ivory * 0.5f, 0.f, Size * Rng.FRandRange(0.3f, 0.8f));
		}
		Mound = Part(NAME_None, FVector::ZeroVector, M, false);
		FWRToolMesh L;
		FkStalk(L, FVector::ZeroVector, FVector(0.f, 0.f, 14.f), 2.6f, 2.f, Pink * 0.65f, 0.f);
		FkCap(L, FVector(0.f, 0.f, 12.f), 9.f, 6.f, Cyan * 0.8f, Cyan * 0.6f, 0.9f, 4.f);
		Lure = Part(NAME_None, C->GetActorRotation().UnrotateVector(LurePoint() - C->GetActorLocation()), L, true);
	}
	else if (Id == TEXT("mycomite") || Id == TEXT("sporeling"))
	{
		// A pale cap grown on its back (the Sporeling is all cap: a bigger one, its gills glowing cyan underneath);
		// the Mycomite's bloated abdomen veined cyan.
		const bool bLing = Id == TEXT("sporeling");
		FWRToolMesh M;
		FkCap(M, FVector(-R * 0.2f, 0.f, H * 0.6f), bLing ? R * 1.3f : R * 0.75f, bLing ? R * 0.9f : R * 0.5f, Ivory * 0.62f, Cyan, bLing ? 0.35f : 0.f, bLing ? 10.f : 4.f);
		for (int32 v = 0; v < 5; ++v) FkStalk(M, FVector(-R * 0.5f, -8.f + v * 4.f, H * 0.2f), FVector(-R * 1.05f, -10.f + v * 5.f, H * 0.35f), 0.9f, 0.6f, Cyan, 0.8f);
		Part(NAME_None, FVector::ZeroVector, M, true);
	}
	else if (Id == TEXT("webspinner"))
	{
		// Two huge forward eyes (silver reflectors), the black abdomen dotted cyan, and eight long banded legs spanning
		// 4 m round the small body (umber and cream bands), stepping as it moves.
		FWRToolMesh E;
		for (const float S : { -1.f, 1.f }) E.Lathe(FVector(R * 0.55f, S * 10.f, H * 0.55f), FVector(1.f, 0.f, 0.f), 14.f, { FVector(0.f, 0.f, 0.f), FVector(0.4f, 9.f, 9.f), FVector(1.f, 0.f, 0.f) }, 8, FkRgb(200, 205, 210), 0.35f);
		for (int32 i = 0; i < 14; ++i) E.Lathe(FVector(-R * Rng.FRandRange(0.4f, 1.0f), Rng.FRandRange(-22.f, 22.f), H * Rng.FRandRange(0.45f, 0.9f)), FVector::UpVector, 4.f, { FVector(0.f, 3.f, 3.f), FVector(1.f, 0.f, 0.f) }, 5, Cyan, 1.f);
		Part(NAME_None, FVector::ZeroVector, E, true);
		for (int32 i = 0; i < 8; ++i)
		{
			FLeg Lg;
			Lg.Angle = i < 4 ? 35.f + i * 33.f : -35.f - (i - 4) * 33.f;
			Lg.Phase = (i % 2) * PI + (i >= 4 ? PI * 0.5f : 0.f);
			for (int32 s = 0; s < 2; ++s)
			{
				FWRToolMesh Seg;
				Seg.Lathe(FVector::ZeroVector, FVector(1.f, 0.f, 0.f), 100.f, { FVector(0.f, 3.5f, 3.5f), FVector(1.f, 2.4f, 2.4f) }, 6, FkRgb(106, 80, 64) * 0.6f, 0.6f);
				for (int32 b = 1; b < 5; ++b) Seg.Lathe(FVector(b * 20.f - 4.f, 0.f, 0.f), FVector(1.f, 0.f, 0.f), 8.f, { FVector(0.f, 3.8f, 3.8f), FVector(1.f, 3.6f, 3.6f) }, 6, Ivory * 0.62f, 0.6f);
				UProceduralMeshComponent* P = NewObject<UProceduralMeshComponent>(C);
				P->SetupAttachment(C->GetRootComponent());
				P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				P->SetUsingAbsoluteLocation(true);
				P->SetUsingAbsoluteRotation(true);
				P->SetUsingAbsoluteScale(true);
				P->RegisterComponent();
				Seg.ApplyTo(P);
				(s == 0 ? Lg.Upper : Lg.Lower) = P;
				Parts.Add(P);
			}
			Legs.Add(Lg);
		}
	}
	else if (Id == TEXT("tunnel_toad"))
	{
		// The throat sac (translucent pink-white), swelling to the size of its head for the croak.
		FWRToolMesh M;
		M.Lathe(FVector(-14.f, 0.f, 0.f), FVector(1.f, 0.f, 0.f), 40.f, { FVector(0.f, 0.f, 0.f), FVector(0.3f, 18.f, 16.f), FVector(0.7f, 20.f, 18.f), FVector(1.f, 0.f, 0.f) }, 10, FkRgb(240, 200, 196), 0.12f);
		Ruff = Part(BoneLike(TEXT("head")), FVector(10.f, 0.f, -30.f), M, true);
	}
	else if (Id == TEXT("shroomback"))
	{
		// The oyster-cap garden on its dome (fan-shaped caps in shelves, grey-blue) and the spiked club on its tail.
		FWRToolMesh M;
		for (int32 i = 0; i < 22; ++i)
		{
			const float A = Rng.FRandRange(0.f, 2.f * PI), Rr = FMath::Sqrt(Rng.FRand()) * R * 0.7f;
			const FVector At(FMath::Cos(A) * Rr, FMath::Sin(A) * Rr * 0.75f, H * (1.05f - Rr / R * 0.6f));
			const FVector Out = FVector(FMath::Cos(A), FMath::Sin(A), 0.6f).GetSafeNormal();
			TArray<TArray<FVector>> Rings;
			const FVector Side = FVector::CrossProduct(Out, FVector::UpVector).GetSafeNormal();
			const float Wd = Rng.FRandRange(18.f, 32.f);
			for (const float T : { 0.f, 1.f })
			{
				TArray<FVector> Ring;
				for (int32 k = 0; k <= 6; ++k) { const float Aa = -0.5f * PI + k * PI / 6.f; Ring.Add(At + Out * (FMath::Cos(Aa) * Wd * 0.9f + 4.f) + Side * FMath::Sin(Aa) * Wd + FVector(0.f, 0.f, T * 5.f)); }
				Ring.Add(At + FVector(0.f, 0.f, T * 5.f));
				Rings.Add(Ring);
			}
			M.Loft(Rings, Oyster * 0.6f, 0.7f, true);
		}
		Part(NAME_None, FVector::ZeroVector, M, false);
		FWRToolMesh Club;
		Club.Lathe(FVector(-14.f, 0.f, 0.f), FVector(1.f, 0.f, 0.f), 34.f, { FVector(0.f, 10.f, 10.f), FVector(0.5f, 20.f, 20.f), FVector(1.f, 8.f, 8.f) }, 8, FkRgb(94, 84, 64) * 0.6f, 0.85f);
		for (int32 k = 0; k < 10; ++k) { const FVector Dir = FVector(Rng.FRandRange(-0.5f, 0.5f), Rng.FRandRange(-1.f, 1.f), Rng.FRandRange(-1.f, 1.f)).GetSafeNormal(); FkStalk(Club, Dir * 16.f, Dir * 34.f, 4.f, 0.5f, Ivory * 0.5f, 0.6f); }
		Part(BoneLike(TEXT("tail")), FVector(-30.f, 0.f, 0.f), Club, false);
	}
	else if (Id == TEXT("truffle_mole"))
	{
		// The star: 22 short pink tentacles in a ring round the nostrils.
		FWRToolMesh M;
		for (int32 i = 0; i < 22; ++i)
		{
			const float A = i * 2.f * PI / 22.f;
			const FVector Dir(0.35f, FMath::Cos(A), FMath::Sin(A));
			FkStalk(M, FVector(0.f, FMath::Cos(A) * 3.f, FMath::Sin(A) * 3.f), Dir.GetSafeNormal() * 11.f, 1.6f, 0.9f, Pink * 0.7f, 0.5f);
		}
		Part(BoneLike(TEXT("head")), FVector(R * 0.45f, 0.f, 0.f), M, false);
	}
	else if (Id == TEXT("glowgrub"))
	{
		// The tail lantern: a soft glowing bulb, blue-green, and its own small light.
		FWRToolMesh M;
		M.Lathe(FVector(-R * 0.9f, 0.f, 4.f), FVector(-1.f, 0.f, 0.f), 26.f, { FVector(0.f, 10.f, 10.f), FVector(0.5f, 14.f, 13.f), FVector(1.f, 0.f, 0.f) }, 10, FkRgb(127, 232, 208), 1.f);
		Sac = Part(NAME_None, FVector::ZeroVector, M, true);
		Lamp = NewObject<UPointLightComponent>(C);
		Lamp->SetupAttachment(C->GetRootComponent());
		Lamp->SetRelativeLocation(FVector(-R * 1.1f, 0.f, 10.f));
		Lamp->SetLightColor(FkRgb(127, 232, 208));
		Lamp->SetAttenuationRadius(FkT(TEXT("glowgrub.wildLight"), 450.f));
		Lamp->SetIntensity(FkT(TEXT("glowgrub.lampPower"), 900.f));
		Lamp->SetCastShadows(false);
		Lamp->RegisterComponent();
	}
	else if (Id == TEXT("capling"))
	{
		// Its back a living rust-red cap with cream spots (it regrows when harvested).
		FWRToolMesh M;
		FkCap(M, FVector(-4.f, 0.f, H * 0.35f), R * 1.15f, R * 0.7f, Rust * 0.6f, Cream * 0.6f, 0.f, 2.f);
		for (int32 i = 0; i < 9; ++i)
		{
			const float A = Rng.FRandRange(0.f, 2.f * PI), Rr = Rng.FRandRange(0.2f, 0.75f) * R;
			M.Lathe(FVector(-4.f + FMath::Cos(A) * Rr, FMath::Sin(A) * Rr, H * 0.35f + 2.f + R * 0.7f * (1.f - Rr / (R * 1.1f)) * 0.9f), FVector::UpVector, 2.f, { FVector(0.f, 3.f, 3.f), FVector(1.f, 2.f, 2.f) }, 6, Cream * 0.7f, 0.6f);
		}
		Part(NAME_None, FVector::ZeroVector, M, false);
	}
	else if (Id == TEXT("puffball"))
	{
		// No rig shows: a pale sac 1.2 m across floating over its capsule, trailing sticky threads with cyan tips (a
		// siphonophore's drift). It bobs and leans with its drift (TickLook).
		Skel->SetVisibility(false, true);
		FWRToolMesh M;
		TArray<FVector> Prof;
		for (int32 k = 0; k <= 10; ++k) { const float T = k / 10.f; const float S = FMath::Sin(T * PI) * 60.f * (1.f + 0.06f * FMath::Sin(T * 17.f)); Prof.Add(FVector(T, S, S * 0.94f)); }
		M.Lathe(FVector(0.f, 0.f, -55.f), FVector::UpVector, 115.f, Prof, 14, FkRgb(237, 230, 214) * 0.7f, 0.06f, 0.f, 3.f);
		for (int32 i = 0; i < 9; ++i)
		{
			const float A = i * 2.f * PI / 9.f + Rng.FRandRange(-0.2f, 0.2f);
			const FVector Top(FMath::Cos(A) * 28.f, FMath::Sin(A) * 28.f, -48.f);
			const float Len = Rng.FRandRange(80.f, 150.f);
			FkStalk(M, Top, Top + FVector(Rng.FRandRange(-12.f, 12.f), Rng.FRandRange(-12.f, 12.f), -Len), 1.6f, 0.8f, FkRgb(220, 214, 200) * 0.5f, 0.04f, 4.f);
			M.Lathe(Top + FVector(0.f, 0.f, -Len), FVector(0.f, 0.f, -1.f), 6.f, { FVector(0.f, 2.f, 2.f), FVector(1.f, 0.f, 0.f) }, 5, Cyan, 1.f);
		}
		Sac = Part(NAME_None, FVector(0.f, 0.f, H * 0.45f), M, true);
	}
	OnRep_Flags();
}

void UWRFungalBody::TickLook(float Dt)
{
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	if (!C) return;
	LookTime += Dt;
	const auto Ease = [Dt](float& V, float To, float Rate) { V = FMath::FInterpTo(V, To, Dt, Rate); };
	Ease(CrestT, Has(WRFungalFlag::Crest) ? 1.f : 0.f, 6.f);
	Ease(RuffT, Has(WRFungalFlag::Flared) ? 1.f : 0.f, 8.f);
	// The crest swells and locks upright (the volley); the ruff flares, the toad's sac swells.
	if (Crest) Crest->SetWorldScale3D(FVector(1.f, 1.f, 1.f + CrestT * 0.45f));
	if (Ruff) Ruff->SetWorldScale3D(FVector(1.f + RuffT * (C->CreatureId == TEXT("tunnel_toad") ? 0.9f : 0.6f)));
	// A Rotmaw in its mound sinks into the floor so only the caps show; the lure trembles.
	if (C->CreatureId == TEXT("rotmaw"))
	{
		const float Was = MoundT;
		Ease(MoundT, Has(WRFungalFlag::Mound) ? 1.f : 0.f, 3.f);
		if (USkeletalMeshComponent* S = C->GetCreatureSkel()) S->AddRelativeLocation(FVector(0.f, 0.f, -(MoundT - Was) * C->GetSimpleCollisionHalfHeight() * 0.6f));
		if (Mound) Mound->AddRelativeLocation(FVector(0.f, 0.f, -(MoundT - Was) * C->GetSimpleCollisionHalfHeight() * 0.6f));
		if (Lure && Lure->IsVisible()) Lure->SetRelativeRotation(FRotator(FMath::Sin(LookTime * 3.1f) * 6.f, 0.f, FMath::Sin(LookTime * 2.3f) * 5.f));
	}
	// The Puffball bobs on the air and leans into its drift.
	if (Sac && C->CreatureId == TEXT("puffball"))
	{
		const FVector V = C->GetVelocity();
		Sac->SetRelativeLocation(FVector(0.f, 0.f, C->GetSimpleCollisionHalfHeight() * 0.45f + FMath::Sin(LookTime * 0.9f) * 18.f));
		Sac->SetWorldRotation(FRotator(-V.X * 0.02f, C->GetActorRotation().Yaw, V.Y * 0.02f));
	}
	// The Webspinner's legs: from hips round the body to feet on the floor at its span, a step's arc as it moves.
	if (Legs.Num())
	{
		const FWRCreatureDef* D = C->GetDef();
		const float Span = FkT(TEXT("webspinner.legReach"), 190.f);
		Stride += C->GetVelocity().Size2D() * Dt / 140.f;
		const FVector Body = C->GetActorLocation() + FVector(0.f, 0.f, D ? D->HalfHeight * 0.1f : 0.f);
		const float Ground = C->GetActorLocation().Z - (D ? D->HalfHeight : 50.f);
		const bool bUpside = Has(WRFungalFlag::Ceiling);
		for (FLeg& L : Legs)
		{
			const FVector Dir = FRotator(0.f, C->GetActorRotation().Yaw + L.Angle, 0.f).Vector();
			const float Lift = FMath::Max(0.f, FMath::Sin(Stride + L.Phase)) * 30.f;
			const FVector Fwd = C->GetActorForwardVector() * FMath::Cos(Stride + L.Phase) * 30.f;
			const FVector Hip = Body + Dir * 20.f;
			const float FootZ = bUpside ? C->GetActorLocation().Z + (D ? D->HalfHeight : 50.f) : Ground;
			const FVector Foot = FVector(Body.X, Body.Y, FootZ) + Dir * Span + Fwd + FVector(0.f, 0.f, bUpside ? -Lift : Lift);
			const FVector Knee = (Hip + Foot) * 0.5f + FVector(0.f, 0.f, bUpside ? -110.f : 110.f);
			const auto Place = [](UProceduralMeshComponent* P, const FVector& A, const FVector& B)
			{
				if (!P) return;
				const FVector V = B - A;
				P->SetWorldLocationAndRotation(A, V.Rotation());
				P->SetWorldScale3D(FVector(V.Size() / 100.f, 1.f, 1.f));
			};
			Place(L.Upper.Get(), Hip, Knee);
			Place(L.Lower.Get(), Knee, Foot);
		}
	}
}

void UWRFungalBody::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	if (!C) return;
	if (C->HasAuthority())
	{
		const double Now = FkNow(C);
		for (int32 b = 0; b < 8; ++b)
		{
			const uint8 F = uint8(1 << b);
			if (Has(F) && Until[b] > 0.0 && Now >= Until[b])
			{
				Until[b] = 0.0;
				Set(F, false);
				if (F == WRFungalFlag::Latched || F == WRFungalFlag::Holding) Victim = nullptr;
			}
		}
		FWRFungalAI::TickBody(C, this, DeltaTime);
	}
	if (GetNetMode() == NM_DedicatedServer) return;
	if (!bBuilt && ++Tries < 60) BuildLook();
	if (bBuilt) TickLook(DeltaTime);
}

// ================================================================================================ FWRFungalAI: the body's server tick

bool FWRFungalAI::Climb(AWRCreature* Self, UWRFungalBody* Body)
{
	if (!Self || !Body || !WRFungal::InCave(Self->GetActorLocation())) return false;
	// The roof straight overhead (the caverns' rock), 3-12 m up: it climbs there and hangs.
	FHitResult Up;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(WRFungalClimb), false, Self);
	const FVector From = Self->GetActorLocation();
	if (!Self->GetWorld()->LineTraceSingleByChannel(Up, From, From + FVector(0.f, 0.f, FkT(TEXT("ceilingReach"), 1200.f)), ECC_WorldStatic, Q)) return false;
	if (Up.Distance < 300.f) return false;
	Body->CeilingAt = Up.ImpactPoint - FVector(0.f, 0.f, Self->GetSimpleCollisionHalfHeight() + 15.f);
	Self->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	Self->SetActorLocation(Body->CeilingAt, false, nullptr, ETeleportType::TeleportPhysics);
	Body->Set(WRFungalFlag::Ceiling, true);
	Self->SetAggroTarget(nullptr);
	return true;
}

void FWRFungalAI::Settle(AWRCreature* Self, UWRFungalBody* Body)
{
	if (!Self || !Body || Body->Has(WRFungalFlag::Mound)) return;
	Body->Set(WRFungalFlag::Mound, true);
	Body->MoundAt = FkNow(Self);
	Self->GetCharacterMovement()->StopMovementImmediately();
}

void FWRFungalAI::TickBody(AWRCreature* Self, UWRFungalBody* Body, float DeltaTime)
{
	const double Now = FkNow(Self);
	AWRCreatureAI* AI = Cast<AWRCreatureAI>(Self->GetController());
	UCharacterMovementComponent* Move = Self->GetCharacterMovement();
	// A drop or a leap coming down: the ring where it lands (blockable; the leap's is low: jump it).
	if ((Body->bDropping || Body->bLeaping) && Move->IsMovingOnGround())
	{
		const FName RowId = FkIs(Self, TEXT("tunnel_toad")) ? FName(TEXT("leap_slam")) : FkIs(Self, TEXT("webspinner")) ? FName(TEXT("ceiling_pounce")) : FName(TEXT("ceiling_drop"));
		const FWRAttackRow* R = FkAttack(Self, RowId);
		const float Rad = R && R->Radius > 0.f ? R->Radius : 250.f;
		Body->bDropping = Body->bLeaping = false;
		WRFx::Broadcast(Self, TEXT("fungal.landing"), Self->GetActorLocation() - FVector(0.f, 0.f, Self->GetSimpleCollisionHalfHeight()), FRotator::ZeroRotator, Rad / 200.f, TEXT("stomp"));
		if (AI)
		{
			for (TActorIterator<AWRCharacterBase> It(Self->GetWorld()); It; ++It)
			{
				if (*It == Self || !It->IsAlive() || !Self->IsHostileTo(*It)) continue;
				// The low ring (the leap): a survivor in the air clears it.
				if (RowId == TEXT("leap_slam") && It->GetCharacterMovement() && It->GetCharacterMovement()->IsFalling()) continue;
				if (FVector::Dist2D(It->GetActorLocation(), Self->GetActorLocation()) < Rad + It->GetSimpleCollisionRadius() && FMath::Abs(It->GetActorLocation().Z - Self->GetActorLocation().Z) < 300.f) AI->RowHit(Self, *It);
			}
		}
	}
	// A ridden toad's leap: the landing slam ring (3 m, 20) after a fall of more than 2 m.
	if (Self->GetRider())
	{
		if (Move->IsFalling())
		{
			if (!Body->bRideLeap) { Body->bRideLeap = true; Body->FallFromZ = Self->GetActorLocation().Z; }
			Body->FallFromZ = FMath::Max(Body->FallFromZ, float(Self->GetActorLocation().Z));
		}
		else if (Body->bRideLeap)
		{
			Body->bRideLeap = false;
			if (FkIs(Self, TEXT("tunnel_toad")) && Body->FallFromZ - Self->GetActorLocation().Z > FkT(TEXT("toad.slamDrop"), 200.f))
			{
				const float Rad = FkT(TEXT("toad.slamRadius"), 300.f);
				WRFx::Broadcast(Self, TEXT("fungal.landing"), Self->GetActorLocation() - FVector(0.f, 0.f, Self->GetSimpleCollisionHalfHeight()), FRotator::ZeroRotator, Rad / 200.f, TEXT("stomp"));
				for (TActorIterator<AWRCharacterBase> It(Self->GetWorld()); It; ++It)
				{
					if (*It == Self || *It == Self->GetRider() || !It->IsAlive() || !Self->IsHostileTo(*It) || !It->IsHostileTo(Self->GetRider())) continue;
					if (FVector::Dist(It->GetActorLocation(), Self->GetActorLocation()) < Rad + It->GetSimpleCollisionRadius() + Self->GetSimpleCollisionHalfHeight())
					{
						FkHit(Self, *It, FkT(TEXT("toad.slamDamage"), 20.f), 30.f, 3, {}, false, true);
					}
				}
			}
		}
	}
	// A Rotmaw's hold (the lure snap): 1.5 s in its beak at 15 a second, unblockable; mash frees sooner.
	if (Body->Has(WRFungalFlag::Holding))
	{
		AWRCharacterBase* V = Cast<AWRCharacterBase>(Body->Victim);
		if (!V || !V->IsAlive() || !Self->IsAlive())
		{
			Body->Set(WRFungalFlag::Holding, false);
			Body->Victim = nullptr;
		}
		else
		{
			V->SetActorLocation(Self->GetActorLocation() + Self->GetActorForwardVector() * (Self->GetSimpleCollisionRadius() + 40.f), true);
			if (Now >= Body->HoldTickAt)
			{
				Body->HoldTickAt = Now + 0.5;
				FkHit(Self, V, FkT(TEXT("rotmaw.holdDps"), 15.f) * 0.5f, 0.f, 1, {}, true);
				if (V->Status) V->Status->Stagger(0.6f);
			}
		}
	}
	// A latched Mycomite rides its victim (one per limb): drain 2 a second, poison 6 a second. A roll may shake it, fire
	// on the victim burns it off.
	if (Body->Has(WRFungalFlag::Latched))
	{
		AWRCharacterBase* V = Cast<AWRCharacterBase>(Body->Victim);
		bool bOff = !V || !V->IsAlive() || !Self->IsAlive() || (V && V->HasTag(WRTags::Status_Burn)) || (V && WRFungal::CarriesFire(V) && FMath::FRand() < DeltaTime * 0.5f);
		if (!bOff && V->HasTag(WRTags::State_Dodging) && Now >= Body->NextPuffAt)
		{
			Body->NextPuffAt = Now + 0.8;   // one roll, one chance
			bOff = FMath::FRand() < FkT(TEXT("mycomite.rollShake"), 0.5f);
		}
		if (bOff)
		{
			Body->Set(WRFungalFlag::Latched, false);
			Body->Victim = nullptr;
			if (V) Self->SetActorLocation(V->GetActorLocation() + FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f).Vector() * 120.f, true);
			Move->SetMovementMode(MOVE_Walking);
		}
		else
		{
			const int32 Slot = int32(GetTypeHash(Self->GetName()) % 4);
			const FVector Limb = FRotator(0.f, V->GetActorRotation().Yaw + 45.f + Slot * 90.f, 0.f).Vector() * (V->GetSimpleCollisionRadius() * 0.8f) + FVector(0.f, 0.f, Slot < 2 ? 30.f : -40.f);
			Self->SetActorLocation(V->GetActorLocation() + Limb, false, nullptr, ETeleportType::TeleportPhysics);
			if (Now >= Body->LatchTickAt)
			{
				Body->LatchTickAt = Now + 1.0;
				FkHit(Self, V, FkT(TEXT("mycomite.drain"), 2.f), 0.f, 1, {}, true);
				if (V->Status && WRFungal::SporeTaken(V) >= 0.f) V->Status->AddStatusBuildup(WRStatus::Poison, FkT(TEXT("mycomite.latchPoison"), 6.f), Self);
			}
		}
	}
	// Webbed survivors: the net lets go when its time is up (mash shortens it).
	for (auto It = FkWebbed().CreateIterator(); It; ++It)
	{
		AWRCharacterBase* V = It->Key.Get();
		if (!V) { It.RemoveCurrent(); continue; }
		if (V->GetWorld() != Self->GetWorld() || Now < It->Value) continue;
		if (V->Status) V->Status->ClearStatus(WRTags::Status_Rooted);
		It.RemoveCurrent();
	}
}

// ================================================================================================ FWRFungalAI: the wild brain

bool FWRFungalAI::TickWild(AWRCreatureAI* AI, AWRCreature* Self, float DeltaSeconds)
{
	UWRFungalBody* B = UWRFungalBody::Get(Self);
	if (!B) return false;
	const double Now = FkNow(Self);
	UCharacterMovementComponent* Move = Self->GetCharacterMovement();
	if (B->Has(WRFungalFlag::Latched) || B->Has(WRFungalFlag::Holding) || B->bDropping || B->bLeaping) return true;
	if (B->Has(WRFungalFlag::Tucked))
	{
		Move->StopMovementImmediately();
		return true;
	}
	// On a ceiling: hold still above the hall; drop on what walks under (a Sporestalker within 8 m below, a Webspinner
	// within 10 m: its thread first).
	if (B->Has(WRFungalFlag::Ceiling))
	{
		Move->StopMovementImmediately();
		Self->SetActorLocation(B->CeilingAt);
		if (AI->RowPhase != 0) return false;
		const bool bSpinner = FkIs(Self, TEXT("webspinner"));
		const float Reach = bSpinner ? FkT(TEXT("webspinner.dropReach"), 500.f) : FkT(TEXT("sporestalker.dropReach"), 450.f);
		const float Below = bSpinner ? 1000.f : 800.f;
		for (TActorIterator<AWRCharacterBase> It(Self->GetWorld()); It; ++It)
		{
			if (*It == Self || !It->IsAlive() || !Self->IsHostileTo(*It) || Cast<AWRCreature>(*It)) continue;
			const float Dz = Self->GetActorLocation().Z - It->GetActorLocation().Z;
			if (FVector::Dist2D(It->GetActorLocation(), Self->GetActorLocation()) < Reach && Dz > 0.f && Dz < Below + Self->GetSimpleCollisionHalfHeight())
			{
				Self->SetAggroTarget(*It);
				AI->StartRow(Self, *It, bSpinner ? FName(TEXT("ceiling_pounce")) : FName(TEXT("ceiling_drop")), false);
				break;
			}
		}
		// A Webspinner on its ceiling reels a Glowgrub up into the False Sky now and then (a light rising into the stars).
		if (bSpinner && Now >= B->NextBeatAt)
		{
			B->NextBeatAt = Now + FMath::FRandRange(20.f, 45.f);
			for (const TWeakObjectPtr<UWRFungalBody>& O : UWRFungalBody::All())
			{
				AWRCreature* G = O.IsValid() ? Cast<AWRCreature>(O->GetOwner()) : nullptr;
				if (!G || !FkIs(G, TEXT("glowgrub")) || G->IsTamed() || !G->IsAlive() || O->Lifting.IsValid() || FVector::Dist2D(G->GetActorLocation(), Self->GetActorLocation()) > 1500.f) continue;
				O->Lifting = Self;
				G->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
				G->GetCharacterMovement()->StopMovementImmediately();
				WRSound::PlayAt(Self, TEXT("fungal.line_reel"), G->GetActorLocation(), 0.6f);
				break;
			}
		}
		return true;
	}
	// A Rotmaw in its mound: still, the lure out. Reach within 1.5 m of the lure and it snaps; come within 3.5 m of the
	// mound and it bursts out. It goes back to its mound when nothing has troubled it for a while.
	if (FkIs(Self, TEXT("rotmaw")))
	{
		if (B->Has(WRFungalFlag::Mound))
		{
			Move->StopMovementImmediately();
			if (AI->RowPhase != 0) return false;
			const FVector Lure = B->LurePoint();
			for (TActorIterator<AWRCharacterBase> It(Self->GetWorld()); It; ++It)
			{
				if (*It == Self || !It->IsAlive()) continue;
				// A Capling hops to the lone "deepcap" and vanishes into the mound (the living world shows the rule).
				if (AWRCreature* Cr = Cast<AWRCreature>(*It))
				{
					if (FkIs(Cr, TEXT("capling")) && !Cr->IsTamed() && FVector::Dist2D(Cr->GetActorLocation(), Lure) < 120.f)
					{
						WRFx::Broadcast(Self, TEXT("fungal.moundBurst"), Self->GetActorLocation(), FRotator::ZeroRotator, 0.8f, TEXT("fungal.lure_snap"));
						Self->PlaySwing(EWRSwingKind::Light, 0.3f);
						Cr->Destroy();
					}
					continue;
				}
				if (!Self->IsHostileTo(*It)) continue;
				const float ToLure = FVector::Dist2D(It->GetActorLocation(), Lure);
				const float ToBody = FVector::Dist2D(It->GetActorLocation(), Self->GetActorLocation()) - Self->GetSimpleCollisionRadius();
				if (ToLure < FkT(TEXT("rotmaw.lureReach"), 150.f) + It->GetSimpleCollisionRadius())
				{
					Self->SetAggroTarget(*It);
					AI->StartRow(Self, *It, TEXT("lure_snap"), false);
					return true;
				}
				if (ToBody < FkT(TEXT("rotmaw.burstReach"), 350.f))
				{
					Self->SetAggroTarget(*It);
					AI->StartRow(Self, *It, TEXT("mound_burst"), false);
					return true;
				}
			}
			return true;
		}
		if (!Self->GetAggroTarget() && Self->SinceHit() > FkT(TEXT("rotmaw.resettle"), 25.f) && AI->RowPhase == 0) Settle(Self, B);
	}
	// A hunter off its ceiling with nothing to hunt climbs back up a while later (Sporestalkers climb walls and ceilings
	// between fights; a Webspinner goes back to its thread).
	if ((FkIs(Self, TEXT("sporestalker")) || FkIs(Self, TEXT("webspinner"))) && !Self->GetAggroTarget() && AI->RowPhase == 0 && Self->SinceHit() > 20.f && Now >= B->NextClimbAt)
	{
		B->NextClimbAt = Now + 10.0;
		if (Climb(Self, B)) return true;
	}
	// A Webspinner answers a twanged lure line within 30 m: it comes for whoever touched it.
	if (FkIs(Self, TEXT("webspinner")) && !Self->GetAggroTarget())
	{
		for (const TWeakObjectPtr<AWRLureLines>& L : AWRLureLines::All())
		{
			if (!L.IsValid() || WRFungal::Now(Self) - L->LastTwangTime > 2.f || FVector::Dist(L->LastTwangAt, Self->GetActorLocation()) > FkT(TEXT("webspinner.twangHear"), 3000.f)) continue;
			AWRCharacterBase* Best = nullptr;
			float BestD = 400.f;
			for (TActorIterator<AWRCharacterBase> It(Self->GetWorld()); It; ++It)
			{
				if (Cast<AWRCreature>(*It) || !It->IsAlive()) continue;
				const float Dd = FVector::Dist2D(It->GetActorLocation(), L->LastTwangAt);
				if (Dd < BestD) { BestD = Dd; Best = *It; }
			}
			if (Best) { Self->SetAggroTarget(Best); break; }
		}
	}
	// Mycomites: warm light draws them from twice as far (a torch, a burning blade, the Fire path's heat over half: cold
	// light doesn't), and twice again in the Spore bloom; with no one to bite they swarm toward any fire near.
	if (FkIs(Self, TEXT("mycomite")) && !Self->GetAggroTarget() && Now >= B->NextSenseAt)
	{
		B->NextSenseAt = Now + 1.0;
		const FWRCreatureDef* D = Self->GetDef();
		const float Base = D ? D->AggroRadius : 1200.f;
		const float Bloom = WRFungal::BloomOn(Self) ? FkT(TEXT("mycomite.bloomMult"), 2.f) : 1.f;
		for (TActorIterator<AWRCharacterBase> It(Self->GetWorld()); It; ++It)
		{
			if (Cast<AWRCreature>(*It) || !It->IsAlive() || !Self->IsHostileTo(*It)) continue;
			const float Range = Base * Bloom * (WRFungal::CarriesFire(*It) ? FkT(TEXT("mycomite.warmLightMult"), 2.f) : 1.f);
			if (FVector::Dist(It->GetActorLocation(), Self->GetActorLocation()) < Range) { Self->SetAggroTarget(*It); break; }
		}
	}
	// A Puffball rides warm air: by day it drifts up (the dolines' updraughts), at night it settles; it drifts toward
	// warm light within 20 m.
	if (FkIs(Self, TEXT("puffball")) && !Self->GetAggroTarget())
	{
		for (TActorIterator<AWRCharacterBase> It(Self->GetWorld()); It; ++It)
		{
			if (Cast<AWRCreature>(*It) || !It->IsAlive() || !WRFungal::CarriesFire(*It) || FVector::Dist(It->GetActorLocation(), Self->GetActorLocation()) > FkT(TEXT("puffball.warmDrift"), 2000.f)) continue;
			AI->MoveToLocation(It->GetActorLocation(), 300.f);
			return true;
		}
	}
	// A Capling near a grazing Shroomback hops onto its shell and rides a while (F2's meadows).
	if (FkIs(Self, TEXT("capling")))
	{
		if (B->Has(WRFungalFlag::Riding))
		{
			AWRCreature* On = Cast<AWRCreature>(B->RideOn.Get());
			if (!On || !On->IsAlive() || Self->SinceHit() < 1.f || Now >= B->Until[FkBit(WRFungalFlag::Riding)] - 0.1)
			{
				B->Set(WRFungalFlag::Riding, false);
				B->RideOn = nullptr;
				Move->SetMovementMode(MOVE_Falling);
				return false;
			}
			const int32 Slot = int32(GetTypeHash(Self->GetName()) % 3) - 1;
			Self->SetActorLocation(On->GetActorLocation() + On->GetActorRotation().RotateVector(FVector(-20.f + Slot * 30.f, Slot * 35.f, On->GetSimpleCollisionHalfHeight() + Self->GetSimpleCollisionHalfHeight())), false, nullptr, ETeleportType::TeleportPhysics);
			Self->SetActorRotation(On->GetActorRotation());
			return true;
		}
		if (Now >= B->NextBeatAt && !Self->GetAggroTarget())
		{
			B->NextBeatAt = Now + FMath::FRandRange(15.f, 40.f);
			for (const TWeakObjectPtr<UWRFungalBody>& O : UWRFungalBody::All())
			{
				AWRCreature* S = O.IsValid() ? Cast<AWRCreature>(O->GetOwner()) : nullptr;
				if (!S || !FkIs(S, TEXT("shroomback")) || S->IsTamed() || FVector::Dist(S->GetActorLocation(), Self->GetActorLocation()) > 600.f) continue;
				B->RideOn = S;
				B->Set(WRFungalFlag::Riding, true);
				B->Until[FkBit(WRFungalFlag::Riding)] = Now + FMath::FRandRange(30.f, 90.f);
				Move->SetMovementMode(MOVE_Flying);
				break;
			}
		}
	}
	// A Glowgrub being reeled up into the False Sky: it rises on the thread and is gone into the stars.
	if (FkIs(Self, TEXT("glowgrub")) && B->Lifting.IsValid())
	{
		const FVector To = B->Lifting->GetActorLocation();
		Self->SetActorLocation(FMath::VInterpConstantTo(Self->GetActorLocation(), To, DeltaSeconds, 160.f), false);
		if (FVector::Dist(Self->GetActorLocation(), To) < 120.f) Self->Destroy();
		return true;
	}
	// Sporestalkers hunt Tunnel toads at the Toad Leaps; the colony's croak blast scatters them.
	if (FkIs(Self, TEXT("sporestalker")) && !Self->GetAggroTarget() && Now >= B->NextBeatAt)
	{
		B->NextBeatAt = Now + FMath::FRandRange(8.f, 16.f);
		for (const TWeakObjectPtr<UWRFungalBody>& O : UWRFungalBody::All())
		{
			AWRCreature* T = O.IsValid() ? Cast<AWRCreature>(O->GetOwner()) : nullptr;
			if (!T || !FkIs(T, TEXT("tunnel_toad")) || T->IsTamed() || !T->IsAlive() || FVector::Dist(T->GetActorLocation(), Self->GetActorLocation()) > 3000.f) continue;
			AI->MoveToLocation(T->GetActorLocation(), 300.f);
			if (FVector::Dist(T->GetActorLocation(), Self->GetActorLocation()) < 600.f)
			{
				// The colony croaks: a ring of sound; the hunter is thrown back and slinks off.
				T->PlaySwing(EWRSwingKind::Heavy, 0.9f);
				WRFx::Broadcast(T, TEXT("fungal.croak"), T->GetActorLocation(), T->GetActorRotation(), 2.f, TEXT("creature.tunnel_toad.signature"));
				Self->LaunchCharacter((Self->GetActorLocation() - T->GetActorLocation()).GetSafeNormal2D() * 900.f + FVector(0.f, 0.f, 300.f), true, true);
				Self->SetHome(Self->GetActorLocation() + (Self->GetActorLocation() - T->GetActorLocation()).GetSafeNormal2D() * 2500.f);
			}
			return false;
		}
	}
	return false;
}

bool FWRFungalAI::TickTame(AWRCreatureAI* AI, AWRCreature* Self, float DeltaSeconds)
{
	UWRFungalBody* B = UWRFungalBody::Get(Self);
	if (!B) return false;
	const double Now = FkNow(Self);
	if (B->Has(WRFungalFlag::Ceiling)) { B->Set(WRFungalFlag::Ceiling, false); Self->GetCharacterMovement()->SetMovementMode(MOVE_Falling); }
	if (B->Has(WRFungalFlag::Mound)) B->Set(WRFungalFlag::Mound, false);
	if (B->Has(WRFungalFlag::Riding)) { B->Set(WRFungalFlag::Riding, false); Self->GetCharacterMovement()->SetMovementMode(MOVE_Falling); }
	// A tamed Truffle mole: it senses ore, deepcap and hidden passages within 15 m and knocks on the rock toward the
	// nearest; it knocks back at a cap-fall ceiling over its owner (marked 10 s early).
	if (FkIs(Self, TEXT("truffle_mole")) && Now >= B->NextSenseAt)
	{
		B->NextSenseAt = Now + FkT(TEXT("mole.senseEvery"), 4.f);
		const float Sense = FkT(TEXT("mole.sense"), 1500.f);
		const AActor* Near = nullptr;
		float Best = Sense;
		if (const UWRFungalWorld* W = UWRFungalWorld::Get(Self))
		{
			for (const TWeakObjectPtr<AWRResourceNode>& N : W->GetNodes())
			{
				if (!N.IsValid() || N->IsDepleted()) continue;
				const FString It = N->ItemId.ToString();
				if (It != TEXT("deepcap") && !It.EndsWith(TEXT("_ore")) && !It.StartsWith(TEXT("gem"))) continue;
				const float Dd = FVector::Dist(N->GetActorLocation(), Self->GetActorLocation());
				if (Dd < Best) { Best = Dd; Near = N.Get(); }
			}
		}
		for (const TWeakObjectPtr<AWRFungalSpot>& S : AWRFungalSpot::All())
		{
			if (!S.IsValid() || S->bOpen) continue;
			const float Dd = FVector::Dist(S->GetActorLocation(), Self->GetActorLocation());
			if (Dd < Best) { Best = Dd; Near = S.Get(); }
		}
		if (Near)
		{
			Self->SetActorRotation(FRotator(0.f, (Near->GetActorLocation() - Self->GetActorLocation()).Rotation().Yaw, 0.f));
			Self->PlaySwing(EWRSwingKind::Light, 0.4f);
			WRFx::Broadcast(Self, TEXT("fungal.moleKnock"), Self->GetActorLocation() + Self->GetActorForwardVector() * 50.f, Self->GetActorRotation(), 1.f, TEXT("fungal.mole_knock"));
		}
		if (AWRCharacterBase* Owner = FkOwnerOf(Self))
		{
			for (const TWeakObjectPtr<AWRCapFall>& F : AWRCapFall::All())
			{
				if (F.IsValid() && FVector::Dist2D(F->GetActorLocation(), Owner->GetActorLocation()) < FkT(TEXT("mole.capFallWarn"), 1500.f)) F->MoleWarn();
			}
		}
	}
	return false;
}

// ================================================================================================ rows

bool FWRFungalAI::RowAllowed(const AWRCreatureAI* AI, const AWRCreature* Self, const FWRAttackRow& Row, const AWRCharacterBase* Target, bool bReading)
{
	const UWRFungalBody* B = UWRFungalBody::Get(Self);
	if (!B) return true;
	// The openers only from where they belong (a ceiling, the mound): the brain starts them itself.
	if (Row.Id == TEXT("ceiling_drop") || Row.Id == TEXT("ceiling_pounce") || Row.Id == TEXT("mound_burst") || Row.Id == TEXT("lure_snap")) return false;
	if (B->Has(WRFungalFlag::Ceiling) || B->Has(WRFungalFlag::Mound) || B->Has(WRFungalFlag::Tucked)) return false;
	// A tame Mycomite doesn't latch on its owner's foes in a way that strands it; the latch only on a survivor with
	// fewer than four on.
	if (Row.Id == TEXT("latch") && (Self->IsTamed() || UWRFungalBody::LatchedOn(Target) >= int32(FkT(TEXT("mycomite.maxLatched"), 4.f)))) return false;
	return true;
}

void FWRFungalAI::RowStarted(AWRCreatureAI* AI, AWRCreature* Self, const FWRAttackRow& Row, AWRCharacterBase* Target)
{
	UWRFungalBody* B = UWRFungalBody::Get(Self);
	if (!B) return;
	const double End = FkNow(Self) + Row.Windup + Row.Hold + Row.Active;
	const auto Timed = [B](uint8 F, double Until) { B->Set(F, true); B->Until[FkBit(F)] = Until; };
	if (Row.Id == TEXT("stalk_volley")) Timed(WRFungalFlag::Crest, End);
	if (Row.Id == TEXT("gill_bite") || Row.Id == TEXT("croak_blast")) Timed(WRFungalFlag::Flared, End);
	// Grit trickles from the ceiling (the drop), a bright thread drops beside you (the spinner), the mound shivers.
	if (Row.Id == TEXT("ceiling_drop")) WRFx::Broadcast(Self, TEXT("fungal.ceilingGrit"), Self->GetActorLocation(), FRotator::ZeroRotator, 1.f, TEXT("creature.sporestalker.rattle"));
	if (Row.Id == TEXT("ceiling_pounce") && Target) WRFx::Broadcast(Self, TEXT("fungal.threadDrop"), FVector(Target->GetActorLocation().X + 80.f, Target->GetActorLocation().Y, Self->GetActorLocation().Z), FRotator::ZeroRotator, 1.f, TEXT("fungal.line_twang"));
	if (Row.Id == TEXT("mound_burst") || Row.Id == TEXT("lure_snap")) WRFx::Broadcast(Self, TEXT("fungal.moundShiver"), Self->GetActorLocation(), FRotator::ZeroRotator, 1.f, TEXT("creature.rotmaw.shiver"));
	if (Row.Id == TEXT("rot_belch")) WRFx::Broadcast(Self, TEXT("fungal.belchTell"), Self->MouthLocation(), Self->GetActorRotation(), 1.f, TEXT("creature.rotmaw.gurgle"));
}

bool FWRFungalAI::StrikeRow(AWRCreatureAI* AI, AWRCreature* Self, const FWRAttackRow& Row, AWRCharacterBase* Target)
{
	UWRFungalBody* B = UWRFungalBody::Get(Self);
	if (!B) return false;
	UWorld* World = Self->GetWorld();
	const FName Id = Row.Id;
	if (Id == TEXT("ceiling_drop") || Id == TEXT("ceiling_pounce"))
	{
		// Off the ceiling and down onto the mark: it falls (its own weight) with a push toward where you stood.
		B->Set(WRFungalFlag::Ceiling, false);
		B->bDropping = true;
		Self->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		const FVector To = Target ? Target->GetActorLocation() - Self->GetActorLocation() : FVector::ZeroVector;
		Self->LaunchCharacter(FVector(To.X * 1.3f, To.Y * 1.3f, -300.f), true, true);
		return true;
	}
	if (Id == TEXT("mound_burst") || Id == TEXT("lure_snap"))
	{
		// Out of the mound: the lure snaps back in, the caps throw spores; the generic stomp / bite does the rest.
		B->Set(WRFungalFlag::Mound, false);
		WRFx::Broadcast(Self, TEXT("fungal.moundBurst"), Self->GetActorLocation(), FRotator::ZeroRotator, 1.5f, TEXT("creature.rotmaw.burst"));
		return false;
	}
	if (Id == TEXT("stalk_volley"))
	{
		// Five pellets lobbed on high arcs to land in a 4 m spread round where you stood (900 cm/s, never homing); each
		// bursts into a 2 m cloud for 4 s, poison 25.
		const FVector From = Self->GetActorLocation() + FVector(0.f, 0.f, Self->GetSimpleCollisionHalfHeight() + 40.f);
		const int32 N = int32(FkT(TEXT("sporestalker.pellets"), 5.f));
		const float Spread = FkT(TEXT("sporestalker.pelletSpread"), 200.f);
		for (int32 i = 0; i < N; ++i)
		{
			const FVector Land = AI->RowAim + (i == 0 ? FVector::ZeroVector : FRotator(0.f, i * 360.f / (N - 1) + FMath::FRandRange(-20.f, 20.f), 0.f).Vector() * Spread * FMath::FRandRange(0.6f, 1.f));
			FActorSpawnParameters P;
			P.Owner = Self;
			P.Instigator = Self;
			P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AWRProjectile* Pellet = World->SpawnActor<AWRProjectile>(AWRProjectile::StaticClass(), From, FRotator::ZeroRotator, P);
			if (!Pellet) continue;
			FWRHit H;
			H.Instigator = Self;
			H.Damage = Self->Weapon.FlatDamage * Row.Dmg;
			H.PoiseDamage = Row.Poise;
			H.Impact = Row.Impact;
			H.bProjectile = true;
			H.Element = WRTags::Element_Physical;
			for (const TPair<FName, float>& Bu : Row.Buildups) H.Buildups.Add(Bu.Key, Bu.Value);
			const float Gravity = FkT(TEXT("sporestalker.pelletGravity"), 1.6f);
			Pellet->InitLob(H, AWRProjectile::LobVelocity(From, Land, Row.ProjectileSpeed, Gravity), Gravity, Row.Radius > 0.f ? Row.Radius : 200.f, TEXT("code:sporepellet"));
			TWeakObjectPtr<AWRCreature> WeakSelf(Self);
			Pellet->OnLanded = [WeakSelf, World](const FVector& At)
			{
				AWRSporeCloud::Spawn(World, At + FVector(0.f, 0.f, 60.f), FkT(TEXT("sporestalker.pelletCloud"), 200.f), TEXT("creature"), FkT(TEXT("sporestalker.pelletSeconds"), 4.f), WeakSelf.Get());
			};
		}
		WRSound::PlayAt(Self, TEXT("creature.sporestalker.volley"), From);
		return true;
	}
	if (Id == TEXT("web_shot"))
	{
		// The net: 900 cm/s with a little drop, never homing; a hit webs you 2 s.
		const FVector From = Self->GetActorLocation() - Self->GetActorForwardVector() * Self->GetSimpleCollisionRadius() * 0.5f + FVector(0.f, 0.f, Self->GetSimpleCollisionHalfHeight());
		const FVector Aim = AI->RowAim + FVector(0.f, 0.f, 80.f);
		FActorSpawnParameters P;
		P.Owner = Self;
		P.Instigator = Self;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AWRProjectile* Net = World->SpawnActor<AWRProjectile>(AWRProjectile::StaticClass(), From, (Aim - From).Rotation(), P))
		{
			FWRHit H;
			H.Instigator = Self;
			H.Damage = Self->Weapon.FlatDamage * Row.Dmg;
			H.PoiseDamage = Row.Poise;
			H.Impact = Row.Impact;
			H.bProjectile = true;
			H.Element = WRTags::Element_Physical;
			const float Gravity = FkT(TEXT("webspinner.netGravity"), 0.35f);
			Net->InitPhysical(H, AWRProjectile::LobVelocity(From, Aim, Row.ProjectileSpeed, Gravity), Gravity, TEXT("code:webnet"), FString(), TEXT("webnet"), NAME_None);
			TWeakObjectPtr<AWRCreature> WeakSelf(Self);
			Net->OnStruck = [WeakSelf](AWRCharacterBase* V, EWRHitResult Result)
			{
				if (V && (Result == EWRHitResult::Hit || Result == EWRHitResult::GuardBroken)) FWRFungalAI::Web(V, FkT(TEXT("webspinner.webbedSeconds"), 2.f), WeakSelf.Get());
			};
		}
		WRSound::PlayAt(Self, TEXT("creature.webspinner.spin"), From);
		return true;
	}
	if (Id == TEXT("croak_blast"))
	{
		// A 4 m ring of sound: knocked back 4 m; lure lines within 8 m snap; the colony comes (its rally call).
		const FVector At = Self->GetActorLocation();
		const float Rad = Row.Radius > 0.f ? Row.Radius : 400.f;
		WRFx::Broadcast(Self, TEXT("fungal.croak"), At, Self->GetActorRotation(), Rad / 200.f, TEXT("creature.tunnel_toad.signature"));
		for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
		{
			if (*It != Self && It->IsAlive() && Self->IsHostileTo(*It) && FVector::Dist(It->GetActorLocation(), At) < Rad + It->GetSimpleCollisionRadius()) AI->RowHit(Self, *It);
		}
		for (const TWeakObjectPtr<AWRLureLines>& L : AWRLureLines::All()) if (L.IsValid()) L->CutNear(At, FkT(TEXT("toad.croakSnaps"), 800.f), TEXT("croak"));
		for (const TWeakObjectPtr<UWRFungalBody>& O : UWRFungalBody::All())
		{
			AWRCreature* T = O.IsValid() ? Cast<AWRCreature>(O->GetOwner()) : nullptr;
			if (T && T != Self && FkIs(T, TEXT("tunnel_toad")) && !T->IsTamed() && !T->GetAggroTarget() && Target && FVector::Dist(T->GetActorLocation(), At) < FkT(TEXT("toad.rally"), 3000.f)) T->SetAggroTarget(Target);
		}
		return true;
	}
	if (Id == TEXT("leap_slam"))
	{
		// A leap for a point up to 12 m off (where you stood), landing in a low ring (TickBody).
		FVector Land = AI->RowAim;
		const FVector From = Self->GetActorLocation();
		const float MaxReach = FkT(TEXT("toad.leapReach"), 1200.f);
		if (FVector::Dist2D(Land, From) > MaxReach) Land = From + (Land - From).GetSafeNormal2D() * MaxReach;
		Land.Z += Self->GetSimpleCollisionHalfHeight();
		B->bLeaping = true;
		Self->SetActorRotation(FRotator(0.f, (Land - From).Rotation().Yaw, 0.f));
		Self->LaunchCharacter(AWRProjectile::LobVelocity(From, Land, Row.Speed > 0.f ? Row.Speed : 1400.f, 1.f), true, true);
		return true;
	}
	if (Id == TEXT("rot_belch"))
	{
		// A 6 m x 40 degree cone of rot gas, four ticks over a second (rot 18 each); not a projectile, so no block.
		const FVector At = Self->GetActorLocation();
		const FVector Fwd = Self->GetActorForwardVector().GetSafeNormal2D();
		WRFx::Broadcast(Self, TEXT("fungal.rotBelch"), Self->MouthLocation(), Self->GetActorRotation(), 1.f, TEXT("creature.rotmaw.belch"));
		TWeakObjectPtr<AWRCreature> WeakSelf(Self);
		const float Rad = Row.Radius > 0.f ? Row.Radius : 600.f, HalfArc = Row.Arc > 0.f ? Row.Arc : 20.f;
		const float Dmg = Self->Weapon.FlatDamage * Row.Dmg;
		const TMap<FName, float> Bu = Row.Buildups;
		for (int32 k = 0; k < 4; ++k)
		{
			FTimerHandle H;
			World->GetTimerManager().SetTimer(H, [WeakSelf, At, Fwd, Rad, HalfArc, Dmg, Bu]()
			{
				AWRCreature* S = WeakSelf.Get();
				if (!S || !S->IsAlive()) return;
				for (TActorIterator<AWRCharacterBase> It(S->GetWorld()); It; ++It)
				{
					if (*It == S || !It->IsAlive() || !S->IsHostileTo(*It)) continue;
					const FVector To = It->GetActorLocation() - At;
					if (To.Size2D() > Rad + It->GetSimpleCollisionRadius() || FVector::DotProduct(To.GetSafeNormal2D(), Fwd) < FMath::Cos(FMath::DegreesToRadians(HalfArc))) continue;
					FkHit(S, *It, Dmg, 0.f, 1, Bu, true, false, false);
				}
			}, 0.05f + k * 0.25f, false);
		}
		return true;
	}
	return false;
}

void FWRFungalAI::Resolved(AWRCreature* Self, AWRCharacterBase* Victim, FName RowId, EWRHitResult Result)
{
	UWRFungalBody* B = UWRFungalBody::Get(Self);
	if (!B || !Victim) return;
	const bool bLanded = Result == EWRHitResult::Hit || Result == EWRHitResult::GuardBroken;
	if (!bLanded) return;
	if (RowId == TEXT("latch"))
	{
		// On (one per limb, at most four): a block flicked it off already (not landed).
		if (UWRFungalBody::LatchedOn(Victim) >= int32(FkT(TEXT("mycomite.maxLatched"), 4.f))) return;
		B->Victim = Victim;
		B->Set(WRFungalFlag::Latched, true);
		B->Until[FkBit(WRFungalFlag::Latched)] = FkNow(Self) + FkT(TEXT("mycomite.latchSeconds"), 8.f);
		B->LatchTickAt = FkNow(Self) + 1.0;
		Self->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
		WRSound::PlayAt(Self, TEXT("creature.mycomite.latch"), Victim->GetActorLocation());
	}
	else if (RowId == TEXT("lure_snap") && !UWRFungalBody::HolderOf(Victim))
	{
		// In its beak 1.5 s at 15 a second.
		B->Victim = Victim;
		B->Set(WRFungalFlag::Holding, true);
		B->Until[FkBit(WRFungalFlag::Holding)] = FkNow(Self) + FkT(TEXT("rotmaw.holdSeconds"), 1.5f);
		B->HoldTickAt = FkNow(Self) + 0.5;
		if (Victim->Status) Victim->Status->Stagger(FkT(TEXT("rotmaw.holdSeconds"), 1.5f));
	}
	else if (RowId == TEXT("tongue_whip"))
	{
		// The tongue's pull: 3 m in toward the mouth.
		const FVector Pull = (Self->GetActorLocation() - Victim->GetActorLocation()).GetSafeNormal2D() * FkT(TEXT("toad.tonguePull"), 300.f) * 2.5f;
		Victim->LaunchCharacter(Pull + FVector(0.f, 0.f, 150.f), true, true);
	}
}

float FWRFungalAI::Incoming(AWRCreature* Self, const FWRHit& Hit, float Damage)
{
	UWRFungalBody* B = UWRFungalBody::Get(Self);
	if (!B || Damage <= 0.f) return Damage;
	// A hit on a latched mite takes it off (an ally's blow, the victim's own).
	if (B->Has(WRFungalFlag::Latched)) { B->Set(WRFungalFlag::Latched, false); B->Victim = nullptr; Self->GetCharacterMovement()->SetMovementMode(MOVE_Walking); }
	// The Rotmaw's lure struck from range while it waits: it comes out stunned 2 s (Ilmari's "The lone cap").
	if (FkIs(Self, TEXT("rotmaw")) && B->Has(WRFungalFlag::Mound))
	{
		const FVector P = Hit.ImpactPoint.IsZero() ? Hit.Origin : Hit.ImpactPoint;
		const bool bRange = Hit.bProjectile || (Hit.Instigator && FVector::Dist(Hit.Instigator->GetActorLocation(), Self->GetActorLocation()) > 600.f);
		if (bRange && FVector::Dist(P, B->LurePoint()) < FkT(TEXT("rotmaw.lureHit"), 160.f) + 60.f)
		{
			B->Set(WRFungalFlag::Mound, false);
			if (Self->Status) Self->Status->Stagger(FkT(TEXT("rotmaw.lureStun"), 2.f));
			if (const APawn* By = Cast<APawn>(Hit.Instigator); By && By->GetPlayerState()) B->LureStruckBy = By->GetPlayerState()->GetPlayerName();
			WRFx::Broadcast(Self, TEXT("fungal.moundBurst"), Self->GetActorLocation(), FRotator::ZeroRotator, 1.5f, TEXT("creature.rotmaw.burst"));
			FkTell(Cast<AActor>(Hit.Instigator), TEXT("The lure jerks back and the mound bursts open: a Rotmaw, stunned"));
			if (AWRCharacterBase* By = Cast<AWRCharacterBase>(Hit.Instigator)) Self->SetAggroTarget(By);
		}
		else if (B->Has(WRFungalFlag::Mound))
		{
			B->Set(WRFungalFlag::Mound, false);
			if (AWRCharacterBase* By = Cast<AWRCharacterBase>(Hit.Instigator)) Self->SetAggroTarget(By);
		}
	}
	// Its belly shows when it rears (the belch) or tips (the spin): x1.3 there.
	if (FkIs(Self, TEXT("rotmaw")))
	{
		if (Self->HasTag(WRTags::State_Attacking) && Hit.ImpactPoint.Z != 0.f && Hit.ImpactPoint.Z < Self->GetActorLocation().Z - Self->GetSimpleCollisionHalfHeight() * 0.2f) Damage *= FkT(TEXT("rotmaw.belly"), 1.3f);
	}
	// A Shroomback tucked in its shell: damage x0.2, a strike x1.5 of that.
	if (B->Has(WRFungalFlag::Tucked)) Damage *= FkT(TEXT("shroomback.tuckedTaken"), 0.2f) * (Hit.PhysType == 2 ? FkT(TEXT("shroomback.tuckedStrike"), 1.5f) : 1.f);
	// A Puffball touched by fire bursts at once (the death's burst), and its cloud flashes in the fire a breath later.
	if (FkIs(Self, TEXT("puffball")) && (Hit.Element == WRTags::Element_Fire || Hit.BurnBuildup > 0.f))
	{
		TWeakObjectPtr<AActor> By(Cast<AActor>(Hit.Instigator));
		const FVector Here = Self->GetActorLocation() + FVector(0.f, 0.f, Self->GetSimpleCollisionHalfHeight() * 0.4f);
		UWorld* W = Self->GetWorld();
		FTimerHandle H;
		W->GetTimerManager().SetTimer(H, [By, Here]() { if (By.IsValid()) WRFungal::FireAt(By.Get(), Here, 500.f); }, 0.1f, false);
		return FMath::Max(Damage, 100000.f);
	}
	// A ceiling hunter struck comes down.
	if (B->Has(WRFungalFlag::Ceiling)) { B->Set(WRFungalFlag::Ceiling, false); B->bDropping = true; Self->GetCharacterMovement()->SetMovementMode(MOVE_Falling); }
	return Damage;
}

void FWRFungalAI::Damaged(AWRCreature* Self, const FWRHit& Hit, float Damage)
{
	UWRFungalBody* B = UWRFungalBody::Get(Self);
	if (!B || !Self->IsAlive()) return;
	const double Now = FkNow(Self);
	const FVector At = Self->GetActorLocation();
	// The Sporestalker's spore shake: 60+ damage in 2 s, its stalks flare and a 3 m puff (poison 25); 6 s cooldown.
	if (FkIs(Self, TEXT("sporestalker")))
	{
		if (Now - B->DamageWindowAt > 2.0) { B->DamageWindowAt = Now; B->DamageWindow = 0.f; }
		B->DamageWindow += Damage;
		if (B->DamageWindow >= FkT(TEXT("sporestalker.shakeDamage"), 60.f) && Now >= B->NextShakeAt)
		{
			B->NextShakeAt = Now + FkT(TEXT("sporestalker.shakeCooldown"), 6.f);
			B->DamageWindow = 0.f;
			B->Set(WRFungalFlag::Crest, true);
			B->Until[FkBit(WRFungalFlag::Crest)] = Now + 0.6;
			TWeakObjectPtr<AWRCreature> WeakSelf(Self);
			FTimerHandle H;
			Self->GetWorldTimerManager().SetTimer(H, [WeakSelf]()
			{
				if (AWRCreature* S = WeakSelf.Get(); S && S->IsAlive()) FkPuff(S, S->GetActorLocation(), FkT(TEXT("sporestalker.shakeRadius"), 300.f), 4.f, FkT(TEXT("sporestalker.shakePoison"), 25.f), TEXT("fungal.sporeShake"));
			}, FkT(TEXT("sporestalker.shakeTell"), 0.5f), false);
		}
	}
	// The Shroomback's garden puffs when its shell is struck (3 m, poison 20, 8 s); after a quarter of its stance in
	// 2 s it tucks 3 s.
	if (FkIs(Self, TEXT("shroomback")))
	{
		const FVector From = (Hit.Origin - At).GetSafeNormal2D();
		const bool bShell = FVector::DotProduct(From, Self->GetActorForwardVector()) < 0.6f;
		if (bShell && Now >= B->NextPuffAt)
		{
			B->NextPuffAt = Now + FkT(TEXT("shroomback.puffCooldown"), 8.f);
			TWeakObjectPtr<AWRCreature> WeakSelf(Self);
			FTimerHandle H;
			Self->GetWorldTimerManager().SetTimer(H, [WeakSelf]()
			{
				if (AWRCreature* S = WeakSelf.Get(); S && S->IsAlive()) FkPuff(S, S->GetActorLocation(), FkT(TEXT("shroomback.puffRadius"), 300.f), 4.f, FkT(TEXT("shroomback.puffPoison"), 20.f), TEXT("fungal.gardenPuff"));
			}, 0.4f, false);
		}
		if (Now - B->StanceWindowAt > 2.0) { B->StanceWindowAt = Now; B->StanceWindow = 0.f; }
		B->StanceWindow += Hit.PoiseDamage;
		const FWRCreatureDef* D = Self->GetDef();
		if (D && B->StanceWindow >= D->Poise * FkT(TEXT("shroomback.tuckShare"), 0.25f) * 4.f && !B->Has(WRFungalFlag::Tucked))
		{
			B->Set(WRFungalFlag::Tucked, true);
			B->Until[FkBit(WRFungalFlag::Tucked)] = Now + FkT(TEXT("shroomback.tuckSeconds"), 3.f);
			B->StanceWindow = 0.f;
			WRSound::PlayAt(Self, TEXT("creature.shroomback.tuck"), At);
		}
	}
	// Passives' reactions: the Glowgrub flashes (a 1.5 s dim on everyone in 3 m) and crawls off; the Capling puffs.
	if (FkIs(Self, TEXT("glowgrub")) && Now >= B->NextPuffAt)
	{
		B->NextPuffAt = Now + 6.0;
		WRFx::Broadcast(Self, TEXT("fungal.glowFlash"), At, FRotator::ZeroRotator, 1.5f, TEXT("creature.glowgrub.flash"));
		for (TActorIterator<AWRCharacterBase> It(Self->GetWorld()); It; ++It)
		{
			if (*It == Self || !It->IsAlive() || FVector::Dist(It->GetActorLocation(), At) > FkT(TEXT("glowgrub.flashRadius"), 300.f)) continue;
			if (AWRPlayerController* PC = Cast<AWRPlayerController>(It->GetController()))
			{
				PC->ClientSetCameraFade(true, FColor::Black, FVector2D(FkT(TEXT("glowgrub.flashDim"), 0.7f), 0.f), FkT(TEXT("glowgrub.flashSeconds"), 1.5f), false, false);
			}
		}
	}
	if (FkIs(Self, TEXT("capling")) && Now >= B->NextPuffAt)
	{
		B->NextPuffAt = Now + 6.0;
		if (B->Has(WRFungalFlag::Riding)) { B->Set(WRFungalFlag::Riding, false); Self->GetCharacterMovement()->SetMovementMode(MOVE_Falling); }
		FkPuff(Self, At, FkT(TEXT("capling.puffRadius"), 200.f), 3.f, FkT(TEXT("capling.puffPoison"), 10.f), TEXT("fungal.puff"));
	}
}

void FWRFungalAI::Died(AWRCreature* Self, AActor* Killer)
{
	UWRFungalBody* B = UWRFungalBody::Get(Self);
	if (!B) return;
	const FVector At = Self->GetActorLocation();
	// The swarm bursts: a 2 m puff (poison 15); a Puffball's sac a 6 m cloud for 8 s (poison 30).
	if (FkIs(Self, TEXT("mycomite")) || FkIs(Self, TEXT("sporeling"))) FkPuff(Self, At, FkT(TEXT("mycomite.burstRadius"), 200.f), 3.f, FkT(TEXT("mycomite.burstPoison"), 15.f), TEXT("fungal.miteBurst"));
	if (FkIs(Self, TEXT("puffball"))) FkPuff(Self, At + FVector(0.f, 0.f, Self->GetSimpleCollisionHalfHeight() * 0.4f), FkT(TEXT("puffball.burstRadius"), 600.f), FkT(TEXT("puffball.burstSeconds"), 8.f), FkT(TEXT("puffball.burstPoison"), 30.f), TEXT("fungal.puffballBurst"));
	if (B->Has(WRFungalFlag::Holding) || B->Has(WRFungalFlag::Latched)) { B->Victim = nullptr; B->Flags &= ~(WRFungalFlag::Holding | WRFungalFlag::Latched); }
	// Ilmari's "The lone cap": a Rotmaw whose lure you struck first, killed by you.
	if (FkIs(Self, TEXT("rotmaw")) && !B->LureStruckBy.IsEmpty())
	{
		const APawn* K = Cast<APawn>(Killer);
		if (const AWRCreature* Tame = Cast<AWRCreature>(Killer); Tame && Tame->IsTamed()) K = Cast<APawn>(FkOwnerOf(Tame));
		AWRPlayerState* PS = K ? K->GetPlayerState<AWRPlayerState>() : nullptr;
		if (PS && PS->GetPlayerName() == B->LureStruckBy)
		{
			if (UWRWorldSystems* WS = Self->GetWorld()->GetSubsystem<UWRWorldSystems>()) WS->QuestEvent(PS, TEXT("event"), TEXT("rotmaw_lure_first"), 1);
			UE_LOG(LogWildreach, Log, TEXT("FUNGAL %s killed a Rotmaw after striking its lure"), *B->LureStruckBy);
		}
	}
}

// ================================================================================================ webs, latches, mash

void FWRFungalAI::Web(AWRCharacterBase* Victim, float Seconds, AActor* By)
{
	if (!Victim || !Victim->HasAuthority() || !Victim->IsAlive() || !Victim->Status) return;
	Victim->Status->ApplyStatus(WRTags::Status_Rooted, Seconds + 0.5f);
	FkWebbed().Add(Victim, FkNow(Victim) + Seconds);
	WRFx::Broadcast(Victim, TEXT("fungal.webbed"), Victim->GetActorLocation(), FRotator::ZeroRotator, 1.f, TEXT("fungal.web_tear"));
	FkTell(Victim, TEXT("Webbed: press buttons to tear free"));
}

bool FWRFungalAI::Mash(AWRCharacterBase* Victim)
{
	if (!Victim) return false;
	if (double* Until = FkWebbed().Find(Victim))
	{
		*Until -= FkT(TEXT("webspinner.mash"), 0.15f);
		return true;
	}
	if (AWRCreature* H = UWRFungalBody::HolderOf(Victim))
	{
		if (UWRFungalBody* B = UWRFungalBody::Get(H)) B->Until[FkBit(WRFungalFlag::Holding)] -= FkT(TEXT("rotmaw.mash"), 0.15f);
		return true;
	}
	return false;
}

float FWRFungalAI::MoveScale(const AActor* Who)
{
	const int32 N = UWRFungalBody::LatchedOn(Who);
	return N > 0 ? FMath::Pow(FkT(TEXT("mycomite.latchSlow"), 0.9f), float(N)) : 1.f;
}

AWRCreature* FWRFungalAI::SpawnSporeling(UWorld* World, const FVector& At, AActor* Toward)
{
	if (!World) return nullptr;
	AWRCreature* C = AWRCreature::SpawnCreature(World, TEXT("sporeling"), At, FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f));
	if (C && Toward) C->SetAggroTarget(Toward);
	if (C) WRFx::Broadcast(C, TEXT("fungal.puff"), At, FRotator::ZeroRotator, 0.8f, TEXT("creature.sporeling.signature"));
	return C;
}

// ================================================================================================ the dens

void UWRFungalWorld::StepDens(float Dt)
{
	// The dens (fungal 2.12.3): the generator's dens (Data/fungal.json: F1's surface spread, every cave hall but the
	// arenas, the Ring of Caps' toads, the Heart Grotto's few) fill from their own table or their sub-zone's (day or
	// night: underground night is 22:00 to 04:00), weights picking the kinds and each kind's pack size its number, when a
	// survivor comes within fungalLife.denNear; and empty when none is within denFar (no creature lives where no one is).
	DenClock -= Dt;
	if (DenClock > 0.f) return;
	DenClock = FkT(TEXT("denEvery"), 5.f);
	UWorld* World = GetWorld();
	const FWRFungalData& D = FWRFungalData::Get();
	if (DenLive.Num() != D.Dens.Num()) DenLive.SetNum(D.Dens.Num());
	const double Now = World->GetTimeSeconds();
	const float Hour = AWRGameState::HourIn(World);
	const float Near = FkT(TEXT("denNear"), 9000.f), Far = FkT(TEXT("denFar"), 14000.f);
	TArray<FVector> Players;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It) if (const APawn* P = It->Get() ? It->Get()->GetPawn() : nullptr) Players.Add(P->GetActorLocation());
	for (int32 i = 0; i < D.Dens.Num(); ++i)
	{
		const FWRFungalData::FDen& Dn = D.Dens[i];
		FDenLive& L = DenLive[i];
		float Closest = TNumericLimits<float>::Max();
		for (const FVector& P : Players) Closest = FMath::Min(Closest, float(FVector::Dist(P, Dn.At)));
		L.Alive.RemoveAll([](const TWeakObjectPtr<AWRCreature>& C) { return !C.IsValid() || !C->IsAlive(); });
		const bool bNight = Dn.bCave ? (Hour >= 22.f || Hour < 4.f) : AWRGameState::IsNightIn(World);
		if (Closest > Far || bNight != L.bNight)
		{
			for (const TWeakObjectPtr<AWRCreature>& C : L.Alive) if (C.IsValid() && !C->IsTamed() && !C->GetAggroTarget()) C->Destroy();
			L.Alive.RemoveAll([](const TWeakObjectPtr<AWRCreature>& C) { return !C.IsValid(); });
			L.bNight = bNight;
			if (Closest > Far) continue;
		}
		if (Closest > Near || L.Alive.Num() > 0 || Now < L.NextAt) continue;
		L.NextAt = Now + FkT(TEXT("denRespawn"), 600.f);
		const TMap<FName, float>* Own = bNight ? &Dn.Night : &Dn.Day;
		const TPair<TMap<FName, float>, TMap<FName, float>>* Zone = D.DenTables.Find(Dn.Zone);
		const TMap<FName, float>* Table = Own->Num() > 0 ? Own : Zone ? (bNight ? &Zone->Value : &Zone->Key) : nullptr;
		if (!Table || Table->Num() == 0) continue;
		// The kinds home tonight: one weighted pick, a second at denSecond chance (a den is a pack, not a zoo).
		FRandomStream Rng(int32(Now * 10.0) ^ int32(Dn.At.X) ^ (i * 7919));
		float Total = 0.f;
		for (const TPair<FName, float>& KV : *Table) Total += KV.Value;
		TArray<FName> Kinds;
		for (int32 pick = 0; pick < 2; ++pick)
		{
			if (pick == 1 && Rng.FRand() > FkT(TEXT("denSecond"), 0.4f)) break;
			float Roll = Rng.FRand() * Total;
			for (const TPair<FName, float>& KV : *Table) { Roll -= KV.Value; if (Roll <= 0.f) { Kinds.AddUnique(KV.Key); break; } }
		}
		int32 Made = 0;
		for (const FName Kind : Kinds)
		{
			const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(Kind);
			if (!Def) continue;
			int32 N = Rng.RandRange(FMath::Max(1, Def->PackMin), FMath::Max(1, Def->PackMax));
			if (Dn.Max > 0) N = FMath::Min(N, Dn.Max - Made);
			for (int32 k = 0; k < N; ++k)
			{
				const FVector Try = Dn.At + FRotator(0.f, Rng.FRandRange(0.f, 360.f), 0.f).Vector() * Rng.FRandRange(0.f, Dn.Spread);
				FHitResult G;
				if (!World->LineTraceSingleByChannel(G, Try + FVector(0.f, 0.f, Dn.bCave ? 300.f : 3000.f), Try - FVector(0.f, 0.f, Dn.bCave ? 900.f : 6000.f), ECC_WorldStatic)) continue;
				if (AWRCreature* C = AWRCreature::SpawnCreature(World, Kind, G.ImpactPoint + FVector(0.f, 0.f, Def->HalfHeight + 30.f), FRotator(0.f, Rng.FRandRange(0.f, 360.f), 0.f)))
				{
					C->SetHome(Dn.At);
					L.Alive.Add(C);
					++Made;
				}
			}
		}
	}
}
