// The Sky's scenery (cooks/biome-sky.md 2.1-2.4, 2.7, 2.14; cooks/build-biome-sky.md 2.2-2.5): the dispatcher, the
// landmarks (the Paper Titan, the Great Kite, the Chain Cliffs' head, the Ring's cloud cap and spore column), the Flue's
// shaft, the bridges and chain walks, the wind-poles, the waterfalls, the Undercloud's bank and every island's
// decoration. Hand sites, the Undercloud's rooms, the kit and the camps are WRSkySites.cpp.

#include "WRSkyWorld.h"
#include "WRSkyLook.h"
#include "WRSkyIsleTypes.h"
#include "WRSkySceneryParts.h"
#include "WRGameState.h"
#include "WRTuning.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

using namespace WRSkyLook;

AWRSkyScenery::AWRSkyScenery()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickInterval = 0.f;
}

namespace WRSkySceneryImpl
{
	bool IsLandmark(FName K)
	{
		return K == TEXT("island") || K == TEXT("paper_titan") || K == TEXT("great_kite") || K == TEXT("flue") || K == TEXT("bank")
			|| K == TEXT("serpents_ring") || K == TEXT("falls") || K == TEXT("chain_cliffs") || K == TEXT("bridge");
	}
	bool IsKit(FName K)
	{
		return K == TEXT("round_house") || K == TEXT("wind_cairn") || K == TEXT("keel_outcrop") || K == TEXT("gossamer") || K == TEXT("harrier_nest")
			|| K == TEXT("mane_ledge") || K == TEXT("fall_pool") || K == TEXT("kill_scene") || K == TEXT("vista") || K == TEXT("teacher_gust")
			|| K == TEXT("teacher_stone") || K == TEXT("pole") || K == TEXT("decor");
	}
}

void AWRSkyScenery::BuildSky()
{
	using namespace WRSkySceneryImpl;
	const FName K = Kind;
	if (K == TEXT("island"))
	{
		BuildIsland();
		WRSkyParts::FCtx C0(this, 0.f);
		WRSkyParts::Decorate(C0);
		C0.B.Finish();
		return;
	}
	const float Cull = IsLandmark(K) ? 0.f : IsKit(K) ? WRTune(TEXT("skyWorld.kitCull"), 40000.f) : WRTune(TEXT("skyWorld.siteCull"), 70000.f);
	WRSkyParts::FCtx C(this, Cull);
	bool bBuilt = true;
	if (K == TEXT("bridge")) WRSkyParts::Bridge(C);
	else if (K == TEXT("pole")) WRSkyParts::Pole(C);
	else if (K == TEXT("falls")) WRSkyParts::Falls(C);
	else if (K == TEXT("flue")) WRSkyParts::Flue(C);
	else if (K == TEXT("bank")) WRSkyParts::Bank(C);
	else if (K == TEXT("paper_titan")) WRSkyParts::PaperTitan(C);
	else if (K == TEXT("great_kite")) WRSkyParts::GreatKite(C);
	else if (K == TEXT("serpents_ring")) WRSkyParts::RingSky(C);
	else if (K == TEXT("chain_cliffs")) WRSkyParts::ChainCliffs(C);
	else bBuilt = WRSkyParts::Site(C);
	if (!bBuilt) UE_LOG(LogTemp, Verbose, TEXT("SKY scenery: nothing to build for %s"), *K.ToString());
	C.B.Finish();
}

void AWRSkyScenery::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double T = WRSkyIsles::ServerTime(this);
	// Near the viewer only (the cloth's flutter is a look; the swings are slow enough to read from any range).
	FVector View = GetActorLocation();
	if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		FRotator R;
		PC->GetPlayerViewPoint(View, R);
	}
	if (Swing)
	{
		if (Kind == TEXT("great_kite") || Kind == TEXT("kite_field"))
		{
			// The weathervane: the kite streams downwind, eased round as the wind turns (the Turning Wind swings it
			// 180 degrees over two minutes); it bobs and yaws a little on the gusts.
			const float Want = WRSkyIsles::WindYaw(this);
			const FRotator Cur = Swing->GetRelativeRotation();
			const float Yaw = FMath::FixedTurn(Cur.Yaw, Want - GetActorRotation().Yaw, 12.f * DeltaSeconds);
			const float Bob = 2.5f * FMath::Sin(float(FMath::Fmod(T, 1000.0)) * 2.f * PI / 9.f);
			Swing->SetRelativeRotation(FRotator(Bob, Yaw, 1.5f * FMath::Sin(float(FMath::Fmod(T, 1000.0)) * 2.f * PI / 6.3f)));
		}
		else if (Kind == TEXT("bridge"))
		{
			// The Fallen Span: a pendulum from the server time (every machine the same), a slow 22 s swing.
			const float A = SwingDegrees * FMath::Sin(float(FMath::Fmod(T, double(SwingPeriod))) * 2.f * PI / SwingPeriod);
			Swing->SetRelativeRotation(FRotator(0.f, 0.f, A));
		}
		else
		{
			// A wheel's hub, a vane: a slow turn on the wind.
			Swing->AddLocalRotation(FRotator(0.f, 0.f, DeltaSeconds * 6.f * WRSkyIsles::WindStrength(this)));
		}
	}
	if (TollLamps)
	{
		const AWRSkyState* S = AWRSkyState::Get(this);
		const bool bShow = S && S->bTollLit && AWRGameState::IsNightIn(GetWorld());
		if (TollLamps->IsVisible() != bShow) TollLamps->SetVisibility(bShow, true);
	}
	if (Cloths.Num() > 0)
	{
		const float Range = WRTune(TEXT("skyWorld.clothRange"), 12000.f);
		// Streamers point downwind and flap: three times as fast when a gust's tell runs here (SKY 2.5 RAISE).
		const float Wind = WRSkyIsles::WindYaw(this);
		const WRSkyAir::FGust G = WRSkyAir::GustAt(this, GetActorLocation(), true);
		const float Rate = G.bOn ? 3.f : 1.f;
		const float GustYaw = G.bOn ? FMath::RadiansToDegrees(FMath::Atan2(G.Dir.Y, G.Dir.X)) : Wind;
		ClothClock += DeltaSeconds * Rate * WRSkyIsles::WindStrength(this);
		for (int32 i = 0; i < Cloths.Num(); ++i)
		{
			if (!Cloths[i] || FVector::DistSquared(View, Cloths[i]->GetComponentLocation()) > Range * Range) continue;
			const float Ph = ClothPhase.IsValidIndex(i) ? ClothPhase[i] : 0.f;
			const float Flap = FMath::Sin(ClothClock * 9.f + Ph) * (G.bOn ? 6.f : 14.f);
			const float WorldYaw = FMath::FixedTurn(Cloths[i]->GetComponentRotation().Yaw, GustYaw, 90.f * DeltaSeconds);
			Cloths[i]->SetWorldRotation(FRotator(G.bOn ? -2.f : -8.f + 3.f * FMath::Sin(ClothClock * 2.3f + Ph), WorldYaw, Flap));
		}
	}
}
