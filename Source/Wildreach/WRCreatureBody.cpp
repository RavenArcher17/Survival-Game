// A creature's body language over its clips (cooks/cookpass-creatures.md 3.1-3.3): what it looks at (its target, the
// one it's wary of, its owner, a curious glance, a scan), a flinch away from every blow, panting after effort, the
// wounded head, the stance-break slump, each mood's pose (graze, sniff, scan, preen, stare, threat, flee, eat) and each
// attack row's coil and release. Drawn on every machine but a dedicated server from replicated state; the numbers are in
// tuning anim.look / flinch / pant / wounded / slump / hungry / beat / gesture.

#include "WRCreature.h"
#include "WRAnimInstance.h"
#include "WRAttributeSet.h"
#include "WRCharacter.h"
#include "WRCombatTypes.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRTuning.h"
#include "Wildreach.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "EngineUtils.h"

namespace WRCreatureBodyDetail
{
	/** Eases toward a goal (frame-rate independent). */
	float Ease(float Value, float Goal, float DeltaSeconds, float Speed)
	{
		return FMath::FInterpTo(Value, Goal, DeltaSeconds, Speed);
	}

	/** The pawn of the survivor with this name (a tame's owner), on any machine. */
	APawn* PawnNamed(const UWorld* World, const FString& Name)
	{
		const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
		if (!GS || Name.IsEmpty())
		{
			return nullptr;
		}
		for (const APlayerState* PS : GS->PlayerArray)
		{
			if (PS && PS->GetPlayerName() == Name)
			{
				return PS->GetPawn();
			}
		}
		return nullptr;
	}

	/** A point on an actor to look at: a character's upper chest / face, anything else its middle. */
	FVector AimPoint(const AActor* At)
	{
		if (const ACharacter* C = Cast<ACharacter>(At))
		{
			return C->GetActorLocation() + FVector(0.f, 0.f, C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 0.6f);
		}
		return At->GetActorLocation();
	}

	/** One of a gesture's two poses: spineYaw, spinePitch, spineRoll, neck, headYaw (tuning anim.gesture.<motion>.<pose>). */
	void GesturePose(FName Motion, const TCHAR* Pose, float Out[5])
	{
		static const TCHAR* Keys[5] = { TEXT("spineYaw"), TEXT("spinePitch"), TEXT("spineRoll"), TEXT("neck"), TEXT("headYaw") };
		const FWRTuning& T = FWRTuning::Get();
		for (int32 i = 0; i < 5; ++i)
		{
			Out[i] = T.Num(FString::Printf(TEXT("anim.gesture.%s.%s.%s"), *Motion.ToString(), Pose, Keys[i]), 0.f);
		}
	}
}

void AWRCreature::SetupCreatureBody(const FWRCreatureDef& Def)
{
	UWRAnimInstance* Anim = CreatureAnim();
	BodyLang = FWRBodyLanguage();
	if (!Anim || !Skel->GetSkeletalMeshAsset())
	{
		return;
	}
	// The neck: the head and its parents named "neck" (generated rigs neck - head; the packs' Neck - Neck1 - Head;
	// Mixamo Neck - Head); under it the spine bones named "spine" / "chest". Root first.
	const FReferenceSkeleton& Ref = Skel->GetSkeletalMeshAsset()->GetRefSkeleton();
	const FName Head = HeadBone();
	const int32 HeadIndex = Head.IsNone() ? INDEX_NONE : Ref.FindBoneIndex(Head);
	TArray<FName> Neck, Spine;
	if (HeadIndex != INDEX_NONE && WRTune(TEXT("anim.look.enabled"), 1.f) > 0.f)
	{
		Neck.Add(Head);
		int32 B = Ref.GetParentIndex(HeadIndex);
		for (; B != INDEX_NONE && Ref.GetBoneName(B).ToString().Contains(TEXT("neck")) && Neck.Num() < 5; B = Ref.GetParentIndex(B))
		{
			Neck.Insert(Ref.GetBoneName(B), 0);
		}
		for (; B != INDEX_NONE && Spine.Num() < 4; B = Ref.GetParentIndex(B))
		{
			const FString Name = Ref.GetBoneName(B).ToString();
			if (!Name.Contains(TEXT("spine")) && !Name.Contains(TEXT("chest")))
			{
				break;
			}
			Spine.Insert(Ref.GetBoneName(B), 0);
		}
	}
	Anim->SetBodyChains(Neck, Spine);
	BodyLang.Head = Head;
	// Its own voice and its own rhythm, seeded as its animation phase is (its kind and where it came into the world):
	// packmates differ, the same animal stays itself.
	const FVector At = GetActorLocation();
	const FIntVector Spot(FMath::RoundToInt(At.X / 10.f), FMath::RoundToInt(At.Y / 10.f), FMath::RoundToInt(At.Z / 10.f));
	FRandomStream Random(int32(HashCombine(GetTypeHash(Spot), FCrc::StrCrc32(*Def.Id.ToString()) ^ 0x5bd1e995u)));
	BodyLang.Seed = Random.FRand();
	const float Spread = WRTune(TEXT("audio.voice.spread"), 0.06f);
	BodyLang.VoicePitch = 1.f + (Random.FRand() * 2.f - 1.f) * Spread;
	BodyLang.GestureSide = Random.FRand() < 0.5f ? 1 : -1;
	BodyLang.bReady = true;
}

float AWRCreature::GetVoicePitch() const
{
	return BodyLang.bReady ? BodyLang.VoicePitch : 1.f;
}

void AWRCreature::SetMood(EWRCreatureMood Mood, AActor* Watch)
{
	if (!HasAuthority())
	{
		return;
	}
	MoodSetAt = GetWorld()->GetTimeSeconds();
	if (MoodRep != static_cast<uint8>(Mood)) MoodRep = static_cast<uint8>(Mood);
	if (WatchRep != Watch) WatchRep = Watch;
}

void AWRCreature::TickMoodStale()
{
	// A biome's own brain took the turn (its own moments) or nothing refreshed it: back to its day.
	if ((MoodRep != 0 || WatchRep) && GetWorld()->GetTimeSeconds() - MoodSetAt > WRTune(TEXT("creatureAI.mood.staleSeconds"), 2.f))
	{
		MoodRep = 0;
		WatchRep = nullptr;
	}
}

void AWRCreature::MulticastRowGesture_Implementation(FName Motion, float Windup, int8 Side)
{
	if (GetNetMode() == NM_DedicatedServer || Motion.IsNone() || !IsAlive())
	{
		return;
	}
	// Only motions with a pose (tuning anim.gesture.<motion>.on); a creature just arriving here doesn't replay an old one.
	if (WRTune(*FString::Printf(TEXT("anim.gesture.%s.on"), *Motion.ToString()), 0.f) <= 0.f || GetGameTimeSinceCreation() < 0.5f)
	{
		return;
	}
	BodyLang.Gesture = Motion;
	BodyLang.GestureStart = GetWorld()->GetTimeSeconds();
	BodyLang.GestureWindup = FMath::Max(0.1f, Windup);
	BodyLang.GestureSide = Side < 0 ? -1 : 1;
}

void AWRCreature::KickFlinch()
{
	// A blow lands on the body: it jerks away from the side it came from (yaw), up when struck in front and down from
	// behind (pitch), the struck side lifting away (roll); an underdamped spring settles it with a small overshoot.
	if (GetNetMode() == NM_DedicatedServer || !IsAlive() || !BodyLang.bReady)
	{
		return;
	}
	const FWRCreatureDef* Def = GetDef();
	const FWRTuning& T = FWRTuning::Get();
	const UWRAttributeSet* A = GetAttributes();
	const float MaxHealth = A ? FMath::Max(1.f, A->GetMaxHealth()) : 100.f;
	float Strength = FMath::Clamp(HitDamageShown / MaxHealth * T.Num(TEXT("anim.flinch.damageGain"), 8.f), T.Num(TEXT("anim.flinch.min"), 0.35f), 1.f);
	if (Def && Def->Tier == TEXT("boss")) Strength *= T.Num(TEXT("anim.flinch.bossScale"), 0.35f);
	else if (Def && Def->bHyperarmor) Strength *= T.Num(TEXT("anim.flinch.hyperarmorScale"), 0.6f);
	if (WRFx::ReactionOf(HitFlagsShown) > 0) Strength = FMath::Min(1.5f, Strength * T.Num(TEXT("anim.flinch.poiseBreakMult"), 2.f));
	const FVector Local = GetActorTransform().InverseTransformVectorNoScale(HitOriginShown - GetActorLocation()).GetSafeNormal2D();
	if (Local.IsNearlyZero())
	{
		return;
	}
	const float SideOf = Local.Y >= 0.f ? 1.f : -1.f;   // + struck on its right
	const float Kick[3] = {
		-SideOf * float(FMath::Abs(Local.Y)) * T.Num(TEXT("anim.flinch.yaw"), 10.f) * Strength,
		(Local.X >= 0.f ? 1.f : -0.6f) * float(FMath::Abs(Local.X)) * T.Num(TEXT("anim.flinch.pitch"), 7.f) * Strength,
		SideOf * float(FMath::Abs(Local.Y)) * T.Num(TEXT("anim.flinch.roll"), 5.f) * Strength };
	const float Omega = T.Num(TEXT("anim.flinch.omega"), 18.f);
	for (int32 i = 0; i < 3; ++i)
	{
		// An impulse whose first swing peaks at about Kick.
		BodyLang.FlinchVel[i] += Kick[i] * Omega * 1.4f;
	}
}

FVector AWRCreature::BodyLookAt(bool& bOutHave, float& OutSpeed)
{
	using namespace WRCreatureBodyDetail;
	bOutHave = false;
	OutSpeed = WRTune(TEXT("anim.look.speed"), 5.f);
	UWorld* World = GetWorld();
	const double Now = World->GetTimeSeconds();
	const FVector Me = GetActorLocation();
	if (!IsAlive() || bAsleep || IsUnconscious() || bStanceBrokenShown)
	{
		return FVector::ZeroVector;
	}
	// Ridden: where its rider looks (the rider's own machine; elsewhere it looks ahead).
	if (const AWRCharacter* R = Rider.Get())
	{
		if (R->IsLocallyControlled() && R->GetController())
		{
			bOutHave = true;
			return Me + R->GetController()->GetControlRotation().Vector() * 3000.f;
		}
		return FVector::ZeroVector;
	}
	const EWRCreatureMood Mood = GetMood();
	const float Engaged = WRTune(TEXT("anim.look.engagedSpeed"), 9.f);
	// Its target: the one it's fighting (or displaying at).
	if (const AActor* Target = AggroRep.Get(); Target && Mood != EWRCreatureMood::Flee)
	{
		bOutHave = true;
		OutSpeed = Engaged;
		return AimPoint(Target);
	}
	// The one it's wary of, or (the first moments of flight) the one it runs from.
	if (const AActor* Watch = WatchRep.Get())
	{
		if (Mood == EWRCreatureMood::Wary || Mood == EWRCreatureMood::Threat
			|| (Mood == EWRCreatureMood::Flee && Now - BodyLang.MoodSince < WRTune(TEXT("anim.look.fleeLookBack"), 0.6f)))
		{
			bOutHave = true;
			OutSpeed = Engaged;
			return AimPoint(Watch);
		}
	}
	if (Mood == EWRCreatureMood::Flee || Mood == EWRCreatureMood::Eat || Mood == EWRCreatureMood::Graze || Mood == EWRCreatureMood::Sniff
		|| Mood == EWRCreatureMood::Preen)
	{
		return FVector::ZeroVector;   // the mood's own pose leads the head
	}
	// Scanning: held looks to each side (saccades: a quick move, then still).
	if (Mood == EWRCreatureMood::Scan)
	{
		if (Now >= BodyLang.NextScanAt)
		{
			static const float Steps[4] = { -50.f, 0.f, 45.f, 10.f };
			BodyLang.ScanYaw = Steps[BodyLang.ScanStep++ % 4] * BodyLang.GestureSide;
			BodyLang.NextScanAt = Now + FMath::FRandRange(WRTune(TEXT("anim.beat.scanHoldMin"), 1.f), WRTune(TEXT("anim.beat.scanHoldMax"), 1.5f));
		}
		bOutHave = true;
		OutSpeed = WRTune(TEXT("anim.beat.scanSpeed"), 12.f);
		const FRotator Look(WRTune(TEXT("anim.beat.scanPitch"), 4.f), GetActorRotation().Yaw + BodyLang.ScanYaw, 0.f);
		return Me + Look.Vector() * 2000.f;
	}
	// Calm: now and then a glance (a tame at its owner; a wild one at a survivor close by), held a moment.
	if (Now >= BodyLang.NextGlanceAt)
	{
		BodyLang.NextGlanceAt = Now + FMath::FRandRange(WRTune(TEXT("anim.look.glanceEveryMin"), 4.f), WRTune(TEXT("anim.look.glanceEveryMax"), 9.f));
		BodyLang.GlanceUntil = Now + FMath::FRandRange(WRTune(TEXT("anim.look.glanceMin"), 1.5f), WRTune(TEXT("anim.look.glanceMax"), 3.f));
		BodyLang.GlanceAt = nullptr;
		if (IsTamed())
		{
			BodyLang.GlanceAt = PawnNamed(World, TamedBy);
		}
		else
		{
			const float Curious = WRTune(TEXT("anim.look.curious"), 1200.f);
			float Best = Curious * Curious;
			for (TActorIterator<AWRCharacter> It(World); It; ++It)
			{
				const float D2 = float(FVector::DistSquared(It->GetActorLocation(), Me));
				if (It->IsAlive() && D2 < Best)
				{
					Best = D2;
					BodyLang.GlanceAt = *It;
				}
			}
		}
	}
	if (Now < BodyLang.GlanceUntil)
	{
		if (const AActor* G = BodyLang.GlanceAt.Get())
		{
			bOutHave = true;
			return AimPoint(G);
		}
	}
	return FVector::ZeroVector;
}

void AWRCreature::TickCreatureBody(float DeltaSeconds)
{
	using namespace WRCreatureBodyDetail;
	UWRAnimInstance* Anim = CreatureAnim();
	if (!Anim || !BodyLang.bReady || !Anim->HasBodyChains() || DeltaSeconds <= 0.f)
	{
		return;
	}
	const FWRCreatureDef* Def = GetDef();
	const FWRTuning& T = FWRTuning::Get();
	const double Now = GetWorld()->GetTimeSeconds();
	const bool bAlive = IsAlive();
	const bool bBoss = Def && Def->Tier == TEXT("boss");
	const float Time = float(FMath::Fmod(Now, 3600.0)) + BodyLang.Seed * 37.f;
	FWRCreatureBodyInput& Out = Anim->Input.Body;

	// The mood as this machine shows it (the server's MoodRep), and since when.
	const EWRCreatureMood Mood = bAlive ? GetMood() : EWRCreatureMood::Calm;
	if (Mood != BodyLang.MoodShown)
	{
		BodyLang.MoodShown = Mood;
		BodyLang.MoodSince = Now;
		BodyLang.ScanStep = 0;
		BodyLang.NextScanAt = 0.0;
		BodyLang.NextCheckAt = Now + FMath::FRandRange(2.f, 4.f);
	}

	// ---- Head aim ----
	bool bHave = false;
	float Speed = 5.f;
	const FVector Point = BodyLookAt(bHave, Speed);
	float YawGoal = 0.f, PitchGoal = 0.f, AlphaGoal = 0.f;
	if (bHave)
	{
		const FVector From = !BodyLang.Head.IsNone() ? Skel->GetSocketLocation(BodyLang.Head) : GetActorLocation();
		const FVector Local = GetActorTransform().InverseTransformVectorNoScale(Point - From);
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(float(Local.Y), float(Local.X)));
		const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(float(Local.Z), FMath::Max(1.f, float(Local.Size2D()))));
		// Behind it the look lets go (the body turns instead): full to 90 degrees off, none by 120.
		AlphaGoal = 1.f - FMath::Clamp((FMath::Abs(Yaw) - T.Num(TEXT("anim.look.fadeFrom"), 90.f)) / 30.f, 0.f, 1.f);
		const float MaxYaw = T.Num(TEXT("anim.look.maxYaw"), 70.f);
		YawGoal = FMath::Clamp(Yaw, -MaxYaw, MaxYaw);
		PitchGoal = FMath::Clamp(Pitch, -T.Num(TEXT("anim.look.maxPitchDown"), 40.f), T.Num(TEXT("anim.look.maxPitchUp"), 35.f));
	}
	// An attack or hit clip moves the head its own way: the look gives most of it back; a boss's big clips lead.
	if (Now < OneShotUntil) AlphaGoal *= T.Num(TEXT("anim.look.oneShotAlpha"), 0.35f);
	if (bBoss) AlphaGoal *= T.Num(TEXT("anim.look.bossAlpha"), 0.6f);
	BodyLang.LookYaw = Ease(BodyLang.LookYaw, YawGoal, DeltaSeconds, Speed);
	BodyLang.LookPitch = Ease(BodyLang.LookPitch, PitchGoal, DeltaSeconds, Speed);
	BodyLang.LookAlpha = Ease(BodyLang.LookAlpha, AlphaGoal, DeltaSeconds, T.Num(TEXT("anim.look.alphaSpeed"), 4.f));

	// ---- Flinch spring (spine yaw / pitch / roll), stepped in small slices so it stays stable at any frame rate ----
	const float Omega = T.Num(TEXT("anim.flinch.omega"), 18.f), Zeta = T.Num(TEXT("anim.flinch.damping"), 0.45f);
	for (float Left = FMath::Min(DeltaSeconds, 0.25f); Left > 0.f; Left -= 1.f / 120.f)
	{
		const float Dt = FMath::Min(Left, 1.f / 120.f);
		for (int32 i = 0; i < 3; ++i)
		{
			BodyLang.FlinchVel[i] += (-Omega * Omega * BodyLang.FlinchPos[i] - 2.f * Zeta * Omega * BodyLang.FlinchVel[i]) * Dt;
			BodyLang.FlinchPos[i] += BodyLang.FlinchVel[i] * Dt;
		}
	}

	// ---- Exertion and wounds: panting ----
	const float Health = GetHealthFraction();
	const bool bWounded = bAlive && !bAsleep && Health < T.Num(TEXT("anim.wounded.below"), 0.35f);
	const bool bRunning = GetVelocity().Size2D() > (Def ? Def->Speed : 450.f) * T.Num(TEXT("anim.pant.runShare"), 0.6f);
	const bool bEngaged = AggroRep.Get() != nullptr || Mood == EWRCreatureMood::Threat;
	const float Build = 1.f / FMath::Max(0.5f, T.Num(TEXT("anim.pant.buildSeconds"), 6.f));
	const float Recover = 1.f / FMath::Max(0.5f, T.Num(TEXT("anim.pant.recoverSeconds"), 10.f));
	BodyLang.Exertion = FMath::Clamp(BodyLang.Exertion + ((bRunning || bEngaged) && bAlive ? Build : -Recover) * DeltaSeconds, 0.f, 1.f);
	float Pant = bAlive && !bAsleep && !IsUnconscious() ? BodyLang.Exertion : 0.f;
	if (bWounded) Pant = FMath::Max(Pant, T.Num(TEXT("anim.wounded.pant"), 0.4f));
	const float Radius = FMath::Max(10.f, GetCapsuleComponent()->GetScaledCapsuleRadius());
	const float Hz = FMath::Clamp(T.Num(TEXT("anim.pant.hz"), 2.4f) * FMath::Pow(60.f / Radius, 0.35f), T.Num(TEXT("anim.pant.minHz"), 1.2f), T.Num(TEXT("anim.pant.maxHz"), 4.5f));
	BodyLang.PantPhase = FMath::Fmod(BodyLang.PantPhase + DeltaSeconds * Hz * 2.f * PI, 2.f * PI);
	const float Heave = FMath::Sin(BodyLang.PantPhase) * Pant * (bBoss ? 0.5f : 1.f);

	// ---- Stance-break slump (the held recoil clip plays under it) ----
	const float SlumpGoal = bAlive && bStanceBrokenShown ? 1.f : 0.f;
	const float SlumpSeconds = SlumpGoal > BodyLang.Slump ? T.Num(TEXT("anim.slump.inSeconds"), 0.4f) : T.Num(TEXT("anim.slump.outSeconds"), 0.5f);
	BodyLang.Slump = FMath::FInterpConstantTo(BodyLang.Slump, SlumpGoal, DeltaSeconds, 1.f / FMath::Max(0.05f, SlumpSeconds));

	// ---- The mood's pose ----
	float DipGoal = 0.f, SpineGoal = 0.f, RollGoal = 0.f, HeadYawGoal = 0.f, Bob = 0.f, Sweep = 0.f, Sway = 0.f;
	const float Since = float(Now - BodyLang.MoodSince);
	switch (Mood)
	{
	case EWRCreatureMood::Graze:
	{
		// Head down, chewing; every few seconds a head-up check (the vigilance of real grazers).
		if (Now >= BodyLang.NextCheckAt)
		{
			BodyLang.CheckUntil = Now + FMath::FRandRange(T.Num(TEXT("anim.beat.checkMin"), 1.f), T.Num(TEXT("anim.beat.checkMax"), 1.6f));
			BodyLang.NextCheckAt = BodyLang.CheckUntil + FMath::FRandRange(T.Num(TEXT("anim.beat.checkEveryMin"), 4.f), T.Num(TEXT("anim.beat.checkEveryMax"), 7.f));
		}
		const bool bCheck = Now < BodyLang.CheckUntil;
		DipGoal = bCheck ? T.Num(TEXT("anim.beat.checkLift"), 4.f) : -T.Num(TEXT("anim.beat.grazeDip"), 50.f);
		Bob = bCheck ? 0.f : FMath::Sin(Time * 2.f * PI * T.Num(TEXT("anim.beat.chewHz"), 1.6f)) * T.Num(TEXT("anim.beat.chew"), 3.f);
		HeadYawGoal = bCheck ? T.Num(TEXT("anim.beat.checkYaw"), 25.f) * BodyLang.GestureSide : 0.f;
		break;
	}
	case EWRCreatureMood::Sniff:
		DipGoal = -T.Num(TEXT("anim.beat.sniffDip"), 30.f);
		Sweep = FMath::Sin(Time * 2.f * PI * T.Num(TEXT("anim.beat.sniffHz"), 0.8f)) * T.Num(TEXT("anim.beat.sniffSweep"), 10.f);
		Bob = FMath::Sin(Time * 2.f * PI * T.Num(TEXT("anim.beat.sniffQuickHz"), 4.f)) * T.Num(TEXT("anim.beat.sniffQuick"), 1.2f);
		break;
	case EWRCreatureMood::Scan:
		DipGoal = T.Num(TEXT("anim.beat.scanLift"), 6.f);
		break;
	case EWRCreatureMood::Preen:
		DipGoal = -T.Num(TEXT("anim.beat.preenDip"), 25.f);
		HeadYawGoal = T.Num(TEXT("anim.beat.preenYaw"), 65.f) * BodyLang.GestureSide;
		RollGoal = T.Num(TEXT("anim.beat.preenRoll"), 8.f) * BodyLang.GestureSide + FMath::Sin(Time * 9.f) * 2.f;
		break;
	case EWRCreatureMood::Wary:
		DipGoal = T.Num(TEXT("anim.beat.waryLift"), 10.f);   // head high, everything still
		break;
	case EWRCreatureMood::Threat:
		DipGoal = -T.Num(TEXT("anim.beat.threatDip"), 12.f);
		SpineGoal = -T.Num(TEXT("anim.beat.threatShoulders"), 6.f);
		Sway = FMath::Sin(Time * 2.f * PI * T.Num(TEXT("anim.beat.threatSwayHz"), 0.5f)) * T.Num(TEXT("anim.beat.threatSway"), 3.f);
		break;
	case EWRCreatureMood::Flee:
		DipGoal = Since < 0.6f ? 0.f : T.Num(TEXT("anim.beat.fleeLift"), 4.f);
		break;
	case EWRCreatureMood::Eat:
		DipGoal = -T.Num(TEXT("anim.beat.eatDip"), 45.f);
		Bob = FMath::Sin(Time * 2.f * PI * T.Num(TEXT("anim.beat.eatHz"), 2.f)) * T.Num(TEXT("anim.beat.eatBob"), 6.f);
		HeadYawGoal = FMath::Sin(Time * 2.f * PI * 1.1f) * T.Num(TEXT("anim.beat.eatTear"), 5.f);
		break;
	default:
		break;
	}
	if (!bAlive)
	{
		DipGoal = SpineGoal = RollGoal = HeadYawGoal = Bob = Sweep = Sway = 0.f;
	}
	else
	{
		// Dozing, hungry or hurt, the head hangs lower.
		if (bAsleep) DipGoal -= T.Num(TEXT("anim.sleepDip"), 15.f);
		if (IsTamed() && TameRep.Hunger < T.Num(TEXT("anim.hungry.below"), 25.f) && Mood == EWRCreatureMood::Calm) DipGoal -= T.Num(TEXT("anim.hungry.dip"), 12.f);
		if (bWounded) DipGoal -= bBoss ? T.Num(TEXT("anim.wounded.bossDip"), 6.f) : T.Num(TEXT("anim.wounded.dip"), 14.f);
	}
	const float PoseSpeed = T.Num(TEXT("anim.beat.poseSpeed"), 3.f);
	BodyLang.Dip = Ease(BodyLang.Dip, DipGoal, DeltaSeconds, PoseSpeed);
	BodyLang.SpinePitch = Ease(BodyLang.SpinePitch, SpineGoal, DeltaSeconds, PoseSpeed);
	BodyLang.HeadRoll = Ease(BodyLang.HeadRoll, RollGoal, DeltaSeconds, PoseSpeed);
	BodyLang.HeadYaw = Ease(BodyLang.HeadYaw, HeadYawGoal, DeltaSeconds, PoseSpeed);

	// ---- The attack row's gesture: coil over the wind-up, release at the strike, back to rest ----
	float G[5] = { 0.f, 0.f, 0.f, 0.f, 0.f };
	if (BodyLang.GestureStart >= 0.0)
	{
		const float Tg = float(Now - BodyLang.GestureStart);
		const float W = BodyLang.GestureWindup;
		const float StrikeTime = T.Num(TEXT("anim.gesture.strikeSeconds"), 0.15f), Back = T.Num(TEXT("anim.gesture.returnSeconds"), 0.35f);
		if (!bAlive || Tg > W + StrikeTime + Back || HasTag(WRTags::Status_Staggered) || bStanceBrokenShown)
		{
			BodyLang.GestureStart = -1.0;
		}
		else
		{
			float Coil[5], Strike[5];
			GesturePose(BodyLang.Gesture, TEXT("coil"), Coil);
			GesturePose(BodyLang.Gesture, TEXT("strike"), Strike);
			for (int32 i = 0; i < 5; ++i)
			{
				float V;
				if (Tg <= W) V = Coil[i] * FMath::SmoothStep(0.f, 1.f, Tg / FMath::Max(0.05f, W * 0.8f));
				else if (Tg <= W + StrikeTime) V = FMath::Lerp(Coil[i], Strike[i], FMath::SmoothStep(0.f, 1.f, (Tg - W) / StrikeTime));
				else V = Strike[i] * (1.f - FMath::SmoothStep(0.f, 1.f, (Tg - W - StrikeTime) / Back));
				// Sideways terms (yaw, roll, head yaw) mirror to the side the server picked.
				G[i] = (i == 0 || i == 2 || i == 4) ? V * BodyLang.GestureSide : V;
			}
		}
	}

	// ---- All of it into the animation ----
	const float SlumpNeck = -T.Num(TEXT("anim.slump.neck"), 30.f) * BodyLang.Slump;
	Out.LookYaw = BodyLang.LookYaw;
	Out.LookPitch = BodyLang.LookPitch;
	Out.LookAlpha = BodyLang.LookAlpha;
	Out.NeckPitch = BodyLang.Dip + Bob + Heave * T.Num(TEXT("anim.pant.neck"), 2.5f) + SlumpNeck + G[3];
	Out.HeadYaw = BodyLang.HeadYaw + Sweep + G[4];
	Out.HeadRoll = BodyLang.HeadRoll + T.Num(TEXT("anim.slump.roll"), 12.f) * BodyLang.Slump * BodyLang.GestureSide;
	Out.SpineYaw = BodyLang.FlinchPos[0] + Sway + G[0];
	Out.SpinePitch = BodyLang.FlinchPos[1] + BodyLang.SpinePitch + Heave * T.Num(TEXT("anim.pant.spine"), 1.8f) - T.Num(TEXT("anim.slump.spine"), 10.f) * BodyLang.Slump + G[1];
	Out.SpineRoll = BodyLang.FlinchPos[2] + G[2];
}
