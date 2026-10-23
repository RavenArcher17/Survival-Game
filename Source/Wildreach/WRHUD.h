#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "WRHUD.generated.h"

/** Debug-grade canvas HUD: survival bars + crosshair. Replaced by UMG later. */
UCLASS()
class WILDREACH_API AWRHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void DrawBar(const FString& Label, float Value, float Max, const FLinearColor& Color, float X, float Y);
};
