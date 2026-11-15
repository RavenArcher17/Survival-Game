// The Desert's scenery builders (cooks/build-biome-desert.md 2.2-2.6): one function a kind, split across
// WRDesertScenery.cpp (landmarks, shrines, camps, caves, the Hourhold, brine crusts, flood wadis, decoration) and
// WRDesertSites.cpp (the 35 hand sites and the kit). Internal to those files.
#pragma once

#include "CoreMinimal.h"
#include "WRDesertLook.h"
#include "WRDesertTypes.h"
#include "WRDesertWorld.h"

namespace WRDesertParts
{
	using namespace WRDesertLook;

	struct FDsCtx
	{
		AWRDesertScenery* A;
		FBuild B;
		const FWRDesertData& D;
		FDsCtx(AWRDesertScenery* InA, float Cull) : A(InA), B(InA, Cull), D(FWRDesertData::Get()) {}
		/** A world point (cm) in this piece's local space. */
		FVector L(const FVector& W) const { return B.Local(W); }
		/** The ground under a local XY (local z). */
		float G(float X, float Y) const { return B.G(X, Y, 0.f); }
	};

	// WRDesertScenery.cpp
	void HaboobWall(FDsCtx& C);
	void Gnomon(FDsCtx& C);
	void SunGate(FDsCtx& C);
	void MotherDune(FDsCtx& C);
	void WhaleRoad(FDsCtx& C);
	void Keelyard(FDsCtx& C);
	void TallWreck(FDsCtx& C);
	void FallenHour(FDsCtx& C);
	void Needles(FDsCtx& C);
	void GlassCanyon(FDsCtx& C);
	void SevenfoldWells(FDsCtx& C);
	void LeviathanRibs(FDsCtx& C);
	void Shrine(FDsCtx& C);
	void Camp(FDsCtx& C);
	void Runback(FDsCtx& C);
	void AmastanShade(FDsCtx& C);
	void Cave(FDsCtx& C);
	void HourholdRoom(FDsCtx& C);
	void HourholdLink(FDsCtx& C);
	void HourholdWay(FDsCtx& C);
	void BrineCrust(FDsCtx& C);
	void FloodWadi(FDsCtx& C);
	void Decor(FDsCtx& C);
	// WRDesertSites.cpp
	bool Site(FDsCtx& C);
	bool Kit(FDsCtx& C);
}
