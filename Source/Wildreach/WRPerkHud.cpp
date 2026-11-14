// Life-skill perks that show on your own screen (see WRPerkHud.h). Drawn by the HUD's combat layer (WRFxHud) under
// the panels, so menus and the map cover it; numbers in tuning lifeSkills.hud.

#include "WRPerkHud.h"
#include "WRBoss.h"
#include "WRCreature.h"
#include "WRGameplayTags.h"
#include "Camera/PlayerCameraManager.h"
#include "WRInventory.h"
#include "WRLifeSkills.h"
#include "WRPlayerController.h"
#include "WRResourceNode.h"
#include "WRTuning.h"
#include "WRUIStyle.h"
#include "EngineUtils.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"

namespace WRPerkHudDetail
{
	/** Ore colours, sRGB like the mockups (FColor), so they read as copper, tin, iron and sulfur. */
	static FLinearColor OreColour(FName Item)
	{
		if (Item == TEXT("copper_ore")) return FLinearColor(FColor(214, 128, 70));
		if (Item == TEXT("tin_ore")) return FLinearColor(FColor(196, 204, 210));
		if (Item == TEXT("iron_ore")) return FLinearColor(FColor(170, 96, 70));
		if (Item == TEXT("sulfur_crystal")) return FLinearColor(FColor(236, 214, 80));
		return FLinearColor(FColor(200, 190, 170));
	}

	static bool IsOre(FName Item)
	{
		return Item.ToString().EndsWith(TEXT("_ore")) || Item == TEXT("sulfur_crystal");
	}

	/** A diamond outline (four lines), with a dark stroke under it so it reads on snow and on ash. */
	static void Diamond(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2D& C, float R, const FLinearColor& Colour, float Thick)
	{
		const TArray<FVector2D> Pts = { C + FVector2D(0.f, -R), C + FVector2D(R, 0.f), C + FVector2D(0.f, R), C + FVector2D(-R, 0.f), C + FVector2D(0.f, -R) };
		TArray<FVector2D> Under;
		for (const FVector2D& P : Pts) Under.Add(P + FVector2D(1.f, 1.f));
		FSlateDrawElement::MakeLines(Out, Layer, Geo.ToPaintGeometry(), Under, ESlateDrawEffect::None, FLinearColor(0.f, 0.f, 0.f, 0.55f * Colour.A), true, Thick + 1.5f);
		FSlateDrawElement::MakeLines(Out, Layer + 1, Geo.ToPaintGeometry(), Pts, ESlateDrawEffect::None, Colour, true, Thick);
	}

	static void Label(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FString& Text, FSlateFontInfo Font, const FVector2D& CentreTop, const FLinearColor& Colour)
	{
		Font.OutlineSettings.OutlineSize = 1;
		Font.OutlineSettings.OutlineColor = FLinearColor(0.02f, 0.015f, 0.01f, 0.9f * Colour.A);
		const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		const FVector2D TextSize = Measure->Measure(Text, Font);
		FSlateDrawElement::MakeText(Out, Layer, Geo.ToPaintGeometry(TextSize, FSlateLayoutTransform(FVector2D(CentreTop.X - TextSize.X * 0.5f, CentreTop.Y))), Text, Font, ESlateDrawEffect::None, Colour);
	}

	/** Beast lore's lines for a creature: what hurts it most, what it ignores, how it's tamed. */
	static TArray<TPair<FString, FLinearColor>> LoreFor(const AWRCreature* C)
	{
		TArray<TPair<FString, FLinearColor>> Lines;
		const FWRCreatureDef* Def = C ? C->GetDef() : nullptr;
		if (!Def)
		{
			return Lines;
		}
		if (const AWRBoss* Boss = Cast<AWRBoss>(C))
		{
			const bool bFire = Boss->GetBossElement() == WRTags::Element_Fire;
			Lines.Add({ FString::Printf(TEXT("Weak to %s  x%.1f"), bFire ? TEXT("frost") : TEXT("fire"), WRTune(TEXT("bosses.weaknessMultiplier"), 1.5f)), bFire ? WRUI::Frost : WRUI::Ember });
		}
		else if (Def->Element == TEXT("fire") || Def->Element == TEXT("frost"))
		{
			Lines.Add({ FString::Printf(TEXT("%s-born: %s path magic calms it"), Def->Element == TEXT("fire") ? TEXT("Fire") : TEXT("Frost"), *Def->Element.ToString()),
				Def->Element == TEXT("fire") ? WRUI::Ember : WRUI::Frost });
		}
		TArray<FString> Shrugs;
		for (const FName& R : Def->Resists)
		{
			Shrugs.Add(R == TEXT("lava") ? TEXT("walks lava") : R == TEXT("ice") ? TEXT("sure-footed on ice") : R.ToString());
		}
		if (Shrugs.Num() > 0)
		{
			Lines.Add({ FString::Join(Shrugs, TEXT(", ")), WRUI::Ink2 });
		}
		if (!C->IsTamed() && Def->TamingMethods.Num() > 0 && !Cast<AWRBoss>(C))
		{
			TArray<FString> Ways;
			for (const FName& M : Def->TamingMethods)
			{
				if (M == TEXT("knockout_feed")) Ways.Add(TEXT("knock out and feed"));
				else if (M == TEXT("offer_food"))
				{
					const FWRItemDef* Food = Def->Food.Num() > 0 ? FWRItemRegistry::Get().Find(Def->Food[0]) : nullptr;
					Ways.Add(Food ? FString::Printf(TEXT("offer %s"), *Food->Name.ToLower()) : FString(TEXT("offer food")));
				}
				else if (M == TEXT("steal_egg")) Ways.Add(TEXT("steal an egg"));
				else if (M == TEXT("path_magic")) Ways.Add(TEXT("path magic"));
			}
			if (Ways.Num() > 0)
			{
				Lines.Add({ TEXT("Tame: ") + FString::Join(Ways, TEXT(" or ")), WRUI::Good });
			}
		}
		return Lines;
	}
}

int32 WRPerkHud::Paint(AWRPlayerController* PC, const FGeometry& Geo, FSlateWindowElementList& Out, int32 Layer, const FVector2D& Size, int32 VX, int32 VY)
{
	using namespace WRPerkHudDetail;
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	const UWRLifeSkillsComponent* Skills = UWRLifeSkillsComponent::FindFor(PC);
	if (!Me || !Skills || VX <= 0 || VY <= 0)
	{
		return Layer;
	}
	const FWRTuning& T = FWRTuning::Get();
	const double Now = PC->GetWorld()->GetTimeSeconds();
	const auto ToLocal = [&Size, VX, VY](const FVector2D& Screen) { return FVector2D(Screen.X * Size.X / VX, Screen.Y * Size.Y / VY); };

	// --- Prospector: ore veins near you, through rock ---
	if (Skills->Stat(TEXT("prospect")) > 0.f)
	{
		const float Range = T.Num(TEXT("lifeSkills.hud.prospectRange"), 3500.f);
		const float Near = T.Num(TEXT("lifeSkills.hud.prospectLabelRange"), 1500.f);
		for (TActorIterator<AWRResourceNode> It(PC->GetWorld()); It; ++It)
		{
			if (It->IsDepleted() || !IsOre(It->ItemId))
			{
				continue;
			}
			const float Dist = FVector::Dist(It->GetActorLocation(), Me->GetActorLocation());
			FVector2D Screen;
			if (Dist > Range || !PC->ProjectWorldLocationToScreen(It->GetActorLocation() + FVector(0.f, 0.f, 40.f), Screen, true))
			{
				continue;
			}
			const FVector2D At = ToLocal(Screen);
			const float A = FMath::GetMappedRangeValueClamped(FVector2D(Near, Range), FVector2D(0.95f, 0.3f), Dist);
			// A slow shimmer, out of step per vein, so marks read as sensed rather than painted on.
			const float Pulse = 0.85f + 0.15f * FMath::Sin(static_cast<float>(Now) * 2.2f + It->GetActorLocation().X * 0.01f);
			const FLinearColor Colour = WRUI::Alpha(OreColour(It->ItemId), A * Pulse);
			const float R = FMath::GetMappedRangeValueClamped(FVector2D(0.f, Range), FVector2D(9.f, 5.f), Dist);
			Diamond(Out, Layer + 1, Geo, At, R, Colour, 2.f);
			Diamond(Out, Layer + 1, Geo, At, R * 0.45f, Colour, 1.5f);
			if (Dist < Near)
			{
				const FWRItemDef* Def = FWRItemRegistry::Get().Find(It->ItemId);
				Label(Out, Layer + 3, Geo, FString::Printf(TEXT("%s  %d m"), Def ? *Def->Name : *It->ItemId.ToString(), FMath::RoundToInt(Dist / 100.f)),
					WRUI::Caps(11), At + FVector2D(0.f, R + 4.f), Colour);
			}
		}
	}

	// --- Predator sense: whatever has picked you as its prey ---
	if (Skills->Stat(TEXT("predatorSense")) > 0.f)
	{
		const float Range = T.Num(TEXT("lifeSkills.hud.predatorRange"), 6000.f);
		const FVector2D Centre = Size * 0.5f;
		const float Ring = FMath::Min(Size.X, Size.Y) * T.Num(TEXT("lifeSkills.hud.predatorRing"), 0.16f);
		const float CamYaw = PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraRotation().Yaw : PC->GetControlRotation().Yaw;
		int32 Hunters = 0;
		for (TActorIterator<AWRCreature> It(PC->GetWorld()); It; ++It)
		{
			if (!It->IsAlive() || It->IsTamed() || It->GetAggroRep() != Me || FVector::Dist(It->GetActorLocation(), Me->GetActorLocation()) > Range)
			{
				continue;
			}
			++Hunters;
			// A wedge on the ring, pointing the way it comes from (screen up = where the camera faces).
			const FVector To = It->GetActorLocation() - Me->GetActorLocation();
			const float Rel = FMath::DegreesToRadians(FMath::FindDeltaAngleDegrees(CamYaw, To.Rotation().Yaw));
			const FVector2D Dir(FMath::Sin(Rel), -FMath::Cos(Rel));
			const FVector2D Side(-Dir.Y, Dir.X);
			const float Pulse = 0.65f + 0.35f * FMath::Abs(FMath::Sin(static_cast<float>(Now) * 4.f));
			const FVector2D Tip = Centre + Dir * (Ring + 16.f), Base = Centre + Dir * Ring;
			const TArray<FVector2D> Wedge = { Base + Side * 9.f, Tip, Base - Side * 9.f };
			FSlateDrawElement::MakeLines(Out, Layer + 2, Geo.ToPaintGeometry(), Wedge, ESlateDrawEffect::None, FLinearColor(0.f, 0.f, 0.f, 0.6f), true, 4.5f);
			FSlateDrawElement::MakeLines(Out, Layer + 3, Geo.ToPaintGeometry(), Wedge, ESlateDrawEffect::None, WRUI::Alpha(WRUI::Bad, Pulse), true, 3.f);
		}
		if (Hunters > 0)
		{
			Label(Out, Layer + 3, Geo, Hunters > 1 ? FString::Printf(TEXT("Hunted by %d"), Hunters) : FString(TEXT("Hunted")), WRUI::Caps(13),
				FVector2D(Centre.X, Centre.Y - Ring - 46.f), WRUI::Alpha(WRUI::Bad, 0.95f));
		}
	}

	// --- Beast lore: under what you've locked on to ---
	if (Skills->Stat(TEXT("beastLore")) > 0.f)
	{
		const AWRCreature* Lock = Cast<AWRCreature>(PC->GetLockTarget());
		FVector2D Feet;
		if (Lock && PC->ProjectWorldLocationToScreen(Lock->GetActorLocation() - FVector(0.f, 0.f, Lock->GetSimpleCollisionHalfHeight()), Feet, true))
		{
			FVector2D At = ToLocal(Feet) + FVector2D(0.f, 14.f);
			for (const TPair<FString, FLinearColor>& Line : LoreFor(Lock))
			{
				Label(Out, Layer + 3, Geo, Line.Key, WRUI::Body(13, 700), At, Line.Value);
				At.Y += 17.f;
			}
		}
	}
	return Layer + 3;
}
