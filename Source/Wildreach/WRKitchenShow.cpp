// The kitchen at work (cooks/cookpass-survival.md 3.4; Valheim's cooking station, a real spit and simmering pot), drawn
// on every machine that draws from the station's replicated queue and tray (no server cost):
//  - a campfire's spit carries the meat or fish being cooked, skewered through its middle where the spit rests in its
//    two forked posts, turning, browning from raw to its cooked colour as the job runs;
//  - what's done waits on a flat hearth stone beside the fire (a campfire, a pot) or on the oven's hearth slab by its
//    mouth, never on the coals;
//  - the pot's lid sits on its rim under the bail and lifts and rattles on its steam while it cooks, the steam leaving at
//    the lid's edge, a simmer under it;
//  - a working smokehouse breathes smoke from its ridge vent, an oven from its chimney.
#include "WRCrafting.h"
#include "WRBuildLook.h"
#include "WRFx.h"
#include "WRHeldFood.h"
#include "WRInventory.h"
#include "WRSound.h"
#include "WRToolMesh.h"
#include "WRTuning.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "ProceduralMeshComponent.h"

namespace WRKitchenShowDetail
{
	/** The item the first order makes (its recipe's output), or none. */
	FName Cooking(const AWRStation* S)
	{
		if (S->Queue.Num() == 0) return NAME_None;
		const FWRRecipe* R = FWRRecipeRegistry::Get().Find(S->Queue[0].Recipe);
		return R ? R->Output : NAME_None;
	}

	UProceduralMeshComponent* Make(AWRStation* S, const TCHAR* Name)
	{
		UProceduralMeshComponent* C = NewObject<UProceduralMeshComponent>(S, Name);
		C->SetupAttachment(S->GetRootComponent());
		C->RegisterComponent();
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCanEverAffectNavigation(false);
		// Placed in the world each time: the station's root is a scaled box shape that mustn't stretch it.
		C->SetUsingAbsoluteLocation(true);
		C->SetUsingAbsoluteRotation(true);
		C->SetUsingAbsoluteScale(true);
		return C;
	}

	/** The station's build piece's frame (its blocks are drawn in it): the station stands ChildLift over the piece. */
	FTransform PieceFrame(const AWRStation* S)
	{
		return FTransform(S->GetActorRotation(), S->GetActorLocation() - FVector(0.f, 0.f, FWRBuildLook::ChildLift));
	}

	void Place(USceneComponent* C, const FTransform& Frame, const FVector& Local, const FRotator& Rot = FRotator::ZeroRotator)
	{
		C->SetWorldLocationAndRotation(Frame.TransformPosition(Local), Frame.TransformRotation(Rot.Quaternion()));
	}
}

bool AWRStation::IsKitchenShow() const
{
	return StationType == TEXT("campfire") || StationType == TEXT("cooking_pot") || StationType == TEXT("oven") || StationType == TEXT("smokehouse");
}

void AWRStation::TickKitchenShow(float DeltaSeconds)
{
	using namespace WRKitchenShowDetail;
	const FWRTuning& T = FWRTuning::Get();
	const UWorld* World = GetWorld();
	const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
	const float Range = T.Num(TEXT("farm.show.range"), 4000.f);
	const bool bNear = Cam && FVector::DistSquared(Cam->GetCameraLocation(), GetActorLocation()) < Range * Range;
	// Smooth when someone's watching, a slow check when not.
	SetActorTickInterval(bNear ? 0.f : 0.5f);
	const bool bWorking = Queue.Num() > 0;
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	const FTransform Frame = PieceFrame(this);

	// ---- The pot's lid: always on the rim (the pot is drawn without it), lifting on the boil.
	if (StationType == TEXT("cooking_pot"))
	{
		if (!ShowLid)
		{
			ShowLid = Make(this, TEXT("PotLid"));
			FWRToolMesh M;
			const FLinearColor Copper(0.45f, 0.17f, 0.07f);
			// A shallow copper dome (42 cm across) with a turned iron knob, as the pot's own lid was.
			M.Lathe(FVector::ZeroVector, FVector::UpVector, 4.f, { FVector(0.f, 21.f, 21.f), FVector(0.5f, 19.f, 19.f), FVector(1.f, 12.f, 12.f) }, 24, Copper * 0.85f, 0.45f);
			M.Lathe(FVector(0.f, 0.f, 3.8f), FVector::UpVector, 3.4f, { FVector(0.f, 1.4f, 1.4f), FVector(0.4f, 1.f, 1.f), FVector(0.7f, 2.4f, 2.4f), FVector(1.f, 2.f, 2.f) }, 10, FLinearColor(0.08f, 0.08f, 0.09f), 0.5f);
			M.ApplyTo(ShowLid);
		}
		float Lift = 0.f, Tip = 0.f;
		if (bWorking && bNear)
		{
			// On its steam: a quick chatter at lidHz, uneven (two waves out of step), never above the bail's arch.
			const float Hz = T.Num(TEXT("farm.show.lidHz"), 7.f);
			const float Chatter = FMath::Max(0.f, FMath::Sin(Now * 2.f * PI * Hz) * FMath::Sin(Now * 2.f * PI * 0.37f + 1.1f));
			Lift = Chatter * T.Num(TEXT("farm.show.lidLift"), 2.5f);
			Tip = Chatter * 3.f * FMath::Sin(Now * 1.3f);
		}
		Place(ShowLid, Frame, FVector(0.f, 0.f, 83.5f + Lift), FRotator(Tip, 0.f, Tip * 0.6f));
	}

	// ---- The spit: the job's meat or fish, browning as it goes.
	if (StationType == TEXT("campfire"))
	{
		const FName Item = bWorking ? Cooking(this) : NAME_None;
		const FWRFoodLook* Look = Item.IsNone() ? nullptr : &FWRFoodLook::For(Item);
		const bool bSpit = Look && (Look->Shape == TEXT("meat") || Look->Shape == TEXT("fish") || Look->Shape == TEXT("skewer") || Look->Shape == TEXT("strip"));
		if (bSpit && bNear)
		{
			if (!ShowSpit) ShowSpit = Make(this, TEXT("SpitFood"));
			// Rebuilt as it browns, in eight shades from raw to done.
			const int32 Shade = FMath::Clamp(FMath::FloorToInt(JobProgress() * 8.f), 0, 8);
			if (ShowSpitFor != Item || ShowSpitShade != Shade)
			{
				ShowSpitFor = Item;
				ShowSpitShade = Shade;
				FWRToolMesh M;
				FWRFoodLook::Build(M, *Look, FVector::ZeroVector, Shade / 8.f, 1.15f);
				M.ApplyTo(ShowSpit);
			}
			ShowSpitAngle = FMath::Fmod(ShowSpitAngle + DeltaSeconds * T.Num(TEXT("farm.show.spitTurn"), 25.f), 360.f);
			// Skewered along the spit (X) through its middle, turning about it.
			Place(ShowSpit, Frame, FVector(0.f, 0.f, T.Num(TEXT("farm.show.spitZ"), 74.f)), FRotator(0.f, 0.f, ShowSpitAngle));
			ShowSpit->SetVisibility(true);
		}
		else if (ShowSpit && ShowSpit->IsVisible())
		{
			ShowSpit->SetVisibility(false);
		}
	}

	// ---- The tray: what's done, laid on a hearth stone (or the oven's slab), up to three pieces.
	if (StationType != TEXT("smokehouse"))
	{
		const FWRItemStack* First = nullptr;
		for (const FWRItemStack& S : Output) if (!S.IsEmpty()) { First = &S; break; }
		const FName Item = First && bNear ? First->ItemId : NAME_None;
		const int32 Count = First ? FMath::Min(First->Count, 3) : 0;
		if (!Item.IsNone())
		{
			if (!ShowTray) ShowTray = Make(this, TEXT("TrayFood"));
			if (ShowTrayFor != Item || ShowTrayCount != Count)
			{
				ShowTrayFor = Item;
				ShowTrayCount = Count;
				const FWRFoodLook& Look = FWRFoodLook::For(Item);
				FWRToolMesh M;
				const bool bOven = StationType == TEXT("oven");
				if (!bOven)
				{
					// A flat river stone set on the ground outside the fire ring, a little proud of the soil.
					M.Lathe(FVector(0.f, 0.f, -1.f), FVector::UpVector, 6.f, { FVector(0.f, 17.f, 13.f), FVector(0.6f, 18.f, 14.f), FVector(1.f, 15.f, 11.f) }, 12, FLinearColor(0.33f, 0.31f, 0.28f), 0.85f, 0.f, 7.f);
				}
				const float Top = bOven ? 0.f : 5.f;
				for (int32 i = 0; i < Count; ++i)
				{
					const FVector At(-8.f + i * 8.f, (i % 2) * 4.f - 2.f, Top + (FWRFoodLook::IsAlongX(Look.Shape) ? 4.f : 3.f) + (Look.Shape == TEXT("bowl") ? -3.f : 0.f));
					FWRFoodLook::Build(M, Look, At, 1.f, FWRFoodLook::IsAlongX(Look.Shape) ? 0.6f : 0.7f);
				}
				M.ApplyTo(ShowTray);
			}
			const FString Key = FString::Printf(TEXT("farm.show.tray.%s."), *StationType.ToString());
			const FVector At(T.Num(Key + TEXT("x"), StationType == TEXT("oven") ? 62.f : T.Num(TEXT("farm.show.stoneX"), 72.f)),
				T.Num(Key + TEXT("y"), StationType == TEXT("oven") ? 42.f : T.Num(TEXT("farm.show.stoneY"), 54.f)),
				T.Num(Key + TEXT("z"), StationType == TEXT("oven") ? 64.f : 0.f));
			Place(ShowTray, Frame, At);
			ShowTray->SetVisibility(true);
		}
		else if (ShowTray && ShowTray->IsVisible())
		{
			ShowTray->SetVisibility(false);
			ShowTrayFor = NAME_None;
		}
	}

	// ---- Steam and smoke where they leave, and the simmer; started and stopped with the work.
	const bool bFx = bWorking && bNear && StationType != TEXT("campfire");
	if (bFx != bShowWorking)
	{
		bShowWorking = bFx;
		if (bFx)
		{
			if (!ShowFx)
			{
				ShowFx = NewObject<USceneComponent>(this, TEXT("KitchenFxAnchor"));
				ShowFx->SetupAttachment(GetRootComponent());
				ShowFx->SetUsingAbsoluteLocation(true);
				ShowFx->SetUsingAbsoluteRotation(true);
				ShowFx->SetUsingAbsoluteScale(true);
				ShowFx->RegisterComponent();
			}
			const bool bPot = StationType == TEXT("cooking_pot");
			const FString Key = FString::Printf(TEXT("farm.show.smoke.%s."), *StationType.ToString());
			const FVector Where = bPot ? FVector(0.f, 0.f, T.Num(TEXT("farm.show.steamZ"), 86.f))
				: FVector(T.Num(Key + TEXT("x"), 0.f), 0.f, T.Num(Key + TEXT("z"), 250.f));
			Place(ShowFx, Frame, Where);
			ShowFxRun = WRFx::Attach(ShowFx, bPot ? FName(TEXT("kitchen.steam")) : FName(TEXT("kitchen.smoke")));
			if (bPot && !ShowLoop) ShowLoop = WRSound::StartLoop(this, TEXT("cook.bubble"));
		}
		else
		{
			if (ShowFxRun)
			{
				WRFx::Stop(ShowFxRun);
				ShowFxRun = nullptr;
			}
			if (ShowLoop)
			{
				ShowLoop->FadeOut(0.6f, 0.f);
				ShowLoop = nullptr;
			}
		}
	}
}
