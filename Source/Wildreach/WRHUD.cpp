#include "WRHUD.h"
#include "WRCharacter.h"
#include "WRSurvivalComponent.h"
#include "Engine/Canvas.h"

namespace
{
	constexpr float BarWidth = 220.f;
	constexpr float BarHeight = 14.f;
	constexpr float BarSpacing = 22.f;
}

void AWRHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	const AWRCharacter* Survivor = Cast<AWRCharacter>(GetOwningPawn());
	if (!Survivor || !Survivor->GetSurvival())
	{
		DrawText(TEXT("Respawning..."), FLinearColor::White, Canvas->ClipX * 0.5f - 50.f, Canvas->ClipY * 0.5f);
		return;
	}

	const UWRSurvivalComponent* S = Survivor->GetSurvival();
	const float X = 24.f;
	float Y = Canvas->ClipY - 4.f * BarSpacing - 24.f;

	DrawBar(TEXT("Health"),  S->Health,  S->MaxHealth,  FLinearColor(0.80f, 0.15f, 0.15f), X, Y); Y += BarSpacing;
	DrawBar(TEXT("Hunger"),  S->Hunger,  S->MaxHunger,  FLinearColor(0.85f, 0.55f, 0.15f), X, Y); Y += BarSpacing;
	DrawBar(TEXT("Thirst"),  S->Thirst,  S->MaxThirst,  FLinearColor(0.20f, 0.50f, 0.90f), X, Y); Y += BarSpacing;
	DrawBar(TEXT("Stamina"), S->Stamina, S->MaxStamina, FLinearColor(0.30f, 0.80f, 0.30f), X, Y);

	DrawText(TEXT("WASD move  Mouse look  Space jump  Shift sprint  V toggle view"),
		FLinearColor(1.f, 1.f, 1.f, 0.6f), 24.f, 24.f);

	if (Survivor->IsFirstPerson())
	{
		const float CX = Canvas->ClipX * 0.5f;
		const float CY = Canvas->ClipY * 0.5f;
		DrawRect(FLinearColor::White, CX - 1.f, CY - 1.f, 3.f, 3.f);
	}
}

void AWRHUD::DrawBar(const FString& Label, float Value, float Max, const FLinearColor& Color, float X, float Y)
{
	const float Fraction = Max > 0.f ? FMath::Clamp(Value / Max, 0.f, 1.f) : 0.f;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), X + 70.f, Y, BarWidth, BarHeight);
	DrawRect(Color, X + 70.f, Y, BarWidth * Fraction, BarHeight);
	DrawText(Label, FLinearColor::White, X, Y - 1.f);
}
