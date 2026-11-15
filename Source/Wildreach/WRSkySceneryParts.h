#pragma once

// The Sky's scenery builders (private to WRSkyScenery.cpp and WRSkySites.cpp): one build context per scenery actor (the
// forest's FBuild, plus the island it stands on so every piece sits on the island's real top), and the builders.

#include "CoreMinimal.h"
#include "WRSkyLook.h"
#include "WRSkyIsleTypes.h"
#include "WRSkyAir.h"

class AWRSkyScenery;

namespace WRSkyParts
{
	struct FCtx
	{
		AWRSkyScenery* A;
		WRForestLook::FBuild B;
		const FWRSkyIsleData& D;
		const FWRSkyIsleData::FIsland* I = nullptr;

		FCtx(AWRSkyScenery* InA, float Cull);
		/** The ground under a local XY as a local Z: the island's top when the piece is on an island, else the land's. */
		float G(float X, float Y) const;
		FVector At(float X, float Y, float Lift = 0.f) const { return FVector(X, Y, G(X, Y) + Lift); }
		/** A world point in the actor's local space. */
		FVector L(const FVector& World) const;
		/** Whether a local XY is on the island's top (inside its outline by Margin cm). */
		bool OnTop(float X, float Y, float Margin = 0.f) const;
		float Rand(int32 Salt) const { return B.Rand(Salt); }
	};

	/** A child component of the actor at a local point (a part that moves: a streamer, a kite, a pendulum). */
	USceneComponent* Pivot(AWRSkyScenery* A, const FVector& Local, const FRotator& Rot = FRotator::ZeroRotator);
	/** Blocks in a pivot's own space, spawned under it (every machine but a dedicated server). */
	void SpawnUnder(AWRSkyScenery* A, USceneComponent* Under, const TFunctionRef<void(WRLookKit::FKit&)>& Fill);
	/** A streamer on a swivel at a pole's top (it turns downwind and flaps: AWRSkyScenery::Tick). */
	void AddStreamer(AWRSkyScenery* A, const FVector& Top, int32 Var);

	void Decorate(FCtx& C);
	void Bridge(FCtx& C);
	void Pole(FCtx& C);
	void Falls(FCtx& C);
	void Flue(FCtx& C);
	void Bank(FCtx& C);
	void PaperTitan(FCtx& C);
	void GreatKite(FCtx& C);
	void RingSky(FCtx& C);
	void ChainCliffs(FCtx& C);
	/** Hand sites, the Undercloud's rooms, shrine settings, camps and the kit (WRSkySites.cpp); false: no such kind. */
	bool Site(FCtx& C);
}
