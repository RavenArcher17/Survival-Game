// The Ocean's scenery builders (cooks/build-biome-ocean.md 2.2-2.5): one function a kind, split across
// WROceanScenery.cpp (landmarks, shrines, camps, caves, the Drowned Hall, decoration) and WROceanSites.cpp (the hand
// sites and the kit). Internal to those files.
#pragma once

#include "CoreMinimal.h"
#include "WROceanLook.h"
#include "WROceanTypes.h"
#include "WROceanWorld.h"

namespace WROceanParts
{
	using namespace WROceanLook;

	struct FOcCtx
	{
		AWROceanScenery* A;
		FBuild B;
		const FWROceanData& D;
		FOcCtx(AWROceanScenery* InA, float Cull) : A(InA), B(InA, Cull), D(FWROceanData::Get()) {}
		/** A world point (cm) in this piece's local space. */
		FVector L(const FVector& W) const { return B.Local(W); }
		/** The mean sea surface in local Z. */
		float SeaZ() const { return float(B.Local(FVector(A->GetActorLocation().X, A->GetActorLocation().Y, WRTideMeanZ())).Z); }
		static float WRTideMeanZ();
	};

	// WROceanScenery.cpp
	void Pound(FOcCtx& C);
	void Colossus(FOcCtx& C);
	void BeaconStack(FOcCtx& C);
	void SpoutingHorn(FOcCtx& C);
	void DykeHead(FOcCtx& C);
	void LongDyke(FOcCtx& C);
	void ReefLighthouse(FOcCtx& C);
	void BellTower(FOcCtx& C);
	void Shrine(FOcCtx& C);
	void Camp(FOcCtx& C);
	void Runback(FOcCtx& C);
	void CaveMouth(FOcCtx& C);
	void Cave(FOcCtx& C);
	void Hall(FOcCtx& C);
	void TsunamiStone(FOcCtx& C);
	void Decor(FOcCtx& C);
	void HaewonHouse(FOcCtx& C);
	// WROceanSites.cpp
	bool Site(FOcCtx& C);
	bool Kit(FOcCtx& C);
}
