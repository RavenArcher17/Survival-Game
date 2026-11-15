// The Fungal caverns' hazards (see WRFungalHazards.h; cooks/biome-fungal.md 2.8-2.10; cooks/build-biome-fungal.md 2.8).

#include "WRFungalHazards.h"
#include "Wildreach.h"
#include "WRCharacterBase.h"
#include "WRCombatLibrary.h"
#include "WRCombatTypes.h"
#include "WRCreature.h"
#include "WRFungalLook.h"
#include "WRFungalTypes.h"
#include "WRFungalWorld.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRInventory.h"
#include "WRPathComponent.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRStormTypes.h"
#include "WRSurvival.h"
#include "WRToolMesh.h"
#include "WRTuning.h"
#include "WRWorldSystems.h"
#include "AbilitySystemComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"

namespace WRFungalTags
{
	UE_DEFINE_GAMEPLAY_TAG(FoulAir, "Fungal.FoulAir");
	UE_DEFINE_GAMEPLAY_TAG(Bloomed, "Fungal.Bloomed");
	UE_DEFINE_GAMEPLAY_TAG(Summited, "Fungal.Summited");
	UE_DEFINE_GAMEPLAY_TAG(GlowPaste, "Fungal.GlowPaste");
}

namespace WRFungalHazImpl
{
	float FhT(const TCHAR* Key, float Def) { return WRTune(Key, Def); }
	float FhHours(float Hours) { return Hours * WRTune(TEXT("clock.dayMinutes"), 60.f) * 60.f / 24.f; }
	float FhSmooth(float T) { T = FMath::Clamp(T, 0.f, 1.f); return T * T * (3.f - 2.f * T); }
	TArray<TWeakObjectPtr<AWRSporeCloud>>& Clouds() { static TArray<TWeakObjectPtr<AWRSporeCloud>> L; return L; }
	TArray<TWeakObjectPtr<AWRLureLines>>& LineSets() { static TArray<TWeakObjectPtr<AWRLureLines>> L; return L; }
	TArray<TWeakObjectPtr<AWRSpringcap>>& Caps() { static TArray<TWeakObjectPtr<AWRSpringcap>> L; return L; }
	TArray<TWeakObjectPtr<AWRCapFall>>& Falls() { static TArray<TWeakObjectPtr<AWRCapFall>> L; return L; }
	TArray<TWeakObjectPtr<AWRFungalSpot>>& Spots() { static TArray<TWeakObjectPtr<AWRFungalSpot>> L; return L; }
	void Prune(TArray<TWeakObjectPtr<AWRSporeCloud>>& L) { L.RemoveAll([](const TWeakObjectPtr<AWRSporeCloud>& W) { return !W.IsValid(); }); }

	/** A world hazard's blow (no instigator): through ApplyHit like every damage (CLAUDE.md). */
	void FhHit(AWRCharacterBase* Who, AActor* By, float Damage, const FGameplayTag& Element, float Burn, uint8 Impact, const FVector& From, bool bDot = false)
	{
		if (!Who || !Who->IsAlive()) return;
		FWRHit H;
		H.Instigator = By;
		H.Damage = Damage;
		H.Element = Element;
		H.BurnBuildup = Burn;
		H.Impact = Impact;
		H.Origin = From;
		H.Tier = Impact >= 3 ? EWRHitTier::Heavy : EWRHitTier::Light;
		H.bDamageOverTime = bDot;
		H.bReaction = true;
		UWRCombatLibrary::ApplyHit(Who, H);
	}

	/** Every live character near a point (players, creatures, tames). */
	template <typename F> void ForCharsNear(UWorld* W, const FVector& At, float R, F&& Fn)
	{
		const float R2 = R * R;
		for (TActorIterator<AWRCharacterBase> It(W); It; ++It)
		{
			if (It->IsAlive() && FVector::DistSquared(It->GetActorLocation(), At) <= R2) Fn(*It);
		}
	}
}

// ================================================================================================ the shared API

float WRFungal::Now(const UObject* WorldContext)
{
	const UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	const AGameStateBase* GS = W ? W->GetGameState() : nullptr;
	return GS ? float(GS->GetServerWorldTimeSeconds()) : (W ? W->GetTimeSeconds() : 0.f);
}

bool WRFungal::BloomTell(const UObject* WorldContext)
{
	const float Every = FMath::Max(60.f, WRTune(TEXT("hazards.fungal.bloom.every"), 1200.f));
	const float Len = WRTune(TEXT("hazards.fungal.bloom.seconds"), 180.f), Tell = WRTune(TEXT("hazards.fungal.bloom.tell"), 30.f);
	const float T = FMath::Fmod(Now(WorldContext), Every);
	return T >= Every - Len - Tell && T < Every - Len;
}

bool WRFungal::BloomOn(const UObject* WorldContext)
{
	const float Every = FMath::Max(60.f, WRTune(TEXT("hazards.fungal.bloom.every"), 1200.f));
	const float Len = WRTune(TEXT("hazards.fungal.bloom.seconds"), 180.f);
	return FMath::Fmod(Now(WorldContext), Every) >= Every - Len;
}

uint8 WRFungal::Sporefall(const UObject* WorldContext)
{
	const AWRFungalState* S = AWRFungalState::Get(WorldContext);
	return S ? S->Sporefall : 0;
}

float WRFungal::VentEvery(const UObject* WorldContext)
{
	return Sporefall(WorldContext) == 2 ? WRTune(TEXT("hazards.fungal.vent.sporefallEvery"), 30.f) : WRTune(TEXT("hazards.fungal.vent.every"), 40.f);
}

bool WRFungal::CarriesFire(const AActor* Who)
{
	const AWRCharacterBase* C = Cast<AWRCharacterBase>(Who);
	if (!C) return false;
	if (UWRSurvivalComponent::HoldsTorch(C)) return true;
	if (C->Weapon.Element == TEXT("fire")) return true;
	if (C->HasTag(WRTags::Status_Burn)) return true;
	// A lantern that isn't cold (horn, beacon, tallow: a flame).
	if (const UWRSurvivalComponent* S = UWRSurvivalComponent::For(C))
	{
		if (S->IsLightOn())
		{
			const TSharedPtr<FJsonObject> Row = FWRTuning::Get().Object(FString::Printf(TEXT("lights.%s"), *S->GetLightItem().ToString()));
			double Cold = 0.0;
			if (!(Row.IsValid() && Row->TryGetNumberField(TEXT("cold"), Cold) && Cold > 0.0)) return true;
		}
	}
	return false;
}

bool WRFungal::CarriesColdLight(const AActor* Who)
{
	const AWRCharacterBase* C = Cast<AWRCharacterBase>(Who);
	if (!C) return false;
	if (C->HasTag(WRFungalTags::GlowPaste)) return true;
	if (const UWRSurvivalComponent* S = UWRSurvivalComponent::For(C))
	{
		if (S->IsLightOn())
		{
			const TSharedPtr<FJsonObject> Row = FWRTuning::Get().Object(FString::Printf(TEXT("lights.%s"), *S->GetLightItem().ToString()));
			double Cold = 0.0;
			if (Row.IsValid() && Row->TryGetNumberField(TEXT("cold"), Cold) && Cold > 0.0) return true;
		}
	}
	// A tamed Glowgrub following within 15 m (fungal 2.12.1: a 12 m cold aura).
	const APlayerState* PS = C->GetPlayerState();
	if (PS)
	{
		for (TActorIterator<AWRCreature> It(C->GetWorld()); It; ++It)
		{
			if (It->CreatureId == TEXT("glowgrub") && It->IsTamed() && It->IsAlive() && It->GetTameOwner() == PS->GetPlayerName() && FVector::Dist(It->GetActorLocation(), C->GetActorLocation()) < 1500.f) return true;
		}
	}
	return false;
}

bool WRFungal::SpellFails(const AActor* Who, const FGameplayTag& Element)
{
	const AWRCharacterBase* C = Cast<AWRCharacterBase>(Who);
	return C && Element == WRTags::Element_Fire && C->HasTag(WRFungalTags::FoulAir);
}

float WRFungal::MoveScale(const AActor* Who)
{
	const AWRCharacterBase* C = Cast<AWRCharacterBase>(Who);
	if (!C) return 1.f;
	// Foul air: heavy breathing, no sprint (the movement's sprint needs the multiplier at 1).
	return C->HasTag(WRFungalTags::FoulAir) ? WRTune(TEXT("hazards.fungal.foulAir.move"), 0.97f) : 1.f;
}

float WRFungal::SporeTaken(const AWRCharacterBase* Who)
{
	if (!Who) return 0.f;
	// Every Fungal creature ignores spore clouds (fungal 2.12 rule).
	if (const AWRCreature* Cr = Cast<AWRCreature>(Who))
	{
		const FWRCreatureDef* Def = Cr->GetDef();
		if (Def && Def->Biome == WRFungal::Biome()) return 0.f;
		return 1.f;
	}
	// The Immune system (the Spore path's root, main path only: immune to poison and rot).
	if (const UWRPathComponent* P = UWRPathComponent::FindFor(Who)) if (P->GetMainPath() == TEXT("spore")) return 0.f;
	// A filter mask, Sporeweave's four pieces (passSpores).
	if (UWRInventoryComponent::GearStatFor(Who, TEXT("passSpores")) > 0.f) return 0.f;
	float M = 1.f;
	if (WRStorm::HasBuff(Who, TEXT("spore_wad"))) M *= WRTune(TEXT("hazards.fungal.cloud.wadMult"), 0.3f);
	return M;
}

void WRFungal::Wind(AActor* By, const FVector& Origin, const FVector& Dir, float Range, float Cone, bool bStrong)
{
	using namespace WRFungalHazImpl;
	if (!By || !By->HasAuthority() || !WRFungal::Live()) return;
	const FVector D = Dir.GetSafeNormal();
	const float CosHalf = FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(Cone, 10.f, 180.f) * 0.5f));
	const auto In = [&](const FVector& P, float Grow) { const FVector To = P - Origin; const float Dist = To.Size(); return Dist < Range + Grow && (Dist < Grow || FVector::DotProduct(To / FMath::Max(1.f, Dist), D) >= CosHalf); };
	const float Push = bStrong ? FhT(TEXT("hazards.fungal.cloud.strongPush"), 1500.f) : FhT(TEXT("hazards.fungal.cloud.windPush"), 600.f);
	for (const TWeakObjectPtr<AWRSporeCloud>& W : Clouds())
	{
		AWRSporeCloud* C = W.Get();
		if (C && C->IsLive() && C->GetWorld() == By->GetWorld() && In(C->Centre(), C->RadiusNow())) C->Push((C->Centre() - Origin).GetSafeNormal2D() * 0.5f + D * 0.5f, Push, By);
	}
	const float Tear = FhT(TEXT("hazards.fungal.lines.windTear"), 800.f);
	for (const TWeakObjectPtr<AWRLureLines>& W : LineSets())
	{
		if (AWRLureLines* L = W.Get())
		{
			// Every line within the gust's reach of its axis (8 m wide) comes down.
			for (float S = 0.f; S <= Range; S += Tear * 0.6f) L->CutNear(Origin + D * S, Tear * 0.6f, TEXT("wind"));
		}
	}
	for (const TWeakObjectPtr<AWRFungalSpot>& W : Spots())
	{
		AWRFungalSpot* S = W.Get();
		if (S && In(S->GetActorLocation(), 400.f)) S->Wind(By);
	}
}

void WRFungal::FireAt(AActor* By, const FVector& At, float Radius)
{
	using namespace WRFungalHazImpl;
	if (!By || !By->HasAuthority() || !WRFungal::Live()) return;
	if (AWRSporeCloud* C = AWRSporeCloud::At(By, At, 100.f)) C->Ignite(By);
	for (const TWeakObjectPtr<AWRLureLines>& W : LineSets()) if (AWRLureLines* L = W.Get()) L->CutNear(At, FMath::Max(Radius, FhT(TEXT("hazards.fungal.lines.burnRadius"), 300.f)), TEXT("fire"));
	for (const TWeakObjectPtr<AWRFungalSpot>& W : Spots())
	{
		AWRFungalSpot* S = W.Get();
		if (S && FVector::Dist(S->GetActorLocation(), At) < FMath::Max(Radius, 400.f) + S->Width * 0.5f) S->Fire(By);
	}
}

void WRFungal::FrostAt(AActor* By, const FVector& At)
{
	if (!By || !By->HasAuthority() || !WRFungal::Live()) return;
	if (AWRSporeCloud* C = AWRSporeCloud::At(By, At, 100.f)) C->Settle(By);
}

void WRFungal::OnHit(AWRCharacterBase* Target, const FWRHit& Hit)
{
	if (!Target || !Target->HasAuthority() || !WRFungal::Live()) return;
	const FVector At = Target->GetActorLocation();
	AActor* By = Hit.Instigator ? Hit.Instigator.Get() : Target;
	if (!Hit.bReaction)
	{
		if (Hit.Element == WRTags::Element_Fire || Hit.BurnBuildup > 0.f) FireAt(By, At, 250.f);
		else if (Hit.Element == WRTags::Element_Frost || Hit.FrostBuildup > 0.f) FrostAt(By, At);
	}
	// A fight is loud work: the Webs' ceilings listen.
	if (!Hit.bDamageOverTime) Noise(Target, At, By);
}

void WRFungal::Swing(AWRCharacterBase* By, const FVector& Start, const FVector& End, float R)
{
	using namespace WRFungalHazImpl;
	if (!By || !By->HasAuthority() || !WRFungal::Live()) return;
	const float Cut = FhT(TEXT("hazards.fungal.lines.cutRadius"), 150.f);
	for (const TWeakObjectPtr<AWRLureLines>& W : LineSets())
	{
		if (AWRLureLines* L = W.Get())
		{
			L->CutNear(Start, Cut, TEXT("blade"));
			L->CutNear((Start + End) * 0.5f, Cut, TEXT("blade"));
			L->CutNear(End, Cut, TEXT("blade"));
		}
	}
	for (const TWeakObjectPtr<AWRSpringcap>& W : Caps())
	{
		AWRSpringcap* S = W.Get();
		if (!S) continue;
		const FVector Top = S->GetActorLocation() + FVector(0.f, 0.f, 120.f);
		if (FMath::PointDistToSegment(Top, Start, End) < R + 140.f) S->Struck(By);
	}
	// A blade at a web sheet cuts it slowly (the fire is quicker: Fire); a torch in the off hand burns it.
	if (CarriesFire(By)) FireAt(By, End, 200.f);
}

void WRFungal::Noise(const UObject* WorldContext, const FVector& At, AActor* By)
{
	using namespace WRFungalHazImpl;
	const float R = FhT(TEXT("hazards.fungal.capFall.noiseRadius"), 1500.f);
	for (const TWeakObjectPtr<AWRCapFall>& W : Falls())
	{
		AWRCapFall* F = W.Get();
		if (F && F->GetWorld() == WorldContext->GetWorld() && FVector::Dist2D(F->GetActorLocation(), At) < R && FMath::Abs(F->GetActorLocation().Z - At.Z) < 1200.f) F->Heard(At, By);
	}
}

// ================================================================================================ the state

AWRFungalState::AWRFungalState()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRFungalState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRFungalState, Sporefall);
	DOREPLIFETIME(AWRFungalState, SporefallStart);
	DOREPLIFETIME(AWRFungalState, FlueInvasion);
}

AWRFungalState* AWRFungalState::Get(const UObject* WorldContext)
{
	static TWeakObjectPtr<AWRFungalState> Cached;
	const UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (Cached.IsValid() && Cached->GetWorld() == W) return Cached.Get();
	if (!W) return nullptr;
	for (TActorIterator<AWRFungalState> It(const_cast<UWorld*>(W)); It; ++It) { Cached = *It; return *It; }
	return nullptr;
}

// ================================================================================================ spore clouds

AWRSporeCloud::AWRSporeCloud()
{
	bReplicates = true;
	SetReplicateMovement(false);
	SetNetCullDistanceSquared(FMath::Square(30000.f));
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRSporeCloud::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRSporeCloud, Kind);
	DOREPLIFETIME(AWRSporeCloud, Radius);
	DOREPLIFETIME(AWRSporeCloud, Height);
	DOREPLIFETIME(AWRSporeCloud, State);
	DOREPLIFETIME(AWRSporeCloud, StateAt);
	DOREPLIFETIME(AWRSporeCloud, Home);
	DOREPLIFETIME(AWRSporeCloud, Offset);
	DOREPLIFETIME(AWRSporeCloud, OffsetFrom);
	DOREPLIFETIME(AWRSporeCloud, OffsetAt);
	DOREPLIFETIME(AWRSporeCloud, Maker);
	DOREPLIFETIME(AWRSporeCloud, Scale);
}

AWRSporeCloud* AWRSporeCloud::Spawn(UWorld* World, const FVector& At, float InRadius, FName InKind, float Lifetime, AActor* Owner, float InHeight)
{
	if (!World || World->GetNetMode() == NM_Client) return nullptr;
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FTransform Tr(FRotator::ZeroRotator, At);
	AWRSporeCloud* C = World->SpawnActorDeferred<AWRSporeCloud>(AWRSporeCloud::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!C) return nullptr;
	C->Kind = InKind;
	C->Radius = InRadius;
	C->Height = InHeight;
	C->Home = At;
	C->Maker = Owner;
	C->StateAt = WRFungal::Now(World);
	C->DieAt = Lifetime > 0.f ? WRFungal::Now(World) + Lifetime : 0.f;
	C->FinishSpawning(Tr);
	return C;
}

const TArray<TWeakObjectPtr<AWRSporeCloud>>& AWRSporeCloud::All()
{
	return WRFungalHazImpl::Clouds();
}

AWRSporeCloud* AWRSporeCloud::At(const UObject* WorldContext, const FVector& Pos, float Grow)
{
	const UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	for (const TWeakObjectPtr<AWRSporeCloud>& C : WRFungalHazImpl::Clouds())
	{
		if (C.IsValid() && C->GetWorld() == W && C->IsLive() && C->Contains(Pos, Grow)) return C.Get();
	}
	return nullptr;
}

void AWRSporeCloud::BeginPlay()
{
	Super::BeginPlay();
	WRFungalHazImpl::Clouds().AddUnique(this);
	if (Home.IsZero()) Home = GetActorLocation();
	UpdateLook();
}

void AWRSporeCloud::EndPlay(const EEndPlayReason::Type Reason)
{
	WRFungalHazImpl::Clouds().Remove(this);
	Super::EndPlay(Reason);
}

FVector AWRSporeCloud::Centre() const
{
	const float T = WRFungalHazImpl::FhSmooth((WRFungal::Now(this) - OffsetAt) / FMath::Max(0.1f, WRTune(TEXT("hazards.fungal.cloud.pushSeconds"), 0.8f)));
	return Home + FMath::Lerp(OffsetFrom, Offset, T);
}

bool AWRSporeCloud::Contains(const FVector& P, float Grow) const
{
	const FVector C = Centre();
	const float R = RadiusNow() + Grow;
	if (Height > 0.f)
	{
		// A column (a vent's plume): a cylinder from its foot up.
		return FVector::DistSquared2D(P, C) <= R * R && P.Z >= C.Z - 200.f && P.Z <= C.Z + Height;
	}
	// A pocket: a flattened sphere (its height 0.6 of its width), lying on the ground it formed over.
	const FVector D = P - C;
	return FMath::Square(D.X / R) + FMath::Square(D.Y / R) + FMath::Square(D.Z / (R * 0.6f + 150.f)) <= 1.f;
}

bool AWRSporeCloud::Spares(const AWRCharacterBase* Who) const
{
	if (!Who) return true;
	const AWRCharacterBase* M = Cast<AWRCharacterBase>(Maker);
	if (!M) return false;
	if (M == Who) return true;
	// A Spore walker's clouds spare their party (their tames too); a boss's spare nothing of its own kind but itself.
	return Kind == TEXT("path") && !M->IsHostileTo(Who);
}

void AWRSporeCloud::Push(const FVector& Dir, float Dist, AActor* By)
{
	if (!HasAuthority() || !IsLive()) return;
	const FVector Now = Centre() - Home;
	FVector To = Now + Dir.GetSafeNormal2D() * Dist;
	// A place's cloud never leaves its hall: it drifts back over 2 minutes after; a vent's column stays on its vent.
	if (Kind == TEXT("vent")) return;
	const float Max = Kind == TEXT("world") ? WRTune(TEXT("hazards.fungal.cloud.maxDrift"), 2500.f) : 1.0e6f;
	if (To.Size2D() > Max) To = To.GetSafeNormal2D() * Max;
	OffsetFrom = Now;
	Offset = FVector(To.X, To.Y, 0.f);
	OffsetAt = WRFungal::Now(this);
	LastWindAt = OffsetAt;
	LastWindBy = By;
	WRFx::Broadcast(this, TEXT("fungal.cloudPush"), Centre(), Dir.Rotation(), RadiusNow() / 800.f, TEXT("fungal.cloud_push"));
}

void AWRSporeCloud::Ignite(AActor* By)
{
	if (!HasAuthority() || State != 0) return;
	State = 1;
	StateAt = WRFungal::Now(this);
	IgnitedBy = By;
	UpdateLook();
	WRSound::PlayAt(this, TEXT("fungal.flash_tell"), Centre());
}

void AWRSporeCloud::Settle(AActor* By)
{
	if (!HasAuthority() || !IsLive()) return;
	State = 3;
	StateAt = WRFungal::Now(this);
	UpdateLook();
	WRFx::Broadcast(this, TEXT("fungal.sporeSnow"), Centre(), FRotator::ZeroRotator, RadiusNow() / 800.f, TEXT("fungal.spore_snow"));
}

void AWRSporeCloud::Flash()
{
	using namespace WRFungalHazImpl;
	// Like flour dust in a mill (fungal 2.8): the flash through the whole cloud, 40 + burn 40 to everyone inside, the
	// lines and web in it burn, and the cloud is gone 2 game hours.
	const FVector C = Centre();
	WRFx::Broadcast(this, TEXT("fungal.cloudFlash"), C, FRotator::ZeroRotator, RadiusNow() / 800.f, TEXT("fungal.flash"));
	AActor* By = IgnitedBy.IsValid() ? IgnitedBy.Get() : this;
	TArray<AWRCharacterBase*> In;
	ForCharsNear(GetWorld(), C, RadiusNow() + 400.f, [&](AWRCharacterBase* Who) { if (Contains(Who->GetActorLocation(), Who->GetSimpleCollisionRadius())) In.Add(Who); });
	for (AWRCharacterBase* Who : In) FhHit(Who, By, FhT(TEXT("hazards.fungal.cloud.flashDamage"), 40.f), WRTags::Element_Fire, FhT(TEXT("hazards.fungal.cloud.flashBurn"), 40.f), 2, C);
	for (const TWeakObjectPtr<AWRLureLines>& W : LineSets()) if (AWRLureLines* L = W.Get()) L->CutNear(C, RadiusNow(), TEXT("fire"));
	for (const TWeakObjectPtr<AWRFungalSpot>& W : Spots()) if (AWRFungalSpot* S = W.Get()) if (Contains(S->GetActorLocation(), S->Width)) S->Fire(By);
	State = 2;
	StateAt = WRFungal::Now(this);
	UpdateLook();
}

void AWRSporeCloud::OnRep_State()
{
	UpdateLook();
}

void AWRSporeCloud::UpdateLook()
{
	if (GetNetMode() == NM_DedicatedServer) return;
	const float R = RadiusNow();
	if (LookState == State && FMath::IsNearlyEqual(LookRadius, R, 20.f)) return;
	LookState = State;
	LookRadius = R;
	for (USceneComponent* C : Fx) if (C) C->DestroyComponent();
	Fx.Reset();
	if (State == 2) return;
	// The haze: slow-turning cyan-green with bright motes and a soft edge (fx rows fungal.cloud / fungal.cloudEdge); a
	// column is a stack of them up its height; the tell a rising glow.
	const int32 Stack = Height > 0.f ? FMath::Clamp(int32(Height / 1500.f), 1, 12) : 1;
	for (int32 i = 0; i < Stack; ++i)
	{
		const FName Row = State == 1 ? FName(TEXT("fungal.flashTell")) : State == 3 ? FName(TEXT("fungal.sporeSnowFall")) : (Kind == TEXT("drift") ? FName(TEXT("fungal.cloudThin")) : FName(TEXT("fungal.cloud")));
		if (USceneComponent* F = WRFx::Attach(GetRootComponent(), Row, NAME_None, R / 800.f))
		{
			F->SetRelativeLocation(FVector(0.f, 0.f, Height > 0.f ? Height * (i + 0.5f) / Stack : 0.f));
			Fx.Add(F);
		}
	}
	if (State == 0) if (USceneComponent* E = WRFx::Attach(GetRootComponent(), TEXT("fungal.cloudEdge"), NAME_None, R / 800.f)) Fx.Add(E);
}

void AWRSporeCloud::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Every machine: the look follows the push (eased) and the scale.
	SetActorLocation(Centre());
	UpdateLook();
	if (HasAuthority()) ServerTick(DeltaSeconds);
}

void AWRSporeCloud::ServerTick(float Dt)
{
	using namespace WRFungalHazImpl;
	const float Now = WRFungal::Now(this);
	if (DieAt > 0.f && Now >= DieAt && State != 1) { Destroy(); return; }
	// The Spore bloom swells the caverns' own clouds half again (fungal 2.9); rain on the surface lays the clouds down.
	if (Kind == TEXT("world"))
	{
		const bool bUnder = WRFungal::InCave(Home + FVector(0.f, 0.f, 100.f));
		const float Want = bUnder && WRFungal::BloomOn(this) ? WRTune(TEXT("hazards.fungal.bloom.spread"), 1.5f) : 1.f;
		if (!FMath::IsNearlyEqual(Scale, Want)) Scale = FMath::FInterpTo(Scale, Want, Dt, 0.5f);
		if (!bUnder)
		{
			const FWRWeatherKind* K = UWRWeatherSubsystem::ActiveKind(this, WRFungal::Biome());
			const bool bRain = K && K->Id == TEXT("rain");
			if (bRain && State == 0) { State = 2; StateAt = Now; UpdateLook(); }
		}
		// Gone (flashed or settled): back after its hours; a pushed cloud drifts home after two minutes.
		if (State == 2)
		{
			const FWRWeatherKind* K = bUnder ? nullptr : UWRWeatherSubsystem::ActiveKind(this, WRFungal::Biome());
			const bool bRain = K && K->Id == TEXT("rain");
			if (Now - StateAt > FhHours(FhT(TEXT("hazards.fungal.cloud.flashGoneHours"), 2.f)) && !bRain)
			{
				State = 0; StateAt = Now; Offset = OffsetFrom = FVector::ZeroVector; OffsetAt = Now - 10.f; UpdateLook();
			}
			return;
		}
		if (State == 0 && !Offset.IsNearlyZero() && Now - OffsetAt > WRTune(TEXT("hazards.fungal.cloud.homeAfter"), 120.f))
		{
			OffsetFrom = Centre() - Home; Offset = FVector::ZeroVector; OffsetAt = Now;
		}
	}
	if (State == 3)
	{
		// The spore snow falls 3 s, then it's gone an hour (a place's) or for good.
		if (Now - StateAt > FhT(TEXT("hazards.fungal.cloud.settleSeconds"), 3.f))
		{
			if (Kind == TEXT("world")) { State = 2; StateAt = Now - FhHours(1.f); UpdateLook(); }
			else Destroy();
		}
		return;
	}
	if (State == 2) { if (Kind != TEXT("world")) Destroy(); return; }
	if (State == 1)
	{
		if (Now - StateAt >= FhT(TEXT("hazards.fungal.cloud.flashTell"), 0.9f)) Flash();
		return;
	}
	CheckClock -= Dt;
	if (CheckClock > 0.f) return;
	const float Step = 0.25f;
	CheckClock = Step;
	const FVector C = Centre();
	const float Grace = FhT(TEXT("hazards.fungal.cloud.grace"), 0.9f);
	const float Poison = FhT(TEXT("hazards.fungal.cloud.poisonPerSecond"), 12.f), RotAfter = FhT(TEXT("hazards.fungal.cloud.rotAfter"), 10.f), Rot = FhT(TEXT("hazards.fungal.cloud.rotPerSecond"), 12.f);
	const float Break = FhT(TEXT("hazards.fungal.cloud.breakSeconds"), 3.f);
	TSet<AWRCharacterBase*> Seen;
	AWRCharacterBase* FireIn = nullptr;
	ForCharsNear(GetWorld(), C, RadiusNow() + 300.f + Height, [&](AWRCharacterBase* Who)
	{
		if (!Contains(Who->GetActorLocation(), Who->GetSimpleCollisionRadius() * 0.5f)) return;
		Seen.Add(Who);
		if (WRFungal::CarriesFire(Who)) FireIn = Who;
		if (Spares(Who)) return;
		const float Taken = WRFungal::SporeTaken(Who);
		FInside& I = Inside.FindOrAdd(Who);
		I.In += Step;
		I.Out = 0.f;
		if (I.In < Grace || Taken <= 0.f || !Who->Status) return;
		I.Continuous += Step;
		Who->Status->AddStatusBuildup(WRStatus::Poison, Poison * Step * Taken, Cast<AWRCharacterBase>(Maker));
		if (I.Continuous >= RotAfter) Who->Status->AddStatusBuildup(WRStatus::Rot, Rot * Step * Taken, Cast<AWRCharacterBase>(Maker));
	});
	for (auto It = Inside.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid()) { It.RemoveCurrent(); continue; }
		if (Seen.Contains(It->Key.Get())) continue;
		It->Value.Out += Step;
		It->Value.In = 0.f;
		if (It->Value.Out >= Break) It.RemoveCurrent();
	}
	// Fire carried in: a torch, a burning blade, a burning body. The flame takes the dust after a breath.
	if (FireIn)
	{
		FireHeld += Step;
		if (FireHeld >= FhT(TEXT("hazards.fungal.cloud.fireCatch"), 0.25f)) Ignite(FireIn);
	}
	else FireHeld = 0.f;
}

// ================================================================================================ lure lines

AWRLureLines::AWRLureLines()
{
	bReplicates = true;
	SetReplicateMovement(false);
	SetNetCullDistanceSquared(FMath::Square(25000.f));
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRLureLines::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRLureLines, SetIndex);
	DOREPLIFETIME(AWRLureLines, Cut);
}

const TArray<TWeakObjectPtr<AWRLureLines>>& AWRLureLines::All()
{
	return WRFungalHazImpl::LineSets();
}

void AWRLureLines::BeginPlay()
{
	Super::BeginPlay();
	WRFungalHazImpl::LineSets().AddUnique(this);
	MakeLines();
	BuildLook();
}

void AWRLureLines::OnRep_Set()
{
	MakeLines();
	BuildLook();
}

void AWRLureLines::OnRep_Cut()
{
	BuildLook();
}

void AWRLureLines::MakeLines()
{
	// The lines are the same on every machine: a seeded scatter over the set's ellipse, each hung from the ceiling the
	// caverns' air finds over it, its bottom 1.6-2.5 m over the floor (you can crouch under any of them).
	const FWRFungalData& D = FWRFungalData::Get();
	Lines.Reset();
	if (!D.Lines.IsValidIndex(SetIndex)) return;
	const FWRFungalData::FLineSet& S = D.Lines[SetIndex];
	FRandomStream R(8821 + SetIndex * 131);
	const float Area = PI * S.Rx * S.Ry / 10000.f;
	const int32 Want = FMath::Clamp(int32(Area * S.Density), 4, int32(WRTune(TEXT("hazards.fungal.lines.maxPerSet"), 700.f)));
	const FRotator Yaw(0.f, S.Yaw, 0.f);
	for (int32 i = 0; i < Want * 3 && Lines.Num() < Want; ++i)
	{
		const float U = R.FRandRange(-1.f, 1.f), V = R.FRandRange(-1.f, 1.f);
		if (U * U + V * V > 1.f) continue;
		const FVector Base = S.At + Yaw.RotateVector(FVector(U * S.Rx, V * S.Ry, 0.f));
		// The ceiling over it.
		float Lo = 200.f, Hi = -1.f;
		for (float T = 200.f; T < (S.Roof - S.Floor) + 3000.f; T += 80.f) { if (D.Air(Base + FVector(0.f, 0.f, T)) >= 0.f) { Hi = T; break; } Lo = T; }
		if (Hi < 0.f) continue;
		const float Top = Base.Z + Lo;
		const float Bottom = Base.Z + S.Low + R.FRandRange(0.f, 90.f);
		if (Top - Bottom < 100.f) continue;
		Lines.Add({ FVector(Base.X, Base.Y, Top), FVector(Base.X, Base.Y, Bottom) });
	}
	if (HasAuthority())
	{
		Cut.SetNumZeroed(Lines.Num());
		RegrowAt.SetNumZeroed(Lines.Num());
	}
	SetActorLocation(S.At);
}

void AWRLureLines::BuildLook()
{
	if (GetNetMode() == NM_DedicatedServer) return;
	// Each line a hair-thin glowing strand with beads of sticky mucus at intervals (the glowworm's snare): the glow mesh.
	FWRToolMesh M;
	const FLinearColor C = WRFungalLook::StarC();
	for (int32 i = 0; i < Lines.Num(); ++i)
	{
		if (Cut.IsValidIndex(i) && Cut[i]) continue;
		const FVector T = Lines[i].Top - GetActorLocation(), B = Lines[i].Bottom - GetActorLocation();
		const FColor Strand = FWRToolMesh::Pack(C, 0.55f), Bead = FWRToolMesh::Pack(WRFungalLook::CyanC(), 1.f);
		M.Tri(T + FVector(1.f, 0.f, 0.f), T - FVector(1.f, 0.f, 0.f), B, Strand, FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector);
		M.Tri(T + FVector(0.f, 1.f, 0.f), T - FVector(0.f, 1.f, 0.f), B, Strand, FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector);
		const int32 NB = FMath::Clamp(int32((T.Z - B.Z) / 25.f), 2, 30);
		for (int32 b = 0; b < NB; ++b)
		{
			// Beads crowd the bottom third (where the prey touches).
			const float F = FMath::Square(float(b + 1) / NB);
			const FVector P = FMath::Lerp(B, T, F * 0.7f);
			const float S = 1.6f + 1.4f * (1.f - F);
			M.Tri(P + FVector(0, 0, S), P + FVector(S, 0, 0), P - FVector(0, 0, S), Bead, FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector);
			M.Tri(P + FVector(0, 0, S), P + FVector(0, S, 0), P - FVector(0, 0, S), Bead, FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector);
		}
	}
	if (!Mesh)
	{
		Mesh = NewObject<UProceduralMeshComponent>(this);
		Mesh->SetupAttachment(GetRootComponent());
		Mesh->RegisterComponent();
	}
	M.ApplyTo(Mesh);
	Mesh->SetMaterial(0, WRFungalLook::GlowMaterial(this));
	Mesh->SetCastShadow(false);
	Mesh->SetCullDistance(WRTune(TEXT("hazards.fungal.lines.cull"), 9000.f));
}

void AWRLureLines::MulticastTwang_Implementation(int32 Line)
{
	if (GetNetMode() == NM_DedicatedServer || !Lines.IsValidIndex(Line)) return;
	// The twang: the line brightens white for 0.9 s with a high note (the tell before it grips).
	TwangLine = Line;
	TwangUntil = GetWorld()->GetTimeSeconds() + WRTune(TEXT("hazards.fungal.lines.tell"), 0.9f);
	FWRToolMesh M;
	const FVector T = Lines[Line].Top - GetActorLocation(), B = Lines[Line].Bottom - GetActorLocation();
	const FColor W = FWRToolMesh::Pack(FLinearColor::White, 1.f);
	M.Tri(T + FVector(3.f, 0.f, 0.f), T - FVector(3.f, 0.f, 0.f), B, W, FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector);
	M.Tri(T + FVector(0.f, 3.f, 0.f), T - FVector(0.f, 3.f, 0.f), B, W, FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector);
	if (!TwangMesh)
	{
		TwangMesh = NewObject<UProceduralMeshComponent>(this);
		TwangMesh->SetupAttachment(GetRootComponent());
		TwangMesh->RegisterComponent();
	}
	M.ApplyTo(TwangMesh);
	TwangMesh->SetMaterial(0, WRFungalLook::GlowMaterial(this));
	TwangMesh->SetVisibility(true);
	WRSound::PlayAt(this, TEXT("fungal.line_twang"), Lines[Line].Bottom);
}

int32 AWRLureLines::CutNear(const FVector& At, float R, FName Why)
{
	if (!HasAuthority() || Lines.Num() == 0) return 0;
	const FWRFungalData& D = FWRFungalData::Get();
	if (!D.Lines.IsValidIndex(SetIndex)) return 0;
	// Cheap reject: the set's own reach.
	const FWRFungalData::FLineSet& S = D.Lines[SetIndex];
	if (FVector::Dist2D(At, S.At) > FMath::Max(S.Rx, S.Ry) + R + 200.f) return 0;
	const float Regrow = WRFungalHazImpl::FhHours(WRTune(TEXT("hazards.fungal.lines.regrowHours"), 1.f));
	const float Now = WRFungal::Now(this);
	int32 N = 0;
	for (int32 i = 0; i < Lines.Num(); ++i)
	{
		if (Cut[i]) continue;
		if (FVector::Dist2D(At, Lines[i].Bottom) > R) continue;
		if (At.Z < Lines[i].Bottom.Z - R - 200.f || At.Z > Lines[i].Top.Z + R) continue;
		Cut[i] = 1;
		RegrowAt[i] = Now + Regrow;
		++N;
	}
	if (N > 0)
	{
		OnRep_Cut();
		if (Why == TEXT("fire")) WRFx::Broadcast(this, TEXT("fungal.linesBurn"), At, FRotator::ZeroRotator, R / 300.f, TEXT("fungal.lines_burn"));
		else if (Why == TEXT("wind")) WRFx::Broadcast(this, TEXT("fungal.linesTear"), At, FRotator::ZeroRotator, R / 600.f, TEXT("fungal.lines_tear"));
		else WRSound::PlayAt(this, TEXT("fungal.line_cut"), At);
		// A survivor on a cut line drops free.
		for (auto& KV : Grips) if (KV.Value.Line >= 0 && Cut.IsValidIndex(KV.Value.Line) && Cut[KV.Value.Line]) KV.Value.Line = -2;
	}
	return N;
}

void AWRLureLines::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (TwangMesh && TwangMesh->IsVisible() && GetWorld()->GetTimeSeconds() > TwangUntil) TwangMesh->SetVisibility(false);
	if (HasAuthority()) ServerTick(DeltaSeconds);
}

void AWRLureLines::ServerTick(float Dt)
{
	const float Now = WRFungal::Now(this);
	// Regrowth.
	bool bGrew = false;
	for (int32 i = 0; i < Cut.Num(); ++i) if (Cut[i] && Now >= RegrowAt[i]) { Cut[i] = 0; bGrew = true; }
	if (bGrew) OnRep_Cut();
	CheckClock -= Dt;
	if (CheckClock > 0.f) return;
	const float Step = 0.1f;
	CheckClock = Step;
	const FWRFungalData& D = FWRFungalData::Get();
	if (!D.Lines.IsValidIndex(SetIndex)) return;
	const FWRFungalData::FLineSet& S = D.Lines[SetIndex];
	const float Tell = WRTune(TEXT("hazards.fungal.lines.tell"), 0.9f), Stick = WRTune(TEXT("hazards.fungal.lines.stick"), 2.f), Reel = WRTune(TEXT("hazards.fungal.lines.reelSeconds"), 1.5f);
	const float Reach = WRTune(TEXT("hazards.fungal.lines.touch"), 45.f);
	TSet<AWRCharacterBase*> Near;
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		AWRCharacterBase* Who = *It;
		if (!Who->IsAlive() || FVector::Dist2D(Who->GetActorLocation(), S.At) > FMath::Max(S.Rx, S.Ry) + 300.f) continue;
		// Survivors and every creature that isn't the lines' own (a Webspinner walks its lines; fungal creatures know them).
		if (const AWRCreature* Cr = Cast<AWRCreature>(Who)) { const FWRCreatureDef* Def = Cr->GetDef(); if (!Def || Def->Biome == WRFungal::Biome()) continue; }
		Near.Add(Who);
		FGrip& G = Grips.FindOrAdd(Who);
		const ACharacter* Ch = Who;
		const float HalfH = Who->GetSimpleCollisionHalfHeight();
		const float HeadTop = Who->GetActorLocation().Z + HalfH * (Ch->bIsCrouched ? 0.55f : 1.f) + 20.f;
		if (G.Line == -1)
		{
			// A brush: the nearest standing line whose bottom is under the head's top, within a hand of the body.
			int32 Best = -1;
			float BestD = Reach + Who->GetSimpleCollisionRadius();
			for (int32 i = 0; i < Lines.Num(); ++i)
			{
				if (Cut[i] || Lines[i].Bottom.Z > HeadTop || Lines[i].Bottom.Z < Who->GetActorLocation().Z - HalfH) continue;
				const float Dd = FVector::Dist2D(Lines[i].Bottom, Who->GetActorLocation());
				if (Dd < BestD) { BestD = Dd; Best = i; }
			}
			if (Best >= 0)
			{
				G.Line = Best;
				G.Touch = 0.f;
				G.Stuck = 0.f;
				G.bReeling = false;
				MulticastTwang(Best);
				LastTwangAt = Lines[Best].Bottom;
				LastTwangTime = Now;
			}
			continue;
		}
		if (G.Line == -2)
		{
			// Cut free (or let go): nothing grips again until you've stepped away from the lines a moment.
			G.Touch += Step;
			if (G.Touch > 1.f) G = FGrip();
			continue;
		}
		G.Touch += Step;
		if (G.Touch < Tell) continue;
		if (G.Stuck <= 0.f && Who->Status)
		{
			// It grips: no moving or rolling 2 s (you can still swing and block: a light swing cuts it), the Weaver's silk
			// talisman halves it.
			const float Hold = Stick * FMath::Max(0.f, 1.f + UWRInventoryComponent::GearStatFor(Who, TEXT("snareTime")));
			Who->Status->ApplyStatus(WRTags::Status_Rooted, Hold);
			WRSound::PlayAt(this, TEXT("fungal.line_grip"), Who->GetActorLocation());
			if (APlayerController* PC = Cast<APlayerController>(Who->GetController())) if (AWRPlayerController* WPC = Cast<AWRPlayerController>(PC)) WPC->ClientNotify(TEXT("A lure line has you: cut it (light attack)"));
		}
		G.Stuck += Step;
		if (G.Stuck >= Stick && !G.bReeling)
		{
			// The reel: up the line 3 m over 1.5 s, then it lets go (the fall is yours).
			G.bReeling = true;
			Who->LaunchCharacter(FVector(0.f, 0.f, WRTune(TEXT("hazards.fungal.lines.reelUp"), 300.f) / Reel + 490.f), true, true);
			WRSound::PlayAt(this, TEXT("fungal.line_reel"), Who->GetActorLocation());
			// Ilmari's Cold light walk is spoiled for this crossing.
			if (AWRPlayerState* PS = Who->GetPlayerState<AWRPlayerState>()) if (UWRFungalWorld* FW = UWRFungalWorld::Get(this)) FW->GalleryWalkSpoiled(PS->GetPlayerName());
		}
		if (G.Stuck >= Stick + Reel) G.Line = -2, G.Touch = 0.f;
	}
	for (auto It = Grips.CreateIterator(); It; ++It) if (!It->Key.IsValid() || !Near.Contains(It->Key.Get())) It.RemoveCurrent();
}

// ================================================================================================ springcaps

AWRSpringcap::AWRSpringcap()
{
	bReplicates = true;
	SetReplicateMovement(false);
	SetNetCullDistanceSquared(FMath::Square(20000.f));
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRSpringcap::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRSpringcap, FiredAt);
	DOREPLIFETIME(AWRSpringcap, LandedAt);
}

const TArray<TWeakObjectPtr<AWRSpringcap>>& AWRSpringcap::All()
{
	return WRFungalHazImpl::Caps();
}

bool AWRSpringcap::IsCharged(float Now) const
{
	return Now - FiredAt >= WRTune(TEXT("hazards.fungal.springcap.regrow"), 20.f);
}

void AWRSpringcap::BeginPlay()
{
	Super::BeginPlay();
	WRFungalHazImpl::Caps().AddUnique(this);
	BuildLook();
}

void AWRSpringcap::BuildLook()
{
	// A bulging cap 2.4 m across on a short thick stipe, its skin glossy and taut; you stand on its crown (a box of
	// collision that rides the cap as it compresses). The cap is its own mesh so it can squash and spring back.
	using namespace WRFungalLook;
	UBoxComponent* Top = NewObject<UBoxComponent>(this);
	Top->SetupAttachment(GetRootComponent());
	Top->SetBoxExtent(FVector(95.f, 95.f, 12.f));
	Top->SetRelativeLocation(FVector(0.f, 0.f, 108.f));
	Top->SetCollisionProfileName(TEXT("BlockAll"));
	Top->RegisterComponent();
	UBoxComponent* Stem = NewObject<UBoxComponent>(this);
	Stem->SetupAttachment(GetRootComponent());
	Stem->SetBoxExtent(FVector(40.f, 40.f, 50.f));
	Stem->SetRelativeLocation(FVector(0.f, 0.f, 50.f));
	Stem->SetCollisionProfileName(TEXT("BlockAll"));
	Stem->RegisterComponent();
	if (GetNetMode() == NM_DedicatedServer) return;
	FWRToolMesh S;
	S.Lathe(FVector(0.f, 0.f, -10.f), FVector::UpVector, 90.f, { FVector(0.f, 60.f, 60.f), FVector(0.4f, 42.f, 42.f), FVector(1.f, 46.f, 46.f) }, 14, StipeC(), 0.7f);
	UProceduralMeshComponent* StemMesh = NewObject<UProceduralMeshComponent>(this);
	StemMesh->SetupAttachment(GetRootComponent());
	StemMesh->RegisterComponent();
	S.ApplyTo(StemMesh);
	FWRToolMesh C;
	C.Lathe(FVector::ZeroVector, FVector::UpVector, 85.f, { FVector(0.f, 112.f, 112.f), FVector(0.25f, 122.f, 122.f), FVector(0.6f, 104.f, 104.f), FVector(0.88f, 62.f, 62.f), FVector(1.f, 2.f, 2.f) }, 22, Rgb(196, 184, 160) * 0.62f, 0.2f);
	CapMesh = NewObject<UProceduralMeshComponent>(this);
	CapMesh->SetupAttachment(GetRootComponent());
	CapMesh->SetRelativeLocation(FVector(0.f, 0.f, 80.f));
	CapMesh->RegisterComponent();
	C.ApplyTo(CapMesh);
	// The gills' glow under it (it brightens as it charges).
	FWRToolMesh G;
	G.Lathe(FVector(0.f, 0.f, 76.f), FVector::UpVector, 8.f, { FVector(0.f, 48.f, 48.f), FVector(1.f, 110.f, 110.f) }, 18, CyanC(), 0.6f);
	UProceduralMeshComponent* GM = NewObject<UProceduralMeshComponent>(this);
	GM->SetupAttachment(GetRootComponent());
	GM->RegisterComponent();
	G.ApplyTo(GM);
	GM->SetMaterial(0, GlowMaterial(this));
	GM->SetCastShadow(false);
}

void AWRSpringcap::Struck(AActor* By)
{
	if (!HasAuthority()) return;
	const float Now = WRFungal::Now(this);
	if (!IsCharged(Now)) return;
	// A blow on it fires it: whoever stands on it is thrown 5 m sideways (fall damage applies).
	Fire(Rider.Get(), false, true);
}

void AWRSpringcap::Fire(AWRCharacterBase* Who, bool bJumped, bool bSideways)
{
	const float Now = WRFungal::Now(this);
	FiredAt = Now;
	LandedAt = -100.f;
	WRFx::Broadcast(this, TEXT("fungal.springcapPop"), GetActorLocation() + FVector(0.f, 0.f, 140.f), FRotator::ZeroRotator, 1.f, TEXT("fungal.springcap_pop"));
	if (!Who || !Who->IsAlive()) { Rider = nullptr; return; }
	const float G = FMath::Abs(Who->GetCharacterMovement() ? Who->GetCharacterMovement()->GetGravityZ() : -980.f);
	FVector V;
	if (bSideways)
	{
		// Thrown 5 m off it, a little up.
		FVector Away = (Who->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		if (Away.IsNearlyZero()) Away = Who->GetActorForwardVector();
		V = Away * WRTune(TEXT("hazards.fungal.springcap.throwSideways"), 500.f) * 1.8f + FVector(0.f, 0.f, 450.f);
	}
	else
	{
		// The launch: 9 m up when you jump in its window (a little pop when you don't), aimable 20 degrees by your stick.
		const float Up = bJumped ? WRTune(TEXT("hazards.fungal.springcap.launch"), 900.f) : WRTune(TEXT("hazards.fungal.springcap.pop"), 300.f);
		const float Vz = FMath::Sqrt(2.f * G * Up);
		FVector Aim = Who->GetLastMovementInputVector().GetSafeNormal2D();
		if (Aim.IsNearlyZero()) Aim = Who->GetVelocity().GetSafeNormal2D();
		V = FVector(0.f, 0.f, Vz) + Aim * Vz * FMath::Tan(FMath::DegreesToRadians(WRTune(TEXT("hazards.fungal.springcap.aimDeg"), 20.f)));
	}
	Who->LaunchCharacter(V, true, true);
	Rider = nullptr;
}

void AWRSpringcap::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Now = WRFungal::Now(this);
	if (CapMesh)
	{
		// The cap's squash: it compresses 30% while someone stands in its creak, springs back over 0.4 s, and sits deflated
		// and dull while it recharges.
		float Z = 1.f;
		if (Now - LandedAt < WRTune(TEXT("hazards.fungal.springcap.popAfter"), 0.6f)) Z = 1.f - 0.3f * FMath::Clamp((Now - LandedAt) / 0.3f, 0.f, 1.f);
		else if (Now - FiredAt < 0.4f) Z = 0.7f + 0.5f * FMath::Sin(FMath::Clamp((Now - FiredAt) / 0.4f, 0.f, 1.f) * PI) * 0.6f + 0.3f * FMath::Clamp((Now - FiredAt) / 0.4f, 0.f, 1.f);
		else if (!IsCharged(Now)) Z = 0.75f + 0.25f * FMath::Clamp((Now - FiredAt) / WRTune(TEXT("hazards.fungal.springcap.regrow"), 20.f), 0.f, 1.f);
		CapMesh->SetRelativeScale3D(FVector(1.f + (1.f - Z) * 0.25f, 1.f + (1.f - Z) * 0.25f, Z));
	}
	if (!HasAuthority()) return;
	// Who stands on its crown (or is falling toward it: a drop of 6 m onto it fires it anyway).
	const FVector Crown = GetActorLocation() + FVector(0.f, 0.f, 120.f);
	AWRCharacterBase* On = nullptr;
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		AWRCharacterBase* Who = *It;
		if (!Who->IsAlive() || FVector::DistSquared2D(Who->GetActorLocation(), Crown) > FMath::Square(1200.f)) continue;
		UCharacterMovementComponent* M = Who->GetCharacterMovement();
		const float Feet = Who->GetActorLocation().Z - Who->GetSimpleCollisionHalfHeight();
		if (M && M->IsFalling()) { float& Top = AirTop.FindOrAdd(Who, Feet); Top = FMath::Max(Top, Feet); }
		if (FVector::DistSquared2D(Who->GetActorLocation(), Crown) > FMath::Square(115.f)) continue;
		if (Feet < Crown.Z - 30.f || Feet > Crown.Z + 60.f) continue;
		On = Who;
	}
	if (!IsCharged(Now)) { Rider = nullptr; return; }
	if (On && On != Rider.Get())
	{
		const float* Top = AirTop.Find(On);
		const float Fell = Top ? *Top - (On->GetActorLocation().Z - On->GetSimpleCollisionHalfHeight()) : 0.f;
		AirTop.Remove(On);
		if (Fell > WRTune(TEXT("hazards.fungal.springcap.autoFromHeight"), 600.f)) { Fire(On, true, false); return; }
		Rider = On;
		RiderSince = Now;
		LandedAt = Now;
		WRSound::PlayAt(this, TEXT("fungal.springcap_creak"), Crown);
	}
	if (AWRCharacterBase* R = Rider.Get())
	{
		UCharacterMovementComponent* M = R->GetCharacterMovement();
		const bool bJumped = M && M->IsFalling() && R->GetVelocity().Z > 100.f;
		if (bJumped) Fire(R, true, false);
		else if (Now - RiderSince >= WRTune(TEXT("hazards.fungal.springcap.popAfter"), 0.6f)) Fire(R, false, false);
		else if (!On) Rider = nullptr;
	}
}

// ================================================================================================ cap-falls

AWRCapFall::AWRCapFall()
{
	bReplicates = true;
	SetReplicateMovement(false);
	SetNetCullDistanceSquared(FMath::Square(20000.f));
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRCapFall::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRCapFall, Roof);
	DOREPLIFETIME(AWRCapFall, KnockAt);
	DOREPLIFETIME(AWRCapFall, FallAt);
	DOREPLIFETIME(AWRCapFall, RegrowAt);
}

const TArray<TWeakObjectPtr<AWRCapFall>>& AWRCapFall::All()
{
	return WRFungalHazImpl::Falls();
}

void AWRCapFall::BeginPlay()
{
	Super::BeginPlay();
	WRFungalHazImpl::Falls().AddUnique(this);
	BuildLook();
}

void AWRCapFall::BuildLook()
{
	if (GetNetMode() == NM_DedicatedServer) return;
	// The slab: a rot-softened block of the ceiling 3 m across, its edges already cracked and furred with mycelium.
	using namespace WRFungalLook;
	FWRToolMesh M;
	M.Lathe(FVector(0.f, 0.f, -60.f), FVector::UpVector, 120.f, { FVector(0.f, 150.f, 130.f), FVector(0.5f, 160.f, 140.f), FVector(1.f, 140.f, 120.f) }, 9, Rgb(46, 40, 34) * 0.7f, 0.9f, 0.f, 5.f);
	M.Lathe(FVector(0.f, 0.f, -66.f), FVector::UpVector, 8.f, { FVector(0.f, 140.f, 120.f), FVector(1.f, 120.f, 100.f) }, 9, MycelC(), 0.95f);
	Slab = NewObject<UProceduralMeshComponent>(this);
	Slab->SetupAttachment(GetRootComponent());
	Slab->RegisterComponent();
	M.ApplyTo(Slab);
}

void AWRCapFall::Heard(const FVector& At, AActor* By)
{
	const float Now = WRFungal::Now(this);
	if (!HasAuthority() || !IsArmed(Now)) return;
	// The tell: two knocks, then three (the knockers), grit, then the dust shaft for its last second. A Knocker idol worn
	// within 20 m gives a second more.
	float Tell = WRTune(TEXT("hazards.fungal.capFall.tell"), 1.f) + WRTune(TEXT("hazards.fungal.capFall.knockLead"), 1.2f);
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		if (It->GetPlayerState() && FVector::Dist(It->GetActorLocation(), GetActorLocation()) < 2000.f && UWRInventoryComponent::GearStatFor(*It, TEXT("capFallWarn")) > 0.f) { Tell += WRTune(TEXT("hazards.fungal.capFall.idolExtra"), 1.f); break; }
	}
	KnockAt = Now;
	FallAt = Now + Tell;
	KnocksPlayed = 0;
	bLanded = false;
}

void AWRCapFall::MoleWarn()
{
	const float Now = WRFungal::Now(this);
	if (!HasAuthority() || !IsArmed(Now)) return;
	KnockAt = Now;
	FallAt = Now + WRTune(TEXT("hazards.fungal.capFall.moleWarn"), 10.f);
	KnocksPlayed = 0;
	bLanded = false;
	WRFx::Broadcast(this, TEXT("fungal.capFallMark"), GetActorLocation() + FVector(0.f, 0.f, 10.f), FRotator::ZeroRotator, 1.f, TEXT("fungal.mole_knock"));
}

void AWRCapFall::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Now = WRFungal::Now(this);
	const float FallSeconds = WRTune(TEXT("hazards.fungal.capFall.fallSeconds"), 0.45f);
	if (Slab)
	{
		// Up in the roof until it falls (a kinematic drop, gravity-like), then lying broken on the floor; hidden while it
		// regrows (the roof heals over).
		const float H = Roof - GetActorLocation().Z;
		if (FallAt > 0.f && Now >= FallAt)
		{
			const float T = FMath::Clamp((Now - FallAt) / FallSeconds, 0.f, 1.f);
			Slab->SetRelativeLocation(FVector(0.f, 0.f, FMath::Lerp(H, 40.f, T * T)));
			Slab->SetRelativeRotation(FRotator(T * 9.f, 0.f, T * 6.f));
			Slab->SetVisibility(true);
		}
		else if (Now < RegrowAt) Slab->SetVisibility(false);
		else { Slab->SetVisibility(true); Slab->SetRelativeLocation(FVector(0.f, 0.f, H)); Slab->SetRelativeRotation(FRotator::ZeroRotator); }
	}
	if (KnockAt < 0.f || FallAt < 0.f) return;
	// Every machine: the knocking and the dust shaft (from the replicated times).
	if (GetNetMode() != NM_DedicatedServer)
	{
		static const float Knocks[] = { 0.f, 0.25f, 0.7f, 0.92f, 1.14f };
		const float T = Now - KnockAt;
		while (KnocksPlayed < 5 && T >= Knocks[KnocksPlayed]) { WRSound::PlayAt(this, TEXT("fungal.cap_knock"), FVector(GetActorLocation().X, GetActorLocation().Y, Roof)); ++KnocksPlayed; }
		if (KnocksPlayed == 5 && Now >= FallAt - WRTune(TEXT("hazards.fungal.capFall.tell"), 1.f))
		{
			WRFx::SpawnAt(this, TEXT("fungal.dustShaft"), FVector(GetActorLocation().X, GetActorLocation().Y, Roof - 50.f), FRotator::ZeroRotator, (Roof - GetActorLocation().Z) / 1000.f);
			KnocksPlayed = 6;
		}
	}
	if (!HasAuthority()) return;
	if (Now >= FallAt + FallSeconds && !bLanded)
	{
		// It lands: 70 and a knockdown to everyone in the 3 m circle, then it lies there; the roof grows back in 2 hours.
		bLanded = true;
		const float R = WRTune(TEXT("hazards.fungal.capFall.radius"), 300.f);
		WRFungalHazImpl::ForCharsNear(GetWorld(), GetActorLocation(), R + 100.f, [&](AWRCharacterBase* Who)
		{
			if (FVector::Dist2D(Who->GetActorLocation(), GetActorLocation()) <= R + Who->GetSimpleCollisionRadius() * 0.5f)
				WRFungalHazImpl::FhHit(Who, this, WRTune(TEXT("hazards.fungal.capFall.damage"), 70.f), WRTags::Element_Physical, 0.f, 3, GetActorLocation() + FVector(0, 0, 400.f));
		});
		WRFx::Broadcast(this, TEXT("fungal.capFallLand"), GetActorLocation() + FVector(0.f, 0.f, 30.f), FRotator::ZeroRotator, 1.f, TEXT("fungal.cap_fall"));
		RegrowAt = Now + WRFungalHazImpl::FhHours(WRTune(TEXT("hazards.fungal.capFall.regrowHours"), 2.f));
		KnockAt = -100.f;
		FallAt = -100.f;
	}
}

// ================================================================================================ the Breath vents

AWRBreathVent::AWRBreathVent()
{
	bReplicates = false;
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

int32 AWRBreathVent::Phase(const UObject* WorldContext, float& OutPhase)
{
	// One of three every Every seconds, its tell first (the same rule as AWRSkyRing::PlumeNow, on the server's clock).
	const float Every = WRFungal::VentEvery(WorldContext);
	const float Now = WRFungal::Now(WorldContext);
	const float Tell = WRTune(TEXT("hazards.fungal.vent.tell"), 1.f), Live = WRTune(TEXT("hazards.fungal.vent.seconds"), 6.f);
	const float T = FMath::Fmod(Now, Every);
	if (T >= Every - Tell) { OutPhase = T - Every; return int32(FMath::Fmod(Now / Every + 1.f, 3.f)); }
	if (T < Live) { OutPhase = T; return int32(FMath::Fmod(Now / Every, 3.f)); }
	OutPhase = -100.f;
	return -1;
}

void AWRBreathVent::BeginPlay()
{
	Super::BeginPlay();
	BuildLook();
}

void AWRBreathVent::BuildLook()
{
	// A Deepfarmer bellows-stone: a carved stone collar 1.5 m high round a fissure, mycelium over it, and on its top a
	// bronze flap lid on a pivot pin through two lugs (it lifts on the exhale, drops on the inhale).
	using namespace WRFungalLook;
	UBoxComponent* B = NewObject<UBoxComponent>(this);
	B->SetupAttachment(GetRootComponent());
	B->SetBoxExtent(FVector(130.f, 130.f, 75.f));
	B->SetRelativeLocation(FVector(0.f, 0.f, 75.f));
	B->SetCollisionProfileName(TEXT("BlockAll"));
	B->RegisterComponent();
	if (GetNetMode() == NM_DedicatedServer) return;
	FWRToolMesh M;
	M.Lathe(FVector(0.f, 0.f, -20.f), FVector::UpVector, 170.f, { FVector(0.f, 160.f, 160.f), FVector(0.5f, 140.f, 140.f), FVector(0.85f, 130.f, 130.f), FVector(1.f, 118.f, 118.f) }, 16, StoneC(), 0.85f, 0.f, 4.f);
	M.Lathe(FVector(0.f, 0.f, 20.f), FVector::UpVector, 60.f, { FVector(0.f, 166.f, 166.f), FVector(1.f, 150.f, 150.f) }, 16, MycelC(), 0.95f);
	for (const float S : { -1.f, 1.f }) M.Lathe(FVector(-70.f, S * 60.f, 150.f), FVector::UpVector, 26.f, { FVector(0.f, 12.f, 12.f), FVector(1.f, 10.f, 10.f) }, 8, Rgb(140, 106, 58) * 0.6f, 0.45f);
	UProceduralMeshComponent* Collar = NewObject<UProceduralMeshComponent>(this);
	Collar->SetupAttachment(GetRootComponent());
	Collar->RegisterComponent();
	M.ApplyTo(Collar);
	// The flap on its pin (the pivot at the lugs; the plate swings up about it).
	Flap = NewObject<USceneComponent>(this);
	Flap->SetupAttachment(GetRootComponent());
	Flap->SetRelativeLocation(FVector(-70.f, 0.f, 168.f));
	Flap->RegisterComponent();
	FWRToolMesh P;
	P.Lathe(FVector(70.f, 0.f, -4.f), FVector::UpVector, 8.f, { FVector(0.f, 112.f, 112.f), FVector(1.f, 108.f, 108.f) }, 18, Rgb(140, 106, 58) * 0.6f, 0.4f);
	P.Lathe(FVector(0.f, -70.f, 0.f), FVector(0.f, 1.f, 0.f), 140.f, { FVector(0.f, 5.f, 5.f), FVector(1.f, 5.f, 5.f) }, 8, Rgb(90, 70, 40) * 0.6f, 0.5f);
	UProceduralMeshComponent* Plate = NewObject<UProceduralMeshComponent>(this);
	Plate->SetupAttachment(Flap);
	Plate->RegisterComponent();
	P.ApplyTo(Plate);
	Glow = NewObject<UPointLightComponent>(this);
	Glow->SetupAttachment(GetRootComponent());
	Glow->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	Glow->SetLightColor(CyanC());
	Glow->SetIntensity(400.f);
	Glow->SetAttenuationRadius(900.f);
	Glow->SetCastShadows(false);
	Glow->RegisterComponent();
}

void AWRBreathVent::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	float Ph = 0.f;
	const int32 Which = Phase(this, Ph);
	const bool bMine = Which == VentIndex;
	const float Tell = WRTune(TEXT("hazards.fungal.vent.tell"), 1.f), Live = WRTune(TEXT("hazards.fungal.vent.seconds"), 6.f);
	if (GetNetMode() != NM_DedicatedServer)
	{
		// The flap: up 35 degrees over the exhale's first half second, held, then down over 0.8 s; the stone brightens in
		// the tell (with a deep huff) and through the exhale.
		float Want = 0.f;
		if (bMine && Ph >= 0.f) Want = Ph < 0.5f ? 35.f * WRFungalHazImpl::FhSmooth(Ph / 0.5f) : Ph < Live - 0.8f ? 35.f : 35.f * (1.f - WRFungalHazImpl::FhSmooth((Ph - (Live - 0.8f)) / 0.8f));
		FlapAngle = Want;
		if (Flap) Flap->SetRelativeRotation(FRotator(FlapAngle, 0.f, 0.f));
		if (Glow) Glow->SetIntensity(bMine ? (Ph < 0.f ? 400.f + 3600.f * FMath::Clamp(1.f + Ph / Tell, 0.f, 1.f) : 4000.f) : 400.f);
		const int32 Cycle = int32(WRFungal::Now(this) / WRFungal::VentEvery(this));
		if (bMine && Ph < 0.f && LastHuff != Cycle) { LastHuff = Cycle; WRSound::PlayAt(this, TEXT("fungal.vent_huff"), GetActorLocation()); }
		if (bMine && Ph >= 0.f && !PlumeFx) PlumeFx = WRFx::Attach(GetRootComponent(), TEXT("fungal.ventPlume"), NAME_None, 1.f);
		if ((!bMine || Ph < 0.f) && PlumeFx) { PlumeFx->DestroyComponent(); PlumeFx = nullptr; }
	}
	if (!HasAuthority() && GetNetMode() == NM_Client) return;
	// The server: the plume is a cloud column up the shaft for its 6 s (a spore cloud for anyone in it).
	if (bMine && Ph >= 0.f && !Column.IsValid())
	{
		Column = AWRSporeCloud::Spawn(GetWorld(), GetActorLocation() + FVector(0.f, 0.f, 150.f), WRTune(TEXT("hazards.fungal.vent.width"), 600.f) * 0.5f, TEXT("vent"), Live - Ph, this, WRTune(TEXT("hazards.fungal.vent.height"), 8000.f));
	}
}
