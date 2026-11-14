#include "WRCameraBoom.h"
#include "WRTuning.h"

UWRCameraBoom::UWRCameraBoom()
{
	// Defaults; OnRegister reads tuning.json (the file isn't loaded yet when the class default object is made).
	ProbeSize = 16.f;
}

void UWRCameraBoom::OnRegister()
{
	Super::OnRegister();
	// A slightly fatter probe starts the pull-in a hair before the lens would touch a wall or a trunk.
	ProbeSize = WRTune(TEXT("camera.probeSize"), 16.f);
}

FVector UWRCameraBoom::BlendLocations(const FVector& DesiredArmLocation, const FVector& TraceHitLocation, bool bHitSomething, float DeltaTime)
{
	// The trace ran from the arm's origin (no location lag on this arm) to where the camera wants to be.
	const FVector Origin = GetComponentLocation() + TargetOffset;
	const FVector ToDesired = DesiredArmLocation - Origin;
	const float Full = ToDesired.Size();
	if (Full < 1.f || DeltaTime <= 0.f)
	{
		Reach = 1.f;
		return bHitSomething ? TraceHitLocation : DesiredArmLocation;
	}
	// As a share of the full arm, so the aiming zoom (a shorter arm) doesn't count as an obstacle clearing.
	const float Want = bHitSomething ? FMath::Clamp(FVector::Dist(Origin, TraceHitLocation) / Full, 0.f, 1.f) : 1.f;
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (Reach < 0.f || Want <= Reach)
	{
		Reach = Want;  // in: at once (never inside the wall)
		PulledAt = Now;
	}
	else if (Now - PulledAt >= WRTune(TEXT("camera.holdSeconds"), 0.15f))
	{
		// Out: held a moment (no breathing against a wall you brush past), then eased back over easeOutSeconds.
		const float Ease = FMath::Max(0.05f, WRTune(TEXT("camera.easeOutSeconds"), 0.4f));
		const float Step = FMath::Max(DeltaTime / Ease, (Want - Reach) * FMath::Min(1.f, DeltaTime * WRTune(TEXT("camera.returnSpeed"), 3.5f)));
		Reach = FMath::Min(Want, Reach + Step);
	}
	return Origin + ToDesired * Reach;
}

float UWRCameraBoom::TickSprintWiden(bool bSprinting, float DeltaSeconds)
{
	SprintBlend = FMath::FInterpTo(SprintBlend, bSprinting ? 1.f : 0.f, DeltaSeconds, WRTune(TEXT("camera.sprintFovSpeed"), 3.f));
	return SprintBlend * WRTune(TEXT("camera.sprintFov"), 6.f);
}
