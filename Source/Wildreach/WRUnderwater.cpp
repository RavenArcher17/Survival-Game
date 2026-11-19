#include "WRUnderwater.h"
#include "Wildreach.h"
#include "WRCharacter.h"
#include "WRCharacterBase.h"
#include "WRCombatLibrary.h"
#include "WRCombatTypes.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRInventory.h"
#include "WRLifeSkills.h"
#include "WRLightningPath.h"
#include "WRPathComponent.h"
#include "WRSound.h"
#include "WRTuning.h"
#include "WRVoid.h"
#include "WROceanTypes.h"
#include "WRSurvival.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/BrushComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "Components/CapsuleComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PhysicsVolume.h"
#include "Net/UnrealNetwork.h"

namespace
{
	FVector UwHead(const AActor* Who)
	{
		const ACharacter* C = Cast<ACharacter>(Who);
		if (!C) return Who ? Who->GetActorLocation() : FVector::ZeroVector;
		// The eyes: a little under the capsule's top (the body art's head sits about there in every stance).
		return C->GetActorLocation() + FVector(0.f, 0.f, C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - WRTune(TEXT("underwater.headBelowTop"), 18.f));
	}
}

// ---------------------------------------------------------------- the rules

APhysicsVolume* WRUnderwater::WaterAt(const UWorld* World, const FVector& Point)
{
	if (!World) return nullptr;
	// Every water and air volume overlaps pawns only (AWRWaterVolume, the tide's and the Pound's): ask the pawn channel.
	TArray<FOverlapResult> Hits;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(WRUnderwater), false);
	World->OverlapMultiByChannel(Hits, Point, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(4.f), Query);
	APhysicsVolume* Best = nullptr;
	for (const FOverlapResult& R : Hits)
	{
		APhysicsVolume* V = Cast<APhysicsVolume>(R.GetActor());
		if (V && (!Best || V->Priority > Best->Priority)) Best = V;
	}
	return Best && Best->bWaterVolume ? Best : nullptr;
}

float WRUnderwater::SurfaceZ(const APhysicsVolume* Water, const FVector& Point)
{
	if (!Water) return -1.0e9f;
	const UBrushComponent* B = Water->GetBrushComponent();
	if (!B) return Water->GetComponentsBoundingBox().Max.Z;
	// A body of boxes all to the same height (every water in the game is): the top of the box this point is in.
	if (const UBodySetup* Setup = B->BrushBodySetup)
	{
		const FTransform Xf = B->GetComponentTransform();
		for (const FKBoxElem& Box : Setup->AggGeom.BoxElems)
		{
			const FVector C = Xf.TransformPosition(Box.Center);
			const FVector H = FVector(Box.X, Box.Y, Box.Z) * 0.5 * Xf.GetScale3D();
			if (FMath::Abs(Point.X - C.X) <= H.X && FMath::Abs(Point.Y - C.Y) <= H.Y && Point.Z <= C.Z + H.Z && Point.Z >= C.Z - H.Z) return C.Z + H.Z;
		}
	}
	return B->Bounds.GetBox().Max.Z;
}

bool WRUnderwater::IsUnder(const AActor* Who)
{
	if (const UWRDiveComponent* D = UWRDiveComponent::Of(Who)) return D->IsUnder();
	return Who && WaterAt(Who->GetWorld(), UwHead(Who)) != nullptr;
}

bool WRUnderwater::IsSubmerged(const ACharacter* Who)
{
	const UCharacterMovementComponent* M = Who ? Who->GetCharacterMovement() : nullptr;
	return M && M->IsSwimming() && M->ImmersionDepth() >= 0.999f;
}

bool WRUnderwater::HasGills(const AActor* Who)
{
	const UWRPathComponent* P = UWRPathComponent::FindFor(Who);
	return P && P->GetMainPath() == TEXT("tide");
}

float WRUnderwater::MeleeRate(const AWRCharacterBase* Who, FName WeaponClass)
{
	if (!Who || !IsUnder(Who) || HasGills(Who)) return 1.f;
	const FWRTuning& T = FWRTuning::Get();
	if (Who->HasTag(FGameplayTag::RequestGameplayTag(TEXT("Status.Tide.FullSpeedUnder"), false))) return 1.f;
	// Hae-won's knife: it swings at full speed under the water in anyone's hand (the weapon's worn underwaterFullSpeed).
	if (UWRInventoryComponent::GearStatFor(Who, TEXT("underwaterFullSpeed")) > 0.f) return 1.f;
	TArray<FString> Thrust;
	T.Str(TEXT("underwater.thrustClasses"), TEXT("spear,dagger,harpoon,fists,throwing_spear")).ParseIntoArray(Thrust, TEXT(","));
	return Thrust.Contains(WeaponClass.ToString()) ? 1.f : T.Num(TEXT("underwater.meleePlayRate"), 0.8f);
}

bool WRUnderwater::CanChargeHeavy(const AWRCharacterBase* Who, FName WeaponClass)
{
	if (!Who || !IsUnder(Who) || HasGills(Who)) return true;
	TArray<FString> Heavy;
	FWRTuning::Get().Str(TEXT("underwater.noChargeClasses"), TEXT("greatsword,great_hammer,axe,greataxe")).ParseIntoArray(Heavy, TEXT(","));
	return !Heavy.Contains(WeaponClass.ToString());
}

float WRUnderwater::GuardStaminaMult(const AWRCharacterBase* Who)
{
	return Who && IsUnder(Who) && !HasGills(Who) ? WRTune(TEXT("underwater.guardStaminaMult"), 1.2f) : 1.f;
}

float WRUnderwater::FireMult(const AActor* Target)
{
	return Target && IsUnder(Target) ? WRTune(TEXT("underwater.fireMult"), 0.3f) : 1.f;
}

float WRUnderwater::BoltSpeedMult(const AActor* Caster, FName PathId)
{
	if (!Caster || PathId == TEXT("tide") || !IsUnder(Caster)) return 1.f;
	return WRTune(TEXT("underwater.boltSpeedMult"), 0.6f);
}

void WRUnderwater::SpreadLightning(AWRCharacterBase* Target, const FWRHit& Hit)
{
	if (!Target || !Target->HasAuthority() || Hit.bReaction || Hit.Element != WRTags::Element_Lightning) return;
	APhysicsVolume* Water = WaterAt(Target->GetWorld(), Target->GetActorLocation());
	if (!Water) return;
	const float Radius = WRTune(TEXT("underwater.lightningSpreadRadius"), 400.f);
	AActor* Caster = Hit.Instigator;
	TArray<AWRCharacterBase*> Struck;
	for (TActorIterator<AWRCharacterBase> It(Target->GetWorld()); It; ++It)
	{
		AWRCharacterBase* Who = *It;
		if (Who == Target || !Who->IsAlive() || FVector::DistSquared(Who->GetActorLocation(), Target->GetActorLocation()) > Radius * Radius) continue;
		if (WaterAt(Who->GetWorld(), Who->GetActorLocation()) != Water) continue;            // the same water only
		if (Who == Caster && WRLightning::IsGrounded(Who)) continue;                          // Grounded casters are spared
		Struck.Add(Who);
	}
	if (Struck.Num() == 0) return;
	WRFx::Broadcast(Target, TEXT("ocean.water_shock"), Target->GetActorLocation(), FRotator::ZeroRotator, Radius / 100.f, TEXT("ocean.water_shock"));
	for (AWRCharacterBase* Who : Struck)
	{
		FWRHit Spread;
		Spread.Instigator = Caster;
		Spread.Damage = Hit.Damage * WRTune(TEXT("underwater.lightningSpreadDamage"), 0.5f);
		Spread.Element = WRTags::Element_Lightning;
		Spread.Buildups.Add(TEXT("shock"), WRTune(TEXT("underwater.lightningSpreadShock"), 25.f));
		Spread.PathId = Hit.PathId;
		Spread.bReaction = true;      // a spread never spreads again
		Spread.bUnblockable = true;
		Spread.Impact = 1;
		Spread.Origin = Target->GetActorLocation();
		UWRCombatLibrary::ApplyHit(Who, Spread);
	}
}

void WRUnderwater::ApplyFog(UExponentialHeightFogComponent* Fog, UWorld* World, float Dt)
{
	static float Blend = 0.f;
	static float SavedFalloff = -1.f, SavedMax = -1.f, SavedStart = -1.f;
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	const bool bUnder = Me && IsUnder(Me);
	Blend = FMath::FInterpTo(Blend, bUnder ? 1.f : 0.f, Dt, 5.f);
	if (Blend < 0.002f)
	{
		if (SavedFalloff >= 0.f)
		{
			Fog->SetFogHeightFalloff(SavedFalloff);
			Fog->SetFogMaxOpacity(SavedMax);
			Fog->SetStartDistance(SavedStart);
			SavedFalloff = -1.f;
		}
		return;
	}
	if (SavedFalloff < 0.f)
	{
		SavedFalloff = Fog->FogHeightFalloff;
		SavedMax = Fog->FogMaxOpacity;
		SavedStart = Fog->StartDistance;
	}
	const FWRTuning& T = FWRTuning::Get();
	// Visibility (m) by where the head is: past the Blue Wall 12, the reef face 25, else 40; Sea fog halves it.
	float Vis = T.Num(TEXT("underwater.visShallow"), 40.f);
	if (Me)
	{
		const FVector P = Me->GetActorLocation();
		if (P.X > T.Num(TEXT("underwater.dropX"), 350000.f)) Vis = T.Num(TEXT("underwater.visDeep"), 12.f);
		else if (WROcean::ZoneAt(P) == TEXT("O5")) Vis = T.Num(TEXT("underwater.visReef"), 25.f);
		if (const FWRWeatherKind* K = UWRWeatherSubsystem::ActiveKind(World, WROcean::Biome()); K && K->Id == TEXT("fog")) Vis *= T.Num(TEXT("underwater.fogVisMult"), 0.5f);
		if (WROcean::InCave(P)) Vis = FMath::Min(Vis, T.Num(TEXT("underwater.visCave"), 18.f));
	}
	// Exponential fog: density so the view is ~95% gone at Vis metres (UE's density is per 100 m-ish units).
	const float Density = T.Num(TEXT("underwater.fogPerVis"), 3.0f) / FMath::Max(4.f, Vis);
	Fog->SetFogDensity(FMath::Lerp(Fog->FogDensity, Density, Blend));
	Fog->SetFogHeightFalloff(FMath::Lerp(SavedFalloff, 0.001f, Blend));
	Fog->SetFogMaxOpacity(FMath::Lerp(SavedMax, 1.f, Blend));
	Fog->SetStartDistance(FMath::Lerp(SavedStart, 0.f, Blend));
	const FLinearColor Water = FLinearColor(FColor(0x1E, 0x7C, 0x86)) * T.Num(TEXT("underwater.fogBrightness"), 0.35f);
	Fog->SetFogInscatteringColor(FMath::Lerp(Fog->FogInscatteringLuminance, Water, Blend));
	Fog->SetDirectionalInscatteringColor(FMath::Lerp(Fog->DirectionalInscatteringLuminance, FLinearColor::Black, Blend));
}

// ---------------------------------------------------------------- the component

UWRDiveComponent::UWRDiveComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.f;
	SetIsReplicatedByDefault(true);
}

void UWRDiveComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UWRDiveComponent, bUnder);
	DOREPLIFETIME_CONDITION(UWRDiveComponent, Breath, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRDiveComponent, MaxBreath, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRDiveComponent, Depth, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRDiveComponent, CrushDepth, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRDiveComponent, bNoLimit, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRDiveComponent, HurtCount, COND_OwnerOnly);
}

UWRDiveComponent* UWRDiveComponent::Of(const AActor* Who)
{
	return Who ? Who->FindComponentByClass<UWRDiveComponent>() : nullptr;
}

float UWRDiveComponent::SinceHurt() const
{
	const UWorld* W = GetWorld();
	return W ? float(W->GetTimeSeconds() - LastHurtAt) : 100.f;
}

void UWRDiveComponent::OnRep_Hurt()
{
	if (const UWorld* W = GetWorld()) LastHurtAt = W->GetTimeSeconds();
}

float UWRDiveComponent::MaxBreathFor(const AActor* Who)
{
	const FWRTuning& T = FWRTuning::Get();
	const UWRLifeSkillsComponent* Skills = UWRLifeSkillsComponent::FindFor(Who);
	const int32 Level = Skills ? Skills->GetLevel(TEXT("swimming")) : 0;
	float Base = T.Num(TEXT("underwater.breath.base"), 30.f) + T.Num(TEXT("underwater.breath.perSwimLevel"), 0.6f) * Level;
	// Diver (perk) x1.5; the diving helm, Reefmail's four and the Deepbreath brew x2 (the strongest of them counts).
	Base *= 1.f + UWRLifeSkillsComponent::StatFor(Who, TEXT("breathMult"));
	const float GearMult = 1.f + FMath::Max(UWRInventoryComponent::GearStatFor(Who, TEXT("breathMult")), WRVoid::BrewStat(Who, TEXT("breathMult")));
	Base *= GearMult;
	// Flat seconds: the pearl ring's +5, the pearl circlet's +10 (gear), the Diver's draught's 90 s floor (a brew).
	Base += UWRInventoryComponent::GearStatFor(Who, TEXT("breathSeconds"));
	Base = FMath::Max(Base, WRVoid::BrewStat(Who, TEXT("breathFloor")));
	return Base;
}

float UWRDiveComponent::CrushDepthFor(const AActor* Who)
{
	const FWRTuning& T = FWRTuning::Get();
	if (UWRLifeSkillsComponent::StatFor(Who, TEXT("noCrush")) > 0.f) return 1.0e9f;              // Abyss diver
	float Limit = T.Num(TEXT("underwater.crush.depth"), 2500.f);
	if (WRUnderwater::HasGills(Who)) Limit = FMath::Max(Limit, T.Num(TEXT("underwater.crush.gills"), 8000.f));
	Limit = FMath::Max(Limit, UWRInventoryComponent::GearStatFor(Who, TEXT("passDepth")));         // helm, Reefmail's four
	Limit = FMath::Max(Limit, T.Num(TEXT("underwater.crush.depth"), 2500.f) + UWRLifeSkillsComponent::StatFor(Who, TEXT("crushDepth")));  // Pressure
	return Limit;
}

void UWRDiveComponent::HoldBreath(float Seconds)
{
	if (const UWorld* W = GetWorld()) HeldUntil = FMath::Max(HeldUntil, W->GetTimeSeconds() + Seconds);
}

void UWRDiveComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AActor* Owner = GetOwner();
	if (!Owner) return;
	if (Owner->HasAuthority())
	{
		Accum += DeltaTime;
		if (Accum >= 0.1f)
		{
			ServerTick(Accum);
			Accum = 0.f;
		}
	}
	if (GetNetMode() != NM_DedicatedServer) LocalTick(DeltaTime);
}

void UWRDiveComponent::ServerTick(float Dt)
{
	AWRCharacterBase* Who = Cast<AWRCharacterBase>(GetOwner());
	if (!Who) return;
	const FWRTuning& T = FWRTuning::Get();
	const FVector Head = UwHead(Who);
	APhysicsVolume* Water = Who->IsAlive() ? WRUnderwater::WaterAt(GetWorld(), Head) : nullptr;
	bUnder = Water != nullptr;
	Depth = bUnder ? FMath::Max(0.f, WRUnderwater::SurfaceZ(Water, Head) - float(Head.Z)) : 0.f;
	bNoLimit = WRUnderwater::HasGills(Who) || WRVoid::BrewStat(Who, TEXT("noBreathLimit")) > 0.f;
	MaxBreath = MaxBreathFor(Who);
	CrushDepth = CrushDepthFor(Who);
	if (!Who->IsAlive())
	{
		Breath = MaxBreath;
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (bUnder && !bNoLimit)
	{
		Breath = FMath::Max(0.f, Breath - Dt);
	}
	else if (!bUnder && Now >= HeldUntil)
	{
		// Back at the surface or in an air pocket: full in refillSeconds whatever the max.
		Breath = FMath::Min(MaxBreath, Breath + MaxBreath * Dt / FMath::Max(0.2f, T.Num(TEXT("underwater.breath.refillSeconds"), 3.f)));
	}
	Breath = FMath::Min(Breath, MaxBreath);

	const auto Hurt = [&](float Damage, const TCHAR* Why)
	{
		FWRHit Hit;
		Hit.Instigator = nullptr;
		Hit.Damage = Damage;
		Hit.bDamageOverTime = true;
		Hit.bUnblockable = true;
		Hit.Impact = 0;
		Hit.StanceDamage = 0.f;
		Hit.Origin = Who->GetActorLocation();
		UWRCombatLibrary::ApplyHit(Who, Hit);
		++HurtCount;
		LastHurtAt = Now;
		UE_LOG(LogWildreach, Verbose, TEXT("DIVE %s %s %.0f (depth %.0f cm, breath %.1f)"), *Who->GetName(), Why, Damage, Depth, Breath);
	};
	// Out of breath: 10 a second until you surface. No floor (you drown).
	if (bUnder && !bNoLimit && Breath <= 0.f)
	{
		DrownClock += Dt;
		if (DrownClock >= 1.f)
		{
			DrownClock -= 1.f;
			Hurt(T.Num(TEXT("underwater.breath.drownDps"), 10.f), TEXT("drowning"));
		}
	}
	else DrownClock = 0.f;
	// Crushing depth: 5 a second past your limit.
	if (bUnder && Depth > CrushDepth)
	{
		CrushClock += Dt;
		if (CrushClock >= 1.f)
		{
			CrushClock -= 1.f;
			Hurt(T.Num(TEXT("underwater.crush.dps"), 5.f), TEXT("crushed"));
		}
	}
	else CrushClock = 0.f;
}

void UWRDiveComponent::LocalTick(float Dt)
{
	AWRCharacterBase* Who = Cast<AWRCharacterBase>(GetOwner());
	if (!Who) return;
	const bool bMine = Who->IsLocallyControlled();
	const FWRTuning& T = FWRTuning::Get();
	// Going under / coming up: the splash-in muffle and the gasp (everyone near hears the gasp).
	if (bUnder != bWasUnder)
	{
		if (!bUnder && bWasUnder) WRSound::PlayOn(Who, Breath < MaxBreath * 0.35f ? FName(TEXT("ocean.gasp")) : FName(TEXT("ocean.surface")));
		else WRSound::PlayOn(Who, TEXT("ocean.go_under"));
		bWasUnder = bUnder;
		bPopped = false;
		bHumming = false;
		HeartClock = 0.f;
	}
	if (!bUnder) return;
	// Bubbles from the mouth: a slow trickle, a stream when the breath is low (a quarter left) or you're drowning.
	const float Frac = MaxBreath > 0.f ? Breath / MaxBreath : 1.f;
	const bool bLow = bMine ? (!bNoLimit && Frac <= T.Num(TEXT("underwater.breath.lowFraction"), 0.25f)) : false;
	BubbleClock -= Dt;
	if (BubbleClock <= 0.f)
	{
		BubbleClock = bLow ? 0.35f : 2.2f + FMath::FRand() * 1.6f;
		WRFx::SpawnAt(Who, bLow ? TEXT("ocean.bubbles_stream") : TEXT("ocean.bubbles_mouth"), UwHead(Who) + Who->GetActorForwardVector() * 12.f, FRotator::ZeroRotator, 1.f);
	}
	if (!bMine) return;
	// The heartbeat comes up in the mix as the breath runs out (owner only).
	if (bLow)
	{
		HeartClock -= Dt;
		if (HeartClock <= 0.f)
		{
			HeartClock = FMath::Lerp(0.55f, 0.9f, FMath::Clamp(Frac / 0.25f, 0.f, 1.f));
			WRSound::PlayOn(Who, TEXT("ocean.heartbeat"));
		}
	}
	// Past 20 m the pressure hum; at 25 m the ears pop.
	if (!bHumming && Depth > T.Num(TEXT("underwater.pressureHumDepth"), 2000.f))
	{
		bHumming = true;
		WRSound::PlayOn(Who, TEXT("ocean.pressure"));
	}
	if (!bPopped && Depth > T.Num(TEXT("underwater.earPopDepth"), 2500.f))
	{
		bPopped = true;
		WRSound::PlayOn(Who, TEXT("ocean.ear_pop"));
	}
	if (Depth < T.Num(TEXT("underwater.pressureHumDepth"), 2000.f) - 300.f) { bHumming = false; bPopped = false; }
}
