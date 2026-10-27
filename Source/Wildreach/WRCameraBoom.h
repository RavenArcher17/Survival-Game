#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SpringArmComponent.h"
#include "WRCameraBoom.generated.h"

/**
 * The survivor's third-person spring arm, smoother near walls and trees: when something comes between the camera and
 * the survivor the arm still pulls in at once (the camera never ends up inside a wall), but it eases back out instead
 * of popping, so brushing past a trunk or turning away from a wall glides. Also keeps the sprint widening of the view
 * (the character asks for it each frame). Numbers in tuning.json "camera".
 */
UCLASS()
class WILDREACH_API UWRCameraBoom : public USpringArmComponent
{
	GENERATED_BODY()

public:
	UWRCameraBoom();
	virtual void OnRegister() override;

	/** Eased 0..1 while sprinting at speed; returns the field-of-view degrees to add this frame. */
	float TickSprintWiden(bool bSprinting, float DeltaSeconds);

protected:
	virtual FVector BlendLocations(const FVector& DesiredArmLocation, const FVector& TraceHitLocation, bool bHitSomething, float DeltaTime) override;

private:
	/** How much of the full arm the camera is out (eased back toward 1 after a pull-in); -1 = not started. */
	float Reach = -1.f;
	float SprintBlend = 0.f;
};
