#pragma once

// The Ancient forest's trees, generated in code (cooks/build-biome-forest.md 1.4, 2.6): no forest Fab pack is on this
// PC and engine primitives are out, so each species is a grammar (bole taper, root flare and buttresses, branch whorls
// at the species' angles, a crown envelope, leaf clusters as crossed cards cut to leaf silhouettes) built ONCE per
// species variant into a UStaticMesh at load and drawn instanced. Real proportions (oak, beech, lime, alder, hazel,
// holly, coast-redwood-scale giants). Bark by vertex colour (+ a moss band on upward faces), leaves two-tone by crown
// height. Defined in WRTreeKit.cpp.
//
// Every mesh has three LODs (two for the fern) inside the triangle budgets of the build plan (giant 14k / 3.5k / 800,
// broadleaf 7k / 1.8k / 400, shrub 2.5k / 600 / 150, fern 400 / 120) and three material slots:
//   "Bark"    bark, roots, logs, fungus (M_TreeKit),
//   "Leaves"  leaf sprays and fronds (M_TreeKitLeaf; both faces of every card are built, the material is one-sided),
//   "Glow"    the strips a caller lights: the heart-trees' amber sap in their bark cracks (#D08A1E), the Eldest's
//             foxfire base cracks and the foxfire log's streaks (#7FE07A). Default M_TreeKitGlow (a faint glow);
//             set GlowMaterial(...) on slot "Glow" (SetMaterialByName) to brighten it at night / in the Bloom.
// The materials come from Tools/art/make_tree_kit_material.py (vertex colour RGB stored sRGB, roughness in A,
// flagged for instanced meshes).
//
// Collision: the meshes carry none (no per-poly collision anywhere, leaves never collide). Callers add the trunk
// capsules from Colliders(): AddCollision() for a placed landmark, or hidden instanced cylinders (the dressing).
//
// Dressing instances (Data/island_dressing.json kinds forest_*, written by Tools/terrain/forest_dressing.mjs, drawn by
// AWRDressing in WRWorldDressing.cpp): the scale field is the wanted height over ReferenceHeight(species); instance i of
// a kind draws variant FirstVariant + i % Variants of its species (the same on every machine, and the harvest ledger's
// part stride). The alder's variants 2 and 3 lean along +X (forest_alder_lean is yawed toward the water).

#include "CoreMinimal.h"

class AActor;
class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UObject;

/** A capsule of a tree's collision: the segment A-B (mesh space, cm, at the reference size) and its radius. */
struct FWRTreeCollider
{
	FVector A = FVector::ZeroVector;
	FVector B = FVector::ZeroVector;
	float Radius = 0.f;
};

namespace WRTreeKit
{
	/**
	 * A species mesh, built on first use and cached for the session (same mesh for every caller on every machine: the
	 * grammar is seeded by Species + Variant). Pivot at the trunk's base centre on the ground, +Z up, 1 unit = 1 cm,
	 * modelled at the species' reference height (see ReferenceHeight); scale it uniformly to the height you want.
	 * Species: oak, beech, lime, alder, ash, hazel, holly, sapling, giant, heart, eldest, dead_giant, bower_oak,
	 * bell_tree, strangler, stump, trail_tree, fallen (a fallen giant log along +X, root plate at 0, broken end at +X),
	 * nurse_log (fallen with a row of saplings on its back), fern, bramble, foxfire_log, root_arch (a single arching
	 * root, both ends on the ground along +X), root_mat (a woven roof of roots over +-X/+-Y), buttress_root.
	 * Also the forest.json tree kinds: hermit_oak (an old oak forking at 12 m), hanging_oak (oak), paper_oak and
	 * treant (dead_giant), dead (dead_giant), ringfort_beech (beech).
	 * Logs and roots (fallen, nurse_log, foxfire_log, root_arch, buttress_root, root_mat) may be scaled unevenly:
	 * X along their length, Y/Z their thickness.
	 * Variant 0..3. Returns null on a dedicated server (no looks there; collision comes from AddCollision).
	 */
	WILDREACH_API UStaticMesh* Mesh(UObject* Outer, FName Species, int32 Variant);

	/** The height (cm) the species is modelled at (scale = wanted height / this). For logs and roots: their length
	 *  along X (fallen 6000, nurse_log 5000, foxfire_log 800, root_arch 1200 span, buttress_root 600, root_mat 3000). */
	WILDREACH_API float ReferenceHeight(FName Species);

	/** The trunk's radius at breast height (cm) at the reference height: collision and placement spacing use it.
	 *  For logs: the log's radius at its root end. */
	WILDREACH_API float ReferenceRadius(FName Species);

	/** The material the kit's meshes use: M_TreeKit (bark) or M_TreeKitLeaf (one-sided; the meshes build both faces). */
	WILDREACH_API UMaterialInterface* Material(bool bLeaves);

	/** A lit material for the "Glow" slot: the strip's own colour x Tint x Strength (emissive). */
	WILDREACH_API UMaterialInstanceDynamic* GlowMaterial(UObject* Outer, const FLinearColor& Tint, float Strength);

	/** The trunk collision of a species variant (mesh space at the reference size): capsules round the bole and the
	 *  buttress flare, a log's length and root plate, the strangler's root columns, its spiral ramp (two capsules
	 *  side by side, walkable) and its top deck, the bell-tree's limb. Pure maths: works on a dedicated server. */
	WILDREACH_API const TArray<FWRTreeCollider>& Colliders(FName Species, int32 Variant);

	/** Adds Colliders() as hidden BlockAll capsule components on Owner, the tree placed at At (world; uneven scale
	 *  allowed for logs). Call on the server and on clients alike. Returns how many. */
	WILDREACH_API int32 AddCollision(AActor* Owner, FName Species, int32 Variant, const FTransform& At);

	/** A named point of a species variant (mesh space, reference size), or ZeroVector:
	 *  bell_tree "bell" (under the horizontal limb at 40 m, where the bell hangs), eldest "door" (the foot of the
	 *  Eldest's Door crack, facing +X), fallen / nurse_log "broken_end" (centre of the amber end), "root_plate",
	 *  strangler "vista" (the top deck), "entrance" (the gap at +X where the ramp starts), hermit_oak "fork" (12 m),
	 *  trail_tree "points" (the end of the knee-high bend: the waymark points along +X). */
	WILDREACH_API FVector Anchor(FName Species, int32 Variant, FName Point);

	/** The variant (0..3) a decoration standing at Ground draws as: the same on every machine. */
	WILDREACH_API int32 VariantAt(const FVector& Ground);
}
