#include "WRLag.h"
#include "Wildreach.h"
#include "WRCharacterBase.h"
#include "WRCombatLibrary.h"
#include "WRGameplayTags.h"
#include "WRTuning.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"

namespace WRLagDetail
{
	const FGameplayTag* DefenceTags()
	{
		static FGameplayTag Tags[4];
		static bool bInit = false;
		if (!bInit)
		{
			Tags[0] = WRTags::State_Invulnerable;
			Tags[1] = WRTags::State_Parrying;
			Tags[2] = WRTags::State_Blocking;
			Tags[3] = WRTags::State_JumpLegsInvuln;
			bInit = true;
		}
		return Tags;
	}
	constexpr int32 NumDefenceTags = 4;
}

UWRLagSubsystem* UWRLagSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UWRLagSubsystem>() : nullptr;
}

bool UWRLagSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	// Only where blows are judged: a server (dedicated or listen) or a standalone game; never a pure client.
	return World && World->IsGameWorld() && World->GetNetMode() != NM_Client;
}

TStatId UWRLagSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWRLagSubsystem, STATGROUP_Tickables);
}

float UWRLagSubsystem::OneWay(const AWRCharacterBase* Who)
{
	if (!Who || !Who->IsPlayerControlled() || Who->IsLocallyControlled())
	{
		return 0.f;   // creatures, and the host's own body: the server sees what it does as it does it
	}
	const APlayerState* PS = Who->GetPlayerState();
	return PS ? FMath::Clamp(PS->GetPingInMilliseconds() * 0.0005f, 0.f, 0.5f) : 0.f;
}

float UWRLagSubsystem::SeenDelay(const AWRCharacterBase* Viewer)
{
	const float One = OneWay(Viewer);
	if (One <= 0.f)
	{
		return 0.f;
	}
	return FMath::Min(One + WRTune(TEXT("net.interpSeconds"), 0.1f), WRTune(TEXT("net.rewindMax"), 0.25f));
}

float UWRLagSubsystem::WindowTolerance(const AWRCharacterBase* Who)
{
	const float One = OneWay(Who);
	return One > 0.f ? FMath::Min(One + 0.05f, WRTune(TEXT("net.rewindMax"), 0.25f)) : 0.f;
}

void UWRLagSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	const double Keep = FMath::Max(0.3f, WRTune(TEXT("net.history"), 1.f));

	// Bodies, at the sample rate.
	if (Now >= NextSample)
	{
		NextSample = Now + 1.0 / FMath::Max(5.f, WRTune(TEXT("net.sampleHz"), 30.f));
		for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
		{
			AWRCharacterBase* Who = *It;
			if (!Who->IsAlive())
			{
				continue;
			}
			FTrack& Track = Tracks.FindOrAdd(Who);
			Watch(Who, Track);
			FSample& S = Track.Samples.AddDefaulted_GetRef();
			S.Time = Now;
			S.Centre = Who->GetActorLocation();
			S.Rotation = Who->GetActorRotation();
			Who->GetCapsuleComponent()->GetScaledCapsuleSize(S.Radius, S.HalfHeight);
			int32 Old = 0;
			while (Old < Track.Samples.Num() - 2 && Track.Samples[Old + 1].Time < Now - Keep) ++Old;
			if (Old > 0) Track.Samples.RemoveAt(0, Old, EAllowShrinking::No);
			Track.Defences.RemoveAll([Now, Keep](const FSpan& D) { return D.End >= 0.0 && D.End < Now - Keep; });
		}
		// Forget the gone.
		for (auto It = Tracks.CreateIterator(); It; ++It)
		{
			if (!It.Key().IsValid())
			{
				if (UAbilitySystemComponent* ASC = It.Value().Watched.Get())
				{
					for (int32 i = 0; i < It.Value().Handles.Num() && i < WRLagDetail::NumDefenceTags; ++i)
					{
						ASC->RegisterGameplayTagEvent(WRLagDetail::DefenceTags()[i], EGameplayTagEventType::NewOrRemoved).Remove(It.Value().Handles[i]);
					}
				}
				It.RemoveCurrent();
			}
		}
	}

	// Held blows whose wait is over.
	for (int32 i = 0; i < Held.Num(); )
	{
		if (Now < Held[i].ResolveAt)
		{
			++i;
			continue;
		}
		FHeld Due = MoveTemp(Held[i]);
		Held.RemoveAtSwap(i);
		AWRCharacterBase* Target = Due.Target.Get();
		const EWRHitResult Result = Target && Target->IsAlive() ? UWRCombatLibrary::ApplyHit(Target, Due.Hit) : EWRHitResult::Ignored;
		if (Due.OnResolved)
		{
			Due.OnResolved(Result);
		}
	}
}

void UWRLagSubsystem::Watch(AWRCharacterBase* Who, FTrack& Track)
{
	UAbilitySystemComponent* ASC = Who->GetAbilitySystemComponent();
	if (!ASC || Track.Watched.Get() == ASC)
	{
		return;
	}
	if (UAbilitySystemComponent* Old = Track.Watched.Get())
	{
		for (int32 i = 0; i < Track.Handles.Num() && i < WRLagDetail::NumDefenceTags; ++i)
		{
			Old->RegisterGameplayTagEvent(WRLagDetail::DefenceTags()[i], EGameplayTagEventType::NewOrRemoved).Remove(Track.Handles[i]);
		}
	}
	Track.Handles.Reset();
	Track.Watched = ASC;
	const TWeakObjectPtr<AWRCharacterBase> Weak(Who);
	for (int32 i = 0; i < WRLagDetail::NumDefenceTags; ++i)
	{
		const FGameplayTag Tag = WRLagDetail::DefenceTags()[i];
		Track.Handles.Add(ASC->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved).AddWeakLambda(this, [this, Weak](const FGameplayTag Changed, int32 Count)
		{
			OnDefenceChanged(Weak, Changed, Count);
		}));
		// A defence already up when we start watching.
		if (ASC->HasMatchingGameplayTag(Tag))
		{
			FSpan& Span = Track.Defences.AddDefaulted_GetRef();
			Span.Tag = Tag;
			Span.Start = GetWorld()->GetTimeSeconds() - OneWay(Who);
		}
	}
}

void UWRLagSubsystem::OnDefenceChanged(TWeakObjectPtr<AWRCharacterBase> Who, FGameplayTag Tag, int32 Count)
{
	AWRCharacterBase* Char = Who.Get();
	FTrack* Track = Char ? Tracks.Find(Who) : nullptr;
	if (!Track || !GetWorld())
	{
		return;
	}
	// The player pressed this that long before we heard of it.
	const double When = GetWorld()->GetTimeSeconds() - OneWay(Char);
	if (Count > 0)
	{
		FSpan& Span = Track->Defences.AddDefaulted_GetRef();
		Span.Tag = Tag;
		Span.Start = When;
	}
	else
	{
		for (int32 i = Track->Defences.Num() - 1; i >= 0; --i)
		{
			if (Track->Defences[i].Tag == Tag && Track->Defences[i].End < 0.0)
			{
				Track->Defences[i].End = FMath::Max(When, Track->Defences[i].Start);
				break;
			}
		}
	}
}

bool UWRLagSubsystem::BodyAt(const AWRCharacterBase* Who, double Time, FVector& OutCentre, float& OutRadius, float& OutHalfHeight, FRotator* OutRotation) const
{
	const FTrack* Track = Tracks.Find(Who);
	if (!Track || Track->Samples.Num() == 0)
	{
		return false;
	}
	const TArray<FSample>& S = Track->Samples;
	if (Time >= S.Last().Time)
	{
		// Newer than the last sample: the present.
		OutCentre = Who->GetActorLocation();
		Who->GetCapsuleComponent()->GetScaledCapsuleSize(OutRadius, OutHalfHeight);
		if (OutRotation) *OutRotation = Who->GetActorRotation();
		return true;
	}
	if (Time <= S[0].Time)
	{
		OutCentre = S[0].Centre; OutRadius = S[0].Radius; OutHalfHeight = S[0].HalfHeight;
		if (OutRotation) *OutRotation = S[0].Rotation;
		return true;
	}
	for (int32 i = S.Num() - 1; i > 0; --i)
	{
		if (S[i - 1].Time <= Time)
		{
			const float A = float((Time - S[i - 1].Time) / FMath::Max(1e-4, S[i].Time - S[i - 1].Time));
			OutCentre = FMath::Lerp(S[i - 1].Centre, S[i].Centre, A);
			OutRadius = FMath::Lerp(S[i - 1].Radius, S[i].Radius, A);
			OutHalfHeight = FMath::Lerp(S[i - 1].HalfHeight, S[i].HalfHeight, A);
			if (OutRotation) *OutRotation = FMath::Lerp(S[i - 1].Rotation, S[i].Rotation, A);
			return true;
		}
	}
	return false;
}

bool UWRLagSubsystem::WasDefended(const AWRCharacterBase* Who, const FGameplayTag& Tag, double Time) const
{
	return DefenceStart(Who, Tag, Time) >= 0.0;
}

double UWRLagSubsystem::DefenceStart(const AWRCharacterBase* Who, const FGameplayTag& Tag, double AtTime) const
{
	const FTrack* Track = Tracks.Find(Who);
	if (!Track)
	{
		return -1.0;
	}
	for (int32 i = Track->Defences.Num() - 1; i >= 0; --i)
	{
		const FSpan& D = Track->Defences[i];
		if (D.Tag == Tag && D.Start <= AtTime && (D.End < 0.0 || D.End >= AtTime))
		{
			return D.Start;
		}
	}
	return -1.0;
}

bool UWRLagSubsystem::ShouldHold(const AWRCharacterBase* Target, const FWRHit& Hit) const
{
	if (!Target || !Target->IsAlive() || Hit.bDamageOverTime || Hit.JudgeTime > 0.0 || OneWay(Target) <= 0.002f)
	{
		return false;
	}
	// Player against player: the swing is already judged where its attacker saw the target (the rewind); the defender's
	// guard, parry or roll pressed on its own screen is judged back-dated too, by waiting the same capped moment (the
	// favour-the-defender rule for defences, cooks/build-combat-full.md 2.22; pvp.defenderLag 0 turns it off).
	const AWRCharacterBase* By = Cast<AWRCharacterBase>(Hit.Instigator);
	if (By && By->IsPlayerControlled() && WRTune(TEXT("pvp.defenderLag"), 1.f) <= 0.f)
	{
		return false;
	}
	// Already defended by something that would take it: no need to wait.
	if (Target->HasTag(WRTags::State_Invulnerable)
		|| (Hit.bLow && Target->HasTag(WRTags::State_JumpLegsInvuln))
		|| (Hit.bParryable && Target->HasTag(WRTags::State_Parrying))
		|| (!Hit.bUnblockable && !Hit.bGrab && Target->HasTag(WRTags::State_Blocking)))
	{
		return false;
	}
	return true;
}

void UWRLagSubsystem::Hold(AWRCharacterBase* Target, const FWRHit& Hit, TFunction<void(EWRHitResult)> OnResolved)
{
	const double Now = GetWorld()->GetTimeSeconds();
	const float One = OneWay(Target);
	const float Seen = SeenDelay(Target);
	FHeld& H = Held.AddDefaulted_GetRef();
	H.Target = Target;
	H.Hit = Hit;
	// It saw the blow land Seen later; a defence pressed then reaches us One after that.
	H.Hit.JudgeTime = Now + Seen;
	H.ResolveAt = Now + FMath::Min(Seen + One + 0.02f, WRTune(TEXT("net.rewindMax"), 0.25f) + 0.1f);
	H.OnResolved = MoveTemp(OnResolved);
	UE_LOG(LogWildreach, Verbose, TEXT("LAG holding a blow on %s for %.0f ms (judged %.0f ms on)"), *Target->GetName(), (H.ResolveAt - Now) * 1000.0, Seen * 1000.f);
}
