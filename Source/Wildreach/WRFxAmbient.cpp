// Ambient effects nobody has to call: flames on Hearths, torches and braziers (and their lights flicker), flames or
// frost on burning / frozen characters, embers rising off the island's lava, and weather around the local camera
// (snow flurries on the tundra side, falling ash on the volcanic side). Found by a cheap scan twice a second near the
// camera, so the gameplay classes need no effect code. Visual only; numbers in tuning.json fx.ambient / lava / weather.

#include "WRFxSystem.h"
#include "WRBuilding.h"
#include "WRCharacterBase.h"
#include "WRCrafting.h"
#include "WRGameplayTags.h"
#include "WRTuning.h"
#include "WRWorldDressing.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

void UWRFxSubsystem::TickAmbient(float DeltaTime)
{
	UWorld* World = GetWorld();
	const FWRTuning& T = FWRTuning::Get();
	AmbientClock -= DeltaTime;
	if (AmbientClock <= 0.f)
	{
		AmbientClock = 0.5f;
		const float Range = T.Num(TEXT("fx.ambient.distance"), 6000.f);
		const double Range2 = FMath::Square(static_cast<double>(Range));
		const auto Near = [&](const AActor* A) { return A && FVector::DistSquared(A->GetActorLocation(), ViewLocation) < Range2; };

		// Fires: drop the ones that went away or out of range (lights back to their own brightness).
		for (int32 i = Ambient.Num() - 1; i >= 0; --i)
		{
			FAmbient& A = Ambient[i];
			const AWRHearth* Hearth = Cast<AWRHearth>(A.Actor.Get());
			if (!Near(A.Actor.Get()) || !A.Light.IsValid() || !A.Light->IsVisible() || (Hearth && Hearth->IsBroken()))
			{
				StopEmitter(A.Emitter);
				if (UPointLightComponent* L = A.Light.Get()) L->SetIntensity(A.BaseIntensity);
				Ambient.RemoveAtSwap(i);
			}
		}
		const auto Track = [&](AActor* Actor, FName Effect, float Scale)
		{
			if (Ambient.ContainsByPredicate([Actor](const FAmbient& A) { return A.Actor.Get() == Actor; })) return;
			// The fire sits where its owner put the light (the Hearth's glow, a torch's flame), so a new mesh that
			// keeps its light in the bowl keeps its flames there too.
			UPointLightComponent* Light = Actor->FindComponentByClass<UPointLightComponent>();
			if (!Light || !Light->IsVisible()) return;
			FAmbient A;
			A.Actor = Actor;
			A.Light = Light;
			A.BaseIntensity = Light->Intensity;
			A.Effect = Effect;
			A.Seed = FMath::FRand() * 100.f;
			A.Emitter = Start(Effect, Light->GetComponentTransform(), Scale, 0.f, Light);
			Ambient.Add(A);
		};
		for (TActorIterator<AWRHearth> It(World); It; ++It)
		{
			if (Near(*It) && !It->IsBroken()) Track(*It, TEXT("hearth.fire"), 1.f);
		}
		for (TActorIterator<AWRBuildPiece> It(World); It; ++It)
		{
			const FWRPieceDef* Def = It->bGhost ? nullptr : It->GetDef();
			if (Def && Def->Kind == TEXT("light") && Near(*It))
			{
				Track(*It, Def->Id == TEXT("brazier") ? FName(TEXT("brazier.fire")) : FName(TEXT("torch.fire")), 1.f);
			}
		}

		// Burning and frozen characters.
		for (int32 i = StatusFx.Num() - 1; i >= 0; --i)
		{
			const AWRCharacterBase* C = Cast<AWRCharacterBase>(StatusFx[i].Actor.Get());
			const bool bBurn = C && Near(C) && C->HasTag(WRTags::Status_Burn) && !C->HasTag(WRTags::State_Dead);
			const bool bFrozen = C && Near(C) && C->HasTag(WRTags::Status_Frozen) && !C->HasTag(WRTags::State_Dead);
			if (!bBurn && StatusFx[i].Burn != INDEX_NONE) { StopEmitter(StatusFx[i].Burn); StatusFx[i].Burn = INDEX_NONE; }
			if (!bFrozen && StatusFx[i].Frozen != INDEX_NONE) { StopEmitter(StatusFx[i].Frozen); StatusFx[i].Frozen = INDEX_NONE; }
			if (StatusFx[i].Burn == INDEX_NONE && StatusFx[i].Frozen == INDEX_NONE) StatusFx.RemoveAtSwap(i);
		}
		const float StatusRange2 = FMath::Square(T.Num(TEXT("fx.ambient.statusDistance"), 4000.f));
		for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
		{
			AWRCharacterBase* C = *It;
			if (FVector::DistSquared(C->GetActorLocation(), ViewLocation) > StatusRange2 || C->HasTag(WRTags::State_Dead)) continue;
			const bool bBurn = C->HasTag(WRTags::Status_Burn);
			const bool bFrozen = C->HasTag(WRTags::Status_Frozen);
			if (!bBurn && !bFrozen) continue;
			FStatusFx* S = StatusFx.FindByPredicate([C](const FStatusFx& X) { return X.Actor.Get() == C; });
			if (!S) { S = &StatusFx.AddDefaulted_GetRef(); S->Actor = C; }
			const float Size = C->GetCapsuleComponent() ? C->GetCapsuleComponent()->GetScaledCapsuleRadius() / 40.f : 1.f;
			if (bBurn && Resolve(S->Burn) == INDEX_NONE) S->Burn = Start(TEXT("status.burn"), C->GetActorTransform(), FMath::Clamp(Size, 0.6f, 4.f), 0.f, C->GetRootComponent());
			if (bFrozen && Resolve(S->Frozen) == INDEX_NONE) S->Frozen = Start(TEXT("status.frozen"), C->GetActorTransform(), FMath::Clamp(Size, 0.6f, 4.f), 0.f, C->GetRootComponent());
		}

		// Lava cells near the camera (the dressing's hazard cells; their surface is drawn 40 cm above the cell height).
		LavaNear.Reset();
		const TMap<FIntPoint, float>& Lava = FWRBiomeHazards::Cells(TEXT("lava"));
		const float Cell = FWRBiomeHazards::CellSize();
		if (Lava.Num() > 0 && Cell > 0.f)
		{
			const float LavaRange = T.Num(TEXT("fx.lava.distance"), 4500.f);
			const int32 Span = FMath::CeilToInt(LavaRange / Cell);
			const FIntPoint Here(FMath::FloorToInt(ViewLocation.X / Cell), FMath::FloorToInt(ViewLocation.Y / Cell));
			for (int32 dx = -Span; dx <= Span; ++dx)
			{
				for (int32 dy = -Span; dy <= Span; ++dy)
				{
					const float* Z = Lava.Find(Here + FIntPoint(dx, dy));
					if (!Z) continue;
					const FVector Centre((Here.X + dx + 0.5f) * Cell, (Here.Y + dy + 0.5f) * Cell, *Z + 40.f);
					if (FVector::DistSquared(Centre, ViewLocation) < FMath::Square(LavaRange)) LavaNear.Add(Centre);
				}
			}
		}

		// Under a roof or in a cave: no snow or ash falling on you.
		FHitResult Roof;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(WRFxRoof), false);
		if (const APlayerController* PC = World->GetFirstPlayerController()) Query.AddIgnoredActor(PC->GetPawn());
		const bool bCovered = World->LineTraceSingleByChannel(Roof, ViewLocation, ViewLocation + FVector(0.f, 0.f, 4000.f), ECC_WorldStatic, Query);
		ShelterTarget = bCovered ? 1.f : 0.f;
	}

	// Fires flicker their lights.
	for (const FAmbient& A : Ambient)
	{
		if (UPointLightComponent* L = A.Light.Get())
		{
			const float F = T.Num(TEXT("fx.ambient.flicker"), 0.18f);
			L->SetIntensity(A.BaseIntensity * (1.f + F * (0.65f * FMath::PerlinNoise1D(Clock * 6.f + A.Seed) + 0.35f * FMath::PerlinNoise1D(Clock * 15.f + A.Seed * 2.f))));
		}
	}

	// Embers off the lava: a few small bursts a second spread over the cells in range.
	if (LavaNear.Num() > 0)
	{
		const float Cell = FWRBiomeHazards::CellSize();
		LavaSpawnAcc += T.Num(TEXT("fx.lava.burstsPerCell"), 0.5f) * LavaNear.Num() * DeltaTime;
		int32 N = FMath::Min(FMath::FloorToInt(LavaSpawnAcc), 40);
		LavaSpawnAcc -= FMath::FloorToInt(LavaSpawnAcc);
		while (N-- > 0)
		{
			const FVector C = LavaNear[FMath::RandRange(0, LavaNear.Num() - 1)];
			const FVector At = C + FVector(FMath::FRandRange(-0.5f, 0.5f) * Cell, FMath::FRandRange(-0.5f, 0.5f) * Cell, 0.f);
			Start(TEXT("lava.embers"), FTransform(At), 1.f, 0.f);
		}
	}
	else
	{
		LavaSpawnAcc = 0.f;
	}
	TickWeather(DeltaTime);
}

void UWRFxSubsystem::TickWeather(float DeltaTime)
{
	const FWRTuning& T = FWRTuning::Get();
	// Only on the island (the arena maps have no dressing), and only if it isn't switched off.
	const bool bIsland = UWRDressingSubsystem::FindDressing(GetWorld()) != nullptr && T.Num(TEXT("fx.weather.enabled"), 1.f) > 0.f;
	Sheltered = FMath::FInterpTo(Sheltered, ShelterTarget, DeltaTime, 3.f);
	// The ridge runs down x = 0: tundra east, volcanic west; the weather builds up away from the ridge.
	const float Start0 = T.Num(TEXT("fx.weather.ridgeStart"), 5000.f), Full = T.Num(TEXT("fx.weather.ridgeFull"), 25000.f);
	const float Snow = bIsland ? FMath::SmoothStep(Start0, Full, static_cast<float>(ViewLocation.X)) * T.Num(TEXT("fx.weather.snow"), 1.f) : 0.f;
	const float Ash = bIsland ? FMath::SmoothStep(Start0, Full, static_cast<float>(-ViewLocation.X)) * T.Num(TEXT("fx.weather.ash"), 1.f) : 0.f;
	const FTransform At(ViewLocation);
	const auto Run = [&](int32& Handle, FName Effect, float Amount)
	{
		const float Rate = Amount * (1.f - Sheltered);
		if (Rate > 0.01f && Resolve(Handle) == INDEX_NONE) Handle = Start(Effect, At, 1.f, -1.f);
		if (Resolve(Handle) != INDEX_NONE)
		{
			MoveEmitter(Handle, At);
			SetRateScale(Handle, Rate);
		}
	};
	Run(SnowEmitter, TEXT("snow"), Snow);
	Run(AshEmitter, TEXT("ash"), Ash);
}
