// The Swamp's creatures (cooks/biome-swamp.md 2.12; cooks/build-biome-swamp.md 2.6). See WRSwampCreatures.h.
// The Storm peaks pattern (WRStormCreatures.cpp): rows in Data/attacks.json, numbers in tuning swampCreatures.*,
// stand-in rigs with the cook's palette and code-built silhouette parts on bone frames.
#include "WRSwampCreatures.h"
#include "GameFramework/PlayerState.h"
#include "WRSwampTypes.h"
#include "WRSwampSystems.h"
#include "WRBloodPath.h"
#include "WRCreature.h"
#include "WRBoss.h"
#include "WRAttackRows.h"
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
#include "WRProjectile.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRTuning.h"
#include "WRWater.h"
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
#include "GameFramework/ProjectileMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

// ---------------------------------------------------------------- shared bits

namespace WRSwampCreaturesLocal
{
	/** creatures.json rows by id, read once (the look and the tame-use keys the def doesn't carry). */
	TSharedPtr<FJsonObject> SwcRow(FName Id)
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

	float SwcNum(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, float Fallback)
	{
		double V = 0.0;
		return O.IsValid() && O->TryGetNumberField(Field, V) ? float(V) : Fallback;
	}

	FLinearColor SwcColor(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, const FLinearColor& Fallback)
	{
		FString S;
		if (O.IsValid() && O->TryGetStringField(Field, S)) return FLinearColor(FColor::FromHex(S));
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (O.IsValid() && O->TryGetArrayField(Field, A) && A->Num() >= 3) return FLinearColor((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), (*A)[2]->AsNumber());
		return Fallback;
	}

	FLinearColor SwcHex(const TCHAR* S) { return FLinearColor(FColor::FromHex(S)); }

	double SwcNow(const UObject* W) { return W && W->GetWorld() ? W->GetWorld()->GetTimeSeconds() : 0.0; }

	/** tuning swampCreatures.<Key>. */
	float SwcT(const TCHAR* Key, float Fallback) { return FWRTuning::Get().Num(FString::Printf(TEXT("swampCreatures.%s"), Key), Fallback); }

	/** A [lo, hi] pair in tuning swampCreatures.<Group>.<Field>. */
	void SwcRange(const TCHAR* Group, const TCHAR* Field, float& Lo, float& Hi)
	{
		const TSharedPtr<FJsonObject> O = FWRTuning::Get().Object(FString::Printf(TEXT("swampCreatures.%s"), Group));
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (O.IsValid() && O->TryGetArrayField(Field, A) && A->Num() >= 2)
		{
			Lo = float((*A)[0]->AsNumber());
			Hi = float((*A)[1]->AsNumber());
		}
	}

	float SwcRand(const TCHAR* Group, const TCHAR* Field, float Lo, float Hi)
	{
		SwcRange(Group, Field, Lo, Hi);
		return FMath::FRandRange(Lo, FMath::Max(Lo, Hi));
	}

	bool SwcWild(const AWRCreature* C) { return C && C->IsAlive() && !C->IsTamed(); }

	TSharedPtr<FJsonObject> SwcLook(FName Id)
	{
		const TSharedPtr<FJsonObject> R = SwcRow(Id);
		const TSharedPtr<FJsonObject>* Look = nullptr;
		return R.IsValid() && R->TryGetObjectField(TEXT("look"), Look) ? *Look : nullptr;
	}

	/** The body the look is built for (look.kind): murkjaw_young looks like a Murkjaw. */
	FName SwcKind(FName Id)
	{
		const TSharedPtr<FJsonObject> L = SwcLook(Id);
		FString S;
		return L.IsValid() && L->TryGetStringField(TEXT("kind"), S) ? FName(*S) : Id;
	}

	FVector SwcGround(const UWorld* World, const FVector& At, bool& bOk, const AActor* Ignore = nullptr)
	{
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRSwampCreatureGround), false, Ignore);
		bOk = World && World->LineTraceSingleByChannel(Hit, At + FVector(0.f, 0.f, 3000.f), At - FVector(0.f, 0.f, 6000.f), ECC_WorldStatic, Q);
		return bOk ? FVector(Hit.ImpactPoint) : At;
	}

	FVector SwcFeet(const AActor* A) { return A->GetActorLocation() - FVector(0.f, 0.f, A->GetSimpleCollisionHalfHeight()); }

	/** Water under it: swimming, or its feet below the drawn surface over a swamp water cell. */
	bool SwcInWater(const ACharacter* C)
	{
		if (!C) return false;
		if (C->GetCharacterMovement() && C->GetCharacterMovement()->IsSwimming()) return true;
		const FVector P = C->GetActorLocation();
		return WRSwamp::Contains(P) && WRSwamp::WaterAt(C, P) != EWRSwampWater::Dry && WRSwamp::IsWet(SwcFeet(C) + FVector(0.f, 0.f, 10.f));
	}

	/** Water within Range of a point (eight samples round it and the point itself). */
	bool SwcWaterNear(const UObject* Ctx, const FVector& At, float Range)
	{
		if (!WRSwamp::Contains(At)) return false;
		if (WRSwamp::WaterAt(Ctx, At) != EWRSwampWater::Dry) return true;
		for (int32 i = 0; i < 8; ++i)
		{
			const FVector P = At + FRotator(0.f, i * 45.f, 0.f).Vector() * Range;
			if (WRSwamp::WaterAt(Ctx, P) != EWRSwampWater::Dry) return true;
		}
		return false;
	}

	AWRCharacter* SwcOwnerBody(const UWorld* World, const FString& Name)
	{
		for (TActorIterator<AWRCharacter> It(World); It; ++It)
		{
			const APlayerState* PS = It->GetPlayerState();
			if (PS && PS->GetPlayerName() == Name && It->IsAlive()) return *It;
		}
		return nullptr;
	}

	APlayerController* SwcOwnerPC(const UWorld* World, const FString& Name)
	{
		const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
		for (APlayerState* PS : GS ? GS->PlayerArray : TArray<TObjectPtr<APlayerState>>())
		{
			if (PS && PS->GetPlayerName() == Name) return Cast<APlayerController>(PS->GetOwner());
		}
		return nullptr;
	}

	void SwcTell(const AActor* Who, const FString& Text)
	{
		const APawn* P = Cast<APawn>(Who);
		if (AWRPlayerController* PC = P ? Cast<AWRPlayerController>(P->GetController()) : nullptr) PC->ClientNotify(Text);
	}

	float SwcHealthFrac(const AWRCharacterBase* C)
	{
		if (const AWRCreature* Cr = Cast<AWRCreature>(C)) return Cr->GetHealthFraction();
		const UWRAttributeSet* A = C ? C->GetAttributes() : nullptr;
		return A && A->GetMaxHealth() > 0.f ? A->GetHealth() / A->GetMaxHealth() : 1.f;
	}

	float SwcDifficulty() { return FMath::Max(0.1f, WRTune(TEXT("server.creatureDifficulty"), 1.f)); }

	bool SwcDay(const UObject* Ctx) { return !AWRGameState::IsNightIn(Ctx); }

	/** The nearest living survivor within Range (not hidden, not in a mouth). */
	AWRCharacter* SwcNearestSurvivor(const AActor* Self, float Range)
	{
		AWRCharacter* Best = nullptr;
		float BestD = Range;
		for (TActorIterator<AWRCharacter> It(Self->GetWorld()); It; ++It)
		{
			if (!It->IsAlive() || It->IsHidden()) continue;
			const float D = FVector::Dist(It->GetActorLocation(), Self->GetActorLocation());
			if (D < BestD) { BestD = D; Best = *It; }
		}
		return Best;
	}

	/** A hold's or a leech's tick of damage: through ApplyHit, a grab no guard stops, with its bleed. */
	void SwcDot(AWRCreature* By, AWRCharacterBase* Victim, float Damage, float Bleed)
	{
		FWRHit Hit;
		Hit.Instigator = By;
		Hit.Damage = Damage;
		Hit.PoiseDamage = 0.f;
		Hit.Impact = 1;
		Hit.bDamageOverTime = true;
		Hit.bGrab = true;
		Hit.bUnblockable = true;
		Hit.PhysType = 3;
		Hit.Element = WRTags::Element_Physical;
		Hit.Origin = By->GetActorLocation();
		if (Bleed > 0.f) Hit.Buildups.Add(TEXT("bleed"), Bleed);
		UWRCombatLibrary::ApplyHit(Victim, Hit);
	}

	/** Where a leech hangs on a body (its capsule's space): left arm, right arm, left leg, right leg, then the back. */
	FVector SwcLimbOffset(const AActor* Victim, int32 Slot)
	{
		const float R = Victim->GetSimpleCollisionRadius(), H = Victim->GetSimpleCollisionHalfHeight();
		switch (Slot)
		{
		case 0: return FVector(4.f, -R * 0.95f, H * 0.32f);
		case 1: return FVector(4.f, R * 0.95f, H * 0.32f);
		case 2: return FVector(R * 0.25f, -R * 0.55f, -H * 0.5f);
		case 3: return FVector(R * 0.25f, R * 0.55f, -H * 0.5f);
		default: return FVector(-R * 0.9f, (Slot % 2 ? 1.f : -1.f) * R * 0.3f, H * (0.1f + 0.1f * (Slot - 4)));
		}
	}

	FName SwcLimbName(int32 Slot)
	{
		static const FName Names[] = { TEXT("arm_l"), TEXT("arm_r"), TEXT("leg_l"), TEXT("leg_r"), TEXT("back") };
		return Names[FMath::Clamp(Slot, 0, 4)];
	}

	/** Its feet over swamp water no deeper than Depth cm: the shallow edge (a Mirestrider's fishing water). */
	bool SwcShallow(const AWRCreature* C, float Depth)
	{
		if (!SwcInWater(C)) return false;
		const UWRWaterSubsystem* W = UWRWaterSubsystem::Get(C);
		return !W || W->DepthAt(C->GetActorLocation()) <= Depth;
	}

	/** The nearest water point (a swamp water cell) within Range of At, from rings of samples; zero if none. */
	FVector SwcNearestWater(const UObject* Ctx, const FVector& At, float Range)
	{
		for (float R = 400.f; R <= Range; R += 400.f)
		{
			const int32 N = FMath::Clamp(int32(R / 250.f), 8, 48);
			for (int32 i = 0; i < N; ++i)
			{
				const FVector P = At + FRotator(0.f, 360.f * i / N, 0.f).Vector() * R;
				if (WRSwamp::Contains(P) && WRSwamp::WaterAt(Ctx, P) != EWRSwampWater::Dry) return P;
			}
		}
		return FVector::ZeroVector;
	}

	bool SwcIsLeech(FName Id) { return Id == TEXT("gorgeleech") || Id == TEXT("latch_leech"); }
}
using namespace WRSwampCreaturesLocal;

// ---------------------------------------------------------------- the body component

TArray<TWeakObjectPtr<UWRSwampBody>> UWRSwampBody::Live;

UWRSwampBody::UWRSwampBody()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.f;
	SetIsReplicatedByDefault(true);
}

void UWRSwampBody::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UWRSwampBody, Flags);
	DOREPLIFETIME(UWRSwampBody, Holding);
	DOREPLIFETIME(UWRSwampBody, RiteState);
}

UWRSwampBody* UWRSwampBody::Get(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UWRSwampBody>() : nullptr;
}

bool UWRSwampBody::IsSwampKind(FName CreatureId)
{
	const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(CreatureId);
	return Def && Def->Biome == TEXT("swamp") && Def->Archetype != TEXT("boss");
}

void UWRSwampBody::Attach(AWRCreature* Creature)
{
	if (!Creature || !Creature->HasAuthority() || !IsSwampKind(Creature->CreatureId) || Get(Creature) || Creature->IsA<AWRBoss>())
	{
		return;
	}
	UWRSwampBody* Body = NewObject<UWRSwampBody>(Creature, TEXT("SwampBody"));
	Body->RegisterComponent();
	Creature->AddInstanceComponent(Body);
}

void UWRSwampBody::BeginPlay()
{
	Super::BeginPlay();
	Live.RemoveAll([](const TWeakObjectPtr<UWRSwampBody>& B) { return !B.IsValid(); });
	Live.Add(this);
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	const FWRCreatureDef* Def = C ? C->GetDef() : nullptr;
	if (!C || !Def)
	{
		return;
	}
	// Waders walk the bottom (a hippo doesn't swim, it walks): every machine that moves it sinks it in deep water, at
	// its own pace there.
	const TSharedPtr<FJsonObject> R = SwcRow(C->CreatureId);
	if (SwcNum(R, TEXT("wadeDepth"), 0.f) > 0.f)
	{
		C->GetCharacterMovement()->Buoyancy = SwcT(TEXT("wade.buoyancy"), 0.4f);
		C->GetCharacterMovement()->MaxSwimSpeed = Def->Speed * SwcT(TEXT("wade.speed"), 0.6f);
	}
	if (C->HasAuthority())
	{
		const double T = SwcNow(this);
		NextFindAt = T + FMath::FRandRange(5.f, 15.f);
		NextYawnAt = T;
		NextStabAt = T + FMath::FRandRange(10.f, 60.f);
		// A young Murkjaw keeps to the channel mouths (cook 2.12.3): its home is the nearest water.
		if (C->CreatureId == TEXT("murkjaw_young"))
		{
			const FVector W = SwcNearestWater(C, C->GetActorLocation(), SwcT(TEXT("murkjaw.youngWaterSearch"), 4000.f));
			if (!W.IsZero()) C->SetHome(W);
		}
	}
}

void UWRSwampBody::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Reason == EEndPlayReason::Destroyed && GetOwner() && GetOwner()->HasAuthority())
	{
		AWRCreature* C = Cast<AWRCreature>(GetOwner());
		if (C && Held.IsValid()) FWRSwampAI::Release(C, this, false);
		if (C && LatchedOn.IsValid()) FWRSwampAI::Unlatch(C, this, false);
	}
	ClearLook();
	Live.RemoveAll([this](const TWeakObjectPtr<UWRSwampBody>& B) { return !B.IsValid() || B.Get() == this; });
	Super::EndPlay(Reason);
}

USkeletalMeshComponent* UWRSwampBody::Skel() const
{
	const AWRCreature* C = Cast<AWRCreature>(GetOwner());
	return C ? C->GetCreatureSkel() : nullptr;
}

void UWRSwampBody::MulticastMoment_Implementation(FName Effect, FName Bone, float Scale)
{
	const AWRCreature* C = Cast<AWRCreature>(GetOwner());
	if (!C || GetNetMode() == NM_DedicatedServer) return;
	const FName B = C->FindBone(Bone);
	const FVector At = B.IsNone() || !Skel() ? C->GetActorLocation() : Skel()->GetBoneLocation(B);
	WRFx::SpawnAt(C, Effect, At, C->GetActorRotation(), Scale);
}

void UWRSwampBody::MulticastLine_Implementation(FName Effect, FVector_NetQuantize From, FVector_NetQuantize To, float Seconds)
{
	if (GetNetMode() == NM_DedicatedServer) return;
	FLineFx L;
	L.Effect = Effect;
	L.From = From;
	L.To = To;
	L.Seconds = FMath::Max(0.1f, Seconds);
	Lines.Add(L);
}

void UWRSwampBody::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	Clock += DeltaTime;
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	if (!C)
	{
		return;
	}
	// Every machine: a leech on a body or a toad in a pouch doesn't push the body it rides.
	const bool bRiding = Has(WRSwampFlag::Latched) || Has(WRSwampFlag::Pouched);
	if (bRiding != bPawnIgnored && C->GetCapsuleComponent())
	{
		bPawnIgnored = bRiding;
		C->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, bRiding ? ECR_Ignore : ECR_Block);
	}
	if (C->HasAuthority())
	{
		FWRSwampAI::TickBody(C, this, DeltaTime);
	}
	if (GetNetMode() != NM_DedicatedServer)
	{
		TickLook(DeltaTime);
	}
}

// ---------------------------------------------------------------- for the other swamp systems

int32 UWRSwampBody::DetachLatches(AActor* Victim, bool bAll)
{
	if (!Victim || !Victim->HasAuthority())
	{
		return 0;
	}
	TArray<UWRSwampBody*> On;
	for (const TWeakObjectPtr<UWRSwampBody>& B : Live)
	{
		if (B.IsValid() && B->LatchedOn.Get() == Victim) On.Add(B.Get());
	}
	On.Sort([](const UWRSwampBody& A, const UWRSwampBody& B) { return A.LatchedAt > B.LatchedAt; });
	int32 Off = 0;
	for (UWRSwampBody* B : On)
	{
		if (AWRCreature* Leech = Cast<AWRCreature>(B->GetOwner()))
		{
			FWRSwampAI::Unlatch(Leech, B, true);
			++Off;
		}
		if (!bAll) break;
	}
	return Off;
}

int32 UWRSwampBody::LatchCount(const AActor* Victim)
{
	int32 N = 0;
	for (const TWeakObjectPtr<UWRSwampBody>& B : Live)
	{
		N += B.IsValid() && Victim && (B->LatchedOn.Get() == Victim || (B->Holding == Victim && B->Has(WRSwampFlag::Latched)));
	}
	return N;
}

bool UWRSwampBody::MountPasses(const AActor* Rider, FName Hazard)
{
	const AWRCreature* Mount = Rider ? Cast<AWRCreature>(Rider->GetAttachParentActor()) : nullptr;
	if (!Mount || Mount->GetRider() != Rider)
	{
		return false;
	}
	const TSharedPtr<FJsonObject> R = SwcRow(Mount->CreatureId);
	const TArray<TSharedPtr<FJsonValue>>* Passes = nullptr;
	if (R.IsValid() && R->TryGetArrayField(TEXT("riderPasses"), Passes))
	{
		for (const TSharedPtr<FJsonValue>& V : *Passes) if (FName(*V->AsString()) == Hazard) return true;
	}
	return false;
}

float UWRSwampBody::GatherMult(const AActor* Gatherer, FName Item)
{
	const APawn* P = Cast<APawn>(Gatherer);
	const APlayerState* PS = P ? P->GetPlayerState() : nullptr;
	if (!PS || Item.IsNone())
	{
		return 1.f;
	}
	float Best = 1.f;
	for (const TWeakObjectPtr<UWRSwampBody>& B : Live)
	{
		const AWRCreature* C = B.IsValid() ? Cast<AWRCreature>(B->GetOwner()) : nullptr;
		if (!C || !C->IsAlive() || C->TamedBy != PS->GetPlayerName() || C->IsBaby()) continue;
		const TSharedPtr<FJsonObject> R = SwcRow(C->CreatureId);
		const TSharedPtr<FJsonObject>* Boost = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Pair = nullptr;
		if (!R.IsValid() || !R->TryGetObjectField(TEXT("gatherBoost"), Boost) || !(*Boost)->TryGetArrayField(Item.ToString(), Pair) || Pair->Num() < 2) continue;
		if (FVector::Dist(C->GetActorLocation(), Gatherer->GetActorLocation()) <= float((*Pair)[1]->AsNumber()))
		{
			Best = FMath::Max(Best, float((*Pair)[0]->AsNumber()));
		}
	}
	return Best;
}

bool UWRSwampBody::IgnoresHazard(const AActor* Who, FName Hazard)
{
	const AWRCreature* C = Cast<AWRCreature>(Who);
	const FWRCreatureDef* Def = C ? C->GetDef() : nullptr;
	return Def && Def->Resists.Contains(Hazard);
}

// ---------------------------------------------------------------- the look

namespace WRSwampCreatureLook
{
	using namespace WRLookKit;

	enum EGroup : int32 { GrpStatic = 0, GrpEyes = 1, GrpHex = 2, GrpJaw = 3, GrpCrest = 4, GrpSac = 5, GrpTongue = 6, GrpShell = 7 };

	/** A curved tusk: rods along an arc in the XZ plane (Side picks the cheek), base to tip, tapering. */
	void Tusk(FKit& K, const FSurf& S, const FVector& Centre, float Side, float R, float A0, float A1, float D0, float D1, int32 Segs)
	{
		FVector Prev = Centre + FVector(FMath::Cos(FMath::DegreesToRadians(A0)) * R, Side * 0.f, FMath::Sin(FMath::DegreesToRadians(A0)) * R);
		for (int32 i = 1; i <= Segs; ++i)
		{
			const float T = float(i) / Segs;
			const float A = FMath::DegreesToRadians(FMath::Lerp(A0, A1, T));
			// It curls outward a little as it goes (the tusk clears the face).
			const FVector P = Centre + FVector(FMath::Cos(A) * R, Side * R * 0.18f * T, FMath::Sin(A) * R);
			K.Rod(S, Prev, P, FMath::Lerp(D0, D1, T));
			Prev = P;
		}
	}
}

USceneComponent* UWRSwampBody::AddGroup(FName Bone, const FVector& WorldAt, const FRotator& WorldRot, const TArray<FWRLookBlock>& Blocks, int32 Kind)
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

void UWRSwampBody::SetGroupGlow(int32 Kind, float Glow)
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

void UWRSwampBody::SetGroupVisible(int32 Kind, bool bVisible)
{
	for (FGroup& G : Groups)
	{
		if (G.Kind != Kind || !G.Frame.IsValid() || G.bVisible == bVisible) continue;
		G.bVisible = bVisible;
		G.Frame->SetVisibility(bVisible, true);
	}
}

void UWRSwampBody::ClearLook()
{
	for (USceneComponent* Comp : LookComps) if (Comp) Comp->DestroyComponent();
	LookComps.Reset();
	Groups.Reset();
	if (LoopFx) { WRFx::Stop(LoopFx); LoopFx = nullptr; }
	if (SacLight) { SacLight->DestroyComponent(); SacLight = nullptr; }
	LookFor = NAME_None;
}

void UWRSwampBody::ApplyFlatten()
{
	// The Murkjaw's low wedge (look.flatten cm): the fitted body squashed to that height, its strides unchanged.
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	USkeletalMeshComponent* Body = Skel();
	const float Want = C ? SwcNum(SwcLook(C->CreatureId), TEXT("flatten"), 0.f) : 0.f;
	if (!Body || !Body->GetSkeletalMeshAsset() || Want <= 0.f)
	{
		return;
	}
	const FBox Box = Body->GetSkeletalMeshAsset()->GetBounds().GetBox();
	FVector S = Body->GetRelativeScale3D();
	const float Base = FMath::Min(S.X, S.Y);   // the fit's own scale (meshWiden stretches one side only)
	const float Height = FMath::Max(1.f, (Box.Max.Z - Box.Min.Z) * Base);
	const float K = FMath::Clamp(Want / Height, 0.1f, 1.f);
	const float NewZ = Base * K;
	if (FMath::IsNearlyEqual(S.Z, NewZ, 0.0005f))
	{
		FlattenZ = NewZ;
		return;
	}
	// The fit stood the mesh's bottom on the capsule's: it stays there.
	FVector L = Body->GetRelativeLocation();
	L.Z -= Box.Min.Z * (NewZ - S.Z);
	S.Z = NewZ;
	Body->SetRelativeScale3D(S);
	Body->SetRelativeLocation(L);
	Flatten = K;
	FlattenZ = NewZ;
}

void UWRSwampBody::BuildLook()
{
	using namespace WRLookKit;
	namespace L = WRSwampCreatureLook;
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	const FWRCreatureDef* Def = C ? C->GetDef() : nullptr;
	USkeletalMeshComponent* Body = Skel();
	if (!Def || !Body || !Body->GetSkeletalMeshAsset() || !Body->IsVisible())
	{
		return;
	}
	ClearLook();
	ApplyFlatten();
	LookFor = C->CreatureId;
	const TSharedPtr<FJsonObject> Look = SwcLook(C->CreatureId);

	// Its palette (the cook's colours) over the rig's own texture: M_CreatureTint, two tones from the texture's light.
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
				if (UTexture* T = P.IsEmpty() ? nullptr : LoadObject<UTexture>(nullptr, *P)) M->SetTextureParameterValue(Param, T);
			};
			Tex(TEXT("tex"), TEXT("BaseColorTex"), TEXT(""));
			Tex(TEXT("normal"), TEXT("NormalTex"), TEXT("/Engine/EngineMaterials/DefaultNormal.DefaultNormal"));
			Tex(TEXT("glowTex"), TEXT("GlowTex"), TEXT("/Engine/EngineResources/Black.Black"));
			M->SetScalarParameterValue(TEXT("Desaturation"), SwcNum(Pick, TEXT("desat"), 1.f));
			M->SetScalarParameterValue(TEXT("Brightness"), SwcNum(Pick, TEXT("bright"), 1.f));
			M->SetScalarParameterValue(TEXT("TwoTone"), SwcNum(Pick, TEXT("twoTone"), 0.f));
			M->SetScalarParameterValue(TEXT("LightFrom"), SwcNum(Pick, TEXT("lightFrom"), 0.f));
			M->SetScalarParameterValue(TEXT("Roughness"), SwcNum(Pick, TEXT("rough"), 0.75f));
			M->SetScalarParameterValue(TEXT("GlowStrength"), SwcNum(Pick, TEXT("glowStrength"), 0.f));
			M->SetVectorParameterValue(TEXT("Tint"), SwcColor(Pick, TEXT("tint"), FLinearColor::White));
			M->SetVectorParameterValue(TEXT("TintLight"), SwcColor(Pick, TEXT("light"), SwcColor(Pick, TEXT("tint"), FLinearColor::White)));
			M->SetVectorParameterValue(TEXT("GlowColor"), SwcColor(Pick, TEXT("glow"), SwcHex(TEXT("#7A0A14"))));
			Body->SetMaterial(Slot, M);
			TintMats.Add(M);
		}
	}

	// The silhouette parts, modelled at its size from its own bones.
	const float Rad = Def->Radius;
	const FRotator Yaw(0.f, C->GetActorRotation().Yaw, 0.f);
	const FVector Fwd = Yaw.Vector(), Right = FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), Up = FVector::UpVector;
	const FTransform W = FTransform::Identity;
	const FName K = SwcKind(C->CreatureId);
	// The body's real height and top after the fit (and the flatten): the parts sit on its skin, not on the capsule.
	const FBox AssetBox = Body->GetSkeletalMeshAsset()->GetBounds().GetBox();
	const FVector BS = Body->GetRelativeScale3D();
	const float BodyH = FMath::Max(10.f, (AssetBox.Max.Z - AssetBox.Min.Z) * BS.Z);
	const float H = BodyH * 0.5f;   // half the drawn body's height: the unit every part is measured in
	const float FloorZ = C->GetActorLocation().Z - C->GetSimpleCollisionHalfHeight();
	const float TopZ = FloorZ + BodyH;
	const auto BoneAt = [&](const TCHAR* Hint, FVector& Out) -> FName
	{
		const FName B = C->FindBone(FName(Hint));
		if (!B.IsNone()) Out = Body->GetBoneLocation(B);
		return B;
	};
	// Bones whose name holds Hint (not end / nub bones), with where they are.
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
	// The front of the body at a height: the snout's tip (the mesh's bounds, along its facing).
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
	static const TCHAR* Spine[] = { TEXT("neck"), TEXT("spine3"), TEXT("spine2"), TEXT("spine1"), TEXT("pelvis") };
	FVector Head = C->GetActorLocation() + Fwd * Rad;
	const FName HeadBone = BoneAt(TEXT("head"), Head);
	const FVector Tip = FrontAt(Head.Z);
	const float Snout = FMath::Max(Rad * 0.5f, float(FVector::Dist2D(Head, Tip)));
	const FName FrameBone = HeadBone.IsNone() ? NAME_None : HeadBone;
	// Eyes: a garnet pair for the hexed (shown only while hexed) at the given place in the head frame.
	const auto HexEyes = [&](const FVector& Local, float Spread, float Size)
	{
		AddGroup(FrameBone, Head, Yaw, Blocks([&](FKit& Kit)
		{
			for (const float Side : { -1.f, 1.f }) Kit.Disc(FSurf::Lit(SwcHex(TEXT("#7A0A14")), 6.f), Local + FVector(0.f, Side * Spread, 0.f), FVector(0.4f, Side, 0.3f), Size, 1.5f);
		}), L::GrpHex);
	};

	if (K == TEXT("murkjaw"))
	{
		// The false-gharial bar of a snout: interlocking teeth splayed outside the closed jaw, the lip seam, nostril
		// bosses at the tip, raised eye turrets with the red tapetum behind them.
		const bool bYoung = SwcNum(Look, TEXT("young"), 0.f) > 0.f;
		const FSurf Tooth = FSurf::Glaze(SwcHex(TEXT("#CFC6AE")), 0.35f);
		const FSurf Seam = FSurf::Stone(SwcHex(TEXT("#15160F")), 0.7f);
		const FSurf Hide = FSurf::Stone(SwcHex(TEXT("#2A2E22")), 0.6f);
		const float HalfW0 = FMath::Max(H * 0.42f, Rad * 0.32f), HalfW1 = HalfW0 * 0.45f;
		const float JawZ = -H * 0.18f;
		AddGroup(FrameBone, Head, Yaw, Blocks([&](FKit& Kit)
		{
			const int32 N = bYoung ? 11 : 15;
			for (int32 i = 0; i < N; ++i)
			{
				const float T = 0.18f + 0.8f * i / (N - 1);
				const float X = Snout * T;
				const float Wd = FMath::Lerp(HalfW0, HalfW1, T);
				const float Len = H * (0.16f + 0.08f * Kit.Rand(FVector(X, 0.f, 0.f), 3)) * (T > 0.8f ? 1.25f : 1.f);
				for (const float Side : { -1.f, 1.f })
				{
					// Upper teeth splay down and out over the lower lip; the lower ones up and out between them.
					Kit.Spike(Tooth, FVector(X, Side * Wd * 0.92f, JawZ + H * 0.02f), FVector(0.12f, Side * 0.85f, -0.75f), H * 0.055f, Len);
					Kit.Spike(Tooth.Dim(0.92f), FVector(X + Snout * 0.025f, Side * Wd * 0.86f, JawZ - H * 0.04f), FVector(0.08f, Side * 0.8f, 0.7f), H * 0.05f, Len * 0.9f);
				}
				Kit.Rod(Seam, FVector(X - Snout * 0.03f, -Wd, JawZ), FVector(X + Snout * 0.03f, -Wd * 0.98f, JawZ), H * 0.03f);
				Kit.Rod(Seam, FVector(X - Snout * 0.03f, Wd, JawZ), FVector(X + Snout * 0.03f, Wd * 0.98f, JawZ), H * 0.03f);
			}
			// The bulbous ghara at the snout's tip and its nostrils.
			Kit.Disc(Hide, FVector(Snout * 0.97f, 0.f, H * 0.06f), Up, HalfW1 * 1.5f, H * 0.12f);
			for (const float Side : { -1.f, 1.f }) Kit.Disc(Seam, FVector(Snout * 0.99f, Side * HalfW1 * 0.3f, H * 0.13f), Up, H * 0.05f, H * 0.02f);
			// Eye turrets on top of the skull.
			for (const float Side : { -1.f, 1.f }) Kit.Disc(Hide, FVector(-Snout * 0.04f, Side * HalfW0 * 0.55f, H * 0.2f), FVector(0.f, Side * 0.3f, 1.f), H * 0.2f, H * 0.12f);
		}), L::GrpStatic);
		AddGroup(FrameBone, Head, Yaw, Blocks([&](FKit& Kit)
		{
			for (const float Side : { -1.f, 1.f }) Kit.Disc(FSurf::Lit(SwcHex(TEXT("#7A0A14")), 1.f), FVector(-Snout * 0.01f, Side * HalfW0 * 0.62f, H * 0.24f), FVector(0.5f, Side, 0.2f), H * 0.08f, 1.2f);
		}), L::GrpEyes);
		HexEyes(FVector(-Snout * 0.01f, 0.f, H * 0.25f), HalfW0 * 0.64f, H * 0.11f);
		// The osteoderm ridge down the back and tail: paired keeled scutes, black-olive, every other one grown over with
		// bloodmoss (garnet tufts), the tail's double crest of upright scutes.
		const FSurf Scute = FSurf::Stone(SwcHex(TEXT("#22261B")), 0.55f);
		const FSurf Moss = FSurf::Cloth(SwcHex(TEXT("#6A1E1E")));
		const FSurf MossDark = FSurf::Cloth(SwcHex(TEXT("#4A0A0F")));
		TArray<TPair<FName, FVector>> Back;
		for (const TCHAR* S : Spine) { FVector At; const FName B = BoneAt(S, At); if (!B.IsNone()) Back.Add({ B, At }); }
		for (const TPair<FName, FVector>& T : BonesWith(TEXT("tail"))) Back.Add(T);
		for (int32 b = 0; b < Back.Num(); ++b)
		{
			const bool bTail = Back[b].Key.ToString().ToLower().Contains(TEXT("tail"));
			FVector At = Back[b].Value;
			const float Top = bTail ? At.Z + H * 0.25f : TopZ - H * 0.05f;
			At.Z = Top;
			const float Seg = Rad * (bTail ? 0.9f : 0.55f);
			AddGroup(Back[b].Key, At, Yaw, Blocks([&](FKit& Kit)
			{
				const int32 N = bTail ? 4 : 3;
				for (int32 i = 0; i < N; ++i)
				{
					const float X = -Seg * i / N;
					for (const float Side : { -1.f, 1.f })
					{
						const float Wd = bTail ? H * 0.12f : H * 0.28f;
						const FVector P(X, Side * Wd, 0.f);
						const float Tall = H * (bTail ? 0.3f : 0.16f) * (0.85f + 0.3f * Kit.Rand(P, 7));
						Kit.Add(Scute, FVector(Seg / N * 0.8f, H * 0.12f, Tall), P + FVector(0.f, 0.f, Tall * 0.3f), FRotator(0.f, 0.f, Side * (bTail ? 4.f : 14.f)));
						if (!bTail && (i + b) % 2 == 0)
						{
							for (int32 m = 0; m < 4; ++m)
							{
								const FVector Q = P + FVector(Kit.Jit(P, 20 + m) * Seg * 0.12f, Side * Kit.Rand(P, 30 + m) * H * 0.06f, Tall * 0.6f);
								Kit.Spike(m % 2 ? Moss : MossDark, Q, FVector(Kit.Jit(Q, 1) * 0.4f, Side * 0.3f, 1.f), H * 0.07f, H * (0.06f + 0.05f * Kit.Rand(Q, 2)));
							}
						}
					}
					if (!bTail) Kit.Add(Scute.Dim(0.85f), FVector(Seg / N * 0.7f, H * 0.1f, H * 0.1f), FVector(X, 0.f, H * 0.03f));
				}
			}), L::GrpStatic);
		}
	}
	else if (K == TEXT("gorgeleech") || K == TEXT("latch_leech"))
	{
		// A medicinal leech: ring annuli round every segment, paired orange-red stripes down the back, the ten-eye
		// crescent on the head, the Y of three jaws in the front sucker (they open on the tell), the back sucker.
		const bool bSmall = K == TEXT("latch_leech");
		const FSurf Band = FSurf::Glaze(SwcHex(TEXT("#1E2216")), 0.2f);
		const FSurf Stripe = FSurf::Glaze(SwcHex(TEXT("#B5482A")), 0.25f);
		const FSurf EyeS = FSurf::Glaze(SwcHex(TEXT("#050505")), 0.1f);
		const FSurf Jaw = FSurf::Glaze(SwcHex(TEXT("#C9B49A")), 0.3f);
		const FSurf Rim = FSurf::Glaze(SwcHex(TEXT("#3A2A22")), 0.2f);
		const float D = FMath::Max(6.f, BodyH * 0.95f);   // the body's thickness
		TArray<TPair<FName, FVector>> Segs = BonesWith(TEXT("spine"));
		Segs.Sort([&](const TPair<FName, FVector>& A, const TPair<FName, FVector>& B) { return FVector::DistSquared(A.Value, Head) < FVector::DistSquared(B.Value, Head); });
		for (int32 s = 0; s < Segs.Num(); ++s)
		{
			const float Seg = s + 1 < Segs.Num() ? FMath::Max(D * 0.5f, float(FVector::Dist(Segs[s].Value, Segs[s + 1].Value))) : D;
			AddGroup(Segs[s].Key, Segs[s].Value, Yaw, Blocks([&](FKit& Kit)
			{
				const int32 Rings = bSmall ? 2 : 4;
				for (int32 r = 0; r < Rings; ++r) Kit.Disc(Band, FVector(-Seg * (r + 0.5f) / Rings, 0.f, 0.f), FVector(1.f, 0.f, 0.f), D * 1.04f, FMath::Max(0.8f, D * 0.04f));
				if (!bSmall)
				{
					for (const float Side : { -1.f, 1.f }) Kit.Add(Stripe, FVector(Seg * 0.95f, D * 0.07f, 1.f), FVector(-Seg * 0.5f, Side * D * 0.17f, D * 0.47f), FRotator(0.f, 0.f, Side * 12.f));
				}
			}), L::GrpStatic);
		}
		// The back sucker on the last segment.
		if (Segs.Num() > 0)
		{
			AddGroup(Segs.Last().Key, Segs.Last().Value, Yaw, Blocks([&](FKit& Kit)
			{
				Kit.Disc(Rim, FVector(-D * 0.4f, 0.f, -D * 0.35f), FVector(-0.4f, 0.f, -1.f), D * 1.25f, D * 0.12f);
			}), L::GrpStatic);
		}
		const FVector Mouth = FrontAt(Head.Z);
		if (!bSmall)
		{
			AddGroup(FrameBone, Head, Yaw, Blocks([&](FKit& Kit)
			{
				for (int32 e = 0; e < 10; ++e)
				{
					const float A = FMath::DegreesToRadians(-70.f + 140.f * e / 9.f);
					Kit.Disc(EyeS, FVector(Snout * (0.55f + 0.18f * FMath::Cos(A)), FMath::Sin(A) * D * 0.32f, D * 0.47f), FVector(0.f, 0.f, 1.f), D * 0.07f, 0.8f);
				}
			}), L::GrpStatic);
		}
		AddGroup(FrameBone, Mouth, Yaw, Blocks([&](FKit& Kit)
		{
			Kit.Disc(Rim, FVector(-D * 0.05f, 0.f, 0.f), FVector(1.f, 0.f, 0.f), D * 1.15f, D * 0.1f);
			for (int32 j = 0; j < 3; ++j)
			{
				// The Y: three blades at 120 degrees, each a toothed half-disc edge.
				const float A = FMath::DegreesToRadians(90.f + 120.f * j);
				const FVector Out(0.f, FMath::Cos(A), FMath::Sin(A));
				Kit.Add(Jaw, FVector(D * 0.12f, D * 0.05f, D * 0.36f), Out * D * 0.2f + FVector(D * 0.02f, 0.f, 0.f), FRotator(0.f, 0.f, FMath::RadiansToDegrees(A) - 90.f));
			}
		}), L::GrpJaw);
	}
	else if (K == TEXT("hexcrow"))
	{
		// The rook's bare face: a bone-white plate from the brow over the base of a long black beak; a crest of ragged
		// hackles on the throat and nape (they flare for the cry); pale blue eyes.
		const FSurf BoneS = FSurf::Glaze(SwcHex(TEXT("#E2DCCB")), 0.5f);
		const FSurf Beak = FSurf::Glaze(SwcHex(TEXT("#121014")), 0.3f);
		const FSurf Feather = FSurf::Cloth(SwcHex(TEXT("#0E0D10")));
		AddGroup(FrameBone, Head, Yaw, Blocks([&](FKit& Kit)
		{
			Kit.Spike(BoneS, FVector(-H * 0.05f, 0.f, H * 0.04f), FVector(1.f, 0.f, -0.12f), H * 0.3f, Snout * 0.75f);
			Kit.Spike(Beak, FVector(Snout * 0.45f, 0.f, -H * 0.01f), FVector(1.f, 0.f, -0.18f), H * 0.13f, Snout * 0.75f);
			for (const float Side : { -1.f, 1.f }) Kit.Disc(FSurf::Lit(SwcHex(TEXT("#A8C8E0")), 0.4f), FVector(Snout * 0.05f, Side * H * 0.11f, H * 0.07f), FVector(0.2f, Side, 0.f), H * 0.06f, 1.f);
		}), L::GrpStatic);
		FVector Neck = Head;
		const FName NeckBone = BoneAt(TEXT("neck"), Neck);
		AddGroup(NeckBone.IsNone() ? FrameBone : NeckBone, Neck, Yaw, Blocks([&](FKit& Kit)
		{
			for (int32 i = 0; i < 16; ++i)
			{
				const float A = PI * (-0.15f + 1.3f * i / 15.f);
				const FVector Out(0.f, FMath::Cos(A), FMath::Sin(A));
				const float Len = H * (0.22f + 0.14f * Kit.Rand(Out, 9 + i));
				Kit.Spike(i % 3 ? Feather : Feather.Dim(1.6f), Out * H * 0.12f, (Out + FVector(-0.9f, 0.f, -0.2f)).GetSafeNormal(), H * 0.06f, Len);
			}
		}), L::GrpCrest);
	}
	else if (K == TEXT("reedfowl"))
	{
		// The swamphen's brow shield running into a stout bill, and long toes for walking the lily pads.
		const FSurf Shield = FSurf::Glaze(SwcHex(TEXT("#7A0A14")) * 1.5f, 0.25f);
		AddGroup(FrameBone, Head, Yaw, Blocks([&](FKit& Kit)
		{
			Kit.Spike(Shield, FVector(-H * 0.02f, 0.f, H * 0.1f), FVector(1.f, 0.f, -0.35f), H * 0.2f, Snout * 0.95f);
			Kit.Add(Shield, FVector(H * 0.24f, H * 0.14f, H * 0.05f), FVector(H * 0.02f, 0.f, H * 0.14f), FRotator(-15.f, 0.f, 0.f));
		}), L::GrpStatic);
		const FSurf Toe = FSurf::Glaze(SwcHex(TEXT("#B8A060")) * 0.8f, 0.5f);
		for (const TPair<FName, FVector>& F : BonesWith(TEXT("foot")))
		{
			FVector At = F.Value;
			At.Z = FloorZ + 2.f;
			AddGroup(F.Key, At, Yaw, Blocks([&](FKit& Kit)
			{
				for (const float A : { -32.f, 0.f, 32.f, 180.f })
				{
					const FVector D = FRotator(0.f, A, 0.f).Vector();
					Kit.Rod(Toe, FVector::ZeroVector, D * H * (A == 180.f ? 0.45f : 0.75f), FMath::Max(1.f, H * 0.04f));
				}
			}), L::GrpStatic);
		}
	}
	else if (K == TEXT("bogstalker"))
	{
		// Mostly mouth: a lip ridge round a mouth as wide as the body, two horned brows, the pale throat sac that pumps
		// twice before the tongue, black blotches on the back; the tongue itself (out on the lash).
		const FSurf Lip = FSurf::Stone(SwcHex(TEXT("#3E3424")), 0.6f);
		const FSurf HornS = FSurf::Glaze(SwcHex(TEXT("#5A4632")), 0.45f);
		const FSurf Blot = FSurf::Stone(SwcHex(TEXT("#14150E")), 0.7f);
		const float HalfW = FMath::Max(Rad * 0.55f, H * 0.6f);
		AddGroup(FrameBone, Head, Yaw, Blocks([&](FKit& Kit)
		{
			// The mouth line: a U from one hinge round the snout to the other.
			FVector Prev(-Snout * 0.2f, -HalfW, -H * 0.12f);
			for (int32 i = 1; i <= 12; ++i)
			{
				const float A = PI * (-0.5f + float(i) / 12.f);
				const FVector P(-Snout * 0.2f + FMath::Cos(A) * Snout * 1.15f, FMath::Sin(A) * HalfW, -H * 0.12f);
				Kit.Rod(Lip, Prev, P, H * 0.07f);
				Prev = P;
			}
			for (const float Side : { -1.f, 1.f })
			{
				Kit.Disc(Lip, FVector(Snout * 0.1f, Side * HalfW * 0.48f, H * 0.3f), FVector(0.f, Side * 0.4f, 1.f), H * 0.3f, H * 0.14f);   // eye bosses
				Kit.Spike(HornS, FVector(Snout * 0.05f, Side * HalfW * 0.5f, H * 0.36f), FVector(-0.2f, Side * 0.25f, 1.f), H * 0.16f, H * 0.32f);   // the horned brows
				Kit.Disc(FSurf::Glaze(SwcHex(TEXT("#C8A040")), 0.2f), FVector(Snout * 0.16f, Side * HalfW * 0.52f, H * 0.33f), FVector(0.4f, Side, 0.4f), H * 0.1f, 1.f);
			}
		}), L::GrpStatic);
		HexEyes(FVector(Snout * 0.17f, 0.f, H * 0.34f), HalfW * 0.53f, H * 0.12f);
		AddGroup(FrameBone, Head + Fwd * Snout * 0.1f - Up * H * 0.38f, Yaw, Blocks([&](FKit& Kit)
		{
			Kit.Disc(FSurf::Glaze(SwcHex(TEXT("#D8D09A")), 0.35f), FVector::ZeroVector, FVector(0.f, 0.f, 1.f), HalfW * 1.3f, H * 0.3f);
			Kit.Disc(FSurf::Glaze(SwcHex(TEXT("#D8D09A")) * 0.9f, 0.35f), FVector(0.f, 0.f, -H * 0.1f), FVector(0.f, 0.f, 1.f), HalfW * 1.0f, H * 0.2f);
		}), L::GrpSac);
		AddGroup(FrameBone, Tip - Up * H * 0.12f, Yaw, Blocks([&](FKit& Kit)
		{
			Kit.Rod(FSurf::Glaze(SwcHex(TEXT("#C9A08C")), 0.15f), FVector::ZeroVector, FVector(100.f, 0.f, 0.f), 9.f);
			Kit.Disc(FSurf::Glaze(SwcHex(TEXT("#E8DCC8")), 0.1f), FVector(100.f, 0.f, 0.f), FVector(1.f, 0.f, 0.f), 22.f, 6.f);
		}), L::GrpTongue);
		for (int32 b = 1; b < 5; ++b)
		{
			FVector At;
			const FName Bone = BoneAt(Spine[b], At);
			if (Bone.IsNone()) continue;
			At.Z = TopZ - H * 0.04f;
			AddGroup(Bone, At, Yaw, Blocks([&](FKit& Kit)
			{
				for (int32 i = 0; i < 3; ++i)
				{
					const FVector P(Kit.Jit(FVector(float(b), float(i), 0.f), 3) * Rad * 0.25f, Kit.Jit(FVector(float(i), float(b), 1.f), 4) * HalfW * 0.6f, 0.f);
					Kit.Disc(Blot, P, FVector(0.f, P.Y / FMath::Max(1.f, HalfW), 1.f), H * (0.18f + 0.12f * Kit.Rand(P, 5)), 1.2f);
				}
			}), L::GrpStatic);
		}
	}
	else if (K == TEXT("mirestrider"))
	{
		// The shoebill's bill: a huge horn-yellow clog with a keel along the top and a hooked nail; a small tuft at the
		// nape, plumes down the S-neck; long black splayed toes (a jacana's).
		const FSurf Bill = FSurf::Glaze(SwcHex(TEXT("#B8A060")), 0.45f);
		const FSurf Plume = FSurf::Cloth(SwcHex(TEXT("#6A7480")));
		const FSurf Toe = FSurf::Glaze(SwcHex(TEXT("#121212")), 0.5f);
		AddGroup(FrameBone, Head, Yaw, Blocks([&](FKit& Kit)
		{
			const float BL = FMath::Max(Snout * 1.1f, H * 0.3f), BW = BL * 0.42f, BH = BL * 0.36f;
			Kit.Add(Bill, FVector(BL, BW, BH * 0.55f), FVector(BL * 0.5f, 0.f, BH * 0.1f));
			Kit.Add(Bill.Dim(0.9f), FVector(BL * 0.95f, BW * 0.92f, BH * 0.4f), FVector(BL * 0.5f, 0.f, -BH * 0.32f));
			Kit.Rod(Bill.Dim(0.8f), FVector(BL * 0.05f, 0.f, BH * 0.42f), FVector(BL * 0.98f, 0.f, BH * 0.3f), BW * 0.18f);   // the keel
			Kit.Spike(Bill.Dim(0.6f), FVector(BL * 0.98f, 0.f, BH * 0.25f), FVector(0.3f, 0.f, -1.f), BW * 0.22f, BH * 0.55f);  // the nail
			for (int32 s = 0; s < 5; ++s) Kit.Disc(FSurf::Stone(SwcHex(TEXT("#5A4E3A")), 0.6f), FVector(BL * (0.2f + 0.15f * s), (s % 2 ? 1.f : -1.f) * BW * 0.3f, BH * 0.38f), Up, BW * 0.12f, 0.8f);
			for (const float Side : { -1.f, 1.f }) Kit.Disc(FSurf::Lit(SwcHex(TEXT("#D8D2A8")), 0.3f), FVector(-BL * 0.02f, Side * BW * 0.45f, BH * 0.35f), FVector(0.f, Side, 0.f), BW * 0.16f, 1.f);
			for (int32 t = 0; t < 5; ++t) Kit.Spike(Plume, FVector(-BL * 0.15f, (t - 2) * BW * 0.08f, BH * 0.4f), FVector(-1.f, 0.f, 0.45f), BW * 0.12f, BL * 0.35f);
		}), L::GrpStatic);
		FVector Neck = Head;
		const FName NeckBone = BoneAt(TEXT("neck"), Neck);
		if (!NeckBone.IsNone())
		{
			AddGroup(NeckBone, Neck, Yaw, Blocks([&](FKit& Kit)
			{
				for (int32 i = 0; i < 7; ++i) Kit.Spike(Plume.Dim(0.85f + 0.05f * (i % 3)), FVector(-H * 0.04f * i, 0.f, H * 0.06f - H * 0.05f * i), FVector(-1.f, 0.f, -0.4f), H * 0.08f, H * 0.3f);
			}), L::GrpStatic);
		}
		for (const TPair<FName, FVector>& F : BonesWith(TEXT("foot")))
		{
			FVector At = F.Value;
			At.Z = FloorZ + 3.f;
			AddGroup(F.Key, At, Yaw, Blocks([&](FKit& Kit)
			{
				for (const float A : { -38.f, -10.f, 18.f, 180.f })
				{
					const FVector D = FRotator(0.f, A, 0.f).Vector();
					Kit.Rod(Toe, FVector::ZeroVector, D * H * (A == 180.f ? 0.2f : 0.34f), FMath::Max(2.f, H * 0.025f));
				}
			}), L::GrpStatic);
		}
	}
	else if (K == TEXT("mudhulk"))
	{
		// A hippo's box of a head: the broad muzzle with nostril bosses on top, eyes and small ears set high, short
		// lower tusks bared at the corners; the red sweat is in its tint (look.tint light).
		const FSurf Skin = FSurf::Glaze(SwcHex(TEXT("#4A4038")), 0.3f);
		const FSurf Ivory = FSurf::Glaze(SwcHex(TEXT("#D8CCB0")), 0.35f);
		AddGroup(FrameBone, Head, Yaw, Blocks([&](FKit& Kit)
		{
			const float MW = FMath::Max(Rad * 0.55f, H * 0.5f);
			Kit.Add(Skin, FVector(Snout * 0.5f, MW * 1.35f, H * 0.42f), FVector(Snout * 0.78f, 0.f, -H * 0.05f));
			for (const float Side : { -1.f, 1.f })
			{
				Kit.Disc(Skin.Dim(0.85f), FVector(Snout * 0.95f, Side * MW * 0.32f, H * 0.18f), Up, H * 0.16f, H * 0.08f);
				Kit.Disc(Skin.Dim(0.85f), FVector(Snout * 0.15f, Side * MW * 0.45f, H * 0.26f), FVector(0.f, Side * 0.3f, 1.f), H * 0.18f, H * 0.1f);
				Kit.Spike(Skin.Dim(0.8f), FVector(-Snout * 0.05f, Side * MW * 0.5f, H * 0.3f), FVector(-0.3f, Side * 0.5f, 1.f), H * 0.08f, H * 0.12f);
				Kit.Spike(Ivory, FVector(Snout * 0.92f, Side * MW * 0.55f, -H * 0.26f), FVector(0.35f, Side * 0.2f, 1.f), H * 0.09f, H * 0.3f);
			}
		}), L::GrpStatic);
	}
	else if (K == TEXT("bogsnout"))
	{
		// The babirusa's tusks: the uppers grow up through the snout and curl back over the face toward the eyes, the
		// lowers sweep up outside the lips; sparse black bristle along the back.
		const FSurf Ivory = FSurf::Glaze(SwcHex(TEXT("#DCCFB4")), 0.35f);
		const FSurf Bristle = FSurf::Cloth(SwcHex(TEXT("#1A1614")));
		AddGroup(FrameBone, Head, Yaw, Blocks([&](FKit& Kit)
		{
			for (const float Side : { -1.f, 1.f })
			{
				L::Tusk(Kit, Ivory, FVector(Snout * 0.45f, Side * H * 0.12f, H * 0.2f), Side, Snout * 0.38f, -10.f, 175.f, H * 0.07f, H * 0.03f, 10);
				L::Tusk(Kit, Ivory.Dim(0.95f), FVector(Snout * 0.8f, Side * H * 0.2f, -H * 0.05f), Side, Snout * 0.22f, -60.f, 110.f, H * 0.06f, H * 0.025f, 7);
			}
		}), L::GrpStatic);
		for (int32 b = 0; b < 5; ++b)
		{
			FVector At;
			const FName Bone = BoneAt(Spine[b], At);
			if (Bone.IsNone()) continue;
			At.Z = TopZ - H * 0.06f;
			AddGroup(Bone, At, Yaw, Blocks([&](FKit& Kit)
			{
				for (int32 i = 0; i < 4; ++i) Kit.Spike(Bristle, FVector(-Rad * 0.1f * i, Kit.Jit(FVector(float(i), float(b), 0.f), 2) * H * 0.1f, 0.f), FVector(-0.5f, 0.f, 1.f), 1.5f, H * (0.12f + 0.06f * Kit.Rand(FVector(float(b), float(i), 0.f), 3)));
			}), L::GrpStatic);
		}
	}
	else if (K == TEXT("lanterntoad"))
	{
		// A cane toad's warts and parotoid glands, gold eyes, and the throat sac that glows gold-green (its light is real).
		const FSurf Wart = FSurf::Stone(SwcHex(TEXT("#3A2E20")), 0.8f);
		AddGroup(FrameBone, Head, Yaw, Blocks([&](FKit& Kit)
		{
			for (const float Side : { -1.f, 1.f })
			{
				Kit.Disc(Wart, FVector(-Snout * 0.2f, Side * H * 0.4f, H * 0.3f), FVector(0.f, Side * 0.4f, 1.f), H * 0.35f, H * 0.14f);
				Kit.Disc(FSurf::Lit(SwcHex(TEXT("#C8A040")), 0.5f), FVector(Snout * 0.2f, Side * H * 0.32f, H * 0.32f), FVector(0.3f, Side, 0.4f), H * 0.16f, 1.f);
			}
			for (int32 i = 0; i < 10; ++i)
			{
				const FVector P(-Snout * (0.4f + 0.8f * Kit.Rand(FVector(float(i)), 1)), Kit.Jit(FVector(float(i)), 2) * H * 0.5f, H * 0.35f);
				Kit.Disc(Wart, P, Up, H * (0.08f + 0.06f * Kit.Rand(P, 3)), H * 0.05f);
			}
		}), L::GrpStatic);
		USceneComponent* Sac = AddGroup(FrameBone, Head + Fwd * Snout * 0.25f - Up * H * 0.35f, Yaw, Blocks([&](FKit& Kit)
		{
			Kit.Disc(FSurf::Lit(SwcHex(TEXT("#C8E070")), 1.f), FVector::ZeroVector, Up, H * 0.75f, H * 0.42f);
		}), L::GrpSac);
		if (Sac)
		{
			SacLight = NewObject<UPointLightComponent>(C);
			SacLight->RegisterComponent();
			SacLight->AttachToComponent(Sac, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			SacLight->SetLightColor(SwcHex(TEXT("#C8E070")));
			SacLight->SetCastShadows(false);
			SacLight->SetIntensity(0.f);
			SacLight->SetAttenuationRadius(SwcNum(SwcRow(C->CreatureId), TEXT("lightRadius"), 1000.f));
		}
	}
	else if (K == TEXT("slimeshell"))
	{
		// An apple snail's shell: a right-handed coil of whorls on the back, banded brown and cream, its aperture to the
		// front-right; the foot (the rig) pulls in under it when it withdraws.
		FVector Mid = C->GetActorLocation();
		const FName MidBone = BoneAt(TEXT("spine2"), Mid);
		const FSurf Brown = FSurf::Glaze(SwcHex(TEXT("#6A4A2A")), 0.35f);
		const FSurf Cream = FSurf::Glaze(SwcHex(TEXT("#E6D8B8")), 0.35f);
		const float R0 = FMath::Max(Rad * 0.75f, H * 0.7f);
		FVector At = Mid;
		At.Z = TopZ - H * 0.1f + R0;
		AddGroup(MidBone.IsNone() ? FrameBone : MidBone, At, Yaw, Blocks([&](FKit& Kit)
		{
			// The coil turns about the lateral axis; whorls shrink inward to the apex (log spiral).
			const int32 N = 34;
			FVector Prev = FVector::ZeroVector;
			for (int32 i = 0; i <= N; ++i)
			{
				const float T = 2.6f * float(i) / N;   // turns
				const float A = 2.f * PI * T;
				const float R = R0 * FMath::Exp(-0.55f * T);
				const FVector P(FMath::Cos(A) * R, -R0 * 0.25f * (T / 2.6f), FMath::Sin(A) * R);
				if (i > 0)
				{
					const float Diam = R * 1.25f + 4.f;
					Kit.Rod(((i / 3) % 2) ? Cream : Brown, Prev, P, Diam);
				}
				Prev = P;
			}
			Kit.Disc(Brown.Dim(0.6f), FVector(R0 * 0.85f, R0 * 0.18f, -R0 * 0.3f), FVector(0.6f, 0.4f, -0.5f), R0 * 0.85f, 3.f);   // the aperture's lip
		}), L::GrpShell);
	}
	UE_LOG(LogWildreach, Verbose, TEXT("SWAMP look %s: %d parts, %d tints"), *C->CreatureId.ToString(), Groups.Num(), TintMats.Num());
}

void UWRSwampBody::TickLook(float DeltaTime)
{
	namespace L = WRSwampCreatureLook;
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	USkeletalMeshComponent* Body = Skel();
	if (!C || !Body)
	{
		return;
	}
	if (LookFor != C->CreatureId)
	{
		// Built once its mesh is on (the server may send the id after this component).
		if (Body->GetSkeletalMeshAsset() && Body->IsVisible() && ++LookTries > 2) BuildLook();
		return;
	}
	// The fit ran again (its id or tame look changed): the low wedge comes back.
	if (FlattenZ > 0.f && !FMath::IsNearlyEqual(Body->GetRelativeScale3D().Z, FlattenZ, 0.0005f)) ApplyFlatten();
	const FName K = SwcKind(C->CreatureId);
	const bool bAlive = C->IsAlive();
	FxClock += DeltaTime;
	const float Alpha = FMath::Clamp(DeltaTime * 8.f, 0.f, 1.f);

	// Lines running toward a point (the duckweed parting, a V-wake, the bulge of a surge).
	for (int32 i = Lines.Num() - 1; i >= 0; --i)
	{
		FLineFx& Ln = Lines[i];
		Ln.Age += DeltaTime;
		const float Len = FVector::Dist2D(Ln.From, Ln.To);
		const int32 Total = FMath::Max(1, FMath::CeilToInt(Len / 60.f));
		const int32 Want = FMath::Min(Total, FMath::CeilToInt(Total * Ln.Age / Ln.Seconds));
		const FRotator Dir = (Ln.To - Ln.From).Rotation();
		for (; Ln.Done < Want; ++Ln.Done)
		{
			WRFx::SpawnAt(C, Ln.Effect, FMath::Lerp(FVector(Ln.From), FVector(Ln.To), float(Ln.Done) / Total), Dir, 1.f);
		}
		if (Ln.Done >= Total) Lines.RemoveAt(i);
	}

	// Eyes: the Murkjaw's red tapetum shines at night (and blazes on the gape); garnet hex eyes on a hexed beast.
	if (K == TEXT("murkjaw"))
	{
		const float Night = AWRGameState::IsNightIn(C) ? SwcT(TEXT("murkjaw.eyeshineNight"), 1.f) : 0.f;
		const float Want = !bAlive ? 0.f : Has(WRSwampFlag::Gape) || Has(WRSwampFlag::Rolling) ? 7.f : Has(WRSwampFlag::Ambush) ? 2.f + 4.f * Night : 0.3f + 4.f * Night;
		EyeGlow = EyeGlow < 0.f ? Want : FMath::FInterpTo(EyeGlow, Want, DeltaTime, 6.f);
		SetGroupGlow(L::GrpEyes, EyeGlow);
	}
	SetGroupVisible(L::GrpHex, bAlive && Has(WRSwampFlag::Hexed));
	if (Has(WRSwampFlag::Hexed) && bAlive) SetGroupGlow(L::GrpHex, 5.f + 2.f * FMath::Sin(Clock * 4.f));

	// The Slimeshell in its shell: the foot gone under it.
	const bool bHide = K == TEXT("slimeshell") && Has(WRSwampFlag::Withdrawn) && bAlive;
	if (bHide != bSkelHidden)
	{
		bSkelHidden = bHide;
		Body->SetVisibility(!bHide, false);
		if (bHide) WRSound::PlayOn(C, TEXT("mire.shell_clack"));
	}

	// The death roll: the whole body turns over and over about its long axis (belly up: the soft target).
	if (Has(WRSwampFlag::Rolling) && bAlive)
	{
		if (!bRollSaved)
		{
			bRollSaved = true;
			RollBaseRot = Body->GetRelativeRotation();
			RollBaseLoc = Body->GetRelativeLocation();
			RollAngle = 0.f;
		}
		RollAngle = FMath::Fmod(RollAngle + DeltaTime * SwcT(TEXT("murkjaw.rollDegrees"), 520.f), 360.f);
		const FVector Pivot = RollBaseLoc + FVector(0.f, 0.f, (Flatten > 0.f ? SwcNum(SwcLook(C->CreatureId), TEXT("flatten"), 100.f) : C->GetSimpleCollisionHalfHeight() * 2.f) * 0.5f);
		const FQuat Q = FQuat(C->GetActorRotation().UnrotateVector(C->GetActorForwardVector()), FMath::DegreesToRadians(RollAngle));
		Body->SetRelativeLocationAndRotation(Pivot + Q.RotateVector(RollBaseLoc - Pivot), Q * RollBaseRot.Quaternion());
		if (FxClock > 0.3f)
		{
			FxClock = 0.f;
			WRFx::SpawnAt(C, WRSwamp::IsWet(SwcFeet(C) + FVector(0.f, 0.f, 40.f)) ? TEXT("mire.splash") : TEXT("mire.mud"), SwcFeet(C), FRotator::ZeroRotator, 0.6f);
		}
	}
	else if (bRollSaved)
	{
		bRollSaved = false;
		Body->SetRelativeLocationAndRotation(RollBaseLoc, RollBaseRot);
	}

	// Moments on a clock: the ambusher's bubbles, the wallow's churn, a leech's feeding.
	if (bAlive && FxClock > 1.4f)
	{
		if (Has(WRSwampFlag::Ambush))
		{
			FxClock = 0.f;
			const FName Head = C->FindBone(TEXT("head"));
			WRFx::SpawnAt(C, TEXT("mire.bubbles"), (Head.IsNone() ? C->GetActorLocation() : Body->GetBoneLocation(Head)) * FVector(1.f, 1.f, 0.f) + FVector(0.f, 0.f, FWRSwampData::Get().SeaZ), FRotator::ZeroRotator, 0.7f);
		}
		else if (Has(WRSwampFlag::Wallow))
		{
			FxClock = 0.f;
			WRFx::SpawnAt(C, TEXT("mire.bubbles"), C->GetActorLocation() * FVector(1.f, 1.f, 0.f) + FVector(0.f, 0.f, FWRSwampData::Get().SeaZ), FRotator::ZeroRotator, 1.4f);
		}
		else if (Has(WRSwampFlag::Latched))
		{
			FxClock = 0.f;
			WRFx::SpawnAt(C, TEXT("blood.hit"), C->GetActorLocation(), C->GetActorRotation(), 0.3f);
		}
	}

	// Moving parts.
	CrestAlpha = FMath::FInterpTo(CrestAlpha, Has(WRSwampFlag::Crying) ? 1.f : 0.f, DeltaTime, 6.f);
	GapeAlpha = FMath::FInterpTo(GapeAlpha, Has(WRSwampFlag::Disc) ? 1.f : 0.f, DeltaTime, 10.f);
	PumpClock += DeltaTime;
	// The tongue: out toward what it caught (or what the frog is after), back in after.
	const AActor* TongueAt = Holding ? Holding.Get() : C->GetAggroRep();
	const float TongueWant = Has(WRSwampFlag::Tongue) && bAlive && TongueAt ? FMath::Min(1000.f, float(FVector::Dist(TongueAt->GetActorLocation(), C->GetActorLocation())) - C->GetSimpleCollisionRadius()) : 0.f;
	TongueLen = FMath::FInterpConstantTo(TongueLen, FMath::Max(0.f, TongueWant), DeltaTime, 6000.f);
	// The sac: a Bogstalker's pumps twice before the tongue; a Lanterntoad's glows (and blazes for its flash).
	const bool bToad = K == TEXT("lanterntoad");
	if (bToad)
	{
		const bool bNight = AWRGameState::IsNightIn(C);
		const float Want = !bAlive ? 0.f : Has(WRSwampFlag::Lit) ? 12.f : C->IsTamed() ? 3.f : bNight ? 1.6f + 0.8f * FMath::Sin(Clock * 1.7f + C->GetUniqueID() % 7) : 0.25f;
		SacGlow = FMath::FInterpTo(SacGlow, Want, DeltaTime, 5.f);
		SetGroupGlow(L::GrpSac, SacGlow);
		if (SacLight)
		{
			const float Full = SwcT(TEXT("lanterntoad.light"), 2600.f);
			SacLight->SetAttenuationRadius(SwcNum(SwcRow(C->CreatureId), TEXT("lightRadius"), 1000.f) * (C->IsTamed() || Has(WRSwampFlag::Lit) ? 1.f : 0.45f));
			SacLight->SetIntensity(Full * SacGlow / 3.f);
		}
		// The toad's own light replaces the generic tame light (one light on it, at the sac).
		TArray<UPointLightComponent*> Lights;
		C->GetComponents(Lights);
		for (UPointLightComponent* Lt : Lights) if (Lt != SacLight && Lt->GetFName() == TEXT("GlowLight") && Lt->IsVisible()) Lt->SetVisibility(false);
	}
	for (FGroup& G : Groups)
	{
		USceneComponent* F = G.Frame.Get();
		if (!F) continue;
		switch (G.Kind)
		{
		case L::GrpJaw:
			// The three jaws spread on the tell and while it feeds; a slow sucking pulse while it's on.
			F->SetRelativeScale3D(G.BaseScale * FVector(1.f, 1.f, 1.f) * (FMath::Lerp(0.55f, 1.35f, GapeAlpha) + (Has(WRSwampFlag::Latched) ? 0.08f * FMath::Sin(Clock * 7.f) : 0.f)));
			break;
		case L::GrpCrest:
			F->SetRelativeScale3D(G.BaseScale * FVector(1.f + 0.3f * CrestAlpha, 1.f + 0.5f * CrestAlpha, 1.f + 0.6f * CrestAlpha));
			break;
		case L::GrpSac:
		{
			float S = 1.f;
			if (bToad) S = 1.f + 0.12f * FMath::Sin(Clock * 2.2f) + (Has(WRSwampFlag::Lit) ? 0.45f : 0.f);
			else if (Has(WRSwampFlag::Pumping)) S = 1.f + 0.45f * FMath::Abs(FMath::Sin(PumpClock * 7.f));
			else S = 1.f + 0.05f * FMath::Sin(Clock * 1.5f);
			F->SetRelativeScale3D(G.BaseScale * FVector(S, S, S));
			break;
		}
		case L::GrpTongue:
			if (TongueLen > 4.f && TongueAt)
			{
				if (!G.bVisible) { G.bVisible = true; F->SetVisibility(true, true); }
				F->SetWorldRotation((TongueAt->GetActorLocation() - F->GetComponentLocation()).Rotation());
				F->SetWorldScale3D(FVector(TongueLen / 100.f, 1.f, 1.f));
			}
			else if (G.bVisible)
			{
				G.bVisible = false;
				F->SetVisibility(false, true);
			}
			break;
		default:
			break;
		}
	}
}

// ---------------------------------------------------------------- the AI hooks

bool FWRSwampAI::TickWild(AWRCreatureAI* AI, AWRCreature* Self, float DeltaSeconds)
{
	UWRSwampBody* Body = UWRSwampBody::Get(Self);
	if (!Body || !AI)
	{
		return false;
	}
	const double T = SwcNow(Self);
	const FName K = SwcKind(Self->CreatureId);
	AWRCharacterBase* Target = Cast<AWRCharacterBase>(Self->GetAggroTarget());
	// Holding something (a death roll, a mouthful, a limb), in its shell, or calmed by the blood rite: nothing else.
	if (Body->Held.IsValid() || Body->LatchedOn.IsValid() || Body->Ritualist.IsValid())
	{
		return true;
	}
	if (K == TEXT("slimeshell") && T < Body->WithdrawUntil)
	{
		return true;
	}
	// The next row it goes into at once (the burst's roll, the tongue's gulp, the clatter, the flash).
	if (!Body->ForceNext.IsNone())
	{
		AWRCharacterBase* On = Body->ForceNext == TEXT("gulp") && Body->Pulled.IsValid() ? Body->Pulled.Get() : Target;
		const FName Next = Body->ForceNext;
		Body->ForceNext = NAME_None;
		if (T < Body->ForceUntil && On && On->IsAlive())
		{
			AI->BreatheUntil = 0.0;
			AI->SetControlRotation((On->GetActorLocation() - Self->GetActorLocation()).Rotation());
			if (AI->StartRow(Self, On, Next, true))
			{
				return true;
			}
		}
	}
	// A hexed beast sees further (aggro x1.3); in the Red Tide the Murkjaws hunt by day.
	if (!Target && !Self->IsAsleep())
	{
		const FWRCreatureDef* Def = Self->GetDef();
		float Reach = 0.f;
		if (Body->IsHexed() && Def) Reach = Def->AggroRadius * SwcT(TEXT("hex.aggroMult"), 1.3f);
		if (K == TEXT("murkjaw") && WRSwamp::IsTide(Self) && SwcDay(Self)) Reach = FMath::Max(Reach, SwcT(TEXT("tide.murkjawHuntRadius"), 4000.f));
		if (Reach > 0.f)
		{
			if (AWRCharacter* P = SwcNearestSurvivor(Self, Reach))
			{
				Self->SetAggroTarget(P);
				Target = P;
			}
		}
	}
	// The Hexcrows would rather peck what can't fight back: a held, latched or downed survivor near them.
	if (K == TEXT("hexcrow") && Target)
	{
		for (TActorIterator<AWRCharacter> It(Self->GetWorld()); It; ++It)
		{
			if (*It == Target || !It->IsAlive() || FVector::Dist(It->GetActorLocation(), Self->GetActorLocation()) > SwcT(TEXT("hexcrow.preferRadius"), 1500.f)) continue;
			if (WRSwampGrip::IsHeld(*It) || UWRSwampBody::LatchCount(*It) > 0 || It->HasTag(WRTags::Status_Staggered))
			{
				Self->SetAggroTarget(*It);
				break;
			}
		}
	}
	// A beat of the living world under way (the murder over the dying and the dead).
	if (!Target && K == TEXT("hexcrow") && Body->CircleOver.IsValid() && T < Body->BeatUntil)
	{
		AActor* Over = Body->CircleOver.Get();
		const AWRCharacterBase* OverC = Cast<AWRCharacterBase>(Over);
		const bool bDead = OverC && !OverC->IsAlive();
		if (!OverC || (!bDead && SwcHealthFrac(OverC) > SwcT(TEXT("hexcrow.circleBelow"), 0.4f) + 0.1f))
		{
			Body->CircleOver = nullptr;
			return false;
		}
		const float Phase = float(Self->GetUniqueID() % 360);
		UCharacterMovementComponent* Move = Self->GetCharacterMovement();
		if (bDead && T - Body->CircleSince > SwcT(TEXT("hexcrow.landAfter"), 14.f))
		{
			// Down on the carcass: a ring of crows round it, pecking.
			const FVector Spot = Over->GetActorLocation() + FRotator(0.f, Phase, 0.f).Vector() * (Over->GetSimpleCollisionRadius() + 120.f);
			const FVector To = Spot - Self->GetActorLocation();
			if (Move->IsFlying() && To.Size() > 250.f)
			{
				Self->AddMovementInput(To.GetSafeNormal(), 0.7f);
			}
			else
			{
				if (Move->IsFlying()) Move->SetMovementMode(MOVE_Falling);
				if (To.Size2D() > 80.f) Self->AddMovementInput(To.GetSafeNormal2D(), 0.3f);
				AI->SetControlRotation((Over->GetActorLocation() - Self->GetActorLocation()).Rotation());
				if (T >= Body->NextPeckAt)
				{
					Body->NextPeckAt = T + FMath::FRandRange(1.5f, 3.5f);
					Self->PlaySwing(EWRSwingKind::Bite, 0.45f);
				}
			}
			return true;
		}
		// Circling: a slow ring high over it, each crow at its own place on the ring.
		const float A = FMath::DegreesToRadians(Phase + float(T) * SwcT(TEXT("hexcrow.circleDegrees"), 25.f));
		const FVector Goal = Over->GetActorLocation() + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * SwcT(TEXT("hexcrow.circleRadius"), 900.f) + FVector(0.f, 0.f, SwcT(TEXT("hexcrow.circleHeight"), 1400.f));
		Move->SetMovementMode(MOVE_Flying);
		Self->AddMovementInput((Goal - Self->GetActorLocation()).GetSafeNormal(), 0.6f);
		return true;
	}
	if (Target)
	{
		return false;   // its rows
	}
	// --- Wild and idle ---
	if (K == TEXT("murkjaw") && !Self->IsFleeing() && !Self->HuntTarget.IsValid() && !Self->IsAsleep())
	{
		// In Miasma it moves its ambush to the boardwalks' edges (where people walk over the water).
		if (WRSwamp::WeatherNow(Self) == TEXT("miasma") && T >= Body->NextBeatAt)
		{
			Body->NextBeatAt = T + 30.0;
			Body->BeatStep = 0;
			float BestD = SwcT(TEXT("murkjaw.boardwalkSearch"), 6000.f);
			for (const FWRSwampData::FRoute& Route : FWRSwampData::Get().Routes)
			{
				if (Route.Kind != TEXT("boardwalk")) continue;
				for (const FVector& P : Route.Points)
				{
					const float D = FVector::Dist2D(P, Self->GetActorLocation());
					if (D >= BestD) continue;
					const FVector Side = P + FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f).Vector() * (Route.Width * 0.5f + SwcT(TEXT("murkjaw.boardwalkHunt"), 200.f));
					if (WRSwamp::WaterAt(Self, Side) == EWRSwampWater::Dry) continue;
					BestD = D;
					Body->BeatAt = Side;
					Body->BeatStep = 1;
				}
			}
		}
		if (Body->BeatStep == 1)
		{
			const FVector To = Body->BeatAt - Self->GetActorLocation();
			if (To.Size2D() > 250.f)
			{
				Self->AddMovementInput(To.GetSafeNormal2D(), 0.6f);
				return true;
			}
			Body->BeatStep = 0;
			Self->SetHome(Body->BeatAt);
		}
		if (SwcInWater(Self))
		{
			// Waiting under the duckweed: eyes and ridge only. Whatever comes within 7 m of its water gets the burst.
			const float Trigger = SwcT(TEXT("murkjaw.ambushWater"), 700.f) * WRSwamp::AmbushRangeMult(Self) + Self->GetSimpleCollisionRadius();
			if (AWRCharacter* P = SwcNearestSurvivor(Self, Trigger))
			{
				Self->SetAggroTarget(P);
				Body->AmbushSince = -1.0;
				return false;
			}
			Body->AmbushSince = T;
			UCharacterMovementComponent* Move = Self->GetCharacterMovement();
			Move->Buoyancy = SwcT(TEXT("murkjaw.ambushBuoyancy"), 0.92f);
			if (Move->IsSwimming())
			{
				const float Visual = FMath::Max(40.f, SwcNum(SwcLook(Self->CreatureId), TEXT("flatten"), Self->GetSimpleCollisionHalfHeight() * 2.f));
				const float WantZ = FWRSwampData::Get().SeaZ - Visual + SwcT(TEXT("murkjaw.eyesAbove"), 12.f) + Self->GetSimpleCollisionHalfHeight();
				FVector P = Self->GetActorLocation();
				P.Z = FMath::FInterpTo(P.Z, WantZ, DeltaSeconds, 1.5f);
				Move->Velocity = FVector::ZeroVector;
				Self->SetActorLocation(P, true);
			}
			return true;
		}
	}
	if (K == TEXT("mudhulk") && !Self->IsFleeing() && !Self->IsAsleep())
	{
		// The yawn: its warning display. Keep coming and it comes for you, and the herd with it.
		const float Range = SwcT(TEXT("mudhulk.yawnRadius"), 1400.f);
		if (AWRCharacter* P = SwcNearestSurvivor(Self, Range))
		{
			const float D = FVector::Dist2D(P->GetActorLocation(), Self->GetActorLocation());
			AI->SetControlRotation((P->GetActorLocation() - Self->GetActorLocation()).Rotation());
			Self->SetActorRotation(FMath::RInterpConstantTo(Self->GetActorRotation(), FRotator(0.f, (P->GetActorLocation() - Self->GetActorLocation()).Rotation().Yaw, 0.f), DeltaSeconds, 90.f));
			if (D < Range * SwcT(TEXT("mudhulk.chargeShare"), 0.45f))
			{
				Self->SetAggroTarget(P);
				if (Self->Spawner.IsValid()) Self->Spawner->AlertPack(P);
				return false;
			}
			if (T >= Body->NextYawnAt)
			{
				Body->NextYawnAt = T + SwcT(TEXT("mudhulk.yawnEvery"), 8.f);
				Body->GapeUntil = T + 1.4;
				Self->PlaySwing(EWRSwingKind::Strike, 1.2f);
				WRSound::Broadcast(Self, TEXT("mire.hulk_yawn"));
				Signature(Self);
			}
			return true;
		}
	}
	if (K == TEXT("lanterntoad") && !Self->IsFleeing())
	{
		// Startled: the flash in your face, then it hops off.
		const TMap<FName, FWRAttackRow>* Rows = FWRAttackRows::Get().Creature(Self->CreatureId);
		const FWRAttackRow* Puff = Rows ? Rows->Find(TEXT("flash_puff")) : nullptr;
		AWRCharacter* P = Puff ? SwcNearestSurvivor(Self, Puff->MaxRange + Self->GetSimpleCollisionRadius() + 40.f) : nullptr;
		if (P && T - Body->LastAttackAt > Puff->Cooldown)
		{
			AI->SetControlRotation((P->GetActorLocation() - Self->GetActorLocation()).Rotation());
			if (AI->StartRow(Self, P, TEXT("flash_puff"), false)) return true;
		}
	}
	if (K == TEXT("reedfowl") && Self->IsFleeing())
	{
		// Flees, unless cornered: then a peck at whatever cornered it.
		AWRCharacterBase* From = Cast<AWRCharacterBase>(Self->GetFleeFrom());
		if (From && From->IsAlive())
		{
			const FVector Away = (Self->GetActorLocation() - From->GetActorLocation()).GetSafeNormal2D();
			FHitResult Wall;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(WRSwampCornered), false, Self);
			const bool bWall = Self->GetWorld()->SweepSingleByChannel(Wall, Self->GetActorLocation(), Self->GetActorLocation() + Away * 200.f, FQuat::Identity, ECC_WorldStatic,
				FCollisionShape::MakeSphere(Self->GetSimpleCollisionRadius() * 0.8f), Q);
			const bool bStuck = Self->GetVelocity().Size2D() < 50.f;
			if (bWall || bStuck) { if (Body->BeatUntil < T) { Body->BeatUntil = T + 30.0; Body->BeatAt = Self->GetActorLocation(); Body->BeatStep = 2; } }
			else if (Body->BeatStep == 2) Body->BeatStep = 0;
			if (Body->BeatStep == 2 && T - (Body->BeatUntil - 30.0) > 0.6 && FVector::Dist2D(From->GetActorLocation(), Self->GetActorLocation()) < WRTune(TEXT("creatureAI.corneredRadius"), 450.f))
			{
				AI->TickRowFight(Self, From, DeltaSeconds);
				return true;
			}
		}
	}
	if (K == TEXT("mirestrider") && !Self->IsFleeing() && SwcDay(Self) && SwcShallow(Self, SwcNum(SwcRow(Self->CreatureId), TEXT("wadeDepth"), 140.f)))
	{
		// Standing for minutes in the shallows, then the stab: a fish taken.
		if (T >= Body->NextStabAt && Body->StabUntil < T)
		{
			Body->StabUntil = T + SwcRand(TEXT("mirestrider"), TEXT("standSeconds"), 90.f, 300.f);
			Body->NextBeatAt = T + SwcRand(TEXT("mirestrider"), TEXT("stabEvery"), 18.f, 40.f);
		}
		if (T < Body->StabUntil)
		{
			if (T >= Body->NextBeatAt)
			{
				Body->NextBeatAt = T + SwcRand(TEXT("mirestrider"), TEXT("stabEvery"), 18.f, 40.f);
				Self->PlaySwing(EWRSwingKind::Strike, 0.55f);
				WRFx::Broadcast(Self, TEXT("mire.splash"), Self->MouthLocation() * FVector(1.f, 1.f, 0.f) + FVector(0.f, 0.f, FWRSwampData::Get().SeaZ), FRotator::ZeroRotator, 0.35f, TEXT("mire.bill_stab"));
			}
			if (T + 0.1 >= Body->StabUntil) Body->NextStabAt = T + FMath::FRandRange(120.f, 240.f);
			return true;
		}
	}
	return false;
}

bool FWRSwampAI::TickTame(AWRCreatureAI* AI, AWRCreature* Self, float DeltaSeconds)
{
	UWRSwampBody* Body = UWRSwampBody::Get(Self);
	const TSharedPtr<FJsonObject> R = SwcRow(Self->CreatureId);
	if (!Body || SwcNum(R, TEXT("pouch"), 0.f) <= 0.f || Self->GetRider() || Self->IsBaby())
	{
		return false;
	}
	// A Lanterntoad on its owner's shoulder pouch while they swim (its light comes along), and down again on land.
	AWRCharacter* Owner = SwcOwnerBody(Self->GetWorld(), Self->TamedBy);
	const bool bWant = Owner && Self->TameOrder == EWRTameOrder::Follow && (Owner->GetCharacterMovement()->IsSwimming() || UWRWaterSubsystem::IsInWater(Owner))
		&& FVector::Dist(Owner->GetActorLocation(), Self->GetActorLocation()) < SwcT(TEXT("lanterntoad.pouchReach"), 2500.f);
	const bool bIn = Body->Has(WRSwampFlag::Pouched);
	if (bWant && !bIn)
	{
		Self->SetActorEnableCollision(false);
		Self->GetCharacterMovement()->DisableMovement();
		Self->AttachToActor(Owner, FAttachmentTransformRules::KeepWorldTransform);
		Self->SetActorRelativeLocation(FVector(-6.f, Owner->GetSimpleCollisionRadius() * 0.55f, Owner->GetSimpleCollisionHalfHeight() * 0.72f));
		Self->SetActorRelativeRotation(FRotator::ZeroRotator);
		Body->Set(WRSwampFlag::Pouched, true);
		Body->PouchOutSince = -1.0;
		WRSound::Broadcast(Self, TEXT("mire.toad_pouch"));
		return true;
	}
	if (bIn)
	{
		// Out of the water for a moment: it hops down.
		if (!bWant)
		{
			const double T = SwcNow(Self);
			if (Body->PouchOutSince < 0.0) Body->PouchOutSince = T;
			if (!Owner || T - Body->PouchOutSince > SwcT(TEXT("lanterntoad.pouchOutSeconds"), 1.5f))
			{
				Self->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
				const FVector From = Owner ? Owner->GetActorLocation() + Owner->GetActorRightVector() * (Owner->GetSimpleCollisionRadius() + 40.f) : Self->GetActorLocation();
				Self->SetActorLocation(From, false, nullptr, ETeleportType::TeleportPhysics);
				Self->SetActorEnableCollision(true);
				Self->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
				Body->Set(WRSwampFlag::Pouched, false);
				return false;
			}
		}
		else
		{
			Body->PouchOutSince = -1.0;
		}
		return true;
	}
	(void)AI; (void)DeltaSeconds;
	return false;
}

bool FWRSwampAI::RowAllowed(const AWRCreatureAI* AI, const AWRCreature* Self, const FWRAttackRow& Row, const AWRCharacterBase* Target, bool bReading)
{
	const UWRSwampBody* Body = UWRSwampBody::Get(Self);
	if (!Body || !Target)
	{
		return true;
	}
	const FName Id = Row.Id;
	const float Dist = FVector::Dist2D(Target->GetActorLocation(), Self->GetActorLocation());
	const bool bWet = SwcInWater(Self);
	if (bReading)
	{
		// The land lunge as a read: a drink or a charged heavy within 8 m.
		return (Target->HasTag(WRTags::State_Drinking) || Target->HasTag(WRTags::State_Charging)) && Dist <= SwcT(TEXT("murkjaw.readRadius"), 800.f) + Self->GetSimpleCollisionRadius();
	}
	if (Id == TEXT("duckweed_burst")) return bWet && Dist <= SwcT(TEXT("murkjaw.ambushWater"), 700.f) * WRSwamp::AmbushRangeMult(Self) + Self->GetSimpleCollisionRadius() + Target->GetSimpleCollisionRadius();
	if (Id == TEXT("swim_lunge") || Id == TEXT("wallow_surge")) return bWet;
	if (Id == TEXT("land_lunge")) return !Self->GetCharacterMovement()->IsSwimming();
	if (Id == TEXT("gulp") || Id == TEXT("clatter") || Id == TEXT("withdraw")) return false;   // forced only
	if (Id == TEXT("spring_lunge"))
	{
		// Pack flankers: only from the side (60+ degrees off its target's facing).
		const FVector Facing = FRotator(0.f, Target->GetController() ? Target->GetControlRotation().Yaw : Target->GetActorRotation().Yaw, 0.f).Vector();
		const float Off = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Facing, (Self->GetActorLocation() - Target->GetActorLocation()).GetSafeNormal2D()), -1.f, 1.f)));
		return Off >= SwcT(TEXT("latch.flankAngle"), 60.f);
	}
	if (Id == TEXT("dive_rake") || Id == TEXT("curse_cry")) return !Body->Has(WRSwampFlag::Grounded);
	if (Id == TEXT("peck_flurry")) return Body->Has(WRSwampFlag::Grounded) || !Self->GetCharacterMovement()->IsFlying() || Target->HasTag(WRTags::Status_Staggered) || WRSwampGrip::IsHeld(Target) || UWRSwampBody::LatchCount(Target) > 0;
	if (Id == TEXT("call_murder"))
	{
		if (Body->bMurderCalled) return false;
		for (TActorIterator<AWRCreature> It(Self->GetWorld()); It; ++It)
		{
			if (*It != Self && SwcWild(*It) && It->CreatureId == Self->CreatureId && !It->GetAggroTarget() && FVector::Dist(It->GetActorLocation(), Self->GetActorLocation()) < SwcT(TEXT("hexcrow.callRadius"), 6000.f)) return true;
		}
		return false;
	}
	if (Id == TEXT("mud_spit")) return Dist >= SwcT(TEXT("bogstalker.spitMinRange"), 450.f);
	if (Id == TEXT("latch")) return !Body->LatchedOn.IsValid();
	(void)AI;
	return true;
}

void FWRSwampAI::RowStarted(AWRCreatureAI* AI, AWRCreature* Self, const FWRAttackRow& Row, AWRCharacterBase* Target)
{
	UWRSwampBody* Body = UWRSwampBody::Get(Self);
	if (!Body)
	{
		return;
	}
	const double T = SwcNow(Self);
	Body->LastAttackAt = T;
	const FName Id = Row.Id;
	const float SeaZ = FWRSwampData::Get().SeaZ;
	if (Id == TEXT("duckweed_burst") && Target)
	{
		// The bellow that shivers the water, then the duckweed parts in a line toward you and bubbles rise.
		Body->AmbushSince = -1.0;
		Body->GapeUntil = T + Row.Windup;
		const FVector From = Self->GetActorLocation() * FVector(1.f, 1.f, 0.f) + FVector(0.f, 0.f, SeaZ + 2.f);
		const FVector To = Target->GetActorLocation() * FVector(1.f, 1.f, 0.f) + FVector(0.f, 0.f, SeaZ + 2.f);
		Body->MulticastLine(TEXT("mire.duckweed"), From, To, Row.Windup * 0.85f);
		WRFx::Broadcast(Self, TEXT("mire.bubbles"), From, FRotator::ZeroRotator, 1.6f, TEXT("mire.murkjaw_bellow"));
		Signature(Self);   // the low bellow that shivers the water before the burst
	}
	else if (Id == TEXT("death_roll") || Id == TEXT("gape_chomp"))
	{
		Body->GapeUntil = T + Row.Windup + Row.Hold;
		if (Id == TEXT("gape_chomp"))
		{
			WRSound::Broadcast(Self, TEXT("mire.hulk_yawn"));
			Signature(Self);   // the yawn groan
		}
	}
	else if (Id == TEXT("charge") && SwcKind(Self->CreatureId) == TEXT("mudhulk"))
	{
		Signature(Self);   // the yawn groan before a charge
	}
	else if (Id == TEXT("squeal_charge"))
	{
		Signature(Self);   // the rising squeal
	}
	else if (Id == TEXT("swim_lunge") && Target)
	{
		// The V-wake running at you.
		Body->MulticastLine(TEXT("mire.wake"), Self->GetActorLocation() * FVector(1.f, 1.f, 0.f) + FVector(0.f, 0.f, SeaZ + 2.f), Target->GetActorLocation() * FVector(1.f, 1.f, 0.f) + FVector(0.f, 0.f, SeaZ + 2.f), Row.Windup);
	}
	else if (Id == TEXT("tongue_lash"))
	{
		WRSound::Broadcast(Self, TEXT("mire.throat_pump"));
	}
	else if (Id == TEXT("latch"))
	{
		WRSound::Broadcast(Self, TEXT("mire.leech_click"));   // the three-jaw Y-click
		Signature(Self);
	}
	else if (Id == TEXT("curse_cry"))
	{
		// It hangs in the air (or stands) with its head thrown back.
		UCharacterMovementComponent* Move = Self->GetCharacterMovement();
		Move->Velocity = FVector::ZeroVector;
	}
	else if (Id == TEXT("wallow_surge") && Target)
	{
		// It sinks; a bulge of water runs to where you stand.
		Body->MulticastLine(TEXT("mire.bubbles"), Self->GetActorLocation() * FVector(1.f, 1.f, 0.f) + FVector(0.f, 0.f, SeaZ), Target->GetActorLocation() * FVector(1.f, 1.f, 0.f) + FVector(0.f, 0.f, SeaZ), Row.Windup);
	}
	else if (Id == TEXT("flash_puff"))
	{
		WRSound::Broadcast(Self, TEXT("mire.toad_swell"));
		Signature(Self);   // the trill swelling as the sac brightens
	}
	else if (Id == TEXT("clatter"))
	{
		WRSound::Broadcast(Self, TEXT("mire.clatter"));
		Signature(Self);   // the clatter that turns a whole herd
	}
	else if (Id == TEXT("call_murder"))
	{
		WRSound::Broadcast(Self, TEXT("mire.murder_call"));
	}
	else if (Id == TEXT("dive_rake"))
	{
		Body->bDiveLanded = true;   // set false at the strike
	}
	(void)AI;
}

bool FWRSwampAI::StrikeRow(AWRCreatureAI* AI, AWRCreature* Self, const FWRAttackRow& Row, AWRCharacterBase* Target)
{
	UWRSwampBody* Body = UWRSwampBody::Get(Self);
	if (!Body || !AI)
	{
		return false;
	}
	const FName M = Row.Motion;
	const FName Id = Row.Id;
	const double T = SwcNow(Self);
	UWorld* World = Self->GetWorld();
	if (Id == TEXT("duckweed_burst"))
	{
		// Up out of the water in a sheet of spray (the leap itself is the generic lunge).
		WRFx::Broadcast(Self, TEXT("mire.splash"), Self->GetActorLocation() * FVector(1.f, 1.f, 0.f) + FVector(0.f, 0.f, FWRSwampData::Get().SeaZ), FRotator::ZeroRotator, 1.4f, TEXT("mire.burst"));
		return false;
	}
	if (M == TEXT("cry"))
	{
		// The rising caw: a cone of black breath from the beak (TickStrike works out who's in it and in sight).
		const float Range = WRSwamp::CurseCryMult(Self) > 1.f ? SwcT(TEXT("hexcrow.cryRangeMiasma"), 1500.f) : SwcT(TEXT("hexcrow.cryRange"), 1000.f);
		const FVector Beak = Self->MouthLocation();
		WRFx::Broadcast(Self, TEXT("mire.curse"), Beak, Self->GetActorForwardVector().Rotation(), Range / 900.f, TEXT("mire.curse_caw"));
		Signature(Self);   // the rising curse caw (several crows: a ragged chorus)
		return true;
	}
	if (M == TEXT("tongue"))
	{
		WRSound::Broadcast(Self, TEXT("mire.tongue_crack"));
		Signature(Self);   // two throat pumps, then the wet tongue-crack
		AI->RowDir = ((AI->RowAim + FVector(0.f, 0.f, 90.f)) - Self->MouthLocation()).GetSafeNormal();
		return true;
	}
	if (M == TEXT("flop"))
	{
		// The leap: a squat, then the whole body to a point up to 8 m off.
		FVector To = AI->RowAim - SwcFeet(Self);
		const float Max = SwcT(TEXT("bogstalker.flopMax"), 800.f);
		if (To.Size2D() > Max) To = To.GetSafeNormal2D() * Max + FVector(0.f, 0.f, To.Z);
		const float Flight = SwcT(TEXT("bogstalker.flopFlight"), 0.6f);
		Self->SetActorRotation(FRotator(0.f, To.Rotation().Yaw, 0.f));
		Self->LaunchCharacter(FVector(To.X / Flight, To.Y / Flight, 0.5f * 980.f * Flight + To.Z / Flight), true, true);
		Body->bFlopLanded = false;
		AI->RowPhaseEnd = FMath::Max(AI->RowPhaseEnd, T + Flight + 0.15);
		return true;
	}
	if (M == TEXT("mudspit"))
	{
		// A ballistic glob at where you stood (never homing, never inside 4.5 m): mud slows you.
		const FVector From = Self->MouthLocation();
		const FVector Aim = AI->RowAim + FVector(0.f, 0.f, 50.f);
		if (FVector::Dist2D(From, Aim) < SwcT(TEXT("bogstalker.spitMinRange"), 450.f))
		{
			return true;
		}
		FActorSpawnParameters Params;
		Params.Owner = Self;
		Params.Instigator = Self;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AWRProjectile* Glob = World->SpawnActor<AWRProjectile>(AWRProjectile::StaticClass(), From, (Aim - From).Rotation(), Params))
		{
			FWRHit Hit;
			Hit.Instigator = Self;
			Hit.Damage = Self->Weapon.FlatDamage * Row.Dmg * SwcDifficulty();
			Hit.PoiseDamage = Row.Poise;
			Hit.Impact = Row.Impact;
			Hit.bProjectile = true;
			Hit.PhysType = 2;
			Hit.Element = WRTags::Element_Physical;
			Hit.Origin = From;
			for (const TPair<FName, float>& B : Row.Buildups) Hit.Buildups.Add(B.Key, B.Value);
			const float Speed = Row.ProjectileSpeed > 0.f ? Row.ProjectileSpeed : 900.f;
			Glob->Init(Hit, Speed, SwcHex(TEXT("#4A3A26")));
			if (UProjectileMovementComponent* PM = Glob->FindComponentByClass<UProjectileMovementComponent>())
			{
				const float Gravity = SwcT(TEXT("bogstalker.spitGravity"), 1.f);
				PM->ProjectileGravityScale = Gravity;
				PM->Velocity = AWRProjectile::LobVelocity(From, Aim, Speed, Gravity);
			}
			if (UWRSwampLife* Life = World->GetSubsystem<UWRSwampLife>()) Life->TrackSpit(Glob, Self);
		}
		WRSound::Broadcast(Self, TEXT("mire.spit"));
		return true;
	}
	if (M == TEXT("call"))
	{
		// Every Hexcrow within 60 m comes for the same target; once a fight.
		Body->bMurderCalled = true;
		int32 Came = 0;
		for (TActorIterator<AWRCreature> It(World); It; ++It)
		{
			if (*It == Self || !SwcWild(*It) || It->CreatureId != Self->CreatureId || It->GetAggroTarget() || FVector::Dist(It->GetActorLocation(), Self->GetActorLocation()) > FMath::Max(Row.Radius, SwcT(TEXT("hexcrow.callRadius"), 6000.f))) continue;
			It->SetAsleep(false);
			if (Target) { It->SetAggroTarget(Target); ++Came; }
			if (UWRSwampBody* B = UWRSwampBody::Get(*It)) B->bMurderCalled = true;
		}
		UE_LOG(LogWildreach, Log, TEXT("SWAMP %s calls the murder: %d come"), *Self->GetName(), Came);
		return true;
	}
	if (M == TEXT("clatter"))
	{
		Rally(Self, Target);
		return true;
	}
	if (M == TEXT("puff"))
	{
		// The flash: a 3 m burst of gold-green light; then it hops away.
		const float Radius = FMath::Max(150.f, Row.Radius);
		WRFx::Broadcast(Self, TEXT("mire.flash"), Self->GetActorLocation(), FRotator::ZeroRotator, Radius / 300.f, TEXT("mire.toad_flash"));
		for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
		{
			if (*It != Self && It->IsAlive() && FVector::Dist(It->GetActorLocation(), Self->GetActorLocation()) < Radius + It->GetSimpleCollisionRadius() && Cast<AWRCharacter>(*It))
			{
				It->Status->ApplyStatus(WRTags::Status_Blind, SwcT(TEXT("lanterntoad.blindSeconds"), 1.5f));
			}
		}
		if (Target) Self->StartFleeing(Target, TEXT("the flash"));
		return true;
	}
	if (M == TEXT("surge"))
	{
		// Up from under you: everything within 3 m is thrown (roll out of it; a jump doesn't clear it).
		const float Radius = FMath::Max(150.f, Row.Radius);
		WRFx::Broadcast(Self, TEXT("mire.splash"), Self->GetActorLocation() * FVector(1.f, 1.f, 0.f) + FVector(0.f, 0.f, FWRSwampData::Get().SeaZ), FRotator::ZeroRotator, Radius / 120.f, TEXT("mire.surge"));
		for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
		{
			if (*It != Self && It->IsAlive() && Self->IsHostileTo(*It) && FVector::Dist2D(It->GetActorLocation(), Self->GetActorLocation()) < Radius + It->GetSimpleCollisionRadius()) AI->RowHit(Self, *It);
		}
		return true;
	}
	if (M == TEXT("flurry"))
	{
		Body->PeckIndex = -1;
		return true;
	}
	if (M == TEXT("withdraw"))
	{
		return true;
	}
	if (Id == TEXT("dive_rake"))
	{
		// Wings folded: a straight line at where the target is now; it lands after, and is down a moment.
		const FVector Aim = Target ? Target->GetActorLocation() : AI->RowAim;
		const FVector Dir = (Aim - Self->GetActorLocation()).GetSafeNormal();
		AI->RowDir = Dir.GetSafeNormal2D();
		Self->SetActorRotation(FRotator(0.f, Dir.Rotation().Yaw, 0.f));
		Self->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		Self->LaunchCharacter(Dir * (Row.Speed > 0.f ? Row.Speed : 1300.f), true, true);
		Body->bDiveLanded = false;
		return true;
	}
	return false;
}

void FWRSwampAI::TickStrike(AWRCreatureAI* AI, AWRCreature* Self, const FWRAttackRow& Row, float DeltaSeconds)
{
	UWRSwampBody* Body = UWRSwampBody::Get(Self);
	if (!Body || !AI)
	{
		return;
	}
	const FName M = Row.Motion;
	const double T = SwcNow(Self);
	UWorld* World = Self->GetWorld();
	if (M == TEXT("cry"))
	{
		// The cone from the beak: in range, in the cone, in sight (solid cover blocks it).
		const float Range = WRSwamp::CurseCryMult(Self) > 1.f ? SwcT(TEXT("hexcrow.cryRangeMiasma"), 1500.f) : SwcT(TEXT("hexcrow.cryRange"), 1000.f);
		const float Cos = FMath::Cos(FMath::DegreesToRadians(SwcT(TEXT("hexcrow.cryHalfAngle"), 20.f)));
		const FVector Beak = Self->MouthLocation();
		const FVector Fwd = Self->GetActorForwardVector();
		for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
		{
			if (*It == Self || !It->IsAlive() || !Self->IsHostileTo(*It) || AI->RowHits.Contains(*It)) continue;
			const FVector To = It->GetActorLocation() - Beak;
			if (To.Size() > Range + It->GetSimpleCollisionRadius() || FVector::DotProduct(Fwd.GetSafeNormal2D(), To.GetSafeNormal2D()) < Cos) continue;
			FHitResult Block;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(WRSwampCurseCry), false, Self);
			Q.AddIgnoredActor(*It);
			if (World->LineTraceSingleByChannel(Block, Beak, It->GetActorLocation(), ECC_WorldStatic, Q)) continue;
			AI->RowHit(Self, *It);
		}
		return;
	}
	if (M == TEXT("tongue"))
	{
		// Out along the line it aimed (no tracking), the first body it touches is struck and pulled.
		if (Body->Pulled.IsValid()) return;
		const float Frac = FMath::Clamp(float(T - AI->RowStrikeStarted) / FMath::Max(0.05f, Row.Active), 0.f, 1.f);
		const FVector From = Self->MouthLocation();
		const FVector To = From + AI->RowDir * Row.Reach * Frac;
		for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
		{
			if (*It == Self || !It->IsAlive() || !Self->IsHostileTo(*It) || AI->RowHits.Contains(*It)) continue;
			const FVector Closest = FMath::ClosestPointOnSegment(It->GetActorLocation(), From, To);
			if (FVector::Dist(Closest, It->GetActorLocation()) < FMath::Max(30.f, Row.Radius) + It->GetSimpleCollisionRadius())
			{
				AI->RowHit(Self, *It);
				break;
			}
		}
		return;
	}
	if (M == TEXT("flop"))
	{
		if (Body->bFlopLanded) return;
		if (T - AI->RowStrikeStarted < 0.2 || Self->GetCharacterMovement()->IsFalling())
		{
			AI->RowPhaseEnd = FMath::Max(AI->RowPhaseEnd, T + 0.05);
			return;
		}
		// Down: the ring of mud and water rolls out (low: a jump clears it).
		Body->bFlopLanded = true;
		const float Radius = FMath::Max(150.f, Row.Radius);
		const FVector Feet = SwcFeet(Self);
		const bool bWet = WRSwamp::IsWet(Feet + FVector(0.f, 0.f, 20.f)) && WRSwamp::WaterAt(Self, Feet) != EWRSwampWater::Dry;
		WRFx::Broadcast(Self, bWet ? TEXT("mire.splash") : TEXT("mire.mud"), Feet, FRotator::ZeroRotator, Radius / 120.f, TEXT("mire.flop_land"));
		for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
		{
			if (*It != Self && It->IsAlive() && Self->IsHostileTo(*It) && FVector::Dist2D(It->GetActorLocation(), Self->GetActorLocation()) < Radius + It->GetSimpleCollisionRadius()) AI->RowHit(Self, *It);
		}
		AI->RowPhaseEnd = FMath::Min(AI->RowPhaseEnd, T + 0.05);
		return;
	}
	if (M == TEXT("flurry"))
	{
		// Three pecks across the active window, each its own blow.
		const int32 Repeats = FMath::Max(1, Row.Repeats);
		const int32 Want = FMath::Min(Repeats - 1, int32((T - AI->RowStrikeStarted) / FMath::Max(0.05f, Row.Active / Repeats)));
		if (Want <= Body->PeckIndex) return;
		Body->PeckIndex = Want;
		AI->RowHits.Reset();
		Self->PlaySwing(EWRSwingKind::Bite, FMath::Max(0.12f, Row.Active / Repeats));
		const FVector Fwd = Self->GetActorForwardVector().GetSafeNormal2D();
		for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
		{
			if (*It == Self || !It->IsAlive() || !Self->IsHostileTo(*It)) continue;
			const FVector To = It->GetActorLocation() - Self->GetActorLocation();
			if (To.Size2D() - It->GetSimpleCollisionRadius() - Self->GetSimpleCollisionRadius() <= Row.Reach && FVector::DotProduct(Fwd, To.GetSafeNormal2D()) >= FMath::Cos(FMath::DegreesToRadians(Row.Arc)))
			{
				AI->RowHit(Self, *It);
				break;
			}
		}
		return;
	}
	(void)DeltaSeconds;
}

void FWRSwampAI::Resolved(AWRCreature* Self, AWRCharacterBase* Victim, FName RowId, EWRHitResult Result)
{
	UWRSwampBody* Body = UWRSwampBody::Get(Self);
	if (!Body || !Victim || !Self->IsAlive())
	{
		return;
	}
	const double T = SwcNow(Self);
	const bool bLanded = Result == EWRHitResult::Hit || Result == EWRHitResult::GuardBroken;
	if (RowId == TEXT("duckweed_burst") && bLanded && Victim->IsAlive())
	{
		// Out of the burst into the roll (40%): its gape is the tell, the dodge still works.
		if (FMath::FRand() < SwcT(TEXT("murkjaw.rollAfterBurst"), 0.4f))
		{
			Body->ForceNext = TEXT("death_roll");
			Body->ForceUntil = T + 2.0;
			CutRow(Self, 0.15f);
		}
	}
	else if (RowId == TEXT("death_roll") && bLanded && Victim->IsAlive())
	{
		StartHold(Self, Body, Victim, TEXT("death_roll"));
	}
	else if (RowId == TEXT("gulp") && bLanded && Victim->IsAlive())
	{
		StartHold(Self, Body, Victim, TEXT("gulp"));
	}
	else if (RowId == TEXT("latch"))
	{
		if (bLanded && Victim->IsAlive())
		{
			StartLatch(Self, Body, Victim);
		}
		else if (Result == EWRHitResult::Blocked || Result == EWRHitResult::Parried)
		{
			// A block knocks it off the guard: thrown back, stunned a moment.
			const FVector Away = (Self->GetActorLocation() - Victim->GetActorLocation()).GetSafeNormal2D();
			Self->LaunchCharacter(Away * SwcT(TEXT("latch.knockOffPush"), 350.f) + FVector(0.f, 0.f, 200.f), true, true);
			Self->Status->Stagger(SwcT(TEXT("latch.dropStagger"), 0.6f));
		}
	}
	else if (RowId == TEXT("tongue_lash") && (bLanded || Result == EWRHitResult::Blocked))
	{
		// Reeled in: 6 m toward the mouth (half that through a full guard).
		const float Pull = SwcT(TEXT("bogstalker.pull"), 600.f) * (Result == EWRHitResult::Blocked ? SwcT(TEXT("bogstalker.pullGuardMult"), 0.5f) : 1.f);
		const FVector Mouth = Self->MouthLocation();
		const FVector To = Mouth - Victim->GetActorLocation();
		const float Dist = FMath::Max(0.f, float(To.Size2D()) - Self->GetSimpleCollisionRadius() * 0.3f);
		const float Travel = FMath::Min(Pull, Dist);
		const float Seconds = SwcT(TEXT("bogstalker.pullSeconds"), 0.35f);
		Victim->LaunchCharacter(To.GetSafeNormal2D() * (Travel / Seconds) + FVector(0.f, 0.f, 180.f), true, true);
		Body->Pulled = Victim;
		Body->Holding = Victim;
		Body->PullCheckAt = T + Seconds + 0.1;
	}
	else if (RowId == TEXT("wallow_surge") && bLanded && Victim->IsAlive())
	{
		// Overturned: thrown up and off the rising back, down on the ground.
		const FVector Away = (Victim->GetActorLocation() - Self->GetActorLocation()).GetSafeNormal2D();
		Victim->LaunchCharacter(Away * 250.f + FVector(0.f, 0.f, SwcT(TEXT("mudhulk.surgeLift"), 600.f)), true, true);
		Victim->Status->Stagger(SwcT(TEXT("mudhulk.surgeDown"), 1.0f));
	}
}

bool FWRSwampAI::Grounded(const AWRCreature* Self)
{
	const UWRSwampBody* Body = UWRSwampBody::Get(Self);
	return Body && Body->Has(WRSwampFlag::Grounded);
}

float FWRSwampAI::Incoming(const AWRCreature* Self, const FWRHit& Hit, float Damage)
{
	const UWRSwampBody* Body = UWRSwampBody::Get(Self);
	if (!Body || Hit.bDamageOverTime)
	{
		return Damage;
	}
	const FName K = SwcKind(Self->CreatureId);
	if (K == TEXT("murkjaw"))
	{
		// The soft belly shows when it rolls or rears (a lunge's or a roll's wind-up): x1.3 there.
		bool bRearing = Body->Has(WRSwampFlag::Rolling) || Body->Has(WRSwampFlag::Gape);
		if (const AWRCreatureAI* AI = Cast<AWRCreatureAI>(Self->GetController()))
		{
			bRearing |= (AI->RowPhase == 1 || AI->RowPhase == 2) && (AI->RowId == TEXT("land_lunge") || AI->RowId == TEXT("duckweed_burst") || AI->RowId == TEXT("death_roll"));
		}
		if (bRearing && (Body->Has(WRSwampFlag::Rolling) || Self->IsNearLockPoint(Hit.ImpactPoint, TEXT("belly"), Self->GetSimpleCollisionRadius() * 1.3f)))
		{
			Damage *= SwcT(TEXT("murkjaw.bellyMult"), 1.3f);
		}
	}
	else if (K == TEXT("slimeshell") && Body->Has(WRSwampFlag::Withdrawn))
	{
		const TMap<FName, FWRAttackRow>* Rows = FWRAttackRows::Get().Creature(Self->CreatureId);
		const FWRAttackRow* W = Rows ? Rows->Find(TEXT("withdraw")) : nullptr;
		Damage *= W ? W->DamageTaken : 0.3f;
	}
	return Damage;
}

void FWRSwampAI::Damaged(AWRCreature* Self, const FWRHit& Hit, float Damage)
{
	UWRSwampBody* Body = UWRSwampBody::Get(Self);
	if (!Body || !Self->HasAuthority())
	{
		return;
	}
	const double T = SwcNow(Self);
	const FName K = SwcKind(Self->CreatureId);
	AActor* By = Hit.Instigator;
	// Holding someone: an ally's blows on its head (the Murkjaw) or throat (the Bogstalker) free them.
	if (Body->Held.IsValid() && By != Body->Held.Get() && Damage > 0.f)
	{
		const FName Spot = Body->HoldKind == TEXT("gulp") ? FName(TEXT("throat")) : FName(TEXT("head"));
		if (Hit.ImpactPoint.IsZero() || Self->IsNearLockPoint(Hit.ImpactPoint, Spot, Self->GetSimpleCollisionRadius() * 1.2f))
		{
			Body->FreeDamage += Damage;
			const float Need = Body->HoldKind == TEXT("gulp") ? SwcT(TEXT("bogstalker.throatFree"), 100.f) : SwcT(TEXT("murkjaw.headFree"), 120.f);
			if (Body->FreeDamage >= Need)
			{
				UE_LOG(LogWildreach, Log, TEXT("SWAMP %s lets go (struck free)"), *Self->GetName());
				Release(Self, Body, true);
			}
		}
	}
	// Any blow knocks a feeding leech off.
	if (Body->LatchedOn.IsValid())
	{
		Unlatch(Self, Body, true);
	}
	if (Self->IsTamed())
	{
		return;
	}
	if (K == TEXT("hexcrow"))
	{
		const AWRCreatureAI* AI = Cast<AWRCreatureAI>(Self->GetController());
		if (AI && AI->RowId == TEXT("curse_cry") && (AI->RowPhase == 1 || AI->RowPhase == 2) && Damage > 0.f)
		{
			// A blow in the cry's wind-up chokes it off.
			Self->Status->Stagger(SwcT(TEXT("hexcrow.cryInterrupt"), 0.5f));
		}
		if (Body->Has(WRSwampFlag::Grounded))
		{
			// Each blow while it's down keeps it down longer (up to the cap from when it fell).
			Body->GroundedUntil = FMath::Min(Body->GroundedSince + SwcT(TEXT("hexcrow.groundedMax"), 6.f), Body->GroundedUntil + SwcT(TEXT("hexcrow.groundedPerHit"), 0.6f));
		}
	}
	else if (K == TEXT("mirestrider") && T - Body->LastClatterAt > SwcT(TEXT("mirestrider.clatterCooldown"), 10.f))
	{
		// The clatter the moment one is hit.
		Body->LastClatterAt = T;
		if (const AWRCharacterBase* Who = Cast<AWRCharacterBase>(By); Who && Who != Self)
		{
			if (!Self->GetAggroTarget()) Self->SetAggroTarget(By);
			Body->ForceNext = TEXT("clatter");
			Body->ForceUntil = T + 1.5;
		}
	}
	else if (K == TEXT("slimeshell") && Damage > 0.f)
	{
		if (T >= Body->WithdrawUntil) Signature(Self);   // the shell's withdraw clack
		Body->WithdrawUntil = T + SwcT(TEXT("slimeshell.withdrawSeconds"), 6.f);
	}
	else if (K == TEXT("reedfowl") && T - Body->LastClatterAt > SwcT(TEXT("reedfowl.alarmCooldown"), 6.f))
	{
		// The alarm honk as it bolts.
		Body->LastClatterAt = T;
		Signature(Self);
	}
	else if (K == TEXT("lanterntoad") && By && T - Body->LastAttackAt > 4.0)
	{
		Body->ForceNext = TEXT("flash_puff");
		Body->ForceUntil = T + 1.0;
		if (AWRCharacterBase* Who = Cast<AWRCharacterBase>(By)) Self->SetAggroTarget(Who);
	}
	// A herd defends its own against a wild attacker (leeches on a Mudhulk: the herd charges in).
	if (const AWRCreature* Wild = Cast<AWRCreature>(By); Wild && !Wild->IsTamed() && Self->Spawner.IsValid() && (K == TEXT("mudhulk") || K == TEXT("mirestrider")))
	{
		for (TActorIterator<AWRCreature> It(Self->GetWorld()); It; ++It)
		{
			if (*It == Self || !SwcWild(*It) || It->Spawner != Self->Spawner || It->GetAggroTarget()) continue;
			It->HuntTarget = const_cast<AWRCreature*>(Wild);
			It->SetAggroTarget(const_cast<AWRCreature*>(Wild));
		}
	}
}

void FWRSwampAI::Died(AWRCreature* Self, AActor* Killer)
{
	UWRSwampBody* Body = UWRSwampBody::Get(Self);
	if (!Body)
	{
		return;
	}
	if (Body->Held.IsValid()) Release(Self, Body, false);
	if (Body->LatchedOn.IsValid()) Unlatch(Self, Body, false);
	if (Body->Has(WRSwampFlag::Pouched)) Self->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Body->Ritualist = nullptr;
	Body->RiteState = 0;
	// The Red Tide: a Gorgeleech gives twice the sacs.
	if (Self->CreatureId == TEXT("gorgeleech") && !Self->IsTamed())
	{
		const float Mult = WRSwamp::SacDropMult(Self);
		AActor* Getter = Killer;
		if (const AWRCreature* Tame = Cast<AWRCreature>(Killer); Tame && Tame->IsTamed()) Getter = SwcOwnerBody(Self->GetWorld(), Tame->TamedBy);
		const FWRCreatureDef* Def = Self->GetDef();
		int32 Base = 0;
		for (const TPair<FName, int32>& L : Def ? Def->Loot : TArray<TPair<FName, int32>>()) if (L.Key == TEXT("leech_sac")) Base = L.Value;
		const int32 Extra = FMath::RoundToInt(Base * (Mult - 1.f));
		if (Extra > 0)
		{
			if (UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Getter)) Inv->AddItem(TEXT("leech_sac"), Extra);
		}
	}
}

bool FWRSwampAI::Feed(AWRCreature* Self, AWRPlayerController* Player, FString& Message)
{
	UWRSwampBody* Body = UWRSwampBody::Get(Self);
	const FWRCreatureDef* Def = Self ? Self->GetDef() : nullptr;
	AWRCharacter* Who = Player ? Cast<AWRCharacter>(Player->GetPawn()) : nullptr;
	if (!Body || !Def || !Who || Self->IsTamed() || !Def->CanTameBy(TEXT("blood_ritual")) || !Self->IsAlive())
	{
		return false;
	}
	// The blood rite (cook 2.12.1): a Blood walker of 10+ points at the mouth of a Murkjaw that's knocked out, or below
	// 30% and bleeding, bleeds into it for 4 s. Once a knockout; after it, E feeds it as usual.
	const int32 Need = FMath::RoundToInt(SwcT(TEXT("ritual.bloodPoints"), 10.f));
	if (UWRBloodPath::BloodPoints(Who) < Need || Body->bRiteDone)
	{
		return false;
	}
	const bool bOut = Self->IsUnconscious();
	const bool bLow = Self->GetHealthFraction() < SwcT(TEXT("ritual.lowHealth"), 0.3f) && (Self->HasTag(WRTags::Status_Bleeding) || (Self->BuildupPct.IsValidIndex(WRStatus::Bleed) && Self->BuildupPct[WRStatus::Bleed] > 0));
	if (!bOut && !bLow)
	{
		return false;
	}
	if (Body->Ritualist.IsValid())
	{
		Message = Body->Ritualist.Get() == Who ? TEXT("Hold still at its mouth: the blood is going in") : TEXT("Someone is already bleeding into it");
		return true;
	}
	if (!UWRBloodPath::CanSpend(Who, SwcT(TEXT("ritual.stopBelowHealth"), 6.f) + SwcT(TEXT("ritual.hpPerSecond"), 5.f)))
	{
		// Too little blood to give: a knocked-out one is fed as usual instead (the caller's feeding goes on).
		if (bOut) return false;
		Message = TEXT("You haven't the blood to spare");
		return true;
	}
	const float Reach = SwcT(TEXT("ritual.reach"), 450.f);
	if (FVector::Dist(Who->GetActorLocation(), Self->MouthLocation()) > Reach)
	{
		Message = TEXT("Get to its mouth for the blood rite");
		return true;
	}
	Body->Ritualist = Who;
	Body->RiteState = 1;
	Body->RitualStarted = SwcNow(Self);
	Body->RitualTick = 0.f;
	Body->RitualFrom = Who->GetActorLocation();
	Body->RitualFxAt = 0.0;
	Self->SetAggroTarget(nullptr);
	WRSound::Broadcast(Self, TEXT("mire.rite"));
	UE_LOG(LogWildreach, Log, TEXT("SWAMP %s begins the blood rite on %s"), *Who->GetName(), *Self->GetName());
	Message = FString::Printf(TEXT("You open your wrist over its jaws: hold still %.0f s"), SwcT(TEXT("ritual.seconds"), 4.f));
	return true;
}

bool FWRSwampAI::RitePrompt(const AWRCreature* Self, const AActor* Who, FString& OutVerb)
{
	const UWRSwampBody* Body = UWRSwampBody::Get(Self);
	const FWRCreatureDef* Def = Self ? Self->GetDef() : nullptr;
	if (!Body || !Def || !Who || Self->IsTamed() || !Self->IsAlive() || !Def->CanTameBy(TEXT("blood_ritual")) || Body->RiteState == 2)
	{
		return false;
	}
	if (UWRBloodPath::BloodPoints(Who) < FMath::RoundToInt(SwcT(TEXT("ritual.bloodPoints"), 10.f)))
	{
		return false;
	}
	const bool bOut = Self->IsUnconscious();
	const bool bLow = Self->GetHealthFraction() < SwcT(TEXT("ritual.lowHealth"), 0.3f) && Self->HasTag(WRTags::Status_Bleeding);
	if (!bOut && !bLow)
	{
		return false;
	}
	if (Body->RiteState == 1)
	{
		OutVerb = TEXT("Blood rite under way: hold still");
		return true;
	}
	// Too little blood to give: a knocked-out one is fed as usual (the taming menu), an awake one says why on E.
	if (bOut && !UWRBloodPath::CanSpend(Who, SwcT(TEXT("ritual.stopBelowHealth"), 6.f) + SwcT(TEXT("ritual.hpPerSecond"), 5.f)))
	{
		return false;
	}
	OutVerb = FString::Printf(TEXT("Blood rite: bleed into its jaws (%.0f HP/s, %.0f s)"), SwcT(TEXT("ritual.hpPerSecond"), 5.f), SwcT(TEXT("ritual.seconds"), 4.f));
	return true;
}

bool FWRSwampAI::WithinWade(const AWRCreature* Self)
{
	const float Depth = Self ? SwcNum(SwcRow(Self->CreatureId), TEXT("wadeDepth"), 0.f) : 0.f;
	const UWRWaterSubsystem* Water = Depth > 0.f ? UWRWaterSubsystem::Get(Self) : nullptr;
	return Water && Water->DepthAt(Self->GetActorLocation()) <= Depth;
}

void FWRSwampAI::Signature(AWRCreature* Self)
{
	if (!Self || !Self->HasAuthority())
	{
		return;
	}
	// Its voice's signature on every machine (WRSound::Voice resolves it by its voice family: a young Murkjaw sounds its
	// grown kind pitched up, a swarm leech the Gorgeleech's: sounds.json creatures).
	WRSound::Broadcast(Self, TEXT("voice.signature"));
}

bool FWRSwampAI::Wade(const AWRCreature* Self, const FVector& Direction, FVector& Out)
{
	const float Depth = Self ? SwcNum(SwcRow(Self->CreatureId), TEXT("wadeDepth"), 0.f) : 0.f;
	const UWRWaterSubsystem* Water = Self ? UWRWaterSubsystem::Get(Self) : nullptr;
	if (Depth <= 0.f || !Water || Direction.IsNearlyZero())
	{
		return false;
	}
	// A wader walks the bottom of water up to its own depth (the Mudhulk 2.5 m, the Mirestrider 1.4 m), no deeper.
	const FVector Here = Self->GetActorLocation();
	const FVector Step = Direction.GetSafeNormal2D();
	const float There = Water->DepthAt(Here + Step * (Self->GetSimpleCollisionRadius() + 150.f));
	const float Now = Water->DepthAt(Here);
	if (There <= Depth || There < Now) Out = Direction;
	else if (Now <= Depth) Out = FVector::ZeroVector;
	else Out = Water->ShoreDirection(Self) * Direction.Size();
	return true;
}

// ---------------------------------------------------------------- the body's server life

void FWRSwampAI::SyncFlags(AWRCreature* Self, UWRSwampBody* Body)
{
	const double T = SwcNow(Self);
	const AWRCreatureAI* AI = Cast<AWRCreatureAI>(Self->GetController());
	const FName Row = AI && AI->RowPhase != 0 ? AI->RowId : NAME_None;
	const int32 Phase = AI ? AI->RowPhase : 0;
	const bool bWind = Phase == 1 || Phase == 2;
	Body->Set(WRSwampFlag::Pumping, Row == TEXT("tongue_lash") && bWind);
	Body->Set(WRSwampFlag::Tongue, (Row == TEXT("tongue_lash") && Phase == 3) || (Body->Pulled.IsValid() && T < Body->PullCheckAt));
	Body->Set(WRSwampFlag::Gape, T < Body->GapeUntil || (Row == TEXT("gulp") && bWind));
	Body->Set(WRSwampFlag::Disc, (Row == TEXT("latch") && Phase >= 1 && Phase <= 3) || Body->LatchedOn.IsValid());
	Body->Set(WRSwampFlag::Crying, Row == TEXT("curse_cry") && Phase >= 1 && Phase <= 3);
	Body->Set(WRSwampFlag::Lit, Row == TEXT("flash_puff") && Phase >= 1 && Phase <= 3);
	Body->Set(WRSwampFlag::Rolling, Body->Held.IsValid() && Body->HoldKind == TEXT("death_roll"));
	Body->Set(WRSwampFlag::Mouthful, Body->Held.IsValid() && Body->HoldKind == TEXT("gulp"));
	Body->Set(WRSwampFlag::Latched, Body->LatchedOn.IsValid());
	Body->Set(WRSwampFlag::Withdrawn, T < Body->WithdrawUntil);
	Body->Set(WRSwampFlag::Grounded, T < Body->GroundedUntil);
	Body->Set(WRSwampFlag::Ambush, Body->AmbushSince >= 0.0 && T - Body->AmbushSince < 0.5);
	Body->Set(WRSwampFlag::Stab, T < Body->StabUntil);
	Body->Set(WRSwampFlag::Hexed, Body->HexUntil > T);
	const bool bWallowRow = Row == TEXT("wallow_surge") && bWind;
	const bool bWallowIdle = SwcKind(Self->CreatureId) == TEXT("mudhulk") && !Self->IsTamed() && !Self->GetAggroTarget() && SwcDay(Self) && SwcInWater(Self) && Self->GetVelocity().Size2D() < 30.f;
	Body->Set(WRSwampFlag::Wallow, bWallowRow || bWallowIdle);
}

void FWRSwampAI::TickBody(AWRCreature* Self, UWRSwampBody* Body, float DeltaTime)
{
	const double T = SwcNow(Self);
	const FName K = SwcKind(Self->CreatureId);
	AWRCreatureAI* AI = Cast<AWRCreatureAI>(Self->GetController());

	// --- The holds: the death roll and the mouthful ---
	if (Body->Held.IsValid())
	{
		AWRCharacterBase* V = Body->Held.Get();
		const bool bRoll = Body->HoldKind == TEXT("death_roll");
		if (!V->IsAlive() || !Self->IsAlive() || Self->IsUnconscious() || Self->HasTag(WRTags::Status_StanceBroken))
		{
			Release(Self, Body, Self->IsAlive() && V->IsAlive());
		}
		else if (T >= Body->HoldUntil)
		{
			UE_LOG(LogWildreach, Log, TEXT("SWAMP %s lets go of %s"), *Self->GetName(), *V->GetName());
			Release(Self, Body, true);
		}
		else if (T >= Body->NextHoldTickAt)
		{
			const float Tick = SwcT(TEXT("hold.tickSeconds"), 0.5f);
			Body->NextHoldTickAt = T + Tick;
			const float Dps = bRoll ? SwcT(TEXT("murkjaw.rollDps"), 18.f) : SwcT(TEXT("bogstalker.gulpDps"), 15.f);
			SwcDot(Self, V, Dps * Tick * SwcDifficulty(), 0.f);
			V->Status->Stagger(float(Body->HoldUntil - T) + 0.3f);
			if (bRoll) WRSound::Broadcast(Self, TEXT("mire.roll_thrash"));
		}
	}

	// --- A leech on a body: drain, bleed, and what knocks it off ---
	if (Body->LatchedOn.IsValid())
	{
		AWRCharacterBase* V = Body->LatchedOn.Get();
		const bool bSwarm = Self->CreatureId == TEXT("latch_leech");
		if (!V->IsAlive() || !Self->IsAlive())
		{
			Unlatch(Self, Body, false);
		}
		else
		{
			const bool bDodging = V->HasTag(WRTags::State_Dodging);
			if (bDodging && !Body->bLatchRollSeen)
			{
				Body->bLatchRollSeen = true;
				if (FMath::FRand() < SwcT(TEXT("latch.rollOff"), 0.6f))
				{
					Unlatch(Self, Body, true);
					return;
				}
			}
			else if (!bDodging)
			{
				Body->bLatchRollSeen = false;
			}
			if (V->HasTag(WRTags::Status_Burn) || Self->HasTag(WRTags::Status_Burn))
			{
				Unlatch(Self, Body, true);   // fire on you or on it
				return;
			}
			Body->LatchTick += DeltaTime;
			const float Every = SwcT(TEXT("latch.tickSeconds"), 0.5f);
			if (Body->LatchTick >= Every)
			{
				const float Tick = Body->LatchTick;
				Body->LatchTick = 0.f;
				const TCHAR* G = bSwarm ? TEXT("latch_leech") : TEXT("latch");
				const float Drain = FWRTuning::Get().Num(FString::Printf(TEXT("swampCreatures.%s.drainPerSecond"), G), bSwarm ? 2.f : 6.f) * Tick * SwcDifficulty();
				const float Bleed = FWRTuning::Get().Num(FString::Printf(TEXT("swampCreatures.%s.bleedPerSecond"), G), bSwarm ? 8.f : 15.f) * Tick;
				const float Before = V->GetAttributes() ? V->GetAttributes()->GetHealth() : 0.f;
				SwcDot(Self, V, Drain, Bleed);
				const float Taken = FMath::Max(0.f, Before - (V->GetAttributes() ? V->GetAttributes()->GetHealth() : Before));
				// It heals twice what it drinks.
				if (UWRAttributeSet* A = Self->GetAttributes(); A && Taken > 0.f && Self->GetAbilitySystemComponent())
				{
					Self->GetAbilitySystemComponent()->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), FMath::Min(A->GetMaxHealth(), A->GetHealth() + Taken * SwcT(TEXT("latch.healMult"), 2.f)));
				}
			}
		}
	}

	// --- The tongue's pull: close enough, and the gulp comes ---
	if (Body->Pulled.IsValid() && T >= Body->PullCheckAt)
	{
		AWRCharacterBase* V = Body->Pulled.Get();
		if (!Body->Held.IsValid()) Body->Holding = nullptr;
		if (V->IsAlive() && FVector::Dist2D(V->GetActorLocation(), Self->MouthLocation()) <= SwcT(TEXT("bogstalker.gulpWithin"), 200.f) + V->GetSimpleCollisionRadius() + Self->GetSimpleCollisionRadius() * 0.3f)
		{
			Body->ForceNext = TEXT("gulp");
			Body->ForceUntil = T + 1.5;
			CutRow(Self, 0.f);
			if (AI) AI->BreatheUntil = 0.0;
			Body->PullCheckAt = T + 1.6;   // keeps Pulled for the gulp's target
		}
		else
		{
			Body->Pulled = nullptr;
		}
		if (T > Body->ForceUntil) Body->Pulled = nullptr;
	}

	// --- The blood rite ---
	if (Body->Ritualist.IsValid())
	{
		AWRCharacter* Who = Body->Ritualist.Get();
		const float Reach = SwcT(TEXT("ritual.reach"), 450.f);
		FString Broke;
		if (!Who->IsAlive() || !Self->IsAlive()) Broke = TEXT("The rite is broken");
		else if (FVector::Dist(Who->GetActorLocation(), Self->MouthLocation()) > Reach + 100.f || FVector::Dist2D(Who->GetActorLocation(), Body->RitualFrom) > SwcT(TEXT("ritual.moveTolerance"), 150.f)) Broke = TEXT("You moved: the rite is broken");
		else if (Who->IsSwinging() || Who->HasTag(WRTags::State_Dodging)) Broke = TEXT("The rite is broken");
		if (Broke.IsEmpty())
		{
			Self->SetAggroTarget(nullptr);
			Body->RitualTick += DeltaTime;
			if (Body->RitualTick >= 0.25f)
			{
				const float Pay = SwcT(TEXT("ritual.hpPerSecond"), 5.f) * Body->RitualTick;
				Body->RitualTick = 0.f;
				const UWRAttributeSet* A = Who->GetAttributes();
				if ((A && A->GetHealth() - Pay < SwcT(TEXT("ritual.stopBelowHealth"), 6.f)) || !UWRBloodPath::SpendBlood(Who, Pay))
				{
					Broke = TEXT("You haven't the blood left: the rite is broken");
				}
			}
			if (Broke.IsEmpty() && T >= Body->RitualFxAt)
			{
				// The garnet stream from the wrist into the jaws.
				Body->RitualFxAt = T + 0.5;
				const FVector From = Who->GetActorLocation() + FVector(0.f, 0.f, Who->GetSimpleCollisionHalfHeight() * 0.3f);
				const FVector To = Self->MouthLocation();
				WRFx::Broadcast(Self, TEXT("mire.bloodfeed"), From, (To - From).Rotation(), FMath::Max(0.3f, float(FVector::Dist(From, To)) / 100.f), NAME_None, 0.55f);
			}
			if (Broke.IsEmpty() && T - Body->RitualStarted >= SwcT(TEXT("ritual.seconds"), 4.f))
			{
				// A full meal of narcotic: under, and kept under; taming goes on as knockout-and-feed.
				Body->Ritualist = nullptr;
				Body->bRiteDone = true;
				Body->RiteState = 2;
				Self->AddTorpor(Self->MaxTorpor * 2.f);
				Self->Torpor = Self->MaxTorpor * SwcT(TEXT("ritual.torpor"), 1.f);
				Self->TameProgress = FMath::Min(0.99f, Self->TameProgress + SwcT(TEXT("ritual.mealProgress"), 0.1f));
				WRFx::Broadcast(Self, TEXT("blood.feast"), Self->MouthLocation(), FRotator::ZeroRotator, 1.f, TEXT("mire.rite_done"));
				SwcTell(Who, FString::Printf(TEXT("The %s drinks and goes under: feed it now"), *Self->GetDef()->Name));
				UE_LOG(LogWildreach, Log, TEXT("SWAMP blood rite done on %s by %s"), *Self->GetName(), *Who->GetName());
			}
		}
		if (!Broke.IsEmpty())
		{
			Body->Ritualist = nullptr;
			Body->RiteState = 0;
			SwcTell(Who, Broke);
		}
	}
	else if (Body->RiteState == 1)
	{
		Body->RiteState = 0;   // its ritualist is gone (left the game)
	}
	// A knockout ends with it waking: the rite can be done again next time.
	if (Body->bRiteDone && !Self->IsUnconscious() && !Self->IsTamed())
	{
		Body->bRiteDone = false;
		Body->RiteState = 0;
	}

	// --- Hexcrows: down after a dive until they're up; the murder called once a fight ---
	if (K == TEXT("hexcrow"))
	{
		UCharacterMovementComponent* Move = Self->GetCharacterMovement();
		if (!Body->bDiveLanded && !Move->IsFalling() && !Move->IsFlying())
		{
			Body->bDiveLanded = true;
			Body->GroundedSince = T;
			Body->GroundedUntil = T + SwcT(TEXT("hexcrow.groundedSeconds"), 1.5f);
			Move->SetMovementMode(MOVE_Walking);
			WRFx::Broadcast(Self, TEXT("mire.mud"), SwcFeet(Self), FRotator::ZeroRotator, 0.6f, NAME_None);
		}
		if (T < Body->GroundedUntil && Move->IsFlying()) Move->SetMovementMode(MOVE_Falling);
		if (!Self->GetAggroTarget()) Body->bMurderCalled = false;
	}
	// A hexed beast stays hexed while the aura holds.
	if (Body->HexUntil > 0.0 && T >= Body->HexUntil) Body->HexUntil = 0.0;
	// Not lying in wait any more: it floats as usual again.
	if (K == TEXT("murkjaw") && (Body->AmbushSince < 0.0 || T - Body->AmbushSince > 0.5) && Self->GetCharacterMovement()->Buoyancy != WRTune(TEXT("water.buoyancy"), 1.6f))
	{
		Body->AmbushSince = -1.0;
		Self->GetCharacterMovement()->Buoyancy = WRTune(TEXT("water.buoyancy"), 1.6f);
	}
	// The surge's wind-up: it travels under the water to where you stand.
	if (AI && AI->RowId == TEXT("wallow_surge") && (AI->RowPhase == 1 || AI->RowPhase == 2))
	{
		const FVector To = AI->RowAim - SwcFeet(Self);
		if (To.Size2D() > 80.f) Self->AddMovementInput(To.GetSafeNormal2D(), 0.9f);
	}
	// Swarm leeches out of the boil that found nobody go back into the water.
	if (Self->CreatureId == TEXT("latch_leech") && Body->bTideSpawned && !Body->LatchedOn.IsValid() && !Self->GetAggroTarget() && T - Body->LastAttackAt > SwcT(TEXT("latch_leech.lingerSeconds"), 30.f))
	{
		Self->Destroy();
		return;
	}

	// --- Tame uses ---
	if (Self->IsTamed() && !Self->IsBaby())
	{
		const TSharedPtr<FJsonObject> R = SwcRow(Self->CreatureId);
		const float FindRange = SwcNum(R, TEXT("findSites"), 0.f);
		if (FindRange > 0.f && Self->TameOrder == EWRTameOrder::Follow && T >= Body->NextFindAt)
		{
			// A Bogsnout roots out what the bog keeps: bog bodies, buried caches, the Coven's chimney, marked for its owner.
			Body->NextFindAt = T + SwcT(TEXT("bogsnout.findEvery"), 30.f);
			APlayerController* PC = SwcOwnerPC(Self->GetWorld(), Self->TamedBy);
			const AWRCharacter* Owner = SwcOwnerBody(Self->GetWorld(), Self->TamedBy);
			if (PC && Owner && FVector::Dist(Owner->GetActorLocation(), Self->GetActorLocation()) < 3000.f)
			{
				TSet<FName> Kinds;
				const TSharedPtr<FJsonObject> Bog = FWRTuning::Get().Object(TEXT("swampCreatures.bogsnout"));
				const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
				if (Bog.IsValid() && Bog->TryGetArrayField(TEXT("findKinds"), List)) for (const TSharedPtr<FJsonValue>& V : *List) Kinds.Add(FName(*V->AsString()));
				const FWRSwampData& D = FWRSwampData::Get();
				const float Mark = SwcT(TEXT("bogsnout.markSeconds"), 30.f);
				int32 Found = 0;
				const auto Consider = [&](const FWRSwampData::FSite& S)
				{
					if ((Kinds.Contains(S.Kind) || (Kinds.Contains(TEXT("cache")) && S.Loot.Num() > 0)) && FVector::Dist2D(S.At, Self->GetActorLocation()) <= FindRange)
					{
						AWRSwampMark::Spawn(PC, S.At + FVector(0.f, 0.f, 60.f), Mark);
						++Found;
					}
				};
				for (const FWRSwampData::FSite& S : D.Sites) Consider(S);
				for (const FWRSwampData::FSite& S : D.Kit) Consider(S);
				if (Kinds.Contains(TEXT("chimney")) && !D.Chimney.IsZero() && FVector::Dist2D(D.Chimney, Self->GetActorLocation()) <= FindRange)
				{
					AWRSwampMark::Spawn(PC, D.Chimney + FVector(0.f, 0.f, 60.f), Mark);
					++Found;
				}
				if (Found > 0)
				{
					WRSound::Broadcast(Self, TEXT("mire.snout_find"));
					Self->GainBondXP(WRTune(TEXT("tame.xp.dig"), 3.f), TEXT("find"));
					if (AWRPlayerController* WPC = Cast<AWRPlayerController>(PC)) WPC->ClientNotify(FString::Printf(TEXT("Your %s roots something out: %d place%s marked"), *Self->GetDef()->Name, Found, Found == 1 ? TEXT("") : TEXT("s")));
				}
			}
		}
		// A pouched toad whose owner is gone or told it to stay comes down.
		if (Body->Has(WRSwampFlag::Pouched) && (Self->TameOrder != EWRTameOrder::Follow || !SwcOwnerBody(Self->GetWorld(), Self->TamedBy)))
		{
			Self->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			Self->SetActorEnableCollision(true);
			Self->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
			Body->Set(WRSwampFlag::Pouched, false);
		}
	}
	SyncFlags(Self, Body);
}

void FWRSwampAI::CutRow(AWRCreature* Self, float After)
{
	AWRCreatureAI* AI = Self ? Cast<AWRCreatureAI>(Self->GetController()) : nullptr;
	if (AI && (AI->RowPhase == 3 || AI->RowPhase == 4))
	{
		AI->RowPhaseEnd = FMath::Min(AI->RowPhaseEnd, SwcNow(Self) + After);
	}
}

void FWRSwampAI::StartHold(AWRCreature* Self, UWRSwampBody* Body, AWRCharacterBase* Victim, FName Kind)
{
	if (!Self || !Body || !Victim || !Victim->IsAlive() || Body->Held.IsValid() || WRSwampGrip::IsHeld(Victim))
	{
		return;
	}
	const double T = SwcNow(Self);
	const bool bRoll = Kind == TEXT("death_roll");
	// In or by the water, the roll drags you under (2 s more).
	const bool bUnder = bRoll && (SwcInWater(Self) || SwcWaterNear(Self, Self->GetActorLocation(), SwcT(TEXT("murkjaw.edgeCm"), 300.f)));
	const float Seconds = (bRoll ? SwcT(TEXT("murkjaw.rollSeconds"), 2.f) : SwcT(TEXT("bogstalker.gulpSeconds"), 2.5f)) + (bUnder ? SwcT(TEXT("murkjaw.underSeconds"), 2.f) : 0.f);
	// Off a mount first.
	if (AWRCreature* Mount = Cast<AWRCreature>(Victim->GetAttachParentActor()); Mount && Mount->GetRider() == Victim)
	{
		Mount->Unhorse(Self);
	}
	Body->Held = Victim;
	Body->HoldKind = Kind;
	Body->HoldStarted = T;
	Body->HoldUntil = T + Seconds;
	Body->HoldTick = 0.f;
	Body->NextHoldTickAt = T + SwcT(TEXT("hold.tickSeconds"), 0.5f);
	Body->FreeDamage = 0.f;
	Body->bDragUnder = bUnder;
	Body->Holding = Victim;
	Body->Pulled = nullptr;
	Body->Set(bRoll ? WRSwampFlag::Rolling : WRSwampFlag::Mouthful, true);
	Victim->Status->Stagger(Seconds + 0.3f);
	Victim->GetCharacterMovement()->DisableMovement();
	Victim->SetActorEnableCollision(false);
	Victim->AttachToActor(Self, FAttachmentTransformRules::KeepWorldTransform);
	const float SelfHalf = Self->GetSimpleCollisionHalfHeight(), VicHalf = Victim->GetSimpleCollisionHalfHeight();
	if (bRoll)
	{
		// In the jaws, crosswise, at the snout; dragged down under the surface by the water.
		Victim->SetActorRelativeLocation(FVector(Self->GetSimpleCollisionRadius() + Victim->GetSimpleCollisionRadius() * 0.5f + 40.f, 0.f, -SelfHalf + VicHalf - (bUnder ? SwcT(TEXT("murkjaw.underDepth"), 120.f) : 0.f)));
		Victim->SetActorRelativeRotation(FRotator(0.f, 90.f, 0.f));
	}
	else
	{
		// Swallowed: inside the body, out of sight.
		Victim->SetActorRelativeLocation(FVector(Self->GetSimpleCollisionRadius() * 0.2f, 0.f, 0.f));
		Victim->SetActorHiddenInGame(true);
	}
	Self->SetAggroTarget(Victim);
	WRFx::Broadcast(Self, bRoll ? TEXT("mire.splash") : TEXT("mire.mud"), Self->MouthLocation(), FRotator::ZeroRotator, 1.f, bRoll ? TEXT("mire.roll_thrash") : TEXT("mire.gulp"));
	SwcTell(Victim, bRoll ? TEXT("Caught in its jaws: mash to break free") : TEXT("Swallowed: mash to get out"));
	UE_LOG(LogWildreach, Log, TEXT("SWAMP %s %s %s (%.1f s%s)"), *Self->GetName(), bRoll ? TEXT("death-rolls") : TEXT("gulps"), *Victim->GetName(), Seconds, bUnder ? TEXT(", dragged under") : TEXT(""));
}

void FWRSwampAI::Release(AWRCreature* Self, UWRSwampBody* Body, bool bThrow)
{
	if (!Self || !Body)
	{
		return;
	}
	AWRCharacterBase* V = Body->Held.Get();
	const bool bGulp = Body->HoldKind == TEXT("gulp");
	Body->Held = nullptr;
	Body->HoldKind = NAME_None;
	Body->Holding = nullptr;
	Body->bDragUnder = false;
	Body->Set(WRSwampFlag::Rolling, false);
	Body->Set(WRSwampFlag::Mouthful, false);
	if (!V)
	{
		return;
	}
	V->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	const FVector Fwd = Self->GetActorForwardVector().GetSafeNormal2D();
	FVector At = Self->GetActorLocation() + Fwd * (Self->GetSimpleCollisionRadius() + V->GetSimpleCollisionRadius() + 50.f);
	At.Z = FMath::Max(At.Z, SwcFeet(Self).Z + V->GetSimpleCollisionHalfHeight() + 20.f);
	V->SetActorLocation(At, false, nullptr, ETeleportType::TeleportPhysics);
	V->SetActorRelativeRotation(FRotator::ZeroRotator);
	V->SetActorRotation(FRotator(0.f, (Self->GetActorLocation() - At).Rotation().Yaw, 0.f));
	V->SetActorHiddenInGame(false);
	V->SetActorEnableCollision(true);
	V->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	V->Status->ClearStatus(WRTags::Status_Staggered);
	if (bThrow && V->IsAlive())
	{
		// Spat out or flung off: knocked down where it lands.
		V->LaunchCharacter(Fwd * SwcT(TEXT("hold.throwPush"), 450.f) + FVector(0.f, 0.f, 350.f), true, true);
		V->Status->Stagger(SwcT(TEXT("bogstalker.spitOutStagger"), 1.2f));
	}
	if (bGulp) WRFx::Broadcast(Self, TEXT("mire.mud"), Self->MouthLocation(), FRotator::ZeroRotator, 1.f, TEXT("mire.spit"));
}

bool FWRSwampAI::StartLatch(AWRCreature* Self, UWRSwampBody* Body, AWRCharacterBase* Victim)
{
	if (!Self || !Body || !Victim || !Victim->IsAlive() || Body->LatchedOn.IsValid() || WRSwampGrip::IsHeld(Victim))
	{
		return false;
	}
	const bool bSwarm = Self->CreatureId == TEXT("latch_leech");
	const int32 Max = FMath::RoundToInt(FWRTuning::Get().Num(bSwarm ? TEXT("swampCreatures.latch_leech.maxPerVictim") : TEXT("swampCreatures.latch.maxPerVictim"), bSwarm ? 4.f : 3.f));
	TSet<int32> Used;
	for (const TWeakObjectPtr<UWRSwampBody>& B : UWRSwampBody::Live)
	{
		if (B.IsValid() && B->LatchedOn.Get() == Victim) Used.Add(B->LatchSlot);
	}
	// Max on one body at once, one a limb (the swarm also finds the back).
	if (Used.Num() >= Max)
	{
		return false;
	}
	int32 Slot = -1;
	for (int32 s = 0; s < (bSwarm ? 8 : 4) && Slot < 0; ++s) if (!Used.Contains(s)) Slot = s;
	if (Slot < 0)
	{
		return false;
	}
	const double T = SwcNow(Self);
	Body->LatchedOn = Victim;
	Body->LatchSlot = Slot;
	Body->LatchBone = SwcLimbName(Slot);
	Body->LatchTick = 0.f;
	Body->bLatchRollSeen = Victim->HasTag(WRTags::State_Dodging);
	Body->LatchedAt = T;
	Body->Holding = Victim;
	Body->Set(WRSwampFlag::Latched, true);
	Self->GetCharacterMovement()->DisableMovement();
	Self->AttachToActor(Victim, FAttachmentTransformRules::KeepWorldTransform);
	const FVector Off = SwcLimbOffset(Victim, Slot);
	Self->SetActorRelativeLocation(Off);
	Self->SetActorRelativeRotation(FRotator(0.f, (-Off * FVector(1.f, 1.f, 0.f)).Rotation().Yaw, 0.f));
	if (bSwarm) Self->SetLifeSpan(0.f);
	WRSound::Broadcast(Self, TEXT("mire.leech_suck"));
	UE_LOG(LogWildreach, Log, TEXT("SWAMP %s latches onto %s (%s)"), *Self->GetName(), *Victim->GetName(), *Body->LatchBone.ToString());
	return true;
}

void FWRSwampAI::Unlatch(AWRCreature* Self, UWRSwampBody* Body, bool bKnocked)
{
	if (!Self || !Body)
	{
		return;
	}
	AWRCharacterBase* V = Body->LatchedOn.Get();
	Body->LatchedOn = nullptr;
	Body->LatchSlot = -1;
	Body->LatchBone = NAME_None;
	Body->Holding = nullptr;
	Body->Set(WRSwampFlag::Latched, false);
	Body->Set(WRSwampFlag::Disc, false);
	Body->LastAttackAt = SwcNow(Self);
	Self->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	const FVector Away = V ? (Self->GetActorLocation() - V->GetActorLocation()).GetSafeNormal2D() : -Self->GetActorForwardVector();
	if (V) Self->SetActorLocation(Self->GetActorLocation() + Away * 30.f, false, nullptr, ETeleportType::TeleportPhysics);
	Self->SetActorRotation(FRotator(0.f, (-Away).Rotation().Yaw, 0.f));
	Self->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	if (bKnocked && Self->IsAlive())
	{
		Self->LaunchCharacter(Away * SwcT(TEXT("latch.knockOffPush"), 350.f) + FVector(0.f, 0.f, 200.f), true, true);
		Self->Status->Stagger(SwcT(TEXT("latch.dropStagger"), 0.6f));
		WRFx::Broadcast(Self, TEXT("blood.hit"), Self->GetActorLocation(), FRotator::ZeroRotator, 0.5f, TEXT("mire.leech_pop"));
	}
	if (Self->CreatureId == TEXT("latch_leech") && Body->bTideSpawned) Self->SetLifeSpan(SwcT(TEXT("latch_leech.lingerSeconds"), 30.f));
}

void FWRSwampAI::Rally(AWRCreature* Self, AActor* By)
{
	// The bill-clatter like gunfire: every Mirestrider within 30 m turns on the attacker together.
	if (!Self || !By)
	{
		return;
	}
	const float Radius = SwcT(TEXT("mirestrider.clatterRadius"), 3000.f);
	int32 Turned = 0;
	for (TActorIterator<AWRCreature> It(Self->GetWorld()); It; ++It)
	{
		if (!SwcWild(*It) || SwcKind(It->CreatureId) != SwcKind(Self->CreatureId) || FVector::Dist(It->GetActorLocation(), Self->GetActorLocation()) > Radius) continue;
		It->SetAsleep(false);
		if (!It->GetAggroTarget()) { It->SetAggroTarget(By); ++Turned; }
		if (AWRCreature* Wild = Cast<AWRCreature>(By); Wild && !Wild->IsTamed()) It->HuntTarget = Wild;
	}
	if (UWRSwampBody* Body = UWRSwampBody::Get(Self)) Body->LastClatterAt = SwcNow(Self);
	UE_LOG(LogWildreach, Log, TEXT("SWAMP %s clatters: %d turn"), *Self->GetName(), Turned);
}

void FWRSwampAI::ModifyHit(const AWRCreature* Self, FWRHit& Hit, const FWRAttackRow* Row)
{
	const UWRSwampBody* Body = UWRSwampBody::Get(Self);
	if (!Body)
	{
		return;
	}
	// A hexed beast's blows carry the hex (curse 25).
	if (Body->IsHexed())
	{
		Hit.Buildups.FindOrAdd(TEXT("curse")) += SwcT(TEXT("hex.curse"), 25.f);
	}
	// The Miasma makes the cry worse.
	if (Row && Row->Id == TEXT("curse_cry") && WRSwamp::CurseCryMult(Self) > 1.f)
	{
		if (float* Curse = Hit.Buildups.Find(TEXT("curse"))) *Curse *= SwcT(TEXT("hexcrow.miasmaCurseMult"), 1.5f);
	}
}

void FWRSwampAI::SetHexed(AActor* Creature, float Seconds)
{
	UWRSwampBody* Body = UWRSwampBody::Get(Creature);
	if (!Body || !Creature->HasAuthority())
	{
		return;
	}
	const double T = SwcNow(Creature);
	Body->HexUntil = Seconds > 0.f ? FMath::Max(Body->HexUntil, T + Seconds) : 0.0;
	Body->SetHexed(Body->HexUntil > T);
}

int32 FWRSwampAI::SpawnLeechSwarm(AWRCharacterBase* Onto, int32 Count)
{
	if (!Onto || !Onto->HasAuthority() || !Onto->IsAlive() || !FWRCreatureRegistry::Get().Find(TEXT("latch_leech")))
	{
		return 0;
	}
	int32 Came = 0;
	const FVector Feet = SwcFeet(Onto);
	for (int32 i = 0; i < Count; ++i)
	{
		const FVector Off = FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f).Vector() * FMath::FRandRange(60.f, 140.f);
		bool bOk = false;
		FVector At = SwcGround(Onto->GetWorld(), Feet + Off, bOk, Onto);
		At.Z = FMath::Max(At.Z, Feet.Z - 50.f) + 20.f;
		AWRCreature* Leech = AWRCreature::SpawnCreature(Onto->GetWorld(), TEXT("latch_leech"), At, FRotator(0.f, (-Off).Rotation().Yaw, 0.f));
		if (!Leech) continue;
		Leech->SetAggroTarget(Onto);
		Leech->SetLifeSpan(SwcT(TEXT("latch_leech.lingerSeconds"), 30.f) * 2.f);
		if (UWRSwampBody* B = UWRSwampBody::Get(Leech))
		{
			B->bTideSpawned = true;
			B->LastAttackAt = SwcNow(Leech);
		}
		WRFx::Broadcast(Leech, TEXT("mire.bubbles"), At, FRotator::ZeroRotator, 0.5f, NAME_None);
		++Came;
	}
	return Came;
}

// ---------------------------------------------------------------- the grips as the rest of the game sees them

bool WRSwampGrip::IsHeld(const AActor* Who)
{
	if (!Who) return false;
	for (const TWeakObjectPtr<UWRSwampBody>& B : UWRSwampBody::Live)
	{
		if (B.IsValid() && B->Holding == Who && (B->Has(WRSwampFlag::Rolling) || B->Has(WRSwampFlag::Mouthful))) return true;
	}
	return false;
}

void WRSwampGrip::Mash(AActor* Who)
{
	if (!Who || !Who->HasAuthority()) return;
	for (const TWeakObjectPtr<UWRSwampBody>& B : UWRSwampBody::Live)
	{
		if (!B.IsValid() || B->Held.Get() != Who) continue;
		const bool bGulp = B->HoldKind == TEXT("gulp");
		const float Off = bGulp ? SwcT(TEXT("bogstalker.mashSeconds"), 0.15f) : SwcT(TEXT("murkjaw.mashSeconds"), 0.15f);
		const float Min = bGulp ? SwcT(TEXT("bogstalker.mashMinSeconds"), 0.6f) : SwcT(TEXT("murkjaw.mashMinSeconds"), 0.6f);
		B->HoldUntil = FMath::Max(B->HoldStarted + Min, B->HoldUntil - Off);
	}
}

float WRSwampGrip::MoveScale(const AActor* Who)
{
	// Leeches on you: you can still act, at x0.85 (once, however many).
	if (!Who) return 1.f;
	for (const TWeakObjectPtr<UWRSwampBody>& B : UWRSwampBody::Live)
	{
		if (B.IsValid() && B->Holding == Who && B->Has(WRSwampFlag::Latched)) return SwcT(TEXT("latch.moveMult"), 0.85f);
	}
	return 1.f;
}

// ---------------------------------------------------------------- owner-only marks

AWRSwampMark::AWRSwampMark()
{
	bReplicates = true;
	bOnlyRelevantToOwner = true;
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

AWRSwampMark* AWRSwampMark::Spawn(APlayerController* For, const FVector& At, float Seconds)
{
	if (!For)
	{
		return nullptr;
	}
	// One mark per place: a fresh find renews it.
	for (TActorIterator<AWRSwampMark> It(For->GetWorld()); It; ++It)
	{
		if (It->GetOwner() == For && FVector::Dist(It->GetActorLocation(), At) < 200.f)
		{
			It->SetLifeSpan(Seconds);
			return *It;
		}
	}
	FActorSpawnParameters Params;
	Params.Owner = For;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWRSwampMark* Mark = For->GetWorld()->SpawnActor<AWRSwampMark>(AWRSwampMark::StaticClass(), At, FRotator::ZeroRotator, Params);
	if (Mark)
	{
		Mark->SetLifeSpan(Seconds);
	}
	return Mark;
}

void AWRSwampMark::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	// A slow pale-green ring over the ground only its owner sees.
	Fx = WRFx::Attach(RootComponent, TEXT("mire.mark"), NAME_None, 1.f);
	WRSound::PlayUI(this, TEXT("discover"));
}

// ---------------------------------------------------------------- the land's living world

bool UWRSwampLife::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UWRSwampLife::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (InWorld.GetNetMode() == NM_Client)
	{
		return;
	}
	SaltHandle = WRSwamp::OnSalted().AddUObject(this, &UWRSwampLife::OnSalted);
	InWorld.GetTimerManager().SetTimer(StepTimer, FTimerDelegate::CreateUObject(this, &UWRSwampLife::Step), FMath::Max(0.5f, SwcT(TEXT("life.stepSeconds"), 2.f)), true);
}

void UWRSwampLife::Deinitialize()
{
	if (SaltHandle.IsValid())
	{
		WRSwamp::OnSalted().Remove(SaltHandle);
		SaltHandle.Reset();
	}
	Super::Deinitialize();
}

void UWRSwampLife::OnSalted(AWRCharacterBase* Target, AActor* Instigator, const FVector& At)
{
	if (!Target || Target->GetWorld() != GetWorld())
	{
		return;
	}
	// Salt on a leech's back makes it let go; salt on you sends every leech on you off.
	if (UWRSwampBody* B = UWRSwampBody::Get(Target); B && B->LatchedOn.IsValid())
	{
		FWRSwampAI::Unlatch(Cast<AWRCreature>(Target), B, true);
	}
	UWRSwampBody::DetachLatches(Target, true);
	(void)Instigator; (void)At;
}

void UWRSwampLife::TrackSpit(AWRProjectile* Glob, AWRCreature* By)
{
	if (!Glob)
	{
		return;
	}
	Spits.Add(Glob, By);
	Glob->OnDestroyed.AddDynamic(this, &UWRSwampLife::OnSpitGone);
}

void UWRSwampLife::OnSpitGone(AActor* Glob)
{
	// Where the mud came down: whoever it struck is slowed (3 s), and it splats there.
	TWeakObjectPtr<AWRCreature> By;
	Spits.RemoveAndCopyValue(Glob, By);
	UWorld* World = GetWorld();
	if (!Glob || !World || World->bIsTearingDown)
	{
		return;
	}
	const FVector At = Glob->GetActorLocation();
	WRFx::Broadcast(World, TEXT("mire.mud"), At, FRotator::ZeroRotator, 0.8f, TEXT("mire.spit_splat"));
	for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
	{
		if (!It->IsAlive() || (By.IsValid() && (*It == By.Get() || !By->IsHostileTo(*It)))) continue;
		if (FVector::Dist(It->GetActorLocation(), At) < It->GetSimpleCollisionRadius() + It->GetSimpleCollisionHalfHeight() + 40.f)
		{
			It->Status->ApplyStatus(WRTags::Status_Slowed, SwcT(TEXT("bogstalker.spitSlowSeconds"), 3.f));
		}
	}
}

void UWRSwampLife::Step()
{
	UWorld* World = GetWorld();
	if (!World || !WRSwamp::IsLive())
	{
		return;
	}
	const float Hour = AWRGameState::HourIn(World);
	BeatLeeches(Hour);
	BeatMurder();
	BeatEdge(Hour);
	BeatHerds(Hour);
	BeatTide();
}

namespace WRSwampCreaturesLocal
{
	bool SwcInHours(float Hour, const TCHAR* From, const TCHAR* To, float F, float T)
	{
		return Hour >= SwcT(From, F) && Hour <= SwcT(To, T);
	}

	bool SwcPlayerWithin(const UWorld* World, const FVector& At, float Range)
	{
		for (TActorIterator<AWRCharacter> It(World); It; ++It) if (It->IsAlive() && FVector::Dist2D(It->GetActorLocation(), At) < Range) return true;
		return false;
	}

	bool SwcIdle(const AWRCreature* C)
	{
		return SwcWild(C) && !C->GetAggroTarget() && !C->IsFleeing() && !C->HuntTarget.IsValid() && !C->IsAsleep() && !C->IsA<AWRBoss>();
	}
}

void UWRSwampLife::BeatLeeches(float Hour)
{
	// A Gorgeleech pack goes for a Mudhulk and latches on to drink; the herd charges in to defend it.
	UWorld* World = GetWorld();
	const double T = World->GetTimeSeconds();
	if (T < NextLeechAt)
	{
		return;
	}
	NextLeechAt = T + SwcRand(TEXT("life"), TEXT("leechOnHulkEvery"), 120.f, 300.f);
	const float Watch = SwcT(TEXT("life.watchRadius"), 30000.f);
	for (TActorIterator<AWRCreature> Hulk(World); Hulk; ++Hulk)
	{
		if (!SwcIdle(*Hulk) || SwcKind(Hulk->CreatureId) != TEXT("mudhulk") || !WRSwamp::Contains(Hulk->GetActorLocation()) || !SwcPlayerWithin(World, Hulk->GetActorLocation(), Watch)) continue;
		int32 Sent = 0;
		for (TActorIterator<AWRCreature> Leech(World); Leech && Sent < 3; ++Leech)
		{
			if (!SwcIdle(*Leech) || Leech->CreatureId != TEXT("gorgeleech") || FVector::Dist(Leech->GetActorLocation(), Hulk->GetActorLocation()) > SwcT(TEXT("life.leechReach"), 5000.f)) continue;
			Leech->HuntTarget = *Hulk;
			Leech->SetAggroTarget(*Hulk);
			++Sent;
		}
		if (Sent > 0)
		{
			UE_LOG(LogWildreach, Log, TEXT("SWAMP %d Gorgeleeches go for %s"), Sent, *Hulk->GetName());
			return;
		}
	}
	(void)Hour;
}

void UWRSwampLife::BeatMurder()
{
	// Hexcrows circle whatever is dying (under 40%) and come down on carcasses: a sign seen from 400 m.
	UWorld* World = GetWorld();
	const double T = World->GetTimeSeconds();
	if (!SwcDay(World))
	{
		return;
	}
	TArray<AWRCharacterBase*> Marks;
	const float Below = SwcT(TEXT("hexcrow.circleBelow"), 0.4f);
	for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
	{
		if (!WRSwamp::Contains(It->GetActorLocation()) || It->IsHidden()) continue;
		const AWRCreature* Cr = Cast<AWRCreature>(*It);
		if (Cr && SwcKind(Cr->CreatureId) == TEXT("hexcrow")) continue;
		if (It->IsAlive() ? SwcHealthFrac(*It) < Below : (Cr && Cr->IsHarvestableBody())) Marks.Add(*It);
	}
	if (Marks.Num() == 0)
	{
		return;
	}
	const float See = SwcT(TEXT("hexcrow.seeRadius"), 40000.f);
	for (TActorIterator<AWRCreature> Crow(World); Crow; ++Crow)
	{
		UWRSwampBody* B = UWRSwampBody::Get(*Crow);
		if (!B || !SwcIdle(*Crow) || SwcKind(Crow->CreatureId) != TEXT("hexcrow") || B->CircleOver.IsValid() || B->Has(WRSwampFlag::Grounded)) continue;
		AWRCharacterBase* Best = nullptr;
		float BestD = See;
		for (AWRCharacterBase* M : Marks)
		{
			const float D = FVector::Dist2D(M->GetActorLocation(), Crow->GetActorLocation());
			if (D < BestD) { BestD = D; Best = M; }
		}
		if (!Best) continue;
		B->CircleOver = Best;
		B->CircleSince = T;
		B->BeatUntil = T + SwcT(TEXT("hexcrow.carcassSeconds"), 90.f);
	}
}

void UWRSwampLife::BeatEdge(float Hour)
{
	// At dawn the Murkjaws take Reedfowl and Mirestriders at the water's edge.
	UWorld* World = GetWorld();
	const double T = World->GetTimeSeconds();
	if (T < NextEdgeAt || !SwcInHours(Hour, TEXT("life.dawnFrom"), TEXT("life.dawnTo"), 5.f, 7.5f))
	{
		return;
	}
	NextEdgeAt = T + SwcRand(TEXT("life"), TEXT("edgeTakeEvery"), 60.f, 160.f);
	const float Range = SwcT(TEXT("life.edgeRadius"), 3000.f);
	for (TActorIterator<AWRCreature> Jaw(World); Jaw; ++Jaw)
	{
		if (!SwcIdle(*Jaw) || Jaw->CreatureId != TEXT("murkjaw") || !SwcPlayerWithin(World, Jaw->GetActorLocation(), SwcT(TEXT("life.watchRadius"), 30000.f))) continue;
		for (TActorIterator<AWRCreature> Prey(World); Prey; ++Prey)
		{
			const FName PK = SwcKind(Prey->CreatureId);
			if (!SwcIdle(*Prey) || (PK != TEXT("reedfowl") && PK != TEXT("mirestrider")) || FVector::Dist(Prey->GetActorLocation(), Jaw->GetActorLocation()) > Range) continue;
			if (!SwcWaterNear(World, Prey->GetActorLocation(), 300.f)) continue;
			Jaw->HuntTarget = *Prey;
			Jaw->SetAggroTarget(*Prey);
			UE_LOG(LogWildreach, Log, TEXT("SWAMP %s takes %s at the water's edge"), *Jaw->GetName(), *Prey->GetName());
			return;
		}
	}
}

void UWRSwampLife::BeatHerds(float Hour)
{
	// Mudhulks crowd the Salt Lick at dawn and wallow in the pools by day.
	UWorld* World = GetWorld();
	const double T = World->GetTimeSeconds();
	const bool bDawn = SwcInHours(Hour, TEXT("life.dawnFrom"), TEXT("life.dawnTo"), 5.f, 7.5f);
	const bool bDay = SwcInHours(Hour, TEXT("life.dayFrom"), TEXT("life.dayTo"), 9.f, 17.f);
	if (!bDawn && !bDay)
	{
		return;
	}
	const FWRSwampData::FSite* Lick = FWRSwampData::Get().FindSite(TEXT("salt_lick"));
	for (TActorIterator<AWRCreature> It(World); It; ++It)
	{
		UWRSwampBody* B = UWRSwampBody::Get(*It);
		if (!B || !SwcIdle(*It) || SwcKind(It->CreatureId) != TEXT("mudhulk") || T < B->NextBeatAt) continue;
		B->NextBeatAt = T + 300.0;
		if (bDawn && Lick && FVector::Dist2D(Lick->At, It->GetActorLocation()) < SwcT(TEXT("life.saltLickRadius"), 8000.f))
		{
			It->SetHome(Lick->At + FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f).Vector() * FMath::FRandRange(200.f, 700.f));
		}
		else if (bDay && !SwcInWater(*It))
		{
			const FVector Pool = SwcNearestWater(World, It->GetActorLocation(), SwcT(TEXT("life.wallowRadius"), 4000.f));
			if (!Pool.IsZero()) It->SetHome(Pool);
		}
	}
}

void UWRSwampLife::BeatTide()
{
	// The Red Tide: leeches x3 (redTide.leechSpawn) round anyone in the bleed zones (M3-M5), out of the bleedwater.
	UWorld* World = GetWorld();
	const double T = World->GetTimeSeconds();
	TideLeeches.RemoveAll([](const TWeakObjectPtr<AWRCreature>& L) { return !L.IsValid() || !L->IsAlive(); });
	if (!WRSwamp::IsTide(World))
	{
		// After it: the tide's extra leeches go back into the water once nobody is near.
		for (const TWeakObjectPtr<AWRCreature>& L : TideLeeches)
		{
			if (L.IsValid() && !L->GetAggroTarget() && !SwcPlayerWithin(World, L->GetActorLocation(), 6000.f)) L->Destroy();
		}
		return;
	}
	if (T < NextTideAt || !FWRCreatureRegistry::Get().Find(TEXT("gorgeleech")))
	{
		return;
	}
	NextTideAt = T + SwcT(TEXT("tide.checkSeconds"), 10.f);
	const int32 Want = FMath::RoundToInt(SwcT(TEXT("tide.leechesNear"), 2.f) * WRSwamp::SpawnMultiplier(World, TEXT("gorgeleech")));
	const float Near = SwcT(TEXT("tide.nearRadius"), 5000.f);
	for (TActorIterator<AWRCharacter> P(World); P; ++P)
	{
		if (!P->IsAlive() || !WRSwamp::Contains(P->GetActorLocation())) continue;
		const TCHAR Zone = FWRSwampData::Get().ZoneAt(P->GetActorLocation());
		if (Zone != TEXT('3') && Zone != TEXT('4') && Zone != TEXT('5')) continue;
		int32 Have = 0;
		for (TActorIterator<AWRCreature> L(World); L; ++L) Have += L->IsAlive() && L->CreatureId == TEXT("gorgeleech") && FVector::Dist(L->GetActorLocation(), P->GetActorLocation()) < Near;
		if (Have >= Want) continue;
		// Up out of the bleedwater 30 to 45 m off.
		for (int32 Try = 0; Try < 10; ++Try)
		{
			const FVector At = P->GetActorLocation() + FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f).Vector() * FMath::FRandRange(3000.f, 4500.f);
			if (!WRSwamp::Contains(At) || WRSwamp::WaterAt(World, At) != EWRSwampWater::Bleedwater) continue;
			bool bOk = false;
			const FVector G = SwcGround(World, At, bOk);
			if (!bOk) continue;
			const int32 Pack = FMath::Min(Want - Have, FMath::RandRange(2, 4));
			for (int32 i = 0; i < Pack; ++i)
			{
				const FVector Spot = G + FVector(FMath::RandPointInCircle(250.f), 60.f);
				if (AWRCreature* Leech = AWRCreature::SpawnCreature(World, TEXT("gorgeleech"), Spot, FRotator(0.f, (P->GetActorLocation() - Spot).Rotation().Yaw, 0.f)))
				{
					TideLeeches.Add(Leech);
					if (UWRSwampBody* B = UWRSwampBody::Get(Leech)) B->bTideSpawned = true;
				}
			}
			UE_LOG(LogWildreach, Log, TEXT("SWAMP the Red Tide brings %d Gorgeleeches up near %s"), Pack, *P->GetName());
			break;
		}
	}
}
