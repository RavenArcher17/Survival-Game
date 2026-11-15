#include "WRAbilitiesTide.h"
#include "Wildreach.h"
#include "WRArcanePath.h"
#include "WRAttributeSet.h"
#include "WRBoss.h"
#include "WRCharacter.h"
#include "WRCharacterBase.h"
#include "WRCombatLibrary.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRNaturePath.h"
#include "WRNecroPath.h"
#include "WRPathComponent.h"
#include "WRPlantSummon.h"
#include "WRPlayerController.h"
#include "WRProjectile.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRTidePath.h"
#include "WRToolMesh.h"
#include "WRTuning.h"
#include "WRUnderwater.h"
#include "WRWeapons.h"
#include "WRWindPath.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"

namespace WRAbilitiesTideImpl
{
	static const FLinearColor TdSea(FColor(0x1E, 0x8C, 0x86));
	float TdNow(const UObject* Ctx)
	{
		const UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
		const AGameStateBase* GS = W ? W->GetGameState() : nullptr;
		return GS ? float(GS->GetServerWorldTimeSeconds()) : (W ? W->GetTimeSeconds() : 0.f);
	}
	AWRCharacterBase* TdLocked(const AWRCharacterBase* Caster)
	{
		const AWRPlayerController* PC = Caster ? Cast<AWRPlayerController>(Caster->GetController()) : nullptr;
		AWRCharacterBase* T = PC ? PC->GetServerLock() : nullptr;
		return T && T->IsAlive() && !T->IsHidden() && Caster->IsHostileTo(T) && FVector::Dist(T->GetActorLocation(), Caster->GetActorLocation()) <= WRTune(TEXT("spells.lockRange"), 4000.f) ? T : nullptr;
	}
	FVector TdCrosshair(const AWRCharacterBase* Caster, const FVector& From)
	{
		const FVector View = Caster->GetBaseAimRotation().Vector();
		const AWRCharacter* S = Cast<AWRCharacter>(Caster);
		FVector Point;
		if (!S || !S->GetController() || !S->CrosshairPoint(20000.f, true, Point)) return View;
		const FVector Dir = (Point - From).GetSafeNormal();
		return Dir.IsNearlyZero() || FVector::DistSquared(Point, From) < FMath::Square(100.f) || FVector::DotProduct(Dir, View) < 0.3f ? View : Dir;
	}
	bool TdCanPay(const FGameplayAbilityActorInfo* ActorInfo, float Cost)
	{
		const AWRCharacterBase* Who = ActorInfo ? Cast<AWRCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
		if (!Who) return false;
		if (WRTidePath::HasFlow(Who, Cost)) return true;
		if (Who->IsLocallyControlled()) WRTidePath::NoteRefused(Who);
		return false;
	}
	void TdTell(const AActor* Who, const FString& Text)
	{
		const APawn* P = Cast<APawn>(Who);
		if (AWRPlayerController* PC = P ? Cast<AWRPlayerController>(P->GetController()) : nullptr) PC->ClientNotify(Text);
	}
	/** lowland / midland / highland, or boss (a major) / mini. */
	FName TdTier(const AWRCharacterBase* Who)
	{
		if (const AWRBoss* B = Cast<AWRBoss>(Who)) return B->IsMajor() ? FName(TEXT("boss")) : FName(TEXT("mini"));
		const AWRCreature* C = Cast<AWRCreature>(Who);
		const FWRCreatureDef* D = C ? C->GetDef() : nullptr;
		return D && !D->Tier.IsNone() ? D->Tier : FName(TEXT("lowland"));
	}
	bool TdSmall(FName Tier) { return Tier == TEXT("lowland") || Tier == TEXT("midland"); }
	/** Someone on the caster's side a heal or a shield may touch (a survivor of theirs, a tame, a raised minion, a plant). */
	bool TdAlly(const AWRCharacterBase* Caster, const AWRCharacterBase* Other)
	{
		return Other && Other->IsAlive() && (Other == Caster || (!Caster->IsHostileTo(Other) && (Other->GetPlayerState() || !Other->GetTameOwner().IsEmpty())));
	}
	/** The ally nearest the caster's crosshair within Range (15 m) in sight, else the caster. */
	AWRCharacterBase* TdAllyAimed(AWRCharacterBase* Caster, float Range)
	{
		const FVector Eye = Caster->GetActorLocation() + FVector(0, 0, 60.f);
		const FVector View = Caster->GetBaseAimRotation().Vector();
		AWRCharacterBase* Best = Caster;
		float BestDot = WRTune(TEXT("spells.shapes.ally.aimDot"), 0.9f);
		for (TActorIterator<AWRCharacterBase> It(Caster->GetWorld()); It; ++It)
		{
			if (*It == Caster || !TdAlly(Caster, *It)) continue;
			const FVector To = It->GetActorLocation() - Eye;
			if (To.SizeSquared() > Range * Range) continue;
			const float Dot = FVector::DotProduct(To.GetSafeNormal(), View);
			if (Dot > BestDot) { BestDot = Dot; Best = *It; }
		}
		return Best;
	}
	TArray<AWRCharacterBase*> TdHostilesNear(AWRCharacterBase* Caster, const FVector& At, float Radius)
	{
		TArray<AWRCharacterBase*> Out;
		for (TActorIterator<AWRCharacterBase> It(Caster->GetWorld()); It; ++It)
		{
			if (!It->IsAlive() || !Caster->IsHostileTo(*It)) continue;
			if (FVector::DistSquared(It->GetActorLocation(), At) <= FMath::Square(Radius + It->GetSimpleCollisionRadius())) Out.Add(*It);
		}
		return Out;
	}
	FVector TdGround(const AActor* Ctx, const FVector& At)
	{
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRTideGround), false, Ctx);
		return Ctx->GetWorld()->LineTraceSingleByChannel(Hit, At + FVector(0, 0, 500), At - FVector(0, 0, 3000), ECC_WorldStatic, Q) ? FVector(Hit.ImpactPoint) : At;
	}
}
using namespace WRAbilitiesTideImpl;

// ================================================================================================ the spell base

UWRGA_TideSpell::UWRGA_TideSpell()
{
	StaminaKey = TEXT("stamina.spell");
	StaminaFallback = 12.f;
	PathId = TEXT("tide");
	ActivationOwnedTags.AddTag(WRTags::State_Attacking);
	FGameplayTagContainer Blocked = ActivationBlockedTags;
	Blocked.AddTag(WRTags::State_Attacking);
	Blocked.AddTag(WRTags::State_Dodging);
	ActivationBlockedTags = Blocked;
	bAllowedMounted = true;
}

float UWRGA_TideSpell::Spell(const TCHAR* Field, float Fallback) const { return FWRTuning::Get().Num(SpellKey + TEXT(".") + Field, Fallback); }
float UWRGA_TideSpell::FlowCost() const { return Spell(TEXT("flowCost"), 0.f); }

bool UWRGA_TideSpell::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	return Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags) && TdCanPay(ActorInfo, FlowCost());
}

void UWRGA_TideSpell::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	Super::ApplyCost(Handle, ActorInfo, ActivationInfo);
	AWRCharacterBase* Who = ActorInfo ? Cast<AWRCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (Who && HasAuthority(&ActivationInfo)) WRTidePath::SpendFlow(Who, FlowCost());
}

FWRHit UWRGA_TideSpell::TideHit(AWRCharacterBase* Caster, float Damage, float Soak) const
{
	FWRHit Hit;
	Hit.Instigator = Caster;
	Hit.Damage = Damage;
	Hit.Element = WRTags::Element_Water;
	Hit.PathId = TEXT("tide");
	Hit.bSpell = true;
	if (Soak > 0.f) Hit.Buildups.Add(TEXT("soak"), Soak);
	Hit.Origin = Caster->GetActorLocation();
	return Hit;
}

void UWRGA_TideSpell::TakeAim(AWRCharacterBase* Caster)
{
	const FVector Chest = Caster->GetActorLocation() + FVector(0.f, 0.f, 40.f);
	AWRCharacterBase* Lock = TdLocked(Caster);
	AimLock = Lock;
	const float Speed = Spell(TEXT("speed"), 2600.f);
	AimDir = Lock ? (UWRCombatLibrary::LeadPoint(Chest, Lock->GetActorLocation(), Lock->GetVelocity(), Speed) - Chest).GetSafeNormal() : TdCrosshair(Caster, Chest);
}

void UWRGA_TideSpell::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
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
	const float Windup = Spell(TEXT("windup"), T.Num(ShapeRow + TEXT(".windup"), 0.3f));
	const float Recovery = Spell(TEXT("recovery"), T.Num(ShapeRow + TEXT(".recovery"), 0.35f));
	const bool bRoots = Spell(TEXT("roots"), T.Num(ShapeRow + TEXT(".roots"), 0.f)) > 0.f;
	if (bRoots) SetMoveRoot(true);
	else SetMoveSlow(true, true);
	if (IsServer() && bHoldsDrain) WRTidePath::HoldDrain(Caster, Windup + Recovery + Spell(TEXT("seconds"), 0.f));
	Caster->PlaySwing(EWRSwingKind::Cast, Windup + 0.1f);
	Caster->SetCombatTag(WRTags::State_Casting, true);
	if (IsServer()) WRFx::BroadcastOn(Caster, TEXT("tide.cast"), TEXT("hand"), 1.f, TEXT("tide.cast"), Windup + 0.1f);
	After(Windup, [this]()
	{
		AWRCharacterBase* C = GetCharacter();
		if (!C) return;
		C->SetCombatTag(WRTags::State_Casting, false);
		if (!IsServer() || !C->IsAlive()) return;
		TakeAim(C);
		CastTide(C);
	});
	After(Windup + Recovery, [this]() { EndSelf(); });
}

void UWRGA_TideSpell::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (AWRCharacterBase* C = GetCharacter()) C->SetCombatTag(WRTags::State_Casting, false);
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// ================================================================================================ Water bolt

UWRGA_WaterBolt::UWRGA_WaterBolt()
{
	DisplayName = TEXT("Water bolt"); NodeId = TEXT("water_bolt");
	SpellKey = TEXT("spells.waterBolt"); CooldownKey = TEXT("spells.waterBolt.cooldown"); CooldownFallback = 1.f;
	ShapeName = TEXT("bolt");
}

void UWRGA_WaterBolt::CastTide(AWRCharacterBase* Caster)
{
	// A bolt of spinning sea-water (2,600 cm/s; full speed under water, where everyone else's bolts slow).
	const float Speed = FMath::Min(Spell(TEXT("speed"), 2600.f), WRTune(TEXT("spells.maxBoltSpeed"), 4000.f));
	const FVector Chest = Caster->GetActorLocation() + FVector(0.f, 0.f, 40.f);
	FWRHit Hit = TideHit(Caster, Spell(TEXT("damage"), 15.f), Spell(TEXT("soak"), 35.f));
	Hit.bProjectile = true;
	Hit.PoiseDamage = Spell(TEXT("poise"), 8.f);
	Hit.Origin = Chest;
	FHitResult Near;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(WRTideMuzzle), false, Caster);
	if (Caster->GetWorld()->SweepSingleByChannel(Near, Chest, Chest + AimDir * 70.f, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(14.f), Query))
	{
		AWRCharacterBase* Blocker = Cast<AWRCharacterBase>(Near.GetActor());
		if (Blocker && Caster->IsHostileTo(Blocker)) { UWRCombatLibrary::ApplyHit(Blocker, Hit); return; }
	}
	FActorSpawnParameters Params;
	Params.Owner = Caster;
	Params.Instigator = Caster;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AWRProjectile* Bolt = Caster->GetWorld()->SpawnActor<AWRProjectile>(AWRProjectile::StaticClass(), Chest + AimDir * 70.f, AimDir.Rotation(), Params))
	{
		Bolt->Init(Hit, Speed, TdSea);
		Bolt->SetTrail(TEXT("tide.bolt"));
		AWRCharacterBase* Lock = AimLock.Get();
		if (Lock && Lock->IsAlive() && Lock->IsCreature())
		{
			const float Homing = WRTune(TEXT("spells.lockHoming"), 6000.f);
			if (Homing > 0.f) Bolt->HomeOn(Lock, Homing);
		}
	}
}

// ================================================================================================ Riptide pull

UWRGA_RiptidePull::UWRGA_RiptidePull()
{
	DisplayName = TEXT("Riptide pull"); NodeId = TEXT("riptide_pull");
	SpellKey = TEXT("spells.riptidePull"); CooldownKey = TEXT("spells.riptidePull.cooldown"); CooldownFallback = 10.f;
	ShapeName = TEXT("pull");
}

void UWRGA_RiptidePull::CastTide(AWRCharacterBase* Caster)
{
	// A rush of water out along the cone, and everything in it dragged back along the current toward you.
	const float Range = Spell(TEXT("range"), 1200.f);
	const float HalfAngle = FMath::DegreesToRadians(Spell(TEXT("angle"), 30.f) * 0.5f);
	const float Drag = WRTidePath::Rank(Caster, TEXT("drag"));
	const float Pull = Spell(TEXT("pull"), 500.f) + FMath::Max(0.f, UWRPathComponent::Stat(Caster, TEXT("pullDistance")));
	const FVector From = Caster->GetActorLocation();
	const FVector Fwd = AimDir.GetSafeNormal2D().IsNearlyZero() ? Caster->GetActorForwardVector() : AimDir.GetSafeNormal2D();
	WRFx::Broadcast(Caster, TEXT("tide.riptide"), From, Fwd.Rotation(), Range / 100.f, TEXT("tide.riptide"));
	for (AWRCharacterBase* Who : TdHostilesNear(Caster, From, Range))
	{
		const FVector To = Who->GetActorLocation() - From;
		if (FMath::Acos(FMath::Clamp(FVector::DotProduct(To.GetSafeNormal2D(), Fwd), -1.f, 1.f)) > HalfAngle) continue;
		const FName Tier = TdTier(Who);
		FWRHit Hit = TideHit(Caster, Spell(TEXT("damage"), 4.f), Spell(TEXT("soak"), 20.f));
		Hit.ImpactPoint = Who->GetActorLocation();
		UWRCombatLibrary::ApplyHit(Who, Hit);
		if (Tier == TEXT("boss") || Tier == TEXT("mini")) continue;                     // bosses are never pulled
		const bool bHigh = Tier == TEXT("highland");
		if (bHigh && Drag < 3) continue;                                                // only Drag 3 moves the highland, half way
		const float Room = FMath::Max(0.f, To.Size2D() - Caster->GetSimpleCollisionRadius() - Who->GetSimpleCollisionRadius() - 60.f);
		const float Dist = FMath::Min(Room, Pull * (bHigh ? 0.5f : 1.f));
		const float Secs = Spell(TEXT("pullSeconds"), 0.45f);
		WRWind::Knock(Caster, Who, -To.GetSafeNormal2D(), Dist, Secs, TEXT("riptide"));
		if (Tier == TEXT("lowland") && Who->Status)
		{
			TWeakObjectPtr<AWRCharacterBase> W(Who);
			FTimerHandle H;
			Who->GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([W, this]() { if (W.IsValid() && W->Status) W->Status->Stagger(Spell(TEXT("stagger"), 0.5f)); }), Secs, false);
		}
	}
}

// ================================================================================================ Whirlpool / Maelstrom

UWRGA_Whirlpool::UWRGA_Whirlpool()
{
	DisplayName = TEXT("Whirlpool"); NodeId = TEXT("whirlpool");
	SpellKey = TEXT("spells.whirlpool"); CooldownKey = TEXT("spells.whirlpool.cooldown"); CooldownFallback = 18.f;
	ShapeName = TEXT("pool");
}

static AWRTideField* TdSpawnField(AWRCharacterBase* Caster, const FVector& At, bool bMaelstrom, float Radius, float Seconds)
{
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWRTideField* F = Caster->GetWorld()->SpawnActor<AWRTideField>(AWRTideField::StaticClass(), FTransform(At), P);
	if (!F) return nullptr;
	F->Caster = Caster;
	F->bMaelstrom = bMaelstrom;
	F->Radius = Radius;
	F->EndAt = TdNow(Caster) + Seconds;
	F->bBoiling = Caster->HasTag(WRTidePath::Combo_BoilingSea);
	return F;
}

void UWRGA_Whirlpool::CastTide(AWRCharacterBase* Caster)
{
	const float Range = Spell(TEXT("range"), 1500.f);
	const FVector Feet = Caster->GetActorLocation() - FVector(0, 0, Caster->GetSimpleCollisionHalfHeight());
	FVector At = Feet + AimDir.GetSafeNormal2D() * FMath::Min(Range, 800.f);
	if (const AWRCharacterBase* Lock = AimLock.Get()) At = Lock->GetActorLocation();
	const AWRCharacter* S = Cast<AWRCharacter>(Caster);
	FVector Point;
	if (!AimLock.IsValid() && S && S->CrosshairPoint(Range, false, Point)) At = Point;
	At = WRUnderwater::IsUnder(Caster) ? At : TdGround(Caster, At);
	TdSpawnField(Caster, At, false, Spell(TEXT("radius"), 350.f), Spell(TEXT("seconds"), 6.f));
	WRSound::PlayAt(Caster, TEXT("tide.whirlpool"), At);
}

UWRGA_Maelstrom::UWRGA_Maelstrom()
{
	DisplayName = TEXT("Maelstrom"); NodeId = TEXT("maelstrom");
	SpellKey = TEXT("spells.maelstrom"); CooldownKey = TEXT("spells.maelstrom.cooldown"); CooldownFallback = 60.f;
	ShapeName = TEXT("storm");
	bHoldsDrain = true;
}

void UWRGA_Maelstrom::CastTide(AWRCharacterBase* Caster)
{
	const float Range = Spell(TEXT("range"), 1500.f);
	FVector At = Caster->GetActorLocation() + AimDir.GetSafeNormal2D() * 600.f;
	if (const AWRCharacterBase* Lock = AimLock.Get()) At = Lock->GetActorLocation();
	const AWRCharacter* S = Cast<AWRCharacter>(Caster);
	FVector Point;
	if (!AimLock.IsValid() && S && S->CrosshairPoint(Range, false, Point)) At = Point;
	At = WRUnderwater::IsUnder(Caster) ? At : TdGround(Caster, At);
	TdSpawnField(Caster, At, true, Spell(TEXT("radius"), 1000.f), Spell(TEXT("seconds"), 8.f));
	WRSound::Broadcast(Caster, TEXT("tide.maelstrom"));
}

AWRTideField::AWRTideField()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	SetNetUpdateFrequency(4.f);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRTideField::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRTideField, bMaelstrom);
	DOREPLIFETIME(AWRTideField, Radius);
	DOREPLIFETIME(AWRTideField, EndAt);
	DOREPLIFETIME(AWRTideField, bBoiling);
}

void AWRTideField::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Now = TdNow(this);
	if (GetNetMode() != NM_DedicatedServer && !bLookBuilt)
	{
		// The swirl: a spiral of foam and a sunk eye (on land a turning column of water); boiling steam with Boiling sea.
		bLookBuilt = true;
		Look = WRFx::Attach(RootComponent, bMaelstrom ? FName(TEXT("tide.maelstrom")) : FName(TEXT("tide.whirlpool")), NAME_None, Radius / 100.f);
		if (bBoiling) WRFx::Attach(RootComponent, TEXT("tide.boil"), NAME_None, Radius / 150.f);
	}
	if (!HasAuthority()) return;
	if (Now >= EndAt) { Destroy(); return; }
	AWRCharacterBase* C = Caster.Get();
	if (!C) { Destroy(); return; }
	const FString K = bMaelstrom ? TEXT("spells.maelstrom.") : TEXT("spells.whirlpool.");
	const FWRTuning& T = FWRTuning::Get();
	Clock += DeltaSeconds;
	const bool bTick = Clock >= 0.25f;
	if (bTick) Clock = 0.f;
	const FVector Centre = GetActorLocation();
	for (AWRCharacterBase* Who : TdHostilesNear(C, Centre, Radius))
	{
		const FName Tier = TdTier(Who);
		const FVector To = Centre - Who->GetActorLocation();
		const bool bBoss = Tier == TEXT("boss") || Tier == TEXT("mini");
		if (bMaelstrom)
		{
			// Everything dragged toward the eye at 3 m/s (highland half), never a boss; the eye bites twice as hard.
			if (!bBoss && To.SizeSquared2D() > 120.f * 120.f)
			{
				const float Speed = T.Num(K + TEXT("pullSpeed"), 300.f) * (Tier == TEXT("highland") ? 0.5f : 1.f);
				WRWind::Knock(C, Who, To.GetSafeNormal2D(), Speed * DeltaSeconds, DeltaSeconds * 1.5f, TEXT("maelstrom"));
			}
			// Frozen maelstrom (Frost + Tide 50): what it drags to the eye freezes there.
			if (!bBoss && C->HasTag(WRTidePath::Combo_FrozenMaelstrom) && To.SizeSquared2D() < 250.f * 250.f && !Frozen.Contains(Who) && Who->Status)
			{
				Frozen.Add(Who);
				Who->Status->ApplyStatus(WRTags::Status_Frozen, WRTune(TEXT("tide.combos.frozenMaelstromSeconds"), 2.5f));
				WRFx::Broadcast(Who, TEXT("tide.flash_freeze"), Who->GetActorLocation(), FRotator::ZeroRotator, 1.f, TEXT("frost.freeze"));
			}
		}
		else if (Who->Status)
		{
			// Holds lowland and midland (rooted while inside), slows the highland x0.7; bosses only soaked.
			if (TdSmall(Tier)) Who->Status->ApplyStatus(WRTags::Status_Rooted, 0.4f);
			else if (Tier == TEXT("highland")) Who->Status->ApplyStatus(WRTags::Status_Slowed, 0.4f);
		}
		if (!bTick) continue;
		const bool bCentre = bMaelstrom && TdSmall(Tier) && To.SizeSquared2D() < FMath::Square(T.Num(K + TEXT("centreRadius"), 250.f));
		FWRHit Hit;
		Hit.Instigator = C;
		Hit.Damage = T.Num(K + TEXT("dps"), bMaelstrom ? 12.f : 6.f) * 0.25f * (bCentre ? T.Num(K + TEXT("centreMult"), 2.f) : 1.f);
		Hit.Element = WRTags::Element_Water;
		Hit.PathId = TEXT("tide");
		Hit.bSpell = true;
		Hit.bDamageOverTime = true;
		Hit.Buildups.Add(TEXT("soak"), T.Num(K + TEXT("soakPerSecond"), bMaelstrom ? 20.f : 15.f) * 0.25f);
		Hit.Origin = Centre;
		// Boiling sea (Fire + Tide 50): the water boils: fire damage and burn on top.
		if (bBoiling)
		{
			Hit.Damage += WRTune(TEXT("tide.combos.boilingDps"), 6.f) * 0.25f;
			Hit.BurnBuildup = WRTune(TEXT("tide.combos.boilingBurn"), 12.f) * 0.25f;
		}
		UWRCombatLibrary::ApplyHit(Who, Hit);
	}
}

// ================================================================================================ the waves

UWRGA_HealingWave::UWRGA_HealingWave()
{
	DisplayName = TEXT("Healing wave"); NodeId = TEXT("healing_wave");
	SpellKey = TEXT("spells.healingWave"); CooldownKey = TEXT("spells.healingWave.cooldown"); CooldownFallback = 12.f;
	ShapeName = TEXT("wall");
}

void UWRGA_HealingWave::CastTide(AWRCharacterBase* Caster)
{
	float Heal = Spell(TEXT("heal"), 35.f) * WRTidePath::MendScale(Caster);
	// Life tide (Blood + Tide 25): the wave costs you HP and heals far more.
	if (Caster->HasTag(WRTidePath::Combo_LifeTide))
	{
		const UWRAttributeSet* A = Caster->GetAttributes();
		const float Cost = WRTune(TEXT("tide.combos.lifeTideHp"), 15.f);
		if (A && A->GetHealth() > Cost + 1.f)
		{
			if (UAbilitySystemComponent* ASC = Caster->GetAbilitySystemComponent()) ASC->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), A->GetHealth() - Cost);
			Heal *= WRTune(TEXT("tide.combos.lifeTideMult"), 2.f);
			WRFx::BroadcastOn(Caster, TEXT("tide.life_tide"), TEXT("spine"), 1.f, TEXT("blood.cast"), 0.4f);
		}
	}
	AWRTideWave::Launch(Caster, false, Spell(TEXT("width"), 600.f), Spell(TEXT("travel"), 1200.f), Spell(TEXT("speed"), 900.f), Heal);
}

UWRGA_HighTide::UWRGA_HighTide()
{
	DisplayName = TEXT("High tide"); NodeId = TEXT("high_tide");
	SpellKey = TEXT("spells.highTide"); CooldownKey = TEXT("spells.highTide.cooldown"); CooldownFallback = 90.f;
	ShapeName = TEXT("wall");
	bHoldsDrain = true;
}

void UWRGA_HighTide::CastTide(AWRCharacterBase* Caster)
{
	AWRTideWave::Launch(Caster, true, Spell(TEXT("width"), 1000.f), Spell(TEXT("travel"), 1800.f), Spell(TEXT("speed"), 1000.f), Spell(TEXT("heal"), 60.f) * WRTidePath::MendScale(Caster));
	WRSound::Broadcast(Caster, TEXT("tide.high_tide"));
}

AWRTideWave::AWRTideWave()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	SetNetUpdateFrequency(2.f);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRTideWave::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRTideWave, Origin);
	DOREPLIFETIME(AWRTideWave, Dir);
	DOREPLIFETIME(AWRTideWave, Width);
	DOREPLIFETIME(AWRTideWave, Length);
	DOREPLIFETIME(AWRTideWave, Speed);
	DOREPLIFETIME(AWRTideWave, StartAt);
	DOREPLIFETIME(AWRTideWave, bHigh);
	DOREPLIFETIME(AWRTideWave, bThunder);
}

AWRTideWave* AWRTideWave::Launch(AWRCharacterBase* Caster, bool bInHigh, float InWidth, float InLength, float InSpeed, float InHeal)
{
	if (!Caster || !Caster->HasAuthority()) return nullptr;
	const FVector Fwd = Caster->GetActorForwardVector().GetSafeNormal2D();
	const FVector From = Caster->GetActorLocation() - FVector(0, 0, Caster->GetSimpleCollisionHalfHeight()) + Fwd * 80.f;
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWRTideWave* W = Caster->GetWorld()->SpawnActor<AWRTideWave>(AWRTideWave::StaticClass(), FTransform(Fwd.Rotation(), From), P);
	if (!W) return nullptr;
	W->Caster = Caster;
	W->Origin = From;
	W->Dir = Fwd;
	W->Width = InWidth;
	W->Length = InLength;
	W->Speed = FMath::Max(100.f, InSpeed);
	W->StartAt = TdNow(Caster);
	W->bHigh = bInHigh;
	W->bThunder = Caster->HasTag(WRTidePath::Combo_ThunderTide);
	W->Heal = InHeal;
	W->Touched.Add(Caster);
	// The caster's own heal (they stand behind the wall as it rises).
	WRNature::Heal(Caster, Caster, InHeal, false, true);
	return W;
}

void AWRTideWave::Touch(AWRCharacterBase* Who)
{
	AWRCharacterBase* C = Caster.Get();
	if (!C || !Who) return;
	const FWRTuning& T = FWRTuning::Get();
	const bool bAlly = TdAlly(C, Who) || (C->HasTag(WRTidePath::Combo_TidalDead) && WRNecro::IsRaised(Who) && WRNecro::MasterOf(Who) == C);
	if (bAlly)
	{
		WRNature::Heal(C, Who, Heal, false, true);
		// Hot spring (Fire + Tide 25): the wave puts out the burns on allies it passes.
		if (C->HasTag(WRTidePath::Combo_HotSpring) && Who->Status) Who->Status->CureStatus(WRStatus::Burn);
		// Current (Tidecaller): allies the wave passes run faster for 6 s (+6% a rank), Tide walkers gain Flow.
		if (const int32 Cur = WRTidePath::Rank(C, TEXT("current")); Cur > 0 && Who->Status)
		{
			Who->Status->ApplyStatus(Cur >= 3 ? WRTidePath::Status_Current3 : Cur == 2 ? WRTidePath::Status_Current2 : WRTidePath::Status_Current, T.Num(TEXT("tide.path.currentSeconds"), 6.f));
			if (WRTidePath::Walks(Who)) WRTidePath::GainFlow(Who, UWRPathComponent::Stat(C, TEXT("allyFlow")));
		}
		// Rain (Nature + Tide 25): the wave makes your plants grow (they quicken and mend).
		if (AWRPlantSummon* Plant = Cast<AWRPlantSummon>(Who); Plant && C->HasTag(WRTidePath::Combo_Rain))
		{
			Plant->Quicken(TdNow(C) + T.Num(TEXT("tide.combos.rainQuicken"), 8.f));
			Plant->HealShare(T.Num(TEXT("tide.combos.rainHeal"), 0.25f));
		}
		WRFx::BroadcastOn(Who, TEXT("tide.heal"), TEXT("spine"), 1.f, TEXT("tide.heal"), 0.6f);
		return;
	}
	if (!C->IsHostileTo(Who) || !Who->IsAlive()) return;
	// Thunder tide (Lightning + Tide 50): your waves carry lightning into the enemies they pass.
	if (bThunder)
	{
		FWRHit Shock;
		Shock.Instigator = C;
		Shock.Damage = T.Num(TEXT("tide.combos.thunderTideDamage"), 8.f);
		Shock.Element = WRTags::Element_Lightning;
		Shock.Buildups.Add(TEXT("shock"), T.Num(TEXT("tide.combos.thunderTideShock"), 30.f));
		Shock.PathId = TEXT("tide");
		Shock.bSpell = true;
		Shock.Origin = Who->GetActorLocation() - FVector(Dir) * 100.f;
		UWRCombatLibrary::ApplyHit(Who, Shock);
	}
	if (!bHigh) return;   // a healing wave passes through enemies harmlessly
	const FName Tier = TdTier(Who);
	FWRHit Hit;
	Hit.Instigator = C;
	Hit.Damage = T.Num(TEXT("spells.highTide.damage"), 10.f);
	Hit.Element = WRTags::Element_Water;
	Hit.PathId = TEXT("tide");
	Hit.bSpell = true;
	Hit.Buildups.Add(TEXT("soak"), T.Num(TEXT("spells.highTide.soak"), 40.f));
	Hit.Origin = Who->GetActorLocation() - FVector(Dir) * 100.f;
	if (Tier == TEXT("boss") || Tier == TEXT("mini")) Hit.StanceDamage = T.Num(TEXT("spells.highTide.bossStance"), 40.f);
	UWRCombatLibrary::ApplyHit(Who, Hit);
	if (TdSmall(Tier)) WRWind::Knock(C, Who, FVector(Dir), T.Num(TEXT("spells.highTide.knockback"), 600.f), 0.5f, TEXT("high_tide"));
	else if (Tier == TEXT("highland") && Who->Status) Who->Status->Stagger(T.Num(TEXT("spells.highTide.stagger"), 1.f));
}

void AWRTideWave::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Elapsed = FMath::Max(0.f, TdNow(this) - StartAt);
	const float Front = FMath::Min(Length, Elapsed * Speed);
	const FVector At = FVector(Origin) + FVector(Dir) * Front;
	if (GetNetMode() != NM_DedicatedServer)
	{
		// The wall of water rolls on every machine from the replicated start: a curling crest and its spray.
		if (!bLookBuilt)
		{
			bLookBuilt = true;
			Look = WRFx::Attach(RootComponent, bHigh ? FName(TEXT("tide.high_tide_wall")) : FName(TEXT("tide.wave_wall")), NAME_None, Width / 300.f);
			if (bThunder) WRFx::Attach(RootComponent, TEXT("tide.thunder_wave"), NAME_None, Width / 300.f);
		}
		SetActorLocationAndRotation(At, FVector(Dir).Rotation());
	}
	if (!HasAuthority()) return;
	SetActorLocation(At);
	// The band it passed over this tick: everyone within half the width of the line, between the last front and this.
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		if (Touched.Contains(*It) || !It->IsAlive()) continue;
		const FVector Off = It->GetActorLocation() - FVector(Origin);
		const float Along = FVector::DotProduct(Off, FVector(Dir));
		const float Across = FMath::Abs(FVector::DotProduct(Off, FVector::CrossProduct(FVector(Dir), FVector::UpVector)));
		if (Along < 0.f || Along > Front + 50.f || Across > Width * 0.5f || FMath::Abs(Off.Z) > 400.f) continue;
		Touched.Add(*It);
		Touch(*It);
	}
	if (Front >= Length) SetLifeSpan(0.6f);
}

// ================================================================================================ Cleanse, Tidal shield

UWRGA_Cleanse::UWRGA_Cleanse()
{
	DisplayName = TEXT("Cleanse"); NodeId = TEXT("cleanse");
	SpellKey = TEXT("spells.cleanse"); CooldownKey = TEXT("spells.cleanse.cooldown"); CooldownFallback = 15.f;
	ShapeName = TEXT("ally");
}

void UWRGA_Cleanse::CastTide(AWRCharacterBase* Caster)
{
	AWRCharacterBase* Who = TdAllyAimed(Caster, Spell(TEXT("range"), 1500.f));
	if (!Who || !Who->Status) return;
	// Every status and buildup but corruption (it isn't a status here: it's the survival meter).
	for (int32 Id = 0; Id < WRStatus::Count; ++Id)
	{
		Who->Status->CureStatus(Id);
		Who->Status->ReduceBuildup(Id, 100000.f);
	}
	const FGameplayTag Held[] = { WRTags::Status_Slowed, WRTags::Status_Rooted, WRTags::Status_Frozen };
	for (const FGameplayTag& Tag : Held)
	{
		if (UAbilitySystemComponent* ASC = Who->GetAbilitySystemComponent()) ASC->SetLooseGameplayTagCount(Tag, 0);
	}
	WRFx::BroadcastOn(Who, TEXT("tide.cleanse"), TEXT("spine"), 1.f, TEXT("tide.cleanse"), 0.8f);
}

UWRGA_TidalShield::UWRGA_TidalShield()
{
	DisplayName = TEXT("Tidal shield"); NodeId = TEXT("tidal_shield");
	SpellKey = TEXT("spells.tidalShield"); CooldownKey = TEXT("spells.tidalShield.cooldown"); CooldownFallback = 15.f;
	ShapeName = TEXT("ally");
}

void UWRGA_TidalShield::CastTide(AWRCharacterBase* Caster)
{
	AWRCharacterBase* Who = TdAllyAimed(Caster, Spell(TEXT("range"), 1500.f));
	if (!Who) return;
	WRTidePath::GiveTidalShield(Caster, Who, Spell(TEXT("amount"), 35.f) * WRTidePath::MendScale(Caster), Spell(TEXT("seconds"), 8.f));
	WRFx::BroadcastOn(Who, TEXT("tide.shield"), TEXT("spine"), 1.f, TEXT("tide.shield"), Spell(TEXT("seconds"), 8.f));
}

// ================================================================================================ Wave cut

UWRGA_WaveCut::UWRGA_WaveCut()
{
	DisplayName = TEXT("Wave cut"); PathId = TEXT("tide"); NodeId = TEXT("wave_cut");
	StaminaKey = TEXT("stamina.weaponArt"); StaminaFallback = 18.f;
	CooldownKey = TEXT("spells.waveCut.cooldown"); CooldownFallback = 2.5f;
	TimingKey = TEXT("weapon.lightTiming"); SwingKind = EWRSwingKind::WeaponArt;
	Element = WRTags::Element_Water; ArtKey = TEXT("spells.waveCut");
	StanceMultiplier = 1.5f;
	bAllowedMounted = true;
}

bool UWRGA_WaveCut::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags) && TdCanPay(ActorInfo, WRTune(TEXT("spells.waveCut.flowCost"), 15.f));
}

void UWRGA_WaveCut::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (IsServer() && IsActive())
	{
		WRTidePath::SpendFlow(GetCharacter(), WRTune(TEXT("spells.waveCut.flowCost"), 15.f));
		WRFx::BroadcastOn(GetCharacter(), TEXT("tide.wave_cut"), TEXT("weapon"), 1.f, TEXT("tide.wave_cut"), 0.5f);
	}
}

// ================================================================================================ Harpoon throw

UWRGA_HarpoonThrow::UWRGA_HarpoonThrow()
{
	DisplayName = TEXT("Harpoon throw"); PathId = TEXT("tide"); NodeId = TEXT("harpoon_throw");
	StaminaKey = TEXT("stamina.weaponArt"); StaminaFallback = 18.f;
	CooldownKey = TEXT("spells.harpoonThrow.cooldown"); CooldownFallback = 8.f;
	ActivationOwnedTags.AddTag(WRTags::State_Attacking);
	FGameplayTagContainer Blocked = ActivationBlockedTags;
	Blocked.AddTag(WRTags::State_Attacking);
	Blocked.AddTag(WRTags::State_Dodging);
	ActivationBlockedTags = Blocked;
	bAllowedMounted = true;
}

AWRHarpoonLine* UWRGA_HarpoonThrow::PendingLine(const FGameplayAbilityActorInfo* ActorInfo) const
{
	const AActor* Who = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	AWRHarpoonLine* L = AWRHarpoonLine::HeldBy(Who);
	return L && !L->bReeled && !L->bTied && TdNow(Who) < L->ReelUntil ? L : nullptr;
}

bool UWRGA_HarpoonThrow::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)) return false;
	if (PendingLine(ActorInfo)) return true;   // the reel
	const AWRCharacterBase* Who = ActorInfo ? Cast<AWRCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	FString Why;
	if (Who && !UWRGA_Melee::WildbrandFits(Who, NodeId, Why))
	{
		if (Who->IsLocallyControlled()) TdTell(Who, FString::Printf(TEXT("Harpoon throw: %s"), *Why));
		return false;
	}
	return true;
}

bool UWRGA_HarpoonThrow::CheckCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	return PendingLine(ActorInfo) ? true : Super::CheckCooldown(Handle, ActorInfo, OptionalRelevantTags);
}

void UWRGA_HarpoonThrow::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	if (!PendingLine(ActorInfo)) Super::ApplyCooldown(Handle, ActorInfo, ActivationInfo);
}

bool UWRGA_HarpoonThrow::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (PendingLine(ActorInfo)) return true;
	const AWRCharacterBase* Who = ActorInfo ? Cast<AWRCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	const float Cost = WRTidePath::Rank(Who, TEXT("leviathans_hook")) > 0 ? WRTune(TEXT("spells.leviathansHook.flowCost"), 40.f) : WRTune(TEXT("spells.harpoonThrow.flowCost"), 25.f);
	return Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags) && TdCanPay(ActorInfo, Cost);
}

void UWRGA_HarpoonThrow::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	if (PendingLine(ActorInfo)) return;
	Super::ApplyCost(Handle, ActorInfo, ActivationInfo);
	AWRCharacterBase* Who = ActorInfo ? Cast<AWRCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (Who && HasAuthority(&ActivationInfo))
	{
		WRTidePath::SpendFlow(Who, WRTidePath::Rank(Who, TEXT("leviathans_hook")) > 0 ? WRTune(TEXT("spells.leviathansHook.flowCost"), 40.f) : WRTune(TEXT("spells.harpoonThrow.flowCost"), 25.f));
	}
}

void UWRGA_HarpoonThrow::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AWRHarpoonLine* Pending = PendingLine(ActorInfo);
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	AWRCharacterBase* Caster = GetCharacter();
	if (!Caster) { EndSelf(true); return; }
	if (Pending)
	{
		// The reel. Held (Anchor, a big body on the line): pin it; tapped: reel or tow.
		Reeling = Pending;
		bWaitingRelease = true;
		if (UAbilityTask_WaitInputRelease* Wait = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true))
		{
			Wait->OnRelease.AddDynamic(this, &UWRGA_HarpoonThrow::OnReelReleased);
			Wait->ReadyForActivation();
		}
		After(WRTune(TEXT("spells.harpoonThrow.holdSeconds"), 0.35f), [this]()
		{
			if (!bWaitingRelease) return;
			// Still held: Anchor (a node) on a highland body or a mini.
			AWRCharacterBase* C = GetCharacter();
			AWRHarpoonLine* L = Reeling.Get();
			bWaitingRelease = false;
			if (!C || !L || !IsServer()) { EndSelf(); return; }
			AWRCharacterBase* Body = Cast<AWRCharacterBase>(L->Struck);
			const FName Tier = TdTier(Body);
			if (WRTidePath::Rank(C, TEXT("anchor")) > 0 && Body && (Tier == TEXT("highland") || Tier == TEXT("mini")) && Body->Status)
			{
				const float Secs = WRTune(TEXT("passives.anchor.rootSeconds"), 3.f);
				if (Tier == TEXT("mini")) Body->Status->ApplyStatus(WRTags::Status_Slowed, Secs);
				else Body->Status->ApplyStatus(WRTags::Status_Rooted, Secs);
				Body->Status->ApplyStatus(WRTidePath::Status_Anchored, Secs);
				L->bAnchored = true;
				L->bTaut = true;
				L->ReelUntil = TdNow(C);
				WRTidePath::GainFlow(C, WRTune(TEXT("passives.anchor.flow"), 10.f));
				WRTidePath::Proc(C, TEXT("tide.anchor"), TEXT("tide.rope_creak"));
				// A mooring stone or a cleat within 3 m: tie it off (the Reef Tyrant's tether).
				FTimerHandle H;
				TWeakObjectPtr<AWRHarpoonLine> WL(L);
				C->GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([WL]() { if (WL.IsValid() && !WL->bTied) WL->bAnchored = false; }), Secs, false);
			}
			else Reel(C, L);
			EndSelf();
		});
		return;
	}
	// The throw: a 0.4 s wind-up, the spear away on its line.
	const float Windup = WRTune(TEXT("wildbrands.harpoon_throw.windup"), 0.4f);
	const float Recovery = WRTune(TEXT("wildbrands.harpoon_throw.recovery"), 0.4f);
	SetMoveSlow(true, true);
	Caster->PlaySwing(EWRSwingKind::WeaponArt, Windup + 0.1f);
	if (IsServer()) WRFx::BroadcastOn(Caster, TEXT("tide.harpoon_wind"), TEXT("weapon"), 1.f, TEXT("tide.harpoon_wind"), Windup);
	After(Windup, [this]() { if (AWRCharacterBase* C = GetCharacter(); C && IsServer() && C->IsAlive()) Throw(C); });
	After(Windup + Recovery, [this]() { EndSelf(); });
}

void UWRGA_HarpoonThrow::OnReelReleased(float /*TimeHeld*/)
{
	if (!bWaitingRelease) return;
	bWaitingRelease = false;
	AWRCharacterBase* C = GetCharacter();
	if (C && IsServer())
	{
		if (AWRHarpoonLine* L = Reeling.Get()) Reel(C, L);
	}
	EndSelf();
}

void UWRGA_HarpoonThrow::Throw(AWRCharacterBase* Caster)
{
	const FWRTuning& T = FWRTuning::Get();
	const FVector Chest = Caster->GetActorLocation() + FVector(0.f, 0.f, 50.f);
	AWRCharacterBase* Lock = TdLocked(Caster);
	const float Speed = T.Num(TEXT("wildbrands.harpoon_throw.speed"), 2700.f);
	const FVector Dir = Lock ? (UWRCombatLibrary::LeadPoint(Chest, Lock->GetActorLocation(), Lock->GetVelocity(), Speed) - Chest).GetSafeNormal() : TdCrosshair(Caster, Chest);
	const bool bHook = WRTidePath::Rank(Caster, TEXT("leviathans_hook")) > 0;
	// The blow: the weapon's damage x1.5 (Barbed tip +6% a rank), soak 30, stance x2, bleed 10 a Barbed rank.
	FWRHit Hit;
	Hit.Instigator = Caster;
	const FWRWeaponClass& Class = FWRWeapons::Get().ClassFor(Caster);
	Hit.Damage = UWRCombatLibrary::WeaponDamage(Caster->Weapon, false) * T.Num(TEXT("wildbrands.harpoon_throw.dmg"), 1.5f) * (1.f + FMath::Max(0.f, UWRPathComponent::Stat(Caster, TEXT("harpoonDamage"))));
	Hit.Element = WRTags::Element_Water;
	Hit.PathId = TEXT("tide");
	Hit.bWeaponArt = true;
	Hit.bProjectile = true;
	Hit.PhysType = 3;
	Hit.StanceDamage = Hit.Damage * T.Num(TEXT("wildbrands.harpoon_throw.stance"), 2.f);
	Hit.Buildups.Add(TEXT("soak"), T.Num(TEXT("wildbrands.harpoon_throw.soak"), 30.f));
	const float Bleed = UWRPathComponent::Stat(Caster, TEXT("harpoonBleed")) + (Caster->HasTag(WRTidePath::Combo_SharkHarpoon) ? T.Num(TEXT("tide.combos.sharkBleed"), 60.f) : 0.f);
	if (Bleed > 0.f) Hit.Buildups.Add(TEXT("bleed"), Bleed);
	Hit.Origin = Chest;
	Hit.bReflectable = true;
	FActorSpawnParameters Params;
	Params.Owner = Caster;
	Params.Instigator = Caster;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWRProjectile* P = Caster->GetWorld()->SpawnActor<AWRProjectile>(AWRProjectile::StaticClass(), Chest + Dir * 60.f, Dir.Rotation(), Params);
	if (!P) return;
	P->InitPhysical(Hit, Dir * Speed, T.Num(TEXT("wildbrands.harpoon_throw.gravity"), 0.15f), bHook ? TEXT("code:harpoon_chain") : TEXT("code:harpoon"), FString(), TEXT("harpoon"), NAME_None);
	P->SetTrail(TEXT("tide.harpoon_trail"));
	(void)Class;
	TWeakObjectPtr<AWRCharacterBase> WeakC(Caster);
	const float LineRange = T.Num(TEXT("wildbrands.harpoon_throw.lineRange"), 1800.f);
	const FVector From = Chest;
	// A body struck: the line runs to it (and the quest counts a harpoon kill under water: WRWorldSystems' "harpoon").
	P->OnStruck = [WeakC, bHook](AWRCharacterBase* Struck, EWRHitResult Result)
	{
		AWRCharacterBase* C = WeakC.Get();
		if (!C || !Struck || (Result != EWRHitResult::Hit && Result != EWRHitResult::GuardBroken)) return;
		AWRHarpoonLine::Make(C, Struck, Struck->GetActorLocation(), bHook);
		WRSound::PlayAt(C, TEXT("tide.harpoon_hit"), Struck->GetActorLocation());
	};
	// A wall, a rock, a hull: with Tow it's a grapple (within 18 m).
	P->OnLanded = [WeakC, LineRange, From, bHook](const FVector& At)
	{
		AWRCharacterBase* C = WeakC.Get();
		if (!C || WRTidePath::Rank(C, TEXT("tow")) <= 0 || FVector::Dist(From, At) > LineRange) return;
		AWRHarpoonLine::Make(C, nullptr, At, bHook);
	};
}

void UWRGA_HarpoonThrow::Reel(AWRCharacterBase* Caster, AWRHarpoonLine* Line)
{
	if (!Caster || !Line || Line->bReeled) return;
	const FWRTuning& T = FWRTuning::Get();
	Line->bReeled = true;
	Line->bTaut = true;
	AWRCharacterBase* Body = Cast<AWRCharacterBase>(Line->Struck);
	const float ReelSpeed = T.Num(TEXT("wildbrands.harpoon_throw.reelSpeed"), 2400.f) * (1.f + FMath::Max(0.f, UWRPathComponent::Stat(Caster, TEXT("reelSpeed"))));
	const FName Tier = TdTier(Body);
	// Leviathan's hook on a boss or a mini: drag it and stagger it (once each 30 s a boss).
	if (Body && Line->bChain && (Tier == TEXT("boss") || Tier == TEXT("mini")))
	{
		static TMap<TWeakObjectPtr<AActor>, float> Last;
		const float Now = TdNow(Caster);
		const bool bBoss = Tier == TEXT("boss");
		if (!bBoss || Now - Last.FindRef(Body) >= T.Num(TEXT("passives.leviathansHook.bossCooldown"), 30.f))
		{
			Last.Add(Body, Now);
			const FVector Toward = (Caster->GetActorLocation() - Body->GetActorLocation()).GetSafeNormal2D();
			const float Drag = bBoss ? T.Num(TEXT("passives.leviathansHook.bossDrag"), 300.f) : T.Num(TEXT("passives.leviathansHook.miniDrag"), 500.f);
			Body->LaunchCharacter(Toward * Drag * 3.f, true, false);
			FWRHit Hit;
			Hit.Instigator = Caster;
			Hit.Element = WRTags::Element_Physical;
			Hit.PathId = TEXT("tide");
			Hit.StanceDamage = bBoss ? T.Num(TEXT("passives.leviathansHook.bossStance"), 60.f) : 30.f;
			Hit.bUnblockable = true;
			Hit.Origin = Caster->GetActorLocation();
			UWRCombatLibrary::ApplyHit(Body, Hit);
			if (Body->Status) Body->Status->Stagger(bBoss ? T.Num(TEXT("passives.leviathansHook.bossStagger"), 1.2f) : T.Num(TEXT("passives.leviathansHook.miniStagger"), 2.f));
			WRTidePath::Proc(Caster, TEXT("tide.leviathan_hook"), TEXT("tide.chain_clank"));
		}
		Line->Cut(false);
		return;
	}
	// Small bodies come to you along the line.
	if (Body && TdSmall(Tier))
	{
		const FVector To = Caster->GetActorLocation() - Body->GetActorLocation();
		const float Room = FMath::Max(0.f, To.Size() - Caster->GetSimpleCollisionRadius() - Body->GetSimpleCollisionRadius() - 80.f);
		WRWind::Knock(Caster, Body, To.GetSafeNormal(), Room, Room / FMath::Max(100.f, ReelSpeed), TEXT("harpoon_reel"));
		WRSound::PlayOn(Caster, TEXT("tide.reel"));
		FTimerHandle H;
		TWeakObjectPtr<AWRHarpoonLine> WL(Line);
		Caster->GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([WL]() { if (WL.IsValid()) WL->Cut(false); }), Room / FMath::Max(100.f, ReelSpeed) + 0.2f, false);
		return;
	}
	// A bigger body or a wall: Tow pulls you along the line (i-frames on the last 0.2 s; a fall resets on arrival).
	if (WRTidePath::Rank(Caster, TEXT("tow")) > 0)
	{
		const FVector Far = Line->FarEnd();
		const FVector To = Far - Caster->GetActorLocation();
		const float Dist = FMath::Max(0.f, FMath::Min(To.Size() - 120.f, T.Num(TEXT("passives.tow.range"), 1800.f)));
		const float Speed = T.Num(TEXT("passives.tow.speed"), 1800.f);
		const float Secs = Dist / FMath::Max(100.f, Speed);
		Caster->LaunchCharacter(To.GetSafeNormal() * Speed + FVector(0, 0, 200.f), true, true);
		UAbilitySystemComponent* ASC = Caster->GetAbilitySystemComponent();
		TWeakObjectPtr<AWRCharacterBase> WC(Caster);
		TWeakObjectPtr<AWRHarpoonLine> WL(Line);
		FTimerHandle H1, H2;
		const float IStart = FMath::Max(0.f, Secs - T.Num(TEXT("passives.tow.iframes"), 0.2f));
		Caster->GetWorldTimerManager().SetTimer(H1, FTimerDelegate::CreateLambda([WC]() { if (WC.IsValid()) WC->GetAbilitySystemComponent()->AddLooseGameplayTag(WRTags::State_Invulnerable); }), FMath::Max(0.01f, IStart), false);
		Caster->GetWorldTimerManager().SetTimer(H2, FTimerDelegate::CreateLambda([WC, WL]()
		{
			if (WC.IsValid()) { WC->GetAbilitySystemComponent()->RemoveLooseGameplayTag(WRTags::State_Invulnerable); WC->GetCharacterMovement()->Velocity *= 0.2f; }
			if (WL.IsValid()) WL->Cut(false);
		}), Secs + 0.05f, false);
		(void)ASC;
		WRTidePath::Proc(Caster, TEXT("tide.tow"), TEXT("tide.line_whine"));
		return;
	}
	// No Tow: the barb tears free.
	Line->Cut(false);
}

// ================================================================================================ the line

AWRHarpoonLine::AWRHarpoonLine()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	SetNetUpdateFrequency(10.f);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRHarpoonLine::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRHarpoonLine, Holder);
	DOREPLIFETIME(AWRHarpoonLine, Struck);
	DOREPLIFETIME(AWRHarpoonLine, StuckAt);
	DOREPLIFETIME(AWRHarpoonLine, bChain);
	DOREPLIFETIME(AWRHarpoonLine, bTaut);
	DOREPLIFETIME(AWRHarpoonLine, bAnchored);
	DOREPLIFETIME(AWRHarpoonLine, bTied);
	DOREPLIFETIME(AWRHarpoonLine, TiedAt);
	DOREPLIFETIME(AWRHarpoonLine, BornAt);
}

AWRHarpoonLine* AWRHarpoonLine::Make(AWRCharacterBase* InHolder, AActor* InStruck, const FVector& At, bool bInChain)
{
	if (!InHolder || !InHolder->HasAuthority()) return nullptr;
	// One line a thrower: a new one replaces the old.
	if (AWRHarpoonLine* Old = HeldBy(InHolder)) Old->Cut(false);
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWRHarpoonLine* L = InHolder->GetWorld()->SpawnActor<AWRHarpoonLine>(AWRHarpoonLine::StaticClass(), FTransform(At), P);
	if (!L) return nullptr;
	const FWRTuning& T = FWRTuning::Get();
	L->Holder = InHolder;
	L->Struck = InStruck;
	L->StuckAt = InStruck ? FVector(At - InStruck->GetActorLocation()) : At;
	L->bChain = bInChain;
	L->BornAt = TdNow(InHolder);
	L->ReelUntil = L->BornAt + T.Num(TEXT("wildbrands.harpoon_throw.reelWindow"), 1.5f);
	L->Lifetime = T.Num(TEXT("wildbrands.harpoon_throw.lineSeconds"), 20.f);
	L->BreakPull = T.Num(TEXT("wildbrands.harpoon_throw.lineHealth"), 600.f);
	return L;
}

FVector AWRHarpoonLine::FarEnd() const
{
	return Struck ? Struck->GetActorLocation() + FVector(StuckAt).GetClampedToMaxSize(60.f) : FVector(StuckAt);
}

FVector AWRHarpoonLine::NearEnd() const
{
	if (bTied) return TiedAt;
	const ACharacter* H = Cast<ACharacter>(Holder);
	if (!H) return GetActorLocation();
	if (const USkeletalMeshComponent* M = H->GetMesh(); M && M->DoesSocketExist(TEXT("weapon_r"))) return M->GetSocketLocation(TEXT("weapon_r"));
	return H->GetActorLocation() + FVector(0, 0, 40.f);
}

void AWRHarpoonLine::TieTo(const FVector& Ring)
{
	if (!HasAuthority()) return;
	bTied = true;
	TiedAt = Ring;
	bTaut = true;
	ReelUntil = TdNow(this);
	BornAt = TdNow(this);   // a tether's twenty seconds start when it's tied
	WRSound::PlayAt(this, TEXT("tide.rope_creak"), Ring);
}

void AWRHarpoonLine::AddPull(float Amount)
{
	if (!HasAuthority()) return;
	Pull += Amount;
	if (Pull >= BreakPull) Cut(true);
}

void AWRHarpoonLine::Cut(bool bSnap)
{
	if (!HasAuthority() || IsActorBeingDestroyed()) return;
	if (bSnap) WRFx::Broadcast(this, TEXT("tide.line_snap"), (NearEnd() + FarEnd()) * 0.5f, FRotator::ZeroRotator, 1.f, TEXT("tide.line_snap"));
	Destroy();
}

AWRHarpoonLine* AWRHarpoonLine::HeldBy(const AActor* InHolder)
{
	if (!InHolder) return nullptr;
	for (TActorIterator<AWRHarpoonLine> It(InHolder->GetWorld()); It; ++It) if (It->Holder == InHolder && !It->bTied && !It->IsActorBeingDestroyed()) return *It;
	return nullptr;
}

TArray<AWRHarpoonLine*> AWRHarpoonLine::On(const AActor* InStruck)
{
	TArray<AWRHarpoonLine*> Out;
	if (!InStruck) return Out;
	for (TActorIterator<AWRHarpoonLine> It(InStruck->GetWorld()); It; ++It) if (It->Struck == InStruck && !It->IsActorBeingDestroyed()) Out.Add(*It);
	return Out;
}

void AWRHarpoonLine::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority())
	{
		const float Now = TdNow(this);
		const AActor* H = Holder;
		const float Range = WRTune(TEXT("wildbrands.harpoon_throw.lineRange"), 1800.f) * 1.5f;
		const bool bHolderGone = !bTied && (!H || FVector::Dist(NearEnd(), FarEnd()) > Range);
		const AWRCharacterBase* Body = Cast<AWRCharacterBase>(Struck);
		const bool bBodyGone = Struck == nullptr ? false : (!Body || !Body->IsAlive());
		// Waiting for the reel: 1.5 s; untied and unreeled it lets go then (an anchor holds its 3 s; a tether its 20).
		const bool bExpired = (!bTied && !bAnchored && !bReeled && Now > ReelUntil + 0.2f) || Now - BornAt > Lifetime;
		if (bHolderGone || bBodyGone || bExpired) { Cut(bHolderGone && !bExpired); return; }
		// A tethered or anchored body straining against the line: it takes the pull (it breaks at 600).
		if ((bTied || bAnchored) && Body)
		{
			const float Stretch = FVector::Dist(NearEnd(), FarEnd()) - WRTune(TEXT("bosses.reef_tyrant.tetherRange"), 1000.f);
			if (Stretch > 0.f) AddPull(Body->GetVelocity().Size() * DeltaSeconds * 0.1f);
		}
	}
	if (GetNetMode() != NM_DedicatedServer)
	{
		DrawClock -= DeltaSeconds;
		if (DrawClock <= 0.f) { DrawClock = 0.03f; DrawLine(); }
	}
}

void AWRHarpoonLine::DrawLine()
{
	// A catenary from the hand to the barb: it sags while it waits, pulls straight when reeled or anchored (glowing
	// sea-green), and Leviathan's hook makes it a chain (thicker, iron).
	if (!Cable)
	{
		Cable = NewObject<UProceduralMeshComponent>(this, TEXT("Cable"));
		Cable->SetupAttachment(RootComponent);
		Cable->SetUsingAbsoluteLocation(true);
		Cable->SetUsingAbsoluteRotation(true);
		Cable->SetWorldLocation(FVector::ZeroVector);
		Cable->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Cable->RegisterComponent();
	}
	const FVector A = NearEnd(), B = FarEnd();
	const float L = FVector::Dist(A, B);
	const float SagCm = bTaut || bAnchored || bTied ? L * 0.005f : L * 0.06f;
	TArray<FVector> Pts;
	for (int32 i = 0; i <= 16; ++i)
	{
		const float T = i / 16.f;
		Pts.Add(FMath::Lerp(A, B, T) - FVector(0, 0, SagCm * 4.f * T * (1.f - T)));
	}
	FWRToolMesh M;
	const FLinearColor Rope = bChain ? FLinearColor(FColor(0x3A, 0x34, 0x30)) * 0.5f : (bAnchored || bTied ? FLinearColor(FColor(0x1E, 0x8C, 0x86)) : FLinearColor(FColor(0x9C, 0x84, 0x60)) * 0.5f);
	M.Tube(Pts, bChain ? 3.5f : 1.4f, 6, Rope, bChain ? 0.5f : 0.9f);
	M.ApplyTo(Cable);
}
