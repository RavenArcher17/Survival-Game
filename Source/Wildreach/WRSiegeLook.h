#pragma once

#include "CoreMinimal.h"

struct FWRToolMesh;
class UProceduralMeshComponent;

/**
 * The siege engines modelled in code (WRSiegeLook.cpp; cookpass-building 3.6), each moving part its own mesh round its
 * own pivot so the engine can move it: real measures from rams, counterweight trebuchets and ballistae (1.3). Every part
 * is built facing +X (the way the engine works), its pivot at the origin.
 */
namespace WRSiegeLook
{
	// --- The battering ram (ground at Z 0)
	/** Soles, axles in bearing blocks, posts, A-frames, ridge, rafters, wet hides, push handles, harness pole. */
	FWRToolMesh RamFrame();
	/** One solid three-plank wheel with its iron tyre and cleats, its axle along Y through the origin. */
	FWRToolMesh RamWheel();
	/** One chain hanging from its hook (origin) straight down Length cm to the log's eye bolt. */
	FWRToolMesh RamChain(float Length);
	/** The heartwood log with its steel cap toward +X, its eye bolts on top at X +-RamChainX(); centred on the origin. */
	FWRToolMesh RamLog();
	float RamChainX();
	/** Where the ridge hooks hang the chains (Z) and how far below them the log rides. */
	float RamHookZ();
	float RamChainLength();
	float RamEyeZ();
	float RamWheelRadius();

	// --- The counterweight trebuchet (ground at Z 0; the upper frame turns on the turntable about Z)
	FWRToolMesh TrebBase();
	/** Upper sills, rollers, the two towers, the axle, the trough, the aiming lever. */
	FWRToolMesh TrebUpper();
	/** The winch: drum, capstan wheels and ratchet wheel, axis along Y through the origin. */
	FWRToolMesh TrebDrum();
	/** The throwing arm on its axle (origin): long end toward -X (TrebLong cm), short end toward +X (TrebShort cm). */
	FWRToolMesh TrebArm();
	/** The release pin at the arm's tip, its prong turned by Pin (0..2). */
	FWRToolMesh TrebPin(int32 Pin);
	/** The hinged counterweight box hanging from its pin (origin) below. */
	FWRToolMesh TrebWeight();
	/** The ratchet's pawl, pivot at the origin, its nose toward +X. */
	FWRToolMesh TrebPawl();
	float TrebAxleZ();
	float TrebLong();
	float TrebShort();
	float TrebSlingLength();
	FVector TrebDrumAt();
	FVector TrebPawlAt();

	// --- The ballista (ground at Z 0)
	FWRToolMesh BalTripod();
	/** The yoke on the swivel (origin at the swivel's top), its trunnion at BalTrunnionZ(). */
	FWRToolMesh BalYoke();
	/** The stock with its groove, the bow's mount and the windlass frame, pivot (the trunnion) at the origin. */
	FWRToolMesh BalStock();
	/** One steel bow leaf from its root (origin) out along +Y (bLeft) or -Y. */
	FWRToolMesh BalLeaf(bool bLeft);
	/** The windlass drum with its two handles, axis along Y through the origin. */
	FWRToolMesh BalWindlass();
	/** The slider and claw that draws the string. */
	FWRToolMesh BalSlider();
	float BalSwivelZ();
	float BalTrunnionZ();
	FVector BalLeafRoot(bool bLeft);
	float BalLeafLength();
	float BalSliderRest();
	float BalSliderDrawn();
	FVector BalWindlassAt();

	// --- Shared
	/** A rope (or cord) one centimetre long along +X: scaled to its length and turned each frame. */
	FWRToolMesh Rope(float Radius);
	/** The trebuchet's sling pouch (leather), the rope's end at the origin. */
	FWRToolMesh Pouch();
	/** A dressed siege stone (70 cm, a little rough), centred on the origin. */
	FWRToolMesh Stone();
	/** A ballista bolt along +X, its head at the front. */
	FWRToolMesh Bolt();
	/** Projectile models (AWRProjectile "code:siegestone" / "code:ballistabolt"). */
	bool BuildThrownModel(const FString& Path, UProceduralMeshComponent* Comp);
}
