// The Graveyard's boss arenas' pieces and the bosses' code-built looks (see WRGraveBosses.h; cooks/biome-graveyard.md
// 2.13-2.14, build-biome-graveyard.md 2.6, 2.15, 2.16). Every machine draws from what replicates: a piece's kind and
// state, the bell's last toll, the chandelier's fall, the Procession's turns; a body's pose, count and veil.
//
// Machines move on a believable mechanism and never snap (build cook 2.6): the bell turns on its headstock with its wheel
// on the same axle (a full revolution a toll, eased in and out, then a dying sway), the rope runs off the wheel's groove
// through the guide in the belfry floor and down the shaft, its sally rising and falling with the wheel; the chandelier
// sways on its chain as a pendulum of its length and, cut, falls on gravity from the time it was cut.

#include "WRGraveBosses.h"
#include "Wildreach.h"
#include "WRAttributeSet.h"
#include "WRBoss.h"
#include "WRBuildLook.h"
#include "WRBuildLookKit.h"
#include "WRCharacter.h"
#include "WRCombatLibrary.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRGraveTypes.h"
#include "WRPlayerController.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRToolMesh.h"
#include "WRTuning.h"
#include "AbilitySystemComponent.h"
#include "Dom/JsonObject.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"

namespace WRGraveBossLookImpl
{
	using GlSurf = WRLookKit::FSurf;
	using GlKit = WRLookKit::FKit;

	FLinearColor GlRgb(uint8 R, uint8 G, uint8 B) { return FLinearColor(FColor(R, G, B)); }
	GlSurf GlStone(float K = 1.f) { return GlSurf::Stone(GlRgb(138, 133, 128) * 0.55f * K, 0.9f); }
	GlSurf GlLimestone(float K = 1.f) { return GlSurf::Stone(GlRgb(180, 175, 164) * 0.58f * K, 0.9f); }
	GlSurf GlBone(float K = 1.f) { return GlSurf::Stone(GlRgb(217, 207, 184) * 0.66f * K, 0.78f); }
	GlSurf GlBronze(float K = 1.f) { return GlSurf::Steel(GlRgb(122, 90, 52) * 0.55f * K, 0.45f); }
	GlSurf GlVerdigris(float K = 1.f) { return GlSurf::Steel(GlRgb(78, 140, 122) * 0.5f * K, 0.62f); }
	GlSurf GlIron(float K = 1.f) { return GlSurf::Steel(FLinearColor(0.03f, 0.028f, 0.027f) * K, 0.7f); }
	GlSurf GlRust(float K = 1.f) { return GlSurf::Steel(GlRgb(98, 52, 30) * 0.45f * K, 0.85f); }
	GlSurf GlOak(float K = 1.f) { return GlSurf::Wood(GlRgb(84, 68, 54) * 0.45f * K); }
	GlSurf GlYew(float K = 1.f) { return GlSurf::Wood(GlRgb(30, 26, 24) * 0.6f * K); }
	GlSurf GlSoil(float K = 1.f) { return GlSurf::Cloth(GlRgb(59, 53, 48) * 0.5f * K); }
	GlSurf GlTurf(float K = 1.f) { return GlSurf::Cloth(GlRgb(110, 122, 94) * 0.45f * K); }
	GlSurf GlLinen(float K = 1.f) { return GlSurf::Cloth(GlRgb(214, 208, 192) * 0.5f * K); }
	GlSurf GlOilskin(float K = 1.f) { return GlSurf::Glaze(GlRgb(40, 38, 32) * 0.5f * K, 0.4f); }
	GlSurf GlSack(float K = 1.f) { return GlSurf::Cloth(GlRgb(150, 132, 104) * 0.45f * K); }
	GlSurf GlSilk(float K = 1.f) { return GlSurf::Cloth(GlRgb(222, 216, 200) * 0.55f * K); }
	GlSurf GlChar(float K = 1.f) { return GlSurf::Cloth(GlRgb(30, 26, 24) * 0.55f * K); }
	GlSurf GlMoss(float K = 1.f) { return GlSurf::Cloth(GlRgb(79, 91, 58) * 0.48f * K); }
	GlSurf GlBlossom(float Glow = 0.f) { return Glow > 0.f ? GlSurf::Lit(GlRgb(240, 236, 226) * 0.6f, Glow) : GlSurf::Cloth(GlRgb(240, 236, 226) * 0.62f); }
	GlSurf GlGrave(float Glow) { return GlSurf::Lit(GlRgb(159, 227, 192) * 0.6f, Glow); }
	GlSurf GlCandle(float Glow) { return GlSurf::Lit(GlRgb(255, 200, 112) * 0.6f, Glow); }
	GlSurf GlSoulstone(float Glow) { return GlSurf::Glass(GlRgb(120, 200, 170) * 0.45f, Glow); }
	GlSurf GlBlack() { return GlSurf::Stone(FLinearColor(0.015f, 0.014f, 0.014f), 0.95f); }
	GlSurf GlMoth(float K = 1.f) { return GlSurf::Cloth(GlRgb(92, 80, 70) * 0.45f * K); }
	GlSurf GlBruise(float Glow = 0.f) { return Glow > 0.f ? GlSurf::Lit(GlRgb(90, 58, 110) * 0.55f, Glow) : GlSurf::Cloth(GlRgb(90, 58, 110) * 0.5f); }
	FLinearColor GlRootC() { return GlRgb(92, 76, 58) * 0.5f; }
	FLinearColor GlRopeC() { return GlRgb(156, 132, 96) * 0.5f; }
	FLinearColor GlBoneC() { return GlRgb(217, 207, 184) * 0.66f; }
	FLinearColor GlGraveC() { return GlRgb(159, 227, 192); }
	FLinearColor GlCandleC() { return GlRgb(255, 200, 112); }

	float GlNow(const UObject* W)
	{
		const UWorld* World = W ? W->GetWorld() : nullptr;
		const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
		return GS ? float(GS->GetServerWorldTimeSeconds()) : (World ? World->GetTimeSeconds() : 0.f);
	}
	float GlT(const TCHAR* Boss, const TCHAR* Key, float Fallback) { return FWRTuning::Get().Num(FString::Printf(TEXT("bosses.%s.%s"), Boss, Key), Fallback); }
	float GlHash(int32 A, int32 B)
	{
		uint32 H = uint32(A) * 73856093u ^ uint32(B) * 19349663u;
		H ^= H >> 13; H *= 0x5bd1e995u; H ^= H >> 15;
		return float(H & 0xFFFFFF) / float(0xFFFFFF);
	}
	bool GlGround(const UWorld* World, const FVector& At, FVector& Out, float Up, float Down, const AActor* Ignore)
	{
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRGraveBossLookGround), false, Ignore);
		if (World && World->LineTraceSingleByChannel(Hit, At + FVector(0.f, 0.f, Up), At - FVector(0.f, 0.f, Down), ECC_WorldStatic, Q) && !Cast<APawn>(Hit.GetActor()))
		{
			Out = Hit.ImpactPoint;
			return true;
		}
		Out = At;
		return false;
	}
	/** A hit shape a swing finds (overlaps pawns: the melee sweep) and an arrow stops on; it never blocks a body. */
	void GlHitShape(UPrimitiveComponent* C)
	{
		C->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		C->SetCollisionObjectType(ECC_WorldDynamic);
		C->SetCollisionResponseToAllChannels(ECR_Ignore);
		C->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
		C->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		C->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		C->SetGenerateOverlapEvents(false);
		C->SetCanEverAffectNavigation(false);
	}
	USceneComponent* GlBlocks(AActor* Owner, USceneComponent* Under, const TArray<FWRLookBlock>& Blocks, TArray<TObjectPtr<USceneComponent>>& Track)
	{
		if (!Owner || Blocks.Num() == 0 || Owner->GetNetMode() == NM_DedicatedServer) return nullptr;
		USceneComponent* Frame = NewObject<USceneComponent>(Owner);
		Frame->SetupAttachment(Under ? Under : Owner->GetRootComponent());
		Frame->RegisterComponent();
		Track.Add(Frame);
		FWRBuildLook::Spawn(Owner, Frame, Blocks, FLinearColor::White, false);
		TArray<USceneComponent*> Children;
		Frame->GetChildrenComponents(false, Children);
		for (USceneComponent* Ch : Children) Track.Add(Ch);
		return Frame;
	}
	UProceduralMeshComponent* GlMesh(AActor* Owner, USceneComponent* Under, const FWRToolMesh& Mesh, TArray<TObjectPtr<USceneComponent>>& Track)
	{
		if (!Owner || Mesh.Verts.Num() == 0 || Owner->GetNetMode() == NM_DedicatedServer) return nullptr;
		UProceduralMeshComponent* P = NewObject<UProceduralMeshComponent>(Owner);
		P->SetupAttachment(Under ? Under : Owner->GetRootComponent());
		P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		P->SetCanEverAffectNavigation(false);
		P->RegisterComponent();
		Mesh.ApplyTo(P);
		Track.Add(P);
		return P;
	}
	USceneComponent* GlPivot(AActor* Owner, USceneComponent* Under, const FVector& Rel, TArray<TObjectPtr<USceneComponent>>& Track)
	{
		USceneComponent* S = NewObject<USceneComponent>(Owner);
		S->SetupAttachment(Under ? Under : Owner->GetRootComponent());
		S->SetRelativeLocation(Rel);
		S->RegisterComponent();
		Track.Add(S);
		return S;
	}
	UPointLightComponent* GlLight(AActor* Owner, USceneComponent* Under, const FVector& Rel, const FLinearColor& Colour, float Intensity, float Radius)
	{
		if (!Owner || Owner->GetNetMode() == NM_DedicatedServer) return nullptr;
		UPointLightComponent* L = NewObject<UPointLightComponent>(Owner);
		L->SetupAttachment(Under ? Under : Owner->GetRootComponent());
		L->SetRelativeLocation(Rel);
		L->SetLightColor(Colour);
		L->SetIntensity(Intensity);
		L->SetAttenuationRadius(Radius);
		L->SetCastShadows(false);
		L->RegisterComponent();
		return L;
	}
	/** A lathe ball (a skull dome, an egg, a knot). */
	void GlBall(FWRToolMesh& M, const FVector& C, float R, const FLinearColor& Col, float Rough = 0.8f, float Squash = 1.f)
	{
		TArray<FVector> P;
		for (int32 i = 0; i <= 6; ++i)
		{
			const float A = PI * i / 6.f;
			P.Add(FVector(i / 6.f, FMath::Max(0.5f, R * FMath::Sin(A)), FMath::Max(0.5f, R * FMath::Sin(A))));
		}
		M.Lathe(C - FVector(0, 0, R * Squash), FVector::UpVector, 2.f * R * Squash, P, 10, Col, Rough);
	}
	/** A skull in blocks (a cranium, a jaw, black sockets), facing +X of Yaw. */
	void GlSkull(GlKit& K, const FVector& C, float S, float Yaw, float Glow = 0.f, const GlSurf* Crust = nullptr)
	{
		const FRotator R(0.f, Yaw, 0.f);
		K.Add(GlBone(), FVector(18.f, 15.f, 15.f) * S, C, R, EWRLookMesh::Cylinder);
		K.Add(GlBone(0.95f), FVector(10.f, 12.f, 7.f) * S, C + R.RotateVector(FVector(7.f, 0.f, -8.f) * S), R);
		for (const float Y : { -3.4f, 3.4f })
		{
			K.Add(Glow > 0.f ? GlGrave(Glow) : GlBlack(), FVector(3.f, 4.f, 4.f) * S, C + R.RotateVector(FVector(8.f, Y, -1.f) * S), R);
		}
		K.Add(GlBlack(), FVector(2.f, 2.f, 3.f) * S, C + R.RotateVector(FVector(9.5f, 0.f, -5.f) * S), R);
		if (Crust) K.Add(*Crust, FVector(12.f, 13.f, 6.f) * S, C + R.RotateVector(FVector(-2.f, 0.f, 6.f) * S), R + FRotator(0.f, 0.f, 8.f));
	}
	/** A long bone (femur-like) along A -> B in blocks. */
	void GlLongBone(GlKit& K, const FVector& A, const FVector& B, float R, const GlSurf& S)
	{
		K.Rod(S, A, B, R * 2.f);
		K.Add(S, FVector(R * 3.2f, R * 2.6f, R * 2.4f), A, (B - A).Rotation());
		K.Add(S, FVector(R * 3.f, R * 2.8f, R * 2.2f), B, (B - A).Rotation());
	}
	/** A robed shade with its lantern pole (the Procession): drawn round its own foot, facing +X. */
	void GlShade(GlKit& K, int32 Salt)
	{
		const float H = 175.f + 20.f * GlHash(Salt, 3);
		const GlSurf Robe = GlSurf::Lit(GlRgb(60, 66, 74) * 0.35f, 0.25f);
		const GlSurf Hem = GlSurf::Lit(GlRgb(120, 170, 150) * 0.35f, 0.6f);
		// The robe falls in a cone from bowed shoulders, ragged at the hem (no feet show).
		K.Add(Robe, FVector(70.f, 60.f, H * 0.75f), FVector(0.f, 0.f, H * 0.375f), FRotator::ZeroRotator, EWRLookMesh::Cone);
		K.Add(Robe, FVector(44.f, 50.f, 40.f), FVector(4.f, 0.f, H * 0.78f), FRotator(0.f, 0.f, 0.f), EWRLookMesh::Cylinder);
		for (int32 t = 0; t < 6; ++t) K.Add(Hem, FVector(18.f, 10.f, 14.f), FRotator(0.f, 60.f * t + 15.f, 0.f).Vector() * 32.f + FVector(0.f, 0.f, 5.f), FRotator(0.f, 60.f * t, 10.f));
		// The hood, bowed forward, an empty dark inside with two pale points.
		K.Add(Robe, FVector(34.f, 32.f, 40.f), FVector(10.f, 0.f, H * 0.9f), FRotator(-18.f, 0.f, 0.f), EWRLookMesh::Cone);
		K.Add(GlBlack(), FVector(6.f, 18.f, 20.f), FVector(22.f, 0.f, H * 0.86f));
		for (const float Y : { -5.f, 5.f }) K.Add(GlGrave(4.f), FVector(2.f, 2.5f, 2.5f), FVector(25.f, Y, H * 0.87f));
		// The pole over the right shoulder, the tōrō-shaped lantern swinging from a hook at its top, lit grave-green.
		const FVector Hand(26.f, 22.f, H * 0.6f);
		K.Rod(GlYew(), Hand - FVector(14.f, 0.f, 70.f), Hand + FVector(40.f, 4.f, 120.f), 5.f);
		const FVector Hook = Hand + FVector(40.f, 4.f, 118.f);
		K.Rod(GlIron(), Hook, Hook + FVector(10.f, 0.f, -26.f), 1.6f);
		K.Add(GlStone(0.7f), FVector(18.f, 18.f, 3.f), Hook + FVector(10.f, 0.f, -28.f));
		K.Add(GlGrave(5.f), FVector(13.f, 13.f, 15.f), Hook + FVector(10.f, 0.f, -38.f));
		K.Add(GlStone(0.7f), FVector(20.f, 20.f, 4.f), Hook + FVector(10.f, 0.f, -48.f));
	}
}
using namespace WRGraveBossLookImpl;

// ================================================================================================ AWRGraveBossProp

AWRGraveBossProp::AWRGraveBossProp()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;
	SetNetUpdateFrequency(4.f);
	SetNetCullDistanceSquared(FMath::Square(25000.f));
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	HitShape = CreateDefaultSubobject<USphereComponent>(TEXT("HitShape"));
	HitShape->SetupAttachment(RootComponent);
	HitShape->InitSphereRadius(80.f);
	GlHitShape(HitShape);
	HitShape->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Solid = CreateDefaultSubobject<UBoxComponent>(TEXT("Solid"));
	Solid->SetupAttachment(RootComponent);
	Solid->SetBoxExtent(FVector(10.f));
	Solid->SetCollisionProfileName(TEXT("BlockAll"));
	Solid->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Solid->SetCanEverAffectNavigation(false);
}

void AWRGraveBossProp::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRGraveBossProp, Kind);
	DOREPLIFETIME(AWRGraveBossProp, State);
	DOREPLIFETIME(AWRGraveBossProp, Index);
	DOREPLIFETIME(AWRGraveBossProp, Size);
	DOREPLIFETIME(AWRGraveBossProp, TollAt);
	DOREPLIFETIME(AWRGraveBossProp, FellAt);
	DOREPLIFETIME(AWRGraveBossProp, StartAt);
	DOREPLIFETIME(AWRGraveBossProp, TurnAt);
	DOREPLIFETIME(AWRGraveBossProp, TurnAngle);
	DOREPLIFETIME(AWRGraveBossProp, Dir);
}

float AWRGraveBossProp::ServerNow() const
{
	return GlNow(this);
}

void AWRGraveBossProp::BeginPlay()
{
	Super::BeginPlay();
	FWRGraveBoss::Props().AddUnique(this);
	if (HasAuthority())
	{
		MaxHealth = Kind == TEXT("belfry") ? GlT(TEXT("bell_ringer"), TEXT("ropeHealth"), 400.f)
			: Kind == TEXT("sac") ? GlT(TEXT("crypt_mother"), TEXT("sacHealth"), 150.f)
			: Kind == TEXT("chandelier") ? GlT(TEXT("ossuary_lord"), TEXT("chainHealth"), 300.f) : 1.f;
		Health = MaxHealth;
	}
	Build();
}

FVector AWRGraveBossProp::HitPoint() const
{
	if (Kind == TEXT("belfry")) return GetActorTransform().TransformPosition(WRGraveBelfry::Frayed());
	// A sac hangs from its mooring on the stone: its middle is half its height below, out from the face.
	if (Kind == TEXT("sac")) return GetActorLocation() + GetActorForwardVector() * (Size * 0.3f) - FVector(0.f, 0.f, Size * 0.55f);
	// The chandelier's chain: halfway down from the pulley.
	if (Kind == TEXT("chandelier")) return GetActorLocation() - FVector(0.f, 0.f, Size * 0.5f);
	if (Kind == TEXT("tomb")) return GetActorLocation() + FVector(0.f, 0.f, 60.f);
	return GetActorLocation();
}

void AWRGraveBossProp::SetState(uint8 NewState)
{
	if (!HasAuthority() || State == NewState) return;
	State = NewState;
	OnRep_Look();
	ForceNetUpdate();
}

void AWRGraveBossProp::OnRep_Look()
{
	if (Kind != BuiltKind || State != BuiltState) Build();
}

bool AWRGraveBossProp::TakeHit(AWRCharacterBase* By, float Damage, const FWRHit* Hit, const FVector& At)
{
	if (!HasAuthority() || Damage <= 0.f) return false;
	const bool bFire = Hit && (Hit->Element == WRTags::Element_Fire || Hit->BurnBuildup > 0.f);
	const bool bMelee = Hit && Hit->bMelee;
	if (Kind == TEXT("belfry"))
	{
		// The frayed section 4 m up the rope (GY 2.13.1): blades and fire bite it twice over.
		if (State != 0) return false;
		Health -= Damage * (bMelee ? GlT(TEXT("bell_ringer"), TEXT("ropeSlash"), 2.f) : 1.f) * (bFire ? GlT(TEXT("bell_ringer"), TEXT("ropeFire"), 2.f) : 1.f);
		WRFx::Broadcast(this, TEXT("grave.rope_fray"), At, FRotator::ZeroRotator, 1.f, TEXT("grave.rope_creak"));
		if (Health <= 0.f)
		{
			Health = 0.f;
			SetState(1);
			// The bell falls silent: he unhooks the hand-bell; his stance drops from 90 to 60.
			if (AWRBoss* B = OwnerBoss.Get())
			{
				if (UWRGraveBossBody* Body = UWRGraveBossBody::Get(B)) Body->bBroken = true;
				if (UAbilitySystemComponent* ASC = B->GetAbilitySystemComponent())
				{
					const float Cut = GlT(TEXT("bell_ringer"), TEXT("stanceCut"), 60.f);
					ASC->SetNumericAttributeBase(UWRAttributeSet::GetMaxStanceAttribute(), Cut);
					if (const UWRAttributeSet* A = B->GetAttributes(); A && A->GetStance() > Cut) ASC->SetNumericAttributeBase(UWRAttributeSet::GetStanceAttribute(), Cut);
				}
			}
			WRFx::Broadcast(this, TEXT("grave.rope_snap"), At, FRotator::ZeroRotator, 1.5f, TEXT("grave.rope_snap"));
			for (TActorIterator<AWRCharacter> It(GetWorld()); It; ++It)
			{
				if (FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) > 5000.f) continue;
				if (AWRPlayerController* PC = Cast<AWRPlayerController>(It->GetController())) PC->ClientNotify(TEXT("The bell rope parts: the Belfry falls silent"));
			}
			UE_LOG(LogWildreach, Log, TEXT("GRAVE the Belfry rope is cut by %s"), *GetNameSafe(By));
		}
		return true;
	}
	if (Kind == TEXT("sac"))
	{
		// Silk and wet eggs: fire eats it three times over (GY 2.13.2).
		if (State != 0) return false;
		Health -= Damage * (bFire ? GlT(TEXT("crypt_mother"), TEXT("sacFire"), 3.f) : 1.f);
		WRFx::Broadcast(this, TEXT("grave.sac_hit"), At, FRotator::ZeroRotator, 1.f, TEXT("grave.sac_hit"));
		if (Health <= 0.f)
		{
			Health = 0.f;
			SetState(1);
			WRFx::Broadcast(this, TEXT("grave.sac_burst"), HitPoint(), FRotator::ZeroRotator, 1.2f, TEXT("grave.sac_burst"));
			UE_LOG(LogWildreach, Log, TEXT("GRAVE egg sac %d destroyed by %s"), Index, *GetNameSafe(By));
		}
		return true;
	}
	if (Kind == TEXT("chandelier"))
	{
		// The iron chain from the vault's pulley (GY 2.13.3): cut, it lets the chandelier fall (once a fight).
		if (State != 0) return false;
		Health -= Damage;
		WRFx::Broadcast(this, TEXT("grave.chain_hit"), At, FRotator::ZeroRotator, 1.f, TEXT("grave.chain_hit"));
		if (Health <= 0.f)
		{
			Health = 0.f;
			FellAt = ServerNow();
			SetState(1);
			WRFx::Broadcast(this, TEXT("grave.chain_snap"), At, FRotator::ZeroRotator, 1.2f, TEXT("grave.chain_snap"));
			UE_LOG(LogWildreach, Log, TEXT("GRAVE the chandelier's chain is cut by %s"), *GetNameSafe(By));
		}
		return true;
	}
	if (Kind == TEXT("tomb"))
	{
		// A Nature blow on an open tomb's slab seals it (GY 2.14): roots burst from the cracks.
		if (State == 1 && Hit && Hit->PathId == TEXT("nature"))
		{
			FWRGraveBoss::SealTomb(this, By);
			return true;
		}
		return false;
	}
	return false;
}

// ------------------------------------------------------------------------------------------------ the Procession

float AWRGraveBossProp::RingRadius(float Now) const
{
	// The ring breathes: it closes on her and opens again (GY 2.14 P3), never wider than the nave.
	const float Lo = GlT(TEXT("nhal"), TEXT("ringMin"), 650.f), Hi = GlT(TEXT("nhal"), TEXT("ringMax"), 1150.f);
	const float Period = FMath::Max(4.f, GlT(TEXT("nhal"), TEXT("ringPeriod"), 18.f));
	const float A = 0.5f + 0.5f * FMath::Cos(2.f * PI * (Now - StartAt) / Period);
	return FMath::Lerp(Lo, Hi, A);
}

float AWRGraveBossProp::RingAngle(float Now) const
{
	return TurnAngle + Dir * GlT(TEXT("nhal"), TEXT("ringSpeed"), 0.22f) * (Now - TurnAt);
}

FVector AWRGraveBossProp::ShadeAt(int32 i, float Now) const
{
	// Forty shades in files of six with a gap after each file (the way through), on an ellipse long down the nave.
	const int32 N = ShadeCount();
	const float Gap = 1.6f;
	const float Slots = float(N) + Gap * float((N + 5) / 6);
	const float Slot = float(i) + Gap * float(i / 6);
	const float A = RingAngle(Now) + 2.f * PI * Slot / Slots;
	const float R = RingRadius(Now);
	const float Stretch = GlT(TEXT("nhal"), TEXT("ringStretch"), 2.2f);
	return GetActorTransform().TransformPosition(FVector(FMath::Cos(A) * R * Stretch, FMath::Sin(A) * R, 0.f));
}

// ------------------------------------------------------------------------------------------------ the look

void AWRGraveBossProp::Build()
{
	BuiltKind = Kind;
	BuiltState = State;
	const bool bHittable = (Kind == TEXT("belfry") && State == 0) || (Kind == TEXT("sac") && State == 0) || (Kind == TEXT("chandelier") && State == 0) || (Kind == TEXT("tomb") && State == 1);
	HitShape->SetWorldLocation(HitPoint());
	HitShape->SetSphereRadius(Kind == TEXT("belfry") ? 70.f : Kind == TEXT("sac") ? FMath::Max(50.f, Size * 0.4f) : Kind == TEXT("chandelier") ? 90.f : 140.f);
	HitShape->SetCollisionEnabled(bHittable ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	// The solid parts every machine walks into: a chest tomb, a thrown headstone, the fallen chandelier.
	Solid->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Kind == TEXT("tomb")) { Solid->SetRelativeLocation(FVector(0.f, 0.f, 45.f)); Solid->SetBoxExtent(FVector(160.f, 80.f, 45.f)); Solid->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); }
	if (Kind == TEXT("headstone")) { Solid->SetRelativeLocation(FVector(0.f, 0.f, 70.f)); Solid->SetBoxExtent(FVector(12.f, 50.f, 70.f)); Solid->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); }
	if (Kind == TEXT("chandelier") && State == 2)
	{
		FVector Fl;
		GlGround(GetWorld(), GetActorLocation() - FVector(0.f, 0.f, Size + GlT(TEXT("ossuary_lord"), TEXT("chandelierDrop"), 900.f)), Fl, 200.f, 1500.f, this);
		Solid->SetWorldLocation(Fl + FVector(0.f, 0.f, 60.f));
		Solid->SetBoxExtent(FVector(150.f, 150.f, 60.f));
		Solid->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
	if (GetNetMode() == NM_DedicatedServer || Kind.IsNone()) return;
	for (USceneComponent* C : LookComps) if (C) C->DestroyComponent();
	LookComps.Reset();
	Shades.Reset();
	Swing = Wheel = RopeLow = Hang = nullptr;
	if (Glow) { Glow->DestroyComponent(); Glow = nullptr; }
	TArray<FWRLookBlock> Blocks;
	GlKit K(GetActorTransform(), Blocks);
	FWRToolMesh M;

	if (Kind == TEXT("belfry"))
	{
		using namespace WRGraveBelfry;
		// The bell frame in the open belfry: two oak A-frames standing on the belfry floor beams, a gudgeon bearing at
		// each apex carrying the headstock's axle (along Y). The tower's stone is the scenery's.
		const FVector Ax = WRGraveBelfry::Wheel();
		const float FloorZ = 2800.f;
		for (const float S : { -1.f, 1.f })
		{
			const float Y = S * 150.f;
			K.Rod(GlOak(0.9f), FVector(-230.f, Y, FloorZ), FVector(0.f, Y, Ax.Z + 10.f), 22.f, true);
			K.Rod(GlOak(0.9f), FVector(230.f, Y, FloorZ), FVector(0.f, Y, Ax.Z + 10.f), 22.f, true);
			K.Rod(GlOak(0.8f), FVector(-140.f, Y, FloorZ + 140.f), FVector(140.f, Y, FloorZ + 140.f), 16.f, true);
			K.Add(GlIron(), FVector(26.f, 30.f, 22.f), FVector(0.f, Y, Ax.Z));                // the gudgeon bearing
		}
		for (const float X : { -230.f, 230.f }) K.Rod(GlOak(0.85f), FVector(X, -170.f, FloorZ), FVector(X, 170.f, FloorZ), 26.f, true);   // sill beams
		// The rope guide: an oak boss across the belfry floor's hatch, the rope through its hole.
		K.Add(GlOak(0.8f), FVector(40.f, 120.f, 18.f), FVector(150.f, 0.f, FloorZ - 4.f));
		GlBlocks(this, nullptr, Blocks, LookComps);
		Blocks.Reset();
		// The swinging set: headstock, bell and wheel turn together about the axle.
		Swing = GlPivot(this, nullptr, Ax, LookComps);
		{
			TArray<FWRLookBlock> SB;
			GlKit SK(GetActorTransform(), SB);
			SK.Add(GlOak(1.f), FVector(70.f, 280.f, 50.f), FVector(0.f, 0.f, 10.f));                 // the headstock
			SK.Rod(GlIron(), FVector(0.f, -160.f, 0.f), FVector(0.f, 160.f, 0.f), 10.f);           // the axle
			for (const float Y : { -60.f, 60.f }) SK.Rod(GlIron(), FVector(-28.f, Y, 30.f), FVector(-28.f, Y, -60.f), 6.f);   // the bell's straps
			GlBlocks(this, Swing, SB, LookComps);
			// The wheel on the headstock's end (Y +130): a rim with a rope groove, six spokes, 2.2 m across.
			FWRToolMesh WM;
			TArray<FVector> Rim;
			const float R = 110.f;
			for (int32 i = 0; i < 24; ++i) { const float A = 2.f * PI * i / 24.f; Rim.Add(FVector(FMath::Cos(A) * R, 130.f, FMath::Sin(A) * R)); }
			WM.Tube(Rim, 7.f, 6, GlRgb(84, 68, 54) * 0.45f, 0.85f, true);
			TArray<FVector> Rim2;
			for (const FVector& P : Rim) Rim2.Add(FVector(P.X * 0.93f, 141.f, P.Z * 0.93f));
			WM.Tube(Rim2, 5.f, 6, GlRgb(84, 68, 54) * 0.4f, 0.85f, true);
			for (int32 s = 0; s < 6; ++s)
			{
				const float A = 2.f * PI * s / 6.f;
				WM.Tube({ FVector(0.f, 130.f, 0.f), FVector(FMath::Cos(A) * R, 130.f, FMath::Sin(A) * R) }, 5.f, 6, GlRgb(84, 68, 54) * 0.45f, 0.85f);
			}
			// The bell: a 1.6 m bronze mouth under the headstock, crown to lip, the clapper inside on its staple.
			WM.Lathe(FVector(0.f, 0.f, -15.f), -FVector::UpVector, 150.f, { FVector(0.f, 42.f, 42.f), FVector(0.12f, 50.f, 50.f), FVector(0.45f, 54.f, 54.f), FVector(0.75f, 64.f, 64.f), FVector(0.92f, 78.f, 78.f), FVector(1.f, 80.f, 80.f) }, 20, GlRgb(122, 90, 52) * 0.55f, 0.4f);
			WM.Lathe(FVector(0.f, 0.f, -150.f), -FVector::UpVector, 10.f, { FVector(0.f, 80.f, 80.f), FVector(1.f, 76.f, 76.f) }, 20, GlRgb(78, 140, 122) * 0.5f, 0.6f);
			WM.Tube({ FVector(0.f, 0.f, -30.f), FVector(0.f, 0.f, -130.f) }, 4.f, 6, GlRgb(30, 28, 27), 0.7f);
			GlBall(WM, FVector(0.f, 0.f, -135.f), 12.f, GlRgb(40, 36, 34), 0.6f);
			GlMesh(this, Swing, WM, LookComps);
		}
		// The rope: off the wheel's groove (its bottom) down to the guide in the floor, then down the shaft to the arch.
		{
			FWRToolMesh RM;
			RM.Tube({ FVector(110.f, 130.f, Ax.Z - 10.f), FVector(150.f, 0.f, FloorZ + 30.f), FVector(150.f, 0.f, FloorZ - 20.f) }, 2.4f, 6, GlRopeC(), 0.95f);
			GlMesh(this, nullptr, RM, LookComps);
		}
		RopeLow = GlPivot(this, nullptr, FVector(150.f, 0.f, 0.f), LookComps);
		{
			FWRToolMesh LM;
			if (State == 0)
			{
				// The run down the shaft to the sally: whole, a fat frayed patch 4 m up glowing faintly (the counter), the
				// woollen sally at hand height, the tail looped on a cleat.
				LM.Tube({ FVector(0.f, 0.f, FloorZ - 20.f), FVector(0.f, 0.f, 520.f) }, 2.4f, 6, GlRopeC(), 0.95f);
				LM.Tube({ FVector(0.f, 0.f, 520.f), FVector(0.f, 0.f, 300.f) }, 3.6f, 6, GlRopeC() * 0.8f, 1.f);
				LM.Tube({ FVector(0.f, 0.f, 300.f), FVector(0.f, 0.f, 230.f) }, 2.4f, 6, GlRopeC(), 0.95f);
				LM.Lathe(FVector(0.f, 0.f, 230.f), -FVector::UpVector, 90.f, { FVector(0.f, 4.f, 4.f), FVector(0.1f, 7.f, 7.f), FVector(0.9f, 7.f, 7.f), FVector(1.f, 4.f, 4.f) }, 8, GlRgb(120, 40, 34) * 0.5f, 1.f);
				LM.Tube({ FVector(0.f, 0.f, 140.f), FVector(0.f, 0.f, 110.f), FVector(30.f, 0.f, 90.f), FVector(50.f, 0.f, 110.f) }, 2.4f, 6, GlRopeC(), 0.95f);
				TArray<FWRLookBlock> FB;
				GlKit FK(GetActorTransform(), FB);
				for (int32 t = 0; t < 8; ++t) FK.Add(GlGrave(1.6f), FVector(1.2f, 1.2f, 14.f), FVector(4.f * FMath::Cos(t * 0.8f), 4.f * FMath::Sin(t * 0.8f), 330.f + 22.f * t), FRotator(25.f * FMath::Sin(t * 1.3f), 45.f * t, 0.f));
				GlBlocks(this, RopeLow, FB, LookComps);
			}
			else
			{
				// Cut: the upper end hangs a few metres short of the floor, the lower length lies coiled at the arch.
				LM.Tube({ FVector(0.f, 0.f, FloorZ - 20.f), FVector(0.f, 0.f, 420.f) }, 2.4f, 6, GlRopeC(), 0.95f);
				LM.Lathe(FVector(0.f, 0.f, 420.f), -FVector::UpVector, 14.f, { FVector(0.f, 3.f, 3.f), FVector(1.f, 5.f, 5.f) }, 6, GlRopeC() * 0.7f, 1.f);
				TArray<FVector> Coil;
				for (int32 i = 0; i < 40; ++i) { const float A = i * 0.6f; const float Rr = 40.f - i * 0.6f; Coil.Add(FVector(30.f + FMath::Cos(A) * Rr, FMath::Sin(A) * Rr, 6.f + i * 0.4f)); }
				LM.Tube(Coil, 2.4f, 6, GlRopeC(), 0.95f);
			}
			GlMesh(this, RopeLow, LM, LookComps);
		}
		Glow = GlLight(this, nullptr, Ax + FVector(0.f, 0.f, -150.f), GlGraveC(), 0.f, 900.f);
		return;
	}
	if (Kind == TEXT("sac"))
	{
		// An egg sac: a silk bag the size of a man moored to the stone by a fan of threads, eggs bulging through the silk,
		// a sick glow inside. Burnt: a blackened torn husk on its threads, ash under it.
		const float H = Size;
		for (int32 t = 0; t < 7; ++t)
		{
			const FVector To(-6.f, (t - 3) * 14.f, 20.f + 10.f * (t % 2));
			M.Tube({ FVector(H * 0.3f, 0.f, -H * 0.12f), To }, 0.8f, 4, GlRgb(222, 216, 200) * 0.5f, 0.9f);
		}
		if (State == 0)
		{
			M.Lathe(FVector(H * 0.3f, 0.f, -H * 0.05f), -FVector::UpVector, H, { FVector(0.f, 6.f, 6.f), FVector(0.15f, H * 0.22f, H * 0.2f), FVector(0.55f, H * 0.32f, H * 0.3f), FVector(0.85f, H * 0.26f, H * 0.25f), FVector(1.f, 4.f, 4.f) }, 14, GlRgb(222, 216, 200) * 0.55f, 0.85f, 6.f);
			for (int32 e = 0; e < 14; ++e)
			{
				const float A = e * 2.399f, Z = -H * (0.25f + 0.55f * GlHash(Index, e));
				const float Rr = H * 0.28f * FMath::Sin(PI * (0.15f + 0.8f * (-Z / H)));
				GlBall(M, FVector(H * 0.3f + FMath::Cos(A) * Rr, FMath::Sin(A) * Rr, Z), 10.f + 4.f * GlHash(e, Index), GlRgb(200, 196, 170) * 0.6f, 0.6f);
			}
			GlMesh(this, nullptr, M, LookComps);
			Glow = GlLight(this, nullptr, FVector(H * 0.3f, 0.f, -H * 0.55f), GlRgb(190, 220, 160), 400.f, 300.f);
		}
		else
		{
			M.Lathe(FVector(H * 0.3f, 0.f, -H * 0.05f), -FVector::UpVector, H * 0.55f, { FVector(0.f, 6.f, 6.f), FVector(0.4f, H * 0.2f, H * 0.12f), FVector(0.8f, H * 0.16f, H * 0.2f), FVector(1.f, H * 0.1f, H * 0.04f) }, 10, GlRgb(30, 26, 24) * 0.55f, 1.f, 9.f);
			GlMesh(this, nullptr, M, LookComps);
			FVector G;
			GlGround(GetWorld(), HitPoint(), G, 50.f, 2000.f, this);
			const FVector L = GetActorTransform().InverseTransformPosition(G);
			for (int32 a = 0; a < 6; ++a) K.Add(GlChar(0.8f + 0.2f * GlHash(a, Index)), FVector(30.f + 20.f * GlHash(a, 2), 22.f, 3.f), L + FVector(FMath::Cos(a * 1.1f) * 40.f, FMath::Sin(a * 1.7f) * 40.f, 2.f), FRotator(0.f, 60.f * a, 0.f));
			GlBlocks(this, nullptr, Blocks, LookComps);
		}
		return;
	}
	if (Kind == TEXT("chandelier"))
	{
		// The bone chandelier (Sedlec): an iron hoop 3 m across hung on four chains to a ring under the long chain from
		// the vault's pulley; garlands of long bones and skulls round the hoop, a second, smaller tier below, skulls holding
		// candles. The pulley is the room's; the chain from it is here.
		const float Drop = GlT(TEXT("ossuary_lord"), TEXT("chandelierDrop"), 900.f);
		Hang = GlPivot(this, nullptr, FVector::ZeroVector, LookComps);
		const auto Body = [&](GlKit& CK, FWRToolMesh& CM, const FVector& Top, bool bDown)
		{
			const float R = 150.f;
			for (int32 c = 0; c < 4; ++c)
			{
				const FVector Rim = Top + FRotator(0.f, 45.f + 90.f * c, 0.f).Vector() * R + FVector(0.f, 0.f, -180.f);
				CM.Tube({ Top, Rim }, 1.6f, 4, GlRgb(30, 28, 27), 0.7f);
			}
			TArray<FVector> Hoop, Hoop2;
			for (int32 i = 0; i < 28; ++i)
			{
				const float A = 2.f * PI * i / 28.f;
				Hoop.Add(Top + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, -180.f));
				Hoop2.Add(Top + FVector(FMath::Cos(A) * R * 0.55f, FMath::Sin(A) * R * 0.55f, -260.f));
			}
			CM.Tube(Hoop, 4.f, 6, GlRgb(30, 28, 27), 0.7f, true);
			CM.Tube(Hoop2, 3.f, 6, GlRgb(30, 28, 27), 0.7f, true);
			for (int32 s = 0; s < 4; ++s) CM.Tube({ Top + FRotator(0.f, 90.f * s, 0.f).Vector() * R * 0.55f + FVector(0.f, 0.f, -260.f), Top + FRotator(0.f, 90.f * s, 0.f).Vector() * R + FVector(0.f, 0.f, -180.f) }, 2.f, 4, GlRgb(30, 28, 27), 0.7f);
			for (int32 g = 0; g < 14; ++g)
			{
				// Bone garlands swag between the hoop's hangers; a skull at each hanger with a candle on its crown.
				const float A0 = 2.f * PI * g / 14.f, A1 = 2.f * PI * (g + 1) / 14.f;
				const FVector P0 = Top + FVector(FMath::Cos(A0) * R, FMath::Sin(A0) * R, -180.f), P1 = Top + FVector(FMath::Cos(A1) * R, FMath::Sin(A1) * R, -180.f);
				for (int32 b = 0; b < 3; ++b)
				{
					const float T0 = b / 3.f, T1 = (b + 1) / 3.f;
					GlLongBone(CK, FMath::Lerp(P0, P1, T0) - FVector(0.f, 0.f, 40.f * FMath::Sin(PI * T0)), FMath::Lerp(P0, P1, T1) - FVector(0.f, 0.f, 40.f * FMath::Sin(PI * T1)), 2.f, GlBone(0.95f));
				}
				GlSkull(CK, P0 + FVector(0.f, 0.f, 10.f), 1.f, FMath::RadiansToDegrees(A0));
				if (!bDown)
				{
					CK.Add(GlCandle(0.f), FVector(4.f, 4.f, 16.f), P0 + FVector(0.f, 0.f, 28.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
					CK.Add(GlCandle(8.f), FVector(2.f, 2.f, 4.f), P0 + FVector(0.f, 0.f, 39.f), FRotator::ZeroRotator, EWRLookMesh::Cone);
				}
			}
			for (int32 s = 0; s < 6; ++s) GlSkull(CK, Top + FRotator(0.f, 60.f * s, 0.f).Vector() * R * 0.55f + FVector(0.f, 0.f, -250.f), 0.9f, 60.f * s);
			GlSkull(CK, Top + FVector(0.f, 0.f, -300.f), 1.3f, 0.f);
		};
		if (State <= 1)
		{
			// The long chain: links every 14 cm (alternately turned), then the chandelier on its end.
			for (float Z = 0.f; Z < Size; Z += 14.f)
			{
				const bool bTurn = int32(Z / 14.f) % 2 == 0;
				K.Add(GlIron(), FVector(3.f, bTurn ? 10.f : 3.f, 16.f), FVector(0.f, 0.f, -Z - 7.f), FRotator(0.f, bTurn ? 0.f : 90.f, 0.f));
			}
			K.Disc(GlIron(), FVector(0.f, 0.f, -Size), FVector::UpVector, 40.f, 6.f);
			Body(K, M, FVector(0.f, 0.f, -Size), false);
			GlBlocks(this, Hang, Blocks, LookComps);
			GlMesh(this, Hang, M, LookComps);
			Glow = GlLight(this, Hang, FVector(0.f, 0.f, -Size - 160.f), GlCandleC(), 2500.f, 1600.f);
		}
		else
		{
			// Down: the hoop crushed and tipped on the floor, bones spilled across it, the chain heaped beside it.
			FVector Fl;
			GlGround(GetWorld(), GetActorLocation() - FVector(0.f, 0.f, Size + Drop), Fl, 200.f, 1500.f, this);
			const FVector Floor = GetActorTransform().InverseTransformPosition(Fl);
			GlKit DK(GetActorTransform(), Blocks);
			DK.Frame = FTransform(FRotator(14.f, 20.f, 9.f), Floor + FVector(0.f, 0.f, 230.f));
			Body(DK, M, FVector::ZeroVector, true);
			DK.Frame = FTransform::Identity;
			for (int32 b = 0; b < 30; ++b)
			{
				const float A = b * 2.399f, D = 120.f + 260.f * GlHash(b, 5);
				const FVector P = Floor + FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, 4.f);
				GlLongBone(DK, P, P + FRotator(0.f, 360.f * GlHash(b, 9), 0.f).Vector() * 40.f, 2.f, GlBone(0.9f));
			}
			for (int32 l = 0; l < 30; ++l) DK.Add(GlIron(), FVector(3.f, 10.f, 16.f), Floor + FVector(-220.f + FMath::Cos(l * 0.7f) * 50.f, FMath::Sin(l * 0.7f) * 50.f, 6.f + l * 0.8f), FRotator(90.f, l * 40.f, 0.f));
			// The short end still hangs from the pulley.
			for (float Z = 0.f; Z < 300.f; Z += 14.f) DK.Add(GlIron(), FVector(3.f, int32(Z / 14.f) % 2 ? 3.f : 10.f, 16.f), FVector(0.f, 0.f, -Z - 7.f), FRotator(0.f, int32(Z / 14.f) % 2 ? 90.f : 0.f, 0.f));
			GlBlocks(this, nullptr, Blocks, LookComps);
			GlMesh(this, nullptr, M, LookComps);
		}
		return;
	}
	if (Kind == TEXT("tomb"))
	{
		// A chest tomb (a table tomb 3 x 1.5 m, 0.9 m high): panelled sides with weepers carved in niches, a moulded lid
		// with a worn effigy's outline. Open: the lid shoved half off and cracked, grave-light up out of the dark inside.
		// Sealed: the lid back, roots up through every crack and over the lid, white blossom on them.
		const float L = 300.f, W = 150.f, H = 90.f;
		K.Add(GlLimestone(0.85f), FVector(L + 20.f, W + 20.f, 14.f), FVector(0.f, 0.f, 7.f));
		for (const float S : { -1.f, 1.f })
		{
			K.Add(GlLimestone(0.9f), FVector(L, 12.f, H - 14.f), FVector(0.f, S * (W * 0.5f - 6.f), 14.f + (H - 14.f) * 0.5f));
			K.Add(GlLimestone(0.9f), FVector(12.f, W - 24.f, H - 14.f), FVector(S * (L * 0.5f - 6.f), 0.f, 14.f + (H - 14.f) * 0.5f));
			for (int32 n = 0; n < 4; ++n)
			{
				// The weepers' niches: a pointed recess, a hooded figure in each.
				const float X = -L * 0.375f + n * L * 0.25f;
				K.Add(GlStone(0.6f), FVector(40.f, 3.f, 50.f), FVector(X, S * (W * 0.5f + 0.5f), 48.f));
				K.Add(GlLimestone(0.8f), FVector(14.f, 6.f, 38.f), FVector(X, S * (W * 0.5f + 1.f), 44.f), FRotator::ZeroRotator, EWRLookMesh::Cone);
			}
		}
		K.Add(GlStone(0.4f), FVector(L - 24.f, W - 24.f, 4.f), FVector(0.f, 0.f, H - 4.f));
		const auto Lid = [&](const FVector& At, const FRotator& R, bool bCracked)
		{
			K.Add(GlLimestone(1.f), FVector(L + 14.f, W + 14.f, 16.f), At, R);
			K.Add(GlLimestone(0.95f), FVector(L - 40.f, W - 50.f, 8.f), At + R.RotateVector(FVector(0.f, 0.f, 12.f)), R);
			// The effigy: a worn figure in a shroud, hands crossed, a lantern at its feet.
			K.Add(GlLimestone(0.9f), FVector(180.f, 50.f, 14.f), At + R.RotateVector(FVector(-10.f, 0.f, 22.f)), R, EWRLookMesh::Cylinder);
			K.Add(GlLimestone(0.9f), FVector(30.f, 30.f, 22.f), At + R.RotateVector(FVector(-110.f, 0.f, 24.f)), R, EWRLookMesh::Cylinder);
			K.Add(GlStone(0.8f), FVector(18.f, 18.f, 24.f), At + R.RotateVector(FVector(100.f, 0.f, 28.f)), R);
			if (bCracked) for (int32 c = 0; c < 5; ++c) K.Add(GlBlack(), FVector(70.f, 2.f, 17.f), At + R.RotateVector(FVector(-60.f + 30.f * c, (c % 2 ? 1.f : -1.f) * 20.f, 1.f)), R + FRotator(0.f, 30.f + 25.f * c, 0.f));
		};
		if (State == 1)
		{
			Lid(FVector(40.f, 70.f, H + 30.f), FRotator(0.f, 22.f, -10.f), true);
			K.Add(GlGrave(3.f), FVector(L - 40.f, W - 40.f, 2.f), FVector(-30.f, -20.f, H - 30.f));
			GlBlocks(this, nullptr, Blocks, LookComps);
			if (USceneComponent* Fx = WRFx::Attach(RootComponent, TEXT("grave.tomb_open"), NAME_None, 1.f)) { Fx->SetRelativeLocation(FVector(0.f, 0.f, H)); LookComps.Add(Fx); }
			Glow = GlLight(this, nullptr, FVector(-30.f, -20.f, H + 60.f), GlGraveC(), 1800.f, 700.f);
			return;
		}
		Lid(FVector(0.f, 0.f, H + 8.f), FRotator::ZeroRotator, State == 2);
		if (State == 2)
		{
			// Roots up from the floor at the tomb's corners and through the lid's cracks, over the lid, blossom on them.
			for (int32 r = 0; r < 8; ++r)
			{
				const float Sx = (r % 4 < 2 ? -1.f : 1.f), Sy = (r % 2 ? -1.f : 1.f);
				const FVector Base(Sx * (L * 0.5f + 20.f) * (0.4f + 0.6f * GlHash(r, Index)), Sy * (W * 0.5f + 16.f), 0.f);
				TArray<FVector> Pts = { Base, Base + FVector(0.f, -Sy * 10.f, H * 0.6f), FVector(Base.X * 0.8f, Sy * W * 0.35f, H + 24.f), FVector(Base.X * 0.3f, -Sy * W * 0.1f, H + 30.f) };
				M.Tube(Pts, 7.f - 0.4f * r, 7, GlRootC(), 0.9f);
				for (int32 b = 0; b < 4; ++b) K.Add(GlBlossom(), FVector(9.f, 9.f, 4.f), Pts[2 + b % 2] + FVector(GlHash(r, b) * 20.f - 10.f, GlHash(b, r) * 20.f - 10.f, 6.f), FRotator(0.f, 40.f * b, 0.f), EWRLookMesh::Cylinder);
			}
			GlMesh(this, nullptr, M, LookComps);
		}
		GlBlocks(this, nullptr, Blocks, LookComps);
		return;
	}
	if (Kind == TEXT("headstone"))
	{
		// A headstone the Bell-Ringer wrenched out and threw: it stands where it came down, tipped, its foot in a split of
		// turf; a slab (Index 0-1) or a cross in a ring (2-3); moss on its back.
		if (Index < 2)
		{
			K.Add(GlStone(0.9f), FVector(14.f, 70.f, 120.f), FVector(0.f, 0.f, 40.f));
			K.Add(GlStone(0.9f), FVector(14.f, 70.f, 20.f), FVector(0.f, 0.f, 103.f), FRotator(0.f, 0.f, 0.f), EWRLookMesh::Cylinder);
			K.Add(GlMoss(), FVector(2.f, 60.f, 60.f), FVector(-8.f, 0.f, 40.f));
			K.Add(GlStone(0.5f), FVector(1.f, 40.f, 6.f), FVector(8.f, 0.f, 70.f));
		}
		else
		{
			K.Add(GlStone(0.85f), FVector(16.f, 30.f, 140.f), FVector(0.f, 0.f, 50.f));
			K.Add(GlStone(0.85f), FVector(16.f, 90.f, 26.f), FVector(0.f, 0.f, 90.f));
			K.Ring(GlStone(0.85f), FVector(0.f, 0.f, 90.f), 38.f, 16.f, 8.f, 16);
		}
		for (int32 c = 0; c < 6; ++c) K.Add(GlSoil(0.9f), FVector(30.f, 24.f, 10.f), FVector(FMath::Cos(c * 1.05f) * 45.f, FMath::Sin(c * 1.05f) * 45.f, -2.f), FRotator(0.f, 60.f * c, 8.f));
		GlBlocks(this, nullptr, Blocks, LookComps);
		return;
	}
	if (Kind == TEXT("dug"))
	{
		// State 0: a grave he opened on his round, a 2 x 0.8 m hole, the spoil heaped beside it, the spade's cut faces.
		// State 1: earth heaped over a survivor he buried (clods, a hand clawing out). State 2: bone hands holding a tame
		// half under the floor (Lay to rest).
		if (State == 0)
		{
			K.Add(GlBlack(), FVector(200.f, 80.f, 4.f), FVector(0.f, 0.f, 1.f));
			for (const float S : { -1.f, 1.f }) K.Add(GlSoil(0.7f), FVector(200.f, 6.f, 30.f), FVector(0.f, S * 40.f, -10.f));
			for (int32 c = 0; c < 12; ++c) K.Add(GlSoil(0.85f + 0.2f * GlHash(c, 1)), FVector(40.f + 30.f * GlHash(c, 2), 30.f + 20.f * GlHash(c, 3), 20.f + 25.f * GlHash(c, 4)), FVector(-90.f + 16.f * c, 95.f + 20.f * GlHash(c, 5), 8.f), FRotator(0.f, 30.f * c, 10.f));
		}
		else if (State == 1)
		{
			for (int32 c = 0; c < 16; ++c) K.Add(GlSoil(0.8f + 0.25f * GlHash(c, 6)), FVector(50.f + 30.f * GlHash(c, 7), 40.f + 30.f * GlHash(c, 8), 30.f + 30.f * GlHash(c, 9)), FVector(FMath::Cos(c * 0.9f) * 55.f * GlHash(c, 1), FMath::Sin(c * 0.9f) * 55.f * GlHash(c, 2), 25.f), FRotator(10.f * c, 25.f * c, 6.f));
			K.Add(GlTurf(0.8f), FVector(80.f, 60.f, 10.f), FVector(30.f, -40.f, 50.f), FRotator(20.f, 10.f, 30.f));
		}
		else
		{
			for (int32 h = 0; h < 6; ++h)
			{
				const FVector B = FRotator(0.f, 60.f * h, 0.f).Vector() * 70.f;
				const FVector T = B + FVector(0.f, 0.f, 70.f) - FRotator(0.f, 60.f * h, 0.f).Vector() * 30.f;
				GlLongBone(K, B, T, 3.f, GlBone(0.9f));
				for (int32 f = 0; f < 4; ++f) K.Rod(GlBone(0.9f), T, T + FRotator(-40.f, 60.f * h + 180.f + (f - 1.5f) * 18.f, 0.f).Vector() * 22.f, 2.4f);
			}
			K.Add(GlGrave(2.f), FVector(160.f, 160.f, 2.f), FVector(0.f, 0.f, 1.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		}
		GlBlocks(this, nullptr, Blocks, LookComps);
		return;
	}
	if (Kind == TEXT("procession"))
	{
		// Forty shades walking the ring (each moved every frame in TickLook).
		for (int32 i = 0; i < ShadeCount(); ++i)
		{
			USceneComponent* S = GlPivot(this, nullptr, FVector::ZeroVector, LookComps);
			TArray<FWRLookBlock> SB;
			GlKit SK(GetActorTransform(), SB);
			GlShade(SK, i);
			GlBlocks(this, S, SB, LookComps);
			Shades.Add(S);
		}
		Glow = GlLight(this, nullptr, FVector(0.f, 0.f, 300.f), GlGraveC(), 3000.f, 2500.f);
		return;
	}
}

void AWRGraveBossProp::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Clock += DeltaSeconds;
	const float Now = ServerNow();
	if (HasAuthority() && Kind == TEXT("chandelier") && State == 1)
	{
		// It falls on gravity from the moment the chain parted (every machine draws the same curve).
		const float Drop = GlT(TEXT("ossuary_lord"), TEXT("chandelierDrop"), 900.f);
		const float Fall = FMath::Sqrt(2.f * Drop / 980.f);
		if (Now - FellAt >= Fall)
		{
			SetState(2);
			FVector Floor;
			GlGround(GetWorld(), GetActorLocation() - FVector(0.f, 0.f, Size + Drop), Floor, 200.f, 1500.f, this);
			WRFx::Broadcast(this, TEXT("grave.chandelier_crash"), Floor, FRotator::ZeroRotator, 3.f, TEXT("grave.chandelier_crash"));
			FWRGraveBoss::ChandelierLanded(this, Floor);
		}
	}
	if (GetNetMode() != NM_DedicatedServer) TickLook(Now);
}

void AWRGraveBossProp::TickLook(float Now)
{
	if (Kind == TEXT("belfry") && Swing)
	{
		// A toll: one full turn of the bell on its headstock (eased: the pull, the rise over the balance, the fall), then
		// a sway that dies away; between tolls it stirs in the wind. The wheel turns with it; the sally rises and falls
		// with the wheel (~1.5 m a stroke). Cut: it swings down to rest.
		const float Period = FMath::Max(1.f, GlT(TEXT("bell_ringer"), TEXT("bellPeriod"), 3.2f));
		const float Sway = GlT(TEXT("bell_ringer"), TEXT("bellSwing"), 28.f), Decay = FMath::Max(0.5f, GlT(TEXT("bell_ringer"), TEXT("bellDecay"), 6.f));
		const float T = Now - TollAt;
		float Angle = 0.f;
		if (T >= 0.f && T < Period && State == 0) Angle = 360.f * FMath::SmoothStep(0.f, 1.f, T / Period);
		else if (T >= Period) Angle = Sway * FMath::Exp(-(T - Period) / Decay) * FMath::Sin(2.f * PI * (T - Period) / 2.4f);
		Angle += 1.5f * FMath::Sin(Clock * 0.7f + 1.3f);
		Swing->SetRelativeRotation(FRotator(Angle, 0.f, 0.f));
		if (RopeLow && State == 0) RopeLow->SetRelativeLocation(FVector(150.f, 0.f, 150.f * 0.5f * (1.f - FMath::Cos(FMath::DegreesToRadians(Angle)))));
		if (Glow) Glow->SetIntensity(T >= 0.f && T < Period + 1.f ? 1500.f * (1.f - FMath::Clamp((T - Period) / 1.f, 0.f, 1.f)) : 0.f);
	}
	if (Kind == TEXT("chandelier") && Hang)
	{
		if (State == 0)
		{
			// A pendulum of its chain's length: a small slow sway (2 degrees) with a second, crossing one.
			const float L = FMath::Max(100.f, Size + 200.f);
			const float W = FMath::Sqrt(980.f / L);
			Hang->SetRelativeRotation(FRotator(2.f * FMath::Sin(Clock * W), 0.f, 1.2f * FMath::Sin(Clock * W * 0.93f + 1.1f)));
			Hang->SetRelativeLocation(FVector::ZeroVector);
		}
		else if (State == 1)
		{
			const float Drop = GlT(TEXT("ossuary_lord"), TEXT("chandelierDrop"), 900.f);
			const float T = FMath::Max(0.f, Now - FellAt);
			Hang->SetRelativeLocation(FVector(0.f, 0.f, -FMath::Min(Drop, 0.5f * 980.f * T * T)));
		}
	}
	if (Kind == TEXT("procession") && Shades.Num())
	{
		// Each shade walks the ring, bobbing a little with its step, facing the way it goes.
		for (int32 i = 0; i < Shades.Num(); ++i)
		{
			USceneComponent* S = Shades[i];
			if (!S) continue;
			const FVector P = ShadeAt(i, Now), Ahead = ShadeAt(i, Now + 0.2f * Dir);
			FVector G;
			GlGround(GetWorld(), P, G, 300.f, 800.f, this);
			const float Bob = 3.f * FMath::Abs(FMath::Sin(Now * 3.1f + i * 0.7f));
			S->SetWorldLocationAndRotation(G + FVector(0.f, 0.f, Bob), FRotator(0.f, (Ahead - P).Rotation().Yaw, 1.5f * FMath::Sin(Now * 3.1f + i)));
		}
	}
	if (Kind == TEXT("sac") && Glow && State == 0) Glow->SetIntensity(300.f + 160.f * FMath::Sin(Clock * 1.7f + Index));
	if (Kind == TEXT("tomb") && Glow && State == 1) Glow->SetIntensity(1500.f + 400.f * FMath::Sin(Clock * 2.3f + Index));
}

// ================================================================================================ UWRGraveBossBody

UWRGraveBossBody::UWRGraveBossBody()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UWRGraveBossBody::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UWRGraveBossBody, Pose);
	DOREPLIFETIME(UWRGraveBossBody, bBroken);
	DOREPLIFETIME(UWRGraveBossBody, Count);
	DOREPLIFETIME(UWRGraveBossBody, Veil);
	DOREPLIFETIME(UWRGraveBossBody, Caged);
}

UWRGraveBossBody* UWRGraveBossBody::Get(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UWRGraveBossBody>() : nullptr;
}

void UWRGraveBossBody::Attach(AWRBoss* Boss)
{
	if (!Boss || !Boss->HasAuthority() || !FWRGraveBoss::IsGraveBoss(Boss) || Get(Boss)) return;
	UWRGraveBossBody* Body = NewObject<UWRGraveBossBody>(Boss, TEXT("GraveBossBody"));
	Body->RegisterComponent();
	Boss->AddInstanceComponent(Body);
}

void UWRGraveBossBody::EndPlay(const EEndPlayReason::Type Reason)
{
	ClearLook();
	Super::EndPlay(Reason);
}

USkeletalMeshComponent* UWRGraveBossBody::Skel() const
{
	const AWRCreature* C = Cast<AWRCreature>(GetOwner());
	return C ? C->GetCreatureSkel() : nullptr;
}

void UWRGraveBossBody::ClearLook()
{
	for (USceneComponent* C : LookComps) if (C) C->DestroyComponent();
	LookComps.Reset();
	for (UPointLightComponent* L : Lights) if (L) L->DestroyComponent();
	Lights.Reset();
	Groups.Reset();
	LookFor = NAME_None;
	ShownCount = ShownVeil = ShownPose = 255;
}

USceneComponent* UWRGraveBossBody::AddGroup(FName Bone, const FVector& WorldAt, const FRotator& WorldRot, const TArray<FWRLookBlock>& Blocks, int32 Kind)
{
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	USkeletalMeshComponent* Body = Skel();
	if (!C || !Body || Blocks.Num() == 0) return nullptr;
	USceneComponent* Frame = NewObject<USceneComponent>(C);
	Frame->RegisterComponent();
	if (Bone.IsNone()) Frame->AttachToComponent(C->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
	else Frame->AttachToComponent(Body, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, false), Bone);
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
	G.Phase = FMath::FRand() * 6.28f;
	Groups.Add(G);
	return Frame;
}

namespace WRGraveBossLookImpl
{
	/** The body's part groups (what TickLook moves or shows by state). */
	enum EGbGroup : int32
	{
		BStatic = 0, BHandBellBelt = 1, BHandBellHand = 2, BRopeWrap = 3,            // the Bell-Ringer
		BRider = 10,                                                              // the Crypt Mother's young (10 + i)
		BWhip = 40,                                                               // her whips
		BKeystone = 50, BPile = 53, BBody = 54, BWave = 55,                       // the Ossuary Lord (keystones 50-52)
		BLantern = 60, BCageDoor = 61, BSheet = 62,                               // the Gravedigger
		BVeil = 70, BVeilUp = 71, BJaw = 72, BCrown = 73                          // Nhal
	};
	/** tuning look fields from creatures.json are read by the creature; the bosses' look numbers are here (cm). */
}

void UWRGraveBossBody::ApplyTint()
{
	// The cook's palette over the rig's own texture (M_CreatureTint), from the boss row's look.tint (the creatures'
	// body reads the same schema).
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	USkeletalMeshComponent* Body = Skel();
	if (!C || !Body) return;
	TintMats.Reset();
	const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("creatures.json"));
	const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
	TSharedPtr<FJsonObject> Look;
	if (Root.IsValid() && Root->TryGetArrayField(TEXT("creatures"), List))
	{
		for (const TSharedPtr<FJsonValue>& V : *List)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			FString Id;
			const TSharedPtr<FJsonObject>* L = nullptr;
			if (O.IsValid() && O->TryGetStringField(TEXT("id"), Id) && FName(*Id) == C->CreatureId && O->TryGetObjectField(TEXT("look"), L)) { Look = *L; break; }
		}
	}
	if (!Look.IsValid()) return;
	// Bones the stand-in rig has that this one doesn't (Ignessa's wings under Nhal's veil).
	const TArray<TSharedPtr<FJsonValue>>* Hide = nullptr;
	if (Look->TryGetArrayField(TEXT("hideBones"), Hide))
	{
		for (const TSharedPtr<FJsonValue>& V : *Hide)
		{
			const FName B = C->FindBone(FName(*V->AsString()));
			if (!B.IsNone()) Body->HideBoneByName(B, EPhysBodyOp::PBO_None);
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* Tints = nullptr;
	UMaterialInterface* Master = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Creatures/M_CreatureTint.M_CreatureTint"));
	if (!Master || !Look->TryGetArrayField(TEXT("tint"), Tints) || Tints->Num() == 0) return;
	const TSharedPtr<FJsonObject> Pick = (*Tints)[0]->AsObject();
	if (!Pick.IsValid()) return;
	const auto Num = [&](const TCHAR* F, float D) { double X = 0.0; return Pick->TryGetNumberField(F, X) ? float(X) : D; };
	const auto Col = [&](const TCHAR* F, const FLinearColor& D) { FString S; return Pick->TryGetStringField(F, S) ? FLinearColor(FColor::FromHex(S)) : D; };
	for (int32 Slot = 0; Slot < Body->GetNumMaterials(); ++Slot)
	{
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Master, C);
		FString Path;
		if (UTexture* T = Pick->TryGetStringField(TEXT("tex"), Path) ? LoadObject<UTexture>(nullptr, *Path) : nullptr) M->SetTextureParameterValue(TEXT("BaseColorTex"), T);
		if (UTexture* T = LoadObject<UTexture>(nullptr, TEXT("/Engine/EngineMaterials/DefaultNormal.DefaultNormal"))) M->SetTextureParameterValue(TEXT("NormalTex"), T);
		if (UTexture* T = LoadObject<UTexture>(nullptr, TEXT("/Engine/EngineResources/Black.Black"))) M->SetTextureParameterValue(TEXT("GlowTex"), T);
		M->SetScalarParameterValue(TEXT("Desaturation"), Num(TEXT("desat"), 1.f));
		M->SetScalarParameterValue(TEXT("Brightness"), Num(TEXT("bright"), 1.f));
		M->SetScalarParameterValue(TEXT("TwoTone"), Num(TEXT("twoTone"), 0.f));
		M->SetScalarParameterValue(TEXT("LightFrom"), Num(TEXT("lightFrom"), 0.f));
		M->SetScalarParameterValue(TEXT("Roughness"), Num(TEXT("rough"), 0.75f));
		M->SetScalarParameterValue(TEXT("GlowStrength"), Num(TEXT("glowStrength"), 0.f));
		const FLinearColor Tint = Col(TEXT("tint"), FLinearColor::White);
		M->SetVectorParameterValue(TEXT("Tint"), Tint);
		M->SetVectorParameterValue(TEXT("TintLight"), Col(TEXT("light"), Tint));
		M->SetVectorParameterValue(TEXT("GlowColor"), Col(TEXT("glow"), GlGraveC()));
		Body->SetMaterial(Slot, M);
		TintMats.Add(M);
	}
}

void UWRGraveBossBody::BuildLook()
{
	using namespace WRLookKit;
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	const FWRCreatureDef* Def = C ? C->GetDef() : nullptr;
	USkeletalMeshComponent* Body = Skel();
	if (!Def || !Body || !Body->GetSkeletalMeshAsset()) return;
	ClearLook();
	LookFor = C->CreatureId;
	ApplyTint();
	const FName K = C->CreatureId;
	const auto BoneAt = [&](const TCHAR* Hint, FVector& Out) -> FName
	{
		const FName B = C->FindBone(FName(Hint));
		if (!B.IsNone()) Out = Body->GetBoneLocation(B);
		return B;
	};
	const auto Blocks = [&](TFunctionRef<void(FKit&)> Fn) { TArray<FWRLookBlock> Out; FKit Kit(FTransform::Identity, Out); Fn(Kit); return Out; };
	const FRotator Yaw(0.f, C->GetActorRotation().Yaw, 0.f);
	const FVector Fwd = Yaw.Vector(), Right = FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y);
	const float Rad = Def->Radius, HH = Def->HalfHeight;
	FVector Head = C->GetActorLocation() + FVector(0.f, 0.f, HH * 0.8f);
	const FName HeadBone = BoneAt(TEXT("head"), Head);
	FVector Spine = C->GetActorLocation();
	const FName SpineBone = BoneAt(TEXT("spine2"), Spine);
	FVector Hips = C->GetActorLocation();
	const FName HipBone = BoneAt(TEXT("hips"), Hips);
	FVector HandR = C->GetActorLocation() + Right * Rad;
	const FName HandRBone = BoneAt(TEXT("righthand"), HandR);
	FVector HandL = C->GetActorLocation() - Right * Rad;
	const FName HandLBone = BoneAt(TEXT("lefthand"), HandL);
	FVector ArmL = HandL;
	const FName ArmLBone = BoneAt(TEXT("leftforearm"), ArmL);
	const float Sc = HH / 210.f;   // the parts' scale to the boss's height (a 4.2 m figure = 1)

	if (K == TEXT("bell_ringer"))
	{
		// Half a cracked bronze bell for a helm (verdigris in the crack, a slot cut to see through), a barrel-sized
		// hand-bell at his belt (in his right hand once the rope is cut), the bell rope wrapped round his left forearm for
		// the Toll, a sexton's cassock in rags.
		AddGroup(HeadBone, Head, Yaw, Blocks([&](FKit& Kt)
		{
			const float R = 46.f * Sc;
			for (int32 s = 0; s < 10; ++s)
			{
				const float A = -100.f + 200.f * s / 9.f;
				const FVector D = FRotator(0.f, A, 0.f).Vector();
				Kt.Add(GlBronze(0.9f + 0.1f * (s % 2)), FVector(10.f * Sc, 2.f * PI * R / 16.f, 60.f * Sc), D * R * 0.95f + FVector(0.f, 0.f, 6.f * Sc), FRotator(-10.f, A, 0.f));
			}
			Kt.Add(GlBronze(0.85f), FVector(R * 1.6f, R * 1.6f, 14.f * Sc), FVector(0.f, 0.f, 36.f * Sc), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			Kt.Add(GlVerdigris(), FVector(4.f * Sc, 6.f * Sc, 56.f * Sc), FVector(R * 0.8f, R * 0.5f, 8.f * Sc), FRotator(0.f, 30.f, 12.f));
			Kt.Add(GlBlack(), FVector(4.f * Sc, R * 0.9f, 6.f * Sc), FVector(R * 0.97f, 0.f, 4.f * Sc));
			Kt.Add(GlBronze(0.7f), FVector(R * 0.5f, R * 0.5f, 10.f * Sc), FVector(0.f, 0.f, 46.f * Sc), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		}), BStatic);
		const auto HandBell = [&](FKit& Kt)
		{
			Kt.Add(GlBronze(), FVector(40.f, 40.f, 46.f) * Sc, FVector(0.f, 0.f, -28.f * Sc), FRotator::ZeroRotator, EWRLookMesh::Cone);
			Kt.Add(GlBronze(0.9f), FVector(46.f, 46.f, 6.f) * Sc, FVector(0.f, 0.f, -50.f * Sc), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			Kt.Add(GlOak(), FVector(8.f, 8.f, 34.f) * Sc, FVector(0.f, 0.f, 10.f * Sc), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		};
		AddGroup(HipBone, Hips + Right * Rad * 0.55f - FVector(0.f, 0.f, 10.f * Sc), Yaw, Blocks([&](FKit& Kt) { HandBell(Kt); Kt.Rod(GlSack(0.8f), FVector(0.f, 0.f, 30.f * Sc), FVector(-20.f * Sc, -30.f * Sc, 50.f * Sc), 4.f * Sc); }), BHandBellBelt);
		AddGroup(HandRBone, HandR, Yaw + FRotator(180.f, 0.f, 0.f), Blocks([&](FKit& Kt) { HandBell(Kt); }), BHandBellHand);
		AddGroup(ArmLBone, ArmL, Yaw, Blocks([&](FKit& Kt)
		{
			for (int32 c = 0; c < 5; ++c) Kt.Add(GlSurf::Cloth(GlRopeC()), FVector(18.f * Sc, 18.f * Sc, 4.f * Sc), FVector(c * 7.f * Sc, 0.f, 0.f), FRotator(90.f, 0.f, 12.f * c), EWRLookMesh::Cylinder);
		}), BRopeWrap);
		AddGroup(SpineBone, Spine, Yaw, Blocks([&](FKit& Kt)
		{
			for (int32 r = 0; r < 9; ++r)
			{
				const float A = 40.f * r;
				Kt.Add(GlSack(0.45f + 0.1f * (r % 3)), FVector(8.f * Sc, 40.f * Sc, (110.f + 30.f * (r % 3)) * Sc), FRotator(0.f, A, 0.f).Vector() * Rad * 0.55f - FVector(0.f, 0.f, 60.f * Sc), FRotator(0.f, A + 90.f, 6.f));
			}
		}), BStatic);
	}
	else if (K == TEXT("crypt_mother"))
	{
		// Two 7 m whips, the palps, and her living cloak: about twenty pale young clinging to her flat back (Count shows how
		// many are left; fire on her back and the shed take them off).
		for (const float S : { -1.f, 1.f })
		{
			AddGroup(HeadBone, Head + Right * S * Rad * 0.1f, Yaw + FRotator(5.f, S * 35.f, 0.f), Blocks([&](FKit& Kt)
			{
				float X = 0.f;
				const float Seg = 700.f / 16.f;
				for (int32 s = 0; s < 16; ++s)
				{
					const float Rr = FMath::Lerp(7.f, 1.5f, s / 15.f);
					Kt.Add(GlSurf::Stone(GlRgb(74, 68, 60) * 0.6f, 0.7f), FVector(Rr * 2.f, Rr * 2.f, Seg * 1.05f), FVector(X + Seg * 0.5f, 0.f, -s * s * 0.5f), FRotator(90.f + s * 1.2f, 0.f, 0.f), EWRLookMesh::Cylinder);
					X += Seg;
				}
			}), BWhip);
			AddGroup(HeadBone, Head + Right * S * Rad * 0.2f, Yaw + FRotator(-10.f, S * 12.f, 0.f), Blocks([&](FKit& Kt)
			{
				Kt.Add(GlSurf::Stone(GlRgb(74, 68, 60) * 0.55f, 0.6f), FVector(Rad * 0.45f, 30.f, 30.f), FVector(Rad * 0.2f, 0.f, 0.f));
				Kt.Add(GlSurf::Stone(GlRgb(74, 68, 60) * 0.55f, 0.6f), FVector(Rad * 0.4f, 24.f, 24.f), FVector(Rad * 0.25f, -S * 30.f, -16.f), FRotator(0.f, S * 150.f, 0.f));
				for (int32 sp = 0; sp < 7; ++sp) Kt.Spike(GlBone(), FVector(Rad * (0.05f + 0.06f * sp), 0.f, 14.f), FVector(0.f, -S * 0.4f, 1.f), 8.f, 34.f);
			}), BStatic);
		}
		for (int32 y = 0; y < 20; ++y)
		{
			const float A = y * 2.399f, D = Rad * 0.55f * FMath::Sqrt((y + 0.5f) / 20.f);
			const FVector P = Spine + Fwd * FMath::Cos(A) * D + Right * FMath::Sin(A) * D + FVector(0.f, 0.f, HH * 0.75f);
			AddGroup(SpineBone, P, Yaw + FRotator(0.f, 360.f * GlHash(y, 4), 0.f), Blocks([&](FKit& Kt)
			{
				Kt.Add(GlSurf::Stone(GlRgb(200, 194, 182) * 0.62f, 0.7f), FVector(40.f, 26.f, 10.f), FVector::ZeroVector, FRotator::ZeroRotator, EWRLookMesh::Cylinder);
				for (int32 l = 0; l < 6; ++l) Kt.Rod(GlSurf::Stone(GlRgb(200, 194, 182) * 0.55f, 0.7f), FVector((l / 2 - 1) * 12.f, (l % 2 ? 1.f : -1.f) * 10.f, 0.f), FVector((l / 2 - 1) * 16.f, (l % 2 ? 1.f : -1.f) * 30.f, -8.f), 3.f);
			}), BRider + y);
		}
	}
	else if (K == TEXT("ossuary_lord"))
	{
		// The bone giant: thigh bones bound along its limbs, ribcages stacked on its chest, a crown of skulls, three
		// keystone skulls crusted in soulstone (brow, chest, spine) that glow. Collapsed: its pile, with the keystones where
		// the blows go (FWRGraveBoss keystone points), dark once smashed.
		const GlSurf Crust = GlSoulstone(3.f);
		AddGroup(HeadBone, Head, Yaw, Blocks([&](FKit& Kt)
		{
			for (int32 s = 0; s < 9; ++s) GlSkull(Kt, FRotator(0.f, 40.f * s, 0.f).Vector() * 34.f * Sc + FVector(0.f, 0.f, 40.f * Sc), 1.4f * Sc, 40.f * s);
		}), BBody);
		AddGroup(HeadBone, Head + Fwd * 30.f * Sc, Yaw, Blocks([&](FKit& Kt) { GlSkull(Kt, FVector::ZeroVector, 2.2f * Sc, 0.f, 2.f, &Crust); }), BKeystone + 0);
		AddGroup(SpineBone, Spine + Fwd * Rad * 0.55f, Yaw, Blocks([&](FKit& Kt)
		{
			for (int32 r = 0; r < 4; ++r) for (int32 rib = 0; rib < 5; ++rib) for (const float S : { -1.f, 1.f })
			{
				Kt.Rod(GlBone(0.9f), FVector(-10.f, S * 6.f, (-60.f + r * 40.f + rib * 7.f) * Sc), FVector(10.f * Sc, S * 50.f * Sc, (-70.f + r * 40.f + rib * 7.f) * Sc), 4.f * Sc);
			}
			GlSkull(Kt, FVector(14.f * Sc, 0.f, 10.f * Sc), 2.f * Sc, 0.f, 2.f, &Crust);
		}), BKeystone + 1);
		AddGroup(SpineBone, Spine - Fwd * Rad * 0.5f, Yaw + FRotator(0.f, 180.f, 0.f), Blocks([&](FKit& Kt) { GlSkull(Kt, FVector::ZeroVector, 2.f * Sc, 0.f, 2.f, &Crust); }), BKeystone + 2);
		static const TCHAR* const Limbs[4][2] = { { TEXT("leftarm"), TEXT("leftforearm") }, { TEXT("rightarm"), TEXT("rightforearm") }, { TEXT("leftupleg"), TEXT("leftleg") }, { TEXT("rightupleg"), TEXT("rightleg") } };
		for (int32 l = 0; l < 4; ++l)
		{
			FVector A, B;
			const FName Ba = BoneAt(Limbs[l][0], A), Bb = BoneAt(Limbs[l][1], B);
			if (Ba.IsNone() || Bb.IsNone()) continue;
			const float Len = float(FVector::Dist(A, B));
			AddGroup(Ba, A, FRotationMatrix::MakeFromX(B - A).Rotator(), Blocks([&](FKit& Kt)
			{
				for (int32 f = 0; f < 4; ++f)
				{
					const FVector O(0.f, FMath::Cos(f * PI * 0.5f) * 16.f * Sc, FMath::Sin(f * PI * 0.5f) * 16.f * Sc);
					GlLongBone(Kt, O, O + FVector(Len * 0.95f, 0.f, 0.f), 6.f * Sc, GlBone(0.85f + 0.05f * f));
				}
				for (int32 b = 1; b < 4; ++b) Kt.Add(GlSurf::Cloth(GlRopeC() * 0.7f), FVector(5.f * Sc, 46.f * Sc, 46.f * Sc), FVector(Len * b / 4.f, 0.f, 0.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			}), BBody);
		}
		// The pile (shown collapsed): a mound of bones 3.5 m across, the keystones in it at the points the blows find.
		for (int32 k = 0; k < 3; ++k)
		{
			const FVector At = C->GetActorLocation() - FVector(0.f, 0.f, HH) + FRotator(0.f, Yaw.Yaw + 120.f * k, 0.f).Vector() * 160.f + FVector(0.f, 0.f, 40.f);
			AddGroup(NAME_None, At, Yaw, Blocks([&](FKit& Kt) { GlSkull(Kt, FVector::ZeroVector, 2.2f * Sc, 120.f * k, 2.f, &Crust); }), BPile + 0);
		}
		AddGroup(NAME_None, C->GetActorLocation() - FVector(0.f, 0.f, HH), Yaw, Blocks([&](FKit& Kt)
		{
			for (int32 b = 0; b < 70; ++b)
			{
				const float A = b * 2.399f, D = FMath::Sqrt((b + 0.5f) / 70.f) * 180.f;
				const FVector P(FMath::Cos(A) * D, FMath::Sin(A) * D, (1.f - D / 180.f) * 90.f);
				GlLongBone(Kt, P, P + FRotator(30.f * GlHash(b, 1) - 15.f, 360.f * GlHash(b, 2), 0.f).Vector() * 55.f, 4.f, GlBone(0.85f + 0.15f * GlHash(b, 3)));
			}
			for (int32 s = 0; s < 14; ++s) GlSkull(Kt, FRotator(0.f, s * 51.f, 0.f).Vector() * (40.f + 9.f * s) + FVector(0.f, 0.f, 80.f - 5.f * s), 1.5f, s * 70.f);
		}), BPile + 0);
		// The surge (a wave of bone it travels in): tumbling bones round it, rolled in TickLook.
		AddGroup(NAME_None, C->GetActorLocation() - FVector(0.f, 0.f, HH * 0.6f), Yaw, Blocks([&](FKit& Kt)
		{
			for (int32 b = 0; b < 60; ++b)
			{
				const float A = b * 2.399f;
				const FVector P(FMath::Cos(A) * 60.f * (b % 4), FMath::Sin(A) * 220.f * GlHash(b, 4), 40.f + 90.f * GlHash(b, 5));
				GlLongBone(Kt, P, P + FRotator(360.f * GlHash(b, 6), 360.f * GlHash(b, 7), 0.f).Vector() * 50.f, 4.f, GlBone(0.9f));
			}
		}), BWave);
	}
	else if (K == TEXT("gravedigger"))
	{
		// A horse skull under a white sheet for a head (the Mari Lwyd), an oilskin coat, a 2.5 m iron spade, a coffin-shaped
		// mortsafe cage strapped to his back (its door on hinges), a lantern on a pole off the cage over his left shoulder.
		AddGroup(HeadBone, Head, Yaw, Blocks([&](FKit& Kt)
		{
			const float S = Sc;
			Kt.Add(GlBone(), FVector(70.f, 28.f, 30.f) * S, FVector(40.f * S, 0.f, 4.f * S));                         // the long face
			Kt.Add(GlBone(0.95f), FVector(40.f, 34.f, 34.f) * S, FVector(0.f, 0.f, 10.f * S), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			for (const float Y : { -1.f, 1.f }) Kt.Add(GlGrave(3.f), FVector(8.f, 8.f, 8.f) * S, FVector(18.f * S, Y * 15.f * S, 16.f * S));
			for (int32 t = 0; t < 8; ++t) Kt.Add(GlBone(0.85f), FVector(3.f, 3.f, 8.f) * S, FVector((50.f + 6.f * t) * S, (t % 2 ? 7.f : -7.f) * S, -10.f * S));
			Kt.Add(GlBone(0.9f), FVector(64.f, 22.f, 8.f) * S, FVector(42.f * S, 0.f, -18.f * S), FRotator(-8.f, 0.f, 0.f));
		}), BStatic);
		AddGroup(HeadBone, Head, Yaw, Blocks([&](FKit& Kt)
		{
			// The sheet falls from the crown over the skull's back and shoulders to mid-chest, its hem ragged.
			for (int32 p = 0; p < 10; ++p)
			{
				const float A = 180.f + (p - 4.5f) * 26.f;
				Kt.Add(GlLinen(0.9f + 0.1f * (p % 2)), FVector(4.f * Sc, 30.f * Sc, (120.f + 25.f * (p % 3)) * Sc), FRotator(0.f, A, 0.f).Vector() * 30.f * Sc - FVector(0.f, 0.f, 50.f * Sc), FRotator(-8.f, A + 90.f, 0.f));
			}
		}), BSheet);
		AddGroup(HandRBone, HandR, Yaw + FRotator(-70.f, 0.f, 0.f), Blocks([&](FKit& Kt)
		{
			Kt.Rod(GlOak(0.8f), FVector(0.f, 0.f, -60.f * Sc), FVector(0.f, 0.f, 180.f * Sc), 8.f * Sc);
			Kt.Add(GlIron(), FVector(40.f, 6.f, 14.f) * Sc, FVector(0.f, 0.f, 190.f * Sc));
			Kt.Add(GlIron(1.2f), FVector(54.f, 4.f, 70.f) * Sc, FVector(0.f, 0.f, 230.f * Sc));
			Kt.Add(GlRust(), FVector(52.f, 4.5f, 18.f) * Sc, FVector(0.f, 0.f, 258.f * Sc));
			Kt.Add(GlOak(0.7f), FVector(30.f, 8.f, 8.f) * Sc, FVector(0.f, 0.f, -66.f * Sc));
		}), BStatic);
		const FVector Back = Spine - Fwd * Rad * 1.05f;
		AddGroup(SpineBone, Back, Yaw, Blocks([&](FKit& Kt)
		{
			// The cage: a coffin of iron bars 2.1 m tall (hexagonal plan), straps over his shoulders.
			const float H = 210.f * Sc, W = 70.f * Sc, D = 50.f * Sc;
			const FVector2D Plan[6] = { FVector2D(0.f, -W * 0.5f), FVector2D(-D * 0.5f, -W * 0.35f), FVector2D(-D, -W * 0.2f), FVector2D(-D, W * 0.2f), FVector2D(-D * 0.5f, W * 0.35f), FVector2D(0.f, W * 0.5f) };
			for (int32 i = 0; i < 6; ++i)
			{
				const FVector2D P0 = Plan[i], P1 = Plan[(i + 1) % 6];
				for (int32 b = 0; b < 4; ++b)
				{
					const FVector2D P = FMath::Lerp(P0, P1, b / 4.f);
					Kt.Rod(GlIron(), FVector(P.X, P.Y, -H * 0.5f), FVector(P.X, P.Y, H * 0.5f), 2.5f * Sc);
				}
				for (const float Z : { -H * 0.5f, 0.f, H * 0.5f }) Kt.Rod(GlRust(), FVector(P0.X, P0.Y, Z), FVector(P1.X, P1.Y, Z), 3.f * Sc, true);
			}
			for (const float Y : { -W * 0.4f, W * 0.4f }) Kt.Rod(GlSurf::Cloth(GlRgb(60, 44, 30) * 0.5f), FVector(0.f, Y, H * 0.3f), FVector(Rad * 0.9f, Y * 0.6f, H * 0.4f), 6.f * Sc, true);
			// The lantern's pole up off the cage's head, over his left shoulder.
			Kt.Rod(GlOak(0.7f), FVector(-D * 0.5f, -W * 0.3f, H * 0.4f), FVector(-D * 0.1f, -W * 0.6f, H * 0.95f), 4.f * Sc);
		}), BStatic);
		{
			// The cage door (the outward face): bars on a frame, its hinge the bar at the face's left edge (the group's
			// origin, so TickLook swings it open about the hinge).
			const float H = 210.f * Sc, W = 70.f * Sc, D = 50.f * Sc;
			AddGroup(SpineBone, Back + Yaw.RotateVector(FVector(-D - 2.f, -W * 0.2f, 0.f)), Yaw, Blocks([&](FKit& Kt)
			{
				for (int32 b = 0; b < 5; ++b) Kt.Rod(GlIron(), FVector(0.f, W * 0.1f * b, -H * 0.45f), FVector(0.f, W * 0.1f * b, H * 0.45f), 2.5f * Sc);
				for (const float Z : { -H * 0.45f, 0.f, H * 0.45f }) Kt.Rod(GlRust(), FVector(0.f, 0.f, Z), FVector(0.f, W * 0.4f, Z), 3.f * Sc, true);
				Kt.Add(GlIron(1.3f), FVector(6.f, 4.f, 10.f) * Sc, FVector(-3.f * Sc, W * 0.38f, 0.f));   // the hasp
			}), BCageDoor);
		}
		const FVector LanternAt = C->GetActorLocation() + Yaw.RotateVector(FVector(-Rad * 1.2f, -Rad * 0.6f, HH * 1.25f));
		if (USceneComponent* Lantern = AddGroup(SpineBone, LanternAt, Yaw, Blocks([&](FKit& Kt)
		{
			Kt.Add(GlIron(), FVector(24.f, 24.f, 4.f) * Sc, FVector(0.f, 0.f, 18.f * Sc));
			for (int32 c = 0; c < 4; ++c) Kt.Rod(GlIron(), FVector(c % 2 ? 11.f : -11.f, c < 2 ? -11.f : 11.f, 16.f) * Sc, FVector(c % 2 ? 11.f : -11.f, c < 2 ? -11.f : 11.f, -16.f) * Sc, 2.f * Sc);
			Kt.Add(GlCandle(6.f), FVector(18.f, 18.f, 26.f) * Sc, FVector::ZeroVector);
			Kt.Add(GlIron(), FVector(26.f, 26.f, 4.f) * Sc, FVector(0.f, 0.f, -18.f * Sc));
		}), BLantern))
		{
			UPointLightComponent* L = NewObject<UPointLightComponent>(C);
			L->SetupAttachment(Lantern);
			L->SetLightColor(GlCandleC());
			L->SetIntensity(5000.f);
			L->SetAttenuationRadius(1600.f);
			L->SetCastShadows(false);
			L->RegisterComponent();
			Lights.Add(L);
		}
	}
	else if (K == TEXT("nhal"))
	{
		// The moth-veil (thousands of roosting Wraithmoths: a layered cone of moth cards from her crown to the floor, that
		// flutters), a crown of tōrō lantern-horns, the 8 m scythe (black yew snath, a bell-bronze blade).
		AddGroup(HeadBone, Head, Yaw, Blocks([&](FKit& Kt)
		{
			for (int32 h = 0; h < 7; ++h)
			{
				const float A = -75.f + 25.f * h;
				const FVector Base = FRotator(0.f, A + 180.f, 0.f).Vector() * 16.f * Sc + FVector(0.f, 0.f, 20.f * Sc);
				const FVector Tip = Base + FRotator(55.f, A + 180.f, 0.f).Vector() * (60.f + 15.f * (h % 2)) * Sc;
				Kt.Rod(GlYew(), Base, Tip, 5.f * Sc);
				Kt.Add(GlStone(0.75f), FVector(14.f, 14.f, 3.f) * Sc, Tip + FVector(0.f, 0.f, 4.f * Sc));
				Kt.Add(GlGrave(5.f), FVector(10.f, 10.f, 11.f) * Sc, Tip + FVector(0.f, 0.f, 11.f * Sc));
				Kt.Add(GlStone(0.75f), FVector(18.f, 18.f, 4.f) * Sc, Tip + FVector(0.f, 0.f, 19.f * Sc));
			}
		}), BCrown);
		const float VeilH = HH * 2.f;
		for (int32 ring = 0; ring < 9; ++ring)
		{
			// Each ring of the veil a frame on the head (it hangs from the crown); the moths are cards round it.
			const float Z = -ring * VeilH / 9.f;
			const float R = 22.f * Sc + ring * (Rad * 0.95f - 22.f * Sc) / 8.f;
			const int32 N = 12 + ring * 3;
			AddGroup(HeadBone, Head + FVector(0.f, 0.f, Z + 30.f * Sc), Yaw, Blocks([&](FKit& Kt)
			{
				for (int32 m = 0; m < N; ++m)
				{
					const float A = 360.f * (m + 0.5f * (ring % 2)) / N;
					const FVector D = FRotator(0.f, A, 0.f).Vector();
					// Skip a slit at the face on the upper rings (where the veil parts for the Keen).
					if (ring < 3 && FMath::Abs(FMath::FindDeltaAngleDegrees(A, 0.f)) < 18.f) continue;
					Kt.Add(m % 5 == 0 ? GlBruise() : GlMoth(0.8f + 0.4f * GlHash(m, ring)), FVector(2.f, 22.f * Sc, 16.f * Sc), D * R, FRotator(-25.f + 10.f * GlHash(m, ring + 3), A + 90.f, 0.f));
					if (m % 9 == 4) Kt.Add(GlBone(0.9f), FVector(2.5f, 5.f * Sc, 5.f * Sc), D * (R + 2.f), FRotator(-25.f, A + 90.f, 0.f));
				}
			}), BVeil);
		}
		// The veil lifted (the Opening): the whole swarm on the roof ribs over her, a slow wheeling cloud (TickLook).
		AddGroup(NAME_None, C->GetActorLocation() + FVector(0.f, 0.f, 1100.f), Yaw, Blocks([&](FKit& Kt)
		{
			for (int32 m = 0; m < 260; ++m)
			{
				const float A = m * 2.399f, D = FMath::Sqrt((m + 0.5f) / 260.f) * 900.f;
				Kt.Add(m % 6 == 0 ? GlBruise(0.5f) : GlMoth(0.7f + 0.5f * GlHash(m, 2)), FVector(2.f, 26.f, 18.f), FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, 120.f * GlHash(m, 7) - 60.f), FRotator(80.f * GlHash(m, 5) - 40.f, 360.f * GlHash(m, 6), 0.f));
			}
		}), BVeilUp);
		AddGroup(HeadBone, Head + Fwd * 18.f * Sc - FVector(0.f, 0.f, 16.f * Sc), Yaw, Blocks([&](FKit& Kt)
		{
			// The unhinged jaw (phase 2): her lower face hanging open to the chest, a dark throat with grave-light in it.
			Kt.Add(GlBlack(), FVector(20.f, 26.f, 60.f) * Sc, FVector(0.f, 0.f, -30.f * Sc));
			Kt.Add(GlGrave(2.5f), FVector(8.f, 12.f, 30.f) * Sc, FVector(-6.f * Sc, 0.f, -36.f * Sc));
			Kt.Add(GlBone(0.85f), FVector(10.f, 28.f, 8.f) * Sc, FVector(4.f * Sc, 0.f, -62.f * Sc));
			for (int32 t = 0; t < 8; ++t) Kt.Add(GlBone(0.9f), FVector(3.f, 3.f, 7.f) * Sc, FVector(8.f * Sc, (-10.f + 3.f * t) * Sc, -56.f * Sc));
		}), BJaw);
		AddGroup(HandRBone, HandR, Yaw + FRotator(-60.f, 0.f, 0.f), Blocks([&](FKit& Kt)
		{
			// The snath 8 m of black yew, a grip at the hand and one a third up; the blade 3 m of bell bronze, verdigris on
			// its back, hung from the snath's head with an iron ring.
			const float L = 800.f * Sc * 0.5f;
			Kt.Rod(GlYew(), FVector(0.f, 0.f, -L * 0.25f), FVector(0.f, 0.f, L), 9.f * Sc);
			Kt.Rod(GlYew(0.8f), FVector(0.f, 0.f, L * 0.3f), FVector(22.f * Sc, 0.f, L * 0.3f), 6.f * Sc);
			Kt.Add(GlIron(), FVector(16.f, 16.f, 10.f) * Sc, FVector(0.f, 0.f, L), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			for (int32 s = 0; s < 8; ++s)
			{
				const float T = s / 8.f;
				const FVector P(18.f * Sc + 260.f * Sc * T, 0.f, L - 8.f * Sc - 70.f * Sc * T * T);
				Kt.Add(GlBronze(1.1f), FVector(36.f * Sc, 3.f * Sc, (34.f - 22.f * T) * Sc), P, FRotator(-14.f * T * 4.f, 0.f, 0.f));
				Kt.Add(GlVerdigris(0.9f), FVector(36.f * Sc, 3.5f * Sc, 6.f * Sc), P + FVector(0.f, 0.f, (17.f - 11.f * T) * Sc), FRotator(-14.f * T * 4.f, 0.f, 0.f));
			}
		}), BStatic);
		if (USceneComponent* G = Groups.Num() ? Groups[0].Frame.Get() : nullptr)
		{
			UPointLightComponent* L = NewObject<UPointLightComponent>(C);
			L->SetupAttachment(G);
			L->SetRelativeLocation(FVector(0.f, 0.f, 60.f * Sc));
			L->SetLightColor(GlGraveC());
			L->SetIntensity(6000.f);
			L->SetAttenuationRadius(2400.f);
			L->SetCastShadows(false);
			L->RegisterComponent();
			Lights.Add(L);
		}
	}
	ShownCount = ShownVeil = ShownPose = 255;
	bShownBroken = !bBroken;
}

void UWRGraveBossBody::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (GetNetMode() == NM_DedicatedServer) return;
	Clock += DeltaTime;
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	USkeletalMeshComponent* Body = Skel();
	if (!C || !Body) return;
	if (LookFor != C->CreatureId)
	{
		// The rig loads after the actor: try a few times until its bones are there.
		if (LookTries++ % 15 == 0 && Body->GetSkeletalMeshAsset()) BuildLook();
		if (LookFor != C->CreatureId) return;
	}
	const bool bAlive = C->IsAlive();
	// Show and hide parts by state.
	if (ShownCount != Count || ShownVeil != Veil || ShownPose != Pose || bShownBroken != bBroken)
	{
		ShownCount = Count;
		ShownVeil = Veil;
		ShownPose = Pose;
		bShownBroken = bBroken;
		const bool bPile = C->CreatureId == TEXT("ossuary_lord") && Pose == 1;
		const bool bWave = C->CreatureId == TEXT("ossuary_lord") && Pose == 2;
		for (const FGroup& G : Groups)
		{
			USceneComponent* F = G.Frame.Get();
			if (!F) continue;
			bool bShow = true;
			if (G.Kind == BHandBellBelt) bShow = !bBroken && Pose != 2;
			else if (G.Kind == BHandBellHand) bShow = bBroken || Pose == 2;
			else if (G.Kind == BRopeWrap) bShow = Pose == 1 && !bBroken;
			else if (G.Kind >= BRider && G.Kind < BRider + 30) bShow = (G.Kind - BRider) < Count;
			else if (G.Kind >= BKeystone && G.Kind < BKeystone + 3) bShow = !bPile && !bWave && (Count & (1 << (G.Kind - BKeystone))) == 0;
			else if (G.Kind == BBody) bShow = !bPile && !bWave;
			else if (G.Kind == BPile) bShow = bPile;
			else if (G.Kind == BWave) bShow = bWave;
			else if (G.Kind == BLantern) bShow = true;
			else if (G.Kind == BVeil) bShow = Veil == 0;
			else if (G.Kind == BVeilUp) bShow = Veil == 1 || Veil == 2;
			else if (G.Kind == BJaw) bShow = Veil == 2;
			F->SetVisibility(bShow, true);
		}
		// The bone giant's own mesh hides while it lies as its pile or rolls as a wave.
		const bool bHideMesh = bPile || bWave;
		if (bHideMesh != bMeshHidden) { Body->SetVisibility(!bHideMesh, false); bMeshHidden = bHideMesh; }
		// The Gravedigger's lantern: lit, or smashed dark.
		if (C->CreatureId == TEXT("gravedigger")) for (UPointLightComponent* L : Lights) if (L) L->SetIntensity(bBroken ? 0.f : 5000.f);
		// The pile's keystones: a smashed one goes dark (the crust cracked off).
		if (bPile) for (const FGroup& G : Groups) if (G.Kind == BPile && G.Frame.IsValid()) G.Frame->SetVisibility(true, true);
		// Nhal's shade (phase 3): the body goes dark and glows faintly.
		if (C->CreatureId == TEXT("nhal")) for (UMaterialInstanceDynamic* M : TintMats) if (M) { M->SetScalarParameterValue(TEXT("Brightness"), Veil == 3 ? 0.45f : 1.2f); M->SetScalarParameterValue(TEXT("GlowStrength"), Veil == 3 ? 1.5f : 0.f); }
	}
	// Motion: whips sway, the veil flutters, the swarm wheels on the ribs, the cage door swings, the wave tumbles,
	// the shade kneels.
	for (FGroup& G : Groups)
	{
		USceneComponent* F = G.Frame.Get();
		if (!F || !F->IsVisible()) continue;
		if (G.Kind == BWhip) F->SetRelativeRotation(G.BaseRot + FRotator(4.f * FMath::Sin(Clock * 1.7f + G.Phase), 9.f * FMath::Sin(Clock * 1.1f + G.Phase), 0.f));
		else if (G.Kind == BVeil) F->SetRelativeRotation(G.BaseRot + FRotator(0.f, 1.5f * FMath::Sin(Clock * 3.f + G.Phase), 2.f * FMath::Sin(Clock * 5.f + G.Phase)));
		else if (G.Kind == BVeilUp) F->SetWorldRotation(FRotator(0.f, Clock * 9.f, 0.f));
		else if (G.Kind == BCageDoor)
		{
			DoorAngle = FMath::FInterpTo(DoorAngle, Pose == 3 ? -80.f : 0.f, DeltaTime, 5.f);
			F->SetRelativeRotation(G.BaseRot + FRotator(0.f, DoorAngle, 0.f));
		}
		else if (G.Kind == BWave) F->SetRelativeRotation(G.BaseRot + FRotator(0.f, 0.f, Clock * 220.f));
		else if (G.Kind >= BRider && G.Kind < BRider + 30) F->SetRelativeLocation(G.BaseLoc + FVector(0.f, 0.f, 2.f * FMath::Sin(Clock * 6.f + G.Phase)));
		else if (G.Kind == BLantern) F->SetRelativeRotation(G.BaseRot + FRotator(6.f * FMath::Sin(Clock * 2.1f + G.Phase), 0.f, 4.f * FMath::Sin(Clock * 1.6f)));
	}
	// Kneeling (Nhal's Procession): the body sinks to its knees over a second and rises again after.
	if (C->CreatureId == TEXT("nhal"))
	{
		const float Target = Pose == 2 ? -C->GetSimpleCollisionHalfHeight() * 0.35f : 0.f;
		Kneel = FMath::FInterpTo(Kneel, Target, DeltaTime, 2.f);
		FVector Rel = Body->GetRelativeLocation();
		Rel.Z += Kneel - KneelShown;
		KneelShown = Kneel;
		Body->SetRelativeLocation(Rel);
	}
	(void)bAlive;
}
