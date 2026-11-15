// The Wind path's abilities (cooks/biome-sky.md 2.18.3, combat.md 2.12-2.13). See WRAbilitiesWind.h; the path's rules
// (Momentum, knockback, passives, combos) are in WRWindPath.cpp.

#include "WRAbilitiesWind.h"
#include "WRFungalHazards.h"
#include "Wildreach.h"
#include "WRAbilitiesLightning.h"
#include "WRAttributeSet.h"
#include "WRCharacter.h"
#include "WRCharacterBase.h"
#include "WRCharacterMovementComponent.h"
#include "WRCombatLibrary.h"
#include "WRCombos.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRInventory.h"
#include "WRPathComponent.h"
#include "WRPlayerController.h"
#include "WRProjectile.h"
#include "WRSkyAir.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRSwampSystems.h"
#include "WRTuning.h"
#include "WRWeapons.h"
#include "WRWindPath.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"

// ---------------------------------------------------------------- shared helpers (named: unity builds join files)

namespace WRWindAbil
{
	static const FName WaWindPath(TEXT("wind"));

	static AWRCharacterBase* WaLocked(const AWRCharacterBase* Caster)
	{
		const AWRPlayerController* PC = Caster ? Cast<AWRPlayerController>(Caster->GetController()) : nullptr;
		AWRCharacterBase* Target = PC ? PC->GetServerLock() : nullptr;
		return Target && Target->IsAlive() && !Target->IsHidden() && Caster->IsHostileTo(Target)
			&& FVector::Dist(Target->GetActorLocation(), Caster->GetActorLocation()) <= WRTune(TEXT("spells.lockRange"), 4000.f) ? Target : nullptr;
	}

	static FVector WaThroughCrosshair(const AWRCharacterBase* Caster, const FVector& From)
	{
		const FVector View = Caster->GetBaseAimRotation().Vector();
		const AWRCharacter* Survivor = Cast<AWRCharacter>(Caster);
		FVector Point;
		if (!Survivor || !Survivor->GetController() || !Survivor->CrosshairPoint(20000.f, true, Point))
		{
			return View;
		}
		const FVector Dir = (Point - From).GetSafeNormal();
		return Dir.IsNearlyZero() || FVector::DistSquared(Point, From) < FMath::Square(100.f) || FVector::DotProduct(Dir, View) < 0.3f ? View : Dir;
	}

	static FVector WaGround(const UWorld* World, const FVector& At, const AActor* Ignore, float Up = 500.f, float Down = 3000.f)
	{
		FHitResult Hit;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(WRWindGround), false, Ignore);
		return World->LineTraceSingleByChannel(Hit, At + FVector(0, 0, Up), At - FVector(0, 0, Down), ECC_WorldStatic, Query) ? FVector(Hit.ImpactPoint) : At;
	}

	static bool WaClearLine(const AWRCharacterBase* Caster, const AActor* Target)
	{
		FHitResult Hit;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(WRWindSight), false, Caster);
		Query.AddIgnoredActor(Target);
		return !Caster->GetWorld()->LineTraceSingleByChannel(Hit, Caster->GetActorLocation() + FVector(0, 0, 50), Target->GetActorLocation(), ECC_WorldStatic, Query);
	}

	static FVector WaFeet(const AWRCharacterBase* Who)
	{
		return Who->GetActorLocation() - FVector(0.f, 0.f, Who->GetSimpleCollisionHalfHeight());
	}

	/** In the air: jumping, falling, gliding, hovering (both machines read the movement the same). */
	static bool WaAirborne(const AWRCharacterBase* Who)
	{
		const UCharacterMovementComponent* Move = Who ? Who->GetCharacterMovement() : nullptr;
		return Move && (Move->IsFalling() || Move->IsFlying() || WRSkyAir::IsGliding(Who));
	}

	/** How high over the ground the body's feet are (cm), up to Max. */
	static float WaHeight(const AWRCharacterBase* Who, float Max = 20000.f)
	{
		FHitResult Hit;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(WRWindHeight), false, Who);
		const FVector F = WaFeet(Who);
		return Who->GetWorld()->LineTraceSingleByChannel(Hit, F, F - FVector(0, 0, Max), ECC_WorldStatic, Query) ? Hit.Distance : Max;
	}

	/**
	 * The half-effect rule, as both machines see it (cooks/biome-sky.md 2.18.2): 1 with the Momentum to pay (or free in
	 * Sky lord), else the half effect. The server takes the Momentum with WRWind::PayMomentum at the same moment.
	 */
	static float WaPredict(const AWRCharacterBase* Who, float Cost, bool bFreeInSkyLord)
	{
		if (Cost <= 0.f || (bFreeInSkyLord && WRWind::InSkyLord(Who))) return 1.f;
		return WRWind::MomentumValue(Who) >= Cost ? 1.f : WRTune(TEXT("resources.momentum.halfEffect"), 0.5f);
	}

	/** The held weapon's light (or heavy) blow as a Wildbrand reads it: the class's own base over the sword's. */
	static float WaWeaponHit(const AWRCharacterBase* C, bool bHeavy, float Mult)
	{
		const FWRWeaponClass& Class = FWRWeapons::Get().ClassFor(C);
		float ClassScale = 1.f;
		if (C->Weapon.FlatDamage <= 0.f && !C->IsCreature())
		{
			const float Sword = bHeavy ? WRTune(TEXT("weapon.copperSwordHeavy"), 22.f) : WRTune(TEXT("weapon.copperSwordLight"), 12.f);
			ClassScale = FWRWeapons::ClassBase(Class, bHeavy) / FMath::Max(1.f, Sword);
		}
		return UWRCombatLibrary::WeaponDamage(C->Weapon, bHeavy) * ClassScale * Mult;
	}

	/** A weapon blow of a Wind Wildbrand (Whirlwind's spins, Plunge, Sky lord's strikes, debris): melee, element Wind. */
	static void WaWeaponBlow(AWRCharacterBase* C, AWRCharacterBase* Target, float Mult, float StanceMult, bool bHeavy, const FVector& Origin, FName PathNode)
	{
		const FWRWeaponClass& Class = FWRWeapons::Get().ClassFor(C);
		const FWRMove* Light = Class.Move(TEXT("light1"));
		const FWRMove* Heavy = Class.Move(TEXT("heavy"));
		const FWRMove* Base = bHeavy ? Heavy : Light;
		FWRHit Hit;
		Hit.Instigator = C;
		Hit.Damage = WaWeaponHit(C, bHeavy, Mult);
		Hit.PoiseDamage = Base ? Base->Poise : 30.f;
		Hit.StanceDamage = (Light ? Light->Stance : 5.f) * StanceMult;
		Hit.StaminaDamage = Base ? Base->GuardStamina : 30.f;
		Hit.Impact = bHeavy ? 3 : 2;
		Hit.Tier = bHeavy ? EWRHitTier::Heavy : EWRHitTier::Light;
		Hit.PhysType = Base ? Base->PhysType : 1;
		Hit.Element = WRWind::Element_Wind;
		Hit.PathId = WaWindPath;
		Hit.bMelee = true;
		Hit.bHeavy = bHeavy;
		Hit.bWeaponArt = true;
		Hit.Origin = Origin;
		Hit.ImpactPoint = Target->GetActorLocation();
		UWRCombatLibrary::ApplyHit(Target, Hit);
		UE_LOG(LogWildreach, Verbose, TEXT("WIND %s %s hits %s"), *C->GetName(), *PathNode.ToString(), *Target->GetName());
	}

	/** Everyone hostile to C, alive, within Radius (+ their own size) of At and HalfHeight of it up or down. */
	static TArray<AWRCharacterBase*> WaAround(AWRCharacterBase* C, const FVector& At, float Radius, float HalfHeight = 250.f)
	{
		TArray<AWRCharacterBase*> Out;
		for (TActorIterator<AWRCharacterBase> It(C->GetWorld()); It; ++It)
		{
			if (*It == C || !It->IsAlive() || It->IsHidden() || !C->IsHostileTo(*It)) continue;
			const FVector L = It->GetActorLocation();
			if (FVector::Dist2D(L, At) <= Radius + It->GetSimpleCollisionRadius() && FMath::Abs(L.Z - At.Z) <= HalfHeight + It->GetSimpleCollisionHalfHeight())
			{
				Out.Add(*It);
			}
		}
		return Out;
	}

	/** The ranged weapon in C's ranged slot, if it's a bow or a crossbow. */
	static const FWRItemDef* WaBowOf(const AActor* Who)
	{
		const AWRCharacter* S = Cast<AWRCharacter>(Who);
		const FWRItemDef* D = S && !S->GetRangedItem().IsNone() ? FWRItemRegistry::Get().Find(S->GetRangedItem()) : nullptr;
		return D && (D->RangedKind == TEXT("bow") || D->RangedKind == TEXT("crossbow")) ? D : nullptr;
	}

	/** An arrow to draw (Storm volley's rain): the plain stone arrow's model. */
	static const FWRItemDef* WaArrowModel()
	{
		static const FName Id(TEXT("stone_arrow"));
		return FWRItemRegistry::Get().Find(Id);
	}

	/** A projectile shot on the wind (Wind arrow, Storm volley, debris): spawned on the server, never homing. */
	static AWRProjectile* WaShoot(AWRCharacterBase* C, const FWRHit& Hit, const FVector& From, const FVector& Velocity, float Gravity, const FString& Model, const FString& Along, FName Kind)
	{
		FActorSpawnParameters Params;
		Params.Owner = C;
		Params.Instigator = C;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AWRProjectile* P = C->GetWorld()->SpawnActor<AWRProjectile>(AWRProjectile::StaticClass(), From, Velocity.Rotation(), Params);
		if (P)
		{
			FWRHit Shot = Hit;
			Shot.bReflectable = true;   // a wind arrow flies straight: a reflector can send it back (crystal 2.12)
			P->InitPhysical(Shot, Velocity, Gravity, Model, Along, Kind, NAME_None);
		}
		return P;
	}
}

using namespace WRWindAbil;

// ---------------------------------------------------------------- the spell base

UWRGA_WindSpell::UWRGA_WindSpell()
{
	StaminaKey = TEXT("stamina.spell");
	StaminaFallback = 12.f;
	PathId = TEXT("wind");
	Element = WRWind::Element_Wind;
	ActivationOwnedTags.AddTag(WRTags::State_Attacking);
	FGameplayTagContainer Blocked = ActivationBlockedTags;
	Blocked.AddTag(WRTags::State_Attacking);
	Blocked.AddTag(WRTags::State_Dodging);
	ActivationBlockedTags = Blocked;
	bAllowedMounted = true;
}

float UWRGA_WindSpell::Spell(const TCHAR* Field, float Fallback) const
{
	return FWRTuning::Get().Num(SpellKey + TEXT(".") + Field, Fallback);
}

FVector UWRGA_WindSpell::AimedGround(AWRCharacterBase* Caster, float Range) const
{
	const FVector F = WaFeet(Caster);
	if (const AWRCharacterBase* Lock = WaLocked(Caster))
	{
		const FVector Off = Lock->GetActorLocation() - F;
		return WaGround(Caster->GetWorld(), FVector(F.X, F.Y, Lock->GetActorLocation().Z) + FVector(Off.X, Off.Y, 0.f).GetClampedToMaxSize(Range), Caster);
	}
	const AWRCharacter* Survivor = Cast<AWRCharacter>(Caster);
	FVector Point;
	if (Survivor && Survivor->CrosshairPoint(Range, false, Point))
	{
		return Point;
	}
	const FVector Flat = FRotator(0.f, Caster->GetBaseAimRotation().Yaw, 0.f).Vector();
	return WaGround(Caster->GetWorld(), F + Flat * Range * 0.7f, Caster);
}

void UWRGA_WindSpell::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	AWRCharacterBase* Caster = GetCharacter();
	if (!Caster)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	const FWRTuning& T = FWRTuning::Get();
	const FString ShapeRow = TEXT("spells.shapes.") + ShapeName;
	// Cast into a Whirlwind's spin (Vortex): no wind-up, it comes out of the spin.
	const bool bInstant = bInstantWhileSpinning && Caster->HasTag(WRWind::State_Spinning);
	const float Windup = bInstant ? 0.f : Spell(TEXT("windup"), T.Num(ShapeRow + TEXT(".windup"), 0.3f));
	const float Recovery = bInstant ? T.Num(TEXT("spells.vortex.spinRecovery"), 0.15f) : Spell(TEXT("recovery"), T.Num(ShapeRow + TEXT(".recovery"), 0.35f));
	if (!bInstant)
	{
		if (T.Num(ShapeRow + TEXT(".roots"), 0.f) > 0.f) SetMoveRoot(true);
		else SetMoveSlow(true, true);
		Caster->PlaySwing(EWRSwingKind::Cast, Windup + 0.1f);
	}
	Caster->SetCombatTag(WRTags::State_Casting, true);
	if (bSpinsWhileCasting && !Caster->HasTag(WRWind::State_Spinning))
	{
		Caster->SetCombatTag(WRWind::State_Spinning, true);
		bSpinTagSet = true;
	}
	if (Spell(TEXT("hyperarmor"), 0.f) > 0.f)
	{
		Caster->SetCombatTag(WRTags::State_Hyperarmor, true);
	}
	if (IsServer())
	{
		WRFx::BroadcastOn(Caster, TEXT("cast.wind"), TEXT("hand"), 1.f, CastSound, Windup + 0.1f);
	}
	const float Cost = Spell(TEXT("momentumCost"), 0.f);
	After(Windup, [this, Cost]()
	{
		AWRCharacterBase* C = GetCharacter();
		if (!C)
		{
			return;
		}
		C->SetCombatTag(WRTags::State_Casting, false);
		if (bSpinTagSet)
		{
			C->SetCombatTag(WRWind::State_Spinning, false);
			bSpinTagSet = false;
		}
		if (!C->IsAlive())
		{
			return;
		}
		// Both machines see the same scale (the owner's Momentum is replicated to it); the server takes the price.
		const float Scale = WaPredict(C, Cost, bFreeInSkyLord);
		if (IsServer())
		{
			WRWind::PayMomentum(C, Cost, bFreeInSkyLord);
			CastWind(C, Scale);
			// The Fungal caverns (fungal 2.8): a wind spell moves spore clouds along its gust (15 m: a spell is a strong
			// gust), tears lure lines and the web sheets (WRFungalHazards).
			WRFungal::Wind(C, C->GetActorLocation() + FVector(0.f, 0.f, 50.f), C->GetBaseAimRotation().Vector(), Spell(TEXT("range"), 900.f), Spell(TEXT("cone"), 60.f), true);
		}
		CastBoth(C, Scale);
	});
	After(Windup + Recovery, [this]() { EndSelf(); });
}

void UWRGA_WindSpell::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (AWRCharacterBase* C = GetCharacter())
	{
		C->SetCombatTag(WRTags::State_Casting, false);
		if (bSpinTagSet)
		{
			C->SetCombatTag(WRWind::State_Spinning, false);
			bSpinTagSet = false;
		}
		if (Spell(TEXT("hyperarmor"), 0.f) > 0.f)
		{
			C->SetCombatTag(WRTags::State_Hyperarmor, false);
		}
	}
	SelfPush = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// ---------------------------------------------------------------- Gust

UWRGA_Gust::UWRGA_Gust()
{
	DisplayName = TEXT("Gust"); NodeId = TEXT("gust");
	SpellKey = TEXT("spells.gust"); CooldownKey = TEXT("spells.gust.cooldown"); CooldownFallback = 4.f;
	ShapeName = TEXT("bolt"); CastSound = TEXT("wind.cast");
	bFreeInSkyLord = true;
}

void UWRGA_Gust::CastWind(AWRCharacterBase* Caster, float Scale)
{
	const float Range = Spell(TEXT("range"), 700.f);
	const float CosCone = FMath::Cos(FMath::DegreesToRadians(Spell(TEXT("cone"), 40.f) * 0.5f));
	const FVector Eye = Caster->GetActorLocation() + FVector(0.f, 0.f, 50.f);
	const FVector Aim = WaLocked(Caster) ? (WaLocked(Caster)->GetActorLocation() - Eye).GetSafeNormal() : Caster->GetBaseAimRotation().Vector();
	WRFx::Broadcast(Caster, TEXT("wind.gust"), Eye + Aim * 60.f, Aim.Rotation(), Range / 100.f, TEXT("wind.gust"));
	for (TActorIterator<AWRCharacterBase> It(Caster->GetWorld()); It; ++It)
	{
		AWRCharacterBase* Who = *It;
		if (Who == Caster || !Who->IsAlive() || Who->IsHidden() || !Caster->IsHostileTo(Who)) continue;
		const FVector To = Who->GetActorLocation() - Eye;
		const float Dist = To.Size();
		if (Dist > Range + Who->GetSimpleCollisionRadius() || FVector::DotProduct(To.GetSafeNormal(), Aim) < CosCone || !WaClearLine(Caster, Who)) continue;
		WRWind::WindHit(Caster, Who, Spell(TEXT("damage"), 6.f) * Scale, Spell(TEXT("poise"), 20.f) * Scale, Spell(TEXT("stance"), 1.5f), Eye, true, true);
		WRWind::Knock(Caster, Who, To, Spell(TEXT("push"), 600.f) * Scale, Spell(TEXT("pushSeconds"), 0.3f), TEXT("gust"));
	}
}

void UWRGA_Gust::CastBoth(AWRCharacterBase* Caster, float Scale)
{
	// In the air the blast throws you the other way too: a mid-air reposition (predicted on the owner, the server agrees).
	if (!WaAirborne(Caster))
	{
		return;
	}
	const float Dist = Spell(TEXT("selfPush"), 300.f) * Scale;
	const float Secs = FMath::Max(0.05f, Spell(TEXT("selfPushSeconds"), 0.25f));
	const FVector Back = -Caster->GetBaseAimRotation().Vector();
	SelfPush = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(this, TEXT("GustSelf"), Back, Dist / Secs, Secs, false, nullptr,
		ERootMotionFinishVelocityMode::ClampVelocity, FVector::ZeroVector, Caster->GetCharacterMovement()->GetMaxSpeed(), false);
	if (SelfPush)
	{
		SelfPush->ReadyForActivation();
	}
}

// ---------------------------------------------------------------- Vortex

UWRGA_Vortex::UWRGA_Vortex()
{
	DisplayName = TEXT("Vortex"); NodeId = TEXT("vortex");
	SpellKey = TEXT("spells.vortex"); CooldownKey = TEXT("spells.vortex.cooldown"); CooldownFallback = 14.f;
	ShapeName = TEXT("nova"); CastSound = TEXT("wind.vortex");
	bInstantWhileSpinning = true;
	bSpinsWhileCasting = true;
	// It's cast into Whirlwind's spin: a spin's own attacking state doesn't hold it back.
	FGameplayTagContainer Blocked;
	Blocked.AddTag(WRTags::State_Dodging);
	Blocked.AddTag(WRTags::State_Casting);
	ActivationBlockedTags = Blocked;
	ActivationOwnedTags = FGameplayTagContainer();
}

void UWRGA_Vortex::CastWind(AWRCharacterBase* Caster, float Scale)
{
	const float Radius = Spell(TEXT("radius"), 600.f);
	const FVector Centre = Caster->GetActorLocation();
	WRFx::Broadcast(Caster, TEXT("wind.vortex"), WaFeet(Caster), FRotator::ZeroRotator, Radius / 100.f, TEXT("wind.vortex"));
	for (AWRCharacterBase* Who : WaAround(Caster, Centre, Radius, 400.f))
	{
		const FVector To = Centre - Who->GetActorLocation();
		const float Room = FMath::Max(0.f, To.Size2D() - Caster->GetSimpleCollisionRadius() - Who->GetSimpleCollisionRadius() - Spell(TEXT("stopShort"), 60.f));
		WRWind::WindHit(Caster, Who, Spell(TEXT("damage"), 8.f) * Scale, Spell(TEXT("poise"), 10.f) * Scale, Spell(TEXT("stance"), 1.f), Centre, true, true);
		WRWind::Knock(Caster, Who, To, FMath::Min(Spell(TEXT("pull"), 400.f) * Scale, Room), Spell(TEXT("pullSeconds"), 0.4f), TEXT("vortex"));
	}
}

// ---------------------------------------------------------------- Storm volley

UWRGA_StormVolley::UWRGA_StormVolley()
{
	DisplayName = TEXT("Storm volley"); NodeId = TEXT("storm_volley");
	SpellKey = TEXT("spells.stormVolley"); CooldownKey = TEXT("spells.stormVolley.cooldown"); CooldownFallback = 50.f;
	ShapeName = TEXT("strike"); CastSound = TEXT("wind.volley_draw");
	bAllowedMounted = false;
}

bool UWRGA_StormVolley::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags) && WaBowOf(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr) != nullptr;
}

void UWRGA_StormVolley::CastWind(AWRCharacterBase* Caster, float Scale)
{
	UWorld* World = Caster->GetWorld();
	const float Radius = Spell(TEXT("radius"), 600.f);
	const FVector Target = AimedGround(Caster, Spell(TEXT("range"), 2500.f));
	const int32 Arrows = FMath::Max(1, FMath::RoundToInt(Spell(TEXT("arrows"), 30.f)));
	const float Delay = Spell(TEXT("delay"), 0.9f);
	const float Spread = Spell(TEXT("spread"), 1.5f);
	const float FallSpeed = FMath::Min(Spell(TEXT("fallSpeed"), 1350.f), 1400.f);
	const float Height = Spell(TEXT("height"), 1500.f);
	// The loosed volley goes up from the bow (the release), and the ground it'll come down on is marked for its whole fall
	// (other players read it).
	WRFx::Broadcast(Caster, TEXT("wind.volley_up"), Caster->GetActorLocation() + FVector(0, 0, 90.f), FRotator(90.f, 0.f, 0.f), 1.f, TEXT("wind.volley"));
	WRFx::Broadcast(Caster, TEXT("wind.volley_mark"), Target, FRotator::ZeroRotator, Radius / 100.f, NAME_None, Delay + Spread + Height / FallSpeed);
	const FWRItemDef* Model = WaArrowModel();
	const FString Mesh = Model ? Model->ProjectileMesh : FString();
	const FString Along = Model ? Model->MeshAlong : FString();
	const bool bEmber = Caster->HasTag(WRWind::Combo_EmberRain);
	const bool bHex = Caster->HasTag(WRWind::Combo_HexStorm);
	// Hex storm (Blood + Wind 50): the bodies this volley hits, counted, so a third arrow curses.
	TSharedRef<TMap<TWeakObjectPtr<AActor>, int32>> Counts = MakeShared<TMap<TWeakObjectPtr<AActor>, int32>>();
	const FVector FromSide = (Caster->GetActorLocation() - Target).GetSafeNormal2D();
	TWeakObjectPtr<AWRCharacterBase> WeakC(Caster);
	const float ArrowDamage = Spell(TEXT("damage"), 8.f) * Scale;
	const float ArrowPoise = Spell(TEXT("poise"), 6.f);
	for (int32 i = 0; i < Arrows; ++i)
	{
		const float At = Delay - Height / FallSpeed + Spread * (float(i) + FMath::FRand() * 0.6f) / float(Arrows);
		const FVector2D Disc = FMath::RandPointInCircle(Radius);
		const FVector Spot = WaGround(World, Target + FVector(Disc.X, Disc.Y, 0.f), Caster, 800.f, 1500.f);
		// Each arrow falls along a slant from the side it was loosed from: a rain you can see coming.
		const FVector From = Spot + FVector(0.f, 0.f, Height) + FromSide * Height * Spell(TEXT("slant"), 0.18f);
		const float FlightSeconds = FVector::Dist(From, Spot) / FallSpeed;
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(Caster, [WeakC, From, Spot, FallSpeed, ArrowDamage, ArrowPoise, Mesh, Along, bEmber, bHex, Counts]()
		{
			AWRCharacterBase* C = WeakC.Get();
			if (!C || !C->IsAlive())
			{
				return;
			}
			FWRHit Hit;
			Hit.Instigator = C;
			Hit.Damage = ArrowDamage * WRWind::ProjectileDamageScale(C);
			Hit.PoiseDamage = ArrowPoise;
			Hit.Element = WRWind::Element_Wind;
			Hit.PathId = WaWindPath;
			Hit.PhysType = 3;
			Hit.bProjectile = true;
			if (bEmber) Hit.BurnBuildup += FWRComboRegistry::Get().Num(TEXT("ember_rain"), TEXT("burn"), 20.f);
			const FVector Dir = (Spot - From).GetSafeNormal();
			if (AWRProjectile* P = WaShoot(C, Hit, From, Dir * FallSpeed, 0.f, Mesh, Along, TEXT("arrow")))
			{
				if (bHex)
				{
					P->OnStruck = [WeakC, Counts](AWRCharacterBase* Target, EWRHitResult Result)
					{
						AWRCharacterBase* By = WeakC.Get();
						if (!By || !Target || Result != EWRHitResult::Hit) return;
						int32& N = Counts->FindOrAdd(Target);
						if (++N == FMath::RoundToInt(FWRComboRegistry::Get().Num(TEXT("hex_storm"), TEXT("arrows"), 3.f)) && Target->IsAlive())
						{
							Target->Status->AddStatusBuildup(WRStatus::Curse, FWRComboRegistry::Get().Num(TEXT("hex_storm"), TEXT("curse"), 100.f), By);
						}
					};
				}
			}
		}), FMath::Max(0.01f, At), false);
		if (bEmber)
		{
			// Ember rain (Fire + Wind 50): each arrow leaves a small fire where it comes down.
			FTimerHandle Fire;
			World->GetTimerManager().SetTimer(Fire, FTimerDelegate::CreateWeakLambda(Caster, [WeakC, Spot]()
			{
				if (AWRCharacterBase* C = WeakC.Get())
				{
					const FWRComboRegistry& R = FWRComboRegistry::Get();
					AWRWindPatch::Lay(C, Spot, R.Num(TEXT("ember_rain"), TEXT("patchRadius"), 100.f), R.Num(TEXT("ember_rain"), TEXT("patchSeconds"), 3.f), 0.f, 0.f,
						R.Num(TEXT("ember_rain"), TEXT("patchBurn"), 15.f));
				}
			}), FMath::Max(0.02f, At + FlightSeconds), false);
		}
	}
	UE_LOG(LogWildreach, Log, TEXT("WIND storm volley by %s: %d arrows on %s"), *Caster->GetName(), Arrows, *Target.ToString());
}

// ---------------------------------------------------------------- Air dash

UWRGA_AirDash::UWRGA_AirDash()
{
	DisplayName = TEXT("Air dash"); PathId = TEXT("wind"); NodeId = TEXT("air_dash");
	StaminaKey = TEXT("stamina.spell"); StaminaFallback = 12.f;
	CooldownKey = TEXT("spells.airDash.cooldown"); CooldownFallback = 3.f;
	ActivationOwnedTags.AddTag(WRWind::State_AirDash);
	FGameplayTagContainer Blocked = ActivationBlockedTags;
	Blocked.AddTag(WRTags::State_Dodging);
	Blocked.AddTag(WRWind::State_AirDash);
	ActivationBlockedTags = Blocked;
	CancelAbilitiesWithTag.AddTag(WRTags::Ability_Melee);
	CancelAbilitiesWithTag.AddTag(WRTags::Ability_Block);
	bRules = true;
	RuleAct = WRCombatRules::EAct::Dodge;
}

bool UWRGA_AirDash::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	const AWRCharacterBase* C = ActorInfo ? Cast<AWRCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	// In the air only, once each airtime (Sky lord's hover: any number).
	return C && WaAirborne(C) && (WRWind::AirDashReady(C) || WRWind::InSkyLord(C));
}

void UWRGA_AirDash::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AWRCharacterBase* C = GetCharacter();
	// Jet stream (Lightning + Wind 50): straight out of a Bolt dash it costs nothing and readies the Bolt dash again.
	const bool bJet = C && C->HasTag(WRWind::Combo_JetStream) && WRWind::BoltDashJustEnded(C);
	CostOverride = bJet ? 0.f : -1.f;
	if (!C || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (!WRWind::InSkyLord(C))
	{
		WRWind::NoteAirDash(C);
	}
	const FWRTuning& T = FWRTuning::Get();
	const float Cost = bJet ? 0.f : T.Num(TEXT("spells.airDash.momentumCost"), 10.f);
	const float Scale = WaPredict(C, Cost, true);
	if (IsServer())
	{
		WRWind::PayMomentum(C, Cost, true);
	}
	if (bJet)
	{
		if (UAbilitySystemComponent* ASC = C->GetAbilitySystemComponent())
		{
			if (FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(UWRGA_BoltDash::StaticClass()))
			{
				if (UWRGameplayAbility* Bolt = Cast<UWRGameplayAbility>(Spec->GetPrimaryInstance()))
				{
					Bolt->ResetCooldown();
				}
			}
		}
	}
	FVector Dir = C->GetCharacterMovement()->GetCurrentAcceleration().GetSafeNormal2D();
	if (Dir.IsNearlyZero())
	{
		Dir = FRotator(0.f, C->GetControlRotation().Yaw, 0.f).Vector();
	}
	const float Windup = T.Num(TEXT("spells.airDash.windup"), 0.04f);
	const float TravelTime = FMath::Max(0.05f, T.Num(TEXT("spells.airDash.travel"), 0.18f));
	const float Recovery = T.Num(TEXT("spells.airDash.recovery"), 0.15f);
	const float Distance = T.Num(TEXT("spells.airDash.distance"), 400.f) * Scale;
	C->SetActorRotation(Dir.Rotation());
	if (IsServer())
	{
		WRFx::BroadcastOn(C, TEXT("wind.dash"), NAME_None, 1.f, TEXT("wind.dash"), Windup + TravelTime);
	}
	After(Windup, [this, Dir, Distance, TravelTime]()
	{
		AWRCharacterBase* Ch = GetCharacter();
		if (!Ch)
		{
			return;
		}
		SetIFrames(true);
		Travel = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(this, TEXT("AirDash"), Dir, Distance / TravelTime, TravelTime, false, nullptr,
			ERootMotionFinishVelocityMode::ClampVelocity, FVector::ZeroVector, Ch->GetCharacterMovement()->GetMaxSpeed(), false);
		if (Travel)
		{
			Travel->ReadyForActivation();
		}
	});
	After(Windup + TravelTime, [this]() { SetIFrames(false); });
	After(Windup + TravelTime + Recovery, [this]() { EndSelf(); });
}

void UWRGA_AirDash::SetIFrames(bool bOn)
{
	AWRCharacterBase* C = GetCharacter();
	if (!C || !IsServer() || bIFrames == bOn)
	{
		return;
	}
	bIFrames = bOn;
	if (bOn) C->GetAbilitySystemComponent()->AddLooseGameplayTag(WRTags::State_Invulnerable);
	else C->GetAbilitySystemComponent()->RemoveLooseGameplayTag(WRTags::State_Invulnerable);
}

void UWRGA_AirDash::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	SetIFrames(false);
	Travel = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// ---------------------------------------------------------------- Wind cut

UWRGA_WindCut::UWRGA_WindCut()
{
	DisplayName = TEXT("Wind cut"); PathId = TEXT("wind"); NodeId = TEXT("wind_cut");
	StaminaKey = TEXT("stamina.weaponArt"); StaminaFallback = 18.f;
	CooldownKey = TEXT("spells.windCut.cooldown"); CooldownFallback = 2.5f;
	TimingKey = TEXT("weapon.lightTiming"); SwingKind = EWRSwingKind::WeaponArt;
	Element = WRWind::Element_Wind; ArtKey = TEXT("spells.windCut");
	StanceMultiplier = 1.5f;   // combat.md 2.12
}

void UWRGA_WindCut::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	AWRCharacterBase* C = GetCharacter();
	if (!C || !IsActive() || !IsServer())
	{
		return;
	}
	const FWRTuning& T = FWRTuning::Get();
	const float Cost = T.Num(TEXT("spells.windCut.momentumCost"), 15.f);
	const float Scale = WaPredict(C, Cost, true);
	WRWind::PayMomentum(C, Cost, true);
	WRFx::BroadcastOn(C, TEXT("wind.weapon"), TEXT("weapon"), 1.f, TEXT("wind.cut"), 0.5f);
	// The blade of wind leaves the edge as the swing's active frames begin.
	const float Windup = T.Num(TEXT("wildbrands.wind_cut.windup"), 0.24f);
	TWeakObjectPtr<AWRCharacterBase> WeakC(C);
	After(Windup, [WeakC, Scale]()
	{
		AWRCharacterBase* Ch = WeakC.Get();
		if (!Ch || !Ch->IsAlive())
		{
			return;
		}
		const FWRTuning& Tt = FWRTuning::Get();
		const FVector Dir = FRotator(0.f, Ch->GetBaseAimRotation().Yaw, 0.f).Vector();
		const FVector From = Ch->GetActorLocation() + Dir * 80.f;
		AWRWindWave::Launch(Ch, From, Dir, WaWeaponHit(Ch, false, Tt.Num(TEXT("spells.windCut.waveDamage"), 0.8f)) * Scale, Tt.Num(TEXT("spells.windCut.waveStance"), 1.f),
			Tt.Num(TEXT("spells.windCut.knockback"), 300.f) * Scale, Tt.Num(TEXT("spells.windCut.waveSpeed"), 2000.f), Tt.Num(TEXT("spells.windCut.waveRange"), 900.f),
			Tt.Num(TEXT("spells.windCut.waveWidth"), 160.f));
	});
}

// ---------------------------------------------------------------- Plunge

UWRGA_Plunge::UWRGA_Plunge()
{
	DisplayName = TEXT("Plunge"); PathId = TEXT("wind"); NodeId = TEXT("plunge");
	StaminaKey = TEXT("stamina.weaponArt"); StaminaFallback = 20.f;
	CooldownKey = TEXT("spells.plunge.cooldown"); CooldownFallback = 4.f;
	ActivationOwnedTags.AddTag(WRTags::State_Attacking);
	FGameplayTagContainer Blocked = ActivationBlockedTags;
	Blocked.AddTag(WRTags::State_Attacking);
	Blocked.AddTag(WRTags::State_Dodging);
	ActivationBlockedTags = Blocked;
}

bool UWRGA_Plunge::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	const AWRCharacterBase* C = ActorInfo ? Cast<AWRCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	FString Why;
	// Airborne only, with room to dive (not in Sky lord's hover: its heavy is its own dive).
	return C && WaAirborne(C) && !WRWind::InSkyLord(C) && UWRGA_Melee::WildbrandFits(C, NodeId, Why)
		&& WaHeight(C, 5000.f) >= WRTune(TEXT("spells.plunge.minHeight"), 150.f);
}

void UWRGA_Plunge::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	const FWRTuning& T = FWRTuning::Get();
	CostOverride = T.Num(TEXT("wildbrands.plunge.stamina"), 20.f);
	AWRCharacterBase* C = GetCharacter();
	if (!C || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	bLanded = false;
	StartZ = C->GetActorLocation().Z;
	const float Cost = T.Num(TEXT("spells.plunge.momentumCost"), 20.f);
	Scale = WaPredict(C, Cost, false);
	if (IsServer())
	{
		WRWind::PayMomentum(C, Cost, false);
		WRFx::BroadcastOn(C, TEXT("wind.weapon"), TEXT("weapon"), 1.2f, TEXT("wind.plunge_tell"), 0.6f);
	}
	// The dive lands you on your blade: its own landing never hurts (the cover lasts to the next landing).
	WRWind::CoverFall(C);
	C->SetCombatTag(WRTags::State_Hyperarmor, true);
	const float Windup = T.Num(TEXT("wildbrands.plunge.windup"), 0.2f);
	C->PlaySwing(EWRSwingKind::Heavy, Windup + 0.4f, 1);
	// The wind-up holds you in the air, blade raised.
	Travel = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(this, TEXT("PlungeHold"), FVector::UpVector, 0.f, Windup, false, nullptr,
		ERootMotionFinishVelocityMode::SetVelocity, FVector::ZeroVector, 0.f, false);
	if (Travel)
	{
		Travel->ReadyForActivation();
	}
	After(Windup, [this]()
	{
		AWRCharacterBase* Ch = GetCharacter();
		if (!Ch)
		{
			return;
		}
		// Down blade-first, leaning toward the lock (or the way you face).
		FVector Ahead = FRotator(0.f, Ch->GetControlRotation().Yaw, 0.f).Vector();
		if (const AWRCharacterBase* Lock = WaLocked(Ch))
		{
			Ahead = (Lock->GetActorLocation() - Ch->GetActorLocation()).GetSafeNormal2D();
		}
		const FVector Dir = (FVector(0.f, 0.f, -1.f) + Ahead * WRTune(TEXT("spells.plunge.lean"), 0.35f)).GetSafeNormal();
		const float Speed = WRTune(TEXT("spells.plunge.diveSpeed"), 2600.f);
		const float MaxSeconds = WRTune(TEXT("spells.plunge.maxDive"), 3.f);
		DiveStartedAt = GetWorld()->GetTimeSeconds();
		Travel = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(this, TEXT("PlungeDive"), Dir, Speed, MaxSeconds, false, nullptr,
			ERootMotionFinishVelocityMode::ClampVelocity, FVector::ZeroVector, 600.f, false);
		if (Travel)
		{
			Travel->ReadyForActivation();
		}
		if (IsServer())
		{
			WRFx::BroadcastOn(Ch, TEXT("wind.plunge"), NAME_None, 1.f, TEXT("wind.plunge_dive"), MaxSeconds);
		}
		DiveTick();
	});
}

void UWRGA_Plunge::DiveTick()
{
	AWRCharacterBase* C = GetCharacter();
	if (!C || bLanded)
	{
		return;
	}
	const bool bDown = !C->GetCharacterMovement()->IsFalling() || WaHeight(C, 300.f) < WRTune(TEXT("spells.plunge.landAt"), 30.f);
	if (bDown || GetWorld()->GetTimeSeconds() - DiveStartedAt >= WRTune(TEXT("spells.plunge.maxDive"), 3.f))
	{
		Land();
		return;
	}
	After(0.02f, [this]() { DiveTick(); });
}

void UWRGA_Plunge::Land()
{
	AWRCharacterBase* C = GetCharacter();
	if (!C || bLanded)
	{
		return;
	}
	bLanded = true;
	if (Travel)
	{
		Travel->EndTask();
		Travel = nullptr;
	}
	const FWRTuning& T = FWRTuning::Get();
	C->SetCombatTag(WRTags::State_Hyperarmor, false);
	SetMoveRoot(true);
	if (IsServer())
	{
		// The height pays: +0.2 a metre fallen, x2 at most (cooks/biome-sky.md 2.18.3).
		const float Metres = FMath::Max(0.f, (StartZ - C->GetActorLocation().Z) / 100.f);
		const float Height = FMath::Min(T.Num(TEXT("spells.plunge.maxMult"), 2.f), 1.f + T.Num(TEXT("spells.plunge.perMetre"), 0.2f) * Metres);
		const FVector At = WaFeet(C);
		const float HitRadius = T.Num(TEXT("spells.plunge.hitRadius"), 150.f);
		const float Ring = T.Num(TEXT("spells.plunge.ringRadius"), 300.f);
		const bool bAvalanche = C->HasTag(WRWind::Combo_AvalanchePlunge);
		WRFx::Broadcast(C, TEXT("wind.plunge_ring"), At, FRotator::ZeroRotator, Ring / 100.f, TEXT("wind.plunge_land"));
		WRFx::Shake(C, FMath::Clamp(0.25f + 0.1f * Metres, 0.25f, 0.8f), At, Ring * 4.f);
		for (AWRCharacterBase* Who : WaAround(C, At + FVector(0, 0, 60.f), Ring, 200.f))
		{
			const bool bDirect = FVector::Dist2D(Who->GetActorLocation(), At) <= HitRadius + Who->GetSimpleCollisionRadius();
			const bool bFrozen = Who->HasTag(WRTags::Status_Frozen);
			// Avalanche plunge (Wind + Frost 50): the ring shatters what's frozen (a heavy blow on a frozen body) and chills the rest.
			WaWeaponBlow(C, Who, (bDirect ? T.Num(TEXT("wildbrands.plunge.dmg"), 1.5f) : T.Num(TEXT("spells.plunge.ringDamage"), 0.8f)) * Height * Scale,
				bDirect ? T.Num(TEXT("wildbrands.plunge.stance"), 4.f) : 1.f, bDirect || (bAvalanche && bFrozen), At, TEXT("plunge"));
			if (bAvalanche && !bFrozen && Who->IsAlive())
			{
				Who->Status->AddBuildup(WRTags::Element_Frost, FWRComboRegistry::Get().Num(TEXT("avalanche_plunge"), TEXT("frost"), 50.f), C);
			}
		}
		UE_LOG(LogWildreach, Log, TEXT("WIND plunge by %s lands from %.1f m (x%.2f)"), *C->GetName(), Metres, Height);
	}
	C->PlaySwing(EWRSwingKind::Heavy, 0.4f, 2);
	After(T.Num(TEXT("wildbrands.plunge.recovery"), 0.45f), [this]() { EndSelf(); });
}

void UWRGA_Plunge::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (AWRCharacterBase* C = GetCharacter())
	{
		C->SetCombatTag(WRTags::State_Hyperarmor, false);
	}
	Travel = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// ---------------------------------------------------------------- Whirlwind

UWRGA_Whirlwind::UWRGA_Whirlwind()
{
	DisplayName = TEXT("Whirlwind"); PathId = TEXT("wind"); NodeId = TEXT("whirlwind");
	StaminaKey = TEXT("stamina.weaponArt"); StaminaFallback = 24.f;
	CooldownKey = TEXT("spells.whirlwind.cooldown"); CooldownFallback = 8.f;
	ActivationOwnedTags.AddTag(WRTags::State_Attacking);
	FGameplayTagContainer Blocked = ActivationBlockedTags;
	Blocked.AddTag(WRTags::State_Attacking);
	Blocked.AddTag(WRTags::State_Dodging);
	ActivationBlockedTags = Blocked;
}

bool UWRGA_Whirlwind::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	const AWRCharacterBase* C = ActorInfo ? Cast<AWRCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	FString Why;
	return C && UWRGA_Melee::WildbrandFits(C, NodeId, Why);
}

void UWRGA_Whirlwind::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	const FWRTuning& T = FWRTuning::Get();
	CostOverride = T.Num(TEXT("wildbrands.whirlwind.stamina"), 24.f);
	AWRCharacterBase* C = GetCharacter();
	if (!C || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	bHeld = true;
	bSpinning = false;
	ExtraSpins = 0;
	const float Cost = T.Num(TEXT("spells.whirlwind.momentumCost"), 30.f);
	Scale = WaPredict(C, Cost, false);
	if (IsServer())
	{
		WRWind::PayMomentum(C, Cost, false);
		WRFx::BroadcastOn(C, TEXT("wind.weapon"), TEXT("weapon"), 1.3f, TEXT("wind.whirl_tell"), 0.4f);
	}
	// Hyperarmor from the wind-up through the last spin; the legs keep walking slowly.
	C->SetCombatTag(WRTags::State_Hyperarmor, true);
	SetMoveSlow(true, true);
	const float Windup = T.Num(TEXT("wildbrands.whirlwind.windup"), 0.3f);
	C->PlaySwing(EWRSwingKind::WeaponArt, Windup + 0.1f, 0);
	if (UAbilityTask_WaitInputRelease* Wait = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true))
	{
		Wait->OnRelease.AddDynamic(this, &UWRGA_Whirlwind::OnReleased);
		Wait->ReadyForActivation();
	}
	After(Windup, [this]()
	{
		if (AWRCharacterBase* Ch = GetCharacter())
		{
			bSpinning = true;
			Ch->SetCombatTag(WRWind::State_Spinning, true);
			Spin(0);
		}
	});
}

void UWRGA_Whirlwind::OnReleased(float TimeHeld)
{
	bHeld = false;
}

void UWRGA_Whirlwind::Spin(int32 Index)
{
	AWRCharacterBase* C = GetCharacter();
	if (!C || !IsActive())
	{
		return;
	}
	const FWRTuning& T = FWRTuning::Get();
	const float SpinSeconds = T.Num(TEXT("spells.whirlwind.spinSeconds"), 0.3f);
	const float Radius = T.Num(TEXT("spells.whirlwind.radius"), 280.f);
	C->PlaySwing(EWRSwingKind::WeaponArt, SpinSeconds, uint8(1 + Index % 2));
	if (IsServer())
	{
		const FVector Centre = C->GetActorLocation();
		WRFx::Broadcast(C, TEXT("wind.whirl"), WaFeet(C), FRotator(0.f, C->GetActorRotation().Yaw + Index * 90.f, 0.f), Radius / 100.f, TEXT("wind.whirl"));
		for (AWRCharacterBase* Who : WaAround(C, Centre, Radius, 200.f))
		{
			WaWeaponBlow(C, Who, T.Num(TEXT("wildbrands.whirlwind.dmg"), 0.7f) * Scale, T.Num(TEXT("wildbrands.whirlwind.stance"), 1.f), false, Centre, TEXT("whirlwind"));
		}
		// Debris: a stone a rank thrown out of each spin in a fixed pattern (front, back, the sides), turning a quarter
		// further each spin so the stones sweep the ground round you.
		const int32 Stones = FMath::RoundToInt(UWRPathComponent::Stat(C, TEXT("debrisStones")));
		if (Stones > 0)
		{
			static const float Pattern[8] = { 0.f, 180.f, 90.f, 270.f, 45.f, 225.f, 135.f, 315.f };
			const float Speed = FMath::Min(T.Num(TEXT("passives.wind.debrisSpeed"), 1400.f), 1400.f);
			for (int32 k = 0; k < FMath::Min(Stones, 8); ++k)
			{
				const FVector Dir = FRotator(T.Num(TEXT("passives.wind.debrisPitch"), 6.f), C->GetActorRotation().Yaw + Pattern[k] + Index * 22.5f, 0.f).Vector();
				FWRHit Stone;
				Stone.Instigator = C;
				Stone.Damage = T.Num(TEXT("passives.wind.debrisDamage"), 8.f) * Scale;
				Stone.PoiseDamage = T.Num(TEXT("passives.wind.debrisPoise"), 10.f);
				Stone.StanceDamage = Stone.Damage * WRTune(TEXT("stance.spellPerDamage"), 0.3f) * T.Num(TEXT("passives.wind.debrisStance"), 1.f);
				Stone.Element = WRWind::Element_Wind;
				Stone.PathId = WaWindPath;
				Stone.PhysType = 2;
				Stone.bProjectile = true;
				WaShoot(C, Stone, Centre + Dir * (Radius * 0.5f) + FVector(0, 0, 30.f), Dir * Speed, T.Num(TEXT("passives.wind.debrisGravity"), 0.5f), TEXT("code:riftstone"), FString(), TEXT("debris"));
			}
		}
		// Lingering gale: the spin leaves a gale on the ground where it turned.
		if (UWRPathComponent::HasNode(C, WaWindPath, TEXT("lingering_gale")))
		{
			AWRWindPatch::Lay(C, WaFeet(C), T.Num(TEXT("passives.wind.galeRadius"), 300.f), T.Num(TEXT("passives.wind.galeSeconds"), 4.f),
				T.Num(TEXT("passives.wind.galeDps"), 5.f), T.Num(TEXT("passives.wind.galePush"), 100.f), 0.f);
		}
	}
	After(SpinSeconds, [this, Index]()
	{
		AWRCharacterBase* Ch = GetCharacter();
		if (!Ch)
		{
			return;
		}
		const FWRTuning& Tt = FWRTuning::Get();
		const int32 Base = FMath::RoundToInt(Tt.Num(TEXT("spells.whirlwind.spins"), 4.f));
		if (Index + 1 < Base)
		{
			Spin(Index + 1);
			return;
		}
		// Held: one more spin for each 8 Momentum, while there's Momentum to pay (the same view on both machines).
		const float HoldCost = Tt.Num(TEXT("spells.whirlwind.holdMomentum"), 8.f);
		if (bHeld && ExtraSpins < FMath::RoundToInt(Tt.Num(TEXT("spells.whirlwind.maxExtraSpins"), 6.f)) && WRWind::MomentumValue(Ch) >= HoldCost)
		{
			if (IsServer())
			{
				WRWind::SpendMomentum(Ch, HoldCost);
			}
			++ExtraSpins;
			Spin(Index + 1);
			return;
		}
		Finish();
	});
}

void UWRGA_Whirlwind::Finish()
{
	AWRCharacterBase* C = GetCharacter();
	if (!C)
	{
		return;
	}
	bSpinning = false;
	C->SetCombatTag(WRWind::State_Spinning, false);
	C->SetCombatTag(WRTags::State_Hyperarmor, false);
	SetMoveRoot(true);
	After(FWRTuning::Get().Num(TEXT("wildbrands.whirlwind.recovery"), 0.5f), [this]() { EndSelf(); });
}

void UWRGA_Whirlwind::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (AWRCharacterBase* C = GetCharacter())
	{
		C->SetCombatTag(WRWind::State_Spinning, false);
		C->SetCombatTag(WRTags::State_Hyperarmor, false);
	}
	bSpinning = false;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// ---------------------------------------------------------------- Wind arrow

UWRGA_WindArrow::UWRGA_WindArrow()
{
	DisplayName = TEXT("Wind arrow"); PathId = TEXT("wind"); NodeId = TEXT("wind_arrow");
	StaminaKey = TEXT("stamina.weaponArt"); StaminaFallback = 14.f;
	CooldownKey = TEXT("spells.windArrow.cooldown"); CooldownFallback = 2.f;
	ActivationOwnedTags.AddTag(WRTags::State_Attacking);
	FGameplayTagContainer Blocked = ActivationBlockedTags;
	Blocked.AddTag(WRTags::State_Attacking);
	Blocked.AddTag(WRTags::State_Dodging);
	ActivationBlockedTags = Blocked;
	bAllowedMounted = true;   // bows from the saddle (combat.md 2.19)
}

bool UWRGA_WindArrow::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	// Through the bow or crossbow in your hands, with an arrow or a bolt to loose (combat.md 2.12 fits).
	const AWRCharacter* S = ActorInfo ? Cast<AWRCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const FWRItemDef* Bow = WaBowOf(S);
	const UWRInventoryComponent* Inv = S ? UWRInventoryComponent::FindFor(S) : nullptr;
	return Bow && S->IsRangedOut() && Inv && !Inv->FindAmmo(Bow->AmmoKind, S->GetSelectedAmmo()).IsNone();
}

void UWRGA_WindArrow::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	const FWRTuning& T = FWRTuning::Get();
	CostOverride = T.Num(TEXT("wildbrands.wind_arrow.stamina"), 14.f);
	AWRCharacterBase* C = GetCharacter();
	if (!C || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	// Draw (Swift draw shortens it), loose, and the 0.30 s after.
	const float Draw = T.Num(TEXT("wildbrands.wind_arrow.windup"), 0.35f) * WRWind::DrawTimeScale(C);
	const float After_ = T.Num(TEXT("wildbrands.wind_arrow.recovery"), 0.3f);
	SetMoveSlow(true, true);
	if (IsServer())
	{
		WRSound::Broadcast(C, TEXT("bow.draw"));
		WRFx::BroadcastOn(C, TEXT("cast.wind"), TEXT("hand"), 0.8f, TEXT("wind.arrow_draw"), Draw);
	}
	After(Draw, [this]() { Release(); });
	After(Draw + After_, [this]() { EndSelf(); });
}

void UWRGA_WindArrow::Release()
{
	AWRCharacter* S = Cast<AWRCharacter>(GetCharacter());
	if (!S || !S->IsAlive() || !IsServer())
	{
		return;
	}
	const FWRTuning& T = FWRTuning::Get();
	const FWRItemDef* Bow = WaBowOf(S);
	UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(S);
	const FName AmmoId = Bow && Inv ? Inv->FindAmmo(Bow->AmmoKind, S->GetSelectedAmmo()) : NAME_None;
	if (!Bow || AmmoId.IsNone())
	{
		return;
	}
	Inv->RemoveItem(AmmoId, 1);
	const FWRItemDef* Ammo = FWRItemRegistry::Get().Find(AmmoId);
	const float Cost = T.Num(TEXT("spells.windArrow.momentumCost"), 15.f);
	const float Scale = WaPredict(S, Cost, false);
	WRWind::PayMomentum(S, Cost, false);
	// The bow's own release and reload on every machine; the next plain shot waits the bow's reload.
	const float Reload = Bow->Reload * WRWind::DrawTimeScale(S);
	S->PlayAbilityShot(Reload);
	S->HoldNextShot(GetWorld()->GetTimeSeconds() + Reload);
	WRSound::Broadcast(S, Bow->RangedKind == TEXT("bow") ? FName(TEXT("bow.release")) : FName(TEXT("crossbow.fire")));
	WRSound::Broadcast(S, TEXT("wind.arrow"));

	FWRHit Hit;
	Hit.Instigator = S;
	Hit.Damage = (Bow->RangedDamage * Inv->GetRangedMultiplier() + (Ammo ? Ammo->AmmoDamage : 0.f)) * T.Num(TEXT("wildbrands.wind_arrow.dmg"), 1.6f) * Scale * WRWind::ProjectileDamageScale(S);
	Hit.PoiseDamage = Hit.Damage * WRTune(TEXT("ranged.poisePerDamage"), 0.4f);
	Hit.StanceDamage = Hit.Damage * WRTune(TEXT("stance.spellPerDamage"), 0.3f) * T.Num(TEXT("wildbrands.wind_arrow.stance"), 1.5f);
	Hit.Element = WRWind::Element_Wind;
	Hit.PathId = WaWindPath;
	Hit.PhysType = 3;
	Hit.bWeaponArt = true;
	Hit.bProjectile = true;
	if (Ammo) WRSwamp::AddAmmoBuildups(Ammo->Id, Hit);
	// Ice arrows (Wind + Frost 25) freeze; Bleeding arrows (Wind + Blood 25) bleed.
	const FWRComboRegistry& R = FWRComboRegistry::Get();
	if (S->HasTag(WRWind::Combo_IceArrows)) Hit.FrostBuildup += R.Num(TEXT("ice_arrows"), TEXT("frost"), 60.f);
	if (S->HasTag(WRWind::Combo_BleedingArrows)) Hit.Buildups.FindOrAdd(TEXT("bleed")) += R.Num(TEXT("bleeding_arrows"), TEXT("bleed"), 50.f);
	const float Knock = T.Num(TEXT("spells.windArrow.knockback"), 300.f) * Scale;
	const FVector Chest = S->GetActorLocation() + FVector(0.f, 0.f, 55.f);
	FVector Dir = WaThroughCrosshair(S, Chest);
	if (const AWRCharacterBase* Lock = WaLocked(S))
	{
		Dir = (Lock->GetActorLocation() - Chest).GetSafeNormal();
	}
	TWeakObjectPtr<AWRCharacterBase> WeakS(S);
	if (S->HasTag(WRWind::Combo_Sunshot))
	{
		// Sunshot (Sun & sand + Wind 50): the arrow becomes a beam of light, there at once, through everything in line.
		const float Range = R.Num(TEXT("sunshot"), TEXT("range"), 2500.f);
		Hit.Buildups.FindOrAdd(TEXT("blind")) += R.Num(TEXT("sunshot"), TEXT("blind"), 50.f);
		FHitResult Wall;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(WRSunshot), false, S);
		const FVector End = GetWorld()->LineTraceSingleByChannel(Wall, Chest, Chest + Dir * Range, ECC_WorldStatic, Query) ? FVector(Wall.ImpactPoint) : Chest + Dir * Range;
		for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
		{
			if (*It == S || !It->IsAlive() || !S->IsHostileTo(*It)) continue;
			const FVector P = FMath::ClosestPointOnSegment(It->GetActorLocation(), Chest, End);
			if (FVector::Dist(P, It->GetActorLocation()) > It->GetSimpleCollisionRadius() + 30.f) continue;
			FWRHit Beam = Hit;
			Beam.Origin = Chest;
			Beam.ImpactPoint = P;
			TWeakObjectPtr<AWRCharacterBase> WeakT(*It);
			UWRCombatLibrary::ApplyHit(*It, Beam, [WeakS, WeakT, Dir, Knock](EWRHitResult Result)
			{
				if (Result == EWRHitResult::Hit && WeakS.IsValid() && WeakT.IsValid()) WRWind::Knock(WeakS.Get(), WeakT.Get(), Dir, Knock, 0.25f, TEXT("wind_arrow"));
			});
		}
		WRFx::Broadcast(S, TEXT("wind.beam"), Chest, Dir.Rotation(), FVector::Dist(Chest, End) / 100.f, TEXT("wind.beam"));
	}
	else
	{
		const float Speed = Bow->ProjectileSpeed * WRWind::ProjectileSpeedScale(S);
		if (AWRProjectile* P = WaShoot(S, Hit, Chest + Dir * 45.f, Dir * Speed, Bow->ProjectileGravity, Ammo && !Ammo->ProjectileMesh.IsEmpty() ? Ammo->ProjectileMesh : FString(),
			Ammo ? Ammo->MeshAlong : FString(), Bow->AmmoKind))
		{
			// Through one body (Piercing gale: two), knocking back each it meets.
			P->PierceLeft = FMath::RoundToInt(T.Num(TEXT("spells.windArrow.pierce"), 1.f)) + WRWind::ShotPierce(S, AmmoId);
			P->OnStruck = [WeakS, Dir, Knock](AWRCharacterBase* Target, EWRHitResult Result)
			{
				if (Result == EWRHitResult::Hit && WeakS.IsValid()) WRWind::Knock(WeakS.Get(), Target, Dir, Knock, 0.25f, TEXT("wind_arrow"));
			};
			P->SetTrail(TEXT("wind.arrow_trail"));
		}
	}
	// Tailwind shot: a Wind arrow always counts as fully drawn.
	WRWind::OnShotFired(S, nullptr, true);
	UE_LOG(LogWildreach, Log, TEXT("WIND arrow by %s (%s, x%.2f)"), *S->GetName(), *AmmoId.ToString(), Scale);
}

// ---------------------------------------------------------------- Sky lord

UWRGA_SkyLord::UWRGA_SkyLord()
{
	DisplayName = TEXT("Sky lord"); PathId = TEXT("wind"); NodeId = TEXT("sky_lord");
	StaminaKey = TEXT("stamina.spell"); StaminaFallback = 12.f;
	CooldownKey = TEXT("spells.skyLord.cooldown"); CooldownFallback = 90.f;
	FGameplayTagContainer Blocked = ActivationBlockedTags;
	Blocked.AddTag(WRTags::State_Attacking);
	Blocked.AddTag(WRTags::State_Dodging);
	Blocked.AddTag(WRWind::State_SkyLord);
	ActivationBlockedTags = Blocked;
}

void UWRGA_SkyLord::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AWRCharacterBase* C = GetCharacter();
	if (!C || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	const FWRTuning& T = FWRTuning::Get();
	const float Cost = T.Num(TEXT("spells.skyLord.momentumCost"), 60.f);
	const float Scale = WaPredict(C, Cost, false);
	if (IsServer())
	{
		WRWind::PayMomentum(C, Cost, false);
		WRFx::BroadcastOn(C, TEXT("cast.wind"), TEXT("hand"), 1.6f, TEXT("wind.skylord_cast"), 1.1f);
	}
	// The transform: 1.0 s rooted with hyperarmor (combat.md 2.13).
	const float CastSeconds = T.Num(TEXT("spells.skyLord.cast"), 1.f);
	SetMoveRoot(true);
	C->SetCombatTag(WRTags::State_Hyperarmor, true);
	C->SetCombatTag(WRTags::State_Casting, true);
	C->PlaySwing(EWRSwingKind::Cast, CastSeconds);
	After(CastSeconds, [this, Scale]()
	{
		AWRCharacterBase* Ch = GetCharacter();
		if (!Ch || !Ch->IsAlive())
		{
			EndSelf(true);
			return;
		}
		const FWRTuning& Tt = FWRTuning::Get();
		SetMoveRoot(false);
		Ch->SetCombatTag(WRTags::State_Hyperarmor, false);
		Ch->SetCombatTag(WRTags::State_Casting, false);
		Ch->SetCombatTag(WRWind::State_SkyLord, true);
		bHovering = true;
		// Hover: flying at 600 cm/s; you rise to 6 m over the ground, or hold the height you're at in the air.
		UCharacterMovementComponent* Move = Ch->GetCharacterMovement();
		SavedFlySpeed = Move->MaxFlySpeed;
		SavedBraking = Move->BrakingDecelerationFlying;
		Move->MaxFlySpeed = Tt.Num(TEXT("spells.skyLord.speed"), 600.f);
		Move->BrakingDecelerationFlying = Tt.Num(TEXT("spells.skyLord.braking"), 1400.f);
		Move->SetMovementMode(MOVE_Flying);
		const float Ground = Ch->GetActorLocation().Z - WaHeight(Ch, 5000.f);
		HoverZ = FMath::Max(Ch->GetActorLocation().Z, Ground + Tt.Num(TEXT("spells.skyLord.hoverHeight"), 600.f) + Ch->GetSimpleCollisionHalfHeight());
		const float Seconds = Tt.Num(TEXT("spells.skyLord.seconds"), 8.f) * Scale;
		EndsAt = GetWorld()->GetTimeSeconds() + Seconds;
		if (IsServer())
		{
			WRFx::BroadcastOn(Ch, TEXT("wind.skylord"), NAME_None, 1.f, TEXT("wind.skylord"), Seconds);
			UE_LOG(LogWildreach, Log, TEXT("WIND sky lord on %s for %.1f s"), *Ch->GetName(), Seconds);
		}
		Hover();
	});
}

void UWRGA_SkyLord::Hover()
{
	AWRCharacterBase* C = GetCharacter();
	if (!C || !bHovering || !IsActive())
	{
		return;
	}
	if (!C->IsAlive() || GetWorld()->GetTimeSeconds() >= EndsAt)
	{
		EndSelf();
		return;
	}
	const FWRTuning& T = FWRTuning::Get();
	const float Step = 0.03f;
	UCharacterMovementComponent* Move = C->GetCharacterMovement();
	if (!Move->IsFlying())
	{
		Move->SetMovementMode(MOVE_Flying);
	}
	const float HalfH = C->GetSimpleCollisionHalfHeight();
	const float Ground = C->GetActorLocation().Z - WaHeight(C, 5000.f) + HalfH;
	const float Rise = T.Num(TEXT("spells.skyLord.riseSpeed"), 500.f);
	// Hold Jump to climb (to 20 m over the ground); let go and you settle back toward 6 m over whatever is under you.
	if (C->bPressedJump)
	{
		HoverZ = FMath::Min(HoverZ + Rise * Step, Ground + T.Num(TEXT("spells.skyLord.maxHeight"), 2000.f));
	}
	else
	{
		HoverZ = FMath::FInterpConstantTo(HoverZ, Ground + T.Num(TEXT("spells.skyLord.hoverHeight"), 600.f), Step, Rise * 0.5f);
	}
	HoverZ = FMath::Clamp(HoverZ, Ground + T.Num(TEXT("spells.skyLord.minHeight"), 200.f), Ground + T.Num(TEXT("spells.skyLord.maxHeight"), 2000.f));
	// A strike (the heavy's dive) owns the height while it's out.
	if (!C->HasTag(WRTags::State_Attacking))
	{
		Move->Velocity.Z = FMath::Clamp((HoverZ - C->GetActorLocation().Z) * T.Num(TEXT("spells.skyLord.heightGain"), 4.f), -Rise, Rise);
	}
	After(Step, [this]() { Hover(); });
}

void UWRGA_SkyLord::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (AWRCharacterBase* C = GetCharacter())
	{
		C->SetCombatTag(WRTags::State_Hyperarmor, false);
		C->SetCombatTag(WRTags::State_Casting, false);
		if (bHovering)
		{
			bHovering = false;
			C->SetCombatTag(WRWind::State_SkyLord, false);
			UCharacterMovementComponent* Move = C->GetCharacterMovement();
			Move->MaxFlySpeed = SavedFlySpeed;
			Move->BrakingDecelerationFlying = SavedBraking;
			if (Move->IsFlying())
			{
				Move->SetMovementMode(MOVE_Falling);
			}
			// The fall from Sky lord's end is always free (cooks/biome-sky.md 2.18.3).
			WRWind::CoverFall(C);
		}
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

/**
 * Sky lord's air strikes (the Light / Heavy inputs while it hovers): a two-hit wind slash (x1.0 each, 320 cm, the front
 * 120 degrees) or a diving strike (x1.6 within 250 cm of where it comes down) that returns you to hover height.
 * Declared in WRAbilities.h (UWRGA_Melee); here with the rest of the Wind path.
 */
void UWRGA_Melee::SkyLordSwing()
{
	AWRCharacterBase* C = GetCharacter();
	if (!C)
	{
		EndSelf(true);
		return;
	}
	const FWRTuning& T = FWRTuning::Get();
	const auto Slash = [this](float Mult)
	{
		AWRCharacterBase* Ch = GetCharacter();
		if (!Ch || !IsServer() || !Ch->IsAlive())
		{
			return;
		}
		const FWRTuning& Tt = FWRTuning::Get();
		const float Reach = Tt.Num(TEXT("spells.skyLord.lightReach"), 320.f);
		const float CosCone = FMath::Cos(FMath::DegreesToRadians(Tt.Num(TEXT("spells.skyLord.lightCone"), 120.f) * 0.5f));
		const FVector Aim = Ch->GetBaseAimRotation().Vector();
		const FVector From = Ch->GetActorLocation();
		WRFx::Broadcast(Ch, TEXT("wind.slash"), From + Aim * 120.f, Aim.Rotation(), 1.f, TEXT("wind.cut"));
		for (TActorIterator<AWRCharacterBase> It(Ch->GetWorld()); It; ++It)
		{
			if (*It == Ch || !It->IsAlive() || !Ch->IsHostileTo(*It)) continue;
			const FVector To = It->GetActorLocation() - From;
			if (To.Size() > Reach + It->GetSimpleCollisionRadius() || FVector::DotProduct(To.GetSafeNormal(), Aim) < CosCone) continue;
			WRWindAbil::WaWeaponBlow(Ch, *It, Mult, Tt.Num(TEXT("spells.skyLord.lightStance"), 1.2f), false, From, TEXT("sky_lord"));
		}
	};
	if (!bHeavy)
	{
		const float First = T.Num(TEXT("spells.skyLord.lightFirst"), 0.18f);
		const float Second = T.Num(TEXT("spells.skyLord.lightSecond"), 0.4f);
		const float End = T.Num(TEXT("spells.skyLord.lightEnd"), 0.62f);
		const float Mult = T.Num(TEXT("spells.skyLord.lightDamage"), 1.f);
		C->PlaySwing(EWRSwingKind::Light, End, 0, First, Second - First, End - Second);
		C->SetCombatTag(WRTags::State_Attacking, true);
		After(First, [Slash, Mult]() { Slash(Mult); });
		After(Second, [Slash, Mult]() { Slash(Mult); });
		After(End, [this]() { if (AWRCharacterBase* Ch = GetCharacter()) Ch->SetCombatTag(WRTags::State_Attacking, false); EndSelf(); });
		return;
	}
	// The diving strike: a short coil, down at the lock (or where the crosshair meets the ground), the blow, back up.
	const float Coil = T.Num(TEXT("spells.skyLord.heavyWindup"), 0.3f);
	const float Dive = T.Num(TEXT("spells.skyLord.heavyDive"), 0.35f);
	const float End = T.Num(TEXT("spells.skyLord.heavyEnd"), 1.1f);
	C->PlaySwing(EWRSwingKind::Heavy, Coil + Dive, 0, Coil, Dive, End - Coil - Dive);
	C->SetCombatTag(WRTags::State_Attacking, true);
	C->SetCombatTag(WRTags::State_Hyperarmor, true);
	After(Coil, [this, Dive]()
	{
		AWRCharacterBase* Ch = GetCharacter();
		if (!Ch)
		{
			return;
		}
		FVector Goal;
		const AWRCharacter* Survivor = Cast<AWRCharacter>(Ch);
		if (const AWRCharacterBase* Lock = WRWindAbil::WaLocked(Ch))
		{
			Goal = Lock->GetActorLocation();
		}
		else if (!Survivor || !Survivor->CrosshairPoint(2500.f, true, Goal))
		{
			Goal = Ch->GetActorLocation() + FVector(0.f, 0.f, -600.f) + Ch->GetActorForwardVector() * 400.f;
		}
		const FVector Way = Goal - Ch->GetActorLocation();
		const float Speed = FMath::Min(FWRTuning::Get().Num(TEXT("spells.skyLord.diveSpeed"), 2400.f), Way.Size() / FMath::Max(0.05f, Dive));
		Ch->GetCharacterMovement()->Velocity = Way.GetSafeNormal() * Speed;
		if (IsServer())
		{
			WRFx::BroadcastOn(Ch, TEXT("wind.plunge"), NAME_None, 1.f, TEXT("wind.plunge_dive"), Dive);
		}
	});
	After(Coil + Dive, [this]()
	{
		AWRCharacterBase* Ch = GetCharacter();
		if (!Ch)
		{
			return;
		}
		Ch->GetCharacterMovement()->Velocity = FVector::ZeroVector;
		Ch->SetCombatTag(WRTags::State_Hyperarmor, false);
		if (!IsServer())
		{
			return;
		}
		const FWRTuning& Tt = FWRTuning::Get();
		const float Radius = Tt.Num(TEXT("spells.skyLord.heavyRadius"), 250.f);
		const FVector At = Ch->GetActorLocation();
		WRFx::Broadcast(Ch, TEXT("wind.plunge_ring"), WRWindAbil::WaGround(Ch->GetWorld(), At, Ch, 100.f, 1200.f), FRotator::ZeroRotator, Radius / 100.f, TEXT("wind.plunge_land"));
		for (AWRCharacterBase* Who : WRWindAbil::WaAround(Ch, At, Radius, 400.f))
		{
			WRWindAbil::WaWeaponBlow(Ch, Who, Tt.Num(TEXT("spells.skyLord.heavyDamage"), 1.6f), Tt.Num(TEXT("spells.skyLord.heavyStance"), 3.f), true, At, TEXT("sky_lord"));
		}
	});
	After(End, [this]() { if (AWRCharacterBase* Ch = GetCharacter()) Ch->SetCombatTag(WRTags::State_Attacking, false); EndSelf(); });
}

// ---------------------------------------------------------------- Tornado

UWRGA_Tornado::UWRGA_Tornado()
{
	DisplayName = TEXT("Tornado"); PathId = TEXT("wind"); NodeId = TEXT("tornado");
	StaminaKey = TEXT("stamina.spell"); StaminaFallback = 12.f;
	CooldownKey = TEXT("spells.tornado.cooldown"); CooldownFallback = 60.f;
	FGameplayTagContainer Blocked = ActivationBlockedTags;
	Blocked.AddTag(WRTags::State_Attacking);
	Blocked.AddTag(WRTags::State_Dodging);
	ActivationBlockedTags = Blocked;
}

void UWRGA_Tornado::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AWRCharacterBase* C = GetCharacter();
	if (!C || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	bReleased = false;
	const FWRTuning& T = FWRTuning::Get();
	const float Cost = T.Num(TEXT("spells.tornado.momentumCost"), 50.f);
	const float Scale = WaPredict(C, Cost, false);
	if (IsServer())
	{
		WRWind::PayMomentum(C, Cost, false);
		WRFx::BroadcastOn(C, TEXT("cast.wind"), TEXT("hand"), 1.4f, TEXT("wind.cast"), 0.7f);
	}
	const float Windup = T.Num(TEXT("spells.tornado.windup"), T.Num(TEXT("spells.shapes.summon.windup"), 0.55f));
	const float Recovery = T.Num(TEXT("spells.tornado.recovery"), T.Num(TEXT("spells.shapes.summon.recovery"), 0.4f));
	SetMoveRoot(true);
	C->SetCombatTag(WRTags::State_Casting, true);
	C->PlaySwing(EWRSwingKind::Cast, Windup + 0.1f);
	// Hold the slot to steer it at the crosshair; let go and it runs straight (the slot is the hold: no bow, no Aim).
	if (UAbilityTask_WaitInputRelease* Wait = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true))
	{
		Wait->OnRelease.AddDynamic(this, &UWRGA_Tornado::OnReleased);
		Wait->ReadyForActivation();
	}
	After(Windup, [this, Scale]()
	{
		AWRCharacterBase* Ch = GetCharacter();
		if (!Ch)
		{
			return;
		}
		Ch->SetCombatTag(WRTags::State_Casting, false);
		if (!IsServer() || !Ch->IsAlive())
		{
			return;
		}
		const FWRTuning& Tt = FWRTuning::Get();
		const FVector Ahead = FRotator(0.f, Ch->GetBaseAimRotation().Yaw, 0.f).Vector();
		const FVector At = WRWindAbil::WaGround(Ch->GetWorld(), Ch->GetActorLocation() + Ahead * Tt.Num(TEXT("spells.tornado.ahead"), 300.f), Ch, 400.f, 2000.f);
		FActorSpawnParameters Params;
		Params.Owner = Ch;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AWRWindTornado* Tor = Ch->GetWorld()->SpawnActor<AWRWindTornado>(AWRWindTornado::StaticClass(), At, Ahead.Rotation(), Params))
		{
			Tor->Caster = Ch;
			Tor->Heading = Ahead;
			Tor->bSteering = !bReleased;
			Tor->Speed = Tt.Num(TEXT("spells.tornado.speed"), 400.f);
			Tor->Dps = Tt.Num(TEXT("spells.tornado.dps"), 10.f) * Scale;
			Tor->Radius = Tt.Num(TEXT("spells.tornado.radius"), 250.f);
			Tor->SetLifeSpan(Tt.Num(TEXT("spells.tornado.seconds"), 10.f));
			WRWind::SetTornado(Ch, Tor, Tor->Radius);
			Spawned = Tor;
			WRFx::Broadcast(Ch, TEXT("wind.vortex"), At, FRotator::ZeroRotator, Tor->Radius / 100.f, TEXT("wind.tornado_start"));
		}
	});
	After(Windup + Recovery, [this]()
	{
		SetMoveRoot(false);
		// It lives on while you hold to steer (or until it blows out).
		PollHold();
	});
}

void UWRGA_Tornado::PollHold()
{
	if (!IsActive())
	{
		return;
	}
	const AWRCharacterBase* C = GetCharacter();
	if (bReleased || (IsServer() && !Spawned.IsValid()) || !C || !C->IsAlive())
	{
		EndSelf();
		return;
	}
	After(0.1f, [this]() { PollHold(); });
}

void UWRGA_Tornado::OnReleased(float TimeHeld)
{
	bReleased = true;
	if (AWRWindTornado* Tor = Spawned.Get())
	{
		Tor->bSteering = false;
	}
}

void UWRGA_Tornado::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (AWRWindTornado* Tor = Spawned.Get())
	{
		Tor->bSteering = false;
	}
	if (AWRCharacterBase* C = GetCharacter())
	{
		C->SetCombatTag(WRTags::State_Casting, false);
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// ---------------------------------------------------------------- the wind wave

AWRWindWave::AWRWindWave()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;   // server only: what players see is the broadcast streak, which flies the same way
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

AWRWindWave* AWRWindWave::Launch(AWRCharacterBase* InBy, const FVector& From, const FVector& InDir, float InDamage, float InStance, float InKnock, float InSpeed, float InRange, float InWidth)
{
	if (!InBy || !InBy->HasAuthority())
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWRWindWave* W = InBy->GetWorld()->SpawnActor<AWRWindWave>(AWRWindWave::StaticClass(), From, InDir.Rotation(), Params);
	if (W)
	{
		W->By = InBy;
		W->Dir = InDir.GetSafeNormal2D();
		W->Damage = InDamage;
		W->Stance = InStance;
		W->Knock = InKnock;
		W->Speed = InSpeed;
		W->Range = InRange;
		W->Width = InWidth;
		W->SetLifeSpan(InRange / FMath::Max(1.f, InSpeed) + 0.2f);
		WRFx::Broadcast(InBy, TEXT("wind.wave"), From, W->Dir.Rotation(), InRange / 900.f, TEXT("wind.wave"));
	}
	return W;
}

void AWRWindWave::BeginPlay()
{
	Super::BeginPlay();
}

void AWRWindWave::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AWRCharacterBase* C = By.Get();
	if (!C || Travelled >= Range)
	{
		Destroy();
		return;
	}
	const FVector From = GetActorLocation();
	const float StepLen = FMath::Min(Speed * DeltaSeconds, Range - Travelled);
	FVector To = From + Dir * StepLen;
	// Walls stop it (after what it struck on the way).
	FHitResult Wall;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(WRWindWave), false, C);
	const bool bWall = GetWorld()->LineTraceSingleByChannel(Wall, From, To, ECC_WorldStatic, Query);
	if (bWall) To = Wall.ImpactPoint;
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		AWRCharacterBase* Who = *It;
		if (Who == C || !Who->IsAlive() || !C->IsHostileTo(Who) || Struck.Contains(Who)) continue;
		const FVector P = FMath::ClosestPointOnSegment(Who->GetActorLocation(), From, To);
		if (FVector::Dist2D(P, Who->GetActorLocation()) > Width * 0.5f + Who->GetSimpleCollisionRadius() || FMath::Abs(P.Z - Who->GetActorLocation().Z) > Who->GetSimpleCollisionHalfHeight() + 120.f) continue;
		Struck.Add(Who);
		FWRHit Hit;
		Hit.Instigator = C;
		Hit.Damage = Damage;
		Hit.PoiseDamage = WRTune(TEXT("spells.windCut.wavePoise"), 20.f);
		Hit.StanceDamage = Damage * WRTune(TEXT("stance.spellPerDamage"), 0.3f) * Stance;
		Hit.Element = WRWind::Element_Wind;
		Hit.PathId = TEXT("wind");
		Hit.PhysType = 1;
		Hit.bWeaponArt = true;
		Hit.bProjectile = true;
		Hit.Origin = From - Dir * 100.f;
		Hit.ImpactPoint = P;
		TWeakObjectPtr<AWRCharacterBase> WeakC(C), WeakT(Who);
		const FVector D = Dir;
		const float K = Knock;
		UWRCombatLibrary::ApplyHit(Who, Hit, [WeakC, WeakT, D, K](EWRHitResult Result)
		{
			if (Result == EWRHitResult::Hit && WeakC.IsValid() && WeakT.IsValid()) WRWind::Knock(WeakC.Get(), WeakT.Get(), D, K, 0.25f, TEXT("wind_cut"));
		});
	}
	Travelled += FVector::Dist(From, To);
	SetActorLocation(To);
	if (bWall)
	{
		WRFx::Broadcast(C, TEXT("hit.dust"), Wall.ImpactPoint, Wall.ImpactNormal.Rotation(), 0.8f, TEXT("wind.slam"));
		Destroy();
	}
}

// ---------------------------------------------------------------- the tornado

AWRWindTornado::AWRWindTornado()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRWindTornado::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() != NM_DedicatedServer)
	{
		// The funnel: a grey-white column of dust and torn air, darker at its foot (Data/fx.json wind.tornado).
		Funnel = WRFx::Attach(GetRootComponent(), TEXT("wind.tornado"), NAME_None, Radius / 100.f);
		Roar = WRSound::StartLoop(this, TEXT("wind.tornado_loop"));
	}
}

void AWRWindTornado::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Funnel) WRFx::Stop(Funnel);
	if (Roar) Roar->FadeOut(0.6f, 0.f);
	if (HasAuthority())
	{
		if (AWRCharacterBase* C = Caster.Get())
		{
			WRWind::SetTornado(C, nullptr, 0.f);
		}
		// What it held drops (a lifted body falls where it is).
		Lifted.Reset();
	}
	Super::EndPlay(Reason);
}

void AWRWindTornado::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority())
	{
		return;
	}
	AWRCharacterBase* C = Caster.Get();
	if (!C || !C->IsAlive())
	{
		Destroy();
		return;
	}
	const FWRTuning& T = FWRTuning::Get();
	// Steered: it turns toward the caster's crosshair (at most turnRate a second); let go, it runs straight on.
	if (bSteering)
	{
		FVector Point;
		const AWRCharacter* Survivor = Cast<AWRCharacter>(C);
		if (Survivor && Survivor->CrosshairPoint(T.Num(TEXT("spells.tornado.steerRange"), 4000.f), false, Point))
		{
			const FVector Want = (Point - GetActorLocation()).GetSafeNormal2D();
			if (!Want.IsNearlyZero() && FVector::Dist2D(Point, GetActorLocation()) > Radius * 0.5f)
			{
				const float Have = Heading.Rotation().Yaw;
				const float Goal = Want.Rotation().Yaw;
				const float Turn = FMath::Clamp(FMath::FindDeltaAngleDegrees(Have, Goal), -T.Num(TEXT("spells.tornado.turnRate"), 120.f) * DeltaSeconds, T.Num(TEXT("spells.tornado.turnRate"), 120.f) * DeltaSeconds);
				Heading = FRotator(0.f, Have + Turn, 0.f).Vector();
			}
		}
	}
	// Along the ground: it climbs and drops with it; a cliff it can't climb turns it back.
	FVector Next = GetActorLocation() + Heading * Speed * DeltaSeconds;
	FHitResult G;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(WRTornadoGround), false, this);
	Query.AddIgnoredActor(C);
	if (GetWorld()->LineTraceSingleByChannel(G, Next + FVector(0, 0, 400.f), Next - FVector(0, 0, 2000.f), ECC_WorldStatic, Query))
	{
		if (G.ImpactPoint.Z - GetActorLocation().Z > T.Num(TEXT("spells.tornado.maxStep"), 250.f))
		{
			Heading = -Heading;
			Next = GetActorLocation();
		}
		else
		{
			Next.Z = G.ImpactPoint.Z;
		}
	}
	SetActorLocation(Next);

	const float LiftHeight = T.Num(TEXT("spells.tornado.liftHeight"), 250.f);
	const float Orbit = T.Num(TEXT("spells.tornado.orbit"), 300.f);
	const float Pull = T.Num(TEXT("spells.tornado.midlandPull"), 150.f);
	const bool bHeal = C->HasTag(WRWind::Combo_HurricaneEye);
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		AWRCharacterBase* Who = *It;
		if (Who == C || !Who->IsAlive()) continue;
		const FVector L = Who->GetActorLocation();
		const FVector Off = L - GetActorLocation();
		if (Off.Size2D() > Radius + Who->GetSimpleCollisionRadius() || Off.Z < -200.f || Off.Z > 900.f) continue;
		if (!C->IsHostileTo(Who))
		{
			// Hurricane eye (Wind + Tide 50): allies inside heal.
			if (bHeal && Who->GetPlayerState() && Who->GetAttributes())
			{
				UWRAttributeSet* A = Who->GetAttributes();
				Who->GetAbilitySystemComponent()->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(),
					FMath::Min(A->GetMaxHealth(), A->GetHealth() + FWRComboRegistry::Get().Num(TEXT("hurricane_eye"), TEXT("healPerSecond"), 6.f) * DeltaSeconds));
			}
			continue;
		}
		WRWind::WindHit(C, Who, Dps * DeltaSeconds, 0.f, 0.f, GetActorLocation(), true, false, true);
		if (Who->GetPlayerState() || WRWind::IsBoss(Who))
		{
			continue;   // survivors (PvP) and bosses only take the damage
		}
		const FName Tier = WRWind::TierOf(Who);
		const FVector Tangent = FVector::CrossProduct(FVector::UpVector, Off.GetSafeNormal2D());
		if (Tier == TEXT("lowland"))
		{
			// Lifted: carried up the funnel and round it, held there while it passes.
			const float Want = GetActorLocation().Z + LiftHeight + Who->GetSimpleCollisionHalfHeight();
			Who->LaunchCharacter(Tangent * Orbit - Off.GetSafeNormal2D() * Pull + FVector(0.f, 0.f, FMath::Clamp((Want - L.Z) * 4.f, -300.f, 600.f)), true, true);
			Lifted.FindOrAdd(Who) += DeltaSeconds;
		}
		else if (Tier == TEXT("midland"))
		{
			// Dragged toward the middle.
			Who->AddActorWorldOffset(-Off.GetSafeNormal2D() * Pull * DeltaSeconds, true);
		}
	}
}

// ---------------------------------------------------------------- ground patches

AWRWindPatch::AWRWindPatch()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

AWRWindPatch* AWRWindPatch::Lay(AWRCharacterBase* InBy, const FVector& At, float InRadius, float Seconds, float InDps, float InPush, float InBurn)
{
	if (!InBy || !InBy->HasAuthority())
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWRWindPatch* P = InBy->GetWorld()->SpawnActor<AWRWindPatch>(AWRWindPatch::StaticClass(), At, FRotator::ZeroRotator, Params);
	if (P)
	{
		P->By = InBy;
		P->Radius = InRadius;
		P->Dps = InDps;
		P->Push = InPush;
		P->BurnPerSecond = InBurn;
		P->SetLifeSpan(FMath::Max(0.1f, Seconds));
		// A fire patch (Ember rain) burns small; a gale swirls dust and grass round the spot.
		WRFx::Broadcast(InBy, InBurn > 0.f ? TEXT("wind.ember_patch") : TEXT("wind.gale"), At, FRotator::ZeroRotator, InRadius / 100.f,
			InBurn > 0.f ? TEXT("fire.impact") : TEXT("wind.gale"), Seconds);
	}
	return P;
}

void AWRWindPatch::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AWRCharacterBase* C = By.Get();
	if (!C)
	{
		Destroy();
		return;
	}
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		AWRCharacterBase* Who = *It;
		if (Who == C || !Who->IsAlive() || !C->IsHostileTo(Who)) continue;
		const FVector FeetAt = Who->GetActorLocation() - FVector(0.f, 0.f, Who->GetSimpleCollisionHalfHeight());
		const FVector Off = FeetAt - GetActorLocation();
		if (Off.Size2D() > Radius + Who->GetSimpleCollisionRadius() || FMath::Abs(Off.Z) > 250.f) continue;
		if (Dps > 0.f)
		{
			WRWind::WindHit(C, Who, Dps * DeltaSeconds, 0.f, 0.f, GetActorLocation(), true, false, true);
		}
		if (BurnPerSecond > 0.f)
		{
			Who->Status->AddBuildup(WRTags::Element_Fire, BurnPerSecond * DeltaSeconds, C);
		}
		// The gale pushes outward; lowland creatures can't walk into it at all.
		if (Push > 0.f && !Who->GetPlayerState() && !WRWind::IsBoss(Who))
		{
			const FVector Out = Off.GetSafeNormal2D().IsNearlyZero() ? FVector::ForwardVector : Off.GetSafeNormal2D();
			const float Speed = WRWind::TierOf(Who) == TEXT("lowland") ? FMath::Max(Push, Who->GetCharacterMovement()->GetMaxSpeed() * 1.1f) : Push;
			Who->AddActorWorldOffset(Out * Speed * DeltaSeconds, true);
		}
	}
}
