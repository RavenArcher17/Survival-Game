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
	/** Canvas text centred on CentreX (nameplates over heads). */
	void DrawCentred(const FString& Text, const FLinearColor& Color, float CentreX, float Y, float Scale);
	/** The nearest knocked-out wild creature: torpor (and when it wakes), progress, effectiveness, food, what it needs. */
	void DrawTamingPanel(const AWRCharacterBase* Self);
	/** The old text list for the open menu (with -oldui, or a menu the new UI has no screen for yet). */
	void DrawMenuPanel(const class AWRPlayerController* PC);
	void DrawDamageNumbers(float DeltaTime);
	/** An enemy bar's trailing chunk (cookpass-combat 3.3): its top, its last fill, when it last dropped, last drawn. */
	struct FBarTrail { float Top = -1.f; float Last = -1.f; double DropAt = -10.0; double Seen = 0.0; };
	TMap<TWeakObjectPtr<const AActor>, FBarTrail> BarTrails;
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
