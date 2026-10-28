#pragma once

#include "CoreMinimal.h"

class AWRResourceNode;
class UStaticMeshComponent;

/**
 * What a resource node looks like. Nodes were engine cones in flat colours (Raven: "weird green cone thing ... and a
 * yellow one, all over"); now each item gets a real look built from the art the island already uses: trees for wood,
 * boulders for stone, ore boulders with coloured lumps, sulfur crystal clusters, lichen-crusted rocks, berry bushes
 * and fibre plants. The node's own mesh stays as its hidden, simple collision (the swing still hits the node), on every
 * machine; the look is drawn on clients only. Falls back to the old coloured cone if a pack is missing.
 */
namespace WRResourceLook
{
	void Apply(AWRResourceNode* Node, UStaticMeshComponent* Collider);
}
