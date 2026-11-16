#include "WRStrikeFront.h"
#include "Wildreach.h"
#include "WRCharacterBase.h"
#include "WRCombatLibrary.h"
#include "WRFx.h"
#include "WRTuning.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"

AWRStrikeFront::AWRStrikeFront()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	SetCanBeDamaged(false);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

AWRStrikeFront* AWRStrikeFront::Make(AActor* By, EKind Kind, const FVector& At, FOnHit OnHit, FName Fx, FName Sound, float Delay)
{
	UWorld* World = By ? By->GetWorld() : nullptr;
	if (!World || World->GetNetMode() == NM_Client || !OnHit)
	{
		return nullptr;   // the server runs fronts; every machine sees their fx rows
	}
	FActorSpawnParameters Params;
	Params.Owner = By;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWRStrikeFront* Front = World->SpawnActor<AWRStrikeFront>(AWRStrikeFront::StaticClass(), At, FRotator::ZeroRotator, Params);
	if (!Front)
	{
		return nullptr;
	}
	Front->Kind = Kind;
	Front->Source = By;
	Front->OnHit = MoveTemp(OnHit);
	Front->Fx = Fx;
	Front->Sound = Sound;
	Front->Centre = At;
	Front->StartAt = World->GetTimeSeconds() + FMath::Max(0.f, Delay);
	Front->SetLifeSpan(30.f + Delay);   // a front never outlives a fight by much, whatever happens to its owner
	return Front;
}

AWRStrikeFront* AWRStrikeFront::Ring(AActor* By, const FVector& Centre, float MaxRadius, float Speed, float Depth, FOnHit OnHit, FName Fx, FName Sound, float Delay)
{
	AWRStrikeFront* F = Make(By, EKind::Ring, Centre, MoveTemp(OnHit), Fx, Sound, Delay);
	if (F)
	{
		F->Centre = F->Floor(Centre);
		F->Radius = FMath::Max(50.f, MaxRadius);
		F->Speed = FMath::Abs(Speed) < 50.f ? 1000.f : Speed;
		F->Depth = FMath::Max(60.f, Depth);
	}
	return F;
}

AWRStrikeFront* AWRStrikeFront::Line(AActor* By, const FVector& From, const FVector& To, float Speed, float HalfWidth, FOnHit OnHit, FName Fx, FName Sound, float EndRing, float Delay)
{
	AWRStrikeFront* F = Make(By, EKind::Line, From, MoveTemp(OnHit), Fx, Sound, Delay);
	if (F)
	{
		F->Centre = F->Floor(From);
		F->To = F->Floor(To);
		F->Speed = FMath::Max(100.f, FMath::Abs(Speed));
		F->HalfWidth = FMath::Max(40.f, HalfWidth);
		F->EndRing = EndRing;
		F->Depth = WRTune(TEXT("bosses.waveDepth"), 180.f);
	}
	return F;
}

AWRStrikeFront* AWRStrikeFront::Arc(AActor* By, const FVector& Centre, float Radius, float FromYaw, float Sweep, float Seconds, FOnHit OnHit, FName Fx, FName Sound, float Delay)
{
	AWRStrikeFront* F = Make(By, EKind::Arc, Centre, MoveTemp(OnHit), Fx, Sound, Delay);
	if (F)
	{
		F->Radius = FMath::Max(50.f, Radius);
		F->FromYaw = FromYaw;
		F->Sweep = Sweep;
		F->Seconds = FMath::Max(0.05f, Seconds);
	}
	return F;
}

AWRStrikeFront::FOnHit AWRStrikeFront::HitWith(const FWRHit& Template)
{
	return [Template](AWRCharacterBase* Who)
	{
		const AWRCharacterBase* By = Cast<AWRCharacterBase>(Template.Instigator);
		if (Who && (!By || (By != Who && By->IsHostileTo(Who))))
		{
			FWRHit Hit = Template;
			if (Hit.ImpactPoint.IsZero()) Hit.ImpactPoint = Who->GetActorLocation();
			UWRCombatLibrary::ApplyHit(Who, Hit);
		}
	};
}

FVector AWRStrikeFront::Floor(const FVector& At) const
{
	// The ground under a point (the front runs on it, over a step or down a slope), else the point itself.
	FHitResult Ground;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(WRFrontFloor), false, Source.Get());
	if (GetWorld()->LineTraceSingleByChannel(Ground, At + FVector(0.f, 0.f, 300.f), At - FVector(0.f, 0.f, 1200.f), ECC_WorldStatic, Q))
	{
		return Ground.ImpactPoint;
	}
	return At;
}

void AWRStrikeFront::HitAround(const TFunction<bool(const AWRCharacterBase*)>& Inside)
{
	const AActor* By = Source.Get();
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		AWRCharacterBase* Who = *It;
		if (Who == By || !Who->IsAlive() || Who->GetAttachParentActor() || Struck.Contains(Who) || !Inside(Who))
		{
			continue;
		}
		Struck.Add(Who);
		OnHit(Who);
	}
}

void AWRStrikeFront::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double Now = GetWorld()->GetTimeSeconds();
	const float Age = float(Now - StartAt);
	if (Age < 0.f)
	{
		return;
	}
	const FWRTuning& T = FWRTuning::Get();
	// A ground front runs along the floor: a body in the air over it (a jump, a leap) clears it.
	const auto Grounded = [](const AWRCharacterBase* Who, const FVector& FloorAt)
	{
		const UCharacterMovementComponent* Move = Who->GetCharacterMovement();
		const float Feet = Who->GetActorLocation().Z - Who->GetSimpleCollisionHalfHeight();
		return !(Move && Move->IsFalling() && Feet > FloorAt.Z + WRTune(TEXT("bosses.frontClear"), 70.f)) && FMath::Abs(Feet - FloorAt.Z) < 450.f;
	};

	if (Kind == EKind::Ring || (Kind == EKind::Line && ArrivedAt >= 0.0))
	{
		// The ring (or the line's burst where it arrived).
		const bool bEnd = Kind == EKind::Line;
		const FVector At = bEnd ? To : Centre;
		const float Max = bEnd ? EndRing : Radius;
		const float RingSpeed = bEnd ? T.Num(TEXT("bosses.waveSpeed"), 1200.f) : Speed;
		const float Run = FMath::Abs(RingSpeed) * float(bEnd ? Now - ArrivedAt : Age);
		const bool bClosing = RingSpeed < 0.f;
		const float Edge = bClosing ? Max - Run : Run;
		if (FxCount == 0)
		{
			// An outward ring's look is one expanding shockwave; a closing one is drawn round its rim as it comes in.
			if (!bClosing) WRFx::Broadcast(this, Fx.IsNone() ? FName(TEXT("shockwave")) : Fx, At, FRotator::ZeroRotator, Max / 100.f, Sound);
			FxCount = 1;
		}
		if (bClosing && Run - FxDone >= 150.f)
		{
			FxDone = Run;
			for (int32 k = 0; k < 10; ++k)
			{
				const FVector P = At + FRotator(0.f, k * 36.f + FxCount * 11.f, 0.f).Vector() * FMath::Max(0.f, Edge);
				WRFx::Broadcast(this, Fx.IsNone() ? FName(TEXT("stomp")) : Fx, P, FRotator::ZeroRotator, 0.6f, (k == 0 && FxCount % 3 == 1) ? Sound : NAME_None);
			}
			++FxCount;
		}
		HitAround([&](const AWRCharacterBase* Who)
		{
			const float D = FVector::Dist2D(Who->GetActorLocation(), At) - (bClosing ? -Who->GetSimpleCollisionRadius() : Who->GetSimpleCollisionRadius());
			const bool bOnFront = bClosing ? (D >= Edge && D <= Edge + Depth) : (D <= Edge && D >= Edge - Depth);
			return D <= Max && bOnFront && Grounded(Who, At);
		});
		if ((bClosing && Edge <= 0.f) || (!bClosing && Edge >= Max + Depth))
		{
			Destroy();
		}
		return;
	}

	if (Kind == EKind::Line)
	{
		const FVector Flat = (To - Centre) * FVector(1.f, 1.f, 0.f);
		const float Length = Flat.Size();
		const FVector Dir = Flat.GetSafeNormal();
		const float Run = FMath::Min(Length, Speed * Age);
		const FVector Head = Floor(Centre + Dir * Run);
		const float Step = T.Num(TEXT("bosses.frontStepCm"), 120.f);
		while (FxDone <= Run)
		{
			// The ground splitting along the way: puffs every Step cm, the sound every third.
			WRFx::Broadcast(this, Fx.IsNone() ? FName(TEXT("stomp")) : Fx, Floor(Centre + Dir * FxDone), Dir.Rotation(), 0.55f, FxCount % 3 == 0 ? Sound : NAME_None);
			++FxCount;
			FxDone += Step;
		}
		HitAround([&](const AWRCharacterBase* Who)
		{
			return FVector::Dist2D(Who->GetActorLocation(), Head) <= HalfWidth + Who->GetSimpleCollisionRadius() && Grounded(Who, Head);
		});
		if (Run >= Length)
		{
			if (EndRing > 0.f)
			{
				ArrivedAt = Now;
				FxCount = 0;
				FxDone = 0.f;
			}
			else
			{
				Destroy();
			}
		}
		return;
	}

	// The arc: the limb's edge goes round; whatever is between where it started and where it is now, within its reach.
	const float Share = FMath::Clamp(Age / Seconds, 0.f, 1.f);
	const float Swept = Sweep * Share;
	while (FMath::Abs(FxDone) <= FMath::Abs(Swept))
	{
		// Leaves, grit and dust thrown off the limb's end every 15 degrees.
		const FVector Tip = Centre + FRotator(0.f, FromYaw + FxDone, 0.f).Vector() * Radius * 0.8f;
		WRFx::Broadcast(this, Fx.IsNone() ? FName(TEXT("stomp")) : Fx, Floor(Tip), FRotator(0.f, FromYaw + FxDone + 90.f * FMath::Sign(Sweep), 0.f), 0.6f, FxCount % 4 == 0 ? Sound : NAME_None);
		++FxCount;
		FxDone += 15.f * FMath::Sign(Sweep == 0.f ? 1.f : Sweep);
	}
	HitAround([&](const AWRCharacterBase* Who)
	{
		const FVector To2 = (Who->GetActorLocation() - Centre) * FVector(1.f, 1.f, 0.f);
		if (To2.Size() - Who->GetSimpleCollisionRadius() > Radius)
		{
			return false;
		}
		// The body's angle from where the sweep began, in the sweep's own direction.
		float Off = FMath::FindDeltaAngleDegrees(FromYaw, To2.Rotation().Yaw) * FMath::Sign(Sweep == 0.f ? 1.f : Sweep);
		if (Off < -1.f) Off += 360.f;
		return Off <= FMath::Abs(Swept) + 6.f && Grounded(Who, Floor(Who->GetActorLocation()));
	});
	if (Share >= 1.f)
	{
		Destroy();
	}
}
