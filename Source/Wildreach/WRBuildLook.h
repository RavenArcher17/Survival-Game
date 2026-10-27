#pragma once

#include "CoreMinimal.h"

struct FWRPieceDef;
class AActor;
class USceneComponent;
class UStaticMeshComponent;

/** What a detail block is made of: the building materials' planks or stone, or a flat cloth / soil colour. */
enum class EWRLookMat : uint8 { Wood, Stone, Cloth };
/** Engine shapes the blocks use (one instanced component per shape and material). */
enum class EWRLookMesh : uint8 { Cube, Cylinder, Cone };

/** One visible block of a built piece: an engine shape, scaled, placed in the piece's (or the door leaf's) space. */
struct FWRLookBlock
{
	FVector Size = FVector(100.f);  // cm
	FVector Centre = FVector::ZeroVector;  // cm
	FRotator Rot = FRotator::ZeroRotator;
	EWRLookMat Mat = EWRLookMat::Wood;
	EWRLookMesh Mesh = EWRLookMesh::Cube;
	/** Which shade of its material (0..1), the world axis its grain runs along (0 X, 1 Y, 2 Z), a brightness factor. */
	float Tone = 0.5f;
	float Axis = 0.f;
	float Dim = 1.f;
};

/**
 * The shapes a built piece shows, made from instanced engine shapes: plank walls with gaps and corner posts, coursed
 * stone, floorboards, flagstones, lapped shingles and slates, door and window frames, braced door leaves, palisade
 * logs, furniture. Pure (the blocks only), so tests can check them and the piece's own (unchanged) collision boxes
 * stay what the game plays with. Each block's shade comes from where it stands in the world, so the corner post two
 * walls share is the same post on both.
 */
struct WILDREACH_API FWRBuildLook
{
	/** The detail blocks of a built piece (empty: the piece keeps its plain blocks). */
	static TArray<FWRLookBlock> Blocks(const FWRPieceDef& Def, const FTransform& PieceToWorld);
	/** A door or gate leaf, in the leaf's own space (centred on it), LeafSize as AWRBuildPiece::ApplyLook makes it. */
	static TArray<FWRLookBlock> LeafBlocks(const FWRPieceDef& Def, const FVector& LeafSize, const FTransform& PieceToWorld);
	/** The flat colour of a piece's cloth / soil blocks (bed covers, the torch's wrapping, crop rows). */
	static FLinearColor ClothColor(const FWRPieceDef& Def);
	/** 0..1, the same for the same world point (to 5 cm) on every machine. */
	static float Hash01(const FVector& WorldPoint, int32 Salt);

	/**
	 * Builds the blocks as instanced static meshes under Parent (one per material and shape, no collision): planks
	 * and stone wear M_BuildWood / M_BuildStone in their per-block mode (shade, grain axis, brightness from custom
	 * data). bAbsoluteScale for a scaled parent (the door leaf). Returns how many components it made.
	 */
	static int32 Spawn(AActor* Owner, USceneComponent* Parent, const TArray<FWRLookBlock>& Blocks, const FLinearColor& Cloth, bool bAbsoluteScale);

	/**
	 * A prop mesh for a station / the Hearth / the totem / the chest (pieces.json "mesh"), fitted inside the piece's
	 * "meshFit" box (cm) with its bottom at BottomZ (in Parent's unscaled space, Parent centred), turned "meshYaw".
	 * No collision: the gameplay box stays. Null (keep the plain shape) if the def has none or the mesh is missing.
	 */
	static UStaticMeshComponent* AttachProp(AActor* Owner, USceneComponent* Parent, const FWRPieceDef& Def, float BottomZ);
	/** How far above its piece (the ground) a station, Hearth or totem actor stands (AWRBuildPiece::SpawnChild). */
	static constexpr float ChildLift = 50.f;
	/** The def that carries a station type's look (pieces.json entries with "station"). */
	static const FWRPieceDef* StationDef(FName StationType);
	/** The first def of a kind ("hearth", "totem"). */
	static const FWRPieceDef* KindDef(FName Kind);
};
