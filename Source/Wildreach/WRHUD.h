#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "WRCombatTypes.h"
#include "WRHUD.generated.h"

class AWRCharacterBase;

/** Debug-grade canvas HUD: vitals, resources, statuses, path info, quick slots, enemy bars, damage numbers. UMG later. */
UCLASS()
class WILDREACH_API AWRHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void DrawHUD() override;

private:
	void DrawBar(const FString& Label, float Value, float Max, const FLinearColor& Color, float X, float Y, float Width = 220.f);
	void DrawWorldBars(const AWRCharacterBase* Self);
	void DrawDamageNumbers(float DeltaTime);
	void OnHitShown(const AActor* Target, float Damage, EWRHitResult Result);

	struct FFloatingText
	{
		FVector WorldPos;
		FString Text;
		FLinearColor Color;
		float Age = 0.f;
	};
	TArray<FFloatingText> Floating;
	FDelegateHandle HitHandle;
	double LastDrawTime = 0.0;
};
