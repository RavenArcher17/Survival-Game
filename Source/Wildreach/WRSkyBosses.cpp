// The Sky islands' bosses and arenas (see WRSkyBosses.h; cooks/biome-sky.md 2.10-2.11, build-biome-sky.md 2.12-2.13).

#include "WRSkyBosses.h"
#include "WRFungalHazards.h"
#include "WRFungalTypes.h"
#include "Wildreach.h"
#include "WRAltar.h"
#include "WRArtifacts.h"
#include "WRAttributeSet.h"
#include "WRBoss.h"
#include "WRBuildLook.h"
#include "WRBuildLookKit.h"
#include "WRCharacter.h"
#include "WRCombatLibrary.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRGameState.h"
#include "WRGameplayTags.h"
#include "WRInventory.h"
#include "WRPlayerController.h"
#include "WRSkyAir.h"
#include "WRSkyCreatures.h"
#include "WRSkyIsleTypes.h"
#include "WRSkyWorld.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRSurvival.h"
#include "WRTuning.h"
#include "WRToolMesh.h"
#include "WRProjectile.h"
#include "WRPlayerState.h"
#include "ProceduralMeshComponent.h"
#include "AbilitySystemComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "WRStrikeFront.h"

namespace WRSkyBossImpl
{
	double SbNow(const UObject* W) { const AGameStateBase* GS = W && W->GetWorld() ? W->GetWorld()->GetGameState() : nullptr; return GS ? GS->GetServerWorldTimeSeconds() : (W && W->GetWorld() ? W->GetWorld()->GetTimeSeconds() : 0.0); }
	double SbLocal(const UObject* W) { return W && W->GetWorld() ? W->GetWorld()->GetTimeSeconds() : 0.0; }
	float SbT(const AWRBoss* B, const TCHAR* Key, float Fallback) { return FWRTuning::Get().Num(FString::Printf(TEXT("bosses.%s.%s"), *B->CreatureId.ToString(), Key), Fallback); }
	float SbAeo(const TCHAR* Key, float Fallback) { return FWRTuning::Get().Num(FString::Printf(TEXT("bosses.aeolith.%s"), Key), Fallback); }
	FLinearColor SbHex(const TCHAR* S) { return FLinearColor(FColor::FromHex(S)); }
	/** What each of the four is doing (server). */
	struct FSbState
	{
		// A grab carrying someone (Kyrra's and Aeolith's snatch) or swallowed (Gulper).
		TWeakObjectPtr<AWRCharacterBase> Held;
		FName HeldHow;
		double HeldUntil = 0.0, HeldTickAt = 0.0;
		FVector HeldFrom = FVector::ZeroVector;
		float FreeStance = 0.f;
		// Kyrra.
		int32 RouteIdx = 0;
		double NextLandAt = 0.0, LandUntil = 0.0;
		FVector LandAt = FVector::ZeroVector;
		bool bScreeched = false;
		// Her stoop (Aaron 11-16, r-kyrra: "Try it, make it good"): where the wind-up began, the fold point she climbs to
		// (up and back off the line, as a peregrine climbs before it stoops), and whether she has folded yet.
		FVector StoopFrom = FVector::ZeroVector, StoopFold = FVector::ZeroVector;
		bool bStooping = false, bStoopFolded = false;
		// The Sentinel.
		bool bAwake = false, bFell = false, bFalling = false;
		double KneelUntil = 0.0;
		double NextSpinTick = 0.0;
		// Gulper.
		double ChokeUntil = 0.0, LastSwallowAt = -100.0;
		TArray<double> Chokes;
		bool bCracked = false;
		// Aeolith.
		float Spore = 0.f;
		double CrownOffUntil = 0.0, WeightedUntil = 0.0, ChestOpenUntil = 0.0, LastPlumeHit = -100.0;
		int32 LastPlumeCycle = -1;
		float CircleA = 0.f;
		double LiftUntil = 0.0;
		TArray<TWeakObjectPtr<AWRCharacterBase>> Lifted;
		double NextPassAt = 0.0, PassUntil = 0.0, NextRollAt = 0.0, RollAt = 0.0, CrashUntil = 0.0;
		float Vent[3] = { 0.f, 0.f, 0.f };
		bool bVentsUp = false, bRegrown = false;
		TArray<TWeakObjectPtr<AWRCharacterBase>> Riders;
		TArray<int32> RiderSlot;
		// Any: a fall off its ground recovering onto the nearest island (bosses never die of a fall).
		double OffSince = 0.0;
	};
	TMap<TWeakObjectPtr<const AWRBoss>, FSbState>& States() { static TMap<TWeakObjectPtr<const AWRBoss>, FSbState> M; return M; }
	FSbState& St(const AWRBoss* B) { return States().FindOrAdd(B); }
	TArray<AWRCharacterBase*> Foes(const AWRBoss* B, float Radius)
	{
		TArray<AWRCharacterBase*> Out;
		for (TActorIterator<AWRCharacterBase> It(B->GetWorld()); It; ++It)
		{
			if (*It != B && It->IsAlive() && B->IsHostileTo(*It) && FVector::Dist(It->GetActorLocation(), B->GetActorLocation()) < Radius) Out.Add(*It);
		}
		return Out;
	}
	FWRHit SbHit(AWRBoss* B, float Damage, bool bUnblockable)
	{
		FWRHit Hit;
		Hit.Instigator = B;
		Hit.Damage = Damage;
		Hit.PoiseDamage = Damage * 2.f;
		Hit.bUnblockable = bUnblockable;
		Hit.bHeavy = true;
		Hit.Impact = 4;
		Hit.Tier = EWRHitTier::Heavy;
		Hit.Origin = B->GetActorLocation();
		Hit.Element = WRTags::Element_Physical;
		return Hit;
	}
	void Tell(AActor* Who, const FString& Text)
	{
		const APawn* P = Cast<APawn>(Who);
		if (AWRPlayerController* PC = P ? Cast<AWRPlayerController>(P->GetController()) : nullptr) PC->ClientNotify(Text);
	}
	TWeakObjectPtr<AWRSkyRing>& RingRef() { static TWeakObjectPtr<AWRSkyRing> R; return R; }
	constexpr int32 RingPlatforms = 8;
}
using namespace WRSkyBossImpl;

// ================================================================================================ the Ring

AWRSkyRing::AWRSkyRing()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void AWRSkyRing::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRSkyRing, UnboundAt);
}

float AWRSkyRing::ServerNow() const { return float(SbNow(this)); }
int32 AWRSkyRing::Count() const { return FMath::Min(RingPlatforms, FWRSkyIsleData::Get().Platforms.Num() > 0 ? FWRSkyIsleData::Get().Platforms.Num() : RingPlatforms); }

float AWRSkyRing::PlatformRadius(int32 i) const
{
	const TArray<FWRSkyIsleData::FPlatform>& P = FWRSkyIsleData::Get().Platforms;
	return (P.IsValidIndex(i) ? P[i].Size : 1600.f) * 0.5f;
}

FVector AWRSkyRing::PlatformAt(int32 i, float Now) const
{
	// Bound: fixed 28 m out round the axis, 4 m gaps. Unbound: each orbits on its own (gaps 4-14 m on a 20 s cycle, the
	// whole ring turning slowly), eased in over 4 s from where it stood.
	const TArray<FWRSkyIsleData::FPlatform>& P = FWRSkyIsleData::Get().Platforms;
	const float A0 = P.IsValidIndex(i) ? FMath::DegreesToRadians(P[i].Angle) : 2.f * PI * i / RingPlatforms;
	const float R0 = FWRSkyIsleData::Get().RingRadius;
	float A = A0, Rr = R0, Z = 0.f;
	if (UnboundAt > 0.f && Now > UnboundAt)
	{
		const float T = Now - UnboundAt;
		const float Ease = FMath::SmoothStep(0.f, 4.f, T);
		const float Period = SbAeo(TEXT("driftPeriod"), 20.f);
		const float Gap = FMath::Lerp(SbAeo(TEXT("gapMin"), 400.f), SbAeo(TEXT("gapMax"), 1400.f), 0.5f + 0.5f * FMath::Sin(2.f * PI * T / Period + i * 0.9f));
		Rr = R0 + Ease * (Gap - 400.f) * 1.3f;
		A = A0 + Ease * FMath::DegreesToRadians(SbAeo(TEXT("ringTurn"), 2.f)) * T + Ease * 0.06f * FMath::Sin(2.f * PI * T / (Period * 1.3f) + i);
		Z = Ease * 120.f * FMath::Sin(2.f * PI * T / (Period * 0.7f) + i * 1.7f);
	}
	return GetActorLocation() + FVector(FMath::Cos(A) * Rr, FMath::Sin(A) * Rr, Z);
}

int32 AWRSkyRing::PlumeNow(float Now, float& OutPhase) const
{
	// One of three vents every 40 s (30 in the Sporefall: tuning plumeEverySporefall when the Fungal lane's event runs).
	// The Fungal caverns' Breath vents run the same rotation on the same clock (WRFungal::VentEvery: 30 s in the
	// Sporefall); while the caverns aren't live, the ring's own number.
	const float Every = WRFungal::Live() ? WRFungal::VentEvery(this) : SbAeo(TEXT("plumeEvery"), 40.f);
	const float T = FMath::Fmod(Now, Every);
	const float Tell = SbAeo(TEXT("plumeTell"), 1.f), Live = SbAeo(TEXT("plumeSeconds"), 6.f);
	const float Start = Every - Tell;
	if (T >= Start) { OutPhase = T - Every; return int32(FMath::Fmod(Now / Every + 1.f, 3.f)); }
	if (T < Live) { OutPhase = T; return int32(FMath::Fmod(Now / Every, 3.f)); }
	OutPhase = -100.f;
	return -1;
}

int32 AWRSkyRing::NearestPlatform(const FVector& P, float Now, FVector& OutCentre) const
{
	int32 Best = 0;
	float BestD = TNumericLimits<float>::Max();
	for (int32 i = 0; i < Count(); ++i)
	{
		const FVector C = PlatformAt(i, Now);
		const float D = FVector::DistSquared2D(C, P);
		if (D < BestD) { BestD = D; Best = i; OutCentre = C; }
	}
	return Best;
}

bool AWRSkyRing::StoneBetween(const FVector& From, const FVector& To, float Now) const
{
	// Each wind-stone stands on its platform's outer rim, 4 m wide and 1.5 m tall: a segment test against its box.
	for (int32 i = 0; i < Count(); ++i)
	{
		const FVector C = PlatformAt(i, Now);
		const FVector Out = (C - GetActorLocation()).GetSafeNormal2D();
		const FVector Stone = C + Out * (PlatformRadius(i) - 120.f) + FVector(0, 0, 75.f);
		const FBox Box = FBox::BuildAABB(Stone, FVector(220.f, 220.f, 90.f));
		FVector HitL, HitN;
		float Time = 0.f;
		if (FMath::LineExtentBoxIntersection(Box, From, To, FVector::ZeroVector, HitL, HitN, Time)) return true;
	}
	return false;
}

void AWRSkyRing::BeginPlay()
{
	Super::BeginPlay();
	RingRef() = this;
	Build();
}

void AWRSkyRing::Build()
{
	using namespace WRLookKit;
	if (bBuilt) return;
	bBuilt = true;
	UStaticMesh* Cyl = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UMaterialInterface* Rock = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_Rock.M_Rock"));
	const bool bDraw = GetNetMode() != NM_DedicatedServer;
	for (int32 i = 0; i < Count(); ++i)
	{
		// Each platform: a root that moves; under it the white stone disc (collision: what you stand on), its wind-stone
		// on the outer rim (collision: it blocks the breath and the quills), a brazier, and the carved look.
		USceneComponent* Root = NewObject<USceneComponent>(this);
		Root->SetupAttachment(GetRootComponent());
		Root->RegisterComponent();
		Roots.Add(Root);
		const float R = PlatformRadius(i);
		UStaticMeshComponent* Disc = NewObject<UStaticMeshComponent>(this);
		Disc->SetStaticMesh(Cyl);
		Disc->SetupAttachment(Root);
		Disc->SetRelativeLocation(FVector(0, 0, -60.f));
		Disc->SetRelativeScale3D(FVector(R * 2.f / 100.f, R * 2.f / 100.f, 1.2f));
		Disc->SetCollisionProfileName(TEXT("BlockAll"));
		Disc->SetCanEverAffectNavigation(false);
		if (Rock && bDraw)
		{
			UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Rock, this);
			M->SetVectorParameterValue(TEXT("Tint"), SbHex(TEXT("F0EDE6")) * 0.72f);
			Disc->SetMaterial(0, M);
		}
		Disc->RegisterComponent();
		UStaticMeshComponent* Stone = NewObject<UStaticMeshComponent>(this);
		Stone->SetStaticMesh(Cube);
		Stone->SetupAttachment(Root);
		Stone->SetRelativeScale3D(FVector(1.2f, 4.f, 1.5f));
		Stone->SetCollisionProfileName(TEXT("BlockAll"));
		Stone->ComponentTags.Add(TEXT("WindStone"));
		if (Rock && bDraw)
		{
			UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Rock, this);
			M->SetVectorParameterValue(TEXT("Tint"), SbHex(TEXT("C9C4BA")) * 0.62f);
			Stone->SetMaterial(0, M);
		}
		Stone->RegisterComponent();
		if (bDraw)
		{
			// The carving: a ring of jade inlay round the rim, a carved wind on the stone, the brazier on its tripod.
			TArray<FWRLookBlock> Blocks;
			FKit Kit(FTransform::Identity, Blocks);
			const FSurf Jade = FSurf::Glaze(SbHex(TEXT("2F8F7A")) * 0.55f, 0.35f);
			const FSurf Bronze = FSurf::Steel(SbHex(TEXT("9A743E")) * 0.6f, 0.45f);
			for (int32 s = 0; s < 24; ++s)
			{
				const float A = s * PI / 12.f;
				Kit.Add(Jade, FVector(40.f, R * 0.25f, 4.f), FVector(FMath::Cos(A) * (R - 60.f), FMath::Sin(A) * (R - 60.f), 1.f), FRotator(0.f, FMath::RadiansToDegrees(A) + 90.f, 0.f));
			}
			Kit.Disc(Jade, FVector(0, 0, 1.f), FVector::UpVector, R * 0.5f, 3.f);
			for (int32 l = 0; l < 3; ++l)
			{
				const float A = l * 2.f * PI / 3.f;
				Kit.Rod(Bronze, FVector(-R * 0.45f + FMath::Cos(A) * 40.f, FMath::Sin(A) * 40.f, 0.f), FVector(-R * 0.45f, 0.f, 110.f), 6.f);
			}
			Kit.Disc(Bronze, FVector(-R * 0.45f, 0.f, 115.f), FVector::UpVector, 90.f, 20.f);
			Kit.Add(FSurf::Lit(SbHex(TEXT("FF9A3C")) * 0.7f, 6.f), FVector(60.f, 60.f, 30.f), FVector(-R * 0.45f, 0.f, 135.f));
			USceneComponent* Look = NewObject<USceneComponent>(this);
			Look->SetupAttachment(Root);
			Look->RegisterComponent();
			FWRBuildLook::Spawn(this, Look, Blocks, FLinearColor::White, false);
			UPointLightComponent* L = NewObject<UPointLightComponent>(this);
			L->SetupAttachment(Root);
			L->SetRelativeLocation(FVector(-R * 0.45f, 0.f, 180.f));
			L->SetLightColor(FLinearColor(1.f, 0.62f, 0.3f));
			L->SetAttenuationRadius(2400.f);
			L->SetIntensity(0.f);
			L->RegisterComponent();
			Lights.Add(L);
		}
		// The stone faces out (it's on the outer rim): its root turns to face the axis in Place.
		Stone->SetRelativeLocation(FVector(R - 120.f, 0.f, 75.f));
	}
	if (bDraw)
	{
		// The chains between the platforms (while it's bound): a sagging line of links from rim to rim; and the three
		// vents' plumes up the axis, the gap columns' rising spirals (shown as the state says).
		for (int32 i = 0; i < Count(); ++i)
		{
			USceneComponent* Ch = NewObject<USceneComponent>(this);
			Ch->SetupAttachment(GetRootComponent());
			Ch->RegisterComponent();
			Chains.Add(Ch);
			USceneComponent* Col = NewObject<USceneComponent>(this);
			Col->SetupAttachment(GetRootComponent());
			Col->RegisterComponent();
			Columns.Add(Col);
		}
	}
	Place(ServerNow());
}

void AWRSkyRing::Place(float Now)
{
	for (int32 i = 0; i < Roots.Num(); ++i)
	{
		const FVector C = PlatformAt(i, Now);
		const FVector Out = (C - GetActorLocation()).GetSafeNormal2D();
		Roots[i]->SetWorldLocationAndRotation(C, Out.Rotation());
	}
}

void AWRSkyRing::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Now = ServerNow();
	Place(Now);
	Clock += DeltaSeconds;
	if (GetNetMode() != NM_DedicatedServer && Clock > 0.25f)
	{
		Clock = 0.f;
		// Braziers lit at night; the chains while bound; the plume's tell and column; the gap columns once unbound.
		const bool bNight = AWRGameState::IsNightIn(this);
		for (UPointLightComponent* L : Lights) if (L) L->SetIntensity(bNight ? 9000.f : 0.f);
		const bool bBound = UnboundAt <= 0.f || Now < UnboundAt;
		if (bBound != bChainsShown)
		{
			bChainsShown = bBound;
			if (!bBound) WRFx::SpawnAt(this, TEXT("wind.vortex_burst"), GetActorLocation(), FRotator::ZeroRotator, 6.f);
		}
		if (bBound)
		{
			for (int32 i = 0; i < Count(); ++i)
			{
				// A chain's sag, link by link (sparks of its rust in the wind are the scenery's job).
				const FVector A = PlatformAt(i, Now), B = PlatformAt((i + 1) % Count(), Now);
				const FVector Dir = (B - A).GetSafeNormal2D();
				const FVector From = A + Dir * PlatformRadius(i), To = B - Dir * PlatformRadius((i + 1) % Count());
				if (Chains.IsValidIndex(i) && Chains[i] && Chains[i]->GetNumChildrenComponents() == 0)
				{
					TArray<FWRLookBlock> Blocks;
					WRLookKit::FKit Kit(FTransform::Identity, Blocks);
					const int32 N = 14;
					for (int32 k = 0; k < N; ++k)
					{
						const float T0 = float(k) / N, T1 = float(k + 1) / N;
						const FVector P0 = FMath::Lerp(From, To, T0) - FVector(0, 0, 180.f * 4.f * T0 * (1.f - T0)) - GetActorLocation();
						const FVector P1 = FMath::Lerp(From, To, T1) - FVector(0, 0, 180.f * 4.f * T1 * (1.f - T1)) - GetActorLocation();
						Kit.Add(WRLookKit::FSurf::Steel(SbHex(TEXT("5C4A3C")) * 0.5f, 0.75f), FVector((P1 - P0).Size(), 14.f, 6.f), (P0 + P1) * 0.5f, (P1 - P0).Rotation() + FRotator(0.f, 0.f, k % 2 ? 90.f : 0.f));
					}
					FWRBuildLook::Spawn(this, Chains[i], Blocks, FLinearColor::White, false);
				}
				if (Chains.IsValidIndex(i) && Chains[i]) Chains[i]->SetVisibility(true, true);
			}
		}
		else for (USceneComponent* Ch : Chains) if (Ch) Ch->SetVisibility(false, true);
		float Phase = 0.f;
		const int32 Vent = PlumeNow(Now, Phase);
		if (Vent != PlumeShown)
		{
			PlumeShown = Vent;
			if (Vent >= 0)
			{
				// The tell: the cyan glow brightens below and a deep huff rises; then the column up the axis for 6 s.
				const FVector Floor = GetActorLocation() - FVector(0, 0, 3000.f) + FVector(FMath::Cos(Vent * 2.1f) * 300.f, FMath::Sin(Vent * 2.1f) * 300.f, 0.f);
				WRFx::SpawnAt(this, TEXT("sky.plume"), Floor, FRotator::ZeroRotator, 1.f);
				WRSound::PlayAt(this, TEXT("sky.plume_huff"), GetActorLocation() - FVector(0, 0, 1500.f));
			}
		}
		// The gap columns after the Unbinding: rising spirals between the platforms.
		ColumnClock += 0.25f;
		if (!bBound && ColumnClock > 2.f)
		{
			ColumnClock = 0.f;
			for (int32 i = 0; i < Count(); ++i)
			{
				const FVector Mid = (PlatformAt(i, Now) + PlatformAt((i + 1) % Count(), Now)) * 0.5f;
				WRFx::SpawnAt(this, TEXT("sky.gap_column"), Mid - FVector(0, 0, 200.f), FRotator::ZeroRotator, 1.f);
			}
		}
	}
	// Server: the gap columns throw whoever steps in up and across (+10 m/s, 15 m tall, airborne 1.5 s; anyone).
	if (HasAuthority() && UnboundAt > 0.f && Now > UnboundAt + 4.f)
	{
		ColumnClock += DeltaSeconds;
		if (ColumnClock > 0.2f)
		{
			ColumnClock = 0.f;
			for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
			{
				ACharacter* C = *It;
				if (FVector::Dist2D(C->GetActorLocation(), GetActorLocation()) > 6000.f) continue;
				for (int32 i = 0; i < Count(); ++i)
				{
					const FVector A = PlatformAt(i, Now), B = PlatformAt((i + 1) % Count(), Now);
					const FVector Mid = (A + B) * 0.5f;
					const float Gap = FVector::Dist2D(A, B) - PlatformRadius(i) - PlatformRadius((i + 1) % Count());
					const FVector D = C->GetActorLocation() - Mid;
					if (Gap > 150.f && D.Size2D() < Gap * 0.5f && D.Z < SbAeo(TEXT("columnHeight"), 1500.f) && D.Z > -2500.f)
					{
						const FVector Across = (Mid - GetActorLocation()).GetSafeNormal2D() * -300.f;
						WRSkyAir::Push(C, FVector(Across.X, Across.Y, SbAeo(TEXT("columnLift"), 1000.f)), 0.3f, TEXT("column"), nullptr);
					}
				}
			}
		}
	}
}

// ================================================================================================ the hooks

bool FWRSkyBoss::IsSkyBoss(const AWRBoss* Boss)
{
	if (!Boss) return false;
	const FName Id = Boss->CreatureId;
	return Id == TEXT("kyrra") || Id == TEXT("bridge_sentinel") || Id == TEXT("old_gulper") || Id == TEXT("aeolith");
}

AWRSkyRing* FWRSkyBoss::Ring(const UObject* WorldContext)
{
	if (AWRSkyRing* R = RingRef().Get()) return R;
	const UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!W) return nullptr;
	for (TActorIterator<AWRSkyRing> It(W); It; ++It) { RingRef() = *It; return *It; }
	return nullptr;
}

void FWRSkyBoss::Begin(AWRBoss* Boss)
{
	if (!IsSkyBoss(Boss)) return;
	FSbState& S = St(Boss);
	S = FSbState();
	S.NextLandAt = SbLocal(Boss) + SbT(Boss, TEXT("landEvery"), 360.f);
	const FName Id = Boss->CreatureId;
	if (Id == TEXT("kyrra") || Id == TEXT("aeolith") || Id == TEXT("old_gulper")) Boss->SetAirborne(true);
	// The Sentinel kneels on its bridge, a statue, until the Toll House's copper is taken or someone strikes it.
	if (Id == TEXT("bridge_sentinel"))
	{
		S.bAwake = false;
		if (UWRSkyWorld* W = UWRSkyWorld::Get(Boss))
		{
			TWeakObjectPtr<AWRBoss> Weak(Boss);
			W->OnTollTaken.AddLambda([Weak](AWRCharacterBase* Who)
			{
				AWRBoss* B = Weak.Get();
				if (!B || !B->IsAlive()) return;
				FSbState& S2 = St(B);
				if (S2.bAwake) return;
				S2.bAwake = true;
				WRSound::Broadcast(B, TEXT("sky.sentinel_wake"));
				if (Who) B->SetAggroTarget(Who);
				UE_LOG(LogWildreach, Log, TEXT("BOSS the Bridge Sentinel wakes: the Toll House's copper taken"));
			});
		}
	}
	if (UWRSkyBody* Body = UWRSkyBody::Get(Boss)) Body->Flags = 0;
}

void FWRSkyBoss::Reset(AWRBoss* Boss)
{
	if (!IsSkyBoss(Boss)) return;
	FSbState& S = St(Boss);
	if (S.Held.IsValid()) { if (AWRCharacterBase* V = S.Held.Get()) { V->SetActorHiddenInGame(false); V->SetActorEnableCollision(true); } }
	for (const TWeakObjectPtr<AWRCharacterBase>& R : S.Riders) if (AWRCharacterBase* C = R.Get()) C->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	const bool bAwake = S.bAwake;
	S = FSbState();
	S.bAwake = bAwake && Boss->CreatureId == TEXT("bridge_sentinel");
	if (Boss->CreatureId == TEXT("aeolith"))
	{
		// The Ring binds again (its chains back) for the next fight.
		if (AWRSkyRing* R = Ring(Boss)) R->UnboundAt = 0.f;
	}
	if (UWRSkyBody* Body = UWRSkyBody::Get(Boss)) Body->Flags = 0;
	const FName Id = Boss->CreatureId;
	if (Id == TEXT("kyrra") || Id == TEXT("aeolith") || Id == TEXT("old_gulper")) Boss->SetAirborne(true);
}

void FWRSkyBoss::EndPlay(AWRBoss* Boss)
{
	if (!IsSkyBoss(Boss)) return;
	Reset(Boss);
	States().Remove(Boss);
}

bool FWRSkyBoss::TickIdle(AWRBoss* Boss, float DeltaSeconds, bool& bSkipWake)
{
	if (!IsSkyBoss(Boss)) return false;
	FSbState& S = St(Boss);
	const FName Id = Boss->CreatureId;
	const double T = SbLocal(Boss);
	if (Id == TEXT("kyrra"))
	{
		// Her loop over the bridges at 16 m/s, Tollgate to Brink and back over Eyrie Rock; every 6 min down on the widest
		// island on her way for 60 s, looking about.
		const FWRSkyIsleData& D = FWRSkyIsleData::Get();
		if (D.KyrraRoute.Num() < 2) return false;
		if (S.LandUntil > T)
		{
			Boss->SetAirborne(false);
			Boss->GetCharacterMovement()->StopMovementImmediately();
			return true;
		}
		if (S.LandUntil > 0.0 && T >= S.LandUntil)
		{
			S.LandUntil = 0.0;
			Boss->SetAirborne(true);
			Boss->LaunchCharacter(FVector(0, 0, 1200.f), true, true);
		}
		if (T >= S.NextLandAt && D.KyrraLands.Num() > 0)
		{
			// The widest island near her now among the ones she lands on.
			const FWRSkyIsleData::FIsland* Best = nullptr;
			float BestScore = -1.f;
			for (const FName& L : D.KyrraLands) if (const FWRSkyIsleData::FIsland* I = D.FindIsland(L))
			{
				const float Score = I->R() / FMath::Max(1000.f, FVector::Dist2D(I->Centre, Boss->GetActorLocation()) * 0.05f);
				if (Score > BestScore) { BestScore = Score; Best = I; }
			}
			if (Best)
			{
				S.LandAt = FVector(Best->Centre.X, Best->Centre.Y, Best->TopZ(Best->Centre) + Boss->GetSimpleCollisionHalfHeight() + 40.f);
				const FVector To = S.LandAt - Boss->GetActorLocation();
				if (To.Size() > 300.f)
				{
					Boss->SetActorLocation(FMath::VInterpConstantTo(Boss->GetActorLocation(), S.LandAt, DeltaSeconds, SbT(Boss, TEXT("routeSpeed"), 1600.f)));
					Boss->SetActorRotation(FRotator(0.f, To.Rotation().Yaw, 0.f));
					return true;
				}
				S.NextLandAt = T + SbT(Boss, TEXT("landEvery"), 360.f);
				S.LandUntil = T + SbT(Boss, TEXT("landSeconds"), 60.f);
				Boss->SetHome(S.LandAt);
				return true;
			}
		}
		const FVector Goal = D.KyrraRoute[S.RouteIdx % D.KyrraRoute.Num()];
		const FVector To = Goal - Boss->GetActorLocation();
		if (To.Size() < 600.f) S.RouteIdx = (S.RouteIdx + 1) % D.KyrraRoute.Num();
		Boss->SetActorLocation(FMath::VInterpConstantTo(Boss->GetActorLocation(), Goal, DeltaSeconds, SbT(Boss, TEXT("routeSpeed"), 1600.f)), false);
		Boss->SetActorRotation(FMath::RInterpTo(Boss->GetActorRotation(), FRotator(0.f, To.Rotation().Yaw, 0.f), DeltaSeconds, 1.5f));
		Boss->SetHome(Boss->GetActorLocation());
		return true;
	}
	if (Id == TEXT("bridge_sentinel"))
	{
		// A statue on its knees until woken: no roam, nothing to see but the vanes turning in the wind.
		if (!S.bAwake) { bSkipWake = true; Boss->GetCharacterMovement()->StopMovementImmediately(); return true; }
		return false;
	}
	if (Id == TEXT("old_gulper"))
	{
		// Floating over the Bight, turning slow circles, 4 m over the water.
		S.CircleA += DeltaSeconds * 0.05f;
		const FVector Home = Boss->GetHome();
		const FVector Want = Home + FVector(FMath::Cos(S.CircleA) * 1500.f, FMath::Sin(S.CircleA) * 1500.f, 0.f);
		Boss->SetActorLocation(FMath::VInterpConstantTo(Boss->GetActorLocation(), Want, DeltaSeconds, 250.f), false);
		Boss->SetActorRotation(FRotator(0.f, (Want - Boss->GetActorLocation()).Rotation().Yaw + 90.f, 0.f));
		return true;
	}
	if (Id == TEXT("aeolith"))
	{
		// It circles the Flue's axis through the Ring, guardian and prisoner: round and round over the plumes.
		AWRSkyRing* R = Ring(Boss);
		const FVector C = R ? R->Centre() : Boss->GetHome();
		S.CircleA += DeltaSeconds * SbAeo(TEXT("circleSpeed"), 1800.f) / SbAeo(TEXT("circleRadius"), 4200.f) * 0.5f;
		const float Rr = SbAeo(TEXT("circleRadius"), 4200.f);
		const FVector Want = C + FVector(FMath::Cos(S.CircleA) * Rr, FMath::Sin(S.CircleA) * Rr, SbAeo(TEXT("circleHeight"), 800.f) + 300.f * FMath::Sin(S.CircleA * 3.f));
		const FVector D = Want - Boss->GetActorLocation();
		Boss->SetActorLocation(FMath::VInterpConstantTo(Boss->GetActorLocation(), Want, DeltaSeconds, SbAeo(TEXT("circleSpeed"), 1800.f) * 1.2f), false);
		if (D.Size() > 10.f) Boss->SetActorRotation(FMath::RInterpTo(Boss->GetActorRotation(), D.Rotation(), DeltaSeconds, 3.f));
		return true;
	}
	return false;
}

bool FWRSkyBoss::Tick(AWRBoss* Boss, float DeltaSeconds)
{
	if (!IsSkyBoss(Boss)) return false;
	FSbState& S = St(Boss);
	const FName Id = Boss->CreatureId;
	const double T = SbLocal(Boss);
	const double Now = SbNow(Boss);
	UWRSkyBody* Body = UWRSkyBody::Get(Boss);
	UCharacterMovementComponent* Move = Boss->GetCharacterMovement();

	// A boss off its ground (over the edge, down the Flue) flies or climbs back onto the nearest island in 3 s: bosses
	// never die of a fall (SKY 2.6 edge rule).
	if (!Boss->bAirborne && Move->IsFalling() && Boss->GetActorLocation().Z < Boss->GetHome().Z - SbAeo(TEXT("outBelow"), 4000.f))
	{
		if (S.OffSince <= 0.0) S.OffSince = T;
		if (T - S.OffSince > SbAeo(TEXT("recoverSeconds"), 3.f))
		{
			S.OffSince = 0.0;
			const FWRSkyIsleData::FIsland* I = FWRSkyIsleData::Get().NearestIsland(Boss->GetHome());
			const FVector Back = I ? FVector(I->Centre.X, I->Centre.Y, I->TopZ(I->Centre) + Boss->GetSimpleCollisionHalfHeight() + 50.f) : Boss->GetHome();
			Boss->TeleportTo(Back, Boss->GetActorRotation());
			WRFx::Broadcast(Boss, TEXT("sky.wing_gust"), Back, FRotator::ZeroRotator, 3.f, TEXT("creature.windmane.wings"));
		}
	}
	else S.OffSince = 0.0;

	// A carry (Kyrra's snatch up 20 m over 3 s; Aeolith's 15 m over 2.5 s) or a swallow (4 s inside Gulper).
	if (AWRCharacterBase* V = S.Held.Get())
	{
		const float Free = S.HeldHow == TEXT("snatch_kyrra") ? SbT(Boss, TEXT("snatchFree"), 120.f) : S.HeldHow == TEXT("snatch_aeolith") ? SbAeo(TEXT("snatchFree"), 150.f) : 1.0e9f;
		if (!V->IsAlive() || T >= S.HeldUntil || S.FreeStance >= Free)
		{
			const bool bSwallow = S.HeldHow == TEXT("swallow");
			S.Held = nullptr;
			if (V->Status) V->Status->ClearStatus(WRTags::Status_Staggered);
			if (bSwallow)
			{
				// Spat 15 m any way (often off the island).
				V->SetActorHiddenInGame(false);
				V->SetActorEnableCollision(true);
				const float A = FMath::FRandRange(0.f, 2.f * PI);
				V->SetActorLocation(Boss->GetActorLocation() + Boss->GetActorForwardVector() * (Boss->GetSimpleCollisionRadius() + 200.f), false, nullptr, ETeleportType::TeleportPhysics);
				WRSkyAir::Push(V, FVector(FMath::Cos(A), FMath::Sin(A), 0.4f) * SbT(Boss, TEXT("spit"), 1500.f), 0.6f, TEXT("spit"), Boss);
				S.LastSwallowAt = T;
				WRFx::Broadcast(Boss, TEXT("sky.spout"), V->GetActorLocation(), FRotator::ZeroRotator, 1.f, TEXT("creature.old_gulper.spit"));
			}
			else
			{
				// Dropped: over a gap you need a glider or Featherfall (the fall's own damage is the rest).
				V->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
				Tell(V, S.FreeStance >= Free ? TEXT("Torn free!") : TEXT("Dropped"));
			}
			if (Body) Body->Holding = nullptr;
		}
		else
		{
			const float F = FMath::Clamp(float(1.0 - (S.HeldUntil - T) / FMath::Max(0.1, double(S.HeldHow == TEXT("snatch_kyrra") ? SbT(Boss, TEXT("snatchSeconds"), 3.f) : SbAeo(TEXT("snatchSeconds"), 2.5f)))), 0.f, 1.f);
			if (S.HeldHow == TEXT("swallow"))
			{
				V->SetActorLocation(Boss->GetActorLocation(), false, nullptr, ETeleportType::TeleportPhysics);
				if (T >= S.HeldTickAt)
				{
					S.HeldTickAt = T + 1.0;
					FWRHit Hit = SbHit(Boss, SbT(Boss, TEXT("swallowDps"), 12.f), true);
					Hit.bDamageOverTime = true;
					UWRCombatLibrary::ApplyHit(V, Hit);
				}
			}
			else
			{
				// Up in the talons (or the jaws): the carrier climbs; the held one hangs under it.
				const float Up = S.HeldHow == TEXT("snatch_kyrra") ? SbT(Boss, TEXT("snatchUp"), 2000.f) : SbAeo(TEXT("snatchUp"), 1500.f);
				Boss->SetActorLocation(FMath::VInterpTo(Boss->GetActorLocation(), S.HeldFrom + FVector(0, 0, Up * F + 300.f), DeltaSeconds, 3.f), false);
				V->SetActorLocation(Boss->GetActorLocation() - FVector(0, 0, Boss->GetSimpleCollisionHalfHeight() + V->GetSimpleCollisionHalfHeight()), false, nullptr, ETeleportType::TeleportPhysics);
				if (V->Status) V->Status->Stagger(0.3f);
			}
			return true;
		}
	}

	if (Id == TEXT("kyrra"))
	{
		// Grounded by a broken wing: 10 s down (x1.25 taken), then up again; both wings broken, she fights on foot.
		const bool bBoth = Boss->IsPartBroken(TEXT("wing_left")) && Boss->IsPartBroken(TEXT("wing_right"));
		if (bBoth && Boss->bAirborne) Boss->SetAirborne(false);
		if (!bBoth && !Boss->bAirborne && Now >= Boss->GroundedUntil && Boss->Current == INDEX_NONE && !Boss->HasTag(WRTags::Status_Staggered))
		{
			Boss->SetAirborne(true);
			Boss->LaunchCharacter(FVector(0, 0, 1200.f), true, true);
		}
		if (bBoth) Boss->SpeedBoost = SbT(Boss, TEXT("footSpeed"), 0.6f);
		// Her stoop's bring-down window over: up off the mantle.
		if (Body && Body->Has(WRSkyFlag::Mantle) && Boss->GetWorld()->GetTimeSeconds() >= Boss->GroundedUntil)
		{
			Body->Set(WRSkyFlag::Mantle, false);
			Body->Set(WRSkyFlag::Grounded, false);
		}
		return false;
	}
	if (Id == TEXT("bridge_sentinel"))
	{
		if (!S.bAwake) return true;
		// Through the deck: a 35 m fall to Winch Isle; a quarter of its health on landing; kneeling 10 s (the riposte).
		if (S.bFalling && Move->IsMovingOnGround())
		{
			S.bFalling = false;
			S.bFell = true;
			FWRHit Hit = SbHit(Boss, Boss->GetAttributes()->GetMaxHealth() * SbT(Boss, TEXT("fallDamage"), 0.25f), true);
			Hit.Instigator = nullptr;
			Hit.bDamageOverTime = true;
			Hit.bBurst = true;
			UWRCombatLibrary::ApplyHit(Boss, Hit);
			UWRCombatLibrary::BreakStance(Boss, SbT(Boss, TEXT("kneelSeconds"), 10.f), nullptr);
			S.KneelUntil = T + SbT(Boss, TEXT("kneelSeconds"), 10.f);
			WRFx::Broadcast(Boss, TEXT("shockwave"), Boss->GetActorLocation() - FVector(0, 0, Boss->GetSimpleCollisionHalfHeight()), FRotator::ZeroRotator, 6.f, TEXT("sky.fall_heavy"));
			Boss->SetHome(Boss->GetActorLocation());
			UE_LOG(LogWildreach, Log, TEXT("BOSS the Bridge Sentinel lands on Winch Isle: kneeling"));
		}
		if (S.bFalling) return true;
		return false;
	}
	if (Id == TEXT("old_gulper"))
	{
		// Choked: sunk onto the lagoon, mouth open, 10 s.
		if (Body) Body->Set(WRSkyFlag::Gape, T < S.ChokeUntil || (Boss->Current != INDEX_NONE && Boss->Attacks.IsValidIndex(Boss->Current) && Boss->Attacks[Boss->Current].Id == TEXT("inhale")));
		if (T < S.ChokeUntil) { Boss->SetAirborne(false); return true; }
		if (!Boss->bAirborne) Boss->SetAirborne(true);
		// It floats 4 m over the water / the island under it.
		float Gz = 0.f;
		if (WRSkyIsles::GroundUnder(Boss->GetActorLocation(), Gz))
		{
			const float Want = Gz + SbT(Boss, TEXT("floatAbove"), 400.f) + Boss->GetSimpleCollisionHalfHeight();
			FVector P = Boss->GetActorLocation();
			P.Z = FMath::FInterpTo(P.Z, Want, DeltaSeconds, 1.5f);
			Boss->SetActorLocation(P, false);
		}
		return false;
	}
	if (Id == TEXT("aeolith"))
	{
		AWRSkyRing* R = Ring(Boss);
		// The plume rule: passing through a live plume weighs its wings (+150 spore, it screams and veers).
		if (R)
		{
			float Ph = 0.f;
			const int32 Vent = R->PlumeNow(float(Now), Ph);
			const int32 Cycle = int32(Now / SbAeo(TEXT("plumeEvery"), 40.f));
			if (Vent >= 0 && Ph >= 0.f && Cycle != S.LastPlumeCycle && FVector::Dist2D(Boss->GetActorLocation(), R->Centre()) < SbAeo(TEXT("plumeRadius"), 300.f) + Boss->GetSimpleCollisionRadius())
			{
				S.LastPlumeCycle = Cycle;
				S.Spore += SbAeo(TEXT("sporePlume"), 150.f);
				WRSound::Voice(Boss, TEXT("hurt"));
				WRFx::Broadcast(Boss, TEXT("sky.spore_burst"), Boss->GetActorLocation(), FRotator::ZeroRotator, 3.f, NAME_None);
				UE_LOG(LogWildreach, Log, TEXT("BOSS Aeolith through a plume: spore %.0f"), S.Spore);
			}
		}
		// Spore-weighted at 400: down on the nearest platform 8 s (x1.25 taken, the Crown off), grey over its feathers.
		if (S.Spore >= SbAeo(TEXT("sporeThreshold"), 400.f) && S.WeightedUntil <= T)
		{
			S.Spore = 0.f;
			S.WeightedUntil = T + SbAeo(TEXT("weightedSeconds"), 8.f);
			S.CrownOffUntil = FMath::Max(S.CrownOffUntil, S.WeightedUntil);
			Boss->Current = INDEX_NONE;
			Boss->StrikeState = AWRBoss::EStrike::None;
			if (R)
			{
				FVector Pc;
				R->NearestPlatform(Boss->GetActorLocation(), float(Now), Pc);
				Boss->SetActorLocation(Pc + FVector(0, 0, Boss->GetSimpleCollisionHalfHeight() + 60.f), false, nullptr, ETeleportType::TeleportPhysics);
			}
			Boss->SetAirborne(false);
			if (Body) Body->Set(WRSkyFlag::Deflated, true);
			WRSound::Voice(Boss, TEXT("hurt"));
			UE_LOG(LogWildreach, Log, TEXT("BOSS Aeolith spore-weighted: grounded"));
		}
		if (S.WeightedUntil > T) return true;
		if (S.WeightedUntil > 0.0 && T >= S.WeightedUntil)
		{
			S.WeightedUntil = 0.0;
			if (Body) Body->Set(WRSkyFlag::Deflated, false);
			Boss->SetAirborne(true);
		}
		// A wing broken: grounded 8 s on the nearest platform (the built rule), the Crown off 20 s.
		if (!Boss->bAirborne && Now >= Boss->GroundedUntil && S.CrashUntil <= T && Boss->Current == INDEX_NONE) Boss->SetAirborne(true);
		if (Body) Body->Set(WRSkyFlag::Lobes, Boss->Current != INDEX_NONE && Boss->Attacks.IsValidIndex(Boss->Current) && Boss->StrikeState == AWRBoss::EStrike::Windup
			&& (Boss->Attacks[Boss->Current].Id == TEXT("gale_gust") || Boss->Attacks[Boss->Current].Id == TEXT("held_gale")));
		// The Updraft Inhale's lift: those caught hang 8 m up, 1.2 s.
		if (S.LiftUntil > T)
		{
			for (const TWeakObjectPtr<AWRCharacterBase>& W : S.Lifted) if (AWRCharacterBase* C = W.Get())
			{
				C->GetCharacterMovement()->Velocity = FVector(0.f, 0.f, FMath::Max(0.f, C->GetCharacterMovement()->Velocity.Z));
				C->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
				C->GetCharacterMovement()->Velocity.Z = 120.f;
			}
		}
		// The Last Circle (phase 3): low passes every 20 s, the vents, the barrel roll, riders.
		if (Boss->Phase >= 3)
		{
			if (!S.bVentsUp && !S.bRegrown && S.Vent[0] <= 0.f && S.Vent[1] <= 0.f && S.Vent[2] <= 0.f && S.CrashUntil <= 0.0)
			{
				for (float& V : S.Vent) V = SbAeo(TEXT("ventHealth"), 900.f);
				S.bVentsUp = true;
				S.NextPassAt = T + SbAeo(TEXT("passEvery"), 20.f);
				S.NextRollAt = T + SbAeo(TEXT("rollEvery"), 12.f);
			}
			if (S.CrashUntil > T) { Boss->SetAirborne(false); return true; }
			if (S.CrashUntil > 0.0 && T >= S.CrashUntil)
			{
				// It rises; the vents regrow at half (once).
				S.CrashUntil = 0.0;
				if (!S.bRegrown) { S.bRegrown = true; for (float& V : S.Vent) V = SbAeo(TEXT("ventHealth"), 900.f) * SbAeo(TEXT("regrow"), 0.5f); S.bVentsUp = true; }
				Boss->SetAirborne(true);
			}
			// Riders ride; the roll: a 1.2 s whistle and its crest flattening, then 2 s rolling: Guard holds you on
			// (30 stamina a second), anything else throws you.
			S.Riders.RemoveAll([](const TWeakObjectPtr<AWRCharacterBase>& W) { return !W.IsValid() || !W->IsAlive(); });
			if (S.Riders.Num() > 0 && T >= S.NextRollAt && S.RollAt <= 0.0)
			{
				S.RollAt = T + SbAeo(TEXT("rollTell"), 1.2f);
				WRSound::PlayOn(Boss, TEXT("sky.aeolith_roll_tell"));
				for (const TWeakObjectPtr<AWRCharacterBase>& W : S.Riders) Tell(W.Get(), TEXT("It's rolling: hold Guard!"));
			}
			if (S.RollAt > 0.0 && T >= S.RollAt)
			{
				if (Body) Body->Set(WRSkyFlag::Breach, T < S.RollAt + SbAeo(TEXT("rollSeconds"), 2.f));
				for (int32 r = S.Riders.Num() - 1; r >= 0; --r)
				{
					AWRCharacterBase* C = S.Riders[r].Get();
					if (!C) continue;
					const UWRAttributeSet* A = C->GetAttributes();
					const float Need = SbAeo(TEXT("rollStamina"), 30.f) * DeltaSeconds;
					if (C->HasTag(WRTags::State_Blocking) && A && A->GetStamina() >= Need)
					{
						C->GetAbilitySystemComponent()->SetNumericAttributeBase(UWRAttributeSet::GetStaminaAttribute(), A->GetStamina() - Need);
						continue;
					}
					// Thrown off: over a platform you land (the fall applies), over the Flue you need a glider or Featherfall.
					C->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
					C->SetActorEnableCollision(true);
					C->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
					WRSkyAir::Push(Cast<ACharacter>(C), Boss->GetActorRightVector() * 900.f + FVector(0, 0, 300.f), 0.4f, TEXT("roll"), Boss);
					S.Riders.RemoveAt(r);
					if (S.RiderSlot.IsValidIndex(r)) S.RiderSlot.RemoveAt(r);
					Tell(C, TEXT("Thrown off"));
				}
				if (T >= S.RollAt + SbAeo(TEXT("rollSeconds"), 2.f)) { S.RollAt = 0.0; S.NextRollAt = T + SbAeo(TEXT("rollEvery"), 12.f); if (Body) Body->Set(WRSkyFlag::Breach, false); }
			}
			// The low pass: told by a rising wing-roar 1.2 s ahead, then 1.5 s across the ring with its back 2 m over the
			// platforms (E within 4 m grabs on).
			if (S.PassUntil <= T && T >= S.NextPassAt - SbAeo(TEXT("passTell"), 1.2f) && S.NextPassAt > 0.0 && Boss->Current == INDEX_NONE)
			{
				if (T < S.NextPassAt) { if (Body && !Body->Has(WRSkyFlag::Hood)) { Body->Set(WRSkyFlag::Hood, true); WRSound::PlayOn(Boss, TEXT("sky.aeolith_pass")); } }
				else
				{
					S.PassUntil = T + SbAeo(TEXT("passSeconds"), 1.5f);
					S.NextPassAt = T + SbAeo(TEXT("passEvery"), 20.f);
					if (Body) Body->Set(WRSkyFlag::Hood, false);
				}
			}
			if (S.PassUntil > T && R)
			{
				// Straight across the Ring's middle, low.
				const float F = 1.f - float((S.PassUntil - T) / SbAeo(TEXT("passSeconds"), 1.5f));
				const FVector Dir = FRotator(0.f, S.CircleA * 57.3f, 0.f).Vector();
				const FVector From = R->Centre() - Dir * 3500.f, To = R->Centre() + Dir * 3500.f;
				Boss->SetActorLocation(FMath::Lerp(From, To, F) + FVector(0, 0, SbAeo(TEXT("passAbove"), 200.f) + Boss->GetSimpleCollisionHalfHeight() + 60.f), false);
				Boss->SetActorRotation(Dir.Rotation());
				return true;
			}
		}
		return false;
	}
	return false;
}

bool FWRSkyBoss::Allowed(const AWRBoss* Boss, const FWRBossAttack& Attack, const AWRCharacterBase* Target)
{
	if (!IsSkyBoss(Boss)) return true;
	const FSbState* Sp = States().Find(Boss);
	const FName Id = Attack.Id;
	if (Boss->CreatureId == TEXT("kyrra"))
	{
		if (Id == TEXT("screech")) return !(Sp && Sp->bScreeched);
		// Both wings gone: no more stoops.
		if (Id == TEXT("dive")) return !(Boss->IsPartBroken(TEXT("wing_left")) && Boss->IsPartBroken(TEXT("wing_right")));
	}
	if (Boss->CreatureId == TEXT("bridge_sentinel"))
	{
		// On Winch Isle there's no deck to throw.
		if (Id == TEXT("deck_throw")) return !(Sp && Sp->bFell) && Target && FVector::Dist(Target->GetActorLocation(), Boss->GetActorLocation()) > 400.f;
	}
	if (Boss->CreatureId == TEXT("old_gulper"))
	{
		if (Id == TEXT("belch")) return Sp && SbLocal(Boss) - Sp->LastSwallowAt < SbT(Boss, TEXT("belchAfter"), 8.f);
	}
	if (Boss->CreatureId == TEXT("aeolith"))
	{
		// Phase 3 never lands: only its air moves.
		if (Boss->Phase >= 3 && (Id == TEXT("tail_sweep") || Id.ToString().StartsWith(TEXT("coil")) || Id == TEXT("crosswind_dive") || Id == TEXT("spiral") || Id == TEXT("updraft_inhale") || Id == TEXT("snatch"))) return false;
		if (Id == TEXT("feather_volley")) return Target && FVector::Dist(Target->GetActorLocation(), Boss->GetActorLocation()) >= SbAeo(TEXT("volleyMinRange"), 600.f);
	}
	return true;
}

void FWRSkyBoss::AttackStarted(AWRBoss* Boss, FWRBossAttack& Attack, AWRCharacterBase* Target)
{
	if (!IsSkyBoss(Boss)) return;
	UWRSkyBody* Body = UWRSkyBody::Get(Boss);
	const FName Id = Attack.Id;
	if (Body)
	{
		// The tells the body shows: the vanes stop and point down (slam), whine up (spin); the hood spreads (dive, inhale).
		Body->Set(WRSkyFlag::Hood, Id == TEXT("vane_slam") || Id == TEXT("crosswind_dive") || Id == TEXT("spiral") || Id == TEXT("updraft_inhale"));
		Body->Set(WRSkyFlag::Flap, Id == TEXT("vane_spin"));
		Body->Set(WRSkyFlag::Gape, Id == TEXT("inhale") || Id.ToString().StartsWith(TEXT("coil")));
		Body->Set(WRSkyFlag::Crest, Id == TEXT("dive"));
		// (A stoop cut short by a stagger leaves no folded wings or mantle behind into the next move.)
		Body->Set(WRSkyFlag::Stoop, false);
		Body->Set(WRSkyFlag::Mantle, false);
	}
	// Kyrra's stoop: during the wind-up she climbs to a fold point up and back off her line to you; the drop starts from
	// there, so it comes down steep out of the sky (SbT stoopClimb / stoopBack / stoopFoldAt).
	if (Boss->CreatureId == TEXT("kyrra") && Id == TEXT("dive"))
	{
		FSbState& S = St(Boss);
		const FVector Aim = Target ? Target->GetActorLocation() : Boss->AttackPoint;
		const FVector Away = (Boss->GetActorLocation() - Aim).GetSafeNormal2D();
		S.StoopFrom = Boss->GetActorLocation();
		S.StoopFold = S.StoopFrom + Away * SbT(Boss, TEXT("stoopBack"), 600.f)
			+ FVector(0.f, 0.f, FMath::Max(SbT(Boss, TEXT("stoopClimb"), 900.f), Aim.Z + SbT(Boss, TEXT("stoopAbove"), 1800.f) - S.StoopFrom.Z));
		S.bStooping = true;
		S.bStoopFolded = false;
		if (Body) Body->Set(WRSkyFlag::Grounded, false);
	}
	if (Id == TEXT("inhale")) WRFx::Broadcast(Boss, TEXT("sky.undertow_tell"), Boss->GetActorLocation() + Boss->GetActorForwardVector() * Boss->GetSimpleCollisionRadius(), Boss->GetActorRotation(), 3.f, TEXT("creature.old_gulper.inhale"), Attack.Windup);
	if (Id == TEXT("updraft_inhale")) WRFx::Broadcast(Boss, TEXT("sky.gap_column"), Boss->GetActorLocation() - FVector(0, 0, 600.f), FRotator::ZeroRotator, 4.f, TEXT("creature.aeolith.roar"), Attack.Windup);
	if (Id == TEXT("downburst")) WRFx::Broadcast(Boss, TEXT("sky.shadow"), Boss->AttackPoint, FRotator::ZeroRotator, 4.f, TEXT("sky.downburst_hiss"), Attack.Windup + Attack.Row.Hold);
	if (Id == TEXT("held_gale") && Target)
	{
		// The breath waits with the sac full and swings to the side with the most players.
		FVector Sum = FVector::ZeroVector;
		for (AWRCharacterBase* F : Foes(Boss, 3000.f)) Sum += (F->GetActorLocation() - Boss->GetActorLocation()).GetSafeNormal2D();
		if (!Sum.IsNearlyZero()) Boss->AttackFacing = Sum.Rotation();
	}
}

bool FWRSkyBoss::TickWindup(AWRBoss* Boss, FWRBossAttack& Attack, float DeltaSeconds)
{
	if (!IsSkyBoss(Boss) || Boss->CreatureId != TEXT("kyrra") || Attack.Id != TEXT("dive")) return false;
	FSbState& S = St(Boss);
	if (!S.bStooping) return false;
	UWRSkyBody* Body = UWRSkyBody::Get(Boss);
	const double Now = Boss->GetWorld()->GetTimeSeconds();
	const float Total = FMath::Max(0.2f, float(Boss->ResolveAt - Boss->StrikeStarted));
	const float FoldAt = SbT(Boss, TEXT("stoopFoldAt"), 0.72f);
	const float A = FMath::Clamp(float(Now - Boss->StrikeStarted) / (Total * FoldAt), 0.f, 1.f);
	if (!S.bStoopFolded)
	{
		// The climb: wings beating hard, eased in and out (never a jump), up and back to the fold point.
		const float E = A * A * (3.f - 2.f * A);
		Boss->SetActorLocation(FMath::Lerp(S.StoopFrom, S.StoopFold, E), false);
		if (Body) Body->Set(WRSkyFlag::Flap, true);
		if (A >= 1.f)
		{
			// The fold: the line locks on where you stand now (never homing after this), wings in to a teardrop, her
			// scream and a streak off her head; the last of the wind-up she hangs there, aimed.
			S.bStoopFolded = true;
			if (AWRCharacterBase* T = Boss->AttackTarget.Get()) Boss->AttackPoint = T->GetActorLocation() - FVector(0.f, 0.f, T->GetSimpleCollisionHalfHeight());
			if (Body)
			{
				Body->Set(WRSkyFlag::Flap, false);
				Body->Set(WRSkyFlag::Stoop, true);
				Body->MulticastMoment(TEXT("sky.stoop_streak"), TEXT("head"), 1.6f, TEXT("creature.galeharrier.signature"));
			}
		}
	}
	// Facing down the line she'll fall along.
	const FVector Line = Boss->AttackPoint - Boss->GetActorLocation();
	Boss->SetActorRotation(FRotator(0.f, Line.Rotation().Yaw, 0.f));
	return true;
}

bool FWRSkyBoss::BeginStrike(AWRBoss* Boss, FWRBossAttack& Attack)
{
	if (!IsSkyBoss(Boss)) return false;
	FSbState& S = St(Boss);
	const FName Id = Attack.Id;
	const double T = SbLocal(Boss);
	if (Id == TEXT("screech"))
	{
		// Two Galeharriers within 200 m come, once a fight.
		S.bScreeched = true;
		int32 N = 0;
		for (TActorIterator<AWRCreature> It(Boss->GetWorld()); It && N < FMath::RoundToInt(SbT(Boss, TEXT("screechCalls"), 2.f)); ++It)
		{
			if (It->CreatureId == TEXT("galeharrier") && It->IsAlive() && !It->IsTamed() && FVector::Dist(It->GetActorLocation(), Boss->GetActorLocation()) < SbT(Boss, TEXT("screechReach"), 20000.f))
			{
				It->SetAggroTarget(Boss->GetAggroTarget());
				++N;
			}
		}
		WRSound::Voice(Boss, TEXT("aggro"));
		return true;
	}
	if (Id == TEXT("vane_slam") && !S.bFell)
	{
		// On a weak deck section the deck gives way under it: it falls through to Winch Isle.
		const FWRSkyIsleData& D = FWRSkyIsleData::Get();
		const FWRSkyIsleData::FBridge* Br = D.FindBridge(TEXT("sentinel_deck"));
		AWRSkyState* State = AWRSkyState::Get(Boss);
		if (Br && State)
		{
			const FVector Feet = Boss->GetActorLocation() - FVector(0, 0, Boss->GetSimpleCollisionHalfHeight());
			for (TActorIterator<AWRSkyScenery> It(Boss->GetWorld()); It; ++It)
			{
				for (int32 i = 0; i < It->Slabs.Num(); ++i)
				{
					UStaticMeshComponent* M = It->Slabs[i];
					if (!M || !It->SlabWeak.IsValidIndex(i) || !It->SlabWeak[i]) continue;
					if (FVector::Dist2D(M->GetComponentLocation(), Feet) > SbT(Boss, TEXT("weakReach"), 350.f)) continue;
					State->DeckOpen.SetNumZeroed(It->Slabs.Num());
					State->DeckCloseAt.SetNumZeroed(It->Slabs.Num());
					// The section gives way: this slab and the next ones along (6 m).
					for (int32 k = FMath::Max(0, i - 1); k <= FMath::Min(It->Slabs.Num() - 1, i + 1); ++k)
					{
						State->DeckOpen[k] = 1;
						State->DeckCloseAt[k] = AWRGameState::Get(Boss) ? AWRGameState::Get(Boss)->GetTotalHours() + SbT(Boss, TEXT("holeHours"), 2.f) : 0.0;
					}
					State->OnRep_Deck();
					State->ForceNetUpdate();
					S.bFalling = true;
					WRFx::Broadcast(Boss, TEXT("hit.dust"), M->GetComponentLocation(), FRotator::ZeroRotator, 6.f, TEXT("sky.deck_break"));
					UE_LOG(LogWildreach, Log, TEXT("BOSS the Bridge Sentinel's slam breaks the deck: it falls"));
					return false;   // the slam's ring still lands
				}
			}
		}
	}
	if (Id == TEXT("vane_spin")) { S.NextSpinTick = T; return false; }
	if (Id == TEXT("barnacle_shed"))
	{
		// Five barnacles off the hump: each one's shadow grows 0.9 s before it lands (r 2 m).
		TArray<AWRCharacterBase*> Who = Foes(Boss, 2500.f);
		const int32 N = FMath::RoundToInt(SbT(Boss, TEXT("barnacles"), 5.f));
		TWeakObjectPtr<AWRBoss> Weak(Boss);
		for (int32 b = 0; b < N; ++b)
		{
			const FVector Base = Who.Num() ? Who[b % Who.Num()]->GetActorLocation() : Boss->GetActorLocation();
			float Gz = Base.Z;
			FVector At = Base + FVector(FMath::RandPointInCircle(450.f), 0.f);
			if (WRSkyIsles::GroundUnder(At + FVector(0, 0, 500.f), Gz)) At.Z = Gz;
			const float Tell = SbT(Boss, TEXT("barnacleTell"), 0.9f) + b * 0.25f;
			WRFx::Broadcast(Boss, TEXT("sky.shadow"), At, FRotator::ZeroRotator, 1.f, NAME_None, Tell);
			// Each barnacle really comes off the hump and falls where its shadow grows (cookpass-combat 3.10): a lob timed
			// to land as the shadow closes, bursting over barnacleRadius.
			const FVector Hump = Boss->GetActorLocation() + FVector(0.f, 0.f, Boss->GetSimpleCollisionHalfHeight() * 0.8f);
			FActorSpawnParameters Params;
			Params.Owner = Boss;
			Params.Instigator = Boss;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			const FVector V = AWRProjectile::LobVelocity(Hump, At, FMath::Max(250.f, FVector::Dist2D(Hump, At) / Tell), 1.f);
			if (AWRProjectile* Barnacle = Boss->GetWorld()->SpawnActor<AWRProjectile>(AWRProjectile::StaticClass(), Hump, V.Rotation(), Params))
			{
				FWRHit Hit = SbHit(Boss, 40.f, false);
				Hit.Origin = Hump;
				Barnacle->InitLob(Hit, V, 1.f, SbT(Boss, TEXT("barnacleRadius"), 200.f), TEXT("/Game/SimpleRocks/3D_Assets/Meshes/Rocks/SM_SimpleRock_05A.SM_SimpleRock_05A"));
				Barnacle->OnLanded = [Weak](const FVector& Landed)
				{
					if (AWRBoss* B = Weak.Get()) WRFx::Broadcast(B, TEXT("hit.dust"), Landed, FRotator::ZeroRotator, 2.f, TEXT("sky.shard_fall"));
				};
			}
		}
		return true;
	}
	if (Id == TEXT("feather_volley"))
	{
		// Twelve quills in a fan from 6 m+ at 14 m/s, never homing; each one's line shows as it lifts. The wind-stones
		// (their collision) stop them.
		const int32 N = FMath::RoundToInt(SbAeo(TEXT("volleyQuills"), 12.f));
		const float Fan = SbAeo(TEXT("volleyFan"), 50.f);
		const FVector From = Boss->GetActorLocation() + FVector(0, 0, 150.f);
		for (int32 q = 0; q < N; ++q)
		{
			const FRotator Dir = Boss->AttackFacing + FRotator(FMath::FRandRange(-6.f, 4.f), -Fan * 0.5f + Fan * q / FMath::Max(1, N - 1), 0.f);
			FActorSpawnParameters P;
			P.Owner = Boss;
			P.Instigator = Boss;
			P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (AWRProjectile* Q = Boss->GetWorld()->SpawnActor<AWRProjectile>(AWRProjectile::StaticClass(), From, Dir, P))
			{
				FWRHit Hit = SbHit(Boss, Attack.Damage, false);
				Hit.bProjectile = true;
				Hit.Impact = 2;
				Q->Init(Hit, FMath::Min(Attack.Row.ProjectileSpeed > 0.f ? Attack.Row.ProjectileSpeed : 1400.f, 1400.f), SbHex(TEXT("E9E4D8")));
				Q->SetTrail(TEXT("wind.arrow_trail"));
			}
		}
		WRSound::PlayOn(Boss, TEXT("wind.volley_draw"));
		return true;
	}
	if (Id == TEXT("unbinding"))
	{
		// It coils round the Flue's axis and spins up a column of wind: the chains snap, the platforms drift apart.
		if (AWRSkyRing* R = Ring(Boss)) { R->UnboundAt = float(SbNow(Boss)); R->ForceNetUpdate(); }
		WRFx::Broadcast(Boss, TEXT("wind.vortex_burst"), Boss->GetActorLocation(), FRotator::ZeroRotator, 8.f, TEXT("creature.aeolith.unbinding"));
		return false;   // the ring of wind rides the self-circle's front
	}
	if (Id == TEXT("updraft_inhale"))
	{
		// Every player within 18 m lifted 8 m and held 1.2 s (brace, anchor boots, Skysilk 4 stop the lift); then the lash.
		S.Lifted.Reset();
		for (AWRCharacterBase* F : Foes(Boss, SbAeo(TEXT("inhaleReach"), 1800.f)))
		{
			ACharacter* Ch = Cast<ACharacter>(F);
			if (!Ch || WRSkyAir::PushScale(Ch, TEXT("lift")) <= 0.f) continue;
			WRSkyAir::Push(Ch, FVector(0, 0, FMath::Sqrt(2.f * 980.f * SbAeo(TEXT("inhaleLift"), 800.f)) * WRSkyAir::PushScale(Ch, TEXT("lift"))), 0.2f, TEXT("lift"), Boss);
			S.Lifted.Add(F);
		}
		S.LiftUntil = T + 0.9 + SbAeo(TEXT("inhaleHold"), 1.2f);
		TWeakObjectPtr<AWRBoss> Weak(Boss);
		// The tail's crimson glint and its sting lashTell s before the lash (cookpass-combat 3.11: 0.2 s was no tell).
		FTimerHandle TellTimer;
		Boss->GetWorldTimerManager().SetTimer(TellTimer, FTimerDelegate::CreateWeakLambda(Boss, [Weak]()
		{
			if (AWRBoss* B = Weak.Get()) B->MulticastTell(2, TEXT("tail"), TEXT("tell.final"));
		}), FMath::Max(0.05f, 0.9f + SbAeo(TEXT("inhaleHold"), 1.2f) - SbAeo(TEXT("lashTell"), 0.9f)), false);
		FTimerHandle H;
		Boss->GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(Boss, [Weak]()
		{
			// At the top: the tail lash across the lift (70; a crimson glint 0.2 s before).
			AWRBoss* B = Weak.Get();
			if (!B) return;
			FSbState& S2 = St(B);
			for (const TWeakObjectPtr<AWRCharacterBase>& W : S2.Lifted) if (AWRCharacterBase* C = W.Get())
			{
				if (C->GetCharacterMovement()->IsMovingOnGround()) continue;
				FWRHit Hit = SbHit(B, SbAeo(TEXT("inhaleLash"), 70.f), true);
				UWRCombatLibrary::ApplyHit(C, Hit);
			}
			S2.Lifted.Reset();
		}), 0.9f + SbAeo(TEXT("inhaleHold"), 1.2f), false);
		return true;
	}
	if (Id == TEXT("crosswind_dive") || Id == TEXT("spiral"))
	{
		// A straight dive across the Ring through its centre, the line locked at the fold (the plume counter uses it).
		if (AWRSkyRing* R = Ring(Boss))
		{
			const FVector Through = R->Centre() + FVector(0, 0, 200.f);
			Boss->AttackFacing = (Through - Boss->GetActorLocation()).GetSafeNormal2D().Rotation();
		}
		return false;   // the built line runs it
	}
	return false;
}

bool FWRSkyBoss::TickStrike(AWRBoss* Boss, FWRBossAttack& Attack, float DeltaSeconds)
{
	if (!IsSkyBoss(Boss)) return false;
	FSbState& S = St(Boss);
	const FName Id = Attack.Id;
	const double T = SbLocal(Boss);
	if (Id == TEXT("inhale"))
	{
		// The pull: 500 cm/s in its 20 m cone toward the mouth; reach the mouth and you're swallowed.
		const FVector Mouth = Boss->GetActorLocation() + Boss->GetActorForwardVector() * Boss->GetSimpleCollisionRadius();
		for (AWRCharacterBase* F : Foes(Boss, Attack.Range + 600.f))
		{
			const FVector D = F->GetActorLocation() - Mouth;
			if (FVector::DotProduct(D.GetSafeNormal(), Boss->GetActorForwardVector()) < FMath::Cos(FMath::DegreesToRadians(Attack.Size))) continue;
			if (D.Size() < SbT(Boss, TEXT("swallowReach"), 450.f) && !S.Held.IsValid())
			{
				// Swallowed: 4 s inside (12 a second), then spat out.
				S.Held = F;
				S.HeldHow = TEXT("swallow");
				S.HeldUntil = T + SbT(Boss, TEXT("swallowSeconds"), 4.f);
				S.HeldTickAt = T + 1.0;
				F->SetActorHiddenInGame(true);
				F->SetActorEnableCollision(false);
				Tell(F, TEXT("Swallowed! (mash to struggle)"));
				WRSound::PlayAt(Boss, TEXT("creature.old_gulper.swallow"), Mouth);
				continue;
			}
			if (ACharacter* Ch = Cast<ACharacter>(F)) WRSkyAir::Push(Ch, -D.GetSafeNormal() * SbT(Boss, TEXT("inhalePull"), 500.f), 0.15f, TEXT("inhale"), Boss);
		}
		return true;
	}
	if (Id == TEXT("vane_spin"))
	{
		// 15 every 0.25 s in 6 m, pushing out 6 m/s.
		if (T >= S.NextSpinTick)
		{
			S.NextSpinTick = T + SbT(Boss, TEXT("spinEvery"), 0.25f);
			for (AWRCharacterBase* F : Foes(Boss, Attack.Size + 100.f))
			{
				Boss->StrikeHits.Remove(F);
				Boss->StrikeHit(F);
				if (ACharacter* Ch = Cast<ACharacter>(F)) WRSkyAir::Push(Ch, (F->GetActorLocation() - Boss->GetActorLocation()).GetSafeNormal2D() * SbT(Boss, TEXT("spinPush"), 600.f), 0.25f, TEXT("vanes"), Boss);
			}
		}
		return true;
	}
	if (Id == TEXT("gale_gust") || Id == TEXT("held_gale"))
	{
		// The breath: a cone of wind from the mask; the edge kills; a wind-stone between you and the mask blocks it.
		AWRSkyRing* R = Ring(Boss);
		const FVector Mouth = Boss->GetActorLocation() + Boss->AttackFacing.Vector() * Boss->GetSimpleCollisionRadius();
		if (Boss->StrikeHits.Num() == 0) WRFx::Broadcast(Boss, TEXT("wind.gust"), Mouth, Boss->AttackFacing, Attack.Range / 100.f, TEXT("creature.aeolith.gale"), FMath::Max(0.1f, float(Boss->StrikeEnds - Boss->StrikeStarted)));
		for (AWRCharacterBase* F : Foes(Boss, Attack.Range + 200.f))
		{
			const FVector D = F->GetActorLocation() - Mouth;
			if (FVector::DotProduct(D.GetSafeNormal2D(), Boss->AttackFacing.Vector().GetSafeNormal2D()) < FMath::Cos(FMath::DegreesToRadians(Attack.Size))) continue;
			if (R && R->StoneBetween(Mouth, F->GetActorLocation(), float(SbNow(Boss)))) continue;
			Boss->StrikeHit(F);
		}
		S.ChestOpenUntil = Boss->StrikeEnds + SbAeo(TEXT("chestOpen"), 1.4f) - SbNow(Boss) + T;
		return true;
	}
	return false;
}

void FWRSkyBoss::EndStrike(AWRBoss* Boss, const FWRBossAttack& Attack)
{
	if (!IsSkyBoss(Boss)) return;
	UWRSkyBody* Body = UWRSkyBody::Get(Boss);
	if (Body)
	{
		Body->Set(WRSkyFlag::Hood, false);
		Body->Set(WRSkyFlag::Flap, false);
		Body->Set(WRSkyFlag::Crest, false);
		if (Boss->CreatureId != TEXT("old_gulper")) Body->Set(WRSkyFlag::Gape, false);
	}
	// Kyrra's stoop lands: a 4 m dust ring (low, 25).
	if (Attack.Id == TEXT("dive") && Boss->CreatureId == TEXT("kyrra"))
	{
		// Down in her bring-down window (diveGroundedSeconds, x1.25 taken): wings spread low over the ground, mantling.
		St(Boss).bStooping = false;
		if (UWRSkyBody* KBody = UWRSkyBody::Get(Boss))
		{
			KBody->Set(WRSkyFlag::Stoop, false);
			KBody->Set(WRSkyFlag::Grounded, true);
			KBody->Set(WRSkyFlag::Mantle, true);
		}
		const FVector Feet = Boss->GetActorLocation() - FVector(0, 0, Boss->GetSimpleCollisionHalfHeight());
		// The dust rolls out from where she struck (cookpass-combat 3.10): a ring, low, jumpable.
		FWRHit Hit = SbHit(Boss, SbT(Boss, TEXT("landRingDamage"), 25.f), false);
		Hit.bLow = true;
		Hit.Origin = Feet;
		AWRStrikeFront::Ring(Boss, Feet, SbT(Boss, TEXT("landRing"), 400.f), 1200.f, 160.f, AWRStrikeFront::HitWith(Hit), TEXT("land.dust"), TEXT("stomp"));
	}
	// Downburst: the column slams the platform, a ring 8 m (low, 30).
	if (Attack.Id == TEXT("downburst"))
	{
		// The column's blast rolls out across the platform from where it slammed (cookpass-combat 3.10).
		FWRHit Hit = SbHit(Boss, SbAeo(TEXT("downburstRingDamage"), 30.f), false);
		Hit.bLow = true;
		Hit.Origin = Boss->AttackPoint;
		AWRStrikeFront::Ring(Boss, Boss->AttackPoint, SbAeo(TEXT("downburstRing"), 800.f), 1200.f, 180.f, AWRStrikeFront::HitWith(Hit), NAME_None, TEXT("stomp"));
	}
	// The Sentinel's deck throw left a hole where it pried the stone (2 game hours).
	if (Attack.Id == TEXT("deck_throw"))
	{
		AWRSkyState* State = AWRSkyState::Get(Boss);
		const FVector Feet = Boss->GetActorLocation() - FVector(0, 0, Boss->GetSimpleCollisionHalfHeight());
		for (TActorIterator<AWRSkyScenery> It(Boss->GetWorld()); It && State; ++It)
		{
			int32 Best = INDEX_NONE;
			float BestD = 500.f;
			for (int32 i = 0; i < It->Slabs.Num(); ++i)
			{
				if (!It->Slabs[i] || (State->DeckOpen.IsValidIndex(i) && State->DeckOpen[i])) continue;
				const float D = FVector::Dist2D(It->Slabs[i]->GetComponentLocation(), Feet + Boss->GetActorForwardVector() * 300.f);
				if (D < BestD) { BestD = D; Best = i; }
			}
			if (Best == INDEX_NONE) continue;
			State->DeckOpen.SetNumZeroed(It->Slabs.Num());
			State->DeckCloseAt.SetNumZeroed(It->Slabs.Num());
			State->DeckOpen[Best] = 1;
			State->DeckCloseAt[Best] = AWRGameState::Get(Boss) ? AWRGameState::Get(Boss)->GetTotalHours() + SbT(Boss, TEXT("holeHours"), 2.f) : 0.0;
			State->OnRep_Deck();
			State->ForceNetUpdate();
		}
	}
}

void FWRSkyBoss::HitLanded(AWRBoss* Boss, AWRCharacterBase* Victim, FName AttackId, EWRHitResult Result)
{
	if (!IsSkyBoss(Boss) || !Victim) return;
	FSbState& S = St(Boss);
	const bool bLanded = Result == EWRHitResult::Hit || Result == EWRHitResult::GuardBroken;
	if (!bLanded) return;
	const double T = SbLocal(Boss);
	if (AttackId == TEXT("snatch") && !S.Held.IsValid() && Victim->IsAlive())
	{
		// Up in the talons (Kyrra: 20 m over 3 s) or the jaws (Aeolith: 15 m over 2.5 s, a tame carried over the Flue).
		const bool bKyrra = Boss->CreatureId == TEXT("kyrra");
		S.Held = Victim;
		S.HeldHow = bKyrra ? FName(TEXT("snatch_kyrra")) : FName(TEXT("snatch_aeolith"));
		S.HeldUntil = T + (bKyrra ? SbT(Boss, TEXT("snatchSeconds"), 3.f) : SbAeo(TEXT("snatchSeconds"), 2.5f));
		S.HeldFrom = Boss->GetActorLocation();
		S.FreeStance = 0.f;
		if (!bKyrra)
		{
			// Aeolith drops a tame over the Flue's open middle; a survivor on another platform (P2: into a gap).
			if (AWRSkyRing* R = Ring(Boss))
			{
				const bool bTame = Victim->IsA<AWRCreature>();
				const float Now = float(SbNow(Boss));
				const int32 Far = (R->NearestPlatform(Victim->GetActorLocation(), Now, S.HeldFrom) + 4) % R->Count();
				S.HeldFrom = bTame ? R->Centre() : (Boss->Phase >= 2 ? (R->PlatformAt(Far, Now) + R->PlatformAt((Far + 1) % R->Count(), Now)) * 0.5f : R->PlatformAt(Far, Now));
			}
		}
		if (UWRSkyBody* Body = UWRSkyBody::Get(Boss)) Body->Holding = Victim;
		Tell(Victim, TEXT("Snatched!"));
		return;
	}
	if (AttackId == TEXT("grab_hurl"))
	{
		// Hurled 12 m sideways (the edge kills).
		const FVector Side = Boss->GetActorRightVector() * (FMath::RandBool() ? 1.f : -1.f);
		if (ACharacter* Ch = Cast<ACharacter>(Victim)) WRSkyAir::Push(Ch, Side * SbT(Boss, TEXT("hurl"), 1200.f) + FVector(0, 0, 400.f), 0.6f, TEXT("hurl"), Boss);
	}
}

float FWRSkyBoss::Incoming(AWRBoss* Boss, const FWRHit& Hit, float Damage)
{
	if (!IsSkyBoss(Boss)) return Damage;
	FSbState& S = St(Boss);
	const double T = SbLocal(Boss);
	const FName Id = Boss->CreatureId;
	if (Id == TEXT("kyrra") && !Boss->bAirborne && SbNow(Boss) < Boss->GroundedUntil) Damage *= SbT(Boss, TEXT("groundedTaken"), 1.25f);
	if (Id == TEXT("bridge_sentinel") && !S.bAwake)
	{
		// Struck where it kneels: it wakes.
		S.bAwake = true;
		WRSound::Broadcast(Boss, TEXT("sky.sentinel_wake"));
	}
	if (Id == TEXT("old_gulper"))
	{
		// Fire in the mouth while it inhales: it chokes. Twice in a minute cracks the hump.
		const bool bInhaling = Boss->Current != INDEX_NONE && Boss->Attacks.IsValidIndex(Boss->Current) && Boss->Attacks[Boss->Current].Id == TEXT("inhale");
		const FVector At = Hit.ImpactPoint.IsZero() ? Hit.Origin : Hit.ImpactPoint;
		const FVector Mouth = Boss->GetActorLocation() + Boss->GetActorForwardVector() * Boss->GetSimpleCollisionRadius();
		if (bInhaling && Hit.Element == WRTags::Element_Fire && FVector::Dist(At, Mouth) < SbT(Boss, TEXT("mouthReach"), 900.f) && S.ChokeUntil <= T)
		{
			S.ChokeUntil = T + SbT(Boss, TEXT("chokeSeconds"), 10.f);
			S.Chokes.Add(T);
			S.Chokes.RemoveAll([&](double C) { return T - C > SbT(Boss, TEXT("crackWindow"), 60.f); });
			if (S.Chokes.Num() >= 2 && !S.bCracked)
			{
				S.bCracked = true;
				WRFx::Broadcast(Boss, TEXT("hit.dust"), Boss->GetActorLocation() + FVector(0, 0, Boss->GetSimpleCollisionHalfHeight()), FRotator::ZeroRotator, 6.f, TEXT("sky.shard_fall"));
				UE_LOG(LogWildreach, Log, TEXT("BOSS Old Gulper's hump cracks"));
			}
			Boss->Current = INDEX_NONE;
			Boss->StrikeState = AWRBoss::EStrike::None;
			WRFx::Broadcast(Boss, TEXT("smoke.puff"), Mouth, FRotator::ZeroRotator, 4.f, TEXT("creature.old_gulper.choke"));
			UE_LOG(LogWildreach, Log, TEXT("BOSS Old Gulper chokes"));
		}
		if (T < S.ChokeUntil) Damage *= SbT(Boss, TEXT("chokeTaken"), 1.5f);
	}
	if (Id == TEXT("aeolith"))
	{
		// The Gale Crown: shots entering 3 m of it are thrown aside (melee and spells ignore it) until it's knocked off.
		if (Hit.bProjectile && T >= S.CrownOffUntil && S.WeightedUntil <= T)
		{
			WRFx::Broadcast(Boss, TEXT("wind.gust"), Hit.ImpactPoint.IsZero() ? Boss->GetActorLocation() : FVector(Hit.ImpactPoint), FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), 2.f, TEXT("wind.deflect"));
			return 0.f;
		}
		// Weak to Spore (x1.5); spore-weighted x1.25; the chest open after a breath x1.3; phase 3: only the vents and the wings.
		if (Hit.PathId == TEXT("spore") || Hit.Buildups.Contains(TEXT("spore"))) Damage *= SbAeo(TEXT("weakSpore"), 1.5f);
		if (S.WeightedUntil > T) Damage *= SbAeo(TEXT("weightedTaken"), 1.25f);
		if (T < S.ChestOpenUntil) Damage *= SbAeo(TEXT("chestTaken"), 1.3f);
		if (Hit.Buildups.Contains(TEXT("spore"))) S.Spore += Hit.Buildups.FindRef(TEXT("spore"));
		if (Boss->Phase >= 3 && S.bVentsUp)
		{
			// A blow near a living vent goes into it; elsewhere on its body (melee) it glances off; arrows into the wings count.
			const FVector At = Hit.ImpactPoint.IsZero() ? Hit.Origin : Hit.ImpactPoint;
			const FVector Back = Boss->GetActorLocation() + FVector(0, 0, Boss->GetSimpleCollisionHalfHeight());
			for (int32 v = 0; v < 3; ++v)
			{
				if (S.Vent[v] <= 0.f) continue;
				const FVector VentAt = Back - Boss->GetActorForwardVector() * (300.f + v * 1800.f);
				if (FVector::Dist(At, VentAt) > 600.f && !(IsRiding(Hit.Instigator) && S.RiderSlot.IsValidIndex(S.Riders.IndexOfByKey(Cast<AWRCharacterBase>(Hit.Instigator))) && S.RiderSlot[S.Riders.IndexOfByKey(Cast<AWRCharacterBase>(Hit.Instigator))] == v)) continue;
				S.Vent[v] -= Damage;
				WRFx::Broadcast(Boss, TEXT("hit.dust"), VentAt, FRotator::ZeroRotator, 1.f, NAME_None);
				if (S.Vent[v] <= 0.f)
				{
					// A vent broken: 8% of its health and a 2 s stagger.
					Boss->Status->Stagger(SbAeo(TEXT("ventStagger"), 2.f));
					WRSound::Voice(Boss, TEXT("hurt"));
					if (S.Vent[0] <= 0.f && S.Vent[1] <= 0.f && S.Vent[2] <= 0.f)
					{
						// All three: it crashes onto the largest platform for 10 s (the riposte window).
						S.bVentsUp = false;
						S.CrashUntil = T + SbAeo(TEXT("crashSeconds"), 10.f);
						for (const TWeakObjectPtr<AWRCharacterBase>& W : S.Riders) if (AWRCharacterBase* C = W.Get()) { C->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform); C->SetActorEnableCollision(true); C->GetCharacterMovement()->SetMovementMode(MOVE_Falling); }
						S.Riders.Reset();
						S.RiderSlot.Reset();
						if (AWRSkyRing* R = Ring(Boss))
						{
							int32 Big = 0;
							for (int32 i = 1; i < R->Count(); ++i) if (R->PlatformRadius(i) > R->PlatformRadius(Big)) Big = i;
							Boss->SetActorLocation(R->PlatformAt(Big, float(SbNow(Boss))) + FVector(0, 0, Boss->GetSimpleCollisionHalfHeight() + 60.f), false, nullptr, ETeleportType::TeleportPhysics);
						}
						UWRCombatLibrary::BreakStance(Boss, SbAeo(TEXT("crashSeconds"), 10.f), Hit.Instigator);
					}
					return Boss->GetAttributes()->GetMaxHealth() * SbAeo(TEXT("ventShare"), 0.08f);
				}
				return 0.f;
			}
			// Arrows into its wings count in full; anything else glances off.
			return Hit.bProjectile ? Damage : 0.f;
		}
	}
	return Damage;
}

bool FWRSkyBoss::OwnWeakness(const AWRBoss* Boss)
{
	// Aeolith's weakness is the Spore path (applied in Incoming), not a fire / frost element.
	return Boss && Boss->CreatureId == TEXT("aeolith");
}

void FWRSkyBoss::Damaged(AWRBoss* Boss, const FWRHit& Hit, float Damage)
{
	if (!IsSkyBoss(Boss)) return;
	FSbState& S = St(Boss);
	const float Stance = Hit.StanceDamage >= 0.f ? Hit.StanceDamage : Hit.PoiseDamage;
	// A carry broken by allies' stance on the carrier.
	if (S.Held.IsValid() && Hit.Instigator != S.Held.Get()) S.FreeStance += Stance;
	// Kyrra's wings take arrows twice over (ranged x2 on the part).
	if (Boss->CreatureId == TEXT("kyrra") && Hit.bProjectile && Stance > 0.f) Boss->DamagePart(Hit, Stance * (SbT(Boss, TEXT("rangedPartMult"), 2.f) - 1.f), Hit.Instigator);
	// Aeolith: a stance break or a broken wing drops the Crown for 20 s.
	if (Boss->CreatureId == TEXT("aeolith") && (Boss->HasTag(WRTags::Status_StanceBroken) || (Boss->IsPartBroken(TEXT("wing_left")) || Boss->IsPartBroken(TEXT("wing_right"))) && !Boss->bAirborne))
	{
		S.CrownOffUntil = FMath::Max(S.CrownOffUntil, SbLocal(Boss) + SbAeo(TEXT("crownOff"), 20.f));
	}
}

void FWRSkyBoss::PhaseChanged(AWRBoss* Boss, int32 NewPhase)
{
	if (!IsSkyBoss(Boss) || Boss->CreatureId != TEXT("aeolith")) return;
	if (NewPhase >= 3)
	{
		// The Last Circle: it never lands again; its three crest vents rise along its back.
		FSbState& S = St(Boss);
		for (float& V : S.Vent) V = SbAeo(TEXT("ventHealth"), 900.f);
		S.bVentsUp = true;
		S.NextPassAt = SbLocal(Boss) + SbAeo(TEXT("passEvery"), 20.f);
		S.NextRollAt = SbLocal(Boss) + SbAeo(TEXT("rollEvery"), 12.f);
		Boss->SetAirborne(true);
		UE_LOG(LogWildreach, Log, TEXT("BOSS Aeolith: the Last Circle"));
	}
}

bool FWRSkyBoss::PlaceRunback(AWRBoss* Boss)
{
	if (!IsSkyBoss(Boss)) return false;
	// Kyrra roams (no stone); the others' stand where the arenas say (Data/sky.json arenas: the Lid Gate for Aeolith).
	if (Boss->CreatureId == TEXT("kyrra")) return true;
	const FWRSkyIsleData::FArena* A = FWRSkyIsleData::Get().FindArena(Boss->CreatureId);
	if (!A || A->Runback.IsNearlyZero()) return true;
	for (TActorIterator<AWRRunbackStone> It(Boss->GetWorld()); It; ++It) if (It->BossId == Boss->CreatureId) { Boss->Runback = *It; return true; }
	FVector At = A->Runback;
	float Gz = At.Z;
	if (WRSkyIsles::GroundUnder(At + FVector(0, 0, 2000.f), Gz)) At.Z = Gz;
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FRotator Face(0.f, (A->At - At).Rotation().Yaw, 0.f);
	if (AWRRunbackStone* Stone = Boss->GetWorld()->SpawnActor<AWRRunbackStone>(AWRRunbackStone::StaticClass(), At, Face, P))
	{
		Stone->BossId = Boss->CreatureId;
		Boss->Runback = Stone;
	}
	return true;
}

FName FWRSkyBoss::BossPart(const AWRBoss* Boss, bool bScarred, FName Fallback)
{
	if (!Boss || Boss->CreatureId != TEXT("aeolith")) return Fallback;
	return TEXT("part_gale_plume");
}

void FWRSkyBoss::Died(AWRBoss* Boss)
{
	if (!IsSkyBoss(Boss)) return;
	FSbState& S = St(Boss);
	if (AWRCharacterBase* V = S.Held.Get()) { V->SetActorHiddenInGame(false); V->SetActorEnableCollision(true); V->GetCharacterMovement()->SetMovementMode(MOVE_Falling); }
	for (const TWeakObjectPtr<AWRCharacterBase>& W : S.Riders) if (AWRCharacterBase* C = W.Get()) { C->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform); C->SetActorEnableCollision(true); C->GetCharacterMovement()->SetMovementMode(MOVE_Falling); }
	// Old Gulper's cracked hump: no barnacle drop (its levistone loot is the barnacles).
	if (Boss->CreatureId == TEXT("old_gulper") && S.bCracked)
	{
		for (const TPair<FString, float>& E : Boss->Ledger)
		{
			for (APlayerState* P : Boss->GetWorld()->GetGameState()->PlayerArray)
			{
				if (P && P->GetPlayerName() == E.Key) if (AWRPlayerState* PS = Cast<AWRPlayerState>(P)) PS->GetInventory()->RemoveItem(TEXT("levistone"), FMath::Min(PS->GetInventory()->CountOf(TEXT("levistone")), 12));
			}
		}
	}
	S = FSbState();
}

bool FWRSkyBoss::Mash(AWRCharacterBase* Victim)
{
	if (!Victim) return false;
	for (TPair<TWeakObjectPtr<const AWRBoss>, FSbState>& E : States())
	{
		FSbState& S = E.Value;
		if (S.Held.Get() == Victim)
		{
			S.HeldUntil -= 0.15;
			return true;
		}
		const int32 R = S.Riders.IndexOfByKey(TWeakObjectPtr<AWRCharacterBase>(Victim));
		if (R != INDEX_NONE)
		{
			// A rider's Jump lets go.
			Victim->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			Victim->SetActorEnableCollision(true);
			Victim->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
			S.Riders.RemoveAt(R);
			if (S.RiderSlot.IsValidIndex(R)) S.RiderSlot.RemoveAt(R);
			return true;
		}
	}
	return false;
}

bool FWRSkyBoss::IsHeld(const AActor* Who)
{
	for (const TPair<TWeakObjectPtr<const AWRBoss>, FSbState>& E : States()) if (E.Value.Held.Get() == Who) return true;
	// Clients: a body holding them (the replicated Holding on the boss's body).
	for (const TWeakObjectPtr<UWRSkyBody>& B : UWRSkyBody::Live) if (B.IsValid() && B->Holding == Who && Cast<AWRBoss>(B->GetOwner())) return true;
	return false;
}

bool FWRSkyBoss::IsRiding(const AActor* Who)
{
	const AActor* Parent = Who ? Who->GetAttachParentActor() : nullptr;
	const AWRBoss* B = Cast<AWRBoss>(Parent);
	return B && B->CreatureId == TEXT("aeolith");
}

void FWRSkyBoss::SporeLanded(UWorld* World, const FVector& At, AActor* Thrower)
{
	if (!World || World->GetNetMode() == NM_Client) return;
	WRFx::Broadcast(Thrower ? Thrower : nullptr, TEXT("sky.spore_burst"), At, FRotator::ZeroRotator, 1.5f, TEXT("sky.spore_pop"));
	for (TActorIterator<AWRBoss> It(World); It; ++It)
	{
		if (It->CreatureId != TEXT("aeolith") || !It->IsAlive()) continue;
		if (FVector::Dist(It->GetActorLocation(), At) < It->GetSimpleCollisionRadius() + 500.f)
		{
			St(*It).Spore += SbAeo(TEXT("sporePot"), 150.f);
			UE_LOG(LogWildreach, Log, TEXT("BOSS Aeolith hit by a spore pot: %.0f"), St(*It).Spore);
		}
	}
}

bool FWRSkyBoss::BuildThrownModel(const FString& ModelPath, UProceduralMeshComponent* Mesh)
{
	if (!Mesh || ModelPath != TEXT("code:sporepot")) return false;
	// The spore pot (SKY 2.11's counter, made from the Flue's deepcap spores): a round clay pot 14 cm long, the spores'
	// cyan bloom crusting its shoulder and the cracks of its glaze, a wax-sealed cork, a twine sling round its neck.
	FWRToolMesh M;
	const FLinearColor Clay = FLinearColor(FColor(150, 112, 84)) * 0.55f, Spore = FLinearColor(FColor(57, 230, 194)) * 0.62f;
	const FLinearColor Wax = FLinearColor(FColor(198, 160, 92)) * 0.55f, Twine = FLinearColor(FColor(120, 98, 70)) * 0.5f;
	M.Lathe(FVector(-7.f, 0.f, 0.f), FVector::ForwardVector, 14.f, { FVector(0.f, 3.f, 3.f), FVector(0.15f, 6.f, 6.f), FVector(0.5f, 7.f, 7.f), FVector(0.82f, 5.f, 5.f), FVector(0.92f, 2.8f, 2.8f), FVector(1.f, 3.2f, 3.2f) }, 12, Clay, 0.9f);
	M.Lathe(FVector(-1.f, 0.f, 0.f), FVector::ForwardVector, 6.f, { FVector(0.f, 7.05f, 7.05f), FVector(0.5f, 6.6f, 6.6f), FVector(1.f, 5.3f, 5.3f) }, 12, Spore, 0.6f);
	M.Lathe(FVector(6.6f, 0.f, 0.f), FVector::ForwardVector, 2.4f, { FVector(0.f, 2.6f, 2.6f), FVector(1.f, 2.9f, 2.9f) }, 8, Wax, 0.5f);
	M.Tube({ FVector(5.5f, 3.6f, 0.f), FVector(5.5f, 0.f, 3.6f), FVector(5.5f, -3.6f, 0.f), FVector(5.5f, 0.f, -3.6f) }, 0.4f, 5, Twine, 0.85f, true);
	M.ApplyTo(Mesh);
	return true;
}

// ================================================================================================ the arenas

bool UWRSkyArenas::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* W = Cast<UWorld>(Outer);
	return W && (W->WorldType == EWorldType::Game || W->WorldType == EWorldType::PIE);
}

bool UWRSkyArenas::IsTickable() const
{
	const UWorld* W = GetWorld();
	return W && W->GetNetMode() != NM_Client && WRSkyIsles::Live();
}

UWRSkyArenas* UWRSkyArenas::Get(const UObject* WorldContext)
{
	const UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	return W ? W->GetSubsystem<UWRSkyArenas>() : nullptr;
}

bool UWRSkyArenas::Place(FName BossId)
{
	UWorld* World = GetWorld();
	const FWRSkyIsleData& D = FWRSkyIsleData::Get();
	const FWRSkyIsleData::FArena* A = D.FindArena(BossId);
	if (!A || !FWRCreatureRegistry::Get().Find(BossId)) return true;
	// Only once someone's in the Sky (the islands build on every machine first).
	bool bNear = false;
	for (TActorIterator<AWRCharacter> It(World); It && !bNear; ++It) bNear = It->IsAlive() && WRSkyIsles::Contains(It->GetActorLocation());
	if (!bNear) return false;
	if (UWRSkyWorld* W = UWRSkyWorld::Get(World); !W || !W->IsBuilt()) return false;
	for (TActorIterator<AWRCreatureSpawner> It(World); It; ++It) if (It->CreatureId == BossId) return true;
	FVector Start = A->At;
	if (BossId == TEXT("aeolith"))
	{
		// The Ring (its platforms and stones), the Wind altar in the Lid Gate's temple.
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		World->SpawnActor<AWRSkyRing>(AWRSkyRing::StaticClass(), D.RingAt, FRotator::ZeroRotator, P);
		if (!A->Altar.IsNearlyZero())
		{
			FVector Al = A->Altar;
			float Gz = Al.Z;
			if (WRSkyIsles::GroundUnder(Al + FVector(0, 0, 2000.f), Gz)) Al.Z = Gz + 50.f;
			const FTransform T(FRotator(0.f, (A->At - Al).Rotation().Yaw, 0.f), Al);
			if (AWRBossAltar* Altar = World->SpawnActorDeferred<AWRBossAltar>(AWRBossAltar::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
			{
				Altar->PathId = TEXT("wind");
				Altar->FinishSpawning(T);
			}
		}
		Start = D.RingAt + FVector(SbAeo(TEXT("circleRadius"), 4200.f), 0.f, SbAeo(TEXT("circleHeight"), 800.f));
	}
	else if (BossId == TEXT("bridge_sentinel"))
	{
		// It kneels on the bridge's middle (the standing deck, Tollgate to Span North).
		if (const FWRSkyIsleData::FBridge* Br = D.FindBridge(TEXT("sentinel_deck"))) Start = FMath::Lerp(Br->A, Br->B, 0.5f) + FVector(0, 0, 50.f);
	}
	else if (BossId == TEXT("old_gulper"))
	{
		Start = A->At + FVector(0, 0, 400.f + 450.f);
	}
	else if (BossId == TEXT("kyrra"))
	{
		if (D.KyrraRoute.Num()) Start = D.KyrraRoute[0];
	}
	const FTransform Where(FRotator::ZeroRotator, Start + FVector(0, 0, 50.f));
	if (AWRCreatureSpawner* Spawner = World->SpawnActorDeferred<AWRCreatureSpawner>(AWRCreatureSpawner::StaticClass(), Where, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		Spawner->CreatureId = BossId;
		Spawner->Count = 1;
		Spawner->Radius = 1.f;
		Spawner->RespawnSeconds = WRTune(TEXT("server.bossRespawnHours"), 4.f) * 3600.f;
		Spawner->ActivationRadius = 0.f;
		Spawner->bTraceGround = BossId == TEXT("bridge_sentinel");
		Spawner->FinishSpawning(Where);
	}
	UE_LOG(LogWildreach, Log, TEXT("SKY arena %s placed at %s"), *BossId.ToString(), *Start.ToCompactString());
	return true;
}

void UWRSkyArenas::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	if (!World || !World->HasBegunPlay()) return;
	PlaceClock += DeltaTime;
	if (PlaceClock < 2.f) return;
	PlaceClock = 0.f;
	static const FName Ids[] = { TEXT("kyrra"), TEXT("bridge_sentinel"), TEXT("old_gulper"), TEXT("aeolith") };
	for (const FName Id : Ids)
	{
		if (Placed.Contains(Id)) continue;
		if (Place(Id)) Placed.Add(Id);
	}
	// The deck's holes close again (the 2 game hours: the Kitewrights' old masonry settling back, a cold forge's mend).
	if (AWRSkyState* S = AWRSkyState::Get(World))
	{
		const double Hours = AWRGameState::Get(World) ? AWRGameState::Get(World)->GetTotalHours() : 0.0;
		bool bChanged = false;
		for (int32 i = 0; i < S->DeckOpen.Num(); ++i)
		{
			if (S->DeckOpen[i] && S->DeckCloseAt.IsValidIndex(i) && Hours >= S->DeckCloseAt[i])
			{
				// Not under the Sentinel while it still fights on Winch Isle below (the hole is how it fell).
				S->DeckOpen[i] = 0;
				bChanged = true;
			}
		}
		if (bChanged) { S->OnRep_Deck(); S->ForceNetUpdate(); }
	}
}

namespace WRSkyArenaImpl
{
	/** The Last Circle's back in reach: Aeolith on a low pass within grabReach of the survivor (every machine reads it). */
	AWRBoss* BackInReach(const AActor* Who)
	{
		if (!Who) return nullptr;
		for (TActorIterator<AWRBoss> It(Who->GetWorld()); It; ++It)
		{
			if (It->CreatureId != TEXT("aeolith") || !It->IsAlive() || It->Phase < 3) continue;
			const FVector Back = It->GetActorLocation() + FVector(0, 0, It->GetSimpleCollisionHalfHeight());
			if (FVector::Dist(Back, Who->GetActorLocation()) < SbAeo(TEXT("grabReach"), 400.f) + It->GetSimpleCollisionRadius()) return *It;
		}
		return nullptr;
	}
}

bool UWRSkyArenas::Prompt(const AWRPlayerController* PC, FString& OutTarget, FString& OutVerb)
{
	const APawn* P = PC ? PC->GetPawn() : nullptr;
	if (FWRSkyBoss::IsRiding(P)) { OutTarget = TEXT("Aeolith's back"); OutVerb = TEXT("Run to the next vent"); return true; }
	if (AWRBoss* B = WRSkyArenaImpl::BackInReach(P)) { OutTarget = B->GetDisplayLabel(); OutVerb = TEXT("Grab on"); return true; }
	return false;
}

bool UWRSkyArenas::ClientInteract(AWRPlayerController* PC)
{
	const APawn* P = PC ? PC->GetPawn() : nullptr;
	if (!FWRSkyBoss::IsRiding(P) && !WRSkyArenaImpl::BackInReach(P)) return false;
	PC->SendCommand(FWRSkyBoss::IsRiding(P) ? TEXT("world sky boss shift") : TEXT("world sky boss grab"));
	return true;
}

FString UWRSkyArenas::Command(AWRPlayerController* PC, const TArray<FString>& Args)
{
	AWRCharacter* Me = PC ? Cast<AWRCharacter>(PC->GetPawn()) : nullptr;
	if (!Me || Args.Num() < 3 || Args[1] != TEXT("boss")) return FString();
	if (Args[2] == TEXT("grab"))
	{
		AWRBoss* B = WRSkyArenaImpl::BackInReach(Me);
		if (!B) return TEXT("Out of reach");
		FSbState& S = St(B);
		if (S.Riders.Num() >= FMath::RoundToInt(SbAeo(TEXT("riders"), 4.f))) return TEXT("No room on its back");
		// On its back at the first vent behind the head; the vents are along the spine (the crest).
		const int32 Slot = S.Riders.Num() % 3;
		Me->SetActorEnableCollision(false);
		Me->AttachToActor(B, FAttachmentTransformRules::KeepWorldTransform);
		Me->SetActorRelativeLocation(FVector(-300.f - Slot * 1800.f, 0.f, B->GetSimpleCollisionHalfHeight() + Me->GetSimpleCollisionHalfHeight()));
		S.Riders.Add(Me);
		S.RiderSlot.Add(Slot);
		return TEXT("On its back: strike the vents. Hold Guard when it rolls; Jump lets go");
	}
	if (Args[2] == TEXT("shift"))
	{
		AWRBoss* B = Cast<AWRBoss>(Me->GetAttachParentActor());
		if (!B) return FString();
		FSbState& S = St(B);
		const int32 R = S.Riders.IndexOfByKey(TWeakObjectPtr<AWRCharacterBase>(Me));
		if (R == INDEX_NONE || !S.RiderSlot.IsValidIndex(R)) return FString();
		// Run the crest: to the next living vent along the spine.
		int32 Next = S.RiderSlot[R];
		for (int32 k = 1; k <= 3; ++k) { const int32 V = (S.RiderSlot[R] + k) % 3; if (S.Vent[V] > 0.f) { Next = V; break; } }
		S.RiderSlot[R] = Next;
		Me->SetActorRelativeLocation(FVector(-300.f - Next * 1800.f, 0.f, B->GetSimpleCollisionHalfHeight() + Me->GetSimpleCollisionHalfHeight()));
		return FString::Printf(TEXT("At the %s vent"), Next == 0 ? TEXT("head's") : Next == 1 ? TEXT("mid-back") : TEXT("tail's"));
	}
	return FString();
}
