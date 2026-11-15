#include "WRAbilitiesBlood.h"
#include "Wildreach.h"
#include "WRAttributeSet.h"
#include "WRBloodPath.h"
#include "WRCharacter.h"
#include "WRCharacterBase.h"
#include "WRCombatLibrary.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRPathComponent.h"
#include "WRPlayerController.h"
#include "WRProjectile.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRTuning.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"

// ---------------------------------------------------------------- aim helpers (the same rules as the other paths' spells)

namespace WRBloodAim
{
	static const FLinearColor Garnet = FLinearColor(FColor(0x7A, 0x0A, 0x14));
	static const FLinearColor HexColor = FLinearColor(FColor(0x5A, 0x24, 0x78));

	/** What the caster has locked on to, if a spell can still go for it. */
	static AWRCharacterBase* Locked(const AWRCharacterBase* Caster)
	{
		const AWRPlayerController* PC = Caster ? Cast<AWRPlayerController>(Caster->GetController()) : nullptr;
		AWRCharacterBase* Target = PC ? PC->GetServerLock() : nullptr;
		return Target && Target->IsAlive() && !Target->IsHidden() && Caster->IsHostileTo(Target)
			&& FVector::Dist(Target->GetActorLocation(), Caster->GetActorLocation()) <= WRTune(TEXT("spells.lockRange"), 4000.f) ? Target : nullptr;
	}

	/** From the caster's chest through the point under the crosshair. */
	static FVector ThroughCrosshair(const AWRCharacterBase* Caster, const FVector& From)
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

	/** The aim from the chest: at the lock (led for Speed), else through the crosshair. */
	static FVector AimFrom(const AWRCharacterBase* Caster, const FVector& Chest, float Speed, AWRCharacterBase*& OutLock)
	{
		OutLock = Locked(Caster);
		if (OutLock)
		{
			const FVector Velocity = WRTune(TEXT("spells.lockLead"), 1.f) > 0.f && Speed > 0.f ? OutLock->GetVelocity() : FVector::ZeroVector;
			return (UWRCombatLibrary::LeadPoint(Chest, OutLock->GetActorLocation(), Velocity, FMath::Max(1.f, Speed)) - Chest).GetSafeNormal();
		}
		return ThroughCrosshair(Caster, Chest);
	}

	static FVector Feet(const AWRCharacterBase* Who)
	{
		return Who->GetActorLocation() - FVector(0.f, 0.f, Who->GetSimpleCollisionHalfHeight());
	}

	/** A line of the path's colour laid from A to B (the recipe runs along X over 100 cm x scale). */
	static void Line(const UObject* Ctx, FName Effect, const FVector& A, const FVector& B, FName Sound)
	{
		const FVector D = B - A;
		if (!D.IsNearlyZero())
		{
			WRFx::Broadcast(Ctx, Effect, A, D.Rotation(), D.Size() / 100.f, Sound);
		}
	}
}

// ---------------------------------------------------------------- the spell base

UWRGA_BloodSpell::UWRGA_BloodSpell()
{
	StaminaKey = TEXT("stamina.spell");
	StaminaFallback = 12.f;
	PathId = TEXT("blood");
	Element = WRBlood::Element_Blood;
	ActivationOwnedTags.AddTag(WRTags::State_Attacking);
	FGameplayTagContainer Blocked = ActivationBlockedTags;
	Blocked.AddTag(WRTags::State_Attacking);
	Blocked.AddTag(WRTags::State_Dodging);
	ActivationBlockedTags = Blocked;
}

float UWRGA_BloodSpell::Spell(const TCHAR* Field, float Fallback) const
{
	return FWRTuning::Get().Num(SpellKey + TEXT(".") + Field, Fallback);
}

bool UWRGA_BloodSpell::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags))
	{
		return false;
	}
	// HP is the resource (2.17): no cast with HP at or under the cost; the slot greys and shakes instead.
	const AWRCharacterBase* Who = ActorInfo ? Cast<AWRCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (UWRBloodPath::CanSpend(Who, Spell(TEXT("hpCost"), 0.f)))
	{
		return true;
	}
	if (Who && Who->IsLocallyControlled())
	{
		WRBlood::NoteRefused(Who);
	}
	return false;
}

void UWRGA_BloodSpell::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	Super::ApplyCost(Handle, ActorInfo, ActivationInfo);
	AWRCharacterBase* Who = ActorInfo ? Cast<AWRCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (Who && HasAuthority(&ActivationInfo))
	{
		UWRBloodPath::SpendBlood(Who, Spell(TEXT("hpCost"), 0.f));
	}
}

void UWRGA_BloodSpell::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
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
	// The shape (combat.md 2.13, tuning spells.shapes.<shape>): its wind-up (a spell's own windup over it), rooted or
	// walking at half speed, its recovery. The creatures that answer a cast read State.Casting.
	const FWRTuning& T = FWRTuning::Get();
	const FString ShapeRow = TEXT("spells.shapes.") + ShapeName;
	const float Windup = Spell(TEXT("windup"), T.Num(ShapeRow + TEXT(".windup"), 0.3f));
	const float Recovery = Spell(TEXT("recovery"), T.Num(ShapeRow + TEXT(".recovery"), 0.35f));
	if (T.Num(ShapeRow + TEXT(".roots"), 0.f) > 0.f) SetMoveRoot(true);
	else SetMoveSlow(true, true);
	Caster->PlaySwing(EWRSwingKind::Cast, Windup + 0.1f);
	Caster->SetCombatTag(WRTags::State_Casting, true);
	if (IsServer())
	{
		// The garnet gathering in the hand: blood drawn up out of the wrist.
		WRFx::BroadcastOn(Caster, TEXT("blood.cast"), TEXT("hand"), 1.f, CastSound, Windup + 0.1f);
	}
	After(Windup, [this]()
	{
		AWRCharacterBase* C = GetCharacter();
		if (!C)
		{
			return;
		}
		C->SetCombatTag(WRTags::State_Casting, false);
		if (IsServer() && C->IsAlive())
		{
			CastBlood(C);
		}
	});
	After(Windup + Recovery, [this]() { EndSelf(); });
}

void UWRGA_BloodSpell::FireBolt(AWRCharacterBase* Caster, FWRHit Hit, const FLinearColor& Color) const
{
	const float Speed = Spell(TEXT("speed"), 2600.f);
	const FVector Chest = Caster->GetActorLocation() + FVector(0.f, 0.f, 40.f);
	AWRCharacterBase* Lock = nullptr;
	const FVector Dir = WRBloodAim::AimFrom(Caster, Chest, Speed, Lock);
	const FVector SpawnAt = Chest + Dir * 70.f;
	Hit.Instigator = Caster;
	Hit.PathId = PathId;
	Hit.bSpell = true;
	Hit.Origin = Chest;

	// Point blank: a body between the chest and the muzzle takes the bolt at once.
	FHitResult Near;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(WRBloodMuzzle), false, Caster);
	if (Caster->GetWorld()->SweepSingleByChannel(Near, Chest, SpawnAt, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(14.f), Query))
	{
		AWRCharacterBase* Blocker = Cast<AWRCharacterBase>(Near.GetActor());
		if (Blocker && Caster->IsHostileTo(Blocker))
		{
			UWRCombatLibrary::ApplyHit(Blocker, Hit);
			return;
		}
	}
	FActorSpawnParameters Params;
	Params.Owner = Caster;
	Params.Instigator = Caster;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AWRProjectile* Bolt = Caster->GetWorld()->SpawnActor<AWRProjectile>(AWRProjectile::StaticClass(), SpawnAt, Dir.Rotation(), Params))
	{
		Bolt->Init(Hit, Speed, Color);
		if (Lock)
		{
			// The fire-frost RAISE 3 rule (lockHomingPlayers 0): it curves after a creature, never at a player.
			const float Homing = Lock->IsCreature() ? WRTune(TEXT("spells.lockHoming"), 6000.f) : Spell(TEXT("lockHomingPlayers"), 0.f);
			if (Homing > 0.f)
			{
				Bolt->HomeOn(Lock, Homing);
			}
		}
	}
}

// ---------------------------------------------------------------- Blood bolt

UWRGA_BloodBolt::UWRGA_BloodBolt()
{
	DisplayName = TEXT("Blood bolt"); NodeId = TEXT("blood_bolt");
	SpellKey = TEXT("spells.bloodBolt"); CooldownKey = TEXT("spells.bloodBolt.cooldown"); CooldownFallback = 1.f;
	ShapeName = TEXT("bolt"); CastSound = TEXT("blood.cast");
	bAllowedMounted = true;  // bolts fly from the saddle (combat.md 2.19)
}

void UWRGA_BloodBolt::CastBlood(AWRCharacterBase* Caster)
{
	FWRHit Hit;
	Hit.Damage = Spell(TEXT("damage"), 22.f);
	Hit.PoiseDamage = Spell(TEXT("poise"), 8.f);
	Hit.Element = WRBlood::Element_Blood;
	Hit.Buildups.Add(TEXT("bleed"), Spell(TEXT("bleedBuildup"), 30.f));
	WRSound::Broadcast(Caster, TEXT("blood.bolt"));
	FireBolt(Caster, Hit, WRBloodAim::Garnet);
}

// ---------------------------------------------------------------- Blood spear

UWRGA_BloodSpear::UWRGA_BloodSpear()
{
	DisplayName = TEXT("Blood spear"); NodeId = TEXT("blood_spear");
	SpellKey = TEXT("spells.bloodSpear"); CooldownKey = TEXT("spells.bloodSpear.cooldown"); CooldownFallback = 6.f;
	ShapeName = TEXT("lance"); CastSound = TEXT("blood.cast");
	bAllowedMounted = true;  // lances fly from the saddle too
}

void UWRGA_BloodSpear::CastBlood(AWRCharacterBase* Caster)
{
	// A spear of blood straight along the aim: it runs through the first three bodies in its line and stops at the world.
	const float Range = Spell(TEXT("range"), 1800.f), Width = Spell(TEXT("width"), 70.f);
	const int32 Pierce = FMath::Max(1, FMath::RoundToInt(Spell(TEXT("pierce"), 3.f)));
	const FVector Eye = Caster->GetActorLocation() + FVector(0.f, 0.f, 40.f);
	AWRCharacterBase* Lock = nullptr;
	const FVector Dir = WRBloodAim::AimFrom(Caster, Eye, 0.f, Lock);
	FVector End = Eye + Dir * Range;
	FHitResult Wall;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(WRBloodSpear), false, Caster);
	if (Caster->GetWorld()->LineTraceSingleByChannel(Wall, Eye, End, ECC_WorldStatic, Query))
	{
		End = Wall.ImpactPoint;
	}
	const FVector Seg = End - Eye;
	const float Len = FMath::Max(1.f, Seg.Size());
	TArray<TPair<float, AWRCharacterBase*>> Line;
	for (TActorIterator<AWRCharacterBase> It(Caster->GetWorld()); It; ++It)
	{
		AWRCharacterBase* Who = *It;
		if (Who == Caster || !Who->IsAlive() || Who->IsHidden() || !Caster->IsHostileTo(Who))
		{
			continue;
		}
		const FVector Loc = Who->GetActorLocation();
		const float Along = FVector::DotProduct(Loc - Eye, Seg / Len);
		if (Along < -Who->GetSimpleCollisionRadius() || Along > Len + Who->GetSimpleCollisionRadius())
		{
			continue;
		}
		const FVector OnLine = Eye + Seg / Len * FMath::Clamp(Along, 0.f, Len);
		const FVector Off = Loc - OnLine;
		if (FVector(Off.X, Off.Y, 0.f).Size() <= Width + Who->GetSimpleCollisionRadius() && FMath::Abs(Off.Z) <= Who->GetSimpleCollisionHalfHeight() + Width)
		{
			Line.Add({ Along, Who });
		}
	}
	Line.Sort([](const TPair<float, AWRCharacterBase*>& A, const TPair<float, AWRCharacterBase*>& B) { return A.Key < B.Key; });
	const int32 Count = FMath::Min(Pierce, Line.Num());
	// It flies to the last body it goes through (or its full length / the wall when fewer stand in it).
	const FVector Tip = Count > 0 && Count == Pierce ? Line[Count - 1].Value->GetActorLocation() : End;
	WRBloodAim::Line(Caster, TEXT("blood.spear"), Eye + Dir * 50.f, Tip, TEXT("blood.spear"));
	for (int32 i = 0; i < Count; ++i)
	{
		AWRCharacterBase* Who = Line[i].Value;
		FWRHit Hit;
		Hit.Instigator = Caster;
		Hit.Damage = Spell(TEXT("damage"), 34.f);
		Hit.PoiseDamage = Spell(TEXT("poise"), 20.f);
		Hit.Element = WRBlood::Element_Blood;
		Hit.Buildups.Add(TEXT("bleed"), Spell(TEXT("bleedBuildup"), 50.f));
		Hit.PathId = PathId;
		Hit.bSpell = true;
		Hit.PhysType = 3;   // it pierces
		Hit.Impact = 2;
		Hit.Origin = Eye;
		Hit.ImpactPoint = Who->GetActorLocation();
		UWRCombatLibrary::ApplyHit(Who, Hit);
	}
}

// ---------------------------------------------------------------- Blood boil

UWRGA_BloodBoil::UWRGA_BloodBoil()
{
	DisplayName = TEXT("Blood boil"); NodeId = TEXT("blood_boil");
	SpellKey = TEXT("spells.bloodBoil"); CooldownKey = TEXT("spells.bloodBoil.cooldown"); CooldownFallback = 12.f;
	ShapeName = TEXT("nova"); CastSound = TEXT("blood.cast");
}

void UWRGA_BloodBoil::CastBlood(AWRCharacterBase* Caster)
{
	const float Radius = Spell(TEXT("radius"), 600.f);
	const float MeterShare = Spell(TEXT("meterShare"), 0.3f), BurstShare = Spell(TEXT("burstShare"), 0.2f);
	WRFx::Broadcast(Caster, TEXT("blood.boil"), WRBloodAim::Feet(Caster), FRotator::ZeroRotator, Radius / 100.f, TEXT("blood.boil"));
	// Every enemy whose bleed is well built up (>= 30% of its threshold) or that has bled in the last few seconds: its
	// wounds flare and burst one after another (the pop chain), each for 20% of its bleed burst again.
	TArray<TWeakObjectPtr<AWRCharacterBase>> Boiling;
	for (TActorIterator<AWRCharacterBase> It(Caster->GetWorld()); It; ++It)
	{
		AWRCharacterBase* Who = *It;
		if (Who == Caster || !Who->IsAlive() || !Caster->IsHostileTo(Who) || FVector::Dist(Who->GetActorLocation(), Caster->GetActorLocation()) > Radius + Who->GetSimpleCollisionRadius())
		{
			continue;
		}
		const float Meter = Who->BuildupPct.IsValidIndex(WRStatus::Bleed) ? Who->BuildupPct[WRStatus::Bleed] / 100.f : 0.f;
		if (Meter >= MeterShare || Who->HasTag(WRTags::Status_Bleeding))
		{
			Boiling.Add(Who);
		}
	}
	TWeakObjectPtr<AWRCharacterBase> WeakCaster(Caster);
	const FName Path = PathId;
	for (int32 i = 0; i < Boiling.Num(); ++i)
	{
		const TWeakObjectPtr<AWRCharacterBase> WeakWho = Boiling[i];
		FTimerHandle H;
		Caster->GetWorld()->GetTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([WeakCaster, WeakWho, BurstShare, Path]()
		{
			AWRCharacterBase* C = WeakCaster.Get();
			AWRCharacterBase* Who = WeakWho.Get();
			if (!C || !Who || !Who->IsAlive())
			{
				return;
			}
			WRFx::BroadcastOn(Who, TEXT("blood.boil_pop"), NAME_None, 1.f, TEXT("blood.boil_pop"));
			FWRHit Hit;
			Hit.Instigator = C;
			Hit.Damage = UWRBloodPath::BleedBurst(Who, C) * BurstShare;
			Hit.PoiseDamage = 15.f;
			Hit.Element = WRBlood::Element_Blood;
			Hit.PathId = Path;
			Hit.bSpell = true;
			Hit.bUnblockable = true;   // it bursts from inside
			Hit.Impact = 1;
			Hit.Origin = Who->GetActorLocation();
			Hit.ImpactPoint = Who->GetActorLocation();
			UWRCombatLibrary::ApplyHit(Who, Hit);
		}), 0.05f + 0.07f * i, false);
	}
	UE_LOG(LogWildreach, Log, TEXT("BLOOD boil by %s: %d bleeding"), *Caster->GetName(), Boiling.Num());
}

// ---------------------------------------------------------------- Transfusion

UWRGA_Transfusion::UWRGA_Transfusion()
{
	DisplayName = TEXT("Transfusion"); NodeId = TEXT("transfusion");
	SpellKey = TEXT("spells.transfusion"); CooldownKey = TEXT("spells.transfusion.cooldown"); CooldownFallback = 10.f;
	ShapeName = TEXT("bolt"); CastSound = TEXT("blood.cast");
}

AWRCharacterBase* UWRGA_Transfusion::FindAlly(const AWRCharacterBase* Caster)
{
	if (!Caster || !Caster->GetWorld())
	{
		return nullptr;
	}
	const FWRTuning& T = FWRTuning::Get();
	const float Range = T.Num(TEXT("spells.transfusion.range"), 1200.f);
	const float CosCone = FMath::Cos(FMath::DegreesToRadians(T.Num(TEXT("spells.transfusion.cone"), 50.f) * 0.5f));
	const FVector Eye = Caster->GetActorLocation() + FVector(0.f, 0.f, 50.f);
	const FVector Aim = Caster->GetBaseAimRotation().Vector();
	const APlayerState* MyPS = Caster->GetPlayerState();
	AWRCharacterBase* Best = nullptr;
	float BestScore = -10.f;
	for (TActorIterator<AWRCharacterBase> It(Caster->GetWorld()); It; ++It)
	{
		AWRCharacterBase* Who = *It;
		if (Who == Caster || !Who->IsAlive() || Who->IsHidden() || Caster->IsHostileTo(Who))
		{
			continue;
		}
		// An ally: another survivor at peace with you, or your own tame.
		const bool bTame = MyPS && !Who->GetTameOwner().IsEmpty() && Who->GetTameOwner() == MyPS->GetPlayerName();
		if (!Who->GetPlayerState() && !bTame)
		{
			continue;
		}
		const FVector To = Who->GetActorLocation() - Eye;
		const float Dist = To.Size();
		const float Dot = FVector::DotProduct(To.GetSafeNormal(), Aim);
		if (Dist > Range + Who->GetSimpleCollisionRadius() || Dot < CosCone)
		{
			continue;
		}
		const float Score = Dot - Dist / (Range * 10.f);
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = Who;
		}
	}
	return Best;
}

bool UWRGA_Transfusion::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	// Someone to give it to: an ally within 12 m in front of you.
	return FindAlly(ActorInfo ? Cast<AWRCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr) != nullptr;
}

void UWRGA_Transfusion::CastBlood(AWRCharacterBase* Caster)
{
	AWRCharacterBase* Ally = FindAlly(Caster);
	if (!Ally)
	{
		// They stepped out of reach during the wind-up: the blood goes back where it came from.
		UWRBloodPath::Heal(Caster, Spell(TEXT("hpCost"), 20.f), false, nullptr);
		return;
	}
	const float Seconds = FMath::Max(0.1f, Spell(TEXT("seconds"), 1.5f));
	const float Total = Spell(TEXT("heal"), 30.f);
	const int32 Ticks = FMath::Max(1, FMath::RoundToInt(Seconds / 0.25f));
	WRSound::Broadcast(Caster, TEXT("blood.transfusion"));
	UE_LOG(LogWildreach, Log, TEXT("BLOOD transfusion %s -> %s (%.0f over %.1f s)"), *Caster->GetName(), *Ally->GetName(), Total, Seconds);
	TWeakObjectPtr<AWRCharacterBase> WeakCaster(Caster), WeakAlly(Ally);
	for (int32 i = 0; i < Ticks; ++i)
	{
		FTimerHandle H;
		Caster->GetWorld()->GetTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([WeakCaster, WeakAlly, Total, Ticks]()
		{
			AWRCharacterBase* C = WeakCaster.Get();
			AWRCharacterBase* A = WeakAlly.Get();
			if (!C || !A || !A->IsAlive())
			{
				return;
			}
			// A garnet stream from your hand into them.
			WRBloodAim::Line(C, TEXT("blood.transfusion"), C->GetActorLocation() + FVector(0.f, 0.f, 40.f) + C->GetActorForwardVector() * 40.f, A->GetActorLocation() + FVector(0.f, 0.f, 30.f), NAME_None);
			UWRBloodPath::Heal(A, Total / Ticks, false, nullptr);
		}), 0.01f + 0.25f * i, false);
	}
}

// ---------------------------------------------------------------- Crimson tide

UWRGA_CrimsonTide::UWRGA_CrimsonTide()
{
	DisplayName = TEXT("Crimson tide"); NodeId = TEXT("crimson_tide");
	SpellKey = TEXT("spells.crimsonTide"); CooldownKey = TEXT("spells.crimsonTide.cooldown"); CooldownFallback = 60.f;
	ShapeName = TEXT("wall"); CastSound = TEXT("blood.cast");
}

void UWRGA_CrimsonTide::CastBlood(AWRCharacterBase* Caster)
{
	const float Width = Spell(TEXT("width"), 600.f), Length = Spell(TEXT("length"), 1500.f), Speed = Spell(TEXT("speed"), 900.f), Height = Spell(TEXT("height"), 250.f);
	const FVector Dir = FRotator(0.f, Caster->GetBaseAimRotation().Yaw, 0.f).Vector();
	const FVector From = WRBloodAim::Feet(Caster) + Dir * 60.f;
	FWRHit Hit;
	Hit.Instigator = Caster;
	Hit.Damage = Spell(TEXT("damage"), 45.f);
	Hit.PoiseDamage = Spell(TEXT("poise"), 40.f);
	Hit.Element = WRBlood::Element_Blood;
	Hit.Buildups.Add(TEXT("bleed"), Spell(TEXT("bleedBuildup"), 60.f));
	Hit.PathId = PathId;
	Hit.bSpell = true;
	Hit.Impact = 3;
	Hit.Tier = EWRHitTier::Heavy;
	AWRBloodTide::SpawnTide(Caster, From, Dir, Hit, Width, Length, Speed, Height);
	// The wave: its recipe lays the wall along X (across your aim) and sends it forward along Y at the wave's speed.
	WRFx::Broadcast(Caster, TEXT("blood.tide"), From, FRotator(0.f, Dir.Rotation().Yaw - 90.f, 0.f), Width / 100.f, TEXT("blood.tide"));
	WRFx::Shake(Caster, 0.25f, From, Length);
	UE_LOG(LogWildreach, Log, TEXT("BLOOD crimson tide by %s"), *Caster->GetName());
}

AWRBloodTide::AWRBloodTide()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

AWRBloodTide* AWRBloodTide::SpawnTide(AWRCharacterBase* InCaster, const FVector& From, const FVector& InDir, const FWRHit& InHit, float InWidth, float InLength, float InSpeed, float InHeight)
{
	if (!InCaster || !InCaster->HasAuthority())
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWRBloodTide* Tide = InCaster->GetWorld()->SpawnActor<AWRBloodTide>(AWRBloodTide::StaticClass(), From, InDir.Rotation(), Params);
	if (Tide)
	{
		Tide->Caster = InCaster;
		Tide->Hit = InHit;
		Tide->Dir = InDir.GetSafeNormal2D();
		Tide->Width = InWidth;
		Tide->Length = InLength;
		Tide->Speed = FMath::Max(100.f, InSpeed);
		Tide->Height = InHeight;
	}
	return Tide;
}

void AWRBloodTide::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AWRCharacterBase* By = Caster.Get();
	if (!By || Travelled >= Length)
	{
		Destroy();
		return;
	}
	const float Step = Speed * DeltaSeconds;
	Travelled += Step;
	SetActorLocation(GetActorLocation() + Dir * Step);
	const FVector Front = GetActorLocation();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Dir);
	TArray<AWRCharacterBase*> Caught;
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		AWRCharacterBase* Who = *It;
		// Your own wave: allies and you pass through it.
		if (Who == By || Struck.Contains(Who) || !Who->IsAlive() || !By->IsHostileTo(Who))
		{
			continue;
		}
		const FVector Rel = Who->GetActorLocation() - Front;
		const float R = Who->GetSimpleCollisionRadius();
		if (FMath::Abs(FVector::DotProduct(Rel, Dir)) <= Step + R + 40.f && FMath::Abs(FVector::DotProduct(Rel, Right)) <= Width * 0.5f + R
			&& Rel.Z >= -Who->GetSimpleCollisionHalfHeight() * 2.f - 50.f && Rel.Z <= Height + Who->GetSimpleCollisionHalfHeight())
		{
			Caught.Add(Who);
		}
	}
	for (AWRCharacterBase* Who : Caught)
	{
		Struck.Add(Who);
		FWRHit Wave = Hit;
		Wave.Instigator = By;
		Wave.Origin = Front - Dir * 100.f;
		Wave.ImpactPoint = Who->GetActorLocation();
		UWRCombatLibrary::ApplyHit(Who, Wave);
	}
}

// ---------------------------------------------------------------- Hex of weakness, Doom

UWRGA_HexOfWeakness::UWRGA_HexOfWeakness()
{
	DisplayName = TEXT("Hex of weakness"); NodeId = TEXT("hex_of_weakness");
	SpellKey = TEXT("spells.hexOfWeakness"); CooldownKey = TEXT("spells.hexOfWeakness.cooldown"); CooldownFallback = 10.f;
	ShapeName = TEXT("bolt"); CastSound = TEXT("blood.hex");
	bAllowedMounted = true;
}

void UWRGA_HexOfWeakness::CastBlood(AWRCharacterBase* Caster)
{
	// No damage: a black-purple bolt that builds curse (creatures deal less; players lose path resource and deal less).
	FWRHit Hit;
	Hit.Damage = Spell(TEXT("damage"), 0.f);
	Hit.Element = WRBlood::Element_Blood;
	Hit.Buildups.Add(TEXT("curse"), Spell(TEXT("curseBuildup"), 80.f));
	FireBolt(Caster, Hit, WRBloodAim::HexColor);
}

UWRGA_Doom::UWRGA_Doom()
{
	DisplayName = TEXT("Doom"); NodeId = TEXT("doom");
	SpellKey = TEXT("spells.doom"); CooldownKey = TEXT("spells.doom.cooldown"); CooldownFallback = 20.f;
	ShapeName = TEXT("bolt"); CastSound = TEXT("blood.hex");
	bAllowedMounted = true;
}

void UWRGA_Doom::CastBlood(AWRCharacterBase* Caster)
{
	// The bolt carries the mark (WRBlood::DoomMarker); where it lands, the sigil starts counting (UWRBloodPath::MarkDoom).
	FWRHit Hit;
	Hit.Damage = 0.f;
	Hit.Element = WRBlood::Element_Blood;
	Hit.Buildups.Add(WRBlood::DoomMarker, 1.f);
	FireBolt(Caster, Hit, WRBloodAim::HexColor);
}

// ---------------------------------------------------------------- Sanguine lord

UWRGA_SanguineLord::UWRGA_SanguineLord()
{
	DisplayName = TEXT("Sanguine lord"); NodeId = TEXT("sanguine_lord");
	SpellKey = TEXT("spells.sanguineLord"); CooldownKey = TEXT("spells.sanguineLord.cooldown"); CooldownFallback = 90.f;
	ShapeName = TEXT("buff"); CastSound = TEXT("blood.cast");
}

void UWRGA_SanguineLord::CastBlood(AWRCharacterBase* Caster)
{
	const float Seconds = Spell(TEXT("seconds"), 10.f);
	Caster->Status->ApplyStatus(WRBlood::Status_SanguineLord, Seconds);
	// The lord's own surge as it takes hold (blood.lord), then for the whole time the veins light garnet (the body,
	// WRBloodPath ApplyLook), a faint garnet aura, a heartbeat.
	WRSound::Broadcast(Caster, TEXT("blood.lord"));
	WRFx::BroadcastOn(Caster, TEXT("blood.sanguine"), NAME_None, 1.f, TEXT("blood.heartbeat"), Seconds);
	UE_LOG(LogWildreach, Log, TEXT("BLOOD sanguine lord on %s for %.0f s"), *Caster->GetName(), Seconds);
}

// ---------------------------------------------------------------- Wildbrands

UWRGA_BloodBrand::UWRGA_BloodBrand()
{
	PathId = TEXT("blood");
	StaminaKey = TEXT("stamina.weaponArt"); StaminaFallback = 18.f;
	TimingKey = TEXT("weapon.lightTiming"); SwingKind = EWRSwingKind::WeaponArt;
	Element = WRBlood::Element_Blood;
	StanceMultiplier = 1.5f;  // combat.md 2.12
}

bool UWRGA_BloodBrand::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags))
	{
		return false;
	}
	const AWRCharacterBase* Who = ActorInfo ? Cast<AWRCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (UWRBloodPath::CanSpend(Who, UWRBloodPath::HpCost(NodeId)))
	{
		return true;
	}
	if (Who && Who->IsLocallyControlled())
	{
		WRBlood::NoteRefused(Who);
	}
	return false;
}

void UWRGA_BloodBrand::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	Super::ApplyCost(Handle, ActorInfo, ActivationInfo);
	AWRCharacterBase* Who = ActorInfo ? Cast<AWRCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (Who && HasAuthority(&ActivationInfo))
	{
		UWRBloodPath::SpendBlood(Who, UWRBloodPath::HpCost(NodeId));
	}
}

void UWRGA_BloodBrand::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (IsServer() && IsActive())
	{
		// The blade runs garnet for the swing.
		WRFx::BroadcastOn(GetCharacter(), TEXT("blood.weapon"), TEXT("weapon"), 1.f, NodeId == TEXT("leech_strike") ? FName(TEXT("blood.leech")) : FName(TEXT("blood.slash")), 0.6f);
	}
}

UWRGA_CrimsonSlash::UWRGA_CrimsonSlash()
{
	DisplayName = TEXT("Crimson slash"); NodeId = TEXT("crimson_slash");
	CooldownKey = TEXT("spells.crimsonSlash.cooldown"); CooldownFallback = 2.5f;
	ArtKey = TEXT("spells.crimsonSlash");
	ActivationOwnedTags.AddTag(WRBlood::Ability_CrimsonSlash);
}

UWRGA_LeechStrike::UWRGA_LeechStrike()
{
	DisplayName = TEXT("Leech strike"); NodeId = TEXT("leech_strike");
	CooldownKey = TEXT("spells.leechStrike.cooldown"); CooldownFallback = 4.f;
	ArtKey = TEXT("spells.leechStrike");
	ActivationOwnedTags.AddTag(WRBlood::Ability_LeechStrike);
}
