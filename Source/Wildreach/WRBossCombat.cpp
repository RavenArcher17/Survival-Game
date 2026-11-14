// Bosses in a fight (cooks/build-combat-full.md 2.16; cooks/combat.md 2.17): attack rows from Data/attacks.json with a
// small glint on the attacking part instead of floor paint, damage only while the part really travels (sweeping arcs,
// a body that runs its line, ground waves you can jump), delayed holds with a final cue, combos, punish windows, choice
// by range band, a transition attack at the phase change, breakable parts, a tame-punishing move, the roaming minis
// (Cinderhorn among his basalt pillars, Old Tusker on the snow flats), shrine-stones and the first-kill drop.
#include "WRBoss.h"
#include "Wildreach.h"
#include "WRAttributeSet.h"
#include "WRCharacter.h"
#include "WRCombatLibrary.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRInventory.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRProjectile.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRTuning.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

namespace WRBossDetail
{
	float DamageScale()
	{
		return WRTune(TEXT("bosses.damageScale"), 1.f) * FMath::Max(0.1f, WRTune(TEXT("server.creatureDifficulty"), 1.f));
	}

	/** Bearing of P from Here relative to Facing, in degrees (-180..180). */
	float Bearing(const FVector& Here, const FRotator& Facing, const FVector& P)
	{
		const FVector To = (P - Here).GetSafeNormal2D();
		return FMath::FindDeltaAngleDegrees(Facing.Yaw, To.Rotation().Yaw);
	}

	bool IsThrown(const FWRBossAttack& A)
	{
		return A.bRow ? A.Row.bThrown : (A.Id == TEXT("slag_throw") || A.Id == TEXT("boulder") || A.Id == TEXT("fire_bolt") || A.Id == TEXT("snow_fling"));
	}
}

// ---------------------------------------------------------------- rows

FWRBossAttack AWRBoss::MakeFromRow(const FWRAttackRow& R) const
{
	FWRBossAttack A;
	A.Id = R.Id;
	A.Shape = R.Shape == TEXT("line") ? EWRBossShape::Line : R.Shape == TEXT("self") ? EWRBossShape::SelfCircle
		: R.Shape == TEXT("target") ? EWRBossShape::TargetCircle : EWRBossShape::Cone;
	// Raven's per-boss keys (bosses.<id>.<attack>.*) still win, so tuning never needs the rows changed.
	const FString P = FString::Printf(TEXT("bosses.%s.%s."), *CreatureId.ToString(), *R.Id.ToString());
	const FWRTuning& T = FWRTuning::Get();
	A.Range = T.Num(P + TEXT("range"), R.Range);
	A.Size = T.Num(P + TEXT("size"), R.Size);
	A.Windup = T.Num(P + TEXT("windup"), R.Windup);
	A.Damage = T.Num(P + TEXT("damage"), R.Dmg);
	A.Cooldown = T.Num(P + TEXT("cooldown"), R.Cooldown);
	A.Poise = T.Num(P + TEXT("poise"), R.Poise);
	A.Buildup = R.Buildups.FindRef(TEXT("burn")) + R.Buildups.FindRef(TEXT("frost"));
	A.Repeats = R.Repeats;
	A.RepeatGap = R.Gap;
	A.MinPhase = R.Phase;
	A.Height = R.Height;
	A.bMelee = R.bMelee;
	A.bParryable = R.bParryable;
	A.bLow = R.bLow;
	A.bGrab = R.bGrab;
	A.bUnblockable = R.bUnblockable;
	A.Impact = R.Impact;
	A.Row = R;
	A.Row.Windup = A.Windup;
	A.bRow = true;
	return A;
}

float AWRBoss::Idle() const
{
	// Idle gaps between strings: phase 2 presses harder (combat.md 2.17.1).
	const FString Key = Phase >= 2 ? TEXT("bosses.idleGap.p2") : TEXT("bosses.idleGap.p1");
	return FMath::FRandRange(WRTune(*(Key + TEXT(".min")), Phase >= 2 ? 0.3f : 0.6f), WRTune(*(Key + TEXT(".max")), Phase >= 2 ? 0.9f : 1.4f));
}

int32 AWRBoss::ChooseAttack(AWRCharacterBase* Target, float Dist, AWRCharacterBase*& OutTarget)
{
	OutTarget = Target;
	const double Now = GetWorld()->GetTimeSeconds();
	const FWRTuning& T = FWRTuning::Get();
	// The phase change plays its transition attack first (once).
	if (PendingPhase > 0)
	{
		for (int32 i = 0; i < Attacks.Num(); ++i)
		{
			if (Attacks[i].bRow && Attacks[i].Row.bTransition && !TransitionsDone.Contains(Attacks[i].Id))
			{
				return i;
			}
		}
		SetPhase(PendingPhase);  // no transition move: the phase simply turns
		PendingPhase = 0;
	}
	const float Close = T.Num(TEXT("bosses.bands.close"), 400.f), Mid = T.Num(TEXT("bosses.bands.mid"), 1200.f);
	const int32 Cap = FMath::RoundToInt(Phase >= 2 ? T.Num(TEXT("bosses.projectilesInFlight.p2"), 2.f) : T.Num(TEXT("bosses.projectilesInFlight.p1"), 1.f));
	const bool bBehind = FVector::DotProduct(GetActorForwardVector().GetSafeNormal2D(), (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D()) < -0.3f;
	// The furthest tame in the fight, for the move that goes after tames (combat.md 2.17.1).
	AWRCharacterBase* FarTame = nullptr;
	float FarDist = 0.f;
	for (TActorIterator<AWRCreature> It(GetWorld()); It; ++It)
	{
		const float D = FVector::Dist2D(It->GetActorLocation(), GetActorLocation());
		if (It->IsTamed() && It->IsAlive() && IsHostileTo(*It) && FVector::Dist2D(It->GetActorLocation(), GetHome()) < ChaseRadius && D > FarDist)
		{
			FarDist = D;
			FarTame = *It;
		}
	}
	TArray<TPair<int32, float>> Options;
	float Total = 0.f;
	for (int32 i = 0; i < Attacks.Num(); ++i)
	{
		const FWRBossAttack& A = Attacks[i];
		const int32 MaxPhase = A.bRow ? A.Row.MaxPhase : 9;
		const bool bHeightOk = A.Height == 2 || (A.Height == 1) == bAirborne;
		if (A.MinPhase > Phase || Phase > MaxPhase || !bHeightOk || Now < A.ReadyAt || (A.bRow && (A.Row.bTransition || A.Row.Id == TEXT("herd_bellow"))))
		{
			continue;
		}
		// Never the same move a third time running; a cap on things in the air.
		if (A.Id == LastAttackId && SameInARow >= FMath::RoundToInt(T.Num(TEXT("bosses.maxRepeats"), 2.f)))
		{
			continue;
		}
		if (WRBossDetail::IsThrown(A) && InFlight >= Cap)
		{
			continue;
		}
		// What a broken part takes away: Vulkarn without his hammer fights with the tongs; the Tusker without tusks.
		const FString Id = A.Id.ToString();
		if (Now < NoHammerUntil && (Id.Contains(TEXT("hammer")) || Id.Contains(TEXT("slam")) || Id.Contains(TEXT("quench"))))
		{
			continue;
		}
		if (A.Id == TEXT("tusk_sweep") && IsPartBroken(TEXT("tusk_left")) && IsPartBroken(TEXT("tusk_right")))
		{
			continue;
		}
		if (A.bRow && A.Row.bBehindOnly != bBehind && A.Row.bBehindOnly)
		{
			continue;
		}
		AWRCharacterBase* Who = A.bRow && A.Row.bTamePunish && FarTame ? FarTame : Target;
		const float D = Who == Target ? Dist : FarDist;
		const float Reach = A.Shape == EWRBossShape::SelfCircle ? A.Size : A.Range;
		if (D > Reach + Who->GetSimpleCollisionRadius() + GetSimpleCollisionRadius())
		{
			continue;
		}
		float W = 1.f;
		if (A.bRow)
		{
			W = D < Close ? A.Row.WClose : D < Mid ? A.Row.WMid : A.Row.WFar;
			if (A.Row.bTamePunish && FarTame) W *= 2.f;
		}
		else
		{
			W = 1.f + A.Cooldown * 0.1f;  // built-in moves: the big ones a little more often, as before
		}
		if (W > 0.f)
		{
			Options.Add({ i, W });
			Total += W;
		}
	}
	float Roll = FMath::FRand() * Total;
	for (const TPair<int32, float>& O : Options)
	{
		Roll -= O.Value;
		if (Roll <= 0.f)
		{
			const FWRBossAttack& A = Attacks[O.Key];
			OutTarget = A.bRow && A.Row.bTamePunish && FarTame ? FarTame : Target;
			return O.Key;
		}
	}
	return Options.Num() > 0 ? Options.Last().Key : INDEX_NONE;
}

// ---------------------------------------------------------------- one attack

void AWRBoss::StartAttack(FWRBossAttack& Attack, AWRCharacterBase* Target)
{
	const double Now = GetWorld()->GetTimeSeconds();
	Current = Attacks.IndexOfByPredicate([&Attack](const FWRBossAttack& A) { return A.Id == Attack.Id; });
	RepeatsLeft = Attack.Repeats;
	RepeatIndex = 1;
	AttackTarget = Target;
	SameInARow = Attack.Id == LastAttackId ? SameInARow + 1 : 1;
	LastAttackId = Attack.Id;
	Attack.ReadyAt = Now + Attack.Cooldown * (CreatureId == TEXT("vulkarn") && Attack.Id == TEXT("anvil_slam") && Phase >= 2 ? 0.6f : 1.f);
	const FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
	AttackFacing = FRotator(0.f, ToTarget.Rotation().Yaw, 0.f);
	SetActorRotation(AttackFacing);
	// Thrown and slammed things land where the target stood when the wind-up began (never homing).
	AttackPoint = Target->GetActorLocation() - FVector(0.f, 0.f, Target->GetSimpleCollisionHalfHeight());
	const float Hold = Attack.bRow && (Attack.Row.HoldOn < 0.f || FMath::RoundToInt(Attack.Row.HoldOn) == RepeatIndex) ? Attack.Row.Hold : 0.f;
	StrikeStarted = Now;
	ResolveAt = Now + Attack.Windup + Hold;
	StrikeState = EStrike::Windup;
	bFinalCue = false;
	bThrown = false;
	if (Attack.bRow && Attack.Row.bTransition)
	{
		TransitionsDone.Add(Attack.Id);
	}

	// The cue is small (combat.md 2.17.1b): a glint on the attacking part (white blockable, crimson unblockable) and
	// its voice; the clip's own wind-up does the rest. Ground marks only for what's really coming down there: the
	// shadow of a thrown boulder or slag, of a diving wyrm, frost creeping where spikes will burst.
	const uint8 Glint = Attack.bRow ? Attack.Row.Glint : (Attack.bUnblockable || Attack.bGrab ? 2 : 1);
	MulticastTell(Glint, Attack.bRow ? Attack.Row.TellBone : FName(TEXT("hand")), TEXT("voice"));
	const float Flight = WRBossDetail::IsThrown(Attack) ? FVector::Dist2D(AttackPoint, GetActorLocation()) / FMath::Max(100.f, Attack.bRow ? Attack.Row.ProjectileSpeed : 1000.f) : 0.f;
	if (WRBossDetail::IsThrown(Attack) || Attack.Id == TEXT("dive"))
	{
		AWRGroundMarker::Spawn(GetWorld(), AttackPoint + FVector(0, 0, 3), AttackFacing, FVector2D(Attack.Size * 0.7f, Attack.Size * 0.7f), false,
			FLinearColor(0.02f, 0.02f, 0.025f), Attack.Windup + Hold + Flight + 0.2f);
	}
	else if (Attack.Id == TEXT("ice_spikes"))
	{
		AWRGroundMarker::Spawn(GetWorld(), AttackPoint + FVector(0, 0, 3), AttackFacing, FVector2D(Attack.Size, Attack.Size), false, FLinearColor(0.75f, 0.9f, 1.f), Attack.Windup + Hold);
	}
	PlaySwing(EWRSwingKind::Heavy, Attack.Windup + Hold + 0.2f);
	UE_LOG(LogWildreach, Log, TEXT("BOSS %s winds up %s at %s%s"), *GetDisplayLabel(), *Attack.Id.ToString(), *Target->GetName(), Hold > 0.f ? TEXT(" (held)") : TEXT(""));
}

void AWRBoss::ResolveAttack()
{
	BeginStrike();
}

void AWRBoss::BeginStrike()
{
	if (!Attacks.IsValidIndex(Current))
	{
		StrikeState = EStrike::None;
		return;
	}
	FWRBossAttack& Attack = Attacks[Current];
	const double Now = GetWorld()->GetTimeSeconds();
	const FWRTuning& T = FWRTuning::Get();
	StrikeState = EStrike::Strike;
	StrikeStarted = Now;
	const float Active = Attack.bRow ? FMath::Max(0.1f, Attack.Row.Active) : 0.25f;
	StrikeEnds = Now + Active;
	StrikeHits.Reset();
	StrikeFrom = GetActorLocation();
	SetCombatTag(WRTags::State_Windup, false);
	const FVector Floor = GetActorLocation() - FVector(0.f, 0.f, bAirborne ? 0.f : GetSimpleCollisionHalfHeight());
	const FString Id = Attack.Id.ToString();
	UE_LOG(LogWildreach, Log, TEXT("BOSS %s %s strikes"), *GetDisplayLabel(), *Id);

	if (WRBossDetail::IsThrown(Attack))
	{
		// The real thing flies: a lob from its hand to where the target stood, bursting where it lands.
		const FVector From = GetActorLocation() + GetActorForwardVector() * GetSimpleCollisionRadius() + FVector(0.f, 0.f, GetSimpleCollisionHalfHeight() * 0.6f);
		const float Speed = Attack.bRow ? Attack.Row.ProjectileSpeed : 1000.f;
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.Instigator = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FVector Velocity = AWRProjectile::LobVelocity(From, AttackPoint, Speed, 1.f);
		if (AWRProjectile* Lob = GetWorld()->SpawnActor<AWRProjectile>(AWRProjectile::StaticClass(), From, Velocity.Rotation(), Params))
		{
			FWRHit Hit;
			Hit.Instigator = this;
			Hit.Damage = Attack.Damage * WRBossDetail::DamageScale() * (Phase >= 3 ? Tune(TEXT("phase3DamageScale"), 1.4f) : 1.f);
			Hit.PoiseDamage = Attack.Poise;
			Hit.Impact = Attack.Impact;
			Hit.bUnblockable = Attack.bUnblockable;
			Hit.bHeavy = true;
			Hit.Tier = EWRHitTier::Heavy;
			Hit.Element = Attack.Buildup > 0.f ? BossElement() : WRTags::Element_Physical;
			(BossElement() == WRTags::Element_Fire ? Hit.BurnBuildup : Hit.FrostBuildup) = Attack.Buildup;
			const bool bRock = Attack.Id == TEXT("boulder");
			Lob->InitLob(Hit, Velocity, 1.f, Attack.Size, bRock ? FString(TEXT("/Game/SimpleRocks/3D_Assets/Meshes/Rocks/SM_SimpleRock_05A.SM_SimpleRock_05A")) : FString());
			++InFlight;
			const FName AttackId = Attack.Id;
			TWeakObjectPtr<AWRBoss> Weak(this);
			Lob->OnLanded = [Weak, AttackId](const FVector& At)
			{
				AWRBoss* Self = Weak.Get();
				if (!Self) return;
				Self->InFlight = FMath::Max(0, Self->InFlight - 1);
				const int32 Index = Self->Attacks.IndexOfByPredicate([AttackId](const FWRBossAttack& A) { return A.Id == AttackId; });
				if (Self->Attacks.IsValidIndex(Index)) Self->Aftermath(Self->Attacks[Index], At);
			};
		}
		bThrown = true;
		return;
	}
	if (Attack.Shape == EWRBossShape::Line && Attack.Id != TEXT("tongs_grab") && Attack.Id != TEXT("pin"))
	{
		// The body runs its line for real (never a teleport): a charge runs on its legs at the row's speed (the
		// movement's own collision and slopes); a lunge too quick to run is a leap. A wall or a pillar stops it.
		const FVector Dir = AttackFacing.Vector().GetSafeNormal2D();
		const float Length = Attack.Range * 0.85f;
		const float Speed = Attack.bRow && Attack.Row.Speed > 0.f ? Attack.Row.Speed : Length / Active;
		StrikeTo = GetActorLocation() + Dir * Length;
		StrikeEnds = Now + FMath::Clamp(Length / FMath::Max(100.f, Speed), 0.15f, FMath::Max(Active, 0.15f) * 2.5f);
		LineTravelled = 0.f;
		const FWRCreatureDef* Def = GetDef();
		if (Speed > T.Num(TEXT("bosses.leapAbove"), 1600.f) && !bAirborne)
		{
			bLineLeap = true;
			LaunchCharacter(Dir * Speed + FVector(0.f, 0.f, T.Num(TEXT("bosses.leapLift"), 350.f)), true, true);
		}
		else
		{
			bLineLeap = false;
			SpeedBoost = Def ? Speed / FMath::Max(100.f, Def->Speed) : 2.f;
		}
		return;
	}
	if (Attack.Id == TEXT("dive"))
	{
		// Down out of the sky onto where the target stood, the wing-beat shadow there since the wind-up.
		StrikeTo = AttackPoint + FVector(0.f, 0.f, GetSimpleCollisionHalfHeight() + 20.f);
		StrikeEnds = Now + FMath::Clamp(FVector::Dist(StrikeFrom, StrikeTo) / T.Num(TEXT("bosses.diveSpeed"), 3000.f), 0.3f, 1.2f);
		return;
	}
	// Slams and stomps: a wave of dust and grit (embers, snow) rolls out from where it struck; damage rides the front.
	const bool bWave = Attack.Shape == EWRBossShape::SelfCircle && !Id.Contains(TEXT("tail")) && !Id.Contains(TEXT("bellow"));
	const bool bPointSlam = Attack.Shape == EWRBossShape::TargetCircle;
	if (bWave || bPointSlam)
	{
		const FVector At = bPointSlam ? AttackPoint : Floor;
		const float Radius = Attack.Size;
		WRFx::Broadcast(this, TEXT("shockwave"), At, FRotator::ZeroRotator, Radius / 100.f, TEXT("stomp"));
		StrikeTo = At;
		StrikeEnds = FMath::Max(StrikeEnds, Now + Radius / T.Num(TEXT("bosses.waveSpeed"), 1200.f));
		if (Attack.bRow && Attack.Row.Knockback > 0.f)
		{
			WRFx::Broadcast(this, TEXT("roar.blast"), Floor, FRotator::ZeroRotator, Radius / 300.f, TEXT("boss.intro"));
		}
		return;
	}
	if (Id.Contains(TEXT("breath")))
	{
		// The breath sweeps across its cone over the strike (the boss turns its head with it).
		const FVector Mouth = GetActorLocation() + AttackFacing.Vector() * GetSimpleCollisionRadius() + FVector(0.f, 0.f, GetSimpleCollisionHalfHeight() * (bAirborne ? 0.f : 0.55f));
		const bool bFire = BossElement() == WRTags::Element_Fire;
		WRFx::Broadcast(this, bFire ? TEXT("breath.fire") : TEXT("breath.frost"), Mouth, AttackFacing, Attack.Range / 1000.f, bFire ? TEXT("firewall") : TEXT("frostnova"), Active);
	}
}

void AWRBoss::StrikeHit(AWRCharacterBase* Who)
{
	if (!Attacks.IsValidIndex(Current) || !Who || StrikeHits.Contains(Who) || !Who->IsAlive() || !IsHostileTo(Who) || Who->GetAttachParentActor())
	{
		return;
	}
	StrikeHits.Add(Who);
	FWRBossAttack& Attack = Attacks[Current];
	const double Now = GetWorld()->GetTimeSeconds();
	const bool bHiddenStrike = IsHidden() && Now > RevealedUntil;
	float Scale = (Phase >= 3 ? Tune(TEXT("phase3DamageScale"), 1.4f) : 1.f) * WRBossDetail::DamageScale();
	if (bHiddenStrike && CreatureId == TEXT("pale_hunter"))
	{
		Scale *= Tune(TEXT("ambushMultiplier"), 2.f);  // ambush from cover
		RevealedUntil = Now + 3.0;
	}
	FWRHit Hit;
	Hit.Instigator = this;
	Hit.Damage = Attack.Damage * Scale;
	Hit.PoiseDamage = Attack.Poise;
	Hit.Element = Attack.Buildup > 0.f || Attack.Id == TEXT("anvil_slam") ? BossElement() : WRTags::Element_Physical;
	(BossElement() == WRTags::Element_Fire ? Hit.BurnBuildup : Hit.FrostBuildup) = Attack.Buildup;
	Hit.bMelee = Attack.bMelee;
	Hit.bHeavy = true;
	Hit.bParryable = Attack.bParryable && Attack.bMelee;
	Hit.bLow = Attack.bLow;
	Hit.bGrab = Attack.bGrab;
	Hit.bUnblockable = Attack.bUnblockable;
	Hit.Impact = Attack.Impact;
	Hit.Tier = EWRHitTier::Heavy;
	Hit.PhysType = Attack.bRow ? Attack.Row.PhysType : 2;
	Hit.Origin = Attack.Shape == EWRBossShape::TargetCircle ? AttackPoint : GetActorLocation();
	const FName AttackId = Attack.Id;
	const float Knockback = Attack.bRow ? Attack.Row.Knockback : 0.f;
	const bool bFreezingBreath = Phase >= 3 && CreatureId == TEXT("skaldrith") && AttackId.ToString().Contains(TEXT("breath"));
	TWeakObjectPtr<AWRBoss> WeakSelf(this);
	TWeakObjectPtr<AWRCharacterBase> WeakWho(Who);
	// What follows the blow needs its real result: a blow on a remote player resolves once its defence is judged.
	UWRCombatLibrary::ApplyHit(Who, Hit, [WeakSelf, WeakWho, AttackId, bFreezingBreath, Knockback](EWRHitResult Landed)
	{
		AWRBoss* Self = WeakSelf.Get();
		AWRCharacterBase* Victim = WeakWho.Get();
		if (!Self || !Victim)
		{
			return;
		}
		if (AttackId == TEXT("tongs_grab"))
		{
			if (Landed == EWRHitResult::Parried)
			{
				// Parrying the tongs staggers him (longer than a normal parry; it counts toward his parry break too).
				if (!Self->HasTag(WRTags::Status_StanceBroken))
				{
					Self->Status->Stagger(Self->Tune(TEXT("grabParryStagger"), 4.f));
				}
				UE_LOG(LogWildreach, Log, TEXT("BOSS %s staggered: tongs grab parried"), *Self->GetDisplayLabel());
			}
			else if (Landed == EWRHitResult::Hit && Victim->IsAlive())
			{
				Victim->Status->Stagger(Self->Tune(TEXT("grabHoldSeconds"), 1.5f));
			}
		}
		if (AttackId == TEXT("pin") && Landed == EWRHitResult::Hit && Victim->IsAlive())
		{
			// Pinned under a claw, then the bite.
			Victim->Status->Stagger(Self->Tune(TEXT("pinSeconds"), 1.4f));
		}
		if (bFreezingBreath && Landed == EWRHitResult::Hit && Victim->IsAlive())
		{
			Victim->Status->ApplyStatus(WRTags::Status_Frozen, WRTune(TEXT("status.frozenSeconds"), 2.5f));
		}
		if (Knockback > 0.f && Victim->IsAlive() && (Landed == EWRHitResult::Hit || Landed == EWRHitResult::GuardBroken))
		{
			const FVector Push = (Victim->GetActorLocation() - Self->GetActorLocation()).GetSafeNormal2D() * Knockback + FVector(0.f, 0.f, Knockback * 0.3f);
			Victim->LaunchCharacter(Push, true, true);
		}
	});
}

void AWRBoss::TickStrike(float DeltaSeconds)
{
	if (!Attacks.IsValidIndex(Current))
	{
		return;
	}
	FWRBossAttack& Attack = Attacks[Current];
	const double Now = GetWorld()->GetTimeSeconds();
	const FWRTuning& T = FWRTuning::Get();
	const float Span = FMath::Max(0.05f, float(StrikeEnds - StrikeStarted));
	const float A = FMath::Clamp(float(Now - StrikeStarted) / Span, 0.f, 1.f);
	const float R = GetSimpleCollisionRadius();
	const FString Id = Attack.Id.ToString();
	if (bThrown)
	{
		return;  // the lob does it
	}
	const auto Hostiles = [this](TFunction<void(AWRCharacterBase*)> Fn)
	{
		for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
		{
			if (*It != this && It->IsAlive() && IsHostileTo(*It) && !It->GetAttachParentActor()) Fn(*It);
		}
	};

	if (Attack.Shape == EWRBossShape::Line && Attack.Id != TEXT("tongs_grab") && Attack.Id != TEXT("pin"))
	{
		// Running the line: whatever it meets on the way takes it; a pillar or a wall stops it.
		const FVector Dir = AttackFacing.Vector().GetSafeNormal2D();
		if (!bLineLeap)
		{
			AddMovementInput(Dir, 1.f);
		}
		LineTravelled += GetVelocity().Size2D() * DeltaSeconds;
		FHitResult Blocked;
		{
			// Stone ahead (static world only: bodies are run down, not walls).
			FCollisionQueryParams Q(SCENE_QUERY_STAT(WRBossLine), false, this);
			if (LineTravelled > T.Num(TEXT("bosses.wallAfter"), 200.f))
			{
				GetWorld()->SweepSingleByChannel(Blocked, GetActorLocation(), GetActorLocation() + Dir * (R + 40.f), FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeSphere(R * 0.6f), Q);
			}
		}
		Hostiles([&](AWRCharacterBase* Who)
		{
			if (FVector::Dist2D(Who->GetActorLocation(), GetActorLocation()) < R + Who->GetSimpleCollisionRadius() + Attack.Size * 0.4f) StrikeHit(Who);
		});
		// Cinderhorn's burning trail along his charge (not with the horn broken, nor for a while after frost).
		if (CreatureId == TEXT("cinderhorn") && Now >= NextTrailAt && !IsPartBroken(TEXT("horn")) && Now >= TrailOffUntil)
		{
			NextTrailAt = Now + T.Num(TEXT("bosses.cinderhorn.trailEvery"), 0.18f);
			const FVector Feet = GetActorLocation() - FVector(0.f, 0.f, GetSimpleCollisionHalfHeight());
			AWRHazard::Spawn(GetWorld(), this, Feet, Tune(TEXT("trailRadius"), 150.f), Tune(TEXT("trailSeconds"), 6.f), Tune(TEXT("trailDps"), 8.f), TEXT("fire"), false);
			WRFx::Broadcast(this, TEXT("firewall"), Feet, FRotator::ZeroRotator, Tune(TEXT("trailRadius"), 150.f) / 100.f, NAME_None, Tune(TEXT("trailSeconds"), 6.f));
		}
		// The Tusker's trample stops in the first body it meets once both tusks are gone.
		if (CreatureId == TEXT("old_tusker") && StrikeHits.Num() > 0 && IsPartBroken(TEXT("tusk_left")) && IsPartBroken(TEXT("tusk_right")))
		{
			StrikeEnds = Now;
		}
		if (Blocked.bBlockingHit)
		{
			const float Stun = Attack.bRow ? Attack.Row.WallStun : 0.f;
			AActor* What = Blocked.GetActor();
			if (AWRBossProp* Pillar = Cast<AWRBossProp>(What))
			{
				// A basalt pillar takes two charges, then it's rubble and the fight moves on along the loop.
				int32& Hits = PillarHits.FindOrAdd(Pillar);
				if (++Hits >= FMath::RoundToInt(T.Num(TEXT("bosses.cinderhorn.pillarCharges"), 2.f)) || Pillar->Kind != TEXT("basalt")) Pillar->Shatter();
				else WRFx::Broadcast(this, TEXT("part.break"), Blocked.ImpactPoint, FRotator::ZeroRotator, 1.4f, TEXT("part.break"));
			}
			if (Stun > 0.f)
			{
				// Into stone at full tilt: stunned, the weak point open (combat.md 2.17.4).
				UE_LOG(LogWildreach, Log, TEXT("BOSS %s crashes into %s"), *GetDisplayLabel(), *GetNameSafe(What));
				WRFx::Broadcast(this, TEXT("stomp"), Blocked.ImpactPoint, FRotator::ZeroRotator, 1.6f, TEXT("stomp"));
				EndStrike();
				Current = INDEX_NONE;
				StrikeState = EStrike::None;
				SpeedBoost = 1.f;
				GetCharacterMovement()->StopMovementImmediately();
				UWRCombatLibrary::BreakStance(this, Stun, nullptr);
				return;
			}
			StrikeEnds = Now;
		}
	}
	else if (Attack.Id == TEXT("tongs_grab") || Attack.Id == TEXT("pin"))
	{
		// The tongs (the claw) reach out along the line.
		const float Reach = FMath::Lerp(R, Attack.Range, A);
		const FVector Fwd = AttackFacing.Vector().GetSafeNormal2D();
		Hostiles([&](AWRCharacterBase* Who)
		{
			const FVector To = Who->GetActorLocation() - GetActorLocation();
			const float Along = FVector::DotProduct(To * FVector(1, 1, 0), Fwd);
			const float Side = FMath::Abs(FVector::CrossProduct(Fwd, To * FVector(1, 1, 0)).Z);
			if (Along > 0.f && Along <= Reach + Who->GetSimpleCollisionRadius() && Side <= Attack.Size + Who->GetSimpleCollisionRadius()) StrikeHit(Who);
		});
	}
	else if (Attack.Id == TEXT("dive"))
	{
		SetActorLocation(FMath::Lerp(StrikeFrom, StrikeTo, A * A), false);
	}
	else if (Attack.Shape == EWRBossShape::Cone)
	{
		// The swing (or the breath) sweeps from one side of its arc to the other while it travels.
		const float Edge = -Attack.Size + 2.f * Attack.Size * A;
		const bool bBreath = Id.Contains(TEXT("breath"));
		Hostiles([&](AWRCharacterBase* Who)
		{
			const float D = FVector::Dist2D(Who->GetActorLocation(), GetActorLocation()) - Who->GetSimpleCollisionRadius();
			const float B = WRBossDetail::Bearing(GetActorLocation(), AttackFacing, Who->GetActorLocation());
			const FVector Feet = Who->GetActorLocation() - FVector(0.f, 0.f, Who->GetSimpleCollisionHalfHeight());
			if (D <= Attack.Range + R && D >= R * 0.3f - Who->GetSimpleCollisionRadius() && B >= -Attack.Size - 8.f && B <= Edge + 8.f && !(bBreath && BreathBlocked(Feet)))
			{
				StrikeHit(Who);
			}
		});
	}
	else if (Attack.Shape == EWRBossShape::SelfCircle && Id.Contains(TEXT("tail")))
	{
		// The tail goes round: from behind one side, all the way round.
		const float Edge = -180.f + 360.f * A;
		Hostiles([&](AWRCharacterBase* Who)
		{
			const float D = FVector::Dist2D(Who->GetActorLocation(), GetActorLocation()) - Who->GetSimpleCollisionRadius();
			const float B = FMath::FindDeltaAngleDegrees(AttackFacing.Yaw + 180.f, (Who->GetActorLocation() - GetActorLocation()).Rotation().Yaw);
			if (D <= Attack.Size && B <= Edge) StrikeHit(Who);
		});
	}
	else if (Attack.Shape == EWRBossShape::SelfCircle || Attack.Shape == EWRBossShape::TargetCircle)
	{
		if (Id.Contains(TEXT("bellow")))
		{
			return;
		}
		// The wave's front: a ring rolling out at waveSpeed; inside the slam's own head (the first half metre) at once.
		const float Front = float(Now - StrikeStarted) * T.Num(TEXT("bosses.waveSpeed"), 1200.f);
		const float Depth = T.Num(TEXT("bosses.waveDepth"), 180.f);
		const FVector At = StrikeTo;
		Hostiles([&](AWRCharacterBase* Who)
		{
			const float D = FVector::Dist2D(Who->GetActorLocation(), At) - Who->GetSimpleCollisionRadius();
			if (D <= Attack.Size && D <= Front && (D >= Front - Depth || D < (Attack.Shape == EWRBossShape::TargetCircle ? Attack.Size * 0.3f : R + 60.f)))
			{
				StrikeHit(Who);
			}
		});
	}
}

void AWRBoss::Aftermath(const FWRBossAttack& Attack, const FVector& At)
{
	// What the blow leaves in the world (real ground effects: lava cracks, the flood, slick ice, pillars broken).
	const FString Id = Attack.Id.ToString();
	if (Id == TEXT("overhead_slam") || Id == TEXT("slag_throw") || Id == TEXT("quench_feint") || Id == TEXT("bellows_into_slam"))
	{
		AWRHazard::Spawn(GetWorld(), this, At, Id == TEXT("slag_throw") ? 160.f : 250.f, Tune(TEXT("crackSeconds"), 12.f), Tune(TEXT("crackDps"), 10.f), TEXT("lava"));
		if (Phase >= 3 && Id == TEXT("overhead_slam")) Flood();
	}
	if (Id == TEXT("anvil_slam"))
	{
		Flood();
	}
	if (Id == TEXT("forge_roar"))
	{
		// The forge floods with him (SPEC: then the floor runs with lava a long while; anvils are safe).
		const FVector Floor = GetHome() - FVector(0.f, 0.f, GetSimpleCollisionHalfHeight());
		AWRHazard::Spawn(GetWorld(), this, Floor, ArenaRadius, Tune(TEXT("transitionFloodSeconds"), 22.f), Tune(TEXT("floodDps"), 30.f), TEXT("flood"));
	}
	if (Id == TEXT("takeoff_gust"))
	{
		SetAirborne(true);
		SpecialClock = GetWorld()->GetTimeSeconds();
	}
	if (Id == TEXT("air_breath"))
	{
		AWRHazard::Spawn(GetWorld(), this, At, Attack.Size, Tune(TEXT("iceSeconds"), 10.f), 0.f, TEXT("ice"));
	}
	if (Id == TEXT("snow_fling"))
	{
		AWRHazard::Spawn(GetWorld(), this, At, Attack.Size * 0.8f, Tune(TEXT("snowSeconds"), 6.f), 0.f, TEXT("ice"));
	}
	if (Id == TEXT("dive"))
	{
		WRFx::Broadcast(this, TEXT("shockwave"), At, FRotator::ZeroRotator, Attack.Size / 100.f, TEXT("stomp"));
		SetAirborne(false);
		GroundedUntil = GetWorld()->GetTimeSeconds() + Tune(TEXT("diveGroundedSeconds"), 1.6f);
	}
	if (Id.Contains(TEXT("tail")) || Id == TEXT("lunge_bite"))
	{
		// Tail and charges shatter pillars they touch.
		for (const TWeakObjectPtr<AWRBossProp>& P : Props)
		{
			if (P.IsValid() && P->Kind == TEXT("pillar") && FVector::Dist2D(P->GetActorLocation(), GetActorLocation()) < P->Radius + GetSimpleCollisionRadius() + (Id.Contains(TEXT("tail")) ? Attack.Size : 100.f))
			{
				P->Shatter();
			}
		}
	}
	if (Id == TEXT("herd_bellow"))
	{
		// The herd answers: two drudges come up behind him (once a fight).
		for (int32 i = 0; i < FMath::RoundToInt(Tune(TEXT("bellowCalls"), 2.f)); ++i)
		{
			const FVector Spot = GetActorLocation() - GetActorForwardVector() * (GetSimpleCollisionRadius() + 600.f) + GetActorRightVector() * (i == 0 ? 500.f : -500.f);
			FHitResult Ground;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(WRBellow), false, this);
			if (GetWorld()->LineTraceSingleByChannel(Ground, Spot + FVector(0, 0, 2000), Spot - FVector(0, 0, 4000), ECC_WorldStatic, Q))
			{
				if (AWRCreature* Calf = AWRCreature::SpawnCreature(GetWorld(), TEXT("woolly_drudge"), Ground.ImpactPoint + FVector(0, 0, 300.f), GetActorRotation()))
				{
					Calf->SetAggroTarget(AttackTarget.Get());
				}
			}
		}
	}
}

void AWRBoss::EndStrike()
{
	if (!Attacks.IsValidIndex(Current))
	{
		StrikeState = EStrike::None;
		return;
	}
	FWRBossAttack& Attack = Attacks[Current];
	const double Now = GetWorld()->GetTimeSeconds();
	if (!bThrown)
	{
		Aftermath(Attack, Attack.Shape == EWRBossShape::TargetCircle ? AttackPoint : GetActorLocation() - FVector(0.f, 0.f, GetSimpleCollisionHalfHeight()));
	}
	UE_LOG(LogWildreach, Log, TEXT("BOSS %s %s done: %d hit"), *GetDisplayLabel(), *Attack.Id.ToString(), StrikeHits.Num());
	// The string goes on: the next repeat (its wind-up the gap; a held one if this is the held hit).
	if (++RepeatIndex <= Attack.Repeats)
	{
		const float Hold = Attack.bRow && FMath::RoundToInt(Attack.Row.HoldOn) == RepeatIndex ? Attack.Row.Hold : 0.f;
		StrikeState = EStrike::Windup;
		StrikeStarted = Now;
		ResolveAt = Now + Attack.RepeatGap + Hold;
		bFinalCue = false;
		bThrown = false;
		if (AWRCharacterBase* Target = AttackTarget.Get(); Target && Target->IsAlive())
		{
			AttackPoint = Target->GetActorLocation() - FVector(0.f, 0.f, Target->GetSimpleCollisionHalfHeight());
			if (Attack.Id == TEXT("ice_spikes"))
			{
				AWRGroundMarker::Spawn(GetWorld(), AttackPoint + FVector(0, 0, 3), AttackFacing, FVector2D(Attack.Size, Attack.Size), false, FLinearColor(0.75f, 0.9f, 1.f), Attack.RepeatGap + Hold);
			}
		}
		PlaySwing(EWRSwingKind::Heavy, Attack.RepeatGap + Hold + 0.2f);
		return;
	}
	// A combo's next move (claw, claw, then the lunge; the breath into the slam).
	if (Attack.bRow && !Attack.Row.Next.IsNone() && FMath::FRand() < Attack.Row.NextChance)
	{
		const int32 Next = Attacks.IndexOfByPredicate([&Attack](const FWRBossAttack& A) { return A.Id == Attack.Row.Next; });
		AWRCharacterBase* Target = AttackTarget.Get();
		if (Attacks.IsValidIndex(Next) && Target && Target->IsAlive() && Attacks[Next].MinPhase <= Phase)
		{
			StartAttack(Attacks[Next], Target);
			return;
		}
	}
	// The punish window always plays (the boss stands, open), then a breath before the next string.
	const bool bTransition = Attack.bRow && Attack.Row.bTransition;
	SpeedBoost = 1.f;
	Current = INDEX_NONE;
	StrikeState = EStrike::None;
	BusyUntil = Now + (Attack.bRow ? Attack.Row.Punish : Tune(TEXT("recoverySeconds"), 0.8f)) + Idle();
	if (bTransition && PendingPhase > 0)
	{
		SetPhase(PendingPhase);
		PendingPhase = 0;
	}
}

void AWRBoss::TickAttack(float DeltaSeconds)
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (!Attacks.IsValidIndex(Current))
	{
		return;
	}
	FWRBossAttack& Attack = Attacks[Current];
	if (StrikeState == EStrike::Windup)
	{
		// Tracking through the first part of the wind-up, then committed (a late step aside still works).
		AWRCharacterBase* Target = AttackTarget.Get();
		const float Track = Attack.bRow ? Attack.Row.TrackUntil : 0.5f;
		if (Target && Now - StrikeStarted < (ResolveAt - StrikeStarted) * Track && Attack.Shape != EWRBossShape::TargetCircle && Attack.Shape != EWRBossShape::SelfCircle)
		{
			const FRotator Want(0.f, (Target->GetActorLocation() - GetActorLocation()).Rotation().Yaw, 0.f);
			AttackFacing = FMath::RInterpConstantTo(AttackFacing, Want, DeltaSeconds, WRTune(TEXT("bosses.trackRate"), 120.f));
			SetActorRotation(AttackFacing);
		}
		// The final cue on a held strike: a second glint and a sting just before contact.
		const float Hold = Attack.bRow ? Attack.Row.Hold : 0.f;
		if (Hold > 0.f && !bFinalCue && Now >= ResolveAt - WRTune(TEXT("creatureAI.finalCueSeconds"), 0.2f)
			&& (Attack.Row.HoldOn < 0.f || FMath::RoundToInt(Attack.Row.HoldOn) == RepeatIndex))
		{
			bFinalCue = true;
			MulticastTell(Attack.Row.Glint, Attack.Row.TellBone, TEXT("tell.final"));
		}
		if (Now >= ResolveAt)
		{
			BeginStrike();
		}
		return;
	}
	if (StrikeState == EStrike::Strike)
	{
		TickStrike(DeltaSeconds);
		if (StrikeState == EStrike::Strike && Attacks.IsValidIndex(Current) && Now >= StrikeEnds)
		{
			EndStrike();
		}
	}
}

// ---------------------------------------------------------------- parts, roaming, the arena's furniture

void AWRBoss::OnPartBroken(const FWRCreatureDef::FBodyPoint& Part, AActor* By)
{
	// Each boss's own consequence (combat.md 2.10, 2.17).
	const double Now = GetWorld()->GetTimeSeconds();
	if (Part.OnBreak == TEXT("noHammer"))
	{
		NoHammerUntil = Now + Tune(TEXT("noHammerSeconds"), 4.f);
	}
	else if (Part.OnBreak == TEXT("grounded") && bAirborne)
	{
		SetAirborne(false);
		GroundedUntil = Now + Tune(TEXT("groundedSeconds"), 8.f);
		WingHits = 0.f;
	}
	else if (Part.OnBreak == TEXT("shell") && ShellHealth > 0.f)
	{
		ShellHealth = 0.f;
		ShellFraction = 0.f;
		SetBodyColor(FLinearColor(1.f, 0.55f, 0.1f));
	}
	// The part's material, once per fight, to everyone with a share of the fight.
	if (!Part.Drop.IsNone())
	{
		for (const TPair<FString, float>& Entry : Ledger)
		{
			for (APlayerState* P : GetWorld()->GetGameState()->PlayerArray)
			{
				AWRPlayerState* PS = Cast<AWRPlayerState>(P);
				if (PS && PS->GetPlayerName() == Entry.Key && PS->GetInventory())
				{
					PS->GetInventory()->AddItem(Part.Drop, 1);
					if (AWRPlayerController* PC = Cast<AWRPlayerController>(PS->GetOwner())) PC->ClientNotify(FString::Printf(TEXT("+1 %s"), *Part.Name));
				}
			}
		}
	}
}

void AWRBoss::TickRoam(float DeltaSeconds)
{
	// A roaming mini walks its loop until something comes at it; its home follows it, so a reset sends it on its way.
	const FWRCreatureDef* Def = GetDef();
	if (!Def || Def->Roam.Num() == 0 || bEngaged)
	{
		return;
	}
	const FVector Goal = Def->Roam[RoamIndex % Def->Roam.Num()];
	const FVector To = Goal - GetActorLocation();
	if (To.Size2D() < 300.f)
	{
		RoamIndex = (RoamIndex + 1) % Def->Roam.Num();
		return;
	}
	SetHome(FVector(Goal.X, Goal.Y, GetActorLocation().Z));
	bReturning = false;
	AddMovementInput(To.GetSafeNormal2D(), WRTune(TEXT("bosses.roamInput"), 0.45f));
}

void AWRBoss::SpawnRoamProps()
{
	// Cinderhorn's basalt pillars (creatures.json props): natural columns along his loop, rooted in the ash, the stone a
	// charge can be baited into (combat.md 2.17.4).
	const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("creatures.json"));
	const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
	if (!Root.IsValid() || !Root->TryGetArrayField(TEXT("creatures"), List))
	{
		return;
	}
	for (const TSharedPtr<FJsonValue>& V : *List)
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		const TSharedPtr<FJsonObject>* PropsObj = nullptr;
		if (!O.IsValid() || O->GetStringField(TEXT("id")) != CreatureId.ToString() || !O->TryGetObjectField(TEXT("props"), PropsObj))
		{
			continue;
		}
		const FString Kind = (*PropsObj)->GetStringField(TEXT("kind"));
		const TArray<TSharedPtr<FJsonValue>>* At = nullptr;
		if (!(*PropsObj)->TryGetArrayField(TEXT("at"), At)) continue;
		for (const TSharedPtr<FJsonValue>& P : *At)
		{
			const TArray<TSharedPtr<FJsonValue>>& XY = P->AsArray();
			if (XY.Num() < 2) continue;
			const FVector Spot(XY[0]->AsNumber(), XY[1]->AsNumber(), 0.f);
			FHitResult Ground;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(WRRoamProp), false, this);
			if (!GetWorld()->LineTraceSingleByChannel(Ground, Spot + FVector(0, 0, 60000), Spot - FVector(0, 0, 60000), ECC_WorldStatic, Q))
			{
				continue;  // its ground isn't loaded here yet
			}
			// Stood on its base, sunk into the ground (the column's own bounds, as AWRBossProp scales it).
			float Lift = 0.f;
			if (const UStaticMesh* Rock = Kind == TEXT("basalt") ? LoadObject<UStaticMesh>(nullptr, TEXT("/Game/SimpleRocks/3D_Assets/Meshes/Rocks/SM_SimpleRock_07A.SM_SimpleRock_07A")) : nullptr)
			{
				const FBox B = Rock->GetBoundingBox();
				Lift = -B.Min.Z * WRTune(TEXT("bosses.basaltHeight"), 320.f) / FMath::Max(1.f, B.GetSize().Z) - WRTune(TEXT("bosses.basaltSink"), 60.f);
			}
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (AWRBossProp* Prop = GetWorld()->SpawnActor<AWRBossProp>(AWRBossProp::StaticClass(), Ground.ImpactPoint + FVector(0.f, 0.f, Lift), FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), Params))
			{
				Prop->Setup(FName(*Kind));
				Props.Add(Prop);
			}
		}
	}
}

void AWRBoss::SpawnRunbackStone()
{
	// A shrine-stone 40-60 m out from the arena toward the way in (the island's middle, where the passes are), on
	// ground about level with the arena floor and clear of rock: the run back is the way you came.
	const FWRCreatureDef* Def = GetDef();
	if (!Def || Def->Roam.Num() > 0 || Runback.IsValid())
	{
		return;
	}
	const FVector ArenaHome = GetHome();
	const FVector Floor = ArenaHome - FVector(0.f, 0.f, GetSimpleCollisionHalfHeight());
	const FVector Toward = (FVector::ZeroVector - ArenaHome).GetSafeNormal2D();
	const float Dist = FMath::Lerp(Tune(TEXT("runbackMin"), WRTune(TEXT("bosses.runbackStone.min"), 4000.f)), Tune(TEXT("runbackMax"), WRTune(TEXT("bosses.runbackStone.max"), 6000.f)), 0.5f);
	for (int32 Try = 0; Try < 9; ++Try)
	{
		const float Turn = (Try == 0 ? 0.f : ((Try + 1) / 2) * 25.f * (Try % 2 ? 1.f : -1.f));
		const FVector Spot = ArenaHome + Toward.RotateAngleAxis(Turn, FVector::UpVector) * Dist;
		FHitResult Ground;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRRunback), false, this);
		if (!GetWorld()->LineTraceSingleByChannel(Ground, Spot + FVector(0, 0, 8000), Spot - FVector(0, 0, 8000), ECC_WorldStatic, Q)
			|| FMath::Abs(Ground.ImpactPoint.Z - Floor.Z) > Tune(TEXT("runbackMaxRise"), 1200.f) || Ground.ImpactNormal.Z < 0.85f
			|| Ground.GetActor()->IsA<AWRBossProp>())
		{
			continue;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		// It faces the arena: you see the fight's ground from where you wake.
		const FRotator Face(0.f, (ArenaHome - Ground.ImpactPoint).Rotation().Yaw, 0.f);
		if (AWRRunbackStone* Stone = GetWorld()->SpawnActor<AWRRunbackStone>(AWRRunbackStone::StaticClass(), Ground.ImpactPoint, Face, Params))
		{
			Stone->BossId = CreatureId;
			Runback = Stone;
			UE_LOG(LogWildreach, Log, TEXT("BOSS %s shrine-stone at %s"), *GetDisplayLabel(), *Ground.ImpactPoint.ToCompactString());
		}
		return;
	}
}

void AWRBoss::FirstKillDrops()
{
	const FWRCreatureDef* Def = GetDef();
	if (!Def || Def->FirstKillItem.IsNone())
	{
		return;
	}
	const float Needed = GetAttributes()->GetMaxHealth() * WRTune(TEXT("bosses.lootDamageFraction"), 0.1f);
	for (const TPair<FString, float>& Entry : Ledger)
	{
		if (Entry.Value < Needed) continue;
		for (APlayerState* P : GetWorld()->GetGameState()->PlayerArray)
		{
			AWRPlayerState* PS = Cast<AWRPlayerState>(P);
			if (!PS || PS->GetPlayerName() != Entry.Key || PS->BossKills.Contains(CreatureId)) continue;
			PS->BossKills.Add(CreatureId);
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (AWRFirstKillDrop* Drop = GetWorld()->SpawnActor<AWRFirstKillDrop>(AWRFirstKillDrop::StaticClass(), GetActorLocation(), FRotator::ZeroRotator, Params))
			{
				Drop->ForPawn = PS->GetPawn();
				Drop->Item = Def->FirstKillItem;
				Drop->BossName = Def->Name;
				Drop->OwnerName = PS->GetPlayerName();
			}
			UE_LOG(LogWildreach, Log, TEXT("BOSS %s first kill for %s: %s"), *GetDisplayLabel(), *PS->GetPlayerName(), *Def->FirstKillItem.ToString());
		}
	}
}

// ---------------------------------------------------------------- shrine-stone

AWRRunbackStone::AWRRunbackStone()
{
	bReplicates = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Stone = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Stone"));
	Stone->SetupAttachment(RootComponent);
	Stone->SetCollisionProfileName(TEXT("BlockAll"));
}

void AWRRunbackStone::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRRunbackStone, BossId);
}

void AWRRunbackStone::BeginPlay()
{
	Super::BeginPlay();
	// A standing stone from the rock set (the dressing's), stood upright 1.6 m tall, sunk a hand into the ground, in the
	// terrain's rock material so it belongs to the hillside; a pale rune glow on its face toward the arena.
	if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/SimpleRocks/3D_Assets/Meshes/Rocks/SM_SimpleRock_09A.SM_SimpleRock_09A")))
	{
		Stone->SetStaticMesh(Mesh);
		const FBox B = Mesh->GetBoundingBox();
		const FVector Size = B.GetSize();
		const float Tall = WRTune(TEXT("bosses.runbackStone.height"), 160.f), Wide = WRTune(TEXT("bosses.runbackStone.width"), 70.f);
		Stone->SetRelativeScale3D(FVector(Wide / FMath::Max(1.f, Size.X), Wide / FMath::Max(1.f, Size.Y), Tall / FMath::Max(1.f, Size.Z)));
		Stone->SetRelativeLocation(FVector(0.f, 0.f, -B.Min.Z * Tall / FMath::Max(1.f, Size.Z) - 12.f));
		if (UMaterialInterface* Rock = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_Rock.M_Rock"))) Stone->SetMaterial(0, Rock);
	}
	if (GetNetMode() != NM_DedicatedServer)
	{
		Rune = WRFx::Attach(Stone, TEXT("runback.rune"), NAME_None, 1.f);
		if (Rune) Rune->SetRelativeLocation(FVector(40.f, 0.f, 110.f));
		UPointLightComponent* Light = NewObject<UPointLightComponent>(this, TEXT("RuneLight"));
		Light->SetupAttachment(Stone);
		Light->SetRelativeLocation(FVector(60.f, 0.f, 110.f));
		Light->SetLightColor(FLinearColor(0.75f, 0.85f, 1.f));
		Light->SetIntensity(WRTune(TEXT("bosses.runbackStone.light"), 1800.f));
		Light->SetAttenuationRadius(400.f);
		Light->SetCastShadows(false);
		Light->RegisterComponent();
	}
}

FString AWRRunbackStone::Bind(AWRCharacter* Who)
{
	AWRPlayerState* PS = Who ? Who->GetPlayerState<AWRPlayerState>() : nullptr;
	if (!PS)
	{
		return FString();
	}
	if (Who->HasTag(WRTags::State_CombatTagged))
	{
		return TEXT("In combat: not now");
	}
	PS->BoundStones.AddUnique(this);
	PS->RespawnPoint = this;
	WRSound::Broadcast(Who, TEXT("discover"));
	const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(BossId);
	return FString::Printf(TEXT("You'll wake at the shrine-stone by %s"), Def ? *Def->Name : TEXT("the arena"));
}

// ---------------------------------------------------------------- first-kill drop

AWRFirstKillDrop::AWRFirstKillDrop()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetReplicateMovement(true);
}

void AWRFirstKillDrop::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRFirstKillDrop, ForPawn);
}

void AWRFirstKillDrop::BeginPlay()
{
	Super::BeginPlay();
	Born = GetWorld()->GetTimeSeconds();
	if (GetNetMode() != NM_DedicatedServer)
	{
		// A gold light (gold = yours): it rises from the body, hangs, then goes to its owner.
		Glow = WRFx::Attach(RootComponent, TEXT("firstkill.light"), NAME_None, 1.f);
		UPointLightComponent* Light = NewObject<UPointLightComponent>(this, TEXT("Light"));
		Light->SetupAttachment(RootComponent);
		Light->SetLightColor(FLinearColor(1.f, 0.8f, 0.4f));
		Light->SetIntensity(5000.f);
		Light->SetAttenuationRadius(700.f);
		Light->SetCastShadows(false);
		Light->RegisterComponent();
		WRSound::PlayOn(this, TEXT("discover"));
	}
}

void AWRFirstKillDrop::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority())
	{
		return;
	}
	const double Age = GetWorld()->GetTimeSeconds() - Born;
	APawn* To = ForPawn.Get();
	if (!To)
	{
		if (Age > 60.0) Destroy();
		return;
	}
	const FVector Chest = To->GetActorLocation() + FVector(0.f, 0.f, 40.f);
	const float Pickup = WRTune(TEXT("bosses.firstKillPickup"), 400.f);
	if (!bFlying)
	{
		// Rising from the body, then hanging there until it's time (or its owner comes close).
		const float Rise = WRTune(TEXT("bosses.firstKillRise"), 180.f);
		SetActorLocation(GetActorLocation() + FVector(0.f, 0.f, FMath::Max(0.f, Rise - float(Age) * Rise) * DeltaSeconds));
		if (Age >= WRTune(TEXT("bosses.firstKillFlySeconds"), 6.f) || FVector::Dist(Chest, GetActorLocation()) < Pickup)
		{
			bFlying = true;
		}
		return;
	}
	const FVector Next = FMath::VInterpConstantTo(GetActorLocation(), Chest, DeltaSeconds, WRTune(TEXT("bosses.firstKillFlySpeed"), 1200.f));
	SetActorLocation(Next);
	if (FVector::Dist(Next, Chest) < 40.f)
	{
		AWRPlayerState* PS = To->GetPlayerState<AWRPlayerState>();
		if (PS && PS->GetInventory())
		{
			PS->GetInventory()->AddItem(Item, 1);
			const FWRItemDef* Def = FWRItemRegistry::Get().Find(Item);
			if (AWRPlayerController* PC = Cast<AWRPlayerController>(To->GetController()))
			{
				// The name card: what it is, and whose it was.
				PC->ClientNotify(FString::Printf(TEXT("%s  —  from %s"), Def ? *Def->Name : *Item.ToString(), *BossName));
			}
		}
		WRFx::Broadcast(this, TEXT("discover"), Chest, FRotator::ZeroRotator, 1.f, TEXT("discover"));
		Destroy();
	}
}
